from io import BytesIO
from embit.descriptor.miniscript import Miniscript

def miniscript_parse(input):
    try:
        ms = Miniscript.from_string(input, taproot=False)
        ms.verify()
        return True
    except Exception as _:
        try:
            ms = Miniscript.from_string(input, taproot=True)
            ms.verify()
            return True
        except Exception as _:
            return False
