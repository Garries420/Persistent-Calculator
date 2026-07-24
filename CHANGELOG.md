# Changelog

## 2.0.0 — 2026-07-24

- Added Scientific and Programmer calculators and Date calculation.
- Added Currency, Volume, Length, Weight and mass, Temperature, Energy, Area, Speed, Time, Power, Data, Pressure, and Angle converters.
- Each converter now starts at zero and keeps its input separate from every other converter.
- Currency now shows locations, currency names and symbols, plus an automatic one-unit comparison and a link to Frankfurter's currency reference.
- Currency choices are alphabetized by location, and typing while the list is open jumps directly to matching countries, currency names, or codes.
- Scientific now uses Windows-style Trigonometry and Function menus with working extended functions.
- Programmer now uses clickable base rows, Bitwise and Bit shift menus, selectable word sizes, and a bit-toggling keypad.
- Programmer values and other mode results support gray drag-selection, right-click Copy and Paste, and `Ctrl+C` or `Ctrl+V`.
- Date calculation uses navigable month calendars for choosing exact dates.
- Added the supplied distinct navigation icons for every calculator and converter mode.
- Currency retrieves Frankfurter's daily blended reference rates over HTTPS and caches the raw JSON locally.
- Currency cache policy uses a three-hour quiet period, checks on every Currency opening from 15:00–18:00 Central European time, and accepts cached Friday rates over the weekend.
- Added dedicated History, Changelogs, and Check for updates header buttons with hover labels.
- Reworked the hamburger menu into a scrollable mode selector.
- Graphing remains intentionally excluded.

## 1.1.0 — 2026-07-21

- Large totals now use readable three-digit spacing, such as `5 000` and `5 000 000`.
- The in-app changelog retains up to the five latest releases with version tabs and vertical scrolling.
- Routine update-status notices now disappear after two seconds.
- Available updates ask for permission before downloading and can be declined until later.
- Accepted updates display private-safe download, verification, and installation progress.

## 1.0.1 — 2026-07-20

- Update notices are now attached to the calculator window and move with it.
- Update-status messages wrap cleanly so their complete text remains readable.
- Added an in-app changelog screen to the hamburger navigation menu.

## 1.0.0 — 2026-07-20

- Initial public release.
- Windows-inspired dark Standard calculator interface.
- Persistent local calculation history and full expression chains.
- History recall, scrolling, wiping, and automatic font fitting.
- Selectable/copyable result text.
- Window-placement persistence.
- Google-style percent-of calculations.
- Secure GitHub release updater with startup and manual checks.
- Privacy/security cleanup, tests, and public documentation.
