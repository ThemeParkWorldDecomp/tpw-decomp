"""Differential tests for the interface's message pump, against libtpw_unit_UIPump.dylib (tests/unit_UIPump.cpp):

  src/game/ui/InterfaceMessageQueue.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/InterfaceWindow.cpp, src/game/ui/UIInterface.cpp, src/game/ui/UI_Base.cpp, src/game/ui/ui_ErrorDisplay.cpp, src/game/ui/ui_Postcard.cpp, src/game/ui/ui_main.cpp   UIInterface::Process, ProcessUI, UI_DispatchMessages, UI_DispatchDrawMessages,
                    UI_DispatchDeferredMessages, UI_SendOnFrameMessages, onframe, UI_SendMessage, UI_ServiceAllTimers, UI_AddTimer,
                    IveGotTicks, InterfaceMessageQueue::Post, Peek, GetNumQd, InterfaceTimedMessages::Process,
                    InterfaceWindow::CallWindowHandler, AmIGettingDestroyed, GetChild, GetNext, GetAttr, UI_GetHighestZ,
                    UI_PostcardPending, TickUIErrors

Real on both sides: those, and under them the window constructor (src/game/ui/InterfaceWindow.cpp), the queues' Add, Flush and
Initialise, UI_PostMessage and UI_FlushAllMessages (src/game/ui/InterfaceMessageQueue.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/UI_Base.cpp), the rectangles (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp), the UI globals
(src/game/ui/UI_Base.cpp). Fakes that record the same event on both sides: the four window handlers and the default one, the two peek
callbacks, the clock, highestZ, UI_DialogBox with the strings it is given, the mutex, UI_ActuallyDoPostcard, the assertion.

The world: twelve windows in a forest (made with the original constructor, then linked, given attributes, handlers and
destroyed flags), a general queue, a draw queue and a timed queue, a table of six timers, one clock, two peek callbacks,
three key lists, three string tables with their texts, and the error queue. A script of operations runs over it; after each
operation the answer, the 35 globals, every window (63 values), both queues, the timed queue, the timer table, the error
queue, the heap traffic and the length of the call log are compared (the whole log at the end).

Scripted behaviour (the case's return queues, the same on both sides): the handlers answer H1 to H4 and "Hdef" for the default
one, and each call also takes an action from "act" (1 mark the window destroyed, 2 unlink its next sibling, 3 post a message
(from "actarg": window, message, a, b), 4 flush all messages, 5 clear the peek callback, 6 detach its children, 7 clear the key
capture's list, 8 clear the keyboard and focus windows, 9 cut its next link, 10 clear its handler); the peek callbacks take
"pact" (1 clear the callback, 2 swap them); the clock answers "tick".

On the PowerPC side the std::deque of errors is the CodeWarrior one in emulated memory (blocks of four records), whose pop_front
is a Python stand-in; the host's is std::deque. An operation the state does not allow (a loop of links, a timer whose slice
would never end, a queue the original would read through NULL) is decided on the PowerPC side and skipped on both.
"""
import ctypes as C
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest
from unicorn import UC_HOOK_CODE
from unicorn import ppc_const as UP

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIPump"

MINUS = 0xFFFFFFFF

# ---- the unit's functions
FN = dict(Process=0x101EF5A8, ProcessUI=0x101866C4, DispatchMessages=0x101C96B4, DispatchDraw=0x101C97D8,
          DispatchDeferred=0x101C987C, SendOnFrame=0x101C9C2C, onframe=0x101C98F0, SendMessage=0x101C9518,
          ServiceTimers=0x101D88E4, AddTimer=0x101D878C, IveGotTicks=0x101CAC3C, Post=0x101C7D7C, Peek=0x101C7FA8,
          GetNumQd=0x101C8190, TimedProcess=0x101C83D4, CallHandler=0x101DE384, Destroyed=0x101C9BE8, GetChild=0x101DBAC4,
          GetNext=0x101DBB38, GetAttr=0x101DBB70, HighestZ=0x101CA880, PostcardPending=0x101B22BC, TickErrors=0x101F1E90)
# what they call that is real too: the window constructor and the rectangles under it, the queues' set-up and Add, posting
REAL = dict(WindowCtor=0x101D98D8, QueueCtor=0x101C7AD8, QueueInit=0x101C7BC4, QueueAdd=0x101C7C98, TimedCtor=0x101C81D0,
            TimedInit=0x101C85F0, PostMessage=0x101C9400, FlushAll=0x101C9E48, DequeIndex=0x101F2004)
REAL_HELPERS = [0x101C7448, 0x101C7494, 0x101C74E0, 0x101C756C, 0x101C75A8, 0x101C7618, 0x101C7678, 0x101C7710, 0x101C7788,
                0x101C7A1C, 0x101C7A78, 0x101D5A3C, 0x101D5A70, 0x101D5AA0, 0x101D5AD0, 0x101D5B00, 0x101D5B74, 0x101DAB88]
# what they call that is a fake
CALC_CLIENT_AREA, DEFAULT_CALLBACK, HIGHEST_Z, CONVERT_MULTI, DIALOG_BOX = 0x101D987C, 0x101DE3F8, 0x101CA6DC, 0x101803CC, 0x101F1800
SET_CALLBACK, POSTCARD, MUTEX_LOCK, MUTEX_UNLOCK, POP_FRONT = 0x101DCBA8, 0x101B20B4, 0x10200D8C, 0x10200DE0, 0x101F2074
DEFAULT_CALLBACK_TV = 0x10268480
ERROR_BUTTON_TV, ERROR_HANDLER_TV = 0x102685F8, 0x102685F0   # the TVectors TickUIErrors hands out

# ---- the original's data
G = dict(root=0x10395C24, pointer_in=0x10395C1C, capture=0x10395C14, key_capture=0x10395C08, draw_queue=0x10395C00,
         msg_queue=0x10395C04, timed=0x10395BFC, peek=0x10395BF8, started=0x10395BF4, hebrew=0x10395B88, pointer_x=0x10395BC2,
         pointer_y=0x10395BC0, cursor=0x10395BB0, table=0x10395BB4, standard=0x10395BB8, number=0x102AF920, help=0x102AF928,
         clock=0x102AF92C, timers=0x10395C54, timer_count=0x10395C50, timer_list=0x10395C58, paused=0x10395C4C,
         last_window=0x10395C18, buttons=0x10395BBE, active=0x102AF924)
PG = dict(keyboard=0x10395C10, focus=0x10395C20, onframe=0x10395B84, general_q=0x1038CC4C, draw_q=0x1038CC48,
          pc_delay=0x10394B04, pc_wanted=0x103956F0, dialog=0x1039647C, mutex=0x10396444, deque=0x10396454)
SIZE_WINDOW = 0x138
SAVE = ([(a, 4) for a in G.values()] + [(G["key_capture"] + 4, 4), (G["hebrew"], 1), (G["pointer_x"], 4), (G["pointer_y"], 4),
                                         (G["standard"], 4), (G["number"], 4), (G["buttons"], 4)] +
        [(a, 4) for a in PG.values()] + [(0x10396440, 0x40)])

# ---- labels (the host's tpw_pump_labels)
SLOTS, TIMERS, TABLES, TEXTS, RECTS = 12, 6, 3, 3, 2
LAB_WINDOW, LAB_QUEUE, LAB_TIMED, LAB_CLOCK, LAB_TIMER, LAB_TABLE = 10, 30, 32, 41, 50, 60
LAB_TEXT, LAB_WIDE, LAB_RECT, LAB_MUTEX, LAB_FUNCTION, LAB_KEYS = 70, 75, 80, 85, 90, 100
WINDOW_FIELDS = 63
GLOBAL_FIELDS = 35
BIG_QUEUE = 16   # a queue of more slots than this is dumped as a checksum
DEQUE_CHUNK, DEQUE_MAP = 4, 5   # records to a block, slots of the block table

# ---- the key lists (the host's gKeys1..3)
KEY_LISTS = [None,
             [(0x41, 0), (0x42, 0x10), (0x41, 0x20), (0, 0), (0x1234, 0xFF), (0, -1)],
             [(0, -1)],
             [(-1, 0xFF), (0x7FFF, 0x80), (0x41, 0x100), (0x43, -2), (0x44, 0x7F), (0, -1)]]


def s16(v):
    return T.s16(v & 0xFFFF)


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MINUS))


def link_table(parents):
    """(parent, previous, next, child) slots of each window of a forest given by its parents."""
    out = []
    for slot in range(len(parents)):
        siblings = [t for t in range(len(parents)) if parents[t] == parents[slot]]
        at = siblings.index(slot)
        children = [t for t in range(len(parents)) if parents[t] == slot]
        out.append((parents[slot], siblings[at - 1] if at > 0 else -1, siblings[at + 1] if at + 1 < len(siblings) else -1,
                    children[0] if children else -1))
    return out


# ======================================================================= the PowerPC side


