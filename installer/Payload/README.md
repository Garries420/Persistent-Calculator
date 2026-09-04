# Installer payload

For a local Developer Mode test installer, packaging places the unpacked app here:

- `LoosePackage/AppxManifest.xml`
- the remaining files from the final unpacked x64 application package
- `Dependencies/` containing the required x64 framework `.appx` packages

For a production release, packaging instead places these generated, signed files here:

- `PersistentCalculator.msixbundle`
- `PersistentCalculator.cer`
- `PersistentCalculator.appinstaller`
- `Dependencies/` containing any required framework `.appx` packages

The installer copies either payload to `%USERPROFILE%\Documents\Persistent Calculator\App`, registers it, and creates `%USERPROFILE%\Documents\Persistent Calculator\Persistent Calculator.exe` as the permanent launcher. Local builds are registered as a Developer Mode loose package, while production releases register the signed bundle. It deliberately does not delete `History.txt`, `Currency Metadata.json`, or `Currency Rates.json` during upgrades.
