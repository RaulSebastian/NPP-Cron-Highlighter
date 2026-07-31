#pragma once

#include <windows.h>

namespace CronNpp {

// Documents at or above this size switch from full-document scanning
// (highlightDocument) to visible-range-only scanning (highlightVisibleRange)
// to stay responsive. See highlightVisibleRange() for the tradeoff.
constexpr long long kLargeDocumentThresholdBytes = 200LL * 1024;

// Must be called once, after the indicator ID has been allocated via
// NPPM_ALLOCATEINDICATOR, for each Scintilla view (main + secondary).
void initIndicatorStyle(HWND scintilla, int indicatorId);

// Removes all cron highlights from the given view.
void clearHighlights(HWND scintilla, int indicatorId);

// True if the document in `scintilla` is at/above
// kLargeDocumentThresholdBytes.
bool isLargeDocument(HWND scintilla);

// Re-scans the full document in the given view and re-applies highlights.
// Safe to call repeatedly (e.g. on every buffer activation or edit).
void highlightDocument(HWND scintilla, int indicatorId);

// Re-scans only the currently visible lines (plus a small margin above and
// below) and re-applies highlights over just that byte range; highlights
// outside the range are left untouched rather than cleared. Intended for
// documents too large for a full-document rescan on every keystroke —
// off-screen matches get (re)verified once they scroll into view, so
// highlighting converges to correct as the user scrolls, but a match that
// changed off-screen (e.g. via find/replace) won't update until scrolled
// to. Not appropriate as the default for typical/small files.
void highlightVisibleRange(HWND scintilla, int indicatorId);

}  // namespace CronNpp
