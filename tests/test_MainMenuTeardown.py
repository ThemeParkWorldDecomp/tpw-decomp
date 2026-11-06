"""The main menu's whole life with the real bodies, against libtpw_unit_MainMenuTeardown.dylib (tests/unit_MainMenuTeardown.cpp), main app
("SimTheme Park"), src/game/ui/ui_mainmenu.cpp, UIMenu.cpp, InterfaceWindow.cpp, InterfaceMessageQueue.cpp, UI_Base.cpp, InterfaceStringHeap.cpp:
  HideMainMenu 0x101fc140, UIMenu::~UIMenu 0x1018ac8c, and under them the handlers MainGameMenuCB 0x101fae30, MainLobbyMenuCB 0x101fb354,
  MainMenuItemCB 0x101fa9dc, UIMenu_DefaultCallback 0x1018ab78, UIMenu_DefaultItemCallback 0x1018aa48, the default window handler 0x101de3f8
  (its message 4 deletes the window), ~InterfaceWindow 0x101d9ba4 with its whole tree, the queues and the string heap

A case is a script of steps that opens the menu, clicks and types at it, closes it and opens it again, the way the game does on every Esc. The
world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the original, and in the host
library's memory (little-endian) for the translation. Nothing between the click and the free is a fake. The fakes on both sides are what
draws (the draw objects, a real window owns and deletes them), the help text, the text measure, the font table, the colour and font setters,
SetText, the panels and dialogs the handlers open, the online system, the key table, the sound, the highest z and the pointer update.

After every step the dump holds: gMainMenu, the pause byte, gGame's word +0x3c, every window pointer the UI keeps (a window that went must be
gone from all of them), the queues (an entry of a window that went is invalidated and keeps the dead pointer, shown as -2), the heap's rows
(the items' copies come back), the menu's words and every live window's links, attributes, z, id, rectangle, draw objects and first three extra
parameters. The event log holds every delete of a labelled block, so a window, title text, menu or row that is not freed, or freed twice,
shows. The blocks are labelled: root 1, menu 2, menu window 3, bystander 4, font 5, lolight 6, gGame 7, online 8, key table 9, items 10 up, title
texts from calllog's first new label, heap rows 50 up (table A) and 70 up (table B), help 40. A freed block is never read on the host.

The unit puts the arena on the plain allocator, so every window's free is logged and is a real free. Run the module with
DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib to catch a use after the free on the host. Not tested: the real draw objects' destructors
(tests/test_UITextObjects.py), the messages 0x1e
(itemOnFrame, which the port paces), 0x10001 and 0x10002 (hover, they end in the cursor and help text fakes; tests/test_UIMenu_DefaultItemCallback.py),
the menu items that reach gLobbyScreens, a pumped draw message on a live window (it draws), the dialogs' answers, and CreateMainMenu (tests/test_HideMainMenu.py).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_MainMenuTeardown"
MASK = 0xFFFFFFFF
INVALID = 0x80808080
ENTRIES, MAX_ROWS, TEXT_HALVES = 32, 20, 0x20
ROOT, MENU, WINDOW, BYSTANDER, FONT, LOLIGHT, GGAME, ONLINE, KEYTABLE, HELP = 1, 2, 3, 4, 5, 6, 7, 8, 9, 40
ITEM, ROW_A, ROW_B = 10, 50, 70

FN_HIDE, FN_DTOR, FN_GAME, FN_LOBBY, FN_ITEMCB = 0x101FC140, 0x1018AC8C, 0x101FAE30, 0x101FB354, 0x101FA9DC
FN_CTOR, FN_INIT, FN_ADD, FN_SHOW, FN_POST, FN_SEND_POST = 0x1018AC44, 0x1018AD20, 0x1018AE78, 0x1018B0E0, 0x101C7D7C, 0x101C9400
WINDOW_CTOR, C_LINK = 0x101D98D8, 0x101DDF20

ADDR = dict(pointer_in=0x10395C1C, capture=0x10395C14, focus=0x10395C20, keyboard=0x10395C10, key_keys=0x10395C08, key_window=0x10395C0C,
            last_window=0x10395C18, button0=0x10395BC4, button1=0x10395BC8, button2=0x10395BCC, root=0x10395C24, menu=0x10396A58,
            flag=0x10396A54, gGame=0x1037F134, sys=0x10380AB0, keytable=0x1037F220, width=0x10395BBC, height=0x10395BBA,
            TableB=0x10395C34, TableA=0x10395C38, RowsB=0x10395C3C, WidthB=0x10395C40, RowsA=0x10395C44, WidthA=0x10395C48,
            retflag=0x102E4860)
QUEUES = dict(msg=0x10395C04, draw=0x10395C00, timed=0x10395BFC, timers=0x10395C54, timer_count=0x10395C50, timer_list=0x10395C58,
              help=0x102AF928, peek=0x10395BF8)
WATCHED = ("pointer_in", "capture", "focus", "keyboard", "key_window", "button0", "button1", "button2")
MAC_LAYOUT = dict(destroying=0, attr=0x44, next=0x50, prev=0x54, child=0x58, parent=0x5C, draw=0x94, client=0x98, extra=0x9C, z=0xDC, id=0x88,
                  rect=8, m04=4, shown=0xC, window=0x10, q_count=4, q_head=8, q_tail=0xC, q_queued=0x10, msg_size=16, f3c=0x3C)
HOST_NAMES = ("destroying", "attr", "next", "prev", "child", "parent", "draw", "client", "extra", "z", "id", "rect", "m04", "shown", "window",
              "q_count", "q_head", "q_tail", "q_queued", "msg_size", "f3c", "rect_left")
HOST_LAYOUT = {}


def glue_save(e):
    """The import glue saves the caller's TOC at 0x14(r1) (the caller reloads r2 from there)."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def st_new(e):
    e.set_r(3, e.alloc(e.r(3)))


