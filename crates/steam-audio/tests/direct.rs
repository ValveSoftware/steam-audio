//! Parity tests for stage 4 against golden data from `tools/golden/src/direct.cpp`: distance attenuation, air
//! absorption, directivity, and the gain, EQ and direct effects. Upstream has no Catch tests for these modules.
//!
//! Inputs are rebuilt from the generator's `Lcg` draws in the same order.

use std::sync::Arc;

use parity::{Element, Lcg, Tolerance, compare, golden};
use steam_audio::air_absorption::AirAbsorptionModel;
use steam_audio::audio_buffer::{AudioBuffer, AudioSettings};
use steam_audio::bands::NUM_BANDS;
use steam_audio::directivity::Directivity;
use steam_audio::distance_attenuation::DistanceAttenuationModel;
use steam_audio::effects::{
    DirectEffect, DirectEffectFlags, DirectEffectParams, DirectSoundPath, EqEffect, GainEffect,
    TransmissionType,
};
use steam_audio::math::{CoordinateSpace3, Vector3};

/// Signal tolerance relative to the peak.
const SIGNAL: f64 = 1e-5;

#[track_caller]
fn check<T: Element + Into<f64>>(name: &str, actual: &[T], tol: impl Fn(&[T]) -> Tolerance) {
    let expected = golden::<T>(name);
    if let Err(m) = compare(actual, &expected.data, tol(&expected.data)) {
        panic!("{name}: {m}");
    }
}

#[track_caller]
fn check_exact_i32(name: &str, actual: &[i32]) {
    assert_eq!(actual, golden::<i32>(name).data, "{name}");
}

fn draw_vector(lcg: &mut Lcg) -> Vector3 {
    let x = lcg.next_signed();
    let y = lcg.next_signed();
    let z = lcg.next_signed();
    Vector3::new(x, y, z)
}

/// Uniform in `[lo, hi)`, as the generator's `drawRange`.
fn draw_range(lcg: &mut Lcg, lo: f32, hi: f32) -> f32 {
    let u = lcg.next_signed();
    lo + (hi - lo) * 0.5 * (u + 1.0)
}

fn fill(lcg: &mut Lcg, buffer: &mut AudioBuffer) {
    for s in buffer.as_mut_slice() {
        *s = lcg.next_signed();
    }
}

fn distances(lcg: &mut Lcg) -> Vec<f32> {
    let mut d = vec![0.0, -0.0, 0.5, 1.0, 1.5, -1.0, 1e-3, 100.0, 1e6, 1e-30];
    d.extend((0..200).map(|_| draw_range(lcg, -5.0, 400.0)));
    d
}

/// `[is_default]` and the `models × models` equality matrix, as the generator's `writeIdentity`.
fn identity<M: PartialEq>(models: &[M], is_default: impl Fn(&M) -> bool) -> (Vec<i32>, Vec<i32>) {
    let defaults = models.iter().map(|m| i32::from(is_default(m))).collect();
    let equal = models
        .iter()
        .flat_map(|a| models.iter().map(move |b| i32::from(a == b)))
        .collect();
    (defaults, equal)
}

// ---------------------------------------------------------------------------------------------------------------------
// Distance attenuation
// ---------------------------------------------------------------------------------------------------------------------

#[test]
fn distance_attenuation() {
    let mut lcg = Lcg::new(401);
    let d = distances(&mut lcg);

    let callback = |k: f32| -> Arc<dyn Fn(f32) -> f32 + Send + Sync> {
        Arc::new(move |distance| 1.0 / (1.0 + k * distance * distance))
    };
    let callback1 = callback(0.3);
    let inverse = |min_distance| DistanceAttenuationModel::InverseDistance { min_distance };
    let models = [
        DistanceAttenuationModel::default(),
        inverse(1.0),
        inverse(0.5),
        inverse(3.7),
        inverse(0.0),
        DistanceAttenuationModel::Callback(callback1.clone()),
        DistanceAttenuationModel::Callback(callback1),
        DistanceAttenuationModel::Callback(callback(0.05)),
    ];

    let values: Vec<f32> = models
        .iter()
        .flat_map(|m| d.iter().map(|&x| m.evaluate(x)))
        .collect();
    check("distance_attenuation/evaluate", &values, |_| Tolerance::F32);

    let mut curves = Vec::new();
    for [from, to] in [[0, 2], [0, 5], [3, 0], [7, 2], [2, 3]] {
        for sampling_rate in [44100, 48000, 24000] {
            let mut curve = vec![0.0; 1001];
            DistanceAttenuationModel::correction_curve(
                &models[from],
                &models[to],
                sampling_rate,
                &mut curve,
            );
            curves.extend(curve);
        }
    }
    check("distance_attenuation/correction_curves", &curves, |_| {
        Tolerance::F32
    });

    let (defaults, equal) = identity(&models, DistanceAttenuationModel::is_default);
    check_exact_i32("distance_attenuation/is_default", &defaults);
    check_exact_i32("distance_attenuation/equal", &equal);
}

