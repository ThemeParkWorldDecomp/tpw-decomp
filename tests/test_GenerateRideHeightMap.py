"""Unit v5675, the ride height map generator, against libtpw_unit_GenerateRideHeightMap.dylib (tests/unit_GenerateRideHeightMap.cpp), main app
("SimTheme Park"), src/game/park/TPHField.cpp:
  GenerateRideHeightMap 0x1005db4c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers in the objects the code builds) for the translation.
Every callee outside the unit is a fake on both sides that records its arguments: InstanceRideAnimSet (answers the prebuilt instance or 0),
InitRideAnimSet, MReleaseAllocF (zeroed blocks), GetAnimStats, SetRideAnimTime, ResetAnimTimeStats, GetAnimTimeStats, SetNewAnim (sets the
control's kind and place), GetAnimRemainingMilliseconds and RATick (answers from queues), SetMatrixIdentity (writes the identity),
CalculateMaxVertexHeights (logs its arguments, the matrix and the globals, and raises the cells to a pattern from the case),
QuickCopyUnmaskedMeshVertices, ResetRideAnimationSingleChannel, DestroyRideAnimSetInstance.

The world of a case: the original set (mesh, header box), the instance (mesh, header with box and nodes, animation list, groups with counts
and entries), a shape or none, the width, depth and flag; the four globals hold a sentinel. What is compared: the answer, the map block (words,
pointer distances, cells, heights and solidity), the four globals, the nodes' first words and the event log.

No fused operations; the fctiwz of the animation time and of the height flag runs under HardwareRounding. Not tested: a width or depth of 0
(a zero-size block), a width or depth past 20 with a shape (the shape holds 20 by 20), an animation kind past 11 (the original indexes past
the groups), NaN or values past 2^31 as the tick or box height (fctiwz saturation differs in the emulator).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_GenerateRideHeightMap"
MASK = 0xFFFFFFFF

FN = 0x1005DB4C
SIZES = {FN: 1892}
OWN = set(SIZES)

ADDR = dict(rows=0x102C2140, stride=0x102C2144, limz=0x102C2148, limx=0x102C214C)

CALLEES = dict(
    inst=0x10062E1C, init=0x100C7768, alloc=0x10064BD4, stats=0x101EF044, settime=0x100C3234, resetstats=0x100C5A90,
    timestats=0x100C5BCC, setnew=0x100C60FC, remaining=0x100C5DB8, tick=0x100C4E20, ident=0x100C0E28, cmh=0x1005D304,
    quick=0x100C4F18, reset=0x100C5054, destroy=0x10063338)

KEYS = ["heads", "groups", "grpsz", "grp_count", "grp_entries", "animcount", "anims", "entsz", "ent_anim", "mesh_header", "mesh_f08",
        "setsz", "meshsz", "nodesz", "hdr_nodecount", "hdr_nodes", "hdr_f78", "hdr_box", "hdrsz", "ctrlsz", "shapesz", "shape_cells",
        "cellsz", "map_cells", "map_f10", "map_min", "map_max", "node_flags"]
MAC = dict(heads=8, groups=0x18, grpsz=12, grp_count=4, grp_entries=8, animcount=0xA8, anims=0xAC, entsz=8, ent_anim=4, mesh_header=4,
           mesh_f08=8, setsz=0x100, meshsz=0x40, nodesz=0xA0, hdr_nodecount=0x44, hdr_nodes=0x70, hdr_f78=0x78, hdr_box=0x80, hdrsz=0x100,
           ctrlsz=0x38, shapesz=8 + 400 * 8, shape_cells=8, cellsz=8, map_cells=0xC, map_f10=0x10, map_min=0x18, map_max=0x24,
           node_flags=0)
SAME = ["nodesz", "hdr_nodecount", "hdr_nodes", "hdr_f78", "hdr_box", "ctrlsz", "shapesz", "shape_cells", "cellsz", "map_cells",
        "map_f10", "map_min", "map_max", "node_flags"]

CTX = {"known": [], "allocs": [], "ncall": 0}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


def idof(p):
    if p == 0:
        return 0
    K = CTX["known"]
    for i, k in enumerate(K):
        if k == p:
            return i + 1
    return -1


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def st_inst(e):
    L = e.cl
    L.rec("InstanceRideAnimSet", (idof(e.r(3)), e.r(4), e.r(5), e.r(7)))
    ok = L.ret("Inst") & MASK
    if ok:
        e.write(e.r(6), struct.pack(">I", CTX["known"][1]))
    e.set_r(3, ok)


def st_init(e):
    e.cl.rec("InitRideAnimSet", (idof(e.r(3)), e.r(4), e.r(5), e.r(6)))
    e.set_r(3, 0)


def st_alloc(e):
    size = e.r(3)
    e.cl.rec("MReleaseAllocF", (size, e.r(4)))
    p = e.alloc(size)
    e.write(p, bytes(size))
    CTX["allocs"].append(p)
    e.set_r(3, p)


def st_stats(e):
    L = e.cl
    L.rec("GetAnimStats", (idof(e.r(3)), e.r(4)))
    e.write(e.r(5), struct.pack(">I", L.ret("gas_kind") & MASK))
    e.write(e.r(6), struct.pack(">I", L.ret("gas_idx") & MASK))
    e.write(e.r(7), struct.pack(">I", L.ret("gas_x") & MASK))


def st_settime(e):
    e.cl.rec("SetRideAnimTime", (e.r(3),))


def st_resetstats(e):
    e.cl.rec("ResetAnimTimeStats", (idof(e.r(4)), e.r(5), fb(e.f(1))))


def st_timestats(e):
    e.cl.rec("GetAnimTimeStats", (e.u32(e.r(4)),))


def st_setnew(e):
    e.cl.rec("SetNewAnim", (idof(e.r(3)), e.r(4), e.r(5), e.r(7), fb(e.f(1))))
    e.write(e.r(6) + 4, struct.pack(">II", e.r(4), e.r(5)))


def st_remaining(e):
    e.set_f1(bitsf(e.cl.ret("rem") & MASK))


def st_tick(e):
    e.cl.rec("RATick", (idof(e.r(4)), idof(e.r(5))))
    e.write(e.r(3) + 0x20, struct.pack(">I", e.cl.ret("tickf") & MASK))
    e.set_r(3, 0)


def st_ident(e):
    e.cl.rec("SetMatrixIdentity", ())
    words = [0x3F800000 if i % 5 == 0 else 0 for i in range(16)]
    e.write(e.r(3), struct.pack(">16I", *words))


def st_cmh(e):
    L = e.cl
    sp = e.r(1)
    node, header = e.r(3), e.r(4)
    words = [e.r(5 + i) for i in range(6)] + [e.u32(sp + 0x38 + 4 * i) for i in range(10)]
    mp = e.u32(sp + 0x60)
    bad = e.u32(sp + 0x64)
    anim = e.u32(sp + 0x68)
    unused = e.u32(sp + 0x6C)
    flag = e.u32(e.u32(sp + 0x70))
    L.rec("CMH", (idof(node), idof(header), int(mp == CTX["allocs"][0]), idof(anim), unused, flag, int(bad != 0)))
    L.rec("CMHM", tuple(words[:8]))
    L.rec("CMHM", tuple(words[8:]))
    mm = [e.u32(mp + 0x18 + 4 * i) for i in range(6)]
    L.rec("CMHG", tuple([e.u32(ADDR["limx"]), e.u32(ADDR["limz"]), e.u32(ADDR["stride"]), e.u32(ADDR["rows"])] + mm))
    seed = L.ret("cmh") & MASK
    CTX["ncall"] += 1
    wh = e.u32(mp + 8)
    n = (wh >> 16) * (wh & 0xFFFF) * 25
    cells = CTX["allocs"][1]
    old = bytearray(e.read(cells, n))
    for i in range(n):
        b = (i * seed + CTX["ncall"] * 31 + (i >> 2)) & 0xFF
        if b > old[i]:
            old[i] = b
    e.write(cells, bytes(old))
    e.set_r(3, 1)


def st_quick(e):
    e.cl.rec("QuickCopyUnmaskedMeshVertices", (idof(e.r(3)), idof(e.r(4)), e.r(5)))


def st_reset(e):
    e.cl.rec("ResetRideAnimationSingleChannel", (idof(e.r(3)), idof(e.r(4)), idof(e.r(5))))


def st_destroy(e):
    e.cl.rec("DestroyRideAnimSetInstance", (idof(e.r(3)), e.r(4), e.r(5)))


for _name, _fn in dict(inst=st_inst, init=st_init, alloc=st_alloc, stats=st_stats, settime=st_settime, resetstats=st_resetstats,
                       timestats=st_timestats, setnew=st_setnew, remaining=st_remaining, tick=st_tick, ident=st_ident, cmh=st_cmh,
                       quick=st_quick, reset=st_reset, destroy=st_destroy).items():
    STUBS.internal(CALLEES[_name], _fn)


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

    def h16(self, a, vs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", v & 0xFFFF) for v in vs))

    def rd_h16(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    fmt = ">I"
    ps = 4
    lay = MAC

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        CTX["known"], CTX["allocs"], CTX["ncall"] = [], [], 0
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

    def wptr(self, a, p):
        self.write(a, struct.pack(">I", p))

    def p32(self, p):
        return p

    def setknown(self, lst):
        CTX["known"] = list(lst)

    def allocs(self):
        return list(CTX["allocs"])

    def log(self):
        return self.L.log

    def call(self, oset, w, h, shape, flag):
        with ROUNDING(self.e):
            r3, _, _ = T.call_ppc(self.e, FN, [oset, w, h, shape, flag & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    ps = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lay = lib.lay
        lib.tpw_gh_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gh_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_gh_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def p32(self, p):
        return self.lib.tpw_gh_off(p) if p else 0

    def setknown(self, lst):
        for i, p in enumerate(lst):
            self.lib.tpw_gh_known(i, p)

    def allocs(self):
        return [self.lib.tpw_gh_alloc_get(i) for i in range(self.lib.tpw_gh_alloc_count())]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, oset, w, h, shape, flag):
        return self.lib.tpw_gh_gen(oset, w, h, shape, s32(flag)) & 0xFFFFFFFFFFFFFFFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_gh_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gh_off.restype, lib.tpw_gh_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_gh_known.restype, lib.tpw_gh_known.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_gh_reset.restype, lib.tpw_gh_reset.argtypes = None, []
    lib.tpw_gh_alloc_get.restype, lib.tpw_gh_alloc_get.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_gh_alloc_count.restype, lib.tpw_gh_alloc_count.argtypes = C.c_int32, []
    lib.tpw_gh_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gh_gen.restype, lib.tpw_gh_gen.argtypes = C.c_uint64, [C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64, C.c_int32]
    out = (C.c_int64 * 64)()
    n = lib.tpw_gh_layout(out)
    assert n == len(KEYS), n
    lay = dict(zip(KEYS, list(out)[:n]))
    for k in SAME:
        assert lay[k] == MAC[k], (k, lay[k], MAC[k])
    lib.lay = lay
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    L = s.lay
    s.words(s.g("stride"), [0xDEADBEE1])
    s.words(s.g("limz"), [0xDEADBEE2])
    s.words(s.g("limx"), [0xDEADBEE3])
    s.words(s.g("rows"), [0xDEADBEE4])
    # the original set: only its mesh's header box is read
    ohdr = s.alloc(L["hdrsz"])
    s.words(ohdr + L["hdr_box"], case["obox"])
    omesh = s.alloc(L["meshsz"])
    s.wptr(omesh + L["mesh_header"], ohdr)
    oset = s.alloc(L["setsz"])
    s.wptr(oset + L["heads"], omesh)
    # the instance
    node0 = s.alloc(0x100)
    nn = len(case["nodes"])
    nodes = s.alloc(max(1, nn) * L["nodesz"])
    for i, flags in enumerate(case["nodes"]):
        s.words(nodes + i * L["nodesz"] + L["node_flags"], [flags])
    ihdr = s.alloc(L["hdrsz"])
    s.h16(ihdr + L["hdr_nodecount"], [nn])
    s.words(ihdr + L["hdr_nodes"], [s.p32(nodes)])
    s.words(ihdr + L["hdr_f78"], [s.p32(node0)])
    s.words(ihdr + L["hdr_box"], case["ibox"])
    imesh8 = s.alloc(L["meshsz"])
    imesh = s.alloc(L["meshsz"])
    s.wptr(imesh + L["mesh_header"], ihdr)
    s.wptr(imesh + L["mesh_f08"], imesh8)
    anims = [s.alloc(0x40) if present else 0 for present in case["present"]]
    na = len(anims)
    arr = s.alloc(max(1, na) * s.ps)
    for i, a in enumerate(anims):
        s.wptr(arr + i * s.ps, a)
    inst = s.alloc(L["setsz"])
    s.wptr(inst + L["heads"], imesh)
    s.words(inst + L["animcount"], [na])
    s.wptr(inst + L["anims"], arr)
    for k in range(12):
        g = inst + L["groups"] + k * L["grpsz"]
        s.words(g + L["grp_count"], [case["counts"][k]])
        ent = case["ent"][k]
        if ent is not None:
            blk = s.alloc(3 * L["entsz"])
            for j in range(3):
                s.words(blk + j * L["entsz"], [j])
                ai = ent[j]
                s.wptr(blk + j * L["entsz"] + L["ent_anim"], anims[ai] if 0 <= ai < na else 0)
            s.wptr(g + L["grp_entries"], blk)
    shape = 0
    if case["shape"] is not None:
        shape = s.alloc(L["shapesz"])
        for y, row in enumerate(case["shape"]):
            for x, v in enumerate(row):
                s.words(shape + L["shape_cells"] + y * 20 * L["cellsz"] + x * L["cellsz"], [v])
    known = [oset, inst, imesh, imesh8, ihdr, node0] + (anims + [0, 0, 0, 0])[:4]
    s.setknown(known)
    return {"oset": oset, "shape": shape, "nodes": nodes, "nn": nn}


def execute(s, case, st):
    return s.call(st["oset"], case["w"], case["h"], st["shape"], case["flag"])


def observe(s, case, st, answer):
    L = s.lay
    out = {"null": answer == 0, "log": s.log()}
    for name in ("rows", "stride", "limz", "limx"):
        out[name] = s.rd_word(s.g(name))
    out["nodes"] = [s.rd_word(st["nodes"] + i * L["nodesz"]) for i in range(st["nn"])]
    allocs = s.allocs()
    out["allocs"] = len(allocs)
    if answer != 0:
        w, h = case["w"], case["h"]
        out["map_is_first"] = answer == allocs[0]
        m = s.read(answer, 0x30)
        mw = [struct.unpack(s.fmt, m[i:i + 4])[0] for i in range(0, 0x30, 4)]
        out["magic"] = mw[0:2]
        out["halves"] = s.rd_h16(answer + 8, 2)
        cells = mw[3]
        out["cells_is_second"] = cells == s.p32(allocs[1])
        out["d10"] = (mw[4] - cells) & MASK
        out["d14"] = (mw[5] - mw[4]) & MASK
        out["minmax"] = mw[6:12]
        out["block"] = s.read(allocs[1], w * h * 25 + w * h * 2)
    return out


def run(s, case):
    try:
        st = build(s, case)
        return observe(s, case, st, execute(s, case, st))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]
TICKS = [0.0, -0.0, 0.5, -0.5, 0.99, -0.99, 1.0, 29.0, 30.0, 1234.567, -1234.567, 2147483520.0, -2147483648.0, 100000.5]
REMS = [0.0, -0.0, 1.0, 33.5, 1000.0, 0.001]


def rf(r):
    k = r.random()
    if k < 0.15:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-300, 300))


def make(r, **kw):
    """A random world; kw overrides any field."""
    c = {}
    c["w"] = r.choice([1, 2, 3, 4, 5, r.randrange(1, 9)])
    c["h"] = r.choice([1, 2, 3, 4, 5, r.randrange(1, 9)])
    c["flag"] = r.choice([0, 0, 1, -1, r.randrange(-5, 6)])
    c["shape"] = None if r.random() < 0.3 else [[r.choice([0, 1, -1, 7, 0x100]) for _ in range(c["w"])] for _ in range(c["h"])]
    c["inst"] = 1 if r.random() < 0.9 else 0
    animated = r.random() < 0.55
    n = r.randrange(1, 5) if animated else r.randrange(0, 4)
    c["present"] = [r.random() < 0.8 for _ in range(n)]
    c["counts"] = [r.choice([0, 0, 1, 3]) for _ in range(12)]
    if animated:
        c["counts"][r.randrange(2, 12)] = r.choice([1, 5])
    elif r.random() < 0.7:
        c["counts"][2:] = [0] * 10
    c["ent"] = []
    for k in range(12):
        if k < 2 and r.random() < 0.35:
            c["ent"].append(None)
        else:
            c["ent"].append([r.randrange(-1, n) if n else -1 for _ in range(3)])
    c["nodes"] = [r.getrandbits(32) for _ in range(r.randrange(0, 4))]
    c["obox"] = [rf(r) for _ in range(6)]
    c["ibox"] = [rf(r) for _ in range(6)]
    c["ibox"][4] = fb(r.choice([0.0, -0.0, 0.39, 1.0, 100.0, r.uniform(-600, 600), r.uniform(-600, 600), -3.7]))
    c["gas_kind"] = [r.randrange(2, 12) for _ in range(3)]
    c["gas_idx"] = [r.randrange(0, 3) for _ in range(3)]
    c["gas_x"] = [r.getrandbits(32) for _ in range(3)]
    rem = [fb(r.choice(REMS)) if r.random() < 0.7 else fb(-1.0) for _ in range(r.randrange(0, 8))]
    if r.random() < 0.1:
        rem.insert(r.randrange(0, len(rem) + 1), 0x7FC00000)
    rem.append(fb(-1.0))
    c["rem"] = rem
    c["tick"] = [fb(r.choice(TICKS)) if r.random() < 0.5 else fb(r.uniform(-5000, 5000)) for _ in range(4)]
    c["cmh"] = [r.getrandbits(32) for _ in range(4)]
    c.update(kw)
    return finish(c)


def finish(c):
    c["queues"] = {"Inst": [c["inst"]], "gas_kind": c["gas_kind"], "gas_idx": c["gas_idx"], "gas_x": c["gas_x"], "rem": c["rem"],
                   "tickf": c["tick"], "cmh": c["cmh"]}
    return c


def edges():
    r = random.Random(675)
    yield make(r, inst=0)
    z = [0] * 12
    for flag in (0, 1, -1):
        yield make(r, present=[], counts=z, flag=flag)
        yield make(r, present=[True], counts=z, flag=flag, shape=None)
    yield make(r, w=1, h=1, present=[], counts=z, shape=[[0]])
    yield make(r, w=20, h=2, present=[], counts=z, shape=[[r.choice([0, 3]) for _ in range(20)] for _ in range(2)])
    yield make(r, w=2, h=20, present=[], counts=z, shape=[[r.choice([0, 3]) for _ in range(2)] for _ in range(20)])
    # kinds 0 and 1 only: not animated; a count without animations: not animated
    c01 = [4, 4] + [0] * 10
    yield make(r, present=[True, True], counts=c01)
    yield make(r, present=[], counts=[0, 0, 5] + [0] * 9)
    for k in (2, 5, 11):
        cnt = [0] * 12
        cnt[k] = 1
        yield make(r, present=[True], counts=cnt, rem=[fb(0.0), fb(-1.0)], flag=0)
        yield make(r, present=[True, False, True], counts=cnt, rem=[fb(-1.0)], flag=1, nodes=[0x00800000, 0xFFFFFFFF, 0, 0x7F7FFFFF])
    cnt = [0, 0, 1] + [0] * 9
    base_ent = [[0, 1, 2]] * 12
    # animations skipped by the first two groups' first entry, and by a null group entry
    yield make(r, present=[True, True, True], counts=cnt, ent=[[0, 1, 1], [1, 0, 0]] + base_ent[2:], rem=[fb(0.0), fb(0.0), fb(-1.0)])
    yield make(r, present=[True, True, True], counts=cnt, ent=[None, [2, 0, 0]] + base_ent[2:], rem=[fb(1.0), fb(-1.0), fb(5.0), fb(-1.0)])
    yield make(r, present=[True, True], counts=cnt, ent=[[0, 1, 1], None] + base_ent[2:])
    for rem in ([fb(-1.0)], [fb(-0.0), fb(-1.0)], [0x7FC00000], [fb(0.0)] * 4 + [fb(-1.0)]):
        yield make(r, present=[True], counts=cnt, rem=rem)
    for t in TICKS:
        yield make(r, present=[True], counts=cnt, rem=[fb(0.0), fb(-1.0)], tick=[fb(t)])
    yield make(r, present=[True], counts=cnt, rem=[fb(0.0), fb(-1.0)], flag=1, w=3, h=3, ibox=[0, 0, 0, 0, fb(2.5), 0])
    yield make(r, present=[True], counts=cnt, rem=[fb(0.0), fb(-1.0)], flag=1, w=3, h=3, ibox=[0, 0, 0, 0, fb(-2.5), 0])


def rnd(r):
    return make(r)


TESTS = [
    FnTest("GenerateRideHeightMap", ppc, host, edge=edges, rnd=rnd),
]
