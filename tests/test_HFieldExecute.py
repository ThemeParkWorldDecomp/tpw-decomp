"""CHField::Execute (src/engine/tb_hfield.cpp), original versus translation.

Everything Execute calls is a fake on both sides (tests/unit_HFieldExecute.cpp has the host's):
the height field's helpers (CalcFullLighting, CalcLine, GetTexturesUsed, ProcessTextureParameters,
qSortTextures_lpTex), the lights (SMapLinkType::PrepareLights, CLightManager::PrepareLighting),
the draw primitive (GetNextPrimitive, DuplicateCurrentPrimitive, CheckFogOnCurrentPrimitive,
ProjectCurrentPrimitive, ExecuteCurrentPrimitive(float)) and CClipping::ClipPoly. Each records its
arguments; the larger things go to a dump of words: the polygon ClipPoly gets (its specular words,
stack garbage in the original, left out), the quads the sort gets, the primitive each execution
gets (header and the whole index memory) and, after the call, every primitive. Compared besides:
the result, the event log, the whole vertex memory, the quad records Execute leaves, the nine
facing values, the light chain and the height field's two row blocks.

A case describes the height field (the rows of cells and grid points in view, the colours, the
record's row length and quads), the globals, the texture entries, the data the helpers hand back
one call after another (CalcLine's rows, the corner bytes, the clipped vertex counts, the texture
list), the primitives and the queues of answers.

Unicorn computes fmadds and fmsubs in double precision and rounds twice (FINDINGS 2026-10-10 and
2026-10-11); HardwareRounding (one code hook per fused instruction of Execute, as
tests/test_SkyRender.py has for fmadds) gives each the processor's single rounding.

Not tested: rows that reach 127 (row + 1 would read past CalcLine's row table and mVertexColours),
cells at column 127 (the corner to its right is past the arrays) and negative cells; the original
reads whatever is there.
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
SHIM_LIBRARY = "unit_HFieldExecute"
MASK = 0xffffffff

EXECUTE_FN, EXECUTE_SIZE = 0x1000f69c, 6072
PREPARE_LIGHTS, PREPARE_LIGHTING, CALC_FULL_LIGHTING = 0x10017454, 0x100146d4, 0x1000e824
GET_TEXTURES_USED, CALC_LINE, PROCESS_TEXTURE, QSORT = 0x1000f1b0, 0x1000ea40, 0x1000f3f8, 0x1000f4ec
GET_NEXT, DUPLICATE, CHECK_FOG, PROJECT, EXECUTE_PRIMITIVE = 0x10006a58, 0x10006bec, 0x10007d08, 0x10007df4, 0x10007360
CLIP_POLY = 0x10040640

DISPLAY, SYSTEM = 0x10165634, 0x10165f80
MAPWHO_VAR, LIGHTS_VAR, LIGHT_MATRIX, DRAW_VAR, CLIP_VAR = 0x100af618, 0x100af1b8, 0x100b00bc, 0x10052b3c, 0x102b3638
CHAIN = 0x100af194
QUAD_TEXTURES, QUAD_SAVE = 0x10052dcc, 0x10062dcc
# gFaceX0, X1, X2, Y0, Y1, Y2, W0, W1, W2
FACE = [0x101731bc, 0x101731b8, 0x101731b4, 0x101731b0, 0x101731ac, 0x101731a8, 0x101731a4, 0x101731a0, 0x1017319c]

FIELD_SIZE = 0x12974
ENTRY_SIZE = 0x5c
RECORD_SIZE = 0x38
PAGE_BASE = 0x7a000000          # a page's address on the PowerPC side (never dereferenced)
POISON = 0xa5a5a5a5
N_ENTRIES, N_PAGES = 12, 8
LABEL_FIELD, LABEL_DRAW, LABEL_LIGHTS, LABEL_MATRIX, LABEL_CLIP = 1, 2, 3, 4, 5
LABEL_LIST, LABEL_TEXTURES, LABEL_SAVE, LABEL_CHAIN = 6, 7, 8, 9
LABEL_PAGE, LABEL_ENTRY, LABEL_RECORD = 20, 40, 70


def fb(x):
    return struct.unpack(">I", struct.pack(">f", T.f32(x)))[0]


def canon(w):
    """Any NaN as one value."""
    return 0x7fc00000 if w & 0x7f800000 == 0x7f800000 and w & 0x007fffff else w


def cfb(x):
    return 0x7fc00000 if math.isnan(x) else struct.unpack(">I", struct.pack(">f", x))[0]


def ws(e, a, values):
    e.write(a, struct.pack(">%dI" % len(values), *[v & MASK for v in values]))


def rs(e, a, n):
    return list(struct.unpack(">%dI" % n, e.read(a, 4 * n)))


def words_bytes(words):
    return struct.pack("<%dI" % len(words), *[w & MASK for w in words])


def vertex_bytes(words):
    """The vertex memory with the texture coordinates' NaNs as one value."""
    return words_bytes([canon(w) if i % 8 >= 6 else w for i, w in enumerate(words)])


# ----------------------------------------------------------------- the emulator's float corrections

LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr_double(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    bits = v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]
    return struct.unpack(">d", struct.pack(">Q", bits))[0]


