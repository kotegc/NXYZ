# NXYZ

A physics-driven modular synthesizer. An ESP32-S3 runs an on-device menu (rotary encoder + display) that hosts swappable physics simulations — 2D particle collisions, a double pendulum, a Chladni plate — and streams their live state over UART to a Daisy Seed, which turns that state into audio by driving oscillators directly from the simulation. Instead of programming a synth voice by hand, you're listening to a physical system.

**Status:** active hardware/firmware prototype. The particle-collision module is driving audio end to end; double pendulum and Chladni modules are implemented on the display side and being wired into the audio path next.

## How it works

- **ESP32-S3** ([`052026_ESP32-S3_Sketch_001_GK/`](052026_Sketch_001/ESP32-S3/052026_ESP32-S3_Sketch_001_GK)) — PlatformIO project. Drives a display (LovyanGFX) and a rotary encoder menu, runs the selected physics module's simulation step each frame, and packs its state into a UART packet for the synth.
- **Daisy Seed** ([`DaisySeed/`](052026_Sketch_001/DaisySeed)) — receives the UART packet and maps simulation state onto oscillator parameters in real time.
- **Modules** ([`src/modules/`](052026_Sketch_001/ESP32-S3/052026_ESP32-S3_Sketch_001_GK/src/modules)) — each physics simulation (`particles`, `pendulum`, `chladni`) implements a common `PhysicsModule` interface, so new simulations can be dropped in without touching the menu or UART plumbing.

## Repo layout

This repo tracks firmware/code only. Mechanical design (enclosure CAD), industrial design (renders), and project docs live in a local, non-public project structure and aren't part of this repo.

```
052026_Sketch_001/
├── ESP32-S3/052026_ESP32-S3_Sketch_001_GK/   PlatformIO project — display, menu, physics modules
└── DaisySeed/                                  Audio firmware — oscillator driven by UART packets
```

## Building

The ESP32-S3 side is a standard [PlatformIO](https://platformio.org/) project:
```
cd 052026_Sketch_001/ESP32-S3/052026_ESP32-S3_Sketch_001_GK
pio run
```
The Daisy Seed side builds with the [Daisy toolchain](https://github.com/electro-smith/DaisyExamples) via the included `Makefile`.

## License

MIT — see [LICENSE](LICENSE).
