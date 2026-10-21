"""Unit v5684, LLCC_SetTrackAnimReverse, against libtpw_unit_LLCC_SetTrackAnimReverse.dylib (tests/unit_LLCC_SetTrackAnimReverse.cpp), main app
("SimTheme Park"), src/game/park/LLCellRide.cpp:
  LLCC_SetTrackAnimReverse 0x1008cad8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (LLCR_OffsetEndOfTrack,
GetMapCellEquiv, IsTrack, IsCorner, OPPOSITE, GetNeighbours, SetCurRideAnim, GetNeighbouringCell, IsTrackStartBlock) are fakes on both
sides that record their arguments (a cell as its index in the track cell array, a ride anim set as its index in gRideAnimSets) and answer
from the case's queues (the last value repeats).

The world of a case: gGameWorld pointing at a block with all 0x4000 track cells (mesh instance +4 and parent id +0x10 filled from a
pattern), gRideAnimSets with distinct sets, the object (bytes +4, +6, rotation +0x10). What is compared: the answer (the original's r3 is
always 0), the whole event log. No fused operations. Not tested: a start so near the grid edge that the walk leaves the 0x4000 cells (the
original reads whatever lies there).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCC_SetTrackAnimReverse"
MASK = 0xFFFFFFFF

FN = 0x1008cad8
SIZES = {FN: 756}
OWN = set(SIZES)

OFFSET_END, GETMAPCELL, ISTRACK, ISCORNER, OPPOSITE, GETNEIGH, SETANIM, GETNC, ISSTART = (
    0x1008c6d8, 0x100f49a0, 0x100a3574, 0x100a37a0, 0x10102500, 0x100816e8, 0x100c6670, 0x1012fddc, 0x100a36cc)

ADDR = dict(world=0x1037ebfc, sets=0x102c79a4)
HOST_LAYOUT = [4, 0x10, 0x1102d8, 0x28]
CELLS = 0x1102d8
WORLD_SIZE = 0x1102d8 + 0xA0000 + 0x1000


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cidx(e, ptr):
    return ((ptr & MASK) - e.u32(ADDR["world"]) - CELLS) // 0x28


def stub_offset_end(e):
    L = e.cl
    px, py = e.r(3), e.r(4)
    x, y = e.s32(px), e.s32(py)
    L.rec("OffsetEnd", (x, y, s32(e.r(5))))
    e.write(px, struct.pack(">I", (x + L.ret("OEdx")) & MASK))
    e.write(py, struct.pack(">I", (y + L.ret("OEdy")) & MASK))
    e.set_r(3, L.ret("OffsetEnd") & 0xFF)


def stub_getmap(e):
    e.cl.rec("GetMapCellEquiv", (cidx(e, e.r(3)),))
    e.set_r(3, 0)


def stub_istrack(e):
    e.cl.rec("IsTrack", (cidx(e, e.r(3)),))
    e.set_r(3, e.cl.ret("IsTrack") & MASK)


def stub_iscorner(e):
    e.cl.rec("IsCorner", (cidx(e, e.r(3)),))
    e.set_r(3, e.cl.ret("IsCorner") & 0xFF)


def stub_opposite(e):
    e.cl.rec("OPPOSITE", (e.r(3) & 0xFF,))
    e.set_r(3, e.cl.ret("OPPOSITE") & 0xFF)


def stub_getneigh(e):
    e.cl.rec("GetNeighbours", (cidx(e, e.r(3)),))
    e.set_r(3, e.cl.ret("GetNeighbours") & 0xFF)


def stub_setanim(e):
    sets = struct.unpack(">32I", e.read(ADDR["sets"], 128))
    p = e.r(3) & MASK
    si = sets.index(p) if p in sets else -1
    e.cl.rec("SetCurRideAnim", (si, e.r(4) & MASK, e.r(5) & MASK, e.r(6) & MASK, fb(e.f(1)), e.r(8) & MASK))


def stub_getnc(e):
    e.cl.rec("GetNeighbouringCell", (cidx(e, e.r(3)), e.r(4) & 0xFF, s32(e.r(5))))
    v = e.cl.ret("GetNeighbouringCell")
    e.set_r(3, 0 if v == 0 else (e.u32(ADDR["world"]) + CELLS + (v - 1) * 0x28) & MASK)


def stub_isstart(e):
    e.cl.rec("IsTrackStartBlock", (cidx(e, e.r(3)),))
    e.set_r(3, e.cl.ret("IsTrackStartBlock") & MASK)


for _a, _f in ((OFFSET_END, stub_offset_end), (GETMAPCELL, stub_getmap), (ISTRACK, stub_istrack), (ISCORNER, stub_iscorner),
               (OPPOSITE, stub_opposite), (GETNEIGH, stub_getneigh), (SETANIM, stub_setanim), (GETNC, stub_getnc),
               (ISSTART, stub_isstart)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ptrs(self, a, ps):
        """Pointers: 4 bytes on the Mac, 8 on the host (whose arena lies above 4 GB)."""
        f = ">I" if self.fmt == ">I" else "<Q"
        self.write(a, b"".join(struct.pack(f, p) for p in ps))

    def cells_bytes(self, salt):
        cache = {}
        out = []
        for i in range(0x4000):
            parent = ((i * salt) >> 3) & 1
            mesh = 0 if (i * 7 + salt) % 3 == 0 else 1 + (i + salt) % 5
            key = (mesh, parent)
            b = cache.get(key)
            if b is None:
                buf = bytearray(0x28)
                struct.pack_into(self.fmt, buf, 4, mesh)
                struct.pack_into(self.fmt[0] + "H", buf, 0x10, parent)
                b = cache[key] = bytes(buf)
            out.append(b)
        return b"".join(out)


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

    def call(self, obj, flag):
        r3, _, _ = T.call_ppc(self.e, FN, [obj & MASK, flag])
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
        return self.lib.tpw_llcc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_llcc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, obj, flag):
        return self.lib.tpw_llcc_run(obj, flag) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_llcc_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    world = s.alloc(WORLD_SIZE)
    s.words(world + CELLS, [])
    s.write(world + CELLS, s.cells_bytes(case["salt"]))
    s.ptrs(s.g("world"), [world])
    sets = [0] + [s.alloc(16) for _ in range(7)]
    s.ptrs(s.g("sets"), sets + [0] * 24)
    obj = s.alloc(0x20)
    s.write(obj + 4, bytes([case["b4"]]))
    s.write(obj + 6, bytes([case["b6"]]))
    s.words(obj + 0x10, [case["rot"]])
    return obj


def run(s, case):
    try:
        obj = build(s, case)
        return {"answer": s.call(obj, case["flag"]), "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


DIRS = [1, 4, 0x10, 0x40]


def make(r, flag=None, direction=None, x=None, y=None):
    x = r.randrange(20, 100) if x is None else x
    y = r.randrange(20, 100) if y is None else y
    b4, b6 = x, y
    if y >= 1 and r.random() < 0.3:
        b4, b6 = x + 128, y - 1
    n = r.randrange(1, 9)
    corner = [r.choice([0, 0, r.randrange(1, 256)]) for _ in range(n)] + [0]
    neigh = [r.choice([0, r.randrange(256), r.choice(DIRS), r.choice(DIRS) | r.choice(DIRS)]) for _ in range(r.randrange(1, 12))] + [0]
    d = r.choice(DIRS) if direction is None else direction
    q = {
        "OEdx": [r.randrange(-3, 4)], "OEdy": [r.randrange(-3, 4)],
        "OffsetEnd": [d if r.random() < 0.9 else r.randrange(256)],
        "IsTrack": [r.choice([1, 1, 1, 0]) for _ in range(r.randrange(1, 6))] + [1],
        "IsCorner": corner,
        "GetNeighbours": neigh,
        "OPPOSITE": [r.choice(DIRS + [r.randrange(256)]) for _ in range(r.randrange(1, 8))],
        "GetNeighbouringCell": [r.choice([0, 0, r.randrange(1, 0x4001)]) for _ in range(r.randrange(1, 8))],
        "IsTrackStartBlock": [r.choice([0, 1]) for _ in range(r.randrange(1, 6))],
    }
    return {"b4": b4, "b6": b6, "rot": r.choice([0, 0x5A, 0xB4, 0x10E, r.randrange(0, 360)]), "flag": flag if flag is not None else r.choice([0, 1]),
            "salt": r.randrange(1, 50), "queues": q}


def edges():
    r = random.Random(684)
    for flag in (0, 1):
        for d in DIRS + [2]:
            c = make(r, flag, d)
            yield c
    for x, y in ((8, 8), (100, 8), (8, 100), (100, 100)):
        yield make(r, None, None, x, y)
    for _ in range(40):
        yield make(r)


def rnd(r):
    return make(r)


TESTS = [
    FnTest("LLCC_SetTrackAnimReverse", ppc, host, edge=edges, rnd=rnd),
]
