"""Unit w27: the cursor classes and the picture loader, against libtpw_unit_CursorClasses.dylib (tests/unit_CursorClasses.cpp).

Main app ("SimTheme Park"):
  src/game/ui/InterfaceBFCursor.cpp  InterfaceBFCursor::~InterfaceBFCursor, GetVisible, Show, Hide, Use
  src/game/ui/InterfaceCursor.cpp, src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceDrawObject.cpp      InterfaceCursor (ten members) and CursorTable (six), unit w21's translation, tested again here
The bullfrog library ("bullfrog symbols shared"):
  src/bullfrog/AndyGLL.cpp, src/bullfrog/LbDrawText.cpp, src/bullfrog/Lbgfsave.cpp, src/bullfrog/TbRenderArea.cpp    TbRenderArea (eight members), TbGraphicFileLoader (seven), TbTGALoader (five), _TgaGLL (seven),
                             and, needed by them and not on the first list, _AndyGLL::DoRead and WriteLine and
                             LbPalette_FromCompact

Two worlds, each a script of operations run on both sides, the state dumped after every operation:

The loader world has a TGA loader, three picture areas, a line reader, two blocks of pixels, a palette block and a file.
Every callee outside the unit is a fake that records the same event on both sides: the file layer (one byte stream that
every file object serves, with its position), the library's heap (blocks filled with 0xa5 and a margin after each, a
run-length run can write past its line), the rectangle helpers, the surface and the pixel converter. The files are the
game's real cursor files (read from ROOT/extracted when the test runs, never copied) and TGA files the test builds: every
image type, depth and row order, short files, bad headers, colour maps, runs that cross the row. The PowerPC objects are
made with the original constructors in memory filled with 0xa5; the host's get the same fill.

The cursor world has six cursors (the base class or InterfaceBFCursor), three fake platform cursors and two cursor
tables; the platform's Show, Hide and IsVisible keep the library's nest count in both fakes.
"""
import ctypes as C
import os
import pathlib
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = os.environ.get("TPW_CURSORCLASSES_PROGRAM", "SimTheme Park")   # branchcov hooks this program's emulator: set it to the bullfrog library's name for that half
SHIM_LIBRARY = "unit_CursorClasses"
MAIN = "SimTheme Park"
BULLFROG = "bullfrog symbols shared"
MASK = 0xFFFFFFFF
ROOT_DIR = pathlib.Path(__file__).resolve().parents[2]
CURSOR_DIR = ROOT_DIR / "extracted/disc-hfs/Theme Park Data/data/ui/cursors"

# ---- the library's functions (bullfrog symbols shared)
RA_DEFAULT, RA_SURFACE, RA_FULL, RA_ALLOC, RA_FREE = 0x10016630, 0x10016560, 0x1001667C, 0x10016730, 0x100167F8
RA_CLIENT_POINT, RA_CLIENT_RECT, RA_MEMSIZE = 0x1001686C, 0x10016928, 0x10016AD0
LD_CTOR, LD_DTOR, LD_HEADER_PATH, LD_HEADER_FILE = 0x10022870, 0x100228E0, 0x1002295C, 0x10022B34
LD_LOAD_PATH, LD_LOAD_FILE, LD_VALID = 0x10022CD4, 0x10022E1C, 0x100230CC
TGA_CTOR, TGA_DTOR, TGA_HEADER, TGA_DATA, TGA_VALID = 0x10023A80, 0x10023AF0, 0x10023B70, 0x10023C14, 0x10023CC0
GLL_DATA, GLL_HEADER, GLL_FIRST, GLL_NEXT, GLL_REVERSE, GLL_RLE, GLL_LENGTH = (
    0x10021228, 0x10021330, 0x100217DC, 0x10021874, 0x10021A28, 0x10021B00, 0x10021D28)
DO_READ, WRITE_LINE, PALETTE = 0x1001DE44, 0x1001E2F4, 0x1001A2F8
UNIT_BULLFROG = [0x10023024, RA_DEFAULT, RA_SURFACE, RA_FULL, RA_ALLOC, RA_FREE, RA_CLIENT_POINT, RA_CLIENT_RECT, RA_MEMSIZE, LD_CTOR, LD_DTOR,
                 LD_HEADER_PATH, LD_HEADER_FILE, LD_LOAD_PATH, LD_LOAD_FILE, LD_VALID, TGA_CTOR, TGA_DTOR, TGA_HEADER, TGA_DATA,
                 TGA_VALID, GLL_DATA, GLL_HEADER, GLL_FIRST, GLL_NEXT, GLL_REVERSE, GLL_RLE, GLL_LENGTH, DO_READ, WRITE_LINE, PALETTE]
# what they call, in the library (the fakes)
FILE_CTOR, FILE_DTOR, FILE_READ, FILE_SEEK, FILE_SIZE = 0x1000B1DC, 0x1000B2B8, 0x10000F28, 0x1000B504, 0x1000B588
BUFFERED_DTOR = 0x10023024
WAD_OPEN, BUFFERED_OPEN, MEM_ALLOC, MEM_FREE, SURFACE_AREA = 0x1000CC48, 0x10049430, 0x10013EA4, 0x10013EFC, 0x10066E84
RECT_INTERSECTION, RECT_EMPTY = 0x10029AD0, 0x10029CD4
CONV_CTOR, CONV_INIT, CONV_BUF, CONV_RELEASE = 0x10016B10, 0x10016B4C, 0x10016E5C, 0x10016FF4
WAD_FILE_VTABLE, GLL_VTABLE = 0x10073038, 0x10072EB4
# the pixel formats the static initialiser fills (src/bullfrog/TbPixFmt.cpp)
FORMATS = {0x10073ED0: (0, 0, 0, 0, 8, 1), 0x10073EB8: (0x7C00, 0x3E0, 0x1F, 0, 16, 0), 0x10073EA0: (0xFF0000, 0xFF00, 0xFF, 0, 24, 0),
           0x10073E88: (0xFF0000, 0xFF00, 0xFF, 0xFF000000, 32, 0)}
# the words the library compares its results with: 0, and -1 for the one in ReadNextLine (that comparison is written the other way round)
RESULT_WORDS = {0x10072F94: 0, 0x10072F98: 0, 0x10072F9C: 0, 0x10072FA0: 0, 0x10072DFC: 0, 0x10072E64: MASK, 0x10072D84: 0, 0x10072D88: 0,
                0x10072D8C: 0}
HEADER_AT = 4 * 11 + 1 + 0x400      # where the loader's header bytes are in the world dump
MARGIN, BLOCK = 0x400, 0x4000
EXTENT = BLOCK + MARGIN
# the labels of the world (tests/unit_CursorClasses.cpp)
LAB_AREA, LAB_SURFACE, LAB_LOADER_AREA, LAB_GLL, LAB_FILE, LAB_BLOCK, LAB_PALETTE, LAB_LOADER_PALETTE = 2, 4, 5, 6, 7, 8, 10, 11
LAB_UCURSOR, LAB_CURSOR, LAB_TABLE = 20, 30, 50
FIRST_HEAP_LABEL = CL.FIRST_NEW

# ---- the main app's functions
CUR_CTOR, CUR_DTOR, CUR_SETDATA, CUR_SETHOT, CUR_SHOW, CUR_HIDE, CUR_GETVIS, CUR_DRAW, CUR_USE, CUR_ANIMATE = (
    0x101CC6A0, 0x101CC6F0, 0x101CC768, 0x101CC7A0, 0x101CC7DC, 0x101CC814, 0x101CC84C, 0x101CC888, 0x101CC8BC, 0x101CCC8C)
TAB_CTOR, TAB_DTOR, TAB_DEALLOC, TAB_ALLOC, TAB_SET, TAB_GET = 0x101CC994, 0x101CC9CC, 0x101CCA38, 0x101CCAFC, 0x101CCBB0, 0x101CCC34
BF_CTOR, BF_DTOR, BF_GETVIS, BF_SHOW, BF_HIDE, BF_USE, BF_LOAD = 0x101FE208, 0x101FE27C, 0x101FE338, 0x101FE390, 0x101FE404, 0x101FE478, 0x101FE52C
UNIT_MAIN = [CUR_CTOR, CUR_DTOR, CUR_SETDATA, CUR_SETHOT, CUR_SHOW, CUR_HIDE, CUR_GETVIS, CUR_DRAW, CUR_USE, CUR_ANIMATE, TAB_CTOR,
             TAB_DTOR, TAB_DEALLOC, TAB_ALLOC, TAB_SET, TAB_GET, BF_CTOR, BF_DTOR, BF_GETVIS, BF_SHOW, BF_HIDE, BF_USE, BF_LOAD]


def s32(v):
    return T.s32(v)


def u32(v):
    return v & MASK


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MASK))


def fnv(data):
    total = 0x811C9DC5
    for byte in data:
        total = ((total ^ byte) * 0x01000193) & MASK
    return total


def swap_header(raw):
    """The six halfwords of a TGA header (offsets 3, 5, 8, 10, 12, 14) with their bytes swapped; swapping twice undoes it."""
    raw = bytearray(raw)
    for at in (3, 5, 8, 10, 12, 14):
        raw[at], raw[at + 1] = raw[at + 1], raw[at]
    return bytes(raw)


