"""Unit w207, the world's load, against libtpw_unit_WorldLoad.dylib (tests/unit_WorldLoad.cpp), main app ("SimTheme Park"), src/game/Thing.cpp, src/game/World.cpp, src/game/rides/rsse.cpp
and src/game/ControlManager.cpp, src/game/Object.cpp, src/game/advisor/Advisor.cpp, src/game/park/BankAccount.cpp, src/game/park/analyser.cpp, src/game/rides/FlyingRide.cpp, src/game/save/FS_MasterSave.cpp:
  CWorld::Load 0x1013c9f0, LOADVAR<CWorld::WorldState> 0x101415b4, LOADVAR<CThing::ThingModel> 0x101416f0, CThing::ForceThingId 0x1012dfa8

A case is a file and a call. The file holds what a saved park holds, little-endian: a version word, the 25 fields of the world, the three parts of
the world (the fakes read nothing), the head of the used list and then, for each entry of the list, the next row and the thing model. The world is
built twice by ONE piece of Python code: the world's block (0x1da748 bytes over the fill 0xa5, only the tail from +0x1da708 holds fields and the case
sets those to distinct values, so a field read from or written to the wrong place shows), the thing table with its heads, and two small stale blocks
that table rows may point at, in emulator memory (big-endian) for the original and in the host library's memory (little-endian) for the translation.
The basic LOADVAR instances (int, unsigned int, long, unsigned long, the thing id struct) run REAL on both sides, so the file is the fake: LbFile_Read
serves the case's bytes from a cursor and answers from the case's queues (the result and the transferred count). Every other callee is a fake that
records the arguments the real one reads and answers from the case's queue: the twenty Serialise functions (the three parts of the world as offsets
from the world, the things by the label of their block), InitThingHeap and ReconstructFreeList (they leave the table as the case set it), the parts
built in place that are not translated here (CThoughts::CThoughts, Pilot::Pilot, CThing::CThing: the label of the block and the offset in it),
the two sprite switches, GAME_ASSERTMSG and the serialise log. Load makes the things with operator new: both sides fill each block with 0xa5, log
its size, give it a label (100 and up, in order) and keep it, so that the module can read the blocks back.

What is compared: the answer, the log, the fields of the world's tail (also the two Load does not read), that the rest of the world still holds
the fill, the table rows the call changed (thing as a label, index word, the three links as row numbers), the two heads, and every block Load made
(label, size, id halfword, kind byte, the byte behind it and a hash of the rest, each aligned word in the smaller of its two byte orders because the
Mac's constructors write numbers into the blocks, which are big-endian there).

No fused operations and no floats. Not tested: operator new answering NULL (the host's cannot, and the compiler folds the tests), a head or next row
of 0x2800 or more (the table has no more rows and the original does not check).
"""
import ctypes as C
import hashlib
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_WorldLoad"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
LOAD, LV_WS, LV_TM, FORCEID = 0x1013C9F0, 0x101415B4, 0x101416F0, 0x1012DFA8
SIZES = {LOAD: 4756, LV_WS: 236, LV_TM: 236, FORCEID: 16}
OWN = set(SIZES)
# run as the original as well (not translated here): the unsigned long, long, int, unsigned int and thing id struct LOADVAR instances
REAL_EXTRA = {0x1000F02C, 0x1000EE04, 0x1000EF18, 0x100F44A4, 0x1000EB4C}

# ---- the callees faked on both sides (the asm's bl targets that are not OWN and not REAL_EXTRA)
ASSERT, WRITELOG = 0x1000DDE8, 0x1015B3DC
INIT_HEAP, RECONSTRUCT = 0x1013AB48, 0x1013DD14
CTHOUGHTS, PILOT, THING_CTOR = 0x1012EC88, 0x10134610, 0x1013DCE0
ESP_ENABLE, ESP_DISABLE = 0x1007AC68, 0x1007ACA4
# the Serialise functions Load calls, all stubs here (the last eight are not translated yet)
SER = {"CBankAccount": 0x100F2D10, "CChallengeHQ": 0x100F6FE0, "CControlManager": 0x100F9ED0, "CEntertainer": 0x100FB4A8,
       "CGuard": 0x100FDF94, "CHandyman": 0x100FF4C0, "CMacroAI": 0x10100890, "CMap": 0x10100E80, "CMechanic": 0x101045E8,
       "CParkAnalyser": 0x100E3004, "CResearcher": 0x101210E4, "CWeather": 0x10137F9C,
       "CPerson": 0x101170B4, "CObject": 0x10106570, "CStaffHQ": 0x1012AC08, "CMechanicHQ": 0x10105348, "CTagSystem": 0x1012C64C,
       "CResearchLab": 0x10121CE8, "CUIMsgReceiver": 0x10130928, "CAdvisor": 0x1000A468}
