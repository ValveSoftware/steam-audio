//! Chunked dataset storage through a version 1 B-tree (upstream `treeRead` in `hdf/btree.c`, and `gunzip.c`).
//!
//! Upstream's version 2 B-tree reader (`btreeRead`) is never called and is not ported.

use miniz_oxide::inflate::TINFLStatus;
use miniz_oxide::inflate::core::{DecompressorOxide, decompress, inflate_flags};

use super::dataobject::DataObject;
use super::reader::Reader;
use crate::Error;

/// Deflate cannot expand data by more than this factor, so a chunk declared larger than this many times its
/// compressed size can never inflate to its full size.
const MAX_DEFLATE_RATIO: usize = 1032;

/// `treeRead`: reads the chunks listed in the B-tree node at the current position into `object.data`. Only the
/// node at the current position is read (upstream does not descend into child nodes).
pub(crate) fn read_tree(reader: &mut Reader, object: &mut DataObject) -> Result<(), Error> {
    let dimensionality = usize::from(object.ds.dimensionality);
    if dimensionality > 3 {
        return Err(Error::InvalidFormat);
    }
    if !reader.signature(b"TREE") {
        return Err(Error::InvalidFormat);
    }

    let node_type = reader.getc() as u8;
    let _node_level = reader.getc();
    let entries_used = reader.read_value(2) as u16;
    if entries_used > 0x1000 {
        return Err(Error::UnsupportedFormat);
    }
    let _address_of_left_sibling = reader.read_offset();
    let _address_of_right_sibling = reader.read_offset();

    let chunk = object.datalayout_chunk;
    let mut elements = 1i32;
    for &c in &chunk[..dimensionality] {
        elements = elements.wrapping_mul(c);
    }
    let size = chunk[dimensionality];
    if elements <= 0 || size <= 0 || elements > i32::MAX / size {
        return Err(Error::InvalidFormat);
    }
    let output_len = elements * size;
    let layout = Layout {
        dimensionality,
        chunk: [chunk[0], chunk[1], chunk[2]],
        elements,
        size,
        dy: chunk[1],
        dz: chunk[2],
        sx: object.ds.dimension_size[0] as i32,
        sy: object.ds.dimension_size[1] as i32,
        sz: object.ds.dimension_size[2] as i32,
    };

    for _ in 0..(i32::from(entries_used) * 2) {
        if node_type == 0 {
            let _key = reader.read_length();
            continue;
        }

        let size_of_chunk = reader.read_value(4) as u32;
        let filter_mask = reader.read_value(4) as u32;
        if filter_mask != 0 {
            return Err(Error::InvalidFormat);
        }
        let mut start = [0u64; 3];
        for s in &mut start[..dimensionality] {
            *s = reader.read_value(8);
        }
        if reader.read_value(8) != 0 {
            break;
        }
        let child_pointer = reader.read_offset();

        let store = reader.tell();
        if !reader.seek_set(child_pointer) {
            return Err(reader.errno_error());
        }
        let input = reader.read(u64::from(size_of_chunk));
        if input.len() as u64 != u64::from(size_of_chunk) {
            return Err(Error::InvalidFormat);
        }
        let output = inflate(input, output_len as usize).ok_or(Error::InvalidFormat)?;
        let data_len = object.data_len;
        let data = object
            .data
            .as_mut()
            .expect("chunked datasets allocate their data first");
        layout.scatter(&output, &start, data, data_len)?;

        if !reader.seek_set(store as u64) {
            return Err(reader.errno_error());
        }
    }

    // Skip the checksum.
    if !reader.seek_cur(4) {
        return Err(reader.errno_error());
    }
    Ok(())
}

/// `gunzip`: inflates a zlib stream that must produce exactly `expected` bytes.
///
/// Upstream makes a single `inflate(Z_SYNC_FLUSH)` call into a buffer of `expected` bytes and fails only on a zlib
/// error or a short output. zlib returns as soon as that buffer is full with a literal or match still pending, so
/// whatever follows (more data, or a corrupt checksum) goes unchecked; running out of input is not an error either.
/// The core inflater with a non-wrapping output buffer of exactly `expected` bytes stops at the same point.
fn inflate(input: &[u8], expected: usize) -> Option<Vec<u8>> {
    if expected
        > input
            .len()
            .saturating_mul(MAX_DEFLATE_RATIO)
            .saturating_add(64)
    {
        return None;
    }
    let mut output = vec![0; expected];
    let mut decompressor = Box::<DecompressorOxide>::default();
    let flags = inflate_flags::TINFL_FLAG_PARSE_ZLIB_HEADER
        | inflate_flags::TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
    let (status, _, written) = decompress(&mut decompressor, input, &mut output, 0, flags);
    let failed = matches!(
        status,
        TINFLStatus::Failed | TINFLStatus::Adler32Mismatch | TINFLStatus::BadParam
    );
    (!failed && written == expected).then_some(output)
}

/// Chunk geometry, in upstream's `int` arithmetic.
struct Layout {
    dimensionality: usize,
    chunk: [i32; 3],
    elements: i32,
    size: i32,
    dy: i32,
    dz: i32,
    sx: i32,
    sy: i32,
    sz: i32,
}

