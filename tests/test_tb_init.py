"""Engine startup and procedural texture cache differential tests.
The rest of the engine and the terrain generator are identical call-log fakes.
"""
import ctypes as C
import random
import struct
import calllog as CL
import tpwtest as T
import u8a_support as S
from u8a_support import fn, VP, I, U, w32, r32
PROGRAM = 'engine symbols shared'
SHIM_LIBRARY = 'unit_tb_init'
setup = S.setup
ADDR = [0x1001275c, 0x100127f0, 0x1001287c, 0x10012b98, 0x10012c94,
    0x10012d98, 0x1001310c, 0x10013298, 0x10013ae0, 0x10013bbc]
EXTERNAL = {'CreateSineTable': 0x10038bf8, 'Gamut_Init': 0x10009904,
    'Gamut_Close': 0x100099d0, 'Matrix_Init': 0x1001a770, 'Matrix_Close': 0x1001a854,
    'DrawPrimitiveOpenGL_Init': 0x10042c70, 'DrawPrimitiveOpenGL_Close': 0x10042d60,
    'RenderMemory_Init': 0x100269d4, 'RenderMemory_Close': 0x10026ac0,
    'LightManager_Init': 0x10014278, 'LightManager_Close': 0x10014334,
    'RenderList_Init': 0x10024650, 'RenderList_Close': 0x10024704, 'HField_Init': 0x1000bca4,
    'HField_Close': 0x1000bd60, 'MapWho_Init': 0x10017318, 'MapWho_Close': 0x100173d0,
    'Switch_Init': 0x100326f0, 'Switch_Close': 0x100327a8, 'Clipping_Init': 0x1003f3bc,
    'Clipping_Close': 0x1003f45c, 'SpriteManager_Init': 0x10031238, 'SpriteManager_Close': 0x100312f8,
    'Particle_Init': 0x1001f21c, 'Particle_Close': 0x1001f35c, 'TCache_Init': 0x1003377c,
    'TCache_Close': 0x10033854, 'TextureLoadManager_Init': 0x10014e68,
    'TextureLoadManager_Close': 0x10014f38, 'Rain_Init': 0x10020458, 'Rain_Close': 0x1002051c,
    'Sky.Init': 0x1002ab70, 'InitWaveLoad': 0x10005c84, 'CloseWaveLoad': 0x10005d68,
    'Rain.Wind': 0x100210c4, 'UVFixup': 0x10029190, 'System.Debug': 0x100287a4,
    'Display.Debug': 0x10028718, 'Texture.Debug': 0x100291e4, 'HField.LoadProc': 0x1000d96c,
    'Landscape.ctor': 0x10001188, 'Landscape.dtor': 0x1000121c, 'Landscape.Size': 0x100013e8,
    'Landscape.Load': 0x10002994, 'Landscape.Tile': 0x10001adc, 'ConvertSquare': 0x10003454,
    'Cache.Remove': 0x10034610, 'Cache.Prepare': 0x10032ddc, 'Error': 0x10008904}

# Call a PowerPC entry point with unsigned register arguments.
def call(e, addr, args=()):
    return T.call_ppc(e, addr, [T.u32(v) for v in args])[0]

# Generate a deterministic randomized fixture.
def case(r):
    size = r.choice([0, 1, 2, 3, 4, 9, 16, 32, 64])
    c = dict(renderer=r.choice([-1, 0, 1, 3, 4]), display=r.getrandbits(32),
        system=r.getrandbits(32), memory=r.choice([0, 50331647, 50331648,
        67108863, 67108864, 0x80000000]), texmemory=r.choice([0, 6291456 - 1,
        6291456, 10485760, 20971520]), format=r.randrange(6), texture_flags=r.getrandbits(32),
        logger=r.randrange(2), count=r.randrange(0, 7), size=size, name=r.choice([None,
        'terrain.lnd', '']), version=r.getrandbits(32), preinit=r.randrange(3),
        dtorflag=r.choice([-1, 0, 1]), q={'Entry.Create': [r.choice([0, 1,
        1]), r.choice([0, 1, 1])], 'Landscape.Load': [r.choice([0, 1, 255,
        256, -1])], 'Draw.Init': [r.choice([0, 1, -1])]}, cached=r.randrange(4),
        n=r.randrange(0, 5), overlay=r.randrange(2))
    if r.random() < 0.6:
        c['q'][r.choice([n for n in EXTERNAL if n.endswith('_Init') or n in ['Sky.Init',
            'InitWaveLoad']])] = [0]
    return c

