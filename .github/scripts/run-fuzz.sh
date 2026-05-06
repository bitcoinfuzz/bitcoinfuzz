#!/usr/bin/env bash
set -euo pipefail

CORPUS_DIR="$1"
TARGET="$2"
CXXFLAGS="$3"
ASAN_OPTIONS="${4:-}"
CXX="${5}"

# Remove the fixed arguments in order to forward
# any extra arguments to libFuzzer
shift 5

if [ -d "./corpora/${CORPUS_DIR}" ]; then
  echo "Using corpora for ${TARGET}"
  export CXXFLAGS="${CXXFLAGS}"
  export CXX="${CXX}"
  [[ -n "${ASAN_OPTIONS}" ]] && export ASAN_OPTIONS="${ASAN_OPTIONS}"
  make
  FUZZ="${TARGET}" ./bitcoinfuzz -runs=1 "$@" "./corpora/${CORPUS_DIR}"
else
  echo "Corpus ./corpora/${CORPUS_DIR} does not exist. Skipping."
fi