"""w118: the main panel. CreateMainPanel, InitStaffPanel, the vertical gradient (constructor, destructor, CalcVerts, CalcUVs, Show), the
number and cash text objects (constructors, destructors), SetOriginalPosition, the three small message functions and the three window
scripts run for real on both sides (tests/unit_MainPanel.cpp, cmake/units/MainPanel.cmake). CoolRect and the text sprite's constructor are
real too. Everything else records the same event on both sides: the window members, the draw object and sprite parts, the text formatter, the
mesh calls, the font table, the text helpers and the game type.

Windows are fixed labelled stores (1 to 30); GetWindow answers by the child's id from a queue per id ("W<hex id>", default the window the id
has in the script), Build by the script ("Build<n>"). Objects the functions make with `new` are labelled as they are allocated, and a
SetClientDrawObject or SetDrawObject event carries the class of the object (read from the vtable word on the PowerPC side, from typeid on the
host). The mesh the gradient makes (DMCreateMesh answering 1) is a set of 0xa5-filled arrays that the dump reads word for word.

The original does not survive a NULL `this` where it reaches a virtual (Enable, SetText), so the cases never make those windows NULL:
0x33, 0x37, 0x36 (when the game is not the easy one), 0x147, 0x148 and 0x149. Where the original passes a window straight to a non-virtual
member, a NULL is tested (the fakes accept it). The compiler's game-type guard cannot be reset, so the PowerPC side sets its guard byte in
every case and the construct-on-first-use path is reviewed only. Division by zero (and INT_MIN / -1) answers the dividend, the emulator's
answer and the convention of notes/w66.md.
"""
import ctypes as C
import itertools
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_MainPanel"
MASK = 0xFFFFFFFF

FUNCTIONS = [
    ("CreateMainPanel", 0x101A7878, 2636),
    ("InitStaffPanel", 0x101C02FC, 688),
    ("InterfaceDrawEngineVGradient::InterfaceDrawEngineVGradient", 0x10188FF8, 792),
    ("InterfaceNumberTextObject::InterfaceNumberTextObject", 0x1018D930, 164),
    ("InterfaceCashAndGTickTextObject::InterfaceCashAndGTickTextObject", 0x1018E190, 164),
    ("InterfaceNumberTextObject::~InterfaceNumberTextObject", 0x1018DA0C, 188),
    ("InterfaceCashAndGTickTextObject::~InterfaceCashAndGTickTextObject", 0x1018E274, 188),
    ("InterfaceDrawEngineVGradient::~InterfaceDrawEngineVGradient", 0x1018934C, 140),
    ("InterfaceDrawEngineVGradient::CalcVerts", 0x10189414, 92),
    ("InterfaceDrawEngineVGradient::CalcUVs", 0x101894B0, 76),
    ("InterfaceDrawEngineVGradient::Show", 0x10189538, 108),
    ("InterfaceWindow::SetOriginalPosition", 0x101DB284, 48),
    ("MainPanelMsg", 0x101A5B6C, 160),
    ("HappyGaugeCallback", 0x101A5C44, 36),
    ("DateWindowCallback", 0x101A61D0, 36),
]
(CREATE, STAFF, GRAD_CTOR, NUM_CTOR, CASH_CTOR, NUM_DTOR, CASH_DTOR, GRAD_DTOR, CALC_VERTS, CALC_UVS, SHOW, SET_ORIGINAL, MAIN_MSG,
 HAPPY, DATE) = range(15)

# ---- the program's data
WINDOWS = 30
FONTS = 16
LAB_DRAW, LAB_TOKEN, LAB_FONT, LAB_BLOCK, LAB_NAME = 31, 33, 40, 60, 70
GLOBALS = [0x103949D8, 0x103949C4, 0x103949D4, 0x103949C8, 0x10395A48, 0x10395A44, 0x10395A40, 0x103949E4, 0x10395C24]
WORDS = [0x103949B4, 0x103949C0, 0x103949BC, 0x103949B8, 0x10395A38]
SCREEN_W, SCREEN_H, GAME_TYPE, GAME_GUARD = 0x10395BBC, 0x10395BBA, 0x102B3AA0, 0x102B3AA4
SCRIPTS = [(0x102AA840, 606), (0x102AE164, 195), (0x102AE2EA, 95)]
SCRIPT_ID = {0x102AA840: 1, 0x102AE164: 2, 0x102AE2EA: 3}
CALLBACKS = {0: 0, 0x10267C60: 1, 0x10267C50: 2, 0x10267C58: 3, 0x10267C70: 4, 0x10267C80: 5, 0x10267C48: 6, 0x10267C78: 7, 0x10267C40: 8,
             0x10267C68: 9, 0x10267C88: 10, 0x10267E90: 11, 0x10267E88: 12, 0x10267E80: 13}
