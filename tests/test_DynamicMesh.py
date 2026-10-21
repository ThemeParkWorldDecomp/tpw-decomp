"""Unit w112b, the dynamic cell meshes, against libtpw_unit_DynamicMesh.dylib (tests/unit_DynamicMesh.cpp), in the main app
("SimTheme Park"), src/game/DynamicMeshSet.cpp, src/game/MeshLoad.cpp, src/game/park/DynamicCell.cpp, src/game/park/LLCellSystem.cpp:
  CDynamicMesh::CDynamicMesh 0x100e1610, ~CDynamicMesh 0x100e191c, SetBoundingBox 0x100e19b0, DC_SetVertex 0x100e1f10,
  DC_CreateSquareFace 0x100e210c, DMCreateMesh 0x100301c0, the two DMEnableMeshRender 0x1003088c and 0x10030928,
  CorrectDTexturePaths 0x100b8710, LLCC_SystemGetMode 0x100914f4

A case is a world and an operation. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's layouts
(big-endian, 32-bit pointers) for the original, and in host memory through the unit's allocation helpers (little-endian, native pointers
in the native records, arena offsets in the file-image structures) for the translation; the same code reads both back. The functions of
the unit call each other for real on both sides (the emulator runs the original code for them); every callee outside the unit is a fake
that records the same event on both sides (UseSmallTextures, GetCurrentThemeName, SetMatrixIdentity, MReleaseAllocF, which hands out
zeroed blocks and numbers them, CreateMeshFromM3D2Header, CreateEngineMesh, the three EnableMeshDraw, DMDisableMeshRender, DMFreeMesh,
FindFilePath, SetNewMeshPaths, OverrideTexturePaths, DetermineHFieldHeightQuick). Pointers are compared as labels, or as (block
number, offset) for what lies in a block MReleaseAllocF handed out.

What is compared after a call: the answer, the events in order, the 0xa4 bytes of the object field by field (the bytes the constructor
never writes keep their 0xa5 fill on both sides), the mesh block structure by structure (header, nodes, texture records, infos, vertex,
point, UV and face arrays, face sets, index buffers, and every byte no structure covers), the other blocks (the info names), the text
buffers, and the set, texture set and entries a hand-built DMCreateMesh was given.

The fused multiply-add of DC_CreateSquareFace (fnmsubs) gets the processor's single rounding through test_RideAnimInterp's
HardwareRounding (Unicorn rounds twice). Not tested (see the report): the number of times LLCC_SystemGetMode is called (the function is a
pure read), a sprintf longer than the host's 1,031 bytes, a sine table mask above 0xfff (the host table has 0x1000 entries), item counts
that make a node write past the block (the Mac corrupts memory there too), the dead `*ok != 1` path of the constructor.
"""
import ctypes as C
import math
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest
from test_RideAnimInterp import LIBM, fpr, fpr_double

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_DynamicMesh"
MASK = 0xFFFFFFFF
NAN = 0x7FC00000

CTOR, DTOR, BBOX, SQUARE, VERTEX = 0x100E1610, 0x100E191C, 0x100E19B0, 0x100E210C, 0x100E1F10
CREATE, ENABLE, ENABLE_XZ, CORRECT, MODE = 0x100301C0, 0x1003088C, 0x10030928, 0x100B8710, 0x100914F4
USE_SMALL, THEME, IDENTITY, ALLOC, CREATE_MESH, ENGINE_MESH = 0x10233DE0, 0x1014A058, 0x100C0E28, 0x10064BD4, 0x10064CF0, 0x10064F40
DRAW, DRAW_MATRIX, DRAW_XZ, DISABLE, FREE_MESH = 0x10065178, 0x10065200, 0x100650C4, 0x100309C8, 0x10030AE8
FIND, SET_PATHS, OVERRIDE, HEIGHT = 0x100597F8, 0x100B89DC, 0x100B8EA8, 0x1005EDA8

# Globals: name -> (the Mac's address, the host library's index, see tpw_dm_global).
GLOBALS = dict(mode=(0x102E480C, 0), db50=(0x102DDB50, 1), db54=(0x102DDB54, 2), bc0=(0x102F9BC0, 3), bc4=(0x102F9BC4, 4),
               bd4=(0x102F9BD4, 5), da0=(0x102F9DA0, 6), uv=(0x102F9B48, 7), tpg=(0x103B74C8, 8), path=(0x102F7CC8, 9),
               entries=(0x102A01BC, 10), game=(0x1037F134, 11))

# Labels of the objects of a case.
K_OBJ, K_TEX, K_ITEM, K_SET, K_ENTRIES, K_GAME, K_MESH, K_MATRIX, K_HEADER, K_SRCTEX, K_SRCSET, K_PLAIN = 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12

# The records, by the Mac's offsets: name -> (kind, offset). Kinds: b h w (1, 2, 4 bytes), f (a float, as its bits), n (a pointer in a
# native record: 4 bytes on the Mac, 8 on the host), q (a pointer in a file image: 4 bytes, an arena offset on the host).
MAC = dict(
    set=dict(flags=("w", 0x00), count=("h", 0x04), f06=("h", 0x06), entries=("n", 0x08), matrix=("n", 0x0C), vsets=("n", 0x10),
             points=("n", 0x14), uvsets=("n", 0x18), faces=("n", 0x1C), block20=("n", 0x20), mesh=("n", 0x24), ntex=("h", 0x28),
             nvert=("h", 0x2A), nfaceset=("h", 0x2C), nface=("h", 0x2E), nuv=("h", 0x30), npoint=("h", 0x32), alloc=("n", 0x34),
             f38=("w", 0x38), shared=("n", 0x3C)),
    tex=dict(flags=("w", 0x00), count=("h", 0x04), folder=("n", 0x08), sfolder=("n", 0x0C), entries=("n", 0x10), infos=("n", 0x14),
             textures=("n", 0x18), use=("w", 0x1C), userec=("w", 0x00)),
    entry=dict(flags=("w", 0x00), name=("n", 0x04)),
    item=dict(flags=("w", 0x00), nface=("h", 0x04), nvert=("h", 0x06), nuv=("h", 0x08), nfaceset=("h", 0x0A), npoint=("h", 0x0C),
              f0e=("h", 0x0E), box=("f", 0x10), matrix=("n", 0x28), facesets=("n", 0x2C)),
    obj=dict(type=("w", 0x00), tex=("w", 0x04), item=("w", 0x24), set=("w", 0x54), cursor=("n", 0x94), nfacesdone=("w", 0x98),
             nvertdone=("w", 0x9C), maxsq=("w", 0xA0)),
    mesh=dict(header=("n", 0x04)),
)
SIZE_MAC = dict(set=0x40, tex=0x20, entry=0x10, item=0x30, obj=0xA4, mesh=0x40)
# The order tpw_dm_layout lists the host's offsets (each record: its fields, then its size).
LAYOUT_ORDER = [
    ("set", ["flags", "count", "f06", "entries", "matrix", "vsets", "points", "uvsets", "faces", "block20", "mesh", "ntex", "nvert",
             "nfaceset", "nface", "nuv", "npoint", "alloc", "f38", "shared"]),
    ("tex", ["flags", "count", "folder", "sfolder", "entries", "infos", "textures", "use", "userec"]),
    ("entry", ["flags", "name"]),
    ("item", ["flags", "nface", "nvert", "nuv", "nfaceset", "npoint", "f0e", "box", "matrix", "facesets"]),
    ("obj", ["type", "tex", "item", "set", "cursor", "nfacesdone", "nvertdone", "maxsq"]),
    ("mesh", ["header"]),
]
WIDTH = dict(b=1, h=2, w=4, f=4, n=4, q=4)
SET_FIELDS = list(MAC["set"])
TEX_FIELDS = ["flags", "count", "folder", "sfolder", "entries", "infos", "textures", "use"]
ITEM_FIELDS = ["flags", "nface", "nvert", "nuv", "nfaceset", "npoint", "f0e", "matrix", "facesets"]

# The file-image structures (the Mac's layout on both sides): offset -> kind of the typed fields; the bytes between are compared raw.
HEADER_SPEC = ([(0x30, "w")] + [(o, "h") for o in range(0x34, 0x4C, 2)] + [(o, "q") for o in (0x4C, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C, 0x70, 0x74, 0x78, 0x7C)] +
               [(o, "f") for o in range(0x80, 0x98, 4)] + [(o, "q") for o in (0x98, 0x9C)] + [(0xA0, "f")] + [(o, "q") for o in (0xA4, 0xAC, 0xB0, 0xB4)])
