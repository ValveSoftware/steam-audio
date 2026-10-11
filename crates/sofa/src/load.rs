//! SOFA loader (upstream `hrtf/reader.c`: `load`, `getHrtf`).

use crate::hdf::{DataObject, Reader, read_superblock};
use crate::{Array, Error, Hrtf, Variable, has_attribute};

/// The `NAME` attribute netCDF gives dimension scales; the dimension's size is the number at its end.
const NETCDF_DIMENSION: &[u8] = b"This is a netCDF dimension but not a netCDF variable.";

/// `load`.
pub(crate) fn load(data: &[u8]) -> Result<Hrtf, Error> {
    let mut reader = Reader::new(data);
    let mut root = DataObject::default();
    let result = read_superblock(&mut reader, &mut root);
    if reader.exhausted {
        return Err(Error::ResourceLimit);
    }
    result?;
    get_hrtf(root)
}

/// `getHrtf`. Upstream walks the root group's children in its list order, the reverse of file order: where
/// several children set the same field, the one first in the file wins.
fn get_hrtf(root: DataObject) -> Result<Hrtf, Error> {
    let mut hrtf = Hrtf {
        attributes: root.attributes,
        ..Hrtf::default()
    };
    if !has_attribute(&hrtf.attributes, "Conventions", "SOFA") {
        return Err(Error::InvalidFormat);
    }

    let mut dimension_flags = 0;
    for dir in root.directory.iter().rev() {
        let Some(&[name]) = dir.name.as_deref() else {
            continue;
        };
        let (dimension, flag) = match name {
            b'I' => (&mut hrtf.i, 0x01),
            b'C' => (&mut hrtf.c, 0x02),
            b'R' => (&mut hrtf.r, 0x04),
            b'E' => (&mut hrtf.e, 0x08),
            b'N' => (&mut hrtf.n, 0x10),
            b'M' => (&mut hrtf.m, 0x20),
            // Tolerated (some files written by API version 0.4.4 have it).
            b'S' => continue,
            _ => return Err(Error::InvalidFormat),
        };
        *dimension = get_dimension(dir)?;
        dimension_flags |= flag;
    }
    if dimension_flags != 0x3f || hrtf.i != 1 || hrtf.c != 3 {
        return Err(Error::InvalidFormat);
    }

    // Each variable overwrites the status: only the last one's survives.
    let mut status = Ok(());
    let mut variables = Vec::new();
    for mut dir in root.directory.into_iter().rev() {
        let Some(name) = dir.name.take() else {
            continue;
        };
        let array = match name.as_slice() {
            b"ListenerPosition" => &mut hrtf.listener_position,
            b"ReceiverPosition" => &mut hrtf.receiver_position,
            b"SourcePosition" => &mut hrtf.source_position,
            b"EmitterPosition" => &mut hrtf.emitter_position,
            b"ListenerUp" => &mut hrtf.listener_up,
            b"ListenerView" => &mut hrtf.listener_view,
            b"Data.IR" => &mut hrtf.data_ir,
            b"Data.SamplingRate" => &mut hrtf.data_sampling_rate,
            b"Data.Delay" => &mut hrtf.data_delay,
            // Dimensions.
            [_] => continue,
            _ => {
                let mut value = Array::default();
                status = get_array(&mut value, &mut dir);
                if status.is_ok() {
                    variables.push(Variable { name, value });
                }
                continue;
            }
        };
        status = get_array(array, &mut dir);
    }
    // Upstream prepends while walking in reverse file order, which leaves the list in file order.
    variables.reverse();
    hrtf.variables = variables;
    hrtf.load_error = status.err();
    Ok(hrtf)
}

/// `getDimension`: the size of a dimension scale, taken from the digits ending its netCDF `NAME` attribute.
fn get_dimension(dir: &DataObject) -> Result<u32, Error> {
    if !has_attribute(&dir.attributes, "CLASS", "DIMENSION_SCALE") {
        return Err(Error::InvalidFormat);
    }
    for attribute in dir.attributes.iter().rev() {
        let Some(value) = &attribute.value else {
            continue;
        };
        if attribute.name == b"NAME" && value.starts_with(NETCDF_DIMENSION) {
            let digits = value
                .iter()
                .rev()
                .take_while(|c| c.is_ascii_digit())
                .count();
            return Ok(atoi(&value[value.len() - digits..]) as u32);
        }
    }
    Err(Error::InvalidFormat)
}

/// MSVC's `atoi` on a run of digits: saturates at `INT_MAX`; 0 when empty.
fn atoi(digits: &[u8]) -> i32 {
    digits.iter().fold(0i32, |n, &d| {
        n.saturating_mul(10).saturating_add(i32::from(d - b'0'))
    })
}

/// `getArray`: converts a 64-bit dataset to `f32` values and moves its attributes into `array`.
fn get_array(array: &mut Array, dir: &mut DataObject) -> Result<(), Error> {
    // Upstream reads the precision through the floating-point member of a union, so 64-bit integers pass too and
    // are reinterpreted as doubles.
    if dir.dt.bit_precision != 64 {
        return Err(Error::UnsupportedFormat);
    }
    array.attributes = std::mem::take(&mut dir.attributes);

    let elements = (dir.data_len / 8) as usize;
    // MSVC's `realloc(NULL, 0)` allocates and `realloc(p, 0)` frees: a dataset without data reads as present and
    // empty, one with fewer than 8 bytes as missing.
    array.values = match dir.data.take() {
        None => Some(Vec::new()),
        Some(_) if elements == 0 => None,
        Some(data) => Some(
            data.chunks_exact(8)
                .take(elements)
                .map(|d| f64::from_le_bytes(d.try_into().expect("8-byte chunk")) as f32)
                .collect(),
        ),
    };
    Ok(())
}
