// C++ only: region files (RegionFile.h, docs/regions.md).
#include "RegionFile.h"

#include "NavGrid.h"
#include "ScreenConfig.h"
#include "TerrainField.h"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION 1
#endif
#include "../ThirdParty/json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numbers>
#include <set>

namespace ac::region {

namespace {

using json = nlohmann::json;
using ordered = nlohmann::ordered_json;

constexpr double pi = std::numbers::pi;

std::string num(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4g", v);
    return buf;
}

std::string pointText(Vec2 p) { return "(" + num(p.x) + ", " + num(p.y) + ")"; }

template <class E> std::string choices() {
    std::string out;
    for (std::string_view n : EnumInfo<E>::names) out += (out.empty() ? "" : ", ") + std::string(n);
    return out;
}

template <class E> std::optional<E> enumNamed(std::string_view s) {
    const auto& names = EnumInfo<E>::names;
    for (size_t i = 0; i < names.size(); i++) if (names[i] == s) return static_cast<E>(i);
    return std::nullopt;
}

/// Edits from one string to the other, for "did you mean".
size_t editDistance(std::string_view a, std::string_view b) {
    std::vector<size_t> row(b.size() + 1);
    for (size_t j = 0; j <= b.size(); j++) row[j] = j;
    for (size_t i = 1; i <= a.size(); i++) {
        size_t diag = row[0];
        row[0] = i;
        for (size_t j = 1; j <= b.size(); j++) {
            const size_t up = row[j];
            row[j] = std::min({row[j] + 1, row[j - 1] + 1, diag + (a[i - 1] == b[j - 1] ? 0 : 1)});
            diag = up;
        }
    }
    return row[b.size()];
}

/// Catches the parser's error and where it happened.
struct ErrorCatcher : nlohmann::json_sax<json> {
    size_t position = 0;
    std::string message;
    bool null() override { return true; }
    bool boolean(bool) override { return true; }
    bool number_integer(number_integer_t) override { return true; }
    bool number_unsigned(number_unsigned_t) override { return true; }
    bool number_float(number_float_t, const string_t&) override { return true; }
    bool string(string_t&) override { return true; }
    bool binary(binary_t&) override { return true; }
    bool start_object(std::size_t) override { return true; }
    bool key(string_t&) override { return true; }
    bool end_object() override { return true; }
    bool start_array(std::size_t) override { return true; }
    bool end_array() override { return true; }
    bool parse_error(std::size_t at, const std::string& token, const nlohmann::detail::exception& ex) override {
        position = at;
        message = ex.what();
        // "[json.exception.parse_error.101] parse error at line 3, column 5: syntax error ..." → the part after
        // the location.
        const size_t colon = message.find(": ", message.find("parse error"));
        if (colon != std::string::npos) message = message.substr(colon + 2);
        if (!token.empty()) message += " (near '" + token.substr(0, 20) + "')";
        return false;
    }
};

/// Reads the JSON into a `RegionFile`, noting every problem.
struct Reader {
    std::vector<Problem>& problems;
    Vec2 extent{0, 0};

    void add(const std::string& where, const std::string& message, std::optional<Vec2> at = std::nullopt) {
        problems.push_back(Problem{where, message, at});
    }

    /// Only `allowed` keys: an unknown one is named, with the nearest
    /// allowed key.
    bool object(const json& j, const std::string& where, std::initializer_list<const char*> allowed) {
        if (!j.is_object()) { add(where, "must be an object {...}"); return false; }
        for (const auto& [key, value] : j.items()) {
            bool known = false;
            for (const char* a : allowed) known = known || key == a;
            if (known) continue;
            std::string best;
            size_t bestDistance = 4;
            for (const char* a : allowed) {
                const size_t d = editDistance(key, a);
                if (d < bestDistance) { bestDistance = d; best = a; }
            }
            std::string all;
            for (const char* a : allowed) all += (all.empty() ? "" : ", ") + std::string(a);
            add(where, "unknown key \"" + key + "\"" + (best.empty() ? "" : " (did you mean \"" + best + "\"?)") +
                           "; allowed: " + all);
        }
        return true;
    }

    const json* field(const json& j, const char* key, const std::string& where, bool required) {
        const auto it = j.find(key);
        if (it == j.end() || it->is_null()) {
            if (required) add(where, "missing \"" + std::string(key) + "\"");
            return nullptr;
        }
        return &*it;
    }

    static std::string at(const std::string& where, const char* key) {
        return where.empty() ? std::string(key) : where + "." + key;
    }

    std::optional<std::string> text(const json& j, const char* key, const std::string& where, bool required) {
        const json* v = field(j, key, where, required);
        if (!v) return std::nullopt;
        if (!v->is_string()) { add(at(where, key), "must be a string"); return std::nullopt; }
        return v->get<std::string>();
    }

    std::optional<double> number(const json& j, const char* key, const std::string& where, bool required,
                                 double lo, double hi) {
        const json* v = field(j, key, where, required);
        if (!v) return std::nullopt;
        if (!v->is_number()) { add(at(where, key), "must be a number"); return std::nullopt; }
        const double d = v->get<double>();
        if (!(d >= lo && d <= hi)) {
            add(at(where, key), "is " + num(d) + ", must be " + num(lo) + "…" + num(hi));
            return std::nullopt;
        }
        return d;
    }

    std::optional<int64_t> integer(const json& j, const char* key, const std::string& where, bool required,
                                   int64_t lo, int64_t hi) {
        const json* v = field(j, key, where, required);
        if (!v) return std::nullopt;
        if (!v->is_number_integer() && !v->is_number_unsigned()) {
            add(at(where, key), "must be a whole number");
            return std::nullopt;
        }
        if (v->is_number_unsigned() && v->get<uint64_t>() > static_cast<uint64_t>(hi)) {
            add(at(where, key), "must be " + std::to_string(lo) + "…" + std::to_string(hi));
            return std::nullopt;
        }
        const int64_t i = v->get<int64_t>();
        if (i < lo || i > hi) {
            add(at(where, key), "is " + std::to_string(i) + ", must be " + std::to_string(lo) + "…" + std::to_string(hi));
            return std::nullopt;
        }
        return i;
    }

    /// `[x, z]`, inside the region.
    std::optional<Vec2> point(const json& v, const std::string& where) {
        if (!(v.is_array() && v.size() == 2 && v[0].is_number() && v[1].is_number())) {
            add(where, "must be a point [x, z]");
            return std::nullopt;
        }
        const Vec2 p(v[0].get<double>(), v[1].get<double>());
        if (!(p.x >= 0 && p.x <= extent.x && p.y >= 0 && p.y <= extent.y)) {
            add(where, "is out of bounds: the region is 0…" + num(extent.x) + " by 0…" + num(extent.y), p);
            return std::nullopt;
        }
        return p;
    }

    std::optional<Vec2> point(const json& j, const char* key, const std::string& where) {
        const json* v = field(j, key, where, true);
        return v ? point(*v, at(where, key)) : std::nullopt;
    }

    std::optional<std::vector<Vec2>> points(const json& v, const std::string& where, size_t least) {
        if (!v.is_array()) { add(where, "must be a list of points [[x, z], ...]"); return std::nullopt; }
        if (v.size() < least) {
            add(where, "needs at least " + std::to_string(least) + " points, has " + std::to_string(v.size()));
            return std::nullopt;
        }
        std::vector<Vec2> out;
        bool ok = true;
        for (size_t i = 0; i < v.size(); i++) {
            const auto p = point(v[i], where + "[" + std::to_string(i) + "]");
            if (p) out.push_back(*p); else ok = false;
        }
        return ok ? std::optional(out) : std::nullopt;
    }

