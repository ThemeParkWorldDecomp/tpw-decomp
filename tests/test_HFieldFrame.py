"""The height field's per-frame members and the texture cache's frame calls (src/engine/Gamut.cpp, src/engine/tb_hfield.cpp
and src/engine/TextureCache.cpp): CHField::Prepare, CHField::DivideAndConquer, CGamut::GetVertices,
CTextureCache::Refresh and CTextureCache::Execute, against the original. Every callee outside the
unit is a fake on both sides (tests/unit_HFieldFrame.cpp on the host) that records its arguments and
answers from the case's queues: CGamut::GetQuads, TestConQuad, the texture entries' Lock and Unlock,
the cache's Prepare and its steps in Execute, the procedural texture handler's PrepareList, the
projection matrix's AdjustProjection, PreprocessMatrices, CycleFrequencyTable and the error handler.
GetVertices, CsTexture::GetTextureEntry and CTextureBase::PositionUnlock run as the original on the
PowerPC side and as the translation (or the inline header code) on the host.

A case builds the objects on both sides from the same numbers (the host reads them as one flat list,
the PowerPC side writes them in the Mac's layouts), runs one function, and compares the result, the
call log, every field the function may change, the bulk arrays (the cells, the procedural tiles and
the overlays) and one flag per object that says nothing else changed.

TestConQuad, which the unit does not contain, is a fake that does what a case's script says to the
grid points' rows: nothing, what the original does for the box it is given (widens the rows it
covers), or overwrites the rows with the script's bytes.

Unicorn rounds fctiwz of a value above INT32_MAX to 0x80000000; the processor saturates to
0x7fffffff. DivideAndConquer's fctiwz instructions get a code hook that gives the processor's result
(as tests/test_SkyRender.py on main does). The functions use no fused multiply-add.
"""
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
SHIM_LIBRARY = "unit_HFieldFrame"
MASK = 0xFFFFFFFF

# The unit (run as the original on the PowerPC side).
PREPARE, DIVIDE, GET_VERTICES = 0x1000ED18, 0x1000E144, 0x1000BC70
REFRESH, EXECUTE = 0x10034204, 0x10033688
GET_TEXTURE_ENTRY, POSITION_UNLOCK = 0x100171EC, 0x10032D20
UNIT = [PREPARE, DIVIDE, GET_VERTICES, REFRESH, EXECUTE, GET_TEXTURE_ENTRY, POSITION_UNLOCK]
DIVIDE_SIZE = 1516

# The fakes (by address).
GET_QUADS, TEST_CON_QUAD = 0x1000BC40, 0x1000DD14
ENTRY_LOCK, ENTRY_UNLOCK, TC_PREPARE = 0x10041738, 0x100417C0, 0x10032DDC
PREPARE_LIST, ADJUST_PROJECTION, PREPROCESS = 0x10012D98, 0x1001C4A8, 0x1001A8C8
DUMP_STREAM, FREE_LOCKED, SORT_TEXTURES = 0x10034D50, 0x1003780C, 0x1003335C
SORT_ENTRIES, LEVEL_SURFACE, GENERATE = 0x10035300, 0x100379E0, 0x10037494
PUSH, CYCLE = 0x10008904, 0x10009770

# Engine data.
SYSTEM, DISPLAY, SETTINGS = 0x10165F80, 0x10165634, 0x10165FC8
VIEW_PTR, PROJECTION, GAMUT_PTR, TCACHE_PTR = 0x100B0078, 0x100B007C, 0x10052DB8, 0x10173184
OVERLAYS, FREQUENCY, ERRORS = 0x100A2DCC, 0x10052D74, 0x10052B44
OVERLAY_BYTES = 2500 * 0x14
SAVED = [(SYSTEM, 0x48), (DISPLAY, 0x93C), (SETTINGS, 0x50), (VIEW_PTR, 4), (GAMUT_PTR, 4), (TCACHE_PTR, 4),
         (OVERLAYS, OVERLAY_BYTES), (FREQUENCY, 4)]

# Fixed labels, as in tests/unit_HFieldFrame.cpp.
LAB_FIELD, LAB_GAMUT, LAB_CACHE, LAB_HANDLER, LAB_PROJECTION, LAB_VIEW, LAB_ERRORS = range(1, 8)
LAB_ENTRY = 10
MAX_CELLS, MAX_TEXTURES, MAX_CS, MAX_ENTRIES, MAX_PLACES, MAX_PAGES = 18000, 16, 8, 24, 24, 12
FIELD_SIZE = 0x12974
OVERLAYS_MAX = 2500
GOOD = 0xA5A5A5A5

QUEUES = {"GeneratePages": [1]}


# ----------------------------------------------------------------- helpers


def fb(x):
    """The bit pattern of a single-precision value."""
    return struct.unpack(">I", struct.pack(">f", T.f32(x)))[0]


def bf(w):
    """The single-precision value of a bit pattern."""
    return struct.unpack(">f", struct.pack(">I", w & MASK))[0]


def ws(e, a, values):
    e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))


def rs(e, a, n):
    return list(struct.unpack(">" + "I" * n, e.read(a, n * 4)))


def s8(v):
    v &= 0xFF
    return v - 256 if v >= 128 else v


