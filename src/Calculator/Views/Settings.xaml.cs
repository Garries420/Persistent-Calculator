using CalculatorApp.Common;
using CalculatorApp.Utils;
using CalculatorApp.ViewModel.Common;
using CalculatorApp.ViewModel.Common.Automation;

using System;
using System.Linq;

using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Automation.Peers;
using Windows.UI.Xaml.Automation.Provider;
using Windows.UI.Xaml.Controls;

// The User Control item template is documented at https://go.microsoft.com/fwlink/?LinkId=234236

namespace CalculatorApp
{
    public sealed partial class Settings : UserControl
    {
        private const string BUILD_YEAR = "2026";

        public event Windows.UI.Xaml.RoutedEventHandler BackButtonClick;

        public GridLength TitleBarHeight
        {
            get => (GridLength)GetValue(TitleBarHeightProperty);
            set => SetValue(TitleBarHeightProperty, value);
        }
        public static readonly DependencyProperty TitleBarHeightProperty =
            DependencyProperty.Register(nameof(TitleBarHeight), typeof(GridLength), typeof(Settings), new PropertyMetadata(default(GridLength)));

        public Settings()
        {
            var locService = LocalizationService.GetInstance();

            InitializeComponent();

            Language = locService.GetLanguage();

            InitializeAboutContentTextBlock();
            AboutExpander.Description = $"\u00A9 {BUILD_YEAR} Garries \u00B7 Independent open-source project";
        }

        private void OnThemeSelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (e.AddedItems.Count > 0 && e.AddedItems[0] is RadioButton selectItem)
            {
                ThemeHelper.RootTheme = ThemeHelper.GetEnum<ElementTheme>(selectItem.Tag.ToString());
            }
        }

        public void SetDefaultFocus()
        {
            AppThemeExpander.Focus(FocusState.Programmatic);
        }

        // OnLoaded would be invoked by Popup several times while contructed once
        private void OnLoaded(object sender, RoutedEventArgs args)
        {
            SystemNavigationManager.GetForCurrentView().BackRequested += System_BackRequested;

            AnnouncePageOpened();

            var currentTheme = ThemeHelper.RootTheme.ToString();
            (ThemeRadioButtons.Items.Cast<RadioButton>().FirstOrDefault(c => c?.Tag?.ToString() == currentTheme)).IsChecked = true;

            SetDefaultFocus();
        }

        private void AnnouncePageOpened()
        {
            string announcementText = AppResourceProvider.GetInstance().GetResourceString("SettingsPageOpenedAnnouncement");
            NarratorAnnouncement announcement = CalculatorAnnouncement.GetSettingsPageOpenedAnnouncement(announcementText);
            NarratorNotifier.Announce(announcement);
        }

        // OnUnloaded would be invoked by Popup several times while contructed once
        private void OnUnloaded(object sender, RoutedEventArgs e)
        {
            // back to the default state
            AppThemeExpander.IsExpanded = false;

            SystemNavigationManager.GetForCurrentView().BackRequested -= System_BackRequested;
        }

        private void InitializeAboutContentTextBlock()
        {
            SetVersionString();
        }

        private void SetVersionString()
        {
            AboutBuildVersion.Text = $"Version {PersistentCalculatorVersion.Current}";
        }

        private void System_BackRequested(object sender, BackRequestedEventArgs e)
        {
            if (!e.Handled && BackButton.IsEnabled)
            {
                var buttonPeer = new ButtonAutomationPeer(BackButton);
                IInvokeProvider invokeProvider = buttonPeer.GetPattern(PatternInterface.Invoke) as IInvokeProvider;
                invokeProvider.Invoke();

                e.Handled = true;
            }
        }

        private void BackButton_Click(object sender, RoutedEventArgs e)
        {
            BackButtonClick?.Invoke(this, e);
        }
    }
}
