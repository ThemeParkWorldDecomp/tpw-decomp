"""Unit v5008, std::basic_string<wchar_t> of the Metrowerks library, against libtpw_unit_std_basic_string_compare.dylib
(tests/unit_std_basic_string_compare.cpp), main app, src/UI_Base_std_basic_string_compare.cpp:
  compare 0x101d5864, assign 0x101f7014, basic_string(str, pos, n) 0x101f7424

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers) for the translation. The callees outside the unit
(strlen, strcpy, operator new / new[] / delete / delete[], __throw, wmemcmp, wmemcpy, the _RefCountedPtr constructor, change_size,
replace, init) are fakes on both sides that record every argument (strings and character runs as FNV hashes, objects as labels) and
answer from the case. The fakes of __throw return, so the code after the throw runs on, as in the original.

The world of a case: a "this" string (object, representation of length / capacity / reference count, a 64-character buffer) and a "src"
string; the src representation may be the this representation, the src object may be the this object. What is compared: the answer,
the event log, both string objects (the representation they point to, its fields and its characters) and both original representations.

No fused operations. Not tested: a pos past the length together with huge counts (the original then reads far past the buffer).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_string_compare"
MASK = 0xFFFFFFFF
BIG = 0xFFFFFFFF

FN_COMPARE, FN_ASSIGN, FN_CTOR = 0x101D5864, 0x101F7014, 0x101F7424
SIZES = {FN_COMPARE: 364, FN_ASSIGN: 868, FN_CTOR: 628}
OWN = set(SIZES)

# callees faked on both sides
G_STRLEN, G_NEWA, G_STRCPY, G_NEW, G_DELA, G_DEL = "strlen", "__nwa__FUl", "strcpy", "__nw__FUl", "__dla__FPv", "__dl__FPv"
CALLEE_RC = 0x100026AC
CALLEE_CHANGE = 0x101B2CD8
CALLEE_REPLACE = 0x101F7740
CALLEE_INIT = 0x10146CF8

ADDR = dict(excA=0x10278BA0, excB=0x10278BB0, excC=0x10278BC0, tv=0x10265698)
HOST_LAYOUT = [0, 4, 8, 16, 8]
NCH = 64


def fnv(vals):
    h = 2166136261
    for v in vals:
        h = ((h ^ v) * 16777619) & MASK
    return h


def cbytes(e, a):
    if a == 0:
        return b""
    s = e.cstr(a)
    return s.encode("latin-1") if isinstance(s, str) else bytes(s)


def r16(e, a, n):
    if n <= 0:
        return []
    return list(struct.unpack(">%dH" % n, e.read(a, 2 * n)))


# ---------------------------------------------------------------------------------------------------------------- PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_strlen(e):
    b = cbytes(e, e.r(3))
    e.cl.rec("strlen", (fnv(b), len(b)))
    e.set_r(3, len(b))


def st_strcpy(e):
    d, s = e.r(3), e.r(4)
    b = cbytes(e, s)
    e.cl.rec("strcpy", (e.cl.lab(d), fnv(b)))
    e.write(d, b + b"\0")
    e.set_r(3, d)


def mk_new(name):
    def f(e):
        size = e.r(3)
        e.cl.rec(name, (size,))
        e.set_r(3, e.alloc(max(size, 32)))
    return f


def mk_del(name):
    def f(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
    return f


def st_rc(e):
    p = e.r(4)
    e.cl.rec("rcptr", (1 if p else 0,))
    e.write(e.r(3), struct.pack(">II", p, 0))


def st_throw(e):
    L = e.cl
    obj = e.r(4)
    vt, ptr = e.u32(obj), e.u32(obj + 4)
    L.rec("throw", (fnv(cbytes(e, e.r(3))), L.lab(vt), fnv(cbytes(e, ptr)), L.lab(e.r(5))))


def st_wmemcmp(e):
    a, b, n = e.r(3), e.r(4), e.r(5)
    k = min(n, 16)
    e.cl.rec("wmemcmp", (n, fnv(r16(e, a, k)), fnv(r16(e, b, k))))
    e.set_r(3, e.cl.ret("wmemcmp") & MASK)


def st_wmemcpy(e):
    d, s, n = e.r(3), e.r(4), e.r(5)
    e.cl.rec("wmemcpy", (n, e.cl.lab(d), fnv(r16(e, s, min(n, 8)))))
    if n:
        e.write(d, e.read(s, 2 * n))
    e.set_r(3, d)


def st_change(e):
    e.cl.rec("change_size", (e.cl.lab(e.r(3)), e.r(4), e.r(5) & 0xFF))


def st_replace(e):
    e.cl.rec("replace", (e.cl.lab(e.r(3)), e.r(4), e.r(5), e.r(6), e.r(7) & 0xFFFF))


def st_init(e):
    first, last = e.r(4), e.r(5)
    n = ((last - first) & MASK) // 2
    chars = r16(e, first, min(n, 8))
    chars += [0] * (8 - len(chars))
    e.cl.rec("init", (e.cl.lab(e.r(3)), n) + tuple(chars))


STUBS.imp(G_STRLEN, st_strlen)
STUBS.imp(G_STRCPY, st_strcpy)
STUBS.imp(G_NEW, mk_new("new"))
STUBS.imp(G_NEWA, mk_new("newa"))
STUBS.imp(G_DEL, mk_del("delete"))
STUBS.imp(G_DELA, mk_del("deletea"))
STUBS.imp("__throw", st_throw)
STUBS.imp("wmemcmp", st_wmemcmp)
STUBS.imp("wmemcpy", st_wmemcpy)
STUBS.imp("__end__catch", lambda e: e.cl.rec("end_catch", ()))
STUBS.internal(CALLEE_RC, st_rc)
STUBS.internal(CALLEE_CHANGE, st_change)
STUBS.internal(CALLEE_REPLACE, st_replace)
STUBS.internal(CALLEE_INIT, st_init)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def chars(self, a, ws):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(ws), *ws))

    def rd_chars(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n))) if n > 0 else []

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    DOFF = 0xC

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = None

    def start(self, pairs):
        self.L = self.e.cl = CL.PpcLog({a: l for a, l in pairs}, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
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

    def call(self, op, args):
        entry = {"compare": FN_COMPARE, "assign": FN_ASSIGN, "construct": FN_CTOR}[op]
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        r3 &= MASK
        return T.s32(r3) if op == "compare" else r3


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    DOFF = 16

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def start(self, pairs):
        CL.host_begin(self.lib, {l: a for a, l in pairs}, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sbs_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sbs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        r = getattr(self.lib, "tpw_sbs_" + op)(*args)
        return T.s32(r) if op == "compare" else r


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sbs_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    f = getattr(lib, p + "compare")
    f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64, C.c_uint32]
    for n in ("assign", "construct"):
        f = getattr(lib, p + n)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def mk_rep(s, d):
    data = s.alloc(2 * NCH + 16)
    s.chars(data, chars_of(d["seed"]))
    rep = s.alloc(32)
    s.words(rep, [d["len"], d["cap"], d["refs"]])
    s.wptr(rep + s.DOFF, data)
    return rep, data


def mk_obj(s, rep):
    o = s.alloc(16)
    if rep:
        s.wptr(o, rep)
    return o


def build(s, case):
    w = {}
    op = case["op"]
    w["this_rep"] = w["this_data"] = None
    if op != "construct":
        w["this_rep"], w["this_data"] = mk_rep(s, case["this"])
        w["this_obj"] = mk_obj(s, w["this_rep"])
    else:
        w["this_obj"] = mk_obj(s, None)
    if case["same_rep"] and op != "construct":
        w["src_rep"], w["src_data"] = w["this_rep"], w["this_data"]
    else:
        w["src_rep"], w["src_data"] = mk_rep(s, case["src"])
    if case["same_obj"] and op == "assign":
        w["src_obj"] = w["this_obj"]
    else:
        w["src_obj"] = mk_obj(s, w["src_rep"])
    pairs, seen = [], set()
    for a, l in ((w["this_obj"], 1), (w["src_obj"], 2), (w["this_rep"], 3), (w["this_data"], 4), (w["src_rep"], 5), (w["src_data"], 6),
                 (s.g("excA"), 7), (s.g("excB"), 8), (s.g("excC"), 9), (s.g("tv"), 10)):
        if a and a not in seen:
            seen.add(a)
            pairs.append((a, l))
    w["pairs"] = pairs
    w["known"] = {a: l for a, l in pairs}
    s.w = w
    s.start(pairs)


def execute(s, case):
    w = s.w
    op, (pos, n1, n2) = case["op"], case["args"]
    if op == "compare":
        return s.call("compare", [w["this_obj"], pos, n1, w["src_data"], n2])
    r = s.call(op, [w["this_obj"], w["src_obj"], pos, n1])
    return r == w["this_obj"]


def snaprep(s, rp):
    if rp == 0:
        return None
    known = s.w["known"]
    ln, cap, refs = s.rd_words(rp, 3)
    d = s.rptr(rp + s.DOFF)
    cnt = NCH if d in known else min(ln + 1, NCH)
    return (ln, cap, refs, known.get(d, "new"), s.rd_chars(d, cnt) if d else None)


def observe(s, case, answer):
    w = s.w
    known = w["known"]
    out = {"answer": answer, "log": s.log()}
    for name in ("this_obj", "src_obj"):
        rp = s.rptr(w[name])
        out[name] = (known.get(rp, "new"), snaprep(s, rp))
    for name in ("this_rep", "src_rep"):
        if w[name]:
            out[name] = snaprep(s, w[name])
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


# ---------------------------------------------------------------------------------------------------------------- case generators


def rstate(r, ln=None, refs=None):
    if ln is None:
        ln = r.choice([0, 1, 2, 3, 4, 5, 8, 13, 20])
    cap = ln + 3 - (ln & 3) if r.random() < 0.7 else ln + r.randrange(0, 9)
    if refs is None:
        refs = r.choice([0, 0, 1, 2, 5])
    return {"len": ln, "cap": cap, "refs": refs, "seed": r.randrange(1 << 30)}


def chars_of(seed):
    r = random.Random(seed)
    return [r.choice([0x41, 0x42, 0x43, 0x141, 0xFFFF, r.randrange(1, 65536)]) for _ in range(NCH)]


def mkcase(r, op, this, src, pos, n1, n2=0, same_rep=False, same_obj=False):
    if same_rep and op == "assign":
        src = this           # one representation: the source is the this state
    if op != "compare" and pos > src["len"]:
        n1 = min(n1, 8)      # past the end the original copies that many characters: keep it inside the buffer
    q = {"wmemcmp": [r.choice([0, 0, -1, 1, 7, -5])]} if op == "compare" else {}
    return Case({"op": op, "queues": q, "this": this, "src": src, "same_rep": same_rep, "same_obj": same_obj, "args": (pos, n1, n2)})


class Case(dict):
    """A short repr, so the judge's first-failure line shows the mismatches."""

    def __repr__(self):
        t, s = self["this"], self["src"]
        return "%s T%s S%s rep=%d obj=%d a=%s q=%s" % (self["op"], (t["len"], t["refs"]), (s["len"], s["refs"]), self["same_rep"],
                                                       self["same_obj"], self["args"], self["queues"])


