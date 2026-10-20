"""The engine's sprite files (original tb_sprite.cpp, tb_decode_rle.cpp and tb_decode_wavelet.cpp),
engine library: CSpriteFileIO::Load, Free and FlagAsSemitransparent, SSpriteEngineData (the
constructor, destructor, CreateTextureEntry, Load_WCT, Load_RLE, Init(int, int),
RefreshPartialTexture and Close), SSpriteGameData (the constructor, SetUsedPosition,
SetUsedSize), CSpriteList (the constructor, destructor, Init), CRLELoad (the constructor,
SetSize, SetPalette, LoadSprite) and CWaveLoad::LoadSprite. The texture entry
(CTextureEntry) and the wavelet loader's other members are the real ones on both sides.

A case is a script of operations run on both sides on the same set of objects (two sprite file
objects, four sprites, two sprite instances, three sprite lists, an RLE loader, a wavelet loader and
a standalone array of texture entries) and a file of the case's bytes. After every operation the
test compares the result, the events of the fakes (the engine allocator, the file callbacks, the
texture cache's Remove, the error handler, the assertion, the splash, the link base table, the
palette converter, the unpacking members of CRLELoad, CSpriteList::Close) and the state of every
object, read in the same order as tests/unit_SpriteFileIO.cpp writes the host's. The heap traffic
(operator new, new[], delete, delete[]) is part of the state.

The file. All fields of a kind 3 file are little-endian and swapped by the original, so both sides
read the same bytes. A kind 2 file's per-sprite header is read RAW by the original (no swap):
the PowerPC side is given it big-endian and the host side little-endian, so that the values are
the same (no kind 2 file exists in the game's data).
"""
import ctypes as C
import pathlib
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_SpriteFileIO"

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent

IO = dict(Load=0x1002da98, Free=0x1002df78, Flag=0x1002e068)
SPRITE = dict(ctor=0x1002e0e0, dtor=0x1002e138, CreateTextureEntry=0x1002e1c4, Load_WCT=0x1002e33c, Load_RLE=0x1002e514,
              Init=0x1002ea18, Refresh=0x1002ef10, Close=0x1002f00c)
GAME = dict(ctor=0x1002f144, SetUsedPosition=0x1002f19c, SetUsedSize=0x1002f2c0)
LIST = dict(ctor=0x1003108c, dtor=0x10031160, Init=0x10031378)
RLE = dict(ctor=0x10005104, SetSize=0x10005300, SetPalette=0x10005334, LoadSprite=0x10005368)
WAVE_SPRITE = 0x10006260
ENTRY = dict(ctor=0x10040d30, Create=0x10040e84, Close=0x10040f7c, GetBlendingMode=0x100410a4, BestSize=0x1004117c,
             GetBestSize=0x10041204, GetData=0x10041274, GetCorrectedData=0x10041334, SetSize=0x10041560,
             FlushEntryFlags=0x1004168c, Lock=0x10041738, Unlock=0x100417c0, StoreAsWrapping=0x10041848,
             GetVSize=0x10025d60, GetUSize=0x10025d98, GetVOffset=0x10025dd0, GetUOffset=0x10025e08,
             GetTexture=0x10025e40)
WAVE = dict(ctor=0x10005de4, dtor=0x10005ea8, LoadTexture=0x10005f68, Close=0x1000640c, Decompress=0x100064b8,
            AverageColor=0x1000660c, Blit=0x10006644, IsNaturallyFlipped=0x100067b8)
INNER = [0x10040dfc, 0x100051d0, 0x10005bf8]
ERROR_PUSH, CACHE_REMOVE, SPLASH = 0x10008904, 0x10034610, 0x1002d7a0
REGISTER_LINK, FREE_LINK, LIST_CLOSE, RLE_DTOR = 0x10019700, 0x10019768, 0x10031408, 0x10005244
P_MALLOC, P_FREE, FILE_OPEN, FILE_READ, FILE_CLOSE = 0x10051f70, 0x10051f74, 0x101735e4, 0x101735d8, 0x101735e0
SYSTEM, SETTINGS, G_TCACHE, HEADER = 0x10165f80, 0x10165fc8, 0x10173184, 0x10052ad4
RLE_VTABLE, WAVE_VTABLE = 0x1005164c, 0x100516d4
ENTRY_CTOR_TV, ENTRY_DTOR_TV = 0x100508b0, 0x100508a8
K_ON_PAGE, K_COMPRESSED = 0x1000, 0x10000000
MASK64 = 0xFFFFFFFFFFFFFFFF
MASK48 = 0xFFFFFFFFFFFF
FNV = 0xcbf29ce484222325

