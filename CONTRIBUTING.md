# Contributing to Persistent Calculator

Persistent Calculator uses `Garries420/Persistent-Calculator` as its official repository. Publication and release changes require explicit owner permission.

## Development rules

- Keep the user-facing version at 1.0 until the owner explicitly changes it.
- Preserve the Microsoft Calculator C++/C# architecture and upstream MIT notices.
- Do not restore graphing mode unless a complete, legally distributable production engine is available and the owner approves the change.
- Use Frankfurter v2 for currency metadata and rates. Do not reintroduce Microsoft's unpublished retail currency service.
- Keep `History.txt`, `Currency Metadata.json`, and `Currency Rates.json` under the current user's `Documents\Persistent Calculator` folder.
- Never commit PFX files, private keys, access tokens, generated signed packages, personal history, or locally cached currency data.
- New update destinations must remain HTTPS and narrowly allow-listed.
- Do not delete persistent user data during an app upgrade or uninstall.

## Validation

Before requesting review:

1. run `git diff --check`;
2. parse changed XAML, project, manifest, and App Installer XML;
3. build the WPF installer in Release;
4. build the x64 Release UWP solution with the required Visual Studio workload;
5. run native unit tests;
6. verify graph projects and implementation references remain absent; and
7. describe any validation that could not be completed locally.

Changes should be reviewed and validated before publication. Stable publication and production signing are separate, owner-approved steps.
