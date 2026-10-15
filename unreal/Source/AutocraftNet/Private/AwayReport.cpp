#include "AwayReport.h"

#include "Standings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#if defined(__APPLE__) || defined(__linux__)
#include <sys/stat.h>
#endif

namespace ac::net {

namespace {

/// How telling a row is: lower first.
int rank(AwayKind k) {
    switch (k) {
    case AwayKind::merc: return 0;
    case AwayKind::fell: return 1;
    case AwayKind::held:
    case AwayKind::fallen: return 2;
    case AwayKind::queenDown:
    case AwayKind::queenBack:
    case AwayKind::hunt: return 3;
    case AwayKind::frontier:
    case AwayKind::week: return 4;
    case AwayKind::projectStart:
    case AwayKind::projectEnd: return 5;
    case AwayKind::raid: return 6;
    case AwayKind::waves: return 7;
    }
    return 8;
}

std::string in(const std::string& where) { return where.empty() ? std::string() : " in " + where; }

} // namespace

std::string awayNumber(int64_t n) {
    const bool minus = n < 0;
    std::string digits = std::to_string(minus ? -n : n);
    std::string out;
    for (size_t i = 0; i < digits.size(); i++) {
        if (i > 0 && (digits.size() - i) % 3 == 0) out += ',';
        out += digits[i];
    }
    return minus ? "-" + out : out;
}

std::string awayRan(double seconds) {
    const int64_t m = int64_t(std::floor(std::max(0.0, seconds) / 60.0));
    if (m < 1) return "under a minute";
    const int64_t h = m / 60;
    if (h == 0) return std::to_string(m) + " m";
    const int64_t d = h / 24;
    if (d == 0) return std::to_string(h) + " h " + std::to_string(m % 60) + " m";
    return std::to_string(d) + " d " + std::to_string(h % 24) + " h";
}

std::string awayWhen(const AwayReport& report, const AwayRow& row) { return awayRan(report.back - row.at) + " ago"; }

std::string awayLine(const AwayRow& r) {
    switch (r.kind) {
    case AwayKind::merc: return r.who + " delivered " + awayNumber(r.value) + " ore";
    case AwayKind::frontier: return r.what;
    case AwayKind::raid: return "A raid hit the " + r.what + in(r.where);
    case AwayKind::fell: return "A raid took the " + r.what + in(r.where);
    case AwayKind::held: return r.what + " held";
    case AwayKind::fallen: return r.what + " fell to the Brood";
    case AwayKind::waves:
        return r.what + ": " + std::to_string(r.value) + (r.value == 1 ? " wave beaten" : " waves beaten");
    case AwayKind::queenDown: return r.who.empty() ? "The Matriarch went down" : "The Matriarch went down, " + r.who + " hardest";
    case AwayKind::queenBack: return "The Matriarch is back in her nest";
    case AwayKind::projectStart: return (r.who.empty() ? std::string("The realm") : r.who) + " started " + r.what;
    case AwayKind::projectEnd: return r.what + " ran its course";
    case AwayKind::week: return weekTopLine(r.who, r.value);
    case AwayKind::hunt:
        return (r.who.empty() ? std::string("A") : r.who + "'s") + " hunt" + in(r.where) + (r.what == "won" ? " brought her down" : " failed: she stands");
    }
    return r.what;
}

std::vector<AwayRow> awayOrdered(const std::vector<AwayRow>& rows) {
    std::vector<AwayRow> out = rows;
    std::stable_sort(out.begin(), out.end(), [](const AwayRow& a, const AwayRow& b) {
        if (rank(a.kind) != rank(b.kind)) return rank(a.kind) < rank(b.kind);
        if (a.kind == AwayKind::merc && a.value != b.value) return a.value > b.value;
        return a.at > b.at;
    });
    return out;
}

std::array<std::string, 2> awaySummary(const AwayReport& report, size_t most) {
    std::array<std::string, 2> out;
    if (report.rows.empty() && report.realmOre == 0 && report.realmHydrogen == 0) return out;
    out[0] = "Your world ran " + awayRan(report.ran) + ".";
    const std::vector<AwayRow> rows = awayOrdered(report.rows);
    for (size_t i = 0; i < rows.size() && i < most; i++) out[1] += (i ? " · " : "") + awayLine(rows[i]);
    if (rows.size() > most) out[1] += " · " + std::to_string(rows.size() - most) + " more";
    if (rows.empty()) out[1] = "The realm's wallet " + std::string(report.realmOre >= 0 ? "+" : "") + awayNumber(report.realmOre) + " ore";
    return out;
}

std::string awayVoice(const AwayReport& report) {
    bool mercs = false, held = false, lost = false;
    for (const AwayRow& r : report.rows) {
        mercs = mercs || (r.kind == AwayKind::merc && r.value > 0);
        held = held || r.kind == AwayKind::held;
        lost = lost || r.kind == AwayKind::fell || r.kind == AwayKind::fallen || r.kind == AwayKind::queenBack;
    }
    if (lost) return "welcome-brood";
    if (held) return "welcome-held";
    if (mercs) return "welcome-mercs";
    return {};
}

namespace {

std::string flat(std::string s) {
    for (char& c : s)
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return s;
}

} // namespace

std::string awayFilePath(const std::string& save) {
    static const std::string ext = ".sqlite";
    if (save.size() > ext.size() && save.compare(save.size() - ext.size(), ext.size(), ext) == 0)
        return save.substr(0, save.size() - ext.size()) + ".away";
    return save + ".away";
}

std::string encodeAwayFile(const AwaySummaries& summaries) {
    std::string out;
    for (const auto& [key, lines] : summaries) out += flat(key) + "\t" + flat(lines[0]) + "\t" + flat(lines[1]) + "\n";
    return out;
}

AwaySummaries decodeAwayFile(const std::string& text) {
    AwaySummaries out;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        const size_t a = line.find('\t');
        if (a == std::string::npos) continue;
        const size_t b = line.find('\t', a + 1);
        if (b == std::string::npos) continue;
        out[line.substr(0, a)] = {line.substr(a + 1, b - a - 1), line.substr(b + 1)};
    }
    return out;
}

bool writeAwayFile(const std::string& path, const AwaySummaries& summaries) {
    const std::string aside = path + ".new";
    {
        std::ofstream f(aside, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f << encodeAwayFile(summaries);
        if (!f.good()) return false;
    }
#if defined(__APPLE__) || defined(__linux__)
    chmod(aside.c_str(), 0600);
#endif
    if (std::rename(aside.c_str(), path.c_str()) != 0) {
        std::remove(aside.c_str());
        return false;
    }
    return true;
}

std::optional<std::array<std::string, 2>> readAwayFile(const std::string& path, const std::string& key) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    std::stringstream text;
    text << f.rdbuf();
    const AwaySummaries all = decodeAwayFile(text.str());
    const auto it = all.find(key);
    if (it == all.end()) return std::nullopt;
    return it->second;
}

} // namespace ac::net
