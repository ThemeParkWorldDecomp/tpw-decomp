"""Unit w37c: 19 originals, real default window handling and window text setter.

Every UI boundary and draw virtual records all semantic arguments. UTF-16 libc
stubs are functional and unrecorded. Cases compare state, full buffers, globals,
heap traffic and call order after each operation. Unsafe original states (invalid
selection indices, null queries subsequently dereferenced, nonpositive grid width
and zero line-height divisions) are excluded explicitly.
"""
import ctypes as C
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIEditBox"
FN = dict(enumerate([0x101CF1EC, 0x101CE990, 0x101CEBC4, 0x101CDD8C,
                    0x101CDC94, 0x101CD858, 0x101CDA6C, 0x101CDAC4,
                    0x101CE8A4, 0x101CD7E0, 0x101CF138, 0x101CF1AC,
                    0x101CE94C, 0x101CD468, 0x101CD65C, 0x1018A850,
                    0x1018A88C, 0x101D8460, 0x101DC5D0]))
NAMES = ["EditBox.Callback", "Edit", "Keydown", "MoveCursor", "NearestCharToPos",
         "_SetText", "SetText", "SetCursorDrawObject", "SelectAll", "SetStaticTextArea",
         "GetCurrentLen", "LockOutMouse", "SetUnAcceptedChars", "CalcClientArea",
         "EditBox.destructor", "DrawFunction", "ClientDraw", "Timer.Start", "CheckClientMouse"]
G = dict(redraw=0x10395C28, height=0x10395BBA, hebrew=0x10395B88,
         buttons=0x10395BD0, help=0x102AF928, focus=0x10395C20, capture=0x10395C14,
         keyboard=0x10395C10, helpresult=0x10395C60, double=0x102AF91C)
CORE = 0x102AF8FC


def w32(e, p, v):
    e.write(p, struct.pack(">I", v & 0xffffffff))


def w16(e, p, v):
    e.write(p, struct.pack(">h", T.s16(v)))


def s16(e, p):
    return struct.unpack(">h", e.read(p, 2))[0]


def s32(e, p):
    return T.s32(e.u32(p))


def rect(e, p):
    return list(struct.unpack(">4h", e.read(p + 4, 8)))


def dump_bytes(values):
    """Canonical bytes retain every field while avoiding recursive scalar comparison."""
    return struct.pack('<' + 'q' * len(values), *values)


def wide(e, p):
    if not p:
        return []
    out = []
    while True:
        v = struct.unpack(">H", e.read(p + len(out) * 2, 2))[0]
        if not v:
            return out
        out.append(v)


