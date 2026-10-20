"""Differential test for the lobby's small leaves (unit w58), against libtpw_unit_LobbyLeaves2.dylib (tests/unit_LobbyLeaves2.cpp):

  src/rsse_normals.cpp  RecalculateInvalidNodeFaceNormals 0x100c7224, RecalculateInvalidFaceNormals 0x100c7418
  src/rsse_rotate.cpp   XRotateLMatrixNoTrans 0x100c13bc, YRotateLMatrixNoTrans 0x100c1698, ZRotateLMatrixNoTrans 0x100c15f0,
                        RotateLMatrixXYZNoTrans 0x100c174c, RotateLMatrixXYZNoTransHi 0x100c1944
  src/bfl_tbrect.cpp    TbRect::Bounding 0x1002a110 (the bullfrog library's program, not the main app's)
  src/ui_main_meshes.cpp  DeInitGeneralUI 0x10185d64, UIInterface::DeInitGeneral 0x101ef64c

The functions run as the original on the PowerPC side. The normals and the rotations call nothing but the C library (sqrt, cos, sin, and
the class code function __fpclassifyf, which stubs here: NaN is class 1), whose host side is the host's libm: the cos and sin stubs are
Python's math.cos and math.sin, which are the host's libm too, so the Hi variant is tested against the same library (see the notes: the
Mac's own cos and sin are not run). The face records and the nodes have the mesh file's layout on both sides (Ptr32 fields: absolute
addresses on the PowerPC side, offsets from the arena's base on the host); the dump after a case holds every word of the node and of the
header except their pointers, the points, the vertices, the faces and the remap table.

Unicorn computes the single-precision fused instructions (fmadds, fmsubs) in double precision and rounds twice; HardwareRounding (one
code hook per fused instruction, as in test_RideAnimTick.py) gives each the processor's single rounding. The normals use the double
precision fmsub, which Unicorn computes with one rounding already.

The end of the interface: the cursor table's DeAllocate and the font's Release run for real on both sides (the cursor table holds no
cursors here: its entries are all NULL); UI_CloseSystem, InterfaceStringHeap::DeAllocate and HFreeHeap are fakes that record how far the
two real calls have got (delete[] blocks so far, the table's count, whether the font has a bitmap) as well as their arguments, so that the
order of all six calls shows; the three windows are fake objects on both sides whose destructor and delete record.
"""
import ctypes as C
import math
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyLeaves2"
BULLFROG = "bullfrog symbols shared"

MASK = 0xFFFFFFFF
NAN_BITS = 0x7FC00000

NODE_NORMALS, HEADER_NORMALS = 0x100C7224, 0x100C7418
X_ROTATE, Y_ROTATE, Z_ROTATE = 0x100C13BC, 0x100C1698, 0x100C15F0
ROTATE_XYZ, ROTATE_XYZ_HI = 0x100C174C, 0x100C1944
BOUNDING = 0x1002A110
DEINIT_UI, DEINIT_GENERAL = 0x10185D64, 0x101EF64C
UI_CLOSE_SYSTEM, STRING_HEAP_DEALLOCATE, HFREE_HEAP = 0x101C8C2C, 0x101D7978, 0x1005B774
CURSOR_DEALLOCATE, FONT_RELEASE = 0x101CCA38, 0x10168FF8
FPCLASSIFY = "__fpclassifyf"

TPGLOBALS = 0x103B74C8
WINDOW_GLOBALS = (0x10380B88, 0x10380B84, 0x10380B80)    # A, B, C
CURSOR_TABLE, UI_HEAP, FONT = 0x1038CC64, 0x1038CC60, 0x1037F1A0

INF, NAN = math.inf, math.nan
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF


# ------------------------------------------------------------------ small helpers


def fb(x):
    """The bit pattern of a single-precision value (a NaN as one pattern)."""
    if x != x:
        return NAN_BITS
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bf(bits):
    """The single-precision value of a bit pattern."""
    return struct.unpack(">f", struct.pack(">I", bits & MASK))[0]


def canon(bits):
    """A word with every NaN pattern made one (the processors differ in the NaNs they make)."""
    bits &= MASK
    return NAN_BITS if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def f32(x):
    return T.f32(x)


class Globals:
    """Keeps the original's globals a case writes and puts them back when it is over."""

    def __init__(self, e):
        self.e = e
        self.kept = []

    def keep(self, address, size):
        self.kept.append((address, self.e.read(address, size)))

    def close(self):
        for address, data in reversed(self.kept):
            self.e.write(address, data)


LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Two things Unicorn (QEMU 5.0) does not execute as the processor does, corrected for the code under test and nowhere else (as in
    test_FrameStandins.py):
    - fmadds, fmsubs, fnmadds and fnmsubs are computed in double precision and rounded to single, which can differ by one unit in the
      last place from the processor's single rounding of the exact result; the result is replaced by the C library's fmaf;
    - fctiwz of a value above INT32_MAX (or +infinity) gives 0x80000000; the processor saturates to 0x7fffffff.
    A code hook looks at each instruction before it runs and rewrites its target register just before the next one. ranges: the
    (begin, end) address ranges of the functions under test."""

    changed = 0    # results the correction replaced (all cases so far), for the notes
    seen = 0       # fused instructions it looked at

    def __init__(self, e, ranges):
        self.e, self.pending, self.hooks, self.ranges = e, None, [], ranges

    def __enter__(self):
        for begin, end in self.ranges:
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
                HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], (fpr(e, target) & 0xFFFFFFFF00000000) | 0x7FFFFFFF)
            else:
                corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
                if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                    HardwareRounding.changed += 1
                uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        if word >> 26 == 63 and (word >> 1) & 0x3FF == 15:          # fctiwz frD, frB
            if fpr_double(e, rb) >= 2147483648.0:
                self.pending = ("fctiwz", rd, None)
            return
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        HardwareRounding.seen += 1
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = ("fused", rd, value)


def function_range(e, address):
    """(begin, end) of the function at `address` in the program's function table."""
    for f in e.functions:
        if f["entry"] == address:
            return address, address + f["size"]
    raise KeyError(hex(address))


