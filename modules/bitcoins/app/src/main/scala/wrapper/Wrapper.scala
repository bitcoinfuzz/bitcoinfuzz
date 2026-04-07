package wrapper

import org.bitcoins.core.crypto._
import org.bitcoins.core.hd.BIP32Path
import scodec.bits.ByteVector

object Wrapper {

  /**
   * JNI entry point. Takes raw seed bytes and returns a serialized master private key (Base58 xprv string).
   * Uses the ExtPrivateKey(version, seedOpt, path) overload which performs BIP32 master key derivation (HMAC-SHA512) internally.
   * Returns "skip error" for seeds that produce invalid keys.
   */

  def createMasterKey(seed: Array[Byte]): String = {
    if (seed == null || seed.length < 16 || seed.length > 64) {
      return "skip error"
    }

    try {
      val xprv: ExtPrivateKey = ExtPrivateKey(
        ExtKeyVersion.LegacyMainNetPriv,
        Some(ByteVector(seed)),
        BIP32Path.empty)
      xprv.toString
    } catch {
      case _: Exception => ""
    }
  }
}
