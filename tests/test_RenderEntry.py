"""SRenderListEntry::CalcRenderEntry (src/engine/RenderList.cpp), original versus translation.

Everything CalcRenderEntry calls is a fake on both sides (tests/unit_RenderEntry.cpp has the host's):
DotProduct, CCompBase::AverageColor (virtual), ColorVertices, CClipping::CPP_TestData and ClipPoly
(the overload with an index list, 0x10040b24), CDrawPrimitive::ApplyFog and the math unit's Project
(virtual). Each records its arguments (pointers into the vertex and index memory as byte offsets);
ColorVertices changes the colours it is given, ClipPoly rewrites the triangle before its index
pointer and moves the vertex count, ApplyFog and Project move the vertex count too, so where the
code reads the count again shows. The inline getters (CsTexture::GetTextureEntry, CTextureEntry's
five, CMatrix::Return) run as the original's out-of-line copies.

A case describes the node (its file image: face sets, faces, index words, normals, coordinate
sets, texture records), the texture objects and entries (in threes, so an entry's next planes are
there), the entry's transformed vertices and colours, the two primitives (the case's and the draw
primitive's current one, which may be the same), the vertex and index memory, the globals and the
fakes' data. Compared after the call: the result, the event log, both primitives, the whole vertex
memory (the face set's texture coordinates with NaNs as one value), the whole index memory and the
nine facing values.

The environment-mapped texture coordinates (entry flag 0x8000000) end in a loop over 0xffffffff
vertices in the original (src/engine/RenderList.cpp, QUIRK), so a case that reaches them names the
DotProduct call that ends it on both sides (the host throws, the PowerPC side stops the emulator)
and the state is compared at that point.

Unicorn computes fmadds, fmsubs and fnmsubs in double precision and rounds twice; HardwareRounding
(one code hook per fused instruction of the function, as tests/test_SkyRender.py) writes the
processor's single rounding instead.

Not tested: face numbers outside the face set when clipping (the original reads its stack past the
outside codes), a face set whose last vertex is more than one below its first (a count that runs
for four billion vertices in both), more than 0x2000 vertices (past the codes on the stack in both).
"""
import contextlib
import ctypes as C
import math
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as P

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_RenderEntry"
MASK = 0xffffffff

CALC, CALC_SIZE = 0x1002477c, 5352
DOT, COLOR, TEST_DATA, CLIP_POLY, APPLY_FOG = 0x10025cac, 0x10003570, 0x1003f6dc, 0x10040b24, 0x10007c4c
# CsTexture::GetTextureEntry, CTextureEntry::GetTexture, GetUOffset, GetVOffset, GetUSize, GetVSize,
# CMatrix::Return: inline in the translation, real on the PowerPC side.
INLINES = [0x100171ec, 0x10025e40, 0x10025e08, 0x10025dd0, 0x10025d98, 0x10025d60, 0x10025d24]

DRAW_VAR, SETTINGS, VIEW_VAR, LIGHTS_VAR = 0x10052b3c, 0x10165fc8, 0x100b0078, 0x100af1b8
CLIP_VAR, MATH_VAR, DISPLAY_Z54 = 0x102b3638, 0x100afe94, 0x10165634 + 0x54
# gFaceX0, X1, X2, Y0, Y1, Y2, W0, W1, W2
FACE = [0x101731bc, 0x101731b8, 0x101731b4, 0x101731b0, 0x101731ac, 0x101731a8, 0x101731a4, 0x101731a0, 0x1017319c]

ENTRY_SIZE, RECORD_SIZE = 0x5c, 0x38
TEXTURES, FRAMES, COMPS, PAGES = 6, 4, 4, 8
PAGE_BASE = 0x7a000000          # a page's address on the PowerPC side (never dereferenced)
F38_ADDRESS = 0x7a00ff00        # what a non-null mF38 holds there
POISON = 0xa5a5a5a5
ABORTED = -999
LABEL_DRAW, LABEL_CLIP, LABEL_MATH, LABEL_PAGE, LABEL_COMP = 1, 2, 3, 10, 30

# texture entry flags
ON_PAGE, NEXT_PLANE, STAGES, ROUND_OFF, ENVIRONMENT, COMPRESSED = 0x1000, 0x800000, 0x400, 0x4000000, 0x8000000, 0x10000000


def fb(x):
    return struct.unpack(">I", struct.pack(">f", T.f32(x)))[0]


def bf(word):
    return struct.unpack(">f", struct.pack(">I", word & MASK))[0]


def canon(w):
    """Any NaN as one value."""
    w &= MASK
    return 0x7fc00000 if w & 0x7f800000 == 0x7f800000 and w & 0x007fffff else w


def ws(e, a, values):
    e.write(a, struct.pack(">%dI" % len(values), *[v & MASK for v in values]))


def rs(e, a, n):
    return list(struct.unpack(">%dI" % n, e.read(a, 4 * n)))


# ----------------------------------------------------------------- the emulator's float corrections

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr_double(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    bits = v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]
    return struct.unpack(">d", struct.pack(">Q", bits))[0]