class Ppc:
    """One case on the PowerPC side: the fixtures, the operations and the dumps."""

    def __init__(self, e, case):
        self.e = e
        e.pump = self
        self.case = case
        self.heap = [0, 0, 0, 0]
        self.saved = {(a, n): bytes(e.read(a, n)) for a, n in SAVE}
        self.windows = [e.alloc(SIZE_WINDOW, b"\xa5" * SIZE_WINDOW) for _ in range(SLOTS)]
        self.made = [False] * SLOTS
        self.queues = [e.alloc(0x14, b"\xa5" * 0x14) for _ in range(2)]
        self.timed_block = e.alloc(8, b"\xa5" * 8)
        self.timers = e.alloc(TIMERS * 0x20, bytes(TIMERS * 0x20))
        for i in range(TIMERS):
            w32(e, self.timers + 0x20 * i + 0x14, 1)
        self.tables = [e.alloc(32, bytes(32)) for _ in range(TABLES)]
        self.texts = [e.alloc(8, bytes([97 + k]) * 8) for k in range(TEXTS)]
        self.wides = [e.alloc(16, b"\0" + bytes([65 + k]) + bytes(14)) for k in range(TEXTS)]
        self.rects = [e.alloc(16, struct.pack(">I4h", 0x102AF8FC, 1, 2, 3, 4)) for _ in range(RECTS)]
        self.clock = e.alloc(8, struct.pack(">II", e.pump_clock_vtable, 0))
        self.key_lists = [0]
        for entries in KEY_LISTS[1:]:
            self.key_lists.append(e.alloc(4 * len(entries), b"".join(struct.pack(">hh", k, m) for k, m in entries)))
        fixed = {}
        fixed.update({w: LAB_WINDOW + i for i, w in enumerate(self.windows)})
        fixed.update({q: LAB_QUEUE + i for i, q in enumerate(self.queues)})
        fixed[self.timed_block] = LAB_TIMED
        fixed[self.clock] = LAB_CLOCK
        fixed.update({self.timers + 0x20 * i: LAB_TIMER + i for i in range(TIMERS)})
        fixed.update({t: LAB_TABLE + i for i, t in enumerate(self.tables)})
        fixed.update({t: LAB_TEXT + i for i, t in enumerate(self.texts)})
        fixed.update({t: LAB_WIDE + i for i, t in enumerate(self.wides)})
        fixed.update({r: LAB_RECT + i for i, r in enumerate(self.rects)})
        fixed[PG["mutex"]] = LAB_MUTEX
        fixed.update({a: LAB_KEYS + i for i, a in enumerate(self.key_lists) if a})
        self.func_labels = {ERROR_BUTTON_TV: LAB_FUNCTION + 6, ERROR_HANDLER_TV: LAB_FUNCTION + 7}
        self.func_labels.update({tv: LAB_FUNCTION + i for i, tv in enumerate(e.pump_handlers) if tv})
        self.L = e.cl = CL.PpcLog(fixed, case.get("q"))
        # the UI's globals as the start-up leaves them
        for name in G:
            w32(e, G[name], 0)
        e.write(G["hebrew"], b"\0")
        e.write(G["pointer_x"], struct.pack(">h", 0))
        e.write(G["pointer_y"], struct.pack(">h", 0))
        e.write(G["standard"], struct.pack(">H", 0))
        e.write(G["number"], struct.pack(">h", -1))
        e.write(G["buttons"], struct.pack(">h", 0))
        w32(e, G["active"], 1)
        w32(e, G["clock"], self.clock)
        for name in ("keyboard", "focus", "onframe", "general_q", "draw_q", "pc_delay", "pc_wanted", "dialog"):
            w32(e, PG[name], 0)
        self.setup_deque(case.get("deque_start", 0))
        # the windows, then their links, attributes, handlers and flags
        specs = case.get("windows", [])
        for slot, (parent, attributes, handler, destroyed) in enumerate(specs):
            self.call(REAL["WindowCtor"], self.windows[slot], self.windows[parent] if parent >= 0 else 0, attributes, slot * 7 - 3,
                      s16(slot), s16(slot), s16(slot + 50), s16(slot + 50))
            self.made[slot] = True
        for slot, (parent, previous, following, child) in enumerate(link_table([s[0] for s in specs])):
            self.run(2, (slot, parent, previous, following, child))
        for slot, (parent, attributes, handler, destroyed) in enumerate(specs):
            self.run(1, (slot, attributes, destroyed, handler))
        # the queues
        general, draw, timed = case.get("queues", (8, 8, 4))
        for k, count in enumerate((general, draw)):
            self.call(REAL["QueueCtor"], self.queues[k])
            if count > 0:
                self.call(REAL["QueueInit"], self.queues[k], count)
        self.call(REAL["TimedCtor"], self.timed_block)
        if timed >= 0:
            self.call(REAL["TimedInit"], self.timed_block, timed)
            entries, count = self.e.u32(self.timed_block + 4), T.s32(self.e.u32(self.timed_block))
            if entries and count > 0:
                e.write(entries, bytes(0x14 * count))
        self.timed_count = timed
        # the log starts here
        self.L.log.clear()
        self.L.next = CL.FIRST_NEW
        self.heap = [0, 0, 0, 0]

    def restore(self):
        for (a, n), data in self.saved.items():
            self.e.write(a, data)

    # ---- fixtures
    def setup_deque(self, start_offset):
        """The error queue as an empty CodeWarrior std::deque of four-record blocks: the block size, the table of blocks
        (its size, count, first slot and array), the offset of the first record and the number of records."""
        e = self.e
        base = PG["deque"]
        data = e.alloc(4 * DEQUE_MAP, bytes(4 * DEQUE_MAP))
        for slot in range(DEQUE_MAP):
            w32(e, data + 4 * slot, e.alloc(8 * DEQUE_CHUNK, bytes(8 * DEQUE_CHUNK)))
        self.deque_map_start = start_offset % DEQUE_MAP
        for offset, value in ((0, DEQUE_CHUNK), (4, DEQUE_MAP), (8, 1), (0xC, self.deque_map_start), (0x10, data),
                              (0x14, DEQUE_CHUNK >> 1), (0x18, 0)):
            w32(e, base + offset, value)

    def deque_record(self, index):
        """The address of the record `index` places after the oldest, or None when its block is not in the table."""
        e = self.e
        base = PG["deque"]
        position = e.u32(base + 0x14) + index
        block, offset = position // DEQUE_CHUNK, position % DEQUE_CHUNK
        if block >= DEQUE_MAP:
            return None
        slot = block + self.deque_map_start
        if slot >= DEQUE_MAP:
            slot -= DEQUE_MAP
        return e.u32(e.u32(base + 0x10) + 4 * slot) + 8 * offset

    def deque_records(self):
        e = self.e
        out = []
        for i in range(e.u32(PG["deque"] + 0x18)):
            record = self.deque_record(i)
            out.append((self.L.lab(e.u32(record)), T.s32(e.u32(record + 4))))
        return out

    # ---- helpers
    def window(self, slot):
        return self.windows[slot] if 0 <= slot < SLOTS and self.made[slot] else 0

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MINUS for a in args])[0]

    def window_at(self, address):
        """The window a global holds (NULL when it holds none)."""
        return self.e.u32(address)

    def handler_tv(self, handler):
        e = self.e
        return 0 if handler == 0 else e.pump_handlers[handler] if handler <= 4 else DEFAULT_CALLBACK_TV if handler == 5 else e.other_callback

    def func_label(self, address):
        return 0 if address == 0 else self.func_labels.get(address, -1)

    def queue_count(self, k):
        return T.s32(self.e.u32(self.queues[k] + 4))

    def queue_ready(self, k):
        """True when the queue has slots (its head and tail are then inside)."""
        return self.e.u32(self.queues[k]) != 0 and self.queue_count(k) > 0

    def acyclic(self):
        """False when the windows' links make a loop (the original would never come back): a parent chain, or the child and
        next links, that comes round to where it began."""
        e = self.e
        known = {w for s, w in enumerate(self.windows) if self.made[s]}
        for w in known:
            seen, at = set(), w
            while at:
                if at in seen:
                    return False
                seen.add(at)
                at = e.u32(at + 0x5C)
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

        if not all(visit(w) for w in known):
            return False
        # a window linked from several places is walked once from each: not so many that the log runs out
        visits = {}

        def count(w):
            if w not in visits:
                total, child = 1, e.u32(w + 0x58)
                while child:
                    total += count(child)
                    child = e.u32(child + 0x50)
                visits[w] = total
            return visits[w]

        return all(count(w) <= 300 for w in known)

    def post_safe(self, window, message):
        """False when posting the message would write through a queue that does not exist or has no slots (the original would
        crash): the draw queue takes messages 1 to 3, the general one the rest; 0x1000a to 0x1000c go to it whatever the window."""
        e = self.e
        m = message & MINUS
        target = e.u32(G["draw_queue"]) if 1 <= m <= 3 else e.u32(G["msg_queue"])
        adds = 0x1000A <= m <= 0x1000C or (target != 0 and window != 0 and e.read(window, 1)[0] == 0)
        return not adds or (target != 0 and e.u32(target) != 0 and T.s32(e.u32(target + 4)) > 0)

    def queue_global_ok(self, name):
        """True when the queue the global names does not exist or has slots."""
        q = self.e.u32(G[name])
        return q == 0 or (self.e.u32(q) != 0 and T.s32(self.e.u32(q + 4)) > 0)

    def tick_candidates(self):
        q = self.L.queues.get("tick")
        if not q:
            return [0]
        pos = self.L.pos.get("tick", 0)
        return list(q[pos:]) + [q[-1]]

    def timers_ok(self):
        """False when servicing the timers would not end soon: a timer in the list with a slice below 1, or a list that
        loops, or more than a few thousand slices in the time a coming tick implies."""
        e = self.e
        if T.s32(e.u32(G["paused"])) != 0:
            return True
        seen, t = set(), e.u32(G["timer_list"])
        while t:
            if t in seen or len(seen) > 8:
                return False
            seen.add(t)
            start, slice_ = e.u32(t + 4), T.s32(e.u32(t + 8))
            if slice_ < 1:
                return False
            for tick in self.tick_candidates():
                elapsed = T.s32((tick - start) & MINUS)
                if elapsed >= slice_ and elapsed // slice_ > 3000:
                    return False
            t = e.u32(t + 0x18)
        return True

    def list_ok(self):
        """False when the list of timers in use loops."""
        e = self.e
        seen, t = set(), e.u32(G["timer_list"])
        while t:
            if t in seen or len(seen) > 8:
                return False
            seen.add(t)
            t = e.u32(t + 0x18)
        return True

    # ---- the dumps (the orders are tpw_pump_dump_* in the library)
    def keys_code(self, value):
        return 0 if value == 0 else self.key_lists.index(value) if value in self.key_lists else 9

    def peek_code(self, value):
        return 0 if value == 0 else {self.e.pump_peeks[1]: 1, self.e.pump_peeks[2]: 2}.get(value, 3)

    def dump_globals(self):
        e, L = self.e, self.L
        u = e.u32
        sh = lambda a: struct.unpack(">h", e.read(a, 2))[0]   # noqa: E731
        out = [L.lab(u(G["root"])), L.lab(u(G["pointer_in"])), L.lab(u(G["capture"])), self.keys_code(u(G["key_capture"])),
               L.lab(u(G["key_capture"] + 4)), L.lab(u(G["draw_queue"])), L.lab(u(G["msg_queue"])), L.lab(u(G["timed"])),
               self.peek_code(u(G["peek"])), T.s32(u(G["started"])), e.read(G["hebrew"], 1)[0], sh(G["pointer_x"]),
               sh(G["pointer_y"]), L.lab(u(G["cursor"])), L.lab(u(G["table"])), struct.unpack(">H", e.read(G["standard"], 2))[0],
               sh(G["number"]), L.lab(u(G["help"])), L.lab(u(G["clock"])), int(u(G["timers"]) != 0), T.s32(u(G["timer_count"])),
               L.lab(u(G["timer_list"])), T.s32(u(G["paused"])), L.lab(u(G["last_window"])), sh(G["buttons"]), T.s32(u(G["active"])),
               L.lab(u(PG["keyboard"])), L.lab(u(PG["focus"])), T.s32(u(PG["onframe"])), T.s32(u(PG["general_q"])),
               T.s32(u(PG["draw_q"])), T.s32(u(PG["pc_delay"])), T.s32(u(PG["pc_wanted"])), L.lab(u(PG["dialog"])),
               u(PG["deque"] + 0x18)]
        assert len(out) == GLOBAL_FIELDS
        return out

    def callback_code(self, value):
        e = self.e
        return 0 if value == 0 else {e.pump_handlers[i]: i for i in range(1, 5)}.get(value, 5 if value == DEFAULT_CALLBACK_TV else 6)

    def dump_window(self, address):
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

    def dump_queue(self, k):
        e, L = self.e, self.L
        address = self.queues[k]
        entries, count, head, tail, queued = struct.unpack(">IiiiI", e.read(address, 0x14))
        out = [L.lab(address), int(entries != 0), count, head, tail, T.s32(queued)]
        if entries == 0 or count <= 0:
            return out
        words = struct.unpack(f">{4 * count}I", e.read(entries, 16 * count))
        if count > BIG_QUEUE:   # a long ring: a checksum of its entries stands for them
            total = 0x811C9DC5
            for i in range(count):
                for word in (L.lab(words[4 * i]) & MINUS, words[4 * i + 1], words[4 * i + 2], words[4 * i + 3]):
                    total = ((total ^ word) * 0x01000193) & MINUS
            return out + [total]
        for i in range(count):
            out += [L.lab(words[4 * i]), T.s32(words[4 * i + 1]), T.s32(words[4 * i + 2]), T.s32(words[4 * i + 3])]
        return out

    def dump_timed(self):
        e, L = self.e, self.L
        count, entries = struct.unpack(">iI", e.read(self.timed_block, 8))
        out = [L.lab(self.timed_block), int(entries != 0), count]
        if entries == 0:
            return out
        for i in range(max(0, count)):
            w, m, a, b, t = struct.unpack(">IiiiI", e.read(entries + 0x14 * i, 0x14))
            out += [L.lab(w), m, a, b, T.s32(t)]
        return out

    def dump_timers(self):
        e, L = self.e, self.L
        out = []
        for i in range(TIMERS):
            window, start, slice_, ident, running, free, nxt, prev = struct.unpack(">IIiiiiII", e.read(self.timers + 0x20 * i, 0x20))
            out += [L.lab(window), T.s32(start), slice_, ident, running, free, L.lab(nxt), L.lab(prev)]
        return out

    def dump_state(self):
        return dict(globals=self.dump_globals(),
                    windows=[self.dump_window(self.windows[s]) if self.made[s] else [0] for s in range(SLOTS)],
                    queues=[self.dump_queue(0), self.dump_queue(1)], timed=self.dump_timed(), timers=self.dump_timers(),
                    errors=[x for pair in self.deque_records() for x in pair], heap=list(self.heap), log=len(self.L.log))

    # ---- the operations
    def op(self, code, a):
        """Runs one operation; returns its answer, or None when the state does not allow it."""
        return self.run(code, a)

    def run(self, code, a):
        e, L = self.e, self.L
        u = e.u32
        if code in (1, 2, 26, 35, 36) and not self.window(a[0]):
            return None
        if code == 1:   # a window's fields: slot, attributes, destroying byte, handler
            w = self.windows[a[0]]
            w32(e, w + 0x44, a[1])
            e.write(w, bytes([a[2] & 0xFF]))
            w32(e, w + 0x110, self.handler_tv(a[3]))
            return 0
        if code == 2:   # links: slot, parent, previous, next, child
            w = self.windows[a[0]]
            for offset, slot in ((0x5C, a[1]), (0x54, a[2]), (0x50, a[3]), (0x58, a[4])):
                w32(e, w + offset, self.window(slot))
            return 0
        if code == 3:
            return self.set_global(a[0], a[1])
        if code == 4:   # a queue Add: queue, window, message, a, b
            if not self.queue_ready(a[0]):
                return None
            return T.s16(self.call(REAL["QueueAdd"], self.queues[a[0]], self.window(a[1]), a[2], a[3], a[4]))
        if code == 5:   # a queue's entry: queue, index, field, value
            if not self.queue_ready(a[0]) or not 0 <= a[1] < self.queue_count(a[0]):
                return None
            address = u(self.queues[a[0]]) + 16 * a[1] + 4 * a[2]
            w32(e, address, self.window(a[3]) if a[2] == 0 else a[3])
            return 0
        if code == 6:   # a queue's words: queue, head, tail, queued
            if not self.queue_ready(a[0]) or not (0 <= a[1] < self.queue_count(a[0]) and 0 <= a[2] < self.queue_count(a[0])):
                return None
            for offset, value in ((8, a[1]), (0xC, a[2]), (0x10, a[3])):
                w32(e, self.queues[a[0]] + offset, value)
            return 0
        if code == 7:   # a timed message: index, window, message, a, b, time
            entries = u(self.timed_block + 4)
            if entries == 0 or not 0 <= a[0] < T.s32(u(self.timed_block)) or (a[5] != 0 and not self.window(a[1])):
                return None
            w32(e, entries + 0x14 * a[0], self.window(a[1]))
            for k in range(4):
                w32(e, entries + 0x14 * a[0] + 4 + 4 * k, a[2 + k])
            return 0
        if code == 8:   # a timer: index, window, start, slice, id, running, free, next, previous
            t = self.timers + 0x20 * a[0]
            for offset, value in ((0, self.window(a[1])), (4, a[2]), (8, a[3]), (0xC, a[4]), (0x10, a[5]), (0x14, a[6]),
                                  (0x18, self.timers + 0x20 * a[7] if 0 <= a[7] < TIMERS else 0),
                                  (0x1C, self.timers + 0x20 * a[8] if 0 <= a[8] < TIMERS else 0)):
                w32(e, t + offset, value)
            return 0
        if code == 9:   # an error: table, id
            index = u(PG["deque"] + 0x14) + u(PG["deque"] + 0x18)
            if index // DEQUE_CHUNK >= DEQUE_MAP:
                return None
            w32(e, PG["deque"] + 0x18, u(PG["deque"] + 0x18) + 1)
            record = self.deque_record(u(PG["deque"] + 0x18) - 1)
            w32(e, record, self.tables[a[0]])
            w32(e, record + 4, a[1])
            return 0
        if code == 11:   # many messages added to a queue: queue, count, window, message, first argument (the second counts up)
            if not self.queue_ready(a[0]):
                return None
            added = 0
            for i in range(a[1]):
                added += T.s16(self.call(REAL["QueueAdd"], self.queues[a[0]], self.window(a[2]), a[3], a[4] + i, i))
            return added
        if code == 10:
            w32(e, PG["deque"] + 0x14, DEQUE_CHUNK >> 1)
            w32(e, PG["deque"] + 0x18, 0)
            return 0
        # ---- the functions under test
        if code in (20, 21):
            if u(G["msg_queue"]) == 0 or u(G["draw_queue"]) == 0 or not self.acyclic() or not self.timers_ok() or not self.list_ok():
                return None
            if not (self.queue_global_ok("msg_queue") and self.post_safe(self.window_at(G["root"]), 2)):
                return None
            self.call(FN["Process"] if code == 20 else FN["ProcessUI"], 0x10396440)
            return 0
        if code == 22:
            return T.s32(self.call(FN["DispatchMessages"], a[0]))
        if code == 23:
            return T.s32(self.call(FN["DispatchDraw"], a[0]))
        if code == 24:
            return T.s32(self.call(FN["DispatchDeferred"]))
        if code in (25, 26):
            if not self.acyclic():
                return None
            if code == 25:
                return T.s32(self.call(FN["SendOnFrame"]))
            self.call(FN["onframe"], self.windows[a[0]])
            return 0
        if code == 27:
            return T.s32(self.call(FN["SendMessage"], self.window(a[0]), a[1], a[2], a[3]))
        if code == 28:
            if not self.timers_ok() or not self.list_ok() or not self.queue_global_ok("msg_queue"):
                return None
            self.call(FN["ServiceTimers"])
            return 0
        if code == 29:
            if not self.list_ok():
                return None
            return L.lab(self.call(FN["AddTimer"], self.window(a[0]), a[1], a[2]))
        if code == 30:
            return self.call(FN["IveGotTicks"]) & MINUS
        if code in (31, 32, 33):
            if code != 33 and u(self.queues[a[0]]) != 0 and not self.queue_ready(a[0]):
                return None
            if code == 31:
                return T.s16(self.call(FN["Post"], self.queues[a[0]]))
            if code == 32:
                found = self.call(FN["Peek"], self.queues[a[0]])
                return (found - u(self.queues[a[0]])) // 16 if found else -1
            return T.s32(self.call(FN["GetNumQd"], self.queues[a[0]]))
        if code == 34:
            return T.s16(self.call(FN["TimedProcess"], self.timed_block))
        if code == 35:
            return T.s32(self.call(FN["CallHandler"], self.windows[a[0]], a[1], a[2], a[3]))
        if code == 36:   # an accessor: slot, which, the argument GetChild ignores
            w = self.windows[a[0]]
            if a[1] == 0:
                return T.s8(self.call(FN["Destroyed"], w) & 0xFF)
            if a[1] == 1:
                return L.lab(self.call(FN["GetChild"], w, a[2]))
            if a[1] == 2:
                return L.lab(self.call(FN["GetNext"], w))
            return self.call(FN["GetAttr"], w) & MINUS
        if code == 37:   # UI_GetHighestZ: window (-1 for NULL), rect (0 none, 1 and 2 the world's)
            return T.s32(self.call(FN["HighestZ"], self.window(a[0]), self.rects[a[1] - 1] if a[1] >= 1 else 0))
        if code == 38:
            return self.call(FN["PostcardPending"]) & 0xFF
        if code == 39:
            self.call(FN["TickErrors"])
            return 0
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
            w32(e, PG["keyboard"], self.window(v))
        elif which == 5:
            w32(e, PG["focus"], self.window(v))
        elif which == 6:
            w32(e, G["key_capture"] + 4, self.window(v))
        elif which == 7:
            w32(e, G["key_capture"], self.key_lists[v])
        elif which == 8:
            w32(e, G["active"], v)
        elif which == 9:
            w32(e, G["paused"], v)
        elif which == 10:
            w32(e, G["peek"], 0 if v == 0 else e.pump_peeks[1] if v == 1 else e.pump_peeks[2])
        elif which in (11, 12):
            w32(e, G["msg_queue"] if which == 11 else G["draw_queue"], self.queues[v - 1] if v in (1, 2) else 0)
        elif which == 13:
            w32(e, G["timed"], self.timed_block if v else 0)
        elif which == 15:
            w32(e, G["timers"], self.timers if v else 0)
            w32(e, G["timer_count"], v)
        elif which == 16:
            w32(e, G["timer_list"], self.timers + 0x20 * v if 0 <= v < TIMERS else 0)
        elif which == 17:
            w32(e, PG["pc_delay"], v)
        elif which == 18:
            w32(e, PG["pc_wanted"], v)
        elif which == 19:
            w32(e, PG["dialog"], self.window(v))
        elif which == 20:
            w32(e, PG["onframe"], v)
        return 0


