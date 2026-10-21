"""Unit v5542, CFSOnlinePark::CFSOnlinePark(info, list) and ~CFSOnlinePark, against libtpw_unit_CFSOnlinePark_CFSOnlinePark.dylib
(tests/unit_CFSOnlinePark_CFSOnlinePark.cpp), main app, src/game/online/FS_OnlineList.cpp:
  CFSOnlinePark::CFSOnlinePark 0x1017c0d8, CFSOnlinePark::~CFSOnlinePark 0x1017c464

The world is built twice by one piece of Python code (emulator memory, big-endian; host library memory, little-endian). The callees
(string constructors, destructors and the virtual assignment through __ptr_glue, the park list's AddTail, Find, RemoveAt, operator
delete) are fakes on both sides recording the same events; the string assignment records the object and the first four characters of
the text. Compared: the event log and the park's fields (read at Mac offsets; the host park is dumped into a Mac-offset buffer).
Not tested: the vtable word and the string internals (the strings are fakes), a NULL this (cannot happen in C++).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlinePark_CFSOnlinePark"
MASK = 0xFFFFFFFF

FN_CT, FN_DT = 0x1017C0D8, 0x1017C464
SIZES = {FN_CT: 440, FN_DT: 236}
OWN = set(SIZES)

WCTOR, CCTOR, WDTOR, CDTOR, DELETE = 0x10238740, 0x10238458, 0x10238710, 0x102382F0, 0x10238218
ADDTAIL, FIND, REMOVEAT, PTRGLUE = 0x1017DA20, 0x1017D674, 0x1017D6F4, 0x1023BAA8
MEMBERS = [0x0C, 0x20, 0x34, 0x7C, 0x94]
TAGW, TAGC = 0x1111, 0x2222
HOST_LAYOUT = [1]
VT = {}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk_ctor(name, tag):
    def f(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
        e.write(e.r(3), struct.pack(">I", VT[tag]))
    return f


def mk_dtor(name):
    def f(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
    return f


def glue(e):
    tag, this, text = e.r(12), e.r(3), e.r(4)
    if tag == TAGW:
        u = struct.unpack(">4H", e.read(text, 8))
        e.cl.rec("WAssign", (e.cl.lab(this),) + u)
    else:
        u = struct.unpack("4B", e.read(text, 4))
        e.cl.rec("CAssign", (e.cl.lab(this),) + u)


def f_addtail(e):
    e.cl.rec("AddTail", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def f_find(e):
    L = e.cl
    L.rec("Find", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("Find") & MASK)


def f_removeat(e):
    e.cl.rec("RemoveAt", (e.cl.lab(e.r(3)), e.u32(e.r(4))))
    e.set_r(3, 0)


def f_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


STUBS.internal(WCTOR, mk_ctor("WCtor", "w"))
STUBS.internal(CCTOR, mk_ctor("CCtor", "c"))
STUBS.internal(WDTOR, mk_dtor("WDtor"))
STUBS.internal(CDTOR, mk_dtor("CDtor"))
STUBS.internal(DELETE, f_delete)
STUBS.internal(ADDTAIL, f_addtail)
STUBS.internal(FIND, f_find)
STUBS.internal(REMOVEAT, f_removeat)
STUBS.internal(PTRGLUE, glue)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, hs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", h & 0xFFFF) for h in hs))


NAMES = ["WCtor", "WDtor", "WAssign", "CCtor", "CDtor", "CAssign"]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.park = e.alloc(0x200)
        self.list = e.alloc(0x40)
        self.tlist = e.alloc(0x10)
        VT["w"] = e.alloc(0x40)
        VT["c"] = e.alloc(0x40)
        e.write(VT["w"] + 0x20, struct.pack(">I", TAGW))
        e.write(VT["c"] + 0x20, struct.pack(">I", TAGC))
        fixed = {self.park: 1, self.list: 7, self.tlist: 8}
        for i, o in enumerate(MEMBERS):
            fixed[self.park + o] = 2 + i
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def ptr(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def prep(self, listed):
        if listed:
            self.words(self.list + 0x2C, [self.tlist])
        self.words(self.park + 4, [self.list if listed else 0])

    def log(self):
        return self.L.log

    def dump(self):
        return self.e.read(self.park, 0xC0)

    def ctor(self, info):
        T.call_ppc(self.e, FN_CT,
                   [self.park, info, self.list])

    def dtor(self, flag):
        T.call_ppc(self.e, FN_DT, [self.park, flag & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.park = lib.tpw_pk_alloc(0x200)
        self.list = lib.tpw_pk_alloc(0x40)
        self.tlist = lib.tpw_pk_alloc(0x10)
        fixed = {1: self.park, 7: self.list, 8: self.tlist}
        for i in range(5):
            fixed[2 + i] = lib.tpw_pk_member(self.park, i)
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def ptr(self, a):
        return self.lib.tpw_pk_off(a)

    def alloc(self, n):
        return self.lib.tpw_pk_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def prep(self, listed):
        self.lib.tpw_pk_prep(self.park, self.list if listed else 0, self.tlist)

    def log(self):
        return CL.host_end(self.lib)

    def dump(self):
        buf = (C.c_uint8 * 0xC0)()
        self.lib.tpw_pk_dump(self.park, buf)
        return bytes(buf)

    def ctor(self, info):
        self.lib.tpw_pk_ctor(self.park, info, self.list)

    def dtor(self, flag):
        self.lib.tpw_pk_dtor(self.park, flag)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, u32, i32 = C.c_uint64, C.c_uint32, C.c_int32
    lib.tpw_pk_alloc.restype, lib.tpw_pk_alloc.argtypes = u64, [u32]
    lib.tpw_pk_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pk_off.restype, lib.tpw_pk_off.argtypes = u32, [u64]
    lib.tpw_pk_member.restype, lib.tpw_pk_member.argtypes = u64, [u64, i32]
    lib.tpw_pk_prep.restype, lib.tpw_pk_prep.argtypes = None, [u64, u64, u64]
    lib.tpw_pk_dump.restype, lib.tpw_pk_dump.argtypes = None, [u64, C.POINTER(C.c_uint8)]
    lib.tpw_pk_ctor.restype, lib.tpw_pk_ctor.argtypes = None, [u64, u64, u64]
    lib.tpw_pk_dtor.restype, lib.tpw_pk_dtor.argtypes = None, [u64, i32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pk_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def mkinfo(s, c):
    """The ccParkInfo image (0x66 bytes) with typed fields written in the side's byte order; strings allocated per side."""
    info = s.alloc(0x70)
    s.words(info + 0x00, [c["id"]])
    for off, key in ((0x04, "w0"), (0x08, "w1"), (0x0C, "w2"), (0x4C, "w3")):
        a = s.alloc(16)
        s.write(a, b"".join(struct.pack(s.fmt[0] + "H", ch) for ch in c[key] + [0]))
        s.words(info + off, [s.ptr(a)])
    a = s.alloc(16)
    s.write(a, bytes(c["c0"] + [0]))
    s.words(info + 0x44, [s.ptr(a)])
    s.words(info + 0x10, c["w10"])
    s.write(info + 0x20, bytes(c["bytes"]))
    s.words(info + 0x40, [c["w40"]])
    s.words(info + 0x48, [c["w48"]])
    s.halves(info + 0x50, c["h"])
    s.words(info + 0x56, c["w56"])
    return info


