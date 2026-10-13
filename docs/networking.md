# Networking

How the MMORPG mode's game talks to its server ([mmorpg.md](mmorpg.md)).
Status, 2026-10-13: a design, no code yet. The numbers in it are starting
points to measure, not measurements.

## Decided

From [mmorpg.md](mmorpg.md), 2026-10-13:

- **The local server is the production server.** The same program, started
  by the game on the player's machine and reached over the network like any
  server. Only the host changes. There is no single-player path through the
  code.
- **Our own networking on UDP**, for efficiency and control: an
  authoritative server built on the engine-free core. Not Unreal's
  replication, and not lockstep (mmorpg.md says why).

What follows from "only the host changes": everything that runs online
runs locally too. Encryption, the handshake, the lag of the network code,
the save to the server's database: all of it is on in single player. A
bug in any of them shows up on the developer's machine, not first on the
public world.

## The pieces

| Piece | What | Built with |
|---|---|---|
| `AutocraftNet` | The network library: sockets, packets, acks, channels, encryption, the messages of the protocol, the lag simulator. Engine-free C++20, shared by the server and the game. | clang (tests), Unreal (in the game), the server's build |
| The server | A program with no engine and no graphics: `AutocraftCore`, `AutocraftNet` and the world code (regions, lots, pilots, AI pilots, saves). | clang on the Mac and Linux, and a Windows build (open) |
| The client | The MMORPG mode in the game module: the connection, a mirror of the world from the server's snapshots, prediction of the driven machine, drawing. | Unreal, in `unreal/Source/Autocraft` |

The game uses `AutocraftNet` directly, not Unreal's `Sockets` module, so
one piece of code is the network on both ends. `AutocraftNet` gets its own
tests in the style of `core-tests`: plain clang, seconds to run.

The server ships in every package, for every platform, because the local
server runs on the player's own machine: Mac, Windows and Linux. The Linux
build is also the one the public world runs.

## The local server

1. The game starts the server program as a child process, hidden, with
   the world's save file and a one-time secret on its command line.
2. The server binds a UDP port on `127.0.0.1` only (port 0: the system
   picks a free one), prints the port on its standard output and starts
   its tick.
3. The game reads the port and connects to it through the same handshake
   a public server gets. The secret proves it is the game that started the
   server.
4. On quit, the game asks the server to stop. The server saves and exits.
   If the game dies, the server notices its parent is gone, saves and exits
   too.
5. If the server dies, the game says so and offers to restart it. The
   server's log goes to `~/Library/Logs/Autocraft/server.log` on the Mac
   (and the matching place on Windows and Linux), beside the game's.

Binding to `127.0.0.1` keeps the local server off the network: nothing
outside the machine can reach it, and the system's firewall should not ask
the player anything.

A public server is the same program started by hand or by a service, bound
to a public address, with no secret on its command line and the login in
its place (accounts are a later step, mmorpg.md).

## The transport

Plain UDP sockets, written by us: POSIX sockets on the Mac and Linux,
Winsock on Windows, behind one small wrapper.

### Packets

- **At most 1,200 bytes** a packet, under the size the internet carries
  without splitting it. Anything bigger is split into pieces by the
  library.
- **A header on every packet:** the protocol id and version, the
  connection id, the packet's sequence number, the newest sequence it has
  received from the other end and a 32-bit field of the 32 before that.
  Every packet so acks the other side's last 33, so an ack is lost only if
  33 packets in a row are.
- **A mismatched version is refused** at the handshake, with a message the
  game can show ("the server runs version x").

### Channels

Each message goes on one of three channels:

| Channel | For | When a packet is lost |
|---|---|---|
| Unreliable | Snapshots, inputs | Nothing: the next one replaces it. |
| Reliable, in order | Requests ("build on this lot"), replies, events (a level-up, a death), chat | Resent until acked. |
| Bulk | Large data sent once: a region's lots and layout when the pilot enters it | Split into pieces, each resent until acked, reassembled. |

### The connection

- **Handshake with a cookie.** The server answers the first hello with a
  challenge the client has to send back, before it keeps any state for
  it. This stops someone faking hellos from a forged address from filling
  the server's memory or using it to flood a third party.
- **Keep-alive and time-out.** Both ends send at least a packet a second;
  a silence of 10 s ends the connection.
- **A budget per client.** The server sends each client at most so many
  bytes a second, and the snapshot fills what is left after the reliable
  messages (see "Interest").

