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
	"encoding/binary"
	"encoding/hex"
	"unsafe"

	"github.com/utreexo/utreexo"
)

//export UtreexoStumpUpdate
func UtreexoStumpUpdate(newTxouts C.ByteArray) *C.char {
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
		return C.CString("")
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

	return C.CString(hex.EncodeToString(stumpSer))
}

//export UtreexoVerify
func UtreexoVerify(buffer *C.char) *C.char {
	panic("unimplemented")
}

func main() {}
