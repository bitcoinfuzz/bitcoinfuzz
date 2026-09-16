import os
from typing import Optional

# python-hdwallet depends on pycryptodome, which dlopens its native extensions
# with RTLD_DEEPBIND. That flag is incompatible with the sanitizer runtime the
# fuzzer is built with, so opt out before hdwallet (and thus Crypto) is imported.
os.environ.setdefault("PYCRYPTODOME_DISABLE_DEEPBIND", "1")

from hdwallet.consts import PUBLIC_KEY_TYPES  # noqa: E402
from hdwallet.cryptocurrencies import Bitcoin  # noqa: E402
from hdwallet.hds import BIP32HD  # noqa: E402

# python-hdwallet rejects seeds shorter than 16 bytes, while other modules
# (embit, pycoin, ...) derive a master key from any length. Skip those inputs
# instead of reporting them as invalid to avoid false mismatches.
MIN_SEED_LEN = 16

XPRV_VERSION = Bitcoin.NETWORKS.MAINNET.XPRIVATE_KEY_VERSIONS.P2PKH


def bip32_master_keygen(data: bytes) -> Optional[str]:
    if len(data) < MIN_SEED_LEN:
        return None
    try:
        hd = BIP32HD(ecc=Bitcoin.ECC, public_key_type=PUBLIC_KEY_TYPES.COMPRESSED)
        hd.from_seed(seed=data)
        return hd.root_xprivate_key(version=XPRV_VERSION)
    except Exception:
        return "INVALID"
