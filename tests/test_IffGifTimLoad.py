"""Unit w223: IFF, GIF and TIM. Real readers and DoRead on both sides.
All fields, headers, statics, guarded heap bytes and file/heap/converter logs are compared.
TIM 16-bit cases use same-format destinations and normalize original pixel words.
GIF malformed blocks are bounded to 257 remaining bytes; unbounded corrupt dictionary walks
and table indices outside the original data are excluded.
"""
import ctypes as C
import random
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_IffGifTimLoad"
MASK = 0xffffffff
MARGIN, BLOCK = 0x400, 0x4000
EXTENT = BLOCK + MARGIN
FIRST_HEAP_LABEL = 100
FUNCTIONS = [
 [0x1001f184,0x1001f078,0x1001f578,0x1001f7b8,0x1001fa14,None,0x100233b0,0x10023454,0x10023500],
 [0x100204b8,0x100202e8,0x100206b4,0x10020a68,0x100211e8,None,0x10023850,0x100238f4,0x100239a0,0x10020d10,0x10020f3c,0x100210e8],
 [0x10021e78,0x10021d70,0x10022448,0x100224d8,0x10022818,None,0x10023f7c,0x10024020,0x100240cc]]
OWN = {v for fs in FUNCTIONS for v in fs if v} | {0x1002419c}
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


