//! Byte source with libmysofa's in-memory `READER` semantics (upstream `hrtf/reader.c`, `mysofa_read` and friends).
//!
//! Positions follow the Windows x64 reference build: `long` is 32 bits, so offsets passed to `mysofa_seek` are
//! truncated to `i32` before they are checked.

use crate::Error;

/// `EINVAL`, the only `errno` value the in-memory reader produces.
pub(crate) const EINVAL: i32 = 22;

/// A global-heap object (upstream `struct GCOL`).
#[derive(Debug, Clone, Copy)]
pub(crate) struct Gcol {
    pub heap_object_index: u16,
    /// Collection address, truncated to 16 bits like upstream's `uint16_t address`.
    pub address: u64,
    pub value: u64,
}

#[derive(Debug)]
pub(crate) struct Reader<'a> {
    memory: &'a [u8],
    pos: u64,
    /// The process-wide `errno` as libmysofa sees it: 0 until a seek fails.
    pub errno: i32,
    /// Bytes still allowed to be consumed (not in upstream; bounds the work on malicious input).
    budget: u64,
    pub exhausted: bool,

    pub size_of_offsets: u8,
    pub size_of_lengths: u8,
    pub end_of_file_address: u64,

    /// Every data object read so far as `(address, name)`, in read order (upstream `reader->all`, searched from
    /// the head, i.e. the most recent first).
    pub all: Vec<(u64, Option<Vec<u8>>)>,
    /// Global-heap objects in read order (upstream `reader->gcol`, searched from the most recent).
    pub gcol: Vec<Gcol>,
    pub recursive_counter: i32,
}

impl<'a> Reader<'a> {
    /// `memory.len()` must fit in an `i32` (upstream's `long` file positions).
    pub fn new(memory: &'a [u8]) -> Self {
        debug_assert!(i32::try_from(memory.len()).is_ok());
        let len = memory.len() as u64;
        Self {
            memory,
            pos: 0,
            errno: 0,
            budget: len.saturating_mul(16).saturating_add(1 << 20),
            exhausted: false,
            size_of_offsets: 0,
            size_of_lengths: 0,
            end_of_file_address: 0,
            all: Vec::new(),
            gcol: Vec::new(),
            recursive_counter: 0,
        }
    }

    fn len(&self) -> u64 {
        self.memory.len() as u64
    }

    /// Whether no further byte can be consumed: every read now fails without moving.
    pub fn at_eof(&self) -> bool {
        self.pos >= self.len() || self.exhausted
    }

    /// `mysofa_tell`.
    pub fn tell(&self) -> i32 {
        self.pos as i32
    }

    fn take_budget(&mut self, n: u64) -> bool {
        if n > self.budget {
            self.exhausted = true;
            self.budget = 0;
            return false;
        }
        self.budget -= n;
        true
    }

    /// `mysofa_getc`: the next byte, or -1 at the end of the data.
    pub fn getc(&mut self) -> i32 {
        if self.pos >= self.len() || !self.take_budget(1) {
            return -1;
        }
        let c = self.memory[self.pos as usize];
        self.pos += 1;
        i32::from(c)
    }

    /// `mysofa_read`: up to `n` bytes (fewer at the end of the data).
    pub fn read(&mut self, n: u64) -> &'a [u8] {
        let n = n.min(self.len() - self.pos.min(self.len()));
        if !self.take_budget(n) {
            return &[];
        }
        let start = self.pos as usize;
        self.pos += n;
        &self.memory[start..start + n as usize]
    }

    /// Reads `n` bytes and checks they equal `signature`.
    pub fn signature(&mut self, signature: &[u8; 4]) -> bool {
        self.read(4) == signature
    }

    /// `readValue`: a little-endian value of `size` bytes, or all ones if the data ends. Bytes beyond the eighth
    /// are ORed in with the shift count taken modulo 64, as x64 does.
    pub fn read_value(&mut self, size: i32) -> u64 {
        let c = self.getc();
        if c < 0 {
            return u64::MAX;
        }
        let mut value = c as u64;
        for i in 1..size {
            let c = self.getc();
            if c < 0 {
                return u64::MAX;
            }
            value |= (c as u64).wrapping_shl((i as u32).wrapping_mul(8));
        }
        value
    }

    /// `readValue(reader, superblock.size_of_offsets)`.
    pub fn read_offset(&mut self) -> u64 {
        self.read_value(i32::from(self.size_of_offsets))
    }

    /// `readValue(reader, superblock.size_of_lengths)`.
    pub fn read_length(&mut self) -> u64 {
        self.read_value(i32::from(self.size_of_lengths))
    }

    fn seek_to(&mut self, offset: i32) -> bool {
        if offset < 0 || offset as u64 > self.len() {
            self.errno = EINVAL;
            return false;
        }
        self.pos = offset as u64;
        true
    }

    /// `mysofa_seek(reader, address, SEEK_SET)`; the address is truncated to a 32-bit `long`.
    pub fn seek_set(&mut self, address: u64) -> bool {
        self.seek_to(address as i32)
    }

    /// `mysofa_seek(reader, delta, SEEK_CUR)`; the delta is truncated to a 32-bit `long`.
    pub fn seek_cur(&mut self, delta: i64) -> bool {
        self.seek_to((delta as i32).wrapping_add(self.pos as i32))
    }

    /// `mysofa_seek(reader, 0, SEEK_END)`.
    pub fn seek_end(&mut self) {
        self.pos = self.len();
    }

    /// `validAddress`.
    pub fn valid_address(&self, address: u64) -> bool {
        address > 0 && address < self.end_of_file_address
    }

    /// `return errno;` after a failed seek.
    pub fn errno_error(&self) -> Error {
        Error::Errno(self.errno)
    }
}

/// Truncates a C string read from the file at its first NUL.
pub(crate) fn c_string(bytes: &[u8]) -> Vec<u8> {
    let end = bytes.iter().position(|&b| b == 0).unwrap_or(bytes.len());
    bytes[..end].to_vec()
}

/// MSVC's `(int)` conversion of a `double` (`cvttsd2si`): `INT_MIN` for NaN and out-of-range values.
pub(crate) fn msvc_f64_to_i32(x: f64) -> i32 {
    if x.is_nan() || x <= -2_147_483_649.0 || x >= 2_147_483_648.0 {
        i32::MIN
    } else {
        x as i32
    }
}

/// MSVC's `(int)` conversion of a `float` (`cvttss2si`): `INT_MIN` for NaN and out-of-range values.
pub(crate) fn msvc_f32_to_i32(x: f32) -> i32 {
    msvc_f64_to_i32(f64::from(x))
}
