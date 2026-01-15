cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_KERNEL_LIB=ON \
  -DBUILD_UTIL_CHAINSTATE=ON \
  -DBUILD_DAEMON=OFF -DBUILD_CLI=OFF -DBUILD_GUI=OFF -DBUILD_TESTS=OFF

cmake --build build -j"$(nproc)" --target bitcoinkernel
cmake --build build -j"$(nproc)" --target bitcoin-chainstate



2025-01-06
GPT
#######################
Ordem sugerida (quase a sua, só mais “laser”)

Mapear o build do repo (CMake + targets + options)

Gerar build dir e listar targets/options

Build do(s) target(s): bitcoinkernel + bitcoin-chainstate

Run do bitcoin-chainstate --help e mais 1 execução simples

Ler o libbitcoinkernel.h com o contexto do que você compilou

Fechar o dia com um “resumo técnico” de 10 linhas (isso vira munição pro estudo do bitcoinfuzz amanhã)
#########################################
i want to understand the build process of btccore

why are there different cmkes

onde ficam as opções (BUILD_KERNEL_LIB, etc)

como essas opções ligam targets (lib + executáveis)

onde o build joga artefatos (biblioteca, binários, headers instaláveis) vao parar?
###############################################################################
TENHO QUE ENTENDER CMAKE MELHOR, NA HORA DE BUILDAR APENAS USEI OS COMANDOS SEM ENTENDER CERTIN

https://cmake.org/cmake/help/latest/

https://cmake.org/cmake/help/latest/guide/tutorial/Getting%20Started%20with%20CMake.html#

.

ok, tenho um entendimento basico melhor do que é o cmake e como ele funciona. Agora, o que eu quero é identificar as opções e targets do libitcoinkernel que eu quero.
Para isso, tenho que gerar o folder de build do cmake. Eu poderia apeans ler os arquivos cmake, mas gostaria de usar comandos cmake para praticar e me habituar.
Lembrei que para compilar btccore era imprescindível configurar o ccache dos exercícios do bdl, para isso, utilizei o ccache
rm -rf build
cmake -S . -B build -G "Unix Makefiles" \
  -DENABLE_IPC=OFF \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DBUILD_KERNEL_LIB=ON \
  -DBUILD_UTIL_CHAINSTATE=ON


preciso instalar capnproto!
CMake Error at src/ipc/libmultiprocess/CMakeLists.txt:18 (message):
  Cap'n Proto is required but was not found.

  To resolve, choose one of the following:

    - Install Cap'n Proto (version 1.0+ recommended)
    - For Bitcoin Core compilation build with -DENABLE_IPC=OFF to disable multiprocess support

 como não preciso para testar apenas o kernel, desligarei com flag
-DENABLE_IPC=OFF
buildou! nice

cmake build -LAH, estamos mostrando as variaveis avançadas com A apenas para fins educacionais.
// Build experimental bitcoin-chainstate executable.
BUILD_UTIL_CHAINSTATE:BOOL=OFF
// Build experimental bitcoinkernel library.
BUILD_KERNEL_LIB:BOOL=OFF
#################################
run auto build
CC=clang CXX=clang++ CXXFLAGS="-DSECP256K1" ./auto_build.py

can I add the submodule initiation here?

def build_module(flag: str, quiet: bool):
    dirpath = get_module_dir(flag)

    if not quiet:
        print(f"Building module: {flag}")

    if needs_rust_nightly(flag):
        execute_in_dir(
            dirpath,
            "rustup default nightly && make cargo && make",
            quiet,
        )
    else:
        execute_in_dir(dirpath, "make", quiet)

    if quiet:
        print(f"✓ {flag} built successfully")



