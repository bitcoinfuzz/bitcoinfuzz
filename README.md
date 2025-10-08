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

You can build the modules in two ways: **manual** or **automatic**. The automatic method is provided by the `auto_build.sh` script, which simplifies the build and clean processes. Additionally, you can use **Docker** or **Docker Compose** to run the application without installing dependencies directly on your machine.

## Automatic Method: `auto_build.sh`

The `auto_build.sh` script allows you to automatically build the modules based on the flags defined in `CXXFLAGS`. It also provides options to clean the builds before compiling.

### How to use:

1. **Automatic Build**:
   - To automatically build the modules, define the flags in `CXXFLAGS` and run the script:
     ```bash
     CXXFLAGS="-DLDK -DLND" ./auto_build.sh
     ```
     This will automatically build the `LDK` and `LND` modules.

2. **Automatic Clean**:
   - The script supports three cleaning modes before building:
     - **Full Clean**: Cleans all modules before building the selected ones.
       ```bash
       CLEAN_BUILD="FULL" CXXFLAGS="-DLDK -DLND" ./auto_build.sh
       ```
     - **Clean**: Cleans only the modules that will be built based on `CXXFLAGS`.
       ```bash
       CLEAN_BUILD="CLEAN" CXXFLAGS="-DLDK -DLND" ./auto_build.sh
       ```
     - **Select Clean**: Cleans specific modules defined in `CLEAN_BUILD`, regardless of `CXXFLAGS`.
       ```bash
       CLEAN_BUILD="-DLDK -DBTCD" CXXFLAGS="-DLDK -DLND" ./auto_build.sh
       ```
       In this case, the script will run `make clean` for `LDK` and `BTCD`, but will only build the modules defined in `CXXFLAGS` (`LDK` and `LND`).

## Docker Method

If you prefer not to install dependencies directly on your machine, you can use Docker to run the application. This method simplifies deployment and ensures a consistent environment.

### Using the Dockerfile

1. **Build the Docker Image**:
   - Build the Docker image using the provided `docker`:
     ```bash
     docker build -t bitcoinfuzz .
     ```

2. **Run the Container**:
   - Run the container with the required `FUZZ` and `CXXFLAGS` environment variables:
     ```bash
     docker run -e FUZZ=target_name -e CXXFLAGS="-DLDK -DLND ..." bitcoinfuzz
     ```

3. **Optional Parameters**:
   - You can also pass optional parameters:
     - **`FUZZ_RUNS`**: Specify the number of fuzzing runs (e.g., 50):
       ```bash
       docker run -e FUZZ=target_name -e CXXFLAGS="-DLDK -DLND ..." -e FUZZ_RUNS=50 bitcoinfuzz
       ```
     - **`FUZZ_INPUT`**: Provide a specific corpus or crash file to test:
       ```bash
       docker run -e FUZZ=target_name -e CXXFLAGS="-DLDK -DLND ..." -e FUZZ_INPUT=/path/to/input bitcoinfuzz
       ```

4. **Accessing Generated Corpus**:
   - The generated corpus is saved in `/app/data` inside the container. To access it, you can mount a volume:
     ```bash
     docker run -e FUZZ=target_name -e CXXFLAGS="-DLDK -DLND ..." -v $(pwd)/corpus:/app/data bitcoinfuzz
     ```

### Using Docker Compose

The `docker-compose.yml` file simplifies running multiple fuzzing scenarios. Each scenario is preconfigured with the required modules and environment variables.

1. **Run All Scenarios**:
   - To run all scenarios, simply execute:
     ```bash
     docker-compose up
     ```

2. **Run a Specific Scenario**:
   - To run a specific scenario (e.g., `script`), use:
     ```bash
     docker-compose up script
     ```

3. **Optional Parameters**:
   - You can pass optional parameters like `FUZZ_RUNS` or `FUZZ_INPUT`:
     - Run with a specific number of fuzzing runs:
       ```bash
       FUZZ_RUNS=50 docker-compose up script
       ```
     - Run with a specific input file:
       ```bash
       FUZZ_INPUT=/path/to/input docker-compose up script
       ```

4. **Accessing Generated Corpus**:
   - The generated corpus for each scenario is saved in the `docker` directory at the same level as the `docker-compose.yml` file. Each scenario has its own subdirectory:
     ```
     docker/
     ├── script/
     ├── deserialize_block/
     ├── script_eval/
     ├── deserialize_offer/
     ├── descriptor_parse/
     ├── miniscript_parse/
     ├── script_asm/
     ├── deserialize_invoice/
     ├── address_parse/
     ├── addrv2/
     ├── psbt_parse/
     ├── parse_p2p_message/
     ├── parse_p2p_lightning_message/
     └── transaction_eval/
     ```
   - To ensure the corpus is saved locally, the `docker-compose.yml` file maps the `/app/data` directory inside the container to the corresponding subdirectory in `docker`.

## Manual Method

If you prefer, you can still build the modules manually. Below are the steps for each module:

### Bitcoin modules:

- ### rust-bitcoin

    ```bash
    cd modules/rustbitcoin
    make cargo && make
    export CXXFLAGS="$CXXFLAGS -DRUST_BITCOIN"
    ```

- ### rust-miniscript

    ```bash
    cd modules/rustminiscript
    make cargo && make
    export CXXFLAGS="$CXXFLAGS -DRUST_MINISCRIPT"
    ```

- ### btcd

    ```bash
    cd modules/btcd
    make
    export CXXFLAGS="$CXXFLAGS -DBTCD"
    ```

- ### NBitcoin

    ```bash
    cd modules/nbitcoin
    make
    export CXXFLAGS="$CXXFLAGS -DNBITCOIN"
    ```

- ### embit

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

- ### Bitcoin Core

    ```bash
    cd modules/bitcoin
    make
    export CXXFLAGS="$CXXFLAGS -DBITCOIN_CORE"
    ```

### Lightning modules:

- ### LDK

    ```bash
    cd modules/ldk
    make cargo && make
    export CXXFLAGS="$CXXFLAGS -DLDK"
    ```

- ### LND

    ```bash
    cd modules/lnd
    make
    export CXXFLAGS="$CXXFLAGS -DLND"
    ```

- ### NLightning

    ```bash
    cd modules/nlightning
    make
    export CXXFLAGS="$CXXFLAGS -DNLIGHTNING"
    ```

- ### C-lightning

    ```bash
    pip install mako
    git submodule update --init --recursive external/lightning
    cd modules/clightning
    make
    export CXXFLAGS="$CXXFLAGS -DCLIGHTNING"
    ```

- ### Eclair

    ```bash
    git submodule update --init --recursive external/eclair
    cd modules/eclair
    make
    export CXXFLAGS="$CXXFLAGS -DECLAIR"
    ```

- ### lightning-kmp

    ```bash
    cd modules/lightningkmp
    make
    export CXXFLAGS="$CXXFLAGS -DLIGHTNING_KMP"
    ```

## Final Build and Execution
Once the modules are compiled, you can compile `bitcoinfuzz` an execute it:
- ### Automatic Method:
    ```bash
    FUZZ=target_name ./bitcoinfuzz
    ```
- ### Manual Method:
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
