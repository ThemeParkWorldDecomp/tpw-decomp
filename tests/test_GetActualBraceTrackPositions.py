"""Unit v5562: the brace placement of the track editor, against libtpw_unit_GetActualBraceTrackPositions.dylib
(tests/unit_GetActualBraceTrackPositions.cpp), main app ("SimTheme Park"), src/GetActualBraceTrackPositions.cpp:
  GetActualBraceTrackPositions 0x1003cc34, GetActualBraceTangents 0x1003c950, GetTangentLengths 0x1003c59c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, host offsets) for the translation. The callees outside
the unit are fakes on both sides that record what they read: YRotateLMatrixNoTrans (0x100c1698), GetVectorRotationAroundXAxisFromZ
(0x100c11a4), GetVectorRotationAroundZAxisFromY (0x100c1290; both answer from a queue) and GetTangentsFromZVector (0x1003be20). sqrt is
the C library's on both sides. GetTangentLengths runs as the original inside GetActualBraceTangents.

The world of a case: gTPGlobals' sine table, mask and scale, the 2.0 global at 0x1029cd14, gCoasterCurrent -> an instance with a type
block, a ride set -> mesh -> table -> four matrix nodes, and three braces (the one called, its next and its previous link) with their
spline blocks and 0x58-byte controls of random words. What is compared: every control of every brace (A and B), the banks (+0xe4),
the nodes, the answer blocks of GetTangentLengths and the event log.

Fused operations: the hooks of HardwareRounding cover the three functions. Not tested: a null ride set where the node is needed, an index
past a spline's count, angles whose table index leaves 32 bits.
"""
import copy
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_GetActualBraceTrackPositions"
MASK = 0xFFFFFFFF

FN_TP, FN_TAN, FN_LEN = 0x1003CC34, 0x1003C950, 0x1003C59C
SIZES = {FN_TP: 1012, FN_TAN: 656, FN_LEN: 864}
OWN = set(SIZES)

YROT, AXISX, AXISZ, GTZV = 0x100C1698, 0x100C11A4, 0x100C1290, 0x1003BE20

ADDR = dict(gTPGlobals=0x103B74C8, gCoasterCurrent=0x102B68A8, scale2=0x1029CD14)

# the Mac's offsets and sizes; the host's come from tpw_bt_layout in the same order
KEYS = ["brace_flags", "brace_prev", "brace_next", "brace_f90", "brace_data", "brace_angle", "brace_fe4", "brace_sz",
        "data_flags", "data_f18t", "data_sz", "spline_a", "spline_b", "spline_sz", "ctrl_sz", "ctrl_p",
        "set_heads", "set_sz", "mesh_f28", "mesh_sz", "table_entries", "table_sz", "entry_sz", "entry_node",
        "inst_type", "inst_sz", "tp_mask", "tp_scale", "tp_table"]
MAC = dict(brace_flags=8, brace_prev=0x64, brace_next=0x68, brace_f90=0x90, brace_data=0x100, brace_angle=0xE0, brace_fe4=0xE4,
           brace_sz=0x110, data_flags=4, data_f18t=0x18, data_sz=0x40, spline_a=4, spline_b=8, spline_sz=0xC, ctrl_sz=0x58,
           ctrl_p=0x40, set_heads=8, set_sz=0x100, mesh_f28=0x28, mesh_sz=0x40, table_entries=4, table_sz=8, entry_sz=0x14,
           entry_node=4, inst_type=4, inst_sz=0x20, tp_mask=4, tp_scale=8, tp_table=0xC)
HOST_LAYOUT = dict(brace_flags=8, data_flags=4, ctrl_sz=0x58, ctrl_p=0x40, tp_mask=4, tp_scale=8, tp_table=0xC)
HOST_OFF = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


TABLE = [fb(math.sin(2 * math.pi * i / 4096)) for i in range(0x1000)]


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rdw(e, a, n=1):
    return struct.unpack(">%dI" % n, e.read(a, 4 * n))


def cw(words):
    return tuple(canon_f(w) for w in words)


