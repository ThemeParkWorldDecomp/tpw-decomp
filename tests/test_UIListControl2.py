"""Unit w93: InterfaceListControl2's item management (15 functions) and InterfaceWindow::SetTextOffset and GetDims.

The real PowerPC code against the native translations. The world is 15 windows (see tests/unit_UIListControl2.cpp): the parent,
the list control, its scroll bar, four row windows and two cell windows under each row, with four draw objects. Every callee
outside the unit (the window's Show, SetPosition, setZ, SetValue, SetText, GetVisibility, the scroll bar, the draw object's
Change and Show, the message pump, operator new, the string library, the row factory and the attach callback) is a recording
fake on both sides, with queued answers; CoolRect and the unit's own functions are real. Each operation of a case compares the
answers, the control's state, the whole item block, the heap counters and the number of events; the complete call log is
compared at the end.

Sizes: a word of the item block, a pointer in an array and a string object are 4 bytes on the Mac and 8 here. The PowerPC fakes
for operator new record the bytes they are asked for and the host records half of them.

The PowerPC side's text and cell forms: a string cell holds a pointer to a 4-byte string object whose word is a pointer to the
representation (length, capacity, count, characters pointer); the fake string constructor builds that.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIListControl2"
FN = {
    20: 0x101D1EB4, 21: 0x101D207C, 22: 0x101D2220, 23: 0x101D2614, 24: 0x101D268C, 25: 0x101D26E0,
    26: 0x101D2864, 27: 0x101D2A84, 28: 0x101D2B64, 29: 0x101D2D6C, 30: 0x101D2E8C, 31: 0x101D3000,
    32: 0x101D3660, 33: 0x101D41F0, 34: 0x101D4854, 35: 0x101DA484, 36: 0x101DBEB0, 37: 0x101D3660,
}
NAMES = {
    20: "SetSelectionObject", 21: "AllocNewBlock", 22: "UpdateDisplay", 23: "SetColumnType", 24: "GetColumnType",
    25: "SetColumnFormat", 26: "SetColumnFormatsAll", 27: "AdvanceIndex", 28: "GetIndexedEntry", 29: "FindFreeEntry",
    30: "ShouldIInsertEntryHere", 31: "AddItem", 32: "ChangeItem", 33: "GetData", 34: "ChangeCurEntry",
    35: "SetTextOffset", 36: "GetDims", 37: "ChangeItem.wide",
}
NWIN = 15
SIZE = [0x138, 0x178, 0x15C] + [0x138] * 12
VT = [0x102A7D94, 0x102AFBFC, 0x102A7D5C] + [0x102A7D94] * 12
CORE = 0x102AF8FC
DRAW_VT = 0x102AFA38
GLOBAL_REDRAW = 0x10395C28


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def w16(e, a, v):
    e.write(a, struct.pack(">h", T.s16(v)))


def s32(e, a):
    return T.s32(e.u32(a))


def s16(e, a):
    return struct.unpack(">h", e.read(a, 2))[0]


def text_of(v):
    """The text of string cell number v (the host's Text)."""
    return "t%02d" % (v % 100)


class Lc:
    """One independent world on the PowerPC side, with the labels of the native fixture."""

    def __init__(self, e):
        self.e = e
        e.lc = self
        self.windows = [e.alloc(n, bytes(n)) for n in SIZE]
        self.draws = [e.alloc(0x3C, bytes(0x3C)) for _ in range(4)]
        self.heap = [0, 0, 0, 0]
        self.strings = 0
        fixed = {w: 10 + i for i, w in enumerate(self.windows)}
        fixed.update({d: 60 + i for i, d in enumerate(self.draws)})
        self.L = e.cl = CL.PpcLog(fixed)
        for i, w in enumerate(self.windows):
            w32(e, w + 0x118, VT[i])
            for offset in (4, 0x10, 0x1C, 0x28, 0x34) + ((0x138,) if i == 1 else ()):
                w32(e, w + offset, CORE)
        for d in self.draws:
            w32(e, d + 0x38, DRAW_VT)
        self.control = self.windows[1]
        self.saved = bytes(e.read(GLOBAL_REDRAW, 4))
        w32(e, GLOBAL_REDRAW, 0)

    def restore(self):
        self.e.write(GLOBAL_REDRAW, self.saved)

    def call(self, fn, *args):
        return T.s32(T.call_ppc(self.e, fn, [a & 0xFFFFFFFF for a in args])[0])

    def stride(self):
        return s16(self.e, self.control + 0x164) + 4

    def setup(self, op, a):
        e, c = self.e, self.control
        if op == 0:
            w = self.windows[a[0]]
            w32(e, w + 0x44, a[1])
            w32(e, w + 0x88, a[2])
            for off, v in zip((0x5C, 0x58, 0x50), a[3:6]):
                w32(e, w + off, 0 if v < 0 else self.windows[v])
        elif op == 1:
            w = self.windows[a[0]]
            off = 4 if a[1] == 0 else 0x138
            e.write(w + off + 4, struct.pack(">4h", *[T.s16(v) for v in a[2:6]]))
        elif op == 2:
            for off, v in zip((0x14C, 0x150, 0x154, 0x160), a[:4]):
                w32(e, c + off, v)
            for off, v in zip((0x170, 0x172, 0x168, 0x16A, 0xDC, 0xDE), a[4:10]):
                w16(e, c + off, v)
            w32(e, c + 0x44, a[10])
            w32(e, c + 0x88, a[11])
        elif op == 3:
            w16(e, c + 0x164, a[0])
            if a[0] > 0:
                p = e.alloc(12 * a[0], bytes(12 * a[0]))
                for i in range(a[0]):
                    w32(e, p + 12 * i, CORE)
                    e.write(p + 12 * i + 4, struct.pack(">4h", 32 * i, 0, 32 * i + 32, T.s16(a[1 + i])))
                w32(e, c + 0x16C, p)
                self.L.labels[p] = 71
        elif op == 4:
            p = e.alloc(4 * a[0], bytes(4 * a[0]))
            w32(e, c + 0x148, p)
            self.L.labels[p] = 70
        elif op == 5:
            w32(e, e.u32(c + 0x148) + 4 * a[0], 0 if a[1] < 0 else self.windows[a[1]])
        elif op == 6:
            w32(e, c + 0x174, 0 if a[0] < 0 else self.draws[a[0]])
        elif op == 7:
            w16(e, self.draws[a[0]] + 0xE, a[1])
            w16(e, self.draws[a[0]] + 0xC, a[2])
        elif op == 8:
            w32(e, self.windows[a[0]] + 0x94, 0 if a[1] < 0 else self.draws[a[1]])
            w32(e, self.windows[a[0]] + 0x98, 0 if a[2] < 0 else self.draws[a[2]])
        elif op == 9:
            e.write(self.windows[a[0]] + 0xF4, struct.pack(">4h", *[T.s16(v) for v in a[1:5]]))
        elif op == 10:
            words = a[0] * self.stride()
            w32(e, c + 0x144, e.alloc(4 * words, bytes(4 * words)) if a[0] > 0 else 0)
            w32(e, c + 0x158, words)
        elif op == 11:
            stride = self.stride()
            base = e.u32(c + 0x144) + 4 * a[0] * stride
            for i, v in enumerate((a[1], a[2], a[3], a[4])):
                if i == 1:
                    w32(e, base + 4, v)
                else:
                    w32(e, base + 4 * i, v if v < 0 else v * stride)
        elif op == 12:
            cell = e.u32(c + 0x144) + 4 * (a[0] * self.stride() + 4 + a[1])
            if a[2] == 1:
                text = text_of(a[3])
                obj = e.alloc(4, bytes(4))
                self.make_string(obj, text)
                w32(e, cell, obj)
                self.L.labels[obj] = 300 + self.strings
                self.strings += 1
            elif a[2] == 0:
                w32(e, cell, a[3])
            else:
                w32(e, cell, 0)
        elif op == 13:
            w32(e, GLOBAL_REDRAW, a[0])
        elif op == 14:
            p = e.u32(c + 0x16C) + 12 * a[0]
            w16(e, p + 4, a[1])
            w16(e, p + 8, a[2])

    def make_string(self, obj, text):
        """The string object `obj` made from text: a representation {length, capacity, count, characters}."""
        e = self.e
        chars = CL.new_wide(e, text)
        rep = e.alloc(16, struct.pack(">IIII", len(text), len(text), 1, chars))
        w32(e, obj, rep)

    def item(self, spec):
        """An item to add: three words (a number, a pointer to text, or 0)."""
        e = self.e
        words = []
        for kind, v in zip(spec[0::2], spec[1::2]):
            if kind == 0:
                words.append(v & 0xFFFFFFFF)
            elif kind == 1:
                words.append(CL.new_wide(e, text_of(v)))
            else:
                words.append(0)
        return e.alloc(12, struct.pack(">3I", *words))

    def op(self, op, a):
        e, c = self.e, self.control
        out = [0, 0, 0]
        fn = FN[op]
        if op == 20:
            self.call(fn, c, 0 if a[0] < 0 else self.draws[a[0]])
        elif op == 21:
            out[0] = int(self.call(fn, c, a[0]) != 0)
        elif op == 22:
            self.call(fn, c)
        elif op == 23:
            self.call(fn, c, a[0], a[1])
        elif op == 24:
            out[0] = self.call(fn, c, a[0])
        elif op == 25:
            self.call(fn, c, a[0], 0 if a[1] < 0 else self.windows[a[1]])
        elif op == 26:
            self.call(fn, c, self.row_factory if a[0] else 0, self.attach if a[1] else 0)
        elif op == 27:
            p = e.alloc(4, struct.pack(">i", -77))
            out[0] = self.call(fn, c, a[0], p if a[1] else 0)
            out[1] = s32(e, p) if a[1] else -77
        elif op == 28:
            out[0] = self.call(fn, c, a[0])
        elif op == 29:
            p = e.alloc(8, struct.pack(">ii", -77, -77))
            self.call(fn, c, p, p + 4)
            out[1], out[2] = s32(e, p), s32(e, p + 4)
        elif op == 30:
            cells = e.u32(c + 0x144) + 4 * (a[0] * self.stride() + 4)
            out[0] = self.call(fn, c, self.item(a[1:7]), cells)
        elif op == 31:
            item = 0 if a[3] == 2 else self.item(a[4:10])
            out[0] = self.call(fn, c, item, a[0], a[1], a[2])
        elif op == 32:
            item = 0 if a[3] == 2 else self.item(a[4:10])
            self.call(fn, c, a[0], item, a[1], a[2], 0)
        elif op == 37:
            words = []
            for kind, v in zip(a[4::2], a[5::2]):
                words.append(v & 0xFFFFFFFF if kind == 0 else 0)
            item = e.alloc(4 * len(words), struct.pack(">%dI" % len(words), *words))
            self.call(fn, c, a[0], item, a[1], a[2], 0)
        elif op == 33:
            p = e.alloc(4, struct.pack(">i", -77))
            out[0] = self.call(fn, c, a[0], p if a[1] else 0)
            out[1] = s32(e, p)
        elif op == 34:
            self.call(fn, c, a[0])
        elif op == 35:
            self.call(fn, self.windows[a[0]], a[1], a[2])
        elif op == 36:
            p = e.alloc(4, struct.pack(">hh", 0x7ABC, 0x7ABD))
            out[0] = self.call(fn, self.windows[a[0]], 0 if a[2] else p, 0 if a[3] else p + 2, a[1])
            out[1], out[2] = s16(e, p), s16(e, p + 2)
        for i, d in enumerate(self.draws):
            if d and self.L.lab(d) == -1:
                self.draws[i] = 0
        return tuple(out)

    def dump(self):
        e, c, lab = self.e, self.control, self.L.lab
        out = list(struct.unpack(">4h", e.read(c + 0x13C, 8)))
        out.extend(s32(e, c + off) for off in (0x14C, 0x150, 0x154, 0x158, 0x160))
        out.extend(s16(e, c + off) for off in (0x164, 0x168, 0x16A, 0x170, 0x172))
        out.extend([lab(e.u32(c + 0x148)), lab(e.u32(c + 0x16C)), lab(e.u32(c + 0x174)), int(e.u32(c + 0x144) != 0),
                    s32(e, GLOBAL_REDRAW)])
        count, rows = s16(e, c + 0x164), s16(e, c + 0x172)
        columns = e.u32(c + 0x16C)
        if columns:
            for i in range(count):
                out.extend(struct.unpack(">4h", e.read(columns + 12 * i + 4, 8)))
        items = e.u32(c + 0x148)
        if items:
            out.extend(lab(e.u32(items + 4 * i)) for i in range(4) if i < rows)
        block = e.u32(c + 0x144)
        stride = count + 4
        for i in range(s32(e, c + 0x158) if block else 0):
            column = i % stride - 4
            if column >= 0 and columns and s16(e, columns + 12 * column + 10) == 0:
                out.append(lab(e.u32(block + 4 * i)))
            else:
                out.append(s32(e, block + 4 * i))
        for d in self.draws:
            out.append(int(bool(d)))
            if d:
                out.extend([s16(e, d + 0xE), s16(e, d + 0xC)])
        return out


def setup(runner):
    """Install scoped recording fakes and mark the unit's functions real."""
    e = runner.emu
    S = CL.Stubs()
    S.real = set(FN.values()) | {0x101C7448, 0x101C7494, 0x101C74E0, 0x101C7618, 0x101C756C}
    specs = {
        0x101C9400: ("Post", "Liii", "i"), 0x101DC1EC: ("Show", "Lh", None), 0x101DB360: ("SetPosition", "Liiii", None),
        0x101DD8BC: ("setZ", "Lh", None), 0x101DFE84: ("SetValue", "Li", None), 0x101D6BD4: ("SetValue", "Li", None),
        0x101DE248: ("SetText", "LS", None), 0x101D6B44: ("SetMinMax", "Lii", None), 0x101D6C34: ("SetPage", "Li", None),
    }
    for addr, (name, spec, ret) in specs.items():
        S.internal(addr, CL.recorder(name, spec, ret=ret))

    def block_low(e):
        return e.u32(e.lc.control + 0x144)

    def send(e):
        w, m, a, b = e.r(3), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))
        shown = (a - T.s32(block_low(e))) // 4 if m == 0x403 else a
        e.cl.rec("Send", [e.cl.lab(w), m, shown, b])
        e.set_r(3, T.u32(e.cl.ret("Send")))

    def visibility(e):
        e.cl.rec("Visibility", [e.cl.lab(e.r(3))])
        e.set_r(3, T.u32(e.cl.ret("Visibility", 1)))

    def get_child(e):
        e.set_r(3, e.u32(e.r(3) + 0x58))

    def get_next(e):
        e.set_r(3, e.u32(e.r(3) + 0x50))

    def get_window(e):
        p = e.u32(e.r(3) + 0x58)
        while p and e.u32(p + 0x88) != e.r(4):
            p = e.u32(p + 0x50)
        e.set_r(3, p)

    def change(e):
        p, flags, rec = e.r(3), e.r(4), e.r(5)
        base = [e.cl.lab(p), flags]
        if flags == 0x17:
            rect = e.u32(rec + 0x14)
            e.cl.rec("Change", base + list(struct.unpack(">3h", e.read(rec + 4, 6)))
                     + list(struct.unpack(">4h", e.read(rect + 4, 8))))
        elif flags == 0x10:
            rect = e.u32(rec + 0x14)
            e.cl.rec("Change", base + list(struct.unpack(">4h", e.read(rect + 4, 8))))
        elif flags == 0xC000:
            style = e.u32(rec + 0x10)
            e.cl.rec("Change", base + [s16(e, style + 0xA), s16(e, style + 0xC)])
        else:
            e.cl.rec("Change", base)

    def draw_show(e):
        p = e.r(3)
        e.cl.rec("Draw.Show", [e.cl.lab(p), T.s16(e.r(4)), s16(e, p + 0xE), s16(e, p + 0xC)])
        if e.cl.ret("ShowMutate"):
            w16(e, p + 0xE, e.cl.ret("ShowRefs"))
            w16(e, p + 0xC, e.cl.ret("ShowLibrary"))
        if e.cl.ret("ShowClear"):
            w32(e, e.lc.control + 0x174, 0)       # the call took the selection object away from the control

    def delete(e, p, array):
        if p:
            e.lc.heap[3 if array else 2] += 1
            if e.cl.lab(p) > 0:
                e.cl.rec("delete", [e.cl.lab(p)])
                e.cl.forget(p)

    def draw_dtor(e):
        p = e.r(3)
        e.cl.rec("Draw.dtor", [e.cl.lab(p)])
        if T.s16(e.r(4)) > 0:
            delete(e, p, False)

    def string_new(e):
        this, text = e.r(3), CL.wide_text(e, e.r(4))
        e.cl.rec("Str.new", [e.cl.newlab(this)], [text])
        e.lc.make_string(this, text)
        e.set_r(3, this)

    def string_assign(e):
        this, source, length = e.r(3), e.r(4), e.r(5)
        e.cl.rec("Str.assign", [e.cl.lab(this), length], [CL.wide_text(e, source)])
        rep = e.u32(this)
        w32(e, rep, length)
        w32(e, rep + 12, CL.new_wide(e, text_of_wide(e, source, length)))
        e.set_r(3, this)

    def text_of_wide(e, source, length):
        return "".join(chr(struct.unpack(">H", e.read(source + 2 * i, 2))[0]) for i in range(length))

    S.internal(0x101DBAC4, get_child)
    S.internal(0x101DBB38, get_next)
    S.internal(0x101DBD9C, visibility)
    S.internal(0x101DBE40, get_window)
    S.internal(0x101C9518, send)
    S.internal(0x101CCEC8, change)
    S.internal(0x101CCFE8, draw_show)
    S.internal(0x101CCD9C, draw_dtor)
    S.internal(0x10146A14, string_new)
    S.internal(0x101B2510, string_assign)

    def op_new(e):
        size = e.r(3)
        e.lc.heap[0] += 1
        e.cl.rec("new", [size])
        e.set_r(3, e.alloc(max(size, 4), bytes(max(size, 4))))

    def op_new_array(e):
        size = e.r(3)
        e.lc.heap[1] += 1
        e.cl.rec("new[]", [size])
        e.set_r(3, e.alloc(max(size, 4), bytes(max(size, 4))))

    def wide(e, p):
        out = []
        while True:
            ch = struct.unpack(">H", e.read(p + 2 * len(out), 2))[0]
            if ch == 0:
                return out
            out.append(ch)

    def compare(e):
        a, b = wide(e, e.r(3)), wide(e, e.r(4))
        e.set_r(3, 0 if a == b else 1 if a > b else 0xFFFFFFFF)

    def memset(e):
        destination, value, size = e.r(3), e.r(4) & 0xFF, e.r(5)
        if size:
            e.write(destination, bytes([value]) * size)

    def memcpy(e):
        destination, source, size = e.r(3), e.r(4), e.r(5)
        if size:
            e.write(destination, e.read(source, size))

    S.imp("memset", memset)
    S.imp("memcpy", memcpy)
    S.imp("__nw__FUl", op_new)
    S.imp("__nwa__FUl", op_new_array)
    S.imp("__dl__FPv", lambda e: delete(e, e.r(3), False))
    S.imp("__dla__FPv", lambda e: delete(e, e.r(3), True))
    S.imp("wcslen", lambda e: e.set_r(3, len(wide(e, e.r(3)))))
    S.imp("wcscmp", compare)
    e.lc_stubs = S

    # The row factory and the attach callback of SetColumnFormatsAll: function descriptors that record.
    def make_row(e):
        which = e.cl.ret("Row", -1)
        e.cl.rec("MakeRow", [which])
        e.set_r(3, 0 if which < 0 else e.lc.windows[which])

    def attach(e):
        e.cl.rec("Attach", [e.cl.lab(e.r(3))])

    Lc.row_factory = T.fake_function(e, lambda em: make_row(em))
    Lc.attach = T.fake_function(e, lambda em: attach(em))


