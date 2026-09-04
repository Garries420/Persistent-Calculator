using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.Linq;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Windows.ApplicationModel.UserActivities;
using Windows.Foundation;
using Windows.Graphics.Display;
using Windows.Storage;
using Windows.System;
using Windows.UI.Core;
using Windows.UI.ViewManagement;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Automation;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Navigation;
using Microsoft.UI.Xaml.Controls;

using CalculatorApp.Common;
using CalculatorApp.Converters;
using CalculatorApp.JsonUtils;
using CalculatorApp.ManagedViewModels;
using CalculatorApp.Utils;
using CalculatorApp.ViewModel.Common;
using CalculatorApp.ViewModel.Common.Automation;

using wuxc = Windows.UI.Xaml.Controls;

namespace CalculatorApp
{
    public sealed partial class MainPage : wuxc.Page
    {
        private const string IgnoredUpdateVersionKey = "IgnoredUpdateVersion";

        public static readonly DependencyProperty NavViewCategoriesSourceProperty =
            DependencyProperty.Register(nameof(NavViewCategoriesSource), typeof(List<object>), typeof(MainPage), new PropertyMetadata(default));

        public List<object> NavViewCategoriesSource
        {
            get => (List<object>)GetValue(NavViewCategoriesSourceProperty);
            set => SetValue(NavViewCategoriesSourceProperty, value);
        }

        public ApplicationViewModel Model { get; }

        public MainPage()
        {
            Model = new ApplicationViewModel();
            InitializeNavViewCategoriesSource();
            InitializeComponent();

            KeyboardShortcutManager.Initialize();

            Application.Current.Suspending += App_Suspending;
            Model.PropertyChanged += OnAppPropertyChanged;
            m_accessibilitySettings = new AccessibilitySettings();

            if (Utilities.GetIntegratedDisplaySize(out var sizeInInches))
            {
                if (sizeInInches < 7.0) // If device's display size (diagonal length) is less than 7 inches then keep the calc always in Portrait mode only
                {
                    DisplayInformation.AutoRotationPreferences = DisplayOrientations.Portrait | DisplayOrientations.PortraitFlipped;
                }
            }

            UserActivityRequestManager.GetForCurrentView().UserActivityRequested += async (_, args) =>
            {
                using (var deferral = args.GetDeferral())
                {
                    if (deferral == null)
                    {
                        // FIXME: https://microsoft.visualstudio.com/DefaultCollection/OS/_workitems/edit/47775705/
                        TraceLogger.GetInstance().LogRecallError("55e29ba5-6097-40ec-8960-458750be3039");
                        return;
                    }
                    var channel = UserActivityChannel.GetDefault();
                    var activity = await channel.GetOrCreateUserActivityAsync($"{Guid.NewGuid()}");
                    string embeddedData;
                    try
                    {
                        var json = JsonSerializer.Serialize(new ApplicationSnapshotAlias(Model.Snapshot));
                        embeddedData = Convert.ToBase64String(DeflateUtils.Compress(json));
                    }
                    catch (Exception ex)
                    {
                        TraceLogger.GetInstance().LogRecallError($"Error occurs during the serialization of Snapshot. Exception: {ex}");
                        deferral.Complete();
                        return;
                    }
                    activity.ActivationUri = new Uri($"persistent-calculator:snapshot/{embeddedData}");
                    activity.IsRoamable = false;
                    var resProvider = AppResourceProvider.GetInstance();
                    activity.VisualElements.DisplayText =
                        $"{resProvider.GetResourceString("AppName")} - {resProvider.GetResourceString(NavCategoryStates.GetNameResourceKey(Model.Mode))}";
                    await activity.SaveAsync();
                    args.Request.SetUserActivity(activity);
                    deferral.Complete();
                    TraceLogger.GetInstance().LogRecallSnapshot(Model.Mode);
                }
            };
        }

        public void UnregisterEventHandlers()
        {
            Window.Current.SizeChanged -= WindowSizeChanged;
            m_accessibilitySettings.HighContrastChanged -= OnHighContrastChanged;

            if (m_calculator != null)
            {
                m_calculator.UnregisterEventHandlers();
            }
        }

        public void SetDefaultFocus()
        {
            if (m_calculator != null && m_calculator.Visibility == Visibility.Visible)
            {
                m_calculator.SetDefaultFocus();
            }
            if (m_dateCalculator != null && m_dateCalculator.Visibility == Visibility.Visible)
            {
                m_dateCalculator.SetDefaultFocus();
            }
            if (m_converter != null && m_converter.Visibility == Visibility.Visible)
            {
                m_converter.SetDefaultFocus();
            }
        }

        public void SetHeaderAutomationName()
        {
            ViewMode mode = Model.Mode;
            var resProvider = AppResourceProvider.GetInstance();

            string name;
            if (NavCategory.IsDateCalculatorViewMode(mode))
            {
                name = resProvider.GetResourceString("HeaderAutomationName_Date");
            }
            else
            {
                string full = string.Empty;
                if (NavCategory.IsCalculatorViewMode(mode))
                {
                    full = resProvider.GetResourceString("HeaderAutomationName_Calculator");
                }
                else if (NavCategory.IsConverterViewMode(mode))
                {
                    full = resProvider.GetResourceString("HeaderAutomationName_Converter");
                }
                name = LocalizationStringUtil.GetLocalizedString(full, Model.CategoryName);
            }

            AutomationProperties.SetName(Header, name);
        }

        protected override void OnNavigatedTo(NavigationEventArgs e)
        {
            var initialMode = ViewMode.Standard;
            var localSettings = ApplicationData.Current.LocalSettings;
            if (localSettings.Values.ContainsKey(nameof(ApplicationViewModel.Mode)))
            {
                initialMode = NavCategoryStates.Deserialize(localSettings.Values[nameof(ApplicationViewModel.Mode)]);
            }

            if (e.Parameter == null)
            {
                Model.Initialize(initialMode);
                return;
            }

            if (e.Parameter is string legacyArgs)
            {
                if (legacyArgs.Length > 0)
                {
                    initialMode = (ViewMode)Convert.ToInt32(legacyArgs);
                }
                Model.Initialize(initialMode);
            }
            else if (e.Parameter is SnapshotLaunchArguments snapshotArgs)
            {
                Model.Initialize(initialMode);
                if (!snapshotArgs.HasError)
                {
                    Model.RestoreFromSnapshot(snapshotArgs.Snapshot);
                    TraceLogger.GetInstance().LogRecallRestore((ViewMode)snapshotArgs.Snapshot.Mode);
                }
                else
                {
                    _ = Window.Current.Dispatcher.RunAsync(CoreDispatcherPriority.Normal,
                        async () => await ShowSnapshotLaunchErrorAsync());
                    TraceLogger.GetInstance().LogRecallError("OnNavigatedTo:Found errors.");
                }
            }
            else
            {
                Environment.FailFast("cd75d5af-0f47-4cc2-910c-ed792ed16fe6");
            }
        }

