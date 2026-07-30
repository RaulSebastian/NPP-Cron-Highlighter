#pragma once

#include <windows.h>

namespace CronNpp {

// Must be called once, after the indicator ID has been allocated via
// NPPM_ALLOCATEINDICATOR, for each Scintilla view (main + secondary).
void initIndicatorStyle(HWND scintilla, int indicatorId);

// Removes all cron highlights from the given view.
void clearHighlights(HWND scintilla, int indicatorId);

// Re-scans the full document in the given view and re-applies highlights.
// Safe to call repeatedly (e.g. on every buffer activation or edit).
void highlightDocument(HWND scintilla, int indicatorId);

}  // namespace CronNpp
