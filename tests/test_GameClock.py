"""Differential test for src/game/FixableTime.cpp, src/game/PausableTime.cpp, src/game/VariableTime.cpp, src/game/save/SaveLoad.cpp, src/game/ui/UI_Base.cpp: the game clock's 28 functions (CFixedTime,
CVariableTime, CPausableTime, CPausableFixedTime, CFixableTime, CSaveableTime and
CSaveableFixedTime, and the user interface's timeGetTimeClockTickFunction), against
libtpw_unit_GameClock.dylib. CClock's ten methods (src/game/park/Clock.cpp) run on top of them as a
chain test, with every time function real on both sides.

A case is an object (a CSaveableTime of 0x44 bytes, a CSaveableFixedTime of 0x20, or a CClock
of 0x64), the state its fields start in, and a script of operations: a few random ones, then
the function under test once to three times. The only things faked, on both sides and with the
same scripted answers, are the bullfrog clock (a queue of 32-bit readings: steady steps,
jumps of 0x7fffffff and 0x80000000, wrap-around through 0xffffffff, readings that go
backwards), the file calls (the count of bytes moved, what a read stores), the debug print and
the platform timer. After every operation: its answer and EVERY field of the object (the
unaligned double at +4 and the doubles at +0x10 and +0x18 as 64-bit values, NaNs as one
value because a fused multiply-add can make one), and at the end the calls recorded, in order.

The Mac swaps a saved word in place around the file call (the file is little-endian) and
this host does not need to, so a Load that fails after the file call has stored all four bytes
leaves those bytes in the field unswapped on both sides. The test compares those four bytes
and then puts the field back, because the number they make differs between a big and a
little-endian memory. A Load that succeeds always has stored its four bytes (a fake that
reports four bytes it did not store would be swapped on the Mac and not here).
"""
import ctypes as C
import math
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_GameClock"

MASK = 0xFFFFFFFF
TIME_FIELDS = [
    ("fixedLast", 0x00, "I"), ("fixedMs", 0x04, "d"), ("last", 0x0C, "I"), ("ms", 0x10, "d"), ("rate", 0x18, "d"),
    ("pausedAt", 0x20, "i"), ("pausedDuration", 0x24, "i"), ("pauseFlag", 0x28, "i"), ("fixedFlag", 0x2C, "i"),
    ("fixedAt", 0x30, "i"), ("fixedStep", 0x34, "I"), ("fixedAdjustment", 0x38, "i"), ("adjustment", 0x3C, "i"),
    ("savedTime", 0x40, "I"),
]
FIXED_FIELDS = [
    ("last", 0x00, "I"), ("ms", 0x04, "d"), ("pausedAt", 0x0C, "i"), ("pausedDuration", 0x10, "i"),
    ("pauseFlag", 0x14, "i"), ("adjustment", 0x18, "i"), ("savedTime", 0x1C, "I"),
]
CLOCK_FIELDS = ([("t." + n, o, f) for n, o, f in TIME_FIELDS] +
                [("f." + n, o + 0x44, f) for n, o, f in FIXED_FIELDS])
LAYOUTS = {"time": TIME_FIELDS, "fixed": FIXED_FIELDS, "clock": CLOCK_FIELDS, "none": []}
SIZES = {"time": 0x44, "fixed": 0x20, "clock": 0x64, "none": 8}
# Labels the call log gives the object (1), a file handle (3) and the saved-time words (2: the
# time's, 4: the fixed time's), by the byte offset of the word in the object.
SAVED_LABELS = {"time": {2: 0x40}, "fixed": {4: 0x1C}, "clock": {2: 0x40, 4: 0x60}, "none": {}}
# The fixed time sits at +0x44 in a CClock.
FIXED_IN_CLOCK = 0x44

