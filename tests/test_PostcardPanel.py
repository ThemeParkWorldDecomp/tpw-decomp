"""w102 postcard panel. Every external boundary records every argument on both sides.

Build is faked; GetWindow uses a small linked tree. Render-area and CoolRect lifetimes
run real code. Queued answers cover allocation failures and unavailable UI resources.
All buffers, window writes, global words and ordered allocation/deletion events compare.
"""
import ctypes as C
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = 'SimTheme Park'
SHIM_LIBRARY = 'unit_PostcardPanel'
FN = [0x101b232c, 0x101b20b4, 0x101b19fc, 0x101b1660, 0x101b2050,
      0x101b0ae4, 0x101b1494, 0x1014e678, 0x101efb6c]
NAMES = ['UI_DoPostcard', 'UI_ActuallyDoPostcard', 'CreatePostcardPanel', 'PostcardPanelMsg',
         'RemovePostcardPanel', 'RefreshPreviewWindow', 'PostcardPreviewMsg', 'TakePostCard', 'UIInterface::DoPostcard']
CARD, PANEL, ONLINE = 0x103956f4, 0x103956fc, 0x10380ab0
SENT, DRAG, LASTX, LASTY, OFFX, OFFY = 0x10394b14, 0x10394b10, 0x10394b0c, 0x10394b08, 0x10394b1c, 0x10394b20
MSGEDIT, ADDREDIT, FETCHED = 0x10394ba5, 0x10394ba4, 0x10395026
MESSAGE, ADDRESS, SUBJECT, TEMP = 0x10395028, 0x1039522a, 0x103952ec, 0x10394e26
GAME, TYPE, GUARD = 0x1037f134, 0x102b3aa0, 0x102b3aa4
WANTED, DELAY = 0x103956f0, 0x10394b04
ROOT, GAMEWIN, ONLINEWIN = 0x10395c24, 0x103949e4, 0x10395e94
WIDTH, HEIGHT, TABLE = 0x10395bbc, 0x10395bba, 0x1037ff18 + 0x564
WORDS = [CARD, PANEL, ONLINE, SENT, DRAG, LASTX, LASTY, OFFX, OFFY, GAME + 0x3c, WANTED, DELAY,
         ROOT, GAMEWIN, ONLINEWIN, TYPE, TABLE]
SAVED = [(x, 4) for x in WORDS] + [(x, 1) for x in (MSGEDIT, ADDREDIT, FETCHED, GUARD)] + [
    (MESSAGE, 0x200), (ADDRESS, 0xc0), (SUBJECT, 0x400), (TEMP, 0x200), (WIDTH, 2), (HEIGHT, 2)]


def w32(e, a, n):
    e.write(a, struct.pack('>I', T.u32(n)))


def w16(e, a, n):
    e.write(a, struct.pack('>h', T.s16(n)))


def w8(e, a, n):
    e.write(a, bytes([n & 255]))


def fixture(op, message=0, a=0, b=0, **changes):
    # type, panel, card, online, child mask, texture, surface size, drag, last coordinates,
    # offsets, edited flags, temp flag, sent, game flag, card dimensions/pixels, failing new,
    # wanted, delay, real screen dimensions.
    values = [0, 0, 1, 1, 15, 4, 317, 193, 0, 19, 27, 10, 20, 0, 0, 0, 9, 5,
              640, 240, 1, 0, 7, 8, 1600, 1200]
    q = dict(CanEmail=[1], Build=[1], Surface=[1], Valid=[1], Hidden=[1], Default=[-7], DefaultPanel=[17])
    ops = [[op, message, a, b]]
    for key, value in changes.items():
        if key == 'ops':
            ops = value
        elif key == 'q':
            q.update(value)
        else:
            values[int(key[1:])] = value
    return dict(v=values, q=q, ops=ops)