### Encryption

Every packet after the handshake is encrypted and signed, so it cannot be
read or forged on the way. Locally too ("Decided"). Open: which
algorithm and which library (a small, well-reviewed one kept in
`ThirdParty/`; writing our own cryptography is the one thing we should not
do).

## The messages

Written by a bit-packing writer of our own, not JSON: quantised values,
only the bits a value needs. JSON stays for the saves and the tools.

**Client to server:**

| Message | Channel | What |
|---|---|---|
| Hello, Answer | handshake | The version, the cookie, the secret or the login. |
| Input | unreliable | The driven machine's controls for one tick: `ac::Pilot` today (walk, heading, act, hold, ability, mend, the crosshair's target), its tick number and the last few inputs again, so a lost packet costs nothing. |
| Ack | unreliable | The newest snapshot received: the baseline for the next delta. |
| Request | reliable | Everything that is not steering: take a machine, hand it back, build on a lot, pick a level-up card, take a contract. |

**Server to client:**

| Message | Channel | What |
|---|---|---|
| Welcome | handshake | The connection, the pilot, the region, the server's tick and time. |
| Region | bulk | The region's static data: its terrain seed, lots, deposits, wells. |
| Snapshot | unreliable | What the pilot can see now (below), and the number of the last input the server used. |
| Event | reliable | What must not be missed: a level-up, a death, a building finished, a lot taken. Today's `ac::GameEvent`s, the ones the pilot is meant to know. |
| Reply | reliable | The answer to a request: done, or refused and why. |

## The tick

- **The server steps at 60 Hz**, as the game steps the simulation today
  (`UAcSimSubsystem`, fixed 1/60 s steps). The balance and the AI were
  tuned at that step; changing it changes the game.
- **Snapshots go out at 20 Hz to start.** A starting point: measure the
  bytes and the feel, and raise or lower it.
- **Inputs go out every tick the client runs**, each with the last few
  repeated.
- **The client shows others about 100 ms in the past**: two snapshots at
  20 Hz, so one lost snapshot still leaves two to blend between. The game
  already blends between fixed steps (`FAcFrame::Alpha`); this is the
  same idea over a longer gap.

## Interest

The server never sends a client the whole region. For each client it works
out what that pilot may know and sends only that:

1. **Fog of war first.** Only what the pilot's side sees: the core already
   works this out (`Vision`). Nothing hidden by the fog ever leaves the
   server, so a hacked client has nothing to reveal.
2. **Near before far.** The region is cut into cells. What is near the
   pilot goes in every snapshot; what is farther goes in fewer of them.
3. **What matters first.** The driven machine, what shoots at it, what it
   aims at, then the rest by distance.
4. **A byte budget.** A snapshot takes things in that order until the
   budget is spent. What is left out waits, and gains priority every
   snapshot it waits, so nothing is starved.

### Snapshots as deltas

The server keeps, for each client, the snapshots that client has not acked
yet. A new snapshot is written as the difference from the newest one the
client acked: only what changed, and only the fields that changed. A
machine standing still costs nearly nothing; a new one costs its full
state once. If the client acks nothing for a while, the server starts over
from a full snapshot.

Quantised fields, to start (to measure):

| Field | As |
|---|---|
| Position | Centimetres in the region, as integers |
| Heading, turret, aim | 16 bits for a full turn |
| Health, shields | The value's whole range in 8 to 12 bits |
| Kind, state, owner | Their enum's own bits |

## The client

### A mirror of the world

The client keeps its own copy of what it was sent: the units, buildings,
shots and lots in view, as the same `ac::` types the game draws today. The
renderer reads `const ac::Unit*` and `const ac::Structure*` now
(`UAcWorldRenderer`), so it can draw the mirror with little change. How
much of the game layer reaches into `ac::Simulation` itself, and what that
needs instead, is the first thing to map before the code.

### Prediction of the driven machine

Steering through a server would lag by a round trip, which no one can
drive with. So the client runs its own machine ahead:

1. Each tick the client applies its input to the driven machine at once,
   with the core's own pilot code (`stepPiloted`), and sends the input.
2. Each snapshot says which input the server has used up to, and where the
   machine really is after it.
3. The client puts its machine back to the server's state and runs again
   all the inputs the server has not used yet.
4. A small difference is eased in over a few frames, so it does not jump.

Only the driven machine is predicted. Everything else is drawn from the
snapshots, in the past.