NODE_SPEC = ([(0, "w"), (4, "q"), (8, "q"), (0xC, "q")] + [(o, "f") for o in range(0x10, 0x50, 4)] + [(0x50, "h"), (0x52, "h"), (0x54, "q")] +
             [(o, "h") for o in (0x58, 0x5A, 0x5C, 0x5E)] + [(o, "q") for o in (0x60, 0x64, 0x68, 0x6C, 0x70, 0x74)] +
             [(o, "f") for o in range(0x78, 0x90, 4)] + [(o, "q") for o in (0x94, 0x98, 0x9C)])
INFO_SPEC = [(0, "w"), (4, "q"), (0xA, "h"), (0xC, "q")]
TEXTURE_SPEC = [(0, "h"), (2, "h"), (4, "q")]
FACESET_SPEC = [(0, "q"), (4, "h"), (6, "h"), (8, "h"), (0xA, "h")]
VSET_SPEC = [(o, "f") for o in range(0, 0x30, 4)]
POINT_SPEC = [(o, "f") for o in range(0, 0xC, 4)]
UVSET_SPEC = [(o, "f") for o in range(0, 0x20, 4)]
FACE_SPEC = [(o, "h") for o in range(0, 8, 2)]

TAG_BITS = 32


def fbits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    """A float's bits with every NaN made one (the processors make different NaNs)."""
    return NAN if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


# ---------------------------------------------------------------------------------------------------------------- the emulator's arithmetic


class HardwareRounding:
    """Two things Unicorn (QEMU 5.0) does not execute as the processor does, corrected for the code under test and nowhere else:
    fnmsubs (and its relatives) are computed in double precision and rounded to single, which can differ by one unit in the last place
    from the processor's single rounding (the result is replaced by the C library's fmaf), and fctiwz of a value above INT32_MAX gives
    0x80000000 where the processor saturates to 0x7fffffff (DC_SetVertex's sine table index). A code hook looks at each instruction
    before it runs and rewrites its target register just before the next one."""

    def __init__(self, e):
        self.e, self.pending, self.hooks = e, None, []

    def __enter__(self):
        for begin, end in ((VERTEX, VERTEX + 444), (SQUARE, SQUARE + 1576)):
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            kind, target, value = self.pending
            self.pending = None
            if kind == "fctiwz":
                uc.reg_write(ppcemu.FPR[target], (fpr(e, target) & 0xFFFFFFFF00000000) | 0x7FFFFFFF)
            else:
                uc.reg_write(ppcemu.FPR[target], struct.unpack(">Q", struct.pack(">d", value))[0])
        word = struct.unpack(">I", e.read(address, 4))[0]
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        if word >> 26 == 63 and (word >> 1) & 0x3FF == 15:          # fctiwz frD, frB
            if fpr_double(e, rb) >= 2147483648.0:
                self.pending = ("fctiwz", rd, None)
            return
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        value = float(LIBM.fmaf(a, c, -b if kind in (28, 30) else b))
        if kind in (30, 31):
            value = -value
        self.pending = ("fused", rd, value)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory: allocation, typed reads and writes of records, calls, the event log."""

    def get(self, rec, base, name):
        kind, off = self.field(rec, name)
        return self.rd(base + off, kind)

    def put(self, rec, base, name, value):
        kind, off = self.field(rec, name)
        self.wr(base + off, kind, value)

    def addr(self, rec, base, name):
        return base + self.field(rec, name)[1]

    def tag(self, pointer):
        if pointer == 0:
            return 0
        for k, (address, size) in enumerate(self.get_blocks()):
            if address <= pointer < address + size:
                return ((k + 1) << TAG_BITS) | (pointer - address)
        return self.lab(pointer)

    def canon(self, kind, value):
        if kind in ("n", "q"):
            return self.tag(value)
        if kind == "f":
            return canon_f(value)
        return value

    def cstring(self, text, lead=0x78):
        """A C string with one byte in front of it (a function may read the byte before an empty string); returns the string's address."""
        raw = text.encode("latin-1")
        address = self.alloc(len(raw) + 2, 0x00)
        self.write(address, bytes([lead]) + raw + b"\0")
        return address + 1

    def set_words(self, address, words):
        self.write(address, b"".join(self.pack_word(w) for w in words))

    def words(self, address, kind, count):
        return [self.canon(kind, self.rd(address + WIDTH[kind] * i, kind)) for i in range(count)]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's scratch area."""
    ppc = True
    mask = MASK

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case.get("q"))
        self.blocks = []
        self.theme_obj = 0
        self.deletes = 0
        self.search = [0]
        e.dm = self
        self.saved = STUBS.activate(e)
        for text in ("SearchPath1:", "Other:SearchPath2:", "S3"):
            self.search.append(CL.new_text(e, text))
        self.L.labels[GLOBALS["game"][0]] = K_GAME
        self.L.labels[GLOBALS["entries"][0]] = K_ENTRIES

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.dm = None

    def begin(self):
        pass

    def end(self):
        return self.L.log

    def field(self, rec, name):
        return MAC[rec][name]

    def size(self, rec):
        return SIZE_MAC[rec]

    def alloc(self, size, fill=0xA5, label=0, arena=False):
        address = self.e.alloc(size, bytes([fill & 0xFF]) * size)
        if label:
            self.L.labels[address] = label
        return address

    def label(self, address, label):
        self.L.labels[address] = label

    def lab(self, pointer):
        return self.L.lab(pointer)

    def get_blocks(self):
        return self.blocks

    def rd(self, a, kind):
        return int.from_bytes(self.e.read(a, WIDTH[kind]), "big")

    def wr(self, a, kind, value):
        n = WIDTH[kind]
        self.e.write(a, (value & ((1 << (8 * n)) - 1)).to_bytes(n, "big"))

    def pack_word(self, w):
        return struct.pack(">I", w & MASK)

    def write(self, a, data):
        self.e.write(a, data)

    def bytes(self, a, n):
        return self.e.read(a, n)

    def text(self, a):
        return self.e.cstr(a)[:159] if a else "(null)"

    def gaddr(self, name):
        return GLOBALS[name][0]

    def set_theme(self, text):
        self.theme_obj = self.e.alloc(0x14)
        CL.tds_set(self.e, self.theme_obj, text)

    def call(self, address, *args):
        return T.call_ppc(self.e, address, list(args))[0]

    def mode(self):
        return T.s32(self.call(MODE))

    def bbox(self, obj, bits):
        self.call(BBOX, obj, *[T.F(bitsf(b)) for b in bits])

    def enable(self, s):
        return T.s32(self.call(ENABLE, s))

    def enable_xz(self, s, a, b):
        return T.s32(self.call(ENABLE_XZ, s, a, b))

    def destroy(self, obj, flag):
        self.call(DTOR, obj, T.S16(flag))

    def correct(self, tex, s):
        self.call(CORRECT, tex, s)

    def vertex(self, obj, x, y, z, u, v, follow, point):
        with HardwareRounding(self.e):
            self.call(VERTEX, obj, T.F(bitsf(x)), T.F(bitsf(y)), T.F(bitsf(z)), T.F(bitsf(u)), T.F(bitsf(v)), follow, point)

    def square(self, obj, x, y, z, side, rotation, texture, f4, large, u10, keep):
        with HardwareRounding(self.e):
            self.call(SQUARE, obj, T.F(bitsf(x)), T.F(bitsf(y)), T.F(bitsf(z)), T.U8(side), T.U8(rotation), texture, T.F(bitsf(f4)), large,
                      T.U8(u10), keep)

    def create(self, s, tex, name, flags):
        return T.s32(self.call(CREATE, s, tex, name, flags))

    def construct(self, obj, size_a, size_b, ok, name, type_, bits):
        with HardwareRounding(self.e):
            self.call(CTOR, obj, size_a, size_b, ok, name, type_, *[T.F(bitsf(b)) for b in bits])