# name: (address, object kind, argument, answer). The argument is "step" (UseFixedTime), "rate"
# (SetTimeRate) or "file"; the answer "u32" (r3), "f64" (f1) or None.
OPS = {
    "CFixedTime::ReadFixedTime": (0x10147C18, "fixed", None, "u32"),
    "CVariableTime::SetTimeRate": (0x101688D4, "time", "rate", None),
    "CVariableTime::IncreaseTimeRate": (0x10168910, "time", None, None),
    "CVariableTime::DecreaseTimeRate": (0x10168988, "time", None, None),
    "CVariableTime::GetTimeRate": (0x10168A00, "time", None, "f64"),
    "CVariableTime::ReadVariableTime": (0x10168A3C, "time", None, "u32"),
    "CPausableTime::Pause": (0x10154268, "time", None, None),
    "CPausableTime::UnPause": (0x101542E0, "time", None, None),
    "CPausableTime::TogglePause": (0x1015436C, "time", None, None),
    "CPausableTime::GetPauseFlag": (0x10154420, "time", None, "u32"),
    "CPausableTime::ReadPausableTime": (0x1015445C, "time", None, "u32"),
    "CPausableFixedTime::Pause": (0x1015400C, "fixed", None, None),
    "CPausableFixedTime::UnPause": (0x10154088, "fixed", None, None),
    "CPausableFixedTime::TogglePause": (0x10154118, "fixed", None, None),
    "CPausableFixedTime::ReadPausableFixedTime": (0x101541D0, "fixed", None, "u32"),
    "CFixableTime::UseFixedTime": (0x101479C0, "time", "step", None),
    "CFixableTime::CancelFixedTime": (0x10147A58, "time", None, None),
    "CFixableTime::NextFrame": (0x10147AE0, "time", None, None),
    "CFixableTime::ReadFixableTime": (0x10147B58, "time", None, "u32"),
    "CSaveableTime::Save": (0x1015763C, "time", "file", "u32"),
    "CSaveableTime::Load": (0x101576EC, "time", "file", "u32"),
    "CSaveableTime::ReadSaveableTime": (0x1015778C, "time", None, "u32"),
    "CSaveableTime::AdjustTime": (0x101577FC, "time", None, None),
    "CSaveableFixedTime::Save": (0x101573F8, "fixed", "file", "u32"),
    "CSaveableFixedTime::Load": (0x101574AC, "fixed", "file", "u32"),
    "CSaveableFixedTime::ReadSaveableFixedTime": (0x10157550, "fixed", None, "u32"),
    "CSaveableFixedTime::AdjustTime": (0x101575C8, "fixed", None, None),
    "timeGetTimeClockTickFunction::tickyticky": (0x101CAB58, "none", None, "u32"),
    "CClock::Read": (0x101472F4, "clock", None, "u32"),
    "CClock::ReadFixed": (0x1014733C, "clock", None, "u32"),
    "CClock::Pause": (0x1014738C, "clock", None, None),
    "CClock::UnPause": (0x101473EC, "clock", None, None),
    "CClock::TogglePause": (0x10147450, "clock", None, None),
    "CClock::GetPauseFlag": (0x101474B8, "clock", None, "u32"),
    "CClock::Save": (0x10147508, "clock", "file", "u32"),
    "CClock::Load": (0x10147588, "clock", "file", "u32"),
    "CClock::PreSave": (0x10147608, "clock", None, None),
    "CClock::PostLoad": (0x10147678, "clock", None, None),
}
# The constructors are real too (CClock's inline construction is not run).
REAL_ADDRESSES = {entry[0] for entry in OPS.values()} | {0x10147BDC, 0x1016886C}
UDEBUG = 0x10000E40
# The direct Loads: the saved word they may leave unswapped.
DIRECT_LOADS = ("CSaveableTime::Load", "CSaveableFixedTime::Load")


def s64(value):
    """An unsigned 64-bit number as the signed one the call log's queues hold."""
    return value - (1 << 64) if value >= 1 << 63 else value


# ------------------------------------------------------------------ objects on both sides


class Memory:
    """The bytes of one test object on one side: big-endian words on the PowerPC side,
    little-endian on the host."""

    def __init__(self, big, kind, read, write):
        self.endian = ">" if big else "<"
        self.fields = LAYOUTS[kind]
        self.read = read
        self.write = write

    def put(self, state):
        for name, offset, fmt in self.fields:
            self.write(offset, struct.pack(self.endian + fmt, state[name]))

    def put_field(self, name, value):
        for field, offset, fmt in self.fields:
            if field == name:
                self.write(offset, struct.pack(self.endian + fmt, value))

    def get(self):
        out = {}
        for name, offset, fmt in self.fields:
            size = 8 if fmt == "d" else 4
            out[name] = struct.unpack(self.endian + fmt, self.read(offset, size))[0]
        return out

    def raw(self, name):
        for field, offset, fmt in self.fields:
            if field == name:
                return self.read(offset, 4)
        raise KeyError(name)


def object_offset(case_kind, name):
    """Where the object of `name` is in a case of this kind: the fixed time's functions run
    on the fixed time inside a CClock."""
    if case_kind == "clock" and OPS[name][1] == "fixed":
        return FIXED_IN_CLOCK
    return 0