def st_delete(e):
    L = e.cl
    obj = L.lab(e.r(3))
    if obj > 0:
        L.rec("delete", (obj,))
    L.forget(e.r(3))


def st_wcslen(e):
    glue_save(e)
    a, n = e.r(3), 0
    while e.read(a + 2 * n, 2) != b"\0\0":
        n += 1
    e.set_r(3, n)


def st_wcscpy(e):
    glue_save(e)
    dst, src, n = e.r(3), e.r(4), 0
    while True:
        ch = e.read(src + 2 * n, 2)
        e.write(dst + 2 * n, ch)
        n += 1
        if ch == b"\0\0":
            break


def st_rand(e):
    glue_save(e)
    e.set_r(3, 7)


def st_measure(e):
    rect = struct.unpack(">4i", e.read(e.r(4), 16))
    e.cl.rec("MeasureText", (rect[2], rect[3], T.s8(e.r(5))))
    e.write(e.r(9), struct.pack(">4i", 0, 0, 99, 19))


def st_title(e):
    e.cl.rec("TitleCtor", (e.cl.newlab(e.r(3)),))
    e.write(e.r(3) + 0x38, struct.pack(">I", e.mt_draw_vt))


def st_find(e):
    e.cl.rec("FindDrawObject", (1 if e.cstr(e.r(3)) == "lolight" else 0,))
    e.set_r(3, e.mt_lolight)


def st_get_font(e):
    e.cl.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, e.mt_font)


def st_search(e):
    e.cl.rec("Search", (T.s16(e.r(4)), T.s16(e.r(5))))
    e.set_r(3, e.cl.ret("Search") & MASK)


def st_answer(name, answer):
    def stub(e):
        e.cl.rec(name, ())
        e.set_r(3, answer)
    return stub


def st_string(e):
    e.set_r(3, e.mt_string)


def st_nothing(e):
    pass


