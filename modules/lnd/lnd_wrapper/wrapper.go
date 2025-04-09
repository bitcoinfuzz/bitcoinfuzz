package main

/*
#include <stdint.h>
#include <stdlib.h>
*/
import "C"

import (
	"bytes"
	"fmt"
	"runtime"
	"strings"
	"unsafe"

	"github.com/btcsuite/btcd/chaincfg"
	"github.com/lightningnetwork/lnd/lnwire"
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

//export LNDParseChannelAnnouncement
func LNDParseChannelAnnouncement(data unsafe.Pointer, length C.int) *C.char {
	// Convert C data to Go slice
	dataSlice := C.GoBytes(data, length)
	if len(dataSlice) == 0 {
		return C.CString("")
	}

	// Create a bytes buffer and try to decode the channel announcement
	r := bytes.NewReader(dataSlice)

	// decode as ChannelAnnouncement1 (legacy ver)
	var msg1 lnwire.ChannelAnnouncement1
	if err := msg1.Decode(r, 0); err != nil {
		return C.CString("")
	}

	result := fmt.Sprintf(
		"NODE1=%x;NODE2=%x;BITCOIN_KEY1=%x;BITCOIN_KEY2=%x;SHORT_CHAN_ID=%v;CHAIN_HASH=%x;FEATURES=%v;",
		msg1.NodeID1,
		msg1.NodeID2,
		msg1.BitcoinKey1,
		msg1.BitcoinKey2,
		msg1.ShortChannelID,
		msg1.ChainHash[:],
		msg1.Features,
	)
	// [TODO!] ChannelAnnouncement2

	return C.CString(result)
}

func main() {}