class Ppc:
    def __init__(self, e, case):
        self.e = e
        texture = e.u32(0x10260b84)
        direct = e.u32(0x10260b28)
        self.texture = texture
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in SAVED + [(texture + 4, 4), (direct, 4)]}
        v = case['v']
        self.windows = [e.alloc(0x168, b'\0' * 0x168) for _ in range(6)]
        self.card = e.alloc(0x70, b'\0' * 0x70)
        self.online = e.alloc(0x404)
        self.pixels = e.alloc(64)
        self.surface = e.alloc(64, b'\x77' * 64)
        self.direct = e.alloc(0x20)
        self.font = e.alloc(4)
        self.cursor = e.alloc(4)
        self.table = e.alloc(4)
        fixed = {w: 10 + i for i, w in enumerate(self.windows)} | {
            self.card: 20, self.online: 21, GAME: 22, self.direct: 23, self.font: 24, self.cursor: 25,
            self.table: 26, MESSAGE: 30, ADDRESS: 31, SUBJECT: 32, TEMP: 33, self.surface: 34,
            self.pixels: 35, 0x102ac00a: 36}
        self.L = e.cl = CL.PpcLog(fixed, case['q'])
        e.cardworld = self
        self.heap = [0, 0, 0, 0]
        self.alloc_count = 0
        self.fail_at = v[21]
        self.strings = [CL.new_wide(e, s) for s in ('title', 'address', 'message', 'subject')]
        self.error = CL.new_wide(e, 'bad address')
        for i, w in enumerate(self.windows):
            w32(e, w + 0x118, 0x102a7cc4 if i >= 3 else 0x102a7d94)
            w32(e, w + 0x88, 0x5d9f if i == 2 else 0x5da0 + i)
        children = [self.windows[i] for i in range(2, 6) if v[4] & (1 << (i - 2))]
        w32(e, self.windows[1] + 0x58, children[0] if children else 0)
        for i, w in enumerate(children):
            w32(e, w + 0x50, children[i + 1] if i + 1 < len(children) else 0)
        for addr, val in [(CARD, self.card if v[2] else 0), (PANEL, self.windows[1] if v[1] else 0),
                          (ONLINE, self.online if v[3] else 0), (TYPE, v[0]), (SENT, v[16]), (DRAG, v[8]),
                          (LASTX, v[9]), (LASTY, v[10]), (OFFX, v[11]), (OFFY, v[12]), (GAME + 0x3c, v[17]),
                          (WANTED, v[22]), (DELAY, v[23]), (ROOT, self.windows[0]), (GAMEWIN, self.windows[0]),
                          (ONLINEWIN, self.windows[0]), (TABLE, self.table), (texture + 4, v[5]), (direct, self.direct)]:
            w32(e, addr, val)
        for addr, val in [(MSGEDIT, v[13]), (ADDREDIT, v[14]), (FETCHED, v[15]), (GUARD, 1)]:
            w8(e, addr, val)
        w16(e, WIDTH, v[24])
        w16(e, HEIGHT, v[25])
        for addr, val in [(self.card + 0x50, v[18]), (self.card + 0x54, v[19]), (self.card + 0x5c, self.pixels if v[20] else 0)]:
            w32(e, addr, val)
        for addr, n, byte, term in [(MESSAGE, 0x200, 0x41, 6), (ADDRESS, 0xc0, 0x42, 8), (SUBJECT, 0x400, 0x43, 10)]:
            e.write(addr, bytes([byte]) * n)
            e.write(addr + term, b'\0\0')
        e.write(TEMP, b'\x44' * 0x200)
        w8(e, TEMP + 7, 0)

    def dump(self):
        e, L = self.e, self.L
        out = [L.lab(e.u32(x)) for x in (CARD, PANEL, ONLINE)]
        out += [T.s32(e.u32(x)) if x != LASTY else e.u32(x) for x in (SENT, DRAG, LASTX, LASTY, OFFX, OFFY)]
        out += [e.read(x, 1)[0] for x in (MSGEDIT, ADDREDIT, FETCHED)]
        out += [T.s32(e.u32(x)) for x in (GAME + 0x3c, WANTED, DELAY)]
        for i, w in enumerate(self.windows):
            out += [e.u32(w + 0x44), L.lab(e.u32(w + 0x98)), 2 if e.u32(w + 0x110) == 0x10267cf0 else 0]
            if i >= 3:
                out += [L.lab(e.u32(w + 0x138)), T.s32(e.u32(w + 0x13c)), L.lab(e.u32(w + 0x15c))]
        for addr, n in [(MESSAGE, 256), (ADDRESS, 96), (SUBJECT, 512)]:
            out += list(struct.unpack('>' + str(n) + 'H', e.read(addr, n * 2)))
        out += list(e.read(TEMP, 512)) + list(e.read(self.surface, 64)) + self.heap
        return out

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)


