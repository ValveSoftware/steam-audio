//! Parity tests for stage 3.4 against golden data from `tools/golden/src/ambisonics.cpp`: Ambisonics encode, rotate,
//! panning, binaural and decode effects. The upstream Catch test `AmbisonicsBinauralEffect.test.cpp` only checks for
//! finite output; that is covered by `binaural_output_is_finite` below.
//!
//! Inputs are rebuilt from the generator's `Lcg` draws in the same order. Every case is a script of operations; an
//! operation is a letter, optionally followed by an order digit. Outputs are prefilled with `SENTINEL`, so channels an
//! effect leaves untouched are compared too.

use parity::{Lcg, Tolerance, compare, golden};
use steam_audio::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use steam_audio::effects::{
    AmbisonicsBinauralEffect, AmbisonicsDecodeEffect, AmbisonicsDecodeParams,
    AmbisonicsEncodeEffect, AmbisonicsPanningEffect, AmbisonicsRotateEffect,
};
use steam_audio::hrtf::{HrtfDatabase, HrtfNormalization, HrtfSettings};
use steam_audio::math::{CoordinateSpace3, Vector3};
use steam_audio::sh::num_coeffs_for_order;
use steam_audio::speaker_layout::{SpeakerLayout, SpeakerLayoutType};

const SAMPLING_RATE: u32 = 48000;
const SENTINEL: f32 = 9.0;

/// Tolerance for outputs that go through the FFT (risk R1).
const CONVOLVED: f64 = 1e-5;

fn draw_vector(lcg: &mut Lcg) -> Vector3 {
    let x = lcg.next_signed();
    let y = lcg.next_signed();
    let z = lcg.next_signed();
    Vector3::new(x, y, z)
}

fn draw_orientation(lcg: &mut Lcg) -> CoordinateSpace3 {
    let ahead = draw_vector(lcg).normalize_or_zero();
    CoordinateSpace3::from_ahead(ahead, Vector3::ZERO)
}

fn fill(lcg: &mut Lcg, buffer: &mut AudioBuffer) {
    for s in buffer.as_mut_slice() {
        *s = lcg.next_signed();
    }
}

fn sentinel(num_channels: usize, num_samples: usize) -> AudioBuffer {
    AudioBuffer::from_planar(
        num_channels,
        num_samples,
        vec![SENTINEL; num_channels * num_samples],
    )
}

/// The operations of a script: a letter and the order digit after it, if any.
fn ops(script: &str) -> Vec<(char, usize)> {
    let mut out = Vec::new();
    let mut chars = script.chars().peekable();
    while let Some(op) = chars.next() {
        let order = match chars.peek() {
            Some(d) if d.is_ascii_digit() => chars.next().unwrap().to_digit(10).unwrap() as usize,
            _ => 0,
        };
        out.push((op, order));
    }
    out
}

fn state_code(state: AudioEffectState) -> i32 {
    match state {
        AudioEffectState::TailRemaining => 0,
        AudioEffectState::TailComplete => 1,
    }
}

/// All outputs, flattened, and `[state, tail samples remaining]` rows, as the generator's `Recorder`.
#[derive(Default)]
struct Recorder {
    outputs: Vec<f32>,
    states: Vec<i32>,
}

impl Recorder {
    fn record(
        &mut self,
        out: &AudioBuffer,
        state: AudioEffectState,
        tail_samples_remaining: usize,
    ) {
        self.outputs.extend_from_slice(out.as_slice());
        self.states.push(state_code(state));
        self.states.push(tail_samples_remaining as i32);
    }

    /// `relative` is `None` for `F32`, otherwise a peak-relative tolerance.
    #[track_caller]
    fn check(&self, name: &str, relative: Option<f64>) {
        let expected = golden::<f32>(name);
        let tol = match relative {
            None => Tolerance::F32,
            Some(r) => Tolerance::peak_relative(&expected.data, r),
        };
        if let Err(m) = compare(&self.outputs, &expected.data, tol) {
            panic!("{name}: {m}");
        }
        assert_eq!(
            self.states,
            golden::<i32>(&format!("{name}_states")).data,
            "{name}_states"
        );
    }
}

fn make_database(sampling_rate: u32, frame_size: usize) -> HrtfDatabase {
    let settings = HrtfSettings {
        data: None,
        volume: 0.0,
        normalization: HrtfNormalization::None,
    };
    HrtfDatabase::new(&settings, sampling_rate, frame_size).unwrap()
}

fn make_layout(ty: SpeakerLayoutType) -> SpeakerLayout {
    match ty {
        // Same custom layout as the generator's `kCustomSpeakers`.
        SpeakerLayoutType::Custom => SpeakerLayout::custom(vec![
            Vector3::new(-1.0, 0.0, -1.0),
            Vector3::new(1.0, 0.0, -1.0),
            Vector3::new(0.0, 0.0, 0.0),
            Vector3::new(0.0, 1.0, 0.0),
            Vector3::new(0.3, -0.5, 0.8),
        ]),
        ty => SpeakerLayout::new(ty),
    }
}

