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
	"log"
	"math/big"
	"strings"
	"unsafe"

	"github.com/btcsuite/btcd/addrmgr"
	"github.com/btcsuite/btcd/blockchain"
	"github.com/btcsuite/btcd/btcutil"
	"github.com/btcsuite/btcd/btcutil/psbt"
	"github.com/btcsuite/btcd/chaincfg"
	"github.com/btcsuite/btcd/txscript"
	"github.com/btcsuite/btcd/wire"
)

//export BTCDEvalScript
func BTCDEvalScript(scriptData C.ByteArray, flags C.uint32_t) C.int {
	script := C.GoBytes(unsafe.Pointer(scriptData.data), scriptData.length)

	// Create empty transaction for context
	tx := wire.NewMsgTx(wire.TxVersion)

	vm, err := txscript.NewEngine(
		script,                      // script
		tx,                          // transaction
		0,                           // input index
		txscript.ScriptFlags(flags), // flags
		nil,                         // sig cache
		nil,                         // hash cache
		0,                           // amount
		txscript.NewCannedPrevOutputFetcher(nil, 0), // prevOutput fetcher
	)
	if err != nil {
		log.Printf("Engine error: %v", err)
		return 0
	}

	if err := vm.EvalScript(); err != nil {
		log.Printf("Execution error: %v", err)
		return 0
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
		println("%v", err.Error())
		return C.CString("0")
	}

	return C.CString(tx.WitnessHash().String())
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

func main() {}
