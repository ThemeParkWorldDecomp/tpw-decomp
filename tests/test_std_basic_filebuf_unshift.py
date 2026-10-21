"""Unit v5743, std::basic_filebuf<wchar_t> unshift, sync and the destructor, against libtpw_unit_std_basic_filebuf_unshift.dylib
(tests/unit_std_basic_filebuf_unshift.cpp), main app, src/game/MslSupport.cpp:
  unshift 0x10226dcc, sync 0x1021e22c, ~basic_filebuf 0x1021cd4c

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory, big-endian, and host memory,
little-endian). The callees outside the unit are fakes on both sides that record their arguments and answer from the case: the codecvt
unshift (through __ptr_glue, vtable slot 0x14; it writes 0..4 bytes), fputc, fflush, close, the locale destructor and operator delete.

The world: a filebuf block of 0x50 bytes with random words, its FILE*, codecvt pointer and the globals' addresses. Compared: the answer,
the whole filebuf block (the vtable and FILE* words as names, not addresses) and the event log.

No fused operations. Not tested: a NULL this for the destructor (the host compiler may drop the check on a member function's this).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_filebuf_unshift"
MASK = 0xFFFFFFFF

FN_UNSHIFT, FN_SYNC, FN_DTOR = 0x10226DCC, 0x1021E22C, 0x1021CD4C
SIZES = {FN_UNSHIFT: 216, FN_SYNC: 92, FN_DTOR: 220}
OWN = set(SIZES)

GLUE, FPUTC, FFLUSH, DELETE, CLOSE, LOCDTOR = 0x1023BAA8, 0x102388A8, 0x10238920, 0x10238218, 0x10221B40, 0x10071F34
VSLOT = 0xC0DE0014

ADDR = dict(files=0x103BD0BC, vtC=0x102B1E60, vtD=0x102B1EB0)
HOST_LAYOUT = [0x24, 0x28, 0x2C, 0x50]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def stub_glue(e):
    L = e.cl
    assert e.r(12) == VSLOT
    code, n = L.ret("cvt"), L.ret("cvtn")
    to = e.r(5)
    L.rec("CodecvtUnshift", (L.lab(e.r(3)), L.lab(e.r(4)), (e.r(6) - to) & MASK, s32(code), s32(n)))
    e.write(to, bytes((0x60 + 0x15 * (i + code + 1)) & 0xFF for i in range(n)))
    e.write(e.r(7), struct.pack(">I", (to + n) & MASK))
    e.set_r(3, code & MASK)


def stub_fputc(e):
    L = e.cl
    L.rec("Fputc", (s32(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("Fputc") & MASK)


def stub_fflush(e):
    L = e.cl
    L.rec("Fflush", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Fflush") & MASK)


def stub_delete(e):
    e.cl.rec("Delete", (e.cl.lab(e.r(3)),))


def stub_close(e):
    e.cl.rec("Close", (e.cl.lab(e.r(3)),))


def stub_locdtor(e):
    e.cl.rec("LocaleDtor", (e.cl.lab(e.r(3)), s32(e.r(4))))


STUBS.internal(GLUE, stub_glue)
STUBS.internal(FPUTC, stub_fputc)
STUBS.internal(FFLUSH, stub_fflush)
STUBS.internal(DELETE, stub_delete)
STUBS.internal(CLOSE, stub_close)
STUBS.internal(LOCDTOR, stub_locdtor)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    base = 0

    def w(self, a):
        """The 32-bit value the original's field holds for an address (host: the offset from the arena base)."""
        return (a - self.base) & MASK

    def prep(self):
        self.fb = self.alloc(0x50)
        self.cvt = self.alloc(0x10)
        self.vt = self.alloc(0x20)

    def labels(self):
        return {self.fb: 1, self.fb + 0x1C: 2, self.cvt: 3, self.fb + 0x28: 4, self.fb + 0x40: 5}


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.prep()
        self.L = e.cl = CL.PpcLog(self.labels(), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "files":
            return self.e.u32(0x10260CA4)   # the TOC slot of the import __files: what the original compares with
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, op, case):
        if op == "unshift":
            return T.call_ppc(self.e, FN_UNSHIFT, [self.fb])[0] & 0xFF
        if op == "sync":
            return T.call_ppc(self.e, FN_SYNC, [self.fb])[0] & MASK
        r3 = T.call_ppc(self.e, FN_DTOR, [self.fb, case["r4"]])[0] & MASK
        return 1 if r3 == self.fb else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.prep()
        self.base = lib.tpw_fbu_base()   # after the first allocation: the arena maps lazily
        CL.host_begin(lib, {v: k for k, v in self.labels().items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fbu_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fbu_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, case):
        if op == "unshift":
            return self.lib.tpw_fbu_unshift(self.fb) & 0xFF
        if op == "sync":
            return self.lib.tpw_fbu_sync(self.fb) & MASK
        r4 = case["r4"] & 0xFFFF
        return self.lib.tpw_fbu_dtor(self.fb, r4 - 0x10000 if r4 >= 0x8000 else r4)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_fbu_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fbu_base.restype, lib.tpw_fbu_base.argtypes = C.c_uint64, []
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "unshift").restype, getattr(lib, p + "unshift").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "sync").restype, getattr(lib, p + "sync").argtypes = C.c_int32, [C.c_uint64]
    getattr(lib, p + "dtor").restype, getattr(lib, p + "dtor").argtypes = C.c_uint32, [C.c_uint64, C.c_int16]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def file_word(s, kind):
    files = s.w(s.g("files"))
    return {"null": 0, "f0": files, "f1": files + 0x54, "f2": files + 0xA8, "other": s.w(s.fb + 0x40),
            "near0": files + 1, "near1": files + 0x53, "near2": files + 0xA9, "prev": files - 1}[kind] & MASK


