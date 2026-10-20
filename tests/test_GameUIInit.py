"""w116: park UI set-up/teardown. All unit functions and CoolRect are real on both sides.
External callees record all arguments. Queues drive allocation failures, window answers,
mode answers and callback-like changes to globals. State includes every tag payload byte,
window links, cursor/help words, selection globals, mouse words and exact float bits.
The compiler's game-type guard cannot be reset, so the PPC guard is pre-set in every case.
"""
import ctypes as C
import itertools
import random
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = 'SimTheme Park'
SHIM_LIBRARY = 'unit_GameUIInit'
MASK = 0xffffffff
FUNCTIONS = [
    ('UIInterface::InitGame', 0x101ef6a4, 44), ('InitGameUI', 0x10185e58, 944),
    ('DeInitGameUI', 0x1018622c, 212), ('UI_ClearSelected', 0x10194838, 32),
    ('CMouse::AssignMouseMode', 0x1007e2e0, 312), ('CMMDefault::CMMDefault', 0x1007ea78, 36),
    ('InitTagPanel', 0x10190350, 272), ('CreateStaticTags', 0x10191b28, 240),
    ('InitCashPanel', 0x1019af14, 116), ('InitCoasterPanel', 0x1019c2dc, 116),
    ('InitControlPanel', 0x1019d180, 116), ('InterfaceWindow::setCursor', 0x101dd7f0, 60),
    ('DeInitTagPanel', 0x10190484, 76), ('DestroyStaticTags', 0x10191c40, 140),
    ('InstantRemoveAllTags', 0x10192280, 112), ('DeInitCashPanel', 0x1019afac, 68),
    ('DeInitCoasterPanel', 0x1019c378, 68), ('DeInitControlPanel', 0x1019d21c, 68),
    ('DeInitStaffPanel', 0x101c05d4, 148), ('CMMDefault::~CMMDefault', 0x100813e8, 88),
    ('CMMDefault::GetMode', 0x10081468, 8), ('CMMDefault::OnLeftClick', 0x1007eac4, 4),
    ('CMMDefault::OnRightClick', 0x1007eaf8, 4), ('InterfaceWindow::unlink', 0x101de084, 124),
    ('UIInterface::DeInitGame', 0x101ef6fc, 36), ('CMMDefault::OnAssign', 0x1007eb2c, 60),
]
POINTERS = [0x10395c24, 0x10395c1c, 0x103949e4, 0x103949e0, 0x1038cc5c,
            0x10394824, 0x10394834, 0x10394838, 0x1038cd78, 0x1038cd80,
            0x10395a48, 0x10395a44, 0x10395a40, 0x102dc1f4, 0x102dc1f0,
            0x103947b8, 0x103947b4, 0x103947b0]
FLOATS = [0x102b2b5c, 0x102b2b58, 0x10278a28, 0x10278a2c]
TAGS = 0x1038cd84
SCRIPTS = [(0x102a76da, 41), (0x102a82a0, 137), (0x102a93cc, 54), (0x102a9438, 235), (0x102a9634, 58)]
CALLBACKS = {0: 0, 0x102676a8: 1, 0x102676a0: 2, 0x10267940: 3,
             0x10267948: 4, 0x10267b00: 5, 0x10267b08: 6, 0x10267b10: 7}
STUBS = None


def ws(e, a, values):
    e.write(a, struct.pack('>' + 'I' * len(values), *[v & MASK for v in values]))


def w16(e, a, v):
    e.write(a, struct.pack('>H', v & 0xffff))


def s16(e, a):
    return struct.unpack('>h', e.read(a, 2))[0]


def F(lib, name, result, *args):
    fn = getattr(lib, name)
    fn.restype = result
    fn.argtypes = list(args)
    return fn


def flag_hash(e):
    h = 2166136261
    for i in range(10):
        h = ((h ^ (s16(e, TAGS + i * 0x208 + 4) & 0xffff)) * 16777619) & MASK
    return h


def light_words(e):
    a = e.gi_light
    return [e.u32(a + 0x54)] + [e.u32(a + o) for o in (4, 8, 12, 16, 0x2c, 0x30, 0x34)]


