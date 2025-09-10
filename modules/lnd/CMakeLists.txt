# Include directories
include_directories(.)
include_directories(../../include)
include_directories(./lnd_wrapper)

# Find required libraries
find_library(SCRIPT_LIB script HINTS .)

# Custom target to build Go library
add_custom_target(golang
    COMMAND ${CMAKE_COMMAND} -E chdir lnd_wrapper go build -o liblnd_wrapper.a -buildmode=c-archive -tags=libfuzzer -gcflags=all=-d=libfuzzer wrapper.go
    COMMENT "Building Go wrapper library"
    BYPRODUCTS lnd_wrapper/liblnd_wrapper.a lnd_wrapper/liblnd_wrapper.h
)

# Create object library for module.cpp
add_library(module_obj OBJECT module.cpp)
target_link_libraries(module_obj ${SCRIPT_LIB})

# Custom command to merge libraries
add_custom_command(
    OUTPUT module.a
    COMMAND bash ../../merge.sh module.a lnd_wrapper/liblnd_wrapper.a $<TARGET_OBJECTS:module_obj>
    COMMAND ranlib module.a
    DEPENDS golang module_obj lnd_wrapper/liblnd_wrapper.a
    COMMENT "Merging libraries into module.a"
)

# Custom target for the final library
add_custom_target(module_library ALL
    DEPENDS module.a
)

# Clean target (CMake provides this automatically, but you can customize if needed)
set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "module.a")