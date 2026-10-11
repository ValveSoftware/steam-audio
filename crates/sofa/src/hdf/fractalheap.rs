//! Fractal heaps (upstream `hdf/fractalhead.c`). libmysofa does not follow heap IDs: it walks the direct blocks
//! and pattern-matches the records netCDF writes into them (links to child objects and dense attributes).

use super::dataobject::{DataObject, read_dataobject};
use super::reader::{Reader, c_string, msvc_f32_to_i32, msvc_f64_to_i32};
use crate::{Attribute, Error};

/// Indirect blocks nested deeper than this are rejected. Not in upstream, where a block pointing to itself
/// recurses until the stack overflows.
const MAX_INDIRECT_DEPTH: u32 = 32;

const MAX_NAME_LENGTH: usize = 0x100;

#[derive(Debug, Default)]
struct FractalHeap {
    flags: u8,
    maximum_size: u32,
    table_width: u16,
    starting_block_size: u64,
    maximum_direct_block_size: u64,
    maximum_heap_size: u16,
    encoded_length: u16,
}

/// `fractalheapRead`: adds the heap's attributes and child objects to `object`.
pub(crate) fn read_fractal_heap(reader: &mut Reader, object: &mut DataObject) -> Result<(), Error> {
    if !reader.signature(b"FRHP") {
        return Err(Error::UnsupportedFormat);
    }
    if reader.getc() != 0 {
        return Err(Error::UnsupportedFormat);
    }

    let mut heap = FractalHeap::default();
    let _heap_id_length = reader.read_value(2);
    heap.encoded_length = reader.read_value(2) as u16;
    if heap.encoded_length > 0x8000 {
        return Err(Error::UnsupportedFormat);
    }
    heap.flags = reader.getc() as u8;
    heap.maximum_size = reader.read_value(4) as u32;

    let _next_huge_object_id = reader.read_length();
    let _btree_address_of_huge_objects = reader.read_offset();
    let _free_space = reader.read_length();
    let _address_free_space = reader.read_offset();
    let _amount_managed_space = reader.read_length();
    let _amount_allocated_space = reader.read_length();
    let _offset_managed_space = reader.read_length();
    let _number_managed_objects = reader.read_length();
    let _size_huge_objects = reader.read_length();
    let number_huge_objects = reader.read_length();
    let _size_tiny_objects = reader.read_length();
    let number_tiny_objects = reader.read_length();

    heap.table_width = reader.read_value(2) as u16;
    heap.starting_block_size = reader.read_length();
    heap.maximum_direct_block_size = reader.read_length();
    heap.maximum_heap_size = reader.read_value(2) as u16;
    let _starting_row = reader.read_value(2);
    let address_of_root_block = reader.read_offset();
    let current_row = reader.read_value(2) as u16;

    if heap.encoded_length > 0 {
        let _size_of_filtered_block = reader.read_length();
        let _filter_mask = reader.read_value(4);
        if reader.read(u64::from(heap.encoded_length)).len() != usize::from(heap.encoded_length) {
            return Err(Error::ReadError);
        }
    }
    // Skip the checksum.
    if !reader.seek_cur(4) {
        return Err(Error::ReadError);
    }

    if number_huge_objects != 0 || number_tiny_objects != 0 {
        return Err(Error::UnsupportedFormat);
    }

    if reader.valid_address(address_of_root_block) {
        if !reader.seek_set(address_of_root_block) {
            return Err(reader.errno_error());
        }
        if current_row != 0 {
            read_indirect_block(reader, object, &heap, heap.starting_block_size, 0)?;
        } else {
            read_direct_block(reader, object, &heap)?;
        }
    }
    Ok(())
}

/// `log2i`: `round(log2(a))` converted like MSVC does.
fn log2i(a: i32) -> i32 {
    msvc_f64_to_i32(f64::from(a).log2().round())
}

/// `ceilf(log2f(x) / 8)` converted like MSVC does: the byte size of a field holding `x`.
fn byte_size(x: f32) -> i32 {
    msvc_f32_to_i32((x.log2() / 8.0).ceil())
}

