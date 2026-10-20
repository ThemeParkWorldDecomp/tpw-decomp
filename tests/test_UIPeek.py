"""w55: peek filter, highlight, IME and unpause, with actual Mac control vtables.

Each case resets globals and scripts operations, comparing every effect word, all IME bytes,
window fields and ordered boundary calls after every operation. External calls are recording
fakes on both sides; EM_SetEmitersOn uses the same deterministic count/handle model. The
GetAttr accessor and CoolRect lifecycle are real. The other window accessors record all
arguments and implement their already translated behavior. No game window is opened.
"""
import ctypes as C
import math
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = 'SimTheme Park'
SHIM_LIBRARY = 'unit_UIPeek'
FN = [0x1017FDF8, 0x1007A934, 0x1007AB1C, 0x1007AB60, 0x1007AC14,
      0x101F6C84, 0x101F6D68, 0x101F6E0C, 0x101867F8, 0x101EF8F0]
NAMES = ['EffectPeekMsg', 'BTN_HighLight_On', 'BTN_HighLight_Move', 'BTN_HighLight_Kill',
         'BTN_HighLight_Off', 'CUIIME::Activate', 'CUIIME::DeActivate',
         'CUIIME::InvalidateStoredString', 'UnPauseUI', 'UIInterface::UnPause']
VT = [0x102A7D94, 0x102AF974, 0x102A7CC4]
SIZE = [0x138, 0x15C, 0x168]
EFFECTS, IME, SETTINGS = 0x102DAB10, 0x103966C0, 0x1038071C
BASE_FLAG, SCALE_FLAG, BASE, SCALE = 0x102DAB08, 0x102DAB00, 0x102DAB0C, 0x102DAB04
MINIMUM, MAXIMUM, BANK, MAIN_WINDOW, ROOT_WINDOW = 0x1029D30C, 0x1029D308, 0x102F7CA0, 0x102B2BB4, 0x10395C24
SAVED = [(EFFECTS, 204), (IME, 0x180), (SETTINGS + 0x36, 1), (BASE_FLAG, 1), (SCALE_FLAG, 1),
         (BASE, 4), (SCALE, 4), (MINIMUM, 4), (MAXIMUM, 4), (BANK, 4), (MAIN_WINDOW, 4), (ROOT_WINDOW, 4)]
MESSAGES = list(range(0x25)) + [0x100, 0x101, 0x800, 0x801, 0x802] + list(range(0x10000, 0x10004))


def w32(e, a, v):
    e.write(a, struct.pack('>I', v & 0xFFFFFFFF))


def w16(e, a, v):
    e.write(a, struct.pack('>h', T.s16(v)))


def w8(e, a, v):
    e.write(a, bytes([v & 255]))


def s16(e, a):
    return struct.unpack('>h', e.read(a, 2))[0]


def operation(op, slot=2, message=0, a=72, b=-37, f=None):
    return [op, slot, message, a, b, f or [0.25, -0.5, 0.04, 0.04]]


def fixture(ops, **changes):
    # Flags, areas, limits; attrs; rect; animation; state, hilight; edit max/length/buffer/lock;
    # IME owner/edit/active/shorts; tooltips, bank, main window, root window.
    a = [0, 0, CL.fbits(0.123), CL.fbits(0.456), 2, 5, 0,
         0, 0, 2048, 1536, 0, 0, 0, 0, 1, 1, 255, 7, 1, 1,
         0, 2, 1, -37, -99, 1, 0x12345678, 1, 0]
    for key, value in changes.items():
        if key == 'rect':
            a[7:11] = value
        elif key == 'anim':
            a[11:15] = value
        else:
            a[int(key[1:])] = value
    effects = [0] * 51
    for effect in range(3):
        for i in range(11):
            effects[17 * effect + i] = CL.fbits((i + effect + 1) / 17)
        effects[17 * effect + 11] = 5
        effects[17 * effect + 12:17 * effect + 17] = [0, 0x1234, 0x87654321, 0, 7]
    return dict(a=a, effects=effects, ops=ops, q={'Clock': [0xF1234567], 'Activate': [0]})