def hostfn(lib, name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype = restype
    fn.argtypes = list(argtypes)
    return fn


class OnProgram(FnTest):
    """A free-form test that runs on the emulator of `program` (the unit lives in two programs of the original)."""

    def __init__(self, program, name, ppc, host, edge=None, rnd=None):
        super().__init__(name, ppc, host, edge, rnd)
        self.program = program

    def run_case(self, runner, case):
        runner.use(self.program)
        return super().run_case(runner, case)


def setup(runner):
    for program in (BULLFROG, MAIN):
        e = runner.use(program)
        T.install_std_stubs(e)

        def strncmp(emu):
            a, b, n = emu.r(3), emu.r(4), emu.r(5)
            result = 0
            for i in range(n):
                x, y = emu.read(a + i, 1)[0], emu.read(b + i, 1)[0]
                if x != y:
                    result = x - y
                    break
                if x == 0:
                    break
            emu.set_r(3, u32(result))

        e.stub("strncmp", strncmp)
    runner.use(PROGRAM)


# =========================================================================== the loader world, PowerPC side


class Bf:
    """One case of the loader world on the PowerPC side: the objects, the fakes and the operations."""

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.stream = bytes(case.get("stream", b""))
        self.pos = 0
        self.hdr_state = "raw"          # the loader's header buffer: never read, as read (swapped in place) or a mixture
        self.heap = {}                  # label -> (address, size) of the library's live blocks
        self.deletes = 0                # calls of operator delete (the loaders' deleting destructors)
        fill = bytes(case.get("fill", b""))[:BLOCK]
        self.blocks = [e.alloc(EXTENT, fill + b"\xa5" * (EXTENT - len(fill))), e.alloc(EXTENT, b"\xa5" * EXTENT)]
        self.palette = e.alloc(0x400, b"\xa5" * 0x400)
        self.areas = [e.alloc(0x28, b"\xa5" * 0x28) for _ in range(3)]
        self.loader = e.alloc(0x600, b"\xa5" * 0x600)
        self.gll = e.alloc(0x100, b"\xa5" * 0x100)
        self.file0 = e.alloc(16, b"\xa5" * 16)
        w32(e, self.file0, WAD_FILE_VTABLE)
        self.surface = e.alloc(8, b"\xa5" * 8)
        self.texts = []
        for text in case.get("texts", []):
            self.texts.append(e.alloc(len(text) + 1, text.encode("latin-1") + b"\0"))
        self.registry = [(self.blocks[0], EXTENT, LAB_BLOCK), (self.blocks[1], EXTENT, LAB_BLOCK + 1), (self.palette, 0x400, LAB_PALETTE),
                         (self.areas[0], 0x28, LAB_AREA), (self.areas[1], 0x28, LAB_AREA + 1), (self.areas[2], 0x28, LAB_SURFACE),
                         (self.loader, 0x600, 1), (self.gll, 0x100, LAB_GLL)]
        fixed = {self.areas[0]: LAB_AREA, self.areas[1]: LAB_AREA + 1, self.surface: LAB_SURFACE, self.loader + 4: LAB_LOADER_AREA,
                 self.gll: LAB_GLL, self.file0: LAB_FILE, self.blocks[0]: LAB_BLOCK, self.blocks[1]: LAB_BLOCK + 1,
                 self.palette: LAB_PALETTE, self.loader + 0x2C: LAB_LOADER_PALETTE}
        self.L = e.cl = CL.PpcLog(fixed, case.get("queues"))
        for address, words in FORMATS.items():
            e.write(address, struct.pack(">6I", *words))
        for address, value in RESULT_WORDS.items():
            w32(e, address, value)

    # ---- pointers, as the dumps describe them
    def describe(self, p):
        if p == 0:
            return [0, 0]
        for start, extent, label in self.registry:
            if start <= p <= start + extent:
                return [label, p - start]
        if p == 0xA5A5A5A5:
            return [-2, 0]
        for start, extent, label in self.registry:
            if label < FIRST_HEAP_LABEL and p < start and start - p <= 0x1000:
                return [label, p - start]
        return [-1, p & MASK]

    def area(self, index):
        return self.loader + 4 if index == 3 else self.areas[index]

    def block(self, index):
        return self.blocks[index - 1] if 1 <= index <= 2 else 0

    def text(self, index):
        return self.texts[index] if 0 <= index < len(self.texts) else 0

    def words(self, address, count):
        return list(struct.unpack(f">{count}I", self.e.read(address, 4 * count)))

    # ---- the fakes (installed for the case)
    def stubs(self):
        S = CL.Stubs()
        S.real.update(UNIT_BULLFROG)
        e, L = self.e, self.L
        rec = L.rec

        def file_ctor(e):
            rec("CFile.ctor", (L.newlab(e.r(3)),))

        def file_dtor(e):
            rec("CFile.dtor", (L.lab(e.r(3)),))

        def file_read(e):
            this, dest, size = e.r(3), e.r(4), e.r(5)
            rec("CFile.Read", (L.lab(this), size))
            count = min(size, max(0, len(self.stream) - self.pos))
            if dest and count > 0:
                e.write(dest, self.stream[self.pos:self.pos + count])
            self.pos += count
            answer = u32(L.ret("CFile.Read", count))
            if dest == self.loader + 0x430 and size == 0x12:
                if answer == 0x12:
                    self.hdr_state = "swapped"
                elif self.hdr_state != "raw":
                    self.hdr_state = "mixed"
            e.set_r(3, answer)

        def file_seek(e):
            this, offset, whence = e.r(3), s32(e.r(4)), e.r(5)
            rec("CFile.Seek", (L.lab(this), offset, whence))
            if whence == 0:
                target = offset
            elif whence == 1:
                target = self.pos + offset
            elif whence == 2:
                target = len(self.stream) + offset
            else:
                target = -1
            answer = -1
            if 0 <= target <= len(self.stream):
                self.pos = target
                answer = target
            e.set_r(3, u32(L.ret("CFile.Seek", answer)))

        def file_size(e):
            rec("CFile.GetSize", (L.lab(e.r(3)),))
            e.set_r(3, u32(L.ret("CFile.GetSize", len(self.stream))))

        def wad_open(e):
            result, this, name, storage = e.r(3), e.r(4), e.r(5), e.r(6)
            rec("CWadFile.Open", (L.lab(this),), (e.cstr(name)[:159] if name else "(null)", e.cstr(storage)[:159] if storage else "(null)"))
            answer = s32(L.ret("CWadFile.Open", 0))
            if answer >= 0:
                self.pos = 0
            w32(e, result, answer)

        def buffered_open(e):
            result, this, other, size = e.r(3), e.r(4), e.r(5), e.r(6)
            rec("Buffered.Open", (L.lab(this), L.lab(other), size))
            w32(e, result, L.ret("Buffered.Open", 0))

        def mem_alloc(e):
            size = e.r(3)
            if L.ret("LbMem_Alloc", 1) == 0:
                rec("LbMem_Alloc", (size, 0))
                e.set_r(3, 0)
                return
            block = e.alloc(size + MARGIN + 1, b"\xa5" * (size + MARGIN + 1))
            label = L.newlab(block)
            self.heap[label] = (block, size)
            self.registry.append((block, size + MARGIN, label))
            rec("LbMem_Alloc", (size, label))
            e.set_r(3, block)

        def mem_free(e):
            block = e.r(3)
            rec("LbMem_Free", (L.lab(block),))
            if L.lab(block) >= FIRST_HEAP_LABEL and L.lab(block) in self.heap:
                self.heap.pop(L.lab(block), None)       # (the registry keeps the block: a pointer left behind still describes)
                L.forget(block)

        def surface_area(e):
            rec("TbSurface.GetRenderArea", (L.lab(e.r(3)),))
            e.set_r(3, self.areas[2])

        def rect_intersection(e):
            a, b = e.r(3), e.r(4)
            ra = list(struct.unpack(">4i", e.read(a, 16)))
            rb = list(struct.unpack(">4i", e.read(b, 16)))
            rec("LbRect_Intersection", ra + rb)
            ra[0], ra[1] = max(ra[0], rb[0]), max(ra[1], rb[1])
            ra[2], ra[3] = min(ra[2], rb[2]), min(ra[3], rb[3])
            e.write(a, struct.pack(">4i", *ra))
            e.set_r(3, L.ret("LbRect_Intersection", 1 if ra[0] < ra[2] and ra[1] < ra[3] else 0) & 0xFF)

        def rect_empty(e):
            a = e.r(3)
            ra = list(struct.unpack(">4i", e.read(a, 16)))
            rec("LbRect_Empty", ra)
            ra[2], ra[3] = ra[0], ra[1]
            e.write(a, struct.pack(">4i", *ra))

        def operator_delete(e):
            block = e.r(3)
            if block:
                self.deletes += 1
            if L.lab(block) > 0:
                rec("delete", (L.lab(block),))
                L.forget(block)

        S.imp("__dl__FPv", operator_delete)

        def conv_ctor(e):
            rec("CConverter.ctor", (L.newlab(e.r(3)),))

        def conv_init(e):
            rec("CConverter.Init", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5)), L.lab(e.r(6)), e.r(7)))

        def conv_buf(e):
            this, source, dest, count = e.r(3), e.r(4), e.r(5), s32(e.r(6))
            rec("CConverter.ConvBuf", (L.lab(this), L.lab(source), L.lab(dest), count))
            if count > 0:
                e.write(dest, bytes(b ^ 0x5A for b in e.read(source, count)))

        def conv_release(e):
            rec("CConverter.Release", (L.lab(e.r(3)),))

        for address, fn in ((FILE_CTOR, file_ctor), (FILE_DTOR, file_dtor), (FILE_READ, file_read), (FILE_SEEK, file_seek),
                            (FILE_SIZE, file_size), (WAD_OPEN, wad_open), (BUFFERED_OPEN, buffered_open), (MEM_ALLOC, mem_alloc),
                            (MEM_FREE, mem_free), (SURFACE_AREA, surface_area), (RECT_INTERSECTION, rect_intersection),
                            (RECT_EMPTY, rect_empty), (CONV_CTOR, conv_ctor), (CONV_INIT, conv_init), (CONV_BUF, conv_buf),
                            (CONV_RELEASE, conv_release)):
            S.internal(address, fn)
        return S

    # ---- calls
    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MASK for a in args])[0]

    def call_result(self, address, *args):
        """A call that returns its TbResult through a hidden first argument."""
        result = self.e.alloc(4, b"\xa5" * 4)
        T.call_ppc(self.e, address, [result] + [a & MASK for a in args])
        return s32(self.e.u32(result))

    def op(self, code, a):
        e = self.e
        a = list(a) + [0] * (12 - len(a))
        area = self.area
        if code == 1:
            self.call(RA_DEFAULT, area(a[0]))
        elif code == 2:
            self.call(RA_SURFACE, area(a[0]), self.surface)
        elif code == 3:
            size = e.alloc(8, struct.pack(">ii", s32(a[1]), s32(a[2])))
            fmt = e.alloc(24, struct.pack(">6I", *[u32(v) for v in a[3:9]]))
            data = self.block(a[9]) + a[10] if a[9] else 0
            self.call(RA_FULL, area(a[0]), size, fmt, data, a[11])
        elif code == 4:
            return self.call_result(RA_ALLOC, area(a[0]))
        elif code == 5:
            self.call(RA_FREE, area(a[0]))
        elif code == 6:
            return u32(self.call(RA_MEMSIZE, area(a[0])))
        elif code == 7:
            point = e.alloc(8, struct.pack(">ii", s32(a[2]), s32(a[3])))
            size = e.alloc(8, struct.pack(">ii", s32(a[4]), s32(a[5])))
            return self.call_result(RA_CLIENT_POINT, area(a[0]), area(a[1]), point, size)
        elif code == 8:
            rect = e.alloc(16, struct.pack(">4i", *[s32(v) for v in a[2:6]]))
            return self.call_result(RA_CLIENT_RECT, area(a[0]), area(a[1]), rect)
        elif code == 10:
            self.call(LD_CTOR, self.loader)
        elif code == 11:
            self.call(LD_DTOR, self.loader, a[0])
        elif code == 12:
            self.call(TGA_CTOR, self.loader)
        elif code == 13:
            self.call(TGA_DTOR, self.loader, a[0])
        elif code == 14:
            second = self.text(a[1]) if a[1] >= 0 else 0
            return self.call_result(LD_HEADER_PATH, self.loader, self.text(a[0]), second, area(a[2]) if a[2] >= 0 else 0)
        elif code == 15:
            return self.call_result(LD_HEADER_FILE, self.loader, self.file0, area(a[0]) if a[0] >= 0 else 0)
        elif code == 16:
            second = self.text(a[2]) if a[2] >= 0 else 0
            return self.call_result(LD_LOAD_PATH, self.loader, area(a[0]), self.text(a[1]), second, area(a[3]) if a[3] >= 0 else 0)
        elif code == 17:
            return self.call_result(LD_LOAD_FILE, self.loader, area(a[0]), self.file0, area(a[1]) if a[1] >= 0 else 0)
        elif code == 18:
            return self.call(LD_VALID, self.loader, self.file0) & 0xFF
        elif code == 19:
            return self.call_result(TGA_HEADER, self.loader, self.file0, area(a[0]))
        elif code == 20:
            return self.call_result(TGA_DATA, self.loader, area(a[0]), self.file0, area(a[1]))
        elif code == 21:
            return self.call(TGA_VALID, self.loader, self.file0) & 0xFF
        elif code == 22:
            address = e.alloc(16, b"\xa5" * 16)
            self.L.rec("CFile.ctor", (self.L.newlab(address),))
            self.call(BUFFERED_DTOR, address, a[0])
        elif code == 29:
            w32(e, self.gll + 0x1C, GLL_VTABLE)
        elif code == 30:
            return self.call_result(GLL_HEADER, self.gll, self.file0, self.loader)
        elif code == 31:
            return self.call_result(GLL_DATA, self.gll, area(a[0]), self.file0, self.loader)
        elif code == 32:
            return self.call_result(GLL_FIRST, self.gll, self.file0)
        elif code == 33:
            return self.call_result(GLL_NEXT, self.gll, self.file0)
        elif code == 34:
            return self.call_result(GLL_REVERSE, self.gll, self.block(a[0]) + a[1])
        elif code == 35:
            return self.call_result(GLL_RLE, self.gll, self.file0)
        elif code == 36:
            return u32(self.call(GLL_LENGTH, self.gll))
        elif code == 37:
            return self.call_result(DO_READ, self.gll, area(a[0]), self.file0, self.loader)
        elif code == 38:
            self.call(WRITE_LINE, self.gll)
        elif code == 39:
            self.call(PALETTE, self.block(a[0]) + a[1], self.block(a[2]) + a[3], a[4])
        elif code == 40:
            base = area(a[0])
            if a[1] <= 3:
                w32(e, base + 4 * a[1], a[2])
            elif a[1] == 4:
                w32(e, base + 0x10, a[2])
            elif a[1] == 5:
                w32(e, base + 0x14, a[2])
            elif a[1] == 6:
                w32(e, base + 0x18, self.block(a[2]) + a[3] if a[2] in (1, 2) else (0 if a[2] == 0 else 0x12340000 + a[3]))
            elif a[1] == 7:
                w32(e, base + 0x1C, a[2])
            elif a[1] == 8:
                w32(e, base + 0x20, a[2])
            else:
                w32(e, base + 0x24, a[2])
        elif code == 41:
            g, field = self.gll, a[0]
            pointer = lambda: (self.block(a[1]) + a[2]) if a[1] in (1, 2) else 0   # noqa: E731
            if field == 0:
                w32(e, g, pointer())
            elif field == 1:
                w32(e, g + 4, a[1])
            elif field == 2:
                w32(e, g + 8, a[1])
            elif field == 3:
                w32(e, g + 0xC, a[1])
            elif field == 4:
                w32(e, g + 0x10, a[1])
            elif field == 5:
                w32(e, g + 0x14, pointer())
            elif field == 6:
                w32(e, g + 0x18, a[1])
            elif field == 7:
                w32(e, g + 0x20, a[1])
            elif field == 8:
                w32(e, g + 0x24, a[1])
            elif field == 9:
                w32(e, g + 0x28, a[1])
            elif field == 10:
                w32(e, g + 0x2C, a[1])
            else:
                header = bytearray(swap_header(e.read(g + 0x30, 18)))
                header[field - 11] = a[1] & 0xFF
                e.write(g + 0x30, swap_header(bytes(header)))
        elif code == 42:
            e.write(self.loader + 0x2C + a[0], bytes([a[1] & 0xFF]))
        elif code == 43:
            w32(e, self.loader + 0x42C, a[0])
        elif code == 44:
            self.pos = a[0]
        else:
            raise ValueError(f"bad operation {code}")
        return 0

    # ---- the state, in the order tpw_cc_dump_world writes it
    def area_words(self, base):
        e = self.e
        words = self.words(base, 6) + [u32(e.u32(base + 0x18))]
        masks, depth, palettised = words[0:4], words[4], words[5]
        return masks + [depth, palettised] + self.describe(words[6]) + self.words(base + 0x1C, 3)

    def dump(self):
        e = self.e
        out = []
        for index in range(4):
            out += self.area_words(self.area(index))
        out.append(e.u32(self.loader + 0x42C))
        out += list(e.read(self.loader + 0x2C, 0x400))
        header = bytes(e.read(self.loader + 0x430, 18))
        if self.hdr_state == "swapped":
            header = swap_header(header)
        out += list(header)
        g = self.gll
        out += self.describe(e.u32(g)) + [e.u32(g + 4), e.u32(g + 8), e.u32(g + 0xC), e.u32(g + 0x10)]
        out += self.describe(e.u32(g + 0x14)) + [e.u32(g + 0x18), e.u32(g + 0x20), e.u32(g + 0x24), e.u32(g + 0x28), e.u32(g + 0x2C)]
        out += list(swap_header(e.read(g + 0x30, 18)))
        out += [fnv(e.read(block, EXTENT)) for block in self.blocks]
        out.append(fnv(e.read(self.palette, 0x400)))
        live = sorted((label, address, size) for label, (address, size) in self.heap.items())
        out.append(len(live))
        for label, address, size in live:
            out += [label, size, fnv(e.read(address, size + MARGIN))]
        out.append(self.pos)
        out.append(self.deletes)
        return out


