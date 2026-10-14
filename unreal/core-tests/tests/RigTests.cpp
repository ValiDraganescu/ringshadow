// The rig of modules (P7, docs/prospector-progression.md "Foundry and Lab"):
// the marks' prices and times, the refusals, fitting with the old module
// back in the stores, jobs finishing on the clock, the blocks a module
// adds, and the rig's JSON (saved apart from the progress).
#include "test.h"

#include "Progression.h"
#include "Session.h"

using namespace ac;

TEST(Rig_theMarksPricesAndTimes) {
    EXPECT_EQ(Modules::all().size(), size_t(5));
    EXPECT_EQ(Modules::ore(1), int64_t(100));
    EXPECT_EQ(Modules::hydrogen(1), int64_t(50));
    EXPECT_EQ(Modules::ore(2), int64_t(300));
    EXPECT_EQ(Modules::hydrogen(2), int64_t(200));
    EXPECT_EQ(Modules::ore(3), int64_t(800));
    EXPECT_EQ(Modules::hydrogen(3), int64_t(600));
    EXPECT_EQ(Modules::makeTime(1), 60.0);
    EXPECT_EQ(Modules::makeTime(2), 180.0);
    EXPECT_EQ(Modules::makeTime(3), 480.0);
    EXPECT_EQ(Modules::researchHydrogen(2), int64_t(300));
    EXPECT_EQ(Modules::researchTime(2), 600.0);
    EXPECT_EQ(Modules::researchHydrogen(3), int64_t(900));
    EXPECT_EQ(Modules::researchTime(3), 1800.0);
    EXPECT_EQ(Modules::share(1), 0.08);
    EXPECT_EQ(Modules::share(2), 0.16);
    EXPECT_EQ(Modules::share(3), 0.25);
    EXPECT_TRUE(Modules::effects(Module::plate, 0).empty());
    EXPECT_EQ(Modules::text(Module::drillHead, 2), std::string("Drill +16%"));
    EXPECT_EQ(Modules::text(Module::torchTip, 3), std::string("Torch damage +25%, range +0.75 cells"));
    EXPECT_EQ(Modules::markName(3), std::string("Mk III"));
}

TEST(Rig_makeResearchFitAndFinish) {
    Rig r;
    EXPECT_EQ(r.makeRefusal(Module::cargoPod, 2, 1000, 1000).value_or(""), std::string("Cargo pod Mk II needs research at the Lab first"));
    EXPECT_EQ(r.makeRefusal(Module::cargoPod, 4, 1000, 1000).value_or(""), std::string("No such mark"));
    EXPECT_TRUE(r.make(Module::cargoPod, 1, 100, 50, 10));
    EXPECT_EQ(r.makeRefusal(Module::plate, 1, 1000, 1000).value_or(""), std::string("The Foundry is busy: Cargo pod Mk I"));
    EXPECT_TRUE(r.startResearch(Module::cargoPod, 2, 300, 10));
    // Not done at 69.9; done at 70 (60 s), the Lab still at work.
    auto [made, opened] = r.finish(69.9);
    EXPECT_TRUE(!made && !opened);
    std::tie(made, opened) = r.finish(70);
    EXPECT_TRUE(made == std::optional<ModuleJob>(ModuleJob{Module::cargoPod, 1, 70}));
    EXPECT_TRUE(!opened);
    EXPECT_EQ(r.stock(Module::cargoPod, 1), int64_t(1));
    std::tie(made, opened) = r.finish(610);
    EXPECT_TRUE(opened.has_value());
    EXPECT_EQ(r.researchedMark(Module::cargoPod), int64_t(2));
    EXPECT_TRUE(!r.makeRefusal(Module::cargoPod, 2, 300, 200));
    // Fitting swaps; the rig's blocks are the fitted marks'.
    EXPECT_TRUE(r.fit(Module::cargoPod, 1));
    EXPECT_EQ(r.stock(Module::cargoPod, 1), int64_t(0));
    r.store(Module::cargoPod, 2);
    EXPECT_TRUE(r.fit(Module::cargoPod, 2));
    EXPECT_EQ(r.stock(Module::cargoPod, 1), int64_t(1));
    EXPECT_TRUE(r.effects() == Modules::effects(Module::cargoPod, 2));
    // The progress's skill boosts count the fitted survey lens.
    ProspectorProgress g;
    g.rig.fitted[Module::surveyLens] = 3;
    EXPECT_NEAR(g.chipChance(), Progression::chipChance * 1.25, 1e-15);
}

TEST(Rig_jsonRoundTripsApartFromTheProgress) {
    Rig r;
    r.fitted = {{Module::plate, 3}};
    r.store(Module::torchTip, 2, 4);
    r.researched[Module::torchTip] = 2;
    r.making = ModuleJob{Module::drillHead, 1, 12.5};
    const auto back = SessionStore::decode<Rig>(SessionStore::encode(r, false));
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(*back == r);
    EXPECT_TRUE(SessionStore::decode<Rig>("{}") == std::optional<Rig>(Rig{}));
    // The progress JSON leaves the rig out (its own column in the world's save).
    ProspectorProgress g;
    g.rig = r;
    const auto p = SessionStore::decode<ProspectorProgress>(SessionStore::encode(g, false));
    ASSERT_TRUE(p.has_value());
    EXPECT_TRUE(p->rig == Rig{});
}
