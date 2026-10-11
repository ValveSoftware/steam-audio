//! Data objects and their header messages (upstream `hdf/dataobject.c`).

use super::btree::read_tree;
use super::fractalheap::read_fractal_heap;
use super::gcol::read_gcol_reference;
use super::reader::{Reader, c_string};
use crate::{Attribute, Error};

/// Datatype message nesting (lists and compound members) deeper than this is rejected. Not in upstream, which
/// recurses until the stack overflows.
const MAX_DATATYPE_DEPTH: u32 = 32;

/// Attribute values with more elements than this are rejected. Not in upstream, where an element that consumes no
/// bytes (an empty string, say) repeated 2^64 times never finishes.
const MAX_ATTRIBUTE_ELEMENTS: u64 = 1 << 20;

/// Upstream `struct DATATYPE`. The integer and floating-point members of its union share `bit_offset` and
/// `bit_precision`, which is how `getArray` reads the precision of either.
#[derive(Debug, Default, Clone, Copy)]
pub(crate) struct Datatype {
    pub class_and_version: u8,
    pub class_bit_field: u32,
    pub size: u32,
    pub bit_offset: u16,
    pub bit_precision: u16,
    pub list: u32,
}

/// Upstream `struct DATASPACE`.
#[derive(Debug, Default, Clone, Copy)]
pub(crate) struct Dataspace {
    pub dimension_size: [u64; 4],
    pub dimension_max_size: [u64; 4],
    pub dimensionality: u8,
    pub flags: u8,
    pub kind: u8,
}

#[derive(Debug, Default, Clone, Copy)]
struct LinkInfo {
    fractal_heap_address: u64,
}

#[derive(Debug, Default, Clone, Copy)]
struct AttributeInfo {
    fractal_heap_address: u64,
}

/// Upstream `struct DATAOBJECT`: an object header with what libmysofa keeps of it.
#[derive(Debug, Default)]
pub(crate) struct DataObject {
    pub name: Option<Vec<u8>>,
    pub address: u64,
    flags: u8,
    pub dt: Datatype,
    pub ds: Dataspace,
    li: LinkInfo,
    ai: AttributeInfo,
    pub datalayout_chunk: [i32; 5],
    /// Attributes in read order (upstream prepends, so its list is in reverse order).
    pub attributes: Vec<Attribute>,
    /// Child objects in read order (upstream prepends, so its list is in reverse order).
    pub directory: Vec<DataObject>,
    pub data: Option<Vec<u8>>,
    pub data_len: i32,
}

/// `dataobjectRead`: reads the object header at the current position, then its attribute and link heaps.
pub(crate) fn read_dataobject(
    reader: &mut Reader,
    object: &mut DataObject,
    name: Option<Vec<u8>>,
) -> Result<(), Error> {
    *object = DataObject {
        address: reader.tell() as u64,
        name,
        ..DataObject::default()
    };

    if !reader.signature(b"OHDR") {
        return Err(Error::InvalidFormat);
    }
    if reader.getc() != 2 {
        return Err(Error::UnsupportedFormat);
    }
    object.flags = reader.getc() as u8;
    if object.flags & (1 << 5) != 0 && !reader.seek_cur(16) {
        return Err(reader.errno_error());
    }
    if object.flags & (1 << 4) != 0 {
        return Err(Error::UnsupportedFormat);
    }

    let size_of_chunk = reader.read_value(1 << (object.flags & 3));
    if size_of_chunk > 0x100_0000 {
        return Err(Error::UnsupportedFormat);
    }
    let end_of_messages = (reader.tell() as u64).wrapping_add(size_of_chunk);
    read_messages(reader, object, end_of_messages)?;

    if reader.valid_address(object.ai.fractal_heap_address) {
        if !reader.seek_set(object.ai.fractal_heap_address) {
            return Err(reader.errno_error());
        }
        read_fractal_heap(reader, object)?;
    }
    if reader.valid_address(object.li.fractal_heap_address) {
        // Upstream ignores a failed seek here.
        reader.seek_set(object.li.fractal_heap_address);
        read_fractal_heap(reader, object)?;
    }

    reader.all.push((object.address, object.name.clone()));
    Ok(())
}