# ------------------------------------------------------------------ the PowerPC side


def build_stubs():
    """The callees, as fakes that record what the host fakes record."""
    stubs = CL.Stubs()
    stubs.real.update(REAL_ADDRESSES)

    # The bullfrog clock: the next reading of the queue.
    def clock(e):
        reading = e.cl.ret("LbTime_GetClock") & MASK
        e.cl.rec("LbTime_GetClock", (reading,))
        e.set_r(3, reading)

    # The platform timer: 64-bit microseconds through a hidden buffer.
    def timer(e):
        value = e.cl.ret("GetAbsolute") & ((1 << 64) - 1)
        e.cl.rec("GetAbsolute", (s64(value),))
        e.write(e.r(3), struct.pack(">Q", value))

    # A file write: records the file, the buffer's label, the length and the four bytes.
    def write(e):
        log = e.cl
        result, file, buffer, length, transferred = (e.r(i) for i in range(3, 8))
        word = int.from_bytes(e.read(buffer, 4), "little")
        log.rec("LbFile_Write", (log.lab(file), log.lab(buffer), length, word))
        e.write(transferred, struct.pack(">I", log.ret("LbFile_Write:count") & MASK))
        e.write(result, struct.pack(">I", log.ret("LbFile_Write:result") & MASK))

    # A file read: stores the first bytes of the queued word, reports the queued count.
    def read(e):
        log = e.cl
        result, file, buffer, length, transferred = (e.r(i) for i in range(3, 8))
        log.rec("LbFile_Read", (log.lab(file), log.lab(buffer), length))
        word = log.ret("LbFile_Read:data") & MASK
        written = log.ret("LbFile_Read:written") & MASK
        e.write(buffer, word.to_bytes(4, "little")[:min(written, 4)])
        e.write(transferred, struct.pack(">I", log.ret("LbFile_Read:count") & MASK))
        e.write(result, struct.pack(">I", log.ret("LbFile_Read:result") & MASK))

    # The debug print: its text.
    def debug(e):
        e.cl.rec("UDebug", (), (e.cstr(e.r(3))[:159],))

    stubs.imp("LbTime_GetClock__Fv", clock)
    stubs.imp("GetAbsolute__Q213SamsUtilities6UTimerFv", timer)
    stubs.imp("LbFile_Write__FPvPCvUlPUl", write)
    stubs.imp("LbFile_Read__FPvPvUlPUl", read)
    stubs.internal(UDEBUG, debug)
    return stubs


def ppc(e, case):
    kind = case["kind"]
    size = SIZES[kind] + 8
    obj = e.alloc(size, b"\xa5" * size) + case["skew"]
    file = e.alloc(16, b"\xa5" * 16)
    fixed = {obj: 1, file: 3}
    for label, offset in SAVED_LABELS[kind].items():
        fixed[obj + offset] = label
    e.cl = CL.PpcLog(fixed, case["queues"])
    memory = Memory(True, kind, lambda o, n: e.read(obj + o, n), lambda o, d: e.write(obj + o, d))
    memory.put(case["state"])

    def invoke(name, argument):
        address, _, argkind, answer = OPS[name]
        args = [obj + object_offset(kind, name)]
        if argkind == "step":
            args.append(argument & MASK)
        elif argkind == "rate":
            args.append(T.D(argument))
        elif argkind == "file":
            args.append(0 if case["null_file"] else file)
        r3, _, f1 = T.call_ppc(e, address, args)
        if answer == "u32":
            return r3 & MASK
        if answer == "f64":
            return f1
        return None

    stubs = build_stubs()
    saved = stubs.activate(e)
    try:
        answers, states = run_script(case, memory, invoke, lambda index: e.cl.rec("op", (index,)))
    finally:
        CL.Stubs.deactivate(e, saved)
    return dict(answers=answers, states=states, log=e.cl.log)


# ------------------------------------------------------------------ the host side