SLOTS, LISTS = 4, 3
PIXEL_BYTES = 0x40
SPRITE_SIZE, ENTRY_SIZE = 0x18, 0x5c


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if not hasattr(e, "sf_fns"):
        route = lambda name: (lambda emu: getattr(emu.sf_side, name)())
        e.sf_fns = {name: T.fake_function(e, route(name)) for name in ("cb_malloc", "cb_free", "cb_open", "cb_read", "cb_close")}


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
        e.sf_side = self
        self.file, self.position = bytes(case["file_ppc"]), 0
        self.wavelet = bytes(case["file_ppc"]) != bytes(case["file_host"])   # see tests/unit_SpriteFileIO.cpp
        self.io = [e.alloc(0x10, bytes(0x10)) for _ in range(2)]
        self.sprites = [e.alloc(SPRITE_SIZE, b"\xa5" * SPRITE_SIZE) for _ in range(3)] + [0]
        self.games = [e.alloc(0x44, b"\xa5" * 0x44) for _ in range(2)]
        self.lists = [e.alloc(0x28, b"\xa5" * 0x28) for _ in range(2)] + [0]
        self.rle = e.alloc(0x20, b"\xa5" * 0x20)
        self.wave = e.alloc(0x34, b"\xa5" * 0x34)
        self.cache, self.handle = e.alloc(16), e.alloc(8)
        self.pix = [e.alloc(PIXEL_BYTES, pattern(PIXEL_BYTES, 7, 1)), e.alloc(PIXEL_BYTES, pattern(PIXEL_BYTES, 5, 3))]
        self.palette = e.alloc(0x400, pattern(0x400, 3, 9))
        fixed = {self.cache: 3, self.pix[0]: 5, self.pix[1]: 6, self.handle: 9, self.rle: 11, self.wave: 12, self.palette: 13,
                 self.lists[0]: 31, self.lists[1]: 32}
        self.L = CL.PpcLog(fixed, case.get("q"))
        e.cl = self.L
        self.built = [False] * SLOTS
        self.own, self.len = [0] * SLOTS, [0] * SLOTS
        self.game_built = [False, False]
        self.list_built = [False] * LISTS
        self.rle_built = self.wave_built = False
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
        e.set_r(3, self.handle)

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
        st.real = (set(IO.values()) | set(SPRITE.values()) | set(GAME.values()) | set(LIST.values()) | set(RLE.values())
                   | {WAVE_SPRITE} | set(ENTRY.values()) | set(WAVE.values()) | set(INNER))
        st.internal(ERROR_PUSH, CL.recorder("Error", "xii"))
        st.internal(CACHE_REMOVE, lambda e: (L.rec("Cache.Remove", (L.lab(e.r(3)), self.entry_label(e.r(4)))), e.set_r(3, 1)))
        st.internal(SPLASH, CL.recorder("Splash.Interpolate", ""))
        st.internal(REGISTER_LINK, lambda e: (L.rec("RegisterLinkBase", (L.lab(e.r(3) - 0x20),)), e.set_r(3, 1)))
        st.internal(FREE_LINK, lambda e: (L.rec("FreeLinkBase", (L.lab(e.r(3) - 0x20),)), e.set_r(3, 1)))
        st.internal(LIST_CLOSE, CL.recorder("SpriteList.Close", "L", ret=lambda e, L: 1))

        def rle_dtor(e):
            this, flag = e.r(3), T.s16(e.r(4))
            L.rec("RLE.dtor", (L.lab(this),))
            if flag > 0:   # the delete that follows the destructor
                self.heap[2] += 1
                if L.lab(this) > 0:
                    L.rec("delete", (L.lab(this),))
                L.forget(this)
        st.internal(RLE_DTOR, rle_dtor)
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
        fns = e.sf_fns
        for a, v in ((P_MALLOC, fns["cb_malloc"]), (P_FREE, fns["cb_free"]), (FILE_OPEN, fns["cb_open"]),
                     (FILE_READ, fns["cb_read"]), (FILE_CLOSE, fns["cb_close"]), (SYSTEM, 0), (SETTINGS + 4, 4),
                     (G_TCACHE, self.cache)):
            w32(e, a, v)
        e.write(HEADER, bytes(0x2c))

    # ---- one operation
    def call(self, addr, *args):
        return T.call_ppc(self.e, addr, list(args))

    def source_of(self, kind):
        if kind == 1 and self.rle_built:
            return self.rle + 0x18
        if kind == 2 and self.wave_built:
            return self.wave + 0x2c
        return 0

    def entry_arg(self, kind):
        if kind == 1 and self.E and self.e_count >= 2:
            return self.E
        if kind == 2 and self.E and self.e_count >= 2:
            return self.E + ENTRY_SIZE
        return 0

    def entry_arg_ok(self, kind):
        return kind == 0 or self.entry_arg(kind) != 0

    def safe_to_free(self, slot, needs_texture):
        e, s = self.e, self.sprites[slot]
        count = struct.unpack(">H", e.read(s + 0x16, 2))[0]
        if count == 0 or (needs_texture and e.u32(s) == 0):
            return True
        return self.own[slot] == 1 and count <= self.len[slot] and e.u32(s) != 0

    def slot_of(self, addr):
        if addr == 0:
            return -1
        for i, s in enumerate(self.sprites):
            if s == addr:
                return i
        return -2

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
            if self.wavelet and a[1] > 0 and a[1] != 8:
                return -8
            self.position = min(a[1], len(self.file)) if a[1] >= 0 else len(self.file)
            return 0
        if op == 7:
            w32(e, SYSTEM, a[1])
            w32(e, SETTINGS + 4, a[2])
            return 0
        # ---- sprites
        if op == 10:
            if slot == 3:
                s = self.sprites[3] = self.new_scalar(SPRITE_SIZE)
            self.call(SPRITE["ctor"], s)
            self.built[slot] = True
            self.own[slot] = self.len[slot] = 0
            return 0
        if op == 11:
            if not self.built[slot] or self.own[slot] == 2 or not self.safe_to_free(slot, True):
                return -8
            self.call(SPRITE["dtor"], s, 1 if slot == 3 else 0)
            if slot == 3:
                self.sprites[3] = 0
                self.built[3] = False
            self.own[slot] = self.len[slot] = 0
            return 0
        if op == 12:
            if not self.built[slot] or not self.entry_arg_ok(a[6]):
                return -8
            source = self.source_of(a[3])
            if a[3] != 0 and (source == 0 or a[6] == 0):
                return -8
            entry = self.entry_arg(a[6])
            image = lambda kind: self.pix[kind - 1] if kind in (1, 2) else 0
            flags = (a[7] | K_COMPRESSED) if source else a[7]   # an entry that unpacks from a loader says so
            result = T.s32(self.call(SPRITE["CreateTextureEntry"], s, image(a[1]), image(a[2]), source, a[4], a[5], entry, flags)[0])
            self.own[slot] = 2 if entry else 1
            self.len[slot] = 1
            return result
        if op in (13, 14):
            kind = a[1] if op == 13 else a[2]
            if not self.built[slot] or not self.entry_arg_ok(kind) or self.position + (0x2c if op == 13 else 20) > len(self.file):
                return -8
            if (not self.wavelet or self.position < 8) if op == 13 else self.wavelet:
                return -8
            entry = self.entry_arg(kind)
            if op == 13:
                result = T.s32(self.call(SPRITE["Load_WCT"], s, self.handle, entry)[0])
            else:
                palette = u(self.io[0] + 0xc) if a[1] == 1 else self.palette if a[1] == 2 else 0
                result = T.s32(self.call(SPRITE["Load_RLE"], s, self.handle, palette, entry)[0])
            self.own[slot] = 2 if entry else 1
            self.len[slot] = 1
            return result
        if op == 15:
            if not self.built[slot]:
                return -8
            return T.s32(self.call(SPRITE["Init"], s, a[1], a[2])[0])
        if op == 16:
            if not self.built[slot] or not self.safe_to_free(slot, True):
                return -8
            count = struct.unpack(">H", e.read(s + 0x16, 2))[0]
            if u(s) != 0 and count != 0:
                self.own[slot] = self.len[slot] = 0
            return T.s32(self.call(SPRITE["Refresh"], s)[0])
        if op == 17:
            if not self.built[slot] or self.own[slot] == 2 or not self.safe_to_free(slot, True):
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
            return 0
        if op == 19:
            if not self.built[slot]:
                return -8
            w32(e, s, 0)
            self.own[slot] = self.len[slot] = 0
            return 0
        # ---- sprite instances
        if op == 20:
            self.call(GAME["ctor"], self.games[k])
            self.game_built[k] = True
            return 0
        if op in (21, 22):
            if not self.game_built[k]:
                return -8
            g = self.games[k]
            target = u(g + 0x28)
            sl = self.slot_of(target)
            if target != 0 and (sl < 0 or not self.built[sl] or not self.safe_to_free(sl, op == 21)):
                return -8
            used = struct.unpack(">hhhh", e.read(g + 0x30, 8))
            if op == 21:
                changed = used[2] != a[1] or used[3] != a[2]
            else:
                changed = used[0] != a[1] or used[0] != a[2]
            count = struct.unpack(">H", e.read(target + 0x16, 2))[0] if target else 0
            frees = target != 0 and changed and count != 0 and (op == 22 or u(target) != 0)
            self.call(GAME["SetUsedPosition" if op == 21 else "SetUsedSize"], g, a[1], a[2])
            if frees:
                self.own[sl] = self.len[sl] = 0
            return 0
        if op == 23:
            if not self.game_built[k]:
                return -8
            target = self.sprites[a[1] & 3] if a[1] >= 0 and self.built[a[1] & 3] else 0
            w32(e, self.games[k] + 0x28, target)
            return 0
        # ---- sprite lists
        if op in (30, 31, 32):
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
            self.unit = 4
            result = self.call(LIST["Init"], addr, a[1])[0]
            self.unit = 1
            return L.lab(result)
        # ---- the RLE loader
        if op == 40:
            self.call(RLE["ctor"], self.rle, 1)
            self.rle_built = True
            return 0
        if op in (41, 42, 43):
            if not self.rle_built or (op == 43 and self.wavelet):
                return -8
            if op == 41:
                self.call(RLE["SetSize"], self.rle, a[1], a[2])
                return 0
            if op == 42:
                self.call(RLE["SetPalette"], self.rle, self.palette if a[1] == 1 else self.pix[0] if a[1] == 2 else 0)
                return 0
            return T.s32(self.call(RLE["LoadSprite"], self.rle, self.handle, a[1])[0])
        # ---- the wavelet loader
        if op == 50:
            self.call(WAVE["ctor"], self.wave, 1)
            self.wave_built = True
            return 0
        if op in (51, 52):
            if not self.wave_built or (op == 51 and (not self.wavelet or self.position < 8 or self.position + 0x2c > len(self.file))):
                return -8
            if op == 51:
                return T.s32(self.call(WAVE_SPRITE, self.wave, self.handle)[0])
            self.call(WAVE["dtor"], self.wave, 0)
            return 0
        if op == 60:
            self.E = self.new_entries(a[1]) if a[1] > 0 else 0
            self.e_count = a[1] if a[1] > 0 else 0
            return 0
        return -99

    # ---- reading state (the order of tests/unit_SpriteFileIO.cpp)
    def entry_words(self, a, owner, hashing=False):
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
        block = u(owner + 4) if owner else 0
        if flags & K_COMPRESSED:
            out.append(-3)
            out += self.loader_words(data)
        elif data == 0:
            out.append(0)
        elif L.lab(data) > 0:
            out.append(L.lab(data))
        elif owner and block and block <= data < block + 0x100:
            out.append(1000 + data - block)
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
        if vtable == WAVE_VTABLE:
            return self.wave_words(source - 0x2c)
        return [-5]

    def wave_words(self, wave):
        e, L, u = self.e, self.L, self.e.u32
        first_word = lambda block: struct.unpack("<I", e.read(block, 4))[0] if block and L.lab(block) > 0 else 0
        out = [2, L.lab(wave)] + list(e.read(wave + 4, 4)) + list(struct.unpack(">6H", e.read(wave + 8, 12)))
        out += [u(wave + 0x14), u(wave + 0x18), u(wave + 0x1c), L.lab(u(wave + 0x20)), L.lab(u(wave + 0x24))]
        out += [first_word(u(wave + 0x24)), first_word(u(wave + 0x20))]
        return out

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
                out += self.entry_words(texture + ENTRY_SIZE * i, s)
        for k in range(2):
            if not self.game_built[k]:
                out.append(-7)
                continue
            g = self.games[k]
            out += [u(g + 0x10), s32(g + 0x24), self.slot_of(u(g + 0x28)), u(g + 0x2c)]
            out += list(struct.unpack(">hhhh", e.read(g + 0x30, 8)))
        for lst in range(LISTS):
            if not self.list_built[lst]:
                out.append(-7)
                continue
            a = self.lists[lst]
            out += [s32(a + 4), s32(a + 8), s32(a + 0xc), s32(a + 0x10), L.lab(u(a + 0x14)), s32(a + 0x18)]
        if self.rle_built:
            out += [L.lab(u(self.rle + 4)), L.lab(u(self.rle + 8)), s32(self.rle + 0xc), s32(self.rle + 0x10)]
        else:
            out.append(-7)
        out += self.wave_words(self.wave) if self.wave_built else [-7]
        hdr = e.read(HEADER, 0x2c)
        out += [struct.unpack(">I", hdr[0:4])[0]] + list(struct.unpack(">HHHH", hdr[4:12])) + list(struct.unpack(">II", hdr[12:20]))
        out += list(struct.unpack(">4I", hdr[20:36])) + list(struct.unpack(">II", hdr[36:44]))
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
    words = (P_MALLOC, P_FREE, FILE_OPEN, FILE_READ, FILE_CLOSE, SYSTEM, SETTINGS + 4, G_TCACHE)
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
    reset = lib["tpw_sf_reset"]
    reset.restype, reset.argtypes = None, [C.c_char_p, C.c_uint32, C.c_int32]
    op_fn = lib["tpw_sf_op"]
    op_fn.restype, op_fn.argtypes = C.c_int64, [C.c_int32, C.POINTER(C.c_int32)]
    state_fn = lib["tpw_sf_state"]
    state_fn.restype, state_fn.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    address = lib["tpw_sf_address"]
    address.restype, address.argtypes = C.c_void_p, [C.c_int32]
    data = bytes(case["file_host"])
    reset(data, len(data), int(bytes(case["file_ppc"]) != data))
    CL.host_begin(lib, {label: address(label) for label in (3, 5, 6, 9, 11, 12, 13, 31, 32)}, case.get("q"))
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


