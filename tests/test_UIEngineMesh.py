"""Differential tests for the engine mesh draw objects' instancing and changes, against libtpw_unit_UIEngineMesh.dylib
(tests/unit_UIEngineMesh.cpp):

  src/ui_DrawObjectEngineMesh.cpp  InterfaceDrawEngineMesh: InstanceMe, CalculateFrame, Change
                                   InterfaceDrawEngineVGradient::Change, InterfaceDrawEngineHGradient::Change and ::CalcVerts
                                   UpdateMeshLocalMatrix
  src/ui_matrix.cpp                GetUIMatrix, CombineMatricesNoWDest1st

Real on both sides: those, SetMatrixIdentity and VectorThroughLMatrixNoW, __ptr_glue (the vertical gradient's virtual calls) and the rectangles.
Fakes that record the same event on both sides: InstanceMesh (which answers one of the world's meshes or leaves its output alone), the engine mesh's
SetLocalMatrix (with all sixteen words), the engine mesh draw object's constructor (a new label, the mesh; it writes recognisable words in the frame
fields), the vertical gradient's CalcVerts and CalcUVs. Operator new is the harness's.

The two functions of numbers (GetUIMatrix, CombineMatricesNoWDest1st) are tested on their own: random shorts, floats (including NaNs, infinities,
denormals and zeros of both signs), a sine table of the case's choosing and, for the combiner, the aliased call. The rest are tested as scripts
on a world: 5 meshes (a tag_sMesh, a header and a chain of 0 to 5 nodes with flags, parents, matrices and boxes), 4 draw objects, 3 gradients (each
horizontal or vertical), the UI's heap and the system's flags. After EACH operation the answer, the heap traffic, the length of the call log and
the whole state are compared (every node's flags, matrix and box; every object's and gradient's words, matrix, vertices and texture coordinates;
the instances InstanceMe made; gei_System's flags), and the whole call log at the end. Floats compare by their bits, NaNs as one value.

The original runs with two corrections that are the emulator's, not the code's (HardwareRounding below, as in test_SkyRender.py): Unicorn's fused
multiply-adds round twice, and its fctiwz gives 0x80000000 above INT32_MAX where the processor saturates to 0x7fffffff (the "huge" sine table).

Quirks the tests pin: InstanceMe answers the object itself unless it is in the library AND has windows using it (mResizable) AND has a mesh; the
new object gets only the frame base, frame and frame count; CalculateFrame without flag 1 or 0x80000 changes nothing and answers the current
node; a frame sum below zero (as a short) walks the chain to its end and gives NULL; Change does nothing before a node is chosen, and keeps the
translation z (the depth) through a placement; the localised scale uses the absolute value of the box depth; the gradients store the depth
back whatever the flags are, redo the quad only when the fraction really changed, and tell the engine the view changed once.
"""
import ctypes as C
import math
import struct

from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as P

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIEngineMesh"

MASK = 0xFFFFFFFF
NAN_BITS = 0x7FC00000

# ---- the unit's functions (run as the original on the PowerPC side)
FN = dict(InstanceMe=0x10188580, CalculateFrame=0x1018806C, Change=0x10188168, GetUIMatrix=0x10187CE4, Combine1st=0x100C2694,
          VChange=0x101895E0, HChange=0x10189EA0, HCalcVerts=0x10189CD4, UpdateMesh=0x100652F4)
# what they call that is real too
SET_MATRIX_IDENTITY, VECTOR_THROUGH, PTR_GLUE = 0x100C0E28, 0x100C1F64, 0x1023BAA8
# the fakes (in-binary functions)
CTOR, INSTANCE_MESH, V_CALC_VERTS, V_CALC_UVS = 0x10187A28, 0x10061B1C, 0x10189414, 0x101894B0
SET_LOCAL_MATRIX = "SetLocalMatrix__5CMeshFiR7sMatrix"

# ---- the original's data
HEAP_GLOBAL = 0x1038CC60
TPGLOBALS = 0x103B74C8
TABLE_BYTES = 0xC + 4 * 0x1000
CORE_VTABLE = 0x102AF8FC
H_VTABLE, V_VTABLE = 0x102A7B18, 0x102A7B58

# ---- the world's size (the host's kMeshes, kNodes, ...)
MESHES, NODES, OBJECTS, GRADIENTS, INSTANCES = 5, 5, 4, 3, 8
OBJECT_WORDS, GRADIENT_WORDS, NODE_WORDS = 40, 86, 23

# ---- labels (the host's kLab...)
LAB_MESH, LAB_ENGINE, LAB_OBJECT, LAB_GRADIENT, LAB_HEAP, LAB_NODE = 10, 20, 30, 40, 50, 100
FIRST_NEW = 300

# the flags of Change
F_FRAME, F_DEPTH, F_ROT, F_VALUE, F_RECT, F_PIVOT, F_BASE = 0x1, 0x2, 0x4, 0x8, 0x10, 0x40, 0x80000


def s16(v):
    return T.s16(v & 0xFFFF)


def u32s16(v):
    """A short as the 32-bit word the dumps hold (sign extended)."""
    return s16(v) & MASK


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MASK))


def w16(e, address, value):
    e.write(address, struct.pack(">h", s16(value)))


def canon(w):
    """A NaN is one value."""
    return NAN_BITS if (w & 0x7F800000) == 0x7F800000 and (w & 0x7FFFFF) != 0 else w


def fbits(x):
    return T.f32_bits(x)


# ---- the sine table
def sine_words(size):
    return [fbits(T.f32(math.sin(2.0 * math.pi * i / size))) for i in range(size)]


def scrambled_words(size):
    return [fbits(T.f32(0.25 + 0.75 * ((i * 37 + 11) % size) / size)) for i in range(size)]


def scale_bits(size):
    return fbits(T.f32(size / 6.2831855))


SINES = {
    "sine": dict(size=0x1000, mask=0xFFF, scale=scale_bits(0x1000), table=sine_words(0x1000)),
    "sine16": dict(size=16, mask=15, scale=scale_bits(16), table=sine_words(16)),
    "scrambled": dict(size=0x1000, mask=0xFFF, scale=scale_bits(0x1000), table=scrambled_words(0x1000)),
    "mask0": dict(size=0x1000, mask=0, scale=scale_bits(0x1000), table=sine_words(0x1000)),
    "mask3": dict(size=4, mask=3, scale=scale_bits(4), table=sine_words(4)),
    "negative": dict(size=0x1000, mask=0xFFF, scale=fbits(-651.8986), table=sine_words(0x1000)),
    "huge": dict(size=0x1000, mask=0xFFF, scale=fbits(3.0e38), table=sine_words(0x1000)),
    "zero": dict(size=0x1000, mask=0xFFF, scale=0, table=sine_words(0x1000)),
}


