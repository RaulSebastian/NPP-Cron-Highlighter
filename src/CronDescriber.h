#pragma once

#include <string>

namespace CronNpp {

// Translates a cron expression (5-field, optional 6th year field, or an
// "@daily"-style shorthand) into a human-readable English description,
// e.g. "0 5 * * *" -> "At 05:00". Best-effort: covers wildcards, single
// values, ranges, steps, and comma lists, but isn't a full natural-language
// engine. Falls back to a generic phrasing for anything it can't render
// more specifically.
std::string describe(const std::string& cronExpr);

}  // namespace CronNpp
