package main

/*
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    char* data;
    int length;
} ByteArray;
*/
import "C"
import (
	"bytes"
	"encoding/base64"
	"fmt"
	"math/big"
	"strconv"
	"strings"
	"unsafe"

	"github.com/btcsuite/btcd/addrmgr"
	"github.com/btcsuite/btcd/blockchain"
	"github.com/btcsuite/btcd/btcutil"
	"github.com/btcsuite/btcd/btcutil/hdkeychain"
	"github.com/btcsuite/btcd/btcutil/psbt"
	"github.com/btcsuite/btcd/chaincfg"
	"github.com/btcsuite/btcd/txscript"
	"github.com/btcsuite/btcd/wire"
)

//export BTCDEvalScript
func BTCDEvalScript(scriptData C.ByteArray, flags C.uint32_t, version C.size_t) C.int {
	script := C.GoBytes(unsafe.Pointer(scriptData.data), C.int(scriptData.length))
	if len(script) == 0 {
		return 0
	}

	/*
		if disasm, err := txscript.DisasmString(script); err == nil {
			log.Printf("disasm: %s", disasm)
			}*/

	// Build a minimal tx so input index 0 is valid
	tx := wire.NewMsgTx(wire.TxVersion)
	tx.AddTxIn(&wire.TxIn{
		PreviousOutPoint: wire.OutPoint{}, // dummy
	})
	tx.AddTxOut(&wire.TxOut{Value: 0})

	// Provide a prevout amount — necessary if the fuzzed flags enable witness verification.
	// Use a small nonzero amount to be safe.
	prevoutAmt := int64(1000)

	// Create a canned fetcher that returns this pkScript+amount for the prevout
	fetcher := txscript.NewCannedPrevOutputFetcher(script, prevoutAmt)

	// If version != 0 in Core, they set SigVersion::WITNESS_V0. For btcd Engine we supply the amount
	// and the fetcher which covers segwit needs. We rely on Engine to follow flags.
	vm, err := txscript.NewEngine(
		script,                              // pkScript being evaluated
		tx,                                  // tx context
		0,                                   // input index
		txscript.ScriptFlags(uint32(flags)), // flags from fuzzer
		nil,                                 // sig cache
		nil,                                 // hash cache
		prevoutAmt,                          // amount (for segwit)
		fetcher,                             // prevout fetcher
	)
	if err != nil {
		return 2
	}

	if err := vm.EvalScript(); err != nil {
		return 2
	}

	return 1
}

//export BTCDParseP2PMessage
func BTCDParseP2PMessage(messageData C.ByteArray) *C.char {
	data := C.GoBytes(unsafe.Pointer(messageData.data), messageData.length)
	reader := bytes.NewReader(data)

	_, msg, _, err := wire.ReadMessageN(reader, 70016, wire.MainNet)
	if err != nil {
		return C.CString("0")
	}

	return C.CString(msg.Command())
}

//export BTCDAddrv2
func BTCDAddrv2(addrv2Data C.ByteArray) *C.char {
	data := C.GoBytes(unsafe.Pointer(addrv2Data.data), addrv2Data.length)
	r := bytes.NewReader(data)
	m := &wire.MsgAddrV2{}
	err := m.BtcDecode(r, 0, wire.WitnessEncoding)

	clearnet_count := 0
	tor_count := 0
	cjdns_count := 0
	i2p_count := 0

	for i := 0; i < len(m.AddrList); i++ {
		if m.AddrList[i].Addr != nil {
			if !addrmgr.IsRoutable(m.AddrList[i]) {
				return C.CString("clearnet=0tor=0cjdns=0i2p=0")
			}
			switch m.AddrList[i].Addr.Network() {
			case string(1):
				clearnet_count += 1
			case string(2):
				clearnet_count += 1
			case string(3):
				//Bitcoin Core does not support torv2 anymore
				tor_count += 0
			case string(4):
				tor_count += 1
			case string(5):
				i2p_count += 1
			case string(6):
				cjdns_count += 1
			}
		}
	}

	if err != nil {
		return C.CString("clearnet=0tor=0cjdns=0i2p=0")
	}

	return C.CString(fmt.Sprintf("clearnet=%dtor=%dcjdns=%di2p=%d",
		clearnet_count, tor_count, cjdns_count, i2p_count))
}

//export BTCDScriptAsm
func BTCDScriptAsm(scriptData C.ByteArray) *C.char {
	script := C.GoBytes(unsafe.Pointer(scriptData.data), scriptData.length)
	disasm, err := txscript.DisasmString(script)
	if err != nil {
		return C.CString("")
	}
	return C.CString(disasm)
}

//export BTCDDesBlock
func BTCDDesBlock(scriptData C.ByteArray) *C.char {
	buffer := C.GoBytes(unsafe.Pointer(scriptData.data), scriptData.length)

	block, err := btcutil.NewBlockFromBytes(buffer)
	if err != nil {
		return C.CString("0")
	}

	// Easiest possible PoW
	powLimit := new(big.Int).Exp(big.NewInt(2), big.NewInt(256), nil)
	err = blockchain.CheckBlockSanity(block, powLimit, blockchain.NewMedianTime())
	if err != nil {
		return C.CString("0")
	}

	err = blockchain.ValidateWitnessCommitment(block)
	if err != nil {
		return C.CString("0")
	}

	return C.CString(block.Hash().String())
}

