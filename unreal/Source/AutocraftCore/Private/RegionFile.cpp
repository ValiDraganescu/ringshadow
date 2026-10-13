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
constexpr int64_t maxLevel = 2;

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
    if (!r.object(root, "region", {"id", "name", "size", "base", "height", "deposits", "wells", "doodads", "pads",
                                   "spawns", "openings"}))
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
    r.list(root, "height", f.height, [&](const json& j, const std::string& w) { return r.edit(j, w); });
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
                    const json& s = (*spans)[i];
                    const std::string w = where + "[" + std::to_string(i) + "]";
                    if (!(s.is_array() && s.size() == 2 && s[0].is_number() && s[1].is_number())) {
                        r.add(w, "must be a span [from, to]");
                        continue;
                    }
                    const Span span{s[0].get<double>(), s[1].get<double>()};
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
    ordered openings = ordered::object();
    for (size_t e = 0; e < 4; e++) {
        if (f.openings[e].empty()) continue;
        ordered spans = ordered::array();
        for (const Span& s : f.openings[e]) spans.push_back(ordered::array({number(s.from), number(s.to)}));
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
    for (const Deposit& d : f.deposits) m.deposits.push_back(OreSpec{g(d.at), d.amount, d.angle * pi / 180});
    for (const WellSite& w : f.wells) m.wells.push_back(WellSpec{g(w.at), w.amount});
    for (const Place& p : f.pads) b.pads.push_back(Place{p.name, g(p.at)});
    for (const Place& p : f.spawns) b.spawns.push_back(Place{p.name, g(p.at)});
    b.openings = f.openings;
    // The base's rocks and plants give way to what the file puts down.
    std::vector<Vec2> deposits, wells, pads, spawns;
    for (const OreSpec& d : m.deposits) deposits.push_back(d.position);
    for (const WellSpec& w : m.wells) wells.push_back(w.position);
    for (const Place& p : b.pads) pads.push_back(p.at);
    for (const Place& p : b.spawns) spawns.push_back(p.at);
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
                    near(c.first, pads, r + padRadius + 0.5) || near(c.first, spawns, r + 1) || inLane(c.first, r);
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
        }
    }
    return out;
}

} // namespace ac::region
