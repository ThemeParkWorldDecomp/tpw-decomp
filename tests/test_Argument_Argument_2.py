"""Unit v5242, the copy constructor of Argument, against libtpw_unit_Argument_Argument_2.dylib (tests/unit_Argument_Argument_2.cpp), main app
("SimTheme Park"), src/game/MslSupport.cpp:
  Argument::Argument(const Argument&) 0x10201878

A case is a world and a call. The world (a source Argument with a narrow and a wide string representation, a destination filled with garbage) is
built twice by ONE piece of Python code: in emulator memory (big-endian, the string pointer at representation +0xc) and in the host library's
memory (little-endian, the native pointer at +0x10). The callees outside the unit (strlen, wcslen, operator new[], strcpy, __throw, the
ref-counted pointer constructor, the two string init functions) are fakes on both sides that record their arguments (strings as FNV hashes).
strlen and wcslen answer a huge length for a marker string, which reaches the length_error path (the fake __throw returns, as the original's
glue does for the test).

What is compared: the call log, the three words of the destination, the source command word. The returned this is not compared (the host
declaration returns void). No fused operations. Not tested: a real length above 2^31 (needs 2 GB buffers); the marker strings stand for it.
"""
import ctypes as C
import struct

import tpwtest as T
import calllog as CL
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Argument_Argument_2"
MASK = 0xFFFFFFFF

FN = 0x10201878
SIZES = {FN: 476}
OWN = set(SIZES)

TOC_VTBL = 0x10278B74
TOC_DTOR_TV = 0x10265680
HOST_LAYOUT = [8, 12, 16]

STATE = {"dest": 0, "last": 0}


def fnv(units):
    h = 2166136261
    for c in units[:4096]:
        if c == 0:
            break
        h = ((h ^ c) * 16777619) & MASK
    return h


def rd_units(e, a, w, limit):
    out = []
    while len(out) < min(limit, 4096):
        v = int.from_bytes(e.read(a + len(out) * w, w), "big")
        if v == 0:
            break
        out.append(v)
    return out


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_strlen(e):
    s = rd_units(e, e.r(3), 1, 4096)
    e.cl.rec("strlen", (fnv(s),))
    n = len(s)
    if s and s[0] == 1 and len(s) > 1:
        if s[1] == 0x42:
            n = 0xFFFFFFFF
        elif s[1] == 0x43:
            n = 0xFFFFFFFE
    e.set_r(3, n & MASK)


def st_wcslen(e):
    s = rd_units(e, e.r(3), 2, 4096)
    e.cl.rec("wcslen", (fnv(s),))
    n = len(s)
    if len(s) > 1 and s[0] == 1:
        n = {0x4200: 0x7FFFFFFF, 0x4300: 0x7FFFFFFE, 0x4400: 0xFFFFFFFF}.get(s[1], n)
    e.set_r(3, n & MASK)


def st_newa(e):
    size = e.r(3) & MASK
    e.cl.rec("newa", (size,))
    STATE["last"] = e.alloc(size)
    e.set_r(3, STATE["last"])


def st_strcpy(e):
    d, s = e.r(3), e.r(4)
    src = rd_units(e, s, 1, 4096)
    e.cl.rec("strcpy", (1 if d == STATE["last"] else 0, fnv(src)))
    e.write(d, bytes(src) + b"\0")
    e.set_r(3, d)


def st_rcctor(e):
    self_, p = e.r(3), e.r(4)
    e.cl.rec("rcctor", (1 if p == STATE["last"] else 0,))
    e.write(self_, struct.pack(">II", p & MASK, 0))


def st_throw(e):
    typ, obj, tv = e.r(3), e.r(4), e.r(5)
    vt = e.u32(obj)
    msg = e.u32(obj + 4)
    e.cl.rec("throw", (fnv(rd_units(e, typ, 1, 4096)), 1 if vt == TOC_VTBL else 0, fnv(rd_units(e, msg, 1, 4096)),
                       1 if tv == TOC_DTOR_TV else 0))


def st_sinit(e):
    self_, first, last = e.r(3), e.r(4), e.r(5)
    diff = (last - first) & MASK
    e.cl.rec("sinit", ((self_ - STATE["dest"]) & MASK if False else self_ - STATE["dest"], diff, fnv(rd_units(e, first, 1, diff))))


