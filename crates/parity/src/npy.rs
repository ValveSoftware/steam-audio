//! Minimal reader/writer for NumPy `.npy` files: little-endian `f4`/`f8`/`i4`, C order.
//!
//! `.npy` is trivial to emit from C++ (see `tools/golden/npy.h`) and easy to inspect with NumPy
//! when a parity test fails.

use std::fmt;
use std::fs;
use std::io;
use std::path::Path;

const MAGIC: &[u8] = b"\x93NUMPY";

/// Element types supported in golden files.
pub trait Element: Copy + Sized {
    /// NumPy `descr` string, e.g. `<f4`.
    const DESCR: &'static str;
    const SIZE: usize;
    fn from_le(bytes: &[u8]) -> Self;
    fn to_le(self, out: &mut Vec<u8>);
}

macro_rules! element {
    ($t:ty, $descr:literal) => {
        impl Element for $t {
            const DESCR: &'static str = $descr;
            const SIZE: usize = size_of::<$t>();
            fn from_le(bytes: &[u8]) -> Self {
                <$t>::from_le_bytes(bytes.try_into().expect("chunk size matches element size"))
            }
            fn to_le(self, out: &mut Vec<u8>) {
                out.extend_from_slice(&self.to_le_bytes());
            }
        }
    };
}

element!(f32, "<f4");
element!(f64, "<f8");
element!(i32, "<i4");
element!(u8, "|u1");

/// An n-dimensional array in C (row-major) order.
#[derive(Debug, Clone, PartialEq)]
pub struct Array<T> {
    pub shape: Vec<usize>,
    pub data: Vec<T>,
}

impl<T> Array<T> {
    /// Row `i` of a 2-D array.
    pub fn row(&self, i: usize) -> &[T] {
        assert_eq!(
            self.shape.len(),
            2,
            "row() needs a 2-D array, shape is {:?}",
            self.shape
        );
        let n = self.shape[1];
        &self.data[i * n..(i + 1) * n]
    }
}

#[derive(Debug)]
pub enum NpyError {
    Io(io::Error),
    Format(String),
}

impl fmt::Display for NpyError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::Io(e) => write!(f, "{e}"),
            Self::Format(m) => write!(f, "invalid .npy: {m}"),
        }
    }
}

impl std::error::Error for NpyError {}

impl From<io::Error> for NpyError {
    fn from(e: io::Error) -> Self {
        Self::Io(e)
    }
}

fn format_err(msg: impl Into<String>) -> NpyError {
    NpyError::Format(msg.into())
}

pub fn read_npy<T: Element>(path: &Path) -> Result<Array<T>, NpyError> {
    parse(&fs::read(path)?)
}

fn parse<T: Element>(bytes: &[u8]) -> Result<Array<T>, NpyError> {
    let rest = bytes
        .strip_prefix(MAGIC)
        .ok_or_else(|| format_err("bad magic"))?;
    let (&major, rest) = rest.split_first().ok_or_else(|| format_err("truncated"))?;
    let rest = rest.get(1..).ok_or_else(|| format_err("truncated"))?;
    let (header_len, rest) = match major {
        1 if rest.len() >= 2 => (u16::from_le_bytes([rest[0], rest[1]]) as usize, &rest[2..]),
        2 | 3 if rest.len() >= 4 => (
            u32::from_le_bytes([rest[0], rest[1], rest[2], rest[3]]) as usize,
            &rest[4..],
        ),
        _ => return Err(format_err(format!("unsupported version {major}"))),
    };
    let header = rest
        .get(..header_len)
        .ok_or_else(|| format_err("truncated header"))?;
    let header = std::str::from_utf8(header).map_err(|_| format_err("header is not UTF-8"))?;
    let body = &rest[header_len..];

    let descr = dict_value(header, "descr")?.trim_matches(['\'', '"']);
    if descr != T::DESCR {
        return Err(format_err(format!("dtype {descr}, expected {}", T::DESCR)));
    }
    if dict_value(header, "fortran_order")? != "False" {
        return Err(format_err("fortran_order arrays are not supported"));
    }
    let shape = parse_shape(dict_value(header, "shape")?)?;

    let len: usize = shape.iter().product();
    if body.len() != len * T::SIZE {
        return Err(format_err(format!(
            "body has {} bytes, shape {shape:?} needs {}",
            body.len(),
            len * T::SIZE
        )));
    }
    let data = body.chunks_exact(T::SIZE).map(T::from_le).collect();
    Ok(Array { shape, data })
}

