"""The sprite classes' remaining members (original tb_decode_rle.cpp, tb_sprite.cpp and tb_spritelist.cpp), engine library:
CRLELoad (the destructor, Close, Decompress, Blit, AverageColor), CSpriteList (Close, Prepare, GetData, GetLinkType),
SSpriteEngineData (Init(char*), SetUsedPart, GeneratePartialTexture, DestroyPartialTexture) and CSpriteFileIO::Load(char*, int).
The members w33b translated (tests/test_SpriteFileIO.py) run here too: the list's and the loader's destructors now run the real
Close, and the script tests run all of them together. The texture entry (CTextureEntry) is the real one on both sides.

Two kinds of test. (1) A script of operations run on both sides on the same set of objects (two sprite file objects, four sprites,
three sprite lists, an RLE loader and a standalone array of texture entries) and a file of the case's bytes; after every operation
the test compares the result, the events of the fakes (the engine allocator, the file callbacks, the texture cache's Remove, the
error handler, the assertion, the splash, the link base table, the palette converter, the sprite manager's AddList, the TGA loader)
and the state of every object, read in the same order as tests/unit_SpriteRest.cpp writes the host's. The heap traffic (operator
new, new[], delete, delete[]) is part of the state. (2) The RLE unpacking on its own (Decompress and Blit): a loader, a palette and a
destination built from the case's numbers on both sides, then the answer, the destination's bytes (all of them, with the guard around
them), the loader's words and the events compared. The pictures are the game's own sprites (every size the files hold) and synthetic
streams (every token, a run past the row end, a picture of no height or width, palettes of one and of 256 colours).

The file. All fields of a kind 3 file are little-endian and swapped by the original, so both sides read the same bytes. (Kind 2
files, with their raw headers, are tests/test_SpriteFileIO.py's.)
"""
import ctypes as C
import pathlib
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_SpriteRest"

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent

IO = dict(Load=0x1002DA98, Free=0x1002DF78, Flag=0x1002E068, LoadName=0x1002DEA0)
SPRITE = dict(ctor=0x1002E0E0, dtor=0x1002E138, CreateTextureEntry=0x1002E1C4, Load_WCT=0x1002E33C, Load_RLE=0x1002E514,
              InitName=0x1002E810, Init=0x1002EA18, SetUsedPart=0x1002EB00, Generate=0x1002EBB8, Destroy=0x1002EE28,
              Refresh=0x1002EF10, Close=0x1002F00C)
LIST = dict(ctor=0x1003108C, dtor=0x10031160, Init=0x10031378, Close=0x10031408, Prepare=0x10031488, GetData=0x10031FB0,
            GetLinkType=0x10031FE4)
RLE = dict(ctor=0x10005104, dtor=0x10005244, SetSize=0x10005300, SetPalette=0x10005334, LoadSprite=0x10005368, Close=0x10005440,
           Decompress=0x100054C0, AverageColor=0x10005B5C, Blit=0x10005B90)
RLE_THUNKS = [0x10005C34, 0x10005C48, 0x10005C5C, 0x10005C70]   # the CCompBase vtable's entries of the loader
WAVE_SPRITE = 0x10006260
ENTRY = dict(ctor=0x10040D30, Create=0x10040E84, Close=0x10040F7C, GetBlendingMode=0x100410A4, BestSize=0x1004117C,
             GetBestSize=0x10041204, GetData=0x10041274, GetCorrectedData=0x10041334, SetSize=0x10041560,
             FlushEntryFlags=0x1004168C, Lock=0x10041738, Unlock=0x100417C0, StoreAsWrapping=0x10041848,
             GetVSize=0x10025D60, GetUSize=0x10025D98, GetVOffset=0x10025DD0, GetUOffset=0x10025E08,
             GetTexture=0x10025E40)
WAVE = dict(ctor=0x10005DE4, dtor=0x10005EA8, LoadTexture=0x10005F68, Close=0x1000640C, Decompress=0x100064B8,
            AverageColor=0x1000660C, Blit=0x10006644, IsNaturallyFlipped=0x100067B8)
INNER = [0x10040DFC, 0x100051D0, 0x10005BF8]
ERROR_PUSH, CACHE_REMOVE, SPLASH, TGA, ADD_LIST = 0x10008904, 0x10034610, 0x1002D7A0, 0x100156D8, 0x1003195C
REGISTER_LINK, FREE_LINK = 0x10019700, 0x10019768
P_MALLOC, P_FREE, FILE_OPEN, FILE_READ, FILE_CLOSE = 0x10051F70, 0x10051F74, 0x101735E4, 0x101735D8, 0x101735E0
SYSTEM, SETTINGS, G_TCACHE, HEADER = 0x10165F80, 0x10165FC8, 0x10173184, 0x10052AD4
DISPLAY, G_MANAGER = 0x10165634, 0x10173154
RLE_VTABLE = 0x1005164C
ENTRY_CTOR_TV, ENTRY_DTOR_TV = 0x100508B0, 0x100508A8
K_ON_PAGE, K_COMPRESSED = 0x1000, 0x10000000
MASK64 = 0xFFFFFFFFFFFFFFFF
MASK48 = 0xFFFFFFFFFFFF
FNV = 0xCBF29CE484222325

SLOTS, LISTS = 4, 3
PIXEL_BYTES = 0x40
SPRITE_SIZE, ENTRY_SIZE = 0x18, 0x5C
CACHE_SIZE = 0x1900   # the texture cache's UI texture size is the word at 0x18d8
UI_SIZE = 0x18D8
NAME_BLOCK = b"X\0SPR_A.TGA\0SPR_A.TGB\0spr_a.tgb\0SPR_AB\0spr_ab\0b\0B\0\0x.tgaB.\0AB.TGAb \0"
NAMES = 10


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if not hasattr(e, "sr_fns"):
        route = lambda name: (lambda emu: getattr(emu.sr_side, name)())
        e.sr_fns = {name: T.fake_function(e, route(name)) for name in ("cb_malloc", "cb_free", "cb_open", "cb_read", "cb_close")}


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def pattern(n, mul, add):
    return bytes((i * mul + add) & 0xFF for i in range(n))


def callback_stub(fn):
    """An import stub that calls PowerPC code back (tpwtest.stub_cb, but for one case's stubs): fn(emu, call)."""
    def hook(e):
        e.pending_cb = fn
        e.uc.reg_write(T.P.UC_PPC_REG_PC, T.ppcemu.RETURN_ADDR)
    return hook


