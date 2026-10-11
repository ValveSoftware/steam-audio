//! Parity tests for stage 2.7 against golden data from `tools/golden/src/dsp.cpp`. The upstream Catch test
//! `IirFilter.test.cpp` is ported as a unit test in `src/iir.rs` (`Bands.test.cpp` is empty).
//!
//! Inputs are rebuilt from the generator's `Lcg` draws in the same order.

use parity::{Element, Lcg, Tolerance, compare, golden};
use steam_audio::bands::{self, NUM_BANDS};
use steam_audio::delay::{Allpass, Delay};
use steam_audio::iir::{Iir, IirFilterer};
use steam_audio::window;

const SAMPLING_RATES: [u32; 3] = [44100, 48000, 24000];
const DESIGNS_PER_RATE: usize = 6;
const NUM_SPECTRUM_FREQUENCIES: usize = 9;

#[track_caller]
fn check<T: Element + Into<f64>>(name: &str, actual: &[T], tol: impl Fn(&[T]) -> Tolerance) {
    let expected = golden::<T>(name);
    if let Err(m) = compare(actual, &expected.data, tol(&expected.data)) {
        panic!("{name}: {m}");
    }
}

fn exact(_: &[f32]) -> Tolerance {
    Tolerance::EXACT
}

fn f32_tol(_: &[f32]) -> Tolerance {
    Tolerance::F32
}

fn signal(expected: &[f32]) -> Tolerance {
    Tolerance::peak_relative(expected, 1e-5)
}

fn noise(lcg: &mut Lcg, n: usize) -> Vec<f32> {
    (0..n).map(|_| lcg.next_signed()).collect()
}

fn coeffs(f: &Iir) -> [f32; 5] {
    [f.a1, f.a2, f.b0, f.b1, f.b2]
}

#[test]
fn bands() {
    check("bands/low_cutoff", &bands::LOW_CUTOFF_FREQUENCIES, exact);
    check("bands/high_cutoff", &bands::HIGH_CUTOFF_FREQUENCIES, exact);
    check("bands/band_pass_norm", &bands::BAND_PASS_NORM, exact);
}

/// Ten designs per parameter set, in the generator's order.
fn designs(lcg: &mut Lcg) -> Vec<Iir> {
    let mut filters = Vec::new();
    for sr in SAMPLING_RATES {
        for _ in 0..DESIGNS_PER_RATE {
            let u = lcg.next_signed();
            let cutoff = 20.0 + 5000.0 * (u + 1.0);
            let v = lcg.next_signed();
            let q = 0.3 + 2.0 * (v + 1.0);
            let w = lcg.next_signed();
            let gain = 0.05 + 2.0 * (w + 1.0);
            let a = lcg.next_signed();
            let low = 50.0 + 2000.0 * (a + 1.0);
            let b = lcg.next_signed();
            let high = low * (1.5 + 2.0 * (b + 1.0));

            filters.extend([
                Iir::low_pass(cutoff, sr),
                Iir::low_pass_q(cutoff, q, sr),
                Iir::high_pass(cutoff, sr),
                Iir::high_pass_q(cutoff, q, sr),
                Iir::band_pass(low, high, sr),
                Iir::low_shelf(cutoff, gain, sr),
                Iir::low_shelf_q(cutoff, q, gain, sr),
                Iir::high_shelf(cutoff, gain, sr),
                Iir::high_shelf_q(cutoff, q, gain, sr),
                Iir::peaking(low, high, gain, sr),
            ]);
        }
    }
    filters
}

#[test]
fn iir_designs() {
    let filters = designs(&mut Lcg::new(27));

    let flat: Vec<f32> = filters.iter().flat_map(coeffs).collect();
    check("iir/designs", &flat, f32_tol);

    let spectrum: Vec<f32> = filters
        .iter()
        .flat_map(|f| (0..NUM_SPECTRUM_FREQUENCIES).map(|j| f.spectrum(j as f32 * 0.39)))
        .collect();
    check("iir/spectrum", &spectrum, f32_tol);

    let band: Vec<f32> = SAMPLING_RATES
        .iter()
        .flat_map(|&sr| Iir::band_filters(sr))
        .flat_map(|f| coeffs(&f))
        .collect();
    check("iir/band_filters", &band, f32_tol);
}

const FRAMES: [usize; 6] = [37, 64, 3, 8, 1, 100];
const NUM_SINGLES: usize = 5;
const RUN_LENGTH: usize = 300;
const COPY_LENGTH: usize = 40;
const RESET_LENGTH: usize = 40;

/// The generator's `filterRun`: frames, single samples, the rest; a copy continuing from the state; a reset.
fn filter_run(filter: Iir, seed: u32) -> Vec<f32> {
    let mut f = IirFilterer::new(filter);
    let mut g = IirFilterer::new(filter);
    let x = noise(&mut Lcg::new(seed), RUN_LENGTH + COPY_LENGTH + RESET_LENGTH);
    let mut y = vec![0.0; x.len()];

    let mut pos = 0;
    for n in FRAMES {
        f.apply(&x[pos..pos + n], &mut y[pos..pos + n]);
        pos += n;
    }
    for _ in 0..NUM_SINGLES {
        y[pos] = f.apply_sample(x[pos]);
        pos += 1;
    }
    f.apply(&x[pos..RUN_LENGTH], &mut y[pos..RUN_LENGTH]);
    pos = RUN_LENGTH;

    g.copy_state(&f);
    g.apply(&x[pos..pos + COPY_LENGTH], &mut y[pos..pos + COPY_LENGTH]);
    pos += COPY_LENGTH;

    f.reset();
    y[pos..].copy_from_slice(&x[pos..]);
    f.apply_in_place(&mut y[pos..]);

    y
}

