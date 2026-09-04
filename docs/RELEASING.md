# Release process

No release action in this document authorizes a change to the stable `Garries420/Persistent-Calculator` repository. Stable publication requires explicit owner approval.

## Required artifacts

- `PersistentCalculator.msixbundle`, signed by the production `CN=Garries420` certificate.
- `PersistentCalculator.cer`, containing only the matching public certificate.
- `PersistentCalculator.appinstaller`, with matching identity and package version.
- `Persistent Calculator Setup.exe`, signed by the same trusted publisher.
- Required framework packages under `Dependencies/` when they cannot be acquired automatically.

## Versioning

The user-facing product version remains `1.0` until explicitly changed. MSIX requires four numeric parts, so releases use `1.0.0.0`; CI packages may increment only the fourth component (`1.0.0.N`) to remain installable over earlier CI builds without changing the displayed release line.

## Signing

Supply the production PFX and password through protected CI secrets. Do not echo secrets or certificate bytes to logs. Verify the package identity, signer thumbprint, and Authenticode status after signing and before assembling the installer payload.

## Validation gates

1. Restore and build `src/Calculator.slnx` for x64 Release with the UWP workload.
2. Run native unit tests, including currency and navigation tests.
3. Install the signed bundle on a clean Windows test user.
4. Confirm cohesive Standard, Scientific, Programmer, Date calculation, Currency, and unit-converter history survives restart, can be filtered by mode, and clears cleanly.
5. Place legacy `Windows Calculator Saved History.txt` and `Windows Calculator Currency Rates.json` files in Documents, launch once, and confirm the first is imported while both legacy files are removed.
6. Confirm Frankfurter metadata/rates are cached in Documents and an offline restart uses the cache.
7. Confirm the startup check remains non-blocking and the manual update action works.
8. Run `build/scripts/TestFrankfurterContract.ps1` once and confirm every selectable rate has matching metadata.
9. Confirm Graphing is absent from the menu and package payload, and that the Compact Overlay entry point is absent from the UI and automation tree.
10. Build and run the installer from a clean folder, then test upgrade and uninstall data retention.
11. Scan tracked project-owned files for local user paths, personal names, private email addresses, temporary certificate paths, tokens, and generated PFX files. Do not publish the private development branch history directly; create the approved public release as a clean/squashed snapshot so private-development metadata is not carried across.
