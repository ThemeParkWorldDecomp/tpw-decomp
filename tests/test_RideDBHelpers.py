"""Unit w119c: seven real functions plus a composition of the four text helpers.
Only six external callees are fake. Code units, input buffers, guards, globals,
all named coaster/texture fields and intermediate call snapshots compare.
Intentionally faulting allocations, table overflow and distant getter indices are excluded.
"""
import ctypes as C
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = 'SimTheme Park'
SHIM_LIBRARY = 'unit_RideDBHelpers'
ADDR = [0x10155170, 0x10155244, 0x10155604, 0x101556A0, 0x10039A28, 0x10156DB0, 0x10156DF8]
NAMES = ['copy_line', 'comp', 'find_string_id', 'find_line', 'LoadCoasterTypeDataFile',
         'RideInfo::GetCustomParticleEffect', 'RideInfo::GetCustomSoundBank']
COUNT, TABLE = 0x102B68B8, 0x102B68BC
MAC = [[0x35C, 4, 0x14, 0x18, 0x1C, 0x78, 0x88, 0x8C, 0x90, 0x94, 0x98,
        0x2FC, 0x300, 0x304, 0x308, 0x3C, 0xA8, 0x1C8, 0x2E8, 0x2F8, 0x318],
       [0x20, 0, 4, 8, 12, 16, 20, 24, 28], [0x808, 0x7D8], [0x3B80, 0, 8]]
HOST = None
STUBS = None
PTR_FIELDS = {2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13, 14}
SHARED = 'Data\\Generic\\Dynamic\\Textures'
VP, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32


def bind(lib, name, result, *args):
    fn = lib[name]
    fn.restype = result
    fn.argtypes = list(args)
    setattr(lib, name, fn)
    return fn


class Memory:
    """One adapter for big-endian Mac records and native host records."""
    def __init__(self, target, ppc):
        self.target = target
        self.ppc = ppc
        self.endian = '>' if ppc else '<'
        self.ptrsize = 4 if ppc else C.sizeof(VP)
        self.layout = MAC if ppc else HOST
        self.keep = []
        self.blocks = []
        self.sizes = []
        self.count = COUNT if ppc else target.tpw_rh_count()
        self.table = TABLE if ppc else target.tpw_rh_table()

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

    def wide(self, units):
        return self.alloc(2 * len(units), struct.pack(self.endian + str(len(units)) + 'H', *units))

    def units(self, a, n):
        return tuple(struct.unpack(self.endian + str(n) + 'H', self.read(a, 2 * n)))

    def lab(self, a):
        return self.target.cl.lab(a) if self.ppc else self.target.tpw_cl_lab(a)

    def call(self, op, *args):
        if self.ppc:
            return self.target.call(ADDR[op], list(args))
        return getattr(self.target, 'tpw_rh_' + ['copy', 'comp', 'id', 'line', 'coaster', 'particle', 'sound'][op])(*args)

    def allocations(self):
        if self.ppc:
            return list(zip(self.blocks, self.sizes))
        return [(self.target.tpw_rh_alloc(i), self.target.tpw_rh_size(i))
                for i in range(self.target.tpw_rh_nalloc())]

    def begin(self, fixed, queues=None):
        if self.ppc:
            self.target.cl = CL.PpcLog({a: label for label, a in fixed.items()}, queues)
        else:
            CL.host_begin(self.target, fixed, queues)

    def end(self):
        return self.target.cl.log if self.ppc else CL.host_end(self.target)

    def coaster_dump(self, a):
        offsets = self.layout[0]
        out = []
        for i, offset in enumerate(offsets[1:], 1):
            if i in (16, 17):
                out.append(self.read(a + offset, 0x120))
            elif i in PTR_FIELDS:
                out.append(self.lab(self.ptr(a + offset)))
            else:
                out.append(self.get(a + offset))
        return out

    def texture_dump(self, a):
        off = self.layout[1]
        return [self.get(a + off[1]), self.get(a + off[2], 'H'),
                self.cstr(self.ptr(a + off[3])), self.cstr(self.ptr(a + off[4])),
                *[self.lab(self.ptr(a + off[i])) for i in (5, 6, 7)], self.get(a + off[8])]

    def snapshot(self, name, a):
        o = self.layout[0]
        self.target.cl.rec(name, (self.lab(a), self.get(self.count), self.get(a + o[1]),
            self.lab(self.ptr(self.table + self.ptrsize * (self.get(self.count) - 1))),
            self.lab(self.ptr(a + o[4])), *[self.get(a + o[i]) for i in (15, 18, 19, 20)]))