def normalized(words, log):
    # Compare floating state bit for bit except NaNs, as the harness conventions require.
    words = list(words)
    for effect in range(3):
        for i in range(11):
            index = effect * 17 + i
            bits = words[index]
            if bits & 0x7F800000 == 0x7F800000 and bits & 0x7FFFFF:
                words[index] = 0x7FC00000
    result = []
    for name, ints, texts in log:
        if name == 'Trace':
            level, bits, count = ints
            bits &= 0xFFFFFFFFFFFFFFFF
            if bits & 0x7FF0000000000000 == 0x7FF0000000000000 and bits & 0xFFFFFFFFFFFFF:
                bits = 0x7FF8000000000000
            ints = (level, T.s32(bits >> 32), bits & 0xFFFFFFFF, count)
        result.append((name, ints, texts))
    return dict(state=words, log=result)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in SAVED}
        self.windows = [e.alloc(n, b'\xa5' * n) for n in SIZE]
        self.L = e.cl = CL.PpcLog({w: 10 + i for i, w in enumerate(self.windows)} |
                                 {IME + 0x1C: 30}, case['q'])
        e.peek = self
        a = case['a']
        e.write(EFFECTS, struct.pack('>51I', *case['effects']))
        for addr, value in [(BASE_FLAG, a[0]), (SCALE_FLAG, a[1]), (SETTINGS + 0x36, a[26])]:
            w8(e, addr, value)
        for addr, value in [(BASE, a[2]), (SCALE, a[3]), (MINIMUM, a[4]), (MAXIMUM, a[5]),
                            (BANK, a[27]), (MAIN_WINDOW, self.windows[0] if a[28] else 0),
                            (ROOT_WINDOW, self.win(a[29]))]:
            w32(e, addr, value)
        for i, w in enumerate(self.windows):
            w32(e, w + 0x118, VT[i])
            w32(e, w + 0x44, a[6])
            w32(e, w + 0x88, 70 + i)
            w32(e, w + 0x58, self.windows[2] if i == 0 else 0)
            w32(e, w + 0x50, 0)
            w32(e, w + 4, 0x102AF8FC)
            for j in range(4):
                w16(e, w + 8 + 2 * j, a[7 + j])
                w16(e, w + 0xF4 + 2 * j, a[11 + j])
        w8(e, self.windows[1] + 0x138, a[15])
        w8(e, self.windows[1] + 0x139, a[16])
        edit = self.windows[2]
        w32(e, edit + 0x13C, a[17])
        text = CL.new_wide(e, 'x' * a[18]) if a[19] else 0
        w32(e, edit + 0x138, text)
        w8(e, edit + 0x161, a[20])
        e.write(IME, b'\xa5' * 0x180)
        w32(e, IME, self.win(a[21]))
        w32(e, IME + 4, self.win(a[22]))
        w8(e, IME + 0xC, a[23])
        w16(e, IME + 0x17C, a[24])
        w16(e, IME + 0x17E, a[25])
        self.original_windows = [bytes(e.read(w, n)) for w, n in zip(self.windows, SIZE)]
        self.text = text
        self.original_text = bytes(e.read(text, 2 * (a[18] + 1))) if text else b''

    def win(self, slot):
        return 0 if slot < 0 else self.windows[slot]

    def op(self, op):
        code, slot, message, a, b, f = op
        if code == 0:
            return T.s32(T.call_ppc(self.e, FN[code], [T.Ptr(self.win(slot)), T.I(message), T.I(a), T.I(b)])[0])
        if code in (1, 2):
            T.call_ppc(self.e, FN[code], [T.F(v) for v in f[:4 if code == 1 else 2]])
        elif code in (5, 7):
            T.call_ppc(self.e, FN[code], [T.Ptr(IME), T.Ptr(self.win(slot))])
        elif code in (6, 9):
            T.call_ppc(self.e, FN[code], [T.Ptr(IME)])
        else:
            T.call_ppc(self.e, FN[code], [])
        return 0

    def dump(self):
        e = self.e
        words = list(struct.unpack('>51I', e.read(EFFECTS, 204)))
        words += [e.read(BASE_FLAG, 1)[0], e.read(SCALE_FLAG, 1)[0], e.u32(BASE), e.u32(SCALE),
                  T.s32(e.u32(MINIMUM)), T.s32(e.u32(MAXIMUM))]
        words += [self.L.lab(e.u32(IME)), self.L.lab(e.u32(IME + 4)), e.read(IME + 12, 1)[0],
                  s16(e, IME + 0x17C), s16(e, IME + 0x17E)]
        words += list(e.read(IME + 8, 4)) + list(e.read(IME + 13, 15)) + list(e.read(IME + 0x1C, 0x160))
        for w in self.windows:
            words += [e.u32(w + 0x44)] + [s16(e, w + 8 + 2 * j) for j in range(4)]
            words += [s16(e, w + 0xF4 + 2 * j) for j in range(4)]
        words += [T.s8(e.read(self.windows[1] + 0x138, 1)[0]), T.s8(e.read(self.windows[1] + 0x139, 1)[0]),
                  T.s32(e.u32(self.windows[2] + 0x13C)), e.read(self.windows[2] + 0x161, 1)[0],
                  e.read(SETTINGS + 0x36, 1)[0], T.s32(e.u32(BANK)), self.L.lab(e.u32(MAIN_WINDOW)),
                  self.L.lab(e.u32(ROOT_WINDOW))]
        for i, (w, n) in enumerate(zip(self.windows, SIZE)):
            current = bytearray(e.read(w, n))
            if i == 2:
                current[0x161] = self.original_windows[i][0x161]
            words.append(int(current == self.original_windows[i]))
        words.append(int(not self.text or bytes(e.read(self.text, len(self.original_text))) == self.original_text))
        return normalized(words, self.L.log)

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)


