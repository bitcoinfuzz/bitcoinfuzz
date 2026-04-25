#include "../mutators/extended_key.h"
#include <cassert>
#include <iostream>
#include <vector>

// Stub for the internal fuzzer mutation engine
extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize) {
    // In a real fuzzer, this would flip bits. For our property test, 
    // we just return the size to verify the wrapper logic.
    return Size;
}

// Invariant A: Output size never exceeds max_size
bool test_max_size_invariant() {
    printf("[TEST] Max Size Invariant...\n");
    uint8_t data[200] = "xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqeseD2tzpwBcqVSv32oUy39C78p";
    size_t size = strlen((char*)data);
    size_t max_size = 150;
    unsigned int seed = 42;

    size_t new_size = LLVMFuzzerCustomMutator(data, size, max_size, seed);

    if (new_size > max_size) {
        printf("  [FAIL] Output size %zu exceeded max_size %zu\n", new_size, max_size);
        return false;
    }
    printf("  [PASS] Output size respects limits.\n");
    return true;
}

// Invariant B: Output decodes to exactly 78 bytes (BIP-32)
bool test_bip32_payload_length() {
    printf("[TEST] BIP-32 Payload Length...\n");
    uint8_t data[200] = "xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqeseD2tzpwBcqVSv32oUy39C78p";
    size_t size = strlen((char*)data);
    
    // Run mutator
    size_t new_size = LLVMFuzzerCustomMutator(data, size, 200, 123);
    
    std::string encoded((char*)data, new_size);
    std::vector<unsigned char> decoded;
    
    // We assume DecodeBase58Check is available via the header/linkage
    if (DecodeBase58Check(encoded, decoded, 200)) {
        if (decoded.size() != 78) { // BIP32_PAYLOAD_LEN is 78 
            printf("  [FAIL] Decoded payload size was %zu, expected 78\n", decoded.size());
            return false;
        }
    } else {
        printf("  [FAIL] Mutator produced invalid Base58Check string\n");
        return false;
    }
    printf("  [PASS] Output is a valid 78-byte BIP-32 key.\n");
    return true;
}

// Invariant C: Small/Empty inputs do not crash
bool test_edge_cases() {
    printf("[TEST] Edge Case Robustness...\n");
    uint8_t data[100] = {0};
    
    // Test 0-byte input
    LLVMFuzzerCustomMutator(data, 0, 100, 7);
    
    // Test 1-byte input
    data[0] = 'a';
    LLVMFuzzerCustomMutator(data, 1, 100, 8);
    
    printf("  [PASS] No crashes on empty or minimal input.\n");
    return true;
}

int main() {
    printf("Extended Key Custom Mutator Test\n\n");
    
    int passed = 0;
    int total = 0;

    total++; if (test_max_size_invariant()) passed++;
    total++; if (test_bip32_payload_length()) passed++;
    total++; if (test_edge_cases()) passed++;

    printf("\nTest Results: %d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}