"""Unit w219: all 21 functions of Bullfrog TbRaConvert.cpp.

One shared world writes Mac fields and native fields through explicit layout maps. Other
converter methods, allocation, deletion, memcpy and global registration are fakes. All
arguments of converter fakes are recorded. Pixel input has eight guard bytes. Native
16-bit destination words are byte-swapped on the original before comparison. In-place
16-bit output is excluded because its host order can change subsequent source reads.
No floats occur. Invalid NULL calls through a converter, negative pixel counts, and
inverse tables above six bits are excluded (unconditional reads, unbounded loops, and
emulator memory/runtime respectively). Sinit registration is asserted only on the Mac;
the host uses namespace initialization and reconstructs the global for the test.
"""
import ctypes as C
import random
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_RaConvert"
MASK = 0xffffffff
FUNCS = [
    (0x10016b10, "CConverter::CConverter", 20), (0x10016b4c, "CConverter::Init", 580),
    (0x10016e5c, "CConverter::ConvBuf", 88), (0x10016ee4, "CConverter::ConvertArea", 212),
    (0x10016ff4, "CConverter::Release", 92), (0x10016de4, "CConverterBase::~CConverterBase", 76),
    (0x10017b78, "CMaskConverter::Init", 308), (0x10018e34, "CMaskConverter::GetShiftBits", 192),
    (0x10017cf4, "CMaskConverter::ConvertLineUp", 2164), (0x100185a4, "CMaskConverter::ConvertLineDown", 2132),
    (0x10018f30, "CMaskConverter::~CMaskConverter", 88), (0x1001707c, "CInverseMap::CInverseMap", 12),
    (0x100170b0, "CInverseMap::~CInverseMap", 76), (0x10017124, "CInverseMap::InvCMap", 676),
    (0x10017408, "CInverseMap::redloop", 420), (0x100175d8, "CInverseMap::greenloop", 724),
    (0x100178dc, "CInverseMap::blueloop", 476), (0x10017ae4, "CInverseMap::maxfill", 100),
    (0x1001901c, "TbRAConv::Move", 652), (0x10018fb4, "TbRAConv::~TbRAConv", 68),
    (0x100192f4, "__sinit_TbRaConvert_cpp", 56)]
FORMATS = [(0, 0, 0, 0, 8, 1), (0x7c00, 0x3e0, 31, 0, 16, 0),
           (0xff0000, 0xff00, 255, 0, 24, 0), (0xff0000, 0xff00, 255, 0xff000000, 32, 0)]
VT = {1: 0x10072700, 2: 0x100737fc, 3: 0x100726bc, 5: 0x100726d8, 4: 0x100726bc}
STAT = [0x10073f18 - 4 * i for i in range(13)]
S = CL.Stubs()
S.real.update(a for a, _, _ in FUNCS)


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib, 'rcv_bound', False):
        return
    for name, restype, args in [
        ('call', C.c_uint64, [C.c_int32, C.c_void_p] + [C.c_uint64] * 6),
        ('alloc', C.c_uint64, [C.c_uint32]), ('global', C.c_uint64, []),
        ('stat', C.c_uint64, [C.c_int32]), ('vptr', C.c_uint64, [C.c_int32]),
        ('layout', C.c_int32, [C.c_int32, C.POINTER(C.c_uint32)])]:
        f = getattr(lib, 'tpw_rcv_' + name)
        f.restype = restype
        f.argtypes = args
    lib.rcv_bound = True


