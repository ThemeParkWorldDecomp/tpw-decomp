"""Unit v5367, the collision object tests, against libtpw_unit_PointInColObject.dylib (tests/unit_PointInColObject.cpp), main app,
src/BMP_InsideArea_PointOutAllInverse_PointInColObject.cpp:
  PointInColObject 0x1002bc34, DoAreasIntersect 0x1002b6c0, ClosestIntersectingColObjectToPoint 0x1002b4cc

A case is a world and a call. The world is built twice by one piece of Python code (emulator memory big-endian, host memory little-endian).
PointOutAllInverse is a fake on both sides recording (x, y) and answering from the queue "PointOut"; labs and sqrt are pure imports
(labs is stubbed here, sqrt by install_std_stubs). DoAreasIntersect runs as the original inside ClosestIntersectingColObjectToPoint.

The world of a case: an array of collision objects (5 words used of 8), the list global (first, count). What is compared: the answer
(for pointers the object index, -1 for NULL), the event log, the objects and the list words after the call.

No fused operations. Not tested: a NULL area in DoAreasIntersect / Closest (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_PointInColObject"
MASK = 0xFFFFFFFF

FN_PIC, FN_DOA, FN_CLO = 0x1002BC34, 0x1002B6C0, 0x1002B4CC
SIZES = {FN_PIC: 408, FN_DOA: 912, FN_CLO: 424}
OWN = set(SIZES)
POINTOUT = 0x1002BB64

ADDR = dict(colList=0x102B56C0)
HOST_LAYOUT = [0x20, 0x18, 0x1C]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_pointout(e):
    L = e.cl
    L.rec("PointOut", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("PointOut") & MASK)


def stub_labs(e):
    v = s32(e.r(3))
    e.set_r(3, (-v if v < 0 else v) & MASK)


STUBS.internal(POINTOUT, stub_pointout)
STUBS.imp("labs", stub_labs)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def idx(self, p):
        if p == 0:
            return -1
        return (p - self.base) // 0x20


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def toword(self, p):
        return p

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK

    def pic(self, x, y, p):
        return self.call(FN_PIC, [x, y, p])

    def doa(self, a, b):
        return self.call(FN_DOA, [a, b])

    def clo(self, a, x, y, f):
        return self.call(FN_CLO, [a, x, y, f])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pic_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_pic_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def toword(self, p):
        return self.lib.tpw_pic_offset(p) if p else 0

    def pic(self, x, y, p):
        return self.lib.tpw_pic_PointInColObject(s32(x), s32(y), p) & MASK

    def doa(self, a, b):
        return self.lib.tpw_pic_DoAreasIntersect(a, b) & MASK

    def clo(self, a, x, y, f):
        return self.lib.tpw_pic_Closest(a, s32(x), s32(y), s32(f))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_pic_addr.restype, lib.tpw_pic_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pic_alloc.restype, lib.tpw_pic_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pic_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pic_offset.restype, lib.tpw_pic_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_pic_PointInColObject.restype = C.c_int32
    lib.tpw_pic_PointInColObject.argtypes = [C.c_int32, C.c_int32, C.c_uint64]
    lib.tpw_pic_DoAreasIntersect.restype = C.c_int32
    lib.tpw_pic_DoAreasIntersect.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_pic_Closest.restype = C.c_uint64
    lib.tpw_pic_Closest.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pic_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


NOBJ = 6


def build(s, case):
    s.base = s.alloc(0x20 * NOBJ)
    for i, o in enumerate(case["objs"]):
        s.words(s.base + 0x20 * i, o + [0, 0, 0])
    lst = s.g("colList")
    first = 0 if case["nofirst"] else s.toword(s.base)
    s.words(lst + 0x18, [first, case["count"]])


def p_of(s, i):
    return 0 if i < 0 else s.base + 0x20 * i


def execute(s, case):
    op, a = case["op"], case["args"]
    if op == "pic":
        return s.pic(a[0], a[1], p_of(s, a[2]))
    if op == "doa":
        return s.doa(p_of(s, a[0]), p_of(s, a[1]))
    return s.idx(s.clo(p_of(s, a[0]), a[1], a[2], a[3]))


def observe(s, case, answer):
    if case["op"] == "clo":
        answer = s32(answer)
    return {"answer": answer, "log": s.log(), "objs": s.rd_words(s.base, 8 * NOBJ),
            "list": [(lambda w: -1 if w == 0 else (w - s.toword(s.base)) // 0x20)(s.rd_words(s.g("colList") + 0x18, 1)[0]),s.rd_words(s.g("colList") + 0x1C, 1)[0]]}


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


# ---------------------------------------------------------------------------------------------------------------- generators

def rv(r):
    k = r.random()
    if k < 0.15:
        return r.choice([0, 1, -1, 0x87C, -0x87C, 0x8000, -0x8000, 0x7FFF, 0x1800, -0x1800])
    if k < 0.55:
        return r.randint(-5000, 5000)
    if k < 0.8:
        return r.randint(-70000, 70000)
    if k < 0.95:
        return r.randint(-(1 << 30), 1 << 30)
    return r.choice([-(1 << 31), (1 << 31) - 1, 1 << 30, -(1 << 30)])


def rr(r):
    k = r.random()
    if k < 0.1:
        return r.choice([0, -1, 1 << 31, 0x7FFFFFFF])
    if k < 0.6:
        return r.randint(0, 4000)
    if k < 0.9:
        return r.randint(0, 100000)
    return r.randint(0, 1 << 30)


def rflags(r):
    k = r.random()
    if k < 0.4:
        f = 0x1E | r.choice([0, 0, 1, 0x20, 0x40])
    else:
        f = r.randrange(0, 0x80)
    if r.random() < 0.6:
        f |= 1
    return f


def robj(r):
    return [rflags(r), rv(r), rv(r), rr(r), rr(r)]


def nearby(r, o):
    """An object near another so the geometry tests do not always fall on the same branch."""
    n = robj(r)
    if r.random() < 0.7:
        n[1] = (o[1] + r.randint(-3000, 3000)) & MASK
        n[2] = (o[2] + r.randint(-3000, 3000)) & MASK
        n[1] = s32(n[1])
        n[2] = s32(n[2])
        if r.random() < 0.5:
            n[3] = r.randint(0, 3000)
            n[4] = r.randint(0, 3000)
    if r.random() < 0.2:
        n[1], n[2] = o[1], o[2]
    if r.random() < 0.15:
        n[1] = s32(o[1] + r.choice([0x1800, -0x1800, 0]))
        n[2] = o[2] if r.random() < 0.5 else s32(o[2] + r.choice([0x1800, -0x1800, 0]))
    return n


def base(r, op):
    objs = [robj(r)]
    for _ in range(NOBJ - 1):
        objs.append(nearby(r, objs[0]))
    live = sum(1 for o in objs if o[0] & 1)
    count = live if r.random() < 0.8 else r.randint(0, live)
    return {"op": op, "objs": objs, "count": count, "nofirst": r.random() < 0.05,
            "queues": {"PointOut": [r.choice([0, 1, 1]) for _ in range(NOBJ)]}}


def pic_case(r, side=None):
    c = base(r, "pic")
    o = c["objs"][0]
    k = r.random()
    if k < 0.1:
        c["args"] = [rv(r), rv(r), -1]
        return c
    x, y = rv(r), rv(r)
    if k < 0.3:
        x, y = o[1], o[2]
    elif k < 0.7:
        x, y = s32(o[1] + r.randint(-6000, 6000)), s32(o[2] + r.randint(-6000, 6000))
        if r.random() < 0.3:
            x = o[1]
    c["args"] = [x, y, 0]
    return c


def pic_edges():
    r = random.Random(5367)
    for f in range(0, 0x80):
        for dx, dy in ((0, 0), (5, 7), (-5, 7), (5, -7), (-5, -7), (0, 3), (3, 0), (0, -3), (-3, 0)):
            c = base(r, "pic")
            c["objs"][0] = [f, 1000, 2000, 10, 12]
            c["args"] = [1000 - dx, 2000 - dy, 0]
            yield c
    # box and circle boundaries
    for f in (0x1E | 0x20, 0x1E):
        for rx, ry in ((7, 9), (50, 50), (5, 100), (0, 0)):
            for dx in (-rx - 1, -rx, 0, rx, rx + 1, 1):
                for dy in (-ry - 1, -ry, ry, ry + 1, 1):
                    c = base(r, "pic")
                    c["objs"][0] = [f, 100, 100, rx, ry]
                    c["args"] = [100 - dx, 100 - dy, 0]
                    yield c
    for _ in range(80):
        yield pic_case(r)


def pic_random(r):
    return pic_case(r)


def doa_case(r):
    c = base(r, "doa")
    k = r.random()
    if k < 0.1:
        c["args"] = [0, 0]
    else:
        c["args"] = [0, r.randrange(1, NOBJ)]
        if r.random() < 0.5:
            c["args"] = [c["args"][1], 0]
    return c


def doa_edges():
    r = random.Random(5368)
    for fa in (0x1E, 0x1C, 0x1A, 0x16, 0xE, 0x1E | 0x20, 0, 0x12, 0xC, 0x2):
        for fb in (0x1E, 0x1C, 0x1A, 0x16, 0xE, 0, 0x1E | 1, 0x12, 0xC, 0x10):
            for dx, dy in ((0, 0), (0x1800, 0), (0, 0x1800), (-0x1800, 0), (0x900, 0x900), (5, -5), (-5, 5), (3000, 3000), (0x87C, 0)):
                c = base(r, "doa")
                c["objs"][0] = [fa, 500, 500, 100, 150]
                c["objs"][1] = [fb, 500 + dx, 500 + dy, 120, 80]
                c["args"] = [0, 1]
                yield c
    for rad in (0, 1, 99, 100, 101, 200, 300, 301):
        for dx in (0, 150, 250, 300, 350, 10000, 70000):
            c = base(r, "doa")
            c["objs"][0] = [0x1E, 0, 0, rad, 3]
            c["objs"][1] = [0x1E, dx, 0, 100, 200]
            c["args"] = [0, 1]
            yield c
    for _ in range(100):
        yield doa_case(r)


def doa_random(r):
    return doa_case(r)


def clo_case(r):
    c = base(r, "clo")
    c["args"] = [r.randrange(0, NOBJ), rv(r), rv(r), r.choice([0, 1, 2])]
    if r.random() < 0.5:
        # make the objects of the list near and mostly 0x1e so DoAreasIntersect and the distance are reached
        for o in c["objs"][1:]:
            if r.random() < 0.7:
                o[0] = (o[0] & ~0x40) | 0x1F
            o[1] = s32(c["objs"][0][1] + r.randint(-4000, 4000))
            o[2] = s32(c["objs"][0][2] + r.randint(-4000, 4000))
        c["objs"][0][0] |= 0x1E
        c["args"][1] = s32(c["objs"][0][1] + r.randint(-4000, 4000))
        c["args"][2] = s32(c["objs"][0][2] + r.randint(-4000, 4000))
        c["count"] = sum(1 for o in c["objs"] if o[0] & 1)
        c["nofirst"] = False
    return c


def clo_edges():
    r = random.Random(5369)
    for _ in range(120):
        yield clo_case(r)
    for f in range(0, 0x80):
        c = base(r, "clo")
        c["objs"][1] = [f, 300, 300, 100, 100]
        c["objs"][0] = [0x1F, 300, 300, 100, 100]
        c["count"] = sum(1 for o in c["objs"] if o[0] & 1)
        c["nofirst"] = False
        c["args"] = [0, 310, 290, 0]
        yield c
    for flag in (0, 1):
        c = base(r, "clo")
        c["objs"] = [[0x1F, 5, 5, 50, 50]] + [[0, 0, 0, 0, 0]] * (NOBJ - 1)
        c["count"] = 1
        c["nofirst"] = False
        c["args"] = [0, 0, 0, flag]
        yield c


def clo_random(r):
    return clo_case(r)


TESTS = [
    FnTest("PointInColObject", ppc, host, edge=pic_edges, rnd=pic_random),
    FnTest("DoAreasIntersect", ppc, host, edge=doa_edges, rnd=doa_random),
    FnTest("ClosestIntersectingColObjectToPoint", ppc, host, edge=clo_edges, rnd=clo_random),
]