# ======================================================================= the fakes (PowerPC side)


def fake_cb_function(e, fn):
    """Like tpwtest.fake_function, but fn(emu, call) may make nested PowerPC calls (the way stub_cb does): returns the TVector
    of a function whose code leaves the emulation so that tpwtest runs fn outside the hook."""
    code = T.persist_alloc(e, 8, b"\x4e\x80\x00\x20" * 2, alignment=8)
    tvec = T.persist_alloc(e, 8, struct.pack(">II", code, e.toc), alignment=8)

    def hook(uc, address, size, user):
        e.pending_cb = fn
        uc.reg_write(UP.UC_PPC_REG_PC, ppcemu.RETURN_ADDR)

    e.uc.hook_add(UC_HOOK_CODE, hook, begin=code, end=code)
    return tvec


def make_fakes(e):
    """The window handlers, peek callbacks, clock and other callbacks, in persistent memory."""
    if hasattr(e, "pump_handlers"):
        return

    def handler(hid):
        def fn(emu, call):
            world, L = emu.pump, emu.cl
            window, message, a, b = emu.r(3), T.s32(emu.r(4)), T.s32(emu.r(5)), T.s32(emu.r(6))
            L.rec("Handler", (hid, L.lab(window), message, a, b))
            action = L.ret("act", 0)
            if action == 1:
                emu.write(window, b"\x01")
            elif action == 2:
                following = emu.u32(window + 0x50)
                if following:
                    w32(emu, window + 0x50, emu.u32(following + 0x50))
            elif action == 3:
                target = world.window(L.ret("actarg", -1))
                m, x, y = L.ret("actarg", 0), L.ret("actarg", 0), L.ret("actarg", 0)
                if world.post_safe(target, m):
                    call(("code", REAL["PostMessage"]), target, m, x, y)
            elif action == 4:
                call(("code", REAL["FlushAll"]))
            elif action == 5:
                w32(emu, G["peek"], 0)
            elif action == 6:
                w32(emu, window + 0x58, 0)
            elif action == 7:
                w32(emu, G["key_capture"], 0)
            elif action == 8:
                w32(emu, PG["keyboard"], 0)
                w32(emu, PG["focus"], 0)
            elif action == 9:
                w32(emu, window + 0x50, 0)
            elif action == 10:
                w32(emu, window + 0x110, 0)
            return T.u32(L.ret(f"H{hid}", 0))
        return fn

    e.pump_handlers = [0] + [fake_cb_function(e, handler(i)) for i in range(1, 5)]
    e.other_callback = T.fake_function(e, lambda emu: emu.set_r(3, 0))

    def peek(pid):
        def fn(emu):
            L = emu.cl
            L.rec("Peek", (pid, L.lab(emu.r(3)), T.s32(emu.r(4)), T.s32(emu.r(5)), T.s32(emu.r(6))))
            action = L.ret("pact", 0)
            if action == 1:
                w32(emu, G["peek"], 0)
            elif action == 2:
                w32(emu, G["peek"], emu.pump_peeks[2 if pid == 1 else 1])
        return fn

    e.pump_peeks = [0, T.fake_function(e, peek(1)), T.fake_function(e, peek(2))]

    def tick(emu):
        emu.cl.rec("tick", (emu.cl.lab(emu.r(3)),))
        emu.set_r(3, T.u32(emu.cl.ret("tick", 0)))

    vtable = T.persist_alloc(e, 0x10, bytes(0x10))
    e.write(vtable + 8, struct.pack(">I", T.fake_function(e, tick)))
    e.pump_clock_vtable = vtable


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(FN.values())
    S.real.update(REAL.values())
    S.real.update(REAL_HELPERS)
    us = lambda e: e.pump   # noqa: E731

    def op_new_array(e):
        size = e.r(3)
        e.set_r(3, e.alloc(max(size, 4), b"\xa5" * max(size, 4)))
        us(e).heap[1] += 1

    S.imp("__nwa__FUl", op_new_array)
    S.imp("__assertion_failed", lambda e: e.cl.rec("assert", (T.s32(e.r(5)),), (e.cstr(e.r(3)), e.cstr(e.r(4)))))
    S.internal(CALC_CLIENT_AREA, lambda e: None)

    def default_callback(e):
        # the handler every window starts with: records and answers "Hdef"
        e.cl.rec("Default", (e.cl.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6))))
        e.set_r(3, T.u32(e.cl.ret("Hdef", 0)))

    def highest_z(e):
        # highestZ: records its two arguments and answers "highestZ"
        e.cl.rec("highestZ", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))
        e.set_r(3, T.u32(e.cl.ret("highestZ", 0)))

    def postcard(e):
        # UI_ActuallyDoPostcard: records, and clears the request unless "pc.clear" says not to
        e.cl.rec("Postcard")
        if e.cl.ret("pc.clear", 1) != 0:
            w32(e, PG["pc_wanted"], 0)

    S.internal(DEFAULT_CALLBACK, default_callback)
    S.internal(HIGHEST_Z, highest_z)
    S.internal(POSTCARD, postcard)
    S.internal(MUTEX_LOCK, lambda e: e.cl.rec("lock", (e.cl.lab(e.r(3)),)))
    S.internal(MUTEX_UNLOCK, lambda e: e.cl.rec("unlock", (e.cl.lab(e.r(3)),)))

    def get_const_string(e):
        e.cl.rec("GetConstString", (e.cl.lab(e.r(3)), e.r(4)))
        e.set_r(3, us(e).texts[e.cl.ret("text", 0) % TEXTS])

    S.imp("GetConstString__8TbITableCFUl", get_const_string)

    def convert_multi(e):
        e.cl.rec("ConvertMulti", (e.cl.lab(e.r(3)),))
        e.set_r(3, us(e).wides[e.cl.ret("wide", 0) % TEXTS])

    S.internal(CONVERT_MULTI, convert_multi)

    def dialog_box(e):
        world, L = us(e), e.cl
        params = e.r(3)
        text = e.u32(params)
        red, green, blue = e.read(params + 4, 3)
        value8, valueC = struct.unpack(">ii", e.read(params + 8, 8))
        L.rec("DialogBox", (L.lab(text), (red << 16) | (green << 8) | blue, value8, valueC, T.s32(e.r(4)), world.func_label(e.r(5)),
                            T.s32(e.r(6)), world.func_label(e.r(7)), T.s32(e.r(8)), world.func_label(e.r(9)), e.r(10) & 0xFF))
        e.set_r(3, world.window(L.ret("dialog", -1)))

    S.internal(DIALOG_BOX, dialog_box)
    S.internal(SET_CALLBACK, lambda e: e.cl.rec("SetCallback", (e.cl.lab(e.r(3)), us(e).func_label(e.r(4)))))

    def pop_front(e):
        base = PG["deque"]
        start, size = e.u32(base + 0x14) + 1, e.u32(base + 0x18) - 1
        w32(e, base + 0x18, size)
        w32(e, base + 0x14, DEQUE_CHUNK >> 1 if size == 0 else start)

    S.internal(POP_FRONT, pop_front)
    return S


