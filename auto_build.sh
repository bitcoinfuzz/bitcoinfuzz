#!/bin/bash

if [ -z "$CXXFLAGS" ]; then
    echo "Error: CXXFLAGS not defined. Example: CXXFLAGS=\"-DLDK -DLND\" ./auto_build.sh"
    exit 1
fi

echo "Cleaning previous builds..."
make clean || { echo "Error: 'make clean' failed"; exit 1; }

# Execute a command in a directory
execute_in_dir() {
    local dir=$1
    local command=$2

    if [ -d "$dir" ]; then
        echo "Executing in $dir: $command"
        (cd "$dir" && eval "$command") || { echo "Error: Failed to execute in $dir"; exit 1; }
    else
        echo "Error: Directory $dir does not exist"
        exit 1
    fi
}

get_module_dir() {
    case "$1" in
        CUSTOM_MUTATOR_*)  echo "custommutator" ;;
        BITCOIN_CORE)      echo "modules/bitcoin" ;;
        *)                 echo "modules/$(echo "$1" | tr '[:upper:]' '[:lower:]' | tr -d '_')" ;;
    esac
}

# Check if module needs rust nightly
needs_rust_nightly() {
    case "$1" in
        RUST_BITCOIN|RUST_MINISCRIPT|LDK|TINY_MINISCRIPT|RUSTBITCOINKERNEL) return 0 ;;
        *) return 1 ;;
    esac
}

# Extract flags from CXXFLAGS (e.g., "-DLDK -DLND" -> "LDK LND")
get_flags() {
    echo "$CXXFLAGS" | grep -oE '\-D[A-Z_]+' | sed 's/-D//g'
}

# Full clean
full_clean() {
    echo "Performing full clean..."
    for dir in modules/*/; do
        execute_in_dir "$dir" "make clean"
    done
    execute_in_dir "custommutator" "make clean"
}

# Clean modules matching flags
clean_by_flags() {
    local flags=$1
    echo "Cleaning modules: $flags"
    for flag in $flags; do
        execute_in_dir "$(get_module_dir "$flag")" "make clean"
    done
}

# Determine which clean to perform
if [ -n "$CLEAN_BUILD" ]; then
    case "$CLEAN_BUILD" in
        FULL)  full_clean ;;
        CLEAN) clean_by_flags "$(get_flags)" ;;
        *)     clean_by_flags "$CLEAN_BUILD" ;;
    esac
else
    echo "No CLEAN_BUILD option specified. Skipping clean step."
fi

# Build modules based on CXXFLAGS
echo "Compiling selected modules with CXXFLAGS=$CXXFLAGS..."

for flag in $(get_flags); do
    dir=$(get_module_dir "$flag")
    if needs_rust_nightly "$flag"; then
        execute_in_dir "$dir" "rustup default nightly && make cargo && make"
    else
        execute_in_dir "$dir" "make"
    fi
done

# Final build
echo "Compiling the main project in the root..."
make || { echo "Error: Failed to compile the main project"; exit 1; }

echo "Build completed successfully!"