/// `readOHDRmessages`.
fn read_messages(
    reader: &mut Reader,
    object: &mut DataObject,
    end_of_messages: u64,
) -> Result<(), Error> {
    // The final gap may be up to 3 bytes long.
    while (reader.tell() as u64) < end_of_messages.wrapping_sub(4) {
        let message_type = reader.getc() as u8;
        let message_size = reader.read_value(2) as u16;
        let message_flags = reader.getc() as u8;
        if message_flags & !5 != 0 {
            return Err(Error::UnsupportedFormat);
        }
        // Skip the creation order.
        if object.flags & (1 << 2) != 0 && !reader.seek_cur(2) {
            return Err(reader.errno_error());
        }

        let end = reader.tell().wrapping_add(i32::from(message_size));
        match message_type {
            0 => {
                // NIL
                if !reader.seek_cur(i64::from(message_size)) {
                    return Err(reader.errno_error());
                }
            }
            1 => read_dataspace(reader, &mut object.ds)?,
            2 => read_link_info(reader, &mut object.li)?,
            3 => read_datatype(reader, &mut object.dt, 0)?,
            4 => read_fill_value_old(reader)?,
            5 => read_fill_value(reader)?,
            8 => read_data_layout(reader, object)?,
            10 => read_group_info(reader)?,
            11 => read_filter_pipeline(reader)?,
            12 => read_attribute(reader, object)?,
            16 => read_continuation(reader, object)?,
            21 => read_attribute_info(reader, &mut object.ai)?,
            _ => return Err(Error::UnsupportedFormat),
        }

        if reader.tell() != end {
            return Err(Error::Internal);
        }
    }

    // Skip the checksum.
    if !reader.seek_set(end_of_messages.wrapping_add(4)) {
        return Err(reader.errno_error());
    }
    Ok(())
}

/// `readOHDRHeaderMessageDataspace`.
fn read_dataspace(reader: &mut Reader, ds: &mut Dataspace) -> Result<(), Error> {
    let version = reader.getc();
    ds.dimensionality = reader.getc() as u8;
    if ds.dimensionality > 4 {
        return Err(Error::InvalidFormat);
    }
    ds.flags = reader.getc() as u8;

    match version {
        1 => {
            let _reserved = reader.read_value(5);
            for i in 0..usize::from(ds.dimensionality) {
                ds.dimension_size[i] = reader.read_length();
                if ds.dimension_size[i] > 1_000_000 {
                    return Err(Error::InvalidFormat);
                }
            }
            read_max_sizes(reader, ds);
            if ds.flags & 2 != 0 {
                // Permutation indices.
                return Err(Error::InvalidFormat);
            }
        }
        2 => {
            ds.kind = reader.getc() as u8;
            for i in 0..usize::from(ds.dimensionality) {
                ds.dimension_size[i] = reader.read_length();
            }
            read_max_sizes(reader, ds);
        }
        _ => return Err(Error::InvalidFormat),
    }
    Ok(())
}

fn read_max_sizes(reader: &mut Reader, ds: &mut Dataspace) {
    if ds.flags & 1 != 0 {
        for i in 0..usize::from(ds.dimensionality) {
            ds.dimension_max_size[i] = reader.read_length();
        }
    }
}

/// `readOHDRHeaderMessageLinkInfo`.
fn read_link_info(reader: &mut Reader, li: &mut LinkInfo) -> Result<(), Error> {
    if reader.getc() != 0 {
        return Err(Error::UnsupportedFormat);
    }
    let flags = reader.getc() as u8;
    if flags & 1 != 0 {
        let _maximum_creation_index = reader.read_value(8);
    }
    li.fractal_heap_address = reader.read_offset();
    let _address_btree_index = reader.read_offset();
    if flags & 2 != 0 {
        let _address_btree_order = reader.read_offset();
    }
    Ok(())
}