RESULT_WORDS.update({a:0 for a in (0x10072d94,0x10072d9c,0x10072da0,0x10072da4,0x10072e68)})
FIELDS = [[('mLine', 0, 'p'), ('mLineSize', 4, 'w'), ('mDestBytes', 8, 'w'), ('mSourceBytes', 12, 'w'), ('mBottomUp', 16, 'w'), ('mDest', 20, 'p'), ('mDestStep', 24, 'w'), ('mRead', 32, 'p'), ('mPlanes', 36, 'w'), ('mPlaneBytes', 40, 'w'), ('mLinesRead', 44, 'w'), ('mLineBytes', 48, 'w'), ('mHeight', 52, 'w')], [('mLine', 0, 'p'), ('mLineSize', 4, 'w'), ('mDestBytes', 8, 'w'), ('mSourceBytes', 12, 'w'), ('mBottomUp', 16, 'w'), ('mDest', 20, 'p'), ('mDestStep', 24, 'w'), ('mInterlaced', 32, 'w'), ('mNewCodes', 36, 'w'), ('mSize', 40, 'w'), ('mFc', 44, 'w'), ('mOc', 48, 'w'), ('mPalette', 52, 'p'), ('mPass', 56, 'w'), ('mRow', 60, 'w'), ('mFirstDest', 64, 'p'), ('mAvailBytes', 68, 'w'), ('mBitsLeft', 72, 'w'), ('mCurrSize', 76, 'w'), ('mClear', 80, 'w'), ('mTopSlot', 84, 'w'), ('mEnding', 88, 'w'), ('mSlot', 92, 'w'), ('mBytes', 96, 'p'), ('mStack', 360, 'p'), ('mSp', 364, 'p'), ('mSuffix', 368, 'p'), ('mPrefix', 372, 'p')], [('mLine', 0, 'p'), ('mLineSize', 4, 'w'), ('mDestBytes', 8, 'w'), ('mSourceBytes', 12, 'w'), ('mBottomUp', 16, 'w'), ('mDest', 20, 'p'), ('mDestStep', 24, 'w'), ('mLinesLeft', 32, 'w'), ('mLineBytes', 36, 'w'), ('mHeight', 40, 'w'), ('mWidth', 44, 'w'), ('mWordWidth', 48, 'w')]]
HSIZE = [40,13,8]
HOFF = [0x38,0x178,0x34]
VTABLE = [0x10072f38,0x10072ee0,0x10072e88]
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
        self.arrays = set()
        self.deletes = 0
        fill = case.get("fill", b"")[:BLOCK]
        self.blocks = [e.alloc(EXTENT, b"\xa5" * EXTENT), e.alloc(EXTENT, fill + b"\xa5" * (EXTENT - len(fill)))]
        self.area = e.alloc(0x28, b"\xa5" * 0x28)
        self.areas = [self.area] * 3
        self.loader = e.alloc(0x600, b"\xa5" * 0x600)
        self.gll = e.alloc(0x300, b"\xa5" * 0x300)
        self.file0 = e.alloc(16)
        w32(e, self.file0, 0x10073038)
        self.registry = [(self.blocks[0], EXTENT, 8), (self.blocks[1], EXTENT, 9),
                         (self.area, 0x28, 2), (self.loader, 0x600, 1), (self.gll, 0x300, 6)]
        self.L = e.cl = CL.PpcLog({self.area: 2, self.loader + 4: 5, self.loader + 0x2c: 11,
                                 self.gll: 6, self.file0: 7, self.blocks[0]: 8, self.blocks[1]: 9}, case.get("queues"))
        for a, words in FORMATS.items():
            e.write(a, struct.pack(">6I", *words))
        for a, word in RESULT_WORDS.items():
            w32(e, a, word)
        e.write(0x10074794, b"\0" * 8)
        e.write(0x10074490, b"\0" * 768)
        e.write(0x10074790, b"\0")

    def describe(self, p):
        if p == 0:
            return [0, 0]
        if p == self.loader + 0x2c:
            return [11, 0]
        if self.kind == 1 and self.gll + 0x64 <= p <= self.gll + 0x165:
            return [17, p - self.gll - 0x64]
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

        def strncmp(e):
            left = e.read(e.r(3), e.r(5))
            right = e.read(e.r(4), e.r(5))
            result = 0
            for a, b in zip(left, right):
                if a != b:
                    result = a - b
                    break
                if a == 0:
                    break
            e.set_r(3, result & MASK)

        S.imp('strncmp', strncmp)

        def file_ctor(e):
            rec("CFile.ctor", (L.newlab(e.r(3)),))

        def file_dtor(e):
            rec("CFile.dtor", (L.lab(e.r(3)),))

        def file_read(e):
            this, dest, size = e.r(3), e.r(4), e.r(5)
            target = self.describe(dest)
            if dest == 0x10074794:
                target = [12, 0]
            elif dest == self.loader + 0x430:
                target = [13, 0]
            elif dest == 0x10074490:
                target = [16, 0]
            elif self.kind == 1 and dest in (self.gll + 0x28, self.gll + 0x44):
                target = [-3, 0] # host uses a local big-endian image of the word
            elif self.kind == 1 and dest == self.gll + 0x64:
                target = [17, 0]
            elif target[0] == -1:
                target = [-3, 0]
            rec("CFile.Read", (L.lab(this), size, *target))
            count = min(size, max(0, len(self.stream) - self.pos))
            if dest and count > 0:
                e.write(dest, self.stream[self.pos:self.pos + count])
            if self.kind == 2 and self.active in (1, 2, 3, 7) and (target[0] in (8, 9) or target[0] >= 100):
                mode = e.u32(self.loader + 0x434) & 7 if self.active in (1, 7) else e.u32(self.gll + 0x38) & 7
                if mode == 2 and count == size:
                    data = e.read(dest, count)
                    e.write(dest, swap_pairs(data))
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

        def array_new(e):
            size = e.r(3)
            block = e.alloc(size + MARGIN + 1, b"\xa5" * (size + MARGIN + 1))
            label = L.newlab(block)
            self.heap[label] = (block, size)
            self.arrays.add(label)
            self.registry.append((block, size + MARGIN, label))
            rec("new[]", (size, label))
            e.set_r(3, block)

        def array_delete(e):
            block = e.r(3)
            rec("delete", (L.lab(block),))
            self.freed.add(L.lab(block))
            L.forget(block)

        S.imp("__nwa__FUl", array_new)
        S.imp("__dla__FPv", array_delete)
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


def swap_pairs(data):
    b = bytearray(data)
    for i in range(0, len(b) - 1, 2):
        b[i], b[i + 1] = b[i + 1], b[i]
    return bytes(b)


