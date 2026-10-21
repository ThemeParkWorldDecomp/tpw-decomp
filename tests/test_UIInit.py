"""Differential tests for the interface's start-up, against libtpw_unit_UIInit.dylib
(tests/unit_UIInit.cpp):

  src/game/ui/InterfaceMessageQueue.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/UI_Base.cpp          UI_InitSystem, the message queues (InterfaceMessageQueue, InterfaceTimedMessages),
                           UI_InitTimerSystem, UI_PostMessage, UI_FlushAllMessages, UI_DevicePointerMove,
                           UI_UpdatePointerIn, the peek, clock, cursor and help text switches
  src/game/ui/InterfaceWindow.cpp  InterfaceWindow's constructor, PointIn, MakeRectAbsolute
  src/game/ui/InterfaceBFCursor.cpp, src/game/ui/UIInterface.cpp, src/game/ui/ui_main.cpp       InitialiseCOLOURCursors, LoadBFCurs, InterfaceBFCursor's constructor and LoadData,
                           InitLobbyUI, UIInterface::InitLobby

Real on both sides: those, the rectangles (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp), the cursor base class and the cursor table
(src/game/ui/InterfaceCursor.cpp, src/game/ui/InterfaceDrawEngine.cpp, src/game/ui/InterfaceDrawObject.cpp) and the UI globals (src/game/ui/UI_Base.cpp). Fakes that record the same event on both
sides: the window's untranslated virtual members, the timers' constructor, the picture loaders, the
platform's pixel and cursor classes, FindFilePath, TRACE, the engine's light and map.

Two worlds. The UI world has eight windows in fixed storage, two message queues and a timed queue, six
cursors in two cursor tables, a help text object, clocks, peek functions and three fake regions; a script of
operations runs over it, and after each operation the answer, the UI's globals, every window, the queues,
the timers, the interface's light, the heap traffic and the length of the call log are compared (the whole
log at the end). The cursor world serves pictures to the loader fakes: the game's real cursor files (read
from ROOT/extracted when the test runs) and variants of their headers.

On the PowerPC side the objects are made with the original constructors in memory filled with 0xa5, and the
virtual members of the fake cursors, regions and help text go through fake vtables; the host's objects get
the same fill from the allocation observer, so a member no constructor sets compares as untouched. An
operation the state does not allow (a window that was not made, a post to a queue that does not exist, which
crashes the original) is decided on the PowerPC side and skipped on both.
"""
import ctypes as C
import pathlib
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIInit"

MINUS = 0xFFFFFFFF
ROOT_DIR = pathlib.Path(__file__).resolve().parents[2]
CURSOR_DIR = ROOT_DIR / "extracted/disc-hfs/Theme Park Data/data/ui/cursors"

# ---- the unit's functions
FN = dict(InitSystem=0x101C8A5C, WindowCtor=0x101D98D8, QueueCtor=0x101C7AD8, QueueInit=0x101C7BC4, QueueAdd=0x101C7C98,
          QueueFlush=0x101C80FC, TimedCtor=0x101C81D0, TimedInit=0x101C85F0, TimedFlush=0x101C8580, InitTimer=0x101D86C4,
          Post=0x101C9400, FlushAll=0x101C9E48, PointerMove=0x101C8D94, UpdatePointerIn=0x101C8F18, PointIn=0x101DC3E8,
          MakeAbs=0x101DAB88, SetPeek=0x101C9F18, SetClock=0x101CABE0, SetCursor=0x101C9F68, SetStandard=0x101CA078,
          HideCursor=0x101CA0B0, ShowCursor=0x101CA110, UseTable=0x101CA170, DisableHelp=0x101CAA4C, EnableHelp=0x101CAAAC,
          InitColour=0x1018575C, LoadBFCurs=0x10185670, BFCtor=0x101FE208, LoadData=0x101FE52C, InitLobby=0x101EF750,
          InitLobbyUI=0x10186324, ButtonDown=0x101C9034, ButtonUp=0x101C9100, Keydown=0x101C929C, Keyup=0x101C9314,
          KeyChar=0x101C9388, MouseWheel=0x101C91CC)
# what they call that is real too: rectangles, the cursor base class, the cursor table
REAL_HELPERS = [0x101C7448, 0x101C7494, 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7618, 0x101C7678, 0x101C7710, 0x101C7788,
                0x101C7A1C, 0x101C7A78, 0x101D5A3C, 0x101D5A70, 0x101D5AA0, 0x101D5AD0, 0x101D5B00, 0x101D5B74,
                0x101CC6A0, 0x101CC6F0, 0x101CC768, 0x101CC7A0, 0x101CC7DC, 0x101CC814, 0x101CC84C, 0x101CC888, 0x101CC8BC,
                0x101CCC8C, 0x101CC994, 0x101CC9CC, 0x101CCA38, 0x101CCAFC, 0x101CCBB0, 0x101CCC34]
CALC_CLIENT_AREA, DEFAULT_CALLBACK, TIMER_CTOR, FIND_FILE_PATH, TRACE = 0x101D987C, 0x101DE3F8, 0x101D819C, 0x100597F8, 0x1006FCFC
CURSOR_CTOR, TABLE_CTOR, TABLE_ALLOCATE, TABLE_SET_ENTRY = 0x101CC6A0, 0x101CC994, 0x101CCAFC, 0x101CCBB0

# ---- the original's data
G = dict(root=0x10395C24, pointer_in=0x10395C1C, capture=0x10395C14, key_capture=0x10395C08, draw_queue=0x10395C00,
         msg_queue=0x10395C04, timed=0x10395BFC, peek=0x10395BF8, started=0x10395BF4, hebrew=0x10395B88, pointer_x=0x10395BC2,
         pointer_y=0x10395BC0, cursor=0x10395BB0, table=0x10395BB4, standard=0x10395BB8, number=0x102AF920, help=0x102AF928,
         clock=0x102AF92C, timers=0x10395C54, timer_count=0x10395C50, timer_list=0x10395C58, paused=0x10395C4C,
         last_window=0x10395C18, buttons=0x10395BBE, active=0x102AF924, button_windows=0x10395BC4)
EXTRA = dict(folder_set=0x10380B8C, folder=0x10380B90, read_failed=0x102B155C, load_failed=0x102B1560, cursor_table=0x1038CC64,
             light=0x1038CC78, settings=0x1038071C)
DEFAULT_CLOCK, DEFAULT_CALLBACK_TV = 0x10395B74, 0x10268480
SIZE_WINDOW = 0x138

# ---- labels (the host's tpw_uiinit_labels)
LAB_WINDOW, LAB_QUEUE, LAB_TIMED, LAB_CURSOR, LAB_HELP, LAB_CLOCK, LAB_DEFAULT_CLOCK = 10, 30, 32, 20, 40, 41, 43
LAB_REGION, LAB_TABLE, LAB_LINK, LAB_FILE = 50, 60, 90, 70
SLOTS, CURSORS, REGIONS, TABLES = 8, 6, 3, 2
KNOWN_NEW = (0x138, 0x14, 8, 0x18, 0x84)   # the sizes operator new is told that the library labels
MAX_ENTRIES = 40                           # the entries of a queue the dumps list
WINDOW_FIELDS = 63                         # the length of a window's dump
LIGHT_SEED = (0.25, 0.5, 0.75, 0.125, 0.375, 0.625, 0.875)
COLOUR_NAMES = ["CNor.tga", "Cbus.tga", "Cpat.tga", "Cque.tga", "Clin.tga", "Cpla.tga", "Cnog.tga", "Cpic.tga", "Ccar.tga",
                "Cera.tga", "Cpyl.tga", "Ccam.tga", "Cash.tga", "Ctra.tga", "Chei.tga", "Crot.tga", "Cpiv.tga", "Csdn.tga",
                "Csup.tga", "CEnd.tga", "CMov.tga", "CCro.tga"]


def s16(v):
    return T.s16(v & 0xFFFF)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


