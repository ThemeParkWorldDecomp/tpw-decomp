"""Unit v51068, thirty CAdvisorScoring scorers that answer one word of the object (16 of them) or a constant (14), against
libtpw_unit_CAdvisorScoring__ADVISOR_NEW_RESEARCH_GROUP_SIDE.dylib, main app.

A case is an object block of 0x140 random words written at Mac offsets on both sides; the function is called with this = block. What is
compared: the answer and the whole block read back (nothing may change). No callees, no fused operations.
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_NEW_RESEARCH_GROUP_SIDE"
MASK = 0xFFFFFFFF
NWORDS = 0x140

FUNCS = [
    ("_ADVISOR_PRANK_SBOMB", 0x100176b8), ("_ADVISOR_PRANK_LITTER", 0x100176fc), ("_ADVISOR_PRANK_BALLOON", 0x10017744),
    ("_ADVISOR_PARK_NOW_OPEN", 0x1001778c), ("_ADVISOR_PARK_NOW_CLOSED", 0x100177d4),
    ("_ADVISOR_UPGRADE_HIRE_MECHANIC", 0x1001781c), ("_ADVISOR_UPGRADE_STIKING_MECHANICS", 0x1001786c),
    ("_ADVISOR_NEW_UPGRADE_NO_MECHANIC", 0x100178c0), ("_ADVISOR_NEW_RESEARCH_GROUP_RIDE", 0x10017910),
    ("_ADVISOR_NEW_RESEARCH_GROUP_SHOP", 0x10017960), ("_ADVISOR_NEW_RESEARCH_GROUP_SIDESHOW", 0x100179b0),
    ("_ADVISOR_NEW_RESEARCH_GROUP_FEATURE", 0x10017a04), ("_ADVISOR_BUS_ARRIVED", 0x10017a58),
    ("_ADVISOR_PLANE_ARRIVED", 0x10017a9c), ("_ADVISOR_ONLINE_NEW_ITEM", 0x10017d64), ("_ADVISOR_ONLINE_MESSAGE", 0x10017dac),
    ("_ADVISOR_CONGRAT_BIG_COASTER", 0x100184a4), ("_ADVISOR_CONGRAT_FUN_COASTER", 0x100184f0),
    ("_ADVISOR_CONGRAT_FUN_GOKART", 0x1001853c), ("_ADVISOR_CONGRAT_FUN_WATERRIDE", 0x10018588),
    ("_ADVISOR_CONGRAT_FAST_GOKART", 0x100185d8), ("_ADVISOR_CONGRAT_GIFTS", 0x10018994),
    ("_ADVISOR_CONGRAT_GOLDEN_KEY_NONE", 0x100189dc), ("_ADVISOR_CONGRAT_GOLDEN_KEY_JUNGLE", 0x10018a2c),
    ("_ADVISOR_CONGRAT_GOLDEN_KEY_HALLOWEEN", 0x10018a80), ("_ADVISOR_CONGRAT_GOLDEN_KEY_SPACE", 0x10018ad8),
    ("_ADVISOR_CONGRAT_GOLDEN_KEY_FANTASY", 0x10018b2c), ("_ADVISOR_CONGRAT_KEY_AT_START", 0x10018b80),
    ("_ADVISOR_CONGRAT_ALL_COMPLETE", 0x10018bd0), ("_ADVISOR_CONGRAT_ALL_COMPLETE_LEAVING", 0x10018c20),
]
EDGE = [0, 1, MASK, 0x80000000, 0x7FFFFFFF, 0x2710, 0x270F]


def setup(runner):
    T.install_std_stubs(runner.emu)


def make_ppc(addr):
    def ppc(e, case):
        obj = e.alloc(NWORDS * 4)
        e.write(obj, b"".join(struct.pack(">I", w) for w in case["words"]))
        r3, _, _ = T.call_ppc(e, addr, [obj])
        back = list(struct.unpack(">%dI" % NWORDS, e.read(obj, NWORDS * 4)))
        return {"answer": r3 & MASK, "words": back}
    return ppc


def make_host(name):
    def host(lib, case):
        fn = getattr(lib, "tpw_advs_" + name)
        fn.restype, fn.argtypes = C.c_uint32, [C.c_uint64]
        lib.tpw_advs_alloc.restype, lib.tpw_advs_alloc.argtypes = C.c_uint64, [C.c_uint32]
        obj = lib.tpw_advs_alloc(NWORDS * 4)
        C.memmove(obj, b"".join(struct.pack("<I", w) for w in case["words"]), NWORDS * 4)
        ans = fn(obj)
        back = list(struct.unpack("<%dI" % NWORDS, C.string_at(obj, NWORDS * 4)))
        return {"answer": ans & MASK, "words": back}
    return host


def edges():
    r = random.Random(7)
    for v in EDGE:
        yield {"words": [v] * NWORDS}
    for v in EDGE:
        yield {"words": [r.getrandbits(32) if i % 2 else v for i in range(NWORDS)]}


def rnd(r):
    return {"words": [r.getrandbits(32) for _ in range(NWORDS)]}


TESTS = [FnTest(n, make_ppc(a), make_host(n), edge=edges, rnd=rnd) for n, a in FUNCS]