class HardwareRounding:
    """fmadds, fmsubs, fnmadds and fnmsubs as the processor rounds them (once): each such
    instruction of the function gets a code hook that writes fmaf's result (negated for the
    negative forms) to the target register and steps over the instruction. Operands that are not
    finite are left to the emulator (the result is an infinity or a NaN either way)."""

    def __init__(self, e):
        self.e, self.hooks, self.sites = e, [], {}
        words = struct.unpack(">%dI" % (CALC_SIZE // 4), e.read(CALC, CALC_SIZE))
        for k, word in enumerate(words):
            kind = (word >> 1) & 31
            if word >> 26 == 59 and kind in (28, 29, 30, 31):
                self.sites[CALC + 4 * k] = (kind, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31,
                                            (word >> 6) & 31)

    def __enter__(self):
        for address in sorted(self.sites):
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=address, end=address))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        kind, rd, ra, rb, rc = self.sites[address]
        a, c, b = fpr_double(self.e, ra), fpr_double(self.e, rc), fpr_double(self.e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        value = float(LIBM.fmaf(a, c, -b if kind in (28, 30) else b))
        if kind in (30, 31):
            value = -value
        uc.reg_write(ppcemu.FPR[rd], struct.unpack(">Q", struct.pack(">d", value))[0])
        uc.reg_write(P.UC_PPC_REG_PC, address + 4)


# ----------------------------------------------------------------- the node's image


class Image:
    """The node and the records it points to, in the file's layout: described once, rendered
    big-endian with absolute addresses (PowerPC) or little-endian with offsets in the image that
    the host turns into Ptr32s (the `relocs`). The first 16 bytes are padding, so no pointer
    target is at offset 0."""

    def __init__(self):
        self.size = 0x10
        self.fields, self.pointers = [], []

    def reserve(self, n, align=4):
        self.size = (self.size + align - 1) & ~(align - 1)
        at = self.size
        self.size += n
        return at

    def put(self, at, fmt, *values):
        self.fields.append((at, fmt, values))

    def ptr(self, at, target):
        self.pointers.append((at, target))

    def render(self, big, base=0):
        out = bytearray(b"\xa5" * self.size)
        e = ">" if big else "<"
        for at, fmt, values in self.fields:
            struct.pack_into(e + fmt, out, at, *values)
        relocs = []
        for at, target in self.pointers:
            struct.pack_into(e + "I", out, at, 0 if target is None else (base + target if big else target))
            relocs.append(at)
        return bytes(out), relocs


def build_image(case):
    """The image of a case; returns it with the offsets of the node and the texture records."""
    img = Image()
    flags, f5c, f5e, f98 = case["node"]
    sets, faces, f94 = case["sets"], case["faces"], case["f94"]
    points, uvs, records = case["points"], case["uvs"], case["records"]
    node = img.reserve(0xa0)
    sets_at = img.reserve(0x10 * max(1, len(sets)))
    faces_at = img.reserve(8 * max(1, len(faces)))
    f94_at = img.reserve(2 * max(2, len(f94)))
    points_at = img.reserve(4 * max(3, len(points)))
    uvs_at = img.reserve(4 * max(8, len(uvs)))
    records_at = img.reserve(8 * max(1, len(records)))
    img.put(node, "I", flags)
    img.put(node + 0x5a, "HHH", len(sets), f5c, f5e)
    img.ptr(node + 0x64, points_at)
    img.ptr(node + 0x68, uvs_at)
    img.ptr(node + 0x6c, sets_at)
    img.ptr(node + 0x70, faces_at)
    img.ptr(node + 0x94, f94_at)
    img.ptr(node + 0x98, records_at + 8 * f98 if f98 >= 0 else None)
    for i, (record, first_face, face_count, first, last, byte) in enumerate(sets):
        a = sets_at + 0x10 * i
        img.ptr(a, records_at + 8 * record if record >= 0 else None)
        img.put(a + 4, "HHHHB", first_face, face_count, first, last, byte)
    for i, face in enumerate(faces):
        img.put(faces_at + 8 * i, "HHHH", *face)
    if f94:
        img.put(f94_at, "%dH" % len(f94), *f94)
    if points:
        img.put(points_at, "%dI" % len(points), *points)
    if uvs:
        img.put(uvs_at, "%dI" % len(uvs), *uvs)
    for i, (f00, frame, texture) in enumerate(records):
        img.put(records_at + 8 * i, "HHI", f00, frame, 0)     # the pointer is set per side
    return img, node, records_at


# ----------------------------------------------------------------- the PowerPC side


def setup(runner):
    """Standard stubs, plus the vtables of the two virtual fakes (they live across cases)."""
    e = runner.emu
    T.install_std_stubs(e)
    if hasattr(e, "re_math"):
        return
    average = T.fake_function(e, lambda em: em.re.average(em))
    project = T.fake_function(e, lambda em: em.re.project(em))
    # CCompBase: the vtable pointer at +0, AverageColor in the slot at +0x14.
    e.re_comp_vtable = T.persist_alloc(e, 0x20)
    ws(e, e.re_comp_vtable + 0x14, [average])
    # The math unit: +4 is the vtable pointer, whose slot at +0x14 is Project.
    vtable = T.persist_alloc(e, 0x40)
    ws(e, vtable + 0x14, [project])
    e.re_math = T.persist_alloc(e, 0x10)
    ws(e, e.re_math, [0, vtable])


def page_address(label):
    if label == 0:
        return 0
    return POISON if label < 0 else PAGE_BASE + 0x100 * label


@contextlib.contextmanager
def guarded(e):
    """The engine globals a case overwrites, put back afterwards (other modules share them)."""
    regions = [(DRAW_VAR, 4), (SETTINGS, 4), (VIEW_VAR, 4), (LIGHTS_VAR, 4), (CLIP_VAR, 4), (MATH_VAR, 4),
               (DISPLAY_Z54, 4), (FACE[-1], 0x24)]
    saved = [(a, e.read(a, n)) for a, n in regions]
    try:
        yield
    finally:
        for a, data in saved:
            e.write(a, data)


class PpcSide:
    """Memory, labels and fakes of one case on the PowerPC side."""

    def __init__(self, e, case):
        self.e, self.case = e, case
        e.re = self
        self.dot_calls = self.clip_calls = 0
        self.aborted = False
        fixed = {e.re_math: LABEL_MATH}
        for k in range(1, PAGES + 1):
            fixed[page_address(k)] = LABEL_PAGE + k
        # the node's image
        img, node, records_at = build_image(case)
        self.image = e.alloc(img.size)
        data, _ = img.render(True, self.image)
        e.write(self.image, data)
        # the texture entries (two blank ones after them) and the compressed images
        self.comps = []
        for k in range(COMPS):
            a = e.alloc(8)
            ws(e, a, [e.re_comp_vtable, POISON])
            self.comps.append(a)
            fixed[a] = LABEL_COMP + k
        entries = case["entries"]
        self.entries = e.alloc(ENTRY_SIZE * (len(entries) + 2))
        for k, (flags, page, us, vs, uo, vo, f38, comp) in enumerate(entries):
            a = self.entries + ENTRY_SIZE * k
            ws(e, a + 8, [flags])
            ws(e, a + 0x18, [page_address(page), us, vs, uo, vo])
            ws(e, a + 0x38, [F38_ADDRESS if f38 else 0, self.comps[comp] if comp >= 0 else 0])
        # the texture objects and the records that point at them
        textures = []
        for t in range(TEXTURES):
            frames = case["textures"][t] if t < len(case["textures"]) else [-1] * FRAMES
            array = e.alloc(4 * FRAMES)
            ws(e, array, [self.entries + ENTRY_SIZE * k if k >= 0 else 0 for k in frames])
            a = e.alloc(0x10)
            ws(e, a, [POISON, POISON, array, POISON])
            textures.append(a)
        for i, (f00, frame, texture) in enumerate(case["records"]):
            ws(e, self.image + records_at + 8 * i + 4, [textures[texture] if texture >= 0 else 0])
        # the render list entry
        src, colours = case["source"], case["colours"]
        self.source = e.alloc(4 * max(4, len(src)))
        ws(e, self.source, src)
        self.colours = e.alloc(4 * max(1, len(colours or [])))
        if colours:
            ws(e, self.colours, colours)
        self.this = e.alloc(0x14)
        ws(e, self.this, [POISON, self.image + node, case["flags"], self.source, self.colours if colours is not None else 0])
        # the vertex and index memory, the primitives, the draw primitive
        self.vwords, self.icount = len(case["vertices"]), len(case["indices"])
        self.vertices = e.alloc(4 * self.vwords)
        ws(e, self.vertices, case["vertices"])
        self.indices = e.alloc(2 * self.icount + 4)
        e.write(self.indices, struct.pack(">%dH" % self.icount, *case["indices"]))
        self.prims = []
        for p in case["prims"]:
            a = e.alloc(RECORD_SIZE)
            ws(e, a, [p[0], p[1], page_address(p[2]), page_address(p[3]), page_address(p[4]), POISON, p[5], p[6], p[7],
                      p[8], self.vertices + p[9], self.indices + p[10], p[11], p[12]])
            self.prims.append(a)
        vsize, current, vend, iend = case["draw"]
        self.draw = e.alloc(0x30, b"\xa5" * 0x30)
        ws(e, self.draw + 0x18, [vsize, self.prims[current]])
        ws(e, self.draw + 0x24, [self.vertices + vend, self.indices + iend])
        clipping = e.alloc(8, b"\xa5" * 8)
        lights = e.alloc(8)
        settings, mask, z54 = case["g"]
        ws(e, lights, [POISON, mask])
        view = e.alloc(64)
        ws(e, view, case["view"])
        fixed.update({self.draw: LABEL_DRAW, clipping: LABEL_CLIP})
        ws(e, DRAW_VAR, [self.draw])
        ws(e, SETTINGS, [settings])
        ws(e, VIEW_VAR, [view])
        ws(e, LIGHTS_VAR, [lights])
        ws(e, CLIP_VAR, [clipping])
        ws(e, MATH_VAR, [e.re_math])
        ws(e, DISPLAY_Z54, [z54])
        for a, w in zip(FACE, case["face"]):
            ws(e, a, [w])
        self.L = CL.PpcLog(fixed, case["q"])
        e.cl = self.L

    def voff(self, a):
        return T.s32(a - self.vertices)

    def ioff(self, a):
        return T.s32(a - self.indices)

    def in_vertices(self, a, n):
        return self.vertices <= a and a + n <= self.vertices + 4 * self.vwords

    def in_indices(self, a, n):
        return self.indices <= a and a + n <= self.indices + 2 * self.icount

    # -- the fakes
    def stubs(self):
        st = CL.Stubs()
        st.real = {CALC, *INLINES}
        st.internal(DOT, self.dot)
        st.internal(COLOR, self.color)
        st.internal(TEST_DATA, self.test_data)
        st.internal(CLIP_POLY, self.clip_poly)
        st.internal(APPLY_FOG, self.apply_fog)
        return st

    def dot(self, e):
        self.L.rec("DotProduct", [e.r(i) & MASK for i in range(3, 9)])
        self.dot_calls += 1
        if self.dot_calls == self.case["abort"]:
            self.aborted = True
            e.uc.emu_stop()
            return
        dots = self.case["dots"]
        e.set_f(1, bf(dots[min(self.dot_calls - 1, len(dots) - 1)]) if dots else 0.0)

    def average(self, e):
        self.L.rec("AverageColor", [self.L.lab(e.r(3))])
        e.set_r(3, self.L.ret("Average", 0) & MASK)

    def color(self, e):
        v, colour, count, stride = e.r(3), e.r(4) & MASK, T.s32(e.r(5)), T.s32(e.r(6))
        self.L.rec("ColorVertices", [self.voff(v), colour, count, stride])
        for i in range(max(count, 0)):
            a = v + i * stride + 0x10
            if self.in_vertices(a, 4):
                ws(e, a, [e.u32(a) * 3 + colour + i])

    def test_data(self, e):
        L = self.L
        result, clipping, codes, vertices = e.r(3), e.r(4), e.r(5), e.r(6)
        count, stride, mask = T.s32(e.r(7)), T.s32(e.r(8)), e.r(9) & MASK
        L.rec("CPP_TestData", [L.lab(clipping), self.voff(vertices), count, stride, mask])
        given = self.case["codes"]
        out = bytes((given[i] if i < len(given) else 0) & mask & 0xff for i in range(max(count, 0)))
        if out:
            e.write(codes, out)
        every = 0xff
        for b in out:
            every &= b
        anyone = 0
        for b in out:
            anyone |= b
        e.write(result, bytes([L.ret("TestData", every if count > 0 else 0) & 0xff, anyone]))

    def clip_poly(self, e):
        L = self.L
        clipping, planes, indices, vertices, count_at, stride = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7), T.s32(e.r(8))
        inside = self.in_indices(indices - 6, 6)
        triangle = list(struct.unpack(">3H", e.read(indices - 6, 6))) if inside else [-1, -1, -1]
        count = T.s32(e.u32(count_at))
        L.rec("ClipPoly", [L.lab(clipping), T.s32(planes), self.ioff(indices), *triangle, self.voff(vertices), count,
                           stride])
        extra = L.ret("Clip.tri", 0)
        for k in range(3 + 3 * extra):
            a = indices - 6 + 2 * k
            if self.in_indices(a, 2):
                e.write(a, struct.pack(">H", (self.case["salt"] + 37 * self.clip_calls + 11 * k) & 0xffff))
        self.clip_calls += 1
        ws(e, count_at, [count + L.ret("Clip.add", 0)])
        e.set_r(3, (indices + 6 * extra) & MASK)

    def apply_fog(self, e):
        L = self.L
        L.rec("ApplyFog", [L.lab(e.r(3)), self.voff(e.r(4)), T.s32(e.r(5))])
        a = self.prims[0] + 0x20
        ws(e, a, [e.u32(a) + L.ret("Fog.add", 0)])

    def project(self, e):
        L = self.L
        L.rec("Project", [L.lab(e.r(3)), self.voff(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7))])
        a = self.prims[0] + 0x20
        ws(e, a, [e.u32(a) + L.ret("Project.add", 0)])
        poke = L.ret("Project.poke", -1)
        if poke >= 0:
            ws(e, self.prims[0] + 0x30, [poke])
        e.set_f(1, bf(L.ret("Project", 0)))

    # -- after the call
    def state(self):
        e, L = self.e, self.L
        prims = []
        for a in self.prims:
            w = rs(e, a, 14)
            prims.append([T.s32(w[0]), T.s32(w[1]), L.lab(w[2]), L.lab(w[3]), L.lab(w[4]), T.s32(w[6]), T.s32(w[7]),
                          T.s32(w[8]), T.s32(w[9]), self.voff(w[10]), self.ioff(w[11]), T.s32(w[12]), T.s32(w[13])])
        face = [e.u32(a) for a in FACE]
        vertices = rs(e, self.vertices, self.vwords)
        indices = list(struct.unpack(">%dH" % self.icount, e.read(self.indices, 2 * self.icount)))
        return prims, face, vertices, indices


