# python-hdwallet module

Upstream: [python-hdwallet](https://github.com/hdwallet-io/python-hdwallet)

## Dependencies

To run the fuzzer with the `pyhdwallet` module, you need to install the
`hdwallet` library:

```bash
cd modules/pyhdwallet
pip install -r ./requirements.txt
```

## Build

```bash
cd modules/pyhdwallet
make
export CXXFLAGS="$CXXFLAGS -DPYHDWALLET"
```
