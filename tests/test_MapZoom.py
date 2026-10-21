"""The map zoom, CMapZoom (src/engine/tb_mapzoom.cpp): all ten methods and the global's construction,
original versus translation.

A case is a script run on both sides: the zoom starts either constructed (the real
__sinit_tb_mapzoom_cpp on the PowerPC side, the constructor on the host, both after zeroing the
object as engine data is) or with every word set from the case, and then a list of operations
(Init, Close, the destructor, the filtering switches, Use, Window, Update, Prepare, Execute, a poke of
one word, and the setting of one texture entry). After every operation the test compares the result,
the zoom's 24 words in the Mac's layout (floats with NaNs as one value; the entry pointer as 0 or 1)
and every entry's flags, pitch, data pointer (as an offset from the picture), page and texture labels
and four floats. At the end it compares the event log, the heap traffic (blocks handled by new[] and
delete[]; the plain new and delete are not counted, the host's dump vector uses them) and the dump of the sixteen corner floats of every DrawSprite call.

Everything the zoom calls outside the unit is a fake on both sides that records the same event: the
texture entry's constructor, destructor, SetSize and GetBlendingMode, the texture cache's Remove and
Prepare, the draw primitive's DrawSprite, and the assertion. The entries are named by their index in
the zoom's array (the pointers are never followed by the fakes). The array itself is made by the
compiler's __nwa__FUl and __construct_new_array on the PowerPC side (stubbed with an eight-byte cookie in
front of the elements, as the runtime does) and by new[] on the host, whose constructor fake sets the
entry to 0xa5 bytes, so that what Init writes and what it leaves can be compared.

Two instructions Unicorn does not execute as the processor does are corrected for the code under test
(HardwareRounding below; FINDINGS 2026-10-08 and 2026-10-10): fmadds, which Unicorn rounds twice, and
fctiwz of a value above INT32_MAX, which it gives as 0x80000000 and the processor saturates.

Execute's sprite record has a flags word the original never sets (stack leftovers); the PowerPC
side clears the stack below the call before running it and the translation uses zero.

Not run (the arithmetic would differ between a 32-bit Mac and a 64-bit host, or the loop would not end):
tile indexes whose byte offset (index * 0x5c) overflows 32 bits, tile ranges whose last tile is
INT32_MAX (the loop counter wraps), and Execute over more than 32 tiles (the original's tables overrun).
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
SHIM_LIBRARY = "unit_MapZoom"
MASK = 0xffffffff

FN = dict(dtor=0x10019884, enable=0x10019904, disable=0x10019940, init=0x1001997c, close=0x10019afc,
          use=0x10019ba8, window=0x10019da8, update=0x10019de8, prepare=0x10019f94, execute=0x1001a0c0,
          sinit=0x1001a6dc)
CALL = dict(dtor=0, enable=1, disable=2, init=3, close=4, use=5, window=6, update=7, prepare=8, execute=9)
ENTRY_SET_SIZE, ENTRY_BLEND, CACHE_REMOVE, CACHE_PREPARE, DRAW_SPRITE = 0x10041560, 0x100410a4, 0x10034610, 0x10032ddc, 0x10007edc
CODE_RANGES = [(FN["prepare"], FN["prepare"] + 260), (FN["execute"], FN["execute"] + 1524)]

OBJ, OBJ_SIZE = 0x100afe20, 0x60
TCACHE_VAR, DRAW_VAR = 0x10173184, 0x10052b3c
DISPLAY, SYSTEM, SETTINGS = 0x10165634, 0x10165f80, 0x10165fc8
GUARDED = [(OBJ, OBJ_SIZE), (TCACHE_VAR, 4), (DRAW_VAR, 4), (DISPLAY, 4), (SYSTEM + 0x1c, 4), (SETTINGS + 4, 4)]
ENTRY_SIZE = 0x5c
MAX_ENTRIES = 1100            # entries the state dump holds
DATA_BASE = 0x20000000        # the picture's address on both sides (never dereferenced)
CACHE_ADDRESS = 0x7d100000
LABEL_ZOOM, LABEL_DRAW, LABEL_CACHE, LABEL_TEXTURE, LABEL_PAGE = 1, 2, 3, 10, 20
FLOAT_WORDS = list(range(8, 16)) + [17]       # the zoom's words that hold floats (index = offset / 4)
BYTES_PER_PIXEL = [2, 2, 2, 4, 2, 4]
FLAG_ON_PAGE, FLAG_SPRITE_4 = 0x1000, 0x100000
SETTABLE = [0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x40, 0x44,
            0x48, 0x4c, 0x50, 0x54, 0x58, 0x5c]


# ----------------------------------------------------------------- small helpers


def fb(x):
    """The bit pattern of a single-precision value."""
    return T.f32_bits(T.f32(x))


def bf(w):
    return T.bits_f32(w)


def canon(w):
    """Any NaN as one value."""
    return 0x7fc00000 if w & 0x7f800000 == 0x7f800000 and w & 0x007fffff else w & MASK


def ws(e, a, values):
    e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))


def rs(e, a, n):
    return list(struct.unpack(">" + "I" * n, e.read(a, n * 4)))


def words_bytes(words):
    """A word list as bytes for a quick whole-list comparison."""
    return struct.pack("<" + "I" * len(words), *[w & MASK for w in words])


def tile(x):
    """The tile of a float coordinate: (int)(x / 64), truncated toward zero (exact in single precision)."""
    return int(T.f32(x) / 64.0)


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
    T.install_std_stubs(runner.emu)


@contextlib.contextmanager
def guarded(e):
    """The engine globals the case overwrites, put back afterwards (other modules share them)."""
    saved = [(a, e.read(a, n)) for a, n in GUARDED]
    try:
        yield
    finally:
        for a, data in saved:
            e.write(a, data)


class PpcSide:
    """Memory, labels, fakes and the runner of one case on the PowerPC side."""

    def __init__(self, e, case):
        self.e, self.case = e, case
        self.built = 0                 # entries constructed and not destroyed yet
        self.arrays = {}               # first element -> element count
        self.pending = None            # the size operator new[] was asked for
        self.heap = [0, 0, 0, 0]       # new, new[], delete, delete[]
        self.dump = []
        self.draw = e.alloc(0x40, b"\xa5" * 0x40)
        fixed = {OBJ: LABEL_ZOOM, self.draw: LABEL_DRAW, CACHE_ADDRESS: LABEL_CACHE}
        self.texture = {0: 0}
        self.page = {0: 0}
        for i in range(1, 4):
            self.texture[i] = e.alloc(0x18, bytes(0x18))
            self.page[i] = e.alloc(0x20, bytes(0x20))
            fixed[self.texture[i]] = LABEL_TEXTURE + i
            fixed[self.page[i]] = LABEL_PAGE + i
        self.L = CL.PpcLog(fixed, case["q"])
        e.cl = self.L
        renderer, display_flags, fmt = case["g"]
        ws(e, DISPLAY, [display_flags])
        ws(e, SYSTEM + 0x1c, [renderer])
        ws(e, SETTINGS + 4, [fmt])
        ws(e, TCACHE_VAR, [CACHE_ADDRESS])
        ws(e, DRAW_VAR, [self.draw])
        for i, (width, height) in case["textures"].items():
            ws(e, self.texture[i] + 4, [width, height])
        for i, texture in case["pages"].items():
            ws(e, self.page[i], [self.texture[texture]])

    # -- the fakes
    def stubs(self):
        st = CL.Stubs()
        st.real = set(FN.values())
        st.internal(ENTRY_SET_SIZE, self.set_size)
        st.internal(ENTRY_BLEND, self.blending)
        st.internal(CACHE_REMOVE, self.remove)
        st.internal(CACHE_PREPARE, self.prepare)
        st.internal(DRAW_SPRITE, self.draw_sprite)
        st.imp("__nwa__FUl", self.new_array)
        st.imp("__construct_new_array", self.construct_array)
        st.imp("__destroy_new_array", self.destroy_array)
        st.imp("__register_global_object", lambda e: None)
        st.imp("__assertion_failed", lambda e: self.L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
        return st

    def index(self, entry):
        """An entry's index in the zoom's array; the second value is the remainder (0 for a real entry)."""
        distance = T.s32(entry - self.e.u32(OBJ + 0x1c))
        return distance // ENTRY_SIZE, distance % ENTRY_SIZE

    def new_array(self, e):
        size = e.r(3)
        self.pending = size
        self.heap[1] += 1
        e.set_r(3, e.alloc(size, b"\xa5" * size))

    def construct_array(self, e):
        raw, size, count = e.r(3), e.r(6), T.s32(e.r(7))
        if raw == 0:
            e.set_r(3, 0)
            return
        if size != ENTRY_SIZE or self.pending != count * ENTRY_SIZE + 8:
            self.L.rec("BadArray", (size, count, self.pending))
        for _ in range(count):
            self.L.rec("Entry.ctor", (self.built,))
            self.built += 1
        self.arrays[raw + 8] = count
        e.set_r(3, raw + 8)

    def destroy_array(self, e):
        first = e.r(3)
        for i in reversed(range(self.arrays.get(first, 0))):
            self.built -= 1
            self.L.rec("Entry.dtor", (self.built, 1 if e.u32(first + ENTRY_SIZE * i + 0x3c) else 0))
        self.heap[3] += 1

    def set_size(self, e):
        index, remainder = self.index(e.r(3))
        self.L.rec("Entry.SetSize", (index, remainder, T.s32(e.r(4)), T.s32(e.r(5))))

    def blending(self, e):
        index, remainder = self.index(e.r(3))
        self.L.rec("Entry.GetBlendingMode", (index, remainder))
        e.set_r(3, T.u32(self.L.ret("Entry.GetBlendingMode", 0)))

    def remove(self, e):
        index, remainder = self.index(e.r(4))
        self.L.rec("TCache.Remove", (self.L.lab(e.r(3)), index, remainder))
        e.set_r(3, T.u32(self.L.ret("TCache.Remove", 1)))

    def prepare(self, e):
        index, remainder = self.index(e.r(4))
        self.L.rec("TCache.Prepare", (self.L.lab(e.r(3)), index, remainder, CL.fbits(e.f(1))))

    def draw_sprite(self, e):
        L = self.L
        p = e.r(4)
        flags, vertices, f08, blend, texture, f14, u0, v0, u1, v1, f28 = rs(e, p, 11)
        L.rec("DrawSprite", (flags, T.s32(f08), blend, L.lab(texture), canon(u0), canon(v0), canon(u1), canon(v1),
                             T.s32(f28), T.s32(e.r(5))))
        self.dump.extend(canon(w) for w in rs(e, vertices, 16))
        e.set_r(3, T.u32(L.ret("DrawSprite", 1)))

    # -- the case's operations
    def call(self, name, *args):
        return T.call_ppc(self.e, FN[name], [OBJ, *args])[0]

    def start(self):
        e, start = self.e, self.case["start"]
        if start[0] == "ctor":
            e.write(OBJ, bytes(OBJ_SIZE))
            T.call_ppc(e, FN["sinit"], [])
        else:
            words = list(start[1])
            words[7] = 0
            ws(e, OBJ, words)

    def op(self, op):
        e = self.e
        name = op[0]
        F = lambda bits: T.F(bf(bits))
        if name == "nop":
            return 0
        if name == "poke":
            ws(e, OBJ + op[1], [op[2]])
            return 0
        if name == "entry":
            _, i, flags, page, texture, floats = op
            a = e.u32(OBJ + 0x1c) + ENTRY_SIZE * i
            ws(e, a + 8, [flags])
            ws(e, a + 0x14, [self.page[page]])
            ws(e, a + 0x18, [self.texture[texture]])
            ws(e, a + 0x1c, floats)
            return 0
        if name == "init":
            return T.s32(self.call("init", DATA_BASE, op[1], op[2], op[3]))
        if name == "close":
            return T.s32(self.call("close"))
        if name == "dtor":
            return T.s32(self.call("dtor", 0)) * 0
        if name == "on":
            self.call("enable")
            return 0
        if name == "off":
            self.call("disable")
            return 0
        if name in ("use", "window"):
            self.call(name, *[F(b) for b in op[1:5]])
            return 0
        if name == "update":
            self.call("update", *op[1:5])
            return 0
        if name == "prepare":
            return T.s32(self.call("prepare"))
        if name == "execute":
            # the stack below the call as a fresh one: the sprite's flags word is a local that is never set
            sp = ppcemu.STACK_BASE + ppcemu.STACK_SIZE - 0x1000 - 0x400
            e.write(sp - 0x800, bytes(0x800))
            return T.s32(self.call("execute"))
        raise ValueError(name)

    # -- the state after an operation
    def state(self):
        e = self.e
        words = rs(e, OBJ, 24)
        base = words[7]
        words[7] = 1 if base else 0
        for i in FLOAT_WORDS:
            words[i] = canon(words[i])
        entries = []
        if base:
            n = max(0, min(T.s32(words[22]), MAX_ENTRIES))
            raw = e.read(base, n * ENTRY_SIZE) if n else b""
            for i in range(n):
                r = raw[i * ENTRY_SIZE:(i + 1) * ENTRY_SIZE]
                flags = struct.unpack(">I", r[8:12])[0]
                pitch = struct.unpack(">H", r[0x30:0x32])[0]
                data = struct.unpack(">I", r[0x3c:0x40])[0]
                page, texture = struct.unpack(">II", r[0x14:0x1c])
                floats = struct.unpack(">IIII", r[0x1c:0x2c])
                entries.extend([flags, pitch, 0x80000001 if data == 0 else (data - DATA_BASE) & MASK,
                                self.L.lab(page) & MASK, self.L.lab(texture) & MASK, *[canon(f) for f in floats]])
        return words_bytes(words), words_bytes(entries)


def ppc(e, case):
    with guarded(e):
        side = PpcSide(e, case)
        stubs = side.stubs()
        saved = stubs.activate(e)
        try:
            side.start()
            steps = []
            with HardwareRounding(e):
                for op in case["ops"]:
                    result = side.op(op)
                    words, entries = side.state()
                    steps.append({"ret": result, "words": words, "entries": entries})
            return {"steps": steps, "log": side.L.log, "heap": [side.heap[1], side.heap[3]], "dump": words_bytes(side.dump)}
        finally:
            stubs.deactivate(e, saved)


# ----------------------------------------------------------------- the host side


def u32_array(words):
    return (C.c_uint32 * max(len(words), 1))(*[w & MASK for w in words])


def host(lib, case):
    lib.tpw_mz_pointer.restype = C.c_void_p
    lib.tpw_mz_pointer.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_mz_world.argtypes = [C.c_int32, C.c_uint32, C.c_int32]
    lib.tpw_mz_texture.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_mz_page.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_mz_set.argtypes = [C.c_int32, C.c_uint32]
    lib.tpw_mz_start.argtypes = [C.c_int32, C.POINTER(C.c_uint32)]
    lib.tpw_mz_words.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_mz_entry_set.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.POINTER(C.c_uint32)]
    lib.tpw_mz_entries.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_mz_entries.restype = C.c_int32
    lib.tpw_mz_call.argtypes = [C.c_int32, C.POINTER(C.c_uint32)]
    lib.tpw_mz_call.restype = C.c_int32
    lib.tpw_mz_dump_n.restype = C.c_uint32
    lib.tpw_mz_dump_copy.argtypes = [C.POINTER(C.c_uint32)]
    renderer, display_flags, fmt = case["g"]
    lib.tpw_mz_world(renderer, display_flags, fmt)
    fixed = {LABEL_ZOOM: lib.tpw_mz_pointer(0, 0), LABEL_DRAW: lib.tpw_mz_pointer(1, 0),
             LABEL_CACHE: lib.tpw_mz_pointer(2, 0)}
    for i in range(1, 4):
        fixed[LABEL_TEXTURE + i] = lib.tpw_mz_pointer(3, i)
        fixed[LABEL_PAGE + i] = lib.tpw_mz_pointer(4, i)
    for i, (width, height) in case["textures"].items():
        lib.tpw_mz_texture(i, width, height)
    for i, texture in case["pages"].items():
        lib.tpw_mz_page(i, texture)
    start = case["start"]
    lib.tpw_mz_start(0 if start[0] == "ctor" else 1, u32_array([] if start[0] == "ctor" else start[1]))
    heap = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
    for k in range(4):
        heap[k] = 0
    CL.host_begin(lib, fixed, case["q"])
    steps = []
    for op in case["ops"]:
        name = op[0]
        result = 0
        if name == "nop":
            pass
        elif name == "poke":
            lib.tpw_mz_set(op[1], op[2] & MASK)
        elif name == "entry":
            _, i, flags, page, texture, floats = op
            lib.tpw_mz_entry_set(i, flags, page, texture, u32_array(floats))
        else:
            args = {"init": op[1:4], "use": op[1:5], "window": op[1:5], "update": op[1:5]}.get(name, [])
            result = lib.tpw_mz_call(CALL[name if name not in ("on", "off") else "enable" if name == "on" else "disable"],
                                     u32_array(args))
            if name in ("dtor", "on", "off", "use", "window", "update"):
                result = 0
        words = (C.c_uint32 * 24)()
        lib.tpw_mz_words(words)
        words = list(words)
        for i in FLOAT_WORDS:
            words[i] = canon(words[i])
        buffer = (C.c_uint32 * (9 * MAX_ENTRIES))()
        n = lib.tpw_mz_entries(buffer)
        steps.append({"ret": result, "words": words_bytes(words), "entries": words_bytes(list(buffer)[:9 * n])})
    log = CL.host_end(lib)
    n = lib.tpw_mz_dump_n()
    dump = (C.c_uint32 * max(n, 1))()
    lib.tpw_mz_dump_copy(dump)
    return {"steps": steps, "log": log, "heap": [heap[1], heap[3]], "dump": words_bytes(list(dump)[:n])}


# ----------------------------------------------------------------- cases
#
# A case: g (renderer, display flags, texture format), q (the queues the fakes answer from), textures
# (label -> width, height), pages (page label -> texture label), start ("ctor",) or ("words", 24 words)
# and ops (see the module comment; the entry operation is ("entry", index, flags, page, texture,
# [U size, V size, U offset, V offset] as float bits)).

FLT_MAX = 3.4028234663852886e38
SPECIAL = [0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 63.9, 64.0, -64.0, 1e-40, 1e10, FLT_MAX, -FLT_MAX, math.inf, -math.inf,
           math.nan, 16777216.0, 16777217.0, 123456.789]
SIZES = [1, 2, 63, 64, 65, 100, 127, 128, 129, 200, 255, 256, 257, 300, 512, 640, 1000, 1023, 1024]


def rfl(r, scale=10.0, special=0.03):
    """A random float's bits: moderate, or now and then an edge value."""
    if r.random() < special:
        return fb(r.choice(SPECIAL))
    return fb(r.uniform(-scale, scale))


