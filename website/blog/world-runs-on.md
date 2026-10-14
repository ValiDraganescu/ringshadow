# The world runs on after you quit

> Quit Ringshadow and its world keeps going: the local server stays up on the Mac, the clock turns, meteors fall and the Brood roams, at about 0.008% of a CPU core.

Source: https://ringshadow.com/blog/world-runs-on

M8 · The realm under siege

14 October 2026

A world that lasts shouldn't stop when you close the game, and now it keeps going. The local server runs on your Mac, and the game picks it back up when you return.

![The home screen with a line reading World server running, since 23:03, and a STOP button](https://ringshadow.com/blog/media/world-runs-on/03-home-running.webp)

*The home screen tells you the world is still running, and since when.*

In Ringshadow's online mode the game is only a client. The world lives in a separate server program, the same one a public world will run, and until today it quit with the game. Now it detaches. Quit, and the world's clock keeps turning, starfalls keep coming, the Foundry and the Lab finish their jobs, and the Brood's director keeps sending bugs to the places worth being. Start the game again and it finds the running server and reattaches.

A server that runs all the time had better be cheap. With nobody connected it slows down: the clock and timers tick twice a second, the regions wait, and it costs about 0.008% of one CPU core. With a pilot in the world it runs at the full 60 steps a second, about 0.3 to 0.5%.

If the Mac was off, there's nothing to run on, so the world catches up instead: on the next start the server works through the downtime, up to 24 hours of it. The same catch-up pays the [mercenary prospectors](https://ringshadow.com/blog/mercenaries) you can hire at the Citadel.

The home screen shows "World server running" with the time it started, and a STOP button for when you want it off.

## Album · Quit, come back

- ![The Prospector on its landing pad at Prospect Landing, before the game quits](https://ringshadow.com/blog/media/world-runs-on/01-first-run.webp) Before quitting: the Prospector on the pad.
- ![The same spot after the game restarts and reattaches, the clouds moved on](https://ringshadow.com/blog/media/world-runs-on/02-reattached.webp) After a restart: same world, same pilot. The clouds moved on while the game was closed.
- ![The home screen showing World server running with a STOP button](https://ringshadow.com/blog/media/world-runs-on/03-home-running.webp) World server running, with STOP.
- ![The home screen after STOP, without the server line](https://ringshadow.com/blog/media/world-runs-on/04-home-stopped.webp) After STOP: the line is gone.

The network protocol didn't change. The tests stand at 467 core, 200 server and 70 network, all passing, and they never leave a server running behind them.
