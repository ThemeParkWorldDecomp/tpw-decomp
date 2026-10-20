"""w85 TrueTypeRender (main app): the constructor, the deleting-destructor shell, load_settings and render_font_unicode.

Every callee outside the unit is a fake on both sides that appends the same text line to a log: the file read and the
byte swap, wcslen, Bitmap::realloc, operator new[] and delete[] (blocks filled with 0xc3; the Nth new[] can answer NULL;
a delete[] is logged with a hash of the block at that moment), and the eleven GDI calls. The GDI handles are small
integers the fakes count out (DIB 0x4001.., font 0x5001.., device context 0x6001..; the caller's is 0x500), SelectObject
answers the bitmap or font selected last (stock ones 0x4000 and 0x5000 first), GetTextExtentPoint answers a scripted list of
extents, and CreateDIBSection hands back a buffer the test filled. The object is compared field by field (the Mac's words
are big-endian, the host's little-endian; the byte tail raw), the output bitmap and the DIB buffer byte by byte.
Text is ASCII (the unit converts with %ls; MSL's 16-bit wchar_t is modelled by taking the low byte).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_TrueTypeRender"
PROGRAM = "SimTheme Park"
CT, DT, RENDER, LOAD = 0x100C9C90, 0x100C9D50, 0x100C9DC0, 0x100CA340
READ = 0x10004260
MASK = 0xFFFFFFFF
DC = 0x500
IMP_DIB = ("CreateDIBSection__10NS_MacDozeFPQ210NS_MacDoze14InternalHandlePCQ210NS_MacDoze13tagBITMAPINFO"
           "UiPPvPQ210NS_MacDoze14InternalHandleUl")
IMP_DC = "CreateCompatibleDC__10NS_MacDozeFPQ210NS_MacDoze14InternalHandle"
IMP_SELECT = "SelectObject__10NS_MacDozeFPQ210NS_MacDoze14InternalHandlePQ210NS_MacDoze14InternalHandle"
IMP_MAPMODE = "SetMapMode__10NS_MacDozeFPQ210NS_MacDoze14InternalHandlei"
IMP_FONT = "CreateFontIndirect__10NS_MacDozeFPCQ210NS_MacDoze11tagLOGFONTA"
IMP_EXTENT = "GetTextExtentPoint__10NS_MacDozeFPQ210NS_MacDoze14InternalHandlePCciPQ210NS_MacDoze7tagSIZE"
IMP_DELOBJ = "DeleteObject__10NS_MacDozeFPQ210NS_MacDoze14InternalHandle"
IMP_BK = "SetBkColor__10NS_MacDozeFPQ210NS_MacDoze14InternalHandleUl"
IMP_TEXTCOLOUR = "SetTextColor__10NS_MacDozeFPQ210NS_MacDoze14InternalHandleUl"
IMP_TEXTOUT = "TextOut__10NS_MacDozeFPQ210NS_MacDoze14InternalHandleiiPCci"
IMP_DELDC = "DeleteDC__10NS_MacDozeFPQ210NS_MacDoze14InternalHandle"
IMP_SWAP = "_USwapBlock32__13SamsUtilitiesFPUll"


# ---------------------------------------------------------------- helpers

def line(name, ints=(), text=None):
    """One log line in the format of the unit file's event()."""
    s = name + "".join(f" {i}" for i in ints)
    return s if text is None else s + " |" + text


def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & MASK
    return h


def pack_object(words, tail, big):
    """The object: nine int32 words (+0x00..+0x23) in the side's byte order, then the 0x16c tail bytes."""
    return struct.pack((">" if big else "<") + "i" * 9, *[T.s32(w) for w in words]) + bytes(tail)


def unpack_object(raw, big):
    return list(struct.unpack((">" if big else "<") + "i" * 9, raw[:0x24])) + list(raw[0x24:0x190])


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def log_lines(text):
    return [l for l in text.split("\n") if l]


# ---------------------------------------------------------------- constructor

def ppc_ctor(e, c):
    obj = e.alloc(0x190, pack_object(c["words"], c["tail"], True))
    result = T.call_ppc(e, CT, [obj])[0]
    return dict(same=int(result == obj), obj=unpack_object(e.read(obj, 0x190), True), log=[])


