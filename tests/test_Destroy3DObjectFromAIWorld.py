"""Unit v5901, the AI-world cell helpers of the level code, against libtpw_unit_Destroy3DObjectFromAIWorld.dylib
(tests/unit_Destroy3DObjectFromAIWorld.cpp), main app ("SimTheme Park"), src/game/park/TPHField.cpp:
  Destroy3DObjectFromAIWorld 0x1005f3fc, Assign3DObjectToAIWorld 0x1005f53c, GetWorldGSquareHeight 0x1005f160,
  DetermineInnerBaseInsets 0x1005f738, GetRideIDAtCell 0x1005f080

A case is a world and a call, built twice by ONE piece of Python code (emulator memory big-endian at the Mac's addresses, host memory
little-endian). The callees outside the unit (CMap::IsXYValid, GetTrackCellEquiv, CMapCell Delete/Modify/ModifyParentID/GetFlag/SetFlag,
CTrackCell::Delete, LLCC_GetTrackMeshAt, LLCR_OverRide, GAME_ASSERTMSG, DetermineHFieldHeightQuick) are fakes on both sides that record their
arguments (cells as an index from the world's cell array) and answer from the case's queues. sqrt is the emulator's C sqrt and the host's.

The world: the map size globals, gGameWorld (a block with random cell words), the thing table (entry 0 and ids 0..9), the ride set table
(ids 1..9 with mesh, header, node, height map block and data), gLevelNoThingId. Compared: the answer (float bits), the call log, the extra
word, the track block, the insets block.

No fused operations that round differently (the fmadd squares a float, exact in double). Not tested: a NULL cell (the world is never NULL),
thing ids past the table, height map reads outside the data block, a 16 bit overflow of the cell id.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Destroy3DObjectFromAIWorld"
MASK = 0xFFFFFFFF

FN_DESTROY, FN_ASSIGN, FN_HEIGHT, FN_INSETS, FN_RIDE = 0x1005F3FC, 0x1005F53C, 0x1005F160, 0x1005F738, 0x1005F080
SIZES = {FN_DESTROY: 268, FN_ASSIGN: 384, FN_HEIGHT: 620, FN_INSETS: 316, FN_RIDE: 184}
OWN = set(SIZES)

IS_VALID, EQUIV, CELL_DELETE, TRACK_DELETE, MODIFY = 0x101012F4, 0x100F48C4, 0x1009D0F4, 0x100A40C4, 0x10097538
PARENT_ID, GET_FLAG, SET_FLAG, MESH_AT, OVERRIDE, ASSERT, QUICK = 0x10099930, 0x1009BBE8, 0x1009BBB8, 0x100A55BC, 0x10089210, 0x1000DDE8, 0x1005EDA8

ADDR = dict(world=0x1037EBFC, hw=0x102C6C68, hh=0x102C6C64, noid=0x1029CF10, things=0x1034CBFC, sets=0x102C79A4)
HOST_LAYOUT = None          # filled by bind: heads, heightmap, mesh header, mesh rotation, thing entry size
MAC_OFF = dict(heads=8, hmap=0xCC, mhdr=4, mrot=0x10, entry=0x14, ptr=4)
HOST_OFF = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def cell_index(e, ptr):
    return (s32(ptr) - s32(e.world) - 0x2D8) // 0x44


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_valid(e):
    e.cl.rec("IsXYValid", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("valid") & MASK)


def st_equiv(e):
    e.cl.rec("Equiv", (cell_index(e, e.r(3)),))
    e.set_r(3, e.trk)


def st_cell_delete(e):
    e.cl.rec("CellDelete", (cell_index(e, e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_modify(e):
    e.cl.rec("Modify", (cell_index(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def st_parent(e):
    e.cl.rec("ParentID", (cell_index(e, e.r(3)), (e.r(4) >> 16) & 0xFFFF))


def st_getflag(e):
    e.cl.rec("GetFlag", (cell_index(e, e.r(3)),))
    e.set_r(3, e.cl.ret("flag") & 0xFFFF)


def st_setflag(e):
    e.cl.rec("SetFlag", (cell_index(e, e.r(3)), e.r(4) & 0xFFFF))


def st_track_delete(e):
    e.cl.rec("TrackDelete", (1 if e.r(3) == e.trk else 0, s32(e.r(4)), s32(e.r(5)), e.r(6) & 0xFF))


def st_mesh(e):
    e.cl.rec("Mesh", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("mesh") & MASK)


def st_override(e):
    e.cl.rec("Override", (s32(e.r(4)), s32(e.r(5))))
    e.write(e.r(3), struct.pack(">H", e.cl.ret("ovr") & 0xFFFF))


def st_quick(e):
    e.cl.rec("Quick", (fb(e.f(1)), fb(e.f(2))))
    e.set_f1(bitsf(e.cl.ret("quick")))


STUBS.internal(IS_VALID, st_valid)
STUBS.internal(EQUIV, st_equiv)
STUBS.internal(CELL_DELETE, st_cell_delete)
STUBS.internal(MODIFY, st_modify)
STUBS.internal(PARENT_ID, st_parent)
STUBS.internal(GET_FLAG, st_getflag)
STUBS.internal(SET_FLAG, st_setflag)
STUBS.internal(TRACK_DELETE, st_track_delete)
STUBS.internal(MESH_AT, st_mesh)
STUBS.internal(OVERRIDE, st_override)
STUBS.internal(ASSERT, CL.recorder("Assert", "is"))
STUBS.internal(QUICK, st_quick)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + ("I" if self.off["ptr"] == 4 else "Q"), v))


class Ppc(Side):
    fmt = ">I"
    off = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def off32(self, a):
        return a

    def trk(self):
        self.e.trk = self.e.alloc(64)
        return self.e.trk

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_world(self, a):
        self.e.world = a

    def call(self, entry, args):
        r3, _, f1 = T.call_ppc(self.e, entry, args)
        return r3 & MASK, canon_f(fb(f1))


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = HOST_OFF
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def off32(self, a):
        return self.lib.tpw_lv_offset(a)

    def g(self, name):
        return self.lib.tpw_lv_addr(name.encode())

    def trk(self):
        a = self.g("trk")
        self.write(a, bytes(64))
        return a

    def alloc(self, n):
        return self.lib.tpw_lv_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_world(self, a):
        pass


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for n, res, args in (("addr", C.c_uint64, [C.c_char_p]), ("alloc", C.c_uint64, [C.c_uint32]), ("offset", C.c_uint32, [C.c_uint64]), ("layout", None, [C.POINTER(C.c_int64)]),
                         ("destroy", None, [C.c_uint32, C.c_uint32]), ("assign", None, [C.c_uint32] * 3 + [C.c_int32]),
                         ("height", C.c_uint32, [C.c_int32, C.c_int32, C.c_uint64]),
                         ("insets", None, [C.c_uint64, C.c_uint64, C.c_int32, C.c_uint64]), ("ride", C.c_int32, [C.c_int32, C.c_int32])):
        f = getattr(lib, "tpw_lv_" + n)
        f.restype, f.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = lib.tpw_lv_layout(out)
    v = list(out)[:n]
    HOST_OFF.update(heads=v[0], hmap=v[1], mhdr=v[2], mrot=v[3], entry=v[4], ptr=8)
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def rbytes(seed, n):
    return random.Random(seed).getrandbits(8 * n).to_bytes(n, "little")


def build_cells(s, case):
    W, H = case["W"], case["H"]
    size = 0x2D8 + 0x44 * 128 * H
    world = s.alloc(size)
    hs = struct.unpack(">%dH" % (size // 2), rbytes(case["fill"], size))
    s.write(world, struct.pack(s.fmt[0] + "%dH" % len(hs), *hs))
    s.set_world(world)
    s.ptr(s.g("world"), world)
    s.words(s.g("hw"), [W])
    s.words(s.g("hh"), [H])
    return s.trk()


def build_things(s, case):
    o = s.off
    s.half(s.g("noid"), case["noid"])
    for i in range(10):
        spec = case["things"][i]
        p = 0
        if spec is not None:
            p = s.alloc(0x40)
            s.write(p + 2, bytes([spec[0]]))
            s.words(p + 0x1C, [spec[1]])
        s.ptr(s.g("things") + i * o["entry"], p)
    for i in range(1, 10):
        spec = case["sets"][i]
        p = 0
        if spec is not None:
            p = s.alloc(0x200)
            mesh, header, node = s.alloc(0x40), s.alloc(0xB0), s.alloc(0x60)
            s.ptr(p + o["heads"], mesh)
            s.ptr(mesh + o["mhdr"], header)
            s.words(mesh + o["mrot"], [spec["rot"]])
            s.words(header + 0x78, [s.off32(node)])
            s.words(node + 0x40, [spec["nx"], spec["ny"], spec["nz"]][0:1])
            s.words(node + 0x44, [spec["ny"]])
            s.words(node + 0x48, [spec["nz"]])
            if spec["hmap"]:
                block, data = s.alloc(0x20), s.alloc(4096)
                s.write(data, rbytes(spec["seed"], 4096))
                s.half(block + 8, spec["w"])
                s.words(block + 0x10, [s.off32(data + 2048)])
                s.ptr(p + o["hmap"], block)
        s.ptr(s.g("sets") + i * o["ptr"], p)


def build_extra(s, case):
    if case["extra"] is None:
        return 0
    a = s.alloc(8)
    s.words(a, [case["extra"]])
    return a


def execute(s, case):
    op = case["op"]
    if op == "destroy":
        s.trkp = build_cells(s, case)
        call(s, FN_DESTROY, [case["a"] & MASK, case["b"] & MASK], lambda lib: lib.tpw_lv_destroy(case["a"] & MASK, case["b"] & MASK))
        return None, None
    if op == "assign":
        s.trkp = build_cells(s, case)
        args = [0, case["a"] & MASK, case["b"] & MASK, case["c"] & MASK, case["d"] & MASK]
        call(s, FN_ASSIGN, args, lambda lib: lib.tpw_lv_assign(case["a"] & MASK, case["b"] & MASK, case["c"] & MASK, s32(case["d"])))
        return None, None
    if op == "ride":
        build_things(s, case)
        return call(s, FN_RIDE, [case["x"] & MASK, case["y"] & MASK], lambda lib: lib.tpw_lv_ride(case["x"], case["y"]) & MASK), None
    if op == "height":
        build_things(s, case)
        extra = build_extra(s, case)
        ans = call(s, FN_HEIGHT, [case["x"] & MASK, case["y"] & MASK, extra], lambda lib: lib.tpw_lv_height(case["x"], case["y"], extra), True)
        return ans, (s.rd_words(extra, 1)[0] if extra else None)
    if op == "insets":
        corners, pts, out = s.alloc(32), s.alloc(max(12 * len(case["pts"]), 4)), s.alloc(32)
        s.words(corners, case["corners"])
        s.words(pts, [w for p in case["pts"] for w in p])
        s.words(out, case["out"])
        call(s, FN_INSETS, [corners, pts, len(case["pts"]), out], lambda lib: lib.tpw_lv_insets(corners, pts, len(case["pts"]), out))
        return None, s.rd_words(out, 8)


def call(s, entry, args, hostfn, isfloat=False):
    if isinstance(s, Ppc):
        r3, f1 = s.call(entry, args)
        return f1 if isfloat else r3
    r = hostfn(s.lib)
    if r is None:
        return None
    return canon_f(r) if isfloat else r & MASK


def run(s, case):
    try:
        answer, extra = execute(s, case)
        if case["op"] in ("destroy", "assign"):
            extra = s.rd_words(s.trkp, 16)
        return {"answer": answer, "extra": extra, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases


def queues(r):
    return {"valid": [r.choice([0, 1, 1])], "flag": [r.randrange(0x10000)], "mesh": [r.choice([0, 0, r.randrange(1, 10)])],
            "ovr": [r.choice([0, 1, 2, 3, 5, 9])], "quick": [fb(r.uniform(-100, 100))]}


def cell_case(r, op, a=None, b=None):
    W = r.choice([1, 2, 7, 64, 128, r.randrange(1, 129)])
    H = r.choice([1, 2, 3, r.randrange(1, 5)])
    pick = lambda hi: r.choice([-1, 0, hi - 1, hi, hi + 1, r.randrange(-3, hi + 4), 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF])
    c = {"op": op, "W": W, "H": H, "fill": r.getrandbits(32), "queues": queues(r), "a": pick(W) if a is None else a, "b": pick(H) if b is None else b,
         "c": r.getrandbits(32), "d": r.choice([0, 1, -1, 5])}
    return c


def thing_world(r):
    noid = r.choice([0, 0, 1, 3, 5])
    things = []
    for i in range(10):
        k = r.random()
        things.append(None if k < 0.25 else (r.choice([3, 3, 3, 0, 2, 4]), r.choice([0, r.randrange(1, 10), r.randrange(1, 10)])))
    if r.random() < 0.3:
        things[0] = None
    sets = {}
    for i in range(1, 10):
        if r.random() < 0.15:
            sets[i] = None
            continue
        sets[i] = {"rot": r.choice([0, 0x5A, 0xB4, 0x10E, 0x2D, -1, 0x100]), "nx": fb(r.uniform(-50, 150)), "ny": fb(r.uniform(-20, 20)),
                   "nz": fb(r.uniform(-50, 150)), "hmap": r.random() > 0.15, "w": r.randrange(1, 33), "seed": r.getrandbits(32)}
    return {"noid": noid, "things": things, "sets": sets}


def ride_case(r, x=None, y=None):
    c = {"op": "ride", "x": r.randrange(-3, 36) if x is None else x, "y": r.randrange(-3, 36) if y is None else y, "queues": queues(r)}
    c.update(thing_world(r))
    return c


def height_case(r, x=None, y=None):
    c = ride_case(r, x, y)
    c["op"] = "height"
    c["extra"] = r.choice([None, 0, r.randrange(1, 10), r.randrange(1, 10)])
    return c


def insets_case(r):
    def coord():
        return fb(float(r.randrange(-5, 6))) if r.random() < 0.5 else fb(r.uniform(-8, 8))
    n = r.choice([0, 1, 2, 3, 4, 5, 6])
    return {"op": "insets", "queues": {}, "corners": [coord() for _ in range(8)], "pts": [[coord(), coord(), coord()] for _ in range(n)],
            "out": [r.getrandbits(32) for _ in range(8)]}


def mk(gen, n_edge):
    def edges():
        r = random.Random(n_edge)
        for i in range(60):
            yield gen(r)

    return edges


def rnd_of(gen):
    return lambda r: gen(r)


def edges_cell(op):
    def e():
        r = random.Random(77)
        for W, H in ((1, 1), (128, 1), (128, 3), (5, 2)):
            for a in (-1, 0, W - 1, W, W + 1, 0xFFFFFFFF):
                for b in (-1, 0, H - 1, H, H + 1):
                    c = cell_case(r, op, a, b)
                    c["W"], c["H"] = W, H
                    yield c

    return e


def edges_height():
    r = random.Random(31)
    for x in (-3, 0, 5, 20, 35):
        for y in (-3, 0, 7, 35):
            yield height_case(r, x, y)
    for i in range(80):
        yield height_case(r)


TESTS = [
    FnTest("Destroy3DObjectFromAIWorld", ppc, host, edge=edges_cell("destroy"), rnd=rnd_of(lambda r: cell_case(r, "destroy"))),
    FnTest("Assign3DObjectToAIWorld", ppc, host, edge=edges_cell("assign"), rnd=rnd_of(lambda r: cell_case(r, "assign"))),
    FnTest("GetWorldGSquareHeight", ppc, host, edge=edges_height, rnd=rnd_of(height_case)),
    FnTest("DetermineInnerBaseInsets", ppc, host, edge=mk(insets_case, 5), rnd=rnd_of(insets_case)),
    FnTest("GetRideIDAtCell", ppc, host, edge=mk(ride_case, 9), rnd=rnd_of(ride_case)),
]
