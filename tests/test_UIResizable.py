"""Differential tests for the resizable frame and slider draw objects and TbColour's constructor, against libtpw_unit_UIResizable.dylib
(tests/unit_UIResizable.cpp):

  src/ui_DrawObjectResizable.cpp  InterfaceDrawEngineResizableFrame::InstanceMe 0x10188a68 and Change 0x101887f0,
                                  InterfaceDrawEngineResizableSlider::InstanceMe 0x10188ed0 and Change 0x10188cf0
  src/TbColour.cpp                TbColour::TbColour 0x1007d9d4

UIMain_ConvertString (0x10181a7c, now in src/ui_main.cpp) is tested in test_UIDrawText.py, whose library links ui_main.cpp since unit w44
moved the body there (NULL, empty and long texts, with and without the converter, the buffer's bytes).

Real on both sides: the four members and the constructor. Fakes that record the same event on both sides: InstanceMesh, the engine mesh
object's constructor (a new label, the mesh; it writes recognisable words in the frame fields), the base class's CalculateFrame (the flags and
the change's frame words; answers the node the queue "CalculateFrame" names) and Change (the flags, whether there is a record, both
rectangles as they are at that moment). Operator new is the harness's.

The draw objects are tested as scripts on a world: 4 meshes (a tag_sMesh, a header and 0 to 3 nodes, each with a box and 16 vertices in four
structure-of-arrays sets), 4 draw objects (0 and 1 frames, 2 and 3 sliders), the UI's heap and the objects InstanceMe makes. After EACH
operation the answer, the heap traffic (blocks and the operator new size 0x84 on the PowerPC side), the length of the call log and the whole
state (every node's box and vertices, every object's words, class and rectangles, the instances) are compared, and the whole log at the end.
Floats compare by their bits, NaNs as one value.

Quirks the tests pin: InstanceMe answers the object itself unless it is in the library AND has windows using it (mResizable) AND has a mesh,
and then makes an object of its own class with InstanceMesh flags 4 (the base class passes 1); the frame's Change does nothing at all (not
even the base's) without a record, but runs the base's whatever the node; the frame's flag test before the stretch is always true; the
slider reads the mesh header's first node before it looks at anything (the chosen node is not used), changes only the first eight vertices and
only in y, and calls the base's Change even without a record.
"""
import ctypes as C
import math
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIResizable"

MASK = 0xFFFFFFFF
NAN_BITS = 0x7FC00000

FRAME_INSTANCE, FRAME_CHANGE, SLIDER_CHANGE, SLIDER_INSTANCE = 0x10188A68, 0x101887F0, 0x10188CF0, 0x10188ED0
TB_COLOUR = 0x1007D9D4
# the fakes (in-binary functions)
CTOR, INSTANCE_MESH, CALC_FRAME, BASE_CHANGE = 0x10187A28, 0x10061B1C, 0x1018806C, 0x10188168
HEAP_GLOBAL = 0x1038CC60
CORE_VTABLE = 0x102AF8FC
FRAME_VTABLE, SLIDER_VTABLE = 0x102A7C04, 0x102A7BBC

MESHES, NODES, OBJECTS, INSTANCES = 4, 3, 4, 8
OBJECT_WORDS, NODE_WORDS = 41, 54
LAB_MESH, LAB_OBJECT, LAB_HEAP, LAB_NODE = 10, 30, 50, 100
FIRST_NEW = 300


def s16(v):
    return T.s16(v & 0xFFFF)


def u32s16(v):
    return s16(v) & MASK


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MASK))


def w16(e, address, value):
    e.write(address, struct.pack(">h", s16(value)))


def canon(w):
    return NAN_BITS if (w & 0x7F800000) == 0x7F800000 and (w & 0x7FFFFF) != 0 else w


def fbits(x):
    return T.f32_bits(x)


# ======================================================================= the case: a world and a script


