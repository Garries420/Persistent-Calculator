# Privacy

Persistent Calculator is designed to work without collecting personal information.

## Local data

The calculator stores two kinds of local data:

1. Calculation history in the configured Windows Documents folder as `Windows Calculator Saved History.txt`.
2. Window position, size, and maximized state in `HKEY_CURRENT_USER\Software\PersistentCalculator`.

The history file contains expressions and results only. The registry value contains window-placement numbers only.

## Network activity

The only built-in network activity is the update checker. It makes an HTTPS `GET` request to:

```text
https://api.github.com/repos/Garries420/Persistent-Calculator/releases/latest
```

If a newer version exists, the calculator asks before downloading anything. After the user accepts, it downloads the exact release EXE from this repository's GitHub Releases path and displays only generic progress stages and percentages. The calculator does not perform telemetry, analytics, advertising, account login, history synchronization, or background uploads.

The update checker does not read or send:

- Calculation history
- Clipboard contents
- Documents or other personal files
- Windows username or account details
- Email addresses
- Location data
- Device identifiers

GitHub and the network provider may process ordinary connection information such as an IP address as part of serving the HTTPS request. Refer to GitHub's own privacy documentation for its handling of that connection data.

## Clipboard

Clipboard access occurs only when the user explicitly copies a result or pastes a number.

## Removing local data

- Use **Wipe history** or delete the history text file.
- Delete `HKEY_CURRENT_USER\Software\PersistentCalculator` to remove saved window placement.
- Delete the EXE to remove the application itself.