def ppc_bf(e, case):
    """Runs the case's operations on the PowerPC side: [(answer, state after)], the log."""
    world = Bf(e, case)
    stubs = world.stubs()
    saved = stubs.activate(e)
    try:
        steps = []
        case["mixed"] = []
        for index, op in enumerate(case["ops"]):
            answer = world.op(op[0], op[1:])
            steps.append((answer, world.dump()))
            if world.hdr_state == "mixed":
                # The loader's header buffer holds a short read over the swapped bytes of an earlier one: it is not compared.
                case["mixed"].append(index)
                steps[-1][1][HEADER_AT:HEADER_AT + 18] = [0] * 18
        return dict(steps=steps, log=world.L.log)
    finally:
        CL.Stubs.deactivate(e, saved)


# =========================================================================== the loader world, host side


def open_host(lib):
    if getattr(lib, "_cc_open", False):
        return
    lib.tpw_cc_begin.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_cc_labels.argtypes = []
    lib.tpw_cc_set_text.argtypes = [C.c_int32, C.c_char_p]
    lib.tpw_cc_set_found_path.argtypes = [C.c_char_p]
    lib.tpw_cc_set_show_count.argtypes = [C.c_int32]
    lib.tpw_cc_fill_block.argtypes = [C.c_int32, C.c_char_p, C.c_int32]
    lib.tpw_cc_op.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_cc_op.restype = C.c_int64
    lib.tpw_cc_dump_world.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cc_dump_world.restype = C.c_int32
    lib.tpw_cc_dump_cursors.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cc_dump_cursors.restype = C.c_int32
    lib.tpw_cc_heap.argtypes = [C.POINTER(C.c_int64)]
    lib._cc_open = True


def host_op(lib, code, args):
    values = list(args) + [0] * (12 - len(args))
    array = (C.c_int64 * 12)(*values)
    return lib.tpw_cc_op(code, array)


def host_dump(lib):
    out = (C.c_int64 * 4096)()
    n = lib.tpw_cc_dump_world(out)
    return list(out[:n])


def host_bf(lib, case):
    open_host(lib)
    stream = bytes(case.get("stream", b""))
    lib.tpw_cc_begin(stream, len(stream))
    fill = bytes(case.get("fill", b""))[:BLOCK]
    if fill:
        lib.tpw_cc_fill_block(1, fill, len(fill))
    for index, text in enumerate(case.get("texts", [])):
        lib.tpw_cc_set_text(index, text.encode("latin-1"))
    CL.host_begin(lib, {}, case.get("queues"))
    lib.tpw_cc_labels()
    steps = []
    for index, op in enumerate(case["ops"]):
        answer = host_op(lib, op[0], op[1:])
        steps.append((answer, host_dump(lib)))
        if index in case.get("mixed", ()):
            steps[-1][1][HEADER_AT:HEADER_AT + 18] = [0] * 18
    return dict(steps=steps, log=CL.host_end(lib))


# =========================================================================== the cases of the loader world

TEXTS = ["data:UI:cursors:Test.tga", "storage.wad", "", "a"]
FOOTER = b"\0" * 8 + b"TRUEVISION-XFILE.\0"


def case_of(ops, stream=b"", texts=None, queues=None, fill=b""):
    return dict(ops=ops, stream=bytes(stream), texts=list(TEXTS if texts is None else texts), queues=queues or {}, fill=bytes(fill))


