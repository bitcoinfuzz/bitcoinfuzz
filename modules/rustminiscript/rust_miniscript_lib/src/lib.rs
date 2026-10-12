use std::ffi::CStr;
use std::os::raw::c_char;
use std::str::Utf8Error;

use miniscript::bitcoin::secp256k1;
use miniscript::descriptor::ShInner;
use miniscript::miniscript::types::Base;
use miniscript::{
    Descriptor, DescriptorPublicKey, Miniscript, MiniscriptKey, ScriptContext, Segwitv0, Tap,
};

unsafe fn c_str_to_str<'a>(input: *const c_char) -> Result<&'a str, Utf8Error> {
    CStr::from_ptr(input).to_str()
}

/// Port of the `sanity_check` methods removed upstream in rust-miniscript
/// f7756f5. Upstream now validates only taproot leaves against the sane
/// parameters at parse time, and those parameters skip the resource limit
/// check, so keep applying the old checks to every descriptor. The base type
/// check restores the old parse-time rejection of non-B top-level scripts,
/// e.g. `wsh(v:pk(..))`, which upstream no longer does outside taproot.
fn ms_sanity_check<Ctx: ScriptContext>(ms: &Miniscript<DescriptorPublicKey, Ctx>) -> bool {
    ms.ty.corr.base == Base::B
        && ms.non_malleable_and_requires_sig()
        && ms.is_non_malleable()
        && ms.within_resource_limits()
        && !ms.has_repeated_keys()
        && !ms.has_mixed_timelocks()
}

fn sanity_check(desc: &Descriptor<DescriptorPublicKey>) -> bool {
    match desc {
        Descriptor::Bare(bare) => ms_sanity_check(bare.as_inner()),
        Descriptor::Pkh(_) => true,
        Descriptor::Wpkh(wpkh) => !wpkh.as_inner().is_uncompressed(),
        Descriptor::Wsh(wsh) => ms_sanity_check(wsh.as_inner()),
        Descriptor::Sh(sh) => match sh.as_inner() {
            ShInner::Wsh(wsh) => ms_sanity_check(wsh.as_inner()),
            ShInner::Wpkh(wpkh) => !wpkh.as_inner().is_uncompressed(),
            ShInner::Ms(ms) => ms_sanity_check(ms),
        },
        Descriptor::Tr(tr) => tr.leaves().all(|leaf| ms_sanity_check(leaf.miniscript())),
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_miniscript_descriptor_parse(input: *const c_char) -> bool {
    let Ok(desc) = c_str_to_str(input) else {
        return false;
    };

    let secp = &secp256k1::Secp256k1::signing_only();
    match Descriptor::<DescriptorPublicKey>::parse_descriptor(secp, desc) {
        Err(_) => false,
        Ok((d, _)) => sanity_check(&d),
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_miniscript_miniscript_parse(input: *const c_char) -> bool {
    let Ok(desc) = c_str_to_str(input) else {
        return false;
    };

    match Miniscript::<String, Segwitv0>::from_str_insane(desc) {
        Err(_) => Miniscript::<String, Tap>::from_str_insane(desc).is_ok(),
        Ok(_) => true,
    }
}
