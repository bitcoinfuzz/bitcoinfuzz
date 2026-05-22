module btcd_wrapper

go 1.25

require (
	github.com/btcsuite/btcd v0.0.0-20260522174054-81e7e80555dc
	github.com/btcsuite/btcd/address/v2 v2.0.1
	github.com/btcsuite/btcd/btcec/v2 v2.5.1
	github.com/btcsuite/btcd/btcutil/v2 v2.0.1
	github.com/btcsuite/btcd/chaincfg/v2 v2.0.1
	github.com/btcsuite/btcd/psbt/v2 v2.0.1
	github.com/btcsuite/btcd/txscript/v2 v2.0.1
	github.com/btcsuite/btcd/wire/v2 v2.0.1
)

require (
	github.com/btcsuite/btcd/chainhash/v2 v2.0.0 // indirect
	github.com/btcsuite/btclog v1.0.0 // indirect
	github.com/decred/dcrd/crypto/blake256 v1.1.0 // indirect
	github.com/decred/dcrd/dcrec/secp256k1/v4 v4.4.0 // indirect
	github.com/kcalvinalvin/anet v0.0.0-20251112173137-d8ddc1f6dbee // indirect
	golang.org/x/crypto v0.40.0 // indirect
	golang.org/x/sys v0.35.0 // indirect
)

replace github.com/btcsuite/btcd => github.com/bitcoinfuzz/btcd v0.0.0-20260522174054-81e7e80555dc

replace github.com/btcsuite/btcd/address/v2 => github.com/bitcoinfuzz/btcd/address/v2 v2.0.1

replace github.com/btcsuite/btcd/btcec/v2 => github.com/bitcoinfuzz/btcd/btcec/v2 v2.5.1

replace github.com/btcsuite/btcd/btcutil/v2 => github.com/bitcoinfuzz/btcd/btcutil/v2 v2.0.1

replace github.com/btcsuite/btcd/chaincfg/v2 => github.com/bitcoinfuzz/btcd/chaincfg/v2 v2.0.1

replace github.com/btcsuite/btcd/chainhash/v2 => github.com/bitcoinfuzz/btcd/chainhash/v2 v2.0.0

replace github.com/btcsuite/btcd/psbt/v2 => github.com/bitcoinfuzz/btcd/psbt/v2 v2.0.1

replace github.com/btcsuite/btcd/txscript/v2 => github.com/bitcoinfuzz/btcd/txscript/v2 v2.0.1

replace github.com/btcsuite/btcd/wire/v2 => github.com/bitcoinfuzz/btcd/wire/v2 v2.0.1