/// `readOHDRHeaderMessageDatatype`. Fields a message does not set keep their previous values, as upstream reads
/// every datatype message of an object into the same struct.
fn read_datatype(reader: &mut Reader, dt: &mut Datatype, depth: u32) -> Result<(), Error> {
    if depth > MAX_DATATYPE_DEPTH {
        return Err(Error::ResourceLimit);
    }

    dt.class_and_version = reader.getc() as u8;
    if dt.class_and_version & 0xf0 != 0x10 && dt.class_and_version & 0xf0 != 0x30 {
        return Err(Error::UnsupportedFormat);
    }
    dt.class_bit_field = reader.read_value(3) as u32;
    dt.size = reader.read_value(4) as u32;
    if dt.size > 64 {
        return Err(Error::UnsupportedFormat);
    }

    match dt.class_and_version & 0xf {
        // Fixed point.
        0 => {
            dt.bit_offset = reader.read_value(2) as u16;
            dt.bit_precision = reader.read_value(2) as u16;
        }
        // Floating point: only IEEE single and double precision.
        1 => {
            dt.bit_offset = reader.read_value(2) as u16;
            dt.bit_precision = reader.read_value(2) as u16;
            let exponent_location = reader.getc() as u8;
            let exponent_size = reader.getc() as u8;
            let mantissa_location = reader.getc() as u8;
            let mantissa_size = reader.getc() as u8;
            let exponent_bias = reader.read_value(4) as u32;

            let single = (
                exponent_location,
                exponent_size,
                mantissa_size,
                exponent_bias,
            ) == (23, 8, 23, 127);
            let double = (
                exponent_location,
                exponent_size,
                mantissa_size,
                exponent_bias,
            ) == (52, 11, 52, 1023);
            if dt.bit_offset != 0
                || mantissa_location != 0
                || (dt.bit_precision != 32 && dt.bit_precision != 64)
                || (dt.bit_precision == 32 && !single)
                || (dt.bit_precision == 64 && !double)
            {
                return Err(Error::UnsupportedFormat);
            }
        }
        // String, reference.
        3 | 7 => {}
        // Compound: the members are parsed and dropped.
        6 => match dt.class_and_version >> 4 {
            3 => {
                for _ in 0..(dt.class_bit_field & 0xffff) {
                    // Name: up to 0xfff bytes including the NUL.
                    let mut length = 0;
                    while length < 0x1000 - 1 {
                        let c = reader.getc();
                        if c < 0 {
                            return Err(Error::ReadError);
                        }
                        if c == 0 {
                            break;
                        }
                        length += 1;
                    }
                    // Byte offset of the member, as many bytes as the compound size needs (at most 1).
                    if dt.size > 0 {
                        reader.getc();
                    }
                    read_datatype(reader, &mut Datatype::default(), depth + 1)?;
                }
            }
            1 => {
                for _ in 0..(dt.class_bit_field & 0xffff) {
                    let mut length = 0i64;
                    loop {
                        if length == 256 {
                            return Err(Error::InvalidFormat);
                        }
                        let c = reader.getc();
                        if c < 0 {
                            return Err(Error::ReadError);
                        }
                        if c == 0 {
                            break;
                        }
                        length += 1;
                    }
                    if !reader.seek_cur((7 - length) & 7) {
                        return Err(Error::ReadError);
                    }
                    let _offset = reader.read_value(4);
                    if reader.getc() != 0 {
                        // Array members are not supported.
                        return Err(Error::InvalidFormat);
                    }
                    if !reader.seek_cur(3 + 4 + 4 + 4 * 4) {
                        return Err(Error::ReadError);
                    }
                    read_datatype(reader, &mut Datatype::default(), depth + 1)?;
                }
            }
            _ => return Err(Error::InvalidFormat),
        },
        // Variable-length list: the base type follows and overwrites this one, except for `list`.
        9 => {
            dt.list = dt.size;
            read_datatype(reader, dt, depth + 1)?;
        }
        _ => return Err(Error::UnsupportedFormat),
    }
    Ok(())
}

/// `readOHDRHeaderMessageDataFill`.
fn read_fill_value(reader: &mut Reader) -> Result<(), Error> {
    match reader.getc() {
        1 | 2 => {
            let space_allocation_time = reader.getc();
            let fill_value_write_time = reader.getc();
            let fill_value_defined = reader.getc();
            if space_allocation_time < 0 || fill_value_write_time < 0 || fill_value_defined < 0 {
                return Err(Error::ReadError);
            }
            if space_allocation_time & !1 != 2
                || fill_value_write_time != 2
                || fill_value_defined & !1 != 0
            {
                return Err(Error::InvalidFormat);
            }
            if fill_value_defined > 0 {
                skip_fill_value(reader)?;
            }
        }
        3 => {
            let flags = reader.getc() as u8;
            if flags & (1 << 5) != 0 {
                skip_fill_value(reader)?;
            }
        }
        _ => return Err(Error::InvalidFormat),
    }
    Ok(())
}

