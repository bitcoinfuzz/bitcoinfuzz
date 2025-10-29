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
	sphinx "github.com/lightningnetwork/lightning-onion"
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

	sb.WriteString(";PAYMENT_SECRET=")
	invoice.PaymentAddr.WhenSome(func(paymentAddr [32]byte) {
		sb.WriteString(fmt.Sprintf("%x", paymentAddr))
	})

	sb.WriteString(";AMOUNT=")
	if invoice.MilliSat != nil {
		// Simulate uint64 overflow behavior for compatibility with all implementations.
		// LND uses milliSatoshis (10^-11 BTC) and does not overflow when handling very large values.
		// However, other implementations like LDK, which use picoBTC (10^-12 BTC), may overflow when parsing
		// extremely large amounts (e.g., >95M BTC). To simulate this behavior, we multiply and divide by 10.
		// This cancels out the value mathematically but mimics the overflow behavior that would occur in those
		// implementations. This is especially relevant for values exceeding Bitcoin’s 21M BTC cap—
		// which should be considered invalid—but must be handled in a consistent way across implementations
		// to avoid parsing errors.
		amountOverflow := (*invoice.MilliSat * 10) / 10
		sb.WriteString(fmt.Sprintf("%d", amountOverflow))
	} else {
		sb.WriteString("0")
	}
	sb.WriteString(";DESCRIPTION=")
	if invoice.Description != nil {
		sb.WriteString(*invoice.Description)
	}

	sb.WriteString(";METADATA=")
	if invoice.Metadata != nil {
		sb.WriteString(fmt.Sprintf("%x", invoice.Metadata))
	}

	sb.WriteString(";RECIPIENT=")
	if invoice.Destination != nil {
		sb.WriteString(fmt.Sprintf("%x", invoice.Destination.SerializeCompressed()))
	}

	sb.WriteString(";DESCRIPTION_HASH=")
	if invoice.DescriptionHash != nil {
		sb.WriteString(fmt.Sprintf("%x", *invoice.DescriptionHash))
	}

	// Convert expiry from time.Duration to seconds as uint64 to ensure consistent
	// overflow behavior across implementations. LND uses signed int64 for expiry in
	// nanoseconds, while other implementations use uint64.
	sb.WriteString(";EXPIRY=")
	sb.WriteString(fmt.Sprintf("%d", uint64(invoice.Expiry().Nanoseconds())/1000000000))

	sb.WriteString(";TIMESTAMP=")
	sb.WriteString(fmt.Sprintf("%d", invoice.Timestamp.Unix()))

	sb.WriteString(";FALLBACK_ADDRESS=")
	if invoice.FallbackAddr != nil {
		sb.WriteString(invoice.FallbackAddr.String())
	}

	for _, routeHint := range invoice.RouteHints {
		sb.WriteString(";PRIVATE_ROUTE=[")
		for i, hopHint := range routeHint {
			if i == 0 {
				sb.WriteString("(")
			} else {
				sb.WriteString(",(")
			}
			sb.WriteString("NODE_ID=")
			sb.WriteString(fmt.Sprintf("%x", hopHint.NodeID.SerializeCompressed()))
			sb.WriteString(",SHORT_CHANNEL_ID=")
			sb.WriteString(fmt.Sprintf("%d", hopHint.ChannelID))
			sb.WriteString(",FEES=")
			sb.WriteString(fmt.Sprintf("%d", hopHint.FeeBaseMSat))
			sb.WriteString(",CLTV_EXPIRY_DELTA=")
			sb.WriteString(fmt.Sprintf("%d", hopHint.CLTVExpiryDelta))
			sb.WriteString(",PROPORTIONAL_MILLIONTHS=")
			sb.WriteString(fmt.Sprintf("%d", hopHint.FeeProportionalMillionths))
			sb.WriteString(")")
		}
		sb.WriteString("]")
	}

	sb.WriteString(";MIN_CLTV=")
	sb.WriteString(fmt.Sprintf("%d", invoice.MinFinalCLTVExpiry()))

	sb.WriteString(";FEATURES=")
	var buf bytes.Buffer
	if err := invoice.Features.RawFeatureVector.EncodeBase256(&buf); err == nil {
		sb.WriteString(fmt.Sprintf("%x", buf.Bytes()))
	}

	return C.CString(sb.String())
}

