"""Unit w213: the coaster track's mesh, against libtpw_unit_TrackExtrude.dylib (tests/unit_TrackExtrude.cpp), main app ("SimTheme Park"),
src/TPConfig_extrude.cpp:
  ExtrudeTrackSection 0x1004f628, ExtrudeCrossSectionVertices 0x1004ed8c, GetNewTrackMeshSet 0x1004ea30,
  SkinCrossSectionFaces 0x1004f410, LinkFEStoreToFETextureCont 0x1004e790, CopyExtrudedFaceData 0x1004e6a0,
  InterpolateCoasterSpline 0x1004e318 (the sTrackSample overload)

A case is a world and one call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's layout
(big-endian, 4-byte pointers) for the original, and in the host library's arena (little-endian, 8-byte pointers, Ptr32 words as arena
offsets) for the translation; the same code reads both back. Blocks are patterned (0x5a) and followed by 512 guard bytes of 0xa5.

The world: the sine table and its mask and scale, the type (flags, mesh entry, master set, texture count, the select cross section),
three pool sets (vertex sets, points, uv sets, faces, mesh, header, node, face sets, textures), the face stores A (256 entries) and B
(one container per texture, fresh or with linked face lists), three cross sections (rows, edges, an optional second row), the params
table (and the static pair), three braces with splines and Bezier controls, the track samples, the current coaster and the instance
whose +0x3c block holds the data (the same instance or another), and the 23 globals. The untranslated callees are fakes on both sides
that log their arguments (pointers as labels in build order, floats as bits): BezierPosition, BezierTangent, GetAlignmentParamsQuick and
GetBankParams write words from queues, SetMeshPosition, DMEnableMeshRender (answers 0) and InstanceDynamicMeshSet (hands out the prepared
sets in turn, answers 1); modf is a glue fake.

Compared: the answer (LinkFE's entry as a block and index), the call log, every global (floats as bits, a NaN as one value, pointers as
block and index), the fields of both instances, and every block the world holds (typed fields: floats as bits, halfwords, Ptr32 words
and native pointers as block and index; the header's and node's other bytes raw) with the guard bytes.

Fused operations are the unit's own business (ppc_fmadds and friends); the floats are compared bit for bit. Not tested: a ring of more
than 50 points with a second row (the Mac's local array overflows), indices that leave a block (the Mac reads or writes its neighbour),
a spline count below 2 with one brace, signalling NaNs, a non-arena Ptr32 (the host stops), a float converted to an integer that is above
2^31 (a sine index of an infinite angle, a box limit of the largest float: the translation saturates as the hardware does, the emulator
gives another word, so the worlds hold no infinity above zero and no largest float).
"""
import bisect
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_TrackExtrude"
MASK = 0xFFFFFFFF
GUARD = 512
GUARD_BYTES = b"\xa5" * GUARD
PAT = 0x5A5A5A5A
PAT8 = 0x5A5A5A5A5A5A5A5A

FN_ETS, FN_ECSV, FN_GNTMS, FN_SKIN, FN_LINK, FN_COPY, FN_ICS = (0x1004F628, 0x1004ED8C, 0x1004EA30, 0x1004F410, 0x1004E790, 0x1004E6A0,
                                                                   0x1004E318)
SIZES = {FN_ETS: 1816, FN_ECSV: 1392, FN_GNTMS: 816, FN_SKIN: 472, FN_LINK: 228, FN_COPY: 196, FN_ICS: 556}
OWN = set(SIZES)
ENTRY = {"ets": FN_ETS, "ecsv": FN_ECSV, "gntms": FN_GNTMS, "skin": FN_SKIN, "link": FN_LINK, "copy": FN_COPY, "ics": FN_ICS}

BPOS, BTAN, ALIGN, BANK, SETPOS, INSTANCE, ENABLE = 0x1004DA98, 0x1004DC00, 0x1004DE98, 0x1004E04C, 0x10065588, 0x10062BA8, 0x1003088C

# The globals in the order of tpw_tex_addr, with the Mac's addresses.
GL = ["gTPGlobals", "gCoasterCurrent", "gCoasterCurrentData", "StoreA", "StoreB", "ParamsStatic", "ParamsTable", "TextureKind", "FaceNext",
      "KindFlag", "Winding", "TOffset", "Max", "Min", "FaceSetCount", "FacesUsed", "PointsUsed", "UVsUsed", "VertsUsed", "FacesLeft",
      "PointsLeft", "UVsLeft", "VertsLeft"]
MACG = dict(zip(GL, [0x103B74C8, 0x102B68A8, 0x102B68A4, 0x102B74B0, 0x102B74B4, 0x102B7650, 0x102B7670, 0x102B74A8, 0x102B74AC,
                     0x102C1674, 0x102C1678, 0x102C167C, 0x102C1680, 0x102C168C, 0x102C1698, 0x102C169C, 0x102C16A0, 0x102C16A4,
                     0x102C16A8, 0x102C16AC, 0x102C16B0, 0x102C16B4, 0x102C16B8]))
INT_GLOBALS = ("TextureKind", "KindFlag", "Winding", "FaceSetCount", "FacesUsed", "PointsUsed", "UVsUsed", "VertsUsed", "FacesLeft",
               "PointsLeft", "UVsLeft", "VertsLeft")

# The offsets and sizes the world needs (tpw_tex_layout, in this order): the Mac's, and the host's (checked in bind).
NAMES = ["inst.size", "inst.type", "inst.f38", "inst.b3c", "inst.f70", "inst.f74", "inst.f78", "inst.samples",
         "type.size", "type.flags", "type.f14", "type.mesh", "type.tex", "type.texcount", "type.sela",
         "set.size", "set.vsets", "set.points", "set.uvs", "set.faces", "set.mesh", "set.f38",
         "mesh.size", "mesh.header",
         "rec.size", "rec.f00", "rec.cnt", "rec.points", "rec.block", "rec.pointsB", "rec.f14",
         "par.size", "par.f00", "par.f04", "par.f08", "par.sec",
         "spl.size", "spl.count", "spl.a", "spl.b",
         "ctl.size", "ctl.f08", "ctl.f0c", "ctl.brace", "ctl.f40",
         "br.size", "br.f00", "br.flags", "br.f6c", "br.f90", "br.fb0", "br.fe4", "br.data",
         "smp.size", "smp.bank", "dtex.size", "dtex.count", "tp.mask", "tp.scale", "tp.table"]
MAC_VALUES = [0x100, 4, 0x38, 0x3C, 0x70, 0x74, 0x78, 0x9C,
              0x35C, 0, 0x14, 0x18, 0x1C, 0x20, 0x80,
              0x40, 0x10, 0x14, 0x18, 0x1C, 0x24, 0x38,
              0x40, 4,
              0x18, 0, 4, 8, 0xC, 0x10, 0x14,
              0x10, 0, 4, 8, 0xC,
              0xC, 0, 4, 8,
              0x58, 8, 0xC, 0x10, 0x40,
              0x120, 0, 8, 0x6C, 0x90, 0xB0, 0xE4, 0x100,
              0x38, 0x18, 0x20, 4, 4, 8, 0xC]
MAC = dict(zip(NAMES, MAC_VALUES), ptr=4)
HOST_LAYOUT = [416, 8, 80, 88, 148, 152, 156, 192, 944, 0, 16, 24, 32, 40, 928, 112, 24, 32, 40, 48, 64, 96, 112, 8, 40, 0, 4, 8, 16, 24,
               32, 24, 0, 4, 8, 16, 24, 0, 8, 16, 88, 8, 12, 16, 64, 288, 0, 8, 280, 272, 184, 236, 264, 72, 24, 88, 0, 4, 8, 12]

