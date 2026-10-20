"""Unit v51035 (ltms), against libtpw_unit_std__RefCountedPtr__RefCountedPtr.dylib (tests/unit_std__RefCountedPtr__RefCountedPtr.cpp):
  _RefCountedPtr(char*) 0x10008adc, vector<CLineInfo>::tear_down 0x1000839c, logic_error::~logic_error 0x100081c4,
  TbDoubleVTag::Format 0x10009408

A case is a world and a call, built twice (emulator memory for the original, the host library for the translation). Callees are fakes
on both sides: operator new, operator delete, the message destructor 0x10008a10, swprintf and the buffer's virtual call (__ptr_glue).
Compared: the answer, the fields read back, the event log. No fused operations.
Not tested: the exception cleanup path of the constructor (unreachable in the listing).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_std__RefCountedPtr__RefCountedPtr"
MASK = 0xFFFFFFFF

FN_CTOR, FN_TEAR, FN_DTOR, FN_FMT = 0x10008ADC, 0x1000839C, 0x100081C4, 0x10009408
SIZES = {FN_CTOR: 188, FN_TEAR: 96, FN_DTOR: 128, FN_FMT: 108}
OWN = set(SIZES)
NEW, DELETE, SWPRINTF, PTR_GLUE, RCDTOR = 0x1000F76C, 0x1000F694, 0x1000F8BC, 0x1000F91C, 0x10008A10

DATA1, DATA2 = 0x30000000, 0x20000000
CUR = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def vtclass_ppc(e, p):
    r2 = CUR["r2"]
    if p == (r2 + 0x103C) & MASK:
        return 1
    if p == (r2 + 0x104C) & MASK:
        return 2
    return 0


def stub_new(e):
    e.cl.rec("new", (e.r(3) & MASK,))
    e.set_r(3, e.alloc(max(e.r(3), 4)) & MASK)


def stub_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def stub_rcdtor(e):
    CUR["r2"] = e.r(2) & MASK      # the TOC while the original runs
    this = (e.r(3) - 4) & MASK
    e.cl.rec("rcdtor", ((e.r(3) - CUR["obj"]) & MASK, s32(e.r(4)) if False else T.s16(e.r(4)), vtclass_ppc(e, e.u32(this))))
    e.set_r(3, e.r(3))


def stub_swprintf(e):
    L = e.cl
    fmt = struct.unpack(">6H", e.read(e.r(5) & MASK, 12))
    a, b = e.r(6) & MASK, e.r(7) & MASK
    if 0 in fmt:
        z = fmt.index(0)
        fmt = fmt[:z] + (0,) * (6 - z)      # only the text up to the terminator is the format
    n = L.ret("swprintf")
    L.rec("swprintf", (e.r(4) & MASK,) + fmt + (a, b))
    e.write(e.r(3) & MASK, struct.pack(">%dH" % (n + 1), *([0x41 + i + (a & 7) for i in range(n)] + [0])))
    e.set_r(3, n & MASK)


def stub_glue(e):
    L = e.cl
    text = struct.unpack(">4H", e.read(e.r(4) & MASK, 8))
    text = tuple(c if i < (e.r(5) & MASK) else 0 for i, c in enumerate(text))   # only the n characters written
    L.rec("append", (L.lab(e.r(3)), e.r(5) & MASK) + text)


STUBS.internal(NEW, stub_new)
STUBS.internal(DELETE, stub_delete)
STUBS.internal(RCDTOR, stub_rcdtor)
STUBS.internal(SWPRINTF, stub_swprintf)
STUBS.internal(PTR_GLUE, stub_glue)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = e.alloc(0x40)
        self.out = e.alloc(0x40)
        self.vt = e.alloc(0x40)
        CUR["obj"] = self.obj
        CUR["r2"] = e.r(2) & MASK
        self.L = e.cl = CL.PpcLog({self.obj: 5, self.out: 6, DATA1: 1, DATA2: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def log(self):
        return self.L.log

    def write(self, a, data):
        self.e.write(a, data)

    def u32(self, a):
        return self.e.u32(a) & MASK

    def ctor(self, p):
        r3, _, _ = T.call_ppc(self.e, FN_CTOR, [self.obj, p])
        cnt = self.u32(self.obj + 4)
        return [int((r3 & MASK) == self.obj), self.u32(self.obj), int(cnt != 0), self.u32(cnt) if cnt else MASK]

    def tear(self, cap, size, data):
        v = self.e.alloc(16)
        self.words(v, [cap, size, data])
        T.call_ppc(self.e, FN_TEAR, [v])
        return [self.u32(v), self.u32(v + 4), self.u32(v + 8)]

    def dtor(self, isnull, flag):
        o = 0 if isnull else self.obj
        r3, _, _ = T.call_ppc(self.e, FN_DTOR, [o, (flag & 0xFFFF) | 0x12340000])
        return [int((r3 & MASK) == o), 0 if isnull else vtclass_ppc(self.e, self.u32(o))]

    def fmt_call(self, a, b):
        tag = self.e.alloc(0x14)
        self.words(tag + 0xC, [a, b])
        self.words(self.out, [self.vt])
        self.words(self.vt + 0x18, [self.vt + 0x30])
        r3, _, _ = T.call_ppc(self.e, FN_FMT, [tag, self.out, self.e.alloc(16)])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.obj = lib.tpw_rc_alloc(0x40)
        self.out = lib.tpw_rc_new_out()
        self.res = (C.c_int64 * 8)()
        CL.host_begin(lib, {5: self.obj, 6: self.out, 1: DATA1, 2: DATA2}, case["queues"])

    def done(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ctor(self, p):
        self.lib.tpw_rc_ctor(self.obj, p, self.res)
        return [int(self.res[0]), self.res[1] & MASK, int(self.res[2]), self.res[3] & MASK]

    def tear(self, cap, size, data):
        self.lib.tpw_rc_tear(cap, size, data, self.res)
        return [self.res[0] & MASK, self.res[1] & MASK, self.res[2] & MASK]

    def dtor(self, isnull, flag):
        o = 0 if isnull else self.obj
        self.lib.tpw_rc_dtor(o, s32(flag), self.res)
        return [int(self.res[0]), int(self.res[1])]

    def fmt_call(self, a, b):
        return self.lib.tpw_rc_format(a, b, self.out) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rc_alloc.restype, lib.tpw_rc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rc_new_out.restype, lib.tpw_rc_new_out.argtypes = C.c_uint64, []
    lib.tpw_rc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rc_ctor.argtypes = [C.c_uint64, C.c_uint64, C.POINTER(C.c_int64)]
    lib.tpw_rc_tear.argtypes = [C.c_uint32, C.c_uint32, C.c_uint64, C.POINTER(C.c_int64)]
    lib.tpw_rc_dtor.argtypes = [C.c_uint64, C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_rc_format.restype, lib.tpw_rc_format.argtypes = C.c_uint32, [C.c_uint32, C.c_uint32, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rc_layout(out)
    assert list(out)[:n] == [8, 16, 16, 8], list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        op = case["op"]
        if op == "ctor":
            res = s.ctor(case["p"])
        elif op == "tear":
            res = s.tear(*case["args"])
        elif op == "dtor":
            res = s.dtor(*case["args"])
        else:
            res = s.fmt_call(*case["args"])
        return {"res": res, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(op, queues=None, **kw):
    c = {"op": op, "queues": queues or {}}
    c.update(kw)
    return c


def ctor_edges():
    for p in (0, 1, 0x1234, 0x7FFFFFFF, 0xFFFFFFF0):
        yield mk("ctor", p=p)


def ctor_random(r):
    return mk("ctor", p=r.choice([0, r.randrange(1, 1 << 31)]))


def tear_edges():
    for data in (0, DATA1, DATA2):
        for cap, size in ((0, 0), (1, 1), (5, 3), (MASK, 2000 if data else MASK)):
            yield mk("tear", args=[cap, size, data])   # the empty loop of the original runs `size` times: kept small


def tear_random(r):
    data = r.choice([0, DATA1, DATA2])
    return mk("tear", args=[r.randrange(0, 1 << 32), r.randrange(0, 1 << 32) if data == 0 else r.randrange(0, 2000), data])


def dtor_edges():
    yield mk("dtor", args=[True, 1])
    for flag in (-32768, -1, 0, 1, 2, 32767, 0x8000, 0xFFFF):
        yield mk("dtor", args=[False, flag])


def dtor_random(r):
    return mk("dtor", args=[r.random() < 0.15, r.randrange(-40000, 40000)])


def fmt_edges():
    for n in (0, 1, 3, 4, 16):
        yield mk("fmt", queues={"swprintf": [n]}, args=[1, 2])
    yield mk("fmt", queues={"swprintf": [5]}, args=[0xFFFFFFFF, 0x80000000])


def fmt_random(r):
    return mk("fmt", queues={"swprintf": [r.randrange(0, 17)]}, args=[r.randrange(0, 1 << 32), r.randrange(0, 1 << 32)])


TESTS = [
    FnTest("std::_RefCountedPtr<char,std::_Array<char>>::_RefCountedPtr", ppc, host, edge=ctor_edges, rnd=ctor_random),
    FnTest("std::vector<CLineInfo,std::allocator<CLineInfo>>::tear_down", ppc, host, edge=tear_edges, rnd=tear_random),
    FnTest("std::logic_error::~logic_error", ppc, host, edge=dtor_edges, rnd=dtor_random),
    FnTest("TbDoubleVTag::Format", ppc, host, edge=fmt_edges, rnd=fmt_random),
]
