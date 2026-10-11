use std::fmt;

/// Acceptance band for one value: `|actual - expected| <= abs + rel * |expected|`.
///
/// Float results differ from the reference in the last bits (different FFT, SIMD width,
/// summation order), so exact equality is never required. Each module states its tolerance and
/// the reason for it in `docs/PORTING_PLAN.md`.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Tolerance {
    pub abs: f64,
    pub rel: f64,
}

impl Tolerance {
    /// Exact equality; for integer outputs and pure data movement.
    pub const EXACT: Self = Self { abs: 0.0, rel: 0.0 };

    /// Scalar `f32` math (vectors, matrices, closed-form formulas).
    pub const F32: Self = Self {
        abs: 1e-6,
        rel: 1e-5,
    };

    pub const fn new(abs: f64, rel: f64) -> Self {
        Self { abs, rel }
    }

    /// Absolute tolerance scaled to the peak magnitude of `expected`.
    ///
    /// Suited to signals (FFT, convolution, filters), where per-sample relative error is
    /// meaningless near zero crossings but error relative to the signal level is not.
    pub fn peak_relative<T: Into<f64> + Copy>(expected: &[T], ratio: f64) -> Self {
        let peak = expected.iter().map(|&x| x.into().abs()).fold(0.0, f64::max);
        Self {
            abs: ratio * peak,
            rel: 0.0,
        }
    }

    fn accepts(self, actual: f64, expected: f64) -> bool {
        if actual.is_nan() || expected.is_nan() {
            return actual.is_nan() && expected.is_nan();
        }
        if actual.is_infinite() || expected.is_infinite() {
            return actual == expected;
        }
        (actual - expected).abs() <= self.abs + self.rel * expected.abs()
    }
}

/// Why a comparison failed.
#[derive(Debug, Clone, PartialEq)]
pub enum Mismatch {
    Length {
        actual: usize,
        expected: usize,
    },
    Values {
        /// Number of elements outside tolerance.
        count: usize,
        len: usize,
        /// Element with the largest absolute error.
        worst_index: usize,
        worst_actual: f64,
        worst_expected: f64,
        tolerance: Tolerance,
    },
}

impl fmt::Display for Mismatch {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Length { actual, expected } => {
                write!(f, "length mismatch: actual {actual}, expected {expected}")
            }
            Self::Values {
                count,
                len,
                worst_index,
                worst_actual,
                worst_expected,
                tolerance,
            } => {
                write!(
                    f,
                    "{count} of {len} values out of tolerance (abs {:e}, rel {:e}); \
                     worst at [{worst_index}]: actual {worst_actual:e}, expected {worst_expected:e}, \
                     error {:e}",
                    tolerance.abs,
                    tolerance.rel,
                    (worst_actual - worst_expected).abs()
                )
            }
        }
    }
}

/// Compares two equally long slices element-wise.
pub fn compare<T: Into<f64> + Copy>(
    actual: &[T],
    expected: &[T],
    tol: Tolerance,
) -> Result<(), Mismatch> {
    if actual.len() != expected.len() {
        return Err(Mismatch::Length {
            actual: actual.len(),
            expected: expected.len(),
        });
    }

    let mut count = 0;
    let mut worst: Option<(usize, f64, f64, f64)> = None;
    for (i, (&a, &e)) in actual.iter().zip(expected).enumerate() {
        let (a, e) = (a.into(), e.into());
        if tol.accepts(a, e) {
            continue;
        }
        count += 1;
        // NaN/inf mismatches compare as infinitely bad.
        let err = (a - e).abs();
        let err = if err.is_nan() { f64::INFINITY } else { err };
        if worst.is_none_or(|(_, _, _, w)| err > w) {
            worst = Some((i, a, e, err));
        }
    }

    match worst {
        None => Ok(()),
        Some((worst_index, worst_actual, worst_expected, _)) => Err(Mismatch::Values {
            count,
            len: actual.len(),
            worst_index,
            worst_actual,
            worst_expected,
            tolerance: tol,
        }),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn within_tolerance() {
        let tol = Tolerance::new(1e-3, 0.0);
        assert_eq!(compare(&[1.0f32, 2.0], &[1.0005, 1.9995], tol), Ok(()));
    }

    #[test]
    fn reports_worst_element() {
        let err = compare(&[1.0f64, 2.0, 3.0], &[1.0, 2.5, 3.1], Tolerance::EXACT).unwrap_err();
        match err {
            Mismatch::Values {
                count, worst_index, ..
            } => {
                assert_eq!(count, 2);
                assert_eq!(worst_index, 1);
            }
            other => panic!("unexpected {other:?}"),
        }
    }

    #[test]
    fn nan_only_matches_nan() {
        assert!(compare(&[f32::NAN], &[f32::NAN], Tolerance::EXACT).is_ok());
        assert!(compare(&[f32::NAN], &[0.0], Tolerance::new(1.0, 1.0)).is_err());
    }

    #[test]
    fn peak_relative_scales_with_signal() {
        let expected = [0.0f32, -4.0, 2.0];
        let tol = Tolerance::peak_relative(&expected, 1e-3);
        assert_eq!(tol.abs, 4e-3);
        assert!(compare(&[0.003f32, -4.0, 2.0], &expected, tol).is_ok());
    }

    #[test]
    fn length_mismatch() {
        assert_eq!(
            compare(&[1.0f32], &[1.0, 2.0], Tolerance::F32),
            Err(Mismatch::Length {
                actual: 1,
                expected: 2
            })
        );
    }
}
