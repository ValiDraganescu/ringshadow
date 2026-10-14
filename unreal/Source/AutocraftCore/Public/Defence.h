// The realm's defences (D1, D2; docs/mmorpg-m8.md, docs/creatures.md "What
// the defences are tuned against"): the Citadel's guns, the AA tower (the
// Sentinel) and the gatling gun, and the marks that upgrade a weapon. The
// core fires them (`Simulation::stepTurret`); the server arms the realm's
// Citadels and sells the Citadel's marks from the realm wallet.
#pragma once

#include "AutocraftCoreApi.h"
#include "Progression.h"
#include "Rules.h"
#include "Types.h"

#include <cstdint>
#include <optional>

namespace ac {

struct AUTOCRAFTCORE_API Defence {
    /// Marks a weapon takes in each of fire rate, damage and range.
    static constexpr int64_t marksMax = 3;
    /// What one mark adds: shots a second × (1 + `ratePerMark` × marks),
    /// damage × (1 + `damagePerMark` × marks), range + `rangePerMark` ×
    /// marks cells (the air reach of a gatling gun with it).
    static constexpr double ratePerMark = 0.2;
    static constexpr double damagePerMark = 0.25;
    static constexpr double rangePerMark = 1.0;

    /// An armed Citadel's corner turret `i` (0 to 3: +x+y, -x+y, -x-y,
    /// +x-y): where it stands, its mount's radius, and the way it faces at
    /// rest (out from the corner).
    static constexpr double cornerInset = 2.0;
    static constexpr double cornerRadius = 0.5;
    static Vec2 cornerGun(Vec2 citadel, int i);
    static double cornerAngle(int i);

    /// `w` with `m` applied.
    static UnitStats apply(UnitStats w, const TurretMarks& m);
    /// The weapon of building `s` as it stands: a Sentinel's or a gatling
    /// gun's (`Rules::turret`), each of a Citadel's corner turrets when the
    /// server armed it (`Structure::marks` set), with its marks. Nil: it
    /// doesn't shoot.
    static std::optional<UnitStats> weapon(const Structure& s);
    /// What the Citadel's `mark`th mark (1 to `marksMax`) of `stat` costs
    /// the realm wallet: ore 250, 500, 1,000; MH 0, 50, 150.
    static ShopPrice citadelPrice(DefenceStat stat, int64_t mark);
    /// Why the Citadel can't take its next mark of `stat` with `marks` now
    /// and the realm wallet holding `ore` and `hydrogen`, or nil.
    static std::optional<std::string> citadelRefusal(DefenceStat stat, const TurretMarks& marks, int64_t ore,
                                                     int64_t hydrogen);
    /// "Fire rate", "Damage", "Range".
    static const char* title(DefenceStat stat);
};

} // namespace ac
