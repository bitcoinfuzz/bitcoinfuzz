#include <cstring>

#include "decredsecp256k1_wrapper/libdecredsecp256k1_wrapper.h"
#include "module.h"

namespace bitcoinfuzz {
namespace module {
Decred_secp256k1::Decred_secp256k1(void) : BaseModule("Decred_secp256k1") {}

std::optional<std::string>
Decred_secp256k1::private_to_public_key(std::span<const uint8_t> buffer) const {
  ByteArray buffer_data{
      .data = reinterpret_cast<char *>(const_cast<uint8_t *>(buffer.data())),
      .length = static_cast<int>(buffer.size())};

  char *result = DecredPrivateToPublicKey(buffer_data);
  if (!result)
    return std::nullopt;

  std::string result_str(result);
  free(result);
  return result_str;
}

std::optional<std::string>
Decred_secp256k1::sign_compact(std::span<const uint8_t> buffer,
                               std::span<const uint8_t> hash) const {
  ByteArray buffer_data{
      .data = reinterpret_cast<char *>(const_cast<uint8_t *>(buffer.data())),
      .length = static_cast<int>(buffer.size())};

  ByteArray hash_data{
      .data = reinterpret_cast<char *>(const_cast<uint8_t *>(hash.data())),
      .length = static_cast<int>(hash.size())};

  char *result = DecredSignCompact(buffer_data, hash_data);
  if (!result)
    return std::nullopt;

  std::string result_str(result);
  free(result);
  return result_str;
}

std::optional<std::string>
Decred_secp256k1::sign_der(std::span<const uint8_t> buffer,
                           std::span<const uint8_t> hash) const {
  ByteArray buffer_data{
      .data = reinterpret_cast<char *>(const_cast<uint8_t *>(buffer.data())),
      .length = static_cast<int>(buffer.size())};

  ByteArray hash_data{
      .data = reinterpret_cast<char *>(const_cast<uint8_t *>(hash.data())),
      .length = static_cast<int>(hash.size())};

  char *result = DecredSignDER(buffer_data, hash_data);
  if (!result)
    return std::nullopt;

  std::string result_str(result);
  free(result);
  return result_str;
}

std::optional<bool>
Decred_secp256k1::sign_verify(std::span<const uint8_t> buffer,
                              std::span<const uint8_t> hash,
                              std::span<const uint8_t> sign) const {
  ByteArray buffer_data{
      .data = reinterpret_cast<char *>(const_cast<uint8_t *>(buffer.data())),
      .length = static_cast<int>(buffer.size())};

  ByteArray hash_data{
      .data = reinterpret_cast<char *>(const_cast<uint8_t *>(hash.data())),
      .length = static_cast<int>(hash.size())};

  ByteArray sign_data{
      .data = reinterpret_cast<char *>(const_cast<uint8_t *>(sign.data())),
      .length = static_cast<int>(sign.size())};

  return DecredSignVerify(buffer_data, hash_data, sign_data) == 1;
}

std::optional<std::string>
Decred_secp256k1::ecdh(std::span<const uint8_t> buffer,
                       std::span<const uint8_t> pubkey) const {
  ByteArray buffer_data{
      .data = reinterpret_cast<char *>(const_cast<uint8_t *>(buffer.data())),
      .length = static_cast<int>(buffer.size())};

  ByteArray pubkey_data{
      .data = reinterpret_cast<char *>(const_cast<uint8_t *>(pubkey.data())),
      .length = static_cast<int>(pubkey.size())};

  char *result = DecredECDH(buffer_data, pubkey_data);
  if (!result)
    return std::nullopt;

  std::string result_str(result);
  free(result);
  return result_str;
}
} // namespace module
} // namespace bitcoinfuzz
