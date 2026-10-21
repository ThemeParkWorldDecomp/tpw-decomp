"""Unit v5640, operator<<(wostream&, const wchar_t*), against libtpw_unit_std_operator.dylib (tests/unit_std_operator.cpp), main app
("SimTheme Park"), src/game/MslSupport.cpp:
  std::operator<<<wchar_t,std::char_traits<wchar_t>> 0x1021f374

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees (good, tie, flush, setstate,
__putnumber, __throw) are fakes on both sides that record their arguments and answer from the case's queues; wcslen, strlen, strcpy and
the allocators run for real.

The world of a case: a stream (pointer to its ios), the ios (stream buffer, flags, state, exceptions, fill), the text. What is compared:
the answer (the stream), the flags/state/exceptions words and the event log (including the thrown object's type, message and counter).
No fused operations. Not tested: the catch landing pads and the inlined setstate block (unreachable code).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std_operator"
MASK = 0xFFFFFFFF

FN = 0x1021F374
OWN = {FN}

VTBL, DTORS = 0x1029D084, 0x102661D0
LABELS = {"stream": 1, "ios": 2, "sb": 3, "vtbl": 4, "dtors": 5}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def hash_units(units):
    h = 0
    for c in units:
        h = (h * 31 + c) & 0x7FFFFFFF
    return h


def cstr_units(s):
    return [b if isinstance(b, int) else ord(b) for b in s]


def stub_good(e):
    L = e.cl
    L.rec("Good", (L.lab(e.r(3)),))
    e.set_r(3, 1 if L.ret("good") else 0)


def stub_tie(e):
    L = e.cl
    L.rec("Tie", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("tie") & MASK)


def stub_flush(e):
    L = e.cl
    L.rec("Flush", (L.lab(e.r(3)),))


def stub_setstate(e):
    L = e.cl
    L.rec("Setstate", (L.lab(e.r(3)), s32(e.r(4))))


def stub_put(e):
    L = e.cl
    n = s32(e.r(10))
    units = struct.unpack(">%dH" % n, e.read(e.r(9), 2 * n)) if n > 0 else ()
    L.rec("PutNumber", (L.lab(e.r(4)), L.lab(e.r(5)), e.r(6) & 0xFFFF, 1 if e.r(7) else 0, s32(e.r(8)), hash_units(units), n))
    e.write(e.r(3), struct.pack(">I", 1 if L.ret("put") else 0))


def stub_throw(e):
    L = e.cl
    obj = e.r(4)
    vt, msg, cnt = e.u32(obj), e.u32(obj + 4), e.u32(obj + 8)
    L.rec("Throw", (hash_units(cstr_units(e.cstr(e.r(3)))), L.lab(vt), hash_units(cstr_units(e.cstr(msg))),
                    s32(e.u32(cnt)) if cnt else -1, L.lab(e.r(5))))


def stub_wcslen(e):
    a = e.r(3)
    n = 0
    while e.read(a + 2 * n, 2) != b"\0\0":
        n += 1
    e.set_r(3, n)


STUBS.internal(0x102079B4, stub_good)
STUBS.internal(0x1021F888, stub_tie)
STUBS.internal(0x102268F8, stub_flush)
STUBS.internal(0x102077C0, stub_setstate)
STUBS.internal(0x10226358, stub_put)
STUBS.internal(0x10238680, stub_throw)
STUBS.internal(0x102389C8, stub_wcslen)
STUBS.internal(0x10238698, lambda e: None)


def stub_strlen(e):
    a = e.r(3)
    n = 0
    while e.read(a + n, 1) != b"\0":
        n += 1
    e.set_r(3, n)


def stub_strcpy(e):
    a = e.r(4)
    n = 0
    while e.read(a + n, 1) != b"\0":
        n += 1
    e.write(e.r(3), e.read(a, n + 1))


STUBS.internal(0x10238650, stub_strlen)
STUBS.internal(0x10238410, stub_strcpy)
STUBS.internal(0x10238668, lambda e: e.set_r(3, e.alloc(max(4, e.r(3)))))
STUBS.internal(0x102384B8, lambda e: e.set_r(3, e.alloc(max(4, e.r(3)))))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def io(self, mac):
        """The ios field's offset: the host's vtable pointer is 8 bytes."""
        return self.ios + mac + (self.shift if mac >= 4 else 0)


class Ppc(Side):
    fmt = ">I"
    shift = 0

    def __init__(self, e, case):
        self.e = e
        self.stream, self.ios, self.sb = e.alloc(16), e.alloc(0x60), 0x5000
        self.L = e.cl = CL.PpcLog({self.stream: 1, self.ios: 2, self.sb: 3, VTBL: 4, DTORS: 5}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def wptr(self, a, v):
        self.words(a, [v])

    def log(self):
        return self.L.log

    def call(self, text):
        r3, _, _ = T.call_ppc(self.e, FN, [self.stream, text])
        return (r3 & MASK) == self.stream


class Host(Side):
    fmt = "<I"
    shift = 4

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.stream, self.ios, self.sb = lib.tpw_wo_alloc(16), lib.tpw_wo_alloc(0x68), 0x5000
        CL.host_begin(lib, {1: self.stream, 2: self.ios, 3: self.sb, 4: lib.tpw_wo_addr(b"vtbl"), 5: lib.tpw_wo_addr(b"dtors")},
                      case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_wo_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, text):
        return self.lib.tpw_wo_write(self.stream, text) == self.stream


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_wo_addr.restype, lib.tpw_wo_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_wo_alloc.restype, lib.tpw_wo_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_wo_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wo_write.restype, lib.tpw_wo_write.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_wo_layout(out)
    assert list(out)[:n] == [0x28, 0x38, 0x3C, 0x40, 0x2C], list(out)[:n]
    lib.bound = True


def build(s, case):
    s.wptr(s.stream, s.ios)
    s.words(s.io(0x24), [0 if case["nosb"] else s.sb])
    s.words(s.io(0x30), [case["flags"], case["state"], case["exc"]])
    s.write(s.io(0x40), struct.pack(s.fmt[0] + "H", case["fill"]))
    units = case["text"] + [0]
    s.text = s.alloc(2 * len(units))
    s.write(s.text, b"".join(struct.pack(s.fmt[0] + "H", c) for c in units))


def run(s, case):
    try:
        build(s, case)
        answer = s.call(s.text)
        return {"answer": answer, "log": s.log(), "ios": s.rd_words(s.io(0x30), 3)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, g1=None, g2=None, tie=None, put=None, nosb=None, flags=None):
    c = {
        "nosb": r.choice([0, 0, 1]) if nosb is None else nosb,
        "flags": r.choice([0, 0x2000, 0x2000 | 0x400, 0x400]) if flags is None else flags,
        "state": r.choice([0, 0, 0, 2, 1, 4]),
        "exc": r.choice([0, 1, 2, 4, 5, 7]),
        "fill": r.choice([0, 0x20, 0x2E, r.randrange(0x10000)]),
        "text": [r.randrange(1, 0x10000) for _ in range(r.choice([0, 1, 2, 5]))],
    }
    g1 = r.choice([0, 1, 1, 1]) if g1 is None else g1
    g2 = r.choice([0, 1, 1]) if g2 is None else g2
    t = r.choice([0, 0x1000]) if tie is None else tie
    c["queues"] = {"good": [g1, g2], "tie": [t, t], "put": [r.choice([0, 1]) if put is None else put]}
    return c


def edges():
    r = random.Random(640)
    for g1 in (0, 1):
        for g2 in (0, 1):
            for tie in (0, 0x1000):
                for put in (0, 1):
                    for nosb in (0, 1):
                        for flags in (0, 0x2000):
                            yield base(r, g1, g2, tie, put, nosb, flags)


def rnd(r):
    return base(r)


TESTS = [
    FnTest("std::operator<<<wchar_t,std::char_traits<wchar_t>>", ppc, host, edge=edges, rnd=rnd),
]