def ppc(e, case):
    w = Lc(e)
    saved = e.lc_stubs.activate(e)
    try:
        for op, a in case["setup"]:
            w.setup(op, a)
        w.L.queues = {k: list(v) for k, v in case["q"].items()}
        states = []
        for op, a in case["ops"]:
            result = w.op(op, a)
            states.append((result, w.dump(), w.heap[:], len(w.L.log)))
        return {"states": states, "log": w.L.log}
    except RuntimeError as error:
        raise RuntimeError(f"{error}: {case['setup'][15:]} {case['ops']}")
    finally:
        w.restore()
        e.lc_stubs.deactivate(e, saved)


def host(lib, case):
    lib.tpw_lc_reset()
    lib.tpw_lc_setup.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_lc_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_lc_dump.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lc_dump.restype = C.c_int32
    for op, a in case["setup"]:
        lib.tpw_lc_setup(op, (C.c_int64 * len(a))(*a))
    CL.host_begin(lib, {}, case["q"])
    lib.tpw_lc_labels()
    heap = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
    for i in range(4):
        heap[i] = 0
    states = []
    for op, a in case["ops"]:
        out = (C.c_int64 * 3)()
        lib.tpw_lc_op(op, (C.c_int64 * max(1, len(a)))(*a), out)
        dump = (C.c_int64 * 8192)()
        count = lib.tpw_lc_dump(dump)
        states.append((tuple(out), list(dump[:count]), list(heap), C.c_int32.in_dll(lib, "tpw_cl_n").value))
    return {"states": states, "log": CL.host_end(lib)}




