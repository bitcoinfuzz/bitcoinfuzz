module btcd_wrapper

go 1.26.10

require (
	github.com/btcsuite/btcd v0.26.0-beta.rc1.0.20261022124420-fcaf21884a3d
	github.com/btcsuite/btcd/address/v2 v2.0.1
	github.com/btcsuite/btcd/btcec/v2 v2.5.1
	github.com/btcsuite/btcd/btcutil/v2 v2.0.2
	github.com/btcsuite/btcd/chaincfg/v2 v2.0.1
	github.com/btcsuite/btcd/chainhash/v2 v2.0.0
	github.com/btcsuite/btcd/psbt/v2 v2.0.1
	github.com/btcsuite/btcd/txscript/v2 v2.0.1
	github.com/btcsuite/btcd/wire/v2 v2.0.2
)

require (
	github.com/btcsuite/btclog v1.0.0 // indirect
	github.com/decred/dcrd/crypto/blake256 v1.1.0 // indirect
	github.com/decred/dcrd/dcrec/secp256k1/v4 v4.4.1 // indirect
	golang.org/x/crypto v0.57.0 // indirect
	golang.org/x/sys v0.48.0 // indirect
)

replace github.com/btcsuite/btcd => github.com/bitcoinfuzz/btcd v0.26.0-beta.rc1.0.20261022124420-fcaf21884a3d

replace github.com/btcsuite/btcd/address/v2 => github.com/bitcoinfuzz/btcd/address/v2 v2.0.1-0.20261022124420-fcaf21884a3d

replace github.com/btcsuite/btcd/btcec/v2 => github.com/bitcoinfuzz/btcd/btcec/v2 v2.5.1-0.20261022124420-fcaf21884a3d

replace github.com/btcsuite/btcd/btcutil/v2 => github.com/bitcoinfuzz/btcd/btcutil/v2 v2.0.1-0.20261022124420-fcaf21884a3d

replace github.com/btcsuite/btcd/chaincfg/v2 => github.com/bitcoinfuzz/btcd/chaincfg/v2 v2.0.1-0.20261022124420-fcaf21884a3d

replace github.com/btcsuite/btcd/chainhash/v2 => github.com/bitcoinfuzz/btcd/chainhash/v2 v2.0.1-0.20261022124420-fcaf21884a3d

replace github.com/btcsuite/btcd/psbt/v2 => github.com/bitcoinfuzz/btcd/psbt/v2 v2.0.1-0.20261022124420-fcaf21884a3d

replace github.com/btcsuite/btcd/txscript/v2 => github.com/bitcoinfuzz/btcd/txscript/v2 v2.0.1-0.20261022124420-fcaf21884a3d

replace github.com/btcsuite/btcd/wire/v2 => github.com/bitcoinfuzz/btcd/wire/v2 v2.0.1-0.20261022124420-fcaf21884a3d
