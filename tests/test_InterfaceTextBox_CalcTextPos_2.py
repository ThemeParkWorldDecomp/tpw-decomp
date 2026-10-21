"""Unit v5975, the text box, the string heap and a timer's operator delete, against libtpw_unit_InterfaceTextBox_CalcTextPos_2.dylib
(tests/unit_InterfaceTextBox_CalcTextPos_2.cpp), main app ("SimTheme Park"), src/game/ui/InterfaceStringHeap.cpp, src/game/ui/InterfaceTextBox.cpp:
  InterfaceTextBox::CalcTextPos 0x101d7dac, InterfaceTextBox::SetText 0x101d7f50, InterfaceStringHeap::GetAndCopy 0x101d7b60,
  InterfaceStringHeap::GetString 0x101d79ec, InterfaceTimer::operator_delete 0x101d822c

A case is a world and a call. The world is built twice by ONE piece of Python code (the Mac's big-endian 32-bit memory for the original,
the host's little-endian 64-bit pointers for the translation; the object's field offsets differ and the host's come from the library's
layout export). Callees outside the unit are fakes on both sides: the font's width and height (through __ptr_glue on the Mac: the font's
vtable slots hold the markers 0x7001 and 0x7002), operator new[] (answers the case's block) and operator delete, __assertion_failed.
wcslen and wcscpy are plain on both sides and log nothing.

The world: a text box (font, text block, alignments, length, client rectangle), the heap's six globals with two tables, the timer
table and its count. Compared: the answer (pointers as labels or offsets into the tables), the object's fields, the text blocks, the
tables, the timers' free words and the event log.

No fused operations. Not tested: a NULL timer (the original writes at 0x14 of NULL), a NULL string for SetText / GetAndCopy
(wcslen reads it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InterfaceTextBox_CalcTextPos_2"
MASK = 0xFFFFFFFF

FN_CALC, FN_SETTEXT, FN_GETSTRING, FN_GETANDCOPY, FN_DELETE = 0x101D7DAC, 0x101D7F50, 0x101D79EC, 0x101D7B60, 0x101D822C
SIZES = {FN_CALC: 368, FN_SETTEXT: 176, FN_GETSTRING: 236, FN_GETANDCOPY: 88, FN_DELETE: 180}
OWN = set(SIZES)

G_PTR_GLUE, G_WCSLEN, G_WCSCPY, G_NEWARR, G_DELETE, G_ASSERT = 0x1023BAA8, 0x102389C8, 0x10238C80, 0x10238668, 0x10238218, 0x10238BD8

ADDR = dict(TableB=0x10395C34, TableA=0x10395C38, RowsB=0x10395C3C, WidthB=0x10395C40, RowsA=0x10395C44, WidthA=0x10395C48,
            TimerCount=0x10395C50, Timers=0x10395C54)

KEYS = ["font", "text", "hor", "ver", "len", "tx", "ty", "cl", "ct", "cr", "cb"]
PPC_OFF = dict(font=0x48, text=0x138, hor=0x13C, ver=0x140, len=0x148, tx=0x14C, ty=0x14E, cl=0x2C, ct=0x2E, cr=0x30, cb=0x32,
               free=0x14, objsize=0x160, timersize=32)
NEWB = [0]


def s16(v):
    return T.s16(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def save_toc(e):
    """What the glue does before a call: the caller reloads r2 from 0x14(r1) afterwards."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))


def stub_ptr_glue(e):
    save_toc(e)
    L = e.cl
    marker = e.r(12)
    if marker == 0x7001:
        L.rec("FontWidth", (L.lab(e.r(3)), L.lab(e.r(4)), s16(e.r(5)), s16(e.r(6))))
        e.set_r(3, L.ret("FontWidth") & MASK)
    elif marker == 0x7002:
        L.rec("FontHeight", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("FontHeight") & MASK)
    else:
        raise AssertionError("__ptr_glue with r12 = %#x" % marker)


def stub_wcslen(e):
    save_toc(e)
    a, n = e.r(3), 0
    while e.read(a + 2 * n, 2) != b"\0\0":
        n += 1
    e.set_r(3, n)


def stub_wcscpy(e):
    save_toc(e)
    dst, src, n = e.r(3), e.r(4), 0
    while True:
        ch = e.read(src + 2 * n, 2)
        e.write(dst + 2 * n, ch)
        n += 1
        if ch == b"\0\0":
            break


