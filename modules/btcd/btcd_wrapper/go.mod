module btcd_wrapper

go 1.23.2

toolchain go1.24.2

require (
	github.com/btcsuite/btcd v0.24.2
	github.com/btcsuite/btcd/btcutil v1.1.7-0.20250506233109-1eb974aab6ef
	github.com/btcsuite/btcd/btcutil/psbt v1.1.11-0.20250506233109-1eb974aab6ef
)

require (
	github.com/btcsuite/btcd/btcec/v2 v2.3.5 // indirect
	github.com/btcsuite/btcd/chaincfg/chainhash v1.1.0 // indirect
	github.com/btcsuite/btclog v0.0.0-20170628155309-84c8d2346e9f // indirect
	github.com/decred/dcrd/crypto/blake256 v1.0.0 // indirect
	github.com/decred/dcrd/dcrec/secp256k1/v4 v4.0.1 // indirect
	golang.org/x/crypto v0.25.0 // indirect
	golang.org/x/sys v0.22.0 // indirect
)

replace github.com/btcsuite/btcd => github.com/brunoerg/btcd v0.0.0-20251028171902-4f6ab3d76fd7
