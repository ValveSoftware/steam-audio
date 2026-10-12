//! Distance attenuation models (upstream `distance_attenuation.{h,cpp}`).

use std::fmt;
use std::sync::Arc;

use crate::math::cpp_max;
use crate::propagation_medium::SPEED_OF_SOUND;

/// Attenuation as a function of distance, for a [`DistanceAttenuationModel::Callback`].
pub type DistanceAttenuationCallback = Arc<dyn Fn(f32) -> f32 + Send + Sync>;

/// How sound is attenuated with distance (upstream `DistanceAttenuationModel`).
///
/// Upstream stores a minimum distance and an optional callback (with a user data pointer), and the callback wins when
/// set; here they are the two variants. Two `Callback` models are equal when they share the same `Arc`, as upstream
/// compares the callback and user data pointers.
#[derive(Clone)]
pub enum DistanceAttenuationModel {
    /// `1 / max(distance, min_distance)`.
    InverseDistance { min_distance: f32 },
    /// A user-defined function of distance.
    Callback(DistanceAttenuationCallback),
}

impl DistanceAttenuationModel {
    /// Minimum distance of the default model.
    pub const DEFAULT_MIN_DISTANCE: f32 = 1.0;

    /// Whether this is the default model: inverse distance with a minimum distance of 1.
    pub fn is_default(&self) -> bool {
        matches!(self, Self::InverseDistance { min_distance } if *min_distance == Self::DEFAULT_MIN_DISTANCE)
    }

    /// Attenuation (a gain) at `distance`.
    pub fn evaluate(&self, distance: f32) -> f32 {
        match self {
            Self::InverseDistance { min_distance } => 1.0 / cpp_max(distance, *min_distance),
            Self::Callback(callback) => callback(distance),
        }
    }

    /// Fills `curve` with the gain that turns attenuation by `from` into attenuation by `to`, for sound that has
    /// travelled for `i / sampling_rate` seconds at sample `i` (upstream `generateCorrectionCurve`).
    pub fn correction_curve(from: &Self, to: &Self, sampling_rate: u32, curve: &mut [f32]) {
        for (i, c) in curve.iter_mut().enumerate() {
            let t = i as f32 / sampling_rate as f32;
            let r = t * SPEED_OF_SOUND;
            *c = to.evaluate(r) / from.evaluate(r);
        }
    }
}

impl Default for DistanceAttenuationModel {
    fn default() -> Self {
        Self::InverseDistance {
            min_distance: Self::DEFAULT_MIN_DISTANCE,
        }
    }
}

impl PartialEq for DistanceAttenuationModel {
    fn eq(&self, other: &Self) -> bool {
        match (self, other) {
            (
                Self::InverseDistance { min_distance: a },
                Self::InverseDistance { min_distance: b },
            ) => a == b,
            (Self::Callback(a), Self::Callback(b)) => Arc::ptr_eq(a, b),
            _ => false,
        }
    }
}

impl fmt::Debug for DistanceAttenuationModel {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InverseDistance { min_distance } => f
                .debug_struct("InverseDistance")
                .field("min_distance", min_distance)
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
    fn default_is_inverse_distance_from_one_metre() {
        let model = DistanceAttenuationModel::default();
        assert!(model.is_default());
        assert_eq!(model.evaluate(0.5), 1.0);
        assert_eq!(model.evaluate(4.0), 0.25);
        assert!(!DistanceAttenuationModel::InverseDistance { min_distance: 2.0 }.is_default());
    }

    #[test]
    fn callbacks_compare_by_identity() {
        let callback: DistanceAttenuationCallback = Arc::new(|d| 1.0 / (1.0 + d));
        let a = DistanceAttenuationModel::Callback(callback.clone());
        let b = DistanceAttenuationModel::Callback(callback);
        let c = DistanceAttenuationModel::Callback(Arc::new(|d| 1.0 / (1.0 + d)));
        assert_eq!(a, b);
        assert_ne!(a, c);
        assert!(!a.is_default());
        assert_eq!(a.evaluate(1.0), 0.5);
    }
}
