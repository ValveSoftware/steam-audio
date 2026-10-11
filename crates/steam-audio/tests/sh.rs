//! Parity tests for stage 2.6 against golden data from `tools/golden/src/sh.cpp`, plus the upstream Catch tests
//! of `core/src/test/SphericalHarmonics.test.cpp`.
//!
//! Inputs are rebuilt from the generator's `Lcg` draws in the same order.

use parity::{Element, Lcg, Tolerance, compare, golden};
use steam_audio::math::{CoordinateSpace3, DynamicMatrix, PI, PI_F64, Quaternion, Vector3};
use steam_audio::sh::{self, ShRotation, google};

const MAX_ORDER: usize = 6;
const MAX_COEFFS: usize = (MAX_ORDER + 1) * (MAX_ORDER + 1);
const NUM_RANDOM_DIRECTIONS: usize = 30;
const ROTATION_ORDER: usize = 5;
const ROTATION_COEFFS: usize = (ROTATION_ORDER + 1) * (ROTATION_ORDER + 1);
const NUM_RANDOM_ROTATIONS: usize = 15;

#[track_caller]
fn check<T: Element + Into<f64>>(name: &str, actual: &[T], tol: Tolerance) {
    let expected = golden::<T>(name);
    if let Err(m) = compare(actual, &expected.data, tol) {
        panic!("{name}: {m}");
    }
}

fn draws(lcg: &mut Lcg, n: usize) -> Vec<f32> {
    (0..n).map(|_| lcg.next_signed()).collect()
}

/// The zero vector, the six axes, then random unit vectors, like the generator's `directions`.
fn directions(lcg: &mut Lcg) -> Vec<Vector3> {
    let mut dirs = vec![
        Vector3::ZERO,
        Vector3::X_AXIS,
        -Vector3::X_AXIS,
        Vector3::Y_AXIS,
        -Vector3::Y_AXIS,
        Vector3::Z_AXIS,
        -Vector3::Z_AXIS,
    ];
    for _ in 0..NUM_RANDOM_DIRECTIONS {
        let x = lcg.next_signed();
        let y = lcg.next_signed();
        let z = lcg.next_signed();
        dirs.push(Vector3::new(x, y, z).normalize_or_zero());
    }
    dirs
}

#[test]
fn sh_wrappers() {
    let mut lcg = Lcg::new(17);
    let dirs = directions(&mut lcg);

    let mut evaluate = Vec::new();
    for &dir in &dirs {
        for l in 0..=MAX_ORDER {
            for m in -(l as i32)..=l as i32 {
                evaluate.push(sh::evaluate(l, m, dir));
            }
        }
    }
    check("sh/evaluate", &evaluate, Tolerance::F32);

    // project_single_point writes exactly what evaluate returns.
    let mut point = vec![0.0; MAX_COEFFS];
    for (i, &dir) in dirs.iter().enumerate() {
        sh::project_single_point(dir, MAX_ORDER, &mut point);
        assert_eq!(point, evaluate[i * MAX_COEFFS..(i + 1) * MAX_COEFFS]);
    }

    let mut projected = Vec::new();
    for &dir in &dirs {
        let mut coeffs = draws(&mut lcg, MAX_COEFFS);
        let gain = lcg.next_signed() * 2.0;
        sh::project_single_point_and_update(dir, MAX_ORDER, gain, &mut coeffs);
        projected.extend(coeffs);
    }
    check(
        "sh/project_single_point_and_update",
        &projected,
        Tolerance::F32,
    );

    let sum_coeffs = draws(&mut lcg, MAX_COEFFS);
    let mut sums = Vec::new();
    for &dir in &dirs {
        for order in 0..=MAX_ORDER {
            sums.push(sh::evaluate_sum(order, &sum_coeffs, dir));
        }
    }
    check("sh/evaluate_sum", &sums, Tolerance::F32);

    let xs = draws(&mut lcg, 37);
    let legendre: Vec<f32> = (0..=3)
        .flat_map(|n| xs.iter().map(move |&x| sh::legendre(n, x)))
        .collect();
    check("sh/legendre", &legendre, Tolerance::F32);
}

