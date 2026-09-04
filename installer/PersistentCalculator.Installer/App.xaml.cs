using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Windows;

namespace PersistentCalculator.Installer;

public partial class App : Application
{
    private const string InstalledLauncherName = "Persistent Calculator.exe";

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        string? executablePath = Assembly.GetEntryAssembly()?.Location;
        if (string.Equals(
            Path.GetFileName(executablePath),
            InstalledLauncherName,
            StringComparison.OrdinalIgnoreCase))
        {
            try
            {
                Process.Start(new ProcessStartInfo("persistent-calculator:") { UseShellExecute = true });
                Shutdown();
            }
            catch (Exception ex)
            {
                MessageBox.Show(
                    $"Persistent Calculator could not be opened.\n\n{ex.Message}",
                    "Persistent Calculator",
                    MessageBoxButton.OK,
                    MessageBoxImage.Error);
                Shutdown(1);
            }
            return;
        }

        new MainWindow().Show();
    }
}
