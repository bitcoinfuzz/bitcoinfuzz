# embit module

Upstream: [embit](https://github.com/diybitcoinhardware/embit)

## Dependencies

To run the fuzzer with the `embit` module, you need to install the `embit`
library:

```bash
cd modules/embit
pip install -r ./requirements.txt
```

## Build

```bash
cd modules/embit
make
export CXXFLAGS="$CXXFLAGS -DEMBIT"
```
