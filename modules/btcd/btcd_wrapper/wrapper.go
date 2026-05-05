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
	"net"
	"reflect"
	"strconv"
	"strings"
	"unsafe"

	"encoding/hex"

	"github.com/btcsuite/btcd/addrmgr"
	"github.com/btcsuite/btcd/blockchain"
	"github.com/btcsuite/btcd/btcec/v2"
	"github.com/btcsuite/btcd/btcec/v2/ellswift"
	"github.com/btcsuite/btcd/btcec/v2/schnorr"
	"github.com/btcsuite/btcd/btcutil"
	"github.com/btcsuite/btcd/btcutil/hdkeychain"
	"github.com/btcsuite/btcd/btcutil/psbt"
	"github.com/btcsuite/btcd/chaincfg"
	"github.com/btcsuite/btcd/txscript"
	"github.com/btcsuite/btcd/wire"
)

//export BTCDVerifyScript
func BTCDVerifyScript(scriptSig C.ByteArray, scriptPubKey C.ByteArray) C.int {
	script_sig := C.GoBytes(unsafe.Pointer(scriptSig.data), C.int(scriptSig.length))
	if len(script_sig) == 0 {
		return 0
	}

	script_pubkey := C.GoBytes(unsafe.Pointer(scriptPubKey.data), C.int(scriptPubKey.length))
	if len(script_pubkey) == 0 {
		return 0
	}

	tx := wire.NewMsgTx(wire.TxVersion)
	txIn := wire.NewTxIn(&wire.OutPoint{}, nil, nil)
	txIn.SignatureScript = script_sig
	tx.AddTxIn(txIn)

	prevoutAmt := int64(1000)
	fetcher := txscript.NewCannedPrevOutputFetcher(script_pubkey, prevoutAmt)
	vm, err := txscript.NewEngine(
		script_pubkey,
		tx,
		0,   // input index
		0,   // flags
		nil, // sigCache
		nil, // hashCache (TxSigHashes)
		prevoutAmt,
		fetcher,
	)

	if err != nil {
		return 2
	}

	if err := vm.Execute(); err != nil {
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

// getAddrBytes extracts raw address bytes from a net.Addr using reflection
// since btcd's address types are unexported
func getAddrBytes(addr net.Addr) []byte {
	v := reflect.ValueOf(addr)
	if v.Kind() == reflect.Ptr {
		v = v.Elem()
	}
	addrField := v.FieldByName("addr")
	if !addrField.IsValid() {
		return nil
	}
	// Get the underlying bytes from the array
	length := addrField.Len()
	result := make([]byte, length)
	for i := 0; i < length; i++ {
		result[i] = byte(addrField.Index(i).Uint())
	}
	return result
}

// getAddrType determines the address type from a net.Addr using reflection
// to inspect the underlying type name since btcd's address types are unexported
func getAddrType(addr net.Addr) string {
	typeName := reflect.TypeOf(addr).String()
	switch {
	case strings.Contains(typeName, "ipv4Addr"):
		return "ipv4"
	case strings.Contains(typeName, "ipv6Addr"):
		return "ipv6"
	case strings.Contains(typeName, "torv3Addr"):
		return "tor"
	case strings.Contains(typeName, "torv2Addr"):
		return "torv2"
	case strings.Contains(typeName, "i2pAddr"):
		return "i2p"
	case strings.Contains(typeName, "cjdnsAddr"):
		return "cjdns"
	default:
		return ""
	}
}

//export BTCDAddrv2
func BTCDAddrv2(addrv2Data C.ByteArray) *C.char {
	data := C.GoBytes(unsafe.Pointer(addrv2Data.data), addrv2Data.length)
	r := bytes.NewReader(data)
	m := &wire.MsgAddrV2{}
	err := m.BtcDecode(r, 0, wire.WitnessEncoding)
	if err != nil {
		// BTCD parses TorV2, so it may return an error for an invalid address size
		// of a TorV2, which other implementations would not throw.
		if err == wire.ErrInvalidAddressSize {
			return nil
		}
		return C.CString("[]")
	}

	// IPv4-mapped IPv6 prefix (::ffff:0:0/96) - RFC 4291
	ipv4MappedPrefix := []byte{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF}

	var entries []string
	for i := 0; i < len(m.AddrList); i++ {
		if m.AddrList[i].Addr != nil {
			addr := m.AddrList[i].Addr
			addrBytes := getAddrBytes(addr)
			if addrBytes == nil {
				continue
			}
			addrHex := hex.EncodeToString(addrBytes)
			addrType := getAddrType(addr)
			if !addrmgr.IsRoutable(m.AddrList[i]) {
				continue
			}
			if addrType == "torv2" {
				continue
			}

			// These verification should be in BTCD library.
			if addrType == "ipv6" {
				// Skip IPv4-mapped IPv6 addresses (RFC 4291)
				// These should use network ID 0x01 (IPv4), not 0x02 (IPv6)
				if len(addrBytes) >= 12 && bytes.HasPrefix(addrBytes, ipv4MappedPrefix) {
					continue
				}

				// RFC 7343 - ORCHIDv2 - 2001:20::/28
				if addrBytes[0] == 0x20 && addrBytes[1] == 0x01 &&
					addrBytes[2] == 0x00 && (addrBytes[3]&0xf0) == 0x20 {
					continue
				}
			}

			time := m.AddrList[i].Timestamp.Unix()
			services := m.AddrList[i].Services
			port := m.AddrList[i].Port

			entry := fmt.Sprintf("{\"addr\":\"%s\",\"type\":\"%s\",\"time\":\"%d\",\"services\":\"%d\",\"port\":\"%d\"}",
				addrHex, addrType, time, services, port)
			entries = append(entries, entry)
		}
	}

	return C.CString("[" + strings.Join(entries, ",") + "]")
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

//export BTCDSignSchnorr
func BTCDSignSchnorr(privKey C.ByteArray, hash C.ByteArray, aux C.ByteArray) *C.char {
	privKeyBytes := C.GoBytes(unsafe.Pointer(privKey.data), privKey.length)
	hashBytes := C.GoBytes(unsafe.Pointer(hash.data), hash.length)
	auxBytes := C.GoBytes(unsafe.Pointer(aux.data), aux.length)

	// Ensure we have exactly 32 bytes for the aux data
	var auxArray [32]byte
	copy(auxArray[:], auxBytes)

	priv, _ := btcec.PrivKeyFromBytes(privKeyBytes)

	// Use CustomNonce to force determinism with the provided aux data
	sig, err := schnorr.Sign(priv, hashBytes, schnorr.CustomNonce(auxArray))
	if err != nil {
		return C.CString("")
	}

	return C.CString(hex.EncodeToString(sig.Serialize()))
}

//export BTCDDecodeEllswift
func BTCDDecodeEllswift(buffer C.ByteArray) *C.char {
	ell64 := C.GoBytes(unsafe.Pointer(buffer.data), buffer.length)
	u := new(btcec.FieldVal)
	t := new(btcec.FieldVal)
	u.SetBytes((*[32]byte)(ell64[0:32]))
	t.SetBytes((*[32]byte)(ell64[32:64]))

	tIsOdd := t.Normalize().IsOdd()

	x, err := ellswift.XSwiftEC(u, t)
	if err != nil {
		panic(fmt.Sprintf("XSwiftEC failed unexpectedly: %v", err))
	}

	ySqr := new(btcec.FieldVal).SquareVal(x).Mul(x).AddInt(7)
	y := new(btcec.FieldVal)
	if !y.SquareRootVal(ySqr) {
		panic("SquareRootVal failed: x from XSwiftEC should always be on curve")
	}

	// y parity should match original t parity
	if y.IsOdd() != tIsOdd {
		y.Negate(1).Normalize()
	}
	pubkey := btcec.NewPublicKey(x, y)

	return C.CString(hex.EncodeToString(pubkey.SerializeCompressed()))
}

//export BTCDRoundtripEllswift
func BTCDRoundtripEllswift(buffer C.ByteArray) *C.char {
	privKeyBytes := C.GoBytes(unsafe.Pointer(buffer.data), buffer.length)

	_, pub := btcec.PrivKeyFromBytes(privKeyBytes)

	compressed := pub.SerializeCompressed()
	var xBytes [32]byte
	copy(xBytes[:], compressed[1:33])

	x := new(btcec.FieldVal)
	x.SetBytes(&xBytes)

	u, t, err := ellswift.XElligatorSwift(x)
	if err != nil {
		return C.CString("")
	}

	tIsOdd := t.Normalize().IsOdd()

	decodedX, err := ellswift.XSwiftEC(u, t)
	if err != nil {
		panic(fmt.Sprintf("XSwiftEC failed unexpectedly: %v", err))
	}

	ySqr := new(btcec.FieldVal).SquareVal(decodedX).Mul(decodedX).AddInt(7)
	y := new(btcec.FieldVal)
	if !y.SquareRootVal(ySqr) {
		panic("SquareRootVal failed: x from XElligatorSwift should always be on curve")
	}

	if y.IsOdd() != tIsOdd {
		y.Negate(1).Normalize()
	}
	pubkey := btcec.NewPublicKey(decodedX, y)

	return C.CString(hex.EncodeToString(pubkey.SerializeCompressed()[1:]))
}

//export BTCDSchnorrVerify
func BTCDSchnorrVerify(buffer C.ByteArray, hash C.ByteArray, sig C.ByteArray) *C.char {
	privkeyBytes := C.GoBytes(unsafe.Pointer(buffer.data), buffer.length)
	hashBytes := C.GoBytes(unsafe.Pointer(hash.data), hash.length)
	sigBytes := C.GoBytes(unsafe.Pointer(sig.data), sig.length)

	_, pubkey := btcec.PrivKeyFromBytes(privkeyBytes)

	signature, err := schnorr.ParseSignature(sigBytes)
	if err != nil {
		return C.CString("INVALID")
	}

	if !signature.Verify(hashBytes, pubkey) {
		return C.CString("INVALID")
	}
	return C.CString("VALID")
}

func main() {}
