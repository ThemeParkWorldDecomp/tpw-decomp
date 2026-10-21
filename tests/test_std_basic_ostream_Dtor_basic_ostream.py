"""Unit v5116, std::basic_ostream<char> destructor and constructor, against libtpw_unit_std_basic_ostream_Dtor_basic_ostream.dylib
(tests/unit_std_basic_ostream_Dtor_basic_ostream.cpp), main app, src/game/MslSupport.cpp:
  ~basic_ostream 0x10070000, basic_ostream 0x10072828

A case is a world and a call. The world is built twice by one piece of Python code: in emulator memory (big-endian) and in the host
library's arena (little-endian). The callees ios_base::~ios_base, ios_base::init, basic_ios::widen and operator delete are fakes on
both sides that record their arguments (objects as labels); widen answers from the case's queue.

The world of a case: an ostream block (0x50 bytes) and a separate ios block (0x50 bytes), both filled with random words, the ostream's
ios pointer set to its own +8 part or to the separate block. What is compared: the answer (this or not), both blocks read back with the
vtable words, the ios pointer and the base offset named, and the event log.

No fused operations. Not tested: a NULL this for the constructor (it stores through it unconditionally).
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_ostream_Dtor_basic_ostream"
MASK = 0xFFFFFFFF

FN_DTOR, FN_CTOR = 0x10070000, 0x10072828
SIZES = {FN_DTOR: 152, FN_CTOR: 188}
OWN = set(SIZES)

CALLEE_IOS_DTOR = 0x10238950
CALLEE_IOS_INIT = 0x10238968
CALLEE_WIDEN = 0x10072960
CALLEE_DELETE = 0x10238218

ADDR = dict(g1=0x1029D0B0, g2=0x1029D0D8, vt=0x103BD034)
HOST_LAYOUT = [0, 4, 8, 0x3C, 0x40, 0x44, 0x48, 0x50]
BLOCK = 0x50

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ios_dtor(e):
    L = e.cl
    L.rec("IosBaseDtor", (L.lab(e.r(3)), T.s32(e.r(4))))


def stub_ios_init(e):
    L = e.cl
    L.rec("IosInit", (L.lab(e.r(3)), e.r(4) & MASK))


def stub_widen(e):
    L = e.cl
    L.rec("Widen", (L.lab(e.r(3)), T.s8(e.r(4))))
    e.set_r(3, L.ret("Widen") & MASK)


def stub_delete(e):
    L = e.cl
    L.rec("Delete", (L.lab(e.r(3)),))


STUBS.internal(CALLEE_IOS_DTOR, stub_ios_dtor)
STUBS.internal(CALLEE_IOS_INIT, stub_ios_init)
STUBS.internal(CALLEE_WIDEN, stub_widen)
STUBS.internal(CALLEE_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(int(w & MASK).to_bytes(4, self.order) for w in ws))

    def rd_words(self, a, n):
        d = self.read(a, 4 * n)
        return [int.from_bytes(d[i:i + 4], self.order) for i in range(0, 4 * n, 4)]

    def make_objects(self, case):
        self.this = self.alloc(BLOCK)
        self.sep = self.alloc(BLOCK)


class Ppc(Side):
    order = "big"

    def __init__(self, e, case):
        self.e = e
        self.make_objects(case)
        self.L = e.cl = CL.PpcLog({self.this: 1, self.sep: 2, self.this + 8: 3}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def enc(self, a):
        return a

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def dtor(self, this, flag, junk):
        return T.call_ppc(self.e, FN_DTOR, [this, (flag & 0xFFFF) | (junk << 16)])[0] & MASK

    def ctor(self, this, flag, junk, sb):
        return T.call_ppc(self.e, FN_CTOR, [this, (flag & 0xFFFF) | (junk << 16), sb])[0] & MASK


class Host(Side):
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objects(case)
        CL.host_begin(lib, {1: self.this, 2: self.sep, 3: self.this + 8}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_os_addr(name.encode()) & MASK

    def enc(self, a):
        return self.lib.tpw_os_offset(a)

    def alloc(self, n):
        return self.lib.tpw_os_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def dtor(self, this, flag, junk):
        return self.lib.tpw_os_dtor(this, T.s16(flag))

    def ctor(self, this, flag, junk, sb):
        return self.lib.tpw_os_ctor(this, T.s16(flag), sb & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_os_addr.restype, lib.tpw_os_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_os_alloc.restype, lib.tpw_os_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_os_offset.restype, lib.tpw_os_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_os_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_os_ctor.restype, lib.tpw_os_ctor.argtypes = C.c_uint64, [C.c_uint64, C.c_int16, C.c_uint32]
    lib.tpw_os_dtor.restype, lib.tpw_os_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    out = (C.c_int64 * 32)()
    n = lib.tpw_os_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    ws = case["fill"]
    s.words(s.this, ws[:BLOCK // 4])
    s.words(s.sep, ws[BLOCK // 4:])
    fb = (ws[0] & MASK).to_bytes(4, "big")      # the fill byte is a byte: the same bytes on both sides
    s.write(s.sep + 0x40, fb)
    s.write(s.this + 0x48, fb)                  # the ios part's own fill (this + 8 + 0x40)
    # the ios pointer word at +0
    s.words(s.this, [s.enc(s.this + 8) if case["ios"] == "in" else s.enc(s.sep)])


def observe(s, case, answer):
    vmap = {s.g("g1"): "G1", s.g("g2"): "G2", (s.g("g2") + 12) & MASK: "G2+12", s.g("vt"): "VT"}
    diff = (s.this + 8 - s.sep) & MASK
    ptrs = {s.enc(s.this + 8): "in", s.enc(s.sep): "sep", 0: "null"}

    def norm(a):
        out = []
        for i, w in enumerate(s.rd_words(a, BLOCK // 4)):
            if a == s.this and i == 0:
                out.append(ptrs.get(w, w))
            elif (a == s.this and i == 18) or (a == s.sep and i == 16):
                raw = s.read(a + 4 * i, 4)
                out.append(("fill", raw[0] if s.order == "big" else raw[0], raw[1:]))
            elif (a == s.this and i == 19) or (a == s.sep and i == 17):
                out.append("D" if w == diff else w)
            else:
                out.append(vmap.get(w, w))
        return out
    this = 0 if case.get("null") else s.this
    return {"answer": answer == this, "log": s.log(), "this": norm(s.this), "sep": norm(s.sep)}


def run_with(s, case, call):
    try:
        build(s, case)
        this = 0 if case.get("null") else s.this
        return observe(s, case, call(s, this))
    finally:
        s.done()


def run_dtor(s, case):
    return run_with(s, case, lambda s, this: s.dtor(this, case["flag"], case["junk"]))


def run_ctor(s, case):
    return run_with(s, case, lambda s, this: s.ctor(this, case["flag"], case["junk"], case["sb"]))


def mk(r, ctor=False, flag=None, ios=None, null=False):
    flag = r.choice([0, 1, -1, 0x7FFF, -0x8000, 0x10000, 0x20001, 2, r.randrange(-40000, 40000)]) if flag is None else flag
    return {"fill": [r.getrandbits(32) for _ in range(BLOCK // 2)], "flag": flag, "junk": r.getrandbits(16),
            "sb": r.getrandbits(32), "ios": r.choice(["in", "sep"]) if ios is None else ios, "null": null,
            "queues": {"Widen": [r.getrandbits(8)]}}


def edges(ctor):
    def gen():
        r = random.Random(5116 + ctor)
        for flag in (0, 1, -1, 2, 0x7FFF, -0x8000, 0x10000, 0xFFFF, 0x10001):
            for ios in ("in", "sep"):
                yield mk(r, ctor, flag, ios)
        if not ctor:
            yield mk(r, ctor, 1, "in", True)
            yield mk(r, ctor, 0, "sep", True)
    return gen


TESTS = [
    FnTest("std::basic_ostream<char,std::char_traits<char>>::~basic_ostream", lambda e, c: run_dtor(Ppc(e, c), c),
           lambda lib, c: run_dtor(Host(lib, c), c), edge=edges(False), rnd=lambda r: mk(r)),
    FnTest("std::basic_ostream<char,std::char_traits<char>>::basic_ostream", lambda e, c: run_ctor(Ppc(e, c), c),
           lambda lib, c: run_ctor(Host(lib, c), c), edge=edges(True), rnd=lambda r: mk(r, True)),
]