def build_stubs():
    st = CL.Stubs()
    st.real = FN + [0x101DBB70, 0x101C7448, 0x101C74E0]
    R = CL.recorder
    for addr, kind in [(0x101D9EB0, 1), (0x101CB010, 2), (0x101CD7A8, 5)]:
        st.internal(addr, R('WhatAmI', 'L', ret=lambda e, L, kind=kind: kind))
    st.internal(0x101CB048, R('GetState', 'L', ret=lambda e, L: T.s8(e.read(e.r(3) + 0x138, 1)[0])))

    def position(e):
        w, out, subtract = e.r(3), e.r(4), T.s16(e.r(5))
        values = [T.s16(s16(e, w + 8 + 2 * j) - (s16(e, w + 0xF4 + 2 * j) if subtract else 0)) for j in range(4)]
        for j, value in enumerate(values):
            w16(e, out + 4 + 2 * j, value)
        e.cl.rec('GetPosition', (e.cl.lab(w), e.cl.lab(out), subtract, *values))
        e.set_r(3, 1)

    def get_window(e):
        w, id_ = e.r(3), T.s32(e.r(4))
        e.cl.rec('GetWindow', (e.cl.lab(w), id_))
        child = e.u32(w + 0x58)
        while child and T.s32(e.u32(child + 0x88)) != id_:
            child = e.u32(child + 0x50)
        e.set_r(3, child)

    def current_len(e):
        e.cl.rec('GetCurrentLen', (e.cl.lab(e.r(3)),))
        text = e.u32(e.r(3) + 0x138)
        n = 0
        while text and s16(e, text + 2 * n):
            n += 1
        e.set_r(3, T.s16(n) & 0xFFFFFFFF)

    def lock(e):
        e.cl.rec('LockOutMouse', (e.cl.lab(e.r(3)), e.r(4)))
        w8(e, e.r(3) + 0x161, e.r(4))

    def activate(e):
        out, obj, window = e.r(3), e.r(4), e.r(5)
        e.cl.rec('IME.Activate', (e.cl.lab(obj), e.cl.lab(window)))
        e.write(out, struct.pack('>4i', T.s32(e.cl.ret('Activate')), 0x123, -7, 42))

    def emitters(e):
        effect, count, emitter = T.s32(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5))
        e.cl.rec('Emitters', (effect, count, emitter))
        addr = EFFECTS + 0x44 * effect
        w32(e, addr + 0x2C, count)
        for i in range(5):
            w32(e, addr + 0x30 + 4 * i, 100 + i if i < count else 0)
        e.set_r(3, 0)

    def clock(e):
        now = e.cl.ret('Clock') & 0xFFFFFFFF
        e.cl.rec('Clock', (now,))
        e.set_r(3, now)

    def trace(e):
        bits = e.r(5) << 32 | e.r(6)
        if bits >= 1 << 63:
            bits -= 1 << 64
        e.cl.rec('Trace', (e.r(3), bits, T.s32(e.r(7))), (e.cstr(e.r(4)),))

    st.internal(0x101DB884, position)
    st.internal(0x101DBE40, get_window)
    st.internal(0x101CF138, current_len)
    st.internal(0x101CF1AC, lock)
    st.internal(0x101FEAB0, activate)
    st.internal(0x101FEAE8, R('IME.UnActivate', 'L'))
    st.internal(0x101FEB18, R('IME.MaxLength', 'Li'))
    st.internal(0x100DD9FC, R('Sound', 'iiiiii', ret='i'))
    st.internal(0x1006FCBC, trace)
    st.internal(0x1007A454, emitters)
    st.internal(0x100BE260, R('Mask', 'ii'))
    st.internal(0x10007C68, clock)
    st.internal(0x101C9518, R('Send', 'Liii', ret='i'))
    st.internal(0x101D8BA0, R('UnPauseTimers', ''))
    st.internal(0x101A8744, R('HidePause', ''))
    return st


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = build_stubs()


