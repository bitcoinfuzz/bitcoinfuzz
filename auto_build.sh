#!/bin/bash

if [ -z "$CXXFLAGS" ]; then
    echo "Error: CXXFLAGS not defined. Example: CXXFLAGS=\"-DLDK -DLND\" ./auto_build.sh"
    exit 1
fi

# Runs make clean to clean everything before starting
echo "Cleaning previous builds..."
make clean || { echo "Error: 'make clean' failed"; exit 1; }

# Function to execute a command in a module's directory
execute_in_module() {
    local module_dir=$1
    local command=$2

    if [ -d "$module_dir" ]; then
        echo "Executing in $module_dir: $command"
        (cd "$module_dir" && eval "$command") || { echo "Error: Failed to execute in $module_dir"; exit 1; }
    else
        echo "Error: Directory $module_dir does not exist"
        exit 1
    fi
}

# Functions for each module
bitcoin_core() {
    execute_in_module "modules/bitcoin" "$1"
}

rust_bitcoin() {
    execute_in_module "modules/rustbitcoin" "$1"
}

rust_miniscript() {
    execute_in_module "modules/rustminiscript" "$1"
}

btcd() {
    execute_in_module "modules/btcd" "$1"
}

nbitcoin() {
    execute_in_module "modules/nbitcoin" "$1"
}

embit() {
    execute_in_module "modules/embit" "$1"
}

lnd() {
    execute_in_module "modules/lnd" "$1"
}

ldk() {
    execute_in_module "modules/ldk" "$1"
}

nlightning() {
    execute_in_module "modules/nlightning" "$1"
}

clightning() {
    execute_in_module "modules/clightning" "$1"
}

eclair() {
    execute_in_module "modules/eclair" "$1"
}

custom_mutator_bolt11() {
    execute_in_module "modules/custommutator" "$1"
}

custom_mutator_bolt12_offer() {
    execute_in_module "modules/custommutator" "$1"
}

custom_mutator_p2p_message() {
    execute_in_module "modules/custommutator" "$1"
}

# Define the list of modules
modules="bitcoin_core rust_bitcoin rust_miniscript btcd nbitcoin embit lnd ldk nlightning clightning eclair custom_mutator_bolt11 custom_mutator_bolt12_offer custom_mutator_p2p_message"

# Full clean: runs `make clean` in all module directories
full_clean() {
    echo "Performing full clean..."
    for module in $modules; do
        $module "make clean"
    done
}

# Clean based on CXXFLAGS
clean_selected_by_cxxflags() {
    echo "Performing clean based on CXXFLAGS=$CXXFLAGS..."
    for module in $modules; do
        module_name=$(echo "$module" | tr '[:lower:]' '[:upper:]')
        if echo "$CXXFLAGS" | grep -q "$module_name"; then
            $module "make clean"
        fi
    done
}

# Selective clean based on CLEAN_BUILD flags
select_clean() {
    echo "Performing selective clean based on CLEAN_BUILD=$CLEAN_BUILD..."
    for module in $modules; do
        module_name=$(echo "$module" | tr '[:lower:]' '[:upper:]')
        if echo "$CLEAN_BUILD" | grep -q "$module_name"; then
            $module "make clean"
        fi
    done
}

# Determine which clean to perform
if [ -n "$CLEAN_BUILD" ]; then
    case "$CLEAN_BUILD" in
        FULL)
            full_clean
            ;;
        CLEAN)
            clean_selected_by_cxxflags
            ;;
        *)
            select_clean
            ;;
    esac
else
    echo "No CLEAN_BUILD option specified. Skipping clean step."
fi

# Builds modules based on flags
echo "Compiling selected modules with CXXFLAGS=$CXXFLAGS..."

for module in $modules; do
    module_name=$(echo "$module" | tr '[:lower:]' '[:upper:]')
    if echo "$CXXFLAGS" | grep -q "$module_name"; then
        case "$module" in
            rust_bitcoin|rust_miniscript|ldk)
                $module "rustup default nightly && make cargo && make"
                ;;
            *)
                $module "make"
                ;;
        esac
    fi
done

# Returns to the root and performs the final build of the project
echo "Compiling the main project in the root..."
make || { echo "Error: Failed to compile the main project"; exit 1; }

echo "Build completed successfully!"