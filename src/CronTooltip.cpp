#include "CronTooltip.h"

#include <ctime>
#include <string>

#include "Scintilla.h"

#include "CronDescriber.h"
#include "CronDetector.h"
#include "CronNextRun.h"

namespace CronNpp {

void enableDwell(HWND scintilla) {
    // Milliseconds of mouse rest before Scintilla fires SCN_DWELLSTART.
    ::SendMessage(scintilla, SCI_SETMOUSEDWELLTIME, 400, 0);
}

void hideTooltip(HWND scintilla) { ::SendMessage(scintilla, SCI_CALLTIPCANCEL, 0, 0); }

void showTooltipIfMatch(HWND scintilla, long long position) {
    if (position < 0) {
        hideTooltip(scintilla);
        return;
    }

    const auto line =
        static_cast<Sci_Position>(::SendMessage(scintilla, SCI_LINEFROMPOSITION,
                                                 static_cast<WPARAM>(position), 0));
    const auto lineStart = static_cast<Sci_Position>(
        ::SendMessage(scintilla, SCI_POSITIONFROMLINE, static_cast<WPARAM>(line), 0));
    const auto lineLen = static_cast<Sci_Position>(
        ::SendMessage(scintilla, SCI_LINELENGTH, static_cast<WPARAM>(line), 0));
    if (lineLen <= 0) {
        hideTooltip(scintilla);
        return;
    }

    std::string lineText(static_cast<size_t>(lineLen), '\0');
    ::SendMessage(scintilla, SCI_GETLINE, static_cast<WPARAM>(line),
                  reinterpret_cast<LPARAM>(lineText.data()));
    // SCI_GETLINE includes the line ending; strip it before field-splitting.
    while (!lineText.empty() && (lineText.back() == '\n' || lineText.back() == '\r')) {
        lineText.pop_back();
    }

    const long long relPos = position - lineStart;
    for (const Match& m : findMatches(lineText)) {
        const long long matchEnd = static_cast<long long>(m.start + m.length);
        if (relPos >= static_cast<long long>(m.start) && relPos < matchEnd) {
            const std::string cronText = lineText.substr(m.start, m.length);
            std::string tooltipText = "Cron expression:\n" + describe(cronText);
            const std::time_t now = std::time(nullptr);
            if (const auto next = nextRun(cronText, now)) {
                tooltipText += "\n\nNext occurrence:\n" + formatNextRun(*next, now);
            }
            ::SendMessage(scintilla, SCI_CALLTIPSHOW,
                          static_cast<WPARAM>(lineStart + static_cast<Sci_Position>(m.start)),
                          reinterpret_cast<LPARAM>(tooltipText.c_str()));
            return;
        }
    }
    hideTooltip(scintilla);
}

}  // namespace CronNpp
