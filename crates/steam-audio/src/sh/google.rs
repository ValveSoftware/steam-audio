//! Port of the Google spherical harmonics library as vendored by upstream (`sh/spherical_harmonics.{h,cc}`,
//! namespace `sh`), originally <https://github.com/google/spherical-harmonics>.
//!
//! Everything here works in the library's own frame (+x forward, +y left, +z up) and evaluates in `f64`, exactly as
//! upstream does. Steam Audio code uses the wrappers in [`crate::sh`] instead, which convert from Steam Audio's
//! frame (+x right, +y up, +z backward).
//!
//! The SH basis is real, without the Condon-Shortley phase, and orthonormal (N3D with 1/√(4π) folded in).
//! Coefficients are stored in ACN order: [`index`]`(l, m) = l(l + 1) + m`.
//!
//! Not ported: `ProjectFunction` (seeds `std::mt19937` from `std::random_device`, so it is not reproducible, and
//! nothing in the library calls it) and the MKL-only `ProjectSparseSamples`. See `docs/DEVIATIONS.md`.

use crate::math::{CoordinateSpace3, DynamicMatrix, Matrix3, PI_F64, Quaternion};

/// Factorials and double factorials below this are looked up instead of computed.
const CACHE_SIZE: usize = 16;

/// Bands up to this one use the hard-coded Cartesian polynomials; higher bands use the recurrence.
const HARD_CODED_ORDER_LIMIT: usize = 4;

/// Number of coefficients for all bands `0..=order`: `(order + 1)²` (upstream `GetCoefficientCount`).
pub fn coefficient_count(order: usize) -> usize {
    (order + 1) * (order + 1)
}

/// ACN index of degree `l` and order `m`: `l(l + 1) + m` (upstream `GetIndex`). Panics unless `-l <= m <= l`.
pub fn index(l: usize, m: i32) -> usize {
    check_lm(l, m);
    (l * (l + 1)).wrapping_add_signed(m as isize)
}

#[track_caller]
fn check_lm(l: usize, m: i32) {
    assert!(
        m.unsigned_abs() as usize <= l,
        "SH order m = {m} is outside [-{l}, {l}]"
    );
}

/// Spherical coordinates → unit vector. `phi` is the azimuth about +z, `theta` the angle down from +z (upstream
/// `ToVector`).
pub fn to_vector(phi: f64, theta: f64) -> [f64; 3] {
    let r = theta.sin();
    [r * phi.cos(), r * phi.sin(), theta.cos()]
}

/// Unit vector → `(phi, theta)`, the inverse of [`to_vector`] (upstream `ToSphericalCoords`). `z` is clamped to
/// `[-1, 1]` so rounding cannot push it outside the domain of `acos`.
pub fn to_spherical_coords(dir: [f64; 3]) -> (f64, f64) {
    let theta = clamp(dir[2], -1.0, 1.0).acos();
    let phi = dir[1].atan2(dir[0]);
    (phi, theta)
}

/// Upstream's local `Clamp`: NaN passes through unchanged.
fn clamp(mut val: f64, min: f64, max: f64) -> f64 {
    if val < min {
        val = min;
    }
    if val > max {
        val = max;
    }
    val
}

/// `|actual - expected| < 32 ε` (upstream `NearByMargin`).
fn near_by_margin(actual: f64, expected: f64) -> bool {
    (actual - expected).abs() < 32.0 * f64::EPSILON
}

/// Basis function `(l, m)` at spherical coordinates (upstream `EvalSH(l, m, phi, theta)`). Bands up to 4 convert
/// to Cartesian and use the hard-coded polynomials; higher bands use [`eval_sh_slow`].
pub fn eval_sh_spherical(l: usize, m: i32, phi: f64, theta: f64) -> f64 {
    if l <= HARD_CODED_ORDER_LIMIT {
        eval_sh(l, m, to_vector(phi, theta))
    } else {
        eval_sh_slow(l, m, phi, theta)
    }
}

