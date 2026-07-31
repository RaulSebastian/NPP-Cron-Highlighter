#pragma once

#include <ctime>
#include <optional>
#include <string>

namespace CronNpp {

// Computes the next time (strictly after `now`) at which `cronExpr` fires.
// Accepts the same shapes as CronDetector/CronDescriber: standard 5-field
// Unix cron, 6/7-field Quartz(.NET) cron (including the L/W/# specials),
// and "@..." shorthands ("@reboot" has no periodic schedule and always
// returns nullopt). Returns nullopt if no match exists within a bounded
// search horizon (e.g. a year field already in the past, or a
// day-of-month/month combination that can never occur, like day 31 in
// February).
std::optional<std::time_t> nextRun(const std::string& cronExpr, std::time_t now);

// Renders `next` relative to `now` for display, e.g. "Today at 05:00:00",
// "Tomorrow at 12:00:00", "Friday at 09:00:00", or
// "Monday, March 3, 2027 at 00:00:00" for anything further out.
std::string formatNextRun(std::time_t next, std::time_t now);

}  // namespace CronNpp
