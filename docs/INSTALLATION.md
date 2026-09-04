# Installation

## Release installer

Distribute the signed `Persistent Calculator Setup.exe` together with its embedded/staged payload. The installer:

1. resolves the current user's configured Documents directory;
2. creates `Documents\Persistent Calculator\App`;
3. copies the signed package, public certificate, App Installer manifest, and dependencies using pending-file replacement;
4. verifies that the bundle signature matches the supplied `CN=Garries420` code-signing certificate;
5. trusts only that public certificate in the current user's `TrustedPeople` store;
6. registers dependencies and the MSIX bundle;
7. creates `Documents\Persistent Calculator\Persistent Calculator.exe` as a permanent launcher; and
8. offers to open the calculator, then closes the setup window.

The progress window reports each stage. Installation does not delete or overwrite `History.txt`, `Currency Metadata.json`, or `Currency Rates.json`.

## Launcher and packaged application

Persistent Calculator is a UWP/MSIX application inherited from Windows Calculator. Windows registers and stores the packaged application files in its protected package repository. This isolation enables clean registration, protocol activation, package identity, and App Installer updates.

The custom installer places a working desktop launcher named `Persistent Calculator.exe` directly in `Documents\Persistent Calculator`. The launcher opens the registered application through the `persistent-calculator:` protocol, so the user does not need to reopen Setup. The app also uses this folder for all requested user-owned persistent files.

## Development installation

Developer packages must be signed with a test certificate whose subject exactly matches `CN=Garries420`. Install the public test certificate for the current user, then register the generated `.msixbundle`. Never commit the private key or PFX.

## Uninstall and data retention

Removing the registered app package does not remove `Documents\Persistent Calculator`. This is intentional so history and cached data can survive a reinstall. A user can remove that folder manually if permanent deletion is desired.
