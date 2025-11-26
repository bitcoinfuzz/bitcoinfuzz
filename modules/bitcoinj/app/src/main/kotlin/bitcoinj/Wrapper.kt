package bitcoinj

import org.bitcoinj.crypto.DeterministicKey
import org.bitcoinj.crypto.HDKeyDerivation
import org.bitcoinj.base.BitcoinNetwork

object BitcoinJWrapper {
    @JvmStatic external fun init(): Unit

    /**
     * JNI entry point. Takes raw entropy bytes and returns a serialized master private key (Base58).
     * If the seed is too short, we return an empty string so C++ can treat it as "skip".
     */
    @JvmStatic
    fun createMasterKey(seed: ByteArray): String {
        return try {

            // Derive master private key
            val key = HDKeyDerivation.createMasterPrivateKey(seed)

            // Serialize using BitcoinJ logic
            key.serializePrivB58(BitcoinNetwork.MAINNET)
        } catch (e: Exception) {
            // BitcoinJ throws for seeds shorter than 16 bytes
            if (e.message?.contains("seed is too short and could be brute forced") == true) {
                "skip error"
            } else {
                ""
            }
        }
    }
}
