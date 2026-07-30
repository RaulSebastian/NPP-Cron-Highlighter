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
void rescanActiveView();

// Forwarded from SCN_DWELLSTART / SCN_DWELLEND.
void onDwellStart(HWND scintilla, long long position);
void onDwellEnd(HWND scintilla);

}  // namespace CronNpp