def case(seed=0, renderer=3, display=0, fmt=0, q=None, textures=None, pages=None, start=("ctor",), ops=None):
    return {"g": [renderer, display, fmt], "q": q or {}, "start": start, "ops": list(ops or []),
            "textures": textures or {1: (256, 256), 2: (128, 64), 3: (512, 1024)},
            "pages": pages or {1: 1, 2: 2, 3: 3}}


def random_words(r):
    """Twenty-four words that read as ordinary values (the entry pointer, word 7, is cleared)."""
    words = [r.getrandbits(32) for _ in range(24)]
    for i in FLOAT_WORDS:
        words[i] = rfl(r, 1000.0, 0.1)
    words[7] = 0
    return words


def distinct_words(base):
    """A word pattern that tells every field apart."""
    words = [(base + 0x01010101 * i) & MASK for i in range(24)]
    words[7] = 0
    return words


def pitch_for(r, width, fmt):
    bpp = BYTES_PER_PIXEL[fmt]
    k = r.random()
    if k < 0.5:
        return width * bpp
    if k < 0.75:
        return (width + r.randrange(0, 70)) * bpp
    if k < 0.85:
        return r.randrange(0, 5000)
    if k < 0.9:
        return -r.randrange(0, 600) * bpp
    return r.randrange(0, 1 << 22)


