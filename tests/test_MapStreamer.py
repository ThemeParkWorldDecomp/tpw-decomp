"""w70: the first sound-library unit. One emulator program, functional string imports,
logged storage/file/refcount fakes, and complete in-place record snapshots.

The unit library forces field swaps. Host file fakes reverse each numeric file field before
Read, so the host and PPC swaps both execute and leave the same numeric fields. Unswapped
bytes, string lengths and names keep the original file bytes. Real data stays outside the tree.
"""
import ctypes as C
import pathlib
import random
import struct

from unicorn import ppc_const as P
import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = 'sound symbols shared'
SHIM_LIBRARY = 'unit_MapStreamer'
ROOT = pathlib.Path(__file__).resolve().parents[2]
MASK = 0xffffffff
CTOR, INSTANCE, DTOR = 0x1001f784, 0x1001f6ec, 0x1002020c
SET_PATH, REG, QUAL, REMOVE, UNLOAD, FIND = 0x1002030c, 0x1002071c, 0x1002051c, 0x10022e78, 0x10022f1c, 0x100237a4
REG_BANK_STORAGE, REG_SFX_STORAGE = 0x10021cc4, 0x1002213c
SINGLETON, GUARD, MANAGER = 0x1003f364, 0x1003f3a4, 0x1003f3b4
SFX_GUID = bytes.fromhex('002c61e9d031d211b40900b0c993f203')
BANK_GUID = bytes.fromhex('012c61e9d031d211b40900a0c993f203')
# (size, swapped fields (offset, width), pointer field offsets)
SHAPES = [
    (16, [(0, 4), (4, 2), (6, 2)], []),
    (20, [(0, 4), (4, 2), (6, 2), (16, 4)], []),
    (8, [(0, 4), (4, 4)], [0]),
    (8, [(0, 4), (4, 4)], [0]),
    (24, [(0, 4), (4, 4), (8, 2), (10, 2), (12, 4), (16, 4), (20, 4)], [4]),
    (20, [(0, 4), (4, 4), (8, 4), (12, 4), (16, 2), (18, 2)], [8]),
    (42, [(0, 4), (4, 4), (8, 4), (16, 2), (18, 2), (22, 2), (24, 2), (26, 2), (28, 2), (30, 4), (34, 4), (38, 4)], [8, 38]),
    (16, [(0, 4), (4, 4), (8, 4), (12, 2), (14, 2)], []),
    (8, [(0, 4), (4, 2)], [0]),
]


def ws(e, a, values):
    e.write(a, struct.pack('>' + 'I' * len(values), *[x & MASK for x in values]))


def cstr(e, value):
    return 0 if value is None else e.alloc(len(value) + 1, value.encode('latin-1') + b'\0')


def string(e, obj):
    ptr, count = e.u32(obj + 8), e.u32(obj + 4)
    return e.read(ptr, count).decode('latin-1') if ptr else ''


def set_string(e, obj, value):
    raw = value.encode('latin-1')
    ptr = e.alloc(len(raw) + 1, raw + b'\0')
    ws(e, obj, [e.string_vt, len(raw), ptr, len(raw), 0])
    e.set_r(3, obj)


