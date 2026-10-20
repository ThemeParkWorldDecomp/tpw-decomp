"""Unit v51116: fifteen advisor scorers (a balance value of the scoring object, or 0), ~CAdvisorBalance, CMsgTag::GetType and ~CMsgTag, and the
sound library's inline byte swaps, against libtpw_unit_CAdvisorScoring__ADVISOR_AWARD_MOVED.dylib, main app ("SimTheme Park"):
  the scorers 0x10011358..0x100160d0, ~CAdvisorBalance 0x1001010c, CMsgTag 0x1000e90c/0x1000e2d0, SamsUtilities USwap32/USwapInplace32/USwapBlock32.

A case is a world and a call. The scorers read a scoring object whose 393 balance slots hold random words (Mac slot i at 0x14 + 4 i, the host's
slots at the layout's offset and stride). The destructors call the fakes UnInit (0x1001ed40) and operator delete (0x10238218); USwapBlock32 calls
_USwapBlock32 (0x10239a78), all recorded on both sides. Compared: the answer, the log, the object's words where the host has the same word.
No fused operations. Not tested: the Mac's vptr store at +4 of ~CAdvisorBalance (the host's vptr is its own); a delete flag with bits above 16 (the
host takes a short, like the callers).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CAdvisorScoring__ADVISOR_AWARD_MOVED"
MASK = 0xFFFFFFFF

SCORERS = [
    ("_ADVISOR_AWARD_MOVED", 0x100160d0), ("_ADVISOR_AWARD_GIVEN", 0x1001608c), ("_ADVISOR_GENERAL_WON_GOLDEN_TICKET", 0x10016038),
    ("_ADVISOR_RESEARCH_FEATURE_RESEARCHED", 0x10015f14), ("_ADVISOR_RESEARCH_SIDESHOW_RESEARCHED", 0x10015ebc),
    ("_ADVISOR_RESEARCH_SHOP_RESEARCHED", 0x10015e68), ("_ADVISOR_RESEARCH_ADDON_RESEARCHED", 0x10015e14),
    ("_ADVISOR_RESEARCH_RIDE_RESEARCHED", 0x10015dc0), ("_ADVISOR_FEATURES_BUILD_REST_AREA", 0x10015bc0),
    ("_ADVISOR_FEATURES_FAR_TO_TOILETS", 0x10015514), ("_ADVISOR_RIDES_BREAKDOWN_MECHANIC_ON_IT", 0x10014500),
    ("_ADVISOR_STAFF_GUARD_COVERAGE_LOST_WORLD", 0x10012d7c), ("_ADVISOR_STAFF_GUARD_COVERAGE", 0x10012d2c),
    ("_ADVISOR_WELCOME", 0x1001139c), ("_ADVISOR_NO_SCORE", 0x10011358),
]
FN_BAL_DTOR, FN_MSG_TYPE, FN_MSG_DTOR = 0x1001010c, 0x1000e90c, 0x1000e2d0
SWAP32 = [("SamsUtilities::USwap32<tagBUMPRIDE*>", 0x10021bf8), ("SamsUtilities::USwap32<tabCOLOBJECT*>", 0x10021c6c),
          ("SamsUtilities::USwap32<tagBUMP*>", 0x10021ce0), ("SamsUtilities::USwap32<tagPEEPOFF*>", 0x10021d48),
          ("SamsUtilities::USwap32<long*>", 0x10009ad0)]
INPLACE = [("SamsUtilities::USwapInplace32<long>@0x10021db8", 0x10021db8), ("SamsUtilities::USwapInplace32<unsigned_long>@0x10021dfc", 0x10021dfc),
           ("SamsUtilities::USwapInplace32<long>@0x10009c74", 0x10009c74), ("SamsUtilities::USwapInplace32<int>", 0x10009c30),
           ("SamsUtilities::USwapInplace32<unsigned_long>@0x10009b2c", 0x10009b2c)]
BLOCK = [("SamsUtilities::USwapBlock32<unsigned_long>", 0x10009bd0), ("SamsUtilities::USwapBlock32<long>", 0x10009b74)]
SIZES = {a: 8 for _, a in SCORERS}
SIZES.update({FN_BAL_DTOR: 100, FN_MSG_TYPE: 8, FN_MSG_DTOR: 76})
SIZES.update({a: 40 for _, a in SWAP32})
SIZES.update({a: 12 for _, a in INPLACE})
SIZES.update({a: 36 for _, a in BLOCK})

STUBS = CL.Stubs()
STUBS.real.update(SIZES)


def stub_uninit(e):
    e.cl.rec("UnInit", (e.cl.lab(e.r(3)),))


def stub_delete(e):
    e.cl.rec("OperatorDelete", (e.cl.lab(e.r(3)),))


def stub_block(e):
    e.cl.rec("USwapBlock32", (e.cl.lab(e.r(3)), T.s32(e.r(4))))


STUBS.internal(0x1001ed40, stub_uninit)
STUBS.internal(0x10238218, stub_delete)
STUBS.internal(0x10239a78, stub_block)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_v51116_alloc.restype, lib.tpw_v51116_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v51116_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_v51116_score.restype, lib.tpw_v51116_score.argtypes = C.c_uint32, [C.c_int32, C.c_uint64]
    lib.tpw_v51116_bal_dtor.restype, lib.tpw_v51116_bal_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    lib.tpw_v51116_msg_dtor.restype, lib.tpw_v51116_msg_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    lib.tpw_v51116_msg_type.restype, lib.tpw_v51116_msg_type.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_v51116_swap32.restype, lib.tpw_v51116_swap32.argtypes = C.c_uint32, [C.c_int32, C.c_uint32]
    lib.tpw_v51116_inplace.restype, lib.tpw_v51116_inplace.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_v51116_block.restype, lib.tpw_v51116_block.argtypes = None, [C.c_int32, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 8)()
    n = lib.tpw_v51116_layout(out)
    lib.layout = list(out)[:n]
    lib.bound = True


def ppc_call(e, case, addr, args, then=None):
    e.cl = CL.PpcLog({}, case.get("queues", {}))
    saved = STUBS.activate(e)
    try:
        r3 = T.call_ppc(e, addr, args)[0] & MASK
        extra = then() if then else {}
    finally:
        CL.Stubs.deactivate(e, saved)
    return r3, e.cl.log, extra


def host_begin(lib, case):
    bind(lib)
    CL.host_begin(lib, {}, case.get("queues", {}))


# ------------------------------------------------------------------------------------------------------ scorers
def score_ppc(addr):
    def f(e, case):
        obj = e.alloc(0x700)
        e.write(obj + 0x14, b"".join(struct.pack(">I", w) for w in case["words"]))
        r3, log, _ = ppc_call(e, case, addr, [obj])
        return {"answer": r3, "log": log}
    return f


def score_host(which):
    def f(lib, case):
        host_begin(lib, case)
        size, off, stride = lib.layout
        obj = lib.tpw_v51116_alloc(size)
        for i, w in enumerate(case["words"]):
            C.memmove(obj + off + stride * i, struct.pack("<I", w), 4)
        r = lib.tpw_v51116_score(which, obj)
        return {"answer": r, "log": CL.host_end(lib)}
    return f


def score_case(r, kind=None):
    n = 393
    k = r.random() if kind is None else kind
    if k == 0:
        words = [0] * n
    elif k == 1:
        words = [MASK] * n
    else:
        words = [r.choice([0, 1, MASK, 0x80000000, r.getrandbits(32)]) for _ in range(n)]
    return {"words": words, "queues": {}}


def score_edges(r_seed):
    def g():
        r = random.Random(r_seed)
        yield score_case(r, 0)
        yield score_case(r, 1)
        yield score_case(r, 2)
    return g


def score_rnd(r):
    return score_case(r)


# ------------------------------------------------------------------------------------------------------ destructors
FLAGS = [0, 1, -1, 2, 0x7FFF, -0x8000, 0x100, -2]


def dtor_case(r, flag=None, null=None):
    return {"flag": r.choice(FLAGS) if flag is None else flag, "null": (r.random() < 0.2) if null is None else null, "queues": {}}


def dtor_edges():
    r = random.Random(11)
    for null in (True, False):
        for f in FLAGS:
            yield dtor_case(r, f, null)


def bal_ppc(e, case):
    obj = 0 if case["null"] else e.alloc(0x20)
    r3, log, _ = ppc_call(e, case, FN_BAL_DTOR, [obj, case["flag"] & 0xFFFF])
    return {"same": int(r3 == obj), "log": log}


def bal_host(lib, case):
    host_begin(lib, case)
    obj = 0 if case["null"] else lib.tpw_v51116_alloc(0x20)
    r = lib.tpw_v51116_bal_dtor(obj, case["flag"])
    return {"same": int(r == obj), "log": CL.host_end(lib)}


def msg_ppc(e, case):
    obj = 0 if case["null"] else e.alloc(8, struct.pack(">I", 0x55667788))
    box = {}

    def rd():
        return {"word": struct.unpack(">I", e.read(obj, 4))[0] if obj else 0}
    r3, log, extra = ppc_call(e, case, FN_MSG_DTOR, [obj, case["flag"] & 0xFFFF], rd)
    return {"same": int(r3 == obj), "log": log, "word": extra["word"]}


def msg_host(lib, case):
    host_begin(lib, case)
    obj = 0 if case["null"] else lib.tpw_v51116_alloc(8)
    if obj:
        C.memmove(obj, struct.pack("<I", 0x55667788), 4)
    r = lib.tpw_v51116_msg_dtor(obj, case["flag"])
    w = struct.unpack("<I", C.string_at(obj, 4))[0] if obj else 0
    return {"same": int(r == obj), "log": CL.host_end(lib), "word": w}


def type_ppc(e, case):
    obj = e.alloc(8)
    r3, log, _ = ppc_call(e, case, FN_MSG_TYPE, [obj])
    return {"answer": r3, "log": log}


def type_host(lib, case):
    host_begin(lib, case)
    obj = lib.tpw_v51116_alloc(8)
    return {"answer": lib.tpw_v51116_msg_type(obj) & MASK, "log": CL.host_end(lib)}


def type_edges():
    yield {"queues": {}}


def type_rnd(r):
    return {"queues": {}}


# ------------------------------------------------------------------------------------------------------ swaps
WORDS = [0, 1, MASK, 0x80000000, 0x7FFFFFFF, 0x12345678, 0xDEADBEEF, 0x000000FF, 0xFF000000, 0x00FF00FF]


def word_case(r, v=None):
    return {"v": r.choice(WORDS) if v is None else v, "queues": {}}


def word_edges():
    r = random.Random(5)
    for v in WORDS:
        yield word_case(r, v)


def word_rnd(r):
    return word_case(r, r.getrandbits(32))


def swap_ppc(addr):
    def f(e, case):
        r3, log, _ = ppc_call(e, case, addr, [case["v"]])
        return {"answer": r3, "log": log}
    return f


def swap_host(which):
    def f(lib, case):
        host_begin(lib, case)
        return {"answer": lib.tpw_v51116_swap32(which, case["v"]), "log": CL.host_end(lib)}
    return f


def inplace_ppc(addr):
    def f(e, case):
        blk = e.alloc(8, struct.pack(">II", case["v"], 0xA5A5A5A5))
        r3, log, extra = ppc_call(e, case, addr, [blk], lambda: {"w": list(struct.unpack(">II", e.read(blk, 8)))})
        return {"words": extra["w"], "log": log}
    return f


def inplace_host(which):
    def f(lib, case):
        host_begin(lib, case)
        blk = lib.tpw_v51116_alloc(8)
        C.memmove(blk, struct.pack("<II", case["v"], 0xA5A5A5A5), 8)
        lib.tpw_v51116_inplace(which, blk)
        return {"words": list(struct.unpack("<II", C.string_at(blk, 8))), "log": CL.host_end(lib)}
    return f


def block_case(r, count=None, null=None):
    return {"count": r.choice([0, 1, 2, 7, -1, 0x7FFFFFFF, -0x80000000]) if count is None else count,
            "null": (r.random() < 0.25) if null is None else null, "queues": {}}


def block_edges():
    r = random.Random(9)
    for null in (False, True):
        for c in (0, 1, 3, -1, 0x7FFFFFFF, -0x80000000):
            yield block_case(r, c, null)


def block_ppc(addr):
    def f(e, case):
        blk = 0 if case["null"] else e.alloc(16, struct.pack(">IIII", 1, 2, 3, 4))
        r3, log, _ = ppc_call(e, case, addr, [blk, case["count"] & MASK])
        return {"log": log, "words": list(struct.unpack(">IIII", e.read(blk, 16))) if blk else []}
    return f


def block_host(which):
    def f(lib, case):
        host_begin(lib, case)
        blk = 0 if case["null"] else lib.tpw_v51116_alloc(16)
        if blk:
            C.memmove(blk, struct.pack("<IIII", 1, 2, 3, 4), 16)
        lib.tpw_v51116_block(which, blk, case["count"])
        return {"log": CL.host_end(lib), "words": list(struct.unpack("<IIII", C.string_at(blk, 16))) if blk else []}
    return f


TESTS = []
for i, (name, addr) in enumerate(SCORERS):
    TESTS.append(FnTest("CAdvisorScoring::" + name, score_ppc(addr), score_host(i), edge=score_edges(100 + i), rnd=score_rnd))
TESTS.append(FnTest("CAdvisorBalance::~CAdvisorBalance", bal_ppc, bal_host, edge=dtor_edges, rnd=dtor_case))
TESTS.append(FnTest("CMsgTag::GetType", type_ppc, type_host, edge=type_edges, rnd=type_rnd))
TESTS.append(FnTest("CMsgTag::~CMsgTag", msg_ppc, msg_host, edge=dtor_edges, rnd=dtor_case))
for i, (name, addr) in enumerate(SWAP32):
    TESTS.append(FnTest(name, swap_ppc(addr), swap_host(i), edge=word_edges, rnd=word_rnd))
for i, (name, addr) in enumerate(INPLACE):
    TESTS.append(FnTest(name, inplace_ppc(addr), inplace_host(i), edge=word_edges, rnd=word_rnd))
for i, (name, addr) in enumerate(BLOCK):
    TESTS.append(FnTest(name, block_ppc(addr), block_host(i), edge=block_edges, rnd=block_case))
