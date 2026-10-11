/// Deterministic input generator, bit-identical to `golden::Lcg` in `tools/golden/src/npy.h`.
///
/// Golden generators seed it to synthesise inputs, so Rust tests can rebuild the same inputs instead of loading them.
#[derive(Debug, Clone)]
pub struct Lcg {
    state: u32,
}

impl Lcg {
    pub fn new(seed: u32) -> Self {
        Self { state: seed }
    }

    pub fn next_u32(&mut self) -> u32 {
        self.state = self
            .state
            .wrapping_mul(1_664_525)
            .wrapping_add(1_013_904_223);
        self.state
    }

    /// Uniform `f32` in `[-1, 1)`: 24 random bits, exactly representable.
    pub fn next_signed(&mut self) -> f32 {
        (self.next_u32() >> 8) as f32 * (1.0 / 8_388_608.0) - 1.0
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn matches_numerical_recipes_sequence() {
        let mut lcg = Lcg::new(0);
        assert_eq!(lcg.next_u32(), 1_013_904_223);
        assert_eq!(lcg.next_u32(), 1_196_435_762);
    }

    #[test]
    fn signed_range() {
        let mut lcg = Lcg::new(42);
        assert!(
            (0..10_000)
                .map(|_| lcg.next_signed())
                .all(|x| (-1.0..1.0).contains(&x))
        );
    }
}
