"""Unit v5260, TbIResourceIndex::ReleaseResource and ReleaseItem, against libtpw_unit_TbIResourceIndex_ReleaseResource.dylib
(tests/unit_TbIResourceIndex_ReleaseResource.cpp), ltms, src/ltms/TbIResourceIndex.cpp:
  ReleaseResource 0x100036d8, ReleaseItem 0x1000362c

A case is an item (or NULL) whose resource slot (Mac +8) is NULL or a fake object. The virtual call (vtable +0x14, through __ptr_glue) is
faked on both sides and records the object. Compared: the event log, the item's id/type words and its resource slot afterwards.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_TbIResourceIndex_ReleaseResource"
MASK = 0xFFFFFFFF

FN_RES, FN_ITEM = 0x100036D8, 0x1000362C
GLUE = 0x1000F91C
SIZES = {FN_RES: 84, FN_ITEM: 80}
OWN = set(SIZES)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_glue(e):
    e.cl.rec("ManagerRelease", (e.cl.lab(e.r(3)),))


STUBS.internal(GLUE, stub_glue)

OBJ = {}


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    vt = T.persist_alloc(emu, 0x20, b"".join(struct.pack(">I", 0x11110000 + i) for i in range(8)))
    OBJ["a"] = T.persist_alloc(emu, 0x10, struct.pack(">II", vt, 1))


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({OBJ["a"]: 1}, {})
        self.saved = STUBS.activate(e)
        self.size = 12

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def obj(self):
        return OBJ["a"]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def put_ptr(self, a, v):
        self.words(a, [v])

    def get_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def log(self):
        return self.L.log

    def call(self, op, item):
        entry = FN_RES if op == "res" else FN_ITEM
        T.call_ppc(self.e, entry, [0x1000, item & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.obj()}, {})
        self.size = 16

    def done(self):
        pass

    def obj(self):
        return self.lib.tpw_rr_addr(b"obj")

    def alloc(self, n):
        return self.lib.tpw_rr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def put_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, item):
        (self.lib.tpw_rr_release_resource if op == "res" else self.lib.tpw_rr_release_item)(item)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rr_addr.restype, lib.tpw_rr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rr_alloc.restype, lib.tpw_rr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rr_release_resource.argtypes = [C.c_uint64]
    lib.tpw_rr_release_item.argtypes = [C.c_uint64]
    out = (C.c_int64 * 8)()
    n = lib.tpw_rr_layout(out)
    assert list(out)[:n] == [8], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        item = 0
        if case["item"]:
            item = s.alloc(s.size)
            s.words(item, [case["id"], case["tw"]])
            s.put_ptr(item + 8, s.obj() if case["res"] else 0)
        s.call(case["op"], item)
        out = {"log": s.log()}
        if item:
            out["words"] = s.rd_words(item, 2)
            p = s.get_ptr(item + 8)
            out["res"] = 0 if p == 0 else (1 if p == s.obj() else -1)
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(op):
    def one(r, item, res):
        return {"op": op, "item": item, "res": res, "id": r.getrandbits(32), "tw": r.getrandbits(32)}

    def edges():
        r = random.Random(5260 if op == "res" else 5261)
        for item in (0, 1):
            for res in (0, 1):
                yield one(r, item, res)

    def rnd(r):
        return one(r, r.choice([0, 1, 1]), r.choice([0, 1]))

    return edges, rnd


_re, _rr = make("res")
_ie, _ir = make("item")

TESTS = [
    FnTest("TbIResourceIndex::ReleaseResource", ppc, host, edge=_re, rnd=_rr),
    FnTest("TbIResourceIndex::ReleaseItem", ppc, host, edge=_ie, rnd=_ir),
]
