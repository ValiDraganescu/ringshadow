// Blueprint sites in region files (AutocraftCore RegionFile.h, the `sites`
// layer; docs/regions.md, docs/prospector-progression.md "Blueprint
// sites"), and The First Reach's sites. Isolated: a map built and looked
// at, nothing stepped. Run from unreal/server.

#include "test.h"

#include "RegionFile.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace ac;
using namespace ac::region;

/// A clean flat field (no resources, no rocks) with `layers` added.
std::string field(const std::string& layers = "") {
    return R"({"id": "t", "name": "T", "size": "medium", "base": {"style": "openField", "seed": 1, "drop": ["resources", "doodads"]})" +
           (layers.empty() ? std::string() : ", " + layers) + "}";
}

RegionFile parsedFile(const std::string& text) {
    const Parsed p = parse(text);
    for (const Problem& q : p.problems) std::printf("    unexpected: %s\n", q.text().c_str());
    return p.file ? *p.file : RegionFile{};
}

std::vector<Problem> problemsIn(const std::string& text) {
    const RegionFile f = parsedFile(text);
    return validate(f, build(f));
}

/// A problem whose place holds `where` and whose message holds `what`.
bool names(const std::vector<Problem>& problems, const std::string& where, const std::string& what) {
    for (const Problem& p : problems)
        if (p.where.find(where) != std::string::npos && p.message.find(what) != std::string::npos) return true;
    std::printf("    no \"%s\" / \"%s\" among:\n", where.c_str(), what.c_str());
    for (const Problem& p : problems) std::printf("      %s\n", p.text().c_str());
    return false;
}

/// One site, as the layer's JSON.
std::string site(const std::string& kind, double x, double z, const std::string& blueprint = "derrick",
                 const std::string& name = "S") {
    char buf[200];
    std::snprintf(buf, sizeof buf, R"({"name": "%s", "kind": "%s", "at": [%g, %g], "blueprint": "%s"})", name.c_str(),
                  kind.c_str(), x, z, blueprint.c_str());
    return buf;
}

std::string sites(const std::string& list) { return R"("sites": [)" + list + "]"; }

/// A level-2 pocket (cliffs all round) at (100, 30), radius 6.
const std::string pocket = R"("height": [{"op": "raise", "area": {"circle": [100, 30, 6]}, "level": 2}])";

std::string readFile(const std::string& path) {
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

} // namespace

TEST(RegionSites_roundTrip) {
    const std::string text = field(sites(site("crate", 40, 30, "citadel", "Landing crate") + ", " +
                                         site("vault", 100.5, 30, "habDome", "High vault")) +
                                   R"(, "pads": [{"name": "Dock", "at": [72, 48]}])");
    const Parsed first = parse(text);
    ASSERT_TRUE(first.file.has_value());
    const RegionFile& f = *first.file;
    ASSERT_TRUE(f.sites.size() == size_t(2));
    EXPECT_EQ(f.sites[0].name, std::string("Landing crate"));
    EXPECT_TRUE(f.sites[0].kind == SiteKind::crate);
    EXPECT_TRUE(f.sites[0].blueprint == StructureKind::citadel);
    EXPECT_TRUE(f.sites[1].kind == SiteKind::vault);
    EXPECT_TRUE(f.sites[1].blueprint == StructureKind::habDome);
    EXPECT_TRUE(f.sites[1].at == Vec2(100.5, 30));
    // Written back, it reads the same and writes the same text again; the
    // layer sits after the spawns, in the reference's key order.
    const std::string written = write(f);
    EXPECT_TRUE(written.find("\"sites\"") > written.find("\"pads\""));
    EXPECT_TRUE(written.find(R"({"name": "Landing crate", "kind": "crate", "at": [40, 30], "blueprint": "citadel"})") !=
                std::string::npos);
    const Parsed again = parse(written);
    ASSERT_TRUE(again.file.has_value());
    EXPECT_TRUE(*again.file == f);
    EXPECT_EQ(write(*again.file), written);
    // No sites, no layer.
    EXPECT_TRUE(write(parsedFile(field())).find("sites") == std::string::npos);
    // The build puts them beside the pads, in ground coordinates.
    const Built b = build(f);
    ASSERT_TRUE(b.sites.size() == size_t(2));
    EXPECT_TRUE(b.sites[0].at == b.ground(Vec2(40, 30)));
    EXPECT_TRUE(b.sites[1].blueprint == StructureKind::habDome);
}

