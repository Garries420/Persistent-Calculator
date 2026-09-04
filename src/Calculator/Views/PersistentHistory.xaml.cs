// Licensed under the MIT License.

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Input;

using CalculatorApp.Common;

using Windows.UI.Xaml;
using Windows.UI.Xaml.Automation;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;

namespace CalculatorApp
{
    public sealed partial class PersistentHistory : UserControl
    {
        private const string AllCategories = "All categories";
        private PersistentHistoryService _service;
        private IReadOnlyList<string> _availableCategories = Array.Empty<string>();
        private bool _isRefreshingCategories;

        public PersistentHistory()
        {
            InitializeComponent();
        }

        public ObservableCollection<PersistentHistoryEntry> VisibleEntries { get; }
            = new ObservableCollection<PersistentHistoryEntry>();

        public event EventHandler CloseRequested;

        public event EventHandler ClearRequested;

        public event EventHandler<PersistentHistoryEntryEventArgs> EntryInvoked;

        public ICommand ClearHistoryCommand
        {
            get
            {
                if (_clearHistoryCommand == null)
                {
                    _clearHistoryCommand = Utils.DelegateCommandUtils.MakeDelegateCommand(
                        this,
                        (that, parameter) => that.ClearRequested?.Invoke(that, EventArgs.Empty));
                }
                return _clearHistoryCommand;
            }
        }

        internal void Initialize(
            PersistentHistoryService service,
            IEnumerable<string> availableCategories)
        {
            _service = service;
            _availableCategories = availableCategories
                .Where(name => !string.IsNullOrWhiteSpace(name))
                .Distinct(StringComparer.CurrentCultureIgnoreCase)
                .ToList();

            _isRefreshingCategories = true;
            CategoryPicker.SelectedIndex = -1;
            _isRefreshingCategories = false;
            Refresh();
        }

        internal void Refresh()
        {
            if (_service == null)
            {
                return;
            }

            var entries = _service.GetEntries();
            string selected = GetSelectedCategory();

            RefreshCategories(entries, selected);
            ApplySelectedCategory(entries);
            RefreshHistorySummary(entries.Count);
        }

        private void ApplySelectedCategory(IReadOnlyList<PersistentHistoryEntry> allEntries)
        {
            string selected = GetSelectedCategory();
            IEnumerable<PersistentHistoryEntry> entries = allEntries;

            if (!selected.Equals(AllCategories, StringComparison.CurrentCultureIgnoreCase))
            {
                entries = allEntries
                    .Where(entry => string.Equals(
                        entry.Mode,
                        selected,
                        StringComparison.CurrentCultureIgnoreCase));
            }

            VisibleEntries.Clear();
            foreach (var entry in entries)
            {
                VisibleEntries.Add(entry);
            }

            bool hasEntries = VisibleEntries.Count > 0;
            HistoryEmpty.Visibility = hasEntries ? Visibility.Collapsed : Visibility.Visible;
            HistoryListView.Visibility = hasEntries ? Visibility.Visible : Visibility.Collapsed;
        }

        private void RefreshHistorySummary(int totalEntryCount)
        {
            ClearHistoryButton.Visibility = totalEntryCount > 0
                ? Visibility.Visible
                : Visibility.Collapsed;

            if (ClearHistoryButton.Visibility == Visibility.Visible)
            {
                HistoryFileSizeText.Text = "• …";
                _ = RefreshHistoryFileSizeAsync();
            }
            else
            {
                HistoryFileSizeText.Text = string.Empty;
            }
        }

        private void RefreshCategories(
            IReadOnlyList<PersistentHistoryEntry> entries,
            string selectedCategory)
        {
            var counts = entries
                .Where(entry => !string.IsNullOrWhiteSpace(entry.Mode))
                .GroupBy(entry => entry.Mode, StringComparer.CurrentCultureIgnoreCase)
                .ToDictionary(group => group.Key, group => group.Count(), StringComparer.CurrentCultureIgnoreCase);

            var categoryNames = _availableCategories
                .Concat(entries.Select(entry => entry.Mode))
                .Where(name => !string.IsNullOrWhiteSpace(name))
                .Distinct(StringComparer.CurrentCultureIgnoreCase)
                .ToList();

            _isRefreshingCategories = true;
            try
            {
                CategoryPicker.Items.Clear();
                CategoryPicker.Items.Add(CreateCategoryItem(AllCategories, entries.Count));
                foreach (string categoryName in categoryNames)
                {
                    counts.TryGetValue(categoryName, out int count);
                    CategoryPicker.Items.Add(CreateCategoryItem(categoryName, count));
                }

                int selectedIndex = 0;
                for (int index = 0; index < CategoryPicker.Items.Count; index++)
                {
                    if (CategoryPicker.Items[index] is ComboBoxItem item
                        && string.Equals(
                            item.Tag as string,
                            selectedCategory,
                            StringComparison.CurrentCultureIgnoreCase))
                    {
                        selectedIndex = index;
                        break;
                    }
                }
                CategoryPicker.SelectedIndex = selectedIndex;
            }
            finally
            {
                _isRefreshingCategories = false;
            }
        }

