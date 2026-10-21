"""Unit v5160, CUIGrapher::DrawGrid and CUIGrapher::DrawGraph, against libtpw_unit_CUIGrapher_DrawGraph.dylib
(tests/unit_CUIGrapher_DrawGraph.cpp), main app, src/game/ui/UIGrapher.cpp:
  DrawGrid 0x101fdb1c, DrawGraph 0x101fddbc

A case is a grapher object (fields by Mac offset) and a call. The object is built twice by one piece of code: in emulator memory
(big-endian) and in the host library's memory. LbDraw_Line/VerticalLine/HorizontalLine, swprintf, UI_DrawText and UI_MeasureText are fakes
on both sides recording every argument (colours as the word and index byte, text as a hash); UI_MeasureText answers its rectangle from
the queues ml and mr; swprintf writes the decimal value.

Compared: the event log and the object bytes. No fused operations. Not tested: DrawGrid with a column count other than 12, 36 or 144
(the original leaves r29 as the caller's value), DrawGraph with a count below 1 (f31, r28 stay the caller's).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CUIGrapher_DrawGraph"
MASK = 0xFFFFFFFF

GRID, GRAPH = 0x101fdb1c, 0x101fddbc
ADDR = dict(font=0x102b1558)
HOST_LAYOUT = [4]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update({GRID, GRAPH})


def col(e, p):
    b = e.read(p, 5)
    return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3], b[4]


def text_hash(e, p):
    n = h = 0
    while n < 32:
        c = struct.unpack(">H", e.read(p + 2 * n, 2))[0]
        if c == 0:
            break
        h = (h * 31 + c) & MASK
        n += 1
    return (h * 31 + n) & MASK


def rect(e, p):
    return tuple(s32(x) for x in struct.unpack(">4I", e.read(p, 16)))


def s8(v):
    v &= 0xFF
    return v - 256 if v >= 128 else v


def stub_line(e):
    e.cl.rec("Line", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & MASK, e.r(8) >> 24))


def stub_vline(e):
    e.cl.rec("VLine", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), e.r(6) & MASK, e.r(7) >> 24))


def stub_hline(e):
    e.cl.rec("HLine", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), e.r(6) & MASK, e.r(7) >> 24))


def stub_swprintf(e):
    size = e.r(4)
    fmt = struct.unpack(">2H", e.read(e.r(5), 4))
    value = s32(e.r(6))
    e.cl.rec("swprintf", (size, (fmt[0] << 16) | fmt[1], value))
    s = str(value)
    chars = [ord(c) for c in s][: max(size - 1, 0)] if size else []
    if size:
        e.write(e.r(3), b"".join(struct.pack(">H", c) for c in chars) + b"\0\0")
    e.set_r(3, len(s))


def stub_drawtext(e):
    packed = ((e.r(7) & 255) << 24) | ((e.r(8) & 255) << 16) | ((e.r(9) & 255) << 8) | (2 if e.r(10) else 0) | (e.u32(e.r(1) + 0x38) & 1)
    cw, ci = col(e, e.r(5))
    e.cl.rec("DrawText", (text_hash(e, e.r(3)),) + rect(e, e.r(4)) + (cw, ci, s8(e.r(6)), packed))


def stub_measure(e):
    e.cl.rec("Measure", (text_hash(e, e.r(3)),) + rect(e, e.r(4)) +
             (s8(e.r(5)), s8(e.r(6)), s8(e.r(7)), s32(e.r(8)), 1 if e.r(9) else 0))
    ml, mr = e.cl.ret("ml"), e.cl.ret("mr")
    e.write(e.r(9), struct.pack(">4I", ml & MASK, 0, mr & MASK, 0))


def glue(fn):
    """The import glue saves the TOC at 0x14(r1) (the caller reloads r2 from there afterwards)."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