def stub_yrot(e):
    e.cl.rec("YRotate", cw(rdw(e, e.r(3) + 0x30, 3)) + (canon_f(fb(e.f(1))), canon_f(fb(e.f(2)))))


def stub_axisx(e):
    L = e.cl
    L.rec("AxisX", cw(rdw(e, e.r(3), 3)))
    e.set_f1(bitsf(L.ret("AxisX")))


def stub_axisz(e):
    L = e.cl
    L.rec("AxisZ", cw(rdw(e, e.r(3), 3)))
    e.set_f1(bitsf(L.ret("AxisZ")))


def stub_gtzv(e):
    L = e.cl
    sp = e.r(1)
    ctrl = e.r(5)
    L.rec("GTZV", (rdw(e, e.r(3))[0], rdw(e, e.r(8))[0], rdw(e, ctrl + 4)[0]) + cw(rdw(e, ctrl + 0x40, 3)) +
          (canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), e.u32(sp + 0x38), e.u32(sp + 0x3C)))
    # the span vectors (r9, r10) stay unwritten stack when a brace has no link at the track's end, and the real callee never reads them
    L.rec("GTZVvec", cw(rdw(e, e.r(4), 3)))


STUBS.internal(YROT, stub_yrot)
STUBS.internal(AXISX, stub_axisx)
STUBS.internal(AXISZ, stub_axisz)
STUBS.internal(GTZV, stub_gtzv)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(canon_f(w) for w in struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    off = MAC
    pw = 4

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

    def ptr(self, a, v):
        self.words(a, [v])

    def log(self):
        return self.L.log

    def call(self, fn, args):
        with ROUNDING(self.e):
            T.call_ppc(self.e, fn, [x & MASK for x in args])


class Host(Side):
    fmt = "<I"
    pw = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = HOST_OFF
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bt_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        l = self.lib
        if fn == FN_TP:
            l.tpw_bt_tp(args[0], args[1])
        elif fn == FN_TAN:
            l.tpw_bt_tan(args[0], args[1])
        else:
            l.tpw_bt_len(args[0] & MASK, args[1], args[2], args[3], s32(args[4]), args[5], args[6])


def s32(v):
    return T.s32(v)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bt_addr.restype, lib.tpw_bt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bt_alloc.restype, lib.tpw_bt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bt_tp.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_bt_tan.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_bt_len.argtypes = [C.c_uint32, C.c_uint64, C.c_uint64, C.c_uint64, C.c_int32, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 64)()
    n = lib.tpw_bt_layout(out)
    assert n == len(KEYS), n
    HOST_OFF.update(zip(KEYS, list(out)[:n]))
    for k, v in HOST_LAYOUT.items():
        assert HOST_OFF[k] == v, (k, HOST_OFF[k])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def mk_brace(s, spec, ctx):
    o = s.off
    pw = s.pw
    brace = s.alloc(o["brace_sz"])
    data = s.alloc(o["data_sz"])
    s.words(data + o["data_flags"], [spec["dflags"]])
    pyl = s.alloc(0x40)
    s.words(pyl, [spec["pylon"]])
    s.ptr(data + o["data_f18t"], pyl)
    spl = s.alloc(o["spline_sz"])
    n = spec["count"]
    ca = s.alloc(n * 0x58)
    cb = s.alloc(n * 0x58)
    for i in range(n):
        s.words(ca + i * 0x58, spec["A"][i])
        s.words(cb + i * 0x58, spec["B"][i])
    s.words(spl, [n])
    s.ptr(spl + o["spline_a"], ca)
    s.ptr(spl + o["spline_b"], cb)
    s.words(brace + o["brace_flags"], [spec["flags"]])
    s.words(brace + o["brace_angle"], [spec["angle"]])
    s.words(brace + o["brace_fe4"], [spec["fe4"]])
    s.ptr(brace + o["brace_f90"], spl)
    s.ptr(brace + o["brace_data"], data)
    return brace, [ca + i * 0x58 for i in range(n)], [cb + i * 0x58 for i in range(n)]


def build(s, case):
    o = s.off
    tp = s.g("gTPGlobals")
    s.words(tp + o["tp_mask"], [case["mask"]])
    s.words(tp + o["tp_scale"], [case["scale"]])
    s.words(tp + o["tp_table"], TABLE)
    s.words(s.g("scale2"), [case["two"]])
    typ = s.alloc(0x40)
    s.words(typ, [case["typeword"]])
    inst = s.alloc(o["inst_sz"])
    s.ptr(inst + o["inst_type"], typ)
    s.ptr(s.g("gCoasterCurrent"), inst)
    # ride set -> mesh -> table -> entries -> nodes
    nodes = []
    ent = s.alloc(4 * o["entry_sz"])
    for i in range(4):
        node = s.alloc(0x40)
        s.words(node, case["nodes"][i])
        nodes.append(node)
        s.ptr(ent + i * o["entry_sz"] + o["entry_node"], node)
    table = s.alloc(o["table_sz"])
    s.ptr(table + o["table_entries"], ent)
    mesh = s.alloc(o["mesh_sz"])
    s.ptr(mesh + o["mesh_f28"], table)
    rset = s.alloc(o["set_sz"])
    s.ptr(rset + o["set_heads"], mesh)
    ctx = {"nodes": nodes, "set": rset if case["useSet"] else 0, "braces": []}
    for spec in case["braces"]:
        ctx["braces"].append(mk_brace(s, spec, ctx))
    m = ctx["braces"][0][0]
    if case["hasNext"]:
        s.ptr(m + o["brace_next"], ctx["braces"][1][0])
    if case["hasPrev"]:
        s.ptr(m + o["brace_prev"], ctx["braces"][2][0])
    ctx["out"] = s.alloc(0x40)
    return ctx


def execute(s, case, ctx):
    m = ctx["braces"][0][0]
    if case["op"] == "tp":
        s.call(FN_TP, [ctx["set"], m])
    elif case["op"] == "tan":
        s.call(FN_TAN, [m, ctx["set"]])
    else:
        out = ctx["out"]
        s.call(FN_LEN, [case["index"], m, out, out + 4, case["useA"], out + 8 if case["vecs"][0] else 0,
                        out + 0x14 if case["vecs"][1] else 0])


def observe(s, case, ctx):
    o = s.off
    res = {"log": s.log()}
    for k, (brace, ca, cb) in enumerate(ctx["braces"]):
        res["A%d" % k] = [s.rd_words(c, 22) for c in ca]
        res["B%d" % k] = [s.rd_words(c, 22) for c in cb]
        res["fe4_%d" % k] = s.rd_word(brace + o["brace_fe4"])
    res["nodes"] = [s.rd_words(n, 16) for n in ctx["nodes"]]
    res["out"] = s.rd_words(ctx["out"], 8)
    return res


def run(s, case):
    try:
        ctx = build(s, case)
        execute(s, case, ctx)
        return observe(s, case, ctx)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x3F800000, 0xBF800000]


