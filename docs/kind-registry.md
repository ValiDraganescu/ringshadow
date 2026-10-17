# The kind registry: one definition per unit kind

Audit of 2026-10-18, before the MMORPG gets its next units. Adding one ordinary unit kind
today touches about 10 core files, 20 game-layer files and 3 net files, and most of the
per-kind code is a `switch` with a `default:` or a loop bounded by an enum position, so a
missed place compiles and fails silently (the gatling was never posed because the pose
table held nine kinds; a new kind would render as a Ranger, die with the Prospector's
sound, get no wreck rig and no icon). This doc is the target architecture and the fix
plan. `docs/new-units.md` keeps the design of the units themselves.

## What does not scale (2026-10-18)

Core (`unreal/Source/AutocraftCore`):

1. The wire has room for exactly 16 kinds and there are 16: `Enum<UnitKind, 4>`
   (`NetCodec.h:184,245`), a 16-bit driven mask (`:351`), `readBits(4)`
   (`NetProtocol.cpp:428,446,471`), snapshot keys `player*16 + k` (`NetSnapshot.cpp:58,708`).
   Nothing ties these to the enum's size; a 17th kind aliases on the wire.
2. Enum order carries meaning: `Unit::brood()` is `kind >= shardling` (`Types.h:1290`),
   `GoldenLevelingTests.cpp:101` and `AcPlayground.cpp:148,951` test the same bound,
   `AcNewKinds.h:22` says `>= peregrine`. Appending a player kind makes it a bug;
   inserting it renumbers the Brood on the wire. (JSON saves store names, not indexes,
   so the comment at `Rules.h:20` and `docs/new-units.md:453` are wrong: only the wire
   uses the index.)
3. Parallel arrays sized by hand: the enum, `EnumInfo<UnitKind>::names[16]`,
   `UnitStats::byKind[16]` (declared and defined, rows matched by position with a
   comment); `Perk` (216), `EnumInfo<Perk>::names[216]`, `Leveling::rows()` and
   `Leveling::built()`; `statNames[42]`; the counts repeated in `TypesTests.cpp:9` and
   `GoldenLevelingTests.cpp:99`.
4. Per-kind behaviour is an if-chain in several places, with different numbers: turret
   rates `Simulation.cpp:1673` (Firefly 6, Hailstorm 5, else 4) against `Pilot.h:109`
   (Longbow 2.4, Firefly 3.5, Hailstorm 3.2); hull turn `Simulation.cpp:3458` against
   `Pilot.h:92`; the turret set in four places (`freshUnit`, `fire`, `Pilot::steers`,
   `turretRate`); the anchoring set in about seven; Dropship cargo slots `Rules.h:257`;
   `needsLab` `Rules.h:278`; `boards` `Rules.h:254`; `vision()` `Rules.cpp:143`;
   `flight()` `Rules.cpp:9`; `Simulation::title()` `Pilot.cpp:1086` (a second copy of
   every name); `Pilot::drivable` lists the 12 player kinds by hand (`Pilot.h:76`).
5. Classification lists disagree: `Leveling::machines`/`bio` (`Types.h:532`) put the
   Prospector in `machines` while its stats row says `bio = true` (there `bio` means it
   boards a Dropship).
6. The AI's training chain (`Commander.cpp:1596-1640`) and `Counters`/`EnemyMix`
   (`Commander+Intel.members.h`) have a branch or field per kind: a new kind is never
   trained until someone writes code.

Game layer (`unreal/Source/Autocraft`):

7. Loops bounded by position: `<= scorpion` (`AcEffectsShatter.cpp:125`,
   `AcEffectsShatterSheet.cpp:77`, `AcPoseVehiclesSheet.cpp:44`), `<= hailstorm`
   (`AcPoseStill.cpp:39`, `AcEffectsDeathsSheet.cpp:47`), `< shardling`
   (`AcPlayground.cpp:148,951`), a 12-slot `IconName` (`AcConsoleInfo.cpp:36`).
8. `AcNewKinds::HasCockpit` keeps `Known[3]` indexed by `Kind - peregrine`
   (`AcNewKinds.cpp:37`): a fourth own-cockpit kind writes out of bounds.
