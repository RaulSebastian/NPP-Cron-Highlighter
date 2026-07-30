#include "CronDescriber.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <functional>
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

const char* kMonthNames[] = {"January", "February", "March",     "April",   "May",      "June",
                             "July",    "August",   "September", "October", "November", "December"};
const char* kDowNames[] = {"Sunday", "Monday",   "Tuesday", "Wednesday",
                            "Thursday", "Friday", "Saturday"};

// Resolves a bare token (no comma/range/step) to an integer, accepting
// either a plain number or a recognized 3-letter month/day name. Named
// tokens always resolve to a 0-based day-of-week (0=Sunday), matching
// kDowNames' indexing. Numeric day-of-week tokens are 0-based in standard
// cron (0 or 7=Sunday) but 1-based in Quartz (1=Sunday); pass
// `dowIsOneBased=true` when parsing a Quartz day-of-week field so a numeric
// token lands on the same 0-based convention as a named one.
bool resolveToken(const std::string& token, bool isMonthOrDow, int* outValue,
                   bool dowIsOneBased = false) {
    if (token.empty()) {
        return false;
    }
    if (std::isdigit(static_cast<unsigned char>(token[0]))) {
        const int v = std::atoi(token.c_str());
        *outValue = dowIsOneBased ? (((v - 1) % 7 + 7) % 7) : v;
        return true;
    }
    if (!isMonthOrDow) {
        return false;
    }
    const std::string lower = toLower(token);
    static const char* kMonthAbbrev[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                         "jul", "aug", "sep", "oct", "nov", "dec"};
    for (int i = 0; i < 12; ++i) {
        if (lower == kMonthAbbrev[i]) {
            *outValue = i + 1;
            return true;
        }
    }
    static const char* kDowAbbrev[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
    for (int i = 0; i < 7; ++i) {
        if (lower == kDowAbbrev[i]) {
            *outValue = i;
            return true;
        }
    }
    return false;
}

std::string joinWithAnd(const std::vector<std::string>& parts) {
    if (parts.empty()) {
        return "";
    }
    if (parts.size() == 1) {
        return parts[0];
    }
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            out += (i + 1 == parts.size()) ? " and " : ", ";
        }
        out += parts[i];
    }
    return out;
}

// Describes one field's value (the part of a comma-separated item before
// any "/step"), given a function to render a resolved integer.
std::string describeBase(const std::string& base, bool isMonthOrDow,
                          const std::function<std::string(int)>& nameOf, bool* isWildcard,
                          bool dowIsOneBased = false) {
    *isWildcard = (base == "*");
    if (*isWildcard) {
        return "";
    }
    const size_t dash = base.find('-');
    if (dash != std::string::npos) {
        int a = 0, b = 0;
        if (resolveToken(base.substr(0, dash), isMonthOrDow, &a, dowIsOneBased) &&
            resolveToken(base.substr(dash + 1), isMonthOrDow, &b, dowIsOneBased)) {
            return nameOf(a) + " through " + nameOf(b);
        }
        return base;  // unparsable; show the raw token rather than guess
    }
    int v = 0;
    if (resolveToken(base, isMonthOrDow, &v, dowIsOneBased)) {
        return nameOf(v);
    }
    return base;
}

// Describes a whole field (comma list of base[/step] items). `unitPlural`
// is used for step-only wildcards, e.g. "every 15 minutes". Pass
// `dowIsOneBased=true` for a Quartz day-of-week field (see resolveToken).
std::string describeField(const std::string& field, const char* unitPlural, bool isMonthOrDow,
                           const std::function<std::string(int)>& nameOf,
                           bool dowIsOneBased = false) {
    std::vector<std::string> items;
    std::stringstream ss(field);
    std::string item;
    while (std::getline(ss, item, ',')) {
        items.push_back(item);
    }

    std::vector<std::string> phrases;
    for (const std::string& raw : items) {
        const size_t slash = raw.find('/');
        const std::string base = (slash == std::string::npos) ? raw : raw.substr(0, slash);
        const std::string step = (slash == std::string::npos) ? "" : raw.substr(slash + 1);

        bool wildcard = false;
        std::string baseDesc = describeBase(base, isMonthOrDow, nameOf, &wildcard, dowIsOneBased);

        if (!step.empty()) {
            if (wildcard) {
                phrases.push_back(std::string("every ") + step + " " + unitPlural);
            } else {
                phrases.push_back(std::string("every ") + step + " " + unitPlural + " from " +
                                   baseDesc);
            }
        } else if (!wildcard) {
            phrases.push_back(baseDesc);
        }
        // A plain "*" with no step carries no restriction and contributes nothing.
    }

    return joinWithAnd(phrases);
}

bool isSingleNumeric(const std::string& field, int* value) {
    if (field.empty()) {
        return false;
    }
    for (char c : field) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    *value = std::atoi(field.c_str());
    return true;
}