# ======================================================================= the host side


def open_host(lib):
    if getattr(lib, "_pump_open", False):
        return
    lib.tpw_pump_op.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_pump_op.restype = C.c_int64
    lib.tpw_pump_make_window.argtypes = [C.c_int32] * 8
    lib.tpw_pump_make_window.restype = C.c_int64
    lib.tpw_pump_make_queues.argtypes = [C.c_int32] * 3
    lib.tpw_pump_dump_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pump_dump_globals.restype = C.c_int32
    lib.tpw_pump_dump_window.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_pump_dump_queue.argtypes = [C.c_int32, C.POINTER(C.c_int64)]
    lib.tpw_pump_dump_queue.restype = C.c_int32
    lib.tpw_pump_dump_timed.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pump_dump_timed.restype = C.c_int32
    lib.tpw_pump_dump_timers.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pump_dump_timers.restype = C.c_int32
    lib.tpw_pump_dump_errors.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_pump_dump_errors.restype = C.c_int32
    lib.tpw_pump_heap.argtypes = [C.POINTER(C.c_int64)]
    lib._pump_open = True


def ints(*values):
    return (C.c_int64 * 12)(*values, *([0] * (12 - len(values))))


class Host:
    """One case on the host side: the same fixtures and operations through the library."""

    def __init__(self, lib, case):
        self.lib = lib
        open_host(lib)
        lib.tpw_pump_reset()
        CL.host_begin(lib, {}, case.get("q"))
        lib.tpw_pump_labels()
        C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
        specs = case.get("windows", [])
        for slot, (parent, attributes, handler, destroyed) in enumerate(specs):
            lib.tpw_pump_make_window(slot, parent, T.s32(attributes), slot * 7 - 3, slot, slot, slot + 50, slot + 50)
        for slot, (parent, previous, following, child) in enumerate(link_table([s[0] for s in specs])):
            lib.tpw_pump_op(2, ints(slot, parent, previous, following, child))
        for slot, (parent, attributes, handler, destroyed) in enumerate(specs):
            lib.tpw_pump_op(1, ints(slot, attributes, destroyed, handler))
        lib.tpw_pump_make_queues(*case.get("queues", (8, 8, 4)))
        lib.tpw_pump_heap_clear()
        C.c_int32.in_dll(lib, "tpw_cl_n").value = 0
        C.c_int32.in_dll(lib, "tpw_cl_on").value = 1

    def op(self, code, a):
        return self.lib.tpw_pump_op(code, ints(*a))

    def dump_state(self, made, queue_made):
        lib = self.lib
        buf = (C.c_int64 * 8192)()
        n = lib.tpw_pump_dump_globals(buf)
        state = dict(globals=list(buf[:n]))
        windows = []
        for slot in range(SLOTS):
            if made[slot]:
                lib.tpw_pump_dump_window(slot, buf)
                windows.append(list(buf[:WINDOW_FIELDS]))
            else:
                windows.append([0])
        state["windows"] = windows
        state["queues"] = [list(buf[:lib.tpw_pump_dump_queue(k, buf)]) for k in range(2)]
        state["timed"] = list(buf[:lib.tpw_pump_dump_timed(buf)])
        state["timers"] = list(buf[:lib.tpw_pump_dump_timers(buf)])
        state["errors"] = list(buf[:lib.tpw_pump_dump_errors(buf)])
        heap = (C.c_int64 * 4)()
        lib.tpw_pump_heap(heap)
        state["heap"] = list(heap)
        state["log"] = C.c_int32.in_dll(lib, "tpw_cl_n").value
        return state


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


@with_stubs
def pump_ppc(e, case):
    world = Ppc(e, case)
    steps, ran = [], []
    try:
        for op in case["ops"]:
            answer = world.op(op[0], op[1])
            if answer is None:
                continue
            ran.append(dict(op=op, made=list(world.made)))
            steps.append(dict(op=op[0], answer=answer, state=world.dump_state()))
        RAN[id(case)] = ran
        return dict(steps=steps, log=list(world.L.log))
    finally:
        world.restore()


def pump_host(lib, case):
    host = Host(lib, case)
    steps = []
    try:
        for step in RAN.pop(id(case)):
            op = step["op"]
            answer = host.op(op[0], op[1])
            steps.append(dict(op=op[0], answer=answer, state=host.dump_state(step["made"], None)))
    finally:
        log = CL.host_end(lib)
    return dict(steps=steps, log=log)


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    make_fakes(e)
    STUBS = build_stubs(e)


TESTS = []


# ======================================================================= scripts: building blocks

