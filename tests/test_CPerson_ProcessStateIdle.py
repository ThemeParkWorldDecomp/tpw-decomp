"""Unit v5098, CPerson::ProcessStateIdle, against libtpw_unit_CPerson_ProcessStateIdle.dylib (tests/unit_CPerson_ProcessStateIdle.cpp), main app
("SimTheme Park"), src/game/people/PersonQueue.cpp:
  CPerson::ProcessStateIdle 0x1011baa8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the random generator, the animation,
litter, thoughts, sounds, the nearest-thing searches, the state change, the walks, the message centre, the thing ids, the assert, the arc tangent,
operator new and the array constructor) is a fake on both sides that records its arguments and answers from the case's queues.

The world of a case: the person block (cell bytes, id, the floats, the counters), the turn, the world and its map pointer, the thing array (eight
rows, each a block), the cell's list head, the message centre (present or absent), the sound bank, the balance value, the litter kind count and the
four "no thing" halfwords. What is compared: the event log, the person's written fields, the words of every block that changed, and whether the
message centre pointer changed.

No fused operations (the float to integer conversions are truncations in range). Not tested: thing ids past the eight rows, the entertainer's
row 0 when the row is empty (the original dereferences NULL), cells outside the map.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CPerson_ProcessStateIdle"
MASK = 0xFFFFFFFF

FN = 0x1011BAA8
SIZES = {FN: 2796}
OWN = set(SIZES)

ADDR = dict(
    gWorld=0x1037EC0C, gWorldMap=0x1037EC08, centre=0x1037F6C4, banks1=0x102F7C98, balance=0x102B4568 + 0xA0, kinds=0x102A0B20,
    noBin=0x102A0FF0, noFw=0x102A0FF2, noBalloon=0x102A0FF4, noDecision=0x102A0FF6, things=0x1034CBFC,
)
HOST_LAYOUT = [0x1D0, 0x44, 0x24, 0x1DA70C, 0x2D8]
ASSERT_TEXT = b"Thing has been allocated in ThingArray index 0 - shouldn't happen\n"
PERSON, ANALYSER, CENTRE, THING0 = 1, 2, 3, 10
PERSIST = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def hidden16(e, value):
    e.write(e.r(3), struct.pack(">H", value & 0xFFFF))


def stub(addr, fn):
    STUBS.internal(addr, fn)


def s_rand(e):
    e.cl.rec("rand", ())
    e.set_r(3, e.cl.ret("rand") & MASK)


def s_spot(e):
    e.cl.rec("spot", (e.cl.lab(e.r(3)), s32(e.r(4))))


def s_litter(e):
    base = e.u32(ADDR["gWorldMap"])
    e.cl.rec("litter", ((s32(e.r(3) - base)) // 0x44, s32(e.r(4))))


def s_event(e):
    e.cl.rec("event", (e.cl.lab(e.r(3) - 0x30), s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF))


def s_sound(e):
    e.cl.rec("sound", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def s_bin(e):
    e.cl.rec("bin", (e.cl.lab(e.r(4)), e.r(5) & MASK))
    hidden16(e, e.cl.ret("bin"))


def s_state(e):
    e.cl.rec("state", (e.cl.lab(e.r(3)), s32(e.r(4))))


def s_lower(e):
    e.cl.rec("lower", (e.cl.lab(e.r(3)), s32(e.r(4))))


def s_closed(e):
    e.cl.rec("closed", ())
    e.set_r(3, e.cl.ret("closed") & MASK)


def s_gates(e):
    e.cl.rec("gates", ())
    e.set_r(3, e.cl.ret("gates") & MASK)


def s_side(e):
    which = s32(e.r(5))
    e.cl.rec("side", (which,))
    hidden16(e, e.cl.ret("side:%d" % which))


def s_setdest(e):
    e.cl.rec("setdest", (e.cl.lab(e.r(3)), (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, e.cl.ret("setdest") & MASK)


def s_fw(e):
    e.cl.rec("fw", (e.cl.lab(e.r(4)),))
    hidden16(e, e.cl.ret("fw"))


def s_ent(e):
    e.cl.rec("ent", (e.cl.lab(e.r(4)),))
    hidden16(e, e.cl.ret("ent"))


def s_assert(e):
    text = e.cstr(e.r(4))
    text = text.encode("latin-1") if isinstance(text, str) else bytes(text)
    e.cl.rec("assert", (e.r(3) & 0xFF, sum(text)))


def s_analyser(e):
    e.cl.rec("analyser", ())
    e.set_r(3, e.u32(PERSIST["slot"]))


def s_near(e):
    e.cl.rec("near", (e.cl.lab(e.r(3)), s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF, s32(e.r(6))))
    e.set_r(3, e.cl.ret("near") & MASK)


def s_thingid(e):
    e.cl.rec("thingid", (e.cl.lab(e.r(4)),))
    e.write(e.r(3), e.read(e.r(4), 2))


def s_pop(e):
    e.cl.rec("pop", (e.cl.lab(e.r(3)),))


def s_post(e):
    m = e.r(4)
    e.cl.rec("post", (e.cl.lab(e.r(3)), e.u32(m + 4), struct.unpack(">H", e.read(m + 8, 2))[0], e.u32(m + 0xC)))
    e.set_r(3, 1)


def s_decide(e):
    p = e.r(3)
    e.cl.rec("decide", (e.cl.lab(p),))
    e.write(p + 0x1DC, struct.pack(">H", e.cl.ret("decide") & 0xFFFF))


def s_randdest(e):
    e.cl.rec("randdest", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("randdest") & MASK)


def s_arctan(e):
    e.cl.rec("arctan", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("arctan") & MASK)


def s_new(e):
    assert e.r(3) == 0x1D0, "operator new size"
    e.set_r(3, e.alloc(0x1D0))


def s_construct(e):
    assert (e.r(4), e.r(5)) is not None and e.r(6) == 0x10 and e.r(7) == 0x1D, "__construct_array arguments"


for _addr, _fn in (
    (0x1013B1E4, s_rand), (0x10117E64, s_spot), (0x10103010, s_litter), (0x1012F3D4, s_event), (0x10115B80, s_sound),
    (0x1011E9EC, s_bin), (0x10120354, s_state), (0x1011B384, s_lower), (0x1013FF7C, s_closed), (0x1013FFB4, s_gates),
    (0x10101B74, s_side), (0x10115400, s_setdest), (0x1011ED54, s_fw), (0x1011EBC4, s_ent), (0x1000DDE8, s_assert),
    (0x1013EBC8, s_analyser), (0x100E6C90, s_near), (0x1012E1F8, s_thingid), (0x1011B11C, s_pop), (0x1015256C, s_post),
    (0x10118430, s_decide), (0x10113870, s_randdest),
):
    stub(_addr, _fn)
STUBS.imp("LbMaths_ArcTan__Fll", s_arctan)
STUBS.imp("__nw__FUl", s_new)
STUBS.imp("__construct_array", s_construct)


def setup(runner):
    T.install_std_stubs(runner.emu)
    PERSIST["world"] = T.persist_alloc(runner.emu, 0x1E0000)
    PERSIST["slot"] = T.persist_alloc(runner.emu, 16)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def w8(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def w16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def w32(self, a, v):
        self.words(a, [v])

    def r16(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def r32(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def wptr(self, a, v):
        # a pointer slot: 4 bytes on the Mac, 8 on the host
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def make_objects(self, case):
        o = {PERSON: self.alloc(0x240), ANALYSER: self.alloc(0x40)}
        if case["centre"]:
            o[CENTRE] = self.alloc(0x1D0)
        for i in range(8):
            if i or case["thing0"]:
                o[THING0 + i] = self.alloc(0x240)
        return o


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.world = PERSIST["world"]
        self.objs = self.make_objects(case)
        self.L = e.cl = CL.PpcLog({addr: label for label, addr in self.objs.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name] if name != "slot" else PERSIST["slot"]

    def set_thing(self, i, ptr):
        self.w32(ADDR["things"] + 0x14 * i, ptr)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self):
        T.call_ppc(self.e, FN, [self.objs[PERSON]])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.world = lib.tpw_idle_world()
        self.objs = self.make_objects(case)
        CL.host_begin(lib, dict(self.objs), case["queues"])

    def done(self):
        pass

    def g(self, name):
        if name == "things":
            return 0
        return self.lib.tpw_idle_addr(name.encode())

    def set_thing(self, i, ptr):
        self.lib.tpw_idle_setthing(i, ptr)

    def alloc(self, n):
        return self.lib.tpw_idle_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self):
        self.lib.tpw_idle_run(self.objs[PERSON])


def bind(lib):
    if getattr(lib, "idle_bound", False):
        return
    lib.tpw_idle_addr.restype, lib.tpw_idle_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_idle_alloc.restype, lib.tpw_idle_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_idle_world.restype, lib.tpw_idle_world.argtypes = C.c_uint64, []
    lib.tpw_idle_setthing.restype, lib.tpw_idle_setthing.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_idle_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_idle_run.restype, lib.tpw_idle_run.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_idle_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.idle_bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    P = s.objs[PERSON]
    p = case["person"]
    s.w16(P + 0, p["id"])
    s.w8(P + 4, p["x"])
    s.w8(P + 6, p["y"])
    s.w32(P + 0x1C, 0)
    s.w32(P + 0x188, p["w188"])
    s.w32(P + 0x19C, p["f19c"])
    s.w32(P + 0x1B0, p["f1b0"])
    s.w32(P + 0x1B4, p["f1b4"])
    s.w32(P + 0x1BC, p["w1bc"])
    s.w8(P + 0x1C0, p["b1c0"])
    s.w16(P + 0x1DC, p["h1dc"])
    s.w32(P + 0x1FC, p["w1fc"])
    s.w32(P + 0x208, p["w208"])
    s.w32(P + 0x220, p["state"])
    s.wptr(s.g("gWorld"), s.world)
    s.wptr(s.g("gWorldMap"), s.world + 0x2D8)
    s.w32(s.world + 0x1DA70C, case["turn"])
    s.wptr(s.g("centre"), s.objs.get(CENTRE, 0))
    s.wptr(s.g("slot"), s.objs[ANALYSER])
    s.w32(s.g("banks1"), case["bank"])
    s.w32(s.g("balance"), case["balance"])
    s.w32(s.g("kinds"), case["kinds"])
    s.w16(s.g("noBin"), case["noBin"])
    s.w16(s.g("noFw"), case["noFw"])
    s.w16(s.g("noBalloon"), case["noBalloon"])
    s.w16(s.g("noDecision"), case["noDecision"])
    for i in range(8):
        s.set_thing(i, s.objs.get(THING0 + i, 0))
        if THING0 + i in s.objs:
            b = s.objs[THING0 + i]
            t = case["things"][i]
            s.w16(b + 0, i)
            s.w8(b + 2, t["kind"])
            s.w8(b + 4, t["x"])
            s.w8(b + 6, t["y"])
            s.w16(b + 0xA, t["next"])
            s.w32(b + 0x19C, t["w19c"])
            s.w32(b + 0x210, t["w210"])
    cell = p["x"] + p["y"] * 128 + 1
    s.w16(s.world + 0x2D8 + (cell - 1) * 0x44 + 0x24, case["head"])


def snapshot(s):
    out = []
    for label, a in sorted(s.objs.items()):
        size = 0x240 if label != ANALYSER and label != CENTRE else (0x40 if label == ANALYSER else 0x1D0)
        out.append((label, a, size, s.read(a, size)))
    return out


def changed(s, snap):
    res = []
    for label, a, size, before in snap:
        after = s.read(a, size)
        for off in range(0, size, 4):
            if before[off:off + 4] != after[off:off + 4]:
                res.append((label, off))
    return res


def observe(s, case, snap):
    P = s.objs[PERSON]
    centre = s.rptr(s.g("centre"))
    return {
        "log": s.log(),
        "angle": s.r32(P + 0x1C), "w188": s.r32(P + 0x188), "f19c": s.r32(P + 0x19C), "f1b0": s.r32(P + 0x1B0),
        "f1b4": s.r32(P + 0x1B4), "h1dc": s.r16(P + 0x1DC), "w1fc": s.r32(P + 0x1FC),
        "changed": changed(s, snap),
        "centre_new": centre != s.objs.get(CENTRE, 0),
        "centre_set": centre != 0,
    }


def run(s, case):
    try:
        build(s, case)
        snap = snapshot(s)
        s.call()
        return observe(s, case, snap)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def hv(r):
    """A float whose truncated low byte matters."""
    if r.random() < 0.35:
        n = r.choice([0, 1, 14, 15, 16, 79, 80, 81, 100, 101, 255, 256, 257, 356, 346, 89, 90, 91, -1, -200])
    else:
        n = r.randrange(0, 400)
    frac = r.choice([0.0, 0.5, 0.99])
    return fb(n + frac if n >= 0 else n - frac)


def r32(r):
    k = r.random()
    if k < 0.3:
        return r.randrange(0, 400)
    return r.randrange(0, 1 << 32)


def ids(r, n, avoid=0):
    out = []
    while len(out) < n:
        v = r.randrange(1, 8)
        if v != avoid or avoid == 0:
            out.append(v)
    return out


def make_case(r, **force):
    turn = r.randrange(0, 0x100000)
    no_bin = r.choice([0, 0, 0, r.randrange(1, 8)])
    no_fw = r.choice([0, 0, 0, r.randrange(1, 8)])
    no_dec = r.choice([0, 0, r.randrange(1, 0x100)])
    no_balloon = r.choice([0, 0, 9])
    d208 = r.choice([0x64, 0x65, 0, 5, 0x1000, 0x1000, -3, 0x66])
    d1fc = r.choice([0x1D, 0x1E, 0x1F, 0, 100, -5, 0x1000])
    chain = r.sample(range(1, 8), r.choice([0, 1, 2, 3, 4]))
    px, py = r.randrange(128), r.randrange(128)
    person = dict(
        id=r.choice([1, 2, 3, 4, 5, 6, 7, 0x155]), x=px, y=py, w188=r.choice([0, 1, 5]), f19c=hv(r), f1b0=hv(r), f1b4=hv(r),
        w1bc=r.choice([0, 1, 1, 7]), b1c0=r.choice([0, 1, 50, 99, 100, 101, 102, 103, 150, 255, r.randrange(256)]),
        h1dc=r.randrange(0x10000), w1fc=(turn - d1fc) & MASK, w208=(turn - d208) & MASK,
        state=r.choice([6, 7, 10, 0, 5, 8, 11, -1, 6, 7, 10]),
    )
    things = []
    for i in range(8):
        near = r.random() < 0.3
        things.append(dict(
            kind=r.choice([1, 1, 1, 0, 3]), x=px if near else r.randrange(128), y=py if near else r.randrange(128),
            next=no_balloon, w19c=r.choice([0xE, 0xE, 0xE, 3]), w210=r.choice([0, 1, 5]),
        ))
    for k, i in enumerate(chain):
        things[i]["next"] = chain[k + 1] if k + 1 < len(chain) else no_balloon

    def pick(none_id):
        if r.random() < 0.4:
            return none_id
        v = r.randrange(1, 8)
        return v if v != none_id else (v % 7) + 1

    queues = {
        "rand": [r32(r), r32(r), r32(r)],
        "bin": [pick(no_bin)], "fw": [pick(no_fw)], "ent": [r.randrange(1, 8)],
        "closed": [r.choice([0, 1])], "gates": [r.choice([0, 1])],
        "side:0": [r.randrange(0x4000)], "side:1": [r.randrange(0x4000)],
        "setdest": [r.choice([0, 0, 1]) for _ in range(r.randrange(1, 5))],
        "near": [r.choice([0, 1, 1])], "randdest": [r.choice([0, 1])],
        "decide": [no_dec if r.random() < 0.5 else r.randrange(0x10000)],
        "arctan": [r.choice([0, 1, 0x1234, 0x12345, -1, r.randrange(-100000, 100000)])],
    }
    c = dict(
        person=person, turn=turn, bank=r.randrange(0x10000), balance=r.choice([0, 1, 50, 100, 101, r.randrange(120)]),
        kinds=r.choice([6, 6, 6, 1, 0, 2, 7]), noBin=no_bin, noFw=no_fw, noBalloon=no_balloon, noDecision=no_dec,
        thing0=r.random() < 0.2, centre=r.random() < 0.7, things=things, head=chain[0] if chain else no_balloon, queues=queues,
    )
    for k, v in force.items():
        if k == "person":
            c["person"].update(v)
        elif k == "queues":
            c["queues"].update(v)
        else:
            c[k] = v
    return c


def random_case(r):
    return make_case(r)


def edges():
    r = random.Random(5098)
    F = fb
    overrides = []
    # early sulk: the turn gap and the happiness around their limits
    for gap, hap in ((0x65, 81), (0x64, 100), (0x65, 80), (0x65, 331), (0x1000, 0)):
        overrides.append(dict(person=dict(w208=0, f19c=F(float(hap)))))
        overrides[-1]["turn"] = gap
    # a bored kid: 100 and 101 and 100 + 256 with the random 0 mod 3, and not
    for b in (100, 101, 80, 356):
        for rd in (3, 4):
            overrides.append(dict(person=dict(f1b0=F(float(b)), w208=0), queues=dict(rand=[rd])))
    # the bin walk and the litter drop, with the kind counts
    for kinds in (6, 1, 0, 2):
        for st in (6, 7, 10, 9):
            for binans in (None, 0):
                q = dict(rand=[4, 1000003, 5])
                over = dict(person=dict(f1b4=F(90.0), state=st, f1b0=F(0.0), w208=0), kinds=kinds, queues=q)
                if binans is not None:
                    over["noBin"] = 0
                    over["queues"] = dict(q, bin=[0])
                overrides.append(over)
    # leaving the park: each reason, the walks that work and those that do not
    for hap, w1bc, closed in ((0.0, 1, 0), (50.0, 0, 0), (50.0, 1, 1)):
        for rd in (2, 3):
            for sd in ([0], [1], [0, 0, 1], [0, 0, 0, 1], [0, 0, 0, 0]):
                overrides.append(dict(person=dict(f19c=F(hap), w1bc=w1bc, f1b0=F(0.0), f1b4=F(0.0), w208=0),
                                      queues=dict(rand=[rd], closed=[closed], setdest=sd, **{"side:0": [0x10], "side:1": [0x22]})))
    # fireworks and the entertainer: the same cell, off by one, far
    for fwx, fwy in ((None, None), (1, 0), (0, 1), (1, -1)):
        o = dict(person=dict(f19c=F(50.0), w1bc=1, f1b0=F(0.0), f1b4=F(0.0), w208=0), queues=dict(closed=[0], fw=[3]), noFw=0)
        o["things"] = None
        o["_fw"] = (fwx, fwy)
        overrides.append(o)
    # the balloon branch, litter by chance, the sound, the thought, the happiness clamp
    for kind in (99, 100, 101, 102, 103):
        for hap in (0.0 + 1, 5.0, 14.9, 99.5, 99.0):
            for rd in (1, 3, 5, 6):
                overrides.append(dict(person=dict(b1c0=kind, f19c=F(hap), w1bc=1, f1b0=F(0.0), f1b4=F(0.0), w208=0),
                                      queues=dict(closed=[0], fw=[0], near=[0], rand=[rd]), noFw=0, balance=100))
    # the final pick: wander or decide, on both sides of the time limit
    for rd in (1, 3):
        for d in (0x1D, 0x1E, 0x1F):
            for rdest in (0, 1):
                for dec in (0, 5):
                    overrides.append(dict(person=dict(f19c=F(50.0), w1bc=1, f1b0=F(0.0), f1b4=F(0.0), w208=0, w1fc=(0x5000 - d) & MASK,
                                                      b1c0=0),
                                          turn=0x5000, noDecision=0, noFw=0,
                                          queues=dict(closed=[0], fw=[0], near=[0], rand=[rd], randdest=[rdest], decide=[dec])))
    for i, o in enumerate(overrides):
        fw = o.pop("_fw", None)
        c = make_case(r, **{k: v for k, v in o.items() if v is not None})
        if fw is not None:
            c["person"]["w208"] = c["turn"]
            if fw[0] is not None:
                px, py = c["person"]["x"], c["person"]["y"]
                c["things"][3]["x"] = (px + fw[0]) & 0x7F
                c["things"][3]["y"] = (py + fw[1]) & 0x7F
                c["things"][3]["w19c"] = 0xE
        yield c


TESTS = [
    FnTest("CPerson::ProcessStateIdle", ppc, host, edge=edges, rnd=random_case),
]
