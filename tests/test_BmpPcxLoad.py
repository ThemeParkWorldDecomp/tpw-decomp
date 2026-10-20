"""Unit w220: the 17 BMP and PCX loader functions of bullfrog symbols shared.
The original and host run scripts on 0xa5-filled loaders, readers and pixel blocks.
Compare results, raw headers, all fields, every heap byte including freed allocations
and margins, and file, heap and converter call logs. DoRead and WriteLine are real.
There are no floating-point operations. Excluded: palette counts above 256 and unterminated
BMP RLE streams, whose unchecked accesses have no portable bounded observation.
"""
import ctypes as C
import random
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_BmpPcxLoad"
MASK = 0xffffffff
MARGIN, BLOCK = 0x400, 0x4000
EXTENT = BLOCK + MARGIN
FIRST_HEAP_LABEL = 100
BMP = [0x1001e478, 0x1001e36c, 0x1001e9fc, 0x1001ec08, 0x1001f004,
       0x1001ede8, 0x10023190, 0x10023234, 0x100232e0]
PCX = [0x1001fb7c, 0x1001fa70, 0x1001ff38, 0x1001fff0, 0x10020294,
       None, 0x10023630, 0x100236d4, 0x10023780]
OWN = set(BMP + [v for v in PCX if v])
DO_READ, WRITE_LINE = 0x1001de44, 0x1001e2f4
FILE_CTOR, FILE_DTOR, FILE_READ, FILE_SEEK, FILE_SIZE = 0x1000b1dc, 0x1000b2b8, 0x10000f28, 0x1000b504, 0x1000b588
WAD_OPEN, BUFFERED_OPEN, MEM_ALLOC, MEM_FREE, SURFACE_AREA = 0x1000cc48, 0x10049430, 0x10013ea4, 0x10013efc, 0x10066e84
RECT_INTERSECTION, RECT_EMPTY = 0x10029ad0, 0x10029cd4
CONV_CTOR, CONV_INIT, CONV_BUF, CONV_RELEASE = 0x10016b10, 0x10016b4c, 0x10016e5c, 0x10016ff4
FORMATS = {0x10073ed0: (0, 0, 0, 0, 8, 1), 0x10073eb8: (0x7c00, 0x3e0, 0x1f, 0, 16, 0),
           0x10073ea0: (0xff0000, 0xff00, 0xff, 0, 24, 0), 0x10073e88: (0xff0000, 0xff00, 0xff, 0xff000000, 32, 0)}
RESULT_WORDS = {0x10072f94: 0, 0x10072f98: 0, 0x10072f9c: 0, 0x10072fa0: 0,
                0x10072dfc: 0, 0x10072e64: MASK, 0x10072d84: 0, 0x10072d88: 0,
                0x10072d8c: 0, 0x10072d90: 0, 0x10072d98: 0}


def s32(v):
    return T.s32(v)


