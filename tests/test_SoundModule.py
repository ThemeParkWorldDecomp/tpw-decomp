"""w79a: sound device base and word-sized channel values, all listed addresses.

External TbError, powf and allocator calls record all arguments on both sides. The
powf fake calls the same host C function on each side; Mac MSL accuracy is not proven.
The four pure virtual device operations remain for the later device implementation.
"""
import ctypes as C
import struct
import tpwtest as T
from tpwtest import FnTest
from calllog import PpcLog, host_begin, host_end

PROGRAM = "sound symbols shared"
SHIM_LIBRARY = "unit_SoundModule"
MASK = 0xffffffff
VT = 0x1003c5c4

# Filled below from the declarations, in the host switch order.
MODULE = [
    ("Process", 0x100125c8, 0xc, "", "void"),
    ("GetDirectSoundObject", 0x10012600, 0x10, "o", "uint32_t"),
    ("GetDirectSoundBuffer", 0x10012660, 0x14, "io", "void"),
    ("GetSystemObject", 0x100126c4, 0x18, "i", "void*"),
    ("SetOptions", 0x10012708, 0x1c, "i", "TbError"),
    ("Init", 0x10012340, 0x20, "s", "TbError"),
    ("StopAll", 0x1000bc34, 0x30, "", "TbError"),
    ("PauseChannel", 0x10012764, 0x34, "i", "TbError"),
    ("RestartChannel", 0x100127c4, 0x38, "i", "TbError"),
    ("PauseAll", 0x10012824, 0x3c, "", "TbError"),
    ("RestartAll", 0x1001287c, 0x40, "", "TbError"),
    ("IsChannelPlaying", 0x100128d8, 0x44, "i", "bool"),
    ("SupportsEnvironmentalEffects", 0x1000bf00, 0x48, "", "bool"),
    ("SetLoPass", 0x10012920, 0x4c, "if", "TbError"),
    ("SetDistanceFactor", 0x1001297c, 0x50, "io", "TbError"),
    ("SetSpeakerConfig", 0x100129f4, 0x54, "o", "TbError"),
    ("Set3DDSPFunctionality", 0x10012a68, 0x58, "ii", "TbError"),
    ("SetSpeedOfSound", 0x10012ad0, 0x5c, "f", "TbError"),
    ("SetPanRate", 0x10012b30, 0x60, "i", "TbError"),
    ("SetPosition", 0x10012b8c, 0x64, "ioi", "TbError"),
    ("GetPosition", 0x10012bfc, 0x68, "io", "TbError"),
    ("SetVelocity", 0x10012c6c, 0x6c, "io", "TbError"),
    ("GetVelocity", 0x10012cdc, 0x70, "io", "TbError"),
    ("SetListenerVelocity", 0x10012d4c, 0x74, "o", "TbError"),
    ("SetListenerPosition", 0x10012dc4, 0x78, "o", "TbError"),
    ("SetListenerDirection", 0x10012e3c, 0x7c, "ot", "TbError"),
    ("SetPitch", 0x10012ec8, 0x80, "iii", "TbError"),
    ("SetFrequency", 0x10012f24, 0x84, "iif", "TbError"),
    ("SetChannelInfo", 0x10012f84, 0x88, "ipo", "TbError"),
    ("SetReverbEnvironment", 0x1001300c, 0x90, "ifff", "TbError"),
    ("SetReverbPosition", 0x10013074, 0x94, "o", "TbError"),
]

