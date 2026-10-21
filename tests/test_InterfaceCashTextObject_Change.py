"""Unit v5983, the cash, password, null cash, percent and visitor number text objects' Change and the password object's destructor, against
libtpw_unit_InterfaceCashTextObject_Change.dylib (tests/unit_InterfaceCashTextObject_Change.cpp), main app ("SimTheme Park"),
src/game/ui/InterfaceTextObjectSprite.cpp:
  InterfaceCashTextObject::Change 0x1018e02c, InterfacePasswordTextObject::Change 0x1018dd50,
  InterfacePasswordTextObject::~InterfacePasswordTextObject 0x1018dcb0, InterfaceNullCashTextObject::Change 0x1018e768,
  InterfacePercentTextObject::Change 0x1018ea44, InterfaceVisitorNumTextObject::Change 0x1018ed10

A case is a world and a call. The world is built twice by ONE piece of Python code (Mac memory big-endian, host little-endian). The callees
outside the unit are fakes on both sides: the sprite's Change (logs the flags, for the password also the change's words and rectangle, and
the length and a hash of the text when flag 0x100), TbITable::FormatString (logs table, size, id and the tag; writes "id:value"),
labs, _wcsset (really sets), swprintf and wcscpy (PowerPC stubs mirroring the host's msl; the visitor format is checked to be "%d"), the
cursor sprite's destructor and operator delete.

The world: the table pointer in gText +0x564, a change block with its style and text, the shared 0x300-character buffer filled with 'A'.
Compared: the event log, the buffer, the destructor's answer (self or NULL). No fused operations. Not tested: the vtable the destructor stores at
+0x38 (the host class has no such slot); the stack garbage of the DOChange the cash objects build (only its text is read).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceCashTextObject_Change"
MASK = 0xFFFFFFFF

FN = {"cash": 0x1018e02c, "pw": 0x1018dd50, "null": 0x1018e768, "pct": 0x1018ea44, "vis": 0x1018ed10}
FNNUM = {"cash": 0, "pw": 1, "null": 2, "pct": 3, "vis": 4}
DTOR = 0x1018dcb0
SIZES = {0x1018e02c: 288, 0x1018dd50: 200, 0x1018dcb0: 104, 0x1018e768: 196, 0x1018ea44: 176, 0x1018ed10: 156}
OWN = set(SIZES)

ADDR = dict(table=0x1038047c, buffer=0x10380c48)
HOST_LAYOUT = [0x10]
BUFCHARS = 0x300
CUR = {"op": "", "table": 0}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def units(e, a, limit=0x400):
    out = []
    while len(out) < limit:
        u = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if u == 0:
            break
        out.append(u)
    return out


def put_units(e, a, us):
    e.write(a, b"".join(struct.pack(">H", u) for u in us) + b"\0\0")


def fnv(us):
    h = 2166136261
    for u in us:
        h = ((h ^ u) * 16777619) & MASK
    return h


def stub_change(e):
    L = e.cl
    self_, flags, ch = e.r(3), e.r(4), e.r(5)
    length, h = -1, 0
    if flags & 0x100:
        style = e.u32(ch + 0x10)
        us = units(e, e.u32(style))
        length, h = len(us), fnv(us)
    w = [0, 0, 0, 0, 0]
    if CUR["op"] == "pw":
        w = [e.u32(ch), e.u32(ch + 4), e.u32(ch + 8), e.u32(ch + 12), L.lab(e.u32(ch + 0x14))]
    L.rec("change", (L.lab(self_), flags, w[0], w[1], w[2], w[3], w[4], length, h))


def stub_format(e):
    L = e.cl
    table, size, id_ = e.r(3), e.r(5), e.r(6)
    tagged = id_ != 0x1c9
    magic = tid = val = isvt = 0
    if tagged:
        tag = e.r(7)
        magic, isvt, tid, val = e.u32(tag), int(e.u32(tag + 4) == e.u32(0x10261344)), e.u32(tag + 8), e.u32(tag + 12)
    L.rec("format", (int(table == CUR["table"]), size, id_, int(tagged), magic, tid, val, isvt))
    text = "%d:%d" % (id_, T.s32(val)) if tagged else "%d" % id_
    put_units(e, e.r(4), [ord(c) for c in text])
    e.set_r(3, 0)


def stub_labs(e):
    v = e.r(3)
    e.cl.rec("labs", (v,))
    e.set_r(3, (-T.s32(v)) & MASK if T.s32(v) < 0 else v)


def stub_wcsset(e):
    a, c = e.r(3), e.r(4) & 0xFFFF
    e.cl.rec("wcsset", (c,))
    n = len(units(e, a, 0x4000))
    e.write(a, struct.pack(">H", c) * n)


def stub_wcscpy(e):
    dst = e.r(3)
    put_units(e, dst, units(e, e.r(4), 0x4000))


def stub_swprintf(e):
    buf, size, fmt, v = e.r(3), e.r(4), e.r(5), e.r(6)
    assert units(e, fmt) == [0x25, 0x64], "the visitor format is not %d"
    put_units(e, buf, [ord(c) for c in ("%d" % T.s32(v))[:size - 1]])


def stub_cursor(e):
    e.cl.rec("cursordtor", (T.s16(e.r(4)),))


def stub_delete(e):
    e.cl.rec("delete", ())


STUBS.internal(0x1018d798, stub_change)
STUBS.internal(0x10239730, stub_format)
STUBS.internal(0x10239928, stub_labs)
STUBS.internal(0x102397f0, stub_wcsset)
STUBS.internal(0x10238c80, stub_wcscpy)
STUBS.internal(0x10239160, stub_swprintf)
STUBS.internal(0x1018f2b8, stub_cursor)
STUBS.internal(0x10238218, stub_delete)


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def put_text(self, us):
        a = self.alloc(2 * len(us) + 2)
        self.write(a, struct.pack(self.fmt[0] + "%dH" % (len(us) + 1), *us, 0))
        return a

    def fill_buffer(self):
        self.write(self.g("buffer"), struct.pack(self.fmt[0] + "%dH" % BUFCHARS, *([0x41] * BUFCHARS)))

    def rd_buffer(self):
        return list(struct.unpack(self.fmt[0] + "%dH" % BUFCHARS, self.read(self.g("buffer"), 2 * BUFCHARS)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
        CUR["op"] = case["op"]
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

    def set_table(self, a):
        CUR["table"] = a
        self.words(ADDR["table"], [a])

    def make_change(self, case):
        text = self.put_text(case["text"])
        style = self.alloc(8)
        self.words(style, [text])
        rect = self.alloc(16) if case["rect"] else 0
        ch = self.alloc(0x20)
        self.words(ch, case["w"] + [style, rect])
        return ch

    def call_change(self, case):
        ch = self.make_change(case)
        self.self_ = self.alloc(0x200)
        T.call_ppc(self.e, FN[case["op"]], [self.self_, case["flags"], ch])
        return None

    def call_dtor(self, case):
        self.self_ = self.alloc(0x200) if case["self"] else 0
        r3, _, _ = T.call_ppc(self.e, DTOR, [self.self_, case["flag"] & 0xFFFF])
        return int(r3 & MASK == self.self_)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_tx_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_tx_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_table(self, a):
        self.lib.tpw_tx_settable(a)

    def call_change(self, case):
        text = self.put_text(case["text"])
        rect = self.alloc(16) if case["rect"] else 0
        self_ = self.alloc(0x200)
        w = case["w"]
        self.lib.tpw_tx_change(FNNUM[case["op"]], self_, case["flags"], w[0], w[1], w[2], w[3], text, rect)
        return None

    def call_dtor(self, case):
        self_ = self.alloc(0x200) if case["self"] else 0
        r = self.lib.tpw_tx_dtor(self_, T.s16(case["flag"]))
        return int(r == self_)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_tx_addr.restype, lib.tpw_tx_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_tx_alloc.restype, lib.tpw_tx_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_tx_settable.argtypes = [C.c_uint64]
    lib.tpw_tx_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_tx_change.argtypes = [C.c_int32, C.c_uint64] + [C.c_uint32] * 5 + [C.c_uint64, C.c_uint64]
    lib.tpw_tx_dtor.restype, lib.tpw_tx_dtor.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_tx_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def run(s, case):
    try:
        s.fill_buffer()
        s.set_table(s.alloc(16))
        ans = s.call_dtor(case) if case["op"] == "dtor" else s.call_change(case)
        return {"answer": ans, "log": s.log(), "buffer": s.rd_buffer()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


VALUES = [0, 1, -1, 2, -2, 99, 0x7FFFFFFF, -0x80000000, 0xEEEEEEE, -0xEEEEEEE, 0x12345678, -0x12345678]


def rtext(r, n=None):
    n = r.choice([0, 1, 2, 5, 17, 40]) if n is None else n
    return [r.randrange(1, 0x10000) for _ in range(n)]


def make(r, op, flags=None, value=None, text=None):
    if flags is None:
        if r.random() < 0.6:
            flags = (0x100 if op == "pw" else 0x80) | r.choice([0, 0, 1, 0x7E, 0x200, 0xFFFFFE00 & ~0x100 & ~0x80])
        else:
            flags = r.choice([0, 1, 2, 0x7F, 0x200, 0x1000, 0xFFFFFE7F]) & ~(0x100 if op == "pw" else 0x180)
        if op == "pw" and not flags & 0x100:
            pass
    v = r.choice(VALUES) if value is None else value
    w = [v & MASK] + [r.randrange(0, 1 << 32) for _ in range(3)]
    return {"op": op, "flags": flags & MASK, "w": w, "text": rtext(r) if text is None else text, "rect": r.choice([0, 1])}


def change_tests(op, name, seed):
    def edges():
        r = random.Random(seed)
        main = 0x100 if op == "pw" else 0x80
        for v in VALUES:
            yield make(r, op, main, v)
        yield make(r, op, main | 0x180 if op != "pw" else 0x1FF, 5)
        for f in (0, 1, 0x80 if op == "pw" else 0x100 - 0x100, 0x7F):
            yield make(r, op, f, 3)
        if op == "pw":
            yield make(r, op, main, 0, [])
            yield make(r, op, main, 0, rtext(r, 0x2F0))
            yield make(r, op, main, 0, rtext(r, 1))

    def rnd(r):
        return make(r, op)
    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


def dtor_edges():
    for self_ in (0, 1):
        for flag in (-1, 0, 1, 2, 0x7FFF, -0x8000):
            yield {"op": "dtor", "self": self_, "flag": flag}


def dtor_random(r):
    return {"op": "dtor", "self": r.choice([0, 1, 1]), "flag": r.choice([-1, 0, 1, 2, 0x7FFF, -0x8000, r.randrange(-0x8000, 0x8000)])}


TESTS = [
    change_tests("cash", "InterfaceCashTextObject::Change", 1),
    change_tests("pw", "InterfacePasswordTextObject::Change", 2),
    FnTest("InterfacePasswordTextObject::~InterfacePasswordTextObject", ppc, host, edge=dtor_edges, rnd=dtor_random),
    change_tests("null", "InterfaceNullCashTextObject::Change", 3),
    change_tests("pct", "InterfacePercentTextObject::Change", 4),
    change_tests("vis", "InterfaceVisitorNumTextObject::Change", 5),
]
