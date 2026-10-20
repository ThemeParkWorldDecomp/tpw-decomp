"""Unit w138: the six coaster type loaders (SetCoasterTypeDefaults, CreateCoasterTrackMasterDMeshSet,
CreateCrossSectionsFromSamFile, SetupPylonControlsFromSamFile, SetupCarAndTrainTypesFromSamFile and CreateCoasterFromSamFile)
run as the original on the Mac side. Faked on both sides: the allocator (zeroed blocks with a guard), ERRORBox and the four
cross-section builders (scripted answers, logged with every argument). The type, the pylon controls, every allocated block, the
balance (untouched), the allocation sizes (host records are bigger: the Mac's size is the truth, each host size must be the
host record size that maps to it) and the call log compare.
"""
import ctypes as C
import random
import struct
import string
import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = 'SimTheme Park'
SHIM_LIBRARY = 'unit_CoasterTypes'
ADDR = [0x10036664, 0x10036B20, 0x10038AE4, 0x10038D00, 0x100392CC, 0x10039518]
NAMES = ['SetCoasterTypeDefaults', 'CreateCoasterTrackMasterDMeshSet', 'CreateCrossSectionsFromSamFile',
         'SetupPylonControlsFromSamFile', 'SetupCarAndTrainTypesFromSamFile', 'CreateCoasterFromSamFile']
HOSTFN = ['defaults', 'mesh', 'cross', 'pylon', 'car', 'create']
TEXTURE_BASE, STATICS, PAIRS = 0x1029CE20, 0x1029CDF0, 0x1029CCE4

# The type's named words in the order of tpw_ct_layout(0, i + 1): offset and kind (w word, p pointer, c pointer into the car array).
TYPE_FIELDS = [(0x00, 'w'), (0x04, 'w'), (0x08, 'w'), (0x0C, 'w'), (0x10, 'w'), (0x14, 'p'), (0x18, 'p'), (0x1C, 'p'), (0x20, 'w'),
               (0x24, 'w'), (0x28, 'w'), (0x2C, 'w'), (0x30, 'w'), (0x34, 'w'), (0x38, 'w'), (0x3C, 'w')]
TYPE_FIELDS += [(o, 'w') for o in range(0x40, 0x74, 4)]
TYPE_FIELDS += [(0x78, 'p'), (0x88, 'p'), (0x8C, 'w'), (0x90, 'p'), (0x94, 'p'), (0x98, 'p'), (0x9C, 'p'), (0xA0, 'w'), (0xA4, 'w'),
                (0x2E8, 'w'), (0x2EC, 'w'), (0x2F0, 'w'), (0x2F4, 'w'), (0x2F8, 'w'), (0x2FC, 'c'), (0x300, 'c'), (0x304, 'c'),
                (0x308, 'p'), (0x30C, 'w'), (0x310, 'w'), (0x314, 'w'), (0x318, 'w'), (0x31C, 'w')]
TYPE_FIELDS += [(o, 'w') for o in range(0x320, 0x35C, 4)]
NTYPE = len(TYPE_FIELDS)
CARS_FIELD = [i for i, (o, k) in enumerate(TYPE_FIELDS) if o == 0x308][0]
PYLON_BASE = (0xA8, 0x1C8)
# The pylon control's named words in the order of tpw_ct_layout(1, i + 1).
PYLON_FIELDS = [(0x00, 'w'), (0x0C, 'w'), (0x10, 'p')] + [(o, 'w') for o in range(0x58, 0x74, 4)] + \
    [(o, 'w') for o in range(0x74, 0x9C, 4)] + [(o, 'w') for o in (0x9C, 0xA0, 0xA4, 0xA8, 0xAC, 0xB0, 0xB4, 0xB8, 0xCC)] + [(0xD0, 'f20')]
MAC = [[0x35C] + [o for o, k in TYPE_FIELDS] + list(PYLON_BASE),
       [0x120] + [o for o, k in PYLON_FIELDS],
       [0x34, 0, 4, 8, 0x10, 0x14, 0x18, 0x1C, 0x2C],
       [0x18, 0, 4, 8, 0xC, 0x10, 0x14],
       [0x10, 0, 4, 8, 0xC],
       [0x20, 0, 4, 8, 0x10],
       [0x40, 0, 4, 8],
       [0x30, 0],
       [0x3B80, 8]]
