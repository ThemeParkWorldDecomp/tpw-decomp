"""TPDialog getters and 18-string initializer, including call/registration order."""
SHIM_LIBRARY = "legacy_testshim"
import ctypes
import struct
import tpwtest as T
from tpwtest import Glob, Test
from balance_support import (F, I32, OPTIONS, PowerPCSinitTest, RECORDS, STRINGS, STRING_DTOR,
                             STRING_VT, U32, VP, host_begin, host_end, ppc_begin, setup, sinit_check)

TEXTS = ["English", "French", "German", "Spanish", "Swedish", "Italian", "Dutch", "Polish",
         "Danish", "Japanese", "TraditionalChinese", "SimplifiedChinese", "Thai", "Korean",
         "Hebrew", "American", "Theme Park World", "Language"]


def flag_edges():
    # All combinations of the seven relevant bits, with unrelated bits set too.
    bits = [0x2000, 0x1000, 0x800, 0x4000, 8, 16, 32]
    return [dict(flags=sum(b for i, b in enumerate(bits) if n & (1 << i)) | unrelated)
            for n in range(128) for unrelated in (0, 0xffff87c7)]


GETTERS = [
    Test("GetOptionsSetting", "GetOptionsSetting__Fv", "tpw_GetOptionsSetting", lambda c: [], ret="u32",
         globs=lambda c: [Glob("flags", OPTIONS, "tpw_TPDialog_options", "I", [c["flags"]])],
         edge=flag_edges, rnd=lambda r: dict(flags=r.getrandbits(32))),
    Test("GetNumKidSprites", "GetNumKidSprites__Fv", "tpw_GetNumKidSprites", lambda c: [], ret="i32",
         globs=lambda c: [Glob("flags", OPTIONS, "tpw_TPDialog_options", "I", [c["flags"]])],
         edge=flag_edges, rnd=lambda r: dict(flags=r.getrandbits(32))),
]


def case(r):
    return dict(words=[T.rwords(r, 5) for _ in range(18)], records=[T.rwords(r, 3) for _ in range(18)],
                flags=r.getrandbits(32), mask=r.getrandbits(32))


def ppc_sinit(e, c):
    labels = {a: "string%d" % i for i, a in enumerate(STRINGS)}
    labels.update({a: "record%d" % i for i, a in enumerate(RECORDS)})
    ppc_begin(e, c, labels)
    for a, words in zip(STRINGS, c["words"]):
        e.write(a, struct.pack(">5I", *words))
    for a, words in zip(RECORDS, c["records"]):
        e.write(a, struct.pack(">3I", *words))
    e.write(OPTIONS, struct.pack(">I", c["flags"]))
    T.call_ppc(e, e.func("__sinit_TPDialog_cpp"), [])
    strings = [(e.u32(a + 4), e.cstr(e.u32(a + 8)), e.u32(a + 12), e.u32(a + 16)) for a in STRINGS]
    return dict(strings=strings, flags=e.u32(OPTIONS),
                trace=[ev for ev in e.balance_trace if ev[0] != 3], allocs=[],
                abi_ok=all(e.u32(a) == STRING_VT for a in STRINGS))


def host_sinit(lib, c):
    labels = {}
    for i in range(18):
        obj = F(lib, "tpw_TPDialog_string", VP, I32)(i)
        labels[obj] = "string%d" % i
        F(lib, "tpw_TPDialog_seed", None, I32, U32, U32, U32, U32)(i, *c["words"][i][1:])
    flags = U32.from_address(F(lib, "tpw_TPDialog_options", VP)())
    flags.value = c["flags"]
    host_begin(lib, c)
    F(lib, "tpw_sinit_TPDialog", None)()
    strings = []
    for i in range(18):
        cap, length, grow, text = U32(), U32(), U32(), ctypes.c_char_p()
        F(lib, "tpw_TPDialog_read", None, I32, VP, VP, VP, VP)(i, ctypes.byref(cap), ctypes.byref(text),
                                                            ctypes.byref(length), ctypes.byref(grow))
        strings.append((cap.value, text.value.decode("ascii"), length.value, grow.value))
    return dict(strings=strings, flags=flags.value, abi_ok=True, **host_end(lib, labels))


def ppc_sinit_check(e, c):
    expected = [(obj, STRING_DTOR, rec) for obj, rec in zip(STRINGS, RECORDS)]
    trace = []
    for i, text in enumerate(TEXTS):
        trace += [(4, "string%d" % i, text, 0),
                  (3, "string%d" % i, "string", "record%d" % i, 0)]
    return sinit_check(e, expected, c["records"], trace)


TESTS = GETTERS + [PowerPCSinitTest("__sinit_TPDialog_cpp", ppc_sinit, host_sinit, rnd=case,
                         ppc_check=ppc_sinit_check,
                         edge=lambda: [dict(words=[[v] * 5 for _ in range(18)], records=[[v] * 3 for _ in range(18)],
                                            flags=v, mask=v) for v in (0, 0xffffffff)])]