/// Basis function `(l, m)` in direction `dir`, which must be a unit vector (upstream `EvalSH(l, m, dir)`). Bands
/// above 4 convert to spherical coordinates and use [`eval_sh_slow`].
///
/// The hard-coded polynomials use upstream's 6-digit constants (`0.282095` for `1/(2√π)` and so on), so they agree
/// with [`eval_sh_slow`] only to about 1e-6 relative.
pub fn eval_sh(l: usize, m: i32, dir: [f64; 3]) -> f64 {
    check_lm(l, m);
    if l > HARD_CODED_ORDER_LIMIT {
        let (phi, theta) = to_spherical_coords(dir);
        return eval_sh_spherical(l, m, phi, theta);
    }

    let [x, y, z] = dir;
    match (l, m) {
        (0, 0) => 0.282095,

        (1, -1) => 0.488603 * y,
        (1, 0) => 0.488603 * z,
        (1, 1) => 0.488603 * x,

        (2, -2) => 1.092548 * x * y,
        (2, -1) => 1.092548 * y * z,
        (2, 0) => 0.315392 * (-x * x - y * y + 2.0 * z * z),
        (2, 1) => 1.092548 * x * z,
        (2, 2) => 0.546274 * (x * x - y * y),

        (3, -3) => 0.590044 * y * (3.0 * x * x - y * y),
        (3, -2) => 2.890611 * x * y * z,
        (3, -1) => 0.457046 * y * (4.0 * z * z - x * x - y * y),
        (3, 0) => 0.373176 * z * (2.0 * z * z - 3.0 * x * x - 3.0 * y * y),
        (3, 1) => 0.457046 * x * (4.0 * z * z - x * x - y * y),
        (3, 2) => 1.445306 * z * (x * x - y * y),
        (3, 3) => 0.590044 * x * (x * x - 3.0 * y * y),

        (4, -4) => 2.503343 * x * y * (x * x - y * y),
        (4, -3) => 1.770131 * y * z * (3.0 * x * x - y * y),
        (4, -2) => 0.946175 * x * y * (7.0 * z * z - 1.0),
        (4, -1) => 0.669047 * y * z * (7.0 * z * z - 3.0),
        (4, 0) => {
            let z2 = z * z;
            0.105786 * (35.0 * z2 * z2 - 30.0 * z2 + 3.0)
        }
        (4, 1) => 0.669047 * x * z * (7.0 * z * z - 3.0),
        (4, 2) => 0.473087 * (x * x - y * y) * (7.0 * z * z - 1.0),
        (4, 3) => 1.770131 * x * z * (x * x - 3.0 * y * y),
        (4, 4) => {
            let (x2, y2) = (x * x, y * y);
            0.625836 * (x2 * (x2 - 3.0 * y2) - y2 * (3.0 * x2 - y2))
        }

        _ => unreachable!("(l, m) checked above"),
    }
}

/// Basis function `(l, m)` from the general formula with associated Legendre polynomials, for any band (upstream
/// `EvalSHSlow`).
pub fn eval_sh_slow(l: usize, m: i32, phi: f64, theta: f64) -> f64 {
    check_lm(l, m);
    let abs_m = m.unsigned_abs() as usize;

    let kml = ((2.0 * l as f64 + 1.0) * factorial(l - abs_m)
        / (4.0 * PI_F64 * factorial(l + abs_m)))
    .sqrt();

    if m > 0 {
        2.0_f64.sqrt()
            * kml
            * (f64::from(m) * phi).cos()
            * eval_legendre_polynomial(l, abs_m, theta.cos())
    } else if m < 0 {
        2.0_f64.sqrt()
            * kml
            * (f64::from(-m) * phi).sin()
            * eval_legendre_polynomial(l, abs_m, theta.cos())
    } else {
        kml * eval_legendre_polynomial(l, 0, theta.cos())
    }
}

/// `Σ coeffs[index(l, m)] · Y(l, m)` over bands `0..=order`, accumulated in `f32` (upstream `EvalSHSum<float>` with
/// a direction). `dir` must be a unit vector. Above order 4 this goes through spherical coordinates, where bands up to
/// 4 are evaluated at `to_vector(to_spherical_coords(dir))`, as upstream does.
pub fn eval_sh_sum(order: usize, coeffs: &[f32], dir: [f64; 3]) -> f32 {
    if order > HARD_CODED_ORDER_LIMIT {
        let (phi, theta) = to_spherical_coords(dir);
        return eval_sh_sum_spherical(order, coeffs, phi, theta);
    }
    sum_bands(order, coeffs, |l, m| eval_sh(l, m, dir))
}