class Ppc:
    """A native-vtable counterpart in the Mac layout, with the same fixture labels."""
    def __init__(self, e):
        self.e = e
        e.eb = self
        self.box = e.alloc(0x168, bytes(0x168))
        self.parent = e.alloc(0x138, bytes(0x138))
        self.draws = [e.alloc(0x3c, bytes(0x3c)) for _ in range(3)]
        self.texts = [e.alloc(2048, bytes(2048)) for _ in range(5)]
        self.buffer = e.alloc(2048, bytes(2048))
        self.font = e.alloc(0x14, bytes(0x14))
        self.timer = e.alloc(0x20, bytes(0x20))
        self.help = e.alloc(0x18, bytes(0x18))
        self.query = e.alloc(12, struct.pack(">I4h", CORE, 0, 0, 0, 0))
        w32(e, self.box + 0x118, 0x102A7CC4)
        e.write(self.box + 0x68, b'\xa5' * 32)
        e.write(self.box + 0x10a, b'\xa5' * 6)
        e.write(self.box + 0x15a, b'\xa5' * 2)
        e.write(self.box + 0x163, b'\xa5')
        w32(e, self.parent + 0x118, 0x102A7D94)
        for w in (self.box, self.parent):
            for off in (4, 0x10, 0x1c, 0x28, 0x34):
                w32(e, w + off, CORE)
        for d in self.draws:
            w32(e, d + 0x38, 0x102AFA38)
        # Only height is called through the font vtable; use its actual internal entry.
        tv = e.alloc(8, struct.pack(">II", 0x10168FC0, e.toc))
        vt = e.alloc(16, bytes(12) + struct.pack(">I", tv))
        w32(e, self.font + 0x10, vt)
        w32(e, self.help + 0x10, 0x102AFA88)
        labels = {self.box: 10, self.parent: 11, self.font: 40, self.timer: 41,
                  self.help: 42, self.buffer: 50}
        labels.update({p: 20 + i for i, p in enumerate(self.draws)})
        labels.update({p: 30 + i for i, p in enumerate(self.texts)})
        self.L = e.cl = CL.PpcLog(labels, first_new=100)
        self.heap = [0, 0, 0, 0]
        self.offset_x = self.offset_y = 0
        self.alive = True
        sizes = {"height": 2, "hebrew": 1, "buttons": 36}
        self.saved = {(p, sizes.get(k, 4)): bytes(e.read(p, sizes.get(k, 4))) for k, p in G.items()}
        for (p, n) in self.saved:
            e.write(p, bytes(n))
        w32(e, G["help"], self.help)
        w32(e, G["double"], 500)

    def restore(self):
        for (p, n), data in self.saved.items():
            self.e.write(p, data)

    def call(self, addr, *a):
        return T.s32(T.call_ppc(self.e, addr, [v & 0xffffffff for v in a])[0])

    def setup(self, op, a):
        e, w = self.e, self.box
        if op == 0:
            e.write(self.texts[a[0]], struct.pack(">" + "H" * (a[1] + 1), *a[2:], 0))
        elif op == 1:
            e.write(self.buffer, e.read(self.texts[0], 2048))
            values = [a[0], self.parent, 0 if a[2] < 0 else self.buffer if a[2] == 0 else self.texts[a[2]],
                      a[3], self.font if a[4] else 0, self.draws[0] if a[5] else 0,
                      self.draws[1] if a[6] else 0, self.draws[2] if a[7] else 0,
                      self.timer if a[8] else 0, a[9]]
            for off, v in zip((0x44, 0x5c, 0x138, 0x13c, 0x48, 0x98, 0x94, 0x15c, 0x164, 0x88), values):
                w32(e, w + off, v)
            w32(e, self.parent + 0x44, a[1])
            w16(e, w + 0xdc, a[10])
            w16(e, w + 0xde, a[11])
        elif op == 2:
            for i, v in enumerate(a[:9]):
                w16(e, w + 0x148 + i * 2, v)
            for i, v in enumerate(a[9:12]):
                e.write(w + 0x160 + i, bytes([v & 255]))
            w32(e, w + 0x140, 0 if a[12] < 0 else self.texts[a[12]])
            w32(e, w + 0x144, 0 if a[13] < 0 else self.texts[a[13]])
        elif op == 3:
            e.write(w + (8 if a[0] == 0 else 0x2c), struct.pack(">4h", *[T.s16(v) for v in a[1:]]))
        elif op == 4:
            w32(e, G["redraw"], a[0])
            w16(e, G["height"], a[1])
            e.write(G["hebrew"], bytes([int(bool(a[2]))]))
            self.tick, self.step = a[3] & 0xffffffff, a[4] & 0xffffffff
            w16(e, self.font + 6, a[5])
            w32(e, w + 0x11c, a[6])
            self.columns, self.glyph, self.row, self.x, self.y, self.null = a[7:13]
        elif op == 5:
            e.write(G["buttons"] + a[0] * 12, struct.pack(">4hi", *[T.s16(v) for v in a[1:4]], 0, T.s32(a[4])))
        elif op == 6:
            w16(e, self.draws[a[0]] + 0xe, a[1])
            w16(e, self.draws[a[0]] + 0xc, a[2])
        elif op == 7:
            w32(e, self.timer + 4, a[0])
            w32(e, self.timer + 0x10, a[1])
            e.write(w + 0x135, bytes([a[2] & 255]))
        elif op == 8:
            p = e.u32(w + 0x138)
            for i in range(a[0], a[1]):
                w16(e, p + i * 2, a[2])
            w16(e, p + a[1] * 2, 0)

    def op(self, op, a):
        e, w = self.e, self.box
        args = [w, *a]
        if op == 4:
            args = [self.draws[0] if a[0] else 0, *a[1:]]
        elif op in (5, 6):
            p = 0 if a[0] < 0 else e.u32(w + 0x138) if a[0] == 5 else self.texts[a[0]]
            args = [w, p, *a[1:]]
        elif op == 7:
            args = [w, 0 if a[0] < 0 else self.draws[a[0]]]
        elif op in (9, 12):
            args = [w, 0 if a[0] < 0 else self.texts[a[0]], *a[1:]]
        elif op == 14:
            args = [w, T.s16(a[0])]
        elif op == 11:
            args = [w, int(bool(a[0]))]
        elif op == 17:
            args = [self.timer]
        result = self.call(FN[op], *args)
        if op == 14:
            self.alive = False
        return result if op in (0, 4, 5, 6, 9, 10) else 0

    def dump(self):
        e, w, lab = self.e, self.box, self.L.lab
        out = [int(self.alive)]
        if self.alive:
            for off in (4, 0x10, 0x1c, 0x28, 0x34):
                out.extend(rect(e, w + off))
            out.extend([e.u32(w + 0x44), lab(e.u32(w + 0x48)), s16(e, w + 0x4c)])
            out.extend(lab(e.u32(w + off)) for off in (0x50, 0x54, 0x58, 0x5c))
            out.extend([e.u32(w + 0x60), e.u32(w + 0x64)])
            out.extend(s32(e, w + off) for off in (0x88, 0x8c, 0x90))
            out.extend(lab(e.u32(w + off)) for off in (0x94, 0x98))
            out.extend(lab(e.u32(w + 0x9c + i * 4)) for i in range(16))
            out.extend(s16(e, w + off) for off in (0xdc, 0xde, 0xe0, 0xe2))
            out.extend([lab(e.u32(w + 0xe4)), s16(e, w + 0xe8), s32(e, w + 0xec), s32(e, w + 0xf0)])
            out.extend(s16(e, w + 0xf4 + i * 2) for i in range(4))
            out.append(lab(e.u32(w + 0xfc)))
            out.extend(s16(e, w + 0x100 + i * 2) for i in range(4))
            out.extend([s16(e, w + 0x108), e.u32(w + 0x11c)])
            out.extend(lab(e.u32(w + 0x120 + i * 4)) for i in range(4))
            out.extend([e.u32(w + 0x130), *e.read(w + 0x134, 2), T.s8(e.read(w + 0x136, 1)[0])])
            out.extend([lab(e.u32(w + 0x138)), s32(e, w + 0x13c), lab(e.u32(w + 0x140)), lab(e.u32(w + 0x144))])
            out.extend(s16(e, w + 0x148 + i * 2) for i in range(9))
            out.extend([lab(e.u32(w + 0x15c)), *e.read(w + 0x160, 4), lab(e.u32(w + 0x164))])
            p = e.u32(w + 0x138)
            if p:
                count = max(0, min(s32(e, w + 0x13c) + 1, 1024))
                out.extend(struct.unpack(">" + "H" * count, e.read(p, count * 2)))
        if self.alive:
            out.extend([e.read(w, 1)[0], lab(e.u32(w + 0x40))])
            for off, size in ((0x68, 32), (0x10a, 6), (0x15a, 2)):
                out.extend(e.read(w + off, size))
        for p in self.texts:
            out.extend(struct.unpack(">1024H", e.read(p, 2048)))
        out.extend([lab(e.u32(self.timer)), e.u32(self.timer + 4), s32(e, self.timer + 8),
                    s32(e, self.timer + 12), s32(e, self.timer + 16), s32(e, self.timer + 20),
                    lab(e.u32(self.timer + 24)), lab(e.u32(self.timer + 28))])
        for p in self.draws:
            out.append(lab(p))
            if lab(p) > 0:
                out.extend(s16(e, p + off) for off in (4, 0xe, 0xc))
        out.extend(rect(e, self.query))
        out.extend([self.offset_x, self.offset_y, self.tick, s32(e, G["redraw"]), s16(e, G["height"]), e.read(G["hebrew"], 1)[0]])
        out.extend(lab(e.u32(G[k])) for k in ("focus", "capture", "keyboard"))
        out.append(s32(e, G["helpresult"]))
        for i in range(3):
            out.extend(struct.unpack(">4hi", e.read(G["buttons"] + i * 12, 12)))
        return out