class World:
    # Shared setup and observation: native pointers only differ in their representation.
    def ptr(self, selector, offset=0):
        if selector == 0:
            return 0
        if selector in (1, 2):
            return self.blocks[selector - 1] + offset
        if selector == 3:
            return self.addr(109) + offset
        return self.addr(102) + offset

    def word(self, a):
        return int.from_bytes(self.read(a, 4), self.endian)

    def put(self, a, value, pointer=False):
        self.write(a, (value & ((1 << (8 * (self.psize if pointer else 4))) - 1)).to_bytes(self.psize if pointer else 4, self.endian))

    def pointer(self, a):
        return int.from_bytes(self.read(a, self.psize), self.endian)

    def init(self, case):
        for operation in case["ops"]:
            code, *args = operation
            if code == 40:
                which, field, value, *offset = args
                v = self.ptr(value, offset[0] if offset else 0) if field == 6 else value
                self.put(self.area_field(which, field), v, field == 6)
            elif code == 41:
                field, value, *offset = args
                v = self.ptr(value, offset[0] if offset else 0) if FIELDS[self.kind][field][2] == 'p' else value
                self.put(self.addr(field), v, FIELDS[self.kind][field][2] == 'p')
            elif code == 45:
                which, offset, byte = args
                self.write(self.addr(104 if which else 105) + offset, bytes([byte]))
            elif code == 46:
                selector, offset, data = args
                self.write(self.ptr(selector, offset), data)
            elif code == 47:
                offset, word = args
                self.write(self.blocks[1] + 0x2000 + 2 * offset, word.to_bytes(2, self.endian))
            elif code == 48:
                self.write(self.addr(108), bytes([args[0]]))
            else:
                self.active = code
                answer = self.callop(code)
                self.steps.append((answer, self.dump(), self.memory()))

    def dump(self):
        out = []
        for which in (0, 1):
            for f in range(10):
                a = self.area_field(which, f)
                out += self.describe(self.pointer(a)) if f == 6 else [self.word(a)]
        out += [self.word(self.addr(103)), self.read(self.addr(102), 1024), self.read(self.addr(104), HSIZE[self.kind])]
        for i, (_, _, t) in enumerate(FIELDS[self.kind]):
            out += self.describe(self.pointer(self.addr(i))) if t == 'p' else [self.word(self.addr(i))]
        out += [self.read(self.addr(105), HSIZE[self.kind]), self.read(self.addr(106), 8),
                self.read(self.addr(107), 768), self.read(self.addr(108), 1), self.position()]
        if self.kind == 1:
            out += [self.read(self.addr(109), 257)]
        return out


class Ppc(World, Bf):
    endian, psize = 'big', 4

    def __init__(self, e, case):
        Bf.__init__(self, e, case)
        self.steps = []
        self.read, self.write = e.read, e.write
        T.call_ppc(e, 0x10022870, [self.loader])
        w32(e, self.gll + 0x1c, VTABLE[self.kind])

    def addr(self, field):
        if field < 100:
            return self.gll + FIELDS[self.kind][field][1]
        return {100:self.area, 101:self.loader+4,102:self.loader+0x2c,103:self.loader+0x42c,
                104:self.loader+0x430,105:self.gll+HOFF[self.kind],106:0x10074794,107:0x10074490,
                108:0x10074790,109:self.gll+0x64,110:self.blocks[0],111:self.blocks[1]}[field]

    def area_field(self, which, field):
        return (self.loader + 4 if which else self.area) + field * 4

    def position(self):
        return self.pos

    def callop(self, code):
        address = FUNCTIONS[self.kind][code]
        if code in (4, 9, 11):
            result = T.call_ppc(self.e, address, [self.gll, self.file0])[0]
            return 0 if code == 11 else s32(result) if code == 9 else result
        if code == 8:
            return T.call_ppc(self.e, address, [self.loader, self.file0])[0] & 255
        out = self.e.alloc(4)
        args = {0:[self.gll,self.file0,self.loader],1:[self.gll,self.area,self.file0,self.loader],
                2:[self.gll,self.file0],3:[self.gll,self.file0],6:[self.loader,self.file0,self.area],
                7:[self.loader,self.area,self.file0,self.area],10:[self.gll,self.file0,self.loader+0x2c]}[code]
        T.call_ppc(self.e,address,[out]+args)
        return s32(self.e.u32(out))

    def memory(self):
        out = [(8+i,EXTENT,0,self.e.read(a,EXTENT)) for i,a in enumerate(self.blocks)]
        for label,(a,size) in self.heap.items():
            extent = size if label in self.arrays else size + MARGIN
            out.append((label,extent,int(label in self.freed),self.e.read(a,extent)))
        if self.kind == 1:
            a = self.e.u32(self.gll+0x174)
            changed = []
            for label,size,freed,data in out:
                start = self.blocks[label-8] if label in (8,9) else self.heap[label][0]
                if start <= a and a + 0x2000 <= start + size:
                    off = a-start
                    data = data[:off]+swap_pairs(data[off:off+0x2000])+data[off+0x2000:]
                changed.append((label,size,freed,data))
            out = changed
        return out