#[test]
fn sh_basis() {
    let mut lcg = Lcg::new(19);

    let mut angles = vec![(0.0, 0.0), (1.0, PI_F64)];
    for _ in 0..35 {
        let phi = f64::from(lcg.next_signed()) * PI_F64;
        let theta = (f64::from(lcg.next_signed()) + 1.0) * 0.5 * PI_F64;
        angles.push((phi, theta));
    }

    let (mut vectors, mut coords, mut slow, mut fast) = (vec![], vec![], vec![], vec![]);
    for &(phi, theta) in &angles {
        let v = google::to_vector(phi, theta);
        vectors.extend(v);

        let (p, t) = google::to_spherical_coords(v);
        coords.extend([p, t]);

        for l in 0..=MAX_ORDER {
            for m in -(l as i32)..=l as i32 {
                slow.push(google::eval_sh_slow(l, m, phi, theta));
                fast.push(google::eval_sh_spherical(l, m, phi, theta));
            }
        }
    }
    check("sh_basis/to_vector", &vectors, Tolerance::F32);
    check("sh_basis/to_spherical_coords", &coords, Tolerance::F32);
    check("sh_basis/eval_sh_slow", &slow, Tolerance::F32);
    check("sh_basis/eval_sh", &fast, Tolerance::F32);
}

/// Band matrices row by row, then `apply` at the full order and at order 3 into outputs pre-filled with 7.
#[derive(Default)]
struct RotationTables {
    bands: Vec<f32>,
    apply: Vec<f32>,
    apply_order3: Vec<f32>,
}

impl RotationTables {
    fn push(&mut self, lcg: &mut Lcg, rotation: &google::Rotation, sh_rotation: &ShRotation) {
        for l in 0..=ROTATION_ORDER {
            let band = rotation.band_rotation(l);
            for i in 0..band.rows() {
                for j in 0..band.cols() {
                    self.bands.push(band[(i, j)]);
                }
            }
        }

        let coeffs = draws(lcg, ROTATION_COEFFS);
        let mut rotated = vec![7.0; ROTATION_COEFFS];
        sh_rotation.apply(ROTATION_ORDER, &coeffs, &mut rotated);
        self.apply.extend_from_slice(&rotated);

        rotated.fill(7.0);
        sh_rotation.apply(3, &coeffs, &mut rotated);
        self.apply_order3.extend_from_slice(&rotated);
    }

    fn check(&self, prefix: &str) {
        check(
            &format!("sh_rotation/{prefix}_bands"),
            &self.bands,
            Tolerance::F32,
        );
        check(
            &format!("sh_rotation/{prefix}_apply"),
            &self.apply,
            Tolerance::F32,
        );
        check(
            &format!("sh_rotation/{prefix}_apply_order3"),
            &self.apply_order3,
            Tolerance::F32,
        );
    }
}

#[test]
fn sh_rotation() {
    let mut lcg = Lcg::new(23);

    let mut spaces = RotationTables::default();
    for i in 0..=NUM_RANDOM_ROTATIONS {
        let mut space = CoordinateSpace3::default();
        if i > 0 {
            let x = lcg.next_signed();
            let y = lcg.next_signed();
            let z = lcg.next_signed();
            space = CoordinateSpace3::from_ahead(
                Vector3::new(x, y, z).normalize_or_zero(),
                Vector3::ZERO,
            );
        }

        let mut rotation = google::Rotation::new(ROTATION_ORDER);
        rotation.set_rotation(&sh::converted_coordinate_space(&space));
        let mut sh_rotation = ShRotation::new(ROTATION_ORDER);
        sh_rotation.set_rotation(&space);
        spaces.push(&mut lcg, &rotation, &sh_rotation);
    }
    spaces.check("space");

    let mut quaternions = RotationTables::default();
    for i in 0..=NUM_RANDOM_ROTATIONS {
        let mut q = Quaternion::IDENTITY;
        if i > 0 {
            let x = lcg.next_signed();
            let y = lcg.next_signed();
            let z = lcg.next_signed();
            let w = lcg.next_signed();
            q = Quaternion::new(x, y, z, w).normalized();
        }

        let mut rotation = google::Rotation::new(ROTATION_ORDER);
        rotation.set_rotation_quaternion(sh::converted_quaternion(q));
        let mut sh_rotation = ShRotation::new(ROTATION_ORDER);
        sh_rotation.set_rotation_quaternion(q);
        quaternions.push(&mut lcg, &rotation, &sh_rotation);
    }
    quaternions.check("quaternion");
}

