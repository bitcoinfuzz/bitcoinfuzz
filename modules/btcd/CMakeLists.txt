# Go Library Path
set(GO_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/btcd_wrapper/libbtcd_wrapper.a")

# Build Go library
add_custom_target(go_build
    COMMAND go build -o libbtcd_wrapper.a -buildmode=c-archive -tags=libfuzzer -gcflags=all=-d=libfuzzer wrapper.go
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/btcd_wrapper
    COMMENT "Building Go BTCD wrapper library"
)

# Create Go library file
add_custom_command(
    OUTPUT ${GO_LIB_PATH}
    COMMAND go build -o libbtcd_wrapper.a -buildmode=c-archive -tags=libfuzzer -gcflags=all=-d=libfuzzer wrapper.go
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/btcd_wrapper
    COMMENT "Building Go BTCD wrapper library"
)

# Create the btcd_module library
add_library(btcd_module STATIC module.cpp module.h)

# Set target properties
target_include_directories(btcd_module PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/btcd_wrapper
)

target_compile_options(btcd_module PRIVATE 
    -Wall 
    -Wextra 
    -fPIC
)

# Link the basemodule library to resolve BaseModule symbols
target_link_libraries(btcd_module PRIVATE basemodule)

# Note: libscript.h is available in btcd_wrapper directory
# The script functionality is likely header-only or included in the Go-built archive

# Custom command to merge libraries after btcd_module is built
add_custom_command(
    TARGET btcd_module POST_BUILD
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/../../merge.sh 
            ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}btcd_module${CMAKE_STATIC_LIBRARY_SUFFIX}
            ${GO_LIB_PATH} 
            ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/btcd_module.dir/module.cpp.o
    COMMAND ranlib ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}btcd_module${CMAKE_STATIC_LIBRARY_SUFFIX}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Merging Go BTCD library with C++ module"
)

# Make sure Go library is built before the module
add_dependencies(btcd_module go_build)