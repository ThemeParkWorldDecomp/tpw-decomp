"""Unit v5527, std::basic_ofstream<char> constructor and destructor, against libtpw_unit_std_basic_ofstream_Dtor_basic_ofstream_2.dylib
(tests/unit_std_basic_ofstream_Dtor_basic_ofstream_2.cpp), main app, src/RideInstance_std_basic_ofstream_Dtor_basic_ofstream_2.cpp:
  basic_ofstream 0x10071e34, ~basic_ofstream 0x1006fdc0

A case is an object block (0xC0 bytes of random words, the virtual ios pointer at +0 and the file word at +0x2c chosen by the case) and a
call, built by one piece of Python code in both memories. The callees (ostream ctor, filebuf ctor and close, locale dtor, ios_base dtor,
operator delete) are fakes on both sides that record their arguments (the sub-objects as labels); the ostream ctor fake also writes the
virtual ios pointer. Compared: the answer, the whole block (words that hold a global's address or an address inside the block are
normalised to names and offsets), the event log. The hidden short flag also gets junk in its high half.
No fused operations. Not tested: the dtor on an object whose virtual ios pointer is not inside the block (the original writes through it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_ofstream_Dtor_basic_ofstream_2"
MASK = 0xFFFFFFFF

FN_CTOR, FN_DTOR = 0x10071E34, 0x1006FDC0
SIZES = {FN_CTOR: 180, FN_DTOR: 324}
OWN = set(SIZES)

C_OSTREAM, C_FBCTOR, C_CLOSE, C_LOCDTOR, C_IOSDTOR, C_DELETE = 0x10072828, 0x10072BBC, 0x10071B54, 0x10071F34, 0x10238950, 0x10238218

ADDR = dict(ofsvt=0x1029D1A8, vt1=0x1029D0B0, vt2=0x1029D0D8, iosvt=0x103BD034, fbA=0x1029D0F8, fbB=0x1029D148, files=0x103BD0BC)
HOST_LAYOUT = [0x40]
NWORDS = 0x30


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ostream(e):
    L = e.cl
    L.rec("OstreamCtor", (L.lab(e.r(3)), s32(e.r(4)), L.lab(e.r(5))))
    e.write(e.r(3), struct.pack(">I", (e.r(3) + 0x48) & MASK))
    e.set_r(3, e.r(3))


def stub_fbctor(e):
    L = e.cl
    L.rec("FilebufCtor", (L.lab(e.r(3)), e.r(4)))


def stub_close(e):
    L = e.cl
    L.rec("Close", (L.lab(e.r(3)),))


def stub_locdtor(e):
    L = e.cl
    L.rec("LocaleDtor", (L.lab(e.r(3)), s32(e.r(4))))


def stub_iosdtor(e):
    L = e.cl
    L.rec("IosBaseDtor", (L.lab(e.r(3)), s32(e.r(4))))


def stub_delete(e):
    L = e.cl
    L.rec("Delete", (L.lab(e.r(3)),))


STUBS.internal(C_OSTREAM, stub_ostream)
STUBS.internal(C_FBCTOR, stub_fbctor)
STUBS.internal(C_CLOSE, stub_close)
STUBS.internal(C_LOCDTOR, stub_locdtor)
STUBS.internal(C_IOSDTOR, stub_iosdtor)
STUBS.internal(C_DELETE, stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def labels(self):
        o = self.obj
        return {o: 1, o + 8: 2, o + 0x24: 3, o + 0x48: 4}


class Ppc(Side):
    fmt = ">I"
    base = 0

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0xC0)
        self.L = e.cl = CL.PpcLog(self.labels(), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "files":   # an import: the TOC slot holds the address the loader gave it
            return self.e.u32(0x10260CA4)
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, ptr, flag):
        r3, _, _ = T.call_ppc(self.e, fn, [ptr & MASK, flag & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_ofs_alloc(0xC0)
        self.base = lib.tpw_ofs_addr(b"arena")
        CL.host_begin(lib, {v: k for k, v in self.labels().items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ofs_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, ptr, flag):
        f = self.lib.tpw_ofs_ctor if fn == FN_CTOR else self.lib.tpw_ofs_dtor
        return f(ptr, s32(flag))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ofs_addr.restype, lib.tpw_ofs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ofs_alloc.restype, lib.tpw_ofs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ofs_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("tpw_ofs_ctor", "tpw_ofs_dtor"):
        getattr(lib, n).restype, getattr(lib, n).argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ofs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    o = s.obj
    s.words(o, case["fill"])
    s.words(o, [o - s.base + (0x48 if case["w0"] == 0 else 0x58)])
    f = case["file"]
    if f is not None:
        s.words(o + 0x2C, [s.g("files") + f if f < 0x100 else f])


def norm(s, w, ptr):
    for name in ("files", "ofsvt", "vt1", "vt2", "iosvt", "fbA", "fbB"):
        g = s.g(name) & MASK
        if g <= w < g + (0xFC if name == "files" else 0x10 if name[0] == "f" else 0x20):
            return (name, w - g)
    if ptr and ptr <= w < ptr + 0xC0:
        return ("obj", w - ptr)
    return w


def observe(s, case, answer):
    ptr = s.obj - s.base
    ws = [norm(s, w, ptr) for w in s.rd_words(s.obj, NWORDS)]
    return {"answer": norm(s, answer - s.base, ptr) if answer else 0, "words": ws, "log": s.log()}


def run(s, case):
    try:
        build(s, case)
        ptr = s.obj if case["live"] else 0
        return observe(s, case, s.call(case["fn"], ptr, case["flag"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


FLAGS = [0, 1, -1, 2, 0x7FFF, -0x8000, 0x10000, 0x10001, 0x8000, -2]


def mk(r, fn, flag=None, live=True, w0=None, file=None):
    return {
        "fn": fn, "queues": {}, "live": live,
        "flag": r.choice(FLAGS) if flag is None else flag,
        "w0": r.randrange(2) if w0 is None else w0,
        "file": r.choice([0, 0x54, 0xA8, None, 0x1234, 0x54 + 1, 0]) if file is None else file,
        "fill": [r.getrandbits(32) for _ in range(NWORDS)],
    }


def edges(fn):
    def gen():
        r = random.Random(0x5527 + fn)
        for flag in FLAGS:
            for file in (0, 0x54, 0xA8, 0x1234, 0x100000):
                for w0 in (0, 1):
                    yield mk(r, fn, flag, True, w0, file)
        if fn == FN_DTOR:   # the constructor does not test for NULL
            yield mk(r, fn, 0, False)
            yield mk(r, fn, 1, False)
            yield mk(r, fn, -1, False)
    return gen


def rnd(fn):
    return lambda r: mk(r, fn, live=fn == FN_CTOR or r.random() > 0.05)


TESTS = [
    FnTest("std::basic_ofstream<char,std::char_traits<char>>::basic_ofstream", ppc, host, edge=edges(FN_CTOR), rnd=rnd(FN_CTOR)),
    FnTest("std::basic_ofstream<char,std::char_traits<char>>::~basic_ofstream", ppc, host, edge=edges(FN_DTOR), rnd=rnd(FN_DTOR)),
]