def host(lib, case):
    kind = case["kind"]
    block = C.create_string_buffer(SIZES[kind] + 16)
    start = C.addressof(block)
    start += (-start) % 8 + case["skew"]
    file = C.create_string_buffer(16)
    fixed = {1: start, 3: C.addressof(file)}
    for label, offset in SAVED_LABELS[kind].items():
        fixed[label] = start + offset
    CL.host_begin(lib, fixed, case["queues"])
    memory = Memory(False, kind, lambda o, n: C.string_at(start + o, n), lambda o, d: C.memmove(start + o, d, len(d)))
    memory.put(case["state"])
    run = lib.tpw_gc_run
    run.restype = C.c_uint32
    run.argtypes = [C.c_char_p, C.c_void_p, C.c_uint32, C.c_double, C.c_void_p, C.POINTER(C.c_double)]
    mark = lib.tpw_gc_mark
    mark.argtypes = [C.c_int32]

    def invoke(name, argument):
        _, _, argkind, answer = OPS[name]
        rate_out = C.c_double(0.0)
        step = argument & MASK if argkind == "step" else 0
        rate = argument if argkind == "rate" else 0.0
        file_arg = None if case["null_file"] or argkind != "file" else C.addressof(file)
        r3 = run(name.encode(), start + object_offset(kind, name), step, rate, file_arg, C.byref(rate_out))
        if answer == "u32":
            return r3
        if answer == "f64":
            return rate_out.value
        return None

    answers, states = run_script(case, memory, invoke, mark)
    return dict(answers=answers, states=states, log=CL.host_end(lib))


# ------------------------------------------------------------------ the script, shared


def run_script(case, memory, invoke, mark):
    """Runs the operations in order. After each: the answer and every field."""
    answers, states = [], []
    loads = 0
    for index, (name, argument) in enumerate(case["script"]):
        mark(index)
        before = memory.get()
        answer = invoke(name, argument)
        state = memory.get()
        if name in DIRECT_LOADS:
            written = case["queues"]["LbFile_Read:written"][loads]
            loads += 1
            if answer == 0 and written == 4:
                # The file call stored all four bytes and reported a failure: the bytes stay as
                # they came on both sides (see the module comment). Compare them, then put the
                # number back.
                state["savedTime"] = memory.raw("savedTime")
                memory.put_field("savedTime", before["savedTime"])
        answers.append(answer)
        states.append(state)
    return answers, states


# ------------------------------------------------------------------ generators

DOUBLES = [0.0, -0.0, 1.0, -1.0, 0.5, 2.0, 0.25, 1.25, 3.0, 1e-9, 1e12, 4294967295.0, 4294967295.5,
           4294967296.0, 2147483647.5, 2147483648.0, 2147483647.0, 2147483648.5, -5.5, math.inf, -math.inf,
           math.nan, 1e300, 5e-324]
# Rates around the limits of the step functions (0.25, 2.0, and 1.25 times or over 1.25 of them).
RATES = [1.0, 0.5, 2.0, 0.0, 0.25, 0.2, 0.19999999999999998, 0.2000000000000001, 1.6, 1.5999999999999999,
         1.6000000000000003, 1.7, 0.3, 0.3125, 0.31, 0.32, 2.5, 2.6, -1.0, 3.0, 1e-9, 1e300, math.inf,
         -math.inf, math.nan, -0.0]
U32_EDGES = [0, 1, 2, 31, 1000, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFE, MASK]
# Differences between two readings of the clock.
DELTAS = [0, 1, 16, 17, 33, 50, 1000, 0x7FFFFFFF, 0x80000000, MASK, MASK - 4999, MASK - 15, 0x10000]
STEPS = [0x20, 0, 1, 2, 3, 7, 30, 60, 999, 1000, 1001, 0x80000000, MASK]
FAILING_COUNTS = [0, 1, 2, 3, 8, 9, 0x100, 0x40000004, MASK - 3, MASK]


def pick_u32(r):
    """A 32-bit word: random, an edge value, or small."""
    k = r.random()
    if k < 0.4:
        return r.getrandbits(32)
    if k < 0.8:
        return r.choice(U32_EDGES)
    return r.randrange(2000)


def pick_i32(r):
    """The same as a signed number."""
    return T.s32(pick_u32(r))


def pick_flag(r):
    """A pause or fixed flag: zero, one, or another nonzero value (the code only tests for zero)."""
    return r.choice([0, 0, 1, 1, 2, -1, 0x10000, -0x80000000])


def pick_double(r):
    """The milliseconds so far: an edge of the unsigned conversion, a usual size, negative, or any bits."""
    k = r.random()
    if k < 0.45:
        return r.choice(DOUBLES)
    if k < 0.75:
        return r.uniform(0, 1e10)
    if k < 0.85:
        return -r.uniform(0, 1e5)
    return struct.unpack("<d", struct.pack("<Q", r.getrandbits(64)))[0]


