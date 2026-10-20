"""The frame's lists: CRenderList (tb_renderlist.cpp), CSpriteManager (tb_sprites.cpp) and
CStarfield (tb_starfield_frame.cpp), 17 functions of the engine library.

Each case builds a small world from one description, on the PowerPC side in emulator memory
and on the host through tpw_fl_make (tests/unit_FrameLists.cpp), runs a script of calls on
both and compares what comes out: the answers, the log of every call the code makes to
something outside the unit, and a dump of the state after each call (the object itself, the
engine's work arrays, the objects the code touches).

Everything the three files call outside themselves is a fake on both sides that records its
arguments and answers from a queue the case fills: the render memory's scratch space, the
math unit (Transform, MultMatrix: the product is a + 3 b + index as words), the draw
primitive (GetNextPrimitive, AddPoly_TLCTEX, ExecuteCurrentPrimitive), the render list
entry's members, the sprites' own drawing members, the texture cache, the texture load
manager, a texture entry's GetBlendingMode and Close, CMatrix::TransformVertex, the error
stack, the assertion and the engine's free. Not faked, and so run for real on both sides:
CsTexture::GetTextureEntry and, in the starfield, CMatrix::operator* (through the fake math
unit). Objects are named by labels the case chooses (see the Labels class); a pointer into
a block is label * 0x1000 + offset.

The original reads r28 uninitialised in RenderSprites (see NOTES.md); the PowerPC side sets
it to 0 before the call, which is what the translation starts from.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_FrameLists"

MASK = 0xFFFFFFFF
DIRTY = 0xA5A5A5A5

# The functions under test (engine addresses) by the name the scripts use.
FN = dict(
    Reset=0x10026478, AddEntry=0x100262e0, RenderByTexture=0x100264b0, Render=0x10026954,
    SMReset=0x10031770, SMRender=0x100317e4, AddSprite=0x10031874, AddPartialSprite=0x100318e4,
    AddList=0x1003195c, SMPrepare=0x100319c8, RenderSprites=0x10031a74,
    RenderPartialSprites=0x10031f08, LoadBlobShadow=0x10031604, SFPrepare=0x1003219c,
    SFExecute=0x10032200, SFClose=0x100320f0, SFDtor=0x1003206c)
# Script function numbers (tpw_fl_run's) and what they call.
RUN = {0: "Reset", 1: "AddEntry", 2: "RenderByTexture", 3: "Render", 10: "SMReset", 11: "SMPrepare",
       12: "SMRender", 13: "AddSprite", 14: "AddPartialSprite", 15: "AddList", 16: "RenderSprites",
       17: "RenderPartialSprites", 18: "LoadBlobShadow", 20: "SFPrepare", 21: "SFExecute", 22: "SFClose",
       23: "SFDtor"}

# Engine data the code reads and writes.
G_DRAWPRIM, G_MAPWHO, G_RMEM, G_MATH, G_FINAL = 0x10052b3c, 0x100af618, 0x10165630, 0x100afe94, 0x100aff70
G_TCACHE, G_TLOAD, G_FREE, G_ENTRYFLAGS = 0x10173184, 0x100af1c8, 0x10051f74, 0x100b0504
SETTINGS, DISPLAY = 0x10165fc8, 0x10165634
FACES, GROUPS, TREE = 0x100c562c, 0x100b162c, 0x10169154
SAVED = [(G_DRAWPRIM, 4), (G_MAPWHO, 4), (G_RMEM, 4), (G_MATH, 4), (G_FINAL, 4), (G_TCACHE, 4), (G_TLOAD, 4),
         (G_FREE, 4), (G_ENTRYFLAGS, 4), (SETTINGS, 4), (DISPLAY, 8), (DISPLAY + 0x948, 4),
         (FACES, 0x14 * 256), (GROUPS, 0x14 * 64)]

# What an empty queue answers (the same constants as the fakes in unit_FrameLists.cpp).
DEFAULT = {"VB": 0, "EU": 1, "LF": 0, "GBM": 0, "GNP": 0, "EX": 1, "AP": 1, "GFB": 0x1234, "SPX": 0, "SP": 0,
           "CRE": 1, "DRS": 1, "ARS": 1, "DR": 1, "AR": 1, "DP": 1, "TV0": 0, "TV1": 0, "TV2": 0, "TV3": 0}


def fb(v):
    return struct.unpack(">I", struct.pack(">f", v))[0]


def s32(v):
    return T.s32(v)


class Labels:
    """The labels a case uses; primary ones are below 200, the second block of an object
    (a texture object's entry array, a node's face sets, a list's instances) is +200."""
    LIST, MATH, RMEM, DRAW, CACHE, LOAD, MAP, MANAGER, STAR = 1, 2, 3, 4, 5, 6, 7, 8, 9
    VIEW, SECOND, FINAL, STARMATRIX, LIGHTS = 10, 11, 12, 13, 14
    PAGE, TEX, CS, ST, NODE, FS, NVERT, NPTS, NWORDS = 20, 30, 50, 80, 90, 130, 150, 154, 158
    VB, LINK, PRIM = 170, 180, 190
    SPRITE, DATA, SLIST = 100, 116, 124
    ENTRY = 300
    STARS, ARRAY = 240, 400


# ------------------------------------------------------------------ PowerPC world


class World:
    """The case's objects in emulator memory, mirroring tpw_fl_make."""

    def __init__(self, e):
        self.e = e
        self.L = e.cl
        self.blocks = []     # (label, base, size)
        self.addr = {}       # label -> address
        self.arrays = {}     # address of a new[] array -> entries
        self.slab = e.alloc(32 * 64)
        self.entry_slab = e.alloc(32 * 0x5c)

    def reg(self, label, base, size):
        self.blocks.append((label, base, size))
        self.addr[label] = base
        self.L.labels[base] = label

    def a(self, label):
        return self.addr.get(label, 0) if label else 0

    def where(self, p):
        if p == 0:
            return 0
        for label, base, size in self.blocks:
            if base <= p < base + size:
                return label * 0x1000 + (p - base)
        return p if p < 0x10000 else -1

    def w32(self, address, *values):
        self.e.write(address, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))

    def make(self, kind, label, v):
        e = self.e
        if kind == 0:                              # page
            self.reg(label, self.slab + v[0] * 64, 64)
        elif kind == 1:                            # texture entry (v[2]: its place in address order)
            base = self.entry_slab + v[2] * 0x5c
            e.write(base, b"\xa5" * 0x5c)
            self.reg(label, base, 0x5c)
            self.w32(base + 8, v[0])
            self.w32(base + 0x18, self.a(v[1]))
        elif kind == 2:                            # texture object
            base = e.alloc(0x10)
            self.reg(label, base, 0x10)
            entries = e.alloc(32)
            self.reg(label + 200, entries, 32)
            self.w32(entries, *[self.a(x) for x in v[:8]])
            self.w32(base + 8, entries)
        elif kind == 3:                            # texture record
            base = e.alloc(8)
            self.reg(label, base, 8)
            e.write(base, struct.pack(">HHI", 0xbeef, v[0] & 0xFFFF, self.a(v[1])))
        elif kind == 4:                            # node
            flags, vcount, sets = v[0], v[1], v[2]
            base = e.alloc(0xa0)
            self.reg(label, base, 0xa0)
            size = max(sets, 1) * 16
            faces = e.alloc(size)
            self.reg(label + 200, faces, size)
            self.w32(base, flags)
            e.write(base + 0x58, struct.pack(">HH", vcount & 0xFFFF, sets & 0xFFFF))
            self.w32(base + 0x60, self.a(v[3]), self.a(v[4]))
            self.w32(base + 0x6c, faces)
            self.w32(base + 0x9c, self.a(v[5]))
            for i in range(sets):
                self.w32(faces + 16 * i, self.a(v[6 + 2 * i]))
                e.write(faces + 16 * i + 4, struct.pack(">H", 0x5a5a))
                self.L.labels[faces + 16 * i] = v[7 + 2 * i]
        elif kind == 5:                            # block
            size = max(v[0], 16)
            self.reg(label, e.alloc(size, bytes([v[1]]) * size), size)
        elif kind == 6:                            # sprite
            base = e.alloc(0x44)
            self.reg(label, base, 0x44)
            self.w32(base + 8, v[0])
            self.w32(base + 0x10, v[1])
            self.w32(base + 0x28, self.a(v[2]))
            self.w32(base + 0x40, DIRTY)
        elif kind == 7:                            # sprite data
            base = e.alloc(0x18)
            self.reg(label, base, 0x18)
            self.w32(base, self.a(v[0]))
        elif kind == 8:                            # sprite list
            n = v[0]
            base = e.alloc(0x28)
            self.reg(label, base, 0x28)
            instances = e.alloc(max(n, 1) * 4)
            self.reg(label + 200, instances, max(n, 1) * 4)
            self.w32(base + 4, n)
            self.w32(base + 0x14, instances)
            self.w32(instances, *[self.a(x) for x in v[1:1 + n]]) if n else None
        elif kind == 9:                            # sprite manager
            base = e.alloc(0x4100)         # (AddPartialSprite may write up to 0x3ffc)
            self.reg(label, base, 0x2410)
            self.w32(base + 0x2400, self.a(v[0]))
        elif kind == 10:                           # starfield
            base = e.alloc(0x28)
            self.reg(label, base, 0x28)
            self.w32(base, v[0], v[1], self.a(v[2]), self.a(v[3]), v[4], v[5], self.a(v[6]))
        elif kind == 11:                           # texture array
            n = v[0]
            base = e.alloc(n * 0x5c, b"\xa5" * (n * 0x5c))
            self.reg(label, base, n * 0x5c)
            self.arrays[base] = n
            for i in range(n):
                entry = base + i * 0x5c
                self.w32(entry + 8, v[1 + 6 * i])
                self.w32(entry + 0x18, self.a(v[2 + 6 * i]))
                self.w32(entry + 0x24, v[3 + 6 * i])      # u offset, v offset, u size, v size
                self.w32(entry + 0x28, v[4 + 6 * i])
                self.w32(entry + 0x1c, v[5 + 6 * i])
                self.w32(entry + 0x20, v[6 + 6 * i])
                if i:                                      # (the first entry has the array's address and label)
                    self.L.labels[entry] = label + 1 + i
        elif kind == 12:                           # matrix
            base = e.alloc(64)
            self.reg(label, base, 64)
            self.w32(base, *v[:16])
        elif kind == 13:                           # render list
            base = e.alloc(0x14004, b"\xa5" * 0x14004)
            self.reg(label, base, 0x14004)
            self.w32(base, 0)
            for i in range(16):
                self.L.labels[base + 4 + 0x14 * i] = Labels.ENTRY + i

    def words(self, label, values):
        self.w32(self.a(label), *values)