class HardwareRounding:
    """fmadds and fmsubs as the processor rounds them (once), for Execute's code: each such
    instruction gets a code hook that writes fmaf's result to the destination register and
    steps over the instruction. Operands that are not finite are left to the emulator (the
    result is an infinity or a NaN either way)."""

    def __init__(self, e):
        self.e, self.hooks, self.sites = e, [], {}
        words = struct.unpack(">%dI" % (EXECUTE_SIZE // 4), e.read(EXECUTE_FN, EXECUTE_SIZE))
        for k, word in enumerate(words):
            if word >> 26 == 59 and (word >> 1) & 31 in (28, 29):     # fmsubs, fmadds frD, frA, frC, frB
                self.sites[EXECUTE_FN + 4 * k] = ((word >> 1) & 31 == 28, (word >> 21) & 31, (word >> 16) & 31,
                                                  (word >> 11) & 31, (word >> 6) & 31)

    def __enter__(self):
        for address in sorted(self.sites):
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=address, end=address))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        subtract, rd, ra, rb, rc = self.sites[address]
        a, c, b = fpr_double(self.e, ra), fpr_double(self.e, rc), fpr_double(self.e, rb)
        if not all(math.isfinite(x) for x in (a, c, b)):
            return
        value = float(LIBM.fmaf(a, c, -b if subtract else b))
        uc.reg_write(ppcemu.FPR[rd], struct.unpack(">Q", struct.pack(">d", value))[0])
        uc.reg_write(P.UC_PPC_REG_PC, address + 4)


# ----------------------------------------------------------------- the PowerPC side


def setup(runner):
    T.install_std_stubs(runner.emu)


def guarded_regions(case):
    return [(DISPLAY, 4), (SYSTEM, 4), (MAPWHO_VAR, 4), (LIGHTS_VAR, 4), (DRAW_VAR, 4), (CLIP_VAR, 4), (CHAIN, 4),
            (FACE[-1], 0x24), (QUAD_TEXTURES, 4 * max(1, len(case["textures"][2]))), (QUAD_SAVE, 16 * case["saved"])]


@contextlib.contextmanager
def guarded(e, case):
    """The engine globals the case overwrites, put back afterwards (other modules share them)."""
    saved = [(a, e.read(a, n)) for a, n in guarded_regions(case)]
    try:
        yield
    finally:
        for a, data in saved:
            e.write(a, data)


def page_address(label):
    if label == 0:
        return 0
    return POISON if label < 0 else PAGE_BASE + 0x100 * label


