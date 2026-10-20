"""Differential tests for engine landscape.cpp, plus CPU (the engine's first code, same
claim), the Bitmap methods landscape uses (clear, realloc, load, the out-of-line
destructor) and SetFileCallbacks (Wavelet.cpp). See difftest.py and tpwtest.py.

Objects holding pointers (Bitmap, Tileset, Landscape, sTileOverlay) have different
layouts on the two sides, so the cases describe them as Python values that PpcSide
(emulator memory at the original offsets, big-endian) and HostSide (shim accessors in
tests/shim_landscape.cpp) write and read back. Pixel buffers carry labels; allocations
and frees compare as ordered logs of (kind, size) and (kind, label): the operator
new[]/delete[] imports on the PowerPC side (tpwtest emu.alloc_log), the replaced global
operators of tests/shim_alloc.cpp on the host. Contents of freshly allocated buffers are
only compared where the code initialises them.

Stubs (tpwtest.install_std_stubs): memset, operator new/new[]/delete/delete[] (blocks in
the per-case scratch area), _USwapBlock32 (faithful), Gestalt (answer chosen per case),
BlockMoveData (memmove), __construct_array/__destroy_arr (call the original
constructors/destructors), __register_global_object (recorded). File callbacks: fake
functions over one in-memory file with the same semantics on both sides
(tpwtest.FakeFile; an assumption only in that the game's real callbacks are C stdio).
"""
import ctypes
import struct

import tpwtest as T
from tpwtest import Buf, FnTest, Glob, I, PpcOnly, Test, rint

PROGRAM = "engine symbols shared"

# Engine data addresses (TOC loads in the listing).
CLOCK_MHZ = 0x1005245c
TILE_BUFFER = 0x100524e8
RAMPS = {"gRampB": 0x100524ec, "gRampA": 0x100525ec, "gRampC": 0x100526ec,
         "gRampCover2": 0x100527ec, "gRampCover1": 0x100528ec}
CONTROLS = {"gControl2": 0x100529ec, "gControl3": 0x10052a08, "gControl1": 0x10052a24}
FILE_FNS = {"open": 0x101735e4, "close": 0x101735e0, "read": 0x101735d8, "seek": 0x101735dc,
            "tell": 0x101735d4, "name": 0x101735d0}
LANDSCAPE_SIZE, TILESET_SIZE = 0x2ac, 0xd8
NBM = 13
TEX4 = {0, 1, 3, 4, 8, 9}          # tileset bitmaps with 4 bytes per pixel


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


VP, U32, I32, I16 = ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int32, ctypes.c_int16

# ----------------------------------------------------------------- the two sides