def pick_rate(r):
    """A rate: one of the interesting ones, or any between 0 and 3."""
    if r.random() < 0.7:
        return r.choice(RATES)
    return r.uniform(0, 3.0)


def pick_step(r):
    """A frames-a-second figure for UseFixedTime."""
    if r.random() < 0.6:
        return r.choice(STEPS)
    return r.randint(1, 5000)


def series(r, count):
    """Readings of the clock: steady steps with jumps, wraps and steps back."""
    out = [r.choice([0, 1000, 0xFFFFFFF0, 0x7FFFFF00, MASK, 0x80000000, r.getrandbits(32)])]
    steady = r.choice([1, 16, 33, 100, 1000])
    for _ in range(count - 1):
        k = r.random()
        delta = steady if k < 0.5 else r.choice(DELTAS) if k < 0.8 else r.getrandbits(32)
        out.append((out[-1] + delta) & MASK)
    return out


def near(r, reading):
    """A previous reading of the clock the way the first one meets it."""
    k = r.random()
    if k < 0.6:
        return (reading - r.choice(DELTAS)) & MASK
    return pick_u32(r)


def time_state(r, first):
    """A random CSaveableTime."""
    return dict(fixedLast=pick_u32(r), fixedMs=pick_double(r), last=near(r, first), ms=pick_double(r),
                rate=pick_rate(r), pausedAt=pick_i32(r), pausedDuration=pick_i32(r), pauseFlag=pick_flag(r),
                fixedFlag=pick_flag(r), fixedAt=pick_i32(r), fixedStep=pick_u32(r), fixedAdjustment=pick_i32(r),
                adjustment=pick_i32(r), savedTime=pick_u32(r))


def fixed_state(r, first):
    """A random CSaveableFixedTime."""
    return dict(last=near(r, first), ms=pick_double(r), pausedAt=pick_i32(r), pausedDuration=pick_i32(r),
                pauseFlag=pick_flag(r), adjustment=pick_i32(r), savedTime=pick_u32(r))


def new_state(r, kind, first):
    """A random state of the object kind; `first` is the first reading of the clock."""
    if kind == "time":
        return time_state(r, first)
    if kind == "fixed":
        return fixed_state(r, first)
    if kind == "clock":
        state = {"t." + k: v for k, v in time_state(r, first).items()}
        state.update({"f." + k: v for k, v in fixed_state(r, first).items()})
        return state
    return {}


def boring_state(kind):
    """What gClock holds after construction: zeros, and the rate 1.0 (the saved words are
    zero too, they are static storage)."""
    if kind == "clock":
        state = {"t." + k: v for k, v in boring_state("time").items()}
        state.update({"f." + k: v for k, v in boring_state("fixed").items()})
        return state
    state = {name: (0.0 if fmt == "d" else 0) for name, _, fmt in LAYOUTS[kind]}
    if kind == "time":
        state["rate"] = 1.0
    return state


def argument_for(r, name):
    """The argument the operation takes (a step, a rate), if any."""
    kind = OPS[name][2]
    if kind == "step":
        return pick_step(r)
    if kind == "rate":
        return pick_rate(r)
    return None


def file_queues(r, script, kind):
    """What the file calls answer, one set of entries for each Save or Load (two for a
    CClock's, which saves the time and then the fixed time)."""
    queues = {}

    def add(key, value):
        queues.setdefault(key, []).append(value)

    for name, _ in script:
        if OPS[name][2] != "file":
            continue
        for _ in range(2 if kind == "clock" else 1):
            success = r.random() < 0.5
            count = r.choice([4, 5, 6, 7]) if success else r.choice(FAILING_COUNTS)
            if name.endswith("Save"):
                add("LbFile_Write:count", count)
                add("LbFile_Write:result", r.choice([0, 1, -1, 5]))
            else:
                direct = name in DIRECT_LOADS
                add("LbFile_Read:count", count)
                add("LbFile_Read:result", r.choice([0, 1, -1, 5]))
                add("LbFile_Read:data", r.getrandbits(32))
                add("LbFile_Read:written", 4 if success or (direct and r.random() < 0.5) else 0)
    return queues


def build(r, kind, script, state=None, readings=None):
    """A whole case: the state, the script, and the answers of the clock and the file calls."""
    readings = readings or series(r, 48)
    queues = {"LbTime_GetClock": readings}
    queues.update(file_queues(r, script, kind))
    return dict(kind=kind, state=state if state is not None else new_state(r, kind, readings[0]), script=script,
                queues=queues, null_file=r.random() < 0.1, skew=r.choice([0, 4]))