// ---------------------------------------------------------------------------------------------------------------------
// Air absorption
// ---------------------------------------------------------------------------------------------------------------------

#[test]
fn air_absorption() {
    let mut lcg = Lcg::new(402);
    let d = distances(&mut lcg);

    let mut random = [0.0; NUM_BANDS];
    for c in &mut random {
        *c = draw_range(&mut lcg, 0.0, 0.05);
    }

    let callback = |k: [f32; NUM_BANDS]| -> Arc<dyn Fn(f32, usize) -> f32 + Send + Sync> {
        Arc::new(move |distance, band| 1.0 / (1.0 + k[band] * distance))
    };
    let callback1 = callback([0.01, 0.02, 0.04]);
    let exponential = |coefficients| AirAbsorptionModel::Exponential { coefficients };
    let models = [
        AirAbsorptionModel::default(),
        exponential([0.0002, 0.0017, 0.0182]),
        exponential(random),
        exponential([0.0, 0.0, 0.0]),
        exponential([-0.0, 0.0, 0.0]),
        exponential([-0.01, 0.0, 0.5]),
        AirAbsorptionModel::Callback(callback1.clone()),
        AirAbsorptionModel::Callback(callback1),
        AirAbsorptionModel::Callback(callback([0.1, 0.2, 0.3])),
    ];

    let values: Vec<f32> = models
        .iter()
        .flat_map(|m| {
            d.iter()
                .flat_map(move |&x| (0..NUM_BANDS).map(move |band| m.evaluate(x, band)))
        })
        .collect();
    check("air_absorption/evaluate", &values, |_| Tolerance::F32);

    let (defaults, equal) = identity(&models, AirAbsorptionModel::is_default);
    check_exact_i32("air_absorption/is_default", &defaults);
    check_exact_i32("air_absorption/equal", &equal);
}

// ---------------------------------------------------------------------------------------------------------------------
// Directivity
// ---------------------------------------------------------------------------------------------------------------------

#[test]
fn directivity() {
    let mut lcg = Lcg::new(403);

    let fixed: [(f32, f32); 10] = [
        (0.0, 0.0),
        (0.0, 3.0),
        (0.5, 1.0),
        (1.0, 1.0),
        (1.0, 2.0),
        (0.3, 0.5),
        (0.7, 4.2),
        (-0.5, 1.0),
        (1.5, 2.0),
        (0.5, 0.0),
    ];
    let mut models: Vec<Directivity> = fixed
        .iter()
        .map(|&(weight, power)| Directivity::Dipole { weight, power })
        .collect();
    for _ in 0..20 {
        let weight = draw_range(&mut lcg, 0.0, 1.0);
        let power = draw_range(&mut lcg, 0.0, 4.0);
        models.push(Directivity::Dipole { weight, power });
    }
    let k = 0.25f32;
    models.push(Directivity::Callback(Arc::new(move |d: Vector3| {
        k + d.x + 2.0 * d.y + 3.0 * d.z
    })));

    let mut spaces = vec![CoordinateSpace3::default()];
    for _ in 1..12 {
        let ahead = draw_vector(&mut lcg).normalize_or_zero();
        let origin = draw_vector(&mut lcg) * 10.0;
        spaces.push(CoordinateSpace3::from_ahead(ahead, origin));
    }

    let mut values = Vec::new();
    for space in &spaces {
        let mut points = vec![space.origin];
        points.extend((1..40).map(|_| draw_vector(&mut lcg) * 10.0));
        for model in &models {
            for &point in &points {
                values.push(model.evaluate_at(point, space));
            }
        }
    }
    check("directivity/evaluate_at", &values, |_| Tolerance::F32);
}

