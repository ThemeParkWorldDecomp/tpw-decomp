"""Differential tests for the complete u8a image-loader range.
External pixel conversion and wavelet operations are call-log fakes on both sides.
"""
import ctypes as C
import struct
import calllog as CL
import tpwtest as T
import u8a_support as S
from u8a_support import fn, VP, I, U, w32, r32, G
PROGRAM = 'engine symbols shared'
SHIM_LIBRARY = 'unit_tb_loaders'
ADDR = [0x10014e68, 0x10014f38, 0x10014fc4, 0x10015194, 0x10015328,
    0x100156d8, 0x10015b08, 0x10015d2c, 0x10015db8, 0x10015dfc, 0x10015ef4,
    0x10015f9c, 0x10016280, 0x10016324]
setup = S.setup

# Call a PowerPC entry point with unsigned register arguments.
def call(e, addr, args=()):
    return T.call_ppc(e, addr, [T.u32(v) for v in args])[0]

# Generate a deterministic randomized fixture.
def case(r):
    w = r.choice([0, 1, 2, 3, 4, 5, 8, 9])
    h = r.choice([0, 1, 2, 3, 4, 5, 8, 9])
    bits = r.choice([16, 24, 32])
    mode = r.choice([-1, 0, 1, 2, 3, 4])
    extra = 2 if mode == 1 else 1 if mode == 3 else 0
    header = struct.pack('<8B4h2B', r.randrange(256), r.randrange(256),
        r.choice([1, 2, 2, 2, 3]), *[r.randrange(256) for _ in range(5)],
        r.randrange(-32768, 32768), r.randrange(-32768, 32768), w, h, bits,
        r.choice([0, 32, 255]))
    c = dict(file=header + bytes((r.randrange(256) for _ in range(w * h * (bits // 8) + w * h))),
        mode=mode, format=r.randrange(6), extra=extra, avg=r.getrandbits(32),
        flags=r.getrandbits(32), db=r.choice([-1, 0, 1, 2, 3, 4, 5]), sb=r.choice([-1,
        0, 1, 2, 3, 4, 5]), flip=r.randrange(2), name=r.choice([None, 'image.tga',
        '...image.longext', 'image.x', 'image.', 'image', '..', '.hidden',
        'dir:name.tga']), loadflags=r.choice([0, 1, 2, 4, 8, 6, 10, 12]),
        count=r.randrange(4), default=r.choice([-1, 0]), folder=r.choice([None,
        '', 'folder', 'folder:']), lookup=r.randrange(7), existed=r.randrange(2),
        q={'Open': [r.choice([0, 1, 1, 1])], 'Malloc': [r.choice([0, 1, 1,
        1]), r.choice([0, 1, 1, 1])], 'Wave.Load': [r.choice([0, 0, 0, 1])],
        'Wave.width': [w], 'Wave.height': [h], 'Wave.alpha': [r.choice([0,
        1, 3])], 'Wave.threshold': [r.choice([0, 1, 255])], 'Wave.level': [r.choice([0,
        1, 2, 3, 4])], 'Averages': [r.getrandbits(32)], 'AlphaCount': [r.randrange(-1,
        10)]})
    return c

# Exercise boundary values and each external failure.
def edges(kind):
    import random
    r = random.Random(93)
    base = case(r)
    base.update(file=struct.pack('<8B4h2B', 0, 0, 2, 0, 0, 0, 0, 0,
        0, 0, 3, 3, 32, 0) + bytes(range(45)), name='image.tga', db=2, sb=4,
        mode=1, extra=2, loadflags=2, default=0, count=1)
    base['q'] = {'Open': [1], 'Malloc': [1], 'Wave.Load': [0], 'AlphaCount': [0]}
    yield base
    for mode in [-1, 0, 1, 2, 3, 4]:
        yield dict(base, mode=mode)
    for n in [None, '', '.', '..', '.abc', 'name', 'a.x', 'a.xy', 'a.xyz',
        'a.long', '.dir:t.tga', 'a.']:
        yield dict(base, name=n)
    for k in ['Open', 'Wave.Load', 'Malloc']:
        for q in [[0], [1], [1, 0]]:
            yield dict(base, q=dict(base['q'], **{k: q}))
    for tell in [-1, 0, 1, 36, 37, 100]:
        yield dict(base, q=dict(base['q'], Tell=[tell]))
    for db in [-1, 0, 1, 2, 3, 4, 5]:
        for sb in [-1, 0, 1, 2, 3, 4, 5]:
            yield dict(base, db=db, sb=sb)
    for count in [0, 1, 3, 4096]:
        for lookup in range(7):
            yield dict(base, count=count, lookup=lookup)
    yield dict(base, q=dict(base['q'], Name=[0]))
    if kind in ['Load', 'Bump']:
        for (width, height, bits) in [(-1, 1, 16), (1, -1, 32), (-32768,
            -32768, 32), (32767, 32767, 32), (0, 5, 24)]:
            header = struct.pack('<8B4h2B', 0, 0, 2, 0, 0, 0, 0, 0,
                -1, -32768, width, height, bits, 0)
            yield dict(base, file=header, q=dict(base['q'], Malloc=[0]))
    if kind == 'TGALoad':
        header = struct.pack('<8B4h2B', 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0)
        yield dict(base, file=header, extra=-1, loadflags=0)

# Decode header fields in the supplied byte order.
def header_values(b, endian):
    return list(struct.unpack(endian + '8B4h2B', b))

# Snapshot original entry flags, pitch, and pixels.
def entry_ppc(e, p, n):
    if not p:
        return []
    return [(e.u32(p + i * 92 + 8), struct.unpack('>h', e.read(p + i * 92 + 48,
        2))[0], S.ppc_buffer(e, e.u32(p + i * 92 + 60))) for i in range(n)]

# Snapshot translated entry flags, pitch, and pixels.
def entry_host(lib, p, n):
    if not p:
        return []
    out = []
    for i in range(n):
        flags = U()
        pitch = I()
        data = VP()
        fn(lib, 'tpw_entry_get', [VP, I, VP, VP, VP], None)(p, i, C.byref(flags),
            C.byref(pitch), C.byref(data))
        out.append((flags.value, pitch.value, S.host_buffer(lib, data.value)))
    return out

# Call the original and collect observable state.
def run_ppc(kind, e, c):
    c = defined_case(kind, c)
    (L, draw, _) = S.prepare(e, c, ADDR)
    try:
        name = S.text(e, c['name'])
        out = {}
        if kind in ['Load', 'Bump', 'Emboss', 'TGALoad', 'Convert']:
            h = e.alloc(18, b'\xa5' * 18)
            w = e.alloc(4, struct.pack('>i', -555))
            hh = e.alloc(4, struct.pack('>i', -777))
            if kind in ['Load', 'Bump']:
                p = call(e, ADDR[2 if kind == 'Load' else 3], [h, name,
                    c['extra']] if kind == 'Load' else [h, name])
                out.update(result=bool(p), header=header_values(e.read(h,
                    18), '>'), buffer=S.ppc_buffer(e, p))
            elif kind == 'Emboss':
                vals = header_values(c['file'][:18], '<')
                e.write(h, struct.pack('>8B4h2B', *vals))
                n = vals[10] * vals[11] * max(4, vals[12] // 8) * 3 + 64
                data = bytes((i * 17 + 13 & 255 for i in range(n)))
                p = e.alloc(n, data)
                call(e, ADDR[4], [h, p])
                out['buffer'] = e.read(p, n)
            elif kind == 'TGALoad':
                p = call(e, ADDR[5], [name, w, hh, c['loadflags'],
                    0, c['db'] if c['db'] in [0, 2, 3, 4] else 2])
                out.update(result=bool(p), width=r32(e, w), height=r32(e,
                    hh), buffer=S.ppc_buffer(e, p))
            else:
                vals = header_values(c['file'][:18], '<')
                (width, height) = vals[10:12]
                src = e.alloc(8192, bytes((i * 13 + 7 & 255 for i in range(8192))))
                dest = e.alloc(8192, b'\xa5' * 8192)
                L.labels[src] = 10
                L.labels[dest] = 11
                out['result'] = call(e, ADDR[6], [dest, src, width,
                    height, 0, c['db'], c['sb'], c['flip']])
                out['buffer'] = e.read(dest, 8192)
        else:
            manager = e.alloc(65548, b'\x00' * 65548)
            w32(e, manager + 65540, -1)
            count = c['count'] if kind == 'LoadTexture' else min(c['count'], 3)
            w32(e, manager + 65536, count)
            w32(e, manager + 65540, c['default'])
            w32(e, manager + 65544, c['loadflags'])
            folder = S.text(e, c['folder'])
            nm = S.text(e, 'texture.tga')
            L.labels[folder] = 20
            L.labels[nm] = 21
            oldfolder = S.text(e, (c['folder'] if c['folder'] is not None else 'folder') if c['lookup'] in [3,
                6] else 'otherfolder')
            for i in range(count):
                w32(e, manager + i * 16, folder if c['lookup'] == 1 else oldfolder if c['lookup'] >= 3 else 0)
                w32(e, manager + i * 16 + 4, nm if c['lookup'] in [1,
                    2, 3, 4] else S.text(e, 'other.tga'))
                w32(e, manager + i * 16 + 8, 0)
                w32(e, manager + i * 16 + 12, 10 + i)
                if kind == 'LoadTexture' and c['lookup'] % 2:
                    entry = e.alloc(92)
                    if i < 482:
                        L.labels[entry] = 30 + i
                    w32(e, manager + i * 16 + 8, entry)
                if kind == 'ManagerClose' and c['lookup'] % 2:
                    n = 1 + i % 3
                    entry = fixture_ppc(e, L, n, 200 + i * 4)
                    w32(e, manager + i * 16 + 8, entry)
            if kind == 'ManagerCtor':
                e.write(manager, b'\xa5' * 65548)
                call(e, ADDR[7], [manager])
                out['zero'] = e.read(manager, 65536) == b'\x00' * 65536
            elif kind == 'SLoadCtor':
                p = e.alloc(16, b'\xa5' * 16)
                call(e, ADDR[8], [p])
                out['slot'] = list(struct.unpack('>4I', e.read(p, 16)))
            elif kind == 'GlobalInit':
                w32(e, G['manager'], manager if c['existed'] else 0)
                out['result'] = call(e, ADDR[0])
                manager = e.u32(G['manager'])
            elif kind == 'GlobalClose':
                w32(e, manager + 65536, 0)
                w32(e, G['manager'], manager if c['existed'] else 0)
                out['result'] = call(e, ADDR[1])
                out['global'] = bool(e.u32(G['manager']))
            elif kind == 'ManagerInit':
                out['result'] = call(e, ADDR[9], [manager])
            elif kind == 'ManagerClose':
                out['result'] = call(e, ADDR[10], [manager])
            elif kind == 'LoadFile':
                p = call(e, ADDR[11], [manager, name])
                out['result'] = bool(p)
                out['entries'] = entry_ppc(e, p, e.u32(G['extra']) + 1)
            elif kind == 'SLoadClose':
                p = e.alloc(16, b'\x00' * 16)
                w32(e, p + 12, 123)
                if c['lookup'] % 2:
                    w32(e, p + 8, fixture_ppc(e, L, 1 + c['count'] % 3, 10))
                out['result'] = call(e, ADDR[12], [p])
                out['slot'] = list(struct.unpack('>4I', e.read(p, 16)))
            elif kind == 'LoadTexture':
                out['result'] = L.lab(call(e, ADDR[13], [manager, folder, nm]))
            if kind not in ['SLoadCtor', 'SLoadClose', 'GlobalClose']:
                out['manager'] = [r32(e, manager + 65536), r32(e, manager + 65540),
                    e.u32(manager + 65544)]
                n = min(out['manager'][0] + 1, 5)
                out['slots'] = [(L.lab(e.u32(manager + i * 16)), L.lab(e.u32(manager + i * 16 + 4)),
                    L.lab(e.u32(manager + i * 16 + 8)), r32(e, manager + i * 16 + 12)) for i in range(max(0,
                    n))]
        out['globals'] = S.globals_ppc(e)
        out['log'] = L.log
        return out
    finally:
        S.finish(e)

# Call the translation and collect matching state.
def run_host(kind, lib, c):
    c = defined_case(kind, c)
    S.host_prepare(lib, c)
    out = {}
    name = c['name'].encode() if c['name'] is not None else None
    if kind in ['Load', 'Bump', 'Emboss', 'TGALoad', 'Convert']:
        h = C.create_string_buffer(b'\xa5' * 18, 18)
        w = I(-555)
        hh = I(-777)
        if kind in ['Load', 'Bump']:
            p = fn(lib, 'tpw_TGA_Load' if kind == 'Load' else 'tpw_TGA_Bump',
                [VP, VP, I] if kind == 'Load' else [VP, VP], VP)(h, name, c['extra']) if kind == 'Load' else fn(lib,
                'tpw_TGA_Bump', [VP, VP], VP)(h, name)
            out.update(result=bool(p), header=header_values(h.raw,
                '<'), buffer=S.host_buffer(lib, p))
        elif kind == 'Emboss':
            vals = header_values(c['file'][:18], '<')
            C.memmove(h, c['file'][:18], 18)
            n = vals[10] * vals[11] * max(4, vals[12] // 8) * 3 + 64
            data = C.create_string_buffer(bytes((i * 17 + 13 & 255 for i in range(n))), n)
            fn(lib, 'tpw_TGA_Emboss', [VP, VP], None)(h, data)
            out['buffer'] = data.raw
        elif kind == 'TGALoad':
            p = fn(lib, 'tpw_TGALoad', [VP, VP, VP, I, I], VP)(name,
                C.byref(w), C.byref(hh), c['loadflags'], c['db'] if c['db'] in [0,
                2, 3, 4] else 2)
            out.update(result=bool(p), width=w.value, height=hh.value, buffer=S.host_buffer(lib, p))
        else:
            vals = header_values(c['file'][:18], '<')
            (width, height) = vals[10:12]
            src = C.create_string_buffer(bytes((i * 13 + 7 & 255 for i in range(8192))), 8192)
            dest = C.create_string_buffer(b'\xa5' * 8192, 8192)
            lib.tpw_cl_label(10, src)
            lib.tpw_cl_label(11, dest)
            out['result'] = fn(lib, 'tpw_Convert', [VP, VP, I, I, I,
                I, I], U)(dest, src, width, height, c['db'], c['sb'], c['flip'])
            out['buffer'] = dest.raw
    else:
        fn(lib, 'tpw_loader_ctor', [], VP)()
        count = c['count'] if kind == 'LoadTexture' else min(c['count'], 3)
        fn(lib, 'tpw_loader_set', [I, I, U], None)(count, c['default'], c['loadflags'])
        folder = C.create_string_buffer(c['folder'].encode()) if c['folder'] is not None else None
        nm = C.create_string_buffer(b'texture.tga')
        other = C.create_string_buffer(b'other.tga')
        if folder:
            lib.tpw_cl_label(20, folder)
        lib.tpw_cl_label(21, nm)
        oldfolder = C.create_string_buffer(((c['folder'] if c['folder'] is not None else 'folder') if c['lookup'] in [3,
            6] else 'otherfolder').encode())
        for i in range(count):
            entry = fixture_host(lib, 1 + i % 3, 200 + i * 4) if kind == 'ManagerClose' and c['lookup'] % 2 else None
            if kind == 'LoadTexture' and c['lookup'] % 2:
                entry = fn(lib, 'tpw_loader_testentry', [I], VP)(i)
                if i < 482:
                    lib.tpw_cl_label(30 + i, entry)
            fn(lib, 'tpw_loader_slot', [I, VP, VP, VP, I], None)(i,
                folder if c['lookup'] == 1 else oldfolder if c['lookup'] >= 3 else None,
                nm if c['lookup'] in [1, 2, 3, 4] else other, entry, 10 + i)
        if kind == 'ManagerCtor':
            fn(lib, 'tpw_loader_ctor', [], VP)()
            out['zero'] = bool(fn(lib, 'tpw_loader_zero', [])())
        elif kind == 'SLoadCtor':
            slot = C.create_string_buffer(b'\xa5' * 32, 32)
            fn(lib, 'tpw_SLoad_ctor', [VP], None)(slot)
            out['slot'] = list(struct.unpack('<3Qi4x', slot.raw))
        elif kind == 'GlobalInit':
            out['result'] = fn(lib, 'tpw_TextureLoadManager_Init', [I], VP)(c['existed']) or 0
        elif kind == 'GlobalClose':
            out['result'] = fn(lib, 'tpw_TextureLoadManager_Close', [I])(c['existed'])
            gg = (I * 5)()
            fn(lib, 'tpw_loader_global', [VP], None)(gg)
            out['global'] = bool(gg[0])
        elif kind == 'ManagerInit':
            out['result'] = fn(lib, 'tpw_Manager_Init', [])()
        elif kind == 'ManagerClose':
            out['result'] = fn(lib, 'tpw_Manager_Close', [])()
        elif kind == 'LoadFile':
            p = fn(lib, 'tpw_Manager_LoadFile', [VP], VP)(name)
            out['result'] = bool(p)
            gg = (I * 5)()
            fn(lib, 'tpw_loader_global', [VP], None)(gg)
            out['entries'] = entry_host(lib, p, gg[1] + 1)
        elif kind == 'SLoadClose':
            entry = fixture_host(lib, 1 + c['count'] % 3, 10) if c['lookup'] % 2 else 0
            slot = C.create_string_buffer(struct.pack('<3Qi4x', 0, 0, entry or 0, 123), 32)
            out['result'] = fn(lib, 'tpw_SLoad_Close', [VP])(slot)
            out['slot'] = list(struct.unpack('<3Qi4x', slot.raw))
        elif kind == 'LoadTexture':
            out['result'] = fn(lib, 'tpw_u8_label', [VP])(fn(lib, 'tpw_Manager_LoadTexture',
                [VP, VP], VP)(folder, nm))
        if kind not in ['SLoadCtor', 'SLoadClose', 'GlobalClose']:
            m = (I * 3)()
            fn(lib, 'tpw_loader_get', [VP], None)(m)
            out['manager'] = [m[0], m[1], m[2] & 0xffffffff]
            n = min(m[0] + 1, 5)
            out['slots'] = []
            for i in range(max(0, n)):
                v = (I * 4)()
                fn(lib, 'tpw_loader_getslot', [I, VP], None)(i, v)
                out['slots'].append(tuple(v))
    out['globals'] = S.globals_host(lib)
    out['log'] = CL.host_end(lib)
    return out
KINDS = ['GlobalInit', 'GlobalClose', 'Load', 'Bump', 'Emboss', 'TGALoad',
    'Convert', 'ManagerCtor', 'SLoadCtor', 'ManagerInit', 'ManagerClose',
    'LoadFile', 'SLoadClose', 'LoadTexture']

# Build a labeled original texture-entry array.
def fixture_ppc(e, L, n, label):
    raw = e.alloc(8 + n * 92, b'\x00' * (8 + n * 92))
    w32(e, raw, 92)
    w32(e, raw + 4, n)
    p = raw + 8
    for i in range(n):
        L.labels[p + i * 92] = label + i
        w32(e, p + i * 92 + 8, 8388608 if i < n - 1 else 0)
    return p

# Build the matching native texture-entry array.
def fixture_host(lib, n, label):
    C.c_int32.in_dll(lib, 'tpw_cl_on').value = 0
    p = fn(lib, 'tpw_entry_new', [I], VP)(n)
    for i in range(n):
        a = fn(lib, 'tpw_entry_at', [VP, I], VP)(p, i)
        lib.tpw_cl_label(label + i, a)
        fn(lib, 'tpw_entry_set', [VP, I, U, VP], None)(p, i, 8388608 if i < n - 1 else 0, None)
    C.c_int32.in_dll(lib, 'tpw_cl_on').value = 1
    return p

# Keep manager bump failures away from undefined dimensions.
def defined_case(kind, c):
    if kind in ['ManagerInit', 'LoadFile', 'LoadTexture'] and c['loadflags'] & 1:
        return dict(c, q=dict(c['q'], Open=[0]))
    return c
TESTS = [T.FnTest(k, lambda e, c, k=k: run_ppc(k, e, c), lambda lib,
    c, k=k: run_host(k, lib, c), edge=lambda k=k: edges(k), rnd=case) for k in KINDS]
