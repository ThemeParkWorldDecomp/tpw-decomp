"""Unit v51033: basic_string<char>::operator[], std::locale::locale, basic_ios<char>::widen, _RefCountedPtr<__locale_imp> destructor,
basic_filebuf<char>::close and underflow, against libtpw_unit_std_basic_string_operator_3.dylib (tests/unit_std_basic_string_operator_3.cpp),
main app ("SimTheme Park"), src/std_basic_string_operator_3.cpp:
  operator[] 0x100733b0, locale::locale 0x10072d24, widen 0x10072960, ~_RefCountedPtr 0x10071f34, close 0x10071b54, underflow 0x10071a80

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian,
4-byte pointers) and in the host library's memory (little-endian, 8-byte pointers where the host structs have native pointers). The callees
outside the unit (operator new/delete, memcpy, the locale impl destructor, classic, locale operator=, getloc, use_facet, the virtual calls
through __ptr_glue, fclose, unshift) are fakes on both sides recording the same events; a pointer is logged as the tag word its block
holds at +0x18.

What is compared: the answer, the string / locale / filebuf state read back (pointers as tags or offsets), the reference counts and the log.
No fused operations. Not tested: the exception (catch) paths of operator[] (reached only by a throwing operator new); a NULL self for
operator[], locale, widen, close, underflow (read unconditionally); the filebuf vtable word (host value is opaque, only the slot is checked
on the PowerPC side through the r12 value the __ptr_glue fake sees).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_string_operator_3"
MASK = 0xFFFFFFFF

FN_INDEX, FN_LOCALE, FN_WIDEN, FN_DTOR, FN_CLOSE, FN_UNDERFLOW = 0x100733B0, 0x10072D24, 0x10072960, 0x10071F34, 0x10071B54, 0x10071A80
SIZES = {FN_INDEX: 328, FN_LOCALE: 160, FN_WIDEN: 136, FN_DTOR: 136, FN_CLOSE: 164, FN_UNDERFLOW: 132}
OWN = set(SIZES)

NEW, MEMCPY, DELETE, CLASSIC, ASSIGN = 0x102384B8, 0x102386B0, 0x10238218, 0x10238998, 0x10072128
GETLOC, USEFACET, PTRGLUE, IMPDTOR, UNSHIFT, FCLOSE = 0x10072A34, 0x10073934, 0x1023BAA8, 0x100737A0, 0x10072A94, 0x102388C0

ADDR = dict(global_s=0x103BCFD4, classic=0x103BCFE0)
HOST_LAYOUT = [16, 8, 0x40, 0x24, 0x3E, 0xC]

VT_WIDEN, VT_UNDERFLOW = 0xA1C, 0xA2C

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def tag(e, p):
    return e.u32(p + 0x18) if p else 0


def stub_new(e):
    n = e.r(3)
    a = e.alloc(n + 0x20)
    e.write(a, bytes(n + 0x20))
    e.cl.rec("New", (n,))
    e.set_r(3, a)


def stub_memcpy(e):
    d, s, n = e.r(3), e.r(4), e.r(5)
    e.cl.rec("Memcpy", (n,))
    e.write(d, e.read(s, n))
    e.set_r(3, d)


def stub_delete(e):
    e.cl.rec("Delete", (tag(e, e.r(3)),))


def stub_classic(e):
    e.cl.rec("Classic", (0,))
    e.set_r(3, ADDR["classic"])


def stub_assign(e):
    this, src = e.r(3), e.r(4)
    e.cl.rec("Assign", (tag(e, this), tag(e, src), tag(e, e.u32(src)), tag(e, e.u32(src + 4))))
    e.set_r(3, this)


def stub_getloc(e):
    out, this = e.r(3), e.r(4)
    e.cl.rec("Getloc", (tag(e, this),))
    e.write(out, e.read(this, 8))


def stub_usefacet(e):
    imp = e.u32(e.r(3))
    e.cl.rec("UseFacet", (tag(e, imp),))
    e.set_r(3, e.u32(imp + 8))


def stub_ptrglue(e):
    L = e.cl
    r12 = e.r(12)
    if r12 == VT_WIDEN:
        L.rec("Widen", (tag(e, e.r(3)), T.s8(e.r(4))))
        e.set_r(3, L.ret("Widen") & MASK)
    elif r12 == VT_UNDERFLOW:
        L.rec("VUnderflow", (tag(e, e.r(3)),))
        e.set_r(3, L.ret("VUnderflow") & MASK)
    else:
        L.rec("BadVirtual", (r12,))


def stub_impdtor(e):
    e.cl.rec("ImpDtor", (tag(e, e.r(3)), T.s32(e.r(4))))


def stub_unshift(e):
    e.cl.rec("Unshift", (tag(e, e.r(3)),))
    e.set_r(3, e.cl.ret("Unshift") & MASK)


def stub_fclose(e):
    e.cl.rec("Fclose", (e.r(3),))
    e.set_r(3, e.cl.ret("Fclose") & MASK)


for _a, _f in ((NEW, stub_new), (MEMCPY, stub_memcpy), (DELETE, stub_delete), (CLASSIC, stub_classic), (ASSIGN, stub_assign),
               (GETLOC, stub_getloc), (USEFACET, stub_usefacet), (PTRGLUE, stub_ptrglue), (IMPDTOR, stub_impdtor),
               (UNSHIFT, stub_unshift), (FCLOSE, stub_fclose)):
    STUBS.internal(_a, _f)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptr(self, a, p):
        self.write(a, struct.pack(self.pfmt, p))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.P))[0]

    def tag(self, p):
        return self.rd_words(p + 0x18, 1)[0] if p else 0


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    P = 4
    CNT = 4
    DATA = 0xC
    base = 0

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "global_s":
            return self.e.u32(0x10260C8C)      # the TOC slot -0x7034 holds the global's address
        return ADDR[name]

    def alloc(self, n):
        a = self.e.alloc(n)
        self.e.write(a, bytes(n))
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        return T.call_ppc(self.e, fn, [a & MASK for a in args])[0] & MASK

    def call_index(self, st, i):
        return self.call(FN_INDEX, [st, i])

    def call_locale(self, p):
        return self.call(FN_LOCALE, [p])

    def call_widen(self, p, c):
        return self.call(FN_WIDEN, [p, c])

    def call_dtor(self, p, flag):
        return self.call(FN_DTOR, [p, flag])

    def call_close(self, p):
        return self.call(FN_CLOSE, [p])

    def call_underflow(self, p):
        return self.call(FN_UNDERFLOW, [p])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    P = 8
    CNT = 8
    DATA = 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_s_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_s_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_s_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_index(self, st, i):
        return self.lib.tpw_s_index(st, i & MASK) & MASK

    def call_locale(self, p):
        return self.lib.tpw_s_locale(p)

    def call_widen(self, p, c):
        return self.lib.tpw_s_widen(p, T.s32(c))

    def call_dtor(self, p, flag):
        return self.lib.tpw_s_dtor(p, T.s32(flag))

    def call_close(self, p):
        return self.lib.tpw_s_close(p)

    def call_underflow(self, p):
        return self.lib.tpw_s_underflow(p) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_s_addr.restype, lib.tpw_s_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_s_base.restype, lib.tpw_s_base.argtypes = C.c_uint64, []
    lib.tpw_s_alloc.restype, lib.tpw_s_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_s_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_s_index.restype, lib.tpw_s_index.argtypes = C.c_uint64, [C.c_uint64, C.c_uint32]
    lib.tpw_s_locale.restype, lib.tpw_s_locale.argtypes = C.c_uint64, [C.c_uint64]
    lib.tpw_s_widen.restype, lib.tpw_s_widen.argtypes = C.c_int32, [C.c_uint64, C.c_int32]
    lib.tpw_s_dtor.restype, lib.tpw_s_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_s_close.restype, lib.tpw_s_close.argtypes = C.c_uint64, [C.c_uint64]
    lib.tpw_s_underflow.restype, lib.tpw_s_underflow.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_s_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def blk(s, t, size=0x80):
    a = s.alloc(size)
    s.words(a + 0x18, [t])
    return a


def vtable(s):
    v = s.alloc(0x80)
    s.words(v, [0xD000 + 4 * i for i in range(0x20)])
    s.words(v + 0x1C, [VT_WIDEN])
    s.words(v + 0x2C, [VT_UNDERFLOW])
    return v


def locale_blocks(s, case):
    """this -> locale object {imp, count}; imp holds the facet pointer at +8; the facet's first word is the vtable."""
    facet = blk(s, 0x3333)
    s.words(facet, [vtable(s)])
    imp = blk(s, 0x2222)
    s.ptr(imp + 8, facet)
    cnt = blk(s, 0x4444)
    s.words(cnt, [case["count"]])
    this = blk(s, 0x1111) if case.get("this", True) else 0
    if this:
        s.ptr(this, imp)
        s.ptr(this + s.CNT, cnt)
    return dict(this=this, cnt=cnt, imp=imp)