# Exercise boundary values and each external failure.
def edges():
    b = case(random.Random(47))
    b.update(renderer=3, memory=67108864, texmemory=20971520, format=4,
        count=4, size=3, n=4, name='terrain.lnd', logger=0)
    b['q'] = {'tile_size': [3]}
    yield b
    for name in EXTERNAL:
        if name.endswith('_Init') or name in ['Sky.Init', 'InitWaveLoad']:
            yield dict(b, q={name: [0]})
    for renderer in [-1, 0, 1, 3, 4]:
        yield dict(b, renderer=renderer)
    for count in [0, 1, 2, 3, 4, 6]:
        for size in [0, 1, 2, 3, 4, 9, 16, 32, 64]:
            yield dict(b, count=count, size=size)
    for mem in [50331647, 50331648, 67108863, 67108864]:
        for tm in [6291455, 6291456, 10485759, 10485760]:
            yield dict(b, memory=mem, texmemory=tm, size=64)
    for queue in [[0], [1], [1, 0], [1, 1, 0]]:
        yield dict(b, q={'Entry.Create': queue})
    for cached in range(4):
        for overlay in [0, 1]:
            yield dict(b, cached=cached, overlay=overlay)
    for value in [0, 1, 255, 256, -1]:
        yield dict(b, q={'Landscape.Load': [value]})
    for name in [None, '', 'a.lnd']:
        yield dict(b, name=name)
    for preinit in range(3):
        yield dict(b, preinit=preinit)
    for flag in [-1, 0, 1]:
        yield dict(b, dtorflag=flag)

# Seed startup state and install procedural texture fakes.
def begin_ppc(e, c):
    w32(e, 0x10052b40, c['version'] ^ 0xa55a73c1)
    (L, draw, gl) = S.prepare(e, c, ADDR)
    st = CL.Stubs()
    w32(e, S.SYSTEM + 28, c['renderer'])
    w32(e, S.SYSTEM, c['system'])
    w32(e, S.SYSTEM + 24, c['memory'])
    w32(e, S.TEXTURE + 20, c['texmemory'])
    w32(e, S.DISPLAY, c['display'])
    hfield = e.alloc(4)
    rain = e.alloc(4)
    cache = e.alloc(4)
    scratch = e.alloc(64 * 64 * 4)
    for (a, l) in [(hfield, 6), (0x101661ec, 7), (rain, 8), (cache, 9), (scratch, 11)]:
        L.labels[a] = l
    w32(e, 0x100af11c, hfield)
    w32(e, 0x100b1628, rain)
    w32(e, 0x10173184, cache)
    w32(e, 0x10052b00, scratch)
    logger = e.alloc(28)
    L.labels[logger] = 10
    w32(e, e.data_import('gLogger__13SamsUtilities'), logger if c['logger'] else 0)
    for (name, addr) in EXTERNAL.items():
        spec = {'TCache_Init': 'i', 'Sky.Init': 'L', 'Rain.Wind': 'Lff',
            'UVFixup': 'L', 'System.Debug': 'L', 'Display.Debug': 'L', 'Texture.Debug': 'L',
            'HField.LoadProc': 'Lsi', 'Landscape.ctor': 'N', 'Landscape.dtor': 'L',
            'Landscape.Size': 'Li', 'Landscape.Load': 'Ls', 'Cache.Remove': 'LL',
            'Cache.Prepare': 'LLf', 'Error': 'xii'}.get(name, '')
        st.internal(addr, CL.recorder(name, spec, ret=lambda e, L, n=name: L.ret(n, 1)))

    # Label and initialize the fake terrain generator.
    def landscape_ctor(e):
        a = e.r(3)
        L.rec('Landscape.ctor', (L.newlab(a),))
        e.write(a, b'\x00' * 684)

    # Record destruction and forget the terrain label.
    def landscape_dtor(e):
        L.rec('Landscape.dtor', (L.lab(e.r(3)),))
        L.forget(e.r(3))
    st.internal(EXTERNAL['Landscape.ctor'], landscape_ctor)
    st.internal(EXTERNAL['Landscape.dtor'], landscape_dtor)

    # Generate deterministic ARGB bytes and record the overlay.
    def tile(e):
        (out, x, y, ov) = (e.r(4), e.r(5), e.r(6), e.r(7))
        L.rec('Landscape.Tile', (L.lab(e.r(3)), L.lab(out), x, y, L.lab(e.u32(ov)),
            r32(e, ov + 4), r32(e, ov + 8), r32(e, ov + 12), e.u32(ov + 16)))
        size = L.ret('tile_size', 3)
        e.write(out, bytes((i * 37 + x * 5 + y * 7 & 255 for i in range(size * size * 4))))

    # Record conversion and fill deterministic 16-bit pixels.
    def square(e):
        (d, s, size) = (e.r(3), e.r(4), e.r(5))
        L.rec('ConvertSquare', (L.lab(d), L.lab(s), T.s32(size)))
        e.write(d, bytes((v ^ 90 for v in e.read(s, size * size * 2))))
    st.internal(EXTERNAL['Landscape.Tile'], tile)
    st.internal(EXTERNAL['ConvertSquare'], square)
    code = 0x1004b080
    st.internal(code, CL.recorder('Draw.Init', 'L', ret='i'))
    tv = e.alloc(8, struct.pack('>II', code, e.toc))
    vt = e.alloc(16)
    w32(e, vt + 12, tv)
    w32(e, draw + 44, vt)
    w32(e, gl + 44, vt)
    saved = st.activate(e)
    e.u8saved = (e.u8saved[0], e.u8saved[1])
    e.u8second = saved
    return L

