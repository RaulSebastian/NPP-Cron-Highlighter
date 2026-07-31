#include "PluginDefinition.h"

#include <cwchar>
#include <vector>

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

// Debounce: a burst of SCN_MODIFIED/scroll events resets this timer instead
// of rescanning immediately, so fast typing or scrolling only triggers one
// rescan after things settle.
constexpr UINT kDebounceMs = 200;
UINT_PTR g_debounceTimerId = 0;

// Buffers we've already shown the "large document" warning for, so it only
// appears once per document rather than on every activation/edit.
std::vector<UINT_PTR> g_warnedBuffers;

HWND activeScintilla() {
    int which = 0;
    ::SendMessage(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0,
                  reinterpret_cast<LPARAM>(&which));
    return which == 0 ? g_nppData._scintillaMainHandle : g_nppData._scintillaSecondHandle;
}

// Returns true (and records it) the first time this is called for the
// currently active buffer; false on every subsequent call for that buffer.
bool shouldWarnForCurrentBuffer() {
    const auto bufferId =
        static_cast<UINT_PTR>(::SendMessage(g_nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0));
    for (UINT_PTR id : g_warnedBuffers) {
        if (id == bufferId) {
            return false;
        }
    }
    g_warnedBuffers.push_back(bufferId);
    return true;
}

void warnIfLarge(HWND sci) {
    if (!shouldWarnForCurrentBuffer()) {
        return;
    }
    const auto lengthMb =
        static_cast<double>(::SendMessage(sci, SCI_GETLENGTH, 0, 0)) / (1024.0 * 1024.0);
    wchar_t msg[512];
    swprintf_s(msg,
                L"This document is large (%.1f MB), so CRON Highlighter is scanning only the "
                L"visible portion of it to stay responsive — off-screen matches get "
                L"highlighted as you scroll to them.\n\n"
                L"If you notice lag while editing, use Plugins > CRON Highlighter > "
                L"Toggle CRON Highlighting to turn it off for this file.",
                lengthMb);
    ::MessageBox(g_nppData._nppHandle, msg, TEXT("CRON Highlighter"), MB_OK | MB_ICONWARNING);
}

// Applies the enabled/disabled and large-vs-small-document dispatch to a
// single view. rescanActiveView() and onToggleHighlighting() both funnel
// through this so the two views (and the toggle command) can't drift out
// of sync with each other's scanning strategy.
void rescanView(HWND sci) {
    if (g_indicatorId < 0 || !sci) {
        return;
    }
    if (!g_enabled) {
        clearHighlights(sci, g_indicatorId);
        return;
    }
    if (isLargeDocument(sci)) {
        warnIfLarge(sci);
        highlightVisibleRange(sci, g_indicatorId);
    } else {
        highlightDocument(sci, g_indicatorId);
    }
}

void CALLBACK onDebounceTimer(HWND, UINT, UINT_PTR id, DWORD) {
    ::KillTimer(nullptr, id);
    g_debounceTimerId = 0;
    rescanActiveView();
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

void pluginCleanUp() {
    if (g_debounceTimerId != 0) {
        ::KillTimer(nullptr, g_debounceTimerId);
        g_debounceTimerId = 0;
    }
}

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

void rescanActiveView() { rescanView(activeScintilla()); }

void scheduleRescan() {
    if (g_debounceTimerId != 0) {
        ::KillTimer(nullptr, g_debounceTimerId);
    }
    g_debounceTimerId = ::SetTimer(nullptr, 0, kDebounceMs, onDebounceTimer);
}

void onViewScrolled(HWND scintilla) {
    if (!g_enabled || g_indicatorId < 0 || !scintilla) {
        return;
    }
    // Small documents are already fully highlighted regardless of scroll
    // position; only the visible-range strategy needs to react to scrolling.
    if (isLargeDocument(scintilla)) {
        scheduleRescan();
    }
}

void onToggleHighlighting() {
    g_enabled = !g_enabled;
    g_funcItems[0]._init2Check = g_enabled;
    ::SendMessage(g_nppData._nppHandle, NPPM_SETMENUITEMCHECK,
                  static_cast<WPARAM>(g_funcItems[0]._cmdID), g_enabled ? TRUE : FALSE);

    // Re-apply (or clear) highlighting in both views, not just the active one.
    rescanView(g_nppData._scintillaMainHandle);
    rescanView(g_nppData._scintillaSecondHandle);
}

void onRescanDocument() {
    if (g_debounceTimerId != 0) {
        ::KillTimer(nullptr, g_debounceTimerId);
        g_debounceTimerId = 0;
    }
    rescanActiveView();
}

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
