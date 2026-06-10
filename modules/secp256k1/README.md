# Libsecp256k1 module

Upstream: [Libsecp256k1](https://github.com/bitcoin-core/secp256k1)

## Build

```bash
git submodule update --init --recursive external/secp256k1
cd modules/secp256k1
make
export CXXFLAGS="$CXXFLAGS -DSECP256K1"
```