def checksum(data):
    total = 0x811C9DC5
    for byte in data:
        total = ((total ^ byte) * 0x01000193) & MINUS
    return total


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures and the operations."""

    ADDRESSES = ([(a, 4) for a in G.values()] + [(G["button_windows"], 12), (G["key_capture"] + 4, 4), (G["hebrew"], 1), (G["pointer_x"], 2),
                                                  (G["pointer_y"], 2), (G["standard"], 2), (G["number"], 2), (G["buttons"], 2),
                                                  (EXTRA["folder_set"], 4), (EXTRA["folder"], 4), (EXTRA["read_failed"], 4),
                                                  (EXTRA["load_failed"], 4), (EXTRA["cursor_table"], 8), (EXTRA["light"], 4),
                                                  (EXTRA["settings"] + 0x34, 8)])

    def __init__(self, e, case):
        self.e = e
        e.us = self
        self.case = case
        self.heap = [0, 0, 0, 0]
        self.timer_ctors = 0
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in self.ADDRESSES}
        # the fixtures
        self.windows = [e.alloc(SIZE_WINDOW, b"\xa5" * SIZE_WINDOW) for _ in range(SLOTS)]
        self.made = [False] * SLOTS
        self.queues = [e.alloc(0x14, b"\xa5" * 0x14) for _ in range(2)]
        self.timed_block = e.alloc(8, b"\xa5" * 8)
        self.queue_made = [False, False]
        self.timed_made = False
        self.cursors = []
        for _ in range(CURSORS):
            c = e.alloc(0x14, b"\xa5" * 0x14)
            T.call_ppc(e, CURSOR_CTOR, [c])
            w32(e, c + 0x10, e.fake_cursor_vtable)
            self.cursors.append(c)
        self.tables = [e.alloc(8, b"\xa5" * 8) for _ in range(TABLES)]
        self.help = e.alloc(0x20, bytes(0x20))
        w32(e, self.help + 0x10, e.fake_help_vtable)
        self.clocks = [e.alloc(8, bytes(8)) for _ in range(2)]
        self.regions = [e.alloc(8, bytes(8)) for _ in range(REGIONS)]
        for r in self.regions:
            w32(e, r, e.fake_region_vtable)
        self.seeds = list(case.get("seeds", (0, 0, 0)))
        self.link = e.alloc(8, bytes(8))
        self.light = e.alloc(0x68, bytes(0x68))
        w32(e, self.light, self.link)
        self.bf_block = e.alloc(0x18, b"\xa5" * 0x18)
        self.bf_made = False
        self.table_made = False
        fixed = {}
        fixed.update({w: LAB_WINDOW + i for i, w in enumerate(self.windows)})
        fixed.update({q: LAB_QUEUE + i for i, q in enumerate(self.queues)})
        fixed[self.timed_block] = LAB_TIMED
        fixed.update({c: LAB_CURSOR + i for i, c in enumerate(self.cursors)})
        fixed[self.help] = LAB_HELP
        fixed.update({c: LAB_CLOCK + i for i, c in enumerate(self.clocks)})
        fixed[DEFAULT_CLOCK] = LAB_DEFAULT_CLOCK
        fixed.update({r: LAB_REGION + i for i, r in enumerate(self.regions)})
        fixed.update({t: LAB_TABLE + i for i, t in enumerate(self.tables)})
        fixed[self.link] = LAB_LINK
        fixed[self.bf_block] = LAB_FILE
        self.L = e.cl = CL.PpcLog(fixed, case.get("q"))
        self.peek_tv = [0, e.fake_peek[0], e.fake_peek[1], e.fake_peek[2]]
        # the UI's globals as the start-up leaves them
        for name in ("root", "pointer_in", "capture", "draw_queue", "msg_queue", "timed", "peek", "started", "cursor", "table",
                     "help", "timers", "timer_count", "timer_list", "paused", "last_window"):
            w32(e, G[name], 0)
        e.write(G["key_capture"], bytes(8))
        e.write(G["hebrew"], b"\0")
        e.write(G["pointer_x"], struct.pack(">h", 0))
        e.write(G["pointer_y"], struct.pack(">h", 0))
        e.write(G["standard"], struct.pack(">H", 0))
        e.write(G["number"], struct.pack(">h", -1))
        e.write(G["buttons"], struct.pack(">h", 0))
        w32(e, G["active"], 1)
        e.write(G["button_windows"], bytes(12))
        w32(e, G["clock"], DEFAULT_CLOCK)
        w32(e, EXTRA["folder_set"], 0)
        w32(e, EXTRA["folder"], 0)
        w32(e, EXTRA["read_failed"], MINUS)
        w32(e, EXTRA["load_failed"], MINUS)
        e.write(EXTRA["settings"] + 0x34, bytes(8))
        w32(e, EXTRA["light"], self.light)
        for i, seed in enumerate(LIGHT_SEED[:4]):
            e.write(self.light + 4 + 4 * i, struct.pack(">f", seed))
        for i, seed in enumerate(LIGHT_SEED[4:]):
            e.write(self.light + 0x2C + 4 * i, struct.pack(">f", seed))
        w32(e, self.light + 0x54, 0x12345678)
        for table, (count, mask) in enumerate(case.get("tables", ((0, 0), (0, 0)))):
            self.make_table(table, count, mask)
        # the cursor world
        self.pictures = []
        self.pixel_store = e.alloc(1 << 16, bytes(1 << 16))
        self.pixel_base = 0
        self.current = None
        self.info = {}
        self.folder_alt = e.alloc(16, b"alt:folder\0")
        for picture in case.get("pictures", ()):
            self.add_picture(picture)
        # the log starts here
        self.L.log.clear()
        self.L.next = CL.FIRST_NEW
        self.heap = [0, 0, 0, 0]

    # ---- the globals the case may leave changed
    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    # ---- fixtures
    def make_table(self, k, count, mask):
        e = self.e
        T.call_ppc(e, TABLE_CTOR, [self.tables[k]])
        T.call_ppc(e, TABLE_ALLOCATE, [self.tables[k], count])
        for i in range(count):
            if (mask >> i) & 1:
                T.call_ppc(e, TABLE_SET_ENTRY, [self.tables[k], i, self.cursors[i % CURSORS]])

    def add_picture(self, picture):
        e = self.e
        entry = dict(picture)
        entry["path_address"] = e.alloc(len(picture["path"]) + 1, picture["path"].encode("latin-1") + b"\0")
        self.pictures.append(entry)

    def window(self, slot):
        return self.windows[slot] if 0 <= slot < SLOTS and self.made[slot] else 0

    def queue(self, k):
        return self.queues[k] if 0 <= k < 2 and self.queue_made[k] else 0

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def folder_ok(self):
        """False when LoadBFCurs's folder is marked set but is NULL (sprintf would read a NULL string)."""
        return not (self.e.read(EXTRA["folder_set"], 1)[0] != 0 and self.e.u32(EXTRA["folder"]) == 0)

    def posting_safe(self):
        """False when a queue the UI posts to has no slots (Add would write before its memory: slot -1)."""
        e = self.e
        for name in ("draw_queue", "msg_queue"):
            q = e.u32(G[name])
            if q and (e.u32(q) == 0 or T.s32(e.u32(q + 4)) <= 0):
                return False
        return True

    def acyclic(self, new_slot=None, new_parent=0):
        """False when the windows' links would make a loop (the original would never come back from one): a parent
        chain, or the child and next links, that comes round to where it began."""
        e = self.e
        known = {w for s, w in enumerate(self.windows) if self.made[s] and s != new_slot}
        if new_slot is not None:
            known.add(self.windows[new_slot])
        for w in known:
            seen, at = set(), w
            while at:
                if at in seen or len(seen) > 20:
                    return False
                seen.add(at)
                at = new_parent if new_slot is not None and at == self.windows[new_slot] else e.u32(at + 0x5C)
                if at not in known:
                    break
        state = {}

        def visit(w):
            if w == 0 or w not in known:
                return True
            if state.get(w) == 1:
                return False
            if state.get(w) == 2:
                return True
            state[w] = 1
            ok = visit(e.u32(w + 0x58)) and visit(e.u32(w + 0x50))
            state[w] = 2
            return ok

        return all(visit(w) for w in known)

    # ---- the dumps (the orders are tpw_uiinit_dump_* in the library)
    def peek_code(self, value):
        return 0 if value == 0 else {self.peek_tv[1]: 1, self.peek_tv[2]: 2}.get(value, 3)

    def dump_globals(self):
        e, L = self.e, self.L
        u = e.u32
        sh = lambda a: struct.unpack(">h", e.read(a, 2))[0]   # noqa: E731
        return [L.lab(u(G["root"])), L.lab(u(G["pointer_in"])), L.lab(u(G["capture"])), L.lab(u(G["key_capture"])),
                L.lab(u(G["key_capture"] + 4)), L.lab(u(G["draw_queue"])), L.lab(u(G["msg_queue"])), L.lab(u(G["timed"])),
                self.peek_code(u(G["peek"])), T.s32(u(G["started"])), e.read(G["hebrew"], 1)[0], sh(G["pointer_x"]),
                sh(G["pointer_y"]), L.lab(u(G["cursor"])), L.lab(u(G["table"])), struct.unpack(">H", e.read(G["standard"], 2))[0],
                sh(G["number"]), L.lab(u(G["help"])), L.lab(u(G["clock"])), int(u(G["timers"]) != 0), T.s32(u(G["timer_count"])),
                L.lab(u(G["timer_list"])), T.s32(u(G["paused"])), L.lab(u(G["last_window"])), sh(G["buttons"]),
                T.s32(u(G["active"]))] + [L.lab(u(G["button_windows"] + 4 * i)) for i in range(3)]

    @staticmethod
    def callback_code(value):
        return 0 if value == 0 else 1 if value == DEFAULT_CALLBACK_TV else 2

    def dump_window(self, address):
        if address == 0:
            return [0]
        d = bytes(self.e.read(address, SIZE_WINDOW))
        L = self.L
        s16_ = lambda o: struct.unpack(">h", d[o:o + 2])[0]   # noqa: E731
        s32_ = lambda o: struct.unpack(">i", d[o:o + 4])[0]   # noqa: E731
        u32_ = lambda o: struct.unpack(">I", d[o:o + 4])[0]   # noqa: E731
        out = [1, d[0]]
        for offset in (4, 0x10, 0x1C, 0x28, 0x34):
            out += [s16_(offset + 4), s16_(offset + 6), s16_(offset + 8), s16_(offset + 10)]
        region = u32_(0x40)
        out.append(0 if region == 0 else 1 if region == address + 4 else L.lab(region))
        out += [s32_(0x44), L.lab(u32_(0x48)), s16_(0x4C)]
        out += [L.lab(u32_(0x50)), L.lab(u32_(0x54)), L.lab(u32_(0x58)), L.lab(u32_(0x5C))]
        out += [s32_(0x60), s32_(0x64), int(d[0x68:0x88] == b"\xa5" * 0x20)]
        out += [s32_(0x88), s32_(0x8C), s32_(0x90), L.lab(u32_(0x94)), L.lab(u32_(0x98)), int(d[0x9C:0xDC] == bytes(0x40))]
        out += [s16_(0xDC), s16_(0xDE), s16_(0xE0), s16_(0xE2), L.lab(u32_(0xE4)), s16_(0xE8), s32_(0xEC), s32_(0xF0)]
        out += [s16_(0xF4), s16_(0xF6), s16_(0xF8), s16_(0xFA), s32_(0xFC), int(d[0x100:0x110] == b"\xa5" * 0x10)]
        out += [self.callback_code(u32_(0x110)), self.callback_code(u32_(0x114)), s32_(0x11C)]
        out += [L.lab(u32_(0x120 + 4 * i)) for i in range(4)]
        out += [s32_(0x130), d[0x134], d[0x135]]
        assert len(out) == WINDOW_FIELDS
        return out

    def dump_queue(self, address):
        if address == 0:
            return [0]
        e, L = self.e, self.L
        entries, count, head, tail, queued = struct.unpack(">IiiiI", e.read(address, 0x14))
        out = [L.lab(address), int(entries != 0), count, head, tail, T.s32(queued)]
        if entries == 0:
            return out
        shown = max(0, min(count, MAX_ENTRIES))
        if shown:
            words = struct.unpack(f">{4 * shown}I", e.read(entries, 16 * shown))
            for i in range(shown):
                out += [-2 if words[4 * i] == 1 else L.lab(words[4 * i]), T.s32(words[4 * i + 1]), T.s32(words[4 * i + 2]), T.s32(words[4 * i + 3])]
        rest = 1
        if count > MAX_ENTRIES:
            rest = int(bytes(e.read(entries + 16 * MAX_ENTRIES, 16 * (count - MAX_ENTRIES))) == bytes(16 * (count - MAX_ENTRIES)))
        return out + [rest]

    def dump_timed(self, address):
        if address == 0:
            return [0]
        e, L = self.e, self.L
        count, entries = struct.unpack(">iI", e.read(address, 8))
        out = [L.lab(address), int(entries != 0), count]
        if entries == 0:
            return out
        for i in range(max(0, min(count, MAX_ENTRIES))):
            w, m, a, b, t = struct.unpack(">IiiiI", e.read(entries + 0x14 * i, 0x14))
            out += [L.lab(w), m, a, b, T.s32(t)]
        return out

    def dump_timers(self):
        e = self.e
        array, count = e.u32(G["timers"]), T.s32(e.u32(G["timer_count"]))
        return [T.s32(e.u32(array + 0x20 * i + 4)) for i in range(max(0, min(count, 300)))] if array else []

    def dump_light(self):
        e = self.e
        return [e.u32(self.light + 0x54)] + [e.u32(self.light + o) for o in (4, 8, 0xC, 0x10, 0x2C, 0x30, 0x34)]

    def dump_state(self, timers):
        e = self.e
        root = e.u32(G["root"])
        state = dict(globals=self.dump_globals(),
                     windows=[self.dump_window(self.windows[s]) if self.made[s] else [0] for s in range(SLOTS)],
                     root=self.dump_window(root) if root not in self.windows else [0],
                     msg_queue=self.dump_queue(e.u32(G["msg_queue"])), draw_queue=self.dump_queue(e.u32(G["draw_queue"])),
                     queues=[self.dump_queue(self.queues[k]) if self.queue_made[k] else [0] for k in range(2)],
                     timed=self.dump_timed(e.u32(G["timed"])),
                     timed_local=self.dump_timed(self.timed_block) if self.timed_made else [0],
                     light=self.dump_light(), heap=list(self.heap), log=len(self.L.log))
        if timers:
            state["timers"] = self.dump_timers()
        return state

    def dump_cursor_state(self):
        e, L = self.e, self.L
        out = dict(heap=list(self.heap), log=len(self.L.log))
        if self.bf_made:
            d = bytes(e.read(self.bf_block, 0x18))
            out["cursor"] = [L.lab(self.bf_block), d[0], T.s8(d[1]), T.s8(d[2]), int(d[3:12] == b"\xa5" * 9),
                             struct.unpack(">i", d[12:16])[0], L.lab(struct.unpack(">I", d[0x14:0x18])[0])]
        else:
            out["cursor"] = [0]
        folder = e.u32(EXTRA["folder"])
        kind = 0 if folder == 0 else 2 if folder == self.folder_alt else 1 if e.cstr(folder) == "data:UI:cursors" else 3
        out["folder"] = [e.read(EXTRA["folder_set"], 1)[0], kind]
        if self.table_made:
            table = EXTRA["cursor_table"]
            entries, count = e.u32(table), struct.unpack(">H", e.read(table + 4, 2))[0]
            dump = [count]
            for i in range(count):
                c = e.u32(entries + 4 * i)
                dump.append(L.lab(c))
                if c:
                    dump += [T.s8(e.read(c + 1, 1)[0]), T.s8(e.read(c + 2, 1)[0]), L.lab(e.u32(c + 0x14))]
            out["table"] = dump
        else:
            out["table"] = [0]
        return out

    # ---- the operations
    def op(self, code, a, text=None):
        """Runs one operation. Returns (answer, timers dumped, array allocations by kind) or None when the state
        does not allow it."""
        e, L = self.e, self.L
        arrays_before = self.heap[1]
        outcome = self.run(code, a, text)
        if outcome is None:
            return None
        answer, timers = outcome
        made = self.heap[1] - arrays_before
        if code == 5:
            kinds = [0, 0, 2, 1]
        elif code == 6:
            kinds = [2]
        elif code == 14:
            kinds = [1 if a[0] == 2 else 0]
        elif code == 46:
            kinds = [4]
        else:
            kinds = [3] * made
        return answer, timers, kinds[:made]

    def run(self, code, a, text):
        e, L = self.e, self.L
        if code == 1:   # a window
            slot, parent, attributes, ident, left, top, right, bottom = a
            if not self.acyclic(slot, self.window(parent)) or not self.posting_safe():
                return None
            e.write(self.windows[slot], b"\xa5" * SIZE_WINDOW)
            self.call(FN["WindowCtor"], self.windows[slot], self.window(parent), attributes, ident, s16(left), s16(top), s16(right),
                      s16(bottom))
            self.made[slot] = True
            return L.lab(self.windows[slot]), False
        if code in (2, 3, 11, 12, 28, 29) and not self.window(a[0]):
            return None
        if code == 2:   # links: slot, parent, previous, next, child
            w = self.windows[a[0]]
            for offset, slot in ((0x5C, a[1]), (0x54, a[2]), (0x50, a[3]), (0x58, a[4])):
                w32(e, w + offset, self.window(slot))
            return 0, False
        if code == 3:   # fields: slot, attributes, destroying, region
            w = self.windows[a[0]]
            w32(e, w + 0x44, a[1])
            e.write(w, bytes([a[2] & 0xFF]))
            w32(e, w + 0x40, w + 4 if a[3] == 0 else self.regions[a[3] - 1])
            return 0, False
        if code == 4:
            return self.set_global(a[0], a[1]), False
        if code in (5, 9, 10, 11, 12) and not self.acyclic():
            return None
        if code in (5, 7, 9, 10, 30, 31, 32, 33, 34, 35) and not self.posting_safe():
            return None
        if code == 5:   # UI_InitSystem
            self.call(FN["InitSystem"], a[0], a[1], a[2])
            return 0, True
        if code == 6:
            self.call(FN["InitTimer"], a[0])
            return 0, True
        if code == 7:   # post
            if 0x1000A <= (a[1] & MINUS) <= 0x1000C and e.u32(G["msg_queue"]) == 0:
                return None
            return T.s32(self.call(FN["Post"], self.window(a[0]), a[1], a[2], a[3])), False
        if code == 8:
            self.call(FN["FlushAll"])
            return 0, False
        if code == 9:
            self.call(FN["PointerMove"], s16(a[0]), s16(a[1]))
            return 0, False
        if code == 10:
            return T.s32(self.call(FN["UpdatePointerIn"])), False
        if code in (30, 31):   # UI_DeviceButtonDown, UI_DeviceButtonUp: button, x, y; the answer is r3 (gUIActive != 0)
            return self.call(FN["ButtonDown" if code == 30 else "ButtonUp"], s16(a[0]), s16(a[1]), s16(a[2])) & MINUS, False
        if code == 35:   # UI_DeviceMouseWheel (lead 2026-10-14): delta, x, y, modifiers; the answer is r3 (gUIActive != 0)
            return self.call(FN["MouseWheel"], a[0], s16(a[1]), s16(a[2]), s16(a[3])) & MINUS, False
        if code in (32, 33, 34):   # UI_Keydown, UI_Keyup, UI_KeyChar (unit w50): key, flags, junk for the register's upper half
            if e.u32(G["active"]) != 0 and e.u32(G["msg_queue"]) == 0:   # the key messages are queued without a test of the queue
                return None
            fn = FN["Keydown" if code == 32 else "Keyup" if code == 33 else "KeyChar"]
            return self.call(fn, (a[0] & 0xFFFF) | (a[2] & 0xFFFF0000), a[1]) & MINUS, False
        if code == 11:
            return L.lab(self.call(FN["PointIn"], self.windows[a[0]], s16(a[1]), s16(a[2]))), False
        if code == 12:
            rect = e.alloc(16, struct.pack(">I4h", 0x102AF8FC, s16(a[1]), s16(a[2]), s16(a[3]), s16(a[4])))
            self.call(FN["MakeAbs"], self.windows[a[0]], rect)
            left, top, right, bottom = struct.unpack(">4h", e.read(rect + 4, 8))
            packed = (left & 0xFFFF) << 48 | (top & 0xFFFF) << 32 | (right & 0xFFFF) << 16 | (bottom & 0xFFFF)
            return packed - (1 << 64) if packed >> 63 else packed, False
        if code == 13:   # a queue's constructor
            if a[0] < 2:
                e.write(self.queues[a[0]], b"\xa5" * 0x14)
                self.call(FN["QueueCtor"], self.queues[a[0]])
                self.queue_made[a[0]] = True
            else:
                e.write(self.timed_block, b"\xa5" * 8)
                self.call(FN["TimedCtor"], self.timed_block)
                self.timed_made = True
            return 0, False
        if code in (14, 15, 16):
            if a[0] < 2 and not self.queue_made[a[0]] or a[0] == 2 and not self.timed_made:
                return None
            if code == 15:
                if a[0] >= 2 or e.u32(self.queues[a[0]]) == 0 or T.s32(e.u32(self.queues[a[0]] + 4)) <= 0:
                    return None   # (an empty ring would be written at slot -1)
                return T.s32(self.call(FN["QueueAdd"], self.queues[a[0]], self.window(a[1]), a[2], a[3], a[4])), False
            if a[0] < 2:
                if code == 14:
                    return T.s16(self.call(FN["QueueInit"], self.queues[a[0]], a[1])), False
                self.call(FN["QueueFlush"], self.queues[a[0]])
                return 0, False
            if code == 14:
                return T.s16(self.call(FN["TimedInit"], self.timed_block, a[1])), False
            self.call(FN["TimedFlush"], self.timed_block)
            return 0, False
        if code == 17:
            self.call(FN["UseTable"], self.tables[a[0]] if a[0] >= 0 else 0)
            return 0, False
        if code == 18:
            self.call(FN["SetStandard"], a[0] & 0xFFFF)
            return 0, False
        if code == 19:
            self.call(FN["SetCursor"], s16(a[0]))
            return 0, False
        if code == 20:
            self.call(FN["HideCursor"])
            return 0, False
        if code == 21:
            self.call(FN["ShowCursor"])
            return 0, False
        if code == 22:
            self.call(FN["SetPeek"], self.peek_tv[a[0]])
            return 0, False
        if code == 23:
            self.call(FN["SetClock"], self.clocks[a[0]] if a[0] >= 0 else 0)
            return 0, False
        if code in (24, 25, 26, 27):
            if e.u32(G["help"]) == 0:
                return None
            self.call(FN[{24: "EnableHelp", 25: "DisableHelp", 26: "InitLobbyUI", 27: "InitLobby"}[code]])
            return 0, False
        if code == 28:   # a window's callbacks: slot, which, code
            value = (0, DEFAULT_CALLBACK_TV, e.other_callback)[a[2]]
            w32(e, self.windows[a[0]] + (0x110 if a[1] == 0 else 0x114), value)
            return 0, False
        if code == 29:   # depth and pivot: slot, z, pivot x, pivot y
            w = self.windows[a[0]]
            e.write(w + 0xDC, struct.pack(">h", s16(a[1])))
            e.write(w + 0xE0, struct.pack(">hh", s16(a[2]), s16(a[3])))
            return 0, False
        # ---- the cursor world
        if code == 41:   # InterfaceBFCursor's constructor
            e.write(self.bf_block, b"\xa5" * 0x18)
            self.call(FN["BFCtor"], self.bf_block)
            self.bf_made = True
            return 0, False
        if code == 42:   # LoadData of a picture, by its number
            if not self.bf_made or not self.pictures:
                return None
            self.current = self.pictures[a[0] % len(self.pictures)]
            return T.s32(self.call(FN["LoadData"], self.bf_block, self.current["path_address"])), False
        if code == 43:   # LoadBFCurs(name)
            if not self.folder_ok():
                return None
            name = e.alloc(len(text) + 1, text.encode("latin-1") + b"\0")
            return L.lab(self.call(FN["LoadBFCurs"], name)), False
        if code == 44:   # InitialiseCOLOURCursors
            if not self.table_made or not self.folder_ok():
                return None
            self.call(FN["InitColour"])
            return 0, False
        if code == 45:   # the constants and the folder: read failed, load failed, flag, folder (0 NULL, 1 the test's)
            w32(e, EXTRA["read_failed"], a[0])
            w32(e, EXTRA["load_failed"], a[1])
            e.write(EXTRA["folder_set"], bytes([a[2] & 0xFF]))
            w32(e, EXTRA["folder"], self.folder_alt if a[3] else 0)
            return 0, False
        if code == 46:   # gCursorTable: its constructor and Allocate
            table = EXTRA["cursor_table"]
            e.write(table, b"\xa5" * 8)
            self.call(TABLE_CTOR, table)
            self.call(TABLE_ALLOCATE, table, a[0])
            self.table_made = True
            return 0, False
        raise ValueError(code)

    def set_global(self, which, v):
        e = self.e
        if which == 1:
            w32(e, G["root"], self.window(v))
        elif which == 2:
            w32(e, G["pointer_in"], self.window(v))
        elif which == 3:
            w32(e, G["capture"], self.window(v))
        elif which == 4:
            w32(e, G["active"], v)
        elif which == 5:
            e.write(G["pointer_x"], struct.pack(">h", s16(v)))
        elif which == 6:
            e.write(G["pointer_y"], struct.pack(">h", s16(v)))
        elif which in (7, 8):
            w32(e, G["draw_queue"] if which == 7 else G["msg_queue"], self.queue(v))
        elif which == 9:
            w32(e, G["timed"], self.timed_block if v == 0 and self.timed_made else 0)
        elif which == 10:
            w32(e, G["cursor"], self.cursors[v] if v >= 0 else 0)
        elif which == 11:
            w32(e, G["help"], self.help if v != 0 else 0)
        elif which == 12:
            e.write(G["buttons"], struct.pack(">h", s16(v)))
        elif which == 13:
            e.write(G["number"], struct.pack(">h", s16(v)))
        elif which == 14:
            e.write(G["standard"], struct.pack(">H", v & 0xFFFF))
        elif which == 15:
            w32(e, G["table"], self.tables[v] if v >= 0 else 0)
        elif which == 16:
            e.write(EXTRA["settings"] + 0x36, bytes([1 if v else 0]))
        elif which in (17, 18, 19):
            w32(e, G["button_windows"] + 4 * (which - 17), self.window(v))
        return 0


