"""Unit w119b: eleven real loader functions plus the four balance-table virtuals.
External functions are recording fakes. Full record buffers and written scalar fields compare exactly.
LoadStrings feeds equal UTF-16 code units and uses a recording no-op swap on both sides.
"""
import ctypes as C
import random
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_SaveLoad import string_stubs, s_write, s_read, wide_read

PROGRAM = 'SimTheme Park'
SHIM_LIBRARY = 'unit_RideDBLoad'
ADDR = [0x1015600C, 0x10154EA8, 0x10155C00, 0x101557A4, 0x101559F8, 0x10156B28,
        0x10156BC4, 0x10156F40, 0x101550BC, 0x1015457C, 0x1003AB7C]
NAMES = ['CRideDatabase::LoadItem', 'CRideDatabase::LoadTheme', 'CRideDatabase::ScanDirectory',
         'CRideDatabase::ProcessShape', 'RideInfo::LoadStrings', 'RideInfo::LoadSounds',
         'RideInfo::LoadParticles', 'RideInfo::SetIsCustomRide', 'RideInfo::DeallocStrings',
         'RideInfo::~RideInfo', 'LoadCoasterType']
G, GAME, ERROR, MISSING = 0x103B74C8, 0x1037F134, 0x1037F1C0, 0x102A5CE2
GLOBAL_OFF = [0x43F8, 0x4290, 0x427C, 0x42A4, 0x40D8, 0x45B0, 0x43A8,
              0x4650, 0x4740, 0x4704, 0x4718, 0x472C]
GLOBAL_TEXT = ['.wad*:', 'Online_', 'Easy_', 'coaster', '*', '*.emt', '.emt',
               'Rides', 'Features', 'Upgrades', 'Shops', 'Sideshow']
VP, I32, U32 = C.c_void_p, C.c_int32, C.c_uint32
STUBS = None
VT = {}


def w32(e, a, v):
    e.write(a, struct.pack('>I', v & 0xFFFFFFFF))


def wh(e, a, v):
    e.write(a, struct.pack('>H', v & 0xFFFF))


def F(lib, name, result, *args):
    fn = lib[name]
    fn.restype = result
    fn.argtypes = list(args)
    return fn


def short_text(e, a):
    return '(null)' if not a else e.cstr(a)[:159]