std::string describeMinuteHour(const std::string& minute, const std::string& hour) {
    int minuteVal = 0, hourVal = 0;
    const bool minuteSingle = isSingleNumeric(minute, &minuteVal);
    const bool hourSingle = isSingleNumeric(hour, &hourVal);

    if (minuteSingle && hourSingle) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "At %02d:%02d", hourVal, minuteVal);
        return buf;
    }
    if (minuteSingle && hour == "*") {
        return "At minute " + std::to_string(minuteVal) + " past every hour";
    }

    const auto plainNumber = [](int v) { return std::to_string(v); };
    const std::string minutePhrase = describeField(minute, "minutes", false, plainNumber);
    const std::string hourPhrase = describeField(hour, "hours", false, plainNumber);

    if (minutePhrase.empty() && hourPhrase.empty()) {
        return "Every minute";
    }

    // describeField() renders a step-wildcard field (e.g. "*/15") as a
    // self-contained phrase like "every 15 minutes" that already includes
    // its unit; anything else (a value or list, e.g. "5" or "5 and 15")
    // still needs a "minute "/"past hour " label to make sense standalone.
    const bool minuteIsEvery = minutePhrase.rfind("every ", 0) == 0;
    const bool hourIsEvery = hourPhrase.rfind("every ", 0) == 0;

    std::string result;
    if (minutePhrase.empty()) {
        result = "At every minute";
    } else if (minuteIsEvery) {
        result = "E" + minutePhrase.substr(1);  // "every 15 minutes" -> "Every 15 minutes"
    } else {
        result = "At minute " + minutePhrase;
    }

    if (!hourPhrase.empty()) {
        result += hourIsEvery ? (", " + hourPhrase) : (" past hour " + hourPhrase);
    }
    return result;
}

std::string describeShorthand(const std::string& expr) {
    const std::string lower = toLower(expr);
    if (lower == "@yearly" || lower == "@annually") return "Once a year at midnight, on January 1";
    if (lower == "@monthly") return "Once a month at midnight, on day 1 of the month";
    if (lower == "@weekly") return "Once a week at midnight, on Sunday";
    if (lower == "@daily" || lower == "@midnight") return "Once a day, at midnight";
    if (lower == "@hourly") return "Once an hour, at the start of the hour";
    if (lower == "@reboot") return "At system startup";
    return expr;
}

std::string ordinal(int n) {
    const char* suffix = "th";
    if (n % 100 < 11 || n % 100 > 13) {
        switch (n % 10) {
            case 1: suffix = "st"; break;
            case 2: suffix = "nd"; break;
            case 3: suffix = "rd"; break;
        }
    }
    return std::to_string(n) + suffix;
}

// Quartz analogue of describeMinuteHour(), one level deeper (seconds are
// the innermost unit, same relationship minutes have to hours there).
std::string describeQuartzTime(const std::string& second, const std::string& minute,
                                const std::string& hour) {
    int secVal = 0, minVal = 0, hourVal = 0;
    const bool secSingle = isSingleNumeric(second, &secVal);
    const bool minSingle = isSingleNumeric(minute, &minVal);
    const bool hourSingle = isSingleNumeric(hour, &hourVal);

    if (secSingle && minSingle && hourSingle) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "At %02d:%02d:%02d", hourVal, minVal, secVal);
        return buf;
    }

    const auto plainNumber = [](int v) { return std::to_string(v); };
    const std::string secPhrase = describeField(second, "seconds", false, plainNumber);

    std::string result;
    const bool secIsEvery = secPhrase.rfind("every ", 0) == 0;
    if (secPhrase.empty()) {
        result = "At every second";
    } else if (secIsEvery) {
        result = "E" + secPhrase.substr(1);  // "every 30 seconds" -> "Every 30 seconds"
    } else {
        result = "At second " + secPhrase;
    }

    const std::string minHour = describeMinuteHour(minute, hour);
    // describeMinuteHour() renders its own "At ..."/"Every ..." lead-in,
    // which reads redundantly once seconds already introduced one; fold it
    // in as a plain clause instead.
    if (minHour != "Every minute") {
        std::string clause = minHour;
        for (const char* lead : {"At ", "Every "}) {
            if (clause.rfind(lead, 0) == 0) {
                clause = clause.substr(std::string(lead).size());
                break;
            }
        }
        result += ", " + clause;
    }
    return result;
}

// Day-of-month description for Quartz, including the L/W specials:
// L ("last day"), L-N ("N days before the last day"), LW ("last weekday"),
// NW ("weekday nearest day N").
std::string describeQuartzDom(const std::string& dom) {
    if (dom == "*") {
        return "";
    }
    if (dom == "L") {
        return "the last day of the month";
    }
    if (dom == "LW") {
        return "the last weekday of the month";
    }
    if (dom.size() > 2 && dom[0] == 'L' && dom[1] == '-') {
        return dom.substr(2) + " days before the last day of the month";
    }
    if (dom.size() > 1 && dom.back() == 'W' && std::isdigit(static_cast<unsigned char>(dom[0]))) {
        return "the weekday nearest day " + dom.substr(0, dom.size() - 1);
    }
    return "day " +
           describeField(dom, "days", false, [](int v) { return std::to_string(v); }) +
           " of the month";
}

