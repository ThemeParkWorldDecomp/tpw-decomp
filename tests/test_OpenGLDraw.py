"""The OpenGL draw primitive's drawing members (engine library, tb_opengl_draw.cpp):
Execute, DrawIndexedPrimitive, DrawFilledBox, DrawPoly, DrawEdgeBox, DrawTestPoly,
DrawDetectPoly, GetPixel, GetPixelFillRate, DrawWiredframedBoundingBox and DrawProfileBox.

A case is a script of operations run on both sides on one draw primitive (the base class's
words, the display's scale, the DirectDraw result and a block of vertices and indices set
by the case), with three texture pages. After every operation the test compares the result,
the recorded calls and the state of everything the code could change.

Everything outside the unit is a fake on both sides that records its arguments: every
OpenGL call (floats by their bits, a NaN as one value), the draw primitive's SetBlendingMode,
SetTexture, BeginDraw and EndDraw (virtual calls through the original's own vtable), the
math unit's Project (which also flips a bit in each position it is given), the error stack,
the profiler's clock and UDebug. The vertex arrays are compared by contents: glLockArraysEXT
dumps the vertices it locks and glDrawElements the indices it draws, and the array pointers
are recorded as offsets from the vertex pointer.

The original's fused multiply-adds and subtracts run under HardwareRounding, as in
test_NodeGamuts.py: Unicorn rounds them twice.
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

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_OpenGLDraw"

FN = dict(
    Execute=0x100431ac, DrawIndexedPrimitive=0x1004390c, DrawFilledBox=0x10043eb0, DrawPoly=0x100441f4,
    DrawEdgeBox=0x10044420, DrawTestPoly=0x10043b70, DrawDetectPoly=0x10043d08, GetPixel=0x10044b38,
    GetPixelFillRate=0x10044c08, DrawWiredframedBoundingBox=0x10044724, DrawProfileBox=0x10044fa4)
# Callees outside the unit: the draw primitive's setters and BeginDraw / EndDraw (by their
# vtable slots), the error stack, the profiler's clock, UDebug and the math unit's Project.
SET_BLENDING, SET_TEXTURE, BEGIN_DRAW, END_DRAW = 0x100433c0, 0x100432ec, 0x100430b8, 0x10043140
ERROR_PUSH, CLOCK, UDEBUG, PROJECT = 0x10008904, 0x1001fc8c, 0x10043b20, 0x10042450
# The texture page's constructor and CTextureBase's, and GetTexture (behind its thunk).
INNER = [0x10045d88, 0x10032b58, 0x10046adc]
MATH_UNIT_CTOR = "__ct__14CMatrixPowerPCFv"

G_DDRESULT, DISPLAY, BLEND, MATH_UNIT = 0x10052b40, 0x10165634, 0x100520fc, 0x100afe94
DRAW_VTABLE, DRAW_BASE_VTABLE = 0x100522a0, 0x100522f0
MINUS_ONE = 0xFFFFFFFF
MASK = 0xFFFFFFFF
VERTEX_CAPACITY, INDEX_CAPACITY, TEXTURES = 64, 128, 3

# The operations (the numbers of tests/unit_OpenGLDraw.cpp).
EXECUTE, DRAW_INDEXED, FILLED_BOX, POLY, EDGE_BOX, TEST_POLY, DETECT_POLY, GET_PIXEL, FILL_RATE, WIRE, PROFILE_BOX = range(1, 12)
SET_FIELD, SET_RESULT, SET_DISPLAY = 20, 21, 22
NAMES = {EXECUTE: "Execute", DRAW_INDEXED: "DrawIndexedPrimitive", FILLED_BOX: "DrawFilledBox", POLY: "DrawPoly",
         EDGE_BOX: "DrawEdgeBox", TEST_POLY: "DrawTestPoly", DETECT_POLY: "DrawDetectPoly", GET_PIXEL: "GetPixel",
         FILL_RATE: "GetPixelFillRate", WIRE: "DrawWiredframedBoundingBox", PROFILE_BOX: "DrawProfileBox"}


def setup(runner):
    """The standard stubs, and a real math unit for gei_MathUnit to point at (its Project is
    the fake of each case)."""
    e = runner.emu
    T.install_std_stubs(e)
    obj = T.persist_alloc(e, 0x14)
    T.call_ppc(e, e.func(MATH_UNIT_CTOR), [obj, 1])
    e.od_math_unit = e.u32(obj)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def cbits(x):
    """A single-precision value as its bits, every NaN the same."""
    if x != x:
        return 0x7FC00000
    return CL.fbits(x)


def canon(word):
    return 0x7FC00000 if (word & 0x7F800000) == 0x7F800000 and (word & 0x007FFFFF) else word


# ----------------------------------------------------------------- the emulator's fused multiply-adds

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]
CODE_RANGES = [(FN["DrawFilledBox"], FN["DrawFilledBox"] + 756), (FN["DrawDetectPoly"], FN["DrawDetectPoly"] + 356)]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Unicorn (QEMU 5.0) computes fmadds, fmsubs, fnmadds and fnmsubs in double precision and
    rounds the result to single, which can differ by one unit in the last place from the
    processor's single rounding of the exact result. A code hook over the functions that use
    them looks at each instruction before it runs and rewrites its target register just before
    the next one, with the C library's fmaf (one rounding)."""

    changed = 0    # results the correction replaced (all cases so far), for the notes

    def __init__(self, e):
        self.e, self.pending, self.hooks = e, None, []

    def __enter__(self):
        for begin, end in CODE_RANGES:
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=end - 1))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)

    def step(self, uc, address, size, user):
        e = self.e
        if self.pending is not None:
            target, value = self.pending
            self.pending = None
            corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
            if fpr(e, target) != corrected and not (value != value and fpr_double(e, target) != fpr_double(e, target)):
                HardwareRounding.changed += 1
            uc.reg_write(ppcemu.FPR[target], corrected)
        word = struct.unpack(">I", e.read(address, 4))[0]
        if word >> 26 != 59 or ((word >> 1) & 31) not in (28, 29, 30, 31):
            return
        kind = (word >> 1) & 31
        rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
        a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        for x in (a, c, b):
            assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
        subtract = kind in (28, 30)
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        if kind in (30, 31):
            value = -value
        self.pending = (rd, value)


