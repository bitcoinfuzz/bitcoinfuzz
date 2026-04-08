use std::ffi::CString;
use std::os::raw::c_char;
use std::slice;

const INCOMPATIBILITY_BASE: u8 = 32;
const FILE_FORMAT: u8 = 4;
const ADDRMAN_NEW_BUCKET_COUNT: i32 = 1 << 10;
const ADDRMAN_TRIED_BUCKET_COUNT: i32 = 1 << 8;
const ADDRMAN_BUCKET_SIZE: i32 = 1 << 6;

unsafe fn str_to_c_string(input: &str) -> *mut c_char {
    CString::new(input).unwrap().into_raw()
}

fn read_i32_le(data: &[u8], offset: usize) -> Option<i32> {
    if offset + 4 > data.len() {
        return None;
    }
    Some(i32::from_le_bytes([
        data[offset],
        data[offset + 1],
        data[offset + 2],
        data[offset + 3],
    ]))
}

fn parse_addrman(data: &[u8]) -> Result<(i32, i32), ()> {
    if data.len() < 42 {
        return Err(());
    }

    let format = data[0];
    let compat = data[1];

    if compat < INCOMPATIBILITY_BASE {
        return Err(());
    }

    let lowest_compatible = compat - INCOMPATIBILITY_BASE;
    if lowest_compatible > FILE_FORMAT {
        return Err(());
    }

    if format > FILE_FORMAT {
        return Err(());
    }

    let n_new = read_i32_le(data, 34).ok_or(())?;
    let n_tried = read_i32_le(data, 38).ok_or(())?;

    if n_new < 0 || n_new > ADDRMAN_NEW_BUCKET_COUNT * ADDRMAN_BUCKET_SIZE {
        return Err(());
    }
    if n_tried < 0 || n_tried > ADDRMAN_TRIED_BUCKET_COUNT * ADDRMAN_BUCKET_SIZE {
        return Err(());
    }

    Ok((n_new, n_tried))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn bitcoin_addrman_serialize(
    data: *const u8,
    len: usize,
) -> *mut c_char {
    let bytes = slice::from_raw_parts(data, len);
    match parse_addrman(bytes) {
        Ok((n_new, n_tried)) => {
            let result = format!("new={};tried={}", n_new, n_tried);
            str_to_c_string(&result)
        }
        Err(_) => str_to_c_string("INVALID"),
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn bitcoin_addrman_free_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        let _ = CString::from_raw(ptr);
    }
}
