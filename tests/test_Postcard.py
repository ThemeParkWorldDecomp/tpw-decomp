"""Differential tests for the postcard composer, against libtpw_unit_Postcard.dylib (tests/unit_Postcard.cpp): src/game/ui/Postcard.cpp,
the 14 functions of PostcardBitmap (0x100bf7c4..0x100c0050 and 0x100c0050 resize) and Postcard (0x100c03a4..0x100c0ac4).

One world per case: a Postcard (seven bitmaps with their pixels), the call's arguments, an output buffer, the display size, the
temporary allocator (absent or present, answering a buffer or NULL), the allocations that fail, the pictures the TGA loader serves
(by path) and the paths FindFilePath answers. The PowerPC side runs the original on that world in the emulator with Python fakes;
the host side runs the translation on the same world through one C entry (tpw_pc_run) with C++ fakes. Both record the same
events: the loader and its area, FindFilePath, the pixel converter (UPixels), the temporary allocator, OpenGL and UDebug, and the
heap (each operator new[] with its number in the call, its size and whether the case made it fail; each operator delete[] with
the number of the allocation it releases, -1 for a block the set-up made, and its size). Every new block is filled with 0xa5 on both sides.
The state compared afterwards is each bitmap's words and pixels (size, crc32 and the first eight bytes), the output buffer with
16 guard bytes, whether the temporary allocator exists, and the answer.

The fakes do what the real calls do to the data: glReadPixels fills the buffer from a repeated random pattern, the loader's
AllocMemory gives a zeroed store that Load fills from the picture, and the converter copies every byte of the source (its rows
reversed when it was flipped) xor 0x5a, so a wrong flip or a wrong size changes the bytes.
"""
import ctypes as C
import random
import struct
import zlib

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Postcard"

# the operations, in the order of the C++ enum
(BITMAP_CTOR, BITMAP_DTOR, BITMAP_CLEAR, LOAD_FROM_TGA, COPY_FROM_OPENGL, ALPHABLT, RESIZE, POSTCARD_CTOR, POSTCARD_DTOR,
 POSTCARD_CLEAR, LOAD_SOURCES, GRAB, GENERATE, PREVIEW) = range(14)
ENTRY = [0x100BF7C4, 0x100BF84C, 0x100BF8D4, 0x100BF954, 0x100BFC3C, 0x100BFF2C, 0x100C0050, 0x100C03A4, 0x100C0478, 0x100C0658,
         0x100C07C4, 0x100C0954, 0x100C09B4, 0x100C0AC4]
RETURNS = (LOAD_FROM_TGA, COPY_FROM_OPENGL, RESIZE, LOAD_SOURCES, GRAB)   # the functions that answer a number
FIND_FILE_PATH, UDEBUG = 0x100597F8, 0x10000E40
TEMP_SIZE = 0x4B000
STORE_SIZE = 1 << 19
OUT_GUARD = 16
PATTERN_SIZE = 4099
BITMAPS = 7
NAMES = ["tpwlogo", "bullfrog", "stamp", "frank", "legal", "card", "grab"]
SOURCE_NAMES = ["data:postcard:tpwlogo.tga", "data:postcard:bullfrog.tga", "data:postcard:stamp.tga", "data:postcard:frank.tga",
                "data:postcard:legal.tga"]
MINUS = 0xFFFFFFFF
FILL_PATTERN = random.Random(99).randbytes(PATTERN_SIZE)


def s32(v):
    return T.s32(v)


def signed64(v):
    return v - (1 << 64) if v >= 1 << 63 else v


# ======================================================================= the case, as a blob for the host


def make_case(fn, bitmaps=(), args=(), out=0, display=(0, 0), alloc=0, alloc_null=0, fail=(), pattern=FILL_PATTERN, pictures=(),
              finds=(), prefix=None, name=None):
    """bitmaps: (width, height, bytes a pixel, pixels or None) for the first bitmaps (the rest are empty)."""
    padded = [tuple(b) for b in bitmaps] + [(0, 0, 0, None)] * (BITMAPS - len(bitmaps))
    return dict(fn=fn, bitmaps=padded, args=list(args) + [0] * (8 - len(args)), out=out, display=display, alloc=alloc,
                alloc_null=alloc_null, fail=list(fail), pattern=pattern, pictures=list(pictures), finds=list(finds), prefix=prefix,
                name=name)


def encode(case):
    blob = bytearray()

    def u32(v):
        blob.extend(struct.pack("<I", v & MINUS))

    def raw(data):
        blob.extend(data)
        blob.extend(b"\0" * (-len(data) % 4))

    def text(value):
        data = value.encode("latin-1")
        u32(len(data))
        raw(data)

    u32(case["fn"])
    for value in case["args"]:
        u32(value)
    u32(BITMAPS)
    for width, height, bpp, pixels in case["bitmaps"]:
        u32(width)
        u32(height)
        u32(bpp)
        u32(MINUS if pixels is None else len(pixels))
        if pixels is not None:
            raw(pixels)
    u32(case["out"])
    u32(case["display"][0])
    u32(case["display"][1])
    u32(case["alloc"])
    u32(case["alloc_null"])
    u32(len(case["fail"]))
    for number in case["fail"]:
        u32(number)
    u32(len(case["pattern"]))
    raw(case["pattern"])
    u32(len(case["pictures"]))
    for picture in case["pictures"]:
        text(picture["path"])
        for key in ("depth", "palettised", "width", "height", "header", "load"):
            u32(picture[key])
        u32(len(picture["pixels"]))
        raw(picture["pixels"])
    u32(len(case["finds"]))
    for path in case["finds"]:
        text(path)
    u32(0 if case["prefix"] is None else 1)
    text(case["prefix"] or "")
    u32(0 if case["name"] is None else 1)
    text(case["name"] or "")
    return bytes(blob)


# ======================================================================= the host side


def host(lib, case):
    lib.tpw_pc_run.argtypes = [C.c_char_p, C.POINTER(C.c_int64)]
    lib.tpw_pc_run.restype = C.c_int32
    blob = encode(case)
    out = (C.c_int64 * 256)()
    CL.host_begin(lib, {})
    try:
        count = lib.tpw_pc_run(blob, out)
    finally:
        log = CL.host_end(lib)
    return dict(state=list(out[:count]), log=log)


