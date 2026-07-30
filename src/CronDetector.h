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
// Deliberately permissive: matches standard 5-field cron ("* * * * *"),
// an optional trailing year field (Quartz/6-field style), and the common
// "@daily" / "@hourly" / ... shorthands. To keep the false-positive rate
// down on arbitrary text files, a match is discarded unless at least one
// field contains a cron-specific character (*, /, ,, or -) — a bare
// "1 2 3 4 5" is technically valid cron syntax but far more likely to be
// unrelated numbers in a text file. Tune isLikelyCron() in the .cpp if
// that heuristic is too strict/loose for your use case.
std::vector<Match> findMatches(const std::string& text);

}  // namespace CronNpp
