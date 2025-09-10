# Set C++ standard and compiler flags
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fsanitize=fuzzer,address")

# Include directories
include_directories(
    .
    ../../include
    /opt/homebrew/include
    ./secp256k1/include
    ./univalue/include
    ./consensus
    ./primitives
    ./script
)

# Find all .cpp files, excluding specific ones
file(GLOB_RECURSE BITCOIN_SRCS CONFIGURE_DEPENDS "*.cpp")
list(FILTER BITCOIN_SRCS EXCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}/univalue/")
list(FILTER BITCOIN_SRCS EXCLUDE REGEX "^${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/")
list(FILTER BITCOIN_SRCS EXCLUDE REGEX "module.cpp$")
list(FILTER BITCOIN_SRCS EXCLUDE REGEX "cleanse.cpp$")

# Define paths to static libs
set(SECP256K1_LIB ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/.libs/libsecp256k1.a)
set(UNIVALUE_LIB ${CMAKE_CURRENT_SOURCE_DIR}/univalue/build/libunivalue.a)

# Check for BOOST_LIB_DIR
if(NOT DEFINED BOOST_LIB_DIR)
    message(FATAL_ERROR "BOOST_LIB_DIR is not defined. Please set it via -DBOOST_LIB_DIR=<path>")
endif()

set(BOOST_FILESYSTEM ${BOOST_LIB_DIR}/libboost_filesystem.a)
set(BOOST_SYSTEM ${BOOST_LIB_DIR}/libboost_system.a)

# Build secp256k1
execute_process(
    COMMAND bash -c "./autogen.sh && CFLAGS='-fsanitize=address' ./configure --enable-module-recovery --enable-experimental --enable-module-schnorrsig --disable-shared --with-pic && make"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/secp256k1
    RESULT_VARIABLE SECP256K1_RESULT
)

# Build univalue
execute_process(
    COMMAND bash -c "cmake -B build -DCMAKE_CXX_FLAGS='-fsanitize=address' && cmake --build build && cd build && make univalue"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/univalue
    RESULT_VARIABLE UNIVALUE_RESULT
)

# Build static lib from core sources
add_library(bitcoin_core_module STATIC module.cpp support/cleanse.cpp ${BITCOIN_SRCS})

target_link_libraries(bitcoin_core_module PUBLIC basemodule)

# Extract and append object files from external static libs
add_custom_command(
    TARGET bitcoin_core_module POST_BUILD
    COMMAND mkdir -p tmp_obj

    # Extract Boost filesystem
    COMMAND ${CMAKE_AR} x ${BOOST_FILESYSTEM}
    COMMAND ${CMAKE_COMMAND} -E copy *.o tmp_obj/
    COMMAND rm -f *.o

    # Extract Boost system
    COMMAND ${CMAKE_AR} x ${BOOST_SYSTEM}
    COMMAND ${CMAKE_COMMAND} -E copy *.o tmp_obj/
    COMMAND rm -f *.o

    # Extract secp256k1
    COMMAND ${CMAKE_AR} x ${SECP256K1_LIB}
    COMMAND ${CMAKE_COMMAND} -E copy *.o tmp_obj/
    COMMAND rm -f *.o

    # Extract univalue
    COMMAND ${CMAKE_AR} x ${UNIVALUE_LIB}
    COMMAND ${CMAKE_COMMAND} -E copy *.o tmp_obj/
    COMMAND rm -f *.o

    # Append all .o files to the static archive
    COMMAND ${CMAKE_AR} rcs $<TARGET_FILE:bitcoin_core_module> tmp_obj/*.o

    # Cleanup
    COMMAND rm -rf tmp_obj
)
