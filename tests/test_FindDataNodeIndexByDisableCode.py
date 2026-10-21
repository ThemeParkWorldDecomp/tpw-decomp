"""Unit v5L21, FindDataNodeIndexByDisableCode 0x1020017c and CTRACE 0x1006fcbc, against
libtpw_unit_FindDataNodeIndexByDisableCode.dylib (src/game/TPEmbed.cpp, src/game/TPMesh.cpp, src/game/rides/RideAnim.cpp, src/game/Debug.cpp).

FindDataNodeIndexByDisableCode: a ride set whose mesh header holds a count (0x48) and the embedded entries (0x7c, 0x14 bytes
each: flags, a word, the id at 0x08) is built in each memory; the answer is compared. The codes cover none of the used bits (the
fallback to 1), single bits, the whole mask and bits outside it; the ids repeat so that the first match counts.
CTRACE: an empty function; it is called with a level, a format and two words and must return without a call.
No callees, no fused operations.
"""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FindDataNodeIndexByDisableCode"
MASK = 0xFFFFFFFF
FN = 0x1020017C
FN_CTRACE = 0x1006FCBC
USED = 0x3DA1F82


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dc_find.restype, lib.tpw_dc_find.argtypes = C.c_int32, [C.c_uint32, C.POINTER(C.c_uint32), C.c_uint32, C.c_uint32]
    lib.tpw_dc_ctrace.restype, lib.tpw_dc_ctrace.argtypes = None, [C.c_uint32, C.c_char_p, C.c_uint32, C.c_uint32]
    lib.bound = True


def ppc(e, case):
    n = len(case["entries"])
    entries = e.alloc(0x14 * n + 0x14)
    for i, (flags, w4, w8) in enumerate(case["entries"]):
        e.write(entries + 0x14 * i, struct.pack(">5I", flags & MASK, w4 & MASK, w8 & MASK, 0, 0))
    header = e.alloc(0x80)
    e.write(header, bytes(0x80))
    e.write(header + 0x48, struct.pack(">H", n))
    e.write(header + 0x7C, struct.pack(">I", entries))
    mesh = e.alloc(0x40)
    e.write(mesh, struct.pack(">II", 0, header))
    rset = e.alloc(0x20)
    e.write(rset, struct.pack(">III", 0, 0, mesh))
    r3, _, _ = T.call_ppc(e, FN, [rset, case["code"] & MASK, case["id"] & MASK])
    return {"answer": T.s32(r3)}


def host(lib, case):
    bind(lib)
    n = len(case["entries"])
    words = (C.c_uint32 * (3 * n + 3))(*[w & MASK for ent in case["entries"] for w in ent])
    return {"answer": lib.tpw_dc_find(n, words, case["code"] & MASK, case["id"] & MASK)}


CODES = [0, 1, 2, 0x80, 0x10, USED, ~USED & MASK, 0x3DA1F83, 0x80000000, 0x40, 0x100, MASK, 0x7C, 0x2000000]
IDS = [0, 1, 0x10, 5, MASK]


def edges():
    r = random.Random(5)
    for code in CODES:
        for id_ in IDS:
            yield {"entries": [], "code": code, "id": id_}
            yield {"entries": [(1, 0, id_)], "code": code, "id": id_}
            yield {"entries": [(0x80, id_, 0), (2, 7, id_), (1, id_, id_)], "code": code, "id": id_}
            yield {"entries": [(r.choice(CODES), r.choice(IDS), r.choice(IDS)) for _ in range(6)], "code": code, "id": id_}
    # the count is an unsigned half word
    yield {"entries": [(0, 0, 0)] * 300 + [(1, 0, 3)], "code": 0, "id": 3}


def rnd(r):
    n = r.choice([0, 1, 2, 5, 12, 40])
    def word():
        return r.choice([r.choice(CODES), 1 << r.randrange(32), r.getrandbits(32)])
    ents = [(word(), r.choice(IDS + [r.getrandbits(32)]), r.choice(IDS + [r.getrandbits(32)])) for _ in range(n)]
    id_ = r.choice([e[2] for e in ents] + IDS) if ents else r.choice(IDS)
    return {"entries": ents, "code": word(), "id": id_}


def c_ppc(e, case):
    fmt = e.alloc(16)
    e.write(fmt, b"%d %s\0")
    T.call_ppc(e, FN_CTRACE, [case["level"] & MASK, fmt, case["a"] & MASK, case["b"] & MASK])
    return {"returned": 1}


def c_host(lib, case):
    bind(lib)
    # the host's CTRACE formats nothing, so the words need not be what the format names
    lib.tpw_dc_ctrace(case["level"] & MASK, b"%d %s", case["a"] & MASK, case["b"] & MASK)
    return {"returned": 1}


def c_edges():
    for level in (0, 1, 5, MASK):
        yield {"level": level, "a": 0, "b": 0}


def c_rnd(r):
    return {"level": r.getrandbits(32), "a": r.getrandbits(32), "b": r.getrandbits(32)}


TESTS = [FnTest("FindDataNodeIndexByDisableCode", ppc, host, edge=edges, rnd=rnd),
         FnTest("CTRACE", c_ppc, c_host, edge=c_edges, rnd=c_rnd)]