# ======================================================================= the emulator's float corrections

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Makes the emulator's fused multiply-adds and fctiwz behave as the processor's do, for the functions that use them (as in
    test_SkyRender.py, with fmsubs and fnmsubs too). Unicorn (QEMU 5.0) computes fmadds, fmsubs, fnmadds and fnmsubs in double precision and
    rounds to single, which can differ by one unit in the last place from the processor's single rounding of the exact result; and its fctiwz
    gives 0x80000000 above INT32_MAX where the processor saturates to 0x7fffffff. Each such instruction gets a code hook of its own (the rest
    of the code runs at full speed): the hook computes the processor's result, writes it to the destination register and steps over the
    instruction. An instruction with an operand that is not finite is left to the emulator (an infinity or a NaN either way)."""

    ENTRIES = ("Change", "GetUIMatrix", "Combine1st", "VChange", "HChange")

    def __init__(self, e):
        self.e = e
        self.hooks = []
        if not hasattr(e, "uem_sites"):
            sizes = {f["entry"]: f["size"] for f in e.functions}
            ranges = [(FN[k], FN[k] + sizes[FN[k]]) for k in self.ENTRIES] + [(VECTOR_THROUGH, VECTOR_THROUGH + sizes[VECTOR_THROUGH])]
            sites = {}
            for begin, end in ranges:
                words = struct.unpack(">" + "I" * ((end - begin) // 4), e.read(begin, end - begin))
                for k, word in enumerate(words):
                    rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
                    if word >> 26 == 59 and (word >> 1) & 31 in (28, 29, 30, 31):      # fmsubs, fmadds, fnmsubs, fnmadds
                        sites[begin + 4 * k] = ("fused", (word >> 1) & 31, rd, ra, rb, rc)
                    elif word >> 26 == 63 and (word >> 1) & 0x3FF == 15:             # fctiwz frD, frB
                        sites[begin + 4 * k] = ("fctiwz", 0, rd, ra, rb, rc)
            e.uem_sites = sites
        self.sites = e.uem_sites

    def __enter__(self):
        for address in sorted(self.sites):
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=address, end=address))
        return self

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        kind, op, rd, ra, rb, rc = self.sites[address]
        e = self.e
        if kind == "fused":
            a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
            if all(math.isfinite(x) for x in (a, c, b)):
                for x in (a, c, b):
                    assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
                value = float(LIBM.fmaf(a, c, -b if op in (28, 30) else b))
                if op in (30, 31):
                    value = -value
                uc.reg_write(ppcemu.FPR[rd], struct.unpack(">Q", struct.pack(">d", value))[0])
                uc.reg_write(P.UC_PPC_REG_PC, address + 4)
        elif fpr_double(e, rb) >= 2147483648.0:
            uc.reg_write(ppcemu.FPR[rd], (fpr(e, rd) & 0xFFFFFFFF00000000) | 0x7FFFFFFF)
            uc.reg_write(P.UC_PPC_REG_PC, address + 4)


# ======================================================================= the case: a world and a script


def identity_bits():
    return [fbits(1.0 if r == c else 0.0) for r in range(4) for c in range(4)]


def matrix_bits(rows):
    return [fbits(T.f32(v)) for row in rows for v in row]


def translation_bits(tx, ty, tz):
    m = identity_bits()
    m[12], m[13], m[14] = fbits(T.f32(tx)), fbits(T.f32(ty)), fbits(T.f32(tz))
    return m


def scale_matrix_bits(sx, sy, sz):
    m = identity_bits()
    m[0], m[5], m[10] = fbits(T.f32(sx)), fbits(T.f32(sy)), fbits(T.f32(sz))
    return m


def rotation_bits(angle, tx=0.0, ty=0.0):
    c, s = math.cos(angle), math.sin(angle)
    return matrix_bits([[c, s, 0, 0], [-s, c, 0, 0], [0, 0, 1, 0], [tx, ty, 0, 1]])


def box_bits(lo, hi):
    return [fbits(T.f32(v)) for v in (*lo, *hi)]


def node(flags=0, parent=-1, matrix=None, box=None):
    return dict(flags=flags & MASK, parent=parent, matrix=matrix or identity_bits(), box=box or box_bits((0, 0, 0), (100, 100, 4)))


def base_words(shown=0, frame_base=0, frame=0, count=3, in_library=1, resizable=1, rect=(100, 200, 900, 700), rect2=(1, 2, 3, 4),
               f28=0, f2a=0, f2c=0, f2e=0, f30=0, f32=0, depth=0):
    return [shown, frame_base, frame, count, in_library, resizable, *rect, *rect2, f28, f2a, f2c, f2e, f30, f32, depth]


def obj(mesh=0, base=None, matrix=None, node_ref=-1, localised=0):
    return dict(mesh=mesh, base=base or base_words(), matrix=matrix or scale_matrix_bits(0.5, 0.25, 2.0), node=node_ref, localised=localised)


def grad(horizontal=True, built=1, base=None, matrix=None, box=None, fraction=50.0, mesh=0, vertices=None, uvs=None):
    return dict(horizontal=horizontal, built=built, base=base or base_words(), matrix=matrix or scale_matrix_bits(0.5, 0.25, 2.0),
                box=box or box_bits((0, 0, 0), (100, 100, 0)), fraction=fbits(T.f32(fraction)),
                mesh=mesh, vertices=vertices or [fbits(T.f32(0.5 * i)) for i in range(24)], uvs=uvs or [fbits(T.f32(0.25 * i)) for i in range(16)])


def default_meshes():
    """Five meshes: a chain of four nodes with a parent chain, a lone node, no nodes, two nodes, and a chain of five."""
    return [
        dict(nodes=[node(0x10, -1, translation_bits(10, 20, 0), box_bits((0, 0, 0), (100, 50, 4))),
                    node(0x8010, 0, rotation_bits(0.3, 5, 6), box_bits((-10, -5, 1), (90, 45, 3))),
                    node(0x200, 1, scale_matrix_bits(2, 2, 2), box_bits((1, 2, 3), (4, 5, 6))),
                    node(0xFFFFFFFF, -1, identity_bits(), box_bits((0, 0, 0), (1, 1, 1)))]),
        dict(nodes=[node(0, -1)]),
        dict(nodes=[]),
        dict(nodes=[node(1, -1, translation_bits(-5, 7, 0)), node(2, 0, scale_matrix_bits(1, -1, 1), box_bits((50, 50, 0), (0, 0, 0)))]),
        dict(nodes=[node(0x8010 * (i % 2), i - 1 if i else -1, rotation_bits(0.1 * i, i, -i)) for i in range(5)]),
    ]


class Case(dict):
    """A case; it prints as its script (the world is long)."""

    def __repr__(self):
        return "Case(sine=%r heap=%r ops=%s)" % (self.get("sine"), self.get("heap"), repr(self["ops"])[:700])


def default_case(ops, **kw):
    case = Case(sine="sine", heap=True, system=0x03000141, meshes=default_meshes(),
                objects=[obj(0, node_ref=0), obj(1), obj(2), obj(3, base_words(in_library=0), localised=40)],
                gradients=[grad(True, mesh=0), grad(False, mesh=1), grad(True, mesh=3)],
                q={"InstanceMesh": [1]}, ops=ops)
    case.update(kw)
    return case


def change(frame=0, depth=0, f08=0, base=0, f0c=0, f0e=0, rect=(100, 200, 900, 700), word=0):
    return dict(word=word & MASK, frame=frame, depth=depth, f08=f08, base=base, f0c=f0c, f0e=f0e, rect=rect)


def change_words(c):
    """The 13 words of the host's ChangeRecord; None is no record at all."""
    if c is None:
        return [0] * 13
    rect = c["rect"]
    return [c["word"] & MASK, u32s16(c["frame"]), u32s16(c["depth"]), u32s16(c["f08"]), u32s16(c["base"]), u32s16(c["f0c"]), u32s16(c["f0e"]),
            *[u32s16(v) for v in (rect or (0, 0, 0, 0))], 1 if rect else 0, 1]


def object_words(o):
    return [o["mesh"] & MASK, *[v & MASK for v in o["base"]], *o["matrix"], o["node"] & MASK, o["localised"] & MASK]


def gradient_words(g):
    return [g["built"], *[v & MASK for v in g["base"]], *g["matrix"], *g["box"], g["fraction"], g["mesh"] & MASK, *g["vertices"], *g["uvs"]]


def node_words(n):
    return [n["flags"], n["parent"] & MASK, *n["matrix"], *n["box"]]


# ======================================================================= the names of the state's words (for failure messages)


