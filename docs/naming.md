# Names

The names of every unit, building and resource in Ringshadow, their short
forms, and the identifiers the code uses for them. The name set is
"Stellar prospectors": a mining frontier on a far world.

## The game

**Ringshadow** (RINGSHADOW in capitals, `ringshadow` as a slug). It is the huge
ringed gas giant in the game's sky: the prospectors live in its shadow. The
game was called Autocraft until 2026-10-07; code, data folders and the GitHub
repo still carry that name ([rename-ringshadow.md](rename-ringshadow.md)). The
world is **Ashfall Reach**, and the radio station is KSTR 88.7 Stardust.

## Resources

|                | Name              | Short form | Code        |
|----------------|-------------------|------------|-------------|
| Resource       | Stardust Ore      | Ore        | `ore`       |
| Map feature    | Ore deposit       | deposit    | `OreDeposit` |
| A row of them  | Ore line          |            |             |
| Resource       | Metallic Hydrogen | MH         | `hydrogen`  |
| Map feature    | MH well           | well       | `well`      |
| Building on it | Derrick           |            | `derrick`   |

Verbs: ore is **mined** (the "Drill ore" card). MH is **pumped** ("Pump
MH", "PUMPING MH"). Cargo reads "5 ore" or "4 MH". Metallic Hydrogen is a
liquid metal, like mercury.

### How they look

- **Stardust Ore**: rough nodules of dark rock. Opal shimmers run over
  them and shift colour with the view angle (teal, violet, gold), and
  tiny glints of dust sparkle on the surface. A faint glitter drifts off
  the deposit. Deposits shrink in three stages as they are mined out.
- **MH well**: a rock mound around a pool of mirror-silver liquid metal
  that heaves slowly. A cold blue-white light shines at its rim, and a
  pale vapour boils off it.
- **Derrick**: built over a well, its throat and vapour silver and cold
  blue.
- **Carried cargo**: a Prospector carries a dark ore chunk with an opal
  glint, or a silver MH canister.
- **HUD icons**: a dark nodule with an opal glint for ore, a silver
  droplet for MH.

## Units

| Name       | Role                                         | Code         |
|------------|----------------------------------------------|--------------|
| Prospector | Mines ore, pumps MH, builds, repairs         | `prospector` |
| Ranger     | Rifle infantry                               | `ranger`     |
| Comet      | Jump-pack raider with pistols, clears cliffs | `comet`      |
| Juggernaut | Heavy grenadier                              | `juggernaut` |
| Firefly    | Fast flame buggy                             | `firefly`    |
| Dropship   | Healing transport                            | `dropship`   |
| Longbow    | Tank that anchors into artillery             | `longbow`    |
| Kestrel    | Rocket gunship, hits the ground only         | `kestrel`    |
| Hailstorm  | Flak half-track, hits the air only           | `hailstorm`  |
| Peregrine  | Air-superiority fighter                      | `peregrine`  |
| Atlas      | Heavy assault walker                         | `atlas`      |
| Scorpion   | Burrowing mine                               | `scorpion`   |
| Mule       | The MMO's hauler the pilot drives (C4)       | `mule`       |

The Longbow **anchors** into artillery mode: `anchor`, `anchored`,
`unanchor`, `anchorTime`, and so on. [new-units.md](new-units.md) designs
the Peregrine, the Atlas and the Scorpion. The Mule ([haul-and-ferry.md](haul-and-ferry.md))
carries a **load** (`haul`) in its **hopper**; the pilot stands on its
**perch**; driven into a Citadel's hall it **books** the load (`Booked`).

## Creatures

The **Brood**: the alien bugs of Ashfall Reach, the MMORPG mode's enemies
([creatures.md](creatures.md)). Code `brood` for the family and its side.

| Name          | Role                                          | Code        |
|---------------|-----------------------------------------------|-------------|
| Shardling     | Ground swarmer with opal crystal quills       | `shardling` |
| Gloamwing     | Flyer that spits acid from above              | `gloamwing` |
| Lodeback      | Ore eater: fills its back sac, then leaves    | `lodeback`  |
| the Matriarch | The giant brood queen in her nest             | `matriarch` |

A Lodeback **eats** ore (not "mines"), and its sac is its **sac**. The
Matriarch is "the Matriarch" in running text; her home is the **nest**. A
group of bugs drawn to a building is a **raid**; "a swarm" in the toasts.

## Buildings

| Name      | Short form | Code        |
|-----------|------------|-------------|
| Citadel   |            | `citadel`   |
| Hab Dome  | Hab        | `habDome`   |
| Garrison  |            | `garrison`  |
| Bastion   |            | `bastion`   |
| Derrick   |            | `derrick`   |
| Foundry   |            | `foundry`   |
| Spacedock |            | `spacedock` |
| Lab       |            | `lab`       |
| Sentinel  |            | `sentinel`  |
| Gatling Gun | Gatling  | `gatling`   |
| Landing Pad | Pad      | `landingPad` |

The Kestrel, the Hailstorm and the Sentinel bring air attack and
anti-air. Their weapons are the Kestrel's **twin rockets**, the
Hailstorm's **flak** and the Sentinel's **missile pods**. In the MMO the
Sentinel is the realm's **AA tower**; the **Gatling Gun** (D2) fires red-hot
**amalgam rounds** of Stardust Ore and Metallic Hydrogen, and the Citadel
carries four **corner turrets** (D1), rapid rotary close-in guns. The
**Dropship** (F2) is the MMO's transport: it carries the Prospector between
the realm's **Landing Pads** (and the Spacedock, the big pad).

"Supply" and "rally" are ordinary RTS words and stay.

## Upgrades, abilities and weapons

| Name              | Unit       | Code                         |
|-------------------|------------|------------------------------|
| Mini gun          | Ranger     | `minigun`                    |
| Aegis shield      | Ranger     | `aegisShield`                |
| Surge             | Ranger     | `rangerSurge` (a pick)       |
| Coil rifle        | Ranger     |                              |
| Twin pistols      | Comet      |                              |
| Pulse mine        | Comet      | `cometPulseMine` (a pick)    |
| Breacher grenades | Juggernaut | `breacher`                   |
| Shock rounds      | Juggernaut | (comments only)              |
| Nova igniters     | Firefly    | `novaIgniters`               |
| Lifeline reactor  | Dropship   | `lifelineReactor`            |
| Boosters          | Dropship   | `dropshipBoosters` (a pick)  |
| Rail cannon       | Longbow    |                              |

The mini gun's sound is called `minigun` too.

Leveling picks are named after their unit: `prospectorBigHaul`,
`rangerSurge`, `juggernautBreacher`, and so on ("Every Prospector pumps
MH 20% faster").

## Map, voice and music

- **Map**: **Ashfall Reach**.
- **Base voice**: the **Oracle**. Its warnings never say its name.
- **Music**: original tracks made with ElevenLabs Music, in
  `unreal/Resources/Sounds/music/` ([music.md](music.md)). The game plays
  them plus anything the player drops into the Audio folder's `music/`.

## Sound files

The per-unit stems carry the unit's name: `v<unit>` (reports in),
`<unit>death`, `<unit>move`. The Juggernaut's grenades are `breacher`;
the Longbow's artillery sounds are `anchorshot`, `anchorhit`, `anchorset`
and `anchorlift`. Generic stems keep plain names: `gun`, `pistol`,
`flame`, `drill`, `weld`, `deposit`, `hum`, `alert`, `heal`, `cannon`,
`step`, `breath`. The manifests in `Tools/AudioGen/` follow.

## Writing the names

Unit names are capitalised everywhere, in running text too: "two
Rangers", "the Comet jumps". Comet, Ranger, Firefly, Longbow, Kestrel,
Hailstorm, Sentinel, Peregrine, Atlas and Scorpion are ordinary words, so
lower case would read as the word, not the unit.
Building names are already capitalised. Creature names too: "three
Shardlings", "a Lodeback", "the Matriarch".

In the game's own text (cards, prompts, the help line, the console and
the messages on screen) unit and building names are also highlighted:
drawn in an accent colour, so they stand out from the words around them.
