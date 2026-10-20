"""Unit v5046, std::basic_filebuf<char>: constructor, open and uflow, against libtpw_unit_std_basic_filebuf_basic_filebuf.dylib
(tests/unit_std_basic_filebuf_basic_filebuf.cpp), main app ("SimTheme Park"), src/std_basic_filebuf_basic_filebuf.cpp:
  std::basic_filebuf<char>::basic_filebuf 0x10072bbc, open 0x10071c44, uflow 0x1007165c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit are fakes on both sides that record their arguments: the locale constructor, getloc, __use_facet, the locale
destructor, the three codecvt virtuals reached through __ptr_glue (the facet's vtable holds tags the glue fake dispatches on), memmove,
the string constructor, destructor, append and operator[], fopen, fseek, fclose, fgetc, operator delete[] and unshift. Pointers into
the filebuf are logged as offsets from the object. The fakes write what the real ones would (a locale word, the mbstate, the output
byte, the string body) from the case's queues.

The world of a case: a 0x80 byte object with junk or chosen words and bytes, a facet block, a string body. What is compared: the answer,
the 16 words of the object (pointers as offsets, the vtable word by name), the buffer bytes, the bytes behind the object, the string
body and the event log.

No fused operations. Not tested: uflow with a get area longer than 14 bytes (the original then writes past the buffer), and a multibyte
loop whose fgetc never reaches end of file (the original loops for ever; the case queues always end with -1).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_filebuf_basic_filebuf"
MASK = 0xFFFFFFFF

FN_CTOR, FN_OPEN, FN_UFLOW = 0x10072BBC, 0x10071C44, 0x1007165C
SIZES = {FN_CTOR: 272, FN_OPEN: 396, FN_UFLOW: 984}
OWN = set(SIZES)

LOCALE_CTOR, GETLOC, USE_FACET, LOCALE_DTOR = 0x10072D24, 0x10070E1C, 0x100739A0, 0x10071F34
PTR_GLUE, MEMMOVE = 0x1023BAA8, 0x102386C8
STR_CTOR, STR_DTOR, STR_APPEND, STR_INDEX = 0x1007363C, 0x1000221C, 0x100731B4, 0x100733B0
FOPEN, FSEEK, FCLOSE, FGETC, DELARR, UNSHIFT = 0x102389B0, 0x102388F0, 0x102388C0, 0x10238938, 0x10238638, 0x10072A94

ADDR = dict(vtA=0x1029D0F8, vtB=0x1029D148)
HOST_LAYOUT = [4, 0x1C, 0x24, 0x28, 0x2C, 0x30, 0x3E, 0x3F, 0x40, 16]

CUR = {}      # the case's object, facet and string body addresses (PowerPC side)


def s32(v):
    return T.s32(v)


def s8(v):
    return T.s8(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def off(e, reg):
    return s32((e.r(reg) - CUR["obj"]) & MASK)


def foff(e, reg):
    return s32((e.r(reg) - CUR["facet"]) & MASK)


def vtidx(e):
    v = e.u32(CUR["obj"])
    return 1 if v == ADDR["vtA"] else 2 if v == ADDR["vtB"] else 0


def ret3(e, key):
    e.set_r(3, e.cl.ret(key) & MASK)


def st_locale_ctor(e):
    e.cl.rec("LocaleCtor", (off(e, 3), vtidx(e)))
    e.write(e.r(3), struct.pack(">I", e.cl.ret("LocaleCtor") & MASK))


def st_getloc(e):
    e.cl.rec("Getloc", (off(e, 4), vtidx(e)))


def st_use_facet(e):
    e.cl.rec("UseFacet", (0,))
    e.set_r(3, CUR["facet"])


def st_locale_dtor(e):
    e.cl.rec("LocaleDtor", (s32(e.r(4)),))


def st_glue(e):
    tag = e.r(12) & 0xFF
    if tag == 0x1C:
        e.cl.rec("AlwaysNoconv", (foff(e, 3),))
        ret3(e, "noconv")
    elif tag == 0x18:
        e.cl.rec("MaxLength", (foff(e, 3),))
        ret3(e, "maxlen")
    elif tag == 0x10:
        frm, fend, to, tend = e.r(5), e.r(6), e.r(8), e.r(9)
        n = (fend - frm) & MASK
        data = e.read(frm, min(n, 6)) if n < 0x80000000 else b""
        byts = [s8(b) if i < len(data) else 0 for i, b in enumerate(list(data) + [0] * 6)][:6]
        byts = [s8(data[i]) if i < len(data) and i < n else -1000 for i in range(6)]
        st = e.r(4)
        e.cl.rec("In", (foff(e, 3), off(e, 4), s32(e.u32(st)), s32((tend - to) & MASK), s32(n)) + tuple(byts))
        e.write(st, struct.pack(">I", e.cl.ret("state") & MASK))
        e.write(to, bytes([e.cl.ret("tobyte") & 0xFF]))
        ret3(e, "in")
    else:
        raise AssertionError("glue tag %x" % tag)


def st_memmove(e):
    e.cl.rec("Memmove", (off(e, 3), off(e, 4), s32(e.r(5))))
    n = e.r(5)
    e.write(e.r(3), e.read(e.r(4), n))


def st_str_ctor(e):
    e.cl.rec("StringCtor", (0,))
    e.write(e.r(3), struct.pack(">I", CUR["rep"]))


def st_str_dtor(e):
    e.cl.rec("StringDtor", (s32(e.r(4)),))


def st_str_append(e):
    c = e.r(5) & 0xFF
    e.cl.rec("Append", (s32(e.r(4)), s8(c)))
    rep = e.u32(e.r(3))
    ln, data = e.u32(rep), e.u32(rep + 12)
    for i in range(e.r(4)):
        e.write(data + ln, bytes([c]))
        ln += 1
    e.write(rep, struct.pack(">I", ln))


def st_str_index(e):
    e.cl.rec("Index", (s32(e.r(4)),))
    rep = e.u32(e.r(3))
    e.set_r(3, (e.u32(rep + 12) + e.r(4)) & MASK)


def st_fopen(e):
    mode = e.read(e.r(4), 4)
    packed = 0
    for b in mode:
        if b == 0:
            break
        packed = (packed << 8) | b
    e.cl.rec("Fopen", (off(e, 3), packed))
    ret3(e, "fopen")


def st_fseek(e):
    e.cl.rec("Fseek", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5))))
    ret3(e, "fseek")


def st_fclose(e):
    e.cl.rec("Fclose", (s32(e.r(3)),))
    ret3(e, "fclose")


def st_fgetc(e):
    e.cl.rec("Fgetc", (s32(e.r(3)),))
    ret3(e, "fgetc")


def st_delarr(e):
    e.cl.rec("Delete", (off(e, 3),))


def st_unshift(e):
    e.cl.rec("Unshift", (off(e, 3),))
    ret3(e, "unshift")


for _a, _f in ((LOCALE_CTOR, st_locale_ctor), (GETLOC, st_getloc), (USE_FACET, st_use_facet), (LOCALE_DTOR, st_locale_dtor),
               (PTR_GLUE, st_glue), (MEMMOVE, st_memmove), (STR_CTOR, st_str_ctor), (STR_DTOR, st_str_dtor),
               (STR_APPEND, st_str_append), (STR_INDEX, st_str_index), (FOPEN, st_fopen), (FSEEK, st_fseek),
               (FCLOSE, st_fclose), (FGETC, st_fgetc), (DELARR, st_delarr), (UNSHIFT, st_unshift)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    base = 0

    def ref(self, a):
        """The 32-bit field value of an address."""
        return (a - self.base) & MASK

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

    def ctx(self, obj, facet, rep):
        CUR.update(obj=obj, facet=facet, rep=rep)

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    @property
    def base(self):
        """32-bit pointer fields hold offsets from the arena base (set by the first allocation)."""
        return self.lib.tpw_fb_addr(b"base")

    def g(self, name):
        return self.lib.tpw_fb_addr(name.encode()) & MASK

    def alloc(self, n):
        return self.lib.tpw_fb_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ctx(self, obj, facet, rep):
        self.lib.tpw_fb_setup(obj, facet, rep)

    def call(self, fn, args):
        L = self.lib
        if fn == FN_CTOR:
            return L.tpw_fb_construct(args[0], args[1]) & MASK
        if fn == FN_OPEN:
            return L.tpw_fb_open(args[0], args[1], s32(args[2])) & MASK
        return L.tpw_fb_uflow(args[0]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_fb_"
    lib.tpw_fb_addr.restype, lib.tpw_fb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fb_alloc.restype, lib.tpw_fb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fb_setup.restype, lib.tpw_fb_setup.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_fb_construct.restype, lib.tpw_fb_construct.argtypes = C.c_uint32, [C.c_uint64, C.c_uint32]
    lib.tpw_fb_open.restype, lib.tpw_fb_open.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_fb_uflow.restype, lib.tpw_fb_uflow.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    obj = s.alloc(0x80)
    facet = s.alloc(16)
    vt = s.alloc(0x40)
    rep = s.alloc(16)
    data = s.alloc(64)
    s.ctx(obj, facet, rep)
    s.words(facet, [s.ref(vt)])
    s.words(vt + 0x10, [0xFACE0010, 0, 0xFACE0018, 0xFACE001C])
    s.words(rep, [0, 64, 1, s.ref(data)])
    ws = list(case["junk"])
    ws[0] = {0: 0, 1: s.g("vtA"), 2: s.g("vtB")}[case["vt"]]
    for i, k in zip((1, 2, 3), case["ptrs"]):
        ws[i] = s.ref(obj) + k
    ws[9] = case["file"]
    ws[11] = s.ref(facet) if case["cvt"] else 0
    s.words(obj, ws)
    s.write(obj + 0x30, bytes(case["buf"]))
    s.write(obj + 0x40, bytes(case["tail"]))
    s.obj, s.facet, s.rep, s.data = obj, facet, rep, data
    s.ctx(obj, facet, rep)


def execute(s, case):
    op = case["op"]
    if op == "ctor":
        return s.call(FN_CTOR, [s.obj, case["arg"]])
    if op == "open":
        return s.call(FN_OPEN, [s.obj, s.obj + 0x60, case["arg"] & MASK])
    return s.call(FN_UFLOW, [s.obj])


def observe(s, case, answer):
    obj = s.obj
    ws = s.rd_words(obj, 12)
    out = []
    for i, w in enumerate(ws):
        if i == 0:
            out.append("vtA" if w == s.g("vtA") else "vtB" if w == s.g("vtB") else w)
        elif i in (1, 2, 3):
            out.append((w - s.ref(obj)) & MASK)
        elif i == 11:
            out.append((w - s.ref(s.facet)) & MASK if w else 0)
        else:
            out.append(w)
    if case["op"] != "uflow":
        answer = "this" if answer == s.ref(obj) else answer
    return {"answer": answer, "log": s.log(), "words": out, "buf": s.read(obj + 0x30, 16), "tail": s.read(obj + 0x40, 0x40),
            "rep": s.rd_words(s.rep, 2), "data": s.read(s.data, 16)}


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
        return "<%s %x>" % (self["op"], self["arg"] & MASK)


def base(r, op):
    return Case({"op": op, "junk": [r.getrandbits(32) for _ in range(12)], "vt": r.randrange(3),
            "ptrs": [0x30, 0x30 + r.randrange(15), 0x30 + r.randrange(15)], "file": r.choice([0, 0x5000, 0x5004, 0x80001234]),
            "cvt": r.random() < 0.7, "buf": [r.randrange(256) for _ in range(16)], "tail": [r.randrange(256) for _ in range(0x40)],
            "arg": 0, "queues": {}})


# ---- ctor
def ctor_case(r):
    c = base(r, "ctor")
    c["arg"] = r.choice([0, 0x5000, r.getrandbits(32)])
    c["queues"] = {"LocaleCtor": [r.getrandbits(32)], "noconv": [r.choice([0, 1, 2, 0x100, 0x1FF, 255, -1, 0x7F])]}
    return c


def ctor_edges():
    r = random.Random(11)
    for _ in range(12):
        yield ctor_case(r)


# ---- open
MODES = list(range(0, 0x90)) + [0x100, 0x108, 0x10A, 0xFFFF, -1, -3, 0x7FFFFFFF, -0x7FFFFFF8, 0x80000008, 0x1000, 0x42, 0x45, 0x46]


def open_case(r, mode=None, file=None):
    c = base(r, "open")
    c["arg"] = r.choice(MODES) if mode is None else mode
    if file is None:
        file = 0 if r.random() < 0.9 else 0x5000
    c["file"] = file
    c["buf"][14] = r.choice([0, 1, 0xFF, 0x80])
    c["queues"] = {"fopen": [r.choice([0, 0x5000, 0x7FFFFFFF, -1 & MASK])], "fseek": [r.choice([0, 0, 1, -1])],
                   "unshift": [r.choice([0, 1, 0x100, 0x1FF, 0xFF])], "fclose": [r.choice([0, 1, -1])]}
    return c


def open_edges():
    r = random.Random(12)
    for m in MODES:
        yield open_case(r, m, 0)
    for m in (8, 0x0A, 0x10, 0x12, 0x18, 0x1A, 0x38, 0x3A, 0x14, 0x16, 0x1C, 0x1E, 0x3C, 0x3E, 0x11, 0x13, 0x15, 0x17):
        for _ in range(6):
            c = open_case(r, m, 0)
            yield c
    for m in (8, 0x10):
        yield open_case(r, m, 0x5000)


# ---- uflow
def uflow_case(r, d=None, g=None, kind=None):
    c = base(r, "uflow")
    d = r.choice([0, 0, 1, 2, 5, 13, 14, 14, r.randrange(15)]) if d is None else d
    g = r.randrange(0, 15) if g is None else g
    c["ptrs"] = [0x30, 0x30 + g, 0x30 + d]
    c["cvt"] = True
    kind = r.choice(["plain", "single", "multi", "single", "multi"]) if kind is None else kind
    c["buf"][14] = r.choice([0, 1, 0xFF])
    n = r.randrange(0, 6)
    fg = [r.choice([r.randrange(256), r.randrange(0x80, 256), r.randrange(32, 127)]) for _ in range(n)] + [-1]
    q = {"fgetc": fg, "state": [r.getrandbits(32)], "tobyte": [r.randrange(256)],
         "in": [r.choice([0, 1, 2, 3, 3, 3, 4, 5, -1, 0x100]) for _ in range(r.randrange(1, 8))],
         "noconv": [0], "maxlen": [1]}
    if kind == "plain":
        c["buf"][15] = r.choice([1, 2, 0xFF])
    else:
        c["buf"][15] = 0
        q["maxlen"] = [1] if kind == "single" else [r.choice([0, 2, 3, 4, 5, -1])]
        if kind == "multi" and r.random() < 0.5:
            q["in"] = [r.choice([0, 1, 3, 3, 4]) for _ in range(6)] + [r.choice([2, 2, 3])]
    c["queues"] = q
    if r.random() < 0.08:
        c["file"] = 0
    elif c["file"] == 0:
        c["file"] = 0x5000
    return c


def uflow_edges():
    r = random.Random(13)
    for kind in ("plain", "single", "multi"):
        for d in (0, 1, 13, 14):
            for g in (0, 1, 5, 14):
                for _ in range(3):
                    yield uflow_case(r, d, g, kind)
    c = uflow_case(r, 3, 1, "plain")
    c["file"] = 0
    yield c
    for res in (0, 1, 2, 3, 4):
        for ml in (1, 3):
            c = uflow_case(r, 4, 4, "single" if ml == 1 else "multi")
            c["queues"]["in"] = [res, 2, 3]
            c["queues"]["maxlen"] = [ml]
            c["queues"]["fgetc"] = [0x80, 0xE9, 65, -1]
            yield c


TESTS = [
    FnTest("std::basic_filebuf<char,std::char_traits<char>>::basic_filebuf", ppc, host, edge=ctor_edges, rnd=ctor_case),
    FnTest("std::basic_filebuf<char,std::char_traits<char>>::open", ppc, host, edge=open_edges, rnd=open_case),
    FnTest("std::basic_filebuf<char,std::char_traits<char>>::uflow", ppc, host, edge=uflow_edges, rnd=uflow_case),
]
