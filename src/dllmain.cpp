#include <windows.h>

#include "PluginInterface.h"
#include "PluginDefinition.h"

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reasonForCall, LPVOID /*lpReserved*/) {
    switch (reasonForCall) {
        case DLL_PROCESS_ATTACH:
            CronNpp::pluginInit(hModule);
            break;
        case DLL_PROCESS_DETACH:
            CronNpp::pluginCleanUp();
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData notepadPlusData) {
    CronNpp::g_nppData = notepadPlusData;
    CronNpp::commandMenuInit();
}

extern "C" __declspec(dllexport) const wchar_t* getName() { return L"CRON Highlighter"; }

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nbF) {
    return CronNpp::getFuncsArray(nbF);
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notifyCode) {
    switch (notifyCode->nmhdr.code) {
        case NPPN_READY:
            CronNpp::pluginReady();
            break;
        case NPPN_FILEOPENED:
        case NPPN_BUFFERACTIVATED:
            CronNpp::rescanActiveView();
            break;
        case SCN_MODIFIED:
            if (notifyCode->modificationType & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) {
                CronNpp::scheduleRescan();
            }
            break;
        case SCN_UPDATEUI:
            if (notifyCode->updated & (SC_UPDATE_V_SCROLL | SC_UPDATE_H_SCROLL)) {
                CronNpp::onViewScrolled(reinterpret_cast<HWND>(notifyCode->nmhdr.hwndFrom));
            }
            break;
        case SCN_DWELLSTART:
            CronNpp::onDwellStart(reinterpret_cast<HWND>(notifyCode->nmhdr.hwndFrom),
                                   static_cast<long long>(notifyCode->position));
            break;
        case SCN_DWELLEND:
            CronNpp::onDwellEnd(reinterpret_cast<HWND>(notifyCode->nmhdr.hwndFrom));
            break;
        default:
            break;
    }
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT /*Message*/, WPARAM /*wParam*/,
                                                       LPARAM /*lParam*/) {
    return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }
