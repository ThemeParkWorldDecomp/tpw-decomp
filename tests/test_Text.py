"""Text.cpp against libtpw_unit_Text: queued ltms fakes, all logical state and call order.

Resource fakes overwrite the output independently of success (or preserve it with -1).
Fonts are resources here; rendering, file loading and string formatting belong to ltms.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
import test_FSSystem as FS
from tpwtest import FnTest

PROGRAM = 'SimTheme Park'
SHIM_LIBRARY = 'unit_Text'
STARTS = [
    0x1015be6c, 0x1015e4b8, 0x1015e93c, 0x1015e9d8, 0x1015ea0c,
    0x1015ea40, 0x1015e524, 0x1015e970, 0x1015e9a4
]
NAMES = [
    'Init', 'SetRez', 'GetLanguageName', 'GetLanguageId', 'SetLanguageId', 'IsHebrew',
    'UnInit', 'InitThemeSpecific', 'DeInitThemeSpecific'
]
A = 0x1037ff18
VP, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32


# Configure a ctypes entry point with its native argument and result types.
def F(lib, name, restype, *args):
    f = lib[name]
    f.restype = restype
    f.argtypes = list(args)
    return f


# Store one big-endian word in the original object.
def w32(e, a, v):
    e.write(a, struct.pack('>I', v & 0xffffffff))


# Install the external fakes while leaving this unit's original entries active.
def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if 'c' not in FS.VT:
        FS.make_vtables(e)
    S = CL.Stubs()
    S.real.update(STARTS + [0x1015ea80, 0x1015ef00, 0x1015eecc])
    FS.string_stubs(S)

    # Build a constructor fake that records its object label.
    def ctor(name):
        return lambda e: e.cl.rec(name, (e.cl.lab(e.r(3)),))

    # Record the CText words already initialised before manager construction.
    S.imp('__ct__21TbTextResourceManagerFv', lambda e: e.cl.rec(
        'ResourceManager', (e.cl.lab(e.r(3)), e.u32(A), e.u32(A + 4))))
    S.imp('__ct__18TbFontRenderParamsFv', ctor('RenderParams'))

    # Record wrapper construction and retain the supplied font pointer.
    def wrapper(e):
        e.cl.rec('FontWrapper', (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
        w32(e, e.r(3) + 0x14, e.r(4))

    S.internal(0x10168ae0, wrapper)

    # Apply a queued manager state change and write the hidden TbError result.
    def manager(e, key):
        w32(e, e.r(4), e.cl.ret('ManagerState', 0x1234))
        w32(e, e.r(3), e.cl.ret(key))

    # Build a manager fake with an optional string argument.
    def simple(key, text=False):
        # Record the call and apply the same queued effects as the host fake.
        def stub(e):
            e.cl.rec(key, (e.cl.lab(e.r(4)),), (e.cstr(e.r(5)),) if text else ())
            manager(e, key)
        return stub

    for key, symbol in [
        ('Open', 'Open__21TbTextResourceManagerFPCc'),
        ('SetLanguage', 'SetLanguage__21TbTextResourceManagerFPCc'),
        ('Close', 'Close__21TbTextResourceManagerFv')
    ]:
        S.imp(symbol, simple(key, key != 'Close'))

    # Build a typed resource-request fake with independent output and status queues.
    def get(name):
        # Record the call and apply the same queued effects as the host fake.
        def stub(e):
            id = e.r(6)
            old = e.u32(e.r(5))
            e.cl.rec(name, (e.cl.lab(e.r(4)), id, e.cl.lab(old)))
            out = e.cl.ret('ResourceOut', 180 + id if id < 1000 else id - 800)
            if out >= 0:
                w32(e, e.r(5), e.text_pointers[out])
            manager(e, 'GetResource')
        return stub

    for name, kind in [
        ('GetTable', '8TbITable'), ('GetFont', '7TbIFont'),
        ('GetMBToUnicode', '14TbIMBToUnicode'), ('GetUnicodeToMB', '14TbIUnicodeToMB')
    ]:
        S.imp('GetResource__21TbTextResourceManagerFPP' + kind + 'Ul', get(name))

    # Record alpha-write selection and update the render parameters.
    def alpha(e):
        e.cl.rec('UseAlphaWriteMode', (e.cl.lab(e.r(4)),))
        w32(e, e.r(4) + 4, e.cl.ret('AlphaState', 0x4321))
        w32(e, e.r(3), e.cl.ret('UseAlphaWriteMode'))

    S.imp('UseAlphaWriteMode__18TbFontRenderParamsFv', alpha)

    # Record the error arguments and mark the error object as set.
    def error(e):
        e.cl.rec('SetError', (e.cl.lab(e.r(3)), T.s32(e.r(4)), e.r(5)), (e.cstr(e.r(6)),))
        w32(e, e.r(3), 1)

    S.internal(0x1014b62c, error)

    # Format and record the original trace message.
    def trace(e):
        text = e.cstr(e.r(3))
        if '%s' in text:
            text = text % e.cstr(e.r(4))
        e.cl.rec('TRACE', (), (text[:159],))

    S.internal(0x1006fcfc, trace)
    # Record the failure diagnostic without platform debug output.
    S.internal(0x10000e40, lambda e: e.cl.rec('UDebug', (), (e.cstr(e.r(3))[:159],)))

    # Format the string-table failure diagnostic into the caller's buffer.
    def sprintf(e):
        text = e.cstr(e.r(4)) % (e.cstr(e.r(5)), e.cstr(e.r(6)))
        e.write(e.r(3), text.encode() + b'\x00')
        e.set_r(3, len(text))

    S.imp('sprintf', sprintf)
    runner.text_stubs = S
    e.text_stubs = S
    # Release records the resource without deleting the fake storage.
    e.text_release = T.fake_function(e, lambda e: e.cl.rec('Release', (e.cl.lab(e.r(3)),)))


# Seed the original object, globals, resource pointers and return queues.
def seed_ppc(e, c):
    e.cl = CL.PpcLog({A: 1, A + 0xc: 2, A + 0x4fc: 3, 0x1037f1c0: 4}, c['queues'])
    vt = e.alloc(0xc, struct.pack('>III', 0, 0, e.text_release))
    e.text_pointers = {0: 0}
    for label in list(range(100, 175)) + list(range(180, 253)):
        p = e.alloc(8, struct.pack('>II', vt, 0))
        e.cl.labels[p] = label
        e.text_pointers[label] = p

    # Resolve a logical resource label to its PowerPC pointer.
    def ptr(label):
        return e.text_pointers[label]

    words = c['words']
    raw = bytearray(0x5a8)

    # Store a word in the object buffer before copying it into emulator memory.
    def put(off, v):
        struct.pack_into('>I', raw, off, v & 0xffffffff)

    for i in range(3):
        put(i * 4, words[i])
    for i, off in enumerate([0xc, 0x14, 0x18]):
        put(off, words[3 + i])
    for i in range(52):
        for k, o in enumerate([0, 4, 8, 0xc, 0x14]):
            value = words[6 + i * 5 + k]
            put(0x1c + i * 0x18 + o, ptr(value) if k == 4 else value)
    for i in range(19):
        put(0x500 + i * 4, words[266 + i])
    for i in range(23):
        put(0x54c + i * 4, ptr(words[285 + i]))
    e.write(A, bytes(raw))
    w32(e, 0x103bc64c, c['override'])
    w32(e, 0x10380720, c['resolution'])
    e.write(0x102b2e18, c['language'].encode() + b'\x00')
    w32(e, 0x1037f1c0, c['error'])


# Read every logical field, converting resource pointers back to labels.
def read_ppc(e):
    w = [e.u32(A + i * 4) for i in range(3)] + [e.u32(A + off) for off in [0xc, 0x14, 0x18]]
    for i in range(52):
        w += [e.u32(A + 0x1c + i * 0x18 + o) for o in [0, 4, 8, 0xc]]
        w += [e.cl.lab(e.u32(A + 0x30 + i * 0x18))]
    w += [e.u32(A + 0x500 + i * 4) for i in range(19)]
    w += [e.cl.lab(e.u32(A + 0x54c + i * 4)) for i in range(23)]
    return w + [e.u32(0x1037f1c0)]


# Run one original entry and collect its result, state and call log.
def ppc(e, c, which):
    saved = e.text_stubs.activate(e)
    try:
        seed_ppc(e, c)
        result = T.call_ppc(e, STARTS[which], [A, c['arg']])[0]
        result = result if which in [0, 3, 5] else int(result == 0x102b2e18) if which == 2 else 0
        return dict(result=result, words=read_ppc(e), log=e.cl.log)

    finally:
        e.text_stubs.deactivate(e, saved)


# Run the matching native entry with the same case and return queues.
def host(lib, c, which):
    F(lib, 'tpw_text_seed', None, VP, I32, I32, C.c_char_p, I32)(
        (U32 * 308)(*c['words']), c['resolution'], c['override'], c['language'].encode(), c['error'])
    fixed = {
        1: F(lib, 'tpw_text_object', VP)(),
        2: F(lib, 'tpw_text_subobject', VP, I32)(0),
        3: F(lib, 'tpw_text_subobject', VP, I32)(1),
        4: F(lib, 'tpw_text_error', VP)()
    }
    resource = F(lib, 'tpw_text_resource', VP, U32)
    for label in list(range(100, 175)) + list(range(180, 253)):
        fixed[label] = resource(label)
    CL.host_begin(lib, fixed, c['queues'])
    result = F(lib, 'tpw_text_call', U32, I32, U32)(which, c['arg'])
    words = (U32 * 309)()
    F(lib, 'tpw_text_read', None, VP)(words)
    return dict(result=result, words=list(words), log=CL.host_end(lib))


# Generate the shared random object state in the original draw order.
def case(r):
    w = [r.getrandbits(32) for _ in range(308)]
    for i in range(52):
        w[6 + i * 5 + 4] = r.choice([0, 100 + i])
    for i in range(23):
        w[285 + i] = r.choice([0, 152 + i])
    w[2] = r.choice([0x40d, 0x409, r.getrandbits(32)])
    return dict(
        words=w,
        resolution=r.choice([-1, 0, 1, 2, 3, 4, 0x7fffffff]),
        override=r.choice([-1, 0, 1, 2, 3, 4, 5, 0x7fffffff]),
        language=r.choice(['English', 'American', 'Hebrew', '']),
        error=r.randrange(3),
        arg=r.choice([512, 513, 640, 641, 800, 801, 0x40d, r.getrandbits(32)]),
        queues={})


# Generate an Init case with queued load failures and pointer writes.
def init_random(r):
    c = case(r)
    fail = r.randrange(39)
    c['queues'] = {
        'Open': [r.choice([-2, -1, 0, 1, 0x7fffffff])] if fail == 0 else [0],
        'SetLanguage': [r.choice([-2, -1, 0, 1])] if fail == 1 else [0],
        'GetResource': [r.choice([0, 1, 0x7fffffff]) for _ in range(max(0, fail - 2))] + [-1]
                       if fail < 38 else [0],
        'ResourceOut': [r.choice([-1, 0, 100 + i % 75]) for i in range(36)],
        'ManagerState': [r.getrandbits(32)],
        'AlphaState': [r.getrandbits(32)],
        'UseAlphaWriteMode': [r.choice([-1, 0, 1])]
    }
    return c


# Exercise each load failure and both resolution-selection inputs.
def init_edges():
    r = random.Random(707)
    for group in range(4):
        for fail in range(-1, 36):
            c = case(r)
            c.update(resolution=group, override=0)
            c['queues'] = {'GetResource': [0] * fail + [-1]} if fail >= 0 else {}
            yield c
    for key in ['Open', 'SetLanguage']:
        for result in [-0x80000000, -2, -1, 0, 1, 0x7fffffff]:
            c = case(r)
            c['queues'] = {key: [result]}
            yield c
    for override in [-0x80000000, -1, 0, 1, 2, 3, 4, 5, 0x7fffffff]:
        for resolution in [-0x80000000, -1, 0, 1, 2, 3, 4, 0x7fffffff]:
            c = case(r)
            c.update(override=override, resolution=resolution)
            yield c


# Exercise scalar boundaries and fully empty or occupied resource slots.
def edges():
    r = random.Random(708)
    for arg in [0, 511, 512, 513, 639, 640, 641, 799, 800, 801, 0x7fffffff, 0x80000000, 0xffffffff, 0x40d]:
        c = case(r)
        c['arg'] = arg
        c['words'][2] = arg
        yield c
    for occupied in [False, True]:
        c = case(r)
        for i in range(52):
            c['words'][6 + i * 5 + 4] = 100 + i if occupied else 0
        for i in range(23):
            c['words'][285 + i] = 152 + i if occupied else 0
        yield c


TESTS = [
    FnTest('CText::' + name, lambda e, c, k=k: ppc(e, c, k), lambda lib, c, k=k: host(lib, c, k),
           edge=init_edges if k == 0 else edges, rnd=init_random if k == 0 else case)
    for k, name in enumerate(NAMES)
]

STRINGS = [0x1038050c + i * 0x14 for i in range(16)] + [0x103804ec, 0x103804cc]


# Run the original static initializer and verify its registration order.
def sinit_ppc(e, c):
    saved = e.text_stubs.activate(e)
    try:
        seed_ppc(e, c)
        for i in range(52):
            e.cl.labels[A + 0x1c + i * 0x18] = 300 + i
        regs = []
        e.stubs['__register_global_object'] = lambda e: regs.append((e.r(3), e.r(4)))
        for addr in STRINGS:
            e.write(addr, b'\x00' * 0x14)
        T.call_ppc(e, 0x1015ea80, [])
        expected = [(a, e.u32(e.toc - 0x7c1c)) for a in STRINGS] + [(A, 0x102672b0)]
        return dict(words=read_ppc(e), log=e.cl.log,
                    texts=[FS.s_read(e, a, False) for a in STRINGS], registrations_ok=regs == expected)
    finally:
        e.text_stubs.deactivate(e, saved)


# Reconstruct the native globals and collect the same observable state.
def sinit_host(lib, c):
    F(lib, 'tpw_text_seed', None, VP, I32, I32, C.c_char_p, I32)(
        (U32 * 308)(*c['words']), c['resolution'], c['override'], c['language'].encode(), c['error'])
    fixed = {
        1: F(lib, 'tpw_text_object', VP)(),
        2: F(lib, 'tpw_text_subobject', VP, I32)(0),
        3: F(lib, 'tpw_text_subobject', VP, I32)(1),
        4: F(lib, 'tpw_text_error', VP)()
    }
    for label in list(range(100, 175)) + list(range(180, 253)):
        fixed[label] = F(lib, 'tpw_text_resource', VP, U32)(label)
    for i in range(52):
        fixed[300 + i] = F(lib, 'tpw_text_font', VP, I32)(i)
    CL.host_begin(lib, fixed, c['queues'])
    F(lib, 'tpw_text_sinit', None)()
    words = (U32 * 309)()
    F(lib, 'tpw_text_read', None, VP)(words)
    texts = []

    # Native layout of the functional fake's narrow dynamic string.
    class String(C.Structure):
        _fields_ = [('vt', VP), ('capacity', I32), ('data', VP), ('length', I32), ('grow', I32)]
    for i in range(18):
        addr = F(lib, 'tpw_text_string', VP, I32)(i)
        texts.append(C.string_at(String.from_address(addr).data).decode())
    return dict(words=list(words), log=CL.host_end(lib), texts=texts, registrations_ok=True)


TESTS.append(FnTest('__sinit_Text_cpp', sinit_ppc, sinit_host, edge=edges, rnd=case))