def host_ctor(lib, c):
    buf = (C.c_uint8 * 0x190).from_buffer_copy(pack_object(c["words"], c["tail"], False))
    lib.tpw_ttr_ctor.argtypes = [C.c_void_p]
    lib.tpw_ttr_ctor(C.addressof(buf))
    return dict(same=1, obj=unpack_object(bytes(buf), False), log=[])


def object_case(r=None, fill=0xA5):
    if r is None:
        return dict(words=[T.s32(fill * 0x01010101)] * 9, tail=bytes([fill]) * 0x16C)
    return dict(words=[r.getrandbits(32) for _ in range(9)], tail=bytes(r.getrandbits(8) for _ in range(0x16C)))


def ctor_edges():
    for fill in (0xA5, 0, 0xFF, 0x7B, 0x80, 0x01):
        yield object_case(fill=fill)


# ---------------------------------------------------------------- destructor shell

def ppc_dtor(e, c):
    log = []
    st = CL.Stubs()
    st.real = {DT}
    st.imp("__dl__FPv", lambda e: log.append(line("delete")))
    obj = 0 if c["null"] else e.alloc(0x190, b"\xa5" * 0x190)
    saved = st.activate(e)
    try:
        result = T.call_ppc(e, DT, [obj, c["flag"] & MASK])[0]
    finally:
        CL.Stubs.deactivate(e, saved)
    return dict(same=int(result == obj), log=log)


def host_dtor(lib, c):
    lib.tpw_ttr_dtor.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_ttr_dtor(T.s32(c["flag"]), c["null"])
    lib.tpw_ttr_log.restype = C.c_char_p
    return dict(same=1, log=log_lines(lib.tpw_ttr_log().decode("latin-1")))


def dtor_edges():
    for null in (0, 1):
        for flag in (0, 1, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x8001, -0x8000, 0x7FFFFFFF, 0xFFFF8000):
            yield dict(flag=flag, null=null)


def dtor_random(r):
    return dict(flag=r.choice([r.getrandbits(32), r.getrandbits(16), r.randrange(-4, 5)]), null=int(r.random() < 0.2))


# ---------------------------------------------------------------- load_settings