# ----------------------------------------------------------------- the cases

INTS = [0, 1, -1, 2, 7, 100, 255, 32767, -32768, 65535, 0x7FFFFFFF, -0x80000000, 12345678, -12345678]


def depth_of(parents, k):
    d = 0
    while parents[k] >= 0:
        k = parents[k]
        d += 1
    return d


def forest(r, types, capacity, n, flat=False, head_free=False, nulls=False, texts=None):
    """The setup ops of an item block with n entries in use among `capacity` (block ops 10, 11 and 12) and the facts a case needs.
    The used entries are placed at random; the top level is chained and then runs into the chain of the free entries; every
    entry's children are chained and end with -1; free entries have parent -2."""
    perm = list(range(capacity))
    r.shuffle(perm)
    used, free = perm[:n], perm[n:]
    parent = [-1 if flat or k == 0 or r.random() < 0.4 else r.randrange(k) for k in range(n)]
    children = {k: [] for k in range(-1, n)}
    for k in range(n):
        children[parent[k]].append(k)
    ops = [(10, [capacity])]
    for k in range(n):
        siblings = children[parent[k]]
        j = siblings.index(k)
        if j + 1 < len(siblings):
            following = used[siblings[j + 1]]
        else:
            following = (free[0] if free else -1) if parent[k] == -1 else -1
        child = used[children[k][0]] if children[k] else -1
        ops.append((11, [used[k], following, r.choice(INTS), child, used[parent[k]] if parent[k] >= 0 else -1]))
        for column, t in enumerate(types):
            if t == 0:
                if nulls and r.random() < 0.15:
                    ops.append((12, [used[k], column, 2, 0]))
                else:
                    ops.append((12, [used[k], column, 1, r.randrange(100) if texts is None else r.choice(texts)]))
            else:
                ops.append((12, [used[k], column, 0, r.choice(INTS)]))
    for i, f in enumerate(free):
        ops.append((11, [f, free[i + 1] if i + 1 < len(free) else -1, 0, -1, -2]))
    top = children[-1]
    first = used[top[0]] if top else (free[0] if (free and head_free) else -1)
    preorder = []

    def walk(k):
        preorder.append(used[k])
        for child in children[k]:
            walk(child)
    for k in top:
        walk(k)
    stride = len(types) + 4
    return ops, {"first": first * stride if first >= 0 else -1, "n": n, "used": used, "free": free, "order": preorder,
                 "parents": parent, "capacity": capacity}