        private void InitializeNavViewCategoriesSource()
        {
            NavViewCategoriesSource = ExpandNavViewCategoryGroups(Model.Categories);
        }

        private List<object> ExpandNavViewCategoryGroups(IEnumerable<NavCategoryGroup> groups)
        {
            var result = new List<object>();
            foreach (var group in groups)
            {
                result.Add(group);
                foreach (var category in group.Categories)
                {
                    result.Add(category);
                }
            }
            return result;
        }

        private void UpdatePopupSize(Windows.UI.Core.WindowSizeChangedEventArgs e)
        {
            if (PopupContent != null)
            {
                PopupContent.Width = e.Size.Width;
                PopupContent.Height = e.Size.Height;
            }

            if (ChangelogPopupContent != null)
            {
                ChangelogPopupContent.Width = e.Size.Width;
                ChangelogPopupContent.Height = e.Size.Height;
            }

            if (HistoryPopupContent != null)
            {
                HistoryPopupContent.Width = e.Size.Width;
                HistoryPopupContent.Height = e.Size.Height;
            }

            if (UpdatePopupContent != null)
            {
                UpdatePopupContent.Width = e.Size.Width;
                UpdatePopupContent.Height = e.Size.Height;
            }
        }

        private void WindowSizeChanged(object sender, Windows.UI.Core.WindowSizeChangedEventArgs e)
        {
            // We don't use layout aware page's view states, we have our own
            UpdateViewState();
            UpdatePopupSize(e);
        }

        private void OnAppPropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            string propertyName = e.PropertyName;
            if (propertyName == nameof(ApplicationViewModel.Mode))
            {
                ViewMode newValue = Model.Mode;
                ViewMode previousMode = Model.PreviousMode;

                KeyboardShortcutManager.DisableShortcuts(false);

                switch (newValue)
                {
                    case ViewMode.Standard:
                        EnsureCalculator();
                        Model.CalculatorViewModel.HistoryVM.AreHistoryShortcutsEnabled = true;
                        m_calculator.AnimateCalculator(NavCategory.IsConverterViewMode(previousMode));
                        Model.CalculatorViewModel.HistoryVM.ReloadHistory(newValue);
                        break;
                    case ViewMode.Scientific:
                        EnsureCalculator();
                        Model.CalculatorViewModel.HistoryVM.AreHistoryShortcutsEnabled = true;
                        if (Model.PreviousMode != ViewMode.Scientific)
                        {
                            m_calculator.AnimateCalculator(NavCategory.IsConverterViewMode(previousMode));
                        }
                        Model.CalculatorViewModel.HistoryVM.ReloadHistory(newValue);
                        break;
                    case ViewMode.Programmer:
                        Model.CalculatorViewModel.HistoryVM.AreHistoryShortcutsEnabled = false;
                        EnsureCalculator();
                        if (Model.PreviousMode != ViewMode.Programmer)
                        {
                            m_calculator.AnimateCalculator(NavCategory.IsConverterViewMode(previousMode));
                        }
                        break;
                    default:
                        if (NavCategory.IsDateCalculatorViewMode(newValue))
                        {
                            if (Model.CalculatorViewModel != null)
                            {
                                Model.CalculatorViewModel.HistoryVM.AreHistoryShortcutsEnabled = false;
                            }
                            EnsureDateCalculator();
                        }
                        else if (NavCategory.IsConverterViewMode(newValue))
                        {
                            if (Model.CalculatorViewModel != null)
                            {
                                Model.CalculatorViewModel.HistoryVM.AreHistoryShortcutsEnabled = false;
                            }

                            EnsureConverter();
                            if (!NavCategory.IsConverterViewMode(previousMode))
                            {
                                m_converter.AnimateConverter();
                            }
                        }
                        break;
                }

                ShowHideControls(newValue);

                UpdateViewState();
                SetDefaultFocus();
            }
            else if (propertyName == nameof(ApplicationViewModel.CategoryName))
            {
                SetHeaderAutomationName();
                AnnounceCategoryName();
            }
        }

        private void SelectNavigationItemByModel()
        {
            var menuItems = (List<object>)NavView.MenuItemsSource;
            var itemCount = menuItems.Count;
            var flatIndex = NavCategoryStates.GetFlatIndex(Model.Mode);

            if (flatIndex >= 0 && flatIndex < itemCount)
            {
                NavView.SelectedItem = menuItems[flatIndex];
            }
        }

        private void OnNavLoaded(object sender, RoutedEventArgs e)
        {
            if (NavView.SelectedItem == null)
            {
                SelectNavigationItemByModel();
            }

            var acceleratorList = new List<MyVirtualKey>();
            NavCategoryStates.GetCategoryAcceleratorKeys(acceleratorList);

            foreach (var accelerator in acceleratorList)
            {
                NavView.SetValue(KeyboardShortcutManager.VirtualKeyAltChordProperty, accelerator);
            }
            // Special case logic for Ctrl+E accelerator for Date Calculation Mode
            NavView.SetValue(KeyboardShortcutManager.VirtualKeyControlChordProperty, MyVirtualKey.E);
        }

        private void OnNavPaneOpened(NavigationView sender, object args)
        {
            KeyboardShortcutManager.HonorShortcuts(false);
            TraceLogger.GetInstance().LogNavBarOpened();
        }

        private void OnNavPaneClosed(NavigationView sender, object args)
        {
            if (Popup.IsOpen)
            {
                return;
            }

            KeyboardShortcutManager.HonorShortcuts(true);

            SetDefaultFocus();
        }

        private void EnsurePopupContent()
        {
            if (PopupContent == null)
            {
                FindName("PopupContent");

                var windowBounds = Window.Current.Bounds;
                PopupContent.Width = windowBounds.Width;
                PopupContent.Height = windowBounds.Height;
            }
        }

