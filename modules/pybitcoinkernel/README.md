# py-bitcoinkernel module

Upstream: [py-bitcoinkernel](https://github.com/stickies-v/py-bitcoinkernel)

## Dependencies

To run the fuzzer with the `py-bitcoinkernel` module, you need to install the
`py-bitcoinkernel` library:

```bash
cd modules/pybitcoinkernel
pip install -r ./requirements.txt
```

## Build

```bash
cd modules/pybitcoinkernel
make
export CXXFLAGS="$CXXFLAGS -DPYBITCOINKERNEL"
```
