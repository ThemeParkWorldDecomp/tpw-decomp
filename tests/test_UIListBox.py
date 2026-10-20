"""Unit w37b: all 16 functions, real PowerPC code against the native translations.

Fixtures use the original vtables and 32-bit layout on PPC. Every external UI and
virtual callee is a recording fake on both sides, with queued return values. CoolRect
and the unit's own callees stay real. Scripts compare state and heap counters after
EVERY operation, then the complete call log. The libc formatting fake covers the two
literal formats used here, with ASCII UTF-16 text; locale-dependent text is doubtful.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIListBox"
FN = {
    20: 0x101D6E9C, 21: 0x101DB2F8, 22: 0x101DB954, 23: 0x101D0B98,
    24: 0x101D154C, 25: 0x101D0EB0, 26: 0x101D0F50, 27: 0x101D10C8,
    28: 0x101D0DC4, 29: 0x1007DA14, 30: 0x1018A3DC, 31: 0x1018A3A0,
    32: 0x1018A934, 33: 0x101D1768, 34: 0x101D1210, 35: 0x101D7348,
}
NAMES = {
    20: "CalcThumbtrack", 21: "UpdatePreviousPosition", 22: "GetPreviousPosition",
    23: "ListBox.CalcClientArea", 24: "ListControl.CalcClientArea", 25: "GetIndexedEntry",
    26: "recurseDelete", 27: "ListBox.RemoveAllItems", 28: "ListBox.destructor",
    29: "CFont.Set", 30: "ListBox.ClientDraw", 31: "ListBox.DrawFunction",
    32: "ListControl.ClientDraw", 33: "ListControl.destructor", 34: "ListBox.Callback",
    35: "Thumb.Callback",
}
VT = [0x102A7D94, 0x102A7D10, 0x102AFBFC, 0x102A7D5C, 0x102AF974] + [0x102A7D94] * 3
SIZE = [0x138, 0x158, 0x178, 0x15C, 0x15C] + [0x138] * 3
CORE = 0x102AF8FC
G = {"redraw": 0x10395C28, "width": 0x10395BBC, "height": 0x10395BBA,
     "buttons": 0x10395BD0, "help": 0x102AF928, "focus": 0x10395C20,
     "capture": 0x10395C14, "fonts": 0x102DC1A8}


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def w16(e, a, v):
    e.write(a, struct.pack(">h", T.s16(v)))


def s32(e, a):
    return T.s32(e.u32(a))


def s16(e, a):
    return struct.unpack(">h", e.read(a, 2))[0]


def rect_values(e, a):
    return list(struct.unpack(">4h", e.read(a + 4, 8)))


class Ppc:
    """One independent world, with labels matching the native fixture."""

    def __init__(self, e):
        self.e = e
        e.lb = self
        self.windows = [e.alloc(n, bytes(n)) for n in SIZE]
        self.entries = []
        self.heap = [0, 0, 0, 0]
        self.L = e.cl = CL.PpcLog({w: 10 + i for i, w in enumerate(self.windows)})
        for i, w in enumerate(self.windows):
            w32(e, w + 0x118, VT[i])
            offsets = [4, 0x10, 0x1C, 0x28, 0x34]
            if i in (1, 2):
                offsets.append(0x138)
            for offset in offsets:
                w32(e, w + offset, CORE)
        self.output = e.alloc(12, struct.pack(">I4h", CORE, 0, 0, 0, 0))
        self.help = e.alloc(0x18, bytes(0x18))
        w32(e, self.help + 0x10, 0x102AFA88)
        self.draw = e.alloc(0x3C, bytes(0x3C))
        w32(e, self.draw + 0x38, 0x102AFA38)
        self.fonts = e.alloc(16 * 0x18, bytes(16 * 0x18))
        self.font_tokens = [e.alloc(4, bytes(4)) for _ in range(16)]
        self.imports = [e.data_import(n) for n in ("_lbGlobalPalette", "_lbOneColourSpriteRender", "_lbSpriteRender")]
        self.L.labels.update({self.help: 31, self.draw: 60})
        self.L.labels.update({p: 30 + i * 2 if i else 30 for i, p in enumerate(self.imports)})
        self.L.labels[self.imports[1]] = 32
        self.L.labels[self.imports[2]] = 33
        self.L.labels.update({p: 40 + i for i, p in enumerate(self.font_tokens)})
        addresses = [(v, 36 if k == "buttons" else 2 if k in ("width", "height") else 4) for k, v in G.items()]
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in addresses}
        for a, n in addresses:
            e.write(a, bytes(n))
        w32(e, G["help"], self.help)

    def restore(self):
        for (a, n), b in self.saved.items():
            self.e.write(a, b)

    def call(self, fn, *args):
        return T.s32(T.call_ppc(self.e, fn, [a & 0xFFFFFFFF for a in args])[0])

    def setup(self, op, a):
        e = self.e
        box, control = self.windows[1:3]
        if op == 0:
            w = self.windows[a[0]]
            for off, value in zip((0x44, 0x88, 0x5C, 0x58, 0x50, 0x11C),
                                  (a[1], a[2], 0 if a[3] < 0 else self.windows[a[3]],
                                   0 if a[4] < 0 else self.windows[a[4]],
                                   0 if a[5] < 0 else self.windows[a[5]], a[6])):
                w32(e, w + off, value)
        elif op == 1:
            off = (4, 0x1C, 0x28, 0x138, 0x10)[a[1]]
            e.write(self.windows[a[0]] + off + 4, struct.pack(">4h", *[T.s16(v) for v in a[2:6]]))
        elif op == 2:
            for off, value in zip((0x148, 0x14C, 0x150), a[:3]):
                w32(e, box + off, value)
            w16(e, box + 0x154, a[3])
            w16(e, box + 0x156, a[4])
        elif op == 3:
            self.entries = [e.alloc(0x50, bytes(0x50)) for _ in range(a[0])]
            for i, p in enumerate(self.entries):
                e.write(p, (chr(65 + i) + str(i % 10) + "\0").encode("utf-16-be"))
                w32(e, p + 0x40, 1000 + i)
                self.L.labels[p] = 100 + i
            w32(e, box + 0x144, self.entries[0] if self.entries else 0)
        elif op == 4:
            for off, i in zip((0x44, 0x48, 0x4C), a[1:4]):
                w32(e, self.entries[a[0]] + off, 0 if i < 0 else self.entries[i])
        elif op == 5:
            w32(e, G["redraw"], a[0])
            w16(e, G["width"], a[1])
            w16(e, G["height"], a[2])
            w32(e, G["fonts"], self.fonts if a[3] else 0)
            for i, p in enumerate(self.font_tokens):
                w32(e, self.fonts + i * 0x18 + 0xC, p if a[4] & (1 << i) else 0)
            w32(e, G["focus"], 0 if a[5] < 0 else self.windows[a[5]])
            w32(e, G["capture"], 0 if a[6] < 0 else self.windows[a[6]])
        elif op == 6:
            e.write(G["buttons"] + a[0] * 12, struct.pack(">4hi", *[T.s16(v) for v in a[1:5]], T.s32(a[5])))
        elif op == 7:
            w32(e, self.windows[3] + 0x140, a[0])
            w16(e, self.windows[3] + 0x14C, a[1])
        elif op == 8:
            w16(e, control + 0x172, a[0])
            if a[1]:
                p = e.alloc(12, bytes(12))
                for i, slot in enumerate(a[2:5]):
                    w32(e, p + i * 4, 0 if slot < 0 else self.windows[slot])
                w32(e, control + 0x148, p)
                self.L.labels[p] = 61
            if a[5]:
                p = e.alloc(16, bytes(16))
                w32(e, control + 0x144, p)
                self.L.labels[p] = 62
            if a[6] >= 0:
                p = e.alloc(max(12 * a[6], 4), bytes(max(12 * a[6], 4)))
                w32(e, control + 0x16C, p)
                self.L.labels[p] = 63
            w32(e, control + 0x174, self.draw if a[7] else 0)
            w16(e, self.draw + 0xE, a[8])
            w16(e, self.draw + 0xC, a[9])

    def op(self, op, a):
        e = self.e
        box, control = self.windows[1:3]
        result = 0
        if op == 20:
            e.write(self.output + 4, struct.pack(">4h", *[T.s16(v) for v in a[:4]]))
            track = e.alloc(12, struct.pack(">I4h", CORE, *[T.s16(v) for v in a[4:8]]))
            self.call(FN[op], self.output, track, *a[8:11])
        elif op in (21, 22):
            args = (self.windows[a[0]],) if op == 21 else (self.windows[a[0]], 0 if a[1] else self.output)
            result = self.call(FN[op], *args)
        elif op == 25:
            result = self.L.lab(self.call(FN[op], box, a[0]) & 0xFFFFFFFF)
        elif op == 26:
            result = self.call(FN[op], e.u32(box + 0x144))
            self.entries = [0] * len(self.entries)
            w32(e, box + 0x144, 0)
        elif op in (28, 33):
            self.call(FN[op], box if op == 28 else control, a[0])
            self.windows[1 if op == 28 else 2] = 0
            if op == 28:
                self.entries = [0] * len(self.entries)
            else:
                self.windows[5:] = [p if self.L.lab(p) != -1 else 0 for p in self.windows[5:]]
        elif op == 29:
            result = self.call(FN[op], *a[:2])
        elif op in (34, 35):
            result = self.call(FN[op], box if op == 34 else self.windows[4], *a[:3])
        else:
            self.call(FN[op], control if op in (24, 32) else box)
            if op == 27:
                self.entries = [0] * len(self.entries)
        return result

    def dump(self):
        e = self.e
        out = []
        lab = self.L.lab
        for i, w in enumerate(self.windows):
            out.append(int(bool(w)))
            if not w:
                continue
            out.append(e.read(w, 1)[0])
            for off in (4, 0x10, 0x1C, 0x28, 0x34):
                out.extend(rect_values(e, w + off))
            out.extend([lab(e.u32(w + 0x40)), s32(e, w + 0x44), lab(e.u32(w + 0x48)), s16(e, w + 0x4C)])
            out.extend(lab(e.u32(w + off)) for off in (0x50, 0x54, 0x58, 0x5C))
            out.extend([e.u32(w + 0x60), e.u32(w + 0x64)])
            out.extend(e.read(w + 0x68, 32))
            out.extend(s32(e, w + off) for off in (0x88, 0x8C, 0x90))
            out.extend(lab(e.u32(w + off)) for off in (0x94, 0x98))
            out.extend(lab(e.u32(w + 0x9C + j * 4)) for j in range(16))
            out.extend(s16(e, w + off) for off in (0xDC, 0xDE, 0xE0, 0xE2))
            out.extend([lab(e.u32(w + 0xE4)), s16(e, w + 0xE8), s32(e, w + 0xEC), s32(e, w + 0xF0)])
            out.extend(s16(e, w + 0xF4 + j * 2) for j in range(4))
            out.append(lab(e.u32(w + 0xFC)))
            out.extend(s16(e, w + 0x100 + j * 2) for j in range(4))
            out.append(s16(e, w + 0x108))
            out.extend(e.read(w + 0x10A, 6))
            out.extend([int(bool(e.u32(w + 0x110))), int(bool(e.u32(w + 0x114))), e.u32(w + 0x11C)])
            out.extend(lab(e.u32(w + 0x120 + j * 4)) for j in range(4))
            out.extend([e.u32(w + 0x130), *e.read(w + 0x134, 2), T.s8(e.read(w + 0x136, 1)[0])])
            if i == 1:
                out.extend(rect_values(e, w + 0x138))
                out.append(lab(e.u32(w + 0x144)))
                out.extend(s32(e, w + off) for off in (0x148, 0x14C, 0x150))
                out.extend(s16(e, w + off) for off in (0x154, 0x156))
            elif i == 2:
                out.extend(rect_values(e, w + 0x138))
                out.extend(lab(e.u32(w + off)) for off in (0x144, 0x148))
                out.extend(s32(e, w + off) for off in (0x14C, 0x150, 0x154, 0x158))
                out.extend([e.u32(w + 0x15C), s32(e, w + 0x160)])
                out.extend(s16(e, w + off) for off in (0x164, 0x166, 0x168, 0x16A))
                out.extend([lab(e.u32(w + 0x16C)), s16(e, w + 0x170), s16(e, w + 0x172), lab(e.u32(w + 0x174))])
            elif i == 3:
                out.extend(s32(e, w + off) for off in (0x138, 0x13C, 0x140, 0x144, 0x148))
                out.append(s16(e, w + 0x14C))
        out.extend(rect_values(e, self.output))
        out.extend([s32(e, G["redraw"]), s16(e, G["width"]), s16(e, G["height"]),
                    lab(e.u32(G["focus"])), lab(e.u32(G["capture"]))])
        for i in range(3):
            out.extend(struct.unpack(">4hi", e.read(G["buttons"] + i * 12, 12)))
        for p in self.entries:
            out.append(int(bool(p)))
            if p:
                out.extend(struct.unpack(">32H", e.read(p, 64)))
                out.append(s32(e, p + 0x40))
                out.extend(lab(e.u32(p + off)) for off in (0x44, 0x48, 0x4C))
        return out


def setup(runner):
    """Install scoped recording fakes and mark all owned functions real."""
    e = runner.emu
    S = CL.Stubs()
    S.real = set(FN.values()) | {0x101C7448, 0x101C74E0, 0x101C75A8, 0x101C7678, 0x101C7710,
                               0x101D5A3C, 0x101D5B00}
    specs = {
        0x101C9400: ("Post", "Liii"), 0x101C9518: ("Send", "Liii"),
        0x101DE3F8: ("Default", "Liii"), 0x101DC320: ("Enable", "Lh"),
        0x101DFE84: ("SetValue", "Li"), 0x101DFF2C: ("GetValue", "L"),
        0x101D6B08: ("GetValue", "L"), 0x101D6BD4: ("SetValue", "Li"),
        0x101D658C: ("Enable", "Lh"), 0x101DA564: ("DrawMe", "L"),
        0x101D67DC: ("ReCalcThumbPos", "Lii"), 0x101D66A0: ("ReCalcValue", "L"),
        0x101D6B44: ("SetMinMax", "Lii"), 0x101CB1D0: ("SetState", "Lbb"),
        0x101CB2B8: ("HiLight", "Lbb"), 0x101D3CC8: ("Control.RemoveAll", "L"),
        0x101CFB24: ("Help.Move", "Lhh"), 0x101CFB68: ("Help.In", "LL"),
        0x101CFC04: ("Help.Out", "L"),
    }
    for addr, (name, spec) in specs.items():
        S.internal(addr, CL.recorder(name, spec, ret="i"))

    def get_parent(e):
        e.set_r(3, e.u32(e.r(3) + 0x5C))

    def get_window(e):
        p = e.u32(e.r(3) + 0x58)
        while p and e.u32(p + 0x88) != e.r(4):
            p = e.u32(p + 0x50)
        e.set_r(3, p)

    def set_position(e):
        w, r = e.r(3), e.r(4)
        e.cl.rec("SetPosition", [e.cl.lab(w), *rect_values(e, r)])
        e.write(w + 8, e.read(r + 4, 8))
        e.set_r(3, 1)

    def focus(e):
        e.cl.rec("CheckFocus", [e.cl.lab(e.r(3))])
        w32(e, G["focus"], e.r(3))

    def capture(e):
        e.cl.rec("CaptureDevice", [e.cl.lab(e.r(3))])
        if e.u32(G["capture"]) == 0:
            w32(e, G["capture"], e.r(3))
            e.set_r(3, 1)
        else:
            e.set_r(3, 0)

    def uncapture(e):
        e.cl.rec("UnCaptureDevice", [e.cl.lab(e.r(3))])
        same = e.u32(G["capture"]) == e.r(3)
        if same:
            w32(e, G["capture"], 0)
        e.set_r(3, int(same))

    def window_dtor(e):
        p, flag = e.r(3), T.s16(e.r(4))
        e.cl.rec("Window.dtor", [e.cl.lab(p)])
        if flag > 0 and p:
            delete(e, p, False)
        e.set_r(3, p)

    def delete(e, p, array):
        if p:
            e.lb.heap[3 if array else 2] += 1
            if e.cl.lab(p) > 0:
                e.cl.rec("delete", [e.cl.lab(p)])
                e.cl.forget(p)

    def show(e):
        p = e.r(3)
        e.cl.rec("Draw.Show", [e.cl.lab(p), T.s16(e.r(4)), s16(e, p + 0xE), s16(e, p + 0xC)])
        if e.cl.ret("ShowMutate"):
            w16(e, p + 0xE, e.cl.ret("ShowRefs"))
            w16(e, p + 0xC, e.cl.ret("ShowLibrary"))

    def draw_dtor(e):
        p = e.r(3)
        e.cl.rec("Draw.dtor", [e.cl.lab(p)])
        if T.s16(e.r(4)) > 0:
            delete(e, p, False)

    S.internal(0x101DBAFC, get_parent)
    S.internal(0x101DBE40, get_window)
    S.internal(0x101DAE98, set_position)
    S.internal(0x101DC6AC, focus)
    S.internal(0x101DCC10, capture)
    S.internal(0x101DCC6C, uncapture)
    S.internal(0x101D9BA4, window_dtor)
    S.internal(0x101CCD9C, draw_dtor)
    S.internal(0x101CCFE8, show)

    # Heap imports preserve call order. A native CoolRect array has an ABI cookie;
    # its unlabelled allocation pointer produces no delete event, but is counted.
    S.imp("__dl__FPv", lambda e: delete(e, e.r(3), False))
    S.imp("__dla__FPv", lambda e: delete(e, e.r(3), True))
    S.imp("__destroy_new_array", lambda e: e.lb.heap.__setitem__(3, e.lb.heap[3] + 1))

    def palette(e):
        e.cl.rec("Palette", [e.cl.lab(e.r(3)), e.r(4) & 255, e.r(5) & 255, e.r(6) & 255])
        e.set_r(3, e.cl.ret("Palette") & 255)

    def font(e):
        e.cl.rec("Font", [e.cl.lab(e.r(4)), e.cl.lab(e.r(5))])
        w32(e, e.r(3), e.cl.ret("Font"))

    def clip(e):
        p = e.r(3)
        e.cl.rec("Clip", [1, *struct.unpack(">4i", e.read(p, 16))] if p else [0])

    def colour(e, reg):
        # TbColour is five bytes passed in two left-justified GPRs.
        return list(struct.pack(">I", e.r(reg))) + [(e.r(reg + 1) >> 24) & 255]

    def fill(e):
        e.cl.rec("Fill", [*struct.unpack(">4i", e.read(e.r(3), 16)), *colour(e, 4)])

    def text(e):
        e.cl.rec("Text", [T.s32(e.r(3)), T.s32(e.r(4)), *colour(e, 6)], [e.cstr(e.r(5))])

    def sprintf(e):
        fmt = e.cstr(e.r(4))
        value = "" if not fmt else e.cstr(e.r(5)) + CL.wide_text(e, e.r(6))
        e.write(e.r(3), value.encode("latin-1") + b"\0")
        e.set_r(3, len(value))

    def strcat(e):
        p = e.r(3)
        e.write(p, (e.cstr(p) + e.cstr(e.r(4))).encode("latin-1") + b"\0")
        e.set_r(3, p)

    S.imp("LbPalette_FindColour__FPC9TbPaletteUcUcUc", palette)
    S.imp("LbDraw_SetTextFont__FPC12TbTextRenderPv", font)
    S.imp("LbDraw_SetClipRect__FPC6TbRect", clip)
    S.imp("LbDraw_RectangleFilled__FPC6TbRect8TbColour", fill)
    S.imp("LbDraw_Text__FiiPCc8TbColour", text)
    S.imp("sprintf", sprintf)
    S.imp("strcat", strcat)
    e.lb_stubs = S


def ppc(e, case):
    w = Ppc(e)
    saved = e.lb_stubs.activate(e)
    try:
        for op, a in case["setup"]:
            w.setup(op, a)
        w.L.queues = {k: list(v) for k, v in case["q"].items()}
        states = []
        for op, a in case["ops"]:
            result = w.op(op, a)
            states.append((result, w.dump(), w.heap[:], len(w.L.log)))
        return {"states": states, "log": w.L.log}
    finally:
        w.restore()
        e.lb_stubs.deactivate(e, saved)


def host(lib, case):
    lib.tpw_lb_reset()
    lib.tpw_lb_setup.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_lb_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_lb_dump.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lb_dump.restype = C.c_int32
    for op, a in case["setup"]:
        lib.tpw_lb_setup(op, (C.c_int64 * len(a))(*a))
    CL.host_begin(lib, {}, case["q"])
    lib.tpw_lb_labels()
    heap = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
    for i in range(4):
        heap[i] = 0
    states = []
    for op, a in case["ops"]:
        out = (C.c_int64 * 1)()
        lib.tpw_lb_op(op, (C.c_int64 * max(1, len(a)))(*a), out)
        dump = (C.c_int64 * 4096)()
        count = lib.tpw_lb_dump(dump)
        states.append((out[0], list(dump[:count]), list(heap), C.c_int32.in_dll(lib, "tpw_cl_n").value))
    return {"states": states, "log": CL.host_end(lib)}


def tree(parents):
    """Ordered, acyclic entry tree with each node owned exactly once."""
    children = {}
    for i, p in enumerate(parents):
        children.setdefault(p, []).append(i)
    links = []
    for i, p in enumerate(parents):
        siblings = children[p]
        j = siblings.index(i)
        links.append((4, [i, siblings[j + 1] if j + 1 < len(siblings) else -1,
                          children.get(i, [-1])[0], p]))
    return [(3, [len(parents)])] + links


def fixture(parents=(), attributes=1, selected=-1, offset=0, height=12, rows=5, redraw=1):
    setup = [
        (0, [0, 1, 7, -1, 1, -1, 0]),
        (0, [1, attributes, 101, 0, 3, 2, 0]),
        (0, [2, 1, 102, 0, -1, -1, 0]),
        (0, [3, 1, 1, 1, 4, -1, 0]),
        (0, [4, 1, 3, 3, -1, -1, 0]),
        (1, [1, 0, 0, 0, 200, 100]),
        (1, [1, 2, 0, 0, 152, 100]),
        (1, [1, 3, 4, 4, 148, 96]),
        (1, [4, 0, 10, 10, 30, 25]),
        (1, [4, 1, 10, 10, 30, 25]),
        (1, [3, 2, 0, 0, 120, 100]),
        (2, [selected, len(parents), offset, height, rows]),
        (5, [redraw, 640, 480, 1, 0xFFFF, -1, -1]),
        (7, [42, 2]),
    ] + tree(parents)
    q = {"Default": [-7, 9, 0, 123], "Send": [555, -1], "Post": [1, 0],
         "GetValue": [42, 9, -1, 0x7FFFFFFF], "SetValue": [-3], "Enable": [1],
         "Palette": [1, 2, 3, 4, 5], "Font": [-1]}
    return {"setup": setup, "ops": [], "q": q}


def case(op, args=(), **kw):
    c = fixture(**kw)
    c["ops"] = [(op, list(args))]
    return c


TREES = [[], [-1], [-1, -1, -1], [-1, 0, 1, 2, 3, 4, 5], [-1, 0, 0, 1, 1, -1, 5, 6, 5]]
RECTS = [(0, 0, 100, 100), (-32768, -32768, 32767, 32767), (32767, 32767, -32768, -32768),
         (1, 2, 1, 2), (100, 80, -10, -20), (-33, -55, -7, -1)]
MSGS = [-1, 0, 3, 0xC, 0x12, 0x13, 0x14, 0x15, 0x16, 0x800, 0x801,
        0x10000, 0x10001, 0x10002, 0x10003, 0x10004, 0x10005, 0x10006, 0x1000C,
        0x1000D, 0x1000E, 0x11005, 0x11006, 0x11007, 0x11008, 0x7FFFFFFF]


def edges(op):
    if op == 20:
        for rect in RECTS:
            for track in RECTS:
                for horizontal in (0, 1, 2, -1):
                    for delta in (-1000, -1, 0, 1, 1000, 65535):
                        yield case(op, [*rect, *track, delta, -delta, horizontal])
    elif op in (21, 22, 23, 24, 31):
        for rect in RECTS:
            c = case(op, [4] if op == 21 else [4, 0] if op == 22 else [])
            for slot in (1, 2, 4):
                c["setup"] += [(1, [slot, 0, *rect]), (1, [slot, 1, *rect])]
            yield c
        if op == 22:
            yield case(op, [4, 1])
    elif op == 25:
        for parents in TREES:
            for index in (-2147483648, -2, -1, 0, 1, len(parents) - 1, len(parents), len(parents) + 1, 2147483647):
                yield case(op, [index], parents=parents)
    elif op in (26, 27, 28, 30):
        for parents in TREES:
            for redraw in (0, 1, -1):
                for flag in (-1, 0, 1, 32767, 32768, 65535):
                    c = case(op, [flag] if op == 28 else [], parents=parents, redraw=redraw,
                             selected=len(parents) - 1, rows=12)
                    yield c
        if op == 27:
            # No scroll bar child (GetWindow answers NULL): the scroll bar calls are skipped, everything else happens
            for redraw in (0, 1):
                c = case(op, parents=TREES[3], redraw=redraw, selected=3, offset=2, rows=12)
                c["setup"].append((0, [1, 1, 101, 0, -1, -1, 0]))
                yield c
        if op == 30:
            for parents in TREES[1:]:
                for off in range(-1, len(parents) + 1):
                    c = case(op, parents=parents, offset=off, selected=off, rows=10)
                    yield c
            for enabled, mask in ((0, 0xFFFF), (1, 0), (1, 1)):
                c = case(op, parents=TREES[4])
                c["setup"].append((5, [1, 640, 480, enabled, mask, -1, -1]))
                yield c
    elif op == 29:
        for font in range(16):
            for render in (-2147483648, -1, 0, 1, 2, 2147483647):
                for enabled, mask in ((0, 0xFFFF), (1, 0), (1, 0xFFFF), (1, 1 << font)):
                    c = case(op, [font, render])
                    c["setup"].append((5, [1, 640, 480, enabled, mask, -1, -1]))
                    yield c
    elif op in (32, 33):
        for count in (-1, 0, 1, 2, 3):
            for array in (0, 1):
                for refs, library in ((0, 0), (1, 0), (2, 0), (1, 1), (32767, -32768)):
                    c = case(op, [1] if op == 33 else [])
                    c["setup"].append((8, [count, array, 5, 6, 7, 1, 2, 1, refs, library]))
                    yield c
        if op == 33:
            for flag in (-32768, -1, 0, 1, 32767, 65535):
                c = case(op, [flag])
                c["setup"].append((8, [3, 1, 5, -1, 7, 1, 0, 1, 1, 0]))
                yield c
            for newrefs in (-2, -1, 0, 1, 32767):
                c = case(op, [1])
                c["setup"].append((8, [0, 0, -1, -1, -1, 0, -1, 1, 99, 0]))
                c["q"].update(ShowMutate=[1], ShowRefs=[newrefs], ShowLibrary=[1])
                yield c
    elif op == 34:
        for message in MSGS:
            for attr in (0, 1, 2, 0x80, 0x82):
                for y in (-20, 0, 3, 4, 15, 16, 63, 64, 99, 100):
                    yield case(op, [message, (10 << 16) | (y & 0xFFFF), y],
                               parents=TREES[4], attributes=attr, selected=1)
        for height in (-32768, -1, 0, 1, 32767):
            for rows in (-1, 0, 1, 5):
                yield case(op, [0x11006, 0, -5], parents=TREES[4], offset=4, height=height, rows=rows)
        for x in (-1, 0, 1, 151, 152, 153):
            for y in (-1, 0, 1, 99, 100, 101):
                yield case(op, [0x10003, ((x & 0xFFFF) << 16) | (y & 0xFFFF), 0],
                           parents=TREES[4], attributes=0x80)
        for count in (0, 1, 9):
            for rows in (0, 1, 9):
                for y in (3, 4, 15, 16, 111, 112):
                    c = case(op, [0x11006, 0, y], parents=TREES[4], rows=rows)
                    c["setup"].append((2, [-1, count, 0, 12, rows]))
                    yield c
        # A click (0x10003) with fewer items than visible rows (row >= count gives -1), and with the global redraw flag off
        for count in (0, 1, 2, 9):
            for y in (3, 4, 15, 16, 27, 28, 39, 40, 111):
                for redraw in (0, 1):
                    c = case(op, [0x10003, (10 << 16) | y, 0], parents=TREES[4], attributes=0x80, redraw=redraw, selected=1)
                    c["setup"].append((2, [-1, count, 0, 12, 5]))
                    yield c
        # Child lookup absent, a nonmatching id, and a later matching sibling.
        for first, childid in ((-1, 1), (3, 2)):
            c = case(op, [0x800, 0, 0])
            c["setup"].extend([(0, [1, 1, 101, 0, first, -1, 0]),
                               (0, [3, 1, childid, 1, -1, -1, 0])])
            yield c
        c = case(op, [0x800, 0, 0])
        c["setup"].extend([(0, [3, 1, 2, 1, -1, 5, 0]), (0, [5, 1, 1, 1, -1, -1, 0])])
        yield c
    elif op == 35:
        for message in MSGS:
            for held in (0, 1, 2, 7):
                for button in (0, 1, 2):
                    c = case(op, [message, button, (20 << 16) | 30])
                    c["setup"].append((0, [4, 1, 3, 3, -1, -1, held]))
                    yield c
        for horizontal in (0, 1, 2, 3):
            for point in (0, 10, 25, 100, 200, -200, 32767, -32768):
                c = case(op)
                c["setup"].append((7, [42, horizontal]))
                c["ops"] = [(35, [0x10005, 0, 0]), (35, [0x10003, ((point & 0xFFFF) << 16) | (point & 0xFFFF), 0]),
                            (35, [0x10004, 0, 0])]
                yield c
        for state in (-1, 1, 2):
            c = case(op, [0x10005, 0, 0])
            c["setup"].append((6, [0, state, 2, 3, 444, 555]))
            yield c
        # Zero travel and an oversized thumb clamp in the leaf; ReCalcValue is a fake here.
        for horizontal in (0, 2):
            c = case(op)
            c["setup"].extend([(7, [42, horizontal]), (1, [3, 2, 10, 10, 30, 25])])
            c["ops"] = [(35, [0x10005, 0, 0]), (35, [0x10003, 0x7FFF7FFF, 0]), (35, [0x10004, 0, 0])]
            yield c


def random_case(r, op):
    parents = []
    for i in range(r.randrange(0, 18)):
        # Depth limited to six so the original's 32-byte indentation buffer stays valid.
        choices = [-1] + [j for j in range(i) if depth(parents, j) < 6]
        parents.append(r.choice(choices))
    c = fixture(parents=parents, attributes=r.choice([0, 1, 2, 0x80, 0x82, r.getrandbits(32)]),
                selected=r.randint(-3, len(parents) + 3), offset=r.randint(-2, len(parents) + 2),
                height=r.choice([-20, -1, 0, 1, 12, 20, 32767]), rows=r.randint(-2, 22),
                redraw=r.choice([0, 1, -1]))
    if op == 20:
        c["ops"] = [(op, [*[r.randint(-32768, 32767) for _ in range(8)], T.rint(r), T.rint(r), r.choice([0, 1, 2, -1])])]
    elif op in (21, 22, 23, 24, 31):
        c["setup"].extend((1, [slot, which, *[r.randint(-32768, 32767) for _ in range(4)]])
                          for slot in (1, 2, 4) for which in (0, 1))
        c["ops"] = [(op, [4] if op == 21 else [4, r.randrange(2)] if op == 22 else [])]
    elif op == 25:
        c["ops"] = [(op, [r.randint(-3, len(parents) + 3)])]
    elif op in (26, 27, 28, 30):
        c["ops"] = [(op, [r.choice([-1, 0, 1, 65535])] if op == 28 else [])]
        if op == 30:
            c["setup"].append((5, [r.choice([0, 1]), r.randint(-32768, 32767), r.randint(-32768, 32767),
                                    r.randrange(2), r.getrandbits(16), -1, -1]))
            c["setup"].append((1, [1, 3, *r.choice(RECTS)]))
    elif op == 29:
        c["setup"].append((5, [1, 640, 480, r.randrange(2), r.getrandbits(16), -1, -1]))
        c["ops"] = [(op, [r.randrange(16), r.choice([-1, 0, 1, 2, T.rint(r)])])]
    elif op in (32, 33):
        count = r.randint(-2, 3)
        slots = [5, 6, 7] if op == 32 else [r.choice([5, -1]), r.choice([6, -1]), r.choice([7, -1])]
        c["setup"].append((8, [count, r.randrange(2), *slots, r.randrange(2), r.choice([-1, 0, 1, 3]),
                                r.randrange(2), r.randint(-2, 3), r.choice([0, 1, -1])]))
        c["ops"] = [(op, [r.choice([-1, 0, 1, 65535])] if op == 33 else [])]
    else:
        c["ops"] = []
        for _ in range(200 if r.random() < 0.025 else r.randint(1, 12)):
            message = r.choice(MSGS)
            a, b = T.rint(r), T.rint(r)
            if op == 35 and message in (0x10004, 0x10005):
                a = r.randrange(3)
            c["ops"].append((op, [message, a, b]))
        c["setup"].append((7, [T.rint(r), r.choice([0, 1, 2, 3])]))
        c["setup"].append((5, [r.choice([0, 1]), 640, 480, 1, 65535, r.choice([-1, 0, 4]), r.choice([-1, 0, 4])]))
        c["setup"].extend((6, [i, r.choice([0, 1, 2]), r.randint(-100, 100), r.randint(-100, 100), 99, T.rint(r)])
                          for i in range(3))
    return c


def depth(parents, j):
    d = 0
    while j >= 0:
        d += 1
        j = parents[j]
    return d


TESTS = [FnTest(NAMES[op], ppc, host, edge=lambda op=op: edges(op),
                rnd=lambda r, op=op: random_case(r, op)) for op in sorted(FN)]
