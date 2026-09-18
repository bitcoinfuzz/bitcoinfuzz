#!/bin/bash
# rename_cgo_symbols.sh - Rename CGO runtime symbols in a Go c-archive to avoid
# duplicate symbol conflicts when linking multiple Go modules together.
#
# Usage: rename_cgo_symbols.sh <archive> <prefix>
#
# When Go builds a c-archive (-buildmode=c-archive), it bundles CGO runtime
# support symbols (e.g. _cgo_topofstack, _cgo_panic, crosscall2, threadentry)
# into the archive. Linking two or more such archives in the same binary causes
# "multiple definition" linker errors.
#
# This script renames every globally-visible symbol the archive defines to
# PREFIX_<original_name> using objcopy --redefine-syms, except for the two
# groups that belong to the module rather than to the CGO runtime:
#
#   * Everything the generated cgo header declares. That header is the
#     archive's public surface -- the //export'ed Go functions the C++ module
#     calls -- and is found next to the archive as <archive-basename>.h.
#   * Module-specific CGO symbols, which carry a 12-char hex hash derived from
#     the package (e.g. _cgo_0ada0d83d011_Cfunc_free).
#
# Selecting by what the module owns rather than by symbol-name patterns matters:
# the CGO runtime helpers are not consistently named, and the set changes across
# Go releases. Go 1.27 stopped marking `fatalf` and `threadentry` static when it
# merged the per-arch gcc_linux_amd64.c into gcc_unix.c, and a pattern matching
# only _cgo_*/x_cgo_*/crosscall* silently stopped covering the runtime.
#
# Both the definition and all internal references are updated, so each module's
# CGO runtime becomes self-contained under its own namespace.
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
HEADER="${ARCHIVE%.a}.h"

# Find objcopy (prefer llvm-objcopy on macOS where GNU objcopy is unavailable)
OBJCOPY=$(command -v objcopy 2>/dev/null || command -v llvm-objcopy 2>/dev/null || echo "")
if [ -z "$OBJCOPY" ]; then
    echo "Error: objcopy not found. Install with: apt-get install binutils (Linux) or brew install llvm (macOS)" >&2
    exit 1
fi

WORK_DIR=$(mktemp -d)
trap 'rm -rf "$WORK_DIR"' EXIT

MAP_FILE="$WORK_DIR/cgo_runtime.map"
KEEP_FILE="$WORK_DIR/keep.txt"
DEFINED_FILE="$WORK_DIR/defined.txt"

# Every global symbol the archive defines, minus the module-specific CGO
# symbols carrying a 12-char hex package hash.
#
# nm type letters: T=text D=data B=bss R=rodata S=other-section (all global)
LC_ALL=C nm "$ARCHIVE" 2>/dev/null \
    | awk '/ [TDBRS] / { print $NF }' \
    | grep -vE '[0-9a-f]{12}' \
    | LC_ALL=C sort -u > "$DEFINED_FILE"

# Names declared by the generated cgo header: the archive's public surface,
# which must keep its original spelling for the C++ module to link against.
if [ -f "$HEADER" ]; then
    awk '/^extern[ \t]/ {
            decl = $0
            paren = index(decl, "(")
            if (paren > 0)
                decl = substr(decl, 1, paren - 1)
            gsub(/[^A-Za-z0-9_]/, " ", decl)
            count = split(decl, parts, " ")
            if (count > 0)
                print parts[count]
         }' "$HEADER" | LC_ALL=C sort -u > "$KEEP_FILE"
else
    # No header next to the archive: fall back to matching the CGO runtime by
    # name, which is what this script did before it could consult the header.
    echo "Warning: $(basename "$HEADER") not found, matching CGO runtime symbols by name." >&2
    grep -vE '(_cgo_|x_cgo_|crosscall)' "$DEFINED_FILE" | LC_ALL=C sort -u > "$KEEP_FILE"
fi

LC_ALL=C comm -23 "$DEFINED_FILE" "$KEEP_FILE" \
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