def pack_ints(values):
    """Signed 32-bit numbers as bytes (the bulk arrays compare as bytes)."""
    return struct.pack("<%di" % len(values), *[T.s32(v) for v in values])


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Makes the emulator's fctiwz of a value above INT32_MAX give the processor's 0x7fffffff, for
    DivideAndConquer's code. Each fctiwz gets a code hook of its own (the rest runs at full speed):
    the hook writes the saturated result to the destination register and steps over the
    instruction."""

    def __init__(self, e):
        self.e, self.hooks, self.sites = e, [], {}
        words = struct.unpack(">" + "I" * (DIVIDE_SIZE // 4), e.read(DIVIDE, DIVIDE_SIZE))
        for k, word in enumerate(words):
            if word >> 26 == 63 and (word >> 1) & 0x3FF == 15:           # fctiwz frD, frB
                self.sites[DIVIDE + 4 * k] = ((word >> 21) & 31, (word >> 11) & 31)

    def __enter__(self):
        for address in sorted(self.sites):
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=address, end=address))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        rd, rb = self.sites[address]
        if fpr_double(self.e, rb) >= 2147483648.0:
            uc.reg_write(ppcemu.FPR[rd], (fpr(self.e, rd) & 0xFFFFFFFF00000000) | 0x7FFFFFFF)
            uc.reg_write(P.UC_PPC_REG_PC, address + 4)


# ----------------------------------------------------------------- the height field: the case


def rows_block(first=0, end=0, low=None, high=None):
    """A block of rows (SGamutQuads): first and end row, 128 low and 128 high columns."""
    return dict(first=first, end=end, low=list(low or [0] * 128), high=list(high or [0] * 128))


def hf_case(op, **kw):
    """The lobby-like case: a 6 by 5 map of plain cells, three textures, first-person quads."""
    quads = rows_block(0, 5, [0] * 128, [6] * 128)
    c = dict(op=op, system=0x80000100, settings=0, format=4, display=0, distance=fb(400.0),
             view=[fb(1.0) if i % 5 == 0 else 0 for i in range(16)],
             origin=[fb(0.0), fb(0.0)], cell=[fb(2.0), fb(2.0)], w=6, h=5, inv=[fb(0.5), fb(0.5)],
             window=[0, 0, 0x7F, 0x7F], handler=1, far=fb(100.0),
             quads=quads, verts=rows_block(1, 2, [3] * 128, [4] * 128),
             g_quads=rows_block(1, 4, [1] * 128, [5] * 128), g_verts=rows_block(2, 3, [2] * 128, [3] * 128),
             margin=0, cells=None,
             textures=[(0, 0x1111, 0x2222), (1, 0x3333, 0x4444), (2, 0x5555, 0x6666)],
             cs=[0, 1, -1], entries=[(64, 64), (32, 128)],
             script=dict(mode=0, first=0, end=0, low=[0] * 128, high=[0] * 128, set_far=0, far=fb(0.0)),
             queues={})
    c.update(kw)
    if c["cells"] is None:
        c["cells"] = [(0, 0)] * (c["w"] * c["h"])
    return c


def with_margin(c, fill=None):
    """Surrounds the cells with a margin of readable cells (3 maps' rows and 16 columns), every
    word a different valid pattern, so cells outside the map read the same on both sides."""
    margin = 3 * c["w"] + 16
    texture_count = len(c["textures"])
    pad = [(0x0100 if i % 5 == 0 else (0x8000 | i) & 0xFFFE, i % texture_count) for i in range(margin)]
    c["margin"] = margin
    c["cells"] = pad + list(c["cells"]) + [(x ^ 0x0200, (y + 1) % texture_count) for x, y in pad]
    return c


def hf_flat(c):
    """The case as the host reads it (tpw_hf_begin in unit_HFieldFrame.cpp)."""
    w = [c["system"], c["settings"], c["format"], c["display"], c["distance"]] + list(c["view"])
    w += list(c["origin"]) + list(c["cell"]) + [c["w"], c["h"]] + list(c["inv"]) + list(c["window"])
    w += [c["handler"], c["far"]]
    for block in (c["quads"], c["verts"], c["g_quads"], c["g_verts"]):
        w += [block["first"], block["end"]] + list(block["low"]) + list(block["high"])
    cells = c["cells"]
    margin = c["margin"]
    w += [margin, len(cells)]
    for flags, tex in cells:
        w += [flags, tex]
    w += [len(c["textures"])]
    for which, w0, w1 in c["textures"]:
        w += [which, w0, w1]
    w += [len(c["cs"])] + list(c["cs"])
    w += [len(c["entries"])]
    for width, height in c["entries"]:
        w += [width, height]
    s = c["script"]
    w += [s["mode"], s["first"], s["end"]] + list(s["low"]) + list(s["high"]) + [s["set_far"], s["far"]]
    return [T.s32(v) for v in w]


# ----------------------------------------------------------------- the height field: PowerPC side


class HfSide:
    """One case on the PowerPC side: the objects in the Mac's layouts, the fakes, the state."""

    def __init__(self, e, c):
        self.e, self.c = e, c
        self.saved = [(a, e.read(a, n)) for a, n in SAVED]
        pattern = lambda n: b"\xa5" * n
        self.field = e.alloc(FIELD_SIZE, pattern(FIELD_SIZE))
        self.record = e.alloc(0x30, pattern(0x30))
        cells = c["cells"]
        self.cells = e.alloc(max(4, 4 * len(cells)))
        self.textures = e.alloc(max(8, 8 * len(c["textures"])), pattern(8 * max(1, len(c["textures"]))))
        self.cs = [e.alloc(0x10, pattern(0x10)) for _ in range(MAX_CS)]
        self.frames = [e.alloc(8) for _ in range(MAX_CS)]
        self.entries = [e.alloc(0x5C, pattern(0x5C)) for _ in range(MAX_ENTRIES)]
        self.gamut = e.alloc(0x2D8, pattern(0x2D8))
        self.view = e.alloc(64)
        self.cache = e.alloc(0x20, pattern(0x20))
        self.handler = e.alloc(0x20, pattern(0x20))
        # the height field
        f = self.field
        ws(e, f, [self.record, self.textures])
        ws(e, f + 0x10, [c["inv"][0], c["inv"][1]])
        ws(e, f + 0x1C, [c["far"]])
        self.write_block(f + 0x128, c["quads"])
        self.write_block(f + 0x20, c["verts"])
        ws(e, f + 0x230, [self.handler if c["handler"] else 0])
        ws(e, f + 0x234, list(c["window"]))
        self.write_block(self.gamut + 0xC4, c["g_quads"])
        self.write_block(self.gamut + 0x1CC, c["g_verts"])
        # the record and the cells
        ws(e, self.record + 8, list(c["origin"]) + list(c["cell"]))
        ws(e, self.record + 0x18, [c["w"], c["h"]])
        ws(e, self.record + 0x2C, [self.cells + 4 * c["margin"]])
        ws(e, self.cells, [(flags << 16) | tex for flags, tex in cells])
        # the textures, their objects, frames and entries
        for i, (which, w0, w1) in enumerate(c["textures"]):
            ws(e, self.textures + 8 * i, [(w0 << 16) | w1, self.cs[which] if which >= 0 else 0])
        for i, which in enumerate(c["cs"]):
            ws(e, self.cs[i] + 8, [self.frames[i]])
            ws(e, self.frames[i], [self.entries[which] if which >= 0 else 0, self.entries[0]])
        for i, (width, height) in enumerate(c["entries"]):
            ws(e, self.entries[i] + 0xC, [((width & 0xFFFF) << 16) | (height & 0xFFFF)])
        ws(e, self.view, c["view"])
        # the engine's globals
        ws(e, SYSTEM, [c["system"]])
        ws(e, SETTINGS, [c["settings"], c["format"] & MASK])
        ws(e, DISPLAY, [c["display"]])
        ws(e, DISPLAY + 0x900, [c["distance"]])
        ws(e, DISPLAY + 0x928, [GOOD] * 4)
        ws(e, VIEW_PTR, [self.view])
        ws(e, GAMUT_PTR, [self.gamut])
        ws(e, TCACHE_PTR, [self.cache])
        e.write(OVERLAYS, pattern(OVERLAY_BYTES))
        # what the stray-write check watches: (name, address, size, allowed ranges)
        quads_end = 0x128 + 0x108
        self.watch = [
            ("field", f, FIELD_SIZE, [(0x1C, 0x20), (0x20, quads_end), (0x248, 0x248 + 4 * OVERLAYS_MAX),
                                      (0x1296C, 0x12970)]),
            ("record", self.record, 0x30, []),
            ("textures", self.textures, 8 * max(1, len(c["textures"])), []),
            ("cs", self.cs[0], 0, []),
            ("frames", self.frames[0], 0, []),
            ("entries", self.entries[0], 0, []),
            ("gamut", self.gamut, 0x2D8, []),
            ("view", self.view, 64, []),
            ("system", SYSTEM, 0x48, []),
            ("display", DISPLAY, 0x93C, [(0x928, 0x938)]),
            ("settings", SETTINGS, 0x50, [(0, 4)]),
        ]
        self.before = {name: e.read(a, n) for name, a, n, _ in self.watch}
        self.before["cs"] = [e.read(x, 0x10) for x in self.cs]
        self.before["frames"] = [e.read(x, 8) for x in self.frames]
        self.before["entries"] = [e.read(x, 0x5C) for x in self.entries]
        labels = {f: LAB_FIELD, self.gamut: LAB_GAMUT, self.cache: LAB_CACHE, self.handler: LAB_HANDLER,
                  PROJECTION: LAB_PROJECTION, self.view: LAB_VIEW}
        labels.update({x: LAB_ENTRY + i for i, x in enumerate(self.entries)})
        e.cl = self.log = CL.PpcLog(labels, {**QUEUES, **c["queues"]})
        self.stubs = self.make_stubs()
        self.old = self.stubs.activate(e)

    def write_block(self, a, block):
        ws(self.e, a, [block["first"], block["end"]])
        self.e.write(a + 8, bytes(v & 0xFF for v in block["low"]) + bytes(v & 0xFF for v in block["high"]))

    def make_stubs(self):
        st = CL.Stubs()
        st.real.update(UNIT)
        for addr, fn in {
                GET_QUADS: self.get_quads, TEST_CON_QUAD: self.test_con_quad,
                ENTRY_LOCK: CL.recorder("EntryLock", "L"), ENTRY_UNLOCK: CL.recorder("EntryUnlock", "L"),
                TC_PREPARE: CL.recorder("TCPrepare", "LLf"), PREPARE_LIST: self.prepare_list,
                ADJUST_PROJECTION: CL.recorder("AdjustProjection", "Lf"),
                PREPROCESS: CL.recorder("PreprocessMatrices", "", ret=lambda e, L: 1)}.items():
            st.internal(addr, fn)
        return st

    # -- the fakes that do more than record
    def get_quads(self, e):
        self.log.rec("GetQuads", [self.log.lab(e.r(3))])
        e.set_r(3, e.r(3) + 0xC4)

    def test_con_quad(self, e):
        this, x0, z0, x1, z1 = e.r(3), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7))
        self.log.rec("TestConQuad", [self.log.lab(this), x0, z0, x1, z1])
        s = self.c["script"]
        base = this + 0x20
        if s["mode"] == 1:
            first, end = T.s32(e.u32(base)), T.s32(e.u32(base + 4))
            low, high = bytearray(e.read(base + 8, 128)), bytearray(e.read(base + 0x88, 128))
            first = min(first, z0)
            end = max(end, z1)
            for z in range(z0, z1 + 1):
                if s8(low[z]) > x0:
                    low[z] = x0 & 0xFF
                if s8(high[z]) < x1:
                    high[z] = x1 & 0xFF
            ws(e, base, [first, end])
            e.write(base + 8, bytes(low) + bytes(high))
            if bf(e.u32(this + 0x1C)) < bf(s["far"]):
                ws(e, this + 0x1C, [s["far"]])
        elif s["mode"] == 2:
            ws(e, base, [s["first"], s["end"]])
            e.write(base + 8, bytes(v & 0xFF for v in s["low"]) + bytes(v & 0xFF for v in s["high"]))
            if s["set_far"]:
                ws(e, this + 0x1C, [s["far"]])

    def prepare_list(self, e):
        handler, tiles, overlays, count = e.r(3), e.r(4), e.r(5), T.s32(e.r(6))
        self.log.rec("PrepareList", [self.log.lab(handler), count])
        for i in range(count):
            tile = e.u32(tiles + 4 * i)
            data, width, height, fmt, flags = rs(e, overlays + 0x14 * i, 5)
            self.log.rec("PLEntry", [i, tile, self.log.lab(data), T.s32(width), T.s32(height), T.s32(fmt), flags])
        e.set_r(3, 1)

    # -- running and reading back
    def run(self):
        e, op = self.e, self.c["op"]
        entry = {1: PREPARE, 2: DIVIDE, 3: GET_VERTICES}[op]
        r3 = T.call_ppc(e, entry, [self.gamut if op == 3 else self.field])[0]
        return T.s32(r3 - self.gamut) if op == 3 else T.s32(r3)

    def same(self, name, address, size, allowed):
        e = self.e
        if name in ("cs", "frames", "entries"):
            blocks = {"cs": (self.cs, 0x10), "frames": (self.frames, 8), "entries": (self.entries, 0x5C)}[name]
            return all(e.read(a, blocks[1]) == b for a, b in zip(blocks[0], self.before[name]))
        saved, after = bytearray(self.before[name]), bytearray(e.read(address, size))
        for begin, end in allowed:
            after[begin:end] = saved[begin:end]
        return after == saved

    def state(self):
        e, f = self.e, self.field
        out = [e.u32(f + 0x1C)]
        for base in (f + 0x20, f + 0x128):
            out += [T.s32(e.u32(base)), T.s32(e.u32(base + 4))]
            out += [s8(b) for b in e.read(base + 8, 128)]
            out += [s8(b) for b in e.read(base + 0x88, 128)]
        out += [T.s32(e.u32(f + 0x1296C))]
        out += [T.s32(w) for w in rs(e, DISPLAY + 0x928, 4)]
        out += [e.u32(SETTINGS)]
        out += [1 if self.same(name, a, n, allowed) else 0 for name, a, n, allowed in self.watch]
        out = [T.s32(v) for v in out]
        big = pack_ints(rs(e, f + 0x248, OVERLAYS_MAX))
        words = rs(e, OVERLAYS, OVERLAYS_MAX * 5)
        overlay = []
        for i in range(OVERLAYS_MAX):
            data, width, height, fmt, flags = words[5 * i:5 * i + 5]
            overlay += [self.log.lab(data), T.s32(width), T.s32(height), T.s32(fmt), T.s32(flags)]
        big += pack_ints(overlay)
        cell_words = []
        for word in rs(e, self.cells, len(self.c["cells"])):
            cell_words += [word >> 16, word & 0xFFFF]
        big += pack_ints(cell_words)
        return dict(small=out, big=big)

    def close(self):
        CL.Stubs.deactivate(self.e, self.old)
        for a, data in self.saved:
            self.e.write(a, bytes(data))


