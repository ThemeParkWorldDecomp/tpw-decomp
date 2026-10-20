"""Differential tests for engine tb_capture.cpp, plus the tb_error.cpp pieces it calls
(CErrorHandler::Push and Pop, SError, the static initialiser) and the Wavelet.cpp engine
callbacks (DefaultMalloc, DefaultFree, SetEngineCallbacks). See difftest.py, tpwtest.py.

Byte order: tb_capture is PC code that stores 16- and 32-bit values into byte buffers
(the TGA header's width and height, the words ConvertCapture stores three bytes apart).
The translation keeps those stores native, so the cases compare the little-endian image
of the PowerPC's values: 16-bit pixels are given to each side as values (each side packs
them in its own order), the PowerPC header fields are byte-swapped before the compare,
and ConvertCapture's 16-bit output is rebuilt from a trace of the original's word stores
(the stwx at 0x10003050). 24- and 32-bit pixels are bytes and compare as they are.

Stubs: tpwtest.install_std_stubs (memset, memcpy, strcpy, __construct_array, DebugStr);
the engine allocator callbacks (gMalloc/gFree) are fakes on both sides (new blocks filled
with 0xcd; free records the block and clears the caller's pointer as the game's MacFree
does); IDirectDrawSurface4::Lock and IDirectDrawSurface::Unlock hand out a test surface
and record their arguments (tests/shim_placeholders.cpp on the host); fopen/fwrite/fclose
are a fake file on the PowerPC side and the real C library on the host (files under
TPW_TMP or the system temporary directory).
"""
import atexit
import ctypes
import os
import random
import shutil
import struct
import tempfile

from unicorn import UC_HOOK_CODE

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"

DISPLAY, CAPTURE, DD_PTR, DD_RESULT, ERRORS = 0x10165634, 0x10052a40, 0x10052ad0, 0x10052b40, 0x10052b44
MALLOC_FN, FREE_FN = 0x10051f70, 0x10051f74
CALLBACKS = [0x101735cc, 0x101735c4, 0x101735c8, 0x101735c0, MALLOC_FN, FREE_FN]   # SetEngineCallbacks order
STORE_PC = 0x10003050           # ConvertCapture: stwx r0, r3, r8 (one 16-bit pixel's word)
FAKE_SURFACE, FAKE_FILE = 0x7C0FE000, 0x7C0FD000
LOCK = ("Lock__19IDirectDrawSurface4FPQ313SamsUtilities10URectangle8LongRectP15_DDSURFACEDESC2Ul"
        "PQ210NS_MacDoze14InternalHandle")
UNLOCK = "Unlock__18IDirectDrawSurfaceFPv"
PATTERN = 0xCD

VP, U32, I32 = ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int32
TMP = tempfile.mkdtemp(prefix="tpw_capture_", dir=os.environ.get("TPW_TMP"))
atexit.register(shutil.rmtree, TMP, True)


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f

# ----------------------------------------------------------------- case model
# disp: (width, height, bpp, [alpha, red, green, blue] as (mask, shift, bits))
# blocks: label -> ("u16", [values]) or ("bytes", data); cap: (screen label, rgb label,
# pixels, name bytes[0x80], pending offset or -1); errors: (count, [(level, code)] * 64)


FORMATS = {   # bpp -> channel patterns (alpha, red, green, blue)
    16: [[(0, 0, 0), (0xF800, 11, 5), (0x07E0, 5, 6), (0x001F, 0, 5)],
         [(0x8000, 15, 1), (0x7C00, 10, 5), (0x03E0, 5, 5), (0x001F, 0, 5)],
         [(0xF000, 12, 4), (0x0F00, 8, 4), (0x00F0, 4, 4), (0x000F, 0, 4)]],
    24: [[(0, 0, 0), (0xFF0000, 16, 8), (0x00FF00, 8, 8), (0x0000FF, 0, 8)]],
    32: [[(0xFF000000, 24, 8), (0xFF0000, 16, 8), (0x00FF00, 8, 8), (0x0000FF, 0, 8)]],
}


def rbits(r):
    """Odd channel patterns too: shifts that go negative or past 31 (slw gives 0)."""
    out = []
    for _ in range(4):
        k = r.random()
        if k < 0.5:
            out.append([r.getrandbits(16), r.randint(0, 16), r.randint(0, 16)])
        else:
            out.append([r.getrandbits(32), r.randint(-40, 40), r.randint(-40, 40)])
    return out


def rdisp(r, bpp=None, w=None, h=None):
    bpp = bpp if bpp is not None else r.choice([16, 16, 24, 32, 32, 8, 15, 0, 40, 64])
    w = w if w is not None else r.choice([0, 1, 2, 3, 5, 8, 13, 16, 31])
    h = h if h is not None else r.choice([0, 1, 2, 3, 4, 7, 12])
    if bpp in FORMATS and r.random() < 0.7:
        bits = [list(c) for c in r.choice(FORMATS[bpp])]
    else:
        bits = rbits(r)
    return (w, h, bpp, bits)


def rerrors(r):
    count = r.choice([0, 0, 1, 2, 5, 63, 64, 65, 100]) if r.random() < 0.8 else r.randint(0, 64)
    return (count, [(T.s32(r.getrandbits(32)), T.s32(r.getrandbits(32))) for _ in range(64)])


