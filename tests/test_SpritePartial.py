"""The partial sprite draw and the sprite shadows (original tb_sprite.cpp), engine library: SSpriteGameData::DrawPartial,
CreateShadow, DrawRegularShadow and AddRegularShadow, original versus translation.

One world is built twice from one description, in emulator memory for the original and through tpw_sp_reset
(tests/unit_SpritePartial.cpp) for the translation: four sprites with a pixel block (made by the real Init(int, int), SetUsedPart and
GeneratePartialTexture), three plain sprites that point into an array of six standalone texture entries, a sprite file of four
sprites (the frames of the shadows), six sprite instances, the shadow object gShadow (up to four lights), a height field with its
heights, the final matrix, the texture cache, the sprite manager and the draw primitive. A script of operations runs on both sides
(the codes are tpw_sp_op's); after every operation the test compares the answer and a flat list of numbers that names the state of
everything the operation could have changed (tpw_sp_state), and at the end the events the fakes recorded (call log), in order.

Fakes that record on both sides: the draw primitive's DrawSprite (which also does what the real one does to the caller's record:
the fog bit joins the blending mode, the colour is ANDed with the light manager's), AddSprite and ExecuteCurrentPrimitive; the
instance members of the regular draw that are not in this unit (Prepare, DrawRegular, AddRegular); RotateVertex_Y (a quarter turn,
exact); the texture cache's Prepare and Remove; the engine allocator; the error handler; the assertion. Real on both sides: the
sprite classes' members that make a partial sprite, CTextureEntry, CShadow's GetFirstShadowMatrix and GetNextShadowMatrix,
CSpriteManager's Add, Render and Reset members and the math unit (a real CMatrixPowerPC: the two transforms of CreateShadow).

Guards. Some scripts would make the original read memory the two sides cannot agree on (a sprite that was never made, more cells
than texture entries, a division by a cache size of zero, a corner so far from the height field that its cell lies outside the
table). The PowerPC run decides, before each operation, whether it is safe and records the skipped operations in the case
(case["skip"]); the host leaves those out. Both sides answer -8 for a skipped operation.

Corrections for the emulator (HardwareRounding): Unicorn (QEMU 5.0) computes fmadds, fmsubs, fnmadds and fnmsubs in double precision
and rounds twice, and gives 0x80000000 for fctiwz of a value above INT32_MAX where the processor gives 0x7fffffff. A hook on each such
instruction of the code under test (DrawPartial, CreateShadow and the math unit's strided Transform) puts the processor's result in
before it runs.

Not tested: a corner so far from the height field that the cell lookup leaves the table (the original reads whatever is there);
NaN or infinite positions in the shadows (the same lookup); a NULL gei_MathUnit (the assertion is followed by a null dereference);
a DrawSprite record's last field in DrawRegularShadow (stack garbage in the original; both sides record 0); a light index outside
the four slots.
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
SHIM_LIBRARY = "unit_SpritePartial"
MASK = 0xFFFFFFFF
PC_REG = P.UC_PPC_REG_PC

# ---------------------------------------------------------------- addresses (engine library)
SPRITE = dict(ctor=0x1002E0E0, dtor=0x1002E138, CreateTextureEntry=0x1002E1C4, Init=0x1002EA18, SetUsedPart=0x1002EB00,
              Generate=0x1002EBB8, Destroy=0x1002EE28, Refresh=0x1002EF10, Close=0x1002F00C)
ENTRY = dict(ctor=0x10040D30, Create=0x10040E84, Close=0x10040F7C, GetBlendingMode=0x100410A4, SetSize=0x10041560,
             StoreAsWrapping=0x10041848)
INNER = [0x10040DFC]                       # CTextureEntry's destructor
GAME = dict(ctor=0x1002F144, Prepare=0x1002F3D0, CreateShadow=0x1002FA14, DrawRegularShadow=0x1002FF20,
            AddRegularShadow=0x10030210, DrawRegular=0x10030A38, AddRegular=0x10030AC0, DrawPartial=0x10030B48)
SHADOW = dict(First=0x100297F4, Next=0x1002984C)
MANAGER = dict(Reset=0x10031770, Render=0x100317E4, AddSprite=0x10031874, AddPartialSprite=0x100318E4,
               RenderSprites=0x10031A74, RenderPartialSprites=0x10031F08)
MATH_TRANSFORM = 0x10041C90                # CMatrixPowerPC::Transform (strided)
ERROR_PUSH, CACHE_REMOVE, CACHE_PREPARE = 0x10008904, 0x10034610, 0x10032DDC
DRAW_SPRITE, ADD_SPRITE, EXECUTE, ROTATE = 0x10007EDC, 0x10008230, 0x10007360, 0x10038CD0

P_MALLOC, P_FREE = 0x10051F70, 0x10051F74
DISPLAY, DISPLAY_SIZE = 0x10165634, 0x93C
SYSTEM, SETTINGS = 0x10165F80, 0x10165FC8
SHADOW_OBJECT, SHADOW_SIZE = 0x10166018, 0x1C8
G_TCACHE, G_MANAGER, G_HFIELD, G_MATH, G_FINAL, G_DRAW = 0x10173184, 0x10173154, 0x100AF11C, 0x100AFE94, 0x100AFF70, 0x10052B3C
GROUPS, GROUPS_SIZE = 0x10169154, 0x800 * 0x14
GUARDED = [(DISPLAY, DISPLAY_SIZE), (SYSTEM, 0x20), (SETTINGS, 8), (SHADOW_OBJECT, SHADOW_SIZE), (G_TCACHE, 4), (G_MANAGER, 4),
           (G_HFIELD, 4), (G_MATH, 4), (G_FINAL, 4), (G_DRAW, 4), (P_MALLOC, 8), (GROUPS, GROUPS_SIZE)]

# Code whose fused multiplies and conversions the emulator gets wrong.
CODE_RANGES = [(GAME["DrawPartial"], GAME["DrawPartial"] + 1296), (GAME["CreateShadow"], GAME["CreateShadow"] + 1220),
               (MATH_TRANSFORM, MATH_TRANSFORM + 1920)]

# ---------------------------------------------------------------- the world (the numbers of tests/unit_SpritePartial.cpp)
BLOCKS, POOL, INSTANCES, ENTRIES, PAGES, FILE_SPRITES = 4, 7, 6, 6, 8, 4
L_DRAW, L_MATH, L_CACHE, L_MANAGER, L_SHADOW, L_FIELD, L_FINAL, L_FILE = 1, 2, 3, 4, 5, 6, 7, 8
L_POOL, L_INSTANCE, L_ENTRY, L_FILE_SPRITE, L_PAGE = 10, 20, 40, 50, 60
SPRITE_SIZE, ENTRY_SIZE, GAME_SIZE, MANAGER_SIZE = 0x18, 0x5C, 0x44, 0x2500
MOST_PARTS = 36
# The names of the environment values, in the order of operation 100's selector.
ENV_KEYS = ("flags", "aspect", "width", "height", "f938", "ui", "renderer", "mask", "current", "lights")
K_ON_PAGE = 0x1000


def s32(v):
    return T.s32(v)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def ws(e, a, values):
    e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))


def rs(e, a, n):
    return list(struct.unpack(">" + "I" * n, e.read(a, n * 4)))


def canon(w):
    """Any NaN (as float bits) as one value."""
    return 0x7FC00000 if w & 0x7F800000 == 0x7F800000 and w & 0x007FFFFF else w


def fb(x):
    """The bits of a single-precision value."""
    return struct.unpack(">I", struct.pack(">f", T.f32(x)))[0]


def bf(w):
    return struct.unpack(">f", struct.pack(">I", w & MASK))[0]


def divw(a, b):
    """The processor's divide as the emulator does it (and the translation): zero divisors never reach it here."""
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


# ---------------------------------------------------------------- the emulator's float corrections
LIBM = C.CDLL(None)
LIBM.fmaf.restype = C.c_float
LIBM.fmaf.argtypes = [C.c_float, C.c_float, C.c_float]


def fpr(e, i):
    v = e.uc.reg_read(ppcemu.FPR[i])
    return v if isinstance(v, int) else struct.unpack(">Q", struct.pack(">d", v))[0]


def fpr_double(e, i):
    return struct.unpack(">d", struct.pack(">Q", fpr(e, i)))[0]


