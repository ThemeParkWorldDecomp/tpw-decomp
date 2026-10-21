"""Differential tests for the island lobby's controller, the global master save and the ride names (unit w32), against
libtpw_unit_IslandLobby.dylib (tests/unit_IslandLobby.cpp):

  src/game/ui/ui_islandLobbyController.cpp, src/game/ui/ui_onlineOutbox.cpp          CreateIslandLobbyController, ShowIslandLobbyController, UpdateIslandLobbyUI,
                                  UpdateIslandLobbyKeyDisplay, EnableIslandLobbyButtons, UpdateOutboxMailIcon
  src/game/ui/InterfaceWindow.cpp   InterfaceWindow::SetFrame
  src/game/save/FS_MasterSave.cpp    CFSGlobalMasterSave: the constructor, ResetGlobalData, HowManyKeys, GetInfoOn, CreateNewTheme;
                                  CFSGlobalParkData: the constructor, HowManyTickets, KeyCostToEnter, CheckBalanceFile,
                                  GetFullName; CGlobalParkDataBalance's two virtual members
  src/game/rides/RideDB.cpp, src/game/save/SaveLoad.cpp            CRideNameDB::GetName, GetNameA, GetNameB; CRideDatabase::GetTypeName(id, buffer, size)

A case is a world and a script. The world: the controller's nine windows (each with a draw object and a client draw object), the
lobby window and its mail icon, four fonts, eight islands and the lobby's screen system, eight themes whose balance files the
world scripts (Load's answer, the key cost and the gate object's number), the ride database (six ride types with names of three
kinds), the ride name table's strings (the real ones, read from the game's OBJECT_NAMES string table when it is there, with a
case's own strings over them) and the globals (the game type, the outbox word, the master save and user info made or not). A
script is a list of operations that call the unit's functions or change the world (the numbers are in OPS below). After every
operation the test compares the answer (a return value or the output buffer, whole, so that a write past the size shows), the
calls the fakes recorded during it, and the state: the controller's globals, the master save with its theme map in order, the
park records, the name builder's buffers, the ride database's cache and the converted-text buffer. At the end it compares
operator new's and delete's counts.

Real on both sides: the unit's functions, the master save's container code (the Metrowerks list, set and map instances run as the
original's on the PowerPC side, the standard containers on the host), the rectangles, the C library's 16-bit string functions on the
host. Fakes that record the same event on both sides: the window toolkit, the draw objects, the text objects' constructors, the
message sender, the font table, the particle emitters, the balance table, the trace output, the user info's constructor and the
string table's FormatString. The PowerPC side decides which operations the state allows (a NULL pointer the original would
read is skipped on both sides).
"""
import ctypes as C
import pathlib
import re
import struct

import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_IslandLobby"

MINUS = 0xFFFFFFFF
ROOT = pathlib.Path(__file__).resolve().parent.parent.parent

# ---- the unit's functions
FN = dict(create=0x101E0F68, show=0x101E1264, update_ui=0x101E15B4, keys=0x101E188C, enable=0x101E1D40, mail=0x101E3698,
          set_frame=0x101DB7DC, ms_ctor=0x101694DC, ms_reset=0x10169A88, ms_keys=0x10169EA0, ms_getinfo=0x1016B47C,
          ms_create=0x1016B5A0, pd_ctor=0x1016BBC4, pd_tickets=0x1016C0E0, pd_cost=0x1016C3BC, pd_check=0x1016C43C,
          pd_fullname=0x1016C57C, bal_desc=0x1016CCD8, bal_start=0x1016CD2C, type_name=0x10154AF0, get_name=0x101570C4,
          name_a=0x10157250, name_b=0x1015734C)
# the real helpers below them: the rectangle's constructor and destructor and the Metrowerks container code
REAL_HELPERS = [0x101C7448, 0x101C74E0, 0x10152DB0, 0x10153B24, 0x10153DEC, 0x10153EFC, 0x1016CF68, 0x1016D63C, 0x1016E0B8,
                0x1016E37C, 0x1016EAF0, 0x1016EB98, 0x1016F080]
# the fakes (in-binary functions)
A = dict(build=0x101DEB78, get_window=0x101DBE40, show=0x101DC1EC, get_position=0x101DB884, set_draw=0x101DB624, set_font=0x101DA118,
         set_text_obj=0x101DE130, font_just=0x101DA204, font_colour=0x101DA3AC, font_entry=0x10180270, text_sprite=0x1018D11C,
         title_text=0x1018FBE8, send=0x101C9518, init_emitter=0x100BEC30, kill_emitter=0x100BE1E8, bal_init=0x1001EA28,
         bal_uninit=0x1001ED40, bal_load=0x10020134, ctrace=0x1006FCBC, user_ctor=0x1017DE54, type_ctor=0x1016F5D8)
DESCRIPTION, CALLBACK_TV = 0x102B006E, 0x102684A8
MISSING_STRING = 0x102A5CE2        # the wide string "MISSING STRING"
# the globals
G = dict(lobby=0x102E4888, screens=0x102E49B8, ctrl=0x10395CBC, emitter=0x10395CB4, keys_shown=0x10395CB8, master=0x10380A8C,
         user=0x10380B34, game_type=0x102B3AA0, game_guard=0x102B3AA4, ride_db=0x1037F6C8, text_table=0x1037FF18 + 0x59C,
         convert=0x10380C48, scratch=0x10380A4A)
SAVED = [(G["lobby"], 4), (G["screens"], 4), (G["ctrl"], 4), (G["emitter"], 4), (G["keys_shown"], 4), (G["master"], 4),
         (G["user"], 4), (G["game_type"], 8), (G["ride_db"], 0x738), (G["text_table"], 4), (G["convert"], 0x8000),
         (G["scratch"], 0x42)]

# ---- the world (the same as the C++ file's)
# (the first four are the game's themes: their global.sam files give these key costs and gate objects)
THEMES = ["hallow", "space", "jungle", "fantasy", "Hollywood", "alpha", "space2", "Alpha"]
THEME_BALANCE = [(1, 1, 2601), (1, 5, 3601), (1, 1, 1601), (1, 3, 4601), (1, 2, 1150), (1, 2, 1150), (1, 0, 1190), (1, 2, 1180)]
SCRATCH = len(THEMES)
ISLAND_TITLES = ["Spooky", "Space Zone", "Jungle", "Hollywood", "Fantasy", "Alpha Isle", "", "A long island title"]
SIGNS = ["", "Spooky", "Park-", "Hollow-", "Big-", "Fun Land", "a", "Twenty-nine characters long tex",
         "0123456789012345678901234567890-", "-"]
W_CTRL, W_KEYCOST, W_DIGIT, W_COUNT, W_ICON, W_TEXT, W_NAME, W_BTN_A, W_BTN_B, W_MAIL, W_LOBBY = range(11)
WINDOWS = 11
WIN_IDS = [0, 0x1E0EA, 0x1E0EB, 0x1E0EC, 0x1E0ED, 0x1E0EE, 0x1E0EF, 0x1E0F0, 0x1E0F1, 0xBF432, 0]
LAB_WINDOW, LAB_DRAW, LAB_FONT, LAB_ISLAND, LAB_TABLE = 10, 30, 60, 70, 90
RIDES = 6
BUFFER_UNITS = 0x90
FIRST_NEW = 100
EMITTER_FIRST = 7

# the ride database of a case: (number, kind of name, text); the kinds: 0 none, 1 "MISSING STRING", 2 the text
DEFAULT_RIDES = [(1150, 0, ""), (1190, 2, "Custom Coaster"), (1180, 1, ""), (1100, 2, ""), (1500, 0, ""),
                 (1108, 2, "A long ride name " + "x" * 100)]