def state_names(case):
    names = []
    for m, mesh in enumerate(case["meshes"]):
        for i in range(len(mesh["nodes"])):
            names += [f"mesh{m}.node{i}.flags"] + [f"mesh{m}.node{i}.matrix{k}" for k in range(16)] + [f"mesh{m}.node{i}.box{k}" for k in range(6)]
    base = ["shown", "frameBase", "frame", "frameCount", "inLibrary", "resizable", "rect.l", "rect.t", "rect.r", "rect.b", "rect2.l", "rect2.t",
            "rect2.r", "rect2.b", "f28", "f2a", "f2c", "f2e", "f30", "f32", "depth"]
    for i in range(OBJECTS):
        names += [f"obj{i}.mesh"] + [f"obj{i}.{b}" for b in base] + [f"obj{i}.matrix{k}" for k in range(16)] + [f"obj{i}.node", f"obj{i}.localised"]
    for i, g in enumerate(case["gradients"]):
        names += [f"grad{i}.built"] + [f"grad{i}.{b}" for b in base] + [f"grad{i}.matrix{k}" for k in range(16)] + [f"grad{i}.box{k}" for k in range(6)]
        names += [f"grad{i}.fraction", f"grad{i}.mesh"] + [f"grad{i}.vertex{k}" for k in range(24)] + [f"grad{i}.uv{k}" for k in range(16)]
    names += [f"instance{{}}.{n}" for n in ["mesh"] + base + [f"matrix{k}" for k in range(16)] + ["node", "localised"]]
    names += ["system.flags"]
    return names


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the world in the emulator's memory, the fakes, and the operations."""

    def __init__(self, e, case):
        self.e = e
        self.case = case
        e.heap_in_scratch = True
        self.system = e.data_import("gei_System")
        self.saved = {a: bytes(e.read(a, n)) for a, n in ((TPGLOBALS, TABLE_BYTES), (HEAP_GLOBAL, 4), (self.system, 4))}
        self.mesh_addr, self.engine_addr, self.node_addr = [], [], []
        self.objects, self.gradients, self.instances = [], [], []
        fixed = {}
        self.heap = e.alloc(16, bytes(16))
        fixed[self.heap] = LAB_HEAP
        for m, mesh in enumerate(case["meshes"]):
            engine = e.alloc(16, bytes(16))
            mesh_a = e.alloc(0x40, b"\xa5" * 0x40)
            header = e.alloc(0xB8, b"\xa5" * 0xB8)
            nodes = [e.alloc(0xA0, b"\xa5" * 0xA0) for _ in mesh["nodes"]]
            for i, (a, n) in enumerate(zip(nodes, mesh["nodes"])):
                w32(e, a, n["flags"])
                w32(e, a + 4, nodes[n["parent"]] if n["parent"] >= 0 else 0)
                w32(e, a + 0xC, nodes[i + 1] if i + 1 < len(nodes) else 0)
                e.write(a + 0x10, struct.pack(">16I", *n["matrix"]))
                e.write(a + 0x78, struct.pack(">6I", *n["box"]))
                fixed[a] = LAB_NODE + m * 8 + i
            w32(e, mesh_a + 4, header)
            w32(e, mesh_a + 0x1C, engine)
            w32(e, header + 0x78, nodes[0] if nodes else 0)
            fixed[mesh_a], fixed[engine] = LAB_MESH + m, LAB_ENGINE + m
            self.mesh_addr.append(mesh_a)
            self.engine_addr.append(engine)
            self.node_addr.append(nodes)
        for slot, o in enumerate(case["objects"]):
            a = e.alloc(0x84, bytes(0x84))
            w32(e, a, self.mesh_addr[o["mesh"]] if o["mesh"] >= 0 else 0)
            self.write_base(a, o["base"])
            e.write(a + 0x3C, struct.pack(">16I", *o["matrix"]))
            w32(e, a + 0x7C, self.node_address(o["node"]))
            w32(e, a + 0x80, o["localised"])
            fixed[a] = LAB_OBJECT + slot
            self.objects.append(a)
        self.vertex_addr, self.uv_addr = [], []
        for slot, g in enumerate(case["gradients"]):
            a = e.alloc(0x124, bytes(0x124))
            vertices = e.alloc(24 * 4, struct.pack(">24I", *g["vertices"]))
            uvs = e.alloc(16 * 4, struct.pack(">16I", *g["uvs"]))
            w32(e, a, g["built"])
            self.write_base(a, g["base"])
            w32(e, a + 0x38, H_VTABLE if g["horizontal"] else V_VTABLE)
            e.write(a + 0x4C, struct.pack(">6I", *g["box"]))
            w32(e, a + 0x7C, vertices)
            w32(e, a + 0x84, uvs)
            w32(e, a + 0x90, self.mesh_addr[g["mesh"]] if g["mesh"] >= 0 else 0)
            e.write(a + 0xDC, struct.pack(">16I", *g["matrix"]))
            w32(e, a + 0x120, g["fraction"])
            fixed[a] = LAB_GRADIENT + slot
            self.gradients.append(a)
            self.vertex_addr.append(vertices)
            self.uv_addr.append(uvs)
        queues = {k: v[:64] for k, v in case["q"].items()}
        self.L = e.cl = CL.PpcLog(fixed, queues, first_new=FIRST_NEW)
        # the globals
        sine = SINES[case["sine"]]
        e.write(TPGLOBALS, struct.pack(">3I", sine["size"], sine["mask"], sine["scale"]) + struct.pack(f">{len(sine['table'])}I", *sine["table"]))
        w32(e, HEAP_GLOBAL, self.heap if case["heap"] else 0)
        w32(e, self.system, case["system"])
        self.stubs = self.make_stubs()
        self.old = self.stubs.activate(e)

    def write_base(self, a, w):
        e = self.e
        for offset, value in zip((4, 6, 8, 0xA, 0xC, 0xE), w[0:6]):
            w16(e, a + offset, value)
        w32(e, a + 0x10, CORE_VTABLE)
        for offset, value in zip((0x14, 0x16, 0x18, 0x1A), w[6:10]):
            w16(e, a + offset, value)
        w32(e, a + 0x1C, CORE_VTABLE)
        for offset, value in zip((0x20, 0x22, 0x24, 0x26), w[10:14]):
            w16(e, a + offset, value)
        for offset, value in zip((0x28, 0x2A, 0x2C, 0x2E, 0x30, 0x32, 0x34), w[14:21]):
            w16(e, a + offset, value)

    def node_address(self, ref):
        return 0 if ref < 0 else self.node_addr[ref // 8][ref % 8]

    def finish(self):
        CL.Stubs.deactivate(self.e, self.old)
        for a, data in self.saved.items():
            self.e.write(a, data)

    # ---- the fakes
    def make_stubs(self):
        e = self.e
        st = CL.Stubs()
        st.real = {FN[k] for k in ("InstanceMe", "CalculateFrame", "Change", "GetUIMatrix", "Combine1st", "VChange", "HChange", "HCalcVerts",
                                   "UpdateMesh")} | {SET_MATRIX_IDENTITY, VECTOR_THROUGH, PTR_GLUE}

        def ctor(e):
            L = e.cl
            this, mesh = e.r(3), e.r(4)
            L.rec("ctor", (L.newlab(this), L.lab(mesh)))
            w32(e, this, mesh)
            for offset, value in ((4, 0), (6, 0x1111), (8, 0x2222), (0xA, 0x3333), (0xC, 0), (0xE, 0), (0x14, 0), (0x16, 0), (0x18, 0), (0x1A, 0),
                                  (0x20, 0), (0x22, 0), (0x24, 0), (0x26, 0), (0x28, 0), (0x2A, 0), (0x2C, 0), (0x2E, 0), (0x30, 0), (0x32, 0),
                                  (0x34, 0)):
                w16(e, this + offset, value)
            e.write(this + 0x3C, bytes(0x48))
            e.set_r(3, this)

        def instance_mesh(e):
            L = e.cl
            mesh, heap, flags, output, mask = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
            L.rec("InstanceMesh", (L.lab(mesh), L.lab(heap), flags, mask, 1 if output else 0, L.lab(e.u32(output)) if output else 0))
            which = L.ret("InstanceMesh")
            if output and 0 < which <= len(self.mesh_addr):
                w32(e, output, self.mesh_addr[which - 1])
            e.set_r(3, 1)

        def set_local_matrix(e):
            L = e.cl
            words = struct.unpack(">16I", e.read(e.r(5), 64))
            L.rec("SetLocalMatrix", (L.lab(e.r(3)), T.s32(e.r(4))))
            L.rec("Matrix", [canon(w) for w in words[:8]])
            L.rec("Matrix", [canon(w) for w in words[8:]])

        st.internal(CTOR, ctor)
        st.internal(INSTANCE_MESH, instance_mesh)
        st.internal(V_CALC_VERTS, lambda e: e.cl.rec("VCalcVerts", (e.cl.lab(e.r(3)),)))
        st.internal(V_CALC_UVS, lambda e: e.cl.rec("VCalcUVs", (e.cl.lab(e.r(3)),)))
        st.imp(SET_LOCAL_MATRIX, set_local_matrix)
        return st

    # ---- the dumps (the order is tpw_uem_dump in the library)
    def node_ref(self, address):
        if address == 0:
            return MASK
        for m, nodes in enumerate(self.node_addr):
            if address in nodes:
                return (m * 8 + nodes.index(address)) & MASK
        return (-2) & MASK

    def mesh_ref(self, address):
        if address == 0:
            return MASK
        return self.mesh_addr.index(address) if address in self.mesh_addr else (-2) & MASK

    def base_dump(self, d):
        out = [v & MASK for v in struct.unpack_from(">6h", d, 4)]
        out += [v & MASK for v in struct.unpack_from(">4h", d, 0x14)]
        out += [v & MASK for v in struct.unpack_from(">4h", d, 0x20)]
        out += [v & MASK for v in struct.unpack_from(">7h", d, 0x28)]
        return out

    def words(self, d, offset, n):
        return list(struct.unpack_from(f">{n}I", d, offset))

    def dump_object(self, a):
        d = bytes(self.e.read(a, 0x84))
        mesh = struct.unpack_from(">I", d, 0)[0]
        out = [self.mesh_ref(mesh)] + self.base_dump(d) + [canon(w) for w in self.words(d, 0x3C, 16)]
        out += [self.node_ref(struct.unpack_from(">I", d, 0x7C)[0]), struct.unpack_from(">I", d, 0x80)[0]]
        assert len(out) == OBJECT_WORDS
        return out

    def dump_gradient(self, slot):
        a = self.gradients[slot]
        d = bytes(self.e.read(a, 0x124))
        out = [1 if struct.unpack_from(">I", d, 0)[0] != 0 else 0] + self.base_dump(d) + [canon(w) for w in self.words(d, 0xDC, 16)]
        out += self.words(d, 0x4C, 6) + [canon(struct.unpack_from(">I", d, 0x120)[0]), self.mesh_ref(struct.unpack_from(">I", d, 0x90)[0])]
        out += [canon(w) for w in self.words(bytes(self.e.read(self.vertex_addr[slot], 96)), 0, 24)]
        out += [canon(w) for w in self.words(bytes(self.e.read(self.uv_addr[slot], 64)), 0, 16)]
        assert len(out) == GRADIENT_WORDS
        return out

    def dump(self):
        e = self.e
        out = []
        for nodes in self.node_addr:
            for a in nodes:
                d = bytes(e.read(a, 0xA0))
                out += self.words(d, 0, 1) + self.words(d, 0x10, 16) + self.words(d, 0x78, 6)
        for a in self.objects:
            out += self.dump_object(a)
        for slot in range(len(self.gradients)):
            out += self.dump_gradient(slot)
        for a in self.instances:
            out += self.dump_object(a)
        out.append(struct.unpack(">I", e.read(self.system, 4))[0])
        return struct.pack(f"<{len(out)}I", *out)

    def globals(self):
        e = self.e
        words = struct.unpack(">3I", e.read(TPGLOBALS, 12))
        table = struct.unpack(">4096I", e.read(TPGLOBALS + 12, 0x4000))
        return [struct.unpack(">I", e.read(self.system, 4))[0], words[0], words[1], words[2], sum(table) & MASK]

    # ---- the operations
    def object_address(self, slot):
        if slot < OBJECTS:
            return self.objects[slot]
        return self.instances[slot - OBJECTS] if slot - OBJECTS < len(self.instances) else None

    def change_record(self, c):
        e = self.e
        if c is None:
            return 0
        record = e.alloc(0x28, bytes(0x28))
        rect = e.alloc(16, struct.pack(">I4h", CORE_VTABLE, *[s16(v) for v in (c["rect"] or (0, 0, 0, 0))]))
        w32(e, record, c["word"])
        for offset, key in ((4, "frame"), (6, "depth"), (8, "f08"), (0xA, "base"), (0xC, "f0c"), (0xE, "f0e")):
            w16(e, record + offset, c[key])
        w32(e, record + 0x14, rect if c["rect"] else 0)
        return record

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MASK for a in args])[0]

    def step(self, op):
        e = self.e
        kind = op[0]
        n0 = len(e.alloc_log)
        answer = 0
        if kind == "instance":
            a = self.object_address(op[1])
            if a is None:
                answer = -1
            else:
                r = self.call(FN["InstanceMe"], a)
                if r == 0:
                    answer = 0
                elif r == a:
                    answer = 1
                else:
                    answer = len(self.instances) + 2
                    if len(self.instances) < INSTANCES:
                        self.instances.append(r)
        elif kind in ("calc", "change"):
            a = self.object_address(op[1])
            if a is None or (kind == "calc" and op[2] & 0x80001 and (self.e.u32(a) == 0 or op[3] is None)):
                answer = -1          # (no such object; or a frame flag on an object without a mesh or without a record: read through NULL)
            else:
                r = self.call(FN["CalculateFrame" if kind == "calc" else "Change"], a, op[2], self.change_record(op[3]))
                if kind == "calc":
                    answer = (T.s32(self.node_ref(r)) if r else -1) + 1
        elif kind in ("hchange", "vchange"):
            a = self.gradients[op[1]]
            self.call(FN["HChange" if kind == "hchange" else "VChange"], a, op[2], self.change_record(op[3]))
        elif kind == "hverts":
            self.call(FN["HCalcVerts"], self.gradients[op[1]])
        elif kind == "update":
            matrix = e.alloc(64, struct.pack(">16I", *op[2]))
            self.call(FN["UpdateMesh"], self.mesh_addr[op[1]], matrix)
        log = e.alloc_log[n0:]
        news = [x for x in log if x[0] == "new"]
        heap = [len(news), len([x for x in log if x[0] == "new[]"]), len([x for x in log if x[0] == "delete"]),
                len([x for x in log if x[0] == "delete[]"])]
        return dict(answer=answer, heap=heap, sizes=[x[2] for x in news], state=self.dump(), log=len(self.L.log))


