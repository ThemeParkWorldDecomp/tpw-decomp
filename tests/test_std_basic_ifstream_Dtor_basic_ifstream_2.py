"""Unit v5642, std::basic_ifstream<char>::~basic_ifstream 0x1020448c, against libtpw_unit_std_basic_ifstream_Dtor_basic_ifstream_2.dylib
(tests/unit_std_basic_ifstream_Dtor_basic_ifstream_2.cpp), main app, src/game/MslSupport.cpp.

A case is an object block (0x200 bytes) and a call (this, delete flag). The object is built twice by one piece of Python code. The callees
(filebuf close, locale dtor, ios_base dtor, operator delete) are fakes on both sides that log their arguments, pointers as labels of
the object's parts. Compared: the answer, the whole block (words that hold a vtable global or a pointer into the block are
made names/offsets), the event log. No fused operations. Not tested: the wrap-around guards (this+0xc == 0), unreachable
for a real pointer, and the __unexpected landing pad.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_ifstream_Dtor_basic_ifstream_2"
MASK = 0xFFFFFFFF

FN = 0x1020448C
SIZES = {FN: 324}
OWN = set(SIZES)
CLOSE, LOCALE, IOSDTOR, DELETE = 0x10071B54, 0x10071F34, 0x10238950, 0x10238218

ADDR = dict(ostream=0x1029D0B0, fbA=0x1029D0F8, fbB=0x1029D148, vtblC=0x102B173C, vtbl=0x102B1778, files=0x103BD0BC)
NAMES = ["ostream", "fbA", "fbB", "vtblC", "vtbl", "files"]
SIZE = 0x200

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(CLOSE, CL.recorder("OfstreamClose", "L"))
STUBS.internal(LOCALE, CL.recorder("LocaleDtor", "Li"))
STUBS.imp("__dt__Q23std8ios_baseFv", CL.recorder("IosBaseDtor", "Li"))
STUBS.imp("__dl__FPv", CL.recorder("delete", "L"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(SIZE) if case["obj"] else 0
        o = self.obj
        self.L = e.cl = CL.PpcLog({o + 0xC: 2, o + 0x28: 3, o + 0x4C: 4, o: 1} if o else {}, case["queues"])
        self.saved = STUBS.activate(e)

    def ptr(self, a):
        return a

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "files":   # the TOC slot holds the import's address in the emulator, not 0x103bd0bc
            return self.e.u32(0x10260CA4)
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, flags, hi):
        r3, _, _ = T.call_ppc(self.e, FN, [self.obj, ((hi << 16) | (flags & 0xFFFF)) & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_ifs2_alloc(SIZE) if case["obj"] else 0
        o = self.obj
        CL.host_begin(lib, {1: o, 2: o + 0xC, 3: o + 0x28, 4: o + 0x4C} if o else {}, case["queues"])

    def ptr(self, a):
        return a - self.lib.tpw_ifs2_base()

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ifs2_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, flags, hi):
        return self.lib.tpw_ifs2_dtor(self.obj, T.s16(flags)) & 0xFFFFFFFFFFFFFFFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ifs2_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "base").restype = C.c_uint64
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "dtor").restype, getattr(lib, p + "dtor").argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == [], list(out)[:n]
    lib.bound = True


def build(s, case):
    if not s.obj:
        return
    o = s.obj
    ios = s.ptr(o + case["iosoff"])
    s.words(o, [ios])
    s.words(o + 8, [case["w8"]])
    s.words(o + 0xC, [case["wc"]])
    s.words(o + 0x28, [case["w28"]])
    f = case["file"]
    fw = s.g("files") + f[1] if f[0] == "files" else f[1]
    s.words(o + 0x30, [fw])
    s.words(o + 0x44, [case["w44"]])
    s.words(o + 0x4C, [case["w4c"]])


def norm(s, w):
    for n in NAMES:
        base = s.g(n) & MASK
        if base <= w < base + 0x100 and n == "files":
            return (n, w - base)
        if w == base:
            return n
        if w == base + 0xC:
            return (n, 0xC)
    if s.obj and s.ptr(s.obj) <= w < s.ptr(s.obj) + SIZE:
        return ("obj", w - s.ptr(s.obj))
    return w


def execute(s, case):
    return s.call(case["flags"], case["hi"])


def observe(s, case, answer):
    r = {"answer": ("obj", 0) if (s.obj and answer == s.obj) else answer, "log": s.log()}
    if s.obj:
        r["block"] = [norm(s, w) for w in s.rd_words(s.obj, SIZE // 4)]
    return r


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


FLAGS = [0, 1, -1, 2, 0x7FFF, -0x8000, 0x100, -0x100]


def base(r, flags=None, obj=True, filek=None):
    junk = lambda: r.choice([0, 0x12345678, 0xFFFFFFFF, r.getrandbits(32)])
    k = r.randrange(4) if filek is None else filek
    file = [("files", 0), ("files", 0x54), ("files", 0xA8), ("raw", r.choice([0, 0x1234, 0x103BD0BD]))][k]
    return {"queues": {}, "obj": obj, "flags": r.choice(FLAGS) if flags is None else flags, "hi": r.choice([0, 0xFFFF, r.getrandbits(16)]),
            "iosoff": r.choice([0x4C, 0x80]), "w8": junk(), "wc": junk(), "w28": junk(), "file": file, "w44": junk(), "w4c": junk()}


def edges():
    r = random.Random(4242)
    yield base(r, 0, obj=False)
    yield base(r, 1, obj=False)
    for flags in FLAGS:
        for k in range(4):
            yield base(r, flags, filek=k)


def rnd(r):
    return base(r)


TESTS = [
    FnTest("std::basic_ifstream<char,std::char_traits<char>>::~basic_ifstream", ppc, host, edge=edges, rnd=rnd),
]