STUBS = CL.Stubs()
STUBS.real.update({FN_HIDE, FN_DTOR, FN_GAME, FN_LOBBY, FN_ITEMCB, FN_CTOR, FN_INIT, FN_ADD, FN_SHOW, FN_POST, FN_SEND_POST, WINDOW_CTOR, C_LINK})
STUBS.imp("__nw__FUl", st_new)
STUBS.imp("__dl__FPv", st_delete)
STUBS.internal(0x102389C8, st_wcslen)
STUBS.internal(0x10238C80, st_wcscpy)
STUBS.internal(0x10239148, st_rand)
STUBS.internal(0x1018B8E4, st_measure)
STUBS.internal(0x1018FBE8, st_title)
STUBS.internal(0x101CD2D4, st_find)
STUBS.internal(0x10180270, st_get_font)
STUBS.internal(0x1014FD68, st_search)
STUBS.internal(0x101D987C, st_nothing)                     # CalcClientArea
STUBS.internal(0x10180468, st_string)                      # UIMain_GetString
STUBS.internal(0x101DA118, CL.recorder("SetFont", "LL"))
STUBS.internal(0x101DA204, CL.recorder("SetFontJust", "Lii"))
STUBS.internal(0x101DA3AC, CL.recorder("SetFontColour", "Liiii"))
STUBS.internal(0x101DE248, CL.recorder("SetText", "L"))
STUBS.internal(0x101802CC, CL.recorder("UISound", "i"))
STUBS.internal(0x101499D0, CL.recorder("Unpause", "L"))
STUBS.internal(0x101A37AC, CL.recorder("CreateLoadPanel", "i"))
STUBS.internal(0x101A3994, CL.recorder("CreateSavePanel", "i"))
STUBS.internal(0x10236E6C, CL.recorder("GS_SetCurrentGameStatus", "i"))
for _addr, _name, _answer in ((0x101C8F18, "UpdatePointerIn", 1), (0x101CA880, "UI_GetHighestZ", 10), (0x10180514, "UIMain_CheckPanel", 0),
                              (0x1018116C, "UIMain_DecideKbCapture", 0), (0x101D8BA0, "UI_UnPauseTimerSystem", 0),
                              (0x101A9FB8, "CreateOptionsPanel", 0), (0x10187220, "UI_ByeByeBankrupt", 0), (0x101AE9E0, "CreateLoginPanel", 0),
                              (0x101F1800, "UI_DialogBox", 0), (0x10171498, "AreWeProcessing", 0), (0x10171398, "Logout", 0),
                              (0x101710AC, "Login", 0), (0x10171404, "StopProcessing", 0)):
    STUBS.internal(_addr, st_answer(_name, _answer))


def make_fakes(e):
    """The draw objects' and the help text's tables and the handlers' TVectors, in persistent memory."""
    if hasattr(e, "mt_draw_vt"):
        return

    def dtor(emu):
        L, this, flag = emu.cl, emu.r(3), T.s16(emu.r(4))
        L.rec("Draw.dtor", (L.lab(this),))
        if flag > 0:
            if L.lab(this) > 0:
                L.rec("delete", (L.lab(this),))
            L.forget(this)

    def change(emu):
        frame, depth = struct.unpack(">hh", emu.read(emu.r(5) + 4, 4))
        emu.cl.rec("Draw.Change", (emu.cl.lab(emu.r(3)), emu.r(4), frame if emu.r(4) & 1 else 0, depth if emu.r(4) & 2 else 0))

    def show(emu):
        emu.cl.rec("Draw.Show", (emu.cl.lab(emu.r(3)), T.s16(emu.r(4))))

    def on_hide_or_delete(emu):
        emu.cl.rec("Help.OnHideOrDelete", (emu.cl.lab(emu.r(3)), emu.cl.lab(emu.r(4))))

    vt = T.persist_alloc(e, 0x30)
    e.write(vt + 0x08, struct.pack(">I", T.fake_function(e, dtor)))
    e.write(vt + 0x14, struct.pack(">I", T.fake_function(e, change)))
    e.write(vt + 0x1C, struct.pack(">I", T.fake_function(e, show)))
    e.mt_draw_vt = vt
    vt = T.persist_alloc(e, 0x50)
    e.write(vt + 0x3C, struct.pack(">I", T.fake_function(e, on_hide_or_delete)))
    e.mt_help_vt = vt
    e.mt_tv = {}
    for code in (FN_GAME, FN_LOBBY, FN_ITEMCB):
        tv = T.persist_alloc(e, 8)
        e.write(tv, struct.pack(">II", code, e.toc))
        e.mt_tv[code] = tv
    e.mt_string = T.persist_alloc(e, 4, struct.pack(">HH", 0x53, 0))


