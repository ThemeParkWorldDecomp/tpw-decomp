"""The texture entry (CTextureEntry, original tb_textureentry.cpp) and the wavelet texture
file loader (CWaveLoad and CCompBase, original tb_decode_wavelet.cpp), engine library.

A case is a script of operations run on both sides on an array of two entries and one
wave loader. After every operation the test compares the result, the fakes' events (the
engine allocator, the file callbacks, the texture cache's Remove, the wavelet
decompressor's two entry points with all their arguments, the object a compressed entry
unpacks from) and the state of the three objects and the two image buffers, read in the
same order as tests/unit_TextureEntry.cpp writes the host's.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"
SHIM_LIBRARY = "unit_TextureEntry"

ENTRY = dict(ctor=0x10040d30, Create=0x10040e84, Close=0x10040f7c, GetBlendingMode=0x100410a4, BestSize=0x1004117c,
             GetBestSize=0x10041204, GetData=0x10041274, GetCorrectedData=0x10041334, SetSize=0x10041560,
             FlushEntryFlags=0x1004168c, Lock=0x10041738, Unlock=0x100417c0, StoreAsWrapping=0x10041848,
             GetVSize=0x10025d60, GetUSize=0x10025d98, GetVOffset=0x10025dd0, GetUOffset=0x10025e08,
             GetTexture=0x10025e40)
WAVE = dict(ctor=0x10005de4, dtor=0x10005ea8, LoadTexture=0x10005f68, Close=0x1000640c, Decompress=0x100064b8,
            AverageColor=0x1000660c, Blit=0x10006644, IsNaturallyFlipped=0x100067b8)
INNER = [0x10040dfc, 0x100051d0, 0x10005bf8]
ERROR_PUSH, CACHE_REMOVE = 0x10008904, 0x10034610
P_MALLOC, P_FREE, FILE_OPEN, FILE_READ, FILE_CLOSE = 0x10051f70, 0x10051f74, 0x101735e4, 0x101735d8, 0x101735e0
PROC_BUFFER, WAVELOAD_WAVELET, SYSTEM, SETTINGS, G_TCACHE = 0x10052b00, 0x10052b04, 0x10165f80, 0x10165fc8, 0x10173184
BYTES_PER_PIXEL = [2, 2, 2, 4, 2, 4]
PIXELS = 0x400
K_COMPRESSED, K_NEXT_IS_PLANE = 0x10000000, 0x00800000
MINUS = 0xFFFFFFFF


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if not hasattr(e, "te_fns"):
        side = lambda emu: emu.te_side
        e.te_fns = {name: T.fake_function(e, (lambda n: lambda emu: getattr(side(emu), n)())(name)) for name in (
            "cb_malloc", "cb_free", "cb_open", "cb_read", "cb_close", "wavelet_decompress", "wavelet_blit",
            "source_dtor", "source_decompress", "source_flipped")}
        # The vtables of the two fake objects: a CWavelet (slots 0xc and 0x10 are the two
        # Decompress entry points the loader calls) and a CCompBase (destructor, Decompress,
        # IsNaturallyFlipped).
        e.te_wavelet_vtable = T.persist_alloc(e, 0x30)
        e.write(e.te_wavelet_vtable + 0xc, struct.pack(">II", e.te_fns["wavelet_decompress"], e.te_fns["wavelet_blit"]))
        e.te_source_vtable = T.persist_alloc(e, 0x30)
        e.write(e.te_source_vtable + 8, struct.pack(">I", e.te_fns["source_dtor"]))
        e.write(e.te_source_vtable + 0x10, struct.pack(">I", e.te_fns["source_decompress"]))
        e.write(e.te_source_vtable + 0x18, struct.pack(">I", e.te_fns["source_flipped"]))


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def image(seed, a, b):
    return bytes((seed * a + i * b) & 0xFF for i in range(PIXELS))


def image_sum(data):
    total = 0
    for byte in data:
        total = (total * 31 + byte) & 0xFFFFFFFFFFFF
    return total


class PpcSide:
    def __init__(self, e, case):
        self.e = e
        e.te_side = self
        self.file, self.position = bytes(case.get("file", b"")), 0
        self.entries = e.alloc(0x5c * 2, b"\xa5" * (0x5c * 2))
        self.wave = e.alloc(0x34, b"\xa5" * 0x34)
        self.cache, self.page, self.texture, self.next = (e.alloc(16) for _ in range(4))
        self.handle = e.alloc(8)
        self.scratch = e.alloc(PIXELS, image(case["seed"], 3, 5))
        self.pixels = e.alloc(PIXELS, image(case["seed"], 1, 7))
        self.wavelet = e.alloc(8, struct.pack(">I", e.te_wavelet_vtable))
        self.kind = [0, 0]
        self.built = False
        fixed = {self.entries: 1, self.entries + 0x5c: 2, self.cache: 3, self.scratch: 4, self.pixels: 5,
                 self.page: 6, self.texture: 7, self.next: 8, self.handle: 9, self.wavelet: 10}
        self.L = CL.PpcLog(fixed, case.get("q"))
        e.cl = self.L

    # ---- callbacks of the engine
    def malloc(self, size):
        L = self.L
        L.rec("Malloc", (size,))
        if not L.ret("Malloc", 1) or size > 0x2000:        # as the unit file's allocator
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
        self.position = 0
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

    def wavelet_decompress(self):
        e, L = self.e, self.L
        L.rec("Wavelet.Decompress", tuple(T.s32(e.r(i)) for i in range(4, 8)) + (L.lab(e.r(8)), L.lab(e.r(9)), L.lab(e.r(10)))
              + tuple(CL.fbits(e.f(i)) for i in range(1, 5)))
        e.set_r(3, 1)

    def wavelet_blit(self):
        e, L = self.e, self.L
        stack = lambda slot: e.u32(e.r(1) + 24 + 4 * slot)
        L.rec("Wavelet.Blit", tuple(T.s32(e.r(i)) for i in range(4, 11)) + (L.lab(stack(8)), L.lab(stack(9)))
              + tuple(CL.fbits(e.f(i)) for i in range(1, 4)))
        L.rec("Wavelet.Blit2", (CL.fbits(e.f(4)), T.s32(stack(10) - self.scratch)))
        e.set_r(3, 1)

    def source_dtor(self):
        e, L = self.e, self.L
        this = e.r(3)
        L.rec("Source.dtor", (L.lab(this),))
        if T.s16(e.r(4)) > 0:
            L.rec("delete", (L.lab(this),))
            L.forget(this)

    def source_decompress(self):
        e, L = self.e, self.L
        dest = e.r(4)
        L.rec("Source.Decompress", (L.lab(e.r(3)), L.lab(e.u32(dest)), T.s32(e.u32(dest + 4)), T.s32(e.u32(dest + 8)),
                                    T.s32(e.u32(dest + 12))))
        e.set_r(3, 1)

    def source_flipped(self):
        self.L.rec("Source.Flipped", (self.L.lab(self.e.r(3)),))
        self.e.set_r(3, T.u32(self.L.ret("Flipped", 0)))

    def stubs(self):
        st = CL.Stubs()
        st.real = set(ENTRY.values()) | set(WAVE.values()) | set(INNER)
        st.internal(ERROR_PUSH, CL.recorder("Error", "xii"))
        st.internal(CACHE_REMOVE, CL.recorder("Cache.Remove", "LL", ret=lambda e, L: 1))
        st.imp("__assertion_failed", lambda e: self.L.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
        return st

    def reset(self):
        e = self.e
        for a, v in ((P_MALLOC, e.te_fns["cb_malloc"]), (P_FREE, e.te_fns["cb_free"]), (FILE_OPEN, e.te_fns["cb_open"]),
                     (FILE_READ, e.te_fns["cb_read"]), (FILE_CLOSE, e.te_fns["cb_close"]), (PROC_BUFFER, self.scratch),
                     (WAVELOAD_WAVELET, self.wavelet), (SYSTEM, 0), (SYSTEM + 0x1c, 3), (SETTINGS + 4, 4),
                     (G_TCACHE, self.cache)):
            w32(e, a, v)

    # ---- the test's own bookkeeping, as in the unit file
    def reconcile(self, index):
        e, entry = self.e, self.entries + 0x5c * index
        flags = e.u32(entry + 8)
        flags = flags | K_COMPRESSED if self.kind[index] == 3 else flags & ~K_COMPRESSED
        if index == 1:
            flags &= ~K_NEXT_IS_PLANE
        w32(e, entry + 8, flags)

    def size_fits(self, entry):
        width, height = struct.unpack(">hh", self.e.read(entry + 0xc, 4))
        return 1 <= width <= 16 and 1 <= height <= 16 and height <= 2 * width

    def call(self, addr, *args):
        return T.call_ppc(self.e, addr, list(args))

    def op(self, op, a):
        e, L, u = self.e, self.L, self.e.u32
        index = a[0] & 1 if a else 0
        entry = self.entries + 0x5c * index
        r3 = lambda name, *args: T.s32(self.call(ENTRY[name], entry, *args)[0])
        if op == 1:
            self.call(ENTRY["ctor"], entry, 1)
            self.kind[index] = 0
            return 0
        if op == 2:
            result = r3("Create", a[1], a[2])
            self.kind[index] = 2 if result else 0
            self.reconcile(index)
            return result
        if op == 3:
            both = index == 0 and (u(entry + 8) & K_NEXT_IS_PLANE) != 0
            result = r3("Close")
            self.kind[index] = 0
            if both:
                self.kind[1] = 0
            return result
        if op == 4:
            return self.call(ENTRY["GetBlendingMode"], entry)[0]
        if op == 5:
            return r3("BestSize", a[1])
        if op == 6:
            return r3("GetBestSize")
        if op == 7:
            if self.kind[index] == 3 and not self.size_fits(entry):
                return -8
            return L.lab(self.call(ENTRY["GetData"], entry)[0])
        if op == 8:
            if self.kind[index] not in (1, 3) or not self.size_fits(entry):
                return -8
            return L.lab(self.call(ENTRY["GetCorrectedData"], entry)[0])
        if op == 9:
            self.call(ENTRY["SetSize"], entry, a[1], a[2])
            return 0
        if op in (10, 11, 12):
            self.call(ENTRY[{10: "FlushEntryFlags", 11: "Lock", 12: "Unlock"}[op]], entry)
            return 0
        if op == 13:
            return r3("StoreAsWrapping")
        if op == 14:
            if a[1] < 4:
                name = ["GetVSize", "GetUSize", "GetVOffset", "GetUOffset"][a[1]]
                return CL.fbits(self.call(ENTRY[name], entry)[2])
            return L.lab(self.call(ENTRY["GetTexture"], entry)[0])
        if op == 15:
            w32(e, entry + 8, a[1])
            e.write(entry + 0xc, struct.pack(">hhh", T.s16(a[2]), T.s16(a[3]), T.s16(a[4])))
            w32(e, entry + 0x2c, a[5])
            for i in range(4):
                w32(e, entry + 0x1c + 4 * i, a[6 + i])
            self.reconcile(index)
            return 0
        if op == 16:
            data = self.pixels if a[1] == 1 else self.malloc(64) if a[1] == 2 else 0
            if a[1] == 3:
                data = e.alloc(8, struct.pack(">I", e.te_source_vtable))
                L.newlab(data)
            w32(e, entry + 0x3c, data)
            w32(e, entry + 0x38, self.malloc(32) if a[2] else 0)
            w32(e, entry + 0x14, self.page if a[3] else 0)
            w32(e, entry + 0x18, self.texture if a[3] else 0)
            w32(e, entry + 4, self.next if a[4] else 0)
            self.kind[index] = a[1]
            self.reconcile(index)
            return 0
        if op == 17:
            w32(e, SYSTEM, a[1])
            w32(e, SYSTEM + 0x1c, a[2])
            w32(e, SETTINGS + 4, a[3])
            return 0
        wave = self.wave
        if op == 20:
            self.call(WAVE["ctor"], wave, 1)
            self.built = True
            return 0
        if op == 21:
            name = e.alloc(32, b"data:image.WCT\0")
            return T.s32(self.call(WAVE["LoadTexture"], wave, name)[0])
        if op == 22:
            return T.s32(self.call(WAVE["Close"], wave)[0])
        if op in (23, 24):
            dest = e.alloc(16, struct.pack(">Iiii", self.scratch if a[1] else 0, a[2], a[3], a[4]))
            if op == 23:
                result = T.s32(self.call(WAVE["Decompress"], wave, dest)[0])
            else:
                result = T.s32(self.call(WAVE["Blit"], wave, dest, a[5])[0])
            data = u(dest)
            return result * 0x1000000 + (u(dest + 12) & 0xFFFF) * 0x100 + (0xFF if data == 0 else (data - self.scratch) & 0xFF)
        if op == 25:
            return self.call(WAVE["AverageColor"], wave)[0]
        if op == 26:
            return T.s32(self.call(WAVE["IsNaturallyFlipped"], wave)[0])
        if op == 27:
            self.call(WAVE["dtor"], wave, 0)
            return 0
        if op == 28:
            e.write(wave + 4, bytes([a[1] & 0xFF]))
            e.write(wave + 8, struct.pack(">HHHHHH", *(v & 0xFFFF for v in a[2:8])))
            w32(e, wave + 0x1c, a[8])
            return 0
        return -99

    def state(self):
        e, L, u = self.e, self.L, self.e.u32
        s = lambda a: T.s32(u(a))
        out = []
        for index in range(2):
            entry = self.entries + 0x5c * index
            width, height, cache_width, cache_height = struct.unpack(">hhhh", e.read(entry + 0xc, 8))
            out += [int(u(entry) == entry + 0x44), L.lab(u(entry + 4)), u(entry + 8), width, height, cache_width,
                    cache_height, L.lab(u(entry + 0x14)), L.lab(u(entry + 0x18))]
            out += [u(entry + 0x1c + 4 * i) for i in range(5)]
            out += [struct.unpack(">h", e.read(entry + 0x30, 2))[0], s(entry + 0x34), L.lab(u(entry + 0x38)),
                    L.lab(u(entry + 0x3c))]
            out += list(struct.unpack(">6I", e.read(entry + 0x44, 24)))
        wave = self.wave
        out += list(e.read(wave + 4, 4)) + list(struct.unpack(">6H", e.read(wave + 8, 12)))
        out += [u(wave + 0x14), u(wave + 0x18), u(wave + 0x1c)]
        alpha, data = u(wave + 0x20), u(wave + 0x24)
        out += [L.lab(alpha) if self.built else -7, L.lab(data) if self.built else -7]
        for block in (data, alpha):
            word = 0
            if self.built and block and L.lab(block) > 0:
                word = struct.unpack("<I", e.read(block, 4))[0]        # as the host reads the same bytes
            out.append(word)
        out += [image_sum(e.read(self.pixels, PIXELS)), image_sum(e.read(self.scratch, PIXELS))]
        return out


def ppc(e, case):
    side = PpcSide(e, case)
    words = (P_MALLOC, P_FREE, FILE_OPEN, FILE_READ, FILE_CLOSE, PROC_BUFFER, WAVELOAD_WAVELET, SYSTEM, SYSTEM + 0x1c,
             SETTINGS + 4, G_TCACHE)
    saved_words = [(a, e.u32(a)) for a in words]
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
    return dict(results=results, log=side.L.log, marks=marks, states=states)


def host(lib, case):
    reset = lib["tpw_te_reset"]
    reset.restype, reset.argtypes = None, [C.c_char_p, C.c_uint32, C.c_uint32]
    op_fn = lib["tpw_te_op"]
    op_fn.restype, op_fn.argtypes = C.c_int64, [C.c_int32, C.POINTER(C.c_int32)]
    state_fn = lib["tpw_te_state"]
    state_fn.restype, state_fn.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    address = lib["tpw_te_address"]
    address.restype, address.argtypes = C.c_void_p, [C.c_int32]
    data = bytes(case.get("file", b""))
    reset(data, len(data), case["seed"])
    CL.host_begin(lib, {label: address(label) for label in range(1, 11)}, case.get("q"))
    results, states, marks = [], [], []
    buffer = (C.c_int64 * 256)()
    try:
        for op, args in case["script"]:
            padded = (C.c_int32 * 16)(*[T.s32(v) for v in (list(args) + [0] * 16)[:16]])
            result = op_fn(op, padded)
            if op in (4, 25) or (op == 14 and args[1] < 4):
                result &= MINUS
            results.append(result)
            states.append(list(buffer[:state_fn(buffer)]))
            marks.append(C.c_int32.in_dll(lib, "tpw_cl_n").value)
    finally:
        log = CL.host_end(lib)
    return dict(results=results, log=log, marks=marks, states=states)


# ----------------------------------------------------------------- scripts


def fb(x):
    return T.f32_bits(x)


def wct(version=0x13, threshold=0, kind=0x20, level=1, width=64, height=64, quality=(4, 8, 12, 2), data=b"DATA1234",
        alpha=b"ALPHAxyz", colour=b"\x11\x22\x33\x44", data_size=None, alpha_size=None):
    """A wavelet texture file: the 0x1c-byte header (little-endian), the data, the alpha."""
    header = struct.pack("<BBBBHHHHHHII", version, threshold, kind, level, width, height, *quality,
                         len(data) if data_size is None else data_size, len(alpha) if alpha_size is None else alpha_size)
    return header + colour + data + (alpha if kind == 0x20 else b"")


def entry_op(r):
    kind = r.choice([2, 3, 3, 4, 4, 5, 6, 7, 7, 8, 8, 8, 9, 9, 10, 11, 12, 13, 14, 15, 15, 15, 16, 16, 16, 17])
    index = r.randrange(2)
    if kind == 2:
        return (2, [index, r.choice([0, 1, 8, 16, 64]), r.randrange(0x40)])
    if kind == 5:
        return (5, [index, r.choice([0, 1, 8, 9, 16, 17, 32, 33, 64, 65, 128, 129, 256, -5, r.randrange(300)])])
    if kind == 9:
        return (9, [index, r.choice([0, 1, 8, 16, 100, 256, r.randrange(1, 40)]), r.choice([0, 1, 8, 16, 300, r.randrange(1, 40)])])
    if kind == 14:
        return (14, [index, r.randrange(5)])
    if kind == 15:
        flags = r.getrandbits(32) if r.random() < 0.5 else r.choice(
            [0, 0x100, 0x200, 0x300, 0x1000, 0x4000, 0x30, 0x35, 0x10000, 0x40000, 0x80000, 0x800000, 0x4000000, 0x4010300])
        width = r.choice([1, 2, 4, 8, 16, 5])
        return (15, [index, flags, width, r.choice([1, 2, 4, 8, 16, 6, min(16, 2 * width)]), r.choice([0, 8, 9, 64, 200]),
                     fb(r.choice([0.0, 0.5, 1.0, 1.5, 2.0, -1.0, 0.123])), fb(r.random()), fb(r.random()), fb(r.random()),
                     fb(r.random())])
    if kind == 16:
        return (16, [index, r.randrange(4), r.randrange(2), r.randrange(2), r.randrange(2)])
    if kind == 17:
        return (17, [0, r.choice([0, 0, 0x40000000, 0x80000000, r.getrandbits(32)]), r.choice([3, 3, 4, 1]),
                     r.choice([4, 4, 3, 5, 0])])
    return (kind, [index])


def wave_op(r):
    kind = r.choice([20, 21, 21, 21, 22, 23, 23, 24, 24, 24, 25, 26, 27, 28, 28])
    if kind in (23, 24):
        return (kind, [0, r.choice([1, 1, 1, 0]) if kind == 23 else 1, r.choice([8, 16, 4]), r.choice([1, 2, 4, 8]),
                       r.choice([8, 16, 32, 64]), r.randrange(2)])
    if kind == 28:
        return (28, [0, r.choice([0x12, 0x13, 0x11, 0x14, 0]), r.choice([64, 128, 1]), r.choice([64, 32, 1])]
                + [r.choice([0, 1, 2, 4, 7, 100, 0xFFFF]) for _ in range(4)] + [r.getrandbits(32)])
    return (kind, [0])


def rnd_file(r):
    if r.random() < 0.15:
        # Never shorter than the header: the original does not check how much it read, and
        # what a short read leaves of an earlier load in a field is seen through the
        # byte swap on the Mac only (the edge cases load a cut-off file once, into zeros).
        return bytes(r.getrandbits(8) for _ in range(r.randrange(0x20, 0x48)))
    return wct(version=r.choice([0x12, 0x13, 0x13, 0x11, 0x14]), threshold=r.choice([0, 0, 9]), kind=r.choice([0x20, 0x20, 0]),
               level=r.randrange(256), width=r.choice([64, 128, 0, 1]), height=r.choice([64, 32, 0]),
               quality=tuple(r.choice([0, 1, 4, 9, 0xFFFF]) for _ in range(4)),
               data=bytes(r.getrandbits(8) for _ in range(r.randrange(4, 40))),
               alpha=bytes(r.getrandbits(8) for _ in range(r.randrange(4, 40))), colour=bytes(r.getrandbits(8) for _ in range(4)))


def queues(r, fail):
    q = {"Flipped": [r.randrange(2) for _ in range(4)], "Open": [1 if r.random() < 0.85 else 0]}
    if fail:
        q["Malloc"] = [1 if r.random() < 0.7 else 0 for _ in range(r.randrange(1, 8))] + [1]
    return q


def case_of(r, makers, length, fail=False):
    script = [(1, [0]), (1, [1]), (20, [0])]
    script += [r.choice(makers)(r) for _ in range(length)]
    script += [(3, [0]), (3, [1]), (27, [0])]
    return dict(seed=r.randrange(256), file=rnd_file(r), q=queues(r, fail), script=script)


def edge_entries():
    r = random.Random(91)
    base = [(1, [0]), (1, [1])]
    yield dict(seed=1, script=base + [(2, [0, 16, 5]), (7, [0]), (6, [0]), (4, [0]), (13, [0]), (3, [0]), (3, [0])])
    yield dict(seed=2, q={"Malloc": [0]}, script=base + [(2, [1, 8, 1]), (3, [1])])
    for size in (0, 1, 8, 9, 16, 17, 32, 33, 64, 65, 128, 129, 256):
        yield dict(seed=3, script=base + [(5, [0, size]), (9, [0, size, 4]), (9, [1, 4, size]), (6, [0])])
    for flags in (0, 0x100, 0x200, 0x300, 0x10000, 0x20000, 0x4000000, 0x4010300):
        for renderer in (3, 4):
            yield dict(seed=4, script=base + [(17, [0, 0, renderer, 4]), (15, [0, flags, 8, 8, 8, 0, 0, 0, 0, 0]), (4, [0]), (13, [0])])
    for system in (0, 0x40000000, 0x80000000):
        for flags in (0, 0x4000, 0x7000, 0xF000):
            yield dict(seed=5, script=base + [(17, [0, system, 3, 4]), (15, [0, flags, 4, 4, 4, 0, 0, 0, 0, 0]), (10, [0]),
                                              (11, [0]), (12, [0])])
    # Close: a further plane, an entry on a cache page, each kind of image.
    for kind in range(4):
        for extra in (0, 1):
            for flags in (0, 0x1000, 0x800000, 0x801000):
                yield dict(seed=6, script=base + [(16, [0, kind, extra, 1, 1]), (16, [1, kind, 0, 0, 0]),
                                                  (15, [0, flags, 4, 4, 4, 0, 0, 0, 0, 0]), (3, [0]), (3, [1])])
    # GetData and GetCorrectedData: plain and compressed, flipped by flag and by the source.
    for kind in (1, 3):
        for flags in (0, 0x40000):
            for natural in (0, 1):
                for fmt in (4, 3):
                    for width, height in ((8, 8), (4, 8), (16, 2), (5, 5), (1, 1), (16, 16)):
                        yield dict(seed=7, q={"Flipped": [natural]},
                                   script=base + [(17, [0, 0, 3, fmt]), (16, [0, kind, 0, 0, 0]),
                                                  (15, [0, flags, width, height, 0, 0, 0, 0, 0, 0]), (7, [0]), (8, [0])])
    for priority in (0.0, 0.5, 1.5, 2.0, -1.0):
        for flags in (0, 0x4000, 0x80000):
            yield dict(seed=8, script=base + [(15, [0, flags, 1, 1, 1, fb(priority), fb(0.25), fb(0.5), fb(0.75), fb(1.0)]),
                                              (9, [0, 24, 40])] + [(14, [0, k]) for k in range(5)])
    yield dict(seed=9, script=base + [(16, [0, 1, 0, 1, 0]), (15, [0, 0x1000, 4, 4, 4, 0, 0, 0, 0, 0]), (14, [0, 4]),
                                      (15, [0, 0, 4, 4, 4, 0, 0, 0, 0, 0]), (14, [0, 4])])
    for _ in range(40):
        yield case_of(r, [entry_op], 16)
    for _ in range(10):
        yield case_of(r, [entry_op], 16, fail=True)


def edge_wave():
    r = random.Random(92)
    base = [(20, [0])]
    yield dict(seed=1, file=wct(), script=base + [(21, [0]), (25, [0]), (26, [0]), (23, [0, 1, 64, 64, 128]),
                                                 (24, [0, 1, 32, 16, 64, 0]), (24, [0, 1, 32, 16, 64, 1]), (22, [0]),
                                                 (22, [0]), (27, [0])])
    for version in (0x10, 0x11, 0x12, 0x13, 0x14, 0xFF):
        yield dict(seed=2, file=wct(version=version), script=base + [(21, [0]), (26, [0]), (24, [0, 1, 8, 8, 16, 1]), (27, [0])])
    for width, height in ((0, 64), (64, 0), (0, 0)):
        yield dict(seed=3, file=wct(width=width, height=height), script=base + [(21, [0]), (27, [0])])
    for kind in (0x20, 0, 0x21):
        yield dict(seed=4, file=wct(kind=kind, level=0xFE), script=base + [(21, [0]), (27, [0])])
    yield dict(seed=5, q={"Open": [0]}, file=wct(), script=base + [(21, [0]), (27, [0])])
    for failing in range(3):
        yield dict(seed=6, q={"Malloc": [1] * failing + [0, 1]}, file=wct(), script=base + [(21, [0]), (27, [0])])
    yield dict(seed=7, file=wct()[:20], script=base + [(21, [0]), (27, [0])])
    yield dict(seed=8, file=wct(), script=base + [(28, [0, 0x13, 64, 64, 1, 2, 3, 4, 0x12345678]), (23, [0, 0, 8, 8, 16]),
                                                 (25, [0])])
    for quality in ((0, 0, 0, 0), (1, 1, 1, 1), (3, 5, 7, 9), (0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF)):
        yield dict(seed=9, script=base + [(28, [0, 0x13, 64, 32, *quality, 0]), (23, [0, 1, 8, 8, 16]),
                                         (24, [0, 1, 8, 4, 16, 1])])
    for _ in range(40):
        yield case_of(r, [wave_op], 12)
    for _ in range(10):
        yield case_of(r, [wave_op], 12, fail=True)


TESTS = [
    FnTest("CTextureEntry", ppc, host, edge=edge_entries,
           rnd=lambda r: case_of(r, [entry_op], 18, fail=r.random() < 0.3)),
    FnTest("CWaveLoad and CCompBase", ppc, host, edge=edge_wave,
           rnd=lambda r: case_of(r, [wave_op], 12, fail=r.random() < 0.3)),
    FnTest("texture entry and wave loader, mixed scripts", ppc, host,
           rnd=lambda r: case_of(r, [entry_op, entry_op, wave_op], 22, fail=r.random() < 0.3)),
]