def ppc(e, case):
    saved = STUBS.activate(e)
    world = Ppc(e, case)
    try:
        out = []
        for op in case['ops']:
            answer = world.op(op)
            out.append(dict(answer=answer, **world.dump()))
        return out
    finally:
        world.restore()
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    lib.tpw_peek_reset.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_uint32)]
    lib.tpw_peek_op.argtypes = [C.c_int32] * 5 + [C.POINTER(C.c_float)]
    lib.tpw_peek_dump.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_peek_dump.restype = C.c_int32
    CL.host_begin(lib, {}, case['q'])
    lib.tpw_peek_reset((C.c_int64 * 30)(*case['a']), (C.c_uint32 * 51)(*case['effects']))
    out = []
    try:
        for op in case['ops']:
            code, slot, message, a, b, f = op
            answer = lib.tpw_peek_op(code, slot, message, a, b, (C.c_float * 4)(*f))
            buf = (C.c_int64 * 1024)()
            n = lib.tpw_peek_dump(buf)
            log = CL.host_end(lib)
            C.c_int32.in_dll(lib, 'tpw_cl_on').value = 1
            out.append(dict(answer=answer, **normalized(list(buf[:n]), log)))
        return out
    finally:
        CL.host_end(lib)


def edges(op):
    if op == 0:
        for msg in MESSAGES:
            for kind in range(3):
                for effects in (0, 1):
                    for attr in (0, 16):
                        for state in (0, 1, 2):
                            for highlight in (0, 1):
                                yield fixture([operation(0, kind, msg)], a26=effects, a6=attr, a15=state, a16=highlight)
        for rect in [(0, 0, 1, 1), (-7, -11, -10, -16), (2047, 1535, 2048, 1536),
                     (-32768, -32768, 32767, 32767), (32767, 32767, -32768, -32768)]:
            for anim in [(0, 0, 0, 0), (1, -1, 7, -9), (-32768, 32767, 32767, -32768)]:
                yield fixture([operation(0, 1, m) for m in [0x10001, 0x13, 0x12, 0x13, 0x10002, 0x100, 0x14]],
                              rect=rect, anim=anim)
        for msg in MESSAGES:
            if msg != 0x801:
                yield fixture([operation(0, 2, msg)], a22=-1, a21=-1)
        for result in [-2147483648, -1, 0, 1, 2147483647]:
            c = fixture([operation(0, 2, m) for m in [0x1F, 0x801, 0x20, 0x1F, 0x14]])
            c['q']['Activate'] = [result]
            yield c
    elif op in (1, 2):
        floats = [0.0, -0.0, 0.04, -0.04, 1.0, 100.0, 1e20, -1e20, math.inf, -math.inf, math.nan]
        for width in floats:
            for height in floats:
                yield fixture([operation(op, f=[width, height, width, height])])
        # triage 2026-10-13a (doubtful 4): areas within a few ulps of every emitter-count threshold,
        # (area - 0.0016f) / 0.0051f * 3 crossing an integer; the clamp (2 to 5) is moved too so every count shows
        for k in range(-3, 9):
            threshold = 0.0016 + k * 0.0051 / 3
            bits = struct.unpack('>I', struct.pack('>f', threshold))[0]
            for ulps in range(-3, 4):
                area = struct.unpack('>f', struct.pack('>I', bits + ulps))[0]
                for height in (1.0, 2.0, 0.5):
                    width = area / height
                    yield fixture([operation(op, f=[0.25, -0.5, width, height])])
                yield fixture([operation(op, f=[0.25, -0.5, area, 1.0])], a4=-1, a5=9)
        for flags in [(0, 0), (1, 0), (0, 1), (1, 1), (-1, -128)]:
            c = fixture([operation(op), operation(op, f=[-1.0, 1.0, 0.00001, 0.00001])], a0=flags[0], a1=flags[1])
            c['q']['Clock'] = [0, 0x80000000, 0xFFFFFFFF]
            yield c
        for lo, hi in [(2, 5), (-8, -3), (5, 2), (0, 0), (-5, 5)]:
            yield fixture([operation(op)], a4=lo, a5=hi)
    elif op in (3, 4):
        for count in [-3, -1, 0, 1, 2, 3, 4, 5]:
            for mask in range(32):
                c = fixture([operation(op), operation(op)])
                c['effects'][45] = count & 0xFFFFFFFF
                c['effects'][46:51] = [0 if mask & (1 << i) else (0xF1230000 + i) for i in range(5)]
                yield c
    elif op in (5, 6, 7):
        for owner in (-1, 0, 1, 2):
            for edit in (-1, 2):
                for result in (-2147483648, -1, 0, 1, 2147483647):
                    for capacity in [0, 1, 255, 32767, 32768, 65535, 65536, -1, -2147483648, 2147483647]:
                        # Invalidate(NULL) with mEditBox NULL dereferences NULL on the Mac; do not execute it.
                        c = fixture([operation(op, 2 if op != 7 else 0)], a21=owner, a22=edit, a17=capacity)
                        if op == 7:
                            c['ops'] += [operation(7, 2)]
                        c['q']['Activate'] = [result]
                        yield c
        for length in [0, 1, 255, 32767, 32768, 65535, 65536]:
            yield fixture([operation(op)], a18=length, a19=1)
            yield fixture([operation(op)], a18=length, a19=0)
    else:
        for root in (-1, 0, 1, 2):
            yield fixture([operation(op), operation(op)], a29=root)


