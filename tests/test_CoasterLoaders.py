"""Unit w148: the coaster type's remaining loaders, against libtpw_unit_CoasterLoaders.dylib (tests/unit_CoasterLoaders.cpp), main app
("SimTheme Park"), src/game/rides/CoasterPylon.cpp, src/game/rides/CoasterType.cpp:
  CreateTrackSectionMesh 0x10036564, SetupTrainType 0x1003a150, SetupCarType 0x10039bc0, LoadCoasterPylons 0x1003a9a8,
  SetupPylonData 0x1003a28c, CreateCrossSectionSelectsFromSamFile 0x1003851c, SetCrossSectionSelectDefaults 0x100369d8,
  OptimiseCrossSections 0x100388cc, RepackCrossSection 0x100387b4, GetTempFixedRideParams 0x1003d5b0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian,
4-byte pointers) for the original, and in the host library's memory (little-endian, 8-byte native pointers, the host offsets exported by
tpw_cld_layout) for the translation; the same code reads both back. The Ptr32 fields of the mesh header and its nodes hold a Mac address
or an arena offset. The callees outside the unit are fakes on both sides that record their arguments (pointers as labels, floats as bits)
and answer from the case's queues: MReleaseAllocF (a zeroed block; the host select records are bigger, so the logged size is mapped
back to the Mac's), LoadRideType, CTRACE, CountDataNodesByType, FindNthDataNodeIndex, IsValidAnimation, SetNewAnim, InstanceRide,
AnimateRideSet (it writes the next queued float into each embedded record's block, so the second animation read differs from the first),
DestroyRideInstance, DMCreateMesh, DMFreeTextures and SetMatrixIdentity. The unit's own functions call each other for real.

What is compared: every word a function writes (car, pylon control, type, select block, mesh entry, boxes and flags of the header and
the nodes, the heads block, used[], points, edges, the f98 block) read back by the same code, pointers as names (a block of the case, an
index in it, or the label of a block a fake allocated), and the event log.

No fused operations and no lfd occur in the ten functions, so no rounding hook is needed. Not tested: OptimiseCrossSections with more than
21 points or an edge index at or above the point count (the Mac's used[] is a 21-word stack local; the host keeps a 64 K-word buffer);
a section number of the .sam above count + 3 (a pointer far outside the table: the Mac wraps at 32 bits, the host does not); a NULL root
node or a NULL instance set (the original reads them unconditionally); CreateTrackSectionMesh's answer (the original returns the type's
mesh set in r3, the host is void); InstanceRide answering 0 with no set in the slot.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CoasterLoaders"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); they run as the original even where other modules left a stub
FN = dict(mesh=0x10036564, train=0x1003A150, car=0x10039BC0, pylons=0x1003A9A8, pylon=0x1003A28C, selects=0x1003851C,
          defaults=0x100369D8, optimise=0x100388CC, repack=0x100387B4, params=0x1003D5B0)
SIZES = {0x10036564: 196, 0x1003A150: 260, 0x10039BC0: 1376, 0x1003A9A8: 380, 0x1003A28C: 1748, 0x1003851C: 552, 0x100369D8: 200,
         0x100388CC: 460, 0x100387B4: 208, 0x1003D5B0: 44}
OWN = set(SIZES)

# ---- the callees faked on both sides
MRELEASEALLOCF, LOADRIDETYPE, CTRACE = 0x10064BD4, 0x10068FD0, 0x1006FCBC
COUNTNODES, FINDNTH, ISVALIDANIM = 0x101FFF0C, 0x101FFE74, 0x100C5D3C
SETNEWANIM, INSTANCERIDE, ANIMATERIDESET = 0x100C60FC, 0x10069B24, 0x100C74D4
DESTROYINSTANCE, DMCREATEMESH, DMFREETEXTURES, SETMATRIXIDENTITY = 0x1006A3EC, 0x100301C0, 0x10030178, 0x100C0E28

# ---- the Mac's addresses of the globals (the asm's TOC comments): gRideTypes, gRideAnimSets, GetTempFixedRideParams' two buffers
ADDR_TYPES, ADDR_SETS, ADDR_PARAMS, ADDR_PAIR = 0x102D19B0, 0x102C79A4, 0x102B5BF8, 0x102B5C08
NSLOTS = 16

# ---- the Mac's offsets and sizes (key, value) in the order tpw_cld_layout writes the host's; the host's come from the library
FIELDS = [
    ('car.size', 0x34),
    ('car.mFlags', 0x0),
    ('car.mValue', 0x4),
    ('car.mName', 0x8),
    ('car.mRideType', 0xc),
    ('car.mF10', 0x10),
    ('car.mF14', 0x14),
    ('car.mF18', 0x18),
    ('car.mF1C', 0x1c),
    ('car.mNodeIndex', 0x20),
    ('car.mF24', 0x24),
    ('car.mF28', 0x28),
    ('car.mBlock', 0x2c),
    ('car.mHeadCount', 0x30),
    ('pyl.size', 0x120),
    ('pyl.mFlags', 0x0),
    ('pyl.mF0C', 0xc),
    ('pyl.mName', 0x10),
    ('pyl.mRideType', 0x14),
    ('pyl.mF18', 0x18),
    ('pyl.mF1C', 0x1c),
    ('pyl.mF20', 0x20),
    ('pyl.mF24', 0x24),
    ('pyl.mF28', 0x28),
    ('pyl.mF2C', 0x2c),
    ('pyl.mF30', 0x30),
    ('pyl.mF34', 0x34),
    ('pyl.mF38', 0x38),
    ('pyl.mF3C', 0x3c),
    ('pyl.mF40', 0x40),
    ('pyl.mF44', 0x44),
    ('pyl.mF48', 0x48),
    ('pyl.mF4C', 0x4c),
    ('pyl.mF50', 0x50),
    ('pyl.mF54', 0x54),
    ('pyl.mF68', 0x68),
    ('pyl.mFB4', 0xb4),
    ('pyl.mFB8', 0xb8),
    ('pyl.mFBC', 0xbc),
    ('pyl.mFC0', 0xc0),
    ('pyl.mFC4', 0xc4),
    ('pyl.mFD0', 0xd0),
    ('typ.size', 0x35c),
    ('typ.mFlags', 0x0),
    ('typ.mF14', 0x14),
    ('typ.mMeshSet', 0x18),
    ('typ.mTextures', 0x1c),
    ('typ.mF30', 0x30),
    ('typ.mF34', 0x34),
    ('typ.mF38', 0x38),
    ('typ.mF3C', 0x3c),
    ('typ.mSelectFlags', 0x74),
    ('typ.mF78', 0x78),
    ('typ.mSelectCount', 0x7c),
    ('typ.mSelectA', 0x80),
    ('typ.mSelectB', 0x84),
    ('typ.mF98', 0x98),
    ('typ.mF2F4', 0x2f4),
    ('typ.mF2F8', 0x2f8),
    ('typ.mCarTypeA', 0x2fc),
    ('typ.mCarTypeB', 0x300),
    ('typ.mCarTypeC', 0x304),
    ('typ.mF30C', 0x30c),
    ('typ.mF310', 0x310),
    ('typ.mF314', 0x314),
    ('dme.size', 0x30),
    ('dme.mFlags', 0x0),
    ('dme.mFaceCount', 0x4),
    ('dme.mVertexCount', 0x6),
    ('dme.mUVCount', 0x8),
    ('dme.mFaceSetCount', 0xa),
    ('dme.mPointCount', 0xc),
    ('dme.mF0E', 0xe),
    ('dme.mBox', 0x10),
    ('dme.mMatrix', 0x28),
    ('tex.size', 0x20),
    ('tex.mCount', 0x4),
    ('ras.size', 0xe0),
    ('ras.mFlags', 0x4),
    ('ras.mHeads', 0x8),
    ('ras.mTimeControls', 0x10),
    ('ras.mGroups', 0x18),
    ('grp.size', 0xc),
    ('grp.mFlags', 0x0),
    ('msh.size', 0x40),
    ('msh.mHeader', 0x4),
    ('msh.mF28', 0x28),
    ('hdr.size', 0x100),
    ('hdr.mNodeCount', 0x44),
    ('hdr.mEmbeddedCount', 0x48),
    ('hdr.mNodes', 0x70),
    ('hdr.mF78', 0x78),
    ('hdr.mEmbedded', 0x7c),
    ('hdr.mBox', 0x80),
    ('nod.size', 0xa0),
    ('nod.mFlags', 0x0),
    ('nod.mVertexCount', 0x58),
    ('nod.mBox', 0x78),
    ('emb.size', 0x14),
    ('emb.mF04', 0x4),
    ('ems.size', 0x14),
    ('ems.mRecords', 0x4),
    ('emr.size', 0x14),
    ('emr.mBlock', 0x4),
    ('tco.size', 0x38),
    ('tco.mF1C', 0x1c),
    ('tco.mF20', 0x20),
    ('sel.size', 0x2c),
    ('sel.mF00', 0x0),
    ('sel.mF20', 0x20),
    ('sel.mF24', 0x24),
    ('sel.mF28', 0x28),
    ('rec.size', 0x18),
    ('rec.mF00', 0x0),
    ('rec.mPointCount', 0x4),
    ('rec.mPoints', 0x8),
    ('rec.mBlock', 0xc),
    ('rec.mPointsB', 0x10),
    ('prm.size', 0x10),
    ('prm.mX', 0x0),
    ('prm.mZ', 0x4),
    ('prm.mRotation', 0x8),
    ('prm.mF0C', 0xc),
    ('pnt.size', 0x14),
    ('edg.size', 0xc),
    ('edg.mF00', 0x0),
    ('edg.mF04', 0x4),
    ('edg.mF06', 0x6),
    ('edg.mF08', 0x8),
    ('bal.size', 0x3b80),
    ('bal.mValues', 0x8),
    ('bal.entry', 0x4),
]
MAC = dict(FIELDS)
HOST = None
SAME = ("hdr.", "nod.", "emb.", "tco.", "pnt.", "edg.")      # same layout on both sides (Ptr32 fields)


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    bits &= MASK
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def tx(e, p):
    return e.cstr(p)[:159] if p else "(null)"


def st_alloc(e):
    size = e.r(3)
    e.cl.rec("MReleaseAllocF", (size, e.r(4)))
    a = e.alloc(size if size else 1)
    e.cl.newlab(a)
    e.set_r(3, a)


def st_load(e):
    L, sp, a = e.cl, e.r(1), e.r(5)
    pair = struct.unpack(">ii", e.read(a, 8)) if a else (0, 0)
    L.rec("LoadRideType", (int(a != 0), pair[0], pair[1], L.lab(e.r(6)), e.r(7), e.r(8), e.r(9), int(e.r(10) != 0),
                           int(e.u32(sp + 0x38) != 0), s32(e.u32(sp + 0x3C)), s32(e.u32(sp + 0x40))), (tx(e, e.r(3)), tx(e, e.r(4))))
    e.set_r(3, L.ret("LoadRideType") & MASK)


def st_ctrace(e):
    e.cl.rec("CTRACE", (e.r(3), s32(e.r(6))), (tx(e, e.r(4)), tx(e, e.r(5))))


def st_count(e):
    L = e.cl
    L.rec("CountDataNodesByType", (L.lab(e.r(3)), e.r(4)))
    e.set_r(3, L.ret("Count:%d" % e.r(4)) & MASK)


def st_find(e):
    L = e.cl
    L.rec("FindNthDataNodeIndex", (L.lab(e.r(3)), e.r(4), e.r(5)))
    e.set_r(3, L.ret("Find:%d:%d" % (e.r(4), e.r(5)), -1) & MASK)


def st_valid(e):
    L = e.cl
    L.rec("IsValidAnimation", (L.lab(e.u32(e.r(3))), e.r(4), e.r(5)))
    e.set_r(3, L.ret("IsValid:%d:%d" % (e.r(4), e.r(5))) & MASK)


def st_setnew(e):
    L, st = e.cl, e.r(3)
    first = e.u32(st + 0x10)
    L.rec("SetNewAnim", (L.lab(st), e.r(4), e.r(5), (e.r(6) - first) // 0x38, e.r(7), fb(e.f(1))))


def st_instance(e):
    L, sp, p = e.cl, e.r(1), e.r(5)
    x, z, rot = struct.unpack(">iii", e.read(p, 12))
    pair = e.u32(p + 0xC)
    p0, p1 = struct.unpack(">ii", e.read(pair, 8)) if pair else (0, 0)
    L.rec("InstanceRide", (e.r(3), e.r(4), x, z, rot, int(pair != 0), p0, p1, e.r(6), s32(e.r(7)), int(e.r(8) != 0), s32(e.r(9))))
    L.rec("InstanceRideMore", (s32(e.r(10)), e.u32(sp + 0x38), s32(e.u32(sp + 0x3C))))
    e.set_r(3, L.ret("InstanceRide") & MASK)


def st_animate(e):
    L, st = e.cl, e.r(3)
    L.rec("AnimateRideSet", (L.lab(st), s32(e.r(4)), e.r(5)))
    mesh = e.u32(st + 8)
    header = e.u32(mesh + 4)
    count = struct.unpack(">H", e.read(header + 0x48, 2))[0]
    records = e.u32(e.u32(mesh + 0x28) + 4)
    for i in range(count):
        block = e.u32(records + 0x14 * i + 4)
        v = L.ret("AnimateRideSet:%d" % i, 0)
        if block:
            e.write(block + 0x34, struct.pack(">I", v & MASK))


def st_destroy(e):
    e.cl.rec("DestroyRideInstance", (e.r(3),))


def st_dmcreate(e):
    L = e.cl
    L.rec("DMCreateMesh", (L.lab(e.r(3)), L.lab(e.r(4)), e.r(6)), (tx(e, e.r(5)),))
    e.set_r(3, L.ret("DMCreateMesh") & MASK)


def st_dmfree(e):
    e.cl.rec("DMFreeTextures", (e.cl.lab(e.r(3)),))


def st_identity(e):
    e.cl.rec("SetMatrixIdentity", (e.cl.lab(e.r(3)),))


for _a, _f in ((MRELEASEALLOCF, st_alloc), (LOADRIDETYPE, st_load), (CTRACE, st_ctrace), (COUNTNODES, st_count), (FINDNTH, st_find),
               (ISVALIDANIM, st_valid), (SETNEWANIM, st_setnew), (INSTANCERIDE, st_instance), (ANIMATERIDESET, st_animate),
               (DESTROYINSTANCE, st_destroy), (DMCREATEMESH, st_dmcreate), (DMFREETEXTURES, st_dmfree),
               (SETMATRIXIDENTITY, st_identity)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)       # the C library's sqrt (SetupCarType) ...
    bind(runner.lib)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory: the field offsets, the pointer width, byte order and the Ptr32 base."""

    def put(self, a, v, n=4):
        self.write(a, (v & ((1 << (8 * n)) - 1)).to_bytes(n, self.order))

    def get(self, a, n=4):
        return int.from_bytes(self.read(a, n), self.order)

    def fw(self, base, key, v, n=4):
        self.put(base + self.o[key], v, n)

    def fr(self, base, key, n=4):
        return self.get(base + self.o[key], n)

    def fwp(self, base, key, p):
        self.put(base + self.o[key], p, self.psz)

    def frp(self, base, key):
        return self.get(base + self.o[key], self.psz)

    def fw32p(self, base, key, p):
        self.put(base + self.o[key], (p - self.base) if p else 0, 4)

    def fr32p(self, base, key):
        v = self.get(base + self.o[key], 4)
        return v + self.base if v else 0

    def new(self, prefix):
        return self.alloc(self.o[prefix + ".size"])

    def sz(self, prefix):
        return self.o[prefix + ".size"]

    def mark(self, name, base, stride, lo=0, hi=1):
        self.marks.append((name, base, stride, lo, hi))

    def desc(self, p):
        """A pointer as a comparable value: null, an index in a marked block, or the label a fake gave the block."""
        if p == 0:
            return "null"
        for name, base, stride, lo, hi in self.marks:
            d = p - base
            if d % stride == 0 and lo <= d // stride < hi:
                return (name, d // stride)
        return ("L", self.lab(p))

    def words(self, a, ws):
        for i, w in enumerate(ws):
            self.put(a + 4 * i, w)

    def gtable(self, which, i):
        return (ADDR_TYPES if which == 0 else ADDR_SETS) + 4 * i if self.ppc else self.tables[which] + 8 * i


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    ppc = True
    order = "big"
    psz = 4
    base = 0

    def __init__(self, e, case):
        self.e, self.o, self.marks = e, MAC, []
        self.L = e.cl = CL.PpcLog({}, case["q"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n, alignment=16)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def text(self, t):
        b = t.encode("latin-1") + b"\0"
        return self.e.alloc(len(b), b, alignment=4)

    def label(self, n, a):
        self.L.labels[a] = n

    def lab(self, a):
        return self.L.lab(a)

    def log(self, op):
        return self.L.log

    def call(self, name, *args):
        return T.call_ppc(self.e, FN[name], [a & MASK for a in args])[0] & MASK


class Host(Side):
    """The same on the host library."""
    ppc = False
    order = "little"
    psz = 8

    def __init__(self, lib, case):
        self.lib, self.o, self.marks = lib, HOST, []
        self.base = lib.tpw_cld_base()
        self.tables = (lib.tpw_cld_addr(0), lib.tpw_cld_addr(1))
        CL.host_begin(lib, {}, case["q"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_cld_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def text(self, t):
        b = t.encode("latin-1") + b"\0"
        a = self.alloc(len(b))
        C.memmove(a, b, len(b))
        return a

    def label(self, n, a):
        self.lib.tpw_cl_label(n, a)

    def lab(self, a):
        return self.lib.tpw_cl_lab(a)

    def log(self, op):
        out = CL.host_end(self.lib)
        if op in ("selects", "defaults"):                      # the host select record is bigger: map the logged size to the Mac's
            ratio = self.o["sel.size"]
            out = [(n, (i[0] // ratio * MAC["sel.size"],) + tuple(i[1:]), t) if n == "MReleaseAllocF" and len(i) == 2 and i[0] % ratio == 0
                   else (n, i, t) for n, i, t in out]
        return out

    def call(self, name, *args):
        return getattr(self.lib, "tpw_cld_" + name)(*args)


def bind(lib):
    """The host library's entry points, typed once; the layout is read and checked here."""
    global HOST
    if getattr(lib, "bound", False):
        return
    U64, U32, I32, VP = C.c_uint64, C.c_uint32, C.c_int32, C.c_void_p
    sig = dict(addr=(U64, [I32]), alloc=(U64, [U32]), base=(U64, []), mesh=(None, [U64]), train=(None, [U64, U64]),
               car=(None, [U64, U64]), pylons=(None, [U64] * 4), pylon=(None, [U64, U64, U32]),
               selects=(I32, [U64, U64, U32, U64, I32]), defaults=(None, [U64, U32, U64, U64, U64, I32]),
               repack=(None, [U64] * 3), optimise=(None, [U64, U64, U32]), params=(U64, [I32] * 4))
    for k, (res, args) in sig.items():
        f = getattr(lib, "tpw_cld_" + k)
        f.restype, f.argtypes = res, args
    lib.tpw_cl_lab.restype, lib.tpw_cl_lab.argtypes = I32, [VP]
    lib.tpw_cl_label.argtypes = [I32, VP]
    out = (C.c_int64 * 256)()
    lib.tpw_cld_layout.argtypes = [C.POINTER(C.c_int64)]
    n = lib.tpw_cld_layout(out)
    assert n == len(FIELDS), (n, len(FIELDS))
    HOST = {k: int(out[i]) for i, (k, _) in enumerate(FIELDS)}
    for k, v in FIELDS:
        if k.startswith(SAME) and k != "hdr.size":
            assert HOST[k] == v, (k, HOST[k], v)
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

FLOAT_SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0xFF7FFFFF,
                 0x00800000, 0x80000001]
NODES_MAX = 6


def cf(v):
    return canon_f(v)


def build_ride(s, sp, lab0):
    """A ride set with its mesh, header (Ptr32 fields), nodes, root node, embedded entries and records, time controls and groups."""
    R = {}
    n, nem = len(sp["nodes"]), len(sp["emb"])
    nsz, esz, rsz = s.sz("nod"), s.sz("emb"), s.sz("emr")
    nodes = R["nodes"] = s.alloc(max(n, 1) * nsz)
    for i, nd in enumerate(sp["nodes"]):
        a = nodes + i * nsz
        s.fw(a, "nod.mFlags", nd["flags"])
        s.fw(a, "nod.mVertexCount", nd["vc"], 2)
        s.words(a + s.o["nod.mBox"], nd["box"])
    root = s.alloc(nsz)
    s.words(root + 0x40, sp["root"])
    emb = s.alloc((nem + 2) * esz)
    for i in range(nem + 2):
        s.put(emb + i * esz + s.o["emb.mF04"], sp["guard"] if i in (0, nem + 1) else sp["emb"][i - 1])
    recs = R["recs"] = s.alloc((nem + 2) * rsz)
    R["blocks"] = []
    for i in range(nem):
        blk = 0
        if sp["blocks"][i] is not None:
            blk = s.alloc(0x40)
            s.put(blk + 0x34, sp["blocks"][i])
        R["blocks"].append(blk)
        s.fwp(recs + (i + 1) * rsz, "emr.mBlock", blk)
    ems = s.new("ems")
    s.fwp(ems, "ems.mRecords", recs + rsz)
    hdr = R["hdr"] = s.new("hdr")
    s.fw(hdr, "hdr.mNodeCount", n, 2)
    s.fw(hdr, "hdr.mEmbeddedCount", nem, 2)
    s.fw32p(hdr, "hdr.mNodes", nodes)
    s.fw32p(hdr, "hdr.mF78", root)
    s.fw32p(hdr, "hdr.mEmbedded", emb + esz)
    s.words(hdr + s.o["hdr.mBox"], sp["hbox"])
    mesh = s.new("msh")
    s.fwp(mesh, "msh.mHeader", hdr)
    s.fwp(mesh, "msh.mF28", ems)
    tcs = R["tcs"] = s.alloc(8 * 0x38)
    for k, (f1c, f20) in enumerate(sp["tcs"]):
        s.put(tcs + 0x38 * k + 0x1C, f1c)
        s.put(tcs + 0x38 * k + 0x20, f20)
    st = R["set"] = s.new("ras")
    s.fw(st, "ras.mFlags", sp["flags"])
    s.fwp(st, "ras.mHeads", mesh)
    s.fwp(st, "ras.mTimeControls", tcs)
    for k, g in enumerate(sp["groups"]):
        s.put(st + s.o["ras.mGroups"] + k * s.sz("grp"), g)
    s.label(lab0, st)
    R["n"], R["nem"] = n, nem
    return R


def obs_ride(s, R):
    """What the functions may change in a ride: the set's flags, the header's and the nodes' boxes and flags, the first time control's
    word at +0x20, the float at +0x34 of the records' blocks."""
    nsz = s.sz("nod")
    return {"flags": s.fr(R["set"], "ras.mFlags"),
            "hbox": [cf(s.get(R["hdr"] + s.o["hdr.mBox"] + 4 * k)) for k in range(6)],
            "nodes": [(s.fr(R["nodes"] + i * nsz, "nod.mFlags"),
                       [cf(s.get(R["nodes"] + i * nsz + s.o["nod.mBox"] + 4 * k)) for k in range(6)]) for i in range(max(R["n"], 1))],
            "tc": [cf(s.get(R["tcs"] + 0x38 * k + 0x20)) for k in range(6)],
            "blk": [cf(s.get(b + 0x34)) if b else None for b in R["blocks"]]}


def build_world(s, c):
    """The ride types (gRideTypes[id], set label 10 + id) and the instances (gRideAnimSets[slot], label 20 + slot) of the case."""
    W = {"types": {}, "insts": {}}
    for which in (0, 1):
        for i in range(NSLOTS):
            s.put(s.gtable(which, i), 0, s.psz)
    for i, sp in c.get("types", {}).items():
        W["types"][i] = R = build_ride(s, sp, 10 + i)
        s.put(s.gtable(0, i), R["set"], s.psz)
    for i, sp in c.get("insts", {}).items():
        W["insts"][i] = R = build_ride(s, sp, 20 + i)
        s.put(s.gtable(1, i), R["set"], s.psz)
    return W


def obs_world(s, W):
    return {"types": {i: obs_ride(s, R) for i, R in W["types"].items()}, "insts": {i: obs_ride(s, R) for i, R in W["insts"].items()}}


CAR_F = ("mFlags", "mValue", "mRideType", "mF10", "mF14", "mF18", "mF1C", "mNodeIndex", "mF24", "mF28", "mHeadCount")
CAR_FLOAT = ("mF10", "mF14", "mF18", "mF1C", "mF24", "mF28")


def build_car(s, W, sp, sentinel):
    car = s.new("car")
    for k in CAR_F:
        s.fw(car, "car." + k, sp[k])
    s.fwp(car, "car.mName", s.text(sp["name"]))
    s.fwp(car, "car.mBlock", sentinel if sp["block"] else 0)
    return car


def obs_car(s, car):
    out = {k: (cf(s.fr(car, "car." + k)) if k in CAR_FLOAT else s.fr(car, "car." + k)) for k in CAR_F}
    count = out["mHeadCount"]
    p = s.frp(car, "car.mBlock")
    out["block"] = s.desc(p)
    out["heads"] = [s.get(p + 4 * i) for i in range(count)] if p and count <= 64 and out["block"][0] == "L" else None
    return out


PYL_INT = ["mF%02X" % o for o in range(0x18, 0x58, 4)] + ["mFB4", "mFB8", "mF0C"]
PYL_FLT = ["mF68", "mFBC", "mFC0", "mFC4"]
PYL_OBS = ["mFlags", "mRideType"] + PYL_INT + PYL_FLT


def build_pylon(s, sp):
    p = s.new("pyl")
    s.fw(p, "pyl.mFlags", sp["mFlags"])
    s.fw(p, "pyl.mRideType", sp["mRideType"])
    for k in PYL_INT:
        s.fw(p, "pyl." + k, sp[k])
    for k in PYL_FLT:
        s.fw(p, "pyl." + k, sp[k])
    s.words(p + s.o["pyl.mFD0"], sp["fd0"])
    s.fwp(p, "pyl.mName", s.text(sp["name"]) if sp["name"] is not None else 0)
    return p


def obs_pylon(s, p):
    out = {k: (cf(s.fr(p, "pyl." + k)) if k in PYL_FLT else s.fr(p, "pyl." + k)) for k in PYL_OBS}
    out["fd0"] = [s.get(p + s.o["pyl.mFD0"] + 4 * i) for i in range(4)]
    return out


# ---------------------------------------------------------------------------------------------------------------- the operations


def op_mesh(s, c):
    W = build_world(s, c)
    t, tex, ms, ent, mat = s.new("typ"), s.new("tex"), s.alloc(0x40), s.new("dme"), s.alloc(0x40)
    s.label(31, tex)
    s.label(32, ms)
    s.label(33, mat)
    s.fw(tex, "tex.mCount", c["tex"], 2)
    s.fwp(t, "typ.mF14", ent)
    s.fwp(t, "typ.mMeshSet", ms)
    s.fwp(t, "typ.mTextures", tex)
    for k, v in zip(("mF30", "mF34", "mF38", "mF3C"), c["w"]):
        s.fw(t, "typ." + k, v)
    s.fw(ent, "dme.mFlags", c["ent"][0])
    for k, v in zip(("mFaceCount", "mVertexCount", "mUVCount", "mFaceSetCount", "mPointCount", "mF0E"), c["ent"][1:7]):
        s.fw(ent, "dme." + k, v, 2)
    s.words(ent + s.o["dme.mBox"], c["ent"][7:13])
    s.fwp(ent, "dme.mMatrix", mat)
    s.call("mesh", t)
    return {"ent": [s.fr(ent, "dme.mFlags")] + [s.fr(ent, "dme." + k, 2) for k in
                                                ("mFaceCount", "mVertexCount", "mUVCount", "mFaceSetCount", "mPointCount", "mF0E")],
            "box": [s.get(ent + s.o["dme.mBox"] + 4 * k) for k in range(6)], "matrix": s.frp(ent, "dme.mMatrix") == mat,
            "type": [s.fr(t, "typ." + k) for k in ("mF30", "mF34", "mF38", "mF3C")], "tex": s.fr(tex, "tex.mCount", 2),
            "mf14": s.frp(t, "typ.mF14") == ent}


def op_car(s, c):
    W = build_world(s, c)
    sent = s.alloc(16)
    s.mark("S", sent, 16)
    car = build_car(s, W, c["car"], sent)
    folder = s.text(c["folder"])
    s.call("car", car, folder)
    return {"car": obs_car(s, car), "world": obs_world(s, W)}


def op_train(s, c):
    W = build_world(s, c)
    sent = s.alloc(16)
    s.mark("S", sent, 16)
    cars = [build_car(s, W, sp, sent) for sp in c["cars"]]
    t = s.new("typ")
    for k, i in zip(("mCarTypeA", "mCarTypeB", "mCarTypeC"), c["pick"]):
        s.fwp(t, "typ." + k, cars[i])
    for k in ("mF2F4", "mF2F8", "mF30C", "mF310", "mF314"):
        s.fw(t, "typ." + k, c["t"][k])
    s.call("train", t, s.text(c["folder"]))
    return {"type": [s.fr(t, "typ.mF2F4"), s.fr(t, "typ.mF2F8")] + [cf(s.fr(t, "typ." + k)) for k in ("mF30C", "mF310", "mF314")], "cars": [obs_car(s, a) for a in cars],
            "world": obs_world(s, W)}


def op_pylon(s, c):
    W = build_world(s, c)
    t = s.new("typ")
    p = build_pylon(s, c["pyl"])
    s.call("pylon", t, p, c["id"])
    return {"pylon": obs_pylon(s, p), "world": obs_world(s, W)}


def op_pylons(s, c):
    W = build_world(s, c)
    t = s.new("typ")
    s.fw(t, "typ.mFlags", c["tflags"])
    ps = [build_pylon(s, sp) for sp in c["pyl"]]
    s.call("pylons", t, ps[0], ps[1], s.text(c["folder"]))
    return {"type": s.fr(t, "typ.mFlags"), "pylons": [obs_pylon(s, p) for p in ps], "world": obs_world(s, W)}


def build_balance(s, c):
    b = s.alloc(s.sz("bal") + 16)
    rng = random.Random(c["seed"])
    words = [rng.getrandbits(32) for _ in range(3806)]
    for off, v in c["slots"].items():
        words[(off - 8) // 4] = v
    pad = b"\0" * (s.o["bal.entry"] - 4)
    s.write(b + s.o["bal.mValues"], b"".join(w.to_bytes(4, s.order) + pad for w in words))
    return b


def build_records(s, count, extra=4):
    rsz = s.sz("rec")
    base = s.alloc((count + extra) * rsz)
    s.mark("rec", base, rsz, 0, count + extra)
    return base


def obs_selects(s, t):
    n = s.fr(t, "typ.mSelectCount")
    p = s.frp(t, "typ.mF78")
    out = {"flags": s.fr(t, "typ.mFlags"), "sflags": s.fr(t, "typ.mSelectFlags"), "count": n, "blk": s.desc(p),
           "a": s.desc(s.frp(t, "typ.mSelectA")), "b": s.desc(s.frp(t, "typ.mSelectB")), "sel": None}
    if p and n <= 24 and out["blk"][0] == "L":
        ssz = s.sz("sel")
        out["sel"] = [([cf(s.get(p + i * ssz + 4 * k)) for k in range(8)], s.desc(s.frp(p + i * ssz, "sel.mF20")),
                       s.desc(s.frp(p + i * ssz, "sel.mF24")), s.desc(s.frp(p + i * ssz, "sel.mF28"))) for i in range(n)]
    return out


def fill_type(s, t, c):
    for k, v in c["t"].items():
        s.fw(t, "typ." + k, v)
    s.fwp(t, "typ.mF78", 0)
    s.fwp(t, "typ.mSelectA", 0)
    s.fwp(t, "typ.mSelectB", 0)


def op_selects(s, c):
    t = s.new("typ")
    fill_type(s, t, c)
    recs = build_records(s, c["count"])
    bal = build_balance(s, c)
    r = s.call("selects", t, recs, c["count"], bal, c["flag"])
    return {"ret": T.s32(r), "type": obs_selects(s, t)}


def op_defaults(s, c):
    t = s.new("typ")
    fill_type(s, t, c)
    recs = build_records(s, 2, 8)
    out = s.alloc(16) if c["out"] else 0
    s.fwp(out, "typ.mFlags", 0) if False else None
    if out:
        s.put(out, 0, s.psz)
    a = recs + s.sz("rec") * c["ia"] if c["ia"] is not None else 0
    b = recs + s.sz("rec") * c["ib"] if c["ib"] is not None else 0
    s.call("defaults", t, c["count"], out, a, b, c["flag"])
    o = obs_selects(s, t)
    o["out"] = s.desc(s.get(out, s.psz)) if out else None
    return o


def build_record(s, base_pts, sp, ptsb):
    """A cross-section record: sp = (np, edges, ...); its edges block; returns the record's address."""
    rec = s.new("rec")
    esz = s.sz("edg")
    edges = s.alloc(max(len(sp["edges"]), 1) * esz)
    for i, (f0, f4, f6, f8, pad) in enumerate(sp["edges"]):
        a = edges + i * esz
        s.fw(a, "edg.mF00", f0)
        s.fw(a, "edg.mF04", f4, 2)
        s.fw(a, "edg.mF06", f6, 2)
        s.fw(a, "edg.mF08", f8, 2)
        s.put(a + 0xA, pad, 2)
    s.fw(rec, "rec.mF00", sp["np"])
    s.fw(rec, "rec.mPointCount", len(sp["edges"]))
    s.fwp(rec, "rec.mPoints", base_pts)
    s.fwp(rec, "rec.mBlock", edges)
    s.fwp(rec, "rec.mPointsB", ptsb)
    return rec, edges


def fill_points(s, a, pts):
    for i, p in enumerate(pts):
        s.words(a + 0x14 * i, p)


def read_points(s, a, n):
    return [[s.get(a + 0x14 * i + 4 * k) for k in range(5)] for i in range(n)]


def obs_edges(s, edges, n):
    esz = s.sz("edg")
    return [(s.fr(edges + i * esz, "edg.mF00"), s.fr(edges + i * esz, "edg.mF04", 2), s.fr(edges + i * esz, "edg.mF06", 2),
             s.fr(edges + i * esz, "edg.mF08", 2), s.get(edges + i * esz + 0xA, 2)) for i in range(n)]


def op_repack(s, c):
    pts = s.alloc(0x14 * 24)
    fill_points(s, pts, c["pts"])
    other = s.alloc(0x14 * 24)
    fill_points(s, other, c["dst"])
    dst = pts if c["same"] else other
    s.mark("src", pts, 0x14, 0, 24)
    s.mark("dst", other, 0x14, 0, 24)
    rec, edges = build_record(s, pts, c["rec"], 0)
    used = s.alloc(4 * 24)
    s.words(used, c["used"])
    s.call("repack", used, rec, dst)
    return {"used": [s.get(used + 4 * i) for i in range(24)], "src": read_points(s, pts, 24), "dst": read_points(s, other, 24),
            "edges": obs_edges(s, edges, len(c["rec"]["edges"])), "rec": [s.fr(rec, "rec.mF00"), s.fr(rec, "rec.mPointCount")],
            "pts": s.desc(s.frp(rec, "rec.mPoints"))}


def op_optimise(s, c):
    t = s.new("typ")
    sent = s.alloc(16)
    s.mark("S", sent, 16)
    s.fwp(t, "typ.mF98", sent)
    pool = []
    for k, pts in enumerate(c["pool"]):
        a = s.alloc(0x14 * 24)
        fill_points(s, a, pts)
        s.mark("pool%d" % k, a, 0x14, 0, 24)
        pool.append(a)
    count = len(c["recs"])
    rsz = s.sz("rec")
    base = s.alloc((count + 2) * rsz)
    s.mark("recs", base, rsz, 0, 8)
    handles = []
    for i, sp in enumerate(c["recs"]):
        r, edges = build_record(s, pool[sp["pool"]], sp, pool[sp["poolb"]] if sp["poolb"] is not None else 0)
        # the record's words go into the table (build_record made a record of its own: copy it)
        s.write(base + i * rsz, s.read(r, rsz))
        handles.append((base + i * rsz, edges))
    s.call("optimise", t, base, count)
    log = s.log("optimise")
    f98 = s.frp(t, "typ.mF98")
    total = log[0][1][0] // 0x14 if f98 and log and log[0][0] == "MReleaseAllocF" else 0
    if f98 and total:
        s.mark("f98", f98, 0x14, 0, total + 1)
    return {"f98": s.desc(f98), "f98pts": read_points(s, f98, total) if f98 and total and total <= 200 else None,
            "recs": [(s.fr(a, "rec.mF00"), s.fr(a, "rec.mPointCount"), s.desc(s.frp(a, "rec.mPoints")), s.desc(s.frp(a, "rec.mPointsB")),
                      obs_edges(s, e, len(sp["edges"]))) for (a, e), sp in zip(handles, c["recs"])],
            "pool": [read_points(s, a, 24) for a in pool], "log": log}


def op_params(s, c):
    out = []
    first = None
    for a, b, x, z in c["calls"]:
        p = s.call("params", a, b, x, z)
        first = p if first is None else first
        pair = s.get(p + s.o["prm.mF0C"], s.psz)
        out.append({"same": p == first, "rec": [s.get(p + s.o["prm.mX"]), s.get(p + s.o["prm.mZ"]), s.get(p + s.o["prm.mRotation"])],
                    "pair": [s.get(pair), s.get(pair + 4)], "first": [s.get(first + s.o["prm.mX"]), s.get(first + s.o["prm.mZ"])],
                    "pairsame": pair == s.get(first + s.o["prm.mF0C"], s.psz)})
        if s.ppc:
            assert p == ADDR_PARAMS and pair == ADDR_PAIR, (hex(p), hex(pair))
    return {"calls": out}


OPS = {"mesh": op_mesh, "car": op_car, "train": op_train, "pylon": op_pylon, "pylons": op_pylons, "selects": op_selects,
       "defaults": op_defaults, "repack": op_repack, "optimise": op_optimise, "params": op_params}


def run(s, c):
    try:
        out = OPS[c["op"]](s, c)
        if c["op"] != "optimise":
            out["log"] = s.log(c["op"])
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    bind(lib)
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rf(r):
    """A float's bits: mostly ordinary numbers of every scale, now and then a special value."""
    k = r.random()
    if k < 0.1:
        return r.choice(FLOAT_SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def ru(r):
    return r.choice([0, 1, 2, 3, 4, 5, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.randrange(16)])


def ride_spec(r, nem=None, nn=None):
    n = r.choice([0, 1, 2, 3, 4, NODES_MAX]) if nn is None else nn
    nem = r.choice([0, 1, 2, 3, 5, 8]) if nem is None else nem
    return {"flags": r.getrandbits(32),
            "nodes": [{"flags": r.getrandbits(32), "vc": r.choice([0, 1, 2, 3, 0xFFFF, 0x8000, r.randrange(65536)]),
                       "box": [rf(r) for _ in range(6)]} for _ in range(n)],
            "root": [rf(r) for _ in range(3)], "hbox": [rf(r) for _ in range(6)],
            "emb": [r.choice([0, 1, 2, 3, 4, 5, 6, 0xFFFFFFFF]) for _ in range(nem)], "guard": r.choice([0, 1, 2, 3, 4]),
            "blocks": [None if r.random() < 0.25 else rf(r) for _ in range(nem)],
            "tcs": [(rf(r), rf(r)) for _ in range(6)], "groups": [r.randrange(8) for _ in range(12)]}


def car_spec(r):
    return {"mFlags": r.choice([0, 1, 1, 1, 2, 3, r.getrandbits(32)]), "mValue": r.getrandbits(32), "mRideType": ru(r),
            "mF10": rf(r), "mF14": rf(r), "mF18": rf(r), "mF1C": rf(r), "mNodeIndex": ru(r), "mF24": rf(r), "mF28": rf(r),
            "mHeadCount": r.choice([0, 1, 3, r.getrandbits(32)]), "block": r.random() < 0.5,
            "name": "car%d" % r.randrange(1000)}


def pylon_spec(r, nem, named=True):
    sp = {"mFlags": r.choice([0, 8, 0x7FF, r.getrandbits(16), r.getrandbits(32)]), "mRideType": ru(r), "fd0": [rf(r) for _ in range(4)],
          "name": ("py%d" % r.randrange(100)) if named else None}
    for k in PYL_INT:
        sp[k] = r.choice([0, 1, -1, r.randrange(-5, 40), r.getrandbits(32)]) & MASK
    for k in PYL_FLT:
        sp[k] = rf(r)
    sp["mF2C"] = r.randint(-1, nem) & MASK
    sp["mFB4"], sp["mFB8"] = r.randrange(0, 30), r.randrange(0, 30)
    return sp


def car_queues(r, q, ids):
    q["LoadRideType"] = [r.choice(ids) for _ in range(4)]
    q["Count:128"] = [r.choice([0, 0, 1, 2, 3, 5, 6, 9]) for _ in range(4)]
    for k in range(1, 13):
        q["Find:128:%d" % k] = [r.choice([-1, 0, 1, 2, 5, r.randrange(-3, 1000)]) for _ in range(4)]


def pylon_queues(r, q, nem, insts):
    for a, b in ((2, 0), (3, 0), (9, 0), (10, 0), (10, 1)):
        q["IsValid:%d:%d" % (a, b)] = [r.choice([0, 1, 1, 2, -1]) for _ in range(4)]
    for kind, nfind in ((0x1000000, 5), (0x800000, 1), (0x2000000, 5), (0x100000, 1), (0x80000, 4), (0x400000, 0)):
        q["Count:%d" % kind] = [r.choice([0, 1, 1, 2, 3, 5]) for _ in range(4)]
        for k in range(1, nfind + 1):
            q["Find:%d:%d" % (kind, k)] = [r.randint(-1, nem) for _ in range(5)]
    q["InstanceRide"] = [r.choice(insts) for _ in range(4)]
    for i in range(nem):
        q["AnimateRideSet:%d" % i] = [rf(r) for _ in range(4)]


def base_case(op, r):
    return {"op": op, "q": {}}


def mesh_case(r):
    c = base_case("mesh", r)
    c["q"]["DMCreateMesh"] = [r.choice([0, 1, -1, 7])]
    c["tex"] = r.choice([0, 1, 2, 5, 0xFFFF, r.randrange(65536)])
    c["w"] = [r.choice([0, 1, 100, 0xFFFF, 0x10000, 0x12345, r.getrandbits(32)]) for _ in range(4)]
    c["ent"] = [r.getrandbits(32)] + [r.randrange(65536) for _ in range(6)] + [rf(r) for _ in range(6)]
    return c


def mesh_edges():
    r = random.Random(1481)
    for ans in (0, 1, -1, 7):
        for tex in (0, 1, 0xFFFF):
            c = mesh_case(r)
            c["q"]["DMCreateMesh"] = [ans]
            c["tex"] = tex
            yield c
    for w in ([0xFFFF, 0x10000, 0x1FFFF, 0xFFFFFFFF], [0, 0, 0, 0]):
        c = mesh_case(r)
        c["w"] = w
        yield c


def car_case(r, ids=(0, 1, 2, 3, 3, 4)):
    c = base_case("car", r)
    car_queues(r, c["q"], list(ids))
    c["types"] = {i: ride_spec(r) for i in (1, 2, 3)}
    c["car"] = car_spec(r)
    c["folder"] = "dir%d" % r.randrange(100)
    return c


def car_edges():
    r = random.Random(1482)
    for ride in (0, 1, 3, 4):                                     # id 0 (not loaded), a null slot, and loaded ones
        for flags in (0, 1):
            c = car_case(r)
            c["q"]["LoadRideType"] = [ride]
            c["car"]["mFlags"] = flags
            yield c
    for count in (0, 1, 2, 5, 12):
        for flags in (0, 1):
            c = car_case(r)
            c["q"]["LoadRideType"] = [2]
            c["q"]["Count:128"] = [count]
            c["car"]["mFlags"] = flags
            yield c
    for nn in (0, 1, 3, 6):                                       # the widened node: none, ties on the vertex count
        for vc in ([0] * 6, [5, 5, 5, 5, 5, 5], [1, 2, 3, 3, 2, 1], [0xFFFF, 1, 0xFFFF, 0, 0, 0]):
            c = car_case(r)
            c["q"]["LoadRideType"] = [1]
            c["car"]["mFlags"] = 1
            c["types"][1] = ride_spec(r, nn=nn)
            for nd, v in zip(c["types"][1]["nodes"], vc):
                nd["vc"] = v
            yield c
    for special in FLOAT_SPECIAL:                                 # NaN, infinities, -0.0 and huge values in the boxes and the corners
        for slot in range(3):
            c = car_case(r)
            c["q"]["LoadRideType"] = [3]
            c["car"]["mFlags"] = 1
            sp = c["types"][3]
            sp["hbox"][r.randrange(6)] = special
            sp["root"][r.randrange(3)] = special
            if sp["nodes"]:
                sp["nodes"][r.randrange(len(sp["nodes"]))]["box"][r.randrange(6)] = special
            c["car"]["mF10"] = special if slot == 0 else c["car"]["mF10"]
            c["car"]["mF14"] = special if slot == 1 else c["car"]["mF14"]
            yield c
    c = car_case(r)
    c["q"]["LoadRideType"] = [2]
    c["car"]["mFlags"] = 1
    c["types"][2]["hbox"] = [fb(-1e30), fb(-1e30), fb(-1e30), fb(1e30), fb(1e30), fb(1e30)]
    c["types"][2]["root"] = [0, 0, 0]
    yield c


def car_random(r):
    return car_case(r)


def train_case(r):
    c = base_case("train", r)
    car_queues(r, c["q"], [1, 2, 3, 3, 4, 0])
    c["types"] = {i: ride_spec(r) for i in (1, 2, 3)}
    c["cars"] = [car_spec(r) for _ in range(3)]
    c["pick"] = [r.randrange(3) for _ in range(3)]
    c["folder"] = "dir%d" % r.randrange(100)
    c["t"] = {"mF2F4": r.choice([0, 1, 2, 3, 4, 7, 40, 300]), "mF2F8": r.getrandbits(32), "mF30C": r.getrandbits(32),
              "mF310": r.getrandbits(32), "mF314": r.getrandbits(32)}
    return c


def train_edges():
    r = random.Random(1483)
    for pick in ((0, 1, 2), (1, 1, 1), (0, 0, 0), (2, 1, 0), (0, 1, 0), (0, 1, 1), (1, 1, 2), (0, 0, 1), (2, 0, 2)):
        for n in (0, 1, 2, 3, 6):
            c = train_case(r)
            c["pick"] = list(pick)
            c["t"]["mF2F4"] = n
            yield c
    c = train_case(r)
    c["t"]["mF2F4"] = 250
    c["pick"] = [0, 1, 2]
    yield c
    c = train_case(r)
    c["q"]["LoadRideType"] = [0]
    yield c


def train_random(r):
    return train_case(r)


def pylon_case(r, ids=(0, 1, 2, 3, 4)):
    c = base_case("pylon", r)
    nem = r.choice([0, 1, 2, 3, 5, 8])
    c["id"] = r.choice(list(ids))
    c["types"] = {i: ride_spec(r, nem=nem) for i in (1, 2, 3)}
    c["insts"] = {i: ride_spec(r, nem=nem) for i in (1, 2, 3)}
    pylon_queues(r, c["q"], nem, [1, 2, 3])
    c["pyl"] = pylon_spec(r, nem)
    c["nem"] = nem
    return c


def pylon_edges():
    r = random.Random(1484)
    for i in (0, 4, 1, 2, 3):
        c = pylon_case(r)
        c["id"] = i
        yield c
    for flags in (0, 8, 0x20, 0x40, 0x80, 0x100, 0x200, 0x3E0, 0x3E8, 0x2000, 0xFFFFFFFF):
        c = pylon_case(r, ids=(1, 2, 3))
        c["pyl"]["mFlags"] = flags
        yield c
    for kinds in range(0, 64):                                    # which of the six data kinds have nodes
        c = pylon_case(r, ids=(1, 2, 3))
        for bit, kind in enumerate((0x1000000, 0x800000, 0x2000000, 0x100000, 0x80000, 0x400000)):
            c["q"]["Count:%d" % kind] = [r.choice([1, 2, 3]) if kinds >> bit & 1 else 0]
        c["pyl"]["mFlags"] = r.choice([0, 8, 0x20])
        yield c
    for count in (1, 2, 3, 4, 7):                                 # the 0x1000000 / 0x2000000 loops with flag 8 set and clear
        for flags in (0, 8):
            c = pylon_case(r, ids=(1, 2, 3))
            c["q"]["Count:16777216"] = [count]
            c["q"]["Count:33554432"] = [count]
            c["pyl"]["mFlags"] = flags
            yield c
    for value in range(0, 7):                                     # the embedded values 0..6 of the 0x80000 switch and the others
        c = pylon_case(r, ids=(1, 2, 3))
        c["id"] = 1
        c["types"][1] = ride_spec(r, nem=c["nem"])
        c["types"][1]["emb"] = [value] * c["nem"]
        c["types"][1]["guard"] = value
        c["q"]["Count:524288"] = [2]
        yield c
    for special in FLOAT_SPECIAL:                                 # the clamps: NaN, infinities, -0.0 and huge values in mFD0, mF68 and the blocks
        for slot in range(4):
            c = pylon_case(r, ids=(1, 2, 3))
            c["pyl"]["fd0"][slot] = special
            c["pyl"]["mF68"] = special if slot == 0 else c["pyl"]["mF68"]
            for sp in c["insts"].values():
                sp["tcs"][0] = (special if slot == 1 else sp["tcs"][0][0], sp["tcs"][0][1])
            if slot == 3:
                c["q"]["AnimateRideSet:%d" % 0] = [special, special]
            yield c
    for lo, hi in ((0.0, 1.0), (1.0, 0.0), (0.0, 0.0), (-1.0, 1.0)):   # ordered, reversed and empty clamp ranges
        for k in (0.0, 1.0, 2.0, -3.0):
            c = pylon_case(r, ids=(1, 2, 3))
            c["pyl"]["fd0"] = [fb(lo), fb(hi), fb(k), fb(0.5)]
            yield c
    for groups in (0, 1, 2, 3, 4, 7):                             # the animation start needs bits 0 or 1 of the group's flags
        c = pylon_case(r, ids=(1, 2, 3))
        for sp in c["insts"].values():
            sp["groups"] = [groups] * 12
        for key in ("IsValid:2:0", "IsValid:3:0", "IsValid:9:0", "IsValid:10:0", "IsValid:10:1"):
            c["q"][key] = [1]
        yield c


def pylon_random(r):
    return pylon_case(r)


def pylons_case(r):
    c = base_case("pylons", r)
    nem = r.choice([0, 1, 2, 3, 5, 8])
    c["types"] = {i: ride_spec(r, nem=nem) for i in (1, 2, 3)}
    c["insts"] = {i: ride_spec(r, nem=nem) for i in (1, 2, 3)}
    pylon_queues(r, c["q"], nem, [1, 2, 3])
    c["q"]["LoadRideType"] = [r.choice([0, 1, 2, 3, 3, 4]) for _ in range(4)]
    c["pyl"] = [pylon_spec(r, nem, named=r.random() < 0.75) for _ in range(2)]
    c["tflags"] = r.choice([0, 1, 2, 3, 0x40, r.getrandbits(32)])
    c["folder"] = "dir%d" % r.randrange(100)
    return c


def pylons_edges():
    r = random.Random(1485)
    for names in ((False, False), (True, False), (False, True), (True, True)):
        for ids in ([0, 0], [1, 0], [0, 2], [3, 3], [4, 1]):
            c = pylons_case(r)
            c["pyl"] = [pylon_spec(r, 3, named=n) for n in names]
            c["q"]["LoadRideType"] = ids
            for sp in c["pyl"]:
                sp["mFlags"] = r.choice([0, 1, 8, 0xFFFFFFFE])
            yield c
    for tflags in (0, 1, 2, 3, 0xFFFFFFFF):
        c = pylons_case(r)
        c["tflags"] = tflags
        c["pyl"] = [pylon_spec(r, 3, named=True) for _ in range(2)]
        c["q"]["LoadRideType"] = [1, 2]
        yield c


def pylons_random(r):
    return pylons_case(r)


def selects_case(r):
    c = base_case("selects", r)
    count = r.choice([0, 1, 2, 3, 5])
    nsel = r.choice([0, 1, 1, 2, 3, 5, 20, 21, 22])
    c["count"], c["flag"], c["seed"] = count, r.choice([0, 1, -1, 5]), r.getrandbits(32)
    c["t"] = {"mFlags": r.getrandbits(32), "mSelectFlags": r.getrandbits(32), "mSelectCount": r.getrandbits(32)}
    slots = {}
    slots[0x3B04] = r.choice([0, 1, count, count + 1, r.randrange(0, count + 4)])
    ref = lambda: r.choice([0, 1, count, count + 1, r.randrange(1, count + 1) if count else 1, r.randrange(1, count + 1) if count else 1])
    for i in range(min(nsel, 22)):
        b = 0x3534 + 0x2C * i
        for p in range(4):
            k = r.random()
            if k < 0.25:
                f1 = f2 = r.choice([0, 0x80000000])
            elif k < 0.35:
                f1, f2 = 0, rf(r)
            elif k < 0.45:
                f1, f2 = rf(r), r.choice([0, 0x80000000])
            else:
                f1, f2 = rf(r), rf(r)
            slots[b + 8 * p], slots[b + 8 * p + 4] = f1, f2
        slots[b + 0x20] = ref() if r.random() < 0.85 else r.choice([0, count + 1])
        slots[b + 0x24] = r.choice([0, ref(), ref()])
        slots[b + 0x28] = r.choice([0, ref(), ref()])
    slots[0x38A4] = nsel
    c["slots"] = slots
    return c


def selects_edges():
    r = random.Random(1486)
    for count in (0, 1, 3):
        for nsel in (0, 1, 2, 4):
            for number in (0, 1, count, count + 1):
                for flag in (0, 1):
                    c = selects_case(r)
                    c["count"], c["flag"] = count, flag
                    c["slots"][0x3B04] = number
                    c["slots"][0x38A4] = nsel
                    for i in range(nsel):
                        b = 0x3534 + 0x2C * i
                        c["slots"][b + 0x20] = 1 if count else 0
                        c["slots"][b + 0x24] = r.choice([0, 1, count])
                        c["slots"][b + 0x28] = r.choice([0, 1, count])
                    yield c
    for first, second, third in ((0, 0, 0), (1, 0, 0), (2, 1, 3), (4, 0, 0), (1, 4, 0), (1, 0, 4), (3, 3, 3), (1, 2, 5), (1, 2, 0)):
        c = selects_case(r)
        c["count"] = 3
        c["slots"][0x38A4] = 2
        for i in range(2):
            b = 0x3534 + 0x2C * i
            c["slots"][b + 0x20], c["slots"][b + 0x24], c["slots"][b + 0x28] = first, second, third
        yield c
    for special in FLOAT_SPECIAL:                                  # pairs with a NaN, infinity, -0.0 in either half
        for half in (0, 1):
            c = selects_case(r)
            c["count"] = 2
            c["slots"][0x38A4] = 1
            c["slots"][0x3534 + 0x20] = 1
            c["slots"][0x3534 + 0x24] = 0
            c["slots"][0x3534 + 0x28] = 0
            for p in range(4):
                c["slots"][0x3534 + 8 * p + 4 * half] = special
                c["slots"][0x3534 + 8 * p + 4 * (1 - half)] = r.choice([0, 0x80000000, fb(2.0)])
            yield c
    c = selects_case(r)
    c["slots"][0x38A4] = 20
    c["count"] = 4
    for i in range(20):
        b = 0x3534 + 0x2C * i
        c["slots"][b + 0x20] = 1
        c["slots"][b + 0x24] = 2
        c["slots"][b + 0x28] = 3
    yield c


def selects_random(r):
    return selects_case(r)


def defaults_case(r):
    c = base_case("defaults", r)
    c["count"] = r.choice([0, 1, 2, 3, 5, 12, 24])
    c["flag"] = r.choice([0, 1, -1, 5, 0x100])
    c["out"] = r.random() < 0.85
    c["ia"] = r.choice([None, 0, 1, 2])
    c["ib"] = r.choice([None, 0, 1, 2])
    c["t"] = {"mFlags": r.getrandbits(32), "mSelectFlags": r.choice([0, 1, 2, r.getrandbits(32)]), "mSelectCount": r.getrandbits(32)}
    return c


def defaults_edges():
    r = random.Random(1487)
    for count in (0, 1, 2, 24):
        for flag in (0, 1, -1):
            for out in (True, False):
                c = defaults_case(r)
                c["count"], c["flag"], c["out"] = count, flag, out
                yield c
    for sf in (0, 1, 2, 0xFFFFFFFF):
        for flag in (0, 1):
            c = defaults_case(r)
            c["t"]["mSelectFlags"], c["flag"], c["out"] = sf, flag, True
            yield c


def defaults_random(r):
    return defaults_case(r)


def edge_sp(r, np, ne, valid=True, mode="rand"):
    edges = []
    for _ in range(ne):
        hi = np if valid and np else (24 if not valid else 1)
        edges.append((r.getrandbits(32), r.getrandbits(16), r.randrange(hi), r.randrange(hi), r.getrandbits(16)))
    return {"np": np, "edges": edges}


def pts_list(r, n=24):
    return [[rf(r) for _ in range(5)] for _ in range(n)]


def repack_case(r, np=None, mode=None, same=None):
    c = base_case("repack", r)
    np = r.choice([0, 1, 2, 3, 5, 8, 21]) if np is None else np
    c["rec"] = edge_sp(r, np, r.choice([1, 2, 3, 5]), valid=r.random() < 0.85)
    mode = r.choice(["all", "none", "alt", "alt1", "rand"]) if mode is None else mode
    nz = lambda: r.choice([1, 2, 0xFFFFFFFF, r.getrandbits(32) | 1])
    used = []
    for i in range(24):
        if mode == "all":
            used.append(nz())
        elif mode == "none":
            used.append(0)
        elif mode == "alt":
            used.append(nz() if i % 2 == 0 else 0)
        elif mode == "alt1":
            used.append(nz() if i % 2 == 1 else 0)
        else:
            used.append(nz() if r.random() < 0.5 else 0)
    c["used"] = used
    c["pts"], c["dst"] = pts_list(r), pts_list(r)
    c["same"] = r.random() < 0.5 if same is None else same
    return c


def repack_edges():
    r = random.Random(1488)
    for np in (0, 1, 2, 5, 21):
        for mode in ("all", "none", "alt", "alt1", "rand"):
            for same in (True, False):
                for flag in (0, 1):
                    c = repack_case(r, np, mode, same)
                    c["rec"]["edges"][0] = (flag,) + c["rec"]["edges"][0][1:]
                    yield c
    c = repack_case(r, 5, "alt")                                    # an edge pointing past the points (a used word the pass did not write)
    c["rec"]["edges"][0] = (0, 0, 20, 23, 0)
    yield c


def repack_random(r):
    return repack_case(r)


def optimise_case(r, count=None):
    c = base_case("optimise", r)
    count = r.choice([0, 1, 2, 3, 4]) if count is None else count
    npool = r.randint(1, 3)
    c["pool"] = [pts_list(r) for _ in range(npool)]
    c["q"] = {}
    recs = []
    for i in range(count):
        np = r.choice([0, 1, 2, 3, 4, 6, 9, 21])
        ne = r.choice([1, 2, 3, 4, 6])
        keep = [j for j in range(np) if r.random() < r.choice([0.3, 0.7, 1.0])]
        edges = []
        for _ in range(ne):
            pick = (lambda: r.choice(keep) if keep else 0) if r.random() < 0.9 else (lambda: r.randrange(np) if np else 0)
            edges.append((r.choice([0, 1, 2, r.getrandbits(32)]), r.getrandbits(16), pick(), pick(), r.getrandbits(16)))
        recs.append({"np": np, "edges": edges, "pool": r.randrange(npool), "poolb": r.randrange(npool) if r.random() < 0.25 else None})
    c["recs"] = recs
    return c


def optimise_edges():
    r = random.Random(1489)
    for count in (0, 1, 2, 3, 4):
        for share in (False, True):
            for poolb in (False, True):
                c = optimise_case(r, count)
                for i, sp in enumerate(c["recs"]):
                    sp["pool"] = 0 if share else i % len(c["pool"])
                    sp["poolb"] = 0 if poolb else None
                yield c
    for np in (1, 2, 5, 21):                                       # all points used, one used, none used by any edge
        for used in ("all", "one", "none"):
            c = optimise_case(r, 1)
            sp = c["recs"][0]
            sp["np"], sp["pool"], sp["poolb"] = np, 0, None
            if used == "all":
                sp["edges"] = [(0, 0, j % np, (j + 1) % np, 0) for j in range(max(np, 2))]
            elif used == "one":
                sp["edges"] = [(0, 0, 0, 0, 0)]
            yield c
    for flag in (0, 1):                                            # a repacked section whose edges are already flagged
        c = optimise_case(r, 2)
        for sp in c["recs"]:
            sp["np"], sp["pool"], sp["poolb"] = 6, 0, None
            sp["edges"] = [(flag, 0, 1, 3, 0), (flag, 0, 3, 5, 0)]
        yield c
    c = optimise_case(r, 2)
    c["pool"] = (c["pool"] * 2)[:max(2, len(c["pool"]))]
    for k, sp in enumerate(c["recs"]):
        sp["np"], sp["pool"], sp["poolb"] = 8, k, None
        sp["edges"] = [(0, 0, 2, 5, 0), (0, 0, 5, 7, 0)]
    yield c


def optimise_random(r):
    return optimise_case(r)


def params_case(r):
    c = base_case("params", r)
    c["calls"] = [tuple(r.choice([0, 1, -1, r.randrange(-100, 100), r.getrandbits(32) - 2 ** 31]) for _ in range(4))
                  for _ in range(r.choice([1, 2, 3]))]
    return c


def params_edges():
    r = random.Random(1490)
    for n in (1, 2, 3):
        for vals in ((0, 0, 0, 0), (1, 2, 3, 4), (-1, -2, -3, -4), (0x7FFFFFFF, -0x80000000, 5, 6)):
            c = params_case(r)
            c["calls"] = [vals] + [tuple(v + k for v in vals) for k in range(1, n)]
            yield c


def params_random(r):
    return params_case(r)


TESTS = [
    FnTest("CreateTrackSectionMesh", ppc, host, edge=mesh_edges, rnd=mesh_case),
    FnTest("SetupTrainType", ppc, host, edge=train_edges, rnd=train_random),
    FnTest("SetupCarType", ppc, host, edge=car_edges, rnd=car_random),
    FnTest("LoadCoasterPylons", ppc, host, edge=pylons_edges, rnd=pylons_random),
    FnTest("SetupPylonData", ppc, host, edge=pylon_edges, rnd=pylon_random),
    FnTest("CreateCrossSectionSelectsFromSamFile", ppc, host, edge=selects_edges, rnd=selects_random),
    FnTest("SetCrossSectionSelectDefaults", ppc, host, edge=defaults_edges, rnd=defaults_random),
    FnTest("OptimiseCrossSections", ppc, host, edge=optimise_edges, rnd=optimise_random),
    FnTest("RepackCrossSection", ppc, host, edge=repack_edges, rnd=repack_random),
    FnTest("GetTempFixedRideParams", ppc, host, edge=params_edges, rnd=params_random),
]
