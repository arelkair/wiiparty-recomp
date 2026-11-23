# Online play design

Goal: two to four players on different PCs play the same Wii Party session over the internet, peer to peer, with no central game server and with the least room for cheating that a peer to peer design allows.

## What the engine does today

The audit of the engine found these sources of non-determinism, which decide the design:

- **Guest time comes from the host clock.** `time_base()` in `engine/src/core/runtime.cpp` is the host `steady_clock` scaled to the 60.75 MHz Wii time base, and its origin is the wall clock. The decrementer, every timeout of the game and `OSGetTime` follow it.
- **Video retraces follow the host clock.** `g_next_retrace` in `engine/src/core/interrupts.cpp` is a host time point, so the retrace interrupt reaches the guest at a different instruction in every run.
- **Other threads feed the guest at their own pace.** The GPU thread signals draw done, the DSP and audio threads raise their interrupts, and the emulated Bluetooth reports every 5 ms of host time (`engine/src/input/wiimote.cpp`). With `video.copy_wait = 0`, EFB copies reach guest memory when the GPU finishes them.
- **The guest reads state that differs between PCs.** The NAND save, the Miis, the console language and the real date (`wall_clock_ticks()`).
- **Floating point must match across builds.** The translated code uses floats; two machines only agree if the engine build is identical and the compiler never contracts or reorders operations.

Consequence: today two instances fed the same inputs do not stay identical, so no netcode can be added without first making the emulation deterministic.

## Options

1. **Lockstep with virtual time (recommended).** Every PC runs the whole game. Only inputs travel over the network (a few hundred bytes per second). A fixed input delay hides the latency. Guest time stops following the host clock and advances with a counter of guest work, and every external event (retrace, draw done, DSP, IPC, disc) is delivered at a fixed point of that counter. Each PC renders its own screen at full quality.
2. **Remote play (streaming).** The host runs the game and sends video and audio to the others, who send their buttons. No determinism work, and the clients cannot cheat, but the host needs 5 to 10 Mbit/s of upload per remote player, adds a video encoder and decoder, and the pointer feels the added latency.
3. **State sync of the board.** Hook the game logic (dice, turns) and send results. It breaks with every minigame and was already tried in a fork that does not work.

Option 1 is the one that scales and that detects cheating, and it needs the most work first.

## Determinism harness (stage 1)

`WP_DETERMINISTIC=1` turns on the virtual time mode: guest time, the video retrace, the DSP, the audio DMA, the emulated Bluetooth and the console clock follow a counter that the guest advances (5 microseconds per poll while it runs, a jump to the next retrace while it is idle, 200 nanoseconds per read of the time base), and the GPU runs on the same thread. `WP_DETERMINISTIC_TURBO=1` skips the real-time pacing, and `WP_DETERMINISM_EVERY=N` logs a hash of the 88 MB of guest memory (and of each megabyte of it) every N retraces. Nothing of this runs when the variable is not set.

Results, on one PC, with the same scripted input (title, menus, player count, Mii and difficulty selection, start of an Isla Aventura game and its first rules screen):

- Two runs of 410 seconds of virtual time (24,600 retraces) have identical memory at all 205 checkpoints, and the same final hash.
- Getting there needed three fixes besides the virtual clock: an idle guest jumps only to the next event (disc, timer or retrace) instead of the next frame; every read of the time base advances the clock a little, so a loop that waits for the time cannot hang; and the EFB copies stay on the GPU as textures and are not written to guest memory in this mode. The copies were the only source of difference found: the GPU rounds them differently from run to run, and they are exactly what would differ between two graphics cards. The cost is that a game that reads a captured picture back from memory (the photo minigames) does not work online.
- The memory that the DSP writes by DMA (audio) is left out of the hash.

Tools for finding a divergence: `WP_DETERMINISM_EVERY=N` logs the hash and the hash of each megabyte; `WP_DETERMINISM_CHUNK=n` adds the hash of each 4 KB page of megabyte n; `WP_DETERMINISM_DUMP=p` writes the content of page p of that megabyte.

Not yet covered: a minigame, another GPU or CPU, and a Linux build.

## Plan

1. **Determinism harness.** A virtual time mode (`WP_DETERMINISTIC`) and a per-second hash of guest memory in the log. Run the same scripted input twice and compare. Fix each divergence until the hashes match for ten minutes of play. This stage decides the real cost of the rest.
2. **Identical start.** A canonical online profile: fixed save, fixed Miis shared between players, fixed language and date, same disc hash and same engine build on every peer.
3. **Transport.** UDP with authenticated encryption, a short handshake, input exchange with a fixed delay, periodic state hash exchange, rooms joined by an invite code.
4. **Connection.** Direct connections with hole punching, a small signaling service to exchange addresses, and a relay only if direct connections fail too often.
5. **Interface.** Lobby in the launcher and the F10 menu, and Discord invitations through the Rich Presence party fields.

## Threat model

A peer to peer game cannot stop a player from modifying their own PC. The aim is to detect it, to keep it from hurting the others, and to avoid new ways in.

- **Tampered game state.** Every peer sends a hash of the guest state every second. A peer whose hash differs from the majority is removed and the match stops. A changed dice roll, score or position shows up as a divergence.
- **Forged inputs.** Each peer sends only the inputs of its own players, signed with the key of the session. Values are checked against the allowed range and rate.
- **Predictable random numbers.** With a shared deterministic state, a modified client could compute future dice. Mitigation: the seed of the random generator is renewed each round from entropy that all peers commit to and reveal.
- **Hidden information.** Every peer holds the full state, so information that the game hides (a secret choice in some minigames) can be read from memory by a modified client. This cannot be prevented in this design and will be listed as a known limit.
- **Attacks through the network.** Every packet is bounded in size, parsed with explicit checks, never trusted for lengths or indexes, rate limited, and fuzzed. No peer can make another run code or write files.
- **Privacy.** Peers see each other's IP address in a direct connection. The relay option hides it; the room code is random and short lived.
- **Saves.** An online match never reads or writes the player's real save.

## Product requirements

The recompilation stays a 1:1 copy of the game. Online play is an optional addition and is off until the player asks for it.

- **Off by default and invisible when off.** With `online.enabled = 0` there is no network code running, no virtual time mode, no deterministic scheduling and no extra button: the game behaves as it does today.
- **Choice at install.** The launcher asks, when the game is installed, whether to enable online multiplayer. The question explains in plain words what it does: the other players' PCs connect directly, they can see the player's IP address unless a relay is used, and the game of an online match does not touch the real save. The answer is stored in `games/wiiparty/settings.ini`.
- **Changeable later.** The same switch is on the launcher Settings page (and in the F10 menu), and the install question says so.
- **A button inside the game.** When online is enabled, the game shows an online button that looks like part of the game, and opens its own menu (create or join a room, invitation code, players, leave). It is drawn by the engine over the game with the look of the game's own menus, and it is not part of the game's code. Patching the game's own menu to add a real button is possible later but is a much larger piece of reverse engineering.
- **A match never changes the offline game.** The canonical online profile is used for the match; the player's own save, Miis and settings are left alone.

## Decisions needed

- Lockstep with virtual time, or remote play.
- Whether a relay server is acceptable if direct connections are not enough (it costs money to run).
- A cryptography library (libsodium) for the handshake and the packets, because writing our own is the wrong place to save a dependency.
