# bitcoinfuzz

Differential Fuzzing of Bitcoin implementations and libraries.
Note this project is a WIP and might be not stable.

# Installation

## Dependencies

### llvm toolset (clang and libfuzzer)

* To support the flags used in some modules `-fsanitize=address,fuzzer -std=c++20` the minimum clang version required is 10.0

* For macOS the llvm tools are installed by default, just check that you have the minimum required version 10.0

    * If not installed or lesser than 10.0 just run:

        ```
        brew install llvm
        ```


* For ubuntu/debian it can be installed using the package manager:
    ```
    sudo apt install clang lld llvm-dev
    ```

* To install it from source check [clang_get_started](https://clang.llvm.org/get_started.html). You must build it with this cmake option: `-DLLVM_ENABLE_PROJECTS="clang;lld;compiler-rt"`


### boost

To build the bitcoin core module the boost library is required. Minimum version 

The module uses only libboost-filesystem and libboost-system modules. For ubuntu you can install with:

```
sudo apt install libboost-filesystem-dev libboost-system-dev
```

Or install the complete boost library with 
```
sudo apt install libboost-all-dev
```


## Bitcoin modules:

### rust-bitcoin

```bash
cd modules/rustbitcoin
make cargo && make
export CXXFLAGS="$CXXFLAGS -DRUST_BITCOIN"
```

### rust-miniscript

```bash
cd modules/rustminiscript
make cargo && make
export CXXFLAGS="$CXXFLAGS -DRUST_MINISCRIPT"
```

### mako

For the `script_eval` target, we recommend to get Mako from https://github.com/brunoerg/mako/tree/bitcoinfuzz since there are some checks that should be skipped for better fuzzing.

```bash
cd modules/mako
export MAKO_LIB_PATH="path/to/libmako.a"
make
```

### btcd

```bash
cd modules/btcd
make
export CXXFLAGS="$CXXFLAGS -DBTCD"
```

### embit

To run the fuzzer with `embit` module, you need to install the `embit` library.

To install the `embit` library, you can use the following command:
```bash
cd modules/embit
pip install -r embit_lib/requirements.txt
```

```bash
cd modules/embit
make
export CXXFLAGS="$CXXFLAGS -DEMBIT"
```

### Bitcoin Core

```bash
cd modules/bitcoin
make
export CXXFLAGS="$CXXFLAGS -DBITCOIN_CORE"
export BOOST_LIB_DIR="path/to/boost/"
```

## Lightning modules:

### LDK

```bash
cd modules/ldk
make cargo && make
export CXXFLAGS="$CXXFLAGS -DLDK"
```

### lnd

```bash
cd modules/lnd
make
export CXXFLAGS="$CXXFLAGS -DLND"
```

### NLightning

```bash
cd modules/nlightning
make
export CXXFLAGS="$CXXFLAGS -DNLIGHTNING"
```

Once the modules are compiled, you can compile bitcoinfuzz and execute it:
```bash
make
FUZZ=target_name ./bitcoinfuzz
```

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
- rust-bitcoin: CVE-2024-44073
- rust-miniscript: https://github.com/rust-bitcoin/rust-miniscript/issues/785
- rust-miniscript: https://github.com/rust-bitcoin/rust-miniscript/issues/788
