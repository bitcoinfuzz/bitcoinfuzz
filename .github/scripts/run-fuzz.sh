#!/usr/bin/env bash
set -euo pipefail

CORPUS_DIR="$1"
TARGET="$2"  
CXXFLAGS="$3"
ASAN_OPTIONS="${4:-}"

export CXXFLAGS="${CXXFLAGS}"
[[ -n "${ASAN_OPTIONS}" ]] && export ASAN_OPTIONS="${ASAN_OPTIONS}"
make
if [ -d "./corpora/${CORPUS_DIR}" ]; then
  echo "Using corpora for ${TARGET}"
  FUZZ="${TARGET}" ./bitcoinfuzz -runs=1 "./corpora/${CORPUS_DIR}"
else
  echo "No/empty corpus for ${TARGET}. Doing a short smoke run."
  TMP_CORPUS="$(mktemp -d)"
  : > "${TMP_CORPUS}/empty"
  printf "\x00" > "${TMP_CORPUS}/zero"
  FUZZ="${TARGET}" ./bitcoinfuzz -runs=1 "${TMP_CORPUS}"
fi