def random_entry(r, index, p_page=0.7):
    flags = r.getrandbits(32) & ~(FLAG_ON_PAGE | FLAG_SPRITE_4)
    if r.random() < p_page:
        flags |= FLAG_ON_PAGE
    if r.random() < 0.4:
        flags |= FLAG_SPRITE_4
    return ("entry", index, flags, r.choice([0, 1, 2, 3, 1, 2, 3]), r.choice([0, 1, 2, 3, 1, 2, 3]),
            [rfl(r, 2.0, 0.04) for _ in range(4)])


def random_textures(r):
    def size():
        return r.choice([r.choice([0, 1, 8, 16, 32, 64, 128, 256, 512, 1024]), r.randrange(-4, 2049)])
    textures = {i: (size(), size()) for i in range(1, 4)}
    pages = {i: r.choice([0, 1, 2, 3, i, i]) for i in range(1, 4)}
    return textures, pages


def grid_dims(r, max_tiles=32):
    w = r.choice(SIZES + [r.randrange(1, 64 * max_tiles)])
    h = r.choice(SIZES + [r.randrange(1, 64 * max_tiles)])
    return min(w, 64 * max_tiles), min(h, 64 * max_tiles)


def grid(w, h):
    return (w + 63) // 64, (h + 63) // 64


