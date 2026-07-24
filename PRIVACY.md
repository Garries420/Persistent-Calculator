# Privacy

Persistent Calculator is designed to work without collecting personal information.

## Local data

The calculator stores three kinds of local data:

1. Calculation history in the configured Windows Documents folder as `Windows Calculator Saved History.txt`.
2. A currency-rate cache in the same Documents folder as `Windows Calculator Currency Rates.json`.
3. Window position, size, and maximized state in `HKEY_CURRENT_USER\Software\PersistentCalculator`.

The history file contains expressions and results only. The currency file contains the unmodified public Frankfurter JSON response. The registry value contains window-placement numbers only.

## Network activity

The built-in update checker makes an HTTPS `GET` request to:

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

Opening Currency may also make an HTTPS `GET` request to:

```text
https://api.frankfurter.dev/v2/rates?base=EUR
```

That request contains no calculation input, history, filenames, username, device identifier, or location. It retrieves public daily reference-rate data. The app normally reuses a saved response for at least three hours, checks on each Currency opening from 15:00–18:00 Central European time, and reuses Friday rates over the weekend. Frankfurter and its network provider may receive ordinary connection information such as an IP address.

## Clipboard

Clipboard access occurs only when the user explicitly copies or pastes a value.

## Removing local data

- Use **Wipe history** or delete the history text file.
- Delete `Windows Calculator Currency Rates.json` to remove the currency cache. It will be recreated the next time Currency successfully retrieves rates.
- Delete `HKEY_CURRENT_USER\Software\PersistentCalculator` to remove saved window placement.
- Delete the EXE to remove the application itself.
