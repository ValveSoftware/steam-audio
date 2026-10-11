//! Delay lines and allpass filters (upstream `delay.h`, `float8_delay.cpp`).

/// A ring buffer read and written in frames (upstream `Delay`).
///
/// Reads and writes have independent cursors, both starting at the beginning of a zeroed ring of
/// `delay + frame_size` samples. Reading a frame and then writing one, every frame, delays the signal by the ring
/// length.
#[derive(Debug, Clone, Default)]
pub struct Delay {
    ring: Vec<f32>,
    cursor: usize,
    read_cursor: usize,
}

impl Delay {
    /// A zeroed delay line of `delay + frame_size` samples.
    pub fn new(delay: usize, frame_size: usize) -> Self {
        let mut d = Self::default();
        d.resize(delay, frame_size);
        d
    }

    /// Reallocates the ring to `delay + frame_size` samples and resets it.
    pub fn resize(&mut self, delay: usize, frame_size: usize) {
        self.ring.resize(delay + frame_size, 0.0);
        self.reset();
    }

    /// Zeroes the ring and rewinds both cursors.
    pub fn reset(&mut self) {
        self.ring.fill(0.0);
        self.cursor = 0;
        self.read_cursor = 0;
    }

    /// Ring length in samples.
    pub fn len(&self) -> usize {
        self.ring.len()
    }

    /// Whether the ring has no samples (a default-constructed delay line).
    pub fn is_empty(&self) -> bool {
        self.ring.is_empty()
    }

    /// Reads `out.len()` samples at the read cursor and advances it, wrapping around the ring.
    ///
    /// # Panics
    ///
    /// If the ring is empty and `out` is not.
    pub fn get(&mut self, out: &mut [f32]) {
        let mut done = 0;
        while done < out.len() {
            assert!(!self.ring.is_empty(), "read from an empty delay line");
            let n = (out.len() - done).min(self.ring.len() - self.read_cursor);
            out[done..done + n].copy_from_slice(&self.ring[self.read_cursor..self.read_cursor + n]);
            done += n;
            self.read_cursor = (self.read_cursor + n) % self.ring.len();
        }
    }

    /// Writes `input` at the write cursor and advances it, wrapping around the ring.
    ///
    /// # Panics
    ///
    /// If the ring is empty and `input` is not.
    pub fn put(&mut self, input: &[f32]) {
        let mut done = 0;
        while done < input.len() {
            assert!(!self.ring.is_empty(), "write to an empty delay line");
            let n = (input.len() - done).min(self.ring.len() - self.cursor);
            self.ring[self.cursor..self.cursor + n].copy_from_slice(&input[done..done + n]);
            done += n;
            self.cursor = (self.cursor + n) % self.ring.len();
        }
    }
}

/// Schroeder allpass filter `y[n] = -g v[n] + v[n - D]`, `v[n] = x[n] + g v[n - D]` (upstream `Allpass`).
///
/// The default value has an empty delay line and must be [`resize`](Self::resize)d before use.
#[derive(Debug, Clone, Default)]
pub struct Allpass {
    delay: Delay,
    b0: f32,
    am: f32,
}

impl Allpass {
    /// An allpass filter over a zeroed delay line of `delay + frame_size` samples.
    ///
    /// Upstream's constructor calls `reset()` after `resize()`, which zeroes the gain; this one keeps it (see
    /// `docs/DEVIATIONS.md`).
    pub fn new(delay: usize, gain: f32, frame_size: usize) -> Self {
        let mut a = Self::default();
        a.resize(delay, gain, frame_size);
        a
    }

    /// Reallocates and zeroes the delay line, and sets the gain.
    pub fn resize(&mut self, delay: usize, gain: f32, frame_size: usize) {
        self.delay.resize(delay, frame_size);
        self.b0 = -gain;
        self.am = -gain;
    }

    /// Zeroes the delay line **and the gain**, as upstream does, which turns the filter into a plain delay until the
    /// next [`resize`](Self::resize). `ReverbEffect` relies on this.
    pub fn reset(&mut self) {
        self.delay.reset();
        self.b0 = 0.0;
        self.am = 0.0;
    }

    /// Filters one sample.
    pub fn apply(&mut self, x: f32) -> f32 {
        let mut vm = [0.0];
        self.delay.get(&mut vm);
        let v = x - self.am * vm[0];
        self.delay.put(&[v]);
        self.b0 * v + vm[0]
    }

    /// Filters a block of `N` samples at once (upstream's `float4_t` and `float8_t` overloads): the whole block is
    /// read from the delay line before any of it is written, which differs from `N` calls to
    /// [`apply`](Self::apply) when the delay line is shorter than `N`.
    pub fn apply_block<const N: usize>(&mut self, x: [f32; N]) -> [f32; N] {
        let mut vm = [0.0; N];
        self.delay.get(&mut vm);
        let v: [f32; N] = std::array::from_fn(|i| x[i] - self.am * vm[i]);
        self.delay.put(&v);
        std::array::from_fn(|i| self.b0 * v[i] + vm[i])
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn read_then_write_delays_by_ring_length() {
        let mut d = Delay::new(3, 2);
        let mut out = Vec::new();
        for frame in [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0].chunks(2) {
            let mut y = [0.0; 2];
            d.get(&mut y);
            d.put(frame);
            out.extend(y);
        }
        assert_eq!(out, [0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0]);
    }

    #[test]
    fn blocks_equal_single_samples_when_the_ring_is_long_enough() {
        let mut a = Allpass::new(9, 0.5, 0);
        let mut b = a.clone();
        let x: Vec<f32> = (0..64).map(|i| ((i * 37) % 11) as f32 - 5.0).collect();
        for block in x.chunks(8) {
            let y = a.apply_block::<8>(block.try_into().unwrap());
            let z: Vec<f32> = block.iter().map(|&s| b.apply(s)).collect();
            assert_eq!(y.as_slice(), z.as_slice());
        }
    }
}
