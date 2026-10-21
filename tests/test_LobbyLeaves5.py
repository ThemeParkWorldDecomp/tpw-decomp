"""Unit w64, the lobby's small leaves part 5, against libtpw_unit_LobbyLeaves5.dylib (tests/unit_LobbyLeaves5.cpp), in the main app
("SimTheme Park"):
  src/game/ui/ui_mainpanel2.cpp  HidePauseWindow 0x101a8744
  src/game/rides/RidePlacement.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/InterfaceStringHeap.cpp   InterfaceStringHeap::DeAllocate 0x101d7978
  src/game/ui/InterfaceMessageQueue.cpp, src/game/ui/InterfaceTimer.cpp, src/game/ui/UI_Base.cpp             UI_CloseSystem 0x101c8c2c, UI_CloseTimerSystem 0x101d8c2c, InterfaceMessageQueue::~InterfaceMessageQueue
                               0x101c7b24, InterfaceTimedMessages::~InterfaceTimedMessages 0x101c8214

A case is a world and an operation, built twice from one description: in emulator memory at the Mac's sizes for the original and through
tpw_l5_run for the translation. The world is the four windows (the root, focus, pointer and pause windows are each none or one of them),
the general and draw queues and the timed messages (each none, an object with slots, or an object without), the string heap's two tables
and four words, and the timer table (its cookie as the Mac's array new makes it) with its links and three timers outside it. The
operation runs with every callee outside the unit a fake that records the same event on both sides: UI_SendMessage and
UIMain_DecideKbCapture (HidePauseWindow), DrawObjectLibrary::RemoveAll, CursorLibrary::RemoveAll, the root window's deleting destructor (a
fake slot 4 of the table the window's word at +0x118 points to), operator delete and delete[]. The events of the window and of the two
RemoveAll fakes carry which globals are set at that moment and the delete counters, so the order of the frees and of the clearing shows.
The timer table's runtime calls (__destroy_new_array) are a fake that follows the runtime's rule (the destructor on each element from the
last to the first with flag -1, then the block freed); a NULL table is assumed to do nothing. The timer's destructor is the original's.

The dump compares the answer, every global as a place (0 NULL, a window 1 to 4, a queue, the table), which objects are still alive (an
object freed by delete loses its label), the words of the queues that are alive, the three outside timers (their links as places, so
the order of the table's destructors shows), the table's timers when the table was not freed, and the two delete counters.

Not tested: the destructors on a NULL object (the Mac tests for it; the host never calls with NULL), __destroy_new_array's own rules (not
in the dump: the runtime library is an import).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyLeaves5"
MASK = 0xFFFFFFFF

HIDE_PAUSE, DEALLOCATE = 0x101A8744, 0x101D7978
CLOSE_SYSTEM, CLOSE_TIMERS, QUEUE_DTOR, TIMED_DTOR = 0x101C8C2C, 0x101D8C2C, 0x101C7B24, 0x101C8214
SEND_MESSAGE, DECIDE_KB, DRAW_REMOVE_ALL, CURSOR_REMOVE_ALL = 0x101C9518, 0x1018116C, 0x101CD214, 0x101CC8EC
TIMER_DTOR = 0x101D830C
ROOT, FOCUS, POINTER, PAUSE = 0x10395C24, 0x10395C20, 0x10395C1C, 0x103949CC
MESSAGE_QUEUE, DRAW_QUEUE, TIMED = 0x10395C04, 0x10395C00, 0x10395BFC
TIMERS, TIMER_COUNT, TIMER_LIST = 0x10395C54, 0x10395C50, 0x10395C58
TABLE_A, TABLE_B, ROWS_B, WIDTH_B, ROWS_A, WIDTH_A = 0x10395C38, 0x10395C34, 0x10395C3C, 0x10395C40, 0x10395C44, 0x10395C48
GLOBALS = [ROOT, FOCUS, POINTER, PAUSE, MESSAGE_QUEUE, DRAW_QUEUE, TIMED, TIMERS, TIMER_COUNT, TIMER_LIST, TABLE_A, TABLE_B, ROWS_B,
           WIDTH_B, ROWS_A, WIDTH_A]
WINDOW_BYTES, QUEUE_BYTES, TIMED_BYTES, TIMER_BYTES, MESSAGE_BYTES, TIMED_MESSAGE_BYTES = 0x138, 0x14, 8, 0x20, 0x10, 0x14
WINDOW_LABEL, MESSAGE_LABEL, DRAW_LABEL, TIMED_LABEL, A_LABEL, B_LABEL = 10, 20, 22, 24, 30, 31
OUTSIDE = 3
MINUS = 0xFFFFFFFF

OP_HIDE, OP_DEALLOC, OP_QUEUE_DTOR, OP_QUEUE_DELETE, OP_TIMED_DTOR, OP_TIMED_DELETE, OP_CLOSE_TIMERS, OP_CLOSE_SYSTEM = range(1, 9)


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def rs(e, a, n):
    return list(struct.unpack(">" + "I" * n, e.read(a, 4 * n)))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    if hasattr(e, "l5_vtable"):
        return
    # The window's vtable (the Mac keeps the pointer at +0x118): slot 4 (+0x10) is the deleting destructor, a fake.
    dtor = T.fake_function(e, lambda em: em.l5.window_dtor(em))
    vtable = T.persist_alloc(e, 0x40)
    e.write(vtable + 0x10, struct.pack(">I", dtor))
    e.l5_vtable = vtable
    e.l5 = None

    def destroy_new_array(em, call):
        # The runtime's __destroy_new_array(first, destructor): the element size and count are the cookie before the array.
        first, dtor_vector = em.r(3), em.r(4)
        if first:
            size, count = struct.unpack(">II", em.read(first - 8, 8))
            for i in reversed(range(count)):
                call(dtor_vector, first + i * size, MINUS)
            em.l5.heap[3] += 1
        return None

    T.stub_cb(e, "__destroy_new_array", destroy_new_array)


def build_stubs():
    S = CL.Stubs()
    S.real = {HIDE_PAUSE, DEALLOCATE, CLOSE_SYSTEM, CLOSE_TIMERS, QUEUE_DTOR, TIMED_DTOR, TIMER_DTOR}
    S.internal(SEND_MESSAGE, lambda e: e.l5.send_message(e))
    S.internal(DECIDE_KB, lambda e: e.l5.decide_kb(e))
    S.internal(DRAW_REMOVE_ALL, lambda e: e.l5.draw_remove_all(e))
    S.internal(CURSOR_REMOVE_ALL, lambda e: e.l5.cursor_remove_all(e))
    S.imp("__dl__FPv", lambda e: e.l5.delete(e, 2))
    S.imp("__dla__FPv", lambda e: e.l5.delete(e, 3))
    return S


STUBS = build_stubs()


class World:
    """The case on the PowerPC side: objects at the Mac's sizes, the globals, the fakes' state."""

    def __init__(self, e, c):
        self.e, self.c = e, c
        self.L = CL.PpcLog({})
        self.heap = [0, 0, 0, 0]
        e.l5 = self
        # The windows.
        self.windows = [e.alloc(WINDOW_BYTES, b"\xa5" * WINDOW_BYTES) for _ in range(4)]
        for k, w in enumerate(self.windows):
            ws(e, w + 0x118, [e.l5_vtable])
            self.L.labels[w] = WINDOW_LABEL + k
        # The queues and the timed messages.
        self.queue_obj, self.queue_slots = [], []
        for key, label in (("mq", MESSAGE_LABEL), ("dq", DRAW_LABEL)):
            kind, n, count, head, tail, queued = c[key]
            obj = slots = 0
            if kind:
                obj = e.alloc(QUEUE_BYTES, b"\xa5" * QUEUE_BYTES)
                if kind == 1:
                    slots = e.alloc(max(n * MESSAGE_BYTES, 4), b"\xa5" * max(n * MESSAGE_BYTES, 4))
                    self.L.labels[slots] = label + 1
                ws(e, obj, [slots, count, head, tail, queued])
                self.L.labels[obj] = label
            self.queue_obj.append(obj)
            self.queue_slots.append(slots)
        kind, n, count = c["tm"]
        self.timed = self.timed_slots = 0
        if kind:
            self.timed = e.alloc(TIMED_BYTES, b"\xa5" * TIMED_BYTES)
            if kind == 1:
                self.timed_slots = e.alloc(max(n * TIMED_MESSAGE_BYTES, 4), b"\xa5" * max(n * TIMED_MESSAGE_BYTES, 4))
                self.L.labels[self.timed_slots] = TIMED_LABEL + 1
            ws(e, self.timed, [count, self.timed_slots])
            self.L.labels[self.timed] = TIMED_LABEL
        # The string heap's tables.
        self.table_a = e.alloc(c["heapA"], b"\xa5" * c["heapA"]) if c["heapA"] > 0 else 0
        self.table_b = e.alloc(c["heapB"], b"\xa5" * c["heapB"]) if c["heapB"] > 0 else 0
        if self.table_a:
            self.L.labels[self.table_a] = A_LABEL
        if self.table_b:
            self.L.labels[self.table_b] = B_LABEL
        # The timers: the array with its cookie (element size, count) as the Mac's array new leaves it, and three outside.
        n = c["timers"]
        self.n = n
        self.block = e.alloc(TIMER_BYTES * n + 8, b"\xa5" * (TIMER_BYTES * n + 8)) if n else 0
        self.table = self.block + 8 if n else 0
        if n:
            ws(e, self.block, [TIMER_BYTES, n])
        self.outside = [e.alloc(TIMER_BYTES, b"\xa5" * TIMER_BYTES) for _ in range(OUTSIDE)]
        for k, fields in enumerate(c["outside"]):
            self.write_timer(self.outside[k], fields)
        for k, fields in enumerate(c["table"]):
            self.write_timer(self.table + k * TIMER_BYTES, fields)
        # The globals.
        self.root, self.focus, self.pointer, self.pause = (self.window(c[k]) for k in ("root", "focus", "pointer", "pause"))
        words = {ROOT: self.root, FOCUS: self.focus, POINTER: self.pointer, PAUSE: self.pause, MESSAGE_QUEUE: self.queue_obj[0],
                 DRAW_QUEUE: self.queue_obj[1], TIMED: self.timed, TIMERS: self.table, TIMER_COUNT: c["count"],
                 TIMER_LIST: self.timer(c["head"]), TABLE_A: self.table_a, TABLE_B: self.table_b, ROWS_B: c["heapw"][0],
                 WIDTH_B: c["heapw"][1], ROWS_A: c["heapw"][2], WIDTH_A: c["heapw"][3]}
        for address, value in words.items():
            ws(e, address, [value])

    def window(self, ref):
        return self.windows[ref - 1] if 1 <= ref <= 4 else 0

    def timer(self, ref):
        if 1 <= ref <= self.n:
            return self.table + (ref - 1) * TIMER_BYTES
        if 101 <= ref < 101 + OUTSIDE:
            return self.outside[ref - 101]
        return 0

    def write_timer(self, address, fields):
        window, start, slice_, ident, running, free, nxt, prev = fields
        ws(self.e, address, [self.window(window), start, slice_, ident, running, free, self.timer(nxt), self.timer(prev)])

    def window_ref(self, address):
        return 0 if address == 0 else self.windows.index(address) + 1 if address in self.windows else -1

    def timer_ref(self, address):
        if address == 0:
            return 0
        if self.n and self.table <= address < self.table + self.n * TIMER_BYTES and (address - self.table) % TIMER_BYTES == 0:
            return (address - self.table) // TIMER_BYTES + 1
        if address in self.outside:
            return 101 + self.outside.index(address)
        return -1

    def queue_ref(self, address):
        if address == 0:
            return 0
        for k, obj in enumerate(self.queue_obj + [self.timed]):
            if address == obj:
                return k + 1
        return -1

    def alive(self, address):
        return int(address != 0 and self.L.lab(address) > 0)

    # -- the snapshot the events carry

    def snap(self):
        e = self.e
        g = lambda a: int(e.u32(a) != 0)
        return (g(ROOT), g(MESSAGE_QUEUE), g(DRAW_QUEUE), g(TIMED), g(FOCUS), g(POINTER), g(TIMER_LIST), g(PAUSE),
                self.heap[2], self.heap[3])

    # -- the fakes

    def send_message(self, e):
        self.L.rec("UI_SendMessage", (self.L.lab(e.r(3)), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), int(e.u32(PAUSE) != 0)))

    def decide_kb(self, e):
        self.L.rec("UIMain_DecideKbCapture", (int(e.u32(PAUSE) != 0),))

    def draw_remove_all(self, e):
        self.L.rec("DrawObjectLibrary.RemoveAll", (T.s16(e.r(3)),) + self.snap())

    def cursor_remove_all(self, e):
        self.L.rec("CursorLibrary.RemoveAll", self.snap())

    def window_dtor(self, e):
        this = e.r(3)
        self.L.rec("Window.dtor", (self.L.lab(this),) + self.snap())
        if T.s16(e.r(4)) > 0:
            self.heap[2] += 1
            self.L.rec("delete", (self.L.lab(this),))
            self.L.forget(this)

    def delete(self, e, kind):
        block = e.r(3)
        if block == 0:
            return
        self.heap[kind] += 1
        if self.L.lab(block) > 0:
            self.L.rec("delete", (self.L.lab(block),))
            self.L.forget(block)

    # -- the dump (the same words as tpw_l5_run puts out)

    def dump(self, result):
        e = self.e
        op = self.c["op"]
        rd = lambda a: e.u32(a)
        out = [result, self.window_ref(rd(ROOT)), self.window_ref(rd(FOCUS)), self.window_ref(rd(POINTER)), self.window_ref(rd(PAUSE)),
               self.queue_ref(rd(MESSAGE_QUEUE)), self.queue_ref(rd(DRAW_QUEUE)), self.queue_ref(rd(TIMED))]
        out.append(0 if rd(TIMERS) == 0 else 1 if rd(TIMERS) == self.table else -1)
        out.append(T.s32(rd(TIMER_COUNT)))
        out.append(self.timer_ref(rd(TIMER_LIST)))
        out.append(0 if rd(TABLE_A) == 0 else 1 if rd(TABLE_A) == self.table_a else -1)
        out.append(0 if rd(TABLE_B) == 0 else 1 if rd(TABLE_B) == self.table_b else -1)
        out += [T.s32(rd(a)) for a in (ROWS_B, WIDTH_B, ROWS_A, WIDTH_A)]
        out += [self.alive(w) for w in self.windows]
        for k in range(2):
            obj, slots = self.queue_obj[k], self.queue_slots[k]
            alive = self.alive(obj)
            out.append(alive)
            if alive:
                entries, count, head, tail, queued = (T.s32(v) if i else v for i, v in enumerate(rs(e, obj, 5)))
                out += [0 if entries == 0 else 1 if entries == slots else -1, count, head, tail, queued]
            else:
                out += [0] * 5
        alive = self.alive(self.timed)
        out.append(alive)
        if alive:
            count, entries = rs(e, self.timed, 2)
            out += [T.s32(count), 0 if entries == 0 else 1 if entries == self.timed_slots else -1]
        else:
            out += [0, 0]
        out += [self.alive(a) for a in (self.queue_slots[0], self.queue_slots[1], self.timed_slots, self.table_a, self.table_b)]
        timers = list(self.outside)
        if not (op == OP_CLOSE_TIMERS or (op == OP_CLOSE_SYSTEM and self.c["root"] != 0)) and self.n:
            timers += [self.table + k * TIMER_BYTES for k in range(self.n)]
        for t in timers:
            window, start, slice_, ident, running, free, nxt, prev = rs(e, t, 8)
            out += [self.window_ref(window), start, T.s32(slice_), T.s32(ident), T.s32(running), T.s32(free), self.timer_ref(nxt),
                    self.timer_ref(prev)]
        out += [self.heap[2], self.heap[3]]
        return out


def flat(c):
    d = [c["op"], c["target"], c["root"], c["focus"], c["pointer"], c["pause"]] + list(c["mq"]) + list(c["dq"]) + list(c["tm"])
    d += [c["heapA"], c["heapB"]] + list(c["heapw"]) + [c["timers"], c["count"], c["head"]]
    for fields in c["outside"] + c["table"]:
        d += list(fields)
    return d


def run_ppc(e, case):
    w = World(e, case)
    saved = STUBS.activate(e)
    c = case
    try:
        op = c["op"]
        r3 = 0
        if op == OP_HIDE:
            T.call_ppc(e, HIDE_PAUSE, [])
        elif op == OP_DEALLOC:
            T.call_ppc(e, DEALLOCATE, [])
        elif op in (OP_QUEUE_DTOR, OP_QUEUE_DELETE):
            T.call_ppc(e, QUEUE_DTOR, [w.queue_obj[c["target"]], 1 if op == OP_QUEUE_DELETE else 0])
        elif op in (OP_TIMED_DTOR, OP_TIMED_DELETE):
            T.call_ppc(e, TIMED_DTOR, [w.timed, 1 if op == OP_TIMED_DELETE else 0])
        elif op == OP_CLOSE_TIMERS:
            T.call_ppc(e, CLOSE_TIMERS, [])
        else:
            r3 = T.call_ppc(e, CLOSE_SYSTEM, [])[0]
        result = r3 & MASK
    finally:
        CL.Stubs.deactivate(e, saved)
        e.l5 = None
    return dict(state=w.dump(result), log=w.L.log)


def run_host(lib, case):
    lib.tpw_l5_run.restype = C.c_int32
    lib.tpw_l5_run.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    description = flat(case)
    values = (C.c_int64 * len(description))(*description)
    out = (C.c_int64 * 512)()
    CL.host_begin(lib, {})
    n = lib.tpw_l5_run(values, out)
    log = CL.host_end(lib)
    # The dump's words as the PowerPC side forms them (a 32-bit global as unsigned where it is a plain word).
    state = [int(out[i]) for i in range(n)]
    state[0] &= MASK
    return dict(state=state, log=log)


# ----------------------------------------------------------------- the cases


def blank_case(op, **kw):
    c = dict(op=op, target=0, root=0, focus=0, pointer=0, pause=0, mq=(0, 0, 0, 0, 0, 0), dq=(0, 0, 0, 0, 0, 0), tm=(0, 0, 0),
             heapA=0, heapB=0, heapw=(0, 0, 0, 0), timers=0, count=0, head=0, outside=[(0, 0, 0, 0, 0, 1, 0, 0)] * OUTSIDE, table=[])
    c.update(kw)
    return c


def rand_word(r):
    return r.choice([0, 1, 2, 5, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32)]) & MASK


def rand_queue(r, kind=None):
    kind = r.choice([0, 1, 1, 2]) if kind is None else kind
    n = r.choice([1, 2, 5, 8, 16, 64])
    return (kind, n, r.randint(-3, 100), r.randint(-3, 100), r.randint(-3, 100), r.randint(-3, 100))


def rand_timer(r, n, window_chance=0.7):
    refs = [0] + list(range(1, n + 1)) + [101 + k for k in range(OUTSIDE)]
    return (r.randint(1, 4) if r.random() < window_chance else 0, rand_word(r), r.randint(-5, 1000), r.randint(-5, 100),
            r.choice([0, 1, 2]), r.choice([0, 1, 1]), r.choice(refs), r.choice(refs))


def rand_timers(r, c, n=None):
    n = r.choice([0, 1, 2, 3, 5, 8, 16, 0x100 // 8]) if n is None else n
    c["timers"] = n
    c["count"] = r.choice([n, n, 0x100, 0, -1, rand_word(r) & 0xFFFF])
    c["table"] = [rand_timer(r, n) for _ in range(n)]
    c["outside"] = [rand_timer(r, n) for _ in range(OUTSIDE)]
    refs = [0] + list(range(1, n + 1)) + [101 + k for k in range(OUTSIDE)]
    c["head"] = r.choice(refs)


def chain(r, c):
    """A consistent list through some of the timers (outside ones included), so unlinking changes neighbours."""
    n = c["timers"]
    members = [k + 1 for k in range(n) if r.random() < 0.6] + [101 + k for k in range(OUTSIDE) if r.random() < 0.6]
    r.shuffle(members)
    fields = {}
    for i, ref in enumerate(members):
        fields[ref] = (members[i + 1] if i + 1 < len(members) else 0, members[i - 1] if i else 0)

    def patch(ref, timer):
        nxt, prev = fields[ref]
        return timer[:6] + (nxt, prev)
    c["table"] = [patch(k + 1, t) if k + 1 in fields else t for k, t in enumerate(c["table"])]
    c["outside"] = [patch(101 + k, t) if 101 + k in fields else t for k, t in enumerate(c["outside"])]
    c["head"] = members[0] if members else 0


def noise(r, c):
    """The parts of the world an operation does not use, random."""
    c["root"] = r.choice([0, 1, 1, 2, 3])
    c["focus"] = r.choice([0, 1, 2, 3, 4])
    c["pointer"] = r.choice([0, 1, 2, 3, 4])
    c["pause"] = r.choice([0, 0, 3, 4])
    c["mq"], c["dq"] = rand_queue(r), rand_queue(r)
    c["tm"] = (r.choice([0, 1, 2]), r.choice([1, 3, 8, 64]), r.randint(-3, 100))
    c["heapA"] = r.choice([0, 4, 32, 400])
    c["heapB"] = r.choice([0, 4, 32, 400])
    c["heapw"] = tuple(rand_word(r) for _ in range(4))
    rand_timers(r, c)
    if r.random() < 0.5:
        chain(r, c)


def pause_case(r, pause=None):
    c = blank_case(OP_HIDE)
    noise(r, c)
    c["pause"] = r.choice([0, 3, 4, 1, 2]) if pause is None else pause
    return c


def edge_pause():
    r = random.Random(641)
    for pause in (0, 1, 2, 3, 4):
        for _ in range(3):
            yield pause_case(r, pause)
    # The root as the pause window; the pause window with a queue and a table around.
    c = pause_case(r, 1)
    c["root"] = 1
    yield c


def random_pause(r):
    return pause_case(r)


def dealloc_case(r, a=None, b=None):
    c = blank_case(OP_DEALLOC)
    noise(r, c)
    c["heapA"] = r.choice([0, 1, 4, 32, 400]) if a is None else a
    c["heapB"] = r.choice([0, 1, 4, 32, 400]) if b is None else b
    return c


def edge_dealloc():
    r = random.Random(642)
    for a in (0, 1, 64):
        for b in (0, 1, 64):
            yield dealloc_case(r, a, b)


def random_dealloc(r):
    return dealloc_case(r)


def queue_case(r, op, target=None, kind=None):
    c = blank_case(op)
    noise(r, c)
    c["target"] = r.randint(0, 1) if target is None else target
    key = "mq" if c["target"] == 0 else "dq"
    c[key] = rand_queue(r, kind if kind is not None else r.choice([1, 1, 2]))
    return c


def edge_queue_dtor():
    r = random.Random(643)
    for op in (OP_QUEUE_DTOR, OP_QUEUE_DELETE):
        for target in (0, 1):
            for kind in (1, 2):
                yield queue_case(r, op, target, kind)
    # Slots in a queue with an empty count, a count of zero slots in the object, the other queue present too.
    c = queue_case(r, OP_QUEUE_DTOR, 0, 1)
    c["mq"] = (1, 1, 0, 0, 0, 0)
    c["dq"] = (1, 4, 3, 2, 1, 0)
    yield c


def random_queue_dtor(r):
    return queue_case(r, r.choice([OP_QUEUE_DTOR, OP_QUEUE_DELETE]))


def timed_case(r, op, kind=None):
    c = blank_case(op)
    noise(r, c)
    c["tm"] = (r.choice([1, 1, 2]) if kind is None else kind, r.choice([1, 3, 8, 64]), r.randint(-3, 100))
    return c


def edge_timed_dtor():
    r = random.Random(644)
    for op in (OP_TIMED_DTOR, OP_TIMED_DELETE):
        for kind in (1, 2):
            for _ in range(2):
                yield timed_case(r, op, kind)


def random_timed_dtor(r):
    return timed_case(r, r.choice([OP_TIMED_DTOR, OP_TIMED_DELETE]))


def timers_case(r, op=OP_CLOSE_TIMERS, n=None, linked=None):
    c = blank_case(op)
    noise(r, c)
    rand_timers(r, c, n)
    if linked if linked is not None else r.random() < 0.5:
        chain(r, c)
    return c


def edge_close_timers():
    r = random.Random(645)
    for n in (0, 1, 2, 3, 5, 16, 32):
        for linked in (False, True):
            yield timers_case(r, n=n, linked=linked)
    # A list through the table and outside timers in an order the destructors' order shows in.
    for _ in range(12):
        c = timers_case(r, n=r.choice([3, 4, 6]), linked=True)
        yield c
    # Links that point at each other and at themselves.
    c = timers_case(r, n=3, linked=False)
    c["table"] = [(0, 0, 0, 0, 0, 0, 1, 1), (0, 0, 0, 0, 0, 0, 3, 3), (0, 0, 0, 0, 0, 0, 2, 101)]
    c["outside"] = [(0, 0, 0, 0, 0, 0, 1, 2), (0, 0, 0, 0, 0, 0, 0, 0), (0, 0, 0, 0, 0, 0, 0, 0)]
    c["head"] = 1
    yield c


def random_close_timers(r):
    return timers_case(r)


def system_case(r, root=None, queues=None, timers=None):
    c = blank_case(OP_CLOSE_SYSTEM)
    noise(r, c)
    if root is not None:
        c["root"] = root
    if queues is not None:
        c["mq"], c["dq"], c["tm"] = queues
    if timers is not None:
        rand_timers(r, c, timers)
        chain(r, c)
    return c


def edge_close_system():
    r = random.Random(646)
    # No root window: nothing at all is done (whatever else is set).
    for _ in range(4):
        yield system_case(r, root=0)
    # Each combination of the three queues being set or not (the others random), with the root set.
    for bits in range(8):
        mq = (1, 4, 3, 2, 1, 0) if bits & 1 else (0, 0, 0, 0, 0, 0)
        dq = (1, 8, 5, 4, 3, 0) if bits & 2 else (0, 0, 0, 0, 0, 0)
        tm = (1, 3, 7) if bits & 4 else (0, 0, 0)
        yield system_case(r, root=1, queues=(mq, dq, tm), timers=r.choice([0, 3]))
    # Queues without slots; the root as the focus and the pointer window; the table absent.
    yield system_case(r, root=1, queues=((2, 0, 1, 2, 3, 4), (2, 0, 5, 6, 7, 8), (2, 0, 9)), timers=0)
    c = system_case(r, root=2, queues=((1, 4, 3, 2, 1, 0), (1, 4, 3, 2, 1, 0), (1, 4, 3)), timers=5)
    c["focus"], c["pointer"], c["pause"] = 2, 2, 2
    yield c
    for _ in range(20):
        yield system_case(r, root=r.choice([1, 2, 3, 4]))


def random_close_system(r):
    return system_case(r, root=r.choice([0, 1, 1, 1, 2, 3, 4]))


TESTS = [
    FnTest("HidePauseWindow", run_ppc, run_host, edge=edge_pause, rnd=random_pause),
    FnTest("InterfaceStringHeap::DeAllocate", run_ppc, run_host, edge=edge_dealloc, rnd=random_dealloc),
    FnTest("InterfaceMessageQueue::~InterfaceMessageQueue", run_ppc, run_host, edge=edge_queue_dtor, rnd=random_queue_dtor),
    FnTest("InterfaceTimedMessages::~InterfaceTimedMessages", run_ppc, run_host, edge=edge_timed_dtor, rnd=random_timed_dtor),
    FnTest("UI_CloseTimerSystem", run_ppc, run_host, edge=edge_close_timers, rnd=random_close_timers),
    FnTest("UI_CloseSystem", run_ppc, run_host, edge=edge_close_system, rnd=random_close_system),
]
