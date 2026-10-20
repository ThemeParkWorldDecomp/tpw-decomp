"""The sky's per-frame members (tb_sky_render.cpp): CSky::Prepare, RenderAdd and Render, original
versus translation.

Everything the sky calls outside the unit is a fake on both sides: the texture cache's Prepare,
CTextureEntry::Lock and GetBlendingMode, CMatrix::TransformVertex, RotateVertex_Y and the draw
primitive's GetNextPrimitive, AddClipPolyList, ExecuteCurrentPrimitive and DrawSprite. Each records
its arguments (tests/calllog.h); the larger things (matrices, corners, primitives with their vertex
memory) go to a dump of words that is compared after the call, a NaN being one value. The
math unit is the real CMatrixPowerPC on both sides (as in test_MeshMatrix.py), so the sky's
matrix products and its vertex transforms are the original's. Compared after every case: the
result, the event log, the dump, the sky's words in the Mac's layout, the display, the three matrices
and every primitive handed out.

Two instructions Unicorn does not execute as the processor does are corrected for the code
under test (HardwareRounding below; FINDINGS 2026-10-08 and 2026-10-10): fmadds, which Unicorn
rounds twice, and fctiwz of a value above INT32_MAX, which it gives as 0x80000000 and the processor
saturates. The sky and the math unit use no other fused instruction.

The original's UV constants in Render are the caller's registers f28..f31 when the sky texture is
on no page; the PowerPC side sets them to zero before each call and the translation uses zero.
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
SHIM_LIBRARY = "unit_SkyRender"
MASK = 0xffffffff

FUNCTIONS = {0: 0x1002b5b4, 1: 0x1002b808, 2: 0x1002bb04}   # Prepare, RenderAdd, Render
TCACHE_PREPARE, ENTRY_LOCK, ENTRY_BLEND = 0x10032ddc, 0x10041738, 0x100410a4
TRANSFORM_VERTEX, ROTATE_VERTEX, DRAW_SPRITE = 0x1001c584, 0x10038cd0, 0x10007edc
GET_NEXT, ADD_CLIP, EXECUTE = 0x10006a58, 0x100074fc, 0x10007360
MATH_TRANSFORM, MATH_MULT, MATH_CTOR_END = 0x10041c90, 0x100428e0, 0x100418ac
PTR_GLUE = 0x1004c858
# The code whose fmadds and fctiwz are corrected: the three functions and the math unit's
# strided Transform (1,920 bytes) and MultMatrix (716 bytes).
CODE_RANGES = [(0x1002b5b4, 0x1002b5b4 + 560), (0x1002b808, 0x1002b808 + 724), (0x1002bb04, 0x1002bb04 + 4000),
               (MATH_TRANSFORM, MATH_TRANSFORM + 1920), (MATH_MULT, MATH_MULT + 716)]

DISPLAY, DISPLAY_SIZE = 0x10165634, 0x93c
SYSTEM = 0x10165f80
RAIN_VAR, TCACHE_VAR, LIGHT_VAR, INVVIEW_VAR = 0x100b1628, 0x10173184, 0x100af1b8, 0x100afff4
PROJECTION, SCREEN, VIEW_ANGLE, MATH_VAR, DRAW_VAR = 0x100b007c, 0x100afe9c, 0x100afe98, 0x100afe94, 0x10052b3c
GUARDED = [(DISPLAY, DISPLAY_SIZE), (SYSTEM, 0x20), (RAIN_VAR, 4), (TCACHE_VAR, 4), (LIGHT_VAR, 4),
           (INVVIEW_VAR, 4), (PROJECTION, 64), (SCREEN, 64), (VIEW_ANGLE, 4), (MATH_VAR, 4), (DRAW_VAR, 4)]

SKY_SIZE = 0x2f14
SKY_WORDS = SKY_SIZE // 4
INDICES_AT, CYLINDER_INDICES_AT = 0x21fc, 0x2c88
N_INDICES, N_CYLINDER_INDICES = 1350, 288
ENTRY_SIZE = 0x5c
RECORD_WORDS = 2048 + 16      # vertex memory of a record: 256 vertices of 8 words and some guard words
LABEL_SKY, LABEL_DRAW, LABEL_CACHE, LABEL_ENTRY, LABEL_PAGE = 1, 2, 3, 10, 40
PAGE_BASE = 0x7d000000        # a page's address on the PowerPC side (never dereferenced)
CACHE_ADDRESS = 0x7d100000
N_RECORDS = 10


# ----------------------------------------------------------------- small helpers


def fb(x):
    """The bit pattern of a single-precision value."""
    return struct.unpack(">I", struct.pack(">f", T.f32(x)))[0]


def bf(w):
    return struct.unpack(">f", struct.pack(">I", w & MASK))[0]


def canon(w):
    """Any NaN as one value."""
    return 0x7fc00000 if w & 0x7f800000 == 0x7f800000 and w & 0x007fffff else w


def cfb(x):
    """Canonical bits of a float register's value."""
    return 0x7fc00000 if math.isnan(x) else struct.unpack(">I", struct.pack(">f", x))[0]


def ws(e, a, values):
    e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))


def rs(e, a, n):
    return list(struct.unpack(">" + "I" * n, e.read(a, n * 4)))


def words_bytes(words):
    """A word list as bytes for a quick whole-list comparison."""
    return struct.pack("<" + "I" * len(words), *[w & MASK for w in words])


