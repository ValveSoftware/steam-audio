//! Ports of the MSVC standard library's random number machinery that upstream relies on (risk R2 in
//! `docs/PORTING_PLAN.md`): `std::default_random_engine`, which is `std::mt19937` on MSVC, and
//! `std::uniform_real_distribution<float>` as implemented by MSVC's `<random>` (VS 2022 17.14).

const N: usize = 624;
const M: usize = 397;
const MATRIX_A: u32 = 0x9908_b0df;
const UPPER_MASK: u32 = 0x8000_0000;
const LOWER_MASK: u32 = 0x7fff_ffff;

/// `std::mt19937`. The algorithm is fixed by the C++ standard; [`Mt19937::default`] is a default-constructed engine
/// (seed 5489).
#[derive(Debug, Clone)]
pub(crate) struct Mt19937 {
    state: [u32; N],
    index: usize,
}

impl Mt19937 {
    pub(crate) const DEFAULT_SEED: u32 = 5489;

    pub(crate) fn new(seed: u32) -> Self {
        let mut state = [0; N];
        state[0] = seed;
        for i in 1..N {
            let prev = state[i - 1];
            state[i] = 1_812_433_253u32
                .wrapping_mul(prev ^ (prev >> 30))
                .wrapping_add(i as u32);
        }
        Self { state, index: N }
    }

    pub(crate) fn next_u32(&mut self) -> u32 {
        if self.index >= N {
            self.twist();
        }
        let mut y = self.state[self.index];
        self.index += 1;
        y ^= y >> 11;
        y ^= (y << 7) & 0x9d2c_5680;
        y ^= (y << 15) & 0xefc6_0000;
        y ^ (y >> 18)
    }

    /// `std::uniform_real_distribution<float>(min, max)` applied to this engine. MSVC builds the canonical value
    /// from the top 24 bits of one draw (`generate_canonical<float, 24>`), then scales it as
    /// `u * (max - min) + min`.
    pub(crate) fn uniform_f32(&mut self, min: f32, max: f32) -> f32 {
        let u = (self.next_u32() >> 8) as f32 * (1.0 / 16_777_216.0);
        u * (max - min) + min
    }

    fn twist(&mut self) {
        for i in 0..N {
            let y = (self.state[i] & UPPER_MASK) | (self.state[(i + 1) % N] & LOWER_MASK);
            let mut next = self.state[(i + M) % N] ^ (y >> 1);
            if y & 1 != 0 {
                next ^= MATRIX_A;
            }
            self.state[i] = next;
        }
        self.index = 0;
    }
}

impl Default for Mt19937 {
    fn default() -> Self {
        Self::new(Self::DEFAULT_SEED)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The C++ standard requires the 10000th draw of a default-constructed `mt19937` to be 4123659995.
    #[test]
    fn ten_thousandth_draw() {
        let mut mt = Mt19937::default();
        let x = (0..10_000).map(|_| mt.next_u32()).last();
        assert_eq!(x, Some(4_123_659_995));
    }

    #[test]
    fn uniform_range() {
        let mut mt = Mt19937::default();
        assert!(
            (0..10_000)
                .map(|_| mt.uniform_f32(-1.0, 1.0))
                .all(|x| (-1.0..1.0).contains(&x))
        );
    }
}
