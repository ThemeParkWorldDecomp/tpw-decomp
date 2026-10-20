"""Unit v5893, the pylon edit helpers, against libtpw_unit_DeletePylon.dylib (tests/unit_DeletePylon.cpp), main app
("SimTheme Park"), src/DisableCurrentPylonEditState_SetCoasterEditState_DeletePylon.cpp:
  RemoveTrackBraceFromPylon 0x10054640, DeletePylon 0x10054814, MoveBrace 0x10054978

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers) for the translation; the same code reads both back. The
callees outside the unit (Destroy3DObjectFromAIWorld, MReleaseFreeF, FlagGenerateTrack, Assign3DObjectToAIWorld, UpdatePylonStackStructures,
UpdatePylonStackMeshes, GetTempFixedRideParams, MoveRide, CTRACE, DetermineLinkLevel, InsertBraceIntoPylon, NewPylon,
FlagCollisionUpdateTrackAroundBrace, DoMeshSkirtDeform, SquishPylonBracesIfRequired, CalculateBraceHeight, GetIntervalForHeight,
SetupBraceAnimFrameData) are fakes on both sides that record their arguments and answer from the case's queues. RemoveTrackBraceFromPylon and
DeletePylon run as the original inside MoveBrace.

The world of a case: an instance, up to five pylon records (four plus NewPylon's answer), six braces, four pylon controls, three ride sets
(set, mesh, header, node), the height field's header, and the globals (gRideAnimSets, gHeightfieldHeader/Width/Height, gPylonMoveOrigData,
gPylonMoveHeight). What is compared: every field of every object the functions may touch, the out floats, the globals and the event log.

Fused operations: MoveBrace's two fmadds (HardwareRounding over the three functions). Not tested: a NULL control in the different-cell path
and a NULL instance outside the same-cell path (the original dereferences them), a brace index past its pylon's count.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_DeletePylon"
MASK = 0xFFFFFFFF

FN_REM, FN_DEL, FN_MOVE = 0x10054640, 0x10054814, 0x10054978
SIZES = {FN_REM: 380, FN_DEL: 284, FN_MOVE: 1232}
OWN = set(SIZES)

CALLEES = dict(
    d3d=0x1005f3fc, release=0x10064c54, flaggen=0x10040f08, assign=0x1005f53c, updstruct=0x1003d844, updmeshes=0x1003d958,
    gettemp=0x1003d5b0, moveride=0x101ed9a0, ctrace=0x1006fcbc, dll=0x1003e7dc, insert=0x1003ef78, newpylon=0x1003d610,
    flagcoll=0x10040fa4, skirt=0x1006e930, squish=0x1003ee10, cbh=0x1003e6e0, gih=0x10040e10, setup=0x1003bb44)

ADDR = dict(sets=0x102c79a4, hfhdr=0x102c6c7c, width=0x102c6c68, height=0x102c6c64, orig=0x102c1774, moveheight=0x102c1778)

# (kind, field, type, Mac offset) in the order tpw_pyl_layout lists the host offsets
ORDER = [
    ("inst", "mType", "p", 0x04), ("inst", "mF40", "w", 0x40), ("inst", "mPylons", "p", 0x4c), ("inst", "mPylonTail", "p", 0x50),
    ("pyl", "mNumber", "w", 0), ("pyl", "mFlags", "w", 4), ("pyl", "mIndex", "w", 8), ("pyl", "mCellX", "w", 0xc),
    ("pyl", "mCellZ", "w", 0x10), ("pyl", "mControl", "p", 0x18), ("pyl", "mX", "f", 0x1c), ("pyl", "mY", "f", 0x20),
    ("pyl", "mZ", "f", 0x24), ("pyl", "mNext", "p", 0x28), ("pyl", "mPrev", "p", 0x2c), ("pyl", "mCount", "w", 0x30),
    ("pyl", "mBraces", "p", 0x34),
    ("brace", "mNumber", "w", 0), ("brace", "place", "w", 4), ("brace", "mFlags", "w", 8), ("brace", "mWord70", "w", 0x70),
    ("brace", "mFE8w", "f", 0xe8), ("brace", "mData", "p", 0x100),
    ("ctl", "mFB4", "w", 0xb4), ("ctl", "mFB8", "w", 0xb8), ("ctl", "mFC4", "f", 0xc4),
    ("set", "mHeads", "p", 8), ("mesh", "mHeader", "p", 4),
    ("hdr", "mFlags", "w", 0x30), ("hdr", "mHField", "p32", 0x6c), ("hdr", "mF78", "p32", 0x78),
    ("hfld", "mF18", "w", 0x18), ("node", "m31", "f", 0x44),
]
MACOFF = {(k, f): o for k, f, t, o in ORDER}
FTYPE = {(k, f): t for k, f, t, o in ORDER}
COUNTS = {("pyl", "mBraces"): 5}
KINDFIELDS = {}
for _k, _f, _t, _o in ORDER:
    KINDFIELDS.setdefault(_k, []).append(_f)
HOSTOFF = {}
HOSTSIZES = []
HOST = {"base": 0}

PN = ["pyl0", "pyl1", "pyl2", "pyl3", "pn"]
BN = ["br%d" % i for i in range(6)]
CN = ["ctl%d" % i for i in range(4)]
KIND = {"inst": "inst", "type": "raw", "outs": "raw"}
for _n in PN:
    KIND[_n] = "pyl"
for _n in BN:
    KIND[_n] = "brace"
for _n in CN:
    KIND[_n] = "ctl"
for _i in (1, 2, 3):
    KIND["set%d" % _i], KIND["mesh%d" % _i], KIND["hdr%d" % _i], KIND["node%d" % _i] = "set", "mesh", "hdr", "node"
KIND["hdrH"], KIND["hfld"] = "hdr", "hfld"
NAMES = list(KIND)
CUR = {}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ff(e):
    return canon_f(fb(e.f(1)))


def u(e, i):
    return e.r(i) & MASK


def stub(name):
    def deco(fn):
        STUBS.internal(CALLEES[name], fn)
        return fn
    return deco


@stub("d3d")
def _d3d(e):
    e.cl.rec("D3D", (u(e, 3), u(e, 4)))


@stub("release")
def _release(e):
    e.cl.rec("MReleaseFreeF", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


@stub("flaggen")
def _flaggen(e):
    e.cl.rec("FlagGenerateTrack", (e.cl.lab(e.r(3)), s32(e.r(4))))


@stub("assign")
def _assign(e):
    e.cl.rec("Assign3D", (e.cl.lab(e.r(3)), u(e, 4), u(e, 5), u(e, 6), s32(e.r(7))))


@stub("updstruct")
def _updstruct(e):
    e.cl.rec("UpdStruct", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


@stub("updmeshes")
def _updmeshes(e):
    e.cl.rec("UpdMeshes", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), s32(e.r(5)), s32(e.r(6))))


@stub("gettemp")
def _gettemp(e):
    e.cl.rec("GetTempParams", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, CUR["params"])


@stub("moveride")
def _moveride(e):
    e.cl.rec("MoveRide", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), u(e, 6), s32(e.r(7)), s32(e.r(8)), u(e, 9)))


@stub("ctrace")
def _ctrace(e):
    text = e.cstr(e.r(4))
    data = text.encode("latin1") if isinstance(text, str) else bytes(text)
    total = sum((i + 1) * b for i, b in enumerate(data))
    e.cl.rec("CTRACE", (u(e, 3), s32(e.r(5)), total))


@stub("dll")
def _dll(e):
    L = e.cl
    L.rec("DetermineLinkLevel", (ff(e), L.lab(e.r(4)), s32(e.r(7))))
    level = L.ret("DLLlevel")
    out = L.ret("DLLout")
    e.write(e.r(5), struct.pack(">I", level & MASK))
    e.write(e.r(6), struct.pack(">I", out & MASK))
    e.set_r(3, L.ret("DLL") & MASK)


@stub("insert")
def _insert(e):
    L = e.cl
    sp = e.r(1)
    L.rec("InsertBrace", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5)), u(e, 6), ff(e), u(e, 8), s32(e.r(9)), s32(e.r(10)),
                          s32(e.u32(sp + 0x38)), s32(e.u32(sp + 0x3c))))


@stub("newpylon")
def _newpylon(e):
    L = e.cl
    L.rec("NewPylon", (u(e, 3), u(e, 4), u(e, 5), s32(e.r(6)), L.lab(e.r(7)), L.lab(e.r(8))))
    e.set_r(3, CUR["pn"])


@stub("flagcoll")
def _flagcoll(e):
    e.cl.rec("FlagCollision", (e.cl.lab(e.r(3)),))


@stub("skirt")
def _skirt(e):
    e.cl.rec("SkirtDeform", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), ff(e), u(e, 6)))      # the float takes r5's slot


@stub("squish")
def _squish(e):
    L = e.cl
    L.rec("Squish", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))
    e.set_r(3, L.ret("SQ") & MASK)


@stub("cbh")
def _cbh(e):
    e.cl.rec("CalcBraceHeight", (e.cl.lab(e.r(3)),))
    e.set_f1(bitsf(e.cl.ret("CBH")))


@stub("gih")
def _gih(e):
    e.cl.rec("GetInterval", (ff(e), e.cl.lab(e.r(4))))
    e.set_f1(bitsf(e.cl.ret("GIH")))


@stub("setup")
def _setup(e):
    e.cl.rec("SetupBrace", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def off(self, kind, field):
        return (MACOFF if self.mac else HOSTOFF)[(kind, field)]

    def w(self, a, v):
        self.write(a, struct.pack(self.fmt, v & MASK))

    def rw(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def addr_of(self, name):
        return 0 if name is None else self.objs[name]

    def name_of(self, addr):
        if addr == 0:
            return None
        return self.rev.get(addr, ("raw", addr))

    def wp(self, a, addr):
        self.write(a, struct.pack(self.pfmt, addr))

    def rp(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def set(self, name, field, val, idx=0):
        kind = KIND[name]
        t = FTYPE[(kind, field)]
        a = self.objs[name] + self.off(kind, field)
        if t in ("w", "f"):
            self.w(a + 4 * idx, val)
        elif t == "p":
            self.wp(a + self.psz * idx, self.addr_of(val))
        else:
            addr = self.addr_of(val)
            self.w(a, addr - HOST["base"] if (addr and not self.mac) else addr)

    def get(self, name, field, idx=0):
        kind = KIND[name]
        t = FTYPE[(kind, field)]
        a = self.objs[name] + self.off(kind, field)
        if t == "w":
            return self.rw(a + 4 * idx)
        if t == "f":
            return canon_f(self.rw(a + 4 * idx))
        if t == "p":
            return self.name_of(self.rp(a + self.psz * idx))
        v = self.rw(a)
        return self.name_of(v + HOST["base"] if (v and not self.mac) else v)

    def setg(self, gname, val, idx=0):
        if gname in ("width", "height", "moveheight"):
            self.w(self.g(gname), val)
        else:
            self.wp(self.g(gname) + self.psz * idx, self.addr_of(val))

    def rev_map(self):
        self.rev = {a: n for n, a in self.objs.items()}


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    mac = True

    def __init__(self, e, case):
        self.e = e
        self.objs = {}
        for n in NAMES:
            size = 0x600 if n == "inst" else 0x400
            self.objs[n] = e.alloc(size, bytes(size))
        params = e.alloc(0x10, bytes(0x10))
        e.write(params + 0xC, struct.pack(">I", 0xABC0))
        CUR.clear()
        CUR["params"] = params
        CUR["pn"] = self.objs["pn"]
        self.rev_map()
        self.L = e.cl = CL.PpcLog({self.objs[n]: i + 1 for i, n in enumerate(NAMES)}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, fn, args):
        entry = {"rem": FN_REM, "del": FN_DEL, "move": FN_MOVE}[fn]
        with ROUNDING(self.e):
            T.call_ppc(self.e, entry, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8
    mac = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.objs = {}
        for n in NAMES:
            size = 0x600 if n == "inst" else 0x400
            self.objs[n] = lib.tpw_pyl_alloc(size)
        self.rev_map()
        HOST["base"] = lib.tpw_pyl_base()      # known only once the arena has been used
        CL.host_begin(lib, {i + 1: self.objs[n] for i, n in enumerate(NAMES)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pyl_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        getattr(self.lib, "tpw_pyl_" + fn)(*[a & 0xFFFFFFFFFFFFFFFF for a in args])


def bind(lib):
    if getattr(lib, "bound", False):
        HOST["base"] = lib.tpw_pyl_base()
        return
    lib.tpw_pyl_addr.restype, lib.tpw_pyl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_pyl_alloc.restype, lib.tpw_pyl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_pyl_base.restype, lib.tpw_pyl_base.argtypes = C.c_uint64, []
    lib.tpw_pyl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pyl_rem.restype, lib.tpw_pyl_rem.argtypes = None, [C.c_uint64, C.c_uint64]
    lib.tpw_pyl_del.restype, lib.tpw_pyl_del.argtypes = None, [C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_pyl_move.restype = None
    lib.tpw_pyl_move.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint32, C.c_uint32, C.c_uint64, C.c_uint64, C.c_int32, C.c_int32]
    out = (C.c_int64 * 64)()
    n = lib.tpw_pyl_layout(out)
    assert n == len(ORDER) + 6, n
    for i, (k, f, t, o) in enumerate(ORDER):
        HOSTOFF[(k, f)] = out[i]
    HOST["base"] = lib.tpw_pyl_base()
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    for n in (1, 2, 3):
        s.set("node%d" % n, "m31", c["nodes"][n - 1])
        s.set("hdr%d" % n, "mFlags", c["hflags"][n - 1])
        s.set("hdr%d" % n, "mF78", "node%d" % n)
        s.set("mesh%d" % n, "mHeader", "hdr%d" % n)
        s.set("set%d" % n, "mHeads", "mesh%d" % n)
        s.setg("sets", "set%d" % n, n)
    s.set("hdrH", "mHField", "hfld")
    s.set("hfld", "mF18", c["stride"])
    s.setg("hfhdr", "hdrH")
    s.setg("width", c["width"])
    s.setg("height", c["height"])
    s.setg("orig", c["orig"])
    s.setg("moveheight", c["mheight"])
    if not s.mac:
        s.setg("newpylon", "pn")      # the PowerPC stub answers CUR["pn"]
    inst = c["inst"]
    s.set("inst", "mType", "type")
    s.set("inst", "mF40", inst["f40"])
    s.set("inst", "mPylons", inst["head"])
    s.set("inst", "mPylonTail", inst["tail"])
    for i, cd in enumerate(c["ctl"]):
        s.set(CN[i], "mFB4", cd["fb4"])
        s.set(CN[i], "mFB8", cd["fb8"])
        s.set(CN[i], "mFC4", cd["fc4"])
    for i, p in enumerate(c["pyl"]):
        if p is None:
            continue
        n = PN[i]
        for f, k in (("mNumber", "number"), ("mFlags", "flags"), ("mIndex", "index"), ("mCellX", "cx"), ("mCellZ", "cz"), ("mX", "x"),
                     ("mY", "y"), ("mZ", "z")):
            s.set(n, f, p[k])
        s.set(n, "mControl", p["ctl"])
        s.set(n, "mNext", p["next"])
        s.set(n, "mPrev", p["prev"])
        s.set(n, "mCount", len(p["braces"]))
        for j, b in enumerate(p["braces"]):
            s.set(n, "mBraces", b, j)
    for i, b in enumerate(c["br"]):
        if b is None:
            continue
        n = BN[i]
        s.set(n, "mNumber", b["number"])
        s.set(n, "place", b["place"])
        s.set(n, "mFlags", b["flags"])
        s.set(n, "mWord70", b["w70"])
        s.set(n, "mFE8w", b["loft"])
        s.set(n, "mData", b["data"])


def execute(s, c):
    a = c["args"]
    o = s.objs
    if c["op"] == "rem":
        s.call("rem", [o["inst"], o[BN[a["brace"]]]])
    elif c["op"] == "del":
        s.call("del", [o["inst"], s.addr_of(a["pylon"]), a["flag"]])
    else:
        outs = o["outs"]
        s.call("move", [s.addr_of(a["inst"]), o["br0"], s.addr_of(a["pylon"]), a["x"], a["z"], outs if a["a"] else 0, outs + 4 if a["b"] else 0,
                        a["c"], a["d"]])


def observe(s, c):
    d = {"log": s.log()}
    for n in NAMES:
        kind = KIND[n]
        if kind == "raw":
            continue
        for f in KINDFIELDS[kind]:
            for i in range(COUNTS.get((kind, f), 1)):
                d["%s.%s.%d" % (n, f, i)] = s.get(n, f, i)
    d["outs"] = [s.rw(s.objs["outs"]), s.rw(s.objs["outs"] + 4)]
    d["orig"] = s.name_of(s.rp(s.g("orig")))
    return d


def run(s, c):
    try:
        build(s, c)
        execute(s, c)
        return observe(s, c)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.06:
        return r.choice(SPECIAL)
    if k < 0.7:
        return fb(r.uniform(-8, 8))
    return fb(r.uniform(-300, 300))


def rgih(r):
    return r.choice([fb(-1.0), fb(0.0), fb(0.5), fb(1.0), fb(2.0), 0x7FC00000, fb(r.uniform(-1, 2)), fb(r.uniform(-1, 2))])


def base(r, op):
    c = {"op": op}
    c["queues"] = {
        "SQ": [r.choice([0, 1])], "DLL": [r.choice([0, 1, 1])], "DLLlevel": [r.randrange(0, 6)], "DLLout": [rf(r)],
        "CBH": [rf(r) for _ in range(6)], "GIH": [rgih(r) for _ in range(6)],
    }
    c["nodes"] = [rf(r) for _ in range(3)]
    c["hflags"] = [r.randrange(1 << 16) | r.choice([0, 0x4000]) for _ in range(3)]
    c["stride"] = r.randrange(1, 120)
    c["width"] = r.randrange(1, 40)
    c["height"] = r.randrange(1, 40)
    c["orig"] = None
    c["mheight"] = rf(r)
    c["inst"] = {"f40": r.randrange(0, 12), "head": None, "tail": None}
    c["ctl"] = [{"fb4": r.randrange(0, 30), "fb8": r.randrange(0, 30), "fc4": rf(r)} for _ in range(4)]
    c["pyl"] = [None] * 5
    c["br"] = [None] * 6
    return c


def pyl(r, **kw):
    d = dict(number=r.randrange(0, 50), flags=r.randrange(0, 8), index=r.randrange(0, 5000), cx=r.randrange(0, 30), cz=r.randrange(0, 30),
             x=rf(r), y=rf(r), z=rf(r), ctl=None, next=None, prev=None, braces=[])
    d.update(kw)
    return d


def brace(r):
    return dict(number=r.randrange(0, 100), place=0, flags=r.randrange(1 << 16), w70=r.randrange(1, 4), loft=rf(r), data=None)


def attach(c, r, pi, bis):
    c["pyl"][pi]["braces"] = [BN[b] for b in bis]
    for pos, b in enumerate(bis):
        if c["br"][b] is None:
            c["br"][b] = brace(r)
        c["br"][b]["place"] = pos
        c["br"][b]["data"] = PN[pi]


def fill_braces(c, r):
    for i in range(6):
        if c["br"][i] is None:
            c["br"][i] = brace(r)


# ---- RemoveTrackBraceFromPylon
def rem_case(r, n=None, idx=None):
    c = base(r, "rem")
    n = r.randrange(1, 6) if n is None else n
    idx = r.randrange(n) if idx is None else idx
    c["pyl"][0] = pyl(r, ctl="ctl0")
    attach(c, r, 0, list(range(n)))
    c["br"].append(None) if False else None
    fill_braces(c, r)
    if n == 5:
        c["br"][5] = None
    c["inst"]["head"], c["inst"]["tail"] = "pyl0", "pyl0"
    c["args"] = {"brace": idx}
    return c


def rem_edges():
    r = random.Random(11)
    for n in range(1, 6):
        for idx in range(n):
            yield rem_case(r, n, idx)


def rem_random(r):
    return rem_case(r)


# ---- DeletePylon
def del_case(r, m=None, k=None, flag=None):
    c = base(r, "del")
    m = r.randrange(1, 5) if m is None else m
    k = r.randrange(m) if k is None else k
    for i in range(m):
        c["pyl"][i] = pyl(r, number=r.randrange(0, 40), prev=PN[i - 1] if i else None, next=PN[i + 1] if i + 1 < m else None)
    c["pyl"][4] = pyl(r)
    fill_braces(c, r)
    head, tail = PN[0], PN[m - 1]
    if r.random() < 0.2:
        head = r.choice(PN[:m] + [None])
    if r.random() < 0.2:
        tail = r.choice(PN[:m] + [None])
    c["inst"]["head"], c["inst"]["tail"] = head, tail
    c["args"] = {"pylon": PN[k], "flag": r.choice([0, 1, 2]) if flag is None else flag}
    return c


def del_edges():
    r = random.Random(12)
    for m in range(1, 5):
        for k in range(m):
            for flag in (0, 1):
                c = del_case(r, m, k, flag)
                c["pyl"][k]["flags"] &= ~1
                yield c
                c = del_case(r, m, k, flag)
                c["pyl"][k]["flags"] |= 1
                yield c


def del_random(r):
    return del_case(r)


# ---- MoveBrace
def move_case(r, same=None, multi=None, target="any", d=None, xz=None):
    c = base(r, "move")
    same = r.random() < 0.25 if same is None else same
    multi = r.random() < 0.5 if multi is None else multi
    n0 = r.randrange(2, 5) if multi else 1
    c["pyl"][0] = pyl(r, ctl="ctl0")
    # the brace under test is br0, at a random place of the pylon
    order = list(range(n0))
    pos = r.randrange(n0)
    order.remove(0)
    order.insert(pos, 0)
    attach(c, r, 0, order)
    c["pyl"][1] = pyl(r, ctl="ctl1")
    attach(c, r, 1, [4, 5][: r.randrange(1, 3)])
    c["pyl"][4] = pyl(r)
    fill_braces(c, r)
    if target == "any":
        target = r.choice([None, "pyl0", "pyl1", "pyl1"])
    cx, cz = c["pyl"][0]["cx"], c["pyl"][0]["cz"]
    if same:
        x, z = cx, cz
        if r.random() < 0.15:
            c["pyl"][0]["ctl"] = None
    else:
        pick = lambda cur: r.choice([r.randrange(0, 40), r.randrange(0, 40), 0, 0xFFFFFFFF, (-r.randrange(1, 9)) & MASK, c["width"], c["width"] - 1,
                                     0x80000000, 0x7FFFFFFF])
        x, z = pick(cx), pick(cz)
        if x == cx and z == cz:
            x += 1
    c["orig"] = r.choice([None, "pyl0", "pyl1"])
    inst_null = same and r.random() < 0.15
    c["args"] = {"inst": None if inst_null else "inst", "pylon": target, "x": x & MASK, "z": z & MASK, "a": r.random() < 0.7, "b": r.random() < 0.7,
                 "c": r.randrange(0, 3), "d": r.choice([0, 1]) if d is None else d}
    c["inst"]["head"], c["inst"]["tail"] = "pyl0", "pyl1"
    return c


def move_edges():
    r = random.Random(13)
    for multi in (False, True):
        for target in (None, "pyl0", "pyl1"):
            for same in (False, True):
                for d in (0, 1):
                    for _ in range(2):
                        yield move_case(r, same, multi, target, d)
    for x in (0, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 1, 5):
        for z in (0, 0xFFFFFFFF, 0x80000000, 7):
            for multi in (False, True):
                c = move_case(r, False, multi, r.choice([None, "pyl1"]), 0)
                c["args"]["x"], c["args"]["z"] = x, z
                yield c
    # the cell exactly at the clamp, and one cell off the pylon's
    for w in (1, 2, 9):
        c = move_case(r, False, False, None, 0)
        c["width"], c["height"] = w, w
        c["args"]["x"], c["args"]["z"] = w, w - 1
        yield c


def move_random(r):
    return move_case(r)


TESTS = [
    FnTest("RemoveTrackBraceFromPylon", ppc, host, edge=rem_edges, rnd=rem_random),
    FnTest("DeletePylon", ppc, host, edge=del_edges, rnd=del_random),
    FnTest("MoveBrace", ppc, host, edge=move_edges, rnd=move_random),
]