def rbytes(r, count):
    return bytes(r.randrange(256) for _ in range(count))


def tga(width, height, depth=24, kind=2, descriptor=0x20, ident=b"", map_type=None, map_start=0, map_length=0, map_bits=24, map_data=None,
        body=None, footer=False, r=None):
    """A TGA file: the header (little endian), the identification, the colour map and the pixels. The pixels are random
    bytes of the loader's size (the library reads depth / 8 bytes a pixel) unless `body` is given."""
    r = r or random.Random(width * 131 + height * 7 + depth)
    if map_type is None:
        map_type = 1 if kind in (1, 9) else 0
    if map_data is None:
        map_data = rbytes(r, map_length * 3)
    if body is None:
        body = rbytes(r, width * height * (depth >> 3))
    header = struct.pack("<BBBHHBHHHHBB", len(ident), map_type, kind, map_start, map_length, map_bits, 0, 0, width, height, depth, descriptor)
    return header + ident + map_data + body + (FOOTER if footer else b"")


def rle_body(width, height, bpp, r, overrun=False, cut=False):
    """Run-length encoded rows: packets of random runs and random raw pixels. `overrun` ends the last row with a packet that
    reaches past its end; `cut` leaves the last packet incomplete."""
    out = bytearray()
    for row in range(height):
        pixels = [rbytes(r, bpp) for _ in range(width)]
        at = 0
        while at < width:
            left = width - at
            count = r.randint(1, min(128, left))
            if overrun and row == height - 1 and count == left and left < 120:
                count = left + r.randint(1, 6)
            if r.random() < 0.5:
                out.append(0x80 | (count - 1))
                out += pixels[min(at, width - 1)]
            else:
                out.append(count - 1)
                for k in range(count):
                    out += pixels[min(at + k, width - 1)]
            at += count
    if cut and out:
        out = out[:-r.randint(1, min(3, len(out)))]
    return bytes(out)


def real_cursors():
    """The game's cursor files, read when the test runs (never copied): [(name, bytes)]."""
    if not CURSOR_DIR.is_dir():
        return []
    return [(p.name, p.read_bytes()) for p in sorted(CURSOR_DIR.glob("*.tga"))]


AREA_FIELDS = ["m0", "m1", "m2", "m3", "depth", "pal", "data", "width", "height", "stride"]


def pokes_area(index, **fields):
    """Operations that set the named fields of an area; data is (block, offset): block 0 NULL, 1 and 2 the world's pixel
    blocks, 3 a pointer that is not in any block."""
    ops = []
    for key, value in fields.items():
        field = AREA_FIELDS.index(key)
        ops.append((40, index, field, value[0], value[1]) if key == "data" else (40, index, field, value))
    return ops


GLL_FIELDS = ["line", "size", "destbytes", "srcbytes", "bottomup", "dest", "step", "rle", "rtl", "left", "linebytes"]


def pokes_gll(**fields):
    ops = []
    for key, value in fields.items():
        field = GLL_FIELDS.index(key)
        ops.append((41, field, value[0], value[1]) if key in ("line", "dest") else (41, field, value))
    return ops


def header_bytes(width, height, depth, kind=2, descriptor=0x20, ident=0, map_type=0, map_start=0, map_length=0, map_bits=24):
    return struct.pack("<BBBHHBHHHHBB", ident, map_type, kind, map_start, map_length, map_bits, 0, 0, width, height, depth, descriptor)


def pokes_header(header):
    """The 18 header bytes of the line reader, one operation each."""
    return [(41, 11 + i, b) for i, b in enumerate(header)]


# ---- TbRenderArea

def ra_pokes_random(r, index, bounded=False):
    if bounded:
        return pokes_area(index, m0=r.getrandbits(32), depth=r.choice([8, 16, 24, 32, 15]), data=(r.choice([1, 2, 3]), r.randrange(0, 300)),
                          width=r.randrange(0, 40), height=r.randrange(0, 40), stride=r.randrange(0, 160))
    return pokes_area(index, m0=r.getrandbits(32), m1=r.getrandbits(32), m2=r.getrandbits(32), m3=r.getrandbits(32),
                      depth=r.choice([0, 1, 7, 8, 15, 16, 24, 32, 33, 64, r.getrandbits(32)]), pal=r.choice([0, 1, r.getrandbits(32)]),
                      data=(r.choice([0, 0, 1, 2, 3]), r.randrange(0, 200)), width=r.choice([0, 1, 3, 17, 40, r.getrandbits(32)]),
                      height=r.choice([0, 1, 5, 33, r.getrandbits(32)]), stride=r.choice([0, 3, 40, 100, r.getrandbits(32)]))


def ra_constructors(r):
    ops = []
    for index in (0, 1, 2, 3):
        ops += ra_pokes_random(r, index)
    for _ in range(r.randint(2, 6)):
        a = r.randrange(4)
        kind = r.randrange(3)
        if kind == 0:
            ops += [(1, a)]
        elif kind == 1:
            ops += [(2, a)]
        else:
            ops += [(3, a, r.choice([0, 1, 13, 64, -5, r.getrandbits(32)]), r.choice([0, 1, 7, 64, -5, r.getrandbits(32)]),
                     r.getrandbits(32), r.getrandbits(32), r.getrandbits(32), r.getrandbits(32), r.choice([0, 8, 15, 16, 24, 32, 100, -3]),
                     r.choice([0, 1, 5]), r.choice([0, 0, 1, 2]), r.randrange(0, 100), r.choice([0, 0, 1, 9, 100, MASK - 3]))]
    return case_of(ops)


def ra_memory(r):
    """Allocations of small pictures (some refused by the heap), sizes, and freeing."""
    ops = []
    queue = [r.choice([1, 1, 1, 0]) for _ in range(5)]
    for _ in range(r.randint(1, 4)):
        a = r.randrange(4)
        ops += pokes_area(a, data=(r.choice([0, 0, 0, 1, 3]), r.randrange(0, 50)), depth=r.choice([8, 16, 24, 32, 1, 7, 15, 0]),
                          width=r.choice([0, 1, 7, 20]), height=r.choice([0, 1, 5, 20]), stride=r.choice([0, 9, 100]))
        ops += r.choice([[(4, a), (6, a)], [(4, a), (4, a), (5, a), (5, a)], [(6, a), (5, a), (4, a)], [(4, a), (5, a), (4, a), (6, a)]])
    return case_of(ops, queues={"LbMem_Alloc": queue})


def ra_client(r):
    ops = pokes_area(0, m0=r.getrandbits(32), depth=r.choice([8, 16, 24, 32, 15, 1]), data=(r.choice([1, 2, 3, 0]), r.randrange(0, 300)),
                     width=r.randrange(0, 40), height=r.randrange(0, 40), stride=r.randrange(0, 160))
    ops += ra_pokes_random(r, 1, bounded=True)
    for _ in range(r.randint(1, 4)):
        a, b = r.choice([(0, 1), (0, 3), (1, 0), (3, 0)])
        if r.random() < 0.5:
            ops.append((7, a, b, r.randint(-20, 50), r.randint(-20, 50), r.randint(-5, 60), r.randint(-5, 60)))
        else:
            ops.append((8, a, b, r.randint(-20, 50), r.randint(-20, 50), r.randint(-20, 70), r.randint(-20, 70)))
    queues = {"LbRect_Intersection": [r.choice([0, 1]) for _ in range(r.randint(1, 3))]} if r.random() < 0.3 else {}
    return case_of(ops, queues=queues)


def ra_edges():
    base = pokes_area(0, depth=24, width=10, height=8, stride=30, data=(1, 16))
    yield case_of([(1, 0), (1, 1), (1, 3), (2, 0)] + base)
    yield case_of(pokes_area(2, m0=0xFF0000, m1=0xFF00, m2=0xFF, depth=24, pal=0, data=(2, 5), width=7, height=9, stride=21) +
                  [(2, 0), (3, 1, 5, 6, 1, 2, 3, 4, 16, 0, 1, 4, 0), (3, 1, 5, 6, 1, 2, 3, 4, 16, 0, 0, 0, 77), (6, 0), (6, 1)])
    for width, height, depth in [(4, 4, 8), (3, 5, 24), (0, 5, 8), (5, 0, 8), (0x10000, 0x10000, 32), (7, 7, 1), (7, 7, 0)]:
        for refuse in (0, 1):
            yield case_of(pokes_area(0, data=(0, 0), width=width, height=height, depth=depth) + [(4, 0), (4, 0), (5, 0), (5, 0), (6, 0)],
                          queues={"LbMem_Alloc": [1 - refuse]})
    # the clipping function says there is something left though the rectangle has no height or no width
    yield case_of(base + [(8, 0, 1, 2, 3, 6, 3)], queues={"LbRect_Intersection": [1]})
    yield case_of(base + [(8, 0, 1, 4, 3, 4, 7)], queues={"LbRect_Intersection": [1]})
    rects = [(0, 0, 10, 8), (2, 3, 6, 5), (-4, -4, 4, 4), (8, 6, 20, 20), (5, 5, 5, 5), (20, 20, 30, 30), (3, 3, 2, 2), (0, 0, 0, 0), (-9, -9, 100, 100)]
    for rect in rects:
        yield case_of(base + [(8, 0, 1) + rect, (7, 0, 1, rect[0], rect[1], rect[2], rect[3])])
        yield case_of(base + pokes_area(0, data=(0, 0)) + [(8, 0, 1) + rect])


def ra_any(r):
    return r.choice([ra_constructors, ra_memory, ra_client])(r)


# ---- the loader through its public face

def tga_file(r, kind=None, depth=None):
    """One file of a random kind: image types 0 to 3 and 9 to 11, depths 8 to 32, either row order, a colour map when the
    type has one (and sometimes when it has not), identification, a footer."""
    depth = depth or r.choice([8, 15, 16, 24, 32])
    kind = r.choice([0, 1, 2, 3, 9, 10, 11]) if kind is None else kind
    width, height = r.randint(1, 12), r.randint(1, 8)
    bpp = depth >> 3
    map_length = r.choice([1, 3, 16, 200, 256]) if kind in (1, 9) else r.choice([0, 0, 0, 2, 5])
    body = rle_body(width, height, bpp, r, overrun=r.random() < 0.15, cut=r.random() < 0.1) if kind >= 9 else None
    return tga(width, height, depth, kind, r.choice([0x20, 0x00, 0x30, 0x10, 0x28, 0x08]), ident=rbytes(r, r.choice([0, 0, 3, 9])),
               map_type=1 if (map_length and kind not in (1, 9) and r.random() < 0.5) else None, map_length=map_length, body=body,
               footer=r.random() < 0.3, r=r)


