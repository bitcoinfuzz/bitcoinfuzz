# Rust Bitcoin Library Path
set(RUST_BITCOIN_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/rust_bitcoin_lib/target/release/librust_bitcoin_lib.a")

# Build rust-bitcoin lib
add_custom_target(cargo_build
    COMMAND ${CMAKE_COMMAND} -E chdir rust_bitcoin_lib ${CMAKE_COMMAND} -E env RUSTFLAGS=-Z\ sanitizer=address cargo rustc --release -- -C passes=sancov-module -C llvm-args=-sanitizer-coverage-inline-8bit-counters -C llvm-args=-sanitizer-coverage-trace-compares -C llvm-args=-sanitizer-coverage-pc-table -C llvm-args=-sanitizer-coverage-level=4 -C llvm-args=-simplifycfg-branch-fold-threshold=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building Rust library with sanitizer coverage"
)

# Create rust-bitcoin library file
add_custom_command(
    OUTPUT ${RUST_BITCOIN_LIB_PATH}
    COMMAND ${CMAKE_COMMAND} -E chdir rust_bitcoin_lib ${CMAKE_COMMAND} -E env RUSTFLAGS=-Z\ sanitizer=address cargo rustc --release -- -C passes=sancov-module -C llvm-args=-sanitizer-coverage-inline-8bit-counters -C llvm-args=-sanitizer-coverage-trace-compares -C llvm-args=-sanitizer-coverage-pc-table -C llvm-args=-sanitizer-coverage-level=4 -C llvm-args=-simplifycfg-branch-fold-threshold=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building Rust library with sanitizer coverage"
)

# Create the rust_bitcoin_module library
add_library(rust_bitcoin_module STATIC module.cpp module.h)

# Set target properties
target_include_directories(rust_bitcoin_module PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(rust_bitcoin_module PRIVATE -fPIC)

# Custom command to merge libraries after rust_bitcoin_module is built
add_custom_command(
    TARGET rust_bitcoin_module POST_BUILD
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../../merge.sh 
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}rust_bitcoin_module${CMAKE_STATIC_LIBRARY_SUFFIX}
            ${RUST_BITCOIN_LIB_PATH} 
            ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/rust_bitcoin_module.dir/module.cpp.o
    COMMAND ranlib ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}rust_bitcoin_module${CMAKE_STATIC_LIBRARY_SUFFIX}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${RUST_BITCOIN_LIB_PATH}
    COMMENT "Merging Rust miniscript library with C++ module"
)

# Make sure Rust library is built before the module
add_dependencies(rust_bitcoin_module cargo_build)