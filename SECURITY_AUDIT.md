# v1.0.0 privacy and security review

Review date: 2026-07-20

## Scope

- C source and tests
- Windows resources and icon assets
- Compiled PE executable strings/imports/resources
- Calculation-history persistence
- Clipboard handling
- Window-placement registry storage
- Startup/manual updater and self-replacement flow
- Public screenshots and repository contents

## Findings addressed before release

1. Removed the previous personal company metadata from the EXE.
2. Renamed the previous personal window-class identifier to `PersistentCalculatorMainWindow`.
3. Hardened Documents/history path construction against overlong-path buffer writes.
4. Kept history imports bounded to 8 MiB and fixed-size destination buffers.
5. Confirmed calculation history is not embedded in the EXE or repository.
6. Added strict parsing for release versions, asset name, repository download URL, and SHA-256 digest.
7. Required HTTPS and disallowed HTTPS-to-HTTP redirects in the updater.
8. Limited release metadata to 2 MiB and downloaded updates to 64 MiB.
9. Added SHA-256 verification before an update is executed.
10. Added hostile/malformed updater-response tests and strict compiler warnings.

## Data-flow conclusion

No application code uploads history or personal files. The updater performs read-only GitHub release requests and writes a verified temporary update EXE. Clipboard data is accessed only after a user copy/paste action.

## Expected public identifiers

The public GitHub coordinate `Garries420/Persistent-Calculator` is intentionally embedded because it is the fixed update source. No real personal name, email address, private path, history entry, credential, API key, or access token is embedded.

## Residual limitations

- History is intentionally human-readable plaintext in the user's Documents folder. Anyone already able to read that Windows account's Documents files can read it.
- The project is not Authenticode-signed, so Windows SmartScreen may show a reputation warning.
- As with any GitHub-hosted updater, compromise of the repository owner's GitHub account is a release-supply-chain risk. Strong GitHub authentication and protected account recovery are recommended.
- A process already running with the same Windows user's permissions can interfere with that user's local files and processes; the updater does not claim to defend against a fully compromised local account.

## Validation

- Calculation-engine tests
- Updater version and hostile JSON/digest/domain tests
- Cross-compilation with warnings treated as errors
- PE GUI-subsystem/import/resource inspection
- String scan for removed personal identifiers and unexpected secrets
- SHA-256 generation for the release binary
