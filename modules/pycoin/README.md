# pycoin module

Upstream: [pycoin](https://github.com/richardkiss/pycoin)

## Dependencies

To run the fuzzer with the `pycoin` module, you need to install the `pycoin`
library:

```bash
cd modules/pycoin
pip install -r ./requirements.txt
```

## Build

```bash
cd modules/pycoin
make
export CXXFLAGS="$CXXFLAGS -DPYCOIN"
```
