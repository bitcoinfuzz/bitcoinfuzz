#include "module.h"
#include "bitcoinkernel_wrapper/bitcoinkernel_wrapper.h"
#include <dlfcn.h>
#include <iomanip>
#include <memory>
#include <span>
#include <sstream>
#include <string.h>
#include <unordered_map>

struct btck_Block;
struct btck_BlockHash;
struct btck_Transaction;
struct btck_Txid;

using btck_WriteBytes = int (*)(const void *, size_t, void *);

using btck_block_create_signature = btck_Block *(*)(const void *, size_t);
using btck_block_copy_signature = btck_Block *(*)(const btck_Block *);
using btck_block_count_transactions_signature = size_t (*)(const btck_Block *);
using btck_block_get_transaction_at_signature =
    const btck_Transaction *(*)(const btck_Block *, size_t);
using btck_block_get_hash_signature = btck_BlockHash *(*)(const btck_Block *);
using btck_block_to_bytes_signature = int (*)(const btck_Block *,
                                              btck_WriteBytes, void *);
using btck_block_hash_to_bytes_signature = void (*)(const btck_BlockHash *,
                                                    unsigned char[32]);
using btck_block_hash_destroy_signature = void (*)(btck_BlockHash *);
using btck_block_hash_copy_signature =
    btck_BlockHash *(*)(const btck_BlockHash *);
using btck_block_hash_equals_signature = int (*)(const btck_BlockHash *,
                                                 const btck_BlockHash *);
using btck_block_destroy_signature = void (*)(btck_Block *);
using btck_transaction_get_txid_signature =
    const btck_Txid *(*)(const btck_Transaction *);
using btck_txid_copy_signature = btck_Txid *(*)(const btck_Txid *);
using btck_txid_equals_signature = int (*)(const btck_Txid *,
                                           const btck_Txid *);
using btck_txid_to_bytes_signature = void (*)(const btck_Txid *,
                                              unsigned char[32]);
using btck_txid_destroy_signature = void (*)(btck_Txid *);
using btck_library_tag_signature = const char *(*)(void);

struct KernelApiTable {
  void *handle{};
  btck_block_create_signature block_create{};
  btck_block_copy_signature block_copy{};
  btck_block_count_transactions_signature block_count_transactions{};
  btck_block_get_transaction_at_signature block_get_transaction_at{};
  btck_block_get_hash_signature block_get_hash{};
  btck_block_to_bytes_signature block_to_bytes{};
  btck_block_hash_copy_signature block_hash_copy{};
  btck_block_hash_equals_signature block_hash_equals{};
  btck_block_hash_to_bytes_signature block_hash_to_bytes{};
  btck_block_hash_destroy_signature block_hash_destroy{};
  btck_block_destroy_signature block_destroy{};
  btck_transaction_get_txid_signature transaction_get_txid{};
  btck_txid_copy_signature txid_copy{};
  btck_txid_equals_signature txid_equals{};
  btck_txid_to_bytes_signature txid_to_bytes{};
  btck_txid_destroy_signature txid_destroy{};
  // showcase function, will be removed later
  btck_library_tag_signature library_tag{};
};

static void *load_symbol(void *handle, const char *name) {
  dlerror();
  void *sym = dlsym(handle, name);
  const char *err = dlerror();
  if (err) {
    throw std::runtime_error(err);
  }
  return sym;
}

static std::string bytes_to_hex(const uint8_t *bytes, int length) {
  std::stringstream string_stream;
  string_stream << std::hex;
  for (int i = length - 1; i >= 0; --i) {
    string_stream << std::setw(2) << std::setfill('0') << (int)bytes[i];
  }

  return string_stream.str();
}

// Is it safe to assume that the tests will always be single threaded?
// If not, we may need to use thread_local storage here, or a mutex to protect
// access to this variable.
static const KernelApiTable *active_api_table = nullptr;