CHILDREN = {0x33: 3, 0x37: 4, 0x34: 5, 0x36: 6, 0x2D: 7, 0x2E: 8, 0x2F: 9, 0x32: 10, 0x30: 11, 0x31: 12, 0x1D: 13, 0x21: 14, 0x22: 15,
            0x23: 16, 0x24: 17, 0x1E: 18, 0x1F: 19, 0x20: 20, 0x25: 21, 0x95C: 22, 0x963: 23, 0x95D: 24, 0x147: 26, 0x148: 27, 0x149: 28}
BUILD_DEFAULT = {1: 29, 2: 29, 3: 25}
VTABLES = {0x102A8238: 1, 0x102A8204: 2, 0x102A80F4: 3, 0x102A7B58: 4}   # text sprite, number, cash, vertical gradient
STALE = 30   # the window the pointer globals start with

# the parts of the object in the block (Mac offsets), by label
GRADIENT_PARTS = {61: 0x6C, 62: 0xBC, 63: 0xDC, 64: 0x3C, 65: 0xAC}
TEXT_PARTS = {66: 0xE8, 67: 0x54, 68: 0x9C, 69: 0xB4}

STUBS = None


def ws(e, a, values):
    e.write(a, struct.pack(">" + "I" * len(values), *[v & MASK for v in values]))


def w16(e, a, v):
    e.write(a, struct.pack(">H", v & 0xFFFF))


def s16(e, a):
    return struct.unpack(">h", e.read(a, 2))[0]


def u16(e, a):
    return struct.unpack(">H", e.read(a, 2))[0]


def s32(e, a):
    return T.s32(e.u32(a))


def F(lib, name, result, *args):
    fn = getattr(lib, name)
    fn.restype = result
    fn.argtypes = list(args)
    return fn


def object_kind(case_op):
    """What the dump reads from the block after the operation: 1 a text object, 2 the gradient, 0 nothing."""
    if case_op in (NUM_CTOR, CASH_CTOR):
        return 1
    if case_op in (GRAD_CTOR, CALC_VERTS, CALC_UVS, SHOW):
        return 2
    return 0


def parts_kind(case_op):
    if case_op in (NUM_CTOR, CASH_CTOR, NUM_DTOR, CASH_DTOR):
        return 1
    if case_op in (GRAD_CTOR, GRAD_DTOR, CALC_VERTS, CALC_UVS, SHOW):
        return 2
    return 0


