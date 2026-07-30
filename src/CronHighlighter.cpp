#include "CronHighlighter.h"

#include <string>
#include <vector>

#include "Scintilla.h"

#include "CronDetector.h"

namespace CronNpp {

void initIndicatorStyle(HWND scintilla, int indicatorId) {
    // A soft yellow highlighter-marker box behind the text, rather than a
    // squiggle. INDICSETUNDER draws it beneath the text so glyphs stay
    // legible on top of the fill.
    ::SendMessage(scintilla, SCI_INDICSETSTYLE, indicatorId, INDIC_STRAIGHTBOX);
    ::SendMessage(scintilla, SCI_INDICSETFORE, indicatorId, RGB(255, 230, 0));
    ::SendMessage(scintilla, SCI_INDICSETALPHA, indicatorId, 120);
    ::SendMessage(scintilla, SCI_INDICSETOUTLINEALPHA, indicatorId, 0);
    ::SendMessage(scintilla, SCI_INDICSETUNDER, indicatorId, TRUE);
}

void clearHighlights(HWND scintilla, int indicatorId) {
    const auto length = static_cast<size_t>(::SendMessage(scintilla, SCI_GETLENGTH, 0, 0));
    ::SendMessage(scintilla, SCI_SETINDICATORCURRENT, indicatorId, 0);
    ::SendMessage(scintilla, SCI_INDICATORCLEARRANGE, 0, static_cast<LPARAM>(length));
}

void highlightDocument(HWND scintilla, int indicatorId) {
    const auto length = static_cast<size_t>(::SendMessage(scintilla, SCI_GETLENGTH, 0, 0));

    clearHighlights(scintilla, indicatorId);
    if (length == 0) {
        return;
    }

    std::string text(length, '\0');
    // SCI_GETTEXT wants room for the trailing NUL in the requested length.
    ::SendMessage(scintilla, SCI_GETTEXT, static_cast<WPARAM>(length + 1),
                  reinterpret_cast<LPARAM>(text.data()));

    const std::vector<Match> matches = findMatches(text);
    if (matches.empty()) {
        return;
    }

    ::SendMessage(scintilla, SCI_SETINDICATORCURRENT, indicatorId, 0);
    for (const Match& m : matches) {
        ::SendMessage(scintilla, SCI_INDICATORFILLRANGE, static_cast<WPARAM>(m.start),
                      static_cast<LPARAM>(m.length));
    }
}

}  // namespace CronNpp
