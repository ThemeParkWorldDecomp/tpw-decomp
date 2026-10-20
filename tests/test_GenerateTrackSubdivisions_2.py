"""Unit v51008: GenerateTrackSubdivisions 0x1004e8c0, CoasterSetSpeed 0x1004b94c, GetBankAtBrace 0x1004b1f8, FlashEndPylons 0x10052158,
DecCoasterConst1 0x100524a0, against libtpw_unit_GenerateTrackSubdivisions_2.dylib (tests/unit_GenerateTrackSubdivisions_2.cpp), main app
("SimTheme Park"), src/TPConfig_extrude_GenerateTrackSubdivisions_2.cpp.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The host structs have
native pointers, so every field has a Mac offset and a host offset (the host's come from tpw_gts2_layout) and the params array has the
host's stride. The callees (LayoutRecordCoasterSpeed, FlagGenerateTrack, FlagRecalcAllCoasterCollisions, DistanceBetweenPoints,
StopFlashingObject, StartFlashingObject, CTRACE) are fakes on both sides that record their arguments (objects by an id word stored in them,
points by their float bits, CTRACE by its level, whether the format matched and the two double arguments' words).

The world of a case: an instance with its type, 6 braces (each with a spline block), 4 ride anim sets, the three globals, a params array
(keys and two +inf sentinels) and a 128-float output buffer. What is compared: the answer, the log, the instance's changed fields, the type's
floats and flags, the flashing brace's id, the output buffer.

Fused operations: fmadds in GenerateTrackSubdivisions and CoasterSetSpeed (HardwareRounding over the unit's functions). Not tested: an
instance without a type, a NULL brace link in GetBankAtBrace when the bank is nonzero (read unconditionally), params arrays without a
stopper (the walk runs on).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_GenerateTrackSubdivisions_2"
MASK = 0xFFFFFFFF

FN_GEN, FN_SPEED, FN_BANK, FN_FLASH, FN_DEC = 0x1004E8C0, 0x1004B94C, 0x1004B1F8, 0x10052158, 0x100524A0
SIZES = {FN_GEN: 260, FN_SPEED: 188, FN_BANK: 216, FN_FLASH: 248, FN_DEC: 156}
OWN = set(SIZES)

CALLEE_LAYOUT, CALLEE_FLAGGEN, CALLEE_RECALC, CALLEE_DIST = 0x100590A0, 0x10040F08, 0x10049078, 0x100C3198
CALLEE_STOP, CALLEE_START, CALLEE_CTRACE = 0x1006D430, 0x1006D2F8, 0x1006FCBC

ADDR = dict(gCoasterEditInstance=0x102B68B0, gExtrudeTOffset=0x102C167C, gRideAnimSets=0x102C79A4)

KEYS = ["i_type", "i_f30", "i_speed", "i_b3c", "i_last", "i_first", "i_preview", "i_b64", "i_flash", "i_id", "i_size",
        "t_flags", "t_f08", "t_f0c", "t_f10", "t_f4c", "t_f50", "t_size",
        "b_flags", "b_prev", "b_next", "b_x", "b_f94", "b_w70", "b_spl", "b_size",
        "s_flags", "s_heads", "s_size", "p_size", "b_y", "b_z"]
MAC = dict(i_type=4, i_f30=0x30, i_speed=0x34, i_b3c=0x3C, i_last=0x54, i_first=0x58, i_preview=0x60, i_b64=0x64, i_flash=0x144, i_id=0x0C,
           i_size=0x150, t_flags=0, t_f08=8, t_f0c=0xC, t_f10=0x10, t_f4c=0x4C, t_f50=0x50, t_size=0x400,
           b_flags=8, b_prev=0x64, b_next=0x68, b_x=0x4C, b_f94=0x94, b_w70=0x70, b_spl=0x90, b_size=0xB0,
           s_flags=4, s_heads=8, s_size=0xB0, p_size=16, b_y=0x50, b_z=0x54)
OUTN = 128


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ident(e, a, off):
    return e.u32(a + off) if a else 0


def stub_layout(e):
    e.cl.rec("LayoutRecordCoasterSpeed", (ident(e, e.r(3), 0x0C), e.r(4) & MASK))


def stub_flaggen(e):
    e.cl.rec("FlagGenerateTrack", (ident(e, e.r(3), 0x0C), s32(e.r(4))))


def stub_recalc(e):
    e.cl.rec("FlagRecalcAllCoasterCollisions", (ident(e, e.r(3), 0x0C),))


def stub_dist(e):
    a, b = e.r(3), e.r(4)
    bits = [canon_f(w) for w in struct.unpack(">3I", e.read(a, 12)) + struct.unpack(">3I", e.read(b, 12))]
    e.cl.rec("DistanceBetweenPoints", tuple(bits))
    e.set_f1(bitsf(e.cl.ret("DistanceBetweenPoints") & MASK))


def stub_stop(e):
    e.cl.rec("StopFlashingObject", (ident(e, e.r(3), 8),))


def stub_start(e):
    e.cl.rec("StartFlashingObject", (ident(e, e.r(3), 8),))


def stub_ctrace(e):
    fmt = e.cstr(e.r(4))
    matched = 1 if fmt == "Curviness %.2f - Straight Divisor %.2f\n" else 0
    e.cl.rec("CTRACE", (e.r(3) & MASK, matched, e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK, e.r(8) & MASK))


STUBS.internal(CALLEE_LAYOUT, stub_layout)
STUBS.internal(CALLEE_FLAGGEN, stub_flaggen)
STUBS.internal(CALLEE_RECALC, stub_recalc)
STUBS.internal(CALLEE_DIST, stub_dist)
STUBS.internal(CALLEE_STOP, stub_stop)
STUBS.internal(CALLEE_START, stub_start)
STUBS.internal(CALLEE_CTRACE, stub_ctrace)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.ptrsz))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    ptrsz = 4
    O = MAC

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

    def call(self, fn, args, fl=None):
        with ROUNDING(self.e):
            r3, _, f1 = T.call_ppc(self.e, fn, args)
        return r3, f1


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    ptrsz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = lib.hostoff
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gts2_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gts2_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_gts2_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    f = getattr(lib, p + "gen")
    f.restype, f.argtypes = C.c_int32, [C.c_uint64] * 4 + [C.c_float]
    f = getattr(lib, p + "speed")
    f.restype, f.argtypes = None, [C.c_uint64, C.c_uint32, C.c_int32]
    f = getattr(lib, p + "bank")
    f.restype, f.argtypes = C.c_float, [C.c_uint64, C.c_uint64, C.c_float]
    f = getattr(lib, p + "flash")
    f.restype, f.argtypes = None, [C.c_uint64, C.c_uint32]
    f = getattr(lib, p + "dec")
    f.restype, f.argtypes = C.c_int32, []
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert n == len(KEYS), n
    lib.hostoff = dict(zip(KEYS, list(out)[:n]))
    # the brace's point is read as three floats from mX4C
    assert lib.hostoff["b_y"] == lib.hostoff["b_x"] + 4 and lib.hostoff["b_z"] == lib.hostoff["b_x"] + 8, lib.hostoff
    lib.bound = True


def build(s, case):
    w = case["w"]
    O = s.O
    t = s.alloc(max(O["t_size"], 0x400))
    t_ = w["t"]
    s.words(t + O["t_flags"], [t_["flags"]])
    for k in ("f08", "f0c", "f10", "f4c", "f50"):
        s.words(t + O["t_" + k], [t_[k]])
    inst = s.alloc(max(O["i_size"], 0x150))
    s.words(inst + O["i_id"], [1])
    s.ptr(inst + O["i_type"], 0 if w["tnull"] else t)
    s.words(inst + O["i_f30"], [w["f30"]])
    s.words(inst + O["i_speed"], [w["speed"]])
    s.words(inst + O["i_b3c"], [w["b3c"]])
    braces = []
    for i, bw in enumerate(w["braces"]):
        b = s.alloc(max(O["b_size"], 0xB0))
        braces.append(b)
    for i, bw in enumerate(w["braces"]):
        b = braces[i]
        s.words(b, [100 + i])
        s.words(b + O["b_flags"], [bw["flags"]])
        s.words(b + O["b_f94"], [bw["f94"]])
        s.words(b + O["b_x"], bw["pt"])
        s.words(b + O["b_w70"], [bw["w70"]])
        s.ptr(b + O["b_prev"], braces[bw["prev"]])
        s.ptr(b + O["b_next"], braces[bw["next"]])
        spl = s.alloc(16)
        s.words(spl, [bw["spl"]])
        s.ptr(b + O["b_spl"], spl)
    for key, idx in (("i_last", w["last"]), ("i_first", w["first"]), ("i_preview", w["preview"]), ("i_b64", w["b64"]), ("i_flash", w["flash"])):
        s.ptr(inst + O[key], braces[idx] if idx >= 0 else 0)
    sets = s.g("gRideAnimSets")
    for i, fl in enumerate(w["sets"]):
        a = s.alloc(max(O["s_size"], 0xB0))
        s.words(a + O["s_flags"], [fl])
        s.words(a + O["s_heads"], [200 + i + 1])
        s.ptr(sets + (i + 1) * s.ptrsz, a)
    s.ptr(s.g("gCoasterEditInstance"), inst if w["edit"] else 0)
    s.words(s.g("gExtrudeTOffset"), [w["t0"]])
    keys = w["keys"]
    stride = O["p_size"]
    params = s.alloc(stride * (len(keys) + 2) + 16)
    for i, k in enumerate(keys):
        s.words(params + i * stride, [k])
    for i in range(2):
        s.words(params + (len(keys) + i) * stride, [0x7F800000])
    out = s.alloc(OUTN * 4)
    return dict(inst=inst, t=t, braces=braces, params=params, out=out)


def execute(s, case, objs):
    op = case["op"]
    a = case["args"]
    if op == "gen":
        r3, _ = s.call(FN_GEN, [objs["braces"][a[0]], objs["braces"][a[1]], objs["params"], objs["out"], T.F(bitsf(a[2]))]) if isinstance(s, Ppc) else (
            s.lib.tpw_gts2_gen(objs["braces"][a[0]], objs["braces"][a[1]], objs["params"], objs["out"], bitsf(a[2])), 0)
        return s32(r3)
    if op == "speed":
        if isinstance(s, Ppc):
            s.call(FN_SPEED, [objs["inst"], a[0] & MASK, a[1] & MASK])
        else:
            s.lib.tpw_gts2_speed(objs["inst"], a[0] & MASK, s32(a[1]))
        return 0
    if op == "bank":
        if isinstance(s, Ppc):
            _, f1 = s.call(FN_BANK, [objs["inst"], objs["braces"][a[0]], T.F(bitsf(a[1]))])
            return canon_f(fb(f1))
        return canon_f(fb(s.lib.tpw_gts2_bank(objs["inst"], objs["braces"][a[0]], bitsf(a[1]))))
    if op == "flash":
        if isinstance(s, Ppc):
            s.call(FN_FLASH, [objs["inst"], a[0] & MASK])
        else:
            s.lib.tpw_gts2_flash(objs["inst"], a[0] & MASK)
        return 0
    if isinstance(s, Ppc):
        r3, _ = s.call(FN_DEC, [])
        return s32(r3)
    return s32(s.lib.tpw_gts2_dec())


def observe(s, case, objs, answer):
    O = s.O
    inst, t = objs["inst"], objs["t"]
    flash = s.rptr(inst + O["i_flash"])
    return {"answer": answer, "log": s.log(),
            "f30": s.rd_word(inst + O["i_f30"]), "speed": canon_f(s.rd_word(inst + O["i_speed"])),
            "flash": s.rd_word(flash) if flash else 0,
            "type": [s.rd_word(t + O["t_flags"])] + [canon_f(s.rd_word(t + O["t_" + k])) for k in ("f08", "f0c", "f10", "f4c", "f50")],
            "out": [canon_f(x) for x in s.rd_words(objs["out"], OUTN)]}


def run(s, case):
    try:
        objs = build(s, case)
        return observe(s, case, objs, execute(s, case, objs))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def pos(r):
    k = r.random()
    if k < 0.1:
        return r.choice([0, 0x80000000, 0x3F800000])
    return fb(r.uniform(0.01, 20))


def world(r, op):
    w = {}
    w["t"] = dict(flags=r.choice([0, 8, 1, 9, 0xFFFFFFFF, r.getrandbits(32)]), f08=rf(r), f0c=rf(r), f10=r.choice([rf(r), pos(r)]), f4c=rf(r), f50=rf(r))
    w["tnull"] = 1 if (op == "dec" and r.random() < 0.15) else 0
    w["f30"] = r.getrandbits(32)
    w["speed"] = rf(r)
    w["b3c"] = r.choice([0, 1, 2, 3, r.getrandbits(32)])
    w["braces"] = [dict(flags=r.choice([0, 0x100, r.getrandbits(32)]), f94=r.choice([0, 0, fb(r.uniform(-3, 3)), rf(r)]),
                        pt=[fb(r.uniform(-100, 100)) for _ in range(3)], prev=r.randrange(6), next=r.randrange(6), w70=r.randrange(1, 5),
                        spl=r.choice([0, 1, 2, 3, 5, 11, 12])) for _ in range(6)]
    w["last"], w["first"], w["preview"], w["b64"] = [r.randrange(-1, 6) for _ in range(4)]
    w["flash"] = r.randrange(-1, 6)
    w["sets"] = [r.choice([0, 0x4000000, r.getrandbits(32)]) for _ in range(4)]
    w["edit"] = r.choice([0, 1, 1])
    w["t0"] = r.choice([0, 0, fb(r.uniform(-2, 2)), fb(r.uniform(-2, 2))])
    return w


def gen_world(r, c):
    w = c["w"]
    t0 = bitsf(w["t0"])
    span = r.choice([r.uniform(-3, 3), r.uniform(0.1, 4), 0.0, 1.0, 10.0])
    c["args"] = [r.randrange(6), None, fb(span)]
    c["args"][1] = c["args"][0] if r.random() < 0.5 else r.randrange(6)
    count = w["braces"][c["args"][1]]["spl"] if c["args"][0] == c["args"][1] else 11
    keys = []
    for _ in range(r.randrange(0, 25)):
        k = r.random()
        if k < 0.3 and count > 1:
            v = t0 + span * r.randrange(0, count + 1) / (count - 1)
        else:
            v = r.uniform(t0 - 1.5, t0 + span + 1.5) if span > 0 else r.uniform(t0 + span - 1.5, t0 + 1.5)
        keys.append(fb(v))
    if r.random() < 0.85:
        keys.sort(key=lambda b: bitsf(b))
    w["keys"] = keys


def base(r, op):
    w = world(r, op)
    c = {"op": op, "w": w, "queues": {"DistanceBetweenPoints": [pos(r), pos(r)]}}
    w["keys"] = [0x7F800000]
    if op == "gen":
        gen_world(r, c)
    elif op == "speed":
        c["args"] = [r.choice([0, 1, 100, 50, 0xFFFFFFFF, r.randrange(0, 200), r.getrandbits(32)]), r.choice([0, 1, -1, 2])]
    elif op == "bank":
        c["args"] = [r.randrange(6), rf(r)]
    elif op == "flash":
        c["args"] = [r.choice([0, 1, 2, 3, 4, 5, 0xFFFFFFFF, r.getrandbits(32)])]
    else:
        c["args"] = []
    return c


def make(op, seed, over):
    def edges():
        r = random.Random(seed)
        for ov in over:
            c = base(r, op)
            for k, v in ov.items():
                if k == "args":
                    c["args"] = v
                elif k in ("t",):
                    c["w"]["t"].update(v)
                elif k == "b0":
                    c["w"]["braces"][0].update(v)
                else:
                    c["w"][k] = v
            yield c
        for _ in range(12):
            yield base(r, op)

    def rnd(r):
        return base(r, op)
    return edges, rnd


F1 = 0x3F800000
EDGE = {
    "gen": [dict(args=[0, 0, F1], keys=[0x7F800000], t0=0), dict(args=[0, 1, F1], keys=[0, 0x3E000000, 0x3F000000, 0x3F800000, 0x7F800000], t0=0),
            dict(args=[0, 1, F1], keys=[0xBF800000, 0, 0x3DCCCCCD, 0x7F800000], t0=0), dict(args=[0, 1, 0], keys=[0, 0, 0x7F800000], t0=0),
            dict(args=[0, 1, F1], keys=[0x7FC00000, 0x7F800000], t0=0), dict(args=[2, 2, F1], keys=[0x7F800000], t0=0, braces=None)],
    "speed": [dict(args=[0, 0], t={"flags": 0}), dict(args=[100, 1], t={"flags": 8}), dict(args=[0xFFFFFFFF, 0], t={"flags": 0xF}), dict(args=[1, 2], t={"flags": 7})],
    "bank": [dict(args=[0, F1], b0={"f94": 0}), dict(args=[0, F1], b0={"f94": F1, "flags": 0}), dict(args=[0, F1], b0={"f94": F1, "flags": 0x100}),
             dict(args=[0, F1], b0={"f94": 0xBF800000, "flags": 0}), dict(args=[0, F1], b0={"f94": 0x80000000, "flags": 0}),
             dict(args=[0, F1], b0={"f94": 0x7FC00000, "flags": 0})],
    "flash": [dict(args=[m], flash=f, b3c=b) for m in (0, 1, 2, 3) for f in (-1, 0) for b in (0, 2)],
    "dec": [dict(edit=0), dict(edit=1, tnull=1), dict(edit=1, tnull=0)],
}
for ov in EDGE["gen"]:
    if ov.get("braces", 1) is None:
        del ov["braces"]

TESTS = []
for _name, _op, _seed in (("GenerateTrackSubdivisions", "gen", 11), ("CoasterSetSpeed", "speed", 12), ("GetBankAtBrace", "bank", 13),
                          ("FlashEndPylons", "flash", 14), ("DecCoasterConst1", "dec", 15)):
    _e, _r = make(_op, _seed, EDGE[_op])
    TESTS.append(FnTest(_name, ppc, host, edge=_e, rnd=_r))