def build(s, case):
    op = case["op"]
    if op == "index":
        cap = case["cap"]
        data = s.alloc(cap + 17)
        s.write(data, bytes(case["bytes"]))
        rep = s.alloc(0x40)
        s.words(rep, [case["len"], cap, case["refs"]])
        s.ptr(rep + s.DATA, data)
        st = s.alloc(16)
        s.ptr(st, rep)
        return dict(st=st, rep=rep, data=data)
    if op == "locale":
        imp = blk(s, 0x6001)
        cnt = blk(s, 0x6002)
        s.words(cnt, [case["count"]])
        g = s.g("global_s")
        s.ptr(g, imp if case["has_imp"] else 0)
        s.ptr(g + s.CNT, cnt if case["has_cnt"] else 0)
        s.ptr(s.g("classic"), 0)
        s.ptr(s.g("classic") + s.CNT, 0)
        s.words(s.g("classic") + 0x18, [0xC1A55])
        this = s.alloc(0x40)
        s.write(this, b"\xaa" * 0x40)
        s.words(this + 0x18, [0x7001])
        return dict(this=this, cnt=cnt, g=g)
    if op in ("widen", "dtor"):
        return locale_blocks(s, case)
    obj = s.alloc(0x80)
    s.words(obj, [vtable(s) - s.base])
    ow = obj - s.base      # the 32-bit address of the object (an arena offset on the host)
    s.words(obj + 4, [ow + case["eback"], ow + case["gptr"], ow + case["egptr"]])
    s.words(obj + 0x18, [0x7777])
    s.words(obj + 0x24, [case["file"]])
    s.write(obj + 0x30, bytes(case["buf"]))
    return dict(this=obj)