# ----------------------------------------------------------------- PowerPC side


class PpcSide:
    def __init__(self, e, case):
        self.e = e
        self.L = CL.PpcLog({}, case.get("q"))
        e.cl = self.L
        self.base = None      # the vertex pointer of the draw call in progress
        self.vertex_block = self.index_block = self.pixel = self.draw = 0
        self.pages = []

    def build(self, case):
        e, L = self.e, self.L
        self.draw = e.alloc(0x4C, b"\xa5" * 0x4C)
        e.write(self.draw, struct.pack(">IIIIII", self.draw + 0x18, DRAW_VTABLE, MINUS_ONE, 0, 0, 0))
        w32(e, self.draw + 0x18 + 0x2C, DRAW_BASE_VTABLE)
        w32(e, self.draw + 0x18 + 0x30, 0)
        self.vertex_block = e.alloc(VERTEX_CAPACITY * 0x20, struct.pack(">%dI" % (VERTEX_CAPACITY * 8), *case["vwords"]))
        self.index_block = e.alloc(INDEX_CAPACITY * 2, struct.pack(">%dH" % INDEX_CAPACITY, *case["iwords"]))
        self.pixel = e.alloc(8, b"\xa5" * 8, alignment=4)
        L.labels.update({self.draw: 1, self.vertex_block: 7, self.index_block: 8, self.pixel: 10})
        for k in range(TEXTURES):
            page = e.alloc(0x34, b"\xa5" * 0x34)
            T.call_ppc(e, 0x10045D88, [page, 1])
            self.pages.append(page)
            L.labels[page] = 21 + k

    def reset_globals(self):
        e = self.e
        w32(e, G_DDRESULT, 0)
        for offset in (0x08, 0x0C, 0x18, 0x1C):
            w32(e, DISPLAY + offset, 0)
        e.write(BLEND, struct.pack(">12I", *(0x7000 + i for i in range(12))))
        w32(e, MATH_UNIT, e.od_math_unit)

    # -- the fakes
    def dump_vertices(self, name, first, count):
        if self.base is None or count <= 0 or count > 256:
            return
        for i in range(first, first + count):
            w = struct.unpack(">8I", self.e.read(self.base + 0x20 * i, 0x20))
            self.L.rec(name, (i, canon(w[0]), canon(w[1]), canon(w[2]), canon(w[3]), w[4], w[5], canon(w[6]), canon(w[7])))

    def dump_indices(self, address, count):
        if address == 0 or count <= 0 or count > 1024:
            return
        values = struct.unpack(">%dH" % count, self.e.read(address, 2 * count))
        for i in range(0, count, 8):
            row = [values[i + k] if i + k < count else -1 for k in range(8)]
            self.L.rec("Index", [i] + row)

    def offset(self, pointer):
        base = self.base if self.base is not None else self.vertex_block
        return T.s32(pointer - base)

    def stubs(self):
        e, L = self.e, self.L
        st = CL.Stubs()
        st.real = set(FN.values()) | set(INNER)
        st.internal(ERROR_PUSH, CL.recorder("Error", "xii"))
        st.internal(CLOCK, CL.recorder("Clock", "", "i"))
        st.internal(SET_BLENDING, CL.recorder("SetBlendingMode", "Li", "i"))
        st.internal(SET_TEXTURE, CL.recorder("SetTexture", "LiL", "i"))
        st.internal(BEGIN_DRAW, CL.recorder("BeginDraw", "L", "i"))
        st.internal(END_DRAW, CL.recorder("EndDraw", "L", "i"))

        def udebug(e):
            L.rec("UDebug", (T.s32(e.r(4)),), (e.cstr(e.r(3)),))

        def project(e):
            vertices, stride, count, kind = e.r(4), T.s32(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7))
            L.rec("Project", (stride, count, kind))
            saved, self.base = self.base, vertices
            self.dump_vertices("ProjectVertex", 0, count)
            self.base = saved
            for i in range(count):
                words = struct.unpack(">4I", e.read(vertices + stride * i, 16))
                e.write(vertices + stride * i, struct.pack(">4I", *[w ^ 0x00200000 for w in words]))
            e.set_f(1, 0.0)

        st.internal(UDEBUG, udebug)
        st.internal(PROJECT, project)

        def gl(name, ints=()):
            L.rec(name, ints)

        def vertex_pointer(e):
            self.base = e.r(6)
            gl("glVertexPointer", (T.s32(e.r(3)), e.r(4), T.s32(e.r(5)), L.lab(e.r(6))))

        def array_pointer(name):
            return lambda e: gl(name, (T.s32(e.r(3)), e.r(4), T.s32(e.r(5)), self.offset(e.r(6))))

        def lock(e):
            gl("glLockArraysEXT", (T.s32(e.r(3)), T.s32(e.r(4))))
            self.dump_vertices("Vertex", T.s32(e.r(3)), T.s32(e.r(4)))

        def unlock(e):
            gl("glUnlockArraysEXT")
            result = L.ret("DDResult", -1)
            if result >= 0:
                w32(e, G_DDRESULT, result)

        def draw_elements(e):
            mode = -1 if e.r(3) == self.draw else e.r(3)
            gl("glDrawElements", (mode, T.s32(e.r(4)), e.r(5), L.lab(e.r(6))))
            self.dump_indices(e.r(6), T.s32(e.r(4)))

        def get_error(e):
            gl("glGetError")
            e.set_r(3, T.u32(L.ret("glGetError", 0)))

        def read_pixels(e):
            gl("glReadPixels", tuple(T.s32(e.r(i)) for i in range(3, 7)) + (e.r(7), e.r(8), L.lab(e.r(9))))
            value = L.ret("Pixel", 0) & MASK
            e.write(e.r(9), bytes([value >> 24, (value >> 16) & 255, (value >> 8) & 255, value & 255]))

        st.imp("glVertexPointer", vertex_pointer)
        st.imp("glTexCoordPointer", array_pointer("glTexCoordPointer"))
        st.imp("glColorPointer", array_pointer("glColorPointer"))
        st.imp("glLockArraysEXT", lock)
        st.imp("glUnlockArraysEXT", unlock)
        st.imp("glDrawElements", draw_elements)
        st.imp("glFinish", lambda e: gl("glFinish"))
        st.imp("glPushAttrib", lambda e: gl("glPushAttrib", (e.r(3),)))
        st.imp("glPopAttrib", lambda e: gl("glPopAttrib"))
        st.imp("glEnable", lambda e: gl("glEnable", (e.r(3),)))
        st.imp("glDisable", lambda e: gl("glDisable", (e.r(3),)))
        st.imp("glDepthMask", lambda e: gl("glDepthMask", (e.r(3) & 0xFF,)))
        st.imp("glBlendFunc", lambda e: gl("glBlendFunc", (e.r(3), e.r(4))))
        st.imp("glAlphaFunc", lambda e: gl("glAlphaFunc", (e.r(3), cbits(e.f(1)))))
        st.imp("glFlush", lambda e: gl("glFlush"))
        st.imp("glGetError", get_error)
        st.imp("glDrawBuffer", lambda e: gl("glDrawBuffer", (e.r(3),)))
        st.imp("glBegin", lambda e: gl("glBegin", (e.r(3),)))
        st.imp("glEnd", lambda e: gl("glEnd"))
        st.imp("glColor4ubv", lambda e: gl("glColor4ubv", tuple(e.read(e.r(3), 4))))
        st.imp("glTexCoord2f", lambda e: gl("glTexCoord2f", (cbits(e.f(1)), cbits(e.f(2)))))
        st.imp("glVertex4f", lambda e: gl("glVertex4f", tuple(cbits(e.f(i)) for i in range(1, 5))))
        st.imp("glPixelStorei", lambda e: gl("glPixelStorei", (e.r(3), T.s32(e.r(4)))))
        st.imp("glReadBuffer", lambda e: gl("glReadBuffer", (e.r(3),)))
        st.imp("glReadPixels", read_pixels)
        return st

    # -- the operations
    def call(self, name, *args):
        return T.call_ppc(self.e, FN[name], list(args))

    def r3(self, name, *args):
        return T.s32(self.call(name, *args)[0])

    def page_base(self, k):
        """The CTextureBase part of page k (0x1c into the object), or 0."""
        return self.pages[k] + 0x1C if 0 <= k < TEXTURES else 0

    def op(self, op, a):
        e = self.e
        self.base = None
        if op == EXECUTE:
            mode, texture, kind, vertex_type, vertex_count, index_count = a
            if 0 <= texture < TEXTURES:
                page = self.pages[texture]
            else:
                page = 0x1234 if texture == 9 else 0
            primitive = e.alloc(0x30, b"\xa5" * 0x30)
            for offset, value in ((0, 0x21), (4, mode), (8, page), (0x18, kind), (0x1C, vertex_type), (0x20, vertex_count),
                                  (0x24, index_count), (0x28, self.vertex_block), (0x2C, self.index_block)):
                w32(e, primitive + offset, value)
            return self.r3("Execute", self.draw, primitive)
        if op == DRAW_INDEXED:
            kind, vertex_type, vertex_count, index_count, flags = a
            e.set_r(29, self.draw)       # the caller's r29: the original draws with it as the mode of a bad type
            return self.r3("DrawIndexedPrimitive", self.draw, kind, vertex_type, self.vertex_block, vertex_count,
                           self.index_block, index_count, flags)
        if op == FILLED_BOX:
            return self.r3("DrawFilledBox", self.draw, *[T.F(T.bits_f32(v)) for v in a[:4]], a[4], self.page_base(a[5]))
        if op == POLY:
            return self.r3("DrawPoly", self.draw, self.vertex_block, a[0], a[1], T.F(T.bits_f32(a[2])))
        if op == EDGE_BOX:
            return self.r3("DrawEdgeBox", self.draw)
        if op == TEST_POLY:
            return self.r3("DrawTestPoly", self.draw)
        if op == DETECT_POLY:
            size, colour, unused, u0, v0, u1, v1 = a
            return self.r3("DrawDetectPoly", self.draw, T.F(T.bits_f32(size)), colour, unused, *[T.F(T.bits_f32(v)) for v in (u0, v0, u1, v1)])
        if op == GET_PIXEL:
            return self.r3("GetPixel", self.draw, a[0], a[1], self.pixel)
        if op == FILL_RATE:
            return cbits(self.call("GetPixelFillRate", self.draw)[2])
        if op == WIRE:
            return self.r3("DrawWiredframedBoundingBox", self.draw, self.vertex_block, a[0], a[1])
        if op == PROFILE_BOX:
            return self.r3("DrawProfileBox", self.draw)
        if op == SET_FIELD:
            w32(e, self.draw + 0x18 + 4 * (a[0] & 15), a[1])
            return 0
        if op == SET_RESULT:
            w32(e, G_DDRESULT, a[0])
            return 0
        if op == SET_DISPLAY:
            for offset, value in ((0x18, a[0]), (0x1C, a[1]), (8, a[2]), (0xC, a[3])):
                w32(e, DISPLAY + offset, value)
            return 0
        return -99

    def state(self):
        e, u = self.e, self.e.u32
        s = lambda a: T.s32(u(a))
        base = self.draw + 0x18
        out = [u(base + 4 * i) for i in range(11)]
        current = u(self.draw + 8)
        out.append(-1 if current == MINUS_ONE else (0 if current == 0 else 1))
        out += [s(self.draw + 0xC), s(self.draw + 0x10), s(self.draw + 0x14)]
        out.append(s(G_DDRESULT))
        out += list(e.read(self.pixel, 4))
        total = mix = 0
        for word in struct.unpack(">%dI" % (VERTEX_CAPACITY * 8), e.read(self.vertex_block, VERTEX_CAPACITY * 0x20)):
            total = (total + word) & MASK
            mix = (mix * 31 + word) & MASK
        out += [total, mix]
        mix = 0
        for value in struct.unpack(">%dH" % INDEX_CAPACITY, e.read(self.index_block, INDEX_CAPACITY * 2)):
            mix = (mix * 31 + value) & MASK
        out.append(mix)
        return out


