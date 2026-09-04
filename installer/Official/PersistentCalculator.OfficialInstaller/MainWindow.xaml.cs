using Microsoft.Win32;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Security.Principal;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Navigation;
using System.Xml.Linq;
using Windows.ApplicationModel;
using Windows.Management.Deployment;
using Forms = System.Windows.Forms;

namespace PersistentCalculator.OfficialInstaller;

public partial class MainWindow : Window
{
    private const string ProductName = "Persistent Calculator";
    private const string ProductVersion = "1.0";
    private const string PackageName = "Garries420.PersistentCalculator";
    private const string PayloadResourceName = "PersistentCalculator.Payload.zip";
    private const string LauncherFileName = "Persistent Calculator.exe";
    private const string UninstallerFileName = "Uninstall Persistent Calculator.exe";
    private const string ManifestFileName = "installation-manifest.txt";
    private const string OptionsFileName = "installation-options.ini";
    private const string MarkerFileName = ".persistent-calculator-install";
    private const string UninstallKeyPath = @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\PersistentCalculator";
    private const string InstallerMutexName = @"Local\Garries420.PersistentCalculator.OfficialInstaller";
    private const string StagingFolderName = "PersistentCalculatorInstaller";
    private static readonly TimeSpan DependencyInstallTimeout = TimeSpan.FromMinutes(3);
    private static readonly Uri IssuesUri = new Uri("https://github.com/Garries420/Persistent-Calculator/issues");
    private readonly string _dataRoot = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), ProductName);
    private bool _installationInProgress;
    private bool _installationFailed;
    private string? _installedLauncherPath;
    private string? _failureDetails;
    private string _requestedInstallRoot = string.Empty;

    public ObservableCollection<ActivityEntry> ActivityItems { get; } = new ObservableCollection<ActivityEntry>();

    public MainWindow()
    {
        InitializeComponent();
        DataContext = this;
        Closing += MainWindow_Closing;
        Closed += MainWindow_Closed;
        SystemEvents.UserPreferenceChanged += SystemEvents_UserPreferenceChanged;
        ApplySystemTheme();
        InstallPathTextBox.Text = GetExistingInstallLocation()
            ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), ProductName);
    }

    private void BrowseButton_Click(object sender, RoutedEventArgs e)
    {
        using var dialog = new Forms.FolderBrowserDialog
        {
            Description = "Choose a dedicated folder for Persistent Calculator",
            ShowNewFolderButton = true,
            SelectedPath = Directory.Exists(InstallPathTextBox.Text)
                ? InstallPathTextBox.Text
                : Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles)
        };
        if (dialog.ShowDialog() == Forms.DialogResult.OK)
        {
            InstallPathTextBox.Text = dialog.SelectedPath;
        }
    }

    private async void InstallButton_Click(object sender, RoutedEventArgs e)
    {
        if (!TryValidateInstallRoot(InstallPathTextBox.Text, out string installRoot, out string validationError))
        {
            System.Windows.MessageBox.Show(validationError, "Persistent Calculator Installer", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }

        if (Directory.Exists(installRoot)
            && Directory.EnumerateFileSystemEntries(installRoot).Any()
            && !IsManagedInstallationFolder(installRoot))
        {
            System.Windows.MessageBox.Show(
                "The selected folder is not empty and is not an existing Persistent Calculator installation. Choose an empty folder or create a new folder for the application.",
                "Choose another installation folder",
                MessageBoxButton.OK,
                MessageBoxImage.Warning);
            return;
        }

        _installationInProgress = true;
        _installationFailed = false;
        _failureDetails = null;
        _requestedInstallRoot = installRoot;
        SetupPanel.Visibility = Visibility.Collapsed;
        SetupButtons.Visibility = Visibility.Collapsed;
        ProgressPanel.Visibility = Visibility.Visible;
        ActivityItems.Clear();

        bool desktopShortcut = DesktopShortcutCheckBox.IsChecked == true;
        bool startMenuShortcut = StartMenuShortcutCheckBox.IsChecked == true;

        try
        {
            await Task.Run(() => InstallCore(installRoot, desktopShortcut, startMenuShortcut));
            _installedLauncherPath = Path.Combine(installRoot, LauncherFileName);
            SetProgress(100, "Installation complete");
            Dispatcher.Invoke(() =>
            {
                ProgressHeading.Text = "Persistent Calculator is ready";
                ProgressStatus.Text = string.Empty;
                CompletionButtons.Visibility = Visibility.Visible;
                PrimaryActionButton.Content = "Open calculator";
                PrimaryActionButton.IsEnabled = true;
            });
        }
        catch (Exception ex)
        {
            _installationFailed = true;
            _failureDetails = ex.ToString();
            Log("Error", ex.Message, 100);
            Dispatcher.Invoke(() =>
            {
                ProgressHeading.Text = "Installation could not finish";
                ShowFailureInstructions();
                CompletionButtons.Visibility = Visibility.Visible;
                PrimaryActionButton.Content = "Download entries";
                PrimaryActionButton.IsEnabled = true;
            });
        }
        finally
        {
            _installationInProgress = false;
        }
    }

    private void InstallCore(string installRoot, bool desktopShortcut, bool startMenuShortcut)
    {
        EnsureAdministrator();
        using var installerMutex = new System.Threading.Mutex(false, InstallerMutexName);
        bool ownsMutex;
        try
        {
            ownsMutex = installerMutex.WaitOne(TimeSpan.Zero);
        }
        catch (System.Threading.AbandonedMutexException)
        {
            ownsMutex = true;
        }
        if (!ownsMutex)
        {
            throw new InvalidOperationException("Another Persistent Calculator installation is already running.");
        }

        try
        {
            string stagingParent = Path.Combine(Path.GetTempPath(), StagingFolderName);
            CleanupAbandonedStagingFolders(stagingParent);
            string stagingRoot = Path.Combine(stagingParent, Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(stagingRoot);
            Log("Create", stagingRoot, 0);

            try
            {
                ExtractPayload(stagingRoot);
                string? previousInstallRoot = GetExistingInstallLocation();
                string[] previousManagedFiles = string.IsNullOrWhiteSpace(previousInstallRoot)
                    ? Array.Empty<string>()
                    : ReadManagedFiles(previousInstallRoot!);
                string[] existingManagedFiles = ReadManagedFiles(installRoot);

                SetProgress(20, "Checking required Windows components");
                InstallDependencies(Path.Combine(stagingRoot, "Dependencies"));

                SetProgress(32, "Closing and unregistering the previous application");
                UnregisterExistingPackage();

                SetProgress(34, "Preparing application files");
                Directory.CreateDirectory(installRoot);
                string stagedApp = Path.Combine(stagingRoot, "App");
                string stagedTools = Path.Combine(stagingRoot, "Tools");
                if (!File.Exists(Path.Combine(stagedApp, "AppxManifest.xml")))
                {
                    throw new InvalidDataException("The embedded calculator application manifest is missing.");
                }

                var sourceFiles = new List<(string Source, string Relative)>();
                sourceFiles.AddRange(Directory.EnumerateFiles(stagedApp, "*", SearchOption.AllDirectories)
                    .Select(path => (path, path.Substring(stagedApp.Length).TrimStart(Path.DirectorySeparatorChar))));
                sourceFiles.Add((Path.Combine(stagedTools, LauncherFileName), LauncherFileName));
                sourceFiles.Add((Path.Combine(stagedTools, UninstallerFileName), UninstallerFileName));
                if (sourceFiles.Any(item => !File.Exists(item.Source)))
                {
                    throw new InvalidDataException("One or more embedded launcher or uninstaller files are missing.");
                }

                string[] managedFiles = sourceFiles.Select(item => item.Relative).OrderBy(path => path, StringComparer.OrdinalIgnoreCase).ToArray();
                RemoveObsoleteManagedFiles(
                    installRoot,
                    managedFiles.ToHashSet(StringComparer.OrdinalIgnoreCase),
                    existingManagedFiles);
                CopyApplicationFiles(installRoot, sourceFiles);

                SetProgress(87, "Registering Persistent Calculator with Windows");
                RegisterApplication(Path.Combine(installRoot, "AppxManifest.xml"));

                SetProgress(92, "Creating selected shortcuts");
                ConfigureShortcuts(installRoot, desktopShortcut, startMenuShortcut);

                SetProgress(96, "Registering the uninstaller");
                RegisterUninstaller(installRoot, managedFiles, desktopShortcut, startMenuShortcut);
                RemoveLegacyInstallationMetadata(installRoot);

                CleanupLegacyDevelopmentInstallation();

                if (!string.IsNullOrWhiteSpace(previousInstallRoot)
                    && !string.Equals(Path.GetFullPath(previousInstallRoot), installRoot, StringComparison.OrdinalIgnoreCase))
                {
                    CleanManagedInstallation(previousInstallRoot!, previousManagedFiles);
                }

                Log("Preserve", _dataRoot, 100);
                if (Directory.Exists(_dataRoot))
                {
                    Log("Data", "Existing history and currency data left unchanged", 100);
                }
                else
                {
                    Log("Data", "Documents data will be created by the calculator on first launch", 100);
                }
            }
            finally
            {
                if (TryDeleteDirectory(stagingRoot))
                {
                    Log("Clean", stagingRoot, 100);
                }
                else
                {
                    Log("Keep", $"Temporary working folder could not be removed: {stagingRoot}", 100);
                }
                TryDeleteEmptyDirectory(stagingParent);
            }
        }
        finally
        {
            installerMutex.ReleaseMutex();
        }
    }

    private void ExtractPayload(string stagingRoot)
    {
        SetProgress(2, "Opening the embedded installation package");
        using Stream payload = Assembly.GetExecutingAssembly().GetManifestResourceStream(PayloadResourceName)
            ?? throw new InvalidDataException("The official installer payload is missing.");
        string archivePath = Path.Combine(stagingRoot, "PersistentCalculator.Payload.zip");
        ActivityEntry payloadActivity = BeginActivity("Prepare", "Embedded offline payload");
        using (var destination = new FileStream(archivePath, FileMode.CreateNew, FileAccess.Write, FileShare.None))
        {
            CopyStreamWithProgress(payload, destination, payload.Length, payloadActivity);
        }
        CompleteActivity(payloadActivity);

        using (var archive = ZipFile.OpenRead(archivePath))
        {
            int index = 0;
            foreach (ZipArchiveEntry entry in archive.Entries)
            {
                index++;
                string destinationPath = GetSafeStagingPath(stagingRoot, entry.FullName);
                if (string.IsNullOrEmpty(entry.Name))
                {
                    Directory.CreateDirectory(destinationPath);
                    continue;
                }
                Directory.CreateDirectory(Path.GetDirectoryName(destinationPath));
                ActivityEntry extractActivity = BeginActivity("Extract", entry.FullName.Replace('/', '\\'));
                try
                {
                    using Stream source = entry.Open();
                    using var destination = new FileStream(destinationPath, FileMode.Create, FileAccess.Write, FileShare.None);
                    CopyStreamWithProgress(source, destination, entry.Length, extractActivity);
                }
                finally
                {
                    CompleteActivity(extractActivity);
                }
                SetProgress(2 + (int)(16.0 * index / Math.Max(1, archive.Entries.Count)), string.Empty);
            }
        }

        // ZipFile.OpenRead keeps the archive locked until it is disposed. Delete
        // the temporary copy only after leaving the using block above.
        File.Delete(archivePath);
    }

    private static string GetSafeStagingPath(string stagingRoot, string relativePath)
    {
        string normalizedRoot = Path.GetFullPath(stagingRoot).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        string destination = Path.GetFullPath(Path.Combine(stagingRoot, relativePath.Replace('/', Path.DirectorySeparatorChar)));
        if (!destination.StartsWith(normalizedRoot, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException($"Unsafe payload entry: {relativePath}");
        }
        return destination;
    }

    private void InstallDependencies(string dependencyRoot)
    {
        string[] dependencies = Directory.Exists(dependencyRoot)
            ? Directory.EnumerateFiles(dependencyRoot, "*.appx", SearchOption.AllDirectories).OrderBy(path => path).ToArray()
            : Array.Empty<string>();
        var packageManager = new PackageManager();
        IReadOnlyList<Package> installedPackages;
        try
        {
            installedPackages = packageManager.FindPackagesForUser(string.Empty).ToList();
        }
        catch (Exception ex)
        {
            Log("Check", $"Installed Windows components could not be enumerated; required packages will be verified by Windows. {ex.Message}");
            installedPackages = Array.Empty<Package>();
        }

        for (int index = 0; index < dependencies.Length; index++)
        {
            string dependency = dependencies[index];
            AppxIdentity requirement = ReadAppxIdentity(dependency);
            Package? installedPackage = FindCompatiblePackage(installedPackages, requirement);
            if (installedPackage != null)
            {
                Version installedVersion = ToVersion(installedPackage.Id.Version);
                string versionDescription = installedVersion > requirement.Version
                    ? $"A newer compatible {Path.GetFileName(dependency)} {installedVersion} is already installed"
                    : $"{Path.GetFileName(dependency)} {installedVersion} is already installed";
                Log("Keep", versionDescription);
                SetProgress(24 + (int)(7.0 * (index + 1) / Math.Max(1, dependencies.Length)), "Checking required Windows components");
                continue;
            }

            ActivityEntry installActivity = BeginActivity("Install", Path.GetFileName(dependency));
            try
            {
                AddAppxPackage(packageManager, dependency, installActivity);
            }
            catch (InvalidOperationException ex) when (ex.Message.IndexOf("0x80073D06", StringComparison.OrdinalIgnoreCase) >= 0)
            {
                Log("Keep", $"A newer compatible {Path.GetFileName(dependency)} is already installed");
            }
            finally
            {
                CompleteActivity(installActivity);
            }
            SetProgress(24 + (int)(7.0 * (index + 1) / Math.Max(1, dependencies.Length)), "Checking required Windows components");
        }
    }

    private static AppxIdentity ReadAppxIdentity(string packagePath)
    {
        using var archive = ZipFile.OpenRead(packagePath);
        ZipArchiveEntry manifestEntry = archive.Entries.FirstOrDefault(entry =>
            string.Equals(entry.FullName, "AppxManifest.xml", StringComparison.OrdinalIgnoreCase))
            ?? throw new InvalidDataException($"{Path.GetFileName(packagePath)} does not contain an AppxManifest.xml file.");
        using Stream manifestStream = manifestEntry.Open();
        XDocument manifest = XDocument.Load(manifestStream, LoadOptions.None);
        XElement identity = manifest.Descendants().FirstOrDefault(element => element.Name.LocalName == "Identity")
            ?? throw new InvalidDataException($"{Path.GetFileName(packagePath)} does not contain a package identity.");

        string name = identity.Attribute("Name")?.Value
            ?? throw new InvalidDataException($"{Path.GetFileName(packagePath)} does not declare a package name.");
        string publisher = identity.Attribute("Publisher")?.Value
            ?? throw new InvalidDataException($"{Path.GetFileName(packagePath)} does not declare a package publisher.");
        string architecture = identity.Attribute("ProcessorArchitecture")?.Value ?? "neutral";
        if (!Version.TryParse(identity.Attribute("Version")?.Value, out Version? version))
        {
            throw new InvalidDataException($"{Path.GetFileName(packagePath)} does not declare a valid package version.");
        }
        return new AppxIdentity(name, publisher, architecture, version);
    }

    private static Package? FindCompatiblePackage(
        IEnumerable<Package> installedPackages,
        AppxIdentity requirement)
    {
        return installedPackages
            .Where(package => string.Equals(package.Id.Name, requirement.Name, StringComparison.OrdinalIgnoreCase)
                && string.Equals(package.Id.Publisher, requirement.Publisher, StringComparison.OrdinalIgnoreCase)
                && IsCompatibleArchitecture(package.Id.Architecture.ToString(), requirement.Architecture)
                && package.Status.VerifyIsOK()
                && ToVersion(package.Id.Version) >= requirement.Version)
            .OrderByDescending(package => ToVersion(package.Id.Version))
            .FirstOrDefault();
    }

    private static bool IsCompatibleArchitecture(string installedArchitecture, string requiredArchitecture)
    {
        return string.Equals(installedArchitecture, requiredArchitecture, StringComparison.OrdinalIgnoreCase)
            || string.Equals(installedArchitecture, "Neutral", StringComparison.OrdinalIgnoreCase);
    }

    private static Version ToVersion(PackageVersion version)
    {
        return new Version(version.Major, version.Minor, version.Build, version.Revision);
    }

    private void RemoveObsoleteManagedFiles(string installRoot, HashSet<string> newFiles, IReadOnlyCollection<string> oldFiles)
    {
        string[] obsoleteFiles = oldFiles.Where(path => !newFiles.Contains(path)).ToArray();
        for (int index = 0; index < obsoleteFiles.Length; index++)
        {
            string relativePath = obsoleteFiles[index];
            string fullPath = GetManagedPath(installRoot, relativePath);
            if (File.Exists(fullPath))
            {
                ActivityEntry removeActivity = BeginActivity("Remove", fullPath);
                try
                {
                    File.Delete(fullPath);
                    UpdateActivity(removeActivity, 100);
                }
                finally
                {
                    CompleteActivity(removeActivity);
                }
            }
        }
        RemoveEmptySubdirectories(installRoot);
    }

    private void CopyApplicationFiles(string installRoot, List<(string Source, string Relative)> sourceFiles)
    {
        int index = 0;
        foreach ((string source, string relative) in sourceFiles)
        {
            index++;
            string destination = GetManagedPath(installRoot, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(destination));
            bool replacing = File.Exists(destination);
            string pending = destination + ".installing";
            ActivityEntry copyActivity = BeginActivity(replacing ? "Replace" : "Add", destination);
            try
            {
                using (var input = new FileStream(source, FileMode.Open, FileAccess.Read, FileShare.Read))
                using (var output = new FileStream(pending, FileMode.Create, FileAccess.Write, FileShare.None))
                {
                    CopyStreamWithProgress(input, output, input.Length, copyActivity);
                }
                if (File.Exists(destination))
                {
                    File.Delete(destination);
                }
                File.Move(pending, destination);
            }
            finally
            {
                CompleteActivity(copyActivity);
            }
            SetProgress(34 + (int)(51.0 * index / Math.Max(1, sourceFiles.Count)), "Copying application files");
        }
    }

    private void CopyStreamWithProgress(Stream source, Stream destination, long totalBytes, ActivityEntry activity)
    {
        var buffer = new byte[128 * 1024];
        long copied = 0;
        int lastPercent = -1;
        while (true)
        {
            int read = source.Read(buffer, 0, buffer.Length);
            if (read == 0)
            {
                break;
            }
            destination.Write(buffer, 0, read);
            copied += read;
            int percent = totalBytes <= 0 ? 100 : (int)Math.Min(100, copied * 100L / totalBytes);
            if (percent != lastPercent)
            {
                UpdateActivity(activity, percent);
                lastPercent = percent;
            }
        }
        UpdateActivity(activity, 100);
    }

    private void ConfigureShortcuts(string installRoot, bool desktopShortcut, bool startMenuShortcut)
    {
        string launcher = Path.Combine(installRoot, LauncherFileName);
        string desktopLink = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory), "Persistent Calculator.lnk");
        string startMenuFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs), "Persistent Calculator");
        string startMenuLink = Path.Combine(startMenuFolder, "Persistent Calculator.lnk");

        ConfigureShortcut(desktopLink, launcher, desktopShortcut, 50);
        ConfigureShortcut(startMenuLink, launcher, startMenuShortcut, 100);
    }

    private void ConfigureShortcut(string shortcutPath, string launcher, bool create, int phasePercent)
    {
        if (!create)
        {
            if (File.Exists(shortcutPath))
            {
                File.Delete(shortcutPath);
                Log("Remove", shortcutPath, phasePercent);
            }
            return;
        }

        Directory.CreateDirectory(Path.GetDirectoryName(shortcutPath));
        Type shellType = Type.GetTypeFromProgID("WScript.Shell")
            ?? throw new InvalidOperationException("Windows shortcut support is unavailable.");
        object shellObject = Activator.CreateInstance(shellType);
        object? shortcutObject = null;
        try
        {
            dynamic shell = shellObject;
            shortcutObject = shell.CreateShortcut(shortcutPath);
            dynamic shortcut = shortcutObject;
            shortcut.TargetPath = launcher;
            shortcut.WorkingDirectory = Path.GetDirectoryName(launcher);
            shortcut.IconLocation = $"{launcher},0";
            shortcut.Description = "Open Persistent Calculator";
            shortcut.Save();
        }
        finally
        {
            if (shortcutObject != null && Marshal.IsComObject(shortcutObject))
            {
                Marshal.FinalReleaseComObject(shortcutObject);
            }
            if (Marshal.IsComObject(shellObject))
            {
                Marshal.FinalReleaseComObject(shellObject);
            }
        }
        Log("Create", shortcutPath, phasePercent);
    }

    private void RegisterUninstaller(string installRoot, string[] managedFiles, bool desktopShortcut, bool startMenuShortcut)
    {
        string uninstaller = Path.Combine(installRoot, UninstallerFileName);
        using RegistryKey baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
        using RegistryKey key = baseKey.CreateSubKey(UninstallKeyPath, true)
            ?? throw new InvalidOperationException("The Windows uninstaller entry could not be created.");
        key.SetValue("DisplayName", ProductName);
        key.SetValue("DisplayVersion", ProductVersion);
        key.SetValue("Publisher", "Garries");
        key.SetValue("InstallLocation", installRoot);
        key.SetValue("DisplayIcon", $"{Path.Combine(installRoot, LauncherFileName)},0");
        key.SetValue("UninstallString", $"\"{uninstaller}\"");
        key.SetValue("NoModify", 1, RegistryValueKind.DWord);
        key.SetValue("NoRepair", 1, RegistryValueKind.DWord);
        key.SetValue("ManagedFiles", managedFiles, RegistryValueKind.MultiString);
        key.SetValue("DesktopShortcut", desktopShortcut ? 1 : 0, RegistryValueKind.DWord);
        key.SetValue("StartMenuShortcut", startMenuShortcut ? 1 : 0, RegistryValueKind.DWord);
        long bytes = Directory.EnumerateFiles(installRoot, "*", SearchOption.AllDirectories).Sum(path => new FileInfo(path).Length);
        key.SetValue("EstimatedSize", (int)Math.Min(int.MaxValue, bytes / 1024), RegistryValueKind.DWord);
        Log("Register", $"Programs and Features entry for {ProductName}", 100);
    }

    private void CleanupLegacyDevelopmentInstallation()
    {
        string legacyAppRoot = Path.Combine(_dataRoot, "App");
        string legacyManifest = Path.Combine(legacyAppRoot, "LoosePackage", "AppxManifest.xml");
        string legacyDependencies = Path.Combine(legacyAppRoot, "Dependencies");
        if (!File.Exists(legacyManifest) && !Directory.Exists(legacyDependencies))
        {
            return;
        }

        Log("Upgrade", "Removing application files left in Documents by the former development installer");
        try
        {
            if (Directory.Exists(legacyAppRoot))
            {
                foreach (string file in Directory.EnumerateFiles(legacyAppRoot, "*", SearchOption.AllDirectories))
                {
                    File.Delete(file);
                    Log("Remove", file);
                }
                foreach (string directory in Directory.EnumerateDirectories(legacyAppRoot, "*", SearchOption.AllDirectories)
                    .OrderByDescending(path => path.Length))
                {
                    if (!Directory.EnumerateFileSystemEntries(directory).Any())
                    {
                        Directory.Delete(directory);
                    }
                }
                Directory.Delete(legacyAppRoot);
                Log("Remove", legacyAppRoot);
            }

            foreach (string legacyFileName in new[] { LauncherFileName, LauncherFileName + ".config" })
            {
                string legacyFile = Path.Combine(_dataRoot, legacyFileName);
                if (File.Exists(legacyFile))
                {
                    File.Delete(legacyFile);
                    Log("Remove", legacyFile);
                }
            }
        }
        catch (Exception ex)
        {
            // Legacy cleanup must not turn an otherwise working official install
            // into a failure. The diagnostic log still identifies what remained.
            Log("Keep", $"Some former development files could not be removed: {ex.Message}");
        }
    }

    private void CleanManagedInstallation(string oldRoot, IReadOnlyCollection<string> managedFiles)
    {
        string fullRoot;
        try
        {
            fullRoot = Path.GetFullPath(oldRoot).TrimEnd(Path.DirectorySeparatorChar);
        }
        catch
        {
            return;
        }
        if (!Directory.Exists(fullRoot) || managedFiles.Count == 0)
        {
            return;
        }
        Log("Upgrade", $"Removing managed files from previous location {fullRoot}", 0);
        int index = 0;
        foreach (string relative in managedFiles)
        {
            index++;
            string file = GetManagedPath(fullRoot, relative);
            if (File.Exists(file))
            {
                File.Delete(file);
                Log("Remove", file, (int)Math.Round(100.0 * index / Math.Max(1, managedFiles.Count)));
            }
        }
        RemoveLegacyInstallationMetadata(fullRoot);
        RemoveEmptySubdirectories(fullRoot);
        if (!Directory.EnumerateFileSystemEntries(fullRoot).Any())
        {
            Directory.Delete(fullRoot);
            Log("Remove", fullRoot, 100);
        }
    }

    private static string[] ReadManagedFiles(string installRoot)
    {
        string normalizedRoot;
        try
        {
            normalizedRoot = Path.GetFullPath(installRoot).TrimEnd(Path.DirectorySeparatorChar);
        }
        catch
        {
            return Array.Empty<string>();
        }

        try
        {
            using RegistryKey baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
            using RegistryKey? key = baseKey.OpenSubKey(UninstallKeyPath);
            string? registeredRoot = key?.GetValue("InstallLocation") as string;
            if (!string.IsNullOrWhiteSpace(registeredRoot)
                && string.Equals(Path.GetFullPath(registeredRoot).TrimEnd(Path.DirectorySeparatorChar), normalizedRoot, StringComparison.OrdinalIgnoreCase)
                && key?.GetValue("ManagedFiles") is string[] registeredFiles)
            {
                return registeredFiles.Where(path => !string.IsNullOrWhiteSpace(path)).ToArray();
            }
        }
        catch
        {
        }

        string legacyManifest = Path.Combine(normalizedRoot, ManifestFileName);
        return File.Exists(legacyManifest)
            ? File.ReadAllLines(legacyManifest).Where(path => !string.IsNullOrWhiteSpace(path)).ToArray()
            : Array.Empty<string>();
    }

    private static bool IsManagedInstallationFolder(string installRoot)
    {
        if (ReadManagedFiles(installRoot).Length > 0 || File.Exists(Path.Combine(installRoot, MarkerFileName)))
        {
            return true;
        }

        string? registeredRoot = GetExistingInstallLocation();
        if (string.IsNullOrWhiteSpace(registeredRoot))
        {
            return false;
        }
        try
        {
            return string.Equals(
                Path.GetFullPath(registeredRoot).TrimEnd(Path.DirectorySeparatorChar),
                Path.GetFullPath(installRoot).TrimEnd(Path.DirectorySeparatorChar),
                StringComparison.OrdinalIgnoreCase);
        }
        catch
        {
            return false;
        }
    }

    private void RemoveLegacyInstallationMetadata(string installRoot)
    {
        foreach (string metadata in new[] { ManifestFileName, OptionsFileName, MarkerFileName })
        {
            string path = Path.Combine(installRoot, metadata);
            if (File.Exists(path))
            {
                File.Delete(path);
                Log("Remove", path, 100);
            }
        }
    }

    private static void RemoveEmptySubdirectories(string root)
    {
        if (!Directory.Exists(root))
        {
            return;
        }
        foreach (string directory in Directory.EnumerateDirectories(root, "*", SearchOption.AllDirectories).OrderByDescending(path => path.Length))
        {
            if (!Directory.EnumerateFileSystemEntries(directory).Any())
            {
                Directory.Delete(directory);
            }
        }
    }

    private static string GetManagedPath(string installRoot, string relativePath)
    {
        string normalizedRoot = Path.GetFullPath(installRoot).TrimEnd(Path.DirectorySeparatorChar);
        string fullPath = Path.GetFullPath(Path.Combine(normalizedRoot, relativePath));
        if (!fullPath.StartsWith(normalizedRoot + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException($"Unsafe managed file path: {relativePath}");
        }
        return fullPath;
    }

    private void UnregisterExistingPackage()
    {
        ActivityEntry activity = BeginActivity("Remove", "Previous Windows app registration, if present");
        try
        {
            RunPowerShell($"$package = Get-AppxPackage -Name '{PackageName}'; if ($null -ne $package) {{ $package | Remove-AppxPackage -ErrorAction Stop }}");
            UpdateActivity(activity, 100);
        }
        finally
        {
            CompleteActivity(activity);
        }
    }

    private void RegisterApplication(string manifestPath)
    {
        string escaped = manifestPath.Replace("'", "''");
        ActivityEntry activity = BeginActivity("Register", manifestPath);
        try
        {
            RunPowerShell($"Add-AppxPackage -Register '{escaped}' -ForceApplicationShutdown -ForceUpdateFromAnyVersion -ErrorAction Stop");
            UpdateActivity(activity, 100);
        }
        finally
        {
            CompleteActivity(activity);
        }
    }

    private void AddAppxPackage(PackageManager manager, string packagePath, ActivityEntry activity)
    {
        var operation = manager.AddPackageAsync(
            new Uri(packagePath),
            null,
            DeploymentOptions.ForceApplicationShutdown);
        operation.Progress = (sender, progress) => UpdateActivity(activity, (int)progress.percentage);
        try
        {
            Task<DeploymentResult> deploymentTask = operation.AsTask();
            Task completedTask = Task.WhenAny(
                deploymentTask,
                Task.Delay(DependencyInstallTimeout)).GetAwaiter().GetResult();
            if (!ReferenceEquals(completedTask, deploymentTask))
            {
                operation.Cancel();
                throw new TimeoutException(
                    $"Windows did not finish installing {Path.GetFileName(packagePath)} within {DependencyInstallTimeout.TotalMinutes:N0} minutes.");
            }

            deploymentTask.GetAwaiter().GetResult();
            UpdateActivity(activity, 100);
        }
        catch (TimeoutException)
        {
            throw;
        }
        catch (Exception ex)
        {
            int errorCode = Marshal.GetHRForException(ex);
            throw new InvalidOperationException($"Windows could not install {Path.GetFileName(packagePath)} (0x{errorCode:X8}). {ex.Message}", ex);
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
            string detail = string.IsNullOrWhiteSpace(error) ? output : error;
            throw new InvalidOperationException(string.IsNullOrWhiteSpace(detail)
                ? $"A Windows installation command failed with exit code {process.ExitCode}."
                : detail.Trim());
        }
    }

    private static void EnsureAdministrator()
    {
        using WindowsIdentity identity = WindowsIdentity.GetCurrent();
        var principal = new WindowsPrincipal(identity);
        if (!principal.IsInRole(WindowsBuiltInRole.Administrator))
        {
            throw new UnauthorizedAccessException("Administrator approval is required to install into Program Files.");
        }
    }

    private static string? GetExistingInstallLocation()
    {
        try
        {
            using RegistryKey baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
            using RegistryKey? key = baseKey.OpenSubKey(UninstallKeyPath);
            return key?.GetValue("InstallLocation") as string;
        }
        catch
        {
            return null;
        }
    }

    private bool TryValidateInstallRoot(string input, out string installRoot, out string error)
    {
        installRoot = string.Empty;
        error = string.Empty;
        if (string.IsNullOrWhiteSpace(input))
        {
            error = "Choose an installation folder.";
            return false;
        }
        try
        {
            installRoot = Path.GetFullPath(Environment.ExpandEnvironmentVariables(input.Trim())).TrimEnd(Path.DirectorySeparatorChar);
        }
        catch (Exception ex)
        {
            error = $"The installation path is not valid. {ex.Message}";
            return false;
        }
        if (!Path.IsPathRooted(installRoot) || string.Equals(installRoot, Path.GetPathRoot(installRoot)?.TrimEnd(Path.DirectorySeparatorChar), StringComparison.OrdinalIgnoreCase))
        {
            error = "Choose a dedicated application folder, not the root of a drive.";
            return false;
        }

        string[] protectedRoots =
        {
            Environment.GetFolderPath(Environment.SpecialFolder.Windows),
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
            Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory),
            _dataRoot
        };
        string validatedRoot = installRoot;
        if (protectedRoots
            .Where(path => !string.IsNullOrWhiteSpace(path))
            .Any(path => string.Equals(validatedRoot, Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar), StringComparison.OrdinalIgnoreCase)))
        {
            error = "Choose a dedicated subfolder for Persistent Calculator. This location is too broad to manage safely.";
            return false;
        }
        string normalizedDataRoot = Path.GetFullPath(_dataRoot).TrimEnd(Path.DirectorySeparatorChar);
        if (validatedRoot.StartsWith(normalizedDataRoot + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
        {
            error = "Choose an application folder outside Documents\\Persistent Calculator so program files remain separate from history and currency data.";
            return false;
        }
        return true;
    }

    private void CleanupAbandonedStagingFolders(string stagingParent)
    {
        if (!Directory.Exists(stagingParent))
        {
            return;
        }

        string normalizedParent = Path.GetFullPath(stagingParent).TrimEnd(Path.DirectorySeparatorChar);
        foreach (string directory in Directory.EnumerateDirectories(normalizedParent))
        {
            string fullPath = Path.GetFullPath(directory);
            string folderName = Path.GetFileName(fullPath);
            if (!fullPath.StartsWith(normalizedParent + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)
                || !Guid.TryParseExact(folderName, "N", out _))
            {
                continue;
            }

            if (TryDeleteDirectory(fullPath))
            {
                Log("Clean", $"Abandoned files from an interrupted installer: {fullPath}");
            }
            else
            {
                Log("Keep", $"An abandoned installer folder is still in use and could not be removed: {fullPath}");
            }
        }
        TryDeleteEmptyDirectory(normalizedParent);
    }

    private static void TryDeleteEmptyDirectory(string path)
    {
        try
        {
            if (Directory.Exists(path) && !Directory.EnumerateFileSystemEntries(path).Any())
            {
                Directory.Delete(path, false);
            }
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    private static bool TryDeleteDirectory(string path)
    {
        for (int attempt = 0; attempt < 3; attempt++)
        {
            try
            {
                if (Directory.Exists(path))
                {
                    Directory.Delete(path, true);
                }
                return true;
            }
            catch (IOException)
            {
                if (attempt < 2)
                {
                    System.Threading.Thread.Sleep(150);
                }
            }
            catch (UnauthorizedAccessException)
            {
                if (attempt < 2)
                {
                    System.Threading.Thread.Sleep(150);
                }
            }
        }
        return !Directory.Exists(path);
    }

    private void SetProgress(int percent, string status)
    {
        Dispatcher.Invoke(() =>
        {
            int value = Math.Max(0, Math.Min(100, percent));
            OverallProgress.Value = value;
            ProgressPercent.Text = $"{value}%";
            ProgressStatus.Text = status;
        });
    }

    private void Log(string action, string detail, int ignoredPhasePercent = 100)
    {
        Dispatcher.Invoke(() =>
        {
            var entry = new ActivityEntry(DateTime.Now, action, detail);
            ActivityItems.Add(entry);
            ActivityList.ScrollIntoView(entry);
        });
    }

    private ActivityEntry BeginActivity(string action, string detail)
    {
        return Dispatcher.Invoke(() =>
        {
            var entry = new ActivityEntry(DateTime.Now, action, detail);
            entry.SetPercent(0);
            ActivityItems.Add(entry);
            ActivityList.ScrollIntoView(entry);
            return entry;
        });
    }

    private void UpdateActivity(ActivityEntry entry, int percent)
    {
        Dispatcher.Invoke(() => entry.SetPercent(percent));
    }

    private void CompleteActivity(ActivityEntry entry)
    {
        Dispatcher.Invoke(entry.ClearPercent);
    }

    private void ShowFailureInstructions()
    {
        ProgressStatus.Inlines.Clear();
        ProgressStatus.Inlines.Add(new Run("Review the final entries below, highlight parts and copy, or press the \"Download entries\" button below and send it as an issue on the "));
        var link = new Hyperlink(new Run("github page"))
        {
            NavigateUri = IssuesUri,
            ToolTip = IssuesUri.AbsoluteUri
        };
        link.RequestNavigate += IssuesLink_RequestNavigate;
        ProgressStatus.Inlines.Add(link);
        ProgressStatus.Inlines.Add(new Run("."));
    }

    private void IssuesLink_RequestNavigate(object sender, RequestNavigateEventArgs e)
    {
        Process.Start(new ProcessStartInfo(e.Uri.AbsoluteUri) { UseShellExecute = true });
        e.Handled = true;
    }

    private void PrimaryActionButton_Click(object sender, RoutedEventArgs e)
    {
        if (_installationFailed)
        {
            SaveDiagnosticEntries();
            return;
        }

        if (string.IsNullOrWhiteSpace(_installedLauncherPath) || !File.Exists(_installedLauncherPath))
        {
            return;
        }
        Process.Start(new ProcessStartInfo(_installedLauncherPath) { UseShellExecute = true });
        Close();
    }

    private void SaveDiagnosticEntries()
    {
        string downloads = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads");
        var dialog = new Microsoft.Win32.SaveFileDialog
        {
            Title = "Save Persistent Calculator installer entries",
            FileName = $"Persistent Calculator Installer Entries {DateTime.Now:yyyy-MM-dd HHmmss}.txt",
            DefaultExt = ".txt",
            Filter = "Text log (*.txt)|*.txt|All files (*.*)|*.*",
            InitialDirectory = Directory.Exists(downloads)
                ? downloads
                : Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
            AddExtension = true,
            OverwritePrompt = true
        };
        if (dialog.ShowDialog(this) != true)
        {
            return;
        }

        var report = new StringBuilder();
        report.AppendLine("Persistent Calculator Installer diagnostic entries");
        report.AppendLine($"Installer version: {ProductVersion}");
        report.AppendLine($"Result: {(_installationFailed ? "Installation failed" : "Installation completed")}");
        report.AppendLine($"Requested installation location: {_requestedInstallRoot}");
        report.AppendLine($"Windows: {Environment.OSVersion.VersionString}");
        report.AppendLine($"Process architecture: {(Environment.Is64BitProcess ? "64-bit" : "32-bit")}");
        report.AppendLine($"Saved: {DateTimeOffset.Now:O}");
        report.AppendLine();
        report.AppendLine("Entries:");
        foreach (ActivityEntry entry in ActivityItems)
        {
            report.AppendLine(entry.FullText);
        }
        if (!string.IsNullOrWhiteSpace(_failureDetails))
        {
            report.AppendLine();
            report.AppendLine("Exception details:");
            report.AppendLine(_failureDetails);
        }

        try
        {
            File.WriteAllText(dialog.FileName, report.ToString(), new UTF8Encoding(true));
        }
        catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException)
        {
            System.Windows.MessageBox.Show(
                $"The entries could not be saved there. Choose another location and try again.\n\n{ex.Message}",
                "Could not save entries",
                MessageBoxButton.OK,
                MessageBoxImage.Warning);
            return;
        }
        System.Windows.MessageBox.Show(
            $"The installer entries were saved to:\n{dialog.FileName}",
            "Entries saved",
            MessageBoxButton.OK,
            MessageBoxImage.Information);
    }

    private void ActivityList_KeyDown(object sender, System.Windows.Input.KeyEventArgs e)
    {
        if (e.Key == Key.C && (Keyboard.Modifiers & ModifierKeys.Control) == ModifierKeys.Control)
        {
            CopySelectedActivityEntries();
            e.Handled = true;
        }
    }

    private void ActivityList_PreviewMouseRightButtonDown(object sender, MouseButtonEventArgs e)
    {
        var source = e.OriginalSource as DependencyObject;
        var item = source == null ? null : ItemsControl.ContainerFromElement(ActivityList, source) as ListBoxItem;
        if (item != null && !item.IsSelected)
        {
            if ((Keyboard.Modifiers & ModifierKeys.Control) != ModifierKeys.Control)
            {
                ActivityList.SelectedItems.Clear();
            }
            item.IsSelected = true;
        }
    }

    private void CopyEntriesMenuItem_Click(object sender, RoutedEventArgs e) => CopySelectedActivityEntries();

    private void CopySelectedActivityEntries()
    {
        string[] selected = ActivityList.SelectedItems.Cast<object>()
            .OfType<ActivityEntry>()
            .Select(item => item.FullText)
            .Where(item => !string.IsNullOrEmpty(item))
            .ToArray();
        if (selected.Length == 0)
        {
            return;
        }

        try
        {
            System.Windows.Clipboard.SetText(string.Join(Environment.NewLine, selected));
        }
        catch (ExternalException ex)
        {
            System.Windows.MessageBox.Show(
                $"Windows could not copy the selected entries. {ex.Message}",
                "Copy entries",
                MessageBoxButton.OK,
                MessageBoxImage.Warning);
        }
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
    {
        Dispatcher.BeginInvoke(new Action(ApplySystemTheme));
    }

    private void MainWindow_Closed(object? sender, EventArgs e)
    {
        SystemEvents.UserPreferenceChanged -= SystemEvents_UserPreferenceChanged;
    }

    private void CancelButton_Click(object sender, RoutedEventArgs e) => Close();

    private void CloseButton_Click(object sender, RoutedEventArgs e) => Close();

    private void MainWindow_Closing(object? sender, CancelEventArgs e)
    {
        if (_installationInProgress)
        {
            e.Cancel = true;
        }
    }
}

internal sealed class AppxIdentity
{
    public AppxIdentity(string name, string publisher, string architecture, Version version)
    {
        Name = name;
        Publisher = publisher;
        Architecture = architecture;
        Version = version;
    }

    public string Name { get; }

    public string Publisher { get; }

    public string Architecture { get; }

    public Version Version { get; }
}

public sealed class ActivityEntry : INotifyPropertyChanged
{
    private string _percentText = string.Empty;

    public ActivityEntry(DateTime time, string action, string detail)
    {
        TimeText = time.ToString("HH:mm:ss");
        ActionText = action;
        DetailText = detail;
    }

    public string TimeText { get; }
    public string PercentText
    {
        get => _percentText;
        private set
        {
            if (_percentText == value)
            {
                return;
            }
            _percentText = value;
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(PercentText)));
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(FullText)));
        }
    }
    public string ActionText { get; }
    public string DetailText { get; }
    public string FullText => string.IsNullOrEmpty(PercentText)
        ? $"{TimeText}         {ActionText,-9}  {DetailText}"
        : $"{TimeText}  {PercentText}  {ActionText,-9}  {DetailText}";

    public event PropertyChangedEventHandler? PropertyChanged;

    public void SetPercent(int percent) => PercentText = $"{Math.Max(0, Math.Min(100, percent)),3}%";
    public void ClearPercent() => PercentText = string.Empty;
    public override string ToString() => FullText;
}