/// `directblockRead`.
fn read_direct_block(
    reader: &mut Reader,
    object: &mut DataObject,
    heap: &FractalHeap,
) -> Result<(), Error> {
    if reader.recursive_counter >= 20 {
        return Err(Error::InvalidFormat);
    }
    reader.recursive_counter += 1;

    if !reader.signature(b"FHDB") {
        return Err(Error::InvalidFormat);
    }
    if reader.getc() != 0 {
        return Err(Error::UnsupportedFormat);
    }
    // Heap header address.
    if !reader.seek_cur(i64::from(reader.size_of_offsets)) {
        return Err(reader.errno_error());
    }
    let _block_offset = reader.read_value((i32::from(heap.maximum_heap_size) + 7) / 8);
    if heap.flags & 2 != 0 && !reader.seek_cur(4) {
        return Err(reader.errno_error());
    }

    let offset_size = byte_size(f32::from(heap.maximum_heap_size));
    let length_size = if heap.maximum_direct_block_size < u64::from(heap.maximum_size) {
        byte_size(heap.maximum_direct_block_size as f32)
    } else {
        byte_size(heap.maximum_size as f32)
    };

    loop {
        let type_and_version = reader.getc() as u8;
        let offset = reader.read_value(offset_size);
        let length = reader.read_value(length_size);
        if offset > 0x1000_0000 || length > 0x1000_0000 {
            return Err(Error::UnsupportedFormat);
        }

        match type_and_version {
            0 => break,
            // A dense attribute: name and value.
            3 => {
                if reader.read_value(5) != 0x00_0004_0008 {
                    return Err(Error::UnsupportedFormat);
                }
                let name = reader.read(length);
                if name.len() as u64 != length {
                    return Err(Error::ReadError);
                }
                let name = c_string(name);
                if reader.read_value(4) != 0x13 {
                    return Err(Error::UnsupportedFormat);
                }
                let len = reader.read_value(2) as i32;
                if !(0..=0x1000).contains(&len) {
                    return Err(Error::UnsupportedFormat);
                }
                let value = match reader.read_value(6) {
                    0x0000_0002_0200 => None,
                    0x0000_0002_0000 => {
                        let value = reader.read(len as u64);
                        if value.len() != len as usize {
                            return Err(Error::ReadError);
                        }
                        Some(c_string(value))
                    }
                    0x0200_0002_0000 => Some(Vec::new()),
                    // Upstream gives up on the rest of the block, without restoring the recursion counter.
                    _ => return Ok(()),
                };
                object.attributes.push(Attribute { name, value });
            }
            1 => match reader.read_value(4) {
                // A link to a child object.
                0 => {
                    if reader.read_value(2) != 0 {
                        return Err(Error::InvalidFormat);
                    }
                    let len = reader.getc();
                    if len < 0 {
                        return Err(Error::ReadError);
                    }
                    let name = reader.read(len as u64);
                    if name.len() != len as usize {
                        return Err(Error::ReadError);
                    }
                    let name = c_string(name);
                    let heap_header_address = reader.read_offset();

                    let store = reader.tell();
                    if !reader.seek_set(heap_header_address) {
                        return Err(reader.errno_error());
                    }
                    let mut child = DataObject::default();
                    let result = read_dataobject(reader, &mut child, Some(name));
                    object.directory.push(child);
                    result?;
                    if !reader.seek_set(store as u64) {
                        return Err(reader.errno_error());
                    }
                }
                // A compact attribute.
                0x0008_0008 | 0x0004_0008 => {
                    let attribute = read_compact_attribute(reader)?;
                    object.attributes.push(attribute);
                }
                _ => return Err(Error::UnsupportedFormat),
            },
            // Upstream gives up on the rest of the block, without restoring the recursion counter.
            _ => return Ok(()),
        }
    }

    reader.recursive_counter -= 1;
    Ok(())
}

