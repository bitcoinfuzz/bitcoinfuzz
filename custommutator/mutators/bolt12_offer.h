#ifndef BITCOINFUZZ_CUSTOMMUTATOR_BOLT12_OFFER_H
#define BITCOINFUZZ_CUSTOMMUTATOR_BOLT12_OFFER_H

#include <custommutator/utils/bech32.h>
#include <custommutator/mutators/customcrossover.h>
#include <string_view>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>

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

#endif // BITCOINFUZZ_CUSTOMMUTATOR_BOLT12_OFFER_H