def fixture(types=(0, 1), rows=3, rowh=12, rect=(0, 0, 100, 40), attributes=0x10, current=-1, top=0, sortcol=0, sortdesc=0,
            z=5, zrot=2, cid=7, redraw=1, bar=True, array=True, draw=None, draw_state=(1, 0), vis=(1,), block=None,
            filled=None, columns=True):
    """A world: the control with its columns, rows and block, the bar, the four rows with two cell windows each."""
    ops = [(0, [0, 0, 1, -1, 1, -1]), (0, [1, attributes, cid, 0, 2 if bar else -1, -1]), (0, [2, 0, 1, 1, -1, -1])]
    for k in range(4):
        ops.append((0, [3 + k, 0, 20 + k, -1, 7 + 2 * k, -1]))
        ops.append((0, [7 + 2 * k, 0, 40 + 2 * k, 3 + k, -1, 8 + 2 * k]))
        ops.append((0, [8 + 2 * k, 0, 41 + 2 * k, 3 + k, -1, -1]))
    # the rows have draw objects (SetTextOffset tells them the indentation): the rows' own and client ones differ per row
    ops.extend([(8, [3, 1, 2]), (8, [4, 2, -1]), (8, [5, -1, 3]), (8, [6, 1, -1])])
    ops.append((1, [1, 1, *rect]))
    facts = {"first": -1, "n": 0}
    block_ops = []
    if block is not None:
        block_ops, facts = block
    ops.append((2, [current, facts["n"], top, facts["first"], rowh, rows, sortcol, sortdesc, z, zrot, attributes, cid]))
    if columns:
        ops.append((3, [len(types), *types]))
    ops.extend(block_ops)
    if array:
        ops.append((4, [4]))
        for k in range(min(rows if filled is None else filled, 4)):
            ops.append((5, [k, 3 + k]))
    if draw is not None:
        ops.append((6, [draw]))
        ops.append((7, [draw, *draw_state]))
    ops.append((13, [redraw]))
    q = {"Visibility": list(vis), "Post": [0], "Send": [0], "Row": [3, 4, 5, 6]}
    return {"setup": ops, "ops": [], "q": q}


def case(op, args=(), **kw):
    c = fixture(**{k: v for k, v in kw.items() if k not in ("q",)})
    c["ops"] = [(op, list(args))]
    c["q"].update(kw.get("q", {}))
    return c


def spec(r, types, nulls=False, texts=None):
    """An item (a flat list of (kind, value) for three words): strings for text columns, numbers for the others."""
    out = []
    for column in range(3):
        t = types[column] if column < len(types) else 1
        if t == 0:
            out += [2, 0] if nulls and r.random() < 0.15 else [1, r.randrange(100) if texts is None else r.choice(texts)]
        else:
            out += [0, r.choice(INTS)]
    return out


def rand_types(r, n=None):
    n = r.randint(1, 3) if n is None else n
    return tuple(r.choice([0, 1]) for _ in range(n))


def rand_world(r, op=None, with_block=True, n_max=9, **over):
    """A random world with a random block (hierarchical unless flat)."""
    types = rand_types(r) if "types" not in over else over.pop("types")
    n = r.randint(0, n_max) if with_block else 0
    capacity = n + r.randint(0, 4) if n or not with_block else r.randint(0, 3)
    flat = over.pop("flat", r.random() < 0.3)
    block = forest(r, types, capacity, n, flat=flat, nulls=over.pop("nulls", False), head_free=over.pop("head_free", False)) if with_block and (n or capacity) else None
    kw = dict(types=types, rows=r.randint(0, 4), rowh=r.choice([1, 8, 12, 16, 20, 300]),
              rect=(r.choice([0, -5, 3]), r.choice([0, 4, -7, 100]), r.choice([60, 100, 32767]), r.choice([40, 100, 64, 130])),
              attributes=r.choice([0, 0x10, 0x100, 0x110, 0x1000, r.getrandbits(32)]), current=r.choice([-1, -1, 0, 1, 3, 7]),
              top=r.choice([0, 0, 1, 2, 5]), sortcol=r.randrange(len(types)), sortdesc=r.choice([0, 0, 1, -1, 256]),
              z=r.choice([0, 5, 255, 256, 32765, -3]), zrot=r.choice([0, 3, -4]), redraw=r.choice([0, 1, -1]),
              bar=r.random() < 0.85, draw=r.choice([None, 0, 1]), draw_state=(r.choice([-1, 0, 1, 2]), r.choice([-1, 0, 1])),
              vis=[r.choice([0, 1, 1, -1, 256])], block=block)
    kw.update(over)
    if block is not None:
        kw["current"] = r.choice([-1, -1, 0, 2, 5, 12])
    return fixture(**kw), block


def positions(r, n):
    return r.choice([-1, -2, 0, 0, 1, 2, n - 1, n, n + 1, n + 5, 0x7FFFFFFF, -0x80000000, r.randint(-3, n + 3)])


# ---- per function

def edges_set_selection():
    for old in (None, 0):
        for new in (-1, 0, 1, 2):
            for vis in (0, 1):
                for current in (-1, 4):
                    for refs, lib in ((1, 0), (0, 0), (2, 0), (1, 1), (1, -1), (-1, 0), (32767, -32768)):
                        yield case(20, [new], draw=old, draw_state=(refs, lib), vis=[vis], current=current, rect=(3, 5, 77, 90),
                                   rowh=11, z=9, zrot=-4)
    for newrefs in (-2, -1, 0, 1, 2):
        for newlib in (-1, 0, 1):
            c = case(20, [1], draw=0, draw_state=(1, 0), q={"ShowMutate": [1], "ShowRefs": [newrefs], "ShowLibrary": [newlib]})
            c["setup"].append((7, [1, 9, 9]))
            yield c
    for z in (0, 32766, 32767, -32768, 70000 & 0x7FFF):
        for rowh in (0, 1, 32767, -1, -32768):
            yield case(20, [2], z=z, rowh=rowh, rect=(-32768, 32767, 32767, 32767), draw=None)
    # Show(0) takes the selection object away from the control: the old one is not deleted (0x101d1f30 falls through)
    for new in (-1, 1):
        for refs, lib in ((1, 0), (0, -1)):
            yield case(20, [new], draw=0, draw_state=(refs, lib), q={"ShowClear": [1]})
    yield case(20, [0], draw=0)            # the same object: nothing happens
    yield case(20, [-1], draw=None)        # NULL over NULL


def edges_alloc():
    r = __import__("random").Random(11)
    for types in ((0,), (1, 0), (0, 1, 0), ()):
        for count in (0, 1, 2, 3, 10, 20):
            yield case(21, [count], types=types)
    for types in ((0,), (1, 0), (0, 1, 0)):
        for entries in (1, 2, 5):
            for used_count in range(1, entries + 1):
                for count in (1, 2, 10):
                    block = forest(r, types, entries, used_count)
                    yield case(21, [count], types=types, block=block)
                    block = forest(r, types, entries, used_count, flat=True)
                    yield case(21, [count], types=types, block=block)


