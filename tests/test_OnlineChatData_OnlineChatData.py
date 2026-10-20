"""Unit v5292, OnlineChatData's constructor and destructor, against libtpw_unit_OnlineChatData_OnlineChatData.dylib
(tests/unit_OnlineChatData_OnlineChatData.cpp), main app ("SimTheme Park"), src/..._SaveIgnore_OnlineChatData_OnlineChatData.cpp:
  OnlineChatData::OnlineChatData 0x1021e068, OnlineChatData::~OnlineChatData 0x10213cac

A case is an object block of 0x20 random words (built twice by one piece of Python) and a call. The callees outside the unit (wstring
ctor/dtor, the three tree ctors and destroys, list ctor/clear, operator delete) are fakes on both sides that record the sub-object's offset
from the object (and the node word for the destroys, the delete flag for the string dtors).

What is compared: the answer (r3 == this), the whole 0x80-byte block afterwards, the event log. The destructor runs with the three roots zero
and nonzero, flag values around 0 and the signed 16-bit edges, and a NULL this. No fused operations. Not tested: none.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OnlineChatData_OnlineChatData"
MASK = 0xFFFFFFFF

CTOR, DTOR = 0x1021E068, 0x10213CAC
SIZES = {CTOR: 184, DTOR: 252}
OWN = set(SIZES)

OP_DELETE = 0x10238218
HOST_LAYOUT = [0x08, 0x14, 0x24, 0x30, 0x34]
NWORDS = 0x20

BASE = [0]
STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec(name, *vals):
    def fn(e):
        e.cl.rec(name, tuple(vals_of(e, v) for v in vals))
    return fn


def vals_of(e, v):
    kind = v[0]
    if kind == "off":
        return (e.r(v[1]) - BASE[0]) & MASK
    if kind == "word":
        return e.r(v[1]) & MASK
    return T.s16(e.r(v[1]))


def stub(addr, name, *spec):
    STUBS.internal(addr, rec(name, *spec))


O3 = ("off", 3)
stub(0x101E0B84, "WStringCtor", O3)
stub(0x101468F8, "WStringDtor", O3, ("s16", 4))
stub(OP_DELETE, "delete", O3)
stub(0x10223C74, "TreeWStringCtor", O3)
stub(0x10222FE0, "TreeBuddyCtor", O3)
stub(0x10224540, "TreeNocaseCtor", O3)
stub(0x1016D63C, "ListCtor", O3)
stub(0x1016D4E8, "ListClear", O3)
stub(0x10223204, "TreeWStringDestroy", O3, ("word", 4))
stub(0x10222564, "TreeBuddyDestroy", O3, ("word", 4))
stub(0x10223E64, "TreeNocaseDestroy", O3, ("word", 4))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
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

    def call(self, op, obj, flag):
        BASE[0] = obj
        if op == "ctor":
            r3, _, _ = T.call_ppc(self.e, CTOR, [obj])
        else:
            r3, _, _ = T.call_ppc(self.e, DTOR, [obj, flag & MASK])
        return (r3 & MASK) == (obj & MASK)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_chat_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, obj, flag):
        if op == "ctor":
            return bool(self.lib.tpw_chat_ctor(obj))
        return bool(self.lib.tpw_chat_dtor(obj, T.s16(flag)))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_chat_alloc.restype, lib.tpw_chat_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_chat_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_chat_ctor.restype, lib.tpw_chat_ctor.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_chat_dtor.restype, lib.tpw_chat_dtor.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_chat_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        if case["null"]:
            obj = 0
        else:
            obj = s.alloc(0x80)
            s.words(obj, case["words"])
        ans = s.call(case["op"], obj, case["flag"])
        mem = []
        if obj:
            raw = s.read(obj, 0x80)
            for i in range(0, 0x80, 4):
                g = raw[i:i + 4]
                mem.append(g if (i == 0x30 or s.fmt[0] == ">") else g[::-1])   # the flag bytes at 0x30 and 0x31 are bytes, not a word
        return {"answer": ans, "log": s.log(), "mem": mem}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def make(r, op, roots=None, flag=None, null=False):
    ws = [r.choice([0, r.getrandbits(32), 0xFFFFFFFF]) for _ in range(NWORDS)]
    ws[0x30 // 4] = r.choice([0, 0x01010101, 0xFFFFFFFF, 0x5A5A5A5A])   # four equal bytes: the same bytes in either byte order
    if roots is not None:
        for off, v in zip((0x0C, 0x18, 0x28), roots):
            ws[off // 4] = v
    return {"op": op, "words": ws, "flag": flag if flag is not None else r.choice([-1, 0, 1, 5, 0x7FFF, -0x8000]),
            "null": null}


def ctor_edges():
    r = random.Random(11)
    for _ in range(3):
        yield make(r, "ctor")


def ctor_random(r):
    return make(r, "ctor")


def dtor_edges():
    r = random.Random(12)
    for a in (0, 0x1234):
        for b in (0, 0x2345):
            for c in (0, 0x3456):
                for flag in (-1, 0, 1):
                    yield make(r, "dtor", (a, b, c), flag)
    for flag in (0x7FFF, -0x8000, 0x10001, 0x18000):
        yield make(r, "dtor", (1, 2, 3), flag)
    yield make(r, "dtor", null=True, flag=1)


def dtor_random(r):
    roots = tuple(r.choice([0, r.getrandbits(32)]) for _ in range(3))
    return make(r, "dtor", roots, r.choice([-1, 0, 1, r.randrange(-32768, 32768)]))


TESTS = [
    FnTest("OnlineChatData::OnlineChatData", ppc, host, edge=ctor_edges, rnd=ctor_random),
    FnTest("OnlineChatData::~OnlineChatData", ppc, host, edge=dtor_edges, rnd=dtor_random),
]
