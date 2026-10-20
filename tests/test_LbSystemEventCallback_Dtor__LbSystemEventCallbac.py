"""Unit v5461, the system event callback object of bullfrog LbSCBack.cpp, against
libtpw_unit_LbSystemEventCallback_Dtor__LbSystemEventCallbac.dylib (tests/unit_LbSystemEventCallback_Dtor__LbSystemEventCallbac.cpp):
  _LbSystemEventCallback::~_LbSystemEventCallback 0x10014564, EventNotification 0x1001462c, DeleteAllCallbacks 0x100147c8

A case is a world and a call. The world is built twice by ONE piece of Python code: the object (six lists of callback nodes, the critical
section pointer at 0x60) in emulator memory (big-endian, 4-byte pointers, lists of 0x10 bytes) and in the host library's memory
(little-endian, 8-byte pointers, lists of 0x20 bytes). The callees outside the unit are fakes on both sides recording the same events:
Enter/Leave/DeleteCriticalSection, operator delete, __destroy_arr, the list's RemoveHead (only the count moves; it answers entry
addresses from the case's queue) and the registered callback (the Mac's __ptr_glue).

What is compared: the answer (this), the event log, the six list counts and whether the critical section pointer is NULL afterwards.
No fused operations. Not tested: an event type outside 0..6 (the original reads before or past the lists).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbSystemEventCallback_Dtor__LbSystemEventCallbac"
MASK = 0xFFFFFFFF

FN_DTOR, FN_EVENT, FN_DELETEALL = 0x10014564, 0x1001462c, 0x100147c8
SIZES = {FN_DTOR: 148, FN_EVENT: 184, FN_DELETEALL: 172}
OWN = set(SIZES)

ENTER, LEAVE, DELCS, DELETE, DESTROY_ARR, PTR_GLUE, REMOVE_HEAD = (0x1006bd6c, 0x1006bd84, 0x1006c0b4, 0x1006b2d4, 0x1006c0cc,
                                                                   0x1006c12c, 0x10014a78)
DTOR_VECTOR = 0x100700c4
HOST_LAYOUT = [8, 16, 24, 32, 24, 192, 200, 8, 16]
K = 4                       # entries per world


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_enter(e):
    e.cl.rec("Enter", (e.cl.lab(e.r(3)),))


def st_leave(e):
    e.cl.rec("Leave", (e.cl.lab(e.r(3)),))


def st_delcs(e):
    e.cl.rec("DeleteCS", (e.cl.lab(e.r(3)),))


def st_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def st_destroy(e):
    e.cl.rec("destroy_arr", (e.cl.lab(e.r(3)), 1 if e.r(4) == DTOR_VECTOR else 0, s32(e.r(5)), s32(e.r(6))))


def st_ptr_glue(e):
    e.cl.rec("ptr_glue", (s32(e.r(3)), e.r(4) & MASK, e.r(5) & MASK))


def st_remove_head(e):
    L = e.cl
    this = e.r(3)
    L.rec("RemoveHead", (L.lab(this),))
    count = e.u32(this + 12)
    if 0 < count < 0x80000000:
        e.write(this + 12, struct.pack(">I", count - 1))
    e.set_r(3, L.ret("RemoveHead") & MASK)


for _a, _f in ((ENTER, st_enter), (LEAVE, st_leave), (DELCS, st_delcs), (DELETE, st_delete), (DESTROY_ARR, st_destroy),
               (PTR_GLUE, st_ptr_glue), (REMOVE_HEAD, st_remove_head)):
    def _glue(e, _f=_f):
        # the import glue saves the caller's TOC at 0x14(r1); the code reloads r2 from there after the call
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        _f(e)
    STUBS.internal(_a, _glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def make(self, case):
        """Allocates the world's blocks (the addresses the log's labels need) with this side's layout."""
        pw = self.PW
        self.stride, self.csoff, size = 4 * pw, 6 * 4 * pw, 6 * 4 * pw + pw
        self.obj = self.alloc(size + 16)
        self.cs = self.alloc(16)
        self.ents = [self.alloc(2 * pw) for _ in range(K)]
        self.nodes = [[self.alloc(3 * pw) for _ in lst] for lst in case["lists"]]

    def fixed(self):
        f = {self.obj + self.stride * i: 10 + i for i in range(6)}
        f[self.cs] = 2
        for k, a in enumerate(self.ents):
            f[a] = 20 + k
        return f

    def queues(self, case):
        q = dict(case["queues"])
        q["RemoveHead"] = [0 if k == 0 else self.ents[k - 1] for k in case["rh"]]
        return q

    def put(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def word(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    PW = 4

    def __init__(self, e, case):
        self.e = e
        self.make(case)
        self.L = e.cl = CL.PpcLog(self.fixed(), self.queues(case))
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def callback(self):
        return 0x10700000

    def call(self, op, args):
        r3, _, _ = T.call_ppc(self.e, {"dtor": FN_DTOR, "event": FN_EVENT, "deleteall": FN_DELETEALL}[op], args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    PW = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make(case)
        CL.host_begin(lib, {v: k for k, v in self.fixed().items()}, self.queues(case))

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_lbse_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def callback(self):
        return self.lib.tpw_lbse_addr(b"callback")

    def call(self, op, args):
        if op == "dtor":
            return self.lib.tpw_lbse_dtor(args[0], T.s16(args[1]))
        if op == "event":
            return self.lib.tpw_lbse_event(args[0], s32(args[1]), args[2] & MASK)
        return self.lib.tpw_lbse_deleteall(args[0])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lbse_addr.restype, lib.tpw_lbse_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lbse_alloc.restype, lib.tpw_lbse_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lbse_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lbse_dtor.restype, lib.tpw_lbse_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    lib.tpw_lbse_event.restype, lib.tpw_lbse_event.argtypes = None, [C.c_uint64, C.c_int32, C.c_uint32]
    lib.tpw_lbse_deleteall.restype, lib.tpw_lbse_deleteall.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lbse_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    pw = s.PW
    for k, (ctx, cb) in enumerate(case["entries"]):
        s.put(s.ents[k], ctx)
        s.put(s.ents[k] + pw, s.callback() if cb else 0)
    for i, idxs in enumerate(case["lists"]):
        lst = s.obj + s.stride * i
        ns = s.nodes[i]
        for j, idx in enumerate(idxs):
            s.put(ns[j], s.ents[idx])
            s.put(ns[j] + pw, ns[j + 1] if j + 1 < len(ns) else 0)
            s.put(ns[j] + 2 * pw, ns[j - 1] if j > 0 else 0)
        s.put(lst + pw, ns[0] if ns else 0)
        s.put(lst + 2 * pw, ns[-1] if ns else 0)
        s.word(lst + 3 * pw, len(idxs) if case.get("count") is None else case["count"][i])
    s.put(s.obj + s.csoff, 0 if case["cs_null"] else s.cs)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(),
            "counts": [s.rd_word(s.obj + s.stride * i + 3 * s.PW) for i in range(6)],
            "cs_null": s.rd_word(s.obj + s.csoff) == 0 if s.PW == 4 else struct.unpack("<Q", s.read(s.obj + s.csoff, 8))[0] == 0}


def run(s, case):
    try:
        build(s, case)
        op = case["op"]
        if case.get("null_this"):
            this = 0
        else:
            this = s.obj
        args = {"dtor": [this, case.get("flag", 0)], "event": [this, case.get("type", 0), case.get("data", 0)],
                "deleteall": [this]}[op]
        ans = s.call(op, args)
        ans = (1 if ans == this else 0) if op == "dtor" else 0   # the other two answer nothing
        return observe(s, case, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def world(r, op, maxn=3):
    ents = [(r.getrandbits(32), r.choice([0, 1, 1])) for _ in range(K)]
    lists = [[r.randrange(K) for _ in range(r.choice([0, 0, 1, 2, maxn]))] for _ in range(6)]
    return {"op": op, "queues": {}, "entries": ents, "lists": lists, "cs_null": r.random() < 0.15,
            "rh": [r.randrange(K + 1) for _ in range(20)]}


def mk(r, op, **kw):
    c = world(r, op)
    c.update(kw)
    return c


def dtor_case(r):
    c = mk(r, "dtor", flag=r.choice([1, 0, 0xFFFF, 0x7FFF, 0x8000, r.getrandbits(16)]))
    c["null_this"] = r.random() < 0.1
    return c


def dtor_edges():
    r = random.Random(11)
    for flag in (0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000 >> 1):
        yield mk(r, "dtor", flag=flag)
        yield mk(r, "dtor", flag=flag, cs_null=True)
        yield mk(r, "dtor", flag=flag, null_this=True)
        yield mk(r, "dtor", flag=flag, lists=[[], [], [], [], [], []])


def event_case(r):
    return mk(r, "event", type=r.randrange(7), data=r.getrandbits(32))


def event_edges():
    r = random.Random(12)
    for t in range(7):
        for cs_null in (False, True):
            yield mk(r, "event", type=t, data=0, cs_null=cs_null)
            yield mk(r, "event", type=t, data=0xFFFFFFFF, cs_null=cs_null, lists=[[0, 1, 2] for _ in range(6)])
            yield mk(r, "event", type=t, data=5, cs_null=cs_null, lists=[[] for _ in range(6)])


def delete_case(r):
    return mk(r, "deleteall")


def delete_edges():
    r = random.Random(13)
    yield mk(r, "deleteall", lists=[[] for _ in range(6)])
    yield mk(r, "deleteall", lists=[[] for _ in range(6)], cs_null=True)
    yield mk(r, "deleteall", lists=[[0] for _ in range(6)])
    yield mk(r, "deleteall", lists=[[1, 2, 3] for _ in range(6)], cs_null=True)
    for i in range(6):
        yield mk(r, "deleteall", lists=[[2, 1] if j == i else [] for j in range(6)])


TESTS = [
    FnTest("_LbSystemEventCallback::~_LbSystemEventCallback", ppc, host, edge=dtor_edges, rnd=dtor_case),
    FnTest("_LbSystemEventCallback::EventNotification", ppc, host, edge=event_edges, rnd=event_case),
    FnTest("_LbSystemEventCallback::DeleteAllCallbacks", ppc, host, edge=delete_edges, rnd=delete_case),
]
