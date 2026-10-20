"""Unit v5687, the track breaker and the track mesh getter, against libtpw_unit_LLCC_BreakTrack.dylib (tests/unit_LLCC_BreakTrack.cpp), main app
("SimTheme Park"), src/TrackCell_flags_CTrackCell_SetHFieldTile_LLCC_BreakTrack.cpp:
  LLCC_BreakTrack 0x100a510c, LLCC_GetTrackMeshAt 0x100a55bc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the CCellBase methods,
SetHFieldTile, GetNeighbouringCell, OPPOSITE, CMap::IsXYValid, CTRACE) are fakes on both sides that record their arguments (a cell as its
index in the track grid) and answer from the case's queues.

The world of a case: gGameWorld pointing at a full world block whose track cells (1500 of them, plus the one before the grid) get random
type, parent id and mesh word; the first block, track end and no-parent globals. What is compared: the answer (as a cell index) and the
event log. No fused operations. Not tested: a cell index past the 1500 written cells (the cells there are zero in both memories anyway).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCC_BreakTrack"
MASK = 0xFFFFFFFF

FN_BREAK, FN_MESH = 0x100a510c, 0x100a55bc
SIZES = {FN_BREAK: 1156, FN_MESH: 224}
OWN = set(SIZES)

GETDIR, GETNB, REMNB, SETHF, GETOVL, DECOVL, GETNC, OPPOSITE, ISXY, MODNB, CTRACE = (
    0x10081930, 0x100816e8, 0x10081678, 0x100a3bac, 0x10081a5c, 0x100819d8, 0x1012fddc, 0x10102500, 0x101012f4, 0x10081608, 0x1006fcbc)

ADDR = dict(gw=0x1037ebfc, fbx=0x1029e02c, fby=0x1029e030, e034=0x1029e034, e038=0x1029e038, e380=0x1029e380)
HOST_LAYOUT = [0x1102d8, 0x28, 4, 8, 0x10]

NCELLS = 1500
WORLD_SIZE = 0x1102d8 + 0xa0000 + 0x100


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def widx(e, a):
    w = e.u32(ADDR["gw"])
    return ((a & MASK) - w - 0x1102d8) // 0x28


def cell_stub(name, extra=(), ret=None):
    """A fake of a method: records the cell index and the listed argument registers; ret is a queue key or None."""
    def fn(e):
        args = [widx(e, e.r(3))]
        for r, kind in extra:
            args.append(e.r(r) & 0xFF if kind == "b" else s32(e.r(r)))
        e.cl.rec(name, tuple(args))
        if ret:
            e.set_r(3, e.cl.ret(ret) & MASK)
    return fn


def stub_getnc(e):
    L = e.cl
    L.rec("GetNeighbouringCell", (widx(e, e.r(3)), e.r(4) & 0xFF, s32(e.r(5))))
    v = L.ret("GetNeighbouringCell")
    e.set_r(3, 0 if v == 0 else (e.u32(ADDR["gw"]) + 0x1102d8 + (v - 1) * 0x28) & MASK)


def stub_opposite(e):
    d = e.r(3) & 0xFF
    e.cl.rec("OPPOSITE", (d,))
    e.set_r(3, ((d << 4) | (d >> 4)) & 0xFF)


def stub_isxy(e):
    e.cl.rec("IsXYValid", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("IsXYValid") & MASK)


STUBS.internal(GETDIR, cell_stub("GetDirection", (), "GetDirection"))
STUBS.internal(GETNB, cell_stub("GetNeighbours", (), "GetNeighbours"))
STUBS.internal(REMNB, cell_stub("RemoveNeighbour", ((4, "b"),)))
STUBS.internal(SETHF, cell_stub("SetHFieldTile"))
STUBS.internal(GETOVL, cell_stub("GetOverlapCounter", (), "GetOverlapCounter"))
STUBS.internal(DECOVL, cell_stub("DecOverlapCounter"))
STUBS.internal(MODNB, cell_stub("ModifyNeighbours", ((4, "b"),)))
STUBS.internal(GETNC, stub_getnc)
STUBS.internal(OPPOSITE, stub_opposite)
STUBS.internal(ISXY, stub_isxy)
STUBS.internal(CTRACE, CL.recorder("CTRACE", "us"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


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

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def cellidx(self, a):
        return ((a & MASK) - self.w - 0x1102d8) // 0x28

    def brk(self, x, y, d):
        r3, _, _ = T.call_ppc(self.e, FN_BREAK, [x & MASK, y & MASK, d & 0xFF])
        return self.cellidx(r3)

    def mesh(self, x, y):
        r3, _, _ = T.call_ppc(self.e, FN_MESH, [x & MASK, y & MASK])
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
        return self.lib.tpw_llcb_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_llcb_alloc(n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def cellidx(self, a):
        return (a - self.w - 0x1102d8) // 0x28

    def brk(self, x, y, d):
        return self.cellidx(self.lib.tpw_llcb_break(s32(x), s32(y), d & 0xFF))

    def mesh(self, x, y):
        return self.lib.tpw_llcb_mesh(s32(x), s32(y)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_llcb_"
    lib.tpw_llcb_addr.restype, lib.tpw_llcb_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_llcb_alloc.restype, lib.tpw_llcb_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_llcb_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_llcb_break.restype, lib.tpw_llcb_break.argtypes = C.c_uint64, [C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_llcb_mesh.restype, lib.tpw_llcb_mesh.argtypes = C.c_uint32, [C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_llcb_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.w = s.alloc(WORLD_SIZE)
    if s.fmt == "<I":
        s.write(s.g("gw"), struct.pack("<Q", s.w))      # a native pointer
    else:
        s.words(s.g("gw"), [s.w & MASK])
    s.words(s.g("fbx"), [case["fbx"]])
    s.words(s.g("fby"), [case["fby"]])
    s.words(s.g("e034"), [case["e034"]])
    s.words(s.g("e038"), [case["e038"]])
    s.write(s.g("e380"), struct.pack(s.fmt[0] + "H", case["e380"]))
    e = s.fmt[0]
    data = b"".join(struct.pack(e + "IIIIHH5I", 0, m, t, 0, p, 0, 0, 0, 0, 0, 0) for (m, t, p) in case["cells"])
    s.write(s.w + 0x1102d8 - 0x28, data)


def observe(s, answer):
    return {"answer": answer, "log": s.log()}


def run(s, case):
    try:
        build(s, case)
        if case["op"] == "brk":
            ans = s.brk(*case["args"])
        else:
            ans = s.mesh(*case["args"])
        return observe(s, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


TYPES = [0xd, 0x12, 0, 0xc, 0x11, 3, 0x19]


def make_cells(r, mode):
    cells = []
    for _ in range(NCELLS + 1):
        k = r.random()
        t = r.choice(TYPES)
        if mode == "mesh":
            t = r.choice([0xc, 0x11, 0xc, 0x11, 0, 5, 0xd])
        p = r.choice([0, 1, r.randrange(1, NCELLS), r.randrange(0, 0x10000)]) if mode == "mesh" else r.randrange(0, 0x10000)
        if mode == "mesh" and p >= NCELLS:
            p = r.randrange(0, NCELLS)
        cells.append((r.randrange(0, MASK + 1), t, p))
    return cells


def qv(r, lo, hi, n=40):
    return [r.randrange(lo, hi + 1) for _ in range(n)]


def base(r, op):
    x, y = r.randrange(2, 10), r.randrange(2, 10)
    dirs = [1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0, r.randrange(256)]
    fbx, fby = r.randrange(0, 12), r.randrange(0, 12)
    k = r.random()
    if k < 0.6:
        dx, dy = r.choice([(2, 0), (0, 2), (-2, 0), (0, -2)])
        fbx, fby = x + dx, y + dy
    e034, e038 = r.randrange(0, 12), r.randrange(0, 12)
    if r.random() < 0.3:
        e034, e038 = x, y
    c = {"op": op, "args": [x, y], "fbx": fbx, "fby": fby, "e034": e034 & MASK, "e038": e038 & MASK,
         "e380": r.choice([0, 0xffff, r.randrange(0, NCELLS)])}
    if op == "brk":
        c["args"].append(r.choice(dirs))
        c["cells"] = make_cells(r, "brk")
        gd = r.choice([0, 1, 4, 0x10, 0x55, r.randrange(256)])
        first_null_ok = c["args"][2] != 0 or gd != 0
        c["queues"] = {
            "GetDirection": [gd],
            "GetNeighbours": [r.choice([0, 0xff, 5, r.randrange(256)])],
            "GetOverlapCounter": [r.choice([0, 0, 1, -1, 7, -32768])],
            # the first answer (the cell beyond) may be NULL; the later ones are dereferenced unchecked by the original
            "GetNeighbouringCell": [r.choice([0, 0, r.randrange(1, NCELLS)]) if first_null_ok else r.randrange(1, NCELLS)] + [r.randrange(1, NCELLS) for _ in range(20)],
            "IsXYValid": [r.choice([0, 1, 1])],
        }
    else:
        c["cells"] = make_cells(r, "mesh")
        c["queues"] = {}
        c["args"] = [r.randrange(0, 12), r.randrange(0, 8)]
    return c


def brk_edges():
    r = random.Random(5687)
    for i in range(60):
        yield base(r, "brk")


def brk_random(r):
    return base(r, "brk")


def mesh_edges():
    r = random.Random(5688)
    for i in range(40):
        c = base(r, "mesh")
        x, y = c["args"]
        idx = x + (y << 7)
        # make the parent of this cell equal to gLLCCE380 and sometimes the cell a track X cell
        m, t, p = c["cells"][idx + 1]
        if i % 3 == 0:
            c["cells"][idx + 1] = (m, 0xc if i % 2 else 0x11, c["e380"])
        elif i % 3 == 1:
            c["cells"][idx + 1] = (m, 0x11, (c["e380"] + 1) % NCELLS)
        yield c


def mesh_random(r):
    return base(r, "mesh")


TESTS = [
    FnTest("LLCC_BreakTrack", ppc, host, edge=brk_edges, rnd=brk_random),
    FnTest("LLCC_GetTrackMeshAt", ppc, host, edge=mesh_edges, rnd=mesh_random),
]