def make_vtable(e, slots):
    table = T.persist_alloc(e, 0x50)
    for slot, callback in slots.items():
        ws(e, table + slot, [T.fake_function(e, callback)])
    return table


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    lib = runner.lib
    lib.tpw_ms_op.argtypes = [C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32), C.c_char_p, C.c_char_p]
    lib.tpw_ms_object.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_ms_object.restype = C.c_void_p
    lib.tpw_ms_get.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_ms_get.restype = C.c_uint32
    lib.tpw_ms_text.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_ms_text.restype = C.c_char_p
    lib.tpw_ms_file.argtypes = [C.c_char_p, C.c_char_p, C.c_void_p, C.c_uint32]
    lib.tpw_ms_scalar.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_ms_scalar.restype = C.c_uint32
    lib.tpw_ms_swap.argtypes = [C.c_uint32, C.c_void_p, C.c_uint32]

    def assign_text(e):
        set_string(e, e.r(3), e.cstr(e.r(4)) if e.r(4) else '')

    def assign_base(e):
        set_string(e, e.r(3), string(e, e.r(4)))

    def append_text(e):
        set_string(e, e.r(3), string(e, e.r(3)) + e.cstr(e.r(4)))

    def append_base(e):
        set_string(e, e.r(3), string(e, e.r(3)) + string(e, e.r(4)))

    def append_char(e):
        ch = e.read(e.r(4), 1).decode('latin-1')
        set_string(e, e.r(3), string(e, e.r(3)) + (ch if ch != '\0' else ''))

    e.string_vt = make_vtable(e, {0x18: assign_base, 0x1c: assign_base, 0x20: assign_text,
                                  0x28: append_base, 0x2c: append_char, 0x30: append_text})
    e.stub('__ct__26TbDynamicStringTemplate<c>Fv', lambda e: set_string(e, e.r(3), ''))
    e.stub('__ct__26TbDynamicStringTemplate<c>FPCc', assign_text)
    for signature in ['__ct__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>',
                      '__as__26TbDynamicStringTemplate<c>FRC26TbDynamicStringTemplate<c>',
                      '__as__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>']:
        e.stub(signature, assign_base)
    e.stub('__as__26TbDynamicStringTemplate<c>FPCc', assign_text)
    e.stub('__apl__26TbDynamicStringTemplate<c>FPCc', append_text)
    e.stub('__apl__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>', append_base)
    e.stub('__apl__26TbDynamicStringTemplate<c>FRCc', append_char)
    e.stub('__dt__26TbDynamicStringTemplate<c>Fv', lambda e: None)
    e.stub('Length__15TbStringBase<c>CFv', lambda e: e.set_r(3, len(string(e, e.r(3)))))
    e.stub('__eq__15TbStringBase<c>CFRC15TbStringBase<c>', lambda e: e.set_r(3, string(e, e.r(3)) == string(e, e.r(4))))

    def substring(e, right):
        source, count = e.r(4), e.r(5)
        text = string(e, source)
        count = min(count, len(text))
        start = len(text) - count if right else 0
        data = e.u32(source + 8)
        ws(e, e.r(3), [e.string_vt, count, data + start if data else 0, 0])

    e.stub('Right__15TbStringBase<c>CFUi', lambda e: substring(e, True))
    e.stub('Left__15TbStringBase<c>CFUi', lambda e: substring(e, False))
    e.stub('FindReverse__15TbStringBase<c>CFPCcUi', lambda e: e.set_r(3, string(e, e.r(3)).rfind(e.cstr(e.r(4)))))
    e.stub('FindReverse__15TbStringBase<c>CFRCcUi', lambda e: e.set_r(3, string(e, e.r(3)).rfind(e.read(e.r(4), 1).decode('latin-1'))))
    e.stub('Find__15TbStringBase<c>CFPCcUi', lambda e: e.set_r(3, string(e, e.r(3)).find(e.cstr(e.r(4)), e.r(5))))
    e.stub('strcmp', lambda e: e.set_r(3, (e.cstr(e.r(3)) > e.cstr(e.r(4))) - (e.cstr(e.r(3)) < e.cstr(e.r(4)))))

    def alloc(e, array):
        size = e.r(3)
        link = e.uc.reg_read(P.UC_PPC_REG_LR)
        is_map = link in [0x1001f810, 0x10020dd4, 0x10021f88]
        element = not array and link == 0x1001f618
        scalar = element or (not array and size in [40, 44])
        if element and e.cl.ret('ElementAlloc', 1) == 0:
            e.cl.rec('new', (8, 0))
            e.set_r(3, 0)
            return
        if array and not is_map:
            fill = struct.pack('>I', getattr(e, 'array_fill', 0))
            data = (fill * ((max(size, 4) + 3) // 4))[:max(size, 4)]
        else:
            data = b'\xa5' * max(size, 4)
        ptr = e.alloc(max(size, 4), data)
        e.alloc_sizes[ptr] = size
        if is_map:
            e.cl.rec('MapAlloc', (size, e.cl.newlab(ptr)))
            e.map_blocks.add(ptr)
        elif scalar:
            e.cl.rec('new', (size, e.cl.newlab(ptr)))
        elif array and getattr(e, 'array_context', False):
            e.cl.rec('new[]', (size, e.cl.newlab(ptr)))
        e.set_r(3, ptr)

    def free(e, array):
        ptr = e.r(3)
        if ptr in e.map_blocks:
            e.cl.rec('MapFree', (e.cl.lab(ptr),))
            e.cl.forget(ptr)
            e.map_blocks.remove(ptr)
        if e.cl.lab(ptr) > 0:
            e.cl.rec('delete', (e.cl.lab(ptr),))
            e.cl.forget(ptr)

    e.stub('__nw__FUl', lambda e: alloc(e, False))
    e.stub('__nwa__FUl', lambda e: alloc(e, True))
    e.stub('__dl__FPv', lambda e: free(e, False))
    e.stub('__dla__FPv', lambda e: free(e, True))

    def construct_array(e, call):
        block, size, count = e.r(3), e.r(6), e.r(7)
        ws(e, block, [size, count])
        for i in range(count):
            set_string(e, block + 8 + i * size, '')
        return block + 8

    def destroy_array(e, call):
        block = e.r(3) - 8
        if e.cl.lab(block) > 0:
            e.cl.rec('delete', (e.cl.lab(block),))
            e.cl.forget(block)
        return None

    T.stub_cb(e, '__construct_new_array', construct_array)
    T.stub_cb(e, '__destroy_new_array', destroy_array)
    e.stub('__register_destructor', lambda e: e.cl.rec('RegisterDestructor'))

    def storage_ctor(e):
        obj = e.r(3)
        ws(e, obj, [e.storage_vt, 0])
        e.storage_paths[obj] = ''
        e.cl.rec('StorageCtor', (e.cl.newlab(obj),))

    def storage_dtor(e):
        obj = e.r(3)
        e.cl.rec('StorageDtor', (e.cl.lab(obj),))
        e.cl.forget(obj)
        e.storage_paths.pop(obj, None)

    def storage_copy(e):
        obj, other = e.r(3), e.r(4)
        name = e.storage_paths.get(other, '')
        e.cl.rec('StorageCopy', (e.cl.lab(obj), e.cl.lab(other)), (name,))
        e.storage_paths[obj] = name
        e.set_r(3, obj)

    def storage_open(e):
        out, obj, name = e.r(3), e.r(4), e.cstr(e.r(5))
        e.cl.rec('StorageOpen', (e.cl.lab(obj),), (name,))
        answer = e.cl.ret('StorageOpen', 0)
        if answer >= 0:
            e.storage_paths[obj] = name
        ws(e, out, [answer])

    def storage_close(e):
        obj = e.r(3)
        e.cl.rec('StorageClose', (e.cl.lab(obj),))
        e.storage_paths[obj] = ''

    def full_path(e):
        out, obj, dst, name = [e.r(i) for i in range(3, 7)]
        text = string(e, name)
        e.cl.rec('FullPath', (e.cl.lab(obj),), (text,))
        set_string(e, dst, e.storage_paths.get(obj, '') + text)
        ws(e, out, [0])

    def file_ctor(e):
        obj = e.r(3)
        ws(e, obj, [e.file_vt, 0])
        e.cl.rec('FileCtor', (e.cl.newlab(obj),))

    def file_open(e):
        out, obj, file, name, mode, extra = [e.r(i) for i in range(3, 9)]
        key = (e.storage_paths.get(obj, ''), e.cstr(name))
        e.cl.rec('FileOpen', (e.cl.lab(obj), e.cl.lab(file), mode, e.cl.lab(extra)), key)
        answer = e.cl.ret('FileOpen', 0)
        if answer >= 0:
            if key in e.files:
                e.file_data[file] = [e.files[key], 0]
            else:
                answer = -1
        ws(e, out, [answer])

    def file_close(e):
        e.cl.rec('FileClose', (e.cl.lab(e.r(3)),))
        e.file_data.pop(e.r(3), None)

    def file_dtor(e):
        e.cl.rec('FileDtor', (e.cl.lab(e.r(3)),))
        e.file_data.pop(e.r(3), None)
        e.cl.forget(e.r(3))

    def file_read(e):
        obj, dst, count = e.r(3), e.r(4), e.r(5)
        e.cl.rec('FileRead', (e.cl.lab(obj), e.cl.lab(dst), count))
        raw, position = e.file_data[obj]
        chunk = raw[position:position + count]
        if chunk:
            e.write(dst, chunk)
        e.file_data[obj][1] += len(chunk)
        e.set_r(3, len(chunk))

    def file_size(e):
        e.cl.rec('FileSize', (e.cl.lab(e.r(3)),))
        e.set_r(3, len(e.file_data[e.r(3)][0]))

    def file_pos(e):
        e.cl.rec('FilePos', (e.cl.lab(e.r(3)),))
        e.set_r(3, e.file_data[e.r(3)][1])

    e.storage_vt = make_vtable(e, {8: storage_close, 0x14: file_open, 0x1c: full_path})
    e.file_vt = make_vtable(e, {0xc: file_close, 0x10: file_read, 0x1c: file_size, 0x20: file_pos})
    e.stub('__ct__Q33bfl10FileSystem12CFileStorageFv', storage_ctor)
    e.stub('__dt__Q33bfl10FileSystem12CFileStorageFv', storage_dtor)
    e.stub('__as__Q33bfl10FileSystem12CFileStorageFRCQ33bfl10FileSystem12CFileStorage', storage_copy)
    e.stub('Open__Q33bfl10FileSystem16CDiscFileStorageFPCc', storage_open)
    e.stub('__ct__Q33bfl10FileSystem5CFileFv', file_ctor)
    e.stub('__dt__Q33bfl10FileSystem5CFileFv', file_dtor)
    e.stub('Close__Q33bfl10FileSystem5CFileFv', file_close)

    def ref_ctor(e):
        obj = e.r(3)
        ws(e, obj + 4, [1])
        label = e.cl.lab(obj)
        if label < 0:
            label = e.cl.newlab(obj)
        e.cl.rec('RefCtor', (label,))

    def ref_dtor(e):
        e.cl.rec('RefDtor', (e.cl.lab(e.r(3)),))

    def add_ref(e):
        obj = e.r(3)
        count = e.u32(obj + 4)
        e.cl.rec('AddRef', (e.cl.lab(obj), count))
        ws(e, obj + 4, [count + 1])
        e.set_r(3, count + 1)

    def release(e, call):
        obj = e.r(3)
        count = e.u32(obj + 4)
        e.cl.rec('Release', (e.cl.lab(obj), count))
        ws(e, obj + 4, [count - 1])
        if count == 1:
            if e.alloc_sizes.get(obj) == 0x28:
                call(('code', 0x1001f410), obj, 1)
            else:
                saved = e.r(3)
                e.set_r(3, obj + 0x24)
                storage_dtor(e)
                e.set_r(3, saved)
                ref_dtor(e)
                free(e, False)
        return count - 1

    e.stub('__ct__12TbRefReleaseFv', ref_ctor)
    e.stub('__dt__12TbRefReleaseFv', ref_dtor)
    e.stub('AddRef__12TbRefReleaseFv', add_ref)
    T.stub_cb(e, 'Release__12TbRefReleaseFv', release)

    def get_bank(e):
        index = e.r(4)
        e.cl.rec('GetBank', (e.cl.lab(e.r(3)), index))
        if e.cl.ret('GetBank', 0):
            obj = e.alloc(8)
            ws(e, obj, [e.bank_vt, 1])
            e.cl.rec('RefCtor', (e.cl.newlab(obj),))
            container = e.u32(e.u32(e.streamer + 0x24) + index * 4)
            ws(e, container + 8, [obj])
            e.set_r(3, obj)
            return
        e.set_r(3, 0)

    def bank_release(e):
        obj = e.r(3)
        count = e.u32(obj + 4)
        e.cl.rec('Release', (e.cl.lab(obj), count))
        ws(e, obj + 4, [count - 1])
        if count == 1:
            ref_dtor(e)
            free(e, False)
        e.set_r(3, count - 1)
    e.bank_vt = make_vtable(e, {8: bank_release})
    T.stub_internal(e, 0x1002748c, get_bank)
    from unicorn import UC_HOOK_CODE
    def count_categories(uc, address, size, user):
        e.category_counts[e.r(4)] = e.r(6)
    e.uc.hook_add(UC_HOOK_CODE, count_categories, begin=0x10022290, end=0x10022290)


def flip_record(raw, offset, kind):
    for off, width in SHAPES[kind][1]:
        start = offset + off
        raw[start:start + width] = raw[start:start + width][::-1]


def host_file(raw, bank=False):
    """Reverse only fields which the original record swaps, retaining every other byte."""
    out = bytearray(raw)
    if len(out) < 20:
        return bytes(out)
    flip_record(out, 0, 1)
    if len(out) < 28:
        return bytes(out)
    flip_record(out, 20, 2 if bank else 3)
    if bank or raw[:16] != SFX_GUID:
        return bytes(out)
    count = int.from_bytes(raw[24:28], 'little')
    next_ = 28 + count * 24
    for category in range(count):
        cat = 28 + category * 24
        events = int.from_bytes(raw[cat:cat + 4], 'little')
        flip_record(out, cat, 4)
        event_base = next_
        next_ += events * 20
        for event in range(events):
            ev = event_base + event * 20
            sounds = int.from_bytes(raw[ev + 4:ev + 8], 'little')
            flip_record(out, ev, 5)
            sound_base = next_
            next_ += sounds * 42
            for sound in range(sounds):
                snd = sound_base + sound * 42
                samples = int.from_bytes(raw[snd:snd + 4], 'little') & 0xffff
                branches = int.from_bytes(raw[snd + 4:snd + 8], 'little')
                flip_record(out, snd, 6)
                for sample in range(samples):
                    flip_record(out, next_ + sample * 16, 7)
                next_ += samples * 16
                for branch in range(branches):
                    flip_record(out, next_ + branch * 8, 8)
                next_ += branches * 8
    return bytes(out)


def bank_file(names, flags=None, guid=BANK_GUID):
    records = []
    strings = []
    for i, name in enumerate(names):
        records.append(struct.pack('<IBIBB', 0x12345678, 0xe1, 0x99887766, (flags or [0] * len(names))[i], 0x81 + i))
        encoded = name.encode('latin-1') + b'\0'
        strings.append(struct.pack('<I', len(encoded)) + encoded)
    return guid + struct.pack('<III', 1, 0x88776655, len(names)) + b''.join(records + strings)


def sfx_file(r, banks=1, version=1, categories=None):
    """Well-formed hierarchical blocks with arbitrary untouched bytes and old cumulative words."""
    cats = []
    tails = []
    count = r.randrange(1, 4) if categories is None else categories
    for _ in range(count):
        events = r.randrange(0, 5)
        cats.append(struct.pack('<IIHHIII', events, r.getrandbits(32), r.getrandbits(16), r.getrandbits(16),
                                r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)))
        event_records = []
        event_tails = []
        for _ in range(events):
            sounds = r.randrange(0, 4)
            event_records.append(struct.pack('<IIIIHH', r.randrange(0, 12), sounds, r.getrandbits(32), r.getrandbits(32),
                                             r.getrandbits(16), r.getrandbits(16)))
            sound_records = []
            sound_tails = []
            for _ in range(sounds):
                samples, branches = r.randrange(0, 4), r.randrange(0, 3)
                record = bytearray(r.randbytes(42))
                struct.pack_into('<III', record, 0, (r.randrange(0x10000) << 16) | samples, branches, r.getrandbits(32))
                sound_records.append(bytes(record))
                samples_raw = [struct.pack('<IIIHH', r.getrandbits(32), r.getrandbits(32), r.getrandbits(32),
                                           r.randrange(banks + 1), r.getrandbits(16)) for _ in range(samples)]
                branch_raw = [struct.pack('<IHBB', r.randrange(1, sounds + 1), r.getrandbits(16), r.randrange(256), r.randrange(256))
                              for _ in range(branches)]
                sound_tails.append(b''.join(samples_raw + branch_raw))
            event_tails.append(b''.join(sound_records + sound_tails))
        tails.append(b''.join(event_records + event_tails))
    return SFX_GUID + struct.pack('<III', version, 0x55667788, count) + b''.join(cats + tails)


def record(read, addr, kind, endian, base=None, arena=0):
    size, fields, pointers = SHAPES[kind]
    raw = read(addr, size)
    result = []
    covered = set()
    for off, width in fields:
        value = int.from_bytes(raw[off:off + width], endian)
        if off in pointers and base is not None:
            value = (value + arena if value and arena else value) - base
        result.append(value)
        covered.update(range(off, off + width))
    result.append(bytes(b for i, b in enumerate(raw) if i not in covered).hex())
    return result


def parsed(read, ptr, endian, arena=0, base=None, rebased=True):
    """Every record and pointer, in tree order. Pointers compare as offsets within its file block."""
    def word(addr, width=4):
        return int.from_bytes(read(addr, width), endian)

    def target(addr):
        value = word(addr)
        return value + arena if value and arena else value

    base = ptr if base is None else base
    cat = record(read, ptr, 4, endian, base, arena)
    events = []
    count, event_base = word(ptr), target(ptr + 4)
    if count == 0 and word(ptr + 4) == 0xa5a5a5a5:
        cat[1] = 0xa5a5a5a5
        return [cat, events]
    for i in range(count):
        ev = event_base + i * 20
        event = record(read, ev, 5, endian, base, arena)
        sounds = []
        for j in range(word(ev + 4)):
            snd = target(ev + 8) + j * 42
            sound = record(read, snd, 6, endian, base, arena)
            samples = [record(read, target(snd + 8) + k * 16, 7, endian) for k in range(word(snd) & 0xffff)]
            branches = [record(read, target(snd + 38) + k * 8, 8, endian, base if rebased else None, arena)
                        for k in range(word(snd + 4))]
            sounds.append([sound, samples, branches])
        events.append([event, sounds])
    return [cat, events]


def ppc_state(e):
    s = e.streamer
    category_size, bank_size, name_size, index_size = [e.u32(s + x + 4) for x in [0x1c, 0x24, 0xc, 0x2c]]
    categories = []
    for i in range(category_size):
        obj = e.u32(e.u32(s + 0x1c) + i * 4)
        if obj == 0:
            categories.append(None)
            continue
        head = e.u32(obj + 0x10)
        elements = []
        while head:
            elements.append(e.u32(head + 4))
            head = e.u32(head)
        data = e.u32(obj + 8)
        table = [parsed(e.read, data + j * 24, 'big', base=data) for j in range(e.category_counts.get(data, 1))] if data else None
        categories.append([e.u32(obj + 4), e.u32(obj + 0xc), string(e, obj + 0x14), elements, table])
    banks = []
    for i in range(bank_size):
        obj = e.u32(e.u32(s + 0x24) + i * 4)
        banks.append(None if obj == 0 else [e.u32(obj + 4), string(e, obj + 0xc), e.u32(obj + 0x20),
                                         e.storage_paths.get(obj + 0x24, ''), e.u32(obj + 8) != 0])
    names = [string(e, e.u32(s + 0xc) + i * 20) for i in range(name_size)]
    indices = [e.u32(e.u32(s + 0x2c) + i * 4) for i in range(index_size)]
    return [e.u32(s + 0x34), e.u32(s + 0x38), e.read(s + 0x3c, 1)[0], e.storage_paths.get(s + 0x14, ''),
            categories, banks, names, indices]


def host_state(lib):
    get = lib.tpw_ms_get
    obj = lib.tpw_ms_object
    txt = lambda kind, index=0: (lib.tpw_ms_text(kind, index) or b'').decode('latin-1')
    categories = []
    for i in range(get(0, 0)):
        if not obj(2, i):
            categories.append(None)
            continue
        head = obj(5, i)
        elements = []
        while head:
            elements.append(C.c_uint32.from_address(head + 8).value)
            head = C.c_void_p.from_address(head).value
        data = obj(4, i)
        table = [parsed(C.string_at, data + j * 24, 'little', obj(6, 0), data) for j in range(lib.tpw_ms_catcount(i))] if data else None
        categories.append([get(9, i), get(8, i), txt(2, i), elements, table])
    banks = [None if not obj(3, i) else [get(11, i), txt(3, i), get(10, i), txt(4, i), bool(get(12, i))] for i in range(get(1, 0))]
    return [get(4, 0), get(5, 0), get(6, 0), txt(0), categories, banks,
            [txt(1, i) for i in range(get(2, 0))], [get(7, i) for i in range(get(3, 0))]]


def ppc_case(e, case):
    e.streamer = SINGLETON
    e.cl = CL.PpcLog({SINGLETON: 1}, case.get('q'))
    e.map_blocks = set()
    e.alloc_sizes = {}
    e.array_fill = 0
    e.array_context = False
    e.storage_paths = {}
    e.file_data = {}
    e.category_counts = {}
    e.files = {(path, name): raw for path, name, raw in case['files']}
    e.write(SINGLETON, b'\xa5' * 0x40)
    ws(e, GUARD, [0x01000000])
    T.call_ppc(e, CTOR, [SINGLETON])
    manager = e.alloc(0x40)
    e.cl.labels[manager] = 2
    set_string(e, manager + 0x2c, 'ROOT:')
    T.call_ppc(e, 0x10016760, [SINGLETON, manager])
    steps = [[0, case.get('index', 0x12345678), ppc_state(e)]]
    index = e.alloc(4, struct.pack('>I', case.get('index', 0x12345678)))
    for op, a, first, second in case['ops']:
        out = e.alloc(4)
        if op == 8:
            obj = e.u32(e.u32(SINGLETON + 0x1c) + a * 4)
            data = e.u32(obj + 8) if obj else 0
            ev = e.alloc(12)
            e.cl.labels[ev] = 3
            ws(e, ev, [e.event_vt, a, int(first)])
            data = data + int(second) * 24 if data else 0
            answer = T.call_ppc(e, 0x10026d24, [0, data, ev])[0]
            result = -1 if answer == 0 else (answer - e.u32(data + 4)) // 20
        elif op == 5:
            result = T.call_ppc(e, FIND, [SINGLETON, cstr(e, first)])[0]
        else:
            address = [SET_PATH, REG, QUAL, REMOVE, UNLOAD, FIND, REG_BANK_STORAGE, REG_SFX_STORAGE][op]
            args = [out, SINGLETON]
            if op == 0:
                args += [cstr(e, first)]
            elif op == 1:
                args += [cstr(e, first), cstr(e, second), index]
            elif op == 2:
                args += [cstr(e, first), index]
            elif op == 3:
                args += [a]
            elif op == 4:
                count = e.u32(SINGLETON + 0x20)
                args += [e.u32(e.u32(SINGLETON + 0x1c) + a * 4) if a < count else 0]
            elif op == 6:
                args += [cstr(e, first), cstr(e, second)]
            elif op == 7:
                args += [cstr(e, first), index]
            T.call_ppc(e, address, args)
            result = T.s32(e.u32(out))
        steps.append([result, e.u32(index), ppc_state(e)])
    return dict(steps=steps, log=e.cl.log)


def host_case(lib, case):
    lib.tpw_ms_begin()
    for key, values in case.get('q', {}).items():
        for value in values:
            lib.tpw_cl_queue(key.encode(), C.c_int64(value))
    for path, name, raw in case['files']:
        data = host_file(raw, name.endswith('BANK.map'))
        lib.tpw_ms_file(path.encode(), name.encode(), data, len(data))
    index = C.c_uint32(case.get('index', 0x12345678))
    steps = [[0, index.value, host_state(lib)]]
    for op, a, first, second in case['ops']:
        if op == 8:
            result = lib.tpw_ms_map_cat(a, int(second), int(first))
        else:
            result = lib.tpw_ms_op(op, a, C.byref(index), None if first is None else first.encode(), None if second is None else second.encode())
        steps.append([result & MASK if op == 5 else result, index.value, host_state(lib)])
    return dict(steps=steps, log=CL.host_end(lib))


def case(ops, files=None, q=None):
    return dict(ops=ops, files=files or [], q=q or {})


def op(kind, a=0, first=None, second=None):
    return kind, a, first, second


def simple_edges():
    yield case([op(0, first='A:'), op(5, first=None), op(5, first=''), op(5, first='unknown')])


def simple_random(r):
    return case([op(0, first=r.choice(['', 'P:', 'x:data:y:', 'a:b:c:']))], q={'StorageOpen': [r.choice([0, 0, 1, -1, -123])]})


def registration_edges():
    for version in [0, 1, MASK]:
        for names in [[], ['Sfx'], ['Sound\\Sfx', 'Music'], ['a\\b\\c', '\\first']]:
            flags = [0] * len(names)
            files = [('P:', 'catBANK.map', bank_file(names, flags)), ('P:', 'catSFX.map', sfx_file(random.Random(8), len(names), version))]
            yield case([op(1, first='P:', second='cat'), op(1, first='P:', second='cat'), op(5, first='P:cat'),
                        op(3, 1), op(3, 1), op(3, 1)], files)
    # name lengths with every byte of the four-byte length in play (the Mac sign-extends each byte before shifting it). The last
    # bank's length is the file's length word (name plus its NUL). A byte of 0x80 or more makes the length negative: the cursor
    # moves back and RemovePath scans backwards from before the name, down to the backslash in the first bank's name (300 long,
    # so its own length bytes, 0x2c and 0x01, are positive). The last name has a backslash near its end, which a scan from the
    # name's true end finds (the emulator side keeps names to 4096 characters, so a 64 KB name shows only through its tail).
    for length in (127, 128, 129, 200, 255, 256, 257, 300, 383, 384, 511, 512, 65536, 65537, 65792, 70000):
        names = ['d\\' + 'a' * 297, 'b' * (length - 6) + '\\wxyz']
        files = [('P:', 'catBANK.map', bank_file(names, [0, 0])), ('P:', 'catSFX.map', sfx_file(random.Random(8), 2, 1))]
        yield case([op(1, first='P:', second='cat'), op(5, first='P:cat'), op(3, 1)], files)
    # a second byte of 0x80 or more (a length of 0x8000 and up): two names of 0x7f7f bytes (both length bytes below 0x80) lie
    # before the last one, whose scan back from its start reaches the first name's backslash, a long way down
    for length in (0x8000, 0xff00):
        names = ['d\\ab\0' + 'a' * (0x7f7f - 6), 'b' * (0x7f7f - 6) + '\\wxyz', 'b' * (length - 6) + '\\wxyz']
        files = [('P:', 'catBANK.map', bank_file(names, [0, 0, 0])), ('P:', 'catSFX.map', sfx_file(random.Random(8), 3, 1))]
        yield case([op(1, first='P:', second='cat'), op(5, first='P:cat'), op(3, 1)], files)
    # the qualified name's "SFX.map" is searched from index 0: a name that is just "SFX.map" splits at 0 (an empty category name),
    # and "SFX.mapSFX.map" at its first occurrence
    for qualified in ('P:SFX.map', 'P:SFX.mapSFX.map', 'P:catSFX.mapSFX.map', 'SFX.map', 'P:SFX.map.x'):
        yield case([op(2, first=qualified), op(2, first=qualified)],
                   [('P:', 'SFX.map', bank_file(['Sfx'])), ('P:', 'BANK.map', bank_file(['Sfx'])),
                    ('P:', 'catBANK.map', bank_file(['Sfx'])), ('P:', 'catSFX.map', sfx_file(random.Random(3))),
                    ('P:', 'SFX.mapBANK.map', bank_file(['Sfx'])), ('P:', 'SFX.mapSFX.map', sfx_file(random.Random(4)))])
    # a category that is already registered answers 0 at once and leaves the bank index table alone: register it, add bank indices
    # to the table with the bank loader on its own (the table is cleared only at the end of a registration), register it again
    for names in (['Sfx'], ['Sfx', 'Music']):
        files = [('P:', 'catBANK.map', bank_file(names, [0] * len(names))), ('P:', 'catSFX.map', sfx_file(random.Random(8), len(names), 1))]
        yield case([op(1, first='P:', second='cat'), op(6, first='catBANK.map', second='P:'), op(1, first='P:', second='cat'),
                    op(6, first='catBANK.map', second='P:')], files)
    # FindCatagory compares the whole name (strcmp): names sharing a prefix with the registered "P:cat" are not found
    files = [('P:', 'catBANK.map', bank_file(['Sfx'])), ('P:', 'catSFX.map', sfx_file(random.Random(8), 1, 1))]
    yield case([op(1, first='P:', second='cat')] + [op(5, first=name) for name in
                                                    ('P:', 'P:c', 'P:ca', 'P:cat', 'P:catx', 'P:cat\\', 'p:cat', 'P:Cat', 'cat')], files)
    yield case([op(1, first='P:', second=None)])
    yield case([op(1, first='P:', second='missing')])
    yield case([op(2, first='P:catBANK.map')])
    yield case([op(2, first='bad')])
    yield case([op(2, first='P:catSFX.map')], [('P:', 'catBANK.map', bank_file(['Sfx'])),
                                           ('P:', 'catSFX.map', sfx_file(random.Random(1)))])


def registration_random(r):
    path = r.choice(['P:', 'Q:', 'Disk:data:sound:', 'Disk:data:x:data:local:', ''])
    names = [r.choice(['Sfx', 'Music', 'Sound\\Sfx', '\\first', 'a\\b\\c']) for _ in range(r.randrange(0, 4))]
    flags = [r.randrange(256) for _ in names]
    files = [(path, 'catBANK.map', bank_file(names, flags)), (path, 'catSFX.map', sfx_file(r, len(names), r.choice([0, 1, 2])))]
    # Global-flag banks require a second storage; the map tables still share their bank indices.
    ops = [op(1, first=path, second='cat')]
    for _ in range(r.randrange(1, 5)):
        ops.append(r.choice([op(1, first=path, second='cat'), op(5, first=path + 'cat'), op(5, first='unknown')]))
    ops += [op(3, 1), op(4, 1)]
    return case(ops, files, {'GetBank': [r.randrange(2) for _ in range(10)]})


TESTS = [
    FnTest('streamer constructor, SetPath, FindCatagory', ppc_case, host_case, edge=simple_edges, rnd=simple_random),
    FnTest('RegisterCatagory, bank/SFX readers, remove/unload scripts', ppc_case, host_case,
           edge=registration_edges, rnd=registration_random),
]

# Direct array scripts exercise every emitted instance, including grow/copy/clear and string Find.
ARRAYS = [
    dict(ctor=0x1001fd4c, dtor=0x1001fccc, clear=0x1001f8e0, get=0x100211a0, size=None, next=None),
    dict(ctor=0x1001fff4, dtor=0x1001fedc, clear=0x1001ff70, get=0x10016fc8, size=0x1001f320, next=0x10023c70),
    dict(ctor=0x1001fe94, dtor=0x1001fd84, clear=0x1001fe14, get=0x100213a0, size=0x10021160, next=0x10023bb0),
    dict(ctor=0x1002016c, dtor=0x10020040, clear=0x100200dc, get=0x1001f954, size=None, next=None),
]


def array_ppc(e, case):
    e.cl = CL.PpcLog({})
    e.alloc_sizes = {}
    e.map_blocks = set()
    e.array_fill = case.get('fill', 0)
    e.array_context = True
    kind, ops = case['kind'], case['ops']
    entries = ARRAYS[kind]
    obj = e.alloc(8, b'\xa5' * 8)
    e.cl.labels[obj] = 1
    T.call_ppc(e, entries['ctor'], [obj])
    ws(e, obj + 4, [case.get('empty_count', 0)])
    steps = [[0, 0, []]]
    found = e.alloc(4, struct.pack('>I', 0xdeadbeef))
    for op, index, value, text in ops:
        if op == 0:
            result = T.call_ppc(e, entries['size'], [obj])[0] if entries['size'] else e.u32(obj + 4)
        elif op in [1, 2]:
            slot = T.call_ppc(e, entries['get'], [obj, index])[0]
            if kind == 3:
                if op == 2:
                    set_string(e, slot, text)
                result = index
            else:
                if op == 2:
                    ws(e, slot, [value])
                result = e.u32(slot)
        elif op == 3:
            T.call_ppc(e, entries['clear'], [obj])
            result = 0
        elif op == 4:
            result = T.call_ppc(e, entries['next'], [obj])[0]
        else:
            wanted = e.alloc(20)
            set_string(e, wanted, text)
            result = T.call_ppc(e, 0x10020a6c, [obj, wanted, found])[0]
        count = e.u32(obj + 4)
        ptr = e.u32(obj)
        values = [string(e, ptr + i * 20) if kind == 3 else e.u32(ptr + i * 4) for i in range(count)] if ptr else None
        steps.append([result, e.u32(found), count, bool(ptr), values])
    T.call_ppc(e, entries['dtor'], [obj, case.get('flag', 0)])
    return dict(steps=steps, state=[bool(e.u32(obj)), e.u32(obj + 4)], log=e.cl.log)


def array_host(lib, case):
    lib.tpw_ms_array_begin.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_ms_array_begin(case['kind'], case.get('fill', 0))
    lib.tpw_ms_array_empty_count(case['kind'], case.get('empty_count', 0))
    lib.tpw_ms_array_op.argtypes = [C.c_uint32] * 4 + [C.c_char_p, C.POINTER(C.c_uint32)]
    lib.tpw_ms_array_op.restype = C.c_uint32
    lib.tpw_ms_array_get.argtypes = [C.c_uint32, C.c_uint32]
    lib.tpw_ms_array_get.restype = C.c_uint32
    lib.tpw_ms_array_size.restype = C.c_uint32
    lib.tpw_ms_array_text.restype = C.c_char_p
    found = C.c_uint32(0xdeadbeef)
    kind = case['kind']
    steps = [[0, 0, []]]
    for op, index, value, text in case['ops']:
        result = lib.tpw_ms_array_op(kind, op, index, value, text.encode(), C.byref(found))
        count = lib.tpw_ms_array_size(kind)
        present = bool(lib.tpw_ms_array_present(kind))
        values = [(lib.tpw_ms_array_text(i) or b'').decode('latin-1') if kind == 3 else lib.tpw_ms_array_get(kind, i)
                  for i in range(count)] if present else None
        steps.append([result, found.value, count, present, values])
    state = (C.c_uint32 * 2)()
    lib.tpw_ms_array_dtor.argtypes = [C.c_uint32, C.c_int32, C.c_void_p]
    lib.tpw_ms_array_dtor(kind, case.get('flag', 0), state)
    return dict(steps=steps, state=[bool(state[0]), state[1]], log=CL.host_end(lib))


def array_edges(kind):
    yield dict(kind=kind, empty_count=5, ops=[(3, 0, 0, ''), (0, 0, 0, '')])
    for flag in [-1, 0, 1, 0x8000, 0xffff, 0x10001]:
        yield dict(kind=kind, ops=[], flag=flag)
    yield dict(kind=kind, ops=[(2, 0, 7, 'a'), (2, 4, 0, 'b'), (2, 1, MASK, 'a'), (1, 4, 0, ''), (3, 0, 0, ''), (3, 0, 0, '')])
    if kind in [1, 2]:
        yield dict(kind=kind, ops=[(4, 0, 0, ''), (2, 2, 7, ''), (4, 0, 0, ''), (2, 0, 8, ''), (4, 0, 0, ''),
                                 (2, 1, 9, ''), (4, 0, 0, '')])
    if kind == 3:
        yield dict(kind=kind, ops=[(5, 0, 0, 'x'), (2, 0, 0, 'a'), (2, 2, 0, 'b'), (5, 0, 0, ''),
                                 (5, 0, 0, 'b'), (5, 0, 0, 'missing')])


def array_random(r, kind):
    ops = [(2, r.randrange(0, 20), r.getrandbits(32), r.choice(['', 'a', 'b']))]
    for _ in range(r.randrange(1, 15)):
        op = r.choice([0, 1, 2, 2, 2, 3] + ([4] if kind in [1, 2] else [5] if kind == 3 else []))
        ops.append((op, r.randrange(0, 20), r.choice([0, 0, 1, MASK, r.getrandbits(32)]), r.choice(['', 'a', 'b', 'same', 'x' * 30])))
    if kind in [1, 2]:
        ops.append((4, 0, 0, ''))
    if kind == 3:
        ops.append((5, 0, 0, r.choice(['', 'a', 'missing'])))
    return dict(kind=kind, ops=ops, flag=r.choice([0, -1, 1]), fill=0 if kind == 3 else r.choice([0, MASK, 0xa5a5a5a5, r.getrandbits(32)]))


for kind, entries in enumerate(ARRAYS):
    TESTS.append(FnTest(f'CResize instance {kind}: ctor, grow, copy, Clear, dtor, Size/Find/free slot', array_ppc, array_host,
                        edge=lambda k=kind: array_edges(k), rnd=lambda r, k=kind: array_random(r, k)))


# Standalone swaps run on equal numeric field values with independently packed native/PPC bytes.
def swap_ppc(e, case):
    ptr = e.alloc(len(case['ppc']), case['ppc'])
    T.call_ppc(e, case['address'], [ptr] + ([case['count']] if case.get('block') else []))
    return dict(data=case['decode'](e.read(ptr, len(case['ppc'])), 'big'))


def swap_host(lib, case):
    ptr = C.create_string_buffer(case['host'], len(case['host']))
    if case.get('block32'):
        lib.tpw_ms_block32.argtypes = [C.c_void_p, C.c_int32]
        lib.tpw_ms_block32(ptr, case['count'])
    else:
        lib.tpw_ms_swap(case['kind'], ptr, case['count'])
    return dict(data=case['decode'](ptr.raw, 'little'))


def swap_case(r, address, kind, block=False):
    size, fields, _ = SHAPES[kind] if kind < 9 else (4 if kind == 9 else 2, [(0, 4 if kind == 9 else 2)], [])
    count = r.randrange(0, 9) if block else 1
    ppc = bytearray(r.randbytes(max(count, 1) * size))
    host = bytearray(ppc)
    for i in range(max(count, 1)):
        for off, width in fields:
            start = i * size + off
            host[start:start + width] = host[start:start + width][::-1]
    def decode(raw, endian):
        rows = []
        for i in range(max(count, 1)):
            values = []
            covered = set()
            for off, width in fields:
                start = i * size + off
                value = int.from_bytes(raw[start:start + width], endian)
                # Float file swaps pass through lfs/stfs; quiet/signalling NaNs compare as one value.
                if kind in [4, 6] and off in ([12, 16, 20] if kind == 4 else [34]):
                    if value & 0x7f800000 == 0x7f800000 and value & 0x7fffff:
                        value = 'NaN'
                values.append(value)
                covered.update(range(off, off + width))
            values.append(bytes(raw[i * size + j] for j in range(size) if j not in covered).hex())
            rows.append(values)
        return rows
    return dict(address=address, kind=kind, count=count, block=block, ppc=bytes(ppc), host=bytes(host), decode=decode)


SWAPS = [(0x10021be4, 0, False), (0x10021b80, 1, False), (0x100219c8, 2, False), (0x10022060, 3, False),
         (0x1002268c, 4, False), (0x1002259c, 5, False), (0x10022970, 6, False), (0x10022e00, 7, False),
         (0x10022cec, 8, False), (0x100224f8, 5, True), (0x100228cc, 6, True), (0x10022d5c, 7, True),
         (0x10022c3c, 8, True)]
for address, kind, block in SWAPS:
    TESTS.append(FnTest(f'file record swap {address:#x}', swap_ppc, swap_host,
                        edge=lambda a=address, k=kind, b=block: [swap_case(random.Random(0), a, k, b)],
                        rnd=lambda r, a=address, k=kind, b=block: swap_case(r, a, k, b)))

# Every emitted primitive swap, including duplicated code addresses in other sound translation units.
SCALAR32 = [0x10008db4, 0x10009020, 0x10009078, 0x1000947c, 0x100094d4, 0x10013220, 0x1001ad24,
            0x1001ad7c, 0x1001add4, 0x1001af8c, 0x1001d8b8, 0x1001d90c, 0x1001d964, 0x1001da34,
            0x10021a7c, 0x100220c8, 0x10022620, 0x1002271c, 0x10022a28, 0x10022aa0, 0x100236b4]
SCALAR16 = [0x10008fd0, 0x1000942c]
INPLACE32 = [0x10009588, 0x1001b63c, 0x1001b684, 0x1001b6cc, 0x1001b710, 0x1001b768,
             0x10021a34, 0x10023a30, 0x1002e6a8, 0x1002e85c]


def scalar_ppc(e, case):
    return dict(value=T.call_ppc(e, case['address'], [case['value']])[0])


def scalar_host(lib, case):
    return dict(value=lib.tpw_ms_scalar(case['width'], case['value']))


for address, width in [(a, 4) for a in SCALAR32] + [(a, 2) for a in SCALAR16]:
    TESTS.append(FnTest(f'USwap{width * 8} {address:#x}', scalar_ppc, scalar_host,
                        edge=lambda a=address, w=width: [dict(address=a, width=w, value=v & ((1 << (w * 8)) - 1))
                                                       for v in T.INT_EDGES],
                        rnd=lambda r, a=address, w=width: dict(address=a, width=w, value=r.getrandbits(w * 8))))
for address, kind in [(a, 9) for a in INPLACE32] + [(0x10021c48, 10)]:
    TESTS.append(FnTest(f'USwapInplace {address:#x}', swap_ppc, swap_host,
                        edge=lambda a=address, k=kind: [swap_case(random.Random(0), a, k)],
                        rnd=lambda r, a=address, k=kind: swap_case(r, a, k)))


def float_ppc(e, case):
    value = T.bits_f32(case['value'])
    result = T.call_ppc(e, case['address'], [T.F(value)])[2]
    return dict(value='NaN' if result != result else T.f32_bits(result))


def float_host(lib, case):
    lib.tpw_ms_float.argtypes = [C.c_float]
    lib.tpw_ms_float.restype = C.c_float
    result = lib.tpw_ms_float(T.bits_f32(case['value']))
    return dict(value='NaN' if result != result else T.f32_bits(result))


for address in [0x1001ae28, 0x1001ae78]:
    TESTS.append(FnTest(f'float swap {address:#x}', float_ppc, float_host,
                        edge=lambda a=address: [dict(address=a, value=v) for v in [0, 0x80000000, 0x3f800000, MASK, 0x7fc01234]],
                        rnd=lambda r, a=address: dict(address=a, value=r.getrandbits(32))))


# The auto-array ownership wrappers exercise constructor, conversion, null destructor and its signed flag.
AUTOS = [(0x10021368, 0x10021298, 0x100212d4), (0x100171d8, 0x100170d0, 0x10017130),
         (0x100215a0, 0x100214a4, 0x100214fc), (0x1001fbc4, 0x1001faa4, 0x1001fb14)]


def auto_ppc(e, case):
    e.cl = CL.PpcLog({})
    kind, count = case['kind'], case['count']
    obj = 0 if case['no_object'] else e.alloc(4, b'\xa5' * 4)
    e.cl.labels[obj] = 1
    ptr = 0 if case['no_data'] else e.alloc(max(count, 1) * (20 if kind == 3 else 4) + (8 if kind == 3 else 0))
    if kind == 3 and ptr:
        ws(e, ptr, [20, count])
        ptr += 8
        for i in range(count):
            set_string(e, ptr + i * 20, '')
    ctor, conv, dtor = AUTOS[kind]
    if obj:
        T.call_ppc(e, ctor, [obj, ptr])
        values = [bool(obj), bool(e.u32(obj)), T.call_ppc(e, conv, [obj])[0] == ptr]
    else:
        values = [False, False, False]
    T.call_ppc(e, dtor, [obj, case['flag']])
    return dict(values=values, log=e.cl.log)


def auto_host(lib, case):
    lib.tpw_ms_auto.argtypes = [C.c_uint32] * 4 + [C.c_int32, C.c_void_p]
    values = (C.c_uint32 * 3)()
    lib.tpw_ms_auto(case['kind'], case['count'], case['no_object'], case['no_data'], case['flag'], values)
    return dict(values=[bool(v) for v in values], log=CL.host_end(lib))


def auto_edges(kind):
    for no_object, no_data in [(0, 0), (0, 1), (1, 1)]:
        for flag in [-1, 0, 1, 0x8000, 0xffff, 0x10001]:
            yield dict(kind=kind, count=3, no_object=no_object, no_data=no_data, flag=flag)


def auto_random(r, kind):
    return dict(kind=kind, count=r.randrange(0, 9), no_object=False,
                no_data=r.randrange(3) == 0, flag=r.choice([-1, 0, 1, 0x8000, 0x10001]))


for kind, entries in enumerate(AUTOS):
    TESTS.append(FnTest(f'CAutoArrayPtr instance {kind}: ctor, conversion, dtor', auto_ppc, auto_host,
                        edge=lambda k=kind: auto_edges(k), rnd=lambda r, k=kind: auto_random(r, k)))

# Read buffers for the MapEvent consumer and ModifyEventForSense, with all predicate boundaries.
def map_ppc(e, case):
    e.cl = CL.PpcLog({})
    rows = case['rows']
    block = e.alloc(24 + 20 * len(rows))
    ws(e, block, [len(rows), block + 24])
    for i, (event, sounds) in enumerate(rows):
        ws(e, block + 24 + i * 20, [event, sounds, 0, 0, 0])
    event = e.alloc(12)
    e.cl.labels[event] = 3
    if not hasattr(e, 'event_vt'):
        def event_id(e):
            e.cl.rec('Event.Event', (e.cl.lab(e.r(3)),))
            e.set_r(3, e.u32(e.r(3) + 8))
        def category_id(e):
            e.cl.rec('Event.Category', (e.cl.lab(e.r(3)),))
            e.set_r(3, e.u32(e.r(3) + 4))
        e.event_vt = make_vtable(e, {0xc: category_id, 0x10: event_id})
    ws(e, event, [e.event_vt, case['category'], case['event']])
    result = T.call_ppc(e, 0x10026d24, [0, 0 if case['null'] else block, event])[0]
    return dict(result=-1 if result == 0 else (result - block - 24) // 20, log=e.cl.log)


def map_host(lib, case):
    lib.tpw_cl_reset(100)
    C.c_int32.in_dll(lib, 'tpw_cl_on').value = 1
    raw = bytearray(24 + 20 * len(case['rows']))
    struct.pack_into('<I', raw, 0, len(case['rows']))
    for i, (event, sounds) in enumerate(case['rows']):
        struct.pack_into('<II', raw, 24 + i * 20, event, sounds)
    lib.tpw_ms_block.argtypes = [C.c_void_p, C.c_uint32]
    lib.tpw_ms_block.restype = C.c_void_p
    lib.tpw_ms_block_free.argtypes = [C.c_void_p]
    ptr = lib.tpw_ms_block(bytes(raw), len(raw))
    base = lib.tpw_ms_object(6, 0)
    C.c_uint32.from_address(ptr + 4).value = ptr + 24 - base
    lib.tpw_ms_map.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint32]
    result = lib.tpw_ms_map(ptr, case['category'], case['event'], case['null'])
    lib.tpw_ms_block_free(ptr)
    return dict(result=result, log=CL.host_end(lib))


def map_edges():
    for rows in [[], [(1, 0)], [(1, 1)], [(1, 0), (1, 1)], [(1, 1), (1, 0)], [(MASK, 1)], [(0, 1)]]:
        for event in [0, 1, 2, MASK]:
            for category, null in [(0, 0), (1, 0), (MASK, 0), (1, 1)]:
                yield dict(rows=rows, event=event, category=category, null=null)


def map_random(r):
    return dict(rows=[(r.randrange(10), r.choice([0, 1, MASK])) for _ in range(r.randrange(0, 10))],
                event=r.choice([0, r.randrange(12), MASK]), category=r.choice([0, 1, MASK]), null=r.randrange(5) == 0)


TESTS.append(FnTest('MapEvent 0x10026d24', map_ppc, map_host, edge=map_edges, rnd=map_random))


def modify_ppc(e, case):
    flags, sounds, samples, branches, f10, f12 = case
    snd = e.alloc(42)
    ws(e, snd, [samples])
    ws(e, snd + 38, [branches])
    e.write(snd + 16, struct.pack('>HH', f10, f12))
    ev = e.alloc(20)
    ws(e, ev + 4, [sounds, snd])
    e.write(ev + 16, struct.pack('>H', flags))
    T.call_ppc(e, 0x10022358, [0, ev])
    event_raw = bytearray(e.read(ev, 20))
    event_raw[8:12] = bytes(4)
    return dict(flags=int.from_bytes(e.read(ev + 16, 2), 'big'),
                event=record(lambda a, n: event_raw[a:a + n], 0, 5, 'big'), sound=record(e.read, snd, 6, 'big'))


def modify_host(lib, case):
    flags, sounds, samples, branches, f10, f12 = case
    snd = bytearray(42)
    struct.pack_into('<I', snd, 0, samples)
    struct.pack_into('<HH', snd, 16, f10, f12)
    struct.pack_into('<I', snd, 38, branches)
    ptr = lib.tpw_ms_block(bytes(snd), len(snd))
    ev = C.create_string_buffer(20)
    struct.pack_into('<I', ev, 4, sounds)
    struct.pack_into('<H', ev, 16, flags)
    lib.tpw_ms_modify.argtypes = [C.c_void_p, C.c_void_p]
    result = lib.tpw_ms_modify(ev, ptr)
    event_raw = bytearray(ev.raw)
    event_raw[8:12] = bytes(4)
    sound_fields = record(C.string_at, ptr, 6, 'little')
    lib.tpw_ms_block_free(ptr)
    return dict(flags=result, event=record(lambda a, n: event_raw[a:a + n], 0, 5, 'little'), sound=sound_fields)


def modify_edges():
    for flags in [0, 2, 4, 6, 0xfffe, 0xffff, 14]:
        for sounds in [0, 1, 2, MASK]:
            for samples, branches, f10, f12 in [(0, 0, 0, 0), (1, 0, 0, 0), (0xffff0001, 0, 0, 0),
                                               (2, 0, 0, 0), (1, 1, 0, 0), (1, 0, 1, 0), (1, 0, 0, 1)]:
                yield flags, sounds, samples, branches, f10, f12


def modify_random(r):
    return (r.choice([6, 0xfffe, r.randrange(0x10000)]), r.choice([1, 1, 0, 2]),
            r.choice([1, 0xffff0001, 0, 2]), r.choice([0, 0, MASK]), r.choice([0, 0, 1, 0xffff]), r.choice([0, 0, 1, 0xffff]))


TESTS.append(FnTest('ModifyEventForSense 0x10022358', modify_ppc, modify_host, edge=modify_edges, rnd=modify_random))


def helper_ppc(e, case):
    kind = case['kind']
    if kind == 'path':
        prefix = case.get('prefix', '').encode('latin-1')
        text = prefix + case['text'].encode('latin-1') + b'\0'
        base = e.alloc(len(text), text)
        ptr = base + len(prefix)
        answer = T.call_ppc(e, 0x100215f8, [0, ptr, case['length']])[0]
        return dict(result=answer - ptr, bytes=e.read(base, len(text)))
    if kind == 'guid':
        first = e.alloc(16, case['first'])
        second = e.alloc(16, case['second'])
        return dict(result=T.call_ppc(e, 0x10021ae4, [first, second])[0])
    if kind == 'header':
        raw = bytearray(case['raw'])
        flip_record(raw, 0, 1)
        ptr = e.alloc(20, bytes(raw))
        obj = e.alloc(64, b'\xa5' * 64)
        answer = T.call_ppc(e, 0x10021e30, [obj, ptr])[0]
        return dict(result=answer, old=e.read(obj + 0x3c, 1)[0])
    if kind == 'header_ctor':
        ptr = e.alloc(20, case['raw'])
        T.call_ppc(e, 0x10021c90, [ptr])
        return dict(guid=e.read(ptr, 16), version=e.u32(ptr + 16))
    if kind == 'fixup':
        ptr = e.alloc(8, b'\xa5' * 8)
        T.call_ppc(e, 0x100201c0, [ptr])
        return dict(data=e.u32(ptr), count=e.u32(ptr + 4))
    raise AssertionError(kind)


def helper_host(lib, case):
    kind = case['kind']
    if kind == 'path':
        prefix = case.get('prefix', '').encode('latin-1')
        ptr = C.create_string_buffer(prefix + case['text'].encode() + b'\0')
        lib.tpw_ms_remove_path.argtypes = [C.c_void_p, C.c_uint32]
        result = lib.tpw_ms_remove_path(C.byref(ptr, len(prefix)), case['length'])
        return dict(result=result, bytes=ptr.raw[:-1])
    if kind == 'guid':
        lib.tpw_ms_guid.argtypes = [C.c_void_p, C.c_void_p]
        return dict(result=lib.tpw_ms_guid(case['first'], case['second']))
    if kind == 'header':
        ptr = C.create_string_buffer(case['raw'])
        old = C.c_uint32()
        lib.tpw_ms_header.argtypes = [C.c_void_p, C.POINTER(C.c_uint32)]
        result = lib.tpw_ms_header(ptr, C.byref(old))
        return dict(result=result, old=old.value)
    if kind == 'header_ctor':
        ptr = C.create_string_buffer(case['raw'])
        lib.tpw_ms_header_ctor.argtypes = [C.c_void_p]
        lib.tpw_ms_header_ctor(ptr)
        return dict(guid=ptr.raw[:16], version=C.c_uint32.from_buffer(ptr, 16).value)
    if kind == 'fixup':
        ptr = C.create_string_buffer(b'\xa5' * 16)
        lib.tpw_ms_fixup_ctor.argtypes = [C.c_void_p]
        lib.tpw_ms_fixup_ctor(ptr)
        return dict(data=C.c_void_p.from_buffer(ptr).value or 0, count=C.c_uint32.from_buffer(ptr, 8).value)
    raise AssertionError(kind)


def helper_random(r, kind):
    if kind == 'path':
        text = ''.join(r.choice('ab\\/:') for _ in range(r.randrange(1, 80)))
        return dict(kind=kind, text=text, length=r.randrange(1, len(text) + 2))
    if kind == 'guid':
        first = r.randbytes(16)
        second = first if r.randrange(2) else r.randbytes(16)
        return dict(kind=kind, first=first, second=second)
    if kind == 'header':
        guid = r.choice([SFX_GUID, BANK_GUID, r.randbytes(16)])
        return dict(kind=kind, raw=guid + struct.pack('<I', r.choice([0, 0, 1, MASK, r.getrandbits(32)])))
    if kind == 'header_ctor':
        return dict(kind=kind, raw=r.randbytes(20))
    return dict(kind=kind)


def helper_edges(kind):
    if kind == 'path':
        yield dict(kind=kind, text='a', length=0, prefix='\\')
        yield dict(kind=kind, text='a', length=MASK, prefix='\\X')
        for text in ['', 'a', '\\a', 'a\\', 'a\\b\\c', 'a/b', ':x:', '\\\\']:
            yield dict(kind=kind, text=text, length=len(text) + 1)
    elif kind == 'guid':
        yield dict(kind=kind, first=SFX_GUID, second=SFX_GUID)
        for i in range(16):
            second = bytearray(SFX_GUID)
            second[i] ^= 1
            yield dict(kind=kind, first=SFX_GUID, second=bytes(second))
    elif kind == 'header':
        for guid in [SFX_GUID, BANK_GUID, bytes(16)]:
            for version in [0, 1, MASK]:
                yield dict(kind=kind, raw=guid + struct.pack('<I', version))
    else:
        yield helper_random(random.Random(0), kind)


for kind, address in [('path', 0x100215f8), ('guid', 0x10021ae4), ('header', 0x10021e30),
                      ('header_ctor', 0x10021c90), ('fixup', 0x100201c0)]:
    TESTS.append(FnTest(f'{kind} helper {address:#x}', helper_ppc, helper_host,
                        edge=lambda k=kind: helper_edges(k), rnd=lambda r, k=kind: helper_random(r, k)))

# Container scripts compare the whole element chain after every operation, and deletion order.
def container_ppc(e, case):
    initial = ppc_case(e, dict(files=[], ops=[], q=case.get('q', {})))
    obj = e.alloc(40, b'\xa5' * 40)
    e.alloc_sizes[obj] = 40
    T.call_ppc(e, 0x1001fc30, [obj, case['index']])
    if case['attach']:
        slot = T.call_ppc(e, 0x10016fc8, [SINGLETON + 0x1c, case['index']])[0]
        ws(e, slot, [obj])
    steps = []
    for op, value in case['ops']:
        result = 0
        if op == 0:
            T.call_ppc(e, 0x1001f5f0, [obj, value])
            head = e.u32(obj + 16)
            if head and e.cl.lab(head) < 0:
                e.cl.newlab(head)
        elif op == 1:
            result = T.call_ppc(e, 0x1001f590, [obj, value])[0]
        elif op == 2:
            head = e.u32(obj + 16)
            if head:
                T.call_ppc(e, 0x1001f50c, [head])
            ws(e, obj + 16, [0])
        elif op == 3:
            T.call_ppc(e, 0x1001f410, [obj, value])
            obj = 0
        fields = None
        elements = []
        if obj:
            fields = [e.u32(obj + 4), e.u32(obj + 8), e.u32(obj + 12), string(e, obj + 20)]
            head = e.u32(obj + 16)
            while head:
                elements.append(e.u32(head + 4))
                head = e.u32(head)
        steps.append([result, fields, elements])
    return dict(initial=initial['steps'], steps=steps, log=e.cl.log, state=ppc_state(e))


def container_host(lib, case):
    initial = host_case(lib, dict(files=[], ops=[], q=case.get('q', {})))
    C.c_int32.in_dll(lib, 'tpw_cl_on').value = 1
    lib.tpw_ms_container_begin(case['index'], case['attach'])
    lib.tpw_ms_container.restype = C.c_void_p
    lib.tpw_ms_elements.restype = C.c_void_p
    lib.tpw_ms_container_field.restype = C.c_uint32
    lib.tpw_ms_container_text.restype = C.c_char_p
    lib.tpw_ms_container_op.argtypes = [C.c_uint32, C.c_uint32]
    steps = []
    for op, value in case['ops']:
        result = lib.tpw_ms_container_op(op, value)
        obj = lib.tpw_ms_container()
        fields = None
        elements = []
        if obj:
            # Native offsets are recovered from accessors instead of Mac layout arithmetic.
            fields = [lib.tpw_ms_container_field(i) for i in range(3)]
            fields.append((lib.tpw_ms_container_text() or b'').decode('latin-1'))
            head = lib.tpw_ms_elements()
            while head:
                elements.append(C.c_uint32.from_address(head + 8).value)
                head = C.c_void_p.from_address(head).value
        steps.append([result, fields, elements])
    return dict(initial=initial['steps'], steps=steps, log=CL.host_end(lib), state=host_state(lib))


def container_edges():
    yield dict(index=1, attach=0, ops=[(0, 7), (0, 5), (1, 7), (3, 0)], q={'ElementAlloc': [1, 0]})
    for index in [1, 2, 7]:
        for attach in [0, 1]:
            for flag in [-1, 0, 1, 0x8000, 0xffff, 0x10001]:
                yield dict(index=index, attach=attach, ops=[(1, 0), (0, 5), (0, 7), (0, 5), (1, 5), (1, 0), (3, flag)])
    yield dict(index=1, attach=0, ops=[(0, 1), (0, 2), (2, 0), (1, 1), (3, 0)])


def container_random(r):
    ops = [(1, r.getrandbits(32)), (0, r.randrange(10))]
    for _ in range(r.randrange(1, 12)):
        kind = r.choice([0, 0, 0, 1, 1, 2])
        value = r.randrange(0, 10) if kind == 0 else r.choice([0, 1, 2, MASK, r.getrandbits(32)])
        ops.append((kind, value))
    value = r.getrandbits(32)
    ops += [(0, value), (1, value), (2, 0), (0, r.randrange(10)), (3, r.choice([-1, 0, 1]))]
    return dict(index=r.randrange(1, 9), attach=r.randrange(2), ops=ops,
                q={'ElementAlloc': [r.choice([1, 1, 1, 0]) for _ in range(20)]})


TESTS.append(FnTest('category container ctor, Find, Add, element ctor/Release, dtor', container_ppc, container_host,
                    edge=container_edges, rnd=container_random))


def storage_ppc(e, case):
    e.cl = CL.PpcLog({})
    e.storage_paths = {}
    obj = 0 if case['null'] else e.alloc(12, b'\xa5' * 12)
    e.cl.labels[obj] = 1
    if obj:
        T.call_ppc(e, 0x100204ac, [obj])
    value = 0 if not obj else e.read(obj, 1)[0]
    T.call_ppc(e, 0x1002041c, [obj, case['flag']])
    return dict(value=value, log=e.cl.log)


def storage_host(lib, case):
    value = (C.c_uint32 * 1)()
    lib.tpw_ms_storage.argtypes = [C.c_int32, C.c_uint32, C.c_void_p]
    lib.tpw_ms_storage(case['flag'], case['null'], value)
    return dict(value=value[0], log=CL.host_end(lib))


def storage_edges():
    for flag in [-1, 0, 1, 0x8000, 0xffff, 0x10001]:
        for null in [False, True]:
            yield dict(flag=flag, null=null)


for address in [0x100204ac, 0x1002041c]:
    TESTS.append(FnTest(f'storage container {address:#x}', storage_ppc, storage_host,
                        edge=storage_edges, rnd=lambda r: dict(flag=r.randrange(-100000, 100000), null=r.randrange(4) == 0)))


def accessor_ppc(e, case):
    initial = ppc_case(e, dict(files=[], ops=[]))
    kind = case['kind']
    if kind == 0:
        result = T.call_ppc(e, INSTANCE, [])[0] == SINGLETON
    elif kind in [1, 2]:
        address = 0x10017274 if kind == 1 else 0x100273cc
        result = T.call_ppc(e, address, [SINGLETON])[0] == SINGLETON + (0x1c if kind == 1 else 0x24)
    elif kind == 3:
        manager = e.u32(MANAGER)
        result = T.call_ppc(e, 0x10016a48, [manager])[0] == manager + 0x2c
    else:
        manager = e.u32(MANAGER)
        T.call_ppc(e, 0x10016760, [SINGLETON, 0])
        result = e.u32(MANAGER) == 0
        T.call_ppc(e, 0x10016760, [SINGLETON, manager])
        result &= e.u32(MANAGER) == manager
    return dict(result=bool(result), initial=initial['steps'], log=e.cl.log)


def accessor_host(lib, case):
    initial = host_case(lib, dict(files=[], ops=[]))
    C.c_int32.in_dll(lib, 'tpw_cl_on').value = 1
    return dict(result=bool(lib.tpw_ms_access(case['kind'])), initial=initial['steps'], log=CL.host_end(lib))


for kind, address in enumerate([INSTANCE, 0x10017274, 0x100273cc, 0x10016a48, 0x10016760]):
    TESTS.append(FnTest(f'streamer accessor {address:#x}', accessor_ppc, accessor_host,
                        edge=lambda k=kind: [dict(kind=k)], rnd=lambda r, k=kind: dict(kind=k)))


def destroy_ppc(e, case):
    result = ppc_case(e, case)
    T.call_ppc(e, DTOR, [SINGLETON, 0])
    return dict(steps=result['steps'], log=e.cl.log,
                arrays=[e.u32(SINGLETON + off) != 0 for off in [0xc, 0x1c, 0x24, 0x2c]])


def destroy_host(lib, case):
    result = host_case(lib, case)
    C.c_int32.in_dll(lib, 'tpw_cl_on').value = 1
    arrays = (C.c_uint32 * 4)()
    lib.tpw_ms_destroy.argtypes = [C.c_void_p]
    lib.tpw_ms_destroy(arrays)
    return dict(steps=result['steps'], log=CL.host_end(lib), arrays=[bool(v) for v in arrays])


TESTS.append(FnTest(f'streamer destructor {DTOR:#x}', destroy_ppc, destroy_host,
                    edge=simple_edges, rnd=simple_random))


def block32_case(r):
    result = swap_case(r, 0x10009528, 9, True)
    result['block32'] = True
    return result


TESTS.append(FnTest('USwapBlock32 0x10009528', swap_ppc, swap_host,
                    edge=lambda: [block32_case(random.Random(i)) for i in range(8)], rnd=block32_case))

# Shared banks, failed opens, exact-name reuse, and slot reuse in 300 independently generated scripts.
def mixed_random(r):
    paths = r.sample(['P:', 'Q:', 'Disk:data:sound:', 'ROOT:Global:'], 2)
    files = []
    names = [r.choice(['Sfx', 'Music', 'Dir\\Sfx', '\\first']) for _ in range(r.randrange(1, 4))]
    flags = [r.choice([0, 0x20, 0x21, 0xff]) for _ in names]
    ops = []
    for i, path in enumerate(paths):
        category = f'cat{i}'
        files += [(path, category + 'BANK.map', bank_file(names, flags)),
                  (path, category + 'SFX.map', sfx_file(r, len(names), r.choice([0, 1])))]
        ops += [op(1, first=path, second=category), op(1, first=path, second=category),
                op(5, first=path + category), op(5, first='unknown')]
    ops += [op(3, 1), op(3, 1), op(3, 2), op(3, 2), op(3, 7), op(4, 1), op(4, 2)]
    ops += [op(1, first=paths[0], second='cat0'), op(5, first='')]
    return case(ops, files, {'GetBank': [r.randrange(2) for _ in range(10)]})


def failure_edges():
    raw = sfx_file(random.Random(12), 1)
    files = [('P:', 'catBANK.map', bank_file(['Sfx'])), ('P:', 'catSFX.map', raw)]
    for storage in [[-1], [0, -1], [0, 1, -1], [0, 0, 0]]:
        yield case([op(1, first='P:', second='cat')], files, {'StorageOpen': storage})
    for opening in [[-1], [0, -1], [0, 0]]:
        yield case([op(1, first='P:', second='cat')], files, {'FileOpen': opening})
    for bank, sfx in [(bank_file([], guid=bytes(16)), raw), (bank_file(['Sfx']), bytes(16) + raw[16:]),
                      (bytes(8), bytes(8)), (bank_file([]), sfx_file(random.Random(0), 0, categories=0))]:
        yield case([op(1, first='P:', second='cat')], [('P:', 'catBANK.map', bank), ('P:', 'catSFX.map', sfx)])
    for suffix in ['BANK.map', 'SFX.map', 'SFX.map.more']:
        yield case([op(2, first='P:cat' + suffix)], files)
    for i in range(100):
        yield mixed_random(random.Random(7000 + i))


TESTS.append(FnTest('300 mixed registration scripts, sharing/reuse/failure paths', ppc_case, host_case,
                    edge=failure_edges, rnd=mixed_random))


def real_file_edges():
    root = ROOT / 'extracted/disc-hfs/Theme Park Data/data'
    global_sound = root / 'global/sound'
    if not global_sound.is_dir():
        print('SKIP real sound maps: extracted Theme Park Data folder is absent')
        return
    selected = [global_sound / (name + 'SFX.map') for name in ['cat_ui', 'cat_globallobbysfx', 'cat_rides']]
    levels = root / 'levels'
    themes = sorted(p for p in levels.iterdir() if p.is_dir())[:2] if levels.is_dir() else []
    for theme in themes:
        folders = [p for p in theme.iterdir() if p.is_dir() and p.name.lower() == 'sound']
        for folder in folders:
            selected += [folder / (name + 'SFX.map') for name in ['cat_locallobbysfx', 'cat_locallobbymusic']]
    for sfx in selected:
        bank = sfx.with_name(sfx.name.replace('SFX.map', 'BANK.map'))
        if not sfx.is_file() or not bank.is_file():
            print(f'SKIP real map pair: {sfx.relative_to(root)}')
            continue
        name = sfx.name[:-7]
        print(f'Real map pair: {sfx.relative_to(root)} ({sfx.stat().st_size} bytes)')
        yield case([op(1, first='REAL:', second=name), op(1, first='REAL:', second=name),
                    op(5, first='REAL:' + name), op(3, 1), op(3, 1)],
                   [('REAL:', name + 'BANK.map', bank.read_bytes()), ('REAL:', name + 'SFX.map', sfx.read_bytes())])


TESTS.append(FnTest('real global and two-theme BANK/SFX files', ppc_case, host_case,
                    edge=real_file_edges, rnd=registration_random))


def file_event_ids(raw):
    count = int.from_bytes(raw[24:28], 'little')
    next_ = 28 + count * 24
    ids = []
    for category in range(count):
        cat = 28 + category * 24
        events = int.from_bytes(raw[cat:cat + 4], 'little')
        event_base = next_
        next_ += events * 20
        for i in range(events):
            ev = event_base + i * 20
            event_id, sounds = struct.unpack_from('<II', raw, ev)
            ids.append((category, event_id))
            sound_base = next_
            next_ += sounds * 42
            for j in range(sounds):
                samples, branches = struct.unpack_from('<II', raw, sound_base + j * 42)
                next_ += (samples & 0xffff) * 16 + branches * 8
    return ids


# Add MapEvent operations to the real-file cases: every event, plus zero and an unknown id per category.
original_real_file_edges = real_file_edges

def real_file_edges():
    for item in original_real_file_edges():
        raw = next(raw for _, name, raw in item['files'] if name.endswith('SFX.map'))
        queries = [op(8, 1, str(event), str(category)) for category, event in file_event_ids(raw)]
        count = int.from_bytes(raw[24:28], 'little')
        for category in range(count):
            queries += [op(8, 1, '0', str(category)), op(8, 1, str(MASK), str(category))]
        item['ops'] = item['ops'][:1] + queries + item['ops'][1:]
        yield item


# Replace the real-file entry after extending its operations.
TESTS[-1]._edge = real_file_edges


# Entry-point counts are case counts, not branch coverage. Repeated calls in one case count once.
FUNCTION_ADDRESSES = [
    0x10008db4, 0x10008fd0, 0x10009020, 0x10009078, 0x1000942c, 0x1000947c, 0x100094d4, 0x10009528,
    0x10009588, 0x10013220, 0x10016760, 0x10016a48, 0x10016fc8, 0x100170d0, 0x10017130, 0x100171d8,
    0x10017274, 0x1001ad24, 0x1001ad7c, 0x1001add4, 0x1001ae28, 0x1001ae78, 0x1001af8c, 0x1001b63c,
    0x1001b684, 0x1001b6cc, 0x1001b710, 0x1001b768, 0x1001d8b8, 0x1001d90c, 0x1001d964, 0x1001da34,
    0x1001e4b0, 0x1001f320, 0x1001f410, 0x1001f50c, 0x1001f590, 0x1001f5f0, 0x1001f684, 0x1001f6ec,
    0x1001f784, 0x1001f8e0, 0x1001f954, 0x1001faa4, 0x1001fb14, 0x1001fbc4, 0x1001fc30, 0x1001fccc,
    0x1001fd4c, 0x1001fd84, 0x1001fe14, 0x1001fe94, 0x1001fedc, 0x1001ff70, 0x1001fff4, 0x10020040,
    0x100200dc, 0x1002016c, 0x100201c0, 0x1002020c, 0x1002030c, 0x1002041c, 0x100204ac, 0x1002051c,
    0x1002071c, 0x10020a6c, 0x10020b4c, 0x10020d74, 0x10020f0c, 0x10021160, 0x100211a0, 0x10021298,
    0x100212d4, 0x10021368, 0x100213a0, 0x100214a4, 0x100214fc, 0x100215a0, 0x100215f8, 0x10021668,
    0x10021914, 0x100219c8, 0x10021a34, 0x10021a7c, 0x10021ae4, 0x10021b80, 0x10021be4, 0x10021c48,
    0x10021c90, 0x10021cc4, 0x10021e30, 0x10021ecc, 0x10022060, 0x100220c8, 0x1002213c, 0x10022290,
    0x10022358, 0x10022410, 0x100224f8, 0x1002259c, 0x10022620, 0x1002268c, 0x1002271c, 0x10022788,
    0x100228cc, 0x10022970, 0x10022a28, 0x10022aa0, 0x10022b10, 0x10022c3c, 0x10022cec, 0x10022d5c,
    0x10022e00, 0x10022e78, 0x10022f1c, 0x100236b4, 0x100237a4, 0x10023a30, 0x10023bb0, 0x10023c70,
    0x10026d24, 0x100273cc, 0x1002e6a8, 0x1002e85c,
]

FUNCTION_CASES = {address: 0 for address in FUNCTION_ADDRESSES}
original_setup = setup


def setup(runner):
    original_setup(runner)
    from unicorn import UC_HOOK_CODE
    e = runner.emu
    e.function_hits = set()
    def entry(uc, address, size, user):
        e.function_hits.add(address)
    for address in FUNCTION_ADDRESSES:
        e.uc.hook_add(UC_HOOK_CODE, entry, begin=address, end=address)


def counted(original):
    def run(runner, case):
        runner.emu.function_hits = set()
        result = original(runner, case)
        if not result:
            for address in runner.emu.function_hits:
                FUNCTION_CASES[address] += 1
        return result
    return run


for test in TESTS:
    test.run_case = counted(test.run_case)




def qualified_random(r):
    path = r.choice(['P:', 'Q:', 'Disk:data:sound:', 'A:B::', ':'])
    name = r.choice(['cat', 'a', 'ui', 'with space', 'catSFX.map_extra'])
    qualified = path + name + r.choice(['SFX.map', 'SFX.map.more', 'BANK.map', '', 'sfx.map'])
    names = [r.choice(['Sfx', 'Dir\\Music']) for _ in range(r.randrange(0, 4))]
    files = [(path, name + 'BANK.map', bank_file(names)), (path, name + 'SFX.map', sfx_file(r, len(names)))]
    return case([op(2, first=qualified), op(2, first=qualified), op(5, first=path + name), op(3, 1)], files)


TESTS.append(FnTest('qualified RegisterCatagory 0x1002051c', ppc_case, host_case,
                    edge=registration_edges, rnd=qualified_random))
TESTS.append(FnTest('USwapInplace32 0x1001e4b0', swap_ppc, swap_host,
                    edge=lambda: [swap_case(random.Random(0), 0x1001e4b0, 9)],
                    rnd=lambda r: swap_case(r, 0x1001e4b0, 9)))
for test in TESTS[-2:]:
    test.run_case = counted(test.run_case)


def wrapping_branch_edges():
    for target in [0, 1, MASK]:
        category = struct.pack('<IIHHIII', 1, 0, 0, 0, 0, 0, 0)
        event = struct.pack('<IIIIHH', 1, 1, 0, 0, 0, 0)
        sound = bytearray(b'\xa5' * 42)
        struct.pack_into('<III', sound, 0, 0, 1, 0)
        branch = struct.pack('<IHBB', target, 0x1234, 0x56, 0x78)
        raw = SFX_GUID + struct.pack('<III', 1, 0, 1) + category + event + bytes(sound) + branch
        yield case([op(1, first='P:', second='cat')], [('P:', 'catBANK.map', bank_file([])), ('P:', 'catSFX.map', raw)])


test = FnTest('ReadEvents 32-bit branch-index wrapping', ppc_case, host_case,
              edge=wrapping_branch_edges, rnd=registration_random)
test.run_case = counted(test.run_case)
TESTS.append(test)

# Direct readers compare the final cursor, the ReadSampleData out parameter, and every parsed field.
def reader_random(r, kind):
    sounds = 1 if kind == 0 else r.randrange(0, 4)
    category = struct.pack('<IIHHIII', 1, r.getrandbits(32), r.getrandbits(16), r.getrandbits(16),
                           r.getrandbits(32), r.getrandbits(32), r.getrandbits(32))
    event = struct.pack('<IIIIHH', r.getrandbits(32), sounds, r.getrandbits(32), r.getrandbits(32),
                        r.getrandbits(16), r.getrandbits(16))
    records = []
    tails = []
    for _ in range(sounds):
        samples, branches = r.randrange(0, 4), r.randrange(0, 3)
        sound = bytearray(r.randbytes(42))
        struct.pack_into('<II', sound, 0, r.getrandbits(16) << 16 | samples, branches)
        records.append(bytes(sound))
        sample_rows = [struct.pack('<IIIHH', r.getrandbits(32), r.getrandbits(32), r.getrandbits(32), r.randrange(4),
                                   r.getrandbits(16)) for _ in range(samples)]
        branch_rows = [struct.pack('<IHBB', r.randrange(1, sounds + 1), r.getrandbits(16), r.randrange(256), r.randrange(256))
                       for _ in range(branches)]
        tails.append(b''.join(sample_rows + branch_rows))
    raw = SFX_GUID + struct.pack('<III', 1, 0, 1) + category + event + b''.join(records + tails)
    return dict(kind=kind, raw=raw, old=r.choice([0, 1, 0xa5]),
                indices=[r.choice([0, 1, 7, 0xffff, 0x10000, MASK, r.getrandbits(32)]) for _ in range(3)])


def reader_ppc(e, case):
    ppc_case(e, dict(files=[], ops=[]))
    ws(e, SINGLETON + 0x38, [0])
    e.write(SINGLETON + 0x3c, bytes([case['old']]))
    for i, value in enumerate(case['indices']):
        slot = T.call_ppc(e, 0x100211a0, [SINGLETON + 0x2c, i])[0]
        ws(e, slot, [value])
    raw = bytearray(case['raw'][28:])
    kind = case['kind']
    for offset, shape in ([(0, 4), (24, 5), (44, 6)] if kind == 0 else [(0, 4), (24, 5)] if kind == 1 else []):
        flip_record(raw, offset, shape)
    block = e.alloc(len(raw) + 16, bytes(raw) + b'\xa5' * 16)
    if kind < 2:
        ws(e, block + 4, [block + 24])
    if kind == 0:
        ws(e, block + 32, [block + 44])
    out = e.alloc(4, struct.pack('>I', 0xdeadbeef))
    if kind == 0:
        address, args = 0x10022b10, [SINGLETON, block + 86, block + 44]
    elif kind == 1:
        address, args = 0x10022788, [SINGLETON, block + 44, block + 24]
    elif kind == 2:
        address, args = 0x10022410, [SINGLETON, block + 24, block]
    else:
        address, args = 0x10022290, [SINGLETON, block, out, 1]
    end = T.call_ppc(e, address, args)[0]
    return dict(end=end - block, output=e.u32(out) - block if kind == 3 else 0xdeadbeef,
                table=parsed(e.read, block, 'big', rebased=kind != 0), state=ppc_state(e), log=e.cl.log)


def reader_host(lib, case):
    host_case(lib, dict(files=[], ops=[]))
    C.c_int32.in_dll(lib, 'tpw_cl_on').value = 1
    indices = (C.c_uint32 * 3)(*case['indices'])
    lib.tpw_ms_reader_state.argtypes = [C.c_uint32, C.c_void_p, C.c_uint32]
    lib.tpw_ms_reader_state(case['old'], indices, 3)
    kind = case['kind']
    raw = bytearray(host_file(case['raw'])[28:])
    for offset, shape in ([(0, 4), (24, 5), (44, 6)] if kind == 0 else [(0, 4), (24, 5)] if kind == 1 else []):
        size = SHAPES[shape][0]
        raw[offset:offset + size] = case['raw'][28 + offset:28 + offset + size]
    block = lib.tpw_ms_block(bytes(raw) + b'\xa5' * 16, len(raw) + 16)
    arena = lib.tpw_ms_object(6, 0)
    if kind < 2:
        C.c_uint32.from_address(block + 4).value = block + 24 - arena
    if kind == 0:
        C.c_uint32.from_address(block + 32).value = block + 44 - arena
    output = C.c_uint32(0xdeadbeef)
    lib.tpw_ms_reader.argtypes = [C.c_uint32, C.c_void_p, C.c_uint32, C.c_void_p]
    lib.tpw_ms_reader.restype = C.c_uint32
    end = lib.tpw_ms_reader(kind, block, 1, C.byref(output))
    result = dict(end=end, output=output.value, table=parsed(C.string_at, block, 'little', arena, rebased=kind != 0),
                  state=host_state(lib), log=CL.host_end(lib))
    lib.tpw_ms_block_free(block)
    return result


for kind, address in enumerate([0x10022b10, 0x10022788, 0x10022410, 0x10022290]):
    test = FnTest(f'direct reader cursor/out parameter {address:#x}', reader_ppc, reader_host,
                  edge=lambda k=kind: [reader_random(random.Random(10), k)], rnd=lambda r, k=kind: reader_random(r, k))
    test.run_case = counted(test.run_case)
    TESTS.append(test)