def rel(ans, base):
    return 1 if ans == base else (0 if ans == 0 else ans)


def observe(s, case, ctx, ans):
    op = case["op"]
    out = {"log": s.log()}
    if op == "index":
        rep = s.rd_ptr(ctx["st"])
        out["same"] = rep == ctx["rep"]
        w = s.rd_words(rep, 3)
        data = s.rd_ptr(rep + s.DATA)
        out["rep"] = w
        out["data"] = s.read(data, w[1] + 1)
        out["ans"] = (ans - data) & MASK
        old = s.rd_words(ctx["rep"], 3)
        out["old"] = old
        out["olddata"] = s.read(s.rd_ptr(ctx["rep"] + s.DATA), old[1] + 1)
    elif op == "locale":
        t = ctx["this"]
        out["ret"] = rel(ans, t)
        out["imp"] = s.tag(s.rd_ptr(t))
        out["cnt"] = s.tag(s.rd_ptr(t + s.CNT))
        out["count"] = s.rd_words(ctx["cnt"], 1)
        out["rest"] = s.rd_words(t + 0x10, 12)
        out["g"] = [s.tag(s.rd_ptr(ctx["g"])), s.tag(s.rd_ptr(ctx["g"] + s.CNT))]
    elif op == "widen":
        out["ans"] = ans & 0xFF
        out["count"] = s.rd_words(ctx["cnt"], 1)
    elif op == "dtor":
        out["ret"] = rel(ans, ctx["this"])
        out["count"] = s.rd_words(ctx["cnt"], 1)
    else:
        obj = ctx["this"]
        w = s.rd_words(obj, 12)
        w[0] = 0
        for i in (1, 2, 3):
            w[i] = (w[i] - (obj - s.base)) & MASK
        out["obj"] = w
        out["buf"] = s.read(obj + 0x30, 16)
        out["tail"] = s.rd_words(obj + 0x40, 16)
        out["ans"] = rel(ans, obj) if op == "close" else ans
    return out


def execute(s, case, ctx):
    op = case["op"]
    if op == "index":
        return s.call_index(ctx["st"], case["i"])
    if op == "locale":
        return s.call_locale(ctx["this"])
    if op == "widen":
        return s.call_widen(ctx["this"], case["c"])
    if op == "dtor":
        return s.call_dtor(ctx["this"], case["flag"])
    if op == "close":
        return s.call_close(ctx["this"])
    return s.call_underflow(ctx["this"])


def run(s, case):
    try:
        ctx = build(s, case)
        return observe(s, case, ctx, execute(s, case, ctx))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

