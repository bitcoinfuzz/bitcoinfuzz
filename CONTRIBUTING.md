# Contributing to bitcoinfuzz

Thank you for your interest in contributing to bitcoinfuzz! This guide covers the most common contribution: **adding a new Bitcoin/Lightning library module** for differential fuzzing.

## How bitcoinfuzz Works (30-second overview)

bitcoinfuzz feeds random input to multiple implementations of the same Bitcoin operation (e.g., script parsing) and asserts they all produce the same output. Each implementation is wrapped as a **module**. (a C++ class that calls the library via its FFI and returns a normalized result.)

When two modules disagree on the same input, a bug has been found.

## Adding a New Module

Every new module requires changes to **exactly six files** plus two build-system files. The table below is your checklist:

### Six-File Checklist

| # | File | What to Add | Purpose |
|---|------|-------------|---------|
| 1 | `modules/<dir>/module.h` | Class declaration inheriting `BaseModule` | Declares which fuzz targets your module supports |
| 2 | `modules/<dir>/module.cpp` | Implementation calling your library's FFI | Wraps your library and returns normalized results |
| 3 | `modules/<dir>/Makefile` | Build rules producing `module.a` | Compiles your module + library into a static archive |
| 4 | `include/bitcoinfuzz/module_defs.h` | `MODULE_ENTRY(FLAG, "NAME", ClassName)` | Single source of truth for flag → name → class mapping |
| 5 | `include/bitcoinfuzz/module_loader.h` | `#ifdef FLAG` / `#include` block | Conditionally includes your module header |
| 6 | Root `Makefile` | `ifneq` block adding `modules/<dir>/module.a` | Links your module archive into the final binary |

Plus two build-system updates:

