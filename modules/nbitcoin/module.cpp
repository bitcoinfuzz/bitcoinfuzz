#include "module.h"
#include "NBitcoin/nbitcoin_lib.h"
#include <iostream>
#include <span>

namespace bitcoinfuzz {
namespace module {
NBitcoin::NBitcoin(void) : BaseModule("NBitcoin") {}
std::optional<bool> NBitcoin::miniscript_parse(std::string str) const {
  return nbitcoin_miniscript_parse(str.c_str());
}
std::optional<bool> NBitcoin::descriptor_parse(std::string str) const {
  return nbitcoin_descriptor_parse(str.c_str());
}
std::optional<bool>
NBitcoin::verify_script(const std::vector<uint8_t> &script_sig,
                        const std::vector<uint8_t> &script_pubkey) const {
  return nbitcoin_verify_script(script_sig.data(), script_sig.size(),
                                script_pubkey.data(), script_pubkey.size());
}
std::optional<bool>
NBitcoin::script_eval(const std::vector<uint8_t> &input_data,
                      unsigned int flags, size_t version) const {
  return nbitcoin_script_eval(input_data.data(), input_data.size(), flags,
                              version);
}
std::optional<std::string>
NBitcoin::bip32_master_keygen(std::span<const uint8_t> buffer) const {
  char *p = nbitcoin_bip32_master_keygen(buffer.data(), buffer.size());
  if (!p)
    return std::nullopt;
  std::string s(p);
  nbitcoin_free_c_string(p);
  return s;
}
std::optional<std::string>
NBitcoin::psbt_parse(std::span<const uint8_t> buffer) const {
  char *p = nbitcoin_psbt_parse(buffer.data(), buffer.size());
  if (!p)
    return std::nullopt;
  std::string s(p);
  nbitcoin_free_c_string(p);
  return s;
}
std::optional<std::string> NBitcoin::bip32_deserialize_extended_key(
    std::span<const uint8_t> buffer) const {
  char *p =
      nbitcoin_bip32_deserialize_extended_key(buffer.data(), buffer.size());
  if (!p)
    return std::nullopt;
  std::string s(p);
  nbitcoin_free_c_string(p);
  return s;
}
std::optional<std::string>
NBitcoin::sign_schnorr(std::span<const uint8_t> buffer,
                       std::span<const uint8_t> hash,
                       std::span<const uint8_t> aux) const {
  char *p = nbitcoin_sign_schnorr(buffer.data(), hash.data(), aux.data());
  if (!p)
    return std::nullopt;
  std::string s(p);
  nbitcoin_free_c_string(p);
  return s;
}
std::optional<std::string>
NBitcoin::bip32_derive_from_path(std::span<const uint8_t> buffer) const {
  char *p = nbitcoin_bip32_derive_from_path(buffer.data(), buffer.size());
  if (!p)
    return std::nullopt;
  std::string s(p);
  nbitcoin_free_c_string(p);
  return s;
}
} // namespace module
} // namespace bitcoinfuzz