def vertex_patch(lo=-50.0, hi=50.0, mid=20.0):
    """16 vertices (x, y, z each) of a 4 x 4 grid: x in (lo, -mid, mid, hi), y the same."""
    xs = (lo, -mid, mid, hi)
    return [fbits(T.f32(v)) for row in range(4) for col in range(4) for v in (xs[col], xs[row], float(4 * row + col))]


def node(box=(-50.0, -50.0, 0.0, 50.0, 50.0, 0.0), vertices=None):
    return dict(box=[fbits(T.f32(v)) for v in box], vertices=vertices or vertex_patch())


def base_words(mesh=0, shown=0, frame_base=0, frame=0, count=3, in_library=1, resizable=1, rect=(100, 200, 900, 700), rect2=(1, 2, 3, 4),
               f28=0, f2a=0, f2c=0, f2e=0, f30=0, f32=0, depth=0, localised=0):
    return [mesh & MASK, shown, frame_base, frame, count, in_library, resizable, *rect, *rect2, f28, f2a, f2c, f2e, f30, f32, depth, localised]


class Case(dict):
    def __repr__(self):
        return "Case(heap=%r q=%r ops=%s)" % (self.get("heap"), self.get("q"), repr(self["ops"])[:700])


def default_meshes():
    return [dict(nodes=[node(), node((-10, -20, 1, 30, 40, 2)), node((0, 0, 0, 100, 100, 0), vertex_patch(0, 100, 40))]),
            dict(nodes=[node()]),
            dict(nodes=[]),
            dict(nodes=[node((-50, -50, 0, 50, 50, 0), vertex_patch(-30, 70, 5)), node()], no_header=False)]


def default_case(ops, **kw):
    case = Case(heap=True, meshes=default_meshes(),
                objects=[dict(frame=True, base=base_words(0)), dict(frame=True, base=base_words(1)),
                         dict(frame=False, base=base_words(0)), dict(frame=False, base=base_words(3))],
                q={"InstanceMesh": [1], "CalculateFrame": [1]}, ops=ops)
    case.update(kw)
    return case


def change(frame=0, base=0, rect=(100, 200, 900, 700)):
    return dict(frame=frame, base=base, rect=rect)


def change_words(c):
    """The 8 words of the host's ChangeRecord; None is no record at all."""
    if c is None:
        return [0] * 8
    rect = c["rect"]
    return [u32s16(c["frame"]), u32s16(c["base"]), *[u32s16(v) for v in (rect or (0, 0, 0, 0))], 1 if rect else 0, 1]


def node_words(n):
    return [*n["box"], *n["vertices"]]