class PpcSide:
    """Memory, labels and fakes of one case on the PowerPC side."""

    def __init__(self, e, case):
        self.e, self.case = e, case
        self.dump = []
        self.last = -1
        self.lines = self.corners = self.clips = 0
        fixed = {LIGHT_MATRIX: LABEL_MATRIX, QUAD_TEXTURES: LABEL_TEXTURES, QUAD_SAVE: LABEL_SAVE}
        for k in range(1, N_PAGES + 1):
            fixed[page_address(k)] = LABEL_PAGE + k
        # the height field and its record
        self.field = e.alloc(FIELD_SIZE, b"\xa5" * FIELD_SIZE)
        fixed[self.field] = LABEL_FIELD
        for at, block in ((0x20, case["points"]), (0x128, case["cells"])):
            ws(e, self.field + at, block[:2])
            e.write(self.field + at + 8, bytes(v & 0xff for v in block[2:258]))
        first, colours = case["colours"]
        for r, row in enumerate(colours):
            ws(e, self.field + 0x2968 + 0x200 * (first + r), row)
        quads = case["quads"]
        self.quads = e.alloc(4 * len(quads) + 4, b"\xa5" * (4 * len(quads) + 4))
        e.write(self.quads, b"".join(struct.pack(">HH", a, b) for a, b in quads))
        record = e.alloc(0x30, b"\xa5" * 0x30)
        ws(e, record + 0x18, [case["row_length"]])
        ws(e, record + 0x2c, [self.quads])
        ws(e, self.field, [record])
        # the objects the globals point at
        display, system, have_list, have_chain, face = case["g"]
        mapwho = e.alloc(0x7c, b"\xa5" * 0x7c)
        self.list = e.alloc(8, b"\xa5" * 8)
        ws(e, mapwho + 0x2c, [self.list if have_list else 0])
        lights = e.alloc(8, b"\xa5" * 8)
        self.draw = e.alloc(0x40, b"\xa5" * 0x40)
        clipping = e.alloc(8, b"\xa5" * 8)
        chain = e.alloc(8, b"\xa5" * 8)
        fixed.update({self.list: LABEL_LIST, lights: LABEL_LIGHTS, self.draw: LABEL_DRAW, clipping: LABEL_CLIP,
                      chain: LABEL_CHAIN})
        ws(e, DISPLAY, [display])
        ws(e, SYSTEM, [system])
        ws(e, MAPWHO_VAR, [mapwho])
        ws(e, LIGHTS_VAR, [lights])
        ws(e, DRAW_VAR, [self.draw])
        ws(e, CLIP_VAR, [clipping])
        ws(e, CHAIN, [chain if have_chain else 0])
        for a, w in zip(FACE, face):
            ws(e, a, [w])
        e.write(QUAD_SAVE, b"\xa5" * (16 * case["saved"]))
        # texture entries
        self.entries = {0: 0}
        for label, (flags, page, place, f40) in case["entries"].items():
            a = e.alloc(ENTRY_SIZE, b"\xa5" * ENTRY_SIZE)
            ws(e, a + 8, [flags])
            ws(e, a + 0x18, [page_address(page)])
            ws(e, a + 0x1c, place)
            ws(e, a + 0x40, [f40])
            self.entries[label] = a
            fixed[a] = LABEL_ENTRY + label
        # the primitives and their memory
        self.vertex_count = len(case["vertices"]) // 8
        self.vertices = e.alloc(4 * len(case["vertices"]))
        ws(e, self.vertices, case["vertices"])
        self.records = []
        for i, (header, offset, indices) in enumerate(case["records"]):
            a = e.alloc(RECORD_SIZE, b"\xa5" * RECORD_SIZE)
            ia = e.alloc(2 * len(indices) + 2, b"\xa5" * (2 * len(indices) + 2))
            e.write(ia, struct.pack(">%dH" % len(indices), *indices))
            flags, mode, page, vcount, icount, f34 = header
            ws(e, a, [flags, mode, page_address(page)])
            ws(e, a + 0x20, [vcount, icount, self.vertices + 32 * offset, ia])
            ws(e, a + 0x34, [f34])
            self.records.append((a, ia, len(indices), self.vertices + 32 * offset))
            fixed[a] = LABEL_RECORD + i
        self.L = CL.PpcLog(fixed, case["q"])
        e.cl = self.L

    # -- the fakes
    def stubs(self):
        L = self.L
        st = CL.Stubs()
        st.real = {EXECUTE_FN}
        st.internal(PREPARE_LIGHTS, lambda e: L.rec("PrepareLights", (L.lab(e.r(3)), L.lab(e.u32(CHAIN)))))
        st.internal(PREPARE_LIGHTING, CL.recorder("PrepareLighting", "LLL", ret=lambda e, L: 1))
        st.internal(CALC_FULL_LIGHTING, CL.recorder("CalcFullLighting", "L"))
        st.internal(GET_TEXTURES_USED, self.get_textures_used)
        st.internal(CALC_LINE, self.calc_line)
        st.internal(PROCESS_TEXTURE, self.process_texture)
        st.internal(QSORT, self.qsort)
        st.internal(GET_NEXT, lambda e: self.hand_out(e, "GetNext", "GetNextPrimitive", 0))
        st.internal(DUPLICATE, lambda e: self.hand_out(e, "Duplicate", "DuplicateCurrentPrimitive", -1))
        st.internal(CHECK_FOG, self.check_fog)
        st.internal(PROJECT, self.project)
        st.internal(EXECUTE_PRIMITIVE, self.execute)
        st.internal(CLIP_POLY, self.clip_poly)
        return st

    @staticmethod
    def next_item(items, k):
        return items[min(k, len(items) - 1)]

    def get_textures_used(self, e):
        L = self.L
        L.rec("GetTexturesUsed", (L.lab(e.r(3)), L.lab(e.r(6))))
        page, count, entries = self.case["textures"]
        ws(e, e.r(4), [page_address(page)])
        ws(e, e.r(5), [count])
        if entries:
            ws(e, e.r(6), [self.entries[label] for label in entries])

    def calc_line(self, e):
        L = self.L
        L.rec("CalcLine", (L.lab(e.r(3)), T.s32(e.r(7)), T.s32(e.r(8)), T.s32(e.r(9))))
        lines = self.case["lines"]
        if lines:
            codes, points, spare = self.next_item(lines, self.lines)
            self.lines += 1
            e.write(e.r(4), bytes(codes))
            ws(e, e.r(5), points)
            ws(e, e.r(6), spare)

    def process_texture(self, e):
        L = self.L
        b = e.read(e.r(4), 4)
        number = T.s32(e.r(5) - self.quads) // 4
        quads = self.case["quads"]
        inside = 0 <= number < len(quads)
        L.rec("ProcessTextureParameters", (L.lab(e.r(3)), struct.unpack(">I", b)[0], number,
                                           quads[number][0] if inside else -1, quads[number][1] if inside else -1))
        corners = self.case["corners"]
        if corners:
            ws(e, e.r(4), [self.next_item(corners, self.corners)])
            self.corners += 1

    def qsort(self, e):
        L = self.L
        base, high = e.r(4), T.s32(e.r(6))
        L.rec("qSortTextures_lpTex", (L.lab(e.r(3)), L.lab(base), T.s32(e.r(5)), high))
        items = []
        for i in range(high + 1):
            page, first, f08, counts = rs(e, base + 16 * i, 4)
            self.dump.extend([L.lab(page) & MASK, first, f08, counts >> 16, counts & 0xffff])
            items.append((page, first, f08, counts))
        mode = L.ret("Sort", 0)
        if mode == 1:
            items.sort(key=lambda q: L.lab(q[0]))      # stable, as the host's insertion sort
        elif mode == 2:
            items.reverse()
        for i, q in enumerate(items):
            ws(e, base + 16 * i, list(q))

    def hand_out(self, e, key, name, default):
        L = self.L
        pick = L.ret(key, default)
        L.rec(name, (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), e.r(6), T.s32(e.r(7)), L.lab(e.r(8)), pick))
        if pick < 0:
            e.set_r(3, 0)
            return
        self.last = pick
        e.set_r(3, self.records[pick][0])

    def flip_mode(self, key):
        if self.last >= 0:
            a = self.records[self.last][0] + 4
            ws(self.e, a, [self.e.u32(a) ^ (self.L.ret(key, 0) & MASK)])

    def last_vertex_count(self):
        return T.s32(self.e.u32(self.records[self.last][0] + 0x20)) if self.last >= 0 else 0

    def check_fog(self, e):
        self.L.rec("CheckFogOnCurrentPrimitive", (self.L.lab(e.r(3)), self.last, self.last_vertex_count()))
        self.flip_mode("Fog.xor")

    def project(self, e):
        self.L.rec("ProjectCurrentPrimitive", (self.L.lab(e.r(3)), T.s32(e.r(4)), self.last, self.last_vertex_count()))
        self.flip_mode("Project.xor")

    def dump_record(self, i):
        e, L = self.e, self.L
        a, ia, n, vertices = self.records[i]
        flags, mode, page = rs(e, a, 3)
        vcount, icount, vdata, idata = rs(e, a + 0x20, 4)
        self.dump.extend([flags, mode, L.lab(page) & MASK, vcount, icount, e.u32(a + 0x34),
                          1 if vdata == vertices and idata == ia else 0])
        self.dump.extend(struct.unpack(">%dH" % n, e.read(ia, 2 * n)))

    def execute(self, e):
        L = self.L
        L.rec("ExecuteCurrentPrimitive", (L.lab(e.r(3)), cfb(e.f(1)), self.last))
        if self.last >= 0:
            self.dump_record(self.last)
        e.set_r(3, T.u32(L.ret("Execute", 1)))

    def clip_poly(self, e):
        L = self.L
        out, vin, count_at, stride = e.r(5), e.r(6), e.r(7), T.s32(e.r(8))
        count = T.s32(e.u32(count_at))
        L.rec("ClipPoly", (L.lab(e.r(3)), T.s32(e.r(4)), T.s32(out - self.vertices), count, stride))
        for i in range(min(count, 21)):
            w = rs(e, vin + i * stride, 8)
            self.dump.extend(canon(x) if j >= 6 else x for j, x in enumerate(w) if j != 5)
        k = self.clips
        made = self.next_item(self.case["clips"], k) if self.case["clips"] else 3
        self.clips += 1
        salt = self.case["salt"]
        for i in range(made):
            ws(e, out + i * stride, [0x9e3779b9 * (k * 64 + i * 8 + j + 1) + salt for j in range(8)])
        ws(e, count_at, [made])
        e.set_r(3, 1)

    # -- after the call
    def state(self):
        e, L = self.e, self.L
        quads = []
        for i in range(self.case["saved"]):
            page, first, f08, counts = rs(e, QUAD_SAVE + 16 * i, 4)
            quads.extend([L.lab(page), T.s32(first), T.s32(f08), T.s16(counts >> 16), T.s16(counts)])
        face = [e.u32(a) for a in FACE]
        rows = []
        for at in (0x20, 0x128):
            rows.extend(T.s32(w) for w in rs(e, self.field + at, 2))
            rows.extend(T.s8(b) for b in e.read(self.field + at + 8, 256))
        return quads, face, L.lab(e.u32(CHAIN)), rows