# ======================================================================= the PowerPC side


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    S = STUBS = CL.Stubs()
    R = CL.recorder
    S.real.update(a for _, a, _ in FUNCTIONS)
    # the text sprite's constructor and CoolRect are real as well
    S.real.update([0x1018D11C, 0x101C7448, 0x101C74E0])

    def kind_of(emu, address):
        if address == 0:
            return 0
        if address not in emu.mp_new:
            return 9
        return VTABLES.get(emu.u32(address + 0x38), 8)

    def new(emu):
        size = emu.r(3)
        a = 0
        if emu.cl.ret("New", 1):
            a = emu.alloc(max(size, 4), b"\xa5" * max(size, 4))
            emu.cl.newlab(a)
            emu.cl.heap[0] += 1
            emu.mp_new.add(a)
        emu.cl.rec("operator_new", (size, emu.cl.lab(a)))
        emu.set_r(3, a)

    def delete(emu, address):
        if address:
            emu.cl.heap[2] += 1
        if emu.cl.lab(address) > 0:
            emu.cl.rec("delete", (emu.cl.lab(address),))
        emu.cl.forget(address)

    def build(emu):
        script = SCRIPT_ID.get(emu.r(4), 0)
        emu.cl.rec("Build", (emu.cl.lab(emu.r(3)), script, CALLBACKS[emu.r(5)]))
        label = emu.cl.ret("Build%d" % script, BUILD_DEFAULT.get(script, 0))
        emu.set_r(3, emu.cl.addr.get(label, 0))

    def get_window(emu):
        ident = emu.r(4)
        emu.cl.rec("GetWindow", (emu.cl.lab(emu.r(3)), T.s32(ident)))
        label = emu.cl.ret("W%x" % ident, CHILDREN.get(ident, 0))
        emu.set_r(3, emu.cl.addr.get(label, 0))

    def position(emu):
        emu.cl.rec("GetPosition", (emu.cl.lab(emu.r(3)), T.s16(emu.r(5))))
        for offset, key in zip((4, 6, 8, 10), ("RectL", "RectT", "RectR", "RectB")):
            w16(emu, emu.r(4) + offset, emu.cl.ret(key, 0))
        emu.set_r(3, 1)

    def set_position(emu):
        rect = emu.r(4)
        emu.cl.rec("SetPosition", (emu.cl.lab(emu.r(3)),) + tuple(s16(emu, rect + o) for o in (4, 6, 8, 10)))
        emu.set_r(3, 1)

    def draw_object(name):
        def stub(emu):
            emu.cl.rec(name, (emu.cl.lab(emu.r(3)), emu.cl.lab(emu.r(4)), kind_of(emu, emu.r(4))))
        return stub

    def adjust(emu):
        address = emu.r(3)
        value = s32(emu, address)
        emu.cl.rec("AdjustJustification", (value,))
        ws(emu, address, [emu.cl.ret("Adjust", value)])

    def measure(emu):
        rect = emu.r(4)
        text = CL.wide_text(emu, emu.r(3)) if emu.r(3) else "(null)"
        emu.cl.rec("MeasureText", (T.s8(emu.r(5)), T.s8(emu.r(6)), T.s8(emu.r(7)), T.s32(emu.r(8))) + tuple(s32(emu, rect + 4 * i) for i in range(4)),
                   (text,))
        extent = emu.r(9)
        for i, key in enumerate(("MeasureL", "MeasureT", "MeasureR", "MeasureB")):
            ws(emu, extent + 4 * i, [emu.cl.ret(key, 0)])

    def sprite_ctor(emu):
        this, data = emu.r(3), emu.r(4)
        emu.cl.rec("DrawObject.ctor", (emu.cl.lab(this), emu.cl.lab(data)))
        ws(emu, this, [data])
        emu.cl.rec("GameData.ctor", (emu.cl.lab(this + 0x54),))
        emu.cl.rec("Surface.ctor", (emu.cl.lab(this + 0x9C),))
        emu.cl.rec("List.ctor", (emu.cl.lab(this + 0xB4),))

    def draw_ctor(emu):
        this, data = emu.r(3), emu.r(4)
        emu.cl.rec("DrawObject.ctor", (emu.cl.lab(this), emu.cl.lab(data)))
        emu.write(this + 4, bytes(0x34))
        ws(emu, this, [data])

    def load_textures(emu):
        emu.cl.rec("DMLoadTextures", (emu.cl.lab(emu.r(3)),))
        emu.set_r(3, T.u32(emu.cl.ret("Load", 1)))

    def create_mesh(emu):
        L = emu.cl
        s, textures, name = emu.r(3), emu.r(4), emu.r(5)
        L.rec("DMCreateMesh", (L.lab(s), L.lab(textures), emu.r(6)), (emu.cstr(name)[:159] if name else "(null)",))
        made = L.ret("Create", 1)
        if made:
            def block(size):
                return emu.alloc(size, b"\xa5" * size)

            ws(emu, s + 0x10, [block(2 * 0x30), block(16 * 0xC), block(2 * 0x20), block(4 * 8)])
            entry = emu.u32(s + 8)
            matrix = block(0x40)
            L.newlab(matrix)
            ws(emu, entry + 0x28, [matrix, block(0x10)])
            ws(emu, textures + 0x18, [block(8)])
        emu.set_r(3, T.u32(made))

    def call_default(emu):
        emu.cl.rec("CallDefault", (emu.cl.lab(emu.r(3)), T.s32(emu.r(4)), T.s32(emu.r(5)), T.s32(emu.r(6))))
        emu.set_r(3, T.u32(emu.cl.ret("Default", 0)))

    def enable(emu):
        emu.cl.rec("Enable", (emu.cl.lab(emu.r(3)), T.s16(emu.r(4))))
        emu.set_r(3, 0)

    def set_text(emu):
        text = CL.wide_text(emu, emu.r(4)) if emu.r(4) else "(null)"
        emu.cl.rec("SetText", (emu.cl.lab(emu.r(3)),), (text,))
        emu.set_r(3, 0)

    e.mp_window_vt = T.persist_alloc(e, 0x40)
    ws(e, e.mp_window_vt + 0x1C, [T.fake_function(e, enable)])
    ws(e, e.mp_window_vt + 0x2C, [T.fake_function(e, set_text)])
    S.imp("__nw__FUl", new)
    S.imp("__dl__FPv", lambda emu: delete(emu, emu.r(3)))
    for a, name, spec in [(0x101DD8BC, "setZ", "Lh"), (0x101DC1EC, "Show", "Lh"), (0x101DA9D0, "ShiftPosition", "Lhh"),
                          (0x101DA118, "SetFont", "LL"), (0x101DA3AC, "SetFontColour", "Liiii"), (0x101DA204, "SetFontJust", "Lii"),
                          (0x101DE130, "SetClientDrawText", "LL"), (0x101DB7DC, "SetFrame", "Li"), (0x101DCFD0, "setExtraParam", "Lhi"),
                          (0x101DD7F0, "setCursor", "Lh"), (0x101DE084, "unlink", "L"), (0x101CCD9C, "DrawObject.dtor", "L"),
                          (0x10030178, "DMFreeTextures", "L"), (0x1003088C, "DMEnableMeshRender", "L"),
                          (0x100309C8, "DMDisableMeshRender", "L"), (0x100C0E28, "SetMatrixIdentity", "L")]:
        S.internal(a, R(name, spec))
    # the text formatter, the sprite list and the surface are other libraries: imports, reached through glue that saves the TOC
    for name, event in (("__ct__15TbTextFormatterFv", "Formatter.ctor"), ("__dt__15TbTextFormatterFv", "Formatter.dtor"),
                        ("__dt__11CSpriteListFv", "List.dtor"), ("__dt__17SSpriteEngineDataFv", "Surface.dtor")):
        S.imp(name, R(event, "L"))
    S.internal(0x10030AE8, R("DMFreeMesh", "L", ret=lambda emu, L: 1))
    S.internal(0x101DEB78, build)
    S.internal(0x101DBE40, get_window)
    S.internal(0x101DCBA8, lambda emu: emu.cl.rec("SetCallback", (emu.cl.lab(emu.r(3)), CALLBACKS[emu.r(4)])))
    S.internal(0x101DB884, position)
    S.internal(0x101DAE98, set_position)
    S.internal(0x101DB474, draw_object("SetDrawObject"))
    S.internal(0x101DB624, draw_object("SetClientDrawObject"))
    S.internal(0x101DE318, call_default)
    S.internal(0x10180270, R("GetFontTableEntry", "u", ret=lambda emu, L: L.addr[LAB_FONT + (emu.r(3) & 15)]))
    S.internal(0x101CD2D4, R("FindDrawObject", "s", ret=lambda emu, L: L.addr.get(L.ret("Draw", LAB_DRAW), 0)))
    S.internal(0x1018B2C8, adjust)
    S.internal(0x1018B8E4, measure)
    S.internal(0x1016F5D8, lambda emu: None)
    S.internal(0x101CCCC0, draw_ctor)
    S.internal(0x1018C978, sprite_ctor)
    S.internal(0x1003013C, load_textures)
    S.internal(0x100301C0, create_mesh)