def wct_sprite(endian, width=64, height=32, used_width=128, used_height=64, offset_x=-5, offset_y=7, quality=(1.5, 2.25, 3.0, 5.5),
               data=b"WCTDATA1", alpha=b"ALPHA2", data_size=None, alpha_size=None, first=0x12345678):
    """One kind 2 sprite: the 0x2c-byte header in the byte order of the machine that reads it raw, the data, the alpha."""
    header = struct.pack(endian + "IHHHHII4fII", first, width, height, used_width, used_height, offset_x & 0xFFFFFFFF,
                         offset_y & 0xFFFFFFFF, *quality, len(data) if data_size is None else data_size,
                         len(alpha) if alpha_size is None else alpha_size)
    return header + data + alpha


def palette_bytes(seed=1):
    return bytes((i * 5 + seed) & 0xFF for i in range(0x400))


def rle_file(sprites, kind=3, second=3, count=None, palette=None):
    """A kind 3 file: it reads the same on both sides."""
    palette = palette_bytes() if palette is None else palette
    return le("HHI", kind & 0xFFFF, second & 0xFFFF, len(sprites) if count is None else count) + palette + b"".join(sprites)


def wct_files(sprites_logical, kind=2, second=3, count=None):
    """A kind 2 file, as bytes for the PowerPC side and for the host: sprites_logical are keyword dicts."""
    head = le("HHI", kind & 0xFFFF, second & 0xFFFF, len(sprites_logical) if count is None else count)
    return (head + b"".join(wct_sprite(">", **s) for s in sprites_logical),
            head + b"".join(wct_sprite("<", **s) for s in sprites_logical))


