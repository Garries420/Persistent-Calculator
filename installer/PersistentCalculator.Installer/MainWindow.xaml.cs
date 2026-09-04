using System.Diagnostics;
using System.IO;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Reflection;
using System.Text;
using System.Windows;

namespace PersistentCalculator.Installer;

public partial class MainWindow : Window
{
    private const string BundleFileName = "PersistentCalculator.msixbundle";
    private const string CertificateFileName = "PersistentCalculator.cer";
    private const string LoosePackageFolderName = "LoosePackage";
    private const string ManifestFileName = "AppxManifest.xml";
    private const string InstalledLauncherName = "Persistent Calculator.exe";
    private const string NewerPackageInstalledError = "0x80073D06";
    private readonly string _installRoot = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "Persistent Calculator Development");

    public MainWindow()
    {
        InitializeComponent();
        InstallPathText.Text = _installRoot;
        ShowPayloadReadiness();
    }

    private void ShowPayloadReadiness()
    {
        string payloadRoot = Path.Combine(AppContext.BaseDirectory, "Payload");
        string sourceBundle = Path.Combine(payloadRoot, BundleFileName);
        string sourceManifest = Path.Combine(payloadRoot, LoosePackageFolderName, ManifestFileName);

        if (File.Exists(sourceManifest))
        {
            StatusText.Text = "Local x64 test package ready";
            DetailText.Text = "Install will copy the complete development build to a Local AppData test folder and register it for your Windows account.";
            return;
        }

        if (File.Exists(sourceBundle))
        {
            StatusText.Text = "Signed release package ready";
            DetailText.Text = "Install will copy and register the signed Windows application package for your account.";
            return;
        }

        StatusText.Text = "Installer files are missing";
        DetailText.Text = "Keep Setup.exe beside its Payload folder, then reopen the installer.";
        InstallButton.IsEnabled = false;
    }

    private async void InstallButton_Click(object sender, RoutedEventArgs e)
    {
        HideErrorDetails();
        InstallButton.IsEnabled = false;
        CloseButton.IsEnabled = false;

        try
        {
            string payloadRoot = Path.Combine(AppContext.BaseDirectory, "Payload");
            string sourceBundle = Path.Combine(payloadRoot, BundleFileName);
            string sourceManifest = Path.Combine(payloadRoot, LoosePackageFolderName, ManifestFileName);
            bool hasSignedBundle = File.Exists(sourceBundle);
            bool hasLoosePackage = File.Exists(sourceManifest);
            if (!hasSignedBundle && !hasLoosePackage)
            {
                throw new FileNotFoundException(
                    "The installer payload is incomplete. Neither the local test package nor the signed release bundle was found next to Setup.",
                    sourceManifest);
            }

            Report(8, "Preparing folders", "Creating the Persistent Calculator Development folder in Local AppData…");
            string appFolder = Path.Combine(_installRoot, "App");
            Directory.CreateDirectory(appFolder);

            Report(20, "Copying application files", "Staging the local Windows application files…");
            await CopyPayloadAsync(payloadRoot, appFolder);

            if (hasSignedBundle)
            {
                string installedCertificate = Path.Combine(appFolder, CertificateFileName);
                if (File.Exists(installedCertificate))
                {
                    Report(45, "Trusting the publisher", "Adding the Persistent Calculator signing certificate for this user…");
                    ValidatePublisherCertificate(installedCertificate);
                    await VerifyPackageSignerAsync(Path.Combine(appFolder, BundleFileName), installedCertificate);
                    await RunProcessAsync(
                        "certutil.exe",
                        $"-user -addstore TrustedPeople \"{installedCertificate}\"");
                }
            }

            string dependenciesFolder = Path.Combine(appFolder, "Dependencies");
            if (Directory.Exists(dependenciesFolder))
            {
                Report(58, "Installing dependencies", "Registering required Windows framework packages…");
                foreach (string dependency in Directory.EnumerateFiles(dependenciesFolder, "*.appx", SearchOption.AllDirectories))
                {
                    await AddDependencyPackageAsync(dependency);
                }
            }

            if (hasLoosePackage)
            {
                Report(72, "Installing local test build", "Windows is registering the unpacked 1.0 development package…");
                await RegisterLoosePackageAsync(Path.Combine(appFolder, LoosePackageFolderName, ManifestFileName));
            }
            else
            {
                Report(72, "Installing Persistent Calculator", "Windows is registering the signed 1.0 app package…");
                await AddAppPackageAsync(Path.Combine(appFolder, BundleFileName));
            }

            Report(90, "Creating launcher", "Adding Persistent Calculator.exe to the main installation folder…");
            await InstallLauncherAsync();

            Report(
                100,
                "Installation complete",
                $"Persistent Calculator 1.0 is ready. You can reopen it later from {Path.Combine(_installRoot, InstalledLauncherName)}.");
            InstallButton.Content = "Open calculator";
            InstallButton.IsEnabled = true;
            InstallButton.Click -= InstallButton_Click;
            InstallButton.Click += OpenCalculatorButton_Click;
        }
        catch (Exception ex)
        {
            Report(0, "Installation could not finish", GetFriendlyErrorMessage(ex));
            ShowErrorDetails(ex.ToString());
            InstallButton.Content = "Try again";
            InstallButton.IsEnabled = true;
        }
        finally
        {
            CloseButton.IsEnabled = true;
        }
    }

    private static async Task CopyPayloadAsync(string sourceRoot, string destinationRoot)
    {
        foreach (string sourcePath in Directory.EnumerateFiles(sourceRoot, "*", SearchOption.AllDirectories))
        {
            string relativePath = sourcePath.Substring(sourceRoot.TrimEnd(Path.DirectorySeparatorChar).Length)
                .TrimStart(Path.DirectorySeparatorChar);
            string destinationPath = Path.Combine(destinationRoot, relativePath);
            Directory.CreateDirectory(Path.GetDirectoryName(destinationPath));

            string pendingPath = destinationPath + ".pending";
            using (var source = new FileStream(sourcePath, FileMode.Open, FileAccess.Read, FileShare.Read, 81920, true))
            using (var destination = new FileStream(pendingPath, FileMode.Create, FileAccess.Write, FileShare.None, 81920, true))
            {
                await source.CopyToAsync(destination);
            }

            if (File.Exists(destinationPath))
            {
                File.Replace(pendingPath, destinationPath, null);
            }
            else
            {
                File.Move(pendingPath, destinationPath);
            }
        }
    }

    private static Task AddAppPackageAsync(string packagePath)
    {
        string escapedPath = packagePath.Replace("'", "''");
        string command = $"Add-AppxPackage -Path '{escapedPath}' -ForceApplicationShutdown -ForceUpdateFromAnyVersion";
        return RunPowerShellAsync(command);
    }

    private static async Task AddDependencyPackageAsync(string packagePath)
    {
        try
        {
            await AddAppPackageAsync(packagePath);
        }
        catch (InvalidOperationException ex) when (
            ex.Message.IndexOf(NewerPackageInstalledError, StringComparison.OrdinalIgnoreCase) >= 0)
        {
            // A newer framework already installed by Windows or another app satisfies this dependency.
        }
    }

    private static Task RegisterLoosePackageAsync(string manifestPath)
    {
        string escapedPath = manifestPath.Replace("'", "''");
        string command = $"Add-AppxPackage -Register '{escapedPath}' -ForceApplicationShutdown -ForceUpdateFromAnyVersion";
        return RunPowerShellAsync(command);
    }

    private async Task InstallLauncherAsync()
    {
        string sourceExecutable = Assembly.GetEntryAssembly()?.Location
            ?? throw new FileNotFoundException("The setup executable could not be located while creating the launcher.");
        if (!File.Exists(sourceExecutable))
        {
            throw new FileNotFoundException("The setup executable could not be located while creating the launcher.");
        }

        string destinationExecutable = Path.Combine(_installRoot, InstalledLauncherName);
        await CopyFileAtomicallyAsync(sourceExecutable, destinationExecutable);

        string sourceConfig = sourceExecutable + ".config";
        if (File.Exists(sourceConfig))
        {
            await CopyFileAtomicallyAsync(sourceConfig, destinationExecutable + ".config");
        }
    }

    private static async Task CopyFileAtomicallyAsync(string sourcePath, string destinationPath)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(destinationPath));
        string pendingPath = destinationPath + ".pending";
        using (var source = new FileStream(sourcePath, FileMode.Open, FileAccess.Read, FileShare.Read, 81920, true))
        using (var destination = new FileStream(pendingPath, FileMode.Create, FileAccess.Write, FileShare.None, 81920, true))
        {
            await source.CopyToAsync(destination);
        }

        if (File.Exists(destinationPath))
        {
            File.Replace(pendingPath, destinationPath, null);
        }
        else
        {
            File.Move(pendingPath, destinationPath);
        }
    }

    private static Task VerifyPackageSignerAsync(string packagePath, string certificatePath)
    {
        string escapedPackage = packagePath.Replace("'", "''");
        string escapedCertificate = certificatePath.Replace("'", "''");
        string command =
            $"$certificate=[System.Security.Cryptography.X509Certificates.X509Certificate2]::new('{escapedCertificate}'); " +
            $"$signature=Get-AuthenticodeSignature -LiteralPath '{escapedPackage}'; " +
            "if ($null -eq $signature.SignerCertificate -or $signature.SignerCertificate.Thumbprint -ne $certificate.Thumbprint) " +
            "{ throw 'The app package signature does not match the supplied publisher certificate.' }";
        return RunPowerShellAsync(command);
    }

    private static Task RunPowerShellAsync(string command)
    {
        string guardedCommand =
            "$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'; " +
            "try { " + command + " } " +
            "catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }";
        string encoded = Convert.ToBase64String(Encoding.Unicode.GetBytes(guardedCommand));
        return RunProcessAsync(
            Path.Combine(Environment.SystemDirectory, "WindowsPowerShell", "v1.0", "powershell.exe"),
            $"-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand {encoded}");
    }

    private static void ValidatePublisherCertificate(string certificatePath)
    {
        using var certificate = new X509Certificate2(certificatePath);
        if (!certificate.Subject.Equals("CN=Garries420", StringComparison.Ordinal) || certificate.HasPrivateKey)
        {
            throw new InvalidOperationException("The installer publisher certificate is invalid.");
        }

        bool hasCodeSigningUsage = certificate.Extensions
            .OfType<X509EnhancedKeyUsageExtension>()
            .SelectMany(extension => extension.EnhancedKeyUsages.Cast<Oid>())
            .Any(oid => oid.Value == "1.3.6.1.5.5.7.3.3");
        if (!hasCodeSigningUsage || DateTime.UtcNow < certificate.NotBefore.ToUniversalTime() || DateTime.UtcNow > certificate.NotAfter.ToUniversalTime())
        {
            throw new InvalidOperationException("The installer publisher certificate is not valid for code signing.");
        }
    }

    private static async Task RunProcessAsync(string fileName, string arguments)
    {
        var startInfo = new ProcessStartInfo(fileName, arguments)
        {
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true
        };

        using var process = Process.Start(startInfo) ?? throw new InvalidOperationException($"Could not start {fileName}.");
        Task<string> outputTask = process.StandardOutput.ReadToEndAsync();
        Task<string> errorTask = process.StandardError.ReadToEndAsync();
        await Task.Run(() => process.WaitForExit());
        string output = await outputTask;
        string error = await errorTask;

        if (process.ExitCode != 0)
        {
            string detail = string.IsNullOrWhiteSpace(error) ? output : error;
            throw new InvalidOperationException(string.IsNullOrWhiteSpace(detail)
                ? $"{Path.GetFileName(fileName)} exited with code {process.ExitCode}."
                : detail.Trim());
        }
    }

    private void OpenCalculatorButton_Click(object sender, RoutedEventArgs e)
    {
        string launcherPath = Path.Combine(_installRoot, InstalledLauncherName);
        Process.Start(new ProcessStartInfo(launcherPath) { UseShellExecute = true });
        Close();
    }

    private void CloseButton_Click(object sender, RoutedEventArgs e) => Close();

    private void CopyDetailsButton_Click(object sender, RoutedEventArgs e)
    {
        if (!string.IsNullOrWhiteSpace(ErrorDetailsTextBox.Text))
        {
            Clipboard.SetText(ErrorDetailsTextBox.Text);
            CopyDetailsButton.Content = "Copied";
        }
    }

    private void HideErrorDetails()
    {
        ErrorDetailsExpander.Visibility = Visibility.Collapsed;
        ErrorDetailsExpander.IsExpanded = false;
        ErrorDetailsTextBox.Text = string.Empty;
        CopyDetailsButton.Content = "Copy details";
    }

    private void ShowErrorDetails(string details)
    {
        ErrorDetailsTextBox.Text = details;
        ErrorDetailsExpander.Visibility = Visibility.Visible;
        ErrorDetailsExpander.IsExpanded = false;
        CopyDetailsButton.Content = "Copy details";
    }

    private static string GetFriendlyErrorMessage(Exception exception)
    {
        string detail = exception.ToString();
        if (detail.IndexOf(NewerPackageInstalledError, StringComparison.OrdinalIgnoreCase) >= 0)
        {
            return "Windows already has a newer compatible framework. Close this installer, reopen the corrected copy, and try again.";
        }

        if (detail.IndexOf("0x80073CF3", StringComparison.OrdinalIgnoreCase) >= 0)
        {
            return "A required Windows framework could not be installed. Expand Technical details below for the exact package name.";
        }

        if (exception is FileNotFoundException)
        {
            return "Part of the local installer payload is missing. Keep Setup.exe beside its Payload folder and reopen it.";
        }

        if (exception is UnauthorizedAccessException)
        {
            return "Windows denied access to one of the installation folders. Close the calculator and try the installation again.";
        }

        return "Windows could not finish registering the local test build. Expand Technical details below for the complete error.";
    }

    private void Report(double progress, string status, string detail)
    {
        InstallProgress.Value = progress;
        StatusText.Text = status;
        DetailText.Text = detail;
    }
}
