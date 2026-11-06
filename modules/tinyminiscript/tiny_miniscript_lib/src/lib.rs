use std::ffi::CStr;
use std::os::raw::c_char;
use std::str::Utf8Error;

use tinyminiscript::descriptor::Descriptor;
use tinyminiscript::parser::keys::KeyToken;
use tinyminiscript::satisfy::{HashFunc, Satisfier, SatisfyError};
use tinyminiscript::Context;

// Analyze possible satisfactions, without requiring actual spending data.
// These placeholders are consumed by the library's satisfaction algorithm;
// they are not valid signatures or preimages for spending a transaction.
struct AvailableSatisfier {
    signatures_available: bool,
}

impl Satisfier for AvailableSatisfier {
    fn check_older(&self, _: u32) -> Option<bool> {
        Some(true)
    }

    fn check_after(&self, _: u32) -> Option<bool> {
        Some(true)
    }

    fn sign(&self, _: &KeyToken) -> Option<(Vec<u8>, bool)> {
        Some((vec![0; 72], self.signatures_available))
    }

    fn preimage(&self, hash_func: HashFunc, _: &[u8]) -> Option<(Vec<u8>, bool)> {
        Some((vec![0; hash_func.expected_length()], true))
    }
}

unsafe fn c_str_to_str<'a>(input: *const c_char) -> Result<&'a str, Utf8Error> {
    CStr::from_ptr(input).to_str()
}

#[no_mangle]
pub unsafe extern "C" fn tiny_miniscript_descriptor_parse(input: *const c_char) -> bool {
    let Ok(desc) = c_str_to_str(input) else {
        return false;
    };

    let ts_descriptor = Context::try_from(desc);
    match ts_descriptor {
        Err(_) => false,
        // The parser also accepts bare miniscript expressions.
        Ok(ctx) => {
            ctx.top_level_descriptor() != Descriptor::Bare
                && match ctx.satisfy(&AvailableSatisfier {
                    signatures_available: true,
                }) {
                    Ok(result) => {
                        // Check existence of a signature-free satisfaction via
                        // the solver, rather than has_sig: this revision does
                        // not set has_sig for public-key-hash fragments.
                        result.sat.available
                            && matches!(
                                ctx.satisfy(&AvailableSatisfier {
                                    signatures_available: false,
                                }),
                                Ok(unsigned) if !unsigned.sat.available
                            )
                    }
                    // Unsupported satisfaction operations (e.g. taproot or
                    // non-definite keys) do not imply an invalid descriptor.
                    Err(SatisfyError::TaprootNotSupported | SatisfyError::NonDefiniteKey(_)) => {
                        true
                    }
                    Err(_) => false,
                }
        }
    }
}
