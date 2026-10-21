"""Unit v5044, std::basic_string<wchar_t> replace / substr / begin, against libtpw_unit_std_basic_string_replace.dylib
(tests/unit_std_basic_string_replace.cpp), main app ("SimTheme Park"), src/game/MslSupport.cpp:
  replace 0x101f7740, substr 0x1021212c, begin 0x10212448

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (strlen, strcpy, operator new / new[] / delete, __throw, wmemcpy / wmemmove / wmemset, the counted-pointer constructor
0x100026ac and the substring constructor 0x101f7424) are fakes on both sides that record their arguments (texts as a hash) and do the
memory work for counts up to 4096 (a wrapped count is only logged).

The world of a case: a string object holding a pointer to a representation (length, capacity, count, data pointer; the data block has
200 spare characters) and the two terminator globals 0x102b1434 / 0x102b1436, random so that using the wrong one shows. What is
compared: the answer, the object's pointer (old / new / null), the old representation and its data block, the new representation with
its data, and the event log (including the thrown exception's vtable, message, counter and destructor vector).

No fused operations. Not tested: a length error (n2 or length - n1 past 0x7ffffffe): the original goes on with a length near 2^31 and
stores at data + 2 * length, which wraps on the Mac and is unmapped on the host.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_string_replace"
MASK = 0xFFFFFFFF

FN_REPLACE, FN_SUBSTR, FN_BEGIN = 0x101F7740, 0x1021212C, 0x10212448
SIZES = {FN_REPLACE: 1228, FN_SUBSTR: 264, FN_BEGIN: 348}
OWN = set(SIZES)

G_STRLEN, G_NEWA, G_NEW, G_DELETE, G_THROW, G_STRCPY = 0x10238650, 0x10238668, 0x102384B8, 0x10238218, 0x10238680, 0x10238410
G_WMEMCPY, G_WMEMMOVE, G_WMEMSET = 0x102389E0, 0x1023ABE8, 0x1023ABD0
F_RCP, F_CONSTRUCT = 0x100026AC, 0x101F7424

ADDR = dict(nulA=0x102B1434, nulB=0x102B1436, vtblA=0x10278BA0, vtblLen=0x10278B74, tvRange=0x10265698, tvLen=0x10265680)
HOST_LAYOUT = [0, 4, 8, 16, 24]
MAXOP = 4096
SPARE = 200


def hsh(b):
    h = 0x811C9DC5
    for x in b:
        h = ((h ^ x) * 0x01000193) & MASK
    return h


def cs(e, a):
    s = e.cstr(a)
    return s.encode("latin1") if isinstance(s, str) else bytes(s)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_strlen(e):
    b = cs(e, e.r(3))
    e.cl.rec("strlen", (hsh(b), len(b)))
    e.set_r(3, len(b))


def st_strcpy(e):
    b = cs(e, e.r(4))
    e.cl.rec("strcpy", (hsh(b),))
    e.write(e.r(3), b + b"\0")


def st_new(e):
    n = e.r(3) & MASK
    e.cl.rec("new", (n,))
    e.set_r(3, e.alloc(max(n, 4)))


def st_newa(e):
    n = e.r(3) & MASK
    e.cl.rec("new[]", (n,))
    e.set_r(3, e.alloc(max(n, 4)))


def st_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def st_throw(e):
    L = e.cl
    obj = e.r(4)
    vt, ps, pc = e.u32(obj), e.u32(obj + 4), e.u32(obj + 8)
    cnt = e.u32(pc) if pc else MASK
    L.rec("throw", (hsh(cs(e, e.r(3))), L.lab(vt), hsh(cs(e, ps)), cnt, L.lab(e.r(5))))


def mem_op(name, move):
    def fn(e):
        d, s, n = e.r(3), e.r(4), e.r(5) & MASK
        e.cl.rec(name, (e.cl.lab(d), e.cl.lab(s), n))
        if n <= MAXOP:
            e.write(d, e.read(s, 2 * n))
    return fn


def st_wmemset(e):
    d, c, n = e.r(3), e.r(4) & MASK, e.r(5) & MASK
    e.cl.rec("wmemset", (e.cl.lab(d), c, n))
    if n <= MAXOP:
        e.write(d, struct.pack(">H", c & 0xFFFF) * n)


def st_rcp(e):
    p = e.r(4)
    e.cl.rec("rcp", (1 if p else 0,))
    e.write(e.r(3), struct.pack(">II", p & MASK, 0))


def st_construct(e):
    L = e.cl
    L.rec("construct", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(5) & MASK, e.r(6) & MASK))


for _a, _f in ((G_STRLEN, st_strlen), (G_STRCPY, st_strcpy), (G_NEW, st_new), (G_NEWA, st_newa), (G_DELETE, st_delete),
               (G_THROW, st_throw), (G_WMEMCPY, mem_op("wmemcpy", False)), (G_WMEMMOVE, mem_op("wmemmove", True)),
               (G_WMEMSET, st_wmemset), (F_RCP, st_rcp), (F_CONSTRUCT, st_construct)):
    def _glue(e, _f=_f, _a=_a):
        if _a >= 0x10238000:      # a glue import: the glue saves the TOC pointer at 20(r1), the caller reloads it after the call
            e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        _f(e)
    STUBS.internal(_a, _glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ptr(self, a, p):
        self.write(a, struct.pack(self.pfmt, p))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psize))[0]

    def hw(self, a, vals):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(vals), *vals))

    def rd_hw(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def lay(self, case):
        self.w = {"self": self.alloc(16), "rep": self.alloc(0x20), "data": self.alloc(2 * len(case["data"])), "res": self.alloc(16)}


class Ppc(Side):
    fmt = ">I"
    pfmt, psize = ">I", 4
    doff = 0xC

    def __init__(self, e, case):
        self.e = e
        self.lay(case)
        w = self.w
        self.L = e.cl = CL.PpcLog({w["self"]: 1, w["rep"]: 2, w["data"]: 3, w["res"]: 4, ADDR["vtblA"]: 10, ADDR["vtblLen"]: 11,
                                   ADDR["tvRange"]: 12, ADDR["tvLen"]: 13}, case["queues"])
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

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt, psize = "<Q", 8
    doff = 0x10

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lay(case)
        w = self.w
        CL.host_begin(lib, {1: w["self"], 2: w["rep"], 3: w["data"], 4: w["res"], 10: self.g("vtblA"), 11: self.g("vtblLen"),
                            12: self.g("tvRange"), 13: self.g("tvLen")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sbr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sbr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        f = {FN_REPLACE: self.lib.tpw_sbr_replace, FN_SUBSTR: self.lib.tpw_sbr_substr, FN_BEGIN: self.lib.tpw_sbr_begin}[fn]
        return f(*args) & 0xFFFFFFFFFFFFFFFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sbr_addr.restype, lib.tpw_sbr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sbr_alloc.restype, lib.tpw_sbr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sbr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sbr_replace.restype = C.c_uint64
    lib.tpw_sbr_replace.argtypes = [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint32, C.c_uint32]
    lib.tpw_sbr_substr.restype = C.c_uint64
    lib.tpw_sbr_substr.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32]
    lib.tpw_sbr_begin.restype, lib.tpw_sbr_begin.argtypes = C.c_uint64, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sbr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    w = s.w
    s.ptr(w["self"], w["rep"])
    s.words(w["rep"], [case["len"], case["cap"], case["refs"]])
    s.ptr(w["rep"] + s.doff, w["data"])
    s.hw(w["data"], case["data"])
    s.hw(s.g("nulA"), [case["nulA"]])
    s.hw(s.g("nulB"), [case["nulB"]])


def execute(s, case):
    w = s.w
    op = case["op"]
    if op == "replace":
        pos, n1, n2, c = case["args"]
        return s.call(FN_REPLACE, [w["self"], pos, n1, n2, c]) == w["self"]
    if op == "substr":
        pos, n = case["args"]
        return s.call(FN_SUBSTR, [w["res"], w["self"], pos, n]) == w["res"]
    ret = s.call(FN_BEGIN, [w["self"]])
    rep = s.rd_ptr(w["self"])
    return ret == s.rd_ptr(rep + s.doff)


def rep_state(s, a):
    ln, cap, refs = s.rd_words(a, 3)
    dp = s.rd_ptr(a + s.doff)
    return (ln, cap, refs, s.rd_hw(dp, cap + 1) if dp and cap < 4096 else None)


def observe(s, case, answer):
    w = s.w
    cur = s.rd_ptr(w["self"])
    kind = "null" if cur == 0 else "old" if cur == w["rep"] else "new"
    ln, cap, refs = s.rd_words(w["rep"], 3)
    dp = s.rd_ptr(w["rep"] + s.doff)
    return {"answer": answer, "log": s.log(), "self": kind,
            "old": (ln, cap, refs, "data" if dp == w["data"] else dp, s.rd_hw(w["data"], len(case["data"]))),
            "new": rep_state(s, cur) if kind == "new" else None,
            "res": s.rd_ptr(w["res"])}


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


# ---------------------------------------------------------------------------------------------------------------- case generators


def base(r, op, ln=None, cap=None, refs=None):
    cap = 4 * r.randrange(0, 10) + 3 if cap is None else cap
    ln = r.choice([0, cap, r.randrange(0, cap + 1), r.randrange(0, cap + 1)]) if ln is None else ln
    refs = r.choice([0, 1, 1, 2, 3]) if refs is None else refs
    return {"op": op, "queues": {}, "len": ln, "cap": cap, "refs": refs,
            "data": [r.randrange(0x10000) for _ in range(cap + 1 + SPARE)],
            "nulA": r.randrange(1, 0x10000), "nulB": r.randrange(1, 0x10000)}


def pick_pos(r, ln):
    return r.choice([0, ln, max(ln - 1, 0), r.randrange(0, ln + 1), r.randrange(0, ln + 1), ln + 1, ln + r.randrange(1, 6)])


def pick_n1(r, ln, pos):
    avail = (ln - pos) & MASK
    if pos > ln:
        return r.choice([0, r.randrange(0, ln + 1), MASK])
    return r.choice([0, 1, r.randrange(0, ln + 2), avail, (avail + 1) & MASK, MASK])


def n2_options(r, c, n1, pos):
    ln, cap = c["len"], c["cap"]
    avail = (ln - pos) & MASK
    eff = min(avail, n1)
    b = cap - ((ln - eff) & MASK) if ((ln - eff) & MASK) <= cap else 0
    return [0, 1, 2, r.randrange(0, 12), max(b - 1, 0), max(b, 0), b + 1, r.randrange(0, 60)]


def replace_case(r, ln=None, cap=None, refs=None, pos=None, n1=None, n2=None):
    c = base(r, "replace", ln, cap, refs)
    pos = pick_pos(r, c["len"]) if pos is None else pos
    n1 = pick_n1(r, c["len"], pos) if n1 is None else n1
    n2 = r.choice(n2_options(r, c, n1, pos)) if n2 is None else n2
    c["args"] = [pos, n1, n2, r.choice([0, 0xFFFF, r.randrange(0x10000), r.randrange(0x10000)])]
    return c


def replace_edges():
    r = random.Random(5044)
    for ln, cap in ((0, 3), (3, 3), (3, 7), (7, 7), (5, 11)):
        for refs in (0, 1, 2):
            for pos in sorted({0, 1, ln, max(ln - 1, 0), ln + 1}):
                avail = (ln - pos) & MASK
                for n1 in sorted({0, 1, avail, (avail + 1) & MASK, MASK}):
                    if pos > ln and n1 not in (0, MASK):
                        continue
                    c0 = base(r, "replace", ln, cap, refs)
                    for n2 in sorted(set(n2_options(r, c0, n1, pos)[:3] + n2_options(r, c0, n1, pos)[4:7])):
                        yield replace_case(r, ln, cap, refs, pos, n1, n2)


def replace_random(r):
    return replace_case(r)


def substr_case(r, ln=None, pos=None, n=None):
    c = base(r, "substr", ln)
    ln = c["len"]
    pos = pick_pos(r, ln) if pos is None else pos
    avail = (ln - pos) & MASK
    n = r.choice([0, 1, r.randrange(0, ln + 2), avail, (avail + 1) & MASK, MASK]) if n is None else n
    c["args"] = [pos, n]
    return c


def substr_edges():
    r = random.Random(1021)
    for ln in (0, 1, 4, 7):
        for pos in sorted({0, 1, ln, max(ln - 1, 0), ln + 1, ln + 4}):
            avail = (ln - pos) & MASK
            for n in sorted({0, 1, avail, (avail + 1) & MASK, MASK, 3}):
                yield substr_case(r, ln, pos, n)


def substr_random(r):
    return substr_case(r)


def begin_case(r, refs=None, ln=None):
    c = base(r, "begin", ln, None, refs)
    c["args"] = []
    return c


def begin_edges():
    r = random.Random(2448)
    for refs in (0, 1, 2, 3, 9):
        for ln in (0, 1, 3, 4, 7):
            yield begin_case(r, refs, ln)


def begin_random(r):
    return begin_case(r, r.choice([0, 1, 2, 3, 5, 2]))


PFX = "std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>::"
TESTS = [
    FnTest(PFX + "replace", ppc, host, edge=replace_edges, rnd=replace_random),
    FnTest(PFX + "substr", ppc, host, edge=substr_edges, rnd=substr_random),
    FnTest(PFX + "begin", ppc, host, edge=begin_edges, rnd=begin_random),
]