VALUES = [
    ("CUniqueHandle::CUniqueHandle()", 0x10009de0),
    ("CUniqueHandle::CUniqueHandle(void*)", 0x10009f34),
    ("CUniqueHandle::operator void*", 0x1000a164),
    ("CUniqueHandle::GetIndex", 0x1000a1f4),
    ("CUniqueHandle::SetIndex", 0x10001c60),
    ("CUniqueHandle::operator++", 0x10000c54),
    ("CAtomicValue::CAtomicValue", 0x10014dc8),
    ("CAtomicValue::operator++", 0x10010df4),
    ("CAtomicValue::operator--", 0x1000a93c),
    ("CAtomicValue::operator long", 0x1000be34),
    ("CChannelState::CChannelState", 0x10014d6c),
    ("CChannelState::Instance", 0x1000a97c),
    ("CChannelState::AddChannel", 0x10010da4),
    ("CChannelState::RemoveChannel", 0x1000a8e8),
    ("CChannelState::GetNumberOfChannelsInUse", 0x1000bdd4),
    ("CLowest::CLowest", 0x10010904),
    ("CLowest::operator()", 0x10010804),
    ("CAutoCriticalSection::CAutoCriticalSection", 0x10009c20),
    ("CAutoCriticalSection::operator void**", 0x1000078c),
    ("LB_MIN first copy", 0x1000fdf0),
    ("LB_MIN second copy", 0x1001bc00),
]
LIFETIMES = [
    ("CAutoCriticalSection::~CAutoCriticalSection", 0x10009ba8),
    ("TbSoundCriticalSection::TbSoundCriticalSection", 0x10000738),
    ("TbSoundCriticalSection::~TbSoundCriticalSection", 0x100006a8),
    ("TbSpeaker::AutoDelete::~AutoDelete", 0x1001493c),
    ("TbSoundSystemModule::~TbSoundSystemModule", 0x100071d4),
    ("CAutoArrayPtr<unsigned char>::CAutoArrayPtr", 0x10009d30),
    ("CAutoArrayPtr<unsigned char>::operator unsigned char*", 0x10009c60),
    ("CAutoArrayPtr<unsigned char>::~CAutoArrayPtr", 0x10009c9c),
]
EDGES = [0, 1, 2, 0x3ffffff, 0x4000000, 0x7ffffff, 0x8000000, 0xffff,
         0x10000, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff, 0xa5a5a5a5]
FLAGS = [0, 1, 2, 0x7fff, 0x8000, 0xffff, 0x10000, 0x10001, 0xffffffff]


def array(words):
    return (C.c_uint32 * len(words))(*words)


def pack(words):
    return struct.pack(">%dI" % len(words), *words)


def image(e, address, count):
    return tuple(struct.unpack(">%dI" % count, e.read(address, 4 * count)))


def double_bits(value):
    return struct.unpack(">Q", struct.pack(">d", value))[0]


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    lib = runner.lib
    lib.tpw_sm_pow.argtypes = [C.c_float, C.c_float]
    lib.tpw_sm_pow.restype = C.c_float

    # Functional external error constructor; the buffer is a fixed case label.
    def error(e):
        value = e.r(4)
        e.cl.rec("TbError", (e.cl.lab(e.r(3)), T.s32(value)))
        e.write(e.r(3), pack([value]))

    # The MSL powf wrapper is external to this unit. Its two arguments are recorded.
    def power(e):
        a, b = e.f(1), e.f(2)
        e.cl.rec("powf", (T.f32_bits(a), T.f32_bits(b)))
        e.set_f1(lib.tpw_sm_pow(a, b))

    def allocate(e):
        count = e.r(3)
        block = e.alloc(max(count, 1), b'\xa5' * max(count, 1))
        e.cl.rec("new[]", (count, e.cl.newlab(block)))
        e.set_r(3, block)

    def release(e):
        block = e.r(3)
        e.cl.rec("delete", (e.cl.lab(block),))
        e.cl.forget(block)

    T.stub_internal(e, 0x100015b8, error)
    T.stub_internal(e, 0x10012580, power)
    e.stub('__nwa__FUl', allocate)
    e.stub('__dl__FPv', release)
    e.stub('__dla__FPv', release)
    lib.tpw_sm_module.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p]
    lib.tpw_sm_value.argtypes = [C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p]
    lib.tpw_sm_lifetime.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, C.c_int32, C.c_int32, C.c_uint32, C.c_void_p]
    lib.tpw_sm_buffer.argtypes = [C.c_int32, C.c_void_p, C.c_uint32, C.c_void_p]
    lib.tpw_sm_sequence.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, C.c_int32, C.c_void_p, C.c_uint32, C.c_void_p]