# What a random script may do before the function under test.
PREFIX = {
    "time": ["CVariableTime::SetTimeRate", "CVariableTime::IncreaseTimeRate", "CVariableTime::DecreaseTimeRate",
             "CVariableTime::ReadVariableTime", "CPausableTime::Pause", "CPausableTime::UnPause",
             "CPausableTime::TogglePause", "CPausableTime::ReadPausableTime", "CFixableTime::UseFixedTime",
             "CFixableTime::CancelFixedTime", "CFixableTime::NextFrame", "CFixableTime::ReadFixableTime",
             "CSaveableTime::ReadSaveableTime", "CSaveableTime::AdjustTime"],
    "fixed": ["CFixedTime::ReadFixedTime", "CPausableFixedTime::Pause", "CPausableFixedTime::UnPause",
              "CPausableFixedTime::TogglePause", "CPausableFixedTime::ReadPausableFixedTime",
              "CSaveableFixedTime::ReadSaveableFixedTime", "CSaveableFixedTime::AdjustTime"],
    "clock": ["CClock::Read", "CClock::ReadFixed", "CClock::Pause", "CClock::UnPause", "CClock::TogglePause",
              "CClock::GetPauseFlag", "CClock::PreSave", "CClock::PostLoad", "CVariableTime::SetTimeRate",
              "CVariableTime::IncreaseTimeRate", "CVariableTime::DecreaseTimeRate", "CFixableTime::UseFixedTime",
              "CFixableTime::CancelFixedTime", "CFixableTime::NextFrame"],
    "none": [],
}


def random_case(name):
    """A generator of random cases for `name`: a random start, up to three random operations,
    then `name` once to three times."""
    kind = OPS[name][1]

    def make(r):
        script = []
        for _ in range(r.choice([0, 0, 1, 2, 3])):
            chosen = r.choice(PREFIX[kind])
            script.append((chosen, argument_for(r, chosen)))
        for _ in range(r.choice([1, 1, 1, 2, 3])):
            script.append((name, argument_for(r, name)))
        return build(r, kind, script)
    return make


def edge_case(kind, script, readings, state=None, queues=None):
    """A case with nothing random: the state gClock has after construction unless one is given."""
    return dict(kind=kind, state=state if state is not None else boring_state(kind), script=script,
                queues=dict({"LbTime_GetClock": readings}, **(queues or {})), null_file=False, skew=0)


# ------------------------------------------------------------------ the edge cases


def reading_edges(name):
    """The reads: the last reading and the new one (steady, wrapped, backwards, a jump of
    0x7fffffff), the total so far (near the conversion's limits, negative, NaN), the rate."""
    kind = OPS[name][1]
    variable = name == "CVariableTime::ReadVariableTime"
    pairs = [(last, now) for last in (0, 5, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF) for now in (0, 4, 6, 0x7FFFFFFF,
                                                                                            0x80000000, MASK)]
    totals = [0.0, 4294967295.0, 4294967295.5, 2147483647.5, 2147483648.0, -1.0, math.nan, math.inf, 4294967000.0]
    rates = [1.0, 0.5, 2.0, 0.0, -1.0, math.nan] if variable else [None]
    for last, now in pairs:
        for total in totals:
            for rate in rates:
                state = boring_state(kind)
                state["ms"] = total
                state["last"] = last
                if rate is not None:
                    state["rate"] = rate
                yield edge_case(kind, [(name, None)], [now], state)


def rate_edges(name):
    kind = "time"
    for rate in RATES:
        state = boring_state(kind)
        state["rate"] = rate
        argument = rate if OPS[name][2] == "rate" else None
        # Setting takes the rate from the argument, the stepping ones from the field.
        yield edge_case(kind, [(name, argument)], [1000], state)
        yield edge_case(kind, [(name, 1.25), (name, argument), (name, 0.2)], [1000, 1010, 1020], state)
    # Repeated steps: up to the limit and back, with a reading between them.
    if name in ("CVariableTime::IncreaseTimeRate", "CVariableTime::DecreaseTimeRate"):
        script = [(name, None), ("CVariableTime::ReadVariableTime", None)] * 6
        yield edge_case(kind, script, [100 * i for i in range(14)])