/// Extracts the raw value text for `key` from the Python dict literal in the header.
fn dict_value<'a>(header: &'a str, key: &str) -> Result<&'a str, NpyError> {
    let pattern = format!("'{key}':");
    let start = header
        .find(&pattern)
        .ok_or_else(|| format_err(format!("missing key {key}")))?
        + pattern.len();
    let value = header[start..].trim_start();
    let end = if value.starts_with('(') {
        value.find(')').map(|i| i + 1)
    } else {
        value.find([',', '}'])
    }
    .ok_or_else(|| format_err(format!("unterminated value for {key}")))?;
    Ok(value[..end].trim())
}

fn parse_shape(text: &str) -> Result<Vec<usize>, NpyError> {
    let inner = text
        .strip_prefix('(')
        .and_then(|t| t.strip_suffix(')'))
        .ok_or_else(|| format_err(format!("bad shape {text}")))?;
    inner
        .split(',')
        .map(str::trim)
        .filter(|s| !s.is_empty())
        .map(|s| {
            s.parse()
                .map_err(|_| format_err(format!("bad shape {text}")))
        })
        .collect()
}

pub fn write_npy<T: Element>(path: &Path, shape: &[usize], data: &[T]) -> Result<(), NpyError> {
    if shape.iter().product::<usize>() != data.len() {
        return Err(format_err(format!(
            "shape {shape:?} does not match {} elements",
            data.len()
        )));
    }
    let dims: Vec<String> = shape.iter().map(ToString::to_string).collect();
    let shape_text = match dims.len() {
        1 => format!("({},)", dims[0]),
        _ => format!("({})", dims.join(", ")),
    };
    let mut header = format!(
        "{{'descr': '{}', 'fortran_order': False, 'shape': {shape_text}, }}",
        T::DESCR
    );
    // Pad so that magic + version + length + header + '\n' is a multiple of 64.
    let unpadded = MAGIC.len() + 2 + 2 + header.len() + 1;
    header.push_str(&" ".repeat(unpadded.next_multiple_of(64) - unpadded));
    header.push('\n');

    let header_len = u16::try_from(header.len()).map_err(|_| format_err("header too long"))?;
    let mut out = Vec::with_capacity(unpadded + data.len() * T::SIZE + 64);
    out.extend_from_slice(MAGIC);
    out.extend_from_slice(&[1, 0]);
    out.extend_from_slice(&header_len.to_le_bytes());
    out.extend_from_slice(header.as_bytes());
    for &x in data {
        x.to_le(&mut out);
    }
    if let Some(dir) = path.parent() {
        fs::create_dir_all(dir)?;
    }
    fs::write(path, out)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn roundtrip() {
        let dir = std::env::temp_dir().join(format!("parity-npy-{}", std::process::id()));
        let path = dir.join("a.npy");
        let data: Vec<f32> = (0..6).map(|i| i as f32 * 0.5).collect();
        write_npy(&path, &[2, 3], &data).unwrap();
        let a: Array<f32> = read_npy(&path).unwrap();
        assert_eq!(a.shape, [2, 3]);
        assert_eq!(a.row(1), &[1.5, 2.0, 2.5]);
        assert!(read_npy::<f64>(&path).is_err(), "dtype must be checked");
        fs::remove_dir_all(dir).unwrap();
    }

    #[test]
    fn parses_numpy_header() {
        // Header exactly as written by numpy.save for np.array([1, 2], dtype='<i4').
        let mut bytes = b"\x93NUMPY\x01\x00".to_vec();
        let header = "{'descr': '<i4', 'fortran_order': False, 'shape': (2,), }";
        let mut header = header.to_string();
        header.push_str(&" ".repeat(118 - header.len() - 1));
        header.push('\n');
        bytes.extend_from_slice(&(header.len() as u16).to_le_bytes());
        bytes.extend_from_slice(header.as_bytes());
        bytes.extend_from_slice(&1i32.to_le_bytes());
        bytes.extend_from_slice(&2i32.to_le_bytes());
        let a: Array<i32> = parse(&bytes).unwrap();
        assert_eq!(a.shape, [2]);
        assert_eq!(a.data, [1, 2]);
    }

    #[test]
    fn scalar_shape() {
        assert_eq!(parse_shape("()").unwrap(), Vec::<usize>::new());
        assert_eq!(parse_shape("(3, 4)").unwrap(), [3, 4]);
    }
}
