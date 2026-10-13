//! Parity tests for stage 6.1 against golden data from `tools/golden/src/reconstructor.cpp`. The upstream Catch tests
//! `EnergyField.test.cpp` and `ImpulseResponse.test.cpp` are empty.
//!
//! Inputs are rebuilt from the generator's `Lcg` draws in the same order. Sizes and operations are bit-exact everywhere;
//! reconstructed IRs are bit-exact on Windows (debug and release). Off Windows, `expf` (Gaussian bumps and air
//! absorption) may differ by an ulp.

use parity::{Element, Lcg, Tolerance, compare, golden};
use steam_audio::air_absorption::AirAbsorptionModel;
use steam_audio::bands::NUM_BANDS;
use steam_audio::simulation::{
    EnergyField, ImpulseResponse, ReconstructionParams, ReconstructionType, Reconstructor,
};

#[track_caller]
fn check<T: Element + Into<f64>>(name: &str, actual: &[T], tol: impl Fn(&[T]) -> Tolerance) {
    let expected = golden::<T>(name);
    assert_eq!(actual.len(), expected.data.len(), "{name}: length");
    if let Err(m) = compare(actual, &expected.data, tol(&expected.data)) {
        panic!("{name}: {m}");
    }
}

fn fill(lcg: &mut Lcg, data: &mut [f32]) {
    for x in data {
        *x = lcg.next_signed();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Sizes and operations
// ---------------------------------------------------------------------------------------------------------------------

const FIELD_SIZES: [(f32, usize); 7] = [
    (0.0, 0),
    (0.005, 0),
    (0.01, 1),
    (0.0123, 2),
    (0.1, 1),
    (0.999, 3),
    (2.5, 0),
];
const IR_SIZES: [(f32, usize, u32); 7] = [
    (0.0, 0, 48000),
    (0.001, 1, 48000),
    (0.00625, 1, 48000),
    (0.1, 2, 44100),
    (0.0731, 3, 22050),
    (1.0, 0, 24000),
    (0.0537, 1, 48000),
];

#[test]
fn sizes() {
    let mut sizes = Vec::new();
    for (duration, order) in FIELD_SIZES {
        let f = EnergyField::new(duration, order);
        sizes.extend([f.num_channels() as i32, f.num_bins() as i32]);
    }
    for (duration, order, rate) in IR_SIZES {
        let ir = ImpulseResponse::new(duration, order, rate);
        sizes.extend([ir.num_channels() as i32, ir.num_samples() as i32]);
    }
    assert_eq!(sizes, golden::<i32>("reconstructor/sizes").data);
}

/// What the generator's `operations` template needs of both types.
trait Operations: Sized {
    fn data(&self) -> &[f32];
    fn data_mut(&mut self) -> &mut [f32];
    fn add(a: &Self, b: &Self, out: &mut Self);
    fn scale(input: &Self, scalar: f32, out: &mut Self);
    fn scale_accumulate(input: &Self, scalar: f32, out: &mut Self);
    fn copy_from(&mut self, src: &Self);
    fn reset(&mut self);
}

macro_rules! impl_operations {
    ($t:ty) => {
        impl Operations for $t {
            fn data(&self) -> &[f32] {
                <$t>::data(self)
            }
            fn data_mut(&mut self) -> &mut [f32] {
                <$t>::data_mut(self)
            }
            fn add(a: &Self, b: &Self, out: &mut Self) {
                <$t>::add(a, b, out)
            }
            fn scale(input: &Self, scalar: f32, out: &mut Self) {
                <$t>::scale(input, scalar, out)
            }
            fn scale_accumulate(input: &Self, scalar: f32, out: &mut Self) {
                <$t>::scale_accumulate(input, scalar, out)
            }
            fn copy_from(&mut self, src: &Self) {
                <$t>::copy_from(self, src)
            }
            fn reset(&mut self) {
                <$t>::reset(self)
            }
        }
    };
}

impl_operations!(EnergyField);
impl_operations!(ImpulseResponse);

fn operations<T: Operations>(lcg: &mut Lcg, mut a: T, mut b: T, mut c: T) -> Vec<f32> {
    fill(lcg, a.data_mut());
    fill(lcg, b.data_mut());
    fill(lcg, c.data_mut());

    let mut rows = Vec::new();
    T::add(&a, &b, &mut c);
    rows.extend_from_slice(c.data());

    let s1 = lcg.next_signed();
    T::scale(&b, s1, &mut c);
    rows.extend_from_slice(c.data());

    let s2 = lcg.next_signed();
    T::scale_accumulate(&a, s2, &mut c);
    rows.extend_from_slice(c.data());

    let s3 = lcg.next_signed();
    T::scale_accumulate(&c, s3, &mut b);
    rows.extend_from_slice(b.data());

    a.copy_from(&b);
    rows.extend_from_slice(a.data());

    b.copy_from(&c);
    rows.extend_from_slice(b.data());

    a.reset();
    rows.extend_from_slice(a.data());
    rows
}

#[test]
fn operations_match() {
    let mut lcg = Lcg::new(601);

    let fields = operations(
        &mut lcg,
        EnergyField::new(0.05, 1),
        EnergyField::new(0.03, 2),
        EnergyField::new(0.07, 0),
    );
    check("reconstructor/energy_field_ops", &fields, |_| {
        Tolerance::F32
    });

    let irs = operations(
        &mut lcg,
        ImpulseResponse::new(0.0011, 1, 48000),
        ImpulseResponse::new(0.0007, 2, 44100),
        ImpulseResponse::new(0.0013, 0, 22050),
    );
    check("reconstructor/impulse_response_ops", &irs, |_| {
        Tolerance::F32
    });
}

// ---------------------------------------------------------------------------------------------------------------------
// Reconstruction
// ---------------------------------------------------------------------------------------------------------------------

/// Durations are fractions of the rate's maximum duration.
struct IrSpec {
    field_duration: f32,
    field_order: usize,
    ir_duration: f32,
    ir_order: usize,
    correction: bool,
    air_model: usize,
}

const fn spec(
    field_duration: f32,
    field_order: usize,
    ir_duration: f32,
    ir_order: usize,
    correction: bool,
    air_model: usize,
) -> IrSpec {
    IrSpec {
        field_duration,
        field_order,
        ir_duration,
        ir_order,
        correction,
        air_model,
    }
}

struct Call {
    kind: ReconstructionType,
    duration: f32,
    order: usize,
    irs: &'static [IrSpec],
}

use ReconstructionType::{Gaussian, Linear};

const CALLS: [Call; 6] = [
    Call {
        kind: Linear,
        duration: 1.0,
        order: 1,
        irs: &[spec(1.0, 1, 1.0, 1, false, 0)],
    },
    Call {
        kind: Gaussian,
        duration: 1.0,
        order: 2,
        irs: &[
            spec(1.0, 2, 1.0, 2, true, 1),
            spec(0.5, 1, 1.0, 1, false, 0),
        ],
    },
    Call {
        kind: Gaussian,
        duration: 0.6,
        order: 0,
        irs: &[spec(1.0, 2, 1.0, 1, false, 0)],
    },
    Call {
        kind: Linear,
        duration: 1.0,
        order: 2,
        irs: &[
            spec(1.0, 2, 0.7, 1, true, 1),
            spec(0.35, 0, 1.0, 0, false, 1),
        ],
    },
    Call {
        kind: Gaussian,
        duration: 1.0,
        order: 1,
        irs: &[
            spec(1.0, 1, 1.0, 1, true, 0),
            spec(0.8, 1, 0.9, 1, false, 1),
            spec(1.0, 0, 1.0, 1, false, 0),
        ],
    },
    Call {
        kind: Linear,
        duration: 0.25,
        order: 1,
        irs: &[spec(1.0, 1, 1.0, 1, false, 0)],
    },
];

fn fill_energies(lcg: &mut Lcg, f: &mut EnergyField) {
    for i in 0..f.num_channels() {
        for j in 0..NUM_BANDS {
            for k in 0..f.num_bins() {
                let u = lcg.next_signed();
                let z = lcg.next_signed();
                f.band_mut(i, j)[k] = if z < -0.6 {
                    0.0
                } else if z < -0.5 {
                    5e-8
                } else if i == 0 {
                    0.01 * (u + 1.0)
                } else {
                    0.01 * u
                };
            }
        }
    }
}

fn check_rate(sampling_rate: u32, max_duration: f32, seed: u32) {
    let mut lcg = Lcg::new(seed);
    let air_models = [
        AirAbsorptionModel::default(),
        AirAbsorptionModel::Exponential {
            coefficients: [0.05, 0.2, 0.9],
        },
    ];

    let mut reconstructor = Reconstructor::new(max_duration, sampling_rate);

    let mut rows = Vec::new();
    for call in &CALLS {
        let mut inputs = Vec::new();
        for s in call.irs {
            let mut field = EnergyField::new(max_duration * s.field_duration, s.field_order);
            fill_energies(&mut lcg, &mut field);

            let mut ir =
                ImpulseResponse::new(max_duration * s.ir_duration, s.ir_order, sampling_rate);
            fill(&mut lcg, ir.data_mut());

            let curve: Option<Vec<f32>> = s.correction.then(|| {
                (0..ir.num_samples())
                    .map(|_| 1.0 + 0.5 * lcg.next_signed())
                    .collect()
            });
            inputs.push((field, ir, curve, &air_models[s.air_model]));
        }

        for (field, ir, curve, air) in &mut inputs {
            let params = ReconstructionParams {
                kind: call.kind,
                duration: max_duration * call.duration,
                order: call.order,
                air_absorption: air,
                distance_attenuation_correction: curve.as_deref(),
            };
            reconstructor.reconstruct(field, ir, &params);
        }
        for (_, ir, _, _) in &inputs {
            rows.extend_from_slice(ir.data());
        }
    }

    check(
        &format!("reconstructor/reconstruct_{sampling_rate}"),
        &rows,
        |e| Tolerance::peak_relative(e, 1e-5),
    );
}

#[test]
fn reconstruct_48000() {
    check_rate(48000, 0.0337, 611);
}

#[test]
fn reconstruct_44100() {
    check_rate(44100, 0.0461, 612);
}

#[test]
fn reconstruct_22050() {
    check_rate(22050, 0.0593, 613);
}
