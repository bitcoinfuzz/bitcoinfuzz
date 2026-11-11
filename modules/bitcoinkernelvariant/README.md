# bitcoinkernel-variant module

This module is used for differential fuzzing between two `libbitcoinkernel`
versions.

For differential fuzzing between two bitcoinkernel versions, build two modules:

- `BITCOINKERNEL` (base; `BITCOINKERNEL_REF`, default `master`)
- `BITCOINKERNEL_VARIANT` (comparison; `BITCOINKERNEL_VARIANT_REF`, default `master`)
- Optional: set `BITCOINKERNEL_VARIANT_URL` to clone or fetch from a fork (default `https://github.com/bitcoin/bitcoin.git`)

## Build

```bash
# 1) base (creates the reference repo at modules/bitcoinkernel/bitcoin)
cd modules/bitcoinkernel
export BITCOINKERNEL_REF=[base_ref]
make
export CXXFLAGS="$CXXFLAGS -DBITCOINKERNEL"

# 2) variant (ref repo is required for build)
cd ../bitcoinkernelvariant
BITCOINKERNEL_VARIANT_REF=<variant_ref>
make
export CXXFLAGS="$CXXFLAGS -DBITCOINKERNEL_VARIANT"
```