def run_ppc(e, case):
    ppc = Ppc(e, case)
    try:
        with HardwareRounding(e):
            steps = [ppc.step(op) for op in case["ops"]]
        return dict(steps=steps, log=list(ppc.L.log), globals=ppc.globals())
    finally:
        ppc.finish()


# ======================================================================= the host side

_prototyped = set()


def prototypes(lib):
    import ctypes as C
    if id(lib) in _prototyped:
        return
    _prototyped.add(id(lib))
    u32p = C.POINTER(C.c_uint32)
    lib.tpw_uem_reset.argtypes = []
    lib.tpw_uem_globals.argtypes = [C.c_uint32, C.c_int32, C.c_uint32, C.c_uint32, C.c_uint32, u32p, C.c_uint32]
    lib.tpw_uem_mesh.argtypes = [C.c_int32, C.c_int32, u32p]
    lib.tpw_uem_object.argtypes = [C.c_int32, u32p]
    lib.tpw_uem_gradient.argtypes = [C.c_int32, C.c_int32, u32p]
    lib.tpw_uem_op.argtypes = [C.c_int32, C.c_int32, C.c_uint32, u32p]
    lib.tpw_uem_op.restype = C.c_int64
    lib.tpw_uem_heap.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uem_dump.argtypes = [u32p]
    lib.tpw_uem_dump.restype = C.c_int32
    lib.tpw_uem_get_globals.argtypes = [u32p]
    lib.tpw_uem_get_matrix.argtypes = [u32p] + [C.c_int32] * 4 + [C.c_uint32] * 4 + [C.c_int32] * 3
    lib.tpw_uem_combine.argtypes = [u32p, u32p, C.c_int32]


def u32array(words):
    import ctypes as C
    return (C.c_uint32 * max(len(words), 1))(*[w & MASK for w in words])