# the thing model (the kind byte of the thing) and the class the original reads it with
CLASS_OF_KIND = {1: "CPerson", 3: "CObject", 4: "CMechanic", 5: "CHandyman", 6: "CEntertainer", 7: "CGuard", 8: "CResearcher",
                 9: "CStaffHQ", 10: "CMechanicHQ", 11: "CAdvisor", 12: "CTagSystem", 13: "CParkAnalyser", 14: "CResearchLab",
                 15: "CWeather", 16: "CBankAccount", 17: "CUIMsgReceiver", 19: "CChallengeHQ"}
HANDLED = sorted(CLASS_OF_KIND)

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_wld_addr(name)
GSAVELOAD = 0x1037FEFC
TABLE, ROWS, ROW_BYTES = 0x1034CBFC, 0x2800, 0x14
USED_HEAD, FREE_HEAD = 0x1034CBF4, 0x1034CBF8
WORLD_SIZE, STALE_SIZE = 0x1DA748, 0x20
FILE_HANDLE = 0x6E100000
# labels in the logs: gSaveLoad, the file, the two stale blocks (10 and 11); the blocks Load makes get 100 and up
LABEL_SAVELOAD, LABEL_FILE, LABEL_STALE = 6, 8, 10
MAX_ROWS = 64       # changed rows listed in an observation (the count of all of them is listed too)

# the fields of the world's tail (offset, width): the 25 fields Load reads and the two it does not (+0x1da71c, +0x1da736)
TAIL = ([(0x1DA708, 4), (0x1DA70C, 4), (0x1DA710, 4), (0x1DA714, 4)] + [(0x1DA718 + 2 * i, 2) for i in range(16)] + [(0x1DA738, 4)] +
        [(0x1DA73C + 2 * i, 2) for i in range(6)])
TAIL_START = 0x1DA708
# ---- the host's offsets and sizes the module relies on, checked once against tpw_wld_layout (catches a header that moved)
HOST_LAYOUT = ([WORLD_SIZE] + [off for off, width in TAIL[:4]] + [off for off, width in TAIL[4:20]] + [0x1DA738] +
               [off for off, width in TAIL[21:]] + [0x2D8, 0x1D943C, 2, ROWS])

FILL_WORLD = b"\xa5" * WORLD_SIZE
FILL_STALE = b"\xa5" * STALE_SIZE
DEFAULT_TABLE = b"".join(struct.pack(">5I", 0, i, 0, 0, 0) for i in range(ROWS))

# the file: the version word, then the 25 fields in the order Save wrote them (the widths; the 19th is the world state), then the head's row
# and per entry the next row and the model
FIELD_WIDTHS = [2] * 5 + [4] + [2] * 2 + [4, 4] + [2] * 2 + [4] + [2] * 5 + [4] + [2] * 6
STATE_FIELD = 18
assert len(FIELD_WIDTHS) == 25 and sum(FIELD_WIDTHS) == 60

# for debugging a mismatch from a scratch script: the last observation of each side and the raw blocks of the last case
LAST = {}
STASH = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.update(REAL_EXTRA)
R = CL.recorder


def where(e, p):
    """A world member as its offset from the world, anything else as 0x40000000 plus its label (the host's Where does the same)."""
    d = p - e.ws_world if e.ws_world is not None else -1
    if 0 <= d < WORLD_SIZE:
        return d
    return 0x40000000 + e.cl.lab(p)


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4))[:159],))


def stub_world_call(name):
    """InitThingHeap and ReconstructFreeList: where the world is, the table stays as the case set it."""
    def stub(e):
        e.cl.rec(name, (where(e, e.r(3)),))
    return stub


def stub_built(name):
    """A part built in place (CThoughts, Pilot, CThing): the label of the block it is in and the offset in it. r3 stays `this`."""
    def stub(e):
        p = e.r(3)
        label, offset = -1, 0
        for base, size, lab in e.ws_allocs:
            if base <= p < base + size:
                label, offset = lab, p - base
                break
        e.cl.rec(name, (label, offset))
    return stub