def bind(lib):
    if getattr(lib,'igt_bound',False):
        return
    lib.tpw_igt_begin.argtypes = [C.c_int,C.c_char_p,C.c_int,C.c_char_p,C.c_int]
    lib.tpw_igt_addr.argtypes = [C.c_int]
    lib.tpw_igt_addr.restype = C.c_void_p
    lib.tpw_igt_area_field.argtypes = [C.c_int,C.c_int]
    lib.tpw_igt_area_field.restype = C.c_void_p
    lib.tpw_igt_op.argtypes = [C.c_int]
    lib.tpw_igt_op.restype = C.c_int64
    lib.tpw_igt_memory.argtypes = [C.c_int,C.c_void_p,C.POINTER(C.c_int64)]
    lib.igt_bound=True


class Host(World):
    endian,psize='little',8

    def __init__(self,lib,case):
        bind(lib)
        self.lib,self.kind,self.steps=lib,case['kind'],[]
        stream,fill=case['stream'],case.get('fill',b'')
        lib.tpw_igt_begin(self.kind,stream,len(stream),fill,len(fill))
        self.blocks=[self.addr(110),self.addr(111)]
        self.read=C.string_at
        self.write=lambda a,b:C.memmove(a,b,len(b))
        self.registry=[(self.blocks[0],EXTENT,8),(self.blocks[1],EXTENT,9),
                       (self.addr(112),0x600,1),(self.addr(113),0x300,6),(self.addr(100),48,2)]

    def addr(self,field):
        return self.lib.tpw_igt_addr(field)

    def area_field(self,which,field):
        return self.lib.tpw_igt_area_field(which,field)

    def describe(self,p):
        if p==0:
            return [0,0]
        if p==self.addr(102):
            return [11,0]
        if p==0xa5a5a5a5a5a5a5a5:
            return [-2,0]
        if self.kind==1 and self.addr(109)<=p<=self.addr(109)+257:
            return [17,p-self.addr(109)]
        for a,n,label in self.registry:
            if a<=p<=a+n:
                return [label,p-a]
        # Heap pointers are observed even after release.
        for label,n,_,_ in self.memory():
            if label>=100:
                # exported registry start addresses
                a=self.lib.tpw_igt_heap_addr(label)
                if a<=p<=a+n:
                    return [label,p-a]
        return [-1,p&MASK]

    def position(self):
        return C.c_int64.from_address(self.addr(114)).value

    def callop(self,code):
        return self.lib.tpw_igt_op(code)

    def memory(self):
        out,meta,result=C.create_string_buffer(0x11000),(C.c_int64*3)(),[]
        while self.lib.tpw_igt_memory(len(result),out,meta):
            result.append((*meta,out.raw[:meta[1]]))
        return result


def ppc(e,case):
    world=Ppc(e,case)
    saved=world.stubs().activate(e)
    try:
        world.init(case)
        return dict(steps=world.steps,log=world.L.log)
    finally:
        CL.Stubs.deactivate(e,saved)