def rf(r):
    k = r.random()
    if k < 0.04:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-30, 30))
    if k < 0.9:
        return fb(r.uniform(-1, 1))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-4, 3))


def control(r):
    w = [r.getrandbits(32) for _ in range(22)]
    w[1] = r.randrange(4)
    for i in (10, 11, 12, 16, 17, 18):
        w[i] = rf(r)
    return w


def brace_spec(r, count, flags, dflags):
    return {"count": count, "flags": flags, "dflags": dflags, "pylon": r.getrandbits(32), "angle": fb(r.uniform(-8, 8)),
            "fe4": r.getrandbits(32), "A": [control(r) for _ in range(count)], "B": [control(r) for _ in range(count)]}


def base(r, op, counts=None, flags=None, dflags=None, use_set=None):
    counts = counts or (r.randrange(1, 5), r.randrange(1, 4), r.randrange(1, 4))
    if flags is None:
        flags = r.getrandbits(32) & ~0x10000040 & ~0x2000 & ~0x80
        for b in (0x40, 0x80, 0x2000, 0x10000000):
            if r.random() < 0.5:
                flags |= b
    if dflags is None:
        dflags = r.getrandbits(32) & ~1 | r.randrange(2)
    scale = r.choice([fb(4096 / (2 * math.pi)), fb(r.uniform(10, 900)), fb(r.uniform(0.5, 20))])
    c = {"op": op, "mask": r.choice([0xFFF, 0xFFF, 0x3FF, 0x7F]), "scale": scale, "two": r.choice([fb(2.0), rf(r)]),
         "typeword": r.getrandbits(32), "nodes": [[rf(r) for _ in range(16)] for _ in range(4)],
         "hasNext": r.random() < 0.6, "hasPrev": r.random() < 0.6,
         "useSet": (r.random() < 0.6) if use_set is None else use_set,
         "braces": [brace_spec(r, counts[0], flags, dflags), brace_spec(r, counts[1], 0, 0), brace_spec(r, counts[2], 0, 0)],
         "queues": {"AxisX": [fb(r.uniform(-4, 4)) for _ in range(3)], "AxisZ": [fb(r.uniform(-4, 4)) for _ in range(3)]},
         "index": 0, "useA": 1, "vecs": (True, True)}
    return c


