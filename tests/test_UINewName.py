"""Unit w43b: the login panel's name entry dialog (NewName), the window maker Dialog, InterfaceWindow::setZ and GetClientRect, and
InterfaceCheckButtons::SetCurrent (and, unit v5L14, InterfaceWindow::SetClientRect), against libtpw_unit_UINewName.dylib
(tests/unit_UINewName.cpp).

The five functions run for real on both sides (with the rectangles CoolRect, and NewName's call of Dialog and Dialog's of setZ). Everything
else they call is a fake that records the same event on both sides: the window's setters and virtual SetText, GetWindow (the first child with
the id), the script builder and the window factory (they answer the windows the case queues), the highest depth and the window's position,
the edit box's members, the buttons' SetState, the draw objects (Change records the depth of a flag 2 change), the draw object library, the
font table, the string table's answer, the title colour, the advisor's Add and the three text objects' constructors. Windows, draw objects,
fonts and texts have fixed labels. After every case the whole log, the answer, every window's depth and current id, the globals and the
name buffer are compared.

Seven tests: NewName (the panel present or not, the dialog tree as the script builds it plus variants: the check buttons with the child 0x7a17
and with a child -1, every current id; random depths, draw objects, strings and queued answers), Dialog (every argument, a missing holder or
window, any rectangle and highest depth, random trees), setZ (random trees of up to eight levels, every draw object combination, depths
around the cut at 0x100 and the 16-bit wrap), SetCurrent (every current id and argument against random children), GetClientRect and SetClientRect (with and
without a rectangle), and the dialog's window description against the image.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UINewName"

# ---- the functions
NEWNAME, DIALOG, SETZ, SETCURRENT, CLIENTRECT = 0x101AF60C, 0x101F12C8, 0x101DD8BC, 0x101CC148, 0x101DBA4C
SETCLIENTRECT = 0x101DB3FC        # unit v5L14
COOLRECT = [0x101C7448, 0x101C7494, 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7678]   # the rectangles run as the original
# the fakes (in-binary functions)
ADVISOR_ADD, FONT_ENTRY, GET_STRING, TITLE_COLOUR = 0x100A7074, 0x10180270, 0x10180468, 0x1018BD3C
TEXT_CTOR, CURSOR_CTOR, TITLE_CTOR = 0x1018D11C, 0x1018F1D0, 0x1018FBE8
FIND_DRAW, DRAW_CHANGE = 0x101CD2D4, 0x101CCEC8
STATIC_AREA, CURSOR_DRAW, SELECT_ALL, UNACCEPTED = 0x101CD7E0, 0x101CDAC4, 0x101CE8A4, 0x101CE94C
SET_FONT, FONT_JUST, FONT_WRAP, FONT_COLOUR = 0x101DA118, 0x101DA204, 0x101DA2D8, 0x101DA3AC
EXTRA, CLIENT_TEXT, CLIENT_DRAW, CAPTURE = 0x101DCFD0, 0x101DE130, 0x101DB624, 0x101DCCEC
CALLBACK, BUILD, CREATE, POSITION, HIGHEST_Z = 0x101DCBA8, 0x101DEB78, 0x101DC750, 0x101DB884, 0x101CA880
GET_WINDOW, SET_TEXT, SET_STATE = 0x101DBE40, 0x101DE248, 0x101CB1D0

# ---- the original's data
G = dict(panel=0x10394ABC, dialog=0x10394A90, text=0x10394A94, root=0x10395C24, screens=0x102E49B8)
SCRIPT, SCRIPT_WORDS = 0x102ABE96, 183
CREATE_PLAYER_MSG, OTHER_MSG = 0x10267CB8, 0x10267CC0   # the TVectors of the dialog's message function and of any other
VTABLE = 0x102A7D94            # the window's vtable (slot 0x2c is SetText: 0x101de248)
DRAW_VTABLE = 0x102AFA38       # the draw object's (slot 0x14 is Change: 0x101ccec8)

# ---- the world: labels
WINDOWS, DRAWS, FONTS, TEXTS, ROOT = 16, 16, 13, 8, 15
LAB_WIN, LAB_DRAW, LAB_FONT, LAB_TEXT = 10, 30, 50, 70
LAB_BUFFER, LAB_ADVISOR, LAB_SCREENS = 80, 81, 82
# the window block's offsets
O_NEXT, O_CHILD, O_ID, O_DRAW, O_CLIENT, O_Z, O_VTABLE, O_RECT, O_CURRENT = 0x50, 0x58, 0x88, 0x94, 0x98, 0xDC, 0x118, 0x2C, 0x138
WINDOW_SIZE = 0x140
# the wide texts of the world (the characters of text i)
TEXTS_SET = ["Create", "Anna", "Pick a name", "", "Easy", "Hard", "Title", "x" * 40]
MASK = 0xFFFFFFFF


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def w16(e, a, v):
    e.write(a, struct.pack(">h", T.s16(v)))


def blank():
    """A window with no draw objects and no relatives."""
    return dict(z=0, draw=-1, client=-1, child=-1, next=-1, id=0, current=-1, rect=(0, 0, 0, 0))


def world(parents, ids=None):
    """Sixteen blank windows linked by `parents` ({child: parent}, children in index order) and given the ids."""
    wins = [blank() for _ in range(WINDOWS)]
    last = {}
    for child in sorted(parents):
        parent = parents[child]
        if parent in last:
            wins[last[parent]]["next"] = child
        else:
            wins[parent]["child"] = child
        last[parent] = child
    for i, v in (ids or {}).items():
        wins[i]["id"] = v
    return wins


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures, the stubs and the dumps."""

    def __init__(self, e, case):
        self.e = e
        e.nn = self
        self.case = case
        self.win = [e.alloc(WINDOW_SIZE, bytes(WINDOW_SIZE)) for _ in range(WINDOWS)]
        self.draw = [e.alloc(0x3C, bytes(0x3C)) for _ in range(DRAWS)]
        self.font = [e.alloc(0x14, bytes(0x14)) for _ in range(FONTS)]
        self.text = [CL.new_wide(e, t) for t in TEXTS_SET]
        self.advisor = e.alloc(0x2C, bytes(0x2C))
        self.screens = e.alloc(0x20, bytes(0x20))
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in [(G["panel"], 4), (G["dialog"], 4), (G["text"], 36), (G["root"], 4),
                                                                (G["screens"], 4)]}
        for (a, n) in self.saved:
            e.write(a, bytes(n))
        labels = {self.win[i]: LAB_WIN + i for i in range(WINDOWS)}
        labels.update({self.draw[i]: LAB_DRAW + i for i in range(DRAWS)})
        labels.update({self.font[i]: LAB_FONT + i for i in range(FONTS)})
        labels.update({self.text[i]: LAB_TEXT + i for i in range(TEXTS)})
        labels.update({G["text"]: LAB_BUFFER, self.advisor: LAB_ADVISOR, self.screens: LAB_SCREENS})
        self.L = e.cl = CL.PpcLog(labels, case.get("q", {}))
        for d in self.draw:
            w32(e, d + 0x38, DRAW_VTABLE)
        for i, w in enumerate(case["wins"]):
            b = self.win[i]
            w32(e, b + O_VTABLE, VTABLE)
            w32(e, b + O_NEXT, self.at(w["next"]))
            w32(e, b + O_CHILD, self.at(w["child"]))
            w32(e, b + O_ID, w["id"])
            w32(e, b + O_DRAW, self.drw(w["draw"]))
            w32(e, b + O_CLIENT, self.drw(w["client"]))
            w16(e, b + O_Z, w["z"])
            e.write(b + O_RECT, struct.pack(">4h", *[T.s16(v) for v in w["rect"]]))
            w32(e, b + O_CURRENT, w["current"])
        w32(e, G["root"], self.win[ROOT])
        w32(e, G["screens"], self.screens)
        w32(e, self.screens + 0x14, self.advisor)
        panel = case.get("panel", -1)
        w32(e, G["panel"], self.at(panel))
        w32(e, G["dialog"], self.at(case.get("dialog", -1)))

    def at(self, i):
        return 0 if i < 0 else self.win[i]

    def drw(self, i):
        return 0 if i < 0 else self.draw[i]

    def index_of(self, address):
        """A window pointer as the index (-1 for NULL, -2 for any other pointer)."""
        if address == 0:
            return -1
        return self.win.index(address) if address in self.win else -2

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    def dump(self):
        """Every window's depth and current id, the dialog global and the name buffer."""
        e = self.e
        out = []
        for b in self.win:
            out.extend([struct.unpack(">h", e.read(b + O_Z, 2))[0], T.s32(e.u32(b + O_CURRENT))])
        out.append(self.index_of(e.u32(G["dialog"])))
        out.append(self.index_of(e.u32(G["panel"])))
        out.extend(e.read(G["text"], 36))
        return out


