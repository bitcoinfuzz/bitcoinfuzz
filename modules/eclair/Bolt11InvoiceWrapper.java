import fr.acinq.eclair.payment.Bolt11Invoice;
import scala.util.Try;
import scala.util.Either;
import scala.util.Success;
import scala.util.Failure;
import scala.Option;
import scodec.bits.ByteVector;
import scala.collection.immutable.List;
import scala.collection.immutable.Seq;
import fr.acinq.eclair.MilliSatoshi;
import fr.acinq.bitcoin.scalacompat.ByteVector32;
import fr.acinq.eclair.payment.Bolt11Invoice.ExtraHop;

public class Bolt11InvoiceWrapper {
  /**
   * Decodes a BOLT11 invoice and returns all values in a formatted string.
   * This method is designed to be called from other languages via JNI or direct
   * Java calls.
   * 
   * @param invoiceString The BOLT11 invoice string to decode
   * @return Formatted string with all invoice values, or empty string if parsing
   *         fails
   */
  public static String decodeBolt11Invoice(String invoiceString) {
    try {
      Try<Bolt11Invoice> result = Bolt11Invoice.fromString(invoiceString);

      if (!result.isSuccess()) {
        Throwable ex = (Throwable) result.failed().get();
        if (ex.getMessage().equals("requirement failed: there must be exactly one payment secret tag")) {
          return "payment_secret_error";
        }
        return "";
      }

      Bolt11Invoice invoice = result.get();

      StringBuilder sb = new StringBuilder();

      sb.append("HASH=").append(invoice.paymentHash());
      sb.append(";PAYMENT_SECRET=").append(invoice.paymentSecret());

      sb.append(";AMOUNT=");
      Option<MilliSatoshi> amountOpt = invoice.amount_opt();
      if (amountOpt.isDefined()) {
        sb.append(amountOpt.get());
      } else {
        sb.append("0");
      }

      Either<String, ByteVector32> descEither = invoice.description();
      sb.append(";DESCRIPTION=");
      if (descEither.isLeft()) {
        sb.append(descEither.left().get());
      }

      sb.append(";METADATA=");
      Option<ByteVector> metadataOpt = invoice.paymentMetadata();
      if (metadataOpt.isDefined()) {
        sb.append(metadataOpt.get().toHex());
      }

      sb.append(";RECIPIENT=").append(invoice.nodeId().toString());

      sb.append(";DESCRIPTION_HASH=");
      if (descEither.isRight()) {
        sb.append(descEither.right().get());
      }

      sb.append(";EXPIRY=").append(invoice.relativeExpiry().toSeconds());
      sb.append(";TIMESTAMP=").append(invoice.createdAt().toLong());

      Option<String> fallbackAddr = invoice.fallbackAddress();
      sb.append(";FALLBACK_ADDRESS=");
      if (fallbackAddr.isDefined()) {
        sb.append(fallbackAddr.get());
      }

      Seq<Seq<ExtraHop>> routingInfo = invoice.routingInfo();
      scala.collection.Iterator<Seq<ExtraHop>> routeIterator = routingInfo.iterator();

      while (routeIterator.hasNext()) {
        Seq<ExtraHop> route = routeIterator.next();
        sb.append(";PRIVATE_ROUTE=[");

        scala.collection.Iterator<ExtraHop> hopIterator = route.iterator();
        int hopIndex = 0;

        while (hopIterator.hasNext()) {
          ExtraHop hop = hopIterator.next();

          if (hopIndex == 0) {
            sb.append("(");
          } else {
            sb.append(",(");
          }

          sb.append("NODE_ID=").append(hop.nodeId().toString());
          sb.append(",SHORT_CHANNEL_ID=").append(hop.shortChannelId().toString());
          sb.append(",FEES=").append(hop.feeBase().toLong());
          sb.append(",CLTV_EXPIRY_DELTA=").append(hop.cltvExpiryDelta().toInt());
          sb.append(",PROPORTIONAL_MILLIONTHS=").append(hop.feeProportionalMillionths());
          sb.append(")");

          hopIndex++;
        }

        sb.append("]");
      }

      sb.append(";MIN_CLTV=").append(invoice.minFinalCltvExpiryDelta().toInt());
      sb.append(";FEATURES=").append(invoice.features().toByteVector().toHex());

      return sb.toString();

    } catch (Exception e) {
      return "";
    }
  }
}