# ======================================================================= the fakes (PowerPC side)


def callback_stub(fn):
    """An import stub that calls PowerPC code back (tpwtest.stub_cb, but for one case's stubs): fn(emu, call)."""
    def hook(e):
        e.pending_cb = fn
        e.uc.reg_write(T.P.UC_PPC_REG_PC, T.ppcemu.RETURN_ADDR)
    return hook


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN.values())
    S.real.update(REAL_HELPERS)
    us = lambda e: e.us   # noqa: E731

    def op_new(e):
        size = e.r(3)
        block = e.alloc(max(size, 4), b"\xa5" * max(size, 4))
        us(e).heap[0] += 1
        if size in KNOWN_NEW:
            e.cl.newlab(block)
            e.cl.rec("new", (size,))
        e.set_r(3, block)

    def op_new_array(e):
        size = e.r(3)
        block = e.alloc(max(size, 4), b"\xa5" * max(size, 4))
        us(e).heap[1] += 1
        e.cl.rec("new[]", (size,))
        e.set_r(3, block)

    def op_delete(e):
        us(e).heap[2] += 1
        block = e.r(3)
        if block and e.cl.lab(block) > 0:
            e.cl.rec("delete", (e.cl.lab(block),))
            e.cl.forget(block)

    def op_delete_array(e):
        us(e).heap[3] += 1

    S.imp("__nw__FUl", op_new)
    S.imp("__nwa__FUl", op_new_array)
    S.imp("__dl__FPv", op_delete)
    S.imp("__dla__FPv", op_delete_array)

    def construct(e, call):
        raw, ctor, dtor, size, n = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
        if raw == 0:
            return 0
        w32(e, raw, size)
        w32(e, raw + 4, n)
        base = raw + 8
        for i in range(n):
            call(ctor, base + i * size, 1)
        return base

    def destroy(e, call):
        base, dtor = e.r(3), e.r(4)
        size, n = e.u32(base - 8), e.u32(base - 4)
        for i in range(n - 1, -1, -1):
            call(dtor, base + i * size, MINUS)
        us(e).heap[3] += 1
        return None

    S.imp("__construct_new_array", callback_stub(construct))
    S.imp("__destroy_new_array", callback_stub(destroy))

    R = CL.recorder
    S.internal(CALC_CLIENT_AREA, R("CalcClientArea", "L"))
    S.internal(DEFAULT_CALLBACK, lambda e: None)

    def timer_ctor(e):
        us(e).timer_ctors += 1
        w32(e, e.r(3) + 4, us(e).timer_ctors)

    S.internal(TIMER_CTOR, timer_ctor)
    S.internal(TRACE, lambda e: e.cl.rec("TRACE", (), (e.cstr(e.r(3))[:159],)))
    S.imp("UpdateLights__7CSystemFv", lambda e: e.cl.rec("UpdateLights"))
    S.imp("Link__7CMapWhoFP12CMapLinkBasei", lambda e: e.cl.rec("Link", (e.cl.lab(e.r(4)), T.s32(e.r(5)))))
    S.internal(FIND_FILE_PATH, find_file_path)
    install_loader_stubs(S)
    return S


def make_fake_vtables(e):
    """The fake cursors', help text's and regions' vtables, in persistent memory."""
    if hasattr(e, "fake_cursor_vtable"):
        return
    real = bytes(e.read(0x102AFA08, 0x28))
    vt = T.persist_alloc(e, 0x28, real)
    for slot, name in ((0x14, "Show"), (0x18, "Hide")):
        tv = T.fake_function(e, (lambda n: lambda emu: emu.cl.rec("Cursor." + n, (emu.cl.lab(emu.r(3)),)))(name))
        e.write(vt + slot, struct.pack(">I", tv))
    tv = T.fake_function(e, lambda emu: emu.cl.rec("Cursor.Use", (emu.cl.lab(emu.r(3)), s16(emu.r(4)))))
    e.write(vt + 0x1C, struct.pack(">I", tv))
    e.fake_cursor_vtable = vt

    vt = T.persist_alloc(e, 0x20)
    for slot, name in ((0x14, "Enable"), (0x18, "Disable")):
        tv = T.fake_function(e, (lambda n: lambda emu: emu.cl.rec("Help." + n, (emu.cl.lab(emu.r(3)),)))(name))
        e.write(vt + slot, struct.pack(">I", tv))
    e.fake_help_vtable = vt

    def region_point_in(emu):
        this, x, y = emu.r(3), s16(emu.r(4)), s16(emu.r(5))
        emu.cl.rec("Region.PointIn", (emu.cl.lab(this), x, y))
        k = emu.cl.lab(this) - LAB_REGION
        emu.set_r(3, 1 if ((x * 7 + y * 3 + emu.us.seeds[k]) & 3) != 0 else 0)

    vt = T.persist_alloc(e, 0x20)
    e.write(vt + 8, struct.pack(">I", T.fake_function(e, region_point_in)))
    e.fake_region_vtable = vt
    e.fake_peek = [T.fake_function(e, lambda emu: None) for _ in range(3)]
    e.other_callback = T.fake_function(e, lambda emu: None)