HOST = None
STUBS = None
VP, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32
BALANCE_SLOTS = 3806


def bind(lib, name, result, *args):
    fn = lib[name]
    fn.restype = result
    fn.argtypes = list(args)
    setattr(lib, name, fn)
    return fn


class Memory:
    """One adapter for big-endian Mac records and native host records."""
    def __init__(self, target, ppc):
        self.target, self.ppc = target, ppc
        self.endian = '>' if ppc else '<'
        self.ptrsize = 4 if ppc else C.sizeof(VP)
        self.layout = MAC if ppc else HOST
        self.keep, self.blocks, self.sizes = [], [], []
        self.statics = STATICS if ppc else target.tpw_ct_statics()
        self.pairs = PAIRS if ppc else target.tpw_ct_pairs()

    def alloc(self, size, data=None):
        if self.ppc:
            return self.target.alloc(size, data or bytes(size), alignment=8)
        buf = C.create_string_buffer(data or bytes(size), size)
        self.keep.append(buf)
        return C.addressof(buf)

    def read(self, a, n):
        return self.target.read(a, n) if self.ppc else C.string_at(a, n)

    def write(self, a, raw):
        if self.ppc:
            self.target.write(a, raw)
        else:
            C.memmove(a, raw, len(raw))

    def get(self, a, fmt='I'):
        return struct.unpack(self.endian + fmt, self.read(a, struct.calcsize(fmt)))[0]

    def put(self, a, v, fmt='I'):
        self.write(a, struct.pack(self.endian + fmt, v))

    def ptr(self, a):
        return self.get(a, 'I' if self.ppc else 'Q')

    def setptr(self, a, v):
        self.put(a, v, 'I' if self.ppc else 'Q')

    def text(self, text):
        return self.alloc(len(text) + 1, text.encode('latin-1') + b'\0')

    def cstr(self, a):
        if not a:
            return '(null)'
        return self.target.cstr(a) if self.ppc else C.string_at(a).decode('latin-1')

    def lab(self, a):
        return self.target.cl.lab(a) if self.ppc else self.target.tpw_cl_lab(a)

    def call(self, op, *args):
        if self.ppc:
            return self.target.call(ADDR[op], list(args))
        return getattr(self.target, 'tpw_ct_' + HOSTFN[op])(*args)

    def allocations(self):
        if self.ppc:
            return list(zip(self.blocks, self.sizes))
        return [(self.target.tpw_ct_alloc(i), self.target.tpw_ct_size(i)) for i in range(self.target.tpw_ct_nalloc())]

    def begin(self, fixed, queues=None):
        if self.ppc:
            self.target.cl = CL.PpcLog({a: label for label, a in fixed.items()}, queues)
        else:
            CL.host_begin(self.target, fixed, queues)

    def end(self):
        return self.target.cl.log if self.ppc else CL.host_end(self.target)

    # A pointer as a comparable value: null, the sentinel, the shared point block, a static table or the n-th allocation.
    def pv(self, p):
        if p == 0:
            return 'null'
        if p == self.sentinel:
            return 'S'
        if p == self.points:
            return 'P'
        if p == self.pairs:
            return 'pairs'
        for k in range(3):
            if p == self.statics + k * self.layout[4][0]:
                return ('static', k)
        for i, (a, n) in enumerate(self.allocations()):
            if a == p:
                return ('a', i)
        return ('?',)

    def isalloc(self, p):
        return any(a == p for a, n in self.allocations())

    def nm(self, p):
        """A name pointer: null, an allocation (index and text) or a literal (text)."""
        if p == 0:
            return 'null'
        if p == self.sentinel:
            return 'S'
        v = self.pv(p)
        return (v if v != ('?',) else 'lit', self.cstr(p))

    def words(self, a, fields, base_layout):
        out = []
        for i, (off, kind) in enumerate(fields):
            o = a + base_layout[i + 1]
            if kind == 'w':
                out.append(self.get(o))
            elif kind == 'p':
                out.append(self.nm(self.ptr(o)) if base_layout is self.layout[1] else self.pv(self.ptr(o)))
            elif kind == 'c':
                p, arr = self.ptr(o), self.ptr(a + self.layout[0][CARS_FIELD + 1])
                if p in (0, self.sentinel) or not arr:
                    out.append(self.pv(p))
                else:
                    out.append(('car', divmod(p - arr, self.layout[2][0])))
            else:
                out.append(tuple(self.get(o + 4 * j) for j in range(20)))
        return out

    def dump_type(self, t, c):
        L = self.layout[0]
        d = dict(words=self.words(t, TYPE_FIELDS, L))
        d['pylons'] = [self.words(t + L[NTYPE + 1 + k], PYLON_FIELDS, self.layout[1]) for k in range(2)]
        ts = self.ptr(t + L[8])
        d['ts'] = None
        if self.isalloc(ts):
            o = self.layout[5]
            count = self.get(ts + o[2], 'H')
            entries = self.ptr(ts + o[4])
            e = self.layout[4]
            d['ts'] = [self.get(ts + o[1]), count, self.cstr(self.ptr(ts + o[3])), self.pv(entries)]
            if self.isalloc(entries):
                d['ts'].append([[self.get(entries + k * e[0] + e[1]), self.nm(self.ptr(entries + k * e[0] + e[2])),
                                 self.get(entries + k * e[0] + e[3]), self.get(entries + k * e[0] + e[4])]
                                for k in range(min(count, 40))])
        ms = self.ptr(t + L[7])
        d['ms'] = None
        if self.isalloc(ms):
            o = self.layout[6]
            ent = self.ptr(ms + o[3])
            d['ms'] = [self.get(ms + o[1]), self.get(ms + o[2], 'H'), self.pv(ent),
                       self.get(ent + self.layout[7][1]) if self.isalloc(ent) else None]
        recs = self.ptr(t + L[31])
        d['records'] = None
        if self.isalloc(recs):
            o = self.layout[3]
            d['records'] = []
            for k in range(c.get('ncross', 0) + 1):
                r = recs + k * o[0]
                block = self.ptr(r + o[4])
                count = self.get(r + o[2])
                d['records'].append([self.get(r + o[1]), count, self.get(r + o[3]), self.pv(block),
                                     self.read(block, count * 12) if self.isalloc(block) and count < 8 else None,
                                     self.get(r + o[5]), self.get(r + o[6])])
        cars = self.ptr(t + L[CARS_FIELD + 1])
        d['cars'] = None
        if self.isalloc(cars):
            o = self.layout[2]
            d['cars'] = [[self.get(cars + k * o[0] + o[1]), self.get(cars + k * o[0] + o[2]),
                          self.nm(self.ptr(cars + k * o[0] + o[3]))] +
                         [self.get(cars + k * o[0] + o[j]) for j in (4, 5, 6, 7)] +
                         [self.pv(self.ptr(cars + k * o[0] + o[8]))] for k in range(c.get('ncar', 0))]
        pairs = self.ptr(t + L[[i for i, (o, kk) in enumerate(TYPE_FIELDS) if o == 0x9C][0] + 1])
        d['pairs'] = None
        if self.isalloc(pairs):
            n = self.get(t + L[[i for i, (o, kk) in enumerate(TYPE_FIELDS) if o == 0xA0][0] + 1])
            d['pairs'] = [self.get(pairs + 4 * k) for k in range(min(2 * n, 16))]
        return d

    def unnamed_ok(self, t, size):
        """The bytes of the type outside its named words are still the 0x5a fill."""
        L = self.layout[0]
        keep = bytearray(size)
        for i, (off, kind) in enumerate(TYPE_FIELDS):
            o = L[i + 1]
            keep[o:o + (self.ptrsize if kind in 'pc' else 4)] = b'\xff' * (self.ptrsize if kind in 'pc' else 4)
        P = self.layout[1]
        for k in range(2):
            b = L[NTYPE + 1 + k]
            keep[b:b + P[0]] = b'\xff' * P[0]
        raw = self.read(t, size)
        ok = all(raw[i] == 0x5A for i in range(size) if not keep[i])
        for k in range(2):
            b = t + L[NTYPE + 1 + k]
            raw = self.read(b, P[0])
            mask = bytearray(P[0])
            for i, (off, kind) in enumerate(PYLON_FIELDS):
                w = self.ptrsize if kind == 'p' else 80 if kind == 'f20' else 4
                mask[P[i + 1]:P[i + 1] + w] = b'\xff' * w
            ok = ok and all(raw[i] == 0x5A for i in range(P[0]) if not mask[i])
        return ok