def setup(runner):
    """Scoped fakes preserve other modules' emulator state."""
    e = runner.emu
    st = CL.Stubs()
    st.real = set(FN.values()) | {0x101DE3F8, 0x101DE248, 0x101D8428, 0x101DA564,
                                 0x101DA794, 0x101DD304, 0x101C7448, 0x101C7494,
                                 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7678}
    for addr, name, spec in [(0x101C9400, "Post", "Liii"), (0x101C9518, "Send", "Liii"),
                             (0x101D8A00, "RemoveTimer", "Li"), (0x101C9F68, "Cursor", "i"),
                             (0x101CFB24, "Help.Move", "Lhh"), (0x101CFB68, "Help.In", "LL"),
                             (0x101CFC04, "Help.Out", "L"), (0x101CFD10, "Help.Frame", "L")]:
        st.internal(addr, CL.recorder(name, spec, ret="i"))

    def tick(e):
        f = e.eb
        f.L.rec("Tick", [f.tick])
        e.set_r(3, f.tick)
        f.tick = (f.tick + f.step) & 0xffffffff

    def height(e):
        e.cl.rec("Height", [e.cl.lab(e.r(3))])
        e.set_r(3, s16(e, e.r(3) + 6) & 0xffffffff)

    def capture(e):
        w, on = e.r(3), T.s32(e.r(4))
        e.cl.rec("CaptureKb", [e.cl.lab(w), on])
        prev = e.u32(G["keyboard"])
        if prev != w:
            if prev:
                e.cl.rec("Post", [e.cl.lab(prev), 0x20, on, 0])
                e.cl.ret("Post")
            w32(e, G["keyboard"], w)
            e.cl.rec("Post", [e.cl.lab(w), 0x1f, on, 0])
            e.cl.ret("Post")

    def visibility(e):
        w = e.r(3)
        e.cl.rec("Visibility", [e.cl.lab(w)])
        while w and e.u32(w + 0x44) & 1:
            w = e.u32(w + 0x5c)
        e.set_r(3, int(w == 0))

    def query(e):
        f = e.eb
        p, what, value = e.r(3), e.r(4), e.r(5)
        e.cl.rec("Query", [e.cl.lab(p), what, e.cl.lab(value) if what == 2 else T.s32(value)],
                 [CL.wide_text(e, value) if what == 2 else ""])
        if f.null == what:
            e.set_r(3, 0)
            return
        index = T.s32(value)
        # C++ integer division truncates towards zero.
        q = abs(index) // f.columns * (-1 if index < 0 else 1)
        rem = index - q * f.columns
        x = f.x + (0 if what == 2 else rem * f.glyph) + f.offset_x
        y = f.y + (0 if what == 2 else q * f.row) + f.offset_y
        e.write(f.query + 4, struct.pack(">4h", T.s16(x), T.s16(y), 123, 456))
        e.set_r(3, f.query)

    def change(e):
        f = e.eb
        p, flags, c = e.r(3), e.r(4), e.r(5)
        style = e.u32(c + 0x10) if flags & 0xc100 else 0
        r = e.u32(c + 0x14) if flags & 0x10 else 0
        x = s16(e, style + 10) if flags & 0x4000 else 0
        y = s16(e, style + 12) if flags & 0x8000 else 0
        e.cl.rec("Change", [e.cl.lab(p), flags, s16(e, c + 4) if flags & 1 else 0,
                            s16(e, c + 6) if flags & 2 else 0, s16(e, c + 8) if flags & 4 else 0,
                            *(rect(e, r) if r else [0] * 4), x, y],
                 [CL.wide_text(e, e.u32(style)) if flags & 0x100 else ""])
        if p == f.draws[0]:
            if flags & 0x4000:
                f.offset_x = x
            if flags & 0x8000:
                f.offset_y = y

    def show(e):
        p, a = e.r(3), T.s16(e.r(4))
        e.cl.rec("Draw.Show", [e.cl.lab(p), a, s16(e, p + 0xe), s16(e, p + 0xc)])
        w16(e, p + 4, a)
        if e.cl.ret("ShowMutate"):
            w16(e, p + 0xe, e.cl.ret("ShowRefs"))
            w16(e, p + 0xc, e.cl.ret("ShowLibrary"))

    def delete(e, p, array=False):
        if not p:
            return
        e.eb.heap[3 if array else 2] += 1
        if e.cl.lab(p) > 0:
            e.cl.rec("delete", [e.cl.lab(p)])
            e.cl.forget(p)

    def draw_dtor(e):
        p, flag = e.r(3), T.s16(e.r(4))
        e.cl.rec("Draw.dtor", [e.cl.lab(p)])
        if flag > 0:
            delete(e, p)

    def window_dtor(e):
        e.cl.rec("Window.dtor", [e.cl.lab(e.r(3))])

    def draw(e):
        e.cl.rec("Draw", [e.cl.lab(e.r(3)), T.s16(e.r(4)), *rect(e, e.r(5)), T.s16(e.r(6))])

    def new_array(e):
        n = e.r(3)
        p = e.alloc(n, bytes(n))
        e.eb.heap[1] += 1
        e.cl.rec("new[]", [e.cl.newlab(p), n])
        e.set_r(3, p)

    def copy(e, padded=False):
        p, source = e.r(3), e.r(4)
        data = wide(e, source)
        if padded:
            n = e.r(5)
            data = data[:n] + [0] * max(0, n - len(data))
        else:
            data += [0]
        e.write(p, struct.pack(">" + "H" * len(data), *data))
        e.set_r(3, p)

    def compare(e):
        a, b = wide(e, e.r(3)), wide(e, e.r(4))
        e.set_r(3, 0 if a == b else 1 if a > b else 0xffffffff)

    def find(e):
        p, ch = e.r(3), e.r(4) & 0xffff
        data = wide(e, p) + [0]
        e.set_r(3, p + data.index(ch) * 2 if ch in data else 0)

    st.internal(0x101CAC3C, tick)
    st.internal(0x10168FC0, height)
    st.internal(0x101DCCEC, capture)
    st.internal(0x101DBD9C, visibility)
    st.internal(0x101CCFAC, query)
    st.internal(0x101CCEC8, change)
    st.internal(0x101CCFE8, show)
    st.internal(0x101CCD9C, draw_dtor)
    st.internal(0x101D9BA4, window_dtor)
    st.internal(0x101CCE44, draw)
    st.imp("__dl__FPv", lambda e: delete(e, e.r(3)))
    st.imp("__dla__FPv", lambda e: delete(e, e.r(3), True))
    st.imp("__nwa__FUl", new_array)
    st.imp("wcslen", lambda e: e.set_r(3, len(wide(e, e.r(3)))))
    st.imp("wcscpy", copy)
    st.imp("wcsncpy", lambda e: copy(e, True))
    st.imp("wcscmp", compare)
    st.imp("wcschr", find)
    st.imp("abs", lambda e: e.set_r(3, abs(T.s32(e.r(3))) & 0xffffffff))
    e.eb_stubs = st


