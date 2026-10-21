"""Unit v5974: CPeep::UpdatePositionInEngine and CPeep::PeepWalks, against libtpw_unit_CPeep_UpdatePositionInEngine.dylib
(tests/unit_CPeep_UpdatePositionInEngine.cpp), main app ("SimTheme Park"), src/game/people/Peep.cpp:
  CPeep::UpdatePositionInEngine 0x101149cc, CPeep::PeepWalks 0x10114fa8

A case is a peep block and a call, built twice by one piece of Python code (emulator memory and host memory). The callees outside the unit
(Pilot::get_pos, move, is_finished_path, CThing::Move, GetThingId, IsStranded, SetStranded, DrawBalloon, IsCarrying, SetSpriteScript,
SetServiceInterval, SCR_SetScriptPosition, DC_AddSquareFaceToList, ESP_RURunningThis, LbMaths_ArcTan) are fakes on both sides that record
their arguments (objects as the offset from the peep block) and answer from the case's queues. sqrt is the real one on both sides.

The world: the peep block (0x240 bytes: kind, cell bytes, script handles, last position, strand stamp), the Peep.cpp statics with their
guard bytes, the balloon offset table, the sprite halfword. Compared: the answer, the log, the peep block, every static.

Fused operations (fmadds) and fctiwz go through ROUNDING. Not tested: positions whose fctiwz would overflow (Unicorn saturates wrongly).

A last test (unit v5L39), "the balloon offsets": the host's gPeepBalloonOffsets as the library was loaded against the program image's
words (tests/imagedata.py), one case per row, the floats compared as bits. The cases above write their own rows; before v5L39 the host's
rows were zero, so balloons did not bob.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import imagedata
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_CPeep_UpdatePositionInEngine"
MASK = 0xFFFFFFFF

FN_UPD, FN_WALK = 0x101149CC, 0x10114FA8
SIZES = {FN_UPD: 1448, FN_WALK: 636}
OWN = set(SIZES)

ADDR = dict(g490=0x1034C490, idx=0x1034C494, g488=0x1034C488, time=0x1034C48C, g480=0x1034C480, rate=0x1034C484,
            g478=0x1034C478, scale=0x1034C47C, g470=0x1034C470, c474=0x1034C474, g468=0x1034C468, divisor=0x1034C46C,
            g460=0x1034C460, lag=0x1034C464, height=0x102A0D98, base=0x102A0D9C, rows=0x102A0DA0, offsets=0x102A0DA4,
            c498=0x1034C498, table=0x102A0D54)
HOST_LAYOUT = [2, 240]
PEEP_SIZE = 0x240
PEEP = [0]


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


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def off(e):
    return s32(e.r(3) - PEEP[0])


def st_get_pos(e):
    L = e.cl
    L.rec("get_pos", (off(e),))
    e.write(e.r(4), struct.pack(">I", L.ret("posx") & MASK))
    e.write(e.r(5), struct.pack(">I", L.ret("posy") & MASK))


def st_move(e):
    e.cl.rec("move", (off(e),))


def st_isfin(e):
    e.cl.rec("is_finished_path", (off(e),))
    e.set_r(3, e.cl.ret("isfin") & MASK)


def st_Move(e):
    e.cl.rec("Move", (off(e), (e.r(4) >> 16) & 0xFFFF, (e.r(5) >> 16) & 0xFFFF))


def st_GetThingId(e):
    L = e.cl
    L.rec("GetThingId", (s32(e.r(4) - PEEP[0]),))
    e.write(e.r(3), struct.pack(">H", L.ret("thingid") & 0xFFFF))


def st_IsStranded(e):
    e.cl.rec("IsStranded", (off(e),))
    e.set_r(3, e.cl.ret("IsStranded") & MASK)


def st_SetStranded(e):
    e.cl.rec("SetStranded", (off(e), s32(e.r(4))))


def st_DrawBalloon(e):
    e.cl.rec("DrawBalloon", (off(e), canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), canon_f(fb(e.f(3))), s32(e.r(7))))


def st_IsCarrying(e):
    e.cl.rec("IsCarrying", (off(e),))
    e.set_r(3, e.cl.ret("IsCarrying") & MASK)


def st_SetSpriteScript(e):
    e.cl.rec("SetSpriteScript", (off(e), s32(e.r(4))))


def st_SetServiceInterval(e):
    e.cl.rec("SetServiceInterval", (off(e), canon_f(fb(e.f(1)))))


def st_SCR(e):
    e.cl.rec("SCR_SetScriptPosition", (s32(e.r(3)), canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), canon_f(fb(e.f(3))), s32(e.r(7))))


def st_DC(e):
    sp = e.r(1)
    e.cl.rec("DC_AddSquareFaceToList", (canon_f(fb(e.f(1))), canon_f(fb(e.f(2))), canon_f(fb(e.f(3))), e.r(6) & 0xFF, s32(e.r(7)),
                                        s32(e.r(8)), canon_f(fb(e.f(4))), s32(e.r(10)), e.u32(sp + 0x38) & 0xFF, s32(e.u32(sp + 0x3C)),
                                        s32(e.u32(sp + 0x40))))


def st_ESP(e):
    e.cl.rec("ESP_RURunningThis", (s32(e.r(3)), e.r(4)))
    e.set_r(3, e.cl.ret("ESP") & MASK)


def st_ArcTan(e):
    e.cl.rec("LbMaths_ArcTan", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("ArcTan") & MASK)


for addr, fn in ((0x10134888, st_get_pos), (0x10134844, st_move), (0x101348C4, st_isfin), (0x1012E5B4, st_Move),
                 (0x1012E1F8, st_GetThingId), (0x1011566C, st_IsStranded), (0x101155BC, st_SetStranded),
                 (0x1011B084, st_DrawBalloon), (0x100FE300, st_IsCarrying), (0x100FAA74, st_SetSpriteScript),
                 (0x100FAD70, st_SetServiceInterval), (0x100D932C, st_SCR), (0x100E02A0, st_DC), (0x100DA46C, st_ESP),
                 ):
    STUBS.internal(addr, fn)
STUBS.imp("LbMaths_ArcTan__Fll", st_ArcTan)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, size, v):
        self.write(a, struct.pack(self.fmt[0] + {1: "B", 2: "H", 4: "I"}[size], v & ((1 << (8 * size)) - 1)))

    def get(self, a, size):
        return struct.unpack(self.fmt[0] + {1: "B", 2: "H", 4: "I"}[size], self.read(a, size))[0]

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

    def upd(self, peep, step):
        with ROUNDING(self.e):
            T.call_ppc(self.e, FN_UPD, [peep, T.F(bitsf(step))])
        return 0

    def walk(self, peep):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN_WALK, [peep])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_peep_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_peep_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def upd(self, peep, step):
        self.lib.tpw_peep_base(peep)
        self.lib.tpw_peep_update(peep, bitsf(step))
        return 0

    def walk(self, peep):
        self.lib.tpw_peep_base(peep)
        return self.lib.tpw_peep_walks(peep) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_peep_addr.restype, lib.tpw_peep_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_peep_alloc.restype, lib.tpw_peep_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_peep_base.argtypes = [C.c_uint64]
    lib.tpw_peep_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_peep_update.argtypes = [C.c_uint64, C.c_float]
    lib.tpw_peep_walks.restype, lib.tpw_peep_walks.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_peep_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

STATIC_WORDS = ["idx", "time", "rate", "scale", "c474", "divisor", "lag", "height", "base", "rows", "c498"]
GUARDS = ["g490", "g488", "g480", "g478", "g470", "g468", "g460"]


def build(s, case):
    p = s.alloc(PEEP_SIZE)
    s.write(p, bytes(PEEP_SIZE))
    PEEP[0] = p
    case["peep_addr"] = p
    pe = case["peep"]
    for o, (size, v) in pe.items():
        s.put(p + o, size, v)
    for name in GUARDS:
        s.put(s.g(name), 1, case["g"][name])
    for name in STATIC_WORDS:
        s.put(s.g(name), 4, case["g"][name])
    s.words(s.g("offsets"), case["g"]["offsets"])
    s.put(s.g("table"), 2, case["g"]["table"])
    return p


def observe(s, case, p, answer):
    out = {"answer": answer, "log": s.log(), "peep": {o: s.get(p + o, sz) for o, (sz, _) in sorted(case["peep"].items())}}
    out["peep_1c"] = s.get(p + 0x1C, 4)
    out["peep_rest"] = [s.get(p + o, 4) for o in range(0x30, PEEP_SIZE, 4) if not any(o <= k < o + 4 for k in case["peep"])]
    for name in GUARDS:
        out[name] = s.get(s.g(name), 1)
    for name in STATIC_WORDS:
        out[name] = canon_f(s.get(s.g(name), 4)) if name in ("time", "rate", "scale", "c474", "divisor", "lag", "height", "base") else s.get(s.g(name), 4)
    out["offsets"] = [canon_f(w) for w in s.rd_words(s.g("offsets"), 60)]
    return out


def run(s, case):
    try:
        p = build(s, case)
        if case["op"] == "upd":
            ans = s.upd(p, case["step"])
        else:
            ans = s.walk(p)
        return observe(s, case, p, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x40000000, 0x3EB33333, 0x3F000000]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-1.5, 3))
    if k < 0.9:
        return fb(r.uniform(-0.5, 1.5))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 3))


def small(r):
    return r.choice([0, 1, -1, r.randint(-5000, 5000), r.randint(-100000000, 100000000), r.randint(-300, 300)])


def base(r, op):
    last190, last194 = small(r), small(r)
    pe = {
        2: (1, r.choice([1, 1, 7, 7, 3, 0, 5])),
        4: (1, r.randrange(256)),
        6: (1, r.randrange(256)),
        0xC: (4, r.choice([0, 5, r.randrange(1, 100)])),
        0x1C: (4, r.choice([0, 0x80, r.randrange(0, 2048), r.getrandbits(32)])),
        0x28: (4, r.choice([0, 0, 1, 7])),
        0xB4: (4, r.choice([0, 0, 9, r.randrange(1, 100)])),
        0xC2: (2, r.choice([0, 1, 5, 0xFFFF, r.randrange(0x10000)])),
        0x18C: (4, r.choice([0, 0, 0, 1, 77])),
        0x190: (4, last190),
        0x194: (4, last194),
        0x210: (4, r.choice([0, 1, 1, 5])),
    }
    g = {name: r.choice([0, 0, 1, 0xFF, 0x80]) for name in GUARDS}
    g.update(
        idx=r.choice([0, 1, 5, 19, 20, 21, 0xFFFFFFFF, r.getrandbits(32)]),
        time=r.choice([0, fb(0.5), fb(1.0), 0x3F800001, 0x3F7FFFFF, 0x7FC00000, fb(2.0), fb(-1.0), rf(r)]),
        rate=rf(r), scale=rf(r), c474=rf(r), divisor=r.choice([fb(0.5), rf(r), 0]), lag=r.choice([fb(0.35), rf(r)]),
        height=r.choice([fb(2.5), rf(r)]), base=r.choice([fb(1.0), rf(r)]),
        rows=r.choice([20, 20, 1, 3, 7, r.randrange(1, 21)]), c498=r.choice([0x10000, 0x10000, r.randrange(0, 0x20000)]),
        offsets=[rf(r) for _ in range(60)], table=r.choice([0, 1, 5, 0x8000, 0xFFFF, r.randrange(0x10000)]),
    )
    if r.random() < 0.3:
        g["idx"] = r.randrange(0, 25)
    c = {"op": op, "peep": pe, "g": g}
    q = {
        "posx": [last190 + small(r) % 3000 - 1500 if r.random() < 0.8 else small(r), last190 + r.randint(-3000, 3000)],
        "posy": [last194 + r.randint(-3000, 3000), last194 + r.randint(-3000, 3000)],
        "IsStranded": [r.choice([0, 1])],
        "thingid": [r.choice([0, 1, 19, 20, r.randrange(0x10000)])],
        "isfin": [r.choice([0x10000, 0x10000, 0, r.randrange(0, 0x20000)])],
        "ArcTan": [r.choice([0, 0x7FF, 0x800, 0x12345, -5 & MASK, r.getrandbits(32)])],
        "IsCarrying": [r.choice([0, 1])],
        "ESP": [r.choice([0, 1])],
    }
    if op == "walk":
        q["isfin"] = [r.choice([0x10000, 0x10000, 0x10000 + 5, r.randrange(0, 0x20000)])]
        pe[0x18C] = (4, r.choice([0, 0, 0, 0, 1]))
        g["c498"] = r.choice([0x10000, 0x10000, 0x10000, 0x7])
        q["posx"] = [small(r), small(r)]
        q["posy"] = [small(r), small(r)]
        if r.random() < 0.5:
            q["posx"] = [r.randint(-100000, 100000), r.randint(-100000, 100000)]
            q["posy"] = [r.randint(-100000, 100000), r.randint(-100000, 100000)]
    if op == "walk" and r.random() < 0.7:
        g["table"] = r.choice([0, 1, 5, 0xFFFF])
        pe[0xC2] = (2, r.choice([0, 1, 5, 6, 0xFFFF]))
    c["queues"] = q
    c["step"] = rf(r)
    return c


def upd_random(r):
    return base(r, "upd")


def walk_random(r):
    return base(r, "walk")


def upd_edges():
    r = random.Random(5974)
    for step in (0, 0x3F800000, 0x80000000, 0xBF800000, 0x40000000, 0x3EB33333, 0x3F000000, 0x7FC00000, 0x40100000, 0xC0000000, fb(0.35 + 2.0),
                 fb(0.35 - 1.0), fb(1.5), fb(0.7), 0x3F800001, 0x3F7FFFFF):
        for kind in (1, 3):
            c = base(r, "upd")
            c["step"] = step
            c["peep"][2] = (1, kind)
            c["peep"][0x210] = (4, 1)
            for name in GUARDS:
                c["g"][name] = 0
            yield c
    for k in range(40):
        c = base(r, "upd")
        c["peep"][2] = (1, 1)
        c["peep"][0x210] = (4, 1)
        c["peep"][0x28] = (4, k & 1)
        c["peep"][0xB4] = (4, (k >> 1) & 1)
        c["queues"]["IsStranded"] = [(k >> 2) & 1]
        c["g"]["time"] = (0, fb(1.0), 0x3F7FFFFF, 0x3F800001, fb(0.25), 0x7FC00000, fb(3.0), 0x80000000)[k % 8]
        c["g"]["rows"] = (20, 1, 20, 7)[(k >> 3) % 4]
        c["peep"][4] = (1, (0, 127, 255, 3, 128)[k % 5])
        c["peep"][6] = (1, (0, 255, 1, 127)[k % 4])
        yield c
    c = base(r, "upd")                                    # a cell of 0: (cell - 1) wraps
    c["peep"][4] = (1, 0)
    c["peep"][6] = (1, 0)
    c["queues"]["IsStranded"] = [1]
    yield c


def walk_edges():
    r = random.Random(5975)
    for k in range(64):
        c = base(r, "walk")
        c["peep"][0x18C] = (4, 1 if k == 0 else 0)
        c["queues"]["isfin"] = [0x10000 if k == 1 else 0x10001]
        c["g"]["c498"] = 0x10000
        c["peep"][2] = (1, (1, 7, 3, 7, 1, 0, 7, 5)[k % 8])
        c["peep"][0xC2] = (2, (5, 6, 5, 6, 0, 0xFFFF, 4, 6)[(k >> 1) % 8])
        c["g"]["table"] = 5
        c["queues"]["IsCarrying"] = [(k >> 3) & 1]
        c["queues"]["ESP"] = [(k >> 4) & 1]
        c["queues"]["ArcTan"] = [(0, 0x12345678, 0xFFFF)[k % 3]]
        yield c


TESTS = [
    FnTest("CPeep::UpdatePositionInEngine", ppc, host, edge=upd_edges, rnd=upd_random),
    FnTest("CPeep::PeepWalks", ppc, host, edge=walk_edges, rnd=walk_random),
    imagedata.table_test("the balloon offsets", [("offsets", ADDR["offsets"], "III", 20)]),
]
