#include <algorithm>
#include <memory>
#include <cstring>
#include "driver.h"
#include <bitcoinfuzz/basemodule.h>

#ifdef BITCOIN_CORE
#include <modules/bitcoin/module.h>
#endif

#ifdef RUST_BITCOIN
#include <modules/rustbitcoin/module.h>
#endif

#ifdef RUST_MINISCRIPT
#include <modules/rustminiscript/module.h>
#endif

#ifdef BTCD
#include <modules/btcd/module.h>
#endif

#ifdef NBITCOIN
#include <modules/nbitcoin/module.h>
#endif

#ifdef LND
#include <modules/lnd/module.h>
#endif

#ifdef LDK
#include <modules/ldk/module.h>
#endif

#ifdef ECLAIR
#include <modules/eclair/module.h>
#endif

#ifdef NLIGHTNING
#include <modules/nlightning/module.h>
#endif

#ifdef EMBIT
#include <modules/embit/module.h>
#endif

#ifdef CLIGHTNING
#include <modules/clightning/module.h>
#endif

#if defined(CUSTOM_MUTATOR_BOLT11) || defined(CUSTOM_MUTATOR_BOLT12_OFFER)
#include <modules/custommutator/bech32.h>
#include <modules/custommutator/customcrossover.h>
#endif

std::shared_ptr<bitcoinfuzz::Driver> driver = nullptr;

static bitcoinfuzz::ModuleLogger module_logger;

#ifdef CUSTOM_MUTATOR_BOLT12_OFFER
constexpr std::string_view bech32_hrp = "lno";
constexpr std::string_view dummy_initial_input = "lno1";

// A custom mutator that decodes the bech32 input, mutates the decoded input,
// and then re-encodes the mutated input. This produces an input corpus that
// consists entirely of correctly encoded bech32 strings, enabling efficient
// fuzzing of the bolt12 decoding logic without the fuzzer getting stuck on
// fuzzing the bech32 decoding logic. This custom mutator is originally from
// core-lightning:
// https://github.com/ElementsProject/lightning/blob/release-v25.02.2/tests/fuzz/bolt12.h#L54
extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                                          unsigned int seed);
extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t *in1, size_t in1_size, const uint8_t *in2,
                                            size_t in2_size, uint8_t *out, size_t max_out_size,
                                            unsigned seed);

/* Encodes a dummy bolt12 offer/invoice-request/invoice into fuzz_data and
 * returns the size of the encoded string. */
static size_t initial_input(uint8_t *fuzz_data, size_t max_size)
{
    size_t output_size = std::min(max_size, dummy_initial_input.size());
    std::memcpy(fuzz_data, dummy_initial_input.data(), output_size);

    return output_size;
}

/* A custom mutator that decodes the bech32 input, mutates the decoded input,
 * and then re-encodes the mutated input. This produces an input corpus that
 * consists entirely of correctly encoded bech32 strings, enabling efficient
 * fuzzing of the bolt12 decoding logic without the fuzzer getting stuck on
 * fuzzing the bech32 decoding logic. */
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size,
                                          size_t max_size, unsigned int seed)
{
    std::string_view input_str(reinterpret_cast<char *>(fuzz_data), size);

    // Decode the input
    bech32::DecodeResult decoded_result = bech32::DecodeNoChecksum(input_str, bech32::CharLimit::CUSTOM_MUTATOR);
    if (decoded_result.encoding != bech32::Encoding::BECH32) {
        // Decoding failed, this should only happen when starting from
        // an empty corpus.
        return initial_input(fuzz_data, max_size);
    }
    std::vector<uint8_t> &decoded_data = decoded_result.data;

    if (decoded_data.size() > max_size) {
        return initial_input(fuzz_data, max_size);
    }

    // Resize the vector to the maximum possible size BEFORE mutation.
    size_t decoded_data_size = decoded_data.size();
    decoded_data.resize(max_size);

    size_t mutated_size = LLVMFuzzerMutate(decoded_data.data(),
                                           decoded_data_size,
                                           max_size);
    decoded_data.resize(mutated_size);

    // It ensures that all values remain valid 5-bit values
    for (uint8_t& val : decoded_data) {
        val &= 0x1F;
    }

    // Encode the mutated input
    std::string encoded_data = bech32::EncodeNoChecksum(bech32_hrp, decoded_data);

    if (encoded_data.length() > max_size) {
        return initial_input(fuzz_data, max_size);
    }

    std::memcpy(fuzz_data, encoded_data.data(), encoded_data.length());

    return encoded_data.length();
}

