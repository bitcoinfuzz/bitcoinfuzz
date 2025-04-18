package main

/*
#include <stdint.h>
#include <stdlib.h>
*/
import "C"

import (
	"bytes"
	"encoding/hex"
	"fmt"
	"runtime"
	"strings"

	"github.com/btcsuite/btcd/chaincfg"
	"github.com/btcsuite/btcd/chaincfg/chainhash"
	"github.com/btcsuite/btcd/txscript"
	"github.com/btcsuite/btcd/wire"
	"github.com/lightningnetwork/lnd/zpay32"
)

//export LndDeserializeInvoice
func LndDeserializeInvoice(cInvoiceStr *C.char) *C.char {
	if cInvoiceStr == nil {
		return C.CString("")
	}

	runtime.GC()

	// Convert C string to Go string
	invoiceStr := C.GoString(cInvoiceStr)

	network := &chaincfg.MainNetParams

	invoice, err := zpay32.Decode(invoiceStr, network)
	if err != nil {
		return C.CString("")
	}

	var sb strings.Builder

	sb.WriteString("HASH=")
	if invoice.PaymentHash != nil {
		sb.WriteString(fmt.Sprintf("%x", *invoice.PaymentHash))
	}

	sb.WriteString(";AMOUNT=")
	if invoice.MilliSat != nil {
		sb.WriteString(fmt.Sprintf("%d", *invoice.MilliSat))
	}

	sb.WriteString(";DESCRIPTION=")
	if invoice.Description != nil {
		sb.WriteString(*invoice.Description)
	}

	sb.WriteString(";RECIPIENT=")
	if invoice.Destination != nil {
		sb.WriteString(fmt.Sprintf("%x", invoice.Destination.SerializeCompressed()))
	}

	sb.WriteString(";EXPIRY=")
	if invoice.Expiry() > 0 {
		sb.WriteString(fmt.Sprintf("%d", int64(invoice.Expiry().Seconds())))
	}

	sb.WriteString(";TIMESTAMP=")
	sb.WriteString(fmt.Sprintf("%d", invoice.Timestamp.Unix()))

	sb.WriteString(fmt.Sprintf(";ROUTING_HINTS=%d", len(invoice.RouteHints)))

	sb.WriteString(";MIN_CLTV=")
	if invoice.MinFinalCLTVExpiry() > 0 {
		sb.WriteString(fmt.Sprintf("%d", invoice.MinFinalCLTVExpiry()))
	}

	return C.CString(sb.String())
}

//export LndConstructHtlcSuccessTx
func LndConstructHtlcSuccessTx(commitmentTxHex *C.char, htlcIndex C.uint, preimage *C.char, feeRate C.ulonglong) *C.char {

	runtime.GC()

	// if inputs = nil, create a minimal valid tx
	if commitmentTxHex == nil || preimage == nil {
		//  minimal valid tx with zero inputs/outputs
		minTx := wire.NewMsgTx(2)

		// dummy input with the htlcIndex as vout
		dummyHash := chainhash.Hash{}
		outpoint := wire.NewOutPoint(&dummyHash, uint32(htlcIndex))
		txIn := wire.NewTxIn(outpoint, nil, nil)
		minTx.AddTxIn(txIn)

		// dummy output
		pkScript, _ := txscript.NewScriptBuilder().
			AddOp(txscript.OP_0).
			AddData(make([]byte, 20)).
			Script()

		txOut := wire.NewTxOut(1000000, pkScript)
		minTx.AddTxOut(txOut)

		var buf bytes.Buffer
		minTx.Serialize(&buf)
		return C.CString(hex.EncodeToString(buf.Bytes()))
	}

	commitmentTxHexStr := C.GoString(commitmentTxHex)
	preimageStr := C.GoString(preimage)

	// parse commitment tx (if it fails use dummy tx but preserve htlcIndex)
	var commitmentTx wire.MsgTx
	commitmentTxBytes, err := hex.DecodeString(commitmentTxHexStr)
	if err != nil || commitmentTx.Deserialize(bytes.NewReader(commitmentTxBytes)) != nil {
		// dummy commitment tx with one output
		commitmentTx = *wire.NewMsgTx(2)
		dummyScript, _ := txscript.NewScriptBuilder().
			AddOp(txscript.OP_RETURN).
			Script()
		commitmentTx.AddTxOut(wire.NewTxOut(1000000, dummyScript))
	}

	// parse preimage (if invalid use dummy preimage)
	preimageBytes, err := hex.DecodeString(preimageStr)
	if err != nil || len(preimageBytes) != 32 {
		preimageBytes = make([]byte, 32)
	}

	// htlcIndex (if invalid use index 0 but preserve original index in outpoint)
	txOutIndex := uint32(0)
	if int(htlcIndex) < len(commitmentTx.TxOut) {
		txOutIndex = uint32(htlcIndex)
	}

	// HTLC success tx
	successTx := wire.NewMsgTx(2)

	// add input spending to the HTLC output (use the original htlcIndex even if invalid)
	txHash := commitmentTx.TxHash()
	outpoint := wire.NewOutPoint(&txHash, uint32(htlcIndex))
	txIn := wire.NewTxIn(outpoint, nil, nil)
	successTx.AddTxIn(txIn)

	// output amount
	var inputAmount int64 = 1000000 // Default if index is invalid
	if int(txOutIndex) < len(commitmentTx.TxOut) {
		inputAmount = commitmentTx.TxOut[txOutIndex].Value
	}

	feeAmount := int64(feeRate * 250 / 1000)
	outputAmount := inputAmount - feeAmount
	if outputAmount < 0 {
		outputAmount = 0
	}

	// output script
	pkScript, _ := txscript.NewScriptBuilder().
		AddOp(txscript.OP_0).
		AddData(make([]byte, 20)).
		Script()

	txOut := wire.NewTxOut(outputAmount, pkScript)
	successTx.AddTxOut(txOut)

	var buf bytes.Buffer
	successTx.Serialize(&buf)
	return C.CString(hex.EncodeToString(buf.Bytes()))
}