def random_case(r, op):
    c = fixture([operation(op)])
    a = c['a']
    a[0:2] = [r.choice([0, 1, -1]), r.choice([0, 1, -1])]
    a[6] = r.getrandbits(32)
    a[7:15] = [r.randint(-32768, 32767) for _ in range(8)]
    a[15:17] = [r.choice([-128, -1, 0, 1, 2, 127]), r.choice([-1, 0, 1, 2])]
    a[17] = T.rint(r)
    a[18] = r.randint(0, 1024)
    a[19] = r.randint(0, 1)
    a[20] = r.randint(0, 255)
    a[21] = r.choice([-1, 0, 1, 2])
    a[22] = r.choice([-1, 2])
    a[23] = r.randint(0, 255)
    a[24:26] = [r.randint(-32768, 32767), r.randint(-32768, 32767)]
    a[26] = r.randint(0, 1)
    a[27] = T.rint(r)
    a[28] = r.randint(0, 1)
    a[29] = r.choice([-1, 0, 1, 2])
    c['q'] = {'Clock': [r.getrandbits(32) for _ in range(3)], 'Activate': [T.rint(r) for _ in range(3)]}
    for effect in range(3):
        c['effects'][17 * effect:17 * effect + 11] = [CL.fbits(T.rfloat_mod(r)) for _ in range(11)]
        c['effects'][17 * effect + 11] = r.randint(0, 5)
        c['effects'][17 * effect + 12:17 * effect + 17] = [r.choice([0, r.getrandbits(32)]) for _ in range(5)]
    if op == 0:
        kind = r.randrange(3)
        msgs = [r.choice(MESSAGES) if r.random() < 0.8 else T.rint(r) for _ in range(5)]
        c['ops'] = [operation(op, 0 if msg == 0x801 else kind, msg,
                              72 if msg == 0x801 else r.choice([72, -1, 0, 71]), T.rint(r)) for msg in msgs]
        # A failed lookup with no active edit box reaches the Mac's unchecked NULL dereference.
        if 0x801 in msgs:
            a[22] = 2
    elif op in (1, 2):
        c['ops'] = [operation(op, f=[T.rfloat_mod(r, -10, 10) for _ in range(4)]) for _ in range(3)]
    elif op == 7:
        c['ops'] = [operation(op, r.choice([0, 1, 2])) for _ in range(3)]
    return c


TESTS = [FnTest(name, ppc, host, edge=lambda op=op: edges(op), rnd=lambda r, op=op: random_case(r, op))
         for op, name in enumerate(NAMES)]
