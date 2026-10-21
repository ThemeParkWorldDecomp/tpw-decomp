"""Unit v5087, the bump removal of the advisor world, against libtpw_unit_BMP_REMOVEBUMP.dylib (tests/unit_BMP_REMOVEBUMP.cpp), main app
("SimTheme Park"), src/game/rides/BumperRide.cpp:
  BMP_REMOVEBUMP 0x1002e614, BMP_DOCK 0x1002e740, RemovePeepsFromBump 0x1002e464, FindTargetBump 0x1002df10

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (DoAreasIntersect, CWorld::GetRand, DisableRideHead, PAR_InitEmitter, PAR_KillEmitter, DestroyRideInstance,
TrackRideIdToThingId, GAME_ASSERTMSG, SND_SendIndirectedEvent, SOUND_MoveSoundEffect, SOUND_FadeSoundEffect, TRACE) are fakes on both
sides that record their arguments (the text of GAME_ASSERTMSG and TRACE as a number) and answer from the case's queues.

The world of a case: the list (gColList: magic, ride records, bumps, count), 4 rides of 0xd0 bytes (in a block of 256), 5 bumps of 0xac
bytes, 8 peep nodes chained on bumps and on the rides' free lists, 3 collision objects, 3 ride instances (anim set, mesh, header, node
with the emitter floats), 4 thing rows, the sound bank word, the advisor thing id and gGameWorld. Pointer fields inside the records are
Ptr32 offsets from the arena base on the host and plain addresses on the Mac; they are compared as (block, offset). What is compared: the
answer, the list's magic and count, every ride, bump and node word, the out word of FindTargetBump and the event log.

No fused operations; the float to int conversions stay below 2^31 (the emulator's fctiwz does not saturate). Not tested: a list count
larger than the live bumps (the loop would run off the block), a thing row that is empty and read (a NULL read), anim sets not set up.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_REMOVEBUMP"
MASK = 0xFFFFFFFF

FN_REMOVE, FN_DOCK, FN_PEEPS, FN_FIND = 0x1002E614, 0x1002E740, 0x1002E464, 0x1002DF10
SIZES = {FN_REMOVE: 260, FN_DOCK: 836, FN_PEEPS: 380, FN_FIND: 288}
OWN = set(SIZES)

CALLEES = dict(
    areas=0x1002B6C0, rand=0x1013B1E4, disable=0x1020039C, emitter=0x100BEC30, kill=0x100BE1E8, destroy=0x1006A3EC,
    track=0x10030020, assertmsg=0x1000DDE8, send=0x100DF858, move=0x100DDF60, fade=0x100DE0FC, trace=0x1006FCFC,
)

ADDR = dict(list=0x102B56C0, thingarray=0x1034CBFC, ridesets=0x102C79A4, banks=0x102F7C94, advisor=0x1029C960, gameworld=0x1037EBFC)

NR, NB, NN, NC, NT = 4, 5, 8, 3, 4
KINDS = [-14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, 0, 3]
LAY = {}          # the host's layout, filled by bind


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ints(name, n, retkey=None):
    def stub(e):
        e.cl.rec(name, tuple(s32(e.r(3 + i)) for i in range(n)))
        if retkey:
            e.set_r(3, e.cl.ret(retkey) & MASK)
    return stub


def stub_areas(e):
    e.cl.rec("DoAreasIntersect", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
    e.set_r(3, e.cl.ret("DoAreasIntersect") & MASK)


def stub_rand(e):
    e.cl.rec("GetRand", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("GetRand") & MASK)


def stub_disable(e):
    e.cl.rec("DisableRideHead", (e.cl.lab(e.r(3)), e.r(4) & MASK))


def stub_destroy(e):
    e.cl.rec("DestroyRideInstance", (e.r(3) & MASK,))


def stub_track(e):
    e.cl.rec("TrackRideIdToThingId", (s32(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", e.cl.ret("TrackRideIdToThingId") & 0xFFFF))


def stub_assert(e):
    text = e.cstr(e.r(4))
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)), 1 if text.startswith("T") else 2))


def stub_trace(e):
    text = e.cstr(e.r(3))
    if text.startswith("Peep"):
        e.cl.rec("TRACE", (1, s32(e.r(4)), s32(e.r(5))))
    else:
        e.cl.rec("TRACE", (2, 0, 0))


STUBS.internal(CALLEES["areas"], stub_areas)
STUBS.internal(CALLEES["rand"], stub_rand)
STUBS.internal(CALLEES["disable"], stub_disable)
STUBS.internal(CALLEES["emitter"], ints("PAR_InitEmitter", 4))
STUBS.internal(CALLEES["kill"], ints("PAR_KillEmitter", 1))
STUBS.internal(CALLEES["destroy"], stub_destroy)
STUBS.internal(CALLEES["track"], stub_track)
STUBS.internal(CALLEES["assertmsg"], stub_assert)
STUBS.internal(CALLEES["send"], ints("SND_SendIndirectedEvent", 7, "SND_SendIndirectedEvent"))
STUBS.internal(CALLEES["move"], ints("SOUND_MoveSoundEffect", 4, "SOUND_MoveSoundEffect"))
STUBS.internal(CALLEES["fade"], ints("SOUND_FadeSoundEffect", 1))
STUBS.internal(CALLEES["trace"], stub_trace)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def alloc_world(self):
        """The same allocation order on both sides; the anim blocks have the side's own sizes."""
        self.rides = self.alloc(256 * 0xD0)
        self.bumps = self.alloc(12 * 0xAC)
        self.nodes = self.alloc(NN * 0x20)
        self.cols = self.alloc(NC * 0x20)
        self.things = [self.alloc(0x40) for _ in range(NT)]
        self.sets = [None] + [(self.alloc(self.sz_set), self.alloc(self.sz_mesh), self.alloc(self.sz_hdr), self.alloc(0x80))
                              for _ in range(3)]
        self.gw = self.alloc(0x10)
        self.out = self.alloc(8)
        self.regions = [("rides", self.rides, 256 * 0xD0), ("bumps", self.bumps, 12 * 0xAC), ("nodes", self.nodes, NN * 0x20),
                        ("cols", self.cols, NC * 0x20)]

    def labels(self):
        d = {self.gw: 5}
        for i in range(1, 4):
            d[self.sets[i][1]] = 10 + i
        for j in range(NC):
            d[self.cols + 0x20 * j] = 20 + j
        return d

    def norm(self, w):
        """A pointer field read back: (block, offset) or 0."""
        if w == 0:
            return 0
        return self.norm_addr(self.dec(w))

    def norm_addr(self, a):
        if a == 0:
            return 0
        for name, base, size in self.regions:
            if base <= a < base + size:
                return (name, a - base)
        return ("?", a)

    def bump_addr(self, i):
        return self.bumps + 0xAC * i

    def ride_addr(self, k):
        return self.rides + 0xD0 * k


