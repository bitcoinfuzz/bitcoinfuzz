import org.bitcoins.core.crypto.ExtKey;
import org.bitcoins.core.crypto.ExtKey$;
import org.bitcoins.core.crypto.ExtKeyPrivVersion;
import org.bitcoins.core.crypto.ExtPrivateKey;
import org.bitcoins.core.crypto.ExtPrivateKey$;
import org.bitcoins.core.crypto.ExtPublicKey;
import org.bitcoins.core.hd.BIP32Path;
import org.bitcoins.core.hd.BIP32Path$;
import org.bitcoins.core.protocol.transaction.Transaction;
import org.bitcoins.core.protocol.transaction.TransactionInput;
import org.bitcoins.core.protocol.transaction.TransactionOutput;
import org.bitcoins.core.psbt.InputPSBTMap;
import org.bitcoins.core.psbt.PSBT;
import org.bitcoins.core.psbt.PSBT$;
import scala.util.Try;
import scodec.bits.ByteVector;
import scodec.bits.ByteVector$;

public class BitcoinSWrapper {

  // LegacyMainNetPriv is a Scala case object whose class name contains '$',
  // which javac cannot reference directly. Load it once via reflection.
  private static ExtKeyPrivVersion LEGACY_MAINNET_PRIV = null;

  private static ExtKeyPrivVersion legacyMainNetPriv() throws Exception {
    if (LEGACY_MAINNET_PRIV == null) {
      Class<?> cls = Class.forName("org.bitcoins.core.crypto.ExtKeyVersion$LegacyMainNetPriv$");
      LEGACY_MAINNET_PRIV = (ExtKeyPrivVersion) cls.getField("MODULE$").get(null);
    }
    return LEGACY_MAINNET_PRIV;
  }

  public static String createMasterKey(byte[] seedBytes) {
    try {
      ByteVector bv = ByteVector$.MODULE$.apply(seedBytes);
      ExtKeyPrivVersion version = legacyMainNetPriv();
      scala.Option<ByteVector> seedOpt = scala.Some$.MODULE$.apply(bv);
      BIP32Path emptyPath = BIP32Path$.MODULE$.empty();
      ExtPrivateKey key = (ExtPrivateKey) ExtPrivateKey$.MODULE$.apply(version, seedOpt, emptyPath);
      return key.toStringSensitive();
    } catch (Exception e) {
      return "";
    }
  }

  public static String deserializeExtendedKey(byte[] bytes) {
    if (bytes.length == 0) return "INVALID";
    try {
      String base58 = new String(bytes, "UTF-8").trim();
      Try<ExtKey> result = ExtKey$.MODULE$.fromStringT(base58);
      if (!result.isSuccess()) return "INVALID";

      ExtKey key = result.get();

      int depth = key.depth().toInt() & 0xff;
      String fp = key.fingerprint().toHex();
      long child = key.childNum().toLong();
      String cc = key.chainCode().bytes().toHex();

      String keyHex;
      if (key instanceof ExtPrivateKey) {
        keyHex = ((ExtPrivateKey) key).key().bytes().toHex();
      } else {
        keyHex = ((ExtPublicKey) key).key().bytes().toHex();
      }

      return String.format(
          "depth=%02x;fp=%s;child=%08x;chaincode=%s;key=%s", depth, fp, child, cc, keyHex);

    } catch (Exception e) {
      return "INVALID";
    }
  }

  public static String parsePSBT(byte[] psbtBytes) {
    if (psbtBytes.length == 0) return "";
    try {
      ByteVector bv = ByteVector$.MODULE$.apply(psbtBytes);
      PSBT psbt = PSBT$.MODULE$.fromBytes(bv);
      Transaction tx = psbt.transaction();

      StringBuilder sb = new StringBuilder();
      sb.append("lt=").append(tx.lockTime().toLong()).append(";");
      sb.append("in=").append(tx.inputs().size()).append(";");
      sb.append("out=").append(tx.outputs().size()).append(";");

      scala.collection.immutable.Seq<?> inputs = tx.inputs();
      for (int i = 0; i < inputs.size(); i++) {
        TransactionInput txIn = (TransactionInput) inputs.apply(i);
        String prevHash = txIn.previousOutput().txIdBE().hex();
        long vout = txIn.previousOutput().vout().toLong();
        sb.append("in")
            .append(i)
            .append("prev=")
            .append(prevHash)
            .append(":")
            .append(vout)
            .append(";");
        sb.append("in").append(i).append("seq=").append(txIn.sequence().toLong()).append(";");

        InputPSBTMap inp = (InputPSBTMap) psbt.inputMaps().apply(i);
        boolean hasUtxo =
            inp.nonWitnessOrUnknownUTXOOpt().isDefined() || inp.witnessUTXOOpt().isDefined();
        if (hasUtxo) {
          sb.append("in").append(i).append("utxo=1;");
        }
        sb.append("in")
            .append(i)
            .append("sigs=")
            .append(inp.partialSignatures().size())
            .append(";");
      }

      scala.collection.immutable.Seq<?> outputs = tx.outputs();
      for (int i = 0; i < outputs.size(); i++) {
        TransactionOutput txOut = (TransactionOutput) outputs.apply(i);
        sb.append("out")
            .append(i)
            .append("val=")
            .append(txOut.value().satoshis().toLong())
            .append(";");
        sb.append("out").append(i).append("script=").append(txOut.scriptPubKey().hex()).append(";");
      }

      return sb.toString();

    } catch (Exception e) {
      return "";
    }
  }
}