// ---------------------------------------------------------------------------------------------------------------------
// Gain effect
// ---------------------------------------------------------------------------------------------------------------------

/// Runs the generator's gain script; `in_place` runs the `'a'` operations in place too.
fn gain_run(frame_size: usize, lcg: &mut Lcg, in_place: bool) -> Vec<f32> {
    let mut effect = GainEffect::new(&AudioSettings::new(48000, frame_size));
    let mut input = AudioBuffer::new(1, frame_size);
    let mut buffer = AudioBuffer::new(1, frame_size);

    let mut outputs = Vec::new();
    for op in "aaaiaaattaraaisa".chars() {
        match op {
            'a' | 'i' => {
                let gain = draw_range(lcg, 0.0, 2.0);
                fill(lcg, &mut input);
                if op == 'a' && !in_place {
                    effect.apply(gain, &input, &mut buffer);
                } else {
                    buffer.as_mut_slice().copy_from_slice(input.as_slice());
                    effect.apply_in_place(gain, &mut buffer);
                }
            }
            't' => {
                fill(lcg, &mut input);
                effect.tail_apply(&input, &mut buffer);
            }
            's' => {
                effect.tail(&mut buffer);
            }
            'r' => {
                effect.reset();
                continue;
            }
            _ => unreachable!(),
        }
        outputs.extend_from_slice(buffer.as_slice());
    }
    outputs
}