/// As [`eval_sh_sum`], at spherical coordinates (upstream `EvalSHSum<float>(order, coeffs, phi, theta)`).
pub fn eval_sh_sum_spherical(order: usize, coeffs: &[f32], phi: f64, theta: f64) -> f32 {
    if order <= HARD_CODED_ORDER_LIMIT {
        return eval_sh_sum(order, coeffs, to_vector(phi, theta));
    }
    sum_bands(order, coeffs, |l, m| eval_sh_spherical(l, m, phi, theta))
}

fn sum_bands(order: usize, coeffs: &[f32], eval: impl Fn(usize, i32) -> f64) -> f32 {
    let coeffs = &coeffs[..coefficient_count(order)];
    let mut sum = 0.0_f32;
    for l in 0..=order {
        for m in -(l as i32)..=l as i32 {
            sum += eval(l, m) as f32 * coeffs[index(l, m)];
        }
    }
    sum
}

/// `x!` as `f64`, from a table below 16 (upstream `Factorial`).
fn factorial(x: usize) -> f64 {
    const CACHE: [f64; CACHE_SIZE] = [
        1.0,
        1.0,
        2.0,
        6.0,
        24.0,
        120.0,
        720.0,
        5040.0,
        40320.0,
        362880.0,
        3628800.0,
        39916800.0,
        479001600.0,
        6227020800.0,
        87178291200.0,
        1307674368000.0,
    ];

    if x < CACHE_SIZE {
        CACHE[x]
    } else {
        let mut s = 1.0;
        for n in 2..=x {
            s *= n as f64;
        }
        s
    }
}

/// `x!!` as `f64`, from a table below 16 (upstream `DoubleFactorial`).
fn double_factorial(x: usize) -> f64 {
    const CACHE: [f64; CACHE_SIZE] = [
        1.0, 1.0, 2.0, 3.0, 8.0, 15.0, 48.0, 105.0, 384.0, 945.0, 3840.0, 10395.0, 46080.0,
        135135.0, 645120.0, 2027025.0,
    ];

    if x < CACHE_SIZE {
        CACHE[x]
    } else {
        let mut s = 1.0;
        let mut n = x as f64;
        while n > 1.0 {
            s *= n;
            n -= 2.0;
        }
        s
    }
}

/// Associated Legendre polynomial `P(l, m)` at `x`, without the Condon-Shortley phase, for `0 <= m <= l` (upstream
/// `EvalLegendrePolynomial`): starts from `P(m, m)` and lifts it band by band.
fn eval_legendre_polynomial(l: usize, m: usize, x: f64) -> f64 {
    // P(m, m) = (2m - 1)!! (1 - x²)^(m/2)
    let mut pmm = 1.0;
    if m > 0 {
        pmm = double_factorial(2 * m - 1) * (1.0 - x * x).powf(m as f64 / 2.0);
    }
    if l == m {
        return pmm;
    }

    // P(m + 1, m) = x (2m + 1) P(m, m)
    let mut pmm1 = x * (2 * m + 1) as f64 * pmm;
    if l == m + 1 {
        return pmm1;
    }

    // P(n, m) = (x (2n - 1) P(n - 1, m) - (n + m - 1) P(n - 2, m)) / (n - m)
    for n in m + 2..=l {
        let pmn = (x * (2 * n - 1) as f64 * pmm1 - (n + m - 1) as f64 * pmm) / (n - m) as f64;
        pmm = pmm1;
        pmm1 = pmn;
    }
    pmm1
}

/// Rotation of SH coefficients, band by band (upstream `sh::Rotation`).
///
/// Holds one `(2l + 1)²` matrix per band, computed with the recurrence of Ivanic & Ruedenberg (with the errata in
/// the comments of upstream's `V`). Setting a rotation and applying it do not allocate.
#[derive(Debug, Clone)]
pub struct Rotation {
    order: usize,
    band_rotations: Vec<DynamicMatrix>,
}