def ser_stub(name):
    def stub(e):
        L = e.cl
        L.rec(name, (where(e, e.r(3)), L.lab(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & MASK))
        e.set_r(3, T.u32(L.ret(name)))
    return stub


def lb_read(e):
    """LbFile_Read: the bytes from the cursor. The count is what it took unless the queue forces another one; the buffer gets the bytes only
    when the count is the length and the result is not negative (the result goes through the hidden result pointer in r3)."""
    L, length = e.cl, e.r(6)
    L.rec("LbFile_Read", (L.lab(e.r(4)), length))
    data = e.ws_file[e.ws_cursor:e.ws_cursor + length]
    taken = len(data)
    count = taken
    forced = L.ret("LbFile_Read:xfer", -1)
    if forced >= 0:
        count = forced & MASK
    result = L.ret("LbFile_Read") & MASK
    e.write(e.r(7), count.to_bytes(4, "big"))
    e.write(e.r(3), result.to_bytes(4, "big"))
    if count == length and s32(result) >= 0:
        e.write(e.r(5), (data + bytes(length))[:length])
    e.ws_cursor += taken


def op_new(e):
    """operator new: a block of 0xa5, a new label, the size and the label logged, the block kept for the module."""
    L, size = e.cl, e.r(3)
    p = e.alloc(size, b"\xa5" * size)
    label = L.newlab(p)
    L.rec("new", (size, label))
    e.ws_allocs.append((p, size, label))
    e.set_r(3, p)


STUBS.internal(ASSERT, stub_assert)
STUBS.internal(WRITELOG, R("CSaveLoad::WriteSerialiseLog", "Lisi"))
STUBS.internal(INIT_HEAP, stub_world_call("CWorld::InitThingHeap"))
STUBS.internal(RECONSTRUCT, stub_world_call("CWorld::ReconstructFreeList"))
STUBS.internal(CTHOUGHTS, stub_built("CThoughts::CThoughts"))
STUBS.internal(PILOT, stub_built("Pilot::Pilot"))
STUBS.internal(THING_CTOR, stub_built("CThing::CThing"))
STUBS.internal(ESP_ENABLE, R("ESP_EnableSpriteDraw", ""))
STUBS.internal(ESP_DISABLE, R("ESP_DisableSpriteDraw", ""))
for _name, _addr in SER.items():
    STUBS.internal(_addr, ser_stub(_name + "::Serialise"))
STUBS.imp("LbFile_Read__FPvPvUlPUl", lb_read)
STUBS.imp("__nw__FUl", op_new)


def setup(runner):
    T.install_std_stubs(runner.emu)       # the C library's memset, memcpy, strlen ... (tpwtest.py 491 on)


# ---------------------------------------------------------------------------------------------------------------- the two sides


def canon(raw):
    """The block without its first four bytes, each aligned word in the smaller of its two byte orders; the odd bytes at the end as they are."""
    n = len(raw) - len(raw) % 4
    out = bytearray()
    for i in range(4, n, 4):
        w = raw[i:i + 4]
        out += min(w, w[::-1])
    out += raw[n:]
    return bytes(out)


def summarize(raw, byte, label):
    """One block as (label, size, id halfword, kind byte, byte 3, hash of the rest)."""
    STASH[(byte, label)] = raw
    return (label, len(raw), int.from_bytes(raw[0:2], byte), raw[2], raw[3], hashlib.sha1(canon(raw)).hexdigest()[:16])


class Side:
    """What the shared code needs from either memory."""

    def put(self, a, width, v):
        self.write(a, (v & ((1 << (8 * width)) - 1)).to_bytes(width, self.byte))

    def get(self, a, width):
        return int.from_bytes(self.read(a, width), self.byte)


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    byte = "big"

    def __init__(self, e, case):
        self.e = e
        fixed = {GSAVELOAD: LABEL_SAVELOAD, FILE_HANDLE: LABEL_FILE}
        self.world = e.alloc(WORLD_SIZE, FILL_WORLD)
        self.stale = []
        for k in range(2):
            a = e.alloc(STALE_SIZE, FILL_STALE)
            self.stale.append(a)
            fixed[a] = LABEL_STALE + k
        e.ws_world = self.world
        e.ws_allocs = []
        e.ws_file = case.get("file", b"")
        e.ws_cursor = 0
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.cl = None

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, addr, words):
        return T.call_ppc(self.e, addr, words, max_insns=400_000_000, timeout_us=1_800_000_000)[0] & MASK

    # ---- the world
    def table_default(self):
        self.e.write(TABLE, DEFAULT_TABLE)
        self.heads(0, 0)

    def row_addr(self, p1):
        return TABLE + (p1 - 1) * ROW_BYTES if p1 else 0

    def row(self, r, thing, index, nextp1, prevp1, freep1):
        self.e.write(TABLE + r * ROW_BYTES, struct.pack(">5I", self.stale[thing - 1] if thing else 0, index & MASK,
                                                         self.row_addr(nextp1), self.row_addr(prevp1), self.row_addr(freep1)))

    def heads(self, usedp1, freep1):
        self.e.write(USED_HEAD, struct.pack(">I", self.row_addr(usedp1)))
        self.e.write(FREE_HEAD, struct.pack(">I", self.row_addr(freep1)))

    def snapshot(self):
        self.snap = bytes(self.e.read(TABLE, ROWS * ROW_BYTES))

    def label_of(self, p):
        """What a row's thing pointer is: 0 for NULL, a stale block's label, the label of a block this case made, else -1."""
        if p == 0:
            return 0
        for k, a in enumerate(self.stale):
            if p == a:
                return LABEL_STALE + k
        for base, size, lab in self.e.ws_allocs:
            if p == base:
                return lab
        return -1

    def rowp1(self, a):
        if a == 0:
            return 0
        d = a - TABLE
        return d // ROW_BYTES + 1 if 0 <= d < ROWS * ROW_BYTES else -1

    def changed_rows(self):
        now = bytes(self.e.read(TABLE, ROWS * ROW_BYTES))
        rows, total = [], 0
        if now != self.snap:
            for r in range(ROWS):
                a = r * ROW_BYTES
                if now[a:a + ROW_BYTES] != self.snap[a:a + ROW_BYTES]:
                    total += 1
                    if len(rows) < MAX_ROWS:
                        thing, index, nxt, prv, fr = struct.unpack(">5I", now[a:a + ROW_BYTES])
                        rows.append((r, self.label_of(thing), index, self.rowp1(nxt), self.rowp1(prv), self.rowp1(fr)))
        return total, tuple(rows)

    def get_heads(self):
        used, free = struct.unpack(">2I", self.e.read(USED_HEAD, 8))
        return self.rowp1(used), self.rowp1(free)

    def blocks(self):
        return tuple(summarize(bytes(self.e.read(a, n)), "big", lab) for a, n, lab in self.e.ws_allocs)

    # ---- the calls
    def load(self):
        return s32(self.call(LOAD, [self.world, FILE_HANDLE]))

    def loadvar(self, which, value, name, nolog):
        cell = self.e.alloc(4, struct.pack(">I", value & MASK))
        text = self.e.alloc(len(name) + 1, name.encode("latin-1") + b"\0")
        answer = s32(self.call(LV_WS if which == 0 else LV_TM, [FILE_HANDLE, cell, text, nolog & MASK]))
        return answer, struct.unpack(">I", self.e.read(cell, 4))[0]

    def forceid(self, ident, junk):
        """The id is the high halfword of the register (the struct is passed left-justified)."""
        self.call(FORCEID, [self.stale[0], ((ident & 0xFFFF) << 16) | (junk & 0xFFFF)])


