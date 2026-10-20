"""Unit v5270, five CMapCell functions, against libtpw_unit_CMapCell_IsHoarding.dylib (tests/unit_CMapCell_IsHoarding.cpp), main app,
src/MapDelta_map_CMapCell_IsHoarding.cpp:
  IsHoarding 0x1009ca9c, IsTrack 0x1009c96c, RefreshHFieldTile 0x1009cf64, ValidTypeChange 0x1009b054, CalculateDirections 0x1009af80

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Callees outside the unit are fakes on both sides that
record their arguments: GetTrackCellEquiv (answers the case's equivalent cell), IsTrackX / IsTrack / IsHoarding (answer from queues; the
pointer is logged as a label: 1 the equivalent cell, 2 the cell, 100 + index for the track table entry), ModifyDirection, LLCCM_CreateQMesh
(answers from a queue), SingleHFieldTextureChange (logs the tile it points at).

The world of a case: gGameWorld (a base number; nothing is read through it), the sentinel 0x1029e224, the lock 0x102e4834, the cell and the
equivalent cell. What is compared: the answer, the log, the mesh word and the tile words of the cell, the globals.

No fused operations. Not tested: nothing is left out (the track table is never read, only addressed).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CMapCell_IsHoarding"
MASK = 0xFFFFFFFF

FN_HOARD, FN_TRACK, FN_REFRESH, FN_VALID, FN_DIR = 0x1009CA9C, 0x1009C96C, 0x1009CF64, 0x1009B054, 0x1009AF80
SIZES = {FN_HOARD: 128, FN_TRACK: 176, FN_REFRESH: 228, FN_VALID: 200, FN_DIR: 160}
OWN = set(SIZES)

GET_EQUIV, MODIFY_DIR, IS_TRACKX, IS_TRACK, IS_HOARDING, CREATE_QMESH, SINGLE_TILE = (
    0x100F4844, 0x10081810, 0x100A367C, 0x100A3574, 0x100A35DC, 0x10081C58, 0x1005B840)

ADDR = dict(world=0x1037EBFC, noparent=0x1029E224, lock=0x102E4834)
HOST_LAYOUT = [0x44, 0, 4, 8, 0x14, 0x10, 0x28]

PTR = {}      # the pointers of the case on the PowerPC side: equiv, cell, world base


def s32(v):
    return T.s32(v)


def lab(p):
    if p and p == PTR.get("equiv"):
        return 1
    if p and p == PTR.get("cell"):
        return 2
    return (p - (PTR["world"] + 0x1102D8)) // 0x28 + 100


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_get_equiv(e):
    e.cl.rec("GetTrackCellEquiv", (lab(e.r(3)),))
    e.set_r(3, PTR["equiv"])


def stub_modify_dir(e):
    e.cl.rec("ModifyDirection", (lab(e.r(3)), e.r(4) & 0xFF))


def mk_bool_stub(name):
    def stub(e):
        e.cl.rec(name, (lab(e.r(3)),))
        e.set_r(3, e.cl.ret(name) & MASK)
    return stub


def stub_create(e):
    e.cl.rec("CreateQMesh", (s32(e.r(3)), s32(e.r(4)), e.r(5) & MASK, e.r(6) & MASK, e.r(7) & MASK, s32(e.r(8))))
    e.set_r(3, e.cl.ret("CreateQMesh") & MASK)


def stub_single(e):
    k, i, r = struct.unpack(">3I", e.read(e.r(5), 12))
    e.cl.rec("SingleHFieldTextureChange", (s32(e.r(3)), s32(e.r(4)), k, i, r))


STUBS.internal(GET_EQUIV, stub_get_equiv)
STUBS.internal(MODIFY_DIR, stub_modify_dir)
STUBS.internal(IS_TRACKX, mk_bool_stub("IsTrackX"))
STUBS.internal(IS_TRACK, mk_bool_stub("IsTrack"))
STUBS.internal(IS_HOARDING, mk_bool_stub("IsHoarding"))
STUBS.internal(CREATE_QMESH, stub_create)
STUBS.internal(SINGLE_TILE, stub_single)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        PTR.clear()
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def setptr(self, name, a):
        PTR[name] = a

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        r3 = T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK
        return 0 if entry in (FN_REFRESH, FN_DIR) else r3      # void: r3 is whatever was left


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v5270_addr(name.encode())

    def setptr(self, name, a):
        self.write(self.g(name), struct.pack("<Q", a))

    def alloc(self, n):
        return self.lib.tpw_v5270_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        name = {FN_HOARD: "IsHoarding", FN_TRACK: "IsTrack", FN_REFRESH: "RefreshHFieldTile", FN_VALID: "ValidTypeChange",
                FN_DIR: "CalculateDirections"}[entry]
        r = getattr(self.lib, "tpw_v5270_" + name)(args[0], *[s32(a) for a in args[1:]])
        return (r or 0) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v5270_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for n in ("IsHoarding", "IsTrack"):
        f = getattr(lib, p + n)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64]
    f = getattr(lib, p + "RefreshHFieldTile")
    f.restype, f.argtypes = None, [C.c_uint64]
    f = getattr(lib, p + "ValidTypeChange")
    f.restype, f.argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    f = getattr(lib, p + "CalculateDirections")
    f.restype, f.argtypes = None, [C.c_uint64, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.words(s.g("world"), [case["world"]])
    s.half(s.g("noparent"), case["noparent"])
    s.words(s.g("lock"), [case["lock"]])
    cell = s.alloc(0x44)
    equiv = s.alloc(0x28)
    s.half(cell + 0, case["id"])
    s.words(cell + 8, [case["mode"]])
    s.words(cell + 0x14, case["tile"])
    s.half(equiv + 0x10, case["parent"])
    s.setptr("cell", cell)
    s.setptr("equiv", equiv)
    PTR["world"] = case["world"]      # the Python side's own copy, for lab()
    return cell


def observe(s, case, cell, answer):
    return {"answer": answer, "log": s.log(), "mesh": s.rd_word(cell + 4), "tile": s.rd_words(cell + 0x14, 3),
            "mode": s.rd_word(cell + 8), "id": s.rd_half(cell), "lock": s.rd_word(s.g("lock")), "noparent": s.rd_half(s.g("noparent"))}


def run(s, case):
    try:
        cell = build(s, case)
        if isinstance(s, Ppc):
            PTR["world"] = case["world"]
        entry = case["entry"]
        args = [cell] + case["args"]
        return observe(s, case, cell, s.call(entry, args))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

MODES = [0, 1, 2, 3, 4, 5, 7, 0xC, 0x11, 0x15, 0x19, 0x1E, -1, 100]


def base(r, entry, args=()):
    return {"entry": entry, "args": list(args),
            "world": r.choice([0x10000000, 0x20000000, r.randrange(0x1000000, 0x60000000) & ~3]),
            "noparent": r.choice([0, 0, 5, 0xFFFF, r.randrange(0, 12)]),
            "lock": r.choice([0, 0, 1, -1, r.randrange(0, 3)]),
            "id": r.choice([0, 1, 2, 127, 128, 129, 0x4000, 0xFFFF, r.randrange(0, 0x10000)]),
            "mode": r.choice(MODES), "tile": [r.choice([2, 2, 0, 1, 0xFFFFFFFF, r.randrange(0, 5)]), r.randrange(0, 1 << 32), r.randrange(0, 1 << 32)],
            "parent": r.choice([0, 1, 2, 5, 0xFFFF, r.randrange(0, 12), r.randrange(0, 0x10000)]),
            "queues": {"IsTrackX": [r.choice([0, 1])], "IsTrack": [r.choice([0, 1, 7, 0xFFFFFFFF, r.randrange(0, 1 << 32)])],
                       "IsHoarding": [r.choice([0, 1, 5, 0xFFFFFFFF, r.randrange(0, 1 << 32)])],
                       "CreateQMesh": [r.choice([0, 1, r.randrange(0, 1 << 32)])]}}


def pred_case(entry):
    def make(r, x=None, parent=None, np=None):
        c = base(r, entry)
        if x is not None:
            c["queues"]["IsTrackX"] = [x]
        if parent is not None:
            c["parent"] = parent
        if np is not None:
            c["noparent"] = np
        return c
    return make


def pred_edges(entry, seed):
    def gen():
        r = random.Random(seed)
        mk = pred_case(entry)
        for x in (0, 1):
            for parent in (0, 1, 2, 7, 0xFFFF):
                for np in (0, 7, 0xFFFF):
                    yield mk(r, x, parent, np)
    return gen


def pred_random(entry):
    return lambda r: pred_case(entry)(r)


def refresh_case(r, id_=None, kind=None):
    c = base(r, FN_REFRESH)
    if id_ is not None:
        c["id"] = id_
    if kind is not None:
        c["tile"][0] = kind
    return c


def refresh_edges():
    r = random.Random(52)
    for id_ in (0, 1, 2, 127, 128, 129, 0x4000, 0x7FFF, 0xFFFF):
        for kind in (2, 0, 1, 3):
            yield refresh_case(r, id_, kind)


def valid_case(r, mode=None, cur=None):
    c = base(r, FN_VALID, [r.choice(MODES)])
    if mode is not None:
        c["args"] = [mode]
    if cur is not None:
        c["mode"] = cur
    return c


def valid_edges():
    r = random.Random(53)
    for m in (0, 1, 3, 4, 5, 0x15, -1):
        for cur in (0, 1, 3, 4, 5, 0x15, 2):
            for lock in (0, 1):
                c = valid_case(r, m, cur)
                c["lock"] = lock
                yield c


def dir_case(r, a=None, b=None):
    return base(r, FN_DIR, [r.choice([-1, 0, 1, 2, -2, r.randrange(-5, 5)]) if a is None else a,
                            r.choice([-1, 0, 1, 2, -2, r.randrange(-5, 5)]) if b is None else b])


def dir_edges():
    r = random.Random(54)
    for a in (-1, 0, 1, 2):
        for b in (-1, 0, 1, 2):
            yield dir_case(r, a, b)


TESTS = [
    FnTest("CMapCell::IsHoarding", ppc, host, edge=pred_edges(FN_HOARD, 50), rnd=pred_random(FN_HOARD)),
    FnTest("CMapCell::IsTrack", ppc, host, edge=pred_edges(FN_TRACK, 51), rnd=pred_random(FN_TRACK)),
    FnTest("CMapCell::RefreshHFieldTile", ppc, host, edge=refresh_edges, rnd=lambda r: refresh_case(r)),
    FnTest("CMapCell::ValidTypeChange", ppc, host, edge=valid_edges, rnd=lambda r: valid_case(r)),
    FnTest("CMapCell::CalculateDirections", ppc, host, edge=dir_edges, rnd=lambda r: dir_case(r)),
]