def code_of(address):
    """A message function's TVector as the C++ side's table numbers it."""
    return 0 if address == 0 else 20 if address == CREATE_PLAYER_MSG else 99


def wide(e, address):
    return CL.wide_text(e, address) if address else "(null)"


def stubs():
    """The PowerPC fakes: every callee of the five functions, each recording what the host's fake records."""
    st = CL.Stubs()
    R = CL.recorder
    st.real = {NEWNAME, DIALOG, SETZ, SETCURRENT, CLIENTRECT, SETCLIENTRECT, *COOLRECT}

    def lab(e, a):
        return e.cl.lab(a)

    def window_of(e, queue):
        n = e.cl.ret(queue)
        return e.nn.win[n - 1] if 1 <= n <= WINDOWS else 0

    def get_window(e):
        L = e.cl
        this, ident = e.r(3), T.s32(e.r(4))
        L.rec("GetWindow", [L.lab(this), ident])
        child = e.u32(this + O_CHILD)
        while child and T.s32(e.u32(child + O_ID)) != ident:
            child = e.u32(child + O_NEXT)
        e.set_r(3, child)

    def font_entry(e):
        index = e.r(3)
        e.cl.rec("GetFontTableEntry", [index])
        e.set_r(3, e.nn.font[index] if index < FONTS else 0)

    def get_string(e):
        ident = T.s32(e.r(3))
        e.cl.rec("UIMain_GetString", [ident])
        n = e.cl.ret("GetString:%d" % ident, 1 + (ident & 7))
        e.set_r(3, e.nn.text[n - 1] if 1 <= n <= TEXTS else 0)

    def text_stub(name):
        """A fake of (this, text): the window's label, the text's and its characters."""
        def stub(e):
            L = e.cl
            L.rec(name, [L.lab(e.r(3)), L.lab(e.r(4))], [wide(e, e.r(4))])
            e.set_r(3, 1)
        return stub

    def ctor(name):
        def stub(e):
            e.cl.rec(name, [e.cl.newlab(e.r(3)), int(e.r(4) != 0)])
        return stub

    def extra(e):
        L = e.cl
        L.rec("setExtraParam", [L.lab(e.r(3)), T.s16(e.r(4)), T.s32(e.r(5))])
        e.set_r(3, 1)

    def callback(e):
        e.cl.rec("SetCallbackFunction", [e.cl.lab(e.r(3)), code_of(e.r(4))])

    def build(e):
        L = e.cl
        desc = e.r(4)
        L.rec("Build", [L.lab(e.r(3)), 0 if desc == 0 else 1 if desc == SCRIPT else 2, code_of(e.r(5))])
        e.set_r(3, window_of(e, "Build"))

    def create(e):
        L = e.cl
        L.rec("CreateObject", [L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), T.s16(e.r(7)), T.s16(e.r(8)),
                               T.s16(e.r(9)), T.s16(e.r(10))])
        e.set_r(3, window_of(e, "CreateObject"))

    def position(e):
        L = e.cl
        rect = e.r(4)
        L.rec("GetPosition", [L.lab(e.r(3)), int(rect != 0), T.s16(e.r(5))])
        if rect:
            e.write(rect + 4, struct.pack(">4h", *[T.s16(L.ret("GetPosition")) for _ in range(4)]))
        e.set_r(3, 1 if rect else 0)

    def highest_z(e):
        L = e.cl
        rect = e.r(4)
        words = list(struct.unpack(">4h", e.read(rect + 4, 8))) if rect else [0, 0, 0, 0]
        L.rec("UI_GetHighestZ", [L.lab(e.r(3)), int(rect != 0)] + words)
        e.set_r(3, T.u32(L.ret("HighestZ")))

    def state(e):
        e.cl.rec("Button.SetState", [e.cl.lab(e.r(3)), e.r(4) & 0xFF, e.r(5) & 0xFF])
        e.set_r(3, 1)

    def change(e):
        flags = e.r(4)
        depth = struct.unpack(">h", e.read(e.r(5) + 6, 2))[0] if flags & 2 else 0
        e.cl.rec("Change", [e.cl.lab(e.r(3)), flags, depth])

    def find_draw(e):
        e.cl.rec("FindDrawObject", [], [e.cstr(e.r(3))])
        n = e.cl.ret("Find")
        e.set_r(3, e.nn.draw[n - 1] if 1 <= n <= DRAWS else 0)

    for addr, fn in [(ADVISOR_ADD, R("Advisor.Add", "Lii")), (FONT_ENTRY, font_entry), (GET_STRING, get_string),
                     (TITLE_COLOUR, R("UI_TITLE_COLOUR", "L")), (TEXT_CTOR, ctor("TextObject.ctor")),
                     (CURSOR_CTOR, ctor("CursorText.ctor")), (TITLE_CTOR, ctor("TitleText.ctor")), (FIND_DRAW, find_draw),
                     (DRAW_CHANGE, change), (STATIC_AREA, R("EditBox.SetStaticTextArea", "LLi")),
                     (CURSOR_DRAW, R("EditBox.SetCursorDrawObject", "LL")), (SELECT_ALL, R("EditBox.SelectAll", "L")),
                     (UNACCEPTED, text_stub("EditBox.SetUnAcceptedChars")), (SET_FONT, R("SetFont", "LL")),
                     (FONT_JUST, R("SetFontJust", "Lii")), (FONT_WRAP, R("SetFontWrap", "Lii")),
                     (FONT_COLOUR, R("SetFontColour", "Liiii")), (EXTRA, extra),
                     (CLIENT_TEXT, text_stub("SetClientDrawText")), (CLIENT_DRAW, R("SetClientDrawObject", "LL")),
                     (CAPTURE, R("CaptureKb", "Li")), (CALLBACK, callback), (BUILD, build), (CREATE, create),
                     (POSITION, position), (HIGHEST_Z, highest_z), (GET_WINDOW, get_window),
                     (SET_TEXT, text_stub("SetText")), (SET_STATE, state)]:
        st.internal(addr, fn)
    return st