def fill_info(e, info, storage, entry):
    name, directory, wad = e.case['entries'][storage][entry]
    w32(e, info, wad)
    w32(e, info + 4, directory)
    raw = ('pre/' + name).encode('latin-1') + b'\0'
    e.write(info + 0x24, raw[:260])
    w32(e, info + 0x20, info + 0x28)


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    S = CL.Stubs()
    S.real.update(ADDR + [0x10156F80, 0x10156FCC, 0x1004595C, 0x100459AC])
    string_stubs(S)
    # String views have the same five-word fake layout as the strings.
    S.imp('Length__15TbStringBase<c>CFv', lambda e: e.set_r(3, e.u32(e.r(3) + 12)))
    def view(e, right):
        text = s_read(e, e.r(4), False)
        count = e.r(5)
        result = text if count >= len(text) else text[-count:] if right and count else text[:count]
        s_write(e, e.r(3), result, False)
    S.imp('Right__15TbStringBase<c>CFUi', lambda e: view(e, True))
    S.imp('Left__15TbStringBase<c>CFUi', lambda e: view(e, False))
    S.imp('__as__26TbDynamicStringTemplate<c>FPCc', lambda e: s_write(e, e.r(3), e.cstr(e.r(4)), False))
    def file_ctor(e):
        e.cl.newlab(e.r(3))
        e.write(e.r(3), struct.pack('>II', VT['file'], 0))
    S.imp('__ct__Q33bfl10FileSystem5CFileFv', file_ctor)
    S.imp('__dt__Q33bfl10FileSystem5CFileFv', lambda e: None)
    S.imp('Close__Q33bfl10FileSystem5CFileFv', CL.recorder('Close', 'L'))
    def alloc_object(e):
        a = e.alloc(e.r(3), bytes([0xA5]) * e.r(3), alignment=8)
        e.set_r(3, a)
    S.imp('__nw__FUl', alloc_object)
    S.imp('__dl__FPv', CL.recorder('delete', 'L'))
    S.internal(0x1000DDE8, CL.recorder('Assert', 'is'))
    S.internal(0x10156E34, CL.recorder('DeleteEffects', 'L'))
    def constructor(e):
        obj, info, path = e.r(3), e.r(4), e.r(5)
        label = e.cl.newlab(obj)
        name = e.cstr(e.u32(info + 0x20))
        text = s_read(e, path, False)
        e.cl.rec('ItemCtor', (label,), (name[:159], text[:159]))
        s_write(e, obj, name, False)
        s_write(e, obj + 0x14, text, False)
        w32(e, obj + 0x28, VT['storage'])
        w32(e, obj + 0x30, VT['item'])
        e.cl.labels[obj + 0x28] = label + 1000
        e.storage_index[obj + 0x28] = 0
    S.internal(0x101612CC, constructor)
    for address, name in [(0x10163D88, 'GetSamFile'), (0x10163E98, 'GetEasySamFile'),
                          (0x10163FDC, 'GetOnlineSamFile'), (0x10164120, 'GetCoasterSamFile'),
                          (0x10164238, 'GetTextFile')]:
        S.internal(address, CL.recorder(name, 'LL', 'i'))
    S.internal(0x10164374, CL.recorder('HasSounds', 'L', 'i'))
    S.internal(0x10164574, CL.recorder('HasParticles', 'L', 'i'))
    S.internal(0x10162D1C, CL.recorder('ListSam', 'LLL', 'i'))
    S.internal(0x10162E24, CL.recorder('ListOnline', 'LLL', 'i'))
    S.internal(0x10163924, CL.recorder('FindItem', 'LT', 'i'))
    def register(e):
        e.cl.rec('RegisterItem', (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
        w32(e, e.r(4) + 0x34, e.r(3))
        e.set_r(3, T.u32(e.cl.ret('RegisterItem', 1)))
    S.internal(0x10163494, register)
    def find(e):
        e.cl.rec('FindResource', (e.cl.lab(e.r(3)),), (s_read(e, e.r(4), False)[:159],))
        result = e.cl.ret('FindResource')
        if result == 1:
            w32(e, e.r(5), e.objects[2])
        if e.cl.ret('manager hook'):
            e.manager_calls += 1
            w32(e, GAME + 0x34, e.objects[10 + (e.manager_calls & 1)])
        e.set_r(3, T.u32(result))
    S.internal(0x10165448, find)
    S.internal(0x10004764, lambda e: (e.cl.rec('IsDir', (), (e.cstr(e.u32(e.r(3) + 0x20))[:159],)), e.set_r(3, int(e.u32(e.r(3) + 4) != 0))))
    S.internal(0x10004AB8, lambda e: (e.cl.rec('IsWad', (), (e.cstr(e.u32(e.r(3) + 0x20))[:159],)), e.set_r(3, int(e.u32(e.r(3)) != 0))))
    def separator(e):
        text = s_read(e, e.r(3), False)
        e.cl.rec('AddSeparator', (), (text[:159],))
        s_write(e, e.r(3), text + ':', False)
    S.internal(0x10004BC0, separator)
    def eq(e):
        a, b = s_read(e, e.r(3), False), s_read(e, e.r(4), False)
        e.cl.rec('StrEq', (), (a[:159], b[:159]))
        e.set_r(3, e.cl.ret('StrEq', int(a == b)))
    S.internal(0x100053F4, eq)
    S.internal(0x1014B62C, CL.recorder('SetError', 'Li us'.replace(' ', '')))
    # Init supplies deterministic fake balance slots and labels the newly allocated record.
    def init(e):
        table = e.r(3)
        coaster = e.u32(table + 4) == 0x1029CD7C
        if coaster:
            e.cl.labels[table] = 50
        else:
            ride = table - 4
            label = e.cl.newlab(ride)
            e.cl.labels[table] = label + 1000
            e.latest = ride
        e.cl.rec('Init', (e.cl.lab(table),))
        w32(e, table, 0)
        if not coaster:
            for slot, value in enumerate(e.case['slots']):
                w32(e, table + 8 + 4 * slot, value)
            w32(e, table + 8 + 4 * 3, e.objects[16])
            w32(e, table + 8 + 4 * 4, 0)
            for i in range(8):
                w32(e, table + 8 + 4 * (142 + i), e.subs[i])
    S.internal(0x1001EA28, init)
    S.internal(0x1001ED40, CL.recorder('UnInit', 'L'))
    S.internal(0x100202C0, CL.recorder('Load', 'LsLi', lambda e, L: L.ret('Load', 1)))
    def get_name(e):
        e.cl.rec('GetName', (e.cl.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(6))))
        text = b''.join(struct.pack('>H', ord(ch)) for ch in 'built name\0')
        e.write(e.r(5), text)
        e.set_r(3, 1)
    S.internal(0x101570C4, get_name)
    def load_type(e):
        extra = [e.u32(e.r(1) + off) for off in [0x38, 0x3C, 0x40]]
        ints = (e.cl.lab(e.r(5)), e.cl.lab(e.r(6)), e.r(7), e.r(8), e.r(9),
                e.cl.lab(e.r(10)), e.cl.lab(extra[0]), T.s32(extra[1]), T.s32(extra[2]))
        e.cl.rec('LoadRideType', ints, (short_text(e, e.r(3)), short_text(e, e.r(4))))
        result = e.cl.ret('LoadRideType', 5)
        if e.latest and e.r(7) == 0xC0:
            hook = e.cl.ret('sub hook', -1)
            if hook >= 0:
                w32(e, e.latest + 0x7B4, hook)
        e.set_r(3, T.u32(result))
    S.internal(0x10068FD0, load_type)
    S.internal(0x100DC6AC, CL.recorder('RegisterSound', 'ss', 'i'))
    S.internal(0x100DC89C, CL.recorder('FreeSound', 'i'))
    S.internal(0x100BBC54, CL.recorder('LoadEmitter', 'L', 'i'))
    S.internal(0x100BBF78, CL.recorder('KillEmitter', 'u'))
    def mem_alloc(e):
        size = e.r(3)
        e.cl.rec('Alloc', (size,))
        block = e.alloc(min(size, 8192) + 16) if e.cl.ret('Alloc', 1) else 0
        if block:
            e.cl.newlab(block)
        e.set_r(3, block)
    S.imp('LbMem_Alloc__FUi', mem_alloc)
    S.imp('LbMem_Free__FPv', CL.recorder('Free', 'L'))
    S.imp('_USwapBlock16__13SamsUtilitiesFPUs l'.replace(' ', ''), CL.recorder('Swap16', 'Li'))
    S.internal(0x10155604, CL.recorder('FindString', 'S', lambda e, L: L.ret('FindString', -1)))
    S.internal(0x101556A0, CL.recorder('FindLine', 'S', lambda e, L: e.r(3) + 2 * L.ret('FindLine', 1)))
    def copy(e):
        e.cl.rec('CopyLine', (e.cl.lab(e.r(3)),), (CL.wide_text(e, e.r(4)),))
        w32(e, e.r(3), e.objects[20 + (e.cl.ret('CopyLine') & 3)])
    S.internal(0x10155170, copy)
    S.internal(0x1006FCBC, CL.recorder('Trace', 'usi'))
    def coaster_data(e):
        e.cl.rec('CoasterData', (e.r(3), e.cl.lab(e.r(6))),
                 (short_text(e, e.r(4)), short_text(e, e.r(5))))
        e.set_r(3, e.objects[12])
    S.internal(0x10039A28, coaster_data)
    S.internal(0x10036564, CL.recorder('TrackMesh', 'L'))
    S.internal(0x1003A150, CL.recorder('Train', 'Ls'))
    S.internal(0x1003A9A8, CL.recorder('Pylons', 'LLLs'))
    # Functional extension fake leaves the actual final-character indexing in LoadParticles.
    def extension(e):
        ext, name = s_read(e, e.r(3), False), s_read(e, e.r(4), False)
        e.cl.rec('RemoveExtension', (), (ext[:159], name[:159]))
        result = e.alloc(20)
        s_write(e, result, name[:-len(ext)] if ext and name.lower().endswith(ext.lower()) else name, False)
        e.set_r(3, result)
    S.internal(0x10005524, extension)
    S.imp('atol', lambda e: e.set_r(3, int(e.cstr(e.r(3))) if e.cstr(e.r(3)).isdigit() else 0))
    def swprintf(e):
        text = e.cstr(e.r(6))[:e.r(4) - 1]
        e.write(e.r(3), b''.join(struct.pack('>H', ord(ch)) for ch in text) + b'\0\0')
        e.set_r(3, len(text))
    S.imp('swprintf', swprintf)
    def strncpy(e):
        source, count = wide_read(e, e.r(4)), e.r(5)
        text = source[:count].ljust(count, '\0')
        e.write(e.r(3), b''.join(struct.pack('>H', ord(ch)) for ch in text))
    S.imp('wcsncpy', strncpy)
    S.internal(0x1016F5D8, lambda e: w32(e, e.r(3), e.case['type']))
    def first(e):
        obj, info = e.r(4), e.r(6)
        index = e.storage_index[obj]
        e.cl.rec('FindFirst', (e.cl.lab(obj),), (short_text(e, e.r(5)),))
        e.next[index] = 1
        result = e.cl.ret('FindFirst', 0 if e.case['entries'][index] else -1)
        if result >= 0:
            fill_info(e, info, index, 0)
        w32(e, e.r(3), result)
    def next_entry(e):
        obj, info = e.r(4), e.r(5)
        index = e.storage_index[obj]
        e.cl.rec('FindNext', (e.cl.lab(obj),))
        nxt = e.next[index]
        e.next[index] += 1
        result = e.cl.ret('FindNext', 0) if nxt < len(e.case['entries'][index]) else -1
        if result >= 0:
            fill_info(e, info, index, nxt)
        w32(e, e.r(3), result)
    def root(e):
        e.cl.rec('GetRootPath', (e.cl.lab(e.r(3)),))
        e.set_r(3, 0 if e.cl.ret('null root') else e.root)
    def open_file(e):
        e.cl.rec('OpenFile', (e.cl.lab(e.r(4)), e.cl.lab(e.r(5)), e.r(7), e.cl.lab(e.r(8))),
                 (short_text(e, e.r(6)),))
        w32(e, e.r(3), e.cl.ret('OpenFile'))
    def read(e):
        size = e.r(5)
        e.cl.rec('Read', (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), size))
        if e.r(4):
            e.write(e.r(4), e.case['file_be'][:min(size, 4096)])
        e.set_r(3, T.u32(e.cl.ret('Read', e.case['text_bytes'])))
    def get_size(e):
        e.cl.rec('GetSize', (e.cl.lab(e.r(3)),))
        e.set_r(3, T.u32(e.cl.ret('GetSize', e.case['text_bytes'])))
    def item_dt(e):
        e.cl.rec('ItemDtor', (e.cl.lab(e.r(3)), e.r(4)))
        e.cl.rec('delete', (e.cl.lab(e.r(3)),))
        e.cl.forget(e.r(3))
    def vtable(slots):
        obj = T.persist_alloc(e, 0x40)
        for off, fn in slots.items():
            w32(e, obj + off, T.fake_function(e, fn))
        return obj
    VT['storage'] = vtable({0xC: first, 0x10: next_entry, 0x14: open_file, 0x20: root})
    VT['file'] = vtable({0x10: read, 0x1C: get_size})
    VT['item'] = vtable({8: item_dt})
    STUBS = S