class Host(Side):
    ppc = False
    mask = (1 << 64) - 1
    layout = None

    def __init__(self, lib, case):
        self.lib = lib
        if Host.layout is None:
            Host.layout = load_layout(lib)
        self.sig()
        lib.tpw_dm_reset()
        CL.host_begin(lib, {}, case.get("q"))
        self.base = lib.tpw_dm_arena_base()
        self.blocks_cache = None
        self.lib.tpw_cl_label(K_GAME, self.gaddr("game"))
        self.lib.tpw_cl_label(K_ENTRIES, self.gaddr("entries"))
        (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")[:] = [0, 0, 0, 0]

    def sig(self):
        lib, P, U, I = self.lib, C.c_void_p, C.c_uint32, C.c_int32
        table = dict(
            tpw_dm_reset=([], None), tpw_dm_alloc=([C.c_uint64, U, I, I], C.c_uint64), tpw_dm_arena_base=([], C.c_uint64),
            tpw_dm_block=([I, P], I), tpw_dm_global=([I], C.c_uint64), tpw_dm_set_theme=([C.c_char_p], None), tpw_dm_layout=([P], I),
            tpw_dm_get_mode=([], I), tpw_dm_bounding_box=([P, U, U, U, U, U, U], None), tpw_dm_enable=([P], I),
            tpw_dm_enable_xz=([P, U, U], I), tpw_dm_destroy=([P, I], None), tpw_dm_correct=([P, P], None),
            tpw_dm_vertex=([P, U, U, U, U, U, I, P], None), tpw_dm_square=([P, U, U, U, U, U, I, U, I, U, I], None),
            tpw_dm_create=([P, P, P, U], I), tpw_dm_construct=([P, U, U, P, P, I, U, U, U, U, U, U], None),
            tpw_cl_label=([I, P], None), tpw_cl_lab=([P], I))
        for name, (args, res) in table.items():
            fn = getattr(lib, name)
            fn.argtypes, fn.restype = args, res

    def done(self):
        self.lib.tpw_dm_reset()

    def begin(self):
        pass

    def end(self):
        return CL.host_end(self.lib)

    def field(self, rec, name):
        kind = MAC[rec][name][0]
        if rec in Host.layout:
            return kind, Host.layout[rec][name]
        return kind, MAC[rec][name][1]

    def size(self, rec):
        return Host.layout[rec]["size"] if rec in Host.layout else SIZE_MAC[rec]

    def alloc(self, size, fill=0xA5, label=0, arena=False):
        return self.lib.tpw_dm_alloc(size, fill, label, 1 if arena else 0)

    def label(self, address, label):
        self.lib.tpw_cl_label(label, address)

    def lab(self, pointer):
        return self.lib.tpw_cl_lab(pointer)

    def get_blocks(self):
        # asked for only after the call under test (the blocks are final by then), so the list is kept
        if self.blocks_cache is None:
            self.blocks_cache = []
            buffer = (C.c_uint64 * 2)()
            k = 0
            while self.lib.tpw_dm_block(k, buffer):
                self.blocks_cache.append((buffer[0], buffer[1]))
                k += 1
        return self.blocks_cache

    def rd(self, a, kind):
        n = 8 if kind == "n" else WIDTH[kind]
        value = int.from_bytes(C.string_at(a, n), "little")
        if kind == "q" and value:
            value += self.base
        return value

    def wr(self, a, kind, value):
        n = 8 if kind == "n" else WIDTH[kind]
        if kind == "q" and value:
            value -= self.base
        C.memmove(a, (value & ((1 << (8 * n)) - 1)).to_bytes(n, "little"), n)

    def pack_word(self, w):
        return struct.pack("<I", w & MASK)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def bytes(self, a, n):
        return C.string_at(a, n)

    def text(self, a):
        return C.string_at(a).decode("latin-1")[:159] if a else "(null)"

    def gaddr(self, name):
        return self.lib.tpw_dm_global(GLOBALS[name][1])

    def set_theme(self, text):
        self.lib.tpw_dm_set_theme(text.encode("latin-1"))

    def mode(self):
        return T.s32(self.lib.tpw_dm_get_mode())

    def bbox(self, obj, bits):
        self.lib.tpw_dm_bounding_box(obj, *bits)

    def enable(self, s):
        return self.lib.tpw_dm_enable(s)

    def enable_xz(self, s, a, b):
        return self.lib.tpw_dm_enable_xz(s, a, b)

    def destroy(self, obj, flag):
        self.lib.tpw_dm_destroy(obj, 1 if T.s16(flag) > 0 else 0)

    def correct(self, tex, s):
        self.lib.tpw_dm_correct(tex, s)

    def vertex(self, obj, x, y, z, u, v, follow, point):
        self.lib.tpw_dm_vertex(obj, x, y, z, u, v, T.s32(follow), point or None)

    def square(self, obj, x, y, z, side, rotation, texture, f4, large, u10, keep):
        self.lib.tpw_dm_square(obj, x, y, z, side, rotation, T.s32(texture), f4, T.s32(large), u10, T.s32(keep))

    def create(self, s, tex, name, flags):
        return self.lib.tpw_dm_create(s, tex, name or None, flags)

    def construct(self, obj, size_a, size_b, ok, name, type_, bits):
        self.lib.tpw_dm_construct(obj, size_a, size_b, ok, name or None, type_, *bits)


def load_layout(lib):
    lib.tpw_dm_layout.argtypes = [C.c_void_p]
    lib.tpw_dm_layout.restype = C.c_int32
    out = (C.c_int64 * 256)()
    n = lib.tpw_dm_layout(out)
    values = iter(out[:n])
    layout = {}
    for rec, names in LAYOUT_ORDER:
        layout[rec] = {name: next(values) for name in names}
        layout[rec]["size"] = next(values)
    assert next(values, None) is None
    return layout


# ---------------------------------------------------------------------------------------------------------------- the fakes (PowerPC side)


def build_stubs():
    S = CL.Stubs()
    S.real = {CTOR, DTOR, BBOX, SQUARE, VERTEX, CREATE, ENABLE, ENABLE_XZ, CORRECT, MODE}

    def rec(e, name, ints=(), texts=()):
        e.cl.rec(name, ints, texts)

    def use_small(e):
        rec(e, "UseSmallTextures")
        e.set_r(3, T.u32(e.cl.ret("small", 0)))

    def theme(e):
        rec(e, "GetCurrentThemeName", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.dm.theme_obj)

    def identity(e):
        rec(e, "SetMatrixIdentity", (e.dm.tag(e.r(3)),))

    def alloc(e):
        size, flags = e.r(3), e.r(4)
        rec(e, "MReleaseAllocF", (size, flags))
        address = e.alloc(size)
        e.dm.blocks.append((address, size))
        e.set_r(3, address)

    def create_mesh(e):
        header = e.r(3)
        rec(e, "CreateMeshFromM3D2Header", (e.dm.tag(header),))
        mesh = e.alloc(0x40)
        e.write(mesh + 4, struct.pack(">I", header))
        e.dm.label(mesh, K_MESH)
        e.set_r(3, mesh)

    def draw_matrix(e):
        rec(e, "EnableMeshDrawMatrix", (e.cl.lab(e.r(3)), e.dm.tag(e.r(4)), T.s32(e.r(5))))

    def disable(e):
        rec(e, "DMDisableMeshRender", (e.cl.lab(e.r(3)), e.u32(e.r(3))))
        e.set_r(3, 1)

    def free_mesh(e):
        rec(e, "DMFreeMesh", (e.cl.lab(e.r(3)), e.u32(e.r(3))))
        e.set_r(3, 1)

    def find(e):
        name, flags, result = e.r(3), e.r(4), e.r(5)
        rec(e, "FindFilePath", (flags, int(result != 0)), (e.cstr(name)[:159],))
        which = e.cl.ret("find", 0)
        e.set_r(3, 0 if which == 0 else e.dm.search[which & 3])

    def text(e, p):
        return e.cstr(p)[:159] if p else "(null)"

    def set_paths(e):
        rec(e, "SetNewMeshPaths", (), tuple(text(e, e.r(i)) for i in (3, 4)))
        rec(e, "SetNewMeshPaths.textures", (), tuple(text(e, e.r(i)) for i in (5, 6)))

    def override(e):
        rec(e, "OverrideTexturePaths", (e.dm.tag(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5))))

    def height(e):
        rec(e, "DetermineHFieldHeightQuick", (fbits(e.f(1)), fbits(e.f(2))))
        e.set_f1(bitsf(e.cl.ret("height", 0)))

    def delete(e):
        p = e.r(3)
        e.dm.deletes += 1
        if p and e.cl.lab(p) > 0:
            rec(e, "delete", (e.cl.lab(p),))
            e.cl.forget(p)

    def strcat(e):
        d = e.r(3)
        e.write(d + len(e.cstr(d, limit=1 << 20).encode("latin-1")), e.cstr(e.r(4), limit=1 << 20).encode("latin-1") + b"\0")

    def strncpy(e):
        d, s, n = e.r(3), e.r(4), e.r(5)
        data = e.cstr(s, limit=1 << 20).encode("latin-1")[:n]
        e.write(d, data + b"\0" * (n - len(data)))

    S.internal(USE_SMALL, use_small)
    S.internal(THEME, theme)
    S.internal(IDENTITY, identity)
    S.internal(ALLOC, alloc)
    S.internal(CREATE_MESH, create_mesh)
    S.internal(ENGINE_MESH, lambda e: rec(e, "CreateEngineMesh", (e.cl.lab(e.r(3)),)))
    S.internal(DRAW, lambda e: rec(e, "EnableMeshDraw", (e.cl.lab(e.r(3)),)))
    S.internal(DRAW_MATRIX, draw_matrix)
    S.internal(DRAW_XZ, lambda e: rec(e, "EnableMeshDrawXZ", (e.cl.lab(e.r(3)), e.r(4), e.r(5))))
    S.internal(DISABLE, disable)
    S.internal(FREE_MESH, free_mesh)
    S.internal(FIND, find)
    S.internal(SET_PATHS, set_paths)
    S.internal(OVERRIDE, override)
    S.internal(HEIGHT, height)
    S.imp("__dl__FPv", delete)
    S.imp("strcat", strcat)
    S.imp("strncpy", strncpy)
    return S