def setup(runner):
    T.install_std_stubs(runner.emu)
    make_fakes(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(int(w & MASK).to_bytes(4, self.order) for w in ws))

    def word(self, a):
        return int.from_bytes(self.read(a, 4), self.order)

    def half(self, a):
        return T.s16(int.from_bytes(self.read(a, 2), self.order))

    def ptr(self, a):
        return int.from_bytes(self.read(a, self.ps), self.order)

    def set_ptr(self, a, value):
        self.write(a, int(value).to_bytes(self.ps, self.order))

    def halves(self, a, hs):
        self.write(a, b"".join(int(h & 0xFFFF).to_bytes(2, self.order) for h in hs))

    def rd_halves(self, a, n):
        data = self.read(a, 2 * n)
        return [int.from_bytes(data[2 * k:2 * k + 2], self.order) for k in range(n)]

    def alive(self, label):
        a = self.windows.get(label, 0)
        return a != 0 and self.lab(a) == label

    def track(self, label, a):
        self.windows[label] = a
        self.set_label(label, a)


class Ppc(Side):
    order = "big"
    ps = 4
    w = MAC_LAYOUT

    def __init__(self, e, case):
        self.e = e
        self.windows = {}
        e.mt_lolight = e.alloc(0x3C)
        e.mt_font = e.alloc(0x10)
        self.help = e.alloc(0x18)
        self.online, self.keytable = e.alloc(0x10), e.alloc(0x10)
        e.write(e.mt_lolight + 0x38, struct.pack(">I", e.mt_draw_vt))
        e.write(e.mt_lolight + 0xC, struct.pack(">h", 1))
        e.write(self.help + 0x10, struct.pack(">I", e.mt_help_vt))
        self.saved = STUBS.activate(e)
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.queue_blocks = [e.alloc(0x14), e.alloc(0x14)]
        self.entries = [e.alloc(ENTRIES * 16), e.alloc(ENTRIES * 16)]
        for k in range(2):
            self.words(self.queue_blocks[k], [self.entries[k], ENTRIES, ENTRIES - 1, ENTRIES - 1, 0])
        self.set_ptr(QUEUES["msg"], self.queue_blocks[0])
        self.set_ptr(QUEUES["draw"], self.queue_blocks[1])
        timed = e.alloc(8)
        self.words(timed, [4, e.alloc(4 * 0x14)])
        self.set_ptr(QUEUES["timed"], timed)
        for name in ("timers", "timer_count", "timer_list", "peek"):
            self.set_ptr(QUEUES[name], 0)
        self.set_ptr(QUEUES["help"], self.help)
        for name in WATCHED + ("key_keys", "last_window", "menu"):
            self.set_ptr(ADDR[name], 0)
        self.set_ptr(ADDR["sys"], self.online)
        self.set_ptr(ADDR["keytable"], self.keytable)
        self.words(ADDR["retflag"], [0])
        self.root = e.alloc(0x138)
        self.call(WINDOW_CTOR, self.root, 0, 1, 0, 0, 0, 0x7FF, 0x5FF)
        self.set_ptr(ADDR["root"], self.root)
        L = self.L
        L.log.clear()
        L.labels = {self.help: HELP, e.mt_lolight: LOLIGHT, e.mt_font: FONT, ADDR["gGame"]: GGAME, self.online: ONLINE, self.keytable: KEYTABLE}
        self.track(ROOT, self.root)

    def call(self, code, *args):
        return T.call_ppc(self.e, code, [a & MASK for a in args])[0]

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def queue(self, k):
        return self.queue_blocks[k]

    def queue_entries(self, k):
        return self.entries[k]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def lab(self, a):
        return self.L.lab(a)

    def set_label(self, label, a):
        self.L.labels[a] = label

    def log(self):
        return self.L.log

    def new_menu(self):
        menu = self.e.alloc(0x14)
        self.call(FN_CTOR, menu)
        return menu

    def new_window(self):
        window = self.e.alloc(0x138)
        self.call(WINDOW_CTOR, window, 0, 1, 77, 100, 100, 200, 200)
        self.call(C_LINK, self.root, window, 1, 1)
        return window

    def init(self, menu, lobby):
        self.call(FN_INIT, menu, self.e.mt_tv[FN_LOBBY if lobby else FN_GAME], self.e.mt_tv[FN_ITEMCB])

    def add(self, menu, position, text, id_, a, b, c, d):
        return T.s32(self.call(FN_ADD, menu, position, text, id_, a, b, c, d))

    def show(self, menu, a):
        self.call(FN_SHOW, menu, a)

    def hide(self):
        self.call(FN_HIDE)

    def destruct(self, menu, flag):
        self.call(FN_DTOR, menu, flag)

    def post(self, window, message, a, b):
        return T.s32(self.call(FN_SEND_POST, window, message, a, b))

    def pump(self, k):
        return T.s16(self.call(FN_POST, self.queue_blocks[k]))


