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
    /// +x-y): where it stands (on the roof deck's pylons, C1), its mount's
    /// radius, and the way it faces at rest (out from the corner).
    /// 6.2 cells from the centre along each axis with the hall
    /// (`Rules::citadelHall`; C2: on a cantilevered bracket over the corner,
    /// the trunnion 0.2 outside the walls' outer faces, so a barrel pointed
    /// down clears the tower's top edge and reaches the wall's foot), 2
    /// without.
    static double cornerInset() { return Rules::citadelHall() ? 6.2 : 2.0; }
    static constexpr double cornerRadius = 0.5;
    static Vec2 cornerGun(Vec2 citadel, int i);
    static double cornerAngle(int i);

    /// C1 (docs/citadel-interior.md section 5): a corner gun fires only with
    /// a gunner at its station. It points within `gunArcHalf` either side of
    /// `cornerAngle(i)` and `gunPitchMin` to `gunPitchMax` up from level.
    /// C2: 270 degrees in all (C1's 220 left a strip along each face's
    /// middle blind): on its outboard bracket a barrel pointing along either
    /// of its walls runs outside the wall's face, so the hull lies in the
    /// other 90 and the barrels never cross it; down to −70°, its rounds
    /// meet the ground about 3 cells out from the wall's foot (−20° left
    /// nothing nearer than about 26 cells).
    static constexpr double gunArcHalf = 135.0 * pi / 180.0;
    static constexpr double gunPitchMin = -70.0 * pi / 180.0;
    static constexpr double gunPitchMax = 50.0 * pi / 180.0;
    /// The roof deck, cells up from the Citadel's ground.
    static constexpr double deckHeight = 7.0;
    /// The turret mounts' base, on the corner towers that rise from the
    /// deck (with the hall; the 5x5's guns stand on its deck, 4.6).
    static double mountHeight() { return Rules::citadelHall() ? 9.0 : 4.6; }
    /// Where gun `i`'s gunner stands, in the hall: 4 cells from the centre
    /// along each axis, in the gun's quadrant. Without the hall there is no
    /// station (inside the solid Citadel: `man` refuses, nothing is sheltered).
    static double stationInset() { return Rules::citadelHall() ? 4.0 : 1.5; }
    static Vec2 gunStation(Vec2 citadel, int i);
    /// A pilot within this many cells of a station mans its gun.
    static constexpr double stationReach = 1.5;
    /// Gun `i`'s camera (the game's view at a gun), riding the cradle so the
    /// barrels stay in frame from `gunPitchMin` to `gunPitchMax`: the mount
    /// (its top at `mountHeight`) + R_yaw(aim) · [trunnion + R_pitch(pitch) ·
    /// eye], the trunnion (the cradle's pivot) `trunnionAlong` along the aim
    /// and `trunnionUp` up, the eye `eyeAlong` along the barrels (behind
    /// them) and `eyeUp` over them. x, y on the ground; z from the Citadel's
    /// ground. It looks along `aim` and `pitch`.
    static constexpr double trunnionAlong = 0.144, trunnionUp = 0.396;
    static constexpr double eyeAlong = -0.40, eyeUp = 0.55;
    static Vec3 gunEye(Vec2 citadel, int i, double aim, double pitch = 0);
    /// Where gun `i`'s rounds leave (C2): the barrels' end, `muzzleAlong`
    /// from the trunnion along the barrels; z from the Citadel's ground.
    static constexpr double muzzleAlong = 1.3;
    static Vec3 gunMuzzle(Vec2 citadel, int i, double aim, double pitch);
    /// A gatling gun's muzzle (C2), from the model (its barrels' axis 1.28
    /// up, the muzzle 1.58 from its centre when level): the barrels pivot
    /// (the cradle) `gatlingPivot` up and `gatlingTrunnion` along the aim,
    /// the muzzle `gatlingBarrel` past the pivot; z from its ground.
    static constexpr double gatlingPivot = 1.28, gatlingTrunnion = 0.3, gatlingBarrel = 1.28;
    /// How far its barrels dip and rise (radians from level): steep enough
    /// down to strike a bug biting at its foot.
    static constexpr double gatlingPitchMin = -60.0 * pi / 180.0;
    static constexpr double gatlingPitchMax = 70.0 * pi / 180.0;
    static Vec3 gatlingMuzzle(Vec2 at, double aim, double pitch);
    /// A Sentinel's missile pods (the model's `pods_k`): they pitch about
    /// `sentinelPivot` over its ground, `sentinelRest` up while it watches
    /// the sky. `sentinelPitch`: radians up from level onto a body `up` over
    /// its ground and `along` away (`Simulation::aimHeight`).
    static constexpr double sentinelPivot = 1.17, sentinelRest = 0.3;
    static double sentinelPitch(double along, double up);
    /// `aim` (world radians) held within gun `i`'s arc, at its hard stops
    /// (no wrap: past a stop it stays at that stop).
    static double clampAim(int i, double aim);

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
