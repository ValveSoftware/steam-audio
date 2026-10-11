//! Sampling rate conversion of the impulse responses (upstream `hrtf/resample.c`).

use crate::speex::Resampler;
use crate::{Error, Hrtf};

/// The port's bound on the number of resampled values, 512 MiB of `f32` (`docs/DEVIATIONS.md`).
const MAX_RESAMPLED_VALUES: u64 = 1 << 27;

/// MSVC x64's conversion of a floating-point value to `unsigned int`: through a 64-bit signed integer, keeping the
/// low 32 bits, with NaN and values outside the `i64` range giving 0.
fn msvc_to_u32(value: f64) -> u32 {
    const LIMIT: f64 = 9_223_372_036_854_775_808.0; // 2^63
    if value.is_nan() || !(-LIMIT..LIMIT).contains(&value) {
        0
    } else {
        value as i64 as u32
    }
}

impl Hrtf {
    /// Resamples the impulse responses to `sample_rate` Hz with libmysofa's Speex resampler (upstream
    /// `mysofa_resample`), scaling the delays and updating `n` and the sampling rate.
    ///
    /// Every response is resampled on its own from silence, its start aligned with the original (the filter's
    /// latency is skipped), and is `ceil(n × sample_rate / rate)` samples long. Fails with
    /// [`Error::InvalidFormat`] if `sample_rate` is below 8000 Hz, there is not exactly one sampling rate, or the
    /// responses are not `M × R × N` values; with [`Error::Resampler`] if the resampler rejects the rates (they are
    /// truncated to integers first, as upstream); and with [`Error::ResourceLimit`] beyond the port's limits on the
    /// filter (downsampling by more than 64:1) and output (2^27 values) sizes.
    pub fn resample(&mut self, sample_rate: f32) -> Result<(), Error> {
        let rate = match self.data_sampling_rate.values.as_deref() {
            Some(&[rate]) => rate,
            _ => return Err(Error::InvalidFormat),
        };
        // `DataIR.elements != R * M * N`, in `unsigned int`; a product that only matches after wrapping around is
        // rejected too (upstream would read past the responses).
        let ir_len = self.data_ir.len() as u64;
        let expected = u64::from(self.r) * u64::from(self.m) * u64::from(self.n);
        if sample_rate < 8000.0 || ir_len != expected {
            return Err(Error::InvalidFormat);
        }
        if sample_rate == rate {
            return Ok(());
        }

        let factor = sample_rate / rate;
        let new_n = msvc_to_u32(f64::from(self.n as f32 * factor).ceil());
        let mut resampler = Resampler::new(
            msvc_to_u32(f64::from(rate)),
            msvc_to_u32(f64::from(sample_rate)),
        )?;

        let count = u64::from(self.r) * u64::from(self.m);
        if count * u64::from(new_n) > MAX_RESAMPLED_VALUES {
            return Err(Error::ResourceLimit);
        }
        let (n, new_n) = (self.n as usize, new_n as usize);
        let mut values = vec![0.0; count as usize * new_n];
        if n != 0 {
            const ZERO: [f32; 10] = [0.0; 10];
            let input = self.data_ir.values.as_deref().unwrap_or_default();
            for (ir, out) in input.chunks_exact(n).zip(values.chunks_exact_mut(new_n)) {
                resampler.reset_mem();
                resampler.skip_zeros();
                let (_, mut written) = resampler.process(ir, out);
                // Flush the filter with silence.
                while written < new_n {
                    written += resampler.process(&ZERO, &mut out[written..]).1;
                }
            }
        }
        self.data_ir.values = Some(values);

        if let Some(delays) = &mut self.data_delay.values {
            for delay in delays {
                *delay *= factor;
            }
        }
        self.data_sampling_rate.values = Some(vec![sample_rate]);
        self.n = new_n as u32;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn msvc_conversion() {
        assert_eq!(msvc_to_u32(-1.5), u32::MAX);
        assert_eq!(msvc_to_u32(-0.5), 0);
        assert_eq!(msvc_to_u32(5e9), 705_032_704);
        assert_eq!(msvc_to_u32(-3e9), 1_294_967_296);
        assert_eq!(msvc_to_u32(1e19), 0);
        assert_eq!(msvc_to_u32(f64::INFINITY), 0);
        assert_eq!(msvc_to_u32(f64::NAN), 0);
    }
}
