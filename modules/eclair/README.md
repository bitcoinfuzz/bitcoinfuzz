# Eclair module

Upstream: [Eclair](https://github.com/ACINQ/eclair)

## Build

```bash
git submodule update --init --recursive external/eclair
cd modules/eclair
make
export CXXFLAGS="$CXXFLAGS -DECLAIR"
```

## Notes

When fuzzing with Eclair, the JVM's JIT compiler allocates memory that LSan
cannot track through the standard allocator, causing false-positive leak
reports. Use the provided suppressions file to silence them:

```bash
LSAN_OPTIONS="suppressions=$(pwd)/lsan.supp" FUZZ=deserialize_invoice ./bitcoinfuzz
LSAN_OPTIONS="suppressions=$(pwd)/lsan.supp" FUZZ=deserialize_offer ./bitcoinfuzz
```

This suppressions file was only validated on Ubuntu and might not work depending
on your setup. You can write your own file or disable the leak detection
globally.
