"""Unit v5201, ~basic_ifstream<wchar_t> with the delete flag, against libtpw_unit_std_basic_ifstream_Dtor_basic_ifstream.dylib, main app,
src/std_basic_ifstream_Dtor_basic_ifstream.cpp:
  std::basic_ifstream<wchar_t,std::char_traits<wchar_t>>::~basic_ifstream 0x1021d5bc

A case is an object (junk fields, a virtual-base table pointer, a FILE* at the filebuf) and a call with a flag. The same Python code builds
both memories; the Mac object has 4-byte pointers, the host object native ones (offset tables MAC / HOST). Callees (filebuf close, locale dtor,
ios_base dtor, operator delete) are fakes recording the subobject as a label and the flag. Compared: the answer, every pointer field, the
untouched fields, the vbase table (pointers normalised to global names, the base offset to a subobject name), the log.
No fused operations. Not tested: a NULL vbase table with a non-NULL object (the original reads through it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_ifstream_Dtor_basic_ifstream"
MASK = 0xFFFFFFFF

FN = 0x1021D5BC
SIZES = {FN: 324}
OWN = set(SIZES)

ADDR = dict(A=0x102B1E04, B=0x102B1E40, C=0x102B1E60, D=0x102B1EB0, E=0x102B1F20, files=0x103BD0BC)
NAMES = list(ADDR)
# field offsets: vb, vt8, fbvt, loc (8 raw bytes), file, ios, rest (0x20 raw bytes), table first, table base offset, table raw (0x30 bytes at tpad)
MAC = dict(vb=0, vt8=8, fbvt=0xc, loc=0x28, file=0x30, ios=0x5c, rest=0x60, tfirst=0, toff=0x44, tpad=4, fb=0xc, lo=0x28)
HST = dict(vb=0, vt8=8, fbvt=0x10, loc=0x18, file=0x28, ios=0x38, rest=0x40, tfirst=0, toff=0x40, tpad=8, fb=0x10, lo=0x18)
HOST_LAYOUT = [0x80, 8, 0x10, 0x18, 0x28, 0x38, 0x40, 0x48, 0x40]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk(name, spec):
    def f(e):
        L = e.cl
        vals = [L.lab(e.r(3))]
        if spec:
            vals.append(T.s32(e.r(4)))
        L.rec(name, tuple(vals))
    return f


STUBS.internal(0x10221B40, mk("Close", False))
STUBS.internal(0x10071F34, mk("LocaleDtor", True))
STUBS.internal(0x10238950, mk("IosBaseDtor", True))
STUBS.internal(0x10238218, mk("Delete", False))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def put(self, a, data):
        self.write(a, data)

    def ptr(self, a, v):
        self.write(a, struct.pack(self.fmt, v & self.pmask))

    def rdptr(self, a):
        return struct.unpack(self.fmt, self.read(a, self.pw))[0]


class Ppc(Side):
    fmt = ">I"
    pw = 4
    pmask = MASK
    O = MAC

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x80)
        self.tab = e.alloc(0x50)
        o = self.obj
        self.L = e.cl = CL.PpcLog({o: 1, o + 0xc: 2, o + 0x28: 3, o + 0x5c: 4}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "files":
            return self.e.u32(0x10260CA4)      # the TOC slot of the import, as the emulator resolved it
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, this, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [this & MASK, flag & 0xFFFF])
        return r3 & MASK


class Host(Side):
    fmt = "<Q"
    pw = 8
    pmask = (1 << 64) - 1
    O = HST

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_ifs_alloc(0x80)
        self.tab = lib.tpw_ifs_alloc(0x50)
        o = self.obj
        CL.host_begin(lib, {1: o, 2: o + 0x10, 3: o + 0x18, 4: o + 0x38}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ifs_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, this, flag):
        return self.lib.tpw_ifs_dtor(this, T.s32(flag)) & self.pmask


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ifs_addr.restype, lib.tpw_ifs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ifs_alloc.restype, lib.tpw_ifs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ifs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ifs_dtor.restype, lib.tpw_ifs_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ifs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    O = s.O
    c = junk(case)
    s.ptr(s.obj + O["vb"], s.tab)
    s.ptr(s.obj + O["vt8"], c["j8"])
    s.ptr(s.obj + O["fbvt"], c["jc"])
    s.ptr(s.obj + O["ios"], c["j5c"])
    s.put(s.obj + O["loc"], c["loc"])
    s.put(s.obj + O["rest"], c["rest"])
    s.put(s.tab + O["tpad"], c["tpad"])
    s.ptr(s.tab + O["tfirst"], c["tfirst"])
    s.ptr(s.tab + O["toff"], c["toff"])
    kind, delta = case["f30"]
    s.ptr(s.obj + O["file"], (s.g("files") + delta) if kind == "files" else delta)


def norm(s, v, file=False):
    f = s.g("files")
    if file:
        return ("files", v - f) if 0 <= v - f < 0xfc else v
    for n in NAMES[:5]:
        g = s.g(n)
        if v == g:
            return ("g", n, 0)
        if v == g + 0xc:
            return ("g", n, 0xc)
    return v


def observe(s, case, answer):
    O = s.O
    ans = "obj" if answer == s.obj else answer
    rp = lambda a: norm(s, s.rdptr(a))
    off = s.rdptr(s.tab + O["toff"])
    off = (off + s.tab - s.obj) & s.pmask    # back to the subobject offset in the object
    return {
        "answer": ans, "log": s.log(),
        "vb": "tab" if s.rdptr(s.obj + O["vb"]) == s.tab else s.rdptr(s.obj + O["vb"]),
        "vt8": rp(s.obj + O["vt8"]), "fbvt": rp(s.obj + O["fbvt"]), "ios": rp(s.obj + O["ios"]), "file": norm(s, s.rdptr(s.obj + O["file"]), True),
        "loc": s.read(s.obj + O["loc"], 8), "rest": s.read(s.obj + O["rest"], 0x20),
        "tfirst": rp(s.tab + O["tfirst"]), "toff": "fb" if off == O["fb"] else ("ios" if off == O["ios"] else s.rdptr(s.tab + O["toff"])),
        "tpad": s.read(s.tab + O["tpad"], 0x30),
    }


def run(s, case):
    try:
        build(s, case)
        this = s.obj if case["this"] else 0
        return observe(s, case, s.call(this, case["flag"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, flag, f30, this=True):
    c = {"flag": flag, "f30": f30, "this": this, "queues": {}}
    c["seed"] = r.randrange(1 << 30)
    return c


def junk(case):
    j = random.Random(case["seed"])
    return dict(j8=j.randrange(1 << 32), jc=j.randrange(1 << 32), j5c=j.randrange(1 << 32), loc=bytes(j.randrange(256) for _ in range(8)),
                rest=bytes(j.randrange(256) for _ in range(0x20)), tpad=bytes(j.randrange(256) for _ in range(0x30)),
                tfirst=j.randrange(1 << 32), toff=j.randrange(1 << 32))


F30 = [("files", 0), ("files", 0x54), ("files", 0xa8), ("files", 4), ("files", 0x53), ("files", 0xa9), ("raw", 0), ("raw", 0x1234)]
FLAGS = [0, 1, 0xFFFF, 0x8000, 0x7FFF, 0x10000, 0x10001, 5]


def edges():
    r = random.Random(77)
    for f in FLAGS:
        for f30 in F30:
            yield base(r, f, f30)
    yield base(r, 1, F30[0], this=False)
    yield base(r, 0, F30[0], this=False)


def rnd(r):
    return base(r, r.choice(FLAGS + [r.randrange(1 << 16)]), r.choice(F30), this=r.random() > 0.05)


TESTS = [
    FnTest("std::basic_ifstream<wchar_t,std::char_traits<wchar_t>>::~basic_ifstream", ppc, host, edge=edges, rnd=rnd),
]
