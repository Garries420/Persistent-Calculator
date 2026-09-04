# Persistent Calculator architecture

Persistent Calculator keeps the upstream Windows Calculator split instead of collapsing the program into one very large executable.

## Runtime layers

1. `CalcManager` contains the native arbitrary-precision calculation engine and native history stores.
2. `CalcViewModel` exposes native C++/CX view models to the UI and handles unit/currency data loading.
3. `Calculator.ManagedViewModels` coordinates the active calculator/converter view model in C#.
4. `Calculator` contains the UWP XAML interface, persistence services, update service, and changelog.
5. `PersistentCalculator.Installer` is a separate WPF bootstrapper that stages files and registers the signed MSIX bundle.

Graphing projects and their DirectX renderer were removed. Enum value and serialization ID 17 remain reserved only so an old saved graphing selection is rejected safely.

## History flow

```text
calculator key, converter change, or Date calculation input
  -> native view model exposes the current expression/result and session ID
  -> managed shell updates one cohesive mode-tagged entry
  -> C or Escape closes that session; Date changes append distinct entries
  -> atomic pending-file replacement writes Documents\Persistent Calculator\History.txt

Microsoft Standard/Scientific history changed
  -> HistoryViewModel raises HistoryChanged
  -> the same file receives the latest native recall snapshot
```

The readable `entries` section drives the global history page, including all categories, mode badges, timestamps, and one mutable entry per active calculation. The compact `historyState` section preserves Microsoft's Standard/Scientific expression tokens and commands so compatible entries can still be recalled. Neither section restores an old active mode or unfinished launch state over a fresh launch.

On startup, `Windows Calculator Saved History.txt` in the Documents root is parsed in both the original two-line and later mode/timestamp formats, merged without duplicate calculations, and deleted only after the new document is written atomically. The obsolete Documents-root `Windows Calculator Currency Rates.json` is deleted without import.

## Currency flow

```text
Frankfurter /v2/currencies + /v2/rates?base=EUR
  -> bounded HTTPS responses
  -> native JSON validation and ratio map
  -> Currency Metadata.json + Currency Rates.json
  -> Microsoft unit-converter view model
```

The cache refresh timestamp, last automatic-attempt timestamp, and selected currency codes remain in packaged local settings. The response bodies themselves live in Documents as requested. A valid cache is loaded first. Production automatically refreshes it only after three hours have elapsed since both the cached data and the last attempt, and only on a CET/CEST weekday; a missing first-use cache can still be fetched, and the physical **Update rates** action deliberately bypasses the automatic schedule. Remembering failed attempts prevents an outage, mode change, or repeated launch from becoming a request loop. Unit tests use isolated mock responses and never write to the real Documents folder.

`build/scripts/TestFrankfurterContract.ps1` is an intentionally manual, two-request live preflight. It checks unique ISO codes, nonempty names, positive finite rates, and the metadata/rate intersection. It is not scheduled in CI so the test suite does not add background load to Frankfurter.

## Updates

The app reads the latest stable release metadata from GitHub's API. It ignores drafts/prereleases, compares semantic versions, and permits only HTTPS destinations under `/Garries420/Persistent-Calculator/` on `github.com`. An `.appinstaller` asset is preferred; a bundle or release page is the fallback.

The startup check visually overlays the mode title with a fully opaque header status pill without removing the title from the accessibility tree, and never blocks calculator input. Update, changelog, and global history use three separate fixed controls. A newer release may prompt the user, while an up-to-date or failed automatic check disappears automatically.