        private void ShowSettingsPopup()
        {
            ChangelogPopup.IsOpen = false;
            UpdatePopup.IsOpen = false;
            EnsurePopupContent();
            Popup.IsOpen = true;
        }

        private void CloseSettingsPopup()
        {
            Popup.IsOpen = false;
            SelectNavigationItemByModel();
            SetDefaultFocus();
        }

        private void Popup_Opened(object sender, object e)
        {
            KeyboardShortcutManager.IgnoreEscape(false);
            KeyboardShortcutManager.HonorShortcuts(false);
        }

        private void Popup_Closed(object sender, object e)
        {
            KeyboardShortcutManager.HonorEscape();
            KeyboardShortcutManager.HonorShortcuts(!NavView.IsPaneOpen);
        }

        private void ChangelogButton_Click(object sender, RoutedEventArgs e)
        {
            Popup.IsOpen = false;
            HistoryPopup.IsOpen = false;
            UpdatePopup.IsOpen = false;
            var bounds = Window.Current.Bounds;
            ChangelogPopupContent.Width = bounds.Width;
            ChangelogPopupContent.Height = bounds.Height;
            ChangelogPopup.IsOpen = true;
        }

        private void GlobalHistoryButton_Click(object sender, RoutedEventArgs e)
        {
            ShowPersistentHistory();
        }

        private void Changelog_CloseRequested(object sender, EventArgs e)
        {
            ChangelogPopup.IsOpen = false;
        }

        private void ChangelogPopup_Opened(object sender, object e)
        {
            KeyboardShortcutManager.IgnoreEscape(false);
            KeyboardShortcutManager.HonorShortcuts(false);
        }

        private void ChangelogPopup_Closed(object sender, object e)
        {
            KeyboardShortcutManager.HonorEscape();
            KeyboardShortcutManager.HonorShortcuts(!NavView.IsPaneOpen);
            SelectNavigationItemByModel();
            SetDefaultFocus();
        }

        private void ShowPersistentHistory()
        {
            Popup.IsOpen = false;
            ChangelogPopup.IsOpen = false;
            UpdatePopup.IsOpen = false;
            m_calculator?.CloseHistoryFlyout();
            m_calculator?.CloseMemoryFlyout();

            var categories = Model.Categories
                .SelectMany(group => group.Categories)
                .Select(category => category.Name);
            PersistentHistoryView.Initialize(_persistentHistory, categories);

            var bounds = Window.Current.Bounds;
            HistoryPopupContent.Width = bounds.Width;
            HistoryPopupContent.Height = bounds.Height;
            HistoryPopup.IsOpen = true;
        }

        private void PersistentHistory_CloseRequested(object sender, EventArgs e)
        {
            HistoryPopup.IsOpen = false;
        }

        private void HistoryPopup_Opened(object sender, object e)
        {
            KeyboardShortcutManager.IgnoreEscape(false);
            KeyboardShortcutManager.HonorShortcuts(false);
        }

        private void HistoryPopup_Closed(object sender, object e)
        {
            KeyboardShortcutManager.HonorEscape();
            KeyboardShortcutManager.HonorShortcuts(!NavView.IsPaneOpen);
            SelectNavigationItemByModel();
            SetDefaultFocus();
        }

        private async void PersistentHistory_ClearRequested(object sender, EventArgs e)
        {
            _suppressNativeHistorySave = true;
            try
            {
                var history = Model.CalculatorViewModel.HistoryVM;
                history.ReloadHistory(ViewMode.Standard);
                if (history.ItemsCount > 0)
                {
                    history.ClearCommand.Execute(null);
                }

                history.ReloadHistory(ViewMode.Scientific);
                if (history.ItemsCount > 0)
                {
                    history.ClearCommand.Execute(null);
                }

                if (Model.Mode == ViewMode.Standard || Model.Mode == ViewMode.Scientific)
                {
                    history.ReloadHistory(Model.Mode);
                }
            }
            finally
            {
                _suppressNativeHistorySave = false;
            }

            await _persistentHistory.ClearAsync();
            await _persistentHistory.SaveAsync(Model.CalculatorViewModel.HistorySnapshot);
            PersistentHistoryView.Refresh();
        }

        private void PersistentHistory_EntryInvoked(object sender, PersistentHistoryEntryEventArgs e)
        {
            var category = Model.Categories
                .SelectMany(group => group.Categories)
                .FirstOrDefault(item => string.Equals(
                    item.Name,
                    e.Entry.Mode,
                    StringComparison.CurrentCultureIgnoreCase));
            if (category == null)
            {
                return;
            }

            if (NavCategory.IsConverterViewMode(category.ViewMode))
            {
                // Mode changes update the converter session id and several observable
                // values. Invalidate any already queued capture and suppress the whole
                // transition so recalling history remains a read-only action.
                _converterHistoryCaptureGeneration++;
                bool wasSuppressed = _suppressConverterHistoryCapture;
                _suppressConverterHistoryCapture = true;
                try
                {
                    Model.Mode = category.ViewMode;
                    RecallConverterHistory(e.Entry);
                }
                finally
                {
                    _suppressConverterHistoryCapture = wasSuppressed;
                }
            }
            else if (NavCategory.IsDateCalculatorViewMode(category.ViewMode))
            {
                _dateHistoryCaptureGeneration++;
                bool wasSuppressed = _suppressDateHistoryCapture;
                _suppressDateHistoryCapture = true;
                try
                {
                    Model.Mode = category.ViewMode;
                    RecallDateHistory(e.Entry);
                }
                finally
                {
                    _suppressDateHistoryCapture = wasSuppressed;
                }
            }
            else if (NavCategory.IsCalculatorViewMode(category.ViewMode))
            {
                bool wasSuppressed = _suppressCalculatorHistoryCapture;
                _suppressCalculatorHistoryCapture = true;
                try
                {
                    Model.Mode = category.ViewMode;
                    RecallCalculatorHistory(e.Entry, category.ViewMode);
                }
                finally
                {
                    _suppressCalculatorHistoryCapture = wasSuppressed;
                }
            }
            HistoryPopup.IsOpen = false;
        }