class Host(Side):
    order = "little"
    ps = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.w = HOST_LAYOUT
        self.windows = {}
        lib.tpw_mt_reset()
        self.root = self.ptr(self.g("root"))
        CL.host_begin(lib, {HELP: lib.tpw_mt_fixed(2), LOLIGHT: lib.tpw_mt_fixed(0), FONT: lib.tpw_mt_fixed(1), GGAME: self.g("gGame"),
                            ONLINE: lib.tpw_mt_fixed(3), KEYTABLE: lib.tpw_mt_fixed(4)}, case["queues"])
        self.track(ROOT, self.root)

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_mt_addr(name.encode())

    def queue(self, k):
        return self.g("queue" if k == 0 else "draw_queue")

    def queue_entries(self, k):
        return self.lib.tpw_mt_entries(k)

    def alloc(self, n):
        return self.lib.tpw_mt_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def lab(self, a):
        return self.lib.tpw_cl_lab(a)

    def set_label(self, label, a):
        self.lib.tpw_cl_label(label, a)

    def log(self):
        return CL.host_end(self.lib)

    def new_menu(self):
        return self.lib.tpw_mt_new_menu()

    def new_window(self):
        window = self.lib.tpw_mt_new_window(0, 1, 77, 100, 100, 200, 200)
        self.lib.tpw_mt_link(window)
        return window

    def init(self, menu, lobby):
        self.lib.tpw_mt_init(menu, 1 if lobby else 0)

    def add(self, menu, position, text, id_, a, b, c, d):
        return self.lib.tpw_mt_add(menu, position, text, id_, a, b, c, d)

    def show(self, menu, a):
        self.lib.tpw_mt_show(menu, a)

    def hide(self):
        self.lib.tpw_mt_hide()

    def destruct(self, menu, flag):
        self.lib.tpw_mt_destruct(menu, flag)

    def post(self, window, message, a, b):
        return self.lib.tpw_mt_post(window, message, a, b)

    def pump(self, k):
        return self.lib.tpw_mt_pump(k)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32 = C.c_uint64, C.c_int32
    lib.tpw_mt_addr.restype, lib.tpw_mt_addr.argtypes = u64, [C.c_char_p]
    lib.tpw_mt_layout.restype, lib.tpw_mt_layout.argtypes = i32, [C.POINTER(C.c_int64)]
    lib.tpw_mt_alloc.restype, lib.tpw_mt_alloc.argtypes = u64, [C.c_uint32]
    lib.tpw_mt_entries.restype, lib.tpw_mt_entries.argtypes = u64, [i32]
    lib.tpw_mt_reset.restype, lib.tpw_mt_reset.argtypes = None, []
    lib.tpw_mt_fixed.restype, lib.tpw_mt_fixed.argtypes = u64, [i32]
    lib.tpw_mt_new_menu.restype, lib.tpw_mt_new_menu.argtypes = u64, []
    lib.tpw_mt_new_window.restype, lib.tpw_mt_new_window.argtypes = u64, [u64] + [i32] * 6
    lib.tpw_mt_init.restype, lib.tpw_mt_init.argtypes = None, [u64, i32]
    lib.tpw_mt_add.restype, lib.tpw_mt_add.argtypes = i32, [u64, i32, u64] + [i32] * 5
    lib.tpw_mt_show.restype, lib.tpw_mt_show.argtypes = None, [u64, i32]
    lib.tpw_mt_hide.restype, lib.tpw_mt_hide.argtypes = None, []
    lib.tpw_mt_destruct.restype, lib.tpw_mt_destruct.argtypes = None, [u64, i32]
    lib.tpw_mt_post.restype, lib.tpw_mt_post.argtypes = i32, [u64] + [i32] * 3
    lib.tpw_mt_pump.restype, lib.tpw_mt_pump.argtypes = i32, [i32]
    lib.tpw_mt_link.restype, lib.tpw_mt_link.argtypes = None, [u64]
    lib.tpw_cl_lab.restype, lib.tpw_cl_lab.argtypes = i32, [C.c_void_p]
    out = (C.c_int64 * 32)()
    n = lib.tpw_mt_layout(out)
    HOST_LAYOUT.update(zip(HOST_NAMES, list(out)[:n]))
    HOST_LAYOUT["rect"] += HOST_LAYOUT.pop("rect_left")
    lib.bound = True