def ppc_begin(e, c):
    e.case = c
    e.latest = 0
    e.manager_calls = 0
    e.next = [0, 0, 0]
    e.objects = {}
    sizes = {0: 0x738, 1: 0x808, 2: 0x50, 3: 0x38, 4: 8, 5: 8, 6: 8,
             10: 12, 11: 12, 12: 0x320, 15: 0x3B80, 16: 0xC88}
    for key, size in sizes.items():
        e.objects[key] = e.alloc(size, bytes([0xA5]) * size, alignment=8)
    o = e.objects
    o[7], o[8], o[9] = o[3] + 0x28, o[0] + 0x52C, ERROR
    o[13], o[14], o[24], o[25] = o[12] + 0xA8, o[12] + 0x1C8, MISSING, o[1] + 4
    for i in range(4):
        o[20 + i] = e.alloc(256)
        o[1000 + i] = o[1] + 0x7F0 + 4 * i
    fixed = {a: key + 1 for key, a in o.items() if key < 20}
    fixed[MISSING] = 99
    fixed[o[25]] = 1002
    for i in range(4):
        fixed[o[20 + i]] = 20 + i
        fixed[o[1000 + i]] = 70 + i
    e.cl = CL.PpcLog(fixed, c['q'])
    db = o[0]
    e.write(db, b'\0' * 0x528)
    w32(e, db + 0x528, c['db_count'])
    wh(e, db + 0x730, c['cached'])
    w32(e, db + 0x734, 0)
    existing_name = CL.new_wide(e, c['existing_name'])
    for i, word in enumerate(c['ids']):
        record = e.alloc(0x808, bytes([0xA5]) * 0x808)
        o[30 + i], o[2000 + i] = record, record + 4
        e.cl.labels[record] = 3000 + i
        e.cl.labels[record + 4] = 2000 + i
        wh(e, record, word)
        e.write(record + 0x7D8, b'\0' * 24)
        e.write(record + 0x7F0, b'\0' * 16)
        w32(e, record + 0x7F0, MISSING if word & 0x10000 else 0 if word & 0x20000 else existing_name)
        w32(e, db + 4 * i, record)
    if c['cache_index'] >= 0:
        w32(e, db + 0x734, o[30 + c['cache_index']])
    e.storage_index = {o[4 + i]: i for i in range(3)}
    e.storage_index[o[7]] = 0
    for obj in list(e.storage_index):
        w32(e, obj, VT['storage'])
    storage_list = e.alloc(16)
    head = 0
    for i in reversed(range(max(0, c['storages']))):
        node = e.alloc(12)
        e.write(node, struct.pack('>III', o[4 + i], head, 0))
        head = node
    w32(e, storage_list + 4, head)
    w32(e, o[2] + 0x2C, 0 if c['storages'] < 0 else storage_list)
    s_write(e, o[2], 'Rides', False)
    s_write(e, o[2] + 0x14, c['path'], False)
    s_write(e, o[3], c['name'], False)
    s_write(e, o[3] + 0x14, c['path'], False)
    for off, text in zip(GLOBAL_OFF, c['globals']):
        s_write(e, G + off, text, False)
    e.root = CL.new_text(e, c['root'])
    w32(e, GAME + 0x34, o[10])
    w32(e, 0x102B3AA0, c['type'])
    e.write(0x102B3AA4, bytes([c['guard']]))
    ride = o[1]
    for slot, value in enumerate(c['slots']):
        w32(e, ride + 0xC + 4 * slot, value)
    w32(e, ride + 4, 0)
    w32(e, ride + 0x18, o[16])
    e.write(o[16], struct.pack('>ii', c['width'], c['height']) + c['shape_be'])
    e.subs = [CL.new_text(e, 'mesh' + str(i)) for i in range(8)]
    for i, value in enumerate(c['fields']):
        w32(e, ride + (0x800 if i == 0 else 0x7D8 + 4 * (i - 1)), value)
    for i, value in enumerate(c['strings']):
        w32(e, ride + 0x7F0 + 4 * i, MISSING if value == -1 else o[19 + value] if value else 0)
    texture = e.alloc(0x20)
    wh(e, texture + 4, c['coaster'][4])
    w32(e, o[12] + 0x1C, texture)
    for off, value in zip([0x3C, 0x2E8, 0x2F8], [c['coaster'][3], c['coaster'][5], c['coaster'][6]]):
        w32(e, o[12] + off, value)
    w32(e, 0x102B6988, c['coaster'][1])
    w32(e, 0x102B6984, c['coaster'][2])
    e.set_obj = e.alloc(0x100)
    w32(e, e.set_obj + 4, c['coaster'][0])
    # Only real coaster calls index these pointers; generated cases keep their type in range.
    for type_number in range(330):
        w32(e, 0x102D19B0 + type_number * 4, e.set_obj)
    return o