def state(case):
    """The starting state shared by both sides: nine window labels, five words, the screen size and the game type."""
    st = [0, STALE, STALE, STALE, STALE, STALE, STALE, 1, 2, 0x5A5A5A5A, 0x12345678, -0x2468ACE, 0x7777, 0x7A7A7A7A, 1024, 768, 0]
    for index, value in case.get("st", {}).items():
        st[index] = value
    return st


class Ppc:
    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        e.mp_new = set()
        self.L = e.cl = CL.PpcLog({}, case.get("q"))
        self.L.heap = [0, 0, 0, 0]
        self.L.addr = {}
        self.saved = STUBS.activate(e)
        L = self.L
        for label in range(1, WINDOWS + 1):
            a = e.alloc(0x200, b"\xa5" * 0x200)
            L.labels[a] = label
            L.addr[label] = a
            ws(e, a + 0x118, [e.mp_window_vt])
        for label in range(LAB_FONT, LAB_FONT + FONTS):
            a = e.alloc(16, b"\xa5" * 16)
            L.labels[a] = label
            L.addr[label] = a
        store = e.alloc(16, b"\xa5" * 16)
        for label, a in ((LAB_DRAW, store), (LAB_TOKEN, store + 8)):
            L.labels[a] = label
            L.addr[label] = a
        name = e.alloc(16, b"meter.wct\0" + b"\xa5" * 6)
        L.labels[name] = LAB_NAME
        L.addr[LAB_NAME] = name
        self.block = e.alloc(1024, b"\xa5" * 1024)
        L.labels[self.block] = LAB_BLOCK
        L.addr[LAB_BLOCK] = self.block
        parts = GRADIENT_PARTS if parts_kind(case["op"]) == 2 else TEXT_PARTS if parts_kind(case["op"]) == 1 else {}
        for label, offset in parts.items():
            L.labels[self.block + offset] = label
            L.addr[label] = self.block + offset
        st = state(case)
        for a, label in zip(GLOBALS, st[0:9]):
            ws(e, a, [self.a(label)])
        for a, value in zip(WORDS, st[9:14]):
            ws(e, a, [value])
        w16(e, SCREEN_W, st[14])
        w16(e, SCREEN_H, st[15])
        ws(e, GAME_TYPE, [st[16]])
        e.write(GAME_GUARD, b"\1")

    def a(self, label):
        return self.L.addr.get(label, 0)

    def text_kind(self):
        return VTABLES.get(self.e.u32(self.block + 0x38), 0) if VTABLES.get(self.e.u32(self.block + 0x38), 0) in (1, 2, 3) else 0

    def dump_text(self, out):
        e, b = self.e, self.block
        out.append(self.text_kind())
        out += [T.s8(e.read(b + 0xDC, 1)[0]), T.s8(e.read(b + 0xDD, 1)[0])]
        out += [e.read(b + o, 1)[0] for o in (0xDE, 0xDF, 0xE0, 0xE1)]
        out += [s16(e, b + 0xE2), s16(e, b + 0xE4)]
        out += [e.read(b + o, 1)[0] for o in (0x48, 0x49, 0x4A, 0x4B)]
        out.append(self.L.lab(e.u32(b)))

    def dump_gradient(self, out):
        e, L, b = self.e, self.L, self.block
        u = e.u32
        out.append(4 if u(b + 0x38) == 0x102A7B58 else 0)
        built = u(b) != 0
        out.append(int(built))
        out += [s32(e, b + 0xAC), L.lab(u(b + 0xB0)), s32(e, b + 0xBC), u16(e, b + 0xC0)]
        folder = u(b + 0xC4)
        out += [ord(c) for c in e.cstr(folder)[:23]] + [0] if folder else [0xFE]
        out += [int(u(b + 0xC8) == 0), int(u(b + 0xCC) == b + 0xAC), int(u(b + 0xD0) == 0)]
        out += [s32(e, b + 0x6C), u16(e, b + 0x70), int(u(b + 0x74) == b + 0x3C), int(u(b + 0x78) == b + 0xDC)]
        out += [s32(e, b + 0x3C)] + [u16(e, b + o) for o in (0x40, 0x42, 0x44, 0x46, 0x48)]
        out += [s32(e, b + 0x4C + 4 * i) for i in range(6)] * 2
        out.append(s32(e, b + 0x120))
        # what the constructor leaves as it found it: the entry's +0xe, the texture entry's +8 and +0xc and the matrix
        out += [u16(e, b + 0x4A), s32(e, b + 0xB4), s32(e, b + 0xB8)] + [s32(e, b + 0xDC + 4 * i) for i in range(16)]
        if not built:
            return
        vertices, points, uvs, faces, faceset = u(b + 0x7C), u(b + 0x80), u(b + 0x84), u(b + 0x88), u(b + 0x68)
        out += [s32(e, vertices + 4 * i) for i in range(24)]
        out += [s32(e, points + 4 * i) for i in range(48)]
        out += [s32(e, uvs + 4 * i) for i in range(16)]
        out += [u16(e, faces + 2 * i) for i in range(16)]
        out += [int(u(faceset) == u(b + 0xD4))] + [u16(e, faceset + o) for o in (4, 6, 8, 0xA)]
        out += list(e.read(faceset + 0xC, 4))
        out += [int(u(b + 0xD4) != 0), L.lab(u(b + 0x64))]

    def dump(self, op):
        e, L = self.e, self.L
        out = [L.lab(e.u32(a)) for a in GLOBALS]
        out += [s32(e, a) for a in WORDS]
        out += [s16(e, SCREEN_W), s16(e, SCREEN_H), s32(e, GAME_TYPE)]
        for label in range(1, WINDOWS + 1):
            a = self.a(label)
            out += [s16(e, a + o) for o in (0x14, 0x16, 0x18, 0x1A)]
        out += L.heap
        kind = object_kind(op)
        if kind == 1:
            self.dump_text(out)
        elif kind == 2:
            self.dump_gradient(out)
        return out

    def run(self, case):
        e = self.e
        op, a, b, c, d = case["op"], case.get("a", 0), case.get("b", 0), case.get("c", 0), case.get("d", 0)
        address = FUNCTIONS[op][1]
        result = 0
        block = self.block

        def call(args):
            return T.call_ppc(e, address, [v & MASK for v in args])[0]

        if op == CREATE or op == STAFF:
            call([])
        elif op == GRAD_CTOR:
            call([block, self.a(LAB_NAME) if a else 0])
        elif op in (NUM_CTOR, CASH_CTOR):
            call([block, self.a(LAB_TOKEN) if a else 0])
        elif op in (NUM_DTOR, CASH_DTOR, GRAD_DTOR):
            ctor = {NUM_DTOR: NUM_CTOR, CASH_DTOR: CASH_CTOR, GRAD_DTOR: GRAD_CTOR}[op]
            T.call_ppc(e, FUNCTIONS[ctor][1], [block, self.a(LAB_NAME) if op == GRAD_DTOR else 0])
            call([block, a])
        elif op in (CALC_VERTS, CALC_UVS, SHOW):
            T.call_ppc(e, FUNCTIONS[GRAD_CTOR][1], [block, self.a(LAB_NAME)])
            if op == SHOW:
                w16(e, block + 4, b)
                call([block, a])
            else:
                ws(e, block + 0x120, [a])
                call([block])
        elif op == SET_ORIGINAL:
            rect = e.alloc(16, struct.pack(">I4h", 0x102AF8FC, *[T.s16(v) for v in case.get("rect", (0, 0, 0, 0))]))
            result = T.s32(call([self.a(a), 0 if b else rect]))
        else:
            result = T.s32(call([self.a(a), b, c, d]))
        return dict(result=result, state=self.dump(op), log=self.L.log)