def same(data):
    return dict(file_ppc=data, file_host=data)


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


def fb(x):
    return T.f32_bits(x)


def rnd_rle_sprite(r):
    data = bytes(r.getrandbits(8) for _ in range(r.choice([0, 1, 4, 8, 20, 60])))
    return rle_sprite(width=r.choice([0, 1, 29, 64, 128, 0xFFFF, 255]), height=r.choice([0, 1, 28, 64, 128, 0x8000, 17]),
                      used_width=r.choice([0, 128, 77, 0xFFFF]), used_height=r.choice([0, 128, 66, 0x8001]),
                      offset_x=r.choice([0, -12, 5, 0x12345, -0x12345, -1, 0x7FFF, 0x8000, 0xFFFF]),
                      offset_y=r.choice([0, -27, 9, 0x23456, -0x23456, -1, 0x7FFF, 0x8000]), data=data)


def rnd_wct_logical(r):
    quality = tuple(r.choice([0.0, 1.0, 1.5, 2.0, 2.25, 3.0, 5.5, 17.0, 100.5, -3.0, 3e8, -1e9, -3.0e38, float("-inf"), float("nan"), 0.5, 1e-3, r.uniform(-50, 200)])
                    for _ in range(4))
    return dict(width=r.choice([0, 1, 64, 128, 0xFFFF]), height=r.choice([0, 1, 32, 128, 0x8000]),
                used_width=r.choice([0, 128, 0xFFFF]), used_height=r.choice([0, 64, 0x8001]),
                offset_x=r.choice([0, -5, 0x12345, -0x12345]), offset_y=r.choice([0, 7, 0x23456, -0x23456]),
                quality=quality, data=bytes(r.getrandbits(8) for _ in range(r.choice([0, 1, 4, 12, 40]))),
                alpha=bytes(r.getrandbits(8) for _ in range(r.choice([0, 1, 4, 9, 33]))), first=r.getrandbits(32))


def rnd_file(r):
    """A sprite file for a random case; sometimes cut short inside the last picture's data."""
    kind = r.choice([3, 3, 3, 3, 2, 2, 2, 2, 1, 4, 0, -1, 0x7FFF, 0x100])
    count = r.choice([0, 1, 2, 3, 5, 8, 12])
    if kind == 2:
        logical = [rnd_wct_logical(r) for _ in range(count)]
        ppc_bytes, host_bytes = wct_files(logical, kind=2, second=r.choice([0, 3, 9]))
        last = logical[-1] if count else None
        if r.random() < 0.15 and last and len(last["data"]) + len(last["alpha"]) > 0:
            cut = r.randrange(1, len(last["data"]) + len(last["alpha"]) + 1)   # inside the last picture's blocks
            ppc_bytes, host_bytes = ppc_bytes[:-cut], host_bytes[:-cut]
        return dict(file_ppc=ppc_bytes, file_host=host_bytes)
    sprites = [rnd_rle_sprite(r) for _ in range(count)]
    data = rle_file(sprites, kind=kind, second=r.choice([0, 3, 9]), palette=palette_bytes(r.randrange(256)))
    if kind == 3 and count and r.random() < 0.15:
        # Short inside the last picture's RLE data: the reads of its header words still come up full.
        extra = len(sprites[-1]) - 20
        if extra > 0:
            data = data[:len(data) - r.randrange(1, extra + 1)]
    if r.random() < 0.08:
        data = data[:r.randrange(0, 5)]   # inside the file's own header, before the count
    return same(data)


