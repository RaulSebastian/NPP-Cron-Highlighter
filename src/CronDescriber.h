#pragma once

#include <string>

namespace CronNpp {

// Translates a cron expression into a human-readable English description,
// e.g. "0 5 * * *" -> "At 05:00". Accepts standard 5-field Unix cron,
// 6/7-field Quartz(.NET) cron ("sec min hour dom month dow [year]",
// including the L/W/# specials), or an "@daily"-style shorthand.
// Best-effort: covers wildcards, single values, ranges, steps, and comma
// lists, but isn't a full natural-language engine. Falls back to a generic
// phrasing for anything it can't render more specifically.
std::string describe(const std::string& cronExpr);

}  // namespace CronNpp
