//! Superblock (upstream `hdf/superblock.c`).

use super::dataobject::{DataObject, read_dataobject};
use super::reader::Reader;
use crate::Error;

const SIGNATURE: &[u8; 8] = b"\x89HDF\r\n\x1a\n";

/// `superblockRead`: reads the superblock and the root group into `root`.
pub(crate) fn read_superblock(reader: &mut Reader, root: &mut DataObject) -> Result<(), Error> {
    if reader.read(8) != SIGNATURE {
        return Err(Error::InvalidFormat);
    }
    match reader.getc() {
        version @ (0 | 1) => read_v0_or_v1(reader, root, version),
        2 | 3 => read_v2_or_v3(reader, root),
        _ => Err(Error::InvalidFormat),
    }
}

fn check_sizes(reader: &Reader) -> Result<(), Error> {
    let valid = |size: u8| (2..=8).contains(&size);
    if valid(reader.size_of_offsets) && valid(reader.size_of_lengths) {
        Ok(())
    } else {
        Err(Error::UnsupportedFormat)
    }
}

fn read_root(reader: &mut Reader, root: &mut DataObject, root_address: u64) -> Result<(), Error> {
    if !reader.seek_set(root_address) {
        return Err(reader.errno_error());
    }
    read_dataobject(reader, root, None)
}

fn read_v2_or_v3(reader: &mut Reader, root: &mut DataObject) -> Result<(), Error> {
    reader.size_of_offsets = reader.getc() as u8;
    reader.size_of_lengths = reader.getc() as u8;
    if reader.getc() < 0 {
        return Err(Error::ReadError);
    }
    check_sizes(reader)?;

    let base_address = reader.read_offset();
    let _superblock_extension_address = reader.read_offset();
    reader.end_of_file_address = reader.read_offset();
    let root_address = reader.read_offset();

    if base_address != 0 {
        return Err(Error::UnsupportedFormat);
    }
    reader.seek_end();
    if reader.end_of_file_address != reader.tell() as u64 {
        return Err(Error::InvalidFormat);
    }
    read_root(reader, root, root_address)
}

fn read_v0_or_v1(reader: &mut Reader, root: &mut DataObject, version: i32) -> Result<(), Error> {
    // Versions of the free-space info, the root group symbol table entry, a reserved byte and the shared header
    // message format.
    for _ in 0..4 {
        if reader.getc() != 0 {
            return Err(Error::InvalidFormat);
        }
    }
    reader.size_of_offsets = reader.getc() as u8;
    reader.size_of_lengths = reader.getc() as u8;
    if reader.getc() != 0 {
        return Err(Error::InvalidFormat);
    }
    check_sizes(reader)?;

    let _group_leaf_node_k = reader.read_value(2);
    let _group_internal_node_k = reader.read_value(2);
    if reader.read_value(4) != 0 {
        return Err(Error::UnsupportedFormat);
    }
    if version == 1 {
        let _indexed_storage_internal_node_k = reader.read_value(2);
        let _reserved = reader.read_value(2);
    }

    if reader.read_offset() != 0 {
        return Err(Error::UnsupportedFormat);
    }
    let _free_space_address = reader.read_offset();
    reader.end_of_file_address = reader.read_offset();
    let _driver_information_address = reader.read_offset();
    let _link_name_offset = reader.read_offset();
    let root_address = reader.read_offset();

    if !matches!(reader.read_value(4), 0..=2) {
        return Err(Error::UnsupportedFormat);
    }
    // Upstream only logs a file size mismatch here.
    reader.seek_end();
    read_root(reader, root, root_address)
}