def ppc_dump(e, c, result):
    o, L = e.objects, e.cl
    db = o[0]
    record = e.latest if c['kind'] <= 2 else o[1]
    present = bool(record) and c['kind'] != 9
    out = [e.u32(db + 0x528), struct.unpack('>H', e.read(db + 0x730, 2))[0],
           L.lab(e.u32(db + 0x734)) & 0xFFFFFFFF, e.u32(db + 0x72C), e.u32(e.set_obj + 4),
           e.u32(0x102B6988), e.u32(0x102B6984), e.u32(o[12] + 0x318), int(present)]
    text = b''
    if present:
        out.append(struct.unpack('>H', e.read(record, 2))[0])
        for i in range(160):
            value = e.u32(record + 0xC + i * 4)
            out.append((L.lab(value) & 0xFFFFFFFF) if i in [3, 4] or 142 <= i < 150 else value)
        out.extend(e.u32(record + off) for off in [0x78C, 0x790, 0x794])
        # stb writes the first byte on PPC and host. Convert these as bytes, not as a native word.
        out.append(int.from_bytes(e.read(record + 0x798, 4), 'little'))
        out.extend(e.u32(record + off) for off in [0x79C, 0x7A0])
        out.append(int.from_bytes(e.read(record + 0x7A4, 4), 'little'))
        out.extend(e.u32(record + off) for off in range(0x7A8, 0x7F0, 4))
        for i in range(4):
            value = e.u32(record + 0x7F0 + i * 4)
            label = L.lab(value)
            out.append(value if label < 0 else label)
        out.extend([e.u32(record + 0x800), e.u32(record + 0x804)])
        narrow = e.read(record + 0x28C, 1024)
        wide = e.read(record + 0x68C, 256)
        text = narrow + b''.join(wide[i:i + 2][::-1] for i in range(0, 256, 2))
    return dict(result=result, state=out, text=text.hex(), log=L.log,
                db_slots=[L.lab(e.u32(db + 4 * i)) for i in range(330)])