BUFFER_SIZES = [0, 1, 2, 3, 0x10, 0x40, 0x41, 0x7F, 0x80, 0x81, 0x90]

# the operations (the C++ file's numbers)
OP = dict(create=1, show=2, update_ui=3, keys=4, enable=5, mail=6, set_frame=7, ms_new=10, ms_reset=11, ms_keys=12, ms_getinfo=13,
          ms_create=14, pd_new=15, pd=16, name_a=20, name_b=21, name=22, type_name=23, bal_row=24, game_type=30, current=31, screens=32,
          outbox=33, balance=34, master_null=35, user_null=36, globals=37, park_tickets=38, sign=39, keys_shown=40, emitter=41,
          rect=42, window_draw=43, lobby_window=44, ctrl_null=45, guard=46)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MINUS))


def w16(e, a, v):
    e.write(a, struct.pack(">H", v & 0xFFFF))


def wide_bytes(text, units=None):
    data = b"".join(struct.pack(">H", ord(ch)) for ch in text) + b"\0\0"
    if units is not None:
        data = data.ljust(2 * units, b"\0")
    return data


def wtext(e, a):
    """A wide text as the recorders write it ("(null)" for NULL)."""
    return "(null)" if a == 0 else CL.narrow(FS.wide_read(e, a))


def codes(text, n):
    """The first n code units of a text, zero after its end."""
    return [ord(c) for c in text[:n]] + [0] * (n - len(text[:n]))


# ======================================================================= the real strings

_REAL = {}


def real_strings():
    """The game's OBJECT_NAMES string table (374 strings, by number), read from the extracted disc; {} when it is not there."""
    if "strings" in _REAL:
        return _REAL["strings"]
    base = ROOT / "extracted/disc-hfs/Theme Park Data/data/Language/American"
    result = {}
    try:
        mb = (base / "MBToUni.dat").read_bytes()
        count = struct.unpack("<H", mb[6:8])[0]
        table = struct.unpack("<%dH" % count, mb[8:8 + 2 * count])
        data = (base / "OBJECT_NAMES.str").read_bytes()
        n = struct.unpack("<I", data[8:12])[0]
        offsets = struct.unpack("<%dI" % n, data[12:12 + 4 * n])
        for i in range(n):
            p = 12 + offsets[i]
            word = struct.unpack("<I", data[p:p + 4])[0]
            if word & 0xFF == 1:
                raw = data[p + 4:p + 4 + (word >> 8)]
                result[i] = "".join(chr(table[b - 1]) if 0 < b <= count else "?" for b in raw)
    except (OSError, struct.error):
        result = {}
    _REAL["strings"] = result
    return result


def base_strings():
    """Every string number the name table uses: the real text, or a made-up one."""
    real = real_strings()
    strings = {0: "", 1: ""}
    for row in NAME_TABLE:
        for sid in row[1:]:
            strings.setdefault(sid, real.get(sid, "Name %d" % sid))
    return strings


# the ride name table (the original's data at 0x102a5d3c): (ride type, string A, string B)
NAME_TABLE = []


def load_name_table(e):
    base = e.u32(0x1026153C)
    NAME_TABLE[:] = []
    for i in range(281):
        NAME_TABLE.append(struct.unpack(">3I", e.read(base + 12 * i, 12)))


# ======================================================================= the PowerPC side's fakes


def make_fakes(e):
    """Persistent fake vtables: a window's Enable and SetText, a draw object's Change."""
    if hasattr(e, "il_window_vtable"):
        return

    def enable(emu):
        emu.cl.rec("Window.Enable", (emu.cl.lab(emu.r(3)), T.s16(emu.r(4))))

    def set_text(emu):
        emu.cl.rec("Window.SetText", (emu.cl.lab(emu.r(3)),), (wtext(emu, emu.r(4)),))
        emu.set_r(3, 1)

    vt = T.persist_alloc(e, 0x40)
    w32(e, vt + 0x1C, T.fake_function(e, enable))
    w32(e, vt + 0x2C, T.fake_function(e, set_text))
    e.il_window_vtable = vt

    def change(emu):
        this, flags, record = emu.r(3), emu.r(4), emu.r(5)
        frame = struct.unpack(">h", emu.read(record + 4, 2))[0] if flags & 1 else 0
        emu.cl.rec("Change", (emu.cl.lab(this), flags, frame))

    vt = T.persist_alloc(e, 0x30)
    w32(e, vt + 0x14, T.fake_function(e, change))
    e.il_draw_vtable = vt


_FORMAT = re.compile(r"%([-+ #0]*)(\d+|\*)?(?:\.(\d*|\*))?(?:l|h)*([sSdiuxXc%])")


