# MMORPG milestone 5: ready to ship

Goal: the MMO mode works in a packaged Mac build, its traffic is encrypted
even on the local server, and the last review leftovers are gone. Still
Prospector only, no AI units. No release is published (the user's call).
Log: [mmorpg-log.md](mmorpg-log.md).

## Decided (2026-10-13, manager, user away)

- Encryption with Monocypher (CC0, small, reviewed): X25519 key exchange in the handshake, XChaCha20-Poly1305 per packet. On locally too: only the host changes.
- The server ships inside the Mac package next to the game; the game finds it there, then falls back to the repo build in editor runs.
- Windows (Winsock) and Linux server builds wait: they need the AWS build machine, which costs money. Ask the user.

## Tasks

| # | Task | Needs | Status |
|---|---|---|---|
| E1 | Encryption: Monocypher in ThirdParty, keys in the handshake, every packet sealed; tests incl. tampered, replayed and wrong-key packets | | running |
| E2 | Server in the Mac package: the release script stages autocraft-server, the game finds it; a packaged hidden run of -AcMmo | | running |
| E3 | Leftovers: false wallet gain flash after an edge crossing; weld progress shown while building up close | | done |

Done when: a packaged Mac build plays the MMO mode in a hidden run; packets on the wire are unreadable and forged ones dropped; all suites and MMO tests pass.
