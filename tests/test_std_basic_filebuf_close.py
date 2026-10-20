"""Unit v5137, std::basic_filebuf<wchar_t> close, underflow, pbackfail, imbue, against libtpw_unit_std_basic_filebuf_close.dylib
(tests/unit_std_basic_filebuf_close.cpp), main app, src/std_basic_filebuf_close.cpp:
  close 0x10221b40, underflow 0x10221a64, pbackfail 0x10221360, imbue 0x10220964

A case is a world and a call. The world is built twice by ONE piece of Python code (emulator memory big-endian for the original, host
library memory little-endian for the translation). The callees outside the unit (unshift, fclose, the virtual underflow and
always_noconv through __ptr_glue, locale operator=, getloc, use_facet, the locale impl destructor, operator delete) are fakes on both
sides recording their arguments and answering from the case's queues.

The world: a 0x60 byte filebuf (pointers, file, flags), a get-area buffer, locale blocks, a refcount block, a facet. Compared: the
answer, the object's words (pointers as block-relative), flag bytes, the refcount, the buffer and the event log.

No fused operations. Not tested: the __unexpected exception landing pads of imbue (unreachable without exceptions). pbackfail returns the
the character, or 0 for eof.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_filebuf_close"
MASK = 0xFFFFFFFF

FN_CLOSE, FN_UNDERFLOW, FN_PBACK, FN_IMBUE = 0x10221B40, 0x10221A64, 0x10221360, 0x10220964
SIZES = {FN_CLOSE: 164, FN_UNDERFLOW: 140, FN_PBACK: 96, FN_IMBUE: 244}
OWN = set(SIZES)

HOST_LAYOUT = [0x24, 0x2C, 0x4C, 0x4D, 0x50]
BLOCKS = [("obj", 0x60), ("loc", 8), ("rc", 8), ("impl", 8), ("facet", 8), ("vt", 0x40), ("fvt", 0x40), ("buf", 0x40)]
GLUE_VU, GLUE_NC = 0xAAAA0001, 0xAAAA0002

W = {}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def u32(v):
    return v & MASK


def st_unshift(e):
    L = e.cl
    L.rec("Unshift", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Unshift") & MASK)


def st_fclose(e):
    L = e.cl
    L.rec("fclose", (e.r(3) & MASK,))
    e.set_r(3, L.ret("fclose") & MASK)


def st_delete(e):
    L = e.cl
    L.rec("Delete", (L.lab(e.r(3)),))


def st_assign(e):
    L = e.cl
    L.rec("LocaleAssign", (L.lab(e.r(3)), L.lab(e.r(4))))


def st_getloc(e):
    L = e.cl
    L.rec("Getloc", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">II", W["impl"], W["rc"]))


def st_usefacet(e):
    L = e.cl
    a = e.r(3)
    L.rec("UseFacet", (L.lab(e.u32(a)), L.lab(e.u32(a + 4))))
    e.set_r(3, W["facet"])


def st_ldtor(e):
    L = e.cl
    L.rec("LocaleDtor", (L.lab(e.r(3)), T.s32(e.r(4))))


def st_glue(e):
    L = e.cl
    r12 = e.r(12) & MASK
    if r12 == GLUE_VU:
        L.rec("VUnderflow", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("vu") & MASK)
    else:
        assert r12 == GLUE_NC, hex(r12)
        L.rec("Noconv", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("nc") & MASK)


STUBS.internal(0x10226DCC, st_unshift)
STUBS.internal(0x102388C0, st_fclose)
STUBS.internal(0x10238218, st_delete)
STUBS.internal(0x10072128, st_assign)
STUBS.internal(0x10220AB4, st_getloc)
STUBS.internal(0x10225CB8, st_usefacet)
STUBS.internal(0x100737A0, st_ldtor)
STUBS.internal(0x1023BAA8, st_glue)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def make(self):
        self.b = {name: self.alloc(size) for name, size in BLOCKS}

    def fixed(self):
        b = self.b
        return {b["obj"]: 1, b["loc"]: 2, b["obj"] + 0x1C: 3, b["rc"]: 4, b["impl"]: 5, b["facet"]: 6}


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make()
        self.L = e.cl = CL.PpcLog(self.fixed(), case["queues"])
        W.update(impl=self.b["impl"], rc=self.b["rc"] if case["rcp"] else 0, facet=self.b["facet"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make()
        lib.tpw_wcl_setup(self.b["impl"], self.b["rc"] if case["rcp"] else 0, self.b["facet"], self.b["obj"] & ~MASK)
        CL.host_begin(lib, {v: k for k, v in self.fixed().items()}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_wcl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        f = {FN_CLOSE: self.lib.tpw_wcl_close, FN_UNDERFLOW: self.lib.tpw_wcl_underflow, FN_PBACK: self.lib.tpw_wcl_pbackfail,
             FN_IMBUE: self.lib.tpw_wcl_imbue}[fn]
        return (f(*args) or 0) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wcl_alloc.restype, lib.tpw_wcl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_wcl_setup.restype, lib.tpw_wcl_setup.argtypes = None, [C.c_uint64] * 4
    lib.tpw_wcl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wcl_close.restype, lib.tpw_wcl_close.argtypes = C.c_uint64, [C.c_uint64]
    lib.tpw_wcl_underflow.restype, lib.tpw_wcl_underflow.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_wcl_pbackfail.restype, lib.tpw_wcl_pbackfail.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    lib.tpw_wcl_imbue.restype, lib.tpw_wcl_imbue.argtypes = None, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_wcl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    b = s.b

    def ptr(v):
        return b["buf"] + v if case["pm"] else v

    ws = [0] * 0x13
    ws[0] = b["vt"]
    ws[1], ws[2], ws[3] = ptr(case["eback"]), ptr(case["gptr"]), ptr(case["egptr"])
    ws[4:7] = case["junk"][:3]
    ws[7:9] = case["locslot"]
    ws[9] = case["file"]
    ws[10] = case["state"]
    ws[11] = case["cvt"]
    s.words(b["obj"], ws)
    s.write(b["obj"] + 0x4C, bytes([case["flag"], case["unb"]]))
    s.words(b["vt"] + 0x2C, [GLUE_VU])
    s.words(b["fvt"] + 0x1C, [GLUE_NC])
    s.words(b["facet"], [b["fvt"]])
    s.words(b["rc"], [case["count"]])
    s.write(b["buf"], struct.pack(s.fmt[0] + "32H", *case["bufh"]))


def execute(s, case):
    o = s.b["obj"]
    op = case["op"]
    if op == "close":
        v = s.call(FN_CLOSE, [o])
        return 1 if v == (o & MASK) else v
    if op == "underflow":
        return s.call(FN_UNDERFLOW, [o])
    if op == "pbackfail":
        return s.call(FN_PBACK, [o, case["c"]]) & 0xFFFF
    s.call(FN_IMBUE, [o, s.b["loc"]])      # void: r3 is whatever the original left
    return 0


def rel(s, w):
    for name, size in [BLOCKS[-1]] + BLOCKS[:-1]:
        d = w - (s.b[name] & MASK)
        if 0 <= d < size or (name == "buf" and -2 <= d <= size):
            return (name, d)
    return w


def observe(s, case, answer):
    b = s.b
    ptrs = {0, 1, 2, 3} | ({11} if case["op"] == "imbue" else set())
    ws = [rel(s, w) if i in ptrs else w for i, w in enumerate(s.rd_words(b["obj"], 0x13))]
    return {"answer": answer, "log": s.log(), "obj": ws, "bytes": list(s.read(b["obj"] + 0x4C, 4)),
            "rc": s.rd_word(b["rc"]), "buf": list(struct.unpack(s.fmt[0] + "32H", s.read(b["buf"], 0x40)))}


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


def base(r, op):
    return {
        "op": op, "pm": True, "rcp": r.choice([0, 1]), "c": 0,
        "queues": {"Unshift": [r.choice([0, 1, 0x100, 0xFF, 5])], "fclose": [r.choice([0, 1, MASK, 7])],
                   "vu": [r.choice([0xFFFF, 0x1234, 0x1FFFF, 0x10000, 0, 0xABCDFFFF, r.randrange(0, 1 << 32)])],
                   "nc": [r.choice([0, 1, 0xFF, 0x100, 0x1FF, 7])]},
        "file": r.choice([0, 0x1234, 0x80000000, 1]), "flag": r.choice([0, 1, 2, 0x80]), "unb": r.choice([0, 1, 9]),
        "eback": 0, "gptr": 0, "egptr": 0, "junk": [r.randrange(0, 1 << 20) for _ in range(3)],
        "locslot": [r.randrange(0, 1 << 20) for _ in range(2)], "state": r.randrange(0, 1 << 20),
        "cvt": r.randrange(0, 1 << 20), "count": r.choice([1, 2, 3, 0, 0x80000000, MASK]),
        "bufh": [r.randrange(0, 1 << 16) for _ in range(32)],
    }


def close_case(r, **kw):
    c = base(r, "close")
    c["pm"] = False
    c["eback"], c["gptr"], c["egptr"] = (r.randrange(1, 1 << 20) for _ in range(3))
    c.update(kw)
    return c


def close_edges():
    r = random.Random(11)
    for file in (0, 0x1234):
        for flag in (0, 1, 2):
            for un in (0, 1, 0x100):
                for fc in (0, 1, MASK):
                    c = close_case(r, file=file, flag=flag)
                    c["queues"]["Unshift"] = [un]
                    c["queues"]["fclose"] = [fc]
                    yield c


def under_case(r, **kw):
    c = base(r, "underflow")
    c["eback"] = r.randrange(0, 33) * 2
    c["gptr"] = r.randrange(0, 32) * 2
    c["egptr"] = r.randrange(0, 33) * 2
    c.update(kw)
    return c


def under_edges():
    r = random.Random(12)
    for file in (0, 0x1234):
        for g, e in ((4, 6), (6, 6), (8, 6), (0, 2), (62, 64), (2, 2)):
            for vu in (0xFFFF, 0x1234, 0x1FFFF, 0x10000, 0xFFFF0000):
                c = under_case(r, file=file, gptr=g, egptr=e)
                c["queues"]["vu"] = [vu]
                yield c


def pback_case(r, **kw):
    c = base(r, "pbackfail")
    c["eback"] = r.randrange(0, 33) * 2
    c["gptr"] = r.randrange(0, 33) * 2
    c["c"] = r.choice([0xFFFF, 0, 0x41, 0xFFFE, r.randrange(0, 1 << 16)])
    c.update(kw)
    return c


def pback_edges():
    r = random.Random(13)
    for file in (0, 0x1234):
        for eb, g in ((4, 6), (6, 6), (8, 6), (0, 2), (0, 0), (62, 64)):
            for ch in (0xFFFF, 0, 0x41, 0xFFFE):
                yield pback_case(r, file=file, eback=eb, gptr=g, c=ch)


def imbue_case(r, **kw):
    c = base(r, "imbue")
    c["eback"] = r.randrange(0, 33) * 2
    c["gptr"] = r.randrange(0, 33) * 2
    c["egptr"] = r.randrange(0, 33) * 2
    c.update(kw)
    return c


def imbue_edges():
    r = random.Random(14)
    for rcp in (0, 1):
        for cnt in (1, 2, 0, 0x80000000, MASK):
            for nc in (0, 1, 0xFF, 0x100, 0x1FF):
                c = imbue_case(r, rcp=rcp, count=cnt)
                c["queues"]["nc"] = [nc]
                yield c


TESTS = [
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::close", ppc, host, edge=close_edges, rnd=close_case),
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::underflow", ppc, host, edge=under_edges, rnd=under_case),
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::pbackfail", ppc, host, edge=pback_edges, rnd=pback_case),
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::imbue", ppc, host, edge=imbue_edges, rnd=imbue_case),
]
