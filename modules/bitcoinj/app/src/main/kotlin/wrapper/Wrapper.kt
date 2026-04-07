package wrapper

import org.bitcoinj.base.BitcoinNetwork
import org.bitcoinj.crypto.DeterministicKey
import org.bitcoinj.crypto.HDKeyDerivation
import java.math.BigInteger

object Wrapper {
    @JvmStatic external fun init(): Unit

    /**
     * JNI entry point. Takes raw entropy bytes and returns a serialized master private key (Base58).
     * If the seed is too short, we return "skip error" so C++ can treat it as "skip" and return std::nullopt.
     */
    @JvmStatic
    fun createMasterKey(seed: ByteArray): String {
        return try {
            // Derive master private key
            val key = HDKeyDerivation.createMasterPrivateKey(seed)

            // Serialize using BitcoinJ logic
            key.serializePrivB58(BitcoinNetwork.MAINNET)
        } catch (e: Exception) {
            // BitcoinJ throws for seeds shorter than 8 bytes
            if (e.message?.contains("seed is too short and could be brute forced") == true) {
                "skip error"
            } else {
                ""
            }
        }
    }

    @JvmStatic
    fun deserializeExtendedKey(bytes: ByteArray): String {
        if (bytes.isEmpty()) {
            return "INVALID"
        }
        val base58 = String(bytes, Charsets.UTF_8)

        // skip keys that are bigint 0 or bigint 1 in private key bytes - BitcoinJ does not allow them
        val decoded =
            try {
                org.bitcoinj.base.Base58.decodeChecked(base58)
            } catch (e: Exception) {
                return "INVALID"
            }
        if (decoded.size == 78) {
            val privBytes = decoded.sliceArray(46..77)
            val priv = BigInteger(1, privBytes) // convert to positive BigInteger
            if (priv == BigInteger.ZERO || priv == BigInteger.ONE) {
                return "skip error"
            }
        }
        val network =
            when (base58[0]) {
                'x' -> BitcoinNetwork.MAINNET
                't' -> BitcoinNetwork.TESTNET
                else -> return "INVALID"
            }
        val key =
            try {
                DeterministicKey.deserializeB58(base58, network)
            } catch (e: Exception) {
                return "INVALID"
            }
        val depth = key.depth
        val fingerprint = key.parentFingerprint
        val child =
            if (key.depth == 0 && key.childNumber.i() == 0) {
                extractChildNumberFromBase58(base58) ?: 0
            } else {
                key.childNumber.i()
            }
        val chainCodeHex =
            key.chainCode.joinToString(
                separator = "",
            ) {
                "%02x".format(it)
            }
        val keyHex =
            if (key.hasPrivKey()) {
                key.privKeyBytes.joinToString(
                    separator = "",
                ) {
                    "%02x".format(it)
                }
            } else {
                // Force lazy public key validation (decode + curve check).
                try {
                    key.getPubKeyPoint()
                } catch (e: Exception) {
                    return "INVALID"
                }
                key.pubKey.joinToString(
                    separator = "",
                ) {
                    "%02x".format(it)
                }
            }
        val result =
            String.format(
                "depth=%02x;fp=%02x%02x%02x%02x;child=%08x;chaincode=%s;key=%s",
                depth,
                (fingerprint ushr 24) and 0xff,
                (fingerprint ushr 16) and 0xff,
                (fingerprint ushr 8) and 0xff,
                fingerprint and 0xff,
                child,
                chainCodeHex,
                keyHex,
            )
        return result
    }

    private fun extractChildNumberFromBase58(base58: String): Int? {
        return try {
            val decoded = org.bitcoinj.base.Base58.decodeChecked(base58)
            if (decoded.size != 78) return null

            // child number is bytes 9..12
            ((decoded[9].toInt() and 0xff) shl 24) or
                ((decoded[10].toInt() and 0xff) shl 16) or
                ((decoded[11].toInt() and 0xff) shl 8) or
                (decoded[12].toInt() and 0xff)
        } catch (_: Exception) {
            null
        }
    }
}
