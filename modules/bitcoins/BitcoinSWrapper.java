import bitcoinfuzz.BfResult;
import org.bitcoins.core.crypto.ExtKey;
import org.bitcoins.core.crypto.ExtKey$;
import org.bitcoins.core.crypto.ExtKeyPrivVersion;
import org.bitcoins.core.crypto.ExtPrivateKey;
import org.bitcoins.core.crypto.ExtPrivateKey$;
import org.bitcoins.core.crypto.ExtPublicKey;
import org.bitcoins.core.hd.BIP32Path;
import org.bitcoins.core.hd.BIP32Path$;
import org.bitcoins.core.protocol.script.Script;
import org.bitcoins.core.protocol.transaction.Transaction;
import org.bitcoins.core.protocol.transaction.TransactionInput;
import org.bitcoins.core.protocol.transaction.TransactionOutput;
import org.bitcoins.core.psbt.InputPSBTMap;
import org.bitcoins.core.psbt.OutputPSBTMap;
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

  public static BfResult createMasterKey(byte[] seedBytes) {
    try {
      ByteVector bv = ByteVector$.MODULE$.apply(seedBytes);
      ExtKeyPrivVersion version = legacyMainNetPriv();
      scala.Option<ByteVector> seedOpt = scala.Some$.MODULE$.apply(bv);
      BIP32Path emptyPath = BIP32Path$.MODULE$.empty();
      ExtPrivateKey key = (ExtPrivateKey) ExtPrivateKey$.MODULE$.apply(version, seedOpt, emptyPath);
      return BfResult.ok(key.toStringSensitive());
    } catch (Exception e) {
      return BfResult.fail();
    }
  }

  public static BfResult deserializeExtendedKey(byte[] bytes) {
    if (bytes.length == 0) return BfResult.fail("INVALID");
    try {
      String base58 = new String(bytes, "UTF-8").trim();
      Try<ExtKey> result = ExtKey$.MODULE$.fromStringT(base58);
      if (!result.isSuccess()) return BfResult.fail("INVALID");

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

      return BfResult.ok(
          String.format(
              "depth=%02x;fp=%s;child=%08x;chaincode=%s;key=%s", depth, fp, child, cc, keyHex));

    } catch (Exception e) {
      return BfResult.fail("INVALID");
    }
  }

  public static BfResult parsePSBT(byte[] psbtBytes) {
    if (psbtBytes.length == 0) return BfResult.skip();
    try {
      ByteVector bv = ByteVector$.MODULE$.apply(psbtBytes);
      PSBT psbt = PSBT$.MODULE$.fromBytes(bv);
      Transaction tx = psbt.transaction();

      StringBuilder sb = new StringBuilder();
      // Int32 is signed; print it unsigned like every other module.
      sb.append("tx_version=").append(Integer.toUnsignedString(tx.version().toInt())).append(";");
      sb.append("lock_time=").append(tx.lockTime().toLong()).append(";");
      sb.append("inputs=").append(tx.inputs().size()).append(";");
      sb.append("outputs=").append(tx.outputs().size()).append(";");

      scala.collection.immutable.Seq<?> inputs = tx.inputs();
      for (int i = 0; i < inputs.size(); i++) {
        TransactionInput txIn = (TransactionInput) inputs.apply(i);
        String prevHash = txIn.previousOutput().txIdBE().hex();
        long vout = txIn.previousOutput().vout().toLong();
        sb.append("input")
            .append(i)
            .append("previous_output=")
            .append(prevHash)
            .append(":")
            .append(vout)
            .append(";");
        sb.append("input")
            .append(i)
            .append("sequence=")
            .append(txIn.sequence().toLong())
            .append(";");

        InputPSBTMap inp = (InputPSBTMap) psbt.inputMaps().apply(i);
        boolean hasUtxo =
            inp.nonWitnessOrUnknownUTXOOpt().isDefined() || inp.witnessUTXOOpt().isDefined();
        if (hasUtxo) {
          sb.append("input").append(i).append("utxo=1;");
        }
        sb.append("input")
            .append(i)
            .append("partial_signatures=")
            .append(inp.partialSignatures().size())
            .append(";");

        // .hex() on a script includes bitcoin-s's own compact-size length
        // prefix; .asmHex() is the raw script bytes, matching the other
        // modules' HexStr(script)-style output.
        String redeemScriptHex =
            inp.redeemScriptOpt().isDefined()
                ? inp.redeemScriptOpt().get().redeemScript().asmHex()
                : "";
        sb.append("input").append(i).append("redeem_script=").append(redeemScriptHex).append(";");

        // witnessScript() is typed as RawScriptPubKey, an interface that
        // doesn't itself expose asmHex() to Java; cast to Script (every
        // concrete implementation extends it) to reach it.
        String witnessScriptHex =
            inp.witnessScriptOpt().isDefined()
                ? ((Script) inp.witnessScriptOpt().get().witnessScript()).asmHex()
                : "";
        sb.append("input").append(i).append("witness_script=").append(witnessScriptHex).append(";");

        // raw PSBT_IN_SIGHASH_TYPE value, or 0 if unset. HashType.num() is a
        // signed Java int holding the raw 4 bytes, so any value with the top
        // bit set (e.g. 0xfe8f263f) would render negative, while every other
        // module formats it unsigned (Bitcoin Core casts to uint32_t, btcd to
        // uint32, rust-psbt uses to_u32). Print it unsigned to match.
        int sighashType =
            inp.sigHashTypeOpt().isDefined() ? inp.sigHashTypeOpt().get().hashType().num() : 0;
        sb.append("input")
            .append(i)
            .append("sighash_type=")
            .append(Integer.toUnsignedString(sighashType))
            .append(";");

        sb.append("input")
            .append(i)
            .append("bip32=")
            .append(inp.BIP32DerivationPaths().length())
            .append(";");

        // Report finalization on a *non-empty* final scriptSig/scriptWitness
        // rather than mere presence, which is what isFinalized() checks.
        // Bitcoin Core stores its final witness as a plain CScriptWitness whose
        // IsNull() is just stack.empty(), so it cannot distinguish an absent
        // PSBT_IN_FINAL_SCRIPTWITNESS key from one present with a zero-item
        // stack; presence-based semantics would make this flag incomparable
        // across modules for that degenerate input. An empty witness finalizes
        // nothing anyway.
        boolean finalizedScriptSig =
            inp.finalizedScriptSigOpt().isDefined()
                && !((Script) inp.finalizedScriptSigOpt().get().scriptSig()).asmHex().isEmpty();
        boolean finalizedScriptWitness =
            inp.finalizedScriptWitnessOpt().isDefined()
                && !inp.finalizedScriptWitnessOpt().get().scriptWitness().stack().isEmpty();
        if (finalizedScriptSig || finalizedScriptWitness) {
          sb.append("input").append(i).append("finalized=1;");
        }
      }

      scala.collection.immutable.Seq<?> outputs = tx.outputs();
      for (int i = 0; i < outputs.size(); i++) {
        TransactionOutput txOut = (TransactionOutput) outputs.apply(i);
        sb.append("output")
            .append(i)
            .append("val=")
            .append(txOut.value().satoshis().toLong())
            .append(";");
        sb.append("output")
            .append(i)
            .append("script=")
            .append(txOut.scriptPubKey().asmHex())
            .append(";");

        if (i < psbt.outputMaps().length()) {
          OutputPSBTMap out = (OutputPSBTMap) psbt.outputMaps().apply(i);

          String outRedeemScriptHex =
              out.redeemScriptOpt().isDefined()
                  ? out.redeemScriptOpt().get().redeemScript().asmHex()
                  : "";
          sb.append("output")
              .append(i)
              .append("redeem_script=")
              .append(outRedeemScriptHex)
              .append(";");

          String outWitnessScriptHex =
              out.witnessScriptOpt().isDefined()
                  ? out.witnessScriptOpt().get().witnessScript().asmHex()
                  : "";
          sb.append("output")
              .append(i)
              .append("witness_script=")
              .append(outWitnessScriptHex)
              .append(";");

          sb.append("output")
              .append(i)
              .append("bip32=")
              .append(out.BIP32DerivationPaths().length())
              .append(";");
        }
      }

      return BfResult.ok(sb.toString());

    } catch (Exception e) {
      return BfResult.fail("INVALID");
    }
  }
}