class World:
    def __init__(self, e, lib, c):
        self.e, self.lib, self.c = e, lib, c
        self.native = lib is not None
        self.keep = []
        self.fixed = {}
        self.heap = []
        self.deleted = []
        if self.native:
            bind(lib)
        self.ptrsize = C.sizeof(C.c_void_p) if self.native else 4
        self.endian = '<' if self.native else '>'
        self.layouts = []
        for kind in range(3):
            if self.native:
                arr = (C.c_uint32 * 32)()
                n = lib.tpw_rcv_layout(kind, arr)
                self.layouts.append(list(arr)[:n])
            else:
                self.layouts.append([16, 0, 4, 8, 12] if kind == 0 else [84] + list(range(4, 84, 4)) if kind == 1 else [88] + list(range(4, 88, 4)))
        if self.native:
            assert self.layouts[0] == [24, 0, 8, 12, 16]
            assert self.layouts[1] == [88] + list(range(8, 88, 4))
            assert self.layouts[2] == [120] + list(range(8, 48, 4)) + list(range(48, 96, 8)) + list(range(96, 116, 4))
        self.build()
        if self.native:
            CL.host_begin(lib, {v: k for k, v in self.fixed.items()})
        else:
            self.L = e.cl = CL.PpcLog(self.fixed)
            self.saved = S.activate(e)
            self.install()

    def alloc(self, n, label=None, data=None):
        if self.native:
            p = self.lib.tpw_rcv_alloc(n)
        else:
            p = self.e.alloc(n)
        self.write(p, data if data is not None else b'\xa5' * n)
        if label is not None:
            self.fixed[p] = label
        return p

    def write(self, p, data):
        if self.native:
            C.memmove(p, data, len(data))
        else:
            self.e.write(p, data)

    def read(self, p, n):
        return C.string_at(p, n) if self.native else self.e.read(p, n)

    def words(self, p, values):
        self.write(p, struct.pack(self.endian + str(len(values)) + 'I', *(v & MASK for v in values)))

    def word(self, p):
        return struct.unpack(self.endian + 'I', self.read(p, 4))[0]

    def ptr(self, p, value=None):
        fmt = self.endian + ('Q' if self.ptrsize == 8 else 'I')
        if value is None:
            return struct.unpack(fmt, self.read(p, self.ptrsize))[0]
        self.write(p, struct.pack(fmt, value))

    def vptr(self, kind):
        if self.native:
            return self.lib.tpw_rcv_vptr(kind)
        return self.fakevt if kind in (3, 4) else VT[kind]

    def objwrite(self, p, kind, values):
        offsets = self.layouts[kind][1:]
        for i, value in enumerate(values):
            if kind == 0 and i == 0 or kind == 2 and 10 <= i <= 15:
                self.ptr(p + offsets[i], value)
            else:
                self.words(p + offsets[i], [value])

    def objread(self, p, kind):
        out = []
        for i, off in enumerate(self.layouts[kind][1:]):
            if kind == 0 and i == 0 or kind == 2 and 10 <= i <= 15:
                value = self.ptr(p + off)
                if kind == 0:
                    value = self.label(value)
                elif i < 13:
                    value = (value - self.dist) // 4
                else:
                    value -= self.rgb
            else:
                value = self.word(p + off)
            out.append(value)
        return out

    def label(self, p):
        if not p:
            return 0
        if p in self.fixed:
            return self.fixed[p]
        return -1

    def build(self):
        c = self.c
        if not self.native:
            self.fakevt = self.alloc(20)
            self.tvectors = self.alloc(24)
            self.stubs = [0x1006c180 + 4 * i for i in range(3)]
            for i, address in enumerate(self.stubs):
                self.words(self.tvectors + i * 8, [address, 0x1006eda0])
            self.words(self.fakevt, [0, 0] + [self.tvectors + i * 8 for i in range(3)])
        self.obj = self.alloc(256, 1)
        self.conv = self.alloc(1200, 2)
        self.dest = self.alloc(2048, 3, c['pixels'])
        self.source = self.dest if c['inplace'] else self.alloc(2048, 4, c['pixels'])
        self.fdest = self.alloc(24, 5)
        self.fsrc = self.alloc(24, 6)
        self.words(self.fdest, c['dstfmt'])
        self.words(self.fsrc, c['srcfmt'])
        self.palette = self.alloc(1024, 7, c['palette'])
        self.outshift = self.alloc(8, 8)
        self.words(self.outshift, [c['shiftseed'], c['widthseed']])
        self.ptr(self.conv, self.vptr(c['kind']))
        self.objwrite(self.conv, 1, c['fields'])
        self.objwrite(self.obj, 0, [self.conv if c['existing'] else 0, c['sb'], c['db'], c['kind']])
        self.globalobj = self.lib.tpw_rcv_global() if self.native else 0x10073f1c
        self.fixed[self.globalobj] = 9
        self.objwrite(self.globalobj, 0, [0, c['sb'], c['db'], 4])
        # Native TbRenderArea inserts four bytes after its pointer.
        self.areas = [self.alloc(64, 10 + i) for i in range(2)]
        self.areaoff = 24 + self.ptrsize
        for i, p in enumerate(self.areas):
            self.words(p, c['dstfmt'] if i == 0 else c['srcfmt'])
            self.ptr(p + 24, 0 if c['nullarea'] == i + 1 else self.dest if i == 0 else self.source)
            self.words(p + self.areaoff, [c['widths'][i], c['heights'][i], c['pitches'][i]])
        n = max(8192, (1 << c['bits']) ** 3 + 4096)
        self.extent = n
        self.dist = self.alloc(n * 4, 12)
        self.rgb = self.alloc(n, 13)
        self.words(self.dist, [c['distance']] * n)
        self.write(self.rgb, bytes([c['rgbseed']]) * n)
        side = 1 << c['bits']
        center = c['center'] % side
        base = 2048 + center + center * side + center * side * side
        values = [c['first'], center, center, center, c['gd'], c['rd'], c['cd'], c['inc'], c['inc'], c['inc'],
                  self.dist + base * 4, self.dist + base * 4, self.dist + base * 4,
                  self.rgb + base, self.rgb + base, self.rgb + base, side, side * side, 1 << (8 - c['bits']), c['square'], side]
        self.inverse = self.alloc(256, 14)
        self.ptr(self.inverse, self.vptr(5))
        self.objwrite(self.inverse, 2, values)
        stats = [c['inc'], center, 0, side - 1, c['inc'], c['inc'], c['rd'], self.dist + base * 4,
                 self.rgb + base, center, 0, side - 1, c['inc']]
        for i, value in enumerate(stats):
            p = self.lib.tpw_rcv_stat(i) if self.native else STAT[i]
            if i in (7, 8):
                self.ptr(p, value)
            else:
                self.words(p, [value])
        # Register every row address, including coinciding source/destination rows, deterministically.
        for i in range(6):
            self.fixed[self.dest + i * c['pitches'][0]] = 30 + i
            self.fixed[self.source + i * c['pitches'][1]] = 40 + i

    def install(self):
        # Allocation and deletion record the Mac allocation size, with filled, guarded memory.
        def new(e):
            n = e.r(3)
            p = e.alloc(n + 64, b'\xa5' * (n + 64))
            label = self.L.newlab(p)
            self.fixed[p] = label
            self.heap.append(p)
            self.L.rec('new', (label, n))
            e.set_r(3, p)
        def delete(e):
            p = e.r(3)
            self.L.rec('delete', (self.L.lab(p),))
            self.deleted.append(p)
            self.L.forget(p)
        def formatfake(name, reverse=False, ctor=False):
            def fake(e):
                p, f, pal = e.r(3), e.r(4), e.r(5)
                if reverse:
                    f, pal = pal, f
                if ctor:
                    self.ptr(p, self.fakevt)
                ws = struct.unpack('>6I', e.read(f, 24))
                self.L.rec(name, (self.L.lab(p), *ws[:4], T.s32(ws[4]), T.s32(ws[5]), self.L.lab(pal)))
            return fake
        def line(name):
            return CL.recorder(name, 'LLLi')
        def dtor(name):
            def fake(e):
                self.L.rec(name, (self.L.lab(e.r(3)),))
                if T.s16(e.r(4)) > 0:
                    delete(e)
            return fake
        self.local = CL.Stubs()
        self.local.imp('__nw__FUl', new)
        self.local.imp('__dl__FPv', delete)
        def copy(e):
            d, s, n = e.r(3), e.r(4), e.r(5)
            self.L.rec('memcpy', (self.L.lab(d), self.L.lab(s), n))
            e.write(d, e.read(s, n))
            e.set_r(3, d)
        self.local.imp('memcpy', copy)
        self.local.internal(0x10047740, formatfake('pal.init', True))
        self.local.internal(0x10048924, formatfake('mask.ctor', ctor=True))
        self.local.internal(0x10048a6c, formatfake('mask.init'))
        self.local.internal(0x1004889c, dtor('pal.dtor'))
        self.local.internal(0x1004786c, line('pal.up'))
        self.local.internal(0x10047fd0, line('pal.down'))
        for a, fake in zip(self.stubs, [dtor('mask.dtor' if self.c['kind'] == 3 else 'rec.dtor'), line('mask.up' if self.c['kind'] == 3 else 'up'), line('mask.down' if self.c['kind'] == 3 else 'down')]):
            self.local.internal(a, fake)
        def register(e):
            assert [e.r(i) for i in (3, 4, 5)] == [0x10073f1c, e.u32(0x1006eda0 + 0x370), 0x10073f2c]
        self.local.imp('__register_global_object', register)
        self.local_saved = self.local.activate(self.e)

    def call(self, op, obj, args):
        if self.native:
            return self.lib.tpw_rcv_call(op, obj, *(args + [0] * (6 - len(args)))) & MASK
        address = FUNCS[op][0]
        if op == 18:
            out = self.alloc(4)
            T.call_ppc(self.e, address, [out, obj] + args)
            return self.word(out)
        if op == 20:
            T.call_ppc(self.e, address, [])
            return 0
        result = T.call_ppc(self.e, address, [obj] + args)[0]
        return result & MASK if op in (14, 15, 16) else 0

    def run(self, op):
        c = self.c
        answer = 0
        obj = self.obj
        args = []
        if op == 1:
            args = [self.fdest, self.fsrc, self.palette if c['haspal'] else 0, c['unused']]
        elif op == 2:
            args = [self.dest, self.source, c['count']]
        elif op == 3:
            args = [0 if c['nulldest'] else self.dest, self.source, *c['pitches'], c['rows'], c['count']]
        elif op in (5, 10, 12, 19):
            obj = 0 if c['nullobj'] else self.conv if op in (5, 10, 19) else self.inverse
            if op == 10:
                self.ptr(self.conv, self.vptr(1))
            args = [c['flag'] & MASK]
        elif op in (6, 7, 8, 9):
            obj = self.conv
            if op == 6:
                args = [self.fsrc, self.fdest]
            elif op == 7:
                args = [c['masks'][0], c['masks'][1], self.outshift, self.outshift + 4]
            else:
                args = [self.dest, self.source, c['count']]
        elif 11 <= op <= 17:
            obj = self.inverse
            if op == 13:
                args = [c['first'], c['colors'], self.palette, c['bits'], self.dist + 8192, self.rgb + 2048]
            elif op in (15, 16):
                args = [c['restart']]
            elif op == 17:
                args = [self.dist + 8192, c['side'] & MASK]
        if op == 18:
            args = [*self.areas, self.palette if c['haspal'] else 0]
        if op == 0:
            self.objwrite(obj, 0, [self.conv, c['sb'], c['db'], c['kind']])
        if op == 11:
            self.ptr(obj, self.vptr(4))
        answer = self.call(op, obj, args)
        out = {'answer': answer if op in (14, 15, 16, 18) else 0}
        if op in (0, 1, 2, 3, 4, 20):
            target = self.globalobj if op == 20 else self.obj
            words = self.objread(target, 0)
            p = self.ptr(target)
            if op == 1 and p:
                # New heap pointer labels come from the recorder, not the initial registry.
                words[0] = self.L.lab(p) if not self.native else words[0]
                if words[3] == 1:
                    out['mask'] = self.objread(p, 1)
            out['converter'] = words
        if op in (6, 7, 8, 9):
            out['fields'] = self.objread(self.conv, 1)
            out['shift'] = [self.word(self.outshift), self.word(self.outshift + 4)]
        if op in (8, 9, 18):
            data = bytearray(self.read(self.dest, 2048))
            db = c['fields'][16] if op in (8, 9) else (c['dstfmt'][4] + 7) >> 3
            if not self.native and db == 2:
                rows = [(0, c['count'])] if op in (8, 9) else [(j * c['pitches'][0], min(c['widths'])) for j in range(min(c['heights']))] if c['nullarea'] == 0 and c['srcfmt'] != c['dstfmt'] else []
                for start, count in rows:
                    for i in range(count):
                        at = start + i * 2
                        data[at], data[at + 1] = data[at + 1], data[at]
            out['pixels'] = bytes(data)
            out['source'] = self.read(self.source, 2048) if not c['inplace'] else b''
        if op == 18:
            out['areas'] = [list(struct.unpack(self.endian + '6I', self.read(p, 24))) + [self.label(self.ptr(p + 24))] + [self.word(p + self.areaoff + i * 4) for i in range(3)] for p in self.areas]
            words = self.objread(self.globalobj, 0)
            if not self.native and self.ptr(self.globalobj):
                words[0] = self.L.lab(self.ptr(self.globalobj))
            out['global'] = words
        if op == 11:
            out['vptr'] = self.ptr(self.inverse) == self.vptr(5)
        if 11 <= op <= 17:
            out['inverse'] = self.objread(self.inverse, 2)
            out['dist'] = list(struct.unpack(self.endian + str(self.extent) + 'I', self.read(self.dist, self.extent * 4)))
            out['rgb'] = self.read(self.rgb, self.extent)
            stats = []
            for i in range(13):
                p = self.lib.tpw_rcv_stat(i) if self.native else STAT[i]
                stats.append((self.ptr(p) - (self.dist if i == 7 else self.rgb)) // (4 if i == 7 else 1) if i in (7, 8) else self.word(p))
            out['statics'] = stats
        out['log'] = CL.host_end(self.lib) if self.native else self.L.log
        if self.native and op in (1, 18):
            # Normalize newly allocated pointer labels through the allocation event.
            newlabels = [event for event in out['log'] if event[0] == 'new']
            if newlabels:
                key = 'converter' if op == 1 else 'global'
                out[key][0] = 100
        return out

    def done(self):
        if not self.native:
            CL.Stubs.deactivate(self.e, self.local_saved)
            CL.Stubs.deactivate(self.e, self.saved)


def run(e, lib, c, op):
    w = World(e, lib, c)
    try:
        return w.run(op)
    finally:
        w.done()


def mask(r):
    if r.randrange(3) == 0:
        return r.choice([0, 1, 0x80000000, MASK])
    low = r.randrange(32)
    return (((1 << r.randint(1, 32 - low)) - 1) << low) if r.randrange(2) else r.getrandbits(32)


class Case(dict):
    def __repr__(self):
        return repr({k: v for k, v in self.items() if k not in ("pixels", "palette")})


def case(r, op):
    sf, df = list(r.choice(FORMATS)), list(r.choice(FORMATS))
    if r.randrange(2):
        sf = [mask(r) for _ in range(4)] + [r.randint(1, 32), r.randrange(2)]
        df = [mask(r) for _ in range(4)] + [r.randint(1, 32), r.randrange(2)]
    db, sb = r.randint(0, 5), r.randint(1, 4)
    inplace = bool(r.randrange(2)) and db != 2
    fields = [r.randrange(64) for _ in range(4)] + [mask(r) for _ in range(8)] + [r.randrange(41) for _ in range(4)] + [db, sb, r.getrandbits(32), r.randrange(2)]
    c = Case(srcfmt=sf, dstfmt=df, pixels=r.randbytes(2048), palette=r.randbytes(1024),
             fields=fields, db=r.randint(1, 4), sb=r.randint(1, 4), inplace=inplace,
             existing=bool(r.randrange(2)), kind=r.randint(1, 4), haspal=bool(r.randrange(2)), unused=r.getrandbits(32),
             shiftseed=r.getrandbits(32), widthseed=r.getrandbits(32), masks=[mask(r), mask(r)],
             count=r.randrange(41), rows=r.randrange(6), pitches=[r.randrange(1, 128), r.randrange(1, 128)],
             nullobj=bool(r.randrange(3) == 0), flag=r.choice([-1, 0, 1, 0x10000]), nulldest=bool(r.randrange(2)),
             bits=r.randint(1, 4), center=r.randrange(64), first=r.randrange(240), colors=r.randrange(17),
             distance=r.choice([0, MASK, r.getrandbits(32)]), rgbseed=r.randrange(256),
             gd=r.randrange(-100000, 100000), rd=r.randrange(-100000, 100000), cd=r.randrange(100000),
             inc=r.randrange(-10000, 10000), square=r.choice([0, 1, 16, 256, 4096]), restart=r.randrange(2),
             side=r.randrange(-1, 13), widths=[r.randrange(21), r.randrange(21)], heights=[r.randrange(6), r.randrange(6)], nullarea=r.randrange(3))
    # A matching kind must carry an object, since the original calls Init without a NULL check.
    if op == 1:
        target = 1 if sf[5] == df[5] == 0 else 2 if sf[5] and not df[5] else 3 if not sf[5] and df[5] else 0
        if c['kind'] == target:
            c['existing'] = True
    if op in (2, 3):
        c['kind'], c['existing'] = r.choice([2, 3, 4]), True
    if op == 18:
        # Exercise real mask conversions and memcpy; palettised crossings have separate Init/dispatch cases.
        c['srcfmt'], c['dstfmt'] = list(r.choice(FORMATS[1:])), list(r.choice(FORMATS[1:]))
        if c['dstfmt'][4] == 16:
            c['inplace'] = False
        c['pitches'] = [128, 128]
    return c


def edges(op):
    r = random.Random(219)
    if op in (8, 9):
        for db in range(6):
            for wide in (0, 1):
                for n in (0, 1, 2, 40):
                    c = case(r, op)
                    c['fields'][16], c['fields'][19], c['count'] = db, wide, n
                    c['inplace'] = db != 2 and n % 2 == 0
                    yield c
    elif op == 1:
        for sf in FORMATS:
            for df in FORMATS:
                for kind in range(1, 5):
                    c = case(r, op)
                    c.update(srcfmt=sf, dstfmt=df, kind=kind, existing=True, haspal=True)
                    yield c
    elif op == 7:
        for low in range(32):
            for width in (1, 32 - low):
                for dest in (0, 1, 0x80000000, MASK):
                    c = case(r, op)
                    c['masks'] = [((1 << width) - 1) << low, dest]
                    yield c
    elif op == 13:
        for bits in range(1, 7):
            for count in (0, 1, 2):
                c = case(r, op)
                c.update(bits=bits, colors=count)
                yield c
    else:
        for i in range(12):
            c = case(r, op)
            c.update(flag=[-1, 0, 1, 0x10000][i % 4], nullobj=i < 4, count=[0, 1, 40][i % 3], rows=i % 6)
            yield c


TESTS = [FnTest(name, lambda e, c, op=i: run(e, None, c, op), lambda lib, c, op=i: run(None, lib, c, op),
                edge=lambda op=i: edges(op), rnd=lambda r, op=i: case(r, op)) for i, (_, name, _) in enumerate(FUNCS)]