# ======================================================================= the PowerPC side


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.case = case
        e.heap_in_scratch = True
        self.saved = {a: bytes(e.read(a, n)) for a, n in ((HEAP_GLOBAL, 4),)}
        self.mesh_addr, self.node_addr, self.header_addr = [], [], []
        self.objects, self.instances = [], []
        fixed = {}
        self.heap = e.alloc(16, bytes(16))
        fixed[self.heap] = LAB_HEAP
        for m, mesh in enumerate(case["meshes"]):
            mesh_a = e.alloc(0x40, b"\xa5" * 0x40)
            header = e.alloc(0xB8, b"\xa5" * 0xB8)
            nodes = []
            for i, n in enumerate(mesh["nodes"]):
                a = e.alloc(0xA0, b"\xa5" * 0xA0)
                sets = e.alloc(0xC0, bytes(0xC0))
                w32(e, a + 0x60, sets)
                e.write(a + 0x78, struct.pack(">6I", *n["box"]))
                for v in range(16):
                    base = sets + 0x30 * (v >> 2) + 4 * (v & 3)
                    w32(e, base, n["vertices"][3 * v])
                    w32(e, base + 0x10, n["vertices"][3 * v + 1])
                    w32(e, base + 0x20, n["vertices"][3 * v + 2])
                nodes.append(a)
                fixed[a] = LAB_NODE + m * 8 + i
            w32(e, mesh_a + 4, 0 if mesh.get("no_header") else header)
            w32(e, header + 0x78, nodes[0] if nodes else 0)
            fixed[mesh_a] = LAB_MESH + m
            self.mesh_addr.append(mesh_a)
            self.header_addr.append(header)
            self.node_addr.append(nodes)
        for slot, o in enumerate(case["objects"]):
            a = e.alloc(0x84, bytes(0x84))
            self.write_object(a, o["base"])
            w32(e, a + 0x38, FRAME_VTABLE if o["frame"] else SLIDER_VTABLE)
            fixed[a] = LAB_OBJECT + slot
            self.objects.append(a)
        queues = {k: v[:64] for k, v in case["q"].items()}
        self.L = e.cl = CL.PpcLog(fixed, queues, first_new=FIRST_NEW)
        w32(e, HEAP_GLOBAL, self.heap if case["heap"] else 0)
        self.stubs = self.make_stubs()
        self.old = self.stubs.activate(e)

    def write_object(self, a, w):
        e = self.e
        w32(e, a, self.mesh_addr[w[0] - (1 << 32) if w[0] > 0x7FFFFFFF else w[0]] if (w[0] & MASK) != MASK else 0)
        for offset, value in zip((4, 6, 8, 0xA, 0xC, 0xE), w[1:7]):
            w16(e, a + offset, value)
        w32(e, a + 0x10, CORE_VTABLE)
        for offset, value in zip((0x14, 0x16, 0x18, 0x1A), w[7:11]):
            w16(e, a + offset, value)
        w32(e, a + 0x1C, CORE_VTABLE)
        for offset, value in zip((0x20, 0x22, 0x24, 0x26), w[11:15]):
            w16(e, a + offset, value)
        for offset, value in zip((0x28, 0x2A, 0x2C, 0x2E, 0x30, 0x32, 0x34), w[15:22]):
            w16(e, a + offset, value)
        w32(e, a + 0x80, w[22])

    def finish(self):
        CL.Stubs.deactivate(self.e, self.old)
        for a, data in self.saved.items():
            self.e.write(a, data)

    def make_stubs(self):
        st = CL.Stubs()
        st.real = {FRAME_INSTANCE, FRAME_CHANGE, SLIDER_CHANGE, SLIDER_INSTANCE}

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

        def calc_frame(e):
            L = e.cl
            this, flags, record = e.r(3), e.r(4), e.r(5)
            frame = T.s16(struct.unpack(">h", e.read(record + 4, 2))[0]) if record else 0
            base = T.s16(struct.unpack(">h", e.read(record + 0xA, 2))[0]) if record else 0
            L.rec("CalculateFrame", (L.lab(this), flags, 1 if record else 0, frame, base))
            which = L.ret("CalculateFrame")
            answer = 0
            if which > 0:
                m, i = (which - 1) // 8, (which - 1) % 8
                if m < len(self.node_addr) and i < len(self.node_addr[m]):
                    answer = self.node_addr[m][i]
            e.set_r(3, answer)

        def base_change(e):
            L = e.cl
            this, flags, record = e.r(3), e.r(4), e.r(5)
            r1 = struct.unpack(">4h", e.read(this + 0x14, 8))
            r2 = struct.unpack(">4h", e.read(this + 0x20, 8))
            L.rec("BaseChange", (L.lab(this), flags, 1 if record else 0, *r1, r2[0]))
            L.rec("BaseChange2", r2[1:])

        st.internal(CTOR, ctor)
        st.internal(INSTANCE_MESH, instance_mesh)
        st.internal(CALC_FRAME, calc_frame)
        st.internal(BASE_CHANGE, base_change)
        return st

    # ---- the dumps (the order is tpw_ur_dump in the library)
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

    def dump_object(self, a):
        d = bytes(self.e.read(a, 0x84))
        vtable = struct.unpack_from(">I", d, 0x38)[0]
        out = [1 if vtable == FRAME_VTABLE else 2 if vtable == SLIDER_VTABLE else 0, self.mesh_ref(struct.unpack_from(">I", d, 0)[0])]
        out += [v & MASK for v in struct.unpack_from(">6h", d, 4)]
        out += [v & MASK for v in struct.unpack_from(">4h", d, 0x14)]
        out += [v & MASK for v in struct.unpack_from(">4h", d, 0x20)]
        out += [v & MASK for v in struct.unpack_from(">7h", d, 0x28)]
        out += [canon(w) for w in struct.unpack_from(">16I", d, 0x3C)]
        out += [self.node_ref(struct.unpack_from(">I", d, 0x7C)[0]), struct.unpack_from(">I", d, 0x80)[0]]
        assert len(out) == OBJECT_WORDS
        return out

    def dump(self):
        e = self.e
        out = []
        for nodes in self.node_addr:
            for a in nodes:
                box = struct.unpack(">6I", e.read(a + 0x78, 24))
                out += [canon(w) for w in box]
                sets = e.u32(a + 0x60)
                for v in range(16):
                    base = sets + 0x30 * (v >> 2) + 4 * (v & 3)
                    out += [canon(e.u32(base)), canon(e.u32(base + 0x10)), canon(e.u32(base + 0x20))]
        for a in self.objects:
            out += self.dump_object(a)
        for a in self.instances:
            out += self.dump_object(a)
        return struct.pack(f"<{len(out)}I", *out)

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
        w16(e, record + 4, c["frame"])
        w16(e, record + 0xA, c["base"])
        w32(e, record + 0x14, rect if c["rect"] else 0)
        return record

    def class_of(self, a):
        return self.e.u32(a + 0x38)

    def step(self, op):
        e = self.e
        kind = op[0]
        n0 = len(e.alloc_log)
        answer = 0
        a = self.object_address(op[1])
        if a is None:
            answer = -1
        elif kind == "instance":
            fn = FRAME_INSTANCE if self.class_of(a) == FRAME_VTABLE else SLIDER_INSTANCE
            r = T.call_ppc(e, fn, [a])[0]
            if r == 0:
                answer = 0
            elif r == a:
                answer = 1
            else:
                answer = len(self.instances) + 2
                if len(self.instances) < INSTANCES:
                    self.instances.append(r)
        else:
            frame = self.class_of(a) == FRAME_VTABLE
            c = op[3]
            mesh = e.u32(a)
            if c is not None and op[2] & 0x10 and not c["rect"]:
                answer = -1          # (flag 0x10 with a record that has no rectangle: read through NULL)
            elif not frame and (mesh == 0 or e.u32(mesh + 4) == 0):
                answer = -1          # (the slider reads the mesh, and its header, before anything else)
            else:
                T.call_ppc(e, FRAME_CHANGE if frame else SLIDER_CHANGE, [a, op[2] & MASK, self.change_record(c)])
        log = e.alloc_log[n0:]
        news = [x for x in log if x[0] == "new"]
        heap = [len(news), len([x for x in log if x[0] == "new[]"]), len([x for x in log if x[0] == "delete"]),
                len([x for x in log if x[0] == "delete[]"])]
        return dict(answer=answer, heap=heap, sizes=[x[2] for x in news], state=self.dump(), log=len(self.L.log))