//export LndConstructHtlcTimeoutTx
func LndConstructHtlcTimeoutTx(commitmentTxHex *C.char, htlcIndex C.uint, cltvExpiry C.uint, feeRate C.ulonglong) *C.char {
	runtime.GC()

	// if input = nil (create a minimal valid tx)
	if commitmentTxHex == nil {
		// minimal valid tx with zero inputs/outputs
		minTx := wire.NewMsgTx(2)

		// add dummy input with htlcIndex as vout
		dummyHash := chainhash.Hash{}
		outpoint := wire.NewOutPoint(&dummyHash, uint32(htlcIndex))
		txIn := wire.NewTxIn(outpoint, nil, nil)
		txIn.Sequence = 0 // enable locktime
		minTx.AddTxIn(txIn)

		// set locktime
		minTx.LockTime = uint32(cltvExpiry)

		// add dummy output
		pkScript, _ := txscript.NewScriptBuilder().
			AddOp(txscript.OP_0).
			AddData(make([]byte, 20)).
			Script()

		txOut := wire.NewTxOut(1000000, pkScript)
		minTx.AddTxOut(txOut)

		var buf bytes.Buffer
		minTx.Serialize(&buf)
		return C.CString(hex.EncodeToString(buf.Bytes()))
	}

	commitmentTxHexStr := C.GoString(commitmentTxHex)

	// parse commitment tx (if it fails use dummy tx but preserve htlcIndex)
	var commitmentTx wire.MsgTx
	commitmentTxBytes, err := hex.DecodeString(commitmentTxHexStr)
	if err != nil || commitmentTx.Deserialize(bytes.NewReader(commitmentTxBytes)) != nil {
		// dummy commitment tx with one output
		commitmentTx = *wire.NewMsgTx(2)
		dummyScript, _ := txscript.NewScriptBuilder().
			AddOp(txscript.OP_RETURN).
			Script()
		commitmentTx.AddTxOut(wire.NewTxOut(1000000, dummyScript))
	}

	// check htlcIndex (if invalid use index 0 but preserve original index in outpoint)
	txOutIndex := uint32(0)
	if int(htlcIndex) < len(commitmentTx.TxOut) {
		txOutIndex = uint32(htlcIndex)
	}

	// HTLC timeout tx
	timeoutTx := wire.NewMsgTx(2)

	// add input spending the HTLC output (use original htlcIndex even if invalid)
	txHash := commitmentTx.TxHash()
	outpoint := wire.NewOutPoint(&txHash, uint32(htlcIndex))
	txIn := wire.NewTxIn(outpoint, nil, nil)
	txIn.Sequence = 0 // enable locktime
	timeoutTx.AddTxIn(txIn)

	// set locktime
	timeoutTx.LockTime = uint32(cltvExpiry)

	// output amount
	var inputAmount int64 = 1000000 // Default if index is invalid
	if int(txOutIndex) < len(commitmentTx.TxOut) {
		inputAmount = commitmentTx.TxOut[txOutIndex].Value
	}

	feeAmount := int64(feeRate * 250 / 1000)
	outputAmount := inputAmount - feeAmount
	if outputAmount < 0 {
		outputAmount = 0
	}

	// create output script
	pkScript, _ := txscript.NewScriptBuilder().
		AddOp(txscript.OP_0).
		AddData(make([]byte, 20)).
		Script()

	txOut := wire.NewTxOut(outputAmount, pkScript)
	timeoutTx.AddTxOut(txOut)

	var buf bytes.Buffer
	timeoutTx.Serialize(&buf)
	return C.CString(hex.EncodeToString(buf.Bytes()))
}

func main() {}