INVAL = -0x7F7F7F80   # 0x80808080, what InvalidateMsgs writes over a message's words
ORDINARY = [1, 2, 3, 4, 0x10, 0x15, 0x1E, 0x1000, 0x10001, 0x10002, 0x10003, 0x10009, 0x1000D, 0, -1, 0x7FFFFFFF, -0x80000000]
KEYS = [0x1000A, 0x1000B, 0x1000C]
VALUES = [0, 1, -1, 2, 0x41, 0x42, 0x10041, 0x1234, 0x12345678, -0x7FFFFFFF, 0x7FFFFFFF, 0x10000, 0xFFFF, -0x80000000, 7]
KEY_ARGS = [0x41, 0x42, 0x10041, 0x1234, 0x7FFF, 0xFFFF, -1, 0, 0x43, 0x44, 0x20041]
KEY_FLAGS = [0, 0x100000, 0x200000, 0x800000, 0xFF0000, 0x1000000, 0x7F0000, 0x120123, 0x0F0010, -1, 0x1230000]
TICKS = [0, 1, 100, 1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFF00, 0x12345678]
ATTRIBUTES = [0, 1, 2, 3, 0x400, 0x401, 0x10000001, 0xF0000001, 0xF0000000, 0x80000000, 0x7FFFFFFF, 0xFFFFFFFF, 0x10001, 0x100]


def forest(parents, attributes=None, handlers=None, destroyed=None):
    """The windows of a case: (parent, attributes, handler, destroyed byte) of each of the forest's windows."""
    n = len(parents)
    return [(parents[i], attributes[i] if attributes else 1, handlers[i] if handlers else 1 + i % 4, destroyed[i] if destroyed else 0)
            for i in range(n)]


TREE = [-1, 0, 0, 1, 3, 4]   # a root, two children, and a chain of three under the first


def case_of(ops, windows=None, queues=(8, 8, 4), q=None, **extra):
    case = dict(windows=windows if windows is not None else forest(TREE), queues=queues, ops=ops)
    if q:
        case["q"] = q
    case.update(extra)
    return case


def wire(root=0, general=1, draw=2, timed=1, timers=6):
    """The globals the UI start-up sets: the root window, the two queues, the timed queue and the timer table."""
    ops = []
    if root is not None:
        ops.append((3, (1, root)))
    return ops + [(3, (11, general)), (3, (12, draw)), (3, (13, timed)), (3, (15, timers))]


def at_index(count, i):
    """Where the i-th message added to a fresh queue of `count` slots is (Add writes at the tail and walks it down)."""
    return (count - 1 - i) % count


# ======================================================================= test 1: the queues' Post, Peek and GetNumQd


def post_edges():
    # rings of every size: filled, emptied, wrapped
    for count in (1, 2, 3, 4, 8):
        ops = wire() + [(31, (0,)), (32, (0,)), (33, (0,))]
        for i in range(count + 2):
            ops.append((4, (0, i % 6, ORDINARY[i % len(ORDINARY)], VALUES[i % len(VALUES)], i)))
        ops += [(33, (0,)), (32, (0,))]
        for i in range(count + 3):
            ops += [(31, (0,)), (33, (0,)), (32, (0,))]
        for i in range(count):
            ops.append((4, (0, i % 6, 0x15 + i, i, -i)))
            if i % 2:
                ops.append((31, (0,)))
        ops += [(31, (0,))] * count
        yield case_of(ops, queues=(count, 8, 4), q={"H1": [1, 2, 3], "H2": [-1], "H3": [0, 7], "H4": [0x7FFFFFFF]})
    # a queue that was constructed and not made: no entries, an uninitialised count of queued messages
    yield case_of([(31, (0,)), (32, (0,)), (33, (0,)), (31, (1,)), (32, (1,)), (33, (1,))], queues=(0, 0, 4))
    # a free head, a window poked out of a waiting message, a message of a window that was destroyed or has no handler
    ops = [(4, (0, 1, 5, 6, 7)), (4, (0, 2, 8, 9, 10)), (5, (0, 7, 0, -1)), (32, (0,)), (31, (0,)), (32, (0,)), (31, (0,)),
           (4, (0, 3, 1, 2, 3)), (4, (0, 4, 4, 4, 4)), (31, (0,)), (31, (0,)), (31, (0,))]
    yield case_of(ops, windows=forest(TREE, handlers=[1, 2, 0, 3, 4, 5], destroyed=[0, 0, 0, 1, 0x80, 0xFF]))
    # InvalidateMsgs' mark in any word of a message: it is dropped without a call and the slot is freed
    for field in (1, 2, 3):
        for message in (0x15, 0x1000A, 0x1000B, 0x1000C):
            for others in (0, 1):
                ops = wire() + [(4, (0, 1, message, 0x41, 0x100000)), (4, (0, 2, 0x15, 5, 6)), (5, (0, 7, field, INVAL))]
                if others:
                    ops += [(5, (0, 7, 1 if field != 1 else 2, 0x7777)), (5, (0, 7, 3 if field != 3 else 2, 0x6666))]
                ops += [(32, (0,)), (33, (0,)), (31, (0,)), (32, (0,)), (31, (0,)), (31, (0,))]
                yield case_of(ops, q={"H1": [5], "H2": [6]})
    # the three key messages against every set-up of the key capture, the keyboard window and the focus window
    # (windows: 0 takes the pointer (attribute 1), 1 does not, 2 and 3 do with other bits)
    keys_world = forest([-1, -1, -1, -1], attributes=[1, 0, 3, 0x10001], handlers=[1, 2, 3, 4])
    for keys in range(4):
        for capture in (-1, 0, 1, 2):
            for keyboard in (-1, 0, 1, 2):
                for focus in (-1, 0, 1, 3):
                    ops = wire(None) + [(3, (7, keys)), (3, (6, capture)), (3, (4, keyboard)), (3, (5, focus))]
                    n = 0
                    for message in KEYS:
                        for a, b in ((0x41, 0), (0x42, 0x100000), (0x10041, 0x200000), (0x1234, 0xFF0000), (0x41, 0x130020), (0x7FFF, 0x800000)):
                            ops += [(4, (0, 0, message, a, b)), (31, (0,))]
                            n += 1
                            if n % 5 == 0:
                                ops.append((4, (0, 0, 0x15, a, b)))
                                ops.append((31, (0,)))
                    yield case_of(ops, windows=keys_world, queues=(8, 8, 4), q={"H1": [1], "H2": [2], "H3": [3], "H4": [4]})
    # every key and modifier of the three lists
    for keys in (1, 2, 3):
        ops = wire(None) + [(3, (7, keys)), (3, (6, 0)), (3, (4, 1)), (3, (5, 2))]
        for a in KEY_ARGS:
            for b in KEY_FLAGS:
                ops += [(4, (0, 3, 0x1000A, a, b)), (31, (0,))]
        yield case_of(ops, windows=keys_world, queues=(8, 8, 4))
    # handlers that act: flush the queue, post to it, destroy, unlink
    for action in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10):
        for count in (2, 8):
            ops = wire() + [(3, (10, 1))]
            for i in range(count + 1):
                ops.append((4, (0, 1 + i % 4, 0x15 + i, i, i)))
            ops += [(31, (0,)), (33, (0,)), (32, (0,)), (31, (0,)), (31, (0,)), (33, (0,)), (4, (0, 1, 9, 9, 9)), (31, (0,)),
                    (31, (0,)), (31, (0,)), (33, (0,))]
            yield case_of(ops, queues=(count, count, 4),
                          q={"act": [action, 0, action, 0], "actarg": [2, 0x33, 4, 5, 3, 0x1000A, 6, 7], "pact": [0]})
    # the counters when the handler flushes, then posts again
    ops = wire() + [(4, (0, 1, 0x15, 1, 2)), (4, (0, 2, 0x16, 3, 4)), (31, (0,)), (33, (0,)), (4, (0, 1, 0x17, 5, 6)), (31, (0,)),
                    (33, (0,)), (31, (0,)), (33, (0,)), (31, (0,))]
    yield case_of(ops, queues=(4, 4, 2), q={"act": [4, 0, 0, 0]})
    # words poked by hand: the head and tail anywhere, the count of queued messages anything
    for head in range(4):
        for queued in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
            ops = [(6, (0, head, (head + 1) % 4, queued)), (33, (0,)), (4, (0, 1, 0x15, 1, 1)), (32, (0,)), (31, (0,)), (33, (0,)),
                   (5, (0, head, 0, 1)), (5, (0, head, 1, 0x20)), (32, (0,)), (31, (0,)), (33, (0,)), (31, (0,)), (32, (1,)),
                   (33, (1,))]
            yield case_of(ops, queues=(4, 4, 2))


def post_random(r):
    return rnd_case(r, "queue")


TESTS.append(FnTest("InterfaceMessageQueue::Post, Peek, GetNumQd", pump_ppc, pump_host, edge=post_edges, rnd=post_random))


# ======================================================================= random worlds and scripts


def rnd_attr(r):
    return r.choice([1, 1, 1, 3, 0, 2, 0x401, r.choice(ATTRIBUTES), r.getrandbits(32)])


def rnd_windows(r, count=None):
    """A random forest (a parent always has the lower number): attributes, handlers (0 none, 1 to 4 scripted, 5 the default,
    6 any other), destroyed bytes."""
    n = count or r.randint(1, SLOTS)
    chain = r.random() < 0.3
    parents = []
    for s in range(n):
        if s == 0 or r.random() < 0.1:
            parents.append(-1)
        elif chain and r.random() < 0.7:
            parents.append(s - 1)
        else:
            parents.append(r.randrange(s))
    return [(parents[s], rnd_attr(r), r.choice([0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 6]), r.choice([0, 0, 0, 0, 0, 1, 0x80, 0xFF])) for s in range(n)]


def rnd_queues(r):
    return (r.choice([0, 1, 2, 3, 4, 8, 8]), r.choice([0, 1, 2, 3, 4, 8, 8]), r.choice([-1, 0, 1, 3, 6]))


def rnd_q(r, base):
    """The call log's return queues: ticks (from `base`), handler actions and answers, texts, the dialog's window."""
    ticks = [base]
    for _ in range(r.randint(0, 12)):
        base = (base + r.choice([0, 1, 5, 50, 300, 1000, -3])) & MINUS
        ticks.append(base)
    q = {"tick": ticks, "act": [r.choice([0] * 6 + [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]) for _ in range(r.randint(0, 10))] + [0],
         "actarg": [r.choice([-1, 0, 1, 2, 3, 4, 5, r.choice(ORDINARY + KEYS), r.choice(VALUES)]) for _ in range(r.randint(1, 12))]}
    for h in (1, 2, 3, 4):
        q[f"H{h}"] = [r.choice([0, 0, 1, -1, 0x7FFFFFFF, r.randint(-1000, 1000)]) for _ in range(r.randint(0, 4))]
    q["Hdef"] = [r.choice([0, 3, -1]) for _ in range(r.randint(0, 3))]
    q["pact"] = [r.choice([0, 0, 0, 1, 2]) for _ in range(r.randint(0, 4))] + [0]
    q["text"] = [r.randrange(3) for _ in range(r.randint(0, 4))]
    q["wide"] = [r.randrange(3) for _ in range(r.randint(0, 4))]
    q["dialog"] = [r.randint(-1, 11) for _ in range(r.randint(0, 4))]
    q["highestZ"] = [r.choice([-1, 0, 1, 0x22, 0x23, 0x24, 100, -0x80000000, 0x7FFFFFFF]) for _ in range(r.randint(0, 4))]
    q["pc.clear"] = [r.randrange(2) for _ in range(r.randint(0, 3))]
    return q