    std::optional<Area> area(const json& j, const std::string& where) {
        const json* v = field(j, "area", where, true);
        if (!v) return std::nullopt;
        const std::string w = at(where, "area");
        Area a;
        if (v->is_array()) {
            const auto ps = points(*v, w, 3);
            if (!ps) return std::nullopt;
            double twice = 0;
            for (size_t i = 0; i < ps->size(); i++) {
                const Vec2 p = (*ps)[i], q = (*ps)[(i + 1) % ps->size()];
                twice += p.x * q.y - q.x * p.y;
            }
            if (!(std::fabs(twice) > 0.5)) { add(w, "encloses no ground (its corners are in a line)"); return std::nullopt; }
            a.shape = Area::Shape::polygon;
            a.points = *ps;
            return a;
        }
        if (!object(*v, w, {"rect", "circle"})) return std::nullopt;
        if (const json* r = field(*v, "rect", w, false)) {
            if (!(r->is_array() && r->size() == 4)) { add(at(w, "rect"), "must be [x0, z0, x1, z1]"); return std::nullopt; }
            const auto p = point(json::array({(*r)[0], (*r)[1]}), at(w, "rect"));
            const auto q = point(json::array({(*r)[2], (*r)[3]}), at(w, "rect"));
            if (!p || !q) return std::nullopt;
            if (!(std::fabs(p->x - q->x) >= 1 && std::fabs(p->y - q->y) >= 1)) {
                add(at(w, "rect"), "must be at least 1 cell each way", *p);
                return std::nullopt;
            }
            a.shape = Area::Shape::rect;
            a.points = {*p, *q};
            return a;
        }
        if (const json* c = field(*v, "circle", w, false)) {
            if (!(c->is_array() && c->size() == 3 && (*c)[2].is_number())) {
                add(at(w, "circle"), "must be [x, z, radius]");
                return std::nullopt;
            }
            const auto p = point(json::array({(*c)[0], (*c)[1]}), at(w, "circle"));
            const double r = (*c)[2].get<double>();
            if (!p) return std::nullopt;
            if (!(r >= 1 && r <= 200)) { add(at(w, "circle"), "radius must be 1…200", *p); return std::nullopt; }
            a.shape = Area::Shape::circle;
            a.points = {*p};
            a.radius = r;
            return a;
        }
        add(w, "must be a list of corners [[x, z], ...], {\"rect\": [x0, z0, x1, z1]} or {\"circle\": [x, z, r]}");
        return std::nullopt;
    }

    std::optional<Edit> edit(const json& j, const std::string& where) {
        if (!j.is_object()) { add(where, "must be an object {\"op\": ...}"); return std::nullopt; }
        const auto op = text(j, "op", where, true);
        if (!op) return std::nullopt;
        const auto kind = enumNamed<HeightEdit::Kind>(*op);
        if (!kind) { add(at(where, "op"), "is \"" + *op + "\"; one of " + choices<HeightEdit::Kind>()); return std::nullopt; }
        Edit e;
        e.op = *kind;
        bool ok = true;
        switch (*kind) {
        case HeightEdit::Kind::raise:
        case HeightEdit::Kind::lower:
        case HeightEdit::Kind::flatten: {
            object(j, where, {"op", "area", "level", "slope"});
            const auto a = area(j, where);
            if (a) e.area = *a; else ok = false;
            break;
        }
        case HeightEdit::Kind::ridge: {
            object(j, where, {"op", "line", "width", "level", "slope"});
            const json* l = field(j, "line", where, true);
            const auto ps = l ? points(*l, at(where, "line"), 2) : std::nullopt;
            if (ps) e.line = *ps; else ok = false;
            const auto w = number(j, "width", where, true, 0.5, 40);
            if (w) e.width = *w; else ok = false;
            break;
        }
        case HeightEdit::Kind::ramp: {
            object(j, where, {"op", "from", "to", "width", "fromLevel", "level"});
            const auto from = point(j, "from", where), to = point(j, "to", where);
            if (from && to) {
                if (!(distance(*from, *to) >= 2)) { add(where, "ramp's ends are less than 2 cells apart", *from); ok = false; }
                e.line = {*from, *to};
            } else {
                ok = false;
            }
            const auto w = number(j, "width", where, true, 2, 30);
            if (w) e.width = *w; else ok = false;
            const auto fl = integer(j, "fromLevel", where, true, 0, maxLevel);
            if (fl) e.fromLevel = *fl; else ok = false;
            break;
        }
        }
        const auto level = integer(j, "level", where, true, 0, maxLevel);
        if (level) e.level = *level; else ok = false;
        if (*kind != HeightEdit::Kind::ramp) {
            const json* s = field(j, "slope", where, false);
            if (s) {
                const auto slope = number(j, "slope", where, false, 0, 40);
                if (slope) e.slope = *slope; else ok = false;
            }
        }
        return ok ? std::optional(e) : std::nullopt;
    }

    std::optional<Place> place(const json& j, const std::string& where) {
        if (!object(j, where, {"name", "at"})) return std::nullopt;
        const auto name = text(j, "name", where, true);
        const auto at_ = point(j, "at", where);
        if (name && name->empty()) { add(at(where, "name"), "must not be empty"); return std::nullopt; }
        if (!name || !at_) return std::nullopt;
        return Place{*name, *at_};
    }

    template <class T, class F> void list(const json& root, const char* key, std::vector<T>& out, F each) {
        const json* v = field(root, key, "", false);
        if (!v) return;
        if (!v->is_array()) { add(key, "must be a list [...]"); return; }
        for (size_t i = 0; i < v->size(); i++) {
            const std::string where = std::string(key) + "[" + std::to_string(i) + "]";
            if (auto item = each((*v)[i], where)) out.push_back(*item);
        }
    }
};

/// Every point of the file's layers, for the build to clear base rocks.
bool near(Vec2 p, const std::vector<Vec2>& points, double r) {
    for (Vec2 q : points) if (distance(p, q) < r) return true;
    return false;
}

/// One line, a space after each comma and colon.
std::string inline_(const ordered& j) {
    if (j.is_object()) {
        std::string out = "{";
        for (const auto& [k, v] : j.items()) out += (out.size() > 1 ? ", " : "") + ordered(k).dump() + ": " + inline_(v);
        return out + "}";
    }
    if (j.is_array()) {
        std::string out = "[";
        for (const auto& v : j) out += (out.size() > 1 ? ", " : "") + inline_(v);
        return out + "]";
    }
    return j.dump();
}

/// JSON text: lists of points, and small objects (a deposit, a pad), on one
/// line each.
void dump(const ordered& j, std::string& out, int indent) {
    const auto flat = [](const ordered& v) {
        return v.is_array() && std::all_of(v.begin(), v.end(), [](const ordered& e) {
            return e.is_number() || (e.is_array() && std::all_of(e.begin(), e.end(), [](const ordered& n) { return n.is_number(); }));
        });
    };
    const std::string pad(static_cast<size_t>(indent + 2), ' '), end(static_cast<size_t>(indent), ' ');
    const std::string line = inline_(j);
    if (j.is_primitive() || (flat(j) && line.size() < 100) ||
        (j.is_object() && indent > 0 && line.size() < 90 && !j.contains("base"))) {
        out += line;
        return;
    }
    if (j.is_object()) {
        out += "{\n";
        size_t i = 0;
        for (const auto& [k, v] : j.items()) {
            out += pad + ordered(k).dump() + ": ";
            dump(v, out, indent + 2);
            out += ++i < j.size() ? ",\n" : "\n";
        }
        out += end + "}";
        return;
    }
    out += "[\n";
    for (size_t i = 0; i < j.size(); i++) {
        out += pad;
        dump(j[i], out, indent + 2);
        out += i + 1 < j.size() ? ",\n" : "\n";
    }
    out += end + "]";
}


/// A whole number as an integer in the JSON.
ordered number(double v) {
    if (v == std::trunc(v) && std::fabs(v) < 1e15) return ordered(static_cast<int64_t>(v));
    return ordered(v);
}

ordered pointNumbers(Vec2 p) { return ordered::array({number(p.x), number(p.y)}); }

/// How far round a site's centre its crate, wreck or vault stands, cells:
/// the base's rocks give way that far.
constexpr double siteRadius = 2.0;

/// The walking grid's view of a built region, for how a site is reached.
struct Reach {
    const TerrainField& field;
    const NavGrid& nav;
    int32_t main = -1;
    /// The main open area's cross-cliff region (`jumpRegions`), and its
    /// most common level.
    int32_t mainJump = -1;
    int64_t mainLevel = 0;

