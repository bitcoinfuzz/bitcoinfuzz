package lightningkmp

import bitcoinfuzz.BfResult
import fr.acinq.bitcoin.utils.Try
import fr.acinq.lightning.payment.Bolt11Invoice
import fr.acinq.lightning.payment.Bolt12Invoice
import fr.acinq.lightning.utils.toByteVector
import fr.acinq.lightning.wire.OfferTypes.Offer

/** Hex-encodes UTF-8 text so NULs and non-ASCII survive the JNI string boundary. */
private fun String.toUtf8Hex(): String = encodeToByteArray().toHexString()

object Wrapper {
    /**
     * Decodes a BOLT11 invoice and returns in a string format. This function is designed to be
     * called from C++ via JNI
     */
    @JvmStatic
    fun decodeBolt11Invoice(invoiceString: String): BfResult {
        val invoice =
            when (val res = Bolt11Invoice.read(invoiceString)) {
                is Try.Success -> res.result
                is Try.Failure -> {
                    // Skip invoices without payment secrets: LND doesn't require them, so these
                    // can't be compared with that implementation
                    if (res.error.message == "there must be exactly one payment secret tag") {
                        return BfResult.skip()
                    }
                    if (res.error.message == "var_onion_optin must be supported") {
                        return BfResult.skip()
                    }
                    return BfResult.fail()
                }
            }
        return BfResult.ok(
            buildString {
                append("HASH=${invoice.paymentHash}")
                append(";PAYMENT_SECRET=${invoice.paymentSecret}")
                append(";AMOUNT=${invoice.amount?.toLong() ?: "0"}")
                append(";DESCRIPTION=${invoice.description?.toUtf8Hex() ?: ""}")
                append(";METADATA=${invoice.paymentMetadata ?: ""}")
                append(";RECIPIENT=${invoice.nodeId}")
                append(";DESCRIPTION_HASH=${invoice.descriptionHash ?: ""}")
                append(";EXPIRY=${invoice.expirySeconds ?: Bolt11Invoice.DEFAULT_EXPIRY_SECONDS}")
                append(";TIMESTAMP=${invoice.timestampSeconds}")
                append(";FALLBACK_ADDRESS=${invoice.fallbackAddress ?: ""}")
                invoice.routingInfo.forEach { routingInfo ->
                    append(";PRIVATE_ROUTE=[")
                    routingInfo.hints.forEachIndexed { i, hop ->
                        if (i == 0) {
                            append("(")
                        } else {
                            append(",(")
                        }
                        append("NODE_ID=${hop.nodeId}")
                        append(",SHORT_CHANNEL_ID=${hop.shortChannelId.toLong()}")
                        append(",FEES=${hop.feeBase.toLong()}")
                        append(",CLTV_EXPIRY_DELTA=${hop.cltvExpiryDelta.toLong()}")
                        append(",PROPORTIONAL_MILLIONTHS=${hop.feeProportionalMillionths}")
                        append(")")
                    }
                    append("]")
                }
                append(
                    ";MIN_CLTV=${invoice.minFinalExpiryDelta?.toLong() ?: Bolt11Invoice.DEFAULT_MIN_FINAL_EXPIRY_DELTA.toLong()}",
                )
                append(";FEATURES=${invoice.features.toByteArray().toByteVector()}")
            },
        )
    }

    /**
     * Decodes a BOLT12 invoice and returns in a string format. This function is designed to be
     * called from C++ via JNI
     */
    @JvmStatic
    fun decodeBolt12Invoice(invoiceString: String): BfResult {
        return try {
            val invoice =
                when (val res = Bolt12Invoice.fromString(invoiceString)) {
                    is Try.Success -> res.result
                    is Try.Failure -> return BfResult.fail()
                }

            // fromString() only checks that a signature is present; CLN and LDK also verify it.
            if (!invoice.checkSignature()) {
                return BfResult.fail()
            }

            // invoice_created_at after 9999-12-31T23:59:59Z (253402300799): lightning-kmp
            // accepts it, Eclair rejects it.
            if (invoice.createdAtSeconds > 253402300799L) {
                return BfResult.skip()
            }

            BfResult.ok(
                buildString {
                    append("INVREQ_METADATA=${invoice.invoiceRequest.metadata}")
                    append(";INVREQ_CHAIN=${invoice.invoiceRequest.chain}")
                    append(";INVREQ_PAYER_ID=${invoice.invoiceRequest.payerId}")
                    append(";INVOICE_CREATED_AT=${invoice.createdAtSeconds}")
                    append(";INVOICE_RELATIVE_EXPIRY=${invoice.relativeExpirySeconds}")
                    append(";INVOICE_PAYMENT_HASH=${invoice.paymentHash}")
                    append(";INVOICE_AMOUNT=${invoice.amount?.toLong() ?: ""}")
                    append(";INVOICE_FEATURES=${invoice.features.toByteArray().toByteVector()}")
                    append(";INVOICE_NODE_ID=${invoice.nodeId}")
                    append(";INVOICE_PATHS=${invoice.blindedPaths.size}")
                },
            )
        } catch (e: Exception) {
            BfResult.fail()
        }
    }

    /**
     * Decodes a BOLT12 offer and returns in a string format. This function is designed to be called
     * from C++ via JNI
     */
    @JvmStatic
    fun decodeBolt12Offer(offerString: String): BfResult {
        val offer =
            when (val res = Offer.decode(offerString)) {
                is Try.Success -> res.result
                is Try.Failure -> {
                    return BfResult.fail()
                }
            }

        return BfResult.ok(
            buildString {
                append("CHAINS=")
                offer.chains.forEach { chain -> append("$chain;") }
                append("METADATA=${offer.metadata ?: ""}")
                if (offer.amount != null) {
                    append(";AMOUNT=${offer.amount}")
                    offer.currency?.let { append(";CURRENCY=${it.toUtf8Hex()}") }
                }
                append(";DESCRIPTION=${offer.description?.toUtf8Hex() ?: ""}")
                append(";FEATURES=${offer.features.toByteArray().toByteVector()}")
                append(";ABSOLUTE_EXPIRY=${offer.expirySeconds ?: ""}")
                offer.paths?.forEachIndexed { i, path ->
                    path.route.blindedHops.forEachIndexed { _, hop ->
                        append(";PATH_${i}_HOP=${hop.blindedPublicKey}")
                    }
                }
                append(";ISSUER=${offer.issuer?.toUtf8Hex() ?: ""}")
                append(";QUANTITY=${offer.quantityMax ?: ""}")
                append(";ISSUER_ID=${offer.issuerId ?: ""}")
            },
        )
    }
}
