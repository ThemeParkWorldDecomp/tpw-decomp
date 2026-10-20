"""Unit v5025, std::basic_filebuf<wchar_t>::uflow, against libtpw_unit_std_basic_filebuf_uflow.dylib
(tests/unit_std_basic_filebuf_uflow.cpp), main app ("SimTheme Park"), src/std_basic_filebuf_uflow.cpp:
  std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::uflow 0x10221410

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory (big-endian) and in the host
library's memory (little-endian). The callees outside the unit are fakes on both sides that record their arguments: fgetc, wmemmove,
the two codecvt virtuals reached through __ptr_glue (vtable entries 0x18 and 0x10), operator new/delete (delete records the size of
the block it frees), string append and operator[]. The facet writes a queued 16-bit value into the output slot and answers a queued
result.

The world of a case: the filebuf object (get pointers, FILE*, state, codecvt, pushback buffer, the two flags), the facet and its
vtable, the NUL byte at 0x102b1fb2, the queues. What is compared: the answer, the event log, the pointers (relative to the object),
every byte of the object. No fused operations. Not tested: a get area with egptr below eback (the original then writes far outside
the object), the exception cleanup blocks (landing pads), and the unreachable tail after the multibyte loop (the listing keeps r27 at 1).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_filebuf_uflow"
MASK = 0xFFFFFFFF

FN = 0x10221410
FGETC, WMEMMOVE, PTR_GLUE, NEW, DELETE, APPEND, INDEX = (0x10238938, 0x1023ABE8, 0x1023BAA8, 0x102384B8, 0x10238218,
                                                         0x100731B4, 0x100733B0)
ADDR = dict(nul=0x102B1FB2)
OBJ_SIZE = 0x100
HOST_LAYOUT = [4, 8, 0xC, 0x24, 0x28, 0x2C, 0x30, 0x4C, 0x4D, 16]


def sext8(v):
    v &= 0xFF
    return v - 256 if v >= 128 else v


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.add(FN)


def stub_fgetc(e):
    L = e.cl
    L.rec("fgetc", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("fgetc") & MASK)


def stub_wmemmove(e):
    L = e.cl
    d, s, n = e.r(3), e.r(4), e.r(5)
    L.rec("wmemmove", ((d - L.obj) & MASK, (s - L.obj) & MASK, n))
    e.write(d, e.read(s, 2 * n))


def stub_ptr_glue(e):
    L = e.cl
    if e.r(12) == 0x18:
        L.rec("cvt_max", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("max") & MASK)
        return
    f = e.r(5)
    n = (e.r(6) - f) & MASK
    data = list(e.read(f, min(n, 8)))
    data += [256] * (8 - len(data))
    L.rec("cvt_in", tuple([L.lab(e.r(3)), (e.r(4) - L.obj) & MASK, n] + data))
    e.write(e.r(8), struct.pack(">H", L.ret("toval") & 0xFFFF))
    e.set_r(3, L.ret("in") & MASK)


def stub_new(e):
    L = e.cl
    size = e.r(3)
    L.rec("new", (size,))
    p = e.alloc(max(size, 4))
    L.allocs[p] = size
    e.set_r(3, p)


def stub_delete(e):
    L = e.cl
    p = e.r(3)
    L.rec("delete", (0 if p == 0 else L.allocs.get(p, -1),))


def stub_append(e):
    L = e.cl
    rep = e.u32(e.r(3))
    ln, cap, data = e.u32(rep), e.u32(rep + 4), e.u32(rep + 12)
    count, c = e.r(4), sext8(e.r(5))
    L.rec("append", (count, c, ln))
    if ln + count > cap:
        ncap = (ln + count) * 2
        nd = e.alloc(ncap + 1)
        e.write(nd, e.read(data, ln))
        data = nd
        e.write(rep + 4, struct.pack(">I", ncap))
        e.write(rep + 12, struct.pack(">I", nd))
    e.write(data + ln, bytes([c & 0xFF]) * count)
    e.write(rep, struct.pack(">I", (ln + count) & MASK))


def stub_index(e):
    L = e.cl
    rep = e.u32(e.r(3))
    L.rec("index", (e.r(4),))
    e.set_r(3, (e.u32(rep + 12) + e.r(4)) & MASK)


for a, f in ((FGETC, stub_fgetc), (WMEMMOVE, stub_wmemmove), (PTR_GLUE, stub_ptr_glue), (NEW, stub_new), (DELETE, stub_delete),
             (APPEND, stub_append), (INDEX, stub_index)):
    STUBS.internal(a, f)


def setup(runner):
    e = runner.emu
    ADDR["file"] = T.persist_alloc(e, 16)
    ADDR["cvt"] = T.persist_alloc(e, 8)
    ADDR["vtbl"] = T.persist_alloc(e, 0x40)


# ---------------------------------------------------------------------------------------------------------------- the two sides


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
        self.obj = e.alloc(OBJ_SIZE)
        self.L = e.cl = CL.PpcLog({ADDR["file"]: 1, ADDR["cvt"]: 2}, case["queues"])
        self.L.obj = self.obj
        self.L.allocs = {}
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, a):
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self):
        r3, _, _ = T.call_ppc(self.e, FN, [self.obj])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_fb_base()
        self.obj = lib.tpw_fb_alloc(OBJ_SIZE)
        lib.tpw_fb_setobj(self.obj)
        CL.host_begin(lib, {1: self.g("file"), 2: self.g("cvt")}, case["queues"])

    def done(self):
        pass

    def ptr(self, a):
        """A 32-bit pointer field holds the offset from the arena base."""
        return (a - self.base) & MASK if a else 0

    def g(self, name):
        return self.lib.tpw_fb_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        return self.lib.tpw_fb_uflow(self.obj) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fb_addr.restype, lib.tpw_fb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fb_alloc.restype, lib.tpw_fb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fb_base.restype, lib.tpw_fb_base.argtypes = C.c_uint64, []
    lib.tpw_fb_setobj.restype, lib.tpw_fb_setobj.argtypes = None, [C.c_uint64]
    lib.tpw_fb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fb_uflow.restype, lib.tpw_fb_uflow.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.obj
    s.words(o, [case["w0"]])
    s.words(o + 4, [s.ptr(o + case["eback"]), s.ptr(o + case["gptr"]), s.ptr(o + case["egptr"])])
    s.write(o + 0x10, case["pad"])
    s.words(o + 0x24, [s.ptr(s.g("file")) if case["file"] else 0])
    s.write(o + 0x28, case["state"])
    s.words(o + 0x2C, [s.ptr(s.g("cvt"))])
    s.write(o + 0x30, struct.pack(s.fmt[0] + "14H", *case["buf"]))
    s.write(o + 0x4C, bytes([case["f4c"], case["f4d"]]))
    s.words(s.g("cvt"), [s.ptr(s.g("vtbl"))])
    s.words(s.g("vtbl") + 0x10, [0x10, 0, 0x18])
    s.write(s.g("nul"), bytes([case["nul"]]))


def observe(s, case, answer):
    o = s.obj
    return {
        "answer": answer,
        "log": s.log(),
        "ptrs": [(w - s.ptr(o)) & MASK for w in s.rd_words(o + 4, 3)],
        "w0": s.rd_word(o),
        "pad": s.read(o + 0x10, 0x14),
        "file": s.rd_word(o + 0x24) == (s.ptr(s.g("file")) if case["file"] else 0),
        "state": s.read(o + 0x28, 4),
        "cvt": s.rd_word(o + 0x2C) == s.ptr(s.g("cvt")),
        "buf": list(struct.unpack(s.fmt[0] + "14H", s.read(o + 0x30, 28))),
        "tail": s.read(o + 0x4C, OBJ_SIZE - 0x4C),
    }


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def fv(r, allow_eof=True):
    k = r.random()
    if allow_eof and k < 0.12:
        return -1
    if k < 0.5:
        return r.randrange(0, 0x80)
    if k < 0.8:
        return r.randrange(0x80, 0x100)
    return r.randrange(0x100, 0x30000)


def mk(r, scen, **kw):
    c = {"scen": scen, "w0": r.getrandbits(32), "pad": bytes(r.getrandbits(8) for _ in range(0x14)),
         "state": bytes(r.getrandbits(8) for _ in range(4)), "buf": [r.getrandbits(16) for _ in range(14)],
         "nul": r.choice([0, 0, r.randrange(256)]), "file": 1, "f4c": r.choice([0, 1, 0xFF]), "f4d": 0}
    if scen == "nofile":
        c["file"] = 0
    if scen == "have":
        gp = r.choice(range(0x30, 0x4A, 2))
        c.update(eback=0x30, gptr=gp, egptr=gp + 2 * r.randrange(1, 6))
    else:
        cnt = r.choice([14, 14, 0, 1, 2, 5, 13, r.randrange(0, 14)])
        # a move that reaches the flag bytes at 0x4c would show as a byte-swapped half in the buffer compare
        eb = r.choice([0x30, 0x30, 0x50, 0x60, 0xA0] + ([0x32, 0x38] if cnt != 14 else []))
        eg = eb + 2 * cnt + r.choice([0, 0, 1])
        c.update(eback=eb, egptr=eg, gptr=eg + r.choice([0, 0, 2, 4]))
    if scen == "unbuf":
        c["f4d"] = r.choice([1, 1, 0x80, 0xFF])
    mx = 1 if scen in ("single",) else r.choice([0, -1, 2, 3, 4, 5, 100])
    ins = [r.choice([0, 1, 2, 3, 4, -1]) for _ in range(r.randrange(1, 5))]
    if scen == "string":
        fg = [fv(r, False) for _ in range(r.randrange(0, 8))] + [-1]
    else:
        fg = [fv(r)]
    c["queues"] = {"fgetc": fg, "max": [mx], "in": ins, "toval": [r.getrandbits(16) for _ in range(r.randrange(1, 4))]}
    for k, v in kw.items():
        if k in c["queues"]:
            c["queues"][k] = v
        else:
            c[k] = v
    return c


def edges():
    r = __import__("random").Random(5)
    yield mk(r, "nofile")
    yield mk(r, "nofile")
    for _ in range(4):
        yield mk(r, "have")
    for cnt in (14, 0, 7):
        for fg in ([-1], [0x41], [0x1234], [0xFFFF], [0x80]):
            yield mk(r, "unbuf", eback=0x30, egptr=0x30 + 2 * cnt, gptr=0x30 + 2 * cnt, **{"fgetc": fg})
    for res in (-1, 0, 1, 2, 3, 4, 5):
        for fg in ([-1], [0x41], [0x80], [0xFF]):
            yield mk(r, "single", **{"in": [res], "fgetc": fg})
    for mx in (0, 1, -1, 2, 3, 4, 5, 100):
        for res in (-1, 0, 1, 2, 3, 4):
            for ln in (0, 1, 3, 5, 8):
                fg = [r.randrange(1, 256) for _ in range(ln)] + [-1]
                yield mk(r, "string", **{"in": [res], "fgetc": fg, "max": [mx]})
    for res in ([0, 3, 2], [3, 3, 1], [1, 0, 4, 2], [4, 3, 3, 2]):
        yield mk(r, "string", **{"in": res, "fgetc": [65, 66, 67, 200, 1, 2, 3, -1], "max": [3]})


def rnd(r):
    return mk(r, r.choice(["nofile", "have", "unbuf", "unbuf", "single", "single", "string", "string", "string"]))


TESTS = [
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::uflow", ppc, host, edge=edges, rnd=rnd),
]