def entry_kind(r):
    return r.choice([0, 0, 0, 1, 2])


def image_kind(r):
    return r.choice([0, 0, 1, 2])


def flags_arg(r):
    # Not the flags that make an entry a loader's (compressed) or give it a further plane.
    return r.choice([0, 0x100, 0x200, 0x300, 0x1000, 0x4000, 0x40000, 0x80000, 0x4000000, r.getrandbits(32)]) & ~(0x10000000 | 0x00800000) & 0xFFFFFFFF


def sprite_ops(r):
    kind = r.choice([10, 10, 11, 12, 12, 13, 14, 14, 14, 15, 16, 16, 17, 17, 18, 18, 18, 19, 23])
    slot = r.randrange(4)
    if kind in (10, 11, 16, 17, 19):
        return (kind, [slot])
    if kind == 12:
        return (12, [slot, image_kind(r), image_kind(r), r.choice([0, 0, 1, 2]), r.choice([0, 1, 8, 29, 64, 300, -4]),
                     r.choice([0, 1, 8, 28, 64, 5000]), entry_kind(r), flags_arg(r)])
    if kind == 13:
        return (13, [slot, entry_kind(r)])
    if kind == 14:
        return (14, [slot, r.choice([0, 1, 2]), entry_kind(r)])
    if kind == 15:
        return (15, [slot, r.choice([0, 1, 4, 16, 33, 65535, 0x10001, -1]), r.choice([0, 1, 4, 8, 33, 65535, 3])])
    if kind == 18:
        n = r.choice([0, 1, 2, 3, 5])
        return (18, [slot, r.randrange(2), n, r.getrandbits(5), r.choice([-1, -1, -1, 0, 1, 2, 5, 9])])
    return (23, [r.randrange(2), r.choice([-1, 0, 1, 2, 3])])


def game_ops(r):
    kind = r.choice([20, 21, 21, 21, 22, 22, 22, 23])
    k = r.randrange(2)
    values = [0, 1, -1, 5, 128, 0x7FFF, 0x8000, 0x10000, -0x8001, 77]
    if kind == 20:
        return (20, [k])
    if kind == 23:
        return (23, [k, r.choice([-1, 0, 1, 2, 3])])
    return (kind, [k, r.choice(values), r.choice(values)])


def list_ops(r):
    kind = r.choice([30, 30, 31, 32, 32])
    lst = r.randrange(3)
    if kind == 32:
        return (32, [lst, r.choice([0, 1, 5, 64, 1000])])
    return (kind, [lst])


def loader_ops(r):
    kind = r.choice([40, 41, 42, 43, 43, 50, 51, 51, 52])
    if kind == 41:
        return (41, [0, r.choice([0, 29, 128, -1]), r.choice([0, 28, 0x10000])])
    if kind == 42:
        return (42, [0, r.choice([0, 1, 2])])
    if kind == 43:
        return (43, [0, r.choice([0, 4, 8, 20, 100, 0x4000, 0x20000, -1])])
    return (kind, [0])


def file_ops(r):
    kind = r.choice([1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 5, 7, 60])
    k = r.randrange(2)
    if kind == 5:
        return (5, [0, r.choice([0, 0, 0, 8, 8 + 0x400, 5, -1])])
    if kind == 7:
        return (7, [0, r.choice([0, 0, 1, 0x40000000]), r.choice([4, 4, 5, 3, 0, 1])])
    if kind == 60:
        return (60, [0, r.choice([0, 2, 3, 4])])
    return (kind, [k])


def queues(r, fail):
    q = {}
    if fail:
        q["Malloc"] = [1 if r.random() < 0.7 else 0 for _ in range(r.randrange(1, 8))] + [1]
    return q


def case_of(r, makers, length, fail=False, prefix=()):
    script = [(60, [0, 4])] + list(prefix)
    script += [(10, [s]) for s in range(3)] + [(20, [0]), (20, [1]), (30, [0]), (30, [1]), (40, [0]), (50, [0])]
    case = dict(q=queues(r, fail))
    case.update(rnd_file(r))
    wavelet = case["file_ppc"] != case["file_host"]
    for _ in range(length):
        script.append(r.choice(makers)(r))
        if r.random() < 0.2:
            script.append((5, [0, r.choice([0, 8]) if wavelet else 0]))
    script += [(3, [0]), (3, [1])]
    case["script"] = script
    return case


# ----------------------------------------------------------------- edge cases


def sprites_small(n, seed=0):
    return [rle_sprite(width=10 + i, height=20 + seed + i, offset_x=-i, offset_y=-2 * i - 1, data=bytes([i + 1] * (3 + i))) for i in range(n)]


def load_script(extra=(), formats=(4,), flags=0):
    script = [(7, [0, flags, formats[0]])]
    script += [(5, [0, 0]), (2, [0])]
    script += list(extra)
    return script