class PpcSide:
    def __init__(self, e, case):
        self.e = e
        e.sr_side = self
        self.file, self.position = bytes(case["file"]), 0
        self.io = [e.alloc(0x10, bytes(0x10)) for _ in range(2)]
        self.sprites = [e.alloc(SPRITE_SIZE, b"\xa5" * SPRITE_SIZE) for _ in range(3)] + [0]
        self.lists = [e.alloc(0x28, b"\xa5" * 0x28) for _ in range(2)] + [0]
        self.rle_static = e.alloc(0x20, b"\xa5" * 0x20)
        self.rle = self.rle_static
        self.cache, self.handle, self.manager = e.alloc(CACHE_SIZE), e.alloc(8), e.alloc(16)
        self.pix = [e.alloc(PIXEL_BYTES, pattern(PIXEL_BYTES, 7, 1)), e.alloc(PIXEL_BYTES, pattern(PIXEL_BYTES, 5, 3))]
        self.palette = e.alloc(0x400, pattern(0x400, 3, 9))
        block = e.alloc(len(NAME_BLOCK), NAME_BLOCK)
        self.names, p = [], block + 2
        for _ in range(NAMES):
            self.names.append(p)
            p += len(e.cstr(p)) + 1
        fixed = {self.cache: 3, self.manager: 4, self.pix[0]: 5, self.pix[1]: 6, self.handle: 9, self.rle_static: 11, self.palette: 13,
                 self.lists[0]: 31, self.lists[1]: 32}
        self.L = CL.PpcLog(fixed, case.get("q"))
        e.cl = self.L
        self.built = [False] * SLOTS
        self.own, self.len, self.base = [0] * SLOTS, [0] * SLOTS, [0] * SLOTS
        self.list_built = [False] * LISTS
        self.rle_built = self.rle_heap = self.rle_used = False
        self.E, self.e_count = 0, 0
        self.heap = [0, 0, 0, 0]
        self.arrays = []             # (block, size) of every array of entries or sprites made
        self.pixels = {}             # the palette converter's objects: address -> [data, format]
        self.unit = 1

    # ---- the engine's callbacks
    def malloc(self, size):
        L = self.L
        L.rec("Malloc", (size // self.unit,))
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

    def cb_open(self):
        e, L = self.e, self.L
        L.rec("Open", (), (e.cstr(e.r(3)), e.cstr(e.r(4))))
        e.set_r(3, self.handle if L.ret("Open", 1) else 0)

    def cb_read(self):
        e, L = self.e, self.L
        dest, size, count = e.r(3), e.r(4), e.r(5)
        L.rec("Read", (size, count, L.lab(e.r(6))))
        data = self.file[self.position:self.position + size * count]
        if data:
            e.write(dest, data)
        self.position += len(data)
        e.set_r(3, len(data) // size if size else 0)

    def cb_close(self):
        self.L.rec("CloseFile", (self.L.lab(self.e.r(3)),))
        self.e.set_r(3, 0)

    # ---- the heap
    def new_scalar(self, size):
        size = max(size, 4)
        block = self.e.alloc(size, b"\xa5" * size)
        self.L.newlab(block)
        self.heap[0] += 1
        return block

    def new_array_block(self, size):
        size = max(size, 4)
        block = self.e.alloc(size, b"\xa5" * size)
        self.L.newlab(block)
        self.arrays.append((block, size))
        self.heap[1] += 1
        return block

    def new_entries(self, n):
        """What `new CTextureEntry[n]` does: the block, its cookie, the constructors."""
        e = self.e
        raw = self.new_array_block(8 + ENTRY_SIZE * n)
        w32(e, raw, ENTRY_SIZE)
        w32(e, raw + 4, n)
        for i in range(n):
            T.call_ppc(e, ENTRY["ctor"], [raw + 8 + i * ENTRY_SIZE, 1])
        return raw + 8

    def entry_label(self, a):
        """An entry is named by the label of the live array that holds it times 100 plus its index."""
        if a == 0:
            return 0
        for block, size in reversed(self.arrays):
            if self.L.lab(block) <= 0 or not (block + 8 <= a < block + size):
                continue
            if (a - block - 8) % ENTRY_SIZE:
                continue
            return self.L.lab(block) * 100 + (a - block - 8) // ENTRY_SIZE
        return -1

    def array_label(self, first):
        return 0 if first == 0 else self.L.lab(first - 8)

    def stubs(self):
        L = self.L
        st = CL.Stubs()
        st.real = (set(IO.values()) | set(SPRITE.values()) | set(LIST.values()) | set(RLE.values()) | set(RLE_THUNKS)
                   | {WAVE_SPRITE} | set(ENTRY.values()) | set(WAVE.values()) | set(INNER))
        st.internal(ERROR_PUSH, CL.recorder("Error", "xii"))
        st.internal(CACHE_REMOVE, lambda e: (L.rec("Cache.Remove", (L.lab(e.r(3)), self.entry_label(e.r(4)))), e.set_r(3, 1)))
        st.internal(SPLASH, CL.recorder("Splash.Interpolate", ""))
        st.internal(REGISTER_LINK, lambda e: (L.rec("RegisterLinkBase", (L.lab(e.r(3) - 0x20),)), e.set_r(3, 1)))
        st.internal(FREE_LINK, lambda e: (L.rec("FreeLinkBase", (L.lab(e.r(3) - 0x20),)), e.set_r(3, 1)))
        st.internal(ADD_LIST, CL.recorder("AddList", "LL", ret=lambda e, L: L.ret("AddList", 1)))

        def tga(e):
            name, width, height, flags, palette, bytes_ = e.cstr(e.r(3)), e.r(4), e.r(5), T.s32(e.r(6)), e.r(7), T.s32(e.r(8))
            L.rec("TGA", (flags, L.lab(palette), bytes_), (name,))
            if not L.ret("TGA", 1):
                e.set_r(3, 0)
                return
            w32(e, width, L.ret("TGAw", 29))
            w32(e, height, L.ret("TGAh", 28))
            e.set_r(3, self.malloc(16))
        st.internal(TGA, tga)
        st.imp("__assertion_failed", lambda e: L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))

        def op_new(e):
            e.set_r(3, self.new_scalar(e.r(3)))

        def op_new_array(e):
            e.set_r(3, self.new_array_block(e.r(3)))

        def op_delete(e):
            block = e.r(3)
            if block:
                self.heap[2] += 1
                if L.lab(block) > 0:
                    L.rec("delete", (L.lab(block),))
                L.forget(block)

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
            if L.lab(base - 8) > 0:
                L.rec("delete", (L.lab(base - 8),))
            L.forget(base - 8)
            return None

        st.imp("__nw__FUl", op_new)
        st.imp("__nwa__FUl", op_new_array)
        st.imp("__dl__FPv", op_delete)
        st.imp("__construct_new_array", callback_stub(construct))
        st.imp("__destroy_new_array", callback_stub(destroy))

        def swap16(e):
            a, n = e.r(3), T.s32(e.r(4))
            for i in range(max(n, 0)):
                e.write(a + 2 * i, e.read(a + 2 * i, 2)[::-1])
        st.imp("_USwapBlock16__13SamsUtilitiesFPUsl", swap16)

        # The palette converter.
        def pixels_ctor(e):
            this, fmt, data, w, h, stride, palette = (e.r(i) for i in range(3, 10))
            L.rec("Pixels.ctor", (L.newlab(this), T.s32(fmt), L.lab(data), T.s32(w), T.s32(h), T.s32(stride), L.lab(palette)))
            self.pixels[this] = [data, T.s32(fmt)]

        def pixels_dtor(e):
            L.rec("Pixels.dtor", (L.lab(e.r(3)),))
            self.pixels.pop(e.r(3), None)
            L.forget(e.r(3))

        def pixels_convert(e):
            dest, source = e.r(3), e.r(4)
            L.rec("Pixels.Convert", (L.lab(dest), L.lab(source), e.r(5) & 0xFF))
            if dest not in self.pixels or source not in self.pixels:
                return
            data, fmt = self.pixels[dest]
            count = 0x100 * (2 if fmt == 4 else 4 if fmt == 10 else 0)
            if count:
                old = e.read(data, count)
                e.write(data, bytes((old[i] + fmt + i) & 0xFF for i in range(count)))
        st.imp("__ct__Q213SamsUtilities7UPixelsFQ213SamsUtilities11PixelFormatPvUlUlUlPQ213SamsUtilities6UPixel", pixels_ctor)
        st.imp("__dt__Q213SamsUtilities7UPixelsFv", pixels_dtor)
        st.imp("Convert__Q213SamsUtilities7UPixelsFRCQ213SamsUtilities7UPixelsUc", pixels_convert)
        return st

    def reset(self):
        e = self.e
        fns = e.sr_fns
        for a, v in ((P_MALLOC, fns["cb_malloc"]), (P_FREE, fns["cb_free"]), (FILE_OPEN, fns["cb_open"]),
                     (FILE_READ, fns["cb_read"]), (FILE_CLOSE, fns["cb_close"]), (SYSTEM, 0), (SETTINGS + 4, 4),
                     (G_TCACHE, self.cache), (G_MANAGER, self.manager), (DISPLAY + 0x80, 0), (DISPLAY + 0x84, 0)):
            w32(e, a, v)
        w32(e, self.cache + UI_SIZE, 32)
        e.write(HEADER, bytes(0x2c))

    # ---- one operation
    def call(self, addr, *args):
        return T.call_ppc(self.e, addr, list(args))

    def entry_arg(self, kind):
        if kind == 1 and self.E and self.e_count >= 2:
            return self.E
        if kind == 2 and self.E and self.e_count >= 2:
            return self.E + ENTRY_SIZE
        return 0

    def entry_arg_ok(self, kind):
        return kind == 0 or self.entry_arg(kind) != 0

    def part_count(self, s):
        return struct.unpack(">H", self.e.read(s + 0x16, 2))[0]

    def safe_to_free(self, slot, needs_texture):
        e, s = self.e, self.sprites[slot]
        count = self.part_count(s)
        if count == 0 or (needs_texture and e.u32(s) == 0):
            return True
        return self.own[slot] == 1 and count <= self.len[slot] and e.u32(s) != 0

    def safe_to_close(self, slot):
        """Close can run: the partial textures are safe to free, and a sprite without a pixel block closes the first entry of its
        texture, which an array of none does not have."""
        e, s = self.e, self.sprites[slot]
        if not self.safe_to_free(slot, True):
            return False
        return not (e.u32(s + 4) == 0 and e.u32(s) != 0 and self.len[slot] < 1)

    def parts_to_make(self, s):
        e = self.e
        size = T.s32(e.u32(self.cache + UI_SIZE))
        if size == 0:
            return -1
        used_w, used_h = struct.unpack(">HH", e.read(s + 0x10, 4))
        mask = (size - 1) & 0xFFFFFFFF

        def trunc_div(a, b):
            q = abs(a) // abs(b)
            return -q if (a < 0) != (b < 0) else q
        columns, rows = trunc_div(T.s32(used_w + mask), size), trunc_div(T.s32(used_h + mask), size)
        count = columns * rows
        if columns > 64 or rows > 64 or columns < -4096 or rows < -4096 or count < 0 or count > 64:
            return -1
        return count

    def op(self, op, a):
        e, L, u = self.e, self.L, self.e.u32
        k = a[0] & 1 if a else 0
        io = self.io[k]
        slot = a[0] & 3 if a else 0
        s = self.sprites[slot]
        if op == 1:
            e.write(io, bytes(0x10))
            return 0
        if op == 2:
            if T.s32(u(io)) != 0 or self.position != 0:
                return -8
            return T.s32(self.call(IO["Load"], io, self.handle)[0])
        if op == 3:
            return T.s32(self.call(IO["Free"], io)[0])
        if op == 4:
            count, sprites = T.s32(u(io)), u(io + 4)
            if count == 0 or sprites == 0:
                return -8
            if any(u(sprites + SPRITE_SIZE * i) == 0 for i in range(count)):
                return -8
            self.call(IO["Flag"], io)
            return 0
        if op == 5:
            self.position = min(a[1], len(self.file)) if a[1] >= 0 else len(self.file)
            return 0
        if op == 7:
            w32(e, SYSTEM, a[1])
            w32(e, SETTINGS + 4, a[2])
            return 0
        if op == 8:
            if T.s32(u(io)) != 0 or self.position != 0:
                return -8
            return T.s32(self.call(IO["LoadName"], io, self.names[a[1] % NAMES], a[2])[0])
        # ---- sprites
        if op == 10:
            if slot == 3:
                s = self.sprites[3] = self.new_scalar(SPRITE_SIZE)
            self.call(SPRITE["ctor"], s)
            self.built[slot] = True
            self.own[slot] = self.len[slot] = 0
            return 0
        if op == 11:
            if not self.built[slot] or self.own[slot] == 2 or not self.safe_to_close(slot):
                return -8
            self.call(SPRITE["dtor"], s, 1 if slot == 3 else 0)
            if slot == 3:
                self.sprites[3] = 0
            self.built[slot] = False   # what it pointed at may be gone
            self.own[slot] = self.len[slot] = self.base[slot] = 0
            return 0
        if op == 12:
            if not self.built[slot] or not self.entry_arg_ok(a[6]):
                return -8
            source = self.rle + 0x18 if a[3] == 1 and self.rle_built else 0
            if a[3] != 0 and (source == 0 or a[6] == 0):
                return -8
            entry = self.entry_arg(a[6])
            image = lambda kind: self.pix[kind - 1] if kind in (1, 2) else 0
            flags = (a[7] | K_COMPRESSED) if source else a[7]   # an entry that unpacks from a loader says so
            result = T.s32(self.call(SPRITE["CreateTextureEntry"], s, image(a[1]), image(a[2]), source, a[4], a[5], entry, flags)[0])
            self.own[slot] = 2 if entry else 1
            self.len[slot] = 1
            self.base[slot] = 0
            if source:
                self.rle_used = True
            return result
        if op == 14:
            if not self.built[slot] or not self.entry_arg_ok(a[2]) or self.position + 20 > len(self.file):
                return -8
            entry = self.entry_arg(a[2])
            palette = u(self.io[0] + 0xc) if a[1] == 1 else self.palette if a[1] == 2 else 0
            result = T.s32(self.call(SPRITE["Load_RLE"], s, self.handle, palette, entry)[0])
            self.own[slot] = 2 if entry else 1
            self.len[slot] = 1
            self.base[slot] = 0
            return result
        if op == 15:
            if not self.built[slot]:
                return -8
            return T.s32(self.call(SPRITE["Init"], s, a[1], a[2])[0])
        if op == 16:
            if not self.built[slot] or not self.safe_to_free(slot, True):
                return -8
            if u(s) != 0 and self.part_count(s) != 0:
                self.own[slot] = self.len[slot] = 0
            return T.s32(self.call(SPRITE["Refresh"], s)[0])
        if op == 17:
            if not self.built[slot] or self.own[slot] == 2 or not self.safe_to_close(slot):
                return -8
            result = T.s32(self.call(SPRITE["Close"], s)[0])
            if u(s) == 0:
                self.own[slot] = self.len[slot] = 0
            return result
        if op == 18:
            if not self.built[slot]:
                return -8
            block = self.malloc(0x100) if a[1] else 0
            w32(e, s + 4, block)
            n = a[2]
            texture = self.new_entries(n) if n > 0 else 0
            w32(e, s, texture)
            for i in range(max(n, 0)):
                entry = texture + ENTRY_SIZE * i
                w32(e, entry + 8, 0x5180200 | (K_ON_PAGE if (a[3] >> i) & 1 else 0))
                self.call(ENTRY["SetSize"], entry, 8 + i, 4 + i)
                e.write(entry + 0x30, struct.pack(">h", 64))
                w32(e, entry + 0x3c, block + 4 * i if block else 0)
            e.write(s + 0x16, struct.pack(">H", (n if a[4] < 0 else a[4]) & 0xFFFF))
            self.own[slot] = 1 if n > 0 else 0
            self.len[slot] = n if n > 0 else 0
            self.base[slot] = block
            return 0
        if op == 19:
            if not self.built[slot]:
                return -8
            w32(e, s, 0)
            self.own[slot] = self.len[slot] = 0
            return 0
        # ---- sprite lists
        if op in (30, 31, 32, 33, 34, 36, 37):
            lst = a[0] % 3
            addr = self.lists[lst]
            if op == 30:
                if lst == 2:
                    addr = self.lists[2] = self.new_scalar(0x28)
                self.call(LIST["ctor"], addr, 1)
                self.list_built[lst] = True
                return 0
            if not self.list_built[lst]:
                return -8
            if op == 31:
                self.call(LIST["dtor"], addr, 1 if lst == 2 else 0xFFFFFFFF)
                if lst == 2:
                    self.lists[2] = 0
                self.list_built[lst] = False
                return 0
            if op == 32:
                self.unit = 4
                result = self.call(LIST["Init"], addr, a[1])[0]
                self.unit = 1
                return L.lab(result)
            if op == 36:
                return self.call(LIST["GetData"], addr)[0]
            return T.s32(self.call(LIST[{33: "Close", 34: "Prepare", 37: "GetLinkType"}[op]], addr)[0])
        if op == 35:
            w32(e, DISPLAY + 0x80, a[1])
            w32(e, DISPLAY + 0x84, a[2])
            return 0
        # ---- the RLE loader
        if op == 40:
            if a[1]:
                self.rle = self.new_scalar(0x20)
                self.rle_heap = True
            else:
                self.rle = self.rle_static
                self.rle_heap = False
            self.call(RLE["ctor"], self.rle, 1)
            self.rle_built = True
            return 0
        if op in (41, 42, 43, 44, 45):
            if not self.rle_built:
                return -8
            if op == 41:
                self.call(RLE["SetSize"], self.rle, a[1], a[2])
                return 0
            if op == 42:
                self.call(RLE["SetPalette"], self.rle, self.palette if a[1] == 1 else self.pix[0] if a[1] == 2 else 0)
                return 0
            if op == 43:
                return T.s32(self.call(RLE["LoadSprite"], self.rle, self.handle, a[1])[0])
            if op == 44:
                return T.s32(self.call(RLE["Close"], self.rle)[0])
            return self.call(RLE["AverageColor"], self.rle)[0]
        if op == 49:
            if not self.rle_built or self.rle_used or (a[1] == 1 and not self.rle_heap):
                return -8
            self.call(RLE["dtor"], self.rle, {0: 0, 1: 1, 2: 0xFFFFFFFF}[a[1]])
            if a[1] == 1:   # the delete that follows the destructor is the heap's event
                pass
            self.rle_built = False
            self.rle = self.rle_static
            return 0
        # ---- the sprites' remaining members
        if op == 70:
            if not self.built[slot]:
                return -8
            result = T.s32(self.call(SPRITE["InitName"], s, self.names[a[1] % NAMES])[0])
            if result != 0:
                self.own[slot] = 1
                self.len[slot] = 1
                self.base[slot] = 0
            return result
        if op == 71:
            if not self.built[slot]:
                return -8
            used = struct.unpack(">HH", e.read(s + 0x10, 4))
            destroys = u(s) != 0 and (used[0] != T.s16(a[1]) or used[1] != T.s16(a[2]))
            if destroys and not self.safe_to_free(slot, True):
                return -8
            frees = destroys and self.part_count(s) != 0
            self.call(SPRITE["SetUsedPart"], s, a[1], a[2])
            if frees:
                self.own[slot] = self.len[slot] = 0
            return 0
        if op == 72:
            if not self.built[slot]:
                return -8
            parts = self.parts_to_make(s)
            if parts < 0 or u(s + 4) == 0:
                return -8
            result = T.s32(self.call(SPRITE["Generate"], s, a[1], a[2])[0])
            self.own[slot] = 1
            self.len[slot] = parts
            self.base[slot] = u(s + 4)
            return result
        if op == 73:
            if not self.built[slot] or not self.safe_to_free(slot, False):
                return -8
            frees = self.part_count(s) != 0
            result = T.s32(self.call(SPRITE["Destroy"], s)[0])
            if frees:
                self.own[slot] = self.len[slot] = 0
            return result
        if op == 76:
            w32(e, self.cache + UI_SIZE, a[1])
            return 0
        if op == 78:
            if not self.built[slot] or self.own[slot] != 1 or u(s) == 0:
                return -8
            for i in range(min(self.len[slot], 6)):
                if (a[1] >> i) & 1:
                    entry = u(s) + ENTRY_SIZE * i
                    w32(e, entry + 8, u(entry + 8) | K_ON_PAGE)
            return 0
        if op == 60:
            self.E = self.new_entries(a[1]) if a[1] > 0 else 0
            self.e_count = a[1] if a[1] > 0 else 0
            return 0
        return -99

    # ---- reading state (the order of tests/unit_SpriteRest.cpp)
    def entry_words(self, a, base, hashing=False):
        e, L, u = self.e, self.L, self.e.u32
        s = lambda x: T.s32(u(x))
        w, h, cw, ch = struct.unpack(">hhhh", e.read(a + 0xc, 8))
        flags = u(a + 8)
        out = [self.entry_label(a), int(u(a) == a + 0x44), L.lab(u(a + 4)), flags, w, h, cw, ch, L.lab(u(a + 0x14)),
               L.lab(u(a + 0x18))]
        out += [u(a + 0x1c + 4 * i) for i in range(5)]
        out += [struct.unpack(">h", e.read(a + 0x30, 2))[0], s(a + 0x34), L.lab(u(a + 0x38)), u(a + 0x40)]
        out += list(struct.unpack(">6I", e.read(a + 0x44, 24)))
        data = u(a + 0x3c)
        if flags & K_COMPRESSED:
            out.append(-3)
            out += self.loader_words(data)
        elif data == 0:
            out.append(0)
        elif data == 0xA5A5A5A5:
            out.append(-2)
        elif base:
            out += [-9, T.s32(data - base)]
        elif L.lab(data) > 0:
            out.append(L.lab(data))
        else:
            out.append(-1)
        return out

    def loader_words(self, source):
        e, L, u = self.e, self.L, self.e.u32
        if source == 0:
            return [-4]
        vtable = u(source)
        first_word = lambda block: struct.unpack("<I", e.read(block, 4))[0] if block and L.lab(block) > 0 else 0
        if vtable == RLE_VTABLE:
            rle = source - 0x18
            return [1, L.lab(rle), L.lab(u(rle + 4)), L.lab(u(rle + 8)), T.s32(u(rle + 0xc)), T.s32(u(rle + 0x10)),
                    first_word(u(rle + 4))]
        return [-5]

    def sprite_words(self, a):
        e, L, u = self.e, self.L, self.e.u32
        w, h, uw, uh = struct.unpack(">HHHH", e.read(a + 0xc, 8))
        offset_x = struct.unpack(">h", e.read(a + 0x14, 2))[0]
        part = struct.unpack(">H", e.read(a + 0x16, 2))[0]
        return [self.entry_label(u(a)), L.lab(u(a + 4)), u(a + 8), w, h, uw, uh, offset_x, part]

    def state(self):
        e, L, u = self.e, self.L, self.e.u32
        s32 = lambda x: T.s32(u(x))
        out = list(self.heap)
        for k in range(2):
            io = self.io[k]
            out.append(-1000 - k)
            count, sprites, textures, palette = s32(io), u(io + 4), u(io + 8), u(io + 0xc)
            out += [count, self.array_label(sprites), self.array_label(textures), L.lab(palette)]
            total = 0
            if palette and L.lab(palette) > 0:
                for byte in e.read(palette, 0x400):
                    total = (total * 31 + byte) & MASK48
            out.append(total)
            h = FNV
            shown = count if sprites and textures and count > 0 else 0
            for i in range(shown):
                words = self.sprite_words(sprites + SPRITE_SIZE * i) + self.entry_words(textures + ENTRY_SIZE * i, 0)
                if i < 6:
                    out += words
                else:
                    for value in words:
                        h = ((h ^ (value & MASK64)) * 0x100000001b3) & MASK64
            out.append(h & MASK48)
        for sl in range(SLOTS):
            out.append(-1010 - sl)
            if not self.built[sl]:
                out.append(-7)
                continue
            s = self.sprites[sl]
            out.append(1)
            out += self.sprite_words(s)
            texture = u(s)
            shown = 0 if texture == 0 else (min(self.len[sl], 4) if self.own[sl] == 1 else 1)
            out.append(shown)
            for i in range(shown):
                out += self.entry_words(texture + ENTRY_SIZE * i, self.base[sl])
        for lst in range(LISTS):
            out.append(-1020 - lst)
            if not self.list_built[lst]:
                out.append(-7)
                continue
            a = self.lists[lst]
            out += [s32(a + 4), s32(a + 8), s32(a + 0xc), s32(a + 0x10), L.lab(u(a + 0x14)), s32(a + 0x18)]
        out.append(-1030)
        if self.rle_built:
            out += [int(self.rle_heap), L.lab(u(self.rle + 4)), L.lab(u(self.rle + 8)), s32(self.rle + 0xc), s32(self.rle + 0x10)]
        else:
            out.append(-7)
        out.append(-1031)
        for i in range(self.e_count):
            out += self.entry_words(self.E + ENTRY_SIZE * i, 0)
        for block in self.pix:
            total = 0
            for byte in e.read(block, PIXEL_BYTES):
                total = (total * 31 + byte) & MASK48
            out.append(total)
        total = 0
        for byte in e.read(self.palette, 0x400):
            total = (total * 31 + byte) & MASK48
        out.append(total)
        return out


def ppc(e, case):
    side = PpcSide(e, case)
    words = (P_MALLOC, P_FREE, FILE_OPEN, FILE_READ, FILE_CLOSE, SYSTEM, SETTINGS + 4, G_TCACHE, G_MANAGER, DISPLAY + 0x80,
             DISPLAY + 0x84)
    saved_words = [(a, e.u32(a)) for a in words]
    saved_header = e.read(HEADER, 0x2c)
    saved = side.stubs().activate(e)
    try:
        side.reset()
        results, states, marks = [], [], []
        for op, args in case["script"]:
            results.append(side.op(op, args))
            states.append(side.state())
            marks.append(len(side.L.log))
    finally:
        CL.Stubs.deactivate(e, saved)
        for a, v in saved_words:
            w32(e, a, v)
        e.write(HEADER, saved_header)
    return dict(results=results, log=side.L.log, marks=marks, states=states)


def host(lib, case):
    reset = lib["tpw_sr_reset"]
    reset.restype, reset.argtypes = None, [C.c_char_p, C.c_uint32]
    op_fn = lib["tpw_sr_op"]
    op_fn.restype, op_fn.argtypes = C.c_int64, [C.c_int32, C.POINTER(C.c_int32)]
    state_fn = lib["tpw_sr_state"]
    state_fn.restype, state_fn.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    address = lib["tpw_sr_address"]
    address.restype, address.argtypes = C.c_void_p, [C.c_int32]
    data = bytes(case["file"])
    reset(data, len(data))
    CL.host_begin(lib, {label: address(label) for label in (3, 4, 5, 6, 9, 11, 13, 31, 32)}, case.get("q"))
    results, states, marks = [], [], []
    buffer = (C.c_int64 * 16384)()
    try:
        for op, args in case["script"]:
            padded = (C.c_int32 * 16)(*[T.s32(v) for v in (list(args) + [0] * 16)[:16]])
            results.append(op_fn(op, padded))
            states.append(list(buffer[:state_fn(buffer)]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(results=results, log=log, marks=marks, states=states)


# ----------------------------------------------------------------- the files


def le(fmt, *values):
    return struct.pack("<" + fmt, *values)


def rle_sprite(width=29, height=28, used_width=128, used_height=128, offset_x=-12, offset_y=-27, data=b"RLEDATA!", size=None):
    return le("IHHHHII", len(data) if size is None else size, width, height, used_width, used_height, offset_x & 0xFFFFFFFF,
              offset_y & 0xFFFFFFFF) + data


def palette_bytes(seed=1):
    return bytes((i * 5 + seed) & 0xFF for i in range(0x400))


def rle_file(sprites, kind=3, second=3, count=None, palette=None):
    """A kind 3 file: it reads the same on both sides."""
    palette = palette_bytes() if palette is None else palette
    return le("HHI", kind & 0xFFFF, second & 0xFFFF, len(sprites) if count is None else count) + palette + b"".join(sprites)


def same(data):
    return dict(file=data)


_REAL = {}


def real_file(name):
    """One of the game's sprite files (kind 3); None when the data is not there."""
    if name not in _REAL:
        path = ROOT / "extracted" / "wads" / "esprites.wad.d" / name
        _REAL[name] = path.read_bytes() if path.exists() else None
    return _REAL[name]


# One picture (2 KB), eight, 175, 170, 210 and 260 (the most: 470 KB).
REAL_FILES = ["Generic/SpecialFX/SPR_SF.TPC", "Generic/Litter/SPR_LI.TPC", "Hallow/Costumes/SPR_HK.TPC", "Hallow/Costumes/SPR_HK.FPC",
              "Hallow/Entertainers/SPR_FK.TPC", "Hallow/Entertainers/SPR_HU.FPC", "Generic/Guards/SPR_GU.FPC"]


# ----------------------------------------------------------------- scripts


def entry_kind(r):
    return r.choice([0, 0, 0, 1, 2])


def image_kind(r):
    return r.choice([0, 0, 1, 2])


def flags_arg(r):
    # Not the flags that make an entry a loader's (compressed) or give it a further plane.
    return r.choice([0, 0x100, 0x200, 0x300, 0x1000, 0x4000, 0x40000, 0x80000, 0x4000000, r.getrandbits(32)]) & ~(0x10000000 | 0x00800000) & 0xFFFFFFFF


SIZES = [0, 1, 7, 8, 9, 16, 17, 31, 32, 33, 40, 64, 65, 100, 127, 128, 129, 200, 300, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, -1, -5]
PARTS = [0, 1, 2, 3, 7, 8, 31, 32, 33, 64, 65, 70, 96, 100, 200, 0x7FFF, 0x8000, 0xFFFF, -1, -2, -32768, -32769, 0x10001, 0x18000]
OFFSETS = [0, 1, -1, 5, -5, 31, 32, 33, 64, 100, 1000, -1000, 0x7FFF, -0x8000, 0x10000, 0x123456, -0x123456, 0x7FFFFFFF, -0x80000000]


def sprite_ops(r):
    kind = r.choice([10, 10, 11, 12, 12, 14, 14, 14, 15, 15, 16, 17, 17, 18, 18, 19, 70, 70, 70, 71, 71, 72, 72, 73, 73, 76, 78])
    slot = r.randrange(4)
    if kind in (10, 11, 16, 17, 19, 73):
        return (kind, [slot])
    if kind == 12:
        return (12, [slot, image_kind(r), image_kind(r), r.choice([0, 0, 1, 2]), r.choice([0, 1, 8, 29, 64, 300, -4]),
                     r.choice([0, 1, 8, 28, 64, 5000]), entry_kind(r), flags_arg(r)])
    if kind == 14:
        return (14, [slot, r.choice([0, 1, 2]), entry_kind(r)])
    if kind == 15:
        return (15, [slot, r.choice([0, 1, 4, 16, 33, 64, 100, 128, 65535, 0x10001, -1]), r.choice([0, 1, 4, 8, 33, 64, 128, 65535, 3])])
    if kind == 18:
        n = r.choice([0, 1, 2, 3, 5])
        return (18, [slot, r.randrange(2), n, r.getrandbits(5), r.choice([-1, -1, -1, 0, 1, 2, 5, 9])])
    if kind == 70:
        return (70, [slot, r.randrange(NAMES)])
    if kind == 71:
        return (71, [slot, r.choice(PARTS), r.choice(PARTS)])
    if kind == 72:
        return (72, [slot, r.choice(OFFSETS), r.choice(OFFSETS)])
    if kind == 76:
        return (76, [0, r.choice([1, 2, 3, 4, 8, 16, 32, 32, 48, 64, 128, 256, -1, -4])])
    return (78, [slot, r.getrandbits(6)])


def list_ops(r):
    kind = r.choice([30, 30, 31, 32, 32, 33, 33, 34, 34, 34, 35, 36, 37])
    lst = r.randrange(3)
    if kind == 32:
        return (32, [lst, r.choice([0, 1, 5, 64, 1000])])
    if kind == 35:
        return (35, [0, r.choice([0, 1, 2, 3, -1, 0x7FFFFFFF]), r.choice([0, 1, 5, -1])])
    return (kind, [lst])


def loader_ops(r):
    kind = r.choice([40, 41, 42, 43, 43, 44, 44, 45, 49])
    if kind == 40:
        return (40, [0, r.randrange(2)])
    if kind == 41:
        return (41, [0, r.choice([0, 29, 128, -1]), r.choice([0, 28, 0x10000])])
    if kind == 42:
        return (42, [0, r.choice([0, 1, 2])])
    if kind == 43:
        return (43, [0, r.choice([0, 4, 8, 20, 100, 0x4000, 0x20000, -1])])
    if kind == 49:
        return (49, [0, r.choice([0, 1, 2])])
    return (kind, [0])


def file_ops(r):
    kind = r.choice([1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 7, 8, 8, 60])
    k = r.randrange(2)
    if kind == 5:
        return (5, [0, r.choice([0, 0, 0, 8, 8 + 0x400, 5, -1])])
    if kind == 7:
        return (7, [0, r.choice([0, 0, 1, 0x40000000]), r.choice([4, 4, 5, 3, 0, 1])])
    if kind == 8:
        return (8, [k, r.randrange(NAMES), r.choice([0, 1, -1, 77])])
    if kind == 60:
        return (60, [0, r.choice([0, 2, 3, 4])])
    return (kind, [k])


def queues(r, fail):
    q = {}
    if fail:
        q["Malloc"] = [1 if r.random() < 0.7 else 0 for _ in range(r.randrange(1, 8))] + [1]
    q["TGA"] = [1 if r.random() < 0.8 else 0 for _ in range(r.randrange(1, 12))]
    q["TGAw"] = [r.choice([29, 1, 0, 64, 65535, 65536, 70001, 33, 7, -9, 129, 256]) for _ in range(r.randrange(1, 12))]
    q["TGAh"] = [r.choice([28, 1, 0, 64, 65535, 65536, 70001, 32, 5, -4, 128]) for _ in range(r.randrange(1, 12))]
    q["Open"] = [1 if r.random() < 0.7 else 0 for _ in range(r.randrange(1, 6))]
    q["AddList"] = [r.choice([1, 1, 0, 2, -1]) for _ in range(r.randrange(1, 8))]
    return q


def rnd_file(r):
    """A sprite file for a random case; sometimes cut short inside the last picture's data."""
    kind = r.choice([3, 3, 3, 3, 3, 1, 4, 0, -1, 0x7FFF, 0x100])
    count = r.choice([0, 1, 2, 3, 5, 8, 12])
    sprites = [rle_sprite(width=r.choice([0, 1, 29, 64, 128, 255]), height=r.choice([0, 1, 28, 64, 128, 17]),
                          used_width=r.choice([0, 128, 77]), used_height=r.choice([0, 128, 66]),
                          offset_x=r.choice([0, -12, 5, 0x12345, -1, 0x7FFF, 0x8000]), offset_y=r.choice([0, -27, 9, -0x23456, 0xFFFF]),
                          data=bytes(r.getrandbits(8) for _ in range(r.choice([0, 1, 4, 8, 20, 60])))) for _ in range(count)]
    data = rle_file(sprites, kind=kind, second=r.choice([0, 3, 9]), palette=palette_bytes(r.randrange(256)))
    if kind == 3 and count and r.random() < 0.15:
        # Short inside the last picture's RLE data: the reads of its header words still come up full.
        extra = len(sprites[-1]) - 20
        if extra > 0:
            data = data[:len(data) - r.randrange(1, extra + 1)]
    if r.random() < 0.08:
        data = data[:r.randrange(0, 5)]   # inside the file's own header, before the count
    return same(data)


def case_of(r, makers, length, fail=False, prefix=()):
    script = [(60, [0, 4])] + list(prefix)
    script += [(10, [s]) for s in range(3)] + [(30, [0]), (30, [1]), (40, [0, r.randrange(2)])]
    case = dict(q=queues(r, fail))
    case.update(rnd_file(r))
    for _ in range(length):
        script.append(r.choice(makers)(r))
        if r.random() < 0.2:
            script.append((5, [0, 0]))
    script += [(3, [0]), (3, [1])]
    case["script"] = script
    return case


# ----------------------------------------------------------------- edge cases


def sprites_small(n, seed=0):
    return [rle_sprite(width=10 + i, height=20 + seed + i, offset_x=-i, offset_y=-2 * i - 1, data=bytes([i + 1] * (3 + i))) for i in range(n)]


def base_script():
    return [(60, [0, 4]), (10, [0]), (10, [1]), (10, [2]), (10, [3])]


def edge_lists():
    r = random.Random(81)
    base = base_script()
    # Close: nothing allocated, allocated, twice, after the destructor's own Close; Init again afterwards.
    for lst in range(3):
        for count in (0, 1, 7, 100):
            yield dict(script=base + [(30, [lst]), (33, [lst]), (32, [lst, count]), (33, [lst]), (33, [lst]), (32, [lst, count + 1]),
                                      (31, [lst]), (30, [lst]), (32, [lst, 3]), (33, [lst]), (31, [lst])], **same(b""))
        # The destructor closes the list: the array is freed (or not, when there is none), then the link base.
        yield dict(script=base + [(30, [lst]), (32, [lst, 5]), (31, [lst]), (30, [lst]), (31, [lst])], **same(b""))
        yield dict(script=base + [(30, [lst]), (32, [lst, 5])], q={"Malloc": [0]}, **same(b""))
        yield dict(script=base + [(30, [lst]), (32, [lst, 5]), (33, [lst]), (31, [lst])], q={"Malloc": [0]}, **same(b""))
    # Prepare: the frame words differ (a new frame), repeat (the same frame), one of two differs; the manager refuses; before Init.
    frames = [(0, 0), (1, 0), (0, 1), (1, 1), (5, 0), (5, 5), (-1, -1), (0x7FFFFFFF, 3)]
    for lst in range(3):
        for (fa, fb) in frames:
            for (ga, gb) in frames:
                yield dict(script=base + [(30, [lst]), (35, [0, fa, fb]), (34, [lst]), (34, [lst]), (35, [0, ga, gb]), (34, [lst]),
                                          (34, [lst]), (35, [0, fa, fb]), (34, [lst])], **same(b""))
    for answers in ([0], [0, 1], [1, 0], [2], [-1, 5], [0, 0, 0]):
        yield dict(script=base + [(30, [0]), (30, [1]), (30, [2]), (35, [0, 3, 4]), (34, [0]), (34, [0]), (35, [0, 9, 4]), (34, [0]),
                                  (34, [1]), (34, [2]), (34, [1]), (34, [2]), (35, [0, 3, 9]), (34, [2]), (34, [0])],
                   q={"AddList": answers}, **same(b""))
    # A list that was never made, GetData and GetLinkType at every stage.
    for lst in range(3):
        yield dict(script=base + [(36, [lst]), (37, [lst]), (34, [lst]), (33, [lst]), (30, [lst]), (36, [lst]), (37, [lst]),
                                  (32, [lst, 4]), (36, [lst]), (37, [lst]), (33, [lst]), (36, [lst]), (37, [lst]), (31, [lst]), (37, [lst])],
                   **same(b""))
    for _ in range(40):
        yield case_of(r, [list_ops, list_ops, list_ops, file_ops], 20)


def edge_loader():
    r = random.Random(82)
    base = [(60, [0, 4])] + [(10, [s]) for s in range(4)]
    data = le("I", 0x01020304) + bytes(range(256)) * 80
    for heap in (0, 1):
        # Close with and without the data; AverageColor; Close twice; the setters then Close; LoadSprite again after Close.
        for size in (0, 1, 4, 100, 0x10000):
            yield dict(script=base + [(40, [0, heap]), (45, [0]), (44, [0]), (41, [0, 29, 28]), (42, [0, 1]), (43, [0, size]), (45, [0]),
                                      (44, [0]), (44, [0]), (43, [0, size]), (41, [0, -1, 5]), (44, [0]), (49, [0, 0])], **same(data))
        # The destructor in each of its three flags, with and without data, then the object built again.
        for flag in (0, 1, 2):
            for load in (0, 1):
                script = base + [(40, [0, heap]), (41, [0, 29, 28]), (42, [0, 2])] + ([(43, [0, 40])] if load else [])
                script += [(49, [0, flag]), (40, [0, heap]), (45, [0]), (49, [0, flag]), (40, [0, 1 - heap]), (49, [0, 0])]
                yield dict(script=script, **same(data))
        # The allocator refuses the data; a picture's loader owned by an entry (destroyed with it: frees the data).
        yield dict(script=base + [(40, [0, heap]), (43, [0, 8]), (44, [0]), (49, [0, 0])], q={"Malloc": [0]}, **same(data))
    # Loaders owned by sprite entries: Close, Free and the destructors all run the real ones.
    sprites = sprites_small(4)
    file_data = rle_file(sprites)
    for n in (1, 2, 4):
        yield dict(script=base + [(5, [0, 0]), (2, [0]), (3, [0]), (2, [0]), (4, [0]), (3, [0])], **same(rle_file(sprites_small(n))))
        yield dict(script=base + [(5, [0, 8 + 0x400]), (14, [0, 2, 0]), (14, [1, 1, 0]), (17, [0]), (11, [1]), (14, [2, 0, 0]), (11, [2])],
                   **same(file_data))
    # Standalone entries with a standalone loader as their source (never closed), then the loader destroyed.
    for entry in (1, 2):
        yield dict(script=base + [(40, [0, 1]), (12, [0, 1, 0, 1, 16, 16, entry, 0x300]), (44, [0]), (49, [0, 0]), (17, [0])], **same(b""))
    for _ in range(40):
        yield case_of(r, [loader_ops, loader_ops, sprite_ops, file_ops], 18)


def edge_init_name():
    r = random.Random(83)
    base = base_script()
    sizes = [(29, 28), (1, 1), (0, 0), (64, 64), (33, 17), (65535, 65535), (65536, 65536), (70001, 70001), (-9, -4), (7, 5), (129, 256),
             (31, 0), (0, 31), (-1, -1), (-2, 3), (0x7FFFFFFF, 0x7FFFFFFF), (-0x80000000, -0x80000000), (3, 0x10005)]
    for name in range(NAMES):
        for (w, h) in sizes:
            yield dict(script=base + [(70, [0, name]), (70, [1, name]), (17, [0]), (11, [1])], q={"TGAw": [w], "TGAh": [h]}, **same(b""))
    # The picture and its alpha: the second load's size is the sprite's; either may fail.
    for name in (0, 1, 2, 3, 4, 5, 6, 8):
        for tga in ([1, 1], [1, 0], [0, 1], [0, 0], [1, 1, 1, 1], [1, 0, 1, 0]):
            yield dict(script=base + [(70, [0, name]), (70, [0, name]), (70, [3, name]), (17, [0]), (17, [3]), (11, [3])],
                       q={"TGA": tga, "TGAw": [29, 64, 33], "TGAh": [28, 32, 5]}, **same(b""))
    # The empty name: its last character is the byte in front of it, a zero, so it never takes the alpha.
    yield dict(script=base + [(70, [0, 7]), (70, [1, 7]), (17, [0]), (17, [1])], q={"TGA": [1, 0, 1]}, **same(b""))
    yield dict(script=base + [(70, [0, 7]), (17, [0])], q={"TGA": [1]}, **same(b""))
    # The block of pixels does not stop it; a sprite from a file, a sprite with partial textures, then a TGA over them.
    yield dict(script=base + [(15, [0, 16, 16]), (71, [0, 16, 16]), (72, [0, 0, 0]), (70, [0, 0]), (17, [0])], **same(b""))
    # The pixel allocator fails for the picture (the loader answers nothing) and for its alpha.
    for queue in ([0], [1, 0], [1, 1, 0], [1, 0, 1], [0, 0, 0]):
        yield dict(script=base + [(70, [0, 1]), (70, [1, 1]), (17, [0]), (17, [1])], q={"Malloc": queue}, **same(b""))
    for _ in range(40):
        yield case_of(r, [sprite_ops, sprite_ops, sprite_ops, file_ops], 16)


def edge_parts():
    r = random.Random(84)
    base = base_script()
    # SetUsedPart: with and without a texture, the same part and a new one, negative parts, the halfword edges.
    for texture in ("none", "tga", "parts"):
        for stored in ((0, 0), (5, 7), (0x7FFF, 0x8000), (0xFFFF, 0xFFFF)):
            for call in ((0, 0), (5, 7), (7, 5), (5, 5), (-1, -1), (-32768, 5), (0x7FFF, 0x8000), (65535, 65535), (0x10005, 7), (5, 0x10007)):
                pre = {"none": [], "tga": [(70, [0, 0])], "parts": [(18, [0, 1, 3, 5, -1])]}[texture]
                yield dict(script=base + pre + [(71, [0, stored[0], stored[1]]), (71, [0, stored[0], stored[1]]), (71, [0, call[0], call[1]]),
                                                (71, [0, call[0], call[1]]), (17, [0])], **same(b""))
    # GeneratePartialTexture and DestroyPartialTexture: used parts around the cache's size, other sizes of the cache, offsets.
    for size in (1, 2, 4, 8, 16, 32, 64, 128, 3, 5, 48, 100):
        for used in ((0, 0), (1, 1), (size, size), (size + 1, size), (size, size + 1), (2 * size, 3 * size + 5), (size - 1, 2 * size - 1),
                     (0, size), (size, 0), (100, 33), (200, 7), (65535, 1), (1, 65535), (3 * size, size), (size + size // 2, size // 2 + 1)):
            if used[0] > 4000 or used[1] > 4000:
                continue
            for x, y in ((0, 0), (5, 9), (-3, -7), (1000, 77)):
                yield dict(script=base + [(76, [0, size]), (15, [0, 64, 64]), (71, [0, used[0], used[1]]), (72, [0, x, y]), (73, [0]),
                                          (73, [0]), (17, [0])], **same(b""))
    # A cache size the game never has (negative) reaches the assertion that every part has a size.
    for size in (-1, -2, -3, -4, -8):
        for used in ((0, 0), (1, 0), (0, 1), (1, 1), (2, 1), (1, 2), (3, 3), (5, 0), (0, 5), (7, 7), (9, 2)):
            yield dict(script=base + [(76, [0, size]), (15, [0, 64, 64]), (71, [0, used[0], used[1]]), (72, [0, 3, 4]), (73, [0]), (17, [0])],
                       **same(b""))
    # The entries sit on a cache page: Destroy takes them off (Remove) in order; and Close / Refresh / SetUsedPart do too.
    for bits in (0, 1, 2, 5, 0x3F, 0x15):
        for closer in ((73,), (16,), (17,), (71, 3, 4)):
            tail = [(closer[0], [0] + list(closer[1:]))]
            yield dict(script=base + [(76, [0, 16]), (15, [0, 100, 70]), (71, [0, 40, 37]), (72, [0, 3, 4]), (78, [0, bits])] + tail
                       + [(73, [0]), (17, [0])], **same(b""))
    # A sprite with no pixel block (data is zero); one with a block of another size; a sprite made from a file or a TGA.
    yield dict(script=base + [(76, [0, 16]), (71, [0, 40, 40]), (72, [0, 8, 8]), (73, [0]), (17, [0])], **same(b""))
    yield dict(script=base + [(76, [0, 16]), (70, [0, 0]), (71, [0, 40, 40]), (72, [0, 8, 8]), (73, [0]), (17, [0])], **same(b""))
    yield dict(script=base + [(76, [0, 16]), (15, [0, 20, 20]), (71, [0, 40, 40]), (72, [0, 8, 8]), (72, [0, -8, 0]), (73, [0]), (17, [0])],
               **same(b""))
    # Generate on the standard formats: the pixel size multiplies the offsets (2 or 4 bytes a pixel).
    for fmt in (0, 1, 2, 3, 4, 5):
        yield dict(script=base + [(7, [0, 0, fmt]), (76, [0, 16]), (15, [0, 64, 64]), (71, [0, 40, 37]), (72, [0, 3, 4]), (73, [0])]
                   + [(17, [0])], **same(b""))
    for _ in range(60):
        yield case_of(r, [sprite_ops, sprite_ops, sprite_ops, sprite_ops, file_ops], 20)


def edge_load_name():
    r = random.Random(85)
    base = [(60, [0, 4])]
    for opens in ([1], [0], [0, 1], [1, 1, 1]):
        for name in (0, 1, 7, 9):
            for n in (0, 1, 2, 7):
                yield dict(script=base + [(8, [0, name, 0]), (4, [0]), (3, [0]), (8, [0, name, 5]), (8, [1, name, -1]), (3, [0]), (3, [1])],
                           q={"Open": opens}, **same(rle_file(sprites_small(n))))
    for fmt, flags in ((4, 0), (5, 1), (3, 0x80000001)):
        yield dict(script=base + [(7, [0, flags, fmt]), (8, [0, 0, 0]), (4, [0]), (3, [0])], **same(rle_file(sprites_small(3))))
    # The file is read from its start: after a seek the call does not run; a file of another kind loads nothing.
    yield dict(script=base + [(5, [0, 8]), (8, [0, 0, 0]), (3, [0])], **same(rle_file(sprites_small(2))))
    for kind in (1, 4, 0, -1, 0x100):
        yield dict(script=base + [(8, [0, 0, 0]), (3, [0])], **same(rle_file(sprites_small(2), kind=kind)))
    for name in REAL_FILES:
        data = real_file(name)
        if data is None:
            continue
        for fmt, flags in ((4, 0), (5, 1)):
            yield dict(script=base + [(7, [0, flags, fmt]), (8, [0, 0, 0]), (4, [0]), (3, [0])], **same(data))
    for _ in range(25):
        yield case_of(r, [file_ops, file_ops, sprite_ops], 14)


def edge_failing():
    r = random.Random(86)
    base = base_script()
    for n in range(0, 5):
        yield dict(script=base + [(18, [0, 1, 3, 5, -1]), (15, [2, 8, 8]), (15, [3, 4, 4]), (70, [1, 1]), (32, [0, 5]), (30, [0]),
                                  (32, [0, 5]), (30, [2]), (32, [2, 3]), (76, [0, 16]), (71, [3, 40, 40]), (72, [3, 0, 0]), (73, [3]),
                                  (17, [0]), (11, [2]), (11, [3]), (31, [0]), (31, [2])],
                   q={"Malloc": [1] * n + [0, 1, 0]}, **same(b""))
    # The allocator refuses the palette: the file is closed and nothing of it is read. (Not the pictures: see FAILING.)
    for op in (2, 8):
        for n in (0, 1, 3):
            yield dict(script=base + [(op, [0, 0, 0]), (3, [0]), (op, [1, 1, 5]), (3, [1])], q={"Malloc": [0, 1]}, **same(rle_file(sprites_small(n))))
    for _ in range(40):
        yield failing(r)


def safe_ops(maker, banned):
    """An operation generator that leaves out the operations in `banned`."""
    def make(r):
        while True:
            op = maker(r)
            if op[0] not in banned:
                return op
    return make


# With a failing allocator no picture is read: a picture block that is not made leaves the file where it was, and what follows is
# then read as a header (stack garbage when it comes up short). So no operation that reads pictures (Load_RLE, the file loads).
FAILING = [safe_ops(sprite_ops, (14,)), safe_ops(sprite_ops, (14,)), list_ops, safe_ops(loader_ops, ()), safe_ops(file_ops, (2, 8))]


def failing(r):
    return case_of(r, FAILING, 30, fail=True)


def mixed(r):
    return case_of(r, [file_ops, sprite_ops, list_ops, loader_ops], 200)


TESTS = [
    FnTest("sprite lists: Close, Prepare, GetData, GetLinkType, with the constructor, Init and destructor", ppc, host, edge=edge_lists,
           rnd=lambda r: case_of(r, [list_ops, list_ops, list_ops, file_ops], 20)),
    FnTest("RLE loader: Close, AverageColor, the destructor in each flag, loaders owned by entries", ppc, host, edge=edge_loader,
           rnd=lambda r: case_of(r, [loader_ops, loader_ops, sprite_ops, file_ops], 18)),
    FnTest("SSpriteEngineData::Init(char*)", ppc, host, edge=edge_init_name,
           rnd=lambda r: case_of(r, [sprite_ops, sprite_ops, sprite_ops, file_ops], 16)),
    FnTest("SetUsedPart, GeneratePartialTexture, DestroyPartialTexture", ppc, host, edge=edge_parts,
           rnd=lambda r: case_of(r, [sprite_ops, sprite_ops, sprite_ops, sprite_ops, file_ops], 20)),
    FnTest("CSpriteFileIO::Load(char*, int)", ppc, host, edge=edge_load_name,
           rnd=lambda r: case_of(r, [file_ops, file_ops, sprite_ops], 14)),
    FnTest("failing allocator: Init(char*), partial textures, lists, loaders", ppc, host, edge=edge_failing, rnd=failing),
    FnTest("sprite files, mixed scripts of 200 operations", ppc, host, rnd=mixed),
]


# ----------------------------------------------------------------- the RLE unpacking on its own

BPP = [2, 2, 2, 4, 2, 4]    # the table at 0x10051600, by texture format
RD_BUFFER, RD_DATA = 0x10000, 0x8000
STACK_ROW = 532             # the room the original has for a row it must scale: 133 words


class Unsafe(Exception):
    pass


def divw(a, b):
    """The processor's divide as the emulator does it (and the translation): zero divisors and the one overflow give all ones for a
    negative dividend and zero otherwise; the quotient truncates toward zero."""
    a, b = T.s32(a), T.s32(b)
    if b == 0 or (a == -0x80000000 and b == -1):
        return -1 if a < 0 else 0
    q = abs(a) // abs(b)
    return -q if (a < 0) != (b < 0) else q


def simulate(c):
    """The unpacking's reads and writes for case `c` (the algorithm of src/tb_rleload.cpp in Python, a guard and not the oracle): raises
    Unsafe when the original would read outside the stream's room, write outside the buffer, read the stack's garbage or overrun its
    row buffer, which the two sides could not agree on. Returns (lowest write, highest write + 1)."""
    bpp = BPP[c["fmt"]]
    data = c["stream"] + bytes(RD_DATA - len(c["stream"]))
    width, height, dw, dh, pitch = c["width"], c["height"], c["dw"], c["dh"], c["pitch"]
    pos, lo, hi = 0, RD_BUFFER, 0

    def read(n):
        nonlocal pos
        if pos < 0 or pos + n > RD_DATA:
            raise Unsafe("read")
        pos += n
        return pos - n

    def wrote(a, n):
        nonlocal lo, hi
        if n <= 0:
            return
        if a < 0 or a + n > RD_BUFFER:
            raise Unsafe("write")
        lo, hi = min(lo, a), max(hi, a + n)

    step_x, step_y = divw(width << 16, dw), divw(height << 16, dh)
    scaled = dw != width
    slack = 0 if pitch == 0 else pitch - T.s32(dw * bpp)
    out, accum, state = c["start"], 0, 1
    for _ in range(height):
        if state == 0:
            b = T.s8(data[read(1)]) if pos < RD_DATA else 0
            pos += b
        elif state == 2:
            n = T.u32(dw * bpp)
            if out - pitch < 0 or out - pitch + n > RD_BUFFER or (pitch != 0 and abs(pitch) < n):
                raise Unsafe("copy")
            wrote(out, n)
            accum += step_y
            out += pitch
        else:
            read(1)
            done, pixels = 0, 0
            while done < width:
                t = T.s8(data[read(1)])
                if t <= 0:
                    read(1)
                    count = -t
                else:
                    read(t)
                    count = t
                pixels += count
                done += count
            if scaled:
                if (width <= 0 and dw > 0) or pixels * bpp > STACK_ROW or dw < 0:
                    raise Unsafe("row buffer")
                wrote(out, dw * bpp)
                end = out + max(dw, 0) * bpp
            else:
                wrote(out, pixels * bpp)
                end = out + pixels * bpp
            out = end + slack
            accum += step_y
        accum = T.s32(accum)
        if accum < 0x20000:
            state = 1 if accum & 0x10000 else 2
            accum &= 0xFFFF
        else:
            accum -= 0x10000
            state = 0
    if hi < lo:
        lo = hi = 0
    return lo, hi


def ppc_rd(e, case):
    L = CL.PpcLog({})
    e.cl = L
    st = CL.Stubs()
    st.real = set(RLE.values()) | set(RLE_THUNKS) | set(INNER)
    st.imp("__assertion_failed", lambda e: L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
    saved_format = e.u32(SETTINGS + 4)
    saved = st.activate(e)
    try:
        w32(e, SETTINGS + 4, case["fmt"])
        loader = e.alloc(0x20, b"\xa5" * 0x20)
        T.call_ppc(e, RLE["ctor"], [loader, 1])
        data = e.alloc(RD_DATA, case["stream"] + bytes(RD_DATA - len(case["stream"])))
        palette = e.alloc(0x400, case["palette"])
        buffer = e.alloc(RD_BUFFER, bytes((i * 13 + case["seed"]) & 0xFF for i in range(RD_BUFFER)))
        dest = e.alloc(16)
        e.write(dest, struct.pack(">IiiI", buffer + case["start"], case["dw"], case["dh"], case["pitch"] & 0xFFFFFFFF))
        e.write(loader + 4, struct.pack(">IIii", data, palette, case["width"], case["height"]))
        results, buffers = [], []
        for kind, flip in case["calls"]:
            if kind == 0:
                r = T.call_ppc(e, RLE["Decompress"], [loader, dest])[0]
            elif kind == 1:
                r = T.call_ppc(e, RLE["Blit"], [loader, dest, flip])[0]
            else:
                r = T.call_ppc(e, RLE["AverageColor"], [loader])[0]
            results.append(T.u32(r))
            buffers.append(e.read(buffer, RD_BUFFER))
        words = [int(e.u32(loader + 4) == data), int(e.u32(loader + 8) == palette), T.s32(e.u32(loader + 0xC)), T.s32(e.u32(loader + 0x10)),
                 e.u32(dest) - buffer, T.s32(e.u32(dest + 4)), T.s32(e.u32(dest + 8)), T.s32(e.u32(dest + 12))]
    finally:
        CL.Stubs.deactivate(e, saved)
        w32(e, SETTINGS + 4, saved_format)
    return dict(results=results, buffers=buffers, words=words, log=L.log)


def host_rd(lib, case):
    setup_fn = lib["tpw_rd_setup"]
    setup_fn.restype = None
    setup_fn.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.c_char_p, C.c_int32, C.c_char_p, C.c_int32, C.c_int32, C.c_int32, C.c_int32,
                         C.c_int32]
    call_fn = lib["tpw_rd_call"]
    call_fn.restype, call_fn.argtypes = C.c_int64, [C.c_int32, C.c_int32]
    read_fn = lib["tpw_rd_read"]
    read_fn.restype, read_fn.argtypes = None, [C.c_char_p, C.POINTER(C.c_int64)]
    CL.host_begin(lib, {})
    setup_fn(case["fmt"], case["width"], case["height"], case["stream"], len(case["stream"]), case["palette"], case["dw"], case["dh"],
             T.s32(case["pitch"]), case["start"], case["seed"])
    results, buffers = [], []
    out = C.create_string_buffer(RD_BUFFER)
    words = (C.c_int64 * 8)()
    try:
        for kind, flip in case["calls"]:
            results.append(call_fn(kind, flip) & 0xFFFFFFFF)
            read_fn(out, words)
            buffers.append(bytes(out.raw))
    finally:
        log = CL.host_end(lib)
    return dict(results=results, buffers=buffers, words=list(words), log=log)


def tok_lit(indices):
    return bytes([len(indices)]) + bytes(indices)


def tok_run(index, count):
    return bytes([(-count) & 0xFF, index])


def tok_zero(index):
    return bytes([0, index])


def rle_row(tokens, length=None):
    body = b"".join(tokens)
    return bytes([len(body) if length is None else length & 0xFF]) + body


def row_tokens(r, width, style):
    """Tokens that fill exactly `width` pixels in the manner of `style`."""
    tokens, left = [], width
    while left > 0:
        if style == "lit":
            n = min(left, r.choice([1, 2, 7, 8, 9, 15, 16, 17, 31, 32, 33, 64, 127]))
            tokens.append(tok_lit([r.getrandbits(8) for _ in range(n)]))
        elif style == "run":
            n = min(left, r.choice([1, 2, 7, 8, 9, 15, 16, 17, 31, 32, 33, 64, 127, 128]))
            tokens.append(tok_run(r.getrandbits(8), n))
        else:
            if r.random() < 0.5:
                n = min(left, r.choice([1, 2, 3, 5, 8, 9, 16, 17, 40, 127]))
                tokens.append(tok_lit([r.getrandbits(8) for _ in range(n)]))
            else:
                n = min(left, r.choice([1, 2, 3, 5, 8, 9, 16, 17, 40, 128]))
                tokens.append(tok_run(r.getrandbits(8), n))
            if r.random() < 0.08:
                tokens.insert(len(tokens) - 1, tok_zero(r.getrandbits(8)))   # a token of no pixels
        left -= n
    return tokens


def picture(r, width, height, style="mixed"):
    return b"".join(rle_row(row_tokens(r, width, style)) for _ in range(height))


def palette_for(r, kind="random"):
    if kind == "one":   # a palette of one colour: entry 0 is the colour, the rest is what a bad index would find
        return bytes([0xAA, 0xBB, 0xCC, 0xDD]) + bytes(0x3FC)
    if kind == "ramp":
        return bytes(i & 0xFF for i in range(0x400))
    return bytes(r.getrandbits(8) for _ in range(0x400))


def rd_case(fmt, width, height, stream, dw=None, dh=None, pitch=0, start=0x1000, palette=None, calls=((0, 0),), seed=7):
    """A case of the unpacking; pitch -1 means a packed destination given its pitch (the row's bytes), None for 0."""
    dw = width if dw is None else dw
    dh = height if dh is None else dh
    return dict(fmt=fmt, width=width, height=height, stream=stream, dw=dw, dh=dh, pitch=pitch, start=start,
                palette=palette if palette is not None else palette_bytes(5), calls=list(calls), seed=seed)


def usable(c):
    """Whether the case is one the two sides can be compared on (see simulate)."""
    try:
        simulate(c)
    except Unsafe:
        return False
    return True


def edge_unpack():
    r = random.Random(91)
    ramp = palette_for(r, "ramp")
    # Every length of a literal token and of a run, in each format, one token a row: the loops unroll by eight.
    for fmt in (4, 5):
        for n in list(range(1, 20)) + [23, 24, 25, 31, 32, 33, 39, 40, 41, 63, 64, 65, 100, 126, 127]:
            yield rd_case(fmt, n, 2, rle_row([tok_lit(list(range(n)))]) + rle_row([tok_lit([200 - i for i in range(n)])]), palette=ramp)
        for n in list(range(1, 20)) + [23, 24, 25, 31, 32, 33, 63, 64, 65, 100, 127, 128]:
            yield rd_case(fmt, n, 2, rle_row([tok_run(37, n)]) + rle_row([tok_run(255, n)]), palette=ramp)
    # The other formats read the same (16 bits for 0, 1, 2 and 4; 32 for 3 and 5).
    for fmt in range(6):
        yield rd_case(fmt, 20, 3, picture(r, 20, 3), palette=ramp, calls=((0, 0), (1, 0), (1, 1), (2, 0)))
    # A token of no pixels, a run of 128 (n = -128), tokens at the row's edge, the palette's first and last entries.
    yield rd_case(4, 10, 2, rle_row([tok_zero(9), tok_run(0, 5), tok_zero(1), tok_lit([255, 0, 255, 0, 255])]) + rle_row([tok_run(255, 10)]),
                  palette=ramp)
    yield rd_case(5, 128, 1, rle_row([tok_run(1, 128)]), palette=ramp)
    yield rd_case(5, 129, 2, rle_row([tok_run(1, 128), tok_lit([2])]) + rle_row([tok_lit([3]), tok_run(4, 128)]), palette=ramp)
    # Palettes of one colour and of 256.
    yield rd_case(4, 20, 4, picture(r, 20, 4), palette=palette_for(r, "one"))
    yield rd_case(5, 20, 4, rle_row([tok_run(0, 20)]) * 4, palette=palette_for(r, "one"))
    yield rd_case(5, 40, 6, picture(r, 40, 6, "lit"), palette=ramp)
    # Zero-length pictures: no rows, no pixels; rows of nothing; a destination of no size (the assertion).
    yield rd_case(4, 20, 0, b"", palette=ramp)
    yield rd_case(5, 0, 0, b"", dw=0, dh=0)
    yield rd_case(5, 0, 5, rle_row([]) * 5, dw=0, dh=7)
    yield rd_case(5, 0, 5, rle_row([]) * 5, dw=0, dh=5, pitch=16)
    yield rd_case(4, 20, 0, b"", dw=0, dh=0)
    for dw, dh in ((0, 5), (5, 0), (0, 0), (0, 28), (29, 0)):
        for fmt in (4, 5):
            yield rd_case(fmt, 29, 28, picture(r, 29, 28), dw=dw, dh=dh, calls=((0, 0), (1, 0)))
    # The destination's rows: packed (pitch 0 and exactly the row), with slack (odd and even), a pitch from the bottom (negative),
    # a start that is not aligned.
    for fmt in (4, 5):
        bpp = BPP[fmt]
        for pitch in (0, 20 * bpp, 20 * bpp + 1, 20 * bpp + 8, 20 * bpp + 33, 500, 1000):
            for start in (0x1000, 0x1001, 0x1002, 0x1003, 0x2000):
                yield rd_case(fmt, 20, 6, picture(r, 20, 6), pitch=pitch, start=start, palette=ramp)
        yield rd_case(fmt, 20, 6, picture(r, 20, 6), pitch=-20 * bpp, start=0xC000, palette=ramp)
        yield rd_case(fmt, 20, 6, picture(r, 20, 6), pitch=-20 * bpp - 8, start=0xC001, palette=ramp)
        # A picture taller than the destination: skipped rows; shorter: repeated rows (and only the picture's rows are made).
        for dh in (1, 2, 3, 4, 5, 6, 7, 9, 11, 12, 13, 17, 30):
            for pitch in (0, 20 * bpp, 20 * bpp + 12, -20 * bpp - 4):
                yield rd_case(fmt, 20, 6, picture(r, 20, 6), dh=dh, pitch=pitch, start=0xC000 if pitch < 0 else 0x1000, palette=ramp)
        # A different width: scaled down and up, in whole and in part ratios, with a different height too.
        for dw in (1, 2, 5, 10, 13, 19, 21, 25, 40, 41, 60, 129):
            for dh in (6, 3, 9):
                yield rd_case(fmt, 20, 6, picture(r, 20, 6), dw=dw, dh=dh, pitch=dw * bpp + 4 * (dw % 3), palette=ramp, calls=((0, 0), (1, 1)))
    # A token that runs past the end of the row: the unscaled picture shifts every later row; the scaled one takes it in its row buffer.
    for fmt in (4, 5):
        over = rle_row([tok_lit([1, 2, 3, 4, 5, 6])]) + rle_row([tok_run(7, 5)]) + rle_row([tok_lit([8, 9, 10, 11, 12, 13, 14])])
        yield rd_case(fmt, 4, 3, over, palette=ramp)
        yield rd_case(fmt, 4, 3, over, pitch=4 * BPP[fmt] + 8, palette=ramp)
        yield rd_case(fmt, 4, 3, over, dw=7, palette=ramp)
        yield rd_case(fmt, 4, 3, over, dw=2, dh=2, palette=ramp)
        yield rd_case(fmt, 6, 3, rle_row([tok_run(5, 100)]) * 3, palette=ramp)
        yield rd_case(fmt, 6, 3, rle_row([tok_run(5, 100)]) * 3, dw=9, palette=ramp)
    # A destination of its own size, with the picture's real shape, repeated calls and the answers of the other members.
    for fmt in (4, 5):
        yield rd_case(fmt, 29, 28, picture(r, 29, 28), calls=((0, 0), (0, 0), (1, 0), (2, 0), (1, 1)))
    # Random streams (any bytes are a stream): kept when the two sides can be compared.
    for _ in range(60):
        fmt = r.choice([4, 5, 4, 5, 0, 3])
        w, h = r.randrange(0, 40), r.randrange(0, 12)
        stream = bytes(r.getrandbits(8) for _ in range(r.randrange(0, 4000)))
        c = rd_case(fmt, w, h, stream, dw=r.choice([None, 1, 3, w + 3, max(w - 3, 1), 2 * w + 1]), dh=r.choice([None, 1, 5, h + 2, h * 2, 17]),
                    pitch=r.choice([0, 0, 200, 700, -300]), start=0xC000 if r.random() < 0.2 else r.choice([0x1000, 0x1001, 0x3003]),
                    palette=palette_for(r))
        if c["pitch"] < 0:
            c["start"] = 0xC000
        if usable(c):
            yield c


def rnd_unpack(r):
    for _ in range(1000):
        fmt = r.choice([4, 5, 4, 5, 4, 5, 0, 1, 2, 3])
        width = r.choice([1, 2, 3, 7, 8, 9, 16, 17, 29, 40, 64, 96, 128, r.randrange(1, 132)])
        height = r.choice([1, 2, 3, 5, 8, 9, 28, 40, r.randrange(1, 30)])
        style = r.choice(["lit", "run", "mixed", "mixed", "mixed"])
        stream = picture(r, width, height, style)
        if r.random() < 0.15:   # a token that overshoots the row
            stream = b"".join(rle_row(row_tokens(r, width + r.randrange(0, 20), style)) for _ in range(height))
        scale = r.random()
        if scale < 0.3:
            dw, dh = None, None
        elif scale < 0.6:
            dw, dh = r.choice([max(1, width // 2), width + r.randrange(1, 9), max(1, width - 1), width * 2, r.randrange(1, 100)]), None
        else:
            dw = r.choice([None, max(1, width // 2), width + 3, r.randrange(1, 100)])
            dh = r.choice([max(1, height // 2), height + 4, max(1, height - 1), height * 2, r.randrange(1, 60)])
        bpp = BPP[fmt]
        w_out = width if dw is None else dw
        pitch = r.choice([0, 0, w_out * bpp, w_out * bpp + r.randrange(1, 40), -w_out * bpp - r.randrange(0, 40)])
        calls = r.choice([((0, 0),), ((1, 0),), ((1, 1),), ((0, 0), (1, 0)), ((2, 0), (0, 0))])
        c = rd_case(fmt, width, height, stream, dw=dw, dh=dh, pitch=pitch, start=0xC000 if pitch < 0 else r.choice([0x1000, 0x1001, 0x1002, 0x1003, 0x2468]),
                    palette=palette_for(r, r.choice(["random", "random", "one", "ramp"])), calls=calls, seed=r.randrange(256))
        if usable(c):
            return c
    raise RuntimeError("no usable random case")


def real_pictures():
    """Every picture of the game's sprite files the test reads: (file name, picture number, width, height, stream, the file's palette)."""
    for name in REAL_FILES:
        data = real_file(name)
        if data is None:
            continue
        count = struct.unpack_from("<I", data, 4)[0]
        palette = data[8:8 + 0x400]
        pos = 8 + 0x400
        for i in range(count):
            size, w, h = struct.unpack_from("<IHH", data, pos)
            yield name, i, w, h, data[pos + 20:pos + 20 + size], palette
            pos += 20 + size


def edge_real():
    r = random.Random(92)
    pictures = list(real_pictures())
    for n, (name, i, w, h, stream, palette) in enumerate(pictures):
        if len(stream) > RD_DATA:
            continue
        fmt = 4 if n % 2 == 0 else 5
        bpp = BPP[fmt]
        # The picture's own size, packed, as the texture entry asks for it.
        c = rd_case(fmt, w, h, stream, palette=palette, pitch=w * bpp, seed=n & 0xFF)
        if usable(c):
            yield c
        if n % 4 == 0:   # and scaled the way the texture cache scales a sprite to a page: up to a power of two, down to half
            for dw, dh in ((128, 128), (max(1, w // 2), max(1, h // 2)), (w + 5, h), (w, h * 2), (32, 32)):
                c = rd_case(fmt, w, h, stream, dw=dw, dh=dh, palette=palette, pitch=dw * bpp, calls=((1, 0), (0, 0)), seed=n & 0xFF)
                if usable(c):
                    yield c
    # The sizes the files hold, one picture of each (the widest and the tallest included), in every format.
    seen = {}
    for name, i, w, h, stream, palette in pictures:
        seen.setdefault((w, h), (w, h, stream, palette))
    for (w, h), (w, h, stream, palette) in sorted(seen.items()):
        if len(stream) > RD_DATA:
            continue
        for fmt in (3, 5, 0):
            c = rd_case(fmt, w, h, stream, palette=palette, seed=w ^ h)
            if usable(c):
                yield c


TESTS += [
    FnTest("CRLELoad::Decompress, Blit, AverageColor: every token, sizes, pitches, scaling, malformed streams", ppc_rd, host_rd, edge=edge_unpack,
           rnd=rnd_unpack),
    FnTest("CRLELoad::Decompress on the game's own sprites (every size the files hold)", ppc_rd, host_rd, edge=edge_real, rnd=rnd_unpack),
]