STUBS = None


def run_ppc(e, case, address, args):
    """Builds the case's world, calls the function, and answers (r3, the log, the state dump) with the world restored."""
    global STUBS
    if STUBS is None:
        STUBS = stubs()
    f = Ppc(e, case)
    saved_heap = e.heap_in_scratch
    e.heap_in_scratch = True
    saved = STUBS.activate(e)
    try:
        r3 = T.call_ppc(e, address, [a(f) if callable(a) else a for a in args])[0]
        return f, T.s32(r3), e.cl.log, f.dump()
    finally:
        CL.Stubs.deactivate(e, saved)
        e.heap_in_scratch = saved_heap
        f.restore()


# ======================================================================= the host side


def load_host(lib):
    lib.tpw_nn_window.restype = lib.tpw_nn_draw.restype = lib.tpw_nn_font.restype = lib.tpw_nn_text.restype = C.c_void_p
    lib.tpw_nn_buffer.restype = lib.tpw_nn_advisor.restype = lib.tpw_nn_screens.restype = C.c_void_p
    lib.tpw_nn_window.argtypes = lib.tpw_nn_draw.argtypes = lib.tpw_nn_font.argtypes = lib.tpw_nn_text.argtypes = [C.c_int]
    lib.tpw_nn_settext.argtypes = [C.c_int, C.POINTER(C.c_uint16), C.c_int]
    lib.tpw_nn_setwin.argtypes = [C.c_int] * 8 + [C.POINTER(C.c_int16)]
    lib.tpw_nn_getwin.argtypes = [C.c_int, C.POINTER(C.c_int32)]
    lib.tpw_nn_globals.argtypes = [C.c_int, C.c_int]
    lib.tpw_nn_run.argtypes = [C.c_int, C.c_int64, C.c_int64, C.c_int64, C.POINTER(C.c_int32)]
    lib.tpw_nn_script.restype = C.POINTER(C.c_int16)