/* A custom cross-over mutator that decodes the bech32 inputs before cross-over
 * mutating them. Like LLVMFuzzerCustomMutator, this enables more efficient
 * fuzzing of bolt12 offers, invoice requests, and invoices. */
extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t *in1, size_t in1_size,
                                            const uint8_t *in2, size_t in2_size,
                                            uint8_t *out, size_t max_out_size,
                                            unsigned seed)
{
    // Decode first input
    std::string_view input1_str(reinterpret_cast<const char*>(in1), in1_size);
    bech32::DecodeResult decoded_result1 = bech32::DecodeNoChecksum(input1_str, bech32::CharLimit::CUSTOM_MUTATOR);

    // Decode second input
    std::string_view input2_str(reinterpret_cast<const char*>(in2), in2_size);
    bech32::DecodeResult decoded_result2 = bech32::DecodeNoChecksum(input2_str, bech32::CharLimit::CUSTOM_MUTATOR);

    // Perform cross-over on decoded data
    std::vector<uint8_t> crossed_data = cross_over(decoded_result1.data, decoded_result2.data,
                                                    max_out_size, seed);

    // Encode the crossed-over data
    std::string encoded_data = bech32::EncodeNoChecksum(bech32_hrp, crossed_data);

    if (encoded_data.length() > max_out_size) {
        return 0;
    }

    std::memcpy(out, encoded_data.data(), encoded_data.length());
    return encoded_data.length();
}
#endif


#ifdef CUSTOM_MUTATOR_P2P_MESSAGE
// Custom mutator for Bitcoin P2P messages that maintains valid checksums.
// This enables efficient fuzzing of Bitcoin message parsing logic without
// the fuzzer getting stuck on checksum validation failures.
//
// This custom mutator does the following:
//   1. Mutate the input using libFuzzer's default mutator `LLVMFuzzerMutate`
//   2. If the data is large enough to contain a header, recalculate and fix the checksum
//   3. Return the mutated data with valid checksum

#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <modules/custommutator/crypto/sha256.h>

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                                         unsigned int seed);

// Bitcoin message constants
static constexpr size_t BITCOIN_HEADER_SIZE = 24;
static constexpr size_t PAYLOAD_LENGTH_OFFSET = 16;
static constexpr size_t CHECKSUM_OFFSET = 20;

// Bitcoin double SHA256 using Bitcoin Core's CSHA256 class
static void bitcoin_double_sha256(const uint8_t* data, size_t len, uint8_t* hash) {
    CSHA256 hasher;
    hasher.Write(data, len);
    unsigned char first_hash[CSHA256::OUTPUT_SIZE];
    hasher.Finalize(first_hash);

    hasher.Reset();
    hasher.Write(first_hash, CSHA256::OUTPUT_SIZE);
    hasher.Finalize(hash);
}

// Fix the checksum in a Bitcoin P2P message
static void fix_bitcoin_checksum(uint8_t* data, size_t size) {
    if (size < BITCOIN_HEADER_SIZE) {
        return;
    }

    uint32_t payload_length;
    memcpy(&payload_length, data + PAYLOAD_LENGTH_OFFSET, 4);

    // Validate that we have the complete message
    if (size < BITCOIN_HEADER_SIZE + payload_length) {
        return;
    }

    // Calculate checksum for the payload
    const uint8_t* payload = data + BITCOIN_HEADER_SIZE;
    uint8_t calculated_checksum[32];
    bitcoin_double_sha256(payload, payload_length, calculated_checksum);

    // Update checksum in header (first 4 bytes of hash)
    memcpy(data + CHECKSUM_OFFSET, calculated_checksum, 4);
}

size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                               unsigned int seed) {
    // First, mutate the data using LibFuzzer's default mutator
    size_t new_size = LLVMFuzzerMutate(fuzz_data, size, max_size);

    // Then fix the checksum if the data looks like it could be a Bitcoin message
    fix_bitcoin_checksum(fuzz_data, new_size);

    return new_size;
}
#endif