def u32(v):
    return v & MASK


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Bf:
    """Original objects, the streaming file and recording callee fakes."""

    def __init__(self, e, case):
        self.e, self.case = e, case
        self.kind = case["kind"]
        self.stream = case["stream"]
        self.pos = 0
        self.heap, self.freed = {}, set()
        self.deletes = 0
        fill = case.get("fill", b"")[:BLOCK]
        self.blocks = [e.alloc(EXTENT, b"\xa5" * EXTENT), e.alloc(EXTENT, fill + b"\xa5" * (EXTENT - len(fill)))]
        self.area = e.alloc(0x28, b"\xa5" * 0x28)
        self.areas = [self.area] * 3
        self.loader = e.alloc(0x600, b"\xa5" * 0x600)
        self.gll = e.alloc(0x100, b"\xa5" * 0x100)
        self.file0 = e.alloc(16)
        w32(e, self.file0, 0x10073038)
        self.registry = [(self.blocks[0], EXTENT, 8), (self.blocks[1], EXTENT, 9),
                         (self.area, 0x28, 2), (self.loader, 0x600, 1), (self.gll, 0x100, 6)]
        self.L = e.cl = CL.PpcLog({self.area: 2, self.loader + 4: 5, self.loader + 0x2c: 11,
                                 self.gll: 6, self.file0: 7, self.blocks[0]: 8, self.blocks[1]: 9}, case.get("queues"))
        for a, words in FORMATS.items():
            e.write(a, struct.pack(">6I", *words))
        for a, word in RESULT_WORDS.items():
            w32(e, a, word)
        e.write(0x1007479c, b"\0" * 14)

    def describe(self, p):
        if p == 0:
            return [0, 0]
        for start, extent, label in self.registry:
            if start <= p <= start + extent:
                return [label, p - start]
        if p == 0xa5a5a5a5:
            return [-2, 0]
        for start, extent, label in self.registry:
            if label < 100 and p < start and start - p <= 0x1000:
                return [label, p - start]
        return [-1, p & MASK]

    def stubs(self):
        S = CL.Stubs()
        S.real.update(OWN | {0x10022870, DO_READ, WRITE_LINE})
        e, L = self.e, self.L
        rec = L.rec

        def file_ctor(e):
            rec("CFile.ctor", (L.newlab(e.r(3)),))

        def file_dtor(e):
            rec("CFile.dtor", (L.lab(e.r(3)),))

        def file_read(e):
            this, dest, size = e.r(3), e.r(4), e.r(5)
            target = self.describe(dest)
            if dest == 0x1007479c:
                target = [12, 0]
            elif dest == self.loader + 0x430:
                target = [13, 0]
            elif self.kind == 1 and dest in (self.gll + 0x30, self.gll + 0x31):
                target = [14 + dest - (self.gll + 0x30), 0]
            elif dest == self.loader + 0x2c:
                target = [16, 0]
            elif target[0] == -1:
                target = [-3, 0]  # local bytes of ValidImage
            rec("CFile.Read", (L.lab(this), size, *target))
            count = min(size, max(0, len(self.stream) - self.pos))
            if dest and count > 0:
                e.write(dest, self.stream[self.pos:self.pos + count])
            self.pos += count
            answer = u32(L.ret("CFile.Read", count))
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
            if L.lab(block) >= FIRST_HEAP_LABEL and L.lab(block) in self.heap and L.lab(block) not in self.freed:
                self.freed.add(L.lab(block))
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

    def result(self, address, *args):
        out = self.e.alloc(4, b"\xa5" * 4)
        self.call(address, out, *args)
        return s32(self.e.u32(out))

    def op(self, code, a):
        e, g, ld = self.e, self.gll, self.loader
        a = list(a) + [0] * (6 - len(a))
        if code < 9:
            address = (BMP if self.kind == 0 else PCX)[code]
            if code == 0:
                return self.result(address, g, self.file0, ld)
            if code == 1:
                return self.result(address, g, self.area, self.file0, ld)
            if code in (2, 3, 5):
                return self.result(address, g, self.file0)
            if code == 4:
                return u32(self.call(address, g))
            if code == 6:
                return self.result(address, ld, self.file0, self.area)
            if code == 7:
                return self.result(address, ld, self.area, self.file0, self.area)
            return self.call(address, ld, self.file0) & 0xff
        if code == 40:
            base = self.area if a[0] == 0 else ld + 4
            value = self.blocks[a[2] - 1] + a[3] if a[1] == 6 and a[2] else a[2]
            w32(e, base + 4 * a[1], value)
        elif code == 41:
            offsets = [0, 4, 8, 12, 16, 20, 24]
            offsets += [0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38] if self.kind == 0 else [0x20, 0x24, 0x30, 0x31]
            pointer_fields = (0, 5, 9, 10) if self.kind == 0 else (0, 5)
            value = (self.blocks[a[1] - 1] + a[2] if a[1] else 0) if a[0] in pointer_fields else a[1]
            if self.kind == 1 and a[0] in (9, 10):
                e.write(g + offsets[a[0]], bytes([value & 0xff]))
            else:
                w32(e, g + offsets[a[0]], value)
        elif code == 42:
            e.write(ld + 0x2c + a[0], bytes([a[1] & 0xff]))
        elif code == 43:
            w32(e, ld + 0x42c, a[0])
        elif code == 44:
            self.pos = a[0]
        elif code == 45:
            base = (g + (0x3c if self.kind == 0 else 0x32)) if a[0] == 0 else ld + 0x430
            e.write(base + a[1], bytes([a[2] & 0xff]))
        else:
            raise ValueError(code)
        return 0

    def dump(self):
        e, g, ld = self.e, self.gll, self.loader
        words = lambda a, n: list(struct.unpack(">%dI" % n, e.read(a, n * 4)))
        out = []
        for base in (self.area, ld + 4):
            out += words(base, 6) + self.describe(e.u32(base + 24)) + words(base + 28, 3)
        out += [e.u32(ld + 0x42c)] + list(e.read(ld + 0x2c, 0x400))
        count = 0x28 if self.kind == 0 else 0x80
        out += list(e.read(ld + 0x430, count))
        out += self.describe(e.u32(g)) + words(g + 4, 4)
        out += self.describe(e.u32(g + 20)) + words(g + 24, 1)
        if self.kind == 0:
            out += words(g + 0x20, 2) + self.describe(e.u32(g + 0x28)) + self.describe(e.u32(g + 0x2c)) + words(g + 0x30, 3)
        else:
            out += words(g + 0x20, 2) + list(e.read(g + 0x30, 2))
        out += list(e.read(g + (0x3c if self.kind == 0 else 0x32), count))
        out += list(e.read(0x1007479c, 14)) + [self.pos]
        return out

    def memory(self):
        out = [(8 + i, EXTENT, 0, self.e.read(a, EXTENT)) for i, a in enumerate(self.blocks)]
        for label, (address, size) in self.heap.items():
            out.append((label, size + MARGIN, int(label in self.freed), self.e.read(address, size + MARGIN)))
        return out