    Reach(const TerrainField& f, const NavGrid& n) : field(f), nav(n), main(n.mainRegion) {
        std::array<int64_t, maxLevel + 1> levels{};
        for (int64_t i = 0; i < nav.width * nav.height; i++) {
            if (nav.regions[static_cast<size_t>(i)] != main) continue;
            if (mainJump < 0) mainJump = nav.jumpRegions[static_cast<size_t>(i)];
            levels[static_cast<size_t>(std::clamp<int64_t>(field.level(nav.centre(i)), 0, maxLevel))] += 1;
        }
        mainLevel = std::max_element(levels.begin(), levels.end()) - levels.begin();
    }

    SiteReach of(Vec2 p) const {
        // The cell under it, or one beside it (a cell is coarser than a site).
        std::vector<int64_t> cells;
        if (const auto k = nav.index(p)) cells.push_back(*k);
        for (int i = 0; i < 8; i++) {
            const double a = 2 * pi * i / 8;
            if (const auto k = nav.index(p + Vec2(std::cos(a), std::sin(a)))) cells.push_back(*k);
        }
        bool hop = false;
        for (int64_t k : cells) {
            if (!nav.free(k) || nav.inRock(nav.centre(k))) continue;
            if (nav.regions[static_cast<size_t>(k)] == main) return field.level(p) > mainLevel ? SiteReach::ramp : SiteReach::walk;
            hop = hop || (mainJump >= 0 && nav.jumpRegions[static_cast<size_t>(k)] == mainJump);
        }
        return hop ? SiteReach::cliffHop : SiteReach::none;
    }
};

} // namespace

std::vector<Vec2> Area::outline() const {
    switch (shape) {
    case Shape::polygon: return points;
    case Shape::rect: {
        const Vec2 a = points[0], b = points[1];
        return {Vec2(min(a.x, b.x), min(a.y, b.y)), Vec2(max(a.x, b.x), min(a.y, b.y)), Vec2(max(a.x, b.x), max(a.y, b.y)),
                Vec2(min(a.x, b.x), max(a.y, b.y))};
    }
    case Shape::circle: {
        std::vector<Vec2> out;
        for (int i = 0; i < 32; i++) {
            const double a = 2 * pi * i / 32;
            out.push_back(points[0] + Vec2(std::cos(a), std::sin(a)) * radius);
        }
        return out;
    }
    }
    return points;
}

Vec2 RegionFile::extent() const {
    const MapCells c = cells(size);
    return Vec2(c.width, c.depth);
}

std::string Problem::text() const {
    return where + (at ? " at " + pointText(*at) : "") + ": " + message;
}

std::optional<PadNear> nearestPad(const std::vector<Place>& pads, Vec2 at) {
    std::optional<PadNear> best;
    for (const Place& p : pads) {
        const double d = distance(p.at, at);
        if (!best || d < best->distance) best = PadNear{p.name, d};
    }
    return best;
}

bool touchesPadPlate(const std::vector<Place>& pads, Vec2 at, double halfSide) {
    for (const Place& p : pads) {
        // The footprint's point nearest the pad's centre, inside the plate.
        const double dx = std::max(std::abs(p.at.x - at.x) - halfSide, 0.0);
        const double dy = std::max(std::abs(p.at.y - at.y) - halfSide, 0.0);
        if (std::sqrt(dx * dx + dy * dy) < padReach) return true;
    }
    return false;
}

std::string_view siteReachName(SiteReach r) {
    static constexpr std::array<std::string_view, 4> names{"on foot", "by a ramp", "by a cliff hop", "not at all"};
    return names[static_cast<size_t>(r)];
}

std::vector<SiteReach> siteReach(const Built& b) {
    const TerrainField field(b.map);
    NavGrid nav(b.map, field);
    GameState state = GameState::new_(b.map);
    state.structures.clear();
    state.units.clear();
    nav.setDynamic(state);
    const Reach reach(field, nav);
    std::vector<SiteReach> out;
    for (const Site& t : b.sites) out.push_back(reach.of(t.at));
    return out;
}

std::string_view edgeName(Edge e) {
    static constexpr std::array<std::string_view, 4> names{"north", "east", "south", "west"};
    return names[static_cast<size_t>(e)];
}

Parsed parse(std::string_view text) {
    Parsed out;
    ErrorCatcher catcher;
    if (!json::sax_parse(text.begin(), text.end(), &catcher)) {
        // The offset as line and column.
        size_t line = 1, column = 1;
        for (size_t i = 0; i + 1 < catcher.position && i < text.size(); i++) {
            if (text[i] == '\n') { line += 1; column = 1; } else { column += 1; }
        }
        out.problems.push_back(Problem{"line " + std::to_string(line) + ", column " + std::to_string(column),
                                       "not valid JSON: " + catcher.message, std::nullopt});
        return out;
    }
    const json root = json::parse(text.begin(), text.end(), nullptr, false);
    Reader r{out.problems};
    if (!r.object(root, "region", {"id", "name", "size", "base", "ground", "brood", "height", "border", "deposits", "wells",
                                   "doodads", "pads", "spawns", "sites", "nest", "openings"}))
        return out;
    RegionFile f;
    if (const auto id = r.text(root, "id", "", true)) {
        const bool slug = !id->empty() && std::all_of(id->begin(), id->end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        });
        if (slug) f.id = *id; else r.add("id", "must be a slug: a-z, 0-9 and -");
    }
    if (const auto name = r.text(root, "name", "", true)) {
        if (name->empty()) r.add("name", "must not be empty"); else f.name = *name;
    }
    if (const auto size = r.text(root, "size", "", true)) {
        if (const auto s = enumNamed<MapSize>(*size)) f.size = *s;
        else r.add("size", "is \"" + *size + "\"; one of " + choices<MapSize>());
    }
    // Without a size the positions cannot be checked: stop here.
    if (!out.problems.empty()) return out;
    r.extent = f.extent();
    if (const json* base = r.field(root, "base", "", true)) {
        if (r.object(*base, "base", {"style", "seed", "drop"})) {
            if (const auto style = r.text(*base, "style", "base", true)) {
                if (const auto s = enumNamed<MapStyle>(*style)) f.style = *s;
                else r.add("base.style", "is \"" + *style + "\"; one of " + choices<MapStyle>());
            }
            if (const json* seed = r.field(*base, "seed", "base", false)) {
                if (seed->is_number_unsigned() || (seed->is_number_integer() && seed->get<int64_t>() >= 0))
                    f.seed = seed->get<uint64_t>();
                else
                    r.add("base.seed", "must be a whole number, 0 or more");
            }
            if (const json* drop = r.field(*base, "drop", "base", false)) {
                if (!drop->is_array()) r.add("base.drop", "must be a list: \"resources\", \"doodads\"");
                else for (const auto& d : *drop) {
                    if (d == "resources") f.dropResources = true;
                    else if (d == "doodads") f.dropDoodads = true;
                    else r.add("base.drop", "has " + d.dump() + "; only \"resources\" and \"doodads\"");
                }
            }
        }
    }
    if (const auto ground = r.text(root, "ground", "", false)) {
        if (const auto g = enumNamed<Ground>(*ground)) f.ground = *g;
        else r.add("ground", "is \"" + *ground + "\"; one of " + choices<Ground>());
    }
    if (const auto brood = r.integer(root, "brood", "", false, 0, 4)) f.brood = *brood;
    r.list(root, "height", f.height, [&](const json& j, const std::string& w) { return r.edit(j, w); });
    if (const json* border = r.field(root, "border", "", false)) {
        if (r.object(*border, "border", {"outline", "level"})) {
            Border b;
            bool ok = true;
            const json* o = r.field(*border, "outline", "border", true);
            const auto ps = o ? r.points(*o, "border.outline", 3) : std::nullopt;
            if (ps) b.outline = *ps; else ok = false;
            if (const json* l = r.field(*border, "level", "border", false)) {
                (void)l;
                if (const auto level = r.integer(*border, "level", "border", false, 1, maxLevel)) b.level = *level; else ok = false;
            }
            if (ps) {
                // A plain outline: no side crosses another.
                for (size_t i = 0; ok && i < ps->size(); i++) {
                    const Vec2 a = (*ps)[i], a1 = (*ps)[(i + 1) % ps->size()];
                    for (size_t j = i + 2; j < ps->size(); j++) {
                        if (i == 0 && j + 1 == ps->size()) continue;
                        const Vec2 c = (*ps)[j], c1 = (*ps)[(j + 1) % ps->size()];
                        const auto side = [](Vec2 p, Vec2 q, Vec2 x) { return (q.x - p.x) * (x.y - p.y) - (q.y - p.y) * (x.x - p.x); };
                        const double d1 = side(a, a1, c), d2 = side(a, a1, c1), d3 = side(c, c1, a), d4 = side(c, c1, a1);
                        if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) {
                            r.add("border.outline[" + std::to_string(i) + "]",
                                  "its side crosses side " + std::to_string(j) + " of the outline", a);
                            ok = false;
                            break;
                        }
                    }
                }
            }
            if (ok) f.border = b;
        }
    }
    r.list(root, "deposits", f.deposits, [&](const json& j, const std::string& w) -> std::optional<Deposit> {
        if (!r.object(j, w, {"at", "amount", "angle"})) return std::nullopt;
        const auto at = r.point(j, "at", w);
        const auto amount = r.integer(j, "amount", w, true, 1, 100000);
        const auto angle = r.number(j, "angle", w, false, -360, 360);
        if (!at || !amount) return std::nullopt;
        return Deposit{*at, *amount, angle.value_or(0)};
    });
    r.list(root, "wells", f.wells, [&](const json& j, const std::string& w) -> std::optional<WellSite> {
        if (!r.object(j, w, {"at", "amount"})) return std::nullopt;
        const auto at = r.point(j, "at", w);
        const auto amount = r.integer(j, "amount", w, true, 1, 100000);
        if (!at || !amount) return std::nullopt;
        return WellSite{*at, *amount};
    });
    r.list(root, "doodads", f.doodads, [&](const json& j, const std::string& w) -> std::optional<Prop> {
        if (!r.object(j, w, {"kind", "at", "rotation", "scale"})) return std::nullopt;
        const auto kind = r.text(j, "kind", w, true);
        const auto at = r.point(j, "at", w);
        const auto rotation = r.number(j, "rotation", w, false, -360, 360);
        const auto scale = r.number(j, "scale", w, false, 0.25, 4);
        if (!kind || !at) return std::nullopt;
        const auto k = enumNamed<Doodad::Kind>(*kind);
        if (!k) { r.add(w + ".kind", "is \"" + *kind + "\"; one of " + choices<Doodad::Kind>(), *at); return std::nullopt; }
        return Prop{*k, *at, rotation.value_or(0), scale.value_or(1)};
    });
    r.list(root, "pads", f.pads, [&](const json& j, const std::string& w) { return r.place(j, w); });
    r.list(root, "spawns", f.spawns, [&](const json& j, const std::string& w) { return r.place(j, w); });
    r.list(root, "sites", f.sites, [&](const json& j, const std::string& w) -> std::optional<Site> {
        if (!r.object(j, w, {"name", "kind", "at", "blueprint"})) return std::nullopt;
        const auto name = r.text(j, "name", w, true);
        const auto kind = r.text(j, "kind", w, true);
        const auto at = r.point(j, "at", w);
        const auto blueprint = r.text(j, "blueprint", w, true);
        if (name && name->empty()) { r.add(w + ".name", "must not be empty"); return std::nullopt; }
        if (!name || !kind || !at || !blueprint) return std::nullopt;
        const auto k = enumNamed<SiteKind>(*kind);
        if (!k) { r.add(w + ".kind", "is \"" + *kind + "\"; one of " + choices<SiteKind>(), *at); return std::nullopt; }
        const auto b = enumNamed<StructureKind>(*blueprint);
        if (b == StructureKind::garrison) {
            r.add(w + ".blueprint", "the Garrison is held back: no blueprint in the world", *at);
            return std::nullopt;
        }
        if (!b) {
            std::string all;
            for (std::string_view n : EnumInfo<StructureKind>::names)
                if (n != "garrison") all += (all.empty() ? "" : ", ") + std::string(n);
            r.add(w + ".blueprint", "is \"" + *blueprint + "\"; one of " + all, *at);
            return std::nullopt;
        }
        return Site{*name, *k, *at, *b};
    });
    if (const json* nest = r.field(root, "nest", "", false))
        if (r.object(*nest, "nest", {"at"}))
            if (const auto at = r.point(*nest, "at", "nest")) f.nest = *at;
    {
        std::set<std::string> seen;
        for (size_t i = 0; i < f.sites.size(); i++)
            if (!seen.insert(f.sites[i].name).second)
                r.add("sites[" + std::to_string(i) + "].name", "\"" + f.sites[i].name + "\" is used twice", f.sites[i].at);
    }
    for (const auto* places : {&f.pads, &f.spawns}) {
        std::set<std::string> seen;
        for (size_t i = 0; i < places->size(); i++) {
            const Place& p = (*places)[i];
            if (seen.insert(p.name).second) continue;
            r.add(std::string(places == &f.pads ? "pads" : "spawns") + "[" + std::to_string(i) + "].name",
                  "\"" + p.name + "\" is used twice", p.at);
        }
    }
    if (const json* openings = r.field(root, "openings", "", false)) {
        if (r.object(*openings, "openings", {"north", "east", "south", "west"})) {
            for (size_t e = 0; e < 4; e++) {
                const std::string name(edgeName(static_cast<Edge>(e)));
                const json* spans = r.field(*openings, name.c_str(), "openings", false);
                if (!spans) continue;
                const std::string where = "openings." + name;
                const double length = e % 2 == 0 ? r.extent.x : r.extent.y;
                if (!spans->is_array()) { r.add(where, "must be a list of spans [[from, to], ...]"); continue; }
                for (size_t i = 0; i < spans->size(); i++) {
                    const json* s = &(*spans)[i];
                    const std::string w = where + "[" + std::to_string(i) + "]";
                    // [from, to], or {"span": [from, to], "level": n}.
                    std::optional<int64_t> level;
                    if (s->is_object()) {
                        if (!r.object(*s, w, {"span", "level"})) continue;
                        if (const json* l = r.field(*s, "level", w, false)) {
                            (void)l;
                            level = r.integer(*s, "level", w, false, 0, maxLevel);
                            if (!level) continue;
                        }
                        s = r.field(*s, "span", w, true);
                        if (!s) continue;
                    }
                    if (!(s->is_array() && s->size() == 2 && (*s)[0].is_number() && (*s)[1].is_number())) {
                        r.add(w, "must be a span [from, to] or {\"span\": [from, to], \"level\": n}");
                        continue;
                    }
                    const Span span{(*s)[0].get<double>(), (*s)[1].get<double>(), level};
                    if (!(span.from >= 0 && span.to <= length && span.from < span.to)) {
                        r.add(w, "is " + num(span.from) + "…" + num(span.to) + "; must lie in 0…" + num(length) +
                                     " along the " + name + " edge, from before to");
                        continue;
                    }
                    if (!(span.to - span.from >= 4)) { r.add(w, "is shorter than 4 cells"); continue; }
                    bool overlaps = false;
                    for (const Span& o : f.openings[e]) overlaps = overlaps || (span.from < o.to && o.from < span.to);
                    if (overlaps) { r.add(w, "overlaps another span on the " + name + " edge"); continue; }
                    f.openings[e].push_back(span);
                }
            }
        }
    }
    if (out.problems.empty()) out.file = f;
    return out;
}