def run_ppc(e, case):
    ppc = Ppc(e, case)
    try:
        steps = [ppc.step(op) for op in case["ops"]]
        return dict(steps=steps, log=list(ppc.L.log))
    finally:
        ppc.finish()


# ======================================================================= the host side

_prototyped = set()


def prototypes(lib):
    if id(lib) in _prototyped:
        return
    _prototyped.add(id(lib))
    u32p = C.POINTER(C.c_uint32)
    lib.tpw_ur_reset.argtypes = []
    lib.tpw_ur_globals.argtypes = [C.c_int32]
    lib.tpw_ur_mesh.argtypes = [C.c_int32, C.c_int32, C.c_int32, u32p]
    lib.tpw_ur_object.argtypes = [C.c_int32, C.c_int32, u32p]
    lib.tpw_ur_op.argtypes = [C.c_int32, C.c_int32, C.c_uint32, u32p]
    lib.tpw_ur_op.restype = C.c_int64
    lib.tpw_ur_heap.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ur_dump.argtypes = [u32p]
    lib.tpw_ur_dump.restype = C.c_int32
    lib.tpw_ur_colour.argtypes = [C.c_uint32, C.c_char_p]


def u32array(words):
    return (C.c_uint32 * max(len(words), 1))(*[w & MASK for w in words])


