# C-lightning module

Upstream: [Core Lightning](https://github.com/ElementsProject/lightning)

## Build

```bash
pip install mako
git submodule update --init --recursive external/lightning
cd modules/clightning
make
export CXXFLAGS="$CXXFLAGS -DCLIGHTNING"
```