def ppc(e, case):
    f = Ppc(e)
    saved = e.eb_stubs.activate(e)
    try:
        for op, args in case["setup"]:
            f.setup(op, args)
        f.L.queues = {k: list(v) for k, v in case["q"].items()}
        states = []
        for op, args in case["ops"]:
            answer = f.op(op, args)
            states.append((answer, dump_bytes(f.dump()), f.heap[:], len(f.L.log)))
        return {"states": states, "log": f.L.log}
    finally:
        f.restore()
        e.eb_stubs.deactivate(e, saved)


def host(lib, case):
    lib.tpw_eb_reset()
    lib.tpw_eb_setup.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_eb_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_eb_dump.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_eb_dump.restype = C.c_int32
    for op, args in case["setup"]:
        lib.tpw_eb_setup(op, (C.c_int64 * max(1, len(args)))(*args))
    CL.host_begin(lib, {}, case["q"], first_new=100)
    lib.tpw_eb_labels()
    heap = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
    for i in range(4):
        heap[i] = 0
    states = []
    for op, args in case["ops"]:
        out = (C.c_int64 * 1)()
        lib.tpw_eb_op(op, (C.c_int64 * max(1, len(args)))(*args), out)
        dump = (C.c_int64 * 8192)()
        count = lib.tpw_eb_dump(dump)
        states.append((out[0], dump_bytes(dump[:count]), list(heap), C.c_int32.in_dll(lib, "tpw_cl_n").value))
    return {"states": states, "log": CL.host_end(lib)}


