"""Texture pages, the texture cache's start-up and shutdown, and the OpenGL draw
primitive's state (engine library): CTextureBase, COpenGLTexture, CTextureLink,
CTextureCache (constructor, page creation, Init, Close, Flush), the rest of COpenGL's
renderer interface, CDrawPrimitive's constructor and COpenGLDrawPrimitive's Init and
state setters.

A case is a script of operations run on both sides on a texture cache, four texture
pages of the test's own, a draw primitive and up to sixteen planted texture entries.
After every operation the test compares the result, the recorded calls (every OpenGL
call with its arguments, operator new and delete, errors) and the state of all of them,
read in the same order as tests/unit_TexturePages.cpp writes the host's.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_TexturePages"

FN = dict(
    TCache_Init=0x1003377c, TCache_Close=0x10033854, InitFrequencyTable=0x1000967c, Cache=0x100338d4,
    AutoDetectPageCreation=0x10033b68, AutoCreateSurfaces=0x10033ecc, CreateSurfaces=0x1003407c,
    Init=0x1003439c, Close=0x10034514, Remove=0x10034610, Flush=0x100347f4, SetUITextureSize=0x10034b48,
    GetFlags=0x10032b98, SetFlags=0x10032bd0, IsPositionLocked=0x10032c14, IsWrapping=0x10032c58,
    IsUsingMaxCompression=0x10032c94, PositionLock=0x10032cdc, PositionUnlock=0x10032d20,
    TextureDtor=0x10045e58, TestUploadSpeed=0x10045f24, Create=0x1004602c, Destroy=0x100461d4,
    Select=0x10046264, SetState=0x10046308, LockSurface=0x10046480, UnlockSurface=0x100464d0,
    MipmapsUsed=0x10046a58, GetPixelCount=0x10046a94, GetTexture=0x10046adc, FlipScreen=0x1004577c,
    GetVRAMSize=0x100457e8, CreateTextureBase=0x10045824, GetTextureMemory=0x100458ac, DrawBase=0x10006954,
    DrawInit=0x10042f0c, BeginDraw=0x100430b8, EndDraw=0x10043140, SetTexture=0x100432ec,
    SetBlendingMode=0x100433c0, SetFogParameters=0x100437c0, SupportsOnePassMultiTexture=0x10044e98,
    ExecuteShadow=0x10044fe8)
# Members the entry points reach (and ConvertSurfaceSizeToFlag, which runs as the original).
INNER = [0x100385a8, 0x10032b58, 0x10032d64, 0x10045d88, 0x100069e0, 0x10044ee8, 0x10029220]
ERROR_PUSH, MAX_TEXTURES, SET_FOG_COLOR, SET_FOG_SPACE, CLOCK = 0x10008904, 0x10045a90, 0x100285c8, 0x10028558, 0x1001fc8c

G_TCACHE, SETTINGS, SYSTEM, DISPLAY, G_DDRAW, G_OPENGL = 0x10173184, 0x10165fc8, 0x10165f80, 0x10165634, 0x10052ad0, 0x102b3640
UPLOAD_DATA, BLEND, FREQUENCY, NVIDIA = 0x102b3648, 0x100520fc, 0x10052d78, 0x10052b38
OPENGL_VTABLE, DRAW_VTABLE, DRAW_BASE_VTABLE = 0x1005235c, 0x100522a0, 0x100522f0
UPLOAD, GL_INSTANCE = 0x7c0fd200, 0x7c0fd210          # labels 5 and 6
HEAP_BASE, HEAP_SIZE = 0x60000000, 0x4000000
TEXTURES, ENTRIES = 4, 16
MINUS_ONE = 0xFFFFFFFF
CACHE_OPS = set(range(1, 13)) | {15, 16}               # the operations that can change the cache's pages
PAGE_OPS = {1, 2, 6, 8, 9, 10, 11}                     # the operations that can create or delete pages


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if not hasattr(e, "es_heap"):                      # shared with test_EngineStartup
        e.uc.mem_map(HEAP_BASE, HEAP_SIZE)
        e.es_heap = HEAP_BASE


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def is_bulk(script):
    """A script that builds a whole cache: its OpenGL calls and allocations are not logged."""
    return any(op in (1, 9, 10) for op, _ in script)


# ----------------------------------------------------------------- PowerPC side


class PpcSide:
    def __init__(self, e, case):
        self.e, self.L = e, CL.PpcLog({UPLOAD: 5, GL_INSTANCE: 6}, case.get("q"))
        e.cl = self.L
        e.es_heap = HEAP_BASE
        self.bulk = is_bulk(case["script"])
        self.next_name = 1001
        self.textures = [0] * TEXTURES          # CTextureBase pointers
        self.draw = 0
        self.entries = [0] * ENTRIES
        self.page_alive = [False] * ENTRIES      # whether a planted entry's page still exists
        self.places = [0] * ENTRIES
        self.cache_state = None

    def heap(self, size):
        e = self.e
        a = (e.es_heap + 15) & ~15
        e.es_heap = a + size + 64
        if e.es_heap > HEAP_BASE + HEAP_SIZE:
            raise RuntimeError("the case's PowerPC heap is used up")
        return a

    def gl(self, name, ints=()):
        if not self.bulk:
            self.L.rec(name, ints)

    def op_new(self, e):
        size = e.r(3)
        # Only a texture page (0x34 bytes) or the cache (0x1910) may fail: the original
        # checks those for NULL and writes through a page link's pointer unchecked.
        if size in (0x34, 0x1910) and not self.L.ret("New", 1):
            e.set_r(3, 0)
            return
        block = self.heap(size)
        e.write(block, b"\xa5" * size)
        label = self.L.newlab(block)
        if not self.bulk:
            self.L.rec("new", (label,))
        e.set_r(3, block)

    def op_delete(self, e):
        block = e.r(3)
        if self.L.lab(block) > 0:
            self.L.rec("delete", (self.L.lab(block),))
        self.L.forget(block)

    def stubs(self):
        e, L, gl = self.e, self.L, self.gl
        st = CL.Stubs()
        st.real = set(FN.values()) | set(INNER)
        st.internal(ERROR_PUSH, CL.recorder("Error", "xii"))
        st.internal(MAX_TEXTURES, CL.recorder("MaxTextures", "xii", "i"))
        st.internal(SET_FOG_COLOR, CL.recorder("SetFogColor", "xu"))
        st.internal(SET_FOG_SPACE, CL.recorder("SetFogSpace", "xff"))
        st.internal(CLOCK, CL.recorder("Clock", "", "i"))
        st.imp("__assertion_failed", lambda emu: L.rec("assert", (T.s32(emu.r(5)),), (emu.cstr(emu.r(3)), emu.cstr(emu.r(4)))))
        st.imp("__nw__FUl", self.op_new)
        st.imp("__dl__FPv", self.op_delete)

        def get_error(e):
            gl("glGetError")
            e.set_r(3, T.u32(L.ret("glGetError", 0)))

        def gen_textures(e):
            gl("glGenTextures", (T.s32(e.r(3)),))
            w32(e, e.r(4), self.next_name)
            self.next_name += 1

        def stack_arg(e):                      # the ninth argument word of a call
            return e.u32(e.r(1) + 24 + 4 * 8)

        st.imp("glGetError", get_error)
        st.imp("glGenTextures", gen_textures)
        st.imp("glDeleteTextures", lambda e: gl("glDeleteTextures", (T.s32(e.r(3)), e.u32(e.r(4)))))
        st.imp("glBindTexture", lambda e: gl("glBindTexture", (e.r(3), e.r(4))))
        st.imp("glTexEnvf", lambda e: gl("glTexEnvf", (e.r(3), e.r(4), CL.fbits(e.f(1)))))
        st.imp("glTexParameteri", lambda e: gl("glTexParameteri", (e.r(3), e.r(4), T.s32(e.r(5)))))
        st.imp("glTexImage2D", lambda e: gl("glTexImage2D", tuple(T.s32(e.r(i)) for i in range(3, 11)) + (L.lab(stack_arg(e)),)))
        st.imp("glTexSubImage2D", lambda e: gl("glTexSubImage2D", tuple(T.s32(e.r(i)) for i in range(3, 11)) + (L.lab(stack_arg(e)),)))
        st.imp("gluBuild2DMipmaps", lambda e: gl("gluBuild2DMipmaps", tuple(T.s32(e.r(i)) for i in range(3, 9)) + (L.lab(e.r(9)),)))
        st.imp("glPixelStorei", lambda e: gl("glPixelStorei", (e.r(3), T.s32(e.r(4)))))
        for name in ("glDisable", "glEnable", "glShadeModel"):
            st.imp(name, (lambda n: lambda e: gl(n, (e.r(3),)))(name))
        st.imp("glHint", lambda e: gl("glHint", (e.r(3), e.r(4))))
        st.imp("glAlphaFunc", lambda e: gl("glAlphaFunc", (e.r(3), CL.fbits(e.f(1)))))
        st.imp("glBlendFunc", lambda e: gl("glBlendFunc", (e.r(3), e.r(4))))
        st.imp("glFlush", lambda e: gl("glFlush"))
        st.imp("glDepthMask", lambda e: gl("glDepthMask", (e.r(3) & 0xFF,)))
        st.imp("glClearColor", lambda e: gl("glClearColor", tuple(CL.fbits(e.f(i)) for i in range(1, 5))))

        def current_instance(e):
            gl("GetCurrentInstance")
            e.set_r(3, GL_INSTANCE)

        st.imp("GetCurrentInstance__Q213SamsUtilities7UOpenGLFv", current_instance)
        st.imp("Swap__Q213SamsUtilities7UOpenGLFv", lambda e: gl("Swap", (L.lab(e.r(3)),)))
        return st

    def reset(self):
        e = self.e
        # The renderer the cache asks for pages: an object whose CreateTextureBase slot is
        # the original's own function (it does not use its object).
        vtable = self.heap(0x20)
        w32(e, vtable + 0x18, e.u32(OPENGL_VTABLE + 32))
        renderer = self.heap(8)
        w32(e, renderer, vtable)
        self.opengl = self.heap(20)
        self.ddraw = self.heap(0x78c)
        e.write(self.ddraw, bytes(0x78c))
        for a, v in ((G_TCACHE, 0), (SYSTEM + 0x24, renderer), (SYSTEM + 0x1c, 3), (DISPLAY, 0), (SETTINGS, 0),
                     (SETTINGS + 4, 4), (SETTINGS + 0x10, 0), (SETTINGS + 0x14, 0), (G_DDRAW, self.ddraw),
                     (G_OPENGL, self.opengl), (UPLOAD_DATA, UPLOAD), (NVIDIA, 0)):
            w32(e, a, v)
        e.write(BLEND, struct.pack(">12I", *(0x7000 + i for i in range(12))))
        e.write(FREQUENCY, bytes(64))

    def call(self, name, *args):
        return T.call_ppc(self.e, FN[name], list(args))

    def r3(self, name, *args):
        return T.s32(self.call(name, *args)[0])

    def op(self, op, a):
        e, u, L = self.e, self.e.u32, self.L
        cache = u(G_TCACHE)
        if op in PAGE_OPS:
            self.page_alive = [False] * ENTRIES
        if op == 1:
            return self.r3("TCache_Init", a[0])
        if op == 2:
            return self.r3("TCache_Close")
        if op == 13:
            for addr, v in ((SETTINGS, a[0]), (SETTINGS + 4, a[1]), (SETTINGS + 0x10, a[2]), (SYSTEM + 0x1c, a[3]),
                            (DISPLAY, a[4]), (self.ddraw + 0x424, a[5]), (self.ddraw + 0x428, a[6]),
                            (self.ddraw + 0x4c0, a[7]), (UPLOAD_DATA, UPLOAD if a[8] else 0)):
                w32(e, addr, v)
            return 0
        if op == 14:
            self.call("InitFrequencyTable")
            return 0
        if op == 15:
            if not cache:
                return -9
            if a[1] >= T.s32(u(cache + 0x1800 + 4 * a[0])):
                return -8
            w32(e, u(u(cache + a[0] * 0x400 + a[1] * 4)), a[2])
            return 0
        if op == 16:
            if not cache:
                return -9
            entry = 0
            if a[0] > 0:
                entry = self.entries[a[0] - 1]
                if not entry:
                    return -8
                if (a[1] & 0x1000) and not self.page_alive[a[0] - 1]:
                    return -8
                w32(e, entry + 8, a[1])
            w32(e, cache + 0x18b4, a[2])
            return self.r3("Remove", cache, entry)
        if op == 7:
            if cache:
                return -9
            block = self.heap(0x1910)
            e.write(block, b"\xa5" * 0x1910)
            label = L.newlab(block)
            if not self.bulk:
                L.rec("new", (label,))
            self.call("Cache", block)
            w32(e, G_TCACHE, block)
            return 0
        if 3 <= op <= 12:
            if not cache:
                return -9
            if op == 3:
                self.call("Flush", cache, a[0])
                return 0
            if op == 4:
                if a[1] >= T.s32(u(cache + 0x1800 + 4 * a[0])):
                    return -8
                link = u(cache + a[0] * 0x400 + a[1] * 4)
                seed, made = a[3] & MINUS_ONE, 0
                for i in range(ENTRIES):
                    if made >= a[2]:
                        break
                    if self.entries[i]:
                        continue
                    entry, place = self.heap(0x5c), self.heap(0x18)
                    e.write(entry, bytes(0x5c))
                    e.write(place, b"\x5a" * 0x18)
                    seed = (seed * 1103515245 + 12345) & MINUS_ONE
                    flags = (seed >> 4) & 0x00FFFFFF
                    if a[4] & 1:
                        flags |= 0x200000
                    if a[4] & 2:
                        flags &= ~0x200000
                    e.write(entry, struct.pack(">IIIhhhh", place, u(link + 4), flags, 1 + ((seed >> 8) & 0xFF),
                                               1 + ((seed >> 16) & 0xFF), 0x1111, 0x2222))
                    e.write(entry + 0x2c, struct.pack(">f", ((seed >> 20) & 0x3FF) / 512.0))
                    w32(e, entry + 0x34, 0x33333333)
                    w32(e, entry + 0x14, link)
                    w32(e, link + 4, entry)
                    self.entries[i], self.places[i] = entry, place
                    self.page_alive[i] = True
                    made += 1
                return made
            if op == 5:
                self.call("SetUITextureSize", cache)
                return 0
            if op == 6:
                return self.r3("AutoDetectPageCreation", cache)
            if op == 8:
                count = self.heap(4)
                w32(e, count, a[1])
                result = self.r3("CreateSurfaces", cache, a[0], count, a[2], a[3])
                return result * 1000 + T.s32(u(count))
            if op == 9:
                return self.r3("AutoCreateSurfaces", cache)
            if op == 10:
                return self.r3("Init", cache, a[0])
            if op == 11:
                return self.r3("Close", cache)
            w32(e, cache + 0x1908, a[0])
            w32(e, cache + 0x18b4, a[1])
            for i in range(6):
                w32(e, cache + 0x187c + 4 * i, a[2 + i])
            w32(e, cache + 0x18cc, a[8])
            return 0
        if op == 20:
            if self.textures[a[0]]:
                return -9
            self.textures[a[0]] = self.call("CreateTextureBase", self.opengl)[0]
            return int(self.textures[a[0]] != 0)
        if op == 50:
            return self.r3("FlipScreen", self.opengl)
        if op == 51:
            return self.r3("GetVRAMSize", self.opengl)
        if op == 52:
            return self.r3("GetTextureMemory", self.opengl)
        if 21 <= op <= 34:
            base = self.textures[a[0]]
            if not base:
                return -9
            texture = base - 0x1c
            if op == 21:
                return self.r3("Create", texture, a[1], a[2], a[3], a[4])
            if op == 22:
                return self.r3("Destroy", texture)
            if op in (23, 24):
                self.call("Select" if op == 23 else "SetState", texture, a[1])
                return 0
            if op == 25:
                return self.r3("LockSurface", texture)
            if op == 26:
                return self.r3("UnlockSurface", texture)
            if op == 27:
                return speed_bits(self.call("TestUploadSpeed", texture)[2])
            if op == 28:
                return self.r3("MipmapsUsed", texture)
            if op == 29:
                return self.r3("GetPixelCount", texture)
            if op == 30:
                self.call("GetTexture", texture)
                return 0
            if op == 31:
                if self.draw and u(self.draw + 8) == texture:
                    w32(e, self.draw + 8, MINUS_ONE)
                self.call("TextureDtor", texture, 1)
                self.textures[a[0]] = 0
                return 0
            if op == 32:
                self.call("SetFlags", base, a[1])
                return 0
            if op == 33:
                name = ["GetFlags", "IsPositionLocked", "IsWrapping", "IsUsingMaxCompression", "PositionLock"]
                if a[1] < 4:
                    return self.call(name[a[1]], base)[0]
                self.call(name[4] if a[1] == 4 else "PositionUnlock", base)
                return 0
            w32(e, base, a[1])
            w32(e, texture + 0x14, a[2])
            w32(e, texture + 0x18, a[3])
            w32(e, texture + 0xc, a[4])
            return 0
        if op == 40:
            if self.draw:
                return -9
            draw = self.heap(0x4c)
            e.write(draw, b"\xa5" * 0x4c)
            e.write(draw, struct.pack(">IIIII", draw + 0x18, DRAW_VTABLE, MINUS_ONE, 0, 0))
            self.call("DrawBase", draw + 0x18)
            # The derived constructor's part: its vtable over the base's own, and the offset.
            w32(e, draw + 0x18 + 0x2c, DRAW_BASE_VTABLE)
            w32(e, draw + 0x18 + 0x30, 0)
            self.draw = draw
            return 0
        if 41 <= op <= 49:
            draw = self.draw
            if not draw:
                return -9
            if op == 41:
                return self.r3("DrawInit", draw)
            if op == 42:
                return self.r3("SetFogParameters", draw, T.F(T.bits_f32(a[0])), T.F(T.bits_f32(a[1])), a[2])
            if op == 43:
                return self.r3("SetBlendingMode", draw, a[0])
            if op == 44:
                texture = 0
                if a[1] >= 0:
                    if not self.textures[a[1]]:
                        return -9
                    texture = self.textures[a[1]] - 0x1c
                return self.r3("SetTexture", draw, a[0], texture)
            if op == 45:
                return self.r3("ExecuteShadow", draw, 0)
            if op == 46:
                return self.r3("BeginDraw", draw)
            if op == 47:
                return self.r3("EndDraw", draw)
            if op == 48:
                return self.r3("SupportsOnePassMultiTexture", draw)
            w32(e, draw + 0x18 + 0x10, a[0])
            return 0
        return -99

    def entry_index(self, entry):
        if entry == 0:
            return 0
        return self.entries.index(entry) + 1 if entry in self.entries else -1

    def texture_state(self, base):
        u, s = self.e.u32, lambda a: T.s32(self.e.u32(a))
        t = base - 0x1c
        return [u(base), s(base + 4), s(base + 8), s(base + 0xc), u(t + 8), s(t + 0xc), s(t + 0x10), s(t + 0x14), s(t + 0x18)]

    def read_cache(self):
        e, L, u = self.e, self.L, self.e.u32
        s = lambda a: T.s32(u(a))
        cache = u(G_TCACHE)
        out = [L.lab(cache)]
        if not cache:
            return out
        for size in range(6):
            count = s(cache + 0x1800 + 4 * size)
            out.append(count)
            for page in range(min(max(count, 0), 256)):
                link = u(cache + size * 0x400 + page * 4)
                out.append(L.lab(link))
                if link == 0 or L.lab(link) <= 0:
                    continue
                out += [s(link + 8), s(link + 0xc), s(link + 0x14)]
                texture = u(link)
                out.append(int(texture != 0))
                if texture:
                    out += self.texture_state(texture)
                entry, guard = u(link + 4), 0
                while entry and guard < 32:
                    out.append(self.entry_index(entry))
                    entry, guard = u(entry + 4), guard + 1
                out.append(-7)
        out += [s(cache + 0x1830 + 4 * i) for i in range(6)] + [s(cache + 0x1860)]
        out += [u(cache + 0x1864 + 4 * i) for i in range(6)]
        out += [s(cache + 0x187c + 4 * i) for i in range(6)] + [s(cache + 0x1894 + 4 * i) for i in range(6)]
        out += [s(cache + 0x18ac), s(cache + 0x18b0), u(cache + 0x18b4), s(cache + 0x18b8), s(cache + 0x18bc)]
        out += [s(cache + o) for o in (0x18cc, 0x18d0, 0x18d4, 0x18d8, 0x18dc, 0x18e0)]
        out += [u(cache + 0x18e8), s(cache + 0x1908)]
        return out

    def state(self, op):
        e, u = self.e, self.e.u32
        s = lambda a: T.s32(u(a))
        # Reading hundreds of pages field by field is slow: the cache is read again only
        # after an operation that can change it (or an entry on it, or a page's texture).
        if self.cache_state is None or op in CACHE_OPS:
            self.cache_state = self.read_cache()
        out = list(self.cache_state)
        for base in self.textures:
            out.append(int(base != 0))
            if base:
                out += self.texture_state(base)
        out.append(int(self.draw != 0))
        if self.draw:
            base = self.draw + 0x18
            out += [u(base + 4 * i) for i in range(11)]
            current = u(self.draw + 8)
            code = -2
            if current == 0:
                code = 0
            elif current == MINUS_ONE:
                code = -1
            else:
                for i, texture in enumerate(self.textures):
                    if texture and texture - 0x1c == current:
                        code = 10 + i
            out += [code, s(self.draw + 0xc), s(self.draw + 0x10)]
        out.append(s(NVIDIA))
        for entry, place in zip(self.entries, self.places):
            out.append(int(entry != 0))
            if not entry:
                continue
            flags, = struct.unpack(">I", e.read(entry + 8, 4))
            cache_width, cache_height = struct.unpack(">hh", e.read(entry + 0x10, 4))
            out += [flags, cache_width, cache_height, s(entry + 0x34), self.entry_index(u(entry + 4))]
            out += list(struct.unpack(">6I", e.read(place, 24)))
        out += [u(SETTINGS), s(SETTINGS + 4), s(SETTINGS + 0x14), u(DISPLAY)]
        out += [s(FREQUENCY + 4 * i) for i in range(16)]
        return out


def speed_bits(value):
    """A double as the signed 64-bit pattern the host returns, every NaN the same."""
    if value != value:
        return 0x7FF8000000000000
    return struct.unpack("<q", struct.pack("<d", value))[0]


def ppc(e, case):
    side = PpcSide(e, case)
    saved_words = [(a, e.u32(a)) for a in (G_TCACHE, SYSTEM + 0x24, SYSTEM + 0x1c, DISPLAY, SETTINGS, SETTINGS + 4,
                                           SETTINGS + 0x10, SETTINGS + 0x14, G_DDRAW, G_OPENGL, UPLOAD_DATA, NVIDIA)]
    saved_blend = e.read(BLEND, 48)
    saved = side.stubs().activate(e)
    try:
        side.reset()
        results, states, marks = [], [], []
        for op, args in case["script"]:
            results.append(side.op(op, args))
            states.append(side.state(op))
            marks.append(len(side.L.log))
    finally:
        CL.Stubs.deactivate(e, saved)
        for a, v in saved_words:
            w32(e, a, v)
        e.write(BLEND, bytes(saved_blend))
    return dict(results=results, log=side.L.log, marks=marks, states=states)


# ----------------------------------------------------------------- host side


def host(lib, case):
    reset = lib["tpw_tp_reset"]
    reset.restype, reset.argtypes = None, []
    bulk = lib["tpw_tp_bulk"]
    bulk.restype, bulk.argtypes = None, [C.c_int32]
    op_fn = lib["tpw_tp_op"]
    op_fn.restype, op_fn.argtypes = C.c_int64, [C.c_int32, C.POINTER(C.c_int32)]
    state_fn = lib["tpw_tp_state"]
    state_fn.restype, state_fn.argtypes = C.c_int32, [C.POINTER(C.c_int64), C.c_int32]
    upload, instance = lib["tpw_tp_upload_buffer"], lib["tpw_tp_gl_instance"]
    upload.restype = instance.restype = C.c_void_p

    reset()
    bulk(1 if is_bulk(case["script"]) else 0)
    CL.host_begin(lib, {5: upload(), 6: instance()}, case.get("q"))
    results, states, marks = [], [], []
    capacity = 32768
    buffer = (C.c_int64 * capacity)()
    try:
        for op, args in case["script"]:
            padded = (C.c_int32 * 16)(*[T.s32(v) for v in args])
            result = op_fn(op, padded)
            if op == 27 and result not in (-9,):
                value = struct.unpack("<d", struct.pack("<q", result))[0]
                result = speed_bits(value)
            results.append(result)
            count = state_fn(buffer, capacity)
            states.append(list(buffer[:min(count, capacity)]) + ([("OVERFLOW", count)] if count > capacity else []))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
        bulk(0)
    return dict(results=results, log=log, marks=marks, states=states)


# ----------------------------------------------------------------- scripts

SIZES = [256, 128, 64, 32, 16, 8]


def fb(x):
    return T.f32_bits(x)


def settings_op(r, wild=False):
    return (13, [r.choice([0, 0, 2, 8, 10, 2, r.getrandbits(4)]), r.choice([4, 4, 4, 5, 0, 3]),
                 r.choice([0, 0, 0, 0x100000, 0x800000, 0x2000000, 0x7FFFFFFF]) if wild else 0,
                 r.choice([3, 3, 3, 1, 4, 0]), r.choice([0, 0, 0x80000000, r.getrandbits(32)]),
                 r.choice([0, 0x1002, 0x10de, 0x121a]), r.choice([0, 0x4742, 0x4750]),
                 r.choice([0, 0x800000, 0x4000000]), r.choice([1, 1, 1, 0])])


def texture_op(r):
    kind = r.choice([20, 20, 21, 21, 21, 22, 23, 23, 24, 24, 24, 25, 26, 26, 27, 28, 29, 30, 31, 32, 33, 33, 34, 34])
    k = r.randrange(TEXTURES)
    state = r.choice([0, 0x20000, 0x40000, 0x60000, 0x400000, 0x420000, 0x440000, 0x460000, r.getrandbits(24)])
    if kind == 21:
        size = r.choice(SIZES + [1, 0, 300])
        return (21, [k, size, r.choice([size, size, 64]), r.choice([4, 5, 0]), r.choice([0, 0, 1, 4, -1])])
    if kind in (23, 24):
        return (kind, [k, state])
    if kind == 32:
        return (32, [k, r.choice([0, 1, 2, 4, 7, 0x10000000, r.getrandbits(32)])])
    if kind == 33:
        return (33, [k, r.randrange(6)])
    if kind == 34:
        return (34, [k, r.choice([0, 0x10000000, 0x10000007, r.getrandbits(32)]), state,
                     r.choice([0x2600, 0x2601, 0x2701, 0x2703]), r.choice([0, 0, 1, 4])])
    return (kind, [k])


def draw_op(r):
    kind = r.choice([40, 40, 41, 41, 42, 43, 43, 43, 43, 44, 44, 44, 45, 46, 47, 48, 49])
    if kind == 42:
        return (42, [fb(r.choice([0.0, 10.0, 100.0, 130.0])), fb(r.choice([0.0, 50.0, 130.0, 400.0])),
                     r.choice([0, 0xFFFFFFFF, 0x40008000, r.getrandbits(32)])])
    if kind == 43:
        bits = r.choice([0, 0, 0x100, 0x152, 0x122, 0x400, 0x600, 0x8000, 0x800, 0x1000000, 0x4000000, 0x460000])
        if r.random() < 0.5:
            bits = r.getrandbits(27)
        return (43, [bits | r.choice([0, 0x52, 0x22, 0x65, r.getrandbits(8)])])
    if kind == 44:
        return (44, [r.randrange(2), r.choice([-1, 0, 1, 2, 3])])
    if kind == 49:
        return (49, [r.choice([0xFFFFFFFF, 0, 0x100, r.getrandbits(32)])])
    return (kind, [])


def small_cache_op(r):
    """A cache built by its constructor and filled a few pages at a time."""
    kind = r.choice([7, 7, 8, 8, 8, 8, 3, 3, 3, 4, 4, 4, 5, 6, 11, 2, 12, 14, 15, 15])
    if kind == 15:
        return (15, [r.randrange(6), r.randrange(3), r.choice([1, 2, 7, 0x10000000, 0x10000005, r.getrandbits(32)])])
    if kind == 8:
        return (8, [r.choice(SIZES), r.choice([0, 1, 2, 3, 5]), r.choice([4, 5]), r.choice([0, 4])])
    if kind == 3:
        return (3, [r.randrange(4)])
    if kind == 4:
        return (4, [r.randrange(6), r.randrange(3), r.randrange(1, 5), r.getrandbits(31), r.choice([0, 0, 1, 2])])
    if kind == 12:
        return (12, [r.choice([0, 0, 1, 2, 3]), r.choice([0, 2, 0x20000000, r.getrandbits(32)])]
                + [0] * 6 + [r.choice([7, 0, 3, 15])])
    return (kind, [])


def remove_op(r):
    """CTextureCache::Remove of one planted entry (0: no entry), mostly marked as on a page."""
    flags = r.getrandbits(24) | 0x1000 if r.random() < 0.8 else r.getrandbits(24) & ~0x1000
    return (16, [r.randrange(0, 9), flags, r.choice([0x20000000, 0x20000000, 0x20000000, 0x20000001, 0, r.getrandbits(32)])])


def queues(r, fail, script):
    # A script that creates a whole cache keeps to a card of at most 60 pages: about 900
    # pages of all sizes, whose deletions still fit the call log.
    reported = [0, 1, 20, 32, 33, 40, 48, 60] if is_bulk(script) else [0, 1, 20, 32, 33, 40, 64, 65, 70, 128, 129, 140, 256, 300, 1000]
    q = {"MaxTextures": [r.choice(reported)], "Clock": [r.getrandbits(31) for _ in range(6)]}
    if fail:
        q["glGetError"] = [0 if r.random() < 0.85 else 0x502 for _ in range(r.randrange(1, 30))] + [0]
        if r.random() < 0.4:
            q["New"] = [1 if r.random() < 0.85 else 0 for _ in range(r.randrange(1, 12))] + [1]
    return q


def tail():
    return [(2, []), (31, [0]), (31, [1]), (31, [2]), (31, [3])]


def case_of(r, makers, length, fail=False, lead=()):
    script = list(lead)
    creations = 0
    for _ in range(length):
        op = r.choice(makers)(r)
        if op[0] in (1, 9, 10):
            creations += 1
            if creations > 2:
                continue
        script.append(op)
    script += tail()
    return dict(q=queues(r, fail, script), script=script)


def edge_textures():
    r = random.Random(81)
    yield dict(script=[(20, [0]), (21, [0, 256, 256, 4, 0]), (25, [0]), (26, [0]), (28, [0]), (29, [0]), (30, [0]),
                       (23, [0, 0x400000]), (23, [0, 0x460000]), (24, [0, 0]), (32, [0, 5]), (33, [0, 0]),
                       (33, [0, 1]), (33, [0, 2]), (33, [0, 3]), (33, [0, 4]), (33, [0, 5]), (22, [0]), (22, [0]),
                       (26, [0]), (31, [0]), (31, [0])])
    # Mipmapped pages: only when the settings allow them, with either filter.
    for flags in (0, 2, 8, 10):
        for mipmaps in (0, 1, 4):
            yield dict(script=[(13, [flags, 4, 0, 3, 0, 0, 0, 0, 1]), (20, [1]), (21, [1, 64, 64, 4, mipmaps]),
                               (26, [1]), (31, [1])])
    # Each OpenGL error in Create and what it leaves behind.
    for at in range(3):
        yield dict(q={"glGetError": [0] * at + [0x502, 0]}, script=[(20, [0]), (21, [0, 128, 128, 4, 0]), (31, [0])])
    yield dict(q={"glGetError": [0, 0, 0x502, 0]}, script=[(20, [0]), (21, [0, 32, 32, 4, 0]), (23, [0, 0]), (31, [0])])
    yield dict(script=[(13, [0, 4, 0, 3, 0, 0, 0, 0, 0]), (20, [0]), (21, [0, 16, 16, 4, 0]), (25, [0]), (26, [0])])
    yield dict(q={"New": [0, 1]}, script=[(20, [0]), (20, [0]), (31, [0])])
    yield dict(script=[(20, [0]), (21, [0, 128, 64, 4, 0]), (29, [0]), (20, [1]), (21, [1, 16, 256, 4, 0]), (29, [1])])
    for clocks in ([0, 0], [100, 4196], [5, 5 + 0x10000000], [7, 3]):
        yield dict(q={"Clock": clocks}, script=[(20, [2]), (21, [2, 256, 256, 4, 0]), (27, [2]), (31, [2])])
    yield dict(script=[(13, [0, 4, 0, 3, 0, 0, 0, 0x1234567, 1]), (50, []), (51, []), (52, [])])
    for state_from in (0, 0x20000, 0x40000, 0x60000, 0x400000, 0x460000):
        for state_to in (0, 0x20000, 0x40000, 0x60000, 0x400000, 0x460000):
            yield dict(script=[(20, [0]), (34, [0, 0, state_from, 0x2703, 1]), (24, [0, state_to])])
    for _ in range(40):
        yield case_of(r, [texture_op, texture_op, lambda r: settings_op(r)], 18)
    for _ in range(10):
        yield case_of(r, [texture_op], 18, fail=True)


def edge_draw():
    r = random.Random(82)
    yield dict(script=[(40, []), (41, []), (48, []), (45, []), (46, []), (47, []), (42, [fb(100.0), fb(130.0), 0x40008000])])
    for vendor, device in ((0x1002, 0x4742), (0x1002, 0x4750), (0x10de, 0x4742)):
        for renderer in (1, 3):
            yield dict(script=[(13, [0, 4, 0, renderer, 0, vendor, device, 0, 1]), (40, []), (41, [])])
    yield dict(q={"glGetError": [0x502, 0]}, script=[(40, []), (41, []), (46, []), (47, [])])
    # Every blending bit switched on from nothing, off from everything, and unchanged.
    for bit in (0x400, 0x200, 0x8000, 0x800, 0x1000000, 0x4000000, 0x100, 0x20000, 0x40000, 0x400000, 0x52, 0x25):
        yield dict(script=[(40, []), (49, [0]), (43, [bit]), (43, [bit]), (43, [0])])
        yield dict(script=[(40, []), (49, [0xFFFFFFFF]), (43, [0xFFFFFFFF ^ bit]), (43, [0xFFFFFFFF])])
    for mode in (0, 0x52, 0x152, 0x22, 0x65, 0x100):
        for current in (0, 0x152, 0x122, 0xFFFFFFFF):
            yield dict(script=[(13, [0, 4, 0, 3, 0x80000000, 0, 0, 0, 1]), (40, []), (49, [current]), (43, [mode])])
    yield dict(script=[(40, []), (20, [0]), (21, [0, 64, 64, 4, 0]), (20, [1]), (21, [1, 32, 32, 4, 0]), (41, []),
                       (44, [0, 0]), (44, [0, 0]), (44, [0, 1]), (44, [0, -1]), (44, [0, -1]), (44, [1, 1]), (46, []),
                       (44, [0, 0])])
    yield dict(q={"glGetError": [0, 0, 0, 0, 0, 0x502, 0x502, 0]},
               script=[(40, []), (20, [0]), (21, [0, 64, 64, 4, 0]), (46, []), (44, [0, 0]), (44, [0, -1])])
    for _ in range(40):
        yield case_of(r, [draw_op, draw_op, texture_op, lambda r: settings_op(r)], 20)
    for _ in range(10):
        yield case_of(r, [draw_op, texture_op], 20, fail=True)


def edge_small_cache():
    r = random.Random(83)
    yield dict(script=[(7, []), (14, []), (5, []), (8, [256, 2, 4, 0]), (8, [128, 3, 4, 0]), (8, [64, 1, 4, 0]), (5, []),
                       (4, [0, 0, 3, 77, 0]), (4, [1, 2, 2, 78, 1]), (3, [0]), (4, [0, 1, 4, 79, 0]), (3, [1]),
                       (4, [1, 0, 4, 80, 0]), (3, [2]), (3, [3]), (11, []), (2, [])])
    for size in SIZES:
        yield dict(script=[(7, []), (8, [size, 2, 4, 4]), (5, []), (2, [])])
    # CreateSurfaces: nothing asked, the renderer out of objects, a page that cannot be made.
    yield dict(script=[(7, []), (8, [64, 0, 4, 0]), (8, [64, -1, 4, 0]), (2, [])])
    yield dict(q={"New": [1, 1, 1, 0, 1]}, script=[(7, []), (8, [64, 4, 4, 0]), (2, [])])
    for at in range(4):
        yield dict(q={"glGetError": [0] * at + [0x502, 0]}, script=[(7, []), (8, [32, 3, 4, 0]), (2, [])])
    for mode in range(4):
        for which in (0, 1, 2):
            yield dict(script=[(7, []), (12, [0, 0, 0, 0, 0, 0, 0, 0, 7]), (8, [16, 2, 4, 0]),
                               (4, [4, 0, 4, 1234 + mode, which]), (4, [4, 1, 3, 99, which]), (3, [mode]), (2, [])])
    for mode in range(4):
        yield dict(script=[(7, []), (8, [32, 2, 4, 0]), (15, [3, 0, 0x10000007]), (15, [3, 1, 5]), (4, [3, 0, 2, 31, 0]),
                           (3, [mode]), (2, [])])
    for mask in (0, 3, 7, 15):
        yield dict(script=[(7, []), (12, [0, 0, 0, 0, 0, 0, 0, 0, mask]), (8, [8, 1, 4, 0]), (4, [5, 0, 5, 4321, 0]),
                           (3, [1]), (2, [])])
    yield dict(script=[(2, []), (3, [0]), (4, [0, 0, 1, 1, 0]), (5, []), (6, []), (8, [8, 1, 4, 0]), (9, []), (11, [])])
    for _ in range(50):
        yield case_of(r, [small_cache_op, small_cache_op, lambda r: settings_op(r)], 16, lead=[(7, [])])
    for _ in range(15):
        yield case_of(r, [small_cache_op], 16, fail=True, lead=[(7, [])])


def edge_remove():
    r = random.Random(85)
    ready = 0x20000000
    lead = [(7, []), (8, [8, 2, 4, 0]), (4, [5, 0, 5, 4321, 0]), (4, [5, 1, 2, 99, 0])]
    # Five entries on one page (the list runs 5, 4, 3, 2, 1) and two on another: the head, the
    # tail and the middle, in each order; then the same entry again (its page no longer has
    # it), no entry at all, and a cache that is not up.
    for order in ((5, 1, 3), (1, 3, 5), (3, 5, 1), (5, 4, 3, 2, 1), (1, 2, 3, 4, 5), (6, 7), (7, 6, 5)):
        yield dict(script=lead + [(16, [n, 0x1000, ready]) for n in order] + [(3, [0]), (2, [])])
    yield dict(script=lead + [(16, [3, 0x1000, ready]), (16, [3, 0x1000, ready]), (16, [3, 0, ready]), (16, [0, 0, ready]),
                              (16, [2, 0x1000, 0]), (16, [0, 0, 0]), (16, [8, 0x1000, ready]), (2, [])])
    for flags in (0x1000, 0x10c0, 0x1040, 0x1080, 0x10f0, 0x5000, 0x81000, 0x85000, 0xffffff, 0xf030, 0x7000, 0xffefff):
        yield dict(script=lead + [(16, [2, flags, ready]), (16, [5, flags, ready]), (2, [])])
    # After a flush the entries are off their pages but still say where they were.
    for mode in range(4):
        yield dict(script=lead + [(3, [mode]), (16, [1, 0x1000, ready]), (16, [5, 0x1000, ready]), (16, [4, 0x2000, ready]), (2, [])])
    yield dict(script=[(16, [0, 0, ready]), (7, []), (16, [0, 0, ready]), (16, [1, 0x1000, ready]), (2, [])])
    for _ in range(40):
        yield case_of(r, [remove_op, remove_op, small_cache_op], 16, lead=lead)
    for _ in range(10):
        yield case_of(r, [remove_op, small_cache_op], 16, fail=True, lead=lead)


def edge_whole_cache():
    r = random.Random(84)
    yield dict(q={"MaxTextures": [64]}, script=[(1, [4]), (1, [4]), (4, [0, 0, 3, 5, 0]), (4, [3, 7, 4, 6, 1]), (3, [1]),
                                                (2, []), (2, [])])
    # What the card reports against the detection's steps, and the texture quality rows.
    # 49 and 50 leave 47 and 48 pages, 83 and 84 leave 79 and 80, 115 and 116 leave 111 and
    # 112: either side of each quality row.
    for pages in (0, 31, 32, 33, 47, 48, 49, 50, 51, 64, 65, 79, 80, 83, 84, 85, 111, 112, 115, 116, 117, 128,
                  129, 136, 256, 257, 400):
        yield dict(q={"MaxTextures": [pages]}, script=[(7, []), (6, []), (2, [])])
    for limit in (0x20000, 0x400000, 0x1000000, 0x4000000):
        for fmt in (3, 4):
            yield dict(q={"MaxTextures": [100]}, script=[(13, [0, fmt, limit, 0, 0, 0, 0, 0, 1]), (7, []), (6, []), (2, [])])
    for quality in (1, 2, 3):
        yield dict(q={"MaxTextures": [90]}, script=[(7, []), (12, [quality, 2, 0, 0, 0, 0, 0, 0, 7]), (6, []), (2, [])])
    for flags in (0, 2, 8):
        for renderer in (3, 1):
            yield dict(q={"MaxTextures": [40]}, script=[(13, [flags, 5, 0, renderer, 0, 0, 0, 0, 1]), (1, [4]), (2, [])])
    # A size that wanted pages and got none: everything made so far is destroyed.
    for bad in (0, 1, 2, 20, 200):
        yield dict(q={"MaxTextures": [32], "glGetError": [0] * bad + [0x502] * 3 + [0]}, script=[(1, [4]), (2, [])])
    yield dict(script=[(7, []), (12, [0, 0, 2, 0, 3, 0, 0, 1, 7]), (9, []), (5, []), (2, [])])
    yield dict(script=[(7, []), (12, [0, 0, 0, 0, 0, 0, 0, 0, 7]), (9, []), (2, [])])
    yield dict(q={"New": [1, 0, 1]}, script=[(7, []), (12, [0, 0, 3, 2, 0, 0, 0, 0, 7]), (9, []), (2, [])])
    # The cache's own allocation failing in TCache_Init.
    yield dict(q={"New": [0, 1], "MaxTextures": [32]}, script=[(1, [4]), (2, []), (1, [4]), (2, [])])
    for _ in range(12):
        yield case_of(r, [small_cache_op, lambda r: (r.choice([1, 10, 9, 6]), [4]), lambda r: settings_op(r, True)], 8)


TESTS = [
    FnTest("CTextureBase and COpenGLTexture", ppc, host, edge=edge_textures,
           rnd=lambda r: case_of(r, [texture_op, texture_op, lambda r: settings_op(r)], 18, fail=r.random() < 0.3)),
    FnTest("COpenGLDrawPrimitive state, CDrawPrimitive constructor", ppc, host, edge=edge_draw,
           rnd=lambda r: case_of(r, [draw_op, draw_op, texture_op, lambda r: settings_op(r)], 20,
                                 fail=r.random() < 0.3)),
    FnTest("CTextureCache pages and Flush", ppc, host, edge=edge_small_cache,
           rnd=lambda r: case_of(r, [small_cache_op, small_cache_op, lambda r: settings_op(r)], 16,
                                 fail=r.random() < 0.3, lead=[(7, [])])),
    FnTest("CTextureCache::Remove", ppc, host, edge=edge_remove,
           rnd=lambda r: case_of(r, [remove_op, remove_op, small_cache_op], 16, fail=r.random() < 0.3,
                                 lead=[(7, []), (8, [8, 2, 4, 0]), (4, [5, 0, 4, r.getrandbits(31), 0])])),
    FnTest("TCache_Init and page detection", ppc, host, edge=edge_whole_cache,
           rnd=lambda r: case_of(r, [small_cache_op, lambda r: (r.choice([1, 10, 9, 6]), [4]),
                                     lambda r: settings_op(r, True)], 6, fail=r.random() < 0.3)),
]
