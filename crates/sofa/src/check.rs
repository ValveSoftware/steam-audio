//! `SimpleFreeFieldHRIR` validation (upstream `hrtf/check.c`).

use crate::{Array, Error, Hrtf, has_attribute};

/// `fequals` on `float` operands: the difference is taken in `float`.
fn fequals(a: f32, b: f32) -> bool {
    f64::from(a - b).abs() < 0.00001
}

/// `compareValues`: whether `array` holds `size` copies of `compare`.
fn compare_values(array: &Array, compare: &[f32; 3], size: i32) -> bool {
    let Some(values) = &array.values else {
        return false;
    };
    // `elements != elements * size` compares as unsigned.
    if values.len() as u32 != 3i32.wrapping_mul(size) as u32 {
        return false;
    }
    values
        .chunks_exact(3)
        .all(|v| v.iter().zip(compare).all(|(&a, &b)| fequals(a, b)))
}

/// `sscanf(version, "%d.%d.%d", ...)`: the number of fields converted, and their values.
fn scan_version(text: &[u8]) -> (usize, [i32; 3]) {
    let mut fields = [0; 3];
    let mut pos = 0;
    for (index, field) in fields.iter_mut().enumerate() {
        if index > 0 {
            if text.get(pos) != Some(&b'.') {
                return (index, fields);
            }
            pos += 1;
        }
        // `%d` skips white space and accepts a sign.
        while text
            .get(pos)
            .is_some_and(|&c| c == b' ' || (b'\t'..=b'\r').contains(&c))
        {
            pos += 1;
        }
        let negative = text.get(pos) == Some(&b'-');
        if matches!(text.get(pos), Some(b'-' | b'+')) {
            pos += 1;
        }
        let start = pos;
        let mut value = 0i32;
        while let Some(&c) = text.get(pos).filter(|c| c.is_ascii_digit()) {
            value = value.wrapping_mul(10).wrapping_add(i32::from(c - b'0'));
            pos += 1;
        }
        if pos == start {
            return (index, fields);
        }
        *field = if negative {
            value.wrapping_neg()
        } else {
            value
        };
    }
    (3, fields)
}

/// `mysofa_check`.
pub(crate) fn check(hrtf: &Hrtf) -> Result<(), Error> {
    const ZERO: [f32; 3] = [0.0, 0.0, 0.0];
    const UP: [f32; 3] = [0.0, 0.0, 1.0];
    const FRONT: [f32; 3] = [1.0, 0.0, 0.0];

    if !has_attribute(&hrtf.attributes, "Conventions", "SOFA")
        || !has_attribute(&hrtf.attributes, "SOFAConventions", "SimpleFreeFieldHRIR")
        || !has_attribute(&hrtf.attributes, "DataType", "FIR")
    {
        return Err(Error::InvalidAttributes);
    }

    if hrtf.c != 3 || hrtf.i != 1 || hrtf.e != 1 || hrtf.r != 2 || hrtf.m == 0 {
        return Err(Error::InvalidDimensions);
    }
    let m = hrtf.m as i32;

    let view = &hrtf.listener_view;
    if view.values.is_some() {
        let mut copies = 1;
        if !has_attribute(&view.attributes, "DIMENSION_LIST", "I,C") {
            if !has_attribute(&view.attributes, "DIMENSION_LIST", "M,C") {
                return Err(Error::InvalidDimensionList);
            }
            copies = m;
        }
        if has_attribute(&view.attributes, "Type", "cartesian") {
            if !compare_values(view, &FRONT, copies) {
                return Err(Error::InvalidFormat);
            }
        } else if has_attribute(&view.attributes, "Type", "spherical") {
            if !compare_values(view, &UP, copies) {
                return Err(Error::InvalidFormat);
            }
        } else {
            return Err(Error::InvalidCoordinateType);
        }
    }

    let emitter = &hrtf.emitter_position;
    let mut copies = 1;
    if !has_attribute(&emitter.attributes, "DIMENSION_LIST", "E,C,I") {
        if !has_attribute(&emitter.attributes, "DIMENSION_LIST", "E,C,M") {
            return Err(Error::OnlyEmitterWithEciSupported);
        }
        copies = m;
    }
    if !compare_values(emitter, &ZERO, copies) {
        return Err(Error::OnlyEmitterWithEciSupported);
    }

    let delay = &hrtf.data_delay;
    if delay.values.is_some()
        && !has_attribute(&delay.attributes, "DIMENSION_LIST", "I,R")
        && !has_attribute(&delay.attributes, "DIMENSION_LIST", "M,R")
    {
        return Err(Error::OnlyDelaysWithIrOrMrSupported);
    }

    if !has_attribute(&hrtf.data_sampling_rate.attributes, "DIMENSION_LIST", "I") {
        return Err(Error::OnlyTheSameSamplingRateSupported);
    }

    let receiver = &hrtf.receiver_position;
    let receivers = hrtf.c.wrapping_mul(hrtf.r) as usize;
    if has_attribute(&receiver.attributes, "DIMENSION_LIST", "R,C,I") {
        // Nothing to check.
    } else if has_attribute(&receiver.attributes, "DIMENSION_LIST", "R,C,M") {
        let values = receiver.values.as_deref().unwrap_or_default();
        if values.len() as u64 != u64::from(hrtf.c) * u64::from(hrtf.r) * u64::from(hrtf.m) {
            return Err(Error::InvalidReceiverPositions);
        }
        // Every measurement must use the same receiver positions.
        for i in 0..receivers {
            let row = &values[i * hrtf.m as usize..(i + 1) * hrtf.m as usize];
            let first = f64::from(row[0]);
            if row[1..]
                .iter()
                .any(|&v| (first - f64::from(v)).abs() >= 0.00001)
            {
                return Err(Error::ReceiversWithRciSupported);
            }
        }
    } else {
        return Err(Error::ReceiversWithRciSupported);
    }

    if !has_attribute(&receiver.attributes, "Type", "cartesian") {
        return Err(Error::ReceiversWithCartesianSupported);
    }

    // The ears must be roughly symmetrical on the y axis.
    let values = receiver.values.as_deref().unwrap_or_default();
    if values.len() < receivers
        || values[0].abs() >= 0.02
        || values[2].abs() >= 0.02
        || values[3].abs() >= 0.02
        || values[5].abs() >= 0.02
    {
        return Err(Error::InvalidReceiverPositions);
    }
    if (values[4] + values[1]).abs() >= 0.02 {
        return Err(Error::InvalidReceiverPositions);
    }
    // Old versions of the ARI SOFA API wrote the ears the wrong way round; that is accepted from them only.
    if values[1] < 0.0 {
        if !has_attribute(
            &hrtf.attributes,
            "APIName",
            "ARI SOFA API for Matlab/Octave",
        ) {
            return Err(Error::InvalidReceiverPositions);
        }
        let Some(version) = hrtf.attribute("APIVersion") else {
            return Err(Error::InvalidReceiverPositions);
        };
        let (count, [a, b, c]) = scan_version(version);
        if count != 3 || a > 1 || (a == 1 && b > 1) || (a == 1 && b == 1 && c > 0) {
            return Err(Error::InvalidReceiverPositions);
        }
    }

    if !has_attribute(&hrtf.source_position.attributes, "DIMENSION_LIST", "M,C") {
        return Err(Error::OnlySourcesWithMcSupported);
    }
    Ok(())
}