def pipeline_ops(r, variant):
    """The calls a cursor makes (the loader's constructor, ReadHeader into the area, AllocMemory, Load, the destructor) in the
    ways the game can make them: by path or by file, with the second name or without, with the loader's area copied or not."""
    second = r.choice([-1, 1, 2, 3])
    other = r.choice([-1, 1])
    ops = pipeline_ops_of(r, variant, second, other)
    if r.random() < 0.25:
        ops = [(1, 0), (10,), (11, r.choice([0, 1]))] + ops[1:]
    ops[-1] = (13, r.choice([0, 0, 1]))
    return ops


def pipeline_ops_of(r, variant, second, other):
    if variant == 0:
        return [(1, 0), (12,), (14, 0, second, 0), (4, 0), (16, 0, 0, second, other), (13,)]
    if variant == 1:
        return [(1, 0), (12,), (15, 0), (4, 0), (17, 0, other), (13,)]
    if variant == 2:
        return [(1, 0), (12,), (18,), (14, 0, second, 0), (4, 0), (16, 0, 0, second, other), (18,), (13,)]
    if variant == 3:
        return [(1, 0), (12,), (15, -1), (4, 3), (17, 3, -1), (13,)]
    # the area is changed between the header and the pixels: smaller, larger, another format or depth
    changes = []
    for _ in range(r.randint(1, 3)):
        kind = r.randrange(5)
        if kind == 0:
            changes += pokes_area(0, width=r.randint(0, 14))
        elif kind == 1:
            changes += pokes_area(0, height=r.randint(0, 10))
        elif kind == 2:
            changes += pokes_area(0, depth=r.choice([8, 16, 24, 32]), m0=r.choice([0, 0xFF0000, 0x7C00]), pal=r.choice([0, 1]))
        elif kind == 3:
            changes += pokes_area(0, m1=r.choice([0, 0xFF00, 0x3E0]))
        else:
            changes += pokes_area(0, pal=r.choice([0, 1, 7]))
    return [(1, 0), (12,), (14, 0, second, 0)] + changes + [(4, 0), (16, 0, 0, second, other), (13,)]


def failure_queues(r, stream=b""):
    """Return values for the fakes that make the library's calls fail now and then. The heap never refuses for a colour-mapped file:
    the original reads and converts the map through a buffer it does not look at, and crashes on a NULL one."""
    queues = {}
    for key, options in (("CWadFile.Open", [0, 0, 0, 0, -1]), ("Buffered.Open", [0, 0, 0, -1]), ("LbMem_Alloc", [1, 1, 1, 1, 0])):
        if r.random() < 0.15:
            queues[key] = [r.choice(options) for _ in range(r.randint(1, 3))]
    if "LbMem_Alloc" in queues and len(stream) > 2 and stream[2] in (1, 9) and stream[1] == 1:
        del queues["LbMem_Alloc"]
    for key in ("CFile.Seek", "CFile.GetSize", "CFile.Read"):
        if r.random() < 0.1:
            queues[key] = [r.choice([-1, 0, 1, 18, 0x12, 5]) for _ in range(r.randint(1, 4))]
    return queues


def pipeline_case(r, stream=None, variant=None, texts=None, damage=True):
    stream = tga_file(r) if stream is None else stream
    if damage and r.random() < 0.25:
        stream = stream[:r.randrange(0, len(stream) + 1)]
    variant = r.randrange(5) if variant is None else variant
    queues = failure_queues(r, stream) if damage and r.random() < 0.5 else {}
    return case_of(pipeline_ops(r, variant), stream, texts, queues)


def pipeline_edges():
    r = random.Random(11)
    # every depth and image type, both row orders
    for depth in (8, 15, 16, 24, 32):
        for kind in (0, 1, 2, 3, 9, 10, 11):
            for descriptor in (0x20, 0x00, 0x10, 0x30):
                width, height = r.randint(1, 7), r.randint(1, 5)
                map_length = 5 if kind in (1, 9) else 0
                body = rle_body(width, height, depth >> 3, r) if kind >= 9 else None
                stream = tga(width, height, depth, kind, descriptor, map_length=map_length, body=body, r=r)
                yield case_of(pipeline_ops(r, 0), stream)
    # the files of the game
    for name, data in real_cursors():
        yield case_of(pipeline_ops(r, r.randrange(4)), data)
    # short files: a cut at every length of a small file
    small = tga(3, 2, 24, 2, 0x20, r=r)
    for length in range(0, len(small) + 1, 3):
        yield case_of(pipeline_ops(r, 0), small[:length])
        yield case_of(pipeline_ops(r, 1), small[:length])
    # bad headers
    good = tga(4, 3, 24, 2, 0x20, r=r)
    for index, value in [(1, 2), (2, 4), (2, 8), (2, 12), (2, 255), (1, 0), (16, 7), (16, 33), (16, 0), (16, 12), (7, 16), (7, 32), (12, 0), (14, 0)]:
        data = bytearray(good)
        data[index] = value
        yield case_of(pipeline_ops(r, 0), bytes(data))
    mapped = tga(4, 3, 8, 1, 0x20, map_length=16, r=r)
    for index, value in [(1, 0), (5, 0x01), (6, 0x01), (7, 16), (7, 32), (2, 2), (2, 9), (5, 0)]:
        data = bytearray(mapped)
        data[index] = value
        yield case_of(pipeline_ops(r, 0), bytes(data))
    # a colour map bigger than 256, of 256, with a start, in a true colour file
    for length in (255, 256, 257, 300):
        yield case_of(pipeline_ops(r, 0), tga(2, 2, 8, 1, 0x20, map_length=length, map_start=3, r=r))
    for length in (1, 7, 200):
        yield case_of(pipeline_ops(r, 0), tga(2, 2, 24, 2, 0x20, map_type=1, map_length=length, r=r))
        yield case_of(pipeline_ops(r, 0), tga(2, 2, 24, 2, 0x20, map_type=0, map_length=length, r=r))
    # footer and no footer, the sizes that end the check early
    for stream in (b"", b"\0" * 17, b"\0" * 18, tga(1, 1, 8, 0, 0x20, footer=True, r=r), tga(1, 1, 24, 7, 0x20, footer=True, r=r),
                   tga(1, 1, 24, 7, 0x20, r=r), tga(1, 1, 24, 2, 0x20, map_type=2, r=r), tga(1, 1, 24, 2, 0x20, map_type=1, r=r),
                   tga(1, 1, 5, 2, 0x20, r=r), tga(1, 1, 15, 2, 0x20, r=r)):
        yield case_of(pipeline_ops(r, 2), stream)
    # ValidImage and IsValid: every image type, map type and depth in the header; the second seek failing
    for kind in (0, 1, 2, 3, 4, 8, 9, 10, 11, 12):
        for map_type in (0, 1):
            for depth in (8, 15, 16, 24, 32, 7):
                stream = tga(2, 2, depth, kind, 0x20, map_type=map_type, map_length=3 if map_type else 0, r=r)
                yield case_of([(12,), (21,)], stream)
    stream = tga(2, 2, 24, 2, 0x20, r=r)
    yield case_of([(12,), (21,), (18,)], stream, queues={"CFile.Seek": [len(stream) - 18, -1]})
    yield case_of([(12,), (21,), (18,)], stream, queues={"CFile.Seek": [len(stream) - 18, 0, 0, 5]})
    yield case_of([(12,), (21,)], stream, queues={"CFile.Seek": [3]})
    yield case_of([(12,), (21,)], stream, queues={"CFile.Read": [18, 5]})
    yield case_of([(12,), (21,)], stream, queues={"CFile.GetSize": [17]})
    yield case_of([(12,), (21,)], stream, queues={"CFile.GetSize": [40, 41]})
    yield case_of([(12,), (21,)], stream, queues={"CFile.GetSize": [-5]})
    yield case_of([(22, 0), (22, 1), (22, 0)])
    # the failures of each call
    stream = tga(3, 3, 24, 2, 0x20, r=r)
    for key, values in (("CWadFile.Open", [-1]), ("Buffered.Open", [-1]), ("CFile.Seek", [-1]), ("CFile.Seek", [0, -1]), ("CFile.Seek", [0, 0, 1]),
                        ("LbMem_Alloc", [0]), ("LbMem_Alloc", [1, 0]), ("CFile.Read", [3]), ("CFile.Read", [0x12, 0]), ("CFile.Read", [0x12, 1, 2])):
        for variant in (0, 1, 2):
            yield case_of(pipeline_ops(r, variant), stream, queues={key: values})
    mapped = tga(3, 3, 8, 9, 0x20, map_length=4, body=rle_body(3, 3, 1, r), r=r)
    for key, values in (("LbMem_Alloc", [1, 0, 1, 1]), ("LbMem_Alloc", [1, 1, 1, 0]), ("CFile.Read", [0x12, 12]), ("CFile.Read", [0x12, 12, 3, 3])):
        yield case_of(pipeline_ops(r, 0), mapped, queues={key: values})
    # areas changed between the header and the pixels, over a file with both row orders
    for descriptor in (0x20, 0x00):
        stream = tga(5, 4, 24, 2, descriptor, r=r)
        for changes in ([("width", 3)], [("width", 9)], [("height", 2)], [("height", 7)], [("depth", 32), ("m0", 0xFF0000)], [("pal", 1)], [("m1", 7)],
                        [("height", 0)], [("width", 0)], [("depth", 16), ("height", 2)]):
            ops = [(1, 0), (12,), (14, 0, -1, 0)]
            for key, value in changes:
                ops += pokes_area(0, **{key: value})
            ops += [(4, 0), (16, 0, 0, -1, 1), (13,)]
            yield case_of(ops, stream)


def pipeline_random(r):
    kind = r.random()
    if kind < 0.1 and real_cursors():
        name, data = r.choice(real_cursors())
        return pipeline_case(r, data, texts=[f"data:UI:cursors:{name}", "wad", "", "a"])
    return pipeline_case(r)


# ---- the header reader and the validity test on random headers