def ppc_run(e, c):
    saved = STUBS.activate(e)
    try:
        o = ppc_begin(e, c)
        kind = c['kind']
        if kind == 0:
            args = [o[0], o[2], o[4], o[3]]
        elif kind == 1:
            args = [o[0], c['argument'] & 0xFFFFFFFF]
        elif kind == 2:
            args = [o[0], o[2]]
        elif kind == 3:
            ref = e.alloc(4, struct.pack('>I', o[1]))
            args = [o[0], ref]
        elif kind == 4:
            file = e.alloc(8, struct.pack('>II', VT['file'], 0))
            e.cl.newlab(file)
            args = [o[1], file]
        elif kind in [5, 6]:
            args = [o[1], o[4]]
        elif kind in [7, 8]:
            args = [o[1]]
        elif kind == 9:
            args = [o[1], c['argument'] & 0xFFFFFFFF]
        else:
            args = [c['argument'], CL.new_text(e, c['path']), o[15]]
        result = T.call_ppc(e, ADDR[kind], args)[0]
        return ppc_dump(e, c, T.s32(result) if kind <= 2 else 0)
    finally:
        CL.Stubs.deactivate(e, saved)


def host_run(lib, c):
    C.c_int32.in_dll(lib, 'tpw_cl_on').value = 0
    addr = lambda key: F(lib, 'tpw_rd_addr', VP, I32)(key)
    F(lib, 'tpw_rd_reset', None, I32, I32, C.c_char_p, C.c_char_p, C.c_char_p)(
        c['kind'], c['type'], c['name'].encode(), c['path'].encode(), c['root'].encode())
    ids = (U32 * len(c['ids']))(*c['ids'])
    name = (C.c_uint16 * (len(c['existing_name']) + 1))(*map(ord, c['existing_name']), 0)
    F(lib, 'tpw_rd_database', None, I32, I32, I32, VP, VP)(c['db_count'], c['cached'], c['cache_index'], ids, name)
    for i, text in enumerate(c['globals']):
        F(lib, 'tpw_rd_global', None, I32, C.c_char_p)(i, text.encode())
    for i, value in enumerate(c['slots']):
        F(lib, 'tpw_rd_slot', None, I32, U32)(i, value)
    shape = (C.c_uint8 * len(c['shape_le'])).from_buffer_copy(c['shape_le'])
    F(lib, 'tpw_rd_shape', None, I32, I32, VP)(c['width'], c['height'], shape)
    fields, strings = (U32 * 7)(*c['fields']), (I32 * 4)(*c['strings'])
    F(lib, 'tpw_rd_fields', None, VP, VP)(fields, strings)
    F(lib, 'tpw_rd_storage', None, I32)(c['storages'])
    for storage, entries in enumerate(c['entries']):
        for name_text, directory, wad in entries:
            F(lib, 'tpw_rd_entry', None, I32, C.c_char_p, I32, I32)(storage, name_text.encode(), directory, wad)
    file = (C.c_uint8 * len(c['file_le'])).from_buffer_copy(c['file_le'])
    F(lib, 'tpw_rd_text', None, VP, I32)(file, c['text_bytes'])
    F(lib, 'tpw_rd_coaster', None, U32, U32, U32, I32, C.c_uint16, I32, I32)(*c['coaster'])
    fixed = {key + 1: addr(key) for key in range(17)}
    fixed[99] = addr(24)
    fixed[1002] = addr(25)
    for i in range(4):
        fixed[20 + i] = addr(20 + i)
        fixed[70 + i] = addr(1000 + i)
    for i in range(len(c['ids'])):
        fixed[3000 + i] = addr(30 + i)
        fixed[2000 + i] = addr(2000 + i)
    CL.host_begin(lib, fixed, c['q'])
    result = F(lib, 'tpw_rd_call', I32, I32, I32)(c['kind'], T.s32(c['argument']))
    log = CL.host_end(lib)
    words, text = (U32 * 256)(), (C.c_uint8 * 1280)()
    n = F(lib, 'tpw_rd_dump', I32, VP, VP)(words, text)
    db_slots = (I32 * 330)()
    F(lib, 'tpw_rd_slots', None, VP)(db_slots)
    return dict(result=result, state=list(words[:n]), text=bytes(text).hex() if words[8] else '', log=log,
                db_slots=list(db_slots))