COUNTS = [0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
FLAGS = [-32768, -1, 0, 1, 2, 5, 32767]
UNSHIFT = [0, 1, 0x100, 0x1FF, 0xFFFFFFFF, 0x80000000, 0xFF]
FCLOSE = [0, 1, -1, 5]
VUNDER = [-1, 0, 0x41, 0xFF, 0x100, 5, 0x7FFFFFFF]
CHARS = [0, 0x41, -1, 0x7F, -128, 1, 0x80]


def queues(r):
    return {"Unshift": [r.choice(UNSHIFT)], "Fclose": [r.choice(FCLOSE)], "VUnderflow": [r.choice(VUNDER)],
            "Widen": [r.randrange(0, 256)]}


def index_case(r, ln=None, refs=None, i=None):
    ln = r.randrange(0, 60) if ln is None else ln
    cap = ln + r.randrange(0, 6)
    return {"op": "index", "queues": queues(r), "len": ln, "cap": cap, "refs": r.choice([0, 1, 2, 3, 100, 0xFFFFFFFF]) if refs is None else refs,
            "bytes": [r.randrange(256) for _ in range(cap + 1)], "i": r.randrange(0, 70) if i is None else i}


def index_edges():
    r = random.Random(11)
    for refs in (0, 1, 2, 3, 0xFFFFFFFF):
        for ln in (0, 1, 3, 4, 7):
            for i in (0, ln, ln + 3):
                yield index_case(r, ln, refs, i)


def locale_case(r, hi=None, hc=None, count=None):
    return {"op": "locale", "queues": queues(r), "has_imp": r.random() < 0.5 if hi is None else hi,
            "has_cnt": r.random() < 0.7 if hc is None else hc, "count": r.choice(COUNTS) if count is None else count}


def locale_edges():
    r = random.Random(12)
    for hi in (False, True):
        for hc in (False, True):
            for c in COUNTS:
                yield locale_case(r, hi, hc, c)


def widen_case(r, count=None, c=None):
    return {"op": "widen", "queues": queues(r), "count": r.choice(COUNTS) if count is None else count,
            "c": r.choice(CHARS) if c is None else c}


def widen_edges():
    r = random.Random(13)
    for count in COUNTS:
        for c in CHARS:
            yield widen_case(r, count, c)


def dtor_case(r, this=None, flag=None, count=None):
    return {"op": "dtor", "queues": queues(r), "this": r.random() < 0.9 if this is None else this,
            "flag": r.choice(FLAGS) if flag is None else flag, "count": r.choice(COUNTS) if count is None else count}


def dtor_edges():
    r = random.Random(14)
    for this in (False, True):
        for flag in FLAGS:
            for count in COUNTS:
                yield dtor_case(r, this, flag, count)


def fb_common(r, op):
    return {"op": op, "queues": queues(r), "eback": r.randrange(-0x100, 0x100), "gptr": r.randrange(-0x100, 0x100),
            "egptr": r.randrange(-0x100, 0x100), "file": r.choice([0, r.randrange(1, 1 << 32)]),
            "buf": [r.randrange(256) for _ in range(16)]}


def close_case(r, file=None, f3e=None, un=None, fc=None):
    c = fb_common(r, "close")
    if file is not None:
        c["file"] = file
    if f3e is not None:
        c["buf"][14] = f3e
    if un is not None:
        c["queues"]["Unshift"] = [un]
    if fc is not None:
        c["queues"]["Fclose"] = [fc]
    return c


def close_edges():
    r = random.Random(15)
    for file in (0, 0x1234):
        for f3e in (0, 1, 0x80, 0xFF):
            for un in UNSHIFT:
                for fc in FCLOSE:
                    yield close_case(r, file, f3e, un, fc)


def underflow_case(r, file=None, g=None, d=None, v=None):
    c = fb_common(r, "underflow")
    g = r.choice([r.randrange(0x30, 0x7F), -0x20]) if g is None else g
    d = r.choice([-3, -1, 0, 1, 2, 5, 0x1000]) if d is None else d
    if g < 0x30 and d > 0:
        d = 0
    c["gptr"] = g
    c["egptr"] = g + d
    if file is not None:
        c["file"] = file
    if v is not None:
        c["queues"]["VUnderflow"] = [v]
    c["buf"][14] = r.randrange(256)
    return c


def underflow_edges():
    r = random.Random(16)
    for file in (0, 0x1234):
        for g in (0x30, 0x7E, -0x20):
            for d in (-3, -1, 0, 1, 2, 0x1000):
                for v in VUNDER:
                    yield underflow_case(r, file, g, d, v)


TESTS = [
    FnTest("std::basic_string<char,std::char_traits<char>,std::allocator<char>>::operator[]", ppc, host, edge=index_edges,
           rnd=lambda r: index_case(r)),
    FnTest("std::locale::locale", ppc, host, edge=locale_edges, rnd=lambda r: locale_case(r)),
    FnTest("std::basic_ios<char,std::char_traits<char>>::widen", ppc, host, edge=widen_edges, rnd=lambda r: widen_case(r)),
    FnTest("std::_RefCountedPtr<std::__locale_imp<__int128>,std::_Single<std::__locale_imp<__int128>>>::~_RefCountedPtr", ppc, host,
           edge=dtor_edges, rnd=lambda r: dtor_case(r)),
    FnTest("std::basic_filebuf<char,std::char_traits<char>>::close", ppc, host, edge=close_edges, rnd=lambda r: close_case(r)),
    FnTest("std::basic_filebuf<char,std::char_traits<char>>::underflow", ppc, host, edge=underflow_edges,
           rnd=lambda r: underflow_case(r)),
]