def hf_ppc(e, c):
    side = HfSide(e, c)
    try:
        if c["op"] == 2:
            with HardwareRounding(e):
                result = side.run()
        else:
            result = side.run()
        return dict(result=result, state=side.state(), log=side.log.log)
    finally:
        side.close()


def hf_host(lib, c):
    CL.host_begin(lib, {}, {**QUEUES, **c["queues"]})
    begin, run, state = lib.tpw_hf_begin, lib.tpw_hf_run, lib.tpw_hf_state
    begin.argtypes, begin.restype = [C.POINTER(C.c_int32)], None
    run.argtypes, run.restype = [C.c_int32], C.c_int32
    state.argtypes, state.restype = [C.POINTER(C.c_int32), C.POINTER(C.c_int32)], C.c_int32
    flat = hf_flat(c)
    begin((C.c_int32 * len(flat))(*flat))
    result = run(c["op"])
    log = CL.host_end(lib)
    out = (C.c_int32 * (20 + OVERLAYS_MAX * 6 + MAX_CELLS * 2))()
    small = C.c_int32()
    n = state(out, C.byref(small))
    values = list(out[:small.value])
    big = bytes(memoryview(out).cast("B")[4 * small.value:4 * n])
    return dict(result=result, state=dict(small=values, big=big), log=log)