def base(kind, r):
    slots = [0xA5A5A5A5] * 160
    for slot, value in {1: 41, 7: 11, 8: 13, 9: -5, 10: 7, 11: 0, 16: 3, 30: 0,
                        36: 0, 89: 2, 150: 0}.items():
        slots[slot] = value & 0xFFFFFFFF
    shape = [(0, 0xA5, b'\xA5' * 3)] * 400
    raw_be = b''.join(struct.pack('>iB3s', *cell) for cell in shape)
    raw_le = b''.join(struct.pack('<iB3s', *cell) for cell in shape)
    return dict(kind=kind, type=0, argument=1, name='ride', path='folder:', root='root',
                globals=list(GLOBAL_TEXT), db_count=0, cached=0, cache_index=-1, ids=[], existing_name='old name',
                storages=0, entries=[[], [], []], slots=slots, width=0, height=0,
                shape_be=raw_be, shape_le=raw_le, fields=[0xA5A5A5A5] * 7, strings=[0, -1, 1, 2],
                text_bytes=0, file_be=b'\0' * 4096, file_le=b'\0' * 4096, guard=1,
                coaster=[0, 0, 0, 0, 0, 0, 0], q={})


def make_case(kind, r, edge=None):
    c = base(kind, r)
    c['type'] = r.choice([0, 1, 2, -1, 3])
    c['guard'] = r.choice([0, 1, 0x80, 0xFF])
    c['name'] = r.choice(['ride', 'a', 'n' * 63, 'n' * 64])
    c['path'] = r.choice(['folder:', '', 'x.wad*:', '.wad*:', 'x', 'a' * 200])
    c['root'] = r.choice(['root', '', 'directory', 'a:b'])
    c['fields'] = [r.getrandbits(32) for _ in range(7)]
    c['strings'] = [r.choice([0, -1, 1, 2, 3, 4]) for _ in range(4)]
    c['slots'][1] = r.choice([0, 1, 41, 0xFFFF, 0x12345, r.getrandbits(32)])
    c['slots'][9] = r.getrandbits(32)
    c['slots'][10] = r.getrandbits(32)
    c['slots'][16] = r.getrandbits(32)
    c['slots'][11] = r.choice([0, 0, 1, 0xFFFFFFFF, 2])
    c['slots'][30] = r.choice([0, 1, 0xFFFFFFFF])
    c['slots'][36] = r.choice([0, 2, 3, 4, 0xFFFFFFFF])
    c['slots'][89] = r.getrandbits(32)
    c['slots'][150] = r.choice([0, 1, 8, 0xFFFFFFFF])
    if kind in [0, 1, 2]:
        count = r.randrange(6)
        c['ids'] = [r.choice([c['slots'][1] & 0xFFFF, 0, 41]) | r.choice([0, 0x10000, 0x20000]) for _ in range(count)]
        c['db_count'] = count
        c['existing_name'] = r.choice(['old name', '', 'x' * 70])
        c['cached'] = r.choice([0, c['slots'][1] & 0xFFFF, 17])
        c['cache_index'] = r.randrange(-1, count) if count else -1
        q = c['q']
        for key in ['ListSam', 'ListOnline', 'GetSamFile', 'GetOnlineSamFile', 'GetEasySamFile',
                    'GetCoasterSamFile', 'GetTextFile', 'HasSounds', 'HasParticles']:
            q[key] = [r.choice([0, 1, 2, -1])]
        q['Load'] = [r.choice([0, 1, 2, -1]) for _ in range(8)]
        q['LoadRideType'] = [r.choice([0, 5, 7]) for _ in range(9)]
        if kind in [1, 2]:
            c['storages'] = r.choice([-1, 0, 1, 2, 3])
            for storage in range(max(0, c['storages'])):
                c['entries'][storage] = [('entry' + str(i), r.randrange(2), r.randrange(2)) for i in range(r.randrange(5))]
            q['FindItem'] = [r.choice([0, 1, 2, -1]) for _ in range(12)]
            q['FindResource'] = [r.choice([0, 1, 2, -1]) for _ in range(5)]
            q['manager hook'] = [r.randrange(2)]
            q['HasParticles'] = [0]
            q['HasSounds'] = [0]
        if edge == 0:
            c['db_count'] = 330
            c['ids'] = [41 | 0x20000] * 330
        if edge == 1 and kind == 1:
            c['db_count'] = -0x80000000
            c['ids'] = []
            c['cache_index'] = -1
    if kind == 3:
        c['width'], c['height'] = r.choice([0, 1, 6, 20, -1]), r.choice([0, 1, 6, 20, -1])
        cells = [(r.choice([0, 8, 9, 10, 11, -1, 0x109]), r.randrange(256), r.randbytes(3)) for _ in range(400)]
        if edge == 0:
            c['width'], c['height'] = 6, 6
            cells = [(0, 0, b'\xA5' * 3)] * 400
            cells[3] = (9, 64, b'\xA5' * 3)
            cells[5 * 20] = (9, 16, b'\xA5' * 3)
            cells[4 * 20 + 1] = (10, 1, b'\xA5' * 3)
        elif edge == 1:
            cells = [(9, 255, b'\x12\x34\x56')] * 400
            c['width'], c['height'] = 20, 20
        elif edge == 2:
            cells = [(10, 255, b'\x12\x34\x56')] * 400
            c['width'], c['height'] = 20, 20
        c['shape_be'] = b''.join(struct.pack('>iB3s', *cell) for cell in cells)
        c['shape_le'] = b''.join(struct.pack('<iB3s', *cell) for cell in cells)
    if kind == 4:
        text = r.choice(['\ufeff', '\ufffe', '']) + r.choice(['', 'A\nB\nC\n', 'unknown', 'NAME\r\nx\r\n'])
        be, le = text.encode('utf-16-be'), text.encode('utf-16-le')
        c['text_bytes'] = len(be)
        c['file_be'], c['file_le'] = be.ljust(4096, b'\0'), le.ljust(4096, b'\0')
        count = r.randrange(5)
        c['q']['FindString'] = [r.randrange(3) for _ in range(count)] + [-1]
        c['q']['FindLine'] = [r.choice([0, 1, 2])]  # finite FindString queue terminates even if this does not advance
        c['q']['CopyLine'] = [r.randrange(4) for _ in range(5)]
        c['q']['Read'] = [r.choice([0, -1, c['text_bytes'], 1])]
        c['q']['Alloc'] = [r.choice([0, 1, 1])]
        if edge is not None:
            c['q']['GetSize'] = [[0, 1, 2, 3, -1, -2][edge % 6]]
            if edge % 6 == 5:
                c['q']['Alloc'] = [0]  # negative even size only exercises Read(NULL), not a pre-buffer terminator store
    if kind in [5, 6]:
        c['q']['RegisterSound'] = [r.choice([0, -1, 7, -0x80000000, 0x7FFFFFFF])]
        if kind == 5:
            c['q']['null root'] = [r.randrange(2)]
        else:
            names = ['x' + str(i) + '.emt' for i in range(10)] + ['x.emt', 'x23.EMT']
            c['entries'][0] = [(r.choice(names), 0, 0) for _ in range(r.randrange(7))]
            c['q']['OpenFile'] = [r.choice([0, 1, -1, -0x80000000]) for _ in range(6)]
            c['q']['LoadEmitter'] = [r.getrandbits(32) for _ in range(6)]
            if edge is not None:
                c['entries'][0] = [(names[edge % len(names)], 0, 0)]
                c['q']['OpenFile'] = [0]
    if kind == 9:
        c['argument'] = r.choice([0, 1, 2, 0x10000, 0x8000, -1])
        if edge is not None:
            c['fields'][1:] = [0] * 6
    if kind == 10:
        c['argument'] = r.choice([1, 5, 329])
        c['coaster'] = [r.getrandbits(32), r.getrandbits(32), r.choice([0, 3, 0xFFFF, 0xFFFFFFFF]),
                        T.s32(r.getrandbits(32)), r.choice([0, 3, 0xFFFF]),
                        r.choice([0, 1, -1, 0x10000, -0x80000000]), r.choice([0, 1, -1, 0x10000, 0x7FFFFFFF])]
        if edge is not None:
            n = [0, 1, -1, -0x80000000, 0x7FFFFFFF][edge % 5]
            c['coaster'][1] = n & 0xFFFFFFFF
            c['coaster'][3] = n
            c['coaster'][2] = c['coaster'][4]
    return c


