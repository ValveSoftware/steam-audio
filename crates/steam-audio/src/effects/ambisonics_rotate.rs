//! Rotation of an Ambisonics sound field into the listener's orientation (upstream `AmbisonicsRotateEffect`).

use crate::audio_buffer::{AudioBuffer, AudioEffectState, AudioSettings};
use crate::math::CoordinateSpace3;
use crate::sh::{self, ShRotation};

/// Rotates Ambisonics buffers, crossfading sample by sample from the previous frame's rotation to the current one.
#[derive(Debug, Clone)]
pub struct AmbisonicsRotateEffect {
    frame_size: usize,
    max_order: usize,
    rotations: [ShRotation; 2],
    /// Index into `rotations` of the rotation the next frame sets.
    current: usize,
    coeffs: Vec<f32>,
    rotated_coeffs: Vec<f32>,
    rotated_coeffs_prev: Vec<f32>,
}

impl AmbisonicsRotateEffect {
    /// Rotates up to `max_order`.
    pub fn new(audio_settings: &AudioSettings, max_order: usize) -> Self {
        let num_coeffs = sh::num_coeffs_for_order(max_order);
        let mut effect = Self {
            frame_size: audio_settings.frame_size,
            max_order,
            rotations: [ShRotation::new(max_order), ShRotation::new(max_order)],
            current: 0,
            coeffs: vec![0.0; num_coeffs],
            rotated_coeffs: vec![0.0; num_coeffs],
            rotated_coeffs_prev: vec![0.0; num_coeffs],
        };
        effect.reset();
        effect
    }

    /// Both the previous and the current rotation become the identity.
    pub fn reset(&mut self) {
        self.current = 0;
        for rotation in &mut self.rotations {
            rotation.set_rotation(&CoordinateSpace3::default());
        }
    }

    /// Rotates one frame into `orientation`, crossfading linearly from the previous frame's orientation over the
    /// frame.
    ///
    /// Only channels up to `min(order, max_order)` are written; the rest of `out` is left untouched, like upstream.
    ///
    /// # Panics
    ///
    /// If the input or output does not have `num_coeffs_for_order(order)` channels, or either is not one frame long.
    pub fn apply(
        &mut self,
        orientation: &CoordinateSpace3,
        order: usize,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        let num_coeffs = sh::num_coeffs_for_order(order);
        assert_eq!(
            input.num_channels(),
            num_coeffs,
            "input channels must match the order"
        );
        assert_eq!(
            out.num_channels(),
            num_coeffs,
            "output channels must match the order"
        );
        self.process(orientation, order, input, out)
    }

    /// [`apply`](Self::apply) with buffers that may have more channels than `order` needs (upstream
    /// `AmbisonicsDecodeEffect` rotates into a buffer of the maximum order).
    pub(crate) fn process(
        &mut self,
        orientation: &CoordinateSpace3,
        order: usize,
        input: &AudioBuffer<impl AsRef<[f32]>>,
        out: &mut AudioBuffer<impl AsRef<[f32]> + AsMut<[f32]>>,
    ) -> AudioEffectState {
        assert_eq!(
            input.num_samples(),
            self.frame_size,
            "input must be one frame long"
        );
        assert_eq!(
            out.num_samples(),
            self.frame_size,
            "output must be one frame long"
        );

        let order = order.min(self.max_order);
        let num_coeffs = sh::num_coeffs_for_order(order);
        let previous = 1 - self.current;

        self.rotations[self.current].set_rotation(orientation);

        for i in 0..self.frame_size {
            for (j, c) in self.coeffs[..num_coeffs].iter_mut().enumerate() {
                *c = input.channel(j)[i];
            }

            self.rotations[self.current].apply(order, &self.coeffs, &mut self.rotated_coeffs);
            self.rotations[previous].apply(order, &self.coeffs, &mut self.rotated_coeffs_prev);

            let weight = i as f32 / self.frame_size as f32;
            for j in 0..num_coeffs {
                out.channel_mut(j)[i] =
                    (1.0 - weight) * self.rotated_coeffs_prev[j] + weight * self.rotated_coeffs[j];
            }
        }

        self.current = 1 - self.current;

        AudioEffectState::TailComplete
    }

    /// Rotation has no tail: outputs silence.
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