def edges_update():
    r = __import__("random").Random(12)
    base = [(0,), (1,), (0, 1), (1, 0), (0, 1, 0), (1, 1, 1), (0, 0, 0)]
    for types in base:
        for rows in (0, 1, 2, 3, 4):
            for top in (0, 1, 3, 5):
                for vis in (0, 1):
                    for n in (0, 1, 2, 4, 6):
                        block = forest(r, types, n + 2, n)
                        yield case(22, [], types=types, rows=rows, top=top, vis=[vis], block=block, rowh=10, rect=(1, 2, 90, 80))
    # row + top wrapping past 2^31 (a signed compare: the rows that wrap are in the list)
    for types in ((0,), (0, 1)):
        for top in (0x7FFFFFFF, 0x7FFFFFFE, 0x7FFFFFFD, -1, -2, -0x80000000):
            for vis in (0, 1):
                block = forest(r, types, 6, 5)
                yield case(22, [], types=types, rows=4, top=top, vis=[vis], block=block, rowh=10, rect=(1, 2, 90, 80))
    yield case(22, [], array=False, types=(0,), rows=3)
    yield case(22, [], types=(0, 1), rows=0)
    # a deep tree: the indentation follows the depth
    for depth_flat in (False, True):
        for top in range(0, 7):
            block = forest(r, (0, 1), 8, 7, flat=depth_flat)
            yield case(22, [], types=(0, 1), rows=4, top=top, block=block)
    # a single chain: each entry the child of the one before
    ops = [(10, [6])]
    for k in range(5):
        ops.append((11, [k, 5 if k == 0 else -1, 100 + k, k + 1 if k < 4 else -1, k - 1]))
        ops.append((12, [k, 0, 1, k]))
        ops.append((12, [k, 1, 0, k * 11]))
    ops.append((11, [5, -1, 0, -1, -2]))
    for top in range(0, 6):
        yield case(22, [], types=(0, 1), rows=4, top=top, block=(ops, {"first": 0, "n": 5}))


def edges_column_type():
    for count in (0, 1, 2, 3):
        for column in (0, 1, 2, 3, 4, 5, 100, 32767):
            for t in (0, 1, 2, -1, 65535, 65536, 0x12345678, -0x80000000, 0x7FFFFFFF):
                yield case(23, [column, t], types=tuple([1] * count), columns=count > 0)


def edges_get_column_type():
    for types in ((0,), (1,), (0, 1), (1, 0), (0, 1, 0), (7, -1, 5), (2, 2, 2)):
        for column in range(len(types)):
            yield case(24, [column], types=types)


def edges_column_format():
    r = __import__("random").Random(13)
    for types in ((0,), (0, 1), (0, 1, 0)):
        for rows in (0, 1, 3, 4):
            for row in (-0, 0, 1, 2, 3, 4, 5, 100, 32767, 32768):
                for win in (-1, 3, 4, 5, 6) + ((7, 11) if len(types) == 1 else ()):
                    for array, columns in ((True, True), (False, True), (True, False)):
                        if array and row >= 4 and rows > row:
                            continue
                        yield case(25, [row, win], types=types, rows=rows, array=array, columns=columns, filled=0)
    for top, rowh, row in ((32767, 300, 3), (-32768, -300, 2), (0, 32767, 3), (5, -7, 1), (100, 0, 2), (-1, 1, 0)):
        for mz in (0, 255, 32765, -32768):
            c = case(25, [row, 3], types=(0, 1, 0), rows=4, rowh=rowh, rect=(0, top, 50, 100), z=mz, filled=0)
            c["setup"].append((14, [0, 7, 90]))
            c["setup"].append((14, [2, -100, 32767]))
            yield c


def edges_column_formats_all():
    r = __import__("random").Random(14)
    for types in ((0,), (0, 1), (0, 1, 0)):
        for mk in (1, 0):
            for at in (1, 0):
                for rect in ((0, 0, 100, 40), (0, 0, 100, 0), (0, 10, 100, 12), (0, -20, 50, 50), (0, 0, 100, 41)):
                    for rowrect in ((0, 0, 50, 12), (0, 0, 50, 13), (5, 5, 25, 25), (0, 0, 50, 1)):
                        for off in ((0, 0, 0, 0), (1, 2, 5, 9), (0, 0, 0, 10)):
                            row_height = rowrect[3] - rowrect[1] - (off[3] - off[1])
                            if row_height < 1:
                                continue
                            rows = (rect[3] - rect[1]) // row_height
                            for array, draw, bar in ((False, None, True), (True, 0, True), (False, 1, False)):
                                if array and rows > 4:
                                    continue
                                c = case(26, [mk, at], types=types, rect=rect, array=array, draw=draw, bar=bar, rows=0, rowh=0)
                                c["setup"] += [(1, [3, 0, *rowrect]), (9, [3, *off])]
                                yield c
    for queue in ([-1, 3, 4, 5, 6], [3, -1, 4, 5, 6], [3, 4, -1, 5], [4, 4, 4, 4, 4, 4]):
        c = case(26, [1, 1], types=(0, 1), rect=(0, 0, 100, 40), rows=0, rowh=0, array=True, filled=4, q={"Row": queue})
        c["setup"] += [(1, [3, 0, 0, 0, 50, 10]), (1, [4, 0, 0, 0, 50, 10]), (1, [5, 0, 0, 0, 50, 10]), (1, [6, 0, 0, 0, 50, 10])]
        yield c
    for rows_present in (True, False):
        c = case(26, [1, 1], types=(0, 1), rect=(0, 0, 100, 40), rows=2, rowh=7, array=rows_present, filled=2)
        c["setup"] += [(1, [3, 0, 0, 0, 50, 10])]
        yield c


def edges_advance():
    r = __import__("random").Random(15)
    for types in ((0,), (1, 0)):
        for n in (0, 1, 2, 3, 5, 8):
            for flat in (True, False):
                block = forest(r, types, n + 2, n, flat=flat)
                stride = len(types) + 4
                for e in block[1]["used"] + block[1]["free"][:1]:
                    for want in (0, 1):
                        yield case(27, [e * stride, want], types=types, block=block)


def nested_block(types, n):
    """A hierarchical block of n entries, none free, whose last entry in preorder is a descendant: the walk off the end of the chain
    answers -1 (the ancestor's next entry)."""
    for seed in range(2000):
        ops, facts = forest(random.Random(seed), types, n, n)
        if facts["parents"][facts["used"].index(facts["order"][-1])] >= 0:
            return ops, facts
    raise AssertionError("no nested block")


def flat_block(types, n):
    """A flat block of n entries, none free, whose chain does not end at the physical entry 0: the walk off its end answers -2 (not
    -1), and AdvanceIndex(-2) reads entry 0's first word as a child, so the chain goes on from entry 0's successor."""
    for seed in range(2000):
        ops, facts = forest(random.Random(seed), types, n, n, flat=True)
        if facts["used"][n - 1] != 0:
            return ops, facts
    raise AssertionError("no flat block")


def claim_count(c, count):
    """Makes mItemCount larger than the chain (a state the control does not reach by itself) so that the walks stop at -1."""
    op2(c)[1] = count
    return c


def no_storage(count, first=-1):
    """A control that counts entries but has no item block: the walks answer the first entry."""
    return [(10, [0])], {"first": first, "n": count}


def edges_indexed():
    r = __import__("random").Random(16)
    for types in ((0,), (1, 0)):
        for n in (0, 1, 2, 4, 7):
            for flat in (True, False):
                block = forest(r, types, n + 1, n, flat=flat)
                for pos in (-0x80000000, -2, -1, 0, 1, 2, 3, n - 1, n, n + 1, 0x7FFFFFFF):
                    yield case(28, [pos], types=types, block=block)
    # a count with no item block (0x101d2ba4) and a count beyond the chain (0x101d2bcc)
    for count in (1, 3):
        for pos in (-1, 0, 1, 2, 3):
            yield case(28, [pos], types=(0,), block=no_storage(count, first=5))
    for types in ((0,), (1, 0)):
        for n in (2, 3, 4):
            block = nested_block(types, n)
            for pos in (n - 1, n, n + 1, n + 2):
                yield claim_count(case(28, [pos], types=types, block=block), n + 3)
    # a count beyond a flat chain: the walk gets -2 and goes on from entry 0's successor
    for types in ((0,), (1, 0)):
        for n in (2, 3, 4):
            block = flat_block(types, n)
            for pos in (n - 1, n, n + 1, n + 2):
                yield claim_count(case(28, [pos], types=types, block=block), n + 3)
    for first in (-1, 0, 5, 9):
        yield case(28, [0], types=(0,), block=None)
        c = case(28, [3], types=(0,))
        c["setup"][[i for i, (o, _) in enumerate(c["setup"]) if o == 2][0]][1][3] = first
        yield c