#ifdef CUSTOM_MUTATOR_BOLT11
// We use a custom mutator to produce an input corpus that consists entirely of
// correctly encoded bech32 strings. This enables us to efficiently fuzz the
// bolt11 decoding logic without the fuzzer getting stuck on fuzzing the bech32
// decoding/encoding logic. This custom mutator is originally from core-lightning:
// https://github.com/ElementsProject/lightning/blob/3a7a1fad4eb56522b6ab590e53c695e2fb08e7e2/tests/fuzz/fuzz-bolt11.c#L59
//
// This custom mutator does the following things:
//   1. Attempt to bech32 decode the given input (returns the encoded dummy
//      invoice on failure).
//   2. Mutate either the human readable or data part of the invoice using
//      libFuzzer's default mutator `LLVMFuzzerMutate`.
//   3. Attempt to bech32 encode the mutated hrp and data (returns the endcoded
//      dummy on failure).
//   4. Write the encoded result to `fuzz_data` if its size does not exceed
//      `max_size`, otherwise return the encoded dummy invoice.
extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
			       unsigned int seed);
extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t *in1, size_t in1_size, const uint8_t *in2,
				 size_t in2_size, uint8_t *out, size_t max_out_size,
				 unsigned seed);

// Encodes a dummy bolt11 invoice into `fuzz_data` and returns the size of the
// encoded string.
static size_t initial_input(uint8_t *fuzz_data, size_t max_size)
{
    constexpr std::string_view dummy =
        "lnbc16lta047pp5h6lta047h6lta047h6lta047h6lta047h6lta047h6lta047"
	    "h6lqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
	    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqxnht6w";

    size_t output_size = std::min(max_size, dummy.size());
    std::memcpy(fuzz_data, dummy.data(), output_size);
    return output_size;
}

size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                               unsigned int seed)
{
    // A minimum size of 9 prevents hrp_maxlen <= 0 and data_maxlen <= 0.
    if (size < 9)
        return initial_input(fuzz_data, max_size);

    // Interpret fuzz input as string
    std::string input(reinterpret_cast<char*>(fuzz_data), size);

    size_t data_maxlen = input.size() - 8;
    size_t hrp_maxlen = input.size() - 6;

    // Attempt to bech32 decode the input
    bech32::DecodeResult decoded = bech32::Decode(input, bech32::CharLimit::CUSTOM_MUTATOR);
    if (decoded.encoding != bech32::Encoding::BECH32) {
        // Decoding failed, this should only happen when starting from
        // an empty corpus.
        return initial_input(fuzz_data, max_size);
    }

    auto data = decoded.data;
    auto hrp = decoded.hrp;

    // Mutate either the hrp or data
    std::srand(seed);
    switch (std::rand() % 2) {
        case 0: { // Mutate hrp
            // Make sure we have a buffer that's large enough for mutation
            std::vector<uint8_t> hrp_buffer(hrp.begin(), hrp.end());
            // Reserve enough space for the maximum mutation size
            hrp_buffer.resize(hrp_maxlen);

            // Mutate the buffer
            size_t new_len = LLVMFuzzerMutate(hrp_buffer.data(),
                                             hrp.size(),
                                             hrp_maxlen);

            // Convert back to string with the new length
            hrp = std::string(reinterpret_cast<char*>(hrp_buffer.data()), new_len);

            // Sanitize hrp - ensure only valid ASCII characters (33-126) and not uppercase
            for (char& c : hrp) {
                if (c < 33 || c > 126) {
                    c = 'a' + (c % 26); // Replace with a valid lowercase letter
                } else if (c >= 'A' && c <= 'Z') {
                    c = c - 'A' + 'a'; // Convert uppercase to lowercase
                }
            }
            break;
        }
        case 1: { // Mutate data
            size_t original_data_size = data.size();
            // Make sure we have a buffer that's large enough for mutation
            data.resize(std::max(data.size(), data_maxlen));

            size_t new_len = LLVMFuzzerMutate(data.data(),
                                                original_data_size,
                                                data_maxlen);

            // It ensures that all values remain valid 5-bit values
            for (auto& val : data) {
                val &= 0x1F;
            }
            break;
        }
    }

    if (!hrp.empty()) {
        for (const char& c : hrp) {
            if (c >= 'A' && c <= 'Z') {
                return initial_input(fuzz_data, max_size);
            }
        }
    }

    std::string output = bech32::Encode(bech32::Encoding::BECH32, hrp, data);

    if (output.length() > max_size) {
        return initial_input(fuzz_data, max_size);
    }

    std::memcpy(fuzz_data, output.data(), output.length());
    return output.length();
}