###########ISSUE
[`auto_build.py`](https://github.com/bitcoinfuzz/bitcoinfuzz/blob/v2/auto_build.py) has a few minor inconsistencies to be adjusted:
- When sequential build fails, it can still print `"All module builds completed successfully!"`. When `sys.exit(1)`is called on the `die()`function, it only terminates the thread that calls it, but the main thread keeps running.
  ```
  def die(msg: str):
      print(f"Error: {msg}", file=sys.stderr)
      sys.exit(1)
  ```
  Which means that, the main thread eventually prints success, even after a failure.
  ```
  # Sequential group (runs in background)
  if sequential:

      def run_sequential():
          print(f"Starting sequential module builds:{' '.join(sequential)}")
          for f in sequential:
              build_module(f, quiet) <-- build fails, die() is called and sequential thread dies quietly

      import threading

      seq_thread = threading.Thread(target=run_sequential)
      seq_thread.start()

  # Parallel builds
  [...]

  if sequential:
      seq_thread.join()

  print("All module builds completed successfully!") <-- main thread still prints success
  ```
  Example building secp256k1:
  ```
  CC=clang CXX=clang++ CXXFLAGS="-DSECP256K1" ./auto_build.py
  Cleaning previous builds...
  make clean
  rm -rf *.o module.a bitcoinfuzz include/bitcoinfuzz/*.o helpers/*.o modules/secp256k1/module.a
  rm -rf modules/eclair/eclair.zip modules/eclair/lib modules/eclair/eclair_extracted
  No CLEAN_BUILD option specified. Skipping clean step.
  Compiling selected modules in parallel (jobs=0) with CXXFLAGS=-DSECP256K1...
  Starting sequential module builds:SECP256K1
  cd ../../external/secp256k1 && \
  ./autogen.sh && \
  ./configure \
          --enable-module-recovery \
          --enable-experimental \
          --enable-module-schnorrsig \
          --disable-shared \
          --with-pic \
          CFLAGS="-fsanitize=address,fuzzer-no-link" && \
  make

  /bin/sh: line 2: ./autogen.sh: No such file or directory
  make: *** [Makefile:15: ../../external/secp256k1/.libs/libsecp256k1.a] Error 127

  Error: Command failed: make                            <-- build unsuccessful
  All module builds completed successfully!      <-- success build message
  Compiling the main project in the root...
  make
  [...]
  ```
 - Automatic script assumes that necessary submodules are already initialized, which may cause build to fail in a freshly cloned repo:
   ```
  CC=clang CXX=clang++ CXXFLAGS="-DSECP256K1" ./auto_build.py
  Cleaning previous builds...
  make clean
  rm -rf *.o module.a bitcoinfuzz include/bitcoinfuzz/*.o helpers/*.o modules/secp256k1/module.a
  rm -rf modules/eclair/eclair.zip modules/eclair/lib modules/eclair/eclair_extracted
  No CLEAN_BUILD option specified. Skipping clean step.
  Compiling selected modules in parallel (jobs=0) with CXXFLAGS=-DSECP256K1...
  Starting sequential module builds:SECP256K1
  cd ../../external/secp256k1 && \
  ./autogen.sh && \
  ./configure \
          --enable-module-recovery \
          --enable-experimental \
          --enable-module-schnorrsig \
          --disable-shared \
          --with-pic \
          CFLAGS="-fsanitize=address,fuzzer-no-link" && \
  make

  /bin/sh: line 2: ./autogen.sh: No such file or directory <-- Missing autogen.sh due to unitialized submodule
  make: *** [Makefile:15: ../../external/secp256k1/.libs/libsecp256k1.a] Error 127

  Error: Command failed: make                            <-- build unsuccessful
  All module builds completed successfully!      <-- success build message
  Compiling the main project in the root...
  make
  [...]
  ```


git submodule status -- external/secp256k1


git submodule deinit -f -- external/secp256k1


rm -rf external/secp256k1


test -e external/secp256k1 && echo "still exists" || echo "removed"
#####################################################################################################
Linking bitcoinfuzz with libbitcoinkernel

# building bitcoinfuzz
CC=clang CXX=clang++ CCXFLAGS="-DDYNAMICBITCOINKERNEL" ./autobuild.py

# running bitcoinfuzz
FUZZ=kernel_block ./bitcoinfuzz -runs =5
# compiling lib
cmake -S . -B build -G "Unix Makefiles" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DENABLE_IPC=OFF \
  -DBUILD_KERNEL_LIB=ON \

 
 ###########################
# building kernel lib
cmake -S . -B build-kernel-2 -G "Unix Makefiles" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DENABLE_IPC=OFF \
  -DBUILD_KERNEL_LIB=ON \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_BITCOIN_BIN=OFF \
  -DBUILD_DAEMON=OFF \
  -DBUILD_CLI=OFF \
  -DBUILD_TESTS=OFF \
  -DBUILD_TX=OFF \
  -DBUILD_UTIL=OFF \
  -DBUILD_KERNEL_TEST=OFF \
  -DENABLE_WALLET=OFF \
  -DBUILD_WALLET_TOOL=OFF \
  -DENABLE_EXTERNAL_SIGNER=OFF
# compiling kernel lib
cmake --build buil-kernel-1 -j"$(nproc)" --target bitcoinkernel