class Host(Side):
    """The same on the host library."""
    byte = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_wld_reset()
        self.world = lib.tpw_wld_world()
        self.stale = [lib.tpw_wld_addr(b"stale0"), lib.tpw_wld_addr(b"stale1")]
        fixed = {LABEL_SAVELOAD: lib.tpw_wld_addr(b"gSaveLoad"), LABEL_FILE: lib.tpw_wld_addr(b"file"),
                 LABEL_STALE: self.stale[0], LABEL_STALE + 1: self.stale[1]}
        data = case.get("file", b"")
        buf = (C.c_uint8 * max(len(data), 1)).from_buffer_copy(data or b"\0")
        lib.tpw_wld_set_file(buf, len(data))
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    # ---- the world
    def table_default(self):
        pass                                   # tpw_wld_reset made it

    def row(self, r, thing, index, nextp1, prevp1, freep1):
        self.lib.tpw_wld_row(r, thing, index & MASK, nextp1, prevp1, freep1)

    def heads(self, usedp1, freep1):
        self.lib.tpw_wld_heads(usedp1, freep1)

    def snapshot(self):
        self.lib.tpw_wld_snapshot()

    def changed_rows(self):
        out = (C.c_int64 * (6 * MAX_ROWS))()
        total = self.lib.tpw_wld_changed(out, MAX_ROWS)
        return total, tuple(tuple(out[6 * i:6 * i + 6]) for i in range(min(total, MAX_ROWS)))

    def get_heads(self):
        out = (C.c_int64 * 2)()
        self.lib.tpw_wld_get_heads(out)
        return tuple(out)

    def blocks(self):
        out, info = [], (C.c_uint64 * 3)()
        for k in range(self.lib.tpw_wld_alloc_count()):
            self.lib.tpw_wld_alloc_info(k, info)
            out.append(summarize(C.string_at(info[0], info[1]), "little", info[2]))
        return tuple(out)

    # ---- the calls
    def load(self):
        return self.lib.tpw_wld_load()

    def loadvar(self, which, value, name, nolog):
        cell = C.c_uint32(value & MASK)
        answer = self.lib.tpw_wld_loadvar(which, C.byref(cell), name.encode("latin-1"), s32(nolog))
        return answer, cell.value

    def forceid(self, ident, junk):
        self.lib.tpw_wld_forceid(self.stale[0], ident & 0xFFFF)


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return

    def fn(name, restype, argtypes):
        f = getattr(lib, "tpw_wld_" + name)
        f.restype, f.argtypes = restype, argtypes

    fn("addr", C.c_uint64, [C.c_char_p])
    fn("layout", C.c_int32, [C.POINTER(C.c_int64)])
    fn("world", C.c_uint64, [])
    fn("row", None, [C.c_int32, C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.c_int32])
    fn("reset", None, [])
    fn("heads", None, [C.c_int32, C.c_int32])
    fn("get_heads", None, [C.POINTER(C.c_int64)])
    fn("set_file", None, [C.POINTER(C.c_uint8), C.c_int32])
    fn("snapshot", None, [])
    fn("changed", C.c_int32, [C.POINTER(C.c_int64), C.c_int32])
    fn("alloc_count", C.c_int32, [])
    fn("alloc_info", None, [C.c_int32, C.POINTER(C.c_uint64)])
    fn("load", C.c_int32, [])
    fn("loadvar", C.c_int32, [C.c_int32, C.POINTER(C.c_uint32), C.c_char_p, C.c_int32])
    fn("forceid", None, [C.c_uint64, C.c_uint32])
    out = (C.c_int64 * 40)()
    n = lib.tpw_wld_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world and the file