impl Rotation {
    /// Zero band matrices for bands `0..=order`; call one of the `set_*` methods before [`apply`](Self::apply).
    pub fn new(order: usize) -> Self {
        let band_rotations = (0..=order)
            .map(|l| DynamicMatrix::new(2 * l + 1, 2 * l + 1))
            .collect();
        Self {
            order,
            band_rotations,
        }
    }

    /// The highest band this rotation covers.
    pub fn order(&self) -> usize {
        self.order
    }

    /// The `(2l + 1)×(2l + 1)` matrix for band `l` (upstream `band_rotation`). Panics if `l > order`.
    pub fn band_rotation(&self, l: usize) -> &DynamicMatrix {
        &self.band_rotations[l]
    }

    /// From a unit quaternion, via its rotation matrix (upstream `SetRotation(Quaternionf)`).
    pub fn set_rotation_quaternion(&mut self, quaternion: Quaternion) {
        self.set_rotation_matrix(&quaternion.to_rotation_matrix());
    }

    /// From an orientation whose rows are `ahead`, `-right` and `up` (upstream `SetRotation(CoordinateSpace3f)`).
    pub fn set_rotation(&mut self, orientation: &CoordinateSpace3) {
        let rotation = Matrix3::from_rows([
            orientation.ahead.to_array(),
            (-orientation.right).to_array(),
            orientation.up.to_array(),
        ]);
        self.set_rotation_matrix(&rotation);
    }

    /// From a 3×3 rotation matrix (upstream `SetRotation(Matrix3x3f)`).
    pub fn set_rotation_matrix(&mut self, rotation: &Matrix3) {
        // Band 0 is invariant under rotation.
        self.band_rotations[0][(0, 0)] = 1.0;

        // Band 1 is a permutation of the rotation matrix (without the Condon-Shortley phase).
        if self.order > 0 {
            let band = &mut self.band_rotations[1];
            band[(0, 0)] = rotation[(1, 1)];
            band[(0, 1)] = rotation[(1, 2)];
            band[(0, 2)] = rotation[(1, 0)];
            band[(1, 0)] = rotation[(2, 1)];
            band[(1, 1)] = rotation[(2, 2)];
            band[(1, 2)] = rotation[(2, 0)];
            band[(2, 0)] = rotation[(0, 1)];
            band[(2, 1)] = rotation[(0, 2)];
            band[(2, 2)] = rotation[(0, 0)];
        }

        for l in 2..=self.order {
            compute_band_rotation(l, &mut self.band_rotations);
        }
    }

    /// Rotates bands `0..=min(order, self.order())` of `coeffs` into `result` (upstream `Apply(order, coeffs,
    /// result)`). Coefficients of higher bands in `result` are left untouched. Panics if either slice is shorter
    /// than the coefficient count of the bands rotated.
    pub fn apply(&self, order: usize, coeffs: &[f32], result: &mut [f32]) {
        let mut offset = 0;
        for band in &self.band_rotations[..=order.min(self.order)] {
            let size = band.rows();
            let range = offset..offset + size;
            DynamicMatrix::multiply_vector(band, &coeffs[range.clone()], &mut result[range]);
            offset += size;
        }
    }
}

fn kronecker_delta(i: i32, j: i32) -> f64 {
    if i == j { 1.0 } else { 0.0 }
}

/// Element `(i, j)` of a band matrix, with indices centred so the middle element is `(0, 0)`.
fn centered_element(r: &DynamicMatrix, i: i32, j: i32) -> f64 {
    let offset = (r.rows() as i32 - 1) / 2;
    f64::from(r[((i + offset) as usize, (j + offset) as usize)])
}

/// The previously computed band matrices that band `l` is built from.
struct Bands<'a> {
    l: i32,
    r1: &'a DynamicMatrix,
    previous: &'a DynamicMatrix,
}