def run_host(lib, case):
    import ctypes as C
    prototypes(lib)
    lib.tpw_uem_reset()
    CL.host_begin(lib, {}, {k: v[:64] for k, v in case["q"].items()}, first_new=FIRST_NEW)
    sine = SINES[case["sine"]]
    lib.tpw_uem_globals(case["system"], 1 if case["heap"] else 0, sine["size"], sine["mask"], sine["scale"], u32array(sine["table"]),
                        len(sine["table"]))
    for m, mesh in enumerate(case["meshes"]):
        words = [w for n in mesh["nodes"] for w in node_words(n)]
        lib.tpw_uem_mesh(m, len(mesh["nodes"]), u32array(words))
    for slot, o in enumerate(case["objects"]):
        lib.tpw_uem_object(slot, u32array(object_words(o)))
    for slot, g in enumerate(case["gradients"]):
        lib.tpw_uem_gradient(slot, 1 if g["horizontal"] else 0, u32array(gradient_words(g)))
    kinds = dict(instance=0, calc=1, change=2, hchange=3, vchange=4, hverts=5, update=6)
    steps = []
    heap = (C.c_int64 * 4)()
    dump = (C.c_uint32 * 4096)()
    for op in case["ops"]:
        kind = op[0]
        record = u32array(op[2] if kind == "update" else change_words(op[3]) if kind in ("calc", "change", "hchange", "vchange") else [0] * 13)
        flags = op[2] if kind in ("calc", "change", "hchange", "vchange") else 0
        answer = lib.tpw_uem_op(kinds[kind], op[1], flags & MASK, record)
        lib.tpw_uem_heap(heap)
        n = lib.tpw_uem_dump(dump)
        steps.append(dict(answer=answer, heap=list(heap), sizes=[0x84] * heap[0], state=bytes(dump)[:4 * n],
                          log=C.c_int32.in_dll(lib, "tpw_cl_n").value))
    out = (C.c_uint32 * 5)()
    lib.tpw_uem_get_globals(out)
    return dict(steps=steps, log=CL.host_end(lib), globals=list(out))


class ScriptTest(FnTest):
    """A scripted test whose failure messages name the state's words."""

    def run_case(self, runner, case):
        messages = FnTest.run_case(self, runner, case)
        names = None
        out = []
        for message in messages:
            if "state: bytes differ at " in message:
                if names is None:
                    names = state_names(case)
                path, rest = message.split(": bytes differ at ", 1)
                index = int(rest.split(" ")[0]) // 4
                step = case["ops"][int(path.split(".")[1])] if path.startswith("steps.") else None
                label = names[index] if index < len(names) else f"word {index}"
                if label.startswith("instance{}"):
                    label = label.replace("{}", "(n)")
                message = f"{message}  [{label}; op {step[:3] if step else None}]"
            out.append(message)
        return out


def setup(runner):
    T.install_std_stubs(runner.emu)


# ======================================================================= random worlds and scripts

SPECIAL_FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 2.0, 100.0, 1024.0, 1536.0, 2048.0, 40.0, 1e-30, 1e30, 3.4e38, -3.4e38, math.inf, -math.inf,
                  1.4e-45, 1.17549435e-38]


def rf(r, wild=0.04, lo=-1500.0, hi=1500.0):
    """A random float's bits: mostly moderate, now and then special or anything (NaNs included)."""
    k = r.random()
    if k < wild:
        return fbits(T.rfloat(r, nan=True))
    if k < wild + 0.06:
        return fbits(r.choice(SPECIAL_FLOATS))
    return fbits(T.f32(r.uniform(lo, hi)))


def rmatrix(r, wild=0.04):
    k = r.random()
    if k < 0.2:
        return identity_bits()
    if k < 0.4:
        return translation_bits(r.uniform(-300, 300), r.uniform(-300, 300), r.uniform(-5, 5))
    if k < 0.55:
        return scale_matrix_bits(r.uniform(-3, 3), r.uniform(-3, 3), r.uniform(-3, 3))
    if k < 0.7:
        return rotation_bits(r.uniform(-4, 4), r.uniform(-100, 100), r.uniform(-100, 100))
    return [rf(r, wild, -50.0, 50.0) for _ in range(16)]


def rbox(r, wild=0.03):
    lo = [r.uniform(-200, 200), r.uniform(-200, 200), r.uniform(-5, 5)]
    hi = [lo[0] + r.uniform(-20, 300), lo[1] + r.uniform(-20, 300), lo[2] + r.uniform(-3, 8)]
    if r.random() < wild * 3:
        return [rf(r, 0.3, -300.0, 300.0) for _ in range(6)]
    if r.random() < 0.1:
        hi = list(lo)
    return box_bits(lo, hi)


def rshort(r, wild=0.1):
    k = r.random()
    if k < wild:
        return r.choice([0, 1, -1, 0x7FFF, -0x8000, 0x4000, 0xFFFF, 0x8000])
    return r.randint(-40, 40) if k < 0.4 else r.randint(-300, 2300)


def rrect(r):
    k = r.random()
    if k < 0.1:
        return (0, 0, 0, 0)
    if k < 0.2:
        x, y = r.randint(0, 2048), r.randint(0, 1536)
        return (x, y, x, y)
    if k < 0.3:
        return (r.randint(0, 2048), r.randint(0, 1536), r.randint(-100, 3000), r.randint(-100, 2000))
    if k < 0.35:
        return tuple(r.choice([0, 1, -1, 0x7FFF, -0x8000]) for _ in range(4))
    left, top = r.randint(-50, 1800), r.randint(-50, 1300)
    return (left, top, left + r.randint(1, 1500), top + r.randint(1, 1000))


def rnode(r, index, chain):
    flags = r.choice([0, 0x10, 0x8010, 0x200, 0xFFFFFFFF, r.getrandbits(32)])
    parent = r.randrange(index) if index and r.random() < 0.5 else -1
    return node(flags, parent, rmatrix(r), rbox(r))


def rmesh(r, good=0.0):
    count = r.choice([1, 2, 3, 3, 4, 5, 5]) if r.random() < good else r.choice([0, 1, 1, 2, 3, 3, 4, 5, 5])
    return dict(nodes=[rnode(r, i, count) for i in range(count)])


def rbase(r, in_library=None):
    return base_words(shown=r.choice([0, 1, 1, -1, 5]), frame_base=r.choice([0, 0, 0, 1, 2]) if r.random() < 0.7 else rshort(r, 0.3),
                      frame=r.randint(0, 3) if r.random() < 0.8 else rshort(r), count=rshort(r, 0.2),
                      in_library=r.choice([0, 1, 1, 1, -1, 2]) if in_library is None else in_library,
                      resizable=r.choice([0, 1, 1, 1, -1, 3]), rect=rrect(r), rect2=rrect(r), f28=rshort(r), f2a=rshort(r),
                      f2c=r.choice([0, 0, 0, rshort(r), r.randint(-4096, 4096)]), f2e=rshort(r),
                      f30=r.choice([0, 1024, rshort(r)]), f32=r.choice([0, 768, rshort(r)]), depth=rshort(r))


def robject(r, meshes, good=0.0):
    """A random draw object. `good` is the chance that it has a mesh with nodes and a node chosen (the productive case)."""
    if r.random() < good:
        usable = [m for m, mesh in enumerate(meshes) if mesh["nodes"]]
        if usable:
            m = r.choice(usable)
            node_ref = m * 8 + r.randrange(len(meshes[m]["nodes"])) if r.random() < 0.8 else -1
            return obj(m, rbase(r), rmatrix(r), node_ref, r.choice([0, 0, 0, 1, 40, -3, 1000, 0x7FFFFFFF, -0x80000000]))
    mesh = r.choice([-1] + [m for m in range(len(meshes))] * 3) if r.random() < 0.85 else r.randrange(len(meshes))
    node_ref = -1
    if r.random() < 0.7:
        m = r.randrange(len(meshes))
        if meshes[m]["nodes"]:
            node_ref = m * 8 + r.randrange(len(meshes[m]["nodes"]))
    return obj(mesh, rbase(r), rmatrix(r), node_ref, r.choice([0, 0, 0, 1, 40, -3, 1000, 0x7FFFFFFF, -0x80000000]))