//export BTCDFreeString
func BTCDFreeString(ptr *C.char) {
	C.free(unsafe.Pointer(ptr))
}

//export BTCDTransactionEval
func BTCDTransactionEval(data C.ByteArray) *C.char {
	buffer := C.GoBytes(unsafe.Pointer(data.data), data.length)
	tx, err := btcutil.NewTxFromBytes(buffer)
	if err != nil {
		return C.CString("0")
	}

	err_sanity := blockchain.CheckTransactionSanity(tx)
	if err_sanity != nil {
		return C.CString("0")
	}

	res := tx.WitnessHash().String()
	res += strconv.Itoa(tx.MsgTx().SerializeSize())
	return C.CString(res)
}

//export BTCDParsePSBT
func BTCDParsePSBT(data C.ByteArray) *C.char {
	buffer := C.GoBytes(unsafe.Pointer(data.data), data.length)

	var packet *psbt.Packet
	var err error

	packet, err = psbt.NewFromRawBytes(bytes.NewBuffer(buffer), false)

	if err != nil { // base64 if binary fails
		str := string(buffer)
		decodedBytes, decodeErr := base64.StdEncoding.DecodeString(str)
		if decodeErr != nil {
			return nil
		}
		packet, err = psbt.NewFromRawBytes(bytes.NewBuffer(decodedBytes), false)
		if err != nil {
			return nil
		}
	}

	var result strings.Builder // format psbt similar to rust_bitcoin

	//result.WriteString(fmt.Sprintf("v=%d;", packet.UnsignedTx.Version))      // add Tx ver
	result.WriteString(fmt.Sprintf("lt=%d;", packet.UnsignedTx.LockTime))    // add locktime
	result.WriteString(fmt.Sprintf("in=%d;", len(packet.UnsignedTx.TxIn)))   // add ip count
	result.WriteString(fmt.Sprintf("out=%d;", len(packet.UnsignedTx.TxOut))) // add op count

	// processing ip
	for i, txIn := range packet.UnsignedTx.TxIn {
		if i < len(packet.Inputs) {
			// prev op (txid:vout)
			result.WriteString(fmt.Sprintf("in%dprev=%s:%d;",
				i, txIn.PreviousOutPoint.Hash.String(), txIn.PreviousOutPoint.Index))

			// seq number
			result.WriteString(fmt.Sprintf("in%dseq=%d;", i, txIn.Sequence))

			// check UTXO info
			if packet.Inputs[i].WitnessUtxo != nil || packet.Inputs[i].NonWitnessUtxo != nil {
				result.WriteString(fmt.Sprintf("in%dutxo=1;", i))
			}

			// count partial sig
			sigCount := len(packet.Inputs[i].PartialSigs)
			result.WriteString(fmt.Sprintf("in%dsigs=%d;", i, sigCount))
		}
	}

	// processing op
	for i, txOut := range packet.UnsignedTx.TxOut {
		if i < len(packet.Outputs) {
			result.WriteString(fmt.Sprintf("out%dval=%d;", i, txOut.Value))
			scriptHex := fmt.Sprintf("%x", txOut.PkScript) // script pubkey as hex
			result.WriteString(fmt.Sprintf("out%dscript=%s;", i, scriptHex))
		}
	}

	return C.CString(result.String())
}

//export BTCDAddress
func BTCDAddress(data C.ByteArray) *C.char {

	addrBytes := C.GoBytes(unsafe.Pointer(data.data), data.length)
	addrStr := string(addrBytes)

	addr, err := btcutil.DecodeAddress(addrStr, &chaincfg.MainNetParams)
	if err != nil {
		return C.CString("INVALID")
	}

	var prefix string
	switch addr.(type) {
	case *btcutil.AddressPubKeyHash:
		prefix = "PKH:"
	case *btcutil.AddressScriptHash:
		prefix = "SH:"
	case *btcutil.AddressWitnessPubKeyHash:
		prefix = "WPKH:"
	case *btcutil.AddressWitnessScriptHash:
		prefix = "WSH:"
	case *btcutil.AddressTaproot:
		prefix = "TR:"
	default:
		prefix = "UNK:"
	}

	return C.CString(prefix + addr.EncodeAddress())
}

//export BTCDBip32MasterKeygen
func BTCDBip32MasterKeygen(data C.ByteArray) *C.char {
	seed := C.GoBytes(unsafe.Pointer(data.data), data.length)
	masterKey, err := hdkeychain.NewMaster(seed, &chaincfg.MainNetParams)
	if err != nil {
		// Skip seed length validation errors (128-512 bits requirement) as other
		// implementations don't enforce this constraint.
		if err.Error() == "seed length must be between 128 and 512 bits" {
			return nil
		}
		return C.CString("")
	}
	return C.CString(masterKey.String())
}
func main() {}