def pause_edges(name):
    kind = OPS[name][1]
    flags = [0, 1, 2, -1, -0x80000000]
    for flag in flags:
        for now in (0, 100, 0x7FFFFFFF, 0x80000000, MASK):
            for paused_at in (0, 50, -1, 0x7FFFFFFF):
                state = boring_state(kind)
                state["pauseFlag"] = flag
                state["pausedAt"] = paused_at
                state["pausedDuration"] = 7
                state["ms"] = 1000.0
                state["last"] = 1000 if kind == "time" else 0
                yield edge_case(kind, [(name, None)], [now])
                yield edge_case(kind, [(name, None)], [now], state)


def fixable_edges(name):
    kind = "time"
    for flag in (0, 1, 2, -1):
        for pause in (0, 1):
            for step in (0x20, 0, 1, 1000, 1001, MASK, 0x80000000, 3):
                state = boring_state(kind)
                state.update(fixedFlag=flag, pauseFlag=pause, fixedAt=0x7FFFFFF0, fixedStep=100, fixedAdjustment=-5,
                             pausedAt=40, pausedDuration=9, ms=500.0, last=100)
                yield edge_case(kind, [(name, step)], [180, 190], state)
                yield edge_case(kind, [(name, step), (name, step)], [180, 190, 200], state)
    # Entered, stepped, left, entered again (the flag test), a step while paused, a read.
    script = [("CFixableTime::UseFixedTime", 0x20), ("CFixableTime::NextFrame", None),
              ("CFixableTime::ReadFixableTime", None), ("CFixableTime::UseFixedTime", 0x20),
              ("CPausableTime::Pause", None), ("CFixableTime::NextFrame", None), ("CPausableTime::UnPause", None),
              ("CFixableTime::CancelFixedTime", None), ("CFixableTime::CancelFixedTime", None),
              ("CFixableTime::UseFixedTime", 0x20), ("CFixableTime::NextFrame", None),
              ("CFixableTime::CancelFixedTime", None), ("CSaveableTime::ReadSaveableTime", None)]
    yield edge_case(kind, script, [100, 200, 300, 400, 500, 600, 700, 800, 900, 1000])


def saveable_edges(name):
    kind = OPS[name][1]
    for saved_value in (0, 1, 0x12345678, 0x80000000, MASK):
        for count in (4, 5, 6, 7, 0, 1, 3, 8, 0x40000004, MASK):
            for written in ((4,) if name.endswith("Load") else (0,)):
                state = boring_state(kind)
                state["savedTime"] = saved_value
                queues = {"LbFile_Write:count": [count], "LbFile_Write:result": [0], "LbFile_Read:count": [count],
                          "LbFile_Read:result": [1], "LbFile_Read:data": [0xA1B2C3D4], "LbFile_Read:written": [written]}
                yield edge_case(kind, [(name, None)], [10], state, queues)
                if name.endswith("Load") and count not in (4, 5, 6, 7):
                    queues = dict(queues, **{"LbFile_Read:written": [0]})
                    yield edge_case(kind, [(name, None)], [10], state, queues)


def adjust_edges(name):
    kind = OPS[name][1]
    for saved_value in (0, 1000, 0x7FFFFFFF, 0x80000000, MASK):
        for now in (0, 100, 0x7FFFFFFF, MASK):
            state = boring_state(kind)
            state["savedTime"] = saved_value
            state["adjustment"] = 99
            yield edge_case(kind, [(name, None)], [now], state)


def clock_edges(name):
    """The chain through the clock the game uses, from the state the constructor leaves."""
    kind = "clock"
    steady = [1000 + 16 * i for i in range(20)]
    yield edge_case(kind, [(name, None)] * 3, steady)
    script = [("CVariableTime::SetTimeRate", 0.5), (name, None), ("CVariableTime::SetTimeRate", 2.0), (name, None),
              ("CVariableTime::SetTimeRate", 0.0), (name, None), ("CVariableTime::SetTimeRate", 1.0), (name, None)]
    yield edge_case(kind, script, [1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700])
    for readings in ([0xFFFFFFF0, 0x10, 0x20, 0x30], [0x7FFFFFFF, 0xFFFFFFFE, 0x7FFFFFFD, 0], [500, 400, 300, 200]):
        yield edge_case(kind, [(name, None)] * 4, readings)
    script = [("CClock::Pause", None), (name, None), ("CClock::UnPause", None), (name, None),
              ("CClock::TogglePause", None), (name, None), ("CClock::TogglePause", None), (name, None),
              ("CClock::Pause", None), ("CClock::Pause", None), (name, None), ("CClock::UnPause", None)]
    yield edge_case(kind, script, [1000 + 25 * i for i in range(24)])