def module_case(r=None, pitch=0):
    words = [0xa5a5a5a5] * 5 if r is None else [r.getrandbits(32) for _ in range(5)]
    words[0] = VT
    args = [pitch & MASK, 0x3f800000, 0x40000000, 0xbf800000]
    token = [0xa5a5a5a5] * 36
    if r is not None:
        args = [r.getrandbits(32) for _ in range(4)]
        token = [r.getrandbits(32) for _ in range(36)]
    return dict(words=words, args=args, token=token)


def module_ppc(op):
    def run(e, c):
        obj = e.alloc(20, pack(c['words']))
        tok = e.alloc(144, pack(c['token']))
        pipe = e.alloc(0x60, b'\xa5' * 0x60)
        e.write(pipe + 0x24, pack([c['args'][0]]))
        result = e.alloc(4, pack([0xa5a5a5a5]))
        e.cl = PpcLog({obj: 1, tok: 2, result: 3})
        if op == -1:
            address = 0x100122f0
            args = [T.I(obj)]
        elif op == -2:
            address = 0x100123c4
            args = [T.I(obj), T.I(pipe)]
        elif op == -3:
            address = 0x1001249c
            args = [T.I(c['args'][0])]
        else:
            name, expected, slot, codes, ret = MODULE[op]
            # Read the descriptor from the object's real Mac vtable for virtual dispatch.
            descriptor = e.u32(e.u32(obj) + slot)
            address = e.u32(descriptor)
            assert address == expected
            args = [T.I(result), T.I(obj)] if ret == 'TbError' else [T.I(obj)]
            for i, code in enumerate(codes):
                if code == 'f':
                    args.append(T.F(T.bits_f32(c['args'][i])))
                elif code == 'i':
                    args.append(T.I(c['args'][i]))
                elif code == 'p':
                    args.append(T.I(pipe))
                else:
                    args.append(T.I(tok + 12 if code == 't' else tok))
        r3, _, f1 = T.call_ppc(e, address, args)
        out = dict(words=image(e, obj, 5), intact=True, direct=c['token'][0], buffer=c['token'][0], log=e.cl.log)
        if op >= 0 and MODULE[op][0] in ('GetDirectSoundObject', 'GetDirectSoundBuffer'):
            copied = list(c['token'])
            copied[0] = 0
            out['intact'] = image(e, tok, 36) == tuple(copied)
            out['direct' if MODULE[op][0] == 'GetDirectSoundObject' else 'buffer'] = e.u32(tok)
        else:
            out['intact'] = image(e, tok, 36) == tuple(c['token'])
        if op == -3:
            out['frequency'] = double_bits(f1)
        elif op == -2 or (op >= 0 and MODULE[op][4] not in ('void',)):
            out['result'] = e.u32(result) if op >= 0 and MODULE[op][4] == 'TbError' else r3
        return out
    return run


def module_host(op):
    def run(lib, c):
        words, args, token = array(c['words']), array(c['args']), array(c['token'])
        out = array([0] * 9)
        value = C.c_double()
        host_begin(lib, {})
        lib.tpw_sm_module(op, words, args, token, out, C.byref(value))
        result = dict(words=tuple(out[:5]), intact=bool(out[6]), direct=out[7], buffer=out[8], log=host_end(lib))
        if op == -3:
            result['frequency'] = double_bits(value.value)
        elif op == -2 or (op >= 0 and MODULE[op][4] != 'void'):
            result['result'] = out[5]
        return result
    return run


def value_case(r=None, word=0, other=0, argument=0, bits=26, cached=0, mask=0):
    return dict(words=[word, other], args=[argument, bits, cached, mask]) if r is None else dict(
        words=[r.choice(EDGES) if r.random() < .5 else r.getrandbits(32) for _ in range(2)],
        args=[r.getrandbits(32), r.choice([0, 1, 25, 26, 27, 31, 32, 33, 63, 64, 65, MASK]),
              r.choice([0, 1, 0x80, 0xff]), r.getrandbits(32)])