def random_start(r):
    if r.random() < 0.6:
        return ("ctor",)
    return ("words", random_words(r) if r.random() < 0.6 else distinct_words(r.getrandbits(32)))


# ---- the construction, Init, Close, the destructor, the filtering switches


def rnd_ctor(r):
    ops = [("nop",)]
    for _ in range(r.randrange(0, 4)):
        k = r.random()
        if k < 0.3:
            ops.append(("poke", r.choice(SETTABLE), r.getrandbits(32)))
        elif k < 0.6:
            ops.append((r.choice(["on", "off"]),))
        else:
            ops.append(("window", *[rfl(r, 800.0, 0.1) for _ in range(4)]))
    return case(ops=ops)


def edges_ctor():
    return [case(ops=[("nop",)]), case(ops=[("nop",), ("on",), ("nop",)]), case(ops=[("nop",), ("off",)]),
            case(ops=[("init", 64, 64, 128), ("close",)])]


def rnd_init(r):
    w, h = grid_dims(r)
    fmt = r.choice([0, 1, 2, 3, 4, 5, 3, 5])
    ops = [("init", w, h, pitch_for(r, w, fmt))]
    k = r.random()
    if k < 0.2:
        ops.append(("init", *grid_dims(r), pitch_for(r, w, fmt)))        # over the first: it leaks, the second wins
    elif k < 0.35:
        ops.append(("close",))
    elif k < 0.45:
        ops.append(("dtor",))
    return case(start=random_start(r), fmt=fmt, ops=ops)


def edges_init():
    cases = []
    for fmt in range(6):
        for w, h in [(1, 1), (63, 63), (64, 64), (65, 65), (64, 1), (1, 64), (65, 129), (640, 480), (2048, 2048),
                     (0, 0), (0, 70), (70, 0), (1000, 3), (3, 1000), (2047, 2047)]:
            bpp = BYTES_PER_PIXEL[fmt]
            cases.append(case(fmt=fmt, ops=[("init", w, h, w * bpp)]))
    for pitch in (0, 1, 2, 3, 4, 5, 7, -2, -4, -6, 130, 1 << 20, (1 << 24) + 6, 0x7fffffff, -0x80000000):
        for fmt in (0, 3, 4):
            cases.append(case(fmt=fmt, ops=[("init", 200, 200, pitch)]))
    # pitch / bytes per pixel with a remainder, and one that does not fit the 16-bit field
    for pitch in (131070, 131072, 131074, 65535 * 2, 65536 * 2 + 2, 65537 * 4, 70000 * 4):
        cases.append(case(fmt=3, ops=[("init", 64, 64, pitch)]))
        cases.append(case(fmt=0, ops=[("init", 64, 64, pitch)]))
    # the sizes around the tile boundaries, one axis at a time
    for n in (1, 2, 62, 63, 64, 65, 66, 127, 128, 129, 191, 192, 193):
        cases.append(case(ops=[("init", n, 64, n * 2)]))
        cases.append(case(ops=[("init", 64, n, 128)]))
    # Init over a zoom that has entries (the array leaks), after a Close, and twice with different grids
    cases.append(case(ops=[("init", 128, 128, 256), ("init", 64, 64, 128)]))
    cases.append(case(ops=[("init", 128, 128, 256), ("close",), ("init", 192, 64, 384)]))
    cases.append(case(start=("words", distinct_words(0x10203040)), ops=[("init", 100, 100, 200), ("close",)]))
    return cases