def canon_state(case, ret, log, prims, face, vertices, indices):
    """The comparable result: NaNs as one value in the depths, the facing values and the face
    set's texture coordinates."""
    stride = case["stride"]
    p = case["prims"][0]
    first, last = case["sets"][case["faceset"]][3:5]
    start = p[9] + stride * p[7]
    out = list(vertices)
    for i in range(max(0, last - first + 1)):
        for d in (0x18, 0x1c, 0x20, 0x24):
            a = start + i * stride + d
            if a % 4 == 0 and 0 <= a < 4 * len(out):
                out[a // 4] = canon(out[a // 4])
    prims = [[*q[:11], canon(q[11]), q[12]] for q in prims]
    return {"ret": ret, "log": log, "prims": prims, "face": [canon(w) for w in face],
            "vertices": struct.pack("<%dI" % len(out), *[w & MASK for w in out]),
            "indices": struct.pack("<%dH" % len(indices), *indices)}


def ppc(e, case):
    with guarded(e):
        side = PpcSide(e, case)
        stubs = side.stubs()
        saved = stubs.activate(e)
        try:
            # With the node's second texture the original reads that entry's flags before it tests
            # the entry for NULL (address 8): page 0 is there, zero-filled, for those cases only.
            page_zero = case["node"][0] & 0x02000000 != 0
            if page_zero:
                e.uc.mem_map(0, 0x1000)
            try:
                with HardwareRounding(e):
                    try:
                        r3 = T.s32(T.call_ppc(e, CALC, [side.this, side.prims[0], case["faceset"], case["stride"]])[0])
                    except RuntimeError:
                        if not side.aborted:
                            raise
                        r3 = ABORTED
            finally:
                if page_zero:
                    e.uc.mem_unmap(0, 0x1000)
            return canon_state(case, r3, side.L.log, *side.state())
        finally:
            stubs.deactivate(e, saved)


# ----------------------------------------------------------------- the host side


def array(ctype, values):
    return (ctype * max(1, len(values)))(*values)


def host(lib, case):
    lib.tpw_re_pointer.restype = C.c_void_p
    lib.tpw_re_pointer.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_re_image.argtypes = [C.c_char_p, C.c_int32, C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_re_entries.argtypes = [C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_re_textures.argtypes = [C.POINTER(C.c_int32), C.c_int32]
    lib.tpw_re_record_texture.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_re_entry.argtypes = [C.c_int32, C.c_uint32, C.POINTER(C.c_uint32), C.c_int32, C.POINTER(C.c_uint32),
                                 C.c_int32, C.c_int32]
    lib.tpw_re_globals.argtypes = [C.c_uint32, C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32)]
    lib.tpw_re_areas.argtypes = [C.POINTER(C.c_uint32), C.c_int32, C.POINTER(C.c_uint16), C.c_int32]
    lib.tpw_re_draw.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_re_prim.argtypes = [C.c_int32, C.POINTER(C.c_uint32)]
    lib.tpw_re_data.argtypes = [C.POINTER(C.c_uint8), C.c_int32, C.POINTER(C.c_uint32), C.c_int32, C.c_int32,
                                C.c_uint32, C.POINTER(C.c_uint32)]
    lib.tpw_re_run.restype = C.c_int32
    lib.tpw_re_run.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_re_state.argtypes = [C.POINTER(C.c_int32), C.POINTER(C.c_uint32), C.POINTER(C.c_uint32),
                                 C.POINTER(C.c_uint16)]

    img, node, records_at = build_image(case)
    data, relocs = img.render(False)
    lib.tpw_re_image(data, len(data), array(C.c_uint32, relocs), len(relocs))
    entries = case["entries"]
    lib.tpw_re_entries(array(C.c_uint32, [w & MASK for entry in entries for w in entry]), len(entries))
    frames = [k for t in case["textures"] for k in t]
    lib.tpw_re_textures(array(C.c_int32, frames), len(case["textures"]))
    for i, (f00, frame, texture) in enumerate(case["records"]):
        lib.tpw_re_record_texture(records_at + 8 * i, texture)
    src, colours = case["source"], case["colours"]
    lib.tpw_re_entry(node, case["flags"], array(C.c_uint32, src), len(src), array(C.c_uint32, colours or []),
                     len(colours or []), 0 if colours is None else 1)
    settings, mask, z54 = case["g"]
    lib.tpw_re_globals(settings, mask, z54, array(C.c_uint32, case["view"]))
    lib.tpw_re_areas(array(C.c_uint32, case["vertices"]), len(case["vertices"]), array(C.c_uint16, case["indices"]),
                     len(case["indices"]))
    for i, p in enumerate(case["prims"]):
        lib.tpw_re_prim(i, array(C.c_uint32, [w & MASK for w in p]))
    lib.tpw_re_draw(*case["draw"])
    lib.tpw_re_data(array(C.c_uint8, case["codes"]), len(case["codes"]), array(C.c_uint32, case["dots"]),
                    len(case["dots"]), case["abort"], case["salt"], array(C.c_uint32, case["face"]))

    fixed = {LABEL_DRAW: lib.tpw_re_pointer(0, 0), LABEL_CLIP: lib.tpw_re_pointer(1, 0),
             LABEL_MATH: lib.tpw_re_pointer(2, 0)}
    for k in range(1, PAGES + 1):
        fixed[LABEL_PAGE + k] = lib.tpw_re_pointer(3, k)
    for k in range(COMPS):
        fixed[LABEL_COMP + k] = lib.tpw_re_pointer(4, k)
    CL.host_begin(lib, fixed, case["q"])
    r = lib.tpw_re_run(case["faceset"], case["stride"])
    log = CL.host_end(lib)
    prims = (C.c_int32 * 26)()
    face = (C.c_uint32 * 9)()
    vertices = (C.c_uint32 * len(case["vertices"]))()
    indices = (C.c_uint16 * max(1, len(case["indices"])))()
    lib.tpw_re_state(prims, face, vertices, indices)
    p = list(prims)
    return canon_state(case, r, log, [p[:13], p[13:]], list(face), list(vertices),
                       list(indices)[:len(case["indices"])])


# ----------------------------------------------------------------- cases
#
# A case is a dict:
#   stride, faceset, flags        the call's vertex size and face set, the entry's flags
#   node                          [flags, mF5C, mF5E, record of mF98 (-1: none)]
#   sets                          (record (-1: none), first face, face count, first vertex, last vertex, flag byte)
#   faces                         (word, three vertex numbers)
#   f94, points, uvs              the node's index halfwords, normals (3 words each), coordinate sets (8 words each)
#   records                       (mF00, frame, texture object (-1: none))
#   textures                      per texture object, FRAMES entry numbers (-1: NULL)
#   entries                       (flags, page, U size, V size, U offset, V offset, mF38 set, compressed image (-1))
#   source, colours               the entry's transformed vertices (4 words each) and colours (None: no data)
#   g                             [texture settings flags, light manager mask, display mZ54 bits]
#   view                          the view matrix (16 words)
#   draw                          [vertex size, current primitive (0, 1), vertex end, index end (byte offsets)]
#   prims                         two primitives: [flags, blending mode, page, page 2, page 3, type, format,
#                                 vertex count, index count, vertex data, index data (byte offsets), depth, mF34]
#   vertices, indices             the vertex memory (words) and index memory (halfwords)
#   codes, dots, abort, salt      the fakes' data (see tests/unit_RenderEntry.cpp)
#   face                          the nine facing values before the call
#   q                             queues

# x0 y0 w0 x1 y1 w1 x2 y2 w2 of two triangles (float bits), one facing the viewer and one not.
FACE_TIES = [
    [0xc32de587, 0x42fcdb72, 0x3fdee556, 0xc346855a, 0x43108d55, 0x3fa98fc4, 0xc3663aad, 0x4327e40f, 0x3f489eb5],
    [0xc376e0c0, 0xc30cdb7c, 0x3fd6e4a8, 0xc3664686, 0xc30365c1, 0x3f4e04ba, 0xc36eb288, 0xc308327c, 0x3f97f395],
]

SPECIAL = [0.0, -0.0, 1.0, -1.0, 0.5, 1e30, -1e30, 3.4028234663852886e38, math.inf, -math.inf, math.nan, 1e-40]


def rfl(r, lo=-4.0, hi=4.0, special=0.03):
    """A random float's bits: in [lo, hi], or now and then an edge value."""
    if r.random() < special:
        return fb(r.choice(SPECIAL))
    return fb(r.uniform(lo, hi))


def quad_source(n, r=None):
    """Transformed vertices around the screen (x, y, z, rhw)."""
    out = []
    for i in range(n):
        if r is None:
            angle = 2 * math.pi * i / max(n, 1)
            out += [fb(100 + 50 * math.cos(angle)), fb(100 + 50 * math.sin(angle)), fb(0.5), fb(1.0)]
        else:
            out += [rfl(r, -200, 200), rfl(r, -200, 200), rfl(r, 0, 1), rfl(r, 0.01, 2, 0.01)]
    return out


def entry(flags=ON_PAGE, page=1, us=0.5, vs=0.25, uo=0.125, vo=0.0625, f38=0, comp=-1):
    return (flags, page, fb(us), fb(vs), fb(uo), fb(vo), f38, comp)


def blank_entry():
    return entry(0, 0, 0.0, 0.0, 0.0, 0.0)


def mk(**kw):
    """A small, valid case: four vertices, two faces, an entry on a page; keywords replace parts."""
    n = kw.pop("n", 4)
    case = {
        "stride": 0x20, "faceset": 0, "flags": 0,
        "node": [0, 2, n, -1],
        "sets": [(0, 0, 2, 0, n - 1, 0)],
        "faces": [(0, 0, 1, 2), (0, 0, 2, 3)],
        "f94": list(range(n)),
        "points": [fb(v) for i in range(n) for v in (0.25 * i, 0.5, -0.75)],
        "uvs": [fb(0.1 * i + 0.01 * j) for i in range((n >> 2) + 2) for j in range(8)],
        "records": [(0, 0, 0)],
        "textures": [[0, 3, -1, -1]],
        "entries": [entry(), blank_entry(), blank_entry(), entry(ON_PAGE, 2), blank_entry(), blank_entry()],
        "source": quad_source(n),
        "colours": [0x80402010 + i for i in range(n)],
        "g": [0, MASK, fb(0.1)],
        "view": [fb(v) for v in (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)],
        "draw": [0x20, 0, 0x4000, 0x1000],
        "prims": [[0, 0x1256, 1, -1, -1, 4, 0x1c4, 0, 0, 0, 0, fb(0.0), -1],
                  [0x10, 0x1256, 2, -1, -1, 4, 0x1c4, 0, 0, 0x2000, 0x400, fb(5.0), -1]],
        "vertices": [POISON] * 0x1000,
        "indices": [0xa5a5] * 0x400,
        "codes": [], "dots": [], "abort": 0, "salt": 0x1234,
        "face": [fb(9.0)] * 9,
        "q": {"Project": [fb(1.5)], "Average": [0x11223344]},
    }
    for k, v in kw.items():
        if k not in case:
            raise KeyError(k)
        case[k] = v
    return case


def with_prim(case, i, **fields):
    names = ["flags", "mode", "page", "page1", "page2", "type", "format", "vcount", "icount", "vdata", "idata", "depth", "f34"]
    p = list(case["prims"][i])
    for k, v in fields.items():
        p[names.index(k)] = v
    case["prims"][i] = p
    return case


def edge_cases():
    # the room tests: the current primitive's memory against both counts and the node's
    for current in (0, 1):
        base = mk()
        vdata = base["prims"][current][9]
        need_v = vdata + 0x20 * (base["prims"][0][7] + 4 + base["prims"][current][7])
        need_i = base["prims"][current][10] + 2 * (base["prims"][0][8] + 3 * 2 + base["prims"][current][8])
        for dv, di in ((0, 0x1000), (-1, 0x1000), (1, 0x1000), (0x4000, 0), (0x4000, -1), (0x4000, 1)):
            vend = need_v + dv if abs(dv) <= 1 else dv
            iend = need_i + di if abs(di) <= 1 else di
            yield mk(draw=[0x20, current, vend, iend])
    yield with_prim(mk(draw=[0x28, 1, 0x4000, 0x1000]), 1, vcount=7, icount=9)
    yield with_prim(with_prim(mk(), 0, vcount=3, icount=4), 1, vcount=2, icount=5)
    # the texture: none, no object, no entry, not on a page (white or the average colour)
    yield mk(sets=[(-1, 0, 2, 0, 3, 0)])
    yield mk(records=[(0, 0, -1)])
    yield mk(textures=[[-1, -1, -1, -1]])
    yield mk(entries=[entry(0, 1)] + [blank_entry()] * 5)
    yield mk(entries=[entry(ON_PAGE, 0)] + [blank_entry()] * 5)
    yield mk(entries=[entry(COMPRESSED, 1, comp=2)] + [blank_entry()] * 5, q={"Average": [0x7f3f1f0f], "Project": [0]})
    yield mk(entries=[entry(COMPRESSED | ON_PAGE, 1, comp=2)] + [blank_entry()] * 5)
    # frames: with settings flag 0x40 the record's frame
    for settings, frame in ((0x40, 1), (0x40, 0), (0, 1), (0x40, 3)):
        yield mk(g=[settings, MASK, fb(0.1)], records=[(0, frame, 0)], textures=[[0, 3, -1, 4]])
    # every count 0..17 and every starting lane (the unrolled loops)
    for n in range(0, 18):
        for lane in (0, 1, 3):
            first = lane + 4 * (1 + n % 2)   # at least 4: a count of 0 needs last = first - 1
            nverts = first + max(n, 1) + 1
            c = mk(n=nverts, sets=[(0, 0, 0, first, first + n - 1, 0)], faces=[])
            yield c
            if n % 3 == 0:
                yield mk(n=nverts, sets=[(-1, 0, 0, first, first + n - 1, 0)], faces=[])
                yield mk(n=nverts, sets=[(0, 0, 0, first, first + n - 1, 1)], faces=[], stride=0x28)
    # the second set of coordinates (the node's texture, then an entry's stages) at counts 0, 1, 7, 8, 9, 16,
    # on a page and off it
    for n in (0, 1, 7, 8, 9, 16):
        nverts, first = n + 6, 5
        for flags in (ON_PAGE, 0):
            ents = [entry(), blank_entry(), blank_entry(), entry(flags, 3, 0.75, 0.5, 0.25, 0.125)] + [blank_entry()] * 2
            yield mk(n=nverts, node=[0x02000000, 0, nverts, 1], records=[(0, 0, 0), (0, 0, 1)], entries=ents,
                     sets=[(0, 0, 0, first, first + n - 1, 0)], faces=[], stride=0x28,
                     textures=[[0, -1, -1, -1], [3, -1, -1, -1]])
            ents = [entry(flags | STAGES, 1), entry(ON_PAGE, 4), blank_entry()] + [blank_entry()] * 3
            yield mk(n=nverts, entries=ents, sets=[(0, 0, 0, first, first + n - 1, 0)], faces=[], stride=0x28)
    # colours: face set flag 1, no data, a mask
    yield mk(sets=[(0, 0, 2, 0, 3, 1)])
    yield mk(colours=None)
    yield mk(g=[0, 0x00ffff00, fb(0.1)])
    # the node's second texture: none, no object, no entry, off its page, on it
    second = [(0, 0, 0), (0, 0, 1)]
    for record, texture, flags in ((-1, 1, ON_PAGE), (1, -1, ON_PAGE), (1, 1, 0), (1, 1, ON_PAGE), (1, 2, ON_PAGE)):
        records = [(0, 0, 0), (0, 0, texture)]
        ents = [entry(), blank_entry(), blank_entry(), entry(flags, 3, 0.75, 0.5, 0.25, 0.125)] + [blank_entry()] * 2
        yield mk(node=[0x02000000, 2, 4, record], records=records, entries=ents, stride=0x28,
                 textures=[[0, -1, -1, -1], [3, -1, -1, -1], [-1, -1, -1, -1]])
    # stages: flag 0x400 with or without mF38, the next entries on their pages or not
    for f38 in (0, 1):
        for next_flags, last_flags in ((ON_PAGE, 0), (0, ON_PAGE), (ON_PAGE | NEXT_PLANE, ON_PAGE),
                                       (NEXT_PLANE, ON_PAGE), (ON_PAGE | NEXT_PLANE, 0), (NEXT_PLANE, 0)):
            ents = [entry(ON_PAGE | STAGES | NEXT_PLANE, 1, f38=f38), entry(next_flags, 4), entry(last_flags, 5)]
            yield mk(entries=ents + [blank_entry()] * 3, stride=0x28)
    yield mk(entries=[entry(STAGES, 1), entry(ON_PAGE, 4), blank_entry()] + [blank_entry()] * 3, stride=0x28)
    yield mk(entries=[entry(STAGES | ON_PAGE, 0), entry(ON_PAGE, 4), blank_entry()] + [blank_entry()] * 3, stride=0x28)
    # the environment map: every call of a short face set ends the case
    for n in (1, 2, 3):
        for abort in range(1, 2 * n + 1):
            dots = [fb(0.25 * k - 0.6) for k in range(2 * n)]
            yield mk(n=n + 1, sets=[(0, 0, 0, 1, n, 0)], faces=[], entries=[entry(ON_PAGE | ENVIRONMENT)] + [blank_entry()] * 5,
                     dots=dots, abort=abort,
                     view=[fb(v) for v in (0.5, -0.25, 0.75, 0, 0.125, 1, -0.5, 0, 0, 0, 1, 0, 3, 4, 5, 1)])
    for abort in (1, 2, 4):
        ents = [entry(), blank_entry(), blank_entry(), entry(ON_PAGE | ENVIRONMENT, 3, 0.75, 0.5, 0.25, 0.125)]
        yield mk(node=[0x02000000, 2, 4, 1], records=[(0, 0, 0), (0, 0, 1)], entries=ents + [blank_entry()] * 2,
                 textures=[[0, -1, -1, -1], [3, -1, -1, -1]], stride=0x28, dots=[fb(0.5), fb(-0.5), fb(2.0), fb(1e-3)],
                 abort=abort)
    # the environment flag without a page: the plain path
    yield mk(entries=[entry(ENVIRONMENT, 1)] + [blank_entry()] * 5)
    yield mk(entries=[entry(ENVIRONMENT | ON_PAGE, 0)] + [blank_entry()] * 5)
    # faces without clipping: both orders, a face set starting further on, vertices already there
    yield mk(faces=[(0, 0, 2, 1), (0, 0, 3, 2)])
    yield with_prim(mk(n=8, sets=[(0, 1, 2, 4, 7, 0)], faces=[(0, 0, 1, 2), (0, 4, 5, 6), (0, 4, 6, 7)]), 0,
                    vcount=5, icount=6, vdata=0x40, idata=0x10)
    yield with_prim(mk(n=8, sets=[(0, 0, 3, 4, 7, 0)], faces=[(0, 2, 5, 6), (0, 4, 9, 6), (0, 7, 6, 3)]), 0, vcount=3)
    yield mk(sets=[(0, 0, 0, 0, 3, 0)])
    # clipping: everything outside one plane, faces outside, crossing, ClipPoly's answers
    yield mk(flags=0x3f, codes=[1, 3, 5, 7])
    yield mk(flags=0x3e, codes=[1, 3, 5, 7])
    yield mk(flags=0x01, codes=[1, 1, 1, 0], q={"Project": [0], "Clip.tri": [1]})
    yield mk(flags=0x21, codes=[0x20, 0x21, 0x20, 0], q={"Project": [0], "Clip.tri": [1]})
    for tri in (-1, 0, 1, 2):
        yield mk(flags=0x3f, codes=[0, 4, 0, 2], q={"Project": [0], "Clip.tri": [tri], "Clip.add": [3]})
    yield mk(flags=0x3f, codes=[0, 0, 0, 0], faces=[(0, 0, 2, 1), (0, 0, 3, 2)])
    yield mk(flags=0x3f, codes=[0, 4, 0, 2], faces=[(0, 0, 2, 1), (0, 0, 1, 2)], q={"Project": [0], "Clip.tri": [-1]})
    yield mk(flags=0x3f, codes=[2, 2, 2, 0], q={"TestData": [0], "Project": [0]})
    yield mk(flags=0x3f, codes=[0, 0, 0, 0], q={"TestData": [4], "Project": [0]})
    # clipping's room: the vertex limit (ten vertices before the end, moved by ClipPoly) and the index limit
    # (the case's own primitive is the current one, so the room test at the start passes)
    many = [(0, 0, 1, 2), (0, 0, 2, 3), (0, 1, 2, 3), (0, 0, 1, 3), (0, 0, 3, 2), (0, 3, 2, 1)] * 2
    for vend in range(0x1b8, 0x230, 8):
        for add in (1, 2):
            yield mk(flags=0x3f, faces=many, sets=[(0, 0, 12, 0, 3, 0)], codes=[0, 1, 0, 0], node=[0, 0, 4, -1],
                     draw=[0x20, 0, vend, 0x1000], q={"Project": [0], "Clip.add": [add]})
    for iend in range(0x30, 0x6c, 2):
        yield mk(flags=0x3f, faces=many, sets=[(0, 0, 12, 0, 3, 0)], codes=[0, 0, 0, 0], node=[0, 0, 4, -1],
                 draw=[0x20, 0, 0x4000, iend], q={"Project": [0]})
    yield mk(flags=0x3f, faces=many, sets=[(0, 0, 12, 0, 3, 0)], codes=[0, 2, 0, 0], node=[0, 0, 4, -1],
             draw=[0x20, 0, 0x4000, 0x48], q={"Project": [0], "Clip.tri": [1, 2, 0]})
    # near-flat triangles whose facing answer depends on the right-hand inner multiply-subtract being fused
    #, unclipped and clipped
    for v in FACE_TIES:
        src = [v[0], v[1], fb(0.5), v[2], v[3], v[4], fb(0.5), v[5], v[6], v[7], fb(0.5), v[8]]
        for flags in (0, 0x3f):
            yield mk(n=3, sets=[(0, 0, 1, 0, 2, 0)], faces=[(0, 0, 1, 2)], source=src, node=[0, 1, 3, -1], flags=flags,
                     codes=[0, 0, 0], q={"Project": [0]})
    # the tail: projection type, the depth from the count, the counts read again, the depth read after
    for flags in (0, 0x02000000):
        for mode in (0x1256, 0x801256):
            yield with_prim(mk(flags=flags, q={"Project": [fb(2.5)]}), 0, mode=mode, depth=fb(1.25))
    yield with_prim(mk(q={"Project": [fb(2.5)], "Fog.add": [3], "Project.add": [5]}), 0, mode=0x800000, vcount=2)
    yield mk(q={"Project": [fb(2.5)], "Project.poke": [fb(100.0)]})
    yield with_prim(mk(q={"Project": [fb(2.5)], "Project.poke": [fb(100.0)]}), 0, mode=0x800000)
    yield mk(g=[0, MASK, fb(0.1234567)], q={"Project": [0]})
    # depths from counts where the fused multiply-add rounds differently from a multiply and an add
    for z54, n in ((0x3ef37cb7, 9), (0x3f237587, 9), (0x3e2bd552, 10), (0x3f378c8a, 7), (0x3ee233a4, 11), (0x3f5b4ef1, 7)):
        yield with_prim(mk(n=n, sets=[(0, 0, 0, 0, n - 1, 0)], faces=[], g=[0, MASK, z54]), 0, mode=0x801256)
    # the round-off flag, with and without an entry
    yield mk(entries=[entry(ON_PAGE | ROUND_OFF)] + [blank_entry()] * 5)
    yield mk(entries=[entry(ROUND_OFF)] + [blank_entry()] * 5)
    yield mk(sets=[(-1, 0, 2, 0, 3, 0)], prims=[[0, 0, 1, -1, -1, 4, 0x1c4, 0, 0, 0, 0, fb(0.0), -1]] * 2)
    # special values in the coordinates
    for value in (math.nan, math.inf, -0.0, 1e30, 1e-40):
        yield mk(entries=[entry(ON_PAGE, 1, value, 0.5, 0.25, value)] + [blank_entry()] * 5,
                 uvs=[fb(value)] * 16)


def random_case(r):
    stride = r.choice([0x20, 0x28, 0x28, 0x20, 0x30, 0x24, 0x2c, 0x40])
    nsrc = r.randint(1, 24)
    nverts = r.randint(1, 40)
    f94 = [r.randrange(nsrc) for _ in range(nverts)]
    points = [rfl(r, -1, 1) for _ in range(3 * nsrc)]
    uvs = [rfl(r, -2, 2) for _ in range(8 * ((nverts >> 2) + 2))]
    flags = r.choice([0, 0, 0, r.getrandbits(32) & 0x3f, r.getrandbits(32)])
    flags |= r.choice([0, 0, 0x02000000])
    clip = flags & 0x3f != 0
    # the face sets: the chosen one's faces come after a few others
    nsets = r.randint(1, 4)
    faceset = r.randrange(nsets)
    first = r.randrange(nverts)
    last = first - 1 if first > 0 and r.random() < 0.05 else r.randint(first, nverts - 1)
    count = last - first + 1
    nfaces = r.choice([0, 1, 2, 3, 5, 8, 12, 20])
    if clip and count <= 0:
        nfaces = 0     # every face would read outside codes nobody wrote (stack garbage in the original)
    lead = r.randint(0, 3)
    faces = [(r.getrandbits(16), r.randrange(nverts), r.randrange(nverts), r.randrange(nverts)) for _ in range(lead)]
    vcount0 = r.randint(0, 6)
    for _ in range(nfaces):
        if count <= 0:
            faces.append((r.getrandbits(16), first, first, first))
        elif clip:
            faces.append((r.getrandbits(16), *[r.randint(first, last) for _ in range(3)]))
        else:
            lo = max(0, first - vcount0, first - 2)
            faces.append((r.getrandbits(16), *[r.randint(lo, last + 2) for _ in range(3)]))
    nrecords = r.randint(1, 3)
    sets = []
    for i in range(nsets):
        record = r.choice([-1] + list(range(nrecords)) * 3)
        if i == faceset:
            sets.append((record, lead, nfaces, first, last, r.choice([0, 0, 0, 1, 2, 3, 0x81])))
        else:
            sets.append((record, r.getrandbits(16), r.getrandbits(16), r.getrandbits(16), r.getrandbits(16), r.getrandbits(8)))
    records = [(r.getrandbits(16), r.randrange(FRAMES), r.choice([-1, 0, 1, 2, 2])) for _ in range(nrecords)]
    # entries in threes, the first of each three the one the frames name
    entries = []
    for t in range(3):
        for plane in range(3):
            bits = r.choice([0, ON_PAGE, ON_PAGE, ON_PAGE])
            if plane == 0:
                bits |= r.choice([0, 0, STAGES, STAGES | NEXT_PLANE]) | r.choice([0, 0, 0, ENVIRONMENT])
                bits |= r.choice([0, ROUND_OFF]) | r.choice([0, 0, COMPRESSED])
            else:
                bits |= r.choice([0, NEXT_PLANE])
            bits |= r.getrandbits(32) & r.choice([0, 0, 0x00f00ff8])
            entries.append((bits, r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 1, 2]), rfl(r, 0, 1), rfl(r, 0, 1), rfl(r, 0, 1),
                            rfl(r, 0, 1), r.choice([0, 0, 1]), r.choice([-1, 0, 1, 2, 3])))
    for k in range(len(entries)):
        if entries[k][0] & COMPRESSED and entries[k][7] < 0:
            entries[k] = entries[k][:7] + (k % COMPS,)
    textures = [[r.choice([-1, 0, 3, 6, 0, 3, 6]) for _ in range(FRAMES)] for _ in range(3)]
    f98 = r.choice([-1] + list(range(nrecords)))
    node_flags = r.getrandbits(32) & r.choice([0, 0xfdffffff]) | r.choice([0, 0, 0x02000000])
    case = mk(n=nverts)
    case.update({
        "stride": stride, "faceset": faceset, "flags": flags,
        "node": [node_flags, r.randint(0, 12), r.randint(0, 40), f98],
        "sets": sets, "faces": faces, "f94": f94, "points": points, "uvs": uvs, "records": records,
        "textures": textures, "entries": entries, "source": quad_source(nsrc, r),
        "colours": None if r.random() < 0.15 else [r.getrandbits(32) for _ in range(nverts)],
        "g": [r.choice([0, 0x40, r.getrandbits(32)]), r.choice([MASK, MASK, r.getrandbits(32)]), rfl(r, 0, 1)],
        "view": [rfl(r, -1, 1) for _ in range(16)],
        "salt": r.getrandbits(16), "face": [rfl(r) for _ in range(9)],
    })
    # the environment map: where the coordinates would come from the normals, the case ends inside
    settings = case["g"][0]

    def entry_of(record_number, frame_from_record):
        if record_number < 0:
            return None
        f00, frame, texture = records[record_number]
        if texture < 0:
            return None
        k = textures[texture][frame if frame_from_record and settings & 0x40 else 0]
        return None if k < 0 else entries[k]

    def environment(e):
        return e is not None and e[0] & ENVIRONMENT and e[0] & ON_PAGE and e[1] != 0

    first_entry = entry_of(sets[faceset][0], True)
    second = node_flags & 0x02000000 and environment(entry_of(f98, False))
    if environment(first_entry) or second:
        if count <= 0:
            case["entries"] = [(e[0] & ~ENVIRONMENT,) + tuple(e[1:]) for e in entries]
        else:
            case["abort"] = r.randint(1, 2 * count)
            case["dots"] = [rfl(r, -1.5, 1.5) for _ in range(2 * count)]
    # the primitives and the memory
    vdata = r.choice([0, 0x40, 0x100, 0x20 * r.randint(0, 8)])
    idata = r.choice([0, 0x10, 0x40, 2 * r.randint(0, 30)])
    icount0 = r.randint(0, 10)
    extra = r.choice([0, 0, 1, 2])
    other_v, other_i = 0x4000, 0x1000
    vwords = (max(vdata + stride * (vcount0 + max(count, 0) + 4) + 0x40, other_v + 0x100)) // 4
    iwords = max(idata // 2 + icount0 + 3 * nfaces * (1 + extra) + 32, other_i // 2 + 0x40)
    mode = r.choice([0x1256, 0x801256, r.getrandbits(32)])
    case["prims"] = [[r.getrandbits(32) & 0xffff, mode, r.randint(-1, 8), r.randint(-1, 8), r.randint(-1, 8), 4, 0x1c4,
                      vcount0, icount0, vdata, idata, rfl(r, 0, 10), -1],
                     [0x10, 0x1256, 2, -1, -1, 4, 0x1c4, r.randint(0, 20), r.randint(0, 60), other_v, other_i, fb(5.0), -1]]
    current = r.choice([0, 0, 1])
    vsize = r.choice([0x20, 0x28, stride])
    cur = case["prims"][current]
    need_v = cur[9] + vsize * (vcount0 + case["node"][2] + cur[7])
    need_i = cur[10] + 2 * (icount0 + 3 * case["node"][1] + cur[8])
    vend = need_v + r.choice([0, 0x100, 0x1000, 0x10000, -1, 1, 0x400])
    iend = need_i + r.choice([0, 0x40, 0x400, 0x4000, -1, 1, 0x100])
    if clip and r.random() < 0.4:
        # a tight end for the clipping loop's limits, with a room test at the start that passes
        current, vsize = 0, stride
        case["node"][1:3] = [r.randint(0, 2), r.randint(0, max(count, 0))]
        cur = case["prims"][0]
        need_v = cur[9] + vsize * (2 * vcount0 + case["node"][2])
        need_i = cur[10] + 2 * (2 * icount0 + 3 * case["node"][1])
        vend = max(need_v, vdata + stride * (vcount0 + max(count, 0) + r.randint(-1, 14)) + stride * 10)
        iend = max(need_i, idata + 2 * (icount0 + r.randint(0, 3 * nfaces + 3)) + 0x30)
    case["draw"] = [vsize, current, vend, iend]
    case["vertices"] = [r.getrandbits(32) if r.random() < 0.5 else POISON for _ in range(vwords)]
    case["indices"] = [r.getrandbits(16) for _ in range(iwords)]
    case["codes"] = [r.choice([0, 0, 0, 1, 2, 4, 8, 0x10, 0x20, 0x3f, r.getrandbits(8)]) for _ in range(max(count, 0))]
    q = {"Project": [rfl(r, -10, 10)], "Average": [r.getrandbits(32)],
         "Clip.tri": [r.choice([-1, 0, 0, 1, 2, extra]) for _ in range(8)], "Clip.add": [r.randint(0, 4) for _ in range(8)]}
    if r.random() < 0.1:
        q["TestData"] = [r.choice([0, 1, 0x20])]
    if r.random() < 0.2:
        q["Fog.add"] = [r.randint(-2, 3)]
    if r.random() < 0.2:
        q["Project.add"] = [r.randint(-2, 3)]
    if r.random() < 0.2:
        q["Project.poke"] = [rfl(r, 0, 100)]
    case["q"] = q
    return case


TESTS = [
    FnTest("SRenderListEntry::CalcRenderEntry", ppc, host, edge=lambda: list(edge_cases()), rnd=random_case),
]