def le(v, width):
    return (v & ((1 << (8 * width)) - 1)).to_bytes(width, "little")


def file_bytes(c):
    """The file of a case: version, the 25 fields, the head's row, then (next row, model) per entry of the list."""
    out = le(c["version"], 4)
    for v, w in zip(c["fields"], FIELD_WIDTHS):
        out += le(v, w)
    out += le(c["head"], 4)
    for nxt, model in c["chain"]:
        out += le(nxt, 4) + le(model, 4)
    return out


def build(s, c):
    """Writes the case's globals through Side.put and Side.row: the same code for both memories (offsets are the Mac's)."""
    if c["op"] == "load":
        for (off, width), v in zip(TAIL, c["tail"]):
            s.put(s.world + off, width, v)
        s.table_default()
        for r, row in c["rows"].items():
            s.row(r, row["thing"], row["index"], row["next"], row["prev"], row["free"])
        s.heads(c["used"], c["free"])
    elif c["op"] == "forceid":
        s.write(s.stale[0], c["fill"])


def execute(s, c):
    """The call."""
    op = c["op"]
    if op == "load":
        s.snapshot()
        return s.load()
    if op == "loadvar":
        return s.loadvar(c["which"], c["value"], c["name"], c["nolog"])
    s.forceid(c["id"], c["junk"])
    return None


def observe(s, c, answer):
    """Everything the call may change, read back the same way on both sides; pointers as labels or row numbers."""
    op = c["op"]
    out = {}
    if op == "load":
        out["answer"] = answer
        out["tail"] = tuple(s.get(s.world + off, width) for off, width in TAIL)
        out["world"] = int(bytes(s.read(s.world, TAIL_START)) == FILL_WORLD[:TAIL_START])
        out["rows"] = s.changed_rows()
        out["heads"] = s.get_heads()
        out["blocks"] = s.blocks()
    elif op == "loadvar":
        out["answer"], out["value"] = answer
    else:
        out["id"] = s.get(s.stale[0], 2)
        out["rest"] = bytes(s.read(s.stale[0] + 2, STALE_SIZE - 2))
        out["other"] = bytes(s.read(s.stale[1], STALE_SIZE))
    out["log"] = s.log()
    return out