def wformat(e, fmt, first_reg):
    """swprintf's formatting for the fake: %ls and %s (wide strings), %d %i %u %x %X, %c and %%, flags, width and precision."""
    slot = [first_reg - 3]

    def arg():
        k = slot[0]
        slot[0] += 1
        return e.r(3 + k) if k < 8 else e.u32(e.r(1) + 24 + 4 * k)

    def one(m):
        flags, width, precision, conversion = m.groups()
        if conversion == "%":
            return "%"
        if width == "*":
            width = T.s32(arg())
            if width < 0:
                flags += "-"
                width = -width
            width = str(width)
        if precision == "*":
            precision = str(T.s32(arg()))
        if conversion in "sS":
            pointer = arg()
            text = "(null)" if pointer == 0 else FS.wide_read(e, pointer, 4000)
            if precision not in (None, "") and int(precision) >= 0:
                text = text[:int(precision)]
            elif precision == "":
                text = ""
        elif conversion == "c":
            text = chr(arg() & 0xFFFF)
        else:
            value = arg()
            spec = "%" + flags + (width or "") + ("." + precision if precision is not None else "")
            if conversion in "di":
                return (spec + "d") % T.s32(value)
            return (spec + {"u": "d", "x": "x", "X": "X"}[conversion]) % T.u32(value)
        pad = int(width or 0) - len(text)
        if pad > 0:
            text = text + " " * pad if "-" in flags else " " * pad + text
        return text
    return _FORMAT.sub(one, fmt)


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN.values())
    S.real.update(REAL_HELPERS)
    FS.string_stubs(S)
    R = CL.recorder
    U = lambda e: e.us   # noqa: E731  (the case's world)

    # the window toolkit
    def build(e):
        L = e.cl
        L.rec("Window.Build", (L.lab(e.r(3)), int(e.r(4) == DESCRIPTION), int(e.r(5) == CALLBACK_TV)))
        e.set_r(3, U(e).windows[W_CTRL])

    def get_window(e):
        L = e.cl
        wid = T.s32(e.r(4))
        L.rec("Window.GetWindow", (L.lab(e.r(3)), wid))
        found = [i for i in range(WINDOWS) if WIN_IDS[i] == wid and wid != 0]
        e.set_r(3, U(e).windows[found[0]] if found else 0)

    def get_position(e):
        L = e.cl
        L.rec("Window.GetPosition", (L.lab(e.r(3)), T.s16(e.r(5))))
        rect = e.r(4)
        e.write(rect + 4, struct.pack(">4h", 3, U(e).top, 9, U(e).bottom))
        e.set_r(3, 1)

    def font_entry(e):
        L = e.cl
        index = e.r(3)
        L.rec("GetFontTableEntry", (index,))
        e.set_r(3, U(e).fonts[index] if index < 4 else 0)

    def text_object(name):
        def stub(e):
            L = e.cl
            L.rec(name, (L.newlab(e.r(3)), int(e.r(4) != 0)))
        return stub

    def init_emitter(e):
        L = e.cl
        L.rec("PAR_InitEmitter", (T.s32(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))
        us = U(e)
        e.set_r(3, us.next_emitter)
        us.next_emitter += 1

    def ctrace(e):
        L = e.cl
        fmt = e.cstr(e.r(4))
        if "%s" in fmt:
            L.rec("CTRACE", (e.r(3),), (fmt[:159], e.cstr(e.r(5))[:159]))
        else:
            L.rec("CTRACE", (e.r(3),), (fmt[:159],))

    def bal_load(e):
        L = e.cl
        this, path = e.r(3), e.cstr(e.r(4))
        L.rec("Balance.Load", (L.lab(this), T.s32(e.r(5))), (path[:159],))
        found = re.fullmatch(r"data:levels:(.*):global\.sam", path)
        theme = THEMES.index(found.group(1)) if found and found.group(1) in THEMES else -1
        balance = U(e).balance[theme] if theme >= 0 else (0, 0, 0)
        if balance[0] == 0:
            e.set_r(3, 0)
            return
        w32(e, this + 0x14, balance[1])
        w32(e, this + 0x18, balance[2])
        e.set_r(3, 1)

    def user_ctor(e):
        e.cl.rec("CFSUserInfo.ctor", (e.cl.newlab(e.r(3)),))
        w32(e, e.r(3) + 0x8C, U(e).outbox)

    S.internal(A["build"], build)
    S.internal(A["get_window"], get_window)
    S.internal(A["show"], R("Window.Show", "Lh", ret=lambda e, L: 1))
    S.internal(A["get_position"], get_position)
    S.internal(A["set_draw"], R("Window.SetClientDrawObject", "LL"))
    S.internal(A["set_font"], R("Window.SetFont", "LL"))
    S.internal(A["set_text_obj"], R("Window.SetClientDrawText", "LS"))
    S.internal(A["font_just"], R("Window.SetFontJust", "Lii"))
    S.internal(A["font_colour"], R("Window.SetFontColour", "Liiii"))
    S.internal(A["font_entry"], font_entry)
    S.internal(A["text_sprite"], text_object("TextSprite.ctor"))
    S.internal(A["title_text"], text_object("TitleText.ctor"))
    S.internal(A["send"], R("UI_SendMessage", "Liii", ret=lambda e, L: 0))
    S.internal(A["init_emitter"], init_emitter)
    S.internal(A["kill_emitter"], R("PAR_KillEmitter", "i"))
    S.internal(A["bal_init"], lambda e: e.cl.rec("Balance.Init", (e.cl.newlab(e.r(3)),)))
    S.internal(A["bal_uninit"], R("Balance.UnInit", "L"))
    S.internal(A["bal_load"], bal_load)
    S.internal(A["ctrace"], ctrace)
    S.internal(A["user_ctor"], user_ctor)
    S.internal(A["type_ctor"], lambda e: w32(e, e.r(3), U(e).game_type))

    # the C library: the 16-bit string functions and swprintf
    def wcslen(e):
        e.set_r(3, len(FS.wide_read(e, e.r(3), 4000)))

    def wcsncpy(e):
        dst, src, count = e.r(3), e.r(4), e.r(5)
        text = FS.wide_read(e, src, 4000)[:count]
        e.write(dst, wide_bytes(text)[:-2].ljust(2 * count, b"\0") if count else b"")
        e.set_r(3, dst)

    def swprintf(e):
        out, size, fmt = e.r(3), e.r(4), FS.wide_read(e, e.r(5), 4000)
        text = wformat(e, fmt, 6)
        if size:
            kept = text[:size - 1]
            e.write(out, wide_bytes(kept))
        e.set_r(3, T.u32(len(text) if len(text) <= size - 1 and size else -1))

    def format_string(e):
        L = e.cl
        this, out, size, sid = e.r(3), e.r(4), e.r(5), e.r(6)
        L.rec("FormatString", (L.lab(this), size, sid))
        text = U(e).strings.get(sid, "") if sid < 2048 else U(e).strings[0]
        if size == 0:
            e.set_r(3, 0)
            return
        kept = text[:size - 1]
        e.write(out, wide_bytes(kept))
        e.set_r(3, len(kept))

    S.imp("wcslen", wcslen)
    S.imp("wcsncpy", wcsncpy)
    S.imp("swprintf", swprintf)
    S.imp("FormatString__8TbITableCFPwUiUle", format_string)
    S.imp("__lt__15TbStringBase<c>CFRC15TbStringBase<c>",
          lambda e: e.set_r(3, int(FS.s_read(e, e.r(3), False) < FS.s_read(e, e.r(4), False))))

    # the heap: counted per case; a delete of a labelled object is an event
    def op_new(e):
        us = U(e)
        block = e.alloc(max(e.r(3), 4), b"\xa5" * max(e.r(3), 4))
        us.heap[0] += 1
        e.set_r(3, block)

    def op_delete(e):
        L, us = e.cl, U(e)
        block = e.r(3)
        if block:
            us.heap[2] += 1
            if L.lab(block) > 0:
                L.rec("delete", (L.lab(block),))
                L.forget(block)

    S.imp("__nw__FUl", op_new)
    S.imp("__dl__FPv", op_delete)
    return S


# ======================================================================= the PowerPC side's world


class Ppc:
    """One case on the PowerPC side: the world and the operations."""

    def __init__(self, e, case):
        self.e = e
        e.us = self
        self.case = case
        self.heap = [0, 0, 0, 0]
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in SAVED}
        self.next_emitter = EMITTER_FIRST
        self.top, self.bottom = 10, 40
        self.outbox = 0
        self.game_type = 0
        self.balance = list(THEME_BALANCE)
        self.strings = dict(base_strings())
        self.strings.update(case.get("strings", {}))
        self.windows = []
        self.draws = []
        for i in range(WINDOWS):
            window = e.alloc(0x138, b"\xa5" * 0x138)
            w32(e, window + 0x118, e.il_window_vtable)
            pair = [0, 0]
            if i < W_MAIL:
                for k in range(2):
                    pair[k] = e.alloc(0x3C, bytes(0x3C))
                    w32(e, pair[k] + 0x38, e.il_draw_vtable)
            w32(e, window + 0x94, pair[0])
            w32(e, window + 0x98, pair[1])
            self.windows.append(window)
            self.draws.append(pair)
        self.fonts = [e.alloc(0x10) for _ in range(4)]
        self.table = e.alloc(16)
        self.lobby = e.alloc(0x38)
        self.screens = e.alloc(0x1C)
        self.theme_screen = e.alloc(0xE8)
        self.islands = []
        for i in range(len(THEMES)):
            island = e.alloc(0x74)
            FS.s_write(e, island + 0x28, THEMES[i], False)
            FS.s_write(e, island + 0x48, ISLAND_TITLES[i], True)
            self.islands.append(island)
        self.rides = []
        rides = case.get("rides", DEFAULT_RIDES)
        for i, (rid, kind, text) in enumerate(rides):
            block = e.alloc(0x800)
            w16(e, block, rid)
            name = 0 if kind == 0 else MISSING_STRING if kind == 1 else e.alloc(len(wide_bytes(text)), wide_bytes(text))
            w32(e, block + 0x7F0, name)
            self.rides.append(block)
        fixed = {}
        for i in range(WINDOWS):
            fixed[self.windows[i]] = LAB_WINDOW + i
            if i < W_MAIL:
                fixed[self.draws[i][0]] = LAB_DRAW + 2 * i
                fixed[self.draws[i][1]] = LAB_DRAW + 2 * i + 1
        for j, font in enumerate(self.fonts):
            fixed[font] = LAB_FONT + j
        for k, island in enumerate(self.islands):
            fixed[island] = LAB_ISLAND + k
        fixed[self.table] = LAB_TABLE
        self.L = e.cl = CL.PpcLog(fixed, {}, FIRST_NEW)
        # the globals as the host's reset leaves them
        w32(e, self.lobby + 4, self.windows[W_LOBBY])
        w32(e, G["lobby"], self.lobby)
        w32(e, self.screens + 4, self.theme_screen)
        w32(e, G["screens"], self.screens)
        w32(e, self.theme_screen + 0xC, 0)
        for key in ("ctrl", "emitter", "keys_shown", "master", "user"):
            w32(e, G[key], 0)
        w32(e, G["game_type"], 0)
        e.write(G["game_guard"], b"\x01")
        e.write(G["ride_db"], bytes(0x738))
        for i, block in enumerate(self.rides):
            w32(e, G["ride_db"] + 4 * i, block)
        w32(e, G["ride_db"] + 0x528, len(self.rides))
        w32(e, G["text_table"], self.table)
        e.write(G["convert"], bytes(0x8000))
        self.scratch_park = 0
        self.buffer = e.alloc(2 * BUFFER_UNITS)

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    # ---- helpers
    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def master(self):
        return self.e.u32(G["master"])

    def ctrl(self):
        return self.e.u32(G["ctrl"])

    def tree(self):
        """The master save's theme map in order: [(name, record)]."""
        e, out = self.e, []

        def walk(node):
            if node:
                walk(e.u32(node))
                out.append((FS.s_read(e, node + 0xC, False), e.u32(node + 0x20)))
                walk(e.u32(node + 4))
        if self.master():
            walk(e.u32(self.master() + 0x38))
        return out

    def park(self, which):
        if which == SCRATCH:
            return self.scratch_park
        if not 0 <= which < len(THEMES) or not self.master():
            return 0
        for name, record in self.tree():
            if name == THEMES[which]:
                return record
        return 0

    def name_string(self, text):
        return FS.new_string(self.e, text, False)

    def fill_buffer(self):
        self.e.write(self.buffer, b"\xaa\xaa" * BUFFER_UNITS)

    def read_buffer(self):
        return list(struct.unpack(">%dH" % BUFFER_UNITS, self.e.read(self.buffer, 2 * BUFFER_UNITS)))

    def new_object(self, size):
        self.heap[0] += 1
        return self.e.alloc(size, b"\xa5" * size)

    # ---- the dump (the C++ file's order)
    def park_fields(self, park):
        e = self.e
        if park == 0:
            return [0]
        v = [1] + list(e.read(park, 6)) + list(e.read(park + 6, 4))
        v += [int(e.u32(park + 0x1C) != 0), e.read(park + 0xB8, 1)[0], e.read(park + 0xB9, 1)[0]]
        v += codes(e.cstr(e.u32(park + 0x28)), 12)
        v += codes(FS.wide_read(e, park + 0x34), 12) + codes(FS.wide_read(e, park + 0x76), 12)
        return v

    def dump_state(self):
        e = self.e
        v = [int(self.ctrl() != 0), T.s32(e.u32(G["emitter"])), T.s32(e.u32(G["keys_shown"])), int(self.master() != 0),
             int(e.u32(G["user"]) != 0)]
        master = self.master()
        if master:
            v += list(e.read(master + 0x18, 4)) + [T.s32(e.u32(master + 0x1C)), T.s32(e.u32(master + 0x20)),
                                                   e.read(master + 0x24, 1)[0], e.read(master + 0x25, 1)[0],
                                                   e.read(master + 0x26, 1)[0], e.read(master + 0x27, 1)[0],
                                                   e.read(master + 0x48, 1)[0], e.u32(master + 0x34)]
            for name, record in self.tree():
                v += codes(name, 12) + self.park_fields(record)
        v += self.park_fields(self.scratch_park)
        db = G["ride_db"] + 0x52C
        for offset in (0, 0x80, 0x100):
            v += codes(FS.wide_read(e, db + offset), 24)
        cached = e.u32(G["ride_db"] + 0x734)
        v += [struct.unpack(">H", e.read(G["ride_db"] + 0x730, 2))[0], self.rides.index(cached) if cached in self.rides else -1]
        v += codes(FS.wide_read(e, G["convert"]), 40)
        return v

    # ---- the operations: the answer, or None when the state does not allow it
    def op(self, code, a):
        e = self.e
        if code in (OP["create"], OP["show"], OP["update_ui"], OP["keys"], OP["enable"], OP["mail"]):
            return self.controller_op(code, a)
        if code == OP["set_frame"]:
            self.call(FN["set_frame"], self.windows[a[0]], a[1])
            return []
        if code == OP["ms_new"]:
            block = self.new_object(0x4C)
            self.call(FN["ms_ctor"], block)
            w32(e, G["master"], block)
            return []
        if code in (OP["ms_reset"], OP["ms_keys"], OP["ms_getinfo"], OP["ms_create"]) and not self.master():
            return None
        if code == OP["ms_reset"]:
            self.call(FN["ms_reset"], self.master())
            return []
        if code == OP["ms_keys"]:
            return [T.s32(self.call(FN["ms_keys"], self.master()))]
        if code in (OP["ms_getinfo"], OP["ms_create"]):
            name = self.name_string(THEMES[a[0]])
            park = self.call(FN["ms_getinfo" if code == OP["ms_getinfo"] else "ms_create"], self.master(), name)
            return [int(park == 0), int(park != 0 and park == self.park(a[0]))]
        if code == OP["pd_new"]:
            block = self.new_object(0xBC)
            self.call(FN["pd_ctor"], block, self.name_string(THEMES[a[0]]))
            self.scratch_park = block
            return []
        if code == OP["pd"]:
            park = self.park(a[0])
            if park == 0:
                return None
            if a[1] == 0:
                return [T.s32(self.call(FN["pd_tickets"], park))]
            if a[1] == 1:
                return [T.s32(self.call(FN["pd_cost"], park))]
            if a[1] == 2:
                return [int(self.call(FN["pd_check"], park) & 0xFF != 0)]
            self.fill_buffer()
            self.call(FN["pd_fullname"], park, self.buffer, a[2])
            return self.read_buffer()
        if code == OP["bal_row"]:
            table = self.new_object(0x1C)
            self.L.rec("Balance.Init", (self.L.newlab(table),))
            row = self.call(FN["bal_desc"], table, a[0])
            start = self.call(FN["bal_start"], table)
            kind, name = e.u32(row), e.cstr(row + 4)
            fields = struct.unpack(">iiIIii", e.read(row + 0x24, 24))
            return [kind] + codes(name, 24) + list(fields) + [int(start == table + 8)]
        if code in (OP["name_a"], OP["name_b"], OP["name"], OP["type_name"]):
            self.fill_buffer()
            db = G["ride_db"] + 0x52C
            if code == OP["name_a"]:
                r = [int(self.call(FN["name_a"], db, a[0], self.buffer, a[1], a[2]) & 0xFF != 0)]
            elif code == OP["name_b"]:
                r = [int(self.call(FN["name_b"], db, a[0], self.buffer, a[1]) & 0xFF != 0)]
            elif code == OP["name"]:
                r = [T.s32(self.call(FN["get_name"], db, a[0], self.buffer, a[1])) & 0xFF != 0 and 1 or 0]
            else:
                # the id goes in the high half of the register; the low half is whatever the caller had
                self.call(FN["type_name"], G["ride_db"], ((a[0] & 0xFFFF) << 16) | 0xA5A5, self.buffer, a[1])
                r = []
            return r + self.read_buffer()
        return self.setter(code, a)

    def controller_op(self, code, a):
        has_window = self.e.u32(self.lobby + 4) != 0
        if code == OP["create"]:
            self.call(FN["create"])
        elif code == OP["show"]:
            if not has_window:
                return None
            self.call(FN["show"])
        elif code == OP["update_ui"]:
            if self.ctrl():
                screens = self.e.u32(G["screens"])
                theme = self.e.u32(screens + 4) if screens else 0
                if not theme or not self.e.u32(theme + 0xC):
                    return None
            self.call(FN["update_ui"])
        elif code == OP["keys"]:
            self.call(FN["keys"])
        elif code == OP["enable"]:
            self.call(FN["enable"], a[0])
        elif code == OP["mail"]:
            if not has_window:
                return None
            self.call(FN["mail"])
        return []

    def setter(self, code, a):
        e = self.e
        if code == OP["game_type"]:
            self.game_type = a[0]
            w32(e, G["game_type"], a[0])
        elif code == OP["current"]:
            w32(e, self.theme_screen + 0xC, 0 if a[0] < 0 else self.islands[a[0]])
        elif code == OP["screens"]:
            w32(e, G["screens"], 0 if a[0] == 0 else self.screens)
            w32(e, self.screens + 4, 0 if a[0] == 1 else self.theme_screen)
        elif code == OP["outbox"]:
            self.outbox = a[0]
            if e.u32(G["user"]):
                w32(e, e.u32(G["user"]) + 0x8C, a[0])
        elif code == OP["balance"]:
            self.balance[a[0]] = (a[1], a[2], a[3])
        elif code == OP["master_null"]:
            w32(e, G["master"], 0)
        elif code == OP["user_null"]:
            w32(e, G["user"], 0)
        elif code == OP["globals"]:
            master = self.master()
            if master:
                e.write(master + 0x18, bytes((a[0] >> i) & 1 for i in range(4)))
                e.write(master + 0x26, bytes((a[1] >> i) & 1 for i in range(2)))
                w32(e, master + 0x20, a[2])
                w32(e, master + 0x1C, a[3])
        elif code == OP["park_tickets"]:
            park = self.park(a[0])
            if park:
                e.write(park, bytes((a[1] >> i) & 1 for i in range(6)))
                e.write(park + 6, bytes((a[2] >> i) & 1 for i in range(4)))
        elif code == OP["sign"]:
            park = self.park(a[0])
            if park:
                e.write(park + 0x34, wide_bytes(SIGNS[a[1] % len(SIGNS)], 0x21))
                e.write(park + 0x76, wide_bytes(SIGNS[a[2] % len(SIGNS)], 0x21))
                e.write(park + 0xB8, bytes([a[3] & 0xFF]))
        elif code == OP["keys_shown"]:
            w32(e, G["keys_shown"], a[0])
        elif code == OP["emitter"]:
            w32(e, G["emitter"], a[0])
        elif code == OP["rect"]:
            self.top, self.bottom = a[0], a[1]
        elif code == OP["window_draw"]:
            w32(e, self.windows[a[0]] + 0x94, self.draws[a[0]][0] if a[1] else 0)
            w32(e, self.windows[a[0]] + 0x98, self.draws[a[0]][1] if a[2] else 0)
        elif code == OP["lobby_window"]:
            w32(e, self.lobby + 4, self.windows[W_LOBBY] if a[0] else 0)
        elif code == OP["ctrl_null"]:
            w32(e, G["ctrl"], 0)
        elif code == OP["guard"]:
            e.write(G["game_guard"], bytes([a[0] & 0xFF]))
        else:
            raise ValueError(code)
        return []


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_il_open", False):
        return
    lib.tpw_il_op.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    lib.tpw_il_dump.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_il_ride.argtypes = [C.c_int32, C.c_int32, C.c_int32, C.POINTER(C.c_uint16)]
    lib.tpw_il_string.argtypes = [C.c_int32, C.POINTER(C.c_uint16)]
    lib.tpw_il_heap.argtypes = [C.POINTER(C.c_int64)]
    lib._il_open = True