def random_header_case(r):
    fields = dict(ident=r.choice([0, 0, 0, 1, 7]), map_type=r.choice([0, 0, 1, 1, 2, 255]), kind=r.choice([0, 1, 2, 3, 4, 8, 9, 10, 11, 12, 255, 2, 2]),
                  map_start=r.choice([0, 3, 255, 65535]), map_length=r.choice([0, 1, 16, 255, 256, 257, 65535]),
                  map_bits=r.choice([24, 24, 24, 16, 32, 8, 0]), width=r.choice([0, 1, 2, 17, 65535]), height=r.choice([0, 1, 2, 17, 65535]),
                  depth=r.choice([7, 8, 15, 16, 24, 32, 33, 0, 255, 24, 8]), descriptor=r.getrandbits(8))
    header = header_bytes(fields["width"], fields["height"], fields["depth"], fields["kind"], fields["descriptor"], fields["ident"],
                          fields["map_type"], fields["map_start"], fields["map_length"], fields["map_bits"])
    body = rbytes(r, r.choice([0, 20, 900, 1700]))
    stream = header + body
    if r.random() < 0.1:
        stream = stream[:r.randrange(0, len(stream))]
    if r.random() < 0.2:
        stream += FOOTER
    ops = r.choice([[(12,), (19, 0), (21,), (18,)], [(12,), (21,), (19, 0), (44, 0), (19, 0)], [(12,), (19, 0), (44, 0), (19, 0), (19, 0)],
                    [(12,), (18,), (21,), (21,)], [(12,), (29,), (30,), (44, 0), (30,), (19, 0)]])
    return case_of(ops, stream, queues=failure_queues(r, stream) if r.random() < 0.3 else {})


# ---- the line reader's members

def line_case(r):
    """ReadFirstLine, ReadNextLine, ReverseScanLine, TGALoadRLE, GetSourceLineLength and WriteLine on pokes of the reader's state."""
    width = r.choice([0, 1, 2, 3, 7, 8, 16, 40])
    height = r.choice([0, 1, 2, 5])
    depth = r.choice([8, 15, 16, 24, 32, 0, 5])   # (more than four bytes a pixel would write over the original's packet byte)
    rle = r.choice([0, 0, 1, 1, 2, -1])
    rtl = r.choice([0, 1])
    bpp = depth >> 3
    line_bytes = width * bpp if r.random() < 0.85 else r.randrange(0, 80)
    ops = [(29,)] + pokes_header(header_bytes(width, height, depth, 2, 0x20)) + pokes_gll(line=(r.choice([1, 2]), r.randrange(0, 200)),
                                                                                         rle=rle, rtl=rtl, left=r.choice([0, 1, 2, height]),
                                                                                         linebytes=line_bytes, bottomup=r.randrange(2))
    steps = []
    for _ in range(r.randint(1, 5)):
        steps.append(r.choice([(32,), (33,), (33,), (34, 1, r.randrange(0, 100)), (35,), (36,), (36,)]))
    # a stream the reads can use: raw pixels or run-length packets
    if rle == 1 or r.random() < 0.3:
        stream = rle_body(max(width, 1), 4, max(bpp, 1), r, overrun=r.random() < 0.2, cut=r.random() < 0.15)
    else:
        stream = rbytes(r, r.choice([0, line_bytes, line_bytes * 2, 400]))
    if (35,) in steps or rle == 1:
        stream = stream
    # a write: the destination, the line and the count
    if r.random() < 0.3:
        ops += pokes_gll(dest=(2, r.randrange(0, 300)), destbytes=r.randrange(0, 64), step=r.choice([0, 7, -9, 40]), line=(1, r.randrange(0, 100)))
        steps.append((38,))
        steps.append((38,))
    return case_of(ops + steps, stream, fill=rbytes(r, 600), queues=failure_queues(r, stream) if r.random() < 0.2 else {})


def reverse_case(r):
    width = r.choice([0, 1, 2, 3, 4, 7, 8, 16, 33])
    depth = r.choice([8, 15, 16, 24, 32])
    line_bytes = width * (depth >> 3) + r.choice([0, 0, 0, 1, 2])
    ops = [(29,)] + pokes_header(header_bytes(width, 1, depth)) + pokes_gll(linebytes=line_bytes)
    ops += [(34, 1, r.randrange(8, 100)), (34, 1, 8)]
    return case_of(ops, fill=rbytes(r, 600))


def rle_line_case(r):
    width = r.choice([1, 2, 3, 5, 16, 40, 127, 128, 129])
    depth = r.choice([8, 15, 16, 24, 32])
    stream = rle_body(width, 3, max(depth >> 3, 1), r, overrun=r.random() < 0.3, cut=r.random() < 0.25)
    ops = [(29,)] + pokes_header(header_bytes(width, 3, depth, 10)) + pokes_gll(line=(1, r.randrange(8, 100)), rle=1, left=3, linebytes=width * (depth >> 3),
                                                                           rtl=r.randrange(2))
    ops += [(35,), (35,), (33,), (33,)]
    return case_of(ops, stream, queues=failure_queues(r, stream) if r.random() < 0.2 else {})


def line_edges():
    r = random.Random(23)
    for rle in (0, 1, 2, -1):
        for rtl in (0, 1):
            for left in (0, 1):
                for stream in (b"", rbytes(r, 5), rbytes(r, 30), rle_body(4, 2, 3, r)):
                    ops = [(29,)] + pokes_header(header_bytes(4, 2, 24)) + pokes_gll(line=(1, 16), rle=rle, rtl=rtl, left=left, linebytes=12) + [(33,), (33,)]
                    yield case_of(ops, stream, fill=rbytes(r, 100))
    for width in (0, 1, 2, 3, 4, 5):
        for depth in (8, 15, 16, 24, 32):
            ops = [(29,)] + pokes_header(header_bytes(width, 2, depth)) + pokes_gll(linebytes=width * (depth >> 3)) + [(34, 1, 4), (36,), (32,)]
            yield case_of(ops, rbytes(r, 200), fill=rbytes(r, 200))
    # run-length packets: runs of one, of 128, reaching past the row, raw ending exactly, a raw packet cut short
    for width, depth in ((4, 8), (4, 16), (4, 24), (4, 32), (130, 8)):
        for packets in ([0x80 | 0, 7] + [0x80 | 2, 9] * 1, [3, 1, 2, 3, 4], [0x83, 5], [0x80 | 6, 5], [0x7F], [0x81, 1, 1, 9]):
            stream = bytes(b & 0xFF for b in packets) + bytes(range(1, 40))
            ops = [(29,)] + pokes_header(header_bytes(width, 1, depth, 10)) + pokes_gll(line=(1, 8), rle=1, left=1, linebytes=width * (depth >> 3)) + [(35,), (35,)]
            yield case_of(ops, stream)


# ---- DoRead and WriteLine

def doread_case(r):
    """A whole picture read by DoRead into an area: the loader's own area holds the file's format and size (the reader's header
    says the same), the destination area is another size or another format; rows top first or bottom first, flipped lines."""
    file_depth = r.choice([8, 15, 16, 24, 32])
    width, height = r.randint(0, 9), r.randint(0, 6)
    bpp = file_depth >> 3
    rle = r.choice([0, 0, 1])
    rtl = r.randrange(2)
    descriptor = (0x20 if r.random() < 0.5 else 0) | (0x10 if rtl else 0)
    fmt = {8: (0, 0, 0, 0, 8, 1), 15: (0x7C00, 0x3E0, 0x1F, 0, 16, 0), 16: (0x7C00, 0x3E0, 0x1F, 0, 16, 0), 24: (0xFF0000, 0xFF00, 0xFF, 0, 24, 0),
           32: (0xFF0000, 0xFF00, 0xFF, 0xFF000000, 32, 0)}[file_depth]
    ops = [(12,), (29,)]
    ops += pokes_area(3, m0=fmt[0], m1=fmt[1], m2=fmt[2], m3=fmt[3], depth=fmt[4], pal=fmt[5], width=width, height=height,
                      stride=width * ((fmt[4] + 7) >> 3), data=(0, 0))
    ops += pokes_header(header_bytes(width, height, file_depth, 10 if rle else 2, descriptor))
    ops += pokes_gll(rle=rle, rtl=rtl, bottomup=0 if descriptor & 0x20 else 1)
    if r.random() < 0.55:
        dest = fmt
    else:
        dest = r.choice([(0, 0, 0, 0, 8, 1), (0x7C00, 0x3E0, 0x1F, 0, 16, 0), (0xFF0000, 0xFF00, 0xFF, 0, 24, 0), (0xFF0000, 0xFF00, 0xFF, 0xFF000000, 32, 0),
                         (0xF000, 0xF00, 0xF0, 0xF, 16, 0), (fmt[0], fmt[1], fmt[2], fmt[3], fmt[4], 2 if fmt[5] else 0),
                         (fmt[0] ^ 1, fmt[1], fmt[2], fmt[3], fmt[4], fmt[5]), (fmt[0], fmt[1] ^ 1, fmt[2], fmt[3], fmt[4], fmt[5]),
                         (fmt[0], fmt[1], fmt[2] ^ 1, fmt[3], fmt[4], fmt[5]), (fmt[0], fmt[1], fmt[2], fmt[3] ^ 1, fmt[4], fmt[5]),
                         (fmt[0], fmt[1], fmt[2], fmt[3], fmt[4] ^ 8, fmt[5]), (fmt[0], fmt[1], fmt[2], fmt[3], fmt[4], 1 - (fmt[5] != 0))])
    dest_width = width if r.random() < 0.5 else r.randint(0, 11)
    dest_height = height if r.random() < 0.5 else r.randint(0, 8)
    dest_stride = dest_width * ((dest[4] + 7) >> 3) + r.choice([0, 0, 3])
    ops += pokes_area(0, m0=dest[0], m1=dest[1], m2=dest[2], m3=dest[3], depth=dest[4], pal=dest[5], width=dest_width, height=dest_height,
                      stride=dest_stride, data=(2, 0x800))
    ops += [(37, 0)]
    if rle:
        stream = rle_body(max(width, 1), max(height, 1), max(bpp, 1), r, overrun=r.random() < 0.1, cut=r.random() < 0.1)
    else:
        stream = rbytes(r, width * height * bpp + r.choice([0, 0, 0, 5]))
        if r.random() < 0.15:
            stream = stream[:r.randrange(0, len(stream) + 1)]
    queues = {"LbMem_Alloc": [r.choice([1, 1, 1, 1, 0])]} if r.random() < 0.1 else {}
    if r.random() < 0.1:
        queues["CFile.Read"] = [r.choice([0, 1, 3]) for _ in range(2)]
    return case_of(ops, stream, queues=queues)