def edges(kind):
    r = random.Random(1190 + kind)
    cases = [make_case(kind, r, i) for i in range(12)]
    if kind == 0:
        # Every parse failure and mesh branch, with all file queries otherwise succeeding.
        for game_type in [0, 1, 2]:
            for failure in range(-1, 6):
                c = base(kind, r)
                c['type'] = game_type
                c['slots'][36] = 3
                c['slots'][150] = 8
                c['slots'][1] = 0xFFFF
                c['q'] = {key: [1] for key in ['ListSam', 'ListOnline', 'GetSamFile', 'GetOnlineSamFile',
                          'GetEasySamFile', 'GetCoasterSamFile', 'GetTextFile', 'HasSounds', 'HasParticles']}
                c['q']['Load'] = [0 if i == failure else 1 for i in range(6)]
                c['q']['LoadRideType'] = [5, 0, 7]
                c['q']['sub hook'] = [2]
                cases.append(c)
        for offset in [0, 1, -1, 2]:
            for head in [0, 1, -1]:
                c = base(kind, r)
                c['slots'][11] = offset & 0xFFFFFFFF
                c['slots'][30] = head & 0xFFFFFFFF
                c['slots'][36] = 3
                c['slots'][150] = 8
                c['q']['LoadRideType'] = [5, 0xFFFFFFFF]
                cases.append(c)
        c = base(kind, r)
        c['db_count'] = 329
        c['ids'] = [0x20029] * 329
        c['q']['LoadRideType'] = [0]
        cases.append(c)
    if kind in [1, 2]:
        # Scan queries test exactly 1, while FindItem tests exactly 0. Easy duplicates retain the item.
        for game_type in [0, 2]:
            for sam in [0, 1, 2, -1]:
                for duplicate in [0, 1, 2, -1]:
                    c = base(kind, r)
                    c['type'] = game_type
                    c['storages'] = 1
                    c['entries'][0] = [('one.wad', 0, 1), ('directory', 1, 1)]
                    c['q'] = {'GetSamFile': [sam], 'GetEasySamFile': [sam], 'FindItem': [duplicate],
                              'FindResource': [1, 0, 2, -1, 1], 'LoadRideType': [0]}
                    cases.append(c)
        c = base(kind, r)
        c['storages'] = 3
        c['entries'] = [[('a', 1, 0)], [('b', 0, 1)], [('c', 1, 1)]]
        c['q'] = {'FindFirst': [-0x80000000, 5, -1], 'GetSamFile': [1], 'FindItem': [0],
                  'ListSam': [1], 'Load': [0], 'FindResource': [1]}
        cases.append(c)
    return cases