NS = 8                                   # track samples in the world
SA_COUNT = 256                           # entries of the face store A
POOL_VSETS, POOL_POINTS, POOL_UVSETS, POOL_FACES, POOL_FACESETS = 48, 192, 48, 192, 24
INF_BITS = 0x7F800000

# The special words of the worlds. No infinity and no largest float: a sine index or a box limit that overflows upwards is converted by
# the translation as the hardware does (to 0x7fffffff) and by the emulator to another value, so the worlds keep such values out.
SPECIAL = [0, 0x80000000, 0x7FC00000, 0x00000001, 0x80000001, 0x3F800000, 0xBF800000, 0x00800000, 0x007FFFFF, 0x80800000]
SCALE_SPECIAL = [0, 0x80000000, 0x7FC00000, 0x00000001, 0x3F800000]


def fbf(x):
    """The single-precision bits of a Python number (an infinity for an overflow)."""
    try:
        return struct.unpack(">I", struct.pack(">f", x))[0]
    except OverflowError:
        return 0x7F800000 if x > 0 else 0xFF800000


def bf(bits):
    """The Python float of single-precision bits."""
    return struct.unpack(">f", struct.pack(">I", bits & MASK))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


SINE = [fbf(math.sin(2.0 * math.pi * i / 4096.0)) for i in range(4096)]
SINE_BYTES = {">": struct.pack(">4096I", *SINE), "<": struct.pack("<4096I", *SINE)}
SCALE_BITS = fbf(4096.0 / (2.0 * math.pi))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STATE = {"prepared": [0, 0, 0], "next": 0}


def stub_bpos(e):
    """BezierPosition(a, b, t, point): the label of a, b, point, t's bits; writes x, y, z from the queue."""
    a, b, point = e.r(3), e.r(4), e.r(6)
    e.cl.rec("BezierPosition", (e.cl.lab(a), e.cl.lab(b), fbf(e.f(1)), e.cl.lab(point)))
    for k in range(3):
        e.write(point + 4 * k, struct.pack(">I", e.cl.ret("bpos", 0) & MASK))


def stub_btan(e):
    """BezierTangent(a, b, t, tangent)."""
    a, b, tangent = e.r(3), e.r(4), e.r(6)
    e.cl.rec("BezierTangent", (e.cl.lab(a), e.cl.lab(b), fbf(e.f(1)), e.cl.lab(tangent)))
    for k in range(3):
        e.write(tangent + 4 * k, struct.pack(">I", e.cl.ret("btan", 0) & MASK))


def stub_align(e):
    """GetAlignmentParamsQuick(tangent by value, twist, orient, out): writes the orientation's seven floats from the queue."""
    words = (e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK)
    orient, out = e.r(7), e.r(8)
    e.cl.rec("GetAlignmentParamsQuick", words + (fbf(e.f(1)), e.cl.lab(orient), e.cl.lab(out)))
    for k in range(7):
        e.write(orient + 4 * k, struct.pack(">I", e.cl.ret("align", 0) & MASK))


def stub_bank(e):
    """GetBankParams(point, c, d, t, orient): writes the orientation's +0xc from the queue."""
    point, c, d, orient = e.r(3), e.r(4), e.r(5), e.r(7)
    e.cl.rec("GetBankParams", (e.cl.lab(point), e.cl.lab(c), e.cl.lab(d), fbf(e.f(1)), e.cl.lab(orient)))
    e.write(orient + 0xC, struct.pack(">I", e.cl.ret("bank", 0) & MASK))


def stub_setpos(e):
    """SetMeshPosition(mesh, x, y, z)."""
    e.cl.rec("SetMeshPosition", (e.cl.lab(e.r(3)), fbf(e.f(1)), fbf(e.f(2)), fbf(e.f(3))))


def stub_instance(e):
    """InstanceDynamicMeshSet(set, heap, flags, output): hands out the prepared sets in turn (the last one again), answers 1."""
    mesh_set, heap, flags, output = e.r(3), e.r(4), e.r(5), e.r(6)
    e.cl.rec("InstanceDynamicMeshSet", (e.cl.lab(mesh_set), e.cl.lab(heap), flags & MASK, e.cl.lab(output)))
    k = min(STATE["next"], 2)
    STATE["next"] += 1
    e.write(output, struct.pack(">I", STATE["prepared"][k]))
    e.set_r(3, 1)


