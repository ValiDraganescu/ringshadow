# Citadel missions and the Journal

The realm's Citadel posts missions on a board. The pilot picks the ones it
wants (easy ones pay less, perilous ones more), follows them in its Journal,
and hears each one's briefing read by a voice. This is task K2 of milestone 8
([mmorpg-m8.md](mmorpg-m8.md)). KSTR's daily radio contracts
([contracts.md](contracts.md)) stay as they are: missions are a separate
system on the same counting.

## The user's words (2026-10-14)

"I also want to sign up on missions from the citadel. I pick the mission I
want: some are easier with smaller rewards, some harder with bigger rewards.
When I pick a mission I can view it in my Journal. A voice narrates the
mission for me (needs to be exciting and adventurous, grave and fearless or
fearful depending on the mission type). In my journal I can read the text,
view instructions, location on the map and I can replay the audio
narration."

## The board

- The Citadel screen (H, at the realm's Citadel, standing where you unload)
  has a third tab, **MISSIONS**, after REALM and SHOP. Tab moves through all
  three.
- The board holds **two offers a tier**, easy, hard and perilous: six in all.
  It is drawn per pilot and per world day by the server's dice, from the
  world's seed, the pilot's key and the day. A new board comes at the world's
  dawn, the same moment as the contracts.
- **Keys 1–6** (or a click on TAKE) sign up for an offer. A pilot holds at
  most **three** missions at once. A taken offer leaves the board.
- Signing up is refused away from the Citadel ("Sign up at the realm's
  Citadel: stand where you unload") and with three already taken ("Your
  Journal holds 3 missions: finish or abandon one (L)").
- When the server keeps a sign-up, the game plays a short **sting**
  (`missionaccept`), then the mission's **narration**, once.

### Tiers

A region's tier comes from its grid steps from the start region: the start
region is easy, its four neighbours hard, everything two or more steps away
perilous. On The First Reach the perilous regions are the four corners,
Brood country, Slag Pockets among them (docs/creatures.md "Brood level").
Hauls are easy or hard only; every other kind can come in any tier.

| Tier | Where | XP | Ore | MH | And |
|---|---|---|---|---|---|
| Easy | home | 8 minutes of play | 150 | 10 + 2 × level | — |
| Hard | next door | 20 minutes | 450 | 30 + 4 × level | 1 blueprint copy, or 2 supplies |
| Perilous | far corners | 40 minutes | 1,000 | 60 + 8 × level | 2 blueprint copies, or 3 supplies |

- **XP** is a number of minutes of play at the pace model's rate
  (`WorldContracts::perMinute`: 4,500 an hour at level 1, +2% a level),
  rounded to 5. At level 1 that is 600, 1,500 and 3,000.
- **Ore** rises 2% a level, rounded to 50. **MH** is rounded to 5.
- **The blueprint** is a random kind the pilot could build within ten levels
  of its own. The Citadel is never a reward.
- **Supplies** are one of the shop's four consumables, added up to the
  stack's cap.

Everything is paid into the wallet when the mission is done.

### Kinds

| Kind | Asks | Counts |
|---|---|---|
| **Haul** | unload ore (200 easy, 800 hard) or MH (40, 150) at the realm's buildings | every unload at a Citadel or Hab Dome |
| **Survey** | stand inside a 6-cell ring round a deposit, well or pad for 10 / 15 / 20 s (the scan) | seconds in the ring, driving |
| **Salvage** | open a blueprint site the pilot hasn't opened and can reach | the site opened, however |
| **Hold** | stand inside a 10-cell ring round a deposit for 15 / 20 / 25 s **while the ringshadow band covers you** | seconds in the ring under the band (cover ≥ 0.5) |
| **Hunt** | kill 3 / 6 / 12 of the Brood in a region | each kill by the pilot, reported by N2's Brood |

- **Asks** scale with the pilot's level: hauls +2% a level.
- **Survey and hold places** in the start region lie at least 30 cells from
  the start pad.
- **Salvage**: taking one puts its site on the pilot's map.
- **Hold**: a starfall does not count yet; only the sweep does.
- **Hunts are gated**: `WorldParams::huntMissions` is false until N2 brings
  the bugs, so no hunt is drawn until then. N2 calls
  `WorldMap::broodKill(pilot, kind, region)` for each kill. The templates and
  the server's count are ready for it.
- **The Matriarch's hunt** (N3): one perilous hunt in three is for her alone
  while her nest's region is in reach and she is home: titled "The
  Matriarch", "Bring down the Matriarch at her nest in Slag Pockets. Every
  pilot who hurts her shares her fall.", its marker her nest. Its target is
  `matriarch:<region>`; her fall counts it for every pilot who hurt her,
  wherever she fell. It reads the perilous hunt's briefing and narration
  (`hunt-perilous`, "Into the Nest", which speaks of her nest), so no new
  recording.

## The Journal (L)

**L** opens the Journal anywhere (J is the rig screen). It shows:

- **On the left:** the missions taken and the ones done today, each with its
  kind, tier, progress and a bar.
- **For the picked mission:**
  - its title;
  - the officer's **briefing**, word for word what the narration says;
  - **WHAT TO DO** (the numbers and names);
  - **PROGRESS**;
  - **REWARD**;
  - **WHERE**: the world's grid with the mission's region lit in the tier's
    colour, the marker and its ring inside it, and the pilot's own dot.
    Under the grid it says where that is from here: "Here, Prospect Landing:
    40 m north-east." or "In Rust Hollow: leave Prospect Landing to get
    there.". A hunt lights its whole region.
  - **PLAY THE BRIEFING** replays the narration (P, Enter or a click).
  - **ABANDON**: X, then X again, drops the mission. Nothing is paid back.

The other keys: Up and Down (or 1–3) pick a mission; L or Esc closes. A done
mission stays in the Journal, marked DONE, until the next board.

**The marker** of every taken mission with a place in the pilot's region is
also shown in the field:

- **On the minimap:** a diamond in the tier's colour (green, amber or red)
  with its ring.
- **On the compass tape:** a filled diamond in the tier's colour, labelled
  "MISSION" with the distance.

When the satellite map (M2) comes, it reads the same data: each mission's
`region`, `at` and `radius`.

When a mission is done, the game shows the toast "MISSION DONE: THE TITLE"
with the pay, and plays the paid chime.

## Templates and narration

Each mission comes from a **template**: a key, a title, a briefing and an
instruction line. They are in `missionTemplates()` in
`unreal/server/world/WorldMissions.cpp`.

- **The briefing** is what the narrator reads. It has no rolled numbers or
  names, so one recording per template serves every mission drawn from it.
- **The instruction line** carries the numbers and names: `{n}`, `{where}`
  and `{what}`.

There are 16 templates: haul ore and haul MH at easy and hard, then survey,
salvage, hold and hunt at each of the three tiers.

Three narrators, one per mood, all in the game's fiction:

| Narrator | Mood | Reads | Voice |
|---|---|---|---|
| The Citadel's **mission officer** | adventurous: upbeat, bold, a little swagger | haul, survey and salvage (easy, hard) | Charlie (deep, confident, energetic), `IKne3meq5aSn9XLyUdCD` |
| The Citadel's **Warden** | grave and fearless: low, slow, solemn | hold and hunt (easy, hard) | W. Clint Oxley (baritone, serious, raspy), `fbIG6gEosVIM95R5qOna` |
| **KSTR's night dispatcher** | afraid: shaky, breathy, on a bad radio line | every perilous one | Hazel X (warm, conversational), `YyqkX0AHv8W5D1vxG9lR` |

### Generating the files

- **Model:** ElevenLabs `eleven_v3`, generated offline.
- **Settings:**
  - Stability 0.0 for the officer and the dispatcher, 0.5 for the Warden.
  - A mood cue in front of each line, which the voice does not speak:
    `[excited]`, `[slowly, solemn]`, `[nervous, whispering, scared]`.
  - "KSTR" is spelled out as K-S-T-R.
- **The dispatcher's radio:**
  - band-limited to 300–3400 Hz, with light saturation;
  - a static hiss 32 dB under the voice, with sparse crackle.
- **Takes:**
  - Two or more takes per key.
  - Each take was transcribed and compared with the briefing word for word.
  - The pick has the exact words and a clean ending; every file gets a
    short fade-out.
- **Levels:** each narration is levelled to −18 LUFS integrated, with its
  peak at or below −1 dBFS and less than 0.3 s of silence at either end.
- **The sting** (`sfx/missionaccept_1.wav`) is an ElevenLabs sound effect: a
  rising three-note chime with a short static tick in front. It runs
  1.31 s, peaks at −1.2 dBFS and measures −16.5 LUFS.
- The files were checked with `unreal/Tools/Audio/check_sounds.py` and never
  played aloud.

### Where the files live

- **Sources:** `unreal/Resources/Sounds/missions/<key>.wav` (mono, 16-bit,
  44.1 kHz).
- **Import:** `unreal/Tools/Editor/import_sounds.py` (its `missions` group)
  imports them as `/Game/Audio/Missions/SW_<key>` and lists them under
  `missions` in `Content/Audio/Sounds.json`.
- **Playback:** `UAcAudioDirector::PlayNarration(key)` plays one in the ear;
  a new narration cuts the last one off.
- **The radio ducks** under a narration (A1, the user: "while the mission
  audio is playing the radio audio level should be very low"): KSTR 88.7
  falls to −24 dB in 0.3 s, stays down for the narration's length (a second
  narration keeps it down), and comes back over 1 s once it ends or is
  stopped (`ac::MusicDuck`, `UAcMusicPlayer::DuckFor`, `Unduck`). Effects
  and voice lines are not ducked: only the music player's gain takes it.
  Every run logs the gain while it moves (`music: duck -24.0 dB (gain
  0.063) at 31.20 s`), a `-nosound` run included; `-AcMmoShotsK2` checks it
  down on signing up and on the replay, and back at full at the end.
- **Silent runs:** with no audio device (`-nosound`) a narration is only
  logged: `audio: narration survey-easy, 14.2 s (no device)`.

| Key | Length | Peak | Loudness |
|---|---|---|---|
| haul-ore-easy | 15.36 s | −1.5 dBFS | −18.0 LUFS |
| haul-ore-hard | 16.91 s | −1.8 | −18.0 |
| haul-mh-easy | 13.20 s | −2.1 | −18.0 |
| haul-mh-hard | 12.78 s | −1.9 | −18.0 |
| survey-easy | 14.20 s | −1.5 | −18.0 |
| survey-hard | 14.99 s | −1.5 | −18.0 |
| salvage-easy | 12.55 s | −1.5 | −18.0 |
| salvage-hard | 14.52 s | −1.5 | −18.0 |
| hold-easy | 20.53 s | −2.4 | −18.0 |
| hold-hard | 19.57 s | −1.8 | −18.0 |
| hunt-easy | 18.13 s | −1.5 | −18.0 |
| hunt-hard | 17.40 s | −1.5 | −18.0 |
| survey-perilous | 20.55 s | −2.2 | −18.0 |
| salvage-perilous | 16.15 s | −1.5 | −18.0 |
| hold-perilous | 21.25 s | −1.5 | −18.0 |
| hunt-perilous | 18.18 s | −2.0 | −18.0 |

## On the server

The code is in `unreal/server/world/WorldMissions.cpp`, with the numbers in
`WorldMissions` in `WorldMap.h`.

**State per pilot** (`PilotState`):

- the board (`board`);
- the day it was drawn for (`boardDay`);
- the Journal (`journal`);
- the next mission number (`nextMission`);
- the seconds a survey or hold has counted toward its next whole second
  (`missionSeconds`; not saved, so at most a second is lost on a restart).

**Each step** (`missions()`, right after `contracts()`):

- a new day draws a new board, and the done missions leave the Journal;
- an opened site finishes its salvage;
- a driven Prospector inside a survey's ring counts seconds; inside a hold's
  ring it counts them only under the band (`Ringshadow::cover` at the world
  clock).

**Hauls** count at the same unloads as the contracts (`workAfter`). **Hunts**
count through `broodKill`.

**Paying** (`payMission`): the XP through `earn`; the ore and MH into the
wallet; the blueprint copies; the supplies up to their stack. The news is
`NewsKind::mission`, with `id` the number, `xp` and `value` the ore.

**Requests:** `takeMission` (at the realm's Citadel, three at most) and
`abandonMission` (anywhere, not a done one). Refusals:

- "No such mission on the board"
- "Your Journal holds 3 missions: finish or abandon one (L)"
- "Sign up at the realm's Citadel: stand where you unload"
- "No such mission in your Journal"
- "That mission is done already"

**Params:** `WorldParams::missions` (on), `missionSeed` (0: the world's id)
and `huntMissions` (off until N2).

## On the wire (protocol 16)

- **`net::CitadelMission`:**
  - id, kind (3 bits), tier (2), state (2: offered, taken, done);
  - key, title, briefing, instructions, target;
  - the marker: region, `at`, radius;
  - need, have;
  - the reward: xp, ore, hydrogen, an optional blueprint (4 bits), an
    optional item (3 bits), count.
- **`ProgressMessage`** gains `board` and `journal` (after `flare`, before
  `news`). A new board comes at `contractsDawn`.
- **`RequestMessage::MissionAsk{mission, take}`** is variant index **16**
  (`take` false abandons). D1's request moves to 17. The client calls
  `net::Client::takeMission` and `abandonMission`.
- `NewsKind::mission` is appended after `built`.

Protocol 15 is N2's (the Brood). Whichever of the two merges second
renumbers.

## Saved (schema 12)

    ALTER TABLE pilots ADD COLUMN missions TEXT NOT NULL DEFAULT ''

The column holds a pilot's board, Journal, day and next number as JSON
(`encodeMissions`). A pilot saved before has an empty column, so it has no
board until its first step draws one. Schema 11 is N2's; whichever merges
second renumbers. Tests: `server/tests/MissionsTests.cpp`
(`missions_saved_and_loaded`, `missions_v10_save_migrates_and_draws`).

## Tests

- `server/tests/MissionsTests.cpp`:
  - the rewards rise by tier;
  - the board: two offers a tier, seeded, worded, in the right regions,
    every kind over a few days, hunts only with the Brood;
  - the dawn refresh;
  - take, abandon and the limit of three;
  - each kind completes and is paid (haul with supplies capped at the
    stack, survey with a blueprint copy, hold only under the band, salvage,
    hunt);
  - saved and loaded;
  - the v10 save migrates.
- `net-tests/tests/ProtocolTests.cpp`: the board and the Journal round-trip;
  MissionAsk sits at index 16.

**Pictures:** `-AcMmoShotsK2` writes `k2-01` to `k2-05`: the MISSIONS tab,
signed up, the Journal, the marker in the field, a mission done. Run it as
`uv run unreal/Tools/hidden_run.py --out DIR -- -AcFlowTest=mmoshots
-AcMmoShots={out} -AcMmoShotsK2`.