def rgradient(r, meshes, horizontal):
    return grad(horizontal, built=1 if r.random() < 0.9 else 0, base=rbase(r), matrix=rmatrix(r),
                box=box_bits([r.choice([0, 0, -10, 5.5]), r.choice([0, 0, -10, 5.5]), 0], [r.choice([100, 100, 50, -20, 0]),
                                                                                    r.choice([100, 100, 50, -20, 0]), 0])
                if r.random() < 0.85 else rbox(r), fraction=r.choice([0.0, 50.0, 100.0, r.uniform(0, 100)]),
                mesh=r.randrange(len(meshes)), vertices=[rf(r, 0.02, -10, 110) for _ in range(24)], uvs=[rf(r, 0.02, -1, 2) for _ in range(16)])


FLAG_BITS = [F_FRAME, F_DEPTH, F_ROT, F_VALUE, F_RECT, F_PIVOT, F_BASE]


def rflags(r, allowed=None):
    flags = 0
    for bit in FLAG_BITS:
        if (allowed is None or bit in allowed) and r.random() < 0.35:
            flags |= bit
    if r.random() < 0.15:
        flags |= r.choice([0x20, 0x100, 0x400, 0x8000, 0x80000000, 0x10000, 0x2000])
    if r.random() < 0.04:
        flags = r.getrandbits(32) & ~(0 if allowed is None else 0)
    return flags & MASK


def rchange(r, flags, always=False):
    """A random change record, now and then none at all (Change does nothing then; CalculateFrame with a frame flag is skipped on both sides);
    one with flag 0x10 always has a rectangle (the original would read NULL)."""
    if r.random() < 0.05:
        return None
    word = r.choice([0, 1024, 512, 2048, -1, -2000, 0x7FFFFFFF, -0x80000000, r.randint(-3000, 3000), r.getrandbits(32)])
    return change(frame=r.randint(0, 3) if r.random() < 0.75 else r.randint(-2, 6) if r.random() < 0.7 else rshort(r), depth=rshort(r),
                  f08=r.choice([0, 0, 512, -512, 1024, 2048, 4095, -4096, rshort(r), r.randint(-4096, 4096)]),
                  base=r.choice([0, 0, 0, 1, -1]) if r.random() < 0.7 else rshort(r), f0c=r.choice([0, 1024, rshort(r)]), f0e=r.choice([0, 768, rshort(r)]),
                  rect=rrect(r) if (flags & F_RECT or r.random() < 0.5) else None, word=word)


def rcase(r, kinds, n_ops, good=0.6):
    meshes = [rmesh(r, good) for _ in range(MESHES)]
    objects = [robject(r, meshes, good) for _ in range(OBJECTS)]
    gradients = [rgradient(r, meshes, r.random() < 0.5) for _ in range(GRADIENTS)]
    q = {"InstanceMesh": [r.choice([0, 1, 2, 3, 4, 5, 5, 0]) for _ in range(12)]}
    ops = []
    instances = 0
    for _ in range(n_ops):
        kind = r.choice(kinds)
        if kind == "instance":
            if instances >= 7:
                kind = "change" if "change" in kinds else "calc"
            else:
                instances += 1
        slot = r.randrange(OBJECTS + (min(instances, 3) if r.random() < 0.3 else 0))
        if kind == "instance":
            ops.append(("instance", slot))
        elif kind in ("calc", "change"):
            flags = rflags(r)
            ops.append((kind, slot, flags, rchange(r, flags)))
        elif kind in ("hchange", "vchange"):
            want = kind == "hchange"
            slots = [i for i, g in enumerate(gradients) if g["horizontal"] == want]
            if not slots:
                continue
            flags = rflags(r)
            ops.append((kind, r.choice(slots), flags, rchange(r, flags, always=True)))
        elif kind == "hverts":
            slots = [i for i, g in enumerate(gradients) if g["horizontal"]]
            if slots:
                ops.append(("hverts", r.choice(slots)))
        elif kind == "update":
            ops.append(("update", r.randrange(MESHES), rmatrix(r)))
    return Case(sine=r.choice(["sine", "sine", "sine", "sine16", "scrambled", "mask0", "mask3", "negative", "huge", "zero"]),
                heap=r.random() < 0.85, system=r.choice([0x03000141, 0, MASK, 0x80000000, r.getrandbits(32)]), meshes=meshes, objects=objects,
                gradients=gradients, q=q, ops=ops)


# ======================================================================= edge scripts


def edge_instance():
    """InstanceMe: every combination of the three conditions, with and without a heap and a mesh from InstanceMesh."""
    for in_library in (0, 1, -1):
        for resizable in (0, 1, -1):
            for mesh in (-1, 0):
                for answer in (0, 1, 4):
                    base = base_words(frame_base=-7, frame=0x7FFF, count=-0x8000, in_library=in_library, resizable=resizable)
                    yield default_case([("instance", 0)], objects=[obj(mesh, base), obj(1), obj(2), obj(3)], q={"InstanceMesh": [answer]},
                                       heap=in_library != 0 or resizable != 0)
    yield default_case([("instance", 0), ("instance", 0), ("instance", 4), ("change", 4, 0x11, change()), ("calc", 4, 1, change(frame=1)),
                        ("instance", 1), ("change", 5, 0x40, change(f0c=3, f0e=4))], q={"InstanceMesh": [2, 3, 0, 4]})
    yield default_case([("instance", 9), ("instance", 4), ("instance", 3)], q={"InstanceMesh": [1]}, heap=False)


def edge_calc():
    """CalculateFrame: the flags, the frame sums (zero, inside the chain, the last, past the end, negative) and the old node."""
    for flags in (0, 1, 0x80000, 0x80001, 2, 4, 0x10, 0x40, 0x20, 0xFFFFFFFF, 0x7FFFE, 0x100000):
        for frame, base in ((0, 0), (1, 0), (0, 2), (2, 1), (3, 0), (4, 0), (2, 3), (-1, 0), (0, -1), (-1, 1), (-3, 2), (0x7FFF, 0x7FFF),
                            (-0x8000, 0), (-0x8000, -0x8000), (100, 0)):
            for mesh_index in (0, 2):
                for old in (-1, 1):
                    base_set = base_words(frame_base=3, frame=3, count=5)
                    yield default_case([("calc", 0, flags, change(frame=frame, base=base))],
                                       objects=[obj(mesh_index, base_set, node_ref=old if mesh_index == 0 else -1), obj(1), obj(2), obj(3)])
    yield default_case([("calc", 0, 1, change(frame=1)), ("calc", 0, 1, change(frame=2)), ("calc", 0, 0, None), ("calc", 0, 0x80000, change(base=-1)),
                        ("calc", 0, 0x10, change()), ("calc", 1, 0, change())])
    yield default_case([("calc", 0, 0, None)])
    yield default_case([("calc", 1, 1, change(frame=0)), ("calc", 1, 1, change(frame=5)), ("calc", 1, 0, change())])