def build(s, case):
    s.words(s.fb, case["fill"])
    s.words(s.fb, [0x11111111])
    s.words(s.fb + 0x24, [file_word(s, case["file"])])
    s.words(s.fb + 0x2C, [s.w(s.cvt)])
    s.words(s.cvt, [s.vt])
    s.words(s.vt + 0x14, [VSLOT])


def norm(s, w, i):
    w &= MASK
    if i == 0:
        return {s.w(s.g("vtC")): "C", s.w(s.g("vtD")): "D"}.get(w, w)
    if i == 9:
        files = s.w(s.g("files"))
        if w == s.w(s.fb + 0x40):
            return "other"
        if -0x200 <= T.s32(w - files) < 0x200:
            return ("files", T.s32(w - files))
        return w
    if i == 11:
        return "cvt" if w == s.w(s.cvt) else w
    return w


def observe(s, case, answer):
    ws = s.rd_words(s.fb, 20)
    return {"answer": answer, "log": s.log(), "fb": [norm(s, w, i) for i, w in enumerate(ws)]}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


KINDS = ["null", "f0", "f1", "f2", "other", "near0", "near1", "near2", "prev"]


def base(r, op):
    return {"op": op, "fill": [r.getrandbits(32) for _ in range(20)], "file": "other", "r4": 0,
            "queues": {"cvt": [0], "cvtn": [0], "Fputc": [0], "Fflush": [0]}}


def unshift_case(r, codes=None, ns=None, putc=None):
    c = base(r, "unshift")
    k = r.randrange(1, 5)
    if codes is None:
        codes = [r.choice([0, 1, 1, 3, -1, 4]) for _ in range(k)] + [r.choice([0, 2, 3, -1, 4, 5])]
    if ns is None:
        ns = [r.randrange(0, 5) for _ in range(len(codes))]
    if putc is None:
        putc = [r.choice([0, 65, 255, -1]) if r.random() < 0.3 else 0 for _ in range(r.randrange(1, 12))]
    c["queues"] = {"cvt": codes, "cvtn": ns, "Fputc": putc, "Fflush": [0]}
    return c


def unshift_edges():
    r = random.Random(11)
    for codes, ns, putc in (([2], [0], [0]), ([0], [0], [0]), ([0], [4], [0]), ([0], [3], [-1]), ([0], [4], [0, 0, -1]),
                            ([1, 1, 0], [2, 4, 1], [0]), ([1, 2], [3, 0], [0]), ([3], [4], [0]), ([-1], [4], [0]),
                            ([4], [1], [0]), ([1, 1, 1, 2], [1, 1, 1, 1], [0]), ([1, 0], [4, 4], [0, 0, 0, 0, 0, -1])):
        yield unshift_case(r, codes, ns, putc)


def unshift_random(r):
    return unshift_case(r)


def sync_case(r, file=None, ff=None):
    c = base(r, "sync")
    c["file"] = file or r.choice(["null", "other"])
    c["queues"]["Fflush"] = [r.choice([0, -1, 1, 5]) if ff is None else ff]
    return c


def sync_edges():
    r = random.Random(12)
    for f in ("null", "other"):
        for ff in (0, -1, 1, 0x100):
            yield sync_case(r, f, ff)


def sync_random(r):
    return sync_case(r)


def dtor_case(r, file=None, flag=None):
    c = base(r, "dtor")
    c["file"] = file or r.choice(KINDS)
    f = r.choice([0, 1, -1, 2, 0x7FFF, -0x8000, 0x100]) if flag is None else flag
    c["r4"] = (r.getrandbits(16) << 16 | (f & 0xFFFF)) if r.random() < 0.5 else (f & MASK)
    return c


def dtor_edges():
    r = random.Random(13)
    for f in KINDS:
        for flag in (0, 1, -1, 0x7FFF, -0x8000):
            yield dtor_case(r, f, flag)


def dtor_random(r):
    return dtor_case(r)


TESTS = [
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::unshift", ppc, host, edge=unshift_edges, rnd=unshift_random),
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::sync", ppc, host, edge=sync_edges, rnd=sync_random),
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::~basic_filebuf", ppc, host, edge=dtor_edges, rnd=dtor_random),
]