def run(s, c):
    try:
        build(s, c)
        out = observe(s, c, execute(s, c))
        LAST[s.byte] = out
        return out
    finally:
        s.done()


def ppc(e, c):
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)


# ---------------------------------------------------------------------------------------------------------------- case generators

ANSWERS = [1, -1, 0x10000, -0x80000000, 2, 0x7FFFFFFF]
STATES = [0, 1, 2, 3, 4, 4, 5, 0xFFFFFFFF, 0x80000000, 0xFFFFFFFC, 0x7FFFFFFF]
# every model value the switch distinguishes: 1 and 3 to 17, 19, the gaps (0, 2, 18), above 19, and values whose low byte is a handled one
MODELS_ALL = list(range(0x15)) + [0x7F, 0x80, 0xFF, 0x100, 0x101, 0x10D, 0x113, 0x80000001, 0xFFFFFFFF, 0x80000000]
POOL = [k for k in HANDLED if k != 13] * 3 + [13] + [0, 2, 18, 20, 0x7F, 0x80, 0xFF, 0x100, 0x101, 0x10D, 0xFFFFFFFF]


def tail_values(r):
    """The tail's fields: distinct random values (now and then a special one), so a field read from or written to the wrong place shows."""
    out = []
    for off, width in TAIL:
        v = r.getrandbits(8 * width)
        if r.random() < 0.12:
            v = r.choice([0, 1, (1 << (8 * width)) - 1, 1 << (8 * width - 1)])
        out.append(v)
    return out


def version_value(r):
    return r.choice([0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32), r.getrandbits(8)])


def field_values(r, state=None):
    out = []
    for w in FIELD_WIDTHS:
        v = r.getrandbits(8 * w)
        if r.random() < 0.12:
            v = r.choice([0, 1, (1 << (8 * w)) - 1, 1 << (8 * w - 1)])
        out.append(v)
    out[STATE_FIELD] = r.choice(STATES + [r.getrandbits(32)]) if state is None else state
    return out


def preset_row(r, row):
    """A row as the case left it before the call: a thing (none or a stale block), the index word and leftover links."""
    return {"thing": r.choice([0, 0, 1, 2]),
            "index": r.choice([row, row, row, r.getrandbits(32), r.getrandbits(16), 0, 0xFFFF, 0x10000, 0xFFFFFFFF, 1 << 31]),
            "next": r.choice([0, r.randrange(1, ROWS + 1)]), "prev": r.choice([0, r.randrange(1, ROWS + 1)]),
            "free": r.choice([0, r.randrange(1, ROWS + 1)])}


def load_case(r, kinds=None, rows=None, version=None, fields=None, state=None, queues=None, cut=None, pre=None, used=None, free=None):
    """A list of len(kinds) entries in the rows `rows` (the first is the head; a row may come twice; only the head may be row 0)."""
    if kinds is None:
        kinds = [r.choice(POOL) for _ in range(r.choice([1, 1, 2, 3, 4, 5, 8]))]
    n = len(kinds)
    if rows is None:
        rows = r.sample(range(1, ROWS), n)
        if r.random() < 0.15:
            rows[0] = r.choice([0, ROWS - 1])
    if pre is None:
        pre = {}
        for row in sorted(set(rows)):
            if r.random() < 0.7:
                pre[row] = preset_row(r, row)
        for _ in range(r.choice([0, 0, 1, 3])):
            row = r.randrange(ROWS)
            if row not in pre:
                pre[row] = preset_row(r, row)
    c = {"op": "load", "tail": tail_values(r), "version": version_value(r) if version is None else version,
         "fields": field_values(r, state) if fields is None else fields, "head": rows[0],
         "chain": [(rows[i + 1] if i + 1 < n else 0, kinds[i]) for i in range(n)], "rows": pre,
         "used": (r.randrange(ROWS + 1) if r.random() < 0.5 else 0) if used is None else used,
         "free": (r.randrange(ROWS + 1) if r.random() < 0.5 else 0) if free is None else free,
         "queues": dict(queues) if queues else {}}
    c["file"] = file_bytes(c) if cut is None else file_bytes(c)[:cut]
    return c