//export LndParseP2pLightningMessage
func LndParseP2pLightningMessage(data *C.char, length C.int) *C.char {
	buffer := C.GoBytes(unsafe.Pointer(data), length)
	r := bytes.NewReader(buffer)

	message, err := lnwire.ReadMessage(r, 0)
	if err != nil {
		// Skip length validation errors to maintain compatibility with other
		// implementations (e.g., C-Lightning, LND) that don't enforce strict
		// address length checks during decoding.
		if strings.Contains(err.Error(), "bytes into addrBytes") {
			return nil
		}
		return C.CString("")
	}
	sb := strings.Builder{}

	switch message.MsgType() {
	case 1:
		sb.WriteString("MSG_TYPE=warning;CHANNEL_ID=")
		sb.WriteString(fmt.Sprintf("%x", message.(*lnwire.Warning).ChanID[:]))
		sb.WriteString(";DATA=")
		sb.WriteString(fmt.Sprintf("%x", message.(*lnwire.Warning).Data))
	case 17:
		sb.WriteString("MSG_TYPE=error;CHANNEL_ID=")
		sb.WriteString(fmt.Sprintf("%x", message.(*lnwire.Error).ChanID[:]))
		sb.WriteString(";DATA=")
		sb.WriteString(fmt.Sprintf("%x", message.(*lnwire.Error).Data))
	case 18:
		sb.WriteString("MSG_TYPE=ping;NUM_PONG_BYTES=")
		sb.WriteString(fmt.Sprintf("%d", message.(*lnwire.Ping).NumPongBytes))
		sb.WriteString(";IGNORED=")
		sb.WriteString(fmt.Sprintf("%d", len(message.(*lnwire.Ping).PaddingBytes)))
	case 19:
		sb.WriteString("MSG_TYPE=pong;IGNORED=")
		sb.WriteString(fmt.Sprintf("%d", len(message.(*lnwire.Pong).PongBytes)))
	case 32:
		openChannel := message.(*lnwire.OpenChannel)

		tlvMap, err := openChannel.ExtraData.ExtractRecords()
		if err != nil {
			return C.CString("")
		}
		// LND supports extra even TLVs for simple taproot channels.
		// Since other implementations do not, we return an error to maintain
		// compatibility.
		for key := range tlvMap {
			if key%2 == 0 && key != 0 {
				return C.CString("")
			}
		}

		// Both ChannelReserve, FundingAmount and DustLimit may overflow the int64 type
		if openChannel.ChannelReserve < 0 || openChannel.FundingAmount < 0 || openChannel.DustLimit < 0 {
			return nil
		}
		sb.WriteString("MSG_TYPE=open_channel")
		sb.WriteString(";CHAIN_HASH=")
		sb.WriteString(fmt.Sprintf("%x", openChannel.ChainHash[:]))
		sb.WriteString(";TEMPORARY_CHANNEL_ID=")
		sb.WriteString(fmt.Sprintf("%x", openChannel.PendingChannelID[:]))
		sb.WriteString(";FUNDING_SATOSHIS=")
		sb.WriteString(fmt.Sprintf("%d", openChannel.FundingAmount))
		sb.WriteString(";PUSH_MSAT=")
		sb.WriteString(fmt.Sprintf("%d", openChannel.PushAmount))
		sb.WriteString(";DUST_LIMIT_SATOSHIS=")
		sb.WriteString(fmt.Sprintf("%d", openChannel.DustLimit))
		sb.WriteString(";MAX_HTLC_IN_FLIGHT_MSAT=")
		sb.WriteString(fmt.Sprintf("%d", openChannel.MaxValueInFlight))
		sb.WriteString(";CHANNEL_RESERVE_SATOSHIS=")
		sb.WriteString(fmt.Sprintf("%d", openChannel.ChannelReserve))
		sb.WriteString(";HTLC_MINIMUM_MSAT=")
		sb.WriteString(fmt.Sprintf("%d", openChannel.HtlcMinimum))
		sb.WriteString(";FEERATE_PER_KW=")
		sb.WriteString(fmt.Sprintf("%d", openChannel.FeePerKiloWeight))
		sb.WriteString(";TO_SELF_DELAY=")
		sb.WriteString(fmt.Sprintf("%d", openChannel.CsvDelay))
		sb.WriteString(";MAX_ACCEPTED_HTLCS=")
		sb.WriteString(fmt.Sprintf("%d", openChannel.MaxAcceptedHTLCs))
		sb.WriteString(";FUNDING_PUBKEY=")
		sb.WriteString(fmt.Sprintf("%x", openChannel.FundingKey.SerializeCompressed()))
		sb.WriteString(";REVOCATION_BASEPOINT=")
		sb.WriteString(fmt.Sprintf("%x", openChannel.RevocationPoint.SerializeCompressed()))
		sb.WriteString(";PAYMENT_BASEPOINT=")
		sb.WriteString(fmt.Sprintf("%x", openChannel.PaymentPoint.SerializeCompressed()))
		sb.WriteString(";DELAYED_PAYMENT_BASEPOINT=")
		sb.WriteString(fmt.Sprintf("%x", openChannel.DelayedPaymentPoint.SerializeCompressed()))
		sb.WriteString(";HTLC_BASEPOINT=")
		sb.WriteString(fmt.Sprintf("%x", openChannel.HtlcPoint.SerializeCompressed()))
		sb.WriteString(";FIRST_PER_COMMITMENT_POINT=")
		sb.WriteString(fmt.Sprintf("%x", openChannel.FirstCommitmentPoint.SerializeCompressed()))
		sb.WriteString(";CHANNEL_FLAGS=")
		sb.WriteString(fmt.Sprintf("%x", openChannel.ChannelFlags))

		if openChannel.UpfrontShutdownScript != nil {
			sb.WriteString(";UPFRONT_SHUTDOWN_SCRIPT=")
			sb.WriteString(fmt.Sprintf("%x", openChannel.UpfrontShutdownScript))
		}

		if openChannel.ChannelType != nil {
			sb.WriteString(";CHANNEL_TYPE=")
			var channelTypeBuf bytes.Buffer
			channelType := lnwire.RawFeatureVector(*openChannel.ChannelType)
			if err := channelType.EncodeBase256(&channelTypeBuf); err == nil {
				sb.WriteString(fmt.Sprintf("%x", channelTypeBuf.Bytes()))
			}
		}
	case 34:
		fc := message.(*lnwire.FundingCreated)
		// LND doesn't check the signature when parsing the message,
		// but we do, otherwise the fuzzer will crash all the time.
		_, err := fc.CommitSig.ToSignature()
		if err != nil {
			return C.CString("")
		}

		// Skip messages that have extra data. (more than 132 bytes)
		// Since rust-lightning returns an error for messages that are too big.
		if fc.ExtraData != nil {
			return nil
		}
		sb.WriteString("MSG_TYPE=funding_created;TEMPORARY_CHANNEL_ID=")
		sb.WriteString(fmt.Sprintf("%x", fc.PendingChannelID))
		sb.WriteString(";FUNDING_TXID=")
		sb.WriteString(fc.FundingPoint.Hash.String())
		sb.WriteString(";FUNDING_OUTPUT_INDEX=")
		sb.WriteString(fmt.Sprintf("%d", fc.FundingPoint.Index))
		sb.WriteString(";SIGNATURE=")
		sb.WriteString(fmt.Sprintf("%x", fc.CommitSig.ToSignatureBytes()))
	case 35:
		fs := message.(*lnwire.FundingSigned)
		_, err := fs.CommitSig.ToSignature()
		if err != nil {
			return C.CString("")
		}
		// If there is any extra data (a message with more than 98 bytes) we will
		// skip the message, because rust-lightning will return an error for
		// messages that are too big.
		if fs.ExtraData != nil {
			return nil
		}
		sb.WriteString("MSG_TYPE=funding_signed;CHANNEL_ID=")
		sb.WriteString(fmt.Sprintf("%x", fs.ChanID[:]))
		sb.WriteString(";SIGNATURE=")
		sb.WriteString(fmt.Sprintf("%x", fs.CommitSig.ToSignatureBytes()))
	case 36:
		channelReadyMsg := message.(*lnwire.ChannelReady)
		tlvMap, err := channelReadyMsg.ExtraData.ExtractRecords()
		if err != nil {
			return C.CString("")
		}
		// LND supports extra even TLVs for simple taproot channels and gossip v2.
		// Since other implementations do not, we return an error to maintain
		// compatibility.
		for key := range tlvMap {
			if key%2 == 0 {
				return C.CString("")
			}
		}
		sb.WriteString("MSG_TYPE=channel_ready;CHANNEL_ID=")
		sb.WriteString(fmt.Sprintf("%x", channelReadyMsg.ChanID[:]))
		sb.WriteString(";POINT=")
		sb.WriteString(fmt.Sprintf("%x", channelReadyMsg.NextPerCommitmentPoint.SerializeCompressed()))
		if channelReadyMsg.AliasScid != nil {
			sb.WriteString(";ALIAS=")
			sb.WriteString(fmt.Sprintf("%d", channelReadyMsg.AliasScid.ToUint64()))
		}
	case 38:
		messageShutdown := message.(*lnwire.Shutdown)
		if messageShutdown.ExtraData != nil || messageShutdown.CustomRecords != nil || messageShutdown.ShutdownNonce.IsSome() {
			return nil
		}
		sb.WriteString("MSG_TYPE=shutdown;CHANNEL_ID=")
		sb.WriteString(fmt.Sprintf("%x", messageShutdown.ChannelID[:]))
		sb.WriteString(";SCRIPTPUBKEY=")
		sb.WriteString(fmt.Sprintf("%x", messageShutdown.Address))
	case 39:
		messageClosingSigned := message.(*lnwire.ClosingSigned)

		// LND actually doesn't check the signature when parsing the message,
		// but we do, otherwise the fuzzer will crash all the time.
		_, err := messageClosingSigned.Signature.ToSignature()
		if err != nil {
			return C.CString("")
		}
		// FeeSatoshis should be u64 but it's i64 in LND
		if messageClosingSigned.FeeSatoshis < 0 {
			return nil
		}
		// If there is any extra data skip.
		if messageClosingSigned.ExtraData != nil {
			return nil
		}
		sb.WriteString("MSG_TYPE=closing_signed;CHANNEL_ID=")
		sb.WriteString(fmt.Sprintf("%x", messageClosingSigned.ChannelID[:]))
		sb.WriteString(";FEE_SATOSHIS=")
		sb.WriteString(fmt.Sprintf("%d", messageClosingSigned.FeeSatoshis))
		sb.WriteString(";SIGNATURE=")
		sb.WriteString(fmt.Sprintf("%x", messageClosingSigned.Signature.ToSignatureBytes()))
	case 40:
		messageClosingComplete := message.(*lnwire.ClosingComplete)
		// FeeSatoshis should be u64 but it's i64 in LND
		if messageClosingComplete.FeeSatoshis < 0 {
			return nil
		}
		sb.WriteString("MSG_TYPE=closing_complete;CHANNEL_ID=")
		sb.WriteString(fmt.Sprintf("%x", messageClosingComplete.ChannelID[:]))
		sb.WriteString(";CLOSER_SCRIPTPUBKEY=")
		sb.WriteString(fmt.Sprintf("%x", messageClosingComplete.CloserScript))
		sb.WriteString(";CLOSEE_SCRIPTPUBKEY=")
		sb.WriteString(fmt.Sprintf("%x", messageClosingComplete.CloseeScript))
		sb.WriteString(";FEE_SATOSHIS=")
		sb.WriteString(fmt.Sprintf("%d", messageClosingComplete.FeeSatoshis))
		sb.WriteString(";LOCKTIME=")
		sb.WriteString(fmt.Sprintf("%d", messageClosingComplete.LockTime))

		// LND actually doesn't check the signature when parsing the message,
		// but we do, otherwise the fuzzer will crash all the time.
		if messageClosingComplete.ClosingSigs.CloserAndClosee.IsSome() {
			sig := messageClosingComplete.ClosingSigs.CloserAndClosee.UnsafeFromSome().Val
			_, err := sig.ToSignature()
			if err != nil {
				return C.CString("")
			}

			sb.WriteString(";CLOSER_AND_CLOSEE_OUTPUTS_SIG=")
			sb.WriteString(fmt.Sprintf("%x", sig.ToSignatureBytes()))
		}

		if messageClosingComplete.ClosingSigs.CloserNoClosee.IsSome() {
			sig := messageClosingComplete.ClosingSigs.CloserNoClosee.UnsafeFromSome().Val
			_, err := sig.ToSignature()
			if err != nil {
				return C.CString("")
			}

			sb.WriteString(";CLOSER_OUTPUT_SIG=")
			sb.WriteString(fmt.Sprintf("%x", sig.ToSignatureBytes()))
		}

		if messageClosingComplete.ClosingSigs.NoCloserClosee.IsSome() {
			sig := messageClosingComplete.ClosingSigs.NoCloserClosee.UnsafeFromSome().Val
			_, err := sig.ToSignature()
			if err != nil {
				return C.CString("")
			}

			sb.WriteString(";CLOSEE_OUTPUT_SIG=")
			sb.WriteString(fmt.Sprintf("%x", sig.ToSignatureBytes()))
		}

		tlvMap, err := messageClosingComplete.ExtraData.ExtractRecords()
		if err != nil {
			return C.CString("")
		}
		// LND supports extra even TLVs for simple taproot channels and gossip v2.
		// Since other implementations do not, we return an error to maintain
		// compatibility.
		for key := range tlvMap {
			if key%2 == 0 && key != 2 {
				return C.CString("")
			}
		}
	case 128:
		messageUpdateAddHTLC := message.(*lnwire.UpdateAddHTLC)

		var onion sphinx.OnionPacket
		err := onion.Decode(bytes.NewReader(messageUpdateAddHTLC.OnionBlob[:]))
		if err != nil {
			return C.CString("")
		}

		tlvMap, err := messageUpdateAddHTLC.ExtraData.ExtractRecords()
		if err != nil {
			return C.CString("")
		}
		// LND doesn't reject even length TLVs, even if the spec says they should be
		// rejected. So we'll return an error.
		for key := range tlvMap {
			if key%2 == 0 {
				return C.CString("")
			}
		}

		sb.WriteString("MSG_TYPE=update_add_htlc;CHANNEL_ID=")
		sb.WriteString(fmt.Sprintf("%x", messageUpdateAddHTLC.ChanID[:]))
		sb.WriteString(";ID=")
		sb.WriteString(fmt.Sprintf("%d", messageUpdateAddHTLC.ID))
		sb.WriteString(";AMOUNT=")
		sb.WriteString(fmt.Sprintf("%d", messageUpdateAddHTLC.Amount))
		sb.WriteString(";PAYMENT_HASH=")
		sb.WriteString(fmt.Sprintf("%x", messageUpdateAddHTLC.PaymentHash[:]))
		sb.WriteString(";EXPIRY=")
		sb.WriteString(fmt.Sprintf("%d", messageUpdateAddHTLC.Expiry))
		sb.WriteString(";ONION_ROUTING_PACKET=[VERSION=")
		sb.WriteString(fmt.Sprintf("%d", onion.Version))
		sb.WriteString(";PUBLIC_KEY=")
		sb.WriteString(fmt.Sprintf("%x", onion.EphemeralKey.SerializeCompressed()))
		sb.WriteString(";HOP_DATA=")
		sb.WriteString(fmt.Sprintf("%x", onion.RoutingInfo))
		sb.WriteString(";HMAC=")
		sb.WriteString(fmt.Sprintf("%x", onion.HeaderMAC))
		sb.WriteString("]")

		if messageUpdateAddHTLC.BlindingPoint.IsSome() {
			blindingPoint := messageUpdateAddHTLC.BlindingPoint.UnsafeFromSome().Val
			sb.WriteString(";BLINDED_PATH=")
			sb.WriteString(fmt.Sprintf("%x", blindingPoint.SerializeCompressed()))
		}
	}

	return C.CString(sb.String())
}

func main() {}
