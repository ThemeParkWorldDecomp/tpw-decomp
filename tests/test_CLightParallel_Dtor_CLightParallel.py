"""Unit v51050, CLightParallel::~CLightParallel and InterfaceImage::SetValue, against libtpw_unit_CLightParallel_Dtor_CLightParallel.dylib
(tests/unit_CLightParallel_Dtor_CLightParallel.cpp), main app, src/game/advisor/Advisor.cpp, src/game/ui/InterfaceImage.cpp:
  CLightParallel::~CLightParallel 0x10007750, InterfaceImage::SetValue 0x101cff88

The destructor is run on the Mac's raw layout: a 0x80 byte object, the block its word 0 points at (0x70), the block that one points at
(0x10) and the block at object +0x10 (0x10), all filled with a pattern. Words that hold addresses are compared as (global or block,
delta); the two stored differences are compared adjusted. operator delete is a fake that records. SetValue: two fake draw objects
(Change records the label, flags and the frame short) and InterfaceWindow::SetValue (records). Host objects use the host's field offsets.
No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CLightParallel_Dtor_CLightParallel"
MASK = 0xFFFFFFFF

FN_DTOR = 0x10007750
FN_SET = 0x101CFF88
DELETE = 0x10238218
PTR_GLUE = 0x1023BAA8
WINSET = 0x101DFE84
ADDR = dict(vt1=0x10278C60, vt2=0x10278C9C, vt3=0x10278CB4)
HOST_LAYOUT = [276, 8]
FILL = 0xA5A5A5A5

STUBS = CL.Stubs()
STUBS.real.update({FN_DTOR, FN_SET})


def stub_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)),))


def stub_glue(e):
    L = e.cl
    assert e.r(12) == e.u32(e.u32(e.r(3) + 0x38) + 0x14)
    L.rec("Change", (L.lab(e.r(3)), e.r(4) & MASK, T.s16(e.u32(e.r(5) + 4) >> 16)))


def stub_winset(e):
    L = e.cl
    L.rec("WinSetValue", (L.lab(e.r(3)), T.s32(e.r(4))))
    e.set_r(3, 0)


STUBS.internal(DELETE, stub_delete)
STUBS.internal(PTR_GLUE, stub_glue)
STUBS.internal(WINSET, stub_winset)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = None

    def g(self, n):
        return ADDR[n]

    def alloc(self, n):
        return self.e.alloc(n)

    def begin(self, fixed):
        self.L = self.e.cl = CL.PpcLog({a: l for l, a in fixed.items()}, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def words(self, a, ws):
        self.e.write(a, b"".join((w & MASK).to_bytes(4, "big") for w in ws))

    def rd_words(self, a, n):
        return [int.from_bytes(self.e.read(a + 4 * i, 4), "big") for i in range(n)]

    def real(self, a):
        return a

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def log(self):
        return self.L.log

    def dtor(self, this, flag):
        return T.call_ppc(self.e, FN_DTOR, [this, flag & MASK])[0] & MASK

    def newdraw(self):
        obj = self.e.alloc(0x40)
        tab = self.e.alloc(0x20)
        desc = self.e.alloc(8)
        self.words(obj + 0x38, [tab])
        self.words(tab + 0x14, [desc])
        return obj

    def newimage(self, d1, d2):
        img = self.e.alloc(0x140)
        self.words(img + 0x94, [d1, d2])
        return img

    def setvalue(self, img, v):
        return T.call_ppc(self.e, FN_SET, [img, v & MASK])[0] & MASK

    def value(self, img):
        return self.rd_words(img + 0x13C, 1)[0]


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def g(self, n):
        return self.lib.tpw_clp_addr(n.encode())

    def alloc(self, n):
        return self.lib.tpw_clp_alloc(n)

    def begin(self, fixed):
        CL.host_begin(self.lib, fixed, self.case["queues"])

    def real(self, a):
        return self.lib.tpw_clp_base() + a

    def words(self, a, ws):
        C.memmove(self.real(a), b"".join((w & MASK).to_bytes(4, "little") for w in ws), 4 * len(ws))

    def rd_words(self, a, n):
        return [int.from_bytes(C.string_at(self.real(a) + 4 * i, 4), "little") for i in range(n)]

    def done(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def dtor(self, this, flag):
        r = self.lib.tpw_clp_dtor(this, T.s32(flag))
        return (r - self.lib.tpw_clp_base()) & MASK if r else 0

    def newdraw(self):
        return self.lib.tpw_clp_newdraw()

    def newimage(self, d1, d2):
        return self.lib.tpw_clp_newimage(d1, d2)

    def setvalue(self, img, v):
        return self.lib.tpw_clp_setvalue(img, T.s32(v)) & MASK

    def value(self, img):
        return self.lib.tpw_clp_value(img) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_clp_addr.restype, lib.tpw_clp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_clp_base.restype, lib.tpw_clp_base.argtypes = C.c_uint64, []
    lib.tpw_clp_alloc.restype, lib.tpw_clp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_clp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_clp_newdraw.restype, lib.tpw_clp_newdraw.argtypes = C.c_uint64, []
    lib.tpw_clp_newimage.restype, lib.tpw_clp_newimage.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    lib.tpw_clp_value.restype, lib.tpw_clp_value.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_clp_dtor.restype, lib.tpw_clp_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_clp_setvalue.restype, lib.tpw_clp_setvalue.argtypes = C.c_int32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_clp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the destructor

def norm(v, cands):
    for name, a in cands:
        d = (v - a) & MASK
        if d < 0x100:
            return (name, d)
    return v


def run_dtor(s, case):
    try:
        this = s.alloc(0x80)
        A = s.alloc(0x70)
        B = s.alloc(0x10)
        R = s.alloc(0x10)
        for blk, n in ((this, 0x80), (A, 0x70), (B, 0x10), (R, 0x10)):
            s.words(blk, [FILL] * (n // 4))
        s.words(this, [A])
        s.words(this + 0x10, [R])
        s.words(A, [B])
        if case["null"]:
            this = 0
        s.begin({s.real(this): 1} if this else {})
        ans = s.dtor(this, case["flag"])
        if case["null"]:
            return {"log": s.log(), "ans": ans}
        v1, v2, v3 = s.g("vt1"), s.g("vt2"), s.g("vt3")
        out = {"log": s.log(), "ans": (ans - this) & MASK}
        # the expected value of each word the function may write (a difference, so the two address spaces agree)
        exp = {
            "this": {0: A, 4: v1, 8: v2, 0x10: R, 0x6c: v3},
            "A": {0: B, 0x5C: v1 + 0x24, 0x60: this + 0x10 - A},
            "B": {0: v1 + 0xC, 4: this + 8 - B},
            "R": {0: v3 + 0x18, 4: this + 0x70 - R},
        }
        for nm, blk, n in (("this", this, 0x80), ("A", A, 0x70), ("B", B, 0x10), ("R", R, 0x10)):
            ws = s.rd_words(blk, n // 4)
            res = []
            for i, w in enumerate(ws):
                if w == FILL:
                    res.append("fill")
                elif i * 4 in exp[nm]:
                    res.append((w - exp[nm][i * 4]) & MASK)
                else:
                    res.append(("raw", w))
            out[nm] = res
        return out
    finally:
        s.done()


def dtor_case(r, flag=None, null=False):
    f = r.choice([0, 1, -1, 2, 0x10000, 0x18001, 0x7FFF, 0x8000, 0xFFFF0001]) if flag is None else flag
    return {"op": "dtor", "flag": f & MASK, "null": null, "queues": {}}


def dtor_edges():
    r = random.Random(51050)
    yield dtor_case(r, 0, True)
    yield dtor_case(r, 1, True)
    for f in (0, 1, -1, 2, 0x10000, 0x18001, 0x7FFF, 0x8000, 0xFFFF0001):
        yield dtor_case(r, f)


def dtor_random(r):
    return dtor_case(r)


# ---------------------------------------------------------------------------------------------------------------- SetValue

def run_set(s, case):
    try:
        d1 = s.newdraw() if case["has1"] else 0
        d2 = s.newdraw() if case["has2"] else 0
        img = s.newimage(d1, d2)
        fixed = {img: 1}
        if d1:
            fixed[d1] = 2
        if d2:
            fixed[d2] = 3
        s.begin(fixed)
        ans = s.setvalue(img, case["value"])
        return {"log": s.log(), "ans": ans, "value": s.value(img)}
    finally:
        s.done()


def set_case(r, h1=None, h2=None, v=None):
    return {"op": "set", "has1": r.random() < 0.7 if h1 is None else h1, "has2": r.random() < 0.7 if h2 is None else h2,
            "value": (r.choice([0, 1, -1, 0x7FFF, 0x8000, 0x10000, 0x12345678, 0x80000000, r.getrandbits(32)]) if v is None else v) & MASK,
            "queues": {}}


def set_edges():
    r = random.Random(101)
    for h1 in (False, True):
        for h2 in (False, True):
            for v in (0, 1, 0xFFFFFFFF, 0x8000, 0x12348765):
                yield set_case(r, h1, h2, v)


def set_random(r):
    return set_case(r)


def ppc(e, case):
    s = Ppc(e, case)
    return run_dtor(s, case) if case["op"] == "dtor" else run_set(s, case)


def host(lib, case):
    s = Host(lib, case)
    return run_dtor(s, case) if case["op"] == "dtor" else run_set(s, case)


TESTS = [
    FnTest("CLightParallel::~CLightParallel", ppc, host, edge=dtor_edges, rnd=dtor_random),
    FnTest("InterfaceImage::SetValue", ppc, host, edge=set_edges, rnd=set_random),
]