def run_host(lib, case):
    prototypes(lib)
    lib.tpw_ur_reset()
    CL.host_begin(lib, {}, {k: v[:64] for k, v in case["q"].items()}, first_new=FIRST_NEW)
    lib.tpw_ur_globals(1 if case["heap"] else 0)
    for m, mesh in enumerate(case["meshes"]):
        words = [w for n in mesh["nodes"] for w in node_words(n)]
        lib.tpw_ur_mesh(m, len(mesh["nodes"]), 1 if mesh.get("no_header") else 0, u32array(words))
    for slot, o in enumerate(case["objects"]):
        lib.tpw_ur_object(slot, 1 if o["frame"] else 0, u32array(o["base"]))
    kinds = dict(instance=0, change=1)
    steps = []
    heap = (C.c_int64 * 4)()
    dump = (C.c_uint32 * 4096)()
    for op in case["ops"]:
        kind = op[0]
        record = u32array(change_words(op[3]) if kind == "change" else [0] * 8)
        flags = op[2] if kind == "change" else 0
        answer = lib.tpw_ur_op(kinds[kind], op[1], flags & MASK, record)
        lib.tpw_ur_heap(heap)
        n = lib.tpw_ur_dump(dump)
        steps.append(dict(answer=answer, heap=list(heap), sizes=[0x84] * heap[0], state=bytes(dump)[:4 * n],
                          log=C.c_int32.in_dll(lib, "tpw_cl_n").value))
    return dict(steps=steps, log=CL.host_end(lib))


class ScriptTest(FnTest):
    """A scripted test whose failure messages name the state's word."""

    def run_case(self, runner, case):
        messages = FnTest.run_case(self, runner, case)
        out = []
        for message in messages:
            if "state: bytes differ at " in message:
                path, rest = message.split(": bytes differ at ", 1)
                index = int(rest.split(" ")[0]) // 4
                message = f"{message}  [word {index}; nodes {sum(len(m['nodes']) for m in case['meshes']) * NODE_WORDS + 0} words first, then " \
                          f"{OBJECT_WORDS} per object]"
            out.append(message)
        return out


def setup(runner):
    T.install_std_stubs(runner.emu)


# ======================================================================= random worlds and scripts

SPECIAL_FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 2.0, 100.0, 1e-30, 1e30, 3.4e38, -3.4e38, math.inf, -math.inf, 1.4e-45, 1.17549435e-38]


def rf(r, wild=0.04, lo=-150.0, hi=150.0):
    k = r.random()
    if k < wild:
        return fbits(T.rfloat(r, nan=True))
    if k < wild + 0.06:
        return fbits(r.choice(SPECIAL_FLOATS))
    return fbits(T.f32(r.uniform(lo, hi)))


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


def rnode(r):
    k = r.random()
    if k < 0.6:
        lo = r.uniform(-100, 0)
        hi = lo + r.uniform(10, 200)
        return node((lo, lo, r.uniform(-3, 3), hi, hi + r.uniform(-30, 30), r.uniform(-3, 3)), vertex_patch(lo, hi, r.uniform(1, 40)))
    return dict(box=[rf(r, 0.15, -200, 200) for _ in range(6)], vertices=[rf(r, 0.1, -100, 100) for _ in range(48)])


def rmesh(r, good=0.0):
    count = r.choice([1, 2, 3]) if r.random() < good else r.choice([0, 1, 1, 2, 3])
    return dict(nodes=[rnode(r) for _ in range(count)], no_header=r.random() < 0.05)