class KernelApiTableGuard {
public:
  KernelApiTableGuard(const KernelApiTable &api_table)
      : previous_api_table(active_api_table) {
    active_api_table = &api_table;
  }
  ~KernelApiTableGuard() { active_api_table = previous_api_table; }

private:
  const KernelApiTable *previous_api_table;
};

namespace bitcoinfuzz {
namespace module {

DynamicKernel::DynamicKernel(std::string const &name,
                             std::string const &library_path)
    : BaseModule(name) {
  // Cache both the dlmopen handle and the resolved function pointers per
  // library
  static std::unordered_map<std::string, std::shared_ptr<const KernelApiTable>>
      api_tables_cache;
  std::shared_ptr<const KernelApiTable> api_table;

  auto cached_api_table = api_tables_cache.find(library_path);
  if (cached_api_table != api_tables_cache.end()) {
    api_table = cached_api_table->second;
  } else {
    dlerror();
    void *handle = dlopen(library_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
      throw std::runtime_error(dlerror());
    }
    auto new_api_table = std::make_shared<KernelApiTable>();
    new_api_table->handle = handle;
    try {
      new_api_table->block_create =
          (btck_block_create_signature)load_symbol(handle, "btck_block_create");
      new_api_table->block_copy =
          (btck_block_copy_signature)load_symbol(handle, "btck_block_copy");
      new_api_table->block_count_transactions =
          (btck_block_count_transactions_signature)load_symbol(
              handle, "btck_block_count_transactions");
      new_api_table->block_get_transaction_at =
          (btck_block_get_transaction_at_signature)load_symbol(
              handle, "btck_block_get_transaction_at");
      new_api_table->block_get_hash =
          (btck_block_get_hash_signature)load_symbol(handle,
                                                     "btck_block_get_hash");
      new_api_table->block_to_bytes =
          (btck_block_to_bytes_signature)load_symbol(handle,
                                                     "btck_block_to_bytes");
      new_api_table->block_hash_copy =
          (btck_block_hash_copy_signature)load_symbol(handle,
                                                      "btck_block_hash_copy");
      new_api_table->block_hash_equals =
          (btck_block_hash_equals_signature)load_symbol(
              handle, "btck_block_hash_equals");
      new_api_table->block_hash_to_bytes =
          (btck_block_hash_to_bytes_signature)load_symbol(
              handle, "btck_block_hash_to_bytes");
      new_api_table->block_hash_destroy =
          (btck_block_hash_destroy_signature)load_symbol(
              handle, "btck_block_hash_destroy");
      new_api_table->block_destroy = (btck_block_destroy_signature)load_symbol(
          handle, "btck_block_destroy");
      new_api_table->transaction_get_txid =
          (btck_transaction_get_txid_signature)load_symbol(
              handle, "btck_transaction_get_txid");
      new_api_table->txid_copy =
          (btck_txid_copy_signature)load_symbol(handle, "btck_txid_copy");
      new_api_table->txid_equals =
          (btck_txid_equals_signature)load_symbol(handle, "btck_txid_equals");
      new_api_table->txid_to_bytes = (btck_txid_to_bytes_signature)load_symbol(
          handle, "btck_txid_to_bytes");
      new_api_table->txid_destroy =
          (btck_txid_destroy_signature)load_symbol(handle, "btck_txid_destroy");
      new_api_table->library_tag =
          (btck_library_tag_signature)load_symbol(handle, "btck_library_tag");
    } catch (const std::runtime_error &) {
      dlclose(handle);
      throw;
    }

    api_tables_cache.emplace(library_path, new_api_table);
    api_table = std::move(new_api_table);
  }

  this->loaded_api_table = api_table;
}
DynamicKernel::~DynamicKernel() noexcept {
  // If we close the handle here, the cached funtcion pointer will become
  // invalid and we'll have to load the libraries again for each run so we won't
  // destroy the handle.
  //
  // if(this->loaded_api_table && this->loaded_api_table->handle) {
  //     dlclose(this->loaded_api_table->handle);
  // }
}

char *
DynamicKernel::libbitcoinkernel_block(std::span<const uint8_t> buffer) const {
  KernelApiTableGuard guard{*loaded_api_table};
  try {
    std::span<const std::byte> raw_span{(const std::byte *)buffer.data(),
                                        buffer.size()};
    btck::Block block{raw_span};

    const auto block_hash_bytes = block.GetHash().ToBytes();
    std::string result =
        bytes_to_hex((const uint8_t *)block_hash_bytes.data(),
                     static_cast<int>(block_hash_bytes.size()));

    const auto txs = block.Transactions();
    for (const auto &tx : txs) {
      const auto txid_bytes = tx.Txid().ToBytes();
      std::cout << std::endl;
      result.append("txid=");
      result.append(bytes_to_hex((const uint8_t *)txid_bytes.data(),
                                 static_cast<int>(txid_bytes.size())));
      result.push_back(';');
    }
    return strdup(result.c_str());

  } catch (...) {
    return strdup("0");
  }
}

std::optional<std::string>
DynamicKernel::kernel_block(std::span<const uint8_t> buffer) const {

  auto result_ptr = libbitcoinkernel_block(buffer);
  if (result_ptr == nullptr)
    return std::nullopt;

  std::string result(result_ptr);
  free(result_ptr);
  return result;
}

// showcase function
std::optional<std::string> DynamicKernel::library_tag() const {
  KernelApiTableGuard guard{*loaded_api_table};
  return btck_library_tag();
}
} // namespace module
} // namespace bitcoinfuzz