def build(s, case):
    """The heap's tables, the screen and the pause state; the rest is each side's constructor."""
    s.write(s.g("width"), (640).to_bytes(2, s.order))
    s.write(s.g("height"), (480).to_bytes(2, s.order))
    s.table_a, s.table_b = s.alloc(2 * case["RowsA"] * case["WidthA"]), s.alloc(2 * case["RowsB"] * case["WidthB"])
    for table, rows, width, first in ((s.table_a, case["RowsA"], case["WidthA"], ROW_A), (s.table_b, case["RowsB"], case["WidthB"], ROW_B)):
        s.halves(table, [0xFFFF] * (rows * width))
        for k in range(rows):
            s.set_label(first + k, table + 2 * k * width)
    for name, value in (("RowsA", case["RowsA"]), ("WidthA", case["WidthA"]), ("RowsB", case["RowsB"]), ("WidthB", case["WidthB"])):
        s.words(s.g(name), [value])
    s.set_ptr(s.g("TableA"), s.table_a)
    s.set_ptr(s.g("TableB"), s.table_b)
    s.write(s.g("flag"), bytes([case["flag"]]))
    s.words(s.g("gGame") + s.w["f3c"], [case["f3c"]])
    s.menu = 0
    s.track(BYSTANDER, s.new_window())


def pointer_label(s, a):
    return s.lab(s.ptr(a))


def dump(s, case):
    d = {"menu": pointer_label(s, s.g("menu")), "flag": s.read(s.g("flag"), 1), "f3c": s.word(s.g("gGame") + s.w["f3c"]),
         "retflag": s.word(s.g("retflag")), "keys": s.ptr(s.g("key_keys")) != 0}
    for name in WATCHED:
        d[name] = pointer_label(s, s.g(name))
    if s.menu and s.lab(s.menu) == MENU:
        d["menu_words"] = (T.s32(s.word(s.menu + s.w["m04"])), T.s32(s.word(s.menu + s.w["shown"])), pointer_label(s, s.menu + s.w["window"]))
    for k in range(2):
        q = s.queue(k)
        entries = s.queue_entries(k)
        size = s.w["msg_size"]
        rows = []
        for n in range(ENTRIES):
            at = entries + n * size
            window = s.ptr(at)
            message, a, b = (s.word(at + s.ps + 4 * j) for j in range(3))
            if window == 0:
                rows.append(0)
            elif INVALID in (message, a, b):
                rows.append((-2, message, a, b))
            else:
                rows.append((s.lab(window), message, a, b))
        d["queue%d" % k] = (s.word(q + s.w["q_count"]), s.word(q + s.w["q_head"]), s.word(q + s.w["q_tail"]), s.word(q + s.w["q_queued"]), rows)
    d["heap"] = (s.rd_halves(s.table_a, case["RowsA"] * case["WidthA"]), s.rd_halves(s.table_b, case["RowsB"] * case["WidthB"]))
    live = {}
    w = s.w
    for label in sorted(s.windows):
        if not s.alive(label):
            continue
        a = s.windows[label]
        extra = [s.ptr(a + w["extra"] + s.ps * k) for k in range(3)]
        live[label] = (s.read(a + w["destroying"], 1), s.word(a + w["attr"]), s.half(a + w["z"]), T.s32(s.word(a + w["id"])),
                       [s.half(a + w["rect"] + 2 * k) for k in range(4)],
                       [pointer_label(s, a + w[f]) for f in ("next", "prev", "child", "parent", "draw", "client")],
                       s.lab(extra[0]), s.lab(extra[1]), extra[2] & MASK)
    d["windows"] = live
    return d


