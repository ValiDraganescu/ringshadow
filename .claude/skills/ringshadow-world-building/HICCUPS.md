# Hiccups

What surprised an agent building a world, and what fixed it. Add a line the
moment something surprises you: date, region, what happened, the fix.

- 2026-10-18, every region: about 40% of each region was wall or cliff in a
  ring round the edge (border level 4, 10-14 m), so a pilot was always
  boxed in. Fix: rims level 1 with a slope, passages wide, rock standing
  free instead of round the edge.
- 2026-10-19, Prospect Landing: free-standing rock as hand-written noisy
  circles and a hand-drawn plateau still read as placed. Fix: landforms
  drawn by code; hand-written ground only where LANDFORMS.md "Hand-written
  ground that stays" allows.
- 2026-10-19, Prospect Landing: a butte 23 cells from the Citadel stopped
  every gun round of a raid. Fix: the pad's 30-cell field-of-fire rule.
- 2026-10-19, Prospect Landing: a style butte's talus apron spread past its
  reach and left no level ground for a Citadel beside the pad. Fix: reach
  counts the apron's rough edge (radius × 1.75 + 3.5).
- 2026-10-19, Prospect Landing: a large swell 30 cells off hid one pilot's
  headlamp sight from another; boulders inside a passage's lane blocked
  it. Fix: swells keep 26 + reach from a pad, boulders obey lanes.
- 2026-10-19, the first buttes stood straight out of the flat like drums.
  Fix: a talus apron (level 1, slope 3.5) under every butte, mesa and spire
  cluster.
- 2026-10-19, an escarpment's back ran straight for 30 cells with sharp
  boomerang tips. Fix: the back wanders ±35% of its depth and its ends
  round off.
- 2026-10-19, rust-hollow, ashen-shelf, slag-pockets, dust-terraces, cold-fork:
  the `badlands`/`highlands`/`ridge` bases drew cliff-ringed plateaus
  under the landforms. Fix: `openField` base, ground from landforms only.
- 2026-10-19, Cold Fork: switching its base style moved its canyon's
  noise-seeded wall shelves; the old gun spot could no longer reach the
  north arm. Every path point still has 2+ rim spots. Fix: tests pick the
  gun spot by `rig::rimGunSpot`.
- 2026-10-19, Cold Fork: a level-1 pad flat (slope 10) on level-0 ground
  made the pad a mound; no Citadel fit within 19 cells. Fix: pad flats at
  level 0, slope 5.
- 2026-10-19, Ashen Shelf: `badlands` ringed the rim with cliff-sided
  shelves and placed no free rock. Fix: `mesa-country`.
- 2026-10-19, Dust Terraces: removing its old plateau left The Roost's ramps
  starting at the wrong level; the prize and the vault were cut off. Fix: a
  gentle hand-written rise (slope 5) north of the zone.
- 2026-10-19, Brine Pans, Dust Terraces: the canyon zones' cuts were the 9
  straight cliff runs each. Since erosion (section 10) they are worn too.
- 2026-10-19, Start: widened spans reaching a corner left their strips
  disconnected; "the border's outline closes the passage". Fix: trim the
  span in both files.
- 2026-10-19, all: old outlines hugged the land (wall 28-47%). Fix: a second
  `sculpt.py --grow 14 --no-widen` pass once the base was `openField`.
- 2026-10-19, the in-game look: cockpit views showed stacked slices that
  swam with the camera's height. Cause: parallax at grazing angles. Fix in
  the material (GROUND-LOOK.md).
- 2026-10-19, zsh: `R="uv run …"; $R …` fails ("no such file"): zsh does
  not split a variable into words. Put chains in a bash script.
- 2026-10-19, still open: a wash on level-0 ground barely shows from above.
- 2026-10-19, every region: the rims ran straight along the edges (the
  outline kept 3 cells in) and the wall rocks, on a jittered grid, stood in
  rows where the wall was a strip. The user: "too straight … so
  artificial". Fix: erosion in `expand` and clustered wall rocks never in a
  row (docs/world-sculpting.md, section 10).
- 2026-10-19, erosion: a wave clipped at its limit (the rim 1.6 cells from
  the edge) runs flat along it: a new straight run. Fix: fold it back in.
- 2026-10-19, Brine Pans: a plateau's slope falls outward and closed the
  Salt tunnel's mouth, which opened out of the face. Fix: a cut whose end
  lay past the old face runs on through the slope.
- 2026-10-19, Dust Terraces: an offset segment carved after the file's ramps
  bit a ramp's side ("too steep to drive here"). Fix: segments and notches
  keep clear of ramps; the face never moves within 2 cells of a ramp's top.
- 2026-10-19, measuring a change in a canyon region: the plateau's sloped
  face costs 1-2.5 points of wall (a slope is wider than a cliff) and,
  near a pad, horizon; in a pad's field of fire set it back by the whole
  slope.
- 2026-10-19, Cold Fork: bending The Fork's north arm (1.2 cells, a cell
  wider) took the rim gun's ledge 5 cells from the path; the test gun found
  no ground and the wave got through (`Canyon_rewardsByDamage`, the
  standings test's turret kills). Narrowing the arm instead hid the floor from the gun. Fix:
  a lane the walkers walk keeps its written line; only its straight runs get
  a 0.5 bow. `CanyonWallTests`' gun line is not snapped to whole cells, so
  half a cell of floor flips whether its gap lets walkers through: check it
  after any change to the Salt arm.
- 2026-10-19, `Autocraft.Mmo.BuildCitadel` failed once with the server
  refusing a spot the client allowed: a stale `unreal/server/build/autocraft-server`.
  `make -C unreal/server build/autocraft-server` before automation tests
  that start the local server.