const LAYOUTS: [(&str, SpeakerLayoutType); 6] = [
    ("mono", SpeakerLayoutType::Mono),
    ("stereo", SpeakerLayoutType::Stereo),
    ("quad", SpeakerLayoutType::Quadraphonic),
    ("surround51", SpeakerLayoutType::FivePointOne),
    ("surround71", SpeakerLayoutType::SevenPointOne),
    ("custom", SpeakerLayoutType::Custom),
];

/// Name, maximum order, frame size, script (the generator's `ScriptCase`).
type ScriptCase = (&'static str, usize, usize, &'static str);

// ---------------------------------------------------------------------------------------------------------------------
// Encode
// ---------------------------------------------------------------------------------------------------------------------

#[test]
fn encode() {
    const CASES: [ScriptCase; 2] = [
        ("max3", 3, 37, "A3A3A3A1A2A0Z3A3RA3TA3"),
        ("max1", 1, 23, "A3A1A3Z1A0A3"),
    ];
    let mut lcg = Lcg::new(67);

    for (name, max_order, frame_size, script) in CASES {
        let mut effect = AmbisonicsEncodeEffect::new(max_order);
        let mut input = AudioBuffer::new(1, frame_size);
        let mut recorder = Recorder::default();

        for (op, order) in ops(script) {
            match op {
                'A' | 'Z' => {
                    let direction = if op == 'Z' {
                        Vector3::ZERO
                    } else {
                        draw_vector(&mut lcg)
                    };
                    fill(&mut lcg, &mut input);
                    let mut out = sentinel(num_coeffs_for_order(order), frame_size);
                    let state = effect.apply(direction, order, &input, &mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'T' => {
                    let mut out = sentinel(16, frame_size);
                    let state = effect.tail(&mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'R' => effect.reset(),
                _ => unreachable!(),
            }
        }

        recorder.check(&format!("ambisonics_encode_effect/{name}"), None);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Rotate
// ---------------------------------------------------------------------------------------------------------------------

#[test]
fn rotate() {
    const CASES: [ScriptCase; 2] = [
        ("max3", 3, 29, "A3A3A3I3A2A1A0A3RA3TA3"),
        ("max2", 2, 19, "A3A2A3I3A1"),
    ];
    let mut lcg = Lcg::new(71);

    for (name, max_order, frame_size, script) in CASES {
        let audio_settings = AudioSettings::new(SAMPLING_RATE, frame_size);
        let mut effect = AmbisonicsRotateEffect::new(&audio_settings, max_order);
        let mut recorder = Recorder::default();

        for (op, order) in ops(script) {
            match op {
                'A' | 'I' => {
                    let orientation = if op == 'I' {
                        CoordinateSpace3::default()
                    } else {
                        draw_orientation(&mut lcg)
                    };
                    let mut input = AudioBuffer::new(num_coeffs_for_order(order), frame_size);
                    fill(&mut lcg, &mut input);
                    let mut out = sentinel(num_coeffs_for_order(order), frame_size);
                    let state = effect.apply(&orientation, order, &input, &mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'T' => {
                    let mut out = sentinel(16, frame_size);
                    let state = effect.tail(&mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'R' => effect.reset(),
                _ => unreachable!(),
            }
        }

        recorder.check(&format!("ambisonics_rotate_effect/{name}"), None);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Panning
// ---------------------------------------------------------------------------------------------------------------------

#[test]
fn panning() {
    const FRAME_SIZE: usize = 31;
    let mut lcg = Lcg::new(73);

    for max_order in [3, 1] {
        let script = if max_order == 3 {
            "A3A3A2A1A0TA3RA1"
        } else {
            "A1A0A1"
        };

        for (name, ty) in LAYOUTS {
            let layout = make_layout(ty);
            let audio_settings = AudioSettings::new(SAMPLING_RATE, FRAME_SIZE);
            let mut effect = AmbisonicsPanningEffect::new(&audio_settings, &layout, max_order);
            let mut recorder = Recorder::default();

            for (op, order) in ops(script) {
                match op {
                    'A' => {
                        let mut input = AudioBuffer::new(num_coeffs_for_order(order), FRAME_SIZE);
                        fill(&mut lcg, &mut input);
                        let mut out = sentinel(layout.num_speakers(), FRAME_SIZE);
                        let state = effect.apply(order, &input, &mut out);
                        recorder.record(&out, state, effect.num_tail_samples_remaining());
                    }
                    'T' => {
                        let mut out = sentinel(layout.num_speakers(), FRAME_SIZE);
                        let state = effect.tail(&mut out);
                        recorder.record(&out, state, effect.num_tail_samples_remaining());
                    }
                    'R' => effect.reset(),
                    _ => unreachable!(),
                }
            }

            recorder.check(
                &format!("ambisonics_panning_effect/{name}_max{max_order}"),
                Some(1e-5),
            );
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Binaural
// ---------------------------------------------------------------------------------------------------------------------

#[test]
fn binaural() {
    const CASES: [ScriptCase; 4] = [
        ("max3", 3, 64, "A3A3A3TTA2A1TTTRA3A0"),
        ("max1", 1, 100, "A1A1A0TTTA1"),
        ("max2", 2, 77, "A2A1TA2"),
        ("switch_database", 3, 64, "A3A3SA3A2TTSA3A3"),
    ];
    let mut lcg = Lcg::new(79);

    for (name, max_order, frame_size, script) in CASES {
        let database = make_database(SAMPLING_RATE, frame_size);
        let other = make_database(44100, frame_size);
        let mut use_other = false;

        let audio_settings = AudioSettings::new(SAMPLING_RATE, frame_size);
        let mut effect = AmbisonicsBinauralEffect::new(&audio_settings, max_order, &database);
        let mut recorder = Recorder::default();

        for (op, order) in ops(script) {
            match op {
                'A' => {
                    let mut input = AudioBuffer::new(num_coeffs_for_order(order), frame_size);
                    fill(&mut lcg, &mut input);
                    let mut out = sentinel(2, frame_size);
                    let hrtf = if use_other { &other } else { &database };
                    let state = effect.apply(hrtf, order, &input, &mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'T' => {
                    let mut out = sentinel(2, frame_size);
                    let state = effect.tail(&mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'R' => effect.reset(),
                'S' => use_other = !use_other,
                _ => unreachable!(),
            }
        }

        recorder.check(
            &format!("ambisonics_binaural_effect/{name}"),
            Some(CONVOLVED),
        );
    }
}

/// Upstream's `AmbisonicsBinauralEffect.test.cpp`: orders 0–3 at frame sizes 512 and 1024 give finite output.
#[test]
fn binaural_output_is_finite() {
    for frame_size in [512, 1024] {
        let database = make_database(SAMPLING_RATE, frame_size);
        let audio_settings = AudioSettings::new(SAMPLING_RATE, frame_size);
        for order in 0..=3 {
            let mut effect = AmbisonicsBinauralEffect::new(&audio_settings, order, &database);
            let num_coeffs = num_coeffs_for_order(order);
            let mut input = AudioBuffer::new(num_coeffs, frame_size);
            for (i, s) in input.as_mut_slice().iter_mut().enumerate() {
                *s = (i as f32 * 0.01).sin();
            }
            let mut out = AudioBuffer::new(2, frame_size);
            effect.apply(&database, order, &input, &mut out);
            assert!(out.as_slice().iter().all(|s| s.is_finite()));
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Decode
// ---------------------------------------------------------------------------------------------------------------------

#[test]
fn decode() {
    // Name, layout, created with an HRTF, maximum order, frame size, script (the generator's `DecodeCase`).
    const CASES: [(&str, SpeakerLayoutType, bool, usize, usize, &str); 4] = [
        (
            "stereo",
            SpeakerLayoutType::Stereo,
            true,
            3,
            64,
            "P3P3B3B3TP3P1B1B3TTP2N3RB3P3",
        ),
        (
            "stereo_no_hrtf",
            SpeakerLayoutType::Stereo,
            false,
            2,
            48,
            "B2P2B1TP2",
        ),
        (
            "surround51",
            SpeakerLayoutType::FivePointOne,
            true,
            3,
            64,
            "P3B3P1P3TRB2",
        ),
        (
            "stereo_max1",
            SpeakerLayoutType::Stereo,
            true,
            1,
            100,
            "B1B1P0P1TB1TT",
        ),
    ];
    let mut lcg = Lcg::new(83);

    for (name, ty, with_hrtf, max_order, frame_size, script) in CASES {
        let layout = make_layout(ty);
        let database = make_database(SAMPLING_RATE, frame_size);
        let audio_settings = AudioSettings::new(SAMPLING_RATE, frame_size);
        let mut effect = AmbisonicsDecodeEffect::new(
            &audio_settings,
            &layout,
            max_order,
            with_hrtf.then_some(&database),
        );
        let mut recorder = Recorder::default();

        for (op, order) in ops(script) {
            match op {
                'P' | 'B' | 'N' => {
                    let orientation = draw_orientation(&mut lcg);
                    let mut input = AudioBuffer::new(num_coeffs_for_order(order), frame_size);
                    fill(&mut lcg, &mut input);
                    let mut out = sentinel(layout.num_speakers(), frame_size);
                    let params = AmbisonicsDecodeParams {
                        order,
                        orientation,
                        binaural: op != 'P',
                        hrtf: (op != 'N').then_some(&database),
                    };
                    let state = effect.apply(params, &input, &mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'T' => {
                    let mut out = sentinel(layout.num_speakers(), frame_size);
                    let state = effect.tail(&mut out);
                    recorder.record(&out, state, effect.num_tail_samples_remaining());
                }
                'R' => effect.reset(),
                _ => unreachable!(),
            }
        }

        recorder.check(&format!("ambisonics_decode_effect/{name}"), Some(CONVOLVED));
    }
}
