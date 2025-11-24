#ifndef BITCOINFUZZ_CUSTOMMUTATOR_BOLT11_H
#define BITCOINFUZZ_CUSTOMMUTATOR_BOLT11_H

#include <custommutator/utils/bech32.h>
#include <custommutator/mutators/customcrossover.h>
#include <string_view>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <span>

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

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
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

extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t *in1, size_t in1_size, const uint8_t *in2,
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

#endif // BITCOINFUZZ_CUSTOMMUTATOR_BOLT11_H
