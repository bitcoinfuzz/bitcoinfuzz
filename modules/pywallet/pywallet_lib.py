import sys
import types
import importlib.abc
import importlib.machinery
from unittest.mock import MagicMock


class _MockModule(types.ModuleType):
    """ModuleType that returns a MagicMock for any attribute access."""

    def __getattr__(self, name: str) -> MagicMock:
        mock = MagicMock()
        setattr(self, name, mock)
        return mock


class _Two1Loader(importlib.abc.Loader):
    def create_module(self, spec):
        mod = _MockModule(spec.name)
        mod.__path__ = []
        return mod

    def exec_module(self, module):
        pass


class _Two1Finder(importlib.abc.MetaPathFinder):
    def find_spec(self, name, path, target=None):
        if name == "two1" or name.startswith("two1."):
            return importlib.machinery.ModuleSpec(name, _Two1Loader())
        return None


sys.meta_path.insert(0, _Two1Finder())

from pywallet import wallet


def bip32_master_keygen(data: bytes) -> str:
    try:
        seed_hex = data.hex()
        w = wallet.create_wallet(network="BTC", seed=seed_hex, children=0)
        return w["xprivate_key"]
    except Exception:
        return "INVALID"
