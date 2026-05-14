#define template c_template // avoid C++ keyword conflict just during includes
// Elements support must be disabled and the user must define
// `WALLY_ABI_NO_ELEMENTS` before including all wally header files.
#define WALLY_ABI_NO_ELEMENTS
extern "C" {
#include <ccan/str/hex/hex.h>
#include <wally_bip32.h>
#include <wally_psbt.h>
}

#undef WALLY_ABI_NO_ELEMENTS
#undef template

#include "module.h"
#include <algorithm>
#include <assert.h>
#include <iomanip>
#include <sstream>

namespace bitcoinfuzz {
namespace module {
LibwallyCore::LibwallyCore(void) : BaseModule("LibwallyCore") {}

std::optional<std::string>
LibwallyCore::psbt_parse(std::span<const uint8_t> buffer) const {
  struct wally_psbt *psbt;
  int res = wally_psbt_from_bytes(buffer.data(), buffer.size(),
                                  WALLY_PSBT_PARSE_FLAG_STRICT, &psbt);
  if (res != WALLY_OK) {
    return std::nullopt;
  }

  // PSBT v2 (BIP-370) has no global transaction. Tx data is per-input/output
  // This harness only supports PSBT v0 format
  if (!psbt->tx) {
    wally_psbt_free(psbt);
    return std::nullopt;
  }

  std::ostringstream result;
  result << "lt=" << psbt->tx->locktime << ";";
  result << "in=" << psbt->tx->num_inputs << ";";
  result << "out=" << psbt->tx->num_outputs << ";";

  for (size_t i = 0; i < psbt->tx->num_inputs; i++) {
    if (i < psbt->num_inputs) {
      wally_tx_input txin = psbt->tx->inputs[i];
      wally_psbt_input psbt_input = psbt->inputs[i];
      std::array<uint8_t, 32> txhash_reversed;
      std::reverse_copy(txin.txhash, txin.txhash + 32, txhash_reversed.begin());

      std::vector<char> txhash_hex(65);
      bool ok = hex_encode(txhash_reversed.data(), 32, txhash_hex.data(),
                           txhash_hex.size());
      assert(ok);
      result << "in" << i << "prev=" << txhash_hex.data() << ":" << txin.index
             << ";";
      result << "in" << i << "seq=" << txin.sequence << ";";

      if (psbt_input.utxo || psbt_input.witness_utxo) {
        result << "in" << i << "utxo=1" << ";";
      }

      result << "in" << i << "sigs=" << psbt->inputs[i].signatures.num_items
             << ";";
    }
  }

  for (size_t i = 0; i < psbt->tx->num_outputs; i++) {
    wally_tx_output txout = psbt->tx->outputs[i];
    size_t hex_len = txout.script_len * 2 + 1;
    std::vector<char> script_hex(hex_len);

    bool ok = hex_encode(txout.script, txout.script_len, script_hex.data(),
                         script_hex.size());

    assert(ok);
    result << "out" << i << "val=" << txout.satoshi << ";";
    result << "out" << i << "script=" << script_hex.data() << ";";
  }

  wally_psbt_free(psbt);

  return result.str();
}
std::optional<std::string>
LibwallyCore::bip32_master_keygen(std::span<const uint8_t> seed) const {
  struct ext_key *master_key = nullptr;
  if (seed.size() != BIP32_ENTROPY_LEN_128 &&
      seed.size() != BIP32_ENTROPY_LEN_256 &&
      seed.size() != BIP32_ENTROPY_LEN_512) {
    return std::nullopt; // libwally accepts only 128, 256 or 512 bit seeds, see
                         // is_valid_seed_len(size_t len) in bip32.c
  }
  if (bip32_key_from_seed_alloc(seed.data(), seed.size(),
                                BIP32_VER_MAIN_PRIVATE, 0,
                                &master_key) != WALLY_OK) {
    return "INVALID";
  }

  char *base58 = nullptr;

  int res = bip32_key_to_base58(master_key,
                                0, // 0 = private key
                                &base58);

  wally_free(master_key);

  if (res != WALLY_OK || !base58)
    return "INVALID";

  std::string result(base58);
  wally_free(base58);

  return result;
}

std::optional<std::string> LibwallyCore::bip32_deserialize_extended_key(
    std::span<const uint8_t> buffer) const {

  struct ext_key key;

  int res = bip32_key_from_base58_n(
      reinterpret_cast<const char *>(buffer.data()), buffer.size(), &key);

  if (res != WALLY_OK) {
    return "INVALID";
  }

  std::ostringstream result;

  result << std::hex << std::setfill('0');

  result << "depth=" << std::setw(2) << (int)key.depth << ";";
  result << "fp=";
  for (int i = 0; i < 4; ++i)
    result << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(key.parent160[i]);
  result << ";";
  result << "child=" << std::setw(8) << key.child_num << ";";

  result << "chaincode=";
  for (size_t i = 0; i < sizeof(key.chain_code); ++i)
    result << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(key.chain_code[i]);
  result << ";";

  bool is_private = key.version == BIP32_VER_MAIN_PRIVATE ||
                    key.version == BIP32_VER_TEST_PRIVATE;

  result << "key=";
  if (is_private) {
    for (size_t i = 1; i < sizeof(key.priv_key); ++i) // skip prefix byte
      result << std::setw(2) << std::setfill('0')
             << static_cast<int>(key.priv_key[i]);
  } else {
    for (size_t i = 0; i < sizeof(key.pub_key); ++i)
      result << std::setw(2) << std::setfill('0')
             << static_cast<int>(key.pub_key[i]);
  }
  return result.str();
}

} // namespace module
} // namespace bitcoinfuzz