# ------------------------------------------------------------------ the fakes


def words_at(e, address, n):
    return list(struct.unpack(">" + "I" * n, e.read(address, 4 * n)))


def matrix_events(e, L, address):
    w = words_at(e, address, 16)
    L.rec("Matrix", w[:8])
    L.rec("Matrix", w[8:])


def pop(L, key):
    return L.ret(key, DEFAULT[key])


def stubs(w):
    """The PowerPC side's fakes for one case."""
    L, st = w.L, CL.Stubs()
    st.real = set(FN.values())

    def lab(a):
        return L.lab(a)

    def answer(e, key, bits=32):
        e.set_r(3, T.u32(pop(L, key)) if bits == 32 else pop(L, key) & 0xFF)

    def pointer(e, key):
        e.set_r(3, w.a(pop(L, key)))

    def vertex_buffer(e):
        L.rec("GetVertexBufferPtr", (lab(e.r(3)), s32(e.r(4))))
        pointer(e, "VB")

    def unrendered(e):
        L.rec("ExecuteUnrendered", (lab(e.r(3)),))
        answer(e, "EU")

    def error(e):
        L.rec("Error", (s32(e.r(4)), s32(e.r(5))))

    def transform_vertex(e):
        this, out, vin = e.r(3), e.r(4), e.r(5)
        L.rec("TransformVertex", tuple(words_at(e, vin, 3)) + (s32(e.r(6)),))
        matrix_events(e, L, this)
        w.w32(out, *[pop(L, "TV%d" % i) for i in range(4)])

    def next_primitive(e):
        L.rec("GetNextPrimitive", (lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), e.r(6), s32(e.r(7)), w.where(e.r(8))))
        pointer(e, "GNP")

    def execute_current(e):
        L.rec("ExecuteCurrent", (lab(e.r(3)), fb(e.f(1))))
        answer(e, "EX")

    def add_poly(e):
        verts, count, stride = e.r(4), s32(e.r(5)), s32(e.r(6))
        L.rec("AddPoly", (lab(e.r(3)), count, stride, lab(e.r(7)), s32(e.r(8))))
        for i in range(min(count, 4)):
            v = words_at(e, verts + i * stride, 8)
            L.rec("Vertex", (v[0], v[1], v[2], v[3], v[4], v[6], v[7]))
        answer(e, "AP")

    def face_set_blend(e):
        L.rec("GetFaceSetBlendMode", (lab(e.r(3)), lab(e.r(4)), lab(e.r(5))))
        answer(e, "GFB")

    def setup_primitive(e):
        L.rec("SetupPrimitive", (lab(e.r(3)), s32(e.r(4))))
        w.w32(e.r(5), pop(L, "SPX"))
        pointer(e, "SP")

    def calc_render_entry(e):
        L.rec("CalcRenderEntry", (lab(e.r(3)), w.where(e.r(4)), s32(e.r(5)), s32(e.r(6))))
        answer(e, "CRE")

    def sprite(name, key=None, with_texture=False):
        def stub(e):
            args = (lab(e.r(3)), lab(e.r(4))) if with_texture else (lab(e.r(3)),)
            L.rec(name, args)
            if key is None:
                e.set_r(3, 1)
            else:
                answer(e, key)
        return stub

    def blending_mode(e):
        L.rec("GetBlendingMode", (lab(e.r(3)),))
        answer(e, "GBM")

    def entry_close(e):
        L.rec("EntryClose", (lab(e.r(3)),))
        e.set_r(3, 1)

    def cache_prepare(e):
        L.rec("TCachePrepare", (lab(e.r(3)), lab(e.r(4)), fb(e.f(1))))

    def load_file(e):
        L.rec("LoadFile", (lab(e.r(3)),), (e.cstr(e.r(4)) if e.r(4) else "(null)",))
        pointer(e, "LF")

    def assertion(e):
        L.rec("assert", (s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4))))

    def destroy_new_array(e):
        block = e.r(3)
        for _ in range(w.arrays.get(block, 0)):
            L.rec("Entry.dtor")
        L.rec("delete", (L.lab(block) + 80,))

    st.internal(0x100271a8, vertex_buffer)
    st.internal(0x10027a8c, unrendered)
    st.internal(0x10008904, error)
    st.internal(0x1001c584, transform_vertex)
    st.internal(0x10006a58, next_primitive)
    st.internal(0x10007360, execute_current)
    st.internal(0x100078c0, add_poly)
    st.internal(0x10025e8c, face_set_blend)
    st.internal(0x10026044, setup_primitive)
    st.internal(0x1002477c, calc_render_entry)
    st.internal(0x1002f3d0, sprite("SpritePrepare"))
    st.internal(0x1002ff20, sprite("DrawRegularShadow", "DRS"))
    st.internal(0x10030210, sprite("AddRegularShadow", "ARS", True))
    st.internal(0x10030a38, sprite("DrawRegular", "DR"))
    st.internal(0x10030ac0, sprite("AddRegular", "AR"))
    st.internal(0x10030b48, sprite("DrawPartial", "DP"))
    st.internal(0x100410a4, blending_mode)
    st.internal(0x10040f7c, entry_close)
    st.internal(0x10032ddc, cache_prepare)
    st.internal(0x10015f9c, load_file)
    st.imp("__assertion_failed", assertion)
    st.imp("__destroy_new_array", destroy_new_array)
    return st


