# Privacy

Persistent Calculator performs two network operations:

- currency mode requests metadata and reference rates from `api.frankfurter.dev`; and
- update checks request the latest stable release metadata from GitHub's API.

The application does not upload calculation history or cached currency files to either service. History and currency responses are stored under the current user's `Documents\Persistent Calculator` folder.

Currency mode reads a valid local cache before considering the network. Automatic refresh is limited to one attempt per three-hour interval on weekdays, including when the preceding attempt failed, and is skipped on Saturdays and Sundays in Central European time. A missing first-use cache may be fetched so that Currency mode can function. The user-operated **Update rates** button is an explicit refresh request. Each refresh uses Frankfurter's currency metadata and current-rates endpoints; the application does not scrape website pages.

The inherited Microsoft Calculator source contains local Windows trace-logging instrumentation. Persistent Calculator development and release builds do not set Microsoft's `SEND_DIAGNOSTICS` build flag. Maintainers must not enable an upstream store/diagnostics build configuration without documenting the resulting data behavior and obtaining explicit project-owner approval.

Windows, GitHub, Frankfurter, network administrators, and operating-system services may process ordinary connection metadata according to their own policies. Users can keep previously cached currency data available offline, but live rate refresh and update checks require internet access.