def resolve(s, target):
    return s.windows.get(target, 0) if target in s.windows and s.alive(target) else 0


def execute(s, case):
    """Every step's answer and the dump after it. A step at a window that has gone does nothing, on both sides."""
    out = []
    for step in case["script"]:
        op = step[0]
        answer = None
        if op == "open":
            _, items, show, lobby = step
            if s.ptr(s.g("menu")):      # the last cycle's menu is still up (a captured keyboard took its Esc); the game never opens over one
                s.hide()
            s.menu = s.new_menu()
            s.set_label(MENU, s.menu)
            s.set_ptr(s.g("menu"), s.menu)
            s.init(s.menu, lobby)
            s.track(WINDOW, s.ptr(s.menu + s.w["window"]))
            answer = []
            for k, (id_, text, font, red, green, blue) in enumerate(items):
                block = s.alloc(2 * TEXT_HALVES)
                s.halves(block, [0x41 + j % 26 for j in range(text)] + [0])
                answer.append(s.add(s.menu, 60 + 30 * k, block, id_, font, red, green, blue))
                s.track(ITEM + k, s.ptr(s.g("last_window")))
            if show is not None:
                s.show(s.menu, show)
        elif op == "capture":
            a = resolve(s, step[2])
            if a:
                s.set_ptr(s.g(step[1]), a)
        elif op == "post":
            a = 1 if step[1] == "key" else resolve(s, step[1])
            if a:
                answer = s.post(a, *step[2:])
        elif op == "pump":
            answer = []
            while len(answer) < 2 * ENTRIES:
                answer.append(s.pump(step[1]))
                if answer[-1] == 0:
                    break
        elif op == "hide":
            s.hide()
        elif op == "destruct":
            s.destruct(s.menu if step[1] else 0, step[2])
            s.set_ptr(s.g("menu"), 0)
        out.append((step[0], answer, dump(s, case)))
    return out


