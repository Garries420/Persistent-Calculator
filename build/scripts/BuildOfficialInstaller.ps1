param(
    [string]$Configuration = 'Release',
    [string]$Platform = 'x64'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$outputRoot = Join-Path $repoRoot 'output-local'
$appOutput = Join-Path $outputRoot 'official-app-x64'
$stagingRoot = Join-Path $outputRoot 'official-installer-staging'
$installerBuild = Join-Path $outputRoot 'official-installer-build'
$finalOutput = Join-Path $outputRoot 'Persistent Calculator Installer'
$buildLog = Join-Path $outputRoot 'official-installer-app-build.binlog'
$payloadFolder = Join-Path $repoRoot 'installer\Official\Payload'
$payloadArchive = Join-Path $payloadFolder 'PersistentCalculator.Payload.zip'
$runtimeDirectives = Join-Path $repoRoot 'src\Calculator\Properties\Default.rd.xml'

function Assert-SafeBuildPath([string]$Path) {
    $fullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    $safeRoot = [System.IO.Path]::GetFullPath($outputRoot).TrimEnd('\') + '\'
    if (-not $fullPath.StartsWith($safeRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean a build path outside output-local: $fullPath"
    }
}

function Reset-BuildDirectory([string]$Path) {
    Assert-SafeBuildPath $Path
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Path | Out-Null
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
New-Item -ItemType Directory -Path $payloadFolder -Force | Out-Null

$runtimeDirectiveText = Get-Content -LiteralPath $runtimeDirectives -Raw
foreach ($requiredNamespace in @('CalculatorApp.Common', 'CalculatorApp.JsonUtils')) {
    $requiredDirective = 'Name="{0}" Serialize="All"' -f $requiredNamespace
    if ($runtimeDirectiveText -notmatch [regex]::Escape($requiredDirective)) {
        throw "Release serialization metadata is not preserved for $requiredNamespace. Refusing to build an installer that could write empty history JSON."
    }
}

Reset-BuildDirectory $appOutput
Reset-BuildDirectory $stagingRoot
Reset-BuildDirectory $installerBuild
Reset-BuildDirectory $finalOutput

$python = 'C:\Users\Tom\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
if (-not (Test-Path -LiteralPath $python)) {
    throw "The bundled Python runtime was not found at $python"
}
& $python (Join-Path $repoRoot 'build\scripts\GeneratePersistentCalculatorIcons.py')
if ($LASTEXITCODE -ne 0) {
    throw "Icon generation failed with exit code $LASTEXITCODE"
}

$msbuild = 'D:\Microsoft Visual Studio\2026\MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) {
    throw "Visual Studio MSBuild was not found at $msbuild"
}
& $msbuild `
    (Join-Path $repoRoot 'src\Calculator.slnx') `
    /m `
    /t:Build `
    "/p:Configuration=$Configuration" `
    "/p:Platform=$Platform" `
    /p:IsOfficialInstallerBuild=True `
    "/p:OutDir=$appOutput" `
    /p:GenerateProjectSpecificOutputFolder=true `
    "/bl:$buildLog" `
    /v:minimal
if ($LASTEXITCODE -ne 0) {
    throw "Persistent Calculator build failed with exit code $LASTEXITCODE"
}

& dotnet build (Join-Path $repoRoot 'installer\Official\PersistentCalculator.Launcher\PersistentCalculator.Launcher.csproj') -c Release
if ($LASTEXITCODE -ne 0) {
    throw "Launcher build failed with exit code $LASTEXITCODE"
}
& dotnet build (Join-Path $repoRoot 'installer\Official\PersistentCalculator.Uninstaller\PersistentCalculator.Uninstaller.csproj') -c Release
if ($LASTEXITCODE -ne 0) {
    throw "Uninstaller build failed with exit code $LASTEXITCODE"
}

$mainPackage = Join-Path $appOutput 'Calculator\Calculator_1.0.0.0_x64.msix'
$makeAppx = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\makeappx.exe'
$stagedApp = Join-Path $stagingRoot 'App'
New-Item -ItemType Directory -Path $stagedApp | Out-Null
& $makeAppx unpack /p $mainPackage /d $stagedApp /o | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Package extraction failed with exit code $LASTEXITCODE"
}

# Loose package registration only needs AppxManifest.xml and the runtime files.
# Block maps and package signatures are MSIX transport metadata, so do not ship
# them in the installed application directory.
foreach ($packageMetadataName in @('AppxBlockMap.xml', 'AppxSignature.p7x')) {
    $packageMetadataPath = Join-Path $stagedApp $packageMetadataName
    if (Test-Path -LiteralPath $packageMetadataPath) {
        Remove-Item -LiteralPath $packageMetadataPath -Force
    }
}

$dependencySource = Join-Path $appOutput 'Calculator\AppPackages\Calculator_1.0.0.0_Test\Dependencies\x64'
$dependencyDestination = Join-Path $stagingRoot 'Dependencies\x64'
New-Item -ItemType Directory -Path $dependencyDestination -Force | Out-Null
$dependencies = @(
    'Microsoft.NET.Native.Framework.2.2.appx',
    'Microsoft.NET.Native.Runtime.2.2.appx',
    'Microsoft.UI.Xaml.2.8.appx',
    'Microsoft.VCLibs.x64.14.00.appx',
    'Microsoft.VCLibs.x64.14.00.Desktop.appx'
)
foreach ($dependency in $dependencies) {
    Copy-Item -LiteralPath (Join-Path $dependencySource $dependency) -Destination $dependencyDestination
}

$toolsDestination = Join-Path $stagingRoot 'Tools'
New-Item -ItemType Directory -Path $toolsDestination | Out-Null
Copy-Item `
    -LiteralPath (Join-Path $repoRoot 'installer\Official\PersistentCalculator.Launcher\bin\Release\net48\Persistent Calculator.exe') `
    -Destination $toolsDestination
Copy-Item `
    -LiteralPath (Join-Path $repoRoot 'installer\Official\PersistentCalculator.Uninstaller\bin\Release\net48\Uninstall Persistent Calculator.exe') `
    -Destination $toolsDestination

if (Test-Path -LiteralPath $payloadArchive) {
    Remove-Item -LiteralPath $payloadArchive -Force
}
Compress-Archive -Path (Join-Path $stagingRoot '*') -DestinationPath $payloadArchive -CompressionLevel Optimal

& dotnet build `
    (Join-Path $repoRoot 'installer\Official\PersistentCalculator.OfficialInstaller\PersistentCalculator.OfficialInstaller.csproj') `
    -c Release `
    -o $installerBuild
if ($LASTEXITCODE -ne 0) {
    throw "Official installer build failed with exit code $LASTEXITCODE"
}

$installerExe = Join-Path $installerBuild 'Persistent Calculator Installer.exe'
Copy-Item -LiteralPath $installerExe -Destination $finalOutput

$payloadEntries = [System.IO.Compression.ZipFile]::OpenRead($payloadArchive)
try {
    $entryCount = $payloadEntries.Entries.Count
} finally {
    $payloadEntries.Dispose()
}

$finalInstaller = Join-Path $finalOutput 'Persistent Calculator Installer.exe'
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $finalInstaller).Hash

# Keep one stable delivery folder and discard reproducible intermediate output.
# The next local build recreates these directories instead of accumulating a
# new version-named folder each time.
foreach ($temporaryBuildPath in @($appOutput, $stagingRoot, $installerBuild)) {
    Assert-SafeBuildPath $temporaryBuildPath
    if (Test-Path -LiteralPath $temporaryBuildPath) {
        Remove-Item -LiteralPath $temporaryBuildPath -Recurse -Force
    }
}
if (Test-Path -LiteralPath $buildLog) {
    Remove-Item -LiteralPath $buildLog -Force
}

Write-Host "Official installer created: $finalInstaller"
Write-Host "Embedded payload entries: $entryCount"
Write-Host "Installer SHA256: $hash"