def delete(e, address):
    if address:
        e.cl.heap[2] += 1
    if e.cl.lab(address) > 0:
        e.cl.rec('delete', (e.cl.lab(address),))
    e.cl.forget(address)


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    S = STUBS = CL.Stubs()
    R = CL.recorder
    S.real.update(a for _, a, _ in FUNCTIONS)
    S.real.update([0x101c7448, 0x101c74e0])

    def new(e):
        size = e.r(3)
        a = 0
        if e.cl.ret('New', 1):
            a = e.alloc(max(size, 4), b'\xa5' * max(size, 4))
            e.cl.newlab(a)
            e.cl.heap[0] += 1
        e.cl.rec('operator_new', (size, e.cl.lab(a), flag_hash(e)))
        e.set_r(3, a)

    def release(e):
        e.cl.rec('OnRelease', (e.cl.lab(e.r(3)), e.cl.lab(e.u32(POINTERS[14]))))
        if e.gi_release >= 0:
            ws(e, POINTERS[14], [e.cl.addr.get(e.gi_release, 0)])

    def assign(e, name):
        e.cl.rec(name, (e.cl.lab(e.r(3)), e.cl.lab(e.u32(POINTERS[14]))))

    def destructor(e, name):
        e.cl.rec(name, (e.cl.lab(e.r(3)),))
        if T.s16(e.r(4)) > 0:
            delete(e, e.r(3))

    def online(e):
        e.cl.rec('Online.ctor', (e.cl.lab(e.r(3)),))
        ws(e, e.r(3), [e.gi_online_vt, 0])

    def window_ctor(e):
        R('Window.ctor', 'LLiihhhh')(e)
        ws(e, e.r(3) + 0x50, [0, 0, 0, e.r(4)])
        ws(e, e.r(3) + 0x118, [e.gi_window_vt])

    def create(e):
        R('CreateObject', 'Liiihhhh')(e)
        e.set_r(3, e.cl.addr.get(e.cl.ret('Create', 0), 0))

    def build(e):
        script = next(i + 1 for i, (a, _) in enumerate(SCRIPTS) if a == e.r(4))
        e.cl.rec('Build', (e.cl.lab(e.r(3)), script, CALLBACKS[e.r(5)]))
        e.set_r(3, e.cl.addr.get(e.cl.ret('Build', 5), 0))

    def get_window(e):
        R('GetWindow', 'Li')(e)
        e.set_r(3, e.cl.addr.get(e.cl.ret('Child', 0), 0))

    def position(e):
        R('GetPosition', 'Lxh')(e)
        for o, name in zip((4, 6, 8, 10), ('RectL', 'RectT', 'RectR', 'RectB')):
            w16(e, e.r(4) + o, e.cl.ret(name, 0))
        e.set_r(3, 1)

    def extra(e):
        v = e.cl.lab(e.r(5)) if T.s16(e.r(4)) == 2 else T.s32(e.r(5))
        e.cl.rec('setExtraParam', (e.cl.lab(e.r(3)), T.s16(e.r(4)), v))
        e.set_r(3, 1)

    def update_pointer(e):
        e.cl.rec('UpdatePointerIn')
        for index, key in zip((5, 6, 7, 8), ('SwapCash', 'SwapCoaster', 'SwapControl', 'SwapTag')):
            to = e.cl.ret(key, -1)
            if to >= 0:
                ws(e, POINTERS[index], [e.cl.addr.get(to, 0)])
        e.set_r(3, 1)

    e.gi_mode_vt = T.persist_alloc(e, 0x40)
    for slot, fn in [(8, lambda e: destructor(e, 'Mode.dtor')),
                     (0x2c, R('GetMode', 'L', ret='i')),
                     (0x30, lambda e: assign(e, 'OnAssign')), (0x34, release)]:
        ws(e, e.gi_mode_vt + slot, [T.fake_function(e, fn)])
    e.gi_online_vt = T.persist_alloc(e, 0x40)
    for slot, fn in [(8, lambda e: destructor(e, 'Online.dtor')),
                     (0x2c, R('Online.GetMode', 'L', ret=lambda e, L: 7)),
                     (0x30, lambda e: assign(e, 'Online.OnAssign'))]:
        ws(e, e.gi_online_vt + slot, [T.fake_function(e, fn)])
    e.gi_window_vt = T.persist_alloc(e, 0x40)
    ws(e, e.gi_window_vt + 0x10, [T.fake_function(e, lambda e: destructor(e, 'Window.dtor'))])
    S.imp('__nw__FUl', new)
    S.imp('__dl__FPv', lambda e: delete(e, e.r(3)))
    S.imp('UpdateLights__7CSystemFv', lambda e: e.cl.rec('UpdateLights', (e.cl.lab(e.r(3)), *light_words(e))))
    S.imp('Link__7CMapWhoFP12CMapLinkBasei', R('Link', 'LLi'))
    S.imp('Unlink__7CMapWhoFP12CMapLinkBasei', R('Unlink', 'LLi'))
    for a, name, spec in [(0x101c9e48, 'Flush', ''), (0x1015e970, 'Text.InitTheme', 'L'),
                          (0x1015e9a4, 'Text.DeInitTheme', 'L'), (0x101a7878, 'CreateMainPanel', ''),
                          (0x101c02fc, 'InitStaffPanel', ''), (0x101caaac, 'EnableHelpText', ''),
                          (0x101caa4c, 'DisableHelpText', ''), (0x101c9518, 'UI_SendMessage', 'Liii'),
                          (0x101c9f68, 'UI_SetCursor', 'h'), (0x101dc1ec, 'Show', 'Lh'),
                          (0x101db474, 'SetDrawObject', 'LL'), (0x101db624, 'SetClientDrawObject', 'LL'),
                          (0x1018d11c, 'TextSprite.ctor', 'LL'), (0x101da118, 'SetFont', 'LL'),
                          (0x101da2d8, 'SetFontWrap', 'Lii'), (0x101da204, 'SetFontJust', 'Lii'),
                          (0x101efb0c, 'CheckMouseContext', 'Li'), (0x1006fcbc, 'CTRACE', 'us')]:
        S.internal(a, R(name, spec))
    S.internal(0x1007faac, online)
    S.internal(0x101d98d8, window_ctor)
    S.internal(0x101dc750, create)
    S.internal(0x101deb78, build)
    S.internal(0x101dcba8, lambda e: e.cl.rec('SetCallback', (e.cl.lab(e.r(3)), CALLBACKS[e.r(4)])))
    S.internal(0x101dbe40, get_window)
    S.internal(0x101db884, position)
    S.internal(0x101dcfd0, extra)
    S.internal(0x101c8f18, update_pointer)
    S.internal(0x1018116c, lambda e: e.cl.rec('DecideKbCapture', (e.cl.lab(e.u32(POINTERS[4])),)))
    S.internal(0x10180270, R('GetFontTableEntry', 'u', ret=lambda e, L: L.addr[30]))
    S.internal(0x101cd2d4, R('FindDrawObject', 's', ret=lambda e, L: L.addr.get(L.ret('Draw', 31), 0)))