// ---- Upstream Catch tests (SphericalHarmonics.test.cpp) ----

/// Catch's `Approx(b).epsilon(eps)` with its default scale of 1.
#[track_caller]
fn assert_approx(a: f64, b: f64, eps: f64) {
    assert!(
        (a - b).abs() < eps * (1.0 + a.abs().max(b.abs())),
        "{a} != approx {b} (epsilon {eps})"
    );
}

#[track_caller]
fn assert_vector_approx(expected: [f64; 3], actual: [f64; 3], eps: f64) {
    for (e, a) in expected.into_iter().zip(actual) {
        assert_approx(e, a, eps);
    }
}

const EPSILON: f64 = 1e-6;
const HARDCODED_ERROR: f64 = 1e-5;

#[test]
fn get_index() {
    let expected = [
        (0, 0),
        (1, -1),
        (1, 0),
        (1, 1),
        (2, -2),
        (2, -1),
        (2, 0),
        (2, 1),
        (2, 2),
    ];
    for (i, (l, m)) in expected.into_iter().enumerate() {
        assert_eq!(google::index(l, m), i);
    }
}

#[test]
fn get_coefficient_count() {
    assert_eq!(google::coefficient_count(0), 1);
    assert_eq!(google::coefficient_count(2), 9);
    assert_eq!(google::coefficient_count(3), 16);
    assert_eq!(sh::num_coeffs_for_order(3), 16);
}

#[test]
fn to_vector() {
    let pi = PI_F64;
    let s = 0.5_f64.sqrt();
    let cases = [
        ([1.0, 0.0, 0.0], 0.0, pi / 2.0),
        ([0.0, 1.0, 0.0], pi / 2.0, pi / 2.0),
        ([0.0, 0.0, 1.0], 0.0, 0.0),
        ([0.5, 0.5, s], pi / 4.0, pi / 4.0),
        ([0.5, 0.5, -s], pi / 4.0, 3.0 * pi / 4.0),
        ([-0.5, 0.5, -s], 3.0 * pi / 4.0, 3.0 * pi / 4.0),
        ([0.5, -0.5, -s], -pi / 4.0, 3.0 * pi / 4.0),
    ];
    for (expected, phi, theta) in cases {
        assert_vector_approx(expected, google::to_vector(phi, theta), EPSILON);
    }
}

#[test]
fn to_spherical_coords() {
    // Upstream compares with `==`; a 1e-15 relative bound keeps this independent of the platform's libm.
    let pi = PI_F64;
    let s = 0.5_f64.sqrt();
    let cases = [
        ([1.0, 0.0, 0.0], 0.0, pi / 2.0),
        ([0.0, 1.0, 0.0], pi / 2.0, pi / 2.0),
        ([0.0, 0.0, 1.0], 0.0, 0.0),
        ([0.5, 0.5, s], pi / 4.0, pi / 4.0),
        ([0.5, 0.5, -s], pi / 4.0, 3.0 * pi / 4.0),
        ([-0.5, 0.5, -s], 3.0 * pi / 4.0, 3.0 * pi / 4.0),
        ([0.5, -0.5, -s], -pi / 4.0, 3.0 * pi / 4.0),
    ];
    for (dir, phi, theta) in cases {
        let (p, t) = google::to_spherical_coords(dir);
        assert_approx(phi, p, 1e-15);
        assert_approx(theta, t, 1e-15);
    }
}

