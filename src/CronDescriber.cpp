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
// either a plain number or a recognized 3-letter month/day name.
bool resolveToken(const std::string& token, bool isMonthOrDow, int* outValue) {
    if (token.empty()) {
        return false;
    }
    if (std::isdigit(static_cast<unsigned char>(token[0]))) {
        *outValue = std::atoi(token.c_str());
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
                          const std::function<std::string(int)>& nameOf, bool* isWildcard) {
    *isWildcard = (base == "*");
    if (*isWildcard) {
        return "";
    }
    const size_t dash = base.find('-');
    if (dash != std::string::npos) {
        int a = 0, b = 0;
        if (resolveToken(base.substr(0, dash), isMonthOrDow, &a) &&
            resolveToken(base.substr(dash + 1), isMonthOrDow, &b)) {
            return nameOf(a) + " through " + nameOf(b);
        }
        return base;  // unparsable; show the raw token rather than guess
    }
    int v = 0;
    if (resolveToken(base, isMonthOrDow, &v)) {
        return nameOf(v);
    }
    return base;
}

// Describes a whole field (comma list of base[/step] items). `unitPlural`
// is used for step-only wildcards, e.g. "every 15 minutes".
std::string describeField(const std::string& field, const char* unitPlural, bool isMonthOrDow,
                           const std::function<std::string(int)>& nameOf) {
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
        std::string baseDesc = describeBase(base, isMonthOrDow, nameOf, &wildcard);

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
    if (fields.size() < 5) {
        return cronExpr;  // not a recognizable cron expression; show it verbatim
    }

    const std::string& minute = fields[0];
    const std::string& hour = fields[1];
    const std::string& dom = fields[2];
    const std::string& month = fields[3];
    const std::string& dow = fields[4];
    const std::string year = (fields.size() >= 6) ? fields[5] : "";

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
    if (!year.empty() && year != "*") {
        result += ", only in " + year;
    }

    return result;
}

}  // namespace CronNpp