def ppc_case(e, case):
    p = Ppc(e, case)
    try:
        return p.run(case)
    finally:
        CL.Stubs.deactivate(e, p.saved)


# ======================================================================= the host side


def host_case(lib, case):
    F(lib, "tpw_mp_reset", None)()
    fixed = {i: F(lib, "tpw_mp_obj", C.c_void_p, C.c_int32)(i) for i in list(range(1, WINDOWS + 1)) + [LAB_DRAW, LAB_TOKEN, LAB_NAME, LAB_BLOCK]
             + list(range(LAB_FONT, LAB_FONT + FONTS))}
    kind = parts_kind(case["op"])
    if kind:
        part = F(lib, "tpw_mp_part", C.c_void_p, C.c_int32, C.c_int32)
        for label in (GRADIENT_PARTS if kind == 2 else TEXT_PARTS):
            fixed[label] = part(label, 4 if kind == 2 else 1)
    CL.host_begin(lib, fixed, case.get("q"))
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    st = state(case)
    F(lib, "tpw_mp_state", None, C.POINTER(C.c_int32))((C.c_int32 * len(st))(*[T.s32(v & MASK) for v in st]))
    rect = case.get("rect", (0, 0, 0, 0))
    F(lib, "tpw_mp_rect", None, C.POINTER(C.c_int32))((C.c_int32 * 4)(*[T.s16(v & 0xFFFF) for v in rect]))
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
    op = case["op"]
    a, b, c, d = (T.s32(case.get(k, 0) & MASK) for k in "abcd")
    result = F(lib, "tpw_mp_call", C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32)(op, a, b, c, d)
    log = CL.host_end(lib)
    out = (C.c_int32 * 4000)()
    n = F(lib, "tpw_mp_dump", C.c_int32, C.POINTER(C.c_int32), C.c_int32)(out, object_kind(op))
    return dict(result=result, state=list(out[:n]), log=log)


