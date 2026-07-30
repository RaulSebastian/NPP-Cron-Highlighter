#pragma once

#include <string>
#include <vector>

namespace CronNpp {

struct Match {
    size_t start;   // byte offset into the scanned text
    size_t length;  // byte length of the match
};

// Scans a block of text (typically a whole document) for lines that look
// like cron expressions and returns their byte offsets/lengths.
//
// Recognizes three shapes:
//   - Standard 5-field Unix cron: "min hour dom month dow".
//   - Quartz(.NET) 6/7-field cron: "sec min hour dom month dow [year]".
//     Enforces Quartz's day-of-month/day-of-week exclusivity rule (exactly
//     one of the two must be "?"), and supports the L/W/# specials (last
//     day, nearest weekday, nth weekday-of-month).
//   - "@daily" / "@hourly" / ... shorthands.
// These two field orderings are ambiguous for a bare 6-token line, which is
// why they're mutually exclusive shapes rather than "5 fields + optional
// trailing field": a plain 5-field match never carries a 6th field, and a
// 6/7-field match is only ever interpreted as Quartz (seconds-first).
//
// To keep the false-positive rate down on arbitrary text files, a 5-field
// match is discarded unless at least one field contains a cron-specific
// character (*, /, ,, or -) — a bare "1 2 3 4 5" is technically valid cron
// syntax but far more likely to be unrelated numbers in a text file. Quartz
// matches are exempt from this check since the day-of-month/day-of-week "?"
// is mandatory and already unambiguous. Tune isLikelyCron() in the .cpp if
// the heuristic is too strict/loose for your use case.
std::vector<Match> findMatches(const std::string& text);

}  // namespace CronNpp