def utf16(text):
    return (C.c_uint16 * (len(text) + 1))(*[ord(c) for c in text], 0)


def host_set_string(lib, sid, text):
    lib.tpw_il_string(sid, utf16(text))


class Host:
    def __init__(self, lib, case, base):
        self.lib = lib
        open_host(lib)
        CL.host_begin(lib, {}, {}, FIRST_NEW)
        lib.tpw_il_reset()
        self.overrides = case.get("strings", {})
        for sid, text in self.overrides.items():
            host_set_string(lib, sid, text)
        for i, (rid, kind, text) in enumerate(case.get("rides", DEFAULT_RIDES)):
            lib.tpw_il_ride(i, rid, kind, utf16(text))
        self.base = base
        buf = (C.c_int64 * 4)()
        lib.tpw_il_heap(buf)
        self.heap0 = list(buf)

    def op(self, code, a):
        args = (C.c_int64 * 8)(*a, *([0] * (8 - len(a))))
        out = (C.c_int64 * 4096)()
        self.lib.tpw_il_op(code, args, out)
        return list(out[1:1 + out[0]])

    def dump_state(self):
        out = (C.c_int64 * 4096)()
        self.lib.tpw_il_dump(out)
        return list(out[1:1 + out[0]])

    def take_log(self):
        """The events recorded since the last call (the log restarts)."""
        n = C.c_int32.in_dll(self.lib, "tpw_cl_n").value
        log = (CL.TpwCallEvent * 4096).in_dll(self.lib, "tpw_cl_log")
        events = []
        for i in range(min(n, 4096)):
            ev = log[i]
            events.append((ev.name.decode("latin-1"), tuple(ev.ints[k] for k in range(ev.nints)),
                           tuple(ev.texts[k].value.decode("latin-1") for k in range(ev.ntexts))))
        if n > 4096:
            events.append(("OVERFLOW", (n,), ()))
        C.c_int32.in_dll(self.lib, "tpw_cl_n").value = 0
        return events

    def heap(self):
        buf = (C.c_int64 * 4)()
        self.lib.tpw_il_heap(buf)
        return [buf[i] - self.heap0[i] for i in range(4)]

    def finish(self):
        C.c_int32.in_dll(self.lib, "tpw_cl_on").value = 0
        for sid in self.overrides:
            host_set_string(self.lib, sid, self.base.get(sid, ""))