def ppc(e, case):
    side = PpcSide(e, case)
    saved_words = [(a, e.u32(a)) for a in (G_DDRESULT, MATH_UNIT) + tuple(DISPLAY + o for o in (8, 0xC, 0x18, 0x1C))]
    saved_blend = e.read(BLEND, 48)
    saved = side.stubs().activate(e)
    results, states, marks = [], [], []
    try:
        side.build(case)
        side.reset_globals()
        with HardwareRounding(e):
            for op, args in case["script"]:
                results.append(side.op(op, args))
                states.append(side.state())
                marks.append(len(side.L.log))
    finally:
        CL.Stubs.deactivate(e, saved)
        for a, v in saved_words:
            w32(e, a, v)
        e.write(BLEND, saved_blend)
    return dict(results=results, log=side.L.log, marks=marks, states=states)


# ----------------------------------------------------------------- host side


def host(lib, case):
    reset = lib["tpw_od_reset"]
    reset.restype, reset.argtypes = None, []
    data = lib["tpw_od_data"]
    data.restype, data.argtypes = None, [C.c_int32, C.POINTER(C.c_uint32), C.c_int32]
    op_fn = lib["tpw_od_op"]
    op_fn.restype, op_fn.argtypes = C.c_int64, [C.c_int32, C.POINTER(C.c_int32)]
    state_fn = lib["tpw_od_state"]
    state_fn.restype, state_fn.argtypes = C.c_int32, [C.POINTER(C.c_int64), C.c_int32]
    obj = lib["tpw_od_object"]
    obj.restype, obj.argtypes = C.c_void_p, [C.c_int32]

    reset()
    data(0, (C.c_uint32 * (VERTEX_CAPACITY * 8))(*case["vwords"]), VERTEX_CAPACITY * 8)
    data(1, (C.c_uint32 * INDEX_CAPACITY)(*case["iwords"]), INDEX_CAPACITY)
    labels = {1: obj(1), 7: obj(7), 8: obj(8), 10: obj(10)}
    labels.update({21 + k: obj(21 + k) for k in range(TEXTURES)})
    CL.host_begin(lib, labels, case.get("q"))
    results, states, marks = [], [], []
    buffer = (C.c_int64 * 64)()
    try:
        for op, args in case["script"]:
            padded = (C.c_int32 * 16)(*[T.s32(v) for v in args])
            results.append(op_fn(op, padded))
            count = state_fn(buffer, 64)
            states.append(list(buffer[:count]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(results=results, log=log, marks=marks, states=states)


# ----------------------------------------------------------------- cases

fb = T.f32_bits
VERTEX_TYPE = 0x1C4
GOOD_TYPES = (4, 3)
BAD_TYPES = (0, 1, 2, 5, 6, -1, 7, 0x7FFFFFFF)


def moderate(r, lo=-100.0, hi=100.0):
    """A float32 in a moderate range, as its bits."""
    return fb(T.f32(r.uniform(lo, hi)))


SPECIAL_FLOATS = [0.0, -0.0, 1.0, -1.0, 0.5, 2.0, 1e-6, -1e-6, 1e30, -1e30, 3.4028234663852886e38, 1.1754943508222875e-38,
                  1.401298464324817e-45, math.inf, -math.inf, math.nan, 2.0 ** -24, 640.0, 480.0, 1.0 + 2.0 ** -23]


def special(r):
    """A special value (zeros, infinities, NaN, denormals, FLT_MAX, rounding boundaries) as bits."""
    return fb(r.choice(SPECIAL_FLOATS))


def any_float(r, scale=100.0):
    """Mostly a moderate float, now and then a special value or any bit pattern."""
    k = r.random()
    if k < 0.7:
        return moderate(r, -scale, scale)
    if k < 0.9:
        return special(r)
    return r.getrandbits(32)


def vertex_words(r):
    """The block of 64 vertices: floats where a vertex has them, any word for the colours."""
    words = []
    for _ in range(VERTEX_CAPACITY):
        words += [any_float(r), any_float(r), any_float(r), any_float(r), r.getrandbits(32), r.getrandbits(32), any_float(r),
                  any_float(r)]
    return words


def index_words(r):
    return [r.randrange(0, 65536) if r.random() < 0.3 else r.randrange(0, 16) for _ in range(INDEX_CAPACITY)]


def display_op(r, special_values=False):
    """2 / width and 2 / height for a screen size, now and then something odd."""
    width = r.choice([640, 800, 1024, 1280, 1600, 2048, 1, 3, r.randrange(1, 4000)])
    height = r.choice([480, 600, 768, 1024, 1200, 1536, 1, 7, r.randrange(1, 4000)])
    sx, sy = fb(T.f32(2.0 / width)), fb(T.f32(2.0 / height))
    if special_values and r.random() < 0.25:
        sx = special(r) if r.random() < 0.5 else sx
        sy = special(r) if r.random() < 0.5 else sy
    return (SET_DISPLAY, [sx, sy, width, height])


STATE_BITS = [0x100, 0x200, 0x400, 0x800, 0x8000, 0x1000000, 0x4000000, 0x80000, 0x100000, 0x56, 0xFF]


def state_word(r):
    k = r.random()
    if k < 0.2:
        return r.choice([0, 0xFFFFFFFF, 0x56, 0x156, 0x8000, 0x100, 0x80000])
    if k < 0.6:
        word = 0
        for bit in STATE_BITS:
            if r.random() < 0.5:
                word |= bit
        return word
    return r.getrandbits(32)


def prelude(r, special_values=False, fields=True):
    """The starting state: a screen, the base class's words and the DirectDraw result."""
    ops = [display_op(r, special_values)]
    if fields:
        for i in range(11):
            ops.append((SET_FIELD, [i, state_word(r) if i == 4 else r.getrandbits(32)]))
    ops.append((SET_RESULT, [r.choice([0, 0, 0, 0, 0x887601C2, -1, r.getrandbits(32)])]))
    return ops


def queues(r, fail=False):
    q = {"SetBlendingMode": [r.choice([0, 1])], "SetTexture": [r.choice([0, 1])], "BeginDraw": [r.choice([1, 1, 1, 0])],
         "EndDraw": [r.choice([1, 1, 1, 0])], "Clock": [r.getrandbits(31), r.getrandbits(31)],
         "Pixel": [r.getrandbits(32)]}
    if fail or r.random() < 0.3:
        q["glGetError"] = [r.choice([0, 0, 0x502]) for _ in range(r.randrange(1, 6))] + [0]
    if r.random() < 0.25:
        q["DDResult"] = [r.choice([-1, -1, 0, 0x887601C2, 5]) for _ in range(r.randrange(1, 5))] + [-1]
    return q


def case_of(r, ops, fail=False, special_values=False, fields=True, **extra):
    case = dict(q=queues(r, fail), vwords=vertex_words(r), iwords=index_words(r),
                script=prelude(r, special_values, fields) + list(ops))
    case.update(extra)
    return case


def dip_args(r):
    kind = r.choice(list(GOOD_TYPES) * 4 + list(BAD_TYPES))
    vertex_type = r.choice([VERTEX_TYPE] * 6 + [0, 0x1C0, 0x44, 0x1C5, 0xFFFFFFFF, 0x100])
    flags = r.choice([0, 1, 0x8000, 0x8001, 9, r.getrandbits(32), 0x7FFF, 0x10000])
    return [kind, vertex_type, r.choice([0, 1, 4, 8, r.randrange(0, 41), VERTEX_CAPACITY]),
            r.choice([0, 3, 6, 0x18, r.randrange(0, 61), INDEX_CAPACITY]), flags]


def execute_args(r):
    mode = state_word(r) | r.choice([0, 0x80000])
    texture = r.choice([-1, 0, 1, 2, 9])
    kind = r.choice(list(GOOD_TYPES) * 4 + list(BAD_TYPES))
    vertex_type = r.choice([VERTEX_TYPE] * 6 + [0, 0x1C0, 0x44, 0xFFFFFFFF])
    return [mode, texture, kind, vertex_type, r.choice([0, 1, 4, 8, r.randrange(0, 41)]),
            r.choice([0, 3, 6, 0x18, r.randrange(0, 61)])]


def box_args(r):
    colour = r.choice([0xFF, 0xFFFFFFFF, 0x00000000, 0x12345678, 0xAABBCCFF, r.getrandbits(32), r.getrandbits(32) | 0xFF,
                       r.getrandbits(32) & ~0xFF])
    return [moderate(r, -50, 2000), moderate(r, -50, 2000), moderate(r, -50, 2000), moderate(r, -50, 2000), colour,
            r.choice([-1, 0, 1, 2])]


def wide_box_args(r):
    return [any_float(r, 1e4), any_float(r, 1e4), any_float(r, 1e4), any_float(r, 1e4), r.getrandbits(32), r.choice([-1, 0, 1, 2])]


def poly_args(r):
    return [r.choice([0, 1, 2, 3, 4, 5, 8, 16, 17, -1, -5]), r.choice([0, 0xFF, r.getrandbits(32)]), any_float(r, 10.0)]


def detect_args(r):
    return [any_float(r, 64.0), r.getrandbits(32), r.getrandbits(32), any_float(r, 4.0), any_float(r, 4.0),
            any_float(r, 4.0), any_float(r, 4.0)]


# ---- DrawIndexedPrimitive

def edge_indexed():
    r = random.Random(101)
    for kind in GOOD_TYPES + tuple(BAD_TYPES):
        for vertex_type in (VERTEX_TYPE, 0x1C0, 0):
            for flags in (0, 1, 0x8000, 0x8001):
                yield case_of(r, [(DRAW_INDEXED, [kind, vertex_type, 4, 6, flags])], fields=False)
    for vertex_count, index_count in ((0, 0), (1, 1), (3, 3), (8, 24), (VERTEX_CAPACITY, INDEX_CAPACITY), (5, 0), (0, 9), (10, 17)):
        yield case_of(r, [(DRAW_INDEXED, [4, VERTEX_TYPE, vertex_count, index_count, 0x8001])], fields=False)
    for flags in (2, 4, 0x4000, 0x10000, 0x7FFE, 0xFFFF0000, 0xFFFFFFFF):
        yield case_of(r, [(DRAW_INDEXED, [4, VERTEX_TYPE, 4, 6, flags])], fields=False)
    # Two draws in a row, the second with a different vertex type: the arrays stay as the first left them.
    yield case_of(r, [(DRAW_INDEXED, [4, VERTEX_TYPE, 4, 6, 0]), (DRAW_INDEXED, [4, 0, 4, 6, 0x8000])], fields=False)


def random_indexed(r):
    return case_of(r, [(DRAW_INDEXED, dip_args(r)) for _ in range(r.randrange(1, 5))], fail=r.random() < 0.2, fields=False)


# ---- Execute

def edge_execute():
    r = random.Random(102)
    for mode in (0, 0x80000, 0x100, 0x56, 0xFFFFFFFF, 0x7FFFF, 0x80001, 0x100000):
        for texture in (-1, 0, 2, 9):
            yield case_of(r, [(EXECUTE, [mode, texture, 4, VERTEX_TYPE, 4, 6])], fields=False)
    for result in (0, 1, -1, 0x887601C2, 0x7FFFFFFF, -0x80000000):
        yield case_of(r, [(SET_RESULT, [result]), (EXECUTE, [0, 0, 4, VERTEX_TYPE, 4, 6])], fields=False)
    for kind in GOOD_TYPES + BAD_TYPES:
        yield case_of(r, [(EXECUTE, [0x80000, 1, kind, VERTEX_TYPE, 6, 12])], fields=False)
    for vertex_type in (VERTEX_TYPE, 0x1C0, 0):
        yield case_of(r, [(EXECUTE, [0x80000, 1, 3, vertex_type, 6, 12])], fields=False)
    # The draw leaving a DirectDraw error behind.
    yield dict(q=dict(DDResult=[0x887601C2, -1]), vwords=vertex_words(r), iwords=index_words(r),
               script=[(EXECUTE, [0, 0, 4, VERTEX_TYPE, 4, 6]), (EXECUTE, [0, 0, 4, VERTEX_TYPE, 4, 6])])


def random_execute(r):
    return case_of(r, [(EXECUTE, execute_args(r)) for _ in range(r.randrange(1, 5))], fail=r.random() < 0.2, fields=False)


# ---- DrawFilledBox

def edge_box():
    r = random.Random(103)
    boxes = [[fb(10.0), fb(20.0), fb(110.0), fb(220.0)], [0, 0, 0, 0], [fb(-5.0), fb(-5.0), fb(5.0), fb(5.0)],
             [fb(1.0e9), fb(-1.0e9), fb(1.0e-9), fb(0.0)], [fb(math.inf), fb(0.0), fb(-math.inf), fb(1.0)],
             [fb(math.nan), fb(1.0), fb(2.0), fb(math.nan)]]
    # Each blending bit set and clear before an opaque and a transparent box.
    for state in (0, 0xFFFFFFFF, 0x100, 0x200, 0x400, 0x8000, 0x56, 0x156, 0x8056, 0x756, 0x8700, 0x55, 0x57, 0xFF, 0x1000056):
        for colour in (0xFF, 0x80, 0x12345678, 0xFFFFFFFF, 0x1FF):
            yield case_of(r, [(SET_FIELD, [4, state]), (FILLED_BOX, boxes[0] + [colour, -1])], fields=False)
    for box in boxes:
        for page in (-1, 0, 2):
            yield case_of(r, [(FILLED_BOX, box + [0x40302010, page])], fields=False)
    # The state carried from one box to the next.
    yield case_of(r, [(SET_FIELD, [4, 0]), (FILLED_BOX, boxes[0] + [0x80, -1]), (FILLED_BOX, boxes[0] + [0x80, 1]),
                      (FILLED_BOX, boxes[0] + [0xFF, 0]), (FILLED_BOX, boxes[0] + [0x7F, 0])], fields=False)
    for width, height in ((640, 480), (1, 1), (2, 3), (0, 0)):
        sx = fb(T.f32(2.0 / width)) if width else 0
        sy = fb(T.f32(2.0 / height)) if height else 0
        yield case_of(r, [(SET_DISPLAY, [sx, sy, width, height]), (FILLED_BOX, boxes[0] + [0x80, 0])], fields=False)


def random_box(r):
    maker = wide_box_args if r.random() < 0.25 else box_args
    return case_of(r, [(FILLED_BOX, maker(r)) for _ in range(r.randrange(1, 5))], fail=r.random() < 0.2,
                   special_values=r.random() < 0.2)


# ---- DrawPoly

def edge_poly():
    r = random.Random(104)
    for count in (0, 1, 2, 17, -1, -100):
        for result in (0, 7):
            yield case_of(r, [(SET_RESULT, [result]), (POLY, [count, 0x123456, fb(1.5)])])


def random_poly(r):
    return case_of(r, [(POLY, poly_args(r)) for _ in range(r.randrange(1, 4))], special_values=r.random() < 0.2)


# ---- DrawEdgeBox, DrawTestPoly, DrawProfileBox

def edge_edge_box():
    r = random.Random(105)
    for width, height in ((640, 480), (1600, 1200), (2048, 1536), (1, 1), (3, 1000), (100000, 5), (0, 0)):
        sx = fb(T.f32(2.0 / width)) if width else 0
        sy = fb(T.f32(2.0 / height)) if height else 0
        yield case_of(r, [(SET_DISPLAY, [sx, sy, width, height]), (EDGE_BOX, [])], fields=False)
    for sx in SPECIAL_FLOATS:
        for sy in (0.0, 0.00125, -0.5, math.inf, math.nan):
            yield case_of(r, [(SET_DISPLAY, [fb(sx), fb(sy), 640, 480]), (EDGE_BOX, [])], fields=False)
    # Values on both sides of the 24-bit rounding of -1 + 5 * scale.
    for scale in (2.0 ** -25, 2.0 ** -26, 3 * 2.0 ** -27, 2.0 ** -22, 0.2, 0.4, 1.0 / 3.0, 3.9999995):
        yield case_of(r, [(SET_DISPLAY, [fb(T.f32(scale)), fb(T.f32(scale)), 1, 1]), (EDGE_BOX, [])], fields=False)
    for result in (1, -1, 0x887601C2):
        yield case_of(r, [(SET_RESULT, [result]), (EDGE_BOX, [])])


def random_edge_box(r):
    return case_of(r, [(EDGE_BOX, []) for _ in range(r.randrange(1, 3))], fail=r.random() < 0.2, special_values=r.random() < 0.3)


def edge_test_poly():
    r = random.Random(106)
    for begin in (1, 0):
        for end in (1, 0):
            yield dict(q=dict(BeginDraw=[begin], EndDraw=[end, 1 - end]), vwords=vertex_words(r), iwords=index_words(r),
                       script=[(TEST_POLY, []), (TEST_POLY, [])])
    for result in (0, 0x887601C2):
        yield case_of(r, [(SET_RESULT, [result]), (TEST_POLY, [])])


def random_test_poly(r):
    return case_of(r, [(TEST_POLY, []) for _ in range(r.randrange(1, 3))], fail=r.random() < 0.2)


def edge_profile_box():
    r = random.Random(107)
    yield case_of(r, [(PROFILE_BOX, [])])
    yield case_of(r, [(PROFILE_BOX, []), (SET_RESULT, [5]), (PROFILE_BOX, [])], fields=False)


def random_profile_box(r):
    return case_of(r, [(PROFILE_BOX, []) for _ in range(r.randrange(1, 4))])


# ---- DrawDetectPoly

def edge_detect():
    r = random.Random(108)
    for size in SPECIAL_FLOATS:
        yield case_of(r, [(DETECT_POLY, [fb(size), 0x11223344, 5, fb(0.0), fb(0.25), fb(0.75), fb(1.0)])], fields=False)
    for value in SPECIAL_FLOATS:
        yield case_of(r, [(DETECT_POLY, [fb(8.0), 0xFF8040C0, 0, fb(value), fb(value), fb(1.0), fb(-value)])], fields=False)
    for colour in (0, 0xFFFFFFFF, 0x01020304, 0x80000001, 0xFF0000FF, 0x00FF00FF):
        yield case_of(r, [(DETECT_POLY, [fb(16.0), colour, -1, fb(0.0), fb(0.0), fb(1.0), fb(1.0)])], fields=False)
    for sx in (0.0, 1.0 / 320, 1.0 / 800, 1.0, 3.0e38, math.nan):
        yield case_of(r, [(SET_DISPLAY, [fb(T.f32(sx)), fb(T.f32(sx)), 4, 4]), (DETECT_POLY, [fb(0.5), 1, 2, fb(0.0), fb(0.0), fb(1.0), fb(1.0)])],
                      fields=False)
    # Sizes that make 0.5 + size round at the last place.
    for size in (2.0 ** -25, 2.0 ** -24, 3 * 2.0 ** -25, 8388607.5, 16777216.0, 1e8):
        yield case_of(r, [(DETECT_POLY, [fb(T.f32(size)), 1, 2, fb(0.0), fb(0.0), fb(1.0), fb(1.0)])], fields=False)
    yield dict(q=dict(glGetError=[0x502, 0]), vwords=vertex_words(r), iwords=index_words(r),
               script=[display_op(r), (DETECT_POLY, [fb(8.0), 7, 7, fb(0.0), fb(0.0), fb(1.0), fb(1.0)]),
                       (DETECT_POLY, [fb(8.0), 7, 7, fb(0.0), fb(0.0), fb(1.0), fb(1.0)])])


def random_detect(r):
    return case_of(r, [(DETECT_POLY, detect_args(r)) for _ in range(r.randrange(1, 4))], fail=r.random() < 0.3,
                   special_values=r.random() < 0.3, fields=False)


# ---- GetPixel

def edge_pixel():
    r = random.Random(109)
    for x, y in ((0, 0), (1, 1), (639, 479), (-1, -1), (0x7FFFFFFF, 0x7FFFFFFF), (-0x80000000, 5), (100, -7)):
        yield case_of(r, [(GET_PIXEL, [x, y])], fields=False)
    for value in (0, 0xFFFFFFFF, 0x01020304, 0x80FF7F00):
        yield dict(q=dict(Pixel=[value]), vwords=vertex_words(r), iwords=index_words(r), script=[(GET_PIXEL, [3, 4])])
    yield dict(q=dict(glGetError=[0x502, 0], Pixel=[0x0A0B0C0D, 0x11121314]), vwords=vertex_words(r), iwords=index_words(r),
               script=[(GET_PIXEL, [3, 4]), (GET_PIXEL, [5, 6])])


def random_pixel(r):
    return case_of(r, [(GET_PIXEL, [rint(r), rint(r)]) for _ in range(r.randrange(1, 4))], fail=r.random() < 0.3, fields=False)


def rint(r):
    return r.choice([r.randrange(-3, 2000), r.randrange(-0x80000000, 0x80000000), 0, 1, 639, 479])


# ---- GetPixelFillRate

def edge_fill():
    r = random.Random(110)
    sizes = [(640, 480), (1600, 1200), (1, 1), (0, 5), (5, 0), (46341, 46341), (65536, 65536), (0x7FFFFFFF, 3),
             (-3, 480), (4096, 4096)]
    for width, height in sizes:
        for clocks in ([0, 0], [100, 5000], [5, 5 + 0x10000000], [7, 3], [0x7FFFFFFF, -0x80000000], [0, 1], [3, 0x1000003]):
            yield dict(q=dict(BeginDraw=[1], EndDraw=[1], Clock=clocks), vwords=vertex_words(r), iwords=index_words(r),
                       script=[(SET_DISPLAY, [0, 0, width, height]), (FILL_RATE, [])])
    for begin in (0, 1):
        for end in (0, 1):
            yield dict(q=dict(BeginDraw=[begin], EndDraw=[end], Clock=[10, 90]), vwords=vertex_words(r), iwords=index_words(r),
                       script=[(SET_DISPLAY, [0, 0, 640, 480]), (FILL_RATE, [])])
    # A draw that leaves a DirectDraw error: after the first, the second or the third pass, or not at all.
    for after in ([5, -1, -1], [-1, 5, -1], [-1, -1, 5], [-1, -1, -1], [0x887601C2, 0, 0], [-1, 0, 0]):
        for end in (1, 0):
            yield dict(q=dict(BeginDraw=[1], EndDraw=[end], Clock=[1, 2], DDResult=after), vwords=vertex_words(r),
                       iwords=index_words(r), script=[(SET_DISPLAY, [0, 0, 800, 600]), (FILL_RATE, [])])
    # The result left over from before.
    yield dict(q=dict(BeginDraw=[1], EndDraw=[1], Clock=[1, 2]), vwords=vertex_words(r), iwords=index_words(r),
               script=[(SET_DISPLAY, [0, 0, 800, 600]), (SET_RESULT, [9]), (FILL_RATE, [])])
    yield dict(q=dict(BeginDraw=[1], EndDraw=[1], Clock=[1, 2], glGetError=[0x502]), vwords=vertex_words(r),
               iwords=index_words(r), script=[(SET_DISPLAY, [0, 0, 800, 600]), (FILL_RATE, []), (FILL_RATE, [])])


def random_fill(r):
    width = r.choice([640, 800, 1024, 1600, r.randrange(1, 5000), r.getrandbits(31), -r.randrange(1, 5000)])
    height = r.choice([480, 600, 768, 1200, r.randrange(1, 5000), r.getrandbits(31)])
    q = queues(r)
    q["BeginDraw"], q["EndDraw"] = [r.choice([1, 1, 1, 0])] * 2, [r.choice([1, 1, 1, 0])]
    q["Clock"] = [r.getrandbits(31), r.getrandbits(31)] if r.random() < 0.7 else [r.getrandbits(31)] * 2
    return dict(q=q, vwords=vertex_words(r), iwords=index_words(r),
                script=[(SET_DISPLAY, [0, 0, width, height]), (SET_RESULT, [r.choice([0, 0, 0, 5])])] +
                       [(FILL_RATE, []) for _ in range(r.randrange(1, 3))])


# ---- DrawWiredframedBoundingBox

def edge_wire():
    r = random.Random(111)
    for faces in range(0, 64):
        yield case_of(r, [(SET_FIELD, [4, 0]), (WIRE, [0, faces])], fields=False)
    for faces in (64, 0x80, 0xFF, 0x7FFFFFC0, -1, 0x123456):
        yield case_of(r, [(WIRE, [5, faces])], fields=False)
    # Each remembered bit alone, and all of them.
    for state in (0, 0x100, 0x8000, 0x800, 0x80000, 0x100000, 0x8900, 0x980900, 0xFFFFFFFF, 0x1000056, 0xFF):
        yield case_of(r, [(SET_FIELD, [4, state]), (WIRE, [0, 1])], fields=False)
    yield dict(q=dict(), vwords=[fb(float(i)) for i in range(VERTEX_CAPACITY * 8)], iwords=index_words(r),
               script=[(WIRE, [0, 0x15])])
    nan_words = [0x7FC00000, 0x7F800001 | 0x400000, 0xFFFFFFFF, 0x7F800000] * (VERTEX_CAPACITY * 2)
    yield dict(q=dict(), vwords=nan_words, iwords=index_words(r), script=[(WIRE, [0, 0x2A])])


def random_wire(r):
    return case_of(r, [(WIRE, [r.getrandbits(32), r.choice([r.randrange(0, 64), r.getrandbits(32), r.getrandbits(6) | 0x40])])
                       for _ in range(r.randrange(1, 4))], fail=r.random() < 0.2, fields=r.random() < 0.7)


# ---- everything together

def edge_mixed():
    r = random.Random(112)
    for _ in range(30):
        yield random_mixed(r)


def random_mixed(r):
    """Several operations in a row, on one draw primitive."""
    makers = [(EXECUTE, execute_args), (DRAW_INDEXED, dip_args), (FILLED_BOX, box_args), (POLY, poly_args),
              (EDGE_BOX, lambda r: []), (TEST_POLY, lambda r: []), (DETECT_POLY, detect_args),
              (GET_PIXEL, lambda r: [rint(r), rint(r)]), (FILL_RATE, lambda r: []),
              (WIRE, lambda r: [0, r.randrange(0, 64)]), (PROFILE_BOX, lambda r: []),
              (SET_FIELD, lambda r: [4, state_word(r)]), (SET_RESULT, lambda r: [r.choice([0, 0, 3])])]
    ops = []
    for _ in range(r.randrange(3, 9)):
        op, maker = r.choice(makers)
        ops.append((op, maker(r)))
    case = case_of(r, ops, fail=r.random() < 0.2)
    case["q"]["BeginDraw"] = [1]
    return case


TESTS = [
    FnTest("COpenGLDrawPrimitive::DrawIndexedPrimitive", ppc, host, edge=edge_indexed, rnd=random_indexed),
    FnTest("COpenGLDrawPrimitive::Execute", ppc, host, edge=edge_execute, rnd=random_execute),
    FnTest("COpenGLDrawPrimitive::DrawFilledBox", ppc, host, edge=edge_box, rnd=random_box),
    FnTest("COpenGLDrawPrimitive::DrawPoly", ppc, host, edge=edge_poly, rnd=random_poly),
    FnTest("COpenGLDrawPrimitive::DrawEdgeBox", ppc, host, edge=edge_edge_box, rnd=random_edge_box),
    FnTest("COpenGLDrawPrimitive::DrawTestPoly", ppc, host, edge=edge_test_poly, rnd=random_test_poly),
    FnTest("COpenGLDrawPrimitive::DrawDetectPoly", ppc, host, edge=edge_detect, rnd=random_detect),
    FnTest("COpenGLDrawPrimitive::GetPixel", ppc, host, edge=edge_pixel, rnd=random_pixel),
    FnTest("COpenGLDrawPrimitive::GetPixelFillRate", ppc, host, edge=edge_fill, rnd=random_fill),
    FnTest("COpenGLDrawPrimitive::DrawWiredframedBoundingBox", ppc, host, edge=edge_wire, rnd=random_wire),
    FnTest("COpenGLDrawPrimitive::DrawProfileBox", ppc, host, edge=edge_profile_box, rnd=random_profile_box),
    FnTest("COpenGLDrawPrimitive, operations in sequence", ppc, host, edge=edge_mixed, rnd=random_mixed),
]