def inject(r, c):
    """Now and then a failure at a random place: a read, a short read, a Serialise answer, a file that ends early."""
    q, n = c["queues"], len(c["chain"])
    k = r.random()
    if k < 0.10:
        step = r.randrange(27 + 2 * n)
        q["LbFile_Read"] = [0] * step + [r.choice([-1, 1, 2, 0x7FFFFFFF])]
    elif k < 0.18:
        step = r.randrange(27 + 2 * n)
        q["LbFile_Read:xfer"] = [-1] * step + [r.choice([0, 1, 2, 3, 4, 5, 0xFFFFFFFF])]
    elif k < 0.40:
        names = ["CControlManager", "CMacroAI", "CMap"] + [CLASS_OF_KIND[m] for _, m in c["chain"] if m in CLASS_OF_KIND]
        name = r.choice(names)
        before = 0
        if name not in ("CControlManager", "CMacroAI", "CMap"):
            hits = [i for i, (_, m) in enumerate(c["chain"]) if CLASS_OF_KIND.get(m) == name]
            before = r.randrange(len(hits))
        q[name + "::Serialise"] = [0] * before + [r.choice(ANSWERS)]
    elif k < 0.46:
        c["file"] = c["file"][:r.randrange(len(c["file"]) + 1)]
    return c


def load_edges():
    """Every branch of the asm at least once: each model alone, all handled models, each answer of each callee, each read step, each cut."""
    r = random.Random(207101)
    for model in MODELS_ALL:
        yield load_case(r, kinds=[model])
    handled_rows = r.sample(range(1, ROWS), len(HANDLED))
    yield load_case(r, kinds=list(HANDLED), rows=handled_rows)
    yield load_case(r, kinds=list(reversed(HANDLED)), rows=list(reversed(handled_rows)))
    yield load_case(r, kinds=[18, 1, 0, 3, 2, 4, 0x14, 5, 0xFF, 6], used=0)
    yield load_case(r, kinds=[10, 12, 16, 17], used=7, free=9)
    yield load_case(r, kinds=[10, 12, 16, 17, 10, 12, 16, 17])
    # the rows of the list: the head in row 0 or in the last row, the last row inside a list, a row twice, a row linking to itself
    yield load_case(r, kinds=[12], rows=[0])
    yield load_case(r, kinds=[12, 16], rows=[0, 3])
    yield load_case(r, kinds=[16], rows=[ROWS - 1])
    yield load_case(r, kinds=[16, 12], rows=[ROWS - 1, 1])
    yield load_case(r, kinds=[12, 16, 17], rows=[1, ROWS - 1, 2])
    yield load_case(r, kinds=[1, 12, 16], rows=[5, 7, 5])
    yield load_case(r, kinds=[12, 12], rows=[5, 5])
    yield load_case(r, kinds=[3, 12, 16, 17, 0], rows=[5, 7, 9, 7, 5])
    yield load_case(r, kinds=[12, 18, 12], rows=[5, 6, 5])
    # rows the case set before: the thing, the index word (the id is its low halfword) and leftover links
    for kind in (12, 0):
        for thing in (0, 1, 2):
            for index in (9, 0x12340009, 0xFFFFFFFF, 0x8000, 0):
                pre = {9: {"thing": thing, "index": index, "next": 4, "prev": 5, "free": 6}, 4: preset_row(r, 4)}
                yield load_case(r, kinds=[kind], rows=[9], pre=pre)
    # a Serialise function of the things answering nonzero, at its first and at its second call
    for kind in HANDLED:
        for v in (1, -1, 0x10000, -0x80000000):
            name = CLASS_OF_KIND[kind] + "::Serialise"
            yield load_case(r, kinds=[kind, kind], queues={name: [v]})
            yield load_case(r, kinds=[kind, kind], queues={name: [0, v]})
    # the three parts of the world answering nonzero
    for name in ("CControlManager", "CMacroAI", "CMap"):
        for v in ANSWERS:
            yield load_case(r, kinds=[1, 3], queues={name + "::Serialise": [v]})
    # a read failing, ending short or long at every step (the version, the 25 fields, the head, then two reads per thing)
    for step in range(27 + 2 * 2):
        for fail in (-1, 1):
            yield load_case(r, kinds=[1, 12], queues={"LbFile_Read": [0] * step + [fail]})
        for x in (0, 1, 2, 3, 4, 5, 0xFFFFFFFF):
            yield load_case(r, kinds=[1, 12], queues={"LbFile_Read:xfer": [-1] * step + [x]})
    # a file that ends after every byte
    full = len(load_case(r, kinds=[1, 12])["file"])
    for cut in range(full):
        yield load_case(r, kinds=[1, 12], cut=cut)
    # the world state, the sprite switch
    for state in STATES:
        yield load_case(r, kinds=[12], state=state)
    # the version word, which every thing's Serialise receives
    for version in (0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x12345678):
        yield load_case(r, kinds=[1, 3], version=version)


