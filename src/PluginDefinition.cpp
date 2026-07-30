#include "PluginDefinition.h"

#include "CronHighlighter.h"
#include "CronTooltip.h"

namespace CronNpp {

NppData g_nppData{};
HANDLE g_hModule = nullptr;

namespace {
bool g_enabled = true;
int g_indicatorId = -1;  // resolved via NPPM_ALLOCATEINDICATOR on NPPN_READY

constexpr int kFuncCount = 3;
FuncItem g_funcItems[kFuncCount];

HWND activeScintilla() {
    int which = 0;
    ::SendMessage(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0,
                  reinterpret_cast<LPARAM>(&which));
    return which == 0 ? g_nppData._scintillaMainHandle : g_nppData._scintillaSecondHandle;
}
}  // namespace

void pluginInit(HANDLE hModule) { g_hModule = hModule; }

void pluginReady() {
    int startId = -1;
    if (::SendMessage(g_nppData._nppHandle, NPPM_ALLOCATEINDICATOR, 1,
                       reinterpret_cast<LPARAM>(&startId))) {
        g_indicatorId = startId;
    }

    if (g_indicatorId < 0) {
        return;  // allocation failed; leave highlighting disabled rather than guess an ID
    }

    initIndicatorStyle(g_nppData._scintillaMainHandle, g_indicatorId);
    initIndicatorStyle(g_nppData._scintillaSecondHandle, g_indicatorId);
    enableDwell(g_nppData._scintillaMainHandle);
    enableDwell(g_nppData._scintillaSecondHandle);
    rescanActiveView();
}

void pluginCleanUp() {}

void commandMenuInit() {
    lstrcpy(g_funcItems[0]._itemName, TEXT("Toggle CRON Highlighting"));
    g_funcItems[0]._pFunc = onToggleHighlighting;
    g_funcItems[0]._init2Check = g_enabled;

    lstrcpy(g_funcItems[1]._itemName, TEXT("Rescan Current Document"));
    g_funcItems[1]._pFunc = onRescanDocument;

    lstrcpy(g_funcItems[2]._itemName, TEXT("About..."));
    g_funcItems[2]._pFunc = onAbout;
}

void commandMenuCleanUp() {}

FuncItem* getFuncsArray(int* nbF) {
    *nbF = kFuncCount;
    return g_funcItems;
}

void rescanActiveView() {
    if (g_indicatorId < 0) {
        return;
    }
    HWND sci = activeScintilla();
    if (!sci) {
        return;
    }
    if (g_enabled) {
        highlightDocument(sci, g_indicatorId);
    } else {
        clearHighlights(sci, g_indicatorId);
    }
}

void onToggleHighlighting() {
    g_enabled = !g_enabled;
    g_funcItems[0]._init2Check = g_enabled;
    ::SendMessage(g_nppData._nppHandle, NPPM_SETMENUITEMCHECK,
                  static_cast<WPARAM>(g_funcItems[0]._cmdID), g_enabled ? TRUE : FALSE);

    // Re-apply (or clear) highlighting in both views, not just the active one.
    if (g_indicatorId >= 0) {
        if (g_enabled) {
            highlightDocument(g_nppData._scintillaMainHandle, g_indicatorId);
            highlightDocument(g_nppData._scintillaSecondHandle, g_indicatorId);
        } else {
            clearHighlights(g_nppData._scintillaMainHandle, g_indicatorId);
            clearHighlights(g_nppData._scintillaSecondHandle, g_indicatorId);
        }
    }
}

void onRescanDocument() { rescanActiveView(); }

void onDwellStart(HWND scintilla, long long position) {
    if (!g_enabled) {
        return;
    }
    showTooltipIfMatch(scintilla, position);
}

void onDwellEnd(HWND scintilla) { hideTooltip(scintilla); }

void onAbout() {
    ::MessageBox(g_nppData._nppHandle,
                 TEXT("NppCronHighlighter\n\n")
                 TEXT("Highlights CRON expressions in any open document.\n")
                 TEXT("Use \"Toggle CRON Highlighting\" in the Plugins menu to turn it off."),
                 TEXT("About NppCronHighlighter"), MB_OK | MB_ICONINFORMATION);
}

}  // namespace CronNpp
