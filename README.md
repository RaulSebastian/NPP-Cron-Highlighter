# NppCronHighlighter

A Notepad++ plugin that highlights CRON expressions (standard 5-field syntax,
6/7-field Quartz(.NET) cron with seconds and the `L`/`W`/`#` specials, and
`@daily`/`@hourly`/... shorthands) in any open text file, using a squiggly
indicator like a spell-checker.

## Supported syntax

- **Standard 5-field Unix cron** — `minute hour day-of-month month day-of-week`.
  ```
  0 5 * * *              # 05:00 every day
  */15 * * * *           # every 15 minutes
  30 4 1 * *              # 04:30 on the 1st of every month
  0 9-17 * * 1-5           # every hour, 9am-5pm, Mon-Fri
  ```
- **Quartz(.NET) 6/7-field cron** — `seconds minutes hours day-of-month month day-of-week [year]`.
  Day-of-month and day-of-week are mutually exclusive, per Quartz semantics:
  exactly one of the two must be `?`.
  ```
  0 0 12 * * ?             # 12:00:00 every day
  0 15 10 ? * MON-FRI       # 10:15:00, weekdays only
  0 0 0 1 1 ? *             # midnight on Jan 1, any year
  0 0 0 15 * ?              # midnight on the 15th of every month
  0 0 0 L * ?               # midnight on the last day of the month
  0 0 0 LW * ?              # midnight on the last weekday of the month
  0 0 0 15W * ?             # midnight on the weekday nearest the 15th
  0 0 0 ? * 6L               # midnight on the last Friday of the month
  0 0 0 ? * 6#3               # midnight on the 3rd Friday of the month
  0 0 0 * * ? 2026-2030       # midnight every day, only in 2026-2030
  ```
- **Shorthands** — `@yearly`, `@annually`, `@monthly`, `@weekly`, `@daily`,
  `@midnight`, `@hourly`, `@reboot`.

Hover over a highlighted expression to see a human-readable translation
(e.g. "At 10:15:00, on Monday through Friday").

## Layout

