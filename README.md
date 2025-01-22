# bitcoinfuzz

Differential Fuzzing of Bitcoin implementations and libraries.
Note this project is a WIP and might be not stable.

# Installation

Change the `RUST_TARGET` according to your architecture.

### rust-bitcoin
```bash
export RUST_TARGET="aarch64-apple-darwin"
cd modules/rustbitcoin
cd rust_bitcoin_lib && cargo build --release --target=$RUST_TARGET
cd .. && make
export CXXFLAGS="$CXXFLAGS -DRUST_BITCOIN"
```

### rust-miniscript 

```bash
export RUST_TARGET="aarch64-apple-darwin"
cd modules/rustminiscript
cd rust_miniscript_lib && cargo build --release --target=$RUST_TARGET
cd .. && make
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

### Bitcoin Core

```bash
cd modules/bitcoin
make
export CXXFLAGS="$CXXFLAGS -DBITCOIN_CORE"
export BOOST_LIB_DIR="path/to/boost/"
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
