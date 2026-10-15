#include "Hunt.h"

#include "AwayReport.h"

#include <algorithm>
#include <cmath>

namespace ac::net {

namespace {

/// "1 m 20 s", "45 s".
std::string clockLeft(double seconds) {
    const int64_t s = int64_t(std::ceil(std::max(0.0, seconds)));
    if (s < 60) return std::to_string(s) + " s";
    return std::to_string(s / 60) + " m " + std::to_string(s % 60) + " s";
}

/// "called/Slag Pockets" → {"called", "Slag Pockets"}.
std::pair<std::string, std::string> split(const std::string& id) {
    const size_t at = id.find('/');
    if (at == std::string::npos) return {id, ""};
    return {id.substr(0, at), id.substr(at + 1)};
}

} // namespace

int64_t huntThere(const HuntMessage& m) {
    int64_t n = 0;
    for (const HuntHunter& h : m.party) n += h.there && !h.down ? 1 : 0;
    return n;
}

std::string huntLine(const HuntMessage& m, double now) {
    const std::string at = m.regionName.empty() ? std::string("her nest") : m.regionName;
    switch (m.state) {
    case HuntMessage::State::none: return m.refusal.empty() ? "Not called today: any pilot may call it" : m.refusal;
    case HuntMessage::State::gathering: {
        std::string line = "Gathering at the ring in " + at + ": " + std::to_string(huntThere(m)) + " of " + std::to_string(m.party.size()) + " there";
        if (now < m.gatherEnds) line += ", " + clockLeft(m.gatherEnds - now) + " left";
        else line += ", waiting for " + (m.caller.empty() ? std::string("the caller") : m.caller) + " at the ring";
        return line;
    }
    case HuntMessage::State::assault: return "The assault on " + at + ": " + std::to_string(huntThere(m)) + " of the party in the fight";
    case HuntMessage::State::won: return "She fell to the hunt " + (m.caller.empty() ? std::string("the realm") : m.caller) + " called";
    case HuntMessage::State::failed: return "The hunt failed: she stands";
    }
    return "";
}

std::string huntNewsTitle(const std::string& id) {
    const std::string kind = split(id).first;
    if (kind == "called") return "THE HUNT IS CALLED";
    if (kind == "gathered") return "THE HUNT IS GATHERED";
    if (kind == "charge") return "THE HUNT GOES IN";
    if (kind == "won") return "THE HUNT BROUGHT HER DOWN";
    if (kind == "failed") return "THE HUNT FAILED";
    return "THE HUNT";
}

std::string huntNewsLine(const std::string& id, const std::string& by, int64_t value) {
    const auto [kind, where] = split(id);
    const std::string at = where.empty() ? std::string("her nest") : where;
    const std::string who = by.empty() ? std::string("A pilot") : by;
    if (kind == "called") return who + " called it: defenders and guards to the ring in " + at;
    if (kind == "gathered")
        return std::to_string(value) + (value == 1 ? " hunter" : " hunters") + " at the ring in " + at + ", waiting for " + (by.empty() ? "the caller" : by);
    if (kind == "charge") return std::to_string(value) + (value == 1 ? " hunter goes" : " hunters go") + " in at " + at;
    if (kind == "won") return by.empty() ? "The Matriarch is down" : "The Matriarch is down, " + by + " hardest";
    if (kind == "failed") return value > 0 ? "The party fell: she stands" : "Ten minutes and she stands";
    return "";
}

} // namespace ac::net