std::string write(const RegionFile& f) {
    ordered root;
    root["id"] = f.id;
    root["name"] = f.name;
    root["size"] = std::string(rawValue(f.size));
    ordered base;
    base["style"] = std::string(rawValue(f.style));
    if (f.seed) base["seed"] = *f.seed;
    if (f.dropResources || f.dropDoodads) {
        ordered drop = ordered::array();
        if (f.dropResources) drop.push_back("resources");
        if (f.dropDoodads) drop.push_back("doodads");
        base["drop"] = drop;
    }
    root["base"] = base;
    if (f.ground != Ground::plain) root["ground"] = std::string(rawValue(f.ground));
    if (f.brood != 0) root["brood"] = f.brood;
    if (!f.height.empty()) {
        ordered list = ordered::array();
        for (const Edit& e : f.height) {
            ordered j;
            j["op"] = std::string(rawValue(e.op));
            switch (e.op) {
            case HeightEdit::Kind::raise:
            case HeightEdit::Kind::lower:
            case HeightEdit::Kind::flatten:
                switch (e.area.shape) {
                case Area::Shape::polygon: {
                    ordered ps = ordered::array();
                    for (Vec2 p : e.area.points) ps.push_back(pointNumbers(p));
                    j["area"] = ps;
                    break;
                }
                case Area::Shape::rect:
                    j["area"] = {{"rect", {number(e.area.points[0].x), number(e.area.points[0].y),
                                           number(e.area.points[1].x), number(e.area.points[1].y)}}};
                    break;
                case Area::Shape::circle:
                    j["area"] = {{"circle", {number(e.area.points[0].x), number(e.area.points[0].y), number(e.area.radius)}}};
                    break;
                }
                j["level"] = e.level;
                if (e.slope > 0) j["slope"] = number(e.slope);
                break;
            case HeightEdit::Kind::ridge: {
                ordered ps = ordered::array();
                for (Vec2 p : e.line) ps.push_back(pointNumbers(p));
                j["line"] = ps;
                j["width"] = number(e.width);
                j["level"] = e.level;
                if (e.slope > 0) j["slope"] = number(e.slope);
                break;
            }
            case HeightEdit::Kind::ramp:
                j["from"] = pointNumbers(e.line[0]);
                j["to"] = pointNumbers(e.line[1]);
                j["width"] = number(e.width);
                j["fromLevel"] = e.fromLevel;
                j["level"] = e.level;
                break;
            }
            list.push_back(j);
        }
        root["height"] = list;
    }
    if (f.border) {
        ordered ps = ordered::array();
        for (Vec2 p : f.border->outline) ps.push_back(pointNumbers(p));
        root["border"] = ordered{{"outline", ps}, {"level", f.border->level}};
    }
    if (!f.deposits.empty()) {
        ordered list = ordered::array();
        for (const Deposit& d : f.deposits) {
            ordered j{{"at", pointNumbers(d.at)}, {"amount", d.amount}};
            if (d.angle != 0) j["angle"] = number(d.angle);
            list.push_back(j);
        }
        root["deposits"] = list;
    }
    if (!f.wells.empty()) {
        ordered list = ordered::array();
        for (const WellSite& w : f.wells) list.push_back(ordered{{"at", pointNumbers(w.at)}, {"amount", w.amount}});
        root["wells"] = list;
    }
    if (!f.doodads.empty()) {
        ordered list = ordered::array();
        for (const Prop& d : f.doodads) {
            ordered j{{"kind", std::string(rawValue(d.kind))}, {"at", pointNumbers(d.at)}};
            if (d.rotation != 0) j["rotation"] = number(d.rotation);
            if (d.scale != 1) j["scale"] = number(d.scale);
            list.push_back(j);
        }
        root["doodads"] = list;
    }
    for (const auto& [key, places] : {std::pair{"pads", &f.pads}, std::pair{"spawns", &f.spawns}}) {
        if (places->empty()) continue;
        ordered list = ordered::array();
        for (const Place& p : *places) list.push_back(ordered{{"name", p.name}, {"at", pointNumbers(p.at)}});
        root[key] = list;
    }
    if (!f.sites.empty()) {
        ordered list = ordered::array();
        for (const Site& t : f.sites)
            list.push_back(ordered{{"name", t.name}, {"kind", std::string(rawValue(t.kind))}, {"at", pointNumbers(t.at)},
                                   {"blueprint", std::string(rawValue(t.blueprint))}});
        root["sites"] = list;
    }
    if (f.nest) root["nest"] = ordered{{"at", pointNumbers(*f.nest)}};
    ordered openings = ordered::object();
    for (size_t e = 0; e < 4; e++) {
        if (f.openings[e].empty()) continue;
        ordered spans = ordered::array();
        for (const Span& s : f.openings[e]) {
            const ordered span = ordered::array({number(s.from), number(s.to)});
            if (s.level) spans.push_back(ordered{{"span", span}, {"level", *s.level}});
            else spans.push_back(span);
        }
        openings[std::string(edgeName(static_cast<Edge>(e)))] = spans;
    }
    if (!openings.empty()) root["openings"] = openings;
    std::string out;
    dump(root, out, 0);
    return out + "\n";
}

