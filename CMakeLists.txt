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
option(WITH_LDK "Build ldk module" OFF)
option(WITH_ECLAIR "Build eclair module" OFF)
option(WITH_EMBIT "Build embit module" OFF)
option(WITH_NBITCOIN "Build nbitcoin module" OFF)
option(WITH_NLIGHTNING "Build nlightning module" OFF)
option(WITH_CLIGHTNING "Build clightning module" OFF)
option(WITH_LIGHTNING_KMP "Build lightning-kmp module" OFF)
option(WITH_LND "Build lnd module" OFF)


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

# Process lnd module if enabled
if(WITH_LND)
    add_definitions(-DLND)
    add_subdirectory(modules/lnd)
endif()

if(WITH_LDK)
    add_definitions(-DLDK)
    add_subdirectory(modules/ldk)
endif()

if(WITH_NBITCOIN)
    add_definitions(-DNBITCOIN)
    add_subdirectory(modules/nbitcoin)
endif()

if(WITH_NLIGHTNING)
    add_definitions(-DNLIGHTNING)
    add_subdirectory(modules/nlightning)
endif()

if(WITH_ECLAIR)
    add_definitions(-DECLAIR)
    add_subdirectory(modules/eclair)
endif()

if(WITH_EMBIT)
    add_definitions(-DEMBIT)
    add_subdirectory(modules/embit)
endif()

if(WITH_CLIGHTNING)
    add_definitions(-DCLIGHTNING)
    add_subdirectory(modules/clightning)
endif()

if(WITH_LIGHTNING_KMP)
    add_definitions(-DLIGHTNING_KMP)
    add_subdirectory(modules/lightningkmp)
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

if(WITH_LND)
    target_link_libraries(bitcoinfuzz PRIVATE lnd_module)
endif()

if(WITH_LDK)
    target_link_libraries(bitcoinfuzz PRIVATE ldk_module)
endif()

if(WITH_CLIGHTNING)
    target_link_libraries(bitcoinfuzz PRIVATE clightning_module)
endif()

if(WITH_ECLAIR)
    target_link_libraries(bitcoinfuzz PRIVATE eclair_module)
endif()

if(WITH_NBITCOIN)
    target_link_libraries(bitcoinfuzz PRIVATE nbitcoin_module)
endif()

if(WITH_NLIGHTNING)
    target_link_libraries(bitcoinfuzz PRIVATE nlightning_module)
endif()

if(WITH_EMBIT)
    target_link_libraries(bitcoinfuzz PRIVATE embit_module)
endif()

if(WITH_LIGHTNING_KMP)
    target_link_libraries(bitcoinfuzz PRIVATE lightningkmp_module)
endif()

message("\n")
message("Configure summary")
message("=================")
message("Bitcoin Core ............................. ${WITH_BITCOIN_CORE}")
message("BTCD ............................. ${WITH_BTCD}")
message("Rust Bitcoin ............................. ${WITH_RUST_BITCOIN}")
message("Rust Miniscript ............................. ${WITH_RUST_MINISCRIPT}")
message("LND ............................. ${WITH_LND}")
message("Embit ............................. ${WITH_EMBIT}")
message("CLightning ............................. ${WITH_CLIGHTNING}")
message("Eclair ............................. ${WITH_ECLAIR}")
message("LDK ............................. ${WITH_LDK}")
message("NBitcoin ............................. ${WITH_NBITCOIN}")
message("Lightning-KMP ............................. ${WITH_LIGHTNING_KMP}")
message("NLightning ............................. ${WITH_NLIGHTNING}")