def rnd_wiring(r, n, queues):
    """Some of the UI start-up's globals, at random (each one may stay unset)."""
    ops = []
    if r.random() < 0.85:
        ops.append((3, (1, r.choice([0, 0, 0, r.randrange(n)]))))
    if r.random() < 0.85:
        ops.append((3, (11, r.choice([1, 2, 1, 2, 0]))))
    if r.random() < 0.85:
        ops.append((3, (12, r.choice([2, 1, 2, 0]))))
    if r.random() < 0.8:
        ops.append((3, (13, 1)))
    if r.random() < 0.8:
        ops.append((3, (15, r.choice([6, 6, 3, 1, 0]))))
    for which in (4, 5, 6):
        if r.random() < 0.3:
            ops.append((3, (which, r.randint(-1, n - 1))))
    if r.random() < 0.3:
        ops.append((3, (7, r.randint(0, 3))))
    if r.random() < 0.3:
        ops.append((3, (10, r.randint(0, 2))))
    return ops


WEIGHTS = {
    "queue": dict(add=8, poke=3, words=1, global_=2, win=1, post=8, peek=3, numqd=2, links=0),
    "timed": dict(timedpoke=7, deferred=3, timedproc=5, ticks=2, global_=1, win=1, add=1, post=1),
    "dispatch": dict(add=8, poke=1, dispatch=5, draw=4, sendmsg=4, callh=2, access=2, global_=3, win=2, links=1),
    "frame": dict(sendframe=3, onframe=4, win=4, links=3, global_=3, sendmsg=1, access=1),
    "timers": dict(timerpoke=6, addtimer=5, service=6, global_=3, ticks=1),
    "misc": dict(highz=4, postcard=5, global_=3, sendmsg=1),
    "errors": dict(error=7, tickerrors=7, clearerr=1, global_=3, highz=1),
    "process": dict(add=5, poke=1, timedpoke=2, timerpoke=3, error=3, global_=3, win=1, links=1, process=5, processui=3, addtimer=2,
                    postcard=2, tickerrors=1, dispatch=1, draw=1, sendframe=1, deferred=1, service=1),
}


def rnd_message(r):
    k = r.random()
    if k < 0.25:
        return r.choice(KEYS)
    return r.choice(ORDINARY)


def rnd_op(r, n, queues, focus, base):
    kinds = WEIGHTS[focus]
    kind = r.choices(list(kinds), list(kinds.values()))[0]
    slot = lambda: r.randrange(n)   # noqa: E731
    nullable = lambda: r.randint(-1, n - 1)   # noqa: E731
    value = lambda: r.choice(VALUES)   # noqa: E731
    if kind == "add":
        k = r.randrange(2)
        m = rnd_message(r)
        return (4, (k, nullable(), m, r.choice(VALUES + [INVAL]) if r.random() < 0.9 else r.choice(KEY_ARGS), r.choice(KEY_FLAGS + VALUES + [INVAL])))
    if kind == "poke":
        k = r.randrange(2)
        count = queues[k]
        return (5, (k, r.randrange(max(1, count)), r.randrange(4), r.choice([INVAL, INVAL, 0, -1] + ORDINARY)))
    if kind == "words":
        k = r.randrange(2)
        count = max(1, queues[k])
        return (6, (k, r.randrange(count), r.randrange(count), r.choice([0, 1, -1, 5, 0x7FFFFFFF, -0x80000000])))
    if kind == "timedpoke":
        return (7, (r.randrange(max(1, queues[2])), nullable(), r.choice(ORDINARY + KEYS), value(), value(), r.choice(TICKS + [0, 0, 5])))
    if kind == "timerpoke":
        i = r.randrange(TIMERS)
        return (8, (i, nullable(), (base + r.choice([-3000, -300, -100, -20, -1, 0, 5, 500, 40, 0x7FFFFFFF])) & MINUS,
                    r.choice([1, 2, 10, 50, 100, 300, 1000, 0x7FFFFFFF, r.randint(1, 500)]), value(), r.randrange(2),
                    r.choice([0, 0, 1]), r.choice([-1] + list(range(i + 1, TIMERS))), r.randint(-1, TIMERS - 1)))
    if kind == "global_":
        which = r.choice([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18, 19, 20])
        v = {1: nullable(), 2: nullable(), 3: nullable(), 4: nullable(), 5: nullable(), 6: nullable(), 7: r.randint(0, 3),
             8: r.choice([0, 1, 1, 1, 2, -1]), 9: r.choice([0, 0, 1, 2]), 10: r.randint(0, 2), 11: r.randint(0, 2),
             12: r.randint(0, 2), 13: r.randrange(2), 15: r.choice([0, 6, 6, 3, 1]), 16: r.randint(-1, TIMERS - 1),
             17: r.choice([0, 1, 2, 3, -1, -5]), 18: r.randrange(2), 19: r.choice([-1, -1] + list(range(n))),
             20: r.choice([0, 5, -1])}[which]
        return (3, (which, v))
    if kind == "win":
        return (1, (slot(), rnd_attr(r), r.choice([0, 0, 0, 1, 0x80, 0xFF]), r.choice([0, 1, 2, 3, 4, 5, 6])))
    if kind == "links":
        # a parent before the window, a next sibling and a child after it (so that no loop of links is made)
        s_ = slot()
        later = lambda: r.choice([-1] + list(range(s_ + 1, n)))   # noqa: E731
        return (2, (s_, r.choice([-1] + list(range(s_))), nullable(), later(), later()))
    if kind == "error":
        return (9, (r.randrange(TABLES), r.choice([0, 1, 5, 0x7FFFFFFF, 0xFFFFFFFF, 12345])))
    if kind == "clearerr":
        return (10, ())
    if kind == "post":
        return (31, (r.randrange(2),))
    if kind == "peek":
        return (32, (r.randrange(2),))
    if kind == "numqd":
        return (33, (r.randrange(2),))
    if kind == "timedproc":
        return (34, ())
    if kind == "dispatch":
        return (22, (r.choice([-1, 0, 1, 2, 3, 5, 8, 9, 20, 0x400]),))
    if kind == "draw":
        return (23, (r.choice([-1, 0, 1, 2, 3, 5, 8, 9, 20, 0x1000]),))
    if kind == "deferred":
        return (24, ())
    if kind == "sendframe":
        return (25, ())
    if kind == "onframe":
        return (26, (slot(),))
    if kind == "sendmsg":
        return (27, (nullable(), rnd_message(r), value(), value()))
    if kind == "service":
        return (28, ())
    if kind == "addtimer":
        return (29, (nullable(), r.choice([1, 2, 10, 50, 100, 300, 1, 2, 10, 50, 100, 300, 1, 2, 10, 50, 100, 300, 0, -1, 0x7FFFFFFF]), value()))
    if kind == "ticks":
        return (30, ())
    if kind == "callh":
        return (35, (slot(), rnd_message(r), value(), value()))
    if kind == "access":
        return (36, (slot(), r.randrange(4), value()))
    if kind == "highz":
        return (37, (nullable(), r.randint(0, 2)))
    if kind == "postcard":
        return (38, ())
    if kind == "tickerrors":
        return (39, ())
    if kind == "process":
        return (20, ())
    if kind == "processui":
        return (21, ())
    raise ValueError(kind)


def rnd_case(r, focus):
    windows = rnd_windows(r)
    n = len(windows)
    queues = rnd_queues(r)
    ops = rnd_wiring(r, n, queues)
    base = r.choice(TICKS + [r.getrandbits(32)])
    for _ in range(r.randint(8, 36)):
        ops.append(rnd_op(r, n, queues, focus, base))
    return dict(windows=windows, queues=queues, ops=ops, q=rnd_q(r, base), deque_start=r.randrange(DEQUE_MAP))


# ======================================================================= test 2: the timed queue, the deferred dispatch, the clock


def timed_edges():
    # IveGotTicks: the clock's answer as it comes
    for t in TICKS + [2, 3]:
        yield case_of([(30, ())], q={"tick": [t]})
    yield case_of([(30, ())] * (len(TICKS) + 1), q={"tick": TICKS})
    # UI_DispatchDeferredMessages: no timed queue (-1), an empty one, with entries
    yield case_of([(24, ()), (3, (13, 1)), (24, ()), (7, (0, 1, 0x15, 1, 2, 5)), (24, ()), (24, ()), (3, (13, 0)), (24, ())],
                  q={"tick": [10, 10, 10, 10]})
    # the timed queue's Process: every size, every time against every clock
    for size in (-1, 0, 1, 2, 3, 6):
        for now in TICKS:
            times = [0, now, (now - 1) & MINUS, (now + 1) & MINUS, 1, MINUS, 2, (now + 0x80000000) & MINUS, 0x7FFFFFFF]
            for shift in range(3):
                ops = wire()
                for i in range(max(size, 0)):
                    ops.append((7, (i, (i + shift) % 6, [0x15, 0x1000A, 0x16, 3, 0x1E, 0x10001][i % 6], i, -i, times[(i + shift * 3) % len(times)])))
                ops += [(34, ()), (34, ()), (24, ()), (24, ())]
                yield case_of(ops, queues=(4, 4, size), q={"tick": [now], "H1": [1], "H2": [2], "H3": [3], "H4": [4]})
    # the clock changes between the slots: the later ones become due, or stop being
    for ticks in ([5, 10, 15, 20, 25], [20, 15, 10, 5, 0], [0xFFFFFFF0, 5, 0xFFFFFFF0, 5, 7], [9, 9, 9, 9, 9]):
        ops = wire() + [(7, (i, i, 0x15 + i, i, i, 10 + 5 * i)) for i in range(5)] + [(34, ()), (34, ()), (34, ())]
        yield case_of(ops, queues=(4, 4, 5), q={"tick": ticks})
    # slots with windows that have no handler, the default handler, or are being destroyed (all still get their message)
    windows = forest(TREE, handlers=[0, 5, 1, 2, 6, 3], destroyed=[0, 0, 1, 0x80, 0xFF, 0])
    ops = wire() + [(7, (i, i, 0x20 + i, i, 7, 100)) for i in range(6)] + [(34, ()), (7, (0, 1, 1, 1, 1, 50)), (34, ())]
    yield case_of(ops, windows=windows, queues=(4, 4, 6), q={"tick": [100, 100, 100, 100, 100, 100, 100, 100, 50, 50], "Hdef": [9]})
    # handlers that act on the window or the queues while they are called
    for action in range(1, 11):
        for first in range(3):
            ops = wire() + [(7, (i, 1 + i % 4, 0x30 + i, 0, 0, 5)) for i in range(4)] + [(34, ()), (34, ()), (24, ())]
            yield case_of(ops, queues=(4, 4, 4), q={"tick": [10], "act": [0] * first + [action, 0], "actarg": [2, 0x15, 1, 2, 3]})


def timed_random(r):
    return rnd_case(r, "timed")


