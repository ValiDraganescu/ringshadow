// The realm's defences (D1, D2; docs/mmorpg-m8.md, docs/creatures.md "What
// the defences are tuned against"): the Citadel's guns, the AA tower (the
// Sentinel) and the gatling gun, and the marks that upgrade a weapon. The
// core fires them (`Simulation::stepTurret`); the server arms the realm's
// Citadels and sells the Citadel's marks from the realm wallet, and a placed
// turret's marks from the upgrading pilot's own wallet (D3).
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
    /// marks cells; a gatling gun's air reach + `airPerMark` × its air
    /// marks (4.5 to 6 cells, never as far as the AA tower's 7: the range
    /// marks leave it be).
    static constexpr double ratePerMark = 0.2;
    static constexpr double damagePerMark = 0.25;
    static constexpr double rangePerMark = 1.0;
    static constexpr double airPerMark = 0.5;

    /// D3: a pilot upgrades a placed turret standing within `turretReach`
    /// cells of its edge (or with it under the crosshair that close).
    static constexpr double turretReach = 4.0;
    /// The kinds a pilot upgrades: the AA tower (the Sentinel) and the
    /// gatling gun.
    static bool upgradable(StructureKind kind);
    /// Whether a weapon of `kind` takes marks of `stat`: air reach only the
    /// gatling gun (the others reach the air as far as the ground).
    static bool takes(StructureKind kind, DefenceStat stat);
    /// What a placed turret's `mark`th mark (1 to `marksMax`) of `stat`
    /// costs the pilot: ore 100, 300, 800 and MH 50, 200, 500 for a
    /// gatling gun, half as much again for an AA tower.
    static ShopPrice turretPrice(StructureKind kind, DefenceStat stat, int64_t mark);
    /// Why a turret of `kind` with `marks` can't take its next mark of
    /// `stat` from a pilot holding `ore` and `hydrogen`, or nil.
    static std::optional<std::string> turretRefusal(StructureKind kind, DefenceStat stat, const TurretMarks& marks,
                                                    int64_t ore, int64_t hydrogen);

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
    /// doesn't shoot. `bonus`: as many marks more in every stat, to
    /// `marksMax` (R2: the realm's Armoury).
    static std::optional<UnitStats> weapon(const Structure& s, int64_t bonus = 0);
    /// What the Citadel's `mark`th mark (1 to `marksMax`) of `stat` costs
    /// the realm wallet: ore 250, 500, 1,000; MH 0, 50, 150.
    static ShopPrice citadelPrice(DefenceStat stat, int64_t mark);
    /// Why the Citadel can't take its next mark of `stat` with `marks` now
    /// and the realm wallet holding `ore` and `hydrogen`, or nil.
    static std::optional<std::string> citadelRefusal(DefenceStat stat, const TurretMarks& marks, int64_t ore,
                                                     int64_t hydrogen);
    /// "Fire rate", "Damage", "Range", "Air reach".
    static const char* title(DefenceStat stat);
};

} // namespace ac