def build_stubs():
    S = CL.Stubs()
    S.real.update(FN + [0x101c7448, 0x101c74e0])
    R = CL.recorder
    for addr, name, args, result in [
        (0x101802cc, 'Sound', 'i', None), (0x10180514, 'CheckPanel', '', None),
        (0x101805a0, 'SetPanel', 'L', None), (0x10180794, 'Title', 'LiS', None),
        (0x101821cc, 'Advisor', 'i', None), (0x1018227c, 'CancelAdvisor', 'i', None),
        (0x10187434, 'OutOfFirstPerson', '', None), (0x1018493c, 'DefaultPanel', 'Liii', 'i'),
        (0x1018116c, 'DecideKb', '', None), (0x10182304, 'EnableKeys', '', None),
        (0x10182364, 'DisableKeys', '', None), (0x101ca0b0, 'HideCursor', '', None),
        (0x101ca110, 'ShowCursor', '', None), (0x1007ab60, 'KillHighlight', '', None),
        (0x10008f00, 'KillAdvisor', 'i', None), (0x101e568c, 'ChatPre', '', None),
        (0x101e57bc, 'ChatFull', '', None), (0x101e5700, 'ChatReinstate', '', None),
        (0x101a8c1c, 'Hidden', '', 'i'), (0x101a8ac0, 'Toggle', '', None),
        (0x101c9518, 'Send', 'Liii', 'i'), (0x101b07a4, 'AddressBook', '', None),
        (0x101b0d88, 'Valid', '', 'i'), (0x101b0f9c, 'SendPostcard', '', None),
        (0x101dcc10, 'Capture', 'L', None), (0x101dcc6c, 'UnCapture', 'L', None),
        (0x101de318, 'Default', 'Liii', 'i'), (0x101dbd1c, 'Finish', 'L', None),
        (0x100c07c4, 'Load', 'L', None), (0x100c09b4, 'Generate', 'L', None),
        (0x1016f7d0, 'Online.Ctor', 'L', None), (0x101717cc, 'CanEmail', 'L', 'i'),
        (0x101f1df0, 'Error', 'iL', None), (0x1018f1d0, 'Cursor.Ctor', 'LL', None),
        (0x1018c978, 'Sprite.Ctor', 'LL', None), (0x101da118, 'Font', 'LL', None),
        (0x101da3ac, 'Colour', 'Liiii', None), (0x101da2d8, 'Wrap', 'Lii', None),
        (0x101da204, 'Just', 'Lii', None), (0x101cdac4, 'CursorDraw', 'LL', None)]:
        S.internal(addr, R(name, args, ret=result))

    def new(e):
        W = e.cardworld
        W.alloc_count += 1
        size = e.r(3)
        p = 0 if W.alloc_count == W.fail_at else e.alloc(size, b'\xa5' * size)
        label = W.L.newlab(p) if p else 0
        W.L.rec('new', (size, label))
        W.heap[0] += bool(p)
        e.set_r(3, p)

    def card_ctor(e):
        e.cl.rec('Postcard.Ctor', (e.cl.lab(e.r(3)),))
        e.write(e.r(3), b'\0' * 0x70)

    def card_dtor(e):
        W = e.cardworld
        p = e.r(3)
        e.cl.rec('Postcard.Dtor', (e.cl.lab(p),))
        if T.s16(e.r(4)) > 0:
            W.heap[2] += 1
            e.cl.rec('delete', (e.cl.lab(p),))
            e.cl.forget(p)

    def pause(e):
        e.cl.rec('Pause', (e.cl.lab(e.r(3)), T.s32(e.r(4)), e.r(5), T.s32(e.u32(e.r(3) + 0x3c))))

    def unpause(e):
        e.cl.rec('Unpause', (e.cl.lab(e.r(3)), T.s32(e.u32(e.r(3) + 0x3c))))

    def lock(e):
        W = e.cardworld
        e.cl.rec('Lock', (e.cl.lab(e.r(4)),))
        words = [0x12340000 + i for i in range(31)]
        words[9] = W.pixels
        e.write(e.r(3), struct.pack('>31I', *words))

    def grab(e):
        e.cl.rec('Grab', (e.cl.lab(e.r(3)),))
        words = struct.unpack('>31I', e.read(e.r(4), 124))
        for i, val in enumerate(words):
            e.cl.rec('Desc', (i, e.cl.lab(val) if i == 9 else val))

    def preview(e):
        e.cl.rec('Preview', (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7))))
        w8(e, e.r(4), e.r(7))

    def draw(e):
        ints = (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))) + tuple(T.s32(e.r(i)) for i in range(5, 11))
        ints += (e.u32(e.r(1) + 0x38), T.s32(e.u32(e.r(1) + 0x3c)))
        e.cl.rec('DrawText', ints, (CL.wide_text(e, e.r(3)),))

    def temp(e):
        e.cl.rec('Temp', (e.r(3), e.cl.lab(e.r(4))))
        e.write(e.r(4), b'temp:postcard\0')

    def build(e):
        e.cl.rec('Build', (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), 1 if e.r(5) == 0x10267ce8 else -1))
        e.set_r(3, e.cardworld.windows[1] if e.cl.ret('Build') else 0)

    def get_window(e):
        w, id_ = e.r(3), T.s32(e.r(4))
        e.cl.rec('GetWindow', (e.cl.lab(w), id_))
        child = e.u32(w + 0x58)
        while child and T.s32(e.u32(child + 0x88)) != id_:
            child = e.u32(child + 0x50)
        e.set_r(3, child)

    def callback(e):
        e.cl.rec('Callback', (e.cl.lab(e.r(3)), 2 if e.r(4) == 0x10267cf0 else -1))
        w32(e, e.r(3) + 0x110, e.r(4))

    def client(e):
        e.cl.rec('ClientDraw', (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
        w32(e, e.r(3) + 0x98, e.r(4))

    def cursor(e):
        e.cl.rec('CursorDraw', (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
        w32(e, e.r(3) + 0x15c, e.r(4))

    def area(e):
        w, p, n = e.r(3), e.r(4), e.r(5)
        e.cl.rec('TextArea', (e.cl.lab(w), e.cl.lab(p), n, struct.unpack('>H', e.read(p, 2))[0],
                              struct.unpack('>H', e.read(p + n * 2, 2))[0]))
        w32(e, w + 0x138, p)
        w32(e, w + 0x13c, n)

    def text(e):
        w, p = e.r(3), e.r(4)
        value = CL.wide_text(e, p)
        e.cl.rec('SetText', (e.cl.lab(w),), (value,))
        out = e.u32(w + 0x138)
        if out:
            value = value[:e.u32(w + 0x13c)]
            e.write(out, value.encode('utf-16-be') + b'\0\0')
        e.set_r(3, 1)

    def show(e):
        e.cl.rec('Show', (e.cl.lab(e.r(3)), T.s16(e.r(4))))
        w32(e, e.r(3) + 0x44, (e.u32(e.r(3) + 0x44) & ~1) | bool(e.r(4)))

    def surface(e):
        W = e.cardworld
        e.cl.rec('Surface', (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), e.cl.lab(e.r(5))))
        w32(e, e.r(4), W.values[6])
        w32(e, e.r(5), W.values[7])
        e.set_r(3, W.surface if e.cl.ret('Surface') else 0)

    def string(e):
        id_ = e.r(3)
        e.cl.rec('String', (id_,))
        e.set_r(3, e.cardworld.error if id_ == 0xd6 else e.cardworld.strings[id_ - 0xd0])

    def format(e):
        table, out, n, id_ = (e.r(i) for i in range(3, 7))
        e.cl.rec('Format', (e.cl.lab(table), e.cl.lab(out), n, id_))
        val = 'default message' if id_ == 0xd9 else 'default address'
        e.write(out, val[:n - 1].encode('utf-16-be') + b'\0\0')

    def dialog(e):
        p = e.r(3)
        ints = tuple(e.read(p + i, 1)[0] for i in (4, 5, 6)) + (T.s32(e.u32(p + 8)),)
        ints += tuple(T.s32(e.r(i)) for i in range(4, 11))
        e.cl.rec('Dialog', ints, (CL.wide_text(e, e.u32(p)),))
        e.set_r(3, 0)

    for addr, fn in [(0x102384b8, new), (0x100c03a4, card_ctor), (0x100c0478, card_dtor),
                     (0x10149904, pause), (0x101499d0, unpause), (0x10239040, lock), (0x100c0954, grab),
                     (0x100c0ac4, preview), (0x1018bbc4, draw), (0x1023a048, temp), (0x101deb78, build),
                     (0x101dbe40, get_window), (0x101dcba8, callback), (0x101db624, client), (0x101cdac4, cursor),
                     (0x101cd7e0, area), (0x101cda6c, text), (0x101dc1ec, show), (0x101dbc98, surface),
                     (0x10180468, string), (0x10239730, format), (0x101f1800, dialog)]:
        if addr >= 0x10230000:
            def glue(e, fn=fn):
                w32(e, e.r(1) + 0x14, e.r(2))
                fn(e)
            S.internal(addr, glue)
        else:
            S.internal(addr, fn)
    def unlock(e):
        w32(e, e.r(1) + 0x14, e.r(2))
        e.cl.rec('Unlock', (e.cl.lab(e.r(3)),))
    S.internal(0x10239118, unlock)
    S.internal(0x10180270, R('FontEntry', 'u', ret=lambda e, L: e.cardworld.font))
    S.internal(0x101cd2d4, R('FindDraw', 's', ret=lambda e, L: e.cardworld.cursor))
    return S


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = build_stubs()
    original_script = struct.unpack('>238h', runner.emu.read(0x102ac00a, 476))
    translated_script = tuple((C.c_int16 * 238).in_dll(runner.lib, 'gPostcardPanelDescription'))
    assert original_script == translated_script
    # Run the original imported TbRenderArea constructor, copied as executable code into main scratch.
    main = runner.emu
    bullfrog = runner.use('bullfrog symbols shared')
    code = bytes(bullfrog.read(0x10016630, 32))
    runner.use(PROGRAM)
    entry = T.persist_alloc(main, 32)
    main.write(entry, code)
    vector = T.persist_alloc(main, 8)
    main.write(vector, struct.pack('>II', entry, main.toc))
    T.stub_cb(main, '__ct__12TbRenderAreaFv', lambda e, call: call(vector, e.r(3)))


def ppc(e, case):
    saved = STUBS.activate(e)
    world = Ppc(e, case)
    world.values = case['v']
    try:
        out = []
        for op, message, a, b in case['ops']:
            args = [T.Ptr(world.windows[1 if op == 3 else 2]), T.I(message), T.I(a), T.I(b)] if op in (3, 6) else (
                [T.Ptr(world.windows[2])] if op == 5 else [T.Ptr(0x10396440)] if op == 8 else [])
            answer = T.s32(T.call_ppc(e, FN[op], args)[0]) if op in (3, 6, 7) else None
            if answer is None:
                T.call_ppc(e, FN[op], args)
                answer = 0
            out.append(dict(answer=answer, state=world.dump(), log=list(world.L.log)))
        return out
    finally:
        world.restore()
        CL.Stubs.deactivate(e, saved)


def host(lib, case):
    lib.tpw_card_reset.argtypes = [C.POINTER(C.c_int32)]
    lib.tpw_card_op.argtypes = [C.c_int32] * 4
    lib.tpw_card_dump.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_card_dump.restype = C.c_int32
    CL.host_begin(lib, {}, case['q'])
    lib.tpw_card_reset((C.c_int32 * len(case['v']))(*case['v']))
    # Screen dimensions are defined in the real UI_Base source.
    C.c_int16.in_dll(lib, 'gUIRealScreenWidth').value = case['v'][24]
    C.c_int16.in_dll(lib, 'gUIRealScreenHeight').value = case['v'][25]
    out = []
    try:
        for op in case['ops']:
            answer = lib.tpw_card_op(*op)
            buf = (C.c_int64 * 2048)()
            n = lib.tpw_card_dump(buf)
            log = CL.host_end(lib)
            C.c_int32.in_dll(lib, 'tpw_cl_on').value = 1
            out.append(dict(answer=answer, state=list(buf[:n]), log=log))
        return out
    finally:
        CL.host_end(lib)


def edges(op):
    if op in (0, 7, 8):
        for kind in (-1, 0, 1, 2, 7):
            for flag in (-1, 0, 1, 0x7fffffff):
                yield fixture(op, v0=kind, v17=flag)
    elif op == 1:
        for kind in (0, 1, 2):
            for card in (0, 1):
                for hidden in (0, 1, -1):
                    for fetched in (0, 1, 255):
                        for fail in (0, 1):
                            yield fixture(op, v0=kind, v2=card, v15=fetched, v21=fail, q={'Hidden': [hidden]})
        yield fixture(op, ops=[[0, 0, 0, 0], [1, 0, 0, 0], [3, 0x14, 0, 0]])
    elif op == 2:
        for kind in (0, 1, 2):
            for panel in (0, 1):
                for online in (0, 1):
                    for email in (0, 1):
                        for build in (0, 1):
                            yield fixture(op, v0=kind, v1=panel, v3=online, q={'CanEmail': [email], 'Build': [build]})
        for kind in (0, 1):
            for mask in range(16):
                for fail in range(6):
                    yield fixture(op, v0=kind, v3=0, v4=mask, v21=fail)
        for fmt in (-0x80000000, -1, *range(9), 0x7fffffff):
            for card in (0, 1):
                for surface in (0, 1):
                    yield fixture(op, v5=fmt, v2=card, v13=1, v14=1, q={'Surface': [surface]})
        yield fixture(op, ops=[[2, 0, 0, 0], [2, 0, 0, 0]])
    elif op == 3:
        messages = [-0x80000000, -1, 0, 4, 5, 6, 0x13, 0x14, 0x15, 0x16, 0xff, 0x100, 0x101, 0x800, 0x801, 0x802, 0x7fffffff]
        buttons = [-0x80000000, -3, -2, -1, 0, 0x5d9f, 0x5da0, 0x5da1, 0x5da2, 0x5da3, 0x5da4, 0x5da5, 0x7fffffff]
        for kind in (0, 1):
            for message in messages:
                for button in buttons if message in (0x100, 0x801) else [71]:
                    for edit in range(4):
                        for valid in (0, 1):
                            yield fixture(op, message, button, -0x1234567, v0=kind, v1=1, v13=edit & 1,
                                          v14=edit >> 1, q={'Valid': [valid]})
        for card in (0, 1):
            yield fixture(op, 0x14, v2=card)
        for box in (0x5da3, 0x5da4):
            yield fixture(op, ops=[[3, 0x801, box, 0], [3, 0x100, -1, 0]])
        yield fixture(op, ops=[[2, 0, 0, 0], [3, 0x100, 0x5da1, 0], [3, 0x100, -1, 0], [3, 0x14, 0, 0]])
    elif op == 4:
        for panel in (0, 1):
            yield fixture(op, v1=panel)
    elif op == 5:
        for fmt in (-0x80000000, -1, *range(9), 0x7fffffff):
            for card in (0, 1):
                for surface in (0, 1):
                    for pixels in (0, 1):
                        yield fixture(op, v5=fmt, v2=card, v20=pixels, q={'Surface': [surface]})
    elif op == 6:
        for msg in [-0x80000000, -1, 0, *range(0x10001, 0x10008), 0x7fffffff]:
            for drag in (0, 1, -1):
                yield fixture(op, msg, 0x8001ffff, 0xffff8001, v8=drag)
        for width, height in [(1, 1), (1600, 1200), (2048, 1536), (-32768, 32767), (-1, -3), (0, 0), (3072, 4096)]:
            for delta in (-4097, -2049, -2048, -1537, -1536, -1025, -2, -1, 0, 1, 2, 1023, 1535, 1536, 2047, 2048, 4097):
                for offset in (0, 1, 100, 0x7fffffff, -0x80000000):
                    yield fixture(op, 0x10003, 0, v8=1, v9=delta, v10=delta, v11=offset, v12=offset,
                                  v24=width, v25=height)
        yield fixture(op, ops=[[6, 0x10005, 11, 0x002a002d], [6, 0x10003, 0x002b002f, 12],
                              [6, 0x10004, 31, 43], [6, 0x10003, 0x00200021, 1]])


def random_case(r, op):
    case = fixture(op)
    v = case['v']
    v[0] = r.choice([-1, 0, 1, 2, 7])
    for index in (1, 2, 3, 8, 13, 14, 15, 16, 20):
        v[index] = r.choice([0, 1, 2, 255])
    v[1] = int(v[1] != 0)
    v[4] = r.randrange(16) if op == 2 else 15
    v[5] = r.choice([-1, *range(9), -0x80000000, 0x7fffffff])
    for index in (6, 7, 9, 10, 11, 12, 17, 18, 19, 22, 23):
        v[index] = T.s32(r.getrandbits(32))
    v[21] = r.randrange(7)
    v[24:26] = [r.randrange(-32768, 32768), r.randrange(-32768, 32768)]
    case['q'].update({name: [r.choice([0, 1, -1, 7])] for name in ('CanEmail', 'Build', 'Surface', 'Valid', 'Hidden')})
    case['q'].update({name: [T.s32(r.getrandbits(32))] for name in ('Default', 'DefaultPanel')})
    if op == 3:
        case['ops'] = [[op, r.choice([5, 0x14, 0x15, 0x801, 0x100, -1, 0x802]),
                        r.choice([-3, -2, -1, *range(0x5d9f, 0x5da7)]), T.s32(r.getrandbits(32))]]
    if op == 6:
        case['ops'] = [[op, r.choice([0x10003, 0x10004, 0x10005, 0, 0x10006]),
                        T.s32(r.getrandbits(32)), T.s32(r.getrandbits(32))]]
    return case


TESTS = [FnTest(name, ppc, host, edge=lambda op=op: edges(op), rnd=lambda r, op=op: random_case(r, op))
         for op, name in enumerate(NAMES)]