def state(case):
    # A word-based input shared with the host; all float inputs are raw bit patterns.
    st = [1, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 25, 26, 15, 16, 17,
          0x12345678, 0x87654321, 640, 480, 1, 0, -1]
    st += [0x12345678, 0x76543210, 0x3f800000, 0x40000000]
    st += [0x3e800000] * 7 + [0x12345678, 0x55]
    st += list(itertools.chain.from_iterable((15 + i, 0) for i in range(10)))
    st += list(itertools.chain.from_iterable((-23131, 0xa5a5a5a5, 0, 0, 0, 0) for _ in range(24)))
    for index, value in case.get('st', {}).items():
        st[index] = value
    return st


class Ppc:
    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        self.L = e.cl = CL.PpcLog({}, case.get('q'))
        self.L.heap = [0, 0, 0, 0]
        self.L.addr = {}
        self.saved = STUBS.activate(e)
        for label in range(1, 37):
            a = e.alloc(0x200, b'\xa5' * 0x200)
            self.L.labels[a] = label
            self.L.addr[label] = a
        self.L.addr[35] = 0x1037ff18
        self.L.addr[36] = 0x10396440
        self.L.addr[32] = e.u32(0x102606e4)
        for label in (32, 35, 36):
            self.L.labels[self.L.addr[label]] = label
        ws(e, e.u32(0x102607e4), [self.a(33)])
        e.gi_light = e.alloc(0x68, bytes(0x68))
        ws(e, 0x1038cc78, [e.gi_light])
        ws(e, e.gi_light, [self.a(34)])
        for i in range(10):
            a = TAGS + i * 0x208 + 6
            self.L.labels[a] = 40 + i
            self.L.addr[40 + i] = a
        for label in range(26, 29):
            ws(e, self.a(label), [e.gi_mode_vt, 0])
        for label in range(1, 25):
            ws(e, self.a(label) + 0x118, [e.gi_window_vt])
        st = state(case)
        for a, label in zip(POINTERS, st):
            ws(e, a, [self.a(label)])
        ws(e, 0x102dc1e0, [st[18], st[19]])
        w16(e, 0x10395bbc, st[20])
        w16(e, 0x10395bba, st[21])
        e.write(0x1038071c + 0x36, bytes([st[22] & 255]))
        ws(e, 0x102b3aa0, [st[23]])
        e.write(0x102b3aa4, b'\1')
        e.gi_release = st[24]
        for a, bits in zip(FLOATS, st[25:29]):
            ws(e, a, [bits])
        for o, bits in zip((4, 8, 12, 16, 0x2c, 0x30, 0x34), st[29:36]):
            ws(e, e.gi_light + o, [bits])
        ws(e, e.gi_light + 0x54, [st[36]])
        for i in range(10):
            a = TAGS + i * 0x208
            ws(e, a, [self.a(st[38 + i * 2])])
            w16(e, a + 4, st[39 + i * 2])
            e.write(a + 6, bytes((st[37] + i * 7 + j) & 255 for j in range(0x202)))
        for i in range(24):
            values = st[58 + i * 6:64 + i * 6]
            w16(e, self.a(i + 1) + 0x4c, values[0])
            ws(e, self.a(i + 1) + 0x8c, [values[1]])
            ws(e, self.a(i + 1) + 0x50, [self.a(label) for label in values[2:]])

    def a(self, label):
        return self.L.addr.get(label, 0)

    def dump(self):
        e, L = self.e, self.L
        out = [L.lab(e.u32(a)) for a in POINTERS]
        out += [T.s32(e.u32(0x102dc1e0)), T.s32(e.u32(0x102dc1e4)), s16(e, 0x10395bbc), s16(e, 0x10395bba),
                int(e.read(0x10380752, 1)[0] != 0), T.s32(e.u32(0x102b3aa0)), e.gi_release]
        out += [T.s32(e.u32(a)) for a in FLOATS]
        out += [T.s32(e.u32(e.gi_light + o)) for o in (4, 8, 12, 16, 0x2c, 0x30, 0x34, 0x54)]
        for i in range(10):
            a = TAGS + i * 0x208
            w = e.u32(a)
            out += [L.lab(w), s16(e, a + 4)] + list(e.read(a + 6, 0x202))
            out += [s16(e, w + 0x4c), T.s32(e.u32(w + 0x8c))] if w else [0, 0]
        for i in range(24):
            w = self.a(i + 1)
            out += [s16(e, w + 0x4c), T.s32(e.u32(w + 0x8c))]
            out += [L.lab(e.u32(w + o)) for o in (0x50, 0x54, 0x58, 0x5c)]
        mode = e.u32(POINTERS[14])
        out += [T.s32(e.u32(self.a(29) + 4)), T.s32(e.u32(mode + 4)) if mode else 0]
        out += L.heap
        return out

    def run(self, case):
        op, a, b = case['op'], case.get('a', 1), case.get('b', 0)
        if op == 4:
            args = [self.a(25), self.a(a)]
        elif op in (5, 19, 20, 25):
            args = [self.a(29), b]
            if op == 19:
                T.call_ppc(self.e, FUNCTIONS[5][1], [self.a(29)])
        elif op in (21, 22):
            args = [self.a(29), a, b]
        elif op in (11, 23):
            args = [self.a(a), b]
        elif op in (0, 24):
            args = [self.a(36)]
        else:
            args = []
        result = T.call_ppc(self.e, FUNCTIONS[op][1], [v & MASK for v in args])[0]
        if op == 5:
            assert self.e.u32(self.a(29)) == 0x1029d5f4
        if op == 19:
            # The destructor restores the base vtable, including when it releases the allocation.
            assert self.e.u32(self.a(29)) == 0x1029d6b0
        result = T.s32(self.e.u32(self.a(29) + 4)) if op in (5, 19) else (T.s32(result) if op == 20 else 0)
        return dict(result=result, state=self.dump(), log=self.L.log)