STUBS = build_stubs()


def setup(runner):
    T.install_std_stubs(runner.emu)


def ppc_case(fn):
    def run(e, case):
        s = Ppc(e, case)
        try:
            return fn(s, case)
        finally:
            s.done()
    return run


def host_case(fn):
    def run(lib, case):
        s = Host(lib, case)
        try:
            return fn(s, case)
        finally:
            s.done()
    return run


def deletes(s):
    return s.deletes if s.ppc else (C.c_int32 * 4).in_dll(s.lib, "tpw_cl_heap")[2]


# ---------------------------------------------------------------------------------------------------------------- reading a world back


def dump_struct(s, a, size, spec, cover=None, base=0):
    """The typed fields of a file-image structure in order, then the bytes between them as one bytes object."""
    raw = s.bytes(a, size)
    out, left, pos = [], bytearray(), 0
    for off, kind in spec:
        left += raw[pos:off]
        out.append(s.canon(kind, s.rd(a + off, kind)))
        pos = off + WIDTH[kind]
    left += raw[pos:size]
    out.append(bytes(left))
    if cover is not None:
        cover.append((a - base, a - base + size))
    return out


def dump_main(s, base, size):
    """The mesh block: the header, then each array the header names (when it lies in the block), then every byte nothing covers."""
    cover = []
    out = [dump_struct(s, base, 0xB8, HEADER_SPEC, cover, base)]

    def rd(off, kind):
        return s.rd(base + off, kind)

    def region(tag, a, count, stride, spec):
        items = []
        if a != 0:
            for i in range(count):
                element = a + i * stride
                if element < base or element + stride > base + size:
                    items.append("outside")
                    break
                items.append(dump_struct(s, element, stride, spec, cover, base))
        out.append([tag, items])

    nodes, node_count = rd(0x70, "q"), rd(0x42, "h")
    region("nodes", nodes, node_count, 0xA0, NODE_SPEC)
    region("textures", rd(0x50, "q"), rd(0x36, "h"), 8, TEXTURE_SPEC)
    region("infos", rd(0x54, "q"), rd(0x36, "h"), 0x10, INFO_SPEC)
    region("vsets", rd(0x58, "q"), rd(0x38, "h"), 0x30, VSET_SPEC)
    region("points", rd(0x5C, "q"), rd(0x4A, "h"), 0xC, POINT_SPEC)
    region("uvsets", rd(0x60, "q"), rd(0x3A, "h"), 0x20, UVSET_SPEC)
    region("faces", rd(0x68, "q"), rd(0x3E, "h"), 8, FACE_SPEC)
    region("facesets", rd(0x64, "q"), rd(0x3C, "h"), 0x10, FACESET_SPEC)
    if nodes != 0:
        for i in range(node_count):
            node = nodes + i * 0xA0
            if base <= node and node + 0xA0 <= base + size:
                region("indices", s.rd(node + 0x94, "q"), s.rd(node + 0x58, "h"), 2, [(0, "h")])
    # every byte no structure covers, as one bytes object (the covered ranges merged, the gaps joined)
    raw = s.bytes(base, size)
    gaps, pos = [], 0
    for begin, end in sorted(cover):
        if begin > pos:
            gaps.append(raw[pos:begin])
        pos = max(pos, end)
    gaps.append(raw[pos:])
    out.append(b"".join(gaps))
    return out


def dump_blocks(s):
    blocks = s.get_blocks()
    out = [[size for _, size in blocks]]
    if blocks:
        out.append(dump_main(s, blocks[0][0], blocks[0][1]))
        for address, size in blocks[1:]:
            out.append(s.bytes(address, size))
    return out


def dump_set(s, a, tex):
    out = []
    for name in SET_FIELDS:
        kind = MAC["set"][name][0]
        value = s.get("set", a, name)
        if name == "shared":
            out.append(int(value != 0 and value == tex + s.field("tex", "userec")[1]))
        else:
            out.append(s.canon(kind, value))
    return out


def dump_tex(s, a):
    out = []
    for name in TEX_FIELDS:
        kind = MAC["tex"][name][0]
        value = s.get("tex", a, name)
        if name in ("folder", "sfolder"):
            out.append("(poison)" if value in (0xA5A5A5A5, 0xA5A5A5A5A5A5A5A5) else s.text(value))
        else:
            out.append(s.canon(kind, value))
    return out


def dump_item(s, a):
    out = [s.canon(MAC["item"][name][0], s.get("item", a, name)) for name in ITEM_FIELDS]
    out.append(s.words(s.addr("item", a, "box"), "f", 6))
    return out


def dump_object(s, obj):
    out = [s.get("obj", obj, "type"), dump_tex(s, obj + s.field("obj", "tex")[1]), dump_item(s, obj + s.field("obj", "item")[1]),
           dump_set(s, obj + s.field("obj", "set")[1], obj + s.field("obj", "tex")[1])]
    out.append(s.canon("n", s.get("obj", obj, "cursor")))
    out += [s.get("obj", obj, n) for n in ("nfacesdone", "nvertdone", "maxsq")]
    return out


def new_object(s, fill=0xA5):
    """A CDynamicMesh over `fill` bytes, with labels on its three embedded records."""
    obj = s.alloc(s.size("obj"), fill, K_OBJ)
    s.label(obj + s.field("obj", "tex")[1], K_TEX)
    s.label(obj + s.field("obj", "item")[1], K_ITEM)
    s.label(obj + s.field("obj", "set")[1], K_SET)
    return obj


def result(s, **state):
    state["log"] = s.end()
    return state


# ---------------------------------------------------------------------------------------------------------------- random values


def fl(r):
    """The bits of a random single: 40% moderate, 30% any pattern (quiet NaNs only), 30% edge values."""
    k = r.random()
    if k < 0.4:
        return fbits(T.f32(r.uniform(-3000.0, 3000.0)))
    if k < 0.55:
        return fbits(T.f32(r.choice([1e-3, 1.0, 1e3, 1e6, 1e9, 1e30]) * r.uniform(-1.0, 1.0)))
    if k < 0.7:
        bits = r.getrandbits(32)
        if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF):
            bits |= 0x400000
        return bits
    return r.choice([0, 0x80000000, 0x3F800000, 0xBF800000, 0x41200000, 0x41A00000, 0x7F800000, 0xFF800000, NAN, 0xFFC00000, 0x4B800000,
                     0x4CBEBC20, 0x7F7FFFFF, 1, 0x00800000, 0x3F000000, 0x4B7FFFFF, 0x4B000001])


def word(r):
    return r.choice([0, 1, 2, 3, 4, 5, 0xE, 0x13, 0xC, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, r.getrandbits(32), r.randint(0, 0x30)])


# ---------------------------------------------------------------------------------------------------------------- LLCC_SystemGetMode


def run_mode(s, case):
    s.begin()
    s.wr(s.gaddr("mode"), "w", case["mode"])
    return result(s, answer=s.mode())


