# Security policy

## Supported versions

Only the latest public release receives security fixes.

## Reporting a vulnerability

Please use GitHub's **Report a vulnerability** / private security-advisory feature for this repository. Do not publish exploit details in a normal public issue before a fix is available.

Include:

- The affected version
- Clear reproduction steps
- The expected and actual behavior
- The security impact
- Any proof-of-concept files or screenshots that are safe to share

## Update security

The updater accepts only stable releases returned by GitHub's `releases/latest` API for `Garries420/Persistent-Calculator`. It asks for consent before downloading, permits only the exact `PersistentCalculator.exe` asset under this repository's GitHub Releases URL, and verifies GitHub's SHA-256 asset digest before launching the file.

The updater fails closed: a missing digest, malformed response, wrong domain/repository path, download error, or hash mismatch cancels installation.

## Current limitation

Releases are not Authenticode-signed because the project does not currently have a code-signing certificate. Users should download from this repository's Releases page and may compare the EXE with the published `.sha256` file.

The project is preparing an application for free open-source signing through SignPath Foundation. Current releases remain unsigned until a published executable carries a verifiable Authenticode signature. See the [Code signing policy](CODE_SIGNING.md).
