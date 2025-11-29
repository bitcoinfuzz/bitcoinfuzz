#ifndef BITCOINFUZZ_CUSTOMMUTATOR_P2P_LIGHTNING_MESSAGE_H
#define BITCOINFUZZ_CUSTOMMUTATOR_P2P_LIGHTNING_MESSAGE_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <random>
#include <optional>
#include <algorithm>

// Custom mutator for Lightning P2P messages that creates lightning messages
// with a specified message type.
//
// This custom mutator does the following:
//   1. Mutate the input using libFuzzer's default mutator `LLVMFuzzerMutate`
//   2. Update the message type to the specified message type
//   3. Return the mutated data with the specified message type

static const std::unordered_map<std::string, uint16_t> message_types = {
        {"warning", 1},
        {"init", 16},
        {"error", 17},
        {"ping", 18},
        {"pong", 19},
        {"funding_created", 34},
        {"open_channel", 32},
        {"funding_signed", 35},
        {"channel_ready", 36},
        {"shutdown", 38},
        {"closing_signed", 39},
        {"closing_complete", 40},
        {"update_add_htlc", 128},
    };

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                               unsigned int seed) {
    // First, mutate the data using LibFuzzer's default mutator
    size_t new_size = LLVMFuzzerMutate(fuzz_data, size, max_size);

    const char* mt_env = std::getenv("P2P_LIGHTNING_MESSAGE_TYPE");
    std::string message_type_env = mt_env ? std::string(mt_env) : std::string();
    uint16_t message_type = 0;

    if (auto it = message_types.find(message_type_env); it != message_types.end()) {
        message_type = it->second;
    }

    if (message_type == 0) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<size_t> dist(0, message_types.size() - 1);
        auto it = message_types.begin();
        std::advance(it, dist(rng));
        message_type = it->second;
    }

    if (new_size < 2) {
        if (max_size < 2) {
            return new_size;
        }
        new_size = 2;
    }

    // Update the message type
    fuzz_data[0] = message_type >> 8;
    fuzz_data[1] = message_type & 0xFF;

    return new_size;
}

#endif // BITCOINFUZZ_CUSTOMMUTATOR_P2P_LIGHTNING_MESSAGE_H
