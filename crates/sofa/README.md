# sofa

Pure-Rust reader for [SOFA](https://www.sofaconventions.org/) (AES69) HRTF files, used by `steam-audio` for custom
HRTFs. It ports the part of [libmysofa](https://github.com/hoene/libmysofa) 1.3.3 that Steam Audio relies on
([ADR 0001](../../docs/adr/0001-sofa-loader.md)): the HDF5 subset and SOFA loader, `mysofa_check`, resampling with
libmysofa's Speex resampler, coordinate conversions, the k-d tree nearest-measurement search and the neighbourhood of
each measurement. It is checked against libmysofa itself: the three SOFA files in `core/data/hrtf`, libmysofa's own
test files and corrupted variants load (or fail) with the same status codes, contents, resampled impulse responses,
lookups and neighbourhoods, bit for bit on Windows.

```rust
// What Steam Audio does with a custom HRTF (`mysofa_open_no_norm`).
let easy = sofa::Easy::load_file_no_norm("core/data/hrtf/sadie_d1.sofa", 48000.0)?;
println!("{} measurements of {} samples", easy.hrtf.m, easy.filter_length());

let mut direction = [1.0, 0.0, 0.0]; // Cartesian, x ahead
let nearest = easy.lookup.lookup(&mut direction).expect("measurements");
let neighbors = easy.neighborhood.get(nearest).expect("in range");
println!("nearest {nearest}, neighbours {neighbors:?}, at {:?}", sofa::c2s(direction));
# Ok::<(), sofa::Error>(())
```

SOFA files are untrusted input: the crate is safe Rust (no `unsafe`), reports malformed data as an `Error` and
bounds recursion, memory and work where libmysofa would crash or hang (`docs/DEVIATIONS.md`).

## Fuzzing

`fuzz/` holds a [cargo-fuzz](https://github.com/rust-fuzz/cargo-fuzz) target that parses arbitrary bytes. It needs a
nightly toolchain and is not part of the workspace:

```bash
cd crates/sofa
cargo +nightly fuzz run load -- -rss_limit_mb=4096
```

A good seed corpus is the SOFA files in `core/data/hrtf` and libmysofa's `tests/*.sofa`.

## License

Apache-2.0, like the rest of this repository. The crate is derived from libmysofa, Copyright (c) 2016-2017,
Symonics GmbH, Christian Hoene, under the BSD 3-Clause license in [`LICENSE-libmysofa`](LICENSE-libmysofa), which
bundles the Speex resampler (Copyright (C) 2007-2008 Jean-Marc Valin, (C) 2008 Thorvald Natvig) and John Tsiombikas's
`kdtree` (Copyright (C) 2007-2011), both under BSD 3-Clause licenses.
