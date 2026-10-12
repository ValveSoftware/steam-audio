//! Air absorption models (upstream `air_absorption.{h,cpp}`).

use std::fmt;
use std::sync::Arc;

use crate::bands::NUM_BANDS;

/// Absorption (a gain) as a function of distance and band index, for an [`AirAbsorptionModel::Callback`].
pub type AirAbsorptionCallback = Arc<dyn Fn(f32, usize) -> f32 + Send + Sync>;

/// How air absorbs sound in each frequency band (upstream `AirAbsorptionModel`).
///
/// Upstream stores coefficients and an optional callback (with a user data pointer), and the callback wins when set;
/// here they are the two variants. Coefficients compare bitwise, like upstream's `memcmp`; two `Callback` models are
/// equal when they share the same `Arc`.
#[derive(Clone)]
pub enum AirAbsorptionModel {
    /// `exp(-coefficients[band] * distance)`.
    Exponential { coefficients: [f32; NUM_BANDS] },
    /// A user-defined function of distance and band.
    Callback(AirAbsorptionCallback),
}

impl AirAbsorptionModel {
    /// Coefficients of the default model, per metre.
    pub const DEFAULT_COEFFICIENTS: [f32; NUM_BANDS] = [0.0002, 0.0017, 0.0182];

    /// Whether this is the default model.
    pub fn is_default(&self) -> bool {
        matches!(self, Self::Exponential { coefficients } if bitwise_eq(coefficients, &Self::DEFAULT_COEFFICIENTS))
    }

    /// Absorption (a gain) in `band` at `distance`.
    ///
    /// # Panics
    ///
    /// If `band` is not below [`NUM_BANDS`] (for the exponential model).
    pub fn evaluate(&self, distance: f32, band: usize) -> f32 {
        match self {
            Self::Exponential { coefficients } => (-coefficients[band] * distance).exp(),
            Self::Callback(callback) => callback(distance, band),
        }
    }
}

fn bitwise_eq(a: &[f32; NUM_BANDS], b: &[f32; NUM_BANDS]) -> bool {
    a.iter().zip(b).all(|(x, y)| x.to_bits() == y.to_bits())
}

impl Default for AirAbsorptionModel {
    fn default() -> Self {
        Self::Exponential {
            coefficients: Self::DEFAULT_COEFFICIENTS,
        }
    }
}

impl PartialEq for AirAbsorptionModel {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (Self::Exponential { coefficients: a }, Self::Exponential { coefficients: b }) => {
                bitwise_eq(a, b)
            }
            (Self::Callback(a), Self::Callback(b)) => Arc::ptr_eq(a, b),
            _ => false,
        }
    }
}

impl fmt::Debug for AirAbsorptionModel {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Exponential { coefficients } => f
                .debug_struct("Exponential")
                .field("coefficients", coefficients)
                .finish(),
            Self::Callback(callback) => f
                .debug_tuple("Callback")
                .field(&Arc::as_ptr(callback))
                .finish(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_model() {
        let model = AirAbsorptionModel::default();
        assert!(model.is_default());
        assert_eq!(model.evaluate(0.0, 2), 1.0);
        assert_eq!(model.evaluate(10.0, 2), (-0.0182f32 * 10.0).exp());
    }

    #[test]
    fn coefficients_compare_bitwise() {
        let a = AirAbsorptionModel::Exponential {
            coefficients: [0.0, 1.0, 2.0],
        };
        let b = AirAbsorptionModel::Exponential {
            coefficients: [-0.0, 1.0, 2.0],
        };
        assert_ne!(a, b);
        assert_eq!(a, a.clone());
    }
}
