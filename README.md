<div align="center">

# 🔊 steam-audio-rust

**An AI-driven Rust reimplementation of [Steam Audio](https://github.com/ValveSoftware/steam-audio)**

Spatial audio, physics-based sound propagation and HRTF rendering — memory-safe, portable, and written in Rust.

[![CI](https://github.com/razedproduction/steam-audio-rust/actions/workflows/ci.yml/badge.svg)](https://github.com/razedproduction/steam-audio-rust/actions/workflows/ci.yml)
[![Status](https://img.shields.io/badge/status-foundations-yellow)](#status)
[![Language](https://img.shields.io/badge/language-Rust-dea584?logo=rust)](https://www.rust-lang.org/)
[![MSRV](https://img.shields.io/badge/MSRV-1.85-dea584)](Cargo.toml)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue)](LICENSE)
[![Fork of](https://img.shields.io/badge/fork%20of-ValveSoftware%2Fsteam--audio-1b2838?logo=steam)](https://github.com/ValveSoftware/steam-audio)

[Overview](#overview) · [Status](#status) · [Roadmap](#roadmap) · [Repository layout](#repository-layout) · [Getting started](#getting-started) · [Contributing](#contributing) · [Credits](#credits-and-license)

</div>

---

## Overview

[Steam Audio](https://valvesoftware.github.io/steam-audio/) is Valve's spatial audio toolkit: it lets game and VR developers render sound that responds to the 3D world around the listener.

This project is an experiment in reimplementing that functionality in **Rust**, with AI-assisted development doing much of the translation and porting work. The reference behavior is the original C++ implementation, so the goal is functional parity and verifiable results, not a loose reinterpretation.

**Why Rust?**

- **Memory safety** for a real-time audio library that runs inside other people's processes.
- **Fearless concurrency** for the multi-threaded simulation and mixing workloads that spatial audio needs.
- **Easy distribution** through Cargo, with straightforward cross-compilation.

## Status

> 🚧 **Foundations stage.** The math and signal-processing building blocks are ported and verified against the C++ reference; no audio effects or simulation exist yet, so nothing here is usable in production.

Ported so far (all checked against golden data produced by the original C++):

| Stage | Module | Parity |
| --- | --- | --- |
| 2.1 | Vectors, matrices, quaternions, coordinate spaces, polar vectors, spheres, boxes, rays | bit-exact on Windows |
| 2.2 | Math functions, sampling | bit-exact on Windows |
| 2.3 | Array math, `DynamicMatrix` | bit-exact on Windows |
| 2.4 | `AudioBuffer`: interleave, mix, downmix, ambisonics format conversions | bit-exact on Windows |
| 2.5 | FFT (on [`realfft`](https://crates.io/crates/realfft)/[`rustfft`](https://crates.io/crates/rustfft) instead of PFFFT) | within `1e-5` × peak (worst measured 5.4e-7) |
| 2.6 | Spherical harmonics: bands 0–6, rotations up to order 5 | bit-exact on Windows |
| 2.7 | DSP primitives: frequency bands, biquad IIR filters, delay lines, allpass filters, window functions | bit-exact on Windows (IIR against upstream's AVX path) |

**Next:** stage 3, HRTF and binaural rendering.

The detailed plan, module order, per-module tolerances and known risks live in [`docs/PORTING_PLAN.md`](docs/PORTING_PLAN.md); intentional behaviour differences from upstream are listed in [`docs/DEVIATIONS.md`](docs/DEVIATIONS.md). Treat the roadmap below as intent, not a promise.

## Roadmap

Planned scope, mirroring the feature areas of the original library. Boxes are checked only when a module passes comparison tests against the reference implementation.

- [x] **Foundations**: math types, ambisonics primitives, FFT, audio buffers and DSP primitives
- [ ] **Binaural rendering**: HRTF loading and interpolation, convolution, panning, ambisonics effects, SOFA files
- [ ] **Direct sound**: distance attenuation, air absorption, directivity, occlusion and transmission
- [ ] **Scene and geometry**: static and instanced meshes, ray tracing backend
- [ ] **Reflections**: real-time and baked sound propagation
- [ ] **Pathing**: diffraction and occlusion-aware paths between source and listener
- [ ] **C API compatibility layer**: drop-in replacement for the original `phonon` interface
- [ ] **Bevy integration**: a `bevy_steam_audio` crate on top of the engine-agnostic core

## Repository layout

| Path | Purpose |
| --- | --- |
| [`core/`](core) | **Reference** C++ sources of upstream Steam Audio (the specification; not modified) |
| [`crates/steam-audio/`](crates/steam-audio) | The Rust reimplementation (library crate) |
| [`crates/parity/`](crates/parity) | Test support: loads golden data and compares with per-module tolerances |
| [`tools/golden/`](tools/golden) | Reference C++ build and golden-data generators ([README](tools/golden/README.md)) |
| [`testdata/golden/`](testdata/golden) | Reference outputs (`.npy`) produced by the C++ implementation |
| [`CLAUDE.md`](CLAUDE.md) | Working rules for AI-assisted sessions (loaded automatically by Claude Code) |
| [`docs/`](docs) | [Porting plan](docs/PORTING_PLAN.md), [deviations](docs/DEVIATIONS.md), [architecture decisions](docs/adr) |

## Getting started

The library is not usable for audio yet, but you can build it and run the parity tests:

```bash
git clone https://github.com/razedproduction/steam-audio-rust.git
cd steam-audio-rust
cargo test --all
```

Requires Rust 1.85 or newer ([rustup](https://rustup.rs/)). The tests read the committed golden files in `testdata/golden/`, so no C++ toolchain is needed. CI runs formatting, Clippy and the tests on Linux, Windows and macOS.

Regenerating the golden data requires building the upstream C++ with MSVC on Windows x64; see [`tools/golden/README.md`](tools/golden/README.md).

## How the AI-driven approach works

- The original C++ code is the **specification**. Behavior is matched against it, not guessed.
- For every module, a C++ generator in `tools/golden/` runs the upstream code on deterministic pseudo-random inputs and stores the results as `.npy` files.
- The Rust tests replay the same inputs and compare outputs with an explicit per-module tolerance; most modules so far match **bit for bit**. Any looser tolerance is justified in the plan.
- AI-generated code is treated like any other contribution: it must pass `cargo fmt`, `cargo clippy -D warnings` and the tests, and goes through a pull request per stage.

## Contributing

The most useful contributions right now are:

1. Opening issues with design questions or scope suggestions.
2. Reviewing ported modules against upstream and the [porting plan](docs/PORTING_PLAN.md).
3. Reporting parity failures on platforms other than Windows (goldens come from MSVC; other platforms may differ in the last bits).

The working rules for a porting slice are in [`CLAUDE.md`](CLAUDE.md).

## Credits and license

- **Steam Audio** is developed by [Valve Corporation](https://github.com/ValveSoftware/steam-audio) and released under the Apache License 2.0. Official documentation: <https://valvesoftware.github.io/steam-audio/>.
- This reimplementation is maintained by **razed.production**.
- Developed with [Claude Code](https://claude.com/claude-code) (Anthropic) as an AI pair programmer; AI-assisted commits carry a `Co-Authored-By: Claude` trailer.
- This project is not affiliated with or endorsed by Valve Corporation.

Licensed under the [Apache License 2.0](LICENSE), consistent with the upstream project.
