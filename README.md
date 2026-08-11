# NXYZ

A handheld synthesizer built on handheld-gaming and chiptune lineage —
not a general-purpose synth with a screen added, but a device designed
around **heuristic metaphor**: borrowing an interaction pattern that's
already intuitive in one context (40 years of stable game-controller
layout — L, R, A, B, X, Y, joystick) and using it to make a new
instrument legible without a manual.

**Status:** active hardware/firmware prototype (RP1). Two audiovisual
interaction modules — built as physics simulations, the current proof
of concept for a broader idea — are implemented on the ESP32-S3
display side; one drives audio end to end already. The RP1 PCB is
designed and currently being routed — not yet fabricated.

## Design thesis

Most synthesizers ask musicians to learn an interface before they can
create. Game controllers don't — decades of shared convention already
did that work. Chiptune and tracker culture (Game Boy, Nintendo DS,
LSDJ, the Dirtywave M8) is the strongest existing precedent for that
same idea applied to music: real instruments built inside consumer
game-hardware constraints, by people fluent in game-controller logic.

To ground the idea in something concrete, I bought a Nintendo DS Lite
as a study sample and ran two focused studies on it: a mechanical
teardown (housing/electronics packaging at a similar footprint to this
device), and a session with *Electroplankton* — a DS game whose
interaction patterns are essentially a set of tiny audiovisual
instruments. That's the clearest existing precedent for the specific
idea this project tests: audiovisual feedback as a real part of the
composition process, not decoration on top of it. Physics simulation
is the current proof of concept for that idea — the broader goal is
any interaction model that pairs sound with something a musician can
feel, not physics specifically.

## Validated so far

Two audiovisual interaction modules, built as physics simulations,
have been tested enough to answer the questions that mattered most
before going further:

- **Particle collisions → sound.** Turning the rotary encoder spawns a
  particle; particles collide and trigger an amplitude-enveloped
  oscillator, with collision position mapping to pitch. Targeted 50
  concurrent particles without dropping frame rate — it held at 100.
  Run through an effects chain, the result is genuinely musical, not
  just a proof of concept.
- **Double pendulum → visual.** Encoder sets the pendulum angle, a
  click drops it and starts the simulation. Audio mapping isn't wired
  up yet, but the visual/interaction side works.

Both answered real questions — whether the hardware has headroom for
this kind of software, and whether audiovisual-driven synthesis is
actually fun to use rather than a gimmick. So far, yes to both.

## How it works

- **ESP32-S3** ([`052026_ESP32-S3_Sketch_001_GK/`](052026_Sketch_001/ESP32-S3/052026_ESP32-S3_Sketch_001_GK)) —
  drives the display (LovyanGFX) and rotary-encoder menu, runs the
  active physics module's simulation step each frame, and streams its
  state over UART.
- **Daisy Seed** ([`DaisySeed/`](052026_Sketch_001/DaisySeed)) —
  receives UART packets and maps simulation state onto oscillator
  parameters in real time.
- **Modules** ([`src/modules/`](052026_Sketch_001/ESP32-S3/052026_ESP32-S3_Sketch_001_GK/src/modules)) —
  each interaction module implements a common `PhysicsModule`
  interface (`init`/`stop`/`loop`/`name`), so new ones drop in without
  touching the menu or UART plumbing. Three exist today, all physics
  simulations (particles, pendulum, Chladni plate), but the interface
  doesn't assume physics — any module that can produce a per-frame
  state and respond to the encoder fits. The particle module runs its
  physics on a dedicated FreeRTOS task — its collision checks scale
  with particle count², heavy enough to want a core of their own —
  while the simpler pendulum/Chladni modules run inline in the main
  loop. Same interface, different concurrency underneath: a
  resource-proportional choice, not an inconsistency (heavier
  workload gets the dedicated core; the other two don't need one).
- **UART protocol** — 7-byte packets at 31250 baud (the MIDI standard
  rate). Payload bytes are capped at 254 so nothing in a packet body
  can be mistaken for the `0xFF` sync byte marking a new packet.

## Vision

The long-term shape of this is closer to a tiny DAW than a
fixed-function synth: transport, tracks, devices, arrangement, a
library of installable "instruments" — the vocabulary Ableton/FL
Studio/Logic/Reaper all converge on, reimagined for a handheld. Each
physics module is a device in that model, the same way a plugin is a
device in a desktop DAW — the DAW doesn't need to know a device
happens to look like a tiny game.

The eventual goal is an SDK: let creative coders and game developers
build new audiovisual "instruments" for the platform themselves.
That's a deliberately constrained layer on top of the firmware —
third-party modules would talk to a small API, never directly to the
display/audio/UART hardware.

Roadmap, roughly:
1. **RP1** *(current)* — finish PCB routing, fabricate, bring up.
2. **RP2** — fix whatever RP1 turns up, finalize hardware.
3. **Firmware platform** — build the DAW-like composition model on
   validated hardware.
4. **SDK** — open the platform to outside developers.

This is early — RP1 hasn't been fabricated yet, so read the SDK/DAW
vision as the direction, not a promise of what's built today.

## Repo layout

This repo tracks firmware/code only. Mechanical design (enclosure
CAD), industrial design (renders), and project docs live in a local,
non-public project structure and aren't part of this repo.

```
052026_Sketch_001/
├── ESP32-S3/052026_ESP32-S3_Sketch_001_GK/   PlatformIO project — display, menu, physics modules
└── DaisySeed/                                  Audio firmware — oscillator driven by UART packets
```

## Building

The ESP32-S3 side is a standard [PlatformIO](https://platformio.org/)
project:
```
cd 052026_Sketch_001/ESP32-S3/052026_ESP32-S3_Sketch_001_GK
pio run
```
PlatformIO auto-detects the board's serial port. If you have multiple
boards attached and need to pin a specific port, create a local
`platformio_override.ini` (gitignored) with `upload_port`/
`monitor_port` set.

The Daisy Seed side builds with the
[Daisy toolchain](https://github.com/electro-smith/DaisyExamples) via
the included `Makefile` — see
[`DaisySeed/README.md`](052026_Sketch_001/DaisySeed/README.md) for the
path override if your checkout isn't at the default location.

## License

MIT — see [LICENSE](LICENSE).