def rnd_close(r):
    w, h = grid_dims(r, 12)
    fmt = r.randrange(6)
    kind = r.random()
    ops = []
    if kind < 0.15:
        pass                                                    # nothing to free
    else:
        ops.append(("init", w, h, pitch_for(r, w, fmt)))
        if kind > 0.8:
            w, h = grid_dims(r, 6)
            ops.append(("init", w, h, pitch_for(r, w, fmt)))
        if r.random() < 0.3:
            ops.append(("poke", 0x58, r.randrange(0, (w + 63) // 64 * ((h + 63) // 64) + 1)))   # fewer than the array holds
    ops.append(("close",))
    if r.random() < 0.3:
        ops.append(("close",))
    if r.random() < 0.2:
        ops.append(("dtor",))
    return case(start=random_start(r), fmt=fmt, ops=ops)


def edges_close():
    cases = [case(ops=[("close",)]), case(ops=[("close",), ("close",)]),
             case(start=("words", distinct_words(0x01020304)), ops=[("close",)]),
             case(start=("words", [0xffffffff] * 24), ops=[("close",)])]
    for w, h in [(1, 1), (64, 64), (65, 65), (640, 128), (256, 256), (192, 128)]:
        cases.append(case(ops=[("init", w, h, w * 2), ("close",)]))
        cases.append(case(ops=[("init", w, h, w * 2), ("close",), ("close",)]))
    n = 4 * 3
    for count in (0, 1, 5, n - 1, n):
        cases.append(case(ops=[("init", 256, 192, 512), ("poke", 0x58, count), ("close",)]))
    cases.append(case(ops=[("init", 128, 128, 256), ("init", 64, 64, 128), ("close",)]))
    return cases


def rnd_dtor(r):
    c = rnd_close(r)
    c["ops"] = [op for op in c["ops"] if op[0] != "close"] + [("dtor",)]
    if r.random() < 0.3:
        c["ops"].append(("dtor",))
    return c


def edges_dtor():
    cases = [case(ops=[("dtor",)]), case(ops=[("dtor",), ("dtor",)])]
    for w, h in [(1, 1), (64, 64), (130, 70), (640, 480)]:
        cases.append(case(ops=[("init", w, h, w * 2), ("dtor",)]))
        cases.append(case(ops=[("init", w, h, w * 2), ("dtor",), ("dtor",)]))
        cases.append(case(ops=[("init", w, h, w * 2), ("dtor",), ("init", w, h, w * 2)]))
    return cases


def rnd_filter(which):
    def make(r):
        ops = [(r.choice(["on", "off"]),) for _ in range(r.randrange(0, 5))]
        ops.insert(r.randrange(0, len(ops) + 1), (which,))
        return case(start=random_start(r), ops=ops)
    return make


def edges_filter(which):
    other = "off" if which == "on" else "on"
    return lambda: [case(ops=[(which,)]), case(ops=[(which,), (which,)]), case(ops=[(other,), (which,)]),
                    case(ops=[(which,), (other,)]),
                    case(start=("words", distinct_words(0x55)), ops=[(which,)]),
                    case(start=("words", [0xffffffff] * 24), ops=[(which,)]),
                    case(start=("words", [0] * 24), ops=[(which,)])]


# ---- Use and Window


def coord(r, limit, special=0.12):
    """A coordinate near the range 0..limit, now and then outside it or an edge value."""
    k = r.random()
    if k < special:
        return fb(r.choice(SPECIAL + [limit, limit - 1, limit + 1, -1, 0, limit - 0.5, limit + 0.5, limit - 1e-5]))
    if k < 0.2:
        return fb(r.uniform(-50, limit + 50))
    return fb(r.uniform(0, limit))


def rnd_use(r):
    w, h = grid_dims(r, 12)
    ops = [] if r.random() < 0.2 else [("init", w, h, w * 2)]
    start = ("ctor",) if ops else ("words", random_words(r))
    if not ops:
        w = struct.unpack(">i", struct.pack(">I", start[1][1]))[0]
        h = struct.unpack(">i", struct.pack(">I", start[1][2]))[0]
        if r.random() < 0.7:
            w, h = grid_dims(r, 12)
            ops.append(("poke", 0x04, w))
            ops.append(("poke", 0x08, h))
    for _ in range(r.randrange(1, 4)):
        ops.append(("use", coord(r, w), coord(r, h), coord(r, w), coord(r, h)))
    return case(start=start, ops=ops)


def edges_use():
    cases = []
    w, h = 200, 100

    def base(*uses):
        return case(ops=[("init", w, h, w * 2), *[("use", *[fb(v) for v in u]) for u in uses]])
    valid = (10.0, 20.0, 150.0, 80.0)
    cases.append(base(valid))
    # each of the eight comparisons at its edge (0, below 0, width/height, just below, above), the others valid
    for x0 in (-1.0, -0.0, 0.0, 199.99, 200.0, 201.0):
        cases.append(base((x0, 20.0, 150.0, 80.0)))
    for x1 in (-1.0, 0.0, 199.99, 200.0, 201.0):
        cases.append(base((10.0, 20.0, x1, 80.0)))
    for y0 in (-1.0, 0.0, 99.99, 100.0, 101.0):
        cases.append(base((10.0, y0, 150.0, 80.0)))
    for y1 in (-1.0, 0.0, 99.99, 100.0, 101.0):
        cases.append(base((10.0, 20.0, 150.0, y1)))
    # the || of the text: the x tests and y0 >= 0 fail, the y tests pass or fail
    for u in [(-5.0, 20.0, 150.0, 80.0), (10.0, 20.0, 250.0, 80.0), (10.0, -3.0, 150.0, 80.0),
              (-5.0, 120.0, 150.0, 80.0), (-5.0, 20.0, 150.0, 180.0), (-5.0, 20.0, 150.0, -1.0),
              (10.0, 100.0, 150.0, 80.0), (10.0, 100.0, 150.0, 100.0), (10.0, 20.0, 150.0, 100.0),
              (210.0, 99.0, 150.0, 99.0), (210.0, 99.0, 150.0, 100.0), (210.0, 99.0, 150.0, -0.5),
              (210.0, 100.0, 150.0, 50.0), (210.0, -1.0, 150.0, 50.0), (-1.0, -1.0, -1.0, -1.0),
              (300.0, 300.0, 300.0, 300.0), (0.0, 0.0, 0.0, 0.0)]:
        cases.append(base(u))
    # each term of the first group at its edge with the second group false (y1 above the height), and each
    # term of the second group at its edge with the first group false (x0 below 0)
    for x0 in (-0.01, -0.0, 0.0, 199.99, 200.0, 200.01):
        cases.append(base((x0, 20.0, 150.0, 150.0)))
    for x1 in (-0.01, -0.0, 0.0, 199.99, 200.0, 200.01):
        cases.append(base((10.0, 20.0, x1, 150.0)))
    for y0 in (-0.01, -0.0, 0.0, 0.01):
        cases.append(base((10.0, y0, 150.0, 150.0)))
    for y0 in (99.99, 100.0, 100.01):
        cases.append(base((-1.0, y0, 150.0, 50.0)))
    for y1 in (-0.01, -0.0, 0.0, 99.99, 100.0, 100.01):
        cases.append(base((-1.0, 50.0, 150.0, y1)))
    # the stored order: four distinct values
    cases.append(base((1.0, 2.0, 3.0, 4.0)))
    cases.append(base((5.0, 6.0, 7.0, 8.0), (9.0, 10.0, 11.0, 12.0)))
    # NaN and infinity in each position (every comparison false)
    for pos in range(4):
        for bad in (math.nan, math.inf, -math.inf):
            u = [10.0, 20.0, 150.0, 80.0]
            u[pos] = bad
            cases.append(base(tuple(u)))
    # the size is converted to float: a huge width (above 2^24, rounding) and a negative one
    for width, x in [(16777217, 16777216.0), (16777217, 16777217.0), (16777219, 16777220.0), (0x7fffffff, 2147483648.0),
                     (-5, 0.0), (0, 0.0), (1, 0.0), (1, 1.0)]:
        cases.append(case(start=("words", distinct_words(0x100)),
                          ops=[("poke", 0x04, width), ("poke", 0x08, 100),
                               ("use", fb(x), fb(1.0), fb(x), fb(1.0))]))
    cases.append(case(start=("words", distinct_words(0x100)), ops=[("use", fb(1.0), fb(2.0), fb(3.0), fb(4.0))]))
    cases.append(case(start=("words", [0xffffffff] * 24), ops=[("use", fb(1.0), fb(2.0), fb(3.0), fb(4.0))]))
    return cases


def rnd_window(r):
    ops = [("window", *[rfl(r, 800.0, 0.1) for _ in range(4)]) for _ in range(r.randrange(1, 4))]
    return case(start=random_start(r), ops=ops)


def edges_window():
    cases = [case(ops=[("window", fb(1.0), fb(2.0), fb(3.0), fb(4.0))]),
             case(start=("words", distinct_words(0x777)), ops=[("window", fb(1.0), fb(2.0), fb(3.0), fb(4.0))]),
             case(start=("words", distinct_words(0x777)),
                  ops=[("window", fb(5.0), fb(6.0), fb(7.0), fb(8.0)), ("window", fb(-1.0), fb(-2.0), fb(-3.0), fb(-4.0))])]
    for v in SPECIAL:
        cases.append(case(ops=[("window", fb(v), fb(1.0), fb(2.0), fb(3.0))]))
        cases.append(case(ops=[("window", fb(1.0), fb(v), fb(2.0), fb(3.0))]))
        cases.append(case(ops=[("window", fb(1.0), fb(2.0), fb(v), fb(3.0))]))
        cases.append(case(ops=[("window", fb(1.0), fb(2.0), fb(3.0), fb(v))]))
    return cases


# ---- Update


def update_ints(r, w, h):
    def inside(limit):
        k = r.random()
        if k < 0.1:
            return r.choice([-1, -64, -65, -63, 0, limit - 1, limit, limit + 1, limit + 64, 2 * limit])
        if k < 0.15:
            return r.randrange(-200, 3 * limit + 200)
        return r.randrange(0, limit)
    return inside(w), inside(h), inside(w), inside(h)


def rnd_update(r):
    w, h = grid_dims(r, 10)
    fmt = r.randrange(6)
    ops = [("init", w, h, pitch_for(r, w, fmt))]
    if r.random() < 0.35:
        ops.append(("poke", 0x48, r.randrange(-40, 40)))
    for _ in range(r.randrange(1, 4)):
        x0, y0, x1, y1 = update_ints(r, w, h)
        # keep the loops short
        x1 = min(x1, x0 + 64 * 6) if x1 >= x0 else x1
        y1 = min(y1, y0 + 64 * 6) if y1 >= y0 else y1
        ops.append(("update", x0, y0, x1, y1))
    return case(fmt=fmt, ops=ops)


def edges_update():
    cases = []
    w, h = 300, 200       # a 5 by 4 grid
    base = [("init", w, h, w * 2)]
    # the eight comparisons at their edges
    valid = [10, 20, 100, 90]
    for pos, limit in [(0, w), (2, w), (1, h), (3, h)]:
        for v in (-65, -64, -1, 0, limit - 1, limit, limit + 1, limit + 64):
            args = list(valid)
            args[pos] = v
            cases.append(case(ops=base + [("update", *args)]))
    # division by 64 of negative numbers and the tile edges
    for x0, x1 in [(0, 0), (63, 64), (64, 64), (63, 63), (64, 127), (64, 128), (0, 299), (-1, 0), (-63, 0), (-64, 0),
                   (-65, 63), (-128, -1), (-1, -1), (130, 70), (299, 299)]:
        for y0, y1 in [(0, 0), (63, 64), (0, 199), (64, 64), (-1, 1), (100, 30)]:
            cases.append(case(ops=base + [("update", x0, y0, x1, y1)]))
    # the first-entry offset, positive and negative
    for first in (-3, -1, 1, 2, 7, 100):
        cases.append(case(ops=base + [("poke", 0x48, first), ("update", 10, 20, 150, 100)]))
    # no entries at all (never Init'd, columns from the case), and after a Close
    cases.append(case(start=("words", distinct_words(0)),
                      ops=[("poke", 0x04, 300), ("poke", 0x08, 200), ("poke", 0x10, 5), ("poke", 0x48, 0), ("update", 10, 20, 150, 100)]))
    cases.append(case(ops=base + [("close",), ("update", 10, 20, 150, 100)]))
    # 1 by 1 and a grid with one column
    cases.append(case(ops=[("init", 1, 1, 2), ("update", 0, 0, 0, 0)]))
    cases.append(case(ops=[("init", 64, 640, 128), ("update", 0, 0, 63, 639)]))
    cases.append(case(ops=[("init", 640, 64, 1280), ("update", 0, 0, 639, 63)]))
    # the whole 32 by 32 grid
    cases.append(case(ops=[("init", 2048, 2048, 4096), ("update", 0, 0, 2047, 2047)]))
    return cases


# ---- Prepare and Execute


def use_rect(r, cols, rows, empty_ok=True):
    """A Use rectangle inside the grid's tiles (so that every tile named exists), as float bits plus the tiles."""
    cw, ch = cols * 64, rows * 64
    k = r.random()

    def one(limit):
        t = r.random()
        if t < 0.1:
            return min(r.choice([0.0, -0.0, -30.5, limit - 0.5, limit - 1.0, 64.0, 63.999, 128.0, limit / 2.0]), limit - 0.01)
        if t < 0.2:
            return min(r.randrange(0, limit // 64 + 1) * 64.0, limit - 0.01)
        return r.uniform(0, limit - 0.01) if r.random() < 0.9 else r.uniform(-60, limit - 0.01)
    x0, x1, y0, y1 = one(cw), one(cw), one(ch), one(ch)
    if k < 0.8 or not empty_ok:
        x0, x1 = min(x0, x1), max(x0, x1)
        y0, y1 = min(y0, y1), max(y0, y1)
        if x1 == x0 and r.random() < 0.9:
            x1 = min(x0 + r.choice([0.5, 1.0, 7.25]), cw - 0.01)
        if y1 == y0 and r.random() < 0.9:
            y1 = min(y0 + r.choice([0.5, 1.0, 7.25]), ch - 0.01)
    return [fb(x0), fb(y0), fb(x1), fb(y1)]


def tile_box(use):
    x0, y0, x1, y1 = [bf(b) for b in use]
    return tile(x0), tile(y0), tile(x1), tile(y1)


def first_entry(r, cols, rows, use):
    """A first-entry offset that keeps every tile of the rectangle inside the array (or 0)."""
    tx0, ty0, tx1, ty1 = tile_box(use)
    if tx1 < tx0 or ty1 < ty0:
        return 0
    low = tx0 + ty0 * cols
    high = tx1 + ty1 * cols
    lo, hi = -low, cols * rows - 1 - high
    return r.randint(lo, hi) if lo <= hi and r.random() < 0.4 else 0


def sprite_ops(r, cols, rows, use, p_page=0.7):
    return [random_entry(r, i, p_page) for i in range(cols * rows)]


def draw_queue(r, tiles):
    if r.random() < 0.8:
        return [1] * 64
    k = r.randrange(0, max(tiles, 1) + 1)
    return [r.choice([1, 1, 5, -1]) for _ in range(k)] + [0, 1, 1]


def window_bits(r):
    x0 = r.uniform(-100, 700)
    y0 = r.uniform(-100, 500)
    return [fb(x0), fb(y0), fb(x0 + r.uniform(-300, 700)), fb(y0 + r.uniform(-300, 500))]


def rnd_prepare(r):
    cols, rows = r.randint(1, 8), r.randint(1, 8)
    w, h = cols * 64 - r.choice([0, 0, 1, 20, 63]), rows * 64 - r.choice([0, 0, 1, 20, 63])
    fmt = r.randrange(6)
    use = use_rect(r, cols, rows)
    ops = [("init", max(w, 1), max(h, 1), max(w, 1) * BYTES_PER_PIXEL[fmt]), ("use", *use)]
    if r.random() < 0.4:
        ops.append(("poke", 0x48, first_entry(r, cols, rows, use) if r.random() < 0.7 else r.randrange(-30, 30)))
    ops.append(("prepare",))
    if r.random() < 0.3:
        ops.append(("use", *use_rect(r, cols, rows)))
        ops.append(("prepare",))
    if r.random() < 0.1:
        ops.insert(0, ("prepare",))
    return case(fmt=fmt, ops=ops)


def edges_prepare():
    cases = [case(ops=[("prepare",)]), case(start=("words", distinct_words(7)), ops=[("prepare",)]),
             case(ops=[("init", 128, 128, 256), ("close",), ("prepare",)])]
    base = [("init", 640, 512, 1280)]      # 10 by 8 tiles
    for x0, x1 in [(0.0, 0.0), (0.0, 63.99), (0.0, 64.0), (63.99, 64.0), (64.0, 64.0), (-1.0, 1.0), (-63.0, 70.0),
                   (100.5, 400.25), (639.0, 639.9), (0.0, 639.99), (200.0, 100.0), (-0.5, -0.25)]:
        for y0, y1 in [(0.0, 0.0), (0.0, 64.0), (10.5, 300.0), (511.0, 511.9), (0.0, 511.99), (300.0, 100.0)]:
            cases.append(case(ops=base + [("use", fb(x0), fb(y0), fb(x1), fb(y1)), ("prepare",)]))
    for first in (-5, -1, 1, 9, 20, 40):
        cases.append(case(ops=base + [("use", fb(70.0), fb(70.0), fb(200.0), fb(200.0)), ("poke", 0x48, first), ("prepare",)]))
    # the full 32 by 32 grid, a 1 by 1 grid, a one-column and a one-row grid
    cases.append(case(ops=[("init", 2048, 2048, 4096), ("use", fb(0.0), fb(0.0), fb(2047.9), fb(2047.9)), ("prepare",)]))
    cases.append(case(ops=[("init", 1, 1, 2), ("use", fb(0.0), fb(0.0), fb(0.5), fb(0.5)), ("prepare",)]))
    cases.append(case(ops=[("init", 64, 640, 128), ("use", fb(0.0), fb(0.0), fb(63.0), fb(639.0)), ("prepare",)]))
    return cases


def rnd_execute(r):
    cols, rows = r.randint(1, 8), r.randint(1, 8)
    if r.random() < 0.15:
        cols, rows = r.randint(1, 32), r.randint(1, 3)
    w, h = max(cols * 64 - r.choice([0, 0, 1, 20, 63]), 1), max(rows * 64 - r.choice([0, 0, 1, 20, 63]), 1)
    fmt = r.randrange(6)
    use = use_rect(r, cols, rows)
    tiles = (tile_box(use)[2] - tile_box(use)[0] + 1) * (tile_box(use)[3] - tile_box(use)[1] + 1)
    textures, pages = random_textures(r)
    renderer = 4 if r.random() < 0.3 else r.choice([3, 3, 0, 1, 2, 5, -1])
    display = r.getrandbits(32) & ~2 | (2 if r.random() < 0.5 else 0)
    ops = [("init", w, h, w * BYTES_PER_PIXEL[fmt])]
    ops += sprite_ops(r, cols, rows, use, p_page=r.choice([0.9, 0.5, 0.1]))
    ops.append(("window", *window_bits(r)))
    ops.append(("use", *use))
    ops.append(("poke", 0x48, first_entry(r, cols, rows, use)))
    if r.random() < 0.6:
        ops.append(("poke", 0x40, r.choice([0xffffffff, 0xff000000, 0xfeffffff, 0x80ffffff, 0, 0xff123456,
                                            r.getrandbits(32)])))
    if r.random() < 0.7:
        ops.append(("poke", 0x44, rfl(r, 3.0, 0.1)))
    if r.random() < 0.4:
        ops.append((r.choice(["on", "off"]),))
    ops.append(("execute",))
    if r.random() < 0.3:
        ops.append(("execute",))
    if r.random() < 0.15:
        use = use_rect(r, cols, rows)
        ops.append(("use", *use))
        ops.append(("poke", 0x48, first_entry(r, cols, rows, use)))
        ops.append(("window", *window_bits(r)))
        ops.append(("execute",))
    q = {"Entry.GetBlendingMode": [r.getrandbits(32) for _ in range(8)], "DrawSprite": draw_queue(r, tiles)}
    return case(renderer=renderer, display=display, fmt=fmt, q=q, textures=textures, pages=pages, ops=ops)


def execute_base(cols=4, rows=3, entries=None, **kw):
    """An initialised zoom of cols by rows tiles with every entry on a page of texture 1 (256 by 128)."""
    w, h = cols * 64, rows * 64
    ops = [("init", w, h, w * 2)]
    for i in range(cols * rows):
        ops.append(("entry", i, FLAG_ON_PAGE | (entries or {}).get(i, 0), 1, 1,
                    [fb(0.25 + i / 64.0), fb(0.5 + i / 128.0), fb(0.125 * i), fb(0.0625 * i)]))
    return case(textures={1: (256, 128), 2: (64, 64), 3: (32, 512)}, pages={1: 1, 2: 2, 3: 3}, ops=ops, **kw)


def edges_execute():
    cases = [case(ops=[("execute",)]), case(start=("words", distinct_words(9)), ops=[("execute",)]),
             case(ops=[("init", 128, 128, 256), ("close",), ("execute",)])]

    def run(c, use, window=(10.0, 20.0, 330.0, 260.0), extra=()):
        c["ops"] += [("window", *[fb(v) for v in window]), ("use", *[fb(v) for v in use]), *extra, ("execute",)]
        return c
    rects = [(0.0, 0.0, 255.99, 191.99), (0.0, 0.0, 63.0, 63.0), (0.0, 0.0, 64.0, 64.0), (10.5, 20.25, 200.75, 150.5),
             (64.0, 64.0, 128.0, 128.0), (70.0, 70.0, 80.0, 80.0), (63.5, 63.5, 64.5, 64.5), (0.0, 0.0, 255.9, 191.9),
             (100.0, 100.0, 100.0, 100.0), (200.0, 150.0, 20.0, 10.0), (-30.0, -30.0, 30.0, 30.0), (0.0, 64.0, 255.5, 64.0),
             (130.0, 5.0, 131.0, 190.0), (5.0, 130.0, 250.0, 131.0)]
    for use in rects:
        cases.append(run(execute_base(), use))
    # the tables' limit: the full width of a 32 tile grid, and 32 rows
    cases.append(run(execute_base(32, 1), (0.0, 0.0, 2047.9, 63.9), (0.0, 0.0, 640.0, 64.0)))
    cases.append(run(execute_base(1, 32), (0.0, 0.0, 63.9, 2047.9), (0.0, 0.0, 64.0, 640.0)))
    cases.append(run(execute_base(32, 2), (3.0, 3.0, 2040.5, 120.5), (0.0, 0.0, 640.0, 480.0)))
    # the colour's top byte, the filtering switch, the z and the renderer
    for colour in (0xffffffff, 0xff000000, 0xfeffffff, 0x00ffffff, 0x80000000, 0xff, 0):
        for filtering in ("on", "off"):
            c = execute_base()
            run(c, (10.0, 10.0, 100.0, 100.0), extra=[("poke", 0x40, colour), (filtering,)])
            cases.append(c)
    for renderer in (4, 3, 0, 5):
        for z in (1.0, 0.0, -2.5, 1e30, math.nan):
            c = execute_base(renderer=renderer)
            run(c, (10.0, 10.0, 100.0, 100.0), extra=[("poke", 0x44, fb(z))])
            cases.append(c)
    # the entry's flags: on a page or not, the sprite flag 4, both
    for flags in (0, FLAG_ON_PAGE, FLAG_SPRITE_4, FLAG_ON_PAGE | FLAG_SPRITE_4, 0xffffffff, 0xffffffff & ~FLAG_SPRITE_4):
        c = execute_base()
        c["ops"] = [op if op[0] != "entry" else ("entry", op[1], flags, *op[3:]) for op in c["ops"]]
        cases.append(run(c, (10.0, 10.0, 200.0, 150.0)))
    # the sprite flag stays once set: only the second tile has it
    c = execute_base(entries={1: FLAG_SPRITE_4})
    cases.append(run(c, (0.0, 0.0, 255.5, 64.0)))
    c = execute_base(entries={0: FLAG_SPRITE_4})
    cases.append(run(c, (0.0, 0.0, 255.5, 64.0)))
    # the page inset: with and without a page, with and without a texture on it, the display flag 2,
    # texture sizes of zero and negative
    for page, texture, size, display in [(0, 0, (256, 128), 0), (1, 1, (256, 128), 0), (1, 1, (256, 128), 2),
                                         (2, 0, (256, 128), 0), (1, 1, (0, 0), 0), (1, 1, (0, 128), 2),
                                         (1, 1, (-8, -4), 0), (1, 1, (1, 1), 2), (1, 1, (4096, 4096), 0),
                                         (3, 2, (256, 128), 3), (1, 2, (256, 128), 0xfffffffd)]:
        c = execute_base(display=display)
        c["pages"][2] = 0
        c["textures"][1] = size
        c["ops"] = [op if op[0] != "entry" else ("entry", op[1], op[2], page, texture, op[5]) for op in c["ops"]]
        cases.append(run(c, (10.0, 10.0, 200.0, 150.0)))
    # the entries' texture pointer: the page's texture (label) when on a page, NULL otherwise
    c = execute_base()
    c["ops"] = [op if op[0] != "entry" else ("entry", op[1], op[2], op[3], 3 if op[1] % 2 else 2, op[5]) for op in c["ops"]]
    cases.append(run(c, (0.0, 0.0, 255.0, 191.0)))
    # DrawSprite failing at each tile
    for k in range(7):
        c = execute_base(q={"DrawSprite": [1] * k + [0, 1, 1]})
        cases.append(run(c, (0.0, 0.0, 255.5, 191.5)))
    # the first-entry offset
    for first in (-5, -3, -1, 1, 0):
        c = execute_base(cols=4, rows=3)
        cases.append(run(c, (70.0, 70.0, 130.0, 130.0), extra=[("poke", 0x48, first)]))
    cases.append(run(execute_base(), (10.0, 10.0, 100.0, 100.0), extra=[("poke", 0x48, 0)]))
    # the window: reversed, empty, huge, negative
    for window in [(10.0, 20.0, 10.0, 20.0), (300.0, 200.0, 10.0, 20.0), (-50.0, -50.0, -10.0, -10.0),
                   (0.0, 0.0, 1e30, 1e30), (0.0, 0.0, 640.0, 480.0), (1e-30, 1e-30, 2e-30, 2e-30)]:
        cases.append(run(execute_base(), (10.0, 10.0, 200.0, 150.0), window=window))
    # NaN and infinities in the window and in the rectangle's inner coordinates
    for pos in range(4):
        for bad in (math.nan, math.inf, -math.inf):
            window = [10.0, 20.0, 330.0, 260.0]
            window[pos] = bad
            cases.append(run(execute_base(), (10.0, 10.0, 200.0, 150.0), window=tuple(window)))
    # GetBlendingMode answers (not used) and a zoom whose entries were never given a page
    cases.append(run(execute_base(q={"Entry.GetBlendingMode": [0x1256, 0x556, 0, 0xffffffff]}), (0.0, 0.0, 255.5, 191.5)))
    # two executes in a row, and a use between them
    c = execute_base()
    run(c, (10.0, 10.0, 100.0, 100.0))
    cases.append(run(c, (30.0, 40.0, 250.0, 180.0), window=(0.0, 0.0, 100.0, 100.0)))
    return cases


# every FnTest gets the edge list and 200 random cases of its own function
TESTS = [FnTest("CMapZoom::CMapZoom (__sinit_tb_mapzoom_cpp)", ppc, host, edge=edges_ctor, rnd=rnd_ctor),
         FnTest("CMapZoom::~CMapZoom", ppc, host, edge=edges_dtor, rnd=rnd_dtor),
         FnTest("CMapZoom::EnableFiltering", ppc, host, edge=edges_filter("on"), rnd=rnd_filter("on")),
         FnTest("CMapZoom::DisableFiltering", ppc, host, edge=edges_filter("off"), rnd=rnd_filter("off")),
         FnTest("CMapZoom::Init", ppc, host, edge=edges_init, rnd=rnd_init),
         FnTest("CMapZoom::Close", ppc, host, edge=edges_close, rnd=rnd_close),
         FnTest("CMapZoom::Use", ppc, host, edge=edges_use, rnd=rnd_use),
         FnTest("CMapZoom::Window", ppc, host, edge=edges_window, rnd=rnd_window),
         FnTest("CMapZoom::Update", ppc, host, edge=edges_update, rnd=rnd_update),
         FnTest("CMapZoom::Prepare", ppc, host, edge=edges_prepare, rnd=rnd_prepare),
         FnTest("CMapZoom::Execute", ppc, host, edge=edges_execute, rnd=rnd_execute)]
