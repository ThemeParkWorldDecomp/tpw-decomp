"""Unit v5014, std::basic_istream<char>::read and ::getline, against libtpw_unit_std_basic_istream_read.dylib
(tests/unit_std_basic_istream_read.cpp), main app, src/game/MslSupport.cpp:
  read 0x1020d6b4, getline 0x102079f8

A case is a world and a call. The world (an istream, its ios part, a stream buffer with a vtable and a get area, a destination buffer)
is built twice by ONE piece of Python code: in emulator memory (big-endian) and in the host library's memory (little-endian). The callees
outside the unit (good, tie, flush, setstate, the buffer's virtual calls through __ptr_glue, __throw) are fakes on both sides that record
their arguments and answer from the case's queues. __throw returns, so the code after it runs, as in the original (the try/catch handler
code is unreachable and not translated).

What is compared: the answer (is it this), the call log (the thrown object's type text, vtable, count, message, destructor vector), the
state word, gcount, the get pointer offset and the whole destination buffer. No fused operations. Not tested: a NULL stream buffer while
characters are read (the original dereferences it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_basic_istream_read"
MASK = 0xFFFFFFFF

FN_READ, FN_GETLINE = 0x1020D6B4, 0x102079F8
SIZES = {FN_READ: 1160, FN_GETLINE: 1336}
OWN = set(SIZES)

GOOD, TIE, FLUSH, SETSTATE, PTR_GLUE = 0x102079B4, 0x10207964, 0x1020A604, 0x102077C0, 0x1023BAA8
VTBL, DTORVEC = 0x1029D084, 0x102661D0
M28, M2C = 0x0A000028, 0x0A00002C
HOST_LAYOUT = [0x24, 0x34, 0x38, 4, 8, 12, 8]


def csum(s):
    b = s.encode("latin-1") if isinstance(s, str) else bytes(s)
    return sum((i + 1) * x for i, x in enumerate(b)) & MASK


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_good(e):
    L = e.cl
    L.rec("good", (e.u32(e.r(3) + 0x34),))
    e.set_r(3, L.ret("good") & 1)


def st_tie(e):
    L = e.cl
    L.rec("tie", (e.u32(e.r(3) + 0x34),))
    e.set_r(3, L.ret("tie") & MASK)


def st_flush(e):
    e.cl.rec("flush", (e.r(3) & MASK,))


def st_setstate(e):
    e.cl.rec("setstate", (e.u32(e.r(3) + 0x34), e.r(4) & MASK))


def st_vcall(e):
    L = e.cl
    marker = e.r(12) & MASK
    L.rec("vcall", (marker,))
    e.set_r(3, L.ret("v28" if marker == M28 else "v2c") & MASK)


def st_throw(e):
    L = e.cl
    typ, obj, dtor = e.r(3), e.r(4), e.r(5)
    vt = count = msg = 0
    if obj:
        vt = 1 if e.u32(obj) == VTBL else 0
        count = e.u32(e.u32(obj + 8))
        msg = csum(e.cstr(e.u32(obj + 4)))
    L.rec("throw", (csum(e.cstr(typ)), vt, count, msg, 1 if dtor == DTORVEC else 0))


def st_strlen(e):
    s = e.cstr(e.r(3))
    e.set_r(3, len(s))


def st_new(e):
    e.set_r(3, e.alloc(max(e.r(3), 4)))


def st_strcpy(e):
    dst, src = e.r(3), e.r(4)
    s = e.cstr(src)
    s = s.encode("latin-1") if isinstance(s, str) else bytes(s)
    e.write(dst, s + b"\0")


STUBS.internal(0x10238650, st_strlen)
STUBS.internal(0x10238668, st_new)
STUBS.internal(0x102384B8, st_new)
STUBS.internal(0x10238410, st_strcpy)
STUBS.internal(GOOD, st_good)
STUBS.internal(TIE, st_tie)
STUBS.internal(FLUSH, st_flush)
STUBS.internal(SETSTATE, st_setstate)
STUBS.internal(PTR_GLUE, st_vcall)
STUBS.internal(0x10238680, st_throw)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def ptr(self, a):
        return a

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

    def call(self, op, args):
        entry = FN_READ if op == "read" else FN_GETLINE
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.base = lib.tpw_isb_base()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_isb_alloc(n)

    def ptr(self, a):
        return (a - self.lib.tpw_isb_base()) if a else 0

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        if op == "read":
            return self.lib.tpw_isb_read(args[0], args[1], T.s32(args[2]))
        return self.lib.tpw_isb_getline(args[0], args[1], T.s32(args[2]), T.s32(args[3]))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_isb_alloc.restype, lib.tpw_isb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_isb_base.restype, lib.tpw_isb_base.argtypes = C.c_uint64, []
    lib.tpw_isb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_isb_read.restype, lib.tpw_isb_read.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_isb_getline.restype, lib.tpw_isb_getline.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_isb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    w = {}
    ios, sb, vt, data = s.alloc(0x40), s.alloc(0x20), s.alloc(0x40), s.alloc(16)
    obj, buf = s.alloc(8), s.alloc(16)
    s.words(vt + 0x28, [M28, M2C])
    s.write(data, case["data"])
    P = s.ptr
    s.words(sb, [P(vt), 0, P(data + case["goff"]), P(data + case["eoff"])])
    s.words(ios + 0x24, [0 if case["sb_null"] else P(sb)])
    s.words(ios + 0x34, [case["state"], case["exc"]])
    s.words(obj, [P(ios), case["gcount0"]])
    s.write(buf, b"\xee" * 16)
    w.update(ios=ios, sb=sb, data=data, obj=obj, buf=buf)
    return w


def execute(s, case, w):
    buf = 0 if case["buf_null"] else w["buf"]
    args = [w["obj"], buf, case["n"]]
    if case["op"] == "getline":
        args.append(case["delim"])
    return s.call(case["op"], args)


def observe(s, case, w, answer):
    gptr = s.rd_words(w["sb"] + 8, 1)[0]
    return {
        "answer": answer == w["obj"],
        "log": s.log(),
        "state": s.rd_words(w["ios"] + 0x34, 1)[0],
        "gcount": s.rd_words(w["obj"] + 4, 1)[0],
        "gptr": T.s32(gptr - s.ptr(w["data"])),
        "buf": s.read(w["buf"], 16),
        "data": s.read(w["data"], 16),
    }


def run(s, case):
    try:
        w = build(s, case)
        return observe(s, case, w, execute(s, case, w))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


STATES = [0, 0, 0, 1, 2, 4, 6, 8]
EXCS = [0, 0, 0, 1, 2, 4, 5, 6, 7, 8]


class Case(dict):
    def __repr__(self):
        return "<%s n=%d st=%d exc=%d>" % (self["op"], self["n"], self["state"], self["exc"])


def base(r, op):
    delim = r.choice([10, 0x2C, 0x80, 0xFF, 0])
    data = bytes(r.choice([10, 0x2C, 0x41, 0x42, 0x80, 0xFF, 0, 0x30]) for _ in range(16))
    goff = r.randrange(0, 9)
    eoff = r.choice([goff, r.randrange(0, 9), r.randrange(0, 9), 8])
    vals = [0, 1, 0x41, 0x2C, 10, 0x80, 0x1FF, 0xFFFFFFFF, 0x7F]

    def q():
        k = r.randrange(0, 8)
        return [r.choice(vals) for _ in range(k)] + [r.choice([0xFFFFFFFF, 0x41, 0x2C, 10])]

    c = {
        "op": op, "data": data, "goff": goff, "eoff": eoff, "delim": delim,
        "sb_null": r.random() < 0.08, "buf_null": r.random() < 0.08,
        "state": r.choice(STATES), "exc": r.choice(EXCS), "gcount0": r.choice([0, 5, 77]),
        "n": r.choice([1, 1, 2, 3, 5, 9, 12, 0, -1, r.randrange(1, 13)]),
        "queues": {
            "good": r.choice([[1, 1], [1, 0], [0, 0], [1, 1], [1, 1]]),
            "tie": [r.choice([0, 0x1234])],
            "v28": q(), "v2c": q(),
        },
    }
    c = Case(c)
    if c["delim"] in (0x80, 0xFF) and r.random() < 0.5:
        c["data"] = bytes([c["delim"]]) * 16
    if (c["n"] > 0 and not c["buf_null"]) and c["sb_null"]:
        c["sb_null"] = False
    return c


def mk(op):
    def gen(r):
        return base(r, op)

    def edges():
        r = random.Random(17 if op == "read" else 19)
        for _ in range(6):
            yield base(r, op)
        for good in ([1, 1], [1, 0], [0, 0]):
            for n in (-1, 0, 1, 2, 5):
                for tie in (0, 0x1234):
                    for exc in (0, 4, 7):
                        c = base(r, op)
                        c["queues"]["good"], c["queues"]["tie"] = good, [tie]
                        c["n"], c["exc"], c["state"], c["buf_null"] = n, exc, 0, False
                        if n > 0:
                            c["sb_null"] = False
                        yield c
        for eoff in (0, 3, 8):
            for n in (1, 2, 4, 12):
                c = base(r, op)
                c["queues"]["good"] = [1, 1]
                c["sb_null"] = c["buf_null"] = False
                c["goff"], c["eoff"], c["n"], c["exc"] = 0, eoff, n, r.choice([0, 2, 4, 6])
                c["delim"] = c["data"][1]
                yield c

    return edges, gen


R_E, R_G = mk("read")
G_E, G_G = mk("getline")

TESTS = [
    FnTest("std::basic_istream<char,std::char_traits<char>>::read", ppc, host, edge=R_E, rnd=R_G),
    FnTest("std::basic_istream<char,std::char_traits<char>>::getline", ppc, host, edge=G_E, rnd=G_G),
]