# ======================================================================= the cases

SHORTS = [-32768, -1, 0, 1, 2, 7, 1535, 1536, 2047, 2048, 32767]
EDGE_INTS = [0, 1, -1, 2047, 2048, 65000, 0x200000, 0x7FFFFFFF, -0x80000000]
SIZES = [1, -1, 2, 7, 1024, 0x7FFF, -0x8000, 0]
FRACTIONS = [0x00000000, 0x80000000, 0x3F800000, 0xBF800000, 0x42480000, 0x42C80000, 0x43160000, 0x7FC00000, 0x7F800000, 0xFF800000,
             0x00000001, 0x007FFFFF, 0x80000001, 0x7F7FFFFF, 0x3C23D70A, 0x42C7FFFF, 0x42C80001]

# the NULL tests of CreateMainPanel that exist, as the queue keys of the windows they test
OPTIONAL = ["W2d", "W2e", "W2f", "W30", "W31", "W1d", "W21", "W22", "W23", "W24", "W1f"]
# unchecked windows that only reach non-virtual members, and so may be NULL
LOOSE = ["W34", "W32", "W1e", "W20", "W25"]


def case(op, **values):
    return dict(op=op, **values)


def rects(r, count=7):
    """Four queues of rectangle values, one value a call."""
    return {key: [r(i) for i in range(count)] for key in ("RectL", "RectT", "RectR", "RectB")}


def create_edges():
    yield case(CREATE, st={0: 30})                                    # the guard: nothing is built
    yield case(CREATE, st={0: 29}, q={"Build1": [0]})
    yield case(CREATE)
    yield case(CREATE, q={"Build1": [0]})                             # Build answers NULL: part A is skipped
    yield case(CREATE, q={"Build1": [0], "W1d": [0]})                 # nothing at all
    for game in (0, 1, 2, 3, -1, 0x100):
        yield case(CREATE, st={16: game})
    for key in OPTIONAL + LOOSE:
        yield case(CREATE, q={key: [0]})
        yield case(CREATE, st={16: 2}, q={key: [0]})
    for keys in itertools.combinations(OPTIONAL, 2):
        yield case(CREATE, q={key: [0] for key in keys})
    for position in range(7):
        yield case(CREATE, q={"New": [0 if i == position else 1 for i in range(7)]})
        yield case(CREATE, st={16: 2}, q={"New": [0 if i == position else 1 for i in range(7)]})
    yield case(CREATE, q={"New": [0]})
    for load, create in ((0, 1), (1, 0), (1, 1), (0, 0)):
        yield case(CREATE, q={"Load": [load], "Create": [create]})
    for answers in ([0, 2], [2, 0], [1, 1], [-1, 5], [258, 3], [0, 0], [2, 2], [3, 1]):
        yield case(CREATE, q={"Adjust": answers})
    for right, bottom in itertools.product(EDGE_INTS, repeat=2):
        yield case(CREATE, q={"MeasureR": [right], "MeasureB": [bottom], "MeasureL": [5], "MeasureT": [-3]})
    for width, height in itertools.product(SIZES, repeat=2):
        yield case(CREATE, st={14: width, 15: height}, q={"MeasureR": [777], "MeasureB": [33]})
    for width, height in itertools.product((1, 0, -1, 7), (1, 0, -1, 7)):
        yield case(CREATE, st={14: width, 15: height}, q={"MeasureR": [-0x80000000], "MeasureB": [-0x80000000]})
    # rectangles: odd and negative sizes (the halves truncate towards zero) and every short at its extreme
    for first, second in itertools.product(SHORTS, repeat=2):
        q = rects(lambda i: first if i % 2 else second)
        q["MeasureB"] = [11]
        q["MeasureR"] = [400]
        yield case(CREATE, q=q)
    for values in ([-32768] * 7, [32767] * 7, [-1] * 7, [1] * 7, [0] * 7, [3, -3, 5, -5, 9, -9, 11]):
        yield case(CREATE, q=rects(lambda i, v=values: v[i]))
    for pattern in itertools.product((-32768, -7, 0, 7, 32767), repeat=4):
        # the rectangle of the window 0x32 (the fourth GetPosition) and the number window's (the third)
        q = {"RectL": [0, 0, pattern[0], pattern[1]], "RectT": [0, 0, pattern[2], pattern[3]], "RectR": [0, 0, 1, 2], "RectB": [0, 0, 3, 4]}
        yield case(CREATE, q=q)
    yield case(CREATE, st={9: 0, 10: 0, 11: 0, 12: 0, 13: 0, 1: 0, 2: 0, 3: 0})
    yield case(CREATE, st={9: -1, 10: -1, 11: -1, 12: -1, 1: 0, 2: 0, 3: 0})


