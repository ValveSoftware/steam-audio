//! Parity tests for stage 2.5 against golden data from `tools/golden/src/fft.cpp`, plus the cases from upstream
//! `FourierTransform.test.cpp`.
//!
//! Inputs are rebuilt from the generator's `Lcg` draws in the same order. Each direction is checked on its own:
//! the inverse transforms are fed the golden spectra, not ours.

use parity::{Lcg, Tolerance, compare, golden};
use steam_audio::Complex;
use steam_audio::fft::{ComplexFft, RealFft};

const SIZES: [usize; 5] = [20, 32, 256, 1024, 4096];

/// Compares against a golden signal or spectrum and returns the golden data.
#[track_caller]
fn check(name: &str, actual: &[f32]) -> Vec<f32> {
    let expected = golden::<f32>(name);
    let tol = Tolerance::peak_relative(&expected.data, 1e-5);
    if let Err(m) = compare(actual, &expected.data, tol) {
        panic!("{name}: {m}");
    }
    expected.data
}

fn flatten(v: &[Complex]) -> Vec<f32> {
    v.iter().flat_map(|c| [c.re, c.im]).collect()
}

fn unflatten(v: &[f32]) -> Vec<Complex> {
    v.chunks_exact(2)
        .map(|c| Complex::new(c[0], c[1]))
        .collect()
}

#[test]
fn fft() {
    for size in SIZES {
        let mut lcg = Lcg::new(size as u32);

        let mut real = RealFft::new(size);
        let mut signal = vec![0.0; real.num_real_samples()];
        for x in &mut signal[..size] {
            *x = lcg.next_signed();
        }

        let mut spectrum = vec![Complex::default(); real.num_complex_samples()];
        real.forward(&signal, &mut spectrum);
        let expected = check(&format!("fft/real_forward_{size}"), &flatten(&spectrum));

        let mut round_trip = vec![0.0; real.num_real_samples()];
        real.inverse(&unflatten(&expected), &mut round_trip);
        check(&format!("fft/real_inverse_{size}"), &round_trip);

        let mut complex = ComplexFft::new(size);
        let mut signal = vec![Complex::default(); complex.num_samples()];
        for x in &mut signal[..size] {
            let re = lcg.next_signed();
            *x = Complex::new(re, lcg.next_signed());
        }

        let mut spectrum = vec![Complex::default(); complex.num_samples()];
        complex.forward(&signal, &mut spectrum);
        let expected = check(&format!("fft/complex_forward_{size}"), &flatten(&spectrum));

        let mut round_trip = vec![Complex::default(); complex.num_samples()];
        complex.inverse(&unflatten(&expected), &mut round_trip);
        check(
            &format!("fft/complex_inverse_{size}"),
            &flatten(&round_trip),
        );
    }
}

/// Catch's `Approx` with its default epsilon (`FLT_EPSILON * 100`) and scale (1).
#[track_caller]
fn approx(actual: f32, expected: f32) {
    let eps = f32::EPSILON * 100.0;
    assert!(
        (actual - expected).abs() <= eps * (1.0 + actual.abs().max(expected.abs())),
        "{actual} != Approx({expected})"
    );
}

fn ramp_20() -> (RealFft, Vec<f32>) {
    let fft = RealFft::new(20);
    let mut signal = vec![0.0; fft.num_real_samples()];
    for (i, x) in signal[..20].iter_mut().enumerate() {
        *x = i as f32 + 1.0;
    }
    (fft, signal)
}

#[test]
fn fourier_transform_computed_correctly() {
    let (mut fft, signal) = ramp_20();
    assert_eq!(fft.num_real_samples(), 32);
    assert_eq!(fft.num_complex_samples(), 17);

    let mut spectrum = vec![Complex::default(); fft.num_complex_samples()];
    fft.forward(&signal, &mut spectrum);

    #[rustfmt::skip]
    let expected = [
        (210.0, 0.0), (-111.881, -69.4845), (46.7185, 1.41779), (-32.2693, 17.5007),
        (7.58579, -26.5563), (6.63635, 19.5842), (-16.0243, -10.4383), (15.9111, -2.6708),
        (-10.0, 10.0), (1.11718, -13.6324), (7.15426, 9.8043), (-10.9873, -3.49605),
        (10.4142, -4.55635), (-4.75235, 9.33214), (-1.84847, -10.3396), (8.22497, 6.46562),
        (-10.0, 0.0),
    ];
    for (s, (re, im)) in spectrum.iter().zip(expected) {
        approx(s.re, re);
        approx(s.im, im);
    }
}

#[test]
fn inverse_of_forward_is_identity() {
    let (mut fft, signal) = ramp_20();
    let mut spectrum = vec![Complex::default(); fft.num_complex_samples()];
    let mut signal2 = vec![0.0; fft.num_real_samples()];
    fft.forward(&signal, &mut spectrum);
    fft.inverse(&spectrum, &mut signal2);
    for (&a, &e) in signal2.iter().zip(&signal) {
        approx(a, e);
    }
}
