using Microsoft.Win32;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Media;

namespace PersistentCalculator.Uninstaller;

public partial class MainWindow : Window
{
    private const string ProductName = "Persistent Calculator";
    private const string PackageName = "Garries420.PersistentCalculator";
    private const string LauncherFileName = "Persistent Calculator.exe";
    private const string UninstallerFileName = "Uninstall Persistent Calculator.exe";
    private const string UninstallKeyPath = @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\PersistentCalculator";
    private const string LegacyManifestFileName = "installation-manifest.txt";
    private const string LegacyOptionsFileName = "installation-options.ini";
    private const string LegacyMarkerFileName = ".persistent-calculator-install";
    private readonly string _installRoot = Path.GetFullPath(AppContext.BaseDirectory).TrimEnd(Path.DirectorySeparatorChar);
    private bool _isWorking;
    private bool _uninstallComplete;
    private bool _cleanupScheduled;

    public ObservableCollection<string> ActivityItems { get; } = new ObservableCollection<string>();

    public MainWindow()
    {
        InitializeComponent();
        DataContext = this;
        Closing += MainWindow_Closing;
        Closed += MainWindow_Closed;
        SystemEvents.UserPreferenceChanged += SystemEvents_UserPreferenceChanged;
        ApplySystemTheme();
        Log("Ready", $"Application folder: {_installRoot}");
        Log("Keep", Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), ProductName));
    }

    private async void UninstallButton_Click(object sender, RoutedEventArgs e)
    {
        _isWorking = true;
        CancelButton.IsEnabled = false;
        UninstallButton.IsEnabled = false;
        StatusText.Text = "Uninstalling Persistent Calculator";

        try
        {
            await Task.Run(UninstallCore);
            _uninstallComplete = true;
            StatusText.Text = "Uninstallation complete";
            Progress.Value = 100;
            CancelButton.Content = "Close";
            CancelButton.IsEnabled = true;
            UninstallButton.Visibility = Visibility.Collapsed;
        }
        catch (Exception ex)
        {
            Log("Error", ex.Message);
            StatusText.Text = "Uninstallation could not finish";
            CancelButton.Content = "Close";
            CancelButton.IsEnabled = true;
        }
        finally
        {
            _isWorking = false;
        }
    }

    private void UninstallCore()
    {
        VerifyInstallRoot();
        string registeredRoot;
        string[] managedFiles = ReadManagedFiles(out registeredRoot);
        if (!string.IsNullOrWhiteSpace(registeredRoot) && !PathsEqual(registeredRoot, _installRoot))
        {
            Log("Use", $"Uninstaller location {_installRoot}; registered location was {registeredRoot}");
        }

        SetProgress(5);
        Log("Remove", "Windows app registration");
        RunPowerShell($"$package = Get-AppxPackage -Name '{PackageName}'; if ($null -ne $package) {{ $package | Remove-AppxPackage -ErrorAction Stop }}");

        SetProgress(18);
        RemoveShortcut(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), ProductName + ".lnk"));
        RemoveShortcut(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), ProductName, ProductName + ".lnk"));

        string currentExecutable = Process.GetCurrentProcess().MainModule?.FileName ?? Path.Combine(_installRoot, UninstallerFileName);
        int index = 0;
        foreach (string relativePath in managedFiles)
        {
            index++;
            string fullPath = GetManagedPath(relativePath);
            if (string.Equals(fullPath, currentExecutable, StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }
            if (File.Exists(fullPath))
            {
                File.Delete(fullPath);
                Log("Remove", fullPath);
            }
            SetProgress(22 + (int)(62.0 * index / Math.Max(1, managedFiles.Length)));
        }

        RemoveLegacyMetadata();
        RemoveEmptySubdirectories();

        SetProgress(90);
        using (RegistryKey baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
        {
            baseKey.DeleteSubKeyTree(UninstallKeyPath, false);
        }
        Log("Remove", "Programs and Features registration");

        SetProgress(96);
        Log("Keep", Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), ProductName));
        Log("Done", "Application files removed. Personal data was not touched.");
    }

    private void VerifyInstallRoot()
    {
        bool currentLayout = File.Exists(Path.Combine(_installRoot, "AppxManifest.xml"))
            && File.Exists(Path.Combine(_installRoot, LauncherFileName));
        bool legacyLayout = File.Exists(Path.Combine(_installRoot, "App", "AppxManifest.xml"))
            && File.Exists(Path.Combine(_installRoot, LauncherFileName));
        if (!currentLayout && !legacyLayout)
        {
            throw new InvalidDataException("This uninstaller is not inside a Persistent Calculator installation folder.");
        }
    }

    private string[] ReadManagedFiles(out string registeredRoot)
    {
        registeredRoot = string.Empty;
        try
        {
            using RegistryKey baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
            using RegistryKey? key = baseKey.OpenSubKey(UninstallKeyPath);
            registeredRoot = key?.GetValue("InstallLocation") as string ?? string.Empty;
            if (PathsEqual(registeredRoot, _installRoot)
                && key?.GetValue("ManagedFiles") is string[] registeredFiles
                && registeredFiles.Length > 0)
            {
                return registeredFiles.Where(path => !string.IsNullOrWhiteSpace(path)).ToArray();
            }
        }
        catch
        {
        }

        string legacyManifest = Path.Combine(_installRoot, LegacyManifestFileName);
        if (File.Exists(legacyManifest))
        {
            return File.ReadAllLines(legacyManifest).Where(path => !string.IsNullOrWhiteSpace(path)).ToArray();
        }

        throw new InvalidDataException("Windows could not find the list of installed Persistent Calculator files. Reinstall to the same folder, then run the uninstaller again.");
    }

    private static bool PathsEqual(string left, string right)
    {
        if (string.IsNullOrWhiteSpace(left) || string.IsNullOrWhiteSpace(right))
        {
            return false;
        }
        try
        {
            return string.Equals(
                Path.GetFullPath(left).TrimEnd(Path.DirectorySeparatorChar),
                Path.GetFullPath(right).TrimEnd(Path.DirectorySeparatorChar),
                StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false;
        }
    }

    private string GetManagedPath(string relativePath)
    {
        string fullPath = Path.GetFullPath(Path.Combine(_installRoot, relativePath));
        string rootPrefix = _installRoot + Path.DirectorySeparatorChar;
        if (!fullPath.StartsWith(rootPrefix, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException($"Unsafe installed-file entry: {relativePath}");
        }
        return fullPath;
    }

    private void RemoveLegacyMetadata()
    {
        foreach (string name in new[] { LegacyManifestFileName, LegacyOptionsFileName, LegacyMarkerFileName })
        {
            string path = Path.Combine(_installRoot, name);
            if (File.Exists(path))
            {
                File.Delete(path);
                Log("Remove", path);
            }
        }
    }

    private void RemoveEmptySubdirectories()
    {
        foreach (string directory in Directory.EnumerateDirectories(_installRoot, "*", SearchOption.AllDirectories)
                     .OrderByDescending(path => path.Length))
        {
            if (!Directory.EnumerateFileSystemEntries(directory).Any())
            {
                Directory.Delete(directory);
            }
        }
    }

    private void RemoveShortcut(string path)
    {
        if (File.Exists(path))
        {
            File.Delete(path);
            Log("Remove", path);
        }
        string? parent = Path.GetDirectoryName(path);
        if (!string.IsNullOrWhiteSpace(parent)
            && Directory.Exists(parent)
            && !Directory.EnumerateFileSystemEntries(parent).Any()
            && string.Equals(Path.GetFileName(parent), ProductName, StringComparison.OrdinalIgnoreCase))
        {
            Directory.Delete(parent);
        }
    }

    private static void RunPowerShell(string command)
    {
        var startInfo = new ProcessStartInfo("powershell.exe")
        {
            Arguments = $"-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"& {{ {command} }}\"",
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };
        using Process process = Process.Start(startInfo) ?? throw new InvalidOperationException("PowerShell could not be started.");
        string output = process.StandardOutput.ReadToEnd();
        string error = process.StandardError.ReadToEnd();
        process.WaitForExit();
        if (process.ExitCode != 0)
        {
            throw new InvalidOperationException(string.IsNullOrWhiteSpace(error) ? output : error);
        }
    }

    private void ScheduleSelfRemoval()
    {
        if (_cleanupScheduled)
        {
            return;
        }
        _cleanupScheduled = true;
        string executable = Process.GetCurrentProcess().MainModule?.FileName ?? string.Empty;
        string escapedExecutable = executable.Replace("'", "''");
        string escapedRoot = _installRoot.Replace("'", "''");
        string command = $"Start-Sleep -Seconds 2; Remove-Item -LiteralPath '{escapedExecutable}' -Force -ErrorAction SilentlyContinue; Remove-Item -LiteralPath '{escapedRoot}' -ErrorAction SilentlyContinue";
        Process.Start(new ProcessStartInfo("powershell.exe")
        {
            Arguments = $"-NoProfile -NonInteractive -WindowStyle Hidden -Command \"& {{ {command} }}\"",
            UseShellExecute = false,
            CreateNoWindow = true,
            WindowStyle = ProcessWindowStyle.Hidden
        });
    }

    private void SetProgress(int value) => Dispatcher.Invoke(() => Progress.Value = value);

    private void Log(string action, string detail)
    {
        Dispatcher.Invoke(() =>
        {
            string line = $"{DateTime.Now:HH:mm:ss}  {action,-8}  {detail}";
            ActivityItems.Add(line);
            ActivityList.ScrollIntoView(line);
        });
    }

    private void ApplySystemTheme()
    {
        bool lightTheme = true;
        try
        {
            using RegistryKey? key = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize");
            if (key?.GetValue("AppsUseLightTheme") is int setting)
            {
                lightTheme = setting != 0;
            }
            else if (key?.GetValue("SystemUsesLightTheme") is int systemSetting)
            {
                lightTheme = systemSetting != 0;
            }
        }
        catch
        {
        }

        SetThemeBrush("WindowBackgroundBrush", lightTheme ? "#F6F6F6" : "#202020");
        SetThemeBrush("SurfaceBrush", lightTheme ? "#FFFFFF" : "#2B2B2B");
        SetThemeBrush("ControlBackgroundBrush", lightTheme ? "#F4F4F4" : "#323232");
        SetThemeBrush("ControlHoverBrush", lightTheme ? "#EAEAEA" : "#3A3A3A");
        SetThemeBrush("ControlPressedBrush", lightTheme ? "#DDDDDD" : "#454545");
        SetThemeBrush("BorderBrush", lightTheme ? "#DADADA" : "#4A4A4A");
        SetThemeBrush("PrimaryTextBrush", lightTheme ? "#1A1A1A" : "#F5F5F5");
        SetThemeBrush("SecondaryTextBrush", lightTheme ? "#5A5A5A" : "#C7C7C7");
        SetThemeBrush("SelectionBrush", lightTheme ? "#CCE8FF" : "#264F78");
    }

    private static void SetThemeBrush(string key, string color)
    {
        var brush = (SolidColorBrush)new BrushConverter().ConvertFromString(color);
        brush.Freeze();
        System.Windows.Application.Current.Resources[key] = brush;
    }

    private void SystemEvents_UserPreferenceChanged(object sender, UserPreferenceChangedEventArgs e)
        => Dispatcher.BeginInvoke(new Action(ApplySystemTheme));

    private void MainWindow_Closed(object? sender, EventArgs e)
        => SystemEvents.UserPreferenceChanged -= SystemEvents_UserPreferenceChanged;

    private void CancelButton_Click(object sender, RoutedEventArgs e)
    {
        if (_uninstallComplete)
        {
            ScheduleSelfRemoval();
        }
        Close();
    }

    private void MainWindow_Closing(object? sender, CancelEventArgs e)
    {
        if (_isWorking)
        {
            e.Cancel = true;
        }
        else if (_uninstallComplete)
        {
            ScheduleSelfRemoval();
        }
    }
}