def tp_case(r, flags=None, dflags=None, counts=None):
    c = base(r, "tp", counts, flags, dflags)
    f = c["braces"][0]["flags"]
    if not (c["braces"][0]["dflags"] & 1 or f & 0x2000):
        c["useSet"] = True
    return c


def tp_edges():
    r = random.Random(5001)
    for dfl in (0, 1):
        for f2000 in (0, 0x2000):
            for f40 in (0, 0x40):
                for f10 in (0, 0x10000000):
                    for cnt in (1, 3):
                        yield tp_case(r, f2000 | f40 | f10 | 0x80, dfl, (cnt, 1, 1))
    yield tp_case(r, 0x40, 1, (2, 1, 1))
    c = tp_case(r, 0x40, 1, (2, 1, 1))
    c["useSet"] = False
    yield c


def tp_random(r):
    return tp_case(r)


def tan_case(r, flags=None, use_set=None, counts=None):
    return base(r, "tan", counts, flags, None, use_set)


def tan_edges():
    r = random.Random(5002)
    for use_set in (False, True):
        for fl in (0, 0x40, 0xC0):
            for cnt in (1, 2, 3):
                c = tan_case(r, fl, use_set, (cnt, 2, 2))
                for hn in (False, True):
                    for hp in (False, True):
                        c2 = copy.deepcopy(c)
                        c2["hasNext"], c2["hasPrev"] = hn, hp
                        yield c2


def tan_random(r):
    return tan_case(r)


def len_case(r, index=None, use_a=None, vecs=None):
    c = base(r, "len")
    n = c["braces"][0]["count"]
    c["index"] = r.randrange(n) if index is None else min(index, n - 1)
    c["useA"] = r.randrange(2) if use_a is None else use_a
    c["vecs"] = (r.random() < 0.7, r.random() < 0.7) if vecs is None else vecs
    return c


def len_edges():
    r = random.Random(5003)
    for use_a in (0, 1):
        for index in (0, 1, 2, 3):
            for cnt in (1, 2, 4):
                for hn in (False, True):
                    for hp in (False, True):
                        for vecs in ((True, True), (False, False)):
                            c = len_case(r, index, use_a, vecs)
                            c["braces"][0] = dict(c["braces"][0])
                            n = cnt
                            c["braces"][0]["count"] = n
                            c["braces"][0]["A"] = [control(r) for _ in range(n)]
                            c["braces"][0]["B"] = [control(r) for _ in range(n)]
                            c["index"] = min(index, n - 1)
                            c["hasNext"], c["hasPrev"] = hn, hp
                            yield c


def len_random(r):
    return len_case(r)


TESTS = [
    FnTest("GetActualBraceTrackPositions", ppc, host, edge=tp_edges, rnd=tp_random),
    FnTest("GetActualBraceTangents", ppc, host, edge=tan_edges, rnd=tan_random),
    FnTest("GetTangentLengths", ppc, host, edge=len_edges, rnd=len_random),
]