RegionFile starter(std::string id, MapStyle style, uint64_t seed, MapSize size) {
    RegionFile f;
    f.name = id;
    f.id = std::move(id);
    f.style = style;
    f.seed = seed;
    f.size = size;
    return f;
}

Vec2 Built::extent() const { return Vec2(map.bounds.maxX - map.bounds.minX, map.bounds.maxZ - map.bounds.minZ); }
Vec2 Built::ground(Vec2 r) const { return Vec2(r.x + map.bounds.minX, r.y + map.bounds.minZ); }
Vec2 Built::regionAt(Vec2 g) const { return Vec2(g.x - map.bounds.minX, g.y - map.bounds.minZ); }

Vec2 Built::edgePoint(Edge e, double along, double depth) const {
    const Vec2 x = extent();
    switch (e) {
    case Edge::north: return Vec2(along, depth);
    case Edge::south: return Vec2(along, x.y - depth);
    case Edge::west: return Vec2(depth, along);
    case Edge::east: return Vec2(x.x - depth, along);
    }
    return Vec2(along, depth);
}

Built build(const RegionFile& f) {
    Built b;
    MapDefinition& m = b.map;
    m = WindowMaps::build(MapChoice{f.style, f.size, 2}, true, f.seed);
    m.name = f.name;
    // The layout's own version, so a changed file never meets a walking
    // grid cached for the old one.
    m.version = static_cast<int64_t>(fnv1a64(write(f)) >> 1);
    if (f.dropResources) {
        for (BaseSite& s : m.bases) { s.ore.clear(); s.wells.clear(); }
    }
    if (f.dropDoodads) m.doodads.clear();
    auto g = [&b](Vec2 r) { return b.ground(r); };
    for (const Edit& e : f.height) {
        HeightEdit h;
        h.kind = e.op;
        for (Vec2 p : e.op == HeightEdit::Kind::ridge || e.op == HeightEdit::Kind::ramp ? e.line : e.area.outline())
            h.points.push_back(g(p));
        h.level = e.level;
        h.fromLevel = e.fromLevel;
        h.width = e.width;
        h.slope = e.slope;
        m.heightEdits.push_back(h);
    }
    // W4: each opening with a level gets a flat mouth at that level, from a
    // little past the edge to `mouthDepth` in (both sides of a shared edge
    // meet exactly there), and the walls and open stretches of the edges.
    const Vec2 ext = f.extent();
    for (size_t e = 0; e < 4; e++) {
        for (const Span& s : f.openings[e]) {
            if (!s.level) continue;
            const double a0 = s.from - 2, a1 = s.to + 2, d0 = -2, d1 = mouthDepth;
            std::vector<Vec2> rect;
            switch (static_cast<Edge>(e)) {
            case Edge::north: rect = {Vec2(a0, d0), Vec2(a1, d0), Vec2(a1, d1), Vec2(a0, d1)}; break;
            case Edge::south: rect = {Vec2(a0, ext.y - d1), Vec2(a1, ext.y - d1), Vec2(a1, ext.y - d0), Vec2(a0, ext.y - d0)}; break;
            case Edge::west: rect = {Vec2(d0, a0), Vec2(d1, a0), Vec2(d1, a1), Vec2(d0, a1)}; break;
            case Edge::east: rect = {Vec2(ext.x - d1, a0), Vec2(ext.x - d0, a0), Vec2(ext.x - d0, a1), Vec2(ext.x - d1, a1)}; break;
            }
            HeightEdit h;
            h.kind = HeightEdit::Kind::flatten;
            for (Vec2 p : rect) h.points.push_back(g(p));
            h.level = *s.level;
            h.slope = mouthSlope;
            m.heightEdits.push_back(h);
        }
    }
    bool anyOpening = false;
    for (const auto& spans : f.openings) anyOpening = anyOpening || !spans.empty();
    if (f.border || anyOpening) {
        RegionEdges edges;
        if (f.border) {
            for (Vec2 p : f.border->outline) edges.outline.push_back(g(p));
            edges.level = f.border->level;
        }
        for (size_t e = 0; e < 4; e++) {
            for (const Span& s : f.openings[e]) {
                const double o = e % 2 == 0 ? m.bounds.minX : m.bounds.minZ;
                edges.gaps.push_back(RegionEdges::Gap{static_cast<int64_t>(e), o + s.from, o + s.to});
            }
        }
        m.edges = edges;
    }
    for (const Deposit& d : f.deposits) m.deposits.push_back(OreSpec{g(d.at), d.amount, d.angle * pi / 180});
    for (const WellSite& w : f.wells) m.wells.push_back(WellSpec{g(w.at), w.amount});
    for (const Place& p : f.pads) b.pads.push_back(Place{p.name, g(p.at)});
    // The foundation plates are the pads' (none under the base's starts).
    for (const Place& p : b.pads) m.plates.push_back(p.at);
    for (const Place& p : f.spawns) b.spawns.push_back(Place{p.name, g(p.at)});
    for (const Site& t : f.sites) b.sites.push_back(Site{t.name, t.kind, g(t.at), t.blueprint});
    if (f.nest) b.nest = g(*f.nest);
    b.openings = f.openings;
    // The base's rocks and plants give way to what the file puts down.
    std::vector<Vec2> deposits, wells, pads, spawns, sites;
    for (const OreSpec& d : m.deposits) deposits.push_back(d.position);
    for (const WellSpec& w : m.wells) wells.push_back(w.position);
    for (const Place& p : b.pads) pads.push_back(p.at);
    for (const Place& p : b.spawns) spawns.push_back(p.at);
    for (const Site& t : b.sites) sites.push_back(t.at);
    const auto inLane = [&](Vec2 groundPoint, double r) {
        const Vec2 q = b.regionAt(groundPoint), x = b.extent();
        for (size_t e = 0; e < 4; e++) {
            for (const Span& s : f.openings[e]) {
                const double along = e % 2 == 0 ? q.x : q.y;
                const double depth = e == 0 ? q.y : e == 1 ? x.x - q.x : e == 2 ? x.y - q.y : q.x;
                if (along > s.from - r && along < s.to + r && depth < laneDepth + 2 + r) return true;
            }
        }
        return false;
    };
    std::vector<Doodad> kept;
    for (const Doodad& d : m.doodads) {
        bool clear = false;
        for (const NavGrid::Circle& c : NavGrid::circles(d)) {
            const double r = c.second + NavGrid::clearance;
            clear = clear || near(c.first, deposits, r + 1.5) || near(c.first, wells, r + 2) ||
                    near(c.first, pads, r + padRadius + 0.5) || near(c.first, spawns, r + 1) ||
                    near(c.first, sites, r + siteRadius) || inLane(c.first, r) ||
                    (b.nest && distance(c.first, *b.nest) < r + nestMound);
        }
        if (clear) b.cleared += 1; else kept.push_back(d);
    }
    m.doodads = kept;
    for (size_t i = 0; i < f.doodads.size(); i++) {
        const Prop& p = f.doodads[i];
        Doodad d;
        d.kind = p.kind;
        d.position = g(p.at);
        d.rotation = p.rotation * pi / 180;
        d.scale = p.scale;
        d.variant = static_cast<uint32_t>(fnv1a64(f.id + "|" + std::to_string(i)));
        m.doodads.push_back(d);
    }
    // The Matriarch's nest (N3): its mound, a landmark at the file's `nest`.
    if (f.nest) {
        Doodad d;
        d.kind = Doodad::Kind::broodNest;
        d.position = g(*f.nest);
        d.variant = static_cast<uint32_t>(fnv1a64(f.id + "|nest"));
        m.doodads.push_back(d);
    }
    // The walls' rocks: boulders, rocks and spires strewn over them, on a
    // jittered grid, past the outline by more than a cliff's width.
    if (f.border) {
        const std::vector<Vec2>& outline = f.border->outline;
        int64_t k = 0;
        for (double z = wallRockSpacing / 2; z < ext.y; z += wallRockSpacing) {
            for (double x = wallRockSpacing / 2; x < ext.x; x += wallRockSpacing) {
                const uint64_t hash = fnv1a64(f.id + "|wall|" + std::to_string(k++));
                const auto unit = [hash](int shift) { return static_cast<double>((hash >> shift) & 0xffff) / 65535.0; };
                const Vec2 p(x + (unit(0) - 0.5) * wallRockSpacing * 1.3, z + (unit(16) - 0.5) * wallRockSpacing * 1.3);
                if (!(signedDistance(p, outline) > 2.5) || unit(32) < 0.4) continue;
                const double pick = unit(48);
                Doodad d;
                d.kind = pick < 0.45 ? Doodad::Kind::boulder : pick < 0.85 ? Doodad::Kind::rock : Doodad::Kind::rockSpire;
                d.position = g(p);
                d.rotation = unit(8) * 2 * pi;
                d.scale = d.kind == Doodad::Kind::rockSpire ? 0.8 + unit(24) * 0.8 : 1.0 + unit(24) * 1.4;
                d.variant = static_cast<uint32_t>(hash >> 7);
                m.doodads.push_back(d);
            }
        }
    }
    return b;
}

