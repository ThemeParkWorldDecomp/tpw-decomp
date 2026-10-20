"""Shared external-callee fakes for engine startup and image-loader tests."""
import ctypes as C
import struct
import calllog as CL
import tpwtest as T
VP = C.c_void_p
I = C.c_int32
U = C.c_uint32
G = dict(extra=0x100af1cc, avg=0x100af1bc, flags=0x100af1c0, wave=0x100af1c4, manager=0x100af1c8)
SYSTEM = 0x10165f80
DISPLAY = 0x10165634
TEXTURE = 0x10165fc8

# Write a big-endian engine word.
def w32(e, a, v):
    e.write(a, struct.pack('>I', v & 0xffffffff))

# Read a signed engine word.
def r32(e, a):
    return T.s32(e.u32(a))

# Allocate a nullable fake C string.
def text(e, s):
    return CL.new_text(e, s) if s is not None else 0

# Declare a native wrapper signature.
def fn(lib, name, args, rest=I):
    f = getattr(lib, name)
    f.argtypes = args
    f.restype = rest
    return f

# Install matching external fakes and seed engine state.
def prepare(e, case, real):
    original_imports = dict(e.stubs)
    st = CL.Stubs()
    st.real = set(real)
    L = CL.PpcLog({SYSTEM: 1, DISPLAY: 2, TEXTURE: 3})
    e.cl = L
    e.u8blocks = []
    e.u8pixels = {}
    e.heap_in_scratch = True
    L.queues = {k: list(v) for (k, v) in case.get('q', {}).items()}
    e.u8file = case.get('file', b'')
    e.u8pos = 0
    draw = e.alloc(64)
    drawGL = e.alloc(64)
    L.labels[draw] = 4
    L.labels[drawGL] = 5
    w32(e, draw + 32, case.get('mode', 1))
    w32(e, 0x10052b3c, draw)
    w32(e, 0x102b363c, drawGL)
    w32(e, TEXTURE + 4, case.get('format', 4))
    w32(e, TEXTURE, case.get('texture_flags', 0x12345678))
    for key in G:
        w32(e, G[key], case.get(key, 0))

    # Build a queued call-log fake.
    def rec(name, spec='', ret=None, then=None):
        return CL.recorder(name, spec, ret, then)

    # Register an imported callee fake.
    def imp(name, f):
        st.imp(name, f)
    imp('__assertion_failed', rec('assert', 'ssi'))
    imp('__assertion_failed', lambda e: L.rec('assert', (T.s32(e.r(5)),
        ), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
    imp('DebugStr', lambda e: L.rec('DebugStr', (), (e.read(e.r(3) + 1,
        e.read(e.r(3), 1)[0]).decode('latin-1'),)))

    # Compare fake strings without recording runtime calls.
    def strcmp(e):
        e.set_r(3, (e.cstr(e.r(3)) > e.cstr(e.r(4))) - (e.cstr(e.r(3)) < e.cstr(e.r(4))))
    imp('strcmp', strcmp)

    # Append fake strings without recording runtime calls.
    def strcat(e):
        a = e.r(3)
        s = e.cstr(a) + e.cstr(e.r(4))
        e.write(a, s.encode() + b'\x00')
        e.set_r(3, a)
    imp('strcat', strcat)

    # Record allocation and reserve guard bytes for returning assertions.
    def alloc(e):
        size = e.r(3)
        L.rec('Malloc', (size,))
        if not L.ret('Malloc', 1):
            e.set_r(3, 0)
            return
        a = e.alloc(size + 4096)
        e.write(a, b'\x00' * (size + 4096))
        L.newlab(a)
        e.u8blocks.append((a, size + 4096))
        e.set_r(3, a)

    # Record release and clear the caller pointer.
    def free(e):
        a = e.u32(e.r(3))
        L.rec('Free', (L.lab(a),))
        L.forget(a)
        w32(e, e.r(3), 0)

    # Record file opening and reset its read position.
    def op(e):
        L.rec('Open', (), (e.cstr(e.r(3)) if e.r(3) else '(null)', e.cstr(e.r(4))))
        e.u8pos = 0
        e.set_r(3, 1 if L.ret('Open', 1) else 0)

    # Record a file read and copy available bytes.
    def read(e):
        (dest, size, count) = (e.r(3), e.r(4), e.r(5))
        L.rec('Read', (size, count))
        data = e.u8file[e.u8pos:e.u8pos + size * count]
        e.write(dest, data)
        e.u8pos += len(data)
        e.set_r(3, len(data) // size if size else 0)

    # Record a relative seek and advance the file position.
    def seek(e):
        L.rec('Seek', (e.r(4), e.r(5)))
        e.u8pos += e.r(4)
        e.set_r(3, 0)

    # Record filename mapping and return the queued result.
    def name(e):
        a = e.r(3)
        L.rec('Name', (T.s32(e.r(4)),), (e.cstr(a),))
        e.set_r(3, a if L.ret('Name', 1) else 0)
    callbacks = [(0x10051f70, alloc), (0x10051f74, free), (0x101735e4,
        op), (0x101735e0, rec('CloseFile', ret=lambda e, L: 0)), (0x101735d4,
        rec('Tell', ret=lambda e, L: L.ret('Tell', len(e.u8file)))), (0x101735d8,
        read), (0x101735dc, seek), (0x101735d0, name)]
    for (i, (addr, f)) in enumerate(callbacks):
        code = 0x1004b000 + i * 4
        st.internal(code, f)
        tv = e.alloc(8, struct.pack('>II', code, e.toc))
        w32(e, addr, tv)

    # Label and initialize a fake wave object.
    def wave_ctor(e):
        a = e.r(3)
        L.rec('Wave.ctor', (L.newlab(a),))
        e.write(a, b'\x00' * 52)
        w32(e, a + 40, 0x100516b8)

    # Record wave loading and fill queued image metadata.
    def wave_load(e):
        a = e.r(3)
        L.rec('Wave.Load', (L.lab(a),), (e.cstr(e.r(4)),))
        e.write(a + 8, struct.pack('>HH', L.ret('Wave.width', 3) & 65535,
            L.ret('Wave.height', 4) & 65535))
        w32(e, a + 32, L.ret('Wave.alpha'))
        e.write(a + 5, bytes([L.ret('Wave.threshold') & 255]))
        e.write(a + 7, bytes([L.ret('Wave.level') & 255]))
        w32(e, a, 4660)
        e.set_r(3, L.ret('Wave.Load'))

    # Record destruction and forget the fake wave label.
    def wave_dtor(e):
        L.rec('Wave.dtor', (L.lab(e.r(3)),))
        L.forget(e.r(3))
    st.internal(0x10005de4, wave_ctor)
    st.internal(0x10005f68, wave_load)
    st.internal(0x10005ea8, wave_dtor)

    # Label and initialize a fake texture entry.
    def entry_ctor(e):
        a = e.r(3)
        L.rec('Entry.ctor', (L.newlab(a),))
        w32(e, a + 8, 0)
        w32(e, a + 60, 0)
        e.write(a + 48, b'\x00\x00')

    # Record destruction and forget the texture entry label.
    def entry_dtor(e):
        L.rec('Entry.dtor', (L.lab(e.r(3)),))
        L.forget(e.r(3))
    st.internal(0x10040d30, entry_ctor)
    st.internal(0x10040dfc, entry_dtor)
    st.internal(0x10040e84, rec('Entry.Create', 'Lii', ret=lambda e, L: L.ret('Entry.Create', 1)))
    st.internal(0x10040f7c, rec('Entry.Close', 'L', ret=lambda e, L: L.ret('Entry.Close', 1)))
    st.internal(0x10041560, rec('Entry.SetSize', 'Lii'))
    st.internal(0x10041274, rec('Entry.GetData', 'L', ret=lambda e, L: e.u32(e.r(3) + 60)))
    st.internal(0x10041334, rec('Entry.Corrected', 'L', ret=lambda e, L: e.u32(e.r(3) + 60)))
    st.internal(0x10041738, rec('Entry.Lock', 'L'))

    # Execute element constructors through their TVectors.
    def new_array(e, call):
        (raw, ctor, dtor, size, n) = [e.r(i) for i in range(3, 8)]
        a = raw + 8
        w32(e, raw, size)
        w32(e, raw + 4, n)
        for i in range(n):
            call(ctor, a + i * size, 1)
        return a

    # Execute element destructors in reverse array order.
    def destroy_array(e, call):
        (a, dtor) = (e.r(3), e.r(4))
        (size, n) = (e.u32(a - 8), e.u32(a - 4))
        for i in range(n - 1, -1, -1):
            call(dtor, a + i * size, -1)
    T.stub_cb(e, '__construct_new_array', new_array)
    T.stub_cb(e, '__destroy_new_array', destroy_array)

    # Record pixel format and preserve conversion metadata.
    def pixel_ctor(e):
        (a, fmt, data, w, h, stride, pal) = [e.r(i) for i in range(3, 10)]
        fmt = T.s32(fmt)
        v = fmt if 0 <= fmt <= 13 else -100 - L.lab(e.r(4))
        L.rec('Pixels.ctor', (L.newlab(a), v, L.lab(data), T.s32(w),
            T.s32(h), T.s32(stride), L.lab(pal)))
        e.u8pixels[a] = (data, fmt, T.s32(w), T.s32(h))

    # Record destruction and discard pixel metadata.
    def pixel_dtor(e):
        L.rec('Pixels.dtor', (L.lab(e.r(3)),))
        L.forget(e.r(3))
        e.u8pixels.pop(e.r(3), None)

    # Record conversion and generate deterministic output bytes.
    def convert(e):
        (d, s) = (e.r(3), e.r(4))
        L.rec('Convert', (L.lab(d), L.lab(s), e.r(5) & 255))
        (data, fmt, w, h) = e.u8pixels[d]
        source = e.u8pixels[s][0]
        n = w * h * (2 if fmt == 4 else 4 if fmt == 10 else 3)
        if 0 < w * h < 16384:
            e.write(data, bytes((e.read(source, 1)[0] ^ i & 255 ^ fmt & 255 for i in range(n))))

    # Return the queued channel averages.
    def averages(e):
        L.rec('Averages', (L.lab(e.r(3)),))
        v = L.ret('Averages', 0x12345678)
        for i in range(4):
            e.write(e.r(4 + i), bytes([v >> 24 - i * 8 & 255]))
    imp('__ct__Q213SamsUtilities7UPixelsFQ213SamsUtilities11PixelFormatPvUlUlUlPQ213SamsUtilities6UPixel',
        pixel_ctor)
    imp('__dt__Q213SamsUtilities7UPixelsFv', pixel_dtor)
    imp('SetAlphaDefault__Q213SamsUtilities7UPixelsFUc', rec('AlphaDefault', 'Lb'))
    imp('SetAlphaThreshold__Q213SamsUtilities7UPixelsFUc', rec('AlphaThreshold', 'Lb'))
    imp('Convert__Q213SamsUtilities7UPixelsFRCQ213SamsUtilities7UPixelsUc', convert)
    imp('GetAverages__Q213SamsUtilities7UPixelsFRUcRUcRUcRUc', averages)
    imp('GetAlphaCount__Q213SamsUtilities7UPixelsFv', rec('AlphaCount', 'L', ret='i'))
    imp('__ct__Q213SamsUtilities11ULogManagerFv', rec('Logger.ctor', 'N'))
    imp('SetContext__Q213SamsUtilities11ULogManagerFPCcPCciUl', rec('Context', 'Lssiu'))

    # Record the format and its first variadic word when present.
    def log(e):
        format = e.cstr(e.r(4))
        values = (L.lab(e.r(3)),)
        if '%x' in format:
            values += (T.s32(e.r(5)),)
        L.rec('Log', values, (format,))
    imp('Log__Q213SamsUtilities11ULogManagerFPCce', log)
    saved = st.activate(e)
    e.u8saved = (original_imports, saved[1])
    return (L, draw, drawGL)

# Restore the original imports and internal callbacks.
def finish(e):
    CL.Stubs.deactivate(e, e.u8saved)

# Reset native call logs and seed the fake file.
def host_prepare(lib, c):
    CL.host_begin(lib, {}, c.get('q', {}))
    fn(lib, 'tpw_u8_setup', [I, I, U], None)(c.get('mode', 1), c.get('format',
        4), c.get('texture_flags', 0x12345678))
    data = c.get('file', b'')
    C.memmove(C.addressof((C.c_uint8 * 65536).in_dll(lib, 'tpw_u8_file')), data, len(data))
    I.in_dll(lib, 'tpw_u8_file_size').value = len(data)
    fn(lib, 'tpw_loader_globals', [I, U, U], None)(c.get('extra', 0),
        c.get('avg', 0), c.get('flags', 0)) if hasattr(lib, 'tpw_loader_globals') else None

# Snapshot the original loader globals.
def globals_ppc(e):
    return [e.u32(G[k]) for k in ['extra', 'avg', 'flags']] + [bool(e.u32(G['wave']))]

# Snapshot the translated loader globals.
def globals_host(lib):
    out = (I * 5)()
    fn(lib, 'tpw_loader_global', [VP], None)(out)
    return [x & 0xffffffff for x in out[1:4]] + [bool(out[4])]

# Read the full allocation including guard bytes.
def ppc_buffer(e, p):
    for (a, n) in e.u8blocks:
        if p == a:
            return e.read(a, n)
    return None

# Read the matching native allocation including guard bytes.
def host_buffer(lib, p):
    n = fn(lib, 'tpw_u8_block_size', [VP])(p)
    return C.string_at(p, n) if n >= 0 else None

# Install standard runtime fakes.
def setup(runner):
    T.install_std_stubs(runner.emu)