def robject(r, frame, good=0.0):
    mesh = r.choice([-1, 0, 1, 2, 3]) if r.random() > good else r.choice([0, 1, 3])
    base = base_words(mesh, shown=r.choice([0, 1, -1, 5]), frame_base=r.choice([0, 0, 1, 2, rshort(r)]), frame=r.randint(0, 3),
                      count=rshort(r, 0.2), in_library=r.choice([0, 1, 1, 1, -1, 2]), resizable=r.choice([0, 1, 1, 1, -1, 3]),
                      rect=rrect(r), rect2=rrect(r), f28=rshort(r), f2a=rshort(r), f2c=rshort(r), f2e=rshort(r), f30=rshort(r), f32=rshort(r),
                      depth=rshort(r), localised=r.choice([0, 0, 1, 40, -3, 0x7FFFFFFF]))
    return dict(frame=frame, base=base)


def rflags(r):
    flags = 0
    for bit in (0x1, 0x2, 0x4, 0x8, 0x10, 0x40, 0x80000):
        if r.random() < 0.45:
            flags |= bit
    if r.random() < 0.15:
        flags |= r.choice([0x20, 0x100, 0x400, 0x8000, 0x80000000])
    if r.random() < 0.04:
        flags = r.getrandbits(32)
    return flags & MASK


def rchange(r, flags):
    if r.random() < 0.06:
        return None
    return change(frame=r.randint(0, 3) if r.random() < 0.7 else rshort(r), base=r.choice([0, 0, 1, -1, rshort(r)]),
                  rect=rrect(r) if (flags & 0x10 or r.random() < 0.5) else None)


def rcase(r, n_ops, kinds=("instance", "change", "change", "change"), slots=(0, 1, 2, 3), good=0.7):
    meshes = [rmesh(r, good) for _ in range(MESHES)]
    objects = [robject(r, i < 2, good) for i in range(OBJECTS)]
    q = {"InstanceMesh": [r.choice([0, 1, 2, 3, 4, 4, 0]) for _ in range(12)],
         "CalculateFrame": [r.choice([0, 1, 2, 3, 9, 10, 11, 17, 25, 4, 5]) for _ in range(40)]}
    ops = []
    instances = 0
    for _ in range(n_ops):
        kind = r.choice(kinds)
        if kind == "instance":
            if instances >= 7:
                kind = "change"
            else:
                instances += 1
        # an object slot of the test's kind, now and then one of the instances made so far (they are of the class of the object they came from)
        pool = list(slots)
        slot = r.choice(pool) if r.random() > 0.3 or instances == 0 else OBJECTS + r.randrange(min(instances, 3))
        if kind == "instance":
            ops.append(("instance", slot))
        else:
            flags = rflags(r)
            ops.append(("change", slot, flags, rchange(r, flags)))
    return Case(heap=r.random() < 0.85, meshes=meshes, objects=objects, q=q, ops=ops)


# ======================================================================= edge scripts


def edge_instance(slots):
    """InstanceMe: every combination of the three conditions, with and without a heap and a mesh from InstanceMesh."""
    for slot in slots:
        for in_library in (0, 1, -1):
            for resizable in (0, 1, -1):
                for mesh in (-1, 0):
                    for answer in (0, 1, 4):
                        base = base_words(mesh, frame_base=-7, frame=0x7FFF, count=-0x8000, in_library=in_library, resizable=resizable)
                        objects = default_case([])["objects"]
                        objects[slot] = dict(frame=slot < 2, base=base)
                        yield default_case([("instance", slot)], objects=objects, q={"InstanceMesh": [answer]},
                                           heap=in_library != 0 or resizable != 0)
    a, b = slots
    yield default_case([("instance", a), ("instance", a), ("instance", 4), ("change", 4, 0x10, change()), ("instance", b), ("instance", 6),
                        ("change", 6, 0x10, change(rect=(5, 6, 7, 8)))], q={"InstanceMesh": [2, 3, 0, 4], "CalculateFrame": [1, 2, 9]})
    yield default_case([("instance", 9), ("instance", 4), ("instance", a)], q={"InstanceMesh": [1]}, heap=False)