def fixture(text="abcdef", cap=16, attr=1, anchor=3, selection=-1, rtl=0, lock=0,
            cursor=1, font=1, client=1, timer=1, dirty=0, accepted=-1, rejected=-1,
            redraw=1, columns=100, glyph=8, row=12, x=0, y=0, height=12):
    setup = [(0, [i, len(s), *map(ord, s)]) for i, s in enumerate([text, "replace", "ace", "bd", ""])]
    setup += [(1, [attr, 1, 0, cap, font, client, 1, cursor, timer, 77, 2, 5]),
              (2, [0, anchor, selection, 0, 0, 0, 0, height, 0, dirty, lock, rtl, accepted, rejected]),
              (3, [0, 10, 20, 90, 68]), (3, [1, 10, 20, 90, 68]),
              (4, [redraw, 480, rtl, 1234, 7, 12, 0, columns, glyph, row, x, y, 0]),
              (6, [2, 2, 0])]
    return {"setup": setup, "q": {"Post": [1, 0, -1], "Send": [7, -9]}, "ops": []}


def case(op, args=(), **kw):
    c = fixture(**kw)
    c["ops"] = [(op, list(args))]
    return c


KEYS = [0, 8, 9, 10, 13, 27, 32, 65, 0x2300, 0x2400, 0x2500, 0x2600,
        0x2700, 0x2800, 0x2e00, -1, -32768, 32767]
MSGS = [-1, 0, 1, 2, 3, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x19, 0x1a, 0x1b,
        0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x10000, 0x10001, 0x10002, 0x10003,
        0x10004, 0x10005, 0x10006, 0x10007, 0x10008, 0x10009, 0x1000a,
        0x1000b, 0x1000c, 0x1000d, 0x11006, 0x7fffffff]