        private void RecallCalculatorHistory(PersistentHistoryEntry entry, ViewMode mode)
        {
            if (entry == null)
            {
                return;
            }

            var calculator = Model.CalculatorViewModel;
            if (mode == ViewMode.Programmer)
            {
                // Older entries did not record their radix. Decimal is the only safe
                // fallback because their visible history value used decimal grouping.
                NumberBase radix = NumberBase.DecBase;
                if (!string.IsNullOrWhiteSpace(entry.ProgrammerRadix))
                {
                    if (Enum.TryParse(entry.ProgrammerRadix, out NumberBase storedRadix)
                        && (storedRadix == NumberBase.HexBase
                            || storedRadix == NumberBase.DecBase
                            || storedRadix == NumberBase.OctBase
                            || storedRadix == NumberBase.BinBase))
                    {
                        radix = storedRadix;
                    }
                }

                if (!string.IsNullOrWhiteSpace(entry.ProgrammerBitLength)
                    && Enum.TryParse(entry.ProgrammerBitLength, out BitLength bitLength)
                    && bitLength != BitLength.BitLengthUnknown)
                {
                    calculator.ValueBitLength = bitLength;
                }
                calculator.SwitchProgrammerModeBase(radix);
            }
            else if (mode == ViewMode.Scientific)
            {
                if (!string.IsNullOrWhiteSpace(entry.ScientificAngle)
                    && Enum.TryParse(entry.ScientificAngle, out NumbersAndOperatorsEnum angle)
                    && (angle == NumbersAndOperatorsEnum.Degree
                        || angle == NumbersAndOperatorsEnum.Radians
                        || angle == NumbersAndOperatorsEnum.Grads)
                    && calculator.PersistentHistoryAngleType != angle)
                {
                    calculator.SwitchAngleType(angle);
                }

                if (entry.ScientificFToE.HasValue
                    && calculator.IsFToEChecked != entry.ScientificFToE.Value)
                {
                    calculator.FtoEButtonToggled();
                }
            }

            string value = string.IsNullOrWhiteSpace(entry.CalculatorValue)
                ? RemoveHistoryGrouping(entry.Result)
                : entry.CalculatorValue;
            calculator.RecallPersistentHistory(entry.Expression, value);
        }

        private static string RemoveHistoryGrouping(string value)
        {
            return value?
                .Replace(" ", string.Empty)
                .Replace("\u00A0", string.Empty)
                .Replace("\u202F", string.Empty);
        }

        private void RecallConverterHistory(PersistentHistoryEntry entry)
        {
            string sourceValue = entry?.SourceValue;
            string sourceUnit = entry?.SourceUnit;
            string targetUnit = entry?.TargetUnit;
            bool hasCanonicalSource = !string.IsNullOrWhiteSpace(sourceValue)
                && !string.IsNullOrWhiteSpace(sourceUnit)
                && !string.IsNullOrWhiteSpace(targetUnit);

            string source = null;
            if (!hasCanonicalSource
                && !TrySplitConverterHistoryEntry(entry, out source, out targetUnit))
            {
                return;
            }

            var converter = Model.ConverterViewModel;
            var unit1 = hasCanonicalSource
                ? converter.Units.FirstOrDefault(unit => string.Equals(
                    unit.Abbreviation,
                    sourceUnit,
                    StringComparison.CurrentCultureIgnoreCase))
                : converter.Units
                    .Where(unit => !string.IsNullOrWhiteSpace(unit.Abbreviation))
                    .OrderByDescending(unit => unit.Abbreviation.Length)
                    .FirstOrDefault(unit => source.EndsWith(
                        $" {unit.Abbreviation}",
                        StringComparison.CurrentCultureIgnoreCase));
            var unit2 = converter.Units.FirstOrDefault(unit => string.Equals(
                unit.Abbreviation,
                targetUnit,
                StringComparison.CurrentCultureIgnoreCase));
            if (unit1 == null || unit2 == null)
            {
                return;
            }

            if (!hasCanonicalSource)
            {
                sourceValue = source
                    .Substring(0, source.Length - unit1.Abbreviation.Length)
                    .TrimEnd();
                sourceValue = NormalizeLegacyConverterHistoryValue(
                    sourceValue,
                    Model.Mode == ViewMode.Currency);
            }
            if (string.IsNullOrWhiteSpace(sourceValue))
            {
                return;
            }

            bool wasSuppressed = _suppressConverterHistoryCapture;
            _suppressConverterHistoryCapture = true;
            try
            {
                converter.Unit1 = unit1;
                converter.Unit2 = unit2;
                converter.Value1Active = true;
                converter.Value2Active = false;
                converter.RestorePersistentHistoryValue(sourceValue);
            }
            finally
            {
                _suppressConverterHistoryCapture = wasSuppressed;
            }
        }

        private static string NormalizeLegacyConverterHistoryValue(string value, bool isCurrency)
        {
            var styles = NumberStyles.Float | NumberStyles.AllowThousands;
            var primaryCulture = isCurrency ? CultureInfo.InvariantCulture : CultureInfo.CurrentCulture;
            var fallbackCulture = isCurrency ? CultureInfo.CurrentCulture : CultureInfo.InvariantCulture;
            if (double.TryParse(value, styles, primaryCulture, out double parsed)
                || double.TryParse(value, styles, fallbackCulture, out parsed))
            {
                return parsed.ToString("R", CultureInfo.InvariantCulture);
            }

            return value;
        }

        private static bool TrySplitConverterHistoryEntry(
            PersistentHistoryEntry entry,
            out string source,
            out string targetUnit)
        {
            source = null;
            targetUnit = null;
            if (entry == null || string.IsNullOrWhiteSpace(entry.Expression))
            {
                return false;
            }

            int arrowIndex = entry.Expression.IndexOf('\u2192');
            if (arrowIndex < 0)
            {
                return false;
            }

            source = entry.Expression.Substring(0, arrowIndex).Trim();
            targetUnit = entry.Expression.Substring(arrowIndex + 1).Trim();
            return !string.IsNullOrWhiteSpace(source)
                && !string.IsNullOrWhiteSpace(targetUnit);
        }