fn run_filters() -> Vec<Iir> {
    let mut filters = Iir::band_filters(44100).to_vec();
    filters.push(Iir::low_shelf(
        bands::HIGH_CUTOFF_FREQUENCIES[0],
        0.3,
        44100,
    ));
    filters.push(Iir::peaking(
        bands::LOW_CUTOFF_FREQUENCIES[1],
        bands::HIGH_CUTOFF_FREQUENCIES[1],
        2.5,
        44100,
    ));
    filters.push(Iir::high_shelf(
        bands::LOW_CUTOFF_FREQUENCIES[2],
        0.7,
        48000,
    ));
    filters.extend_from_slice(&designs(&mut Lcg::new(27))[..10]);
    assert_eq!(filters.len(), NUM_BANDS + 13);
    filters
}

#[test]
fn iir_filtering() {
    let filters = run_filters();
    let flat: Vec<f32> = filters.iter().flat_map(coeffs).collect();
    check("iir/run_filters", &flat, f32_tol);

    let runs: Vec<f32> = filters
        .iter()
        .enumerate()
        .flat_map(|(i, &f)| filter_run(f, 100 + i as u32))
        .collect();

    // The port uses upstream's 8-sample (AVX) recurrence.
    check("iir/run_float8", &runs, signal);
    // Upstream's 4-sample recurrence (CPUs without AVX) rounds differently: measured worst error 3.0e-6 x peak.
    check("iir/run_float4", &runs, signal);
}

const DELAY_FRAMES: [usize; 11] = [5, 5, 3, 7, 12, 1, 5, 5, 11, 2, 9];

fn apply_blocks(a: &mut Allpass, lcg: &mut Lcg, num_blocks: usize) -> Vec<f32> {
    (0..num_blocks)
        .flat_map(|_| {
            let x: [f32; 4] = std::array::from_fn(|_| lcg.next_signed());
            a.apply_block(x)
        })
        .collect()
}

fn apply_singles(a: &mut Allpass, lcg: &mut Lcg, n: usize) -> Vec<f32> {
    (0..n).map(|_| a.apply(lcg.next_signed())).collect()
}

#[test]
fn delay_and_allpass() {
    let mut lcg = Lcg::new(37);

    let mut delayed = Vec::new();
    let mut d = Delay::new(7, 5);
    for _ in 0..2 {
        for n in DELAY_FRAMES {
            let mut y = vec![0.0; n];
            d.get(&mut y);
            delayed.extend(y);
            d.put(&noise(&mut lcg, n));
        }
        d.reset();
    }
    check("delay/delay", &delayed, exact);

    let mut allpass = Vec::new();
    let mut a = Allpass::new(13, 0.5, 3);
    allpass.extend(apply_blocks(&mut a, &mut lcg, 10));
    allpass.extend(apply_singles(&mut a, &mut lcg, 7));
    allpass.extend(apply_blocks(&mut a, &mut lcg, 5));
    a.reset();
    allpass.extend(apply_blocks(&mut a, &mut lcg, 10));
    check("delay/allpass", &allpass, f32_tol);

    let mut short = Vec::new();
    let mut b = Allpass::new(2, 0.7, 0);
    short.extend(apply_blocks(&mut b, &mut lcg, 5));
    short.extend(apply_singles(&mut b, &mut lcg, 3));
    short.extend(apply_blocks(&mut b, &mut lcg, 2));
    check("delay/allpass_short", &short, f32_tol);

    let mut c = Allpass::new(225, 0.5, 0);
    check(
        "delay/allpass_reverb",
        &apply_blocks(&mut c, &mut lcg, 100),
        f32_tol,
    );
}

const WINDOW_SIZES: [usize; 6] = [1, 2, 5, 31, 64, 257];
const TUKEY: [(usize, usize); 6] = [(64, 16), (37, 9), (5, 2), (4, 1), (3, 5), (0, 3)];

type WindowFn = fn(&mut [f32]);

#[test]
fn windows() {
    let windows: [(&str, WindowFn); 6] = [
        ("rectangular", window::rectangular),
        ("bartlett", window::bartlett),
        ("hann", window::hann),
        ("hamming", window::hamming),
        ("blackman", window::blackman),
        ("blackman_harris", window::blackman_harris),
    ];
    for (name, f) in windows {
        let all: Vec<f32> = WINDOW_SIZES
            .iter()
            .flat_map(|&n| {
                let mut w = vec![0.0; n];
                f(&mut w);
                w
            })
            .collect();
        check(&format!("window/{name}"), &all, f32_tol);
    }

    let tukey: Vec<f32> = TUKEY
        .iter()
        .flat_map(|&(size, overlap)| {
            let mut w = vec![0.0; size + overlap];
            window::tukey(size, overlap, &mut w);
            w
        })
        .collect();
    check("window/tukey", &tukey, f32_tol);
}
