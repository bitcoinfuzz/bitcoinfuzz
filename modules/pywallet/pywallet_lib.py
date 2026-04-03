from pywallet.utils import Wallet


def bip32_master_keygen(data: bytes) -> str:
    try:
        master = Wallet.from_master_secret(seed=data, network="BTC")
        return master.serialize_b58(private=True)
    except Exception:
        return "INVALID"