        private void RecallDateHistory(PersistentHistoryEntry entry)
        {
            if (entry == null || string.IsNullOrWhiteSpace(entry.Expression))
            {
                return;
            }

            bool wasSuppressed = _suppressDateHistoryCapture;
            _suppressDateHistoryCapture = true;
            try
            {
                var date = Model.DateCalcViewModel;
                Match difference = Regex.Match(entry.Expression, @"^From (?<from>.+) to (?<to>.+)$");
                if (difference.Success
                    && TryParseHistoryDate(difference.Groups["from"].Value, out DateTimeOffset fromDate)
                    && TryParseHistoryDate(difference.Groups["to"].Value, out DateTimeOffset toDate))
                {
                    date.IsDateDiffMode = true;
                    date.FromDate = fromDate;
                    date.ToDate = toDate;
                    return;
                }

                Match offset = Regex.Match(
                    entry.Expression,
                    @"^(?<date>.+?)\s(?<operation>[+\u2212-])\s(?<years>-?\d+)y\s(?<months>-?\d+)m\s(?<days>-?\d+)d$");
                if (offset.Success
                    && TryParseHistoryDate(offset.Groups["date"].Value, out DateTimeOffset startDate)
                    && int.TryParse(offset.Groups["years"].Value, NumberStyles.Integer, CultureInfo.InvariantCulture, out int years)
                    && int.TryParse(offset.Groups["months"].Value, NumberStyles.Integer, CultureInfo.InvariantCulture, out int months)
                    && int.TryParse(offset.Groups["days"].Value, NumberStyles.Integer, CultureInfo.InvariantCulture, out int days))
                {
                    date.IsDateDiffMode = false;
                    date.IsAddMode = offset.Groups["operation"].Value == "+";
                    date.StartDate = startDate;
                    date.YearsOffset = years;
                    date.MonthsOffset = months;
                    date.DaysOffset = days;
                }
            }
            finally
            {
                _suppressDateHistoryCapture = wasSuppressed;
            }
        }

        private static bool TryParseHistoryDate(string value, out DateTimeOffset date)
        {
            return DateTimeOffset.TryParse(
                value,
                CultureInfo.CurrentCulture,
                DateTimeStyles.AllowWhiteSpaces,
                out date);
        }

        private void OnNavSelectionChanged(object sender, NavigationViewSelectionChangedEventArgs e)
        {
            if (e.IsSettingsSelected)
            {
                ShowSettingsPopup();
                return;
            }

            if (e.SelectedItemContainer is NavigationViewItem item)
            {
                Model.Mode = (ViewMode)item.Tag;
            }
        }

        private void OnNavItemInvoked(NavigationView sender, NavigationViewItemInvokedEventArgs e)
        {
            NavView.IsPaneOpen = false;
        }

        private void ShowHideControls(ViewMode mode)
        {
            var isCalcViewMode = NavCategory.IsCalculatorViewMode(mode);
            var isDateCalcViewMode = NavCategory.IsDateCalculatorViewMode(mode);
            var isConverterViewMode = NavCategory.IsConverterViewMode(mode);

            if (m_calculator != null)
            {
                m_calculator.Visibility = BooleanToVisibilityConverter.Convert(isCalcViewMode);
                m_calculator.IsEnabled = isCalcViewMode;
            }

            if (m_dateCalculator != null)
            {
                m_dateCalculator.Visibility = BooleanToVisibilityConverter.Convert(isDateCalcViewMode);
                m_dateCalculator.IsEnabled = isDateCalcViewMode;
            }

            if (m_converter != null)
            {
                m_converter.Visibility = BooleanToVisibilityConverter.Convert(isConverterViewMode);
                m_converter.IsEnabled = isConverterViewMode;
            }
        }

        private void UpdateViewState()
        {
            // All layout related view states are now handled only inside individual controls (standard, scientific, programmer, date, converter)
            if (NavCategory.IsConverterViewMode(Model.Mode))
            {
                int modeIndex = NavCategoryStates.GetIndexInGroup(Model.Mode, CategoryGroupType.Converter);
                Model.ConverterViewModel.CurrentCategory = Model.ConverterViewModel.Categories[modeIndex];
            }
        }

        private void UpdatePanelViewState()
        {
            if (m_calculator != null)
            {
                m_calculator.UpdatePanelViewState();
            }
        }

        private void OnHighContrastChanged(AccessibilitySettings sender, object args)
        {
            if (Model.IsAlwaysOnTop && ActualHeight < 394)
            {
                // Sets to default always-on-top size to force re-layout
                ApplicationView.GetForCurrentView().TryResizeView(new Size(320, 394));
            }
        }

        private async void OnPageLoaded(object sender, RoutedEventArgs args)
        {
            if (m_converter == null && m_calculator == null && m_dateCalculator == null)
            {
                // We have just launched into our default mode (standard calc) so ensure calc is loaded
                EnsureCalculator();
                Model.CalculatorViewModel.IsStandard = true;
            }

            // History must be loaded even when the saved launch mode is a converter
            // or Date Calculator and the calculator control is still delay-loaded.
            Model.EnsureCalculatorViewModel();
            await InitializePersistentHistoryAsync();

            _ = CheckForUpdatesAsync(false);

            Window.Current.SizeChanged += WindowSizeChanged;
            m_accessibilitySettings.HighContrastChanged += OnHighContrastChanged;
            UpdateViewState();

            SetHeaderAutomationName();
            SetDefaultFocus();

            // Delay load things later when we get a chance.
            _ = Dispatcher.RunAsync(
                CoreDispatcherPriority.Normal, new DispatchedHandler(() =>
                {
                    if (TraceLogger.GetInstance().IsWindowIdInLog(ApplicationView.GetApplicationViewIdForWindow(CoreWindow.GetForCurrentThread())))
                    {
                        AppLifecycleLogger.GetInstance().LaunchUIResponsive();
                        AppLifecycleLogger.GetInstance().LaunchVisibleComplete();
                    }
                }));
        }

        private async void App_Suspending(object sender, Windows.ApplicationModel.SuspendingEventArgs e)
        {
            var deferral = e.SuspendingOperation.GetDeferral();
            if (Model.IsAlwaysOnTop)
            {
                ApplicationDataContainer localSettings = ApplicationData.Current.LocalSettings;
                localSettings.Values[ApplicationViewModel.WidthLocalSettingsKey] = ActualWidth;
                localSettings.Values[ApplicationViewModel.HeightLocalSettingsKey] = ActualHeight;
            }

            try
            {
                if (_persistentHistoryInitialized && Model.CalculatorViewModel != null)
                {
                    await _persistentHistory.SaveAsync(Model.CalculatorViewModel.HistorySnapshot);
                }
            }
            finally
            {
                deferral.Complete();
            }
        }