def pixel_block(r, n, bpp, label):
    if bpp == 16:
        return {label: ("u16", [r.getrandbits(16) for _ in range(n)])}
    return {label: ("bytes", r.randbytes(n * max(bpp, 8) // 8))}

# ----------------------------------------------------------------- the two sides


def bit_words(bits):
    """The four SBitData as 12 words: mask unsigned, shift and bits signed."""
    return [T.u32(x) if i % 3 == 0 else T.s32(x) for i, x in enumerate(v for c in bits for v in c)]


def le16(values):
    return struct.pack("<%dH" % len(values), *values)


class Side:
    """Shared by both sides: block labels, the normalised view of buffers."""

    def __init__(self):
        self.labels, self.sizes, self.kinds = {}, {}, {}

    def label(self, a):
        return None if not a else self.labels.get(a, "?%x" % a)


class PpcSide(Side):
    def __init__(self, emu):
        super().__init__()
        self.emu = emu
        emu.cap_log, emu.cap_trace, emu.cap_mallocs, emu.cap_fail_at = [], [], 0, -1
        emu.cap_lock_log, emu.cap_unlock_log, emu.cap_file = [], [], None
        emu.debugstrs = []

    def block(self, label, kind, data):
        raw = struct.pack(">%dH" % len(data), *data) if kind == "u16" else bytes(data)
        a = self.emu.alloc(max(len(raw), 4), raw)
        self.labels[a], self.sizes[a], self.kinds[a] = label, len(raw), kind
        return a

    def contents(self, a):
        """Little-endian image of a block (u16 blocks repacked; bytes as they are)."""
        if not a or a not in self.sizes:
            return None
        raw = self.emu.read(a, self.sizes[a])
        if self.kinds[a] == "u16":
            return le16(struct.unpack(">%dH" % (len(raw) // 2), raw[:len(raw) // 2 * 2])) + raw[len(raw) // 2 * 2:]
        if self.kinds[a] == "rgb16":
            # rebuild from the traced word stores: the Mac image must match the trace, and
            # the little-endian image is what a little-endian host stores
            be, le = bytearray([PATTERN]) * self.sizes[a], bytearray([PATTERN]) * self.sizes[a]
            for addr, word in self.emu.cap_trace:
                off = addr - a
                if 0 <= off <= self.sizes[a] - 4:
                    be[off:off + 4] = struct.pack(">I", word)
                    le[off:off + 4] = struct.pack("<I", word)
            if bytes(be) != raw:
                return ("trace does not explain the Mac bytes", raw.hex())
            return bytes(le)
        return raw

    def set_display(self, disp):
        w, h, bpp, bits = disp
        e = self.emu
        e.write(DISPLAY + 8, struct.pack(">ii", w, h))
        e.write(DISPLAY + 0x24, struct.pack(">i", bpp))
        e.write(DISPLAY + 0x88, struct.pack(">" + "Iii" * 4, *bit_words(bits)))

    def set_errors(self, a, errs):
        count, entries = errs
        self.emu.write(a, struct.pack(">128ii", *sum(map(list, entries), []), count))

    def get_errors(self, a):
        v = struct.unpack(">128ii", self.emu.read(a, 0x204))
        return (v[128], [tuple(v[2 * i:2 * i + 2]) for i in range(64)])

    def capture_obj(self, cap, blocks, at=None):
        screen, rgb, pixels, name, pending = cap
        addrs = {lab: self.block(lab, *blocks[lab]) for lab in blocks}
        obj = at if at is not None else self.emu.alloc(0x90)
        p = 0 if pending < 0 else obj + 0xc + pending
        self.emu.write(obj, struct.pack(">IIi", addrs.get(screen, 0), addrs.get(rgb, 0), pixels) + name +
                       struct.pack(">I", p))
        return obj

    def get_capture(self, obj):
        screen, rgb, pixels = struct.unpack(">IIi", self.emu.read(obj, 12))
        name = self.emu.read(obj + 0xc, 0x80)
        p = self.emu.u32(obj + 0x8c)
        pending = -1 if p == 0 else (p - obj - 0xc if obj + 0xc <= p < obj + 0x8c else "other")
        return dict(screen=self.label(screen), rgb=self.label(rgb), pixels=pixels, name=name, pending=pending,
                    screen_data=self.contents(screen), rgb_data=self.contents(rgb))

    def allocs(self):
        out = []
        for kind, size, a in self.emu.cap_log:
            out.append((kind, size, self.label(a)) if kind == "malloc" else (kind, self.label(a)))
        return out

    def call(self, label, *args):
        return T.call_ppc(self.emu, self.emu.func(label), list(args))[0]


class HostSide(Side):
    def __init__(self, lib):
        super().__init__()
        self.lib = lib
        self.keep = []
        F(lib, "tpw_cap_reset", None)()
        F(lib, "tpw_cap_install", None)()
        I32.in_dll(lib, "tpw_debugstr_count").value = 0
        for n in ("tpw_lock_calls", "tpw_unlock_calls"):
            I32.in_dll(lib, n).value = 0

    def addr(self, sym):
        return ctypes.addressof(ctypes.c_char.in_dll(self.lib, sym))

    def block(self, label, kind, data):
        raw = le16(data) if kind == "u16" else bytes(data)
        b = ctypes.create_string_buffer(raw, max(len(raw), 4))
        self.keep.append(b)
        a = ctypes.addressof(b)
        self.labels[a], self.sizes[a], self.kinds[a] = label, len(raw), kind
        return a

    def contents(self, a):
        if not a or a not in self.sizes:
            return None
        return ctypes.string_at(a, self.sizes[a])

    def set_display(self, disp):
        w, h, bpp, bits = disp
        d = self.addr("gei_Display")
        ctypes.memmove(d + 8, struct.pack("<ii", w, h), 8)
        ctypes.memmove(d + 0x24, struct.pack("<i", bpp), 4)
        ctypes.memmove(d + 0x88, struct.pack("<" + "Iii" * 4, *bit_words(bits)), 48)

    def set_errors(self, a, errs):
        count, entries = errs
        ctypes.memmove(a, struct.pack("<128ii", *sum(map(list, entries), []), count), 0x204)

    def get_errors(self, a):
        v = struct.unpack("<128ii", ctypes.string_at(a, 0x204))
        return (v[128], [tuple(v[2 * i:2 * i + 2]) for i in range(64)])

    def capture_obj(self, cap, blocks, obj=None):
        screen, rgb, pixels, name, pending = cap
        addrs = {lab: self.block(lab, *blocks[lab]) for lab in blocks}
        obj = obj if obj is not None else F(self.lib, "tpw_capture_store", VP)()
        F(self.lib, "tpw_capture_set", None, VP, VP, VP, I32, ctypes.c_char_p, I32)(
            obj, addrs.get(screen), addrs.get(rgb), pixels, name, pending)
        return obj

    def get_capture(self, obj):
        screen, rgb, pixels, pending = VP(), VP(), I32(), I32()
        name = ctypes.create_string_buffer(0x80)
        F(self.lib, "tpw_capture_get", None, VP, ctypes.POINTER(VP), ctypes.POINTER(VP), ctypes.POINTER(I32),
          ctypes.c_char_p, ctypes.POINTER(I32))(obj, ctypes.byref(screen), ctypes.byref(rgb), ctypes.byref(pixels),
                                                name, ctypes.byref(pending))
        return dict(screen=self.label(screen.value), rgb=self.label(rgb.value), pixels=pixels.value, name=name.raw,
                    pending=pending.value, screen_data=self.contents(screen.value), rgb_data=self.contents(rgb.value))

    def allocs(self):
        n = I32.in_dll(self.lib, "tpw_cap_log_n").value
        kinds = (I32 * 64).in_dll(self.lib, "tpw_cap_log_kind")
        sizes = (ctypes.c_uint64 * 64).in_dll(self.lib, "tpw_cap_log_size")
        ptrs = (VP * 64).in_dll(self.lib, "tpw_cap_log_ptr")
        out, k = [], 0
        for i in range(n):
            if kinds[i] == 1:
                if ptrs[i]:
                    self.labels[ptrs[i]], self.sizes[ptrs[i]], self.kinds[ptrs[i]] = "m%d" % k, sizes[i], "bytes"
                    k += 1
                out.append(("malloc", sizes[i], self.label(ptrs[i])))
            else:
                out.append(("free", self.label(ptrs[i])))
        return out

    def release(self):
        F(self.lib, "tpw_cap_release", None)()


def ppc_malloc(e):
    size = e.r(3)
    idx = e.cap_mallocs
    e.cap_mallocs += 1
    if idx == e.cap_fail_at:
        e.cap_log.append(("malloc", size, 0))
        e.set_r(3, 0)
        return
    a = e.alloc(max(size, 4), bytes([PATTERN]) * size if size else None)
    e.cap_log.append(("malloc", size, a))
    e.cap_new.append((a, size))
    e.set_r(3, a)


def ppc_free(e):
    ref = e.r(3)
    e.cap_log.append(("free", 0, e.u32(ref)))
    e.write(ref, b"\0\0\0\0")


def label_new_blocks(s, kinds):
    """Name the PowerPC side's malloc blocks m0, m1, ... in order (kinds: label -> kind)."""
    for k, (a, size) in enumerate(s.emu.cap_new):
        lab = "m%d" % k
        s.labels[a], s.sizes[a], s.kinds[a] = lab, size, kinds.get(lab, "bytes")


def lock_ppc(e):
    this, rect, desc, flags, event = (e.r(i) for i in range(3, 8))
    e.cap_lock_log.append((this == FAKE_SURFACE, rect, flags, event, e.u32(desc) == 0x7c))
    if e.cap_lock[0] == 0:
        e.write(desc + 0x24, struct.pack(">I", e.cap_surface))
        e.write(desc + 0x10, struct.pack(">i", e.cap_lock[1]))
    e.set_r(3, T.u32(e.cap_lock[0]))


def unlock_ppc(e):
    e.cap_unlock_log.append((e.r(3) == FAKE_SURFACE, e.r(4)))
    e.set_r(3, T.u32(e.cap_unlock))


def fopen_ppc(e):
    e.cap_file = dict(name=e.cstr(e.r(3)), mode=e.cstr(e.r(4)), data=b"", closed=False)
    if e.cap_fopen_fail:
        e.cap_file = None
        e.set_r(3, 0)
    else:
        e.set_r(3, FAKE_FILE)


def fwrite_ppc(e):
    ptr, size, count, f = (e.r(i) for i in range(3, 7))
    n = (size * count) & 0xFFFFFFFF
    assert f == FAKE_FILE and n < 1 << 24
    e.cap_file["data"] += e.read(ptr, n) if n else b""
    e.set_r(3, count)


def fclose_ppc(e):
    assert e.r(3) == FAKE_FILE
    e.cap_file["closed"] = True
    e.set_r(3, 0)


def trace_hook(uc, address, size, emu):
    emu.cap_trace.append(((emu.r(3) + emu.r(8)) & 0xFFFFFFFF, emu.r(0)))


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    emu.write(MALLOC_FN, struct.pack(">I", T.fake_function(emu, ppc_malloc)))
    emu.write(FREE_FN, struct.pack(">I", T.fake_function(emu, ppc_free)))
    emu.cap_malloc_tv, emu.cap_free_tv = emu.u32(MALLOC_FN), emu.u32(FREE_FN)
    emu.stub(LOCK, lock_ppc)
    emu.stub(UNLOCK, unlock_ppc)
    emu.stub("fopen", fopen_ppc)
    emu.stub("fwrite", fwrite_ppc)
    emu.stub("fclose", fclose_ppc)
    emu.uc.hook_add(UC_HOOK_CODE, trace_hook, emu, begin=STORE_PC, end=STORE_PC)

# ----------------------------------------------------------------- environment per case


def ppc_env(s, c):
    """Display, DirectDraw stand-ins, allocator fakes, error stack, file, results."""
    e = s.emu
    e.cap_new, e.cap_trace = [], []
    e.write(MALLOC_FN, struct.pack(">I", e.cap_malloc_tv))
    e.write(FREE_FN, struct.pack(">I", e.cap_free_tv))
    e.cap_fail_at = c.get("fail_at", -1)
    s.set_display(c["disp"])
    s.set_errors(ERRORS, c["errors"])
    dd = e.alloc(0x20)
    e.write(dd + 0x14, struct.pack(">I", FAKE_SURFACE))
    e.write(DD_PTR, struct.pack(">I", dd))
    e.write(DD_RESULT, struct.pack(">i", c.get("ddresult", 0x1234)))
    if "surface" in c:
        kind, data, pitch = c["surface"]
        e.cap_surface = s.block("surface", kind, data)
        e.cap_lock = (c["lock"], pitch)
        e.cap_unlock = c["unlock"]
    e.cap_fopen_fail = c.get("fopen_fail", False)


def host_env(s, c):
    lib = s.lib
    I32.in_dll(lib, "tpw_cap_malloc_fail_at").value = c.get("fail_at", -1)
    s.set_display(c["disp"])
    s.set_errors(s.addr("gei_ErrorHandler"), c["errors"])
    F(lib, "tpw_ddraw_install", None, I32)(1)
    I32.in_dll(lib, "gDDResult").value = c.get("ddresult", 0x1234)
    if "surface" in c:
        kind, data, pitch = c["surface"]
        VP.in_dll(lib, "tpw_lock_surface").value = s.block("surface", kind, data)
        I32.in_dll(lib, "tpw_lock_pitch").value = pitch
        I32.in_dll(lib, "tpw_lock_result").value = c["lock"]
        I32.in_dll(lib, "tpw_unlock_result").value = c["unlock"]


def ppc_after(s, obj=None, ret=None):
    e = s.emu
    out = dict(errors=s.get_errors(ERRORS), allocs=s.allocs(), ddresult=e.s32(DD_RESULT),
               lock=list(e.cap_lock_log), unlock=list(e.cap_unlock_log))
    if obj is not None:
        out["cap"] = s.get_capture(obj)
    if ret is not None:
        out["ret"] = T.s32(ret)
    return out


def host_after(s, obj=None, ret=None):
    lib = s.lib
    allocs = s.allocs()            # names the new blocks first
    surf = F(lib, "tpw_fake_surface", VP)()
    lock = []
    if I32.in_dll(lib, "tpw_lock_calls").value:
        lock = [(VP.in_dll(lib, "tpw_lock_this").value == surf, VP.in_dll(lib, "tpw_lock_rect").value or 0,
                 U32.in_dll(lib, "tpw_lock_flags").value, VP.in_dll(lib, "tpw_lock_event").value or 0,
                 U32.in_dll(lib, "tpw_lock_dwsize").value == F(lib, "tpw_ddsurfacedesc2_size", U32)())]
    unlock = []
    if I32.in_dll(lib, "tpw_unlock_calls").value:
        unlock = [(VP.in_dll(lib, "tpw_unlock_this").value == surf, VP.in_dll(lib, "tpw_unlock_rect").value or 0)]
    out = dict(errors=s.get_errors(s.addr("gei_ErrorHandler")), allocs=allocs,
               ddresult=I32.in_dll(lib, "gDDResult").value, lock=lock, unlock=unlock)
    if obj is not None:
        out["cap"] = s.get_capture(obj)
    if ret is not None:
        out["ret"] = T.s32(ret)
    return out

# ----------------------------------------------------------------- TGA files


def tga_header_le(data):
    """The PowerPC header's width and height (stored big-endian there) in little-endian."""
    if len(data) < 18:
        return data
    w, h = struct.unpack(">hh", data[12:16])
    return data[:12] + struct.pack("<hh", w, h) + data[16:]


def tga_rows(rgb, w, h):
    return b"".join(rgb[w * (h - 1 - y) * 3:w * (h - 1 - y) * 3 + 3 * w] for y in range(h))


def ppc_file(s, w, h, rgb_label=None):
    f = s.emu.cap_file
    if f is None:
        return None
    data = f["data"]
    if rgb_label is not None and len(data) >= 18:
        # 16-bit captures: the rows are the converted buffer, rebuilt in little-endian
        a = next(a for a, lab in s.labels.items() if lab == rgb_label)
        raw = s.emu.read(a, s.sizes[a])
        if data[18:] != tga_rows(raw, w, h):
            return ("rows are not the converted buffer", data.hex())
        data = data[:18] + tga_rows(s.contents(a), w, h)
    return dict(name=f["name"], closed=f["closed"], data=tga_header_le(data))


JUNK = b"\xee" * 4096


def file_name(c):
    """The name both sides get; the host runs inside TMP, where it is a real file."""
    return ("missing-directory/" if c.get("fopen_fail") else "") + c["fname"]


def host_prepare(c):
    if not c.get("fopen_fail"):
        with open(os.path.join(TMP, c["fname"]), "wb") as fh:    # "w+b" must truncate this
            fh.write(JUNK)


def host_file(c):
    """The file as the host left it, or None when it was never opened for writing."""
    path = os.path.join(TMP, file_name(c))
    if not os.path.exists(path):
        return None
    with open(path, "rb") as fh:
        data = fh.read()
    return None if data == JUNK else dict(name=file_name(c), closed=True, data=data)


def in_tmp(fn):
    old = os.getcwd()
    os.chdir(TMP)
    try:
        return fn()
    finally:
        os.chdir(old)

# ----------------------------------------------------------------- the capture methods


def rname(r, n=None):
    n = n if n is not None else r.choice([0, 1, 8, 12, 30, 0x7f])
    return bytes(r.choice(b"abcdefghijklmnopqrstuvwxyz0123456789_.") for _ in range(n))


def rcap(r, labels=(None,), pixels=None):
    return (r.choice(labels), r.choice(labels), pixels if pixels is not None else T.rint(r),
            r.randbytes(0x80), r.choice([-1, -1, 0, 5, 0x7f]))


def setcapture_case(r):
    return dict(cap=rcap(r), name=None if r.random() < 0.15 else rname(r))


def setcapture_ppc(emu, c):
    s = PpcSide(emu)
    obj = s.capture_obj(c["cap"], {})
    name = emu.alloc(0x80, c["name"] + b"\0") if c["name"] is not None else 0
    ret = s.call("SetCapture__8CCaptureFPc", obj, name)
    return dict(ret=T.s32(ret), cap=s.get_capture(obj))


def setcapture_host(lib, c):
    s = HostSide(lib)
    obj = s.capture_obj(c["cap"], {})
    name = ctypes.create_string_buffer(c["name"]) if c["name"] is not None else None
    ret = F(lib, "tpw_CCapture_SetCapture", I32, VP, ctypes.c_char_p)(obj, name)
    return dict(ret=ret, cap=s.get_capture(obj))


def surface_for(r, disp, pixel_kind=None):
    w, h, bpp, _ = disp
    bypp = bpp >> 3 if bpp >= 0 else 0
    pitch = w * bypp + r.choice([0, 0, 4, 6, 64])
    if bpp == 16:
        n = (pitch * h + 1) // 2
        return ("u16", [r.getrandbits(16) for _ in range(n)], pitch)
    return ("bytes", r.randbytes(max(pitch * h, 0)), pitch)


def getcapture_case(r):
    disp = rdisp(r, bpp=r.choice([16, 16, 24, 32, 32, 8, 0]))
    k = r.random()
    lock = 0 if k < 0.8 else r.choice([-1, 1, T.s32(0x88760096)])
    unlock = 0 if r.random() < 0.85 else r.choice([-1, 7])
    return dict(disp=disp, errors=rerrors(r), surface=surface_for(r, disp), lock=lock, unlock=unlock,
                fail_at=0 if r.random() < 0.08 else -1, cap=rcap(r, pixels=T.rint(r)),
                ddresult=T.rint(r))


def getcapture_ppc(emu, c):
    s = PpcSide(emu)
    ppc_env(s, c)
    obj = s.capture_obj(c["cap"], {})
    ret = s.call("GetCapture__8CCaptureFv", obj)
    label_new_blocks(s, {"m0": "u16" if c["disp"][2] == 16 else "bytes"})
    return ppc_after(s, obj, ret)


def getcapture_host(lib, c):
    s = HostSide(lib)
    host_env(s, c)
    obj = s.capture_obj(c["cap"], {})
    ret = F(lib, "tpw_CCapture_GetCapture", I32, VP)(obj)
    out = host_after(s, obj, ret)
    s.release()
    return out


def convert_case(r):
    disp = rdisp(r)
    w, h, bpp, _ = disp
    n = r.choice([0, 1, 2, 3, 7, 16, 50]) if r.random() < 0.6 else w * h
    blocks = pixel_block(r, n, bpp, "screen")
    if bpp == 16 and r.random() < 0.5:
        blocks["screen"] = ("u16", [r.choice([0, 0xFFFF, 0x8000, 0x0001, 0xF800, 0x07E0, 0x001F])
                                    for _ in range(n)])
    cap = ("screen", r.choice([None, "screen"]) if r.random() < 0.2 else None, n, r.randbytes(0x80), -1)
    return dict(disp=disp, errors=rerrors(r), blocks=blocks, cap=cap, fail_at=0 if r.random() < 0.1 else -1)


def convert_ppc(emu, c):
    s = PpcSide(emu)
    ppc_env(s, c)
    obj = s.capture_obj(c["cap"], c["blocks"])
    ret = s.call("ConvertCapture__8CCaptureFv", obj)
    label_new_blocks(s, {"m0": "rgb16"})
    return ppc_after(s, obj, ret)


def convert_host(lib, c):
    s = HostSide(lib)
    host_env(s, c)
    obj = s.capture_obj(c["cap"], c["blocks"])
    ret = F(lib, "tpw_CCapture_ConvertCapture", I32, VP)(obj)
    out = host_after(s, obj, ret)
    s.release()
    return out


def save_case(r):
    disp = rdisp(r, bpp=24)
    w, h = disp[0], disp[1]
    rgb = r.randbytes(3 * w * h + r.choice([0, 0, 1, 5]))
    return dict(disp=disp, errors=rerrors(r), blocks={"rgb": ("bytes", rgb)},
                cap=(None, "rgb", w * h, r.randbytes(0x80), -1), fname="shot%04d.tga" % r.randint(0, 9999),
                fopen_fail=r.random() < 0.1)


def save_ppc(emu, c):
    s = PpcSide(emu)
    ppc_env(s, c)
    obj = s.capture_obj(c["cap"], c["blocks"])
    name = emu.alloc(0x40, file_name(c).encode() + b"\0")
    ret = s.call("SaveUncompressedTGA__8CCaptureFPc", obj, name)
    out = ppc_after(s, obj, ret)
    out["file"] = ppc_file(s, c["disp"][0], c["disp"][1])
    return out


def save_host(lib, c):
    s = HostSide(lib)
    host_env(s, c)
    obj = s.capture_obj(c["cap"], c["blocks"])
    host_prepare(c)
    fn = F(lib, "tpw_CCapture_SaveUncompressedTGA", I32, VP, ctypes.c_char_p)
    ret = in_tmp(lambda: fn(obj, file_name(c).encode()))
    out = host_after(s, obj, ret)
    out["file"] = host_file(c)
    s.release()
    return out


def capture_case(r):
    c = getcapture_case(r)
    c["disp"] = rdisp(r, bpp=r.choice([16, 16, 24, 32, 32, 8]))
    c["surface"] = surface_for(r, c["disp"])
    c["fname"] = "shot%04d.tga" % r.randint(0, 9999)
    c["fopen_fail"] = r.random() < 0.08
    k = r.random()
    c["fail_at"] = -1 if k < 0.85 else r.choice([0, 1])
    name = file_name(c).encode()
    pending = -1 if r.random() < 0.1 else 0
    # mRGB starts on an old block: with a depth ConvertCapture does not handle, Capture
    # writes (and frees) whatever mRGB held (NULL would crash the host, as on any real Mac
    # with protected low memory).
    w, h = c["disp"][0], c["disp"][1]
    c["blocks"] = {"old": ("bytes", r.randbytes(3 * w * h))}
    c["cap"] = (None, "old", T.rint(r), name + b"\0" + r.randbytes(0x80 - len(name) - 1), pending)
    return c


def capture_ppc(emu, c):
    s = PpcSide(emu)
    ppc_env(s, c)
    obj = s.capture_obj(c["cap"], c["blocks"])
    ret = s.call("Capture__8CCaptureFv", obj)
    bpp = c["disp"][2]
    label_new_blocks(s, {"m0": "u16" if bpp == 16 else "bytes", "m1": "rgb16"})
    out = ppc_after(s, obj, ret)
    out["file"] = ppc_file(s, c["disp"][0], c["disp"][1], "m1" if bpp == 16 and len(emu.cap_new) > 1 else None)
    out["blocks"] = {s.labels[a]: s.contents(a) for a, _ in emu.cap_new}
    return out


def capture_host(lib, c):
    s = HostSide(lib)
    host_env(s, c)
    host_prepare(c)
    obj = s.capture_obj(c["cap"], c["blocks"])
    fn = F(lib, "tpw_CCapture_Capture", I32, VP)
    ret = in_tmp(lambda: fn(obj))
    out = host_after(s, obj, ret)
    out["file"] = host_file(c)
    out["blocks"] = {lab: s.contents(a) for a, lab in s.labels.items() if lab.startswith("m")}
    s.release()
    return out


def sinit_capture_ppc(emu, c):
    emu.write(CAPTURE, c["junk"])
    T.call_ppc(emu, emu.func("__sinit_tb_capture_cpp"), [])
    v = emu.read(CAPTURE, 0x90)
    return dict(head=struct.unpack(">IIi", v[:12]), name=v[12:0x8c], pending=struct.unpack(">I", v[0x8c:])[0])


def sinit_capture_host(lib, c):
    obj = F(lib, "tpw_gei_Capture", VP)()
    junk = c["junk"]
    F(lib, "tpw_capture_set", None, VP, VP, VP, I32, ctypes.c_char_p, I32)(
        obj, 1, 2, 3, junk[12:0x8c], 0)
    F(lib, "tpw_sinit_tb_capture", None)()
    s = HostSide(lib)
    got = s.get_capture(obj)
    return dict(head=(0 if got["screen"] is None else 1, 0 if got["rgb"] is None else 1, got["pixels"]),
                name=got["name"], pending=0 if got["pending"] == -1 else 1)

# ----------------------------------------------------------------- tb_error.cpp


def errors_ppc(label, extra=()):
    def run(emu, c):
        a = emu.alloc(0x204)
        PpcSide(emu).set_errors(a, c["errors"])
        r3 = T.call_ppc(emu, emu.func(label), [a, *[T.u32(x) for x in extra_args(c, extra)]])[0]
        out = dict(errors=PpcSide(emu).get_errors(a))
        if label.startswith("Pop"):
            out["ret"] = -1 if r3 == 0 else T.s32(r3 - a)
        return out
    return run


def extra_args(c, names):
    return [c[n] for n in names]


def push_host(lib, c):
    b = ctypes.create_string_buffer(0x204)
    s = HostSide(lib)
    s.set_errors(ctypes.addressof(b), c["errors"])
    F(lib, "tpw_CErrorHandler_Push", None, VP, I32, I32)(ctypes.addressof(b), c["level"], c["code"])
    return dict(errors=s.get_errors(ctypes.addressof(b)))


def pop_host(lib, c):
    b = ctypes.create_string_buffer(0x204)
    s = HostSide(lib)
    s.set_errors(ctypes.addressof(b), c["errors"])
    ret = F(lib, "tpw_CErrorHandler_Pop", I32, VP)(ctypes.addressof(b))
    return dict(errors=s.get_errors(ctypes.addressof(b)), ret=ret)


def push_case(r):
    errs = rerrors(r)
    if r.random() < 0.3:
        errs = (r.randint(0, 64), errs[1])
    return dict(errors=errs, level=T.s32(T.rint(r)), code=T.s32(r.getrandbits(32)))


def pop_case(r):
    errs = rerrors(r)
    if r.random() < 0.2:
        errs = (r.choice([-5, -1, -0x80000000]), errs[1])
    return dict(errors=errs)


def serror_ppc(emu, c):
    a = emu.alloc(8, c["junk"])
    r3 = T.call_ppc(emu, emu.func("__ct__6SErrorFv"), [a])[0]
    return dict(obj=emu.read(a, 8), ret_this=r3 == a)


def serror_host(lib, c):
    b = ctypes.create_string_buffer(c["junk"], 8)
    F(lib, "tpw_SError_ctor", None, VP)(ctypes.addressof(b))
    return dict(obj=b.raw[:8], ret_this=True)


def sinit_error_ppc(emu, c):
    s = PpcSide(emu)
    s.set_errors(ERRORS, c["errors"])
    T.call_ppc(emu, emu.func("__sinit_tb_error_cpp"), [])
    return dict(errors=s.get_errors(ERRORS))


def sinit_error_host(lib, c):
    s = HostSide(lib)
    s.set_errors(s.addr("gei_ErrorHandler"), c["errors"])
    F(lib, "tpw_sinit_tb_error", None)()
    return dict(errors=s.get_errors(s.addr("gei_ErrorHandler")))

# ----------------------------------------------------------------- Wavelet.cpp callbacks


def defmalloc_ppc(emu, c):
    emu.debugstrs = []
    r3 = T.call_ppc(emu, emu.func("DefaultMalloc__FUl"), [T.u32(c["size"])])[0]
    return dict(ret=r3, msgs=list(emu.debugstrs))


def debugstr_host(lib):
    n = I32.in_dll(lib, "tpw_debugstr_count").value
    last = ctypes.string_at(ctypes.addressof(ctypes.c_char.in_dll(lib, "tpw_debugstr_last")), 256)
    return [last[1:1 + last[0]]] if n == 1 else ["count %d" % n]


def defmalloc_host(lib, c):
    I32.in_dll(lib, "tpw_debugstr_count").value = 0
    r = F(lib, "tpw_DefaultMalloc", VP, U32)(T.u32(c["size"]))
    return dict(ret=r or 0, msgs=debugstr_host(lib))


def deffree_ppc(emu, c):
    emu.debugstrs = []
    ref = emu.alloc(4, struct.pack(">I", c["ptr"]))
    T.call_ppc(emu, emu.func("DefaultFree__FRPv"), [ref])
    return dict(ptr=emu.u32(ref), msgs=list(emu.debugstrs))


def deffree_host(lib, c):
    I32.in_dll(lib, "tpw_debugstr_count").value = 0
    ref = VP(c["ptr"] or None)
    F(lib, "tpw_DefaultFree", None, ctypes.POINTER(VP))(ctypes.byref(ref))
    return dict(ptr=ref.value or 0, msgs=debugstr_host(lib))


def setcb_ppc(emu, c):
    T.call_ppc(emu, emu.func("SetEngineCallbacks__FPFv_iPFv_bPFv_UlPFv_vPFUl_PvPFRPv_v"), list(c["v"]))
    out = dict(fns=[emu.u32(a) for a in CALLBACKS])
    emu.write(MALLOC_FN, struct.pack(">I", emu.cap_malloc_tv))
    emu.write(FREE_FN, struct.pack(">I", emu.cap_free_tv))
    return out


def setcb_host(lib, c):
    F(lib, "tpw_SetEngineCallbacks", None, *[ctypes.c_size_t] * 6)(*c["v"])
    out = (ctypes.c_uint64 * 6)()
    F(lib, "tpw_get_engine_callbacks", None, VP)(out)
    F(lib, "tpw_cap_install", None)()
    return dict(fns=list(out))

# ----------------------------------------------------------------- edges and the tests


def save_edges():
    r = random.Random(5)
    out = []
    for w, h in ((0, 0), (1, 1), (3, 2), (0, 5), (5, 0), (640 // 20, 480 // 20)):
        c = save_case(r)
        c["disp"] = (w, h, 24, c["disp"][3])
        c["blocks"] = {"rgb": ("bytes", bytes(range(256)) * ((3 * w * h) // 256 + 1))}
        c["cap"] = (None, "rgb", w * h, bytes(0x80), -1)
        c["fopen_fail"] = False
        out.append(c)
    c = dict(out[2])
    c["fopen_fail"] = True
    out.append(c)
    return out


def convert_edges():
    r = random.Random(6)
    out = []
    for bpp in (16, 24, 32, 8, 7, 33, 0, -8):
        for fmt in FORMATS.get(bpp, [rbits(r)]):
            n = 5
            c = dict(disp=(4, 2, bpp, [list(x) for x in fmt]), errors=(0, [(0, 0)] * 64),
                     blocks=pixel_block(r, n, bpp if bpp > 0 else 8, "screen"),
                     cap=("screen", None, n, bytes(0x80), -1), fail_at=-1)
            out.append(c)
    c = dict(out[0])
    c["fail_at"] = 0
    out.append(c)
    return out


def getcapture_edges():
    r = random.Random(7)
    out = []
    for bpp in (16, 24, 32):
        for lock, unlock, fail in ((0, 0, -1), (1, 0, -1), (0, 5, -1), (0, 0, 0)):
            c = getcapture_case(r)
            c["disp"] = rdisp(r, bpp=bpp, w=6, h=3)
            c["surface"] = surface_for(r, c["disp"])
            c.update(lock=lock, unlock=unlock, fail_at=fail)
            out.append(c)
    return out


TESTS = [
    FnTest("CCapture::SetCapture", setcapture_ppc, setcapture_host, rnd=setcapture_case,
           edge=lambda: [dict(cap=(None, None, 0, bytes(0x80), -1), name=n) for n in (None, b"", b"a", b"x" * 0x7f)]),
    FnTest("CCapture::GetCapture", getcapture_ppc, getcapture_host, edge=getcapture_edges, rnd=getcapture_case),
    FnTest("CCapture::ConvertCapture", convert_ppc, convert_host, edge=convert_edges, rnd=convert_case),
    FnTest("CCapture::SaveUncompressedTGA", save_ppc, save_host, edge=save_edges, rnd=save_case),
    FnTest("CCapture::Capture", capture_ppc, capture_host, rnd=capture_case),
    FnTest("__sinit_tb_capture_cpp", sinit_capture_ppc, sinit_capture_host,
           rnd=lambda r: dict(junk=r.randbytes(0x90)), edge=lambda: [dict(junk=b"\xff" * 0x90)]),
    FnTest("CErrorHandler::Push", errors_ppc("Push__13CErrorHandlerFii", ("level", "code")), push_host,
           rnd=push_case, edge=lambda: [dict(errors=(n, [(1, 2)] * 64), level=2, code=-0x4f8ac000)
                                        for n in (0, 1, 63, 64, 65, 0x7fffffff)]),
    FnTest("CErrorHandler::Pop", errors_ppc("Pop__13CErrorHandlerFv"), pop_host, rnd=pop_case,
           edge=lambda: [dict(errors=(n, [(i, -i) for i in range(64)])) for n in (0, 1, 2, 64, -1)]),
    FnTest("SError::SError", serror_ppc, serror_host, rnd=lambda r: dict(junk=r.randbytes(8))),
    FnTest("__sinit_tb_error_cpp", sinit_error_ppc, sinit_error_host, rnd=lambda r: dict(errors=rerrors(r))),
    FnTest("DefaultMalloc", defmalloc_ppc, defmalloc_host, rnd=lambda r: dict(size=T.rint(r))),
    FnTest("DefaultFree", deffree_ppc, deffree_host, rnd=lambda r: dict(ptr=r.choice([0, r.getrandbits(32)]))),
    FnTest("SetEngineCallbacks", setcb_ppc, setcb_host, rnd=lambda r: dict(v=[r.getrandbits(32) for _ in range(6)])),
]