def pos_choice(r, ln):
    return r.choice([0, 0, 1, ln // 2, ln, ln, ln + 1, ln + 3, ln + 9])


def count_choice(r, avail):
    return r.choice([0, 1, 2, avail, avail, avail + 1, max(avail - 1, 0), BIG, BIG, r.randrange(0, 25)])


def rand_compare(r):
    this, src = rstate(r), rstate(r)
    ln = this["len"]
    pos = pos_choice(r, ln)
    n1 = count_choice(r, max(ln - pos, 0)) if pos <= ln else r.choice([0, 1, 3, 8])
    m1 = min(n1, (ln - pos) & MASK)
    n2 = r.choice([0, 1, m1, m1, m1 + 1, max(m1 - 1, 0), 7, BIG]) if m1 <= 16 else r.choice([0, 1, 3, m1])
    return mkcase(r, "compare", this, src, pos, n1, n2)


def rand_assign(r, op):
    this, src = rstate(r), rstate(r)
    sl = src["len"]
    pos = pos_choice(r, sl)
    n = count_choice(r, max(sl - pos, 0)) if pos <= sl else r.choice([0, 1, 3, 8])
    mode = r.choice(["no", "no", "rep", "obj"]) if op == "assign" else "no"
    return mkcase(r, op, this, src, pos, n, 0, mode != "no", mode == "obj")


def a_edges():
    r = random.Random(101)
    for ln in (0, 1, 4, 9):
        for pos in sorted({0, 1, ln, ln + 1, ln + 5}):
            for n1 in (0, 2, 7, BIG):
                for n2 in (0, 2, 7, BIG):
                    if pos > ln and (n1 > 16 and n2 > 16):
                        continue
                    this = rstate(r, ln)
                    yield mkcase(r, "compare", this, rstate(r), pos, n1, n2)


def assign_edges():
    r = random.Random(102)
    for lt in (4,):
        for ls in (0, 6):
            for rs in (0, 1, 3):
                for rt in (0, 1, 2):
                    for mode in ("no", "rep", "obj"):
                        for pos, n in ((0, BIG), (0, ls), (1, BIG), (ls + 1, 2), (2, 3), (0, 0), (ls, 5)):
                            this = rstate(r, lt, rt)
                            src = rstate(r, ls, rs)
                            yield mkcase(r, "assign", this, src, pos, n, 0, mode != "no", mode == "obj")


def ctor_edges():
    r = random.Random(103)
    for ls in (0, 1, 6):
        for rs in (0, 1, 3):
            for pos, n in ((0, BIG), (0, ls), (1, BIG), (ls + 1, 2), (2, 3), (0, 0), (ls, 5), (1, 2)):
                yield mkcase(r, "construct", rstate(r), rstate(r, ls, rs), pos, n)


def a_random(r):
    return rand_compare(r)


def assign_random(r):
    return rand_assign(r, "assign")


def ctor_random(r):
    return rand_assign(r, "construct")


TESTS = [
    FnTest("std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>::compare", ppc, host, edge=a_edges, rnd=a_random),
    FnTest("std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>::assign", ppc, host, edge=assign_edges,
           rnd=assign_random),
    FnTest("std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>::basic_string", ppc, host, edge=ctor_edges,
           rnd=ctor_random),
]