# ======================================================================= running a script

RAN = {}
STUBS = None
BASE = {}


def with_stubs(fn):
    def run(e, case):
        saved = STUBS.activate(e)
        try:
            return fn(e, case)
        finally:
            CL.Stubs.deactivate(e, saved)
    return run


@with_stubs
def il_ppc(e, case):
    world = Ppc(e, case)
    steps, ran = [], []
    try:
        for code, a in case["ops"]:
            answer = world.op(code, a)
            if answer is None:
                continue
            ran.append((code, a))
            log, world.L.log = list(world.L.log), []
            steps.append(dict(op=code, answer=answer, log=log, state=world.dump_state()))
        RAN[id(case)] = ran
        return dict(steps=steps, heap=[world.heap[0], world.heap[2]])
    finally:
        world.restore()


def il_host(lib, case):
    host = Host(lib, case, BASE)
    steps = []
    try:
        for code, a in RAN.pop(id(case)):
            answer = host.op(code, a)
            log = host.take_log()
            steps.append(dict(op=code, answer=answer, log=log, state=host.dump_state()))
        heap = host.heap()
    finally:
        host.finish()
    return dict(steps=steps, heap=[heap[0], heap[2]])


def setup(runner):
    global STUBS, BASE
    e = runner.emu
    T.install_std_stubs(e)
    if not FS.VT:
        FS.make_vtables(e)
    make_fakes(e)
    load_name_table(e)
    STUBS = build_stubs(e)
    BASE = base_strings()
    lib = runner.lib
    open_host(lib)
    for sid, text in BASE.items():
        host_set_string(lib, sid, text)


