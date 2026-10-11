//! Encoding of a mono point source into Ambisonics (upstream `AmbisonicsEncodeEffect`): each channel is the input
//! scaled by a spherical harmonic evaluated in the source's direction.

use crate::audio_buffer::{AudioBuffer, AudioEffectState};
use crate::math::Vector3;
use crate::sh;

/// Encodes a mono signal into Ambisonics (ACN order, N3D / √(4π) normalisation), crossfaded from the previous
/// frame's direction.
#[derive(Debug, Clone)]
pub struct AmbisonicsEncodeEffect {
    max_order: usize,
    prev_direction: Vector3,
}

impl AmbisonicsEncodeEffect {
    /// Encodes up to `max_order`.
    pub fn new(max_order: usize) -> Self {
        Self {
            max_order,
            prev_direction: Vector3::ZERO,
        }
    }

    pub fn reset(&mut self) {
        self.prev_direction = Vector3::ZERO;
    }

    /// Encodes the mono `input` arriving from `direction` (relative to the listener, need not be normalized; a
    /// nearly zero vector encodes to order 0 only) into `out`.
    ///
    /// Only channels up to `min(order, max_order)` are written; the rest of `out` is left untouched, like upstream.
    /// As in [`PanningEffect`](super::PanningEffect) (risk R14), the crossfade factor is `channel / num_samples`, so
    /// each channel uses one constant mix of the current and previous weights. The previous direction starts (and
    /// resets) at the zero vector.
    ///
    /// # Panics
    ///
    /// If the input is not mono, the output does not have `num_coeffs_for_order(order)` channels, or the lengths
    /// differ.
    pub fn apply(
        &mut self,
        direction: Vector3,
        order: usize,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(input.num_samples(), out.num_samples(), "sample count");
        assert_eq!(input.num_channels(), 1, "input must be mono");
        assert_eq!(
            out.num_channels(),
            sh::num_coeffs_for_order(order),
            "output channels must match the order"
        );

        let order = order.min(self.max_order);

        // A (nearly) zero direction becomes the zero vector, which the SH evaluation projects onto order 0.
        let direction_length = direction.length();
        let normalized_direction = if direction_length < Vector3::NEARLY_ZERO {
            Vector3::ZERO
        } else {
            direction / direction_length
        };

        let num_samples = input.num_samples();
        let mut i = 0;
        for l in 0..=order {
            for m in -(l as i32)..=l as i32 {
                let weight = sh::evaluate(l, m, normalized_direction);
                let weight_prev = sh::evaluate(l, m, self.prev_direction);

                let alpha = i as f32 / num_samples as f32;
                let blended_weight = alpha * weight + (1.0 - alpha) * weight_prev;

                for (o, &x) in out.channel_mut(i).iter_mut().zip(input.channel(0)) {
                    *o = blended_weight * x;
                }

                i += 1;
            }
        }

        self.prev_direction = normalized_direction;

        AudioEffectState::TailComplete
    }

    /// Encoding has no tail: outputs silence.
    pub fn tail(
        &mut self,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        out.make_silent();
        AudioEffectState::TailComplete
    }

    pub fn num_tail_samples_remaining(&self) -> usize {
        0
    }
}