#[test]
fn eval_sh_slow() {
    // Closed forms from Wikipedia's table of real spherical harmonics, with |m| = 1 negated (no Condon-Shortley
    // phase).
    let pi = PI_F64;
    let (phi, theta) = (pi / 4.0, pi / 3.0);
    let [x, y, z] = google::to_vector(phi, theta);
    let eval = |l, m| google::eval_sh_slow(l, m, phi, theta);

    assert_approx(0.5 * (1.0 / pi).sqrt(), eval(0, 0), EPSILON);

    let c1 = (3.0 / (4.0 * pi)).sqrt();
    assert_approx(c1 * y, eval(1, -1), EPSILON);
    assert_approx(c1 * z, eval(1, 0), EPSILON);
    assert_approx(c1 * x, eval(1, 1), EPSILON);

    let c2 = 0.5 * (15.0 / pi).sqrt();
    assert_approx(c2 * x * y, eval(2, -2), EPSILON);
    assert_approx(c2 * y * z, eval(2, -1), EPSILON);
    assert_approx(
        0.25 * (5.0 / pi).sqrt() * (-x * x - y * y + 2.0 * z * z),
        eval(2, 0),
        EPSILON,
    );
    assert_approx(c2 * z * x, eval(2, 1), EPSILON);
    assert_approx(
        0.25 * (15.0 / pi).sqrt() * (x * x - y * y),
        eval(2, 2),
        EPSILON,
    );
}

#[test]
fn eval_sh_hardcoded() {
    let (phi, theta) = (0.4296, 1.73234);
    let d = google::to_vector(phi, theta);
    for l in 0..=4 {
        for m in -(l as i32)..=l as i32 {
            let expected = google::eval_sh_slow(l, m, phi, theta);
            assert_approx(
                expected,
                google::eval_sh_spherical(l, m, phi, theta),
                HARDCODED_ERROR,
            );
            assert_approx(expected, google::eval_sh(l, m, d), HARDCODED_ERROR);
        }
    }
}

#[test]
fn closed_form_z_axis_rotation() {
    // Band rotations about z have a closed form; upstream compares them transposed.
    let alpha = PI / 4.0;
    let rz = Quaternion::new(0.0, 0.0, 0.382_683_43, 0.923_879_5);
    let mut rotation = google::Rotation::new(3);
    rotation.set_rotation_quaternion(rz);

    let c = |k: f32| (k * alpha).cos();
    let s = |k: f32| (k * alpha).sin();
    let expected = [
        DynamicMatrix::from_rows(&[[1.0]]),
        DynamicMatrix::from_rows(&[
            [c(1.0), 0.0, -s(1.0)],
            [0.0, 1.0, 0.0],
            [s(1.0), 0.0, c(1.0)],
        ]),
        DynamicMatrix::from_rows(&[
            [c(2.0), 0.0, 0.0, 0.0, -s(2.0)],
            [0.0, c(1.0), 0.0, -s(1.0), 0.0],
            [0.0, 0.0, 1.0, 0.0, 0.0],
            [0.0, s(1.0), 0.0, c(1.0), 0.0],
            [s(2.0), 0.0, 0.0, 0.0, c(2.0)],
        ]),
        DynamicMatrix::from_rows(&[
            [c(3.0), 0.0, 0.0, 0.0, 0.0, 0.0, -s(3.0)],
            [0.0, c(2.0), 0.0, 0.0, 0.0, -s(2.0), 0.0],
            [0.0, 0.0, c(1.0), 0.0, -s(1.0), 0.0, 0.0],
            [0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0],
            [0.0, 0.0, s(1.0), 0.0, c(1.0), 0.0, 0.0],
            [0.0, s(2.0), 0.0, 0.0, 0.0, c(2.0), 0.0],
            [s(3.0), 0.0, 0.0, 0.0, 0.0, 0.0, c(3.0)],
        ]),
    ];

    for (l, e) in expected.iter().enumerate() {
        let actual = rotation.band_rotation(l);
        assert_eq!((e.rows(), e.cols()), (actual.rows(), actual.cols()));
        for i in 0..e.rows() {
            for j in 0..e.cols() {
                assert_approx(f64::from(e[(i, j)]), f64::from(actual[(j, i)]), EPSILON);
            }
        }
    }
}
