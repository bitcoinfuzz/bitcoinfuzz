cmake_minimum_required(VERSION 3.22)

# Set the compiler before project() is called
set(CMAKE_CXX_COMPILER "clang++")

project(bitcoinfuzz CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set global include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add basemodule
add_library(basemodule STATIC
    include/bitcoinfuzz/basemodule.h
    include/bitcoinfuzz/basemodule.cpp
)

# Add driver code
add_library(driver STATIC
    driver.h
    driver.cpp
)

# Platform-specific settings
if(APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -Wl,-ld_classic")
endif()

# Build modules options
option(WITH_BITCOIN_CORE "Build bitcoin-core module" OFF)
option(WITH_RUST_BITCOIN "Build rust-bitcoin module" OFF)
option(WITH_RUST_MINISCRIPT "Build rust-minscript module" OFF)
option(WITH_BTCD "Build btcd module" OFF)


# Process bitcoin-core module if enabled
if(WITH_BITCOIN_CORE)
    add_definitions(-DBITCOIN_CORE)
    add_subdirectory(modules/bitcoin)
endif()

# Process rust-bitcoin module if enabled
if(WITH_RUST_BITCOIN)
    add_definitions(-DRUST_BITCOIN)
    add_subdirectory(modules/rustbitcoin)
endif()

# Process rust-minscript module if enabled
if(WITH_RUST_MINISCRIPT)
    add_definitions(-DRUST_MINSCRIPT)
    add_subdirectory(modules/rustminiscript)
endif()

# Process btcd module if enabled
if(WITH_BTCD)
    add_definitions(-DBTCD)
    add_subdirectory(modules/btcd)
endif()

# Define the main executable
add_executable(bitcoinfuzz main.cpp)
target_link_libraries(bitcoinfuzz PRIVATE
    basemodule
    driver
)

# Link target libraries
if(WITH_BITCOIN_CORE)
    target_link_libraries(bitcoinfuzz PRIVATE bitcoin_core_module)
endif()

if(WITH_RUST_BITCOIN)
    target_link_libraries(bitcoinfuzz PRIVATE rust_bitcoin_module)
endif()

if(WITH_RUST_MINISCRIPT)
    target_link_libraries(bitcoinfuzz PRIVATE rust_miniscript_module)
endif()

if(WITH_BTCD)
    target_link_libraries(bitcoinfuzz PRIVATE btcd_module)
endif()