class Ppc(Side):
    fmt = ">I"
    sz_set, sz_mesh, sz_hdr = 0x100, 0x20, 0xA8

    def __init__(self, e, case):
        self.e = e
        self.alloc_world()
        self.L = e.cl = CL.PpcLog(self.labels(), case["queues"])
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

    def enc(self, a):
        return a

    def dec(self, w):
        return w

    def pset(self, a, v):
        self.words(a, [v])

    def set_slot(self, i):
        return self.g("ridesets") + 4 * i

    def thing_row(self, t):
        return self.g("thingarray") + 0x14 * t

    off_heads, off_header, off_f78 = 8, 4, 0x78

    def half(self, a, v):
        self.write(a, struct.pack(">H", v & 0xFFFF))

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.sz_set, self.sz_mesh, self.sz_hdr = LAY["set"], LAY["mesh"], LAY["hdr"]
        self.off_heads, self.off_header, self.off_f78 = LAY["heads"], LAY["header"], LAY["f78"]
        self.alloc_world()
        self.base = lib.tpw_bmp_base()          # set by the first allocation
        CL.host_begin(lib, {lab: a for a, lab in self.labels().items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bmp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bmp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def enc(self, a):
        return a - self.base if a else 0

    def dec(self, w):
        return w + self.base if w else 0

    def pset(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def set_slot(self, i):
        return self.g("ridesets") + 8 * i

    def thing_row(self, t):
        return self.g("thingarray") + LAY["row"] * t

    def half(self, a, v):
        self.write(a, struct.pack("<H", v & 0xFFFF))

    def call(self, fn, args):
        L = self.lib
        if fn == FN_REMOVE:
            return L.tpw_bmp_remove(s32(args[0]), s32(args[1])) & MASK
        if fn == FN_DOCK:
            return L.tpw_bmp_dock(args[0]) & MASK
        if fn == FN_PEEPS:
            return L.tpw_bmp_peeps(args[0], s32(args[1])) & MASK
        return L.tpw_bmp_find(args[0], args[1], args[2])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_bmp_"
    lib.tpw_bmp_addr.restype, lib.tpw_bmp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bmp_alloc.restype, lib.tpw_bmp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bmp_base.restype, lib.tpw_bmp_base.argtypes = C.c_uint64, []
    lib.tpw_bmp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bmp_remove.restype, lib.tpw_bmp_remove.argtypes = C.c_int32, [C.c_int32, C.c_int32]
    lib.tpw_bmp_dock.restype, lib.tpw_bmp_dock.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_bmp_peeps.restype, lib.tpw_bmp_peeps.argtypes = C.c_int32, [C.c_uint64, C.c_int32]
    lib.tpw_bmp_find.restype, lib.tpw_bmp_find.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bmp_layout(out)
    v = list(out)[:n]
    assert v[4] == 0x78 and v[8] == 0x20, v        # the header's mF78 at the Mac's offset, the list block's size
    LAY.update(heads=v[0], set=v[1], header=v[2], mesh=v[3], f78=v[4], hdr=v[5], mthing=v[6], row=v[7])
    assert LAY["mthing"] == 0, v
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    g = s.g("list")
    s.words(g, [0x4A454647 if case["live"] else case["magic"]])
    s.words(g + 8, [s.enc(s.rides)])
    s.words(g + 0x10, [s.enc(s.bumps), case["count"]])
    # nodes and their chains
    for n in range(NN):
        s.words(s.nodes + 0x20 * n, case["nodes"][n])
    heads = {}
    for owner, chain in case["chains"].items():
        for j, n in enumerate(chain):
            nxt = s.nodes + 0x20 * chain[j + 1] if j + 1 < len(chain) else 0
            s.words(s.nodes + 0x20 * n + 0x10, [s.enc(nxt)])
        heads[owner] = s.nodes + 0x20 * chain[0] if chain else 0
    for k in range(NR):
        a = s.ride_addr(k)
        s.words(a, case["rides"][k])
        s.words(a + 0x58, [s.enc(s.bump_addr(case["r58"][k])) if case["r58"][k] >= 0 else 0])
        s.words(a + 0xC8, [s.enc(heads.get(("r", k), 0))])
    for i in range(NB):
        a = s.bump_addr(i)
        s.words(a, case["bumps"][i])
        s.words(a + 0x30, [s.enc(heads.get(("b", i), 0))])
        s.words(a + 0x94, [0])
        col = case["bcol"][i]
        s.words(a + 0x98, [s.enc(s.cols + 0x20 * col) if col >= 0 else 0])
        ride = case["bride"][i]
        s.words(a + 0x9C, [s.enc(s.ride_addr(ride)) if ride >= 0 else 0])
    for t in range(NT):
        s.words(s.things[t] + 0x20, [case["things"][t]])
        if case["rowset"][t]:
            s.pset(s.thing_row(t), s.things[t])
    for i in range(1, 4):
        st, mesh, hdr, node = s.sets[i]
        s.pset(st + s.off_heads, mesh)
        s.pset(mesh + s.off_header, hdr)
        s.words(hdr + s.off_f78, [s.enc(node)])
        s.words(node + 0x40, case["emit"][i])
        s.pset(s.set_slot(i), st)
    s.pset(s.g("gameworld"), s.gw)
    s.words(s.g("banks") + 0x1C, [case["bank7"]])
    s.half(s.g("advisor"), case["advisor"])
    s.words(s.out, [0x12345678])


def execute(s, case):
    op = case["op"]
    a = case["args"]
    if op == "remove":
        return s.call(FN_REMOVE, [a[0], a[1]])
    if op == "dock":
        return s.call(FN_DOCK, [s.bump_addr(a[0])])
    if op == "peeps":
        return s.call(FN_PEEPS, [s.bump_addr(a[0]), a[1]])
    out = s.out if a[2] else 0
    return s.call(FN_FIND, [s.bump_addr(a[0]), s.ride_addr(a[1]), out])


def observe(s, case, answer):
    g = s.g("list")
    res = {"answer": s.norm_addr(answer) if case["op"] == "find" else answer, "log": s.log(),
           "magic": s.rd_word(g), "count": s.rd_word(g + 0x14), "out": s.rd_word(s.out)}
    res["rides"] = []
    for k in range(NR):
        w = s.rd_words(s.ride_addr(k), 52)
        for i in (22, 50):
            w[i] = s.norm(w[i])
        res["rides"].append(w)
    res["bumps"] = []
    for i in range(NB):
        w = s.rd_words(s.bump_addr(i), 43)
        for j in (12, 37, 38, 39):
            w[j] = s.norm(w[j])
        res["bumps"].append(w)
    res["nodes"] = []
    for n in range(NN):
        w = s.rd_words(s.nodes + 0x20 * n, 8)
        w[4] = s.norm(w[4])
        res["nodes"].append(w)
    return res


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rint32(r):
    return r.choice([0, 1, -1, r.randrange(-10 ** 6, 10 ** 6), r.randrange(-3000, 3000), 2 ** 31 - 1, -2 ** 31]) & MASK


def rflt(r):
    k = r.random()
    if k < 0.1:
        return fb(0.0)
    if k < 0.7:
        return fb(r.uniform(-100, 100))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-3, 5))


def base(r, op):
    c = {"op": op}
    c["live"] = r.random() < 0.9
    c["magic"] = r.choice([0, 0x4A454647 ^ 1, r.getrandbits(32)])
    c["rides"] = []
    for k in range(NR):
        w = [r.getrandbits(32) for _ in range(52)]
        w[0] = r.choice(KINDS) & MASK
        w[20] = r.choice([0, 1, 2, 3, 4, 5])
        w[23] = r.choice([0, 1, 1, 2, 3, 5])
        w[24] = r.choice([0, 1, 2, 5])
        c["rides"].append(w)
    c["bumps"] = []
    nlive = 0
    for i in range(NB):
        w = [r.getrandbits(32) for _ in range(43)]
        live = r.random() < 0.75
        nlive += live
        flags = (1 if live else 0) | r.choice([0, 0x20, 0x400000, 0x2000, 0x02000000, 0x02002000, 0x4000, 0x100000, 0x1F5E0])
        w[0] = flags | (r.choice([0, 0x400000, 0x20]) if r.random() < 0.3 else 0)
        w[2] = r.randrange(1, 4)
        w[3] = r.randrange(1, 4)
        w[8] = r.choice([0, 5, 0x1234, 0xFFFFFFFF])
        w[11] = r.choice([0xFFFFFFFF, 7, 0])
        w[13] = rint32(r)
        w[14] = rint32(r)
        c["bumps"].append(w)
    c["bride"] = [r.choice([0, 0, 1, 1, 2, -1 if r.random() < 0.1 and op == "remove" else 3]) for _ in range(NB)]
    c["bcol"] = [r.choice([-1, 0, 1, 2]) for _ in range(NB)]
    c["count"] = nlive if r.random() < 0.85 else r.randrange(0, nlive + 1)
    c["r58"] = [r.choice([-1, -1, r.randrange(NB)]) for _ in range(NR)]
    c["nodes"] = []
    for n in range(NN):
        w = [r.getrandbits(32) for _ in range(8)]
        w[0] = r.getrandbits(16)
        w[2] = r.choice([0xFFFFFFFF, 0, 1, 5])
        w[4] = 0
        c["nodes"].append(w)
    owners = [None] + [("b", i) for i in range(NB)] * 2 + [("r", k) for k in range(NR)]
    c["chains"] = {}
    for n in range(NN):
        o = r.choice(owners)
        if o is not None:
            c["chains"].setdefault(o, []).append(n)
    c["things"] = [r.getrandbits(32) for _ in range(NT)]
    c["rowset"] = [r.random() < 0.3, True, True, True]
    c["advisor"] = r.choice([0, 1, 2, 3])
    c["emit"] = [None] + [[rflt(r), rflt(r), rflt(r)] for _ in range(3)]
    c["bank7"] = r.getrandbits(32)
    tid = r.choice([1, 2, 3, c["advisor"]])
    c["queues"] = {
        "DoAreasIntersect": [r.choice([0, 1]) for _ in range(8)],
        "GetRand": [r.choice([0, 1, 2, 7, r.getrandbits(32), 0xFFFFFFFF])],
        "TrackRideIdToThingId": [tid],
        "SND_SendIndirectedEvent": [r.choice([0, 0x77, 0xFFFFFFFB])],
        "SOUND_MoveSoundEffect": [r.choice([0, 0x99])],
    }
    return c


def force(c, bump=None, bflags=None, kind=None, state=None, count=None, handle=None):
    """Makes the bump live (unless said otherwise) and sets what the edge case is about."""
    k = c["bride"][bump]
    if k < 0:
        k = c["bride"][bump] = 0
    if bflags is not None:
        c["bumps"][bump][0] = bflags
    if handle is not None:
        c["bumps"][bump][8] = handle
    if kind is not None:
        c["rides"][k][0] = kind & MASK
    if state is not None:
        c["rides"][k][20] = state
    if count is not None:
        c["rides"][k][23] = count
    return k


def ride_id(c, k):
    return ((c["rides"][k][0] << 8) | k) & MASK


# ---- BMP_REMOVEBUMP
def remove_case(r, mode=None):
    c = base(r, "remove")
    k = r.randrange(NR)
    rid = ride_id(c, k)
    mode = r.randrange(6) if mode is None else mode
    if mode == 1:
        rid = (rid + r.choice([1, 256, -256, 0x100000])) & MASK
    elif mode == 2:
        rid = r.randrange(0, 300)
    elif mode == 3:
        rid = r.getrandbits(32)
    elif mode == 4:
        rid = 0
    c["args"] = [rid, r.choice([0, 1])]
    if mode == 0:       # make a bump of that ride live, some with peeps (the a flag skips those)
        i = r.randrange(NB)
        c["bride"][i] = k
        c["bumps"][i][0] |= 1
        c["count"] = sum(w[0] & 1 for w in c["bumps"])
    return c


def remove_edges():
    r = random.Random(5087)
    for mode in range(6):
        for a in (0, 1):
            for live in (True, False):
                c = remove_case(r, mode)
                c["args"][1] = a
                c["live"] = live
                yield c
    for a in (0, 1):                     # a bump with peeps, a bump without
        for withpeeps in (True, False):
            c = remove_case(r, 0)
            c["args"][1] = a
            k = c["bride"][[i for i in range(NB) if c["bumps"][i][0] & 1][0]]
            c["args"][0] = ride_id(c, k)
            c["chains"] = {("b", i): [i] for i in range(NB)} if withpeeps else {}
            yield c


# ---- BMP_DOCK
def dock_case(r):
    c = base(r, "dock")
    i = r.randrange(NB)
    c["args"] = [i]
    return c


def dock_edges():
    r = random.Random(50871)
    for kind in KINDS:
        for state in (0, 4):
            for cnt in (1, 2):
                c = dock_case(r)
                force(c, 0, bflags=1 | r.choice([0, 0x400000, 0x02000000, 0x2000, 0x02002000]), kind=kind, state=state, count=cnt,
                      handle=r.choice([0x1234, 5]))
                c["args"] = [0]
                yield c
    for tid_adv in (True, False):
        for row0 in (True, False):
            c = dock_case(r)
            force(c, 1, bflags=1, kind=-7, state=0, count=3, handle=9)
            c["args"] = [1]
            c["advisor"] = 2
            c["rowset"][0] = row0
            c["queues"]["TrackRideIdToThingId"] = [2 if tid_adv else (0 if row0 else 3)]
            yield c
    for handle in (0, 0xFFFFFFFF):
        c = dock_case(r)
        force(c, 2, bflags=1, kind=-7, state=1, count=2, handle=handle)
        c["bumps"][2][11] = 0xFFFFFFFF if handle == 0 else 3
        c["args"] = [2]
        yield c
    for send in (0, 0x55):
        c = dock_case(r)
        force(c, 2, bflags=1, kind=-9, state=1, count=2, handle=3)
        c["queues"]["SND_SendIndirectedEvent"] = [send]
        c["args"] = [2]
        yield c


# ---- RemovePeepsFromBump
def peeps_case(r):
    c = base(r, "peeps")
    c["args"] = [r.randrange(NB), r.choice([0, 1, 2])]
    return c


def peeps_edges():
    r = random.Random(50872)
    for emit in (0, 1):
        for live in (True, False):
            for n in (0, 1, 3):
                c = peeps_case(r)
                c["live"] = live
                c["args"] = [0, emit]
                c["bride"][0] = 0
                c["chains"] = {("b", 0): list(range(n))}
                for q in range(n):
                    c["nodes"][q][2] = [0xFFFFFFFF, 3, 0][q % 3]
                yield c


# ---- FindTargetBump
def find_case(r):
    c = base(r, "find")
    i = r.randrange(NB)
    k = c["bride"][i]
    if k < 0:
        k = c["bride"][i] = 0
    c["args"] = [i, k if r.random() < 0.9 else r.randrange(NR), r.choice([0, 1])]
    if r.random() < 0.5:
        for w in c["bumps"]:
            w[0] = (w[0] | 1) & ~0x20 & MASK
        c["count"] = NB
        c["bride"] = [k if r.random() < 0.85 else (k + 1) % NR for _ in range(NB)]
        c["bride"][i] = k
    return c


def find_edges():
    r = random.Random(50873)
    for cnt in (0, 1, 2, 3):
        for rnd in (0, 1, 2, 3, 5, 0xFFFFFFFF):
            for out in (0, 1):
                c = find_case(r)
                k = c["args"][1]
                c["rides"][k][23] = cnt
                c["args"][2] = out
                c["queues"]["GetRand"] = [rnd]
                c["queues"]["DoAreasIntersect"] = [1]
                for w in c["bumps"]:
                    w[0] = (w[0] | 1) & ~0x20 & MASK
                c["count"] = NB
                c["bride"] = [c["bride"][c["args"][0]]] * NB
                yield c
    for flagbits in (0x20, 0):
        c = find_case(r)
        c["bumps"][0][0] = 1 | flagbits
        c["count"] = sum(w[0] & 1 for w in c["bumps"])
        yield c


TESTS = [
    FnTest("BMP_REMOVEBUMP", ppc, host, edge=remove_edges, rnd=lambda r: remove_case(r)),
    FnTest("BMP_DOCK", ppc, host, edge=dock_edges, rnd=dock_case),
    FnTest("RemovePeepsFromBump", ppc, host, edge=peeps_edges, rnd=peeps_case),
    FnTest("FindTargetBump", ppc, host, edge=find_edges, rnd=find_case),
]