# ======================================================================= scripts

TESTS = []


def o(name, *args):
    return (OP[name], list(args))


def case_of(ops, rides=None, strings=None):
    case = dict(ops=ops)
    if rides is not None:
        case["rides"] = rides
    if strings:
        case["strings"] = strings
    return case


# ---- SetFrame

FRAMES = [0, 1, -1, 3, 5, 0x7FFF, 0x8000, 0x12345, -0x8001, 0x7FFFFFFF, -0x80000000]


def frame_edges():
    for win in range(W_MAIL):
        ops = []
        for frame in FRAMES:
            ops.append(o("set_frame", win, frame))
        yield case_of(ops)
    for draw in ((0, 0), (1, 0), (0, 1), (1, 1)):
        ops = [o("window_draw", W_KEYCOST, *draw)] + [o("set_frame", W_KEYCOST, f) for f in (0, 1, -1, 0x4321)]
        yield case_of(ops)


def frame_random(r):
    ops = []
    for _ in range(r.randrange(2, 8)):
        if r.random() < 0.3:
            ops.append(o("window_draw", r.randrange(W_MAIL), r.randrange(2), r.randrange(2)))
        else:
            ops.append(o("set_frame", r.randrange(W_MAIL), r.choice(FRAMES + [r.randrange(-70000, 70000)])))
    return case_of(ops)


# ---- the master save