        private async Task InitializePersistentHistoryAsync()
        {
            if (_persistentHistoryInitialized || Model.CalculatorViewModel == null)
            {
                return;
            }

            try
            {
                var historyState = await _persistentHistory.LoadAsync();
                if (historyState != null)
                {
                    // Documents persistence restores history only. Mode, display, and
                    // in-progress command state always start from the fresh app launch.
                    Model.CalculatorViewModel.RestoreHistory(historyState);
                    if (Model.Mode == ViewMode.Standard || Model.Mode == ViewMode.Scientific)
                    {
                        Model.CalculatorViewModel.HistoryVM.ReloadHistory(Model.Mode);
                    }
                }

                Model.CalculatorViewModel.HistoryVM.HistoryChanged += OnPersistentHistoryChanged;
                Model.CalculatorViewModel.PropertyChanged -= OnCalculatorHistoryPropertyChanged;
                Model.CalculatorViewModel.PropertyChanged += OnCalculatorHistoryPropertyChanged;
                _persistentHistoryInitialized = true;

                // Create Documents\Persistent Calculator\History.txt on first launch,
                // even before the first completed calculation.
                await _persistentHistory.SaveAsync(Model.CalculatorViewModel.HistorySnapshot);
            }
            catch (Exception ex)
            {
                // Persistent history must never prevent the calculator from opening.
                TraceLogger.GetInstance().LogRecallError(
                    $"Persistent history could not be restored. {ex.GetType().Name}: {ex.Message}");
            }
        }

        private async void OnPersistentHistoryChanged()
        {
            if (_persistentHistoryInitialized && !_suppressNativeHistorySave)
            {
                await _persistentHistory.SaveAsync(Model.CalculatorViewModel.HistorySnapshot);
            }
        }

        private async void OnCalculatorHistoryPropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            if (!_persistentHistoryInitialized
                || _suppressCalculatorHistoryCapture
                || e.PropertyName != nameof(ViewModel.StandardCalculatorViewModel.PersistentExpression)
                || !NavCategory.IsCalculatorViewMode(Model.Mode))
            {
                return;
            }

            var calculator = Model.CalculatorViewModel;
            string expression = calculator.PersistentExpression?.Trim();
            string result = calculator.DisplayValue?.Trim();
            if (string.IsNullOrWhiteSpace(expression)
                || string.IsNullOrWhiteSpace(result))
            {
                return;
            }