def rotate_ranges(e):
    """The three single-precision rotation functions, which hold every fused instruction of the rotations, and the walk's own (its
    fctiwz)."""
    return [function_range(e, a) for a in (X_ROTATE, Y_ROTATE, Z_ROTATE, ROTATE_XYZ)]


# ------------------------------------------------------------------ the two sides' memory


class PpcMem:
    """The PowerPC's memory: absolute addresses, big-endian words, blocks poisoned with 0xa5."""

    def __init__(self, e):
        self.e = e

    def alloc(self, size):
        return self.e.alloc(size, b"\xa5" * size)

    def ptr(self, address):
        return address

    def w(self, address, fmt, *values):
        self.e.write(address, struct.pack(">" + fmt, *values))

    def r(self, address, fmt):
        return struct.unpack(">" + fmt, self.e.read(address, struct.calcsize(">" + fmt)))


class HostMem:
    """The host's memory: arena blocks, little-endian words, Ptr32 offsets from the arena's base."""

    def __init__(self, lib):
        self.lib = lib
        lib.tpw_ll_reset()
        lib.tpw_ll_alloc(16)
        self.base = lib.tpw_ll_base()

    def alloc(self, size):
        address = self.lib.tpw_ll_alloc(size)
        C.memset(address, 0xA5, size)
        return address

    def ptr(self, address):
        return address - self.base if address else 0

    def w(self, address, fmt, *values):
        data = struct.pack("<" + fmt, *values)
        C.memmove(address, data, len(data))

    def r(self, address, fmt):
        return struct.unpack("<" + fmt, C.string_at(address, struct.calcsize("<" + fmt)))


# ------------------------------------------------------------------ the mesh records


def build_mesh(m, spec):
    """Lays out the case's header, nodes, vertices, faces, points and remap tables; answers the addresses."""
    nodes = spec["nodes"]
    header = m.alloc(0xB8)
    nodes_address = m.alloc(0xA0 * max(1, len(nodes)))
    infos = []
    for i, nd in enumerate(nodes):
        node = nodes_address + 0xA0 * i
        verts = m.alloc(0x30 * max(1, len(nd["verts"])))
        for s, values in enumerate(nd["verts"]):
            m.w(verts + 0x30 * s, "12f", *values)
        points = m.alloc(12 * max(1, nd["npoints"]))
        faces = m.alloc(8 * max(1, len(nd["faces"])))
        for k, face in enumerate(nd["faces"]):
            m.w(faces + 8 * k, "4H", *face)
        remap = m.alloc(2 * max(1, len(nd["remap"])))
        if nd["remap"]:
            m.w(remap, "%dH" % len(nd["remap"]), *nd["remap"])
        m.w(node, "I", nd["flags"])
        m.w(node + 0x5C, "H", nd["count"])
        m.w(node + 0x60, "I", m.ptr(verts))
        m.w(node + 0x64, "I", m.ptr(points))
        m.w(node + 0x70, "I", m.ptr(faces))
        m.w(node + 0x94, "I", m.ptr(remap))
        infos.append((node, verts, points, faces, remap))
    m.w(header + 0x30, "I", spec["hflags"])
    m.w(header + 0x44, "H", spec.get("hcount", len(nodes)))
    m.w(header + 0x70, "I", m.ptr(nodes_address))
    return header, infos