/// `readOHDRHeaderMessageDataFillOld`.
fn read_fill_value_old(reader: &mut Reader) -> Result<(), Error> {
    skip_fill_value(reader)
}

fn skip_fill_value(reader: &mut Reader) -> Result<(), Error> {
    let size = reader.read_value(4) as u32;
    if !reader.seek_cur(i64::from(size)) {
        return Err(reader.errno_error());
    }
    Ok(())
}

/// `readOHDRHeaderMessageDataLayout`: version 3, contiguous (read at once) or chunked (read through the v1
/// B-tree) storage.
fn read_data_layout(reader: &mut Reader, object: &mut DataObject) -> Result<(), Error> {
    if reader.getc() != 3 {
        return Err(Error::InvalidFormat);
    }

    match reader.getc() {
        1 => {
            let data_address = reader.read_offset();
            let data_size = reader.read_length();
            if reader.valid_address(data_address) {
                let store = reader.tell();
                if !reader.seek_set(data_address) {
                    return Err(reader.errno_error());
                }
                object.data = None;
                if data_size > 0x1000_0000 {
                    return Err(Error::InvalidFormat);
                }
                object.data_len = data_size as i32;
                let data = reader.read(data_size);
                if data.len() as u64 != data_size {
                    return Err(Error::ReadError);
                }
                object.data = Some(data.to_vec());
                if !reader.seek_set(store as u64) {
                    return Err(reader.errno_error());
                }
            }
        }
        2 => {
            let dimensionality = reader.getc() as u8;
            if !(1..=5).contains(&dimensionality) {
                return Err(Error::InvalidFormat);
            }
            let data_address = reader.read_offset();
            for i in 0..usize::from(dimensionality) {
                object.datalayout_chunk[i] = reader.read_value(4) as i32;
            }

            let mut size = object.datalayout_chunk[usize::from(dimensionality) - 1] as i64 as u64;
            for i in 0..usize::from(object.ds.dimensionality) {
                size = size.wrapping_mul(object.ds.dimension_size[i]);
            }

            if reader.valid_address(data_address) && dimensionality <= 4 {
                let store = reader.tell();
                if !reader.seek_set(data_address) {
                    return Err(reader.errno_error());
                }
                if object.data.is_none() {
                    if size > 0x1_0000_0000 {
                        return Err(Error::InvalidFormat);
                    }
                    // Upstream keeps the size in an `int` and crashes or misreads beyond 2 GiB.
                    let data_len = i32::try_from(size).map_err(|_| Error::ResourceLimit)?;
                    object.data_len = data_len;
                    object.data = Some(vec![0; size as usize]);
                }
                read_tree(reader, object)?;
                if !reader.seek_set(store as u64) {
                    return Err(reader.errno_error());
                }
            }
        }
        _ => return Err(Error::InvalidFormat),
    }
    Ok(())
}

/// `readOHDRHeaderMessageGroupInfo`.
fn read_group_info(reader: &mut Reader) -> Result<(), Error> {
    if reader.getc() != 0 {
        return Err(Error::UnsupportedFormat);
    }
    let flags = reader.getc() as u8;
    if flags & 1 != 0 {
        let _maximum_compact_value = reader.read_value(2);
        let _minimum_dense_value = reader.read_value(2);
    }
    if flags & 2 != 0 {
        let _number_of_entries = reader.read_value(2);
        let _length_of_entries = reader.read_value(2);
    }
    Ok(())
}

