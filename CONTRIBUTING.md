# Contributing a New Module to bitcoinfuzz

This guide explains how to wire a new Bitcoin library into the bitcoinfuzz harness.
Every module follows the same six-file contract. Miss one and the build will fail or
the module will never be called.

## The Six-File Checklist

When adding a module named `<name>` with compile flag `-D<FLAG>`, you must touch
exactly these six locations:

| # | File | What to do |
|---|------|------------|
| 1 | `modules/<name>/module.h` | Declare your class inheriting `BaseModule`; list every target method you override |
| 2 | `modules/<name>/module.cpp` | Implement each target method by calling into your library via FFI |
| 3 | `modules/<name>/Makefile` | Build your library's static archive and output `module.a` |
| 4 | `Makefile` (root) | Add an `ifneq` block that appends `modules/<name>/module.a` to `MODULES` when `-D<FLAG>` is set |
| 5 | `auto_build.py` | Register any git submodule path, Rust-nightly requirement, or sequential-build constraint |
| 6 | `docker-compose.yml` | Add one service per fuzz target that should include your module |

Do not skip step 5 even if your module needs no special handling — verify the
existing `get_module_dir()` naming rule covers your flag (see below) and that no
entry is needed.

---

## Flag → Directory Naming Convention

The function `get_module_dir()` in [auto_build.py](auto_build.py) maps a compile
flag to a module directory. The rules, in priority order:

| Condition | Result | Example |
|-----------|--------|---------|
| Flag starts with `CUSTOM_MUTATOR_` | `custommutator/` | `CUSTOM_MUTATOR_BOLT11` → `custommutator/` |
| Flag is exactly `BITCOIN_CORE` | `modules/bitcoin/` | hardcoded special case |
| All other flags | `modules/<flag_lowercased_no_underscores>/` | `RUST_BITCOIN` → `modules/rustbitcoin/` |

The third rule is the default for every new module:

```
FLAG_NAME  →  modules/flagnamelowercase/
```

Examples:

| Flag | Directory |
|------|-----------|
| `RUST_BITCOIN` | `modules/rustbitcoin/` |
| `LIGHTNING_KMP` | `modules/lightningkmp/` |
| `DECRED_SECP256K1` | `modules/decredsecp256k1/` |
| `NBITCOIN_SECP256K1` | `modules/nbitcoinsecp256k1/` |
| `LIBWALLY_CORE` | `modules/libwallycore/` |
| `MY_NEW_LIB` | `modules/mynewlib/` |

> **Rule of thumb:** strip underscores, lowercase everything.

---

## Step-by-Step Walkthrough

### 1. `modules/<name>/module.h`

Inherit from `bitcoinfuzz::BaseModule` and override only the targets your library
implements. Each virtual method corresponds to one fuzz target in `driver.cpp`.

```cpp
#pragma once
#include <bitcoinfuzz/basemodule.h>
#include <optional>
#include <span>
#include <string>

namespace bitcoinfuzz {
namespace module {
class MyLib : public BaseModule {
public:
  MyLib();
  std::optional<std::string>
  script_parse(std::span<const uint8_t> buffer) const override;
  // add more overrides here as needed
  ~MyLib() noexcept override = default;
};
} // namespace module
} // namespace bitcoinfuzz
```

Available targets are declared in [include/bitcoinfuzz/basemodule.h](include/bitcoinfuzz/basemodule.h).

### 2. `modules/<name>/module.cpp`

Call your library and return a canonical string result (or `std::nullopt` on
invalid input). Return values are compared across modules to detect divergence.

```cpp
#include "module.h"
#include "mylib_ffi/mylib.h"  // your FFI header

namespace bitcoinfuzz {
namespace module {
MyLib::MyLib() : BaseModule("MyLib") {}

std::optional<std::string>
MyLib::script_parse(std::span<const uint8_t> buffer) const {
    auto result = mylib_parse_script(buffer.data(), buffer.size());
    if (!result) return std::nullopt;
    std::string s(result);
    mylib_free_string(result);
    return s;
}
} // namespace module
} // namespace bitcoinfuzz
```