def dump_mesh(m, spec, header, infos):
    """Every word the functions could have changed, the pointers aside."""
    words = [canon(w) for w in m.r(header, "46I")]
    words[0x70 // 4] = 0
    words[0x44 // 4] = m.r(header + 0x44, "H")[0] | (m.r(header + 0x46, "H")[0] << 16)    # (a word with halves written is two halves)
    out = {"header": words, "nodes": []}
    for nd, (node, verts, points, faces, remap) in zip(spec["nodes"], infos):
        node_words = [canon(w) for w in m.r(node, "40I")]
        for offset in (0x60, 0x64, 0x70, 0x94):
            node_words[offset // 4] = 0
        node_words[0x5C // 4] = m.r(node + 0x5C, "H")[0] | (m.r(node + 0x5E, "H")[0] << 16)
        node_words[0x58 // 4] = m.r(node + 0x58, "H")[0] | (m.r(node + 0x5A, "H")[0] << 16)
        out["nodes"].append({
            "node": node_words,
            "points": [canon(w) for w in m.r(points, "%dI" % (3 * max(1, nd["npoints"])))],
            "verts": [canon(w) for w in m.r(verts, "%dI" % (12 * max(1, len(nd["verts"]))))],
            "faces": list(m.r(faces, "%dH" % (4 * max(1, len(nd["faces"]))))),
            "remap": list(m.r(remap, "%dH" % max(1, len(nd["remap"])))),
        })
    return out


# ------------------------------------------------------------------ the PowerPC fakes


STUBS = None


def make_stubs():
    """The C library's class function (NaN is class 1, the others are some other code) and the real functions of the unit; the end of the
    interface's fakes (they record into the case's log, see deinit_ints)."""
    S = CL.Stubs()
    S.real.update([NODE_NORMALS, HEADER_NORMALS, X_ROTATE, Y_ROTATE, Z_ROTATE, ROTATE_XYZ, ROTATE_XYZ_HI, BOUNDING, DEINIT_UI,
                   DEINIT_GENERAL, CURSOR_DEALLOCATE, FONT_RELEASE])

    def classify(e):
        x = e.f(1)
        e.set_r(3, 1 if x != x else (2 if math.isinf(x) else (3 if x == 0.0 else 4)))

    S.imp(FPCLASSIFY, classify)
    S.internal(UI_CLOSE_SYSTEM, lambda e: e.cl.rec("UI_CloseSystem", deinit_ints(e)))
    S.internal(STRING_HEAP_DEALLOCATE, lambda e: e.cl.rec("StringHeap.DeAllocate", deinit_ints(e)))

    def free_heap(e):
        e.cl.rec("HFreeHeap", [e.cl.lab(e.r(3))] + deinit_ints(e))
        e.set_r(3, 1)

    S.internal(HFREE_HEAP, free_heap)
    return S


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = make_stubs()
    lib = runner.lib
    lib.tpw_ll_base.restype = C.c_uint64
    lib.tpw_ll_alloc.restype = C.c_uint64
    lib.tpw_ll_alloc.argtypes = [C.c_uint32]
    lib.tpw_ll_normals_node.argtypes = [C.c_void_p]
    lib.tpw_ll_normals_header.argtypes = [C.c_void_p]
    lib.tpw_ll_rotate.argtypes = [C.c_int32, C.c_void_p, C.c_float, C.c_float]
    lib.tpw_ll_rotate_xyz.argtypes = [C.c_int32, C.c_void_p, C.c_int32, C.c_void_p]
    lib.tpw_ll_set_sin.argtypes = [C.c_uint32, C.c_uint32, C.c_float, C.c_void_p]
    lib.tpw_ll_bounding.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p]
    lib.tpw_ll_window.restype = C.c_uint64
    lib.tpw_ll_window.argtypes = [C.c_int32]
    lib.tpw_ll_set_cursors.argtypes = [C.c_int32, C.c_uint32]
    lib.tpw_ll_get_cursors.argtypes = [C.c_void_p]
    lib.tpw_ll_set_windows.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_ll_get_windows.argtypes = [C.c_void_p]
    lib.tpw_ll_set_heap.argtypes = [C.c_void_p]
    lib.tpw_ll_get_heap.restype = C.c_uint64
    lib.tpw_ll_set_font.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, C.c_int32, C.c_int32]
    lib.tpw_ll_get_font.argtypes = [C.c_void_p]
    lib.tpw_ll_deinit.argtypes = [C.c_int32]


class Ppc:
    """Runs one case's PowerPC half: the stubs on, the globals the case writes put back afterwards."""

    def __init__(self, e, ranges=()):
        self.e, self.ranges, self.g = e, ranges, Globals(e)

    def __enter__(self):
        self.saved = STUBS.activate(self.e)
        self.rounding = HardwareRounding(self.e, self.ranges)
        return self

    def call(self, entry, args):
        with self.rounding:
            return T.call_ppc(self.e, entry, args)

    def __exit__(self, *exc):
        CL.Stubs.deactivate(self.e, self.saved)
        self.g.close()


# ------------------------------------------------------------------ the face normals


def fl(x):
    return f32(x)


def vertex_value(r):
    k = r.random()
    if k < 0.45:
        return fl(r.uniform(-10, 10))
    if k < 0.58:
        return fl(r.choice([0.0, 1.0, -1.0, 0.5, 2.0, -3.0]))
    if k < 0.66:
        return fl(r.uniform(-1000, 1000))
    if k < 0.74:
        return fl(r.uniform(-1, 1) * 1e-20)      # the squares of the normal underflow in single precision
    if k < 0.80:
        return fl(r.uniform(-1, 1) * 1e19)       # the squares are near the largest single
    if k < 0.85:
        return fl(r.uniform(-1, 1) * 1e30)       # the cross products overflow to infinity
    if k < 0.89:
        return r.choice([NAN, INF, -INF])
    return float(round(r.uniform(-4, 4)))        # whole numbers: degenerate and collinear faces are common


def vertex_sets(r, n):
    out = []
    for _ in range(n):
        out.append([vertex_value(r) for _ in range(12)])
    return out


def random_node(r, flags=None):
    nsets = r.randint(1, 3)
    nfaces = r.randint(0, 4)
    remap_size = r.randint(1, 8)
    remap = [r.randint(0, 4 * nsets - 1) for _ in range(remap_size)]
    npoints = r.randint(1, 5)
    faces = []
    for _ in range(nfaces):
        slots = [r.randint(0, remap_size - 1) for _ in range(3)]
        if r.random() < 0.15:
            slots[r.randint(0, 2)] = slots[r.randint(0, 2)]    # two equal corners
        faces.append((r.randint(0, 2 * npoints - 1) if r.random() < 0.9 else r.randint(0, 2 * npoints - 1) | 1,) + tuple(slots))
    return {"flags": r.choice([0x10000, 0x10000 | r.getrandbits(32), r.getrandbits(32), 0]) if flags is None else flags,
            "faces": faces, "remap": remap, "verts": vertex_sets(r, nsets), "npoints": npoints,
            "count": nfaces if r.random() < 0.85 else r.randint(0, nfaces)}


def node_case(nodes, op="node", hflags=0x40000, hcount=None):
    spec = {"op": op, "nodes": nodes, "hflags": hflags}
    if hcount is not None:
        spec["hcount"] = hcount
    return spec


def nd(faces, remap, verts, flags=0x10000, npoints=None, count=None):
    return {"flags": flags, "faces": faces, "remap": remap, "verts": verts,
            "npoints": npoints if npoints is not None else max([f[0] >> 1 for f in faces] + [0]) + 1,
            "count": len(faces) if count is None else count}


def set_of(points):
    """A vertex set from up to four (x, y, z) points."""
    pts = list(points) + [(0.0, 0.0, 0.0)] * (4 - len(points))
    return [p[0] for p in pts] + [p[1] for p in pts] + [p[2] for p in pts]


def normal_edges():
    unit = set_of([(0, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1)])
    skew = set_of([(1.5, -2.0, 0.25), (3.0, 1.0, -4.0), (-2.5, 0.5, 2.0), (0.125, 7.0, -1.0)])
    flat = set_of([(0, 0, 0), (1, 1, 1), (2, 2, 2), (0, 0, 0)])
    other = set_of([(5, 5, 5), (6, 5, 5), (5, 7, 5), (5, 5, 9)])
    cases = []
    ident = [0, 1, 2, 3]
    # one face, every ordering of the corners of the first set (lanes 0 to 3)
    for slots in [(0, 1, 2), (1, 2, 0), (2, 0, 1), (0, 2, 1), (1, 0, 2), (2, 1, 0), (0, 1, 3), (3, 2, 1), (3, 0, 2)]:
        cases.append(node_case([nd([(0,) + slots], ident, [unit])]))
        cases.append(node_case([nd([(0,) + slots], ident, [skew])]))
    # the face index: the flag bit and the high bits, with other faces that collide
    cases.append(node_case([nd([(1, 0, 1, 2)], ident, [unit])]))
    cases.append(node_case([nd([(6, 0, 1, 2)], ident, [unit], npoints=5)]))
    cases.append(node_case([nd([(2, 0, 1, 2), (3, 2, 1, 0)], ident, [unit, skew], npoints=3)]))
    cases.append(node_case([nd([(4, 0, 1, 2), (4, 1, 2, 3), (5, 3, 2, 1)], ident, [skew], npoints=4)]))
    # zero, one to four faces over three sets, and a remap that permutes across the sets
    remap = [11, 4, 9, 0, 6, 2, 8, 1, 10, 3, 5, 7]
    for count in range(5):
        faces = [(2 * k, (3 * k) % 12, (3 * k + 1) % 12, (3 * k + 2) % 12) for k in range(count)]
        cases.append(node_case([nd(faces, remap, [unit, skew, other], npoints=max(count, 1))]))
    # the count of faces is the node's own (fewer than the table holds)
    faces = [(0, 0, 1, 2), (2, 1, 2, 3), (4, 3, 2, 0)]
    cases.append(node_case([nd(faces, ident, [skew], npoints=3, count=1)]))
    cases.append(node_case([nd(faces, ident, [skew], npoints=3, count=0)]))
    # degenerate faces: a repeated corner (two of the three), all three the same, a flat set
    for slots in [(0, 0, 1), (0, 1, 1), (0, 1, 0), (2, 2, 2), (0, 1, 3)]:
        cases.append(node_case([nd([(0,) + slots], ident, [unit])]))
    cases.append(node_case([nd([(0, 0, 1, 2), (2, 1, 2, 3)], ident, [flat], npoints=2)]))
    # sets with a NaN, an infinity and huge or tiny numbers
    big = set_of([(1e30, 0, 0), (0, 1e30, 0), (0, 0, 1e30), (-1e30, 1e30, -1e30)])
    tiny = set_of([(0, 0, 0), (1e-20, 0, 0), (0, 1e-20, 0), (1e-20, 1e-20, 1e-20)])
    mid = set_of([(0, 0, 0), (1e19, 0, 0), (0, 1e19, 0), (3e18, 1e19, 2e19)])
    nan = set_of([(0, 0, 0), (NAN, 0, 0), (0, 1, 0), (0, 0, 1)])
    inf = set_of([(0, 0, 0), (INF, 0, 0), (0, 1, 0), (0, 0, 1)])
    for s in (big, tiny, mid, nan, inf):
        for slots in [(0, 1, 2), (0, 1, 3), (1, 2, 3)]:
            cases.append(node_case([nd([(0,) + slots], ident, [s])]))
    # the node's flags: with and without the bit, other bits kept
    for flags in (0, 0x10000, 0xFFFFFFFF, 0x10001, 0xFFFEFFFF, 0x40000, 0x7FFFFFFF):
        cases.append(node_case([nd([(0, 0, 1, 2)], ident, [unit], flags=flags)]))
    # the header: nodes with and without the bit, no node, the flag cleared, more nodes than the count
    two = [nd([(0, 0, 1, 2)], ident, [unit], flags=0x10000), nd([(0, 0, 1, 2)], ident, [skew], flags=0)]
    three = two + [nd([(0, 3, 2, 1), (2, 1, 0, 2)], ident, [skew], flags=0x7FFFFFFF, npoints=2)]
    for hflags in (0x40000, 0, 0xFFFFFFFF, 0xFFFBFFFF, 0x40001):
        cases.append(node_case(three, "header", hflags))
        cases.append(node_case([], "header", hflags, hcount=0))
    cases.append(node_case(three, "header", 0x40000, hcount=1))
    cases.append(node_case(three, "header", 0x40000, hcount=2))
    cases.append(node_case([two[1]], "header", 0x40000))
    # a mesh with many faces (the face index reaches into the second half of the points)
    many = [(2 * k, k % 4, (k + 1) % 4, (k + 2) % 4) for k in range(40)]
    cases.append(node_case([nd(many, ident, [skew], npoints=40)]))
    cases.append(node_case([nd([(0xFFFE, 0, 1, 2)], ident, [unit], npoints=0x8000)]))
    # general faces: four faces over three sets of ordinary numbers, a permuting remap (the squares and the sum are rounded in the
    # listing's way, which only shows with numbers that are not whole)
    g = random.Random(7)
    for k in range(150):
        scale = g.choice([1.0, 0.1, 10.0, 100.0, 1e-3])
        sets = [[fl(g.uniform(-1, 1) * scale) for _ in range(12)] for _ in range(3)]
        perm = list(range(12))
        g.shuffle(perm)
        faces = [(2 * f, g.randint(0, 11), g.randint(0, 11), g.randint(0, 11)) for f in range(4)]
        cases.append(node_case([nd(faces, perm, sets, npoints=4)], "node" if k % 3 else "header"))
    return cases


def random_normals(r, op):
    nodes = [random_node(r) for _ in range(r.randint(1, 3))]
    hflags = r.choice([0x40000, 0, r.getrandbits(32), 0x40000 | r.getrandbits(32)])
    hcount = None if r.random() < 0.9 else r.randint(0, len(nodes))
    return node_case(nodes, op, hflags, hcount)


def run_normals_ppc(e, case):
    m = PpcMem(e)
    with Ppc(e) as run:
        header, infos = build_mesh(m, case)
        if case["op"] == "node":
            run.call(NODE_NORMALS, [infos[0][0]])
        else:
            run.call(HEADER_NORMALS, [header])
        return dump_mesh(m, case, header, infos)


def run_normals_host(lib, case):
    m = HostMem(lib)
    header, infos = build_mesh(m, case)
    if case["op"] == "node":
        lib.tpw_ll_normals_node(infos[0][0])
    else:
        lib.tpw_ll_normals_header(header)
    return dump_mesh(m, case, header, infos)


def normal_cases(op):
    return lambda: [c for c in normal_edges() if c["op"] == op]


# ------------------------------------------------------------------ the rotations


def random_float(r, span=4.0):
    k = r.random()
    if k < 0.65:
        return fl(r.uniform(-span, span))
    if k < 0.75:
        return fl(r.choice([0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0]))
    if k < 0.83:
        return fl(r.uniform(-1, 1) * 1e-3)
    if k < 0.88:
        return fl(r.uniform(-1, 1) * 1e30)
    if k < 0.92:
        return fl(r.uniform(-1, 1) * 1e-30)
    if k < 0.95:
        return fl(r.uniform(-1, 1) * 1e-42)      # denormals
    return r.choice([NAN, INF, -INF])


def random_matrix(r):
    return [fb(random_float(r)) for _ in range(16)]


def identity():
    return [fb(1.0 if i % 5 == 0 else 0.0) for i in range(16)]


def matrix_of(values):
    return [fb(v) for v in values]


ROOT_THREE_HALF = fl(0.8660254037844386)
CS_PAIRS = [(1.0, 0.0), (0.0, 1.0), (0.5, ROOT_THREE_HALF), (ROOT_THREE_HALF, 0.5), (-1.0, 0.0), (0.0, -1.0), (-0.5, ROOT_THREE_HALF),
            (0.5, -ROOT_THREE_HALF), (fl(0.6), fl(0.8)), (fl(-0.28), fl(0.96)), (fl(0.123456), fl(-0.987654)), (2.0, 3.0), (0.0, 0.0),
            (NAN, 0.5), (0.5, INF)]


def rotate_edges(which):
    def gen():
        r = random.Random(100 + which)
        matrices = [identity(), matrix_of([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]),
                    matrix_of([fl(0.1 * k - 0.7) for k in range(16)]), matrix_of([-1] * 16), matrix_of([0] * 16),
                    matrix_of([1e30, 1e30, 1e30, 0, 1e30, 1e30, 1e30, 0, 1e30, 1e30, 1e30, 0, 1, 2, 3, 1])]
        for matrix in matrices:
            for c, s in CS_PAIRS:
                yield {"which": which, "matrix": matrix, "c": c, "s": s}
        for _ in range(20):
            yield {"which": which, "matrix": random_matrix(r), "c": CS_PAIRS[r.randint(0, 11)][0], "s": CS_PAIRS[r.randint(0, 11)][1]}
    return gen


def random_rotation(which):
    def gen(r):
        return {"which": which, "matrix": random_matrix(r), "c": random_float(r, 1.5), "s": random_float(r, 1.5)}
    return gen


def ppc_rotate(e, case):
    entry = (X_ROTATE, Y_ROTATE, Z_ROTATE)[case["which"]]
    with Ppc(e, rotate_ranges(e)) as run:
        matrix = e.alloc(64, struct.pack(">16I", *case["matrix"]))
        run.call(entry, [matrix, T.F(case["c"]), T.F(case["s"])])
        return [canon(w) for w in struct.unpack(">16I", e.read(matrix, 64))]


def host_rotate(lib, case):
    words = (C.c_uint32 * 16)(*case["matrix"])
    lib.tpw_ll_rotate(case["which"], C.addressof(words), case["c"], case["s"])
    return [canon(w) for w in words]


# the order walk


ORDER_NAMES = ["FUK", "X", "Y", "Z", "XY", "YX", "YZ", "ZY", "XZ", "ZX", "XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"]
TWO_PI = 2 * math.pi
ANGLE_SETS = [(0.0, 0.0, 0.0), (0.5, 1.0, -1.0), (7.0, -7.0, 100.0), (1e10, -1e10, 3.0), (INF, -INF, 1.0), (NAN, 0.5, 0.25),
              (1.5707964, 3.1415927, -1.5707964), (-0.0, 6.2831855, -6.2831855), (0.7853982, -0.7853982, 2.3561945)]
PINNED = [0.0, 0.7853982, 1.5707964, 2.3561945, 3.1415927, -0.7853982, -1.5707964, -3.1415927, 6.2831855, 0.5, 0.25, 0.125, -0.5, 1.0,
          2.0, 0.1, 1e-3, 1e-6, 100.0, -100.0]


def sine_table(r, size):
    """A table that tells its entries apart: random values, so that a wrong index shows."""
    return [fb(fl(r.uniform(-1.5, 1.5))) for _ in range(size)]


def walk_case(r, order, angles, size=None, hi=0):
    size = size if size is not None else r.choice([16, 64, 4096])
    return {"hi": hi, "order": order, "angles": [fb(fl(a)) for a in angles], "matrix": random_matrix(r) if r.random() < 0.7 else identity(),
            "size": size, "scale": fb(fl(size / TWO_PI if r.random() < 0.8 else r.uniform(0.5, 700.0))), "table": sine_table(r, size)}


def walk_edges(hi):
    def gen():
        r = random.Random(500 + hi)
        for order in range(16):
            for angles in ANGLE_SETS:
                yield walk_case(r, order, angles, size=r.choice([16, 64]), hi=hi)
        for size in (16, 64, 4096):
            for order in (2, 4, 10):
                for angles in ANGLE_SETS[:4]:
                    yield walk_case(r, order, angles, size=size, hi=hi)
        if hi:
            for angle in PINNED:
                yield walk_case(r, 10, (angle, -angle, angle * 0.5), size=16, hi=hi)
        else:
            # triage 2026-10-13a (doubtful 7): angles within a few ulps of the points where the sine index (angle * scale) and the cosine
            # index ((pi/2 + angle) * scale, a single precision sum then product) step to the next integer
            def step(value, ulps):
                bits = struct.unpack(">I", struct.pack(">f", value))[0]
                return struct.unpack(">f", struct.pack(">I", (bits + ulps) & 0xFFFFFFFF))[0]
            for size in (16, 64):
                scale = fl(size / TWO_PI)
                for k in range(-6, 12, 2):
                    for base in (k / scale, k / scale - math.pi / 2):
                        for ulps in range(-2, 3):
                            a = step(fl(base), ulps)
                            yield walk_case(r, 10, (a, step(a, 1), step(a, -1)), size=size, hi=hi)
    return gen


def random_walk(hi):
    def gen(r):
        angles = [r.choice([r.uniform(-8, 8), r.uniform(-1, 1), r.uniform(-1000, 1000), r.choice(PINNED)]) for _ in range(3)]
        if r.random() < 0.06:
            angles[r.randint(0, 2)] = r.choice([1e10, -1e10, 3e9, -3e9, INF, -INF, NAN])
        return walk_case(r, r.randint(0, 15), angles, hi=hi)
    return gen


def ppc_walk(e, case):
    with Ppc(e, rotate_ranges(e)) as run:
        run.g.keep(TPGLOBALS, 0xC + 4 * case["size"])
        e.write(TPGLOBALS, struct.pack(">3I", case["size"], case["size"] - 1, case["scale"]))
        e.write(TPGLOBALS + 0xC, struct.pack(">%dI" % case["size"], *case["table"]))
        matrix = e.alloc(64, struct.pack(">16I", *case["matrix"]))
        angles = e.alloc(16, struct.pack(">3I", *case["angles"]) + b"\xa5" * 4)
        run.call(ROTATE_XYZ_HI if case["hi"] else ROTATE_XYZ, [matrix, case["order"], angles])
        return {"matrix": [canon(w) for w in struct.unpack(">16I", e.read(matrix, 64))],
                "angles": list(struct.unpack(">3I", e.read(angles, 12)))}


def host_walk(lib, case):
    table = (C.c_uint32 * case["size"])(*case["table"])
    lib.tpw_ll_set_sin(case["size"], case["size"] - 1, bf(case["scale"]), C.addressof(table))
    words = (C.c_uint32 * 16)(*case["matrix"])
    angles = (C.c_uint32 * 4)(*case["angles"], 0xA5A5A5A5)
    lib.tpw_ll_rotate_xyz(case["hi"], C.addressof(words), case["order"], C.addressof(angles))
    return {"matrix": [canon(w) for w in words], "angles": list(angles)[:3]}


# ------------------------------------------------------------------ TbRect::Bounding


INTERVALS = [(0, 4), (2, 6), (4, 8), (5, 7), (0, 0), (3, 3), (6, 2), (-2, 1), (INT_MIN, INT_MAX), (INT_MAX, INT_MIN), (-1, 0), (-9, -3),
             (INT_MIN, INT_MIN), (INT_MAX, INT_MAX), (1, 0)]


def bounding_edges():
    fixed = [(INTERVALS[0], INTERVALS[1]), (INTERVALS[0], INTERVALS[0]), (INTERVALS[6], INTERVALS[3]), (INTERVALS[9], INTERVALS[8]),
             (INTERVALS[1], INTERVALS[2]), (INTERVALS[11], INTERVALS[7])]
    for x in INTERVALS:
        for x2 in INTERVALS:
            for y, y2 in fixed:
                yield [[x[0], y[0], x[1], y[1]], [x2[0], y2[0], x2[1], y2[1]]]
    # the same on the other axis, so that left and top, right and bottom are told apart
    for y in INTERVALS:
        for y2 in INTERVALS:
            for x, x2 in fixed[:4]:
                yield [[x[0], y[0], x[1], y[1]], [x2[0], y2[0], x2[1], y2[1]]]


def coord(r, span):
    k = r.random()
    if k < 0.6:
        return r.randint(-span, span)
    if k < 0.8:
        return r.randint(-1000, 1000)
    return r.choice([INT_MIN, INT_MAX, -1, 0, 1, INT_MIN + 1, INT_MAX - 1])


def random_rects(r):
    return [[coord(r, 6) for _ in range(4)], [coord(r, 6) for _ in range(4)]]


def ppc_bounding(e, case):
    a = e.alloc(16, struct.pack(">4i", *case[0]))
    b = e.alloc(16, struct.pack(">4i", *case[1]))
    out = e.alloc(16, b"\xa5" * 16)
    T.call_ppc(e, BOUNDING, [out, a, b])
    return {"result": list(struct.unpack(">4i", e.read(out, 16))), "this": list(struct.unpack(">4i", e.read(a, 16))),
            "other": list(struct.unpack(">4i", e.read(b, 16)))}


def host_bounding(lib, case):
    a, b, out = (C.c_int32 * 4)(*case[0]), (C.c_int32 * 4)(*case[1]), (C.c_int32 * 4)(*([-0x5A5A5A5B] * 4))
    lib.tpw_ll_bounding(C.addressof(a), C.addressof(b), C.addressof(out))
    return {"result": list(out), "this": list(a), "other": list(b)}


class BullfrogTest(FnTest):
    """A test of a function of the bullfrog library's program: the runner is switched to its emulator for the case and back."""

    def run_case(self, runner, case):
        main = runner.program
        runner.use(BULLFROG)
        try:
            return FnTest.run_case(self, runner, case)
        finally:
            runner.use(main)


# ------------------------------------------------------------------ the end of the interface


def progress(e):
    """The delete[] blocks (not NULL) the original has freed so far in the case."""
    return sum(1 for kind, address, _ in e.alloc_log if kind == "delete[]" and address != 0)


def deinit_ints(e):
    """What the fakes record besides their arguments: the delete[] count, the cursor table's count, whether the font has a bitmap."""
    return [progress(e), struct.unpack(">H", e.read(CURSOR_TABLE + 4, 2))[0], int(e.u32(FONT + 8) != 0)]


def window_vtable(e):
    """A vtable (in persistent memory) whose third slot (+8) is the fake deleting destructor."""
    if hasattr(e, "ll_window_vtable"):
        return e.ll_window_vtable

    def destructor(emu):
        L, this, flag = emu.cl, emu.r(3), T.s16(emu.r(4))
        L.rec("Window.dtor", [L.lab(this)] + deinit_ints(emu))
        if flag > 0:
            L.rec("delete", (L.lab(this),))
            L.forget(this)

    vtable = T.persist_alloc(e, 0x20)
    e.write(vtable + 8, struct.pack(">I", T.fake_function(e, destructor)))
    e.ll_window_vtable = vtable
    return vtable


def deinit_case(r, windows, cursor, font, heap, member):
    return {"windows": windows, "cursor": cursor, "font": font, "heap": heap, "member": member}


FONT_WORDS = [(0, 2, 2, 0, 0), (0x5A5A, 7, 9, -3, 17), (255, 1, 0, 0x7FFF, -1)]
CURSORS = [(0, 0), (0, 1), (3, 1), (22, 1)]
HEAPS = [0, 7, -1]       # NULL, a labelled block, a pointer nobody knows


def deinit_edges():
    r = random.Random(900)
    for member in (0, 1):
        for mask in range(8):
            windows = [bool(mask & 1), bool(mask & 2), bool(mask & 4)]
            for cursor in CURSORS:
                for bitmap in (0, 40):
                    for heap in HEAPS:
                        words = FONT_WORDS[r.randint(0, 2)]
                        yield deinit_case(r, windows, cursor, (bitmap,) + words, heap, member)


def random_deinit(member):
    def gen(r):
        windows = [r.random() < 0.5 for _ in range(3)]
        cursor = r.choice(CURSORS)
        font = (r.choice([0, 1, 40, 1000]),) + r.choice(FONT_WORDS)
        return deinit_case(r, windows, cursor, font, r.choice(HEAPS), member)
    return gen


def deinit_edge_cases(member):
    return lambda: [c for c in deinit_edges() if c["member"] == member]


def ppc_deinit(e, case):
    with Ppc(e) as run:
        run.g.keep(CURSOR_TABLE, 8)
        run.g.keep(UI_HEAP, 4)
        run.g.keep(FONT, 0x10)
        for address in WINDOW_GLOBALS:
            run.g.keep(address, 4)
        vtable = window_vtable(e)
        fixed = {}
        for role, (address, present) in enumerate(zip(WINDOW_GLOBALS, case["windows"])):
            window = 0
            if present:
                window = e.alloc(0x40, b"\xa5" * 0x40)
                e.write(window + 0x38, struct.pack(">I", vtable))
                fixed[window] = role + 1
            e.write(address, struct.pack(">I", window))
        count, entries = case["cursor"]
        e.write(CURSOR_TABLE, struct.pack(">IH", e.alloc(4 * max(count, 1)) if entries else 0, count))
        heap = {0: 0, 7: e.alloc(16), -1: 0x1234}[case["heap"]]
        if case["heap"] == 7:
            fixed[heap] = 7
        e.write(UI_HEAP, struct.pack(">I", heap))
        bitmap, f00, f02, f04, height, ident = case["font"]
        e.write(FONT, struct.pack(">HHHhII", f00 & 0xFFFF, f02, f04, height, e.alloc(max(bitmap, 4)) if bitmap else 0, ident & MASK))
        e.cl = CL.PpcLog(fixed)
        e.alloc_log = []
        run.call(DEINIT_UI if case["member"] == 0 else DEINIT_GENERAL, [0x10396440])
        return {"log": e.cl.log, "windows": [int(e.u32(a) != 0) for a in WINDOW_GLOBALS],
                "cursor": [int(e.u32(CURSOR_TABLE) != 0), struct.unpack(">H", e.read(CURSOR_TABLE + 4, 2))[0]],
                "heap": int(e.u32(UI_HEAP) == heap), "font": font_words(e), "freed": progress(e)}


def font_words(e):
    f00, f02, f04, height, bitmap, ident = struct.unpack(">HHHhII", e.read(FONT, 16))
    return [f00, f02, f04, height, int(bitmap != 0), ident]


def host_deinit(lib, case):
    lib.tpw_ll_reset()
    fixed = {}
    indices = []
    for role, present in enumerate(case["windows"]):
        indices.append(role if present else -1)
    lib.tpw_ll_set_windows(*indices)
    out = (C.c_uint64 * 3)()
    lib.tpw_ll_get_windows(C.addressof(out))
    for role, present in enumerate(case["windows"]):
        if present:
            fixed[out[role]] = role + 1
    count, entries = case["cursor"]
    lib.tpw_ll_set_cursors(count, entries)
    heap = {0: 0, 7: lib.tpw_ll_alloc(16), -1: 0x1234}[case["heap"]]
    if case["heap"] == 7:
        fixed[heap] = 7
    lib.tpw_ll_set_heap(C.c_void_p(heap))
    bitmap, f00, f02, f04, height, ident = case["font"]
    lib.tpw_ll_set_font(bitmap, f00, f02, f04, height, ident)
    CL.host_begin(lib, {label: address for address, label in fixed.items()})
    counters = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
    for i in range(4):
        counters[i] = 0
    lib.tpw_ll_deinit(case["member"])
    log = CL.host_end(lib)
    freed = counters[3]
    lib.tpw_ll_get_windows(C.addressof(out))
    cursors = (C.c_int64 * 2)()
    lib.tpw_ll_get_cursors(C.addressof(cursors))
    words = (C.c_int32 * 6)()
    lib.tpw_ll_get_font(C.addressof(words))
    return {"log": log, "windows": [int(out[i] != 0) for i in range(3)], "cursor": list(cursors),
            "heap": int(lib.tpw_ll_get_heap() == heap),
            "font": [words[0] & 0xFFFF, words[1] & 0xFFFF, words[2] & 0xFFFF, words[3], words[4], words[5] & MASK], "freed": freed}


# ------------------------------------------------------------------ the tests


TESTS = [
    FnTest("RecalculateInvalidNodeFaceNormals", run_normals_ppc, run_normals_host, edge=normal_cases("node"),
           rnd=lambda r: random_normals(r, "node")),
    FnTest("RecalculateInvalidFaceNormals", run_normals_ppc, run_normals_host, edge=normal_cases("header"),
           rnd=lambda r: random_normals(r, "header")),
    FnTest("XRotateLMatrixNoTrans", ppc_rotate, host_rotate, edge=rotate_edges(0), rnd=random_rotation(0)),
    FnTest("YRotateLMatrixNoTrans", ppc_rotate, host_rotate, edge=rotate_edges(1), rnd=random_rotation(1)),
    FnTest("ZRotateLMatrixNoTrans", ppc_rotate, host_rotate, edge=rotate_edges(2), rnd=random_rotation(2)),
    FnTest("RotateLMatrixXYZNoTrans", ppc_walk, host_walk, edge=walk_edges(0), rnd=random_walk(0)),
    FnTest("RotateLMatrixXYZNoTransHi", ppc_walk, host_walk, edge=walk_edges(1), rnd=random_walk(1)),
    BullfrogTest("TbRect::Bounding", ppc_bounding, host_bounding, edge=bounding_edges, rnd=random_rects),
    FnTest("DeInitGeneralUI", ppc_deinit, host_deinit, edge=deinit_edge_cases(0), rnd=random_deinit(0)),
    FnTest("UIInterface::DeInitGeneral", ppc_deinit, host_deinit, edge=deinit_edge_cases(1), rnd=random_deinit(1)),
]
