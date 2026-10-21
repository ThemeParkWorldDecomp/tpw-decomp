"""Unit v5627, one step of a bump car (BMP_Move), against libtpw_unit_BMP_Move.dylib (tests/unit_BMP_Move.cpp), main app
("SimTheme Park"), src/game/rides/BumperRide.cpp:
  BMP_Move 0x10029b9c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers where the host headers have them) for the
translation; the same code reads both back. The callees outside the unit are fakes on both sides that record their arguments and answer
from the case's queues: PAR_InitEmitter, PossitionSound1, TrackRideIdToThingId, GAME_ASSERTMSG, RSSE_GetSoundEventCode,
SOUND_SetParamValue, GetRideFromID, GetDataNodeWorldMatrix, NormaliseVector, PAR_InitEmitterLaunch, PAR_SetEmitterPos,
FlushDeferredAnim, SetCurRideAnim, BMP_MaskCar, GetCurrentAnim, GetDeferredAnim; the imports labs and atan2 compute on both sides,
sqrt is install_std_stubs' on the PowerPC side.

The world of a case: the bump (0xa8 bytes), its owner (a ride record in a block of four), the collision object and its trigger, four
ride anim sets (gRideAnimSets 0..3) each with its mesh, model header, node and head table of four entries and head nodes, the list at
0x102b56c0 (magic, tick, rides, seed), gAdvisorThingId, gThingArray rows 0..7 and their things, gBumpTurnMax and gBumpTurnMin, the
matrix GetDataNodeWorldMatrix answers. What is compared: the whole bump (its two Ptr32 fields as "unchanged"), the trigger, the list's
words, the event log.

The fused operation (fnmsub on the atan2 result) is double precision and stays below 512: no rounding hook. Not tested: a trigger whose
anim set index is 0 on the paths that read the anim kind (the original then reads an unset stack slot); a sum of squared velocities
past 2^31.
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_Move"
MASK = 0xFFFFFFFF

FN = 0x10029B9C
# The jump table at 0x1029cb38 (kind + 14): -14 -11 -6 -3 -1 steer by velocity (0x10029f1c), -13 -12 -8 -5 by push (0x10029ce0),
# -10 -9 -7 -4 drive (0x1002a3c0), -2 the default (0x1002a9f4); the edges run every kind from -15 to 0.

ADDR = dict(gColList=0x102B56C0, gAdvisorThingId=0x1029C960, gThingArray=0x1034CBFC, gRideAnimSets=0x102C79A4,
            gBumpTurnMax=0x1029CB30, gBumpTurnMin=0x1029CB34)

KEYS = ["set_heads", "mesh_header", "mesh_f28", "hdr_f78", "table_entries", "entry_size", "entry_node", "thing_size", "thing_ptr",
        "ptr", "head_x", "node_matrix", "bump_size"]
MAC_LAY = dict(zip(KEYS, [0x08, 0x04, 0x28, 0x78, 0x04, 0x14, 0x04, 0x14, 0x00, 4, 0x30, 0x10, 0xa8]))
HOST_LAYOUT = [0x10, 0x08, 0x48, 0x78, 0x08, 0x20, 0x08, 0x30, 0x00, 8, 0x30, 0x10, 0xa8]
HOST_LAY = dict(zip(KEYS, HOST_LAYOUT))

# the case block (offsets within one allocation)
BLK = 0x1800
O_BUMP, O_RIDES, O_COL, O_TRIG, O_MAT, O_SET, O_THING = 0x0, 0x100, 0x480, 0x4c0, 0x500, 0x600, 0x1600
S_MESH, S_HDR, S_NODE, S_TABLE, S_ENTRIES, S_HEADS = 0x20, 0x80, 0x100, 0x180, 0x1a0, 0x240

CUR = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


def tdiv(a, b):
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b > 0) else -q


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.add(FN)


def rec(e, name, ints):
    e.cl.rec(name, tuple(ints))


def ret(e, key):
    return e.cl.ret(key) & MASK


def st_par_init(e):
    rec(e, "PAR_InitEmitter", [s32(e.r(i)) for i in (3, 4, 5, 6)])
    e.set_r(3, ret(e, "PAR_InitEmitter"))


def st_poss(e):
    rec(e, "PossitionSound1", [e.cl.lab(e.r(3))])


def st_track(e):
    rec(e, "TrackRideIdToThingId", [s32(e.r(4))])
    e.write(e.r(3), struct.pack(">H", ret(e, "Thing") & 0xFFFF))


def st_code(e):
    rec(e, "RSSE_GetSoundEventCode", [s32(e.r(3)), s32(e.r(4))])
    e.set_r(3, ret(e, "Code"))


def st_snd(e):
    rec(e, "SOUND_SetParamValue", [s32(e.r(3)), e.r(4) & MASK, e.r(5) & MASK])
    e.set_r(3, ret(e, "Snd"))


def st_ride(e):
    rec(e, "GetRideFromID", [e.u32(e.r(3))])
    e.set_r(3, ADDR["gRideAnimSets"] + 4 * ret(e, "Ride"))


def st_matrix(e):
    rec(e, "GetDataNodeWorldMatrix", [e.cl.lab(e.u32(e.r(3))), e.u32(e.r(4))])
    e.set_r(3, CUR["matrix"] if ret(e, "Mat") else 0)


def st_norm(e):
    a = e.r(3)
    rec(e, "NormaliseVector", [e.u32(a), e.u32(a + 4), e.u32(a + 8)])
    e.write(a, struct.pack(">3I", ret(e, "NormX"), ret(e, "NormY"), ret(e, "NormZ")))


def st_launch(e):
    rec(e, "PAR_InitEmitterLaunch", [s32(e.r(i)) for i in range(3, 10)])
    e.set_r(3, ret(e, "Launch"))


def st_pos(e):
    rec(e, "PAR_SetEmitterPos", [s32(e.r(i)) for i in (3, 4, 5, 6)])


def st_flush(e):
    rec(e, "FlushDeferredAnim", [e.cl.lab(e.r(3)), e.r(4) & MASK])


def st_setcur(e):
    rec(e, "SetCurRideAnim", [e.cl.lab(e.r(3)), e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, fb(e.f(1)), e.r(8) & MASK])


def st_mask(e):
    rec(e, "BMP_MaskCar", [e.cl.lab(e.r(3)), s32(e.r(4))])


def anim_query(name, kind, idx):
    def f(e):
        rec(e, name, [e.cl.lab(e.r(3)), e.r(6) & MASK])
        e.write(e.r(4), struct.pack(">I", ret(e, kind)))
        e.write(e.r(5), struct.pack(">I", ret(e, idx)))
    return f


def st_labs(e):
    v = s32(e.r(3))
    e.set_r(3, abs(v) & MASK)


def st_atan2(e):
    e.set_f1(math.atan2(e.f(1), e.f(2)))


STUBS.internal(0x100BEC30, st_par_init)
STUBS.internal(0x1002B248, st_poss)
STUBS.internal(0x10030020, st_track)
STUBS.internal(0x1000DDE8, CL.recorder("GAME_ASSERTMSG", "is"))
STUBS.internal(0x100D6D04, st_code)
STUBS.internal(0x100DD380, st_snd)
STUBS.internal(0x1002AE38, st_ride)
STUBS.internal(0x1002ADDC, st_matrix)
STUBS.internal(0x1002AD34, st_norm)
STUBS.internal(0x100BE7E4, st_launch)
STUBS.internal(0x100BE150, st_pos)
STUBS.internal(0x100C6604, st_flush)
STUBS.internal(0x100C6670, st_setcur)
STUBS.internal(0x1002AE74, st_mask)
STUBS.internal(0x100C7A00, anim_query("GetCurrentAnim", "CurKind", "CurIdx"))
STUBS.internal(0x100C7A74, anim_query("GetDeferredAnim", "DefKind", "DefIdx"))
STUBS.imp("labs", st_labs)
STUBS.imp("atan2", st_atan2)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"
    LAY = MAC_LAY

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = None

    def begin(self, labels):
        self.L = self.e.cl = CL.PpcLog({a: l for l, a in labels.items()}, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def nptr(self, a, target):
        self.words(a, [target])

    def p32(self, target):
        return target

    def set_matrix(self, a):
        CUR["matrix"] = a

    def log(self):
        return self.L.log

    def call(self, bump):
        T.call_ppc(self.e, FN, [bump])


class Host(Side):
    fmt = "<I"
    LAY = HOST_LAY

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        self.base = lib.tpw_bm_base()

    def begin(self, labels):
        CL.host_begin(self.lib, labels, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def nptr(self, a, target):
        self.write(a, struct.pack("<Q", target))

    def p32(self, target):
        return target - self.base if target else 0

    def set_matrix(self, a):
        self.lib.tpw_bm_set_matrix(a)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, bump):
        self.lib.tpw_bm_move(bump)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_bm_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "base").restype, getattr(lib, p + "base").argtypes = C.c_uint64, []
    getattr(lib, p + "set_matrix").restype, getattr(lib, p + "set_matrix").argtypes = None, [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "move").restype, getattr(lib, p + "move").argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    lay = s.LAY
    blk = s.alloc(BLK)
    bump = blk + O_BUMP
    rides = blk + O_RIDES
    sets = [blk + O_SET + i * 0x400 for i in range(4)]
    labels = {1: bump}
    for i, a in enumerate(sets):
        labels[10 + i] = a
        labels[20 + i] = a + S_MESH

    owner = rides + c["owner_slot"] * 0xd0 + c["owner_extra"]
    s.words(owner, [c["kind"]])
    s.words(owner + 0x24, [c["scale"]])
    s.words(owner + 0x50, [c["state"], c["broken"]])

    col = blk + O_COL if c["col"] is not None else 0
    trig = blk + O_TRIG if c["trig"] is not None else 0
    w = list(c["bump"])
    w[0x98 // 4] = s.p32(col)
    w[0x9c // 4] = s.p32(owner)
    s.written = (w[0x98 // 4], w[0x9c // 4])
    s.words(bump, w)
    if col:
        cw = list(c["col"])
        cw[6] = s.p32(trig)
        s.words(col, cw)
    if trig:
        s.words(trig, c["trig"])

    s.words(blk + O_MAT, c["matrix"])
    s.set_matrix(blk + O_MAT)

    for i, st in enumerate(sets):
        mesh, hdr, node, table, entries = st + S_MESH, st + S_HDR, st + S_NODE, st + S_TABLE, st + S_ENTRIES
        s.nptr(st + lay["set_heads"], mesh)
        s.nptr(mesh + lay["mesh_header"], hdr)
        s.nptr(mesh + lay["mesh_f28"], table)
        s.words(hdr + lay["hdr_f78"], [s.p32(node)])
        s.words(node + lay["node_matrix"] + 0x30, c["set_node"][i])
        s.nptr(table + lay["table_entries"], entries)
        for j in range(4):
            ent = entries + j * lay["entry_size"]
            s.words(ent, [c["entry_flags"][i][j]])
            head = st + S_HEADS + j * 0x40 if c["head_xyz"][i][j] is not None else 0
            s.nptr(ent + lay["entry_node"], head)
            if head:
                s.words(head + lay["head_x"], c["head_xyz"][i][j])
        s.nptr(s.g("gRideAnimSets") + i * lay["ptr"], st)

    for k in range(8):
        thing = blk + O_THING + k * 0x40 if (k or c["thing0"]) else 0
        s.nptr(s.g("gThingArray") + k * lay["thing_size"] + lay["thing_ptr"], thing)
        if thing:
            s.words(thing + 0x20, [c["thing_words"][k]])
    s.half(s.g("gAdvisorThingId"), c["advisor"])
    gl = s.g("gColList")
    s.words(gl, [c["magic"], c["tick"], s.p32(rides + c["rides_slot"] * 0xd0)])
    s.words(gl + 0x38, [c["seed"]])
    s.words(s.g("gBumpTurnMax"), [c["tmax"]])
    s.words(s.g("gBumpTurnMin"), [c["tmin"]])
    s.begin(labels)
    return bump, trig, gl


def observe(s, c, world):
    bump, trig, gl = world
    bw = s.rd_words(bump, 0xa8 // 4)
    bw[0x98 // 4] = bw[0x98 // 4] == s.written[0]
    bw[0x9c // 4] = bw[0x9c // 4] == s.written[1]
    lw = s.rd_words(gl, 0x3c // 4)
    return {"bump": bw, "trig": s.rd_words(trig, 5) if trig else None, "list": [lw[0], lw[1], lw[0x38 // 4]], "log": s.log()}


def run(s, c):
    try:
        world = build(s, c)
        s.call(world[0])
        return observe(s, c, world)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rnd_float(r, lo, hi):
    return fb(r.uniform(lo, hi)) if r.random() < 0.9 else r.choice([0, 0x80000000, fb(1.0), fb(-0.5)])


def seed_for(r, roll, mask):
    while True:
        seed = r.getrandbits(32)
        if ((s32((seed * 0x343FD + 0x269EC3) & MASK) >> 16) & mask) == roll:
            return seed


def moved(c):
    """The bump's x and y after the move (the scale as BMP_Move computes it)."""
    flags = c["bump"][0]
    scale = c["scale"]
    if flags & 0x80000:
        scale = tdiv(scale * 7, 10)
    elif c["broken"] == 2:
        scale = 1000
    vx = tdiv(s32(c["bump"][0x3c // 4]) * scale, 1024)
    vy = tdiv(s32(c["bump"][0x40 // 4]) * scale, 1024)
    return s32(c["bump"][0x34 // 4]) + vx, s32(c["bump"][0x38 // 4]) + vy


def make(r, kind=None, flags=None, vx=None, vy=None, scale=None, push=None, D=None, open_=None, roll=None, **over):
    bump = [r.getrandbits(32) for _ in range(0xa8 // 4)]
    if flags is None:
        flags = r.getrandbits(32) & ~0x884700 & MASK
        for bit in (0x80000, 0x4000, 0x400, 0x200, 0x800000, 0x100):
            if r.random() < 0.4:
                flags |= bit
    bump[0] = flags
    bump[0x08 // 4] = r.randrange(4)
    bump[0x10 // 4] = r.choice([5, 0xc, 0, 7])
    bump[0x14 // 4] = r.choice([0, 0, 1, 4])
    bump[0x20 // 4] = r.choice([0, r.randrange(1, 1000)])
    bump[0x24 // 4] = r.choice([-1, 0, 1, 2, 3]) & MASK
    bump[0x28 // 4] = r.choice([-1, 0, 1, 2, 3]) & MASK
    bump[0x2c // 4] = r.choice([-1, r.randrange(0, 50)]) & MASK
    bump[0x34 // 4] = r.randrange(-100000, 100000) & MASK
    bump[0x38 // 4] = r.randrange(-100000, 100000) & MASK
    bump[0x3c // 4] = (r.randrange(-3000, 3001) if vx is None else vx) & MASK
    bump[0x40 // 4] = (r.randrange(-3000, 3001) if vy is None else vy) & MASK
    bump[0x50 // 4] = r.choice([r.randrange(512), r.getrandbits(32)])
    bump[0x54 // 4] = r.choice([r.randrange(512), r.getrandbits(32)])
    bump[0x58 // 4] = (r.choice([r.randrange(-20000, 20001), r.randrange(-100, 101), r.getrandbits(32)]) if push is None else push) & MASK
    c = dict(
        kind=(r.choice(list(range(-15, 1)) + list(range(-14, 0)) * 2) if kind is None else kind) & MASK,
        scale=(r.choice([1024, 1024, 512, 700, 2048, r.randrange(0, 3000), -1024]) if scale is None else scale) & MASK,
        state=r.choice([2, 2, 0, 1]), broken=r.choice([0, 1, 1, 2]),
        owner_slot=r.randrange(4), owner_extra=r.choice([0, 0, 4, 0x68]), rides_slot=r.randrange(4),
        bump=bump,
        matrix=[rnd_float(r, -2, 2) for _ in range(16)],
        set_node=[[rnd_float(r, -60, 60) for _ in range(3)] for _ in range(4)],
        entry_flags=[[r.choice([0, 0x10, r.getrandbits(32)]) for _ in range(4)] for _ in range(4)],
        head_xyz=[[None if r.random() < 0.2 else [rnd_float(r, -60, 60) for _ in range(3)] for _ in range(4)] for _ in range(4)],
        thing0=r.random() < 0.5, thing_words=[r.getrandbits(32) for _ in range(8)], advisor=r.randrange(8),
        magic=r.choice([0x4A454647, 0x4A454647, 0, 0x4A454646]),
        tick=r.choice([0, 0, 0x20, 0x1f, 1, r.getrandbits(32)]), seed=r.getrandbits(32),
        tmax=r.choice([16, 16, 16, r.randrange(-40, 41)]) & MASK, tmin=r.choice([-16, -16, -16, r.randrange(-40, 41)]) & MASK,
        col=None, trig=None,
        queues={"Thing": [r.randrange(8)], "Code": [r.getrandbits(32)], "Snd": [r.getrandbits(32)], "Ride": [r.randrange(4)],
                "Mat": [r.choice([0, 1, 1])], "NormX": [rnd_float(r, -1, 1)], "NormY": [rnd_float(r, -1, 1)],
                "NormZ": [rnd_float(r, -1, 1)], "Launch": [r.getrandbits(32)], "PAR_InitEmitter": [r.getrandbits(32)],
                "CurKind": [r.choice([5, 5, 0xc, 2])], "CurIdx": [r.getrandbits(32)], "DefKind": [r.choice([0xc, 0xc, 5])],
                "DefIdx": [r.getrandbits(32)]})
    if roll is not None:
        c["seed"] = seed_for(r, roll[0], roll[1])
    for k, v in over.items():
        c[k] = v
    # the collision object and its trigger
    if r.random() < 0.85 or D is not None:
        bx, by = moved(c)
        if D is None:
            D = r.choice([r.randrange(-2000, 2001), 0x4af, 0x4b0, 0x4b1, -0x4af, -0x4b0, -0x4b1, 0, -1])
        axis = r.choice([0xc, 0xc, 0xd, 0])
        cx = (bx - D) if axis == 0xc else r.randrange(-100000, 100000)
        cy = (by - D) if axis != 0xc else r.randrange(-100000, 100000)
        c["col"] = [r.choice([0x100, 0x100, 0x100, 0, 0x1ff, 0xfeff]), cx & MASK, cy & MASK, 100, 100, 0, 0, 0]
        if r.random() < 0.9:
            tflags = r.choice([0, 2, r.getrandbits(32)])
            tset = r.randrange(4)
            topen = r.choice([0, 0, 1, -1]) if open_ is None else open_
            if tset == 0:   # the original reads an unset stack slot on these paths
                topen = topen or 1
                tflags &= ~2
            c["trig"] = [tflags, axis, topen & MASK, tset, r.choice([0xdac, 0xdac, 0xdac, 0xdad])]
    # gThingArray row 0 empty: the thing id 0 is read only when it is the advisor's
    if not c["thing0"] and c["queues"]["Thing"][0] == 0 and c["advisor"] != 0:
        c["queues"]["Thing"] = [1]
    return c


def edges():
    r = random.Random(5627)
    yield make(r)
    gate = dict(tick=0, state=2)
    for kind in range(-15, 1):
        for roll in range(8):
            yield make(r, kind=kind, roll=(roll, 7), vx=200, vy=-100, flags=0x4000, **gate, broken=1)
            yield make(r, kind=kind, roll=(roll, 7), vx=-90, vy=10, flags=0x4400 | 0x80000, **gate, broken=1)
        for roll in range(4):
            yield make(r, kind=kind, roll=(roll, 3), vx=50, vy=300, flags=0x4000, **gate, broken=0)
        for push in (0x30, 0x31, -0x30, -0x31, 127, 128, -127, -128, -129, 2047, 2048, 2049, -2048, -2049, 0, -1,
                     0x7FFFFFFF, -0x80000000, 1600, -1600):
            yield make(r, kind=kind, push=push)
        for v in (0x28, 0x29, 0x50, 0x51, 0xe6, 0xe7, 0, 32000):
            yield make(r, kind=kind, vx=v, vy=0, scale=1024, flags=0x4000, **gate, broken=1, roll=(2, 7))
            yield make(r, kind=kind, vx=0, vy=-v, scale=1024, flags=0x400 * r.randrange(2), **gate, broken=1)
        for t in ((16, -16), (0, 0), (-5, 5), (40, 40)):
            yield make(r, kind=kind, tmax=t[0] & MASK, tmin=t[1] & MASK)
        yield make(r, kind=kind, magic=0x4A454647, owner_extra=0x68, rides_slot=3, owner_slot=0)
        yield make(r, kind=kind, magic=0)
    for D in (0x4af, 0x4b0, 0x4b1, -0x4af, -0x4b0, -0x4b1, 0, -1, 1):
        for fl in (0, 0x200, 0x800000, 0x800200):
            for op in (0, 1):
                yield make(r, flags=fl, D=D, open_=op)


TESTS = [
    FnTest("BMP_Move", ppc, host, edge=edges, rnd=make),
]