RECTS = [(0, 0, 0, 0), (0, 0, 2048, 1536), (100, 200, 900, 700), (500, 500, 500, 500), (900, 700, 100, 200), (-100, -50, 100, 50),
         (0, 0, 1, 1), (0, 0, 1, 700), (0, 0, 900, 1), (2000, 1500, 2048, 1536), (0x7FFF, 0x7FFF, -0x8000, -0x8000), (-0x8000, -0x8000, 0x7FFF, 0x7FFF),
         (10, 20, 10, 700), (10, 20, 900, 20), (0, 0, 100, 100), (0, 0, 99, 99), (0, 0, 101, 101)]


def edge_change(frame):
    slots = (0, 1) if frame else (2, 3)
    for flags in (0, 1, 2, 4, 8, 0x10, 0x11, 0x40, 0x80000, 0x80011, 0xFFFFFFFF, 0x20, 0x100, 0x8000):
        for rect in RECTS[:4] + RECTS[6:8]:
            for node_pick in (0, 1, 2, 3, 9, 10):
                yield default_case([("change", slots[0], flags, change(rect=rect))], q={"CalculateFrame": [node_pick]})
    for rect in RECTS:
        for mesh in (0, 3):
            objects = default_case([])["objects"]
            objects[slots[0]] = dict(frame=frame, base=base_words(mesh))
            yield default_case([("change", slots[0], 0x10, change(rect=rect))], objects=objects, q={"CalculateFrame": [1 + 8 * mesh]})
    # the old rectangle (no flag 0x10)
    for stored in RECTS[:6]:
        objects = default_case([])["objects"]
        objects[slots[0]] = dict(frame=frame, base=base_words(0, rect=stored))
        yield default_case([("change", slots[0], 0x1, change())], objects=objects)
    # boxes and vertices of every kind
    boxes = [(-50, -50, 0, 50, 50, 0), (0, 0, 0, 0, 0, 0), (10, 10, 1, -10, -10, 2), (-50, -50, 0, 50, 50, 0), (1e30, 1e30, 0, 3e38, 3e38, 0),
             (-0.0, -0.0, 0, 0.0, 0.0, 0)]
    for box in boxes:
        for patch in (vertex_patch(), vertex_patch(0, 100, 40), vertex_patch(-3e38, 3e38, 1e30), vertex_patch(-1, 1, 0)):
            meshes = default_meshes()
            meshes[0] = dict(nodes=[node(box, patch)])
            yield default_case([("change", slots[0], 0x10, change(rect=(0, 0, 300, 400)))], meshes=meshes, q={"CalculateFrame": [1]})
    # dy = -(height - boxHeight) with the two exactly equal: dy is -0.0 (not +0.0), which shows in a box minimum y of -0.0 (-0.0 + -0.0)
    meshes = default_meshes()
    meshes[0] = dict(nodes=[node((-50.0, -0.0, 0.0, 50.0, 100.0, 0.0), vertex_patch())])
    yield default_case([("change", slots[0], 0x10, change(rect=(0, 0, 100, 100)))], meshes=meshes, q={"CalculateFrame": [1]})
    # NaN, infinity and signed zero in the box and the vertices
    special = [fbits(math.nan), fbits(math.inf), fbits(-math.inf), 0x80000000, 0, fbits(1.0), fbits(-1.0), 0x7F800001, 0xFFC00000]
    for k, value in enumerate(special):
        vertices = vertex_patch()
        vertices[3 * k] = value
        vertices[3 * k + 1] = value
        vertices[3 * k + 2] = value
        box = [fbits(-50.0), fbits(-50.0), 0, fbits(50.0), fbits(50.0), 0]
        box[k % 6] = value
        meshes = default_meshes()
        meshes[0] = dict(nodes=[dict(box=box, vertices=vertices)])
        yield default_case([("change", slots[0], 0x10, change(rect=(0, 0, 300, 400)))], meshes=meshes, q={"CalculateFrame": [1]})
    yield default_case([("change", slots[0], 0x10, change(rect=(0, 0, 300, 400))), ("change", slots[0], 0x10, change(rect=(0, 0, 150, 200))),
                        ("change", slots[0], 0x11, change(frame=1, rect=(0, 0, 150, 200)))], q={"CalculateFrame": [1, 1, 2]})
    # no record, no node, no mesh, no header
    yield default_case([("change", slots[0], 0x11, None), ("change", slots[0], 0, None), ("change", slots[0], 0x10, change())], q={"CalculateFrame": [1]})
    for slot in slots:
        objects = default_case([])["objects"]
        objects[slot] = dict(frame=frame, base=base_words(-1))
        yield default_case([("change", slot, 0x10, change())], objects=objects)
        objects[slot] = dict(frame=frame, base=base_words(2))
        yield default_case([("change", slot, 0x10, change())], objects=objects, q={"CalculateFrame": [0]})
    meshes = default_meshes()
    meshes[0]["no_header"] = True
    yield default_case([("change", slots[0], 0x10, change())], meshes=meshes, q={"CalculateFrame": [1]})


