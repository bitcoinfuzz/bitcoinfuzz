#!/usr/bin/env bash
# Build every module present at the current commit.
# This script is called by CI when doing commit-by-commit builds so that
# each commit defines its own set of modules rather than relying on the
# static step list baked into the workflow file.
set -euo pipefail

REPO_ROOT=$(git rev-parse --show-toplevel)
cd "$REPO_ROOT"

BUILT=0
FAILED=0

build() {
    local path="$1"
    local name
    name=$(basename "$path")

    if [ ! -d "$path" ]; then
        return 0
    fi

    if [ ! -f "$path/Makefile" ]; then
        echo "==> Skipping $name (no Makefile)"
        return 0
    fi

    echo "==> Building $name"
    if (cd "$path" && make); then
        BUILT=$((BUILT + 1))
    else
        echo "FAILED: $name"
        FAILED=$((FAILED + 1))
    fi
}

# Install Rust nightly without changing the user's global default toolchain.
if command -v rustup &>/dev/null; then
    rustup toolchain install nightly --profile minimal
fi

# Install embit Python dependencies only when the module is present
if [ -f "modules/embit/requirements.txt" ]; then
    echo "==> Installing embit dependencies"
    pip install -r modules/embit/requirements.txt -q
    pip install mako -q
fi

# Build custommutator first; fuzz targets may depend on it
build "custommutator"

# Build every module present at this commit
if [ -d "modules" ]; then
    for module_path in modules/*/; do
        build "${module_path%/}"
    done
fi

echo ""
echo "Modules built: $BUILT  Modules failed: $FAILED"
[ "$FAILED" -eq 0 ]