def read_stub(e, w, log):
    """The fake TPFile_Read: copies what the blob still holds (a short file leaves the rest of the request untouched)."""
    dest, size, count = e.r(3), e.r(4), e.r(5)
    off = dest - w["obj"]
    log.append(line("Read", [off, size, count, w["pos"]]))
    want = size * count
    got = max(0, min(want, len(w["blob"]) - w["pos"]))
    e.write(dest, w["blob"][w["pos"]:w["pos"] + got])
    w["pos"] += got
    e.set_r(3, got // size if size else 0)


def ppc_load(e, c):
    log = []
    obj = e.alloc(0x190, bytes(c["fill"]))
    w = dict(obj=obj, blob=c["blob"], pos=0)

    def swap(e):
        a, n = e.r(3), e.r(4)
        log.append(line("Swap", [a - obj, T.s32(n)]))
        for i in range(n):
            e.write(a + 4 * i, e.read(a + 4 * i, 4)[::-1])

    st = CL.Stubs()
    st.real = {LOAD}
    st.internal(READ, lambda e: read_stub(e, w, log))
    st.imp(IMP_SWAP, swap)
    saved = st.activate(e)
    try:
        result = T.call_ppc(e, LOAD, [obj, 0])[0]
    finally:
        CL.Stubs.deactivate(e, saved)
    return dict(result=result, obj=list(e.read(obj, 0x190)), pos=w["pos"], log=log)


def host_load(lib, c):
    obj = (C.c_uint8 * 0x190).from_buffer_copy(bytes(c["fill"]))
    blob = (C.c_uint8 * (len(c["blob"]) + 1)).from_buffer_copy(c["blob"] + b"\0")
    lib.tpw_ttr_load.argtypes = [C.c_void_p, C.c_void_p, C.c_int32]
    lib.tpw_ttr_load.restype = C.c_int32
    result = lib.tpw_ttr_load(C.addressof(obj), C.addressof(blob), len(c["blob"]))
    lib.tpw_ttr_log.restype = C.c_char_p
    log = log_lines(lib.tpw_ttr_log().decode("latin-1"))
    lib.tpw_ttr_position.restype = C.c_int32
    return dict(result=result & MASK, obj=list(bytes(obj)), pos=lib.tpw_ttr_position(), log=log)


def load_case(r=None, size=0x188, fill=0xA5):
    if r is None:
        data = bytes((i * 37 + 11) & 0xFF for i in range(size))
        return dict(fill=[fill] * 0x190, blob=data)
    return dict(fill=[r.getrandbits(8) for _ in range(0x190)], blob=bytes(r.getrandbits(8) for _ in range(size)))


def load_edges():
    for size in (0x188, 0x189, 0x200, 0, 1, 0x3F, 0x40, 0x41, 0x143, 0x144, 0x145, 0x148, 0x14B, 0x14C, 0x14D, 0x150, 0x187,
                 0x180, 0x160):
        yield load_case(size=size)
    yield dict(fill=[0] * 0x190, blob=bytes(range(256)) + bytes(range(0x188 - 256)))
    yield dict(fill=[0xFF] * 0x190, blob=b"\xff" * 0x188)
    yield dict(fill=[0] * 0x190, blob=b"\x00" * 0x188)
    yield dict(fill=[0x5A] * 0x190, blob=b"\x80\x00\x00\x01" * 0x62)


def load_random(r):
    size = r.choice([0x188, 0x188, 0x188, r.randrange(0, 0x1A0)])
    return load_case(r, size)


# ---------------------------------------------------------------- render_font_unicode

class Render:
    """The PowerPC side's fakes for one render case (the same lines as the unit file's)."""

    def __init__(self, e, c):
        self.e, self.c = e, c
        self.log = []
        self.blocks = []                 # (address, size) of the new[] blocks, in order (ordinal = index + 1)
        self.new_calls = 0
        self.extent_next = 0
        self.next_dib = self.next_font = self.next_dc = 0
        self.current = {4: 0x4000, 5: 0x5000}
        self.bits = 0

    def rec(self, name, ints=(), text=None):
        self.log.append(line(name, ints, text))

    def new_array(self, e):
        size = e.r(3)
        self.new_calls += 1
        self.rec("new[]", [size])
        if self.new_calls == self.c["fail_new"]:
            e.set_r(3, 0)
            return
        a = e.alloc(max(size, 1), b"\xc3" * size)
        if len(self.blocks) < 16:
            self.blocks.append((a, size))
        e.set_r(3, a)

    def delete_array(self, e):
        a = e.r(3)
        for i, (addr, size) in enumerate(self.blocks):
            if addr == a:
                skip = 0x28 if size == 0x42C else 0
                self.rec("delete[]", [i + 1, fnv(e.read(a + skip, size - skip))])
                self.blocks[i] = (None, size)
                break

    def wcslen(self, e):
        a, n = e.r(3), 0
        while e.read(a + 2 * n, 2) != b"\0\0":
            n += 1
        e.set_r(3, n)

    def snprintf(self, e):
        buf, size, fmt, wide = e.r(3), e.r(4), e.cstr(e.r(5)), e.r(6)
        if fmt != "%ls":
            self.rec("snprintf-bad-format", [], fmt)
        text = bytearray()
        while True:
            ch = struct.unpack(">H", e.read(wide + 2 * len(text), 2))[0]
            if ch == 0:
                break
            text.append(ch if ch < 0x100 else 0x3F)
        if size:
            e.write(buf, bytes(text[:size - 1]) + b"\0")
        e.set_r(3, len(text))

    def realloc(self, e):
        a, w, h, bpp = e.r(3), e.r(4), e.r(5), e.r(6)
        ow, oh, obpp, data = struct.unpack(">IIII", e.read(a, 16))
        self.rec("realloc", [w, h, bpp, ow, oh, obpp])
        size = (w * h * bpp) & MASK
        if (w, h, bpp) != (ow, oh, obpp) or not data:
            data = e.alloc(max(size, 1))
            ws(e, a, [w, h, bpp, data])
        e.write(data, bytes(size))
        e.set_r(3, 1)

    def create_dib(self, e):
        dc, info, usage, bits, section, offset = [e.r(i) for i in range(3, 9)]
        raw = e.read(info, 0x430)
        size, width, height = struct.unpack(">Iii", raw[:12])
        planes, bitcount = struct.unpack(">HH", raw[12:16])
        rest = struct.unpack(">IIiiII", raw[16:40])
        self.rec("CreateDIBSection", [dc, usage, section, offset, size, width, height, planes, bitcount] + list(rest)
                 + [fnv(raw[0x28:0x428]), fnv(raw[0x428:0x42C])])
        if self.c["dib_null"]:
            e.set_r(3, 0)
            return
        ws(e, bits, [self.bits])
        e.set_r(3, 0x4001 + self.next_dib)
        self.next_dib += 1

    def create_dc(self, e):
        result = 0x6001 + self.next_dc
        self.next_dc += 1
        self.rec("CreateCompatibleDC", [e.r(3), result])
        e.set_r(3, result)

    def select(self, e):
        dc, obj = e.r(3), e.r(4)
        previous = 0
        if obj >> 12 in self.current:
            previous = self.current[obj >> 12]
            self.current[obj >> 12] = obj
        self.rec("SelectObject", [dc, obj, previous])
        e.set_r(3, previous)

    def font(self, e):
        a = e.r(3)
        raw = e.read(a, 0x3C)
        words = struct.unpack(">5i", raw[:20])
        result = 0x5001 + self.next_font
        self.next_font += 1
        self.rec("CreateFontIndirect", list(words) + list(raw[0x14:0x1C]) + [result], e.cstr(a + 0x1C))
        e.set_r(3, result)

    def extent(self, e):
        dc, text, length, size = e.r(3), e.r(4), e.r(5), e.r(6)
        self.rec("GetTextExtentPoint", [dc, T.s32(length)], e.cstr(text))
        pairs = self.c["extents"]
        i = self.extent_next if self.extent_next < len(pairs) else len(pairs) - 1
        self.extent_next += 1
        ws(e, size, [pairs[i][0], pairs[i][1]])
        e.set_r(3, 1)

    def text_out(self, e):
        self.rec("TextOut", [e.r(3), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(7))], e.cstr(e.r(6)))
        e.set_r(3, 1)

    def simple(self, name, count=1, signed=()):
        def fake(e):
            self.rec(name, [T.s32(e.r(3 + i)) if i in signed else e.r(3 + i) for i in range(count)])
            e.set_r(3, 1)
        return fake

    def stubs(self):
        st = CL.Stubs()
        st.real = {RENDER}
        st.imp("__nwa__FUl", self.new_array)
        st.imp("__dla__FPv", self.delete_array)
        st.imp("wcslen", self.wcslen)
        st.imp("snprintf", self.snprintf)
        st.imp("realloc__6BitmapFUiUiUi", self.realloc)
        st.imp(IMP_DIB, self.create_dib)
        st.imp(IMP_DC, self.create_dc)
        st.imp(IMP_SELECT, self.select)
        st.imp(IMP_MAPMODE, self.simple("SetMapMode", 2, signed=(1,)))
        st.imp(IMP_FONT, self.font)
        st.imp(IMP_EXTENT, self.extent)
        st.imp(IMP_DELOBJ, self.simple("DeleteObject"))
        st.imp(IMP_BK, self.simple("SetBkColor", 2))
        st.imp(IMP_TEXTCOLOUR, self.simple("SetTextColor", 2))
        st.imp(IMP_TEXTOUT, self.text_out)
        st.imp(IMP_DELDC, self.simple("DeleteDC"))
        return st


