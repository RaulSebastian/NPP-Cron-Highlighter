#include "CronNextRun.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace CronNpp {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s) {
    const size_t begin = s.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        return "";
    }
    const size_t end = s.find_last_not_of(" \t");
    return s.substr(begin, end - begin + 1);
}

std::vector<std::string> splitFields(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        out.push_back(tok);
    }
    return out;
}

// A naive (no timezone/DST) Gregorian calendar point. Search/carry logic
// below operates purely on calendar arithmetic; only the final result is
// converted to a real std::time_t via mktime().
struct DateTime {
    int year;
    int month;   // 1-12
    int day;     // 1-31
    int hour;    // 0-23
    int minute;  // 0-59
    int second;  // 0-59
};

bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(int year, int month) {
    static const int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return kDays[month - 1];
}

// Sakamoto's algorithm. Returns 0=Sunday .. 6=Saturday.
int dayOfWeek(int year, int month, int day) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) {
        year -= 1;
    }
    return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

void carryDay(DateTime& t) {
    t.day++;
    if (t.day > daysInMonth(t.year, t.month)) {
        t.day = 1;
        t.month++;
        if (t.month > 12) {
            t.month = 1;
            t.year++;
        }
    }
}

void bumpYear(DateTime& t) {
    t.year++;
    t.month = 1;
    t.day = 1;
    t.hour = 0;
    t.minute = 0;
    t.second = 0;
}

void bumpMonth(DateTime& t) {
    t.month++;
    if (t.month > 12) {
        t.month = 1;
        t.year++;
    }
    t.day = 1;
    t.hour = 0;
    t.minute = 0;
    t.second = 0;
}

void bumpDay(DateTime& t) {
    carryDay(t);
    t.hour = 0;
    t.minute = 0;
    t.second = 0;
}

void bumpHour(DateTime& t) {
    t.hour++;
    if (t.hour >= 24) {
        t.hour = 0;
        carryDay(t);
    }
    t.minute = 0;
    t.second = 0;
}

void bumpMinute(DateTime& t) {
    t.minute++;
    if (t.minute >= 60) {
        t.minute = 0;
        t.hour++;
        if (t.hour >= 24) {
            t.hour = 0;
            carryDay(t);
        }
    }
    t.second = 0;
}

void bumpSecond(DateTime& t) {
    t.second++;
    if (t.second >= 60) {
        t.second = 0;
        t.minute++;
        if (t.minute >= 60) {
            t.minute = 0;
            t.hour++;
            if (t.hour >= 24) {
                t.hour = 0;
                carryDay(t);
            }
        }
    }
}

enum class NameKind { None, Month, DayOfWeek };