#[test]
fn gain_effect() {
    for in_place in [false, true] {
        let mut lcg = Lcg::new(404);
        for frame_size in [37, 100, 1] {
            let outputs = gain_run(frame_size, &mut lcg, in_place);
            check(&format!("gain_effect/frame{frame_size}"), &outputs, |e| {
                Tolerance::peak_relative(e, SIGNAL)
            });
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// EQ effect
// ---------------------------------------------------------------------------------------------------------------------

fn normalize_gains(lcg: &mut Lcg) {
    let denormal = f32::from_bits(1);
    let tiny = f32::MIN_POSITIVE;
    let nan = f32::NAN;
    let mut cases = vec![
        [0.0, 0.0, 0.0, 2.0],
        [denormal, 0.0, denormal, 2.0],
        [0.0, tiny, 0.0, 3.0],
        [-1.0, -0.5, -2.0, 1.0],
        [1.0, 1.0, 1.0, 0.5],
        [0.01, 1.0, 0.001, 0.7],
        [nan, 0.5, 0.25, 1.0],
        [0.5, nan, 0.25, 1.0],
        [4.0, 2.0, 0.1, 0.25],
    ];
    for _ in 0..200 {
        let mut c = [0.0; 4];
        for v in &mut c {
            *v = draw_range(lcg, 0.0, 1.5);
        }
        cases.push(c);
    }

    let inputs: Vec<f32> = cases.iter().flatten().copied().collect();
    check("eq_effect/normalize_inputs", &inputs, |_| Tolerance::EXACT);

    let mut outputs = Vec::new();
    for c in &cases {
        let mut gains = [c[0], c[1], c[2]];
        let mut overall = c[3];
        EqEffect::normalize_gains(&mut gains, &mut overall);
        outputs.extend(gains);
        outputs.push(overall);
    }
    check("eq_effect/normalize_outputs", &outputs, |_| Tolerance::F32);
}

/// Runs the generator's EQ script; `in_place` runs the `'a'` and `'k'` operations in place too.
fn eq_run(sampling_rate: u32, frame_size: usize, lcg: &mut Lcg, in_place: bool) -> Vec<f32> {
    let mut effect = EqEffect::new(&AudioSettings::new(sampling_rate, frame_size));
    let mut input = AudioBuffer::new(1, frame_size);
    let mut buffer = AudioBuffer::new(1, frame_size);
    let mut gains = [1.0; NUM_BANDS];

    let mut outputs = Vec::new();
    for op in "akaakiaktrakaiatsa".chars() {
        match op {
            'a' | 'k' | 'i' => {
                if op != 'k' {
                    for g in &mut gains {
                        *g = draw_range(lcg, 0.05, 2.0);
                    }
                }
                fill(lcg, &mut input);
                if op == 'i' || in_place {
                    buffer.as_mut_slice().copy_from_slice(input.as_slice());
                    effect.apply_in_place(&gains, &mut buffer);
                } else {
                    effect.apply(&gains, &input, &mut buffer);
                }
            }
            't' => {
                fill(lcg, &mut input);
                effect.tail_apply(&input, &mut buffer);
            }
            's' => {
                effect.tail(&mut buffer);
            }
            'r' => {
                effect.reset();
                continue;
            }
            _ => unreachable!(),
        }
        outputs.extend_from_slice(buffer.as_slice());
    }
    outputs
}

#[test]
fn eq_effect() {
    for in_place in [false, true] {
        let mut lcg = Lcg::new(405);
        normalize_gains(&mut lcg);
        for (sampling_rate, frame_size) in [(48000, 37), (44100, 64), (24000, 100), (48000, 5)] {
            let outputs = eq_run(sampling_rate, frame_size, &mut lcg, in_place);
            check(
                &format!("eq_effect/run_{sampling_rate}_{frame_size}"),
                &outputs,
                |e| Tolerance::peak_relative(e, SIGNAL),
            );
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Direct effect
// ---------------------------------------------------------------------------------------------------------------------

fn draw_path(lcg: &mut Lcg) -> DirectSoundPath {
    let mut path = DirectSoundPath {
        distance_attenuation: draw_range(lcg, 0.0, 1.5),
        ..Default::default()
    };
    for a in &mut path.air_absorption {
        *a = draw_range(lcg, 0.0, 1.0);
    }
    path.delay = draw_range(lcg, 0.0, 1.0);
    path.occlusion = draw_range(lcg, 0.0, 1.0);
    for t in &mut path.transmission {
        *t = draw_range(lcg, 0.0, 1.0);
    }
    path.directivity = draw_range(lcg, 0.0, 1.0);
    path
}

#[test]
fn direct_effect() {
    for in_place in [false, true] {
        let mut lcg = Lcg::new(406);
        for (sampling_rate, frame_size, num_channels) in
            [(48000, 37, 2), (44100, 64, 1), (24000, 20, 3)]
        {
            let settings = AudioSettings::new(sampling_rate, frame_size);
            let mut outputs = Vec::new();
            for transmission_type in [
                TransmissionType::FreqIndependent,
                TransmissionType::FreqDependent,
            ] {
                for flags in 0..32 {
                    let mut effect = DirectEffect::new(&settings, num_channels);
                    let mut input = AudioBuffer::new(num_channels, frame_size);
                    let mut buffer = AudioBuffer::new(num_channels, frame_size);
                    let mut params = DirectEffectParams {
                        flags: DirectEffectFlags::from_bits(flags),
                        transmission_type,
                        ..Default::default()
                    };

                    for op in "aakazarakas".chars() {
                        match op {
                            'a' => params.direct_path = draw_path(&mut lcg),
                            'z' => {
                                params.direct_path = draw_path(&mut lcg);
                                params.direct_path.air_absorption = [0.0; NUM_BANDS];
                                params.direct_path.transmission = [0.0; NUM_BANDS];
                                params.direct_path.occlusion = 0.0;
                            }
                            'k' => {}
                            'r' => {
                                effect.reset();
                                continue;
                            }
                            's' => {
                                effect.tail(&mut buffer);
                                outputs.extend_from_slice(buffer.as_slice());
                                continue;
                            }
                            _ => unreachable!(),
                        }

                        fill(&mut lcg, &mut input);
                        if in_place {
                            buffer.as_mut_slice().copy_from_slice(input.as_slice());
                            effect.apply_in_place(&params, &mut buffer);
                        } else {
                            effect.apply(&params, &input, &mut buffer);
                        }
                        outputs.extend_from_slice(buffer.as_slice());
                    }
                }
            }
            check(
                &format!("direct_effect/run_{sampling_rate}_{frame_size}_{num_channels}"),
                &outputs,
                |e| Tolerance::peak_relative(e, SIGNAL),
            );
        }
    }
}
