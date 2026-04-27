#!/bin/bash
# rename_cgo_symbols.sh - Rename CGO runtime symbols in a Go c-archive to avoid
# duplicate symbol conflicts when linking multiple Go modules together.
#
# Usage: rename_cgo_symbols.sh <archive> <prefix>
#
# When Go builds a c-archive (-buildmode=c-archive), it bundles CGO runtime
# support symbols (e.g. _cgo_topofstack, _cgo_panic, crosscall2) into the
# archive. Linking two or more such archives in the same binary causes
# "multiple definition" linker errors.
#
# This script renames every globally-visible CGO runtime symbol to
# PREFIX_<original_name> using objcopy --redefine-syms. Both the definition
# and all internal references are updated, so each module's CGO runtime
# becomes self-contained under its own namespace.
#
# Module-specific CGO symbols (those containing a 12-char hex hash such as
# _cgo_0ada0d83d011_Cfunc_free) are left untouched.
#
# Requires: nm, ar, ranlib, objcopy (or llvm-objcopy)

set -e

ARCHIVE="$1"
PREFIX="$2"

if [ -z "$ARCHIVE" ] || [ -z "$PREFIX" ]; then
    echo "Usage: $0 <archive> <prefix>" >&2
    exit 1
fi

if [ ! -f "$ARCHIVE" ]; then
    echo "Error: Archive '$ARCHIVE' not found" >&2
    exit 1
fi

# Resolve to absolute path before changing directories
ARCHIVE="$(cd "$(dirname "$ARCHIVE")" && pwd)/$(basename "$ARCHIVE")"

# Find objcopy (prefer llvm-objcopy on macOS where GNU objcopy is unavailable)
OBJCOPY=$(command -v objcopy 2>/dev/null || command -v llvm-objcopy 2>/dev/null || echo "")
if [ -z "$OBJCOPY" ]; then
    echo "Error: objcopy not found. Install with: apt-get install binutils (Linux) or brew install llvm (macOS)" >&2
    exit 1
fi

WORK_DIR=$(mktemp -d)
trap 'rm -rf "$WORK_DIR"' EXIT

MAP_FILE="$WORK_DIR/cgo_runtime.map"

# Find all globally-visible CGO runtime symbols in the archive.
#
# CGO runtime symbols match: _cgo_*, x_cgo_*, crosscall1, crosscall2, and
# derived names like _x_crosscall2_ptr.
#
# Module-specific CGO symbols contain a 12-char lowercase hex hash (e.g.
# _cgo_0ada0d83d011_Cfunc_free or _cgoexp_0ada0d83d011_MyFunc) and must be
# excluded so the exported Go functions remain accessible under their
# original names.
#
# nm type letters: T=text D=data B=bss R=rodata S=other-section (all global)
nm "$ARCHIVE" 2>/dev/null \
    | awk '/ [TDBRS] / { print $NF }' \
    | grep -E '(_cgo_|x_cgo_|crosscall)' \
    | grep -vE '[0-9a-f]{12}' \
    | sort -u \
    | while IFS= read -r sym; do
        printf '%s %s_%s\n' "$sym" "$PREFIX" "$sym"
    done > "$MAP_FILE"

if [ ! -s "$MAP_FILE" ]; then
    echo "No CGO runtime symbols found in $(basename "$ARCHIVE"), nothing to rename."
    exit 0
fi

COUNT=$(wc -l < "$MAP_FILE" | tr -d ' ')
echo "Renaming $COUNT CGO runtime symbols with prefix '${PREFIX}_' in $(basename "$ARCHIVE") ..."

# Extract all objects, apply the symbol rename map, repack the archive.
OBJS_DIR="$WORK_DIR/objs"
mkdir -p "$OBJS_DIR"
(cd "$OBJS_DIR" && ar x "$ARCHIVE")

for obj in "$OBJS_DIR"/*.o; do
    [ -f "$obj" ] || continue
    "$OBJCOPY" --redefine-syms="$MAP_FILE" "$obj" 2>/dev/null || true
done

ar rcs "$ARCHIVE" "$OBJS_DIR"/*.o
ranlib "$ARCHIVE"

echo "Done. CGO runtime symbols in $(basename "$ARCHIVE") now prefixed with '${PREFIX}_'."