9. Defaults that hide a miss: `ModelBase` → "ranger", `VoiceOf` → "vranger",
   `DeathOf` → "prospectordeath", `WeaponOf` → "gun", `Eye` → (0,1,0), minimap 1.8,
   `SpecOf` false. No test walks every kind.
10. Names copied instead of derived: the base name in `ModelBase`, `IconName`, the
    playground Titles, `AcEffectsBursts.cpp:55`, `SpecOf`'s model literals,
    `AcNewKinds::OwnCockpit`, `AcEffectsDeaths.cpp:123`; the title in `Simulation::title`
    and the playground; `<kind>death`, `v<kind>`, `<kind>move` spelt out; `KindNamed`
    three times with three bounds; `ChestHeight` three times; the cockpit's own-model set
    three times.
11. One kind's look is spread over about 14 files with no record: muzzles
    (`AcEffectsShots.cpp:58`), shatter (`AcEffectsShatter.cpp:33`), sounds
    (`AcAudioDirector.cpp:71-268`), cockpit eye/pivot/pitch/boom (`AcPilotCamera.cpp`),
    minimap (`SAcMinimap.cpp:123`), pilot text (`AcPilotText.cpp:130`), cockpit family
    (`AcCockpitKinds.cpp:319`, `AcNewKinds.cpp`).

MMORPG (`unreal/server`, `Progression.*`):

12. The pilot's unit is `UnitKind::prospector` in five spawns (`WorldMap.cpp:452`,
    `WorldRoles.cpp:171`, `WorldMapSave.cpp:93`, `WorldStarfall.cpp:360`,
    `WorldHunt.cpp:389`), the boarding check (`WorldMap.cpp:544`) and the save default
    (`WorldStore.h:49`). The skill tree is one table for one kind (`Progression::nodes()`,
    `ProspectorProgress`). A second playable kind has nowhere to go.

## The target

One rule: **a kind is defined once; everything else derives from the definition or is
checked against `allCases`.**

### Core: the kind table

`UnitStats::byKind` stays the one table (it is `constexpr` and the hot path indexes it).
It grows the facts that are if-chains today, and the if-chains read the table:

| field | replaces | notes |
|---|---|---|
| `std::string_view title` | `Simulation::title()` | the display name (`docs/naming.md`) |
| `bool brood` | `kind >= shardling` | `Unit::brood()` reads it; order no longer matters |
| `bool boards` | `Rules::boards()` | bio soldiers, the Prospector, the Scorpion |
| `bool machine` | `Leveling::machines` | `Leveling::machines`/`bio` become `constexpr` sets built from the table, so `covers()` and the leveling signature do not change |
| `bool drivable` | `Pilot::drivable` | the 12 player kinds |
| `int cargoSlots` | `Rules::slots()` | 1 default; Atlas 16, Juggernaut 2, Longbow 4 |
| `bool needsLab` | `Rules::needsLab()` | |
| `std::optional<StructureKind> trainedBy` | `Rules::trains()` | `trains(s)` filters the table in kind order; the row order inside each building must equal today's `trains()` lists |
| `double vision` | `Rules::vision()` | |
| `double shotFlightBase, shotFlightPerCell` | `Rules::flight()` | the anchored Longbow's figures live on `UnitStats::anchor`, so `flight()` reads the active weapon's row |
| `bool turret` | the turret set (`freshUnit`, `fire`, `steers`, `turretRate`) | |
| `double turretRateAi, turretRatePilot` | `Simulation.cpp:1673`, `Pilot.h:109` | keep today's two values; unifying them is a balance change for later |
| `bool steers` | `Pilot::steers()` | |
| `double hullTurnRateAi, hullTurnRatePilot` | `Simulation.cpp:3458`, `Pilot.h:92` | 0 means the default each site uses today |
| `bool anchors, burrows, jumps, builds` | the Longbow/Scorpion, Scorpion, Comet, Prospector tests | `planted()`, `burrowed()`, `Pilot::jumps()`, `Pilot::builds()` read them |
| `bool aiTrains` | nothing today | default false for every current kind; when true the Commander's training chain trains the kind from its building after its hand-written picks (`Rules::trains` order). No current kind changes |

