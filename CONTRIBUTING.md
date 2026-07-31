# Contributing

Thanks for considering a contribution to NppCronHighlighter.

## Bug reports and feature requests

Open an issue using the appropriate template. For bugs, include:

- Notepad++ version and DLL bitness (32/64-bit)
- The cron expression (or file snippet) that triggers the issue
- What you expected vs. what happened

## Development setup

See the [Building](README.md#building) and [Testing](README.md#testing) sections
of the README for how to build the plugin and manually verify a change in
Notepad++.

## Making changes

1. Fork the repo and create a branch off `main`.
2. Keep changes focused — a bug fix shouldn't bundle in unrelated refactors.
3. Follow the existing code style (see `src/`); `CronDetector`, `CronDescriber`,
   and `CronNextRun` are pure/testable and have no Scintilla/Windows
   dependency — prefer keeping new logic there when possible over the
   Scintilla-facing files.
4. Manually test in Notepad++ per the README's Testing steps before opening a PR.
5. Open a pull request against `main` describing what changed and why.

CI (`.github/workflows/build-and-release.yml`) builds your PR on Windows
automatically; make sure it passes before requesting review.

## Reporting security issues

Please don't file a public issue for security vulnerabilities — see
[SECURITY.md](SECURITY.md) instead.
