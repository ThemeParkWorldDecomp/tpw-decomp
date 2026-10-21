"""Unit v5082, the cell system's rectangle clearer and hoarding walk, against libtpw_unit_LLCC_Kill_Them_KILL_THEM_ALL.dylib
(tests/unit_LLCC_Kill_Them_KILL_THEM_ALL.cpp), main app ("SimTheme Park"), src/game/park/LLCellRide.cpp:
  LLCC_Kill_Them_KILL_THEM_ALL 0x1008ebc0, LLCC_BuildHoardingDB 0x1008e6e0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the cell predicates and mutators, IsXYValid, LLCR_DeleteRide, OPPOSITE, ADD_DIRECTION, GetNeighbouringCell, the image redraw) are
fakes on both sides that record their arguments (cells by their id halfword) and answer from the case's queues; OPPOSITE and ADD_DIRECTION
are pure fakes (rotate by 4, rotate by the lowest set bit of the second argument), the same code on both sides.

The world of a case: a block of the size of the Mac's world (map cells 0x44 bytes at +0x2d8, track cells 0x28 bytes at +0x1102d8, a 24 x 24
corner of the 128 wide grid written with ids, modes and parents), gGameWorld, the flag byte gLLCC4818, the three compass offsets and the
four extent words. What is compared: the answer, the four extent words and the event log.

No fused operations. Not tested: a walk that never reaches its target (the original loops for ever), a crossroad whose direction byte is 0
(its search loop never ends), cells outside the 24 x 24 corner of the grid.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCC_Kill_Them_KILL_THEM_ALL"
MASK = 0xFFFFFFFF

FN_KILL, FN_BUILD = 0x1008EBC0, 0x1008E6E0
SIZES = {FN_KILL: 1212, FN_BUILD: 1196}
OWN = set(SIZES)

ADDR = dict(world=0x1037EBFC, c4818=0x102E4818, minx=0x102DE0C8, miny=0x102DE0C4, maxx=0x102DE0C0, maxy=0x102DE0BC,
            dplusy=0x1034C25C, dplusx=0x1034C284, dplusxy=0x1034C270)
HOST_LAYOUT = [0x44, 0x28, 0, 8, 0x10]
N = 24
WORLD_SIZE = 0x1102D8 + 0x28 * 3100


def s32(v):
    return T.s32(v)


def rotl8(a, s):
    s &= 7
    return ((a << s) | (a >> (8 - s))) & 0xFF


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cid(e, a):
    return struct.unpack(">H", e.read(a, 2))[0]


def fake(addr, name, args, ret=None):
    """args: list of (register, kind) kinds: 'c' cell id, 'i' s32, 'h' u16, 'b' u8, 'w' high half; ret: None or 'i'/'b'."""
    def stub(e):
        vals = []
        for reg, kind in args:
            v = e.r(reg)
            if kind == "c":
                vals.append(cid(e, v))
            elif kind == "i":
                vals.append(s32(v))
            elif kind == "h":
                vals.append(v & 0xFFFF)
            elif kind == "b":
                vals.append(v & 0xFF)
            elif kind == "w":
                vals.append((v >> 16) & 0xFFFF)
        e.cl.rec(name, tuple(vals))
        if ret == "i":
            e.set_r(3, e.cl.ret(name) & MASK)
        elif ret == "b":
            e.set_r(3, e.cl.ret(name) & 0xFF)
    STUBS.internal(addr, stub)


fake(0x1009BB70, "IsFlag", [(3, "c"), (4, "h")], "i")
fake(0x1009C44C, "IsPath", [(3, "c")], "i")
fake(0x1009D0F4, "Delete", [(3, "c"), (4, "i"), (5, "i")])
fake(0x101012F4, "IsXYValid", [(3, "i"), (4, "i")], "i")
fake(0x100892C4, "DeleteRide", [(3, "i"), (4, "i"), (5, "i")])
fake(0x10081744, "NumberOfNeighbours", [(3, "c")], "i")
fake(0x100816E8, "GetNeighbours", [(3, "c")], "b")
fake(0x10081930, "GetDirection", [(3, "c")], "b")
fake(0x10081810, "ModifyDirection", [(3, "c"), (4, "b")])
fake(0x10081678, "RemoveNeighbour", [(3, "c"), (4, "b")])
fake(0x100A37A0, "IsCorner", [(3, "c")], "b")
fake(0x100A3914, "IsCrossRoad", [(3, "c")], "b")
fake(0x1009DFE4, "Modify", [(3, "c"), (4, "i"), (5, "i"), (6, "i")])
fake(0x100A3BAC, "SetHFieldTile", [(3, "c")])
fake(0x100A0828, "ModifyType", [(3, "c"), (4, "i")])
fake(0x100A0B68, "ModifyParentID", [(3, "c"), (4, "w")])
fake(0x1009668C, "CreateCurrentHoardingImage", [])


def stub_opposite(e):
    d = e.r(3) & 0xFF
    e.cl.rec("OPPOSITE", (d,))
    e.set_r(3, rotl8(d, 4))


def stub_add(e):
    a, b = e.r(3) & 0xFF, e.r(4) & 0xFF
    e.cl.rec("ADD_DIRECTION", (a, b))
    s = ((b & -b).bit_length() - 1) & 7 if b else 0
    e.set_r(3, rotl8(a, s))


def stub_neighbour(e):
    cell, p, off = e.r(3), e.r(4), e.r(5)
    e.cl.rec("GetNeighbouringCell", (cid(e, cell), s32(e.u32(p)), s32(e.u32(p + 4)), s32(off)))
    i = e.cl.ret("GetNeighbouringCell")
    e.set_r(3, 0 if i == 0 else (e.u32(ADDR["world"]) + 0x2D8 + 0x110000 + (i - 1) * 0x28) & MASK)


STUBS.internal(0x10102500, stub_opposite)
STUBS.internal(0x1010254C, stub_add)
STUBS.internal(0x1012FCB8, stub_neighbour)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"
    ppc = True

    def __init__(self, e, case):
        self.e = e
        e.cl = CL.PpcLog({}, case["queues"])
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
        return self.e.cl.log

    def kill(self, a, b, c, d):
        r3, _, _ = T.call_ppc(self.e, FN_KILL, [a & MASK, b & MASK, c & MASK, d & MASK])
        return r3 & MASK

    def build(self, a, b, c, d, mode, flag):
        r3, _, _ = T.call_ppc(self.e, FN_BUILD, [a & MASK, b & MASK, c & MASK, d & MASK, mode & 0xFF, flag & 1])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    ppc = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_kt_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_kt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def kill(self, a, b, c, d):
        return self.lib.tpw_kt_kill(s32(a), s32(b), s32(c), s32(d)) & MASK

    def build(self, a, b, c, d, mode, flag):
        return self.lib.tpw_kt_build(s32(a), s32(b), s32(c), s32(d), mode & 0xFF, flag & 1) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_kt_addr.restype, lib.tpw_kt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_kt_setworld.restype, lib.tpw_kt_setworld.argtypes = None, [C.c_uint64]
    lib.tpw_kt_alloc.restype, lib.tpw_kt_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_kt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_kt_kill.restype, lib.tpw_kt_kill.argtypes = C.c_uint32, [C.c_int32] * 4
    lib.tpw_kt_build.restype, lib.tpw_kt_build.argtypes = C.c_uint32, [C.c_int32] * 4 + [C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_kt_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    E = s.fmt[0]
    world = s.alloc(WORLD_SIZE)
    if s.ppc:
        s.words(s.g("world"), [world])
    else:
        s.lib.tpw_kt_setworld(world)
    s.write(s.g("c4818"), bytes([case["c4818"]]))
    s.words(s.g("minx"), [case["init"][0]])
    s.words(s.g("miny"), [case["init"][1]])
    s.words(s.g("maxx"), [case["init"][2]])
    s.words(s.g("maxy"), [case["init"][3]])
    if s.ppc:
        s.words(s.g("dplusy"), [0, 1])
        s.words(s.g("dplusx"), [1, 0])
        s.words(s.g("dplusxy"), [1, 1])
    for y in range(N):
        mrow = bytearray(N * 0x44)
        trow = bytearray(N * 0x28)
        for x in range(N):
            i = 1 + x + (y << 7)
            typ, parent = case["map"].get((x, y), (0, 0))
            struct.pack_into(E + "H", mrow, x * 0x44, i)
            struct.pack_into(E + "I", mrow, x * 0x44 + 8, typ)
            struct.pack_into(E + "H", mrow, x * 0x44 + 0x10, parent)
            struct.pack_into(E + "H", trow, x * 0x28, i)
            struct.pack_into(E + "I", trow, x * 0x28 + 8, case["tmap"].get((x, y), case["ttype"]))
        s.write(world + 0x2D8 + (y << 7) * 0x44, bytes(mrow))
        s.write(world + 0x1102D8 + (y << 7) * 0x28, bytes(trow))


def execute(s, case):
    if case["op"] == "kill":
        return s.kill(*case["args"])
    return s.build(*case["args"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "minx": s.rd_words(s.g("minx"), 1), "miny": s.rd_words(s.g("miny"), 1),
            "maxx": s.rd_words(s.g("maxx"), 1), "maxy": s.rd_words(s.g("maxy"), 1)}


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


def queue(r, choices, n=48, last=None):
    q = [r.choice(choices) for _ in range(n)]
    if last is not None:
        q[-1] = last
    return q


def rand_init(r):
    return [r.choice([0, 5, 0x400, -1, r.randrange(-50, 2000)]) & MASK for _ in range(4)]


def kill_case(r, args=None, flagp=0.04, pathp=0.4, xyp=0.1, margin=None, uniform=True):
    xa, xb = r.randrange(2, 7), r.randrange(2, 7)
    ya, yb = r.randrange(2, 7), r.randrange(2, 7)
    if r.random() < 0.3:
        xb, yb = xa, ya
    if args is None:
        args = [xa, ya, xb, yb]
    xs = (min(args[0], args[2]), max(args[0], args[2]))
    ys = (min(args[1], args[3]), max(args[1], args[3]))
    m = margin if margin is not None else [r.choice([0, 0, 0, 1]) for _ in range(4)]
    parent = r.choice([1, 2])
    cells = {}
    for y in range(ys[0] - m[2], ys[1] + m[3] + 1):
        for x in range(xs[0] - m[0], xs[1] + m[1] + 1):
            t = 4 if r.random() < 0.55 else r.choice([9, 10, 5, 11, 0, 8, 4, 12])
            if not uniform and r.random() < 0.2:
                t = r.choice([4, 9, 10])
            p = parent if r.random() < 0.9 else 3 - parent
            cells[(x, y)] = (t, p)
    if r.random() < 0.3:
        cells[(r.randrange(1, 9), r.randrange(1, 9))] = (r.choice([4, 9, 10, 5]), r.choice([1, 2]))
    q = {"IsFlag": queue(r, [0] * int(1 / max(flagp, 0.01)) + [1], last=0), "IsPath": queue(r, [0, 1] if pathp > 0.3 else [0], last=r.choice([0, 1])),
         "IsXYValid": queue(r, [1] * int(1 / max(xyp, 0.01)) + [0], last=1)}
    return {"op": "kill", "args": args, "map": cells, "tmap": {}, "ttype": 0, "c4818": 0, "init": rand_init(r), "queues": q}


def kill_edges():
    r = random.Random(5082)
    yield kill_case(r, [3, 3, 3, 3], margin=[0, 0, 0, 0])
    yield kill_case(r, [5, 5, 2, 2], margin=[0, 0, 0, 0])
    yield kill_case(r, [2, 5, 5, 2], margin=[0, 0, 0, 0])
    yield kill_case(r, [2, 2, 5, 5], margin=[1, 0, 0, 0])
    yield kill_case(r, [2, 2, 5, 5], margin=[0, 1, 0, 0])
    yield kill_case(r, [2, 2, 5, 5], margin=[0, 0, 1, 0])
    yield kill_case(r, [2, 2, 5, 5], margin=[0, 0, 0, 1])
    yield kill_case(r, [2, 2, 4, 4], margin=[0, 0, 0, 0], flagp=1, pathp=1)
    yield kill_case(r, [2, 2, 4, 4], margin=[0, 0, 0, 0], xyp=1)
    for _ in range(8):
        yield kill_case(r, margin=[0, 0, 0, 0])


def kill_random(r):
    return kill_case(r)


DIRV = {1: (0, -2), 4: (2, 0), 0x10: (0, 2), 0x40: (-2, 0)}


def build_case(r, k=0, d=None, flag=None, corner=False, cross=False):
    if d is None:
        d = r.choice([0, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, r.randrange(256), r.choice([1, 4, 0x10, 0x40])])
    if k > 0:
        d = r.choice([1, 4, 0x10, 0x40])
    flag = r.choice([0, 1]) if flag is None else flag
    x, y = r.randrange(7, 15), r.randrange(7, 15)
    tx, ty = x + DIRV.get(d, (0, 0))[0] * k, y + DIRV.get(d, (0, 0))[1] * k
    if k == 0 and r.random() < 0.3:
        tx, ty = x, y
    q = {"NumberOfNeighbours": queue(r, [0, 1, 2, 5]),
         "IsCorner": queue(r, [0, 0xA5] if corner else [0], last=0xA5 if corner else 0),
         "IsCrossRoad": queue(r, [0xC3] if cross else [0], last=0xC3 if cross else 0),
         "GetNeighbours": queue(r, [r.randrange(256) for _ in range(8)]),
         "GetDirection": queue(r, [0, d, 1, 4, 0x10, 0x40, r.randrange(256)]),
         "GetNeighbouringCell": queue(r, [0, 0, 1 + r.randrange(N) + (r.randrange(N) << 7)])}
    if cross:
        bit = d if d and (d & (d - 1)) == 0 else r.choice([1, 4, 0x10, 0x40])
        if d == 0 or d & (d - 1):
            d = bit
        n = r.choice([bit, bit | 0x22, bit | r.randrange(256), rotl8(bit, 6) | r.choice([0, 0x80]), 0xFF & ~rotl8(bit, 6) | bit])
        n &= ~rotl8(bit, 6) if r.random() < 0.5 else 0xFF
        n |= bit
        q["GetNeighbours"] = [n]
    elif corner:
        q["GetNeighbours"] = queue(r, [r.randrange(1, 256) for _ in range(8)])
    ttype = 0x19 if (k > 0 or r.random() < 0.7) else r.choice([5, 0xC, 0x11])
    return {"op": "build", "args": [x, y, tx, ty, d, flag], "map": {}, "tmap": {}, "ttype": ttype, "c4818": r.choice([0, 1, 1]),
            "init": rand_init(r), "queues": q}


def build_edges():
    r = random.Random(50821)
    for d in (0, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80):
        yield build_case(r, 0, d)
    for d in (1, 4, 0x10, 0x40):
        for k in (1, 2, 3):
            yield build_case(r, k, d, flag=r.choice([0, 1]))
    for d in (1, 4, 0x10, 0x40, 2, 0x80):
        for _ in range(3):
            yield build_case(r, 0, d, corner=True)
            yield build_case(r, 0, d, cross=True)


def build_random(r):
    k = r.choice([0, 0, 1, 2, 3])
    kind = r.random()
    return build_case(r, k, corner=(k == 0 and kind < 0.3), cross=(k == 0 and 0.3 <= kind < 0.7))


TESTS = [
    FnTest("LLCC_Kill_Them_KILL_THEM_ALL", ppc, host, edge=kill_edges, rnd=kill_random),
    FnTest("LLCC_BuildHoardingDB", ppc, host, edge=build_edges, rnd=build_random),
]
