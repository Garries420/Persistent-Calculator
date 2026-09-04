// Licensed under the MIT License.

using System;

using CalculatorApp.Common;

using Windows.UI.Xaml;
using Windows.UI.Xaml.Automation;
using Windows.UI.Xaml.Controls;

namespace CalculatorApp
{
    public sealed partial class UpdatePage : UserControl
    {
        private UpdateCheckResult _update;

        public UpdatePage()
        {
            InitializeComponent();
        }

        public event EventHandler CloseRequested;

        public event EventHandler DownloadRequested;

        public event EventHandler<UpdateIgnoreChangedEventArgs> IgnoreChanged;

        internal void Initialize(UpdateCheckResult update, bool isIgnored)
        {
            _update = update;
            string version = update?.Version ?? string.Empty;
            UpdateVersionText.Text = $"Version {version}";
            ChangelogVersionText.Text = $"Version {version}";
            ReleaseNotesText.Text = string.IsNullOrWhiteSpace(update?.ReleaseNotes)
                ? "No changelog available."
                : update.ReleaseNotes;
            IgnoreUpdateCheckBox.IsChecked = isIgnored;
            ShowUpdateArea();
            UpdatePrimaryButton();
        }

        private void ViewChangelogButton_Click(object sender, RoutedEventArgs e)
        {
            UpdateArea.Visibility = Visibility.Collapsed;
            UpdateChangelogArea.Visibility = Visibility.Visible;
        }

        private void CloseChangelogButton_Click(object sender, RoutedEventArgs e)
        {
            ShowUpdateArea();
        }

        private void IgnoreUpdateCheckBox_Click(object sender, RoutedEventArgs e)
        {
            bool isIgnored = IgnoreUpdateCheckBox.IsChecked == true;
            IgnoreChanged?.Invoke(
                this,
                new UpdateIgnoreChangedEventArgs(_update?.Version, isIgnored));
            UpdatePrimaryButton();
        }

        private void PrimaryUpdateButton_Click(object sender, RoutedEventArgs e)
        {
            if (IgnoreUpdateCheckBox.IsChecked == true)
            {
                CloseRequested?.Invoke(this, EventArgs.Empty);
            }
            else
            {
                DownloadRequested?.Invoke(this, EventArgs.Empty);
            }
        }

        private void ShowUpdateArea()
        {
            UpdateChangelogArea.Visibility = Visibility.Collapsed;
            UpdateArea.Visibility = Visibility.Visible;
        }

        private void UpdatePrimaryButton()
        {
            bool isIgnored = IgnoreUpdateCheckBox.IsChecked == true;
            PrimaryUpdateButton.Content = isIgnored ? "Close" : "Download";
            AutomationProperties.SetName(
                PrimaryUpdateButton,
                isIgnored ? "Close update" : "Download update");
        }
    }

    public sealed class UpdateIgnoreChangedEventArgs : EventArgs
    {
        public UpdateIgnoreChangedEventArgs(string version, bool isIgnored)
        {
            Version = version;
            IsIgnored = isIgnored;
        }

        public string Version { get; }

        public bool IsIgnored { get; }
    }
}