            string sessionKey = $"calculator:{_persistentHistoryRunId}:{NavCategoryStates.Serialize(Model.Mode)}:{calculator.PersistentHistorySessionId}";
            string calculatorValue = calculator.PersistentHistoryValue?.Trim();
            string programmerRadix = Model.Mode == ViewMode.Programmer
                ? calculator.CurrentRadixType.ToString()
                : null;
            string programmerBitLength = Model.Mode == ViewMode.Programmer
                ? calculator.ValueBitLength.ToString()
                : null;
            string scientificAngle = Model.Mode == ViewMode.Scientific
                ? calculator.PersistentHistoryAngleType.ToString()
                : null;
            bool? scientificFToE = Model.Mode == ViewMode.Scientific
                ? (bool?)calculator.IsFToEChecked
                : null;
            await _persistentHistory.RecordSessionAsync(
                sessionKey,
                Model.CategoryName,
                expression,
                result,
                calculatorValue: calculatorValue,
                programmerRadix: programmerRadix,
                programmerBitLength: programmerBitLength,
                scientificAngle: scientificAngle,
                scientificFToE: scientificFToE);
        }

        private void OnConverterHistoryPropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            if (!_persistentHistoryInitialized
                || _suppressConverterHistoryCapture
                || !NavCategory.IsConverterViewMode(Model.Mode)
                || (e.PropertyName != nameof(ViewModel.UnitConverterViewModel.Value1)
                    && e.PropertyName != nameof(ViewModel.UnitConverterViewModel.Value2)
                    && e.PropertyName != nameof(ViewModel.UnitConverterViewModel.Unit1)
                    && e.PropertyName != nameof(ViewModel.UnitConverterViewModel.Unit2)
                    && e.PropertyName != nameof(ViewModel.UnitConverterViewModel.Value1Active)
                    && e.PropertyName != nameof(ViewModel.UnitConverterViewModel.Value2Active)
                    && e.PropertyName != nameof(ViewModel.UnitConverterViewModel.PersistentHistorySessionId)))
            {
                return;
            }

            ScheduleConverterHistoryCapture();
        }

        private void ScheduleConverterHistoryCapture()
        {
            int generation = _converterHistoryCaptureGeneration;
            if (_converterHistoryCaptureScheduledGeneration == generation)
            {
                return;
            }

            _converterHistoryCaptureScheduledGeneration = generation;
            _ = Dispatcher.RunAsync(CoreDispatcherPriority.Low, async () =>
            {
                try
                {
                    if (generation == _converterHistoryCaptureGeneration
                        && !_suppressConverterHistoryCapture)
                    {
                        await CaptureConverterHistoryAsync();
                    }
                }
                finally
                {
                    if (_converterHistoryCaptureScheduledGeneration == generation)
                    {
                        _converterHistoryCaptureScheduledGeneration = -1;
                    }
                }
            });
        }

        private async Task CaptureConverterHistoryAsync()
        {
            var converter = Model.ConverterViewModel;
            if (!_persistentHistoryInitialized
                || _suppressConverterHistoryCapture
                || converter?.Unit1 == null
                || converter.Unit2 == null
                || !NavCategory.IsConverterViewMode(Model.Mode))
            {
                return;
            }

            string value1 = converter.Value1?.Trim();
            string value2 = converter.Value2?.Trim();
            if (string.IsNullOrWhiteSpace(value1)
                || string.IsNullOrWhiteSpace(value2)
                || (value1 == "0" && value2 == "0"))
            {
                return;
            }

            bool firstIsSource = converter.Value1Active;
            string sourceValue = firstIsSource ? value1 : value2;
            string targetValue = firstIsSource ? value2 : value1;
            string sourceUnit = firstIsSource ? converter.Unit1.Abbreviation : converter.Unit2.Abbreviation;
            string targetUnit = firstIsSource ? converter.Unit2.Abbreviation : converter.Unit1.Abbreviation;
            string expression = $"{sourceValue} {sourceUnit} \u2192 {targetUnit}";
            string result = $"{targetValue} {targetUnit}";
            string sessionKey = $"converter:{_persistentHistoryRunId}:{NavCategoryStates.Serialize(Model.Mode)}:{converter.PersistentHistorySessionId}";
            string canonicalSourceValue = converter.PersistentHistorySourceValue?.Trim();

            await _persistentHistory.RecordSessionAsync(
                sessionKey,
                Model.CategoryName,
                expression,
                result,
                canonicalSourceValue,
                sourceUnit,
                targetUnit);
        }

        private void OnDateHistoryPropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            if (!_persistentHistoryInitialized
                || _suppressDateHistoryCapture
                || !NavCategory.IsDateCalculatorViewMode(Model.Mode)
                || (e.PropertyName != nameof(ViewModel.DateCalculatorViewModel.IsDateDiffMode)
                    && e.PropertyName != nameof(ViewModel.DateCalculatorViewModel.IsAddMode)
                    && e.PropertyName != nameof(ViewModel.DateCalculatorViewModel.FromDate)
                    && e.PropertyName != nameof(ViewModel.DateCalculatorViewModel.ToDate)
                    && e.PropertyName != nameof(ViewModel.DateCalculatorViewModel.StartDate)
                    && e.PropertyName != nameof(ViewModel.DateCalculatorViewModel.DaysOffset)
                    && e.PropertyName != nameof(ViewModel.DateCalculatorViewModel.MonthsOffset)
                    && e.PropertyName != nameof(ViewModel.DateCalculatorViewModel.YearsOffset)))
            {
                return;
            }

            int generation = _dateHistoryCaptureGeneration;
            if (_dateHistoryCaptureScheduledGeneration == generation)
            {
                return;
            }

            _dateHistoryCaptureScheduledGeneration = generation;
            _ = Dispatcher.RunAsync(CoreDispatcherPriority.Low, async () =>
            {
                try
                {
                    if (generation == _dateHistoryCaptureGeneration
                        && !_suppressDateHistoryCapture)
                    {
                        await CaptureDateHistoryAsync();
                    }
                }
                finally
                {
                    if (_dateHistoryCaptureScheduledGeneration == generation)
                    {
                        _dateHistoryCaptureScheduledGeneration = -1;
                    }
                }
            });
        }

        private async Task CaptureDateHistoryAsync()
        {
            var date = Model.DateCalcViewModel;
            if (_suppressDateHistoryCapture
                || date == null
                || !NavCategory.IsDateCalculatorViewMode(Model.Mode))
            {
                return;
            }

            string expression;
            string result;
            if (date.IsDateDiffMode)
            {
                expression = $"From {date.FromDate:d} to {date.ToDate:d}";
                result = date.StrDateDiffResult;
                if (!date.IsDiffInDays && !string.IsNullOrWhiteSpace(date.StrDateDiffResultInDays))
                {
                    result = $"{result} ({date.StrDateDiffResultInDays})";
                }
            }
            else
            {
                string operation = date.IsAddMode ? "+" : "\u2212";
                expression = $"{date.StartDate:d} {operation} {date.YearsOffset}y {date.MonthsOffset}m {date.DaysOffset}d";
                result = date.StrDateResult;
            }

            await _persistentHistory.AppendAsync(Model.CategoryName, expression, result);
        }

        private async void CheckForUpdatesButton_Click(object sender, RoutedEventArgs e)
        {
            await CheckForUpdatesAsync(true);
        }

        private async Task CheckForUpdatesAsync(bool userInitiated)
        {
            if (_updateCheckInProgress)
            {
                return;
            }

            _updateCheckInProgress = true;
            CheckForUpdatesButton.IsEnabled = false;
            ShowUpdateStatus("Checking...", true);

            try
            {
                var result = await UpdateService.CheckAsync();
                switch (result.Status)
                {
                    case UpdateCheckStatus.UpToDate:
                        await ShowTemporaryUpdateStatusAsync(
                            $"Up to date - V{PersistentCalculatorVersion.Current}",
                            TimeSpan.FromSeconds(3));
                        break;

                    case UpdateCheckStatus.UpdateAvailable:
                        HideUpdateStatus();
                        if (userInitiated || !IsUpdateIgnored(result.Version))
                        {
                            ShowUpdatePage(result);
                        }
                        break;

                    default:
                        await ShowTemporaryUpdateStatusAsync(
                            userInitiated ? "Check failed" : "Check unavailable",
                            TimeSpan.FromSeconds(2));
                        break;
                }
            }
            finally
            {
                _updateCheckInProgress = false;
                CheckForUpdatesButton.IsEnabled = true;
            }
        }

        private bool IsUpdateIgnored(string version)
        {
            string ignoredVersion = ApplicationData.Current.LocalSettings.Values[IgnoredUpdateVersionKey] as string;
            return !string.IsNullOrWhiteSpace(version)
                && string.Equals(ignoredVersion, version, StringComparison.OrdinalIgnoreCase);
        }

        private void ShowUpdatePage(UpdateCheckResult result)
        {
            _availableUpdate = result;
            Popup.IsOpen = false;
            ChangelogPopup.IsOpen = false;
            HistoryPopup.IsOpen = false;
            UpdateView.Initialize(result, IsUpdateIgnored(result.Version));

            var bounds = Window.Current.Bounds;
            UpdatePopupContent.Width = bounds.Width;
            UpdatePopupContent.Height = bounds.Height;
            UpdatePopup.IsOpen = true;
        }

        private void UpdateView_CloseRequested(object sender, EventArgs e)
        {
            UpdatePopup.IsOpen = false;
        }

        private void UpdateView_IgnoreChanged(object sender, UpdateIgnoreChangedEventArgs e)
        {
            var values = ApplicationData.Current.LocalSettings.Values;
            if (e.IsIgnored && !string.IsNullOrWhiteSpace(e.Version))
            {
                values[IgnoredUpdateVersionKey] = e.Version;
            }
            else
            {
                values.Remove(IgnoredUpdateVersionKey);
            }
        }

        private async void UpdateView_DownloadRequested(object sender, EventArgs e)
        {
            Uri releasePage = _availableUpdate?.ReleasePage ?? UpdateService.LatestReleasePage;
            bool opened = await Launcher.LaunchUriAsync(releasePage);
            if (!opened)
            {
                return;
            }

            if (_persistentHistoryInitialized && Model.CalculatorViewModel != null)
            {
                await _persistentHistory.SaveAsync(Model.CalculatorViewModel.HistorySnapshot);
            }

            ApplicationData.Current.LocalSettings.Values["PreviousSessionEndedCleanly"] = true;
            Application.Current.Exit();
        }

        private void UpdatePopup_Opened(object sender, object e)
        {
            KeyboardShortcutManager.IgnoreEscape(false);
            KeyboardShortcutManager.HonorShortcuts(false);
        }

        private void UpdatePopup_Closed(object sender, object e)
        {
            KeyboardShortcutManager.HonorEscape();
            KeyboardShortcutManager.HonorShortcuts(!NavView.IsPaneOpen);
            SelectNavigationItemByModel();
            SetDefaultFocus();
        }

        private void ShowUpdateStatus(string message, bool busy)
        {
            ++_updateStatusGeneration;
            UpdateStatusText.Text = message;
            UpdateStatusProgress.IsActive = busy;
            UpdateStatusProgress.Visibility = busy ? Visibility.Visible : Visibility.Collapsed;
            UpdateStatusDot.Visibility = busy ? Visibility.Collapsed : Visibility.Visible;
            UpdateStatusChip.Visibility = Visibility.Visible;
        }

        private void HideUpdateStatus()
        {
            ++_updateStatusGeneration;
            UpdateStatusProgress.IsActive = false;
            UpdateStatusDot.Visibility = Visibility.Collapsed;
            UpdateStatusChip.Visibility = Visibility.Collapsed;
        }

        private async Task ShowTemporaryUpdateStatusAsync(string message, TimeSpan duration)
        {
            ShowUpdateStatus(message, false);
            await HideUpdateStatusAfterAsync(duration, _updateStatusGeneration);
        }

        private async Task HideUpdateStatusAfterAsync(TimeSpan duration, int generation)
        {
            await Task.Delay(duration);
            if (generation == _updateStatusGeneration)
            {
                UpdateStatusProgress.IsActive = false;
                UpdateStatusDot.Visibility = Visibility.Collapsed;
                UpdateStatusChip.Visibility = Visibility.Collapsed;
            }
        }

        private void EnsureCalculator()
        {
            if (m_calculator == null)
            {
                // delay load calculator.
                m_calculator = new Calculator
                {
                    Name = "Calculator",
                    DataContext = Model.CalculatorViewModel
                };
                Binding isStandardBinding = new Binding
                {
                    Path = new PropertyPath("IsStandard")
                };
                m_calculator.SetBinding(Calculator.IsStandardProperty, isStandardBinding);
                Binding isScientificBinding = new Binding
                {
                    Path = new PropertyPath("IsScientific")
                };
                m_calculator.SetBinding(Calculator.IsScientificProperty, isScientificBinding);
                Binding isProgramerBinding = new Binding
                {
                    Path = new PropertyPath("IsProgrammer")
                };
                m_calculator.SetBinding(Calculator.IsProgrammerProperty, isProgramerBinding);
                Binding isAlwaysOnTopBinding = new Binding
                {
                    Path = new PropertyPath("IsAlwaysOnTop")
                };
                m_calculator.SetBinding(Calculator.IsAlwaysOnTopProperty, isAlwaysOnTopBinding);
                m_calculator.Style = CalculatorBaseStyle;

                CalcHolder.Child = m_calculator;

                // Calculator's "default" state is visible, but if we get delay loaded
                // when in converter, we should not be visible. This is not a problem for converter
                // since its default state is hidden.
                ShowHideControls(Model.Mode);
            }

            if (m_dateCalculator != null)
            {
                m_dateCalculator.CloseCalendarFlyout();
            }
        }

        private void EnsureDateCalculator()
        {
            if (m_dateCalculator == null)
            {
                // delay loading converter
                m_dateCalculator = new DateCalculator
                {
                    Name = "dateCalculator",
                    DataContext = Model.DateCalcViewModel
                };

                DateCalcHolder.Child = m_dateCalculator;
                Model.DateCalcViewModel.PropertyChanged -= OnDateHistoryPropertyChanged;
                Model.DateCalcViewModel.PropertyChanged += OnDateHistoryPropertyChanged;
            }

            if (m_calculator != null)
            {
                m_calculator.CloseHistoryFlyout();
                m_calculator.CloseMemoryFlyout();
            }
        }

        private void EnsureConverter()
        {
            if (m_converter == null)
            {
                // delay loading converter
                m_converter = new CalculatorApp.UnitConverter
                {
                    Name = "unitConverter",
                    DataContext = Model.ConverterViewModel,
                    Style = UnitConverterBaseStyle
                };
                ConverterHolder.Child = m_converter;
                Model.ConverterViewModel.PropertyChanged -= OnConverterHistoryPropertyChanged;
                Model.ConverterViewModel.PropertyChanged += OnConverterHistoryPropertyChanged;
            }
        }

        private void AnnounceCategoryName()
        {
            string categoryName = AutomationProperties.GetName(Header);
            NarratorAnnouncement announcement = CalculatorAnnouncement.GetCategoryNameChangedAnnouncement(categoryName);
            NarratorNotifier.Announce(announcement);
        }

        private GridLength DoubleToGridLength(double value)
        {
            return new GridLength(value);
        }

        private void Settings_BackButtonClick(object sender, RoutedEventArgs e)
        {
            CloseSettingsPopup();
        }

        private async Task ShowSnapshotLaunchErrorAsync()
        {
            var resProvider = AppResourceProvider.GetInstance();
            var dialog = new wuxc.ContentDialog
            {
                Title = resProvider.GetResourceString("AppName"),
                Content = new wuxc.TextBlock { Text = resProvider.GetResourceString("SnapshotRestoreError") },
                CloseButtonText = resProvider.GetResourceString("ErrorButtonOk"),
                DefaultButton = wuxc.ContentDialogButton.Close
            };
            await dialog.ShowAsync();
        }

        private Calculator m_calculator;
        private UnitConverter m_converter;
        private DateCalculator m_dateCalculator;
        private readonly AccessibilitySettings m_accessibilitySettings;
        private readonly PersistentHistoryService _persistentHistory = new PersistentHistoryService();
        private readonly string _persistentHistoryRunId = Guid.NewGuid().ToString("N");
        private bool _persistentHistoryInitialized;
        private bool _suppressNativeHistorySave;
        private bool _suppressCalculatorHistoryCapture;
        private bool _suppressConverterHistoryCapture;
        private bool _suppressDateHistoryCapture;
        private int _converterHistoryCaptureGeneration;
        private int _converterHistoryCaptureScheduledGeneration = -1;
        private int _dateHistoryCaptureGeneration;
        private int _dateHistoryCaptureScheduledGeneration = -1;
        private bool _updateCheckInProgress;
        private int _updateStatusGeneration;
        private UpdateCheckResult _availableUpdate;
    }
}
