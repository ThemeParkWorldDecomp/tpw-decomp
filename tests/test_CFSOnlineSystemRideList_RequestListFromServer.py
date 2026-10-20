"""Unit v5718, CFSOnlineSystemRideList::RequestListFromServer and StoreReceivedList, against
libtpw_unit_CFSOnlineSystemRideList_RequestListFromServer.dylib (tests/unit_CFSOnlineSystemRideList_RequestListFromServer.cpp), main app,
src/CFSOnlineSystemRideList_RequestListFromServer.cpp:
  RequestListFromServer 0x1017bb4c, StoreReceivedList 0x1017bbf0

A case is a world and a call, built by one piece of Python code on both memories. Callees (fakes on both sides): UDebug (length and weighted
checksum of the text), City::GetRideList (this, context, id, flag; answer from the queue), operator new (the harness stub; the host uses
the real one) and CFSOnlineRide::CFSOnlineRide (new object label, first and last word of the info, list label).

The world: a ride list of n infos (0x34 bytes each, random words, first word distinct). Compared: the answer, the event log.
No fused operations. Not tested: a NULL list (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemRideList_RequestListFromServer"
MASK = 0xFFFFFFFF

FN_REQ, FN_STORE = 0x1017bb4c, 0x1017bbf0
SIZES = {FN_REQ: 88, FN_STORE: 116}
OWN = set(SIZES)

UDEBUG, GETRIDELIST, RIDE_CTOR = 0x10000e40, 0x10239358, 0x1017ca7c

HOST_LAYOUT = [0x34, 0, 8]

THIS = {}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_udebug(e):
    s = e.cstr(e.r(3))
    b = s.encode("latin1") if isinstance(s, str) else bytes(s)
    e.cl.rec("UDebug", (len(b), sum(c * (i + 1) for i, c in enumerate(b))))


def stub_getridelist(e):
    L = e.cl
    L.rec("GetRideList", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK, T.s32(e.r(6))))
    e.set_r(3, L.ret("GetRideList") & MASK)


def stub_ride_ctor(e):
    L = e.cl
    info = e.r(4)
    L.rec("CFSOnlineRide", (L.newlab(e.r(3)), e.u32(info), e.u32(info + 0x30), L.lab(e.r(5))))


STUBS.internal(UDEBUG, stub_udebug)
STUBS.internal(GETRIDELIST, stub_getridelist)
STUBS.internal(RIDE_CTOR, stub_ride_ctor)


def setup(runner):
    T.install_std_stubs(runner.emu)
    THIS["list"] = T.persist_alloc(runner.emu, 256)
    THIS["city"] = T.persist_alloc(runner.emu, 256)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({THIS["list"]: 1, THIS["city"]: 2}, case["queues"])
        self.saved = STUBS.activate(e)
        self.list, self.city = THIS["list"], THIS["city"]

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def put_list(self, a, n, infos):
        self.words(a, [n, infos, 0, 1])

    def request(self, id_):
        r3, _, _ = T.call_ppc(self.e, FN_REQ, [self.list, self.city, id_ & MASK])
        return r3 & MASK

    def store(self, rl):
        T.call_ppc(self.e, FN_STORE, [self.list, rl])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.list = lib.tpw_rl_addr(b"list")
        self.city = lib.tpw_rl_addr(b"city")
        CL.host_begin(lib, {1: self.list, 2: self.city}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_rl_alloc(n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def put_list(self, a, n, infos):
        C.memmove(a, struct.pack("<i", n), 4)
        C.memmove(a + HOST_LAYOUT[2], struct.pack("<Q", infos), 8)

    def request(self, id_):
        return self.lib.tpw_rl_request(self.list, self.city, id_ & MASK) & MASK

    def store(self, rl):
        self.lib.tpw_rl_store(self.list, rl)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rl_addr.restype, lib.tpw_rl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rl_alloc.restype, lib.tpw_rl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rl_request.restype, lib.tpw_rl_request.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_uint32]
    lib.tpw_rl_store.restype, lib.tpw_rl_store.argtypes = None, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def execute(s, case):
    if case["op"] == "req":
        return s.request(case["id"])
    n = case["n"]
    infos = s.alloc(max(n, 1) * 0x34)
    for i in range(n):
        s.words(infos + i * 0x34, case["infos"][i])
    rl = s.alloc(32)
    s.put_list(rl, n, infos)
    return s.store(rl)


def run(s, case):
    try:
        return {"answer": execute(s, case), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def req_case(r, id_=None):
    return {"op": "req", "id": r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]) if id_ is None else id_,
            "queues": {"GetRideList": [r.choice([0, 1, -1, r.getrandbits(32)])]}}


def store_case(r, n=None):
    n = r.choice([0, 1, 2, 3, 5, 8]) if n is None else n
    return {"op": "store", "n": n, "queues": {},
            "infos": [[(0x1000 + i) ^ (r.getrandbits(8) << 24)] + [r.getrandbits(32) for _ in range(12)] for i in range(n)]}


def req_edges():
    r = random.Random(11)
    for i in (0, 1, 0xFFFFFFFF, 0x80000000):
        yield req_case(r, i)


def store_edges():
    r = random.Random(12)
    for n in (0, 1, 2, 7):
        yield store_case(r, n)


TESTS = [
    FnTest("CFSOnlineSystemRideList::RequestListFromServer", ppc, host, edge=req_edges, rnd=req_case),
    FnTest("CFSOnlineSystemRideList::StoreReceivedList", ppc, host, edge=store_edges, rnd=store_case),
]