def setup(runner):
    global HOST, STUBS
    lib = runner.lib
    bind(lib, 'tpw_rh_layout', U32, U32, U32)
    HOST = [[lib.tpw_rh_layout(k, i) for i in range(len(MAC[k]))] for k in range(4)]
    for name, result, args in [
        ('copy', None, [VP, VP]), ('comp', I32, [VP, VP]), ('id', I32, [VP]),
        ('line', VP, [VP]), ('coaster', VP, [U32, VP, VP, VP]),
        ('particle', U32, [VP, I32]), ('sound', I32, [VP]),
        ('reset', None, [VP, VP, VP]), ('alloc', VP, [U32]), ('size', U32, [U32]),
        ('nalloc', U32, []), ('count', VP, []), ('table', VP, []), ('tag', VP, [U32])]:
        bind(lib, 'tpw_rh_' + name, result, *args)
    bind(lib, 'tpw_cl_lab', I32, VP)
    S = CL.Stubs()
    S.real.update(ADDR)

    def allocate(e, release):
        m = e.rh
        size = e.r(3)
        e.cl.rec('ReleaseAlloc' if release else 'Alloc', (size, e.r(4)) if release else (size,))
        a = m.alloc(size + 16, (bytes(size) if release else b'\xa5' * size) + b'\xa5' * 16)
        m.blocks.append(a)
        m.sizes.append(size)
        e.cl.newlab(a)
        e.set_r(3, a)

    S.imp('LbMem_Alloc__FUi', lambda e: allocate(e, False))
    S.internal(0x10064BD4, lambda e: allocate(e, True))

    def init(e):
        m, a = e.rh, e.r(3)
        m.local = a
        e.cl.labels[a] = 50
        e.cl.rec('Init', (50, int(m.get(a + 4) == 0x1029CD7C)))
        m.setptr(a, m.marker)
        m.put(a + 8, 0x12345678)

    def defaults(e):
        m, a = e.rh, e.r(3)
        m.latest = a
        m.snapshot('Defaults', a)
        m.put(a + 0x3C, 150)
        m.put(a + 0x2E8, T.u32(e.cl.ret('factor')))
        m.setptr(a + 0x1C, m.textures[0])
        m.setptr(a + 0x14, m.marker)
        m.write(a + 0xA8, b'\x67')

    def create(e):
        m, a, balance = e.rh, e.r(3), e.r(5)
        e.cl.rec('Create', (m.lab(a), m.lab(balance)), (m.cstr(e.r(4)),))
        m.snapshot('BeforeCreate', a)
        m.setptr(a + 0x1C, m.textures[int(e.cl.ret('replace') != 0)])
        m.put(a + 0x2F8, T.u32(e.cl.ret('second')))
        m.put(a + 0x318, T.u32(e.cl.ret('product')))
        m.setptr(a + 0x88, m.marker)
        m.write(a + 0x2E7, b'\x89')
        m.put(balance + 8 + 5 * 4, T.u32(e.cl.ret('balance')))
        e.set_r(3, T.u32(e.cl.ret('Create')))

    def uninit(e):
        m, a = e.rh, e.r(3)
        folder = m.ptr(m.ptr(m.latest + 0x1C) + 12)
        e.cl.rec('UnInit', (m.lab(a), int(a == m.local), m.lab(m.ptr(a)), m.get(a + 8)), (m.cstr(folder),))
        m.snapshot('AfterFolder', m.latest)

    S.internal(0x1001EA28, init)
    S.internal(0x1001ED40, uninit)
    S.internal(0x10036664, defaults)
    S.internal(0x10039518, create)
    STUBS = S