def result(ret, log, dump, vertices, quads, face, chain, rows):
    return {"ret": ret, "log": log, "dump": words_bytes(dump), "vertices": vertex_bytes(vertices), "quads": quads,
            "face": face, "chain": chain, "rows": rows}


def ppc(e, case):
    with guarded(e, case):
        side = PpcSide(e, case)
        stubs = side.stubs()
        saved = stubs.activate(e)
        try:
            with HardwareRounding(e):
                r3 = T.call_ppc(e, EXECUTE_FN, [side.field])[0]
            for i in range(len(case["records"])):
                side.dump_record(i)
            vertices = rs(e, side.vertices, len(case["vertices"]))
            return result(T.s32(r3), side.L.log, side.dump, vertices, *side.state())
        finally:
            stubs.deactivate(e, saved)


# ----------------------------------------------------------------- the host side


def array(ctype, values):
    return (ctype * max(1, len(values)))(*values)


def host(lib, case):
    lib.tpw_hx_pointer.restype = C.c_void_p
    lib.tpw_hx_pointer.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_hx_field.argtypes = [C.POINTER(C.c_int32), C.POINTER(C.c_int32), C.POINTER(C.c_uint32), C.c_int32,
                                 C.c_int32, C.c_uint32, C.POINTER(C.c_uint16), C.c_int32]
    lib.tpw_hx_globals.argtypes = [C.c_uint32, C.c_uint32, C.c_int32, C.c_int32, C.POINTER(C.c_uint32)]
    lib.tpw_hx_entry.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_hx_vertices.argtypes = [C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_hx_record.argtypes = [C.c_int32, C.POINTER(C.c_uint32), C.c_int32, C.POINTER(C.c_uint16), C.c_int32]
    lib.tpw_hx_lines.argtypes = [C.POINTER(C.c_uint8), C.POINTER(C.c_uint32), C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_hx_corners.argtypes = [C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_hx_clips.argtypes = [C.POINTER(C.c_int32), C.c_int32, C.c_uint32]
    lib.tpw_hx_textures.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int32), C.c_int32]
    lib.tpw_hx_run.restype = C.c_int32
    lib.tpw_hx_dump_records.argtypes = [C.c_int32]
    lib.tpw_hx_dump_n.restype = C.c_uint32
    lib.tpw_hx_dump_copy.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_hx_vertices_store.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_hx_state.argtypes = [C.c_int32, C.POINTER(C.c_int32), C.POINTER(C.c_uint32), C.POINTER(C.c_int32),
                                 C.POINTER(C.c_int32)]

    first, colours = case["colours"]
    flat_colours = [w & MASK for row in colours for w in row]
    quads = [v for q in case["quads"] for v in q]
    lib.tpw_hx_field(array(C.c_int32, [T.s32(v) for v in case["points"]]),
                     array(C.c_int32, [T.s32(v) for v in case["cells"]]), array(C.c_uint32, flat_colours), first,
                     len(colours), case["row_length"], array(C.c_uint16, quads), len(case["quads"]))
    display, system, have_list, have_chain, face = case["g"]
    lib.tpw_hx_globals(display, system, have_list, have_chain, array(C.c_uint32, face))
    for label, (flags, page, place, f40) in case["entries"].items():
        lib.tpw_hx_entry(label, flags, page, array(C.c_uint32, place), T.s32(f40))
    lib.tpw_hx_vertices(array(C.c_uint32, case["vertices"]), len(case["vertices"]))
    for i, (header, offset, indices) in enumerate(case["records"]):
        lib.tpw_hx_record(i, array(C.c_uint32, [v & MASK for v in header]), offset, array(C.c_uint16, indices),
                          len(indices))
    lines = case["lines"]
    lib.tpw_hx_lines(array(C.c_uint8, [b for line in lines for b in line[0]]),
                     array(C.c_uint32, [w for line in lines for w in line[1]]),
                     array(C.c_uint32, [w for line in lines for w in line[2]]), len(lines))
    lib.tpw_hx_corners(array(C.c_uint32, case["corners"]), len(case["corners"]))
    lib.tpw_hx_clips(array(C.c_int32, case["clips"]), len(case["clips"]), case["salt"])
    page, count, entries = case["textures"]
    lib.tpw_hx_textures(page, count, array(C.c_int32, entries), len(entries))

    fixed = {LABEL_FIELD: lib.tpw_hx_pointer(0, 0), LABEL_DRAW: lib.tpw_hx_pointer(1, 0),
             LABEL_LIGHTS: lib.tpw_hx_pointer(2, 0), LABEL_MATRIX: lib.tpw_hx_pointer(3, 0),
             LABEL_CLIP: lib.tpw_hx_pointer(4, 0), LABEL_LIST: lib.tpw_hx_pointer(5, 0),
             LABEL_TEXTURES: lib.tpw_hx_pointer(6, 0), LABEL_SAVE: lib.tpw_hx_pointer(7, 0),
             LABEL_CHAIN: lib.tpw_hx_pointer(8, 0)}
    for k in range(1, N_PAGES + 1):
        fixed[LABEL_PAGE + k] = lib.tpw_hx_pointer(9, k)
    for label in case["entries"]:
        fixed[LABEL_ENTRY + label] = lib.tpw_hx_pointer(10, label)
    for i in range(len(case["records"])):
        fixed[LABEL_RECORD + i] = lib.tpw_hx_pointer(11, i)
    CL.host_begin(lib, fixed, case["q"])
    r = lib.tpw_hx_run()
    log = CL.host_end(lib)
    lib.tpw_hx_dump_records(len(case["records"]))
    n = lib.tpw_hx_dump_n()
    dump = (C.c_uint32 * max(n, 1))()
    lib.tpw_hx_dump_copy(dump)
    vertices = (C.c_uint32 * len(case["vertices"]))()
    lib.tpw_hx_vertices_store(vertices)
    saved = case["saved"]
    quads_out = (C.c_int32 * max(1, 5 * saved))()
    face_out = (C.c_uint32 * 9)()
    chain = C.c_int32()
    rows = (C.c_int32 * 516)()
    lib.tpw_hx_state(saved, quads_out, face_out, C.byref(chain), rows)
    return result(r, log, list(dump)[:n], list(vertices), list(quads_out)[:5 * saved], list(face_out), chain.value,
                  list(rows))


# ----------------------------------------------------------------- cases
#
# A case is a dict: points and cells (the two SGamutQuads blocks: first row, end row, 128 low and
# 128 high columns), colours (first row, a list of 128-word rows), row_length and quads (the
# record's row length, its quads as (flags, second halfword)), g (display flags, system flags,
# whether global list 6 has a head, whether the light chain holds something, the nine facing
# values), entries (label -> flags, page, [U size, V size, U offset, V offset] as bits, mF40),
# textures (GetTexturesUsed's first page, count and entry list), lines (CalcLine's rows: 128
# codes, 512 point words, 128 spare words), corners (ProcessTextureParameters' words, the first
# corner's byte highest), clips (ClipPoly's vertex counts) and salt, vertices (the vertex memory),
# records (header [flags, blending mode, page, vertex count, index count, mF34], first vertex,
# index memory), saved (quad records compared) and q (queues).

SPECIAL = [0.0, -0.0, 1.0, -1.0, 1e30, -1e30, 3.4028234663852886e38, math.inf, -math.inf, math.nan, 1e-40, 1e-30]


def rfl(r, lo, hi, special=0.02):
    """A random float's bits: in [lo, hi], or now and then an edge value."""
    if r.random() < special:
        return fb(r.choice(SPECIAL))
    return fb(r.uniform(lo, hi))


def random_code(r, bias):
    """An outside code: 0 mostly, else bits of six planes with plane 1 the commonest (so that
    neighbours often share one)."""
    if r.random() < bias:
        return 0
    if r.random() < 0.6:
        return 1
    return r.getrandbits(6) | (1 if r.random() < 0.3 else 0)


def make_line(r, bias=0.6, special=0.02):
    codes = [random_code(r, bias) for _ in range(128)]
    points = []
    for _ in range(128):
        points += [rfl(r, -200, 200, special), rfl(r, -200, 200, special), rfl(r, 0, 1, special),
                   rfl(r, 0.01, 2, special)]
    return codes, points, [r.getrandbits(32) for _ in range(128)]


def visited(case):
    """The cells Execute walks: (row, cell, quad number) in its order."""
    cells = case["cells"]
    out = []
    for row in range(T.s32(cells[0]), T.s32(cells[1])):
        for cell in range(T.s8(cells[2 + row]), T.s8(cells[130 + row])):
            out.append((row, cell, cell + row * case["row_length"]))
    return out


def finish(case, r, entry_null=0.15, clip_max=7):
    """Fills in what depends on the cells: the texture list, the helpers' data, the memory sizes."""
    cells = visited(case)
    drawn = [c for c in cells if not case["quads"][c[2]][0] & 1]
    page = r.randint(0, N_PAGES)
    entries = [0 if r.random() < entry_null else r.randint(1, N_ENTRIES) for _ in drawn]
    case["textures"] = (page, r.randint(0, 9), entries)
    if case["lines"] is None:
        case["lines"] = [make_line(r) for _ in range(max(1, T.s32(case["cells"][1]) - T.s32(case["cells"][0]) + 1))]
    if case["corners"] is None:
        case["corners"] = [r.choice([0x00010203, 0x01020300, 0x03000102, 0x02030001, 0x00030201]) if r.random() < 0.7
                           else (r.randint(0, 3) << 24) | (r.randint(0, 3) << 16) | (r.randint(0, 3) << 8) | r.randint(0, 3)
                           for _ in range(max(1, len(drawn)))]
    if case["clips"] is None:
        case["clips"] = [r.choice([3, 3, 4, 5, 0, 1, 2, 6, clip_max]) if r.random() < 0.97 else r.randint(10, 20)
                         for _ in range(2 * max(1, len(drawn)))]
    biggest = max([4] + [2 * c for c in case["clips"]])
    n_vertices = len(drawn) * biggest + 16
    offset = case.get("offset", 0)
    case["vertices"] = [r.getrandbits(32) for _ in range(8 * (n_vertices + offset))]
    n_indices = len(drawn) * 3 * (biggest - 2 if biggest > 4 else 2) + 8
    records = []
    for i in range(case.get("n_records", 10)):
        header = [r.getrandbits(32), r.getrandbits(32), r.randint(-1, N_PAGES), r.getrandbits(32), r.getrandbits(32),
                  r.getrandbits(32)]
        records.append((header, offset if i == 0 else r.randint(0, 3), [r.getrandbits(16) for _ in range(n_indices)]))
    case["records"] = records
    case["saved"] = len(cells) + 2
    q = case["q"]
    q.setdefault("Duplicate", list(range(1, len(records))) + [-1])
    return case


def random_entries(r):
    entries = {}
    for label in range(1, N_ENTRIES + 1):
        flags = r.getrandbits(32)
        if r.random() < 0.7:
            flags |= 0x1000
        elif r.random() < 0.8:
            flags &= ~0x1000
        place = [rfl(r, 0.0, 1.0), rfl(r, 0.0, 1.0), rfl(r, 0.0, 1.0), rfl(r, 0.0, 1.0)]
        entries[label] = (flags, r.randint(0, N_PAGES), place, r.getrandbits(32))
    return entries


def base(seed, first=None, rows=None, width=None, **kw):
    """A case of `rows` rows of cells from row `first`, up to `width` cells a row."""
    r = random.Random(seed)
    if rows is None:
        rows = r.randint(1, 6)
    if first is None:
        first = r.randint(0, 126 - rows)
    if width is None:
        width = r.randint(1, 16)
    points = [first, first + rows + 1] + [r.randint(-128, 127) for _ in range(256)]
    cells = [first, first + rows] + [0] * 256
    for row in range(128):
        low = r.randint(0, 126 - width) if width < 127 else 0
        cells[2 + row] = low
        cells[130 + row] = low + r.randint(max(0, width // 2), width) if r.random() < 0.9 else low
    row_length = max(cells[130:258]) + r.randint(0, 3)
    n_quads = (first + rows + 1) * row_length + 130
    quads = []
    for _ in range(n_quads):
        flags = r.getrandbits(16)
        if r.random() < 0.85:
            flags &= ~1
        quads.append((flags, r.getrandbits(16)))
    colours = (first, [[r.getrandbits(32) for _ in range(128)] for _ in range(rows + 1)])
    face = [r.getrandbits(32) for _ in range(9)]
    display = r.getrandbits(32)
    system = r.getrandbits(32) | (0x80000000 if r.random() < 0.5 else 0x40000000)
    case = {"points": points, "cells": cells, "colours": colours, "row_length": row_length, "quads": quads,
            "g": (display, system, int(r.random() < 0.8), int(r.random() < 0.8), face),
            "entries": random_entries(r), "lines": None, "corners": None, "clips": None,
            "salt": r.getrandbits(32), "q": {"GetNext": [0], "Execute": [1], "Sort": [r.choice([0, 1, 1, 2])],
                                             "Fog.xor": [r.getrandbits(32)], "Project.xor": [r.getrandbits(32)]}}
    case.update(kw)
    return case, r


def set_g(case, display=None, system=None, have_list=None, have_chain=None):
    g = list(case["g"])
    for i, v in enumerate((display, system, have_list, have_chain)):
        if v is not None:
            g[i] = v
    case["g"] = tuple(g)


def rnd(r):
    seed = r.getrandbits(32)
    case, rr = base(seed)
    return finish(case, rr)


def set_all_quads(case, flags):
    case["quads"] = [(flags, b) for _, b in case["quads"]]


def flat_line(r, code=0, x=None):
    """A row whose points all have one code; x as given (a list of 128 bits) or random."""
    codes, points, colours = make_line(r, special=0.0)
    codes = [code] * 128
    if x is not None:
        for i in range(128):
            points[4 * i] = x[i]
    return codes, points, colours


def edges():
    cases = []
    # early returns: rows the wrong way round, no view or light change, no primitive
    for first, end in ((5, 4), (127, 0), (5, 5), (0, 0)):
        c, r = base(1, rows=1)
        c["cells"][0], c["cells"][1] = first, end
        cases.append(finish(c, r))
    for system in (0, 0x3fffffff, 0x40000000, 0x80000000, 0xc0000000):
        c, r = base(2, rows=2)
        set_g(c, system=system)
        cases.append(finish(c, r))
    c, r = base(3, rows=2)
    c["q"]["GetNext"] = [-1]
    cases.append(finish(c, r))
    # fog on and off, the halved colour, the light list and chain
    for display in (0, 0x2000, 0x800, 0x2800, 0xffffffff, 0xffffd7ff):
        for flags in (0x0400, 0x0402, 0x0000, 0x0002):
            c, r = base(4, rows=2, width=6)
            set_g(c, display=display, system=0xc0000000)
            set_all_quads(c, flags)
            cases.append(finish(c, r))
    for have_list in (0, 1):
        for have_chain in (0, 1):
            c, r = base(5, rows=1)
            set_g(c, have_list=have_list, have_chain=have_chain)
            cases.append(finish(c, r))
    # every cell skipped by flag 1; every cell outside one plane; every cell inside
    c, r = base(6, rows=3)
    set_all_quads(c, 0x0001)
    cases.append(finish(c, r))
    for code in (1, 0x20, 0):
        for flags in (0, 2):
            c, r = base(7, rows=3, width=10)
            set_all_quads(c, flags)
            c["lines"] = [flat_line(r, code) for _ in range(4)]
            cases.append(finish(c, r))
    # the facing combinations of the two triangles, inside and clipped: points laid out so that
    # each triangle's determinant has the sign asked for (x per column, y per row, rhw 1)
    for code in (0, 3):
        for flags in (0, 2):
            for xs in ((1.0, 1.0), (1.0, -1.0), (-1.0, 1.0), (-1.0, -1.0), (0.0, 0.0)):
                c, r = base(8, rows=2, width=8)
                set_all_quads(c, flags)
                lines = []
                for k in range(3):
                    codes, points, colours = make_line(r, special=0.0)
                    for i in range(128):
                        points[4 * i] = fb(i * xs[0] if k % 2 == 0 else i * xs[1] + 0.5)
                        points[4 * i + 1] = fb(k * 3.0)
                        points[4 * i + 3] = fb(1.0)
                    lines.append(([code] * 128, points, colours))
                c["lines"] = lines
                cases.append(finish(c, r))
    # clipped vertex counts, including none, too few for a triangle and the most
    for counts in ([0], [0, 3], [3, 0], [1, 2], [2, 2], [9, 9], [5, 1], [4, 8, 0, 0, 7], [10, 11], [17, 18, 26, 9],
                   [12, 0, 19, 3]):
        c, r = base(9, rows=3, width=8)
        c["lines"] = [make_line(r, bias=0.2) for _ in range(4)]
        c["clips"] = counts
        cases.append(finish(c, r))
    # null entries, entries off the cache, pages in a run
    for null in (0.0, 1.0):
        c, r = base(10, rows=3)
        cases.append(finish(c, r, entry_null=null))
    c, r = base(11, rows=3)
    c["entries"] = {k: (f & ~0x1000, p, pl, f40) for k, (f, p, pl, f40) in c["entries"].items()}
    cases.append(finish(c, r))
    # the sort's orders, the runs, the duplicates failing, the executions failing
    for mode in (0, 1, 2):
        c, r = base(12, rows=4, width=12)
        c["q"]["Sort"] = [mode]
        cases.append(finish(c, r))
    for k in range(4):
        c, r = base(13, rows=4, width=12)
        c["q"]["Sort"] = [1]
        c["q"]["Duplicate"] = list(range(1, k + 1)) + [-1]
        cases.append(finish(c, r))
        c, r = base(13, rows=4, width=12)
        c["q"]["Sort"] = [1]
        c["q"]["Execute"] = [1] * k + [0, 1]
        cases.append(finish(c, r))
    # the blending mode is read after the fog check and before the projection
    for fog, project in ((0, 0), (0x5a5a, 0), (0, 0x3c3c), (0x100000, 0x80000)):
        c, r = base(14, rows=2)
        set_g(c, display=0x2000)
        c["q"]["Fog.xor"] = [fog]
        c["q"]["Project.xor"] = [project]
        cases.append(finish(c, r))
    # the first primitive's vertices start further in; extreme rows and columns
    for offset in (1, 3):
        c, r = base(15, rows=2, offset=offset)
        cases.append(finish(c, r))
    for first, rows in ((0, 1), (125, 1), (120, 6), (0, 4)):
        c, r = base(16, first=first, rows=rows, width=127)
        cases.append(finish(c, r))
    # enough vertices for vertices * 2,000,000 to wrap (more than 1,073)
    c, r = base(17, rows=6, width=30)
    set_all_quads(c, 0x0002)
    c["lines"] = [make_line(r, bias=0.0, special=0.0) for _ in range(7)]
    c["lines"] = [([1 if (i // 2) % 2 == 0 else 2 for i in range(128)], p, col) for _, p, col in c["lines"]]
    c["clips"] = [9]
    c["q"]["Sort"] = [1]
    cases.append(finish(c, r, clip_max=9))
    # special values in the points and the entries' places
    for special in (0.3, 1.0):
        c, r = base(18, rows=3)
        c["lines"] = [make_line(r, special=special) for _ in range(4)]
        for k, (f, p, pl, f40) in list(c["entries"].items()):
            c["entries"][k] = (f, p, [fb(r.choice(SPECIAL)) for _ in range(4)], f40)
        cases.append(finish(c, r))
    # every corner order at every corner
    for word in (0x00000000, 0x01010101, 0x02020202, 0x03030303, 0x03020100, 0x00010203):
        c, r = base(19, rows=2)
        c["corners"] = [word]
        cases.append(finish(c, r))
    return cases


def bf(w):
    return struct.unpack(">f", struct.pack(">I", w & MASK))[0]


def faces(a, b, c, unfused=()):
    """The facing test on corners (x, y, rhw) as Python floats: fused as the original, or with
    the multiply-subtracts named in `unfused` (0, 1 the left side's inner and outer, 2, 3 the
    right side's) rounded after the multiply."""
    x0, y0, w0 = a
    x1, y1, w1 = b
    x2, y2, w2 = c

    def mul(p, q):
        return T.f32(p * q)

    def msub(k, p, q, s):
        return T.f32(mul(p, q) - s) if k in unfused else float(LIBM.fmaf(p, q, -s))
    left = msub(1, x1, msub(0, y2, w0, mul(y0, w2)), mul(w1, mul(x0, y2)))
    right = msub(3, x2, msub(2, y1, w0, mul(y0, w1)), mul(w2, mul(x0, y1)))
    return left > right


def sensitive_corners(r, variant):
    """Four corners (x, y, rhw) of a cell whose triangles (0, 1, 2) and (0, 2, 3) are nearly
    flat, so that the facing test's answer for one of them depends on the multiply-subtracts
    `variant` names being fused."""
    while True:
        c0 = (T.f32(r.uniform(-100, 100)), T.f32(r.uniform(-100, 100)), T.f32(r.uniform(0.5, 2)))
        c1 = (T.f32(r.uniform(-100, 100)), T.f32(r.uniform(-100, 100)), T.f32(r.uniform(0.5, 2)))
        t = r.uniform(-2, 2)
        c2 = tuple(T.f32(c0[i] + t * (c1[i] - c0[i]) + r.uniform(-1, 1) * 1e-5 * abs(c1[i])) for i in range(3))
        c3 = tuple(T.f32(c0[i] + r.uniform(-3, 3) * (c2[i] - c0[i]) + r.uniform(-1, 1) * 1e-5) for i in range(3))
        for tri in ((c0, c1, c2), (c0, c2, c3)):
            if faces(*tri) != faces(*tri, unfused=variant):
                return c0, c1, c2, c3


def edges_rounding():
    """Cells whose facing answers depend on the fused rounding: one row, the even cells drawn
    (quad flag 2: corners in order), the odd ones skipped so that no two drawn cells share a
    corner; inside the view or clipped."""
    cases = []
    for variant in ((0,), (1,), (2,), (3,), (0, 1, 2, 3)):
        for code in (0, 4):
            c, r = base(40 + len(cases), first=20, rows=1, width=8)
            n = 30
            c["cells"][2 + 20], c["cells"][130 + 20] = 0, 2 * n - 1
            c["row_length"] = 64
            c["quads"] = [(0x0002 | (k % 64) % 2, r.getrandbits(16)) for k in range(len(c["quads"]))]
            top, bottom = make_line(r, special=0.0), make_line(r, special=0.0)
            for k in range(n):
                corners = sensitive_corners(r, variant)
                for (line, column), (x, y, w) in zip(((top, 2 * k), (bottom, 2 * k), (bottom, 2 * k + 1),
                                                      (top, 2 * k + 1)), corners):
                    line[1][4 * column:4 * column + 4] = [fb(x), fb(y), fb(0.5), fb(w)]
            c["lines"] = [([code] * 128, top[1], top[2]), ([code] * 128, bottom[1], bottom[2])]
            cases.append(finish(c, r))
    return cases


TESTS = [FnTest("CHField::Execute", ppc, host, edge=lambda: edges() + edges_rounding(), rnd=rnd)]
