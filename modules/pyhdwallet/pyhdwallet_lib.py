from hdwallet import HDWallet
from hdwallet.cryptocurrencies import Bitcoin
from hdwallet.hds import BIP32HD
from hdwallet.seeds import BIP39Seed
from hdwallet.consts import PUBLIC_KEY_TYPES


def bip32_master_keygen(data: bytes) -> str:
    try:
        if len(data) < 16:
            return "INVALID"
        seed_hex = data.hex()
        hdwallet = HDWallet(
            cryptocurrency=Bitcoin,
            hd=BIP32HD,
            network=Bitcoin.NETWORKS.MAINNET,
            public_key_type=PUBLIC_KEY_TYPES.COMPRESSED,
        ).from_seed(seed=BIP39Seed(seed=seed_hex))
        return hdwallet.root_xprivate_key()
    except Exception:
        return "INVALID"