class PpcSide:
    e = ">"

    def __init__(self, emu):
        self.emu = emu
        self.labels, self.sizes = {}, {}
        emu.alloc_log = []
        emu.registered = []

    def pack(self, fmt, *v):
        return struct.pack(self.e + fmt, *v)

    def unpack(self, fmt, data):
        return struct.unpack(self.e + fmt, data)

    def read(self, a, n):
        return self.emu.read(a, n)

    def write(self, a, data):
        self.emu.write(a, bytes(data))

    def obj(self, size, junk):
        return self.emu.alloc(size, (junk * (size // len(junk) + 1))[:size])

    def buf(self, label, data):
        a = self.emu.alloc(max(len(data), 4), bytes(data))
        self.labels[a], self.sizes[a] = label, len(data)
        return a

    def label(self, a):
        return None if a == 0 else self.labels.get(a, "?%x" % a)

    def contents(self, a, n):
        if a == 0 or a not in self.sizes:
            return None
        return self.emu.read(a, min(n & 0xFFFFFFFF, self.sizes[a]))

    # layout
    def control(self, L):
        return L + 0x14

    def tileset(self, L, i):
        return L + 0x24 + TILESET_SIZE * i

    def tbitmap(self, t, k):
        return t + 8 + 16 * k

    def glob_bitmap(self, name):
        return CONTROLS[name]

    def ramp(self, name):
        return RAMPS[name]

    def set_bitmap(self, a, bm):
        w, h, bpp, lab, data = bm
        p = self.buf(lab, data) if lab is not None else 0
        self.write(a, struct.pack(">IIII", w, h, bpp, p))

    def get_bitmap(self, a, contents=True):
        w, h, bpp, p = struct.unpack(">IIII", self.read(a, 16))
        return (w, h, bpp, self.label(p), self.contents(p, w * h * bpp) if contents else None)

    def set_tilebuf(self, lab, data):
        self.write(TILE_BUFFER, struct.pack(">I", self.buf(lab, data) if lab is not None else 0))

    def get_tilebuf(self, contents=True):
        p = self.emu.u32(TILE_BUFFER)
        return (self.label(p), self.contents(p, 1 << 20) if contents else None)

    def begin(self):
        self.emu.alloc_log = []

    def finish(self, this=None):
        out, k = [], 0
        for kind, a, size in self.emu.alloc_log:
            if kind in ("new", "new[]"):
                self.labels[a], self.sizes[a] = "new%d" % k, size
                k += 1
                out.append((kind, size))
            else:
                out.append((kind, "self" if this is not None and a == this else self.label(a)))
        return out

    def call(self, label, *args):
        return T.call_ppc(self.emu, self.emu.func(label), list(args))

    # files
    def files(self, data, fail_open=False):
        for name in ("open", "close", "read", "seek"):
            self.write(FILE_FNS[name], struct.pack(">I", self.emu.file_tvecs[name]))
        self.emu.fake_file = T.FakeFile(data)
        self.emu.fake_file.fail_open = fail_open
        return T.FAKE_FILE_HANDLE

    def file_log(self):
        return list(self.emu.fake_file.log)


class HostSide:
    e = "<"

    def __init__(self, lib):
        self.lib = lib
        self.labels, self.sizes, self.live = {}, {}, set()
        self.keep = []
        self._log_on = I32.in_dll(lib, "tpw_alloc_log_on")
        self._log_on.value = 0

    pack = PpcSide.pack
    unpack = PpcSide.unpack

    def read(self, a, n):
        return ctypes.string_at(a, n)

    def write(self, a, data):
        data = bytes(data)
        ctypes.memmove(a, data, len(data))

    def buf(self, label, data):
        data = bytes(data)
        p = F(self.lib, "tpw_new_bytes", VP, U32, ctypes.c_char_p)(len(data), data)
        self.labels[p], self.sizes[p] = label, len(data)
        self.live.add(p)
        return p

    def label(self, a):
        return None if not a else self.labels.get(a, "?%x" % a)

    def contents(self, a, n):
        if not a or a not in self.sizes:
            return None
        return ctypes.string_at(a, min(n & 0xFFFFFFFF, self.sizes[a]))

    def control(self, L):
        return F(self.lib, "tpw_landscape_control", VP, VP)(L)

    def tileset(self, L, i):
        return F(self.lib, "tpw_landscape_tileset", VP, VP, I32)(L, i)

    def tbitmap(self, t, k):
        return F(self.lib, "tpw_tileset_bitmap", VP, VP, I32)(t, k)

    def glob_bitmap(self, name):
        return ctypes.addressof(ctypes.c_char.in_dll(self.lib, name))

    def ramp(self, name):
        return ctypes.addressof(ctypes.c_char.in_dll(self.lib, name))

    def set_bitmap(self, a, bm):
        w, h, bpp, lab, data = bm
        p = self.buf(lab, data) if lab is not None else None
        F(self.lib, "tpw_bitmap_set", None, VP, U32, U32, U32, VP)(a, w, h, bpp, p)

    def get_bitmap(self, a, contents=True):
        whb, p = (U32 * 3)(), VP()
        F(self.lib, "tpw_bitmap_get", None, VP, ctypes.POINTER(U32), ctypes.POINTER(VP))(a, whb, ctypes.byref(p))
        w, h, bpp = whb
        return (w, h, bpp, self.label(p.value), self.contents(p.value, w * h * bpp) if contents else None)

    def set_tilebuf(self, lab, data):
        VP.in_dll(self.lib, "gTileBuffer").value = self.buf(lab, data) if lab is not None else None

    def get_tilebuf(self, contents=True):
        p = VP.in_dll(self.lib, "gTileBuffer").value
        return (self.label(p), self.contents(p, 1 << 20) if contents else None)

    def begin(self):
        F(self.lib, "tpw_alloc_reset", None)()
        self._log_on.value = 1

    def finish(self, freed_self=False):
        self._log_on.value = 0
        n = I32.in_dll(self.lib, "tpw_alloc_n").value
        kinds = (I32 * 1024).in_dll(self.lib, "tpw_alloc_kind")
        ptrs = (ctypes.c_uint64 * 1024).in_dll(self.lib, "tpw_alloc_ptr")
        sizes = (ctypes.c_uint64 * 1024).in_dll(self.lib, "tpw_alloc_size")
        out, k = [], 0
        for i in range(n):
            kind, p = kinds[i], ptrs[i]
            if kind in (1, 2):
                self.labels[p], self.sizes[p] = "new%d" % k, sizes[i]
                self.live.add(p)
                k += 1
                out.append(("new" if kind == 1 else "new[]", sizes[i]))
            else:
                out.append(("delete" if kind == 3 else "delete[]", self.label(p)))
                self.live.discard(p)
        if freed_self:
            out.append(("delete", "self"))
        return out

    def release(self):
        """Free every buffer still live and clear the globals that could point at them
        (gControl1..3 have static destructors that would free them again at exit)."""
        self._log_on.value = 0
        for name in CONTROLS:
            self.set_bitmap(self.glob_bitmap(name), (0, 0, 0, None, None))
        VP.in_dll(self.lib, "gTileBuffer").value = None
        free = F(self.lib, "tpw_free_bytes", None, VP)
        for p in self.live:
            free(p)
        self.live.clear()

    # files
    def files(self, data, fail_open=False):
        b = ctypes.create_string_buffer(bytes(data), max(len(data), 1))
        self.keep.append(b)
        F(self.lib, "tpw_file_set", None, VP, U32)(ctypes.addressof(b), len(data))
        I32.in_dll(self.lib, "tpw_file_fail_open").value = 1 if fail_open else 0
        F(self.lib, "tpw_file_install", None)()
        return F(self.lib, "tpw_file_handle", VP)()

    def file_log(self):
        n = I32.in_dll(self.lib, "tpw_file_log_n").value
        log = ((U32 * 4) * 2048).in_dll(self.lib, "tpw_file_log")
        name = ctypes.string_at(ctypes.addressof(ctypes.c_char.in_dll(self.lib, "tpw_file_open_name"))).decode("latin-1")
        mode = ctypes.string_at(ctypes.addressof(ctypes.c_char.in_dll(self.lib, "tpw_file_open_mode"))).decode("latin-1")
        out = []
        for i in range(n):
            kind, a, b, ok = log[i]
            if kind == 1:
                out.append(("open", name, mode))
            elif kind == 2:
                out.append(("close", bool(ok)))
            elif kind == 3:
                out.append(("read", a, b, bool(ok)))
            else:
                out.append(("seek", a, b, bool(ok)))
        return out

# ----------------------------------------------------------------- object models
# Bitmap value: (width, height, bytes per pixel, buffer label or None, buffer bytes).


def put_tileset(side, t, ts):
    side.write(t, side.pack("iB", ts["size"], ts["level"]))
    for k in range(NBM):
        side.set_bitmap(side.tbitmap(t, k), ts["bms"][k])


def get_tileset(side, t, contents=True):
    size, level = side.unpack("iB", side.read(t, 5))
    return dict(size=size, level=level, bms=[side.get_bitmap(side.tbitmap(t, k), contents) for k in range(NBM)])


def put_landscape(side, L, st):
    side.write(L, side.pack("12Bii", *st["head"]))
    side.set_bitmap(side.control(L), st["control"])
    for i in range(3):
        put_tileset(side, side.tileset(L, i), st["tilesets"][i])


def get_landscape(side, L, contents=True):
    return dict(head=side.unpack("12Bii", side.read(L, 20)),
                control=side.get_bitmap(side.control(L), contents),
                tilesets=[get_tileset(side, side.tileset(L, i), contents) for i in range(3)])


def ppc_landscape(side, st):
    L = side.obj(LANDSCAPE_SIZE, b"\xa5")
    put_landscape(side, L, st)
    return L


def host_landscape(side, st):
    L = F(side.lib, "tpw_landscape_store", VP)()
    put_landscape(side, L, st)
    return L

# ----------------------------------------------------------------- random values


def rbm(r, label, w=None, h=None, bpp=None, null=0.25):
    w = r.randint(0, 6) if w is None else w
    h = r.randint(0, 6) if h is None else h
    bpp = r.choice([1, 4]) if bpp is None else bpp
    if r.random() < null:
        return (w, h, bpp, None, None)
    return (w, h, bpp, label, r.randbytes(w * h * bpp))


def rtileset(r, tag, null=0.25):
    return dict(size=rint(r), level=r.getrandbits(8), bms=[rbm(r, "%s.%d" % (tag, k), null=null) for k in range(NBM)])


def rhead(r):
    return [r.getrandbits(8) for _ in range(12)] + [rint(r), rint(r)]


def rlandscape(r, null=0.25):
    return dict(head=rhead(r), control=rbm(r, "control", null=null),
                tilesets=[rtileset(r, "ts%d" % i, null) for i in range(3)])


def bm_header(w, h, bpp):
    return struct.pack("<III", w, h, bpp)          # little-endian, as the data files

# ----------------------------------------------------------------- CPU


def cpu_case(r):
    k = r.random()
    clock = 0 if k < 0.6 else rint(r)
    err = 0 if r.random() < 0.7 else r.choice([-5550, -5551, 1, -1, 0x7fff, -0x8000])
    hz = r.choice([0, 1, 999999, 1000000, 1000001, 266000000, 450000000, -1, -999999, -1000000, -1000001,
                   0x7fffffff, -0x80000000]) if r.random() < 0.3 else rint(r)
    return dict(clock=clock, err=err, hz=hz, junk=r.randbytes(32))


def cpu_edges():
    out = []
    for hz in (0, 1, 999999, 1000000, 266000000, -1, -1000000, -1000001, 0x7fffffff, -0x80000000):
        out.append(dict(clock=0, err=0, hz=hz, junk=bytes(range(32))))
    out.append(dict(clock=7, err=0, hz=500000000, junk=bytes(32)))
    out.append(dict(clock=0, err=-5550, hz=500000000, junk=b"\xff" * 32))
    return out


def cpu_detect_ppc(emu, c):
    s = PpcSide(emu)
    obj = s.buf("cpu", c["junk"])
    emu.write(CLOCK_MHZ, struct.pack(">i", c["clock"]))
    emu.gestalt = (T.u32(c["err"]), c["hz"])
    s.call("detect__3CPUFv", obj)
    return dict(obj=s.unpack("4i", s.read(obj, 16)), name=s.read(obj + 16, 16),
                clock=s.unpack("i", emu.read(CLOCK_MHZ, 4))[0])


def cpu_detect_host(lib, c):
    s = HostSide(lib)
    obj = F(lib, "tpw_cpu_store", VP)()
    s.write(obj, c["junk"])
    I32.in_dll(lib, "gCPUClockMHz").value = c["clock"]
    I16.in_dll(lib, "tpw_gestalt_err").value = c["err"]
    I32.in_dll(lib, "tpw_gestalt_value").value = c["hz"]
    F(lib, "tpw_CPU_detect", None, VP)(obj)
    return dict(obj=s.unpack("4i", s.read(obj, 16)), name=s.read(obj + 16, 16),
                clock=I32.in_dll(lib, "gCPUClockMHz").value)


def cpm_case(r):
    return dict(clock=rint(r), arg=rint(r), junk=r.randbytes(32))


def cpm_ppc(emu, c):
    s = PpcSide(emu)
    obj = s.buf("cpu", c["junk"])
    emu.write(CLOCK_MHZ, struct.pack(">i", c["clock"]))
    r3, _, _ = s.call("clocks_per_millisec__3CPUFi", obj, I(c["arg"]))
    return dict(ret=T.s32(r3), obj=s.read(obj, 32))


def cpm_host(lib, c):
    s = HostSide(lib)
    obj = F(lib, "tpw_cpu_store", VP)()
    s.write(obj, c["junk"])
    I32.in_dll(lib, "gCPUClockMHz").value = c["clock"]
    ret = F(lib, "tpw_CPU_clocks_per_millisec", I32, VP, I32)(obj, T.s32(c["arg"]))
    return dict(ret=ret, obj=s.read(obj, 32))

# ----------------------------------------------------------------- resample_control


def resample_case(r):
    tx, ty = r.randint(0, 3), r.randint(0, 3)
    sw, sh = tx * 4 + 5 + r.randint(0, 3), ty * 4 + 5 + r.randint(0, 3)
    dw = r.choice([4, 8, 16, 32, 64, 128, 8, 16, 32, 64, 6, 12, 13, 20, 256])
    k = r.random()
    if k < 0.2:
        src = bytes([r.choice([0, 255])]) * (sw * sh * 4)
    elif k < 0.4:
        src = bytes(r.choice([0, 255, r.getrandbits(8)]) for _ in range(sw * sh * 4))
    else:
        src = r.randbytes(sw * sh * 4)
    return dict(tx=tx, ty=ty, ch=r.randint(0, 3), src=(sw, sh, 4, "src", src),
                dst=(dw, dw, 1, "dst", r.randbytes(dw * dw)))


def resample_edges():
    out = []
    for dw in (4, 8, 16, 32, 64, 128, 256, 12):
        for ch in range(4):
            src = bytes((i * 37 + ch * 11) & 0xff for i in range(9 * 9 * 4))
            out.append(dict(tx=1, ty=1, ch=ch, src=(9, 9, 4, "src", src), dst=(dw, dw, 1, "dst", bytes(dw * dw))))
    # More destination widths: the other arms of the switch on n = width / 4 (128, 256, and
    # values between and above the powers of two).
    for dw in (512, 1024, 320, 600, 1100, 48, 100):
        src = bytes((i * 53 + 7) & 0xff for i in range(9 * 9 * 4))
        out.append(dict(tx=1, ty=1, ch=2, src=(9, 9, 4, "src", src), dst=(dw, dw, 1, "dst", bytes(dw * dw))))
    out.append(dict(tx=0, ty=0, ch=1, src=(5, 5, 4, "src", b"\xff" * 100), dst=(64, 64, 1, "dst", bytes(4096))))
    out.append(dict(tx=0, ty=0, ch=2, src=(5, 5, 4, "src", bytes(100)), dst=(64, 64, 1, "dst", b"\x55" * 4096)))
    return out


def resample_ppc(emu, c):
    s = PpcSide(emu)
    src, dst = s.obj(16, b"\0"), s.obj(16, b"\0")
    s.set_bitmap(src, c["src"])
    s.set_bitmap(dst, c["dst"])
    r3, _, _ = s.call("resample_control__FRC6BitmapUlUlUlR6Bitmap", src, c["tx"], c["ty"], c["ch"], dst)
    return dict(ret=T.s32(r3), src=s.get_bitmap(src), dst=s.get_bitmap(dst))


def resample_host(lib, c):
    s = HostSide(lib)
    src, dst = F(lib, "tpw_bitmap_store", VP, I32)(0), F(lib, "tpw_bitmap_store", VP, I32)(1)
    s.set_bitmap(src, c["src"])
    s.set_bitmap(dst, c["dst"])
    ret = F(lib, "tpw_resample_control", I32, VP, U32, U32, U32, VP)(src, c["tx"], c["ty"], c["ch"], dst)
    out = dict(ret=ret, src=s.get_bitmap(src), dst=s.get_bitmap(dst))
    s.release()
    return out

# ----------------------------------------------------------------- blend kernels


def kernel_bytes(r, n):
    k = r.random()
    if k < 0.1:
        return [0] * n
    if k < 0.2:
        return [255] * n
    if k < 0.35:
        return [r.choice([0, 1, 127, 128, 254, 255]) for _ in range(n)]
    return list(r.randbytes(n))


KERNEL_ARGS = {
    "blend_3": ["a1:64", "b1:64", "m1:16", "t1:256", "a2:64", "b2:64", "m2:16", "t2:256",
                "a3:64", "b3:64", "m3:16", "t3:256", "c:16", "w12:16", "w23:16", "out:64"],
    "blend_2": ["a1:64", "b1:64", "m1:16", "t1:256", "a2:64", "b2:64", "m2:16", "t2:256", "c:16", "w:16", "out:64"],
    "single_layer": ["a:64", "b:64", "m:16", "t:256", "c:16", "out:64"],
    "calc_cover": ["mapA:16", "mapB:16", "ramp:256", "control:16", "out:16"],
}
KERNEL_PPC = {
    "blend_3": "blend_3__FPUcPUcPUcPUcPUcPUcPUcPUcPUcPUcPUcPUcPUcPUcPUcPUc",
    "blend_2": "blend_2__FPUcPUcPUcPUcPUcPUcPUcPUcPUcPUcPUc",
    "single_layer": "single_layer__FPUcPUcPUcPUcPUcPUc",
    "calc_cover": "calc_cover__FPUcPUcPUcPUcPUc",
}


def kernel_test(name):
    specs = [(a.split(":")[0], int(a.split(":")[1])) for a in KERNEL_ARGS[name]]

    def rnd(r):
        return {n: kernel_bytes(r, size) for n, size in specs}

    def edges():
        out = []
        for v in (0, 255, 128):
            out.append({n: [v] * size for n, size in specs})
        out.append({n: [(i * 29 + len(n) * 7) & 0xff for i in range(size)] for n, size in specs})
        out.append({n: ([255] * size if n.startswith(("t", "ramp")) else [(i * 16) & 0xff for i in range(size)])
                    for n, size in specs})
        return out

    return Test(name, KERNEL_PPC[name], "tpw_" + name,
                lambda c: [Buf(n, "%dB" % size, c[n]) for n, size in specs],
                ret="u8" if name == "calc_cover" else "none", edge=edges, rnd=rnd)

# ----------------------------------------------------------------- Bitmap methods


def bitmap_state_case(r):
    w, h, bpp = r.randint(0, 9), r.randint(0, 9), r.choice([0, 1, 2, 3, 4])
    return rbm(r, "data", w, h, bpp, null=0.3)


def bitmap_fn(method, extra_ppc, extra_host, ret=True):
    """FnTest for Bitmap::<method> on one bitmap: case["bm"] is the starting state."""
    def ppc(emu, c):
        s = PpcSide(emu)
        b = s.obj(16, b"\0")
        s.set_bitmap(b, c["bm"])
        handle = s.files(c["file"]) if "file" in c else None
        s.begin()
        r3, _, _ = s.call(method, b, *extra_ppc(c, handle))
        log = s.finish()
        out = dict(bm=s.get_bitmap(b), allocs=log)
        if ret:
            out["ret"] = T.s32(r3)
        if "file" in c:
            out["file"] = s.file_log()
        return out

    def host(lib, c):
        s = HostSide(lib)
        b = F(lib, "tpw_bitmap_store", VP, I32)(0)
        s.set_bitmap(b, c["bm"])
        handle = s.files(c["file"]) if "file" in c else None
        s.begin()
        r = extra_host(lib, b, c, handle)
        log = s.finish()
        out = dict(bm=s.get_bitmap(b), allocs=log)
        if ret:
            out["ret"] = r
        if "file" in c:
            out["file"] = s.file_log()
        s.set_bitmap(b, (0, 0, 0, None, None))
        s.release()
        return out
    return ppc, host


def realloc_case(r):
    bm = bitmap_state_case(r)
    if r.random() < 0.4:
        w, h, bpp = bm[0], bm[1], bm[2]
    else:
        w, h, bpp = r.randint(0, 9), r.randint(0, 9), r.choice([0, 1, 2, 4])
    return dict(bm=bm, w=w, h=h, bpp=bpp)


def realloc_edges():
    return [dict(bm=(4, 4, 1, "data", bytes(range(16))), w=4, h=4, bpp=1),
            dict(bm=(4, 4, 1, None, None), w=4, h=4, bpp=1),
            dict(bm=(4, 4, 1, "data", bytes(range(16))), w=2, h=8, bpp=1),
            dict(bm=(0, 0, 0, None, None), w=0, h=0, bpp=0),
            dict(bm=(3, 3, 4, "data", b"\x77" * 36), w=3, h=3, bpp=4),
            dict(bm=(3, 3, 4, "data", b"\x77" * 36), w=3, h=3, bpp=1)]


def file_bitmap(r, w, h, bpp, truncate=0.1):
    data = bm_header(w, h, bpp) + r.randbytes(w * h * bpp)
    if r.random() < truncate:
        data = data[:r.randint(0, len(data))]
    return data


def load_case(r):
    bm = bitmap_state_case(r)
    if r.random() < 0.4:
        w, h, bpp = bm[0], bm[1], bm[2]
    else:
        w, h, bpp = r.randint(0, 9), r.randint(0, 9), r.choice([1, 2, 4])
    data = file_bitmap(r, w, h, bpp, truncate=0.0) + r.randbytes(r.randint(0, 8))
    if r.random() < 0.1:           # cut anywhere (reads past the end give zeros)
        data = data[:r.randint(0, len(data))]
    return dict(bm=bm, file=data)


def load_edges():
    out = []
    for bm in ((3, 2, 4, "data", bytes(24)), (3, 2, 4, None, None), (0, 0, 0, None, None), (5, 5, 1, "data", bytes(25))):
        out.append(dict(bm=bm, file=bm_header(3, 2, 4) + bytes(range(24))))
    out.append(dict(bm=(3, 2, 4, "data", bytes(24)), file=bm_header(3, 2, 4) + bytes(range(10))))
    out.append(dict(bm=(1, 1, 1, "data", b"\x09"), file=b""))
    # same width and height, other depth; same width, other height
    out.append(dict(bm=(3, 2, 4, "data", bytes(24)), file=bm_header(3, 2, 1) + bytes(range(6))))
    out.append(dict(bm=(3, 2, 4, "data", bytes(24)), file=bm_header(3, 1, 4) + bytes(range(12))))
    return out


def bitmap_dtor_ppc(emu, c):
    s = PpcSide(emu)
    b = s.obj(16, b"\0")
    s.set_bitmap(b, c["bm"])
    s.begin()
    r3, _, _ = s.call("__dt__6BitmapFv", b, T.u32(T.s16(c["flag"])))
    return dict(bm=s.get_bitmap(b), allocs=s.finish(this=b), ret_this=r3 == b)


def bitmap_dtor_host(lib, c):
    s = HostSide(lib)
    b = F(lib, "tpw_bitmap_store", VP, I32)(0)
    s.set_bitmap(b, c["bm"])
    s.begin()
    freed = F(lib, "tpw_Bitmap_dtor", I32, VP, I16)(b, T.s16(c["flag"]))
    out = dict(bm=s.get_bitmap(b), allocs=s.finish(freed_self=bool(freed)), ret_this=True)
    s.release()
    return out


FLAGS = [-1, 0, 1, 2, 0x7fff, -0x8000, 0x100]

# ----------------------------------------------------------------- Tileset


def tileset_fn(method, ppc_args=lambda c, h: [], host_fn=None, ret=False, flag=False):
    def ppc(emu, c):
        s = PpcSide(emu)
        t = s.obj(TILESET_SIZE, c.get("junk", b"\0"))
        if "ts" in c:
            put_tileset(s, t, c["ts"])
        handle = s.files(c["file"]) if "file" in c else None
        s.begin()
        r3, _, _ = s.call(method, t, *ppc_args(c, handle))
        out = dict(allocs=s.finish(this=t), ts=get_tileset(s, t))
        if ret:
            out["ret"] = T.s32(r3)
        if flag:
            out["ret_this"] = r3 == t
        if "file" in c:
            out["file"] = s.file_log()
        return out

    def host(lib, c):
        s = HostSide(lib)
        t = F(lib, "tpw_tileset_store", VP)()
        if "junk" in c:
            s.write(t, (c["junk"] * 8)[:8])
        if "ts" in c:
            put_tileset(s, t, c["ts"])
        handle = s.files(c["file"]) if "file" in c else None
        s.begin()
        r = host_fn(lib, t, c, handle)
        out = dict(allocs=s.finish(freed_self=flag and bool(r)), ts=get_tileset(s, t))
        if ret:
            out["ret"] = r
        if flag:
            out["ret_this"] = True
        if "file" in c:
            out["file"] = s.file_log()
        for k in range(NBM):
            s.set_bitmap(s.tbitmap(t, k), (0, 0, 0, None, None))
        s.release()
        return out
    return ppc, host


def tileset_file(r, sizes=None):
    data = struct.pack("<i", rint(r)) + bytes([r.getrandbits(8)])
    for k in range(NBM):
        w, h = (r.randint(0, 5), r.randint(0, 5)) if sizes is None else sizes[k]
        data += file_bitmap(r, w, h, 4 if k in TEX4 else 1, truncate=0.0)
        # (no cut inside a tileset: the next header would come from pixel bytes and ask for a huge new[])
    return data


def tileset_load_case(r):
    ts = rtileset(r, "old")
    sizes = None
    if r.random() < 0.3:           # some bitmaps keep their size (no reallocation)
        sizes = [(bm[0], bm[1]) if r.random() < 0.7 else (r.randint(0, 5), r.randint(0, 5)) for bm in ts["bms"]]
        for k in range(NBM):
            w, h, bpp, lab, data = ts["bms"][k]
            bpp = 4 if k in TEX4 else 1
            ts["bms"][k] = (w, h, bpp, lab, None if lab is None else r.randbytes(w * h * bpp))
    data = tileset_file(r, sizes)
    if r.random() < 0.1:
        data = data[:r.randint(0, len(data))]
    return dict(ts=ts, file=data)

# ----------------------------------------------------------------- Landscape lifecycle


def landscape_fn(method, ppc_args=lambda c: [], host_fn=None, flag=False, tilebuf=True, contents=True,
                 ramps=False):
    def ppc(emu, c):
        s = PpcSide(emu)
        L = ppc_landscape(s, c["land"])
        if tilebuf:
            s.set_tilebuf(*c["tilebuf"])
        if ramps:
            for n in RAMPS:
                s.write(s.ramp(n), c["ramps"][n])
        s.begin()
        r3, _, _ = s.call(method, L, *ppc_args(c))
        out = dict(allocs=s.finish(this=L), land=get_landscape(s, L, contents))
        if tilebuf:
            out["tilebuf"] = s.get_tilebuf(contents)
        if ramps:
            out["ramps"] = {n: s.read(s.ramp(n), 256) for n in RAMPS}
        if flag:
            out["ret_this"] = r3 == L
        return out

    def host(lib, c):
        s = HostSide(lib)
        L = host_landscape(s, c["land"])
        if tilebuf:
            s.set_tilebuf(*c["tilebuf"])
        if ramps:
            for n in RAMPS:
                s.write(s.ramp(n), c["ramps"][n])
        s.begin()
        r = host_fn(lib, L, c)
        out = dict(allocs=s.finish(freed_self=flag and bool(r)), land=get_landscape(s, L, contents))
        if tilebuf:
            out["tilebuf"] = s.get_tilebuf(contents)
        if ramps:
            out["ramps"] = {n: s.read(s.ramp(n), 256) for n in RAMPS}
        if flag:
            out["ret_this"] = True
        put_landscape(s, L, dict(head=[0] * 14, control=(0, 0, 0, None, None),
                                 tilesets=[dict(size=0, level=0, bms=[(0, 0, 0, None, None)] * NBM)] * 3))
        s.release()
        return out
    return ppc, host


def rtilebuf(r):
    if r.random() < 0.3:
        return (None, None)
    return ("tilebuf", r.randbytes(r.choice([16, 64, 256])))


def land_case(r):
    return dict(land=rlandscape(r), tilebuf=rtilebuf(r), flag=r.choice(FLAGS))


def land_edges():
    empty = dict(head=[0] * 14, control=(0, 0, 0, None, None),
                 tilesets=[dict(size=0, level=0, bms=[(0, 0, 0, None, None)] * NBM) for _ in range(3)])
    full = dict(head=list(range(12)) + [5, 6], control=(2, 2, 4, "control", bytes(16)),
                tilesets=[dict(size=64 >> i, level=i, bms=[(1, 1, 1, "ts%d.%d" % (i, k), b"\x01") for k in range(NBM)])
                          for i in range(3)])
    return [dict(land=empty, tilebuf=(None, None), flag=0), dict(land=full, tilebuf=("tilebuf", bytes(64)), flag=1),
            dict(land=full, tilebuf=(None, None), flag=-1), dict(land=empty, tilebuf=("tilebuf", bytes(16)), flag=1)]


def ctor_case(r):
    return dict(junk=r.randbytes(16), land=rlandscape(r), tilebuf=rtilebuf(r))


def landscape_ctor_ppc(emu, c):
    s = PpcSide(emu)
    L = s.obj(LANDSCAPE_SIZE, c["junk"])
    s.begin()
    r3, _, _ = s.call("__ct__9LandscapeFv", L)
    return dict(allocs=s.finish(), land=get_landscape(s, L), ret_this=r3 == L)


def landscape_ctor_host(lib, c):
    s = HostSide(lib)
    L = F(lib, "tpw_landscape_store", VP)()
    s.write(L, (c["junk"] * 2)[:20])
    s.begin()
    F(lib, "tpw_Landscape_ctor", None, VP)(L)
    out = dict(allocs=s.finish(), land=get_landscape(s, L), ret_this=True)
    s.release()
    return out


def tileset_ctor_ppc(emu, c):
    s = PpcSide(emu)
    t = s.obj(TILESET_SIZE, c["junk"])
    s.begin()
    r3, _, _ = s.call("__ct__7TilesetFv", t)
    return dict(allocs=s.finish(), ts=get_tileset(s, t), ret_this=r3 == t)


def tileset_ctor_host(lib, c):
    s = HostSide(lib)
    t = F(lib, "tpw_tileset_store", VP)()
    s.write(t, (c["junk"] * 2)[:8])
    s.begin()
    F(lib, "tpw_Tileset_ctor", None, VP)(t)
    out = dict(allocs=s.finish(), ts=get_tileset(s, t), ret_this=True)
    s.release()
    return out


def set_tilesize_case(r):
    size = r.choice([64, 32, 16, 64, 32, 16, 0, 8, 128, 0x140, 0x120, -64, 48, 255, 256 + 32]) \
        if r.random() < 0.85 else rint(r)
    land = rlandscape(r)
    return dict(land=land, tilebuf=rtilebuf(r), size=size)


def precalc_case(r):
    head = rhead(r)
    for i in range(2, 12):
        if r.random() < 0.3:
            head[i] = r.choice([0, 1, 127, 128, 254, 255])
    return dict(land=dict(head=head, control=(0, 0, 0, None, None),
                          tilesets=[dict(size=0, level=0, bms=[(0, 0, 0, None, None)] * NBM)] * 3),
                ramps={n: r.randbytes(256) for n in RAMPS})


def precalc_edges():
    out = []
    for centre, width in ((0, 0), (255, 255), (128, 0), (128, 255), (0, 255), (255, 0), (100, 1), (100, 2), (1, 3)):
        head = [64, 0] + [centre, width] * 5 + [0, 0]
        out.append(dict(land=dict(head=head, control=(0, 0, 0, None, None),
                                  tilesets=[dict(size=0, level=0, bms=[(0, 0, 0, None, None)] * NBM)] * 3),
                        ramps={n: bytes(256) for n in RAMPS}))
    return out

# ----------------------------------------------------------------- generate_tile


def ramp_table(r):
    k = r.random()
    if k < 0.12:
        return bytes(256)
    if k < 0.24:
        return b"\xff" * 256
    if k < 0.36:
        return r.randbytes(256)
    lo = r.randint(0, 200)
    hi = r.randint(lo, 255)
    return bytes(0 if i < lo else 255 if i > hi else (i - lo) * 255 // (hi - lo + 1) for i in range(256))


def runs_map(r, w, h):
    """1-byte map with long runs of 0 / 255 (so whole 16-pixel runs agree) and noise."""
    out = bytearray()
    for _ in range(h):
        row = bytearray()
        while len(row) < w:
            k = r.random()
            n = r.choice([16, 32, 48, 96])
            if k < 0.4:
                row += bytes(n)
            elif k < 0.8:
                row += b"\xff" * n
            else:
                row += r.randbytes(n)
        out += row[:w]
    return bytes(out)


def gentile_case(r, size=None):
    s = size if size is not None else r.choice([16, 32, 64, 16, 32, 64, 16, 32, 8, 48])
    t = r.randint(0, 2)
    wt, ht = r.randint(1, 3), r.randint(1, 3)
    cw, ch = 4 * wt + 1 + r.randint(0, 2), 4 * ht + 1 + r.randint(0, 2)
    tx, ty = r.randint(0, wt - 1), r.randint(0, ht - 1)
    ctl = bytearray()
    full = r.random() < 0.85
    for _ in range(cw * ch):
        a = 0xff if full or r.random() < 0.7 else r.getrandbits(8)
        px = [a]
        for _ in range(3):
            k = r.random()
            px.append(255 if k < 0.45 else 0 if k < 0.6 else r.getrandbits(8))
        ctl += bytes(px)
    bms = []
    for k in range(NBM):
        w, h = s + r.randint(1, 24), s + r.randint(1, 24)
        if k in TEX4:
            bms.append((w, h, 4, "tex%d" % k, r.randbytes(w * h * 4)))
        elif k in (2, 5, 10):
            bms.append((w, h, 1, "mask%d" % k, r.randbytes(w * h)))
        else:
            bms.append((w, h, 1, "cover%d" % k, runs_map(r, w, h)))
    empty = dict(size=0, level=0, bms=[(0, 0, 0, None, None)] * NBM)
    tilesets = [empty, empty, empty]
    tilesets[t] = dict(size=s, level=t, bms=bms)
    head = [s, t] + [r.getrandbits(8) for _ in range(10)] + [(cw - 1) >> 2, (ch - 1) >> 2]
    land = dict(head=head, control=(cw, ch, 4, "control", bytes(ctl)), tilesets=tilesets)
    controls = {}
    for n in CONTROLS:
        if r.random() < 0.6:
            controls[n] = (s, s, 1, n, r.randbytes(s * s))
        elif r.random() < 0.5:
            controls[n] = (0, 0, 0, None, None)
        else:
            w = r.choice([16, 32, 64, 8])
            controls[n] = (w, w, 1, n, r.randbytes(w * w))
    k = r.random()
    if k < 0.4:
        ov = None
    elif k < 0.45:
        ov = dict(fmt=2, w=s, h=s, flags=0, data=None)
    else:
        fmt = r.choice([2, 4, 2, 4, 3])
        w, h = s * r.randint(1, 3) + r.choice([0, 0, 0, 5]), s * r.randint(1, 3) + r.choice([0, 0, 0, 7])
        flags = 0
        for bit in (8, 0x10, 0x20, 0x40):
            if r.random() < 0.35:
                flags |= bit
        if r.random() < 0.2:
            flags |= r.getrandbits(32) & ~0x78
        nvals = w * h
        vals = [r.getrandbits(16 if fmt == 2 else 32) for _ in range(nvals)]
        ov = dict(fmt=fmt, w=w, h=h, flags=flags, data=vals)
    tb = ("tilebuf", r.randbytes(s * s * 4)) if r.random() < 0.97 else (None, None)
    return dict(s=s, land=land, tx=tx, ty=ty, controls=controls,
                ramps={n: ramp_table(r) for n in RAMPS}, ov=ov, tilebuf=tb, out=r.randbytes(s * s * 4))


def gentile_edges():
    import random
    out = []
    r = random.Random(77)
    for s in (16, 32, 64):
        for flags in (0, 8, 0x10, 0x20, 0x40, 0x48, 0x50, 0x60):
            c = gentile_case(r, s)
            c["ov"] = dict(fmt=4, w=s, h=s, flags=flags, data=[r.getrandbits(32) for _ in range(s * s)])
            c["tilebuf"] = ("tilebuf", r.randbytes(s * s * 4))
            out.append(c)
    return out


def ov_bytes(side, ov):
    fmt = "%dH" % len(ov["data"]) if ov["fmt"] == 2 else "%dI" % len(ov["data"])
    return side.pack(fmt, *ov["data"]) if ov["fmt"] in (2, 4) else side.pack(fmt.replace("I", "H"), *[v & 0xffff for v in ov["data"]])


def gentile_setup(s, c):
    for n in RAMPS:
        s.write(s.ramp(n), c["ramps"][n])
    for n in CONTROLS:
        s.set_bitmap(s.glob_bitmap(n), c["controls"][n])
    s.set_tilebuf(*c["tilebuf"])


def gentile_result(s, out_addr, c, log):
    n = c["s"] * c["s"] * 4
    return dict(out=s.read(out_addr, n), allocs=log, tilebuf=s.get_tilebuf(),
                controls={k: s.get_bitmap(s.glob_bitmap(k)) for k in CONTROLS})


def gentile_ppc(emu, c):
    s = PpcSide(emu)
    L = ppc_landscape(s, c["land"])
    gentile_setup(s, c)
    out = s.buf("out", c["out"])
    ov = 0
    if c["ov"] is not None:
        data = s.buf("ovdata", ov_bytes(s, c["ov"])) if c["ov"]["data"] is not None else 0
        ov = s.buf("ov", struct.pack(">IiiiI", data, c["ov"]["w"], c["ov"]["h"], c["ov"]["fmt"], c["ov"]["flags"]))
    s.begin()
    s.call("generate_tile__9LandscapeFPUcUlUlP12sTileOverlay", L, out, c["tx"], c["ty"], ov)
    return gentile_result(s, out, c, s.finish())


def gentile_host(lib, c):
    s = HostSide(lib)
    L = host_landscape(s, c["land"])
    gentile_setup(s, c)
    outbuf = ctypes.create_string_buffer(c["out"], len(c["out"]))
    ov = c["ov"]
    data = None
    if ov is not None and ov["data"] is not None:
        data = ctypes.create_string_buffer(ov_bytes(s, ov))
    s.begin()
    F(lib, "tpw_Landscape_generate_tile", None, VP, VP, U32, U32, I32, VP, I32, I32, I32, U32)(
        L, outbuf, c["tx"], c["ty"], 0 if ov is None else 1, data,
        0 if ov is None else ov["w"], 0 if ov is None else ov["h"], 0 if ov is None else ov["fmt"],
        0 if ov is None else ov["flags"])
    res = gentile_result(s, ctypes.addressof(outbuf), c, s.finish())
    s.release()
    return res

# ----------------------------------------------------------------- load_landscape


def landscape_file(r, ts_sizes=None):
    data = bytes([3 if r.random() < 0.9 else r.choice([0, 2, 4, 0xff])])
    data += r.randbytes(10)
    cw, ch = r.randint(0, 9), r.randint(0, 9)
    data += file_bitmap(r, cw, ch, 4, truncate=0.0)
    for i in range(3):
        data += tileset_file(r)
    if r.random() < 0.08:
        data = data[:r.randint(0, len(data))]
    return data


def load_landscape_case(r):
    land = rlandscape(r)
    land["head"][1] = r.choice([0, 1, 2, 0, 1, 2, 3, 0xff])
    name = "levels/%s/land%d.lnd" % (r.choice(["jungle", "hallow", "fantasy", "space"]), r.randint(0, 99))
    return dict(land=land, name=name, file=landscape_file(r), fail=r.random() < 0.05,
                ramps={n: r.randbytes(256) for n in RAMPS})


def load_landscape_ppc(emu, c):
    s = PpcSide(emu)
    L = ppc_landscape(s, c["land"])
    for n in RAMPS:
        s.write(s.ramp(n), c["ramps"][n])
    name = s.buf("name", c["name"].encode() + b"\0")
    s.files(c["file"], c["fail"])
    s.begin()
    r3, _, _ = s.call("load_landscape__9LandscapeFPc", L, name)
    return dict(ret=T.s32(r3), allocs=s.finish(), land=get_landscape(s, L), file=s.file_log(),
                ramps={n: s.read(s.ramp(n), 256) for n in RAMPS})


def load_landscape_host(lib, c):
    s = HostSide(lib)
    L = host_landscape(s, c["land"])
    for n in RAMPS:
        s.write(s.ramp(n), c["ramps"][n])
    name = ctypes.create_string_buffer(c["name"].encode())
    s.files(c["file"], c["fail"])
    s.begin()
    ret = F(lib, "tpw_Landscape_load_landscape", I32, VP, ctypes.c_char_p)(L, name)
    out = dict(ret=ret, allocs=s.finish(), land=get_landscape(s, L), file=s.file_log(),
               ramps={n: s.read(s.ramp(n), 256) for n in RAMPS})
    put_landscape(s, L, dict(head=[0] * 14, control=(0, 0, 0, None, None),
                             tilesets=[dict(size=0, level=0, bms=[(0, 0, 0, None, None)] * NBM)] * 3))
    s.release()
    return out

# ----------------------------------------------------------------- sinit, SetFileCallbacks


def sinit_case(r):
    return dict(controls={n: rbm(r, n, null=0.5) for n in CONTROLS})


def sinit_ppc(emu, c):
    s = PpcSide(emu)
    for n in CONTROLS:
        s.set_bitmap(s.glob_bitmap(n), c["controls"][n])
    s.begin()
    s.call("__sinit_landscape_cpp")
    names = {v: k for k, v in CONTROLS.items()}
    dtor = emu.func("__dt__6BitmapFv")
    reg = [(names.get(obj, hex(obj)), emu.u32(fn) == dtor, rec - obj) for obj, fn, rec in emu.registered]
    return dict(allocs=s.finish(), controls={n: s.get_bitmap(s.glob_bitmap(n)) for n in CONTROLS}, registered=reg)


def sinit_host(lib, c):
    s = HostSide(lib)
    for n in CONTROLS:
        s.set_bitmap(s.glob_bitmap(n), c["controls"][n])
    s.begin()
    F(lib, "tpw_sinit_landscape", None)()
    # C++ registers the destructors itself; the expected record: object, ~Bitmap, and a
    # 12-byte destructor-chain record right after the 16-byte object.
    reg = [("gControl1", True, 16), ("gControl3", True, 16), ("gControl2", True, 16)]
    out = dict(allocs=s.finish(), controls={n: s.get_bitmap(s.glob_bitmap(n)) for n in CONTROLS}, registered=reg)
    s.release()
    return out


def setfcb_case(r):
    return dict(v=[r.getrandbits(32) for _ in range(6)])


def setfcb_ppc(emu, c):
    s = PpcSide(emu)
    s.call("SetFileCallbacks__FPFPCcPCc_PvPFPv_UlPFPvUlUlPv_UiPFPvUlUl_UlPFPv_lPFPCci_Pc", *c["v"])
    return dict(fns=[emu.u32(FILE_FNS[n]) for n in ("open", "close", "read", "seek", "tell", "name")])


def setfcb_host(lib, c):
    F(lib, "tpw_SetFileCallbacks", None, *[ctypes.c_size_t] * 6)(*c["v"])
    out = (ctypes.c_uint64 * 6)()
    F(lib, "tpw_get_file_callbacks", None, VP)(out)
    return dict(fns=list(out))

# ----------------------------------------------------------------- the tests


def _bm_method(name, label, args_ppc, host_name, host_args, ret=True):
    ppc, host = bitmap_fn(label, args_ppc,
                          lambda lib, b, c, h: F(lib, host_name, I32 if ret else None, VP, *[a[0] for a in host_args])(
                              b, *[a[1](c, h) for a in host_args]), ret=ret)
    return ppc, host


_clear_ppc, _clear_host = _bm_method("clear", "clear__6BitmapFv", lambda c, h: [], "tpw_Bitmap_clear", [], ret=False)
_realloc_ppc, _realloc_host = _bm_method(
    "realloc", "realloc__6BitmapFUiUiUi", lambda c, h: [c["w"], c["h"], c["bpp"]], "tpw_Bitmap_realloc",
    [(U32, lambda c, h: c["w"]), (U32, lambda c, h: c["h"]), (U32, lambda c, h: c["bpp"])])
_load_ppc, _load_host = _bm_method("load", "load__6BitmapFPv", lambda c, h: [h], "tpw_Bitmap_load",
                                   [(VP, lambda c, h: h)])

_ts_dtor = tileset_fn("__dt__7TilesetFv", lambda c, h: [T.u32(T.s16(c["flag"]))],
                      lambda lib, t, c, h: F(lib, "tpw_Tileset_dtor", I32, VP, I16)(t, T.s16(c["flag"])), flag=True)
_ts_clear = tileset_fn("clear__7TilesetFv", host_fn=lambda lib, t, c, h: F(lib, "tpw_Tileset_clear", None, VP)(t))
_ts_load = tileset_fn("load__7TilesetFPv", lambda c, h: [h],
                      lambda lib, t, c, h: F(lib, "tpw_Tileset_load", I32, VP, VP)(t, h), ret=True)

_land_dtor = landscape_fn("__dt__9LandscapeFv", lambda c: [T.u32(T.s16(c["flag"]))],
                          lambda lib, L, c: F(lib, "tpw_Landscape_dtor", I32, VP, I16)(L, T.s16(c["flag"])), flag=True)
_land_clear = landscape_fn("clear__9LandscapeFv", host_fn=lambda lib, L, c: F(lib, "tpw_Landscape_clear", None, VP)(L))
_land_tilesize = landscape_fn("set_tilesize__9LandscapeFi", lambda c: [I(c["size"])],
                              lambda lib, L, c: F(lib, "tpw_Landscape_set_tilesize", None, VP, I32)(L, T.s32(c["size"])),
                              contents=False)
_land_precalc = landscape_fn("precalc_tables__9LandscapeFv",
                             host_fn=lambda lib, L, c: F(lib, "tpw_Landscape_precalc_tables", None, VP)(L),
                             tilebuf=False, ramps=True)


def _with(fn, **extra):
    """Case generator wrapper adding fields."""
    def g(r):
        c = fn(r)
        for k, v in extra.items():
            c[k] = v(r)
        return c
    return g


TESTS = [
    FnTest("CPU::detect", cpu_detect_ppc, cpu_detect_host, edge=cpu_edges, rnd=cpu_case),
    FnTest("CPU::clocks_per_millisec", cpm_ppc, cpm_host,
           edge=lambda: [dict(clock=v, arg=1000, junk=bytes(32)) for v in (0, 1, 266, 2147483, 2147484, -1, 0x7fffffff)],
           rnd=cpm_case),
    FnTest("resample_control", resample_ppc, resample_host, edge=resample_edges, rnd=resample_case),
    kernel_test("blend_3"),
    kernel_test("blend_2"),
    kernel_test("single_layer"),
    kernel_test("calc_cover"),
    FnTest("Tileset::Tileset", tileset_ctor_ppc, tileset_ctor_host,
           edge=lambda: [dict(junk=b"\xff" * 16), dict(junk=bytes(16))], rnd=lambda r: dict(junk=r.randbytes(16))),
    FnTest("Bitmap::~Bitmap", bitmap_dtor_ppc, bitmap_dtor_host,
           edge=lambda: [dict(bm=(2, 2, 1, "data", bytes(4)), flag=f) for f in FLAGS] + [dict(bm=(0, 0, 0, None, None), flag=1)],
           rnd=_with(lambda r: dict(bm=bitmap_state_case(r)), flag=lambda r: r.choice(FLAGS))),
    FnTest("Tileset::~Tileset", *_ts_dtor,
           edge=lambda: [dict(ts=dict(size=64, level=0, bms=[(1, 1, 1, "b%d" % k, b"\x05") for k in range(NBM)]), flag=f)
                         for f in FLAGS],
           rnd=lambda r: dict(ts=rtileset(r, "ts"), flag=r.choice(FLAGS))),
    FnTest("Tileset::clear", *_ts_clear, rnd=lambda r: dict(ts=rtileset(r, "ts"))),
    FnTest("Tileset::load", *_ts_load, rnd=tileset_load_case),
    FnTest("Landscape::Landscape", landscape_ctor_ppc, landscape_ctor_host,
           edge=lambda: [dict(junk=b"\xff" * 16), dict(junk=bytes(16))], rnd=lambda r: dict(junk=r.randbytes(16))),
    FnTest("Landscape::~Landscape", *_land_dtor, edge=land_edges, rnd=land_case),
    FnTest("Landscape::clear", *_land_clear, edge=land_edges, rnd=land_case),
    FnTest("Landscape::set_tilesize", *_land_tilesize,
           edge=lambda: [dict(e, size=sz) for e in land_edges() for sz in (64, 32, 16, 0, 0x140)],
           rnd=set_tilesize_case),
    FnTest("Landscape::precalc_tables", *_land_precalc, edge=precalc_edges, rnd=precalc_case),
    FnTest("Landscape::generate_tile", gentile_ppc, gentile_host, edge=gentile_edges, rnd=gentile_case),
    FnTest("Landscape::load_landscape", load_landscape_ppc, load_landscape_host, rnd=load_landscape_case),
    FnTest("__sinit_landscape_cpp", sinit_ppc, sinit_host, rnd=sinit_case),
    FnTest("Bitmap::clear", _clear_ppc, _clear_host, rnd=lambda r: dict(bm=bitmap_state_case(r))),
    FnTest("Bitmap::realloc", _realloc_ppc, _realloc_host, edge=realloc_edges, rnd=realloc_case),
    FnTest("Bitmap::load", _load_ppc, _load_host, edge=load_edges, rnd=load_case),
    FnTest("SetFileCallbacks", setfcb_ppc, setfcb_host, rnd=setfcb_case),
]


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    emu.heap_in_scratch = True
    T.install_ppc_file_callbacks(emu, FILE_FNS)
    emu.file_tvecs = {n: emu.u32(FILE_FNS[n]) for n in ("open", "close", "read", "seek")}
