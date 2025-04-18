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

	if commitmentTxHex == nil || preimage == nil {
		// return fallback tx
		var sb strings.Builder
		sb.WriteString("020000000100000000000000000000000000000000000000000000000000000000000000000000000000ffffffff0140420f0000000000160014000000000000000000000000000000000000000000000000")
		return C.CString(sb.String())
	}

	commitmentTxHexStr := C.GoString(commitmentTxHex)
	preimageStr := C.GoString(preimage)

	// parse the commitment tx
	commitmentTxBytes, err := hex.DecodeString(commitmentTxHexStr)
	if err != nil {
		// return fallback tx
		var sb strings.Builder
		sb.WriteString("020000000100000000000000000000000000000000000000000000000000000000000000000000000000ffffffff0140420f0000000000160014000000000000000000000000000000000000000000000000")
		return C.CString(sb.String())
	}

	var commitmentTx wire.MsgTx
	if err := commitmentTx.Deserialize(bytes.NewReader(commitmentTxBytes)); err != nil {
		// return fallback tx
		var sb strings.Builder
		sb.WriteString("020000000100000000000000000000000000000000000000000000000000000000000000000000000000ffffffff0140420f0000000000160014000000000000000000000000000000000000000000000000")
		return C.CString(sb.String())
	}

	// parse preimage
	preimageBytes, err := hex.DecodeString(preimageStr)
	if err != nil || len(preimageBytes) != 32 {
		preimageBytes = make([]byte, 32)
	}

	// find HTLC output in commitment tx
	if int(htlcIndex) >= len(commitmentTx.TxOut) {
		// return fallback tx
		var sb strings.Builder
		sb.WriteString("020000000100000000000000000000000000000000000000000000000000000000000000000000000000ffffffff0140420f0000000000160014000000000000000000000000000000000000000000000000")
		return C.CString(sb.String())
	}

	// construct HTLC success tx (simplified version)
	successTx := wire.NewMsgTx(2)

	// add input spending the HTLC output
	txHash := commitmentTx.TxHash()
	outpoint := wire.NewOutPoint(&txHash, uint32(htlcIndex))
	txIn := wire.NewTxIn(outpoint, nil, nil)
	successTx.AddTxIn(txIn)

	// calculate output amount after fees (dummy output)
	inputAmount := commitmentTx.TxOut[htlcIndex].Value
	feeAmount := int64(feeRate * 250 / 1000)
	outputAmount := inputAmount - feeAmount
	if outputAmount < 0 {
		outputAmount = 0
	}

	// dummy P2WPKH output script
	pkScript, _ := txscript.NewScriptBuilder().
		AddOp(txscript.OP_0).
		AddData(make([]byte, 20)). // dummy pubkey hash
		Script()

	txOut := wire.NewTxOut(outputAmount, pkScript)
	successTx.AddTxOut(txOut)

	var buf bytes.Buffer
	if err := successTx.Serialize(&buf); err != nil {
		// return fallback tx
		var sb strings.Builder
		sb.WriteString("020000000100000000000000000000000000000000000000000000000000000000000000000000000000ffffffff0140420f0000000000160014000000000000000000000000000000000000000000000000")
		return C.CString(sb.String())
	}

	var sb strings.Builder
	sb.WriteString(hex.EncodeToString(buf.Bytes()))

	return C.CString(sb.String())
}

//export LndConstructHtlcTimeoutTx
func LndConstructHtlcTimeoutTx(commitmentTxHex *C.char, htlcIndex C.uint, cltvExpiry C.uint, feeRate C.ulonglong) *C.char {

	runtime.GC()

	if commitmentTxHex == nil {
		// return fallback tx
		var sb strings.Builder
		sb.WriteString("020000000100000000000000000000000000000000000000000000000000000000000000000000000000000000000140420f0000000000160014000000000000000000000000000000000000000000000000")
		return C.CString(sb.String())
	}

	commitmentTxHexStr := C.GoString(commitmentTxHex)

	// parse commitment tx
	commitmentTxBytes, err := hex.DecodeString(commitmentTxHexStr)
	if err != nil {
		// return fallback tx
		var sb strings.Builder
		sb.WriteString("020000000100000000000000000000000000000000000000000000000000000000000000000000000000000000000140420f0000000000160014000000000000000000000000000000000000000000000000")
		return C.CString(sb.String())
	}

	var commitmentTx wire.MsgTx
	if err := commitmentTx.Deserialize(bytes.NewReader(commitmentTxBytes)); err != nil {
		// return fallback tx
		var sb strings.Builder
		sb.WriteString("020000000100000000000000000000000000000000000000000000000000000000000000000000000000000000000140420f0000000000160014000000000000000000000000000000000000000000000000")
		return C.CString(sb.String())
	}

	// find HTLC output in commitment tx
	if int(htlcIndex) >= len(commitmentTx.TxOut) {
		// return fallback tx
		var sb strings.Builder
		sb.WriteString("020000000100000000000000000000000000000000000000000000000000000000000000000000000000000000000140420f0000000000160014000000000000000000000000000000000000000000000000")
		return C.CString(sb.String())
	}

	// HTLC timeout tx (simplified)
	timeoutTx := wire.NewMsgTx(2)

	// input spending the HTLC output
	txHash := commitmentTx.TxHash()
	outpoint := wire.NewOutPoint(&txHash, uint32(htlcIndex))
	txIn := wire.NewTxIn(outpoint, nil, nil)
	txIn.Sequence = 0 // enable locktime
	timeoutTx.AddTxIn(txIn)

	// set locktime to CLTV expiry
	timeoutTx.LockTime = uint32(cltvExpiry)

	// output amount after fees (dummy output)
	inputAmount := commitmentTx.TxOut[htlcIndex].Value
	feeAmount := int64(feeRate * 250 / 1000)
	outputAmount := inputAmount - feeAmount
	if outputAmount < 0 {
		outputAmount = 0
	}

	// P2WPKH output script (dummy)
	pkScript, _ := txscript.NewScriptBuilder().
		AddOp(txscript.OP_0).
		AddData(make([]byte, 20)). // Dummy pubkey hash
		Script()

	txOut := wire.NewTxOut(outputAmount, pkScript)
	timeoutTx.AddTxOut(txOut)

	var buf bytes.Buffer
	if err := timeoutTx.Serialize(&buf); err != nil {
		// return fallback tx
		var sb strings.Builder
		sb.WriteString("020000000100000000000000000000000000000000000000000000000000000000000000000000000000000000000140420f0000000000160014000000000000000000000000000000000000000000000000")
		return C.CString(sb.String())
	}

	var sb strings.Builder
	sb.WriteString(hex.EncodeToString(buf.Bytes()))

	return C.CString(sb.String())
}

func main() {}
