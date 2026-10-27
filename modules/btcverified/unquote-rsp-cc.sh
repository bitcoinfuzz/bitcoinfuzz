#!/bin/sh
# Lake links btc-verified's kernel through a response file holding one
# JSON-quoted path per line. clang strips those quotes, but afl-clang-fast
# expands response files itself and keeps them. Hand the real compiler
# (BTC_VERIFIED_REAL_CC) an unquoted copy of each response file instead.
set -eu

for arg; do
  shift
  case "$arg" in
  @*)
    unquoted="${arg#@}.unquoted"
    sed 's/^"\(.*\)"$/\1/' "${arg#@}" >"$unquoted"
    set -- "$@" "@$unquoted"
    ;;
  *)
    set -- "$@" "$arg"
    ;;
  esac
done

exec "$BTC_VERIFIED_REAL_CC" "$@"