def decode(s, d):
    f = s.fmt[0]
    w = lambda o: struct.unpack(f + "I", d[o:o + 4])[0]
    h = lambda o: struct.unpack(f + "H", d[o:o + 2])[0]
    return {"list": w(4) == (s.list & MASK), "w": [w(o) for o in (0x08, 0x48, 0x4C, 0x50, 0x54, 0x78, 0x90, 0xB0, 0xB4, 0xB8, 0xBC)],
            "h": [h(0xA8), h(0xAA), h(0xAC)], "bytes": d[0x58:0x78]}


def run(s, case):
    try:
        if case["op"] == "ct":
            s.prep(True)
            s.ctor(mkinfo(s, case))
        else:
            s.prep(case["listed"])
            s.dtor(case["flag"])
        return {"log": s.log(), "park": decode(s, s.dump())}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def rch(r):
    return r.choice([0, 1, 0x41, 0x7F, 0x80, 0xFF, 0x100, 0x1234, 0xFFFF]) if r.random() < 0.5 else r.randrange(1, 0x10000)


def ct_case(r, special=None):
    c = {"op": "ct", "queues": {"Find": [0]}}
    c["id"] = r.choice([0, -1, 0x7FFFFFFF, r.getrandbits(32)]) & MASK
    for k in ("w0", "w1", "w2", "w3"):
        c[k] = [rch(r) for _ in range(4)]
    c["c0"] = [r.choice([0, 1, 0x7F, 0x80, 0xFF, r.randrange(256)]) for _ in range(4)]
    c["w10"] = [r.getrandbits(32) for _ in range(4)]
    c["bytes"] = [r.randrange(256) for _ in range(0x20)]
    c["w40"] = r.getrandbits(32)
    c["w48"] = r.getrandbits(32)
    c["h"] = [r.choice([0, 0x7FFF, 0x8000, 0xFFFF, r.getrandbits(16)]) for _ in range(3)]
    c["w56"] = [r.getrandbits(32) for _ in range(4)]
    return c


def ct_edges():
    r = random.Random(5421)
    for _ in range(6):
        yield ct_case(r)


def dt_case(r, listed=None, find=None, flag=None):
    c = {"op": "dt"}
    c["listed"] = r.random() < 0.7 if listed is None else listed
    c["queues"] = {"Find": [r.choice([0, 0x1234, 0x7FFFFFF0]) if find is None else find]}
    c["flag"] = r.choice([-1, 0, 1, 0x7FFF, -0x8000]) if flag is None else flag
    return c


def dt_edges():
    r = random.Random(5422)
    for listed in (False, True):
        for find in (0, 0x1234):
            for flag in (-1, 0, 1, 0x7FFF, -0x8000):
                yield dt_case(r, listed, find, flag)


TESTS = [
    FnTest("CFSOnlinePark::CFSOnlinePark", ppc, host, edge=ct_edges, rnd=ct_case),
    FnTest("CFSOnlinePark::~CFSOnlinePark", ppc, host, edge=dt_edges, rnd=dt_case),
]