def value_ppc(op):
    def run(e, c):
        if 'steps' in c:
            return sequence_ppc(0 if op == 12 else 1, e, c)
        w, a = c['words'], c['args']
        obj = e.alloc(8, pack(w))
        e.write(0x10035c88, pack([a[1]]))
        e.write(0x1003e320, bytes([a[2] & 255]))
        e.write(0x1003e324, pack([a[3]]))
        e.write(0x1003f350, pack([w[1]]))
        args = [T.I(obj)]
        if op in (1, 4):
            args.append(T.I(a[0]))
        elif op == 16:
            args += [T.I(a[0]), T.I(a[1])]
        elif op in (19, 20):
            args.append(T.I(obj + 4))
        r3, _, _ = T.call_ppc(e, VALUES[op][1], args)
        h, atomic, state, singleton, pointer, priority, lock = w[0], w[0], w[0], w[1], w[0], w[1], w[0]
        if op <= 5:
            h = e.u32(obj)
        elif op <= 9:
            atomic = e.u32(obj)
        elif op <= 14:
            state = e.u32(obj)
        elif op <= 16:
            pointer, priority = image(e, obj, 2)
        elif op <= 18:
            lock = e.u32(obj)
        result = r3 if op in (2, 3, 5, 7, 8, 9, 14) else int(r3 == 0x1003f350) if op == 11 else int(r3 == obj) if op == 18 else int(r3 == obj + 4) if op in (19, 20) else 0
        return dict(state=(h, atomic, state, e.u32(0x1003f350), pointer, priority, lock, result,
                           e.read(0x1003e320, 1)[0], e.u32(0x1003e324)))
    return run


def value_host(op):
    def run(lib, c):
        if 'steps' in c:
            return sequence_host(0 if op == 12 else 1, lib, c)
        out = array([0] * 10)
        lib.tpw_sm_value(op, array(c['words']), array(c['args']), out)
        return dict(state=tuple(out))
    return run


def lifetime_case(r=None, initial=0, flag=0, null=False, pointer=True, count=16):
    if r is not None:
        initial = r.getrandbits(32)
        flag = r.choice(FLAGS)
        null = bool(r.getrandbits(1))
        pointer = bool(r.getrandbits(1))
        count = r.randrange(257)
    return dict(initial=initial, flag=flag, null=null, pointer=pointer, count=count)


def lifetime_ppc(kind):
    def run(e, c):
        if 'steps' in c:
            return sequence_ppc(2, e, c)
        obj = 0 if c['null'] and kind not in (1, 5, 6) else e.alloc(20, b'\xa5' * 20)
        word = e.alloc(4, pack([c['initial']]))
        data = e.alloc(max(c['count'], 16), b'\xa5' * max(c['count'], 16)) if c['pointer'] else 0
        e.cl = PpcLog({obj: 1, word: 2, data: 4})
        e.write(0x1003f348, pack([data]))
        if obj:
            if kind == 0:
                e.write(obj, pack([c['initial']]))
            elif kind in (1, 2):
                e.write(obj, pack([word if c['pointer'] else 0]))
            elif kind in (5, 6, 7):
                e.write(obj, pack([data]))
            elif kind == 4:
                T.call_ppc(e, 0x100122f0, [T.I(obj)])
        args = [T.I(obj), T.I(word if c['pointer'] else 0)] if kind == 1 else [T.I(obj), T.I(c['count'])] if kind == 5 else [T.I(obj)] if kind == 6 else [T.I(obj), T.I(c['flag'])]
        r3, _, _ = T.call_ppc(e, LIFETIMES[kind][1], args)
        out = dict(log=e.cl.log)
        if kind == 0:
            out['state'] = (e.u32(obj) if obj else c['initial'],)
        elif kind in (1, 2):
            stored = e.u32(obj) if obj else word if c['pointer'] else 0
            out['state'] = (e.u32(word), 2 if stored == word else 0 if stored == 0 else MASK)
        elif kind in (3, 4):
            state = [4 if e.u32(0x1003f348) else 0]
            state += [e.read(obj, 1)[0] if obj else 0xa5] if kind == 3 else list(image(e, obj + 4, 4)) if obj else [0, 0, 22050, 0xa5a5a5a5]
            out['state'] = tuple(state)
        else:
            stored = e.u32(obj) if obj else data
            label = e.cl.lab(stored) if kind == 5 else 4 if stored == data and data else 0 if stored == 0 else MASK
            intact = not stored or e.read(stored, c['count']) == b'\xa5' * c['count']
            result = e.cl.lab(e.u32(obj)) if kind == 5 else e.cl.lab(r3) if kind == 6 else 0
            out['state'] = (result, label, int(intact))
        return out
    return run


