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

bool isLargeDocument(HWND scintilla) {
    const auto length = static_cast<long long>(::SendMessage(scintilla, SCI_GETLENGTH, 0, 0));
    return length >= kLargeDocumentThresholdBytes;
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

namespace {
// Extra lines scanned above/below the viewport so a small scroll doesn't
// immediately require another rescan.
constexpr Sci_Position kVisibleRangeMarginLines = 50;
}  // namespace

void highlightVisibleRange(HWND scintilla, int indicatorId) {
    const auto totalLines = static_cast<Sci_Position>(::SendMessage(scintilla, SCI_GETLINECOUNT, 0, 0));
    if (totalLines <= 0) {
        return;
    }

    const auto firstVisibleLine =
        static_cast<Sci_Position>(::SendMessage(scintilla, SCI_GETFIRSTVISIBLELINE, 0, 0));
    const auto firstDocLine = static_cast<Sci_Position>(
        ::SendMessage(scintilla, SCI_DOCLINEFROMVISIBLE, static_cast<WPARAM>(firstVisibleLine), 0));
    const auto linesOnScreen = static_cast<Sci_Position>(::SendMessage(scintilla, SCI_LINESONSCREEN, 0, 0));

    Sci_Position startLine = firstDocLine - kVisibleRangeMarginLines;
    Sci_Position endLine = firstDocLine + linesOnScreen + kVisibleRangeMarginLines;
    if (startLine < 0) {
        startLine = 0;
    }
    if (endLine > totalLines - 1) {
        endLine = totalLines - 1;
    }

    const auto rangeStart =
        static_cast<Sci_Position>(::SendMessage(scintilla, SCI_POSITIONFROMLINE, static_cast<WPARAM>(startLine), 0));
    const auto rangeEnd = static_cast<Sci_Position>(
        ::SendMessage(scintilla, SCI_GETLINEENDPOSITION, static_cast<WPARAM>(endLine), 0));
    if (rangeEnd <= rangeStart) {
        return;
    }
    const auto rangeLength = static_cast<size_t>(rangeEnd - rangeStart);

    std::string text(rangeLength + 1, '\0');  // +1: room for Scintilla's trailing NUL
    Sci_TextRangeFull tr{};
    tr.chrg.cpMin = rangeStart;
    tr.chrg.cpMax = rangeEnd;
    tr.lpstrText = text.data();
    ::SendMessage(scintilla, SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<LPARAM>(&tr));
    text.resize(rangeLength);

    ::SendMessage(scintilla, SCI_SETINDICATORCURRENT, indicatorId, 0);
    ::SendMessage(scintilla, SCI_INDICATORCLEARRANGE, static_cast<WPARAM>(rangeStart),
                  static_cast<LPARAM>(rangeLength));

    const std::vector<Match> matches = findMatches(text);
    for (const Match& m : matches) {
        ::SendMessage(scintilla, SCI_INDICATORFILLRANGE, static_cast<WPARAM>(rangeStart + m.start),
                      static_cast<LPARAM>(m.length));
    }
}

}  // namespace CronNpp