TEST(RegionSites_parseProblems) {
    const auto p = [](const std::string& layers) { return parse(field(layers)).problems; };
    EXPECT_TRUE(names(p(sites(site("chest", 40, 30))), "sites[0].kind", "one of crate, wreck, cache, vault"));
    EXPECT_TRUE(names(p(sites(site("crate", 40, 30, "hangar"))), "sites[0].blueprint", "one of citadel, habDome, bastion"));
    EXPECT_TRUE(names(p(sites(site("crate", 40, 30, "garrison"))), "sites[0].blueprint", "held back"));
    EXPECT_TRUE(names(p(sites(site("crate", 150, 30))), "sites[0].at", "out of bounds"));
    EXPECT_TRUE(names(p(sites(site("crate", 40, 30, "derrick", ""))), "sites[0].name", "must not be empty"));
    EXPECT_TRUE(names(p(sites(R"({"name": "S", "kind": "crate", "at": [40, 30]})")), "sites[0]", "missing \"blueprint\""));
    EXPECT_TRUE(names(p(sites(R"({"name": "S", "kind": "crate", "at": [40, 30], "blueprint": "lab", "blue": 1})")),
                      "sites[0]", "unknown key \"blue\""));
    EXPECT_TRUE(names(p(sites(site("crate", 40, 30, "lab", "A") + ", " + site("wreck", 60, 30, "lab", "A"))), "sites[1].name",
                      "used twice"));
    EXPECT_TRUE(names(p(R"("sites": {})"), "sites", "must be a list"));
    EXPECT_TRUE(names(p(R"("sitess": [])"), "region", "did you mean \"sites\"?"));
    EXPECT_TRUE(p(sites(site("wreck", 40, 30, "spacedock"))).empty());
}

TEST(RegionSites_validationProblems) {
    // On a cliff face: the pocket's edge.
    EXPECT_TRUE(names(problemsIn(field(pocket + ", " + sites(site("wreck", 106, 30)))), "sites[0] \"S\"", "on a cliff face"));
    // In rock.
    EXPECT_TRUE(names(problemsIn(field(sites(site("crate", 40, 30)) + R"(, "doodads": [{"kind": "boulder", "at": [41, 30]}])")),
                      "sites[0]", "in rock: a boulder"));
    // On ore, on a well.
    EXPECT_TRUE(names(problemsIn(field(sites(site("cache", 40, 30)) + R"(, "deposits": [{"at": [41, 30], "amount": 900}])")),
                      "sites[0]", "on top of deposits[0]"));
    EXPECT_TRUE(names(problemsIn(field(sites(site("wreck", 40, 30)) + R"(, "wells": [{"at": [42, 31], "amount": 900}])")),
                      "sites[0]", "on top of wells[0]"));
    // On a pad's plate.
    EXPECT_TRUE(names(problemsIn(field(sites(site("crate", 76, 48)) + R"(, "pads": [{"name": "Dock", "at": [72, 48]}])")),
                      "sites[0]", "on pad \"Dock\"'s plate"));
    // On another site.
    EXPECT_TRUE(names(problemsIn(field(sites(site("crate", 40, 30, "lab", "A") + ", " + site("crate", 42, 30, "lab", "B")))),
                      "sites[1] \"B\"", "on top of sites[0] \"A\""));
    // Cut off: inside a ring of ridge with no way in. Only a vault may sit
    // where only Cliff hop reaches; any other kind is reported.
    EXPECT_TRUE(names(problemsIn(field(pocket + ", " + sites(site("wreck", 100, 30)))), "sites[0]", "only Cliff hop reaches it"));
    // A vault stands on high ground.
    EXPECT_TRUE(names(problemsIn(field(sites(site("vault", 40, 30)))), "sites[0]", "a vault stands on high ground"));
    // A vault walled in by rocks on every side: no ramp, no cliff to hop.
    std::string rocks;
    for (int i = 0; i < 24; i++) {
        char buf[96];
        const double a = 2 * 3.14159265358979 * i / 24;
        std::snprintf(buf, sizeof buf, R"(%s{"kind": "boulder", "at": [%.2f, %.2f], "scale": 1.6})", i ? ", " : "",
                      100 + 9 * std::cos(a), 30 + 9 * std::sin(a));
        rocks += buf;
    }
    EXPECT_TRUE(names(problemsIn(field(R"("height": [{"op": "raise", "area": {"circle": [100, 30, 6]}, "level": 1}], )" +
                                       sites(site("vault", 100, 30)) + R"(, "doodads": [)" + rocks + "]")),
                      "sites[0]", "cut off: no ramp leads up to it"));
    // Clean: a crate on the open ground, and vaults on high ground.
    EXPECT_TRUE(problemsIn(field(sites(site("crate", 40, 30)))).empty());
    EXPECT_TRUE(problemsIn(field(pocket + ", " + sites(site("vault", 100, 30)))).empty());
}