def run_host(lib, case, op, a=0, b=0, c=0, out=None):
    """The same world on the host, one call of the function `op`, and (the out words, the log, the state dump)."""
    load_host(lib)
    lib.tpw_nn_reset()
    fixed = {LAB_WIN + i: lib.tpw_nn_window(i) for i in range(WINDOWS)}
    fixed.update({LAB_DRAW + i: lib.tpw_nn_draw(i) for i in range(DRAWS)})
    fixed.update({LAB_FONT + i: lib.tpw_nn_font(i) for i in range(FONTS)})
    fixed.update({LAB_TEXT + i: lib.tpw_nn_text(i) for i in range(TEXTS)})
    fixed.update({LAB_BUFFER: lib.tpw_nn_buffer(), LAB_ADVISOR: lib.tpw_nn_advisor(), LAB_SCREENS: lib.tpw_nn_screens()})
    for i, t in enumerate(TEXTS_SET):
        chars = (C.c_uint16 * (len(t) + 1))(*[ord(ch) for ch in t])
        lib.tpw_nn_settext(i, chars, len(t))
    for i, w in enumerate(case["wins"]):
        rect = (C.c_int16 * 4)(*[T.s16(v) for v in w["rect"]])
        lib.tpw_nn_setwin(i, w["z"], w["draw"], w["client"], w["child"], w["next"], w["id"], w["current"], rect)
    lib.tpw_nn_globals(case.get("panel", -1), case.get("dialog", -1))
    words = (C.c_int32 * 8)(*(out or [0] * 8))
    CL.host_begin(lib, fixed, case.get("q", {}))
    lib.tpw_nn_run(op, a, b, c, words)
    log = CL.host_end(lib)
    dump = []
    cell = (C.c_int32 * 2)()
    for i in range(WINDOWS):
        lib.tpw_nn_getwin(i, cell)
        dump.extend([cell[0], cell[1]])
    lib.tpw_nn_dialog_index.restype = C.c_int
    dump.append(lib.tpw_nn_dialog_index())
    dump.append(case.get("panel", -1))
    text = (C.c_uint8 * 36).from_address(lib.tpw_nn_buffer())
    dump.extend(bytes(bytearray(text)))
    return list(words), log, dump


def result(r3, log, dump, extra=None):
    """The comparison record of a case: the answer, the log, the state and any extra words."""
    return dict(ret=r3, log=log, state=dump, extra=extra or [])