def edge_mixed():
    yield default_case([("instance", 0), ("change", 4, 0x10, change(rect=(0, 0, 400, 300))), ("instance", 2), ("change", 5, 0x10, change(rect=(0, 0, 40, 30))),
                        ("change", 0, 0x11, change(frame=1)), ("change", 2, 0x10, change(rect=(5, 5, 6, 6)))], q={"InstanceMesh": [1, 4], "CalculateFrame": [1, 9, 2, 3]})


def make_script(name, edges, kinds, slots, ops=8):
    def rnd(r):
        return rcase(r, r.randint(1, ops), kinds, slots)
    return ScriptTest(name, run_ppc, run_host, edge=edges, rnd=rnd)


def mixed_script(r):
    return rcase(r, 200, good=0.85)


# ---- TbColour


def ppc_colour(e, case):
    st = CL.Stubs()
    st.real = {TB_COLOUR}
    old = st.activate(e)
    try:
        a = e.alloc(8, bytes([case["fill"]]) * 8)
        T.call_ppc(e, TB_COLOUR, [a])
        return dict(bytes=bytes(e.read(a, 8)))
    finally:
        CL.Stubs.deactivate(e, old)


def host_colour(lib, case):
    prototypes(lib)
    out = C.create_string_buffer(8)
    lib.tpw_ur_colour(case["fill"], out)
    return dict(bytes=bytes(out.raw))


def colour_edges():
    for fill in (0, 1, 0x5A, 0xA5, 0xFF, 0x80):
        yield dict(fill=fill)


def colour_rnd(r):
    return dict(fill=r.getrandbits(8))


TESTS = [
    make_script("InterfaceDrawEngineResizableFrame::InstanceMe 0x10188a68", lambda: edge_instance((0, 1)), ("instance", "instance", "change"), (0, 1)),
    make_script("InterfaceDrawEngineResizableFrame::Change 0x101887f0", lambda: edge_change(True), ("change", "change", "instance"), (0, 1)),
    make_script("InterfaceDrawEngineResizableSlider::InstanceMe 0x10188ed0", lambda: edge_instance((2, 3)), ("instance", "instance", "change"), (2, 3)),
    make_script("InterfaceDrawEngineResizableSlider::Change 0x10188cf0", lambda: edge_change(False), ("change", "change", "instance"), (2, 3)),
    FnTest("TbColour::TbColour 0x1007d9d4", ppc_colour, host_colour, edge=colour_edges, rnd=colour_rnd),
    ScriptTest("Frame and slider draw objects: scripts of 200 operations", run_ppc, run_host, edge=edge_mixed, rnd=mixed_script),
]
