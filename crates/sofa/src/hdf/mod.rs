//! The read-only HDF5 subset libmysofa implements (upstream `src/hdf/`): superblock versions 0–3, version 2 object
//! headers, fractal heaps, global heaps, contiguous and deflated chunked datasets.

mod btree;
mod dataobject;
mod fractalheap;
mod gcol;
mod reader;
mod superblock;

pub(crate) use dataobject::DataObject;
pub(crate) use reader::Reader;
pub(crate) use superblock::read_superblock;
