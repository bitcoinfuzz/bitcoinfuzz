#!/usr/bin/env bash
set -euo pipefail

IMAGE=${IMAGE:-bitcoinfuzz:descriptor_parse}
WORK_ROOT=${WORK_ROOT:-/tmp/descriptor_cases}

mkdir -p "$WORK_ROOT"

run_case() {
  local name="$1"
  local value="$2"
  local dir="$WORK_ROOT/$name"

  mkdir -p "$dir"
  printf '%s' "$value" > "$dir/one"

  local out
  local exit_code
  set +e
  out=$(docker run --rm \
    --entrypoint /app/bitcoinfuzz \
    -e MODULES=BITCOIN_CORE,NBITCOIN \
    -e LIBFUZZ_DETECT_LEAKS=0 \
    -e ASAN_OPTIONS=detect_leaks=0 \
    -v "$dir:/work" \
    "$IMAGE" \
    -runs=1 -detect_leaks=0 /work/one 2>&1)
  exit_code=$?
  set -e

  local mismatch
  mismatch=$(printf '%s\n' "$out" | grep -E 'Descriptor parse failed|Assertion' -c || true)

  printf 'CASE=%s EXIT=%s MISMATCH_LINES=%s\n' "$name" "$exit_code" "$mismatch"
}

run_case raw_bc 'raw(bc)'
run_case raw_long 'raw(77227225555755575555755575)'
run_case wsh_raw 'wsh(raw(deadbeef))'
run_case sh_raw 'sh(raw(deadbeef))'
run_case raw_deadbeef 'raw(deadbeef)'
run_case raw_odd 'raw(abc)'
run_case raw_nonhex 'raw(zz)'
run_case raw_empty 'raw()'
run_case raw_checksum_inside 'raw(deadbeef#abcd)'
run_case sh_raw_checksum_inside 'sh(raw(deadbeef#abcd))'
run_case tr_raw_context 'tr(02c0ded5f5d9d6cd0f2df9f8b7f8f2a0fb36f10f2dd6f0b11a0d5fdac9b5f7f56a,raw(deadbeef))'