def text_run(m, c, op):
    a = m.wide(c['a'])
    b = m.wide(c.get('b', [0, 0xBEEF]))
    old = m.wide([0x7777, 0])
    out = m.alloc(m.ptrsize)
    m.setptr(out, old if c.get('old') else 0)
    m.begin({10: a, 11: b, 12: old, 13: out})
    start = c.get('start', 0)
    if op == 0:
        m.call(0, out, a + 2 * start)
        result = m.lab(m.ptr(out))
    elif op == 1:
        result = T.s32(m.call(1, a, b))
    elif op == 2:
        result = T.s32(m.call(2, a))
    else:
        result = (m.call(3, a + 2 * start) - a) // 2
    allocations = [(m.lab(p), n, m.units(p, n // 2), m.read(p + n, 16)) for p, n in m.allocations()]
    tags = []
    if op == 2:
        for i, n in enumerate([5, 6, 6]):
            p = m.ptr(0x102A5D00 + i * 4) if m.ppc else m.target.tpw_rh_tag(i)
            tags.append(m.units(p, n))
        tags.append(m.ptr(0x102A5D0C) if m.ppc else m.target.tpw_rh_tag(3) or 0)
    return dict(result=result, a=m.units(a, len(c['a'])), b=m.units(b, len(c.get('b', [0, 0xBEEF]))),
                old=m.units(old, 2), allocations=allocations, tags=tags, log=m.end())


def getter_run(m, c, op):
    size, offset = m.layout[2]
    a = m.alloc(size, b'\xa5' * size)
    m.write(a + offset, struct.pack(m.endian + '6I', *c['words']))
    before = m.read(a, size)
    m.begin({10: a})
    result = T.u32(m.call(op, a, c['index']) if op == 5 else m.call(op, a))
    return dict(result=result, untouched=m.read(a, size) == before,
                words=tuple(m.get(a + offset + 4 * i) for i in range(6)), log=m.end())


def coaster_run(m, c):
    sentinel = m.alloc(16, b'\x5a' * 16)
    m.marker = sentinel
    m.textures = [m.alloc(m.layout[1][0], b'\xa5' * m.layout[1][0]) for _ in range(2)]
    folder = m.text(c['folder']) if c['folder'] is not None else 0
    path = m.text(c['path'])
    oldfolder = m.text('old shared')
    texfolder = m.text('original texture folder')
    balance = m.alloc(m.layout[3][0], b'\x5a' * m.layout[3][0])
    boff = m.layout[3][2]
    stride = 4 if m.ppc else 8
    for i, word in enumerate(c['slots']):
        m.put(balance + boff + i * stride, word)
    fixed = {90: sentinel, 60: balance, 70: m.textures[0], 71: m.textures[1], 80: path, 81: folder}
    if not m.ppc:
        m.target.tpw_rh_reset(*m.textures, m.marker)
    m.begin(fixed, c['queues'])
    for a in m.textures:
        o = m.layout[1]
        m.put(a + o[1], 0xCAFEBABE)
        m.put(a + o[2], 7, 'H')
        m.setptr(a + o[3], texfolder)
        m.setptr(a + o[4], oldfolder)
        for i in (5, 6, 7):
            m.setptr(a + o[i], sentinel)
        m.put(a + o[8], 0x87654321)
    textures_before = [m.read(a, m.layout[1][0]) for a in m.textures]
    balance_before = m.read(balance, m.layout[3][0])
    m.put(m.count, c['count'])
    for i in range(50):
        m.setptr(m.table + i * m.ptrsize, sentinel if i % 3 else 0)
    results = []
    states = []
    for typeword in c['types']:
        a = m.call(4, typeword, folder, path, balance)
        results.append(m.lab(a))
        states.append(m.coaster_dump(a))
    # Check that bytes outside the logical fields written by the loader/fakes remain zero.
    unexpected = []
    touched = [(i, 1 if i in (16, 17) else m.ptrsize if i in PTR_FIELDS else 4)
               for i in (1, 2, 4, 6, 15, 16, 18, 19, 20)]
    for a, n in m.allocations():
        mask = set()
        for i, width in touched:
            start = m.layout[0][i]
            mask.update(range(start, start + width))
        mask.add(m.layout[0][17] + 0x11F)
        raw = m.read(a, n)
        unexpected.append(any(value for i, value in enumerate(raw) if i not in mask))
    after = bytearray(m.read(balance, m.layout[3][0]))
    before = bytearray(balance_before)
    start = boff + 5 * stride
    before[start:start + 4] = after[start:start + 4]
    texuntouched = []
    for a, original in zip(m.textures, textures_before):
        now = bytearray(m.read(a, len(original)))
        offset = m.layout[1][4]
        now[offset:offset + m.ptrsize] = original[offset:offset + m.ptrsize]
        texuntouched.append(bytes(now) == original)
    return dict(results=results, states=states, count=m.get(m.count),
                table=[m.lab(m.ptr(m.table + i * m.ptrsize)) for i in range(50)],
                textures=[m.texture_dump(a) for a in m.textures], texture_untouched=texuntouched,
                balance_untouched=before == after,
                slots=[m.get(balance + boff + i * stride) for i in range(3806)],
                unused_zero=unexpected, guards=[m.read(a + n, 16) for a, n in m.allocations()],
                sizes=[MAC[0][0] if not m.ppc and n == HOST[0][0] else n for a, n in m.allocations()], path=m.cstr(path), folder=m.cstr(folder), log=m.end())


def composition_run(m, c):
    a = m.wide(c['a'])
    out = m.alloc(m.ptrsize)
    m.setptr(out, 0)
    m.begin({10: a, 11: out})
    cursor = a
    steps = []
    while m.get(cursor, 'H'):
        tag = T.s32(m.call(2, cursor))
        if tag == -1:
            steps.append((tag, (cursor - a) // 2))
            break
        value = m.call(3, cursor)
        m.call(0, out, value)
        block, size = m.allocations()[-1]
        nxt = m.call(3, value)
        steps.append((tag, (value - a) // 2, (nxt - a) // 2, m.units(block, size // 2)))
        cursor = nxt
    return dict(steps=steps, input=m.units(a, len(c['a'])),
                guards=[m.read(a + n, 16) for a, n in m.allocations()], log=m.end())


def run(target, c, op, ppc):
    m = Memory(target, ppc)
    if ppc:
        target.rh = m
        saved = STUBS.activate(target)
    else:
        target.tpw_rh_reset(None, None, None)
    try:
        if op == 4:
            return coaster_run(m, c)
        if op in (5, 6):
            return getter_run(m, c, op)
        if op == 7:
            return composition_run(m, c)
        return text_run(m, c, op)
    finally:
        if ppc:
            STUBS.deactivate(target, saved)


E = [0, 32, 9, 10, 13]
NON = [65, 97, 11, 12, 0xA0, 0x2003, 0x8000, 0xFFFF, 0xD800, 0xDC00]


def units(text):
    return list(map(ord, text))


def textcase(a, b=None, **extra):
    c = dict(a=(units(a) if isinstance(a, str) else list(a)) + [0, 0xBEEF, 0xD800], **extra)
    if b is not None:
        c['b'] = (units(b) if isinstance(b, str) else list(b)) + [0, 0xCAFE, 0xDC00]
    return c


def copy_edges():
    cases = [textcase(''), textcase('\n'), textcase('\r'), textcase('A'),
             textcase(' \tA\t B'), textcase([0x8000, 0xFFFF, 0xD800, 0xDC00])]
    for n in [1, 2, 3, 4, 255, 512]:
        for end in ['\0', '\n', '\r', '\r\n', '\n\r']:
            for old in [False, True]:
                cases.append(textcase('a' * n + end + 'suffix', old=old))
    return cases


def comp_edges():
    cases = [textcase([a, 65], [b, 66]) for a in E for b in E]
    for n in range(10):
        prefix = [65] * n
        for a in E:
            for b in NON:
                cases.append(textcase(prefix + [a, 67], prefix + [b, 68]))
                cases.append(textcase(prefix + [b, 67], prefix + [a, 68]))
        for b in NON:
            cases.append(textcase(prefix + [b], prefix + [b]))
            cases.append(textcase(prefix + [b], prefix + [b ^ 1]))
    cases += [textcase('NAME', 'name'), textcase(' NAME', 'NAME'), textcase('NAME', 'NAME value'),
              textcase('a' * 120, 'a' * 120), textcase('a' * 120, 'a' * 119 + 'b')]
    return cases


def id_edges():
    cases = []
    for tag in ['NAME', 'SIGNA', 'SIGNB']:
        cases += [textcase(tag), textcase(tag.lower()), textcase(tag[:-1]),
                  textcase(tag + 'X'), textcase(' ' + tag), textcase('\t' + tag)]
        for delim in E:
            cases.append(textcase(units(tag) + [delim, 0xFFFF, 65]))
        for delim in [11, 12, 0xA0, 0x2003]:
            cases.append(textcase(units(tag) + [delim]))
    cases += [textcase(x) for x in ['', ' ', '\t', 'UNKNOWN', 'SIGNC', '\r\n', '\ufeffNAME']]
    return cases


def line_edges():
    tails = ['', 'next', '  next', '\t next', '\vnext', '\fnext', '\u00a0next', '   ', '\t \t',
             '\r\n\t \n\r  next', '\r\r', '\n\n', '\r\n \t\n', '\n\r\n\r']
    return [textcase('prefix' + end + tail, start=start)
            for end in ['', '\r', '\n', '\r\n', '\n\r', '\r\r', '\n\n']
            for tail in tails for start in [0, 3]] + [textcase('', start=0)]


def random_text(r, op):
    if op == 0:
        body = [r.choice(NON + [32, 9]) for _ in range(r.randrange(350))]
        return textcase(body + [r.choice([0, 10, 13])] + [65, 66], old=bool(r.randrange(2)))
    if op == 1:
        prefix = [r.choice(NON) for _ in range(r.randrange(60))]
        a = prefix + [r.choice(E + NON)] + [r.choice(NON) for _ in range(10)]
        b = a[:] if r.randrange(4) == 0 else prefix + [r.choice(E + NON)] + [r.choice(NON) for _ in range(10)]
        return textcase(a, b)
    if op == 2:
        tag = r.choice(['NAME', 'SIGNA', 'SIGNB', 'SIGNC', 'name', 'SignB', 'UNKNOWN', 'NAMEX'])
        return textcase(r.choice(['', '', '', ' ', '\t', '\ufeff']) + tag + chr(r.choice(E + NON)) + 'value')
    chunks = ['skip']
    for _ in range(r.randrange(12)):
        chunks.append(r.choice(['', ' ', '\t \t', '   ']))
    chunks.append(r.choice(['  real', '\treal', '\v', '\f', '\u00a0', '', ' \t']))
    text = ''.join(chunk + r.choice(['\r', '\n', '\r\n', '\n\r']) for chunk in chunks)
    return textcase(text + r.choice(['', 'end', ' \t']), start=r.randrange(4))


def getter_edges():
    cases = []
    for word in [0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]:
        for index in range(-1, 5):
            words = [word ^ (i * 0x10101) for i in range(6)]
            words[index + 1] = word
            words[5] = word
            cases.append(dict(words=words, index=index))
    return cases


def random_getter(r):
    return dict(words=[r.getrandbits(32) for _ in range(6)], index=r.randrange(-1, 5))


def coaster_case(count=0, types=None, replace=1, result=0, folder='unused folder', seed=0):
    return dict(count=count, types=types or [seed], folder=folder, path='texture path ' + str(seed),
                slots=[(seed + i * 0x10203) & 0xFFFFFFFF for i in range(3806)],
                queues={'replace': [replace], 'Create': [result], 'factor': [0x80000001 ^ seed],
                        'second': [0xFEDCBA98 ^ seed], 'product': [0xFFFFFFFF ^ seed], 'balance': [seed ^ 0xCAFEBABE]})


def coaster_edges():
    cases = [coaster_case(count, [word], replace, result, folder)
             for count in [0, 1, 7, 49] for word in [0, 1, 0xFFFF, 0x80000000, 0xFFFFFFFF]
             for replace in [0, 1] for result in [0, 1, -1] for folder in [None, 'different folder']]
    c = coaster_case(2, [1, 0x80000000, 0xFFFFFFFF])
    c['queues']['replace'] = [0, 1, 0]
    c['queues']['Create'] = [0, 1, -1]
    cases.append(c)
    return cases


def random_coaster(r):
    count = r.randrange(50)
    return coaster_case(count, [r.getrandbits(32) for _ in range(r.randint(1, min(3, 50 - count)))],
                        r.randrange(2), r.choice([0, 1, -1, 7]), r.choice([None, 'folder:' + str(r.randrange(1000))]),
                        r.getrandbits(32))


def composition_case(ending='\r\n', values=None):
    values = values or ['Ride name', 'Sign A', 'Sign B']
    text = ''
    for tag, value in zip(['NAME', 'SIGNA', 'SIGNB'], values):
        text += tag + ending + ' \t' + ending + value + ending + ending
    return textcase(text + 'UNKNOWN' + ending + 'ignored')


def make_test(op, edge, rnd):
    return FnTest(NAMES[op] if op < 7 else 'RideDB helper composition',
                  lambda e, c: run(e, c, op, True), lambda lib, c: run(lib, c, op, False), edge=edge, rnd=rnd)


TESTS = [make_test(0, copy_edges, lambda r: random_text(r, 0)),
         make_test(1, comp_edges, lambda r: random_text(r, 1)),
         make_test(2, id_edges, lambda r: random_text(r, 2)),
         make_test(3, line_edges, lambda r: random_text(r, 3)),
         make_test(4, coaster_edges, random_coaster),
         make_test(5, getter_edges, random_getter), make_test(6, getter_edges, random_getter),
         make_test(7, lambda: [composition_case(e) for e in ['\r', '\n', '\r\n', '\n\r']],
                   lambda r: composition_case(r.choice(['\r', '\n', '\r\n', '\n\r']),
                       ['value' + str(r.randrange(10000)) for _ in range(3)]))]
