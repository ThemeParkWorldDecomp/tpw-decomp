"""Unit v5700, std::operator<<(basic_ostream<char>&, const char*), against libtpw_unit_std_operator_2.dylib
(tests/unit_std_operator_2.cpp), main app ("SimTheme Park"), src/email_load_std_operator_2.cpp:
  std::operator<<<std::char_traits<char>> 0x10207150

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (ios_base::good, basic_ios::tie, basic_ostream::flush, ios_base::setstate, __putnumber, __throw, strlen, strcpy,
operator new, operator new[]) are fakes on both sides that record their arguments (pointers as codes, texts as a hash and length) and
answer from the case's queues.

The world of a case: a stream (pointer to its ios part), the ios part (streambuf pointer 0x24, width 0x2c, flags 0x30, state 0x34, mask
0x38, fill 0x40), a streambuf block and the text. What is compared: the answer (the stream comes back), the ios part, the event log
(including everything the thrown ios_base::failure object holds).

No fused operations. Not tested: the exception-cleanup blocks of the listing (reached only by unwinding), and a NULL from operator new
(the runtime never returns one).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_operator_2"
MASK = 0xFFFFFFFF

FN = 0x10207150
SIZES = {FN: 976}
OWN = set(SIZES)

CALLEE = dict(good=0x102079b4, tie=0x10207964, flush=0x1020a604, setstate=0x102077c0, putnumber=0x1020a140, throw=0x10238680,
              strlen=0x10238650, strcpy=0x10238410, new=0x102384b8, newa=0x10238668)

ADDR = dict(vtbl=0x1029d084, vec=0x102661d0)
HOST_LAYOUT = [0x24, 0x2C, 0x30, 0x34, 0x38, 0x40]

CUR = {}


def s32(v):
    return T.s32(v)


def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & MASK
    return h


def code(p):
    if p == 0:
        return 0
    if p == CUR["ios"]:
        return 1
    if p == CUR["os"]:
        return 2
    if p == CUR["sb"]:
        return 3
    return -1


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_good(e):
    L = e.cl
    L.rec("good", (code(e.r(3)),))
    e.set_r(3, L.ret("good") & 0xFF)


def st_tie(e):
    L = e.cl
    L.rec("tie", (code(e.r(3)),))
    e.set_r(3, L.ret("tie") & MASK)


def st_flush(e):
    L = e.cl
    p = e.r(3)
    L.rec("flush", (code(p), p if p < 0x10000 else 0))


def st_setstate(e):
    e.cl.rec("setstate", (code(e.r(3)), s32(e.r(4))))


def st_putnumber(e):
    L = e.cl
    suffix = e.r(9)
    ios = e.r(5)
    L.rec("putnumber", (code(e.r(4)), code(ios), e.r(6) & 0xFF, code(e.r(7)), s32(e.r(8)), fnv(raw(e, suffix)),
                        s32(e.r(10)), s32(e.u32(ios + 0x2C))))
    e.write(e.r(3), struct.pack(">I", e.r(4) if L.ret("put") else 0))


def raw(e, a):
    s = e.cstr(a)
    return s.encode("latin-1") if isinstance(s, str) else s


def st_throw(e):
    L = e.cl
    obj = e.r(4)
    vtbl, msg, cnt = struct.unpack(">III", e.read(obj, 12))
    L.rec("throw", (fnv(raw(e, e.r(3))), 1 if vtbl == ADDR["vtbl"] else 0, fnv(raw(e, msg)) if msg else -1, 1 if cnt else 0,
                    e.u32(cnt) if cnt else -7, 1 if e.r(5) == ADDR["vec"] else 0))


def st_strlen(e):
    L = e.cl
    s = raw(e, e.r(3))
    L.rec("strlen", (fnv(s), len(s)))
    e.set_r(3, len(s))


def st_strcpy(e):
    L = e.cl
    s = raw(e, e.r(4))
    L.rec("strcpy", (fnv(s),))
    e.write(e.r(3), s + b"\0")


def st_new(e):
    e.cl.rec("new", (e.r(3),))
    e.set_r(3, e.alloc(e.r(3)))


def st_newa(e):
    e.cl.rec("new[]", (e.r(3),))
    e.set_r(3, e.alloc(e.r(3)))


for _n, _f in (("good", st_good), ("tie", st_tie), ("flush", st_flush), ("setstate", st_setstate), ("putnumber", st_putnumber),
               ("throw", st_throw), ("strlen", st_strlen), ("strcpy", st_strcpy), ("new", st_new), ("newa", st_newa)):
    STUBS.internal(CALLEE[_n], _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


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

    def expect(self, ios, os, sb):
        CUR.update(ios=ios, os=os, sb=sb)

    def off(self, p):
        return p

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, os, text):
        r3, _, _ = T.call_ppc(self.e, FN, [os, text])
        return 1 if (r3 & MASK) == os else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def expect(self, ios, os, sb):
        CUR.update(ios=ios, os=os, sb=sb)
        self.lib.tpw_ow_expect(ios, os, sb)

    def off(self, p):
        return p - self.lib.tpw_ow_base()

    def alloc(self, n):
        return self.lib.tpw_ow_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, os, text):
        return self.lib.tpw_ow_write(os, text) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ow_addr.restype, lib.tpw_ow_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ow_alloc.restype, lib.tpw_ow_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ow_base.restype, lib.tpw_ow_base.argtypes = C.c_uint64, []
    lib.tpw_ow_expect.restype, lib.tpw_ow_expect.argtypes = None, [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_ow_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ow_write.restype, lib.tpw_ow_write.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ow_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    ios = s.alloc(0x50)
    sb = s.alloc(0x20)
    os = s.alloc(8)
    text = s.alloc(len(case["text"]) + 1)
    s.write(text, case["text"] + b"\0")
    s.words(os, [s.off(ios)])
    s.words(ios + 0x24, [s.off(sb) if case["sb"] else 0, 0, case["width"], case["flags"], case["state"], case["mask"]])
    s.write(ios + 0x40, bytes([case["fill"]]))
    s.expect(ios, os, sb)
    return ios, os, sb, text


def run(s, case):
    try:
        ios, os, sb, text = build(s, case)
        answer = s.call(os, text)
        if isinstance(s, Host):
            answer = 1 if answer else 0
        words = s.rd_words(ios, 16)
        if words[9] == s.off(sb):
            words[9] = "sb"
        return {"answer": answer, "log": s.log(), "ios": words, "fill": s.read(ios + 0x40, 1), "sb": s.read(sb, 0x20),
                "os": s.rd_words(os, 1)[0] == s.off(ios)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

TEXTS = [b"", b"a", b"hello world", b"nobody@nowhere.net", b"\x80\xff\x01x" , b"x" * 100]


def mk(good, tie, put, state, mask, flags, sb, text=b"abc", width=0, fill=0x20):
    return {"queues": {"good": list(good), "tie": [tie], "put": [put]}, "state": state, "mask": mask, "flags": flags, "sb": sb,
            "text": text, "width": width, "fill": fill}


def edges():
    for good in ([0], [1, 0], [1, 1]):
        for tie in (0, 0x1000):
            for put in (0, 1):
                for state in (0, 1, 2, 4):
                    for mask in (0, 1, 4, 5):
                        for flags in (0, 0x2000):
                            for sb in (0, 1):
                                yield mk(good, tie, put, state, mask, flags, sb)


def rnd(r):
    good = r.choice([[0], [1, 0], [1, 1], [1, 1]])
    return mk(good, r.choice([0, 0x1000]), r.choice([0, 1]), r.choice([0, 1, 2, 4, 5, 8, r.randrange(16)]),
              r.choice([0, 1, 4, 5, r.randrange(16)]), r.choice([0, 0x2000, r.randrange(1 << 16)]), r.choice([0, 1]),
              text=r.choice(TEXTS), width=r.choice([0, 5, -1, r.randrange(-5, 200)]), fill=r.randrange(256))


TESTS = [
    FnTest("std::operator<<<std::char_traits<char>>", ppc, host, edge=edges, rnd=rnd),
]