STUBS.internal(G_PTR_GLUE, stub_ptr_glue)
STUBS.internal(G_WCSLEN, stub_wcslen)
STUBS.internal(G_WCSCPY, stub_wcscpy)
STUBS.internal(G_NEWARR, CL.recorder("newarr", "u", ret=lambda e, log: NEWB[0]))
STUBS.internal(G_DELETE, CL.recorder("delete", "L"))
STUBS.internal(G_ASSERT, CL.recorder("assert", "ssi"))


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def halves(self, a, hs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", h & 0xFFFF) for h in hs))

    def rd_halves(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def rd_half(self, a):
        return self.rd_halves(a, 1)[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def make_blocks(self):
        sizes = dict(obj=0x200, font=0x40, vt=0x40, txt=128, newb=128, src=64, ta=128, tb=128, tim=1200)
        self.B = {k: self.alloc(v) for k, v in sizes.items()}
        self.labels = {self.B["obj"]: 1, self.B["font"]: 2, self.B["txt"]: 3, self.B["newb"]: 4, self.B["src"]: 5}

    def label(self, p):
        return 0 if p == 0 else self.labels.get(p, -1)


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    off = PPC_OFF

    def __init__(self, e, case):
        self.e = e
        self.make_blocks()
        NEWB[0] = self.B["newb"]
        self.L = e.cl = CL.PpcLog(dict(self.labels), case["queues"])
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

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK

    def op_calc(self):
        self.call(FN_CALC, [self.B["obj"]])
        return 0

    def op_settext(self):
        return self.call(FN_SETTEXT, [self.B["obj"], self.B["src"]]) & 0xFF

    def op_getstring(self, n):
        return self.call(FN_GETSTRING, [n])

    def op_getandcopy(self):
        return self.call(FN_GETANDCOPY, [self.B["src"]])

    def op_delete(self, p):
        self.call(FN_DELETE, [p])
        return 0


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.off
        self.make_blocks()
        CL.host_begin(lib, dict((v, k) for k, v in self.labels.items()), case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v5975_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v5975_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def op_calc(self):
        self.lib.tpw_v5975_calc(self.B["obj"])
        return 0

    def op_settext(self):
        return self.lib.tpw_v5975_settext(self.B["obj"], self.B["src"], self.B["newb"]) & 0xFF

    def op_getstring(self, n):
        return self.lib.tpw_v5975_getstring(n)

    def op_getandcopy(self):
        return self.lib.tpw_v5975_getandcopy(self.B["src"])

    def op_delete(self, p):
        self.lib.tpw_v5975_timerdelete(p)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v5975_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_v5975_calc.argtypes = [C.c_uint64]
    lib.tpw_v5975_settext.restype, lib.tpw_v5975_settext.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_v5975_getstring.restype, lib.tpw_v5975_getstring.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_v5975_getandcopy.restype, lib.tpw_v5975_getandcopy.argtypes = C.c_uint64, [C.c_uint64]
    lib.tpw_v5975_timerdelete.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    vals = list(out)[:n]
    off = dict(zip(KEYS + ["objsize", "free", "timersize"], vals))
    lib.off = off
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def put_box(s, c):
    o, off = s.B["obj"], s.off
    s.ptr(o + off["font"], s.B["font"] if c["font"] else 0)
    s.ptr(o + off["text"], s.B["txt"] if c["text"] else 0)
    s.words(o + off["hor"], [c["hor"]])
    s.words(o + off["ver"], [c["ver"]])
    s.words(o + off["len"], [c["len"]])
    s.halves(o + off["tx"], [c["tx"]])
    s.halves(o + off["ty"], [c["ty"]])
    for k, v in zip(("cl", "ct", "cr", "cb"), c["rect"]):
        s.halves(o + off[k], [v])
    f, vt = s.B["font"], s.B["vt"]
    s.ptr(f + 0x10, vt)
    s.words(vt + 0xC, [0x7002, 0x7001])
    s.halves(s.B["txt"], c["txtinit"] + [0] * (64 - len(c["txtinit"])))
    s.halves(s.B["src"], c["src"] + [0] * (32 - len(c["src"])))


def put_heap(s, c):
    s.halves(s.B["ta"], c["ta"])
    s.halves(s.B["tb"], c["tb"])
    for name in ("RowsA", "WidthA", "RowsB", "WidthB"):
        s.words(s.g(name), [c[name]])
    s.ptr(s.g("TableA"), s.B["ta"] if c["tableA"] else 0)
    s.ptr(s.g("TableB"), s.B["tb"] if c["tableB"] else 0)
    s.halves(s.B["src"], c["src"] + [0] * (32 - len(c["src"])))


def slot_addr(s, k):
    return s.B["tim"] + (2 + k) * s.off["timersize"]


def put_timers(s, c):
    for k in range(-2, 18):
        s.words(slot_addr(s, k) + s.off["free"], [c["free"][k + 2]])
    s.ptr(s.g("Timers"), slot_addr(s, 0) if c["table"] else 0)
    s.words(s.g("TimerCount"), [c["count"]])


def where(s, p):
    if p == 0:
        return None
    for k in ("ta", "tb", "txt", "newb", "obj", "src"):
        d = p - s.B[k]
        if 0 <= d < 128:
            return (k, d)
    return ("other", p)


def obs_box(s):
    o, off = s.B["obj"], s.off
    return {"font": s.label(s.rd_ptr(o + off["font"])), "text": s.label(s.rd_ptr(o + off["text"])),
            "words": [s.rd_word(o + off[k]) for k in ("hor", "ver", "len")],
            "halves": [s.rd_half(o + off[k]) for k in ("tx", "ty", "cl", "ct", "cr", "cb")],
            "txt": s.rd_halves(s.B["txt"], 64), "newb": s.rd_halves(s.B["newb"], 64), "src": s.rd_halves(s.B["src"], 32)}


def obs_heap(s):
    return {"ta": s.rd_halves(s.B["ta"], 64), "tb": s.rd_halves(s.B["tb"], 64), "src": s.rd_halves(s.B["src"], 32),
            "globals": [s.rd_word(s.g(n)) for n in ("RowsA", "WidthA", "RowsB", "WidthB")]}


def run(s, case):
    try:
        op = case["op"]
        if op in ("calc", "settext"):
            put_box(s, case)
            ans = s.op_calc() if op == "calc" else s.op_settext()
            return {"answer": ans, "log": s.log(), "box": obs_box(s)}
        if op in ("getstring", "getandcopy"):
            put_heap(s, case)
            if op == "getstring":
                ans = where(s, s.op_getstring(case["arg"] & MASK if False else case["arg"]))
            else:
                ans = where(s, s.op_getandcopy())
            return {"answer": ans, "log": s.log(), "heap": obs_heap(s)}
        put_timers(s, case)
        p = slot_addr(s, case["k"]) + case["extra"]
        s.op_delete(p)
        S = s.off["timersize"]
        return {"log": s.log(), "free": [s.rd_word(slot_addr(s, k) + s.off["free"]) for k in range(-2, 18)],
                "at": s.rd_word(p + s.off["free"])}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

WIDTHS = [-70000, -1, 0, 1, 7, 40, 200, 32768, 65535, 0x12345, -32769]
HEIGHTS = [-3, 0, 1, 12, 13, 32767, 32768, 0x10005, -40000]


def rs16(r):
    return r.choice([0, 1, -1, 32767, -32768, r.randrange(-300, 300), r.randrange(-300, 300), r.randrange(-32768, 32768)])


def text(r, maxlen=10):
    n = r.randrange(0, maxlen + 1)
    return [r.choice([0x41, 0x61, 0x20, 0x100, 0xFFFF, r.randrange(1, 0x10000)]) for _ in range(n)]


def box_case(op, r, **kw):
    c = {"op": op, "font": r.random() > 0.15, "text": r.random() > 0.15, "hor": r.choice([0, 1, 2, 3, -1, 5]),
         "ver": r.choice([0, 1, 2, 3, -1, 5]), "len": r.randrange(0, 13), "tx": rs16(r), "ty": rs16(r),
         "rect": [rs16(r) for _ in range(4)], "txtinit": text(r), "src": text(r),
         "queues": {"FontWidth": [r.choice(WIDTHS + [r.randrange(-300, 300)])], "FontHeight": [r.choice(HEIGHTS + [r.randrange(-50, 50)])]}}
    c.update(kw)
    return c


def calc_edges():
    r = random.Random(11)
    for font in (False, True):
        for txt in (False, True):
            for hor in (0, 1, 2, 3, -1):
                for ver in (0, 1, 2, 3, -1):
                    yield box_case("calc", r, font=font, text=txt, hor=hor, ver=ver)
    for w in (0, 1, 2, 3, 40, 65535, 32768):
        for rect in ([0, 0, 0, 0], [1, 1, 8, 8], [-5, -5, 4, 4], [-32768, -32768, 32767, 32767], [3, 3, 100, 100]):
            yield box_case("calc", r, font=True, text=True, hor=2, ver=2, rect=rect, txtinit=[], queues={"FontWidth": [w], "FontHeight": [w]})


def calc_random(r):
    return box_case("calc", r)


def settext_edges():
    r = random.Random(12)
    for src_len in (0, 1, 5):
        for dl in (-1, 0, 1):
            for txt in (False, True):
                for font in (False, True):
                    yield box_case("settext", r, src=[0x42] * src_len, len=max(0, src_len + dl), text=txt, font=font)
    yield box_case("settext", r, src=[0x42] * 3, len=-1, text=True)
    yield box_case("settext", r, src=[], len=0, text=True)


def settext_random(r):
    return box_case("settext", r)


def heap_case(op, r, **kw):
    ra, rb = r.randrange(0, 6), r.randrange(0, 6)
    wa, wb = r.randrange(0, 11), r.randrange(0, 11)
    halves = [r.choice([0xFFFF, 0xFFFF, 0x41, r.randrange(0x10000)]) for _ in range(64)]
    tb = [r.choice([0xFFFF, 0xFFFF, 0x41, r.randrange(0x10000)]) for _ in range(64)]
    for rows, width, arr in ((ra, wa, halves), (rb, wb, tb)):
        for i in range(rows):
            if r.random() < 0.5 and i * width + 1 < 64:
                arr[i * width] = arr[i * width + 1] = 0xFFFF
            elif i * width + 1 < 64 and r.random() < 0.5:
                arr[i * width] = 0x42
    c = {"op": op, "RowsA": ra, "WidthA": wa, "RowsB": rb, "WidthB": wb, "ta": halves, "tb": tb,
         "tableA": r.random() > 0.1, "tableB": r.random() > 0.1, "arg": r.randrange(-1, 13), "src": text(r, 9), "queues": {}}
    c.update(kw)
    return c


def getstring_edges():
    r = random.Random(13)
    for arg in (-1, 0, 1, 4, 5, 6, 9, 10, 12):
        for ta, tb in ((False, True), (True, False), (True, True), (False, False)):
            yield heap_case("getstring", r, arg=arg, tableA=ta, tableB=tb)
    for arg in range(0, 11):
        yield heap_case("getstring", r, arg=arg, RowsA=3, WidthA=6, RowsB=3, WidthB=6, tableA=True, tableB=True)


def getstring_random(r):
    return heap_case("getstring", r)


def getandcopy_edges():
    r = random.Random(14)
    for n in (0, 1, 3, 5, 8):
        for ta, tb in ((False, True), (True, False), (True, True)):
            yield heap_case("getandcopy", r, src=[0x43] * n, tableA=ta, tableB=tb)
    for n in range(0, 10):
        yield heap_case("getandcopy", r, src=[0x44] * n, RowsA=2, WidthA=9, RowsB=2, WidthB=5, tableA=True, tableB=True)


def getandcopy_random(r):
    return heap_case("getandcopy", r)


def timer_case(r, **kw):
    c = {"op": "delete", "free": [r.choice([0, 1, 7]) for _ in range(20)], "table": r.random() > 0.1, "count": r.randrange(0, 7),
         "k": r.randrange(-2, 10), "extra": r.choice([0, 0, 0, 4]), "queues": {}}
    c.update(kw)
    return c


def delete_edges():
    r = random.Random(15)
    for count in (0, 1, 3):
        for k in (-2, -1, 0, count - 1, count, count + 1):
            if -2 <= k <= 9:
                for table in (True, False):
                    yield timer_case(r, count=count, k=k, table=table, extra=0)
    yield timer_case(r, count=3, k=1, table=True, extra=4)
    yield timer_case(r, count=3, k=2, table=True, extra=4)


def delete_random(r):
    return timer_case(r)


TESTS = [
    FnTest("InterfaceTextBox::CalcTextPos", ppc, host, edge=calc_edges, rnd=calc_random),
    FnTest("InterfaceTextBox::SetText", ppc, host, edge=settext_edges, rnd=settext_random),
    FnTest("InterfaceStringHeap::GetString", ppc, host, edge=getstring_edges, rnd=getstring_random),
    FnTest("InterfaceStringHeap::GetAndCopy", ppc, host, edge=getandcopy_edges, rnd=getandcopy_random),
    FnTest("InterfaceTimer::operator_delete", ppc, host, edge=delete_edges, rnd=delete_random),
]
