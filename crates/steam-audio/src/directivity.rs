//! Source directivity patterns (upstream `directivity.{h,cpp}`).

use std::fmt;
use std::sync::Arc;

use crate::math::{CoordinateSpace3, Vector3};

/// Gain as a function of a unit direction in the source's local space, for a [`Directivity::Callback`].
pub type DirectivityCallback = Arc<dyn Fn(Vector3) -> f32 + Send + Sync>;

/// How loud a source is in each direction (upstream `Directivity`).
///
/// Upstream stores the dipole parameters and an optional callback (with a user data pointer), and the callback wins
/// when set; here they are the two variants.
#[derive(Clone)]
pub enum Directivity {
    /// `|(1 - weight) + weight * cos θ| ^ power`, where θ is the angle from the source's ahead direction: weight 0 is
    /// omnidirectional, 0.5 a cardioid, 1 a dipole.
    Dipole { weight: f32, power: f32 },
    /// A user-defined function of the direction in the source's local space.
    Callback(DirectivityCallback),
}

impl Directivity {
    /// The gain towards `point` of a source with the local axes `coordinates` (upstream `evaluateAt`).
    pub fn evaluate_at(&self, point: Vector3, coordinates: &CoordinateSpace3) -> f32 {
        if let Self::Dipole { weight, .. } = self {
            if *weight == 0.0 {
                return 1.0;
            }
        }

        let world_space_direction = (point - coordinates.origin).normalize_or_zero();
        let local_space_direction = coordinates.direction_world_to_local(world_space_direction);
        self.evaluate(local_space_direction)
    }

    fn evaluate(&self, direction: Vector3) -> f32 {
        match self {
            Self::Dipole { weight, power } => {
                let cosine = -direction.z;
                ((1.0 - weight) + weight * cosine).abs().powf(*power)
            }
            Self::Callback(callback) => callback(direction),
        }
    }
}

impl Default for Directivity {
    /// Omnidirectional.
    fn default() -> Self {
        Self::Dipole {
            weight: 0.0,
            power: 0.0,
        }
    }
}

impl fmt::Debug for Directivity {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Dipole { weight, power } => f
                .debug_struct("Dipole")
                .field("weight", weight)
                .field("power", power)
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
    fn dipole_patterns() {
        let space = CoordinateSpace3::default();
        let ahead = Vector3::new(0.0, 0.0, -2.0);
        let behind = Vector3::new(0.0, 0.0, 3.0);
        let cardioid = Directivity::Dipole {
            weight: 0.5,
            power: 1.0,
        };
        assert_eq!(cardioid.evaluate_at(ahead, &space), 1.0);
        assert_eq!(cardioid.evaluate_at(behind, &space), 0.0);
        assert_eq!(Directivity::default().evaluate_at(behind, &space), 1.0);
    }

    #[test]
    fn callback_receives_local_direction() {
        let space = CoordinateSpace3::canonical_at(Vector3::new(1.0, 0.0, 0.0));
        let directivity = Directivity::Callback(Arc::new(|d: Vector3| d.x));
        assert_eq!(
            directivity.evaluate_at(Vector3::new(3.0, 0.0, 0.0), &space),
            1.0
        );
    }
}
