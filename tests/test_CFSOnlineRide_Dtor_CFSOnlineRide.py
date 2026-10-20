"""Unit v5741, CFSOnlineRide::~CFSOnlineRide, against libtpw_unit_CFSOnlineRide_Dtor_CFSOnlineRide.dylib
(tests/unit_CFSOnlineRide_Dtor_CFSOnlineRide.cpp), main app, src/CFSOnlineRide_Dtor_CFSOnlineRide.cpp:
  CFSOnlineRide::~CFSOnlineRide 0x1017cbf4

A case is a ride list (up to 6 nodes holding rides 0..4, ride 0 is the one destroyed, maybe absent or repeated) and a delete flag. The
list code (Find 0x1017cdf4, RemoveAt 0x1017cf24) runs as the original on the PowerPC side and as the real template on the host. The three
string destructors and operator delete are fakes on both sides recording the same events.

Compared: the answer (this), the event log, the list afterwards (count, head, tail, the ids in order). Not tested: the vtable store (the
host has its own vtable); a flag whose delete frees memory (the fake delete frees nothing on both sides).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineRide_Dtor_CFSOnlineRide"
MASK = 0xFFFFFFFF

FN = 0x1017CBF4
FIND, REMOVE_AT = 0x1017CDF4, 0x1017CF24
STR_DTOR, OP_DELETE = 0x10238710, 0x10238218

STUBS = CL.Stubs()
STUBS.real.update({FN, FIND, REMOVE_AT})
STATE = {}


def stub_str_dtor(e):
    e.cl.rec("StrDtor", (T.s32(e.r(3) - STATE["ride"]), T.s32(e.r(4))))
    e.set_r(3, e.r(3))


def stub_delete(e):
    pass


STUBS.internal(STR_DTOR, stub_str_dtor)
STUBS.internal(OP_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc(e, case):
    L = e.cl = CL.PpcLog({}, {})
    saved = STUBS.activate(e)
    try:
        def w(a, ws):
            e.write(a, b"".join((x & MASK).to_bytes(4, "big") for x in ws))
        sysl = e.alloc(0x40)
        lst = e.alloc(0x10)
        rides = [e.alloc(0x6C) for _ in range(5)]
        for r in rides:
            w(r + 4, [sysl])
        w(sysl + 0x2C, [lst])
        prev = 0
        head = 0
        for i in case["ids"]:
            node = e.alloc(12)
            w(node, [rides[i], 0, prev])
            if prev:
                w(prev + 4, [node])
            else:
                head = node
            prev = node
        w(lst + 4, [head, prev, len(case["ids"])])
        STATE["ride"] = rides[0]
        this = 0 if case["null"] else rides[0]
        r3, _, _ = T.call_ppc(e, FN, [this, case["flag"] & MASK])
        ident = {r: i for i, r in enumerate(rides)}
        node_of = lambda p: ident.get(e.u32(p), -99)
        head, tail, count = e.u32(lst + 4), e.u32(lst + 8), e.u32(lst + 12)
        seq = []
        p = head
        while p and len(seq) < 16:
            seq.append(node_of(p))
            p = e.u32(p + 4)
        seq += [-1] * (16 - len(seq))
        out = [count, node_of(head) if head else -1, node_of(tail) if tail else -1] + seq
        return {"answer": 1 if r3 == this and this else 0, "log": L.log, "list": [T.s32(x) for x in out]}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    fn = lib.tpw_ride_run
    fn.restype = C.c_int32
    fn.argtypes = [C.POINTER(C.c_int32), C.c_int32, C.c_uint32, C.c_int32, C.POINTER(C.c_int32)]
    CL.host_begin(lib, {}, {})
    ids = (C.c_int32 * 8)(*case["ids"])
    out = (C.c_int32 * 19)()
    ans = fn(ids, len(case["ids"]), case["flag"] & MASK, 1 if case["null"] else 0, out)
    return {"answer": ans, "log": CL.host_end(lib), "list": list(out)}


FLAGS = [0, MASK, 1, 0x7FFF, 0x8000, 0x10000, 0x10001, 0xFFFF]


def mk(ids, flag=0, null=False):
    return {"ids": ids, "flag": flag, "null": null}


def edges():
    for flag in FLAGS:
        for ids in ([], [0], [1], [0, 1], [1, 0], [1, 0, 2], [1, 2, 3], [0, 0], [2, 0, 0, 3], [1, 2, 3, 4, 0]):
            yield mk(ids, flag)
    yield mk([0, 1], 1, True)
    yield mk([], 0, True)


def rnd(r):
    return mk([r.randrange(5) for _ in range(r.randrange(7))], r.choice(FLAGS + [r.randrange(1 << 32)]), r.random() < 0.05)


TESTS = [
    FnTest("CFSOnlineRide::~CFSOnlineRide", ppc, host, edge=edges, rnd=rnd),
]