TESTS.append(FnTest("InterfaceTimedMessages::Process, UI_DispatchDeferredMessages, IveGotTicks", pump_ppc, pump_host, edge=timed_edges,
                    rnd=timed_random))


# ======================================================================= test 3: the dispatch, UI_SendMessage and the window members


def fill(count, queue=0, message_base=0x15, windows=6):
    """`count` messages added to a queue, to windows in turn, each different."""
    return [(4, (queue, 1 + i % (windows - 1), message_base + i, 100 + i, -i)) for i in range(count)]


def dispatch_edges():
    # the general queue: every length against every limit, without a peek callback and with each of the two
    for peek in (0, 1, 2):
        for count in (0, 1, 2, 5, 8):
            for limit in (-1, 0, 1, 2, 4, 5, 8, 9, 100):
                ops = wire() + [(3, (10, peek))] + fill(count) + [(22, (limit,)), (33, (0,)), (22, (limit,)), (33, (0,))]
                yield case_of(ops, q={"H1": [1], "H2": [2], "H3": [3], "H4": [4]})
    # the draw queue
    for count in (0, 1, 2, 5, 8):
        for limit in (-1, 0, 1, 2, 4, 5, 8, 9, 100):
            ops = wire() + fill(count, 1, 1) + [(23, (limit,)), (33, (1,)), (23, (limit,)), (33, (1,))]
            yield case_of(ops, q={"H1": [1], "H2": [2]})
    # no queue: the general one, the draw one, both
    for general, draw in ((0, 2), (1, 0), (0, 0)):
        ops = wire(general=general, draw=draw) + fill(3) + [(22, (5,)), (23, (5,))]
        yield case_of(ops)
    # a queue constructed and not made
    yield case_of(wire() + [(22, (5,)), (23, (5,)), (3, (10, 1)), (22, (5,)), (22, (0,))], queues=(0, 0, 4))
    # messages with the invalid mark are not shown to the peek callback and not delivered; the callback may clear or swap itself
    for peek in (1, 2):
        for field in (1, 2, 3):
            ops = wire() + [(3, (10, peek))] + fill(5) + [(5, (0, 7, field, INVAL)), (5, (0, 5, field, INVAL)), (22, (9,)), (33, (0,))]
            yield case_of(ops, q={"H1": [1]})
    for pact in (1, 2):
        for first in range(4):
            for limit in (2, 6):
                ops = wire() + [(3, (10, 1))] + fill(5) + [(22, (limit,)), (33, (0,)), (22, (limit,))]
                yield case_of(ops, q={"pact": [0] * first + [pact, 0]})
    for action in range(1, 11):
        for peek in (0, 1):
            for first in range(3):
                ops = wire() + [(3, (10, peek))] + fill(6) + [(22, (9,)), (33, (0,)), (23, (3,)), (22, (9,))]
                yield case_of(ops, q={"act": [0] * first + [action, 0], "actarg": [2, 0x15, 3, 4]})
    # the limits of the real pass: more than 0x400 and 0x1000 messages queued
    for peek in (0, 1):
        ops = wire() + [(3, (10, peek)), (11, (0, 0x420, 5, 0x15, 0)), (22, (0x400,)), (33, (0,)), (22, (0x400,)), (33, (0,))]
        yield case_of(ops, queues=(0x430, 8, 4), windows=forest(TREE, handlers=[0] * 6))
    ops = wire() + [(11, (1, 0x1010, 5, 0x15, 0)), (23, (0x1000,)), (33, (1,)), (23, (0x1000,)), (33, (1,))]
    yield case_of(ops, queues=(8, 0x1020, 4), windows=forest(TREE, handlers=[0] * 6))
    # UI_SendMessage: every kind of window against each peek callback
    windows = forest([-1, 0, 0, 1], handlers=[1, 0, 5, 2], destroyed=[0, 0, 0, 1])
    for peek in (0, 1, 2):
        ops = wire() + [(3, (10, peek))]
        for slot in (-1, 0, 1, 2, 3):
            for message in (0x15, -1, 0x1E):
                ops.append((27, (slot, message, slot * 3, 77)))
        yield case_of(ops, windows=windows, q={"H1": [0, 5, -7, 0x7FFFFFFF], "H2": [4], "Hdef": [8]})
    # a destroyed byte of any value
    for byte in (0, 1, 0x7F, 0x80, 0xFF, 2):
        ops = [(1, (1, 1, byte, 2)), (27, (1, 5, 6, 7)), (35, (1, 5, 6, 7)), (36, (1, 0, 0))]
        yield case_of(ops, q={"H2": [3]})
    # CallWindowHandler and the accessors on every window of a tree
    for peek in (0, 2):
        ops = [(3, (10, peek))]
        for slot in range(6):
            ops += [(35, (slot, 0x15, slot, -slot)), (36, (slot, 0, 0)), (36, (slot, 1, slot)), (36, (slot, 1, -5)), (36, (slot, 2, 0)),
                    (36, (slot, 3, 0))]
        windows = forest(TREE, attributes=ATTRIBUTES[:6], handlers=[1, 0, 5, 6, 2, 3], destroyed=[0, 1, 0, 0x80, 0xFF, 0])
        yield case_of(ops, windows=windows, q={"H1": [11], "H2": [-12], "H3": [13], "Hdef": [14]})
    # handlers that act (including on the window the accessors then read)
    for action in range(1, 11):
        ops = [(35, (3, 1, 2, 3)), (36, (3, 0, 0)), (36, (3, 2, 0)), (36, (0, 1, 0)), (27, (3, 4, 5, 6)), (35, (0, 7, 8, 9)),
               (36, (1, 2, 0)), (36, (0, 1, 0))]
        yield case_of(ops, q={"act": [action, action, 0], "actarg": [2, 0x15, 1, 2]})


def dispatch_random(r):
    return rnd_case(r, "dispatch")


TESTS.append(FnTest("UI_DispatchMessages, UI_DispatchDrawMessages, UI_SendMessage, CallWindowHandler, AmIGettingDestroyed, GetChild, "
                    "GetNext, GetAttr", pump_ppc, pump_host, edge=dispatch_edges, rnd=dispatch_random))


# ======================================================================= test 4: onframe and UI_SendOnFrameMessages


def chain(n):
    return [-1] + list(range(n - 1))


SHAPES = [[-1], [-1, 0], chain(3), chain(5), chain(6), chain(7), chain(9), chain(12), [-1] + [0] * 11, [-1, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5],
          TREE, [-1, 0, 0, 1, 3, 4, 5, 6, 6, 2, 9, 10], [-1, -1, 0, 1]]


