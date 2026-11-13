#include "module.h"

#include <bitcoinfuzz/result.h>
#include <bitcoinkernel.h>

#include <dlfcn.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <span>
#include <sstream>
#include <string>
#include <vector>

// btc-verified implements the kernel transaction slice in Lean and ships it as
// a shared library exporting the unprefixed btck_* names of the upstream
// header. Load it locally and call it only through this table, so it can never
// bind to (or be bound by) another module's copy of libbitcoinkernel. The
// library is never unloaded: Lean's runtime and module constants live for the
// whole process.

#define BTC_VERIFIED_SYMBOLS(X)                                                \
  X(btck_transaction_create)                                                   \
  X(btck_transaction_copy)                                                     \
  X(btck_transaction_destroy)                                                  \
  X(btck_transaction_to_bytes)                                                 \
  X(btck_transaction_check)                                                    \
  X(btck_tx_validation_state_create)                                           \
  X(btck_tx_validation_state_destroy)                                          \
  X(btck_tx_validation_state_get_validation_mode)                              \
  X(btck_tx_validation_state_get_tx_validation_result)

namespace {
struct Provider {
#define DECLARE(name) decltype(&name) name;
  BTC_VERIFIED_SYMBOLS(DECLARE)
#undef DECLARE
};

Provider provider;

[[noreturn]] void infrastructure(const char *message) {
  std::fprintf(stderr, "btc-verified infrastructure error: %s\n", message);
  std::abort();
}

// Load before any default-priority constructor runs. AFL++'s runtime starts
// its fork server from one when the binary runs outside afl-fuzz (e.g. when
// replaying a crash), and aborts if an instrumented library is dlopen()ed
// after that; loading this early also keeps the library's coverage
// registered before libFuzzer or AFL++ start fuzzing.
__attribute__((constructor(101))) void load_provider() {
  const char *path = std::getenv("BTC_VERIFIED_KERNEL_LIB");
  if (path == nullptr || path[0] == '\0')
    path = BTC_VERIFIED_KERNEL_LIB;
  void *library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (library == nullptr)
    infrastructure(dlerror());
#define LOAD(name)                                                             \
  provider.name = reinterpret_cast<decltype(&name)>(dlsym(library, #name));    \
  if (provider.name == nullptr)                                                \
    infrastructure("missing symbol: " #name);
  BTC_VERIFIED_SYMBOLS(LOAD)
#undef LOAD
}

std::string bytes_to_hex(std::span<const unsigned char> bytes) {
  std::stringstream string_stream;
  string_stream << std::hex;
  for (const auto byte : bytes) {
    string_stream << std::setw(2) << std::setfill('0')
                  << static_cast<int>(byte);
  }

  return string_stream.str();
}

int collect_bytes(const void *bytes, size_t size, void *context) {
  auto &output = *static_cast<std::vector<unsigned char> *>(context);
  const auto *first = static_cast<const unsigned char *>(bytes);
  output.insert(output.end(), first, first + size);
  return 0;
}

// Opt-in negative control: flips the observed check verdict after both calls
// completed, so the driver must report a mismatch against Core.
bool inject_mismatch() {
  const char *value = std::getenv("BTC_VERIFIED_INJECT_MISMATCH");
  return value != nullptr && value[0] != '\0' && std::strcmp(value, "0") != 0;
}

// A parseable but invalid transaction (version 1, no inputs or outputs).
constexpr unsigned char empty_transaction[10] = {1};
} // namespace

namespace bitcoinfuzz {
namespace module {
BtcVerified::BtcVerified(void) : BaseModule("BtcVerified") {}

std::optional<std::string>
BtcVerified::kernel_transaction_check(std::span<const uint8_t> buffer) const {
  btck_Transaction *original =
      provider.btck_transaction_create(buffer.data(), buffer.size());
  if (original == nullptr)
    return Fail("parse");
  btck_Transaction *transaction = provider.btck_transaction_copy(original);
  if (transaction == nullptr)
    infrastructure("transaction copy failed");
  provider.btck_transaction_destroy(original);

  btck_TxValidationState *state = provider.btck_tx_validation_state_create();
  if (state == nullptr)
    infrastructure("validation state allocation failed");
  btck_Transaction *prior = provider.btck_transaction_create(
      empty_transaction, sizeof(empty_transaction));
  if (prior == nullptr)
    infrastructure("state-primer transaction did not parse");
  (void)provider.btck_transaction_check(prior, state);
  provider.btck_transaction_destroy(prior);

  std::vector<unsigned char> transaction_bytes;
  const int serialization = provider.btck_transaction_to_bytes(
      transaction, collect_bytes, &transaction_bytes);
  bool ok = provider.btck_transaction_check(transaction, state) == 1;
  const auto mode =
      provider.btck_tx_validation_state_get_validation_mode(state);
  const auto validation_result =
      provider.btck_tx_validation_state_get_tx_validation_result(state);
  provider.btck_tx_validation_state_destroy(state);
  provider.btck_transaction_destroy(transaction);

  if (serialization != 0)
    return Fail("serialize");

  if (inject_mismatch())
    ok = !ok;

  std::string result = "bytes=";
  result.append(bytes_to_hex(transaction_bytes));
  result.append(";verdict=");
  result.append(ok ? "1" : "0");
  result.append(";mode=");
  result.append(std::to_string(static_cast<int>(mode)));
  result.append(";result=");
  result.append(std::to_string(static_cast<int>(validation_result)));
  return Ok(result);
}

} // namespace module
} // namespace bitcoinfuzz