        private static ComboBoxItem CreateCategoryItem(string name, int count)
        {
            string countLabel = count.ToString("N0", CultureInfo.CurrentCulture);
            var content = new StackPanel
            {
                Orientation = Orientation.Horizontal
            };
            content.Children.Add(new TextBlock
            {
                Text = name
            });
            content.Children.Add(new TextBlock
            {
                Margin = new Thickness(8, 0, 0, 0),
                Foreground = (Brush)Application.Current.Resources["AccentTextFillColorPrimaryBrush"],
                FontWeight = Windows.UI.Text.FontWeights.SemiBold,
                Text = countLabel
            });

            var item = new ComboBoxItem
            {
                Content = content,
                Tag = name
            };
            AutomationProperties.SetName(
                item,
                $"{name}, {countLabel} saved calculation{(count == 1 ? string.Empty : "s")}");
            return item;
        }

        private string GetSelectedCategory()
        {
            return (CategoryPicker.SelectedItem as ComboBoxItem)?.Tag as string
                ?? AllCategories;
        }

        private async Task RefreshHistoryFileSizeAsync()
        {
            ulong? bytes = await _service.GetHistoryFileSizeAsync();
            if (_service == null || ClearHistoryButton.Visibility != Visibility.Visible)
            {
                return;
            }

            string size = bytes.HasValue ? FormatFileSize(bytes.Value) : "Size unavailable";
            HistoryFileSizeText.Text = $"• {size}";
            AutomationProperties.SetName(ClearHistoryButton, $"Clear all saved history, {size}");
        }

        internal static string FormatFileSize(ulong bytes)
        {
            if (bytes < 1024)
            {
                return $"{bytes.ToString("N0", CultureInfo.CurrentCulture)} B";
            }

            string[] units = { "B", "kB", "MB", "GB", "TB", "PB", "EB" };
            double value = bytes;
            int unitIndex = 0;
            while (value >= 1024 && unitIndex < units.Length - 1)
            {
                value /= 1024;
                unitIndex++;
            }

            // Match File Explorer's friendly whole-kilobyte display for small
            // history files (for example, 7,538 bytes is shown as 8 kB).
            if (unitIndex == 1)
            {
                return $"{Math.Ceiling(value).ToString("N0", CultureInfo.CurrentCulture)} {units[unitIndex]}";
            }

            return $"{value.ToString("0.##", CultureInfo.CurrentCulture)} {units[unitIndex]}";
        }

        private void BackButton_Click(object sender, RoutedEventArgs e)
        {
            CloseRequested?.Invoke(this, EventArgs.Empty);
        }

        private void CategoryPicker_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (CategoryPicker != null && !_isRefreshingCategories && _service != null)
            {
                // Never clear or rebuild a ComboBox's Items collection from inside
                // its own SelectionChanged event. Doing that corrupts the native
                // flyout state and can crash Windows.UI.Xaml when it is opened again.
                ApplySelectedCategory(_service.GetEntries());
            }
        }

        private void HistoryListView_ItemClick(object sender, ItemClickEventArgs e)
        {
            if (e.ClickedItem is PersistentHistoryEntry entry)
            {
                EntryInvoked?.Invoke(this, new PersistentHistoryEntryEventArgs(entry));
            }
        }

        private ICommand _clearHistoryCommand;
    }

    public sealed class PersistentHistoryEntryEventArgs : EventArgs
    {
        public PersistentHistoryEntryEventArgs(PersistentHistoryEntry entry)
        {
            Entry = entry;
        }

        public PersistentHistoryEntry Entry { get; }
    }
}