def ppc_render(e, c):
    w, h, bpp = c["bm"]
    size = (w * h * bpp) & MASK
    obj = e.alloc(0x190, pack_object(c["words"], c["tail"], True))
    data = e.alloc(max(size, 1), b"\x7e" * size)
    bm = e.alloc(16)
    ws(e, bm, [w, h, bpp, data])
    wide = b"".join(struct.pack(">H", ch) for ch in c["text"]) + b"\0\0"
    text = e.alloc(len(wide), wide)
    world = Render(e, c)
    world.bits = e.alloc(len(c["bits"]) + 8, c["bits"] + b"\xa5" * 8)
    saved = world.stubs().activate(e)
    try:
        result = T.call_ppc(e, RENDER, [obj, DC, text, bm])[0]
    finally:
        CL.Stubs.deactivate(e, saved)
    ow, oh, obpp, odata = struct.unpack(">IIII", e.read(bm, 16))
    after = (obpp * ow * oh) & MASK
    return dict(result=result, obj=unpack_object(e.read(obj, 0x190), True), bitmap=[ow, oh, obpp],
                data=e.read(odata, after), bits=e.read(world.bits, len(c["bits"]) + 8), log=world.log)


def host_render(lib, c):
    w, h, bpp = c["bm"]
    obj = (C.c_uint8 * 0x190).from_buffer_copy(pack_object(c["words"], c["tail"], False))
    wide = (C.c_uint16 * (len(c["text"]) + 1))(*c["text"], 0)
    bm = (C.c_uint32 * 3)(w, h, bpp)
    out = (C.c_uint8 * (max((w * h * bpp) & MASK, 1) + 1))()
    pairs = c["extents"]
    ext = (C.c_int32 * (2 * len(pairs)))(*[T.s32(v) for p in pairs for v in p])
    bits = (C.c_uint8 * (len(c["bits"]) + 8)).from_buffer_copy(c["bits"] + b"\xa5" * 8)
    fn = lib.tpw_ttr_render
    fn.restype = C.c_int32
    fn.argtypes = [C.c_void_p] * 2 + [C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32, C.c_void_p, C.c_int32, C.c_int32]
    result = fn(C.addressof(obj), C.addressof(wide), C.addressof(bm), C.addressof(out), C.addressof(ext), len(pairs),
                C.addressof(bits), c["fail_new"], c["dib_null"])
    lib.tpw_ttr_log.restype = C.c_char_p
    log = log_lines(lib.tpw_ttr_log().decode("latin-1"))
    after = (bm[2] * bm[0] * bm[1]) & MASK
    return dict(result=result & MASK, obj=unpack_object(bytes(obj), False), bitmap=list(bm), data=bytes(out)[:after],
                bits=bytes(bits), log=log)


