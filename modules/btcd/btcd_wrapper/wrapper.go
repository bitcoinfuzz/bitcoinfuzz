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
	"log"
	"math/big"
	"unsafe"

	"github.com/btcsuite/btcd/blockchain"
	"github.com/btcsuite/btcd/btcutil"
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

	return C.CString("true")
}

func main() {}