def st_winit(e):
    self_, first, last = e.r(3), e.r(4), e.r(5)
    diff = (last - first) & MASK
    e.cl.rec("winit", (self_ - STATE["dest"], diff, fnv(rd_units(e, first, 2, diff // 2))))


STUBS.internal(0x10238650, st_strlen)
STUBS.internal(0x102389C8, st_wcslen)
STUBS.internal(0x10238668, st_newa)
STUBS.internal(0x10238410, st_strcpy)
STUBS.internal(0x10238680, st_throw)
STUBS.internal(0x100026AC, st_rcctor)
STUBS.internal(0x10002B8C, st_sinit)
STUBS.internal(0x10146CF8, st_winit)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        STATE["last"] = 0

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def off(self, a):
        return a

    def rep(self, data):
        a = self.alloc(0x10)
        self.words(a + 0xC, [data])
        return a

    def log(self):
        return self.L.log

    def call(self, dest, src):
        STATE["dest"] = dest
        T.call_ppc(self.e, FN, [dest, src])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ac_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def off(self, a):
        return a - self.lib.tpw_ac_base()

    def rep(self, data):
        a = self.alloc(0x20)
        self.write(a + 0x10, struct.pack("<Q", data))
        return a

    def log(self):
        return CL.host_end(self.lib)

    def call(self, dest, src):
        self.lib.tpw_ac_copy(dest, src)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ac_alloc.restype, lib.tpw_ac_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ac_base.restype, lib.tpw_ac_base.argtypes = C.c_uint64, []
    lib.tpw_ac_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ac_copy.restype, lib.tpw_ac_copy.argtypes = None, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ac_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        dest = s.alloc(12)
        s.words(dest, case["garb"])
        src = s.alloc(12)
        n, w = case["narrow"], case["wide"]
        nd = s.alloc(len(n) + 1)
        s.write(nd, bytes(n) + b"\0")
        wd = s.alloc(2 * (len(w) + 1))
        s.write(wd, struct.pack(s.fmt[0] + "%dH" % (len(w) + 1), *w, 0))
        s.words(src, [s.off(s.rep(nd)), s.off(s.rep(wd)), case["cmd"]])
        s.call(dest, src)
        return {"log": s.log(), "dest": s.rd_words(dest, 3), "cmd": s.rd_words(src + 8, 1)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(narrow, wide, cmd, garb):
    return {"queues": {}, "narrow": list(narrow), "wide": list(wide), "cmd": cmd, "garb": garb}


GARB = [0xDEADBEEF, 0x12345678, 0x9ABCDEF0]


def edges():
    yield mk([], [], 0, GARB)
    yield mk(b"a", [0x41], 0xFFFFFFFF, GARB)
    yield mk(b"hello world", [0x68, 0x69, 0xFFFF, 0x100], 7, [0, 0, 0])
    yield mk([0xFF, 0x80, 2], [0x8000, 2], 0x80000000, GARB)
    yield mk([1], [1], 5, GARB)                      # the marker byte alone is not a marker
    yield mk([1, 0x42], [0x41], 1, GARB)              # strlen 0xFFFFFFFF: throws
    yield mk([1, 0x43], [0x41], 2, GARB)              # strlen 0xFFFFFFFE: no throw
    yield mk(b"x", [1, 0x4200], 3, GARB)              # wcslen 0x7FFFFFFF: throws
    yield mk(b"x", [1, 0x4300], 4, GARB)              # wcslen 0x7FFFFFFE: no throw
    yield mk(b"x", [1, 0x4400], 5, GARB)              # wcslen 0xFFFFFFFF: throws
    yield mk([1, 0x42], [1, 0x4200], 6, GARB)         # both throw
    yield mk([1, 0x43], [1, 0x4300], 6, GARB)         # neither


def rnd(r):
    n = [r.randrange(2, 256) for _ in range(r.choice([0, 1, 2, 5, 17, 40]))]
    w = [r.randrange(2, 0x10000) for _ in range(r.choice([0, 1, 2, 5, 17, 40]))]
    return mk(n, w, r.getrandbits(32), [r.getrandbits(32) for _ in range(3)])


TESTS = [
    FnTest("Argument::Argument", ppc, host, edge=edges, rnd=rnd),
]