Sizes derive: `EnumInfo<UnitKind>::count = names.size()`; `byKind` is
`std::array<UnitStats, EnumInfo<UnitKind>::count>`; one `static_assert` pins
`count == size_t(UnitKind::matriarch) + 1` with the comment "the last kind: move it when
you append". `Counters` and `EnemyMix` become arrays indexed by kind (`EnumArray<UnitKind,
int>`), their named accessors kept. Tests derive every count from `allCases` and a
`Leveling::perksPerKind = 18` constant; a new test `KindTableTests` walks `allCases` and
checks every row is complete (title non-empty, a player kind is drivable and trained by
a building, a Brood kind is neither).

Leveling: `rows()` and `built()` merge into one table, one row per perk (perk, kind,
title, text, effects). `EnumInfo<Perk>::names` stays, and a test asserts the four facts
agree: the names array has `allCases<Perk>` entries, every perk has exactly one row,
`kind(perk)` matches the row's kind, every player kind has exactly `perksPerKind` rows in
level order, every perk name starts with its kind's raw name. `balance()` and
`signature()` must produce the same golden (`core-tests/golden/leveling.json` does not
change).

### Net: widths from the count

`kKindBits = bitsFor(EnumInfo<UnitKind>::count)` (4 today), `Enum<UnitKind, kKindBits>`,
the driven mask `Enum<uint64_t, EnumInfo<UnitKind>::count>`, snapshot keys multiply by the
count, `readBits(kKindBits)`. A net test asserts every kind round-trips and the widths
cover the count. With 16 kinds the bytes do not change, so `kProtocolVersion` stays 35;
the doc at `NetPacket.h:30` gains the line "a new unit kind changes the widths: bump the
version".

### Game layer: one look record per kind

`AcKindTable.h/.cpp`: `struct FAcKindLook` and `const FAcKindLook& AcKinds::Look(ac::UnitKind)`,
one row per kind in `allCases` order:

- names derived, never stored: base `FName(rawValue(kind))`, model `<base>_blue`, cockpit
  model `cockpit_<base>_blue`, icon `<base>`, death sound `<base>death`, voice `v<base>`,
  move loop `<base>move`; a row may override any of them (the Kestrel's `anchorshot`, the
  Peregrine's `seekers`);
- `Muzzles` (part names), `Weapon` (sound, gain, voices), `Hover`, `ChestHeight`,
  `Eye`, `Pivot`, `StartPitch`, `Boom`, `MinimapRadius`, `MinimapZ`, `MinimapShape`,
  `Cockpit` (Own, Borrowed from kind X, StandIn), `Death` (Shatter spec or BodyFall),
  `Burst` (what `AcEffectsBursts` keys by name today), `Outline` (buried or not).

Every switch listed in 9 to 11 reads the record; `AcPose::ModelBase`, `IconName`, the
playground Titles, `KindNamed` ×3, `ChestHeight` ×3 and the literal names go. The
positional loops become `for (kind : allCases)` filtered by `stats(kind).brood` or by
`Look(kind)`. `AcNewKinds::HasCockpit` keys by kind (`TMap` or an array of `count`).

`Autocraft.Kinds.Complete` (an automation test, like `Autocraft.Gait.*`): for every kind
in `allCases`: a pose is registered, the catalog has `<base>_blue`, a cockpit model or a
declared stand-in, an icon texture, a shatter spec or a body-fall death, muzzles or an
explicit "no weapon", a death sound and a voice in `Sounds.json`. It fails naming the kind
and the field. The existing tests that list `{peregrine, atlas, scorpion}` by hand iterate
`allCases` with the Brood filtered by the table.

### MMORPG: the pilot's kind

`WorldStore` already saves the pilot's kind by name with the Prospector as the default.
One accessor `World::pilotKind(pilot)` feeds the five spawns and the boarding check
(`Rules::boards`/`drivable`, not `== prospector`); mercenaries and frontier pilots keep a
named constant `kMercenaryKind = prospector`. `Progression::nodes(UnitKind)` returns the
tree for a kind (the Prospector's today, empty for the others, so a pick on a kind without
a tree is refused as it is now); `ProspectorProgress` becomes `PilotProgress` with a
`kind` field defaulting to the Prospector and a `using ProspectorProgress = PilotProgress`
alias; the JSON blob's keys do not change and a test loads a blob written before the
change. The tree's content, the Prospector's branches and the balance do not move.

## The fix plan (2026-10-18)

Five workers in pool worktrees, non-overlapping files, merged fast-forward onto
`worktree/MMORPG-variant` as each lands:

| worker | owns | done when |
|---|---|---|
| core | `Rules.h/.cpp`, `Types.h` (kind enum, kind sets, `brood()`), `Pilot.h/.cpp`, `Pilot+Kinds.cpp`, `Simulation.cpp` per-kind sites, `Commander*.cpp/.h`, `core-tests` (`TypesTests`, `GoldenLevelingTests` counts, new `KindTableTests`), `unreal/PORTING.md` | `make -C unreal/core-tests` green, golden unchanged, no behaviour change |
| leveling | `Leveling.cpp`, `Types.h` (`Perk` enum and names only), `core-tests` leveling tests, `docs/leveling.md` "How it fits the code" | core tests green, `leveling.json` golden unchanged |
| net | `Source/AutocraftNet`, `unreal/net-tests` | `make -C unreal/net-tests` green, bytes unchanged at 16 kinds |
| game | `Source/Autocraft` and its tests, `unreal/GAME-LAYER.md` | `unreal/Tools/build.sh` green, `Autocraft.Kinds.Complete` and the existing Autocraft.* tests pass through `hidden_run.py` |
| server | `unreal/server`, `Progression.h/.cpp`, `Simulation+Progression.*`, `docs/mmorpg.md`, `docs/prospector-progression.md` | `make -C unreal/server test` green, old save blobs load |

Then `docs/new-units.md` "Where it lands in the code" is rewritten as the checklist that
follows from this doc.

## Adding a kind after this

1. Append the enum case before the Brood block and its name in `EnumInfo<UnitKind>::names`;
   move the `static_assert`'s "last kind"; bump `kProtocolVersion`.
2. One row in `UnitStats::byKind` (stats, title, flags, building, rates).
3. `perksPerKind` rows in the leveling table and their `Perk` cases and names
   (`docs/leveling.md` template); edit the golden by hand.
4. One row in `AcKindTable.cpp`; `AcPose<Kind>.cpp` with its registration; the model and
   cockpit scripts, icon, sounds (the `ringshadow-model-pipeline` skill).
5. Run the suites: `Autocraft.Kinds.Complete` and `KindTableTests` name whatever is missing.
6. A mechanic the table cannot express (burrowing, a stomp) is new code in `Simulation`,
   `Pilot+Kinds`, the effects and the Commander, as before.

## What landed (2026-10-18)

All five, fast-forward onto `worktree/MMORPG-variant`, every suite green on the merged
tree (core 521, net 105, server 356, `Autocraft.Kinds.*` and the playground self-test),
no golden and no wire byte changed: leveling `07d4e1d` (`perkRows()`,
`LevelingTableTests`), net `6938848` (`kUnitKindBits`/`kStructureKindBits` from the counts,
`KindWidthTests`), server `d28e66e` (`WorldMap::pilotKind`, `Progression::nodes(kind)`,
`PilotProgress`, `PilotKindTests`), game `442759e` (`AcKindTable`, `AcNewKinds` deleted,
`Autocraft.Kinds.Complete`), core `bb88605` (the kind table in `UnitStats::byKind`,
`EnumArray` counters, the `aiTrains` fallback, `KindTableTests`). Deliberately left as
code: the mechanics (anchor, bury, stomp, the cockpit pose functions, the per-kind help
text, the Near carry sounds). Known, not changed: `AcEffectsNewKinds.cpp` and
`AcPoseScorpion.cpp` use a flat 0.62 chest height instead of the row's (a probable bug,
kept for zero behaviour change); the AI's and the pilot's turret and hull rates still
differ (two fields, a balance decision for later); `WorldBrood.cpp`'s guard distance
still names the Prospector.