// Day-of-week description for Quartz, including the L/# specials: bare L
// (= Saturday), N/name + L ("the last <day> of the month"), and N/name +
// #M ("the Mth <day> of the month"). Numeric tokens are 1-based (1=Sunday)
// per Quartz convention, so they go through resolveToken/describeField with
// dowIsOneBased=true.
std::string describeQuartzDow(const std::string& dow, const std::function<std::string(int)>& dowName) {
    if (dow == "*") {
        return "";
    }
    if (dow == "L") {
        return dowName(6);  // bare L => Saturday
    }
    const size_t hash = dow.find('#');
    if (hash != std::string::npos) {
        int v = 0;
        if (resolveToken(dow.substr(0, hash), true, &v, /*dowIsOneBased=*/true)) {
            const int nth = std::atoi(dow.substr(hash + 1).c_str());
            return ordinal(nth) + " " + dowName(v) + " of the month";
        }
        return dow;
    }
    if (dow.size() > 1 && dow.back() == 'L') {
        int v = 0;
        if (resolveToken(dow.substr(0, dow.size() - 1), true, &v, /*dowIsOneBased=*/true)) {
            return "the last " + dowName(v) + " of the month";
        }
    }
    return describeField(dow, "days", true, dowName, /*dowIsOneBased=*/true);
}

// Quartz(.NET) 6/7-field cron: "sec min hour dom month dow [year]". Unlike
// standard cron, dom/dow are mutually exclusive (the grammar in
// CronDetector.cpp guarantees exactly one of them is "?"), so there's no
// "or" clause to build — just describe whichever one isn't "?".
std::string describeQuartz(const std::vector<std::string>& fields) {
    const std::string& second = fields[0];
    const std::string& minute = fields[1];
    const std::string& hour = fields[2];
    const std::string& dom = fields[3];
    const std::string& month = fields[4];
    const std::string& dow = fields[5];
    const std::string year = (fields.size() >= 7) ? fields[6] : "";

    std::string result = describeQuartzTime(second, minute, hour);

    const auto monthName = [](int v) { return (v >= 1 && v <= 12) ? kMonthNames[v - 1] : "?"; };
    const auto dowName = [](int v) { return kDowNames[((v % 7) + 7) % 7]; };

    const std::string dayDesc = (dom != "?") ? describeQuartzDom(dom) : describeQuartzDow(dow, dowName);
    if (!dayDesc.empty()) {
        result += ", on " + dayDesc;
    }

    const std::string monthDesc = (month == "*") ? "" : describeField(month, "months", true, monthName);
    if (!monthDesc.empty()) {
        result += ", in " + monthDesc;
    }
    if (!year.empty() && year != "*") {
        result += ", only in " + year;
    }
    return result;
}

}  // namespace

std::string describe(const std::string& cronExpr) {
    const std::string expr = trim(cronExpr);
    if (expr.empty()) {
        return expr;
    }
    if (expr[0] == '@') {
        return describeShorthand(expr);
    }

    const std::vector<std::string> fields = splitFields(expr);
    if (fields.size() == 6 || fields.size() == 7) {
        return describeQuartz(fields);  // sec min hour dom month dow [year]
    }
    if (fields.size() != 5) {
        return cronExpr;  // not a recognizable cron expression; show it verbatim
    }

    const std::string& minute = fields[0];
    const std::string& hour = fields[1];
    const std::string& dom = fields[2];
    const std::string& month = fields[3];
    const std::string& dow = fields[4];

    std::string result = describeMinuteHour(minute, hour);

    const auto monthName = [](int v) {
        return (v >= 1 && v <= 12) ? kMonthNames[v - 1] : "?";
    };
    const auto dowName = [](int v) {
        const int idx = v % 7;  // cron allows 7 for Sunday in addition to 0
        return kDowNames[idx];
    };

    const std::string domDesc =
        (dom == "*") ? "" : ("day " + describeField(dom, "days", false, [](int v) {
                                 return std::to_string(v);
                             }) + " of the month");
    const std::string dowDesc = (dow == "*") ? "" : describeField(dow, "days", true, dowName);
    const std::string monthDesc = (month == "*") ? "" : describeField(month, "months", true, monthName);

    if (!domDesc.empty() && !dowDesc.empty()) {
        // Per cron semantics, when both are restricted either one matching is enough.
        result += ", on " + domDesc + " or on " + dowDesc;
    } else if (!domDesc.empty()) {
        result += ", on " + domDesc;
    } else if (!dowDesc.empty()) {
        result += ", on " + dowDesc;
    }

    if (!monthDesc.empty()) {
        result += ", in " + monthDesc;
    }

    return result;
}

}  // namespace CronNpp