impl Layout {
    /// Copies an inflated chunk starting at element `start` into `data`. The chunk is stored byte-plane by
    /// byte-plane (`b` is the byte index within an element, as upstream reads it). Bytes outside the dataset or
    /// beyond `data_len` are dropped.
    fn scatter(
        &self,
        output: &[u8],
        start: &[u64; 3],
        data: &mut [u8],
        data_len: i32,
    ) -> Result<(), Error> {
        if self.dimensionality == 0 {
            return Err(Error::Internal);
        }
        match self.chunk_extent() {
            Some(extent) => self.scatter_fast(output, start, data, data_len, extent),
            None => self.scatter_exact(output, start, data, data_len),
        }
    }

    /// The chunk's extent along each dimension (1 for unused ones) when every extent is positive and their product
    /// is `elements` without overflow. The per-byte divisions of [`Layout::scatter_exact`] then reduce to nested
    /// counters.
    fn chunk_extent(&self) -> Option<[i32; 3]> {
        let mut extent = [1i32; 3];
        let mut product = 1i32;
        for (e, &c) in extent.iter_mut().zip(&self.chunk[..self.dimensionality]) {
            if c <= 0 {
                return None;
            }
            *e = c;
            product = product.checked_mul(c)?;
        }
        (product == self.elements).then_some(extent)
    }

    /// [`Layout::scatter_exact`] for a chunk of the given extent, without the divisions.
    fn scatter_fast(
        &self,
        output: &[u8],
        start: &[u64; 3],
        data: &mut [u8],
        data_len: i32,
        extent: [i32; 3],
    ) -> Result<(), Error> {
        let offset = |a: i32, s: u64| a.wrapping_add(s as i32);
        let szy = self.sz.wrapping_mul(self.sy);
        let mut bytes = output.iter();
        for b in 0..self.size {
            for xe in 0..extent[0] {
                let x = offset(xe, start[0]);
                for ye in 0..extent[1] {
                    let y = offset(ye, start[1]);
                    for ze in 0..extent[2] {
                        let z = offset(ze, start[2]);
                        let byte = *bytes.next().expect("output holds elements * size bytes");
                        let j = match self.dimensionality {
                            1 => (x < self.sx).then(|| x.wrapping_mul(self.size).wrapping_add(b)),
                            2 => (y < self.sy && x < self.sx).then(|| {
                                x.wrapping_mul(self.sy)
                                    .wrapping_add(y)
                                    .wrapping_mul(self.size)
                                    .wrapping_add(b)
                            }),
                            _ => (z < self.sz && y < self.sy && x < self.sx).then(|| {
                                x.wrapping_mul(szy)
                                    .wrapping_add(y.wrapping_mul(self.sz))
                                    .wrapping_add(z)
                                    .wrapping_mul(self.size)
                                    .wrapping_add(b)
                            }),
                        };
                        if let Some(j) = j.filter(|&j| j >= 0 && j < data_len) {
                            data[j as usize] = byte;
                        }
                    }
                }
            }
        }
        Ok(())
    }

    /// Upstream's loop, one division per byte.
    fn scatter_exact(
        &self,
        output: &[u8],
        start: &[u64; 3],
        data: &mut [u8],
        data_len: i32,
    ) -> Result<(), Error> {
        // Upstream divides by these and would trap on a zero divisor or on `INT_MIN / -1`.
        let div = |a: i32, b: i32| a.checked_div(b).ok_or(Error::Internal);
        let rem = |a: i32, b: i32| a.checked_rem(b).ok_or(Error::Internal);
        // `int + uint64_t` truncated back to `int`.
        let offset = |a: i32, s: u64| a.wrapping_add(s as i32);
        let dzy = self.dz.wrapping_mul(self.dy);
        let szy = self.sz.wrapping_mul(self.sy);

        let mut store = |j: i32, byte: u8| {
            if j >= 0 && j < data_len {
                data[j as usize] = byte;
            }
        };

        for (i, &byte) in output.iter().enumerate() {
            let i = i as i32;
            let b = i / self.elements;
            let e = i % self.elements;
            match self.dimensionality {
                1 => {
                    let x = offset(e, start[0]);
                    if x < self.sx {
                        store(x.wrapping_mul(self.size).wrapping_add(b), byte);
                    }
                }
                2 => {
                    let y = offset(rem(e, self.dy)?, start[1]);
                    let x = offset(div(e, self.dy)?, start[0]);
                    if y < self.sy && x < self.sx {
                        let j = x
                            .wrapping_mul(self.sy)
                            .wrapping_add(y)
                            .wrapping_mul(self.size)
                            .wrapping_add(b);
                        store(j, byte);
                    }
                }
                3 => {
                    let z = offset(rem(e, self.dz)?, start[2]);
                    let y = offset(rem(div(e, self.dz)?, self.dy)?, start[1]);
                    let x = offset(div(e, dzy)?, start[0]);
                    if z < self.sz && y < self.sy && x < self.sx {
                        let j = x
                            .wrapping_mul(szy)
                            .wrapping_add(y.wrapping_mul(self.sz))
                            .wrapping_add(z)
                            .wrapping_mul(self.size)
                            .wrapping_add(b);
                        store(j, byte);
                    }
                }
                _ => return Err(Error::Internal),
            }
        }
        Ok(())
    }
}
