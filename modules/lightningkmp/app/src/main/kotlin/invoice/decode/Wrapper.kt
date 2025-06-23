package invoice.decode

import fr.acinq.bitcoin.utils.Try
import fr.acinq.lightning.payment.Bolt11Invoice
import fr.acinq.lightning.utils.toByteVector

object InvoiceDecoder {

    @JvmStatic external fun init(): Unit

    /**
     * Decodes a BOLT11 invoice and returns in a string format. This function is designed to be called from
     * C++ via JNI
     */
    @JvmStatic
    fun decodeBolt11Invoice(invoiceString: String): String {
        val invoice =
                when (val res = Bolt11Invoice.read(invoiceString)) {
                    is Try.Success -> res.result
                    is Try.Failure -> {
                        // Handle invoices without payment secrets by returning null
                        // This is needed because LND don't require payment secrets,
                        // and we need to maintain compatibility with that implementation
                        if (res.error.message == "there must be exactly one payment secret tag") {
                            return "skip error"
                        }
                        if (res.error.message == "var_onion_optin must be supported") {
                            return "skip error"
                        }
                        return ""
                    }
                }
        return buildString {
            append("HASH=${invoice.paymentHash}")
            append(";PAYMENT_SECRET=${invoice.paymentSecret}")
            append(";AMOUNT=${invoice.amount?.toLong() ?: "0"}")
            append(";DESCRIPTION=${invoice.description ?: ""}")
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
            append(";MIN_CLTV=${invoice.minFinalExpiryDelta?.toLong() ?: Bolt11Invoice.DEFAULT_MIN_FINAL_EXPIRY_DELTA.toLong()}")
            append(";FEATURES=${invoice.features.toByteArray().toByteVector()}")
        }
    }
}
