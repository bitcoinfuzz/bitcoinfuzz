# libbitcoin-system module

Upstream: [libbitcoin-system](https://github.com/libbitcoin/libbitcoin-system)

## Dependencies

Requires Boost (>= 1.86), CMake (>= 3.30), and binutils (`objcopy`).

On Debian/Ubuntu:

```bash
apt-get install libboost-all-dev cmake binutils
```

## Build

```bash
git submodule update --init --recursive external/libbitcoin-system external/secp256k1
cd modules/libbitcoinsystem
make
export CXXFLAGS="$CXXFLAGS -DLIBBITCOIN_SYSTEM"
```
