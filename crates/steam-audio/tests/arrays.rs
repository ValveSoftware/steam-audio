//! Parity tests for stages 2.2-2.3 against golden data from `tools/golden/src/arrays.cpp`.
//!
//! Inputs are rebuilt from the generator's `Lcg` draws in the same order.

use parity::{Lcg, Tolerance, compare, golden};
use steam_audio::geometry::Sphere;
use steam_audio::math::{DynamicMatrix, Vector3};
use steam_audio::{Complex, array_math, sampling};

const ARRAY_SIZE: usize = 37;
const NUM_SAMPLES: usize = 97;

#[track_caller]
fn check(name: &str, actual: &[f32], tol: Tolerance) {
    let expected = golden::<f32>(name);
    if let Err(m) = compare(actual, &expected.data, tol) {
        panic!("{name}: {m}");
    }
}

fn flatten_vectors(v: &[Vector3]) -> Vec<f32> {
    v.iter().flat_map(|v| v.to_array()).collect()
}

fn flatten_complex(v: &[Complex]) -> Vec<f32> {
    v.iter().flat_map(|c| [c.re, c.im]).collect()
}

fn random_floats(lcg: &mut Lcg, n: usize, scale: f32) -> Vec<f32> {
    (0..n).map(|_| lcg.next_signed() * scale).collect()
}

fn random_complex(lcg: &mut Lcg, n: usize, scale: f32) -> Vec<Complex> {
    (0..n)
        .map(|_| {
            let re = lcg.next_signed() * scale;
            Complex::new(re, lcg.next_signed() * scale)
        })
        .collect()
}

#[test]
fn sampling() {
    let mut sphere = vec![Vector3::ZERO; NUM_SAMPLES];
    let mut hemisphere = vec![Vector3::ZERO; NUM_SAMPLES];
    let mut volume = vec![Vector3::ZERO; NUM_SAMPLES];
    sampling::sphere_samples(&mut sphere);
    sampling::hemisphere_samples(&mut hemisphere);
    sampling::sphere_volume_samples(&mut volume);
    check("sampling/sphere", &flatten_vectors(&sphere), Tolerance::F32);
    check(
        "sampling/hemisphere",
        &flatten_vectors(&hemisphere),
        Tolerance::F32,
    );
    check(
        "sampling/sphere_volume",
        &flatten_vectors(&volume),
        Tolerance::F32,
    );

    let mut lcg = Lcg::new(7);
    let mut transformed_hemisphere = Vec::new();
    let mut transformed_volume = Vec::new();
    for i in 0..NUM_SAMPLES {
        let x = lcg.next_signed();
        let y = lcg.next_signed();
        let z = lcg.next_signed();
        let normal = Vector3::new(x, y, z).normalize_or_zero();
        transformed_hemisphere.push(sampling::transform_hemisphere_sample(hemisphere[i], normal));

        let cx = lcg.next_signed() * 5.0;
        let cy = lcg.next_signed() * 5.0;
        let cz = lcg.next_signed() * 5.0;
        let sphere = Sphere::new(Vector3::new(cx, cy, cz), lcg.next_signed().abs() * 3.0);
        transformed_volume.push(sampling::transform_sphere_volume_sample(volume[i], &sphere));
    }
    check(
        "sampling/transformed_hemisphere",
        &flatten_vectors(&transformed_hemisphere),
        Tolerance::F32,
    );
    check(
        "sampling/transformed_sphere_volume",
        &flatten_vectors(&transformed_volume),
        Tolerance::F32,
    );
}

