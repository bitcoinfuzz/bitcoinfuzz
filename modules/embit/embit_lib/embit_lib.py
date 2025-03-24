from io import BytesIO
from embit.descriptor.miniscript import Miniscript
from embit.descriptor import Descriptor

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
        
def descriptor_parse(input):
    try:
        desc = Descriptor.from_string(input)
        return True
    except Exception as _:
        return False