#pragma once

#include <windows.h>

#include "PluginInterface.h"

namespace CronNpp {

extern NppData g_nppData;
extern HANDLE g_hModule;

// Called once, from DllMain, to stash the module handle.
void pluginInit(HANDLE hModule);

// Called from beNotified() on NPPN_READY: allocates our command IDs and
// indicator ID, and applies indicator styling to both Scintilla views.
void pluginReady();

// Called from NPP's cleanup export.
void pluginCleanUp();

// Populates the plugin's "Plugins" submenu entries.
void commandMenuInit();
void commandMenuCleanUp();

FuncItem* getFuncsArray(int* nbF);

// Menu command callbacks.
void onToggleHighlighting();
void onRescanDocument();
void onAbout();

// Re-highlights (or clears, if disabled) the currently active view.
// Runs synchronously; prefer scheduleRescan() for edit/scroll-driven calls.
void rescanActiveView();

// Debounces rescanActiveView(): resets a short timer on each call so a
// burst of edits (fast typing) or scroll events only triggers one rescan
// after things settle, instead of one per keystroke/event.
void scheduleRescan();

// Forwarded from SCN_UPDATEUI when the view scrolled. Only large documents
// (see CronHighlighter::isLargeDocument) need this, since only they use
// visible-range-only scanning; small documents are already fully
// highlighted regardless of scroll position.
void onViewScrolled(HWND scintilla);

// Forwarded from SCN_DWELLSTART / SCN_DWELLEND.
void onDwellStart(HWND scintilla, long long position);
void onDwellEnd(HWND scintilla);

}  // namespace CronNpp