def edges_find_free():
    r = __import__("random").Random(17)
    for types in ((0,), (1, 0, 1)):
        for n in (0, 1, 2, 4):
            for cap in (n, n + 1, n + 3):
                for flat in (True, False):
                    for head_free in (False, True):
                        block = forest(r, types, cap, n, flat=flat, head_free=head_free)
                        yield case(29, [], types=types, block=block)
    yield case(29, [], types=(0,))          # no block: the answers are not touched


def edges_should():
    r = __import__("random").Random(18)
    texts = [0, 1, 5, 10, 11, 99]
    for types in ((0,), (1,), (0, 1), (1, 0), (0, 1, 0), (1, 1, 1)):
        n = 3
        block = forest(r, types, n + 1, n, flat=True, texts=texts, nulls=True)
        for sortcol in range(len(types)):
            for sortdesc in (0, 1, -1, 256):
                for e in block[1]["used"][:2] + block[1]["free"][:1]:
                    for k in range(4):
                        item = []
                        for column in range(3):
                            t = types[column] if column < len(types) else 1
                            item += [1, r.choice(texts)] if t == 0 else [0, r.choice([0, 1, -1, 5, 100, 0x7FFFFFFF, -0x80000000])]
                        yield case(30, [e, *item], types=types, block=block, sortcol=sortcol, sortdesc=sortdesc)


def edges_get_data():
    r = __import__("random").Random(19)
    for types in ((0,), (1, 0)):
        for n in (0, 1, 3, 6):
            for flat in (True, False):
                block = forest(r, types, n + 1, n, flat=flat)
                for pos in (-0x80000000, -2, -1, 0, 1, 2, n - 1, n, n + 1, 0x7FFFFFFF):
                    for want in (0, 1):
                        yield case(33, [pos, want], types=types, block=block)
    yield case(33, [0, 1], types=(0,))
    # a count beyond a flat chain: entry -2 is an entry (data NULL: nothing is read from before the block), then the walk goes on
    for types in ((0,), (1, 0)):
        for n in (2, 3, 4):
            block = flat_block(types, n)
            yield claim_count(case(33, [n, 0], types=types, block=block), n + 3)
            for pos in (n + 1, n + 2):
                for want in ((0, 1) if pos == n + 1 else (0,)):   # (entry -2 comes back every second step: its data word is before the block)
                    yield claim_count(case(33, [pos, want], types=types, block=block), n + 3)
    # a count with no item block (0x101d4238) and a count beyond the chain (0x101d4260)
    for count in (1, 3):
        for pos in (-1, 0, 1, 2, 3):
            yield case(33, [pos, 1], types=(0,), block=no_storage(count))
            yield case(33, [pos, 0], types=(0,), block=no_storage(count, first=5))   # no data pointer: nothing is read from the block
    for types in ((0,), (1, 0)):
        for n in (2, 3, 4):
            block = nested_block(types, n)
            for pos in (n - 1, n, n + 1, n + 2):
                for want in (0, 1):
                    yield claim_count(case(33, [pos, want], types=types, block=block), n + 3)


def edges_cur_entry():
    for attributes in (0, 0x100, 0x110, 0x1000):
        for rows in (0, 1, 3, 4):
            for count in (0, 1, 2, 5):
                for row in (-0x80000000, -1, 0, 1, 2, 3, 4, 5, 0x7FFFFFFF):
                    for current in (-1, 0, 2):
                        for draw in (None, 0):
                            for vis in (0, 1):
                                c = case(34, [row], attributes=attributes, rows=rows, current=current, top=1, draw=draw, vis=[vis],
                                         rowh=9, rect=(2, 3, 70, 90), redraw=1)
                                c["setup"][[i for i, (o, _) in enumerate(c["setup"]) if o == 2][0]][1][1] = count
                                yield c
    for redraw in (0, 1, -1):
        yield case(34, [0], rows=2, redraw=redraw, current=-1, draw=0, top=0)
        yield case(34, [-1], rows=2, redraw=redraw, current=1, draw=0, top=0)
    for rowh, top, cur in ((32767, 32767, 5), (-300, -32768, 2), (300, 5, 100000)):
        c = case(34, [1], rows=3, rowh=rowh, rect=(0, top, 50, 100), top=cur - 1, current=-1, draw=0)
        c["setup"][[i for i, (o, _) in enumerate(c["setup"]) if o == 2][0]][1][1] = 9
        yield c


def edges_text_offset():
    for draws in ((-1, -1), (0, -1), (-1, 1), (0, 1), (2, 2)):
        for x, y in ((0, 0), (1, 2), (-1, -2), (20, 0), (32767, -32768), (32768, 65535), (-0x80000000, 0x7FFFFFFF), (0x12345, 0x6789A)):
            c = case(35, [4, x, y])
            c["setup"].append((8, [4, *draws]))
            yield c


def edges_dims():
    for rect in ((0, 0, 100, 40), (5, 6, 7, 8), (0, 0, 0, 0), (-32768, -32768, 32767, 32767), (100, 80, -10, -20), (1, 1, 1, 1)):
        for off in ((0, 0, 0, 0), (1, 2, 5, 9), (10, 10, 0, 0), (-32768, -32768, 32767, 32767), (0, 0, 32767, 32767)):
            for adjust in (0, 1, -1, 2, 32768, 65536, 65537):
                for nw, nh in ((0, 0), (1, 0), (0, 1), (1, 1)):
                    c = case(36, [4, adjust, nw, nh])
                    c["setup"] += [(1, [4, 0, *rect]), (9, [4, *off])]
                    yield c


def edges_add_item():
    r = __import__("random").Random(22)
    # single additions to prepared lists: flat and hierarchical, sorted either way and unsorted, at every kind of position
    for types in ((0,), (1,), (0, 1), (1, 0, 1), (0, 0)):
        for sorted_list in (True, False):
            for sortdesc in (0, 1):
                for n in (0, 1, 2, 4):
                    for flat in (True, False):
                        for _ in range(3):
                            cap = n + r.choice([0, 1, 3])
                            if cap == 0:
                                block = None
                            else:
                                block = forest(r, types, cap, n, flat=flat, texts=[0, 3, 5, 8, 50, 99])
                            for pos in sorted({-1, 0, 1, n - 1, n, n + 2} if n else {-1, 0, 4}):
                                for update in ((0,) if sorted_list and not flat else (0, 1)):   # a sorted tree can break the order the display walks
                                    item = spec(r, types, texts=[0, 3, 4, 5, 8, 50, 99, 7])
                                    c = fixture(types=types, block=block, attributes=0x10 if sorted_list else 0, sortcol=r.randrange(len(types)),
                                                sortdesc=sortdesc, rows=r.choice([0, 2, 4]), top=r.choice([0, 1]), redraw=r.choice([0, 1]),
                                                draw=r.choice([None, 0]), bar=r.random() < 0.85)
                                    c["ops"] = [(31, [r.choice(INTS), pos, update, 0, *item])]
                                    yield c
    for update in (0, 1):
        yield case(31, [5, -1, update, 2, 0, 0, 0, 0, 0, 0], types=(0, 1))     # a NULL item: -1 and nothing else
    # a NULL text in a string column (0x101d3160 as a child of an entry with no child, 0x101d324c as the first entry of an empty list)
    for types in ((0,), (0, 1), (1, 0), (0, 0, 1)):
        for n in (0, 1, 3):
            block = forest(r, types, n + 1, n, flat=(n == 3), texts=[0, 3, 5, 8]) if n else forest(r, types, 2, 0)
            for pos in sorted({-1, 0, n - 1} if n else {-1, 0}):
                for update in (0,):   # the display of a NULL string cell crashes
                    item = [2, 0] * 3
                    c = fixture(types=types, block=block, attributes=0, rows=2, top=0)
                    c["ops"] = [(31, [r.choice(INTS), pos, update, 0, *item])]
                    yield c
                    item = spec(r, types, nulls=True, texts=[0, 3, 5, 8])
                    c = fixture(types=types, block=block, attributes=0, rows=2, top=0)
                    c["ops"] = [(31, [r.choice(INTS), pos, update, 0, *item])]
                    yield c
    # the scroll bar: shown when more entries than rows, hidden otherwise; the visible range sent to the parent
    for rows in (0, 1, 2, 3, 4):
        for n in (0, 1, 2, 3, 4, 5):
            for top in (0, 1, 3, 9):
                block = forest(r, (0, 1), n + 2, n, flat=True) if n else None
                c = fixture(types=(0, 1), block=block, rows=rows, top=top, attributes=0x10)
                c["ops"] = [(31, [7, -1, 0, 0, 1, 20, 0, 5, 0, 0])]
                yield c