/// `readOHDRHeaderMessageFilterPipeline`. Only deflate (1) and shuffle (2) are accepted; the pipeline itself is
/// not kept, and chunks are always inflated (upstream never applies shuffle).
fn read_filter_pipeline(reader: &mut Reader) -> Result<(), Error> {
    let version = reader.getc();
    let filters = reader.getc();
    if version < 0 || filters < 0 {
        return Err(Error::ReadError);
    }
    if filters > 32 {
        return Err(Error::InvalidFormat);
    }

    match version {
        1 => {
            if reader.read_value(6) != 0 {
                return Err(Error::InvalidFormat);
            }
            for _ in 0..filters {
                check_filter_id(reader.read_value(2) as u16)?;
                let name_length = reader.read_value(2) as u16;
                let _flags = reader.read_value(2);
                let client_data_values = reader.read_value(2) as u16;
                if name_length > 0
                    && !reader.seek_cur(i64::from(((i32::from(name_length) - 1) & !7) + 8))
                {
                    return Err(Error::ReadError);
                }
                if client_data_values > 0x1000 {
                    return Err(Error::UnsupportedFormat);
                }
                for _ in 0..client_data_values {
                    reader.read_value(4);
                }
                if client_data_values & 1 == 1 {
                    reader.read_value(4);
                }
            }
        }
        2 => {
            for _ in 0..filters {
                check_filter_id(reader.read_value(2) as u16)?;
                let _flags = reader.read_value(2);
                let client_data_values = reader.read_value(2) as u16;
                if client_data_values > 0x1000 {
                    return Err(Error::UnsupportedFormat);
                }
                for _ in 0..client_data_values {
                    reader.read_value(4);
                }
            }
        }
        _ => return Err(Error::InvalidFormat),
    }
    Ok(())
}

fn check_filter_id(id: u16) -> Result<(), Error> {
    if id == 1 || id == 2 {
        Ok(())
    } else {
        Err(Error::InvalidFormat)
    }
}

/// `readDataVar`: one element of an attribute value. Only strings and object references produce a value.
fn read_data_var(
    reader: &mut Reader,
    string: &mut Option<Vec<u8>>,
    dt: &Datatype,
) -> Result<(), Error> {
    let mut gcol = 0;
    if dt.list != 0 {
        if dt.list.wrapping_sub(dt.size) == 8 {
            reader.read_value(4);
            gcol = reader.read_value(4);
        } else {
            gcol = reader.read_value(dt.list.wrapping_sub(dt.size) as i32);
        }
    }

    match dt.class_and_version & 0xf {
        // Fixed point and compound values are skipped.
        0 | 6 => {
            if !reader.seek_cur(i64::from(dt.size)) {
                return Err(reader.errno_error());
            }
        }
        3 => {
            let bytes = reader.read(u64::from(dt.size));
            if bytes.len() != dt.size as usize {
                return Err(Error::ReadError);
            }
            *string = Some(c_string(bytes));
        }
        7 => {
            reader.read_value(4);
            let reference = reader.read_value(dt.size.wrapping_sub(4) as i32);
            // Upstream ignores unresolvable references.
            let Ok(address) = read_gcol_reference(reader, gcol, reference as i32) else {
                return Ok(());
            };
            // A reference resolves to the name of an object read earlier, or to its heap index. Upstream would
            // crash on an unnamed object; the root group is the only one and is never in the list in time.
            let name = match reader.all.iter().rev().find(|(a, _)| *a == address) {
                Some((_, name)) => name.clone().unwrap_or_default(),
                None => format!("REF{:08X}", reference as u32).into_bytes(),
            };
            match string {
                Some(s) => {
                    s.push(b',');
                    s.extend_from_slice(&name);
                }
                None => *string = Some(name),
            }
        }
        _ => return Err(Error::Internal),
    }
    Ok(())
}

/// `readData`: every element of an attribute's dataspace, row-major.
fn read_data(
    reader: &mut Reader,
    string: &mut Option<Vec<u8>>,
    dt: &Datatype,
    ds: &mut Dataspace,
) -> Result<(), Error> {
    if ds.dimensionality == 0 {
        ds.dimension_size[0] = 1;
    }
    let mut elements = 1u64;
    for i in 0..usize::from(ds.dimensionality.max(1)) {
        elements = elements.saturating_mul(ds.dimension_size[i]);
    }
    if elements > MAX_ATTRIBUTE_ELEMENTS {
        return Err(Error::ResourceLimit);
    }
    read_data_dim(reader, string, dt, ds, 0)
}