| # | File | What to Add | Purpose |
|---|------|-------------|---------|
| 7 | `auto_build.py` | Update `get_module_dir()` (only if your flag doesn't follow the default convention), and optionally `needs_rust_nightly()` or `should_build_sequentially()` | Automatic build support |
| 8 | `docker-compose.yml` | A new service block for each fuzz target that includes your module | Docker-based fuzzing |

### Naming Conventions

The preprocessor flag you choose determines your directory name, registry name, and class name. The mapping rule from `auto_build.py`'s `get_module_dir()` is:

```py
directory = modules/{FLAG.lower().replace('_', '')}
```

**Special cases:** `BITCOIN_CORE` → `modules/bitcoin`, `CUSTOM_MUTATOR_*` → `custommutator`

Here are all existing modules as reference:

| Preprocessor Flag | Directory | Registry Name | Class Name | Language |
|---|---|---|---|---|
| `BITCOIN_CORE` | `modules/bitcoin` | `BITCOIN_CORE` | `Bitcoin` | C++ |
| `RUST_BITCOIN` | `modules/rustbitcoin` | `RUST_BITCOIN` | `Rustbitcoin` | Rust |
| `RUST_PSBT` | `modules/rustpsbt` | `RUST_PSBT` | `RustPsbt` | Rust |
| `RUST_MINISCRIPT` | `modules/rustminiscript` | `RUST_MINISCRIPT` | `Rustminiscript` | Rust |
| `TINY_MINISCRIPT` | `modules/tinyminiscript` | `TINY_MINISCRIPT` | `Tinyminiscript` | Rust |
| `BTCD` | `modules/btcd` | `BTCD` | `Btcd` | Go |
| `GOCOIN` | `modules/gocoin` | `GOCOIN` | `Gocoin` | Go |
| `NBITCOIN` | `modules/nbitcoin` | `NBITCOIN` | `NBitcoin` | C# (.NET) |
| `LND` | `modules/lnd` | `LND` | `Lnd` | Go |
| `LDK` | `modules/ldk` | `LDK` | `Ldk` | Rust |
| `ECLAIR` | `modules/eclair` | `ECLAIR` | `Eclair` | Java/Scala |
| `NLIGHTNING` | `modules/nlightning` | `NLIGHTNING` | `NLightning` | C# (.NET) |
| `PYBITCOINKERNEL` | `modules/pybitcoinkernel` | `PYBITCOINKERNEL` | `Pybitcoinkernel` | Python |
| `BITCOINKERNEL` | `modules/bitcoinkernel` | `BITCOINKERNEL` | `BitcoinKernel` | C |
| `BITCOINKERNEL_VARIANT` | `modules/bitcoinkernelvariant` | `BITCOINKERNEL_VARIANT` | `BitcoinKernelVariant` | C |
| `RUSTBITCOINKERNEL` | `modules/rustbitcoinkernel` | `RUSTBITCOINKERNEL` | `Rustbitcoinkernel` | Rust |
| `EMBIT` | `modules/embit` | `EMBIT` | `Embit` | Python |
| `CLIGHTNING` | `modules/clightning` | `CLIGHTNING` | `CLightning` | C |
| `LIGHTNING_KMP` | `modules/lightningkmp` | `LIGHTNING_KMP` | `LightningKmp` | Kotlin (JVM) |
| `BITCOINJ` | `modules/bitcoinj` | `BITCOINJ` | `BitcoinJ` | Java (JVM) |
| `DECRED_SECP256K1` | `modules/decredsecp256k1` | `DECRED_SECP256K1` | `Decred_secp256k1` | Go |
| `SECP256K1` | `modules/secp256k1` | `SECP256K1` | `Secp256k1` | C |
| `NBITCOIN_SECP256K1` | `modules/nbitcoinsecp256k1` | `NBITCOIN_SECP256K1` | `NBitcoin_secp256k1` | C# (.NET) |
| `RUST_K256` | `modules/rustk256` | `RUST_K256` | `K256` | Rust |
| `LIBWALLY_CORE` | `modules/libwallycore` | `LIBWALLY_CORE` | `LibwallyCore` | C |
| `RUSTREEXO` | `modules/rustreexo` | `RUSTREEXO` | `Rustreexo` | Rust |
| `UTREEXO` | `modules/utreexo` | `UTREEXO` | `Utreexo` | Go |
| `PYCOIN` | `modules/pycoin` | `PYCOIN` | `Pycoin` | Python |

## Step-by-Step Walkthrough

This example adds a hypothetical Rust library called `mybitcoinlib` that can parse Bitcoin scripts.

### 1. Choose your flag and names

Following the convention:
- **Flag:** `MY_BITCOIN_LIB`
- **Directory:** `modules/mybitcoinlib` (flag lowercased, underscores removed)
- **Registry name:** `MY_BITCOIN_LIB`
- **Class name:** `MyBitcoinLib`

### 2. Create `modules/mybitcoinlib/module.h`

```cpp
#pragma once

#include <bitcoinfuzz/basemodule.h>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace bitcoinfuzz {
namespace module {
class MyBitcoinLib : public BaseModule {
public:
  MyBitcoinLib(void);
  // Override only the targets your library supports.
  // Targets you don't override return std::nullopt by default
  // and are silently skipped during fuzzing.
  std::optional<std::string>
  script_parse(std::span<const uint8_t> buffer) const override;
  ~MyBitcoinLib() noexcept override = default;
};
} // namespace module
} // namespace bitcoinfuzz
```

### 3. Create `modules/mybitcoinlib/module.cpp`

```cpp
#include "module.h"
#include <cstring>

// FFI declarations from your library's C API or bindings
extern "C" {
  char* my_bitcoin_lib_script_parse(const uint8_t* data, size_t len);
  void my_bitcoin_lib_free_string(char* s);
}

namespace bitcoinfuzz {
namespace module {

MyBitcoinLib::MyBitcoinLib() : BaseModule("MyBitcoinLib") {}

std::optional<std::string>
MyBitcoinLib::script_parse(std::span<const uint8_t> buffer) const {
  char* result = my_bitcoin_lib_script_parse(buffer.data(), buffer.size());
  if (result == nullptr) {
    return std::nullopt;
  }
  std::string ret(result);
  my_bitcoin_lib_free_string(result);
  return ret;
}

} // namespace module
} // namespace bitcoinfuzz
```

**Key rule:** Return `std::nullopt` when your library rejects the input (parse failure, unsupported format). Return the **normalized result** when it succeeds. The driver asserts that all modules returning a value agree.

### 4. Create `modules/mybitcoinlib/Makefile`

The exact Makefile depends on your library's language. See existing modules for patterns:
- **Rust:** `modules/rustbitcoin/Makefile`: builds a Rust static lib, then `merge.sh` combines it with the C++ object
- **Go:** `modules/btcd/Makefile`: builds a cgo archive, uses `rename_cgo_symbols.sh`
- **Python:** `modules/embit/Makefile`: compiles the C++ wrapper only; Python is loaded at runtime
- **Java/Kotlin:** `modules/eclair/Makefile`: produces JARs; C++ calls via JNI through `helpers/jvmloader`
- **C# (.NET):** `modules/nbitcoin/Makefile`: builds a NativeAOT shared library
- **C/C++:** `modules/bitcoin/Makefile`: compiles directly

All Makefiles must produce a `module.a` static archive.

### 5. Add entry to `include/bitcoinfuzz/module_defs.h`

```cpp
#ifdef MY_BITCOIN_LIB
MODULE_ENTRY(MY_BITCOIN_LIB, "MY_BITCOIN_LIB", MyBitcoinLib)
#endif
```

### 6. Add include guard to `include/bitcoinfuzz/module_loader.h`

Add with the other `#ifdef` blocks (before the `namespace bitcoinfuzz {` block):

```cpp
#ifdef MY_BITCOIN_LIB
#include <modules/mybitcoinlib/module.h>
#endif
```

### 7. Add link block to root `Makefile`

Add with the other module blocks:

```makefile
ifneq ($(findstring -DMY_BITCOIN_LIB,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
	MODULES += modules/mybitcoinlib/module.a
endif
```

### 8. Update build-system files

**`auto_build.py`**: Usually nothing to change. The default `get_module_dir()` maps `MY_BITCOIN_LIB` → `modules/mybitcoinlib` automatically. Update only if:
- Your module needs Rust nightly → add to `needs_rust_nightly()`
- Your module must build sequentially → add to `should_build_sequentially()`
- Your module needs a git submodule → add to `SUBMODULES_BY_FLAG`

**`docker-compose.yml`**: Add a service for each fuzz target that includes your module. Example:

```yaml
  script:  # existing service (add your flag)
    build:
      args:
        CXXFLAGS: "-DBITCOIN_CORE -DRUST_BITCOIN -DMY_BITCOIN_LIB"  # add flag here
```

Or create a new service if your module introduces a new target combination.

### 9. Build and test

```bash
# Automatic build
CXXFLAGS="-DBITCOIN_CORE -DMY_BITCOIN_LIB" ./auto_build.py

# Run the fuzzer
FUZZ=script ./bitcoinfuzz -runs=1000

# Or with Docker
docker compose up script --build
```

## Available Fuzz Targets

Your module can support any of these targets by overriding the corresponding `BaseModule` method. You only need to implement the ones relevant to your library:

| Target (`FUZZ=`) | BaseModule Method |
|---|---|
| `script` | `script_parse()` |
| `deserialize_block` | `deserialize_block()` |
| `script_eval` | `script_eval()` |
| `verify_script` | `verify_script()` |
| `descriptor_parse` | `descriptor_parse()` |
| `miniscript_parse` | `miniscript_parse()` |
| `deserialize_invoice` | `deserialize_invoice()` |
| `address_parse` | `address_parse()` |
| `psbt_parse` | `psbt_parse()` |
| `addrv2` | `addrv2_parse()` |
| `deserialize_offer` | `deserialize_offer()` |
| `cmpctblocks_parse` | `cmpctblocks_parse()` |
| `parse_p2p_message` | `parse_p2p_message()` |
| `parse_p2p_lightning_message` | `parse_p2p_lightning_message()` |
| `transaction_eval` | `transaction_eval()` |
| `bip32_master_keygen` | `bip32_master_keygen()` |
| `kernel_block` | `kernel_block()` |
| `kernel_transaction` | `kernel_transaction()` |
| `private_to_public_key` | `private_to_public_key()` |
| `sign_compact` | `sign_compact()` |
| `sign_der` | `sign_der()` |
| `sign_verify` | `sign_verify()` |
| `ecdh` | `ecdh()` |
| `sign_schnorr` | `sign_schnorr()` |
| `bip32_deserialize_extended_key` | `bip32_deserialize_extended_key()` |
| `decode_ellswift` | `decode_ellswift()` |
| `schnorr_verify` | `schnorr_verify()` |
| `decode_onion` | `decode_onion()` |
| `stump_modify_add` | `stump_modify_add()` |

## Output Normalization

The critical contract: **all modules implementing the same target must return the same string for the same valid input.** This means:

- Serialize results to a canonical form (e.g., hex-encoded, sorted fields)
- Return `std::nullopt` (not an error string) for inputs your library rejects
- Look at how existing modules for the same target normalize their output and match that format exactly

## Language-Specific Notes

### Rust Modules
- Requires **Rust nightly** (for sanitizer support: `-Z sanitizer=address`)
- Write a Rust static library (`crate-type = ["staticlib"]`) with `extern "C"` functions
- Use `merge.sh` to combine the Rust `.a` with C++ object files
- Return allocated C strings via `CString::into_raw()`; expose a `free_c_string()` function
- Reference: `modules/rustbitcoin/`, `modules/ldk/`

### Go Modules
- Build with `CGO_ENABLED=1` and `-buildmode=c-archive`
- Use `//export` comments on Go functions for cgo
- Run `rename_cgo_symbols.sh` on the archive to avoid symbol conflicts with other Go modules
- Reference: `modules/btcd/`, `modules/lnd/`

### Python Modules
- Embed CPython via `#include <Python.h>` and `python3-config --ldflags --embed`
- Acquire the GIL with `PyGILState_Ensure()` before calling Python code
- Install Python dependencies in a `requirements.txt`
- Reference: `modules/embit/`, `modules/pycoin/`

### Java/Kotlin Modules (JVM)
- The JVM is loaded once via `helpers/jvmloader` (singleton)
- Place JAR files in `modules/<dir>/lib/`, they are auto-discovered
- Call methods via JNI (`JNIEnv*`, `FindClass`, `GetMethodID`, `CallStaticObjectMethod`)
- Add your module to `should_build_sequentially()` in `auto_build.py`
- Reference: `modules/eclair/`, `modules/bitcoinj/`, `modules/lightningkmp/`

### C# (.NET) Modules
- Build a NativeAOT shared library (`.so`/`.dylib`) with `dotnet publish`
- Expose `[UnmanagedCallersOnly]` entry points
- Reference: `modules/nbitcoin/`, `modules/nlightning/`

### C/C++ Modules
- Compile directly or link against a built external dependency (via git submodule in `external/`)
- If using a submodule, add it to `SUBMODULES_BY_FLAG` in `auto_build.py`
- Reference: `modules/bitcoin/` (Bitcoin Core), `modules/secp256k1/`, `modules/clightning/`

## Code Style

- C++20
- Format with `clang-format` (run `make format` in the root or in your module directory)
- Use `-Wall -Wextra` (already set in the root Makefile)

## Reporting Bugs Found by Fuzzing

If you find a mismatch (assertion failure), the fuzzer saves a crash artifact. Report bugs responsibly:

1. Check the upstream project's `SECURITY.md` for disclosure procedures
2. Include the crash input and which modules disagree
3. You can merge your corpus with the [public corpora](https://github.com/bitcoinfuzz/corpora)

## Questions?

Open an issue on the repository or reach out to the maintainers.