def ppc_case(e, case):
    p = Ppc(e, case)
    try:
        return p.run(case)
    finally:
        CL.Stubs.deactivate(e, p.saved)


def host_case(lib, case):
    F(lib, 'tpw_gi_reset', None)()
    labels = list(range(1, 37)) + list(range(40, 50))
    CL.host_begin(lib, {i: F(lib, 'tpw_gi_obj', C.c_void_p, C.c_int32)(i) for i in labels}, case.get('q'))
    C.c_int32.in_dll(lib, 'tpw_cl_on').value = 0
    st = state(case)
    F(lib, 'tpw_gi_state', None, C.POINTER(C.c_int32))((C.c_int32 * len(st))(*[T.s32(v & MASK) for v in st]))
    C.c_int32.in_dll(lib, 'tpw_cl_on').value = 1
    result = F(lib, 'tpw_gi_call', C.c_int32, C.c_int32, C.c_int32, C.c_int32)(case['op'], T.s32(case.get('a', 1) & MASK),
                                                                         T.s32(case.get('b', 0) & MASK))
    log = CL.host_end(lib)
    out = (C.c_int32 * 6000)()
    n = F(lib, 'tpw_gi_dump', C.c_int32, C.POINTER(C.c_int32))(out)
    return dict(result=result, state=list(out[:n]), log=log)