def host(lib,case):
    world=Host(lib,case)
    lib.tpw_igt_heap_addr.argtypes=[C.c_int]
    lib.tpw_igt_heap_addr.restype=C.c_void_p
    CL.host_begin(lib,{},case.get('queues'))
    lib.tpw_igt_labels()
    world.init(case)
    return dict(steps=world.steps,log=CL.host_end(lib))

# Bytes are deliberately in the order consumed by the Mac, including its PC-format quirks.
def rb(r,n):
    return bytes(r.randrange(256) for _ in range(n))


def case(kind,ops,stream=b'',queues=None,fill=b''):
    return dict(kind=kind,ops=ops,stream=bytes(stream),queues=queues or {},fill=fill)


def header_ops(h,loader=False):
    return [(45,int(loader),i,b) for i,b in enumerate(h)]


def setf(kind,name,value,offset=0):
    i=next(i for i,f in enumerate(FIELDS[kind]) if f[0]==name)
    return (41,i,value,offset)


def area_ops(fmt,width,height,stride=None):
    values=list(fmt)+[1,width,height,stride if stride is not None else width*((fmt[4]+7)//8)]
    return [(40,0,i,v,0x800) for i,v in enumerate(values)]


def iff_header(w,h,planes=8,ilbm=False,compression=0):
    b=bytearray(40)
    b[:4]=b'MROF'
    b[8:12]=b'MBLI' if ilbm else b' MBP'
    b[12:16]=b'DHMB'
    struct.pack_into('<IHH',b,16,20,w,h)
    b[28],b[30]=planes,compression
    return bytes(b)


def iff_chunk(name,data,size=None):
    return name+struct.pack('<I',len(data) if size is None else size)+data+(b'\0' if len(data)&1 else b'')


def gif_header(w,h,flags=7):
    return b'GIF89a'+struct.pack('>HHBBB',w,h,flags,0,0)


def tim(w,h,mode,palette=None,bad=0):
    flags=mode if palette is None else mode|8
    b=struct.pack('>II',16,flags)
    if palette is not None:
        n=len(palette)//2
        b+=struct.pack('>IHHHH',12+len(palette)+bad,0,0,n,1)+palette
    return b+struct.pack('>IHHHH',12+w*h*2,0,0,w,h)


def line_setup(kind,h):
    return header_ops(h)+[setf(kind,'mLine',1,256),setf(kind,'mLineSize',512)]


def lzw_codes(codes,size=2):
    # Pack codes using the decoder's dictionary-growth timing.
    curr,slot,top=size+1,(1<<size)+2,1<<(size+1)
    bits=0
    nbits=0
    out=bytearray()
    first=False
    for code in codes:
        bits|=code<<nbits
        nbits+=curr
        while nbits>=8:
            out.append(bits&255)
            bits>>=8
            nbits-=8
        if code==(1<<size):
            curr,slot,top=size+1,(1<<size)+2,1<<(size+1)
            first=True
        elif first:
            first=False
        elif code!=(1<<size)+1:
            if slot<top:
                slot+=1
            if slot>=top and curr<12:
                curr+=1
                top*=2
    if nbits:
        out.append(bits&255)
    return bytes(out)


def gif_state(data,size=2,count=10,bits=0,avail=None,slot=None,curr=None):
    k=1
    ops=line_setup(k,gif_header(count,10))
    values={'mSourceBytes':count,'mSize':size,'mNewCodes':(1<<size)+2,'mFc':0,'mOc':0,
            'mCurrSize':size+1 if curr is None else curr,'mClear':1<<size,'mTopSlot':1<<(size+1 if curr is None else curr),
            'mEnding':(1<<size)+1,'mSlot':(1<<size)+2 if slot is None else slot,
            'mBitsLeft':bits,'mAvailBytes':len(data) if avail is None else avail}
    ops += [setf(k,n,v) for n,v in values.items()]
    ops += [setf(k,'mStack',2),setf(k,'mSp',2),setf(k,'mSuffix',2,0x1000),setf(k,'mPrefix',2,0x2000),
            setf(k,'mBytes',1,0x1800),(46,1,0x1800,data)]
    return ops


def pipeline(kind,op,r,variant=0):
    w,h=r.randint(1,16),r.randint(1,5)
    queues={}
    if kind==0:
        ilbm=bool(variant&1)
        n=((w//8+1)&~1)*8 if ilbm else (w+1)&~1
        compressed=bool(variant&2)
        pixels=b''.join((bytes([max(1,n)-1])+rb(r,max(1,n))) if compressed else rb(r,n) for _ in range(h))
        stream=iff_header(w,h,ilbm=ilbm,compression=int(compressed))
        if variant&4:
            stream+=iff_chunk(b'PAMC',rb(r,768))
        stream+=iff_chunk(b'YDOB',pixels)
        fmt=FORMATS[0x10073ed0]
    elif kind==1:
        flags=7|(0x80 if variant&1 else 0)
        stream=gif_header(w,h,flags)+(rb(r,768) if flags&128 else b'')
        stream+=b','+bytes(8)+bytes([0x40 if variant&2 else 0])+bytes([2])
        fmt=FORMATS[0x10073ed0]
    else:
        mode=variant%4
        palette=rb(r,2*r.choice([0,1,8,9,255,256])) if variant&4 and mode<2 else None
        stream=tim(w,h,mode,palette)+rb(r,w*h*2)
        fmt=(0x1f,0x3e0,0x7c00,0,16,0) if mode==2 else FORMATS[0x10073ea0 if mode==3 else 0x10073ed0]
        w=(w*4 if mode==0 else w*2 if mode==1 else w if mode==2 else w*2//3)
    if variant%13==12:
        stream=stream[:r.randrange(len(stream)+1)]
    if variant%17==16:
        queues={'LbMem_Alloc':[0]}
    return case(kind,area_ops(fmt,max(1,w),h)+[(op,)],stream,queues)


def random_case(kind,op,r):
    v=r.randrange(64)
    if op in (0,1,6,7,8):
        c=pipeline(kind,op,r,v)
        if op in (0,6,8) and r.randrange(4)==0:
            b=bytearray(c['stream'])
            if b:
                b[r.randrange(min(len(b),40 if kind==0 else 13 if kind==1 else 8))]=r.randrange(256)
            c['stream']=bytes(b)
        return c
    if op==4:
        h=iff_header(r.randrange(65536),1,ilbm=bool(v&1)) if kind==0 else gif_header(r.randrange(65536),1) if kind==1 else struct.pack('>II',16,r.randrange(32))
        ops=header_ops(h)
        if kind==2:
            ops+=[setf(kind,'mWordWidth',T.rint(r))]
        return case(kind,ops+[(op,)])
    if kind==0:
        planes=r.randint(1,8)
        ilbm=bool(v&1)
        compression=bool(v&2)
        w,h=r.randint(1,48),r.randint(1,5)
        n=((w//planes+1)&~1)*planes if ilbm else (w+1)&~1
        head=iff_header(w,h,planes,ilbm,int(compression))
        data=(bytes([max(1,n)-1])+rb(r,max(1,n))) if compression else rb(r,n)
        ops=line_setup(kind,head)
        if op==2:
            stream=head+iff_chunk(b'JUNK',b'abc')+iff_chunk(b'YDOB',data)
        else:
            ops += [setf(kind,'mRead',1,1024),setf(kind,'mPlanes',planes),setf(kind,'mPlaneBytes',n//planes),
                    setf(kind,'mLinesRead',r.choice([0,1,5,MASK])),setf(kind,'mHeight',h),setf(kind,'mLineBytes',n)]
            stream=data
        if v%7==0:
            stream=stream[:-1]
        return case(kind,ops+[(op,)],stream)
    if kind==2:
        mode=v%4
        w,h=r.randint(0,32),r.randint(0,5)
        head=struct.pack('>II',16,mode)
        ops=line_setup(kind,head)+[setf(kind,'mHeight',h),setf(kind,'mWordWidth',w)]
        if op==3:
            ops += [setf(kind,'mLinesLeft',h),setf(kind,'mLineBytes',2*w)]
        stream=rb(r,2*w)
        if v%7==0:
            stream=stream[:-1]
        return case(kind,ops+[(op,)],stream)
    if op==2:
        desc=b','+bytes(8)+bytes([r.choice([0,64,128,135,255])])+bytes([r.randrange(256)])
        prefix=r.choice([b'',b'!\xf9\x03abc\0',b'!\0\0!\0\0'])
        stream=prefix+desc
        if v%5==0:
            stream=stream[:r.randrange(len(stream)+1)]
        return case(1,line_setup(1,gif_header(10,10))+[(2,)],stream)
    if op==10:
        return case(1,[(10,)],rb(r,r.choice([0,1,767,768,769])))
    if op==11:
        width,height=r.randint(0,32),r.randint(1,40)
        ops=line_setup(1,gif_header(width,height))+[setf(1,'mDest',1,0x800),setf(1,'mDestBytes',width),
             setf(1,'mDestStep',r.choice([32,-32])),setf(1,'mFirstDest',0),setf(1,'mInterlaced',v&1),
             setf(1,'mPass',r.randrange(4)),setf(1,'mRow',r.randrange(height))]
        return case(1,ops+[(11,)]*r.randint(1,2))
    if op==9:
        curr=r.randint(1,12)
        data=rb(r,10)
        ops=gif_state(data,curr=curr,bits=r.randrange(9),avail=r.choice([0,1,2,10,0xffffffff]))+[(48,r.randrange(256))]
        stream=rb(r,r.choice([0,1,2,20,257]))
        return case(1,ops+[(9,)],stream)
    count=r.randint(1,100)
    codes=[4]+[r.randrange(4) for _ in range(count)]+[5]
    data=lzw_codes(codes)
    ops=gif_state(data,count=r.randint(1,count+3))
    return case(1,ops+[(3,)])


def edges(kind,op):
    r=random.Random(223+kind*20+op)
    if op in (0,1,6,7,8):
        for v in range(16):
            yield pipeline(kind,op,r,v)
        if op==8:
            for size in [-1,0,2,3,4,39,40]:
                c=pipeline(kind,op,r,0)
                c['queues']={'CFile.GetSize':[size]}
                yield c
        if kind==2:
            for n in (0,1,8,9,255,256,257):
                for bad in (0,1):
                    data=tim(2,1,0,rb(r,n*2),bad)+b'1234'
                    yield case(kind,area_ops(FORMATS[0x10073ed0],8,1)+[(op,)],data)
            data=tim(2,1,1,rb(r,512))+b'1234'
            for end in (0,7,8,9,19,20,21,531,532,533,543,544):
                yield case(kind,area_ops(FORMATS[0x10073ed0],4,1)+[(op,)],data[:end])
        if kind==0:
            head=iff_header(8,1)
            for size in (0,1,767,768,769):
                yield case(kind,area_ops(FORMATS[0x10073ed0],8,1)+[(op,)],head+iff_chunk(b'PAMC',bytes(min(size,768)),size))
        if kind==1 and op in (1,7):
            for queue in ([0],[1,0],[1,1,0],[1,1,1,0]):
                c=pipeline(kind,op,r,0)
                c['queues']={'LbMem_Alloc':queue}
                yield c
        if op in (0,6,8):
            data = pipeline(kind,op,r,1)['stream']
            for end in range(HSIZE[kind]+1):
                yield case(kind,[(op,)],data[:end])
        if kind==0 and op in (0,6):
            for planes in (0,1,7,8,9,255):
                yield case(0,[(op,)],iff_header(8,2,planes=planes))
            yield case(0,[(op,)],b'FORM'+bytes(4)+b'ILBMBMHD'+bytes(24))
        if kind==1 and op in (0,6):
            for flags in range(16):
                yield case(1,[(op,)],gif_header(7,3,flags))
        if kind==2 and op in (0,6):
            for flags in (4,7,10,11,16,17,18,19,0xffffffff):
                yield case(2,[(op,)],struct.pack('>II',16,flags)+struct.pack('>IHHHH',16,0,0,2,1))
    else:
        for _ in range(12):
            yield random_case(kind,op,r)
        if kind==0 and op==3:
            head=iff_header(16,1,compression=1)
            ops=line_setup(0,head)+[setf(0,'mRead',1,1024),setf(0,'mPlanes',1),setf(0,'mPlaneBytes',0),
                 setf(0,'mLinesRead',0),setf(0,'mHeight',1),setf(0,'mLineBytes',16)]
            yield case(0,ops+[(3,)],b'\x80\x81\x33\xf1\x77')
        if kind==1 and op==3:
            for codes in ([4,4,0,1,6,5],[4,0,6,7,5],[5],[4,5],[4,0,1,2,3]*8+[5]):
                yield case(1,gif_state(lzw_codes(codes),count=16)+[(3,)])
            for curr in range(3,13):
                codes=[0]*12
                # No clear: directly exercise each code width and full dictionary boundaries.
                data=bytes(24)
                ops=gif_state(data,curr=curr,count=8,slot=(1<<curr)-1)+[(3,)]
                yield case(1,ops)
            ops=gif_state(b'',count=3)+[setf(1,'mSp',2,5),(46,2,0,b'abcde'),(3,),(3,)]
            yield case(1,ops)
            codes=[4]+[i&3 for i in range(4100)]+[5]
            yield case(1,gif_state(lzw_codes(codes),count=4100)+[(3,)])
        if kind==1 and op==9:
            for byte in (0,1,2,127,128,255):
                yield case(1,gif_state(b'',avail=0)+[(9,)],bytes([byte])+b'abc')
            for bits in (0,8,16,31,32,33,63,64,65,0xffffffff):
                yield case(1,gif_state(b'abc',bits=bits,curr=3)+[(48,255),(9,)])
        if kind==1 and op==11:
            for height in range(1,41):
                ops=line_setup(1,gif_header(8,height))+[setf(1,'mDest',1,0x800),setf(1,'mDestBytes',8),
                    setf(1,'mDestStep',16),setf(1,'mFirstDest',0),setf(1,'mInterlaced',1),setf(1,'mPass',0),setf(1,'mRow',0)]
                yield case(1,ops+[(11,)]*height)


def function_test(kind,op):
    cl=['_AndyIFFGLL','_AndyGIFGLL','_TimGLL'][kind]
    ld=['TbIFFLoader','TbGIFLoader','TbTIMLoader'][kind]
    names={0:cl+'::ReadHeaderInfo',1:cl+'::ReadData',2:cl+'::ReadFirstLine',3:cl+'::ReadNextLine',
           4:cl+'::GetSourceLineLength',6:ld+'::ReadHeaderInfo',7:ld+'::ReadData',8:ld+'::ValidImage',
           9:cl+'::_GifGetNextCode',10:cl+'::_GifReadPalette',11:cl+'::WriteLine'}
    return FnTest(names[op],ppc,host,edge=lambda:edges(kind,op),rnd=lambda r:random_case(kind,op,r))


def sinit_ppc(e,c):
    e.write(0x100747ac,b'\xa5'*48)
    T.call_ppc(e,0x1002419c,[])
    return {'words':list(struct.unpack('>12I',e.read(0x100747ac,48)))}


def sinit_host(lib,c):
    bind(lib)
    return {'words':list(struct.unpack('<6I',C.string_at(lib.tpw_igt_addr(116),24)))+
                    list(struct.unpack('<6I',C.string_at(lib.tpw_igt_addr(115),24)))}


TESTS=[function_test(k,op) for k,fs in enumerate(FUNCTIONS) for op,a in enumerate(fs) if a]
TESTS += [FnTest('__sinit_Lbgfsave_cpp',sinit_ppc,sinit_host,edge=lambda:[{}],rnd=lambda r:{})]