def free_callback(e):
    """The engine's free (the pointer at 0x10051f74): records, then clears the caller's pointer."""
    where = e.r(3)
    e.cl.rec("Free", (e.cl.lab(e.u32(where)),))
    e.write(where, b"\0\0\0\0")


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    # The math unit: a block with the vtable at +4; Transform at vtable +0xc, MultMatrix at +0x20.
    vtable = T.persist_alloc(e, 0x40)
    e.fl_math = T.persist_alloc(e, 0x10)
    e.fl_free = T.fake_function(e, free_callback)
    e.write(e.fl_math + 4, struct.pack(">I", vtable))

    def transform(e):
        L, w = e.cl, e.fl
        L.rec("Transform", (L.lab(e.r(3)), w.where(e.r(5)), w.where(e.r(6)), s32(e.r(7)), s32(e.r(8)), s32(e.r(9))))
        matrix_events(e, L, e.r(4))

    def mult(e):
        L, w = e.cl, e.fl
        a, b, out = e.r(4), e.r(5), e.r(6)
        L.rec("MultMatrix", (L.lab(e.r(3)), w.where(out)))
        matrix_events(e, L, a)
        matrix_events(e, L, b)
        wa, wb = words_at(e, a, 16), words_at(e, b, 16)
        w.w32(out, *[(wa[i] + 3 * wb[i] + i) & MASK for i in range(16)])

    e.write(vtable + 0xc, struct.pack(">I", T.fake_function(e, transform)))
    e.write(vtable + 0x20, struct.pack(">I", T.fake_function(e, mult)))


# ------------------------------------------------------------------ one case on each side


def ppc(e, case):
    e.cl = CL.PpcLog({}, case["queues"])
    L = e.cl
    w = World(e)
    e.fl = w
    saved = [(a, e.read(a, n)) for a, n in SAVED]
    st = stubs(w)
    old = st.activate(e)
    mapped = False
    try:
        # the objects every case has
        L.labels[e.fl_math] = Labels.MATH
        rmem, draw, cache, load = e.alloc(0x80), e.alloc(0x40), e.alloc(0x20), e.alloc(0x20)
        mapwho = e.alloc(0x100)
        for label, base in ((Labels.RMEM, rmem), (Labels.DRAW, draw), (Labels.CACHE, cache), (Labels.LOAD, load),
                            (Labels.MAP, mapwho)):
            w.reg(label, base, 0x20)
        w.w32(G_DRAWPRIM, draw)
        w.w32(G_MAPWHO, mapwho)
        w.w32(G_RMEM, rmem)
        w.w32(G_MATH, e.fl_math)
        w.w32(G_FINAL, 0)
        w.w32(G_TCACHE, cache)
        w.w32(G_TLOAD, load)
        w.w32(G_FREE, e.fl_free)
        w.w32(G_ENTRYFLAGS, 0)
        w.w32(SETTINGS, 0)
        w.w32(DISPLAY, 0, 0)
        w.w32(DISPLAY + 0x948, 0)
        w.w32(mapwho + 0x30, 0)
        w.w32(draw + 8, 0)
        w.w32(draw + 0x1c, 0)
        e.write(FACES, b"\xa5" * (0x14 * 256))
        e.write(GROUPS, b"\xa5" * (0x14 * 64))
        e.write(TREE, b"\xff" * 0xa000)
        for kind, label, v in case["world"]:
            w.make(kind, label, v)
        for label, values in case.get("words", []):
            w.words(label, values)
        for what, value in case["sets"]:
            ppc_set(w, what, value, mapwho, draw)
        if case.get("low_memory"):
            e.uc.mem_map(0, 0x1000)
            mapped = True
        results = []
        for fn, a in case["ops"]:
            results.append(("call", fn, ppc_run(e, w, fn, a)))
            results.append(("dump",) + tuple(ppc_dump(e, w, case["dump"], draw)))
        if mapped:
            e.uc.mem_unmap(0, 0x1000)
            mapped = False
        return {"results": results, "log": list(L.log)}
    finally:
        if mapped:
            e.uc.mem_unmap(0, 0x1000)
        CL.Stubs.deactivate(e, old)
        for a, data in saved:
            e.write(a, data)