size_t LLVMFuzzerCustomCrossOver(const uint8_t *in1, size_t in1_size, const uint8_t *in2,
                               size_t in2_size, uint8_t *out, size_t max_out_size,
                               unsigned seed)
{
    if (in1_size < 9 || in2_size < 9)
        return 0;

    // Interpret fuzz inputs as strings
    std::string input1(reinterpret_cast<const char*>(in1), in1_size);
    std::string input2(reinterpret_cast<const char*>(in2), in2_size);

    // Attempt to bech32 decode the inputs
    bech32::DecodeResult result1 = bech32::Decode(input1, bech32::CharLimit::CUSTOM_MUTATOR);
    if (result1.encoding != bech32::Encoding::BECH32) {
        // Decoding failed
        return 0;
    }

    bech32::DecodeResult result2 = bech32::Decode(input2, bech32::CharLimit::CUSTOM_MUTATOR);
    if (result2.encoding != bech32::Encoding::BECH32) {
        // Decoding failed
        return 0;
    }

    std::string hrp1 = result1.hrp;
    std::string hrp2 = result2.hrp;
    std::vector<uint8_t> data1 = result1.data;
    std::vector<uint8_t> data2 = result2.data;

    std::srand(seed);
    std::string out_hrp;
    std::vector<uint8_t> out_data;

    if (std::rand() % 2) {
        // Cross-over the HRP
        out_data = data1;

        std::span<const uint8_t> hrp1_span(reinterpret_cast<const uint8_t*>(hrp1.data()), hrp1.size());
        std::span<const uint8_t> hrp2_span(reinterpret_cast<const uint8_t*>(hrp2.data()), hrp2.size());

        size_t max_out_data_size = max_out_size - data1.size() - 8;

        auto out_hrp_vec = cross_over(
            hrp1_span,
            hrp2_span,
            max_out_data_size,
            static_cast<unsigned>(std::rand()));

        // Convert back to string and ensure null termination
        out_hrp = std::string(out_hrp_vec.begin(), out_hrp_vec.begin() + out_hrp_vec.size());
    } else {
        // Cross-over the data part
        out_hrp = hrp1;

        size_t max_out_data_size = max_out_size - hrp1.size() - 8;

        out_data = cross_over(
            data1,
            data2,
            max_out_data_size,
            static_cast<unsigned>(std::rand()));
    }

    // Encode the output
    std::string encoded = bech32::Encode(bech32::Encoding::BECH32, out_hrp, out_data);

    if (encoded.size() > max_out_size) {
        return 0;
    }

    // Copy the result to out buffer
    std::memcpy(out, encoded.data(), encoded.size());
    return encoded.size();
}
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
  const char* target = std::getenv("FUZZ");
  driver = std::make_shared<bitcoinfuzz::Driver>(module_logger);
#ifdef BITCOIN_CORE
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Bitcoin>());
#endif
#ifdef RUST_BITCOIN
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Rustbitcoin>());
#endif
#ifdef RUST_MINISCRIPT
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Rustminiscript>());
#endif
#ifdef BTCD
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Btcd>());
#endif
#ifdef NBITCOIN
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::NBitcoin>());
#endif
#ifdef LND
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Lnd>());
#endif
#ifdef LDK
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Ldk>());
#endif
#ifdef NLIGHTNING
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::NLightning>());
#endif
#ifdef EMBIT
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Embit>());
#endif
#ifdef CLIGHTNING
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::CLightning>());
#endif
#ifdef ECLAIR
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Eclair>());
#endif

#ifdef CUSTOM_MUTATOR_BOLT11
  module_logger.addCustomMutator("BOLT11 Bech32 Custom Mutator");
#endif

#ifdef CUSTOM_MUTATOR_BOLT12_OFFER
  module_logger.addCustomMutator("BOLT12 Offer/Invoice Custom Mutator");
#endif

#ifdef CUSTOM_MUTATOR_P2P_MESSAGE
  module_logger.addCustomMutator("Bitcoin P2P Message Custom Mutator");
#endif

  module_logger.logModules();
  driver->Run(Data, Size, target);
  return 0;
}
