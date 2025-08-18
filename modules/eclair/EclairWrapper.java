import fr.acinq.eclair.payment.Bolt11Invoice;
import fr.acinq.eclair.wire.protocol.OfferTypes.Offer$;
import fr.acinq.eclair.wire.protocol.OfferTypes.Offer;
import fr.acinq.eclair.wire.protocol.OfferTypes.OfferCurrency$;
import fr.acinq.eclair.wire.protocol.OfferTypes.OfferCurrency;
import fr.acinq.eclair.wire.protocol.OfferTypes.OfferAmount$;
import fr.acinq.eclair.wire.protocol.OfferTypes.OfferAmount;
import fr.acinq.eclair.wire.protocol.OfferTypes.BlindedPath;
import fr.acinq.eclair.wire.protocol.OfferTypes.ContactInfo;
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
import fr.acinq.bitcoin.scalacompat.Crypto.PublicKey;
import fr.acinq.eclair.crypto.Sphinx;
import fr.acinq.eclair.crypto.Sphinx$;
import java.util.Currency;
import fr.acinq.bitcoin.scalacompat.BlockHash;
import fr.acinq.eclair.Features;
import fr.acinq.eclair.Bolt12Feature;
import fr.acinq.eclair.TimestampSecond;

public class EclairWrapper {
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
        sb.append(amountOpt.get().toLong());
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
          sb.append(",SHORT_CHANNEL_ID=").append(hop.shortChannelId().toLong());
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

  /**
   * Decodes a offer and returns all values in a formatted string.
   */
  public static String decodeOffer(String offerString) {
    try {
      Try<Offer> result = Offer$.MODULE$.decode(offerString);
      if (!result.isSuccess()) {
        return "";
      }

      Offer offer = result.get();
      StringBuilder sb = new StringBuilder();

      sb.append("CHAINS=");
      Seq<BlockHash> chains = offer.chains();
      for (int i = 0; i < chains.length(); i++) {
        sb.append(chains.apply(i).toString());
        if (i < chains.length() - 1) {
          sb.append(";");
        }
      }

      sb.append(";METADATA=");
      Option<ByteVector> metadata = offer.metadata();
      if (metadata.isDefined()) {
        sb.append(metadata.get().toHex());
      }

      scala.collection.Iterator recordsIterator = offer.records().records().iterator();
      while (recordsIterator.hasNext()) {
        Object record = recordsIterator.next();
        if (record instanceof OfferAmount) {
          OfferAmount offerAmount = (OfferAmount) record;
          sb.append(";AMOUNT=");
          sb.append(offerAmount.amount());
          break;
        }
      }

      recordsIterator = offer.records().records().iterator();
      while (recordsIterator.hasNext()) {
        Object record = recordsIterator.next();
        if (record instanceof OfferCurrency) {
          OfferCurrency currencyTlv = (OfferCurrency) record;
          Currency currency = currencyTlv.currency();
          sb.append(";CURRENCY=");
          sb.append(currency.getCurrencyCode());
          break;
        }
      }

      sb.append(";DESCRIPTION=");
      Option<String> description = offer.description();
      if (description.isDefined()) {
        sb.append(description.get());
      }

      sb.append(";FEATURES=");
      Features<Bolt12Feature> features = offer.features();
      sb.append(features.toByteVector().toHex());

      sb.append(";ABSOLUTE_EXPIRY=");
      Option<TimestampSecond> expiry = offer.expiry();
      if (expiry.isDefined()) {
        sb.append(expiry.get().toLong());
      }

      Seq<ContactInfo> contactInfos = offer.contactInfos();
      int pathIndex = 0;
      for (int i = 0; i < contactInfos.length(); i++) {
        ContactInfo contactInfo = contactInfos.apply(i);
        if (contactInfo instanceof BlindedPath) {
          BlindedPath blindedPath = (BlindedPath) contactInfo;
          Object route = blindedPath.route();
          java.lang.reflect.Method blindedNodeIdsMethod = route.getClass().getMethod("blindedNodeIds");
          Seq<PublicKey> blindedNodeIds = (Seq<PublicKey>) blindedNodeIdsMethod.invoke(route);

          for (int j = 0; j < blindedNodeIds.length(); j++) {
            sb.append(";PATH_").append(pathIndex).append("_HOP=");
            sb.append(blindedNodeIds.apply(j).toString());
          }
          pathIndex++;
        }
      }

      sb.append(";ISSUER=");
      Option<String> issuer = offer.issuer();
      if (issuer.isDefined()) {
        sb.append(issuer.get());
      }

      sb.append(";QUANTITY=");
      Option<Object> quantityMax = offer.quantityMax();
      if (quantityMax.isDefined()) {
        sb.append((Long) quantityMax.get());
      }

      sb.append(";ISSUER_ID=");
      Option<PublicKey> nodeId = offer.nodeId();
      if (nodeId.isDefined()) {
        sb.append(nodeId.get().toString());
      }

      return sb.toString();
    } catch (Exception e) {
      return "";
    }
  }
}