impl Bands<'_> {
    /// Helper `P` of Ivanic & Ruedenberg. Only valid where the matching `u`, `v` or `w` coefficient is non-zero.
    fn p(&self, i: i32, a: i32, b: i32) -> f64 {
        let (l, r1, prev) = (self.l, self.r1, self.previous);
        if b == l {
            centered_element(r1, i, 1) * centered_element(prev, a, l - 1)
                - centered_element(r1, i, -1) * centered_element(prev, a, -l + 1)
        } else if b == -l {
            centered_element(r1, i, 1) * centered_element(prev, a, -l + 1)
                + centered_element(r1, i, -1) * centered_element(prev, a, l - 1)
        } else {
            centered_element(r1, i, 0) * centered_element(prev, a, b)
        }
    }

    fn u(&self, m: i32, n: i32) -> f64 {
        self.p(0, m, n)
    }

    fn v(&self, m: i32, n: i32) -> f64 {
        if m == 0 {
            self.p(1, 1, n) + self.p(-1, -1, n)
        } else if m > 0 {
            self.p(1, m - 1, n) * (1.0 + kronecker_delta(m, 1)).sqrt()
                - self.p(-1, -m + 1, n) * (1.0 - kronecker_delta(m, 1))
        } else {
            self.p(1, m + 1, n) * (1.0 - kronecker_delta(m, -1))
                + self.p(-1, -m - 1, n) * (1.0 + kronecker_delta(m, -1)).sqrt()
        }
    }

    fn w(&self, m: i32, n: i32) -> f64 {
        if m == 0 {
            0.0
        } else if m > 0 {
            self.p(1, m + 1, n) + self.p(-1, -m - 1, n)
        } else {
            self.p(1, m - 1, n) - self.p(-1, -m + 1, n)
        }
    }
}

/// The `u`, `v`, `w` coefficients of Ivanic & Ruedenberg (upstream `ComputeUVWCoeff`).
fn compute_uvw_coeff(m: i32, n: i32, l: i32) -> (f64, f64, f64) {
    let d = kronecker_delta(m, 0);
    let denom = if n.abs() == l {
        2.0 * f64::from(l) * (2.0 * f64::from(l) - 1.0)
    } else {
        f64::from((l + n) * (l - n))
    };

    let u = (f64::from((l + m) * (l - m)) / denom).sqrt();
    let v = 0.5
        * ((1.0 + d) * (f64::from(l + m.abs()) - 1.0) * f64::from(l + m.abs()) / denom).sqrt()
        * (1.0 - 2.0 * d);
    let w = -0.5 * (f64::from((l - m.abs() - 1) * (l - m.abs())) / denom).sqrt() * (1.0 - d);
    (u, v, w)
}

/// Fills `rotations[l]` from `rotations[1]` and `rotations[l - 1]` (upstream `ComputeBandRotation`).
fn compute_band_rotation(l: usize, rotations: &mut [DynamicMatrix]) {
    let (previous, rest) = rotations.split_at_mut(l);
    let out = &mut rest[0];
    let bands = Bands {
        l: l as i32,
        r1: &previous[1],
        previous: &previous[l - 1],
    };
    let l = l as i32;

    for m in -l..=l {
        for n in -l..=l {
            let (mut u, mut v, mut w) = compute_uvw_coeff(m, n, l);

            // U, V and W read out of bounds where their coefficient is zero.
            if !near_by_margin(u, 0.0) {
                u *= bands.u(m, n);
            }
            if !near_by_margin(v, 0.0) {
                v *= bands.v(m, n);
            }
            if !near_by_margin(w, 0.0) {
                w *= bands.w(m, n);
            }

            out[((m + l) as usize, (n + l) as usize)] = (u + v + w) as f32;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn legendre_polynomial_closed_forms() {
        let x = 0.3_f64;
        assert_eq!(eval_legendre_polynomial(0, 0, x), 1.0);
        assert_eq!(eval_legendre_polynomial(1, 0, x), x);
        assert!((eval_legendre_polynomial(2, 0, x) - 0.5 * (3.0 * x * x - 1.0)).abs() < 1e-15);
        assert!((eval_legendre_polynomial(1, 1, x) - (1.0 - x * x).sqrt()).abs() < 1e-15);
        assert!((eval_legendre_polynomial(2, 2, x) - 3.0 * (1.0 - x * x)).abs() < 1e-15);
    }

    #[test]
    fn factorials_beyond_cache() {
        assert_eq!(factorial(16), 20_922_789_888_000.0);
        assert_eq!(double_factorial(17), 34_459_425.0);
    }
}
