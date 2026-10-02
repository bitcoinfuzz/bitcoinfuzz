//! Rust mirror of include/bitcoinfuzz/ffi.h.
//!
//! Crates pull this in with `#[path]` instead of depending on a shared crate,
//! so each staticlib compiles its own copy and several can link into one
//! binary without duplicate symbols.
//!
//! A BfResult hands its Vec's buffer to the harness, which frees it with
//! free(). That holds because Rust's default allocator is malloc on Unix, so
//! wrapper crates must not set a #[global_allocator].

// Not every wrapper uses every constructor.
#![allow(dead_code)]

use std::mem::ManuallyDrop;
use std::os::raw::c_char;

const BF_OK: u8 = 0;
const BF_SKIP: u8 = 1;
const BF_FAIL: u8 = 2;

#[repr(C)]
pub struct BfResult {
    status: u8,
    data: *mut c_char,
    len: usize,
}

const _: () = assert!(std::mem::size_of::<BfResult>() == 24);

impl BfResult {
    /// Pass an owned String to hand it over without a copy.
    pub fn ok(value: impl Into<Vec<u8>>) -> Self {
        Self::new(BF_OK, value.into())
    }

    pub fn skip() -> Self {
        Self::new(BF_SKIP, Vec::new())
    }

    pub fn fail() -> Self {
        Self::new(BF_FAIL, Vec::new())
    }

    /// The driver compares the reason, so it must match what the target's other
    /// modules return on rejection (e.g. "INVALID").
    pub fn fail_with(reason: impl Into<Vec<u8>>) -> Self {
        Self::new(BF_FAIL, reason.into())
    }

    fn new(status: u8, bytes: Vec<u8>) -> Self {
        // An empty Vec holds no allocation to hand over.
        if bytes.is_empty() {
            return Self {
                status,
                data: std::ptr::null_mut(),
                len: 0,
            };
        }
        let mut bytes = ManuallyDrop::new(bytes);
        Self {
            status,
            data: bytes.as_mut_ptr().cast(),
            len: bytes.len(),
        }
    }
}