# ======================================================================= the tests

# ---- NewName


def name_tree(extra_child=None, extra_first=False, minus_child=False, current=-1):
    """The dialog as the script builds it: the holder 0 above the dialog 1; the dialog's buttons -2 and -1, the title, the name entry,
    a picture and the check buttons 7 (children 0x70d, 0x70e); optionally a further child of the check buttons with the id 0x7a17 (first or
    last) or -1, and its current id."""
    parents = {1: 0, 2: 1, 3: 1, 4: 1, 5: 1, 6: 1, 7: 1, 8: 7, 9: 7}
    ids = {1: 0x708, 2: -2, 3: -1, 4: 0x709, 5: 0x70A, 6: 0x70B, 7: 0x70C, 8: 0x70D, 9: 0x70E}
    if extra_child is not None:
        parents[10] = 7
        ids[10] = extra_child
    wins = world(parents, ids)
    if extra_first and extra_child is not None:
        # the extra child first among the check buttons' children: swap the links
        wins[7]["child"], wins[10]["next"], wins[9]["next"] = 10, 8, -1
    wins[7]["current"] = current
    return wins


def name_case(slot=1, panel=14, wins=None, hz=5, find=1, strings=None, position=(100, 200, 900, 800), holder=1, build=2):
    q = {"CreateObject": [holder], "Build": [build], "HighestZ": [hz], "Find": [find], "GetPosition": list(position)}
    for ident, n in (strings or {}).items():
        q["GetString:%d" % ident] = [n]
    return dict(wins=wins or name_tree(), panel=panel, dialog=-1, q=q, args=[slot])


def name_ppc(e, case):
    f, r3, log, dump = run_ppc(e, case, NEWNAME, case["args"])
    return result(0, log, dump)


def name_host(lib, case):
    words, log, dump = run_host(lib, case, 0, case["args"][0])
    return result(0, log, dump)


def name_edges():
    yield name_case()
    yield name_case(panel=-1)                            # no panel: nothing happens
    yield name_case(slot=0)
    yield name_case(slot=-1)
    yield name_case(slot=0x7FFFFFFF)
    yield name_case(slot=-0x80000000)
    yield name_case(find=0)                              # no "hilight" draw object
    yield name_case(hz=-1)
    yield name_case(hz=0x100)
    yield name_case(hz=0x7FFF)
    yield name_case(hz=0x7FFFFFFF)
    yield name_case(hz=-0x80000000)
    yield name_case(strings={0xF3: 0, 0xF1: 0, 0xF2: 0, 0xEF: 0})                    # no strings at all
    yield name_case(strings={0xF3: 3, 0xF1: 5, 0xF2: 6, 0xEF: 7})
    yield name_case(strings={0xF3: 8})
    # the check buttons with the child 0x7a17: SetCurrent selects it
    for current in (-1, 0x70D, 0x7A17, 0x70E, 99):
        yield name_case(wins=name_tree(extra_child=0x7A17, current=current))
        yield name_case(wins=name_tree(extra_child=0x7A17, extra_first=True, current=current))
        yield name_case(wins=name_tree(extra_child=-1, current=current))
        yield name_case(wins=name_tree(current=current))
    # depths and draw objects on the dialog's windows
    wins = name_tree()
    for i in range(10):
        wins[i].update(z=0x100 - i, draw=i, client=15 - i)
    yield name_case(wins=wins)
    wins = name_tree()
    for i in range(10):
        wins[i].update(z=0x7FF0 + i, draw=i if i % 2 else -1, client=-1 if i % 2 else i)
    yield name_case(wins=wins, hz=0x7FF0)


def name_rnd(r):
    kinds = [None, 0x7A17, -1, 0x70D, 5]
    wins = name_tree(extra_child=r.choice(kinds), extra_first=r.random() < 0.5, current=r.choice([-1, 0x70D, 0x70E, 0x7A17, 99, 0]))
    for i in range(10):
        wins[i].update(z=r.choice([0, 1, 5, 0xFE, 0x100, 0x101, -1, -300, r.randrange(-32768, 32768)]),
                       draw=r.choice([-1, r.randrange(DRAWS)]), client=r.choice([-1, r.randrange(DRAWS)]))
    strings = {i: r.randrange(0, 10) for i in (0xF3, 0xF1, 0xF2, 0xEF) if r.random() < 0.5}
    return name_case(slot=r.choice([0, 1, 2, 3, -1, r.randrange(-2**31, 2**31)]), panel=r.choice([-1, 14, 14, 14]), wins=wins,
                     hz=r.choice([0, 1, 255, 256, -1, r.randrange(-2**31, 2**31)]), find=r.choice([0, 1, r.randrange(0, 17)]),
                     strings=strings, position=tuple(r.randrange(-32768, 32768) for _ in range(4)))


