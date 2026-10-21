"""Unit v5949, the Metrowerks exception objects and the 3D line blend, against libtpw_unit_std_logic_error_Dtor_logic_error.dylib
(tests/unit_std_logic_error_Dtor_logic_error.cpp), main app, src/game/MslSupport.cpp, src/game/ui/3DFont.cpp:
  std::logic_error::~logic_error 0x100020cc, std::out_of_range::~out_of_range 0x10002008,
  std::_RefCountedPtr<char,std::_Array<char>>::~_RefCountedPtr 0x100025e0, ::_RefCountedPtr(char*) 0x100026ac,
  Antialiased<CTraits<__vec2x32float__>> 0x10006394.

A case is a world and a call, built twice by one piece of Python code (emulator memory, big-endian 32-bit pointers; host memory,
little-endian 64-bit pointers: the Side knows the pointer width and the offsets of the two layouts). The callees outside the unit
(operator new, delete, delete[], CColour::GetFormattedValue, IntensifyPix<2>) are fakes on both sides that record every argument; the
sqrt is the real one on both. The destructors and the constructor are compared by the object's words (vtable as A/B/C, pointers as
labels), the count cell, the new block and the event log; Antialiased by the log of its IntensifyPix calls (pixel offset from the
buffer, alpha as double bits, colour, the four mask words, x, y, the clip rectangle).

No fused operations. Not tested: the original's exception cleanup path in the constructor (reached only when operator new throws, which
it never does); the constructor's return value (the host one returns void).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_logic_error_Dtor_logic_error"
MASK = 0xFFFFFFFF

FN_LOGIC, FN_OOR, FN_RCD, FN_RCC, FN_AA = 0x100020CC, 0x10002008, 0x100025E0, 0x100026AC, 0x10006394
SIZES = {FN_LOGIC: 136, FN_OOR: 148, FN_RCD: 132, FN_RCC: 188, FN_AA: 1264}
OWN = set(SIZES)

NEW, DELETE, DELETEA, GETFMT, INTENSIFY = 0x102384B8, 0x10238218, 0x10238638, 0x1023AF18, 0x10007030

ADDR = dict(vtblA=0x10278BA0, vtblB=0x10278BB0, vtblC=0x10278BC0)
HOST_LAYOUT = [0x18, 0x28, 0x30, 16, 8, 24]


def s32(v):
    return T.s32(v)


def canon64(x):
    b = T.f64_bits(x)
    if (b & 0x7FF0000000000000) == 0x7FF0000000000000 and (b & 0xFFFFFFFFFFFFF):
        b = 0x7FF8000000000000
    return b


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_new(e):
    L = e.cl
    L.rec("new", (e.r(3) & MASK,))
    e.set_r(3, L.newblk if L.ret("new") else 0)


def st_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def st_deletea(e):
    e.cl.rec("delete[]", (e.cl.lab(e.r(3)),))


def st_getfmt(e):
    L = e.cl
    L.rec("GetFormattedValue", (L.lab(e.r(3)), L.lab(e.r(4))))
    e.set_r(3, L.ret("fmt") & MASK)


def st_intensify(e):
    L = e.cl
    sp = e.r(1)
    alpha = e.f(1)
    b = canon64(alpha)
    rect = e.u32(sp + 0x44)
    L.rec("Intensify", ((e.r(4) - L.base) & MASK, b >> 32, b & MASK, e.r(7) & MASK, e.r(8) & MASK, e.r(9) & MASK, e.r(10) & MASK,
                        e.u32(sp + 0x38), e.u32(sp + 0x3C), e.u32(sp + 0x40)))
    L.rec("IntensifyRect", tuple(e.u32(rect + 4 * i) for i in range(4)))


def keep_toc(fn):
    """The glue saves r2 at 0x14(r1) and the caller reloads it after the call: do the save."""
    def wrapped(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


STUBS.internal(NEW, keep_toc(st_new))
STUBS.internal(DELETE, keep_toc(st_delete))
STUBS.internal(DELETEA, keep_toc(st_deletea))
STUBS.internal(GETFMT, keep_toc(st_getfmt))
STUBS.internal(INTENSIFY, keep_toc(st_intensify))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wp(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rp(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.P))[0]

    def blocks(self):
        self.obj = self.alloc(32)
        self.arr = self.alloc(16)
        self.cnt = self.alloc(8)
        self.newblk = self.alloc(8)
        self.base = self.alloc(16)
        self.area = self.alloc(64)
        self.colour = self.alloc(64)
        self.clip = self.alloc(16)
        self.pts = self.alloc(16)
        self.traits = self.alloc(8)

    def lab(self, v):
        m = {0: 0, self.obj: 1, self.arr: 2, self.cnt: 3, self.newblk: 4}
        return m.get(v, -1)

    def vt(self, v):
        for n in ("vtblA", "vtblB", "vtblC"):
            if v == self.g(n):
                return n
        return v

    def labels(self):
        return {self.obj: 1, self.arr: 2, self.cnt: 3, self.newblk: 4, self.area: 5, self.colour: 6, self.clip: 7}


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    P = 4
    AREA_DATA, AREA_STRIDE = 0x18, 0x24

    def __init__(self, e, case):
        self.e = e
        self.blocks()
        self.L = e.cl = CL.PpcLog(self.labels(), case["queues"])
        self.L.newblk = self.newblk
        self.L.base = self.base
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, addr, args):
        r3, _, _ = T.call_ppc(self.e, addr, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    P = 8
    AREA_DATA, AREA_STRIDE = 0x18, 0x28

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks()
        lib.tpw_slec_setup(self.newblk, self.base)
        CL.host_begin(lib, {v: k for k, v in self.labels().items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_slec_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_slec_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, addr, args):
        f = {FN_RCD: self.lib.tpw_slec_rcdtor, FN_LOGIC: self.lib.tpw_slec_logic, FN_OOR: self.lib.tpw_slec_oor}.get(addr)
        if f:
            return f(args[0], s32(args[1]))
        if addr == FN_RCC:
            self.lib.tpw_slec_rcctor(args[0], args[1])
            return 0
        self.lib.tpw_slec_aa(*args)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_slec_addr.restype, lib.tpw_slec_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_slec_alloc.restype, lib.tpw_slec_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_slec_setup.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_slec_layout.argtypes = [C.POINTER(C.c_int64)]
    for n in ("rcdtor", "logic", "oor"):
        f = getattr(lib, "tpw_slec_" + n)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_slec_rcctor.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_slec_aa.argtypes = [C.c_uint64] * 6
    out = (C.c_int64 * 32)()
    n = lib.tpw_slec_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


MARK = 0x1111


def build(s, case):
    op = case["op"]
    if op == "aa":
        s.words(s.area, case["masks"])
        s.wp(s.area + s.AREA_DATA, s.base + case["dataoff"])
        s.words(s.area + s.AREA_STRIDE, [case["stride"]])
        s.words(s.clip, case["clip"])
        s.words(s.pts, case["from"] + case["to"])
        return
    s.words(s.cnt, [case["cell"]])
    s.words(s.newblk, [0xABCD])
    off = s.P if op in ("logic", "oor") else 0
    if op in ("logic", "oor"):
        s.wp(s.obj, MARK)
    if op == "ctor":
        s.wp(s.obj, 0x1234)
        s.wp(s.obj + s.P, 0x5678)
    else:
        s.wp(s.obj + off, s.arr if case["ptr"] else 0)
        s.wp(s.obj + off + s.P, s.cnt if case["cntp"] else 0)


def execute(s, case):
    op = case["op"]
    this = s.obj if case["this"] else 0
    if op == "aa":
        return s.call(FN_AA, [s.traits, s.area, s.clip, s.colour, s.pts, s.pts + 8])
    if op == "ctor":
        return s.call(FN_RCC, [s.obj, s.arr if case["ptr"] else 0])
    addr = {"rc": FN_RCD, "logic": FN_LOGIC, "oor": FN_OOR}[op]
    return s.call(addr, [this, case["flag"]])


def observe(s, case, answer):
    op = case["op"]
    out = {"log": s.log()}
    if op == "aa":
        return out
    off = s.P if op in ("logic", "oor") else 0
    out["cell"] = s.rd_word(s.cnt)
    out["nb"] = s.rd_word(s.newblk)
    out["ptr"] = s.lab(s.rp(s.obj + off))
    out["cntp"] = s.lab(s.rp(s.obj + off + s.P))
    if op in ("logic", "oor"):
        out["vt"] = s.vt(s.rp(s.obj))
    if op != "ctor":
        out["answer"] = s.lab(answer)
    return out


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


FLAGS = [-1, 0, 1, 2, 0x7FFF, -0x8000]


def obj_case(r, op, **kw):
    c = {"op": op, "this": r.choice([1, 1, 1, 0]), "ptr": r.choice([0, 1]), "cntp": r.choice([0, 1, 1]),
         "cell": r.choice([1, 1, 2, 0, -1, 0x7FFFFFFF, r.randrange(-5, 50)]), "flag": r.choice(FLAGS),
         "queues": {"new": [r.choice([0, 1])]}}
    c.update(kw)
    return c


def obj_edges(op, seed):
    def gen():
        r = random.Random(seed)
        for this in (0, 1):
            for ptr in (0, 1):
                for cntp in (0, 1):
                    for cell in (1, 2, 0, -1):
                        for flag in FLAGS:
                            for nw in (0, 1):
                                yield obj_case(r, op, this=this, ptr=ptr, cntp=cntp, cell=cell, flag=flag, queues={"new": [nw]})
    return gen


def obj_random(op):
    return lambda r: obj_case(r, op)


def aa_case(r, p0=None, p1=None):
    f = p0 if p0 is not None else [r.randrange(-8, 30), r.randrange(-8, 30)]
    t = p1 if p1 is not None else [r.randrange(-8, 30), r.randrange(-8, 30)]
    return {"op": "aa", "this": 1, "masks": [r.randrange(0, 1 << 32) for _ in range(4)], "dataoff": r.randrange(0, 8),
            "stride": r.choice([64, 64, 100, 7, r.randrange(1, 300), -64]),
            "clip": [r.randrange(-5, 5), r.randrange(-5, 5), r.randrange(10, 40), r.randrange(10, 40)],
            "from": f, "to": t, "queues": {"fmt": [r.randrange(0, 1 << 32)]}}


def aa_edges():
    r = random.Random(77)
    ds = [(0, 0), (5, 0), (-5, 0), (0, 5), (0, -5), (4, 4), (-4, 4), (4, -4), (-4, -4), (7, 2), (-7, 2), (7, -2), (-7, -2),
          (2, 7), (-2, 7), (2, -7), (-2, -7), (1, 0), (0, 1), (-1, 0), (0, -1), (1, 1), (3, 2), (2, 3)]
    for dx, dy in ds:
        yield aa_case(r, [3, 4], [3 + dx, 4 + dy])


def aa_random(r):
    return aa_case(r)


TESTS = [
    FnTest("std::logic_error::~logic_error", ppc, host, edge=obj_edges("logic", 11), rnd=obj_random("logic")),
    FnTest("std::out_of_range::~out_of_range", ppc, host, edge=obj_edges("oor", 12), rnd=obj_random("oor")),
    FnTest("std::_RefCountedPtr<char,std::_Array<char>>::~_RefCountedPtr", ppc, host, edge=obj_edges("rc", 13), rnd=obj_random("rc")),
    FnTest("std::_RefCountedPtr<char,std::_Array<char>>::_RefCountedPtr", ppc, host, edge=obj_edges("ctor", 14), rnd=obj_random("ctor")),
    FnTest("Antialiased<CTraits<__vec2x32float__>>", ppc, host, edge=aa_edges, rnd=aa_random),
]