// Resolves a bare numeric-or-name token to an integer. Day-of-week numbers
// are 0-based (0=Sunday) in standard cron (7 also accepted as Sunday) but
// 1-based in Quartz (1=Sunday); `dowOneBased` normalizes both onto the same
// 0-based scale used by dayOfWeek(). Returns -1 on failure.
int resolveToken(const std::string& token, NameKind kind, bool dowOneBased) {
    if (token.empty()) {
        return -1;
    }
    if (std::isdigit(static_cast<unsigned char>(token[0]))) {
        int v = std::atoi(token.c_str());
        if (kind == NameKind::DayOfWeek) {
            v = dowOneBased ? (((v - 1) % 7 + 7) % 7) : (v % 7);
        }
        return v;
    }
    if (kind == NameKind::None) {
        return -1;
    }
    const std::string lower = toLower(token);
    if (kind == NameKind::Month) {
        static const char* kAbbrev[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                         "jul", "aug", "sep", "oct", "nov", "dec"};
        for (int i = 0; i < 12; ++i) {
            if (lower == kAbbrev[i]) {
                return i + 1;
            }
        }
        return -1;
    }
    static const char* kAbbrev[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
    for (int i = 0; i < 7; ++i) {
        if (lower == kAbbrev[i]) {
            return i;
        }
    }
    return -1;
}

// Matches a single comma-separated item (base or base/step) against value.
bool itemMatches(const std::string& item, int value, int fieldMin, NameKind kind,
                  bool dowOneBased) {
    const size_t slash = item.find('/');
    const std::string base = (slash == std::string::npos) ? item : item.substr(0, slash);
    const bool hasStep = slash != std::string::npos;
    const int step = hasStep ? std::atoi(item.c_str() + slash + 1) : 0;

    if (base == "*") {
        if (!hasStep) {
            return true;
        }
        return step > 0 && value >= fieldMin && (value - fieldMin) % step == 0;
    }
    const size_t dash = base.find('-');
    if (dash != std::string::npos) {
        const int lo = resolveToken(base.substr(0, dash), kind, dowOneBased);
        const int hi = resolveToken(base.substr(dash + 1), kind, dowOneBased);
        if (lo < 0 || hi < 0 || value < lo || value > hi) {
            return false;
        }
        return !hasStep || (step > 0 && (value - lo) % step == 0);
    }
    const int v = resolveToken(base, kind, dowOneBased);
    if (v < 0) {
        return false;
    }
    if (hasStep) {
        return step > 0 && value >= v && (value - v) % step == 0;
    }
    return value == v;
}

bool fieldMatches(const std::string& field, int value, int fieldMin, NameKind kind = NameKind::None,
                   bool dowOneBased = false) {
    size_t start = 0;
    while (true) {
        const size_t comma = field.find(',', start);
        const std::string item =
            field.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (itemMatches(item, value, fieldMin, kind, dowOneBased)) {
            return true;
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return false;
}

// Quartz "nearest weekday" rule shared by LW and NW: never crosses a month
// boundary, so a Saturday/Sunday target that would spill over the month's
// edge falls back the other direction instead.
int nearestWeekday(int year, int month, int target) {
    const int last = daysInMonth(year, month);
    const int wd = dayOfWeek(year, month, target);
    if (wd == 6) {  // Saturday
        return (target > 1) ? target - 1 : target + 2;
    }
    if (wd == 0) {  // Sunday
        return (target < last) ? target + 1 : target - 2;
    }
    return target;
}

bool domMatches(const std::string& dom, const DateTime& t) {
    if (dom == "*") {
        return true;
    }
    const int last = daysInMonth(t.year, t.month);
    if (dom == "L") {
        return t.day == last;
    }
    if (dom == "LW") {
        return t.day == nearestWeekday(t.year, t.month, last);
    }
    if (dom.size() > 2 && dom[0] == 'L' && dom[1] == '-') {
        return t.day == last - std::atoi(dom.c_str() + 2);
    }
    if (dom.size() > 1 && dom.back() == 'W' && std::isdigit(static_cast<unsigned char>(dom[0]))) {
        const int target = std::min(std::atoi(dom.c_str()), last);
        return t.day == nearestWeekday(t.year, t.month, target);
    }
    return fieldMatches(dom, t.day, 1);
}

bool dowMatches(const std::string& dow, bool quartzMode, const DateTime& t) {
    if (dow == "*") {
        return true;
    }
    const int wd = dayOfWeek(t.year, t.month, t.day);
    if (dow == "L") {
        return wd == 6;  // bare L => Saturday
    }
    const size_t hash = dow.find('#');
    if (hash != std::string::npos) {
        const int target = resolveToken(dow.substr(0, hash), NameKind::DayOfWeek, true);
        if (target < 0 || wd != target) {
            return false;
        }
        const int nth = std::atoi(dow.c_str() + hash + 1);
        return ((t.day - 1) / 7) + 1 == nth;
    }
    if (dow.size() > 1 && dow.back() == 'L') {
        const int target = resolveToken(dow.substr(0, dow.size() - 1), NameKind::DayOfWeek, true);
        if (target < 0 || wd != target) {
            return false;
        }
        return t.day + 7 > daysInMonth(t.year, t.month);
    }
    return fieldMatches(dow, wd, 0, NameKind::DayOfWeek, quartzMode);
}

bool dayMatches(const std::string& dom, const std::string& dow, bool isQuartz, const DateTime& t) {
    if (isQuartz) {
        return (dom != "?") ? domMatches(dom, t) : dowMatches(dow, true, t);
    }
    const bool domWild = (dom == "*");
    const bool dowWild = (dow == "*");
    if (domWild && dowWild) {
        return true;
    }
    if (domWild) {
        return dowMatches(dow, false, t);
    }
    if (dowWild) {
        return domMatches(dom, t);
    }
    return domMatches(dom, t) || dowMatches(dow, false, t);
}

struct ParsedCron {
    bool isQuartz = false;
    bool hasYear = false;
    std::string second, minute, hour, dom, month, dow, year;
};

std::optional<ParsedCron> parseCron(const std::string& exprIn) {
    const std::string expr = trim(exprIn);
    if (expr.empty()) {
        return std::nullopt;
    }
    if (expr[0] == '@') {
        const std::string lower = toLower(expr);
        ParsedCron p;
        if (lower == "@yearly" || lower == "@annually") {
            p.minute = "0"; p.hour = "0"; p.dom = "1"; p.month = "1"; p.dow = "*";
        } else if (lower == "@monthly") {
            p.minute = "0"; p.hour = "0"; p.dom = "1"; p.month = "*"; p.dow = "*";
        } else if (lower == "@weekly") {
            p.minute = "0"; p.hour = "0"; p.dom = "*"; p.month = "*"; p.dow = "0";
        } else if (lower == "@daily" || lower == "@midnight") {
            p.minute = "0"; p.hour = "0"; p.dom = "*"; p.month = "*"; p.dow = "*";
        } else if (lower == "@hourly") {
            p.minute = "0"; p.hour = "*"; p.dom = "*"; p.month = "*"; p.dow = "*";
        } else {
            return std::nullopt;  // "@reboot" has no periodic schedule
        }
        return p;
    }

    const std::vector<std::string> fields = splitFields(expr);
    ParsedCron p;
    if (fields.size() == 5) {
        p.minute = fields[0]; p.hour = fields[1]; p.dom = fields[2];
        p.month = fields[3]; p.dow = fields[4];
        return p;
    }
    if (fields.size() == 6 || fields.size() == 7) {
        p.isQuartz = true;
        p.second = fields[0]; p.minute = fields[1]; p.hour = fields[2];
        p.dom = fields[3]; p.month = fields[4]; p.dow = fields[5];
        p.hasYear = fields.size() == 7;
        if (p.hasYear) {
            p.year = fields[6];
        }
        return p;
    }
    return std::nullopt;
}

}  // namespace

std::optional<std::time_t> nextRun(const std::string& cronExpr, std::time_t now) {
    const std::optional<ParsedCron> parsed = parseCron(cronExpr);
    if (!parsed) {
        return std::nullopt;
    }
    const ParsedCron& p = *parsed;

    const std::tm* nowTm = std::localtime(&now);
    if (!nowTm) {
        return std::nullopt;
    }
    DateTime t{nowTm->tm_year + 1900, nowTm->tm_mon + 1, nowTm->tm_mday,
               nowTm->tm_hour, nowTm->tm_min, nowTm->tm_sec};

    if (p.isQuartz) {
        bumpSecond(t);  // strictly after `now`
    } else {
        t.second = 0;
        bumpMinute(t);  // standard cron always fires at :00 seconds
    }

    const int cutoffYear = t.year + 8;  // bounded search horizon
    int iterations = 0;
    const int kMaxIterations = 2000000;
    while (t.year <= cutoffYear && iterations++ < kMaxIterations) {
        if (p.hasYear && !fieldMatches(p.year, t.year, 1970)) {
            bumpYear(t);
            continue;
        }
        if (!fieldMatches(p.month, t.month, 1, NameKind::Month)) {
            bumpMonth(t);
            continue;
        }
        if (!dayMatches(p.dom, p.dow, p.isQuartz, t)) {
            bumpDay(t);
            continue;
        }
        if (!fieldMatches(p.hour, t.hour, 0)) {
            bumpHour(t);
            continue;
        }
        if (!fieldMatches(p.minute, t.minute, 0)) {
            bumpMinute(t);
            continue;
        }
        if (p.isQuartz && !fieldMatches(p.second, t.second, 0)) {
            bumpSecond(t);
            continue;
        }

        std::tm result{};
        result.tm_year = t.year - 1900;
        result.tm_mon = t.month - 1;
        result.tm_mday = t.day;
        result.tm_hour = t.hour;
        result.tm_min = t.minute;
        result.tm_sec = t.second;
        result.tm_isdst = -1;
        const std::time_t rt = std::mktime(&result);
        if (rt == static_cast<std::time_t>(-1)) {
            return std::nullopt;
        }
        return rt;
    }
    return std::nullopt;
}

std::string formatNextRun(std::time_t next, std::time_t now) {
    const std::tm* nextTmPtr = std::localtime(&next);
    if (!nextTmPtr) {
        return "";
    }
    const std::tm nextTm = *nextTmPtr;

    const std::tm* nowTmPtr = std::localtime(&now);
    const std::tm nowTm = nowTmPtr ? *nowTmPtr : nextTm;

    static const char* kDowNames[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                       "Thursday", "Friday", "Saturday"};
    static const char* kMonthNames[] = {"January", "February", "March",     "April",
                                         "May",     "June",     "July",     "August",
                                         "September", "October", "November", "December"};

    char timeBuf[16];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", nextTm.tm_hour, nextTm.tm_min,
                  nextTm.tm_sec);

    const bool sameYear = nextTm.tm_year == nowTm.tm_year;
    const bool sameDay =
        sameYear && nextTm.tm_yday == nowTm.tm_yday;
    const bool isTomorrow =
        sameYear && nextTm.tm_yday == nowTm.tm_yday + 1;
    // Handles the Dec 31 -> Jan 1 boundary, where tm_yday resets to 0.
    const bool isTomorrowAcrossYearEnd =
        !sameYear && nextTm.tm_year == nowTm.tm_year + 1 && nextTm.tm_yday == 0 &&
        nowTm.tm_yday == (isLeapYear(nowTm.tm_year + 1900) ? 365 : 364);

    if (sameDay) {
        return std::string("Today at ") + timeBuf;
    }
    if (isTomorrow || isTomorrowAcrossYearEnd) {
        return std::string("Tomorrow at ") + timeBuf;
    }

    const double diffDays = std::difftime(next, now) / 86400.0;
    if (diffDays < 7.0) {
        return std::string(kDowNames[nextTm.tm_wday]) + " at " + timeBuf;
    }

    std::string date = std::string(kDowNames[nextTm.tm_wday]) + ", " +
                        kMonthNames[nextTm.tm_mon] + " " + std::to_string(nextTm.tm_mday);
    if (!sameYear) {
        date += ", " + std::to_string(nextTm.tm_year + 1900);
    }
    return date + " at " + timeBuf;
}

}  // namespace CronNpp
