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
	"unsafe"

	"github.com/piotrnar/gocoin/lib/btc"
	"github.com/piotrnar/gocoin/lib/script"
)

// GocoinEvalScript evaluates a Bitcoin script using gocoin's script engine.
// This is exported to C/C++ and will be called by the fuzzer.
//
// Parameters:
//   - scriptData: The raw script bytes to evaluate
//   - flags: Script verification flags (like VER_P2SH, VER_WITNESS, etc.)
//   - version: Signature version (0 = base, 1 = witness v0)
//
// Returns:
//   - 1 if script evaluation succeeded
//   - 0 if script is empty
//   - 2 if script evaluation failed
//
//export GocoinEvalScript
func GocoinEvalScript(scriptData C.ByteArray, flags C.uint32_t, version C.size_t) C.int {
	// Convert C byte array to Go slice
	scriptBytes := C.GoBytes(unsafe.Pointer(scriptData.data), C.int(scriptData.length))
	if len(scriptBytes) == 0 {
		return 0
	}

	// Create a minimal dummy transaction for script evaluation context
	tx := createDummyTransaction(scriptBytes)

	// Create the SigChecker which gocoin uses for script verification
	// Based on gocoin's lib/script package
	checker := &script.SigChecker{
		Tx:     tx,
		Idx:    0,    // We're checking input 0
		Amount: 1000, // Dummy amount (needed for witness verification)
	}

	// Call gocoin's EvalScript function directly
	var stack script.ScrStack
	var execdata btc.ScriptExecutionData
	result := script.EvalScript(scriptBytes, &stack, checker, uint32(flags), script.SIGVERSION_BASE, &execdata)

	if result {
		return 1
	}
	return 2
}

// createDummyTransaction creates a minimal transaction structure
// that gocoin needs for script evaluation context.
func createDummyTransaction(pkScript []byte) *btc.Tx {
	// Create a minimal transaction with one input and one output
	tx := new(btc.Tx)
	tx.Version = 1
	tx.Lock_time = 0

	// Create dummy hash (32 bytes of zeros)
	var dummyHash [32]byte

	// Add one input (the one we're "spending" with our script)
	tx.TxIn = make([]*btc.TxIn, 1)
	tx.TxIn[0] = &btc.TxIn{
		Input: btc.TxPrevOut{
			Hash: dummyHash, // [32]byte, not Uint256
			Vout: 0,
		},
		ScriptSig: []byte{}, // Empty signature script for now
		Sequence:  0xffffffff,
	}

	// Add one output
	tx.TxOut = make([]*btc.TxOut, 1)
	tx.TxOut[0] = &btc.TxOut{
		Value:     0,
		Pk_script: []byte{},
	}

	return tx
}

// GocoinFreeString frees a C string that was allocated by Go.
// Must be called to prevent memory leaks.
//
//export GocoinFreeString
func GocoinFreeString(ptr *C.char) {
	C.free(unsafe.Pointer(ptr))
}

// main is required for cgo but does nothing
func main() {}
