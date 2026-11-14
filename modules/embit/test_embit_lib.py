import unittest

from bfresult import FAIL, OK, SKIP
from embit.descriptor.miniscript import JustZero, Miniscript, N

from embit_lib import descriptor_parse, is_satisfiable, miniscript_parse

KEY = "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"
XKEY = KEY[2:]


class SatisfiabilityTest(unittest.TestCase):
    def test_unsatisfiable_fragments(self):
        for text in (
            "0",
            "a:0",
            "sdv:0",
            "tv:0",
            "dv:0",
            "v:0",
            "jdv:0",
            "jjdv:0",
            "n:0",
            "l:0",
            "u:0",
            "and_v(v:0,1)",
            "and_v(v:1,0)",
            "and_b(0,a:1)",
            "and_b(1,a:0)",
            "and_n(0,1)",
            "and_n(1,0)",
            "or_b(0,a:0)",
            "or_c(0,v:0)",
            "or_d(0,0)",
            "or_i(0,0)",
            "andor(0,1,0)",
            "andor(1,0,0)",
            "thresh(1,0,a:0)",
            f"thresh(2,pk({KEY}),a:0)",
        ):
            with self.subTest(text=text):
                ms = Miniscript.from_string(text)
                ms.verify()
                self.assertFalse(is_satisfiable(ms))

    def test_satisfiable_fragments(self):
        for text in (
            "1",
            "tv:1",
            "dv:1",
            "n:1",
            "l:1",
            "u:1",
            "and_v(v:1,1)",
            "and_b(1,a:1)",
            "and_n(1,1)",
            "or_b(0,adv:1)",
            "or_c(0,v:1)",
            "or_d(0,1)",
            "or_i(0,1)",
            "or_i(1,0)",
            "andor(0,0,1)",
            "andor(1,1,0)",
            f"thresh(1,pk({KEY}),a:0)",
            f"pk({KEY})",
            f"pkh({KEY})",
            f"multi(1,{KEY})",
            "older(1)",
            "after(1)",
            f"sha256({'00' * 32})",
            f"hash256({'00' * 32})",
            f"ripemd160({'00' * 20})",
            f"hash160({'00' * 20})",
        ):
            with self.subTest(text=text):
                ms = Miniscript.from_string(text)
                ms.verify()
                self.assertTrue(is_satisfiable(ms))

    def test_deeply_nested_ast(self):
        ms = JustZero()
        for _ in range(2000):
            ms = N(ms)
        self.assertFalse(is_satisfiable(ms))


class DescriptorParseTest(unittest.TestCase):
    # module.cpp reads an OK value of "1" as accepted and anything else as
    # rejected, so check the value along with the status.
    def assertResult(self, data, status, value=b""):
        result = descriptor_parse(data)
        self.assertEqual((result.status, result.value), (status, value))

    def test_unsatisfiable_descriptors(self):
        for text in (
            "wsh(jjdv:0)",
            "sh(wsh(jjdv:0))",
            "wsh(and_v(v:0,1))",
        ):
            with self.subTest(text=text):
                self.assertResult(text.encode(), SKIP)

    def test_satisfiable_descriptors(self):
        for text in (
            f"wpkh({KEY})",
            f"pkh({KEY})",
            f"sh(wpkh({KEY}))",
            f"wsh(pk({KEY}))",
            f"sh(wsh(pk({KEY})))",
            f"wsh(or_i(0,pk({KEY})))",
            f"tr({XKEY})",
            f"tr({XKEY},pk({XKEY}))",
            f"tr({XKEY},{{pk({XKEY}),{{pk({XKEY}),pk({XKEY})}}}})",
        ):
            with self.subTest(text=text):
                self.assertResult(text.encode(), OK, b"1")

    def test_invalid_descriptors(self):
        for data in (b"", b"wsh(", b"wsh(unknown(0))", b"\xff"):
            with self.subTest(data=data):
                self.assertResult(data, FAIL, b"0")

    def test_miniscript_parse_retains_unsatisfiable_inputs(self):
        for text in (b"0", b"jjdv:0", b"and_v(v:0,1)"):
            with self.subTest(text=text):
                self.assertTrue(miniscript_parse(text))


if __name__ == "__main__":
    unittest.main()