def face(r, name=None):
    text = (name if name is not None else "".join(chr(r.randrange(0x41, 0x7B)) for _ in range(r.randrange(0, 20)))).encode()
    return text + b"\0" * (32 - len(text))


def render_case(r=None, text="Hi", bm=(4, 3, 1), height=-16, width=0, text_y=3, extents=None, bits=None, fail_new=0,
                dib_null=0, left=0x11, right=0x22, name=None):
    """A case. extents: (cx, cy) pairs (default: narrow enough that no search runs). bits: the DIB bytes (default: a ramp)."""
    w, h, bpp = bm
    words = [left, right, 100, text_y, height, width, 0x01020304, -5, 400]
    flags = bytes([0, 0, 0, 1, 4, 1, 4, 0])
    rest = bytes((0x30 + i) & 0xFF for i in range(0x16C - 40))
    tail = flags + face(r, name or "Tahoma") + rest
    if extents is None:
        extents = [(w * 2 - 1, 7)]
    if bits is None:
        bits = bytes((i * 7 + 3) & 0xFF for i in range(w * 2 * h * 2))
    return dict(words=words, tail=tail, text=[ord(ch) for ch in text], bm=bm, extents=extents, bits=bits,
                fail_new=fail_new, dib_null=dib_null)


def search_extents(r, length, width_limit, height, width, scale, natural=None):
    """Extents as a font would give them: natural width for the first call, then scale * lfWidth * length for each width the
    search tries. Model of the algorithm, used only to script the fake; the original's own loop decides what is asked."""
    first = natural if natural is not None else r.randrange(width_limit, width_limit * 3 + 2)
    out = [(first, 10)]
    if first < width_limit:
        return out
    hi = width if width != 0 else -height
    lo = 1
    mid = ((hi - 1) >> 1) + 1
    for _ in range(80):
        cx = max(0, mid * scale * max(length, 1) // 2 + r.randrange(0, 3))
        out.append((cx, 11))
        if hi - lo < 3 and cx < width_limit:
            break
        if cx < width_limit:
            lo = mid
            mid = mid + ((hi - mid) >> 1)
        else:
            hi = mid
            mid = lo + ((mid - lo) >> 1)
    if out[-1][0] >= width_limit:
        out.append((0, 12))   # the original never ends if the extents never fit: always finish with one that does
    return out


def inked_bits(r, w, h, pixels, value=None):
    """DIB bytes (2w x 2h) where only the listed mask pixels (x, y) are inked: all four source bytes set to `value`."""
    data = bytearray(w * 2 * h * 2)
    for x, y in pixels:
        v = value if value is not None else r.randrange(4, 256)
        for dy in (0, 1):
            for dx in (0, 1):
                data[(2 * y + dy) * w * 2 + 2 * x + dx] = v
    return bytes(data)


def render_edges():
    import random
    r = random.Random(85)
    # empty text: the zeroing product bpp * (w * h)
    for bm in [(4, 3, 1), (4, 3, 2), (5, 7, 3), (1, 1, 4), (0, 5, 1), (5, 0, 1), (3, 3, 0), (16, 16, 1), (7, 1, 5)]:
        yield render_case(r, text="", bm=bm)
    # allocation and DIB failures
    for fail in (1, 2):
        yield render_case(r, fail_new=fail)
    yield render_case(r, dib_null=1)
    yield render_case(r, text="", fail_new=1)
    # no search: extents below the limit (w * 2), including exactly one below
    for cx in (0, 1, 7, 8 - 1):
        yield render_case(r, bm=(4, 3, 1), extents=[(cx, 5)])
    # the limit itself and above run the search (scripted, converging at once or later)
    for bm in [(4, 3, 1), (4, 3, 2), (8, 2, 1)]:
        for width, height in [(0, -16), (20, -16), (0, 16), (0, 0), (1, 0), (2, -2), (3, 0), (5, 0), (7, 0), (100, -16)]:
            limit = bm[0] * 2
            for natural in (limit, limit + 1, limit * 2):
                yield render_case(r, text="Wide", bm=bm, height=height, width=width,
                                  extents=search_extents(r, 4, limit, height, width, 1, natural=natural))
    # scripted sequences not tied to a font: fit at once, never fit until the end, alternate
    limit = 8
    yield render_case(r, bm=(4, 3, 1), width=40, extents=[(limit, 1), (limit - 1, 2)])
    yield render_case(r, bm=(4, 3, 1), width=40, extents=[(limit, 1), (limit, 2), (limit + 5, 3), (limit - 3, 4)])
    yield render_case(r, bm=(4, 3, 1), width=3, extents=[(limit, 1), (limit - 1, 1)])
    yield render_case(r, bm=(4, 3, 1), width=4, extents=[(limit, 1), (limit + 3, 1), (limit - 1, 1)])
    yield render_case(r, bm=(4, 3, 1), width=6, extents=[(limit, 1), (limit, 1), (limit - 1, 1)])
    yield render_case(r, bm=(4, 3, 1), width=0, height=-1, extents=[(limit, 1), (limit, 1), (limit, 1), (0, 1)])
    yield render_case(r, bm=(4, 3, 1), width=0, height=-1, extents=[(limit, 1), (limit - 8, 1)])
    yield render_case(r, bm=(4, 3, 1), width=-3, height=-1, extents=[(limit, 1), (limit - 8, 1)])
    # extreme widths and heights for the search's start (hi = lfWidth, or -lfHeight when the width is 0): huge values need about
    # thirty rounds of the bisection, negative ones end at the first (hi - lo < 3 once the extent fits); INT_MIN is left out (the
    # negation and hi - 1 overflow, undefined in the translation)
    limit = 8
    for width in (0x7FFFFFFF, 0x40000000, 0x10000, -1, -100, -0x40000000, -0x7FFFFFFF):
        for scale in (1, 3):
            yield render_case(r, text="Wide", bm=(4, 3, 1), height=-16, width=width,
                              extents=search_extents(r, 4, limit, -16, width, scale, natural=limit + 1))
    for height in (-0x7FFFFFFF, -0x40000001, 0x7FFFFFFF, 0x40000000):
        yield render_case(r, text="Wide", bm=(4, 3, 1), height=height, width=0,
                          extents=search_extents(r, 4, limit, height, 0, 2, natural=limit + 1))
    # text x position: size.cx halved toward zero (negative and odd extents)
    for cx in (0, 1, 2, 3, 5, 7):
        yield render_case(r, extents=[(cx, 1)])
    for cx in (-1, -2, -3, -5, -100):
        yield render_case(r, extents=[(cx, 1)])
    for text_y in (-7, 0, 1, 0x7FFFFFFF, -0x80000000):
        yield render_case(r, text_y=text_y)
    # row scan: no ink, one inked row (the mRight -1 quirk), two, many, ink only in the last column, ink in the first
    w, h = 5, 6
    ink = {
        "none": [], "one row": [(2, 3)], "one row first": [(0, 0)], "one row last row": [(1, 5)],
        "two rows": [(1, 1), (3, 4)], "adjacent rows": [(1, 2), (1, 3)], "many": [(x, y) for y in range(h) for x in range(w)],
        "last column": [(w - 1, 2)], "last column two rows": [(w - 1, 1), (w - 1, 3)],
        "last column and another": [(w - 1, 2), (0, 3)], "row with last col and an earlier pixel": [(w - 2, 2), (w - 1, 2)],
        "first column rows 1 2 4": [(0, 1), (0, 2), (0, 4)],
    }
    for name, pixels in ink.items():
        yield render_case(r, text="ink", bm=(w, h, 1), bits=inked_bits(r, w, h, pixels))
    # downsample: a faint pixel (the four source bytes sum below 4 gives 0), exact sums
    yield render_case(r, bm=(2, 2, 1), bits=bytes([1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]))
    yield render_case(r, bm=(2, 2, 1), bits=bytes([1, 1, 1, 1] + [0] * 12))
    yield render_case(r, bm=(2, 2, 1), bits=bytes([0, 1, 1, 1, 0, 0, 0, 0] + [255] * 8))
    yield render_case(r, bm=(2, 2, 1), bits=bytes([255] * 16))
    yield render_case(r, bm=(2, 2, 1), bits=bytes([255, 255, 0, 0, 0, 0, 255, 255, 3, 3, 3, 3, 0, 0, 0, 1]))
    # bitmap depths and shapes: realloc to depth 1, a bitmap of another depth, empty bitmap
    for bm in [(3, 3, 2), (3, 3, 4), (2, 5, 3), (1, 1, 1), (0, 4, 1), (6, 0, 2)]:
        yield render_case(r, bm=bm)
    # the face name and object contents travel through CreateFontIndirect
    yield render_case(r, name="Arial Black", width=0, height=-12)
    yield render_case(r, name="")
    # text lengths
    for text in ("a", "Hello World", "x" * 40, "A B", "\x01\x7f~"):
        yield render_case(r, text=text)


def render_random(r):
    w = r.choice([1, 2, 3, 4, 5, 8, 9, 12, 16])
    h = r.choice([1, 2, 3, 4, 6, 8])
    bpp = r.choice([1, 1, 1, 2, 3, 4])
    length = r.choice([0, 1, 2, 3, 5, 9, 14])
    text = "".join(chr(r.randrange(0x20, 0x7F)) for _ in range(length))
    height = r.choice([0, -8, -16, -40, 12, r.randrange(-60, 60)])
    width = r.choice([0, 0, 1, 2, 3, 4, 7, 10, 25, 64, r.randrange(-3, 200)])
    text_y = r.randrange(-10, 40)
    limit = w * 2
    mode = r.random()
    if mode < 0.35:
        extents = [(r.randrange(0, limit), r.randrange(0, 30))]
    elif mode < 0.85:
        extents = search_extents(r, length, limit, height, width, r.choice([1, 2, 3, 5]))
    else:
        extents = [(r.randrange(limit - 2, limit * 3), 1) for _ in range(r.randrange(1, 6))] + [(r.randrange(0, limit), 2)]
    kind = r.random()
    count = w * 2 * h * 2
    if kind < 0.3:
        bits = bytes(r.getrandbits(8) for _ in range(count))
    elif kind < 0.7:
        pixels = [(x, y) for y in range(h) for x in range(w) if r.random() < r.choice([0.05, 0.15, 0.4])]
        bits = inked_bits(r, w, h, pixels)
    elif kind < 0.9:
        # sparse bytes: mostly zero with a few small ones
        bits = bytes(r.choice([0, 0, 0, 0, 0, 0, 1, 2, 3, r.getrandbits(8)]) for _ in range(count))
    else:
        rows = r.sample(range(h), r.randrange(0, h + 1))
        pixels = [(r.randrange(w), y) for y in rows]
        bits = inked_bits(r, w, h, pixels)
    fail_new = r.choice([0] * 9 + [1, 2])
    dib_null = int(r.random() < 0.05)
    return render_case(r, text=text, bm=(w, h, bpp), height=height, width=width, text_y=text_y, extents=extents, bits=bits,
                       fail_new=fail_new, dib_null=dib_null, left=r.getrandbits(32), right=r.getrandbits(32))


TESTS = [
    FnTest("TrueTypeRender::TrueTypeRender", ppc_ctor, host_ctor, edge=ctor_edges, rnd=object_case),
    FnTest("TrueTypeRender::~TrueTypeRender", ppc_dtor, host_dtor, edge=dtor_edges, rnd=dtor_random),
    FnTest("TrueTypeRender::load_settings", ppc_load, host_load, edge=load_edges, rnd=load_random),
    FnTest("TrueTypeRender::render_font_unicode", ppc_render, host_render, edge=render_edges, rnd=render_random),
]


def setup(runner):
    T.install_std_stubs(runner.emu)