/// The `0x00080008` / `0x00040008` records of a direct block.
fn read_compact_attribute(reader: &mut Reader) -> Result<Attribute, Error> {
    // The name runs up to the first NUL; the record continues up to a 0x13 byte.
    let mut name = Vec::new();
    let mut length = None;
    for i in 0..MAX_NAME_LENGTH {
        let c = reader.getc();
        if c < 0 || i == MAX_NAME_LENGTH - 1 {
            return Err(Error::ReadError);
        }
        name.push(c as u8);
        if length.is_none() && c == 0 {
            length = Some(i);
        }
        if c == 0x13 {
            break;
        }
    }
    // Without a NUL, upstream's `realloc(name, 0)` returns NULL, reported as out of memory.
    let length = length.ok_or(Error::NoMemory)?;
    name.truncate(length);

    if reader.read_value(3) != 0 {
        return Err(Error::UnsupportedFormat);
    }
    let mut len = reader.read_value(4) as i32;
    if !(0..=0x1000).contains(&len) {
        return Err(Error::UnsupportedFormat);
    }
    // Upstream assigns through an `int`.
    let unknown = reader.read_value(8) as i32 as i64 as u64;
    if unknown != 0x0000_0001 && unknown != 0x0200_0002 {
        return Err(Error::UnsupportedFormat);
    }
    if unknown == 0x0200_0002 {
        len = 0;
    }
    let value = reader.read(len as u64);
    if value.len() != len as usize {
        return Err(Error::ReadError);
    }
    Ok(Attribute {
        name,
        value: Some(c_string(value)),
    })
}

/// `indirectblockRead`.
fn read_indirect_block(
    reader: &mut Reader,
    object: &mut DataObject,
    heap: &FractalHeap,
    iblock_size: u64,
    depth: u32,
) -> Result<(), Error> {
    if !reader.signature(b"FHIB") {
        return Err(Error::InvalidFormat);
    }
    if reader.getc() != 0 {
        return Err(Error::UnsupportedFormat);
    }
    let _heap_header_address = reader.read_offset();
    let block_offset = reader.read_value((i32::from(heap.maximum_heap_size) + 7) / 8);
    if block_offset != 0 {
        return Err(Error::UnsupportedFormat);
    }

    let width = i32::from(heap.table_width);
    let start = log2i(heap.starting_block_size as i32);
    let nrows = log2i(iblock_size as i32)
        .wrapping_sub(start)
        .wrapping_add(1);
    let max_dblock_rows = log2i(heap.maximum_direct_block_size as i32)
        .wrapping_sub(start)
        .wrapping_add(2);
    let mut k = if nrows < max_dblock_rows {
        nrows
    } else {
        max_dblock_rows
    }
    .wrapping_mul(width);
    let mut n = k.wrapping_sub(max_dblock_rows.wrapping_mul(width));

    let mut child_direct_block = 0;
    while k > 0 {
        if reader.at_eof() {
            // Every remaining entry reads as all ones, an invalid address.
            child_direct_block = u64::MAX;
            break;
        }
        child_direct_block = reader.read_offset();
        if heap.encoded_length > 0 {
            let _size_filtered = reader.read_length();
            let _filter_mask = reader.read_value(4);
        }
        if reader.valid_address(child_direct_block) {
            let store = reader.tell();
            if !reader.seek_set(child_direct_block) {
                return Err(reader.errno_error());
            }
            read_direct_block(reader, object, heap)?;
            if !reader.seek_set(store as u64) {
                return Err(reader.errno_error());
            }
        }
        k -= 1;
    }

    // Upstream tests the last direct block's address here, not the indirect one's. `n` is positive only when the
    // row arithmetic overflows.
    while n > 0 {
        if !reader.valid_address(child_direct_block) {
            // The remaining entries are read and ignored.
            reader.read((n as u64) * u64::from(reader.size_of_offsets));
            break;
        }
        let child_indirect_block = reader.read_offset();
        let store = reader.tell();
        if !reader.seek_set(child_indirect_block) {
            return Err(reader.errno_error());
        }
        if depth >= MAX_INDIRECT_DEPTH {
            return Err(Error::ResourceLimit);
        }
        read_indirect_block(reader, object, heap, iblock_size.wrapping_mul(2), depth + 1)?;
        if !reader.seek_set(store as u64) {
            return Err(reader.errno_error());
        }
        n -= 1;
    }
    Ok(())
}
