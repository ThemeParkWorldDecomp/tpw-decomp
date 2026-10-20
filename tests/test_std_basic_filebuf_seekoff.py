"""Unit v5093, std::basic_filebuf<wchar_t> seekoff, seekpos and overflow, against libtpw_unit_std_basic_filebuf_seekoff.dylib
(tests/unit_std_basic_filebuf_seekoff.cpp), main app, src/std_basic_filebuf_seekoff.cpp:
  seekoff 0x10220cd4, seekpos 0x10220b30, overflow 0x10220f18

A case is a world and a call. The world is built twice by ONE piece of Python code (PowerPC big-endian memory, host little-endian). The
callees outside the unit are fakes on both sides that record their arguments (pointers as small kinds) and answer from the case's
queues: the codecvt facet's encoding (vtable 0x18) and out (vtable 0xc) through __ptr_glue (the facet's vtable holds tags the glue stub
reads), unshift, fseek, ftell, fputc, the string init/assign/resize and operator delete.

The world: a 0x50-byte filebuf (get pointers, FILE*, mbstate, facet, pushback buffer, flags), a facet with a two-slot vtable. Compared: the
fpos answer words or the overflow answer, the filebuf's fields (pointers equal to this+0x30 as a marker), the event log.
No fused operations. Not tested: overflow's argument with bits above 16 set (the original returns r30 whole); the listing's code after the
endless loop of overflow (unreachable, r28 stays 1).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_filebuf_seekoff"
MASK = 0xFFFFFFFF

FN_SEEKOFF, FN_SEEKPOS, FN_OVERFLOW = 0x10220CD4, 0x10220B30, 0x10220F18
SIZES = {FN_SEEKOFF: 452, FN_SEEKPOS: 304, FN_OVERFLOW: 1016}
OWN = set(SIZES)

GLUE_PTR, UNSHIFT = 0x1023BAA8, 0x10226DCC
FSEEK, FTELL, FPUTC, DELETE = 0x102388F0, 0x10238908, 0x102388A8, 0x10238218
S_INIT, S_ASSIGN, S_RESIZE = 0x10072E6C, 0x100730E8, 0x10073558

HOST_LAYOUT = [0x24, 0x28, 0x2C, 0x30, 0x4C, 0x4D, 0x50]
CUR = {}


def s32(v):
    return T.s32(v)


def sgn8(v):
    v &= 0xFF
    return v - 256 if v & 0x80 else v


def be(v):
    return struct.pack(">I", v & MASK)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_glue(e):
    L = e.cl
    tag = e.u32(e.r(12))
    isc = 1 if e.r(3) == CUR["cvt"] else 0
    if tag == 1:
        L.rec("Encoding", (isc,))
        e.set_r(3, L.ret("Enc") & MASK)
        return
    state, frm, frm_end, to, to_end, to_next = e.r(4), e.r(5), e.r(6), e.r(8), e.r(9), e.r(10)
    ch = struct.unpack(">H", e.read(frm, 2))[0]
    is_data = 1 if CUR.get("data") is not None and to == CUR["data"] else 0
    L.rec("Out", (isc, 1 if state == CUR["fb"] + 0x28 else 0, ch, (frm_end - frm) // 2, to_end - to, is_data))
    res = L.ret("Out")
    k = L.ret("OutK")
    b = L.ret("OutB")
    e.write(to_next, be(to + k))
    e.write(to, bytes([b & 0xFF]))
    e.set_r(3, res & MASK)


def stub_unshift(e):
    e.cl.rec("Unshift", (1 if e.r(3) == CUR["fb"] else 0,))
    e.set_r(3, e.cl.ret("Unshift") & 0xFF)


def stub_fseek(e):
    e.cl.rec("Fseek", (e.r(3) & MASK, s32(e.r(4)), s32(e.r(5))))
    e.set_r(3, e.cl.ret("Fseek") & MASK)


def stub_ftell(e):
    e.cl.rec("Ftell", (e.r(3) & MASK,))
    e.set_r(3, e.cl.ret("Ftell") & MASK)


def stub_fputc(e):
    e.cl.rec("Fputc", (s32(e.r(3)), e.r(4) & MASK))
    e.set_r(3, e.cl.ret("Fputc") & MASK)


def stub_init(e):
    e.cl.rec("StringInit", (e.r(4) & MASK, sgn8(e.r(5))))
    rep = e.alloc(16)
    data = e.alloc(256)
    e.write(rep, struct.pack(">IIII", e.r(4) & MASK, 256, e.cl.ret("InitRefs") & MASK, data))
    e.write(e.r(3), be(rep))
    CUR["rep"], CUR["data"] = rep, data


def stub_assign(e):
    e.cl.rec("StringAssign", (e.r(4) & MASK, sgn8(e.r(5))))
    rep = e.u32(e.r(3))
    e.write(e.u32(rep + 12), bytes([e.r(5) & 0xFF]))
    e.write(rep, be(e.r(4)))


def stub_resize(e):
    e.cl.rec("StringResize", (s32(e.r(4)), sgn8(e.r(5))))
    e.write(e.u32(e.r(3)), be(e.r(4)))


def stub_delete(e):
    p = e.r(3)
    kind = 1 if p == CUR.get("data") and p else (2 if p == CUR.get("rep") and p else 0)
    e.cl.rec("Delete", (kind,))


STUBS.internal(GLUE_PTR, stub_glue)
STUBS.internal(UNSHIFT, stub_unshift)
STUBS.internal(FSEEK, stub_fseek)
STUBS.internal(FTELL, stub_ftell)
STUBS.internal(FPUTC, stub_fputc)
STUBS.internal(DELETE, stub_delete)
STUBS.internal(S_INIT, stub_init)
STUBS.internal(S_ASSIGN, stub_assign)
STUBS.internal(S_RESIZE, stub_resize)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def call_pos(self, entry, out, fb, a, b, mode):
        T.call_ppc(self.e, entry, [out, fb, a & MASK, b & MASK, mode & MASK])
        return 0

    def call_overflow(self, fb, c):
        r3, _, _ = T.call_ppc(self.e, FN_OVERFLOW, [fb, c & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_wseek_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_pos(self, entry, out, fb, a, b, mode):
        fn = self.lib.tpw_wseek_seekoff if entry == FN_SEEKOFF else self.lib.tpw_wseek_seekpos
        fn(out, fb, s32(a), s32(b), mode & MASK)
        return 0

    def call_overflow(self, fb, c):
        return self.lib.tpw_wseek_overflow(fb, c & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wseek_alloc.restype, lib.tpw_wseek_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_wseek_layout.argtypes = [C.POINTER(C.c_int64)]
    for nm in ("seekoff", "seekpos"):
        f = getattr(lib, "tpw_wseek_" + nm)
        f.restype, f.argtypes = None, [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_wseek_overflow.restype, lib.tpw_wseek_overflow.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_wseek_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    fb = s.alloc(0x50)
    cvt = s.alloc(8)
    vt = s.alloc(0x20)
    d_out = s.alloc(8)
    d_enc = s.alloc(8)
    s.words(d_out, [2, 0])
    s.words(d_enc, [1, 0])
    s.words(vt + 0xC, [d_out])
    s.words(vt + 0x18, [d_enc])
    s.words(cvt, [vt])
    s.words(fb + 4, [case["eback"], case["gptr"], case["egptr"]])
    s.words(fb + 0x24, [case["file"], case["state"], cvt])
    s.write(fb + 0x30, bytes(case["buf"]))
    s.write(fb + 0x4C, bytes([case["flag4c"], case["unbuf"]]))
    CUR.clear()
    CUR.update(fb=fb, cvt=cvt)
    s.fb = fb
    s.out = s.alloc(8)


def execute(s, case):
    op = case["op"]
    if op == "seekoff":
        return s.call_pos(FN_SEEKOFF, s.out, s.fb, case["off"], case["dir"], case["mode"])
    if op == "seekpos":
        return s.call_pos(FN_SEEKPOS, s.out, s.fb, case["off"], case["pstate"], case["mode"])
    return s.call_overflow(s.fb, case["ch"])


def observe(s, case, answer):
    fb = s.fb

    def norm(v):
        return "SELF30" if v == (fb + 0x30) & MASK else (v if v < 100000 else "HI%x_%x" % (v, fb))

    return {"answer": answer, "log": s.log(), "ptrs": [norm(v) for v in s.rd_words(fb + 4, 3)],
            "tail": s.rd_words(fb + 0x24, 2), "buf": s.read(fb + 0x30, 0x20), "out": s.rd_words(s.out, 2)}


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


class Case(dict):
    def __repr__(self):
        return "C(%s)" % ",".join("%s=%s" % (k, v) for k, v in self.items() if k in ("dir", "mode"))


def base(r, op, **kw):
    c = Case(op=op)
    c.update({"op": op,
         "file": r.choice([0, 0x1000 + r.randrange(0x10000)] + [0x2000 + r.randrange(0x10000)] * 5),
         "eback": r.randrange(0, 4000), "gptr": r.randrange(0, 4000), "egptr": r.randrange(0, 4000),
         "state": r.randrange(0, MASK), "buf": [r.randrange(256) for _ in range(0x1C)] + [0] * 4,
         "flag4c": r.choice([0, 1]), "unbuf": 0, "off": 0, "dir": 0, "mode": 0, "pstate": 0, "ch": 0})
    c.update(kw)
    return c


def tail_fix(c):
    # bytes 0x4c/0x4d are written separately; the buffer block is 0x1c bytes
    c["buf"] = c["buf"][:0x1C]
    return c


def off_case(r, **kw):
    c = base(r, "seekoff")
    c["dir"] = r.choice([-1, 0, 1, 2, 2, 3, 4, 4, 5, 6])
    c["off"] = r.choice([0, 1, -1, 7, -9, r.randrange(-1000, 1000), 0x7FFFFFFF, -0x80000000])
    c["mode"] = r.choice([0, 8, 0x10, 0x18, 0x20, 0x1F, r.randrange(0, 64)])
    enc = r.choice([-3, -1, 0, 1, 2, 4])
    c["queues"] = {"Enc": [enc], "Unshift": [r.choice([0, 1])], "Fseek": [r.choice([0, 0, 0, 1, -1])],
                   "Ftell": [r.randrange(-2 ** 31, 2 ** 31)]}
    c.update(kw)
    return tail_fix(c)


def off_edges():
    r = random.Random(9301)
    for d in (-1, 0, 1, 2, 3, 4, 5, 6):
        for o in (0, 5, -5):
            for enc in (-2, -1, 0, 1, 2):
                for file_zero in (False, True):
                    c = off_case(r, dir=d, off=o)
                    c["queues"]["Enc"] = [enc]
                    if file_zero:
                        c["file"] = 0
                    yield c
    for gp, eg in ((10, 21), (21, 10), (5, 6), (6, 5), (7, 7), (0, 1)):
        for mode in (0, 8, 0x18):
            yield off_case(r, dir=2, off=100, mode=mode, gptr=gp, egptr=eg)
    for fs in (0, 1, -1):
        for mode in (0, 8):
            c = off_case(r, dir=1, off=3, mode=mode)
            c["queues"].update(Fseek=[fs], Enc=[1])
            yield c


def pos_case(r, **kw):
    c = base(r, "seekpos")
    c["off"] = r.choice([0, 1, -1, r.randrange(-100000, 100000), 0x7FFFFFFF, -0x80000000])
    c["pstate"] = r.choice([0, 1, r.randrange(0, MASK)])
    c["mode"] = r.choice([0, 8, 0x10, 0x18, 0x20, 0x1F, 0x1C, r.randrange(0, 64)])
    c["queues"] = {"Enc": [r.choice([-2, -1, 0, 1, 2])], "Unshift": [r.choice([0, 1])],
                   "Fseek": [r.choice([0, 0, 0, 1, -1])]}
    c.update(kw)
    return tail_fix(c)


def pos_edges():
    r = random.Random(9302)
    for mode in (0, 1, 4, 8, 0x10, 0x18, 0x20, 0x28, 0x30):
        for enc in (-1, 0, 1):
            for fl in (0, 1):
                for un in (0, 1):
                    for fs in (0, 1):
                        c = pos_case(r, mode=mode, flag4c=fl)
                        c["queues"].update(Enc=[enc], Unshift=[un], Fseek=[fs])
                        yield c
    c = pos_case(r, mode=8)
    c["file"] = 0
    yield c


def ovf_case(r, **kw):
    c = base(r, "overflow")
    c["ch"] = r.choice([0, 1, 0x7F, 0x80, 0xFF, 0x100, 0x1234, 0xFFFE, 0xFFFF, r.randrange(65536)])
    c["unbuf"] = r.choice([0, 0, 1])
    enc = r.choice([1, 1, 0, 2, -1, 4])
    q = {"Enc": [enc], "Fputc": [r.choice([-1, 0, 65, 200])], "InitRefs": [r.choice([0, 1, 2, 3])],
         "OutK": [r.randrange(0, 12) for _ in range(8)], "OutB": [r.randrange(256) for _ in range(8)]}
    if enc == 1:
        q["Out"] = [r.choice([-1, 0, 1, 2, 3, 4, 5])]
    else:
        q["Out"] = [r.choice([0, 1, 3, -1, 4, 5]) for _ in range(r.randrange(0, 7))] + [2]
    c["queues"] = q
    c.update(kw)
    return tail_fix(c)


def ovf_edges():
    r = random.Random(9303)
    for ch in (0, 0x7F, 0x80, 0xFF, 0x100, 0xFFFE, 0xFFFF):
        for unbuf in (0, 1):
            for fp in (-1, 5):
                c = ovf_case(r, ch=ch, unbuf=unbuf)
                c["queues"]["Fputc"] = [fp]
                yield c
    for res in (-1, 0, 1, 2, 3, 4, 5):
        for fp in (-1, 5):
            c = ovf_case(r, unbuf=0, ch=0x41FF)
            c["queues"].update(Enc=[1], Out=[res], Fputc=[fp])
            yield c
    for refs in (0, 1, 2):
        for seq in ([2], [0, 2], [1, 2], [3, 2], [1, 1, 0, 3, 2], [-1, 4, 2]):
            c = ovf_case(r, unbuf=0, ch=0x90)
            c["queues"].update(Enc=[2], Out=seq, InitRefs=[refs])
            yield c
    c = ovf_case(r, unbuf=0)
    c["file"] = 0
    yield c


TESTS = [
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::seekoff", ppc, host, edge=off_edges, rnd=off_case),
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::seekpos", ppc, host, edge=pos_edges, rnd=pos_case),
    FnTest("std::basic_filebuf<wchar_t,std::char_traits<wchar_t>>::overflow", ppc, host, edge=ovf_edges, rnd=ovf_case),
]
