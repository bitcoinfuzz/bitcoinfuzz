# bitcoinfuzz

Differential Fuzzing of Bitcoin implementations and libraries.
Note this project is a WIP and might be not stable.

# Installation

## Platform Support

*bitcoinfuzz is developed and supported on Linux only.*

### MacOS or Windows Users:

We recommend using Docker to run bitcoinfuzz if you're on macOS or Windows. This provides the most reliable fuzzing experience with full toolchain support.

## Dependencies

### llvm toolset (clang and libfuzzer)

* To support the flags used in some modules `-fsanitize=address,fuzzer -std=c++20` the minimum clang version required is 10.0

* For ubuntu/debian it can be installed using the package manager:
    ```
    sudo apt install clang lld llvm-dev
    ```

* To install it from source check [clang_get_started](https://clang.llvm.org/get_started.html). You must build it with this cmake option: `-DLLVM_ENABLE_PROJECTS="clang;lld;compiler-rt"`

# Running

The fuzzer has a lot of dependencies from the number of projects (and their runtime/lang) it supports and for that reason we provide a docker workflow to ease the burden of setting up and building the project.

See [RUNNING.md](./RUNNING.md) to understand better the options and their configurations.

If you ended up luckily finding more bugs report them responsibly (see the respective SECURITY.md file on the project repo). You can merge your fuzzing corpus with our [public corpora](https://github.com/bitcoinfuzz/corpora).


# Fuzzing with AFL++

To quickly get started fuzzing using [afl++](https://github.com/AFLplusplus/AFLplusplus):

```sh
$ git clone https://github.com/AFLplusplus/AFLplusplus
$ make -C AFLplusplus/ source-only
# To choose/use any other afl compiler, see
# https://github.com/AFLplusplus/AFLplusplus/blob/stable/docs/fuzzing_in_depth.md#a-selecting-the-best-afl-compiler-for-instrumenting-the-target
$ CC="AFLplusplus/afl-clang-fast" CXX="AFLplusplus/afl-clang-fast++" make
$ mkdir -p inputs/ outputs/
$ echo A > inputs/thin-air-input
$ FUZZ=addrv2 ./AFLplusplus/afl-fuzz -i inputs/ -o outputs/ -- ./bitcoinfuzz
```

Read the [afl++ documentation](https://github.com/AFLplusplus/AFLplusplus) for more information.

# Build Options

You can build the modules in two ways: **manual** or **automatic**. The automatic method is provided by the `auto_build.py` script, which simplifies the build and clean processes. Additionally, you can use **Docker** or **Docker Compose** to run the application without installing dependencies directly on your machine.

## Automatic Method: `auto_build.py`

The `auto_build.py` script allows you to automatically build the modules based on the flags defined in `CXXFLAGS`. It also provides options to clean the builds before compiling.

### How to use:

1. **Automatic Build**:
   - To automatically build the modules, define the flags in `CXXFLAGS` and run the script:
     ```bash
     CXXFLAGS="-DLDK -DLND" ./auto_build.py
     ```
     This will automatically build the `LDK` and `LND` modules.

2. **Automatic Clean**:
   - The script supports three cleaning modes before building:
     - **Full Clean**: Cleans all modules before building the selected ones.
       ```bash
       CLEAN_BUILD="FULL" CXXFLAGS="-DLDK -DLND" ./auto_build.py
       ```
     - **Clean**: Cleans only the modules that will be built based on `CXXFLAGS`.
       ```bash
       CLEAN_BUILD="CLEAN" CXXFLAGS="-DLDK -DLND" ./auto_build.py
       ```
     - **Select Clean**: Cleans specific modules defined in `CLEAN_BUILD`, regardless of `CXXFLAGS`.
       ```bash
       CLEAN_BUILD="-DLDK -DBTCD" CXXFLAGS="-DLDK -DLND" ./auto_build.py
       ```
       In this case, the script will run `make clean` for `LDK` and `BTCD`, but will only build the modules defined in `CXXFLAGS` (`LDK` and `LND`).

## Docker

See [RUNNING.md](./RUNNING.md) for more information.

## Manual Method

If you prefer, you can still build the modules manually. Below are the steps for each module:

### Bitcoin modules:

- ### [rust-bitcoin](https://github.com/rust-bitcoin/rust-bitcoin)

    ```bash
    cd modules/rustbitcoin
    make
    export CXXFLAGS="$CXXFLAGS -DRUST_BITCOIN"
    ```

- ### [tinyminiscript](https://github.com/unldenis/tinyminiscript)

    ```bash
    cd modules/tinyminiscript
    make
    export CXXFLAGS="$CXXFLAGS -DTINY_MINISCRIPT"
    ```

- ### [rust-miniscript](https://github.com/rust-bitcoin/rust-miniscript)

    ```bash
    cd modules/rustminiscript
    make
    export CXXFLAGS="$CXXFLAGS -DRUST_MINISCRIPT"
    ```

- ### [btcd](https://github.com/btcsuite/btcd)

    ```bash
    cd modules/btcd
    make
    export CXXFLAGS="$CXXFLAGS -DBTCD"
    ```

- ### [gocoin](https://github.com/piotrnar/gocoin)

    [gocoin](https://github.com/piotrnar/gocoin) is a full Bitcoin node implementation written in Go.

    ```bash
    cd modules/gocoin
    make
    export CXXFLAGS="$CXXFLAGS -DGOCOIN"
    ```

    **Note:** gocoin cannot be used together with btcd in the same build due to cgo symbol conflicts (both embed the Go runtime).

- ### [NBitcoin](https://github.com/MetacoSA/NBitcoin)

    ```bash
    cd modules/nbitcoin
    make
    export CXXFLAGS="$CXXFLAGS -DNBITCOIN"
    ```

- ### [embit](https://github.com/diybitcoinhardware/embit)

    To run the fuzzer with `embit` module, you need to install the `embit` library.

    To install the `embit` library, you can use the following command:
    ```bash
    cd modules/embit
    pip install -r ./requirements.txt
    ```

    ```bash
    cd modules/embit
    make
    export CXXFLAGS="$CXXFLAGS -DEMBIT"
    ```

- ### [rust-bitcoinkernel](https://github.com/sedited/rust-bitcoinkernel)

    ```bash
    cd modules/rustbitcoinkernel
    make
    export CXXFLAGS="$CXXFLAGS -DRUSTBITCOINKERNEL"
    ```

- ### [py-bitcoinkernel](https://github.com/stickies-v/py-bitcoinkernel)

    To run the fuzzer with `py-bitcoinkernel` module, you need to install the `py-bitcoinkernel` library.

    To install the `py-bitcoinkernel` library, you can use the following command:
    ```bash
    cd modules/pybitcoinkernel
    pip install -r ./requirements.txt
    ```

    ```bash
    cd modules/pybitcoinkernel
    make
    export CXXFLAGS="$CXXFLAGS -DPYBITCOINKERNEL"
    ```

- ### [bitcoinkernel](https://github.com/bitcoin/bitcoin/blob/master/src/kernel/bitcoinkernel.cpp) (Bitcoin Core's [`libbitcoinkernel`](https://github.com/bitcoin/bitcoin/issues/27587) project)

    Use this module when you want to run kernel fuzz targets against **a single** `libbitcoinkernel` version.

    - **Default version**: `master`
    - **Pin a version**: set `BITCOINKERNEL_REF` (tag/branch/commit)

    ```bash
    # build manual (default: master)
    cd modules/bitcoinkernel
    export BITCOINKERNEL_REF=[ref]
    make
    export CXXFLAGS="$CXXFLAGS -DBITCOINKERNEL"
- ### bitcoinkernel-variant (differential fuzzing for `libbitcoinkernel`)

    For differential fuzzing between two bitcoinkernel versions, build **two** modules:
    - `BITCOINKERNEL` (base; `BITCOINKERNEL_REF`, default `master`)
    - `BITCOINKERNEL_VARIANT` (comparison; `BITCOINKERNEL_VARIANT_REF`, default `master`)

    ```bash
    # 1) base (creates the reference repo at modules/bitcoinkernel/bitcoin)
    cd modules/bitcoinkernel
    export BITCOINKERNEL_REF=[base_ref]
    make
    export CXXFLAGS="$CXXFLAGS -DBITCOINKERNEL"

    # 2) variant (ref repo is required for build)
    cd ../bitcoinkernelvariant
    BITCOINKERNEL_VARIANT_REF=<variant_ref>
    make
    export CXXFLAGS="$CXXFLAGS -DBITCOINKERNEL_VARIANT"
    ```

- ### [Bitcoin Core](https://github.com/bitcoin/bitcoin)

    ```bash
    git submodule update --init external/bitcoin-core
    cd modules/bitcoin
    make
    export CXXFLAGS="$CXXFLAGS -DBITCOIN_CORE"
    ```

    To update to a newer Bitcoin Core version:
    ```bash
    git submodule update --remote external/bitcoin-core
    cd modules/bitcoin && make clean && make
    ```

- ### [bitcoinj](https://github.com/bitcoinj/bitcoinj)

    ```bash
    cd modules/bitcoinj
    make
    export CXXFLAGS="$CXXFLAGS -DBITCOINJ"
    ```

- ### [libwally-core](https://github.com/ElementsProject/libwally-core)

    ```bash
    git submodule update --init --recursive external/libwally-core
    cd modules/libwallycore
    make
    export CXXFLAGS="$CXXFLAGS -DLIBWALLY_CORE"
    ```

- ### [pycoin](https://github.com/richardkiss/pycoin)

    To run the fuzzer with `pycoin` module, you need to install the `pycoin` library.

    To install the `pycoin` library, you can use the following command:
    ```bash
    cd modules/pycoin
    pip install -r ./requirements.txt
    ```

    ```bash
    cd modules/pycoin
    make
    export CXXFLAGS="$CXXFLAGS -DPYCOIN"
    ```

### Lightning modules:

- ### [LDK](https://github.com/lightningdevkit/rust-lightning)

    ```bash
    cd modules/ldk
    make
    export CXXFLAGS="$CXXFLAGS -DLDK"
    ```

- ### [LND](https://github.com/lightningnetwork/lnd)

    ```bash
    cd modules/lnd
    make
    export CXXFLAGS="$CXXFLAGS -DLND"
    ```

- ### [NLightning](https://github.com/ipms-io/nlightning)

    ```bash
    cd modules/nlightning
    make
    export CXXFLAGS="$CXXFLAGS -DNLIGHTNING"
    ```

- ### [C-lightning](https://github.com/ElementsProject/lightning)

    ```bash
    pip install mako
    git submodule update --init --recursive external/lightning
    cd modules/clightning
    make
    export CXXFLAGS="$CXXFLAGS -DCLIGHTNING"
    ```

- ### [Eclair](https://github.com/ACINQ/eclair)

    ```bash
    git submodule update --init --recursive external/eclair
    cd modules/eclair
    make
    export CXXFLAGS="$CXXFLAGS -DECLAIR"
    ```

    **Note:** When fuzzing with Eclair, the JVM's JIT compiler allocates memory that LSan cannot track through the standard allocator, causing false-positive leak reports. Use the provided suppressions file to silence them:

    ```bash
    LSAN_OPTIONS="suppressions=$(pwd)/lsan.supp" FUZZ=deserialize_invoice ./bitcoinfuzz
    LSAN_OPTIONS="suppressions=$(pwd)/lsan.supp" FUZZ=deserialize_offer ./bitcoinfuzz
    ```
    
    This suppressions file was only validated on Ubuntu and might not work depending on your setup. You can write your own file or disable the leak detection globally.

- ### [lightning-kmp](https://github.com/ACINQ/lightning-kmp)

    ```bash
    cd modules/lightningkmp
    make
    export CXXFLAGS="$CXXFLAGS -DLIGHTNING_KMP"
    ```

### Secp256k1 modules:

- ### [Decred-Secp256k1](https://github.com/decred/dcrd/tree/master/dcrec/secp256k1)

    ```bash
    cd modules/decredsecp256k1
    make
    export CXXFLAGS="$CXXFLAGS -DDECRED_SECP256K1"
    ```

- ### [Libsecp256k1](https://github.com/bitcoin-core/secp256k1)

    ```bash
    git submodule update --init --recursive external/secp256k1
    cd modules/secp256k1
    make
    export CXXFLAGS="$CXXFLAGS -DSECP256K1"
    ```

- ### [NBitcoin-Secp256k1](https://github.com/MetacoSA/NBitcoin/tree/master/NBitcoin.Secp256k1)

    ```bash
    cd modules/nbitcoinsecp256k1
    make
    export CXXFLAGS="$CXXFLAGS -DNBITCOIN_SECP256K1"
    ```

- ### [K256](https://github.com/RustCrypto/elliptic-curves/tree/master/k256)

    ```bash
    cd modules/rustk256
    make
    export CXXFLAGS="$CXXFLAGS -DRUST_K256"
    ```

### Utreexo Modules

- ### [Utreexo](https://github.com/utreexo/utreexo)

    ```bash
    cd modules/utreexo
    make
    export CXXFLAGS="$CXXFLAGS -DUTREEXO"
    ```

- ### [Rustreexo](https://github.com/mit-dci/rustreexo)

    ```bash
    cd modules/rustreexo
    make
    export CXXFLAGS="$CXXFLAGS -DRUSTREEXO"
    ```

## Final Build and Execution
Once the modules are compiled, you can compile `bitcoinfuzz` and execute it:

```bash
make
FUZZ=target_name ./bitcoinfuzz
```

## Selective Module Loading

By default, all compiled modules are loaded when running the fuzzer. You can use the `MODULES` environment variable to load only specific modules at runtime, **without needing to recompile**.

### Usage

Set the `MODULES` environment variable to a comma-separated list of module names:

```bash
MODULES="BITCOIN_CORE,RUST_BITCOIN" FUZZ=target_name ./bitcoinfuzz
```

### Examples

Load only Bitcoin Core and rust-bitcoin for comparison:
```bash
MODULES="BITCOIN_CORE,RUST_BITCOIN" FUZZ=bip32_master_keygen ./bitcoinfuzz
```

Load Lightning implementations only:
```bash
MODULES="LDK,LND,CLIGHTNING" FUZZ=deserialize_invoice ./bitcoinfuzz
```

Load all compiled modules (default behavior):
```bash
FUZZ=target_name ./bitcoinfuzz
```

### Notes

- If `MODULES` is unset or empty, all compiled modules are loaded (existing behavior)
- The fuzzer will abort with an error if you request a module that was not compiled
- Whitespace around module names is trimmed (e.g., `"BTCD, LND"` works)

-------------------------------------------
### Bugs/inconsistences/mismatches found by Bitcoinfuzz

- sipa/miniscript: https://github.com/sipa/miniscript/issues/140
- rust-miniscript: https://github.com/rust-bitcoin/rust-miniscript/issues/633
- rust-bitcoin: https://github.com/rust-bitcoin/rust-bitcoin/issues/2681
- btcd: https://github.com/btcsuite/btcd/issues/2195 (API mismatch with Bitcoin Core)
- Bitcoin Core: https://github.com/brunoerg/bitcoinfuzz/issues/34
- rust-miniscript: https://github.com/rust-bitcoin/rust-miniscript/issues/696 (not found but reproductive)
- rust-miniscript: https://github.com/brunoerg/bitcoinfuzz/issues/39
- rust-bitcoin: https://github.com/rust-bitcoin/rust-bitcoin/issues/2891
- rust-bitcoin: https://github.com/rust-bitcoin/rust-bitcoin/issues/2879
- btcd: https://github.com/btcsuite/btcd/issues/2199
- rust-bitcoin: https://github.com/brunoerg/bitcoinfuzz/issues/57
- rust-miniscript: CVE-2024-44073
- rust-miniscript: https://github.com/rust-bitcoin/rust-miniscript/issues/785
- rust-miniscript: https://github.com/rust-bitcoin/rust-miniscript/issues/788
- LND: https://github.com/lightningnetwork/lnd/issues/9591
- Embit: https://github.com/diybitcoinhardware/embit/issues/70
- btcd: https://github.com/btcsuite/btcd/issues/2351
- Core Lightning: https://github.com/ElementsProject/lightning/pull/8219
- LND: https://github.com/lightningnetwork/lnd/issues/9808
- Core Lightning:  https://github.com/ElementsProject/lightning/pull/8282
- btcd: https://github.com/btcsuite/btcd/issues/2372
- bolts: https://github.com/lightning/bolts/pull/1264
- rust-lightning: https://github.com/lightningdevkit/rust-lightning/pull/3814
- LND: https://github.com/lightningnetwork/lnd/issues/9904
- LND: https://github.com/lightningnetwork/lnd/issues/9915
- Eclair: https://github.com/ACINQ/eclair/issues/3104
- rust-bitcoin: https://github.com/rust-bitcoin/rust-bitcoin/issues/4617
- NBitcoin: https://github.com/MetacoSA/NBitcoin/issues/1278
- lightning-kmp: https://github.com/ACINQ/lightning-kmp/issues/799
- lightning-kmp: https://github.com/ACINQ/lightning-kmp/pull/801
- bitcoin-kmp: https://github.com/ACINQ/bitcoin-kmp/issues/157
- secp256k1: https://github.com/bitcoin-core/secp256k1/issues/1718
- lightning-kmp: https://github.com/ACINQ/lightning-kmp/issues/802
- rust-lightning: https://github.com/lightningdevkit/rust-lightning/pull/3998
- bolts: https://github.com/lightning/bolts/pull/1279
- rust-lightning: https://github.com/lightningdevkit/rust-lightning/pull/4018
- btcd: https://github.com/btcsuite/btcd/issues/2402
- btcd: https://github.com/btcsuite/btcd/issues/2424
- rust-lightning: https://github.com/lightningdevkit/rust-lightning/pull/4090
- btcd: https://github.com/btcsuite/btcd/issues/2431
- LND: https://github.com/lightningnetwork/lnd/pull/10249
- NBitcoin: https://github.com/MetacoSA/NBitcoin/issues/1283
- tinyminiscript: https://github.com/unldenis/tinyminiscript/issues/54
- NBitcoin: https://github.com/MetacoSA/NBitcoin/pull/1288
- bolts: https://github.com/lightning/bolts/pull/1303
- lightning-onion: https://github.com/lightningnetwork/lightning-onion/pull/74
- NBitcoin: https://github.com/MetacoSA/NBitcoin/pull/1294
- Floresta: https://github.com/bitcoinfuzz/bitcoinfuzz/issues/383 / https://github.com/getfloresta/Floresta/pull/781
- gocoin: https://github.com/piotrnar/gocoin/commit/42763e1efb5f09ab563aa95a288b1dbe92b90cce
- bitcoinj: https://github.com/bitcoinj/bitcoinj/issues/4054
- libwally-core: https://github.com/ElementsProject/libwally-core/commit/a3fd0aa8ba78d37819ea3b03d22c77028e958cf5
- libwally-core: https://github.com/ElementsProject/libwally-core/commit/a1de7372913092cc664ceb98e7b96dd57fec44e7
- NBitcoin: https://github.com/MetacoSA/NBitcoin/issues/1297
- btcd: https://github.com/btcsuite/btcd/pull/2485
- rust-bitcoin: https://github.com/rust-bitcoin/rust-bitcoin/issues/5617
- rust-bitcoin: https://github.com/rust-bitcoin/rust-bitcoin/issues/5697
- LND: https://github.com/lightningnetwork/lnd/pull/10597
- gocoin: https://github.com/piotrnar/gocoin/commit/3234dfcf5433718a5aa521db618e40f7c89c3690
- rust-lightning: https://github.com/lightningdevkit/rust-lightning/issues/4442
- rust-bitcoin: https://github.com/rust-bitcoin/rust-bitcoin/issues/5730
- utreexo: https://github.com/utreexo/utreexo/issues/190
- libwally-core: https://github.com/ElementsProject/libwally-core/pull/521
- pycoin: https://github.com/richardkiss/pycoin/issues/437
- embit: https://github.com/diybitcoinhardware/embit/issues/113