STUBS.internal(0x1023adb0, glue(stub_line))
STUBS.internal(0x1023adc8, glue(stub_vline))
STUBS.internal(0x1023ade0, glue(stub_hline))
STUBS.internal(0x10239160, glue(stub_swprintf))
STUBS.internal(0x1018b578, stub_drawtext)
STUBS.internal(0x1018b8e4, stub_measure)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


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

    def grid(self, obj):
        T.call_ppc(self.e, GRID, [obj])
        return 0

    def graph(self, obj, data, count, limit, cw, ci):
        T.call_ppc(self.e, GRAPH, [obj, data, count & MASK, limit & MASK, cw, ci << 24])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def grid(self, obj):
        self.lib.tpw_gr_grid(obj)
        return 0

    def graph(self, obj, data, count, limit, cw, ci):
        self.lib.tpw_gr_graph(obj, data, s32(count), s32(limit), cw, ci)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gr_addr.restype, lib.tpw_gr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gr_alloc.restype, lib.tpw_gr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gr_grid.argtypes = [C.c_uint64]
    lib.tpw_gr_graph.argtypes = [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32, C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


OBJ = 0x48


def build(s, case):
    s.words(s.g("font"), [case["font"]])
    obj = s.alloc(OBJ)
    s.write(obj, bytes(OBJ))
    for off, v in case["fields"].items():
        s.words(obj + off, [v])
    s.write(obj + 0x30, bytes(case["gcol"]))
    s.write(obj + 0x3a, bytes(case["lcol"]))
    data = 0
    if case["op"] == "graph":
        data = s.alloc(4 * len(case["data"]))
        s.words(data, case["data"])
    return obj, data


def run(s, case):
    try:
        obj, data = build(s, case)
        if case["op"] == "grid":
            ans = s.grid(obj)
        else:
            cw = (case["colour"][0] << 24) | (case["colour"][1] << 16) | (case["colour"][2] << 8) | case["colour"][3]
            ans = s.graph(obj, data, case["count"], case["limit"], cw, case["colour"][4])
        return {"answer": ans, "log": s.log(), "obj": list(struct.unpack(s.fmt[0] + "%dI" % (0x30 // 4), s.read(obj, 0x30))),
                "cols": s.read(obj + 0x30, 5) + s.read(obj + 0x3a, 5)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, op):
    f = {}
    left = r.choice([0, 1, 2, 5, -7, 40, 100, r.randrange(-50, 400)])
    f[0x00] = r.choice([0, 1, 100, 144, r.randrange(1, 2000), -r.randrange(1, 50)])
    f[0x04] = r.choice([0, 1, 60, r.randrange(1, 1000), -r.randrange(1, 50)])
    f[0x08] = left
    f[0x0c] = r.choice([0, 3, -5, r.randrange(-20, 200)])
    f[0x10] = left + r.choice([0, 1, 100, r.randrange(1, 600), -30])
    f[0x14] = f[0x0c] + r.choice([0, 1, 100, r.randrange(1, 400), -30])
    ymin = r.choice([0, 5, -100, r.randrange(-1000, 1000)])
    f[0x1c] = ymin
    f[0x24] = ymin + r.choice([0, 1, -1, 50, 1000, r.randrange(-500, 3000)])
    f[0x28] = 0x24
    f[0x2c] = r.choice([1, 2, 5, 10])
    return {"op": op, "fields": f, "font": r.choice([10, 6, 0, 0x80, 0x1ff, 0xfffffff7, 3]),
            "gcol": [r.randrange(256) for _ in range(5)] + [0] * 5, "lcol": [r.randrange(256) for _ in range(5)] + [0] * 3,
            "queues": {"ml": [r.choice([0, 3, -4])], "mr": [r.choice([0, 20, 200, r.randrange(-50, 500)])]}}


def fix_cols(c):
    c["gcol"] = c["gcol"][:5] + [0] * 5
    c["lcol"] = c["lcol"][:5]
    # the 0x30 colour spans 0x30..0x34; keep bytes 0x35..0x39 zero
    c["gcol"] = c["gcol"][:5] + [0, 0, 0, 0, 0]
    # 0x3a..0x3e colour; 0x3f.. zero
    c["lcol"] = c["lcol"][:5] + [0, 0, 0]
    return c


def grid_case(r, columns=None, rows=None):
    c = base(r, "grid")
    c["fields"][0x28] = r.choice([0xc, 0x24, 0x90]) if columns is None else columns
    if rows is not None:
        c["fields"][0x2c] = rows
    return fix_cols(c)


def grid_edges():
    r = random.Random(501)
    for cols in (0xc, 0x24, 0x90, 0, 0xc):
        for rows in (0, 1, 3):
            yield grid_case(r, cols, rows)
    for _ in range(20):
        yield grid_case(r)


def graph_case(r, count=None, limit=None):
    c = base(r, "graph")
    c["count"] = r.choice([1, 2, 3, 10, 30]) if count is None else count
    c["limit"] = r.choice([0x8f, 0x90, 0x91, 10, 100, 1, 0, -5, 143]) if limit is None else limit
    if c["limit"] == 0 and c["fields"][0] == 0:
        c["fields"][0] = 1     # 0/0 is NaN: Unicorn's fcmpo answers an unordered compare differently from the hardware
    lo = c["fields"][0x1c]
    c["data"] = [r.choice([lo, c["fields"][0x24], lo - 20, r.randrange(lo - 100, lo + 3000), r.randrange(-2000, 2000)])
                 for _ in range(c["count"] + 2)]
    c["colour"] = [r.randrange(256) for _ in range(5)]
    return fix_cols(c)


def graph_edges():
    r = random.Random(777)
    for count in (1, 2, 5):
        for limit in (0x8f, 0x90, 1, 0, 144):
            yield graph_case(r, count, limit)
    for _ in range(30):
        yield graph_case(r)


TESTS = [
    FnTest("CUIGrapher::DrawGrid", ppc, host, edge=grid_edges, rnd=lambda r: grid_case(r)),
    FnTest("CUIGrapher::DrawGraph", ppc, host, edge=graph_edges, rnd=lambda r: graph_case(r)),
]
