// Licensed under the MIT License.

using System;

using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;

namespace CalculatorApp
{
    public sealed partial class Changelog : UserControl
    {
        public Changelog()
        {
            InitializeComponent();
        }

        public event EventHandler CloseRequested;

        private void BackButton_Click(object sender, RoutedEventArgs e)
        {
            CloseRequested?.Invoke(this, EventArgs.Empty);
        }
    }
}
