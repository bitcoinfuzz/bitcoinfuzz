# btc-verified module

Upstream: [btc-verified](https://github.com/ProofOfKeags/btc-verified)

btc-verified implements a slice of `bitcoinkernel.h` in Lean 4: transaction
create, copy, serialize, check, destroy, and the transaction validation-state
objects. This module loads that shared library and implements the
`kernel_transaction_check` target, which runs the same public-API sequence as
the `BITCOINKERNEL` and `BITCOINKERNEL_VARIANT` modules, so it is compared
against Core's own kernel.

- Default version: `master`
- Pin a version: set `BTC_VERIFIED_REF` to a branch, tag, or commit

## Requirements

[elan](https://github.com/leanprover/elan) (provides `lake` and the pinned
Lean toolchain) and `patchelf`. The first build downloads the mathlib cache
(several GB) and compiles btc-verified's Lean code to C with libFuzzer
coverage. Set `BTC_VERIFIED_DIR` to keep that checkout outside the module tree.

The build copies the instrumented kernel library and the Lean runtime it needs
into `lib/`, linked through `$ORIGIN`, so running the fuzzer needs neither
elan nor the checkout. The Docker images and CI ship that directory.

## Build

Build the `bitcoinkernel` module first (see its README), then:

```bash
cd modules/btcverified
make
cd ../..
export CXXFLAGS="$CXXFLAGS -DBITCOINKERNEL -DBTC_VERIFIED"
make
```

btc-verified pins the Core revision whose header it implements in
`Kernel/abi.toml`, and the build warns when `modules/bitcoinkernel` is at a
different revision. A different revision is still a valid comparison while the
transaction slice is unchanged: the nine functions, the validation enums, and
`CheckTransaction`. The warning prints the `git diff` to confirm that. The
`bitcoinkernel` module itself needs a Core recent enough to have
`btck_transaction_get_wtxid`, so it cannot always be built at the pin.

## Run

```bash
FUZZ=kernel_transaction_check ./bitcoinfuzz
```

or with Docker: `just run kernel_transaction_check`.

The Lean runtime keeps its one-time initialization allocations for the life of
the process, which LeakSanitizer reports at exit. When leak detection is on
(`LIBFUZZ_FORK=0`, or replaying a single input), use the suppressions file:

```bash
LSAN_OPTIONS="suppressions=$(pwd)/lsan.supp" FUZZ=kernel_transaction_check ./bitcoinfuzz
```

The library path is compiled in; override it at runtime with
`BTC_VERIFIED_KERNEL_LIB=/path/to/libbtc_verified_kernel.so`. The library is
loaded with `RTLD_LOCAL` and never unloaded, so its unprefixed `btck_*`
symbols cannot collide with another module's copy of libbitcoinkernel.

Set `BTC_VERIFIED_INJECT_MISMATCH=1` to flip btc-verified's reported check
verdict. A run with it set must abort with a `kernel_transaction_check`
mismatch; it is a check that the comparison is live.