### 3. `modules/<name>/Makefile`

Build your library and produce `module.a`. The root Makefile links this archive.

```makefile
all: module.a

# example for a Rust library built with cargo
mylib_ffi/libmylib.a:
	cargo build --release
	cp target/release/libmylib.a mylib_ffi/

module.a: mylib_ffi/libmylib.a module.cpp module.h
	$(CXX) $(CXXFLAGS) -c module.cpp -o module.o
	ar rcs module.a module.o mylib_ffi/libmylib.a

clean:
	rm -f module.o module.a

format:
	clang-format -i module.cpp module.h

check-format:
	clang-format -Werror --fail-on-incomplete-format -n module.cpp module.h

.PHONY: all clean format check-format
```

> The `format` and `check-format` targets are required — CI enforces them.

### 4. Root `Makefile` — flag block

Add an `ifneq` block alongside the existing ones:

```makefile
ifneq ($(findstring -DMY_NEW_LIB,$(BASE_CXXFLAGS) $(CXXFLAGS)),)
    MODULES += modules/mynewlib/module.a
endif
```

### 5. `auto_build.py` — optional entries

Only add entries when your module actually needs them:

```python
# If your module lives in a git submodule:
SUBMODULES_BY_FLAG = {
    ...
    "MY_NEW_LIB": ["external/my-new-lib"],
}

# If your Rust module requires nightly:
def needs_rust_nightly(flag: str) -> bool:
    return flag in {
        ...
        "MY_NEW_LIB",
    }

# If your module must build sequentially (e.g. JVM or heavy submodule):
def should_build_sequentially(flag: str) -> bool:
    return flag in {"SECP256K1", "BITCOINJ", "LIGHTNING_KMP", "MY_NEW_LIB"} or ...
```

If none of the above apply, no changes to `auto_build.py` are needed.

### 6. `docker-compose.yml` — service entry

Add one service for each fuzz target your module participates in. Reuse an
existing target service if your module can be added to it, or create a new one:

```yaml
  script_parse:           # existing service — just add -DMY_NEW_LIB to CXXFLAGS
    build:
      args:
        CXXFLAGS: "-DBITCOIN_CORE -DRUST_BITCOIN -DMY_NEW_LIB"
        FUZZ: script_parse
    ...
```

---

## FFI Mechanisms by Language

Different language runtimes require different integration patterns:

| Language | FFI mechanism | Example modules |
|----------|--------------|-----------------|
| C / C++ | Direct link (headers + `.a`) | `secp256k1`, `libwallycore`, `clightning`, `bitcoin`, `bitcoinkernel` |
| Rust | cbindgen → C header + static lib | `rustbitcoin`, `ldk`, `rustk256`, `rustreexo` |
| Rust (shared lib) | `.so`/`.dylib` via cbindgen | `tinyminiscript` |
| Go | CGo → shared lib | `btcd`, `gocoin`, `lnd`, `decredsecp256k1`, `utreexo` |
| Java / Kotlin / Scala (JVM) | JNI via `jvmloader` helper | `bitcoinj`, `eclair`, `lightningkmp` |
| C# (.NET) | P/Invoke → shared lib | `nbitcoin`, `nlightning`, `nbitcoinsecp256k1` |
| Python | Python C API (`Python.h`) | `embit`, `pybitcoinkernel`, `pycoin` |

See [docs/adding-a-module/](docs/adding-a-module/) for a language-specific guide
for each mechanism (coming soon).

---

## Pre-submission Checklist

Before opening a PR, confirm:

- [ ] Directory name matches `get_module_dir()` output for your flag
- [ ] `module.h` declares only the targets your library actually implements
- [ ] `module.cpp` returns `std::nullopt` for invalid/unsupported input (never throws)
- [ ] `modules/<name>/Makefile` has `format` and `check-format` targets
- [ ] Root `Makefile` flag block is present
- [ ] `docker-compose.yml` has at least one service using your flag
- [ ] `make check-format-all` passes
- [ ] `docker compose up <your_target> --build` succeeds end-to-end
