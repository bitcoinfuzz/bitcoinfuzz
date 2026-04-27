#ifndef BITCOINFUZZ_CUSTOMMUTATOR_EXTENDED_KEY_H
#define BITCOINFUZZ_CUSTOMMUTATOR_EXTENDED_KEY_H
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <custommutator/utils/base58.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>
extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);
constexpr size_t VERSION_PREFIX_LEN = 4;
constexpr size_t BIP32_PAYLOAD_LEN = 78;
struct VersionBytes {
  uint8_t data[VERSION_PREFIX_LEN];
};
constexpr std::array<VersionBytes, 4> VERSION_PREFIXES = {{
    {{0x04, 0x88, 0xB2, 0x1E}}, // xpub
    {{0x04, 0x88, 0xAD, 0xE4}}, // xprv
    {{0x04, 0x35, 0x87, 0xCF}}, // tpub
    {{0x04, 0x35, 0x83, 0x94}}, // tprv
}};
constexpr size_t IDX_XPUB = 0;
constexpr size_t IDX_XPRV = 1;
constexpr size_t IDX_TPUB = 2;
constexpr size_t IDX_TPRV = 3;
const VersionBytes &GetVersionBytes(std::string_view key_type) {
  if (key_type == "xprv")
    return VERSION_PREFIXES[IDX_XPRV];
  if (key_type == "tpub")
    return VERSION_PREFIXES[IDX_TPUB];
  if (key_type == "tprv")
    return VERSION_PREFIXES[IDX_TPRV];
  return VERSION_PREFIXES[IDX_XPUB];
}
const VersionBytes &GetRandomVersion(unsigned int seed) {
  return VERSION_PREFIXES[seed % VERSION_PREFIXES.size()];
}
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size,
                                          size_t max_size, unsigned int seed) {

  // Decode input from base58check
  std::string_view input{reinterpret_cast<const char *>(fuzz_data), size};
  std::vector<unsigned char> payload(BIP32_PAYLOAD_LEN, 0x00);

  std::vector<unsigned char> decoded;
  if (DecodeBase58Check(std::string{input}, decoded,
                        static_cast<int>(max_size))) {
    const size_t copy_len = std::min(decoded.size(), BIP32_PAYLOAD_LEN);
    std::memcpy(payload.data(), decoded.data(), copy_len);
  } else {
    const size_t copy_len = std::min(size, BIP32_PAYLOAD_LEN);
    std::memcpy(payload.data(), fuzz_data, copy_len);
  }

  // Mutate the payload
  size_t mutated_size =
      LLVMFuzzerMutate(payload.data(), BIP32_PAYLOAD_LEN, BIP32_PAYLOAD_LEN);

  // Ensure we have exactly BIP32_PAYLOAD_LEN bytes
  if (mutated_size < BIP32_PAYLOAD_LEN) {
    std::memset(payload.data() + mutated_size, 0,
                BIP32_PAYLOAD_LEN - mutated_size);
  }
  payload.resize(BIP32_PAYLOAD_LEN);

  // Apply version prefix: use env if set, otherwise random
  const char *env = std::getenv("EXTKEYTYPE");
  const auto &version = env ? GetVersionBytes(env) : GetRandomVersion(seed);
  std::memcpy(payload.data(), version.data, VERSION_PREFIX_LEN);

  // Encode back to base58check
  std::string encoded =
      EncodeBase58Check(std::span<const unsigned char>{payload});

  // Copy result to output buffer
  const size_t output_len = std::min(encoded.size(), max_size);
  std::memcpy(fuzz_data, encoded.data(), output_len);
  return output_len;
}
#endif // BITCOINFUZZ_CUSTOMMUTATOR_EXTENDED_KEY_H