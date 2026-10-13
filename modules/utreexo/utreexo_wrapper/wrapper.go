package main

/*
#cgo CFLAGS: -I${SRCDIR}/../../../include
#include <stdint.h>

#include "bitcoinfuzz/ffi.h"

typedef struct {
    char* data;
    int length;
} ByteArray;
*/
import "C"

import (
	"encoding/binary"
	"encoding/hex"
	"unsafe"

	"github.com/utreexo/utreexo"
)

// bfOk and bfFail build the bf_result from include/bitcoinfuzz/ffi.h. C.CBytes
// copies the string straight into malloc'd memory, which the harness pairs with
// free.
func bfOk(s string) C.bf_result {
	return C.bf_result{
		status: C.BF_OK,
		data:   (*C.char)(C.CBytes(unsafe.Slice(unsafe.StringData(s), len(s)))),
		len:    C.size_t(len(s)),
	}
}

func bfFail() C.bf_result {
	return C.bf_result{status: C.BF_FAIL}
}

//export UtreexoStumpUpdate
func UtreexoStumpUpdate(newTxouts C.ByteArray) C.bf_result {
	count := int(newTxouts.length) / 32

	addHashes := make([]utreexo.Hash, count)
	if count > 0 {
		hashBytes := unsafe.Slice((*byte)(unsafe.Pointer(newTxouts.data)), newTxouts.length)
		for i := range count {
			var h utreexo.Hash
			copy(h[:], hashBytes[i*32:(i+1)*32])
			addHashes[i] = h
		}
	}

	var stump utreexo.Stump
	_, err := stump.Update([]utreexo.Hash{}, addHashes, utreexo.Proof{})
	if err != nil {
		return bfFail()
	}

	// Serialize the Stump into a hex string.
	//
	// NOTE: since `utreexo` does not implement `Stump.serialize`,
	// we just serialize this exactly like `rustreexo`.
	var stumpSer []byte

	leaves := make([]byte, 8)
	binary.LittleEndian.PutUint64(leaves, stump.NumLeaves)
	stumpSer = append(stumpSer, leaves...)
	rootCount := make([]byte, 8)
	binary.LittleEndian.PutUint64(rootCount, uint64(len(stump.Roots)))
	stumpSer = append(stumpSer, rootCount...)
	for _, root := range stump.Roots {
		stumpSer = append(stumpSer, 0x02)
		stumpSer = append(stumpSer, root[:]...)
	}

	return bfOk(hex.EncodeToString(stumpSer))
}

func main() {}