def compat(h, mac):
    if h == mac:
        return True
    for k, fixed in ((5, 0x20), (6, 0x40), (7, 0x30)):
        if h == HOST[k][0] and mac == fixed:
            return True
    return any(h % HOST[k][0] == 0 and mac == h // HOST[k][0] * MAC[k][0] for k in (2, 3, 4))


def setup(runner):
    global HOST, STUBS
    lib = runner.lib
    bind(lib, 'tpw_ct_layout', U32, U32, U32)
    HOST = [[lib.tpw_ct_layout(k, i) for i in range(len(MAC[k]))] for k in range(9)]
    HOST[0][NTYPE + 1:] = [lib.tpw_ct_layout(0, i) for i in range(NTYPE + 1, NTYPE + 3)]
    bind(lib, 'tpw_ct_defaults', None, VP)
    bind(lib, 'tpw_ct_mesh', I32, VP, VP, VP)
    for name in ('cross', 'pylon', 'car'):
        bind(lib, 'tpw_ct_' + name, I32, VP, VP)
    bind(lib, 'tpw_ct_create', I32, VP, VP, VP)
    bind(lib, 'tpw_ct_reset', None, VP, U32)
    bind(lib, 'tpw_ct_alloc', VP, U32)
    bind(lib, 'tpw_ct_size', U32, U32)
    bind(lib, 'tpw_ct_nalloc', U32)
    bind(lib, 'tpw_ct_statics', VP)
    bind(lib, 'tpw_ct_pairs', VP)
    bind(lib, 'tpw_cl_lab', I32, VP)
    T.install_std_stubs(runner.emu)
    S = CL.Stubs()
    S.real.update(ADDR)

    def allocate(e):
        m = e.ct
        size = e.r(3)
        e.cl.rec('ReleaseAlloc', (size, e.r(4)))
        a = m.alloc(size + 16, bytes(size) + b'\xa5' * 16)
        m.blocks.append(a)
        m.sizes.append(size)
        e.cl.newlab(a)
        e.set_r(3, a)

    def errorbox(e):
        e.cl.rec('ERRORBox', (), (e.ct.cstr(e.r(3)),))

    def points(e):
        m, t, rec, bal, out = e.ct, e.r(3), e.r(4), e.r(5), e.r(6)
        e.cl.rec('Points', (m.lab(t), m.lab(rec), m.lab(bal), m.lab(out)))
        for i in range(m.get(t + 0x8C)):
            r = rec + 0x18 * i
            m.put(r, T.u32(e.cl.ret('p00')))
            m.put(r + 4, T.u32(e.cl.ret('pcount')))
            m.put(r + 8, T.u32(e.cl.ret('p08')))
            m.setptr(r + 0xC, m.points if e.cl.ret('pblock') else 0)
            m.put(r + 0x10, T.u32(e.cl.ret('p10')))
            m.put(r + 0x14, T.u32(e.cl.ret('p14')))
        m.put(out, T.u32(e.cl.ret('out')))
        e.set_r(3, T.u32(e.cl.ret('Points', 1)))

    def edges(e):
        m = e.ct
        e.cl.rec('Edges', (m.lab(e.r(3)), m.lab(e.r(4)), m.lab(e.r(5))))
        e.set_r(3, T.u32(e.cl.ret('Edges', 1)))

    def selects(e):
        m, t, rec, count, bal, out = e.ct, e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
        r = rec + 0x18 * count
        e.cl.rec('Selects', (m.lab(t), m.lab(rec), count, m.lab(bal), T.s32(out), m.get(r), m.get(r + 4), m.get(r + 8),
                             m.lab(m.ptr(r + 0xC)), m.get(r + 0x10), m.get(r + 0x14)))
        e.set_r(3, T.u32(e.cl.ret('Selects', 1)))

    def optimise(e):
        m = e.ct
        e.cl.rec('Optimise', (m.lab(e.r(3)), m.lab(e.r(4)), e.r(5)))

    S.internal(0x10064BD4, allocate)
    S.internal(0x1006FD3C, errorbox)
    S.internal(0x10036D5C, points)
    S.internal(0x10037AA8, edges)
    S.internal(0x1003851C, selects)
    S.internal(0x100388CC, optimise)
    STUBS = S


def case_run(m, c, op):
    t = m.target
    base = c['base']
    if m.ppc:
        m.put(TEXTURE_BASE, base)
    m.sentinel = m.alloc(16, b'\x5a' * 16)
    m.points = m.alloc(64, bytes((7 * i + 1) & 0xFF for i in range(64)))
    if not m.ppc:
        t.tpw_ct_reset(m.points, base)
    path = m.text(c['path'])
    L = m.layout
    balance = m.alloc(L[8][0], b'\x5a' * L[8][0])
    stride = 4 if m.ppc else 8
    boff = L[8][1]
    fill = random.Random(c['seed'] ^ 0x5A5A)
    for i in range(BALANCE_SLOTS):
        m.put(balance + boff + i * stride, fill.getrandbits(32))
    for off, (kind, v) in c['slots'].items():
        a = balance + boff + (off - 8) // 4 * stride
        if kind == 'i':
            m.put(a, v)
        else:
            m.setptr(a, m.text(v) if v is not None else 0)
    ty = m.alloc(L[0][0], b'\x5a' * L[0][0])
    rng = random.Random(c['seed'])
    for i, (off, kind) in enumerate(TYPE_FIELDS):
        if kind == 'w':
            m.put(ty + L[0][i + 1], rng.getrandbits(32))
        else:
            m.setptr(ty + L[0][i + 1], rng.choice([0, m.sentinel]))
    for k in range(2):
        b = ty + L[0][NTYPE + 1 + k]
        for i, (off, kind) in enumerate(PYLON_FIELDS):
            o = b + L[1][i + 1]
            if kind == 'w':
                m.put(o, rng.getrandbits(32))
            elif kind == 'p':
                m.setptr(o, rng.choice([0, m.sentinel]))
            else:
                for j in range(20):
                    m.put(o + 4 * j, rng.getrandbits(32))
    before = m.read(balance, L[8][0])
    fixed = {90: m.sentinel, 91: m.points, 60: balance, 61: ty, 80: path}
    m.begin(fixed, c['queues'])
    args = {0: (ty,), 1: (ty, path, balance), 2: (ty, balance), 3: (ty, balance), 4: (ty, balance),
            5: (ty, path, balance)}[op]
    result = m.call(op, *args)
    log = m.end()
    allocs = m.allocations()
    hsizes = [n for a, n in allocs]
    if m.ppc:
        c['_mac_sizes'] = list(hsizes)
        sizes = hsizes
    else:
        mac = c.get('_mac_sizes')
        sizes = [x if mac is None or len(mac) != len(hsizes) or not compat(x, mac[i]) else mac[i]
                 for i, x in enumerate(hsizes)]
        k = 0
        fixed_log = []
        for ev in log:
            if ev[0] == 'ReleaseAlloc':
                ev = (ev[0], (sizes[k], ev[1][1]), ev[2])
                k += 1
            fixed_log.append(ev)
        log = fixed_log
    d = m.dump_type(ty, c)
    d.update(result=None if op == 0 else T.u32(result), unnamed_ok=m.unnamed_ok(ty, L[0][0]),
             balance_untouched=m.read(balance, L[8][0]) == before, sizes=sizes,
             guards=[m.read(a + n, 16) == b'\xa5' * 16 for a, n in allocs] if not m.ppc else
             [m.read(a + n, 16) == b'\xa5' * 16 for a, n in allocs],
             path=m.cstr(path), log=log)
    if m.ppc:
        m.put(TEXTURE_BASE, 3)
    return d


def run(target, c, op, ppc):
    m = Memory(target, ppc)
    if ppc:
        target.ct = m
        saved = STUBS.activate(target)
    try:
        return case_run(m, c, op)
    finally:
        if ppc:
            STUBS.deactivate(target, saved)
        else:
            target.tpw_ct_reset(None, 3)


# ---------------------------------------------------------------- cases

LETTERS = string.ascii_letters + string.digits + '._'
SPECIAL_FLOATS = [0x80000000, 0x3F800000, 0xBF800000, 0x41200000, 0x40490FDB, 0x3DCCCCCD, 0x00000001, 0x7FC00000, 0x7F800000,
                  0xFF800000, 0xFF7FFFFF, 0x80000001]


class Slots:
    def __init__(self, r):
        self.r, self.d = r, {}

    def i(self, off, v):
        self.d[off] = ('i', v & 0xFFFFFFFF)

    def s(self, off, text):
        self.d[off] = ('s', text)

    def rint(self, off, zero=0.35):
        r = self.r
        x = r.random()
        self.i(off, 0 if x < zero else r.randint(1, 9) if x < 0.65 else -r.randint(1, 9) if x < 0.8 else r.getrandbits(32))

    def rfloat(self, off, unset=0.3):
        r = self.r
        x = r.random()
        if x < unset:
            bits = 0
        elif x < unset + 0.05:
            bits = r.choice(SPECIAL_FLOATS)
        else:
            bits = struct.unpack('>I', struct.pack('>f', r.uniform(-200, 200)))[0]
        self.i(off, bits)

    def name(self, off):
        self.s(off, ''.join(self.r.choice(LETTERS) for _ in range(self.r.randint(1, 30))))


def gen_mesh(b, r, n):
    b.i(0xFC, n)
    for k in range(n):
        b.name(0xC + 12 * k)
        b.rint(0x10 + 12 * k, 0.5)
        b.rfloat(0x14 + 12 * k, 0.5)


def gen_cross(b, r, n, queues):
    b.i(0x3530, n)
    b.i(0x3B00, r.choice([0, 0, 1, n, n + 1, r.randint(0, n + 1)]))
    for key, f in (('p00', lambda: r.getrandbits(32)), ('pcount', lambda: r.randint(0, 4)), ('p08', lambda: r.getrandbits(32)),
                   ('pblock', lambda: r.choice([0, 1, 1])), ('p10', lambda: r.getrandbits(32)), ('p14', lambda: r.getrandbits(32))):
        queues[key] = [f() for _ in range(max(n, 1))]
    queues['out'] = [r.getrandbits(32)]
    for key in ('Points', 'Edges', 'Selects'):
        queues[key] = [r.choice([1, 1, 1, 0, 7])]


def gen_pylon(b, r, count, valid=True):
    b.i(0x3A20, count)
    for k in range(count):
        o = 0x38A8 + k * 0xBC
        ok = valid or r.random() > 0.4
        if ok or r.random() < 0.5:
            b.name(o)
        else:
            b.s(o, None)
        b.i(o + 4, r.getrandbits(32) if ok or r.random() < 0.5 else 0)
        b.rint(o + 8)
        b.rfloat(o + 0xC, 0.0 if ok else 0.4)
        if ok and not b.d[o + 0xC][1]:
            b.i(o + 0xC, 0x40000000)
        b.rfloat(o + 0x10)
        for q in (0x14, 0x18, 0x1C):
            b.rint(o + q)
        for q in range(0x20, 0x54, 4):
            b.rfloat(o + q)
        b.rint(o + 0x54)
        b.rint(o + 0x58)
        for q in range(0x5C, 0xBC, 4):
            b.rfloat(o + q)
        b.i(o + 0x5C, b.d[o + 0x5C][1])


def gen_car(b, r, n, valid=True):
    b.i(0x3A78, n)
    for k in range(n):
        o = 0x3A24 + 0x1C * k
        ok = valid or r.random() > 0.4
        b.rint(o, 0.5)
        b.i(o + 4, r.getrandbits(32) | 1 if ok or r.random() < 0.5 else 0)
        if ok or r.random() < 0.5:
            b.name(o + 8)
        else:
            b.s(o + 8, None)
        for q in (0xC, 0x10, 0x14, 0x18):
            b.rfloat(o + q, 0.3 if q == 0x18 else 0.0)
    for q in (0x5C, 0x60, 0x64):
        b.i(0x3A24 + q, r.randint(0, n))
    b.rint(0x3A7C)
    for off in (0x3A8C, 0x3A90, 0x3A94):
        b.rfloat(off, 0.2)


def gen_optional(b, r):
    for off in (0x3AE8, 0x3AEC, 0x3AF0, 0x3AF4, 0x3AF8, 0x3AFC, 0x3B78, 0x3B7C):
        b.rint(off)
    for off in [0x3AE4, 0x3ACC, 0x3AD0, 0x3AD8, 0x3ADC, 0x3AE0, 0x3B08, 0x3B74, 0x3B1C, 0x3B20, 0x3B24] + \
            list(range(0x3B0C, 0x3B1C, 4)) + list(range(0x3B28, 0x3B74, 4)):
        b.rfloat(off)


def gen_pairs(b, r, count):
    b.i(0x3AC8, count)
    for k in range(6):
        b.rfloat(0x3A98 + 8 * k, 0.1)
        b.rfloat(0x3A9C + 8 * k, 0.1)


def make_case(op, seed, build, **extra):
    r = random.Random(seed * 7919 + op)
    b, queues = Slots(r), {}
    ncross = ncar = 0
    if build in ('mesh', 'create'):
        gen_mesh(b, r, extra.get('ntex', r.choice([1, 2, 3, 5, 20])))
    if build in ('cross', 'create'):
        ncross = extra.get('ncross', r.choice([1, 2, 3, 7]))
        gen_cross(b, r, ncross, queues)
    if build in ('pylon', 'create'):
        gen_pylon(b, r, extra.get('npylon', r.choice([0, 1, 2, 2])), extra.get('pylon_ok', True))
    if build in ('car', 'create'):
        ncar = extra.get('ncar', r.choice([1, 2, 3, 3]))
        gen_car(b, r, ncar, extra.get('car_ok', True))
    if build == 'create':
        gen_optional(b, r)
        gen_pairs(b, r, extra.get('npairs', r.choice([0, 1, 3, 6])))
    c = dict(op=op, seed=seed, path='Data\\tex\\' + ''.join(r.choice(LETTERS) for _ in range(r.randint(1, 12))), base=extra.get('base', 3),
             slots=b.d, queues=queues, ncross=ncross, ncar=ncar)
    for key, v in extra.get('queues', {}).items():
        queues[key] = v
    return c


def defaults_edges():
    return [make_case(0, s, 'none') for s in range(4)]


def random_defaults(r):
    return make_case(0, r.getrandbits(30), 'none')


def mesh_edges():
    cases = [make_case(1, 100 + s, 'mesh', ntex=n, base=base) for s, (n, base) in enumerate(
        [(1, 3), (2, 3), (3, 3), (20, 3), (4, 0), (4, 1), (2, 2), (5, 3)])]
    for s in range(6):
        cases.append(make_case(1, 200 + s, 'mesh', ntex=r_choice(s)))
    zero = make_case(1, 300, 'mesh', ntex=1)
    zero['slots'][0xFC] = ('i', 0)
    return cases + [zero]


def r_choice(s):
    return [1, 2, 3, 7, 12, 20][s % 6]


def random_mesh(r):
    c = make_case(1, r.getrandbits(30), 'mesh', ntex=r.choice([1, 2, 3, 4, 8, 20]), base=r.choice([3, 3, 3, 0, 1, 2]))
    if r.random() < 0.08:
        c['slots'][0xFC] = ('i', 0)
    return c


def cross_edges():
    cases = []
    for s, (n, ans) in enumerate([(1, (1, 1, 1)), (2, (1, 1, 1)), (3, (0, 1, 1)), (3, (1, 0, 1)), (3, (1, 1, 0)), (5, (1, 1, 1)),
                                  (7, (1, 1, 1))]):
        cases.append(make_case(2, 400 + s, 'cross', ncross=n, queues=dict(zip(('Points', 'Edges', 'Selects'), [[a] for a in ans]))))
    for v in (0, 1, 2, 3, 4):
        c = make_case(2, 500 + v, 'cross', ncross=3)
        c['slots'][0x3B00] = ('i', v)
        cases.append(c)
    zero = make_case(2, 600, 'cross', ncross=1)
    zero['slots'][0x3530] = ('i', 0)
    return cases + [zero]


def random_cross(r):
    c = make_case(2, r.getrandbits(30), 'cross', ncross=r.randint(1, 8))
    if r.random() < 0.05:
        c['slots'][0x3530] = ('i', 0)
        c['ncross'] = 0
    return c


def pylon_edges():
    cases = [make_case(3, 700 + s, 'pylon', npylon=n) for s, n in enumerate([0, 1, 2, 1, 2, 2])]
    cases += [make_case(3, 800 + s, 'pylon', npylon=2, pylon_ok=False) for s in range(8)]
    return cases


def random_pylon(r):
    return make_case(3, r.getrandbits(30), 'pylon', npylon=r.choice([0, 1, 2, 2, 2]), pylon_ok=r.random() < 0.8)


def car_edges():
    cases = [make_case(4, 900 + s, 'car', ncar=n) for s, n in enumerate([1, 2, 3, 3, 1])]
    cases += [make_case(4, 950 + s, 'car', ncar=3, car_ok=False) for s in range(8)]
    zero = make_case(4, 990, 'car', ncar=1)
    zero['slots'][0x3A78] = ('i', 0)
    return cases + [zero]


def random_car(r):
    c = make_case(4, r.getrandbits(30), 'car', ncar=r.choice([1, 2, 3]), car_ok=r.random() < 0.8)
    if r.random() < 0.05:
        c['slots'][0x3A78] = ('i', 0)
        c['ncar'] = 0
    return c


def create_edges():
    cases = [make_case(5, 1000 + s, 'create', npairs=n) for s, n in enumerate([0, 1, 3, 6, 2, 5])]
    for s, kind in enumerate(['track', 'cross0', 'points', 'edges', 'selects', 'pylon', 'car']):
        c = make_case(5, 1100 + s, 'create')
        if kind == 'track':
            c['slots'][0xFC] = ('i', 0)
        elif kind == 'cross0':
            c['slots'][0x3530] = ('i', 0)
            c['ncross'] = 0
        elif kind in ('points', 'edges', 'selects'):
            c['queues'][kind.capitalize()] = [0]
        elif kind == 'pylon':
            c['slots'][0x3A20] = ('i', 1)
            c['slots'][0x38A8] = ('s', None)
        else:
            c['slots'][0x3A78] = ('i', 0)
            c['ncar'] = 0
        cases.append(c)
    for s in range(6):
        cases.append(make_case(5, 1200 + s, 'create', base=[0, 1, 2, 3, 3, 3][s]))
    return cases


def random_create(r):
    c = make_case(5, r.getrandbits(30), 'create', base=r.choice([3, 3, 3, 3, 0, 1, 2]))
    x = r.random()
    if x < 0.05:
        c['slots'][0xFC] = ('i', 0)
    elif x < 0.1:
        c['slots'][0x3530] = ('i', 0)
        c['ncross'] = 0
    elif x < 0.18:
        c['queues'][r.choice(['Points', 'Edges', 'Selects'])] = [0]
    elif x < 0.24:
        c['slots'][0x38A8] = ('s', None)
    elif x < 0.3:
        c['slots'][0x3A78] = ('i', 0)
        c['ncar'] = 0
    return c


def make_test(op, edge, rnd):
    return FnTest(NAMES[op], lambda e, c: run(e, c, op, True), lambda lib, c: run(lib, c, op, False), edge=edge, rnd=rnd)


TESTS = [make_test(0, defaults_edges, random_defaults), make_test(1, mesh_edges, random_mesh),
         make_test(2, cross_edges, random_cross), make_test(3, pylon_edges, random_pylon),
         make_test(4, car_edges, random_car), make_test(5, create_edges, random_create)]