def ppc_set(w, what, value, mapwho, draw):
    if what == 0:
        w.w32(SETTINGS, value)
    elif what == 1:
        w.w32(G_ENTRYFLAGS, value)
    elif what == 2:
        w.w32(mapwho + 0x30, w.a(value))
    elif what == 3:
        w.w32(draw + 8, value)
    elif what == 4:
        w.w32(DISPLAY, value)
    elif what == 5:
        w.w32(DISPLAY + 4, value)
    elif what == 6:
        w.w32(DISPLAY + 0x948, DIRTY if value < 0 else w.a(value))
    elif what == 7:
        w.w32(draw + 0x1c, DIRTY if value < 0 else w.a(value))
    elif what == 8:
        w.w32(G_FINAL, w.a(value))


def ppc_run(e, w, fn, a):
    A = w.a
    if fn == 30:
        w.w32(A(a[0]) + 0x2404, a[1], a[2], a[3])
        return 0
    if fn == 31:
        w.w32(A(a[0]), a[1])
        return 0
    entry = FN[RUN[fn]]
    call = lambda args: T.call_ppc(e, entry, args)[0]
    if fn == 0:
        call([A(a[0])])
        return 0
    if fn == 1:
        return s32(call([A(a[0]), A(a[1]), A(a[2]), A(a[5]), A(a[3]), a[4] & MASK]))
    if fn in (2, 3, 11, 12, 17, 20, 21, 22):
        return s32(call([A(a[0])]))
    if fn == 10:
        call([A(a[0])])
        return 0
    if fn in (13, 14, 15):
        return sum(call([A(a[0]), A(a[1])]) & 1 for _ in range(a[2]))
    if fn == 16:
        e.set_r(28, 0)              # the caller's r28, which the original reads
        return s32(call([A(a[0])]))
    if fn == 18:
        path = e.alloc(16, b"blob.tga\0") if a[1] else 0
        return s32(call([A(a[0]), path]))
    if fn == 23:
        call([A(a[0]), 0])
        return 0
    raise ValueError(fn)