# ---- Dialog


def forest(r, count=15, depth_bias=0.7):
    """Random parents for windows 1..count-1 (a parent has a lower index, so there is no cycle), 15 is the root with no relatives."""
    parents = {}
    for i in range(1, count):
        parents[i] = r.randrange(i) if r.random() < depth_bias else r.randrange(max(1, i))
    return parents


def dress(r, wins):
    """Random depths and draw objects."""
    for w in wins:
        w["z"] = r.choice([0, 1, 2, 0xFD, 0xFE, 0xFF, 0x100, 0x101, -1, -2, 0x7FFE, 0x7FFF, -0x8000, r.randrange(-32768, 32768)])
        w["draw"] = r.choice([-1, r.randrange(DRAWS)])
        w["client"] = r.choice([-1, r.randrange(DRAWS)])
    return wins


def dialog_case(desc=1, cb=1, modal=1, holder=1, build=2, hz=5, position=(10, 20, 30, 40), find=1, wins=None):
    q = {"CreateObject": [holder], "Build": [build], "HighestZ": [hz], "Find": [find], "GetPosition": list(position)}
    return dict(wins=wins or name_tree(), panel=-1, dialog=-1, q=q, args=[desc, cb, modal])


def dialog_ppc(e, case):
    desc, cb, modal = case["args"]
    args = [[0, SCRIPT, 0x10269999][desc], [0, CREATE_PLAYER_MSG, OTHER_MSG][cb], modal & MASK]
    f, r3, log, dump = run_ppc(e, case, DIALOG, args)
    return result(f.index_of(r3 & MASK), log, dump)


def dialog_host(lib, case):
    desc, cb, modal = case["args"]
    words, log, dump = run_host(lib, case, 1, desc, cb, modal)
    return result(words[0], log, dump)


def dialog_edges():
    yield dialog_case()
    for modal in (0, 1, 2, -1, 0x10000):
        for cb in (0, 1, 2):
            for desc in (0, 1, 2):
                yield dialog_case(desc, cb, modal)
    yield dialog_case(holder=0)                            # no holder: nothing else happens
    yield dialog_case(build=0)                             # the script made nothing
    yield dialog_case(build=0, modal=0)
    for hz in (-1, 0, 0xFF, 0x100, 0x7FFE, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, -0x80000000, 0x1FFFF):
        yield dialog_case(hz=hz)
        yield dialog_case(hz=hz, modal=0)
    yield dialog_case(find=0)
    yield dialog_case(find=0, modal=0)
    yield dialog_case(holder=3, build=3)                   # the dialog is the holder itself
    yield dialog_case(holder=16, build=1)                  # a queue answer past the pool is NULL
    for position in ((0, 0, 0, 0), (-1, -1, -1, -1), (32767, -32768, 32767, -32768), (1, 2, 3, 4)):
        yield dialog_case(modal=0, position=position)


def dialog_rnd(r):
    wins = dress(r, world(forest(r)))
    return dialog_case(desc=r.choice([0, 1, 1, 2]), cb=r.choice([0, 1, 2]), modal=r.choice([0, 1, 1, 2, -1, r.randrange(-2**31, 2**31)]),
                       holder=r.choice([0, 1 + r.randrange(15), 1 + r.randrange(15), 1 + r.randrange(15)]),
                       build=r.choice([0, 1 + r.randrange(15), 1 + r.randrange(15), 1 + r.randrange(15)]),
                       hz=r.choice([-1, 0, 5, 255, 256, 0x7FFF, 0xFFFF, r.randrange(-2**31, 2**31)]), find=r.choice([0, 1, 7]),
                       position=tuple(r.randrange(-32768, 32768) for _ in range(4)), wins=wins)


# ---- setZ


def setz_case(wins, window, z):
    return dict(wins=wins, panel=-1, dialog=-1, q={}, args=[window, z])


def setz_ppc(e, case):
    window, z = case["args"]
    f, r3, log, dump = run_ppc(e, case, SETZ, [lambda f: f.win[window], z & MASK])
    return result(0, log, dump)


def setz_host(lib, case):
    window, z = case["args"]
    words, log, dump = run_host(lib, case, 2, window, z)
    return result(0, log, dump)


def chain(depth):
    """A single line of windows 0 > 1 > 2 ... > depth-1."""
    return world({i: i - 1 for i in range(1, depth)})