# ----------------------------------------------------------------- the height field: cases

GOOD_TEXTURES = (0, 1)           # a texture with an entry (table indexes)
TEXTURES = [(0, 0x1111, 0x2222), (1, 0x3333, 0x4444), (2, 0x5555, 0x6666), (-1, 0x7777, 0x8888)]
CS = [0, 1, -1]                  # frame 0 of each texture object: an entry index, or no entry
ENTRIES = [(64, 64), (32, 128), (-1, 0x7FFF), (-32768, 5)]


def random_block(r, lo=-20, hi=140, first_range=(-2, 130)):
    """A block of rows with random bytes (the markers 0x7f and -1 often)."""
    pick = lambda: r.choice([0x7F, -1, 0xFF, r.randrange(lo, hi), r.randrange(0, 12), r.randrange(-128, 128)])
    return rows_block(r.randrange(*first_range), r.randrange(*first_range), [pick() for _ in range(128)],
                      [pick() for _ in range(128)])


def map_cells(c, fn):
    """The cells of the w by h map from fn(row, column) -> (flags, texture)."""
    c["cells"] = [fn(row, col) for row in range(c["h"]) for col in range(c["w"])]
    return c


def quads_over(c, first, end, low, high):
    """The height field's own cells in view: rows first..end - 1, columns low..high - 1 in each."""
    c["quads"] = rows_block(first, end, [low] * 128, [high] * 128)
    return c


def proc_case(op=1, **kw):
    """Procedural textures on, a handler, the lobby-like map (6 by 5) and cells that carry pictures."""
    args = dict(settings=4, handler=1, textures=TEXTURES, cs=CS, entries=ENTRIES)
    args.update(kw)
    return hf_case(op, **args)


def prepare_edges():
    # which frames run, and first person or not
    for system in (0, 0x100, 0x40000000, 0x80000000, 0xC0000000, 0x40000100, 0x80000100, 0xC0000100, 0x3FFFFFFF,
                   0xFFFFFFFF):
        yield hf_case(1, system=system)
    r = random.Random(7)
    for first, end in ((0, 0), (3, 2), (0, 1), (2, 5), (0, 5), (4, 5), (5, 5), (6, 5), (0, 7), (0x7F, -1), (-1, -1)):
        c = with_margin(hf_case(1, system=0xC0000000))
        c["g_quads"] = rows_block(first, end, [r.randrange(0, 3) for _ in range(128)],
                                  [r.randrange(3, 7) for _ in range(128)])
        c["g_verts"] = random_block(r)
        yield c
        c = with_margin(hf_case(1))
        c["quads"] = rows_block(first, end, [r.randrange(0, 3) for _ in range(128)], [r.randrange(3, 7) for _ in range(128)])
        yield c
    # the plain map: every cell asks the cache for its picture
    yield hf_case(1)
    yield with_margin(map_cells(hf_case(1), lambda row, col: (0, (row + col) % 3)))
    yield map_cells(hf_case(1, textures=TEXTURES, cs=CS, entries=ENTRIES), lambda row, col: (0, 3))   # no texture object
    yield map_cells(hf_case(1), lambda row, col: (0, 2))                 # an entry-less texture object
    yield map_cells(hf_case(1, textures=TEXTURES, cs=CS, entries=ENTRIES), lambda row, col: (0, (row * 2 + col) % 4))
    yield map_cells(hf_case(1, textures=TEXTURES, cs=CS, entries=ENTRIES),
                    lambda row, col: ((row + col) % 2, (row + col) % 4))        # hidden cells
    for flags in (0x0001, 0x0100, 0x0080, 0x0180, 0x0181, 0xFFFE, 0xFFFF, 0x007E):
        yield map_cells(hf_case(1, textures=TEXTURES, cs=CS, entries=ENTRIES), lambda row, col: (flags, 0))
    # columns and rows outside the map (the margin holds readable cells)
    for first, end, low, high in ((0, 3, -4, 3), (0, 8, 0, 9), (2, 7, -2, 10), (0, 2, 4, 1), (0, 5, 3, 3)):
        c = with_margin(map_cells(hf_case(1), lambda row, col: (0x100 * (col % 2), (row + col) % 3)))
        quads_over(c, first, end, low, high)
        yield c
    # uneven rows
    c = with_margin(map_cells(hf_case(1), lambda row, col: (0, 1)))
    c["quads"] = rows_block(0, 5, [0, 1, 2, 3, 4] + [0] * 123, [6, 5, 4, 3, 2] + [0] * 123)
    yield c
    # procedural textures
    yield proc_case(1)                                                    # plain cells, nothing to list
    yield proc_case(1, settings=0)                                        # off
    for window in ([0, 0, 0x7F, 0x7F], [1, 1, 4, 3], [2, 2, 2, 2], [3, 0, 2, 4], [0, 4, 5, 3], [-2, -3, 8, 9],
                   [5, 4, 5, 4], [0, 0, 0, 0], [6, 5, 20, 20], [-100000, -100000, 100000, 100000],
                   [0x7FFFFFFF, 0, 0x7FFFFFFF, 4], [-0x80000000, -0x80000000, 0, 0]):
        c = map_cells(proc_case(1, window=window), lambda row, col: (0x80 if (row + col) % 3 == 0 else 0x100, (row + col) % 2))
        yield c
    for flags in (0, 1, 0x80, 0x100, 0x180, 0x181, 0x80 | 0x7E, 0xFFFE, 0xFFFF, 0x8000, 0x0140):
        yield map_cells(proc_case(1), lambda row, col: (flags, (row * 3 + col) % 2))
    yield map_cells(proc_case(1, handler=0), lambda row, col: (0x180, (row + col) % 2))          # no handler: bit cleared
    yield map_cells(proc_case(1, handler=0, settings=0x1C), lambda row, col: (0x180, (row + col) % 4))
    for settings in (0, 4, 5, 0x14, 0xFFFFFFFB, 0xFFFFFFFF, 0x80, 0x84):
        yield map_cells(proc_case(1, settings=settings), lambda row, col: (0x80, 0))
    for fmt in range(6):
        yield map_cells(proc_case(1, format=fmt), lambda row, col: (0x80, row % 2))
    # more cells than the list holds
    for w, h in ((50, 50), (51, 50), (60, 60), (100, 26), (101, 25), (127, 25), (127, 126), (30, 84), (84, 30), (10, 250)):
        if h > 126:
            continue
        c = proc_case(1, w=w, h=h)
        map_cells(c, lambda row, col: (0x180 if (row + col) % 3 else 0x100, (row + col) % 2))
        c["quads"] = rows_block(0, h, [0] * 128, [w] * 128)
        yield c
    c = proc_case(1, w=60, h=60, window=[0, 0, 49, 59])
    map_cells(c, lambda row, col: (0x180, 0))
    c["quads"] = rows_block(0, 60, [0] * 128, [60] * 128)
    yield c
    # the cache's answers do not matter to Prepare; the handler's neither
    c = map_cells(proc_case(1), lambda row, col: (0x80, 1))
    c["queues"] = {"x": [1]}
    yield c