def ms_edges():
    yield case_of([o("ms_new"), o("ms_keys"), o("ms_reset"), o("ms_keys")])
    # the keys: a third of the tickets, plus the extra keys
    for tickets in range(0, 13):
        ops = [o("ms_new")] + [o("ms_getinfo", t) for t in range(3)]
        mask = [0, 0, 0]
        ops.append(o("globals", tickets % 16, (tickets // 4) % 4, 0, 0))
        for t in range(3):
            ops.append(o("park_tickets", t, (1 << min(6, (tickets + t) % 7)) - 1, 0))
        ops.append(o("ms_keys"))
        yield case_of(ops)
    for extra in (0, 1, 2, -1, -5, 100, 0x7FFFFFFF):
        yield case_of([o("ms_new"), o("globals", 0xF, 3, extra, 7), o("ms_keys"), o("ms_reset"), o("ms_keys")])
    # parks whose balance file cannot be read do not count their tickets
    ops = [o("ms_new")] + [o("ms_getinfo", t) for t in range(4)] + [o("park_tickets", t, 0x3F, 0xF) for t in range(4)]
    ops += [o("ms_keys"), o("balance", 1, 0, 0, 0), o("balance", 2, 0, 0, 0), o("ms_keys")]
    yield case_of(ops)
    # GetInfoOn and CreateNewTheme: a theme that is not there, one that is, one whose file is missing; all eight names
    ops = [o("ms_new")] + [o("ms_getinfo", t) for t in range(8)] + [o("ms_getinfo", t) for t in (7, 3, 0)]
    yield case_of(ops)
    ops = [o("ms_new"), o("balance", 0, 0, 0, 0), o("ms_getinfo", 0), o("ms_getinfo", 0), o("balance", 0, 1, 4, 1190),
           o("ms_getinfo", 0), o("ms_getinfo", 0)]
    yield case_of(ops)
    ops = [o("ms_new")] + [o("ms_create", t) for t in (3, 5, 7, 0, 3, 5, 1, 1, 2, 6, 4)]
    yield case_of(ops)
    ops = [o("ms_new"), o("ms_create", 2), o("park_tickets", 2, 7, 3), o("ms_create", 2), o("park_tickets", 2, 1, 0),
           o("ms_keys"), o("ms_getinfo", 2), o("ms_keys")]
    yield case_of(ops)
    # no master save: the operations that need one are skipped
    yield case_of([o("ms_keys"), o("ms_getinfo", 1), o("ms_new"), o("master_null"), o("ms_keys")])


# ---- the park balance table's description and values (the two virtual members)

def bal_edges():
    yield case_of([o("bal_row", row) for row in range(11)])


# ---- the park record

def pd_edges():
    # tickets
    for mask in (0, 1, 0x3F, 0x15, 0x2A):
        yield case_of([o("pd_new", 1), o("park_tickets", SCRATCH, mask, 0), o("pd", SCRATCH, 0)])
    # the key cost: with the file, without, and again after the file appears
    for cost in (0, 1, 2, 3, 7, -1, 0x7FFFFFFF, -0x80000000):
        yield case_of([o("pd_new", 2), o("balance", 2, 1, cost, 1150), o("pd", SCRATCH, 1), o("pd", SCRATCH, 1)])
    ops = [o("pd_new", 0), o("balance", 0, 0, 5, 1150), o("pd", SCRATCH, 1), o("pd", SCRATCH, 2), o("balance", 0, 1, 5, 1150),
           o("pd", SCRATCH, 2), o("pd", SCRATCH, 1), o("pd", SCRATCH, 2)]
    yield case_of(ops)
    # a theme name that is not one of the world's (its balance file reads as missing)
    yield case_of([o("pd_new", 7), o("pd", SCRATCH, 2), o("pd", SCRATCH, 0)])
    # the full name: the sign names (every pair of the sign texts with the flag), then the gate object's name
    for size in (0x90, 0x50, 12, 5, 2, 1, 0):
        for a in range(len(SIGNS)):
            for b in (0, 1, 2, 4, 8, 9):
                yield case_of([o("pd_new", 1), o("sign", SCRATCH, a, b, 1), o("pd", SCRATCH, 3, size)])
    for flag in (0, 2, 255):
        yield case_of([o("pd_new", 1), o("sign", SCRATCH, 2, 5, flag), o("pd", SCRATCH, 3, 0x50)])
    for gate in (1150, 1190, 1180, 1100, 1500, 1108, 1601, 2601, 3601, 4601, 7777, 0, 65535, 0x10000 + 1190, -1, 0x7FFFFFFF):
        for size in (0x50, 0x90, 6, 1):
            yield case_of([o("pd_new", 1), o("balance", 1, 1, 2, gate), o("pd", SCRATCH, 3, size)])
    yield case_of([o("pd_new", 1), o("balance", 1, 0, 2, 1190), o("pd", SCRATCH, 3, 0x50), o("pd", SCRATCH, 3, 0x50)])
    # a park in the map, through GetInfoOn
    yield case_of([o("ms_new"), o("ms_getinfo", 3), o("sign", 3, 2, 5, 1), o("pd", 3, 3, 0x50), o("pd", 3, 0), o("pd", 3, 1)])


# ---- the ride names

def real_ids():
    return [row[0] for row in NAME_TABLE]


def name_edges():
    ids = real_ids()
    # every ride type of the table, and ids that are not in it
    for rid in ids:
        yield case_of([o("name_a", rid, 0x40, 0), o("name_a", rid, 0x40, 1), o("name_b", rid, 0x40), o("name", rid, 0x40),
                       o("type_name", rid, 0x90)])
    for rid in (0, 1, 2, 7777, 65535, 1151, 1149, -1, 0x10000 + 1150):
        yield case_of([o("name_a", rid, 0x40, 1), o("name_b", rid, 0x40), o("name", rid, 0x80), o("type_name", rid, 0x40)])
    # sizes: the clamp at 0x80, a size of 0, the string table's truncation
    for rid in (1150, 1190, 1501, 1108):
        for size in BUFFER_SIZES:
            yield case_of([o("name", rid, size), o("type_name", rid, size), o("name_a", rid, size, 1), o("name_b", rid, size)])
    # the hyphen: a first part that ends with one is joined without a blank, and the hyphen is dropped
    hyphens = {2: "Roller-", 3: "coaster", 4: "-", 5: "x", 6: "", 7: "", 8: "Ends with two--", 9: "tail", 10: "a" * 80, 11: "b" * 80}
    yield case_of([o("name_a", 1150, 0x40, 1), o("name_a", 1150, 0x40, 0), o("name", 1150, 0x80), o("name", 1190, 0x80),
                   o("name", 1180, 0x80), o("name", 1170, 0x80), o("name", 1108, 0x80), o("name", 1106, 0x80),
                   o("type_name", 1150, 0x80)], strings=hyphens)
    # the ride database: names of the three kinds, the cache (id 0 first finds nothing), ids repeated and alternating
    ops = [o("type_name", 0, 0x40), o("type_name", 1150, 0x40), o("type_name", 1150, 0x40), o("type_name", 1190, 0x40),
           o("type_name", 1180, 0x40), o("type_name", 1100, 0x40), o("type_name", 1500, 0x40), o("type_name", 1108, 0x90),
           o("type_name", 1108, 0x20), o("type_name", 0, 0x40), o("type_name", 7777, 0x40), o("type_name", 1190, 0x40)]
    yield case_of(ops)
    yield case_of([o("type_name", 1190, 0x50), o("type_name", 1150, 0x50)], rides=[(1150, 2, "Own name")])
    yield case_of([o("type_name", 1150, 0x50), o("type_name", 0, 0x50), o("type_name", 1150, 0x50)], rides=[])
    yield case_of([o("type_name", 5, 0x50), o("type_name", 0, 0x50)], rides=[(0, 2, "Zero"), (5, 1, "")])


def name_random(r):
    ids = real_ids() + [0, 1, 7777, 65535]
    ops = []
    strings = {}
    if r.random() < 0.3:
        for sid in r.sample(range(2, 60), 4):
            strings[sid] = r.choice(["", "x", "Name-", "-", "ab-", "a" * 70, "Two words", "q" * 63, "q" * 64])
    for _ in range(r.randrange(2, 9)):
        kind = r.choice(("name_a", "name_b", "name", "type_name", "type_name"))
        rid = r.choice(ids)
        size = r.choice(BUFFER_SIZES + [r.randrange(0, 0x91)])
        ops.append(o(kind, rid, size, r.randrange(2)) if kind == "name_a" else o(kind, rid, size))
    rides = [(r.choice(ids), r.randrange(3), r.choice(["", "Own", "A longer own name", "z" * 50])) for _ in range(r.randrange(0, 7))]
    return case_of(ops, rides=rides, strings=strings)


# ---- focused random scripts for the master save and the park record

def ms_random(r):
    ops = [o("ms_new")]
    for _ in range(r.randrange(8, 30)):
        k = r.random()
        if k < 0.3:
            ops.append(o("ms_getinfo", r.randrange(8)))
        elif k < 0.4:
            ops.append(o("ms_create", r.randrange(8)))
        elif k < 0.55:
            ops.append(o("ms_keys"))
        elif k < 0.65:
            ops.append(o("globals", r.randrange(16), r.randrange(4), r.choice([0, 0, 1, 2, -3, 40]), r.randrange(5)))
        elif k < 0.8:
            ops.append(o("park_tickets", r.randrange(8), r.randrange(64), r.randrange(16)))
        elif k < 0.9:
            ops.append(o("balance", r.randrange(8), r.randrange(2), r.choice([0, 1, 2, 3, 7, -1]), r.choice([1150, 1190, 7777])))
        elif k < 0.95:
            ops.append(o("ms_reset"))
        else:
            ops.append(o("ms_new"))
    return case_of(ops)


def pd_random(r):
    ops = [o("pd_new", r.randrange(8))] if r.random() < 0.7 else [o("ms_new"), o("ms_getinfo", r.randrange(8))]
    which = SCRATCH if ops[0][0] == OP["pd_new"] else None
    for _ in range(r.randrange(4, 20)):
        target = which if which is not None else r.randrange(8)
        k = r.random()
        if k < 0.25:
            ops.append(o("sign", target, r.randrange(10), r.randrange(10), r.randrange(2)))
        elif k < 0.35:
            ops.append(o("balance", r.randrange(8), r.randrange(2), r.choice([0, 1, 2, 3, 7, -1]),
                         r.choice([1150, 1190, 1180, 1100, 1500, 1108, 7777, 0, 0x10000 + 1190])))
        elif k < 0.45:
            ops.append(o("park_tickets", target, r.randrange(64), r.randrange(16)))
        elif k < 0.6:
            ops.append(o("pd", target, r.randrange(3), 0))
        else:
            ops.append(o("pd", target, 3, r.choice(BUFFER_SIZES)))
    return case_of(ops)


# ---- the controller

def ctrl_edges():
    # the controller made with every kind of lobby: no lobby window (nothing is made), a window
    yield case_of([o("lobby_window", 0), o("create"), o("create"), o("show"), o("mail"), o("keys"), o("update_ui"), o("enable", 1)])
    yield case_of([o("create"), o("create")])
    # without a master save, user info or islands; with an island
    yield case_of([o("outbox", 0), o("create"), o("mail"), o("outbox", 3), o("mail"), o("show"), o("user_null"), o("mail")])
    for game_type in (0, 1, 2, 3, -1):
        ops = [o("game_type", game_type), o("current", 0), o("create"), o("keys"), o("update_ui"), o("enable", 0), o("enable", 1)]
        yield case_of(ops)
    # the game's own four themes with their own costs and gate objects (the parks' names are those of the game: LOST Kingdom,
    # HALLOWEEN world, Space Zone, Wonder Land)
    for island in range(4):
        for tickets in (0, 3, 6, 9, 12, 15):
            ops = [o("current", island), o("ms_new"), o("create"), o("keys"), o("update_ui"), o("globals", tickets % 16, tickets // 4, tickets // 6, 0)]
            ops += [o("park_tickets", t, 0x3F if tickets > 6 else 0x07, 0) for t in range(4)] + [o("keys"), o("update_ui"), o("ms_keys")]
            yield case_of(ops)
    # keys and tickets: 0, 1, the limit and above, for an island that costs 0, 1, 2, 3, 5 keys
    for cost in (0, 1, 2, 3, 5, 9, -1):
        for tickets in (0, 1, 3, 6, 9, 11):
            ops = [o("current", 1), o("balance", 1, 1, cost, 1190), o("ms_new"), o("ms_getinfo", 1), o("create"),
                   o("globals", tickets % 16, tickets % 4, tickets // 5, 0), o("park_tickets", 1, (1 << (tickets % 7)) - 1, 0),
                   o("keys"), o("update_ui"), o("keys"), o("update_ui")]
            yield case_of(ops)
    # the key count display near the digits: 0 keys hides it, 1 or more shows "N x"
    for extra in (0, 1, 2, 9, 10, 99, 100, 12345, -1, -7):
        yield case_of([o("current", 2), o("create"), o("globals", 0, 0, extra, 0), o("keys"), o("update_ui"), o("globals", 0, 0, 0, 0), o("keys")])
    # the emitter: started when the island can be paid for, killed when not, never started twice; the panel's place
    for top, bottom in ((10, 40), (0, 0), (-5, 7), (32767, 32767), (-32768, 32767), (100, 0), (1, 2), (-1, -2), (300, 301), (7, 8)):
        ops = [o("current", 0), o("ms_new"), o("balance", 0, 1, 1, 1190), o("globals", 3, 0, 5, 0), o("rect", top, bottom), o("create"),
               o("emitter", 0), o("keys"), o("keys"), o("update_ui"), o("keys_shown", 0), o("update_ui"), o("balance", 0, 1, 99, 1190),
               o("keys"), o("emitter", 55), o("keys"), o("update_ui"), o("globals", 0xF, 3, 50, 0), o("keys"), o("update_ui")]
        yield case_of(ops)
    # UpdateIslandLobbyUI compares with the keys the display last saw
    for shown in (-1, 0, 1, 2, 3, 4):
        ops = [o("current", 4), o("balance", 4, 1, 3, 1180), o("create"), o("keys_shown", shown), o("emitter", 0), o("update_ui"), o("update_ui")]
        yield case_of(ops)
    # no park (the balance file cannot be read): the island's own title; the screen system missing in each way
    yield case_of([o("current", 3), o("balance", 3, 0, 0, 0), o("create"), o("keys"), o("update_ui"), o("balance", 3, 1, 2, 1180), o("update_ui")])
    for mode in (0, 1, 2):
        yield case_of([o("screens", mode), o("create"), o("keys"), o("update_ui"), o("screens", 3), o("current", 5), o("keys"), o("update_ui")])
    # the islands: every one, with every theme's balance (a theme with no file, a free one)
    for island in range(8):
        for (ok, cost) in ((1, 2), (0, 0), (1, 0)):
            ops = [o("current", island), o("balance", island, ok, cost, 1150 + 40 * (island % 3)), o("create"), o("ms_keys"),
                   o("keys"), o("update_ui"), o("sign", island, 5, 1, 1), o("update_ui"), o("mail")]
            yield case_of(ops)
    # window draw objects present and absent (SetFrame inside the controller)
    for w in (W_KEYCOST, W_DIGIT, W_ICON):
        for draw in ((0, 0), (1, 0), (0, 1)):
            yield case_of([o("window_draw", w, *draw), o("current", 1), o("create"), o("keys"), o("update_ui")])
    yield case_of([o("current", 1), o("create"), o("show"), o("ctrl_null"), o("show"), o("keys"), o("update_ui"), o("enable", 1)])
    # the game type object made on first use (the guard byte clear), in each of the three functions
    for first in ("keys", "update_ui", "create"):
        yield case_of([o("game_type", 2), o("current", 1), o("ms_new"), o("ms_getinfo", 1), o("create"), o("guard", 0), o(first)])
        yield case_of([o("game_type", 0), o("current", 1), o("guard", 0), o("create"), o("guard", 0), o("keys"), o("guard", 0), o("update_ui")])
    # a master save that is made by the display (none before), and again after it is dropped
    yield case_of([o("current", 6), o("create"), o("master_null"), o("keys"), o("master_null"), o("update_ui"), o("master_null"), o("keys")])


CTRL_OPS = ["create", "show", "update_ui", "keys", "enable", "mail"]


def random_op(r):
    k = r.random()
    if k < 0.34:
        name = r.choice(CTRL_OPS)
        return o(name, r.randrange(2)) if name == "enable" else o(name)
    if k < 0.40:
        return o("set_frame", r.randrange(W_MAIL), r.choice(FRAMES))
    if k < 0.46:
        return o("ms_new") if r.random() < 0.3 else o("master_null") if r.random() < 0.3 else o("ms_keys")
    if k < 0.52:
        return o("ms_getinfo", r.randrange(8)) if r.random() < 0.7 else o("ms_create", r.randrange(8))
    if k < 0.56:
        return o("pd_new", r.randrange(8)) if r.random() < 0.4 else o("pd", r.randrange(9), r.randrange(4), r.choice(BUFFER_SIZES))
    if k < 0.59:
        return o("ms_reset")
    if k < 0.62:
        return o("current", r.randrange(-1, 8))
    if k < 0.64:
        return o("screens", r.randrange(4))
    if k < 0.67:
        return o("balance", r.randrange(8), r.randrange(2), r.choice([0, 1, 2, 3, 4, 5, 9, -1, 100]), r.choice([1150, 1190, 1180, 1100, 1500, 7777, 0]))
    if k < 0.72:
        return o("globals", r.randrange(16), r.randrange(4), r.choice([0, 0, 1, 2, 5, -2, 40]), r.randrange(5))
    if k < 0.77:
        return o("park_tickets", r.randrange(9), r.randrange(64), r.randrange(16))
    if k < 0.80:
        return o("sign", r.randrange(9), r.randrange(10), r.randrange(10), r.randrange(2))
    if k < 0.83:
        return o("game_type", r.choice([0, 0, 0, 1, 2, 2, 3]))
    if k < 0.85:
        return o("outbox", r.choice([0, 1, 5]))
    if k < 0.87:
        return o("rect", r.randrange(-40, 400), r.randrange(-40, 400))
    if k < 0.89:
        return o("keys_shown", r.choice([-1, 0, 1, 2, 3, 4, 8]))
    if k < 0.90:
        return o("emitter", r.choice([0, 0, 33]))
    if k < 0.92:
        return o("window_draw", r.randrange(W_MAIL), r.randrange(2), r.randrange(2))
    if k < 0.93:
        return o("lobby_window", r.randrange(2))
    if k < 0.935:
        return o("guard", 0)
    if k < 0.94:
        return o("user_null")
    if k < 0.95:
        return o("ctrl_null")
    kind = r.choice(("name_a", "name_b", "name", "type_name"))
    rid = r.choice(real_ids() + [0, 7777])
    size = r.choice(BUFFER_SIZES)
    return o(kind, rid, size, r.randrange(2)) if kind == "name_a" else o(kind, rid, size)


def script_random(r):
    ops = [random_op(r) for _ in range(r.randrange(20, 60))]
    if r.random() < 0.5:
        ops.insert(0, o("create"))
    strings = {}
    if r.random() < 0.2:
        strings = {2: "Dino-", 3: "Karts", 4: "", 5: "x" * 70}
    return case_of(ops, strings=strings)


def long_script(r):
    """Two hundred operations."""
    ops = [o("current", r.randrange(8)), o("create")] + [random_op(r) for _ in range(198)]
    return case_of(ops)


# ---- the tests: each module-level test runs scripts through the same runner

TESTS.append(FnTest("InterfaceWindow::SetFrame", il_ppc, il_host, edge=frame_edges, rnd=frame_random))
TESTS.append(FnTest("CFSGlobalMasterSave: the constructor, ResetGlobalData, HowManyKeys, GetInfoOn, CreateNewTheme", il_ppc, il_host,
                    edge=ms_edges, rnd=ms_random))
TESTS.append(FnTest("CFSGlobalParkData: the constructor, HowManyTickets, KeyCostToEnter, CheckBalanceFile, GetFullName", il_ppc, il_host,
                    edge=pd_edges, rnd=pd_random))
TESTS.append(FnTest("CGlobalParkDataBalance: GetBalanceDescription and GetBalanceValuesStart", il_ppc, il_host, edge=bal_edges,
                    rnd=lambda r: case_of([o("bal_row", r.randrange(11)) for _ in range(3)])))
TESTS.append(FnTest("CRideNameDB::GetName, GetNameA, GetNameB and CRideDatabase::GetTypeName", il_ppc, il_host, edge=name_edges,
                    rnd=name_random))
TESTS.append(FnTest("The island lobby's controller (six functions)", il_ppc, il_host, edge=ctrl_edges, rnd=script_random))
TESTS.append(FnTest("Scripts of 200 operations", il_ppc, il_host, edge=lambda: [], rnd=long_script))