class HardwareRounding:
    """Makes the emulator's single-precision fused multiplies and fctiwz behave as the processor's do, for the code ranges above.
    Each such instruction gets a code hook of its own (the rest of the code runs at full speed): the hook computes the processor's
    result, writes it to the destination register and steps over the instruction. A fused operand that is not finite is left to the
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
                if word >> 26 == 59 and (word >> 1) & 31 in (28, 29, 30, 31):       # fmsubs, fmadds, fnmsubs, fnmadds
                    self.sites[address] = ("fma", (word >> 1) & 31, rd, ra, rb, rc)
                elif word >> 26 == 63 and (word >> 1) & 0x3FF == 15:                  # fctiwz frD, frB
                    self.sites[address] = ("fctiwz", 0, rd, ra, rb, rc)

    def __enter__(self):
        for address in sorted(self.sites):
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=address, end=address))

    def __exit__(self, *exc):
        for h in self.hooks:
            self.e.uc.hook_del(h)
        self.hooks = []

    def step(self, uc, address, size, user):
        e = self.e
        kind, form, rd, ra, rb, rc = self.sites[address]
        if kind == "fma":
            a, c, b = fpr_double(e, ra), fpr_double(e, rc), fpr_double(e, rb)
            if all(math.isfinite(x) for x in (a, c, b)):
                for x in (a, c, b):
                    assert struct.unpack(">f", struct.pack(">f", x))[0] == x, "a fused operand is not a single"
                value = float(LIBM.fmaf(a, c, -b if form in (28, 30) else b))
                if form in (30, 31):
                    value = -value
                corrected = struct.unpack(">Q", struct.pack(">d", value))[0]
                uc.reg_write(ppcemu.FPR[rd], corrected)
                uc.reg_write(PC_REG, address + 4)
        else:
            if fpr_double(e, rb) >= 2147483648.0:
                uc.reg_write(ppcemu.FPR[rd], (fpr(e, rd) & 0xFFFFFFFF00000000) | 0x7FFFFFFF)
                uc.reg_write(PC_REG, address + 4)


# ---------------------------------------------------------------- the PowerPC side


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    # a real math unit, as in test_MeshMatrix.py
    obj = T.persist_alloc(e, 0x14)
    T.call_ppc(e, e.func("__ct__14CMatrixPowerPCFv"), [obj, 1])
    e.sp_math_unit = e.u32(obj)
    if not hasattr(e, "sp_fns"):
        route = lambda name: (lambda emu: getattr(emu.sp_side, name)())
        e.sp_fns = {name: T.fake_function(e, route(name)) for name in ("cb_malloc", "cb_free")}


def callback_stub(fn):
    """An import stub that calls PowerPC code back (tpwtest.stub_cb, but for one case's stubs): fn(emu, call)."""
    def hook(e):
        e.pending_cb = fn
        e.uc.reg_write(T.P.UC_PPC_REG_PC, T.ppcemu.RETURN_ADDR)
    return hook


class PpcSide:
    """The case's world in emulator memory, the fakes, the guards, the operations and the state."""

    def __init__(self, e, case):
        self.e, self.case = e, case
        e.sp_side = self
        self.env = dict(case["env"])    # the operations change the environment; the host starts from the case's
        self.pool = [e.alloc(SPRITE_SIZE, b"\xa5" * SPRITE_SIZE) for _ in range(POOL)]
        self.inst = [e.alloc(GAME_SIZE, b"\xa5" * GAME_SIZE) for _ in range(INSTANCES)]
        self.entries = e.alloc(ENTRIES * ENTRY_SIZE, bytes(ENTRIES * ENTRY_SIZE))
        self.fsprites = e.alloc(FILE_SPRITES * SPRITE_SIZE, bytes(FILE_SPRITES * SPRITE_SIZE))
        self.file = e.alloc(0x10, bytes(0x10))
        self.markers = e.alloc(16, bytes(16))
        self.cache = e.alloc(0x1900, bytes(0x1900))
        self.manager = e.alloc(MANAGER_SIZE, bytes(MANAGER_SIZE))
        self.draw = e.alloc(0x40, bytes(0x40))
        self.final = e.alloc(64, bytes(64))
        self.field = e.alloc(0x20, bytes(0x20))
        self.record = e.alloc(0x40, bytes(0x40))
        count = len(case["heights"])
        self.heights = e.alloc(count * 4 + 16, bytes(count * 4 + 16))
        self.corners = e.alloc(0x40, bytes(0x40))
        fixed = {self.draw: L_DRAW, e.sp_math_unit: L_MATH, self.cache: L_CACHE, self.manager: L_MANAGER, SHADOW_OBJECT: L_SHADOW,
                 self.field: L_FIELD, self.final: L_FINAL, self.file: L_FILE}
        fixed.update({self.pool[p]: L_POOL + p for p in range(POOL)})
        fixed.update({self.inst[g]: L_INSTANCE + g for g in range(INSTANCES)})
        fixed.update({self.entry(i): L_ENTRY + i for i in range(ENTRIES)})
        fixed.update({self.fsprites + SPRITE_SIZE * i: L_FILE_SPRITE + i for i in range(FILE_SPRITES)})
        fixed.update({self.marker(p): L_PAGE + p for p in range(1, PAGES + 1)})
        self.L = CL.PpcLog(fixed, case.get("q"))
        e.cl = self.L
        self.built = [False] * BLOCKS
        self.gen_block = [0] * BLOCKS
        self.plain = [False] * (POOL - BLOCKS)
        self.rotation = [False] * INSTANCES
        self.file_set = False
        self.heap = [0, 0, 0, 0]
        self.light_mask = self.env["mask"]
        self.skipped = set()

    def entry(self, i):
        return self.entries + ENTRY_SIZE * i if 0 <= i < ENTRIES else 0

    def marker(self, p):
        return self.markers + p if 1 <= p <= PAGES else 0

    # ---- the engine's callbacks
    def malloc(self, size):
        L = self.L
        L.rec("Malloc", (size,))
        if not L.ret("Malloc", 1) or size > 0x10000:
            return 0
        block = self.e.alloc(size + 16, bytes(size + 16))
        L.newlab(block)
        return block

    def cb_malloc(self):
        self.e.set_r(3, self.malloc(self.e.r(3)))

    def cb_free(self):
        e, L = self.e, self.L
        block = e.u32(e.r(3))
        L.rec("Free", (L.lab(block),))
        L.forget(block)
        w32(e, e.r(3), 0)

    # ---- the heap (the host's operator new gives its blocks no labels here, so neither does this side; only the engine allocator's
    # blocks have labels and the heap counters count the rest)
    def new_scalar(self, size):
        size = max(size, 4)
        block = self.e.alloc(size, b"\xa5" * size)
        self.heap[0] += 1
        return block

    def new_array_block(self, size):
        size = max(size, 4)
        block = self.e.alloc(size, b"\xa5" * size)
        self.heap[1] += 1
        return block

    # ---- labels
    def entry_label(self, a):
        """A standalone entry by its number; a partial texture by its block and index: 1000 + 100 * block + index."""
        e = self.e
        if a == 0:
            return 0
        for i in range(ENTRIES):
            if a == self.entry(i):
                return L_ENTRY + i
        for k in range(BLOCKS):
            if not self.built[k]:
                continue
            texture = e.u32(self.pool[k])
            if texture == 0:
                continue
            count = struct.unpack(">H", e.read(self.pool[k] + 0x16, 2))[0]
            for i in range(min(count, 64)):
                if a == texture + ENTRY_SIZE * i:
                    return 1000 + 100 * k + i
        return -1

    # ---- the fakes
    def from_regular_shadow(self):
        """DrawRegularShadow leaves the record's last field as stack garbage: the call is told apart by its return address."""
        lr = self.e.uc.reg_read(P.UC_PPC_REG_LR)
        return GAME["DrawRegularShadow"] <= lr < GAME["DrawRegularShadow"] + 692

    def draw_sprite(self, name, last_field):
        e, L = self.e, self.L
        this, record, mode = e.r(3), e.r(4), s32(e.r(5))
        flags, vertices, colour, blend, page = rs(e, record, 5)
        u0, v0, u1, v1 = rs(e, record + 0x18, 4)
        last = s32(e.u32(record + 0x28))
        L.rec(name, (L.lab(this), mode, flags, colour, blend, L.lab(page), canon(u0), canon(v0), canon(u1), canon(v1),
                     last if last_field else 0))
        words = [canon(x) for x in rs(e, vertices, 16)]
        L.rec("Corners", tuple(words[0:8]))
        L.rec("Corners", tuple(words[8:16]))
        if e.u32(DISPLAY) & 0x2000:
            w32(e, record + 0x0C, blend | 0x100000)
        w32(e, record + 8, colour & self.light_mask)
        e.set_r(3, T.u32(L.ret(name, 1)))

    def rotate_vertex(self):
        e, L = self.e, self.L
        vertices, centre = e.r(3), e.r(5)   # the angle is a float in f1 and takes the slot of r4
        w = rs(e, vertices, 16)
        L.rec("RotateVertex_Y", (canon(CL.fbits(e.f(1))), L.lab(centre), canon(w[0]), canon(w[1]), canon(w[2]), canon(w[4]),
                                 canon(w[5]), canon(w[6]), canon(w[8]), canon(w[9]), canon(w[10])))
        for i in range(4):
            x, z = w[4 * i], w[4 * i + 2]
            w[4 * i], w[4 * i + 2] = z, x ^ 0x80000000
        ws(e, vertices, w)

    def stubs(self):
        L = self.L
        st = CL.Stubs()
        st.real = (set(SPRITE.values()) | set(ENTRY.values()) | set(INNER) | set(GAME.values()) | set(SHADOW.values())
                   | set(MANAGER.values()) | {MATH_TRANSFORM})
        # the fakes are not "real": take the ones that are fakes out of the set
        st.real -= {GAME["Prepare"], GAME["DrawRegular"], GAME["AddRegular"]}
        st.internal(ERROR_PUSH, CL.recorder("Error", "xii"))
        st.internal(CACHE_REMOVE, lambda e: (L.rec("Cache.Remove", (L.lab(e.r(3)), self.entry_label(e.r(4)))), e.set_r(3, 1)))
        st.internal(CACHE_PREPARE, lambda e: L.rec("TCache.Prepare", (L.lab(e.r(3)), self.entry_label(e.r(4)), CL.fbits(e.f(1)))))
        st.internal(DRAW_SPRITE, lambda e: self.draw_sprite("DrawSprite", not self.from_regular_shadow()))
        st.internal(ADD_SPRITE, lambda e: self.draw_sprite("AddSprite", True))
        st.internal(EXECUTE, CL.recorder("Execute", "Lf", ret=lambda e, L: L.ret("Execute", 1)))
        st.internal(ROTATE, lambda e: self.rotate_vertex())
        st.internal(GAME["Prepare"], CL.recorder("SpritePrepare", "L", ret=lambda e, L: 1))
        st.internal(GAME["DrawRegular"], CL.recorder("DrawRegular", "L", ret=lambda e, L: L.ret("DrawRegular", 1)))
        st.internal(GAME["AddRegular"], CL.recorder("AddRegular", "L", ret=lambda e, L: L.ret("AddRegular", 1)))
        st.imp("__assertion_failed", lambda e: L.rec("assert", (s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))

        def op_new(e):
            e.set_r(3, self.new_scalar(e.r(3)))

        def op_new_array(e):
            e.set_r(3, self.new_array_block(e.r(3)))

        def op_delete(e):
            block = e.r(3)
            if block:
                self.heap[2] += 1

        def construct(e, call):
            raw, ctor, dtor, size, n = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
            w32(e, raw, size)
            w32(e, raw + 4, n)
            for i in range(n):
                call(ctor, raw + 8 + i * size, 1)
            return raw + 8

        def destroy(e, call):
            base, dtor = e.r(3), e.r(4)
            size, n = e.u32(base - 8), e.u32(base - 4)
            for i in range(n - 1, -1, -1):
                call(dtor, base + i * size, 0xFFFFFFFF)
            self.heap[3] += 1
            return None

        st.imp("__nw__FUl", op_new)
        st.imp("__nwa__FUl", op_new_array)
        st.imp("__dl__FPv", op_delete)
        st.imp("__construct_new_array", callback_stub(construct))
        st.imp("__destroy_new_array", callback_stub(destroy))
        return st

    def reset(self):
        e, case = self.e, self.case
        field = case["field"]
        fns = e.sp_fns
        for a, v in ((P_MALLOC, fns["cb_malloc"]), (P_FREE, fns["cb_free"]), (G_TCACHE, self.cache), (G_MANAGER, self.manager),
                     (G_HFIELD, self.field), (G_MATH, e.sp_math_unit), (G_FINAL, self.final), (G_DRAW, self.draw)):
            w32(e, a, v)
        self.set_env()
        w32(e, DISPLAY + 0x80, 0)
        w32(e, SYSTEM, 0)
        w32(e, SETTINGS + 4, 4)
        shadow = case["shadow"]
        ws(e, SHADOW_OBJECT + 0x40, shadow["matrices"])
        ws(e, SHADOW_OBJECT + 0x140, shadow["cross"])
        ws(e, SHADOW_OBJECT + 0x1B0, shadow["shades"])
        ws(e, self.final, case["final"])
        ws(e, self.heights, case["heights"])
        ws(e, self.record + 8, [field["ox"], field["oz"], field["cx"], field["cz"]])
        w32(e, self.record + 0x28, self.heights + 4 * field["margin"])
        w32(e, self.field, self.record)
        ws(e, self.field + 8, [field["columns"], field["rows"], field["inv_x"], field["inv_z"]])
        e.write(GROUPS, b"\xff" * GROUPS_SIZE)
        for g in range(INSTANCES):
            self.call(GAME["ctor"], self.inst[g])

    def set_env(self):
        for key in ENV_KEYS:
            self.write_env(key)

    def write_env(self, key):
        e, value = self.e, self.env[key]
        address = {"flags": DISPLAY, "aspect": DISPLAY + 4, "width": DISPLAY + 8, "height": DISPLAY + 0xC, "f938": DISPLAY + 0x938,
                   "ui": self.cache + 0x18D8, "renderer": SYSTEM + 0x1C, "lights": SHADOW_OBJECT + 0x1C0,
                   "current": SHADOW_OBJECT + 0x1C4}.get(key)
        if address is not None:
            w32(e, address, value)

    # ---- memory readers
    def u16(self, a):
        return struct.unpack(">H", self.e.read(a, 2))[0]

    def s16(self, a):
        return struct.unpack(">h", self.e.read(a, 2))[0]

    def f(self, a):
        return bf(self.e.u32(a))

    def sprite_of(self, g):
        """The pool index an instance's sprite pointer names; None for NULL or anything else."""
        pointer = self.e.u32(self.inst[g] + 0x28)
        return self.pool.index(pointer) if pointer in self.pool else None

    def sprite_made(self, p):
        if p is None:
            return False
        return self.built[p] if p < BLOCKS else self.plain[p - BLOCKS]

    # ---- the guards: can the original run this operation without reading what the two sides cannot agree on
    def entries_after(self, p):
        """How many entries the sprite's texture pointer has from there on: None for no texture, else the length of the array."""
        texture = self.e.u32(self.pool[p])
        if texture == 0:
            return None
        if p < BLOCKS:
            return self.u16(self.pool[p] + 0x16)
        index = (texture - self.entries) // ENTRY_SIZE
        return ENTRIES - index

    def partial_ok(self, g):
        p = self.sprite_of(g)
        if not self.sprite_made(p):
            return False
        # The word at 0x30 is two halfwords here (the offset) and a float for the shadows; a float stored there reads as its
        # halves swapped on a little-endian host, so only a zero one is the same on both sides.
        if self.rotation[g] and self.e.u32(self.inst[g] + 0x30) != 0:
            return False
        cell = s32(self.e.u32(self.cache + 0x18D8))
        if cell == 0:
            return False
        s = self.pool[p]
        columns = divw(self.u16(s + 0x10) + cell - 1, cell)
        rows = divw(self.u16(s + 0x12) + cell - 1, cell)
        if columns <= 0 or rows <= 0:
            return True
        available = self.entries_after(p)
        count = columns * rows
        return count <= 1 if available is None else count <= available

    def texture_ok(self, p):
        """The sprite's texture entry can be read (its first entry exists)."""
        if self.e.u32(self.pool[p]) == 0:
            return True
        available = self.entries_after(p)
        return available is not None and available >= 1

    def light_count(self):
        return s32(self.e.u32(SHADOW_OBJECT + 0x1C0))

    def corners_ok(self, g, light, matrix, current, scale_x, scale_z):
        """The shadow's four corners worked out in double precision: finite, and every height lookup inside the table (a guard,
        not the oracle). `matrix` is the shadow matrix's 16 floats, `current` the light whose cross product is used."""
        e = self.e
        p = self.sprite_of(g)
        if not self.sprite_made(p):
            return False
        s, d = self.pool[p], self.inst[g]
        used_w, used_h = self.u16(s + 0x10), self.u16(s + 0x12)
        if used_w == 0 or used_h == 0:
            return False
        field, flags = self.case["field"], e.u32(d + 0x10)
        word = e.u32(d + 0x30)
        if flags & 0x80 and not self.rotation[g] and (word >> 16) != (word & 0xFFFF):
            return False   # two halfwords read as a float: the halves swap on a little-endian host
        pos = [self.f(d + 0x14), self.f(d + 0x18), self.f(d + 0x1C)]
        size = 20.0 * self.f(d + 0x2C)
        wide = size * self.f(DISPLAY + 4)
        offset_x, offset_y, width, height = self.s16(s + 0x14), self.s16(s + 0x16), self.u16(s + 0xC), self.u16(s + 0xE)
        far_z = scale_z * (size * ((offset_y + height) / used_h))
        near_z = scale_z * (size * (offset_y / used_h))
        right_x = scale_x * (wide * ((offset_x + width) / used_w))
        left_x = scale_x * (wide * (offset_x / used_w))
        if flags & 0x10:
            left_x, right_x = -left_x, -right_x
        if flags & 0x20:
            near_z, far_z = -near_z, -far_z
        cross = [self.f(SHADOW_OBJECT + 0x140 + 12 * current + 4 * i) for i in range(3)]
        points = [(left_x, far_z), (right_x, far_z), (right_x, near_z), (left_x, near_z)]
        corners = [[pos[0] + a * cross[0], pos[1] + z, pos[2] + a * cross[2]] for a, z in points]
        low = min(c[1] for c in corners)
        for c in corners:
            c[1] -= low
        if flags & 0x80:
            corners = [[c[2], c[1], -c[0]] for c in corners]
        for c in corners:
            if not all(math.isfinite(v) for v in c):
                return False
        m = matrix
        out = []
        for x, y, z in corners:
            out.append((m[12] + z * m[8] + x * m[0] + y * m[4], m[14] + z * m[10] + x * m[2] + y * m[6]))
        columns, rows, margin, count = field["columns"], field["rows"], field["margin"], len(self.case["heights"])
        for x, z in out:
            if not (math.isfinite(x) and math.isfinite(z)):
                return False
            cx = int((x - bf(field["ox"])) * bf(field["inv_x"]))
            cz = int((z - bf(field["oz"])) * bf(field["inv_z"]))
            if abs(cx) > 1e6 or abs(cz) > 1e6:
                return False
            nx, nz = min(max(cx + 1, 0), columns), min(max(cz + 1, 0), rows)
            indices = [cx + columns * cz, nx + columns * cz, cx + columns * nz]
            if min(indices) < -margin + 4 or max(indices) > count - margin - 8:
                return False
        return True

    def matrix_words(self, light):
        return [bf(w) for w in rs(self.e, SHADOW_OBJECT + 0x40 + 64 * light, 16)]

    def shadow_ok(self, g):
        """DrawRegularShadow / AddRegularShadow on instance g run safely (every light, as the loop visits them)."""
        e = self.e
        flags = e.u32(self.inst[g] + 0x10)
        if not (flags & 0x200 and flags & 0x10000000):
            return True
        p = self.sprite_of(g)
        if not self.sprite_made(p) or not self.texture_ok(p):
            return False
        lights = self.light_count()
        if lights > 4:
            return False
        if e.u32(DISPLAY) & 0x100000 and lights > 0:
            # each shadow takes the picture of its own frame from the sprite file
            if e.u32(self.inst[g] + 0x34) != self.file or not self.file_set:
                return False
            count = s32(e.u32(self.file))
            base = self.u16(self.inst[g] + 0x38)
            for index in range(lights):
                frame = base + e.read(self.inst[g] + 0x3C + index, 1)[0]
                if 0 <= frame < count and frame >= FILE_SPRITES:
                    return False
        for light in range(lights):
            if not self.corners_ok(g, light, self.matrix_words(light), light, 1.0, 1.0):
                return False
        return True

    def render_ok(self, sprites, partials):
        e = self.e
        if sprites:
            flags = e.u32(DISPLAY)
            for i in range(min(s32(e.u32(self.manager + 0x2404)), 0x800)):
                g = self.instance_of(e.u32(self.manager + 4 * i))
                if g is None:
                    return False
                if not flags & 0x80:
                    # the sorted draw looks at the sprite's texture entry
                    p = self.sprite_of(g)
                    if not self.sprite_made(p) or e.u32(self.pool[p]) == 0 or not self.texture_ok(p):
                        return False
                if not (flags & 0x20000) and not self.shadow_ok(g):
                    return False
        if partials:
            for i in range(min(s32(e.u32(self.manager + 0x2408)), 0x80)):
                g = self.instance_of(e.u32(self.manager + 0x2000 + 4 * i))
                if g is None or not self.partial_ok(g):
                    return False
        return True

    def instance_of(self, pointer):
        return self.inst.index(pointer) if pointer in self.inst else None

    # ---- one operation
    def call(self, addr, *args):
        return T.call_ppc(self.e, addr, list(args))

    def block(self, k):
        return self.pool[k & 3]

    def op(self, index, code, a):
        e, L = self.e, self.L
        result = self.run_op(code, a)
        if result is None:
            self.skipped.add(index)
            return -8
        return result

    def run_op(self, code, a):
        """The operation's answer; None when the guard says the original could not run it safely."""
        e, L, u = self.e, self.L, self.e.u32
        if code == 100:
            key = ENV_KEYS[a[0]]
            self.env[key] = a[1] & MASK
            if key == "mask":
                self.light_mask = a[1] & MASK
            self.write_env(key)
            return 0
        if code in (10, 11, 12, 13, 14, 15):
            k = a[0] & 3
            s = self.pool[k]
            if code == 10:
                if not self.built[k]:
                    self.call(SPRITE["ctor"], s)
                    self.built[k] = True
                return s32(self.call(SPRITE["Init"], s, a[1], a[2])[0])
            if not self.built[k]:
                return -8
            if code == 11:
                # a sprite without a pixel block closes the first entry of its texture, which an array of none does not have
                if u(s + 4) == 0 and u(s) != 0 and self.u16(s + 0x16) < 1:
                    return None
                return s32(self.call(SPRITE["Close"], s)[0])
            if code == 12:
                self.call(SPRITE["SetUsedPart"], s, T.s16(a[1]), T.s16(a[2]))
                return 0
            if code == 13:
                cell = s32(u(self.cache + 0x18D8))
                if cell <= 0:
                    return None
                columns = divw(self.u16(s + 0x10) + cell - 1, cell)
                rows = divw(self.u16(s + 0x12) + cell - 1, cell)
                if columns * rows > 64 or columns * rows < 0:
                    return None
                made = s32(self.call(SPRITE["Generate"], s, a[1], a[2])[0])
                self.gen_block[k] = u(s + 4)
                return made
            if code == 14:
                # the Close of a sprite without a pixel block leaves the count of the array it destroyed
                if self.u16(s + 0x16) != 0 and u(s) == 0:
                    return None
                return s32(self.call(SPRITE["Destroy"], s)[0])
            return s32(self.call(SPRITE["Refresh"], s)[0])
        if code == 16:
            k, i = a[0] & 3, a[1]
            texture = u(self.pool[k])
            if not self.built[k] or texture == 0 or i < 0 or i >= self.u16(self.pool[k] + 0x16):
                return -8
            self.write_entry(texture + ENTRY_SIZE * i, a[2:8])
            return 0
        if code == 17:
            entry = self.entry(a[0])
            if entry == 0:
                return -8
            self.write_entry(entry, a[1:7])
            return 0
        if code == 18:
            p = BLOCKS + a[0] % 3
            s = self.pool[p]
            e.write(s, bytes(SPRITE_SIZE))
            w32(e, s, self.entry(a[1]))
            e.write(s + 0xC, struct.pack(">HHHHhh", a[2] & 0xFFFF, a[3] & 0xFFFF, a[4] & 0xFFFF, a[5] & 0xFFFF, T.s16(a[6]), T.s16(a[7])))
            self.plain[p - BLOCKS] = True
            return 0
        if code == 19:
            e.write(self.fsprites, bytes(FILE_SPRITES * SPRITE_SIZE))
            for i in range(FILE_SPRITES):
                w32(e, self.fsprites + SPRITE_SIZE * i, self.entry(a[1 + i]))
            w32(e, self.file, a[0])
            w32(e, self.file + 4, self.fsprites)
            self.file_set = True
            return 0
        if code == 20:
            g = a[0] % INSTANCES
            d = self.inst[g]
            sub = a[1]
            if sub == 0:
                w32(e, d + 0x10, a[2])
            elif sub == 1:
                ws(e, d + 0x14, a[2:5])
            elif sub == 2:
                w32(e, d + 0x24, a[2])
            elif sub == 3:
                w32(e, d + 0x2C, a[2])
            elif sub == 4:
                e.write(d + 0x30, struct.pack(">hh", T.s16(a[2]), T.s16(a[3])))
                self.rotation[g] = False
            elif sub == 5:
                w32(e, d + 0x30, a[2])
                self.rotation[g] = True
            elif sub == 6:
                w32(e, d + 0x20, a[2])
            elif sub == 7:
                w32(e, d + 0x28, self.pool[a[2] % POOL] if a[2] >= 0 else 0)
            elif sub == 8:
                w32(e, d + 0x34, self.file if a[2] else 0)
                e.write(d + 0x38, struct.pack(">H", a[3] & 0xFFFF))
                e.write(d + 0x3C, bytes((a[4] >> (8 * i)) & 0xFF for i in range(4)))
            elif sub == 9:
                w32(e, d + 8, a[2])
            else:
                return -8
            return 0
        if code == 21:
            g = a[0] % INSTANCES
            if not self.partial_ok(g):
                return None
            return s32(self.call(GAME["DrawPartial"], self.inst[g])[0])
        if code in (22, 23):
            g = a[0] % INSTANCES
            if not self.shadow_ok(g):
                return None
            if code == 22:
                return s32(self.call(GAME["DrawRegularShadow"], self.inst[g])[0])
            return s32(self.call(GAME["AddRegularShadow"], self.inst[g], self.entry(a[1]))[0])
        if code == 24:
            g, m = a[0] % INSTANCES, a[1] & 3
            current = s32(u(SHADOW_OBJECT + 0x1C4))
            if not 0 <= current < 4:
                return None
            if not self.corners_ok(g, m, self.matrix_words(m), current, bf(a[2]), bf(a[3])):
                return None
            e.write(self.corners, b"\xa5" * 0x40)
            result = s32(T.call_ppc(e, GAME["CreateShadow"], [self.inst[g], self.corners, self.matrix_address(m), T.F(bf(a[2])), T.F(bf(a[3]))])[0])
            w = [canon(x) for x in rs(e, self.corners, 16)]
            L.rec("Shadow", tuple(w[0:8]))
            L.rec("Shadow", tuple(w[8:16]))
            return result
        if code == 25:
            self.call(MANAGER["Reset"], self.manager)
            return 0
        if code in (26, 27):
            g = a[0] % INSTANCES
            queued = [e.u32(self.manager + 4 * i) for i in range(min(s32(u(self.manager + 0x2404)), 0x800))]
            if code == 26 and self.inst[g] in queued:
                return None   # the sorted draw links an instance to itself and never ends
            return s32(self.call(MANAGER["AddSprite" if code == 26 else "AddPartialSprite"], self.manager, self.inst[g])[0])
        if code in (28, 29, 30):
            if not self.render_ok(code in (28, 29), code in (28, 30)):
                return None
            return s32(self.call(MANAGER[{28: "Render", 29: "RenderSprites", 30: "RenderPartialSprites"}[code]], self.manager)[0])
        if code == 31:
            w32(e, self.manager + 0x2400, self.entry(a[0]))
            return 0
        if code == 32:
            return 1 if self.call(SHADOW["First"], SHADOW_OBJECT)[0] != 0 else 0
        return -9

    def matrix_address(self, m):
        return SHADOW_OBJECT + 0x40 + 64 * m

    def write_entry(self, entry, v):
        """flags, page, u0, v0, usize, vsize"""
        e = self.e
        w32(e, entry + 8, v[0])
        w32(e, entry + 0x18, self.marker(v[1]))
        w32(e, entry + 0x24, v[2])
        w32(e, entry + 0x28, v[3])
        w32(e, entry + 0x1C, v[4])
        w32(e, entry + 0x20, v[5])

    # ---- reading state (the order of tpw_sp_state)
    def entry_words(self, a, partial, reference):
        e, L, u = self.e, self.L, self.e.u32
        w, h, cw, ch = struct.unpack(">hhhh", e.read(a + 0xC, 8))
        out = [self.entry_label(a), u(a + 8), w, h, cw, ch, L.lab(u(a + 0x18)), canon(u(a + 0x1C)), canon(u(a + 0x20)),
               canon(u(a + 0x24)), canon(u(a + 0x28)), struct.unpack(">h", e.read(a + 0x30, 2))[0]]
        data = u(a + 0x3C)
        if data == 0:
            out.append(-1)
        elif partial:
            out.append(T.s32(data - reference))
        else:
            out.append(-2)
        return out

    def state(self):
        e, L, u = self.e, self.L, self.e.u32
        out = list(self.heap)
        for k in range(BLOCKS):
            out.append(1 if self.built[k] else 0)
            if not self.built[k]:
                continue
            s = self.pool[k]
            w, h, uw, uh = struct.unpack(">HHHH", e.read(s + 0xC, 8))
            out += [self.entry_label(u(s)), L.lab(u(s + 4)), canon(u(s + 8)), w, h, uw, uh, self.s16(s + 0x14), self.u16(s + 0x16)]
            texture, count = u(s), self.u16(s + 0x16)
            if texture != 0 and count != 0:
                for i in range(min(count, MOST_PARTS)):
                    out += self.entry_words(texture + ENTRY_SIZE * i, True, self.gen_block[k])
        for g in range(INSTANCES):
            d = self.inst[g]
            p = self.sprite_of(g)
            out += [s32(u(d + 0x10)), canon(u(d + 0x14)), canon(u(d + 0x18)), canon(u(d + 0x1C)), canon(u(d + 0x20)), s32(u(d + 0x24)),
                    -1 if u(d + 0x28) == 0 else (-2 if p is None else p), canon(u(d + 0x2C))]
            if self.rotation[g]:
                out.append(canon(u(d + 0x30)))
            else:
                out += [self.s16(d + 0x30), self.s16(d + 0x32)]
            frame_file = u(d + 0x34)
            out.append(0 if frame_file == 0 else L_FILE if frame_file == self.file else -1)
            out.append(self.u16(d + 0x38))
            b = e.read(d + 0x3C, 4)
            out.append(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24))
            out.append(canon(u(d + 8)))
            out.append(0 if u(d + 0x40) == 0 else 1)
        for i in range(ENTRIES):
            out += self.entry_words(self.entry(i), False, 0)
        out += [s32(u(SHADOW_OBJECT + 0x1C4)), s32(u(SHADOW_OBJECT + 0x1C0))]
        m = self.manager
        n_sprites, n_partials, n_lists = s32(u(m + 0x2404)), s32(u(m + 0x2408)), s32(u(m + 0x240C))
        out += [n_sprites, n_partials, n_lists]
        for i in range(min(n_sprites, 40)):
            g = self.instance_of(u(m + 4 * i))
            out.append(-1 if g is None else g)
        for i in range(min(n_partials, 40)):
            g = self.instance_of(u(m + 0x2000 + 4 * i))
            out.append(-1 if g is None else g)
        out.append(self.entry_label(u(m + 0x2400)))
        out.append(0)
        return out


STATS = {"ops": 0, "skipped": 0}


def ppc(e, case):
    side = PpcSide(e, case)
    saved = [(a, e.read(a, n)) for a, n in GUARDED]
    saved_stubs = side.stubs().activate(e)
    try:
        side.reset()
        results, states, marks = [], [], []
        with HardwareRounding(e):
            for index, (code, args) in enumerate(case["script"]):
                results.append(side.op(index, code, args))
                states.append(side.state())
                marks.append(len(side.L.log))
    finally:
        CL.Stubs.deactivate(e, saved_stubs)
        for a, data in saved:
            e.write(a, data)
    case["skip"] = side.skipped
    STATS["ops"] += len(case["script"])
    STATS["skipped"] += len(side.skipped)
    return dict(results=results, log=side.L.log, marks=marks, states=states)


def host(lib, case):
    reset = lib["tpw_sp_reset"]
    reset.restype = None
    reset.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_uint32), C.POINTER(C.c_uint32), C.c_int32, C.c_int32, C.POINTER(C.c_uint32)]
    op_fn = lib["tpw_sp_op"]
    op_fn.restype, op_fn.argtypes = C.c_int64, [C.POINTER(C.c_int64)]
    state_fn = lib["tpw_sp_state"]
    state_fn.restype, state_fn.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    address = lib["tpw_sp_address"]
    address.restype, address.argtypes = C.c_void_p, [C.c_int32]
    env, field, shadow = case["env"], case["field"], case["shadow"]
    s = (C.c_int64 * 18)(env["flags"], env["aspect"], env["width"], env["height"], env["f938"], env["ui"], env["renderer"],
                         env["mask"], env["lights"], env["current"], field["columns"], field["rows"], field["inv_x"], field["inv_z"],
                         field["ox"], field["oz"], field["cx"], field["cz"])
    shadow_words = (C.c_uint32 * 80)(*(shadow["matrices"] + shadow["cross"] + shadow["shades"]))
    heights = (C.c_uint32 * len(case["heights"]))(*case["heights"])
    final = (C.c_uint32 * 16)(*case["final"])
    reset(s, shadow_words, heights, len(case["heights"]), field["margin"], final)
    labels = [L_DRAW, L_MATH, L_CACHE, L_MANAGER, L_SHADOW, L_FIELD, L_FINAL, L_FILE]
    labels += [L_POOL + p for p in range(POOL)] + [L_INSTANCE + g for g in range(INSTANCES)] + [L_ENTRY + i for i in range(ENTRIES)]
    labels += [L_FILE_SPRITE + i for i in range(FILE_SPRITES)] + [L_PAGE + p for p in range(1, PAGES + 1)]
    CL.host_begin(lib, {label: address(label) for label in labels}, case.get("q"))
    skip = case.get("skip", set())
    results, states, marks = [], [], []
    buffer = (C.c_int64 * 4096)()
    try:
        for index, (code, args) in enumerate(case["script"]):
            if index in skip:
                results.append(-8)
            else:
                padded = (C.c_int64 * 16)(code, *(list(args) + [0] * 15)[:15])
                results.append(op_fn(padded))
            states.append(list(buffer[:state_fn(buffer)]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(results=results, log=log, marks=marks, states=states)


# ---------------------------------------------------------------- random values


def pick(r, values):
    return r.choice(values)


def rfloat(r, lo, hi):
    return fb(r.uniform(lo, hi))


SPECIAL_FLOATS = [fb(v) for v in (0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0, 1e-30, 3.4e38, -3.4e38, 1e10, 1e-10)] + [
    0x7F800000, 0xFF800000, 0x7FC00000, 0xFFC00001, 0x7F800001, 0x00000001, 0x80000001]


def special(r):
    return r.choice(SPECIAL_FLOATS)


ENTRY_FLAG_BITS = [0x1000, 0x100000, 0x100, 0x200, 0x4000000, 0x10000, 0x20000, 0x40, 0x80, 0x140, 0x4000, 0x40000, 0x1000000, 0x2000]


def entry_flags(r):
    """Flags for a texture entry: the ones the draw reads (page 0x1000, the 0x100000 bit, the blending bits 0x300 and 0x4000000,
    the wrapping bits) and some it does not; never kCompressed or kNextIsPlane, which would make the entry's Close unpack or recurse."""
    flags = 0
    for bit in ENTRY_FLAG_BITS:
        if r.random() < 0.3:
            flags |= bit
    if r.random() < 0.1:
        flags = r.getrandbits(32)
    return flags & ~0x10800000 & MASK


def entry_args(r, page_probability=0.7):
    page = r.randrange(1, PAGES + 1) if r.random() < page_probability else 0
    if r.random() < 0.1:
        u0, v0, us, vs = (special(r) for _ in range(4))
    else:
        u0, v0, us, vs = rfloat(r, 0, 1), rfloat(r, 0, 1), pick(r, [rfloat(r, 0, 1), fb(0.125), fb(0.25), fb(1.0), fb(0.0)]), pick(r, [rfloat(r, 0, 1), fb(0.125), fb(0.25), fb(1.0), fb(0.0)])
    return [entry_flags(r), page, u0, v0, us, vs]


def lcg_matrix(r, kind):
    """A 4 x 4 matrix as float words. kind: "shadow" (a projection onto the ground along a light: y collapses), "turn", "free"."""
    if kind == "ident":
        m = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    elif kind == "shadow":
        lx, lz = r.uniform(-1, 1), r.uniform(-1, 1)
        ly = r.uniform(0.3, 1.0)
        m = [1, 0, 0, 0, -lx / ly, 0, -lz / ly, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    elif kind == "turn":
        t = r.uniform(0, 2 * math.pi)
        c, s_ = math.cos(t), math.sin(t)
        m = [c, 0, s_, 0, 0, 1, 0, 0, -s_, 0, c, 0, r.uniform(-5, 5), r.uniform(-5, 5), r.uniform(-5, 5), 1]
    else:
        m = [r.uniform(-1.5, 1.5) for _ in range(16)]
    return [fb(v) for v in m]


def make_world(r, **over):
    """The case's environment, height field, shadow object and final matrix."""
    width, height = pick(r, [(640, 480), (800, 600), (1024, 768), (1600, 1200), (2048, 1536), (320, 200), (97, 61)])
    base = 0x410100
    for bit in (0x2000, 0x80, 0x20000, 0x40000, 0x100000, 0x200, 0x400, 0x800):
        if r.random() < 0.25:
            base |= bit
    if r.random() < 0.05:
        base = r.getrandbits(32) & ~0x00000000
    env = dict(flags=base, aspect=fb(height / width) if r.random() < 0.8 else rfloat(r, 0.3, 2.0), width=width, height=height,
               f938=pick(r, [fb(0.0), rfloat(r, -2, 2), fb(0.5)]), ui=pick(r, [32, 32, 64, 16, 128, 8, 24, 48, 4, 2, 1]),
               renderer=pick(r, [3, 3, 4, 0, 5]), mask=pick(r, [MASK, MASK, 0x00FFFFFF, 0xFF000000, 0, 0x80808080]),
               lights=r.randrange(0, 5), current=r.randrange(0, 4))
    env.update({k: v for k, v in over.items() if k in env})
    cell = pick(r, [0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 3.0, 0.7, 5.5])
    columns, rows = r.randrange(1, 17), r.randrange(1, 17)
    margin = 4096
    count = margin + columns * rows + 4096
    heights = [fb(r.uniform(-8, 8)) for _ in range(count)]
    if r.random() < 0.3:
        for _ in range(8):
            heights[r.randrange(count)] = pick(r, SPECIAL_FLOATS[:12])
    field = dict(columns=columns, rows=rows, inv_x=fb(1.0 / cell), inv_z=fb(1.0 / (cell if r.random() < 0.7 else cell * 2)),
                 ox=fb(r.uniform(-50, 50)), oz=fb(r.uniform(-50, 50)), cx=fb(cell), cz=fb(cell), margin=margin)
    field["cz"] = fb(1.0 / bf(field["inv_z"]))
    kinds = ["shadow"] * 8 + ["turn", "free"]
    matrices = []
    for _ in range(4):
        matrices += lcg_matrix(r, pick(r, kinds))
    cross = []
    for _ in range(4):
        a = r.uniform(0, 2 * math.pi)
        cross += [fb(math.cos(a)), fb(0.0), fb(math.sin(a))] if r.random() < 0.8 else [fb(r.uniform(-1, 1)), fb(r.uniform(-1, 1)), fb(r.uniform(-1, 1))]
    shades = [r.choice([127, 100, 1, 0, 64, 255, r.getrandbits(8)]) for _ in range(4)]
    return dict(env=env, field=field, heights=heights, shadow=dict(matrices=matrices, cross=cross, shades=shades),
                final=lcg_matrix(r, "turn"))


# ---------------------------------------------------------------- operations


def block_sizes(r):
    return pick(r, [(1, 1), (2, 2), (5, 3), (8, 8), (16, 16), (31, 33), (32, 32), (33, 31), (48, 20), (64, 64), (65, 33), (100, 70),
                    (128, 64), (127, 127), (128, 128), (200, 100), (0, 0), (256, 256), (300, 300)])


def part_sizes(r, ui):
    """A used part around the cell size."""
    c = max(ui, 1)
    return pick(r, [(1, 1), (c - 1, c), (c, c), (c + 1, c), (c, c + 1), (2 * c, c), (c, 2 * c), (2 * c + 1, 2 * c - 1), (3 * c, 2 * c),
                    (r.randrange(1, 4 * c + 1), r.randrange(1, 4 * c + 1)), (0, 0), (0, c), (c, 0), (4 * c, 3 * c + 1), (33, 7), (70, 70)])


def sprite_op(r, ui=32):
    code = r.choice([10, 10, 12, 12, 12, 13, 13, 13, 14, 15, 11, 16, 16, 16, 16, 17, 17, 18, 19])
    k = r.randrange(BLOCKS)
    if code == 10:
        w, h = block_sizes(r)
        return (10, [k, w, h])
    if code == 12:
        w, h = part_sizes(r, ui)
        return (12, [k, w if r.random() < 0.97 else pick(r, [-1, 65535, 0x8000, 40000]), h])
    if code == 13:
        return (13, [k, pick(r, [0, 0, 0, 1, 5, -3, 100, 1000]), pick(r, [0, 0, 2, 7, -9, 64, 1000])])
    if code in (11, 14, 15):
        return (code, [k])
    if code == 16:
        return (16, [k, r.randrange(0, 9)] + entry_args(r))
    if code == 17:
        return (17, [r.randrange(-1, ENTRIES + 1)] + entry_args(r))
    if code == 18:
        w, h = pick(r, [(32, 32), (64, 16), (8, 8), (17, 33), (1, 1), (100, 50), (0, 0)])
        uw, uh = pick(r, [(w, h), (max(w - 3, 0), max(h - 1, 0)), (w + 5, h), (1, 1), (0, 0), (64, 64)])
        return (18, [r.randrange(3), r.randrange(-1, ENTRIES + 1), w, h, uw, uh,
                     pick(r, [0, -w // 2, 3, -7, w, -w, 100, -100]), pick(r, [0, -h // 2, 5, -3, h, -h])])
    return (19, [r.randrange(0, 5), r.randrange(-1, ENTRIES), r.randrange(-1, ENTRIES), r.randrange(-1, ENTRIES), r.randrange(-1, ENTRIES)])


def position(r):
    return [rfloat(r, -1.5, 1.5), rfloat(r, -1.5, 1.5), rfloat(r, 0.0, 1.0)] if r.random() < 0.85 else [special(r), special(r), special(r)]


def shadow_position(r):
    return [rfloat(r, -30, 30), rfloat(r, -5, 20), rfloat(r, -30, 30)]


HANG_Y = [0, 0x02000000, 0x04000000, 0x06000000, 0x01000000, 0x07000000, 0x03000000]
HANG_X = [0, 0x20000000, 0x40000000, 0x60000000, 0x10000000, 0x70000000, 0x30000000]


def instance_flags(r, shadows=0.5):
    flags = pick(r, HANG_Y) | pick(r, HANG_X)
    for bit in (0x10, 0x20, 0x80, 0x800, 0x20000, 0x4, 0x2000, 0x4000, 0x10000, 0x200000):
        if r.random() < 0.25:
            flags |= bit
    if r.random() < shadows:
        flags |= 0x200 | 0x10000000
    elif r.random() < 0.3:
        flags |= pick(r, [0x200, 0x10000000])
    return flags


def instance_op(r, shadows=0.5, partial=True):
    g = r.randrange(INSTANCES)
    sub = r.choice([0, 1, 1, 2, 3, 3, 4, 4, 5, 6, 7, 7, 8, 9])
    if sub == 0:
        return (20, [g, 0, instance_flags(r, shadows)])
    if sub == 1:
        return (20, [g, 1] + (position(r) if partial else shadow_position(r)))
    if sub == 2:
        return (20, [g, 2, pick(r, [MASK, 0xFF808080, 0, r.getrandbits(32)])])
    if sub == 3:
        return (20, [g, 3, pick(r, [fb(1.0), fb(10.0), rfloat(r, 0.1, 4.0), fb(0.0), special(r)] if partial else [fb(1.0), rfloat(r, 0.2, 2.0), fb(0.5)])])
    if sub == 4:
        return (20, [g, 4, pick(r, [0, 5, -5, 32, -32, 100, 0x7FFF, -0x8000, 17]), pick(r, [0, 7, -7, 32, 64, -64, 200, 33])])
    if sub == 5:
        return (20, [g, 5, pick(r, [fb(0.0), rfloat(r, -7, 7), special(r)])])
    if sub == 6:
        return (20, [g, 6, pick(r, [fb(0.0), rfloat(r, -3, 3), fb(1.0)])])
    if sub == 7:
        return (20, [g, 7, pick(r, [0, 1, 2, 3, 4, 5, 6, r.randrange(POOL), -1])])
    if sub == 8:
        return (20, [g, 8, r.randrange(2), r.randrange(0, 4), r.getrandbits(32) & 0x03030303])
    return (20, [g, 9, rfloat(r, -10, 10)])


def env_op(r, partial=True):
    sub = r.choice([0, 0, 1, 2, 3, 4, 5, 5, 6, 7, 8, 8, 9, 9])
    if sub == 0:
        flags = 0x410100
        for bit in (0x2000, 0x80, 0x20000, 0x40000, 0x100000):
            if r.random() < 0.3:
                flags |= bit
        return (100, [0, flags])
    if sub == 1:
        return (100, [1, rfloat(r, 0.3, 2.0)])
    if sub == 2:
        return (100, [2, pick(r, [640, 800, 1024, 2048, 1, 97])])
    if sub == 3:
        return (100, [3, pick(r, [480, 600, 768, 1536, 1, 61])])
    if sub == 4:
        return (100, [4, pick(r, [fb(0.0), rfloat(r, -2, 2)])])
    if sub == 5:
        return (100, [5, pick(r, [32, 16, 64, 8, 128, 48, 1, 4])])
    if sub == 6:
        return (100, [6, pick(r, [3, 4, 0, 5])])
    if sub == 7:
        return (100, [7, pick(r, [MASK, 0x00FFFFFF, 0, 0xFF000000, 0x80808080])])
    if sub == 8:
        return (100, [8, r.randrange(0, 4)])
    return (100, [9, r.randrange(0, 5)])


def draw_op(r, weights):
    code = r.choices([21, 22, 23, 24, 32], weights=weights)[0]
    g = r.randrange(INSTANCES)
    if code == 21 or code == 22:
        return (code, [g])
    if code == 23:
        return (23, [g, r.randrange(-1, ENTRIES)])
    if code == 24:
        return (24, [g, r.randrange(4), pick(r, [fb(1.0), rfloat(r, 0.3, 2.0), fb(-1.0), fb(0.0), fb(2.0)]),
                     pick(r, [fb(1.0), rfloat(r, 0.3, 2.0), fb(-1.0), fb(0.0), fb(0.5)])])
    return (32, [])


def manager_op(r):
    code = r.choice([25, 26, 26, 26, 27, 27, 27, 28, 28, 29, 30, 31])
    g = r.randrange(INSTANCES)
    if code in (26, 27):
        return (code, [g])
    if code == 31:
        return (31, [r.randrange(-1, ENTRIES)])
    return (code, [])


def prologue(r, env, shadows=0.6, partial=True):
    """A script that gives the world sprites, partial textures and instances worth drawing."""
    ui = env["ui"]
    script = [(19, [r.randrange(0, 5)] + [r.randrange(0, ENTRIES) for _ in range(4)])]
    for i in range(ENTRIES):
        script.append((17, [i] + entry_args(r)))
    for p in range(3):
        script.append(sprite_op_plain(r, p))
    for k in range(BLOCKS):
        if r.random() < 0.85:
            w, h = block_sizes(r)
            script.append((10, [k, max(w, 1), max(h, 1)]))
            uw, uh = part_sizes(r, ui)
            if r.random() < 0.6:
                uw, uh = min(uw, 3 * max(ui, 1)), min(uh, 3 * max(ui, 1))
            script.append((12, [k, uw, uh]))
            script.append((13, [k, pick(r, [0, 0, 3]), pick(r, [0, 0, 5])]))
            for i in range(r.randrange(0, 8)):
                script.append((16, [k, i] + entry_args(r)))
    for g in range(INSTANCES):
        script.append((20, [g, 7, r.randrange(POOL)]))
        script.append((20, [g, 0, instance_flags(r, shadows)]))
        script.append((20, [g, 1] + (position(r) if partial else shadow_position(r))))
        script.append((20, [g, 2, pick(r, [MASK, 0xFF808080, 0, r.getrandbits(32)])]))
        script.append((20, [g, 3, pick(r, [fb(10.0), fb(1.0), rfloat(r, 0.1, 4.0)] if partial else [fb(1.0), rfloat(r, 0.2, 2.0), fb(0.5)])]))
        if r.random() < 0.25:
            script.append((20, [g, 5, rfloat(r, -7, 7)]))
        else:
            script.append((20, [g, 4, pick(r, [0, 5, 32, 64, 100, 17, -5]), pick(r, [0, 7, 32, 33, 64, 20])]))
        script.append((20, [g, 6, pick(r, [fb(0.0), rfloat(r, -3, 3)])]))
        script.append((20, [g, 8, r.randrange(2), r.randrange(0, 4), r.getrandbits(32) & 0x03030303]))
        script.append((20, [g, 9, rfloat(r, -10, 10)]))
    return script


def sprite_op_plain(r, p):
    w, h = pick(r, [(32, 32), (64, 16), (8, 8), (17, 33), (100, 50), (40, 40), (24, 24)])
    uw, uh = pick(r, [(w, h), (max(w - 3, 1), max(h - 1, 1)), (w + 5, h), (1, 1), (64, 64), (33, 17)])
    return (18, [p, r.randrange(-1, ENTRIES), w, h, uw, uh, pick(r, [0, -w // 2, 3, -7, -w, 10]), pick(r, [0, -h // 2, 5, -3, -h])])


def queues(r):
    q = {}
    if r.random() < 0.3:
        q["DrawSprite"] = [1] * r.randrange(0, 6) + [pick(r, [0, 1, 2, -1])]
    if r.random() < 0.3:
        q["AddSprite"] = [1] * r.randrange(0, 6) + [pick(r, [0, 1, 3])]
    if r.random() < 0.15:
        q["Execute"] = [1] * r.randrange(0, 4) + [0]
    if r.random() < 0.15:
        q["DrawRegular"] = [1] * r.randrange(0, 4) + [0]
    if r.random() < 0.15:
        q["AddRegular"] = [1] * r.randrange(0, 4) + [0]
    if r.random() < 0.2:
        q["Malloc"] = [1 if r.random() < 0.7 else 0 for _ in range(r.randrange(1, 6))] + [1]
    return q


def random_case(r, weights, length, shadows=0.6, partial=True, manager=False):
    world = make_world(r)
    env = world["env"]
    script = prologue(r, env, shadows, partial)
    makers = [lambda r: sprite_op(r, env["ui"]), lambda r: instance_op(r, shadows, partial), env_op,
              lambda r: draw_op(r, weights[:5])] + ([manager_op] if manager else [])
    makers_weights = [3, 3, 1, weights[5]] + ([weights[6]] if manager else [])
    for _ in range(length):
        script.append(r.choices(makers, weights=makers_weights)[0](r))
    case = dict(script=script, q=queues(r))
    case.update(world)
    return case


# ---------------------------------------------------------------- edge cases


def fixed_world(seed, env=None, field=None, lights=None):
    """A world from the seed with some values chosen: env (overrides of the environment), field (overrides of the height field),
    lights ((matrix kind, cross angle, shade) for each light that exists)."""
    r = random.Random(seed)
    world = make_world(r)
    world["env"].update(dict(flags=0x410100, width=800, height=600, aspect=fb(0.75), renderer=3, mask=MASK, f938=fb(0.25), ui=32,
                             lights=1, current=0))
    world["env"].update(env or {})
    world["field"].update(field or {})
    if lights is not None:
        world["env"]["lights"] = len(lights)
        matrices, cross, shades = [], [], []
        for kind, angle, shade in lights:
            matrices += lcg_matrix(r, kind)
            cross += [fb(math.cos(angle)), fb(0.0), fb(math.sin(angle))]
            shades.append(shade)
        while len(shades) < 4:
            matrices += lcg_matrix(r, "shadow")
            cross += [fb(1.0), fb(0.0), fb(0.0)]
            shades.append(0)
        world["shadow"] = dict(matrices=matrices, cross=cross, shades=shades)
    return world


def inst_ops(g, sprite, flags=0, pos=(0.0, 0.0, 0.5), colour=MASK, scale=10.0, used=(0, 0), rotation=None, lift=None, frames=None,
             depth=None):
    """The operations that set up instance g."""
    ops = [(20, [g, 7, sprite]), (20, [g, 0, flags]), (20, [g, 1] + [fb(v) for v in pos]), (20, [g, 2, colour]), (20, [g, 3, fb(scale)])]
    ops.append((20, [g, 4, used[0], used[1]]) if rotation is None else (20, [g, 5, fb(rotation)]))
    if lift is not None:
        ops.append((20, [g, 6, fb(lift)]))
    if frames is not None:
        ops.append((20, [g, 8, frames[0], frames[1], frames[2]]))
    if depth is not None:
        ops.append((20, [g, 9, fb(depth)]))
    return ops


def block_ops(k, size, used, at=(0, 0)):
    """Block sprite k: its pixels, the used part and the partial textures."""
    return [(10, [k, size[0], size[1]]), (12, [k, used[0], used[1]]), (13, [k, at[0], at[1]])]


def entry_ops(k, count, entries):
    ops = []
    for i in range(count):
        flags, page, u0, v0, us, vs = entries(i)
        ops.append((16, [k, i, flags, page, fb(u0), fb(v0), fb(us), fb(vs)]))
    return ops


def cells(used, ui):
    return -(-used[0] // ui) * -(-used[1] // ui) if used[0] > 0 and used[1] > 0 else 0


def entry_style(variant):
    """An entry placement as a function of the index: the page, the 0x1000 and 0x100000 flags vary by variant."""
    def make(i):
        flags = [0x1000 | 0x100000, 0x1000, 0x100000, 0, 0x1000 | 0x300, 0x1000 | 0x100000 | 0x200, 0x1000 | 0x100 | 0x4000000,
                 0x30000 | 0x1000][(i + variant) % 8]
        page = 1 + (i * 3 + variant) % PAGES if flags & 0x1000 or variant % 2 else 0
        return flags, page, 0.0625 * (i % 4), 0.125 * (i % 3), 0.0625 + 0.015625 * (i % 5), 0.125 + 0.03125 * (i % 2)
    return make


FUSED_ROWS = [(-0.0254720803, 10.9275961, 2491, 600), (-0.422982275, 1.08046377, 2006, 1080), (-1.09823716, 8.24718285, 2317, 768),
              (0.740667582, 5.42614746, 2883, 768), (1.21452606, 6.12127161, 772, 768), (-1.19450068, 6.61272478, 2224, 1200),
              (-1.05478227, 1.60632277, 1990, 1080), (1.0670743, 6.31728125, 1052, 1080), (-0.140559465, 10.8014793, 2255, 1200),
              (-0.542611659, 5.01385021, 2728, 600), (-0.595199883, 10.7289963, 1173, 1536), (-0.0440964065, 6.16223621, 2724, 768)]


def edge_partial():
    r = random.Random(41)
    cases = []
    # Used parts below, at and above the cell in each direction (the last row and column are partial cells).
    for ui in (32, 16, 24):
        sizes = sorted({1, ui - 1, ui, ui + 1, 2 * ui - 1, 2 * ui, 2 * ui + 1, 3 * ui})
        for w in sizes:
            for h in sizes:
                world = fixed_world(w * 131 + h, env=dict(ui=ui, flags=0x410100 | (0x2000 if (w + h) % 3 == 0 else 0)))
                n = cells((w, h), ui)
                script = block_ops(0, (3 * ui + 5, 3 * ui + 3), (w, h), (w % 5, h % 4)) + entry_ops(0, n, entry_style(w + h))
                script += inst_ops(0, 0, flags=pick(r, HANG_Y) | pick(r, HANG_X), pos=(r.uniform(-1, 1), r.uniform(-1, 1), r.uniform(0, 1)),
                                   scale=r.choice([1.0, 10.0, 3.5]), used=(r.choice([0, 7, -9, 40]), r.choice([0, 5, 33])))
                script += [(21, [0]), (21, [0])]
                cases.append(dict(script=script, q=({"DrawSprite": [1, 1, 0]} if (w + h) % 5 == 0 else {}), **world))
    # Many entries: as many cells as the generator allows (64), in a square, a row and a column.
    for ui, used in ((8, (64, 64)), (16, (128, 128)), (4, (256, 4)), (4, (4, 256)), (1, (8, 8)), (2, (16, 8)), (8, (63, 57))):
        world = fixed_world(ui * 3 + used[0], env=dict(ui=ui, flags=0x410100 | 0x2000))
        n = cells(used, ui)
        script = block_ops(0, (min(used[0] + 3, 300), min(used[1] + 5, 200)), used, (2, 3)) + entry_ops(0, n, entry_style(ui))
        script += inst_ops(0, 0, flags=0x22000000 | 0x20000000, pos=(0.0, 0.0, 0.5), scale=4.0, used=(30, 40)) + [(21, [0])]
        cases.append(dict(script=script, q={"DrawSprite": [1] * 70}, **world))
    # The vertical offset is one fused multiply (fnmsubs); two operations (the product rounded first) move the quad to another pixel
    # row for these: position y, scale, the instance's height, the display's.
    for k, (pos_y, scale, height, display) in enumerate(FUSED_ROWS):
        world = fixed_world(k + 900, env=dict(width=800, height=display))
        script = [(18, [0, 0, 32, 32, 8, 8, 0, 0])] + inst_ops(0, 4, flags=0, pos=(0.1, pos_y, 0.5), scale=scale, used=(0, height)) + [(21, [0])]
        cases.append(dict(script=script, q={}, **world))
    # Every hang flag combination, with a pixel offset and a scale.
    for hy in HANG_Y:
        for hx in HANG_X:
            world = fixed_world(hy + hx, env=dict(ui=32, aspect=fb(0.75)))
            script = block_ops(1, (70, 40), (65, 33)) + entry_ops(1, 6, entry_style(3))
            script += inst_ops(0, 1, flags=hy | hx, pos=(0.2, -0.3, 0.7), scale=2.0, used=(9, 12))
            script += inst_ops(1, 1, flags=hy | hx, pos=(-0.5, 0.5, 0.1), scale=10.0, used=(-20, -7), colour=0xFF00FF00)
            script += [(21, [0]), (21, [1])]
            cases.append(dict(script=script, q={}, **world))
    # The draw primitive gives up at the k-th quad: the early return (always 1) and the cells after it are not drawn.
    for k in range(0, 7):
        for answer in (0, 2, -1):
            world = fixed_world(k + 7)
            script = block_ops(2, (96, 96), (70, 66)) + entry_ops(2, 9, entry_style(k)) + inst_ops(0, 2, scale=4.0, used=(3, 3))
            script += [(21, [0]), (21, [0])]
            cases.append(dict(script=script, q={"DrawSprite": [1] * k + [answer]}, **world))
    # The cache's size, the renderer and the fog bit, with a light mask the colour is ANDed with at every quad.
    for ui in (1, 2, 4, 8, 48):
        for mask in (MASK, 0x00FFFFFF, 0xFF000000, 0):
            world = fixed_world(ui * 7 + mask % 11, env=dict(ui=ui, mask=mask, flags=0x410100 | 0x2000, renderer=r.choice([3, 4])))
            used = (min(2 * ui + 1, 70), min(ui + 1, 40))
            n = cells(used, ui)
            script = block_ops(3, (80, 50), used) + entry_ops(3, n, entry_style(ui)) + inst_ops(0, 3, colour=0xFFFFFFFF, scale=1.0)
            script += [(21, [0])]
            cases.append(dict(script=script, q={}, **world))
    # No texture: a sprite that was never cut (at most one cell, drawn with the texture coordinates zero), one cut into none,
    # a plain sprite over the standalone entries (several cells walk them in order), the entry flags the draw reads.
    for used in ((0, 0), (1, 1), (32, 32), (33, 32), (16, 5), (0, 9)):
        world = fixed_world(sum(used) + 3)
        script = [(10, [0, 64, 64]), (12, [0, used[0], used[1]])] + inst_ops(0, 0, scale=3.0, used=(4, 4)) + [(21, [0])]
        script += [(13, [0, 0, 0])] + entry_ops(0, cells(used, 32), entry_style(1)) + [(21, [0])]
        cases.append(dict(script=script, q={}, **world))
    for first in range(ENTRIES):
        for used in ((10, 10), (64, 32), (65, 32), (32, 96), (100, 70)):
            world = fixed_world(first * 17 + used[0])
            script = [(17, [i] + [entry_flags(r), 1 + i, fb(0.1 * i), fb(0.05 * i), fb(0.25), fb(0.125)]) for i in range(ENTRIES)]
            script += [(18, [0, first, 100, 100, used[0], used[1], 0, 0])] + inst_ops(0, 4, flags=0x23000000, scale=5.0, used=(3, 3))
            script += [(21, [0])]
            cases.append(dict(script=script, q={}, **world))
    # Wild values: positions and scale that are not numbers, a display of no size, and sprite sizes of zero.
    specials = [0, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x7F7FFFFF, 0x00000001]
    for i, x in enumerate(specials):
        for j, y in enumerate(specials):
            world = fixed_world(i * 11 + j, env=dict(width=pick(r, [800, 0, 1]), height=pick(r, [600, 0, 3])))
            script = block_ops(0, (64, 64), (40, 33)) + entry_ops(0, 4, entry_style(i)) + inst_ops(0, 0, flags=pick(r, HANG_Y) | pick(r, HANG_X))
            script += [(20, [0, 1, x, y, specials[(i + j) % len(specials)]]), (20, [0, 3, specials[(i * 2 + j) % len(specials)]]), (21, [0])]
            cases.append(dict(script=script, q={}, **world))
    for sizes in ((0, 0), (0, 40), (40, 0), (1, 1)):
        world = fixed_world(sizes[0] * 3 + sizes[1])
        script = [(18, [0, 0, sizes[0], sizes[1], 8, 8, 0, 0])] + inst_ops(0, 4, flags=0x77000000, scale=2.0, used=(5, 6)) + [(21, [0])]
        cases.append(dict(script=script, q={}, **world))
    return cases


GEOMETRIES = [   # a plain sprite's width, height, used width, used height, offset x and y
    (32, 32, 32, 32, -16, -16), (64, 16, 60, 12, 3, -5), (17, 33, 17, 33, -7, 12), (8, 8, 1, 1, 0, 0), (100, 50, 100, 50, -100, -50),
    (40, 40, 20, 30, 25, -2)]


def shadow_lights(r, count):
    return [(pick(r, ["shadow", "shadow", "turn", "free"]), r.choice([0.0, math.pi / 2, math.pi, 4.0, r.uniform(0, 6.28)]),
             r.choice([127, 100, 1, 0, 255, 77])) for _ in range(count)]


def edge_create_shadow():
    r = random.Random(42)
    cases = []
    # Flips, the turn (the fake turns a quarter), the lift, the light the cross product and shade come from, the sprite's geometry.
    for bits in range(16):
        flags = 0x200 | 0x10000000 | (0x10 if bits & 1 else 0) | (0x20 if bits & 2 else 0) | (0x80 if bits & 4 else 0) | (0x800 if bits & 8 else 0)
        for current in range(4):
            for sx, sz in ((1.0, 1.0), (2.0, 0.5), (-1.0, 1.0), (1.0, -2.0), (0.0, 0.0)):
                world = fixed_world(bits * 100 + current * 10 + int(sx * 3), lights=shadow_lights(r, 4), env=dict(current=current, f938=fb(r.choice([0.0, 0.5, -1.25]))))
                geometry = GEOMETRIES[(bits + current) % len(GEOMETRIES)]
                script = [(18, [0, 0] + list(geometry))] + inst_ops(0, 4, flags=flags, pos=(r.uniform(-20, 20), r.uniform(-3, 10), r.uniform(-20, 20)),
                                                                     scale=r.choice([1.0, 0.5, 2.0]), rotation=r.uniform(-4, 4), lift=r.choice([0.0, 1.5, -2.0]))
                script += [(24, [0, current, fb(sx), fb(sz)]), (24, [0, (current + 1) % 4, fb(sz), fb(sx)])]
                cases.append(dict(script=script, q={}, **world))
    # The turn: the angle the fake is given, whatever it is (it turns a quarter), with the flags that go with it.
    for k, angle in enumerate((0.0, -0.0, math.pi / 2, math.pi, -math.pi / 2, 2 * math.pi, 1e10, math.inf, -math.inf, math.nan, 1e-30, 7.0)):
        for flags in (0x200 | 0x10000000 | 0x80, 0x200 | 0x10000000 | 0x80 | 0x10 | 0x20 | 0x800):
            world = fixed_world(k * 3 + (flags & 0x10), lights=shadow_lights(r, 2))
            script = [(18, [0, 0] + list(GEOMETRIES[k % len(GEOMETRIES)]))]
            script += inst_ops(0, 4, flags=flags, pos=(r.uniform(-10, 10), 1.0, r.uniform(-10, 10)), scale=1.0, rotation=0.0, lift=0.75)
            script += [(20, [0, 5, struct.unpack(">I", struct.pack(">f", angle))[0]]), (24, [0, 0, fb(1.0), fb(1.0)]), (22, [0]), (23, [0, -1])]
            cases.append(dict(script=script, q={}, **world))
    # The corner against the edges of the height field: before its origin, on it, inside, at the last sample and beyond.
    for columns, rows in ((1, 1), (2, 2), (8, 8), (16, 3), (3, 16)):
        for cell in (0.5, 1.0, 4.0):
            for k in range(8):
                world = fixed_world(columns * 31 + rows + k, field=dict(columns=columns, rows=rows, inv_x=fb(1.0 / cell), inv_z=fb(1.0 / cell),
                                                                       cx=fb(cell), cz=fb(cell), ox=fb(-4.0), oz=fb(2.0)),
                                    lights=[("ident", 0.0, 100), ("ident", 1.0, 50)])
                spots = [-4.0 - 3 * cell, -4.0, -4.0 + 0.5 * cell, -4.0 + (columns - 1) * cell, -4.0 + columns * cell, -4.0 + (columns + 2) * cell, -4.0 + 0.999 * cell, -4.0 + cell]
                spots_z = [2.0 - 2 * cell, 2.0, 2.0 + 0.25 * cell, 2.0 + (rows - 1) * cell, 2.0 + rows * cell, 2.0 + (rows + 3) * cell, 2.0 + cell, 2.0 + 0.5 * cell]
                script = [(18, [0, 0, 8, 8, 8, 8, 0, 0])] + inst_ops(0, 4, flags=0x200 | 0x10000000, pos=(spots[k], 0.5, spots_z[(k * 3) % 8]), scale=0.001,
                                                                     rotation=0.0)
                script += [(24, [0, 0, fb(1.0), fb(1.0)]), (100, [8, 1]), (24, [0, 1, fb(1.0), fb(1.0)])]
                cases.append(dict(script=script, q={}, **world))
    return cases


def edge_draw_shadow(add):
    r = random.Random(43 + add)
    cases = []
    op = 23 if add else 22
    # Which flags make a shadow, how many lights, the display's frame mode and the picture each shadow takes.
    for flag_bits in range(8):
        cast = 0x200 if flag_bits & 1 else 0
        shadows = 0x10000000 if flag_bits & 2 else 0
        blend = 0x20000 if flag_bits & 4 else 0
        for lights in range(5):
            for frame_mode in (0, 0x100000):
                for fail in (None, 0, 1, 2):
                    if fail is not None and lights == 0:
                        continue
                    world = fixed_world(flag_bits * 1000 + lights * 10 + fail_code(fail), lights=shadow_lights(r, lights),
                                        env=dict(flags=0x410100 | frame_mode | pick(r, [0, 0x2000])))
                    geometry = GEOMETRIES[(flag_bits + lights) % len(GEOMETRIES)]
                    script = [(17, [i] + entry_args(r, 0.8)) for i in range(ENTRIES)]
                    script += [(18, [0, r.choice([-1, 0, 1, 2]) if frame_mode == 0 else r.choice([-1, 3])] + list(geometry)),
                               (19, [r.choice([0, 1, 2, 3, 4])] + [r.choice([-1, 0, 1, 2, 3, 4, 5]) for _ in range(4)])]
                    script += inst_ops(0, 4, flags=cast | shadows | blend | r.choice([0, 0x10, 0x20, 0x80, 0x800]), pos=(r.uniform(-20, 20), r.uniform(0, 8), r.uniform(-20, 20)),
                                       scale=r.choice([1.0, 0.5]), rotation=r.uniform(-3, 3), lift=r.choice([0.0, 1.0]),
                                       frames=(1, r.choice([0, 1, 2, 3, 5]), r.getrandbits(32) & 0x03030303))
                    script += [(op, [0, r.choice([-1, -1, 0, 3, 5])] if add else [0]), (op, [0, 1] if add else [0])]
                    queue = {} if fail is None else {("AddSprite" if add else "DrawSprite"): [1] * fail + [0]}
                    cases.append(dict(script=script, q=queue, **world))
    # The frame of each light against the file's count: below, at and above it (the file's first sprite when it is not below).
    if not add:
        for count in range(0, 5):
            for delta in (-1, 0, 1):
                for pattern in (0x00000000, 0x03020100, 0x00010203):
                    base = max(count + delta, 0)
                    world = fixed_world(count * 31 + base + pattern % 7, lights=shadow_lights(r, 4), env=dict(flags=0x410100 | 0x100000))
                    script = [(17, [i] + entry_args(r, 1.0)) for i in range(ENTRIES)]
                    script += [(18, [0, 0] + list(GEOMETRIES[0])), (19, [count, 1, 2, 3, 4])]
                    script += inst_ops(0, 4, flags=0x10000200, pos=(1.0, 2.0, 3.0), scale=1.0, rotation=0.0, frames=(1, base, pattern))
                    script += [(22, [0])]
                    cases.append(dict(script=script, q={}, **world))
    # A sprite with no picture at all (NULL entry) and one with partial textures as its picture.
    for add_arg in range(-1, 3):
        world = fixed_world(add_arg + 500, lights=shadow_lights(r, 2))
        script = [(18, [0, -1, 32, 32, 32, 32, -16, -16]), (10, [0, 64, 64]), (12, [0, 40, 40]), (13, [0, 0, 0])] + entry_ops(0, 4, entry_style(2))
        script += inst_ops(0, 4, flags=0x10000200, pos=(1.0, 2.0, 3.0), scale=1.0, rotation=1.0)
        script += inst_ops(1, 0, flags=0x10000200, pos=(-1.0, 2.0, 5.0), scale=1.0, used=(32, 32))
        script += [(18, [1, 0, 32, 32, 32, 32, -16, -16])] + inst_ops(2, 5, flags=0x10000200, pos=(0.0, 1.0, 1.0), scale=1.0, used=(0, 0))
        script += [(op, [0, add_arg] if add else [0]), (op, [1, add_arg] if add else [1]), (op, [2, add_arg] if add else [2])]
        cases.append(dict(script=script, q={}, **world))
    return cases


def fail_code(fail):
    return 9 if fail is None else fail


def edge_manager():
    """The frame through the real manager: every display flag that changes the sprite pass (one by one, no shadows, one blob shadow
    texture), the blob texture present or not, and a failure of each draw call at the first and the third time it is asked."""
    r = random.Random(44)
    cases = []
    failures = [None] + [(name, k) for name in ("DrawSprite", "AddSprite", "Execute", "DrawRegular", "AddRegular") for k in (0, 2)]
    for display in (0, 0x80, 0x20000, 0x40000, 0x80 | 0x20000, 0x80 | 0x40000, 0x40000 | 0x20000, 0x100000, 0x100000 | 0x80):
        for blob in (-1, 0, 4):
            for failure in failures:
                world = fixed_world(display + blob * 3 + (len(failures) if failure is None else failures.index(failure)),
                                    lights=shadow_lights(r, r.randrange(1, 4)), env=dict(flags=0x410100 | display | pick(r, [0, 0x2000])))
                script = [(17, [i] + entry_args(r, 0.8)) for i in range(ENTRIES)]
                script += [(18, [p, p + 1] + list(GEOMETRIES[p + 1])) for p in range(3)] + [(19, [3] + [r.randrange(-1, ENTRIES) for _ in range(4)])]
                script += block_ops(0, (80, 70), (70, 40)) + entry_ops(0, 6, entry_style(display % 7))
                for g in range(3):
                    script += inst_ops(g, 4 + g, flags=0x200 | 0x10000000 | pick(r, [0, 4, 0x2000 | 4, 0x200000, 0x20000, 0x4000, 0x10000]),
                                       pos=(r.uniform(-10, 10), r.uniform(0, 5), r.uniform(-10, 10)), scale=1.0, rotation=r.uniform(-2, 2),
                                       frames=(1, g, 0x03020100), depth=r.choice([1.0, 2.0, 2.0, -3.0]))
                for g in (3, 4):
                    script += inst_ops(g, 0, flags=pick(r, HANG_Y) | pick(r, HANG_X), pos=(r.uniform(-1, 1), r.uniform(-1, 1), 0.5), scale=2.0, used=(1, 2))
                script += [(31, [blob]), (25, [])] + [(26, [g]) for g in range(3)] + [(27, [3]), (27, [4]), (28, []), (29, []), (30, [])]
                script += [(25, [])] + [(26, [g]) for g in (2, 0)] + [(28, [])]
                queue = {} if failure is None else {failure[0]: [1] * failure[1] + [0]}
                cases.append(dict(script=script, q=queue, **world))
    return cases


# ---------------------------------------------------------------- tests


def rnd_partial(r):
    return random_case(r, [10, 1, 1, 1, 1, 6, 0], 25)


def rnd_create_shadow(r):
    return random_case(r, [0, 0, 0, 10, 1, 5, 0], 25, shadows=0.9, partial=False)


def rnd_draw_shadow(r):
    return random_case(r, [0, 10, 0, 1, 0, 5, 0], 25, shadows=0.9, partial=False)


def rnd_add_shadow(r):
    return random_case(r, [0, 0, 10, 1, 0, 5, 0], 25, shadows=0.9, partial=False)


def rnd_manager(r):
    return random_case(r, [3, 3, 3, 1, 1, 3, 3], 200, shadows=0.7, partial=True, manager=True)


TESTS = [
    FnTest("SSpriteGameData::DrawPartial", ppc, host, edge=edge_partial, rnd=rnd_partial),
    FnTest("SSpriteGameData::CreateShadow", ppc, host, edge=edge_create_shadow, rnd=rnd_create_shadow),
    FnTest("SSpriteGameData::DrawRegularShadow", ppc, host, edge=lambda: edge_draw_shadow(0), rnd=rnd_draw_shadow),
    FnTest("SSpriteGameData::AddRegularShadow", ppc, host, edge=lambda: edge_draw_shadow(1), rnd=rnd_add_shadow),
    FnTest("random scripts of 200 operations through CSpriteManager::Render", ppc, host, edge=edge_manager, rnd=rnd_manager),
]