def ppc_dump(e, w, spec, draw):
    what, args = spec
    out = []
    put = out.append
    lab = e.cl.lab
    base = w.a(args[0])
    u = e.u32
    if what == 0:
        put(lab(base))
        put(s32(u(base)))
        for i in range(12):
            entry = base + 4 + 0x14 * i
            link, node, flags, vertices, data = words_at(e, entry, 5)
            put(lab(link))
            put(lab(node))
            put(flags)
            put(w.where(vertices))
            put(w.where(data))
        for i in range(64):
            face = FACES + 0x14 * i
            entry, texture, blend, packed, nxt = words_at(e, face, 5)
            if entry == DIRTY:
                break
            put(1)
            put(lab(entry))
            put(lab(texture))
            put(s32(blend))
            put(packed >> 24)
            put((packed >> 16) & 0xFF)
            put((nxt - FACES) // 0x14 + 1 if nxt else 0)
        put(-100)
        for i in range(32):
            group = GROUPS + 0x14 * i
            page, above, below, sorted_, unsorted = words_at(e, group, 5)
            if page == DIRTY:
                break
            put(lab(page))
            put((above - GROUPS) // 0x14 + 1 if above else 0)
            put((below - GROUPS) // 0x14 + 1 if below else 0)
            put((sorted_ - FACES) // 0x14 + 1 if sorted_ else 0)
            put((unsorted - FACES) // 0x14 + 1 if unsorted else 0)
    elif what == 1:
        put(lab(base))
        counts = [s32(u(base + 0x2404 + 4 * i)) for i in range(3)]
        for c in counts:
            put(c)
        put(lab(u(base + 0x2400)))
        put(u(u(base + 0x2400) + 8) if u(base + 0x2400) else 0)
        # (the Mac's partial table has room for 0x80 only: see tb_starfield.h)
        for table, count, limit in ((0, counts[0], 0x800), (0x2000, counts[1], 0x80), (0x2200, counts[2], 0x80)):
            for i in range(min(max(count, 0), limit)):
                put(lab(u(base + table + 4 * i)))
            put(-100)
        for i in range(64):
            node = TREE + 0x14 * i
            above, below, page, first, blend = words_at(e, node, 5)
            if page == MASK:
                break
            put(lab(page))
            for link in (above, below):
                put(-1 if link == MASK else ((link - TREE) // 0x14 + 1 if link else 0))
            put(lab(first))
            put(s32(blend))
        put(-100)
        for label in args[1:]:
            put(lab(u(w.a(label) + 0x40)))
        put(lab(u(DISPLAY + 0x948)))
        put(lab(u(draw + 0x1c)))
    else:
        put(lab(base))
        put(s32(u(base)))
        put(s32(u(base + 4)))
        put(lab(u(base + 8)))
        put(lab(u(base + 0xc)))
        put(u(base + 0x10))
        put(s32(u(base + 0x14)))
        put(lab(u(base + 0x18)))
        put(u(w.a(args[1])) if args[1] else 0)
    return out


def host(lib, case):
    CL.host_begin(lib, {}, case["queues"])
    lib.tpw_fl_reset()
    try:
        results = []
        tpw_make = lib.tpw_fl_make
        tpw_make.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int64)]
        tpw_make.restype = None
        lib.tpw_fl_set.argtypes = [C.c_int32, C.c_int64]
        lib.tpw_fl_words.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.c_int32]
        lib.tpw_fl_run.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
        lib.tpw_fl_run.restype = C.c_int64
        lib.tpw_fl_dump.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.c_int32, C.POINTER(C.c_int64), C.c_int32]
        lib.tpw_fl_dump.restype = C.c_int32
        for kind, label, v in case["world"]:
            tpw_make(kind, label, (C.c_int64 * (len(v) + 1))(*v))
        for label, values in case.get("words", []):
            lib.tpw_fl_words(label, (C.c_int64 * (len(values) + 1))(*values), len(values))
        for what, value in case["sets"]:
            lib.tpw_fl_set(what, value)
        dump_what, dump_args = case["dump"]
        for fn, a in case["ops"]:
            args = (C.c_int64 * 8)(*(list(a) + [0] * (8 - len(a))))
            results.append(("call", fn, lib.tpw_fl_run(fn, args)))
            dargs = (C.c_int64 * (len(dump_args) + 1))(*dump_args)
            out = (C.c_int64 * 20000)()
            n = lib.tpw_fl_dump(dump_what, dargs, len(dump_args) - 1, out, 20000)
            if n > 20000:
                raise RuntimeError("the dump is longer than 20000 numbers")
            results.append(("dump",) + tuple(out[:n]))
        log = CL.host_end(lib)
    finally:
        lib.tpw_fl_cleanup()
    return {"results": results, "log": log}


# ------------------------------------------------------------------ cases


def new_case(dump):
    return dict(world=[], words=[], sets=[], queues={}, ops=[], dump=dump)


def queue(c, key, values):
    c["queues"][key] = list(values)


def rwords(r, n=16):
    """Sixteen words of a matrix: floats mostly, some arbitrary bits."""
    return [fb(r.choice([0.0, 1.0, -1.0, 0.5, 100.0, -3.25, r.uniform(-50, 50)])) if r.random() < 0.8 else r.getrandbits(32)
            for _ in range(n)]


BLENDS = [0x1000, 0x2000, 0x3000, 0x3001, 0x1256, 0x100, 0, -1, -0x10000, 0x7fffffff, -0x80000000, 5, 6, 0x3000, 0x2000]


def pick_fail(r, n, p, ok=1, bad=0):
    """A queue of n answers where each fails with probability p."""
    return [bad if r.random() < p else ok for _ in range(n)]


def rl_world(c, r, o):
    """A small texture world and some nodes for the render list tests; returns the node labels."""
    c["world"].append((13, Labels.LIST, []))
    c["world"].append((12, Labels.VIEW, rwords(r)))
    c["world"].append((12, Labels.SECOND, rwords(r)))
    c["world"].append((5, Labels.LIGHTS, [16, 0, 0]))
    for i in range(8):
        c["world"].append((5, Labels.VB + i, [0x400, 0x22, 0]))
    for i in range(4):
        c["world"].append((5, Labels.LINK + i, [16, 0x33, 0]))
    for i in range(10):
        c["world"].append((5, Labels.PRIM + i, [16, 0xa5, 0]))
    npages = o.get("npages", r.randint(1, 4))
    orders = r.sample(range(32), npages)
    for i in range(npages):
        c["world"].append((0, Labels.PAGE + i, [orders[i]]))
    ntex = o.get("ntex", r.randint(1, 5))
    entry_orders = r.sample(range(32), ntex)
    for i in range(ntex):
        flags = r.choice([0x1000, 0x1000, 0x1000, 0x1000 | r.getrandbits(32), 0, r.getrandbits(32) & ~0x1000])
        page = r.choice([Labels.PAGE + r.randrange(npages)] * 5 + [0])
        c["world"].append((1, Labels.TEX + i, [flags, page, entry_orders[i]]))
    ncs = o.get("ncs", r.randint(1, 3))
    for i in range(ncs):
        entries = [r.choice([Labels.TEX + r.randrange(ntex)] * 3 + [0]) for _ in range(8)]
        c["world"].append((2, Labels.CS + i, entries))
    nst = o.get("nst", r.randint(1, 4))
    for i in range(nst):
        frame = r.choice([0, 0, 1, 2, 7, r.randrange(8)])
        cs = r.choice([Labels.CS + r.randrange(ncs)] * 6 + [0])
        c["world"].append((3, Labels.ST + i, [frame, cs]))
    nnodes = o.get("nnodes", r.randint(1, 4))
    nodes = []
    for k in range(nnodes):
        flags = (r.getrandbits(32) & ~0x01000000 & ~0x02000000) | r.choice([0, 0x01000000])
        vcount = r.choice(o.get("vcounts", [0, 1, 4, 100, 65535]))
        sets = r.randint(0, o.get("maxsets", 4))
        v = [flags, vcount, sets]
        v += [Labels.NVERT + k if r.random() < 0.9 else 0, Labels.NPTS + k if r.random() < 0.8 else 0,
              Labels.NWORDS + k if r.random() < 0.8 else 0]
        for j in range(sets):
            v += [r.choice([Labels.ST + r.randrange(nst)] * 5 + [0]), Labels.FS + 4 * k + j]
        c["world"].append((5, Labels.NVERT + k, [0x60, 0x11, 1]))
        c["world"].append((5, Labels.NPTS + k, [0x40, 0x12, 1]))
        c["world"].append((5, Labels.NWORDS + k, [0x40, 0x13, 1]))
        c["world"].append((4, Labels.NODE + k, v))
        nodes.append(Labels.NODE + k)
    c["sets"].append((0, r.choice([0, 0x40, 0x40, 0xffffffff, r.getrandbits(32)])))
    c["sets"].append((1, r.choice([0, 0x01000000, r.getrandbits(32)])))
    c["sets"].append((2, r.choice([0, Labels.LINK + r.randrange(4)])))
    c["sets"].append((3, r.choice([0x3000, 0x3000, 0x1000, 0x2, 0xffffffff, 0, 0x100])))
    queue(c, "GFB", [r.choice(BLENDS) for _ in range(12)])
    return nodes


def rl_add(c, r, nodes, n, vb_fail=0.1):
    """n AddEntry calls on random nodes; the render memory gives scratch space unless vb_fail."""
    labels = [Labels.VB + i for i in range(8)]
    queue(c, "VB", [0 if r.random() < vb_fail else r.choice(labels) for _ in range(n)] or [labels[0]])
    for _ in range(n):
        c["ops"].append((1, [Labels.LIST, Labels.VIEW, Labels.SECOND, r.choice(nodes), r.choice([0, 1, 0x20, r.getrandbits(32)]),
                             Labels.LIGHTS]))


def rl_render_queues(c, r, fail):
    prims = [Labels.PRIM + i for i in range(10)]
    queue(c, "SP", [0 if r.random() < fail else r.choice(prims) for _ in range(12)])
    queue(c, "SPX", [r.choice([0, 1, -1, r.getrandbits(31)]) for _ in range(12)])
    queue(c, "CRE", pick_fail(r, 12, fail))
    queue(c, "EX", pick_fail(r, 12, fail))
    queue(c, "EU", pick_fail(r, 3, fail))


LIST_DUMP = (0, [Labels.LIST])


def rl_add_case(r, o=None):
    o = o or {}
    c = new_case(LIST_DUMP)
    nodes = rl_world(c, r, o)
    rl_add(c, r, nodes, o.get("n", r.randint(1, 7)), o.get("vb_fail", r.choice([0, 0, 0.2, 0.5])))
    # and now and then a Reset in the middle
    if r.random() < 0.3:
        c["ops"].insert(r.randrange(len(c["ops"]) + 1), (0, [Labels.LIST]))
    return c


def rl_render_case(r, o=None, render=2):
    o = o or {}
    c = new_case(LIST_DUMP)
    nodes = rl_world(c, r, o)
    rl_add(c, r, nodes, o.get("n", r.randint(0, 6)), o.get("vb_fail", r.choice([0, 0, 0, 0.15])))
    rl_render_queues(c, r, o.get("fail", r.choice([0, 0, 0, 0.1, 0.3])))
    c["ops"].append((render, [Labels.LIST]))
    if r.random() < 0.25:
        c["ops"] += [(0, [Labels.LIST])]
        rl_add(c, r, nodes, r.randint(0, 3), 0)
        c["ops"].append((render, [Labels.LIST]))
    return c


def rl_full_case(r, count):
    """AddEntry with the list nearly full: count entries are there already."""
    c = new_case(LIST_DUMP)
    nodes = rl_world(c, r, {"vcounts": [1, 4], "nnodes": 2})
    queue(c, "VB", [Labels.VB])
    c["ops"].append((31, [Labels.LIST, count]))
    for _ in range(3):
        c["ops"].append((1, [Labels.LIST, Labels.VIEW, Labels.SECOND, nodes[0], 7, Labels.LIGHTS]))
    return c


def rl_edges_add():
    r = random.Random(11)
    for count in (0xffe, 0xfff, 0x1000, 0x1001, 0x7fffffff):
        yield rl_full_case(r, count)
    for n in (1, 2, 3):
        yield rl_add_case(r, {"n": n, "vb_fail": 0, "vcounts": [4]})
    yield rl_add_case(r, {"n": 3, "vb_fail": 1.0})
    yield rl_add_case(r, {"n": 4, "vcounts": [0]})
    for seed in range(8):
        yield rl_add_case(random.Random(100 + seed), {"n": 5, "vb_fail": 0, "vcounts": [1, 7, 65535]})


def rl_edges_render(render):
    # an empty list; one entry; many facesets of one page; every blend mode in one group
    r = random.Random(21)
    yield rl_render_case(r, {"n": 0}, render)
    for seed in range(40):
        rr = random.Random(200 + seed)
        yield rl_render_case(rr, {"n": rr.randint(1, 6), "vb_fail": 0, "vcounts": [4, 5], "npages": rr.randint(1, 3),
                                  "ntex": rr.randint(1, 4), "nst": rr.randint(1, 3), "maxsets": 4, "fail": 0}, render)
    for seed in range(20):
        rr = random.Random(300 + seed)
        yield rl_render_case(rr, {"n": rr.randint(2, 6), "vb_fail": 0, "vcounts": [4], "npages": 1, "ntex": rr.randint(1, 4),
                                  "ncs": 1, "nst": 2, "maxsets": 4, "fail": rr.choice([0, 0.2, 0.4])}, render)


def rnd_add(r):
    return rl_add_case(r)


def rnd_render(r):
    return rl_render_case(r)


def rnd_render_full(r):
    return rl_render_case(r, None, 3)


# ---- the sprite manager


def sprite_world(c, r, o):
    """Textures, sprites and lists for the sprite manager tests; returns the sprite labels."""
    shadow = o.get("shadow", 0)
    # the blob shadow texture arrays: two entries, one entry
    c["world"].append((11, Labels.ARRAY, [2, 0x1000, 0, 0, 0, 0, 0, 0x1000, 0, 0, 0, 0, 0]))
    c["world"].append((11, Labels.ARRAY + 8, [1, 0x1000, 0, 0, 0, 0, 0]))
    c["world"].append((9, Labels.MANAGER, [shadow]))
    npages = o.get("npages", r.randint(1, 3))
    orders = r.sample(range(32), npages)
    for i in range(npages):
        c["world"].append((0, Labels.PAGE + i, [orders[i]]))
    ntex = o.get("ntex", r.randint(1, 4))
    entry_orders = r.sample(range(32), ntex)
    for i in range(ntex):
        flags = r.choice([0x1000, 0x1000, 0x1000, 0, 0x1000 | r.getrandbits(32), r.getrandbits(32) & ~0x1000])
        page = r.choice([Labels.PAGE + r.randrange(npages)] * 4 + [0])
        c["world"].append((1, Labels.TEX + i, [flags, page, entry_orders[i]]))
    ndata = o.get("ndata", r.randint(1, 3))
    for i in range(ndata):
        c["world"].append((7, Labels.DATA + i, [Labels.TEX + r.randrange(ntex)]))
    nsprites = o.get("nsprites", r.randint(1, 8))
    depths = [0x3f800000, 0x40000000, 0xc0000000, 0x00000000, 0x80000000, 0x41200000, 0x7f800000, 0xff800000, 0x7fc00000]
    sprites = []
    for i in range(nsprites):
        flags = 0
        for bit in (0x4, 0x2000, 0x200000, 0x20000, 0x4000, 0x10000):
            if r.random() < 0.4:
                flags |= bit
        flags |= r.choice([0, 0, 0x58000, r.getrandbits(32) & 0x00ff0000 & ~0x270000 & ~0x6000])
        flags &= MASK
        depth = r.choice(depths[:6] * 2 + depths[6:]) if o.get("nan", True) else r.choice(depths[:6])
        c["world"].append((6, Labels.SPRITE + i, [depth, flags, Labels.DATA + r.randrange(ndata)]))
        sprites.append(Labels.SPRITE + i)
    return sprites


def sprite_lists(c, r, sprites, nlists):
    lists = []
    for k in range(nlists):
        members = [r.choice(sprites) for _ in range(r.randint(0, 4))]
        c["world"].append((8, Labels.SLIST + 2 * k, [len(members)] + members))
        lists.append(Labels.SLIST + 2 * k)
    return lists


def sprite_queues(c, r, p):
    for key in ("AR", "ARS", "DR", "DRS", "EX", "DP"):
        queue(c, key, pick_fail(r, 16, p))
    queue(c, "GBM", [r.getrandbits(31) for _ in range(4)])


def sprite_dump(sprites):
    return (1, [Labels.MANAGER] + sprites)


DISPLAY_FLAGS = [0, 0x80, 0x20000, 0x40000, 0x80 | 0x20000, 0x20000 | 0x40000, 0x80 | 0x40000, 0xffffffff, 0x7f, 0x60000]


def sprite_add_case(r, o=None):
    """Reset, AddSprite, AddPartialSprite and AddList, with the tables nearly full now and then."""
    o = o or {}
    c = new_case(sprite_dump([]))
    sprites = sprite_world(c, r, {"nsprites": 3})
    lists = sprite_lists(c, r, sprites, 2)
    c["dump"] = sprite_dump(sprites)
    queue(c, "EX", [1])
    for _ in range(r.randint(2, 7)):
        k = r.randrange(5)
        if k == 0:
            c["ops"].append((13, [Labels.MANAGER, r.choice(sprites), r.choice([1, 1, 2, 3])]))
        elif k == 1:
            c["ops"].append((14, [Labels.MANAGER, r.choice(sprites), r.choice([1, 1, 2, 3])]))
        elif k == 2:
            c["ops"].append((15, [Labels.MANAGER, r.choice(lists), r.choice([1, 1, 2, 3])]))
        elif k == 3:
            # (a negative count would be written at: the original would write before the object)
            c["ops"].append((30, [Labels.MANAGER, r.choice([0, 5, 0x7fd, 0x7fe, 0x7ff, 0x800, 0x801, 0x7fffffff]),
                                  r.choice([0, 1, 5, 0x7ff, 0x800, 0x801, 0x7fffffff]),
                                  r.choice([0, 3, 0x7e, 0x7f, 0x80, 0x81, 0x7fffffff])]))
        else:
            c["ops"].append((10, [Labels.MANAGER]))
    return c


def sprite_reset_case(r):
    c = new_case(sprite_dump([]))
    sprites = sprite_world(c, r, {"nsprites": 2, "shadow": r.choice([0, Labels.ARRAY])})
    c["dump"] = sprite_dump(sprites)
    c["ops"] = [(13, [Labels.MANAGER, sprites[0], 2]), (15, [Labels.MANAGER, sprites[0], 1]), (10, [Labels.MANAGER])]
    return c


def sprite_prepare_case(r):
    c = new_case(sprite_dump([]))
    sprites = sprite_world(c, r, {"nsprites": r.randint(1, 6)})
    lists = sprite_lists(c, r, sprites, r.randint(0, 3))
    c["dump"] = sprite_dump(sprites)
    for k in lists:
        c["ops"].append((15, [Labels.MANAGER, k, 1]))
    c["ops"].append((11, [Labels.MANAGER]))
    return c


def sprite_partial_case(r, render=17):
    c = new_case(sprite_dump([]))
    sprites = sprite_world(c, r, {"nsprites": r.randint(1, 5), "shadow": r.choice([0, Labels.ARRAY])})
    c["dump"] = sprite_dump(sprites)
    sprite_queues(c, r, r.choice([0, 0, 0.2, 0.5]))
    c["sets"].append((4, r.choice(DISPLAY_FLAGS)))
    for s in sprites:
        c["ops"].append((14, [Labels.MANAGER, s, 1]))
    if render == 12:
        for s in sprites:
            if r.random() < 0.7:
                c["ops"].append((13, [Labels.MANAGER, s, 1]))
    c["ops"].append((render, [Labels.MANAGER]))
    return c


def sprite_render_case(r, o=None, flags=None, fail=None):
    o = dict(o or {})
    c = new_case(sprite_dump([]))
    shadow = o.get("shadow", r.choice([0, Labels.ARRAY]))
    sprites = sprite_world(c, r, dict(o, shadow=shadow))
    c["dump"] = sprite_dump(sprites)
    sprite_queues(c, r, fail if fail is not None else r.choice([0, 0, 0, 0.1, 0.3]))
    c["sets"].append((4, flags if flags is not None else r.choice(DISPLAY_FLAGS)))
    c["sets"].append((6, r.choice([-1, 0] + sprites[:1])))
    c["sets"].append((7, r.choice([-1, Labels.PRIM, 0])))
    c["world"].append((5, Labels.PRIM, [16, 0xa5, 0]))
    order = list(sprites)
    r.shuffle(order)
    for s in order[:o.get("added", len(order))]:
        c["ops"].append((13, [Labels.MANAGER, s, 1]))
    c["ops"].append((16, [Labels.MANAGER]))
    return c


def sprite_render_edges():
    for flags in DISPLAY_FLAGS:
        for seed in range(3):
            rr = random.Random(500 + seed + flags % 97)
            yield sprite_render_case(rr, {"nsprites": rr.randint(0, 6), "nan": False}, flags, 0)
    yield sprite_render_case(random.Random(9), {"nsprites": 0}, 0, 0)
    yield sprite_render_case(random.Random(9), {"nsprites": 0}, 0x40000, 0)
    # the blob shadow's primitive fails to execute (the one group's own primitive went through)
    for seed in range(6):
        c = sprite_render_case(random.Random(40 + seed), {"nsprites": 1, "shadow": Labels.ARRAY, "nan": False}, 0x40000, 0)
        queue(c, "EX", [1, 0])
        yield c
    for seed in range(30):
        rr = random.Random(700 + seed)
        yield sprite_render_case(rr, {"nsprites": rr.randint(2, 8), "npages": 1, "ntex": 1, "ndata": 1}, rr.choice([0, 0x40000]), rr.choice([0, 0.2]))
    for seed in range(30):
        rr = random.Random(800 + seed)
        yield sprite_render_case(rr, {"nsprites": rr.randint(2, 8), "nan": False}, rr.choice([0, 0x80, 0x40000, 0x20000]), rr.choice([0, 0.15, 0.4]))


def blob_case(r, path=1, current=None, loaded=None):
    c = new_case((1, [Labels.MANAGER]))
    current = r.choice([0, Labels.ARRAY]) if current is None else current
    loaded = r.choice([0, Labels.ARRAY + 8, Labels.ARRAY + 16]) if loaded is None else loaded
    c["world"].append((11, Labels.ARRAY, [2, 0x1000, 0, 0, 0, 0, 0, 0x2000, 0, 0, 0, 0, 0]))
    flags = r.choice([0, 0x80, 0x40, 0xc0, 0xffffffff, r.getrandbits(32)])
    c["world"].append((11, Labels.ARRAY + 8, [1, flags, 0, 0, 0, 0, 0]))
    c["world"].append((11, Labels.ARRAY + 16, [1, r.getrandbits(32), 0, 0, 0, 0, 0]))
    c["world"].append((9, Labels.MANAGER, [current]))
    # (a second call must not be given an array the first one has just freed)
    again = r.choice([0] + [x for x in (Labels.ARRAY + 8, Labels.ARRAY + 16) if x != loaded])
    queue(c, "LF", [loaded, again])
    c["ops"].append((18, [Labels.MANAGER, path]))
    if r.random() < 0.4:
        c["ops"].append((18, [Labels.MANAGER, r.choice([0, 1])]))
    return c


def blob_edges():
    r = random.Random(31)
    for path in (0, 1):
        for current in (0, Labels.ARRAY):
            for loaded in (0, Labels.ARRAY + 8):
                yield blob_case(r, path, current, loaded)


# ---- the starfield


def star_world(c, r, o):
    """A starfield with some stars; returns (label of the primitive it may be given)."""
    n = o.get("count", r.randint(0, 5))
    floats = [0.0, 1.0, -1.0, 0.5, 2.5, -7.25, 100.0, 0.001, 1e10, -1e10, 12345.678]
    c["world"].append((12, Labels.FINAL, rwords(r)))
    c["world"].append((12, Labels.STARMATRIX, rwords(r)))
    c["world"].append((5, Labels.PRIM, [16, 0xa5, 0]))
    texture = 0
    if o.get("texture", r.random() < 0.85):
        texture = Labels.ARRAY
        entries = r.randint(1, 3)
        v = [entries]
        for _ in range(entries):
            flags = r.choice([0x1000, 0x1000, 0, r.getrandbits(32) | 0x1000, r.getrandbits(32) & ~0x1000])
            v += [flags, Labels.PAGE if r.random() < 0.8 else 0, fb(r.choice(floats[:8])), fb(r.choice(floats[:8])),
                  fb(r.choice(floats[:8])), fb(r.choice(floats[:8]))]
        c["world"].append((0, Labels.PAGE, [r.randrange(32)]))
        c["world"].append((11, Labels.ARRAY, v))
    stars = 0
    if o.get("stars", r.random() < 0.85):
        stars = Labels.STARS
        c["world"].append((5, Labels.STARS, [max(n * 12, 16), 0x77, 0]))
        c["words"].append((Labels.STARS, [r.getrandbits(32) for _ in range(3 * max(n, 0))]))
    elif not o.get("close"):
        n = 0                       # (Execute reads the stars: without any, no count is valid)
    matrix = Labels.STARMATRIX if o.get("matrix", r.random() < 0.5) else 0
    size = fb(r.choice(floats[:7]))
    c["world"].append((10, Labels.STAR, [n if o.get("signed", True) else n, r.choice([0, 7, -1]), texture, stars, size,
                                         r.choice([0, 5]), matrix]))
    c["sets"].append((5, fb(r.choice([0.0, 1.0, 0.75, 1.33, 2.0]))))
    c["sets"].append((8, Labels.FINAL))
    queue(c, "GNP", [Labels.PRIM if not o.get("noprim", r.random() < 0.1) else 0])
    z = [fb(1.5), fb(0.25), fb(-2.0), fb(0.0), 0x80000000, 0x7fc00000, fb(1e-30), fb(1e20)]
    queue(c, "TV0", [fb(r.choice(floats)) for _ in range(8)])
    queue(c, "TV1", [fb(r.choice(floats)) for _ in range(8)])
    queue(c, "TV2", [r.choice(z) for _ in range(8)])
    queue(c, "TV3", [fb(r.choice(floats[:7])) for _ in range(8)])
    queue(c, "AP", pick_fail(r, 8, o.get("fail", r.choice([0, 0, 0.1, 0.3]))))
    queue(c, "EX", [r.choice([0, 1, 5, -1])])
    queue(c, "GBM", [r.choice([0x1256, 0, 0x2000, 0x7fffffff, r.getrandbits(31)])])
    return texture, stars


STAR_DUMP = (2, [Labels.STAR, Labels.PRIM])


def star_execute_case(r, o=None):
    o = o or {}
    c = new_case(STAR_DUMP)
    texture, stars = star_world(c, r, o)
    c["ops"].append((21, [Labels.STAR]))
    if o.get("low_memory") or not texture:
        c["low_memory"] = True
    return c


def star_edges():
    for seed in range(40):
        rr = random.Random(900 + seed)
        yield star_execute_case(rr, {"texture": seed % 4 != 0, "stars": True, "count": rr.randint(0, 4), "matrix": seed % 2 == 0,
                                     "noprim": seed % 10 == 9})
    for count in (0, 1, -3):
        rr = random.Random(950 + count)
        yield star_execute_case(rr, {"count": count, "stars": True, "texture": True})


def star_close_case(r):
    c = new_case(STAR_DUMP)
    texture, stars = star_world(c, r, {"count": r.randint(0, 4), "close": True})
    ops = [(20, [Labels.STAR])]
    # a texture array can be freed once: one of Close and the destructor, or neither
    kind = r.choice([22, 23, None])
    if kind is not None:
        ops.append((kind, [Labels.STAR]))
    if not texture and kind is not None and r.random() < 0.5:
        ops.append((r.choice([22, 23]), [Labels.STAR]))
    if stars and r.random() < 0.3:
        ops.insert(0, (21, [Labels.STAR]))
        c["low_memory"] = not texture
    c["ops"] = ops
    return c


def star_close_edges():
    for texture in (True, False):
        for stars in (True, False):
            for kind in (22, 23):
                rr = random.Random(1000 + 4 * kind + 2 * texture + stars)
                c = star_execute_case(rr, {"texture": texture, "stars": stars, "count": 3, "close": True})
                c["ops"] = [(kind, [Labels.STAR])]
                c["low_memory"] = False
                yield c


# ---- the tests


def rnd_sprite_add(r):
    return sprite_add_case(r)


TESTS = [
    FnTest("CRenderList::AddEntry, Reset", ppc, host, edge=rl_edges_add, rnd=rnd_add),
    FnTest("CRenderList::RenderByTexture", ppc, host, edge=lambda: rl_edges_render(2), rnd=rnd_render),
    FnTest("CRenderList::Render", ppc, host, edge=lambda: rl_edges_render(3), rnd=rnd_render_full),
    FnTest("CSpriteManager::AddSprite, AddPartialSprite, AddList, Reset", ppc, host,
           edge=lambda: [sprite_reset_case(random.Random(s)) for s in range(4)], rnd=sprite_add_case),
    FnTest("CSpriteManager::Prepare", ppc, host, rnd=sprite_prepare_case),
    FnTest("CSpriteManager::RenderSprites", ppc, host, edge=sprite_render_edges, rnd=lambda r: sprite_render_case(r)),
    FnTest("CSpriteManager::RenderPartialSprites, Render", ppc, host,
           edge=lambda: [sprite_partial_case(random.Random(s), render) for s in range(10) for render in (12, 17)],
           rnd=lambda r: sprite_partial_case(r, r.choice([12, 17]))),
    FnTest("CSpriteManager::LoadBlobShadow", ppc, host, edge=blob_edges, rnd=blob_case),
    FnTest("CStarfield::Execute", ppc, host, edge=star_edges, rnd=star_execute_case),
    FnTest("CStarfield::Prepare, Close, destructor", ppc, host, edge=star_close_edges, rnd=star_close_case),
]