// Function calls match the C API the wrapper expects and forward to the active
// table.
extern "C" {
btck_Block *btck_block_create(const void *raw_block, size_t raw_block_len) {
  return active_api_table->block_create(raw_block, raw_block_len);
}
btck_Block *btck_block_copy(const btck_Block *block) {
  return active_api_table->block_copy(block);
}
size_t btck_block_count_transactions(const btck_Block *block) {
  return active_api_table->block_count_transactions(block);
}
const btck_Transaction *btck_block_get_transaction_at(const btck_Block *block,
                                                      size_t idx) {
  return active_api_table->block_get_transaction_at(block, idx);
}
btck_BlockHash *btck_block_get_hash(const btck_Block *block) {
  return active_api_table->block_get_hash(block);
}
int btck_block_to_bytes(const btck_Block *block, btck_WriteBytes writer,
                        void *user_data) {
  return active_api_table->block_to_bytes(block, writer, user_data);
}
void btck_block_destroy(btck_Block *block) {
  active_api_table->block_destroy(block);
}
btck_BlockHash *btck_block_hash_copy(const btck_BlockHash *hash) {
  return active_api_table->block_hash_copy(hash);
}
int btck_block_hash_equals(const btck_BlockHash *h1, const btck_BlockHash *h2) {
  return active_api_table->block_hash_equals(h1, h2);
}
void btck_block_hash_to_bytes(const btck_BlockHash *hash,
                              unsigned char output[32]) {
  active_api_table->block_hash_to_bytes(hash, output);
}
void btck_block_hash_destroy(btck_BlockHash *hash) {
  active_api_table->block_hash_destroy(hash);
}
const btck_Txid *btck_transaction_get_txid(const btck_Transaction *tx) {
  return active_api_table->transaction_get_txid(tx);
}
btck_Txid *btck_txid_copy(const btck_Txid *txid) {
  return active_api_table->txid_copy(txid);
}
int btck_txid_equals(const btck_Txid *txid1, const btck_Txid *txid2) {
  return active_api_table->txid_equals(txid1, txid2);
}
void btck_txid_to_bytes(const btck_Txid *txid, unsigned char output[32]) {
  active_api_table->txid_to_bytes(txid, output);
}
void btck_txid_destroy(btck_Txid *txid) {
  active_api_table->txid_destroy(txid);
}
const char *btck_library_tag(void) { return active_api_table->library_tag(); }
}