def lifetime_host(kind):
    def run(lib, c):
        if 'steps' in c:
            return sequence_host(2, lib, c)
        out = array([0] * 5)
        host_begin(lib, {})
        lib.tpw_sm_lifetime(kind, c['initial'], c['flag'], c['null'], c['pointer'], c['count'], out)
        n = 5 if kind == 4 else 3 if kind >= 5 else 2 if kind in (1, 2, 3) else 1
        return dict(state=tuple(out[:n]), log=host_end(lib))
    return run


def buffer_case(r=None, word=0, value=0):
    words = [0xa5a5a5a5] * 24 if r is None else [r.getrandbits(32) for _ in range(24)]
    words[9] = word if r is None else r.getrandbits(32)
    return dict(words=words, value=value if r is None else r.getrandbits(32))


def buffer_ppc(setter):
    def run(e, c):
        obj = e.alloc(0x60, pack(c['words']))
        address = 0x10000f54 if setter else 0x1001245c
        args = [T.I(obj), T.I(c['value'])] if setter else [T.I(obj)]
        r3, _, _ = T.call_ppc(e, address, args)
        expected = list(c['words'])
        if setter:
            expected[9] = c['value']
        return dict(value=e.u32(obj + 0x24) if setter else r3, intact=image(e, obj, 24) == tuple(expected))
    return run


def buffer_host(setter):
    def run(lib, c):
        out = array([0, 0])
        lib.tpw_sm_buffer(setter, array(c['words']), c['value'], out)
        return dict(value=out[0], intact=bool(out[1]))
    return run


def module_edges(op):
    if op == -3:
        # Single conversion, powf overflow/underflow and reciprocal subnormal boundaries.
        extra = [0x7fffffff, -0x80000000, -0x7fffffff, 0x7ffffffe]
        for boundary in (96, 5777, 8701, 12096, 12192, 12288, 14208, 14304, 14400, 1 << 24, 1 << 25):
            for delta in (-2, -1, 0, 1, 2):
                extra += [boundary + delta, -boundary - delta]
        return [module_case(pitch=i) for i in range(-200, 201)] + [module_case(pitch=i) for i in extra]
    cases = [module_case()]
    for value in EDGES:
        c = module_case()
        c['args'] = [value] * 4
        c['token'] = [value] * 36
        cases.append(c)
    if op >= 0 and MODULE[op][0] == 'Init':
        for mask in range(256):
            c = module_case()
            c['token'][5:8] = [0x12345678, mask, 0x87654321]
            cases.append(c)
    return cases


def value_edges(op):
    cases = [value_case(word=x, other=y, argument=z) for x in EDGES for y in (0, x, MASK) for z in (0, 1, 0xffff)]
    if op == 4:
        # Every ushort index, with random-looking prior count bits including bit 26.
        cases += [value_case(word=(i * 0x9e3779b9) & MASK, argument=i) for i in range(65536)]
    if op == 12:
        cases += [dict(words=[w, 0], steps=steps) for w in EDGES for steps in
                  ([0, 0, 2, 1, 1, 1, 2], [1, 1, 0, 2, 0], [2] * 8)]
    if op == 16:
        cases += [dict(words=[7, w], steps=[w, 100, w, 101, (w - 1) & MASK, 102,
                                          (w + 1) & MASK, 103, 0, 0, 0, 105]) for w in EDGES]
    return cases


def lifetime_edges(kind):
    cases = [lifetime_case(initial=w, flag=f, null=n, pointer=p, count=count)
            for w in (0, 1, MASK, 0x7fffffff, 0x80000000)
            for f in FLAGS for n in (False, True) for p in (False, True)
            for count in ((0, 1, 16) if kind >= 5 else (16,))]
    if kind == 1:
        cases += [dict(words=[w, 0], pointer=p, steps=[1] * n + [0] * n)
                  for w in EDGES for p in (False, True) for n in (1, 2, 16, 32)]
    return cases