# Restore procedural texture and shared external fakes.
def end_ppc(e):
    CL.Stubs.deactivate(e, e.u8second)
    S.finish(e)

# Snapshot original startup globals.
def state_ppc(e):
    return [e.u32(S.DISPLAY), e.u32(S.TEXTURE), e.cl.lab(e.u32(0x10052b3c)),
        bool(e.u32(e.data_import('gLogger__13SamsUtilities')))]

# Snapshot translated startup globals.
def state_host(lib):
    a = (U * 4)()
    fn(lib, 'tpw_init_state', [VP], None)(a)
    return [a[0], a[1], a[2], bool(a[3])]

# Seed the matching native startup fixture.
def begin_host(lib, c):
    I.in_dll(lib, 'gDDResult').value = T.s32(c['version'] ^ 0xa55a73c1)
    S.host_prepare(lib, c)
    fn(lib, 'tpw_init_setup', [I, U, U, U, I, I], None)(c['renderer'],
        c['display'], c['system'], c['memory'], c['texmemory'], c['logger'])

class Overlay(C.Structure):
    _fields_ = [('data', VP), ('w', I), ('h', I), ('fmt', I), ('flags', U)]

# Bound procedural buffers and seed the terrain output size.
def prepared_case(kind, c):
    if kind in ['Prepare', 'Create']:
        c = dict(c, count=max(3, c['count']), size=min(c['size'], 9), q=dict(c['q']))
        c['q']['tile_size'] = [c['size']]
    return c

