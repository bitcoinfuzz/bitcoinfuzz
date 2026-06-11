# libwally-core module

Upstream: [libwally-core](https://github.com/ElementsProject/libwally-core)

## Build

```bash
git submodule update --init --recursive external/libwally-core
cd modules/libwallycore
make
export CXXFLAGS="$CXXFLAGS -DLIBWALLY_CORE"
```