def run(s, case):
    try:
        build(s, case)
        steps = execute(s, case)
        return {"steps": steps, "log": s.log()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# The items a case can click. Each closes the menu, or opens a dialog and leaves it up.
GAME_CLOSES = (0, 1, 2, 5, 6, 7, 16, 19, 21, 22, 24)
GAME_STAYS = (3, 4, 8, 9, 15, 17, 18, 20, 23, 25, 26, 1000)
LOBBY_CLOSES = (9, 11, 14)
LOBBY_STAYS = (8, 12, 15, 16, 1000)
CAPTURES = ("pointer_in", "capture", "focus", "keyboard", "button0", "button1", "button2")
TEXTS = (0, 1, 3, 4, 5, 7, 8, 9, 20)


def items_of(r, ids):
    return [(id_, r.choice(TEXTS), r.choice([0, 1, 5]), r.randrange(256), r.randrange(256), r.randrange(256)) for id_ in ids]


def noise(r, count):
    """Messages queued at the menu's windows and at the window that stays."""
    steps = []
    for _ in range(r.randrange(0, 4)):
        target = r.choice([WINDOW, BYSTANDER] + [ITEM + k for k in range(count)])
        steps.append(("post", target, r.choice([0x7777, 0x11]) if target >= ITEM else 0x7777, r.randrange(0, 5), r.randrange(0, 5)))
    return steps


def captures(r, count):
    steps = []
    for _ in range(r.randrange(0, 3)):
        steps.append(("capture", r.choice(CAPTURES), r.choice([WINDOW, BYSTANDER] + [ITEM + k for k in range(count)])))
    return steps


def click_close(r, lobby, closing):
    """Open, a click at the closing item with more messages behind it, what is left of the queue."""
    closes = LOBBY_CLOSES if lobby else GAME_CLOSES
    id_ = closing if closing is not None else r.choice(closes)
    stays = LOBBY_STAYS if lobby else GAME_STAYS
    ids = [r.choice(stays) for _ in range(r.randrange(0, 4))]
    at = r.randrange(0, len(ids) + 1)
    ids.insert(at, id_)
    steps = [("open", items_of(r, ids), r.choice([0, 1]), lobby)] + captures(r, len(ids))
    if r.random() < 0.5:
        steps.append(("post", ITEM + at, 1, 0, 0))
    steps += [("post", ITEM + at, 0x10006, 0, 0)] + noise(r, len(ids)) + [("pump", 0), ("pump", 1)]
    return steps


def key_close(r, lobby, key, search):
    """Open and a key: Esc closes, another key closes when the key table answers 0."""
    ids = [r.choice(LOBBY_STAYS if lobby else GAME_STAYS) for _ in range(r.randrange(0, 4))]
    steps = [("open", items_of(r, ids), r.choice([0, 1]), lobby)] + captures(r, len(ids))
    steps += noise(r, len(ids)) + [("post", "key", 0x1000b, key, r.choice([0, 0x10000, 0x20000]))] + noise(r, len(ids)) + [("pump", 0)]
    steps += [("hide",), ("pump", 0), ("pump", 1)] if key != 0x1B and search != 0 else [("pump", 1)]
    return steps


def stays_up(r, lobby):
    """A click that leaves the menu up, then the Esc path by a direct hide."""
    ids = [r.choice(LOBBY_STAYS if lobby else GAME_STAYS) for _ in range(r.randrange(1, 5))]
    steps = [("open", items_of(r, ids), r.choice([0, 1]), lobby)] + captures(r, len(ids))
    steps += [("post", ITEM + r.randrange(len(ids)), 0x10006, 0, 0)] + noise(r, len(ids)) + [("pump", 0)]
    return steps + [("hide",), ("pump", 0), ("pump", 1)]


def direct(r, lobby, menu=True, flag=1, show=True):
    """The destructor itself, whatever the flag, and a menu that was never shown."""
    if not menu:
        return [("destruct", False, flag)]
    ids = [r.choice(LOBBY_STAYS if lobby else GAME_STAYS) for _ in range(r.randrange(0, 4))]
    steps = [("open", items_of(r, ids), r.choice([0, 1]) if show else None, lobby)] + captures(r, len(ids)) + noise(r, len(ids))
    return steps + [("destruct", True, flag), ("pump", 0)]


def base(r, cycles, search=None):
    width_b = r.choice([5, 6, 8])
    width_a = width_b + r.choice([1, 4, 8])
    script = []
    for cycle in cycles:
        script += cycle
    return {"script": script, "queues": {"Search": [r.choice([0, 1, -1]) if search is None else search]}, "RowsA": r.choice([0, 1, 3, 6]),
            "WidthA": width_a, "RowsB": r.choice([0, 1, 3, 6]), "WidthB": width_b, "flag": r.choice([0, 1, 2]), "f3c": r.choice([0, 1, 2])}


def edges():
    r = random.Random(5305)
    for id_ in GAME_CLOSES:
        yield base(r, [click_close(r, False, id_), click_close(r, False, id_)])
    for id_ in LOBBY_CLOSES:
        yield base(r, [click_close(r, True, id_), click_close(r, True, id_)])
    for lobby in (False, True):
        yield base(r, [key_close(r, lobby, 0x1B, 1), key_close(r, lobby, 0x1B, 1), key_close(r, lobby, 0x1B, 1)], 1)
        yield base(r, [key_close(r, lobby, 0x41, 0), key_close(r, lobby, 0x41, 0)], 0)
        yield base(r, [key_close(r, lobby, 0x41, 1), key_close(r, lobby, 0x41, 1)], 1)
        yield base(r, [stays_up(r, lobby), stays_up(r, lobby)])
        for flag in (1, 0, -1):
            yield base(r, [direct(r, lobby, True, flag), direct(r, lobby, True, flag)])
        yield base(r, [direct(r, lobby, True, 1, False)])
    yield base(r, [direct(r, False, False, 1)])


def rnd(r):
    cycles = []
    for _ in range(r.randrange(1, 4)):
        lobby = r.random() < 0.3
        kind = r.randrange(4)
        if kind == 0:
            cycles.append(click_close(r, lobby, None))
        elif kind == 1:
            cycles.append(key_close(r, lobby, 0x1B, 1))
        elif kind == 2:
            cycles.append(stays_up(r, lobby))
        else:
            cycles.append(direct(r, lobby, True, r.choice([1, 0, -1]), r.random() < 0.7))
    return base(r, cycles, 1)


TESTS = [
    FnTest("the main menu's life", ppc, host, edge=edges, rnd=rnd),
]