def ppc(e, case):
    world = Bf(e, case)
    saved = world.stubs().activate(e)
    try:
        world.call(0x10022870, world.loader)
        w32(e, world.gll + 0x1c, 0x10072f64 if case["kind"] == 0 else 0x10072f0c)
        steps = []
        for code, *args in case["ops"]:
            answer = world.op(code, args)
            if code < 9:
                steps.append((answer, world.dump(), world.memory()))
        return dict(steps=steps, log=world.L.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def bind(lib):
    if getattr(lib, "bpl_bound", False):
        return
    lib.tpw_bpl_begin.argtypes = [C.c_int32, C.c_char_p, C.c_int32, C.c_char_p, C.c_int32]
    lib.tpw_bpl_labels.argtypes = []
    lib.tpw_bpl_op.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_bpl_op.restype = C.c_int64
    lib.tpw_bpl_dump.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bpl_memory.argtypes = [C.c_int32, C.c_void_p, C.POINTER(C.c_int64)]
    lib.bpl_bound = True


def host_dump(lib):
    out = (C.c_int64 * 1600)()
    n = lib.tpw_bpl_dump(out)
    return list(out[:n])


def host_memory(lib):
    out, meta, result = C.create_string_buffer(0x11000), (C.c_int64 * 3)(), []
    while lib.tpw_bpl_memory(len(result), out, meta):
        result.append((*meta, out.raw[:meta[1]]))
    return result


def host(lib, case):
    bind(lib)
    stream, fill = case["stream"], case.get("fill", b"")
    lib.tpw_bpl_begin(case["kind"], stream, len(stream), fill, len(fill))
    CL.host_begin(lib, {}, case.get("queues"))
    lib.tpw_bpl_labels()
    steps = []
    for code, *args in case["ops"]:
        answer = lib.tpw_bpl_op(code, (C.c_int64 * 6)(*args))
        if code < 9:
            steps.append((answer, host_dump(lib), host_memory(lib)))
    return dict(steps=steps, log=CL.host_end(lib))


# File builders preserve the file bytes and can deliberately use the Windows byte order.
def bmp(width, height, bpp, compression=0, used=0, palette=b"", pixels=b"", magic=b"MB", size=None,
        offbits=None, info_size=0x28, planes=1, image_size=None, little=False):
    endian = "<" if little else ">"
    if little and magic == b"MB":
        magic = b"BM"
    info = struct.pack(endian + "IIIHHIIIIII", info_size, u32(width), u32(height), planes, bpp, u32(compression),
                       len(pixels) if image_size is None else image_size, 0, 0, used, 0)
    total = 54 + len(palette) + len(pixels)
    head = magic + struct.pack(endian + "IHHI", total if size is None else u32(size), 0, 0,
                               54 + len(palette) if offbits is None else u32(offbits))
    return head + info + palette + pixels


def rle8(ops):
    out = bytearray()
    for op in ops:
        if op[0] == "run":
            out += bytes(op[1:])
        elif op[0] == "abs":
            data = bytes(op[1])
            out += bytes((0, len(data))) + data + b"\0" * (len(data) & 1)
        elif op[0] == "delta":
            out += bytes((0, 2, op[1], op[2]))
        else:
            out += bytes((0, 0 if op[0] == "eol" else 1))
    return bytes(out)


def pcx(width, height, planes, bpl=None, body=b"", palette=True, bits=8, xmin=0, ymin=0, little=False):
    header = bytearray(128)
    header[:4] = bytes((10, 5, 1, bits))
    endian = "<" if little else ">"
    struct.pack_into(endian + "4H", header, 4, xmin, ymin, (xmin + width - 1) & 0xffff, (ymin + height - 1) & 0xffff)
    header[0x41] = planes
    struct.pack_into(endian + "H", header, 0x42, width if bpl is None else bpl)
    pal = bytes((12 if palette else 11,)) + bytes((i * 37 + 11) & 255 for i in range(768))
    return bytes(header) + body + pal


def pcx_rle(data):
    out = bytearray()
    at = 0
    while at < len(data):
        count = 1
        while at + count < len(data) and count < 63 and data[at + count] == data[at]:
            count += 1
        value = data[at]
        out += bytes((0xc0 | count, value)) if count > 1 or value >= 0xc0 else bytes((value,))
        at += count
    return bytes(out)


def case(kind, ops, stream=b"", queues=None, fill=b""):
    return dict(kind=kind, ops=ops, stream=bytes(stream), queues=queues or {}, fill=bytes(fill))


def rb(r, n):
    return bytes(r.randrange(256) for _ in range(n))


def header_ops(data, loader=False):
    return [(45, int(loader), i, b) for i, b in enumerate(data)]


def area_ops(depth=8, width=7, height=4, stride=None):
    fmt = FORMATS[0x10073ed0 if depth == 8 else 0x10073ea0]
    values = list(fmt) + [1, width, height, width * ((depth + 7) >> 3) if stride is None else stride]
    return [(40, 0, i, v, 0x800) for i, v in enumerate(values)]


def line_setup(kind, header, size=128, left=4, linebytes=16, run=0, value=0):
    ops = header_ops(header) + [(41, 0, 1, 64), (41, 1, size)]
    if kind == 0:
        return ops + [(41, 7, left), (41, 8, linebytes)]
    return ops + [(41, 7, linebytes), (41, 8, left), (41, 9, run), (41, 10, value)]


def header_edges(kind, op):
    if kind == 0:
        for n in (0, 1, 2, 13, 14, 15, 53):
            stream = bytearray(bmp(3, 2, 8)[:n])
            if n >= 6:
                struct.pack_into(">I", stream, 2, n)
            yield case(0, [(op,)], stream)
        variants = [dict(magic=b"BM"), dict(magic=b"xx"), dict(size=53), dict(info_size=39), dict(width=0),
                    dict(width=-1), dict(height=0), dict(height=-1), dict(planes=2), dict(bpp=1), dict(bpp=16),
                    dict(bpp=32), dict(little=True), dict(offbits=999), dict(compression=-1)]
        for changes in variants:
            args = dict(width=3, height=2, bpp=8)
            args.update(changes)
            yield case(0, [(op,)], bmp(**args))
        for bpp in (4, 8, 24):
            for used in (0, 1, 7, 8, 9, 255, 256):
                palette = bytes((i * 29 + 13) & 255 for i in range(used * 4))
                yield case(0, [(op,)], bmp(9, 2, bpp, used=used, palette=palette, pixels=b"\x89" * 64))
        for queues in ({"CFile.Read": [13]}, {"CFile.Read": [14, 39]}, {"CFile.Read": [14, 40, 3]},
                       {"CFile.Seek": [-1]}, {"CFile.GetSize": [-1]}):
            yield case(0, [(op,)], bmp(2, 1, 8, used=1, palette=b"abcd"), queues)
        yield case(0, [(op,)], bmp(2, 1, 8, used=2, palette=b"a"))
    else:
        for n in (0, 1, 127, 128, 0x200, 0x300):
            stream = pcx(2, 2, 1)
            stream = stream[:n] if n <= len(stream) else stream + b"\0" * (n - len(stream))
            yield case(1, [(op,)], stream)
        for bits in (0, 4, 8):
            for planes in (0, 1, 2, 3, 4):
                yield case(1, [(op,)], pcx(5, 3, planes, bits=bits))
        for marker in (False, True):
            for planes in (1, 3):
                yield case(1, [(op,)], pcx(5, 3, planes, palette=marker))
        stream = pcx(3, 2, 1, body=b"\x12" * 8)
        for queues in ({"LbMem_Alloc": [0]}, {"CFile.Read": [127]}, {"CFile.Read": [128, 768]},
                       {"CFile.Seek": [-1]}, {"CFile.Seek": [len(stream) - 769, -1]}, {"CFile.GetSize": [-1]}):
            yield case(1, [(op,)], stream, queues)
        yield case(1, [(op,)], pcx(3, 5, 3, little=True))
        yield case(1, [(op,)], pcx(1, 1, 1, xmin=10, ymin=20))
        # Header dimensions are not checked for positivity.
        stream = bytearray(pcx(1, 1, 1, xmin=10, ymin=20))
        struct.pack_into(">HH", stream, 8, 4, 8)
        yield case(1, [(op,)], stream)


def header_random(kind, op, r):
    if kind == 0:
        args = dict(width=r.choice([0, -1, 1, 2, 9, 17]), height=r.choice([0, -1, 1, 3]),
                    bpp=r.choice([4, 8, 24, 1, 16, 32]), compression=r.choice([0, 1, 2, -1]),
                    used=r.choice([0, 1, 8, 9, 255, 256]), planes=r.choice([1, 1, 1, 2]))
        args["palette"] = rb(r, args["used"] * 4)
        args["pixels"] = rb(r, 100)
        if r.random() < 0.12:
            args[r.choice(["info_size", "size", "offbits"])] = r.randrange(1000)
        stream = bmp(**args)
    else:
        stream = pcx(r.randint(1, 32), r.randint(1, 12), r.choice([0, 1, 1, 3, 3, 2, 4]),
                     bpl=r.randint(1, 35), body=rb(r, 40), palette=r.choice([True, False]), bits=r.choice([8, 8, 8, 4]))
    if r.random() < 0.15:
        stream = stream[:r.randrange(len(stream))]
    queues = {}
    if r.random() < 0.15:
        queues = {r.choice(["CFile.Read", "CFile.Seek", "CFile.GetSize", "LbMem_Alloc"]): [r.choice([-1, 0, 1, 13, 128])]}
    return case(kind, [(op,)], stream, queues)


def valid_edges(kind):
    streams = [b"", b"M", b"MB", b"BM", b"xx", b"\x0a", b"\x0b", b"\0\0", b"\x0ahello"]
    for stream in streams:
        yield case(kind, [(8,)], stream)
    for queues in ({"CFile.Read": [0]}, {"CFile.Read": [-1]}, {"CFile.GetSize": [-1]}, {"CFile.GetSize": [0x80000000]}):
        yield case(kind, [(8,)], b"MB" if kind == 0 else b"\x0a", queues)
    yield case(kind, [(44, 1), (8,), (8,)], b"xMB" if kind == 0 else b"x\x0a\x0b")


def valid_random(kind, r):
    stream = rb(r, r.randrange(16))
    if r.random() < 0.4:
        stream = (b"MB" if kind == 0 else b"\x0a") + stream
    queues = {r.choice(["CFile.Read", "CFile.GetSize"]): [r.choice([-1, 0, 1, 2])]} if r.random() < 0.25 else {}
    return case(kind, [(8,)], stream, queues)


def length_edges(kind):
    if kind == 0:
        for w in (0, 1, 2, 3, 4, 5, 7, 8, 9, 0x1fffffff, 0x20000000, 0x7fffffff, 0x80000000, -1):
            for bpp in (0, 1, 4, 8, 24, 65535):
                yield case(0, header_ops(bmp(w, 1, bpp)[14:54]) + [(4,)])
    else:
        for planes in (0, 1, 2, 3, 4, 255):
            for bpl in (0, 1, 2, 255, 256, 65535):
                yield case(1, header_ops(pcx(1, 1, planes, bpl=bpl)[:128]) + [(4,)])


def length_random(kind, r):
    header = bmp(r.getrandbits(32), 1, r.choice([4, 8, 24, r.getrandbits(16)]))[14:54] if kind == 0 else pcx(1, 1, r.randrange(256), bpl=r.randrange(65536))[:128]
    return case(kind, header_ops(header) + [(4,)])


def bmp_line(w=7, h=3, bpp=8, compression=0, first=True, left=3, linebytes=None, stream=None, queues=None, calls=1):
    count = (((w + (1 if bpp == 4 else 0)) * bpp >> 3) + 3) & ~3
    if stream is None:
        stream = bytes((i * 37 + 0x81) & 255 for i in range(count * max(1, abs(h))))
    header = bmp(w, h, bpp, compression=compression, pixels=stream)[14:54]
    ops = line_setup(0, header, left=left, linebytes=count if linebytes is None else linebytes)
    ops += [(2 if first else 3,)] + [(3,)] * (calls - 1)
    return case(0, ops, stream, queues)


def rle_case(r=None, direct=True, variant=0):
    r = r or random.Random(variant + 31)
    streams = [rle8([("run", 7, 99), ("eol",)]), rle8([("eob",)]),
               rle8([("abs", b"abc"), ("abs", b"defg"), ("eol",)]),
               rle8([("run", 2, 5), ("delta", 3, 0), ("run", 3, 9), ("eob",)]),
               rle8([("run", 2, 5), ("delta", 3, 2), ("run", 3, 9), ("eob",)]),
               rle8([("run", 90, 12), ("eob",)])]
    stream = streams[variant] if variant < len(streams) else rle8([
        ("run", r.randint(1, 20), r.randrange(256)), ("abs", rb(r, r.randint(3, 13))),
        ("delta", r.randrange(8), r.randrange(4)), ("run", r.randint(1, 12), r.randrange(256)), ("eob",)])
    if direct:
        ops = [(41, 0, 1, 64), (41, 9, 2, 0), (41, 10, 2, 0), (41, 11, 0), (41, 12, 0), (41, 13, 0)]
        return case(0, ops + [(5,)] * 7, fill=stream + b"\0\x01" * 10)
    result = bmp_line(compression=1, stream=stream + b"\0\x01" * 10, calls=7)
    return result


def bmp_line_edges(first):
    for bpp in (4, 8, 24):
        for width in range(1, 10):
            yield bmp_line(width, 2, bpp, first=first, calls=4)
    for compression in (1, 2, 3, -1):
        for bpp in (4, 8, 24):
            if compression == 1 and bpp == 8:
                continue
            yield bmp_line(bpp=bpp, compression=compression, first=first)
    for h in (0, -1):
        yield bmp_line(h=h, first=first)
    yield bmp_line(first=first, stream=b"a")
    yield bmp_line(first=first, queues={"CFile.Read": [-1]})
    if first:
        for i in range(6):
            yield rle_case(direct=False, variant=i)
        yield bmp_line(compression=1, stream=b"\0\x01", queues={"LbMem_Alloc": [0]})
        c = bmp_line(compression=1, stream=b"\0\x01", queues={"CFile.Read": [1]})
        yield c
    else:
        yield bmp_line(first=False, left=0)
        yield bmp_line(first=False, left=1, bpp=4, linebytes=0, stream=b"")
        # ReadNextLine delegates compression 1 straight to the RLE decoder.
        c = rle_case(variant=4)
        c["ops"] = header_ops(bmp(8, 3, 8, compression=1)[14:54]) + c["ops"]
        c["ops"] = [(3,) if op[0] == 5 else op for op in c["ops"]]
        yield c


def bmp_line_random(first, r):
    if first and r.random() < 0.25:
        return rle_case(r, direct=False, variant=10)
    w, h, bpp = r.randint(1, 24), r.randint(1, 5), r.choice([4, 8, 24])
    c = bmp_line(w, h, bpp, compression=r.choice([0, 0, 0, 2, -1]), first=first,
                 left=r.randrange(5), calls=r.randint(1, 5))
    if r.random() < 0.15:
        c["stream"] = c["stream"][:r.randrange(len(c["stream"]) + 1)]
    return c


def pcx_line(planes=1, w=5, h=3, bpl=None, first=True, stream=None, run=0, value=0, left=3,
             decode=None, queues=None, calls=3):
    bpl = w if bpl is None else bpl
    if stream is None:
        stream = pcx_rle(bytes((i * 29 + 7) & 255 for i in range(planes * bpl * max(1, h))))
    header = pcx(w, h, planes, bpl=bpl)[:128]
    size = max(128, planes * bpl * 2)
    ops = line_setup(1, header, size=size, left=left, linebytes=planes * bpl if decode is None else decode, run=run, value=value)
    return case(1, ops + [(2 if first else 3,)] + [(3,)] * (calls - 1), stream, queues)


def pcx_line_edges(first):
    for planes in (1, 3):
        for width in range(1, 10):
            yield pcx_line(planes, width, 3, bpl=width + 2, first=first, calls=5)
        for n in (1, 2, 3, 7, 63):
            yield pcx_line(planes, 5, first=first, stream=bytes((0xc0 | n, 0xdf)) + b"abc" * 100)
        for stream in (b"", b"\xc1", b"\xc0\x55abcde" * 3, b"\xff\x37abcde" * 10):
            yield pcx_line(planes, first=first, stream=stream)
        yield pcx_line(planes, first=first, queues={"CFile.Read": [0]})
        yield pcx_line(planes, first=first, stream=b"\xc1\x55" * 100, queues={"CFile.Read": [1, 0]})
    if not first:
        for run in (1, 3, 7, 63, 255):
            for decode in (0, 5, 10):
                yield pcx_line(first=False, run=run, value=0xe3, decode=decode, stream=b"xyz" * 100)
        yield pcx_line(first=False, left=0)
    yield pcx_line(3, 1, first=first, bpl=0)


def pcx_line_random(first, r):
    planes, width = r.choice([1, 3]), r.randint(1, 20)
    bpl, height = width + r.randint(0, 3), r.randint(1, 5)
    raw = bytearray(rb(r, planes * bpl * height))
    for at in range(0, len(raw), 9):
        raw[at:at + 7] = bytes([r.randrange(256)]) * min(7, len(raw) - at)
    stream = pcx_rle(raw)
    if r.random() < 0.2:
        stream = stream[:r.randrange(len(stream) + 1)]
    if r.random() < 0.15:
        stream = b"\xc0\x55" + stream
    return pcx_line(planes, width, height, bpl, first, stream, run=r.randrange(64), value=r.randrange(256),
                    left=r.randrange(5), calls=r.randint(1, 6))


def pipeline(kind, op, r, variant=0, damage=False):
    width, height = r.randint(1, 16), r.randint(1, 5)
    if kind == 0:
        bpp = [4, 8, 24, 8][variant % 4]
        used = r.choice([0, 1, 16, 256]) if bpp != 24 else 0
        palette = rb(r, used * 4)
        count = (((width + (bpp == 4)) * bpp >> 3) + 3) & ~3
        if variant % 4 == 3:
            pixels = rle8(sum(([('run', width, r.randrange(256)), ('eol',)] for _ in range(height)), [])) + b"\0\x01"
        else:
            pixels = rb(r, count * height)
        stream = bmp(width, height, bpp, compression=int(variant % 4 == 3), used=used, palette=palette, pixels=pixels)
        depth = 24 if bpp == 24 else 8
    else:
        planes = 1 if variant % 2 == 0 else 3
        depth = 8 if planes == 1 else 24
        bpl = width + r.randrange(3)
        pixels = pcx_rle(rb(r, planes * bpl * height))
        stream = pcx(width, height, planes, bpl, pixels, palette=bool(variant % 3))
    # Cropping, bottom-up skip, excess rows and format conversion use the existing DoRead.
    dw, dh = max(1, width + r.choice([-1, 0, 2])), max(1, height + r.choice([-1, 0, 2]))
    if variant >= 4:
        depth = 24 if depth == 8 else 8
    queues = {}
    if damage and r.random() < 0.2:
        queues = {"LbMem_Alloc": [0]}
    if damage and r.random() < 0.15:
        stream = stream[:r.randrange(len(stream))]
    return case(kind, area_ops(depth, dw, dh, dw * ((depth + 7) >> 3) + 3) + [(op,)], stream, queues)


def pipeline_edges(kind, op):
    for variant in range(8):
        for seed in (1, 4, 9):
            yield pipeline(kind, op, random.Random(seed), variant)
    c = pipeline(kind, op, random.Random(7))
    c["queues"] = {"LbMem_Alloc": [0]}
    yield c
    yield case(kind, area_ops() + [(op,)], b"")


def edges(kind, op):
    if op in (0, 6):
        return header_edges(kind, op)
    if op in (1, 7):
        return pipeline_edges(kind, op)
    if op == 8:
        return valid_edges(kind)
    if op == 4:
        return length_edges(kind)
    if op == 5:
        cases = [rle_case(variant=i) for i in range(6)]
        c = rle_case(variant=3)
        c["ops"][6:6] = [(41, 11, -1), (41, 12, 3)]
        cases.append(c)
        return cases
    return bmp_line_edges(op == 2) if kind == 0 else pcx_line_edges(op == 2)


def random_case(kind, op, r):
    if op in (0, 6):
        return header_random(kind, op, r)
    if op in (1, 7):
        return pipeline(kind, op, r, r.randrange(8), damage=True)
    if op == 8:
        return valid_random(kind, r)
    if op == 4:
        return length_random(kind, r)
    if op == 5:
        return rle_case(r, variant=10)
    return bmp_line_random(op == 2, r) if kind == 0 else pcx_line_random(op == 2, r)


def function_test(kind, op):
    reader = "_AndyBMPGLL" if kind == 0 else "_AndyPCXGLL"
    loader = "TbBMPLoader" if kind == 0 else "TbPCXLoader"
    names = [reader + "::" + n for n in ("ReadHeaderInfo", "ReadData", "ReadFirstLine", "ReadNextLine", "GetSourceLineLength", "BMPLoadRLE")]
    names += [loader + "::" + n for n in ("ReadHeaderInfo", "ReadData", "ValidImage")]
    return FnTest(names[op], ppc, host, edge=lambda: edges(kind, op), rnd=lambda r: random_case(kind, op, r))


TESTS = [function_test(kind, op) for kind in (0, 1) for op in range(9) if kind == 0 or op != 5]