# ----------------------------------------------------------------- the emulator's float corrections

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Makes the emulator's fmadds and fctiwz behave as the processor's do, for the code ranges
    above. Each such instruction gets a code hook of its own (the rest of the code runs at full
    speed): the hook computes the processor's result, writes it to the destination register
    and steps over the instruction. fmadds with an operand that is not finite is left to the
    emulator (the result is a NaN or an infinity either way)."""

    def __init__(self, e):
        self.e = e
        self.hooks = []
        self.sites = {}
        for begin, end in CODE_RANGES:
            words = struct.unpack(">" + "I" * ((end - begin) // 4), e.read(begin, end - begin))
            for k, word in enumerate(words):
                address = begin + 4 * k
                rd, ra, rb, rc = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, (word >> 6) & 31
                if word >> 26 == 59 and (word >> 1) & 31 == 29:        # fmadds frD, frA, frC, frB
                    self.sites[address] = ("fmadds", rd, ra, rb, rc)
                elif word >> 26 == 63 and (word >> 1) & 0x3ff == 15:   # fctiwz frD, frB
                    self.sites[address] = ("fctiwz", rd, ra, rb, rc)

    def __enter__(self):
        for address in sorted(self.sites):
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=address, end=address))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        kind, rd, ra, rb, rc = self.sites[address]
        e = self.e
        if kind == "fmadds":
            a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
            if all(math.isfinite(x) for x in (a, c, b)):
                for x in (a, c, b):
                    assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "fmadds operand is not a single"
                value = float(LIBM.fmaf(a, c, b))
                uc.reg_write(ppcemu.FPR[rd], struct.unpack(">Q", struct.pack(">d", value))[0])
                uc.reg_write(P.UC_PPC_REG_PC, address + 4)
        else:
            x = fpr_double(e, rb)
            if x >= 2147483648.0:
                uc.reg_write(ppcemu.FPR[rd], (fpr(e, rd) & 0xffffffff00000000) | 0x7fffffff)
                uc.reg_write(P.UC_PPC_REG_PC, address + 4)


# ----------------------------------------------------------------- the PowerPC side


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    # a real math unit, as in test_MeshMatrix.py
    obj = T.persist_alloc(e, 0x14)
    T.call_ppc(e, e.func("__ct__14CMatrixPowerPCFv"), [obj, 1])
    e.sky_math_unit = e.u32(obj)
    e.sky_rain = T.persist_alloc(e, 0xa1cc)
    e.sky_light = T.persist_alloc(e, 8)
    e.sky_inverse_view = T.persist_alloc(e, 64)


@contextlib.contextmanager
def guarded(e):
    """The engine globals the case overwrites, put back afterwards (other modules share them)."""
    saved = [(a, e.read(a, n)) for a, n in GUARDED]
    try:
        yield
    finally:
        for a, data in saved:
            e.write(a, data)


class Record:
    """A primitive the draw primitive hands out: its header, vertex memory and index memory."""

    def __init__(self, header, vertices, n_indices):
        self.header, self.vertices, self.n_indices = header, vertices, n_indices
        self.address = self.vertex_address = self.index_address = 0


class PpcSide:
    """Memory, labels and fakes of one case on the PowerPC side."""

    def __init__(self, e, case):
        self.e, self.case = e, case
        self.dump = []
        self.last = None
        self.sky = e.alloc(SKY_SIZE, b"\xa5" * SKY_SIZE)
        self.entry_address = {0: 0}
        fixed = {self.sky: LABEL_SKY, CACHE_ADDRESS: LABEL_CACHE}
        for label, (flags, page, floats) in case["entries"].items():
            a = e.alloc(ENTRY_SIZE, b"\xa5" * ENTRY_SIZE)
            ws(e, a + 8, [flags])
            ws(e, a + 0x18, [0 if page == 0 else PAGE_BASE + 0x100 * page])
            ws(e, a + 0x1c, floats)
            self.entry_address[label] = a
            fixed[a] = LABEL_ENTRY + label
        for page in range(1, 9):
            fixed[PAGE_BASE + 0x100 * page] = LABEL_PAGE + page
        self.draw = e.alloc(0x40, b"\xa5" * 0x40)
        fixed[self.draw] = LABEL_DRAW
        self.records = []
        for header, vertices, n_indices in case["records"]:
            r = Record(header, vertices, n_indices)
            r.address = e.alloc(0x30, b"\xa5" * 0x30)
            r.vertex_address = e.alloc(len(vertices) * 4, b"\xa5" * (len(vertices) * 4))
            r.index_address = e.alloc(max(2, n_indices * 2), b"\xa5" * max(2, n_indices * 2))
            ws(e, r.vertex_address, vertices)
            ws(e, r.address, header + [0, 0])
            ws(e, r.address + 0x28, [r.vertex_address, r.index_address])
            self.records.append(r)
        self.L = CL.PpcLog(fixed, case["q"])
        e.cl = self.L
        self.load(case)

    def load(self, case):
        e = self.e
        sky = list(case["sky"])
        slots = [0x1fd0 // 4, 0x1fd4 // 4, 0x1fd8 // 4] + [0x1fdc // 4 + 8 * i for i in range(16)]
        for s in slots:
            sky[s] = self.entry_address[sky[s]]
        ws(e, self.sky, sky)
        e.write(self.sky + INDICES_AT, struct.pack(">" + "H" * N_INDICES, *case["indices"]))
        e.write(self.sky + CYLINDER_INDICES_AT, struct.pack(">" + "H" * N_CYLINDER_INDICES, *case["cylinder_indices"]))
        e.write(DISPLAY, b"\xa5" * DISPLAY_SIZE)
        flags, aspect, z5c, z7c, system_flags, renderer, dt, light, angle = case["g"]
        ws(e, DISPLAY, [flags, aspect])
        ws(e, DISPLAY + 0x5c, [z5c])
        ws(e, DISPLAY + 0x7c, [z7c])
        ws(e, SYSTEM, [system_flags])
        ws(e, SYSTEM + 0x1c, [renderer])
        ws(e, e.sky_rain + 0x9d98, [dt])
        ws(e, e.sky_light + 4, [light])
        ws(e, VIEW_ANGLE, [angle])
        ws(e, RAIN_VAR, [e.sky_rain])
        ws(e, LIGHT_VAR, [e.sky_light])
        ws(e, TCACHE_VAR, [CACHE_ADDRESS])
        ws(e, INVVIEW_VAR, [e.sky_inverse_view])
        ws(e, MATH_VAR, [e.sky_math_unit])
        ws(e, DRAW_VAR, [self.draw])
        inverse, projection, screen = case["matrices"]
        ws(e, e.sky_inverse_view, inverse)
        ws(e, PROJECTION, projection)
        ws(e, SCREEN, screen)
        ws(e, self.draw + 0x18, [case["stride"]])
        ws(e, self.draw + 0x1c, [0])

    # -- the fakes
    def stubs(self):
        st = CL.Stubs()
        st.real = set(FUNCTIONS.values()) | {MATH_TRANSFORM, MATH_MULT, MATH_CTOR_END, PTR_GLUE}
        st.internal(TCACHE_PREPARE, CL.recorder("TCache.Prepare", "LLf"))
        st.internal(ENTRY_LOCK, CL.recorder("Entry.Lock", "L"))
        st.internal(ENTRY_BLEND, CL.recorder("Entry.GetBlendingMode", "L", ret="i"))
        st.internal(TRANSFORM_VERTEX, self.transform_vertex)
        st.internal(ROTATE_VERTEX, self.rotate_vertex)
        st.internal(GET_NEXT, self.get_next)
        st.internal(ADD_CLIP, self.add_clip)
        st.internal(EXECUTE, self.execute)
        st.internal(DRAW_SPRITE, self.draw_sprite)
        st.imp("__assertion_failed", lambda e: self.L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
        return st

    def dump_floats(self, words):
        self.dump.extend(canon(w) for w in words)

    def transform_vertex(self, e):
        matrix = rs(e, e.r(3), 16)
        vector = rs(e, e.r(5), 3)
        self.L.rec("TransformVertex", (T.s32(e.r(6)), *vector))
        self.dump_floats(matrix)
        ws(e, e.r(4), [self.L.ret(f"TV{i}") for i in range(4)])

    def rotate_vertex(self, e):
        a = e.r(3)
        angle = cfb(e.f(1))
        w = rs(e, a, 16)
        self.L.rec("RotateVertex_Y", (angle, self.L.lab(e.r(5)), canon(w[0]), canon(w[1]), canon(w[4]), canon(w[5]),
                                      canon(w[8]), canon(w[9]), canon(w[12]), canon(w[13])))
        turn = angle & 3
        for i in range(4):
            x, y = w[4 * i], w[4 * i + 1]
            if turn == 1:
                w[4 * i], w[4 * i + 1] = y, x ^ 0x80000000
            elif turn == 2:
                w[4 * i], w[4 * i + 1] = x ^ 0x80000000, y ^ 0x80000000
            elif turn == 3:
                w[4 * i], w[4 * i + 1] = y ^ 0x80000000, x
        ws(e, a, w)

    def index_of(self, address):
        for i, r in enumerate(self.records):
            if r.address == address:
                return i
        return -1

    def get_next(self, e):
        L = self.L
        pick = L.ret("GetNext", -1)
        current = L.ret("GetNext.cur", -2)
        vertex_limit = L.ret("GetNext.vlim", 0)
        index_limit = L.ret("GetNext.ilim", 0)
        L.rec("GetNextPrimitive", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), e.r(6), T.s32(e.r(7)), L.lab(e.r(8)), pick))
        if pick < 0:
            e.set_r(3, 0)
            return
        result = self.records[pick]
        self.last = result
        cur = result if current == -2 else self.records[current]
        ws(e, self.draw + 0x1c, [cur.address])
        ws(e, self.draw + 0x24, [cur.vertex_address + vertex_limit])
        ws(e, self.draw + 0x28, [cur.index_address + index_limit])
        e.set_r(3, result.address)

    def add_clip(self, e):
        L = self.L
        count_address = e.r(4)
        count = T.s32(e.u32(count_address))
        indices, index_count = e.r(5), T.s32(e.r(6))
        which = 1 if indices == self.sky + INDICES_AT else 2 if indices == self.sky + CYLINDER_INDICES_AT else 0
        h = 17
        if index_count > 0:
            for (v,) in struct.iter_unpack(">H", e.read(indices, 2 * index_count)):
                h = (h * 31 + v) & MASK
        L.rec("AddClipPolyList", (L.lab(e.r(3)), count, which, index_count, T.s32(e.r(7)), h))
        ws(e, count_address, [L.ret("Clip.count", count)])
        e.set_r(3, T.u32(L.ret("Clip.result", 0)))

    def dump_record(self, r):
        e = self.e
        header = rs(e, r.address, 12)
        self.dump.extend(header[:10])
        self.dump.append(1 if header[10] == r.vertex_address and header[11] == r.index_address else 0)
        vertices = rs(e, r.vertex_address, len(r.vertices))
        self.dump.extend(w if i % 8 in (4, 5) else canon(w) for i, w in enumerate(vertices))

    def execute(self, e):
        L = self.L
        current = self.index_of(e.u32(self.draw + 0x1c))
        L.rec("Execute", (L.lab(e.r(3)), cfb(e.f(1)), current, -1 if self.last is None else self.records.index(self.last)))
        if self.last is not None:
            self.dump_record(self.last)
        e.set_r(3, T.u32(L.ret("Execute", 1)))

    def draw_sprite(self, e):
        L = self.L
        p = e.r(4)
        flags, vertices, f08, blend, texture, f14, u0, v0, u1, v1, f28 = rs(e, p, 11)
        L.rec("DrawSprite", (flags, T.s32(f08), blend, L.lab(texture), canon(u0), canon(v0), canon(u1), canon(v1),
                             T.s32(f28), T.s32(e.r(5))))
        self.dump_floats(rs(e, vertices, 16))
        e.set_r(3, T.u32(L.ret("DrawSprite", 1)))

    # -- results
    def sky_state(self):
        e = self.e
        words = rs(e, self.sky, SKY_WORDS)
        reverse = {a: label for label, a in self.entry_address.items()}
        for s in [0x1fd0 // 4, 0x1fd4 // 4, 0x1fd8 // 4] + [0x1fdc // 4 + 8 * i for i in range(16)]:
            words[s] = reverse.get(words[s], 0xfffffff0)
        return words, list(struct.unpack(">" + "H" * N_INDICES, e.read(self.sky + INDICES_AT, 2 * N_INDICES))), \
            list(struct.unpack(">" + "H" * N_CYLINDER_INDICES, e.read(self.sky + CYLINDER_INDICES_AT, 2 * N_CYLINDER_INDICES)))


def canonical_sky(words):
    """The words that hold floats the functions write (positions, the matrix) with NaNs as one value."""
    words = list(words)
    for i in list(range(0x1fa0 // 4, 0x1fd0 // 4)) + list(range(0x2ecc // 4, 0x2f0c // 4)):
        words[i] = canon(words[i])
    for i in range(INDICES_AT // 4, 0x2ec8 // 4):       # the two halfword arrays are compared apart
        words[i] = 0
    return words


def canonical_display(words):
    """The display's words; the two that copy a float are compared with NaNs as one value."""
    words = list(words)
    for i in (0x5c // 4, 0x7c // 4):
        words[i] = canon(words[i])
    return words


def result(ret, log, dump, sky, indices, cylinder_indices, display, matrices):
    return {"ret": ret, "log": log, "dump": words_bytes(dump), "sky": words_bytes(canonical_sky(sky)),
            "indices": indices, "cylinder_indices": cylinder_indices,
            "display": words_bytes(canonical_display(display)), "matrices": words_bytes(matrices)}


def ppc(e, case):
    fn = case["fn"]
    with guarded(e):
        side = PpcSide(e, case)
        stubs = side.stubs()
        saved = stubs.activate(e)
        try:
            for i in range(28, 32):
                e.set_f(i, 0.0)
            args = [side.sky] + ([case["arg"]] if fn == 1 else [])
            with HardwareRounding(e):
                r3 = T.call_ppc(e, FUNCTIONS[fn], args)[0]
            for r in side.records:
                side.dump_record(r)
            sky, indices, cylinder_indices = side.sky_state()
            matrices = rs(e, e.sky_inverse_view, 16) + rs(e, PROJECTION, 16) + rs(e, SCREEN, 16)
            return result(T.s32(r3), side.L.log, side.dump, sky, indices, cylinder_indices,
                          rs(e, DISPLAY, DISPLAY_SIZE // 4), matrices)
        finally:
            stubs.deactivate(e, saved)


# ----------------------------------------------------------------- the host side


def u32_array(words):
    return (C.c_uint32 * len(words))(*[w & MASK for w in words])


def host(lib, case):
    fn = case["fn"]
    lib.tpw_sr_pointer.restype = C.c_void_p
    lib.tpw_sr_pointer.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_sr_entry.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.POINTER(C.c_uint32)]
    lib.tpw_sr_draw.argtypes = [C.c_uint32]
    lib.tpw_sr_record.argtypes = [C.c_int32, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32), C.c_int32, C.c_int32]
    lib.tpw_sr_run.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_sr_run.restype = C.c_int32
    lib.tpw_sr_dump_n.restype = C.c_uint32
    lib.tpw_sr_dump_records.argtypes = [C.c_int32]
    lib.tpw_sr_sky_load.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint16), C.POINTER(C.c_uint16)]
    lib.tpw_sr_sky_store.argtypes = [C.POINTER(C.c_uint32), C.POINTER(C.c_uint16), C.POINTER(C.c_uint16)]
    lib.tpw_sr_globals.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_sr_matrices.argtypes = [C.POINTER(C.c_uint32)] * 3
    lib.tpw_sr_matrices_store.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_sr_display_store.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_sr_dump_copy.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_sr_clear()
    fixed = {LABEL_SKY: lib.tpw_sr_pointer(0, 0), LABEL_DRAW: lib.tpw_sr_pointer(1, 0), LABEL_CACHE: lib.tpw_sr_pointer(2, 0)}
    for page in range(1, 9):
        fixed[LABEL_PAGE + page] = lib.tpw_sr_pointer(4, page)
    for label, (flags, page, floats) in case["entries"].items():
        lib.tpw_sr_entry(label, flags, page, u32_array(floats))
        fixed[LABEL_ENTRY + label] = lib.tpw_sr_pointer(3, label)
    for i, (header, vertices, n_indices) in enumerate(case["records"]):
        lib.tpw_sr_record(i, u32_array(header), u32_array(vertices), len(vertices), n_indices)
    lib.tpw_sr_sky_load(u32_array(case["sky"]), (C.c_uint16 * N_INDICES)(*case["indices"]),
                        (C.c_uint16 * N_CYLINDER_INDICES)(*case["cylinder_indices"]))
    lib.tpw_sr_globals(u32_array(case["g"]))
    lib.tpw_sr_matrices(*[u32_array(m) for m in case["matrices"]])
    lib.tpw_sr_draw(case["stride"])
    CL.host_begin(lib, fixed, case["q"])
    r = lib.tpw_sr_run(fn, case.get("arg", 0))
    log = CL.host_end(lib)
    lib.tpw_sr_dump_records(len(case["records"]))
    n = lib.tpw_sr_dump_n()
    dump_buffer = (C.c_uint32 * max(n, 1))()
    lib.tpw_sr_dump_copy(dump_buffer)
    sky = (C.c_uint32 * SKY_WORDS)()
    indices = (C.c_uint16 * N_INDICES)()
    cylinder_indices = (C.c_uint16 * N_CYLINDER_INDICES)()
    lib.tpw_sr_sky_store(sky, indices, cylinder_indices)
    display = (C.c_uint32 * (DISPLAY_SIZE // 4))()
    lib.tpw_sr_display_store(display)
    matrices = (C.c_uint32 * 48)()
    lib.tpw_sr_matrices_store(matrices)
    return result(r, log, list(dump_buffer)[:n], list(sky), list(indices), list(cylinder_indices), list(display),
                  list(matrices))


# ----------------------------------------------------------------- cases
#
# A case is a dict: fn (0 Prepare, 1 RenderAdd, 2 Render), arg (RenderAdd's layer mask), entries
# (label -> flags, page label, [U size, V size, U offset, V offset] as bits), sky (the Mac's
# 0x2f14 bytes as words with entry labels in the texture slots), indices and cylinder_indices
# (halfwords), g (display flags, aspect, mZ5C, mZ7C, system flags, renderer, the weather's frame
# time, the light mask, the view angle), matrices (inverse view, projection, screen), stride (the
# draw primitive's vertex size), records (header words, vertex words, index memory size) and q
# (the queues the fakes answer from).

FLT_MAX = 3.4028234663852886e38
SPECIAL = [0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0, 25.0, -25.0, 1e30, -1e30, FLT_MAX, -FLT_MAX, math.inf, -math.inf,
           math.nan, 1e-40, 3e9, -3e9, 2147483648.0, 2147483520.0, 1.0000001, -1.0000001, 0.9999999, 1e10, 123456.789]
OFF = dict(grid=0x360, colors=0xf60, index_count=0x1f60, scroll=0x1f70, pos=0x1fa0, sky_tex=0x1fd0, colour_tex=0x1fd4,
           cyl_tex=0x1fd8, adds=0x1fdc, add_count=0x21dc, angles=0x21ec, active=0x2ec8, matrix=0x2ecc,
           f2f0c=0x2f0c, f2f10=0x2f10)
N_LABELS = 12


def rfl(r, scale=10.0, special=0.03):
    """A random float's bits: moderate, or now and then an edge value."""
    if r.random() < special:
        return fb(r.choice(SPECIAL))
    return fb(r.uniform(-scale, scale))


def random_entries(r):
    entries = {}
    for label in range(1, N_LABELS + 1):
        flags = r.getrandbits(32) & ~0x101000
        if r.random() < 0.8:
            flags |= 0x1000
        if r.random() < 0.4:
            flags |= 0x100000
        page = r.randrange(1, 9) if r.random() < 0.85 else 0
        entries[label] = (flags, page, [rfl(r, 2.0) for _ in range(4)])
    return entries


def random_matrix(r):
    k = r.random()
    if k < 0.15:
        return [fb(1.0 if i % 5 == 0 else 0.0) for i in range(16)]
    if k < 0.3:
        return [rfl(r, 2.0, 0.1) for _ in range(16)]
    return [rfl(r, 8.0, 0.01) for _ in range(16)]


def label_or_none(r, p_none):
    return 0 if r.random() < p_none else r.randrange(1, N_LABELS + 1)


def random_sky(r, layers=None, p_none=0.1, adds=None, mask_bits=(1, 2, 3)):
    w = [r.getrandbits(32) for _ in range(SKY_WORDS)]
    for off in range(0, OFF["colors"], 4):                       # cylinder and grid
        w[off // 4] = rfl(r, 500.0, 0.01)
    w[OFF["index_count"] // 4] = r.randrange(0, N_INDICES + 1)
    for k in range(12):
        w[OFF["scroll"] // 4 + k] = rfl(r, 3.0, 0.05)
        w[OFF["pos"] // 4 + k] = rfl(r, 3.0, 0.08)
    w[OFF["sky_tex"] // 4] = label_or_none(r, p_none)
    w[OFF["colour_tex"] // 4] = label_or_none(r, p_none)
    w[OFF["cyl_tex"] // 4] = label_or_none(r, p_none)
    n_adds = r.randrange(0, 17) if adds is None else adds
    w[OFF["add_count"] // 4] = n_adds
    for i in range(16):
        a = OFF["adds"] // 4 + 8 * i
        w[a] = label_or_none(r, 0.15)
        w[a + 1] = 0 if r.random() < 0.5 else r.getrandbits(r.choice([8, 16, 32]))
        w[a + 2] = r.choice(mask_bits) if r.random() < 0.8 else r.getrandbits(32)
        for k in range(3):
            w[a + 3 + k] = rfl(r, 100.0, 0.02)
        w[a + 6] = rfl(r, 40.0, 0.03)
    for k in range(4):
        w[OFF["angles"] // 4 + k] = fb(r.uniform(0.0, 0.3)) if r.random() < 0.95 else rfl(r, 1.0, 1.0)
    w[OFF["active"] // 4] = r.randrange(0, 5) if layers is None else layers
    for k in range(16):
        w[OFF["matrix"] // 4 + k] = rfl(r, 8.0, 0.02)
    w[OFF["f2f0c"] // 4] = (-49) & MASK if r.random() < 0.7 else r.getrandbits(32)
    w[OFF["f2f10"] // 4] = 56 if r.random() < 0.7 else r.getrandbits(32)
    return w


def random_globals(r, fn):
    flags = r.getrandbits(32) & ~(0x80000000 | 0x400000 | 0x8 | 0x2000000)
    if fn == 0 and r.random() < 0.12:
        flags |= 0x80000000
    if fn != 0:
        flags |= 0x80000000 if r.random() < 0.3 else 0                 # not read outside Prepare
    if fn == 1 and r.random() < 0.8:
        flags |= 0x400000
    if fn == 2 and r.random() < 0.5:
        flags |= 0x400000
    if r.random() < (0.2 if fn != 2 else 0.25):
        flags |= 0x8
    if fn == 2 and r.random() < 0.4:
        flags |= 0x2000000
    system_flags = r.getrandbits(32) & ~0x4
    if r.random() < 0.2:
        system_flags |= 4
    renderer = 3 if r.random() < 0.88 else (4 if r.random() < 0.6 else r.getrandbits(32))
    dt = fb(r.uniform(-1.4, 1.4)) if r.random() < 0.9 else rfl(r, 3.0, 1.0)
    angle = fb(r.uniform(-4.0, 4.0)) if r.random() < 0.9 else rfl(r, 3.0, 1.0)
    return [flags, rfl(r, 2.0, 0.05), rfl(r, 2.0, 0.1), rfl(r, 2.0, 0.1), system_flags, renderer, dt,
            r.getrandbits(32) if r.random() < 0.5 else MASK, angle]


def random_records(r, n=N_RECORDS):
    records = []
    for i in range(n):
        header = [r.getrandbits(32) for _ in range(8)] + [r.randrange(0, 700), r.randrange(0, 2000)]
        records.append((header, [r.getrandbits(32) for _ in range(RECORD_WORDS)], r.randrange(1, 40)))
    return records


def room_queues(r, records, stride, picks, curs, fail_vertex=(), fail_index=()):
    """GetNext queues for the calls in order: the record handed out, the one in use, and the room
    before the limits, generous except where a call is asked to fail."""
    vlim, ilim = [], []
    for k, (pick, cur) in enumerate(zip(picks, curs)):
        use = pick if cur == -2 else cur
        header = records[max(use, 0)][0]
        need_v = (stride * (header[8] + 256)) & MASK
        need_i = (header[9] * 2) & MASK
        vlim.append(need_v + (r.choice([0, 0, 1, 4096]) if r.random() < 0.7 else 0) - (1 if k in fail_vertex else 0))
        ilim.append(need_i + (r.choice([0, 0, 1, 512]) if r.random() < 0.7 else 0) - (1 if k in fail_index else 0))
        vlim[-1] = max(vlim[-1], 0)
        ilim[-1] = max(ilim[-1], 0)
    return {"GetNext": picks, "GetNext.cur": curs, "GetNext.vlim": vlim, "GetNext.ilim": ilim}


def fill_queues(r, q, sprites=40, p_fail=0.05):
    q["Entry.GetBlendingMode"] = [r.getrandbits(32) if r.random() < 0.5 else r.randrange(0x1000, 0x7000)
                                  for _ in range(sprites)]
    for i in range(4):
        q[f"TV{i}"] = [rfl(r, 200.0, 0.03) for _ in range(sprites)]
    q["DrawSprite"] = [0 if r.random() < p_fail else r.choice([1, 1, 1, 5, -1]) for _ in range(sprites)]
    q["Clip.count"] = [r.choice([256, 72, r.randrange(0, 300), -1]) for _ in range(12)]
    # (a negative index count would wrap the Mac's 32-bit address sum in the room check, which the host
    # adds in 64 bits; AddClipPolyList never returns one)
    q["Clip.result"] = [r.choice([r.randrange(0, 2000), 0, 1350, 0x7fffffff // 4]) for _ in range(12)]
    q["Execute"] = [0 if r.random() < p_fail else r.choice([1, 1, 1, 2, -1]) for _ in range(12)]
    return q


def build(r, fn, **o):
    """A random case; o: sky arguments and, for Render, the draw primitive's behaviour."""
    stride = 0x20 if r.random() < 0.75 else r.choice([0, 0x10, 0x18, 0x24, 0x30, 0x40])
    records = random_records(r)
    n_calls = N_RECORDS
    picks = [k if r.random() > 0.04 else r.choice([-1, r.randrange(N_RECORDS)]) for k in range(n_calls)]
    curs = [-2 if r.random() < 0.85 else r.randrange(N_RECORDS) for _ in range(n_calls)]
    q = room_queues(r, records, stride, picks, curs,
                    fail_vertex=[k for k in range(n_calls) if r.random() < 0.04],
                    fail_index=[k for k in range(n_calls) if r.random() < 0.04])
    fill_queues(r, q, p_fail=o.get("p_fail", 0.05))
    case = {"fn": fn, "arg": o.get("arg", r.choice([0, 1, 2, 3, 3, 2, 1, -1, r.getrandbits(32)])),
            "entries": random_entries(r),
            "sky": random_sky(r, o.get("layers"), o.get("p_none", 0.1), o.get("adds"), o.get("mask_bits", (1, 2, 3))),
            "indices": [r.getrandbits(16) for _ in range(N_INDICES)],
            "cylinder_indices": [r.getrandbits(16) for _ in range(N_CYLINDER_INDICES)],
            "g": random_globals(r, fn), "matrices": [random_matrix(r) for _ in range(3)],
            "stride": stride, "records": records, "q": q}
    if fn == 1:
        case["arg"] = o.get("arg", r.choice([1, 2, 3, 2, 1, 0, 4, -1, r.getrandbits(32)]))
    return case


def rnd(fn):
    if fn == 2:
        # mostly cases that get as far as drawing: textures present, a renderer that draws
        def make(r):
            c = build(r, 2, layers=r.choice([1, 2, 3, 4, 4, 4, 0]) if r.random() < 0.9 else None,
                      p_none=0.03 if r.random() < 0.9 else 0.3, p_fail=0.012)
            if r.random() < 0.9:
                c["g"][5] = 3
            return c
        return make
    return lambda r: build(r, fn)


def sky_set(case, off, value):
    case["sky"][off // 4] = value & MASK


def set_g(case, **kw):
    names = ["flags", "aspect", "z5c", "z7c", "system_flags", "renderer", "dt", "light", "angle"]
    for k, v in kw.items():
        case["g"][names.index(k)] = v & MASK


def tex_label(case, which):
    return case["sky"][OFF[which] // 4]


def set_entry(case, label, flags=None, page=None, floats=None):
    f, pg, fl = case["entries"][label]
    case["entries"][label] = (f if flags is None else flags & MASK, pg if page is None else page,
                              fl if floats is None else floats)


def add_set(case, i, tex=None, f04=None, mask=None, size=None, position=None):
    a = OFF["adds"] // 4 + 8 * i
    if tex is not None:
        case["sky"][a] = tex
    if f04 is not None:
        case["sky"][a + 1] = f04 & MASK
    if mask is not None:
        case["sky"][a + 2] = mask & MASK
    if size is not None:
        case["sky"][a + 6] = size
    if position is not None:
        case["sky"][a + 3:a + 6] = position


def clean(case, fn, flags=0, system_flags=0):
    """A case that does the whole job: textures present and on pages, a drawing renderer."""
    for which in ("sky_tex", "cyl_tex", "colour_tex"):
        if tex_label(case, which) == 0:
            case["sky"][OFF[which] // 4] = {"sky_tex": 1, "cyl_tex": 2, "colour_tex": 3}[which]
        label = tex_label(case, which)
        set_entry(case, label, flags=(case["entries"][label][0] | 0x1000) & ~0x100000, page=1 + label % 8)
    set_g(case, flags=flags, system_flags=system_flags, renderer=3)
    case["q"]["Execute"] = [1]
    case["q"]["DrawSprite"] = [1]
    if fn != 0:
        picks = list(range(N_RECORDS))
        curs = [-2] * N_RECORDS
        case["q"].update(room_queues(random.Random(5), case["records"], case["stride"], picks, curs))
    return case


def rebuild_room(case, picks=None, curs=None, fail_vertex=(), fail_index=()):
    picks = list(range(N_RECORDS)) if picks is None else picks
    curs = [-2] * len(picks) if curs is None else curs
    case["q"].update(room_queues(random.Random(6), case["records"], case["stride"], picks, curs, fail_vertex, fail_index))


def base(fn, seed, **o):
    c = build(random.Random(seed), fn, p_none=0.0, **o)
    c["stride"] = 0x20
    return clean(c, fn)


def edges_prepare():
    cases = []

    def mk(seed=11, **kw):
        c = base(0, seed, layers=kw.pop("layers", 2), adds=kw.pop("adds", 3))
        set_g(c, dt=fb(0.5), system_flags=kw.pop("system_flags", 0), renderer=kw.pop("renderer", 3),
              flags=kw.pop("flags", 0))
        return c
    for layers in range(5):
        cases.append(mk(layers=layers))
    for missing in (("sky_tex",), ("cyl_tex",), ("sky_tex", "cyl_tex"), ("colour_tex",)):
        c = mk()
        for which in missing:
            sky_set(c, OFF[which], 0)
        cases.append(c)
    cases.append(mk(renderer=4))
    cases.append(mk(renderer=5))
    cases.append(mk(flags=0x80000000))
    cases.append(mk(flags=0x7fffffff))
    for system_flags in (0x40000000, 0x80000000, 0xc0000000, 0x3fffffff, 4, 0xc0000004):
        cases.append(mk(system_flags=system_flags))
    for adds in (0, 1, 16):
        cases.append(mk(adds=adds))
    c = mk(adds=5)
    for i in range(5):
        add_set(c, i, tex=(0 if i % 2 else i + 1))
    cases.append(c)
    # the scroll step and the wrapping of the positions
    for dt in (0.0, 1.0, 1.0000001, -1.0, -1.0000001, 0.04, -0.04, 0.9999999, math.nan, math.inf, -math.inf, -0.0, 1e-40):
        c = mk(system_flags=0)
        set_g(c, dt=fb(dt))
        cases.append(c)
        c = mk(system_flags=4)
        set_g(c, dt=fb(dt))
        cases.append(c)
    specials = [1.0, -1.0, 1.0000001, -1.0000001, 0.9999999, 2.5, -2.5, 1e10, -1e10, 3e9, -3e9, 2147483648.0,
                -2147483648.0, 2147483520.0, -2147483904.0, math.inf, -math.inf, math.nan, 0.0, -0.0, 4294967296.0]
    for k, value in enumerate(specials):
        c = mk(layers=2)
        sky_set(c, OFF["pos"], fb(value))
        sky_set(c, OFF["pos"] + 8, fb(specials[(k * 7 + 3) % len(specials)]))
        sky_set(c, OFF["pos"] + 12, fb(specials[(k * 5 + 1) % len(specials)]))
        sky_set(c, OFF["pos"] + 20, fb(value))
        for j in range(12):
            sky_set(c, OFF["scroll"] + 4 * j, fb(0.0))
        cases.append(c)
        c = mk(layers=3)                      # the scroll pushes a position out of range
        sky_set(c, OFF["pos"], fb(0.9))
        sky_set(c, OFF["scroll"], fb(value if math.isfinite(value) and abs(value) < 100 else 1.0))
        cases.append(c)
    return cases


def edges_render_add():
    cases = []

    def mk(seed=21, flags=0x400000, arg=3, adds=4, **kw):
        c = base(1, seed, adds=adds)
        set_g(c, flags=flags)
        c["arg"] = arg
        return c
    for flags in (0, 0x400000, 0x8, 0x400008, 0xffffffff, 0xffbffff7, 0x400000 | 0x2000000):
        cases.append(mk(flags=flags))
    for arg in (0, 1, 2, 3, 4, 5, 0xffffffff, 0x80000000):
        cases.append(mk(arg=arg))
    # per add: texture or none, with or without a blending override, flag 0x100000 on the entry
    for seed in range(30, 42):
        c = mk(seed=seed, adds=4)
        r = random.Random(seed)
        for i in range(4):
            add_set(c, i, tex=r.choice([0, 1, 2, 3, 4]), f04=r.choice([0, 0, 0x4321, 0xffffffff, 0x1800]),
                    mask=r.choice([1, 2, 3, 0, 4]))
        for label in range(1, 5):
            set_entry(c, label, flags=r.getrandbits(32) & ~0x101000 | r.choice([0, 0x1000]) | r.choice([0, 0x100000]))
        cases.append(c)
    # the sprite flag 4 stays once an add has set it
    for pattern in ([0, 1, 0], [1, 0, 0], [0, 0, 1], [0, 0, 0], [1, 1, 1]):
        c = mk(adds=3, arg=1)
        for i, f in enumerate(pattern):
            add_set(c, i, tex=1 + i, mask=1)
            set_entry(c, 1 + i, flags=0x1000 | (0x100000 if f else 0))
        cases.append(c)
    # DrawSprite fails at the first, a later and the last call
    for k in range(4):
        c = mk(adds=4, arg=3)
        for i in range(4):
            add_set(c, i, mask=3)
        c["q"]["DrawSprite"] = [1] * k + [0, 1, 1, 1]
        cases.append(c)
    for adds in (0, 1, 16):
        cases.append(mk(adds=adds))
    c = mk(adds=2, arg=3)
    set_g(c, aspect=fb(math.nan))
    cases.append(c)
    c = mk(adds=2, arg=3)
    set_g(c, angle=fb(0.25))
    cases.append(c)
    for angle in (0.25, 0.5, 0.75, 1.0, -0.25, math.nan, math.inf):
        c = mk(adds=2, arg=3)
        set_g(c, angle=fb(angle))
        cases.append(c)
    return cases


def edges_render():
    cases = []

    def mk(seed=31, flags=0, layers=4, system_flags=0, **kw):
        c = base(2, seed, layers=layers, adds=kw.pop("adds", 3))
        set_g(c, flags=flags, system_flags=system_flags)
        return c
    for layers in range(5):
        cases.append(mk(layers=layers))
    for flags in (0, 0x8, 0x2000000, 0x2000008, 0x400000, 0x400008, 0x2400000, 0x2400008, 0xffffffff):
        for layers in (1, 4):
            cases.append(mk(flags=flags, layers=layers))
    for which in ("sky_tex", "cyl_tex", "colour_tex"):
        c = mk()
        sky_set(c, OFF[which], 0)
        cases.append(c)
    for renderer in (4, 0):
        c = mk()
        set_g(c, renderer=renderer)
        cases.append(c)
    # a texture that is on no page
    for which in ("sky_tex", "cyl_tex"):
        for flags in (0, 0x2000000):
            c = mk(flags=flags)
            set_entry(c, tex_label(c, which), flags=0x100)
            cases.append(c)
    c = mk(flags=0x2000000, layers=3)
    set_entry(c, tex_label(c, "sky_tex"), flags=0x0)
    cases.append(c)
    # the primitive buffer: nothing handed out, no room for the vertices, no room for the indices,
    # at each call (layers 0..3, the mirrored 4..7, the cylinder)
    for flags, layers, calls in ((0, 4, 5), (0x2000000, 4, 9), (0x2000000, 2, 5), (0x8, 4, 2), (0x2008, 3, 2)):
        for k in range(calls):
            c = mk(flags=flags, layers=layers)
            picks = list(range(N_RECORDS))
            picks[k] = -1
            rebuild_room(c, picks)
            cases.append(c)
            c = mk(flags=flags, layers=layers)
            rebuild_room(c, fail_vertex=[k])
            cases.append(c)
            c = mk(flags=flags, layers=layers)
            rebuild_room(c, fail_index=[k])
            cases.append(c)
            c = mk(flags=flags, layers=layers)
            c["q"]["Execute"] = [1] * k + [0, 1]
            cases.append(c)
            c = mk(flags=flags, layers=layers)
            c["q"]["Execute"] = [1] * k + [1] + [1, 1, 1, 1, 1, 1, 1, 1, 1]
            curs = [-2] * N_RECORDS
            curs[k] = (k + 3) % N_RECORDS
            rebuild_room(c, curs=curs)
            cases.append(c)
    # the room is exactly enough, one byte short, for several strides and counts
    for stride in (0x20, 0x10, 0x18, 0x40, 0):
        for count in (0, 1, 255, 256, 700):
            for delta in (-1, 0, 1):
                c = mk(layers=1)
                c["stride"] = stride
                c["records"][0][0][8] = count
                rebuild_room(c)
                need = (stride * (count + 256)) & MASK
                c["q"]["GetNext.vlim"] = [max(need + delta, 0)] * N_RECORDS
                cases.append(c)
    for icount in (0, 1, 100, 1350):
        for delta in (-1, 0, 1):
            c = mk(layers=1)
            c["records"][0][0][9] = icount
            rebuild_room(c)
            c["q"]["GetNext.ilim"] = [max(icount * 2 + delta, 0)] * N_RECORDS
            cases.append(c)
    # the later calls of the draw primitive failing
    for flags, results in ((0, [0]), (0, [1, 0]), (0x400000, [1] * 5 + [0]), (0x400000, [1] * 10 + [0])):
        c = mk(adds=5, flags=flags)
        c["q"]["DrawSprite"] = results
        cases.append(c)
    # the colour mask and the layer angles
    for f0c, f10 in ((-49, 56), (0, 0), (-1, 1), (0x7fffffff, -0x7fffffff), (0, 0x80000000)):
        c = mk()
        sky_set(c, OFF["f2f0c"], f0c)
        sky_set(c, OFF["f2f10"], f10)
        for light in (0xffffffff, 0x12345678, 0):
            c2 = mk()
            sky_set(c2, OFF["f2f0c"], f0c)
            sky_set(c2, OFF["f2f10"], f10)
            set_g(c2, light=light)
            cases.append(c2)
    for flags in (0, 0x2000000, 0x2008):
        for value in (math.nan, math.inf, 0.0, 1e30):
            c = mk(flags=flags)
            sky_set(c, OFF["angles"], fb(value))
            sky_set(c, OFF["pos"], fb(value))
            cases.append(c)
            c = mk(flags=flags)
            sky_set(c, OFF["grid"], fb(value))
            sky_set(c, OFF["matrix"], fb(value))
            cases.append(c)
    return cases


TESTS = [FnTest("CSky::Prepare", ppc, host, edge=edges_prepare, rnd=rnd(0)),
         FnTest("CSky::RenderAdd", ppc, host, edge=edges_render_add, rnd=rnd(1)),
         FnTest("CSky::Render", ppc, host, edge=edges_render, rnd=rnd(2))]