def edges_change_item_wide():
    # more columns than a shift count of 32 reaches: bit n of the mask names column n for n < 32 only (the PowerPC's slw)
    for columns in (33, 34, 40, 63, 64, 65, 66, 96):   # (the shift count is cut to 6 bits: column 64 names bit 0 again)
        types = tuple([1] * columns)
        block = forest(random.Random(5), types, 1, 1)
        for mask in (1, 2, 3, -1, 0x7FFFFFFF, -0x80000000, 0x80000001):
            c = fixture(types=types, block=block, rows=0, array=False)
            c["ops"] = [(37, [0, mask, 0, columns, *[x for k in range(columns) for x in (0, 1000 + k)]])]
            yield c


def random_change_item_wide(r):
    columns = r.randint(33, 44)
    types = tuple([1] * columns)
    c = fixture(types=types, block=forest(r, types, 2, r.randint(1, 2)), rows=0, array=False)
    c["ops"] = [(37, [0, r.choice([r.getrandbits(32) - (1 << 31), -1, 1 << 31 - 1]), 0, columns,
                      *[x for k in range(columns) for x in (0, r.choice(INTS))]])]
    return c


def edges_change_item():
    r = __import__("random").Random(23)
    for types in ((0,), (1,), (0, 1), (1, 0, 1), (0, 0, 1), (0, 0, 0)):
        for n in (0, 1, 3):
            block = forest(r, types, n + 1, n, flat=False) if n else None
            for pos in (-0x80000000, -1, 0, 1, 2, n - 1, n, n + 1, 0x7FFFFFFF):
                for mask in (0, 1, 2, 4, 3, 5, 7, 0x80000000 - 0x100000000, -1, 8, 32):
                    for update in (0, 1):
                        for null_item in (0, 2):
                            item = spec(r, types, nulls=True, texts=[0, 3, 5, 8])
                            c = fixture(types=types, block=block, rows=3, top=0)
                            c["ops"] = [(32, [pos, mask, update, null_item, *item])]
                            yield c
    # a count beyond the chain (0x101d36ec): the walk ends at -1 and nothing is changed
    for types in ((0,), (1, 0)):
        for n in (2, 3):
            block = nested_block(types, n)
            for pos in (n - 1, n, n + 1, n + 2):
                for mask in (1, 3):
                    c = fixture(types=types, block=block, rows=3, top=0)
                    c["ops"] = [(32, [pos, mask, 0, 0, *spec(r, types, texts=[0, 3, 5, 8])])]   # (the display would walk past the chain)
                    yield claim_count(c, n + 3)
    # a text cell that is NULL in the block (0x101d375c): the assignment to it is skipped
    for types in ((0,), (0, 1), (1, 0, 0)):
        for seed in range(50):
            block = forest(random.Random(seed), types, 2, 2, nulls=True)
            if sum(1 for o, a in block[0] if o == 12 and a[2] == 2) >= 1:
                break
        for pos in (0, 1):
            for mask in (1, 2, 3, 5, 7):
                c = fixture(types=types, block=block, rows=3, top=0)
                c["ops"] = [(32, [pos, mask, 0, 0, *spec(r, types, texts=[0, 3, 5, 8])])]   # (the display of a NULL string cell crashes)
                yield c
    # a block without strings in the cells the mask names: nothing to assign to
    for mask in (1, 3):
        c = fixture(types=(0, 1), block=forest(r, (0, 1), 2, 1, nulls=True))
        c["ops"] = [(32, [0, mask, 1, 0, 1, 5, 0, 77, 0, 0])]
        yield c


# ---- random cases and scripts

def op2(c):
    return c["setup"][[i for i, (o, _) in enumerate(c["setup"]) if o == 2][0]][1]