TESTS = [FnTest(name, ppc_run, host_run, edge=lambda kind=kind: edges(kind),
                rnd=lambda r, kind=kind: make_case(kind, r)) for kind, name in enumerate(NAMES)]


class Description(C.Structure):
    _fields_ = [('type', I32), ('name', C.c_char * 32), ('min_int', I32), ('max_int', I32),
                ('min_float', U32), ('max_float', U32), ('count', I32), ('flags', U32)]


def table_ppc(e, c):
    which, row = c
    base_address = 0x1028EC14 if which else 0x10299854
    description = e.read(base_address + row * 60, 60)
    ints = struct.unpack('>i32siiIIiI', description)
    obj = e.alloc(16)
    saved = STUBS.activate(e)
    try:
        pointer = T.call_ppc(e, 0x1004595C if which else 0x10156F80, [obj, row])[0]
        start = T.call_ppc(e, 0x100459AC if which else 0x10156FCC, [obj])[0]
    finally:
        CL.Stubs.deactivate(e, saved)
    return dict(fields=list(ints[:1]) + [ints[1].split(b'\0')[0].decode()] + list(ints[2:]),
                row_pointer=pointer == base_address + row * 60, values=start == obj + 8)


def table_host(lib, c):
    which, row = c
    pointer = F(lib, 'tpw_rd_description', C.POINTER(Description), I32, I32)(which, row)
    d = pointer.contents
    return dict(fields=[d.type, d.name.decode(), d.min_int, d.max_int, d.min_float, d.max_float, d.count, d.flags],
                row_pointer=True, values=bool(F(lib, 'tpw_rd_values', I32, I32)(which)))


TESTS.append(FnTest('CRideBalance::GetBalanceDescription', table_ppc, table_host,
                    edge=lambda: [(0, row) for row in range(132)], rnd=lambda r: (0, r.randrange(132))))
TESTS.append(FnTest('CCoasterBalance::GetBalanceDescription', table_ppc, table_host,
                    edge=lambda: [(1, row) for row in range(384)], rnd=lambda r: (1, r.randrange(384))))


def values_ppc(e, c):
    obj = e.alloc(32)
    saved = STUBS.activate(e)
    try:
        pointer = T.call_ppc(e, 0x100459AC if c else 0x10156FCC, [obj])[0]
        return dict(values=pointer == obj + 8)
    finally:
        CL.Stubs.deactivate(e, saved)


def values_host(lib, c):
    return dict(values=bool(F(lib, 'tpw_rd_values', I32, I32)(c)))


for which, name in enumerate(['CRideBalance', 'CCoasterBalance']):
    TESTS.append(FnTest(name + '::GetBalanceValuesStart', values_ppc, values_host,
                       edge=lambda which=which: [which], rnd=lambda r, which=which: which))
