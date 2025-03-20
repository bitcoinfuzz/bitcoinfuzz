
from io import BytesIO
from embit.descriptor.miniscript import Miniscript

def miniscript_parse(input):
    try:
        ms = Miniscript.read_from(BytesIO(input.encode()), taproot=False)
        ms.verify()
        return True
    except Exception as _:
        try:
            ms = Miniscript.read_from(BytesIO(input.encode()), taproot=True)
            ms.verify()
            return True
        except Exception as _:
            return False