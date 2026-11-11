# bitcoinkernel module

Upstream: [Bitcoin Core libbitcoinkernel](https://github.com/bitcoin/bitcoin/blob/master/src/kernel/bitcoinkernel.cpp)

Use this module when you want to run kernel fuzz targets against a single
`libbitcoinkernel` version.

- Default version: `master`
- Pin a version: set `BITCOINKERNEL_REF` to a tag, branch, or commit
- Optional: set `BITCOINKERNEL_URL` to clone or fetch from a fork (default `https://github.com/bitcoin/bitcoin.git`)

## Build

```bash
cd modules/bitcoinkernel
export BITCOINKERNEL_REF=[ref]
make
export CXXFLAGS="$CXXFLAGS -DBITCOINKERNEL"
```