def edge_file():
    r = random.Random(71)
    base = [(60, [0, 4])]
    # A kind 3 file in each texture format and with the splash flag; then Free, and again.
    for fmt in (4, 5, 3, 0, 1, 6, 7, 100, -1):   # the palette becomes 16 bits, 32 bits or nothing the converter has
        for flags in (0, 1, 0x80000001):
            yield dict(script=base + load_script(formats=(fmt,), flags=flags, extra=[(4, [0]), (3, [0]), (3, [0])]),
                       **same(rle_file(sprites_small(3))))
    # Counts: none, one, many.
    for n in (0, 1, 2, 7, 30):
        yield dict(script=base + load_script(extra=[(4, [0]), (3, [0]), (2, [0]), (3, [0])]), **same(rle_file(sprites_small(n))))
    # The sprite header values at their edges.
    edges = [rle_sprite(width=w, height=h, used_width=uw, used_height=uh, offset_x=ox, offset_y=oy, data=b"x" * (n % 5))
             for n, (w, h, uw, uh, ox, oy) in enumerate([(1, 1, 1, 1, 0, 0), (0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x7FFF, 0x8000),
                                                         (0, 5, 0, 0, -1, 0xFFFF), (5, 0, 5, 5, 0x12345678, -0x12345678),
                                                         (256, 128, 256, 128, 32767, 32768)])]
    yield dict(script=base + load_script(extra=[(4, [0]), (3, [0])]), **same(rle_file(edges)))
    # Other kinds of file: the version assertion, files with nothing to load.
    for kind in (-1, 0, 1, 3, 4, 5, 0x100, 0x7FFF, -0x8000):
        yield dict(script=base + load_script(extra=[(4, [0]), (3, [0])]), **same(rle_file(sprites_small(2), kind=kind)))
    # A file of kind 2.
    logical = [dict(width=64 + i, height=32, quality=(1.5 + i, 2.25, 3.0, 5.5 + i), data=b"D" * (4 + i), alpha=b"A" * (2 + i)) for i in range(4)]
    ppc_bytes, host_bytes = wct_files(logical)
    for flags in (0, 1):
        yield dict(script=base + load_script(flags=flags, extra=[(4, [0]), (3, [0]), (3, [0])]), file_ppc=ppc_bytes, file_host=host_bytes)
    # Truncated files: inside the header (no picture is read: a header word that comes up short is stack in the
    # original), inside the last picture's data.
    whole = rle_file(sprites_small(3))
    for cut in (0, 1, 2, 3, 4):
        yield dict(script=base + load_script(extra=[(3, [0])]), **same(whole[:cut]))
    for cut in (1, 2, 3, 5):
        yield dict(script=base + load_script(extra=[(4, [0]), (3, [0])]), **same(whole[:-cut]))
    # The allocator fails for the palette, and for the last picture's data (earlier ones would leave the
    # reads out of step with the file).
    for queue in ([0, 1], [1, 1, 1, 0, 1]):
        yield dict(script=base + load_script(extra=[(4, [0]), (3, [0])]), q={"Malloc": queue}, **same(rle_file(sprites_small(3))))
    yield dict(script=base + load_script(extra=[(3, [0])]), q={"Malloc": [1, 0]}, **same(rle_file(sprites_small(1))))
    # Loading twice into one object (after a Free), and into both.
    yield dict(script=base + [(5, [0, 0]), (2, [0]), (5, [0, 0]), (2, [0]), (3, [0]), (2, [0]), (2, [1]), (5, [0, 0]), (3, [0]), (2, [1]),
                              (5, [0, 0]), (2, [0]), (3, [1]), (3, [0])], **same(rle_file(sprites_small(2))))
    yield dict(script=base + [(3, [0]), (4, [0]), (3, [1]), (3, [0])], **same(b""))
    # The game's own files.
    for name in REAL_FILES:
        data = real_file(name)
        if data is None:
            continue
        for fmt, flags in ((4, 0), (5, 1)):
            yield dict(script=base + load_script(formats=(fmt,), flags=flags, extra=[(4, [0]), (3, [0])]), **same(data))
        # Several pictures, then loaded through the sprite header alone.
        yield dict(script=base + [(5, [0, 8 + 0x400]), (10, [0]), (14, [0, 2, 0]), (10, [1]), (14, [1, 0, 1]), (17, [0]), (11, [0])],
                   **same(data))
    for _ in range(25):
        yield case_of(r, [file_ops], 14)


def edge_sprites():
    r = random.Random(72)
    base = [(60, [0, 4]), (10, [0]), (10, [1]), (10, [2]), (10, [3])]
    # CreateTextureEntry: images, alpha, sources, sizes, entries, flags.
    for entry in (0, 1, 2):
        for image in (0, 1):
            for alpha in (0, 2):
                for flags in (0, 0x1000, 0x300, 0x4000000):
                    for w, h in ((1, 1), (16, 8), (300, 256), (0, 0), (-1, 70000)):
                        source = 0 if entry == 0 else 1
                        yield dict(script=base + [(40, [0]), (12, [0, image, alpha, source, w, h, entry, flags]), (17, [0]), (11, [3])],
                                   **same(b""))
    # Init: each format, sizes, failure.
    for fmt in (0, 3, 4, 5):
        for w, h in ((0, 0), (1, 1), (64, 33), (0xFFFF, 0xFFFF), (0x10001, 3), (-1, 5)):
            yield dict(script=base + [(7, [0, 0, fmt]), (15, [0, w, h]), (17, [0]), (11, [3])], **same(b""))
    yield dict(script=base + [(15, [0, 8, 8]), (17, [0])], q={"Malloc": [0]}, **same(b""))
    # Partial textures and the closing members.
    for data_mode in (0, 1):
        for n in (0, 1, 2, 3, 5):
            for bits in (0, 1, 0x15, 0x1f):
                for count in (-1, 0, 1):
                    yield dict(script=base + [(18, [0, data_mode, n, bits, count]), (16, [0]), (16, [0]), (18, [1, data_mode, n, bits, count]),
                                              (17, [1]), (17, [1]), (18, [2, data_mode, n, bits, count]), (11, [2]), (11, [2])],
                               **same(b""))
    # A sprite made by CreateTextureEntry owns an array of one.
    yield dict(script=base + [(12, [0, 1, 0, 0, 16, 16, 0, 0x300]), (17, [0]), (12, [1, 1, 2, 0, 8, 8, 0, 0x4000]), (11, [1]),
                              (11, [3]), (10, [3]), (15, [3, 4, 4]), (11, [3])], **same(b""))
    for _ in range(30):
        yield case_of(r, [sprite_ops, sprite_ops, file_ops], 16)


