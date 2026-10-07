"""Python mirror of include/bitcoinfuzz/ffi.h, read back by helpers/pybridge.cpp."""

OK = 0
SKIP = 1
FAIL = 2


class BfResult:
    __slots__ = ("status", "value")

    def __init__(self, status: int, value: str = ""):
        self.status = status
        self.value = value.encode()


def ok(value: str) -> BfResult:
    return BfResult(OK, value)


def skip() -> BfResult:
    return BfResult(SKIP)


def fail(reason: str = "") -> BfResult:
    """The driver compares the reason, so it must match what the target's other
    modules return on rejection (e.g. "INVALID")."""
    return BfResult(FAIL, reason)
