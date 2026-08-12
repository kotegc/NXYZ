# DaisySeed

Audio firmware for NXYZ. Receives simulation state from the ESP32-S3 over
UART and turns it into sound: an 8-voice oscillator/envelope pool triggers
on incoming collision packets (and directly from the Daisy Seed's own
encoder for manual testing), mapping normalized position/speed data onto
pitch and amplitude.

## Building

Requires the [Daisy examples toolchain](https://github.com/electro-smith/DaisyExamples)
(`libDaisy` + `DaisySP`) cloned locally. By default the Makefile expects it
at `~/Desktop/DaisyExamples/`, relative to this folder — override if yours
lives elsewhere:

```
make LIBDAISY_DIR=/path/to/DaisyExamples/libDaisy DAISYSP_DIR=/path/to/DaisyExamples/DaisySP
```

## Protocol

Wire format is defined once in [`../shared/uart_protocol.h`](../shared/uart_protocol.h)
and included by both firmware projects — see the root
[README](../README.md#how-it-works) for the packet framing.
