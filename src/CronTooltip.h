#pragma once

#include <windows.h>

namespace CronNpp {

// Turns on mouse-dwell notifications for a Scintilla view; call once per
// view during setup.
void enableDwell(HWND scintilla);

// Called on SCN_DWELLSTART: if `position` falls inside a recognized cron
// expression on its line, shows a call tip with its human-readable
// description anchored at the start of the match. Otherwise hides any
// existing tooltip.
void showTooltipIfMatch(HWND scintilla, long long position);

// Called on SCN_DWELLEND.
void hideTooltip(HWND scintilla);

}  // namespace CronNpp
