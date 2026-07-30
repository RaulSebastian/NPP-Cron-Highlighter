#include "CronDetector.h"

#include <regex>

namespace CronNpp {

namespace {

// A single numeric cron field: *, N, N-N, */N, N-N/N, and comma lists of
// the above (e.g. "1,15-20/2,*/5").
constexpr auto kNumField =
    R"((?:\*|[0-9]{1,2}(?:-[0-9]{1,2})?)(?:/[0-9]{1,3})?)"
    R"((?:,(?:\*|[0-9]{1,2}(?:-[0-9]{1,2})?)(?:/[0-9]{1,3})?)*)";

// Month/day-of-week field: same as above but also accepts 3-letter names
// (JAN-DEC, SUN-SAT), since those are common in real crontabs.
constexpr auto kNameField =
    R"((?:\*|[0-9]{1,2}(?:-[0-9]{1,2})?|[A-Za-z]{3}(?:-[A-Za-z]{3})?))"
    R"((?:/[0-9]{1,3})?)"
    R"((?:,(?:\*|[0-9]{1,2}(?:-[0-9]{1,2})?|[A-Za-z]{3}(?:-[A-Za-z]{3})?)(?:/[0-9]{1,3})?)*)";

const std::regex& cronRegex() {
    static const std::regex re(
        std::string(R"(^[ \t]*()")
        + R"(@(?:yearly|annually|monthly|weekly|daily|midnight|hourly|reboot))"
        + R"(|)" + kNumField + R"([ \t]+)" + kNumField + R"([ \t]+)" + kNumField +
        R"([ \t]+)" + kNameField + R"([ \t]+)" + kNameField +
        R"((?:[ \t]+)" + kNumField + R"()?)"
        + R"()(?=[ \t]|$))",
        std::regex::ECMAScript | std::regex::multiline);
    return re;
}

// Rejects matches with no cron-specific punctuation at all, e.g. a bare
// "1 2 3 4 5" that's more likely five unrelated numbers than a schedule.
bool isLikelyCron(const std::string& matched) {
    if (matched.size() >= 1 && matched[0] == '@') {
        return true;  // "@daily" etc. is unambiguous
    }
    return matched.find_first_of("*/,-") != std::string::npos;
}

}  // namespace

std::vector<Match> findMatches(const std::string& text) {
    std::vector<Match> results;

    auto begin = std::sregex_iterator(text.begin(), text.end(), cronRegex());
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        const std::smatch& m = *it;
        const std::string matched = m[1].str();
        if (!isLikelyCron(matched)) {
            continue;
        }
        results.push_back(Match{
            static_cast<size_t>(m.position(1)),
            static_cast<size_t>(m.length(1)),
        });
    }

    return results;
}

}  // namespace CronNpp