# Snapshot original procedural handler fields.
def proc_ppc(e, p):
    count = r32(e, p + 65552)
    cyclic = e.u32(p + 65544)
    current = e.u32(p + 65548)
    return [e.cl.lab(e.u32(p)), r32(e, p + 4), bool(cyclic), count,
        (current - cyclic) // 16 if count else -1]

# Snapshot translated procedural handler fields.
def proc_host(lib):
    out = (I * 5)()
    fn(lib, 'tpw_proc_get', [VP], None)(out)
    return list(out)

# Call the original and collect observable state.
def run_ppc(kind, e, c):
    c = prepared_case(kind, c)
    L = begin_ppc(e, c)
    try:
        out = {}
        name = S.text(e, c['name'])
        if kind.startswith('Engine'):
            if kind == 'EngineInit':
                out['result'] = call(e, ADDR[7], [c['format'], c['version'], name])
            elif kind == 'EngineClose':
                out['result'] = call(e, ADDR[8])
            else:
                out['result'] = call(e, ADDR[9]) & 255
        else:
            p = e.alloc(65556, b'\xa5' * 65556)
            call(e, ADDR[0], [p])
            if kind == 'ProcCtor':
                out['table_zero'] = e.read(p + 8, 65536) == b'\x00' * 65536
                out['untouched'] = e.u32(p + 65548) == 0xa5a5a5a5
            elif kind == 'ProcInit':
                if c['preinit'] == 1:
                    w32(e, p + 65544, 1)
                if c['preinit'] == 2:
                    w32(e, p, 1)
                out['result'] = call(e, ADDR[2], [p, c['count'], c['size']])
            elif kind in ['ProcClose', 'ProcDtor']:
                call(e, ADDR[2], [p, c['count'], c['size']])
                L.log.clear()
                L.next = 100
                if kind == 'ProcClose':
                    out['result'] = call(e, ADDR[3], [p])
                else:
                    call(e, ADDR[1], [p, c['dtorflag']])
            elif kind == 'ProcLoad':
                land = e.alloc(684)
                L.labels[land] = 12
                w32(e, p, land if c['overlay'] else 0)
                w32(e, p + 4, c['size'])
                out['result'] = call(e, ADDR[4], [p, name]) & 255
            else:
                (count, size) = (c['count'], c['size'])
                land = e.alloc(684)
                L.labels[land] = 12
                w32(e, p, land)
                w32(e, p + 4, size)
                nodes = e.alloc(count * 16, b'\x00' * (count * 16))
                w32(e, p + 65544, nodes)
                w32(e, p + 65552, count)
                w32(e, p + 65548, nodes + count // 2 * 16)
                data = []
                entries = []
                for i in range(count):
                    entry = e.alloc(92)
                    buf = e.alloc(size * size * 4 + 64, b'\x00' * (size * size * 4 + 64))
                    L.labels[entry] = 20 + i
                    L.labels[buf] = 40 + i
                    w32(e, entry + 60, buf)
                    entries.append(entry)
                    data.append(buf)
                    for (j, v) in enumerate([nodes + (i + count - 1) % count * 16,
                        nodes + (i + 1) % count * 16, 0, entry]):
                        w32(e, nodes + i * 16 + j * 4, v)
                tilekeys = [1, 2, 257, 258]
                cached = c['cached']
                for i in range(cached):
                    key = tilekeys[i]
                    slot = p + 8 + ((key >> 8) * 128 + (key & 255)) * 4
                    w32(e, slot, nodes + i * 16)
                    w32(e, nodes + i * 16 + 8, slot)
                n = c['n']
                overlay = e.alloc(max(20, n * 20))
                prep = e.alloc(max(4, n * 4))
                for i in range(max(1, n)):
                    ov = overlay + i * 20
                    for (j, v) in enumerate([entries[(i + 1) % count] if c['overlay'] else 0,
                        2, 3, 4, 256 if i % 2 else 0]):
                        w32(e, ov + j * 4, v)
                    if i < n:
                        w32(e, prep + i * 4, tilekeys[i])
                if kind == 'Prepare':
                    out['result'] = call(e, ADDR[5], [p, prep, overlay, n])
                    out['prepared'] = [L.lab(e.u32(prep + i * 4)) for i in range(n)]
                    out['overlays'] = [L.lab(e.u32(overlay + i * 20)) for i in range(n)]
                    out['links'] = [((e.u32(nodes + i * 16) - nodes) // 16,
                        (e.u32(nodes + i * 16 + 4) - nodes) // 16, (e.u32(nodes + i * 16 + 8) - (p + 8)) // 4 if e.u32(nodes + i * 16 + 8) else -1,
                        L.lab(e.u32(nodes + i * 16 + 12))) for i in range(count)]
                else:
                    out['result'] = call(e, ADDR[6], [p, entries[0],
                        c['cached'], c['overlay'], overlay])
                out['data'] = [e.read(buf, size * size * 4 + 64) for buf in data]
            if kind != 'ProcDtor' or c['dtorflag'] <= 0:
                out['proc'] = proc_ppc(e, p)
        out['state'] = state_ppc(e)
        out['log'] = L.log
        return out
    finally:
        end_ppc(e)

# Call the translation and collect matching state.
def run_host(kind, lib, c):
    c = prepared_case(kind, c)
    begin_host(lib, c)
    out = {}
    name = c['name'].encode() if c['name'] is not None else None
    if kind.startswith('Engine'):
        if kind == 'EngineInit':
            out['result'] = fn(lib, 'tpw_EngineInit', [I, I, VP])(c['format'], c['version'], name)
        elif kind == 'EngineClose':
            out['result'] = fn(lib, 'tpw_EngineClose', [])()
        else:
            out['result'] = fn(lib, 'tpw_EngineReinit', [])()
    else:
        fn(lib, 'tpw_proc_ctor', [], VP)()
        if kind == 'ProcCtor':
            check = (I * 2)()
            fn(lib, 'tpw_proc_ctorcheck', [VP], None)(check)
            out['table_zero'] = bool(check[0])
            out['untouched'] = bool(check[1])
        elif kind == 'ProcInit':
            fn(lib, 'tpw_proc_preinitialized', [I], None)(c['preinit'])
            out['result'] = fn(lib, 'tpw_proc_init', [I, I])(c['count'], c['size'])
        elif kind in ['ProcClose', 'ProcDtor']:
            fn(lib, 'tpw_proc_init', [I, I])(c['count'], c['size'])
            C.c_int32.in_dll(lib, 'tpw_cl_n').value = 0
            if kind == 'ProcClose':
                out['result'] = fn(lib, 'tpw_proc_close', [])()
            else:
                fn(lib, 'tpw_proc_dtor', [I], None)(c['dtorflag'])
        elif kind == 'ProcLoad':
            fn(lib, 'tpw_proc_loadsetup', [I, I], None)(c['overlay'], c['size'])
            out['result'] = fn(lib, 'tpw_proc_load', [VP])(name)
        else:
            C.c_int32.in_dll(lib, 'tpw_cl_on').value = 0
            fn(lib, 'tpw_proc_testsetup', [I, I], None)(c['count'], c['size'])
            for i in range(c['cached']):
                fn(lib, 'tpw_proc_cached', [U, I], None)([1, 2, 257, 258][i], i)
            CL.host_begin(lib, {}, c['q'])
            fn(lib, 'tpw_u8_setup', [I, I, U], None)(c.get('mode',
                1), c['format'], c['texture_flags'])
            fn(lib, 'tpw_init_setup', [I, U, U, U, I, I], None)(c['renderer'],
                c['display'], c['system'], c['memory'], c['texmemory'], c['logger'])
            fn(lib, 'tpw_proc_labels', [I], None)(c['count'])
            n = c['n']
            prep = (C.c_uint64 * max(1, n))(*[1, 2, 257, 258][:n])
            ov = (Overlay * max(1, n))()
            for i in range(max(1, n)):
                ov[i] = Overlay(fn(lib, 'tpw_proc_entry', [I], VP)((i + 1) % c['count']) if c['overlay'] else None,
                    2, 3, 4, 256 if i % 2 else 0)
            if kind == 'Prepare':
                out['result'] = fn(lib, 'tpw_proc_prepare', [VP, VP, I])(prep, ov, n)
                out['prepared'] = [fn(lib, 'tpw_u8_label', [VP])(prep[i]) for i in range(n)]
                out['overlays'] = [fn(lib, 'tpw_u8_label', [VP])(ov[i].data) for i in range(n)]
                out['links'] = []
                for i in range(c['count']):
                    a = (I * 4)()
                    fn(lib, 'tpw_proc_links', [I, VP], None)(i, a)
                    out['links'].append(tuple(a))
            else:
                out['result'] = fn(lib, 'tpw_proc_create', [I, I, I,
                    VP])(0, c['cached'], c['overlay'], ov)
            out['data'] = [C.string_at(fn(lib, 'tpw_proc_data', [I],
                VP)(i), c['size'] * c['size'] * 4 + 64) for i in range(c['count'])]
        if kind != 'ProcDtor' or c['dtorflag'] <= 0:
            out['proc'] = proc_host(lib)
    out['state'] = state_host(lib)
    out['log'] = CL.host_end(lib)
    return out
KINDS = ['ProcCtor', 'ProcDtor', 'ProcInit', 'ProcClose', 'ProcLoad',
    'Prepare', 'Create', 'EngineInit', 'EngineClose', 'EngineReinit']
TESTS = [T.FnTest(k, lambda e, c, k=k: run_ppc(k, e, c), lambda lib,
    c, k=k: run_host(k, lib, c), edge=edges, rnd=case) for k in KINDS]