def setz_edges():
    zs = [0, 1, -1, 5, 0xFD, 0xFE, 0xFF, 0x100, 0x101, 0x102, 0x7FF9, 0x7FFA, 0x7FFB, 0x7FFE, 0x7FFF, -0x8000, -0x7FFF, 0x2000]
    for depth in (1, 2, 3, 4, 5, 8, 12):
        for z in zs:
            wins = chain(depth)
            for w in wins[:depth]:
                w["draw"], w["client"] = 1, 2
            yield setz_case(wins, 0, z)
    # one draw object kind at a time, on every level of a chain of six
    for mask in range(1, 4):
        wins = chain(6)
        for i, w in enumerate(wins[:6]):
            w["draw"] = i if mask & 1 else -1
            w["client"] = 8 + i if mask & 2 else -1
            w["z"] = 7 * i
        yield setz_case(wins, 0, 0x100)
        yield setz_case(wins, 0, 0xFC)
    # a wide tree: the root's children, each with children, each with children and deeper
    parents = {i: 0 for i in range(1, 4)}
    parents.update({4: 1, 5: 1, 6: 2, 7: 3, 8: 4, 9: 4, 10: 6, 11: 8, 12: 11})
    wins = world(parents)
    for i in range(13):
        wins[i].update(draw=i, client=15 - i % 16, z=100 + i)
    yield setz_case(wins, 0, 40)
    yield setz_case(wins, 1, 0xFE)
    yield setz_case(wins, 4, 0x7FFE)
    yield setz_case(wins, 12, 3)
    # triage: a fan-out of fourteen children under the root, and a chain of fifteen windows (the deepest the fixture holds)
    for z in (0, 0xFE, 0x100, 0x7FFB):
        wins = world({i: 0 for i in range(1, 15)})
        for i in range(15):
            wins[i].update(draw=i, client=14 - i, z=3 * i)
        yield setz_case(wins, 0, z)
        wins = chain(15)
        for i in range(15):
            wins[i].update(draw=i, client=14 - i, z=3 * i)
        yield setz_case(wins, 0, z)
    # a window with a bad z: the children get the argument's, not the cut one
    wins = chain(4)
    wins[0]["draw"] = 0
    yield setz_case(wins, 0, 0x300)
    yield setz_case(wins, 0, 0x7FFF)


def setz_rnd(r):
    wins = dress(r, world(forest(r, depth_bias=r.choice([0.3, 0.8, 0.95]))))
    return setz_case(wins, r.randrange(15), r.choice([0, 1, 0xFE, 0xFF, 0x100, 0x101, -1, 0x7FFA, 0x7FFB, 0x7FFF, -0x8000,
                                                      r.randrange(-32768, 32768), r.randrange(-32768, 32768)]))


# ---- SetCurrent

CHECK_IDS = [0x70D, 0x70E, -1, -2, 0, 5, 0x7A17, 0x8000, -0x8000, 0x12345, 0x7FFFFFFF]


def current_case(ids, current, window=0):
    """Window `window` (the check buttons) with the children `ids` (windows 1.. in order), the current id and the argument in args."""
    parents = {i + 1: window for i in range(len(ids))}
    wins = world(parents, {i + 1: v for i, v in enumerate(ids)})
    wins[window]["current"] = current
    return dict(wins=wins, panel=-1, dialog=-1, q={}, args=[window])


def with_arg(case, ident):
    case = dict(case)
    case["args"] = [case["args"][0], ident]
    return case


def current_ppc(e, case):
    window, ident = case["args"]
    f, r3, log, dump = run_ppc(e, case, SETCURRENT, [lambda f: f.win[window], ident & MASK])
    return result(r3, log, dump)


def current_host(lib, case):
    window, ident = case["args"]
    words, log, dump = run_host(lib, case, 3, window, ident)
    return result(words[0], log, dump)


def current_edges():
    for current in (-1, 0x70D, 0x70E, 0x7A17, 99, -2, 0x12345, 0x8001, 0x18000):
        for ident in CHECK_IDS + [0x18000, 0x8001]:
            yield with_arg(current_case([0x70D, 0x70E, 0x7A17], current), ident)
    yield with_arg(current_case([], -1), 5)                        # no children at all
    yield with_arg(current_case([], -1), -1)
    yield with_arg(current_case([-1, 0x70D], -1), 0x70D)           # a child with the id -1 and the current id -1
    yield with_arg(current_case([-1, 0x70D], 0x70D), -1)
    yield with_arg(current_case([-1], -1), -1)
    yield with_arg(current_case([0x70D, 0x70D], 0x70D), 0x70D)     # two children with the same id
    yield with_arg(current_case([0x70D, 0x70D], -1), 0x70D)
    yield with_arg(current_case([0x70D, 0x70E], 0x70D), 0x70E)
    yield with_arg(current_case([0x70D, 0x70E], 0x10000), 0x70E)   # the current id is 0x10000 (a short would read 0)
    yield with_arg(current_case([0x70D, 0x70E, 0], 0x10000), 0x70E)
    yield with_arg(current_case([0x70D, 0x70E, 0], 0), 0x70D)
    yield with_arg(current_case([0x70D, 0x70E, 0, 0x7FFF], 0x7FFF), 0x70D)
    yield with_arg(current_case([0x70D, 0x70E, 0x8000], 0x8000), 0x70D)   # answers a negative short
    yield with_arg(current_case([0x70D, 0x70E, 0x18000], 0x18000), 0x70D)
    yield with_arg(current_case([3], 3, window=7), 3)