def mode_edges():
    for v in (0, 1, 4, 0xE, 0x13, 0x7FFFFFFF, 0x80000000, MASK, 0x12345678, 5):
        yield dict(mode=v)


def mode_rnd(r):
    return dict(mode=word(r))


# ---------------------------------------------------------------------------------------------------------------- SetBoundingBox


def run_bbox(s, case):
    s.begin()
    obj = new_object(s)
    s.bbox(obj, case["bits"])
    return result(s, obj=dump_object(s, obj))


def bbox_edges():
    base = [0x3F800000, 0x40000000, 0x40400000, 0x40800000, 0x40A00000, 0x40C00000]
    yield dict(bits=base)
    for special in (0x80000000, NAN, 0x7F800000, 0xFF800000, 0, 0x00000001):
        for i in range(6):
            bits = list(base)
            bits[i] = special
            yield dict(bits=bits)


def bbox_rnd(r):
    return dict(bits=[fl(r) for _ in range(6)])


# ---------------------------------------------------------------------------------------------------------------- DMEnableMeshRender


def build_enable_set(s, case):
    st = s.alloc(s.size("set"), 0xA5, K_SET)
    mesh = s.alloc(s.size("mesh"), 0xA5, K_MESH)
    s.put("set", st, "flags", case["flags"])
    s.put("set", st, "mesh", mesh)
    s.put("set", st, "matrix", s.alloc(0x40, 0xA5, K_MATRIX) if case["matrix"] else 0)
    return st


def run_enable(s, case):
    s.begin()
    st = build_enable_set(s, case)
    answer = s.enable(st)
    return result(s, answer=answer, set=dump_set(s, st, 0))


def run_enable_xz(s, case):
    s.begin()
    st = build_enable_set(s, case)
    answer = s.enable_xz(st, case["from"], case["to"])
    return result(s, answer=answer, set=dump_set(s, st, 0))


def enable_edges():
    for flags in (0, 0x4000, MASK, 0x1234, 0xFFFFBFFF, 0x80000000):
        for matrix in (0, 1):
            yield dict(flags=flags, matrix=matrix)


def enable_rnd(r):
    return dict(flags=word(r), matrix=r.randint(0, 1))


def enable_xz_edges():
    for flags in (0, 0x4000, MASK, 0xFFFFBFFF):
        for a, b in ((0, 0), (MASK, MASK), (0x00010002, 0x00030004), (0x80008000, 0x7FFF7FFF), (0xFFFF0000, 0x0000FFFF)):
            yield dict(flags=flags, matrix=0, **{"from": a, "to": b})
    yield dict(flags=0, matrix=1, **{"from": 5, "to": 6})


def enable_xz_rnd(r):
    return dict(flags=word(r), matrix=r.randint(0, 1), **{"from": r.getrandbits(32), "to": r.getrandbits(32)})


# ---------------------------------------------------------------------------------------------------------------- the destructor


def run_destroy(s, case):
    s.begin()
    obj = new_object(s)
    st = obj + s.field("obj", "set")[1]
    s.put("set", st, "flags", case["flags"])
    s.put("set", st, "mesh", s.alloc(s.size("mesh"), 0xA5, K_MESH))
    s.destroy(obj, case["flag"])
    state = dict(deletes=deletes(s))
    if T.s16(case["flag"]) <= 0:
        state["set_flags"] = s.get("set", st, "flags")
    return result(s, **state)


def destroy_edges():
    for flag in (1, 0, -1, 2, 0x7FFF, -0x8000, 0xFFFF, 0x10000, 0x10001):
        for flags in (0, 0x4000, 0xFFFFFFFF, 0x2000):
            yield dict(flag=flag, flags=flags)


def destroy_rnd(r):
    return dict(flag=r.choice([1, 0, -1, 2, r.randint(-0x8000, 0x7FFF)]), flags=word(r))


# ---------------------------------------------------------------------------------------------------------------- CorrectDTexturePaths


def build_texture_set(s, case, label=K_SRCTEX):
    """A texture set with its folders and entries, over 0xa5 bytes."""
    count = len(case["names"])
    tex = s.alloc(s.size("tex"), 0xA5, label)
    entries = s.alloc(s.size("entry") * max(count, 1), 0xA5)
    for i, name in enumerate(case["names"]):
        ea = entries + i * s.size("entry")
        s.put("entry", ea, "flags", case["eflags"][i])
        s.put("entry", ea, "name", s.cstring(name))
    if count == 0:
        s.put("entry", entries, "name", s.cstring("none.tga"))
    s.put("tex", tex, "flags", case["tflags"])
    s.put("tex", tex, "count", count)
    s.put("tex", tex, "folder", s.cstring(case["folder"], case.get("lead", 0x78)))
    s.put("tex", tex, "sfolder", s.cstring(case["sfolder"]))
    s.put("tex", tex, "entries", entries)
    s.put("tex", tex, "infos", 0)
    s.put("tex", tex, "textures", 0)
    s.put("tex", tex, "use", case["use"])
    return tex, entries


def run_correct(s, case):
    s.begin()
    tex, _ = build_texture_set(s, case)
    st = s.alloc(s.size("set"), 0xA5, K_SET)
    mesh = s.alloc(s.size("mesh"), 0x00, K_MESH)
    header = s.alloc(0xB8, 0x00, K_HEADER, arena=True)
    s.put("mesh", mesh, "header", header)
    s.put("set", st, "mesh", mesh)
    s.correct(tex, st)
    return result(s, tex=dump_tex(s, tex), path=s.text(s.gaddr("path")))


def correct_case(folder, name, find, lead=0x78, sfolder="Shared:Folder"):
    return dict(folder=folder, sfolder=sfolder, names=[name], eflags=[7], tflags=0, use=0, lead=lead, q=dict(find=find))