# ======================================================================= the PowerPC side


class Us:
    """One case on the PowerPC side."""

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.blocks = {}          # address -> (size, number in the call; -1 for the set-up's)
        self.allocations = 0
        self.temp = 0             # the temporary buffer
        self.store = 0            # the loader's pixel store
        self.pixels_info = {}     # UPixels object -> [data, width, height, flipped]
        self.pictures = {p["path"]: p for p in case["pictures"]}
        self.find_ptrs = []
        self.find_next = 0
        self.subject = 0


def u32(e, address):
    return e.u32(address)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


def xor5a(data):
    return (int.from_bytes(data, "big") ^ int.from_bytes(b"\x5a" * len(data), "big")).to_bytes(len(data), "big") if data else b""


def build_stubs(e):
    S = CL.Stubs()
    S.real.update(ENTRY)
    us = lambda emu: emu.us   # noqa: E731

    def rec(emu, name, ints=(), texts=()):
        emu.cl.rec(name, ints, texts)

    # ---- the heap
    def op_new(emu):
        size = emu.r(3)
        if size == 8:
            rec(emu, "new", (8,))
        emu.set_r(3, emu.alloc(max(size, 4), b"\xa5" * max(size, 4)))

    def op_new_array(emu):
        size = emu.r(3)
        u = us(emu)
        u.allocations += 1
        number = u.allocations
        failed = number in u.case["fail"]
        rec(emu, "new[]", (number, size, int(failed)))
        if failed:
            emu.set_r(3, 0)
            return
        block = emu.alloc(max(size, 4), b"\xa5" * max(size, 4))
        u.blocks[block] = (size, number)
        emu.set_r(3, block)

    def op_delete_array(emu):
        block = emu.r(3)
        entry = us(emu).blocks.pop(block, None)
        rec(emu, "delete[]", (entry[1] if entry is not None else -2, entry[0] if entry is not None else 0))

    S.imp("__nw__FUl", op_new)
    S.imp("__nwa__FUl", op_new_array)
    S.imp("__dla__FPv", op_delete_array)
    S.imp("__dl__FPv", lambda emu: None)

    def strcat(emu):
        dest, source = emu.r(3), emu.r(4)
        tail = len(emu.cstr(dest, limit=1 << 20).encode("latin-1"))
        emu.write(dest + tail, emu.cstr(source, limit=1 << 20).encode("latin-1") + b"\0")

    S.imp("strcat", strcat)

    # ---- the loader
    def render_area_ctor(emu):
        rec(emu, "RenderArea.ctor", (emu.cl.newlab(emu.r(3)),))
        emu.write(emu.r(3) + 0x10, bytes(0x18))

    def alloc_memory(emu):
        area = emu.r(4)
        width, depth = s32(u32(emu, area + 0x1C)), s32(u32(emu, area + 0x10))
        stride = s32(width * ((depth + 7) >> 3))
        w32(emu, area + 0x24, stride)
        rec(emu, "AllocMemory", (emu.cl.lab(area), stride, s32(u32(emu, area + 0x14)), s32(u32(emu, area + 0x20))))
        u = us(emu)
        if not u.store:
            u.store = emu.alloc(STORE_SIZE)
        emu.write(u.store, bytes(STORE_SIZE))
        w32(emu, area + 0x18, u.store)
        w32(emu, emu.r(3), 0)

    def read_header(emu):
        ret, loader, path, second, area = emu.r(3), emu.r(4), emu.r(5), emu.r(6), emu.r(7)
        text = emu.cstr(path)
        rec(emu, "ReadHeader", (emu.cl.lab(loader), emu.cl.lab(area), int(second == 0)), (text[:159],))
        picture = us(emu).pictures.get(text)
        if picture is None:
            w32(emu, ret, MINUS)
            return
        w32(emu, area + 0x10, picture["depth"])
        w32(emu, area + 0x14, picture["palettised"])
        w32(emu, area + 0x1C, picture["width"])
        w32(emu, area + 0x20, picture["height"])
        w32(emu, ret, picture["header"])

    def load(emu):
        ret, loader, area, path, second, other = [emu.r(i) for i in range(3, 9)]
        text = emu.cstr(path)
        rec(emu, "Load", (emu.cl.lab(loader), emu.cl.lab(area), int(second == 0), int(other == 0)), (text[:159],))
        picture = us(emu).pictures.get(text)
        if picture is None:
            w32(emu, ret, MINUS)
            return
        stride, height, data = s32(u32(emu, area + 0x24)), s32(u32(emu, area + 0x20)), u32(emu, area + 0x18)
        count = min(len(picture["pixels"]), max(0, stride * height), STORE_SIZE)
        if data and count:
            emu.write(data, picture["pixels"][:count])
        w32(emu, ret, picture["load"])

    S.imp("__ct__12TbRenderAreaFv", render_area_ctor)
    S.imp("AllocMemory__12TbRenderAreaFv", alloc_memory)
    S.imp("FreeMemory__12TbRenderAreaFv", lambda emu: rec(emu, "FreeMemory", (emu.cl.lab(emu.r(3)),)))
    S.imp("__ct__11TbTGALoaderFv", lambda emu: rec(emu, "TGALoader.ctor", (emu.cl.newlab(emu.r(3)),)))
    S.imp("__dt__11TbTGALoaderFv", lambda emu: rec(emu, "TGALoader.dtor", (emu.cl.lab(emu.r(3)),)))
    S.imp("ReadHeader__19TbGraphicFileLoaderFPCcPCcP12TbRenderArea", read_header)
    S.imp("Load__19TbGraphicFileLoaderFRC12TbRenderAreaPCcPCcP12TbRenderArea", load)

    # ---- the file finder and UDebug
    def find_file_path(emu):
        name = emu.cstr(emu.r(3))
        rec(emu, "FindFilePath", (emu.r(4), int(emu.r(5) != 0)), (name[:159],))
        u = us(emu)
        if not u.find_ptrs:
            emu.set_r(3, 0)
            return
        index = u.find_next if u.find_next < len(u.find_ptrs) else len(u.find_ptrs) - 1
        if u.find_next < len(u.find_ptrs):
            u.find_next += 1
        emu.set_r(3, u.find_ptrs[index])

    S.internal(FIND_FILE_PATH, find_file_path)
    S.internal(UDEBUG, lambda emu: rec(emu, "UDebug", (), (emu.cstr(emu.r(3))[:159],)))

    # ---- the pixel converter
    def pixels_wrap(emu):
        this, fmt, data, width, height, stride, palette = [emu.r(i) for i in range(3, 10)]
        u = us(emu)
        subject_pixels = u32(emu, u.subject + 0xC)
        which = 1 if data == u.temp else 2 if data == subject_pixels else 0
        crc = zlib.crc32(emu.read(data, s32(width) * s32(height) * 4)) if data == u.temp else -1
        rec(emu, "UPixels.wrap", (emu.cl.newlab(this), s32(fmt), which, crc, s32(width), s32(height), s32(stride), int(palette == 0)))
        u.pixels_info[this] = [data, s32(width), s32(height), False]

    def set_flipped(emu):
        this, flag = emu.r(3), emu.r(4) & 0xFF
        rec(emu, "UPixels.SetFlipped", (emu.cl.lab(this), flag))
        us(emu).pixels_info[this][3] = flag != 0

    def convert(emu):
        this, source, dither = emu.r(3), emu.r(4), emu.r(5) & 0xFF
        info = us(emu).pixels_info
        flipped = info[source][3]
        rec(emu, "UPixels.Convert", (emu.cl.lab(this), emu.cl.lab(source), dither, int(flipped)))
        data, width, height = info[this][:3]
        source_data = info[source][0]
        row_bytes = width * 4
        rows = [emu.read(source_data + (height - 1 - row if flipped else row) * row_bytes, row_bytes) for row in range(height)]
        emu.write(data, xor5a(b"".join(rows)))

    S.imp("__ct__Q213SamsUtilities7UPixelsFQ213SamsUtilities11PixelFormatPvUlUlUlPQ213SamsUtilities6UPixel", pixels_wrap)
    S.imp("SetFlipped__Q213SamsUtilities7UPixelsFUc", set_flipped)
    S.imp("Convert__Q213SamsUtilities7UPixelsFRCQ213SamsUtilities7UPixelsUc", convert)
    S.imp("__dt__Q213SamsUtilities7UPixelsFv", lambda emu: rec(emu, "UPixels.dtor", (emu.cl.lab(emu.r(3)),)))

    # ---- the temporary allocator: its table's two entries are persistent fake functions
    def allocate(emu):
        u = us(emu)
        rec(emu, "Allocate", (emu.cl.lab(emu.r(3)), emu.r(4)))
        if u.case["alloc_null"]:
            emu.set_r(3, 0)
            return
        u.temp = emu.alloc(TEMP_SIZE + OUT_GUARD, b"\xa5" * (TEMP_SIZE + OUT_GUARD))
        emu.set_r(3, u.temp)

    def free(emu):
        u = us(emu)
        reference = emu.r(4)
        pointer = u32(emu, reference)
        is_temp = pointer != 0 and pointer == u.temp
        rec(emu, "Free", (emu.cl.lab(emu.r(3)), int(is_temp), zlib.crc32(emu.read(pointer, TEMP_SIZE)) if is_temp else -1))
        w32(emu, reference, 0)

    impl = T.persist_alloc(e, 0x20)
    e.write(impl + 0xC, struct.pack(">I", T.fake_function(e, allocate)))
    e.write(impl + 0x14, struct.pack(">I", T.fake_function(e, free)))
    e.pc_impl = impl

    def allocator_ctor(emu):
        this = emu.r(3)
        rec(emu, "UAllocatorTemporary.ctor", (emu.cl.newlab(this),))
        w32(emu, this + 4, emu.pc_impl)

    S.imp("__ct__Q213SamsUtilities19UAllocatorTemporaryFv", allocator_ctor)

    # ---- OpenGL
    def read_pixels(emu):
        x, y, width, height = s32(emu.r(3)), s32(emu.r(4)), s32(emu.r(5)), s32(emu.r(6))
        fmt, kind, pixels = emu.r(7), emu.r(8), emu.r(9)
        u = us(emu)
        rec(emu, "glReadPixels", (x, y, width, height, fmt, kind, int(pixels != 0), int(pixels == u32(emu, u.subject + 0xC))))
        if pixels == 0 or width <= 0 or height <= 0:
            return
        count = width * height * 4
        pattern = u.case["pattern"]
        emu.write(pixels, (pattern * (count // len(pattern) + 1))[:count])

    S.imp("glReadBuffer", lambda emu: rec(emu, "glReadBuffer", (emu.r(3),)))
    S.imp("glDrawBuffer", lambda emu: rec(emu, "glDrawBuffer", (emu.r(3),)))
    S.imp("glReadPixels", read_pixels)
    return S


STUBS = None


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    STUBS = build_stubs(e)


def ppc(e, case):
    fn = case["fn"]
    e.cl = CL.PpcLog({})
    us = Us(e, case)
    e.us = us
    display = e.data_imports["gei_Display"]
    cell = e.data_imports["gMemoryTemporary__13SamsUtilities"]
    saved = (bytes(e.read(display + 8, 8)), bytes(e.read(cell, 4)))
    old = STUBS.activate(e)
    try:
        pc = e.alloc(0x70, b"\xa5" * 0x70)
        us.subject = pc + 0x60 if fn == GRAB else pc
        if fn != POSTCARD_CTOR:
            for i, (width, height, bpp, pixels) in enumerate(case["bitmaps"]):
                pointer = 0
                if pixels is not None:
                    pointer = e.alloc(max(len(pixels), 4), bytes(pixels) + b"\0" * max(0, 4 - len(pixels)))
                    us.blocks[pointer] = (len(pixels), -1)
                e.write(pc + 16 * i, struct.pack(">IIII", width, height, bpp, pointer))
        out_size = case["out"]
        out = e.alloc(out_size + OUT_GUARD, b"\xa5" * (out_size + OUT_GUARD))
        w32(e, display + 8, case["display"][0])
        w32(e, display + 0xC, case["display"][1])
        w32(e, cell, 0)
        if case["alloc"]:
            block = e.alloc(8, b"\xa5" * 8)
            w32(e, block + 4, e.pc_impl)
            e.cl.newlab(block)   # the host's constructor gave it a label
            w32(e, cell, block)
        us.find_ptrs = [e.alloc(len(p) + 1, p.encode("latin-1") + b"\0") for p in case["finds"]]
        a = case["args"]
        prefix = e.alloc(len(case["prefix"]) + 1, case["prefix"].encode("latin-1") + b"\0") if case["prefix"] is not None else 0
        name = e.alloc(len(case["name"]) + 1, case["name"].encode("latin-1") + b"\0") if case["name"] is not None else 0
        this = pc + 0x60 if fn == GRAB else pc
        arguments = {
            BITMAP_CTOR: [this, a[0], a[1], a[2]], BITMAP_DTOR: [this, 0], BITMAP_CLEAR: [this],
            LOAD_FROM_TGA: [this, prefix, name], COPY_FROM_OPENGL: [this], ALPHABLT: [this, pc + 0x10, a[0], a[1]],
            RESIZE: [this, out, a[0], a[1], a[2]], POSTCARD_CTOR: [pc], POSTCARD_DTOR: [pc, 0], POSTCARD_CLEAR: [pc],
            LOAD_SOURCES: [pc], GRAB: [pc, 0], GENERATE: [pc], PREVIEW: [pc, out, a[0], a[1], a[2]],
        }[fn]
        result = T.call_ppc(e, ENTRY[fn], arguments)
        state = [s32(result[0]) if fn in RETURNS else 0]
        for i in range(BITMAPS):
            width, height, bpp, pointer = struct.unpack(">IIII", e.read(pc + 16 * i, 16))
            state += [width, height, bpp]
            if pointer == 0:
                state += [0, 0, 0, 0]
                continue
            size = us.blocks.get(pointer, (0, 0))[0]
            data = e.read(pointer, size)
            state += [1, size, zlib.crc32(data), signed64(int.from_bytes(data[:8], "big"))]
        state.append(zlib.crc32(e.read(out, out_size + OUT_GUARD)))
        state.append(int(u32(e, cell) != 0))
        return dict(state=state, log=e.cl.log)
    finally:
        e.write(display + 8, saved[0])
        e.write(cell, saved[1])
        CL.Stubs.deactivate(e, old)


# ======================================================================= case generators


def pixels(r, count):
    return r.randbytes(count)


def bitmap(r, width, height, bpp=4, present=True, data_bpp=None):
    """A bitmap with random pixels: bpp is the bytes-a-pixel word, data_bpp the bytes a pixel the data really has."""
    return (width, height, bpp, pixels(r, width * height * (data_bpp or bpp)) if present else None)


def picture(r, path, width, height, kind="full", header=0, load=0, extra=0):
    """A picture the loader serves: kind full (32 bits, palettised word 0), eight (8 bits, palettised 1) or odd (24 bits, 2)."""
    depth, palettised = {"full": (32, 0), "eight": (8, 1), "odd": (24, 2), "zero": (32, 0)}[kind]
    stride = width * ((depth + 7) >> 3)
    return dict(path=path, depth=depth, palettised=palettised, width=width, height=height, header=header, load=load,
                pixels=pixels(r, max(0, stride * height + extra)))


ALPHABET = "abcdefghijklmnopqrstuvwxyz:._-/0123456789"


def word(r, low, high):
    return "".join(r.choice(ALPHABET) for _ in range(r.randint(low, high)))


# ---- PostcardBitmap::PostcardBitmap, ~PostcardBitmap, clear
def ctor_edges():
    r = random.Random(1)
    for args in [(0, 0, 0), (1, 1, 1), (4, 3, 4), (640, 240, 4), (0, 5, 4), (5, 0, 4), (7, 3, 1), (65536, 65537, 4), (65536, 65536, 4)]:
        yield make_case(BITMAP_CTOR, args=args)
    yield make_case(BITMAP_CTOR, args=(3, 3, 4), fail=[1])
    yield make_case(BITMAP_CTOR, bitmaps=[bitmap(r, 4, 4)], args=(2, 2, 2))   # an old bitmap there (the constructor does not look)


def ctor_random(r):
    return make_case(BITMAP_CTOR, args=(r.randint(0, 40), r.randint(0, 40), r.choice([0, 1, 2, 3, 4, 8])),
                     fail=[1] if r.random() < 0.1 else [])


def single_bitmap(r, junk_words=False):
    """A bitmap with or without pixels; its words are anything."""
    present = r.random() < 0.7
    if r.random() < 0.5:
        width, height, bpp = r.randint(0, 10), r.randint(0, 10), r.choice([1, 2, 4])
        return bitmap(r, width, height, bpp, present)
    width, height, bpp = r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)
    return (width, height, bpp, pixels(r, r.randint(0, 64)) if present else None)


def dtor_edges():
    r = random.Random(2)
    yield make_case(BITMAP_DTOR)
    yield make_case(BITMAP_DTOR, bitmaps=[bitmap(r, 5, 4)])
    yield make_case(BITMAP_DTOR, bitmaps=[(7, 7, 4, None)])


def dtor_random(r):
    return make_case(BITMAP_DTOR, bitmaps=[single_bitmap(r)])


def clear_edges():
    r = random.Random(3)
    yield make_case(BITMAP_CLEAR)
    yield make_case(BITMAP_CLEAR, bitmaps=[bitmap(r, 5, 4)])
    yield make_case(BITMAP_CLEAR, bitmaps=[(7, 7, 4, None)])


def clear_random(r):
    return make_case(BITMAP_CLEAR, bitmaps=[single_bitmap(r)])


# ---- PostcardBitmap::load_from_tga
def tga_case(r, prefix, name, width, height, kind="full", header=0, load=0, found=True, existing=None, fail=(), extra=0,
             path=None):
    full = (prefix or "") + (name or "")
    pictures = [picture(r, path if path is not None else full, width, height, kind, header, load, extra)] if found else []
    return make_case(LOAD_FROM_TGA, bitmaps=[existing] if existing is not None else [], pictures=pictures, prefix=prefix, name=name,
                     fail=fail)


def tga_edges():
    r = random.Random(4)
    yield make_case(LOAD_FROM_TGA)                                  # NULL path, NULL name
    yield make_case(LOAD_FROM_TGA, name="x.tga")                    # NULL path
    for kind in ("full", "eight", "odd"):
        yield tga_case(r, "", "data:postcard:a.tga", 4, 3, kind)
        yield tga_case(r, "data:", "x.tga", 1, 1, kind)
        yield tga_case(r, "a", "b", 7, 5, kind, existing=bitmap(r, 3, 3))
        yield tga_case(r, "", "n.tga", 0, 5, kind)
        yield tga_case(r, "", "n.tga", 5, 0, kind, existing=bitmap(r, 2, 2))
    yield tga_case(r, "", "m.tga", 3, 3, found=False)                                  # the file is not there
    yield tga_case(r, "", "m.tga", 3, 3, header=-1)                                    # the header fails
    yield tga_case(r, "", "m.tga", 3, 3, header=-7)
    yield tga_case(r, "", "m.tga", 3, 3, header=1)                                     # positive answers pass
    yield tga_case(r, "", "m.tga", 3, 3, load=-1, existing=bitmap(r, 2, 2))            # the pixels fail: the area is freed
    yield tga_case(r, "", "m.tga", 3, 3, load=5)
    yield tga_case(r, "", "m.tga", 3, 3, existing=bitmap(r, 2, 2), fail=[1])           # no memory: the old pixels are gone
    yield tga_case(r, "", "m.tga", 3, 3, "eight", existing=(2, 2, 1, None), fail=[1])
    yield tga_case(r, "", "m.tga", 3, 3, extra=50)                                     # the file has more bytes than the area
    yield tga_case(r, "", "m.tga", 6, 6, extra=-30)                                    # fewer: the store's zeros follow
    yield tga_case(r, "", "m.tga", 64, 64)
    yield tga_case(r, "", "m.tga", 256, 256, "eight")
    yield tga_case(r, "a" * 100, "b" * 120, 2, 2)                                      # a long path (255 characters)


def tga_random(r):
    prefix = r.choice(["", "", "data:", "x/", None if r.random() < 0.1 else word(r, 0, 30)])
    name = word(r, 1, 40)
    kind = r.choice(["full", "full", "eight", "odd"])
    width, height = r.randint(0, 24), r.randint(0, 24)
    existing = single_bitmap(r) if r.random() < 0.5 else None
    return tga_case(r, prefix, name, width, height, kind, header=r.choice([0, 0, 0, -1, 3, -9]), load=r.choice([0, 0, 0, -1, 2, -4]),
                    found=r.random() < 0.9, existing=existing, fail=[1] if r.random() < 0.1 else [], extra=r.randint(-20, 20),
                    path=word(r, 1, 20) if r.random() < 0.05 else None)


# ---- PostcardBitmap::copy_from_opengl and Postcard::grab
def grab_case(r, fn, width, height, alloc=0, alloc_null=0, fail=(), existing=None, pattern=None):
    existing = existing or (0, 0, 0, None)
    if 1 in fail and existing[0] != 0 and existing[1] != 0:
        # the first allocation fails: the old words stay, the pixels are NULL and the scaling reads them (a crash if both are
        # non-zero, on the Mac too)
        existing = (0, existing[1], existing[2], None)
    bitmaps = [(0, 0, 0, None)] * 6 + [existing] if fn == GRAB else [existing]
    return make_case(fn, bitmaps=bitmaps, display=(width, height), alloc=alloc, alloc_null=alloc_null, fail=fail,
                     pattern=pattern or r.randbytes(PATTERN_SIZE))


def opengl_edges(fn):
    def edges():
        r = random.Random(5 + fn)
        for size in [(320, 240), (640, 480), (160, 120), (321, 241), (1, 1), (0, 0), (8, 6), (319, 240), (640, 240), (800, 600),
                     (2, 600), (333, 77)]:
            yield grab_case(r, fn, *size)
            yield grab_case(r, fn, *size, alloc=1)
        yield grab_case(r, fn, 40, 30, alloc_null=1)
        yield grab_case(r, fn, 40, 30, alloc=1, alloc_null=1)
        for number in range(1, 7):
            yield grab_case(r, fn, 40, 30, fail=[number])
        yield grab_case(r, fn, 40, 30, fail=[1, 5])
        yield grab_case(r, fn, 40, 30, fail=[2, 3, 4])
        yield grab_case(r, fn, 40, 30, existing=bitmap(r, 6, 6))
        yield grab_case(r, fn, 40, 30, existing=(6, 6, 4, None))
        yield grab_case(r, fn, 0, 9, existing=bitmap(r, 2, 2))
    return edges


def opengl_random(fn):
    def rnd(r):
        size = r.choice([(r.randint(0, 60), r.randint(0, 60)), (r.randint(100, 400), r.randint(40, 300))])
        existing = single_bitmap(r) if r.random() < 0.4 else None
        fail = [r.randint(1, 6)] if r.random() < 0.15 else []
        return grab_case(r, fn, *size, alloc=r.randint(0, 1), alloc_null=int(r.random() < 0.1), fail=fail, existing=existing)
    return rnd


# ---- PostcardBitmap::alphablt
def blt_case(r, source, dest, x, y, dest_bpp=4, slack=0):
    sw, sh, sbpp, spix = source
    dw, dh = dest
    start = dest_bpp * (x + dw * y)
    end = start + (max(sh, 1) - 1) * (4 * sw + dest_bpp * (dw - sw)) + 4 * sw if sw and sh else start
    size = max(4 * dw * dh, end, start) + slack + 8
    return make_case(ALPHABLT, bitmaps=[source, (dw, dh, dest_bpp, pixels(r, size))], args=(x, y))


def blt_edges():
    r = random.Random(7)
    yield blt_case(r, bitmap(r, 3, 2), (8, 6), 0, 0)
    yield blt_case(r, bitmap(r, 3, 2), (8, 6), 5, 4)
    yield blt_case(r, bitmap(r, 8, 6), (8, 6), 0, 0)
    yield blt_case(r, bitmap(r, 1, 1), (1, 1), 0, 0)
    yield blt_case(r, bitmap(r, 0, 3), (8, 6), 2, 2)
    yield blt_case(r, bitmap(r, 3, 0), (8, 6), 2, 2)
    yield blt_case(r, (0, 3, 4, None), (8, 6), 7, 5)
    for sbpp in (1, 2, 3):
        yield blt_case(r, bitmap(r, 3, 2, sbpp, data_bpp=4), (8, 6), 1, 1)
    for dbpp in (1, 2, 3):
        yield blt_case(r, bitmap(r, 3, 2), (8, 6), 1, 1, dbpp, slack=64)
    yield blt_case(r, bitmap(r, 6, 3), (4, 10), 0, 0)                        # wider than the dest: the rows run into the next
    yield blt_case(r, bitmap(r, 5, 2), (3, 9), 1, 1)
    # alpha 0, 255 and middling, light over dark and dark over light
    for alpha in (0, 1, 127, 128, 254, 255):
        for source_value, dest_value in ((0, 255), (255, 0), (10, 200), (200, 10), (128, 128), (0, 0), (255, 255)):
            source = (2, 2, 4, bytes([alpha, source_value, 255 - source_value, source_value // 2] * 4))
            dest = (4, 3, 4, bytes([dest_value, dest_value, 255 - dest_value, dest_value // 3] * 12))
            yield make_case(ALPHABLT, bitmaps=[source, dest], args=(1, 1))
    # the real sizes: the card
    yield blt_case(r, bitmap(r, 100, 90), (640, 240), 4, 0x9F)
    yield blt_case(r, bitmap(r, 96, 20), (640, 240), 0x220, 0)
    yield blt_case(r, bitmap(r, 163, 234), (640, 240), 0x1DD, 6)
    yield blt_case(r, bitmap(r, 319, 22), (640, 240), 0x141, 0xDA)


def blt_random(r):
    sw, sh = r.randint(0, 12), r.randint(0, 12)
    dw, dh = r.randint(max(sw, 1), 16), r.randint(max(sh, 1), 16)
    x, y = r.randint(0, dw - sw), r.randint(0, dh - sh)
    source = bitmap(r, sw, sh, r.choice([4, 4, 4, 1, 3]), data_bpp=4)
    if r.random() < 0.1:   # the source wider than the dest
        sw, dw = r.randint(5, 9), r.randint(2, 4)
        dh = sh + r.randint(1, 4) + 3
        x, y = 0, 0
        source = bitmap(r, sw, sh)
    dest_bpp = 4 if r.random() < 0.85 else r.choice([1, 2, 3])
    return blt_case(r, source, (dw, dh), x, y, dest_bpp, slack=64)


# ---- PostcardBitmap::resize_into_buffer and Postcard::generate_preview_into_buffer
def out_bytes(width, height, fmt):
    return width * height * {1: 4, 2: 2, 3: 4}.get(fmt, 0)


def resize_case(r, fn, source, width, height, fmt, fail=()):
    bitmaps = [source] if fn == RESIZE else [(0, 0, 0, None)] * 5 + [source]
    return make_case(fn, bitmaps=bitmaps, args=(width, height, fmt), out=out_bytes(max(width, 0), max(height, 0), fmt), fail=fail)


def resize_edges(fn):
    def edges():
        r = random.Random(11 + fn)
        for fmt in (0, 1, 2, 3, 4, -1, 99):
            yield resize_case(r, fn, bitmap(r, 8, 6), 4, 3, fmt)
        for source, out in [((8, 6), (8, 6)), ((8, 6), (1, 1)), ((8, 6), (2, 2)), ((8, 6), (3, 5)), ((5, 7), (2, 3)), ((1, 1), (1, 1)),
                            ((9, 9), (4, 4)), ((16, 8), (5, 3)), ((6, 4), (13, 9)), ((3, 3), (10, 7)), ((1, 1), (5, 5)),
                            ((7, 5), (7, 1)), ((7, 5), (1, 5)), ((100, 80), (33, 17))]:
            for fmt in (1, 2, 3):
                yield resize_case(r, fn, bitmap(r, *source), *out, fmt)
        for size in [(640, 240), (320, 240), (160, 120)]:
            for out in [(320, 240), (160, 120), (1, 1)]:
                yield resize_case(r, fn, bitmap(r, *size), *out, 3)
        yield resize_case(r, fn, bitmap(r, 640, 240), 320, 240, 1)
        yield resize_case(r, fn, bitmap(r, 640, 240), 80, 60, 2)
        yield resize_case(r, fn, bitmap(r, 640, 480), 320, 240, 3)
        yield resize_case(r, fn, bitmap(r, 300, 2), 1, 1, 3)                # a run of 300 wraps to 44 in its byte
        yield resize_case(r, fn, bitmap(r, 2, 300), 1, 1, 3)
        yield resize_case(r, fn, bitmap(r, 1024, 2), 2, 1, 3)               # 512 wraps to 0
        yield resize_case(r, fn, bitmap(r, 600, 40), 2, 1, 1)
        yield resize_case(r, fn, bitmap(r, 255, 3), 1, 1, 2)
        yield resize_case(r, fn, bitmap(r, 256, 3), 1, 1, 2)
        yield resize_case(r, fn, bitmap(r, 257, 3), 1, 1, 2)
        yield resize_case(r, fn, (0, 0, 4, None), 4, 4, 3)                  # nothing to read
        yield resize_case(r, fn, (0, 5, 4, None), 4, 4, 1)
        yield resize_case(r, fn, (5, 0, 4, None), 4, 4, 2)
        yield resize_case(r, fn, bitmap(r, 4, 4), 0, 3, 3)                  # no columns
        yield resize_case(r, fn, bitmap(r, 4, 4), 3, 0, 3)                  # no rows (the divisor 0)
        yield resize_case(r, fn, bitmap(r, 4, 4), 0, 0, 3)
        for number in (1, 2, 3):
            yield resize_case(r, fn, bitmap(r, 8, 6), 4, 3, 3, fail=[number])
        yield resize_case(r, fn, bitmap(r, 8, 6), 4, 3, 3, fail=[2, 3])
    return edges


def resize_random(fn):
    def rnd(r):
        sw, sh = r.randint(1, 40), r.randint(1, 40)
        if r.random() < 0.3:
            width, height = r.randint(1, sw), r.randint(1, sh)
        else:
            width, height = r.randint(0, 50), r.randint(1, 50)
        return resize_case(r, fn, bitmap(r, sw, sh), width, height, r.choice([0, 1, 2, 3, 3, 3, 4, -2]),
                           fail=[r.randint(1, 3)] if r.random() < 0.08 else [])
    return rnd


# ---- the Postcard
def postcard_bitmaps(r, present=0.7):
    out = []
    for i in range(BITMAPS):
        out.append(single_bitmap(r) if r.random() < present else (0, 0, 0, None))
    return out


def ctor_postcard_edges():
    yield make_case(POSTCARD_CTOR)


def ctor_postcard_random(r):
    return make_case(POSTCARD_CTOR)


def postcard_dtor_edges(fn):
    def edges():
        r = random.Random(20 + fn)
        yield make_case(fn)
        yield make_case(fn, bitmaps=[bitmap(r, 2 + i, 2) for i in range(BITMAPS)])
        yield make_case(fn, bitmaps=[(3, 3, 4, None)] * BITMAPS)
        yield make_case(fn, bitmaps=[bitmap(r, 2 + i, 2) if i % 2 else (0, 0, 0, None) for i in range(BITMAPS)])
        yield make_case(fn, bitmaps=[bitmap(r, 2 + i, 3) if i % 2 == 0 else (0, 0, 0, None) for i in range(BITMAPS)])
    return edges


def postcard_dtor_random(fn):
    return lambda r: make_case(fn, bitmaps=postcard_bitmaps(r))


# load_sources: five names, each answered by FindFilePath, then the card and the grab
def sources_case(r, present=(1, 1, 1, 1, 1), fail=(), existing=None, kinds=None, headers=None):
    finds = [word(r, 3, 30) for _ in range(5)]
    pictures = []
    for i in range(5):
        if present[i]:
            kind = (kinds or ["full"] * 5)[i]
            pictures.append(picture(r, finds[i], r.randint(1, 9), r.randint(1, 9), kind, header=(headers or [0] * 5)[i]))
    return make_case(LOAD_SOURCES, bitmaps=existing or [], pictures=pictures, finds=finds, fail=fail)


def sources_edges():
    r = random.Random(30)
    yield sources_case(r)
    yield sources_case(r, (0, 0, 0, 0, 0))
    yield sources_case(r, (1, 0, 1, 0, 1))
    yield sources_case(r, kinds=["eight", "odd", "full", "eight", "full"])
    yield sources_case(r, headers=[-1, 0, 0, -1, 0])
    for number in range(1, 7):
        yield sources_case(r, fail=[number])
    yield sources_case(r, fail=[6, 3])
    yield sources_case(r, fail=[1, 2, 3, 4, 5, 6])
    yield sources_case(r, existing=[bitmap(r, 3 + i, 3) for i in range(BITMAPS)])
    yield sources_case(r, existing=[bitmap(r, 3 + i, 3) for i in range(BITMAPS)], fail=[6])
    yield sources_case(r, existing=[(2, 2, 4, None)] * 5 + [(9, 9, 4, None), (5, 5, 4, None)], fail=[6])
    yield sources_case(r, existing=[bitmap(r, 2, 2), (0, 0, 0, None), bitmap(r, 1, 1), (0, 0, 0, None), bitmap(r, 4, 1)])
    # the same path for two pictures, a path that is the name of another
    finds = ["data:p:a", "data:p:a", "b", "b", "c"]
    yield make_case(LOAD_SOURCES, finds=finds, pictures=[picture(r, "data:p:a", 3, 2), picture(r, "b", 2, 2, "eight")])
    yield make_case(LOAD_SOURCES, finds=["only"], pictures=[picture(r, "only", 2, 2)])      # the last answer repeats


def sources_random(r):
    present = [int(r.random() < 0.8) for _ in range(5)]
    kinds = [r.choice(["full", "full", "eight", "odd"]) for _ in range(5)]
    headers = [r.choice([0, 0, 0, -1]) for _ in range(5)]
    existing = postcard_bitmaps(r, 0.4) if r.random() < 0.5 else None
    fail = [r.randint(1, 6)] if r.random() < 0.15 else []
    return sources_case(r, present, fail, existing, kinds, headers)


# generate: the card's colour and the six pictures over it


def card_bitmaps(r, sizes, card=(640, 240)):
    """[logo, bullfrog, stamp, frank, legal, card, grab] with the given source sizes (logo, bullfrog, stamp, frank, legal, grab)."""
    logo, bullfrog, stamp, frank, legal, grab = sizes
    return [bitmap(r, *logo, r.choice([4, 4, 1, 2]), data_bpp=4), bitmap(r, *bullfrog), bitmap(r, *stamp), bitmap(r, *frank),
            bitmap(r, *legal), bitmap(r, *card), bitmap(r, *grab)]


def generate_case(r, fn, sizes, card=(640, 240), out=(8, 6, 3)):
    bitmaps = card_bitmaps(r, sizes, card)
    if fn == PREVIEW:
        return make_case(fn, bitmaps=bitmaps, args=out, out=out_bytes(out[0], out[1], out[2]))
    return make_case(fn, bitmaps=bitmaps)


def generate_edges():
    r = random.Random(40)
    full = [(458, 240), (636, 81), (96, 240), (163, 234), (319, 22), (640, 240)]
    yield generate_case(r, GENERATE, full)
    yield generate_case(r, GENERATE, [(0, 0)] * 6)
    yield generate_case(r, GENERATE, [(1, 1)] * 6)
    yield generate_case(r, GENERATE, [(30, 30), (30, 30), (30, 30), (30, 30), (30, 22), (30, 30)])
    yield generate_case(r, GENERATE, [(0, 5), (5, 0), (0, 0), (2, 2), (0, 9), (3, 3)])
    yield generate_case(r, GENERATE, [(5, 4)] * 6, card=(640, 240))
    yield generate_case(r, GENERATE, [(0, 0)] * 6, card=(4, 3))          # a small card: background only
    yield generate_case(r, GENERATE, [(0, 0)] * 6, card=(0, 0))
    yield generate_case(r, GENERATE, [(0, 0)] * 6, card=(0, 7))


def generate_random(r):
    if r.random() < 0.2:
        return generate_case(r, GENERATE, [(0, r.randint(0, 3))] * 6, card=(r.randint(0, 20), r.randint(0, 20)))
    sizes = [(r.randint(0, min(30, limit[0])), r.randint(0, min(30, limit[1]))) for limit in
             [(458, 240), (636, 81), (96, 240), (163, 234), (319, 22), (640, 240)]]
    return generate_case(r, GENERATE, sizes)


def preview_case(r, source, out, fmt, card_present=True, sizes=None, fail=()):
    bitmaps = [(0, 0, 0, None)] * 5 + [source]
    return make_case(PREVIEW, bitmaps=bitmaps, args=(out[0], out[1], fmt), out=out_bytes(out[0], out[1], fmt), fail=fail)


def preview_edges():
    r = random.Random(41)
    for fmt in (0, 1, 2, 3, 4):
        yield preview_case(r, bitmap(r, 16, 8), (4, 2), fmt)
    yield preview_case(r, bitmap(r, 640, 240), (160, 60), 3)
    yield preview_case(r, bitmap(r, 640, 240), (640, 240), 1)
    yield preview_case(r, bitmap(r, 12, 12), (30, 30), 2)
    yield preview_case(r, bitmap(r, 16, 8), (4, 2), 3, fail=[1])
    yield preview_case(r, bitmap(r, 16, 8), (4, 2), 3, fail=[3])
    # the card has no pixels: the original composes (from address 0) and then scales; only a card of no pixels and sources
    # that write nothing run without a crash
    yield make_case(PREVIEW, bitmaps=[(0, 0, 0, None)] * 7, args=(3, 2, 3), out=out_bytes(3, 2, 3))
    yield make_case(PREVIEW, bitmaps=[(0, 4, 4, None)] * 5 + [(0, 0, 0, None), (0, 3, 4, None)], args=(2, 2, 1), out=out_bytes(2, 2, 1))
    yield make_case(PREVIEW, bitmaps=[(0, 4, 4, None)] * 5 + [(5, 0, 4, None), (0, 3, 4, None)], args=(2, 2, 2), out=out_bytes(2, 2, 2))
    yield make_case(PREVIEW, bitmaps=[(3, 0, 4, None)] * 5 + [(0, 6, 4, None), (3, 0, 4, None)], args=(4, 3, 3), out=out_bytes(4, 3, 3))


def preview_random(r):
    if r.random() < 0.15:
        sources = [(r.choice([0, 3]), 0, 4, None) if r.random() < 0.5 else (0, r.randint(0, 5), 4, None) for _ in range(6)]
        card = r.choice([(0, 0, 0, None), (0, 4, 4, None), (5, 0, 4, None)])
        out = (r.randint(1, 8), r.randint(1, 8))
        fmt = r.choice([1, 2, 3])
        return make_case(PREVIEW, bitmaps=sources[:5] + [card, sources[5]], args=(out[0], out[1], fmt), out=out_bytes(out[0], out[1], fmt))
    sw, sh = r.randint(1, 40), r.randint(1, 40)
    out = (r.randint(1, 45), r.randint(1, 45))
    return preview_case(r, bitmap(r, sw, sh), out, r.choice([0, 1, 2, 3, 3, 4]), fail=[r.randint(1, 3)] if r.random() < 0.08 else [])


TESTS = [
    FnTest("PostcardBitmap::PostcardBitmap (0x100bf7c4)", ppc, host, edge=ctor_edges, rnd=ctor_random),
    FnTest("PostcardBitmap::~PostcardBitmap (0x100bf84c)", ppc, host, edge=dtor_edges, rnd=dtor_random),
    FnTest("PostcardBitmap::clear (0x100bf8d4)", ppc, host, edge=clear_edges, rnd=clear_random),
    FnTest("PostcardBitmap::load_from_tga (0x100bf954)", ppc, host, edge=tga_edges, rnd=tga_random),
    FnTest("PostcardBitmap::copy_from_opengl (0x100bfc3c)", ppc, host, edge=opengl_edges(COPY_FROM_OPENGL),
           rnd=opengl_random(COPY_FROM_OPENGL)),
    FnTest("PostcardBitmap::alphablt (0x100bff2c)", ppc, host, edge=blt_edges, rnd=blt_random),
    FnTest("PostcardBitmap::resize_into_buffer (0x100c0050)", ppc, host, edge=resize_edges(RESIZE), rnd=resize_random(RESIZE)),
    FnTest("Postcard::Postcard (0x100c03a4)", ppc, host, edge=ctor_postcard_edges, rnd=ctor_postcard_random),
    FnTest("Postcard::~Postcard (0x100c0478)", ppc, host, edge=postcard_dtor_edges(POSTCARD_DTOR), rnd=postcard_dtor_random(POSTCARD_DTOR)),
    FnTest("Postcard::clear (0x100c0658)", ppc, host, edge=postcard_dtor_edges(POSTCARD_CLEAR), rnd=postcard_dtor_random(POSTCARD_CLEAR)),
    FnTest("Postcard::load_sources (0x100c07c4)", ppc, host, edge=sources_edges, rnd=sources_random),
    FnTest("Postcard::grab (0x100c0954)", ppc, host, edge=opengl_edges(GRAB), rnd=opengl_random(GRAB)),
    FnTest("Postcard::generate (0x100c09b4)", ppc, host, edge=generate_edges, rnd=generate_random),
    FnTest("Postcard::generate_preview_into_buffer (0x100c0ac4)", ppc, host, edge=preview_edges, rnd=preview_random),
]