#[test]
fn array_math() {
    let n = ARRAY_SIZE;
    let mut lcg = Lcg::new(8);
    let a = random_floats(&mut lcg, n, 4.0);
    let b = random_floats(&mut lcg, n, 4.0);
    let ca = random_complex(&mut lcg, n, 2.0);
    let cb = random_complex(&mut lcg, n, 2.0);
    let s = lcg.next_signed() * 3.0;

    let mut t = a.clone();
    t[1] = f32::NAN;
    t[2] = -0.0;
    t[n - 1] = f32::NAN;
    t[n - 2] = -0.0;

    let mut real = vec![0.0; n];
    let mut complex = vec![Complex::default(); n];

    array_math::add(&a, &b, &mut real);
    check("array_math/add", &real, Tolerance::F32);
    array_math::add_complex(&ca, &cb, &mut complex);
    check(
        "array_math/add_complex",
        &flatten_complex(&complex),
        Tolerance::F32,
    );

    array_math::multiply(&a, &b, &mut real);
    check("array_math/multiply", &real, Tolerance::F32);
    array_math::multiply_complex(&ca, &cb, &mut complex);
    check(
        "array_math/multiply_complex",
        &flatten_complex(&complex),
        Tolerance::F32,
    );

    real.copy_from_slice(&b);
    array_math::multiply_accumulate(&a, &b, &mut real);
    check("array_math/multiply_accumulate", &real, Tolerance::F32);
    complex.copy_from_slice(&cb);
    array_math::multiply_accumulate_complex(&ca, &cb, &mut complex);
    check(
        "array_math/multiply_accumulate_complex",
        &flatten_complex(&complex),
        Tolerance::F32,
    );

    array_math::scale(&a, s, &mut real);
    check("array_math/scale", &real, Tolerance::F32);
    array_math::scale_complex(&ca, s, &mut complex);
    check(
        "array_math/scale_complex",
        &flatten_complex(&complex),
        Tolerance::F32,
    );

    real.copy_from_slice(&b);
    array_math::scale_accumulate(&a, s, &mut real);
    check("array_math/scale_accumulate", &real, Tolerance::F32);

    array_math::add_constant(&a, s, &mut real);
    check("array_math/add_constant", &real, Tolerance::F32);

    let (max_value, max_index) = array_math::max_index(&a);
    check(
        "array_math/max",
        &[array_math::max(&a), max_value],
        Tolerance::EXACT,
    );
    assert_eq!(
        golden::<i32>("array_math/max_index").data,
        [max_index as i32]
    );

    array_math::threshold(&t, 0.0, &mut real);
    check("array_math/threshold", &real, Tolerance::EXACT);
    let expected = golden::<f32>("array_math/threshold").data;
    for i in [2, n - 2] {
        assert_eq!(
            real[i].is_sign_negative(),
            expected[i].is_sign_negative(),
            "sign of zero at {i}"
        );
    }

    let positive: Vec<f32> = a.iter().map(|x| x.abs() + 0.01).collect();
    array_math::log(&positive, &mut real);
    check("array_math/log", &real, Tolerance::F32);

    array_math::exp(&a, &mut real);
    check("array_math/exp", &real, Tolerance::F32);
    array_math::exp_complex(&ca, &mut complex);
    check(
        "array_math/exp_complex",
        &flatten_complex(&complex),
        Tolerance::F32,
    );

    array_math::magnitude(&ca, &mut real);
    check("array_math/magnitude", &real, Tolerance::F32);
    array_math::phase(&ca, &mut real);
    check("array_math/phase", &real, Tolerance::F32);

    array_math::polar_to_cartesian(&positive, &a, &mut complex);
    check(
        "array_math/polar_to_cartesian",
        &flatten_complex(&complex),
        Tolerance::F32,
    );
}

#[test]
fn dynamic_matrix() {
    let mut lcg = Lcg::new(9);
    let mut fill = |rows: usize, cols: usize| {
        let mut m = DynamicMatrix::new(rows, cols);
        for i in 0..rows {
            for j in 0..cols {
                m[(i, j)] = lcg.next_signed() * 2.0;
            }
        }
        m
    };
    let a = fill(5, 7);
    let b = fill(7, 3);
    let c = fill(5, 7);
    let v = random_floats(&mut lcg, 7, 1.0);

    let mut product = DynamicMatrix::new(5, 3);
    DynamicMatrix::multiply(&a, &b, &mut product);
    check("dynamic_matrix/product", product.as_slice(), Tolerance::F32);

    let mut mv = vec![0.0; 5];
    DynamicMatrix::multiply_vector(&a, &v, &mut mv);
    check("dynamic_matrix/mul_vector", &mv, Tolerance::F32);

    let mut out = DynamicMatrix::new(5, 7);
    DynamicMatrix::add(&a, &c, &mut out);
    check("dynamic_matrix/sum", out.as_slice(), Tolerance::F32);
    DynamicMatrix::subtract(&a, &c, &mut out);
    check("dynamic_matrix/difference", out.as_slice(), Tolerance::F32);
    DynamicMatrix::scale(&a, 0.3, &mut out);
    check("dynamic_matrix/scaled", out.as_slice(), Tolerance::F32);
}