def stub_enable(e):
    """DMEnableMeshRender(set): answers 0."""
    e.cl.rec("DMEnableMeshRender", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


def modf(e):
    """double modf(double x, double* whole): C semantics."""
    fraction, whole = math.modf(e.f(1))
    e.write(e.r(5), struct.pack(">d", whole))
    e.set_f1(fraction)


for _addr, _fn in ((BPOS, stub_bpos), (BTAN, stub_btan), (ALIGN, stub_align), (BANK, stub_bank), (SETPOS, stub_setpos),
                   (INSTANCE, stub_instance), (ENABLE, stub_enable)):
    STUBS.internal(_addr, _fn)
STUBS.imp("modf", modf)

# Where the events carry floats (indices into the ints): canonicalised before the comparison.
FLOAT_AT = {"BezierPosition": (2,), "BezierTangent": (2,), "GetAlignmentParamsQuick": (0, 1, 2, 3), "GetBankParams": (3,),
            "SetMeshPosition": (1, 2, 3)}


def canon_log(log):
    out = []
    for name, ints, _texts in log:
        ints = [int(x) for x in ints]
        for k in FLOAT_AT.get(name, ()):
            ints[k] = canon_f(ints[k] & MASK)
        out.append((name, tuple(ints)))
    return out


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

# The fixed layouts (the same bytes on both sides): (offset, kind) with kind I word, H halfword, B byte, f float, P Ptr32, p native pointer.
HDR = ([(0x30, "I"), (0x38, "H"), (0x3A, "H"), (0x3C, "H"), (0x3E, "H"), (0x50, "P"), (0x78, "P")] + [(0x80 + 4 * i, "f") for i in range(6)] +
       [(0xA8 + i, "B") for i in range(4)])
NODE = [(0x58, "H"), (0x5A, "H"), (0x5C, "H"), (0x5E, "H"), (0x6C, "P")] + [(0x78 + 4 * i, "f") for i in range(6)]
FSET = [(0, "P"), (4, "H"), (6, "H"), (8, "H"), (0xA, "H"), (0xC, "B"), (0xD, "B"), (0xE, "B"), (0xF, "B")]
FENTRY = [(0, "H"), (2, "H"), (4, "H"), (6, "H"), (8, "P"), (0xC, "P")]
CONT = [(0, "I"), (4, "I"), (8, "P"), (0xC, "P"), (0x10, "P"), (0x14, "I"), (0x18, "I")]
EDGE = [(0, "I"), (4, "H"), (6, "H"), (8, "H"), (0xA, "H")]
MENTRY = [(4, "H"), (6, "H"), (8, "H"), (0xA, "H"), (0xC, "H")]


def rep(fields, n, stride):
    return [(i * stride + off, k) for i in range(n) for off, k in fields]


class Side:
    """What the shared code needs from either memory, and the registry of the world's blocks."""

    def init_side(self):
        self.reg, self.labelled = [], []
        e = self.fmt[0]
        self.S = {"I": struct.Struct(e + "I"), "f": struct.Struct(e + "I"), "P": struct.Struct(e + "I"), "H": struct.Struct(e + "H"),
                  "B": struct.Struct("B"), "p": struct.Struct(e + ("I" if self.lay["ptr"] == 4 else "Q"))}
        self.nat_starts = self.nat = self.p32_starts = self.p32l = None

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def rd_ptr(self, a):
        return self.S["p"].unpack(self.read(a, self.S["p"].size))[0]

    def wptr(self, a, p):
        self.write(a, self.S["p"].pack(p))

    def w16(self, a, v):
        self.write(a, self.S["H"].pack(v & 0xFFFF))

    def w8(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def blk(self, name, size, unit=1, kind="g", fields=None, rest=False):
        """A patterned block of `size` bytes and 512 guard bytes, registered; kind g guard only, f float words, H halfwords, t typed."""
        a = self.alloc(size + GUARD)
        self.write(a, b"\x5a" * size + GUARD_BYTES)
        self.reg.append({"name": name, "addr": a, "p32": self.p32(a), "size": size, "unit": max(unit, 1), "kind": kind, "fields": fields,
                         "rest": rest})
        return a

    def lbl(self, a):
        """Gives an address the next label (the same order on both sides)."""
        if a not in self.labelled:
            self.labelled.append(a)
        return a

    def prep_dec(self):
        self.nat = sorted(self.reg, key=lambda b: b["addr"])
        self.nat_starts = [b["addr"] for b in self.nat]
        self.p32l = sorted(self.reg, key=lambda b: b["p32"])
        self.p32_starts = [b["p32"] for b in self.p32l]

    def dec(self, v, as32=False):
        """A pointer as (block, index, remainder), 0, 'pat' for the pattern or ('?', value)."""
        if v == 0:
            return 0
        if v in (PAT, PAT8):
            return "pat"
        starts, blocks, key = (self.p32_starts, self.p32l, "p32") if as32 else (self.nat_starts, self.nat, "addr")
        i = bisect.bisect_right(starts, v) - 1
        if i >= 0:
            b = blocks[i]
            d = v - b[key]
            if d < b["size"] + GUARD:
                return (b["name"], d // b["unit"], d % b["unit"])
        return ("?", v)

    def typed(self, data, fields):
        out = []
        for off, k in fields:
            v = self.S[k].unpack_from(data, off)[0]
            if k == "f":
                v = canon_f(v)
            elif k == "P":
                v = self.dec(v, True)
            elif k == "p":
                v = self.dec(v)
            out.append(v)
        return out

    def untyped(self, data, fields):
        b = bytearray(data)
        for off, k in fields:
            n = self.S[k].size
            b[off:off + n] = bytes(n)
        return bytes(b)

    def snap(self, b):
        a, size, kind = b["addr"], b["size"], b["kind"]
        out = {}
        if kind == "g":
            guard = self.read(a + size, GUARD)
        else:
            data = self.read(a, size + GUARD)
            body, guard = data[:size], data[size:]
            e = self.fmt[0]
            if kind == "f":
                out["v"] = [canon_f(w) for w in struct.unpack(e + "%dI" % (size // 4), body)]
            elif kind == "H":
                out["v"] = list(struct.unpack(e + "%dH" % (size // 2), body))
            else:
                out["v"] = self.typed(body, b["fields"])
                if b["rest"]:
                    out["rest"] = self.untyped(body, b["fields"])
        out["guard"] = None if guard == GUARD_BYTES else next((i, x) for i, x in enumerate(guard) if x != 0xA5)
        return out


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, memory in the emulator's image and scratch."""
    fmt = ">I"
    lay = MAC

    def __init__(self, e, case):
        self.e = e
        self.init_side()
        e.cl = CL.PpcLog({}, queues_of(case))
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n, alignment=8)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def p32(self, a):
        return a

    def g(self, name):
        return MACG[name]

    def begin(self, w, queues):
        self.e.cl = CL.PpcLog({a: i + 1 for i, a in enumerate(self.labelled)}, queues)
        STATE["prepared"], STATE["next"] = list(w.prepared), 0

    def log(self):
        return self.e.cl.log

    def call(self, w):
        args = [T.F(bf(a[1])) if isinstance(a, tuple) else a & MASK for a in w.args]
        r3, _, _ = T.call_ppc(self.e, ENTRY[w.op], args)
        return r3 & MASK if w.op == "link" else None


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lay = dict(zip(NAMES, HOST_LAYOUT), ptr=8)
        self.init_side()
        self.owned, self.gaddr = [], {}

    def done(self):
        for a in self.owned:
            self.lib.tpw_tex_free(a)

    def alloc(self, n):
        a = self.lib.tpw_tex_alloc(n)
        self.owned.append(a)
        return a

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def p32(self, a):
        return self.lib.tpw_tex_off(a)

    def g(self, name):
        if name not in self.gaddr:
            self.gaddr[name] = self.lib.tpw_tex_addr(GL.index(name))
        return self.gaddr[name]

    def begin(self, w, queues):
        CL.host_begin(self.lib, {i + 1: a for i, a in enumerate(self.labelled)}, queues)
        self.lib.tpw_tex_prepare(*w.prepared)

    def log(self):
        return list(CL.host_end(self.lib))

    def call(self, w):
        args = [a[1] if isinstance(a, tuple) else a for a in w.args]
        result = getattr(self.lib, "tpw_tex_" + w.op)(*args)
        return result if w.op == "link" else None


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    u64, u32, i32 = C.c_uint64, C.c_uint32, C.c_int32
    lib.tpw_tex_alloc.restype, lib.tpw_tex_alloc.argtypes = u64, [u32]
    lib.tpw_tex_free.argtypes = [u64]
    lib.tpw_tex_off.restype, lib.tpw_tex_off.argtypes = u32, [u64]
    lib.tpw_tex_prepare.argtypes = [u64] * 3
    lib.tpw_tex_addr.restype, lib.tpw_tex_addr.argtypes = u64, [i32]
    lib.tpw_tex_layout.restype, lib.tpw_tex_layout.argtypes = i32, [C.POINTER(C.c_int64)]
    lib.tpw_tex_ics.argtypes = [u64] * 6 + [u32] + [u64] * 2
    lib.tpw_tex_ecsv.argtypes = [u32] * 4 + [u64] * 3 + [i32] + [u64] * 2
    lib.tpw_tex_gntms.argtypes = [i32]
    lib.tpw_tex_skin.argtypes = [u64, i32, i32]
    lib.tpw_tex_link.restype, lib.tpw_tex_link.argtypes = u64, [u64]
    lib.tpw_tex_copy.argtypes = []
    lib.tpw_tex_ets.argtypes = [u64, u64, i32, u64, i32]
    out = (C.c_int64 * 80)()
    n = lib.tpw_tex_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def queues_of(case):
    """The words the fakes pop: floats of every scale and now and then a special value (no signalling NaN)."""
    r = random.Random(case["plan"]["qseed"])

    def word():
        k = r.random()
        if k < 0.08:
            return r.choice(SPECIAL)
        if k < 0.82:
            return fbf(r.uniform(-6.5, 6.5))
        return fbf(r.uniform(-300, 300))
    return {key: [word() for _ in range(90)] for key in ("bpos", "btan", "align", "bank")}


class World:
    """The objects of one case, built by the same code on both sides (offsets from the side's layout, contents from the plan)."""

    def __init__(self, s, case):
        self.s, self.op, self.p = s, case["op"], case["plan"]
        self.r = random.Random(self.p["wseed"])
        self.L = s.lay
        self.sec, self.br, self.bd, self.sp, self.ca, self.cb, self.pool = [], [], [], [], [], [], []
        self.ptab = self.smp = self.sa = self.sb = self.ty = self.master = self.entry = self.dtex = 0
        self.tpg_done = False
        self.fnext, self.fsc = 0, self.p["fsc"]
        self.pt = self.ori = self.pin = self.pmin = self.pmax = self.ts = 0
        self.prepared = [0, 0, 0]
        self.args = []

    def fl(self, lo, hi, special=0.0):
        r = self.r
        if special and r.random() < special:
            return r.choice(SPECIAL)
        return fbf(r.uniform(lo, hi))

    def u16(self):
        r = self.r
        return r.choice([r.randrange(24), r.getrandbits(16)])

    def build(self):
        op, p = self.op, self.p
        if op in ("ecsv", "ets"):
            self.tpg()
        if op in ("ecsv", "skin", "ets"):
            self.sections()
        if op in ("ecsv", "ets"):
            self.params()
        if op in ("ics", "ets"):
            self.braces()
            self.samples()
        self.pools({"gntms": 3, "ets": 3, "ics": 0}.get(op, 1))
        if op in ("gntms", "skin", "link", "copy", "ets"):
            self.stores()
        if op in ("gntms", "copy", "ets"):
            self.type_block()
        self.instances()
        self.scratch()
        self.globals_()
        self.make_call()

    def tpg(self):
        s, L, p = self.s, self.L, self.p
        g = s.g("gTPGlobals")
        s.words(g + L["tp.mask"], [p["mask"]])
        s.words(g + L["tp.scale"], [p["scale"]])
        s.write(g + L["tp.table"], SINE_BYTES[s.fmt[0]])

    def sections(self):
        s, L, p, r, N = self.s, self.L, self.p, self.r, self.p["N"]
        for k in range(3):
            npv, nev = p["np"][k], p["ne"][k]
            nrows = npv if 0 <= npv <= 64 else 0
            rows = s.blk("ra%d" % k, nrows * 20, 4, "f")
            s.words(rows, [self.fl(-3, 3, 0.015) for _ in range(nrows * 5)])
            rowsb = 0
            if p["blend"][k]:
                rowsb = s.blk("rb%d" % k, nrows * 20, 4, "f")
                s.words(rowsb, [self.fl(-3, 3, 0.015) for _ in range(nrows * 5)])
            edg = s.blk("eg%d" % k, nev * 12, 12, "t", rep(EDGE, nev, 12))
            for i in range(nev):
                s.words(edg + 12 * i, [r.getrandbits(32)])
                s.w16(edg + 12 * i + 4, r.randrange(N))
                s.w16(edg + 12 * i + 6, r.choice([r.randrange(16), r.getrandbits(16)]))
                s.w16(edg + 12 * i + 8, r.choice([r.randrange(16), r.getrandbits(16)]))
                s.w16(edg + 12 * i + 10, r.choice([0, r.getrandbits(16)]))
            rec = s.blk("sc%d" % k, L["rec.size"], 1, "t", [(L["rec.f00"], "I"), (L["rec.cnt"], "I"), (L["rec.points"], "p"),
                                                          (L["rec.block"], "p"), (L["rec.pointsB"], "p"), (L["rec.f14"], "I")])
            s.words(rec + L["rec.f00"], [npv])
            s.words(rec + L["rec.cnt"], [nev])
            s.wptr(rec + L["rec.points"], rows)
            s.wptr(rec + L["rec.block"], edg)
            s.wptr(rec + L["rec.pointsB"], rowsb)
            s.words(rec + L["rec.f14"], [p["mark"][k]])
            self.sec.append(rec)

    def params(self):
        s, L, p = self.s, self.L, self.p
        K, psz = p["K"], L["par.size"]
        fields = [(L["par.f00"], "f"), (L["par.f04"], "f"), (L["par.f08"], "f"), (L["par.sec"], "p")]
        self.ptab = s.blk("PT", (K + 1) * psz, psz, "t", rep(fields, K + 1, psz))
        for j in range(K + 1):
            base = self.ptab + j * psz
            s.words(base + L["par.f00"], [INF_BITS if j == K else p["th"][j]])
            s.words(base + L["par.f04"], [p["pf04"][j]])
            s.words(base + L["par.f08"], [p["pf08"][j]])
            s.wptr(base + L["par.sec"], self.sec[p["psec"][j]])

    def braces(self):
        s, L, p, r = self.s, self.L, self.p, self.r
        csz = L["ctl.size"]
        ctl_fields = [(L["ctl.f08"], "f"), (L["ctl.f0c"], "f"), (L["ctl.brace"], "p")] + [(L["ctl.f40"] + 4 * i, "f") for i in range(3)]
        br_fields = [(L["br.f00"], "I"), (L["br.flags"], "I"), (L["br.f6c"], "f"), (L["br.f90"], "p"), (L["br.fb0"], "I"),
                     (L["br.fe4"], "f"), (L["br.data"], "p")]
        spl_fields = [(L["spl.count"], "I"), (L["spl.a"], "p"), (L["spl.b"], "p")]
        for k in range(3):
            self.br.append(s.blk("br%d" % k, L["br.size"], 1, "t", br_fields))
            self.bd.append(s.blk("bd%d" % k, 8, 1, "t", [(0, "I"), (4, "I")]))
            self.sp.append(s.blk("sp%d" % k, L["spl.size"], 1, "t", spl_fields))
            n = p["cnt"][k] + 3
            self.ca.append(s.blk("ca%d" % k, n * csz, csz, "t", rep(ctl_fields, n, csz)))
            self.cb.append(s.blk("cb%d" % k, n * csz, csz, "t", rep(ctl_fields, n, csz)))
            for i in range(n):
                s.lbl(self.ca[k] + i * csz)
            for i in range(n):
                s.lbl(self.cb[k] + i * csz)
        for k in range(3):
            n = p["cnt"][k] + 3
            for arr, which in ((self.ca[k], 0), (self.cb[k], 1)):
                for i in range(n):
                    base = arr + i * csz
                    if which == 1 and p["sp"]:
                        f08 = self.fl(-1.0, NS - 0.01)
                    else:
                        f08 = self.fl(-4, 12, 0.02)
                    s.words(base + L["ctl.f08"], [f08])
                    s.words(base + L["ctl.f0c"], [self.fl(-1, 1, 0.03)])
                    s.wptr(base + L["ctl.brace"], self.br[r.randrange(3)])
                    s.words(base + L["ctl.f40"], [self.fl(-8, 8, 0.02) for _ in range(3)])
            b = self.br[k]
            s.words(b + L["br.f00"], [p["bn"][k]])
            s.words(b + L["br.flags"], [p["bf"][k]])
            s.words(b + L["br.f6c"], [p["b6c"][k]])
            s.wptr(b + L["br.f90"], self.sp[k])
            s.words(b + L["br.fb0"], [p["bfb"][k]])
            s.words(b + L["br.fe4"], [p["bfe"][k]])
            s.wptr(b + L["br.data"], self.bd[k])
            s.words(self.bd[k], [r.getrandbits(32), p["bdf"][k]])
            sp = self.sp[k]
            s.words(sp + L["spl.count"], [p["cnt"][k]])
            s.wptr(sp + L["spl.a"], self.ca[k])
            s.wptr(sp + L["spl.b"], self.cb[k])

    def samples(self):
        s, L = self.s, self.L
        self.smp = s.blk("smp", NS * L["smp.size"], L["smp.size"], "t", [(i * L["smp.size"] + L["smp.bank"], "f") for i in range(NS)])
        for i in range(NS):
            s.words(self.smp + i * L["smp.size"] + L["smp.bank"], [self.fl(-3, 3, 0.03)])

    def pools(self, n):
        s, L, r, N = self.s, self.L, self.r, self.p["N"]
        for k in range(n):
            vs = s.blk("vs%d" % k, POOL_VSETS * 0x30, 0x30, "f")
            pn = s.blk("pn%d" % k, POOL_POINTS * 0xC, 0xC, "f")
            uv = s.blk("uv%d" % k, POOL_UVSETS * 0x20, 0x20, "f")
            fc = s.blk("fc%d" % k, POOL_FACES * 8, 8, "H")
            fs = s.blk("fs%d" % k, POOL_FACESETS * 0x10, 0x10, "t", rep(FSET, POOL_FACESETS, 0x10))
            tx = s.blk("tx%d" % k, 8 * N, 8, "g")
            nd = s.blk("nd%d" % k, 0x100, 1, "t", NODE, True)
            hd = s.blk("hd%d" % k, 0x100, 1, "t", HDR, True)
            ms = s.blk("ms%d" % k, L["mesh.size"], 1, "t", [(L["mesh.header"], "p")])
            st = s.blk("st%d" % k, L["set.size"], 1, "t", [(L["set.vsets"], "p"), (L["set.points"], "p"), (L["set.uvs"], "p"),
                                                         (L["set.faces"], "p"), (L["set.mesh"], "p"), (L["set.f38"], "p")])
            s.lbl(st)
            s.lbl(ms)
            for field, target in (("set.vsets", vs), ("set.points", pn), ("set.uvs", uv), ("set.faces", fc), ("set.mesh", ms)):
                s.wptr(st + L[field], target)
            s.wptr(ms + L["mesh.header"], hd)
            s.words(hd + 0x30, [r.getrandbits(32)])
            for off in (0x38, 0x3A, 0x3C, 0x3E):
                s.w16(hd + off, self.u16())
            s.words(hd + 0x50, [s.p32(tx)])
            s.words(hd + 0x78, [s.p32(nd)])
            s.words(hd + 0x80, [self.fl(-100, 100, 0.05) for _ in range(6)])
            for i in range(4):
                s.w8(hd + 0xA8 + i, r.getrandbits(8))
            for off in (0x58, 0x5A, 0x5C, 0x5E):
                s.w16(nd + off, self.u16())
            s.words(nd + 0x6C, [s.p32(fs)])
            s.words(nd + 0x78, [self.fl(-100, 100, 0.05) for _ in range(6)])
            self.pool.append({"set": st, "mesh": ms, "hdr": hd, "node": nd, "fs": fs, "tx": tx})
        if n == 3:
            self.prepared = [self.pool[1]["set"], self.pool[2]["set"], self.pool[2]["set"]]

    def stores(self):
        s, p, r, N = self.s, self.p, self.r, self.p["N"]
        e = s.fmt[0]
        sa = s.blk("SA", SA_COUNT * 16, 16, "t", rep(FENTRY, SA_COUNT, 16))
        sb = s.blk("SB", N * 0x1C, 0x1C, "t", rep(CONT, N, 0x1C))
        sap = s.p32(sa)
        fsp = s.p32(self.pool[0]["fs"])
        ents = [[r.getrandbits(16), r.getrandbits(16), r.getrandbits(16), r.getrandbits(16), PAT, PAT] for _ in range(SA_COUNT)]
        lc = p["lc"] if p["linked"] else [0] * N
        total = sum(lc)
        slots = list(range(total))
        r.shuffle(slots)
        pos = nfs = 0
        conts = []
        for c in range(N):
            chain = slots[pos:pos + lc[c]]
            pos += lc[c]
            for q, idx in enumerate(chain):
                ents[idx][4] = sap + 16 * chain[q + 1] if q + 1 < len(chain) else 0
                ents[idx][5] = sap + 16 * chain[q - 1] if q > 0 else 0
            head = sap + 16 * chain[0] if chain else 0
            tail = sap + 16 * chain[-1] if chain else 0
            hasfs = p["linked"] and (bool(chain) or p["fsx"][c])
            faceset = fsp + 0x10 * nfs if hasfs else 0
            nfs += 1 if hasfs else 0
            count = lc[c] if p["cm"][c] < 0 else p["cm"][c]
            low = r.randrange(0, 300) if p["linked"] else r.choice([10000, r.randrange(0, 300)])
            high = r.randrange(0, 300) if p["linked"] else r.choice([0, r.randrange(0, 300)])
            conts.append([p["bfl"][c], count, head, tail, faceset, low, high])
        s.write(sa, b"".join(struct.pack(e + "4H2I", *x) for x in ents))
        s.write(sb, b"".join(struct.pack(e + "7I", *[v & MASK for v in x]) for x in conts))
        self.sa, self.sb = sa, sb
        self.fnext = max(p["fnext"], total)
        self.fsc = max(p["fsc"], nfs)

    def type_block(self):
        s, L, p = self.s, self.L, self.p
        self.dtex = s.blk("dtex", L["dtex.size"], 1, "t", [(L["dtex.count"], "H")])
        s.w16(self.dtex + L["dtex.count"], p["N"])
        self.entry = s.blk("ent", 0x20, 1, "t", MENTRY)
        for off, key in ((4, "ef"), (6, "ev"), (8, "eu"), (0xA, "efs"), (0xC, "ep")):
            s.w16(self.entry + off, p[key])
        self.master = s.lbl(s.blk("master", L["set.size"], 1, "g"))
        self.ty = s.blk("type", L["type.size"], 1, "t", [(L["type.flags"], "I"), (L["type.f14"], "p"), (L["type.mesh"], "p"),
                                                       (L["type.tex"], "p"), (L["type.texcount"], "I"), (L["type.sela"], "p")])
        s.words(self.ty + L["type.flags"], [p["tflags"]])
        s.wptr(self.ty + L["type.f14"], self.entry)
        s.wptr(self.ty + L["type.mesh"], self.master)
        s.wptr(self.ty + L["type.tex"], self.dtex)
        s.words(self.ty + L["type.texcount"], [p["ntex"]])
        s.wptr(self.ty + L["type.sela"], self.sec[0] if self.sec else 0)

    def instances(self):
        s, L, p, op = self.s, self.L, self.p, self.op
        fields = ([(L["inst.f38"], "p"), (L["inst.f70"], "f"), (L["inst.f74"], "f")] + [(L["inst.f78"] + 4 * i, "f") for i in range(7)] +
                  [(L["inst.b3c"], "I")])
        A = s.blk("A", L["inst.size"], 1, "t", fields)
        sep = p["sep"] and op in ("ics", "gntms", "ets")
        B = s.blk("B", L["inst.size"], 1, "t", fields) if sep else A
        s.words(B + L["inst.f70"], [p["b70"]])
        s.words(B + L["inst.f74"], [p["b74"]])
        s.words(B + L["inst.f78"], p["orient"])
        dword = (p["dw"] & ~8) | (8 if p["sp"] else 0)
        s.words(B + L["inst.b3c"], [dword])
        s.wptr(A + L["inst.type"], self.ty)
        pool0 = self.pool[0]["set"] if self.pool and not (p["anull"] and op in ("gntms", "ets")) else 0
        s.wptr(A + L["inst.f38"], pool0)
        s.wptr(A + L["inst.samples"], self.smp)
        s.words(A + L["inst.f74"], [p["a74"]])
        self.A, self.B, self.data = A, B, B + L["inst.b3c"]
        s.lbl(A + L["inst.f38"])
        s.lbl(B + L["inst.f78"])

    def scratch(self):
        s, p, op = self.s, self.p, self.op
        if op == "ics":
            self.pt = s.lbl(s.blk("pt", 12, 4, "f"))
            self.ori = s.lbl(s.blk("ori", 0x1C, 4, "f"))
        elif op == "ecsv":
            self.pin = s.blk("pin", 12, 4, "f")
            self.pmin = s.blk("pmin", 12, 4, "f")
            self.pmax = s.blk("pmax", 12, 4, "f")
            s.words(self.pin, [self.fl(-8, 8, 0.03) for _ in range(3)])
            s.words(self.pmin, [self.fl(-20, 20, 0.05) for _ in range(3)])
            s.words(self.pmax, [self.fl(-20, 20, 0.05) for _ in range(3)])
        elif op == "ets":
            self.ts = s.blk("ts", len(p["ts"]) * 4, 4, "f")
            s.words(self.ts, p["ts"])

    def globals_(self):
        s, L, p = self.s, self.L, self.p
        g = s.g
        psz = L["par.size"]
        s.wptr(g("gCoasterCurrent"), self.A)
        s.wptr(g("gCoasterCurrentData"), self.data)
        s.wptr(g("StoreA"), self.sa)
        s.wptr(g("StoreB"), self.sb)
        for i in range(2):
            base = g("ParamsStatic") + i * psz
            s.words(base + L["par.f00"], [INF_BITS])
            s.words(base + L["par.f04"], [p["sf04"][i]])
            s.words(base + L["par.f08"], [p["sf08"][i]])
            sec = self.sec[p["s0sec"] if i == 0 else p["s1sec"]] if self.sec else 0
            s.wptr(base + L["par.sec"], sec)
        s.wptr(g("ParamsTable"), self.ptab + psz if self.ptab else 0)
        s.wptr(g("FaceNext"), self.sa + 16 * self.fnext if self.sa else 0)
        values = {"TextureKind": p["kind"], "KindFlag": p["kflag"], "Winding": p["wind"], "FaceSetCount": self.fsc, "FacesUsed": p["fused"],
                  "PointsUsed": p["pused"], "UVsUsed": p["uvused"], "VertsUsed": p["vused"], "FacesLeft": p["fleft"],
                  "PointsLeft": p["pleft"], "UVsLeft": p["uvleft"], "VertsLeft": p["vleft"], "TOffset": p["toff"]}
        for name, v in values.items():
            s.words(g(name), [v])
        s.words(g("Min"), p["bmin"])
        s.words(g("Max"), p["bmax"])

    def make_call(self):
        s, L, p, op = self.s, self.L, self.p, self.op
        psz, csz = L["par.size"], L["ctl.size"]
        self.op = op
        if op == "ics":
            def ctl(arrays, ref):
                return arrays[ref[0]] + ref[1] * csz
            self.args = [self.smp, self.data, ctl(self.ca, p["ia"]), ctl(self.ca, p["ib"]), ctl(self.cb, p["ic"]), ctl(self.cb, p["id"]),
                         ("f", p["it"]), self.pt, 0 if p["inull"] else self.ori]
        elif op == "ecsv":
            tag, j = p["epar"]
            params = self.ptab + j * psz if tag == 0 else s.g("ParamsStatic") + j * psz
            self.args = [("f", p["et"]), ("f", p["ea2"]), ("f", p["ea3"]), ("f", p["ea4"]), self.pin, self.pmin, self.pmax, p["efirst"],
                         self.sec[p["esec"]], params]
        elif op == "gntms":
            self.args = [p["flag"]]
        elif op == "skin":
            self.args = [self.sec[p["ssec"]], p["sedges"], p["spts"]]
        elif op == "link":
            self.args = [self.sb + 0x1C * p["lcont"]]
        elif op == "copy":
            self.args = []
        else:
            self.args = [self.br[p["b1"]], self.br[p["b2"]], p["count"], self.ts, p["force"]]


def observe(s, answer):
    """Everything the call may change, read back the same way on both sides."""
    s.prep_dec()
    L = s.lay
    res = {"answer": s.dec(answer) if answer is not None else None, "log": canon_log(s.log())}
    glob = {name: s.rd_word(s.g(name)) for name in INT_GLOBALS}
    glob["TOffset"] = canon_f(s.rd_word(s.g("TOffset")))
    glob["Max"] = [canon_f(w) for w in s.rd_words(s.g("Max"), 3)]
    glob["Min"] = [canon_f(w) for w in s.rd_words(s.g("Min"), 3)]
    glob["FaceNext"] = s.dec(s.rd_ptr(s.g("FaceNext")))
    glob["ParamsTable"] = s.dec(s.rd_ptr(s.g("ParamsTable")))
    for i in range(2):
        base = s.g("ParamsStatic") + i * L["par.size"]
        glob["static%d" % i] = [canon_f(s.rd_word(base + L["par.f00"])), canon_f(s.rd_word(base + L["par.f04"])),
                                canon_f(s.rd_word(base + L["par.f08"])), s.dec(s.rd_ptr(base + L["par.sec"]))]
    res["globals"] = glob
    res["blocks"] = {b["name"]: s.snap(b) for b in s.reg}
    return res


def run(s, case):
    try:
        w = World(s, case)
        w.build()
        s.begin(w, queues_of(case))
        answer = s.call(w)
        return observe(s, answer)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rbits(r, lo, hi, special=0.05):
    """A float's bits: ordinary numbers of lo..hi, now and then a special value."""
    return r.choice(SPECIAL) if r.random() < special else fbf(r.uniform(lo, hi))


def world(op, seed, **ov):
    """A case: the plan of a world and a call (random where not given). The structural keys (N, cnt, np, ne, K, same, b1, b2, branch,
    count, selfree) are read first; every draw happens whether or not a key is overridden, so an override changes nothing else."""
    r = random.Random(seed)
    g = lambda key, dflt: ov.get(key, dflt)
    N = g("N", r.randint(3, 8) if op == "ets" else r.randint(1, 8))
    p = {"N": N, "wseed": g("wseed", r.getrandbits(48)), "qseed": g("qseed", r.getrandbits(48))}
    cnt = list(g("cnt", [r.randint(2, 6) for _ in range(3)]))
    p["cnt"] = cnt
    p["np"] = list(g("np", [r.choice([0, 1, 2, 3, 3, 4, 5, 6, 8]) for _ in range(3)]))
    p["ne"] = list(g("ne", [r.choice([0, 1, 2, 3, 4, 6, 8]) for _ in range(3)]))
    K = g("K", r.randint(1, 4))
    p["K"] = K
    same = g("same", r.random() < 0.5)
    b1 = g("b1", r.randint(0, 2))
    b2 = b1 if same else g("b2", (b1 + r.randint(1, 2)) % 3)
    branch = g("branch", r.choice("AAABBBBCCC"))
    selfree = g("selfree", branch == "A" and r.random() < 0.15)
    p["b1"], p["b2"] = b1, b2
    # the counters
    vused = g("vused", r.randint(0, 30))
    vu = min(vused, 30)
    p["vused"] = vused
    p["uvused"] = g("uvused", r.randint(0, 30))
    p["fused"] = g("fused", r.randint(0, 60))
    p["pused"] = g("pused", r.randint(0, 30))
    p["fsc"] = g("fsc", r.randint(0, 10))

    def room(hi):
        return r.choice([r.randint(-3, 14), r.randint(15, hi), r.randint(15, hi)])
    p["vleft"] = g("vleft", room(176 - vu))
    p["fleft"] = g("fleft", room(110))
    p["uvleft"] = g("uvleft", r.randint(-5, 170))
    p["pleft"] = g("pleft", r.randint(-5, 170))
    for key in ("ev", "eu", "ep", "ef", "efs"):
        p[key] = g(key, r.choice([r.randint(0, 20), r.randint(20, 170), r.randint(60, 170)]))
    p["kind"] = g("kind", r.getrandbits(32) if selfree else (r.getrandbits(16) << 16) | r.randint(1, 3))
    p["kflag"] = g("kflag", r.choice([0, 0, 1, 1, 5]))
    p["wind"] = g("wind", r.choice([0, 0, 1, 1, 7]))
    toff_f = r.uniform(-2, 2)
    p["toff"] = g("toff", fbf(toff_f) if same else rbits(r, -3, 3, 0.08))
    p["bmin"] = g("bmin", [rbits(r, -50, 50, 0.04) for _ in range(3)])
    p["bmax"] = g("bmax", [rbits(r, -50, 50, 0.04) for _ in range(3)])
    # the face stores
    p["fnext"] = g("fnext", r.randint(0, 60))
    p["linked"] = g("linked", r.random() < 0.5)
    p["lc"] = list(g("lc", [r.choice([0, 0, 1, 2, 3, 5]) for _ in range(N)]))
    p["fsx"] = list(g("fsx", [r.random() < 0.2 for _ in range(N)]))
    p["bfl"] = list(g("bfl", [r.getrandbits(32) for _ in range(N)]))
    p["cm"] = list(g("cm", [-1 if r.random() < 0.8 else r.choice([0, 1, 7, 1000, 0xFFFFFFFF]) for _ in range(N)]))
    # the cross sections
    p["blend"] = list(g("blend", [False, False, r.random() < 0.6]))
    p["mark"] = list(g("mark", [r.choice([0x3F800000, 0xBF800000, 0, 0x80000000, 0x7FC00000, fbf(r.uniform(-2, 2))]) for _ in range(3)]))
    # the params
    span = (toff_f, toff_f + 5) if same else (-3, 8)
    p["th"] = list(g("th", [fbf(v) for v in sorted(r.uniform(*span) for _ in range(K + 1))]))
    p["pf04"] = list(g("pf04", [fbf(v) for v in sorted(r.uniform(-1, 6) for _ in range(K + 1))]))
    p["pf08"] = list(g("pf08", [rbits(r, -0.5, 1.5, 0.04) for _ in range(K + 1)]))
    low = 1 if selfree else 0
    p["psec"] = list(g("psec", [r.randint(low, 2) for _ in range(K + 1)]))
    p["s0sec"] = g("s0sec", 1 if selfree else r.randint(0, 1))
    p["s1sec"] = g("s1sec", r.randint(1, 2) if selfree else r.choice([0, 0, 1, 2]))
    p["sf04"] = list(g("sf04", [rbits(r, -1, 6, 0.04) for _ in range(2)]))
    p["sf08"] = list(g("sf08", [rbits(r, -0.5, 1.5, 0.04) for _ in range(2)]))
    # the braces and the branch
    flags = [r.getrandbits(32) for _ in range(3)]
    fb0 = [0, 0, 0]
    tflags = r.getrandbits(32)
    trig = [r.random() < 0.5 for _ in range(3)]
    if not any(trig):
        trig[r.randint(0, 2)] = True
    fb0_value = r.getrandbits(32) | 1
    b2_bits = r.choice([0x800000, 0x2000, 0x802000])
    drop800 = r.random() < 0.5
    if branch == "A":
        fb0[b1] = fb0_value if trig[0] else 0
        flags[b1] = (flags[b1] | 0x1000000) if trig[1] else (flags[b1] & ~0x1000000)
        flags[b2] = (flags[b2] | b2_bits) if trig[2] else (flags[b2] & ~0x802000)
    elif branch == "C":
        flags[b1] = (flags[b1] | 0x800) & ~0x1000000
        flags[b2] &= ~0x802000
        tflags |= 0x40
    else:
        flags[b1] &= ~0x1000000
        flags[b2] &= ~0x802000
        if drop800:
            flags[b1] &= ~0x800
        else:
            tflags &= ~0x40
    p["tflags"] = g("tflags", tflags & MASK)
    p["bf"] = list(g("bf", [f & MASK for f in flags]))
    p["bfb"] = list(g("bfb", fb0))
    early = g("early", same and r.random() < 0.12)
    bdf = [r.getrandbits(32) & ~1 for _ in range(3)]
    if early:
        bdf[b1] |= 1
    p["bdf"] = list(g("bdf", bdf))
    bn = [r.choice([0, 0, 1, 2, 5, 17, 0xFFFFFFFF, 0x80000000, 0x01000001]) for _ in range(3)]
    if r.random() < 0.4:
        bn[b1] = 0
    elif bn[b1] == 0:
        bn[b1] = 1
    p["bn"] = list(g("bn", bn))
    p["b6c"] = list(g("b6c", [rbits(r, -3, 3, 0.05) for _ in range(3)]))
    p["bfe"] = list(g("bfe", [rbits(r, -2, 2, 0.05) for _ in range(3)]))
    p["ntex"] = g("ntex", r.randint(0, N) if selfree else r.randint(0, max(0, N - 3)))
    # the instances
    p["sp"] = g("sp", r.random() < 0.5)
    p["dw"] = g("dw", r.getrandbits(32))
    p["a74"] = g("a74", rbits(r, 0, 20, 0.04))
    p["b70"] = g("b70", rbits(r, 0, 10, 0.04))
    p["b74"] = g("b74", rbits(r, 0, 20, 0.04))
    p["orient"] = list(g("orient", [rbits(r, -4, 4, 0.04) for _ in range(7)]))
    p["sep"] = g("sep", r.random() < 0.7)
    p["mask"] = g("mask", 0xFFF if r.random() < 0.85 else r.choice([0xFF, 0x3FF, 0x7FF, 0x1FF, 0]))
    p["scale"] = g("scale", SCALE_BITS if r.random() < 0.85 else (r.choice(SCALE_SPECIAL) if r.random() < 0.3 else fbf(r.uniform(100, 1000))))
    # ExtrudeTrackSection's call
    p["force"] = g("force", r.choice([0, 0, 1, 7]))
    first_ring = p["bn"][b1] == 0 or p["force"] != 0
    p["anull"] = g("anull", first_ring and r.random() < 0.4)
    count = g("count", r.choice([0, 1, 2, 3, 3, 4, 5, 6, 8, -1]))
    p["count"] = count
    ts = [rbits(r, -5, 5, 0.1)]
    tf = bf(fbf(toff_f))
    for _ in range(1, max(count, 0) + 2):
        if same:
            c = cnt[b1]
            v = r.choice([r.uniform(0, c - 0.05), c - 1.0, r.randint(0, c - 1) + 0.5, r.uniform(-0.9, 0.0), 0.0])
            ts.append(fbf(tf + v))
        else:
            ts.append(rbits(r, -5, 12, 0.08))
    p["ts"] = list(g("ts", ts))
    # InterpolateCoasterSpline's call
    def ref(array_cnt):
        k = r.randint(0, 2)
        return [k, r.randint(0, array_cnt[k] + 2)]
    p["ia"], p["ib"], p["ic"], p["id"] = (list(g(key, ref(cnt))) for key in ("ia", "ib", "ic", "id"))
    p["it"] = g("it", fbf(r.uniform(0, 1)) if r.random() < 0.8 else rbits(r, -2, 3, 0.5))
    p["inull"] = g("inull", r.random() < 0.15)
    # ExtrudeCrossSectionVertices' call
    p["et"] = g("et", rbits(r, -1, 4, 0.04))
    p["ea2"], p["ea3"], p["ea4"] = (g(key, rbits(r, -7, 7, 0.05)) for key in ("ea2", "ea3", "ea4"))
    p["efirst"] = g("efirst", r.choice([0, 1, 1, 5]))
    p["esec"] = g("esec", r.randint(0, 2))
    tag = r.randint(0, 1)
    epar = [0, r.randint(1, K)] if tag == 0 else [1, r.randint(0, 1)]
    if p["blend"][p["esec"]] and epar == [1, 0]:
        epar = [1, 1]
    p["epar"] = list(g("epar", epar))
    # the other calls
    p["flag"] = g("flag", r.choice([0, 0, 1, 2, -1]))
    ssec = g("ssec", r.randint(0, 2))
    p["ssec"] = ssec
    p["sedges"] = g("sedges", r.randint(-1, p["ne"][ssec]))
    p["spts"] = g("spts", r.choice([0, 1, r.randint(1, 50), -1, 0x7FFFFFFF]))
    p["lcont"] = g("lcont", r.randrange(N))
    return {"op": op, "seed": seed, "plan": p}


def edges_ics():
    seed = 3000
    n = 0
    tbits = [fbf(x) for x in (0.0, 1.0, 0.5, 0.25, 0.9999, -0.5, 1.5, -0.0, 1e-30, 3e30)] + [0x7FC00000, 0x7F800000, 0xFF800000]
    for sp in (False, True):
        for t in tbits:
            for inull in (False, True):
                n += 1
                yield world("ics", seed + n, sp=sp, it=t, inull=inull)
    for sp in (False, True):
        for number in (0, 1, 2, 17, 0x01000001, 0x80000000, 0xFFFFFFFF):
            for t in (0.3, 0.75):
                n += 1
                yield world("ics", seed + n, sp=sp, bn=[number, 3, number], it=fbf(t), inull=False)
    for sp in (False, True):
        for bits in (0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x3F800000):
            for t in (0.3, 0.75):
                n += 1
                yield world("ics", seed + n, sp=sp, bfe=[bits] * 3, it=fbf(t), inull=False)
    for sp in (False, True):
        for bits in (0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000, 0x40490FDB):
            n += 1
            yield world("ics", seed + n, sp=sp, b6c=[bits] * 3, it=fbf(0.4), inull=False)


def edges_ecsv():
    seed = 4000
    n = 0
    marks = [0x3F800000, 0xBF800000, 0, 0x80000000, 0x7FC00000]
    for npv in (0, 1, 2, 3, 4, 5, 8, 50, 51):
        for first in (0, 1):
            for blend in (False, True) if npv < 51 else (False,):
                for mark in (marks if blend else [0x3F800000]):
                    n += 1
                    yield world("ecsv", seed + n, np=[npv] * 3, esec=2 if blend else 1, blend=[False, False, blend], mark=[mark] * 3,
                                efirst=first)
    for huge in (0xFFFFFFFF, 0x80000000, 0xFFFFFFF0):
        for first in (0, 1):
            n += 1
            yield world("ecsv", seed + n, np=[huge] * 3, esec=1, blend=[False] * 3, efirst=first)
    for bits in (0x7FC00000, 0x7F800000, 0xFF800000, 0, 0x80000000):
        for key in ("pf04", "pf08", "et"):
            n += 1
            extra = {key: [bits] * 3} if key != "et" else {"et": bits}
            yield world("ecsv", seed + n, K=2, np=[4, 4, 4], esec=2, blend=[False, False, True], epar=[0, 1], **extra)
    for a in (0x7FC00000, 0xFF800000, 0x80000000, 0x49800000, 0xC9800000, 0x00000001):
        n += 1
        yield world("ecsv", seed + n, np=[3, 3, 3], esec=1, ea2=a, ea3=a, ea4=a)


def edges_gntms():
    seed = 5000
    n = 0
    for flag in (0, 1, 2, -1):
        for anull in (False, True):
            for linked in (False, True):
                for N in (1, 3, 8):
                    for ctr in ((0, 0, 0), (30, 30, 60), (7, 3, 11)):
                        n += 1
                        yield world("gntms", seed + n, flag=flag, anull=anull, linked=linked, N=N, vused=ctr[0], uvused=ctr[1],
                                    fused=ctr[2])


def edges_skin():
    seed = 6000
    n = 0
    vused = [0, 7, 30, 0xFFFFFFF0, 0x80000000, 0xFFFFFFFF]
    for wind in (0, 1, 5):
        for edges in (-3, -1, 0, 1, 2, 3, 8):
            for pts in (0, 1, 5, -1, 0x7FFFFFFF):
                n += 1
                yield world("skin", seed + n, wind=wind, sedges=edges, spts=pts, ne=[8, 8, 8], vused=vused[n % len(vused)])


def edges_link():
    seed = 7000
    n = 0
    for linked in (False, True):
        for first in (0, 1, 3):
            for fsx in (False, True):
                for flag in (0, 1):
                    for fsc in (0, 5, 12):
                        for N in (1, 4):
                            n += 1
                            yield world("link", seed + n, linked=linked, lc=[first] + [0] * (N - 1), fsx=[fsx] * N, bfl=[flag] * N,
                                        fsc=fsc, N=N, lcont=0)


def edges_copy():
    seed = 8000
    n = 0
    variants = [dict(linked=False), dict(linked=True, fsx=False), dict(linked=True, fsx=True), dict(linked=True, cmx=True),
                dict(linked=True, cmx=True, fsx=True)]
    for N in range(1, 9):
        for v in variants:
            for shape in (0, 1, 2):
                n += 1
                lc = [0] * N if shape == 0 else [1] * N if shape == 1 else [(i * 2 + 1) % 4 for i in range(N)]
                cm = [(i * 7 + 3) % 5 for i in range(N)] if v.get("cmx") else [-1] * N
                yield world("copy", seed + n, N=N, lc=lc, cm=cm, linked=v["linked"], fsx=[v.get("fsx", False)] * N)


def edges_ets():
    seed = 9000
    n = 0
    for count in (0, 1, 2, -1):
        for branch in "ABC":
            n += 1
            yield world("ets", seed + n, count=count, branch=branch, force=1)
    for same in (True, False):
        for branch in "ABC":
            n += 1
            yield world("ets", seed + n, count=8, branch=branch, same=same, force=1, vleft=170, fleft=100, vused=0, fused=0)
    for branch in "ABC":
        n += 1
        yield world("ets", seed + n, count=5, branch=branch, same=True, early=True)
    for branch in "ABC":
        n += 1
        yield world("ets", seed + n, count=6, branch=branch, same=True, vleft=0, fleft=0)


def edges(op):
    return {"ics": edges_ics, "ecsv": edges_ecsv, "gntms": edges_gntms, "skin": edges_skin, "link": edges_link, "copy": edges_copy,
            "ets": edges_ets}[op]


def rnd(op):
    return lambda r: world(op, r.getrandbits(48))


TESTS = [
    FnTest("InterpolateCoasterSpline", ppc, host, edge=edges("ics"), rnd=rnd("ics")),
    FnTest("ExtrudeCrossSectionVertices", ppc, host, edge=edges("ecsv"), rnd=rnd("ecsv")),
    FnTest("GetNewTrackMeshSet", ppc, host, edge=edges("gntms"), rnd=rnd("gntms")),
    FnTest("SkinCrossSectionFaces", ppc, host, edge=edges("skin"), rnd=rnd("skin")),
    FnTest("LinkFEStoreToFETextureCont", ppc, host, edge=edges("link"), rnd=rnd("link")),
    FnTest("CopyExtrudedFaceData", ppc, host, edge=edges("copy"), rnd=rnd("copy")),
    FnTest("ExtrudeTrackSection", ppc, host, edge=edges("ets"), rnd=rnd("ets")),
]
