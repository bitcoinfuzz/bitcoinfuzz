# Bitcoin Core module

Upstream: [Bitcoin Core](https://github.com/bitcoin/bitcoin)

## Build

```bash
git submodule update --init external/bitcoin-core
cd modules/bitcoin
make
export CXXFLAGS="$CXXFLAGS -DBITCOIN_CORE"
```

To update to a newer Bitcoin Core version:

```bash
git submodule update --remote external/bitcoin-core
cd modules/bitcoin && make clean && make
```
