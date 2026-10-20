"""Differential test for src/RideInstance.cpp: InstanceRideAnimSet, InstanceMesh, InstanceAnim,
InstanceDynamicMeshSet, DisableDataNodesRender, DisableDataNodesRenderExcept, SetupRideHeads,
CorrectFaceFlags1, CorrectNodeFlags2, OverrideActiveChannels, GetFontPath and ERRORBox, against
libtpw_unit_RideInstance.dylib (tests/unit_RideInstance.cpp).

The twelve run as the original on the PowerPC side and call each other for real (InstanceRideAnimSet runs
InstanceMesh, InstanceAnim and the dynamic mesh set's instancing). Outside the unit is a fake on both sides:
the allocator (MReleaseAllocF, which hands out poisoned blocks and records what it was asked for), the
engine's 64-bit alignment, CreateEngineMesh, the game's local path and the debug routine.

THE TWO SIDES' MEMORY. The mesh file's records (headers, nodes, tables) have the file's layout on both
sides: one builder describes them for the two, the PowerPC side writing big-endian words and absolute
addresses, the host little-endian words and arena offsets (Ptr32 fields). The game's own records (the mesh
record, the embedded set and its records, the ride set and its entries, the dynamic mesh set) have the Mac's
layout on the PowerPC side and the host's native layout on the other (tpw_ri_layout reports it): the same
builder writes both from field names. Every object has a margin of 0x100 bytes on both sides, so that a
pointer into it, or a little before or past it, has a label (object, offset): that is how pointers
compare. A pointer into an array of native records compares as (object, index, remainder), and a pointer
into a block the allocator fake handed out compares as (block, region, index, remainder), the regions
being the pieces the function lays out in it (their sizes differ between the Mac and the host, their order
does not).

WHAT IS COMPARED. After a call: the answer; the output pointer as a label; the call log (the allocator's
sizes brought to the Mac's: the records laid out in the blocks are larger on the host, see sizes()); every
record of every object the case built, read back by type (so that a record the function changed, or should
not have, shows), and the new blocks, read by what their headers say.
"""
import ctypes as C
import json
import pathlib
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RideInstance"

MASK = 0xFFFFFFFF
MARGIN = 0x100

INSTANCE_SET, INSTANCE_MESH, INSTANCE_ANIM, INSTANCE_DMESH = 0x10062E1C, 0x10061B1C, 0x10062938, 0x10062BA8
DISABLE, DISABLE_EXCEPT, CORRECT_FACE, CORRECT_NODE = 0x101FFC4C, 0x101FFD08, 0x10067FDC, 0x10067E1C
SETUP_HEADS, GET_FONT, OVERRIDE, ERRORBOX = 0x10200B28, 0x101EEAC8, 0x100C609C, 0x1006FD3C
ALLOC, CREATE_ENGINE_MESH, GET_LOCAL_PATH = 0x10064BD4, 0x10064F40, 0x10004D4C
ALL_FUNCTIONS = [INSTANCE_SET, INSTANCE_MESH, INSTANCE_ANIM, INSTANCE_DMESH, DISABLE, DISABLE_EXCEPT, CORRECT_FACE,
                 CORRECT_NODE, SETUP_HEADS, GET_FONT, OVERRIDE, ERRORBOX]
G_GAME_OPTIONS, G_DEFAULT_HEAP, G_POLY_COUNT = 0x102B2B48, 0x102C6C50, 0x102C1E18
G_FILE_INFO = 0x102B3458        # gTPFileInfo, the object GetLocalPath is called on


# ------------------------------------------------------------------ layouts

LAYOUT = {}      # the host's, from the library


def host_layout(lib):
    if not LAYOUT:
        lib.tpw_ri_layout.restype = C.c_char_p
        LAYOUT.update(json.loads(lib.tpw_ri_layout()))
    return LAYOUT


# The game's own records. kind: I u32, i s32, H u16, p native pointer, "nB" n bytes, "nI" n words. The Mac's
# offsets; the host's come from the library. mac_size: the Mac's size.
NATIVE = {
    "mesh": dict(mac_size=0x40, mac=dict(mF00=0, mHeader=4, mF08=8, mF0C=0xC, mF10=0x10, mF14=0x14, mEngineMesh=0x1C,
                                          mTypeEngineMesh=0x20, mAllocation=0x24, mF28=0x28, mRootNode=0x2C,
                                          mF30=0x30, mEngineMeshes=0x34, mF38=0x38),
                 kinds=dict(mF00="I", mHeader="p", mF08="p", mF0C="I", mF10="i", mF14="8B", mEngineMesh="p",
                            mTypeEngineMesh="p", mAllocation="p", mF28="p", mRootNode="p", mF30="p",
                            mEngineMeshes="i", mF38="8B")),
    "eset": dict(mac_size=0x14, mac=dict(mFlags=0, mRecords=4, mF08=8, mBlockCount=0xC, mBlocks=0x10),
                 kinds=dict(mFlags="I", mRecords="p", mF08="I", mBlockCount="I", mBlocks="p")),
    "erec": dict(mac_size=0x14, mac=dict(mFlags=0, mBlock=4, mF08=8, mF0C=0xC, mF10=0x10),
                 kinds=dict(mFlags="I", mBlock="p", mF08="I", mF0C="I", mF10="I")),
    "dmesh": dict(mac_size=0x40, mac=dict(mF00=0, mCount=4, mF06=6, mEntries=8, mF0C=0xC, mVertexSets=0x10,
                                           mPoints=0x14, mUVSets=0x18, mFaces=0x1C, mF20=0x20, mMesh=0x24, mF28=0x28,
                                           mF2C=0x2C, mF30=0x30, mAllocation=0x34, mF38=0x38, mShared=0x3C),
                  kinds=dict(mF00="I", mCount="H", mF06="H", mEntries="p", mF0C="I", mVertexSets="p", mPoints="p",
                             mUVSets="p", mFaces="p", mF20="I", mMesh="p", mF28="I", mF2C="I", mF30="I",
                             mAllocation="p", mF38="I", mShared="p")),
    "dentry": dict(mac_size=0x30, mac=dict(mF00=0, mMatrix=0x28, mFaceSets=0x2C),
                   kinds=dict(mF00="10I", mMatrix="p", mFaceSets="p")),
    "shared": dict(mac_size=0x20, mac=dict(mF00=0, mInstances=0x1C), kinds=dict(mF00="7I", mInstances="I")),
    # the ride set (mInstanceSelf is the Mac's word at 0xe0; the host's mFE0 is a word of its own, not used here)
    "set": dict(mac_size=0xFC, mac=dict(mSelf=0, mFlags=4, mHeads=8, mF0C=0xC, mTimeCount=0xE, mTimeControls=0x10,
                                         mF14=0x14, mAnimationCount=0xA8, mAnimations=0xAC, mDynamicMeshSet=0xB4,
                                         mFC0=0xC0, mFC4=0xC4, mFC8=0xC8, mFCC=0xCC, mFD0=0xD0, mFD4=0xD4,
                                         mEntryAllocation=0xD8, mAnimationHeaders=0xDC, mInstanceSelf=0xE0,
                                         mFE4=0xE4, mFE8=0xE8, mFEC=0xEC, mFF0=0xF0, mFF4=0xF4, mFF8=0xF8),
                kinds=dict(mSelf="p", mFlags="I", mHeads="p", mF0C="H", mTimeCount="H", mTimeControls="p", mF14="p",
                           mAnimationCount="I", mAnimations="p", mDynamicMeshSet="p", mFC0="I", mFC4="I", mFC8="I",
                           mFCC="I", mFD0="p", mFD4="i", mEntryAllocation="p", mAnimationHeaders="p",
                           mInstanceSelf="p", mFE4="I", mFE8="i", mFEC="i", mFF0="i", mFF4="i", mFF8="I")),
}
for _k in range(12):
    NATIVE["set"]["kinds"][f"g{_k}.mFlags"] = "I"
    NATIVE["set"]["kinds"][f"g{_k}.mCount"] = "I"
    NATIVE["set"]["kinds"][f"g{_k}.mEntries"] = "p"
    NATIVE["set"]["mac"][f"g{_k}.mFlags"] = 0x18 + 0xC * _k
    NATIVE["set"]["mac"][f"g{_k}.mCount"] = 0x1C + 0xC * _k
    NATIVE["set"]["mac"][f"g{_k}.mEntries"] = 0x20 + 0xC * _k
NATIVE["entry"] = dict(mac_size=8, mac=dict(mIndex=0, mAnimation=4), kinds=dict(mIndex="I", mAnimation="p"))


def native_offsets(side, struct_name):
    """(size, {field: offset}) of a game record on a side."""
    spec = NATIVE[struct_name]
    if side.is_ppc:
        return spec["mac_size"], spec["mac"]
    lay = side.layout
    if struct_name == "entry":
        return lay["set"]["entry"], dict(mIndex=0, mAnimation=lay["set"]["entry_animation"])
    offsets = {k: v for k, v in lay[struct_name].items() if k not in ("size", "end", "group", "entry", "entry_animation")}
    if struct_name == "set":
        for k in range(12):
            base = lay["set"]["mGroups"] + k * lay["set"]["group"]
            offsets[f"g{k}.mFlags"], offsets[f"g{k}.mCount"] = base, base + 4
            offsets[f"g{k}.mEntries"] = base + 8
    return lay[struct_name]["size"], offsets


# The mesh file's records: (offset, kind) with kind I (32-bit word), H (16-bit), P (Ptr32 field).
def words(start, n):
    return [(start + 4 * i, "I") for i in range(n)]


HEADER = (words(0, 12) + [(0x30, "I")] + [(0x34 + 2 * i, "H") for i in range(12)] + [(0x4C + 4 * i, "P") for i in range(13)]
          + words(0x80, 6) + [(0x98, "P"), (0x9C, "P"), (0xA0, "I"), (0xA4, "P"), (0xA8, "I"), (0xAC, "P"), (0xB0, "P"),
                              (0xB4, "I")])
NODE = (words(0, 1) + [(4, "P"), (8, "P"), (0xC, "P")] + words(0x10, 16)
        + [(0x50, "H"), (0x52, "H"), (0x54, "P"), (0x58, "H"), (0x5A, "H"), (0x5C, "H"), (0x5E, "H")]
        + [(0x60, "P"), (0x64, "P"), (0x68, "P"), (0x6C, "P"), (0x70, "P"), (0x74, "P")] + words(0x78, 6)
        + [(0x90, "I"), (0x94, "P"), (0x98, "P"), (0x9C, "P")])
MINI = words(0, 1) + [(4, "P"), (8, "P"), (0xC, "P")] + words(0x10, 16) + [(0x50, "H"), (0x52, "H"), (0x54, "P")]
FSET = [(0, "P"), (4, "I"), (8, "I"), (0xC, "B"), (0xD, "B"), (0xE, "B"), (0xF, "B")]
TEX = [(0, "H"), (2, "H"), (4, "I")]
TINFO = [(0, "I"), (4, "P"), (8, "I"), (0xC, "P")]
EMB = words(0, 3) + [(0xC, "P"), (0x10, "P")]
NAME = words(0, 5) + [(0x14, "H")]
ANIM = (words(0, 3) + [(0xC + 2 * i, "H") for i in range(9)] + [(0x1E, "H")] + [(0x20 + 4 * i, "P") for i in range(10)])
NODEANIM = (words(0, 4) + [(0x10, "H"), (0x12, "H"), (0x14, "H"), (0x16, "H")] + [(0x18 + 4 * i, "P") for i in range(8)]
            + [(0x38, "I"), (0x3C, "P")])
MORPH = [(0, "H"), (2, "H"), (4, "I"), (8, "I"), (0xC, "P"), (0x10, "P")] + words(0x14, 8)
KEYDATA = [(0, "I"), (4, "P"), (8, "P"), (0xC, "P"), (0x10, "I")]
MORPH_REAL = [(0, "H"), (2, "H"), (4, "I"), (8, "I"), (0xC, "P"), (0x10, "P")] + words(0x14, 6)      # the 0x2c bytes of a morph set
SPECS = {"header": (HEADER, 0xB8), "node": (NODE, 0xA0), "mini": (MINI, 0x58), "fset": (FSET, 0x10), "tex": (TEX, 8),
         "tinfo": (TINFO, 0x10), "emb": (EMB, 0x14), "name": (NAME, 0x16), "anim": (ANIM, 0x48),
         "nodeanim": (NODEANIM, 0x40), "morph": (MORPH, 0x34), "keydata": (KEYDATA, 0x14),
         "vset": (words(0, 12), 0x30), "point": (words(0, 3), 0xC), "uv": (words(0, 8), 0x20), "face": (words(0, 2), 8),
         "word": (words(0, 1), 4), "block": (words(0, 16), 0x40), "node78": (NODE, 0xA0), "byte": ([(0, "B")], 1),
         "morph_real": (MORPH_REAL, 0x2C)}
