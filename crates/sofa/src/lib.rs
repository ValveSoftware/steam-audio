//! Pure-Rust reader for [SOFA](https://www.sofaconventions.org/) (AES69) HRTF files.
//!
//! A port of the part of [libmysofa](https://github.com/hoene/libmysofa) 1.3.3 that Steam Audio uses
//! (`docs/adr/0001-sofa-loader.md`): the read-only HDF5 subset, the `SimpleFreeFieldHRIR` loader ([`Hrtf::load`],
//! upstream `mysofa_load_data`) and its validation ([`Hrtf::check`], `mysofa_check`). It accepts and rejects the same
//! files as libmysofa and reports the same status codes ([`Error::code`]), verified against golden data produced by
//! libmysofa itself.
//!
//! SOFA files are untrusted input. The parser is safe Rust, returns an [`Error`] for malformed data and never
//! panics on it; it adds limits where libmysofa would crash or not terminate (`docs/DEVIATIONS.md`).
//!
//! libmysofa is Copyright (c) 2016-2017, Symonics GmbH, Christian Hoene, under the BSD 3-Clause license (see
//! `LICENSE-libmysofa` in this crate).

mod check;
mod error;
mod hdf;
mod load;

use std::path::Path;

pub use error::Error;

/// A name/value attribute (upstream `MYSOFA_ATTRIBUTE`). Strings are kept as bytes, cut at the first NUL like C
/// strings.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Attribute {
    pub name: Vec<u8>,
    /// `None` when the attribute has no value (a NULL `value` upstream).
    pub value: Option<Vec<u8>>,
}

/// A variable's values, converted from `double` to `f32`, and its attributes (upstream `MYSOFA_ARRAY`).
#[derive(Debug, Clone, Default, PartialEq)]
pub struct Array {
    /// `None` when the variable is missing (a NULL `values` upstream). A present variable can be empty.
    pub values: Option<Vec<f32>>,
    /// In file order.
    pub attributes: Vec<Attribute>,
}

impl Array {
    /// Number of values (`elements` upstream).
    pub fn len(&self) -> usize {
        self.values.as_ref().map_or(0, Vec::len)
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// The value of the attribute `name`; with duplicates, the last one in the file (upstream's list order).
    pub fn attribute(&self, name: &str) -> Option<&[u8]> {
        find_attribute(&self.attributes, name)
    }
}

/// A variable that is not part of the convention (upstream `MYSOFA_VARIABLE`).
#[derive(Debug, Clone, PartialEq)]
pub struct Variable {
    pub name: Vec<u8>,
    pub value: Array,
}

/// The contents of a SOFA file (upstream `MYSOFA_HRTF`).
#[derive(Debug, Default)]
pub struct Hrtf {
    /// Singleton dimension (1).
    pub i: u32,
    /// Coordinate dimension (3).
    pub c: u32,
    /// Number of receivers (ears).
    pub r: u32,
    /// Number of emitters.
    pub e: u32,
    /// Number of samples per measurement.
    pub n: u32,
    /// Number of measurements.
    pub m: u32,

    pub listener_position: Array,
    pub receiver_position: Array,
    pub source_position: Array,
    pub emitter_position: Array,
    pub listener_up: Array,
    pub listener_view: Array,
    /// Impulse responses, `M × R × N`.
    pub data_ir: Array,
    pub data_sampling_rate: Array,
    pub data_delay: Array,

    /// Global attributes, in file order.
    pub attributes: Vec<Attribute>,
    /// Other variables, in file order.
    pub variables: Vec<Variable>,

    /// The status libmysofa's `mysofa_load` reports together with a usable HRTF: the error of the last variable
    /// it failed to read (for example one that is not stored as 64-bit values), which is then left missing.
    /// `mysofa_open` and Steam Audio ignore it.
    pub load_error: Option<Error>,
}

impl Hrtf {
    /// Parses a SOFA file held in memory (upstream `mysofa_load_data`).
    ///
    /// Data larger than `i32::MAX` bytes is rejected with [`Error::ResourceLimit`] (upstream file positions are a
    /// 32-bit `long` on Windows).
    pub fn load(data: &[u8]) -> Result<Self, Error> {
        if i32::try_from(data.len()).is_err() {
            return Err(Error::ResourceLimit);
        }
        load::load(data)
    }

    /// Reads and parses a SOFA file (upstream `mysofa_load`). The whole file is read into memory first, so malformed
    /// files are reported as [`Hrtf::load`] reports them (`docs/DEVIATIONS.md`).
    pub fn load_file(path: impl AsRef<Path>) -> Result<Self, Error> {
        Self::load(&std::fs::read(path)?)
    }

    /// Checks that the file follows the `SimpleFreeFieldHRIR` convention in the way libmysofa supports
    /// (upstream `mysofa_check`).
    pub fn check(&self) -> Result<(), Error> {
        check::check(self)
    }

    /// The value of the global attribute `name` (upstream `mysofa_getAttribute`); with duplicates, the last one in
    /// the file.
    pub fn attribute(&self, name: &str) -> Option<&[u8]> {
        find_attribute(&self.attributes, name)
    }
}

/// `mysofa_getAttribute`: upstream's lists are in reverse file order, so the last match in the file wins.
fn find_attribute<'a>(attributes: &'a [Attribute], name: &str) -> Option<&'a [u8]> {
    attributes
        .iter()
        .rev()
        .find(|a| a.name == name.as_bytes())
        .and_then(|a| a.value.as_deref())
}

/// `verifyAttribute` (and `checkAttribute`): whether any attribute has this name and value; a missing value never
/// matches.
fn has_attribute(attributes: &[Attribute], name: &str, value: &str) -> bool {
    attributes
        .iter()
        .any(|a| a.name == name.as_bytes() && a.value.as_deref() == Some(value.as_bytes()))
}
