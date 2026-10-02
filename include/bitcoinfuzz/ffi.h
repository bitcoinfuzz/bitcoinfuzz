// Result every C-ABI wrapper (Rust, Go, C#) returns to its module.cpp. The
// status codes are shared by every language mirror: ffi.rs, BfResult.cs,
// BfResult.java, bfresult.py and the bf* helpers in each Go wrapper. The
// static_assert below guards the struct layout the C-ABI mirrors hardcode.
#ifndef BITCOINFUZZ_FFI_H
#define BITCOINFUZZ_FFI_H

#include <stddef.h>
#include <stdint.h>

enum { BF_OK = 0, BF_SKIP = 1, BF_FAIL = 2 };

// data holds the BF_OK value or the optional BF_FAIL reason. It is allocated
// with malloc by the wrapper and released by the harness with free, so wrappers
// export no free function. It is NULL for BF_SKIP and may be NULL when empty.
typedef struct {
  uint8_t status;
  char *data;
  size_t len;
} bf_result;

#ifdef __cplusplus
#include <bitcoinfuzz/result.h>
#include <cassert>
#include <cstdlib>
#include <optional>
#include <string>

static_assert(sizeof(bf_result) == 24 && offsetof(bf_result, len) == 16);

namespace bitcoinfuzz {
// Takes ownership of result.data and maps the status to its outcome.
inline std::optional<std::string> TakeResult(bf_result result) {
  assert(result.status == BF_OK || result.status == BF_SKIP ||
         result.status == BF_FAIL);
  std::string data(result.data, result.len);
  std::free(result.data);
  if (result.status == BF_SKIP)
    return Skip();
  return result.status == BF_OK ? Ok(std::move(data)) : Fail(std::move(data));
}
} // namespace bitcoinfuzz
#endif

#endif // BITCOINFUZZ_FFI_H
