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

# Core Lightning uses Mako to generate source files.
if [ -f "modules/clightning/Makefile" ] && ! python3 -c 'import mako.template' 2>/dev/null; then
    python3 -m pip install -q mako
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