def load_random(r):
    return inject(r, load_case(r))


# ---- LOADVAR of the two enums
NAMES = ["", "a", "mWorldState ", "thingmodel", "x" * 30, "tab\tname"]
VALUES = [0, 1, 2, 3, 0x13, 0x14, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x12345678, 0xDEADBEEF]


def lv_case(r, which, value=None, name=None, nolog=None, file=None, queues=None):
    q = {}
    if queues is not None:
        q.update(queues)
    elif r.random() < 0.3:
        if r.random() < 0.4:
            q["LbFile_Read"] = [r.choice([-1, 1, 2])]
        else:
            q["LbFile_Read:xfer"] = [r.choice([0, 1, 2, 3, 4, 5, 0xFFFFFFFF])]
    return {"op": "loadvar", "which": which, "value": r.choice(VALUES + [r.getrandbits(32)]) if value is None else value,
            "name": r.choice(NAMES) if name is None else name, "nolog": r.choice([0, 0, 0, 1, 2, -1, 0x80000000]) if nolog is None else nolog,
            "file": r.randbytes(r.choice([4, 4, 4, 4, 3, 2, 0, 5, 6])) if file is None else file, "queues": q}


def lv_edges(which):
    r = random.Random(207201 + which)
    for v in VALUES:
        yield lv_case(r, which, value=0x55AA55AA, name="thingmodel", nolog=0, file=struct.pack("<I", v), queues={})
    for size in range(7):
        yield lv_case(r, which, value=0x55AA55AA, name="mWorldState ", nolog=0, file=bytes(range(1, 1 + size)), queues={})
    for name in NAMES:
        yield lv_case(r, which, name=name, nolog=0, file=b"\x01\x02\x03\x04", queues={})
    for nolog in (0, 1, 2, -1, 0x80000000, 0x7FFFFFFF):
        yield lv_case(r, which, value=0x1234, nolog=nolog, file=b"\x01\x02\x03\x04", queues={})
    for res in (-1, 1, 2, 0x7FFFFFFF):
        yield lv_case(r, which, value=5, name="mWorldState ", nolog=0, file=b"\x01\x02\x03\x04", queues={"LbFile_Read": [res]})
    for x in (0, 1, 3, 4, 5, 0xFFFFFFFF):
        yield lv_case(r, which, value=5, name="mWorldState ", nolog=0, file=b"\x01\x02\x03\x04", queues={"LbFile_Read:xfer": [x]})


def lv_random(r, which):
    return lv_case(r, which)


# ---- CThing::ForceThingId
def fid_case(r, ident=None, junk=None, fill=None):
    return {"op": "forceid", "id": r.getrandbits(16) if ident is None else ident, "junk": r.getrandbits(16) if junk is None else junk,
            "fill": r.randbytes(STALE_SIZE) if fill is None else fill, "queues": {}}


def fid_edges():
    r = random.Random(207301)
    for ident in (0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x1234, 0x00FF, 0xFF00):
        for junk in (0, 0xFFFF, 0xABCD):
            yield fid_case(r, ident=ident, junk=junk)
    for fill in (b"\xa5" * STALE_SIZE, bytes(STALE_SIZE), b"\xff" * STALE_SIZE, bytes(range(STALE_SIZE))):
        yield fid_case(r, fill=fill)


def fid_random(r):
    return fid_case(r)


TESTS = [
    FnTest("CWorld::Load", ppc, host, edge=load_edges, rnd=load_random),
    FnTest("LOADVAR<CWorld::WorldState>", ppc, host, edge=lambda: lv_edges(0), rnd=lambda r: lv_random(r, 0)),
    FnTest("LOADVAR<CThing::ThingModel>", ppc, host, edge=lambda: lv_edges(1), rnd=lambda r: lv_random(r, 1)),
    FnTest("CThing::ForceThingId", ppc, host, edge=fid_edges, rnd=fid_random),
]