RECTS = [(0, 0, 2047, 1535), (0, 0, 0, 0), (7, 1, 4, -2), (-1, -1, -2, -2),
         (-32768, -32768, 32767, 32767), (32767, 32767, -32768, -32768), (-100, -101, 1, 2)]
SHORTS = [-32768, -1, 0, 1, 1535, 1536, 2047, 2048, 32767]
FLAGS = [0, 1, -1, 0x100, -32768, 32767]


def case(op, **values):
    return dict(op=op, **values)


def edges(op):
    if op in (0, 1):
        for mouse, mode, tooltip, creates, new in itertools.product((0, 25), (0, 26), (0, 1, 255),
                                                                  ([0, 0], [2, 0], [0, 3], [2, 3]),
                                                                  ([0], [1], [1] * 10 + [0], [1] * 11 + [0])):
            yield case(op, st={13: mouse, 14: mode, 22: tooltip}, q={'Create': creates, 'New': new})
        for rect, width, height in itertools.product(RECTS, SHORTS, SHORTS):
            q = dict(zip(('RectL', 'RectT', 'RectR', 'RectB'), ([v] for v in rect)))
            q.update(Create=[2, 3], Build=[3, 7, 5, 8, 6], Child=[14], New=[1])
            yield case(op, st={20: width, 21: height, 1: 3, 13: 0}, q=q)
        for game, mode_new, online_new in itertools.product((0, 1, 2, -1), (0, 1), (0, 1)):
            yield case(op, st={13: 0, 23: game}, q={'New': [1] * 11 + [mode_new, online_new], 'Create': [2, 3]})
    elif op == 4:
        for cur, release_to, game, mode, answer, new in itertools.product((0, 26), (-1, 0, 28), (0, 1, 2, -1, 3),
                                                                       (0, 27), (0, 1, 7, 9, -1, 2, 0x7fffffff), (0, 1)):
            yield case(op, a=mode, st={14: cur, 23: game, 24: release_to}, q={'GetMode': [answer], 'New': [new]})
    elif op == 11:
        for pointer, cursor in itertools.product((0, 1, 2), SHORTS + [21, 65535, 65536, 0x10015]):
            yield case(op, a=1, b=cursor, st={1: pointer})
    elif op == 23:
        for parent, previous, next_window in itertools.product((0, 2), (0, 3), (0, 4)):
            yield case(op, a=1, st={60: next_window, 61: previous, 63: parent, 68: 1})
    elif op in (6, 8, 9, 10):
        # unlink is real; a NULL Build would dereference NULL and is therefore outside the valid domain.
        for window, child, new in itertools.product((5, 8), (0, 14), (0, 1)):
            yield case(op, q={'Build': [window], 'Child': [child], 'New': [new]})
        key = {6: 'SwapTag', 8: 'SwapCash', 9: 'SwapCoaster', 10: 'SwapControl'}[op]
        yield case(op, q={'Build': [5], key: [8], 'Child': [14]}, st={84: 6, 85: 4, 87: 1})
    elif op == 7:
        for pattern in (0, 1023, 341, 682) + tuple(1 << i for i in range(10)):
            for draw in (0, 31):
                yield case(op, st={39 + i * 2: FLAGS[i % len(FLAGS)] for i in range(10)},
                           q={'New': [(pattern >> i) & 1 for i in range(10)], 'Draw': [draw]})
    elif op == 19:
        for flag in (-32768, -1, 0, 1, 2, 32767, 65536, 65537, 65535):
            yield case(op, b=flag)
    elif op in (13, 14):
        for flag, window in itertools.product(FLAGS, (0, 1)):
            yield case(op, st={k: v for i in range(10) for k, v in ((38 + i * 2, 15 + i if window else 0), (39 + i * 2, flag))})
    elif op == 18:
        for pattern in range(8):
            yield case(op, st={10 + i: 10 + i if (pattern >> i) & 1 else 0 for i in range(3)})
    elif op in (2, 12, 15, 16, 17, 24):
        for pattern in range(256 if op in (2, 24) else 8):
            st = {i: i if (pattern >> (i - 5)) & 1 else 0 for i in range(5, 13)}
            if op in (2, 24):
                st.update({2: 0 if pattern & 1 else 2, 3: 0 if pattern & 2 else 3})
                for i in range(10):
                    st[38 + i * 2] = 15 + i if (pattern >> (i % 8)) & 1 else 0
                    st[39 + i * 2] = FLAGS[(pattern + i) % len(FLAGS)]
            yield case(op, st=st)
    else:
        yield case(op)


