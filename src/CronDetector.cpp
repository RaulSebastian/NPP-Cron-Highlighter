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

// Quartz(.NET)-only day-of-month specials: L, L-N, LW, NW. These don't mix
// with comma lists/steps in real Quartz expressions, so they're kept as a
// separate alternative rather than folded into kNumField.
constexpr auto kDayOfMonthSpecial = R"(L(?:-[0-9]{1,2})?W?|[0-9]{1,2}W)";

// Quartz(.NET)-only day-of-week specials: bare L (= Saturday), N/name + L
// ("the last <day> of the month"), and N/name + #M ("the Mth <day>").
constexpr auto kDayOfWeekSpecial =
    R"(L|(?:[0-9]{1,2}|[A-Za-z]{3})(?:L|#[0-9]{1,2}))";

// Quartz(.NET)-only optional trailing year field: blank/omitted, or numeric
// with the same */N/,/- combinators as the other numeric fields.
constexpr auto kYearField =
    R"((?:\*|[0-9]{1,4}(?:-[0-9]{1,4})?)(?:/[0-9]{1,4})?)"
    R"((?:,(?:\*|[0-9]{1,4}(?:-[0-9]{1,4})?)(?:/[0-9]{1,4})?)*)";

const std::regex& cronRegex() {
    // Day-of-month and day-of-week are mutually exclusive in Quartz: exactly
    // one of the two must be the literal "?". Built once (these are static
    // locals) since cronRegex() runs on every keystroke.
    static const std::string domField =
        std::string(R"((?:)") + kDayOfMonthSpecial + R"(|)" + kNumField + R"())";
    static const std::string dowField =
        std::string(R"((?:)") + kDayOfWeekSpecial + R"(|)" + kNameField + R"())";
    static const std::string domDowExclusive =
        R"(\?[ \t]+)" + std::string(kNameField) + R"([ \t]+)" + dowField + R"(|)" + domField +
        R"([ \t]+)" + std::string(kNameField) + R"([ \t]+\?)";
    static const std::string quartzPattern =
        std::string(kNumField) + R"([ \t]+)" + kNumField + R"([ \t]+)" + kNumField +
        R"([ \t]+(?:)" + domDowExclusive + R"()(?:[ \t]+)" + kYearField + R"()?)";

    // std::regex alternation takes the first branch that matches at a given
    // position, not the longest — it does not backtrack across "|" to prefer
    // a longer overall match. Quartz must therefore be tried before the
    // plain 5-field pattern: otherwise a valid 6/7-field Quartz expression
    // (e.g. "0 0 12 * * ?") gets shadowed by the 5-field branch matching
    // just its first five tokens and stopping before the "?". This is safe
    // in the other direction — Quartz structurally requires at least 6
    // whitespace-separated fields, so it can never shadow a genuine 5-field
    // match.
    static const std::regex re(
        std::string(R"(^[ \t]*()")
        + R"(@(?:yearly|annually|monthly|weekly|daily|midnight|hourly|reboot))"
        + R"(|)" + quartzPattern
        + R"(|)" + kNumField + R"([ \t]+)" + kNumField + R"([ \t]+)" + kNumField +
        R"([ \t]+)" + kNameField + R"([ \t]+)" + kNameField
        + R"()(?=[ \t]|$))",
        std::regex::ECMAScript | std::regex::multiline);
    return re;
}

// Rejects matches with no cron-specific punctuation at all, e.g. a bare
// "1 2 3 4 5" that's more likely five unrelated numbers than a schedule.
// Quartz matches always contain a literal "?" (day-of-month/day-of-week
// exclusivity is baked into the grammar above), so this filter never
// actually rejects a valid Quartz expression.
bool isLikelyCron(const std::string& matched) {
    if (matched.size() >= 1 && matched[0] == '@') {
        return true;  // "@daily" etc. is unambiguous
    }
    return matched.find_first_of("*/,-?") != std::string::npos;
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
