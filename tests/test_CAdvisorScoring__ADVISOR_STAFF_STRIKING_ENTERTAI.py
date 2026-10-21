"""Unit v51055, the CAdvisorScoring word getters (lwz r3, off(r3); blr) and the two that return 0, against
libtpw_unit_CAdvisorScoring__ADVISOR_STAFF_STRIKING_ENTERTAI.dylib, main app, src/game/advisor/AdvisorScoring.cpp.

A case is a 0x260-byte object of random words. The same bytes are written on both sides; compared: the answer and the whole object
after the call. No callees, no fused operations. Not tested: nothing.
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_STAFF_STRIKING_ENTERTAI"
MASK = 0xFFFFFFFF
SIZE = 0x260

# (name, address, offset or None for "returns 0")
FUNCS = [
    ("_ADVISOR_STAFF_STRIKING_ENTERTAINERS", 0x10013980, 0x1f0),
    ("_ADVISOR_STAFF_STRIKING_GUARDS", 0x10013930, 0x1ec),
    ("_ADVISOR_STAFF_STRIKING_RESEARCHERS", 0x100139d4, 0x1f4),
    ("_ADVISOR_STAFF_STRIKING_HANDYMEN", 0x100138e0, 0x1e8),
    ("_ADVISOR_STAFF_HAPPIER_MECHANICS", 0x10013a28, 0x1f8),
    ("_ADVISOR_STAFF_STRIKING_MECHANICS", 0x1001388c, 0x1e4),
    ("_ADVISOR_STAFF_HAPPIER_HANDYMEN", 0x10013a78, 0x1fc),
    ("_ADVISOR_STAFF_HAPPIER_GUARDS", 0x10013ac8, 0x200),
    ("_ADVISOR_STAFF_VERY_UNHAPPY_RESEARCHERS", 0x10013834, 0x1e0),
    ("_ADVISOR_STAFF_HAPPIER_ENTERTAINERS", 0x10013b18, 0x204),
    ("_ADVISOR_STAFF_VERY_UNHAPPY_ENTERTAINERS", 0x100137dc, 0x1dc),
    ("_ADVISOR_STAFF_HAPPIER_RESEARCHERS", 0x10013b6c, 0x208),
    ("_ADVISOR_STAFF_VERY_UNHAPPY_GUARDS", 0x10013788, 0x1d8),
    ("_ADVISOR_STAFF_STRIKE_END_BAD_MECHANICS", 0x10013bc0, 0x20c),
    ("_ADVISOR_STAFF_VERY_UNHAPPY_HANDYMEN", 0x10013734, 0x1d4),
    ("_ADVISOR_STAFF_STRIKE_END_BAD_HANDYMEN", 0x10013c18, 0x210),
    ("_ADVISOR_STAFF_VERY_UNHAPPY_MECHANICS", 0x100136dc, 0x1d0),
    ("_ADVISOR_STAFF_STRIKE_END_BAD_GUARDS", 0x10013c70, 0x214),
    ("_ADVISOR_STAFF_UNHAPPY_RESEARCHERS", 0x10013688, 0x1cc),
    ("_ADVISOR_STAFF_STRIKE_END_BAD_ENTERTAINERS", 0x10013cc4, 0x218),
    ("_ADVISOR_STAFF_UNHAPPY_ENTERTAINERS", 0x10013634, 0x1c8),
    ("_ADVISOR_STAFF_UNHAPPY_GUARDS", 0x100135e4, 0x1c4),
    ("_ADVISOR_STAFF_STRIKE_END_BAD_RESEARCHERS", 0x10013d20, 0x21c),
    ("_ADVISOR_STAFF_UNHAPPY_HANDYMEN", 0x10013594, 0x1c0),
    ("_ADVISOR_RIDES_LONG_QUEUES_BORED_VISITORS", 0x10013d7c, None),
    ("_ADVISOR_STAFF_UNHAPPY_MECHANICS", 0x10013544, 0x1bc),
    ("_ADVISOR_RIDES_USE_UPGRADES", 0x100143b0, None),
    ("_ADVISOR_RIDES_BREAKDOWN_IMMINENT", 0x100143fc, 0x248),
    ("_ADVISOR_RIDES_BREAKDOWN_NO_MECHANICS", 0x10014450, 0x24c),
    ("_ADVISOR_RIDES_BREAKDOWN_BUSY_MECHANICS", 0x100144a8, 0x250),
]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_adv_call.restype, lib.tpw_adv_call.argtypes = C.c_uint32, [C.c_int32, C.c_uint64]
    lib.tpw_adv_alloc.restype, lib.tpw_adv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.bound = True


def make(idx):
    name, addr, off = FUNCS[idx]

    def ppc(e, case):
        a = e.alloc(SIZE)
        e.write(a, b"".join(struct.pack(">I", w) for w in case["words"]))
        r3, _, _ = T.call_ppc(e, addr, [a])
        return {"answer": r3 & MASK, "obj": list(struct.unpack(">%dI" % (SIZE // 4), e.read(a, SIZE)))}

    def host(lib, case):
        bind(lib)
        a = lib.tpw_adv_alloc(SIZE)
        C.memmove(a, b"".join(struct.pack("<I", w) for w in case["words"]), SIZE)
        r = lib.tpw_adv_call(idx, a) & MASK
        return {"answer": r, "obj": list(struct.unpack("<%dI" % (SIZE // 4), C.string_at(a, SIZE)))}

    return ppc, host


def case(r):
    return {"words": [r.choice([0, MASK, 0x80000000, 0x7FFFFFFF, r.getrandbits(32)]) for _ in range(SIZE // 4)]}


def edges():
    r = random.Random(51055)
    for _ in range(4):
        yield case(r)


def rnd(r):
    return case(r)


def _host_wrap(host):
    # the host objects are little-endian: the words are written byte-swapped, so the answer and the object compare as the Mac's
    return host


TESTS = []
for i, (n, a, o) in enumerate(FUNCS):
    p, h = make(i)
    TESTS.append(FnTest("CAdvisorScoring::" + n, p, h, edge=edges, rnd=rnd))