def edge_game_lists():
    r = random.Random(73)
    base = [(60, [0, 4]), (10, [0]), (10, [1]), (10, [2]), (10, [3]), (20, [0]), (20, [1]), (30, [0]), (30, [1])]
    values = [0, 1, -1, 7, 0x7FFF, 0x8000, 0x10000, -0x8000, -0x8001, 0x12345678]
    # SetUsedPosition and SetUsedSize: with no sprite, with one that has nothing, with partial textures.
    for sprite in (-1, 0, 1, 3):
        for n, bits, count in ((0, 0, -1), (1, 1, -1), (3, 5, -1), (3, 0, 2), (2, 3, 0)):
            for x, y in ((0, 0), (5, 0), (0, 5), (5, 5), (0x10000, 0), (-1, -1)):
                yield dict(script=base + [(18, [0, 1, n, bits, count]), (18, [1, 0, n, bits, count]), (23, [0, sprite]), (23, [1, sprite]),
                                          (21, [0, x, y]), (21, [0, x, y]), (22, [1, x, y]), (22, [1, y, y]), (22, [1, y, x]),
                                          (21, [1, y, x]), (17, [0]), (17, [1])], **same(b""))
    for x in values:
        for y in values:
            yield dict(script=base + [(21, [0, x, y]), (22, [0, x, y]), (21, [1, y, x]), (22, [1, y, x])], **same(b""))
    # SetUsedSize looks at the used width only (twice): from a stored size, a call whose width and height both equal the
    # stored width changes nothing even when the stored height differs; one whose width differs, or whose height
    # does not equal that width, frees the partial textures.
    for stored in ((5, 7), (5, 5), (7, 5), (0, 0)):
        for call in ((5, 5), (5, 7), (7, 7), (7, 5), (0, 0), (0, 7)):
            yield dict(script=base + [(23, [0, 0]), (23, [1, 0]), (18, [0, 1, 3, 5, -1]), (22, [0, stored[0], stored[1]]),
                                      (18, [0, 1, 3, 5, -1]), (22, [0, call[0], call[1]]), (22, [0, call[0], call[1]]),
                                      (18, [0, 1, 2, 3, -1]), (22, [1, stored[0], stored[1]]), (22, [1, call[1], call[0]]), (17, [0])],
                       **same(b""))
    # The list: construction, Init at sizes and failures, destruction (static and made with new).
    for lst in range(3):
        for count in (0, 1, 7, 100, 0x40000):
            yield dict(script=base + [(30, [lst]), (32, [lst, count]), (32, [lst, count + 1]), (31, [lst]), (31, [lst])], **same(b""))
        yield dict(script=base + [(30, [lst]), (31, [lst]), (30, [lst]), (31, [lst])], **same(b""))
        yield dict(script=base + [(30, [lst]), (32, [lst, 5]), (31, [lst])], q={"Malloc": [0]}, **same(b""))
    for _ in range(40):
        yield case_of(r, [game_ops, game_ops, sprite_ops, list_ops, list_ops], 18)


def edge_loaders():
    r = random.Random(74)
    base = [(60, [0, 4])]
    # CRLELoad: construction, the setters, LoadSprite at sizes, twice (the assertion), failure.
    for size in (0, 1, 4, 100, 0x10000, 0x10001, 0x7FFFFFFF, -1):
        yield dict(script=base + [(40, [0]), (41, [0, 29, 28]), (42, [0, 1]), (43, [0, size]), (43, [0, size]), (42, [0, 0]), (41, [0, -1, 0x10000])],
                   **same(le("I", 0x01020304) + bytes(range(256)) * 80))
    yield dict(script=base + [(40, [0]), (43, [0, 8])], q={"Malloc": [0]}, **same(b"x" * 20))
    yield dict(script=base + [(41, [0, 1, 2]), (42, [0, 1]), (43, [0, 8])], **same(b"x" * 20))
    # CWaveLoad::LoadSprite: scale values, sizes, failures, truncated, twice. (Not above 2^31 after the scaling: the
    # emulator's fctiwz answers 0x80000000 there where the processor saturates.)
    qualities = [(0.0, 0.0, 0.0, 0.0), (1.0, 1.0, 1.0, 1.0), (1.5, 2.25, 3.0, 5.5), (2.0, 2.0, 2.0, 2.0), (3e8, -1e9, 5e8, -3.4e38),
                 (float("nan"), float("-inf"), float("nan"), 1.0), (0.5, 0.25, 0.125, 0.0625), (-1.0, -2.0, -3.0, -4.0),
                 (32767.5, 32768.5, 16384.25, 65537.0), (1.0000001, 2.0000002, 3.0000005, 4.0000005)]
    seek = [(5, [0, 8])]
    for quality in qualities:
        ppc_bytes, host_bytes = wct_files([dict(quality=quality, data=b"DATA", alpha=b"ALPHA")])
        yield dict(script=base + seek + [(50, [0]), (51, [0]), (52, [0])], file_ppc=ppc_bytes, file_host=host_bytes)
    for sizes in ((0, 0), (1, 0), (0, 1), (8, 8), (0x10000, 4), (0x10001, 4), (4, 0x10001), (0xFFFFFFFF, 0xFFFFFFFF)):
        ppc_bytes, host_bytes = wct_files([dict(data=b"DATA1234", alpha=b"AL", data_size=sizes[0], alpha_size=sizes[1])])
        yield dict(script=base + seek + [(50, [0]), (51, [0]), (5, [0, 8]), (51, [0]), (52, [0])], file_ppc=ppc_bytes, file_host=host_bytes)
    ppc_bytes, host_bytes = wct_files([dict()])
    for failing in range(3):
        yield dict(script=base + seek + [(50, [0]), (51, [0]), (52, [0])], q={"Malloc": [1] * failing + [0, 1]}, file_ppc=ppc_bytes,
                   file_host=host_bytes)
    for cut in (0, 44, 45, 50):   # short inside the blocks only: a cut header would be half of each byte order
        yield dict(script=base + seek + [(50, [0]), (51, [0]), (52, [0])], file_ppc=ppc_bytes[:8 + cut], file_host=host_bytes[:8 + cut])
    yield dict(script=base + seek + [(51, [0])], file_ppc=ppc_bytes, file_host=host_bytes)
    # Load_WCT and Load_RLE with entries of the test's, and the sprite's state.
    ppc_bytes, host_bytes = wct_files([dict(width=64, height=32), dict(width=0, height=0), dict(width=3, height=5)])
    for entry in (0, 1, 2):
        yield dict(script=base + [(10, [0]), (10, [1]), (5, [0, 8]), (13, [0, entry]), (13, [1, entry]), (13, [0, entry]), (17, [0]), (11, [1])],
                   file_ppc=ppc_bytes, file_host=host_bytes)
    rle_data = rle_file(sprites_small(3))
    for entry in (0, 1, 2):
        for palette in (0, 1, 2):
            yield dict(script=base + [(10, [0]), (10, [1]), (5, [0, 8 + 0x400]), (14, [0, palette, entry]), (14, [1, palette, entry]), (17, [0]), (11, [1])],
                       **same(rle_data))
    for _ in range(40):
        yield case_of(r, [loader_ops, loader_ops, sprite_ops, file_ops], 18)