def create_random(r):
    q = {}
    for key in OPTIONAL + LOOSE:
        if r.random() < 0.1:
            q[key] = [0]
    if r.random() < 0.1:
        q["Build1"] = [0]
    q["New"] = [0 if r.random() < 0.1 else 1 for _ in range(7)]
    q["Load"] = [r.choice([0, 1, 1, 1])]
    q["Create"] = [r.choice([0, 1, 1, 1])]
    q["Adjust"] = [r.choice([0, 1, 2, -1, 258, r.randrange(-100, 100)]) for _ in range(2)]
    q["MeasureR"] = [r.choice(EDGE_INTS + [r.randrange(-5000, 5000)])]
    q["MeasureB"] = [r.choice(EDGE_INTS + [r.randrange(-5000, 5000)])]
    for key in ("RectL", "RectT", "RectR", "RectB"):
        q[key] = [r.choice(SHORTS + [r.randrange(-32768, 32768)]) for _ in range(7)]
    st = {14: r.choice(SIZES + [r.randrange(1, 4000)]), 15: r.choice(SIZES + [r.randrange(1, 4000)]), 16: r.choice([0, 1, 2, 2, -1, 3]),
          9: r.getrandbits(31), 10: r.getrandbits(31), 11: r.getrandbits(31), 12: r.getrandbits(31), 13: r.getrandbits(31)}
    if r.random() < 0.1:
        st[0] = 30
    return case(CREATE, st=st, q=q)


STAFF_OPTIONAL = ["Build2", "Build3", "W95c", "W963", "W95d"]


def staff_edges():
    yield case(STAFF)
    for key in STAFF_OPTIONAL:
        yield case(STAFF, q={key: [0]})
    yield case(STAFF, q={"W95d": [0, 24]})
    yield case(STAFF, q={"W95d": [24, 0]})
    for keys in itertools.combinations(STAFF_OPTIONAL, 2):
        yield case(STAFF, q={key: [0] for key in keys})
    yield case(STAFF, q={key: [0] for key in STAFF_OPTIONAL})
    yield case(STAFF, q={"New": [0]})
    yield case(STAFF, q={"Draw": [0]})
    yield case(STAFF, q={"New": [0], "Draw": [0], "Build3": [0]})
    yield case(STAFF, st={13: 0, 4: 0, 5: 0, 6: 0})
    yield case(STAFF, st={13: -1, 4: 22, 5: 23, 6: 25})
    yield case(STAFF, q={"W95c": [11], "W963": [12], "Build3": [13]})   # other windows in the globals


def staff_random(r):
    q = {}
    for key in STAFF_OPTIONAL:
        if r.random() < 0.15:
            q[key] = [0]
        elif r.random() < 0.3:
            q[key] = [r.randrange(1, 29)]
    q["New"] = [r.choice([0, 1, 1])]
    q["Draw"] = [r.choice([0, LAB_DRAW])]
    return case(STAFF, st={13: r.getrandbits(31), 4: r.randrange(0, 30), 5: r.randrange(0, 30), 6: r.randrange(0, 30)}, q=q)


def grad_queues(load, create):
    return {"Load": [load], "Create": [create]}


def grad_ctor_edges():
    for load, create, name in itertools.product((0, 1), (0, 1), (0, 1)):
        yield case(GRAD_CTOR, a=name, q=grad_queues(load, create))
    yield case(GRAD_CTOR, a=1, q={"Load": [7], "Create": [-5]})


def grad_ctor_random(r):
    return case(GRAD_CTOR, a=r.randrange(2), q=grad_queues(r.choice([0, 1, 1, 1, 3]), r.choice([0, 1, 1, 1, 9])))


def text_ctor_edges(op):
    def edges():
        yield case(op, a=0)
        yield case(op, a=1)
    return edges


def dtor_edges(op):
    def edges():
        for flag in (-32768, -1, 0, 1, 2, 32767, 65535, 65536, 65537):
            if op == GRAD_DTOR:
                for load, create in ((0, 1), (1, 0), (1, 1)):
                    yield case(op, a=flag, q=grad_queues(load, create))
            else:
                yield case(op, a=flag)
    return edges


def dtor_random(op):
    def random(r):
        q = grad_queues(r.choice([0, 1, 1]), r.choice([0, 1, 1])) if op == GRAD_DTOR else None
        return case(op, a=r.choice([r.randrange(-32768, 32768), r.getrandbits(32)]), q=q)
    return random