def random_case(r, op):
    if op == 20:
        c, _ = rand_world(r)
        c["ops"] = [(op, [r.choice([-1, 0, 1, 2, 3])])]
        c["q"]["ShowMutate"] = [r.choice([0, 1])]
        c["q"]["ShowRefs"] = [r.choice([-2, -1, 0, 1, 2])]
        c["q"]["ShowLibrary"] = [r.choice([-1, 0, 1])]
    elif op == 21:
        types = rand_types(r)
        n = r.randint(1, 6)
        cap = n + r.randint(0, 4)
        c, _ = rand_world(r, types=types, with_block=False)
        block = forest(r, types, cap, n, flat=r.random() < 0.4)
        c = fixture(types=types, block=block, rows=0)
        c["ops"] = [(op, [r.choice([1, 2, 3, 10, 20, 0])])]
        if r.random() < 0.15:
            c = fixture(types=types, rows=0)
            c["ops"] = [(op, [r.choice([1, 20, 0, 10])])]
    elif op == 22:
        types = rand_types(r)
        n = r.randint(0, 9)
        c, _ = rand_world(r, types=types, flat=r.random() < 0.4)
        c["ops"] = [(op, [])]
    elif op in (23, 24):
        types = rand_types(r)
        c = fixture(types=types)
        c["ops"] = [(op, [r.randrange(len(types) + 3) if op == 23 else r.randrange(len(types)),
                          r.choice([0, 1, 2, -1, 65535, 65536, r.getrandbits(32) - (1 << 31)])][:2 if op == 23 else 1])]
    elif op == 25:
        c, _ = rand_world(r, with_block=False)
        c["ops"] = [(op, [r.choice([0, 1, 2, 3, 4, 5, 32767, r.randint(0, 6)]), r.choice([-1, 3, 4, 5, 6])])]
        setup = op2(c)
        setup[5] = r.choice([0, 1, 2, 3, 4])
    elif op == 26:
        types = rand_types(r)
        preset = r.random() < 0.3
        while True:
            height = r.randint(1, 40)
            off = [r.randint(0, 3) for _ in range(4)]
            top = r.randint(-10, 10)
            bottom = r.randint(top, top + 6 * height)
            row_height = height - (off[3] - off[1])
            if row_height >= 1 and (bottom - top) // row_height <= (4 if preset else 6):
                break
        c = fixture(types=types, rows=0, rowh=0, array=preset, filled=0, draw=r.choice([None, 0]),
                    rect=(r.randint(-5, 5), top, r.randint(50, 100), bottom), bar=r.random() < 0.8)
        c["setup"] += [(1, [3, 0, 0, 0, r.randint(10, 60), height]), (9, [3, *off])]
        c["q"]["Row"] = [3] + [r.choice([3, 3, 4, 5, 6]) for _ in range(7)]
        c["ops"] = [(op, [r.choice([1, 1, 1, 0]), r.choice([0, 1])])]
    elif op in (27, 28, 29, 33):
        types = rand_types(r)
        n = r.randint(1 if op == 27 else 0, 9)
        block = forest(r, types, n + r.randint(0, 3), n, flat=r.random() < 0.4, head_free=r.random() < 0.2)
        c = fixture(types=types, block=block)
        stride = len(types) + 4
        if op == 27:
            pool = block[1]["used"] + block[1]["free"][:1]
            if not pool:
                pool = [0]
            c["ops"] = [(op, [r.choice(pool) * stride, r.randrange(2)])]
            if block[1]["capacity"] == 0:
                c = fixture(types=types)
                c["ops"] = [(op, [0, 0])]
                c["setup"] = [s for s in c["setup"]]
        elif op == 28:
            c["ops"] = [(op, [positions(r, n)])]
        elif op == 29:
            c["ops"] = [(op, [])]
        else:
            c["ops"] = [(op, [positions(r, n), r.randrange(2)])]
    elif op == 30:
        types = rand_types(r)
        n = 3
        texts = [0, 1, 5, 10, 11, 99]
        block = forest(r, types, n + 1, n, flat=True, texts=texts, nulls=True)
        c = fixture(types=types, block=block, sortcol=r.randrange(len(types)), sortdesc=r.choice([0, 0, 1, -1, 256]))
        item = []
        for column in range(3):
            t = types[column] if column < len(types) else 1
            item += [1, r.choice(texts)] if t == 0 else [0, r.choice(INTS)]
        c["ops"] = [(op, [r.choice(block[1]["used"] + block[1]["free"][:1]), *item])]
    elif op == 31:
        types = rand_types(r)
        n = r.randint(0, 9)
        sorted_list = r.random() < 0.6
        flat = r.random() < 0.6 if not sorted_list else r.random() < 0.7
        cap = n + r.randint(0, 3)
        block = forest(r, types, cap, n, flat=flat) if cap else None
        if block is not None and n == 0:
            block = (block[0], dict(block[1], first=-1))
        c = fixture(types=types, block=block, attributes=0x10 if sorted_list else 0, sortcol=r.randrange(len(types)),
                    sortdesc=r.choice([0, 1]), rows=r.randint(0, 4), top=r.choice([0, 1, 2]), redraw=r.choice([0, 1]),
                    bar=r.random() < 0.85, draw=r.choice([None, 0]), current=r.choice([-1, 0, 1]))
        pos = r.choice([-1, -1, n, n + 3, 0 if n == 0 else -5]) if (sorted_list and not flat) or n == 0 else positions(r, n)
        update = 0 if sorted_list and not flat else r.choice([0, 1])
        c["ops"] = [(op, [r.choice(INTS), pos, update, 0 if r.random() > 0.05 else 2, *spec(r, types, nulls=not update and not sorted_list)])]
    elif op == 32:
        types = rand_types(r)
        n = r.randint(0, 8)
        block = forest(r, types, n + r.randint(0, 3), n, flat=r.random() < 0.4)
        c = fixture(types=types, block=block, rows=r.randint(0, 4), top=r.choice([0, 1, 2]), bar=True,
                    attributes=r.choice([0, 0x10]))
        update = r.choice([0, 1])
        c["ops"] = [(op, [positions(r, n), r.choice([0, 1, 2, 3, 5, 7, -1, 0x7FFFFFFF, -0x80000000, r.getrandbits(31)]), update,
                          0 if r.random() > 0.08 else 2, *spec(r, types, nulls=not update)])]
    elif op == 34:
        c, _ = rand_world(r, with_block=False)
        c["ops"] = [(op, [r.choice([-5, -1, 0, 1, 2, 3, 4, 5, r.randint(-3, 7)])])]
        op2(c)[1] = r.randint(0, 5)
    elif op == 35:
        c = fixture()
        c["setup"].append((8, [4, r.choice([-1, 0, 1, 2]), r.choice([-1, 0, 1, 3])]))
        c["ops"] = [(op, [4, r.choice(INTS + [r.getrandbits(32)]), r.choice(INTS + [r.getrandbits(32)])])]
    elif op == 36:
        c = fixture()
        rect = [r.randint(-32768, 32767) for _ in range(4)]
        off = [r.randint(-32768, 32767) for _ in range(4)]
        c["setup"] += [(1, [4, 0, *rect]), (9, [4, *off])]
        c["ops"] = [(op, [4, r.choice([0, 1, -1, 5, 65536]), r.randrange(2), r.randrange(2)])]
    return c


def script_case(r, kind):
    """A run of operations over one world, the state compared after each."""
    types = rand_types(r)
    sorted_list = r.random() < 0.5 if kind != "tree" else False
    attributes = 0x10 if sorted_list else 0
    c = fixture(types=types, attributes=attributes, sortcol=r.randrange(len(types)), sortdesc=r.choice([0, 1]),
                rows=r.randint(0, 4), top=0, bar=r.random() < 0.9, draw=r.choice([None, 0]), redraw=r.choice([0, 1]),
                vis=[r.choice([0, 1])], rowh=r.choice([10, 12, 20]), rect=(0, 0, 100, 80))
    ops = []
    count = 0
    steps = r.choice([4, 8, 12, 25, 25]) if kind in ("flat", "tree") else r.randint(4, 14)
    for step in range(steps):
        pick = r.random()
        if pick < 0.7 or count == 0:
            if kind == "tree" and count > 0 and r.random() < 0.7:
                pos = r.randrange(count)
            else:
                pos = r.choice([-1, -1, count, count + 4])
            ops.append((31, [r.choice(INTS), pos, r.choice([0, 0, 1]), 0, *spec(r, types)]))
            count += 1
        elif pick < 0.8:
            ops.append((32, [r.randrange(count), r.choice([1, 2, 3, 5, 7, 0]), r.choice([0, 1]), 0, *spec(r, types)]))
        elif pick < 0.85:
            ops.append((33, [r.randrange(-1, count + 1), r.randrange(2)]))
        elif pick < 0.9:
            ops.append((34, [r.randint(-1, 4)]))
        elif pick < 0.95:
            ops.append((22, []))
        else:
            ops.append((28, [r.randrange(-1, count + 1)]))
    c["ops"] = ops
    return c


def edges_scripts():
    r = __import__("random").Random(21)
    for kind in ("flat", "tree"):
        for _ in range(12):
            yield script_case(r, kind)
    # the growth: more than 20 entries make the block grow twice
    for sorted_list in (False, True):
        types = (0, 1)
        c = fixture(types=types, attributes=0x10 if sorted_list else 0, rows=3, draw=0)
        c["ops"] = [(31, [k, -1, 0, 0, 1, (k * 37) % 100, 0, (k * 91) % 50 - 25, 0, 0]) for k in range(34)]
        yield c


def random_script(r):
    return script_case(r, r.choice(["flat", "flat", "tree", "mixed"]))


TESTS = []
for _op in sorted(set(FN) - {37}):
    _edges = {
        20: edges_set_selection, 21: edges_alloc, 22: edges_update, 23: edges_column_type, 24: edges_get_column_type,
        25: edges_column_format, 26: edges_column_formats_all, 27: edges_advance, 28: edges_indexed, 29: edges_find_free,
        30: edges_should, 31: edges_add_item, 32: edges_change_item, 33: edges_get_data, 34: edges_cur_entry, 35: edges_text_offset, 36: edges_dims,
    }.get(_op, lambda: iter(()))
    TESTS.append(FnTest(NAMES[_op], ppc, host, edge=lambda e=_edges: e(), rnd=lambda r, op=_op: random_case(r, op)))
TESTS.append(FnTest("ChangeItem.wide", ppc, host, edge=lambda: edges_change_item_wide(), rnd=lambda r: random_change_item_wide(r)))
TESTS.append(FnTest("AddItem.ChangeItem.scripts", ppc, host, edge=lambda: edges_scripts(), rnd=lambda r: random_script(r)))