def correct_edges():
    for folder in ("Data:Generic:Dynamic:Textures", "Data:Generic:Dynamic:Textures:", "x", ":", "a:b", "dir.with.dot", "dir.dot:", ""):
        for name in ("blue.tga", "no_dot", "a.b.c.tga", "", ".", ".tga", "x."):
            for find in ([0], [1], [0, 1], [0, 0], [2], [3, 1], [0, 3]):
                yield correct_case(folder, name, find, lead=0x3A if folder == "" else 0x78)
    yield correct_case("", "blue.tga", [0], lead=0x78)
    yield correct_case("", "blue.tga", [1], lead=0x3A)
    for n in (900, 1000, 1010):
        yield correct_case("F" * n, "x.tga", [0])
        yield correct_case("D" * (n // 2) + ":", "y" * (n // 4) + ".tga", [0, 1])
    yield correct_case("Data:Generic:Dynamic:STexture", "gby_sur1.tga", [0, 1], sfolder="Data:Levels:Hallow:Dynamic:STexture")


def correct_rnd(r):
    def text(n):
        return "".join(r.choice("abcXYZ019_.:") for _ in range(n))
    folder = r.choice([text(r.randint(1, 40)), text(r.randint(1, 20)) + ":", "Data:Generic:Dynamic:Textures"])
    name = r.choice([text(r.randint(0, 25)), "t" + text(r.randint(0, 10)) + ".tga"])
    return correct_case(folder, name, [r.randint(0, 3) for _ in range(r.randint(1, 3))], sfolder=text(r.randint(0, 30)))


# ---------------------------------------------------------------------------------------------------------------- the cell world (vertices and squares)

GUARD = 0x40
TABLES = {}


def sine_table(seed):
    if seed not in TABLES:
        import random
        r = random.Random(seed)
        TABLES[seed] = [fbits(T.f32(r.uniform(-1.0, 1.0))) for _ in range(0x1000)]
    return TABLES[seed]


def set_globals(s, case):
    s.wr(s.gaddr("da0"), "f", case["da0"])
    tpg = s.gaddr("tpg")
    s.wr(tpg + 4, "w", case["mask"])
    s.wr(tpg + 8, "f", case["scale"])
    s.set_words(tpg + 0xC, sine_table(case["table"]))
    s.set_words(s.gaddr("uv"), case["uv"])
    for name in ("mode", "db50", "db54", "bd4"):
        s.wr(s.gaddr(name), "w", case[name])
    s.wr(s.gaddr("bc0"), "f", case["bc0"])
    s.wr(s.gaddr("bc4"), "f", case["bc4"])


def cell_world(s, case):
    """The object with its vertex, point, UV and face buffers placed so that vertex number `start` falls in the middle of each."""
    obj = new_object(s)
    start = case["start"]
    group = start >> 2
    vs = s.alloc(4 * 0x30 + 2 * GUARD, 0xA5)
    first_point = start & ~3
    ps = s.alloc(16 * 0xC + 2 * GUARD, 0xA5)
    uv = s.alloc(4 * 0x20 + 2 * GUARD, 0xA5)
    fc = s.alloc(8 * 8 + 2 * GUARD, 0xA5)
    st = obj + s.field("obj", "set")[1]
    s.put("set", st, "vsets", (vs + GUARD - group * 0x30) & s.mask)
    s.put("set", st, "points", (ps + GUARD - first_point * 0xC) & s.mask)
    s.put("set", st, "uvsets", (uv + GUARD - group * 0x20) & s.mask)
    s.put("obj", obj, "cursor", fc + GUARD + 8 * case.get("slot", 0))
    s.put("obj", obj, "nfacesdone", case.get("faces", 0))
    s.put("obj", obj, "nvertdone", start)
    s.put("obj", obj, "maxsq", 0x1234)
    return obj, dict(vs=(vs, 4 * 0x30 + 2 * GUARD), ps=(ps, 16 * 0xC + 2 * GUARD), uv=(uv, 4 * 0x20 + 2 * GUARD), fc=(fc, 8 * 8 + 2 * GUARD))


def dump_cell(s, obj, regions):
    fc = regions["fc"][0]
    return dict(
        vs=s.words(regions["vs"][0], "f", regions["vs"][1] // 4), ps=s.words(regions["ps"][0], "f", regions["ps"][1] // 4),
        uv=s.words(regions["uv"][0], "f", regions["uv"][1] // 4), faces=s.words(fc, "h", regions["fc"][1] // 2),
        cursor=s.get("obj", obj, "cursor") - fc, nfaces=s.get("obj", obj, "nfacesdone"), nverts=s.get("obj", obj, "nvertdone"))


def run_vertex(s, case):
    s.begin()
    set_globals(s, case)
    obj, regions = cell_world(s, case)
    point = 0
    if case["point"] is not None:
        point = s.alloc(0xC, 0xA5)
        for i, b in enumerate(case["point"]):
            s.wr(point + 4 * i, "f", b)
    s.vertex(obj, case["x"], case["y"], case["z"], case["u"], case["v"], case["follow"], point)
    return result(s, **dump_cell(s, obj, regions))


def run_square(s, case):
    s.begin()
    set_globals(s, case)
    obj, regions = cell_world(s, case)
    s.square(obj, case["x"], case["y"], case["z"], case["side"], case["rotation"], case["texture"], case["f4"], case["large"], case["u10"],
             case["keep"])
    return result(s, **dump_cell(s, obj, regions))


def base_cell(**k):
    uv = [fbits(float(v)) for v in (0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0.5, 0.5, 0, 0.25, 0.75, 0, 0.75, 0.25, 0, 0.1, 0.9, 0, 0.9, 0.1, 0, 0.5, 0.5, 0)]
    case = dict(start=0, slot=0, faces=0, x=fbits(12.5), y=fbits(3.0), z=fbits(-7.25), u=fbits(0.25), v=fbits(0.75), follow=0, point=None,
                da0=fbits(0.5), mask=0xFFF, scale=fbits(T.f32(4096 / 6.2831855)), table=1, uv=uv, mode=0, db50=0, db54=0, bd4=0,
                bc0=fbits(0.0), bc4=fbits(0.0), side=0, rotation=0xFF, texture=-1, f4=fbits(1.0), large=0, u10=0, keep=0)
    case["q"] = dict(height=[fbits(1.5)])
    case.update(k)
    return case


STARTS = [0, 1, 3, 4, 5, 7, 8, 100, 0xFFFC, 0xFFFF, 0x10000, 0x10005]


def vertex_edges():
    for follow in (0, 1, -1, 0x100):
        for start in STARTS:
            yield base_cell(start=start, follow=follow)
    for point in ((0x3F800000, 0x40000000, 0x40400000), (NAN, 0x80000000, 0x7F800000)):
        for start in (0, 5, 7):
            yield base_cell(start=start, follow=0, point=point)
    # the wave: the listing adds the phase to x first (z + (G + x)), and the table index masks a two's complement
    for da0 in (16777216.0, 1e8, 0.0, -3.5, 3.14159):
        for x, z in ((1.0, 1.0), (1.5, 0.5), (3.0, 1.0), (-1.0, 5.0), (0.1, 0.2)):
            yield base_cell(follow=1, x=fbits(T.f32(x)), z=fbits(T.f32(z)), da0=fbits(T.f32(da0)))
    for scale in (651.8986, 1.0, 1e9, -1e9, 0.0, NAN, -651.8986, 3e38, -1.0):
        scale_bits = scale if isinstance(scale, int) else fbits(T.f32(scale))
        for z in (0.0, 1.0, -1.0, 100.5, -100.5, 1e6, -1e6):
            yield base_cell(follow=1, scale=scale_bits, z=fbits(T.f32(z)), x=fbits(2.0))
    # the order of the additions: (z + G) + x and z + (G + x) round differently here, and with a scale of 1 the index shows it
    for da0 in (16777216.0, 8388608.0, 1e7, -16777216.0):
        for x, z in ((1.0, 2.0), (3.0, 2.0), (1.0, 6.0), (0.5, 2.0), (2.0, 1.0), (5.0, 3.0), (-1.0, -2.0)):
            for scale in (1.0, 0.25, 2.0):
                yield base_cell(follow=1, da0=fbits(T.f32(da0)), x=fbits(T.f32(x)), z=fbits(T.f32(z)), scale=fbits(scale))
    for mask in (0xFFF, 0x7FF, 0, 0xF, 1):
        yield base_cell(follow=1, mask=mask, z=fbits(-3.0))
        yield base_cell(follow=1, mask=mask, z=fbits(77.0))
    for h in (0, 0x80000000, NAN, 0x7F800000, 0xFF800000, 0x7F7FFFFF, 0x49800000):
        q = dict(height=[h])
        yield base_cell(follow=1, q=q)
        yield base_cell(follow=1, q=q, y=0x7F7FFFFF)
    for y in (0, 0x80000000, NAN, 0x7F800000, 0xFF800000, 0x4B000000):
        yield base_cell(follow=1, y=y)
        yield base_cell(follow=0, y=y, u=y, v=y)


def vertex_rnd(r):
    follow = r.choice([0, 0, 1, 1, -1, 0x100, r.getrandbits(32)])
    point = None if r.random() < 0.5 else tuple(fl(r) for _ in range(3))
    case = base_cell(start=r.choice(STARTS + [r.randint(0, 0x2000)]), follow=follow, point=point, x=fl(r), y=fl(r), z=fl(r), u=fl(r), v=fl(r),
                     da0=fl(r), mask=r.choice([0xFFF, 0x7FF, 0xFFF, 0x3FF, 0, 0xF, r.randint(0, 0xFFF)]), scale=r.choice([fl(r), fbits(T.f32(651.8986)),
                     fbits(T.f32(r.uniform(-1000.0, 1000.0)))]), table=r.randint(1, 4))
    case["q"] = dict(height=[fl(r)])
    return case


SIDES = (0, 1, 2, 3, 4, 5, 0x10, 0x40, 0x41, 0x80, 0xFF)
ROTATIONS = (0, 1, 2, 4, 0x10, 0x40, 0x41, 0x80, 0xFF)
TEXTURES = (-5, -2, -1, 0, 7, 8, 12, 13, 14, 15, 16, 0x7FFFFFFF, -0x80000000)


def square_edges():
    for side in SIDES:
        for rotation in ROTATIONS:
            yield base_cell(side=side, rotation=rotation, texture=-1)
            yield base_cell(side=side, rotation=rotation, texture=9, start=5)
    for side in (0, 1, 4, 0x10, 0x40):
        for texture in TEXTURES:
            yield base_cell(side=side, rotation=0, texture=texture, start=4, q=dict(height=[fbits(2.5), fbits(0.5)]))
    for keep in (0, 1, -1):
        for bd4 in (0, 1, -1):
            for texture in (-1, 5, 12, 13, 20):
                for mode in (4, 0xE, 0x13, 5, 0):
                    yield base_cell(side=0, texture=texture, keep=keep, bd4=bd4, mode=mode, db54=7, db50=-3, bc4=fbits(0.25), bc0=fbits(0.75), x=fbits(1.5),
                                    z=fbits(2.5))
    for large in (0, 1, -1, 7):
        yield base_cell(side=1, large=large)
        yield base_cell(side=0x10, large=large, start=2)
    for a in (0, 1, -1, 5, 0x1000000, 0x1000001, 0x7FFFFFFF, -0x80000000, 0x12345679):
        for b in (0, -1, 0x1000001, -0x80000000):
            for x in (0.0, 0.1, 100.0, 1e7):
                yield base_cell(mode=4, db54=a, db50=b, x=fbits(T.f32(x)), z=fbits(T.f32(x + 0.3)), bc4=fbits(0.5), bc0=fbits(-1.5))
    # An origin whose float value is odd and above 2^24 / 5 makes 10 * origin inexact (167772150 is not a float), so the fused
    # fnmsubs (x - 10 * origin rounded once) and an unfused multiply then subtract differ for a position of a few units.
    for a in (16777215, -16777215):
        for x in (-4.0, 4.0, -3.0):
            yield base_cell(mode=4, db54=a, db50=a, x=fbits(T.f32(x)), z=fbits(T.f32(x)), bc4=fbits(0.5), bc0=fbits(-1.5))
    for x in (0.1, 0.3, 123.456, 1e6 + 0.5, 99999.99):
        for a in (3, 7, 12345, 777777, 16777217, -9999):
            yield base_cell(mode=0xE, db54=a, db50=a + 1, x=fbits(T.f32(x)), z=fbits(T.f32(-x)), bc4=NAN if a == 3 else fbits(1.0))
    for start in STARTS + [0x20003, 0xFFFFFFFC]:
        for side in (0, 0x80):
            yield base_cell(side=side, start=start, slot=2, faces=0x7FFFFFFF)
    for f4 in (0, NAN, 0x7F800000):
        yield base_cell(f4=f4, u10=0xFF, side=0)
    yield base_cell(uv=[NAN] * 30, side=0)
    yield base_cell(uv=[0x7F800000] * 30, side=4, texture=8)


def square_rnd(r):
    texture = r.choice(list(TEXTURES) + [r.randint(-3, 20), r.randint(-3, 20)])
    case = base_cell(start=r.choice(STARTS + [r.randint(0, 0x2000)]), slot=r.randint(0, 3), faces=word(r), x=fl(r), y=fl(r), z=fl(r),
                     side=r.choice(SIDES + (0, 1, 4, 0x10, 0x40, r.randint(0, 255))), rotation=r.choice(ROTATIONS + (r.randint(0, 255),)),
                     texture=texture, f4=fl(r), large=r.choice([0, 1, -1, 7]), u10=r.randint(0, 255), keep=r.choice([0, 0, 0, 1, -1]),
                     bd4=r.choice([0, 0, 1, -1]), mode=r.choice([4, 0xE, 0x13, 0, 5, 4, word(r)]), db50=r.choice([word(r), r.randint(-50, 50)]),
                     db54=r.choice([word(r), r.randint(-50, 50)]), bc0=fl(r), bc4=fl(r), da0=fl(r),
                     mask=r.choice([0xFFF, 0x7FF, 0x3FF, 0, r.randint(0, 0xFFF)]), scale=r.choice([fl(r), fbits(T.f32(651.8986))]), table=r.randint(1, 4),
                     uv=[fl(r) if r.random() < 0.9 else NAN for _ in range(30)])
    case["q"] = dict(height=[fl(r) for _ in range(r.randint(1, 4))])
    return case


# ---------------------------------------------------------------------------------------------------------------- DMCreateMesh


def build_items(s, items):
    size = s.size("item")
    arr = s.alloc(size * max(len(items), 1), 0xA5)
    for i, item in enumerate(items):
        ia = arr + i * size
        s.put("item", ia, "flags", item["flags"])
        for name in ("nface", "nvert", "nuv", "nfaceset", "npoint"):
            s.put("item", ia, name, item[name])
        box = s.addr("item", ia, "box")
        for j, b in enumerate(item["box"]):
            s.wr(box + 4 * j, "f", b)
    return arr


def run_create(s, case):
    s.begin()
    items = case["items"]
    arr = build_items(s, items)
    st = s.alloc(s.size("set"), 0xA5, K_SET)
    s.put("set", st, "flags", case["set_flags"])
    s.put("set", st, "count", len(items))
    s.put("set", st, "entries", arr)
    tcase = dict(case["tex"])
    tex, _ = build_texture_set(s, tcase, K_SRCTEX)
    infos = 0
    if tcase["tflags"] & 1:
        infos = s.alloc(0x10 * max(len(tcase["names"]), 1), 0x5A, 0, arena=True)
        s.put("tex", tex, "infos", infos)
        s.put("tex", tex, "textures", s.alloc(8 * max(len(tcase["names"]), 1), 0x6B, 0, arena=True))
    name = s.cstring(case["name"]) if case["name"] is not None else 0
    answer = s.create(st, tex, name, case["flags"])
    size = s.size("item")
    state = dict(answer=answer, set=dump_set(s, st, tex), tex=dump_tex(s, tex),
                 items=[dump_item(s, arr + i * size) for i in range(len(items))], blocks=dump_blocks(s),
                 infos=s.bytes(infos, 0x10 * max(len(tcase["names"]), 1)) if infos else b"")
    return result(s, **state)


def item(flags=3, nface=2, nvert=4, nuv=4, nfaceset=1, npoint=0, box=None):
    return dict(flags=flags, nface=nface, nvert=nvert, nuv=nuv, nfaceset=nfaceset, npoint=npoint,
                box=box or [0x3F800000, 0x40000000, 0x40400000, 0x40800000, 0x40A00000, 0x40C00000])


def tex_case(names=("blue.tga", "red.tga"), flags=(7, 3), fresh=True, use=0, folder="Data:Generic:Dynamic:Textures", sfolder="Data:Levels:Hallow:Dynamic",
             hi=0):
    return dict(folder=folder, sfolder=sfolder, names=list(names), eflags=list(flags)[:len(names)] + [0] * (len(names) - len(flags)),
                tflags=(0 if fresh else 1) | hi, use=use)


def create_case(items=None, set_flags=0, tex=None, name="BlueprintMesh", flags=3, find=(0,)):
    return dict(items=items if items is not None else [item()], set_flags=set_flags, tex=tex or tex_case(), name=name, flags=flags,
                q=dict(find=list(find)))


def create_edges():
    yield create_case()
    for count in (0, 1, 2, 3, 5):
        for flags in (0, 1, 2, 3, 4, MASK):
            yield create_case(items=[item(nvert=3 + i, nface=i + 1, nuv=i + 2, nfaceset=i, npoint=i * 3) for i in range(count)], flags=flags)
    for flags in (0, 4, 5, MASK, 0x10):
        for nvert in (0, 1, 3, 4, 5, 8):
            yield create_case(items=[item(flags=flags, nvert=nvert), item(flags=flags ^ 4, nvert=nvert + 1)])
    for field in ("nface", "nvert", "nuv", "nfaceset", "npoint"):
        yield create_case(items=[item(**{field: 0})])
        yield create_case(items=[item(), item(**{field: 0})])
        yield create_case(items=[item(**{field: 0xFFF0}), item(**{field: 0x20})]) if field in ("nface", "nuv", "npoint") else create_case(items=[item(), item(**{field: 1})])
    # the sums wrap in 16 bits (the last entry's vertex count is not used to place another array)
    yield create_case(items=[item(nvert=0xFFFF), item(nvert=0x11)], flags=0)
    yield create_case(items=[item(nface=0x8001, nuv=0x8001, npoint=0xFFFF, flags=4), item(nface=0x8001, nuv=0x8003, npoint=1, flags=0)])
    for set_flags in (0, 0x1000, 0x8000, MASK, 0x2000, 8):
        yield create_case(set_flags=set_flags)
    for names, flags in ((("a.tga",), (0,)), (("a", "b", "c", "d"), (0, 1, 2, 3)), ((), ()), (("x" * 30,), (7,)), (("y" * 20,), (1,)), (("z" * 19,), (2,))):
        for fresh in (True, False):
            for use in (0, 1, MASK):
                yield create_case(tex=tex_case(names, flags, fresh, use), items=[item(nfaceset=max(len(names), 1))])
    for hi in (0x2, 0xFFFFFFFE, 0x80):
        yield create_case(tex=tex_case(hi=hi))
    for name in (None, "", "short", "x" * 19, "y" * 20, "z" * 31):
        yield create_case(name=name)
    for find in ((0,), (1,), (0, 1), (2,), (0, 0)):
        yield create_case(find=find)
    yield create_case(items=[item(nvert=0, nface=0, nuv=0, nfaceset=0, npoint=0)])
    yield create_case(items=[])


def rnd_item(r):
    return item(flags=r.choice([0, 4, 3, 7, r.getrandbits(32)]), nface=r.choice([0, r.randint(1, 40)]), nvert=r.choice([0, r.randint(1, 40)]),
                nuv=r.choice([0, r.randint(1, 40)]), nfaceset=r.choice([0, r.randint(1, 6)]), npoint=r.choice([0, r.randint(1, 40)]),
                box=[fl(r) for _ in range(6)])


def rnd_items(r):
    items = [rnd_item(r) for _ in range(r.randint(0, 5))]
    if len(items) >= 2 and r.random() < 0.15:
        # the 16 bit sums wrap: two entries' counts add up to a small number (the first entry's vertex count is left alone so that no
        # index is written past the block)
        field = r.choice(["nface", "nuv", "npoint"])
        small = r.randint(0, 20)
        items[0][field] = (0x10000 - r.randint(1, 30)) & 0xFFFF
        items[1][field] = (small + 0x10000 - items[0][field]) & 0xFFFF
    return items


def create_rnd(r):
    items = rnd_items(r)
    count = r.randint(0, 4)
    names = ["".join(r.choice("abcdef_.0") for _ in range(r.choice([0, 3, 12, 19, 20, 25, 30]))) for _ in range(count)]
    tex = tex_case(names, [r.randint(0, 3) for _ in range(count)], r.random() < 0.6, r.choice([0, 1, 2, MASK, r.getrandbits(32)]),
                   folder="".join(r.choice("abc:.D") for _ in range(r.randint(1, 30))), sfolder="".join(r.choice("abc:.") for _ in range(r.randint(0, 20))),
                   hi=r.choice([0, 0, 2, r.getrandbits(32) & ~1]))
    return create_case(items, r.choice([0, 0x1000, 0x8000, 0x2008, r.getrandbits(32)]), tex, r.choice([None, "", "Mesh", "M" * r.randint(1, 30)]),
                       r.choice([0, 1, 2, 3, 4, MASK, r.getrandbits(32)]), [r.randint(0, 2) for _ in range(r.randint(1, 3))])


# ---------------------------------------------------------------------------------------------------------------- the constructor


def run_construct(s, case):
    s.begin()
    s.set_theme(case["theme"])
    set_globals(s, case)
    entries = s.gaddr("entries")
    esize = s.size("entry")
    for i in range(20):
        name, flags = case["entries"][i]
        s.put("entry", entries + i * esize, "flags", flags)
        s.put("entry", entries + i * esize, "name", s.cstring(name))
    s.set_words(s.gaddr("path"), [0x5A5A5A5A] * 8)
    obj = new_object(s)
    ok = s.alloc(16, 0xEF)
    s.wr(ok, "w", 0xDEADBEEF)
    name = s.cstring(case["name"]) if case["name"] is not None else 0
    s.construct(obj, case["size_a"], case["size_b"], ok, name, case["type"], case["box"])
    state = dict(obj=dump_object(s, obj), ok=[s.rd(ok + 4 * i, "w") for i in range(4)], blocks=dump_blocks(s), path=s.text(s.gaddr("path")))
    return result(s, **state)


def construct_case(r=None, **k):
    case = base_cell()
    names = [f"tex{i}.tga" for i in range(20)]
    case.update(size_a=3, size_b=0x14, type=3, name="FlagsMesh", box=[0, fbits(-10.0), fbits(-200.0), fbits(1024.0), fbits(1024.0), fbits(1024.0)],
                theme="Hallow", entries=[(names[i], 7 if i < 15 else 3) for i in range(20)])
    case["q"] = dict(small=[0, 0], find=[0])
    case.update(k)
    return case


def construct_edges():
    yield construct_case()
    for size_a in (0, 1, 2, 3, 7, 0x20):
        for size_b in (0, 1, 2, 0x14):
            yield construct_case(size_a=size_a, size_b=size_b)
    for type_ in (0, 1, 3, 7, -1):
        yield construct_case(type=type_)
    for name in (None, "BlueprintMesh", "n" * 40, ""):
        yield construct_case(name=name)
    for small in ([0, 0], [1, 1], [1, 0], [0, 1], [0x100, 0x100], [0x100, 1], [0xFF00, 0x2], [0x1FF, 0x80000000]):
        yield construct_case(q=dict(small=small, find=[0]))
    for theme in ("", "jungle", "Lost Kingdom", "T" * 60):
        yield construct_case(theme=theme)
    for find in ([1], [0, 1], [0, 0], [2]):
        yield construct_case(q=dict(small=[0, 0], find=find))
    box = [0x80000000, NAN, 0x7F800000, 0xFF800000, 1, 0x7F7FFFFF]
    yield construct_case(box=box)
    yield construct_case(mode=4, db54=5, db50=-9, bc4=fbits(0.25), bc0=fbits(-0.5), size_a=4)
    yield construct_case(mode=0xE, db54=5, db50=-9, bd4=1, size_a=2)
    yield construct_case(size_a=5, size_b=3, entries=[(f"e{i}" * 12, i & 3) for i in range(20)])


def construct_rnd(r):
    case = construct_case(size_a=r.choice([0, 1, 2, 3, 7, r.randint(0, 0x30)]), size_b=r.choice([0, 1, 2, 0x14, r.randint(0, 20)]), type=r.choice([0, 1, 3, 7, -1, word(r)]),
                          name=r.choice([None, "", "BlueprintMesh", "n" * r.randint(1, 40)]),
                          box=[fl(r) for _ in range(6)], theme=r.choice(["", "jungle", "Hallow", "T" * r.randint(1, 60)]),
                          entries=[("".join(r.choice("abcdef_.0") for _ in range(r.choice([0, 3, 12, 19, 20, 25, 30]))), r.randint(0, 3)) for _ in range(20)],
                          mode=r.choice([4, 0xE, 0x13, 0, 5, word(r)]), db50=r.randint(-50, 50), db54=r.randint(-50, 50), bd4=r.choice([0, 0, 1]),
                          bc0=fl(r), bc4=fl(r), uv=[fl(r) if r.random() < 0.9 else NAN for _ in range(30)])
    case["q"] = dict(small=[r.choice([0, 1, 0x100, r.getrandbits(32)]) for _ in range(2)], find=[r.randint(0, 2) for _ in range(r.randint(1, 3))])
    return case


TESTS = [
    FnTest("LLCC_SystemGetMode", ppc_case(run_mode), host_case(run_mode), edge=mode_edges, rnd=mode_rnd),
    FnTest("CDynamicMesh::SetBoundingBox", ppc_case(run_bbox), host_case(run_bbox), edge=bbox_edges, rnd=bbox_rnd),
    FnTest("DMEnableMeshRender", ppc_case(run_enable), host_case(run_enable), edge=enable_edges, rnd=enable_rnd),
    FnTest("DMEnableMeshRender (places)", ppc_case(run_enable_xz), host_case(run_enable_xz), edge=enable_xz_edges, rnd=enable_xz_rnd),
    FnTest("CDynamicMesh::~CDynamicMesh", ppc_case(run_destroy), host_case(run_destroy), edge=destroy_edges, rnd=destroy_rnd),
    FnTest("CorrectDTexturePaths", ppc_case(run_correct), host_case(run_correct), edge=correct_edges, rnd=correct_rnd),
    FnTest("CDynamicMesh::DC_SetVertex", ppc_case(run_vertex), host_case(run_vertex), edge=vertex_edges, rnd=vertex_rnd),
    FnTest("CDynamicMesh::DC_CreateSquareFace", ppc_case(run_square), host_case(run_square), edge=square_edges, rnd=square_rnd),
    FnTest("DMCreateMesh", ppc_case(run_create), host_case(run_create), edge=create_edges, rnd=create_rnd),
    FnTest("CDynamicMesh::CDynamicMesh", ppc_case(run_construct), host_case(run_construct), edge=construct_edges, rnd=construct_rnd),
]