std::vector<Problem> validate(const RegionFile& f, const Built& b) {
    std::vector<Problem> out;
    const MapDefinition& m = b.map;
    const TerrainField field(m);
    NavGrid nav(m, field);
    GameState state = GameState::new_(m);
    // Only the ground and what is mined: no start bases in a region.
    state.structures.clear();
    state.units.clear();
    nav.setDynamic(state);
    const int32_t main = nav.mainRegion;
    auto add = [&](const std::string& where, const std::string& message, Vec2 groundPoint) {
        out.push_back(Problem{where, message, b.regionAt(groundPoint)});
    };
    const auto steep = [&](Vec2 p) {
        const double e = NavGrid::cell / 2;
        const double dx = (field.height(p + Vec2(e, 0)) - field.height(p - Vec2(e, 0))) / NavGrid::cell;
        const double dz = (field.height(p + Vec2(0, e)) - field.height(p - Vec2(0, e))) / NavGrid::cell;
        return std::sqrt(dx * dx + dz * dz) > NavGrid::maxSlope;
    };
    // Steep within a unit's reach of `p`, as the walking grid tests it.
    const auto steepNear = [&](Vec2 p) {
        const double c = NavGrid::clearance;
        for (Vec2 q : {p, p + Vec2(c, 0), p - Vec2(c, 0), p + Vec2(0, c), p - Vec2(0, c)})
            if (steep(q)) return true;
        return false;
    };
    // The doodad whose rock comes within `r` of `p`.
    const auto rock = [&](Vec2 p, double r) -> std::optional<Doodad> {
        for (const Doodad& d : m.doodads)
            for (const NavGrid::Circle& c : NavGrid::circles(d))
                if (distance(c.first, p) < c.second + r) return d;
        return std::nullopt;
    };
    const auto rockText = [&](const Doodad& d) {
        return "in rock: a " + std::string(rawValue(d.kind)) + " at " + pointText(b.regionAt(d.position));
    };
    // Open ground joined to the region's main open ground, on a ring
    // around `p`.
    const auto reachable = [&](Vec2 p, double r) {
        for (int i = 0; i < 16; i++) {
            const double a = 2 * pi * i / 16;
            const Vec2 q = p + Vec2(std::cos(a), std::sin(a)) * r;
            const auto k = nav.index(q);
            if (k && nav.free(*k) && !nav.inRock(q) && nav.regions[static_cast<size_t>(*k)] == main) return true;
        }
        return false;
    };
    // Everything mined: the base's and the file's, as (where, ground point).
    std::vector<std::pair<std::string, Vec2>> deposits, wells;
    for (const BaseSite& s : m.bases) for (const OreSpec& o : s.ore) deposits.push_back({"a base's deposit", o.position});
    for (size_t i = 0; i < m.deposits.size(); i++) deposits.push_back({"deposits[" + std::to_string(i) + "]", m.deposits[i].position});
    for (const BaseSite& s : m.bases) for (Vec2 w : s.wells) wells.push_back({"a base's well", w});
    for (size_t i = 0; i < m.wells.size(); i++) wells.push_back({"wells[" + std::to_string(i) + "]", m.wells[i].position});
    const auto crowded = [&](Vec2 p, const std::string& self, double depositGap, double wellGap) -> std::optional<std::string> {
        for (const auto& [name, q] : deposits)
            if (name != self && distance(p, q) < depositGap) return "on top of " + name + " at " + pointText(b.regionAt(q));
        for (const auto& [name, q] : wells)
            if (name != self && distance(p, q) < wellGap) return "on top of " + name + " at " + pointText(b.regionAt(q));
        return std::nullopt;
    };

    // Height edits: ramps meet their levels and can be driven.
    for (size_t i = 0; i < f.height.size(); i++) {
        const Edit& e = f.height[i];
        if (e.op != HeightEdit::Kind::ramp) continue;
        const std::string where = "height[" + std::to_string(i) + "]";
        const HeightEdit& h = m.heightEdits[i];
        const Vec2 dir = normalize(h.points[1] - h.points[0]);
        const Vec2 low = h.points[0] - dir * 1.5, high = h.points[1] + dir * 1.5;
        if (field.level(low) != e.fromLevel)
            add(where, "the ramp's \"from\" end meets level " + std::to_string(field.level(low)) + " ground, not level " +
                           std::to_string(e.fromLevel), h.points[0]);
        if (field.level(high) != e.level)
            add(where, "the ramp's \"to\" end meets level " + std::to_string(field.level(high)) + " ground, not level " +
                           std::to_string(e.level), h.points[1]);
        const double len = distance(h.points[0], h.points[1]);
        for (double s = 0; s <= len; s += 0.5) {
            const Vec2 p = h.points[0] + dir * s;
            if (!steep(p)) continue;
            add(where, "too steep to drive here: a ramp needs about 5 cells of length a level (this one is " + num(len) +
                           " for " + std::to_string(std::abs(e.level - e.fromLevel)) + ")", p);
            break;
        }
    }
    // Deposits.
    for (size_t i = 0; i < m.deposits.size(); i++) {
        const std::string where = "deposits[" + std::to_string(i) + "]";
        const OreSpec& d = m.deposits[i];
        const Vec2 axis(std::cos(d.angle), -std::sin(d.angle));
        if (steep(d.position) || steep(d.position + axis) || steep(d.position - axis)) {
            add(where, "on a cliff face", d.position);
        } else if (const auto r = rock(d.position, 1.0)) {
            add(where, rockText(*r), d.position);
        } else if (const auto c = crowded(d.position, where, 2.0, 3.0)) {
            add(where, *c, d.position);
        } else if (!reachable(d.position, 1.6) && !reachable(d.position, 2.2)) {
            add(where, "cut off: no open ground beside it joins the region's open ground", d.position);
        }
    }
    // Wells: a Derrick stands on each.
    for (size_t i = 0; i < m.wells.size(); i++) {
        const std::string where = "wells[" + std::to_string(i) + "]";
        const Vec2 w = m.wells[i].position;
        bool onCliff = steep(w);
        for (int k = 0; k < 8 && !onCliff; k++) {
            const double a = 2 * pi * k / 8;
            onCliff = steep(w + Vec2(std::cos(a), std::sin(a)) * 1.5);
        }
        if (onCliff) {
            add(where, "on a cliff face (a Derrick needs flat ground 1.5 cells round it)", w);
        } else if (const auto r = rock(w, 1.5)) {
            add(where, rockText(*r), w);
        } else if (const auto c = crowded(w, where, 3.0, 3.0)) {
            add(where, *c, w);
        } else if (!reachable(w, 2.2) && !reachable(w, 2.8)) {
            add(where, "cut off: no open ground beside it joins the region's open ground", w);
        }
    }
    // Pads: flat, clear, joined to the open ground.
    for (size_t i = 0; i < b.pads.size(); i++) {
        const std::string where = "pads[" + std::to_string(i) + "] \"" + b.pads[i].name + "\"";
        const Vec2 c = b.pads[i].at;
        std::optional<Vec2> cliff;
        double lo = std::numeric_limits<double>::infinity(), hi = -lo;
        for (double x = -padRadius; x <= padRadius; x += 0.5) {
            for (double z = -padRadius; z <= padRadius; z += 0.5) {
                if (x * x + z * z > padRadius * padRadius) continue;
                const Vec2 p = c + Vec2(x, z);
                if (!cliff && steep(p)) cliff = p;
                const double h = field.height(p);
                lo = min(lo, h);
                hi = max(hi, h);
            }
        }
        if (cliff) {
            add(where, "on a cliff: steep at " + pointText(b.regionAt(*cliff)) + "; a pad needs flat ground " +
                           num(padRadius) + " cells round it", c);
        } else if (hi - lo > 0.6) {
            add(where, "on uneven ground (heights " + num(lo) + "…" + num(hi) + "): flatten it", c);
        } else if (const auto r = rock(c, padRadius)) {
            add(where, rockText(*r), c);
        } else if (const auto k = crowded(c, "", padRadius + 1.5, padRadius + 2)) {
            add(where, *k, c);
        } else {
            const auto cell = nav.index(c);
            if (!(cell && nav.free(*cell) && nav.regions[static_cast<size_t>(*cell)] == main))
                add(where, "cut off from the region's open ground", c);
        }
    }
    // Spawn points: open ground joined to the rest.
    for (size_t i = 0; i < b.spawns.size(); i++) {
        const std::string where = "spawns[" + std::to_string(i) + "] \"" + b.spawns[i].name + "\"";
        const Vec2 p = b.spawns[i].at;
        const auto cell = nav.index(p);
        if (steepNear(p)) add(where, "on a cliff face", p);
        else if (const auto r = rock(p, NavGrid::clearance)) add(where, rockText(*r), p);
        else if (const auto c = crowded(p, "", 1.0 + NavGrid::clearance, 1.5 + NavGrid::clearance)) add(where, *c, p);
        else if (!(cell && nav.free(*cell))) add(where, "blocked ground (the map's edge?)", p);
        else if (nav.regions[static_cast<size_t>(*cell)] != main) add(where, "cut off from the region's open ground", p);
    }
    // Sites: open ground clear of ore, wells and the pads' plates, joined to
    // the rest; a vault on high ground, which a ramp or a cliff hop reaches.
    const Reach reach(field, nav);
    for (size_t i = 0; i < b.sites.size(); i++) {
        const Site& t = b.sites[i];
        const std::string where = "sites[" + std::to_string(i) + "] \"" + t.name + "\"";
        const Vec2 p = t.at;
        const bool vault = t.kind == SiteKind::vault;
        std::optional<std::string> plate, onSite;
        for (const Place& pad : b.pads)
            if (!plate && distance(pad.at, p) < padReach + 1)
                plate = "on pad \"" + pad.name + "\"'s plate: keep a site " + num(padReach + 1) + " cells from a pad's centre";
        for (size_t j = 0; j < i && !onSite; j++)
            if (distance(b.sites[j].at, p) < 2 * siteRadius)
                onSite = "on top of sites[" + std::to_string(j) + "] \"" + b.sites[j].name + "\"";
        const SiteReach how = reach.of(p);
        if (steepNear(p)) add(where, "on a cliff face", p);
        else if (const auto r = rock(p, 1.0)) add(where, rockText(*r), p);
        else if (const auto c = crowded(p, "", 1.5 + NavGrid::clearance, 2.0 + NavGrid::clearance)) add(where, *c, p);
        else if (plate) add(where, *plate, p);
        else if (onSite) add(where, *onSite, p);
        else if (vault && field.level(p) < 1)
            add(where, "a vault stands on high ground (level 1 or more); this is level " + std::to_string(field.level(p)), p);
        else if (how == SiteReach::none)
            add(where, vault ? "cut off: no ramp leads up to it and no cliff a Cliff hop jumps"
                             : "cut off from the region's open ground", p);
        else if (how == SiteReach::cliffHop && !vault)
            add(where, "cut off from the region's open ground (only Cliff hop reaches it; only a vault may sit so)", p);
    }
    // The nest (N3): on open ground joined to the rest, in a brood level 4
    // region, no pad in its horde's ring.
    if (b.nest) {
        const Vec2 p = *b.nest;
        const SiteReach how = reach.of(p);
        if (f.brood != 4) add("nest", "a nest belongs in a brood level 4 region; this one is " + std::to_string(f.brood), p);
        else if (steepNear(p)) add("nest", "on a cliff face", p);
        else if (const auto r = rock(p, 2.0)) add("nest", rockText(*r), p);
        else if (how != SiteReach::walk && how != SiteReach::ramp) add("nest", "cut off from the region's open ground", p);
        for (const Place& pad : b.pads)
            if (distance(pad.at, p) < nestClear)
                add("nest", "pad \"" + pad.name + "\" is in the nest's ring: keep pads " + num(nestClear) + " cells from it", p);
    }
    // Openings: a lane `laneDepth` in from the edge, open all along the span
    // and joined to the region's open ground.
    for (size_t e = 0; e < 4; e++) {
        const Edge edge = static_cast<Edge>(e);
        for (size_t i = 0; i < f.openings[e].size(); i++) {
            const Span& s = f.openings[e][i];
            const std::string where = "openings." + std::string(edgeName(edge)) + "[" + std::to_string(i) + "] " +
                                      num(s.from) + "…" + num(s.to);
            int blocked = 0, total = 0;
            std::optional<std::pair<Vec2, std::string>> first;
            for (double a = s.from + 0.5; a <= s.to - 0.5 + 1e-9; a += 0.5) {
                total += 1;
                const Vec2 p = b.ground(b.edgePoint(edge, a, laneDepth));
                std::string why;
                const auto cell = nav.index(p);
                // The walking grid's cells are coarser than rocks: name
                // what stands a little further off.
                const bool open = cell && nav.free(*cell) && !nav.inRock(p) && !rock(p, NavGrid::clearance);
                if (steepNear(p)) {
                    why = "a cliff";
                } else if (!open) {
                    if (const auto r = rock(p, NavGrid::clearance + 0.75)) why = "a " + std::string(rawValue(r->kind));
                    else if (const auto c = crowded(p, "", 1.75 + NavGrid::clearance, 2.25 + NavGrid::clearance)) why = *c;
                    else why = "blocked ground";
                } else if (nav.regions[static_cast<size_t>(*cell)] != main) {
                    why = "ground cut off from the region's open ground";
                }
                if (why.empty()) continue;
                blocked += 1;
                if (!first) first = {p, why};
            }
            if (first)
                add(where, "blocked at " + std::to_string(blocked) + " of " + std::to_string(total) + " points " +
                               num(laneDepth) + " cells in; first here: " + first->second, first->first);
            // W4: the border's outline lets the passage through, from the
            // edge to well inside.
            if (f.border) {
                std::optional<Vec2> shut;
                for (double a = s.from + 0.5; a <= s.to - 0.5 + 1e-9 && !shut; a += 1) {
                    for (double d = RegionEdges::edgeBand; d <= mouthDepth + 4 && !shut; d += 1) {
                        const Vec2 q = b.edgePoint(edge, a, d);
                        if (signedDistance(q, f.border->outline) > 0) shut = q;
                    }
                }
                if (shut) add(where, "the border's outline closes the passage here: draw it out to the edge round the span", b.ground(*shut));
            }
        }
    }
    // W4: the border's outline lies inside the region, and every pad, spawn
    // point and site inside it.
    if (f.border) {
        const auto inside = [&](Vec2 r) { return signedDistance(r, f.border->outline) < -1; };
        for (size_t i = 0; i < f.pads.size(); i++)
            if (!inside(f.pads[i].at)) add("pads[" + std::to_string(i) + "]", "is outside the border's outline", b.ground(f.pads[i].at));
        for (size_t i = 0; i < f.spawns.size(); i++)
            if (!inside(f.spawns[i].at)) add("spawns[" + std::to_string(i) + "]", "is outside the border's outline", b.ground(f.spawns[i].at));
        for (size_t i = 0; i < f.sites.size(); i++)
            if (!inside(f.sites[i].at)) add("sites[" + std::to_string(i) + "]", "is outside the border's outline", b.ground(f.sites[i].at));
        for (size_t i = 0; i < f.deposits.size(); i++)
            if (!inside(f.deposits[i].at)) add("deposits[" + std::to_string(i) + "]", "is outside the border's outline", b.ground(f.deposits[i].at));
        for (size_t i = 0; i < f.wells.size(); i++)
            if (!inside(f.wells[i].at)) add("wells[" + std::to_string(i) + "]", "is outside the border's outline", b.ground(f.wells[i].at));
        if (f.nest && !inside(*f.nest)) add("nest", "is outside the border's outline", b.ground(*f.nest));
    }
    return out;
}

} // namespace ac::region
