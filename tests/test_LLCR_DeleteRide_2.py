"""Unit v5999, LLCR_DeleteRide, against libtpw_unit_LLCR_DeleteRide_2.dylib (tests/unit_LLCR_DeleteRide_2.cpp), main app
("SimTheme Park"), src/game/park/LLCellRide.cpp:
  LLCR_DeleteRide 0x100892c4 (2,576 bytes)

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit is a fake on both sides that records its arguments (pointers as codes: 1 the ride, 2 its record, 3 the bank account, 4 the track
cell, 5 the world, 100 + n the map cell n, 1000 + k a neighbour block) and answers from the case's queues: LLCC_SystemGetLastMode,
LLCR_OverRide (also moves the position), GAME_ASSERTMSG (the assert list), the ride's GetResidualValuePercentage and GetDBInfo, the click
stack and mode calls (which also log the two words gLLCC47E0 and gLLCC47E4 as they stand), the bank account, CMap::IsXYValid, the cell
predicates and neighbours, OPPOSITE and ADD_DIRECTION (a rotation of the byte, so the original's walks end), the cell and track cell
deletes, BMP_DestroyRide, KillMe and SOUND_SendEvent.

The world of a case: the thing table (a ride in some rows), the no-ride halfword, the ride (rotation, queue length, anim word), its record
(shape pointer, has-queue word, track type), the shape (a loaded file image: width, height, rows of 0xa0 bytes of eight-byte entries), three
neighbour cells (type and attribute byte), the world with its map cells, the sound banks, the action recorder's recording word and the two
words of the cell system. What is compared: the event log, the assert list and the two words afterwards.

No fused operations (the sound coordinates are single precision converts and a multiply). Not tested: a NULL neighbour for the cell beyond
an entrance or exit (the original reads its type unconditionally), a negative shape size, a coordinate outside 0..34 (the map is only
allocated up to there), a values above 2^31 / 10 for the sound coordinates (fctiwz saturation differs in the emulator).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCR_DeleteRide_2"
MASK = 0xFFFFFFFF

FN = 0x100892C4
SIZES = {FN: 2576}
OWN = set(SIZES)

ADDR = dict(e0=0x102E47E0, e4=0x102E47E4, things=0x1034CBFC, world=0x1037EBFC, recorder=0x1037EC44, banks=0x102F7C94, norid=0x1029E04E)
# The Mac's offsets
MAC = dict(thing_stride=0x14, obj_size=0x400, rot=0x10, f24=0x24, f3c=0x3c, info_size=0x1000, shape=0x18, hasq=0x40, track=0x9c,
           ctype=0x8, cstat=0x26, rec=0x24, ptr=4)
# the host's: [sizeof CThingArrayEntry, sizeof CObject, rot, f24, f3c, sizeof RideInfo, shape, hasq, track, ctype, cstat, rec, sizeof CMapCell, mBase]
HOST_LAYOUT = [48, 1096, 16, 36, 60, 2824, 48, 128, 312, 8, 38, 64, 68, 0]

CELLS = 4400


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def code(e, a):
    """A pointer as the small code the host fakes compute alike."""
    a &= MASK
    c = e.ctx
    if a == 0:
        return 0
    for name, v in (("obj", 1), ("info", 2), ("bank", 3), ("track", 4), ("world", 5)):
        if a == c[name]:
            return v
    off = a - c["cells"]
    if 0 <= off < 0x44 * CELLS and off % 0x44 == 0:
        return 100 + off // 0x44
    for k, b in enumerate(c["nbr"]):
        if a == b:
            return 100000 + k
    return -1


def rol8(v, n):
    v &= 0xFF
    return ((v << n) | (v >> (8 - n))) & 0xFF


def ew(e):
    """The two words of the cell system, as the fakes that log them read them."""
    return (s32(e.u32(ADDR["e0"])), s32(e.u32(ADDR["e4"])))


def flag(v):
    return 1 if v & 0xFF else 0


def stub(addr, name, conv, retkey=None, ret=None, words=False):
    """conv: one letter per argument register from r3 (p code, i s32, b byte, h halfword, f flag byte); retkey: a queue answering r3."""
    def fn(e):
        vals = []
        for i, k in enumerate(conv):
            r = e.r(3 + i)
            vals.append({"p": lambda: code(e, r), "i": lambda: s32(r), "b": lambda: r & 0xFF, "h": lambda: r & 0xFFFF,
                         "f": lambda: flag(r)}[k]())
        if words:
            vals.extend(ew(e))
        e.cl.rec(name, tuple(vals))
        if retkey is not None:
            e.set_r(3, e.cl.ret(retkey) & MASK)
        elif ret is not None:
            e.set_r(3, ret(e, vals) & MASK)
    STUBS.internal(addr, fn)


def stub_override(e):
    x, y = e.s32(e.r(4)), e.s32(e.r(5))
    L = e.cl
    L.rec("OverRide", (x, y))
    e.write(e.r(3), struct.pack(">H", L.ret("ovr") & 0xFFFF))
    e.write(e.r(4), struct.pack(">I", L.ret("ox") & MASK))
    e.write(e.r(5), struct.pack(">I", L.ret("oy") & MASK))


def stub_neighbour(e):
    c = code(e, e.r(3))
    e.cl.rec("GetNeighbouringCell", (c, e.r(4) & 0xFF))
    k = e.cl.ret("nbrA" if 100 <= c < 100000 else "nbrB")
    e.set_r(3, 0 if k == 0 else e.ctx["nbr"][k - 1])


def stub_sound(e):
    e.cl.rec("SendEvent", tuple(s32(e.r(3 + i)) for i in range(6)))
    e.set_r(3, 0)


STUBS.internal(0x1008905C, stub_override)
STUBS.internal(0x10103814, stub_neighbour)
STUBS.internal(0x100DD9FC, stub_sound)
stub(0x1009152C, "GetLastMode", "", retkey="mode")
stub(0x1010FD98, "Residual", "p", retkey="pct")


def stub_assert(e):
    same = e.cstr(e.r(4)) == "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"
    e.cl.rec("Assert", (e.r(3) & 0xFF, 1 if same else 0))


STUBS.internal(0x1000DDE8, stub_assert)
stub(0x10107CD4, "GetDBInfo", "p", ret=lambda e, v: e.ctx["info"])
stub(0x10092644, "ConstructQueue", "p", ret=lambda e, v: 0, words=True)
stub(0x10090E18, "SetMode", "if", words=True)
stub(0x100911F0, "SetModeNR", "if", words=True)
stub(0x10092280, "Backtrack", "pf", retkey="bt", words=True)
stub(0x10091BD8, "QueueCost", "", retkey="qcost")
stub(0x10091C54, "KartCost", "", retkey="kcost")
stub(0x10091CD4, "WaterCost", "", retkey="wcost")
stub(0x10091DD4, "ResetClickStack", "", words=True)
stub(0x10092E44, "ConstructTrack", "p", words=True)
stub(0x10093280, "ConstructReverse", "p", words=True)
stub(0x1013EDFC, "GetBank", "p", ret=lambda e, v: e.ctx["bank"])
stub(0x100F33B0, "Withdraw", "pi", words=True)
stub(0x101012F4, "IsXYValid", "ii", retkey="valid")
stub(0x1009C61C, "IsRide", "p", retkey="isride")
stub(0x1009C4DC, "IsEntrance", "p", retkey="isent")
stub(0x1009C524, "IsExit", "p", retkey="isexit")
stub(0x10081930, "GetDirection", "p", ret=lambda e, v: e.cl.ret("dir") & 0xFF)
stub(0x100816E8, "GetNeighbours", "p", ret=lambda e, v: e.cl.ret("nb") & 0xFF)
stub(0x10102500, "OPPOSITE", "b", ret=lambda e, v: rol8(v[0], 4))
stub(0x1010254C, "ADD_DIRECTION", "bb", ret=lambda e, v: rol8(v[0], 2))
stub(0x1009BB70, "IsFlag", "ph", retkey="isflag")
stub(0x1009BC18, "UnSetFlag", "ph")
stub(0x1009D0F4, "CellDelete", "pii")
stub(0x100F48C4, "GetTrackCellEquiv", "p", ret=lambda e, v: e.ctx["track"])
stub(0x100A29CC, "TrackUnSetFlag", "ph")
stub(0x100A40C4, "TrackDelete", "piif")
stub(0x10027274, "DestroyRide", "i")
stub(0x1012E728, "KillMe", "p")


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"
    off = MAC

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

    def putptr(self, a, v):
        self.words(a, [v])

    def set_ctx(self, obj, info, bank, track, world, nbr):
        self.e.ctx = dict(obj=obj, info=info, bank=bank, track=track, world=world, cells=world + 0x2D8, nbr=nbr)

    def log(self):
        return self.L.log

    def asserts(self):
        return [tuple(a) for a in self.e.asserts]

    def call(self, x, y, f):
        T.call_ppc(self.e, FN, [x & MASK, y & MASK, f & MASK])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lo = lib.layout
        self.off = dict(thing_stride=lo[0], obj_size=lo[1], rot=lo[2], f24=lo[3], f3c=lo[4], info_size=lo[5], shape=lo[6], hasq=lo[7],
                        track=lo[8], ctype=lo[9], cstat=lo[10], rec=lo[11], ptr=8)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_dr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def putptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def set_ctx(self, obj, info, bank, track, world, nbr):
        self.lib.tpw_dr_ctx(obj, info, bank, track, world, *nbr)

    def log(self):
        return CL.host_end(self.lib)

    def asserts(self):
        return [tuple(a) for a in T.host_asserts(self.lib)]

    def call(self, x, y, f):
        self.lib.tpw_dr_delete(s32(x), s32(y), s32(f))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_dr_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "ctx").argtypes = [C.c_uint64] * 8
    getattr(lib, p + "delete").argtypes = [C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.layout = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.off
    world = s.alloc(0x2D8 + 0x44 * CELLS)
    s.putptr(s.g("world"), world)
    obj = s.alloc(o["obj_size"])
    info = s.alloc(o["info_size"])
    bank = s.alloc(16)
    track = s.alloc(0x44)
    nbr = [s.alloc(0x44) for _ in range(3)]
    shape = s.alloc(8 + 0xA0 * 4 + 64)
    s.words(shape, [case["w"], case["h"]])
    for r in range(4):
        for c in range(4):
            s.words(shape + 8 + r * 0xA0 + c * 8, [case["grid"][r][c], 0])
    s.putptr(info + o["shape"], shape)
    s.words(info + o["hasq"], [case["hasq"]])
    s.words(info + o["track"], [case["tracktype"]])
    s.words(obj + o["rot"], [case["rot"]])
    s.words(obj + o["f24"], [case["f24"]])
    s.words(obj + o["f3c"], [case["f3c"]])
    for i in range(8):
        s.putptr(s.g("things") + i * o["thing_stride"], obj if case["slots"][i] else 0)
    s.half(s.g("norid"), case["norid"])
    s.words(s.g("recorder") + o["rec"], [case["rec"]])
    s.words(s.g("banks"), case["banks"])
    s.words(s.g("e0"), [case["e0"]])
    s.words(s.g("e4"), [case["e4"]])
    for k in range(3):
        s.words(nbr[k] + o["ctype"], [case["ntype"][k]])
        s.write(nbr[k] + o["cstat"], bytes([case["nstat"][k]]))
    s.set_ctx(obj, info, bank, track, world, nbr)


def execute(s, case):
    s.call(*case["args"])


def observe(s, case):
    return {"log": s.log(), "e0": s.rd_words(s.g("e0"), 1), "e4": s.rd_words(s.g("e4"), 1)}


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

PCTS = [0, 1, 50, 99, 100, 101, -1, -100, -7, 12345, 100000, 0x7FFFFFFF, -0x80000000]
COSTS = [0, 1, 99, 100, 101, 199, 200, -1, -99, -100, -101, 70000, 0x7FFFFFFF, -0x80000000, 2500, 123456]
ROTS = [0, 0x5A, 0xB4, 0x10E, 0x2D, 0x168, 0x5B, 0xB3, 0x10F]


def bits(r, n, p):
    return [1 if r.random() < p else 0 for _ in range(n)]


def gen(r, **kw):
    w, h = r.randrange(0, 5), r.randrange(0, 5)
    bt = [r.randrange(2) for _ in range(r.randrange(0, 4))] + [0]
    q = {
        "mode": [r.randrange(0, 0x3C)],
        "ovr": [r.randrange(0, 7)],
        "ox": [r.randrange(5, 31)],
        "oy": [r.randrange(5, 31)],
        "pct": [r.choice(PCTS)],
        "bt": bt,
        "qcost": [r.choice(COSTS)],
        "kcost": [r.choice(COSTS) & MASK],
        "wcost": [r.choice(COSTS) & MASK],
        "valid": bits(r, 40, 0.85),
        "isride": bits(r, 40, 0.5),
        "isent": bits(r, 40, 0.3),
        "isexit": bits(r, 40, 0.3),
        "isflag": bits(r, 40, 0.6),
        "dir": [r.choice([1 << r.randrange(8), r.randrange(256)]) for _ in range(20)],
        "nb": [r.randrange(256) for _ in range(40)],
        "nbrA": [r.randrange(1, 4) for _ in range(20)],
        "nbrB": [r.randrange(0, 4) for _ in range(40)],
    }
    c = {
        "queues": q, "w": w, "h": h, "grid": [bits(r, 4, 0.7) for _ in range(4)], "hasq": r.choice([0, 1, 1, 7]),
        "tracktype": r.choice([-1, 0, 1, 2, 3, 5]), "rot": r.choice(ROTS), "f24": r.choice([0, 0, 77, -3]),
        "f3c": r.choice([-2, 0, 1, 5]), "slots": bits(r, 8, 0.8), "norid": r.choice([0, 0, 3, 0x1008]),
        "rec": r.choice([0, 0, 1, 5]), "banks": [r.randrange(1 << 32) for _ in range(8)], "e0": r.choice([0, 1, 9]),
        "e4": r.choice([0, 1, 9]), "ntype": [r.choice([1, 1, 2, 7]) for _ in range(3)], "nstat": [r.randrange(256) for _ in range(3)],
        "args": [r.choice([0, 1, -1, r.randrange(-50, 50), r.randrange(-100000, 100000)]), r.choice([0, 1, -1, r.randrange(-50, 50)]),
                 r.choice([0, 1, 2, -1])],
    }
    c["ovr_id"] = q["ovr"][0]
    # make the early outs rarer: the ride's row is usually the one the table holds
    if r.random() < 0.85 and q["ovr"][0] == c["norid"]:
        q["ovr"] = [r.randrange(1, 7)]
        if c["norid"] == q["ovr"][0]:
            c["norid"] = 0
    for k, v in kw.items():
        if k.startswith("q_"):
            q[k[2:]] = v
        else:
            c[k] = v
    return c


def edges():
    r = random.Random(5999)
    for rot in ROTS:
        for f in (0, 1):
            for tt in (-1, 0, 1, 2, 3):
                for hq in (0, 1):
                    yield gen(r, rot=rot, args=[r.randrange(0, 20), r.randrange(0, 20), f], tracktype=tt, hasq=hq, f3c=r.choice([0, 3]),
                              slots=[0, 1, 1, 1, 1, 1, 1, 1], rec=0, q_ovr=[2], norid=0)
    for p in PCTS:
        for cst in COSTS:
            yield gen(r, hasq=1, f3c=5, tracktype=r.choice([0, 1, 2]), q_pct=[p], q_qcost=[cst], q_kcost=[cst & MASK], q_wcost=[cst & MASK],
                      slots=[0, 1, 1, 1, 1, 1, 1, 1], q_ovr=[1], norid=0)
    # every ride-count band of the sound, with every cell valid, a ride and a full shape
    for k in (0, 1, 3, 4, 5, 7, 8, 9, 15, 16):
        grid = [[1 if 4 * rr + cc < k else 0 for cc in range(4)] for rr in range(4)]
        yield gen(r, w=4, h=4, grid=grid, q_valid=[1], q_isride=[1], q_isent=[0], q_isexit=[0], rec=0, hasq=0, slots=[0, 1, 1, 1, 1, 1, 1, 1],
                  q_ovr=[3], norid=0, args=[1, 2, 1 if k % 2 else 0])
    # a shape without rows or columns: the sound reads the argument itself or the other size
    for w, h in ((0, 0), (3, 0), (0, 3), (1, 1)):
        for x in (0, -5, 7, 16777217, -16777217, 100000):
            yield gen(r, w=w, h=h, args=[x, 4, 1], rec=0, slots=[0, 1, 1, 1, 1, 1, 1, 1], q_ovr=[2], norid=0, hasq=r.choice([0, 1]))
    # the early outs: no ride row, an empty slot, a ride in row 0
    yield gen(r, q_ovr=[0], norid=0, slots=[0, 1, 1, 1, 1, 1, 1, 1])
    yield gen(r, q_ovr=[0], norid=0, slots=[1, 1, 1, 1, 1, 1, 1, 1])
    yield gen(r, q_ovr=[4], norid=4)
    yield gen(r, q_ovr=[4], norid=0, slots=[1, 1, 1, 1, 0, 1, 1, 1])
    # an entrance and exit walk in every shape: the attribute bit 8, the neighbour counts
    for st in (0, 8, 0x20):
        for nbtype in (1, 2):
            yield gen(r, w=2, h=2, grid=[[1, 1, 0, 0], [1, 1, 0, 0], [0] * 4, [0] * 4], q_valid=[1], q_isent=[1, 0], q_isexit=[0, 1],
                      q_isflag=[1], ntype=[nbtype] * 3, nstat=[st] * 3, rec=1, q_ovr=[2], norid=0, slots=[0, 1, 1, 1, 1, 1, 1, 1])


def rnd(r):
    return gen(r)


TESTS = [
    FnTest("LLCR_DeleteRide", ppc, host, edge=edges, rnd=rnd),
]