KINDFMT = {"I": "I", "H": "H", "P": "I", "B": "B"}
KINDMASK = {"I": MASK, "H": 0xFFFF, "P": MASK, "B": 0xFF}


# ------------------------------------------------------------------ the two sides' memory


class Side:
    """Common part: the objects of a case by name, labels for addresses, and the writers and readers of the
    two kinds of record."""

    def __init__(self):
        self.objs = {}          # name -> (base, size, element size)
        self.blocks = []        # blocks the allocator fake handed out: (address, size asked, regions)
        self.misalign = 0
        self.adjust = {}        # allocation index -> function from the host's size to the Mac's

    def alloc_size(self, index, size):
        return self.adjust[index](size) if index in self.adjust else size

    # -- objects
    def base(self, name):
        return self.objs[name][0]

    def ptr32(self, name, offset=0):
        """The value of a Ptr32 field that points at `name` + offset (0 for None)."""
        return 0 if name is None else self.p32(self.base(name) + offset)

    def nat(self, name, offset=0):
        """The value of a native pointer field that points at `name` + offset (0 for None)."""
        return 0 if name is None else self.base(name) + offset

    # -- records of the file's layout
    def put(self, addr, spec_name, values):
        """Writes one record: values maps offset -> a number, or a (name, offset) pair for a pointer."""
        spec, _ = SPECS[spec_name]
        for offset, kind in spec:
            v = values.get(offset, 0)
            if kind == "P" and isinstance(v, tuple):
                v = self.ptr32(*v)
            self.w(addr + offset, KINDFMT[kind], v & KINDMASK[kind])

    def get(self, addr, spec_name):
        """Reads one record back: pointers as labels, the rest as numbers."""
        spec, _ = SPECS[spec_name]
        out = []
        for offset, kind in spec:
            (v,) = self.r(addr + offset, KINDFMT[kind])
            out.append(self.label32(v) if kind == "P" else v)
        return out

    def raw(self, addr, spec_name, offset):
        (v,) = self.r(addr + offset, KINDFMT[dict(SPECS[spec_name][0])[offset]])
        return v

    # -- the game's records
    def nput(self, addr, struct_name, field, value, index=0):
        size, offsets = native_offsets(self, struct_name)
        kind = NATIVE[struct_name]["kinds"][field]
        at = addr + index * size + offsets[field]
        if kind == "p":
            self.w(at, self.pfmt, value)
        elif kind.endswith("B") or (kind.endswith("I") and kind[:-1].isdigit()):
            self.w(at, kind, *value)
        else:
            self.w(at, kind, value & (0xFFFF if kind == "H" else MASK) if kind != "i" else value)

    def nraw(self, addr, struct_name, field, index=0):
        size, offsets = native_offsets(self, struct_name)
        kind = NATIVE[struct_name]["kinds"][field]
        at = addr + index * size + offsets[field]
        if kind == "p":
            return self.r(at, self.pfmt)[0]
        if len(kind) > 1:
            return self.r(at, kind)
        return self.r(at, kind)[0]

    def nget(self, addr, struct_name, index=0):
        """A native record read back: pointers as labels."""
        out = []
        for field, kind in NATIVE[struct_name]["kinds"].items():
            if field not in native_offsets(self, struct_name)[1]:
                continue
            v = self.nraw(addr, struct_name, field, index)
            out.append((field, self.labelp(v) if kind == "p" else v))
        return out

    # -- labels
    def label(self, addr):
        if addr == 0:
            return 0
        for name, (base, size, elem) in self.objs.items():
            if base - MARGIN <= addr < base + size + MARGIN:
                rel = addr - base
                return (name, rel) if elem == 1 else (name, rel // elem, rel % elem)
        for i, (base, size, regions) in enumerate(self.blocks):
            if base - 0x20 <= addr < base + size + MARGIN - 0x20:
                rel = addr - base
                for start, end, region, elem in regions:
                    if start <= rel < end:
                        return (f"alloc{i}", region, (rel - start) // elem, (rel - start) % elem)
                return (f"alloc{i}", "?", rel)
        return None

    def label32(self, value):
        """A Ptr32 field's value as a label."""
        if value == 0:
            return 0
        found = self.label(self.a32(value))
        return found if found is not None else ("?", value)

    def labelp(self, value):
        """A native pointer field's value as a label."""
        if value == 0:
            return 0
        found = self.label(value)
        return found if found is not None else ("?", value & MASK)


class PpcSide(Side):
    is_ppc = True
    pfmt = "I"
    psize = 4

    def __init__(self, e, log):
        super().__init__()
        self.e, self.log = e, log
        self.reserved = []
        self.layout = None

    def new(self, name, size, elem=1):
        raw = self.e.alloc(size + 2 * MARGIN, b"\xa5" * (size + 2 * MARGIN))
        self.objs[name] = (raw + MARGIN, size, elem)
        return raw + MARGIN

    def reserve(self, size):
        raw = self.e.alloc(size + 0x10 + 2 * MARGIN, b"\xa5" * (size + 0x10 + 2 * MARGIN))
        self.reserved.append(raw + MARGIN)

    def w(self, addr, fmt, *values):
        self.e.write(addr, struct.pack(">" + fmt, *values))

    def r(self, addr, fmt):
        return struct.unpack(">" + fmt, self.e.read(addr, struct.calcsize(">" + fmt)))

    def p32(self, addr):
        return addr

    def a32(self, value):
        return value


class HostSide(Side):
    is_ppc = False
    pfmt = "Q"
    psize = 8

    def __init__(self, lib):
        super().__init__()
        self.lib = lib
        self.layout = host_layout(lib)
        lib.tpw_ri_base.restype = C.c_uint64
        lib.tpw_ri_block.restype = C.c_uint64
        lib.tpw_ri_block.argtypes = [C.c_uint32]
        lib.tpw_ri_reserve.restype = C.c_uint64
        lib.tpw_ri_reserve.argtypes = [C.c_uint32]
        lib.tpw_ri_begin.argtypes = [C.c_uint32, C.c_uint32]
        self.arena = lib.tpw_ri_base()

    def new(self, name, size, elem=1):
        base = self.lib.tpw_ri_block(size)
        self.objs[name] = (base, size, elem)
        return base

    def reserve(self, size):
        self.lib.tpw_ri_reserve(size)

    def w(self, addr, fmt, *values):
        data = struct.pack("<" + fmt, *values)
        C.memmove(addr, data, len(data))

    def r(self, addr, fmt):
        return struct.unpack("<" + fmt, C.string_at(addr, struct.calcsize("<" + fmt)))

    def p32(self, addr):
        return addr - self.arena if addr else 0

    def a32(self, value):
        return value + self.arena if value else 0


def align8(x):
    return (x + 7) & ~7


def sizes(side, name):
    return native_offsets(side, name)[0]


# ------------------------------------------------------------------ the PowerPC's fakes and the calls


class Globals:
    """Keeps the original's globals a case writes and puts them back when it is over."""

    def __init__(self, e):
        self.e, self.kept = e, []

    def set32(self, address, value):
        self.kept.append((address, self.e.read(address, 4)))
        self.e.write(address, struct.pack(">I", value & MASK))

    def close(self):
        for address, data in reversed(self.kept):
            self.e.write(address, data)


def make_stubs(side):
    """The PowerPC side's fakes; each records the event the host's fake of the same name records."""
    S = CL.Stubs()
    S.real.update(ALL_FUNCTIONS)

    def alloc(e):
        size, flags = e.r(3), e.r(4)
        index = len(side.blocks)
        if side.reserved:
            base = side.reserved.pop(0)
        else:
            raw = e.alloc(size + 0x10 + 2 * MARGIN, b"\xa5" * (size + 0x10 + 2 * MARGIN))
            base = raw + MARGIN
        base += side.misalign
        e.write(base, bytes([side.fill]) * (size + 0x10))
        side.blocks.append([base, size, []])
        e.cl.rec("Alloc", (index, size, flags))
        e.cl.labels[base] = 200 + index
        e.cl.labels[align8(base)] = 200 + index
        e.set_r(3, base)

    S.internal(ALLOC, alloc)

    def aligner(step):
        def stub(e):
            ref, moved = e.r(3), e.r(4)
            value = e.u32(ref)
            aligned = (value + step - 1) & ~(step - 1) & MASK
            if moved:
                e.write(moved, struct.pack(">I", (e.u32(moved) + aligned - value) & MASK))
            e.write(ref, struct.pack(">I", aligned))
        return stub

    for name in ("AlignTo64Bit__FRUlPUl", "AlignTo64Bit__FRPvPUl"):
        S.imp(name, aligner(8))
    for name in ("AlignTo32Bit__FRUlPUl", "AlignTo32Bit__FRPvPUl"):
        S.imp(name, aligner(4))
    S.internal(CREATE_ENGINE_MESH, lambda e: e.cl.rec("CreateEngineMesh", (e.cl.lab(e.r(3)),)))

    def local_path(e):
        e.cl.rec("GetLocalPath")
        e.set_r(3, side.path_object)

    S.internal(GET_LOCAL_PATH, local_path)
    return S


FUNCS = {   # key: (the original's address, the library's function, argument types, result type)
    "mesh": (INSTANCE_MESH, "tpw_ri_instance_mesh", [C.c_void_p, C.c_uint64, C.c_uint32, C.c_void_p, C.c_uint32], C.c_int32),
    "anim": (INSTANCE_ANIM, "tpw_ri_instance_anim", [C.c_void_p, C.c_uint64, C.c_uint32, C.c_void_p], C.c_int32),
    "dmesh": (INSTANCE_DMESH, "tpw_ri_instance_dmesh", [C.c_void_p, C.c_uint64, C.c_uint32, C.c_void_p], C.c_int32),
    "set": (INSTANCE_SET, "tpw_ri_instance_set", [C.c_void_p, C.c_uint64, C.c_uint32, C.c_void_p, C.c_uint32], C.c_int32),
    "disable": (DISABLE, "tpw_ri_disable", [C.c_void_p, C.c_uint32], None),
    "except": (DISABLE_EXCEPT, "tpw_ri_disable_except", [C.c_void_p, C.c_uint32], None),
    "heads": (SETUP_HEADS, "tpw_ri_setup_heads", [C.c_void_p], None),
    "face_flags": (CORRECT_FACE, "tpw_ri_correct_face_flags", [C.c_void_p, C.c_int32], None),
    "node_flags": (CORRECT_NODE, "tpw_ri_correct_node_flags", [C.c_void_p], None),
    "channels": (OVERRIDE, "tpw_ri_override_channels", [C.c_void_p], None),
    "font": (GET_FONT, "tpw_ri_font_path", [C.c_void_p], C.c_int32),
}


def call(side, key, *args):
    entry, symbol, types, result = FUNCS[key]
    if side.is_ppc:
        return T.call_ppc(side.e, entry, [a & MASK for a in args])[0]
    fn = getattr(side.lib, symbol)
    fn.argtypes, fn.restype = types, result
    return fn(*args) or 0


def finish(side):
    """The call log of the case, with the host's allocation sizes brought to the Mac's."""
    if side.is_ppc:
        return list(side.log.log)
    log = CL.host_end(side.lib)
    out = []
    for name, ints, texts in log:
        if name == "Alloc":
            ints = (ints[0], side.alloc_size(ints[0], ints[1]), ints[2])
        out.append((name, tuple(ints), texts))
    return out


def begin_ppc(e, case):
    log = CL.PpcLog({})
    e.cl = log
    side = PpcSide(e, log)
    side.misalign = case.get("misalign", 0)
    side.fill = case.get("fill", 0xA5)
    side.path_object = e.alloc(0x20, b"\0" * 0x20)
    side.stubs = make_stubs(side)
    side.saved = side.stubs.activate(e)
    side.globals = Globals(e)
    return side


def end_ppc(side):
    CL.Stubs.deactivate(side.e, side.saved)
    side.globals.close()


def begin_host(lib, case):
    side = HostSide(lib)
    lib.tpw_ri_begin(case.get("misalign", 0), case.get("fill", 0xA5))
    CL.host_begin(lib, {})
    return side


def end_host(side):
    C.c_int32.in_dll(side.lib, "tpw_cl_on").value = 0


def set_globals(side, options=0, heap=0, poly=None):
    """gGameOptions and the default heap (and gPolySearchCount) on a side."""
    if side.is_ppc:
        side.globals.set32(G_GAME_OPTIONS, options)
        side.globals.set32(G_DEFAULT_HEAP, heap)
        if poly is not None:
            side.globals.set32(G_POLY_COUNT, poly)
    else:
        side.lib.tpw_ri_set_globals.argtypes = [C.c_uint32, C.c_uint64]
        side.lib.tpw_ri_set_globals(options, heap)
        if poly is not None:
            side.lib.tpw_ri_set_poly_search_count.argtypes = [C.c_uint32]
            side.lib.tpw_ri_set_poly_search_count(poly)


def poly_count(side):
    if side.is_ppc:
        return side.e.u32(G_POLY_COUNT)
    side.lib.tpw_ri_poly_search_count.restype = C.c_uint32
    return side.lib.tpw_ri_poly_search_count()


def guards(side, case):
    """For each block the allocator handed out: True when the sixteen bytes after the size asked for are as the
    allocator filled them (the function stayed inside its allocation)."""
    fill = case.get("fill", 0xA5)
    return [bytes(side.r(base + size, "16B")) == bytes([fill]) * 16 for base, size, _ in side.blocks]


def read_blocks(side):
    """The blocks the allocator fake handed out, as the library reports them (the PowerPC side keeps its own)."""
    if side.is_ppc:
        return
    side.blocks = []
    address, size = C.c_uint64(), C.c_uint32()
    side.lib.tpw_ri_allocation.argtypes = [C.c_int32, C.c_void_p, C.c_void_p]
    i = 0
    while side.lib.tpw_ri_allocation(i, C.byref(address), C.byref(size)):
        side.blocks.append([address.value, size.value, []])
        i += 1


# ------------------------------------------------------------------ the mesh world


class Ctx:
    """One side of one case: the objects built so far and the records to read back after the call."""

    def __init__(self, side):
        self.side = side
        self.views = []     # (label, address, record type, count)
        self.facts = {}     # what the builder knows that the records alone do not tell (real files)

    def view(self, label, name, spec, count, offset=0):
        self.views.append((label, self.side.base(name) + offset, spec, count))

    def native_view(self, label, name, struct_name, count):
        self.views.append((label, self.side.base(name), "native:" + struct_name, count))


class Scope:
    """A side's objects under a name prefix (a case may hold two meshes): the builder names objects without it."""

    def __init__(self, ctx, prefix):
        self.ctx, self.side, self.prefix = ctx, ctx.side, prefix

    def __getattr__(self, name):
        return getattr(self.side, name)

    def new(self, name, size, elem=1):
        return self.side.new(self.prefix + name, size, elem)

    def base(self, name):
        return self.side.base(self.prefix + name)

    def nat(self, name, offset=0):
        return 0 if name is None else self.side.nat(self.prefix + name, offset)

    def put(self, addr, spec_name, values):
        mapped = {k: (self.prefix + v[0], v[1]) if isinstance(v, tuple) else v for k, v in values.items()}
        self.side.put(addr, spec_name, mapped)

    def view(self, label, name, spec, count, offset=0):
        self.ctx.views.append((self.prefix + label, self.side.base(self.prefix + name) + offset, spec, count))

    def native_view(self, label, name, struct_name, count):
        self.ctx.views.append((self.prefix + label, self.side.base(self.prefix + name), "native:" + struct_name, count))


def word(rnd):
    """An opaque word: never small enough to be taken for an address on either side."""
    return rnd.getrandbits(32) | 0x80000000


def mesh_params(rnd, **fixed):
    """A random mesh: counts, tables and flags (fixed overrides)."""
    c = dict(seed=rnd.getrandbits(32), n=rnd.choice([0, 1, 1, 2, 3, 4]), m=rnd.choice([0, 0, 1, 2, 3]),
             e=rnd.choice([0, 0, 1, 2, 3, 4]), k=rnd.choice([0, 1, 2, 3]), t=rnd.choice([0, 1, 2, 3, 4]),
             s=rnd.choice([0, 1, 2, 3, 5]), v=rnd.choice([0, 1, 2, 3, 6]), p=rnd.choice([0, 1, 2, 3, 6]),
             f=rnd.choice([0, 1, 2, 3, 6]), u=rnd.choice([0, 1, 2, 3, 5]),
             hflags=rnd.choice([0, 0, 0x1000, 8, 0x8000, 0x10000, 0x18000, 0x1000 | 0x10000, 8 | 0x8000, 8 | 0x10000])
             | (rnd.getrandbits(8) & 0x61), below=rnd.randrange(2), misalign=rnd.choice([0, 0, 4]),
             options=rnd.choice([0, 0x2000, 0x2000, 0xdfff & rnd.getrandbits(16)]), heap=rnd.choice([0, 0x1234]),
             fill=rnd.choice([0xA5, 0xA5, 0xA5, 0]))
    c.update(fixed)
    c["e"] = min(c["e"], c["n"] + c["m"])
    return c


def build_mesh(ctx, c, prefix=""):
    """The mesh of case `c`: the game's record, the header, the nodes and tables, the embedded entries and
    set. Returns nothing; the objects are named (see below) and the views registered."""
    s = Scope(ctx, prefix)
    rnd = random.Random(c["seed"])
    n, m, e, k = c["n"], c["m"], c["e"], c["k"]
    nodes = n + m
    s.new("engine", 0x40)
    s.new("othermesh", sizes(s, "mesh"))
    s.new("mesh", sizes(s, "mesh"))
    s.new("hdr", 0xB8)
    s.new("nodes", max(n * 0xA0 + m * 0x58, 8))
    s.new("solo", 0xA0)
    s.new("vsets", max(c["v"], 1) * 0x30)
    s.new("points", max(c["p"], 1) * 0xC)
    s.new("faces", max(c["f"], 1) * 8)
    s.new("fsets", max(c["s"], 1) * 0x10)
    s.new("uvs", max(c["u"], 1) * 0x20)
    s.new("tex", max(c["t"], 1) * 8)
    s.new("tinfo", max(c["t"], 1) * 0x10)
    s.new("folder", 8)
    for i in range(c["t"]):
        s.new(f"name{i}", 0x20)
    for i in range(n):
        s.new(f"nuv{i}", 0x20 * 12)
    s.new("emb", max(e, 1) * 0x14)
    s.new("eset", sizes(s, "eset"))
    s.new("erecs", max(e, 1) * sizes(s, "erec"), sizes(s, "erec"))
    s.new("eblocks", max(k, 1) * 0x40)

    # the nodes
    def ref(i):
        return ("nodes", i * 0xA0) if i < n else ("nodes", n * 0xA0 + (i - n) * 0x58)

    nodes_pointer = n > 0 or rnd.random() < 0.4
    # mini nodes counted but no table (a zero-filled allocator only: the copy's mini nodes are zeros, which are not
    # rebased); nothing may then point at a mini node, since the copy of the next table lies where they would be
    mini_null = bool(m) and c.get("fill") == 0 and bool(c.get("flags", 0) & 5) and nodes_pointer and e <= n and rnd.random() < 0.4
    reachable = n if mini_null else nodes

    def pick_link(i):
        """A link to a later node (the links never loop, as the walk along them would not end)."""
        return ref(rnd.randrange(i + 1, reachable)) if i + 1 < reachable and rnd.random() < 0.5 else 0

    next_fset = 0       # the nodes' face sets do not overlap (a face set that two nodes share would be rebased twice)
    fsets_null = rnd.random() < 0.1     # no face set table at all: then no node names one (the rebasing would use 0)
    nuv_present = []
    for i in range(n):
        flags = 0
        for bit, chance in ((0x80, 0.45), (0x200, 0.15), (0x4000, 0.2), (0x20000, 0.15), (0x40, 0.15), (0x400, 0.2)):
            if rnd.random() < chance:
                flags |= bit
        flags |= rnd.getrandbits(32) & 0x0F000000
        left = c["s"] - next_fset
        count = rnd.randrange(0, min(3, left) + 1) if left > 0 else 0
        first = next_fset + (rnd.randrange(0, left - count + 1) if left > 0 else 0)
        next_fset = first + count
        if next_fset > c["s"] or fsets_null:
            first = next_fset = c["s"]
            count = 0
        uv_ok = rnd.random() > 0.15
        nuv_present.append(bool(flags & 0x80 and uv_ok))
        v = {0: flags, 4: pick_link(i), 8: pick_link(i), 0xC: pick_link(i), 0x50: rnd.getrandbits(16), 0x52: rnd.getrandbits(16),
             0x54: ("folder", 0) if rnd.random() < 0.5 else 0, 0x58: rnd.randrange(40), 0x5A: count,
             0x5C: rnd.randrange(40), 0x5E: rnd.randrange(48)}
        for off in range(0x10, 0x50, 4):
            v[off] = word(rnd)
        # pointers into the tables: any distance (they are only rebased), the face sets and UV sets within
        reach = lambda size, step: rnd.randrange(-0x70, size + 1)
        v[0x60] = ("vsets", reach(c["v"] * 0x30, 0x30)) if rnd.random() < 0.8 else 0
        v[0x64] = ("points", reach(c["p"] * 0xC, 0xC)) if rnd.random() < 0.8 else 0
        if flags & 0x80:
            v[0x68] = (f"nuv{i}", 0) if uv_ok else 0       # the UV sets of a node that has some, or none to copy
        else:
            v[0x68] = ("uvs", rnd.randrange(0, max(c["u"], 1)) * 0x20)
        v[0x6C] = ("fsets", first * 0x10)
        if rnd.random() < 0.1:      # a pointer off the table: only for a node without face sets (they would be written)
            v[0x6C] = ("fsets", rnd.randrange(-0x70, c["s"] * 0x10 + 1))
            v[0x5A] = 0
        if fsets_null:
            v[0x6C] = 0
        v[0x70] = ("faces", rnd.randrange(0, max(c["f"], 1)) * 8) if rnd.random() < 0.8 else 0
        v[0x74] = 0 if rnd.random() < 0.7 else ("points", 0)
        for off in range(0x78, 0x90, 4):
            v[off] = word(rnd)
        v[0x90] = word(rnd)
        v[0x94] = 0
        v[0x98] = 0
        v[0x9C] = ("points", 0) if rnd.random() < 0.3 else 0
        s.put(s.base("nodes") + i * 0xA0, "node", v)
    for j in range(m):
        x = 0x200 | (rnd.getrandbits(16) & 0xC0C0 & ~0x8000 | (0x4000 if rnd.random() < 0.2 else 0))
        v = {0: x * 0x10001, 4: pick_link(n + j), 8: pick_link(n + j), 0xC: pick_link(n + j), 0x50: rnd.getrandbits(16),
             0x52: rnd.getrandbits(16), 0x54: 0}
        for off in range(0x10, 0x50, 4):
            v[off] = word(rnd)
        s.put(s.base("nodes") + n * 0xA0 + j * 0x58, "mini", v)
    s.view("nodes", "nodes", "node", n)
    s.view("minis", "nodes", "mini", m, n * 0xA0)
    # the other node the header names when the nodes are not copied: a node of the table, or a record of its own
    s.put(s.base("solo"), "node", {0: 0x200 if rnd.random() < 0.4 else 0, 0x54: 0, 0x5A: 0, 0x6C: 0})
    s.view("solo", "solo", "node78", 1)
    root78 = ref(rnd.randrange(reachable)) if reachable else ("solo", 0)
    if rnd.random() < 0.12 and c.get("flags", 0) & 5:
        root78 = 0      # no other node: only when the nodes are copied (else the size is read from it)

    # the tables
    for name, spec, count, size in (("vsets", "vset", c["v"], 0x30), ("points", "point", c["p"], 0xC),
                                    ("faces", "face", c["f"], 8), ("uvs", "uv", c["u"], 0x20), ("tex", "tex", c["t"], 8),
                                    ("eblocks", "block", k, 0x40)):
        for i in range(count):
            s.put(s.base(name) + i * size, spec, {off: word(rnd) for off, _ in SPECS[spec][0]})
        s.view(name, name, spec, count)
    for i in range(n):
        if nuv_present[i]:
            for q in range(12):
                s.put(s.base(f"nuv{i}") + q * 0x20, "uv", {off: word(rnd) for off, _ in SPECS["uv"][0]})
            s.view(f"nuv{i}", f"nuv{i}", "uv", 12)
    ctx.facts[prefix + "nuv_present"] = nuv_present
    for i in range(c["s"]):
        v = {0: ("tex", rnd.randrange(0, max(c["t"], 1)) * 8) if c["t"] and rnd.random() < 0.8 else 0,
             4: word(rnd), 8: word(rnd), 0xC: rnd.getrandbits(8), 0xD: rnd.getrandbits(8), 0xE: rnd.getrandbits(8),
             0xF: rnd.getrandbits(8)}
        s.put(s.base("fsets") + i * 0x10, "fset", v)
    s.view("fsets", "fsets", "fset", c["s"])
    named = 0       # without header flag 0x10000 the size allows two names, however many infos have flag 2
    for i in range(c["t"]):
        info_flags = rnd.getrandbits(8) & 3
        if info_flags & 2 and not c["hflags"] & 0x10000 and named >= 2:
            info_flags &= ~2
        named += (info_flags & 2) != 0
        v = {0: info_flags | (rnd.getrandbits(8) << 8), 4: ("folder", 0) if rnd.random() < 0.5 else 0, 8: word(rnd),
             0xC: (f"name{i}", 0) if e or rnd.random() < 0.9 else 0}      # a missing name only without embedded data
        s.put(s.base("tinfo") + i * 0x10, "tinfo", v)
        s.put(s.base(f"name{i}"), "name", {off: word(rnd) for off, _ in NAME})
        s.view(f"name{i}", f"name{i}", "name", 1)
    s.view("tinfo", "tinfo", "tinfo", c["t"])
    ctx.facts[prefix + "info_names"] = [s.raw(s.base("tinfo") + i * 0x10, "tinfo", 0xC) != 0 for i in range(c["t"])]
    # textures: the flags that matter to CorrectNodeFlags2 (the first 16-bit word)
    for i in range(c["t"]):
        s.w(s.base("tex") + i * 8, "H", rnd.choice([0, 0x40, 0x80, 0xC0, 0x1234, 0xFFFF]))

    # embedded entries, the mesh's embedded set and its records
    first = rnd.randrange(0, max(reachable - e, 0) + 1) if reachable else 0
    for i in range(e):
        s.put(s.base("emb") + i * 0x14, "emb", {0: rnd.choice(c.get("emb_flags") or [0, 2, 0x30, 0x80, 0x40040, 0x580F00, 0x100, 0x80 | 2, word(rnd)]),
                                                  4: word(rnd), 8: rnd.choice([0, 1, 2, 4, 8, 0x10, 3, 0x21, 0xFF]),
                                                  0xC: 0, 0x10: 0})
    s.view("emb", "emb", "emb", e)
    s.nput(s.base("eset"), "eset", "mFlags", rnd.getrandbits(4))
    records_present = bool(e) and rnd.random() > 0.1
    blocks_present = bool(k) and rnd.random() > 0.1
    if blocks_present and e and not records_present:
        blocks_present = False      # the blocks would be copied over the records that were not
    ctx.facts[prefix + "eset_present"] = (records_present, blocks_present)
    s.nput(s.base("eset"), "eset", "mRecords", s.nat("erecs") if records_present else 0)
    s.nput(s.base("eset"), "eset", "mF08", word(rnd))
    s.nput(s.base("eset"), "eset", "mBlockCount", k)
    s.nput(s.base("eset"), "eset", "mBlocks", s.nat("eblocks") if blocks_present else 0)
    flagged = 0
    for i in range(e):
        flag = rnd.random() < 0.5
        flagged += flag
        s.nput(s.base("erecs"), "erec", "mFlags", (1 if flag else 0) | rnd.getrandbits(8) & 0xE, i)
        s.nput(s.base("erecs"), "erec", "mBlock", s.nat("eblocks", rnd.randrange(max(k, 1)) * 0x40) if flag and k else 0, i)
        for f in ("mF08", "mF0C", "mF10"):
            s.nput(s.base("erecs"), "erec", f, word(rnd), i)
    s.native_view("eset", "eset", "eset", 1)
    s.native_view("erecs", "erecs", "erec", e)

    # the header
    hflags = c["hflags"]
    h = {0x00: word(rnd), 0x04: word(rnd), 0x08: word(rnd), 0x0C: word(rnd), 0x10: word(rnd), 0x14: word(rnd),
         0x18: word(rnd), 0x1C: word(rnd), 0x20: word(rnd), 0x24: word(rnd), 0x28: word(rnd), 0x2C: word(rnd),
         0x30: hflags, 0x34: rnd.getrandbits(16), 0x36: c["t"], 0x38: c["v"], 0x3A: c["u"], 0x3C: c["s"], 0x3E: c["f"],
         0x40: rnd.getrandbits(16), 0x42: nodes, 0x44: n, 0x46: first, 0x48: e, 0x4A: c["p"],
         0x4C: ("folder", 0), 0x50: ("tex", 0) if c["t"] or rnd.random() > 0.3 else 0, 0x54: ("tinfo", 0) if c["t"] or rnd.random() < 0.5 else 0,
         0x58: ("vsets", 0) if rnd.random() > 0.15 else 0, 0x5C: ("points", 0),
         0x60: ("uvs", 0) if c["u"] or rnd.random() < 0.5 else 0, 0x64: 0 if fsets_null else ("fsets", 0),
         0x68: ("faces", 0) if c["f"] or rnd.random() < 0.5 else 0, 0x6C: 0,
         0x70: ("nodes", 0) if nodes_pointer else 0, 0x74: ("nodes", n * 0xA0) if m else 0, 0x78: root78,
         0x7C: ("emb", 0) if e else 0, 0x98: 0, 0x9C: ("hdr", 0), 0xA0: word(rnd), 0xA4: 0, 0xA8: word(rnd), 0xAC: 0,
         0xB0: ("hdr", 0), 0xB4: word(rnd)}
    for off in range(0x80, 0x98, 4):
        h[off] = word(rnd)
    if not c.get("flags", 0) & 4 and rnd.random() < 0.15:
        h[0x5C] = 0         # no points table: the nodes' points are only rebased with the vertex sets (flag 4)
    if mini_null:
        h[0x74] = 0
    s.put(s.base("hdr"), "header", h)
    s.view("hdr", "hdr", "header", 1)
    ctx.facts[prefix + "src_header"] = s.base("hdr")

    # the game's record
    mesh = s.base("mesh")
    s.nput(mesh, "mesh", "mF00", word(rnd) | rnd.choice([0, 1]))
    s.nput(mesh, "mesh", "mHeader", s.nat("hdr"))
    s.nput(mesh, "mesh", "mF08", s.nat("othermesh") if rnd.random() < 0.5 else 0)
    s.nput(mesh, "mesh", "mF0C", word(rnd))
    s.nput(mesh, "mesh", "mF10", rnd.randrange(-100, 100))
    s.nput(mesh, "mesh", "mF14", tuple(rnd.getrandbits(8) for _ in range(8)))
    s.nput(mesh, "mesh", "mEngineMesh", s.nat("engine") if rnd.random() < 0.7 else 0)
    s.nput(mesh, "mesh", "mTypeEngineMesh", s.nat("engine") if rnd.random() < 0.7 else 0)
    s.nput(mesh, "mesh", "mAllocation", s.nat("engine") if rnd.random() < 0.5 else 0)
    s.nput(mesh, "mesh", "mF28", s.nat("eset") if (e and not c.get("no_eset")) or rnd.random() < 0.3 else 0)
    s.nput(mesh, "mesh", "mRootNode", s.nat("nodes", rnd.randrange(n) * 0xA0) if n and rnd.random() < 0.6 else 0)
    s.nput(mesh, "mesh", "mF30", s.nat("points", 0x10) if rnd.random() < 0.5 else 0)
    s.nput(mesh, "mesh", "mEngineMeshes", rnd.randrange(-5, 50))
    s.nput(mesh, "mesh", "mF38", tuple(rnd.getrandbits(8) for _ in range(8)))
    s.native_view("mesh", "mesh", "mesh", 1)
    # the other mesh the record may name is left poisoned: nothing reads it
    return c


def dump_views(ctx):
    """Every record the case built, read back."""
    s = ctx.side
    out = {}
    for label, address, spec, count in ctx.views:
        if spec.startswith("native:"):
            out[label] = [s.nget(address, spec[7:], i) for i in range(count)]
        else:
            size = SPECS[spec][1]
            out[label] = [s.get(address + i * size, spec) for i in range(count)]
    return out


# ------------------------------------------------------------------ the ride set of the small functions


def build_set(ctx, c, rnd):
    """A ride set (the game's record) whose mesh is the case's: the words nothing here reads are random."""
    s = ctx.side
    s.new("set", sizes(s, "set"))
    base = s.base("set")
    s.nput(base, "set", "mSelf", s.nat("set"))
    s.nput(base, "set", "mFlags", word(rnd))
    s.nput(base, "set", "mHeads", s.nat("mesh"))
    s.nput(base, "set", "mF0C", rnd.getrandbits(16))
    s.nput(base, "set", "mTimeCount", 0)
    s.nput(base, "set", "mTimeControls", 0)
    s.nput(base, "set", "mF14", s.nat("engine") if rnd.random() < 0.5 else 0)
    s.nput(base, "set", "mAnimationCount", 0)
    s.nput(base, "set", "mAnimations", 0)
    s.nput(base, "set", "mDynamicMeshSet", 0)
    for field in ("mFC0", "mFC4", "mFC8", "mFCC", "mFE4", "mFF8"):
        s.nput(base, "set", field, word(rnd))
    for field in ("mFD4", "mFE8", "mFEC", "mFF0", "mFF4"):
        s.nput(base, "set", field, rnd.randrange(-50, 50))
    for field in ("mFD0", "mEntryAllocation", "mAnimationHeaders", "mInstanceSelf"):
        s.nput(base, "set", field, 0)
    for k in range(12):
        s.nput(base, "set", f"g{k}.mFlags", rnd.getrandbits(3))
        s.nput(base, "set", f"g{k}.mCount", 0)
        s.nput(base, "set", f"g{k}.mEntries", 0)
    ctx.native_view("set", "set", "set", 1)


# ------------------------------------------------------------------ the small functions


def run_small(side, c):
    """DisableDataNodesRender, DisableDataNodesRenderExcept, SetupRideHeads, CorrectFaceFlags1,
    CorrectNodeFlags2 and OverrideActiveChannels on the case's mesh (or set)."""
    ctx = Ctx(side)
    set_globals(side, c["options"], c["heap"], c.get("poly"))
    if c["below"]:
        side.reserve(0x4000)
    build_mesh(ctx, c)
    key = c["key"]
    rnd = random.Random(c["seed"] ^ 0x5555)
    mesh = side.base("mesh")
    if key in ("node_flags", "channels"):
        build_set(ctx, c, rnd)
        args = (side.base("set"),)
    elif key in ("disable", "except", "heads"):
        if side.is_ppc:
            cell = side.new("cell", 4)
            side.w(cell, "I", mesh)
            arg = cell
        else:
            arg = mesh
        args = (arg, c["mask"]) if key != "heads" else (arg,)
    else:
        args = (mesh, c["all"])
    call(side, key, *args)
    out = dict(dump=dump_views(ctx), poly=poly_count(side), log=finish(side))
    return out


def small_random(key):
    def make(rnd):
        c = mesh_params(rnd, key=key)
        c["mask"] = rnd.choice([0, 1, 2, 3, 4, 5, 0x10, 0x21, 0xFF, rnd.getrandbits(8), 0xFFFFFFFF])
        c["all"] = rnd.choice([0, 1, -1, 7, 0x80000000])
        c["poly"] = rnd.choice([0, 1, 5, 100, 0x7FFFFFFF, 0xFFFFFFFF, rnd.getrandbits(8)])
        c["no_eset"] = key != "heads" and rnd.random() < 0.2        # no embedded set, though there are entries
        return c
    return make


def small_edges(key):
    def edges():
        rnd = random.Random(7)
        out = []
        for n, m, e in ((0, 0, 0), (1, 0, 0), (1, 0, 1), (2, 1, 3), (3, 2, 4), (0, 2, 2), (4, 3, 4)):
            for mask in (0, 1, 0xFFFFFFFF):
                for below in (0, 1):
                    c = mesh_params(rnd, key=key, n=n, m=m, e=e, below=below)
                    c["mask"], c["all"], c["poly"] = mask, mask & 1, 0
                    out.append(c)
        if key == "face_flags":     # each bit of the mask 0x580f00 alone, with and without the "all" argument
            for bit in (0x100, 0x200, 0x400, 0x800, 0x80000, 0x100000, 0x400000, 0x1000, 0x2, 0x8000000):
                for all_ in (0, 1):
                    c = mesh_params(rnd, key=key, n=2, m=1, e=3, emb_flags=[bit, bit | 2])
                    c["mask"], c["all"], c["poly"] = 0, all_, 0
                    out.append(c)
        for _ in range(150):
            out.append(small_random(key)(rnd))
        return out
    return edges


def make_test(name, runner, edge, rnd):
    def ppc(e, case):
        side = begin_ppc(e, case)
        try:
            return runner(side, case)
        finally:
            end_ppc(side)

    def host(lib, case):
        side = begin_host(lib, case)
        try:
            return runner(side, case)
        finally:
            end_host(side)

    return FnTest(name, ppc, host, edge=edge, rnd=rnd)


def setup(runner):
    T.install_std_stubs(runner.emu)

# ------------------------------------------------------------------ GetFontPath and ERRORBox


def run_font(side, c):
    """GetFontPath over the game's local path `c["path"]` (None: no path)."""
    ctx = Ctx(side)
    path_pointer = 0
    if c["path"] is not None:
        data = c["path"].encode("latin-1") + b"\0"
        text = side.new("path", len(data))
        if side.is_ppc:
            side.e.write(text, data)
        else:
            C.memmove(text, data, len(data))
        path_pointer = text
    if side.is_ppc:
        side.w(side.path_object + 8, "I", path_pointer)
    else:
        side.lib.tpw_ri_set_local_path.argtypes = [C.c_void_p]
        side.lib.tpw_ri_set_local_path(path_pointer or None)
    out = side.new("out", 256)
    ret = call(side, "font", out)
    written = bytes(side.r(out, "256B"))
    return dict(ret=ret & MASK, out=written, log=finish(side))


def font_edges():
    for path in (None, "", "a", "disc:", "disc", "Theme Park Data:", "a:b", ":", "x" * 100, "x" * 100 + ":"):
        yield dict(path=path)


def font_random(rnd):
    if rnd.random() < 0.1:
        return dict(path=None)
    head = rnd.choice(["", "TPW:Theme Park Data:", "TPW:Theme Park Data", "C:\\games\\tpw", "x:"])
    return dict(path=head + "".join(rnd.choice("ab:c Z") for _ in range(rnd.randrange(0, 20))))


FORMATS = ["Ride %s has relative animation incorrectly set\nStripping flag, animation will not work!\n",
           "Ridename texture generator: Could not find local path\n", "%s", "%d", "%s %d", "%d %s", "100%% %s", "", "%5d|%-6s|"]


def run_error_box(side, c):
    """ERRORBox. The original is an empty function (it only spills its argument registers), so the PowerPC
    side logs nothing and calls nothing; the port writes the formatted text through UDebug. The test
    states that difference: the PowerPC result is what the port is meant to log, after checking that the
    original did nothing at all."""
    fmt, text, number = c["format"], c["text"], c["number"]
    f = side.new("fmt", 200)
    t = side.new("text", 200)
    for address, string in ((f, fmt), (t, text)):
        data = string.encode("latin-1") + b"\0"
        if side.is_ppc:
            side.e.write(address, data)
        else:
            C.memmove(address, data, len(data))
    if side.is_ppc:
        T.call_ppc(side.e, ERRORBOX, [f, t, number & MASK])
        if side.e.calls != [] or len(side.log.log) != 0:
            return dict(log=[("the original did something", tuple(side.e.calls), ())])
        order = re_conversions(fmt)
        expected = "ERRORBox: " + (fmt % tuple(text if o == "s" else number for o in order))
        return dict(log=[("UDebug", (), (expected,))])
    order = re_conversions(fmt)
    if order[:1] == ["d"]:
        side.lib.tpw_ri_error_box_number_first.argtypes = [C.c_char_p, C.c_int32, C.c_char_p]
        side.lib.tpw_ri_error_box_number_first(fmt.encode("latin-1"), number, text.encode("latin-1"))
    else:
        side.lib.tpw_ri_error_box_text_first.argtypes = [C.c_char_p, C.c_char_p, C.c_int32]
        side.lib.tpw_ri_error_box_text_first(fmt.encode("latin-1"), text.encode("latin-1"), number)
    return dict(log=finish(side))


def re_conversions(fmt):
    import re
    return re.findall(r"%[-0-9]*([sd])", fmt)


def error_edges():
    for fmt in FORMATS:
        yield dict(format=fmt, text="Test Ride", number=42)


def error_random(rnd):
    return dict(format=rnd.choice(FORMATS), text="".join(rnd.choice("abcXYZ 09:") for _ in range(rnd.randrange(0, 30))),
                number=rnd.choice([0, 1, -1, 12345, -987654, rnd.randrange(-2**31, 2**31)]))


# ------------------------------------------------------------------ InstanceAnim


def anim_params(rnd, **fixed):
    c = dict(seed=rnd.getrandbits(32), na=rnd.choice([0, 1, 1, 2, 3, 4]), below=rnd.randrange(2),
             misalign=rnd.choice([0, 0, 4]), heap=rnd.choice([0, 0x77]), flags=rnd.getrandbits(32))
    c.update(fixed)
    return c


def build_anim(ctx, c, prefix=""):
    """An animation header with c["na"] node animations, some with a morph set and its key data."""
    s = Scope(ctx, prefix)
    rnd = random.Random(c["seed"])
    na = c["na"]
    s.new("anim", 0x48)
    s.new("nodeanims", max(na, 1) * 0x40)
    s.new("misc", 0x80)
    for i in range(na):
        s.new(f"morph{i}", 0x60)
        s.new(f"keys{i}", 8 * 0x14)
    s.put(s.base("misc"), "word", {})
    for q in range(0, 0x80, 4):
        s.w(s.base("misc") + q, "I", word(rnd))
    for i in range(na):
        flags = rnd.choice([0, 0x1000, 0x1000, 0x1000, 0x4000, 0x1000 | 0x4000, 0x2000, 0x5000 ^ 0x1000])
        flags |= rnd.getrandbits(32) & ~0x5000 & 0x0FFFFFFF
        has_morph = (flags & 0x1000) != 0 and (flags & 0x4000) == 0
        count = rnd.randrange(0, 7)
        v = {0: word(rnd), 4: flags, 8: word(rnd), 0xC: word(rnd), 0x10: rnd.getrandbits(16), 0x12: rnd.getrandbits(16),
             0x14: rnd.getrandbits(16), 0x16: rnd.getrandbits(16), 0x38: word(rnd), 0x3C: 0}
        for off in range(0x18, 0x38, 4):
            v[off] = ("misc", rnd.randrange(0, 0x70, 4)) if rnd.random() < 0.4 else 0
        v[0x28] = (f"morph{i}", 0) if has_morph or rnd.random() < 0.3 else ("misc", 0x20)
        s.put(s.base("nodeanims") + i * 0x40, "nodeanim", v)
        m = {0: rnd.getrandbits(16), 2: count, 4: word(rnd), 8: word(rnd),
             0xC: (f"keys{i}", 0) if rnd.random() < 0.85 else 0, 0x10: ("misc", 0x10)}
        for off in range(0x14, 0x34, 4):
            m[off] = word(rnd)
        s.put(s.base(f"morph{i}"), "morph", m)
        for q in range(count):
            s.put(s.base(f"keys{i}") + q * 0x14, "keydata", {0: word(rnd), 4: ("misc", 0), 8: 0, 0xC: ("misc", 4),
                                                              0x10: word(rnd)})
        s.view(f"morph{i}", f"morph{i}", "morph", 1)
        s.view(f"keys{i}", f"keys{i}", "keydata", count)
    ctx.facts[prefix + "keys_present"] = [s.r(s.base(f"morph{i}") + 0xC, "I")[0] != 0 for i in range(na)]
    h = {0: word(rnd), 4: word(rnd), 8: word(rnd), 0x12: na, 0x1E: rnd.getrandbits(16)}
    for off in (0xC, 0xE, 0x10, 0x14, 0x16, 0x18, 0x1A, 0x1C):
        h[off] = rnd.getrandbits(16)
    for off in range(0x20, 0x48, 4):
        h[off] = ("misc", rnd.randrange(0, 0x70, 4)) if rnd.random() < 0.5 else 0
    h[0x2C] = ("nodeanims", 0) if na or rnd.random() < 0.5 else 0
    s.put(s.base("anim"), "anim", h)
    s.view("anim", "anim", "anim", 1)
    s.view("nodeanims", "nodeanims", "nodeanim", na)
    s.view("misc", "misc", "word", 32)
    ctx.facts[prefix + "anim_ref"] = (prefix + "anim", 0)
    ctx.facts[prefix + "anim_address"] = s.base("anim")


def anim_block(side, index, na):
    """Names the pieces of the block the allocator gave InstanceAnim."""
    side.blocks[index][2] = [(0, 0x48, "hdr", 1), (0x48, 0x48 + na * 0x40, "nodeanims", 0x40), (0x48 + na * 0x40, 1 << 20, "tail", 1)]


def dump_new_anim(ctx, header, block, label, prefix=""):
    """The instance of an animation header at `header` (an address), read by what its header says."""
    s = ctx.side
    start, size, _ = block
    inside = lambda a: start <= a < start + size
    out = {label: [s.get(header, "anim")]}
    nodeanims = s.r(header + 0x2C, KINDFMT["P"])[0]
    count = s.r(header + 0x12, "H")[0]
    if nodeanims and inside(s.a32(nodeanims)):
        base = s.a32(nodeanims)
        out[label + ".nodeanims"] = [s.get(base + i * 0x40, "nodeanim") for i in range(count)]
        for i in range(count):
            morph = s.a32(s.r(base + i * 0x40 + 0x28, "I")[0])
            if morph and inside(morph):
                out[f"{label}.morph{i}"] = [s.get(morph, ctx.facts.get(prefix + "morph_spec", "morph"))]
                kd = s.a32(s.r(morph + 0xC, "I")[0])
                copied = ctx.facts[prefix + "keys_present"][i]      # a missing original has nothing copied
                if kd and inside(kd) and copied:
                    n = s.r(morph + 2, "H")[0]
                    out[f"{label}.keys{i}"] = [s.get(kd + q * 0x14, "keydata") for q in range(n)]
    return out


def build_anim_of(ctx, c, prefix=""):
    if "real" in c:
        build_real_anim(ctx, c, prefix)
    else:
        build_anim(ctx, c, prefix)


def run_anim(side, c):
    ctx = Ctx(side)
    set_globals(side, 0, c["heap"])
    if c["below"]:
        side.reserve(0x2000 + (3 * len(Image(c["real"]).d) if "real" in c else 0))
    build_anim_of(ctx, c)
    cell = side.new("cell", side.psize)
    ret = call(side, "anim", ctx.facts["anim_address"], c["heap"], c["flags"], cell)
    read_blocks(side)
    anim_block(side, 0, c["na"])
    out_pointer = side.r(cell, side.pfmt)[0]
    out = dict(ret=ret & MASK, out=side.labelp(out_pointer), dump=dump_views(ctx))
    out["dump"].update(dump_new_anim(ctx, out_pointer, side.blocks[0], "new"))
    out["guards"] = guards(side, c)
    out["log"] = finish(side)
    return out


def anim_edges():
    rnd = random.Random(11)
    for na in range(0, 5):
        for below in (0, 1):
            yield anim_params(rnd, na=na, below=below)


def anim_random(rnd):
    return anim_params(rnd)



# ------------------------------------------------------------------ InstanceMesh

MESH_FLAGS = (1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000)


def instance_params(rnd, **fixed):
    flags = 0
    for bit in MESH_FLAGS:
        if rnd.random() < 0.45:
            flags |= bit
    c = mesh_params(rnd, flags=flags, mask=rnd.choice([0, 1, 2, 3, 4, 8, 0x10, 0x21, 0xFF, rnd.getrandbits(8),
                                                       0xFFFFFFFF]))
    c.update(fixed)
    c["e"] = min(c["e"], c["n"] + c["m"])
    return c


def reserve_size(c):
    """Room for the block of an instance of the case's mesh (a real file is copied whole at most)."""
    return 0x4000 + (3 * len(Image(c["real"]).d) if "real" in c else 0)


def build_mesh_of(ctx, c, prefix=""):
    if "real" in c:
        build_real_mesh(ctx, c, prefix)
    else:
        build_mesh(ctx, c, prefix)


def mesh_regions(side, index, c, copy):
    """Names the pieces of the block InstanceMesh was given (allocation `index`): the pad before the aligned
    start, the game's record, the header copy, the data, and the embedded set with its records and blocks."""
    base, size = side.blocks[index][0], side.blocks[index][1]
    pad = align8(base) - base
    hdr = pad + sizes(side, "mesh")
    data = hdr + 0xB8
    end = size + MARGIN
    regions = [(0, pad, "pad", 1), (pad, hdr, "mesh", 1), (hdr, data, "hdr", 1)]
    embedded = side.nraw(copy, "mesh", "mF28")
    rec = sizes(side, "erec")
    if c["e"] and base <= embedded < base + size:
        e_rel = embedded - base
        records = side.nraw(embedded, "eset", "mRecords") - base
        blocks = side.nraw(embedded, "eset", "mBlocks") - base
        regions.append((data, e_rel, "data", 1))
        regions.append((e_rel, e_rel + sizes(side, "eset"), "eset", 1))
        regions.append((records, records + c["e"] * rec, "erecs", rec))
        regions.append((blocks, blocks + c["k"] * 0x40, "eblocks", 0x40))
        regions.append((blocks + c["k"] * 0x40, end, "beyond", 1))
    else:
        regions.append((data, end, "data", 1))
    side.blocks[index][2] = regions


def dump_new_mesh(ctx, copy, index, c, label, prefix=""):
    """The instance of a mesh whose game record is at `copy` (an address), read by what its header says."""
    s = ctx.side
    base, size = s.blocks[index][0], s.blocks[index][1]
    inside = lambda a: base <= a < base + size
    hdr = s.nraw(copy, "mesh", "mHeader")
    out = {label + ".mesh": [s.nget(copy, "mesh")], label + ".hdr": [s.get(hdr, "header")]}
    h = lambda off: s.raw(hdr, "header", off)
    count = lambda off: h(off)
    tables = (("nodes", 0x70, "node", count(0x44)), ("minis", 0x74, "mini", max(count(0x42) - count(0x44), 0)),
              ("vsets", 0x58, "vset", count(0x38)), ("points", 0x5C, "point", count(0x4A)),
              ("faces", 0x68, "face", count(0x3E)), ("fsets", 0x64, "fset", count(0x3C)),
              ("uvs", 0x60, "uv", count(0x3A)), ("tex", 0x50, "tex", count(0x36)), ("tinfo", 0x54, "tinfo", count(0x36)))
    seen = set()
    src_hdr = ctx.facts[prefix + "src_header"]
    for name, off, spec, n in tables:
        a = s.a32(h(off))
        if a and inside(a) and s.raw(src_hdr, "header", off) != 0:      # nothing is copied from a missing table
            out[f"{label}.{name}"] = [s.get(a + q * SPECS[spec][1], spec) for q in range(n)]
            seen.add(a)
    nodes = s.a32(h(0x70))
    if not (nodes and inside(nodes)):
        a = s.a32(h(0x78))
        if a and inside(a):
            spec = "mini" if s.raw(a, "node", 0) & 0x200 else "node"
            out[f"{label}.node78"] = [s.get(a, spec)]
    if nodes and inside(nodes):
        for i in range(count(0x44)):
            node = nodes + i * 0xA0
            uv = s.a32(s.raw(node, "node", 0x68))
            per_node = c["flags"] & 0x90 == 0x10      # each node with UV sets of its own gets a copy (flag 0x80: all share)
            if uv and inside(uv) and uv not in seen and per_node and ctx.facts.get(prefix + "nuv_present", [True] * 99)[i]:
                seen.add(uv)
                sets = (s.raw(node, "node", 0x5E) >> 2) + 1
                out[f"{label}.nuv{i}"] = [s.get(uv + q * 0x20, "uv") for q in range(sets)]
    infos = s.a32(h(0x54))
    if infos and inside(infos):
        for i in range(count(0x36)):
            name = s.a32(s.raw(infos + i * 0x10, "tinfo", 0xC))
            copied = ctx.facts[prefix + "info_names"][i]     # nothing is copied from a missing name
            if name and inside(name) and copied:
                out[f"{label}.name{i}"] = [s.get(name, "name")]
    embedded = s.nraw(copy, "mesh", "mF28")
    if c["e"] and inside(embedded):
        out[label + ".eset"] = [s.nget(embedded, "eset")]
        records_present, blocks_present = ctx.facts[prefix + "eset_present"]
        records = s.nraw(embedded, "eset", "mRecords")
        if inside(records) and records_present:
            out[label + ".erecs"] = [s.nget(records, "erec", i) for i in range(c["e"])]
        blocks = s.nraw(embedded, "eset", "mBlocks")
        if c["k"] and inside(blocks) and blocks_present:
            out[label + ".eblocks"] = [s.get(blocks + q * 0x40, "block") for q in range(1)]
    return out


def mesh_sizes(side, c, index=0):
    """The host's allocation size for InstanceMesh as the Mac's: the game's record, and the embedded set and
    its records (larger on the host), less the eight bytes of room the host leaves for their alignment."""
    if side.is_ppc:
        return
    msize, rec, eset = sizes(side, "mesh"), sizes(side, "erec"), sizes(side, "eset")
    emb = (c["e"] * (rec - 0x14) + (eset - 0x14) + 8) if c["e"] else 0
    side.adjust[index] = lambda size: size - (msize - 0x40) - emb


def run_mesh(side, c):
    ctx = Ctx(side)
    set_globals(side, c["options"], c["heap"])
    mesh_sizes(side, c)
    if c["below"]:
        side.reserve(reserve_size(c))
    build_mesh_of(ctx, c)
    cell = side.new("cell", side.psize)
    ret = call(side, "mesh", side.base("mesh"), c["heap"], c["flags"], cell, c["mask"])
    read_blocks(side)
    copy = side.r(cell, side.pfmt)[0]
    mesh_regions(side, 0, c, copy)
    out = dict(ret=ret & MASK, out=side.labelp(copy), dump=dump_views(ctx))
    out["dump"].update(dump_new_mesh(ctx, copy, 0, c, "new"))
    out["guards"] = guards(side, c)
    out["log"] = finish(side)
    return out


def instance_edges():
    rnd = random.Random(21)
    base = dict(n=2, m=1, e=2, k=2, t=3, s=3, v=3, p=3, f=3, u=3, hflags=0)
    for flags in (0,) + MESH_FLAGS + (0x1F, 0x9F, 0x13F, 0x3FF, 0x3FFF):
        for below in (0, 1):
            yield instance_params(rnd, flags=flags, below=below, mask=3, options=0, **base)
    for hflags in (0x1000, 8, 0x8000, 0x10000, 0x18000, 8 | 0x10000, 0x1000 | 0x8000):
        for options in (0, 0x2000):
            for flags in (0, 1, 0x1F):
                yield instance_params(rnd, flags=flags, hflags=hflags, options=options, **{k: v for k, v in base.items()
                                                                                         if k != "hflags"})
    yield instance_params(rnd, flags=0, n=0, m=0, e=0)
    yield instance_params(rnd, flags=1, n=0, m=0, e=0)
    yield instance_params(rnd, flags=0x3FF, n=1, m=0, e=0, t=0, s=0, v=0, p=0, f=0, u=0)
    for _ in range(300):        # more of the random kind, from a fixed seed (the runner adds 200 of its own)
        yield instance_params(rnd)


def instance_random(rnd):
    return instance_params(rnd)



# ------------------------------------------------------------------ InstanceDynamicMeshSet


def dmesh_params(rnd, **fixed):
    c = instance_params(rnd)
    c.update(dcount=rnd.choice([0, 1, 2, 3]), shared=rnd.choice([0, 1, 5, 0xFFFFFFFF]))
    c.update(fixed)
    c["n"] = max(c["n"], c["dcount"] + 1)      # each entry takes a node of the new mesh (the first is left out)
    c["e"] = min(c["e"], c["n"] + c["m"])
    return c


def build_dmesh(ctx, c, prefix=""):
    """A dynamic mesh set (the game's record, `dcount` entries, a shared record) over a mesh of its own."""
    s = Scope(ctx, prefix)
    side = ctx.side
    rnd = random.Random(c["seed"] ^ 0xD35)
    mesh = prefix + "m."
    build_mesh(ctx, c, mesh)
    count = c["dcount"]
    entry = sizes(side, "dentry")
    s.new("dset", sizes(side, "dmesh"))
    s.new("dents", max(count, 1) * entry, entry)
    s.new("shared", sizes(side, "shared"))
    d = s.base("dset")
    side.nput(d, "dmesh", "mF00", word(rnd))
    side.nput(d, "dmesh", "mCount", count)
    side.nput(d, "dmesh", "mF06", rnd.getrandbits(16))
    side.nput(d, "dmesh", "mEntries", s.nat("dents"))
    side.nput(d, "dmesh", "mF0C", word(rnd))
    for field, table in (("mVertexSets", "vsets"), ("mPoints", "points"), ("mUVSets", "uvs"), ("mFaces", "faces")):
        side.nput(d, "dmesh", field, side.nat(mesh + table))
    side.nput(d, "dmesh", "mF20", word(rnd))
    side.nput(d, "dmesh", "mMesh", side.nat(mesh + "mesh"))
    for field in ("mF28", "mF2C", "mF30", "mF38"):
        side.nput(d, "dmesh", field, word(rnd))
    side.nput(d, "dmesh", "mAllocation", side.nat(mesh + "engine"))
    side.nput(d, "dmesh", "mShared", s.nat("shared"))
    for i in range(count):
        side.nput(s.base("dents"), "dentry", "mF00", tuple(word(rnd) for _ in range(10)), i)
        side.nput(s.base("dents"), "dentry", "mMatrix", side.nat(mesh + "engine"), i)
        side.nput(s.base("dents"), "dentry", "mFaceSets", side.nat(mesh + "engine"), i)
    side.nput(s.base("shared"), "shared", "mF00", tuple(word(rnd) for _ in range(7)))
    side.nput(s.base("shared"), "shared", "mInstances", c["shared"])
    s.native_view("dset", "dset", "dmesh", 1)
    s.native_view("dents", "dents", "dentry", count)
    s.native_view("shared", "shared", "shared", 1)


def dmesh_adjust(side, c, index):
    """The host's size for the dynamic set's block as the Mac's (0x44 and 0x30 for each entry)."""
    if side.is_ppc:
        return
    dm, entry = sizes(side, "dmesh"), sizes(side, "dentry")
    side.adjust[index] = lambda size: size - c["dcount"] * (entry - 0x30) - (dm - 0x40)


def dmesh_regions(side, index, c, copy):
    base, size = side.blocks[index][0], side.blocks[index][1]
    pad = align8(base) - base
    dm, entry = sizes(side, "dmesh"), sizes(side, "dentry")
    side.blocks[index][2] = [(0, pad, "pad", 1), (pad, pad + dm, "set", 1),
                             (pad + dm, pad + dm + c["dcount"] * entry, "entries", entry),
                             (pad + dm + c["dcount"] * entry, size + MARGIN, "tail", 1)]


def regions_dmesh(side, copy, index, mesh_index, c):
    """Names the pieces of the dynamic set's block and of its mesh's block."""
    dmesh_regions(side, index, c, copy)
    mesh_regions(side, mesh_index, c, side.nraw(copy, "dmesh", "mMesh"))


def dump_new_dmesh(ctx, copy, index, mesh_index, c, label, prefix=""):
    """The instance of a dynamic mesh set (regions_dmesh is done)."""
    s = ctx.side
    out = {label: [s.nget(copy, "dmesh")]}
    entries = s.nraw(copy, "dmesh", "mEntries")
    out[label + ".entries"] = [s.nget(entries, "dentry", i) for i in range(c["dcount"])]
    out.update(dump_new_mesh(ctx, s.nraw(copy, "dmesh", "mMesh"), mesh_index, c, label + ".mesh", prefix + "m."))
    return out


def run_dmesh(side, c):
    ctx = Ctx(side)
    set_globals(side, c["options"], c["heap"])
    dmesh_adjust(side, c, 0)
    mesh_sizes(side, c, 1)
    if c["below"]:
        side.reserve(0x400)
        side.reserve(0x4000)
    build_dmesh(ctx, c)
    cell = side.new("cell", side.psize)
    ret = call(side, "dmesh", side.base("dset"), c["heap"], c["flags"], cell)
    read_blocks(side)
    copy = side.r(cell, side.pfmt)[0]
    regions_dmesh(side, copy, 0, 1, c)
    out = dict(ret=ret & MASK, out=side.labelp(copy), dump=dump_views(ctx))
    out["dump"].update(dump_new_dmesh(ctx, copy, 0, 1, c, "new", ""))
    out["guards"] = guards(side, c)
    out["log"] = finish(side)
    return out


def dmesh_edges():
    rnd = random.Random(31)
    for count in (0, 1, 2, 3):
        for below in (0, 1):
            for flags in (0x21DF, 0x1F, 0, 0x13F, 0x1000 | 0x1F):
                yield dmesh_params(rnd, dcount=count, below=below, flags=flags, mask=0)
    for _ in range(100):
        yield dmesh_params(rnd)


def dmesh_random(rnd):
    return dmesh_params(rnd)


# ------------------------------------------------------------------ InstanceRideAnimSet


def set_params(rnd, **fixed):
    c = instance_params(rnd, misalign=0)
    anims = fixed.get("a", rnd.choice([0, 1, 2, 3, 4]))
    c.update(tc=rnd.choice([0, 1, 2, 3]), a=anims, dyn=rnd.random() < 0.4, misalign=0)
    c["dc"] = dmesh_params(rnd, flags=0x21DF, mask=0, misalign=0)
    c["dc"]["options"] = c["options"]
    c["ac"] = [anim_params(rnd, misalign=0) for _ in range(anims)]
    c["present"] = [rnd.random() < 0.85 for _ in range(anims)]
    c["groups"] = [rnd.randrange(12) if rnd.random() < 0.92 else None for _ in range(anims)]
    c.update(fixed)
    c["dc"]["fill"] = c["fill"]
    return c


def build_ride_set(ctx, c):
    """A ride set: the case's mesh, maybe a dynamic mesh set, animations (some missing), time controls and
    the groups' entries."""
    side = ctx.side
    s = Scope(ctx, "")
    rnd = random.Random(c["seed"] ^ 0x5E7)
    psize = side.psize
    entry = sizes(side, "entry")
    build_mesh_of(ctx, c)
    if c["dyn"]:
        build_dmesh(ctx, c["dc"], "d.")
    a, tc = c["a"], c["tc"]
    for i in range(a):
        if c["present"][i]:
            build_anim_of(ctx, c["ac"][i], f"a{i}.")
    side.new("ctl", max(tc, 1) * 0x38)
    for q in range(tc * 14):
        side.w(side.base("ctl") + q * 4, "I", word(rnd))
    s.view("ctl", "ctl", "word", tc * 14)
    side.new("animptrs", max(a, 1) * psize, psize)
    for i in range(a):
        side.w(side.base("animptrs") + i * psize, side.pfmt, side.nat(*ctx.facts[f"a{i}.anim_ref"]) if c["present"][i] else 0)
    s.native_view("animptrs", "animptrs", "entry", 0)
    base_ = side.new("set", sizes(side, "set"))
    members = {g: [i for i in range(a) if c["groups"][i] == g] for g in range(12)}
    for g, indexes in members.items():
        if indexes:
            side.new(f"ents{g}", len(indexes) * entry, entry)
            for j, i in enumerate(indexes):
                side.nput(side.base(f"ents{g}"), "entry", "mIndex", i, j)
                side.nput(side.base(f"ents{g}"), "entry", "mAnimation",
                          side.nat(*ctx.facts[f"a{i}.anim_ref"]) if c["present"][i] else 0, j)
            s.native_view(f"ents{g}", f"ents{g}", "entry", len(indexes))
    side.nput(base_, "set", "mSelf", side.nat("set"))
    side.nput(base_, "set", "mFlags", word(rnd))
    side.nput(base_, "set", "mHeads", side.nat("mesh"))
    side.nput(base_, "set", "mF0C", rnd.getrandbits(16))
    side.nput(base_, "set", "mTimeCount", tc)
    side.nput(base_, "set", "mTimeControls", side.nat("ctl") if tc or rnd.random() < 0.5 else 0)
    side.nput(base_, "set", "mF14", side.nat("engine") if rnd.random() < 0.5 else 0)
    side.nput(base_, "set", "mAnimationCount", a)
    side.nput(base_, "set", "mAnimations", side.nat("animptrs") if a or rnd.random() < 0.5 else 0)
    side.nput(base_, "set", "mDynamicMeshSet", side.nat("d.dset") if c["dyn"] else 0)
    for field in ("mFC0", "mFC4", "mFC8", "mFCC", "mFE4", "mFF8"):
        side.nput(base_, "set", field, word(rnd))
    for field in ("mFD4", "mFE8", "mFEC", "mFF0", "mFF4"):
        side.nput(base_, "set", field, rnd.randrange(-50, 50))
    for field in ("mFD0", "mEntryAllocation", "mAnimationHeaders", "mInstanceSelf"):
        side.nput(base_, "set", field, side.nat("engine") if rnd.random() < 0.3 else 0)
    missing = c.get("fill") == 0 and rnd.random() < 0.4       # a group with entries counted but no array: zeros are copied
    for g in range(12):
        side.nput(base_, "set", f"g{g}.mFlags", rnd.getrandbits(3))
        side.nput(base_, "set", f"g{g}.mCount", len(members[g]))
        side.nput(base_, "set", f"g{g}.mEntries", side.nat(f"ents{g}") if members[g] and not (missing and g == min(
            [x for x in members if members[x]] or [0])) else 0)
    s.native_view("set", "set", "set", 1)


def set_adjust(side, c, index):
    """The host's size for the set's block as the Mac's: the record (0xfc) and, for each animation, an
    animation pointer (4) and an entry (8) that are larger here."""
    if side.is_ppc:
        return
    setsize, entry = sizes(side, "set"), sizes(side, "entry")
    side.adjust[index] = lambda size: size - (setsize - 0xFC) - c["a"] * ((side.psize + entry) - 12)


def set_regions(side, index, c):
    base, size = side.blocks[index][0], side.blocks[index][1]
    setsize, entry = sizes(side, "set"), sizes(side, "entry")
    ctl = setsize
    anims = ctl + c["tc"] * 0x38
    entries = anims + c["a"] * side.psize
    side.blocks[index][2] = [(0, ctl, "set", 1), (ctl, anims, "ctl", 1), (anims, entries, "anims", side.psize),
                             (entries, entries + c["a"] * entry, "entries", entry),
                             (entries + c["a"] * entry, size + MARGIN, "tail", 1)]


def run_set(side, c):
    ctx = Ctx(side)
    set_globals(side, c["options"], c["heap"])
    dyn, a = c["dyn"], c["a"]
    mesh_i, dset_i, dmesh_i = 0, 1, 2
    set_i = 3 if dyn else 1
    anim_i = {}
    for i in range(a):
        if c["present"][i]:
            anim_i[i] = set_i + 1 + len(anim_i)
    mesh_sizes(side, c, mesh_i)
    if dyn:
        dmesh_adjust(side, c["dc"], dset_i)
        mesh_sizes(side, c["dc"], dmesh_i)
    set_adjust(side, c, set_i)
    if c["below"]:
        side.reserve(reserve_size(c))
    build_ride_set(ctx, c)
    cell = side.new("cell", side.psize)
    ret = call(side, "set", 0 if c.get("null_set") else side.base("set"), c["heap"], c["flags"], cell, c["mask"])
    read_blocks(side)
    copy = side.r(cell, side.pfmt)[0]
    if c.get("null_set"):        # no set: the answer is 0 and nothing else happens
        return dict(ret=ret & MASK, out=side.labelp(copy), dump=dump_views(ctx), log=finish(side))
    set_regions(side, set_i, c)
    mesh = side.nraw(copy, "set", "mHeads")
    mesh_regions(side, mesh_i, c, mesh)
    dset = side.nraw(copy, "set", "mDynamicMeshSet")
    if dyn:
        regions_dmesh(side, dset, dset_i, dmesh_i, c["dc"])
    animations = side.nraw(copy, "set", "mAnimations")
    new_anims = []
    if a and animations and animations - side.blocks[set_i][0] < side.blocks[set_i][1]:
        new_anims = [side.r(animations + i * side.psize, side.pfmt)[0] for i in range(a)]
    for i, index in anim_i.items():
        anim_block(side, index, c["ac"][i]["na"])
    out = dict(ret=ret & MASK, out=side.labelp(copy), dump=dump_views(ctx))
    dump = out["dump"]
    dump["new"] = [side.nget(copy, "set")]
    ctl = side.nraw(copy, "set", "mTimeControls")
    if c["tc"] and ctl:
        dump["new.ctl"] = [side.r(ctl + 4 * q, "I")[0] for q in range(c["tc"] * 14)]
    if new_anims:
        dump["new.anims"] = [side.labelp(v) for v in new_anims]
    for g in range(12):
        entries = side.nraw(copy, "set", f"g{g}.mEntries")
        count = side.nraw(copy, "set", f"g{g}.mCount")
        if entries and count and entries - side.blocks[set_i][0] < side.blocks[set_i][1]:
            dump[f"new.ents{g}"] = [side.nget(entries, "entry", j) for j in range(count)]
    dump.update(dump_new_mesh(ctx, mesh, mesh_i, c, "new.mesh"))
    if dyn:
        dump.update(dump_new_dmesh(ctx, dset, dset_i, dmesh_i, c["dc"], "new.dyn", "d."))
    for i, index in anim_i.items():
        if i < len(new_anims):
            dump.update(dump_new_anim(ctx, new_anims[i], side.blocks[index], f"new.anim{i}", f"a{i}."))
    out["guards"] = guards(side, c)
    out["log"] = finish(side)
    return out


def set_edges():
    rnd = random.Random(41)
    for a in (0, 1, 2, 4):
        for tc in (0, 1, 3):
            for dyn in (False, True):
                for flags in (0x2000, 0x800, 0x2800, 0x1F, 0):
                    yield set_params(rnd, a=a, tc=tc, dyn=dyn, flags=flags, below=0)
    for below in (0, 1):
        yield set_params(rnd, below=below)
    for _ in range(150):
        yield set_params(rnd)
    yield set_params(rnd, null_set=True)
    yield set_params(rnd, null_set=True, a=2, dyn=True)


def set_random(rnd):
    return set_params(rnd)



# ------------------------------------------------------------------ real mesh and animation files

WADS = pathlib.Path(__file__).resolve().parent.parent.parent / "extracted" / "wads"
GATE = "lobby.wad.d/terrain/Hal_gate"       # a small ride of the lobby: a mesh with a mini node, three animations
REAL_RIDES = [   # (mesh, its animations, the group they belong to): "CDILSMEUWBRO" numbers the groups
    (GATE + ".MD2", [GATE + f"M{i}.MD2" for i in (1, 2, 3)], 5),
    ("lobby.wad.d/terrain/Jun_gate.MD2", [f"lobby.wad.d/terrain/Jun_gateM{i}.MD2" for i in (1, 2, 3)], 5),
    ("lobby.wad.d/terrain/Spa_gate.MD2", ["lobby.wad.d/terrain/Spa_gateM1.MD2"], 5),
    ("lobby.wad.d/terrain/Fan_gate.MD2", [f"lobby.wad.d/terrain/Fan_gateM{i}.MD2" for i in (1, 2, 3)], 5),
    ("lobby.wad.d/terrain/Bat.MD2", ["lobby.wad.d/terrain/Batm1.MD2"], 5),
    ("lobby.wad.d/terrain/Bfly_YELL.MD2", ["lobby.wad.d/terrain/Bfly_YELLm1.MD2"], 5),
    ("lobby.wad.d/terrain/Fan_isle.MD2", [f"lobby.wad.d/terrain/Fan_isleM{i}.MD2" for i in (1, 2)], 5),
    ("levels/hallow/rides/haunt.wad.d/haunt.MD2", [], 0),
]
# The loader turns a file offset into an address for these fields; the others are left as read.
RELOCATE = {
    "header": {0x50, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C, 0x70, 0x74, 0x78, 0x7C, 0x98, 0xAC},
    "node": {4, 8, 0xC, 0x54, 0x60, 0x64, 0x68, 0x6C, 0x70, 0x94}, "mini": {4, 8, 0xC, 0x54}, "fset": {0}, "tinfo": {0xC},
    "emb": {0xC, 0x10}, "anim": {0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C}, "nodeanim": {0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34},
    "morph_real": {0xC, 0x10}, "keydata": {4, 8, 0xC},
}


class Image:
    """A mesh or animation file as the loader leaves it in memory (read from the disc's data at test time)."""

    def __init__(self, rel):
        self.rel = rel
        self.d = (WADS / rel).read_bytes()

    def u(self, offset, fmt):
        return struct.unpack_from("<" + fmt, self.d, offset)[0]

    def regions(self):
        """(offset, record type, count) of the records the translation reads, in file order."""
        typed = [(0, "header", 1)]
        flags = self.u(0x30, "I")
        if flags & 1:
            nodes, all_nodes = self.u(0x44, "H"), self.u(0x42, "H")
            typed += [(self.u(0x70, "I"), "node", nodes), (self.u(0x74, "I"), "mini", max(all_nodes - nodes, 0)),
                      (self.u(0x64, "I"), "fset", self.u(0x3C, "H")), (self.u(0x54, "I"), "tinfo", self.u(0x36, "H")),
                      (self.u(0x50, "I"), "tex", self.u(0x36, "H")), (self.u(0x7C, "I"), "emb", self.u(0x48, "H"))]
        anim = self.u(0x98, "I")
        if flags & 0x20 and anim:
            typed.append((anim, "anim", 1))
            nodeanims, count = self.u(anim + 0x2C, "I"), self.u(anim + 0x12, "H")
            typed.append((nodeanims, "nodeanim", count))
            for i in range(count):
                node_flags = self.u(nodeanims + i * 0x40 + 4, "I")
                morph = self.u(nodeanims + i * 0x40 + 0x28, "I")
                if node_flags & 0x1000 and not node_flags & 0x4000 and morph:
                    typed.append((morph, "morph_real", 1))
                    keys = self.u(morph + 0xC, "I")
                    if keys and self.u(morph + 2, "H"):
                        typed.append((keys, "keydata", self.u(morph + 2, "H")))
        typed = sorted(t for t in typed if t[0] and t[2])
        typed = [(0, "header", 1)] + [t for t in typed if t[0] != 0 or t[1] != "header"]
        out, position = [], 0
        for offset, spec, count in typed:
            if offset < position:
                continue        # records that overlap an earlier region are not read twice
            if offset > position:
                out += self.gap(position, offset)
            out.append((offset, spec, count))
            position = offset + count * SPECS[spec][1]
        if position < len(self.d):
            out += self.gap(position, len(self.d))
        return out

    @staticmethod
    def gap(begin, end):
        out = []
        if (end - begin) // 4:
            out.append((begin, "word", (end - begin) // 4))
        for q in range(begin + (end - begin) // 4 * 4, end):
            out.append((q, "byte", 1))
        return out


def put_image(ctx, name, image):
    """Allocates the file under `name` and writes its records as both sides hold them; registers them for the
    read-back. Returns the object's name."""
    s = ctx.side
    base = s.new(name, len(image.d) + 0x40)
    for offset, spec, count in image.regions():
        size = SPECS[spec][1]
        for i in range(count):
            at = offset + i * size
            if spec == "word":
                s.w(base + at, "I", image.u(at, "I"))
            elif spec == "byte":
                s.w(base + at, "B", image.d[at])
            else:
                values = {}
                for field, kind in SPECS[spec][0]:
                    v = image.u(at + field, {"I": "I", "H": "H", "B": "B", "P": "I"}[kind])
                    values[field] = (name, v) if kind == "P" and v != 0 and field in RELOCATE[spec] else v
                if spec == "header":
                    values.update({0x9C: (name, 0), 0xB0: (name, 0), 0xB4: 0, 0x4C: 0})
                if spec == "tinfo":
                    values[4] = 0
                s.put(base + at, spec, values)
        ctx.views.append((f"{name}@{offset:x}", base + offset, spec, count))
    return name


def build_real_natives(ctx, c, prefix, rnd, image):
    """The game's records of a mesh read from a file: the record, the embedded set, its records and blocks."""
    s = Scope(ctx, prefix)
    side = ctx.side
    e, k = image.u(0x48, "H"), c["k"]
    nodes = image.u(0x70, "I")
    s.new("engine", 0x40)
    s.new("othermesh", sizes(side, "mesh"))
    s.new("mesh", sizes(side, "mesh"))
    s.new("eset", sizes(side, "eset"))
    s.new("erecs", max(e, 1) * sizes(side, "erec"), sizes(side, "erec"))
    s.new("eblocks", max(k, 1) * 0x40)
    for q in range(k * 16):
        side.w(s.base("eblocks") + q * 4, "I", word(rnd))
    s.view("eblocks", "eblocks", "block", k)
    eset = s.base("eset")
    side.nput(eset, "eset", "mFlags", rnd.getrandbits(4))
    side.nput(eset, "eset", "mRecords", s.nat("erecs") if e else 0)
    side.nput(eset, "eset", "mF08", word(rnd))
    side.nput(eset, "eset", "mBlockCount", k)
    side.nput(eset, "eset", "mBlocks", s.nat("eblocks") if k else 0)
    for i in range(e):
        flag = rnd.random() < 0.5
        side.nput(s.base("erecs"), "erec", "mFlags", (1 if flag else 0) | rnd.getrandbits(8) & 0xE, i)
        side.nput(s.base("erecs"), "erec", "mBlock", s.nat("eblocks", rnd.randrange(max(k, 1)) * 0x40) if flag and k else 0, i)
        for f in ("mF08", "mF0C", "mF10"):
            side.nput(s.base("erecs"), "erec", f, word(rnd), i)
    mesh = s.base("mesh")
    side.nput(mesh, "mesh", "mF00", word(rnd) | rnd.choice([0, 1]))
    side.nput(mesh, "mesh", "mHeader", side.nat(prefix + "f"))
    side.nput(mesh, "mesh", "mF08", s.nat("othermesh") if rnd.random() < 0.5 else 0)
    side.nput(mesh, "mesh", "mF0C", word(rnd))
    side.nput(mesh, "mesh", "mF10", rnd.randrange(-100, 100))
    side.nput(mesh, "mesh", "mF14", tuple(rnd.getrandbits(8) for _ in range(8)))
    side.nput(mesh, "mesh", "mEngineMesh", s.nat("engine") if rnd.random() < 0.7 else 0)
    side.nput(mesh, "mesh", "mTypeEngineMesh", s.nat("engine") if rnd.random() < 0.7 else 0)
    side.nput(mesh, "mesh", "mAllocation", s.nat("engine") if rnd.random() < 0.5 else 0)
    side.nput(mesh, "mesh", "mF28", s.nat("eset") if e else 0)
    side.nput(mesh, "mesh", "mRootNode", side.nat(prefix + "f", nodes) if nodes else 0)
    side.nput(mesh, "mesh", "mF30", 0)
    side.nput(mesh, "mesh", "mEngineMeshes", rnd.randrange(-5, 50))
    side.nput(mesh, "mesh", "mF38", tuple(rnd.getrandbits(8) for _ in range(8)))
    s.native_view("mesh", "mesh", "mesh", 1)
    s.native_view("eset", "eset", "eset", 1)
    s.native_view("erecs", "erecs", "erec", e)


def build_real_mesh(ctx, c, prefix=""):
    """A mesh read from the file c["real"]: the file (named prefix + "f") and the game's records for it."""
    image = Image(c["real"])
    rnd = random.Random(c["seed"])
    put_image(ctx, prefix + "f", image)
    ctx.facts[prefix + "src_header"] = ctx.side.base(prefix + "f")
    ctx.facts[prefix + "eset_present"] = (True, True)
    build_real_natives(ctx, c, prefix, rnd, image)
    names = [image.u(image.u(0x54, "I") + i * 0x10 + 0xC, "I") != 0 for i in range(image.u(0x36, "H"))]
    ctx.facts[prefix + "info_names"] = names


def real_mesh_params(rnd, rel, **fixed):
    image = Image(rel)
    flags = 0
    for bit in MESH_FLAGS:
        if rnd.random() < 0.45:
            flags |= bit
    c = dict(seed=rnd.getrandbits(32), real=rel, e=image.u(0x48, "H"), k=rnd.choice([0, 1, 2, 3]), flags=flags,
             mask=rnd.choice([0, 1, 2, 3, 4, 8, 0xFF, 0xFFFFFFFF]), below=rnd.randrange(2), misalign=rnd.choice([0, 0, 4]),
             options=rnd.choice([0, 0x2000]), heap=rnd.choice([0, 0x1234]))
    c.update(fixed)
    c["k"] = max(c["k"], c["e"])      # a block for each record that may take one
    return c


def available(rel):
    """The file is there (and, for an animation, is one the loader would take: header flag 0x20)."""
    path = WADS / rel
    if not path.is_file():
        return False
    return rel.endswith(("M1.MD2", "M2.MD2", "M3.MD2", "m1.MD2")) is False or Image(rel).u(0x30, "I") & 0x20 != 0


def real_mesh_edges():
    rnd = random.Random(51)
    for mesh, _, _ in REAL_RIDES:
        if not available(mesh):
            continue
        for flags in (0, 0x1F, 0x211F, 0x231F, 0x13F, 0x3FF, 0x1000 | 0x211F):
            for below in (0, 1):
                yield real_mesh_params(rnd, mesh, flags=flags, below=below, mask=rnd.choice([0, 3]))
        for options in (0, 0x2000):
            yield real_mesh_params(rnd, mesh, flags=0, options=options)


def real_mesh_random(rnd):
    candidates = [r[0] for r in REAL_RIDES if available(r[0])]
    return real_mesh_params(rnd, rnd.choice(candidates)) if candidates else None


def build_real_anim(ctx, c, prefix=""):
    """An animation read from the file c["real"]: the file (named prefix + "f") and the header inside it."""
    image = Image(c["real"])
    put_image(ctx, prefix + "f", image)
    anim = image.u(0x98, "I")
    nodeanims, count = image.u(anim + 0x2C, "I"), image.u(anim + 0x12, "H")
    present = []
    for i in range(count):
        node_flags = image.u(nodeanims + i * 0x40 + 4, "I")
        morph = image.u(nodeanims + i * 0x40 + 0x28, "I")
        present.append(bool(node_flags & 0x1000 and not node_flags & 0x4000 and morph and image.u(morph + 0xC, "I")))
    ctx.facts[prefix + "keys_present"] = present
    ctx.facts[prefix + "morph_spec"] = "morph_real"
    ctx.facts[prefix + "anim_ref"] = (prefix + "f", anim)
    ctx.facts[prefix + "anim_address"] = ctx.side.base(prefix + "f") + anim


def real_anim_params(rnd, rel, **fixed):
    image = Image(rel)
    c = dict(seed=rnd.getrandbits(32), real=rel, na=image.u(image.u(0x98, "I") + 0x12, "H"), below=rnd.randrange(2),
             misalign=rnd.choice([0, 0, 4]), heap=rnd.choice([0, 0x77]), flags=rnd.getrandbits(32))
    c.update(fixed)
    return c


def real_anim_files():
    return sorted({a for _, anims, _ in REAL_RIDES for a in anims if available(a)})


def real_anim_edges():
    rnd = random.Random(61)
    for rel in real_anim_files():
        for below in (0, 1):
            yield real_anim_params(rnd, rel, below=below)


def real_anim_random(rnd):
    return real_anim_params(rnd, rnd.choice(real_anim_files()))


def real_set_params(rnd, ride, **fixed):
    mesh, anims, group = ride
    anims = [a for a in anims if available(a)]
    c = real_mesh_params(rnd, mesh, misalign=0)
    c.update(tc=rnd.choice([0, 1, 1, 2]), a=len(anims), dyn=False, misalign=0)
    c["ac"] = [real_anim_params(rnd, a, misalign=0) for a in anims]
    c["present"] = [True] * len(anims)
    c["groups"] = [group] * len(anims)
    if anims and rnd.random() < 0.2:
        c["present"][rnd.randrange(len(anims))] = False
    c.update(fixed)
    return c


def real_set_edges():
    rnd = random.Random(71)
    for ride in REAL_RIDES:
        if not available(ride[0]):
            continue
        for flags in (0x211F, 0x231F, 0x2800 | 0x211F, 0x2000, 0):
            for below in (0, 1):
                yield real_set_params(rnd, ride, flags=flags, below=below, mask=0)


def real_set_random(rnd):
    return real_set_params(rnd, rnd.choice([r for r in REAL_RIDES if available(r[0])]))



# ------------------------------------------------------------------ AlignTo32Bit

ENGINE = "engine symbols shared"
ALIGN32 = {"value": 0x10039120, "address": 0x10039174}      # AlignTo32Bit(unsigned long&, unsigned long*), (void*&, unsigned long*)


class AlignTest:
    """AlignTo32Bit, both overloads, against the engine's functions (the engine program's emulator, borrowed for
    the case: this module runs in the main app's)."""

    name = "AlignTo32Bit"

    def edge_cases(self):
        for form in ("value", "address"):
            for value in list(range(0, 10)) + [0x7FFFFFFC, 0x7FFFFFFD, 0xFFFFFFF0, 0xFFFFFFFC]:
                for moved in (None, 0, 5, 0xFFFFFFFE):
                    yield dict(form=form, value=value, moved=moved)

    def random_case(self, rnd):
        return dict(form=rnd.choice(["value", "address"]), value=rnd.choice([rnd.getrandbits(32) & 0xFFFFFFFC, rnd.getrandbits(32) & 0xFFFFFFFC | rnd.randrange(4), rnd.randrange(64)]),
                    moved=rnd.choice([None, 0, rnd.getrandbits(32)]))

    def run_case(self, runner, case):
        if ENGINE not in runner.emus:
            runner.use(ENGINE)
            runner.use(PROGRAM)
        e = runner.emus[ENGINE]
        e.reset_scratch()
        value, moved = case["value"], case["moved"]
        if case["form"] == "address" and value > 0xFFFFFFFC:
            value &= 0xFFFFFFFC      # the host's address does not wrap at 32 bits
        cell = e.alloc(4, struct.pack(">I", value))
        moved_cell = e.alloc(4, struct.pack(">I", moved or 0))
        T.call_ppc(e, ALIGN32[case["form"]], [cell, 0 if moved is None else moved_cell])
        expected = (e.u32(cell), e.u32(moved_cell) if moved is not None else 0)
        lib = runner.lib
        out = C.c_uint32(moved or 0)
        pointer = C.byref(out) if moved is not None else None
        if case["form"] == "value":
            lib.tpw_ri_align32_value.argtypes = [C.c_uint32, C.c_void_p]
            lib.tpw_ri_align32_value.restype = C.c_uint32
            got = lib.tpw_ri_align32_value(value, pointer)
        else:
            lib.tpw_ri_align32_address.argtypes = [C.c_uint64, C.c_void_p]
            lib.tpw_ri_align32_address.restype = C.c_uint64
            got = lib.tpw_ri_align32_address(value, pointer)
        actual = (got & MASK, out.value if moved is not None else 0)
        return [] if actual == expected else [f"{case}: engine {expected} port {actual}"]


TESTS = [
    make_test("DisableDataNodesRender", run_small, small_edges("disable"), small_random("disable")),
    make_test("DisableDataNodesRenderExcept", run_small, small_edges("except"), small_random("except")),
    make_test("SetupRideHeads", run_small, small_edges("heads"), small_random("heads")),
    make_test("CorrectFaceFlags1", run_small, small_edges("face_flags"), small_random("face_flags")),
    make_test("CorrectNodeFlags2", run_small, small_edges("node_flags"), small_random("node_flags")),
    make_test("OverrideActiveChannels", run_small, small_edges("channels"), small_random("channels")),
    make_test("GetFontPath", run_font, font_edges, font_random),
    make_test("ERRORBox", run_error_box, error_edges, error_random),
    make_test("InstanceAnim", run_anim, anim_edges, anim_random),
    make_test("InstanceMesh", run_mesh, instance_edges, instance_random),
    make_test("InstanceDynamicMeshSet", run_dmesh, dmesh_edges, dmesh_random),
    make_test("InstanceRideAnimSet", run_set, set_edges, set_random),
    AlignTest(),
]
if available(GATE + ".MD2"):
    TESTS += [
        make_test("InstanceMesh on the disc's meshes", run_mesh, real_mesh_edges, real_mesh_random),
        make_test("InstanceAnim on the disc's animations", run_anim, real_anim_edges, real_anim_random),
        make_test("InstanceRideAnimSet on the disc's rides", run_set, real_set_edges, real_set_random),
    ]