def calc_edges(op):
    def edges():
        for fraction in FRACTIONS:
            yield case(op, a=fraction, q=grad_queues(1, 1))
    return edges


def calc_random(op):
    def random(r):
        bits = r.getrandbits(32)
        if r.random() < 0.3:
            bits = struct.unpack(">I", struct.pack(">f", r.choice([-1.0, 0.0, 1.0, 50.0, 100.0, 150.0, r.uniform(-300, 300)])))[0]
        return case(op, a=bits, q=grad_queues(1, 1))
    return random


def show_edges():
    for shown, state_word in itertools.product((0, 1, 2, -1, 32767, -32768, 65536, 65537, 65535, 0x10000 + 5), (0, 1, 2, -1, -32768, 32767)):
        yield case(SHOW, a=shown, b=state_word, q=grad_queues(1, 1))


def show_random(r):
    return case(SHOW, a=r.choice([r.randrange(-32768, 32768), r.getrandbits(32)]), b=r.choice([0, 1, r.randrange(-32768, 32768)]),
                q=grad_queues(1, 1))


def original_edges():
    for window in (1, 14, 30):
        for rect in itertools.product((-32768, -1, 0, 1, 32767), repeat=4):
            yield case(SET_ORIGINAL, a=window, rect=rect)
        yield case(SET_ORIGINAL, a=window, b=1, rect=(1, 2, 3, 4))
        yield case(SET_ORIGINAL, a=window, rect=(0x1234, 0x5678 - 65536, -2, 3))


def original_random(r):
    return case(SET_ORIGINAL, a=r.randrange(1, 31), b=int(r.random() < 0.1), rect=tuple(r.randrange(-32768, 32768) for _ in range(4)))


def message_edges(op):
    def edges():
        for window in (1, 13, 30):
            for message in (0x14, 0x13, 0x15, 0, 1, -1, 0x7FFFFFFF, 0x100000014 & MASK, 0x24):
                for panel in (0, 30):
                    yield case(op, a=window, b=message, c=-5, d=0x1234567, st={0: panel}, q={"Default": [-9]})
        yield case(op, a=13, b=0x14, c=MASK, d=0, q={"Default": [0x7FFFFFFF]})
    return edges


def message_random(op):
    def random(r):
        return case(op, a=r.randrange(1, 31), b=r.choice([0x14, 0x14, 0x13, r.getrandbits(32)]), c=r.getrandbits(32), d=r.getrandbits(32),
                    st={0: r.choice([0, 30, 13])}, q={"Default": [r.getrandbits(32)]})
    return random


# ---- the three scripts, word for word


def table_ppc(e, c):
    a, _ = SCRIPTS[c["table"]]
    return list(struct.unpack(">" + "h" * c["n"], e.read(a + c["start"] * 2, c["n"] * 2)))


def table_host(lib, c):
    p = F(lib, "tpw_mp_script", C.POINTER(C.c_int16), C.c_int32)(c["table"])
    return [p[c["start"] + i] for i in range(c["n"])]


def table_edges():
    for i, (_, n) in enumerate(SCRIPTS):
        yield dict(table=i, start=0, n=n)


def table_random(r):
    i = r.randrange(3)
    n = SCRIPTS[i][1]
    start = r.randrange(n)
    return dict(table=i, start=start, n=r.randrange(1, n - start + 1))


def fn(op, edges, random):
    return FnTest(FUNCTIONS[op][0], ppc_case, host_case, edge=edges, rnd=random)


TESTS = [
    fn(CREATE, create_edges, create_random),
    fn(STAFF, staff_edges, staff_random),
    fn(GRAD_CTOR, grad_ctor_edges, grad_ctor_random),
    fn(NUM_CTOR, text_ctor_edges(NUM_CTOR), lambda r: case(NUM_CTOR, a=r.randrange(2))),
    fn(CASH_CTOR, text_ctor_edges(CASH_CTOR), lambda r: case(CASH_CTOR, a=r.randrange(2))),
    fn(NUM_DTOR, dtor_edges(NUM_DTOR), dtor_random(NUM_DTOR)),
    fn(CASH_DTOR, dtor_edges(CASH_DTOR), dtor_random(CASH_DTOR)),
    fn(GRAD_DTOR, dtor_edges(GRAD_DTOR), dtor_random(GRAD_DTOR)),
    fn(CALC_VERTS, calc_edges(CALC_VERTS), calc_random(CALC_VERTS)),
    fn(CALC_UVS, calc_edges(CALC_UVS), calc_random(CALC_UVS)),
    fn(SHOW, show_edges, show_random),
    fn(SET_ORIGINAL, original_edges, original_random),
    fn(MAIN_MSG, message_edges(MAIN_MSG), message_random(MAIN_MSG)),
    fn(HAPPY, message_edges(HAPPY), message_random(HAPPY)),
    fn(DATE, message_edges(DATE), message_random(DATE)),
    FnTest("the three window scripts, word for word", table_ppc, table_host, edge=table_edges, rnd=table_random),
]