def clock_file_edges(name):
    """The clock's Save and Load: the time and the fixed time each succeed or fail."""
    for first in (4, 7, 0, 8):
        for second in (4, 6, 3, MASK):
            queues = {"LbFile_Write:count": [first, second], "LbFile_Write:result": [0, 0],
                      "LbFile_Read:count": [first, second], "LbFile_Read:result": [1, 1],
                      "LbFile_Read:data": [0x01020304, 0xA0B0C0D0],
                      "LbFile_Read:written": [4 if first in (4, 7) else 0, 4 if second in (4, 6) else 0]}
            state = boring_state("clock")
            state["t.savedTime"] = 0x11223344
            state["f.savedTime"] = 0x55667788
            yield edge_case("clock", [(name, None)], [10, 20], state, queues)


def tick_edges(name):
    for microseconds in (0, 1, 999, 1000, 1001, 1999, 2000, 123456789, 4294967295999, 4294967296000, 4294967296999,
                         1 << 63, (1 << 63) - 1, (1 << 64) - 1, (1 << 64) - 1000, (1 << 64) - 1001, 1 << 32,
                         (1 << 32) - 1, 1 << 40, 999999999999):
        yield edge_case("none", [(name, None)], [0], queues={"GetAbsolute": [s64(microseconds)]})


def tick_random(name):
    def make(r):
        bits = r.choice([8, 16, 32, 40, 50, 63, 64])
        value = r.getrandbits(bits)
        script = [(name, None)] * r.choice([1, 1, 2])
        case = edge_case("none", script, [0], queues={"GetAbsolute": [s64(value), s64(r.getrandbits(64))]})
        case["skew"] = r.choice([0, 4])
        return case
    return make


EDGES = {}
for _name, (_a, _k, _arg, _ans) in OPS.items():
    if _name.endswith(("::ReadFixedTime", "::ReadVariableTime")):
        EDGES[_name] = reading_edges
    elif _name.endswith(("TimeRate",)):
        EDGES[_name] = rate_edges
    elif _name.endswith(("::Pause", "::UnPause", "::TogglePause", "::GetPauseFlag", "::ReadPausableTime",
                         "::ReadPausableFixedTime")) and not _name.startswith("CClock"):
        EDGES[_name] = pause_edges
    elif _name.startswith("CFixableTime"):
        EDGES[_name] = fixable_edges
    elif _name.endswith(("::Save", "::Load")) and not _name.startswith("CClock"):
        EDGES[_name] = saveable_edges
    elif _name.endswith(("::ReadSaveableTime", "::ReadSaveableFixedTime", "::AdjustTime")):
        EDGES[_name] = adjust_edges
    elif _name in ("CClock::Save", "CClock::Load"):
        EDGES[_name] = clock_file_edges
    elif _name.startswith("CClock"):
        EDGES[_name] = clock_edges
    elif _name.startswith("timeGetTime"):
        EDGES[_name] = tick_edges


def make_test(name):
    """The test of one function: its edge cases and 200 random ones."""
    edges = EDGES[name]
    rnd = tick_random(name) if name.startswith("timeGetTime") else random_case(name)
    return FnTest(name, ppc, host, edge=lambda: list(edges(name)), rnd=rnd)


def scenario(title, kind, extra):
    """Long random scripts over a whole kind of object, from the state gClock starts in or a
    random one: the operations interact (pause, rate, fixed steps, loads) as in the game."""
    pool = PREFIX[kind] + extra

    def make(r):
        script = []
        for _ in range(r.randint(6, 18)):
            chosen = r.choice(pool)
            script.append((chosen, argument_for(r, chosen)))
        case = build(r, kind, script)
        if r.random() < 0.5:
            case["state"] = boring_state(kind)
        return case
    return FnTest(title, ppc, host, edge=lambda: [], rnd=make)


TESTS = [make_test(name) for name in OPS]
TESTS += [
    scenario("Scenario: CClock", "clock", ["CClock::Save", "CClock::Load", "CClock::Read", "CClock::ReadFixed"]),
    scenario("Scenario: CSaveableTime", "time", ["CSaveableTime::Save", "CSaveableTime::Load", "CPausableTime::GetPauseFlag"]),
    scenario("Scenario: CSaveableFixedTime", "fixed", ["CSaveableFixedTime::Save", "CSaveableFixedTime::Load"]),
]