def frame_edges():
    for parents in SHAPES:
        n = len(parents)
        plain = forest(parents)
        base = wire(0) + [(3, (20, 5))]
        yield case_of(base + [(25, ()), (25, ()), (26, (0,))], windows=plain)
        for i in range(n):
            # one window being destroyed, one with no handler, one with the default handler: the walk goes on through them
            for kind in ("destroyed", "none", "default"):
                specs = list(plain)
                parent, attributes, handler, destroyed = specs[i]
                specs[i] = (parent, attributes, handler if kind == "destroyed" else 0 if kind == "none" else 5, 1 if kind == "destroyed" else 0)
                yield case_of(base + [(25, ()), (26, (i,))], windows=specs, q={"Hdef": [4]})
        for peek in (1, 2):
            yield case_of(base + [(3, (10, peek)), (25, ())], windows=plain)
        for i in range(0, n, max(1, n // 4)):
            yield case_of(base + [(26, (i,)), (25, ())], windows=plain)
        yield case_of([(3, (20, 7)), (25, ()), (3, (1, 0)), (25, ()), (3, (1, n - 1)), (25, ()), (26, (n - 1,))], windows=plain)
    # the handlers act while the tree is walked: unlink, detach, destroy, clear a handler, change the peek callback, post, flush
    for parents in (TREE, SHAPES[9], chain(7), [-1] + [0] * 6):
        n = len(parents)
        for action in (1, 2, 3, 4, 5, 6, 8, 9, 10):
            for first in range(min(n, 7)):
                for peek in (0, 1):
                    ops = wire(0) + [(3, (10, peek)), (25, ()), (25, ())]
                    yield case_of(ops, windows=forest(parents), q={"act": [0] * first + [action, 0], "actarg": [2, 0x15, 3, 4],
                                                                    "pact": [0, 0] + [1]})
    # a handler of window 0 that destroys the next sibling's subtree walk, with the peek callback clearing itself half way
    for pact in (1, 2):
        for first in range(6):
            yield case_of(wire(0) + [(3, (10, 1)), (25, ()), (25, ())], q={"pact": [0] * first + [pact, 0]})


def frame_random(r):
    return rnd_case(r, "frame")


TESTS.append(FnTest("onframe, UI_SendOnFrameMessages", pump_ppc, pump_host, edge=frame_edges, rnd=frame_random))


# ======================================================================= test 5: the timers


def list_ops(order, free_others=1):
    """The timer table with the timers `order` in the list (in that order, not free) and the others free."""
    ops = []
    for i in range(TIMERS):
        if i in order:
            at = order.index(i)
            ops.append((8, (i, 0, 1000 + i, 50 + i, 10 + i, 1, 0, order[at + 1] if at + 1 < len(order) else -1, order[at - 1] if at > 0 else -1)))
        else:
            ops.append((8, (i, -1, 0, 0, 0, 0, free_others, -1, -1)))
    return ops + [(3, (16, order[0] if order else -1))]


def timer_edges():
    # UI_AddTimer: no table (the assertion), tables of every size, filled, and filled again
    yield case_of(wire(timers=0) + [(29, (0, 10, 5)), (29, (1, 10, 5))], q={"tick": [7]})
    yield case_of([(3, (15, 3)), (3, (15, 0)), (29, (2, 20, 6))], q={"tick": [7]})
    for count in range(1, 7):
        ops = wire(timers=count) + [(29, (i % 6 if i % 3 else -1, [10, 1, 0x7FFFFFFF, -1, 0, 50, 100][i % 7], VALUES[i % len(VALUES)])) for i in range(count + 2)]
        yield case_of(ops, q={"tick": [100, 200, 0xFFFFFFFF, 3, 4, 5, 6, 7]})
    # free flags of any pattern, with the list empty or not
    for pattern in range(0, 64, 5):
        ops = wire() + [(8, (i, -1, 0, 0, 0, 0, (pattern >> i) & 1, -1, -1)) for i in range(TIMERS)] + [(29, (0, 5, 9))] * 3 + [(28, ())]
        yield case_of(ops, q={"tick": [11, 12, 13]})
    for order in ([0], [1], [3, 1, 0], [5, 4, 3, 2, 1], [2, 0], [0, 1, 2, 3, 4]):
        for adds in (1, 2, 3):
            ops = wire() + list_ops(order) + [(29, (i, 10 * i + 1, i)) for i in range(adds)] + [(28, ())]
            yield case_of(ops, q={"tick": [1000, 1020, 1040, 1060]})
    # the free flag taken from the first free timer only: flags that are not 0 or 1
    ops = wire() + [(8, (0, -1, 0, 0, 0, 0, 5, -1, -1)), (8, (1, -1, 0, 0, 0, 0, -1, -1, -1)), (29, (0, 5, 9)), (29, (0, 6, 8)), (29, (0, 7, 7))]
    yield case_of(ops, q={"tick": [1]})
    # UI_ServiceAllTimers: paused, nothing in the list, nothing due
    yield case_of(wire() + [(28, ())], q={"tick": [5]})
    for paused in (1, -1, 5):
        yield case_of(wire() + list_ops([0, 1]) + [(3, (9, paused)), (28, ()), (3, (9, 0)), (28, ())], q={"tick": [5000]})
    # one timer: the slice, how long since it started, where the clock is (the wrap), running, window, active
    for slice_ in (1, 7, 100, 0x7FFFFFFF):
        for elapsed in (-5, -1, 0, 1, slice_ - 1, slice_, slice_ + 1, 2 * slice_, 2 * slice_ + 1, 3 * slice_ + 2, 5 * slice_ + 3):
            if elapsed != T.s32(elapsed & MINUS) or elapsed < -slice_ - 6:
                continue
            for start in (1000, 0xFFFFFFF0, 0, 0x7FFFFFF0):
                tick = (start + elapsed) & MINUS
                ops = wire() + [(8, (0, 1, start, slice_, 77, 1, 0, -1, -1)), (3, (16, 0)), (28, ()), (28, ())]
                yield case_of(ops, q={"tick": [tick]})
    for running in (0, 1, 2, -1):
        for window in (-1, 1):
            for active in (1, 0, -1, 3):
                for general in (1, 0):
                    ops = wire(general=general) + [(8, (0, window, 100, 10, 55, running, 0, -1, -1)), (3, (16, 0)), (3, (8, active)), (28, ())]
                    yield case_of(ops, q={"tick": [135, 136]})
    # several timers, each reading the clock; the clock moving between them; a full queue
    for ticks in ([2000], [2000, 2000, 2100, 2100, 2200, 2200, 2300, 2300], [2300, 2200, 2100, 2000, 1900, 1800], [0xFFFFFF00, 50, 0xFFFFFF00, 50]):
        for order in ([0, 1, 2], [2, 0, 1], [0], [3, 2, 1, 0]):
            ops = wire() + list_ops(order) + [(8, (order[0], 1, 1900, 30, 70, 1, 0, order[1] if len(order) > 1 else -1, -1))] + [(28, ()), (28, ())]
            yield case_of(ops, q={"tick": ticks}, queues=(3, 3, 2))
    # the timer that is posted to a window whose message queue is full, or missing
    for size in (1, 2, 3):
        ops = wire() + [(8, (0, 2, 100, 5, 9, 1, 0, -1, -1)), (3, (16, 0)), (28, ()), (33, (0,)), (28, ()), (33, (0,))]
        yield case_of(ops, queues=(size, 4, 2), q={"tick": [130, 160]})
    # the message the timer posts is delivered later, with the pass's handlers
    ops = wire() + [(8, (0, 1, 0, 10, 8, 1, 0, -1, -1)), (3, (16, 0)), (28, ()), (22, (9,)), (28, ()), (22, (9,))]
    yield case_of(ops, q={"tick": [45, 90], "H2": [5]})


def timer_random(r):
    return rnd_case(r, "timers")


TESTS.append(FnTest("UI_AddTimer, UI_ServiceAllTimers", pump_ppc, pump_host, edge=timer_edges, rnd=timer_random))


# ======================================================================= test 6: UI_GetHighestZ, UI_PostcardPending


def misc_edges():
    # UI_GetHighestZ: no root, a root, a window given; the rectangle or none; what highestZ answers
    for root in (None, 0):
        for slot in (-1, 0, 3, 5):
            for rect in (0, 1, 2):
                ops = (wire(root) if root is not None else []) + [(37, (slot, rect)), (37, (slot, rect))]
                yield case_of(ops, q={"highestZ": [5, 0x22, -3, 0x7FFFFFFF]})
    yield case_of([(3, (1, 0)), (37, (-1, 0)), (3, (1, -1)), (37, (-1, 1)), (37, (2, 2)), (3, (1, 4)), (37, (-1, 2))],
                  q={"highestZ": [-0x80000000, 0, 9]})
    # UI_PostcardPending: the delay counts down to 0 and stops; the request is looked at only then
    for delay in (-3, -1, 0, 1, 2, 3, 0x7FFFFFFF):
        for wanted in (0, 1, -1, 2):
            ops = [(3, (17, delay)), (3, (18, wanted))] + [(38, ())] * 5 + [(3, (18, 1 - (wanted != 0))), (38, ()), (38, ())]
            yield case_of(ops)
    ops = [(3, (17, 2)), (3, (18, 1)), (38, ()), (3, (17, 2)), (38, ()), (38, ()), (38, ())]
    yield case_of(ops)


def misc_random(r):
    return rnd_case(r, "misc")


TESTS.append(FnTest("UI_GetHighestZ, UI_PostcardPending", pump_ppc, pump_host, edge=misc_edges, rnd=misc_random))


# ======================================================================= test 7: TickUIErrors


def errors_edges():
    # no error waiting: only the mutex, whatever else is up
    for dialog in (-1, 0):
        for root in (None, 0):
            yield case_of((wire(root) if root is not None else []) + [(3, (19, dialog)), (39, ()), (39, ())])
    # one error against the height of the interface, with and without a root window
    for root in (None, 0):
        for height in (-1, 0, 5, 0x22, 0x23, 0x24, 100, 0x7FFFFFFF, -0x80000000):
            for dialog in (-1, 2):
                ops = (wire(root) if root is not None else []) + [(9, (1, 7)), (3, (19, dialog)), (39, ()), (39, ())]
                yield case_of(ops, q={"highestZ": [height], "dialog": [3], "text": [1], "wide": [2]})
    # the dialog it makes: no window, any window; the next error waits for the dialog to be closed
    for answer in (-1, 0, 4, 5, 7):
        ops = wire() + [(9, (0, 1)), (9, (2, 0xFFFFFFFF)), (9, (1, 0)), (39, ()), (39, ()), (3, (19, -1)), (39, ()), (3, (19, -1)), (39, ()),
                        (39, ())]
        yield case_of(ops, q={"dialog": [answer, answer + 1 if answer > 0 else -1], "highestZ": [0], "text": [0, 1, 2], "wide": [2, 1, 0]})
    # the queue of errors over the blocks of the C++ deque: pushed and popped in turn, from every starting place
    for start in range(DEQUE_MAP):
        ops = wire()
        for i in range(14):
            ops += [(9, (i % 3, 100 + i))]
            if i % 3:
                ops += [(39, ()), (3, (19, -1))]
        ops += [(39, ()), (3, (19, -1))] * 6 + [(10, ()), (39, ())]
        yield case_of(ops, deque_start=start, q={"dialog": [2], "highestZ": [1], "text": [0, 1, 2], "wide": [1, 2, 0]})
    # more errors than the blocks hold: the later pushes are refused (on both sides)
    ops = wire() + [(9, (i % 3, i)) for i in range(24)] + [(39, ()), (3, (19, -1))] * 3 + [(9, (0, 99)), (10, ()), (9, (1, 5)), (39, ())]
    yield case_of(ops, q={"dialog": [3]})
    # TickUIErrors under the whole pass
    ops = wire() + [(9, (0, 3)), (21, ()), (3, (19, -1)), (20, ()), (3, (19, -1)), (21, ())]
    yield case_of(ops, q={"dialog": [1]})


def errors_random(r):
    return rnd_case(r, "errors")


TESTS.append(FnTest("TickUIErrors", pump_ppc, pump_host, edge=errors_edges, rnd=errors_random))


# ======================================================================= test 8: the whole pass


def pass_edges():
    # every subset of what a pass has to do, twice over
    features = ["errors", "timer", "postcard", "messages", "timed", "draw", "peek"]
    for subset in range(1 << len(features)):
        have = {name for k, name in enumerate(features) if (subset >> k) & 1}
        ops = wire(0)
        if "errors" in have:
            ops += [(9, (1, 4)), (9, (2, 5))]
        if "timer" in have:
            ops += [(8, (0, 2, 1000, 20, 8, 1, 0, -1, -1)), (3, (16, 0))]
        if "postcard" in have:
            ops += [(3, (17, 1)), (3, (18, 1))]
        if "messages" in have:
            ops += fill(4)
        if "timed" in have:
            ops += [(7, (0, 1, 0x40, 1, 2, 500)), (7, (1, 3, 0x41, 3, 4, 2000))]
        if "draw" in have:
            ops += fill(3, 1, 2)
        if "peek" in have:
            ops += [(3, (10, 1))]
        ops += [(20 + subset % 2, ()), (3, (19, -1)), (20 + (subset + 1) % 2, ()), (33, (0,)), (33, (1,))]
        yield case_of(ops, q={"tick": [1030, 1030, 1030, 1030, 1050, 1050, 1050], "dialog": [3], "highestZ": [4], "H1": [1], "H2": [2], "H3": [3]})
    # no queue, no root
    yield case_of(wire(None) + [(20, ()), (21, ())], q={"tick": [1]})
    yield case_of(wire(None, general=0) + [(20, ())])
    yield case_of(wire(0, draw=0) + [(21, ())])
    # a root with the whole tree, the on-frame messages and a draw message
    for parents in (TREE, SHAPES[9], chain(8)):
        yield case_of(wire(0) + [(20, ()), (20, ())], windows=forest(parents), q={"tick": [10]})
    # the limits of a pass: more waiting than it takes
    ops = wire() + [(11, (0, 0x410, 1, 0x15, 0)), (21, ()), (33, (0,)), (21, ()), (33, (0,))]
    yield case_of(ops, queues=(0x430, 8, 4), windows=forest(TREE, handlers=[0, 1, 0, 0, 0, 0]))
    ops = wire() + [(3, (10, 1)), (11, (0, 0x410, 1, 0x15, 0)), (21, ()), (33, (0,))]
    yield case_of(ops, queues=(0x430, 8, 4), windows=forest(TREE, handlers=[0, 1, 0, 0, 0, 0]))
    ops = wire() + [(11, (1, 0x1010, 5, 0x15, 0)), (21, ()), (33, (1,)), (21, ()), (33, (1,))]
    yield case_of(ops, queues=(8, 0x1020, 4), windows=forest(TREE, handlers=[0] * 6))
    # handlers that act during the pass
    for action in range(1, 11):
        for first in range(4):
            ops = wire(0) + fill(3) + fill(2, 1, 2) + [(7, (0, 1, 0x40, 1, 2, 5)), (20, ()), (20, ())]
            yield case_of(ops, q={"tick": [10], "act": [0] * first + [action, 0], "actarg": [2, 0x15, 1, 2], "pact": [0]})


def pass_random(r):
    return rnd_case(r, "process")


TESTS.append(FnTest("UIInterface::Process, ProcessUI", pump_ppc, pump_host, edge=pass_edges, rnd=pass_random))