def current_rnd(r):
    ids = [r.choice(CHECK_IDS + [r.randrange(-2**31, 2**31)]) for _ in range(r.randrange(0, 6))]
    current = r.choice([-1, -1] + ids + [r.randrange(-2**31, 2**31), 0x10000 + r.choice(ids or [0])])
    ident = r.choice([-1, current] + ids + [r.randrange(-2**31, 2**31), 0x10000 + r.choice(ids or [0])])
    return with_arg(current_case(ids, current, window=r.randrange(0, 10 if len(ids) < 6 else 9)), ident)


# ---- GetClientRect


def rect_case(window, rect, out, present):
    wins = world({})
    wins[window]["rect"] = rect
    return dict(wins=wins, panel=-1, dialog=-1, q={}, args=[window, present, out])


def rect_ppc(e, case):
    window, present, out = case["args"]
    box = e.alloc(0x14, struct.pack(">I4h", 0xA5A5A5A5, *[T.s16(v) for v in out]) + b"\xa5" * 8)
    f, r3, log, dump = run_ppc(e, case, CLIENTRECT, [lambda f: f.win[window], box if present else 0])
    return result(r3, log, dump, list(struct.unpack(">4h", e.read(box + 4, 8))))


def rect_host(lib, case):
    window, present, out = case["args"]
    words, log, dump = run_host(lib, case, 4, window, present, 0, [0] + [T.s16(v) for v in out] + [0, 0, 0])
    return result(words[0], log, dump, words[1:5])


def rect_edges():
    for rect in [(0, 0, 0, 0), (1, 2, 3, 4), (-1, -2, -3, -4), (32767, -32768, 32767, -32768), (-32768, 32767, -32768, 32767),
                 (0, 0, 2048, 1536)]:
        for present in (1, 0):
            yield rect_case(3, rect, (7, 8, 9, 10), present)
    yield rect_case(0, (5, 6, 7, 8), (0, 0, 0, 0), 1)
    yield rect_case(15, (5, 6, 7, 8), (-1, -1, -1, -1), 1)


def rect_rnd(r):
    return rect_case(r.randrange(WINDOWS), tuple(r.randrange(-32768, 32768) for _ in range(4)),
                     tuple(r.randrange(-32768, 32768) for _ in range(4)), r.choice([0, 1, 1, 1]))


# ---- SetClientRect (unit v5L14): the same cases, the rectangle given and the window's client rectangle read back


def setrect_ppc(e, case):
    window, present, given = case["args"]
    box = e.alloc(0x14, struct.pack(">I4h", 0xA5A5A5A5, *[T.s16(v) for v in given]) + b"\xa5" * 8)
    f, r3, log, dump = run_ppc(e, case, SETCLIENTRECT, [lambda f: f.win[window], box if present else 0])
    return result(r3, log, dump, list(struct.unpack(">4h", e.read(f.win[window] + O_RECT, 8))))


def setrect_host(lib, case):
    window, present, given = case["args"]
    words, log, dump = run_host(lib, case, 5, window, present, 0, [0] + [T.s16(v) for v in given] + [0, 0, 0])
    return result(words[0], log, dump, words[1:5])


# ---- the dialog's window description


def script_ppc(e, case):
    return dict(words=list(struct.unpack(">%dh" % SCRIPT_WORDS, e.read(SCRIPT, 2 * SCRIPT_WORDS))))


def script_host(lib, case):
    load_host(lib)
    p = lib.tpw_nn_script()
    return dict(words=[p[i] for i in range(SCRIPT_WORDS)])


def setup(runner):
    T.install_std_stubs(runner.emu)


TESTS = [FnTest("NewName", name_ppc, name_host, edge=name_edges, rnd=name_rnd),
         FnTest("Dialog", dialog_ppc, dialog_host, edge=dialog_edges, rnd=dialog_rnd),
         FnTest("InterfaceWindow::setZ", setz_ppc, setz_host, edge=setz_edges, rnd=setz_rnd),
         FnTest("InterfaceCheckButtons::SetCurrent", current_ppc, current_host, edge=current_edges, rnd=current_rnd),
         FnTest("InterfaceWindow::GetClientRect", rect_ppc, rect_host, edge=rect_edges, rnd=rect_rnd),
         FnTest("InterfaceWindow::SetClientRect", setrect_ppc, setrect_host, edge=rect_edges, rnd=rect_rnd),
         FnTest("gNewNameWindowData", script_ppc, script_host, edge=lambda: [{}], rnd=lambda r: {})]