The client and the server must agree closely, not bit for bit: the
corrections take care of the rest. The core's build flags
(`-ffp-contract=off`) already keep floating-point work the same on
different machines.

### Shots

Today the view measures what is under the crosshair each frame and the
simulation shoots exactly that (`ac::Pilot::crosshair`): a shooter's
rules, no aim assist. Online the client sees others 100 ms in the past, so
the server judges a shot where the target was on the shooter's screen:

- The client sends the target under the crosshair with the snapshot time
  it saw it at.
- The server keeps the positions of the last 250 ms, rewinds the target to
  that moment and checks that the shot could hit it from there (range,
  line of sight, the sight's cone).
- It rewinds no further than that limit. Players with very high lag aim
  ahead, as in any online shooter.

Shells, grenades and rockets fly on the server. The client shows the
muzzle and the sound at once and the flight from the snapshots.

## What the client may decide

Nothing but its own inputs and what it draws. The server checks every
request (may this pilot take this machine, does it own this lot, can it
pay) and clamps every input (walk within −1…1, a heading, a known target).
Requests have a rate limit per client.

## Saves

The world is the server's. It saves to its own database: SQLite, as the
game already uses for tracking (`TrackingStore`). Locally the file sits in
`~/Library/Application Support/Autocraft/` on the Mac (and the matching
place elsewhere). The server writes what changed every few seconds and
everything on stop, so a crash loses seconds, not a session. Whether the
public world keeps SQLite or moves to another database is open.

## Testing

- **The lag simulator.** `AutocraftNet` can add delay, jitter, loss,
  duplicates and reordering to every packet, set per connection. On
  `127.0.0.1` there is no lag at all, which hides every bug in prediction
  and correction, so the tests run through it and the game has a setting
  for it.
- **`AutocraftNet`'s own tests**, engine-free: acks, resends, splitting,
  the handshake, encryption, deltas, all through the lag simulator.
- **Isolated scenarios.** A server and one or two clients on
  `127.0.0.1`, playing one moment (a pilot drives and turns under 150 ms
  of lag and 5% loss; a shot at a moving target; a request refused). Never
  a whole game ([AGENTS.md](../AGENTS.md)).
- **Measured, not guessed:** bytes a second per client in a quiet scene and
  in a fight, the server's tick time per region, the snapshot size per
  machine in view. These replace the starting numbers in this doc.

## The order of work

1. **`AutocraftNet`:** sockets, packets, acks, the three channels, the
   handshake, the lag simulator, with its tests.
2. **The server program:** a region running on the core, a pilot driving
   one machine, saves. Built for the Mac first.
3. **The local server:** the game starts it, connects, stops it.
4. **The client mirror:** snapshots, deltas, interest, drawing.
5. **Prediction** of the driven machine, and shots with lag compensation.
6. **Encryption.**
7. **The server on Windows and Linux**, in the release builds
   ([builds.md](builds.md)).

The first milestone to aim at: drive one machine on one region through the
local server, under the lag simulator, and have it feel like today's game.

## Open

- **The server's build** on Windows: clang, MSVC, or an Unreal program
  target without the engine. The release machine builds Windows already.
- **Encryption:** the algorithm and the library.
- **The rates:** snapshot rate, the byte budget per client, the
  interpolation delay. To measure.
- **How the game layer reads the simulation today:** what the mirror has
  to stand in for.
- **The public world's database.**
- **The server program's name**, in the code's naming (the `Autocraft`
  modules, [rename-ringshadow.md](rename-ringshadow.md)).

## Words

| Word | Meaning |
|---|---|
| Ack | A note back saying "I got that packet". |
| Baseline | The last snapshot the client confirmed; the next one is sent as the difference from it. |
| Channel | A lane in the connection with its own rules for loss: unreliable, reliable or bulk. |
| Cookie | A challenge in the handshake that proves the sender's address is real. |
| Delta | Only what changed since the baseline. |
| Interest | What a client is told about: what its pilot may see, most important first. |
| Interpolation | Drawing others smoothly between two snapshots, slightly in the past. |
| Lag compensation | The server judging a shot by where the target was on the shooter's screen. |
| Prediction | The client moving its own machine at once, corrected by the server later. |
| Quantise | Storing a value in fewer bits, at a fixed precision. |
| Reconciliation | Resetting the predicted machine to the server's state and replaying the inputs since. |
