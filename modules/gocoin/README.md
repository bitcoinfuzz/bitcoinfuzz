# gocoin module

Upstream: [gocoin](https://github.com/piotrnar/gocoin)

[gocoin](https://github.com/piotrnar/gocoin) is a full Bitcoin node
implementation written in Go.

## Build

```bash
cd modules/gocoin
make
export CXXFLAGS="$CXXFLAGS -DGOCOIN"
```

## Notes

gocoin cannot be used together with btcd in the same build due to cgo symbol
conflicts. Both embed the Go runtime.