- `vendor/npp-sdk/` — official Notepad++/Scintilla plugin API headers
  (`PluginInterface.h`, `Notepad_plus_msgs.h`, `Scintilla.h`,
  `Sci_Position.h`), fetched verbatim from the
  [notepad-plus-plus](https://github.com/notepad-plus-plus/notepad-plus-plus)
  repo. These are the same headers every third-party NPP plugin builds
  against, and carry Notepad++'s own GPLv3 license header — keep it intact
  if you redistribute them.
- `src/PluginDefinition.*` — plugin lifecycle, menu commands, plugin state.
- `src/CronDetector.*` — pure regex-based cron detection over a text blob,
  no Windows/Scintilla dependency (easy to unit test standalone).
- `src/CronHighlighter.*` — talks to Scintilla to apply/clear the indicator.
- `src/dllmain.cpp` — DLL entry point and the exported NPP plugin ABI
  (`setInfo`, `getName`, `getFuncsArray`, `beNotified`, `messageProc`,
  `isUnicode`).

## Building

Requires a Windows machine with MSVC (Visual Studio Build Tools or full VS)
and CMake 3.20+.

```bash
cmake -B build -A x64
cmake --build build --config Release
```

This produces `build/Release/NppCronHighlighter.dll`. Use `-A Win32` instead
of `-A x64` if you're targeting a 32-bit Notepad++ install — the DLL's
bitness must match the host.

The MSVC runtime is statically linked (`CMAKE_MSVC_RUNTIME_LIBRARY`), so the
built DLL has no dependency on a Visual C++ Redistributable being installed
on the target VM — copy the DLL, nothing else.

## Installing

Find your Notepad++ plugins folder — usually one of:

- `C:\Program Files\Notepad++\plugins\` (per-machine install)
- `%LocalAppData%\Notepad++\plugins\` (per-user install)

If unsure, open Notepad++ and check **? menu > Debug Info** — it lists the
exact plugin path in use.

Plugins must live in their own subfolder matching the DLL name:

```
<plugins folder>\NppCronHighlighter\NppCronHighlighter.dll
```

Restart Notepad++ after copying the DLL in.

**If the DLL was copied from another machine/network share**, Windows may
mark it as blocked (Mark-of-the-Web), which makes Notepad++ silently skip
loading it. Right-click the DLL → Properties → check "Unblock" at the
bottom → OK. Or from PowerShell: `Unblock-File .\NppCronHighlighter.dll`.

## Testing

1. Launch Notepad++ and open the **Plugins** menu — you should see a
   **CRON Highlighter** submenu with "Toggle CRON Highlighting", "Rescan
   Current Document", and "About...". If it's missing, the DLL didn't load
   (check bitness match and the Mark-of-the-Web issue above).
2. Open a new plain text file and type a few lines:
   ```
   0 5 * * * /usr/bin/backup.sh
   */15 * * * * echo hi
   @daily /usr/bin/cleanup
   0 0 12 * * ?
   0 15 10 ? * MON-FRI
   0 15 10 ? * 6#3
   1 2 3 4 5
   just some random text
   ```
   The first five lines should get an orange squiggly underline (the 4th and
   5th are Quartz(.NET) 6-field cron; the 6th uses the `#` nth-weekday
   special); the last two should not (the 7th is a deliberate false-positive
   test — see `CronDetector.cpp`'s `isLikelyCron` heuristic).
3. Edit a line to add/remove a cron expression and confirm the highlight
   updates live.
4. Use **Plugins > CRON Highlighter > Toggle CRON Highlighting** to confirm
   highlights clear and reappear.
5. Split the view (main + secondary Scintilla) and confirm highlighting
   works in both.

## Installing / sharing across VMs

Copy the built DLL into Notepad++'s plugin folder:

```
%ProgramFiles%\Notepad++\plugins\NppCronHighlighter\NppCronHighlighter.dll
```

(Notepad++ expects each plugin in its own subfolder matching the DLL name.)
Since it's a single self-contained DLL with no external runtime
dependencies beyond the standard MSVC runtime, distributing it to other VMs
is just copying that one file+folder — no separate installer, no other
plugin required as a dependency (unlike a PythonScript-based approach).

## How it works

- On `NPPN_READY`, the plugin calls `NPPM_ALLOCATEINDICATOR` to get an
  indicator ID that Notepad++ guarantees won't collide with other plugins,
  then styles it as an orange squiggle on both Scintilla views.
- On `NPPN_BUFFERACTIVATED`, `NPPN_FILEOPENED`, and `SCN_MODIFIED` (insert/
  delete), it re-scans the active view's full text and re-applies the
  indicator over matched ranges.
- A "Toggle CRON Highlighting" entry is added to the Plugins menu.

## Known limitations / where the real complexity is

- **Rescans the whole document on every keystroke.** Fine for typical files;
  on very large files this should be debounced (e.g. a short timer, or only
  rescanning the visible/changed range) rather than done synchronously in
  `SCN_MODIFIED`.
- **False positives are inherent.** `1 2 3 4 5` is technically a valid cron
  expression. `CronDetector` requires at least one `* / , -` character to
  reduce false hits on plain numeric text, but that's a heuristic, not a
  guarantee — tune `isLikelyCron()` in `CronDetector.cpp` if it's too
  strict/loose for your files.
- **No numeric range validation.** Fields aren't checked against their real
  bounds (e.g. minutes 0-59, months 1-12), so `99 99 * * *` would still
  highlight. Adding bounds checking is straightforward but was left out to
  keep the regex readable.
- **No tooltip/human-readable translation** (e.g. "runs every day at
  05:00") on hover — would need a `SCN_DWELLSTART`/`SCN_DWELLEND` handler
  plus a small cron-to-English translator.
- **Single indicator style for all matches** — no visual distinction
  between a plain 5-field cron, a 6-field Quartz-style one, and a
  shorthand like `@daily`.
