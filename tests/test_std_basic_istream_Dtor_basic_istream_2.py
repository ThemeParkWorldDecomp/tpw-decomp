"""Unit v5708, std::basic_istream<wchar_t>: ~basic_istream, getline and the constructor, against
libtpw_unit_std_basic_istream_Dtor_basic_istream_2.dylib (tests/unit_std_basic_istream_Dtor_basic_istream_2.cpp), main app
("SimTheme Park"), src/COnlineControl_..._SaveIgnore_std_basic_istream_Dtor_basic_istream_2.cpp:
  std::basic_istream<wchar_t>::~basic_istream 0x1021d4a0, getline 0x102202b0, basic_istream 0x102266e4

A case is a world and a call, built twice by one piece of Python code (emulator memory big-endian at the Mac's layout for the
original, the host library's arena little-endian with native pointers for the translation). Callees outside the unit are fakes on
both sides that record their arguments: ios_base::good/setstate/~ios_base/init, basic_ios::tie/widen, ostream::flush, the stream
buffer's virtual peek/bump (__ptr_glue), strlen, operator new/new[]/delete, strcpy and __throw (the object it is given is read
back: vtable identity, message text, count, destructor vector).

The world: a stream object with its ios part (or a separate ios block), a stream buffer with a get window of 16 bit characters and a
vtable block whose entries 0x28 and 0x2c are tokens, an output buffer. Compared: the answer, the object, both ios blocks (vtable
words as names of the globals, base offset, state, mask, the words the constructor writes), the output buffer, the get pointer and
the event log.

No fused operations. Not tested: a NULL stream buffer with a valid, good getline (the original dereferences it), a NULL this in
getline, this == 0xfffffff4 in the destructor, the compiler's exception handler regions (unreachable without a real exception),
a NULL result of operator new (the runtime never returns it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_istream_Dtor_basic_istream_2"
MASK = 0xFFFFFFFF

FN_DTOR, FN_GETLINE, FN_CTOR = 0x1021D4A0, 0x102202B0, 0x102266E4
SIZES = {FN_DTOR: 152, FN_GETLINE: 1336, FN_CTOR: 200}
OWN = set(SIZES)

GOOD, TIE, FLUSH, SETSTATE, WIDEN = 0x102079B4, 0x1021F888, 0x102268F8, 0x102077C0, 0x10226828
PTRGLUE, IOSINIT, IOSDTOR, DELETE = 0x1023BAA8, 0x10238968, 0x10238950, 0x10238218
STRLEN, NEWARR, NEW, STRCPY, THROW = 0x10238650, 0x10238668, 0x102384B8, 0x10238410, 0x10238680

ADDR = dict(A=0x102B1E04, E=0x102B1F20, FV=0x1029D084, FD=0x102661D0)
HOST_LAYOUT = [8, 0x10, 0x18, 0x28, 0x38, 0x3C, 0x40, 0x48]
PEEK, BUMP = 0xA028, 0xA02C
FAIL_TYPE = b"!std::exception!!std::ios_base::failure!!"
FAIL_TEXT = b"ios_base failure in clear"

PPC_OFF = dict(gc=4, vt=8, part=0xC, sbuf=0x24, state=0x34, exc=0x38, f3c=0x3C, fill=0x40, base=0x44)
HOST_OFF = dict(gc=8, vt=0x10, part=0x18, sbuf=0x28, state=0x38, exc=0x3C, f3c=0x40, fill=0x44, base=0x48)


def tcode(b):
    h = 0
    for x in b:
        h = (h * 31 + x) & MASK
    return h


def as_bytes(t):
    return t.encode("latin-1") if isinstance(t, str) else bytes(t)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def glue(fn):
    """An import's glue saves the TOC pointer at 0x14(r1); the translated code reloads it after the call."""
    def wrapper(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapper


def s_good(e):
    L = e.cl
    L.rec("Good", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Good") & 0xFF)


def s_tie(e):
    L = e.cl
    L.rec("Tie", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("Tie") & MASK)


def s_flush(e):
    e.cl.rec("Flush", (e.r(3) & MASK,))


def s_setstate(e):
    L = e.cl
    L.rec("Setstate", (L.lab(e.r(3)), e.r(4) & MASK))


def s_widen(e):
    L = e.cl
    L.rec("Widen", (L.lab(e.r(3)), e.r(4) & 0xFF))
    e.set_r(3, L.ret("Widen") & 0xFFFF)


def s_ptrglue(e):
    L = e.cl
    entry = e.r(12) & MASK
    L.rec("PtrGlue", (L.lab(e.r(3)), entry))
    e.set_r(3, L.ret("Peek" if entry == PEEK else "Bump") & MASK)


def s_iosinit(e):
    L = e.cl
    L.rec("IosInit", (L.lab(e.r(3)), e.r(4) & MASK))


def s_iosdtor(e):
    L = e.cl
    L.rec("IosDtor", (L.lab(e.r(3)), e.r(4) & MASK))


def s_delete(e):
    L = e.cl
    L.rec("Delete", (L.lab(e.r(3)),))


def s_strlen(e):
    t = as_bytes(e.cstr(e.r(3)))
    e.cl.rec("Strlen", (tcode(t),))
    e.set_r(3, len(t))


def s_newarr(e):
    n = e.r(3) & MASK
    e.cl.rec("NewArray", (n,))
    e.set_r(3, e.alloc(max(n, 4)))


def s_new(e):
    e.cl.rec("New", (e.r(3) & MASK,))
    e.set_r(3, e.alloc(4))


def s_strcpy(e):
    dst, src = e.r(3) & MASK, as_bytes(e.cstr(e.r(4)))
    e.cl.rec("Strcpy", (tcode(src), 1 if dst else 0))
    if dst:
        e.write(dst, src + b"\0")


def s_throw(e):
    typ = as_bytes(e.cstr(e.r(3)))
    obj = e.r(4) & MASK
    vtbl, msg, cnt = e.u32(obj), e.u32(obj + 4), e.u32(obj + 8)
    e.cl.rec("Throw", (tcode(typ), 1 if vtbl == ADDR["FV"] else 0, tcode(as_bytes(e.cstr(msg))), e.u32(cnt),
                       1 if (e.r(5) & MASK) == ADDR["FD"] else 0))


for _a, _f in ((GOOD, s_good), (TIE, s_tie), (FLUSH, s_flush), (SETSTATE, s_setstate), (WIDEN, s_widen)):
    STUBS.internal(_a, _f)
for _a, _f in ((PTRGLUE, s_ptrglue), (IOSINIT, s_iosinit), (IOSDTOR, s_iosdtor), (DELETE, s_delete), (STRLEN, s_strlen),
               (NEWARR, s_newarr), (NEW, s_new), (STRCPY, s_strcpy), (THROW, s_throw)):
    STUBS.internal(_a, glue(_f))


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, hs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", h & 0xFFFF) for h in hs))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def blocks(self):
        """The case's blocks, allocated before the log knows their addresses."""
        self.o = self.alloc(0x200)
        self.other = self.alloc(0x100)
        self.sb = self.alloc(0x20)
        self.vt = self.alloc(0x40)
        self.text = self.alloc(0x100)
        self.out = self.alloc(0x100)
        self.part = self.o + self.P["part"]

    def sym(self, v):
        for name in ADDR:
            g = self.g(name)
            if g <= v < g + 0x20:
                return (name, v - g)
        return ("raw", v)

    def plabel(self, v):
        for name, a in (("part", self.part), ("other", self.other), ("o", self.o)):
            if v == a:
                return name
        return ("raw", v)


class Ppc(Side):
    fmt = ">I"
    P = PPC_OFF

    def __init__(self, e, case):
        self.e = e
        self.blocks()
        self.L = e.cl = CL.PpcLog({self.o: 1, self.part: 2, self.other: 3, self.sb: 4}, case["queues"])
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

    def wp(self, a, v):
        self.words(a, [v])

    def rp(self, a):
        return self.rd_word(a)

    def ptrv(self, a):
        return a

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    P = HOST_OFF

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks()
        CL.host_begin(lib, {1: self.o, 2: self.part, 3: self.other, 4: self.sb}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_wi_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_wi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def wp(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rp(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def ptrv(self, a):
        return self.lib.tpw_wi_off(a)

    def call(self, fn, args):
        if fn == FN_DTOR:
            return self.lib.tpw_wi_dtor(args[0], T.s16(args[1]))
        if fn == FN_CTOR:
            return self.lib.tpw_wi_ctor(args[0], T.s16(args[1]), args[2] & MASK)
        return self.lib.tpw_wi_getline(args[0], args[1], T.s32(args[2]), args[3] & 0xFFFF)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_wi_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "off").restype, getattr(lib, p + "off").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wi_dtor.restype, lib.tpw_wi_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    lib.tpw_wi_ctor.restype, lib.tpw_wi_ctor.argtypes = C.c_uint64, [C.c_uint64, C.c_int16, C.c_uint32]
    lib.tpw_wi_getline.restype = C.c_uint64
    lib.tpw_wi_getline.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_uint16]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    P = s.P
    o, part, other = s.o, s.part, s.other
    ios = {"part": part, "other": other}[case["ios"]]
    s.wp(o, ios)
    s.words(o + P["gc"], [0x13572468])
    s.wp(o + P["vt"], 0x2222)
    for blk in (part, other):
        s.wp(blk, 0x3333)
        s.words(blk + P["f3c"], [0x44444444])
        s.halves(blk + P["fill"], [0x5555])
        s.words(blk + P["base"], [0x66666666 + part - blk])
        s.words(blk + P["state"], [0x1234])
        s.words(blk + P["exc"], [0x2345])
        s.words(blk + P["sbuf"], [0x3456 + s.ptrv(s.sb)])
    if case["op"] == "getline":
        s.words(ios + P["state"], [case["state"]])
        s.words(ios + P["exc"], [case["exc"]])
        s.words(ios + P["sbuf"], [s.ptrv(s.sb) if case["hassb"] else 0])
        n = len(case["chars"])
        s.words(s.sb, [s.ptrv(s.vt), 0x7777, s.ptrv(s.text), s.ptrv(s.text) + 2 * n])
        s.words(s.vt + 0x28, [PEEK, BUMP])
        s.halves(s.text, case["chars"])
        s.halves(s.out, [0x5555] * 0x80)


def execute(s, case):
    o = s.o
    if case["op"] == "dtor":
        return s.call(FN_DTOR, [0 if case["null"] else o, case["flag"] & MASK])
    if case["op"] == "ctor":
        return s.call(FN_CTOR, [o, case["flag"] & MASK, case["streambuf"]])
    buf = 0 if case["nullbuf"] else s.out
    return s.call(FN_GETLINE, [o, buf, case["count"] & MASK, case["delim"] | (case["dhi"] << 16)])


def observe(s, case, ans):
    P = s.P
    o, part, other = s.o, s.part, s.other
    d = {"ans": "o" if ans == o else ans, "log": s.log(),
         "mios": s.plabel(s.rp(o)), "gc": s.rd_word(o + P["gc"]), "vt": s.sym(s.rp(o + P["vt"]))}
    for name, blk in (("part", part), ("other", other)):
        d[name + "_vt"] = s.sym(s.rp(blk))
        d[name + "_f3c"] = s.rd_word(blk + P["f3c"])
        d[name + "_fill"] = s.rd_halves(blk + P["fill"], 1)
        d[name + "_base"] = (s.rd_word(blk + P["base"]) - (part - blk)) & MASK
        d[name + "_state"] = s.rd_word(blk + P["state"])
        d[name + "_exc"] = s.rd_word(blk + P["exc"])
        v = s.rd_word(blk + P["sbuf"])
        d[name + "_sbuf"] = "null" if v == 0 else (v - s.ptrv(s.sb)) & MASK
    if case["op"] == "getline":
        d["out"] = s.rd_halves(s.out, 0x80)
        d["sb"] = [(s.rd_word(s.sb) - s.ptrv(s.vt)) & MASK, s.rd_word(s.sb + 4)]
        d["gptr"] = s.rd_word(s.sb + 8) - s.ptrv(s.text)
        d["egptr"] = s.rd_word(s.sb + 12) - s.ptrv(s.text)
    return d


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

FLAGS = [0, 1, -1, 2, 0x7FFF, -0x8000, 0x10000, 0x10001, 0x18000, 0xFFFF0000 & MASK]


def dtor_case(r, **kw):
    c = {"op": "dtor", "queues": {}, "ios": r.choice(["part", "other"]), "flag": r.choice(FLAGS), "null": r.random() < 0.1}
    c.update(kw)
    return c


def dtor_edges():
    r = random.Random(5081)
    for ios in ("part", "other"):
        for flag in FLAGS:
            yield dtor_case(r, ios=ios, flag=flag, null=False)
    yield dtor_case(r, null=True, flag=1)
    yield dtor_case(r, null=True, flag=0)


def dtor_random(r):
    return dtor_case(r)


def ctor_case(r, **kw):
    c = {"op": "ctor", "queues": {"Widen": [r.choice([0x20, 0, 0xFFFF, r.randrange(0x10000)])]}, "ios": r.choice(["part", "other"]),
         "flag": r.choice(FLAGS), "streambuf": r.choice([0, 0x1234, r.randrange(1 << 32)])}
    c.update(kw)
    return c


def ctor_edges():
    r = random.Random(5082)
    for ios in ("part", "other"):
        for flag in FLAGS:
            yield ctor_case(r, ios=ios, flag=flag)


def ctor_random(r):
    return ctor_case(r)


TIE_TOKEN = 0x7001


def fix(c):
    """The original dereferences a NULL stream buffer in the loop: keep one whenever the loop runs."""
    valid = not c["nullbuf"] and c["count"] > 0
    ok = c["good"][:2] == [1, 1] or (len(c["good"]) > 1 and c["good"][0] == 1 and c["good"][1] == 1)
    if valid and ok:
        c["hassb"] = True
    return c


def gl_case(r, **kw):
    delim = r.choice([0x41, 0x0A, 0x20, 0xFFFF, 0x0, 0x141, 0x1000A])
    delim &= 0xFFFF
    alpha = [0x41, 0x42, 0x0A, 0x20, 0x141, 0x100, 0x0, 0xFFFF, delim]
    n = r.choice([0, 0, 1, 2, 3, 5, 8])
    chars = [r.choice(alpha[:8] + [delim] * 3) for _ in range(n)]
    if r.random() < 0.6:
        chars = [x for x in chars if x != 0xFFFF and x != delim]
    peek = [(r.choice([0, 0, 0xABCD]) << 16) | r.choice(alpha + [delim]) for _ in range(r.randrange(0, 6))] + [0xFFFF]
    good = r.choice([[1, 1], [1, 1], [1, 1], [1, 0], [0]])
    tie = r.choice([[0], [TIE_TOKEN], [TIE_TOKEN]])
    c = {"op": "getline", "ios": "part", "good": good, "nullbuf": r.random() < 0.1,
         "count": r.choice([0, -1, 1, 2, 3, 4, 6, 10, 0x7FFFFFFF, 100, -0x80000000]),
         "delim": delim, "dhi": r.choice([0, 0, 0xABCD]), "state": r.choice([0, 0, 1, 2, 4, 6]),
         "exc": r.choice([0, 0, 0, 1, 2, 4, 5, 7]), "hassb": r.random() < 0.9, "chars": chars}
    c["queues"] = {"Good": good, "Tie": tie, "Peek": peek, "Bump": [r.randrange(1 << 32)], "Widen": [0x20]}
    c.update(kw)
    return fix(c)


def gl_edges():
    r = random.Random(5083)
    for good in ([0], [1, 0], [1, 1]):
        for tie in (0, TIE_TOKEN):
            for mode in ("valid", "nullbuf", "count0", "countneg"):
                for exc in (0, 4, 5):
                    for hassb in (True, False):
                        kw = {"good": good, "exc": exc, "hassb": hassb, "nullbuf": mode == "nullbuf",
                              "count": {"valid": 4, "nullbuf": 4, "count0": 0, "countneg": -1}[mode]}
                        c = gl_case(r, **kw)
                        c["queues"]["Good"] = good
                        c["queues"]["Tie"] = [tie]
                        yield fix(c)
    # the loop: delimiter in the window or from the virtual peek, count running out, EOF, one-character counts
    for count in (1, 2, 3, 5, 100):
        for chars in ([], [0x41], [0x41, 0x42], [0x41, 0x0A, 0x42], [0xFFFF, 0x41], [0x0A], [0x41, 0x42, 0x43, 0x44]):
            for peek in ([0xFFFF], [0x41, 0x0A], [0x42, 0x43, 0xFFFF], [0xABCD000A], [0x1000A, 0xFFFF]):
                for exc, state in ((0, 0), (2, 0), (4, 0), (1, 1), (6, 2)):
                    c = gl_case(r, count=count, chars=chars, delim=0x0A, dhi=0xABCD, good=[1, 1], nullbuf=False, hassb=True,
                                exc=exc, state=state)
                    c["queues"]["Good"] = [1, 1]
                    c["queues"]["Peek"] = peek
                    yield c
    # a stream buffer pointer of NULL that is reported through the state bits only
    for state in (0, 1):
        yield gl_case(r, good=[1, 1], nullbuf=True, hassb=False, exc=1, state=state, count=3)


def gl_random(r):
    return gl_case(r)


TESTS = [
    FnTest("std::basic_istream<wchar_t,std::char_traits<wchar_t>>::~basic_istream", ppc, host, edge=dtor_edges, rnd=dtor_random),
    FnTest("std::basic_istream<wchar_t,std::char_traits<wchar_t>>::getline", ppc, host, edge=gl_edges, rnd=gl_random),
    FnTest("std::basic_istream<wchar_t,std::char_traits<wchar_t>>::basic_istream", ppc, host, edge=ctor_edges, rnd=ctor_random),
]