def doread_edges():
    r = random.Random(31)
    for _ in range(40):
        yield doread_case(r)


def writeline_case(r):
    ops = [(29,)] + pokes_gll(line=(1, r.randrange(0, 100)), dest=(2, r.randrange(0x400, 0x1000)), destbytes=r.randrange(0, 64),
                              step=r.choice([0, 1, 7, -9, 40, -64]))
    ops += [(38,)] * r.randint(1, 3)
    return case_of(ops, fill=rbytes(r, 300))


def palette_case(r):
    count = r.choice([0, 1, 2, 7, 8, 9, 15, 16, 17, 24, 40, 100])
    ops = [(39, 2, r.randrange(0, 20), 1, r.randrange(0, 40), count)]
    return case_of(ops, fill=rbytes(r, 600))


def line_any(r):
    return r.choice([line_case, line_case, reverse_case, rle_line_case, writeline_case, doread_case, doread_case, palette_case])(r)


def run_bf(name, edge, rnd):
    return OnProgram(BULLFROG, name, ppc_bf, host_bf, edge=edge, rnd=rnd)


# =========================================================================== the cursor world, PowerPC side

SLOTS, UCURSORS, TABLES = 6, 3, 2
HEAP_NAMES = ["new", "new[]", "delete", "delete[]"]


def fake_ucursor_vtable(e):
    """The fake platform cursor's vtable (persistent): the deleting destructor in slot 0x8 and SetHotspot in slot 0x30."""
    if hasattr(e, "cc_ucursor_vtable"):
        return e.cc_ucursor_vtable
    vt = T.persist_alloc(e, 0x40)

    def destructor(emu):
        this, flag = emu.r(3), T.s16(emu.r(4))
        cl = emu.cl
        cl.rec("UCursor.dtor", (cl.lab(this),))
        if flag > 0:
            emu.cur.heap[2] += 1
            if cl.lab(this) > 0:
                cl.rec("delete", (cl.lab(this),))
            cl.forget(this)

    def set_hotspot(emu):
        this, point = emu.r(3), emu.r(4)
        x, y = struct.unpack(">ii", emu.read(point, 8))
        emu.cl.rec("UCursor.SetHotspot", (emu.cl.lab(this), x, y))

    e.write(vt + 8, struct.pack(">I", T.fake_function(e, destructor)))
    e.write(vt + 0x30, struct.pack(">I", T.fake_function(e, set_hotspot)))
    e.cc_ucursor_vtable = vt
    return vt


class Cur:
    """One case of the cursor world on the PowerPC side."""

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.heap = [0, 0, 0, 0]
        self.show = case.get("show", 1)
        self.slots = [[0, 0] for _ in range(SLOTS)]       # [address, kind] (kind 0: none, 1: base class, 2: InterfaceBFCursor)
        self.ucursors = [0] * UCURSORS
        self.tables = [0] * TABLES
        self.L = e.cl = CL.PpcLog({}, case.get("queues"))
        e.cur = self
        self.vtable = fake_ucursor_vtable(e)

    def stubs(self):
        S = CL.Stubs()
        S.real.update(UNIT_MAIN)
        e, L = self.e, self.L

        def operator_delete(e):
            block = e.r(3)
            if block:
                self.heap[2] += 1
                if L.lab(block) > 0:
                    L.rec("delete", (L.lab(block),))
                    L.forget(block)

        def operator_delete_array(e):
            if e.r(3):
                self.heap[3] += 1

        def operator_new_array(e):
            size = e.r(3)
            self.heap[1] += 1
            e.set_r(3, e.alloc(max(size, 4), b"\xa5" * max(size, 4)))

        def operator_new(e):
            size = e.r(3)
            self.heap[0] += 1
            e.set_r(3, e.alloc(max(size, 4), b"\xa5" * max(size, 4)))

        def show(e):
            L.rec("UCursor.Show")
            self.show += 1
            e.set_r(3, u32(self.show))

        def hide(e):
            L.rec("UCursor.Hide")
            self.show -= 1
            e.set_r(3, u32(self.show))

        def visible(e):
            L.rec("UCursor.IsVisible")
            e.set_r(3, 1 if self.show > 0 else 0)

        S.imp("__dl__FPv", operator_delete)
        S.imp("__dla__FPv", operator_delete_array)
        S.imp("__nwa__FUl", operator_new_array)
        S.imp("__nw__FUl", operator_new)
        S.imp("Show__Q213SamsUtilities7UCursorFv", show)
        S.imp("Hide__Q213SamsUtilities7UCursorFv", hide)
        S.imp("IsVisible__Q213SamsUtilities7UCursorFv", visible)
        S.imp("Set__Q213SamsUtilities7UCursorFv", lambda e: L.rec("UCursor.Set", (L.lab(e.r(3)),)))
        S.imp("Reset__Q213SamsUtilities7UCursorFv", lambda e: L.rec("UCursor.Reset", (L.lab(e.r(3)),)))
        return S

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MASK for a in args])[0]

    def op(self, code, a):
        e, L = self.e, self.L
        a = list(a) + [0] * (4 - len(a))
        if code in (100, 101):
            size = 0x14 if code == 100 else 0x18
            address = e.alloc(size, b"\xa5" * size)
            self.call(CUR_CTOR if code == 100 else BF_CTOR, address)
            self.slots[a[0]] = [address, 1 if code == 100 else 2]
            L.labels[address] = LAB_CURSOR + a[0]
            if code == 101 and a[1] >= 0:
                w32(e, address + 0x14, self.ucursors[a[1]])
        elif code == 102:
            address = e.alloc(0x20, b"\xa5" * 0x20)
            w32(e, address, self.vtable)
            self.ucursors[a[0]] = address
            L.labels[address] = LAB_UCURSOR + a[0]
        elif code == 103:
            address, kind = self.slots[a[0]]
            which = a[1]
            bf = kind == 2
            if which == 0:
                self.call(BF_DTOR if bf else CUR_DTOR, address, 0)
                self.slots[a[0]] = [0, 0]
            elif which == 1:
                self.call(BF_DTOR if bf else CUR_DTOR, address, 1)
                self.slots[a[0]] = [0, 0]
            elif which == 2:
                return self.call(BF_GETVIS if bf else CUR_GETVIS, address) & 0xFF
            elif which == 3:
                self.call(CUR_SETDATA, address, 0, a[2])
            elif which == 4:
                self.call(CUR_SETHOT, address, a[2], a[3])
            elif which == 5:
                self.call(BF_SHOW if bf else CUR_SHOW, address)
            elif which == 6:
                self.call(BF_HIDE if bf else CUR_HIDE, address)
            elif which == 7:
                self.call(CUR_DRAW, address, a[2], a[3])
            elif which == 8:
                self.call(BF_USE if bf else CUR_USE, address, a[2])
            else:
                self.call(CUR_ANIMATE, address, a[2])
        elif code == 104:
            address = self.slots[a[0]][0]
            if a[1] == 0:
                e.write(address, bytes([a[2] & 0xFF]))
            elif a[1] == 1:
                e.write(address + 1, bytes([a[2] & 0xFF]))
            elif a[1] == 2:
                e.write(address + 2, bytes([a[2] & 0xFF]))
            else:
                w32(e, address + 0xC, a[2])
        elif code == 110:
            address = e.alloc(8, b"\xa5" * 8)
            self.call(TAB_CTOR, address)
            self.tables[a[0]] = address
            L.labels[address] = LAB_TABLE + a[0]
        elif code == 111:
            return self.call(TAB_ALLOC, self.tables[a[0]], a[1]) & MASK
        elif code == 112:
            return self.call(TAB_SET, self.tables[a[0]], a[1], self.slots[a[2]][0] if a[2] >= 0 else 0) & MASK
        elif code == 113:
            return L.lab(self.call(TAB_GET, self.tables[a[0]], a[1]))
        elif code == 114:
            table = self.tables[a[0]]
            entries, count = e.u32(table), struct.unpack(">H", e.read(table + 4, 2))[0]
            if entries:
                for k in range(count):
                    for slot in self.slots:
                        if slot[0] and slot[0] == e.u32(entries + 4 * k):
                            slot[:] = [0, 0]
            self.call(TAB_DEALLOC, table)
        elif code == 115:
            self.call(TAB_DTOR, self.tables[a[0]], a[1])
            if a[1] > 0:
                self.tables[a[0]] = 0
        else:
            raise ValueError(f"bad operation {code}")
        return 0

    def dump(self):
        e, L = self.e, self.L
        out = []
        for address, kind in self.slots:
            out.append(kind)
            if kind == 0:
                continue
            d = bytes(e.read(address, 0x18 if kind == 2 else 0x14))
            out += [L.lab(address), d[0], T.s8(d[1]), T.s8(d[2]), int(d[3:12] == b"\xa5" * 9), struct.unpack(">I", d[12:16])[0]]
            if kind == 2:
                out.append(L.lab(struct.unpack(">I", d[0x14:0x18])[0]))
        for address in self.tables:
            out.append(1 if address else 0)
            if address == 0:
                continue
            entries, count = e.u32(address), struct.unpack(">H", e.read(address + 4, 2))[0]
            out += [count, 1 if entries else 0]
            for k in range(count if entries else 0):
                if k < 16:
                    out.append(L.lab(e.u32(entries + 4 * k)))
        out.append(self.show)
        return out