def sequence_ppc(kind, e, c):
    steps = c['steps']
    out = []
    if kind == 0:
        e.write(0x1003f350, pack([c['words'][0]]))
        for step in steps:
            address, _, _ = T.call_ppc(e, 0x1000a97c, [])
            if step != 2:
                T.call_ppc(e, 0x10010da4 if step == 0 else 0x1000a8e8, [T.I(address)])
            count, _, _ = T.call_ppc(e, 0x1000bdd4, [T.I(address)])
            out += [count, int(address == 0x1003f350)]
    elif kind == 1:
        obj = e.alloc(8, pack(c['words']))
        for i in range(0, len(steps), 2):
            T.call_ppc(e, 0x10010804, [T.I(obj), T.I(steps[i]), T.I(steps[i + 1])])
            out += list(image(e, obj, 2))
    else:
        word = e.alloc(4, pack([c['words'][0]]))
        stack = []
        for step in steps:
            if step:
                obj = e.alloc(4)
                T.call_ppc(e, 0x10000738, [T.I(obj), T.I(word if c['pointer'] else 0)])
                stack.append(obj)
            else:
                T.call_ppc(e, 0x100006a8, [T.I(stack.pop()), T.I(MASK)])
            out += [e.u32(word), len(stack)]
    return dict(trace=tuple(out))


def sequence_host(kind, lib, c):
    steps = c['steps']
    count = len(steps) // 2 if kind == 1 else len(steps)
    out = array([0] * (count * 2))
    lib.tpw_sm_sequence(kind, c['words'][0], c['words'][1], c.get('pointer', True), array(steps), count, out)
    return dict(trace=tuple(out))


def random_value(op, r):
    if op in (12, 16) and r.random() < .5:
        steps = [r.randrange(3) for _ in range(r.randrange(1, 65))] if op == 12 else [
            r.choice(EDGES) if r.random() < .5 else r.getrandbits(32) for _ in range(r.randrange(1, 33) * 2)]
        return dict(words=[r.getrandbits(32), r.getrandbits(32)], steps=steps)
    return value_case(r)


def random_lifetime(kind, r):
    if kind == 1 and r.random() < .5:
        depth = 0
        steps = []
        for _ in range(r.randrange(1, 65)):
            step = 1 if depth == 0 else 0 if depth == 32 else r.randrange(2)
            depth += 1 if step else -1
            steps.append(step)
        steps += [0] * depth
        return dict(words=[r.getrandbits(32), 0], pointer=bool(r.getrandbits(1)), steps=steps)
    return lifetime_case(r)


TESTS = []
for op, name, addr in [(-1, 'TbSoundSystemModule::TbSoundSystemModule', 0x100122f0),
                       (-2, 'TbSoundSystemModule::GetBufferSize', 0x100123c4),
                       (-3, 'TbSoundSystemModule::ConvertPitchIndexToFrequency', 0x1001249c)]:
    TESTS.append(FnTest(f'{name} 0x{addr:08x}', module_ppc(op), module_host(op),
                        edge=lambda op=op: module_edges(op), rnd=module_case))
for op, (name, addr, slot, codes, ret) in enumerate(MODULE):
    TESTS.append(FnTest(f'TbSoundSystemModule::{name} 0x{addr:08x}', module_ppc(op), module_host(op),
                        edge=lambda op=op: module_edges(op), rnd=module_case))
for op, (name, addr) in enumerate(VALUES):
    TESTS.append(FnTest(f'{name} 0x{addr:08x}', value_ppc(op), value_host(op),
                        edge=lambda op=op: value_edges(op), rnd=lambda r, op=op: random_value(op, r)))
for kind, (name, addr) in enumerate(LIFETIMES):
    TESTS.append(FnTest(f'{name} 0x{addr:08x}', lifetime_ppc(kind), lifetime_host(kind),
                        edge=lambda kind=kind: lifetime_edges(kind), rnd=lambda r, kind=kind: random_lifetime(kind, r)))
for setter, name, addr in [(0, 'GetBufferSize', 0x1001245c), (1, 'SetBufferSize', 0x10000f54)]:
    TESTS.append(FnTest(f'TbSoundPipeChannel::{name} 0x{addr:08x}', buffer_ppc(setter), buffer_host(setter),
                        edge=lambda: [buffer_case(word=w, value=v) for w in EDGES for v in EDGES], rnd=buffer_case))
