<p align="center">
  <img src="docs/assets/calculator-icon-165.png" width="165" height="165" alt="Persistent Calculator icon">
</p>

<h1 align="center">Persistent Calculator</h1>

<p align="center">
  <a href="https://github.com/Garries420/Persistent-Calculator/releases/tag/v1.1.0"><img src="https://img.shields.io/badge/release-v1.1.0-8250df" alt="release v1.1.0"></a>
  <img src="https://img.shields.io/badge/platform-Windows-2563eb" alt="platform Windows">
</p>

<p align="center">
  A compact, dark Windows Standard calculator with permanent local history and secure, user-approved updates.
</p>

<p align="center">
  <a href="https://github.com/Garries420/Persistent-Calculator/releases/latest"><strong>Download the latest release</strong></a>
</p>

## Screenshots

| Update status | Standard calculator |
|---|---|
| ![Persistent Calculator showing its attached update-status notice](docs/screenshots/update-status.png) | ![Persistent Calculator showing a grouped large result](docs/screenshots/calculator.png) |
| **Persistent history** | **Changelog** |
| ![Persistent Calculator history panel](docs/screenshots/history.png) | ![Persistent Calculator changelog panel](docs/screenshots/changelog.png) |

## Features

- Familiar Windows Calculator-inspired dark Standard layout.
- Google-style percentage calculations: `20 % 100 = 20` is preserved as `20% × 100 = 20`.
- Traditional percentage calculations remain supported, such as `50 + 10% = 55`.
- Results can be selected with a custom gray highlight, right-clicked, and copied.
- Permanent, human-readable calculation history stored locally as a text file.
- Clicking a history entry restores both its result and preserved calculation chain.
- Continuing a restored calculation updates the same working history session.
- Red **Wipe history** button with a bin icon.
- Remembers its last screen position, window size, and maximized state.
- Manual **Check for updates** control and current version display in the hamburger menu.
- Built-in scrollable **Changelog** screen retains up to the five latest releases.
- Portable single-executable release: no installer and no bundled personal history.

## Standard mode only

Persistent Calculator intentionally focuses on the normal **Standard calculator** experience. It currently does **not** include the extra modes and converters available in Microsoft Calculator, such as:

- Scientific, Graphing, and Programmer modes
- Date calculation
- Currency conversion
- Volume, Length, Weight and mass, and Temperature conversion
- Energy, Area, Speed, Time, Power, Data, Pressure, or Angle conversion

These additional modes may be added in future versions depending on user feedback and which features people actually request. Suggestions are welcome through the repository's [Issues page](https://github.com/Garries420/Persistent-Calculator/issues).

## Download and use

1. Download `PersistentCalculator.exe` from the [latest GitHub release](https://github.com/Garries420/Persistent-Calculator/releases/latest).
2. Place it anywhere your Windows account can write to, such as Downloads, Desktop, or a personal applications folder.
3. Run the EXE. No installation is required.

The project does not ship with anyone else's calculation history. Each Windows user gets a separate local text file when the calculator first runs.

> Windows SmartScreen may warn about a newly downloaded build because the project does not currently have a paid code-signing certificate. Release SHA-256 values are published beside every EXE, and the built-in updater verifies GitHub's asset digest before installing an update.

The project is preparing an application for free open-source signing. See the [Code signing policy](CODE_SIGNING.md) for its signing scope, maintainers, approval rules, and privacy disclosure.

## Where the history text file is created

The calculator creates:

```text
%USERPROFILE%\Documents\Windows Calculator Saved History.txt
```

More precisely, it uses Windows' configured **Documents** known folder, so systems that redirect Documents to OneDrive or another location will use that redirected folder. It creates a text file—not a new folder.

- The file contains calculation expressions and results only.
- It is readable in Notepad.
- **Wipe history** empties it.
- Deleting it manually is also safe; an empty file is created the next time the calculator starts.
- The file is never uploaded by the calculator or included in GitHub releases.

Window placement is stored separately under `HKEY_CURRENT_USER\Software\PersistentCalculator`. That registry value contains only window coordinates, size, and maximized state.

## Updates and privacy

On startup, the calculator performs one HTTPS `GET` request to the public `Garries420/Persistent-Calculator` latest-release endpoint. It sends no calculation history, clipboard contents, filenames, account information, or telemetry.

When a newer stable release exists, the calculator:

1. Shows the current and available versions and asks whether to update.
2. Downloads nothing unless **Yes, update** is selected. Choosing **No, not now** keeps the calculator open and asks again on the next startup or manual check.
3. Downloads only the exact `PersistentCalculator.exe` asset from this repository's GitHub Releases path.
4. Shows generic download, verification, and installation progress without exposing user paths.
5. Calculates the downloaded file's SHA-256 hash and compares it with GitHub's release-asset digest.
6. Cancels the update if any verification fails.
7. Replaces and restarts the calculator only after verification succeeds.

See [PRIVACY.md](PRIVACY.md) and [SECURITY.md](SECURITY.md) for more detail.

## Code signing policy

Free code signing provided by [SignPath.io](https://signpath.io/), certificate by [SignPath Foundation](https://signpath.org/).

The dedicated [Code signing policy](CODE_SIGNING.md) defines the permitted public build source, project roles, manual release approval, signature verification, and privacy requirements. Existing releases remain unsigned unless their executable carries a verifiable Authenticode signature.

## Useful keyboard controls

| Action | Keyboard |
|---|---|
| Digits and operators | `0–9`, `+`, `-`, `*`, `/`, `%` |
| Decimal separator | `.` or `,` |
| Calculate | `Enter` or `=` |
| Clear current entry | `Delete` |
| Clear calculation | `Escape` |
| Remove last digit | `Backspace` |
| Copy selected/full result | `Ctrl+C` |
| Paste a number | `Ctrl+V` |
| Open/close history | `Ctrl+H` |

## Building from source

The release build uses the Microsoft C compiler available with Visual Studio Build Tools:

```bat
scripts\build-release.cmd
```

The script runs the calculation-engine tests, updater-parser security tests, compiles the resources/icon, and produces `dist\PersistentCalculator.exe` with the static Microsoft C runtime.

Every push to the public `main` branch is rebuilt by GitHub Actions. A release is created only when the version in `VERSION` does not already have a matching release tag.

## License

Persistent Calculator is available under the [MIT License](LICENSE).
