# LDK library path
set(LDK_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/ldk_lib/target/release/libldk_lib.a")

# Build rust-bitcoin lib
add_custom_target(cargo_build
    COMMAND ${CMAKE_COMMAND} -E chdir ldk_lib ${CMAKE_COMMAND} -E env RUSTFLAGS=-Z\ sanitizer=address cargo rustc --release -- -C passes=sancov-module -C llvm-args=-sanitizer-coverage-inline-8bit-counters -C llvm-args=-sanitizer-coverage-trace-compares -C llvm-args=-sanitizer-coverage-pc-table -C llvm-args=-sanitizer-coverage-level=4 -C llvm-args=-simplifycfg-branch-fold-threshold=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building LDK library with sanitizer coverage"
)

# Create ldk library file
add_custom_command(
    OUTPUT ${LDK_LIB_PATH}
    COMMAND ${CMAKE_COMMAND} -E chdir ldk_lib ${CMAKE_COMMAND} -E env RUSTFLAGS=-Z\ sanitizer=address cargo rustc --release -- -C passes=sancov-module -C llvm-args=-sanitizer-coverage-inline-8bit-counters -C llvm-args=-sanitizer-coverage-trace-compares -C llvm-args=-sanitizer-coverage-pc-table -C llvm-args=-sanitizer-coverage-level=4 -C llvm-args=-simplifycfg-branch-fold-threshold=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building LDK library with sanitizer coverage"
)

# Create the ldk_module library
add_library(ldk_module STATIC module.cpp module.h)

# Set target properties
target_include_directories(ldk_module PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(ldk_module PRIVATE -fPIC)

# Custom command to merge libraries after ldk_module is built
add_custom_command(
    TARGET ldk_module POST_BUILD
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../../merge.sh 
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}ldk_module${CMAKE_STATIC_LIBRARY_SUFFIX}
            ${LDK_LIB_PATH} 
            ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/ldk_module.dir/module.cpp.o
    COMMAND ranlib ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}ldk_module${CMAKE_STATIC_LIBRARY_SUFFIX}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${LDK_LIB_PATH}
    COMMENT "Merging LDK library with C++ module"
)

# Make sure Rust library is built before the module
add_dependencies(ldk_module cargo_build)
