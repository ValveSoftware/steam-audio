# sofa

Pure-Rust reader for [SOFA](https://www.sofaconventions.org/) (AES69) HRTF files, used by `steam-audio` for custom
HRTFs. It ports the part of [libmysofa](https://github.com/hoene/libmysofa) 1.3.3 that Steam Audio relies on
([ADR 0001](../../docs/adr/0001-sofa-loader.md)) and is checked against libmysofa itself: the three SOFA files in
`core/data/hrtf`, libmysofa's own test files and corrupted variants load (or fail) with the same status codes and
contents, bit for bit.

```rust
let hrtf = sofa::Hrtf::load_file("core/data/hrtf/sadie_d1.sofa")?;
hrtf.check()?;
println!("{} measurements of {} samples", hrtf.m, hrtf.n);
# Ok::<(), sofa::Error>(())
```

SOFA files are untrusted input: the parser is safe Rust (no `unsafe`), reports malformed data as an `Error` and
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

Apache-2.0, like the rest of this repository. The parser is derived from libmysofa, Copyright (c) 2016-2017,
Symonics GmbH, Christian Hoene, under the BSD 3-Clause license in [`LICENSE-libmysofa`](LICENSE-libmysofa).