def edge_change():
    """Change: every flag alone, the pairs, rectangles of every kind, the localised word, the pivot and the angle, deep parents."""
    rects = [(0, 0, 0, 0), (0, 0, 2048, 1536), (100, 200, 900, 700), (500, 500, 500, 500), (900, 700, 100, 200), (-100, -50, 100, 50),
             (0, 0, 1, 1), (2000, 1500, 2048, 1536), (0x7FFF, 0x7FFF, -0x8000, -0x8000), (10, 20, 10, 700), (10, 20, 900, 20)]
    for flags in (0, 1, 2, 4, 8, 0x10, 0x40, 0x80000, 0x3, 0x5, 0x12, 0x14, 0x15, 0x41, 0x50, 0x54, 0x55, 0x1F, 0x5F, 0x80055, 0x80000 | 2,
                  0xFFFFFFFF, 0x20, 0x100, 0xAA):
        for rect in rects[:3] + [(0, 0, 0, 0)]:
            yield default_case([("change", 0, flags, change(frame=2, depth=7, f08=1024, base=1, f0c=1000, f0e=700, rect=rect))])
    for rect in rects:
        for node_ref in (0, 1, 2, 3):
            yield default_case([("change", 0, 0x10 | 1, change(frame=node_ref, rect=rect))], objects=[obj(0, base_words(frame_base=0, frame=0)),
                                                                                                         obj(1), obj(2), obj(3)])
    for localised in (0, 1, -1, 40, 1000, -0x80000000, 0x7FFFFFFF):
        for frame in (0, 1, 2, 3):
            yield default_case([("change", 3, 0x11, change(frame=frame))],
                               objects=[obj(0), obj(1), obj(0, base_words(), localised=localised), obj(3, base_words(), localised=localised)])
            yield default_case([("change", 2, 0x11, change(frame=frame))],
                               objects=[obj(0), obj(1), obj(0, base_words(), localised=localised), obj(3, base_words(), localised=localised)])
    for angle in (0, 1, -1, 100, 512, 1024, 2048, 3072, 4095, 4096, -4096, 8192, 32767, -32768):
        for pivot in ((0, 0), (1024, 768), (500, 300), (2048, 1536), (-100, 5000)):
            yield default_case([("change", 0, 0x44, change(f08=angle, f0c=pivot[0], f0e=pivot[1]))])
    for sine in SINES:
        yield default_case([("change", 0, 0x45, change(f08=1500, frame=0, f0c=300, f0e=400))], sine=sine)
    yield default_case([("change", 0, 0x12, change(depth=0)), ("change", 0, 0x12, change(depth=-40)), ("change", 0, 2, change(depth=40)),
                        ("change", 0, 1, change(frame=1)), ("change", 0, 0x11, change(frame=2))])
    yield default_case([("change", 0, 0x11, change(frame=3)), ("change", 0, 0x10, change()), ("change", 0, 4, change(f08=512))])
    # a null record, no mesh, no node chosen yet
    yield default_case([("change", 0, 0x11, None), ("change", 0, 0, change()), ("change", 1, 0x10, change())],
                       objects=[obj(0), obj(-1), obj(1, node_ref=-1), obj(3)])
    yield default_case([("change", 1, 0x54, change()), ("change", 1, 1, change(frame=0)), ("change", 1, 0x54, change())],
                       objects=[obj(0), obj(1, node_ref=-1), obj(1), obj(3)])
    # the old node's flags
    yield default_case([("change", 0, 1, change(frame=1)), ("change", 0, 1, change(frame=0)), ("change", 0, 0x80000, change(base=3))])


def edge_gradient(horizontal):
    kind = "hchange" if horizontal else "vchange"
    slot = 0 if horizontal else 1
    rects = [(0, 0, 0, 0), (0, 0, 2048, 1536), (100, 200, 900, 700), (500, 500, 500, 500), (900, 700, 100, 200)]
    boxes = [box_bits((0, 0, 0), (100, 100, 0)), box_bits((100, 100, 0), (0, 0, 0)), box_bits((5, 5, 0), (5, 5, 0)), box_bits((-10, 20, 3), (60, -40, 2))]
    for flags in (0, 4, 8, 2, 0x10, 0x14, 0x1C, 0x1E, 0x1F, 0x40, 0x12, 0x18, 0xA, 0xC, 0x80000, 0xFFFFFFFF, 1):
        for rect in rects[:3]:
            yield default_case([(kind, slot, flags, change(depth=3, f08=1024, rect=rect, word=512))])
    for box in boxes:
        for rect in rects:
            yield default_case([(kind, slot, 0x14, change(f08=700, rect=rect, f0c=100, f0e=200))],
                               gradients=[grad(True, box=box, mesh=0), grad(False, box=box, mesh=1), grad(True, box=box, mesh=3)])
    for word in (0, 1, 512, 1023, 1024, 1025, 2048, -1, -1024, 0x7FFFFFFF, -0x80000000, 100, 3):
        for fraction in (0.0, 50.0, 100.0, 12.5):
            for extra in (0, 0x10, 2):
                yield default_case([(kind, slot, 8 | extra, change(word=word, depth=-5))],
                                   gradients=[grad(True, fraction=fraction, mesh=0), grad(False, fraction=fraction, mesh=1),
                                              grad(True, fraction=fraction, mesh=3)])
    for fraction in (math.nan, math.inf, -0.0):
        yield default_case([(kind, slot, 8, change(word=0))], gradients=[grad(True, fraction=fraction, mesh=0), grad(False, fraction=fraction, mesh=1),
                                                                       grad(True, mesh=3)])
    yield default_case([(kind, slot, 8, change(word=1024)), (kind, slot, 8, change(word=1024)), (kind, slot, 8, change(word=512)),
                        (kind, slot, 0x18, change(word=512)), (kind, slot, 0x18, change(word=256))])
    yield default_case([(kind, slot, 0x14, change()), (kind, slot, 2, change(depth=9)), (kind, slot, 0x10, change(rect=(1, 2, 3, 4))),
                        (kind, slot, 1, change())])
    yield default_case([(kind, slot, 0x1E, change(word=2000))], gradients=[grad(True, built=0, mesh=0), grad(False, built=0, mesh=1), grad(True, mesh=3)])
    yield default_case([(kind, slot, 0x14, change(f08=2000, f0c=0, f0e=0)), (kind, slot, 4, change(f08=-2000))], sine="sine16")


def edge_hverts():
    for fraction in (0.0, 100.0, 37.5, -1.0, math.nan, math.inf, -0.0, 1e-40):
        yield default_case([("hverts", 0)], gradients=[grad(True, fraction=fraction, mesh=0), grad(False, mesh=1), grad(True, mesh=3)])
    yield default_case([("hverts", 0), ("hchange", 0, 8, change(word=300)), ("hverts", 2), ("hverts", 0)])


def edge_update():
    for m in range(MESHES):
        for matrix in (identity_bits(), translation_bits(1, 2, 3), [fbits(math.nan)] * 16, [0x80000000] * 16, [0xA5A5A5A5 + i for i in range(16)]):
            yield default_case([("update", m, matrix)])
    yield default_case([("update", 0, identity_bits()), ("update", 3, rotation_bits(1.0)), ("update", 0, scale_matrix_bits(2, 3, 4))])


# ======================================================================= the tests


def make_script(name, kinds, edges, ops=10):
    def rnd(r):
        return rcase(r, kinds, r.randint(1, ops))
    return ScriptTest(name, run_ppc, run_host, edge=edges, rnd=rnd)


def mixed_script(r):
    return rcase(r, ["instance", "calc", "change", "change", "change", "hchange", "vchange", "hverts", "update"], 200, good=0.8)


def edge_mixed():
    for sine in ("sine", "sine16"):
        yield default_case([("instance", 0), ("change", 0, 0x55, change(f08=300, rect=(10, 20, 700, 500))), ("hchange", 0, 0x1F, change(word=700)),
                            ("vchange", 1, 0x1F, change(word=300)), ("hverts", 0), ("update", 1, identity_bits()), ("instance", 4),
                            ("change", 4, 0x11, change(frame=1)), ("calc", 4, 0x81, change(frame=1, base=1))], sine=sine)


# ---- the two functions of numbers


def ppc_get_matrix(e, case):
    sine = SINES[case["sine"]]
    saved = bytes(e.read(TPGLOBALS, TABLE_BYTES))
    try:
        e.write(TPGLOBALS, struct.pack(">3I", sine["size"], sine["mask"], sine["scale"]) + struct.pack(f">{len(sine['table'])}I", *sine["table"]))
        out = e.alloc(96, struct.pack(">24I", *([0xA5A5A5A5] * 4), *case["init"], *([0xA5A5A5A5] * 4)))
        st = CL.Stubs()
        st.real = {FN["GetUIMatrix"], FN["Combine1st"], SET_MATRIX_IDENTITY}
        old = st.activate(e)
        try:
            a, b, c, d, x, y, w, h, pe, pf, pg = case["args"]
            args = [out + 16, T.S16(a), T.S16(b), T.S16(c), T.S16(d), T.F(bits_to_float(x)), T.F(bits_to_float(y)), T.F(bits_to_float(w)),
                    T.F(bits_to_float(h)), T.S16(pe), T.S16(pf), T.S16(pg)]
            with HardwareRounding(e):
                T.call_ppc(e, FN["GetUIMatrix"], args)
        finally:
            CL.Stubs.deactivate(e, old)
        words = struct.unpack(">24I", e.read(out, 96))
        return dict(guard=[words[0:4], words[20:24]], matrix=[canon(w) for w in words[4:20]])
    finally:
        e.write(TPGLOBALS, saved)