def safe_ops(maker, banned):
    """An operation generator that leaves out the operations in `banned`."""
    def make(r):
        while True:
            op = maker(r)
            if op[0] not in banned:
                return op
    return make


# With a failing allocator no picture is read: a picture block that is not made leaves the file where it was, and
# what follows is then read as a header (stack garbage when it comes up short, and in the byte order of the other
# machine for the raw wavelet headers).
FAILING = [safe_ops(sprite_ops, (13, 14)), safe_ops(sprite_ops, (13, 14)), game_ops, list_ops, safe_ops(loader_ops, (51,)),
           safe_ops(file_ops, (2,))]


def failing(r):
    case = case_of(r, FAILING, 30, fail=True)
    return case


def edge_failing():
    r = random.Random(75)
    base = [(60, [0, 4]), (10, [0]), (10, [1]), (10, [2]), (10, [3])]
    for n in range(0, 5):
        yield dict(script=base + [(18, [0, 1, 3, 5, -1]), (18, [1, 1, 2, 1, -1]), (15, [2, 8, 8]), (15, [3, 4, 4]), (32, [0, 5]), (30, [0]),
                                  (32, [0, 5]), (30, [2]), (32, [2, 3]), (17, [0]), (17, [1]), (11, [2]), (11, [3]), (31, [0]), (31, [2])],
                   q={"Malloc": [1] * n + [0, 1, 0]}, **same(b""))
    for _ in range(40):
        yield failing(r)


def wave_ops(r):
    """Operations for a wavelet sprite file, whose raw headers are only read whole from a header."""
    kind = r.choice([13, 13, 13, 51, 51, 5, 5, 50, 52, 10, 17, 11, 12, 2, 3, 4, 1, 60])
    slot = r.randrange(4)
    if kind == 5:
        return (5, [0, r.choice([0, 8, 8, -1, 100])])
    if kind == 13:
        return (13, [slot, entry_kind(r)])
    if kind == 12:
        return (12, [slot, image_kind(r), image_kind(r), r.choice([0, 0, 1, 2]), r.choice([0, 8, 300]), r.choice([0, 8, 28]), entry_kind(r),
                     flags_arg(r)])
    if kind == 60:
        return (60, [0, r.choice([0, 2, 3, 4])])
    if kind in (10, 11, 17):
        return (kind, [slot])
    return (kind, [r.randrange(2)] if kind in (1, 2, 3, 4) else [0])


def wave_case(r):
    """A script on a file of wavelet pictures (the sprite header alone, or the whole file through Load)."""
    case = case_of(r, [wave_ops], 40)
    logical = [rnd_wct_logical(r) for _ in range(r.choice([1, 2, 3, 5, 8]))]
    ppc_bytes, host_bytes = wct_files(logical, second=r.choice([0, 3, 9]))
    last = logical[-1]
    if r.random() < 0.2 and len(last["data"]) + len(last["alpha"]) > 0:
        cut = r.randrange(1, len(last["data"]) + len(last["alpha"]) + 1)
        ppc_bytes, host_bytes = ppc_bytes[:-cut], host_bytes[:-cut]
    case.update(file_ppc=ppc_bytes, file_host=host_bytes)
    return case


def mixed(r):
    return case_of(r, [file_ops, sprite_ops, game_ops, list_ops, loader_ops], 200)


TESTS = [
    FnTest("sprite file reader: CSpriteFileIO Load, Free, FlagAsSemitransparent", ppc, host, edge=edge_file,
           rnd=lambda r: case_of(r, [file_ops], 14)),
    FnTest("sprite engine data: constructor, destructor, CreateTextureEntry, Load_WCT, Load_RLE, Init, RefreshPartialTexture, Close",
           ppc, host, edge=edge_sprites, rnd=lambda r: case_of(r, [sprite_ops, sprite_ops, file_ops], 16)),
    FnTest("sprite instances and lists: SSpriteGameData, CSpriteList", ppc, host, edge=edge_game_lists,
           rnd=lambda r: case_of(r, [game_ops, game_ops, sprite_ops, list_ops, list_ops], 18)),
    FnTest("loaders: CRLELoad, CWaveLoad::LoadSprite, and the loaders through the sprites", ppc, host, edge=edge_loaders,
           rnd=lambda r: case_of(r, [loader_ops, loader_ops, sprite_ops, file_ops], 18)),
    FnTest("wavelet sprite files: Load_WCT, CWaveLoad::LoadSprite", ppc, host, rnd=wave_case),
    FnTest("failing allocator: Init, partial textures, lists, loaders", ppc, host, edge=edge_failing, rnd=failing),
    FnTest("sprite files, mixed scripts of 200 operations", ppc, host, rnd=mixed),
]