def prepare_random(r):
    w, h = r.randrange(1, 25), r.randrange(1, 25)
    proc = r.random() < 0.5
    c = hf_case(1, system=r.choice([0x80000100, 0x40000100, 0xC0000100, 0x80000000, 0x40000000, 0xC0000000, 0x100, 0x0]),
                w=w, h=h, settings=r.choice([4, 0, 5, 0x14, 0x84]) if proc else r.choice([0, 1, 0x84, 0x14]),
                format=r.randrange(6), handler=1 if r.random() < 0.85 else 0, textures=TEXTURES, cs=CS, entries=ENTRIES,
                window=[r.randrange(-2, 8), r.randrange(-2, 8), r.randrange(-2, 30), r.randrange(-2, 30)])
    c["window"] = c["window"] if r.random() < 0.8 else [0, 0, 0x7F, 0x7F]
    flag_choices = [0, 0, 0x100, 0x80, 0x180, 1, 0x181, 0x7E, 0x5A80, r.getrandbits(16)]

    def cell(row, col):
        flags = r.choice(flag_choices)
        if flags & 0x80:
            tex = r.choice(GOOD_TEXTURES)
        else:
            tex = r.randrange(4)
        return (flags, tex)

    map_cells(c, cell)
    # the margin cells are read only when a row or column is outside the map; they also need
    # textures with entries for the procedural picture
    with_margin(c)
    c["cells"] = [((f if not (f & 0x80) else f), (t if not (f & 0x80) else 0)) for f, t in c["cells"]]
    first = r.randrange(0, h + 1)
    end = r.randrange(first - 1, h + 2)
    low = r.randrange(-2, w + 1)
    high = r.randrange(low - 1, w + 3)
    c["quads"] = rows_block(first, end, [r.choice([low, r.randrange(-2, w + 1)]) for _ in range(128)],
                            [r.choice([high, r.randrange(-2, w + 3)]) for _ in range(128)])
    c["g_quads"] = rows_block(r.randrange(0, h), r.randrange(0, h + 1), [r.randrange(0, 2) for _ in range(128)],
                              [r.randrange(w // 2, w + 1) for _ in range(128)])
    c["g_verts"] = random_block(r)
    c["verts"] = random_block(r)
    return c


def divide_case(**kw):
    """A camera over the middle of a 40 by 30 map of cells 2 by 2."""
    view = [fb(1.0) if i % 5 == 0 else 0 for i in range(16)]
    view[8], view[10] = fb(0.0), fb(1.0)
    view[12], view[14] = fb(40.0), fb(30.0)
    c = hf_case(2, view=view, w=40, h=30, cell=[fb(2.0), fb(2.0)], inv=[fb(0.5), fb(0.5)], display=0,
                distance=fb(60.0), quads=random_block(random.Random(3)), verts=random_block(random.Random(4)))
    c["script"] = dict(mode=1, first=0, end=0, low=[0] * 128, high=[0] * 128, set_far=0, far=fb(250.0))
    c.update(kw)
    c["cells"] = []
    return c


def with_view(c, x, z, m02=0.0, m22=1.0):
    c["view"] = list(c["view"])
    c["view"][8], c["view"][10], c["view"][12], c["view"][14] = fb(m02), fb(m22), fb(x), fb(z)
    return c


def divide_edges():
    r = random.Random(11)
    yield divide_case()
    yield divide_case(display=0x2000)
    yield divide_case(display=0x20)
    yield divide_case(display=0x2020)
    for mode in (0, 1, 2):
        for display in (0, 0x20, 0x2000):
            c = divide_case(display=display)
            c["script"]["mode"] = mode
            c["script"].update(first=3, end=9, low=[r.choice([0x7F, 4, -3, 0, -1]) for _ in range(128)],
                               high=[r.choice([-1, 9, 20, 0x7F, 0]) for _ in range(128)], set_far=1, far=fb(77.0))
            yield c
    # distances
    for distance in (0.0, 1.0, 4.0, 10.0, 59.9, 200.0, 5000.0, -50.0, 1e30, -1e30, float("inf"), float("-inf"), float("nan"),
                     1e-30):
        yield divide_case(display=0x2000, distance=fb(distance))
    # cameras: inside, on the edges, outside each side, far away
    for x, z in ((40.0, 30.0), (0.0, 0.0), (79.99, 59.99), (80.0, 30.0), (40.0, 60.0), (-60.0, 30.0), (40.0, -45.0),
                 (-31.0, -31.0), (-30.0, 30.0), (110.0, 30.0), (110.01, 30.0), (40.0, 90.01), (-35.0, 30.0),
                 (1e6, 0.0), (-1e6, 0.0), (0.0, 1e9), (0.0, -1e9), (1e30, 1e30), (-1e30, -1e30),
                 (float("nan"), 30.0), (40.0, float("nan")), (float("inf"), 0.0), (0.0, float("-inf"))):
        c = with_view(divide_case(display=0x2000), x, z)
        yield c
    for m02, m22 in ((1.0, 0.0), (-1.0, 0.0), (0.0, -1.0), (0.7, 0.7), (100.0, -100.0), (1e30, 1e30), (float("nan"), 0.0),
                     (0.0, float("inf")), (-3.0, 4.0)):
        yield with_view(divide_case(display=0x2000), 30.0, 20.0, m02, m22)
    # cell sizes and their inverses
    for cell, inv in (((2.0, 2.0), (0.5, 0.5)), ((4.0, 1.0), (0.25, 1.0)), ((1.0, 4.0), (1.0, 0.25)),
                      ((0.0, 2.0), (0.5, 0.5)), ((2.0, 0.0), (0.5, 0.5)), ((0.0, 0.0), (0.5, 0.5)),
                      ((2.0, 2.0), (0.0, 0.5)), ((2.0, 2.0), (0.5, 0.0)), ((2.0, 2.0), (-0.5, -0.5)),
                      ((-2.0, 3.0), (0.5, 0.3333)), ((float("nan"), 2.0), (0.5, 0.5)), ((2.0, 2.0), (float("nan"), 0.5)),
                      ((2.0, float("inf")), (0.5, 0.5)), ((2.0, 2.0), (1e30, 1e30)), ((1e-30, 1e-30), (1e30, 1e30)),
                      ((3.0, 1.5), (0.333333, 0.666667))):
        for display in (0, 0x2000):
            c = with_view(divide_case(display=display, cell=[fb(cell[0]), fb(cell[1])], inv=[fb(inv[0]), fb(inv[1])]),
                          40.0, 30.0, 0.3, 0.8)
            yield c
    for origin in ((0.0, 0.0), (10.0, -10.0), (-100.0, 100.0), (1e30, 0.0), (float("nan"), 1.0), (float("inf"), float("-inf"))):
        for display in (0, 0x20, 0x2000):
            yield with_view(divide_case(display=display, origin=[fb(origin[0]), fb(origin[1])]), 40.0, 30.0)
    # map sizes
    for w, h in ((1, 1), (1, 30), (40, 1), (127, 30), (128, 40), (130, 40), (200, 30), (30, 124), (30, 125), (255, 60), (300, 20),
                 (3, 3), (5, 2)):
        for display in (0, 0x20):
            c = with_view(divide_case(display=display, w=w, h=h), 20.0, 20.0, 0.5, 0.5)
            yield c
    # the far plane and the row before
    for far in (0.0, 100.0, -5.0, 1e30, float("nan"), float("inf"), 5000.0):
        c = divide_case(far=fb(far))
        c["script"]["far"] = fb(far * 2 if far == far else 1.0)
        yield c
    # a script that leaves row 0 empty and others (the byte before the low columns)
    for end in (0, 1, 2, 5, 100, 126, -1):
        c = divide_case(display=0x20)
        c["script"].update(mode=2, first=0, end=end, low=[0x7F] * 128, high=[0xFF] * 128, set_far=0)
        yield c
    c = divide_case(display=0x20)
    c["script"].update(mode=2, first=0, end=3, low=[0x7F, 5, 0x7F, 0x7F] + [0] * 124, high=[-1, 9, -1, 12] + [0] * 124)
    yield c
    c = divide_case(display=0x20)
    c["script"].update(mode=2, first=2, end=5, low=[0] * 128, high=[0] * 128)
    for z in range(128):
        c["script"]["low"][z] = (z * 7) % 33 - 16
        c["script"]["high"][z] = (z * 5) % 29 - 9
    yield c


def divide_random(r):
    w, h = r.choice([r.randrange(1, 130), r.randrange(10, 60), 40]), r.randrange(1, 125)
    cx, cz = r.choice([0.5, 1.0, 2.0, 3.0, r.uniform(0.2, 8.0)]), r.choice([0.5, 1.0, 2.0, 3.0, r.uniform(0.2, 8.0)])
    display = r.choice([0, 0x2000, 0x20, 0x2020, 0, 0x2000])
    origin = [r.choice([0.0, 0.0, r.uniform(-50, 50)]), r.choice([0.0, 0.0, r.uniform(-50, 50)])]
    span_x, span_z = w * cx, h * cz
    pos = lambda span, o: r.choice([o + r.uniform(-0.3, 1.3) * span, o + r.uniform(0, 1) * span, r.uniform(-1e4, 1e4)])
    c = divide_case(display=display, w=w, h=h, cell=[fb(cx), fb(cz)],
                    inv=[fb(1.0 / cx if r.random() < 0.9 else r.uniform(-1, 1)), fb(1.0 / cz if r.random() < 0.9 else r.uniform(-1, 1))],
                    origin=[fb(origin[0]), fb(origin[1])], distance=fb(r.choice([10.0, 60.0, 400.0, r.uniform(0, 500), r.uniform(-50, 5000)])),
                    quads=random_block(r), verts=random_block(r), far=fb(r.choice([100.0, r.uniform(0, 1000), 1e9])))
    with_view(c, pos(span_x, origin[0]), pos(span_z, origin[1]), r.choice([0.0, 1.0, -1.0, r.uniform(-1, 1)]),
              r.choice([0.0, 1.0, -1.0, r.uniform(-1, 1)]))
    mode = r.choice([0, 1, 1, 1, 2])
    c["script"] = dict(mode=mode, first=r.randrange(0, h + 2), end=r.randrange(-1, h + 2),
                       low=[r.choice([0x7F, -1, r.randrange(-30, 140), r.randrange(0, w + 1) % 128]) for _ in range(128)],
                       high=[r.choice([0xFF, -1, r.randrange(-30, 140), r.randrange(0, w + 1) % 128]) for _ in range(128)],
                       set_far=r.randrange(2), far=fb(r.choice([r.uniform(0, 1000), 100.0, float("nan"), 1e30])))
    return c


def vertices_edges():
    for gamut_garbage in (1, 2):
        yield hf_case(3)


def vertices_random(r):
    return hf_case(3, g_quads=random_block(r), g_verts=random_block(r))




# ----------------------------------------------------------------- the texture cache: the case

SIZES = 6


def tc_case(op, **kw):
    """A cache with three pages (sizes 0, 0 and 3), five entries and two place records."""
    c = dict(op=op, system=0x80000000, settings=0, freq=3, cache_flags=0x20000000, waiting=-1,
             counts=[2, 0, 0, 1, 0, 0],
             pages=[(0x1, 0), (0xFFFFFFFF, 2), (0x30000001, -1)],
             entries=[(0x1000, 1, 0, fb(0.25)), (0x1000, -1, 1, fb(7.5)), (0x4000, 3, 0, fb(0.0)),
                      (0x40002000, 4, 1, fb(-1.0)), (0xFFFFFFFF, -1, 0, fb(2.0))],
             places=[([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16], 136, 100),
                     ([16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1], 136, -100)],
             queues={})
    c.update(kw)
    return c


def tc_flat(c):
    """The case as the host reads it (tpw_tc_begin in unit_HFieldFrame.cpp)."""
    w = [c["system"], c["settings"], c["freq"], c["cache_flags"], c["waiting"]]
    pages = iter(c["pages"])
    for size in range(SIZES):
        w.append(c["counts"][size])
        for _ in range(c["counts"][size]):
            flags, head = next(pages)
            w += [flags, head]
    w.append(len(c["entries"]))
    for flags, nxt, place, priority in c["entries"]:
        w += [flags, nxt, place, priority]
    w.append(len(c["places"]))
    for data, f10, f14 in c["places"]:
        w += list(data) + [f10, f14]
    return [T.s32(v) for v in w]


class TcSide:
    """One cache case on the PowerPC side."""

    def __init__(self, e, c):
        self.e, self.c = e, c
        self.saved = [(a, e.read(a, n)) for a, n in SAVED]
        pattern = lambda n: b"\xa5" * n
        self.cache = e.alloc(0x1910, pattern(0x1910))
        pages = c["pages"]
        self.links = [e.alloc(0x20, pattern(0x20)) for _ in pages]
        self.textures = [e.alloc(0x18, pattern(0x18)) for _ in pages]
        self.entries = [e.alloc(0x5C, pattern(0x5C)) for _ in range(MAX_ENTRIES)]
        self.places = [e.alloc(0x18, pattern(0x18)) for _ in c["places"]]
        ws(e, SYSTEM, [c["system"]])
        ws(e, SETTINGS, [c["settings"]])
        ws(e, FREQUENCY, [c["freq"]])
        ws(e, TCACHE_PTR, [self.cache])
        f = self.cache
        ws(e, f + 0x18B4, [c["cache_flags"]])
        ws(e, f + 0x18AC, [self.entries[c["waiting"]] if c["waiting"] >= 0 else 0])
        k = 0
        for size in range(SIZES):
            ws(e, f + 0x1800 + 4 * size, [c["counts"][size]])
            for page in range(c["counts"][size]):
                flags, head = pages[k]
                ws(e, f + 0x400 * size + 4 * page, [self.links[k]])
                ws(e, self.links[k], [self.textures[k], self.entries[head] if head >= 0 else 0])
                ws(e, self.textures[k], [flags])
                k += 1
        for i, (flags, nxt, place, priority) in enumerate(c["entries"]):
            ws(e, self.entries[i], [self.places[place], self.entries[nxt] if nxt >= 0 else 0, flags])
            ws(e, self.entries[i] + 0x2C, [priority])
        for i, (data, f10, f14) in enumerate(c["places"]):
            e.write(self.places[i], bytes(data))
            ws(e, self.places[i] + 0x10, [f10, f14])
        # what the stray-write check watches
        entry_ranges = [(8, 12), (0x2C, 0x30)]
        self.watch = [("cache", [f], 0x1910, [(0x18AC, 0x18B0), (0x18B4, 0x18B8)]),
                      ("links", self.links, 0x20, []), ("entries", self.entries, 0x5C, entry_ranges),
                      ("system", [SYSTEM], 0x48, []), ("settings", [SETTINGS], 0x50, []),
                      ("display", [DISPLAY], 0x93C, [])]
        self.before = {name: [e.read(a, n) for a in addresses] for name, addresses, n, _ in self.watch}
        labels = {f: LAB_CACHE, ERRORS: LAB_ERRORS}
        labels.update({x: LAB_ENTRY + i for i, x in enumerate(self.entries)})
        e.cl = self.log = CL.PpcLog(labels, {**QUEUES, **c["queues"]})
        self.stubs = self.make_stubs()
        self.old = self.stubs.activate(e)

    def make_stubs(self):
        st = CL.Stubs()
        st.real.update(UNIT)
        for addr, fn in {
                ENTRY_UNLOCK: CL.recorder("EntryUnlock", "L"), CYCLE: CL.recorder("CycleFrequencyTable", ""),
                DUMP_STREAM: CL.recorder("DumpStream", "L"), FREE_LOCKED: CL.recorder("FreeLocked", "L"),
                SORT_TEXTURES: CL.recorder("SortTextures", "L"), SORT_ENTRIES: CL.recorder("SortEntries", "L"),
                LEVEL_SURFACE: CL.recorder("LevelSurface", "L"),
                GENERATE: CL.recorder("GeneratePages", "L", ret="i"), PUSH: CL.recorder("Push", "Lii")}.items():
            st.internal(addr, fn)
        return st

    def run(self):
        entry = {1: REFRESH, 2: EXECUTE}[self.c["op"]]
        r3 = T.call_ppc(self.e, entry, [self.cache])[0]
        return 1 if self.c["op"] == 1 else T.s32(r3)

    def index_of(self, address):
        return 0 if address == 0 else 1 + self.entries.index(address)

    def state(self):
        e, c, f = self.e, self.c, self.cache
        out = [e.u32(f + 0x18B4), self.index_of(e.u32(f + 0x18AC)), e.u32(FREQUENCY)]
        for texture in self.textures:
            out.append(e.u32(texture))
        for i in range(len(c["entries"])):
            a = self.entries[i]
            out += [e.u32(a + 8), e.u32(a + 0x2C), self.index_of(e.u32(a + 4))]
        for place in self.places:
            out += list(e.read(place, 16)) + [T.s32(e.u32(place + 0x10)), T.s32(e.u32(place + 0x14))]
        for name, addresses, size, allowed in self.watch:
            same = True
            for a, saved in zip(addresses, self.before[name]):
                after = bytearray(e.read(a, size))
                saved = bytearray(saved)
                for begin, end in allowed:
                    after[begin:end] = saved[begin:end]
                same &= after == saved
            out.append(1 if same else 0)
        return [T.s32(v) for v in out]

    def close(self):
        CL.Stubs.deactivate(self.e, self.old)
        for a, data in self.saved:
            self.e.write(a, bytes(data))


def tc_ppc(e, c):
    side = TcSide(e, c)
    try:
        result = side.run()
        return dict(result=result, state=side.state(), log=side.log.log)
    finally:
        side.close()


def tc_host(lib, c):
    CL.host_begin(lib, {}, {**QUEUES, **c["queues"]})
    begin, run, state = lib.tpw_tc_begin, lib.tpw_tc_run, lib.tpw_tc_state
    begin.argtypes, begin.restype = [C.POINTER(C.c_int32)], None
    run.argtypes, run.restype = [C.c_int32], C.c_int32
    state.argtypes, state.restype = [C.POINTER(C.c_int32)], C.c_int32
    flat = tc_flat(c)
    begin((C.c_int32 * len(flat))(*flat))
    result = run(c["op"])
    log = CL.host_end(lib)
    out = (C.c_int32 * 2000)()
    n = state(out)
    return dict(result=result, state=list(out[:n]), log=log)


def chain(r, pool, length):
    """Takes `length` entries off the pool as a list; returns (head, next-links)."""
    items = [pool.pop() for _ in range(min(length, len(pool)))]
    links = {a: b for a, b in zip(items, items[1:])}
    return (items[0] if items else -1), links, items


def tc_random_case(r, op):
    entry_count = r.randrange(0, MAX_ENTRIES + 1)
    place_count = r.randrange(1, 6)
    counts = [0] * SIZES
    for _ in range(r.randrange(0, 8)):
        counts[r.randrange(SIZES)] += 1
    page_total = sum(counts)
    pool = list(range(entry_count))
    r.shuffle(pool)
    nexts = {i: -1 for i in range(entry_count)}
    heads = []
    for _ in range(page_total):
        head, links, items = chain(r, pool, r.choice([0, 1, 2, 3, 5]))
        nexts.update(links)
        heads.append(head)
    waiting_items = []
    waiting = -1
    if pool and r.random() < 0.6:
        waiting, links, waiting_items = chain(r, pool, r.randrange(1, 5))
        nexts.update(links)
    flag_pick = lambda: r.choice([0, 0x2000, 0x40000000, 0x40002000, 0xFFFFFFFF, r.getrandbits(32), 0x1000, 0x4000])
    entries = [(flag_pick(), nexts[i], r.randrange(place_count), fb(r.choice([0.0, 1.0, -2.5, 0.25, 1e9]))) for i in range(entry_count)]
    places = [([r.choice([0, 1, 255, r.randrange(256), 16]) for _ in range(16)],
               r.choice([0, 17, -5, 0x7FFFFFFF, -0x80000000, r.randrange(-100000, 100000)]),
               r.choice([0, 1, -1, 2, -2, 0x7FFFFFFF, -0x80000000, r.randrange(-100000, 100000)])) for _ in range(place_count)]
    return tc_case(op, system=r.choice([0, 0x40000000, 0x80000000, 0xC0000000, r.getrandbits(32), 0x3FFFFFFF]),
                   settings=r.choice([0, 0x80, 0xFFFFFFFF]), freq=r.randrange(16),
                   cache_flags=r.choice([0, 1, 0x20000000, 0xFFFFFFFF, 0xFFFFFFFE, r.getrandbits(32)]),
                   waiting=waiting, counts=counts,
                   pages=[(r.choice([0, 1, 0xFFFFFFFF, 0xFFFFFFFE, r.getrandbits(32)]), heads[i]) for i in range(page_total)],
                   entries=entries, places=places, queues={"GeneratePages": [r.choice([0, 1])]})


def refresh_edges():
    for system in (0, 0x40000000, 0x80000000, 0xC0000000, 0x3FFFFFFF, 0xFFFFFFFF, 0x20000000):
        for cache_flags in (0, 1, 0x20000000, 0xFFFFFFFE, 0xFFFFFFFF):
            yield tc_case(1, system=system, cache_flags=cache_flags)
    yield tc_case(1, counts=[0] * SIZES, pages=[], entries=[], places=[([0] * 16, 0, 0)])
    yield tc_case(1, counts=[0] * SIZES, pages=[], waiting=0)
    yield tc_case(1, waiting=2)
    yield tc_case(1, waiting=0, system=0)
    for freq in range(16):
        yield tc_case(1, freq=freq)
    yield tc_case(1, counts=[1, 1, 1, 1, 1, 1], pages=[(0xFFFFFFFF, 0), (0, 1), (3, 2), (0x30000, -1), (1, 3), (0, 4)],
                  system=0xC0000000)
    yield tc_case(1, counts=[3, 0, 0, 0, 0, 0], pages=[(0xFFFFFFFF, 0), (0xFFFFFFFE, -1), (1, 4)], system=0)
    # an entry two pages share, a place two entries share, an entry that is also waiting
    yield tc_case(1, counts=[2, 0, 0, 0, 0, 0], pages=[(1, 0), (1, 0)])
    yield tc_case(1, waiting=0)
    yield tc_case(1, entries=[(0x2000, -1, 0, fb(0.5)), (0x2000, -1, 0, fb(0.5)), (0x2000, -1, 0, fb(0.5)),
                              (0x2000, -1, 0, fb(0.5)), (0x2000, -1, 0, fb(0.5))])
    for f10, f14 in ((0, 0), (5, 3), (-1, -1), (0x7FFFFFFF, 0x7FFFFFFF), (-0x80000000, -0x80000000), (3, -3), (1, 1), (0, 255)):
        yield tc_case(1, places=[([255] * 16, f10, f14), ([0, 1] * 8, f10, f14)])
    yield tc_case(1, places=[([0] * 16, 0x7FFFFFFF, 0), ([0] * 16, -0x80000000, 0)], freq=15)
    yield tc_case(1, entries=[(0x1000, 1, 0, fb(float("nan"))), (0x1000, -1, 1, fb(float("inf"))), (0, 3, 0, fb(-0.0)),
                              (0x40002000, 4, 1, fb(1.0)), (0xFFFFFFFF, -1, 0, fb(1e-40))])


def refresh_random(r):
    return tc_random_case(r, 1)


def execute_edges():
    for settings in (0, 0x80, 0xFFFFFFFF, 0x7F, 0x100):
        for waiting in (-1, 0, 3):
            for system in (0, 0x40000000, 0x80000000, 0xC0000000, 0x3FFFFFFF):
                for generate in (1, 0):
                    yield tc_case(2, settings=settings, waiting=waiting, system=system, queues={"GeneratePages": [generate]})
    yield tc_case(2, waiting=0, queues={"GeneratePages": [-7]})
    yield tc_case(2, waiting=0, queues={"GeneratePages": [0x100000000 - 1]})
    yield tc_case(2, waiting=0, queues={"GeneratePages": [0x7FFFFFFF]})


def execute_random(r):
    return tc_random_case(r, 2)


# ----------------------------------------------------------------- the tests


def setup(runner):
    T.install_std_stubs(runner.emu)


TESTS = [FnTest("CHField::Prepare", hf_ppc, hf_host, edge=prepare_edges, rnd=prepare_random),
         FnTest("CHField::DivideAndConquer", hf_ppc, hf_host, edge=divide_edges, rnd=divide_random),
         FnTest("CGamut::GetVertices", hf_ppc, hf_host, edge=vertices_edges, rnd=vertices_random),
         FnTest("CTextureCache::Refresh", tc_ppc, tc_host, edge=refresh_edges, rnd=refresh_random),
         FnTest("CTextureCache::Execute", tc_ppc, tc_host, edge=execute_edges, rnd=execute_random)]
