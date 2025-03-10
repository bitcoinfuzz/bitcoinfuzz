package main

/*
#include <stdint.h>
#include <stdlib.h>
*/
import "C"

import (
	"runtime"

	"github.com/btcsuite/btcd/chaincfg"
	"github.com/lightningnetwork/lnd/zpay32"
)

//export LndDeserializeInvoice
func LndDeserializeInvoice(cInvoiceStr *C.char) C.int {
	if cInvoiceStr == nil {
		return 0
	}

	runtime.GC()

	// Convert C string to Go string
	invoiceStr := C.GoString(cInvoiceStr)

	network := &chaincfg.MainNetParams

	_, err := zpay32.Decode(invoiceStr, network)
	if err != nil {
		return 0
	}

	runtime.GC()

	return 1
}

func main() {}