def rnd(op, r):
    st = {18: r.getrandbits(32), 19: r.getrandbits(32), 20: r.randrange(-32768, 32768), 21: r.randrange(-32768, 32768),
          22: r.choice([0, 1, 255]), 23: r.choice([0, 1, 2, -1, 3]), 24: r.choice([-1, 0, 28]), 37: r.getrandbits(8)}
    for i in range(2, 13):
        st[i] = r.choice([0, i])
    st[13] = r.choice([0, 25])
    st[14] = r.choice([0, 26])
    for i in range(15, 18):
        st[i] = r.randrange(1, 25)
    for i in range(25, 37):
        st[i] = r.getrandbits(32)
    for i in range(10):
        st[38 + i * 2] = r.choice([0, 15 + i])
        st[39 + i * 2] = r.choice(FLAGS + [r.randrange(-32768, 32768)])
    for i in range(24):
        st[58 + i * 6] = r.randrange(-32768, 32768)
        st[59 + i * 6] = r.getrandbits(32)
    # Random linked sibling configurations for real unlink and each panel's unlink.
    for label in (1, 5, 6, 7, 8):
        k = 58 + (label - 1) * 6
        st[k + 2] = r.choice([0, 13])
        st[k + 3] = r.choice([0, 14])
        st[k + 5] = r.choice([0, 2])
    q = {'New': [r.randrange(2) for _ in range(16)], 'Create': [r.choice([0, 2]), r.choice([0, 3])],
         'Build': [r.choice([5, 6, 7, 8]) for _ in range(5)], 'Child': [r.choice([0, 14])],
         'GetMode': [r.choice([0, 1, 7, 9, -1, r.getrandbits(32)])], 'Draw': [r.choice([0, 31])]}
    for key in ('RectL', 'RectT', 'RectR', 'RectB'):
        q[key] = [r.randrange(-32768, 32768)]
    a = r.choice([0, 27]) if op == 4 else (r.getrandbits(32) if op in (21, 22) else 1)
    b = r.getrandbits(32)
    return case(op, a=a, b=b, st=st, q=q)


def table_ppc(e, c):
    a, _ = SCRIPTS[c['table']]
    return list(struct.unpack('>' + 'h' * c['n'], e.read(a + c['start'] * 2, c['n'] * 2)))


def table_host(lib, c):
    p = F(lib, 'tpw_gi_script', C.POINTER(C.c_int16), C.c_int32)(c['table'])
    return [p[c['start'] + i] for i in range(c['n'])]


def table_edges():
    for i, (_, n) in enumerate(SCRIPTS):
        yield dict(table=i, start=0, n=n)


def table_rnd(r):
    i = r.randrange(5)
    n = SCRIPTS[i][1]
    start = r.randrange(n)
    return dict(table=i, start=start, n=r.randrange(1, n - start + 1))


TESTS = [FnTest(name, ppc_case, host_case, edge=lambda op=op: edges(op), rnd=lambda r, op=op: rnd(op, r))
         for op, (name, _, _) in enumerate(FUNCTIONS)]
TESTS += [FnTest('five park window scripts, word for word', table_ppc, table_host, edge=table_edges, rnd=table_rnd)]