def edges(op):
    if op in (1, 2, 3, 5, 6, 8, 10, 14):
        args = {1: [65], 2: [8, 0], 3: [-5, 1, -1], 5: [1, 1], 6: [1], 8: [], 10: [], 14: [0]}[op]
        c = case(op, args)
        c['setup'].append((1, [1, 1, -1, 16, 1, 1, 1, 1, 1, 77, 2, 5]))
        yield c
    if op == 0:
        for m in MSGS:
            for rtl in (0, 1):
                for lock in (0, 1):
                    for attr in (0, 1, 0x81):
                        yield case(op, [m, 0, (30 << 16) | 30], rtl=rtl, lock=lock, attr=attr)
        for m in (0x1000a, 0x1000b, 0x1000c):
            for key in KEYS:
                for shift in (0, 0x30000):
                    yield case(op, [m, key, shift])
        for state in (-1, 0, 1, 2, 32767):
            for sel in (-1, 0, 6):
                c = case(op, [0x10, 123, 456], selection=sel)
                c["setup"].append((2, [0, 3, sel, 0, 0, 0, 0, 12, state, 0, 0, 0, -1, -1]))
                yield c
        for font, client, cursor, timer in ((0, 1, 1, 1), (1, 0, 1, 1), (1, 1, 0, 1), (1, 1, 1, 0)):
            for m in (0x1f, 0x20, 0x13, 0x10005):
                yield case(op, [m, 0, 0], font=font, client=client, cursor=cursor, timer=timer, text="", anchor=0)
    elif op in (1, 2):
        # Insertion shifts characters without writing a new terminator.
        if op == 1:
            for anchor in (0, 3, 6):
                for tail in (0x41, 0xa5a5, 0xffff):
                    c = case(op, [65], anchor=anchor)
                    c['setup'].append((8, [7, 16, tail]))
                    yield c
        for text in ("", "a", "abcdef", "abcdefghijklmnop"):
            for anchor in (0, len(text) // 2, len(text)):
                for sel in (-1, 0, len(text)):
                    for key in (KEYS if op == 2 else [0, 8, 9, 10, 13, 27, 65, 98, 0x5d0, -1]):
                        yield case(op, [key, 0x30000] if op == 2 else [key], text=text, anchor=anchor,
                                   selection=sel, cap=16)
        for rtl in (0, 1):
            for attr in (1, 0x81):
                for key in KEYS:
                    for shift in (0, 0x10000, 0x20000, 0x30000, 0x40000, 0x1030000):
                        yield case(op, [key, shift] if op == 2 else [key], rtl=rtl, attr=attr)
        if op == 1:
            for accepted in (-1, 2, 4):
                for rejected in (-1, 3, 4):
                    for key in (0, ord('a'), ord('b'), ord('c'), 0x5d0, -1):
                        yield case(op, [key], accepted=accepted, rejected=rejected)
    elif op == 3:
        for bounds in ((10, 20, 10, 68), (10, 20, 9, 68), (-32768, 20, 32767, 68),
                       (-32768, -32768, 0, 0), (32767, 32767, -32768, -32768)):
            for attr in (1, 0x81):
                c = case(op, [4, 0, 1], attr=attr, columns=3)
                c['setup'].append((3, [1, *bounds]))
                yield c
        for position in range(-8, 9):
            for selection in (-7, -5, -4, -3, -2, -1, 0, 3, 6, 9):
                for rtl in (0, 1):
                    for attr in (1, 0x81):
                        yield case(op, [position, 1, selection], rtl=rtl, attr=attr, columns=3)
        for x in (-80, -8, -1, 0, 79, 80, 81, 100):
            for y in (-48, -12, -1, 0, 35, 36, 37, 48):
                yield case(op, [4, 1, 1], x=x, y=y, attr=0x81, columns=3)
        for show in (-32768, -1, 0, 1, 32767):
            for height in (0, 1, 12, 32767):
                yield case(op, [-5, show, -1], height=height)
        yield case(op, [-5, 1, -1], font=0)
    elif op == 4:
        for x in (-2147483648, -2147483647, 2147483647):
            for y in (-2147483648, -2147483647, 2147483647):
                yield case(op, [1, x, y, 6], columns=3)
        for present in (0, 1):
            for length in (-2, -1, 0, 1, 6, 20):
                for x in (-32768, -1, 0, 4, 8, 9, 80, 32767):
                    for y in (-32768, 0, 6, 12, 24, 32767):
                        yield case(op, [present, x, y, length], columns=3)
    elif op in (5, 6):
        for attr in (1, 0x41):
            for cap in (0, 3, 6, 7, 16):
                for dirty in (0, 1):
                    for slot in (-1, 0, 1, 4, 5):
                        for notify in (0, 1):
                            yield case(op, [slot, notify] if op == 5 else [slot], attr=attr, cap=cap, dirty=dirty)
        for length in (0, 1, 16, 17, 255, 256, 511):
            c = case(op, [1, 1] if op == 5 else [1], attr=0x41, cap=16)
            c["setup"].insert(0, (0, [1, length, *([65] * length)]))
            # Override after fixture texts were installed.
            c["setup"].append((0, [1, length, *([65] * length)]))
            yield c
    elif op == 7:
        for slot in (-1, 0, 1, 2):
            for refs, library in ((0, 0), (1, 0), (2, 0), (1, 1), (-32768, 32767)):
                for shown in (0, 1):
                    c = case(op, [slot])
                    c["setup"].extend([(6, [2, refs, library]), (1, [shown, shown, 0, 16, 1, 1, 1, 1, 1, 77, 32767, -32768])])
                    yield c
        for refs in (-2, -1, 0, 1, 32767):
            c = case(op, [0])
            c["q"].update(ShowMutate=[1], ShowRefs=[refs], ShowLibrary=[1])
            yield c
    elif op in (8, 10, 15, 16):
        for text in ("", "a", "abcdef", "a" * 255):
            for rtl in (0, 1):
                yield case(op, text=text, cap=max(16, len(text)), anchor=0, rtl=rtl)
    elif op == 9:
        for attr in (0, 1, 0x40, 0x41):
            for slot in (-1, 0, 1, 4):
                for cap in (-2147483648, -1, 0, 1, 255, 2147483647):
                    yield case(op, [slot, cap], attr=attr)
    elif op == 11:
        for flag in (0, 1, 2, -1, 255):
            yield case(op, [flag])
    elif op == 12:
        for slot in (-1, 0, 1, 2, 3, 4):
            yield case(op, [slot])
    elif op == 13:
        for r in ((0, 0, 0, 0), (-32768, -32768, 32767, 32767), (32767, 1, -32768, -1), (99, 88, -77, -66)):
            c = case(op)
            c["setup"].append((3, [0, *r]))
            yield c
    elif op == 14:
        for attr in (1, 0x41):
            for cursor in (0, 1):
                for timer in (0, 1):
                    for refs, library in ((0, 0), (1, 0), (2, 0), (1, 1), (-32768, 32767)):
                        for flag in (-32768, -1, 0, 1, 32767, 65535):
                            c = case(op, [flag], attr=attr, cursor=cursor, timer=timer)
                            c["setup"].append((6, [2, refs, library]))
                            yield c
    elif op == 17:
        for tick in (0, 1, 0x7fffffff, 0x80000000, 0xffffffff):
            c = case(op)
            c["setup"].append((4, [1, 480, 0, tick, 1, 12, 0, 100, 8, 12, 0, 0, 0]))
            c["ops"] *= 3
            yield c
    elif op == 18:
        for x in (9, 10, 11, 89, 90, 91, -32768, 32767):
            for y in (19, 20, 21, 67, 68, 69, -32768, 32767):
                for button in (0, 1, 2):
                    c = case(op, [0x10006, button, ((x & 65535) << 16) | (y & 65535)])
                    c["setup"].append((5, [button, 1, -7, -8, -1]))
                    yield c


def random_case(r, op):
    text = ''.join(r.choice('abcdef') for _ in range(r.randrange(17)))
    anchor = r.randrange(len(text) + 1)
    selection = r.choice([-1, r.randrange(len(text) + 1)])
    c = fixture(text=text, anchor=anchor, selection=selection, rtl=r.randrange(2),
                attr=r.choice([0, 1, 0x81]), dirty=r.randrange(2), lock=r.randrange(2),
                redraw=r.choice([0, 1, -1]), columns=r.randint(1, 7), glyph=r.randint(1, 16),
                x=r.randint(-30, 30), y=r.randint(-30, 30))
    if op == 0:
        m = r.choice(MSGS)
        a, b = r.randrange(3), (r.randint(-30, 100) & 65535) << 16 | r.randrange(100)
        if m in (0x1000a, 0x1000b, 0x1000c):
            a, b = r.choice(KEYS), r.choice([0, 0x30000, T.rint(r)])
        c['ops'] = [(op, [m, a, b])]
    elif op == 1:
        c['ops'] = [(op, [r.choice([0, 8, 9, 10, 13, 27, 65, 0x5d0, -1])])]
    elif op == 2:
        c['ops'] = [(op, [r.choice(KEYS), r.choice([0, 0x30000, T.rint(r)])])]
    elif op == 3:
        c['ops'] = [(op, [r.randint(-8, 24), r.choice([-1, 0, 1]), r.randint(-5, 24)])]
    elif op == 4:
        c['ops'] = [(op, [r.randrange(2), r.randint(-100, 100), r.randint(-100, 100), r.randint(-2, 32)])]
    elif op in (5, 6):
        c['ops'] = [(op, [r.choice([-1, 0, 1, 4, 5]), r.randrange(2)] if op == 5 else [r.choice([-1, 0, 1, 4, 5])])]
    elif op == 7:
        c['ops'] = [(op, [r.randint(-1, 2)])]
        c['setup'].append((6, [2, r.randint(-2, 3), r.randrange(2)]))
    elif op == 9:
        c['ops'] = [(op, [r.randint(-1, 4), r.choice([-1, 0, 1, 16, 255])])]
    elif op in (11, 12):
        c['ops'] = [(op, [r.randint(-1, 4)])]
    elif op == 13:
        c['setup'].append((3, [0, *[r.randint(-32768, 32767) for _ in range(4)]]))
        c['ops'] = [(op, [])]
    elif op == 18:
        c['ops'] = [(op, [T.rint(r), r.randrange(3), T.rint(r)])]
    else:
        c['ops'] = [(op, [r.choice([-1, 0, 1, 32767])])] if op == 14 else [(op, [])]
    return c



def no_buffer(c, attr=1):
    """The fixture with no text buffer at all (the box's buffer pointer is NULL)."""
    c["setup"].append((1, [attr, 1, -1, 16, 1, 1, 1, 1, 1, 77, 2, 5]))
    return c


def with_query(c, held=0, null=0):
    """The fixture's globals again with the box's button word and the query kind that answers NULL (0 none, 1 per-character, 2 whole-text)."""
    c["setup"].append((4, [1, 480, 0, 1234, 7, 12, held, 100, 8, 12, 0, 0, null]))
    return c


def extra(op):
    """Directions of branches the other edges never take (triage-2026-10-12)."""
    if op == 0:
        # the blink tick with no cursor draw object, and with the global redraw flag off
        for cursor in (0, 1):
            for redraw in (0, 1):
                for state in (0, 1, 2):
                    c = case(op, [0x10, 0, 0], cursor=cursor, redraw=redraw)
                    c["setup"].append((2, [0, 3, -1, 0, 0, 0, 0, 12, state, 0, 0, 0, -1, -1]))
                    yield c
        # select all on focus (0x1f) and on double click (0x10007) with no buffer
        for m in (0x1f, 0x10007):
            yield no_buffer(case(op, [m, 0, 0]))
            yield no_buffer(case(op, [m, 0, 0], rtl=1))
        # the mouse move (0x10005) and the drag (0x10003): no buffer, no client draw object, a query that answers NULL
        for m in (0x10005, 0x10003):
            args = [m, 0, (30 << 16) | 30]
            yield with_query(case(op, args), held=1)
            yield with_query(no_buffer(case(op, args)), held=1)
            yield with_query(case(op, args, client=0), held=1)
            yield with_query(case(op, args), held=1, null=1)
            if m == 0x10005:
                yield with_query(case(op, args, font=0), held=1)
    elif op == 1:
        # an edit with no client draw object (rtl keeps the cursor move from reaching it)
        for sel in (-1, 0, 4):
            for anchor in (0, 3):
                yield case(op, [65], client=0, rtl=1, selection=sel, anchor=anchor)
    elif op == 2:
        # keys next to the handled ones fall to the default answer
        for key in (0x2301, 0x24ff, 0x25ff, 0x2601, 0x2701, 0x27ff, 0x2801, 0x2dff, 0x2e01):
            yield case(op, [key, 0], attr=0x81)
            yield case(op, [key, 0x30000])
        # backspace and delete with no client draw object (rtl keeps the cursor move from reaching it)
        for key in (8, 0x2e00):
            for sel in (-1, 0, 4):
                for anchor in (0, 1, 3):
                    if key == 0x2e00 and sel == 0 and anchor == 3:
                        continue    # (the delete moves to the OLD anchor, which is then the new end: the move would reach the client)
                    yield case(op, [key, 0], client=0, rtl=1, selection=sel, anchor=anchor)
    elif op == 3:
        # up/down with no client draw object and nothing to change (selection and anchor as they are)
        for position in (-6, -7):
            for show in (0, 1):
                yield case(op, [position, show, -1], client=0, attr=0x81)
        # up and down on the neighbouring line: two characters at the same distance from the cursor's x (4 is between the columns at 0 and
        # 8): the search stops at the first (strictly closer wins), so the cursor lands on the first one met
        for position, anchor, cursor_y in ((-6, 4, 12), (-7, 1, 0), (-6, 5, 12), (-7, 0, 0)):
            c = case(op, [position, 1, -1], attr=0x81, columns=3, anchor=anchor)
            c["setup"].append((2, [0, anchor, -1, 0, 4, 0, cursor_y, 12, 0, 0, 0, 0, -1, -1]))
            yield c
        # multi-line: the whole-text query answers NULL (with no cursor draw object: the original then leaves the cursor rectangle's
        # bottom in r29, which it never set, i.e. the caller's register; with one the two sides cannot agree)
        for position in (-4, 2, 4, 6):
            for selection in (-1, 0, 3):
                yield with_query(case(op, [position, 1, selection], attr=0x81, columns=3, cursor=0), null=2)
        # no cursor draw object at all
        for attr in (1, 0x81):
            for position in (-4, 0, 4, 6):
                yield case(op, [position, 1, -1], cursor=0, attr=attr, columns=3)
    elif op == 5:
        # the static-size text grows with no buffer to free first
        for notify in (0, 1):
            for cap in (0, 3, 6):
                c = no_buffer(case(op, [1, notify]), attr=0x41)
                c["setup"].append((1, [0x41, 1, -1, cap, 1, 1, 1, 1, 1, 77, 2, 5]))
                yield c
    elif op == 7:
        # replacing no cursor draw object
        for slot in (-1, 0, 1, 2):
            yield case(op, [slot], cursor=0)
            yield case(op, [slot], cursor=0, attr=0)


def all_edges(op):
    yield from edges(op)
    yield from extra(op)


TESTS = [FnTest(NAMES[op], ppc, host, edge=lambda op=op: all_edges(op),
                rnd=lambda r, op=op: random_case(r, op)) for op in FN]

# Long scripts choose operations after seeing the previous original's state. Editing
# a stale anchor after the Mac's backwards-delete quirk would access invalid memory.
def safe_operation(e, op, args):
    w = e.eb.box
    if op in (1, 2) or op == 0 and args[0] in (0x1000a, 0x1000c):
        p = e.u32(w + 0x138)
        if p:
            n = len(wide(e, p))
            return 0 <= s16(e, w + 0x14a) <= n and s16(e, w + 0x14c) in range(-1, n + 1)
    return True


basic_ppc = ppc
basic_host = host


def script_ppc(e, c):
    f = Ppc(e)
    saved = e.eb_stubs.activate(e)
    try:
        for op, args in c['setup']:
            f.setup(op, args)
        f.L.queues = {k: list(v) for k, v in c['q'].items()}
        states = []
        allowed = []
        for op, args in c['ops']:
            ok = safe_operation(e, op, args)
            allowed.append(ok)
            answer = f.op(op, args) if ok else 0
            states.append((answer, dump_bytes(f.dump()), f.heap[:], len(f.L.log)))
        c['allowed'] = allowed
        return {'states': states, 'log': f.L.log}
    finally:
        f.restore()
        e.eb_stubs.deactivate(e, saved)


def script_host(lib, c):
    lib.tpw_eb_reset()
    lib.tpw_eb_setup.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_eb_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_eb_dump.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_eb_dump.restype = C.c_int32
    for op, args in c['setup']:
        lib.tpw_eb_setup(op, (C.c_int64 * max(1, len(args)))(*args))
    CL.host_begin(lib, {}, c['q'], first_new=100)
    lib.tpw_eb_labels()
    heap = (C.c_int32 * 4).in_dll(lib, 'tpw_cl_heap')
    for i in range(4):
        heap[i] = 0
    states = []
    for ok, (op, args) in zip(c['allowed'], c['ops']):
        out = (C.c_int64 * 1)()
        if ok:
            lib.tpw_eb_op(op, (C.c_int64 * max(1, len(args)))(*args), out)
        dump = (C.c_int64 * 8192)()
        n = lib.tpw_eb_dump(dump)
        states.append((out[0], dump_bytes(dump[:n]), list(heap), C.c_int32.in_dll(lib, 'tpw_cl_n').value))
    return {'states': states, 'log': CL.host_end(lib)}


def script_edges():
    for rtl in (0, 1):
        for attr in (1, 0x81):
            c = fixture(rtl=rtl, attr=attr, columns=3)
            cycle = [(0, [0x10005, 0, (30 << 16) | 30]),
                     (0, [0x10003, (40 << 16) | 32, 0]),
                     (0, [0x10004, 0, (40 << 16) | 32]),
                     (0, [0x1a, 0, 0]), (0, [0x1f, 0, 0]),
                     (2, [0x2400, 0]), (1, [65]), (2, [0x2500, 0x30000]),
                     (1, [98]), (2, [8, 0]), (0, [0x10, 0x75e88, 0]),
                     (0, [0x20, 0, 0]), (0, [3, 0, 0]), (8, []),
                     (2, [0x2e00, 0]), (5, [1, 1]), (10, []), (11, [0]),
                     (12, [3]), (17, [])]
            c['ops'] = cycle * 10
            yield c


def script_random(r):
    c = fixture(rtl=r.randrange(2), attr=r.choice([1, 0x81]), columns=r.randint(2, 8))
    for _ in range(200):
        op = r.choice([0, 1, 2, 3, 5, 6, 8, 10, 11, 12, 15, 16, 17, 18])
        if op == 0:
            m = r.choice(MSGS)
            a, b = r.randrange(3), (r.randrange(100) << 16) | r.randrange(100)
            if m in (0x1000a, 0x1000b, 0x1000c):
                a, b = r.choice(KEYS), r.choice([0, 0x30000])
            args = [m, a, b]
        elif op == 1:
            args = [r.choice([65, 98, 0x5d0, 0, 10])]
        elif op == 2:
            args = [r.choice(KEYS), r.choice([0, 0x30000])]
        elif op == 3:
            args = [r.randint(-7, 20), r.randrange(2), r.randint(-5, 20)]
        elif op in (5, 6):
            args = [r.choice([-1, 1, 4])]
            if op == 5:
                args.append(r.randrange(2))
        elif op in (11, 12):
            args = [r.randrange(2) if op == 11 else r.choice([-1, 3])]
        elif op == 18:
            args = [0x10006, r.randrange(3), (r.randrange(100) << 16) | r.randrange(100)]
        else:
            args = []
        c['ops'].append((op, args))
    return c


TESTS.append(FnTest('EditBox.scripts', script_ppc, script_host, edge=script_edges, rnd=script_random))
