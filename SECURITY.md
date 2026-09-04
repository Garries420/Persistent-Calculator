# Security policy

## Supported version

Persistent Calculator is currently maintained as version 1.0 in the private development repository. No development build should be treated as a stable release until it has passed CI, package-signature verification, and installation testing.

## Reporting a vulnerability

Do not publish credentials, signing material, personal history data, or a working exploit in a public issue. Report suspected vulnerabilities privately to the repository owner through GitHub's private vulnerability reporting or security-advisory feature when enabled.

Please include:

- the affected branch and commit;
- the affected source file and relevant line or function;
- reproduction steps and required configuration;
- expected and observed behavior;
- the likely impact; and
- a minimal proof of concept, if it can be shared safely.

Issues inherited unchanged from Microsoft's upstream source may also qualify for Microsoft's own security reporting process. Do not send Persistent Calculator-specific signing keys, user data, or private repository details to Microsoft.

## Security properties

- Stable update metadata is fetched over HTTPS from the allow-listed `Garries420/Persistent-Calculator` GitHub repository.
- The app opens only allow-listed GitHub release URLs returned by its update check.
- The installer verifies that the MSIX bundle signer matches the supplied `CN=Garries420` code-signing certificate before trusting that certificate for the current user.
- The release signing private key is supplied through protected release infrastructure and must never be stored in this repository or the Documents data folder.
- History and currency data are limited to `.txt` and `.json` access under the user's Documents library.
- Currency codes are restricted to three uppercase ASCII letters before being placed in a request URL.