def ppc_cur(e, case):
    world = Cur(e, case)
    stubs = world.stubs()
    saved = stubs.activate(e)
    try:
        steps = []
        for op in case["ops"]:
            answer = world.op(op[0], op[1:])
            steps.append((answer, world.dump()))
        return dict(steps=steps, heap=list(world.heap), log=world.L.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host_cur(lib, case):
    open_host(lib)
    lib.tpw_cc_begin(b"", 0)
    lib.tpw_cc_set_show_count(case.get("show", 1))
    CL.host_begin(lib, {}, case.get("queues"))
    lib.tpw_cc_labels()
    for k in range(4):
        C.c_int32.in_dll(lib, "tpw_cl_heap")  # (the counters are read below)
    heap = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
    for k in range(4):
        heap[k] = 0
    steps = []
    for op in case["ops"]:
        answer = host_op(lib, op[0], op[1:])
        out = (C.c_int64 * 512)()
        n = lib.tpw_cc_dump_cursors(out)
        steps.append((answer, list(out[:n])))
    return dict(steps=steps, heap=list(heap), log=CL.host_end(lib))


# =========================================================================== the cases of the cursor world


def cursor_script(r):
    """A random script over six cursor slots, three platform cursors and two cursor tables, valid for the original too: a
    cursor in a table is only deleted by the table, a table is emptied once between allocations."""
    kinds = [0] * SLOTS
    in_table = [False] * SLOTS
    made = [False] * UCURSORS
    used = [False] * UCURSORS
    tables = [dict(made=False, count=0, entries={}) for _ in range(TABLES)]
    ops = []
    for _ in range(r.randint(4, 24)):
        choice = r.randrange(11)
        free = [i for i in range(SLOTS) if kinds[i] == 0]
        live = [i for i in range(SLOTS) if kinds[i] != 0]
        if choice == 0 and free:
            slot = r.choice(free)
            ops.append((100, slot))
            kinds[slot] = 1
        elif choice == 1:
            j = r.randrange(UCURSORS)
            if not made[j]:
                ops.append((102, j))
                made[j] = True
        elif choice == 2 and free:
            slot = r.choice(free)
            options = [-1] + [j for j in range(UCURSORS) if made[j] and not used[j]]
            j = r.choice(options)
            ops.append((101, slot, j))
            kinds[slot] = 2
            if j >= 0:
                used[j] = True
        elif choice in (3, 4, 5) and live:
            slot = r.choice(live)
            which = r.choice([2, 3, 4, 5, 6, 7, 8, 9, 5, 6, 8, 2])
            ops.append((103, slot, which, r.randint(-130, 130), r.randint(-130, 130)))
        elif choice == 6 and live:
            slot = r.choice(live)
            ops.append((104, slot, r.randrange(4), r.choice([0, 1, 2, 0x7F, 0x80, 0xFF, r.getrandbits(32)])))
        elif choice == 7 and live:
            slot = r.choice([i for i in live if not in_table[i]] or [None])
            if slot is not None:
                ops.append((103, slot, r.choice([0, 1])))
                kinds[slot] = 0
        elif choice == 8:
            t = r.randrange(TABLES)
            table = tables[t]
            if not table["made"]:
                ops.append((110, t))
                table["made"] = True
            elif table["count"] == 0 and not table["entries"] and r.random() < 0.7:
                table["count"] = r.choice([0, 1, 3, 6, 8])
                ops.append((111, t, table["count"]))
                table["allocated"] = True
            elif table["count"] > 0:
                index = r.randrange(0, table["count"] + 2)
                candidates = [i for i in live if not in_table[i] and kinds[i] in (1, 2)]
                if candidates and r.random() < 0.75:
                    slot = r.choice(candidates)
                    ops.append((112, t, index, slot))
                    if index < table["count"] and index not in table["entries"]:
                        table["entries"][index] = slot
                        in_table[slot] = True
                else:
                    ops.append((112, t, index, -1))
        elif choice == 9:
            t = r.randrange(TABLES)
            if tables[t]["made"]:
                ops.append((113, t, r.randrange(0, tables[t]["count"] + 3)))
        elif choice == 10:
            t = r.randrange(TABLES)
            table = tables[t]
            if table["made"] and table.get("allocated"):
                ops.append((114, t))
                for slot in table["entries"].values():
                    kinds[slot] = 0
                    in_table[slot] = False
                table["entries"] = {}
                table["count"] = 0
                table["allocated"] = False
                table["freed"] = True
            elif table["made"] and r.random() < 0.3:
                flag = r.choice([0, 1])
                ops.append((115, t, flag))
                if flag:
                    table["made"] = False
                    for slot in table["entries"].values():
                        in_table[slot] = False
                    table["entries"] = {}
    return dict(ops=ops, show=r.choice([1, 1, 0, 2, -1, 3]), queues={})


def cursor_edges():
    yield dict(ops=[(100, 0), (103, 0, 2), (103, 0, 5), (103, 0, 2), (103, 0, 6), (103, 0, 2), (103, 0, 4, 10, -20), (103, 0, 3, 5), (103, 0, 7, 1, 2),
                    (103, 0, 8, 1), (103, 0, 9, 7), (104, 0, 3, 99), (104, 0, 0, 7), (103, 0, 2), (103, 0, 1)], show=1, queues={})
    for show in (1, 0, -1, 2):
        yield dict(ops=[(102, 0), (101, 0, 0), (101, 1, -1), (103, 0, 2), (103, 1, 2), (103, 0, 5), (103, 0, 5), (103, 0, 6), (103, 0, 6), (103, 0, 6),
                        (103, 0, 2), (103, 0, 8, 1), (103, 0, 8, 0), (103, 1, 8, 1), (103, 1, 5), (103, 1, 6), (103, 1, 2), (103, 0, 4, 3, -3),
                        (103, 0, 8, 7), (103, 0, 1), (103, 1, 1)], show=show, queues={})
        yield dict(ops=[(102, 0), (101, 0, 0), (103, 0, 0)], show=show, queues={})
    yield dict(ops=[(100, 0), (101, 1, -1), (102, 0), (101, 2, 0), (110, 0), (111, 0, 3), (112, 0, 0, 0), (112, 0, 1, 1), (112, 0, 2, 2), (112, 0, 1, 2),
                    (112, 0, 3, 2), (112, 0, 2, -1), (113, 0, 0), (113, 0, 1), (113, 0, 2), (113, 0, 3), (113, 0, 9), (114, 0), (113, 0, 0), (114, 0)][:19],
               show=1, queues={})
    yield dict(ops=[(110, 0), (113, 0, 0), (112, 0, 0, -1), (111, 0, 0), (113, 0, 0), (112, 0, 0, -1), (114, 0), (115, 0, 0), (110, 0), (115, 0, 1)], show=1, queues={})
    yield dict(ops=[(110, 0), (114, 0), (115, 0, 0)], show=1, queues={})


# ---- InterfaceBFCursor::LoadData over the real loader, on the game's own cursor files: the host against a plain Python reading of the file

PIXEL_FORMATS = {1: 1, 2: 5, 3: 8, 4: 11}   # bytes a pixel -> kPixels8Indexed, 16BGRA, 24BGR, 32BGRA


def reference_cursor(case):
    """What LoadData must hand the platform for an uncompressed TGA of cursor frames: the pictures' rows top first (the file's
    rows are bottom first unless the descriptor says otherwise), a frame every 32 pixels with the format of the depth, and the
    palette of a colour-mapped file (red, green, blue, 0) converted by the fake (each byte exclusive-or 0x5a)."""
    data = case["stream"]
    ident, map_type, kind, map_start, map_length, map_bits, ox, oy, width, height, depth, descriptor = struct.unpack("<BBBHHBHHHHBB", data[:18])
    format_depth = {8: 8, 15: 16, 16: 16, 24: 24, 32: 32}[depth]
    bpp = (format_depth + 7) >> 3
    at = 18 + ident
    palette = bytearray(0x400)
    if kind == 1:
        for i in range(map_length):
            b, g, r = data[at + 3 * i:at + 3 * i + 3]
            palette[4 * i:4 * i + 4] = bytes([r, g, b, 0])
        at += map_length * 3
    line = width * (depth >> 3)
    rows = [data[at + i * line:at + (i + 1) * line] for i in range(height)]
    rows = [r + b"\0" * (width * bpp - len(r)) for r in rows]
    if (descriptor & 0x20) == 0:
        rows.reverse()
    area = b"".join(rows) + b"\xa5" * MARGIN
    frames = width >> 5
    palette_sum = fnv(bytes(b ^ 0x5A for b in palette)) if kind == 1 else 0
    out = dict(result=1, frames=frames, set_data=[], wrap=None)
    for i in range(frames):
        offset = i * bpp * 32
        out["set_data"].append((PIXEL_FORMATS[bpp], offset, 32, 32, width * bpp, fnv(area[offset:offset + 64]), palette_sum))
    if kind == 1:
        out["wrap"] = (10, fnv(bytes(palette)), 256, 1, -1, 1)
    return out


def ppc_loaddata(e, case):
    return reference_cursor(case)


def host_loaddata(lib, case):
    open_host(lib)
    stream = bytes(case["stream"])
    lib.tpw_cc_begin(stream, len(stream))
    for index, text in enumerate(case["texts"]):
        lib.tpw_cc_set_text(index, text.encode("latin-1"))
    CL.host_begin(lib, {}, {})
    lib.tpw_cc_labels()
    result = host_op(lib, 120, (0, 0))
    log = CL.host_end(lib)
    out = dict(result=result, frames=0, set_data=[], wrap=None)
    for name, ints, texts in log:
        if name == "UPixels.SetData":
            out["set_data"].append(tuple(ints[1:6]) + tuple(ints[6:8]))
            out["frames"] += 1
        elif name == "UPixels.wrap":
            out["wrap"] = (ints[1], ints[2], ints[3], ints[4], ints[5], ints[6])
    out["set_data"] = [(f, o, w, h, st, cs, ps) for (f, o, w, h, st, cs, ps) in out["set_data"]]
    return out


def loaddata_cases():
    for name, data in real_cursors():
        yield case_of([], data, texts=[f"data:UI:cursors:{name}"])


TESTS = [
    run_bf("TbRenderArea", ra_edges, ra_any),
    run_bf("TbGraphicFileLoader and TbTGALoader, whole pictures", pipeline_edges, pipeline_random),
    run_bf("TbTGALoader and _TgaGLL header reader, random headers", lambda: iter(()), random_header_case),
    run_bf("_TgaGLL line reader and _AndyGLL::DoRead", lambda: list(line_edges()) + list(doread_edges()), line_any),
    OnProgram(MAIN, "InterfaceCursor, CursorTable and InterfaceBFCursor", ppc_cur, host_cur, edge=cursor_edges, rnd=cursor_script),
    FnTest("InterfaceBFCursor::LoadData over the real loader, the game's cursor files", ppc_loaddata, host_loaddata, edge=loaddata_cases,
           rnd=lambda r: r.choice(list(loaddata_cases()))),
]