def find_file_path(e):
    us = e.us
    name = e.cstr(e.r(3))
    e.cl.rec("FindFilePath", (e.r(4), int(e.r(5) != 0)), (name[:159],))
    last = name.rsplit(":", 1)[-1].lower()
    for picture in us.pictures:
        if picture["name"].lower() == last:
            us.current = picture
            e.set_r(3, picture["path_address"])
            return
    e.set_r(3, 0)


def install_loader_stubs(S):
    """The picture loaders, the platform's pixels and cursor: each records what it was asked and does what the
    host's fake does."""
    us = lambda e: e.us   # noqa: E731
    imp = S.imp

    def tga_ctor(e):
        e.cl.rec("TGALoader.ctor", (e.cl.newlab(e.r(3)),))
        w32(e, e.r(3) + 0x42C, 0)

    imp("__ct__11TbTGALoaderFv", tga_ctor)
    imp("__dt__11TbTGALoaderFv", lambda e: e.cl.rec("TGALoader.dtor", (e.cl.lab(e.r(3)),)))

    def area_ctor(e):
        e.cl.rec("RenderArea.ctor", (e.cl.newlab(e.r(3)),))
        e.write(e.r(3) + 0x10, bytes(24))

    imp("__ct__12TbRenderAreaFv", area_ctor)

    def alloc_memory(e):
        area, store = e.r(4), us(e).pixel_store
        width, depth, height = T.s32(e.u32(area + 0x1C)), T.s32(e.u32(area + 0x10)), T.s32(e.u32(area + 0x14))
        stride = T.s32(width * ((depth + 7) >> 3))
        w32(e, area + 0x24, stride)
        e.cl.rec("AllocMemory", (e.cl.lab(area), stride, height))
        e.write(store, bytes(1 << 16))
        w32(e, area + 0x18, store)
        us(e).pixel_base = store
        w32(e, e.r(3), 0)

    imp("AllocMemory__12TbRenderAreaFv", alloc_memory)

    def read_header(e):
        ret, loader, path, second, area = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
        e.cl.rec("ReadHeader", (e.cl.lab(loader), e.cl.lab(area), int(second == 0)), (e.cstr(path)[:159],))
        p = us(e).current
        w32(e, area + 0x10, p["depth"])
        w32(e, area + 0x14, p["height"])
        w32(e, area + 0x1C, p["width"])
        w32(e, loader + 0x42C, int(p["has_palette"]))
        if p["has_palette"]:
            e.write(loader + 0x2C, p["palette"])
        w32(e, ret, p["header_code"])

    imp("ReadHeader__19TbGraphicFileLoaderFPCcPCcP12TbRenderArea", read_header)

    def load(e):
        ret, loader, area, path, second, other = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7), e.r(8)
        e.cl.rec("Load", (e.cl.lab(loader), e.cl.lab(area), int(second == 0), int(other == 0)), (e.cstr(path)[:159],))
        p = us(e).current
        stride, height, data = T.s32(e.u32(area + 0x24)), T.s32(e.u32(area + 0x14)), e.u32(area + 0x18)
        wanted = max(0, stride * height)
        pixels = p["pixels"][:min(len(p["pixels"]), wanted)]
        if data and pixels:
            e.write(data, pixels)
        w32(e, ret, p["load_code"])

    imp("Load__19TbGraphicFileLoaderFRC12TbRenderAreaPCcPCcP12TbRenderArea", load)

    def info(e, obj):
        found = us(e).info.get(obj)
        if found is None:
            found = us(e).info[obj] = dict(source=0, buffer=e.alloc(0x400, bytes(0x400)))
        return found

    def fresh(e, obj, source):
        us(e).info[obj] = dict(source=source, buffer=e.alloc(0x400, bytes(0x400)))

    imp("__ct__Q213SamsUtilities7UPixelsFv", lambda e: e.cl.rec("UPixels.ctor", (e.cl.newlab(e.r(3)),)))

    def pixels_alloc(e):
        this, fmt, width, height, palette = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
        e.cl.rec("UPixels.alloc", (e.cl.newlab(this), T.s32(fmt), T.s32(width), T.s32(height), int(palette == 0)))
        fresh(e, this, 0)

    imp("__ct__Q213SamsUtilities7UPixelsFQ213SamsUtilities11PixelFormatUlUlPQ213SamsUtilities6UPixel", pixels_alloc)

    def pixels_wrap(e):
        this, fmt, data, width, height, stride, palette = [e.r(i) for i in range(3, 10)]
        e.cl.rec("UPixels.wrap", (e.cl.newlab(this), T.s32(fmt), checksum(e.read(data, 0x400)), T.s32(width), T.s32(height),
                                  T.s32(stride), int(palette == 0)))
        fresh(e, this, data)

    imp("__ct__Q213SamsUtilities7UPixelsFQ213SamsUtilities11PixelFormatPvUlUlUlPQ213SamsUtilities6UPixel", pixels_wrap)
    imp("__dt__Q213SamsUtilities7UPixelsFv", lambda e: e.cl.rec("UPixels.dtor", (e.cl.lab(e.r(3)),)))

    def convert(e):
        this, source, dither = e.r(3), e.r(4), e.r(5) & 0xFF
        e.cl.rec("UPixels.Convert", (e.cl.lab(this), e.cl.lab(source), dither))
        to, frm = info(e, this), info(e, source)
        data = bytes(b ^ 0x5A for b in e.read(frm["source"], 0x400)) if frm["source"] else bytes(0x400)
        e.write(to["buffer"], data)

    imp("Convert__Q213SamsUtilities7UPixelsFRCQ213SamsUtilities7UPixelsUc", convert)

    def get_data(e):
        e.cl.rec("UPixels.GetData", (e.cl.lab(e.r(3)),))
        e.set_r(3, info(e, e.r(3))["buffer"])

    imp("GetData__Q213SamsUtilities7UPixelsFv", get_data)

    def set_data(e):
        this, fmt, data, width, height, stride, palette = [e.r(i) for i in range(3, 10)]
        e.cl.rec("UPixels.SetData", (e.cl.lab(this), T.s32(fmt), data - us(e).pixel_base, T.s32(width), T.s32(height), T.s32(stride),
                                     checksum(e.read(data, 64)), 0 if palette == 0 else checksum(e.read(palette, 0x400))))

    imp("SetData__Q213SamsUtilities7UPixelsFQ213SamsUtilities11PixelFormatPvUlUlUlPQ213SamsUtilities6UPixel", set_data)
    imp("SetTransparentColour__Q213SamsUtilities7UPixelsFQ213SamsUtilities6UPixel",
        lambda e: e.cl.rec("UPixels.Transparent", (e.cl.lab(e.r(3)), e.r(4))))

    def cursor_gl(e):
        this, point, frames, count, context = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
        x, y = struct.unpack(">ii", e.read(point, 8))
        e.cl.rec("UCursorOpenGL", (e.cl.newlab(this), x, y, e.cl.lab(frames), T.s32(count), int(context == 0)))

    imp("__ct__Q213SamsUtilities13UCursorOpenGLFRQ213SamsUtilities6UPointPQ213SamsUtilities7UPixelslPQ213SamsUtilities7UOpenGL",
        cursor_gl)
    imp("SetRate__Q213SamsUtilities7UCursorFRCQ213SamsUtilities13UMicroseconds",
        lambda e: e.cl.rec("SetRate", (e.cl.lab(e.r(3)), struct.unpack(">q", e.read(e.r(4), 8))[0])))


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_uiinit_open", False):
        return
    lib.tpw_uiinit_op.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uiinit_op.restype = C.c_int64
    lib.tpw_uiinit_table.argtypes = [C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_uiinit_region_seed.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_uiinit_expect.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_uiinit_dump_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uiinit_dump_window.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uiinit_dump_queue.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uiinit_dump_timed.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uiinit_dump_timers.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_uiinit_heap.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uiinit_picture.argtypes = [C.c_char_p, C.c_char_p, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32,
                                       C.c_char_p, C.c_char_p, C.c_int32]
    lib.tpw_uiinit_cursor_op.argtypes = [C.c_int32, C.c_char_p, C.POINTER(C.c_int64)]
    lib.tpw_uiinit_cursor_op.restype = C.c_int64
    lib.tpw_uiinit_dump_bfcursor.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uiinit_dump_folder.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uiinit_dump_table.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_uiinit_dump_light.argtypes = [C.POINTER(C.c_uint32)]
    lib._uiinit_open = True


def ints(*values):
    return (C.c_int64 * 8)(*values, *([0] * (8 - len(values))))


class Host:
    """One case on the host side: the same operations through the library."""

    def __init__(self, lib, case):
        self.lib = lib
        open_host(lib)
        lib.tpw_uiinit_reset()
        CL.host_begin(lib, {}, case.get("q"))
        lib.tpw_uiinit_labels()
        C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
        for k, (count, mask) in enumerate(case.get("tables", ((0, 0), (0, 0)))):
            lib.tpw_uiinit_table(k, count, mask)
        for k, seed in enumerate(case.get("seeds", (0, 0, 0))):
            lib.tpw_uiinit_region_seed(k, seed)
        lib.tpw_uiinit_light_seed()
        for p in case.get("pictures", ()):
            lib.tpw_uiinit_picture(p["name"].encode(), p["path"].encode(), p["depth"], p["width"], p["height"], p["header_code"],
                                   p["load_code"], int(p["has_palette"]), p["palette"], p["pixels"], len(p["pixels"]))
        lib.tpw_uiinit_heap_clear()
        C.c_int32.in_dll(lib, "tpw_cl_n").value = 0
        C.c_int32.in_dll(lib, "tpw_cl_on").value = 1

    def op(self, code, a, text=None):
        lib = self.lib
        if code >= 41:
            return lib.tpw_uiinit_cursor_op({41: 1, 42: 2, 43: 3, 44: 4, 45: 5, 46: 6}[code], (text or "").encode(), ints(*a))
        return lib.tpw_uiinit_op(code, ints(*a))

    def log_length(self):
        return C.c_int32.in_dll(self.lib, "tpw_cl_n").value

    def queue(self, which):
        buf = (C.c_int64 * 4096)()
        self.lib.tpw_uiinit_dump_queue(which, MAX_ENTRIES, buf)
        if buf[0] == 0:
            return [0]
        shown = max(0, min(buf[2], MAX_ENTRIES)) if buf[1] else 0
        return list(buf[:6 + (4 * shown + 1 if buf[1] else 0)])

    def timed(self, which):
        buf = (C.c_int64 * 4096)()
        self.lib.tpw_uiinit_dump_timed(which, MAX_ENTRIES, buf)
        if buf[0] == 0:
            return [0]
        shown = max(0, min(buf[2], MAX_ENTRIES)) if buf[1] else 0
        return list(buf[:3 + 5 * shown])

    def dump_state(self, step, timers):
        """The same dump as Ppc.dump_state; `step` says which slots and objects exist."""
        lib = self.lib
        buf = (C.c_int64 * 4096)()
        lib.tpw_uiinit_dump_globals(buf)
        state = dict(globals=list(buf[:29]))
        windows = []
        for slot in range(SLOTS):
            if step["made"][slot]:
                lib.tpw_uiinit_dump_window(slot, buf)
                windows.append(list(buf[:WINDOW_FIELDS]))
            else:
                windows.append([0])
        state["windows"] = windows
        root_label = state["globals"][0]
        state["root"] = [0]
        if root_label != 0 and root_label not in range(LAB_WINDOW, LAB_WINDOW + SLOTS):
            lib.tpw_uiinit_dump_window(-1, buf)
            state["root"] = list(buf[:WINDOW_FIELDS])
        state["msg_queue"] = self.queue(0)
        state["draw_queue"] = self.queue(1)
        state["queues"] = [self.queue(2 + k) if step["queue_made"][k] else [0] for k in range(2)]
        state["timed"] = self.timed(0)
        state["timed_local"] = self.timed(1) if step["timed_made"] else [0]
        light = (C.c_uint32 * 8)()
        lib.tpw_uiinit_dump_light(light)
        state["light"] = list(light)
        heap = (C.c_int64 * 4)()
        lib.tpw_uiinit_heap(heap)
        state["heap"] = list(heap)
        state["log"] = self.log_length()
        if timers:
            lib.tpw_uiinit_dump_timers(300, buf)
            count = min(max(state["globals"][20], 0), 300) if state["globals"][19] else 0
            state["timers"] = list(buf[:count])
        return state

    def dump_cursor_state(self, step):
        lib = self.lib
        buf = (C.c_int64 * 4096)()
        heap = (C.c_int64 * 4)()
        lib.tpw_uiinit_heap(heap)
        out = dict(heap=list(heap), log=self.log_length())
        lib.tpw_uiinit_dump_bfcursor(buf)
        out["cursor"] = list(buf[:7]) if step["bf_made"] else [0]
        lib.tpw_uiinit_dump_folder(buf)
        out["folder"] = list(buf[:2])
        if step["table_made"]:
            lib.tpw_uiinit_dump_table(buf)
            count, dump, at = buf[0], [buf[0]], 1
            for _ in range(count):
                dump.append(buf[at])
                at += 1
                if buf[at - 1] != 0:
                    dump += [buf[at], buf[at + 1], buf[at + 2]]
                    at += 3
            out["table"] = dump
        else:
            out["table"] = [0]
        return out


# ======================================================================= running a script


RAN = {}   # what the PowerPC side decided to run, for the host side (by case)


def with_stubs(fn):
    def run(e, case):
        saved = STUBS.activate(e)
        try:
            return fn(e, case)
        finally:
            CL.Stubs.deactivate(e, saved)
    return run


def kinds_expected(host, kinds):
    host.lib.tpw_uiinit_expect(-1, 0)
    # the host's observer expects the array kinds in the order the original asked for them
    for kind in kinds:
        host.lib.tpw_uiinit_expect(kind, 1)


@with_stubs
def ui_ppc(e, case):
    world = Ppc(e, case)
    steps, ran = [], []
    try:
        for op in case["ops"]:
            code, a = op[0], op[1]
            outcome = world.op(code, a, op[2] if len(op) > 2 else None)
            if outcome is None:
                continue
            answer, timers, kinds = outcome
            ran.append(dict(op=op, kinds=kinds, timers=timers, made=list(world.made), queue_made=list(world.queue_made),
                            timed_made=world.timed_made))
            steps.append(dict(op=code, answer=answer, state=world.dump_state(timers)))
        RAN[id(case)] = ran
        return dict(steps=steps, log=list(world.L.log))
    finally:
        world.restore()


def ui_host(lib, case):
    host = Host(lib, case)
    steps = []
    try:
        for step in RAN.pop(id(case)):
            op = step["op"]
            kinds_expected(host, step["kinds"])
            answer = host.op(op[0], op[1])
            steps.append(dict(op=op[0], answer=answer, state=host.dump_state(step, step["timers"])))
    finally:
        log = CL.host_end(lib)
    return dict(steps=steps, log=log)


@with_stubs
def cursor_ppc(e, case):
    world = Ppc(e, case)
    steps, ran = [], []
    try:
        for op in case["ops"]:
            code, a = op[0], op[1]
            outcome = world.op(code, a, op[2] if len(op) > 2 else None)
            if outcome is None:
                continue
            answer, _, kinds = outcome
            ran.append(dict(op=op, kinds=kinds, bf_made=world.bf_made, table_made=world.table_made))
            steps.append(dict(op=code, answer=answer, state=world.dump_cursor_state()))
        RAN[id(case)] = ran
        return dict(steps=steps, log=list(world.L.log))
    finally:
        world.restore()


def cursor_host(lib, case):
    host = Host(lib, case)
    steps = []
    try:
        for step in RAN.pop(id(case)):
            op = step["op"]
            kinds_expected(host, step["kinds"])
            answer = host.op(op[0], op[1], op[2] if len(op) > 2 else None)
            steps.append(dict(op=op[0], answer=answer, state=host.dump_cursor_state(step)))
    finally:
        log = CL.host_end(lib)
    return dict(steps=steps, log=log)


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    make_fake_vtables(e)
    STUBS = build_stubs(e)


# ======================================================================= scripts: building blocks

TESTS = []

ATTRIBUTES = [0, 1, 2, 3, 0x400, 0x401, 0x10000000, 0x10000001, 0xF0000000, 0xF0000001, 0x80000000, 0x100, 0x7FFFFFFF, 0xFFFFFFFF,
              0x402, 0x20000000, 0x4000000, 0x1000]
IDS = [0, 1, -1, 0x12345678, -0x29A, 0x7FFFFFFF, -0x80000000, 77]
MESSAGES = [0, 1, 2, 3, 4, 0x15, 0x10001, 0x10002, 0x10003, 0x1000A, 0x1000B, 0x1000C, 0x1000D, 0x10009, 0x1000, -1, 0x7FFFFFFF,
            -0x80000000, 0x20, 0x100, 0x800, 0x10000, 0x10004]
VALUES = [0, 1, -1, 2, 0x12345678, -0x7FFFFFFF, 0x7FFFFFFF, 0x10000, 0xFFFF, -0x80000000, 7, 0x20010]
COUNTS = [0, 1, 2, 3, 4, 5, 7, 8, 16, 40, 41, 64, 100]
TIMED_COUNTS = [0, 1, 2, 3, 63, 64, 65, 0x1FF, 0x200, 0x201, 1000]


def window_ops(parents, rects, attributes, regions=None, destroyed=None, ids=None):
    """Makes windows 0.. with the given parents (slots, -1 for none), rectangles and attributes, sets each one's fields
    and links the tree."""
    ops = []
    n = len(parents)
    for slot in range(n):
        ops.append((1, (slot, parents[slot], attributes[slot], ids[slot] if ids else slot * 11 - 3) + tuple(rects[slot])))
    for slot in range(n):
        ops.append((3, (slot, attributes[slot], destroyed[slot] if destroyed else 0, regions[slot] if regions else 0)))
    ops += link_ops(parents)
    return ops


def link_ops(parents):
    ops = []
    for slot in range(len(parents)):
        siblings = [t for t in range(len(parents)) if parents[t] == parents[slot]]
        at = siblings.index(slot)
        previous = siblings[at - 1] if at > 0 else -1
        following = siblings[at + 1] if at + 1 < len(siblings) else -1
        children = [t for t in range(len(parents)) if parents[t] == slot]
        ops.append((2, (slot, parents[slot], previous, following, children[0] if children else -1)))
    return ops


def rnd_rect(r, around=150):
    left, top = r.randint(-30, around), r.randint(-30, around)
    return (left, top, left + r.choice([0, 1, 2, r.randint(-5, 200), r.randint(1, 100)]),
            top + r.choice([0, 1, 2, r.randint(-5, 200), r.randint(1, 100)]))


def rnd_attribute(r):
    k = r.random()
    if k < 0.6:
        return r.choice(ATTRIBUTES)
    return r.getrandbits(32)


def rnd_windows(r, count=None):
    """Random windows: a forest (a parent always has the lower number), rectangles, attributes, regions."""
    n = count or r.randint(1, SLOTS)
    parents = [-1 if r.random() < 0.3 or s == 0 else r.randrange(s) for s in range(n)]
    rects = [rnd_rect(r) for _ in range(n)]
    attributes = [rnd_attribute(r) for _ in range(n)]
    regions = [r.choice([0, 0, 0, 1, 2, 3]) for _ in range(n)]
    destroyed = [r.choice([0, 0, 0, 0, 1, 0x80, 0xFF]) for _ in range(n)]
    ids = [r.choice(IDS) for _ in range(n)]
    return window_ops(parents, rects, attributes, regions, destroyed, ids), n


def rnd_tables(r):
    return tuple((r.choice([0, 1, 3, 6, 0x16]), r.getrandbits(22)) for _ in range(TABLES))


def base_case(r=None):
    case = dict(tables=((6, 0x3F), (4, 0b0101)), seeds=(0, 1, 5))
    if r is not None:
        case = dict(tables=rnd_tables(r), seeds=tuple(r.randrange(8) for _ in range(REGIONS)))
    return case


def case_of(ops, r=None, **extra):
    case = base_case(r)
    case.update(extra)
    case["ops"] = ops
    return case


def queue_setup(counts=(8, 8, 4), assign=True):
    """Both message queues and the timed queue made and sized, and made the UI's own."""
    ops = [(13, (0,)), (13, (1,)), (13, (2,)), (14, (0, counts[0])), (14, (1, counts[1])), (14, (2, counts[2]))]
    if assign:
        ops += [(4, (8, 0)), (4, (7, 1)), (4, (9, 0))]
    return ops


# ======================================================================= test 1: the queues


def queue_edges():
    windows = [(1, (0, -1, 1, 5, 0, 0, 50, 50)), (1, (1, -1, 1, 6, 10, 10, 80, 80))]
    for count in COUNTS:
        ops = list(windows) + [(13, (0,)), (16, (0,)), (14, (0, count))]
        for i in range(count + 3):
            ops.append((15, (0, [0, -1, 1][i % 3], MESSAGES[i % len(MESSAGES)], VALUES[i % len(VALUES)], i)))
        ops += [(16, (0,)), (15, (0, 0, 1, 2, 3)), (15, (0, 1, 4, 5, 6)), (14, (0, count)), (13, (1,)), (14, (1, count)),
                (15, (1, 1, -1, -2, -3)), (16, (1,)), (13, (0,)), (16, (0,)), (14, (0, 2)), (15, (0, 0, 9, 9, 9))]
        yield case_of(ops)
    # an uninitialised queue, flushed and refused; the wrap round the ring
    yield case_of(windows + [(13, (0,)), (16, (0,)), (15, (0, 0, 1, 1, 1)), (14, (0, 3)), (15, (0, 0, 1, 1, 1)), (15, (0, 1, 2, 2, 2)),
                             (15, (0, 0, 3, 3, 3)), (15, (0, 1, 4, 4, 4)), (16, (0,)), (15, (0, 1, 5, 5, 5)), (15, (0, 1, 6, 6, 6)),
                             (15, (0, 1, 7, 7, 7)), (15, (0, 1, 8, 8, 8))])
    for count in TIMED_COUNTS:
        yield case_of([(13, (2,)), (16, (2,)), (14, (2, count)), (16, (2,)), (14, (2, count)), (16, (2,)), (13, (2,)), (14, (2, 3)),
                       (16, (2,))])
    # the queue made a second time over a used one
    yield case_of(windows + [(13, (0,)), (14, (0, 3)), (15, (0, 0, 1, 2, 3)), (13, (0,)), (16, (0,)), (14, (0, 5)), (15, (0, 0, 1, 2, 3))])


def queue_random(r):
    ops, n = rnd_windows(r, r.randint(1, 3))
    for _ in range(r.randint(4, 24)):
        k = r.random()
        which = r.choice([0, 0, 1, 2])
        if k < 0.2:
            ops.append((13, (which,)))
        elif k < 0.4:
            ops.append((14, (which, r.choice(TIMED_COUNTS if which == 2 else COUNTS))))
        elif k < 0.85:
            ops.append((15, (which % 2, r.randint(-1, n - 1), r.choice(MESSAGES), r.choice(VALUES), r.choice(VALUES))))
        else:
            ops.append((16, (which,)))
    return case_of(ops, r)


TESTS.append(FnTest("InterfaceMessageQueue ctor, Initialise, Add, Flush; InterfaceTimedMessages ctor, Initialise, Flush", ui_ppc, ui_host,
                    edge=queue_edges, rnd=queue_random))


# ======================================================================= test 2: the system, posting, the timer table


def system_edges():
    # UI_InitSystem: sizes and the Hebrew flag; again over what the first made; with a queue already the UI's
    yield case_of([(5, (0x80, 0, 1)), (8, ()), (5, (0, 0, 0))])
    for size in (1, 2, 3, 16, 0x80, 0x800):
        for hebrew in (0, 1):
            yield case_of([(5, (0x80, size, hebrew)), (1, (0, -1, 0x401, 5, 0, 0, 50, 50)), (7, (0, 0x10005, 1, 2)),
                           (7, (0, 1, 3, 4)), (7, (0, 0x1000A, 5, 6)), (5, (0, 3, 1 - hebrew)), (8, ())])
    yield case_of([(5, (0, 4, 1)), (5, (1, 4, 0)), (5, (2, 0x800, 1))])
    yield case_of([(1, (0, -1, 0x401, 5, 0, 0, 2047, 1535)), (1, (1, 0, 0x1, 6, 5, 5, 50, 50))] + queue_setup((4, 4, 4)) +
                  [(4, (1, 0)), (4, (2, 1)), (9, (10, 10)), (5, (0, 8, 0)), (9, (11, 11)), (8, ())])
    for n in (0, 1, 2, 5, 0x100):
        yield case_of([(6, (n,)), (6, (n,)), (6, (0x100,))])
    # UI_PostMessage: every kind of message to a window that is fine, NULL, being destroyed, with and without queues
    windows = [(1, (0, -1, 1, 5, 0, 0, 50, 50)), (1, (1, -1, 1, 6, 10, 10, 80, 80)), (3, (1, 1, 1, 0)), (3, (0, 1, 0, 0))]
    for draw in (0, 1):
        for general in (0, 1):
            ops = list(windows) + [(13, (0,)), (13, (1,)), (14, (0, 3)), (14, (1, 3))]
            if general:
                ops.append((4, (8, 0)))
            if draw:
                ops.append((4, (7, 1)))
            for message in MESSAGES:
                for slot in (0, 1, -1):
                    ops.append((7, (slot, message, 11, 22)))
            ops += [(8, ()), (7, (0, 1, 1, 1)), (7, (0, 4, 1, 1))]
            yield case_of(ops)
    # a full queue answers 1 and keeps its messages
    yield case_of(windows + queue_setup((2, 2, 2)) + [(7, (0, 1, 1, 1)), (7, (0, 1, 2, 2)), (7, (0, 1, 3, 3)), (7, (0, 4, 4, 4)),
                                                      (7, (0, 4, 5, 5)), (7, (0, 4, 6, 6)), (7, (0, 0x1000B, 7, 7)), (8, ()),
                                                      (7, (0, 4, 8, 8))])
    # UI_FlushAllMessages: every combination of the three being there
    for draw in (0, 1):
        for general in (0, 1):
            for timed in (0, 1):
                ops = list(windows) + [(13, (0,)), (13, (1,)), (13, (2,)), (14, (0, 4)), (14, (1, 4)), (14, (2, 4))]
                if general:
                    ops.append((4, (8, 0)))
                if draw:
                    ops.append((4, (7, 1)))
                if timed:
                    ops.append((4, (9, 0)))
                ops += [(7, (0, 1, 1, 1)), (7, (0, 4, 2, 2)), (8, ()), (7, (0, 4, 3, 3)), (7, (0, 2, 4, 4)), (8, ())]
                yield case_of(ops)


def system_random(r):
    ops, n = rnd_windows(r, r.randint(1, 3))
    if r.random() < 0.6:
        ops += queue_setup((r.choice([2, 4, 8, 20]), r.choice([2, 4, 8, 20]), r.choice([1, 8, 40])),
                           assign=r.random() < 0.8)
    for _ in range(r.randint(4, 20)):
        k = r.random()
        if k < 0.15:
            ops.append((5, (r.choice(VALUES), r.choice([0, 1, 2, 4, 8, 16, 40]), r.randrange(2))))
        elif k < 0.22:
            ops.append((6, (r.choice([0, 1, 2, 7, 0x100]),)))
        elif k < 0.6:
            ops.append((7, (r.randint(-1, n - 1), r.choice(MESSAGES), r.choice(VALUES), r.choice(VALUES))))
        elif k < 0.75:
            ops.append((8, ()))
        elif k < 0.85:
            ops.append((4, (r.choice([7, 8, 9]), r.choice([-1, 0, 1]))))
        elif k < 0.92:
            ops.append((1, (r.randrange(SLOTS), r.randint(-1, n - 1), r.choice(ATTRIBUTES), r.choice(IDS)) + rnd_rect(r)))
        else:
            ops.append((4, (r.choice([1, 2, 3]), r.randint(-1, n - 1))))
    return case_of(ops, r)


TESTS.append(FnTest("UI_InitSystem, UI_InitTimerSystem, UI_PostMessage, UI_FlushAllMessages", ui_ppc, ui_host, edge=system_edges,
                    rnd=system_random))


# ======================================================================= test 3: the window, PointIn, the pointer

EDGE_X = [-1, 0, 1, 9, 10, 11, 49, 50, 51, 99, 100, 101]


def point_ops(slot, rect):
    """PointIn at the corners and edges of a rectangle, and one point well outside."""
    left, top, right, bottom = rect
    ops = []
    for x in (left - 1, left, left + 1, right - 1, right, right + 1):
        for y in (top - 1, top, bottom - 1, bottom):
            ops.append((11, (slot, x, y)))
    ops.append((11, (slot, 32767, -32768)))
    return ops


def pointer_edges():
    root = (1, (0, -1, 0xF0000001, -0x29A, 0, 0, 200, 200))
    # PointIn: one window with each attribute, a point in and out of it
    for attributes in ATTRIBUTES + [0x40000003, 0x8]:
        for destroyed in (0, 1):
            for region in (0, 1, 2, 3):
                ops = [(1, (0, -1, attributes, 5, 10, 10, 50, 40)), (3, (0, attributes, destroyed, region))]
                ops += point_ops(0, (10, 10, 50, 40))
                yield case_of(ops, seeds=(0, 1, 6))
    # the children answer in order and the last that answers wins; the window itself only when none does
    for children in ((0, 0), (0, 1), (1, 0), (1, 1), (2, 0)):
        for attributes in (1, 0x10000000, 3, 2):
            parents = [-1, 0, 0, 1, 2, 4]
            rects = [(0, 0, 100, 100), (10, 10, 60, 60), (40, 40, 90, 90), (15, 15, 30, 30), (50, 50, 80, 80), (55, 55, 75, 75)]
            attrs = [1, attributes, 1, 1, children[0] and 1 or 0x10000000, 1]
            regions = [0, 0, children[1], 0, 0, 0]
            ops = window_ops(parents, rects, attrs, regions)
            for slot in range(6):
                for x, y in ((12, 12), (45, 45), (52, 52), (57, 57), (95, 95), (-5, -5), (20, 20)):
                    ops.append((11, (slot, x, y)))
            yield case_of(ops, seeds=(0, 2, 5))
    # a window destroyed or disabled in the middle of a chain hides what is under it
    for broken in range(4):
        for how in (2, 0x80):
            parents = [-1, 0, 1, 2]
            rects = [(0, 0, 100, 100)] * 4
            attrs = [1, 1, 1, 1]
            attrs[broken] = how if how == 2 else 1
            destroyed = [0, 0, 0, 0]
            if how != 2:
                destroyed[broken] = 1
            ops = window_ops(parents, rects, attrs, None, destroyed)
            ops += [(11, (s, 50, 50)) for s in range(4)]
            yield case_of(ops)
    # MakeRectAbsolute: chains of every depth and rectangles that overflow
    for depth in range(0, 6):
        for rect in ((0, 0, 10, 10), (5, -7, 20, 30), (32760, 32760, 32767, 32767), (-32768, -32768, 0, 0), (0, 0, 0, 0), (3, 4, 2, 1)):
            parents = [-1] + list(range(depth))
            rects = [(1, 2, 100, 100), (10, 20, 90, 90), (-3, 5, 50, 50), (7000, 7000, 9000, 9000), (32000, 32000, 32767, 32767),
                     (-9, -9, 9, 9)][:depth + 1]
            attrs = [0] * (depth + 1)
            ops = window_ops(parents, rects, attrs)
            ops += [(12, (s,) + rect) for s in range(depth + 1)]
            yield case_of(ops)
    # the pointer: every combination of root, window the pointer is in, device capture and activity
    layout = window_ops([-1, 0, 0, 1], [(0, 0, 200, 200), (10, 10, 100, 100), (110, 10, 190, 100), (20, 20, 60, 60)], [0xF0000001, 1, 1, 1])
    for active in (0, 1, -1):
        for root in (-1, 0, 2):
            for inside in (-1, 0, 1, 3):
                for capture in (-1, 0, 2):
                    ops = list(layout) + queue_setup((32, 32, 4)) + [(4, (4, active)), (4, (1, root)), (4, (2, inside)),
                                                                     (4, (3, capture))]
                    ops += [(9, (30, 30)), (9, (30, 30)), (9, (120, 20)), (9, (-5, -5)), (10, ()), (9, (25, -7)), (4, (3, -1)),
                            (9, (25, 25)), (10, ()), (4, (1, -1)), (9, (1, 1)), (10, ())]
                    yield case_of(ops)
    # negative y fills the high half of the position with ones; both halves at the extremes
    ops = list(layout) + queue_setup((16, 16, 4)) + [(4, (1, 0)), (9, (32767, -32768)), (9, (-32768, 32767)), (9, (-1, -1)), (9, (0, 0)),
                                                    (9, (1, -1)), (4, (5, -3)), (4, (6, -9)), (10, ()), (4, (2, -1)), (10, ())]
    yield case_of(ops)


def pointer_random(r):
    ops, n = rnd_windows(r, r.randint(2, SLOTS))
    ops += queue_setup((r.choice([2, 4, 16, 32]), r.choice([2, 4, 16, 32]), 4), assign=r.random() < 0.9)
    ops.append((4, (1, r.randint(-1, n - 1))))
    for _ in range(r.randint(6, 24)):
        k = r.random()
        px, py = r.randint(-20, 220), r.randint(-20, 220)
        if k < 0.35:
            ops.append((9, (px, py)))
        elif k < 0.45:
            ops.append((10, ()))
        elif k < 0.6:
            ops.append((11, (r.randrange(n), px, py)))
        elif k < 0.7:
            ops.append((12, (r.randrange(n),) + rnd_rect(r)))
        elif k < 0.78:
            ops.append((4, (r.choice([1, 2, 3]), r.randint(-1, n - 1))))
        elif k < 0.84:
            ops.append((4, (4, r.choice([0, 1, 1, 1, 2, -1]))))
        elif k < 0.9:
            ops.append((3, (r.randrange(n), rnd_attribute(r), r.choice([0, 0, 1, 0xFF]), r.choice([0, 0, 1, 2, 3]))))
        elif k < 0.94:
            ops.append((1, (r.randrange(n), r.randint(-1, n - 1), rnd_attribute(r), r.choice(IDS)) + rnd_rect(r)))
        elif k < 0.97:
            ops.append((8, ()))
        else:
            ops += link_ops([-1 if s == 0 or r.random() < 0.3 else r.randrange(s) for s in range(n)])
    return case_of(ops, r)


TESTS.append(FnTest("PointIn, MakeRectAbsolute, UI_DevicePointerMove, UI_UpdatePointerIn", ui_ppc, ui_host, edge=pointer_edges,
                    rnd=pointer_random))


# ---- test 3b: the mouse buttons (UI_DeviceButtonDown, UI_DeviceButtonUp)

BUTTON_XY = [(0, 0), (1, -1), (-1, 1), (-1, -1), (32767, -32768), (-32768, 32767), (100, 50), (0x1234, -0x4321), (0, -32768),
             (-32768, 0), (255, 256), (-256, -255)]
BUTTON_FLAGS = [0, 1, 2, 4, 7, 0x8000, -1, 0x5555, -0x5556, 0x00FF, -0x100]


def button_layout():
    """A root and two windows, the queues, the pointer in window 1."""
    ops = window_ops([-1, 0, 0], [(0, 0, 200, 200), (10, 10, 100, 100), (110, 10, 190, 100)], [0xF0000001, 1, 1])
    return ops + queue_setup((64, 64, 4)) + [(4, (1, 0)), (4, (2, 1))]


def button_edges():
    # every button down and up at the coordinates that matter, with every kind of flags word and table
    for flags in BUTTON_FLAGS:
        for button in range(3):
            for x, y in BUTTON_XY:
                ops = button_layout() + [(4, (12, flags)), (30, (button, x, y)), (30, (button, y, x)), (31, (button, x, y)),
                                         (31, (button, y, x))]
                yield case_of(ops)
    # the UI not active: nothing happens, whatever the state (a table entry set, flags set)
    for active in (0, 1, -1, 2, 0x10000):
        for button in range(3):
            ops = button_layout() + [(4, (4, active)), (4, (12, -1)), (4, (17 + button, 2)), (30, (button, 5, -5)), (31, (button, 5, -5)),
                                     (30, (button, 5, -5)), (4, (4, 1)), (30, (button, 7, 7)), (4, (4, active)), (31, (button, 7, 7))]
            yield case_of(ops)
    # Down with the pointer in no window (NULL posted), in the root, in a window with the destroying byte set; Up for a button never down
    for inside in (-1, 0, 1, 2):
        for destroying in (0, 1):
            ops = button_layout() + [(3, (inside if inside >= 0 else 0, 1, destroying, 0)), (4, (2, inside))]
            ops += [(31, (0, 1, 1)), (30, (0, 2, 2)), (30, (1, 3, -3)), (30, (2, -4, 4)), (31, (1, 5, 5)), (31, (0, 6, 6)),
                    (31, (2, 7, 7)), (31, (2, 8, 8))]
            yield case_of(ops)
    # the table entries are windows, destroyed windows or none: Up posts to the entry and clears it
    for entries in ((-1, -1, -1), (0, 1, 2), (2, 1, 0), (1, -1, 1), (-1, 2, -1)):
        for destroying in (0, 1):
            for button in range(3):
                ops = button_layout() + [(4, (17, entries[0])), (4, (18, entries[1])), (4, (19, entries[2])),
                                         (3, (1, 1, destroying, 0)), (4, (12, 0x0707)), (31, (button, -2, -2)), (31, (button, 9, 9)),
                                         (30, (button, 11, 11)), (31, (0, 12, 12)), (31, (1, 13, 13)), (31, (2, 14, 14))]
                yield case_of(ops)
    # triage: the pointer position sits in the halfwords next to the flags word; a button going down or up must not touch it
    for flags in (0, -1, 0x5555, -0x8000):
        for button in range(3):
            ops = button_layout() + [(4, (5, 0x1234)), (4, (6, -0x4321)), (4, (12, flags)), (30, (button, 1, 2)), (31, (button, 3, 4)),
                                     (30, (button, 5, 6))]
            yield case_of(ops)
    # no message queue: the message is dropped and the table and flags still change
    for button in range(3):
        ops = window_ops([-1, 0], [(0, 0, 200, 200), (10, 10, 100, 100)], [0xF0000001, 1]) + [(4, (1, 0)), (4, (2, 1)),
                                                                                                (30, (button, 1, 2)), (31, (button, 3, 4))]
        yield case_of(ops)
    # a small queue that fills up
    ops = window_ops([-1, 0], [(0, 0, 200, 200), (10, 10, 100, 100)], [0xF0000001, 1]) + queue_setup((2, 2, 4)) + [(4, (1, 0)), (4, (2, 1))]
    for i in range(6):
        ops += [(30, (i % 3, i, -i)), (31, (i % 3, -i, i))]
    yield case_of(ops)
    # buttons down in turn: the flags accumulate and the entries follow the pointer's window
    ops = button_layout() + [(30, (0, 1, 1)), (4, (2, 2)), (30, (1, 2, 2)), (4, (2, -1)), (30, (2, 3, 3)), (31, (1, 4, 4)), (31, (0, 5, 5)),
                             (31, (2, 6, 6))]
    yield case_of(ops)


def button_random(r):
    ops, n = rnd_windows(r, r.randint(2, SLOTS))
    ops += queue_setup((r.choice([2, 4, 16, 32]), r.choice([2, 4, 16, 32]), 4), assign=r.random() < 0.9)
    ops.append((4, (1, r.randint(-1, n - 1))))
    ops.append((4, (4, r.choice([0, 1, 1, 1, 1, 2, -1]))))
    ops.append((4, (12, r.choice(BUTTON_FLAGS + [r.randint(-32768, 32767)]))))
    for i in range(3):
        ops.append((4, (17 + i, r.randint(-1, n - 1))))
    for _ in range(r.randint(4, 20)):
        k = r.random()
        if k < 0.8:
            x, y = r.choice([r.randint(-32768, 32767), r.randint(-20, 220), r.choice([0, -1, 1, 255, 256, -256])]), \
                r.choice([r.randint(-32768, 32767), r.randint(-20, 220), r.choice([0, -1, 1, 255, 256, -256])])
            ops.append((r.choice([30, 31]), (r.randrange(3), x, y)))
        elif k < 0.9:
            ops.append((4, (2, r.randint(-1, n - 1))))
        elif k < 0.95:
            ops.append((4, (4, r.choice([0, 1, 1, 1, 2, -1]))))
        elif k < 0.98:
            ops.append((4, (17 + r.randrange(3), r.randint(-1, n - 1))))
        else:
            ops.append((3, (r.randrange(n), rnd_attribute(r), r.choice([0, 0, 1, 0xFF]), r.choice([0, 0, 1, 2, 3]))))
    return case_of(ops, r)


TESTS.append(FnTest("UI_DeviceButtonDown, UI_DeviceButtonUp", ui_ppc, ui_host, edge=button_edges, rnd=button_random))


# ---- test 3b2: the wheel (UI_DeviceMouseWheel, lead 2026-10-14)

WHEEL_DELTAS = [0, 1, -1, 120, -120, 3, -3, 0x7FFFFFFF, -0x80000000, 0x10000, -0x10000]


def wheel_edges():
    # the capture window or the pointer's window (none, the root, a window), the UI active or not, every delta and position
    for active in (0, 1, -1, 2, 0x10000):
        for capture in (-1, 0, 1, 2):
            for inside in (-1, 0, 1, 2):
                ops = button_layout() + [(4, (4, active)), (4, (3, capture)), (4, (2, inside))]
                ops += [(35, (delta, x, y, flags)) for delta, (x, y), flags in zip(WHEEL_DELTAS, BUTTON_XY, BUTTON_FLAGS)]
                yield case_of(ops)
    # the window that gets it is being destroyed
    for capture in (-1, 1):
        for destroying in (0, 1):
            ops = button_layout() + [(3, (1, 1, destroying, 0)), (4, (3, capture)), (35, (120, 4, 4, 0)), (35, (-120, -4, 4, 0))]
            yield case_of(ops)
    # no message queue, and a small queue that fills up
    ops = window_ops([-1, 0], [(0, 0, 200, 200), (10, 10, 100, 100)], [0xF0000001, 1]) + [(4, (1, 0)), (4, (2, 1)), (35, (1, 2, 3, 4))]
    yield case_of(ops)
    ops = window_ops([-1, 0], [(0, 0, 200, 200), (10, 10, 100, 100)], [0xF0000001, 1]) + queue_setup((2, 2, 4)) + [(4, (1, 0)), (4, (2, 1))]
    yield case_of(ops + [(35, (i - 3, i, -i, i)) for i in range(7)])


def wheel_random(r):
    ops, n = rnd_windows(r, r.randint(2, SLOTS))
    ops += queue_setup((r.choice([2, 4, 16, 32]), r.choice([2, 4, 16, 32]), 4), assign=r.random() < 0.9)
    ops.append((4, (1, r.randint(-1, n - 1))))
    ops.append((4, (4, r.choice([0, 1, 1, 1, 1, 2, -1]))))
    for _ in range(r.randint(4, 20)):
        k = r.random()
        if k < 0.6:
            delta = r.choice([r.randint(-0x80000000, 0x7FFFFFFF), r.randint(-500, 500), r.choice(WHEEL_DELTAS)])
            x, y = r.choice([r.randint(-32768, 32767), r.randint(-20, 220)]), r.choice([r.randint(-32768, 32767), r.randint(-20, 220)])
            ops.append((35, (delta, x, y, r.randint(-32768, 32767))))
        elif k < 0.75:
            ops.append((4, (2, r.randint(-1, n - 1))))
        elif k < 0.9:
            ops.append((4, (3, r.randint(-1, n - 1))))
        elif k < 0.95:
            ops.append((4, (4, r.choice([0, 1, 1, 1, 2, -1]))))
        else:
            ops.append((3, (r.randrange(n), rnd_attribute(r), r.choice([0, 0, 1, 0xFF]), r.choice([0, 0, 1, 2, 3]))))
    return case_of(ops, r)


TESTS.append(FnTest("UI_DeviceMouseWheel", ui_ppc, ui_host, edge=wheel_edges, rnd=wheel_random))


# ---- test 3c: the keys (UI_Keydown, UI_Keyup, UI_KeyChar; unit w50)

KEY_VALUES = [0, 1, -1, 0x41, 0x7FFF, -0x8000, 0x1234, 0xFF, 0x100, 0x8000, -0x1234]
KEY_FLAGS = [0, 0x1000000, 0xFF0000, -1, 0x7FFFFFFF, -0x80000000, 0x30000, 0x1030000, 0x10000, 0xFFFF, 1, -0x10000]
KEY_JUNK = [0, 0x5A5A0000, 0xFFFF0000, 0x80000000]


def key_edges():
    # every entry point with every key and flags word, the UI active and not
    for active in (1, 0, -1, 2, 0x10000):
        for op in (32, 33, 34):
            for key in KEY_VALUES:
                for flags in KEY_FLAGS:
                    ops = button_layout() + [(4, (4, active)), (op, (key, flags, 0))]
                    yield case_of(ops)
    # junk in the upper half of the key register is ignored
    for junk in KEY_JUNK:
        for op in (32, 33, 34):
            for key in (0x41, -1, 0x7FFF, -0x8000):
                yield case_of(button_layout() + [(op, (key, 0x30000, junk))])
    # the three in turn, a queue that fills up (the key messages are added without a test of the window)
    ops = window_ops([-1, 0], [(0, 0, 200, 200), (10, 10, 100, 100)], [0xF0000001, 1]) + queue_setup((3, 2, 4))
    for i in range(7):
        ops += [(32 + i % 3, (0x40 + i, i << 16, 0))]
    yield case_of(ops)
    ops = window_ops([-1, 0], [(0, 0, 200, 200), (10, 10, 100, 100)], [0xF0000001, 1]) + queue_setup((2, 2, 4))
    ops += [(4, (4, 0))] + [(32 + i % 3, (i, i, 0)) for i in range(4)] + [(4, (4, 1))] + [(32 + i % 3, (i, i, 0)) for i in range(4)]
    yield case_of(ops)
    # no queues at all while the UI is not active: nothing happens (and nothing is read)
    for op in (32, 33, 34):
        yield case_of(window_ops([-1], [(0, 0, 200, 200)], [0xF0000001]) + [(4, (4, 0)), (op, (0x41, 0x30000, 0))])
    # mixed with the mouse: the key messages and the button messages share the queue in order
    ops = button_layout() + [(30, (0, 5, 5)), (32, (0x41, 0x30000, 0)), (31, (0, 5, 5)), (33, (0x41, 0x30000, 0)), (34, (0x61, 0x1000000, 0))]
    yield case_of(ops)


def key_random(r):
    ops, n = rnd_windows(r, r.randint(1, SLOTS))
    ops += queue_setup((r.choice([2, 4, 16, 32]), r.choice([2, 4, 16, 32]), 4), assign=r.random() < 0.9)
    ops.append((4, (1, r.randint(-1, n - 1))))
    ops.append((4, (4, r.choice([0, 1, 1, 1, 1, 2, -1]))))
    for _ in range(r.randint(1, 20)):
        k = r.random()
        if k < 0.85:
            ops.append((r.choice([32, 33, 34]), (r.choice(KEY_VALUES + [r.randint(-32768, 32767)]),
                                                  r.choice(KEY_FLAGS + [r.getrandbits(32) - (1 << 31)]), r.choice(KEY_JUNK))))
        elif k < 0.9:
            ops.append((4, (4, r.choice([0, 1, 1, 1, 2, -1]))))
        elif k < 0.95:
            ops.append((30 + r.randrange(2), (r.randrange(3), r.randint(-20, 220), r.randint(-20, 220))))
        else:
            ops.append((4, (2, r.randint(-1, n - 1))))
    return case_of(ops, r)


TESTS.append(FnTest("UI_Keydown, UI_Keyup, UI_KeyChar", ui_ppc, ui_host, edge=key_edges, rnd=key_random))


def window_edges():
    """InterfaceWindow's constructor: attributes, parents, rectangles, depths, with the root and queues in place."""
    root = (1, (0, -1, 0xF0000001, -0x29A, 0, 0, 200, 200))
    setup = queue_setup((64, 64, 4))
    for attributes in ATTRIBUTES + [0x401, 0x8401]:
        for parent in (-1, 0):
            ops = [root, (3, (0, 1, 0, 0))] + setup + [(4, (1, 0)), (4, (5, 20)), (4, (6, 20)), (1, (1, parent, attributes, 77, 10, 10, 60, 60)),
                                                       (2, (1, parent, -1, -1, -1)), (11, (0, 20, 20)), (1, (2, 1, attributes, -1, -5, -6, 7, 8))]
            yield case_of(ops)
    # the pointer's window changes under the constructor's own UpdatePointerIn
    for inside in (-1, 0):
        ops = [root, (3, (0, 1, 0, 0))] + setup + [(4, (1, 0)), (4, (2, inside)), (4, (5, 20)), (4, (6, 20)),
                                                   (1, (1, 0, 1, 5, 10, 10, 60, 60)), (1, (2, 0, 0x401, 6, 30, 30, 70, 70)),
                                                   (4, (3, 0)), (1, (3, 0, 1, 7, 0, 0, 5, 5))]
        yield case_of(ops)
    # rectangles: reversed, empty, at the extremes; a parent's depth and pivot at the extremes
    rects = [(0, 0, 0, 0), (10, 10, 5, 5), (-32768, -32768, 32767, 32767), (32767, 32767, 32767, 32767), (-1, -1, 0, 0),
             (100, 200, 101, 203), (-7, 3, 4, -9), (1, 1, 2, 2)]
    for rect in rects:
        for attributes in (0, 0x400):
            parents = [-1, 0, 1]
            ops = [(1, (0, -1, 0, 1) + (3, 4, 20, 30)), (1, (1, 0, attributes, 2) + rect), (1, (2, 1, attributes, 3) + rect)]
            yield case_of(ops)
    for z in (0, 1, 0xFE, 0xFF, 0x100, 0x101, 0x7FFF, -1, -32768, 0x7FFE):
        for pivot in ((0, 0), (5, -5), (32767, 32767), (-32768, -32768)):
            ops = [(1, (0, -1, 0, 1, 0, 0, 10, 10)), (29, (0, z) + pivot), (1, (1, 0, 0, 2, 5, 6, 8, 9)), (1, (2, 1, 0, 3, 0, 0, 1, 1))]
            yield case_of(ops)
    # callbacks and the other fields survive a second construction over a used window
    ops = [(1, (0, -1, 0, 1, 0, 0, 10, 10)), (28, (0, 0, 2)), (28, (0, 1, 0)), (3, (0, 5, 0xFF, 2)), (1, (0, -1, 3, 4, 1, 2, 3, 4))]
    yield case_of(ops, seeds=(0, 0, 0))


def window_random(r):
    ops, n = rnd_windows(r, r.randint(1, SLOTS))
    if r.random() < 0.8:
        ops += queue_setup((r.choice([4, 16, 64]), r.choice([4, 16, 64]), 4))
    ops += [(4, (1, r.randint(-1, n - 1))), (4, (2, r.randint(-1, n - 1))), (4, (5, r.randint(-5, 200))), (4, (6, r.randint(-5, 200)))]
    for _ in range(r.randint(3, 14)):
        k = r.random()
        if k < 0.6:
            ops.append((1, (r.randrange(SLOTS), r.randint(-1, SLOTS - 1), rnd_attribute(r), r.choice(IDS)) + rnd_rect(r)))
        elif k < 0.7:
            ops.append((29, (r.randrange(n), r.choice([0, 1, 0xFE, 0x100, 0x7FFF, -1, r.randint(-32768, 32767)]),
                              r.randint(-32768, 32767), r.randint(-32768, 32767))))
        elif k < 0.8:
            ops.append((28, (r.randrange(n), r.randrange(2), r.randrange(3))))
        elif k < 0.9:
            ops.append((9, (r.randint(-10, 200), r.randint(-10, 200))))
        else:
            ops.append((4, (r.choice([1, 2, 3]), r.randint(-1, n - 1))))
    return case_of(ops, r)


TESTS.append(FnTest("InterfaceWindow::InterfaceWindow", ui_ppc, ui_host, edge=window_edges, rnd=window_random))


# ======================================================================= test 4: cursors, callbacks, help text

CURSOR_NUMBERS = [-1, 0, 1, 2, 3, 4, 5, 6, 7, 0x15, 0x16, -2, 0x7FFF, -0x8000, 0xFFFF]
STANDARDS = [0, 1, 3, 5, 6, 0x15, 0x8000, 0xFFFF]


def cursor_edges():
    help_on = (4, (11, 1))
    for tables in (((6, 0x3F), (4, 0b0101)), ((0, 0), (3, 0b010)), ((0x16, 0x3FFFFF), (1, 1)), ((5, 0b10110), (6, 0b100001))):
        for standard in STANDARDS:
            ops = [(18, (standard,)), (19, (1,)), (17, (0,)), (19, (1,)), (20, ()), (21, ())]
            for number in CURSOR_NUMBERS:
                ops += [(19, (number,)), (19, (number,)), (20, ()), (21, ())]
            ops += [(17, (1,)), (19, (2,)), (17, (1,)), (17, (-1,)), (17, (0,)), (19, (-1,)), (18, (standard ^ 1,)), (19, (-1,)),
                    (19, (3,)), (17, (1,)), (19, (4,))]
            yield case_of(ops, tables=tables)
    # the number in use and the cursor in use set by hand: a table of the other kinds
    for number in (-1, 0, 2, 5, 6, 0x15, 0x7FFF, -0x8000):
        for cursor in (-1, 0, 3):
            ops = [(4, (13, number)), (4, (10, cursor)), (17, (0,)), (4, (13, number)), (4, (10, cursor)), (17, (1,)), (17, (1,)),
                   (4, (15, 0)), (19, (number,)), (19, (4,)), (20, ()), (21, ())]
            yield case_of(ops)
    # no table: nothing happens; no cursor: hide and show do nothing
    yield case_of([(19, (1,)), (19, (-1,)), (20, ()), (21, ()), (17, (-1,)), (18, (3,)), (19, (3,))])
    # the peek function, the clock, the help text
    ops = []
    for k in (0, 1, 2, 3, 0, 3):
        ops.append((22, (k,)))
    for k in (0, 1, -1, 0, -1, 1):
        ops.append((23, (k,)))
    yield case_of(ops)
    yield case_of([help_on, (24, ()), (25, ()), (24, ()), (4, (11, 0)), (24, ()), (25, ()), (4, (11, 1)), (25, ())])


def cursor_random(r):
    ops = []
    for _ in range(r.randint(4, 30)):
        k = r.random()
        if k < 0.3:
            ops.append((19, (r.choice(CURSOR_NUMBERS + [r.randint(-100, 100)]),)))
        elif k < 0.4:
            ops.append((17, (r.randint(-1, TABLES - 1),)))
        elif k < 0.5:
            ops.append((18, (r.choice(STANDARDS + [r.getrandbits(16)]),)))
        elif k < 0.58:
            ops.append((20, ()))
        elif k < 0.66:
            ops.append((21, ()))
        elif k < 0.72:
            ops.append((22, (r.randrange(4),)))
        elif k < 0.78:
            ops.append((23, (r.randint(-1, 1),)))
        elif k < 0.84:
            which = r.choice([10, 13, 14, 15])
            value = {10: r.randint(-1, CURSORS - 1), 13: r.choice(CURSOR_NUMBERS), 14: r.choice(STANDARDS),
                     15: r.randint(-1, TABLES - 1)}[which]
            ops.append((4, (which, value)))
        elif k < 0.9:
            ops.append((4, (11, r.randrange(2))))
        else:
            ops.append((r.choice([24, 25]), ()))
    return case_of(ops, r)


TESTS.append(FnTest("UI_UseCursorTable, UI_SetStandardCursor, UI_SetCursor, UI_HideCursor, UI_ShowCursor, UI_SetPeekCallBack, "
                    "UI_SetClockTickFunction, UI_EnableHelpText, UI_DisableHelpText", ui_ppc, ui_host, edge=cursor_edges,
                    rnd=cursor_random))


# ======================================================================= test 5: the lobby's interface


def lobby_edges():
    for tooltips in (0, 1):
        for call in (26, 27):
            for queues in (False, True):
                ops = [(4, (11, 1)), (4, (16, tooltips))]
                if queues:
                    ops += queue_setup((8, 8, 4)) + [(1, (0, -1, 1, 5, 0, 0, 20, 20)), (7, (0, 1, 1, 1)), (7, (0, 4, 2, 2)),
                                                       (13, (2,)), (14, (2, 4))]
                ops += [(call, ()), (4, (16, 1 - tooltips)), (call, ()), (7, (0, 4, 3, 3)), (call, ())]
                yield case_of(ops)
    yield case_of([(26, ()), (27, ())])   # without a help text handler: skipped


def lobby_random(r):
    ops = [(4, (11, 1))]
    if r.random() < 0.7:
        ops += queue_setup((r.choice([2, 4, 8]), r.choice([2, 4, 8]), 4))
    ops.append((1, (0, -1, 1, 5, 0, 0, 20, 20)))
    for _ in range(r.randint(2, 10)):
        k = r.random()
        if k < 0.4:
            ops.append((r.choice([26, 27]), ()))
        elif k < 0.6:
            ops.append((4, (16, r.randrange(2))))
        elif k < 0.8:
            ops.append((7, (0, r.choice([1, 2, 4, 0x10001]), r.choice(VALUES), r.choice(VALUES))))
        else:
            ops.append((8, ()))
    return case_of(ops, r)


TESTS.append(FnTest("InitLobbyUI, UIInterface::InitLobby", ui_ppc, ui_host, edge=lobby_edges, rnd=lobby_random))


# ======================================================================= test 6: the cursor pictures


def real_picture(name):
    """The game's cursor file `name` (found without regard to case), as the loader fakes serve it."""
    for entry in CURSOR_DIR.iterdir():
        if entry.name.lower() == name.lower():
            data = entry.read_bytes()
            id_length, map_type = data[0], data[1]
            map_first, map_length, map_bits = struct.unpack("<HHB", data[3:8])
            width, height, depth = struct.unpack("<HHB", data[12:17])
            position = 18 + id_length
            palette, has_palette = bytes(1024), False
            if map_type:
                step = map_bits // 8
                raw = data[position:position + map_length * step]
                position += map_length * step
                rgba = bytearray(1024)
                for i in range(min(map_length, 256)):
                    b, g, r = raw[i * step], raw[i * step + 1], raw[i * step + 2]
                    rgba[4 * i:4 * i + 4] = bytes([r, g, b, 255])
                palette, has_palette = bytes(rgba), True
            return dict(name=name, path="Disk:Theme Park Data:data:ui:cursors:" + entry.name, depth=depth, width=width,
                        height=height, header_code=0, load_code=0, has_palette=has_palette, palette=palette, pixels=data[position:])
    raise FileNotFoundError(name)


def variant(base, **changes):
    picture = dict(base)
    picture.update(changes)
    return picture


def synthetic(name, depth, width, height, has_palette=False, header_code=0, load_code=0, seed=0):
    size = min(16000, width * ((depth + 7) >> 3) * height)
    pixels = bytes((i * 7 + seed * 31 + len(name)) & 0xFF for i in range(size))
    palette = bytes((i * 3 + seed) & 0xFF for i in range(1024))
    return dict(name=name, path="Disk:cursors:" + name, depth=depth, width=width, height=height, header_code=header_code,
                load_code=load_code, has_palette=has_palette, palette=palette, pixels=pixels)


def all_real():
    return [real_picture(name) for name in COLOUR_NAMES]


CODES = [0, -1, 1, 5, 0x7FFFFFFF, -0x80000000, -5]


def file_edges():
    real = all_real()
    table = (45, (-1, -1, 0, 0))
    # InitialiseCOLOURCursors: every picture, none, each one missing, twice, a short table
    yield case_of([(46, (0x16,)), (44, ()), (44, ())], pictures=real)
    yield case_of([(46, (0x16,)), (44, ())], pictures=[])
    for missing in range(len(COLOUR_NAMES)):
        yield case_of([(46, (0x16,)), (44, ())], pictures=real[:missing] + real[missing + 1:])
    for size in (0, 1, 5, 10, 0x15, 0x16, 0x20):
        yield case_of([(46, (size,)), (44, ())], pictures=real)
    # the constants the loader's answers are compared with, and the answers
    for read_failed in (-1, 0, 5):
        for load_failed in (-1, 0, 5):
            pictures = [variant(real[0], header_code=h, load_code=l) for h in CODES[:5] for l in CODES[:5]]
            ops = [(41, ()), (45, (read_failed, load_failed, 0, 0))] + [(42, (i,)) for i in range(len(pictures))]
            yield case_of(ops, pictures=pictures)
    # the pictures' shapes
    shapes = []
    for depth in (0, 1, 2, 4, 8, 9, 15, 16, 24, 25, 32, 33, 40, 64, 72):
        for width in (0, 1, 31, 32, 33, 64, 96, 128, 160, 1000):
            shapes.append((depth, width))
    for start in range(0, len(shapes), 10):
        pictures = [synthetic("shape%d.tga" % i, d, w, 32 if i % 3 else 1, has_palette=i % 2 == 0, seed=i)
                    for i, (d, w) in enumerate(shapes[start:start + 10])]
        yield case_of([(41, ())] + [(42, (i,)) for i in range(len(pictures))], pictures=pictures)
    # real pictures loaded one by one, and with the cursor made twice
    yield case_of([(41, ())] + [(42, (i,)) for i in range(len(real))] + [(41, ()), (42, (1,)), (42, (0,))], pictures=real)
    # LoadData without ever making the cursor is skipped; the cursor's constructor alone
    yield case_of([(42, (0,)), (41, ())], pictures=real[:2])
    # LoadBFCurs: found and not found, the folder's flag and pointer
    for flag in (0, 1):
        for folder in (0, 1):
            ops = [(45, (-1, -1, flag, folder)), (43, (), "CNor.tga"), (43, (), "cbus.TGA"), (43, (), "nothing.tga"), (43, (), ""),
                   (43, (), "CCro.tga")]
            yield case_of(ops, pictures=real)
    yield case_of([(43, (), name) for name in COLOUR_NAMES[:6]], pictures=real[:3])
    yield case_of([(45, (0, 0, 1, 1)), (43, (), "CNor.tga"), (45, (-1, -1, 0, 0)), (43, (), "CNor.tga")], pictures=real[:1])


def file_random(r):
    real = all_real()
    pictures = []
    for _ in range(r.randint(0, 12)):
        k = r.random()
        if k < 0.4:
            pictures.append(variant(r.choice(real), header_code=r.choice(CODES[:3] + [0, 0, 0]), load_code=r.choice(CODES[:3] + [0, 0, 0])))
        else:
            name = r.choice(COLOUR_NAMES + ["other.tga"])
            pictures.append(synthetic(name, r.choice([0, 1, 2, 3, 4, 5, 8, 16, 24, 32, 40]), r.choice([0, 16, 32, 64, 96, 128, 200, 256]),
                                      r.choice([0, 1, 32, 33]), r.random() < 0.4, r.choice(CODES[:2] + [0, 0, 0, 0]),
                                      r.choice(CODES[:2] + [0, 0, 0, 0]), r.randrange(100)))
    ops = []
    if r.random() < 0.5:
        ops.append((46, (r.choice([0x16, 0x16, 0x10, 0x20, 5]),)))
    for _ in range(r.randint(2, 12)):
        k = r.random()
        if k < 0.25:
            ops.append((41, ()))
        elif k < 0.5 and pictures:
            ops.append((42, (r.randrange(len(pictures)),)))
        elif k < 0.7:
            ops.append((43, (), r.choice(COLOUR_NAMES + ["none.tga", pictures[0]["name"] if pictures else "x"])))
        elif k < 0.8:
            ops.append((44, ()))
        else:
            ops.append((45, (r.choice([-1, 0, 5, -5]), r.choice([-1, 0, 5, -5]), r.randrange(2), r.randrange(2))))
    return case_of(ops, r, pictures=pictures)


TESTS.append(FnTest("InterfaceBFCursor ctor and LoadData, LoadBFCurs, InitialiseCOLOURCursors", cursor_ppc, cursor_host, edge=file_edges,
                    rnd=file_random))
