//! Global heap collections (upstream `hdf/gcol.c`), which hold the targets of object references.

use super::reader::{Gcol, Reader};
use crate::Error;

/// `readGCOL`: adds the collection's objects to `reader.gcol`.
fn read_gcol(reader: &mut Reader) -> Result<(), Error> {
    if !reader.signature(b"GCOL") {
        return Err(Error::InvalidFormat);
    }
    if reader.getc() != 1 {
        return Err(Error::InvalidFormat);
    }
    if reader.getc() < 0 || reader.getc() < 0 || reader.getc() < 0 {
        return Err(Error::ReadError);
    }

    // Upstream keeps the collection address in a `uint16_t`.
    let address = u64::from(reader.tell() as u16);
    let collection_size = reader.read_length();
    if collection_size > 0x4_0000_0000 {
        return Err(Error::InvalidFormat);
    }
    let end = address.wrapping_add(collection_size.wrapping_sub(8));
    let last = end
        .wrapping_sub(8)
        .wrapping_sub(u64::from(reader.size_of_lengths));

    while reader.tell() as u64 <= last {
        let heap_object_index = reader.read_value(2) as u16;
        if heap_object_index == 0 {
            break;
        }
        let _reference_count = reader.read_value(2);
        if !reader.seek_cur(4) {
            return Err(reader.errno_error());
        }
        let object_size = reader.read_length();
        if object_size > 8 {
            return Err(Error::UnsupportedFormat);
        }
        let value = reader.read_value(object_size as i32);
        reader.gcol.push(Gcol {
            heap_object_index,
            address,
            value,
        });
    }
    Ok(())
}

/// `gcolRead`: the value (an object address) of a global-heap object. Like upstream, the search stops at the first
/// object (most recent first) that matches either the collection address or the index.
pub(crate) fn read_gcol_reference(
    reader: &mut Reader,
    gcol: u64,
    reference: i32,
) -> Result<u64, Error> {
    let find = |reader: &Reader| {
        reader
            .gcol
            .iter()
            .rev()
            .find(|p| p.address == gcol || i32::from(p.heap_object_index) == reference)
            .map(|p| p.value)
    };
    if let Some(value) = find(reader) {
        return Ok(value);
    }

    let position = reader.tell();
    if !reader.seek_set(gcol) {
        return Err(Error::ReadError);
    }
    // Upstream ignores the result: objects read before an error stay in the list.
    let _ = read_gcol(reader);
    if !reader.seek_set(position as u64) {
        return Err(Error::ReadError);
    }
    find(reader).ok_or(Error::InvalidFormat)
}