def bits_to_float(bits):
    return T.bits_f32(bits)


def host_get_matrix(lib, case):
    import ctypes as C
    prototypes(lib)
    lib.tpw_uem_reset()
    sine = SINES[case["sine"]]
    lib.tpw_uem_globals(0, 0, sine["size"], sine["mask"], sine["scale"], u32array(sine["table"]), len(sine["table"]))
    out = u32array(case["init"])
    a, b, c, d, x, y, w, h, pe, pf, pg = case["args"]
    lib.tpw_uem_get_matrix(out, a, b, c, d, x & MASK, y & MASK, w & MASK, h & MASK, pe, pf, pg)
    return dict(guard=[[0xA5A5A5A5] * 4, [0xA5A5A5A5] * 4], matrix=list(out)[:16])


def get_matrix_case(a=100, b=200, c=800, d=500, x=0.0, y=0.0, w=100.0, h=100.0, pe=0, pf=0, pg=0, sine="sine", init=None):
    return dict(args=[a, b, c, d, fbits(x) if isinstance(x, float) else x, fbits(y) if isinstance(y, float) else y,
                      fbits(w) if isinstance(w, float) else w, fbits(h) if isinstance(h, float) else h, pe, pf, pg], sine=sine,
                init=init or [0xC0DE0000 + i for i in range(16)])


def edges_get_matrix():
    for a, b, c, d in ((100, 200, 800, 500), (0, 0, 2048, 1536), (0, 1536, 2048, 1536), (-50, 1600, 10, 10), (1024, 768, 1, 1), (5, 5, 0, 0),
                       (5, 5, 0, 7), (5, 5, 7, 0), (-1, -1, -1, -1), (0x7FFF, -0x8000, 0x7FFF, -0x8000), (300, 400, 640, 480)):
        for x, y, w, h in ((0.0, 0.0, 100.0, 100.0), (0.0, 0.0, 1.0, 1.0), (-10.5, 20.25, 60.0, 30.0), (5.0, 5.0, 0.0, 0.0), (1e10, -1e10, 1e-10, 1e10),
                           (math.nan, 0.0, 1.0, 1.0), (0.0, 0.0, math.inf, 1.0), (0.0, math.inf, 1.0, -math.inf), (-0.0, -0.0, -0.0, -0.0),
                           (3.0, 4.0, -50.0, -80.0)):
            yield get_matrix_case(a, b, c, d, x, y, w, h)
    for g in (0, 1, -1, 100, 512, 1024, 2048, 3072, 4095, 4096, -4096, 8191, 32767, -32768):
        for pe, pf in ((0, 0), (1024, 768), (500, 300), (2048, 1536), (-2000, 3000), (0x7FFF, -0x8000)):
            yield get_matrix_case(pe=pe, pf=pf, pg=g, x=-3.5, y=7.25, w=60.0, h=45.0)
    for sine in SINES:
        for g in (1, 700, -700, 4095, 20000):
            yield get_matrix_case(pe=300, pf=400, pg=g, w=100.0, h=100.0, sine=sine)
    yield get_matrix_case(pg=700, w=math.nan)
    yield get_matrix_case(pg=700, x=math.inf, y=math.nan)
    yield get_matrix_case(c=0, d=0, pg=700, pe=1, pf=2)


def rnd_get_matrix(r):
    wild = 0.15
    return get_matrix_case(rshort(r), rshort(r), rshort(r, 0.2), rshort(r, 0.2), rf(r, wild, -500, 500), rf(r, wild, -500, 500), rf(r, wild, -500, 500),
                           rf(r, wild, -500, 500), rshort(r), rshort(r), r.choice([0, 0, rshort(r), r.randint(-4096, 4096), r.randint(-100, 100)]),
                           sine=r.choice(["sine", "sine", "sine16", "scrambled", "mask0", "mask3", "negative", "huge", "zero"]),
                           init=[r.getrandbits(32) for _ in range(16)])


def ppc_combine(e, case):
    st = CL.Stubs()
    st.real = {FN["Combine1st"]}
    old = st.activate(e)
    try:
        destination = e.alloc(80, struct.pack(">20I", *([0xA5A5A5A5] * 2), *case["dest"], *([0xA5A5A5A5] * 2)))
        source = e.alloc(64, struct.pack(">16I", *case["src"]))
        with HardwareRounding(e):
            T.call_ppc(e, FN["Combine1st"], [destination + 8, destination + 8 if case["alias"] else source])
        words = struct.unpack(">20I", e.read(destination, 80))
        other = struct.unpack(">16I", e.read(source, 64))
    finally:
        CL.Stubs.deactivate(e, old)
    return dict(dest=[canon(w) for w in words[2:18]], guard=[words[0:2], words[18:20]], src=[canon(w) for w in other])


def host_combine(lib, case):
    prototypes(lib)
    destination = u32array(case["dest"])
    source = u32array(case["src"])
    lib.tpw_uem_combine(destination, source, 1 if case["alias"] else 0)
    return dict(dest=list(destination)[:16], guard=[[0xA5A5A5A5] * 2, [0xA5A5A5A5] * 2], src=list(source)[:16])


def combine_case(dest, src, alias=False):
    return dict(dest=dest, src=src, alias=alias)


def edges_combine():
    ident = identity_bits()
    shapes = [ident, translation_bits(10, -20, 30), scale_matrix_bits(2, 3, 4), rotation_bits(0.7, 5, 6), [fbits(1.0 + i * 0.37) for i in range(16)],
              [fbits(math.nan)] * 16, [fbits(math.inf)] * 16, [0x80000000] * 16, [0] * 16, [fbits(1e30)] * 16, [fbits(-1e-30)] * 16]
    for d in shapes:
        for s in shapes:
            yield combine_case(list(d), list(s))
    for d in shapes:
        yield combine_case(list(d), list(d), alias=True)


def rnd_combine(r):
    return combine_case(rmatrix(r, 0.15) if r.random() < 0.5 else [rf(r, 0.15, -100, 100) for _ in range(16)],
                        rmatrix(r, 0.15) if r.random() < 0.5 else [rf(r, 0.15, -100, 100) for _ in range(16)], alias=r.random() < 0.15)


TESTS = [
    FnTest("GetUIMatrix 0x10187ce4", ppc_get_matrix, host_get_matrix, edge=edges_get_matrix, rnd=rnd_get_matrix),
    FnTest("CombineMatricesNoWDest1st 0x100c2694", ppc_combine, host_combine, edge=edges_combine, rnd=rnd_combine),
    make_script("InterfaceDrawEngineMesh::InstanceMe 0x10188580", ["instance", "instance", "change", "calc"], edge_instance),
    make_script("InterfaceDrawEngineMesh::CalculateFrame 0x1018806c", ["calc"], edge_calc, ops=6),
    make_script("InterfaceDrawEngineMesh::Change 0x10188168", ["change", "change", "calc", "instance"], edge_change, ops=8),
    make_script("InterfaceDrawEngineVGradient::Change 0x101895e0", ["vchange"], lambda: edge_gradient(False), ops=6),
    make_script("InterfaceDrawEngineHGradient::Change 0x10189ea0", ["hchange", "hverts"], lambda: edge_gradient(True), ops=6),
    make_script("InterfaceDrawEngineHGradient::CalcVerts 0x10189cd4", ["hverts", "hchange"], edge_hverts, ops=5),
    make_script("UpdateMeshLocalMatrix 0x100652f4", ["update"], edge_update, ops=4),
    ScriptTest("Engine mesh objects: scripts of 200 operations", run_ppc, run_host, edge=edge_mixed, rnd=mixed_script),
]