/// `readDataDim`.
fn read_data_dim(
    reader: &mut Reader,
    string: &mut Option<Vec<u8>>,
    dt: &Datatype,
    ds: &Dataspace,
    dim: usize,
) -> Result<(), Error> {
    if dim >= ds.dimension_size.len() {
        return Err(Error::UnsupportedFormat);
    }
    for _ in 0..ds.dimension_size[dim] {
        if dim + 1 < usize::from(ds.dimensionality) {
            read_data_dim(reader, string, dt, ds, dim + 1)?;
        } else {
            read_data_var(reader, string, dt)?;
        }
    }
    Ok(())
}

/// `readOHDRHeaderMessageContinue`.
fn read_continuation(reader: &mut Reader, object: &mut DataObject) -> Result<(), Error> {
    let offset = reader.read_offset();
    let length = reader.read_length();
    if offset > 0x200_0000 || length > 0x1000_0000 {
        return Err(Error::UnsupportedFormat);
    }
    if reader.recursive_counter >= 25 {
        return Err(Error::UnsupportedFormat);
    }
    reader.recursive_counter += 1;

    let store = reader.tell();
    if !reader.seek_set(offset) {
        return Err(reader.errno_error());
    }
    if !reader.signature(b"OCHK") {
        return Err(Error::InvalidFormat);
    }
    // The end excludes the checksum.
    read_messages(reader, object, (offset + length).wrapping_sub(4))?;
    if !reader.seek_set(store as u64) {
        return Err(reader.errno_error());
    }
    Ok(())
}

/// `readOHDRHeaderMessageAttribute`: versions 1 and 3.
fn read_attribute(reader: &mut Reader, object: &mut DataObject) -> Result<(), Error> {
    let version = reader.getc();
    if version != 1 && version != 3 {
        return Err(Error::InvalidFormat);
    }
    let flags = reader.getc() as u8;
    let name_size = reader.read_value(2) as u16;
    let datatype_size = reader.read_value(2) as u16;
    let dataspace_size = reader.read_value(2) as u16;
    if version == 3 {
        let _encoding = reader.getc();
    }

    if name_size > 0x1000 {
        return Err(Error::NoMemory);
    }
    let name = reader.read(u64::from(name_size));
    if name.len() != usize::from(name_size) {
        // Upstream returns `errno`, which is still 0 unless a seek has failed: the attribute is then dropped
        // silently.
        return if reader.errno == 0 {
            Ok(())
        } else {
            Err(reader.errno_error())
        };
    }
    let name = c_string(name);
    let padding = |size: u16| i64::from((8 - i32::from(size)) & 7);
    if version == 1 && !reader.seek_cur(padding(name_size)) {
        return Err(reader.errno_error());
    }
    if version == 3 && flags & 3 != 0 {
        return Err(Error::InvalidFormat);
    }

    let mut dt = Datatype::default();
    read_datatype(reader, &mut dt, 0).map_err(|_| Error::InvalidFormat)?;
    if version == 1 && !reader.seek_cur(padding(datatype_size)) {
        return Err(reader.errno_error());
    }
    let mut ds = Dataspace::default();
    read_dataspace(reader, &mut ds).map_err(|_| Error::InvalidFormat)?;
    if version == 1 && !reader.seek_cur(padding(dataspace_size)) {
        return Err(reader.errno_error());
    }
    let mut value = None;
    read_data(reader, &mut value, &dt, &mut ds).map_err(|_| Error::InvalidFormat)?;

    object.attributes.push(Attribute { name, value });
    Ok(())
}

/// `readOHDRHeaderMessageAttributeInfo`.
fn read_attribute_info(reader: &mut Reader, ai: &mut AttributeInfo) -> Result<(), Error> {
    if reader.getc() != 0 {
        return Err(Error::UnsupportedFormat);
    }
    let flags = reader.getc() as u8;
    if flags & 1 != 0 {
        let _maximum_creation_index = reader.read_value(2);
    }
    ai.fractal_heap_address = reader.read_offset();
    let _attribute_name_btree = reader.read_offset();
    if flags & 2 != 0 {
        let _attribute_creation_order_btree = reader.read_offset();
    }
    Ok(())
}