TEST(RegionSites_reach) {
    // On foot; up a ramp to a pocket; only by a cliff hop to a walled one.
    const std::string ramped = R"("height": [{"op": "raise", "area": {"circle": [40, 30, 6]}, "level": 1},
                                             {"op": "ramp", "from": [40, 46], "to": [40, 35], "width": 4, "fromLevel": 0, "level": 1},
                                             {"op": "raise", "area": {"circle": [100, 30, 6]}, "level": 1}])";
    const RegionFile f = parsedFile(field(ramped + ", " + sites(site("crate", 72, 70, "lab", "A") + ", " +
                                                               site("vault", 40, 29, "lab", "B") + ", " +
                                                               site("vault", 100, 30, "lab", "C"))));
    const Built b = build(f);
    const auto problems = validate(f, b);
    for (const Problem& p : problems) std::printf("    unexpected: %s\n", p.text().c_str());
    EXPECT_TRUE(problems.empty());
    const std::vector<SiteReach> r = siteReach(b);
    ASSERT_TRUE(r.size() == size_t(3));
    EXPECT_TRUE(r[0] == SiteReach::walk);
    EXPECT_TRUE(r[1] == SiteReach::ramp);
    EXPECT_TRUE(r[2] == SiteReach::cliffHop);
    EXPECT_EQ(siteReachName(SiteReach::cliffHop), std::string_view("by a cliff hop"));
}

// The First Reach (worlds/first.json): 15 sites, one Citadel crate and two
// sites for every other blueprint, every region clean; the Citadel's crate
// by Prospect Landing's pad, the Derrick's wreck by its wells in sight of it.
TEST(RegionSites_firstReach) {
    const std::vector<std::string> regions{"start", "brine-pans", "rust-hollow", "tern-ridge", "cold-fork",
                                           "dust-terraces", "glass-flats", "ashen-shelf", "slag-pockets"};
    std::map<StructureKind, int> count;
    size_t all = 0;
    for (const std::string& id : regions) {
        const RegionFile f = parsedFile(readFile("regions/" + id + ".json"));
        EXPECT_EQ(f.id, id);
        const Built b = build(f);
        const auto problems = validate(f, b);
        for (const Problem& p : problems) std::printf("    %s: %s\n", id.c_str(), p.text().c_str());
        EXPECT_TRUE(problems.empty());
        const auto reach = siteReach(b);
        for (size_t i = 0; i < f.sites.size(); i++) {
            count[f.sites[i].blueprint] += 1;
            EXPECT_TRUE(reach[i] != SiteReach::none);
            if (f.sites[i].kind != SiteKind::vault) EXPECT_TRUE(reach[i] != SiteReach::cliffHop);
        }
        all += f.sites.size();
    }
    EXPECT_EQ(all, size_t(15));
    EXPECT_EQ(count[StructureKind::citadel], 1);
    for (StructureKind k : {StructureKind::derrick, StructureKind::habDome, StructureKind::foundry, StructureKind::lab,
                            StructureKind::spacedock, StructureKind::bastion, StructureKind::sentinel})
        EXPECT_EQ(count[k], 2);
    EXPECT_EQ(count[StructureKind::garrison], 0);

    const RegionFile start = parsedFile(readFile("regions/start.json"));
    ASSERT_TRUE(start.pads.size() == size_t(1));
    const Vec2 pad = start.pads[0].at;
    std::optional<Site> crate, wreck;
    for (const Site& s : start.sites) {
        if (s.blueprint == StructureKind::citadel) crate = s;
        if (s.blueprint == StructureKind::derrick) wreck = s;
    }
    ASSERT_TRUE(crate && wreck);
    EXPECT_TRUE(crate->kind == SiteKind::crate);
    EXPECT_TRUE(distance(crate->at, pad) < 10);
    EXPECT_TRUE(wreck->kind == SiteKind::wreck);
    EXPECT_TRUE(distance(wreck->at, pad) < 32);
    double nearestWell = 1e9;
    for (const WellSite& w : start.wells) nearestWell = std::min(nearestWell, distance(w.at, wreck->at));
    EXPECT_TRUE(start.wells.size() >= 2 && nearestWell < 14);
}
