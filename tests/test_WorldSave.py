"""Unit w205, the world's save and its small neighbours, against libtpw_unit_WorldSave.dylib (tests/unit_WorldSave.cpp), main app
("SimTheme Park"), src/World_save.cpp and src/ChunkFile.cpp:
  CWorld::Save 0x1013bf18, SAVEVAR<CWorld::WorldState> 0x1014182c, SAVEVAR<CThing::ThingModel> 0x10141980, CWorld::GetStaffHQ 0x1013e99c,
  CWorld::GetMechanicHQ 0x1013ea24, CMacroAI::~CMacroAI 0x1013a13c, CMapAttribFileReader::~CMapAttribFileReader 0x1013e6b0,
  TbPtrList<CIndividualChunkHandler>::~TbPtrList 0x1013e7f0, CThing::CThing(const CSerialise&) 0x1013dce0

A case is a world and a call. The world is built twice by ONE piece of Python code: the world's block (0x1da748 bytes over the fill 0xa5, only the
tail from +0x1da708 holds fields), the thing table with its heads and the thing blocks (0x80 bytes, id at +0, kind at +2, dead flag at +3) in
emulator memory (big-endian) for the original and in the host library's memory (little-endian) for the translation. The basic SAVEVAR instances
(int, unsigned int, long, unsigned long, the thing id struct) run REAL on both sides, so the file is the fake: LbFile_Write records the bytes as
hex and answers from the case's queues (the result and the transferred count), LbFile_GetPosition answers from its queue. Every other callee is
a fake that records the arguments the real one reads and answers from the case's queue: the twenty Serialise functions (the three parts of the
world as offsets from the world, the things by the label of their block), the mouse (operator new answers NULL or a block and logs which of the
two sizes it was, the default mode's constructor takes a new label, the mouse's AssignMouseMode logs both labels), CTRACE, GAME_ASSERTMSG,
CStaffDB's destructor and operator delete (the label of each node and of the object).

What is compared: the answer, the log, the mouse after the call (none, the old one, a new one) and the two mouse words, that the world, the
table, the heads and the thing blocks are as they were (the getters also the halfword they read, which the assert fake may rewrite), and for the
destructors the list's count and ends and that the reader's magic and its handler's chunk id are as they were.

No fused operations and no floats. Not tested: a NULL used head (the original reads the word at address 4), a used entry with a NULL thing, a
row index of 0x2800 or more in the getters, a NULL `this` in the destructors (the host has no such test, a delete-expression does not make one).
"""
import ctypes as C
import itertools
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_WorldSave"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
SAVE, SV_WS, SV_TM, GET_STAFF, GET_MECH = 0x1013BF18, 0x1014182C, 0x10141980, 0x1013E99C, 0x1013EA24
MACRO_DTOR, ATTR_DTOR, PTR_DTOR, THING_CTOR = 0x1013A13C, 0x1013E6B0, 0x1013E7F0, 0x1013DCE0
SIZES = {SAVE: 2740, SV_WS: 260, SV_TM: 260, GET_STAFF: 96, GET_MECH: 96, MACRO_DTOR: 96, ATTR_DTOR: 112, PTR_DTOR: 104, THING_CTOR: 4}
OWN = set(SIZES)
# run as the original as well (not translated here): the int, unsigned int, long, unsigned long and thing id struct SAVEVAR instances, the
# list's destructor (0x10141320) and RemoveAll (0x101413e0)
REAL_EXTRA = {0x1000F4B0, 0x100F45BC, 0x1000F384, 0x1000F5DC, 0x1000E9D4, 0x10141320, 0x101413E0}

# ---- the callees faked on both sides (the asm's bl targets that are not OWN)
ASSERT, WRITELOG, CTRACE = 0x1000DDE8, 0x1015B3DC, 0x1006FCBC
CMM_CTOR, ASSIGN, STAFF_DTOR = 0x1007EA78, 0x1007E2E0, 0x10128F68
# the Serialise functions Save calls, all stubs here (the last eight are not translated yet)
SER = {"CBankAccount": 0x100F2D10, "CChallengeHQ": 0x100F6FE0, "CControlManager": 0x100F9ED0, "CEntertainer": 0x100FB4A8,
       "CGuard": 0x100FDF94, "CHandyman": 0x100FF4C0, "CMacroAI": 0x10100890, "CMap": 0x10100E80, "CMechanic": 0x101045E8,
       "CParkAnalyser": 0x100E3004, "CResearcher": 0x101210E4, "CWeather": 0x10137F9C,
       "CPerson": 0x101170B4, "CObject": 0x10106570, "CStaffHQ": 0x1012AC08, "CMechanicHQ": 0x10105348, "CTagSystem": 0x1012C64C,
       "CResearchLab": 0x10121CE8, "CUIMsgReceiver": 0x10130928, "CAdvisor": 0x1000A468}
# the thing model (the kind byte of the thing) and the class the original saves it with
CLASS_OF_KIND = {1: "CPerson", 3: "CObject", 4: "CMechanic", 5: "CHandyman", 6: "CEntertainer", 7: "CGuard", 8: "CResearcher",
                 9: "CStaffHQ", 10: "CMechanicHQ", 11: "CAdvisor", 12: "CTagSystem", 13: "CParkAnalyser", 14: "CResearchLab",
                 15: "CWeather", 16: "CBankAccount", 17: "CUIMsgReceiver", 19: "CChallengeHQ"}
HANDLED = sorted(CLASS_OF_KIND)

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_wsv_addr(name)
GSAVELOAD = 0x1037FEFC
GMOUSE, GMOUSE_W0, GMOUSE_W1 = 0x102DC1F4, 0x102DC1E0, 0x102DC1E4
TABLE, ROWS, ROW_BYTES = 0x1034CBFC, 0x2800, 0x14
USED_HEAD, FREE_HEAD = 0x1034CBF4, 0x1034CBF8
WORLD_SIZE, BLOCK = 0x1DA748, 0x80
FILE_HANDLE = 0x6E100000
O_STAFF, O_MECH = 0x1DA718, 0x1DA71A
SENTINELS = (0x11223344, 0x55667788)
# labels in the logs: the old mouse, gSaveLoad, the file, the destructor object, its nodes (in list order), the thing blocks
LABEL_OLDMOUSE, LABEL_SAVELOAD, LABEL_FILE, LABEL_BLOCK, LABEL_OBJECT, LABEL_NODE = 4, 6, 8, 10, 19, 20
MAX_NODES = 8

# the fields of the world's tail (offset, width): the 25 fields Save writes and the two it does not (+0x1da71c, +0x1da736)
TAIL = ([(0x1DA708, 4), (0x1DA70C, 4), (0x1DA710, 4), (0x1DA714, 4)] + [(0x1DA718 + 2 * i, 2) for i in range(16)] + [(0x1DA738, 4)] +
        [(0x1DA73C + 2 * i, 2) for i in range(6)])
# ---- the host's offsets and sizes the module relies on, checked once against tpw_wsv_layout (catches a header that moved)
HOST_LAYOUT = ([WORLD_SIZE] + [off for off, width in TAIL[:4]] + [off for off, width in TAIL[4:20]] + [0x1DA738] +
               [off for off, width in TAIL[21:]] + [0x2D8, 0x1D943C, 2, ROWS])

FILL_WORLD = b"\xa5" * WORLD_SIZE
FILL_BLOCK = b"\xa5" * BLOCK
DEFAULT_TABLE = b"".join(struct.pack(">5I", 0, i, 0, 0, 0) for i in range(ROWS))

WORLD_OPS = ("save", "staff", "mech")
OBJECT_OPS = ("macro", "attr", "plist")
DTOR_ADDR = {"macro": MACRO_DTOR, "attr": ATTR_DTOR, "plist": PTR_DTOR}
OBJ_WHAT = {"macro": 0, "attr": 1, "plist": 2}


def s32(v):
    return T.s32(v)


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


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


def count_args(fmt):
    n, i = 0, 0
    while i < len(fmt):
        if fmt[i] == "%":
            if i + 1 < len(fmt) and fmt[i + 1] == "%":
                i += 2
                continue
            n += 1
        i += 1
    return n


def stub_ctrace(e):
    fmt = e.cstr(e.r(4))
    n = min(count_args(fmt), 3)
    e.cl.rec("CTRACE", [e.r(3) & MASK] + [e.r(5 + i) & MASK for i in range(n)], [fmt[:159]])


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4))[:159],))
    if e.ws_twist is not None:       # the case's rewrite of the world halfword the getters read (the original read it before the call)
        e.write(e.ws_twist[0], struct.pack(">H", e.ws_twist[1]))


def lb_write(e):
    L, length = e.cl, e.r(6)
    L.rec("LbFile_Write", (L.lab(e.r(4)), length), (e.read(e.r(5), length).hex()[:150] if length else "",))
    forced = L.ret("LbFile_Write:xfer", -1)
    e.write(e.r(7), ((forced if forced >= 0 else length) & MASK).to_bytes(4, "big"))
    e.write(e.r(3), (L.ret("LbFile_Write") & MASK).to_bytes(4, "big"))


def lb_pos(e):
    e.cl.rec("LbFile_GetPosition", (e.cl.lab(e.r(3)),))
    e.set_r(3, T.u32(e.cl.ret("LbFile_GetPosition")))


def ser_stub(name):
    def stub(e):
        L = e.cl
        L.rec(name, (where(e, e.r(3)), L.lab(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & MASK))
        e.set_r(3, T.u32(L.ret(name)))
    return stub


def op_new(e):
    """operator new: the case's queue "new" answers (0 for NULL); the log holds which of the two sizes it was (the Mac's mouse is 1 byte and
    its default mode 8, the host's differ, so both sides log 1 for the mouse and 2 for the mode)."""
    L, size = e.cl, e.r(3)
    L.rec("new", (1 if size == 1 else (2 if size == 8 else 0),))
    e.set_r(3, e.alloc(size, b"\xa5" * size) if L.ret("new", 1) != 0 else 0)


def op_delete(e):
    L, p = e.cl, e.r(3)
    if p and L.lab(p) > 0:
        L.rec("delete", (L.lab(p),))
        L.forget(p)


def staff_dtor(e):
    """CStaffDB::~CStaffDB: the macro layer's destructor passes a flag that is not positive (no delete there); the host logs a constant 0."""
    e.cl.rec("CStaffDB::~CStaffDB", (e.cl.lab(e.r(3)), 1 if s16(e.r(4)) > 0 else 0))


STUBS.internal(ASSERT, stub_assert)
STUBS.internal(WRITELOG, R("CSaveLoad::WriteSerialiseLog", "Lisi"))
STUBS.internal(CTRACE, stub_ctrace)
STUBS.internal(CMM_CTOR, R("CMMDefault::CMMDefault", "N"))
STUBS.internal(ASSIGN, R("CMouse::AssignMouseMode", "LL"))
STUBS.internal(STAFF_DTOR, staff_dtor)
for _name, _addr in SER.items():
    STUBS.internal(_addr, ser_stub(_name + "::Serialise"))
STUBS.imp("LbFile_Write__FPvPCvUlPUl", lb_write)
STUBS.imp("LbFile_GetPosition__FPv", lb_pos)
STUBS.imp("__nw__FUl", op_new)
STUBS.imp("__dl__FPv", op_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)       # the C library's memset, memcpy, strlen ... (tpwtest.py 491 on)


# ---------------------------------------------------------------------------------------------------------------- the two sides


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
        op = case["op"]
        fixed = {GSAVELOAD: LABEL_SAVELOAD, FILE_HANDLE: LABEL_FILE}
        self.world = self.oldmouse = self.obj = None
        self.blocks, self.nodes = [], []
        if op in WORLD_OPS:
            self.world = e.alloc(WORLD_SIZE, FILL_WORLD)
            self.oldmouse = e.alloc(8, b"\xa5" * 8)
            fixed[self.oldmouse] = LABEL_OLDMOUSE
        if op in WORLD_OPS or op == "ctor":
            for k in range(len(case["blocks"])):
                a = e.alloc(BLOCK, FILL_BLOCK)
                self.blocks.append(a)
                fixed[a] = LABEL_BLOCK + k
        if op in OBJECT_OPS:
            self.make_object(case, fixed)
        e.ws_world = self.world
        e.ws_twist = None
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def make_object(self, case, fixed):
        e, n, w = self.e, case["nodes"], case["words"]
        self.nodes = [e.alloc(12, b"\xa5" * 12) for _ in range(n)]
        for k, a in enumerate(self.nodes):
            e.write(a, struct.pack(">3I", 0x1000 + 16 * k, self.nodes[k + 1] if k + 1 < n else 0, self.nodes[k - 1] if k else 0))
            fixed[a] = LABEL_NODE + k
        head, tail = (self.nodes[0], self.nodes[-1]) if n else (0, 0)
        if case["op"] == "macro":
            self.obj = e.alloc(0x2A0, b"\xa5" * 0x2A0)
        elif case["op"] == "attr":      # list part, the reader's magic at +0x10, the handler member at +0x14 (vtable, chunk id, map)
            self.obj = e.alloc(0x20, struct.pack(">8I", w[0], head, tail, n, w[1], w[2], w[3], w[4]))
            self.kept0 = (w[1], w[3])
        else:
            self.obj = e.alloc(0x10, struct.pack(">4I", w[0], head, tail, n))
        fixed[self.obj] = LABEL_OBJECT

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
        return T.call_ppc(self.e, addr, words, max_insns=50_000_000, timeout_us=300_000_000)[0] & MASK

    # ---- the world
    def thing(self, k, ident, kind, dead):
        self.put(self.blocks[k], 2, ident)
        self.put(self.blocks[k] + 2, 1, kind)
        self.put(self.blocks[k] + 3, 1, dead)

    def table_default(self):
        self.e.write(TABLE, DEFAULT_TABLE)
        self.heads(0, 0)

    def row_addr(self, p1):
        return TABLE + (p1 - 1) * ROW_BYTES if p1 else 0

    def row(self, r, blockp1, index, nextp1, prevp1, freep1):
        self.e.write(TABLE + r * ROW_BYTES, struct.pack(">5I", self.blocks[blockp1 - 1] if blockp1 else 0, index & MASK,
                                                         self.row_addr(nextp1), self.row_addr(prevp1), self.row_addr(freep1)))

    def heads(self, usedp1, freep1):
        self.e.write(USED_HEAD, struct.pack(">I", self.row_addr(usedp1)))
        self.e.write(FREE_HEAD, struct.pack(">I", self.row_addr(freep1)))

    def mouse(self, which):
        self.put(GMOUSE, 4, self.oldmouse if which else 0)
        self.put(GMOUSE_W0, 4, SENTINELS[0])
        self.put(GMOUSE_W1, 4, SENTINELS[1])

    def twist(self, offset, value):
        self.e.ws_twist = (self.world + offset, value) if offset >= 0 else None

    def take(self):
        e = self.e
        return (e.read(self.world, WORLD_SIZE), e.read(TABLE, ROWS * ROW_BYTES), [e.read(a, BLOCK) for a in self.blocks],
                e.read(USED_HEAD, 8))

    def snapshot(self):
        self.snap = self.take()

    def unchanged(self):
        return int(self.snap == self.take())

    def get_mouse(self):
        m = self.get(GMOUSE, 4)
        return 0 if m == 0 else (1 if m == self.oldmouse else 2)

    def get_words(self):
        return (self.get(GMOUSE_W0, 4), self.get(GMOUSE_W1, 4))

    def blocknum(self, p):
        return 0 if p == 0 else (self.blocks.index(p) + 1 if p in self.blocks else -1)

    # ---- the calls
    def save(self):
        return s32(self.call(SAVE, [self.world, FILE_HANDLE]))

    def savevar(self, which, value, name, nolog):
        cell = self.e.alloc(4, struct.pack(">I", value & MASK))
        text = self.e.alloc(len(name) + 1, name.encode("latin-1") + b"\0")
        answer = s32(self.call(SV_WS if which == 0 else SV_TM, [FILE_HANDLE, cell, text, nolog & MASK]))
        return answer, struct.unpack(">I", self.e.read(cell, 4))[0]

    def get_hq(self, which):
        return self.blocknum(self.call(GET_STAFF if which == 0 else GET_MECH, [self.world]))

    def thing_ctor(self):
        arg = self.e.alloc(16, b"\xa5" * 16)
        return int(self.call(THING_CTOR, [self.blocks[0], arg]) == self.blocks[0])

    def destroy(self, op, flag):
        self.call(DTOR_ADDR[op], [self.obj, flag & MASK])

    def object_state(self, op):
        if op == "macro":
            return None, None
        lst = (self.get(self.obj + 0xC, 4), int(self.get(self.obj + 4, 4) == 0), int(self.get(self.obj + 8, 4) == 0))
        kept = (int(self.get(self.obj + 0x10, 4) == self.kept0[0]), int(self.get(self.obj + 0x18, 4) == self.kept0[1])) if op == "attr" else None
        return lst, kept


class Host(Side):
    """The same on the host library."""
    byte = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_wsv_reset()
        op = case["op"]
        fixed = {LABEL_SAVELOAD: lib.tpw_wsv_addr(b"gSaveLoad"), LABEL_FILE: lib.tpw_wsv_addr(b"file"),
                 LABEL_OLDMOUSE: lib.tpw_wsv_addr(b"oldmouse")}
        self.world = lib.tpw_wsv_world()
        self.blocks = []
        if op in WORLD_OPS or op == "ctor":
            for k in range(len(case["blocks"])):
                self.blocks.append(lib.tpw_wsv_block(k))
                fixed[LABEL_BLOCK + k] = self.blocks[k]
        if op in OBJECT_OPS:
            lib.tpw_wsv_build(OBJ_WHAT[op], case["nodes"])
            fixed[LABEL_OBJECT] = lib.tpw_wsv_addr(b"object")
            for k in range(case["nodes"]):
                fixed[LABEL_NODE + k] = lib.tpw_wsv_node(k)
            self.state0 = self.state()
        CL.host_begin(lib, fixed, case["queues"])

    def state(self):
        out = (C.c_uint32 * 5)()
        self.lib.tpw_wsv_state(out)
        return tuple(out)

    def done(self):
        pass

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    # ---- the world
    def thing(self, k, ident, kind, dead):
        self.put(self.blocks[k], 2, ident)
        self.put(self.blocks[k] + 2, 1, kind)
        self.put(self.blocks[k] + 3, 1, dead)

    def table_default(self):
        pass                                   # tpw_wsv_reset made it

    def row(self, r, blockp1, index, nextp1, prevp1, freep1):
        self.lib.tpw_wsv_row(r, blockp1, index & MASK, nextp1, prevp1, freep1)

    def heads(self, usedp1, freep1):
        self.lib.tpw_wsv_heads(usedp1, freep1)

    def mouse(self, which):
        self.lib.tpw_wsv_mouse(which)

    def twist(self, offset, value):
        self.lib.tpw_wsv_twist(offset, value)

    def snapshot(self):
        self.lib.tpw_wsv_snapshot()

    def unchanged(self):
        return self.lib.tpw_wsv_unchanged()

    def get_mouse(self):
        return self.lib.tpw_wsv_get_mouse()

    def get_words(self):
        out = (C.c_uint32 * 2)()
        self.lib.tpw_wsv_get_words(out)
        return tuple(out)

    # ---- the calls
    def save(self):
        return self.lib.tpw_wsv_save()

    def savevar(self, which, value, name, nolog):
        cell = C.c_uint32(value & MASK)
        answer = self.lib.tpw_wsv_savevar(which, C.byref(cell), name.encode("latin-1"), s32(nolog))
        return answer, cell.value

    def get_hq(self, which):
        return self.lib.tpw_wsv_get_hq(which)

    def thing_ctor(self):
        return int(self.lib.tpw_wsv_thing_ctor(0) == 1)

    def destroy(self, op, flag):
        self.lib.tpw_wsv_destroy(OBJ_WHAT[op], 1 if s16(flag) > 0 else 0)

    def object_state(self, op):
        if op == "macro":
            return None, None
        now = self.state()
        lst = (now[0], now[1], now[2])
        kept = (int(now[3] == self.state0[3]), int(now[4] == self.state0[4])) if op == "attr" else None
        return lst, kept


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return

    def fn(name, restype, argtypes):
        f = getattr(lib, "tpw_wsv_" + name)
        f.restype, f.argtypes = restype, argtypes

    fn("addr", C.c_uint64, [C.c_char_p])
    fn("layout", C.c_int32, [C.POINTER(C.c_int64)])
    fn("world", C.c_uint64, [])
    fn("block", C.c_uint64, [C.c_int32])
    fn("row", None, [C.c_int32, C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.c_int32])
    fn("reset", None, [])
    fn("heads", None, [C.c_int32, C.c_int32])
    fn("mouse", None, [C.c_int32])
    fn("get_mouse", C.c_int32, [])
    fn("get_words", None, [C.POINTER(C.c_uint32)])
    fn("snapshot", None, [])
    fn("unchanged", C.c_int32, [])
    fn("save", C.c_int32, [])
    fn("savevar", C.c_int32, [C.c_int32, C.POINTER(C.c_uint32), C.c_char_p, C.c_int32])
    fn("get_hq", C.c_int64, [C.c_int32])
    fn("twist", None, [C.c_int32, C.c_uint32])
    fn("thing_ctor", C.c_int64, [C.c_int32])
    fn("build", None, [C.c_int32, C.c_int32])
    fn("node", C.c_uint64, [C.c_int32])
    fn("destroy", None, [C.c_int32, C.c_int32])
    fn("state", None, [C.POINTER(C.c_uint32)])
    out = (C.c_int64 * 40)()
    n = lib.tpw_wsv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    """Writes the case's globals and blocks through Side.put: the same code for both memories (offsets are the Mac's)."""
    op = c["op"]
    if op in WORLD_OPS:
        for (off, width), v in zip(TAIL, c["tail"]):
            s.put(s.world + off, width, v)
        for k, b in enumerate(c["blocks"]):
            s.thing(k, b["id"], b["kind"], b["dead"])
        s.table_default()
        if op == "save":
            used = c["used"]
            for i, u in enumerate(used):
                s.row(u["row"], u["block"] + 1, u["index"], used[i + 1]["row"] + 1 if i + 1 < len(used) else 0, u["prev"], u["free"])
            s.heads(used[0]["row"] + 1, c["freehead"])
            s.mouse(c["mouse"])
        else:
            for row, blockp1 in c["rows"].items():
                s.row(row, blockp1, row, 0, 0, 0)
            s.twist(*c["twist"])
    elif op == "ctor":
        for i in range(BLOCK):
            s.put(s.blocks[0] + i, 1, c["fill"][i])


def execute(s, c):
    """The call."""
    op = c["op"]
    if op in WORLD_OPS or op == "ctor":
        s.snapshot() if op in WORLD_OPS else None
    if op == "save":
        return s.save()
    if op == "savevar":
        return s.savevar(c["which"], c["value"], c["name"], c["nolog"])
    if op == "staff":
        return s.get_hq(0)
    if op == "mech":
        return s.get_hq(1)
    if op == "ctor":
        return s.thing_ctor()
    s.destroy(op, c["flag"])
    return None


def observe(s, c, answer):
    """Everything the call may change, read back the same way on both sides; pointers as labels or block numbers."""
    op = c["op"]
    out = {"answer": answer}
    if op in WORLD_OPS:
        out["unchanged"] = s.unchanged()
    if op == "save":
        out["mouse"] = s.get_mouse()
        out["words"] = s.get_words()
    if op in ("staff", "mech"):
        out["half"] = s.get(s.world + (O_STAFF if op == "staff" else O_MECH), 2)
    if op == "ctor":
        out["bytes"] = bytes(s.read(s.blocks[0], BLOCK))
    if op in OBJECT_OPS:
        # The memory of a deleted object is not read: the host compiler drops the destructor's stores to an object that operator delete
        # follows at once, so only the destructor-alone path shows the emptied list. The log shows the nodes' and the object's deletes.
        out["list"], out["kept"] = s.object_state(op) if s16(c["flag"]) <= 0 else (None, None)
    out["log"] = s.log()
    return out


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, execute(s, c))
    finally:
        s.done()


def ppc(e, c):
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)


# ---------------------------------------------------------------------------------------------------------------- case generators

ANSWERS = [1, -1, 0x10000, -0x80000000, 2, 0x7FFFFFFF]
KINDS_ALL = list(range(0x15)) + [0x7F, 0x80, 0xFF]
KINDS = HANDLED * 3 + [0, 2, 18, 20, 0x7F, 0x80, 0xFF]


def positions(r):
    """The 11 answers of LbFile_GetPosition (no signed overflow in the differences the traces print)."""
    k = r.random()
    if k < 0.15:
        return [0] * 11
    if k < 0.45:
        base = r.randrange(0, 1000)
        return [base + 7 * i * i for i in range(11)]
    if k < 0.7:
        return [r.randrange(0, 0x7FFFFFFF) for _ in range(11)]
    return [r.choice([-1, r.randrange(0, 0x40000000)]) for _ in range(11)]


def tail_values(r):
    """The tail's fields: distinct random values (now and then a special one), so a field read from the wrong place shows."""
    out = []
    for off, width in TAIL:
        v = r.getrandbits(8 * width)
        if r.random() < 0.12:
            v = r.choice([0, 1, (1 << (8 * width)) - 1, 1 << (8 * width - 1)])
        out.append(v)
    return out


def save_case(r, kinds=None, mouse=None, queues=None, index=None):
    if kinds is None:
        kinds = [r.choice(KINDS) for _ in range(r.choice([1, 1, 2, 3, 4, 5, 8]))]
    n = len(kinds)
    rows = r.sample(range(1, ROWS), n)
    blocks = [{"id": r.getrandbits(16), "kind": kinds[i], "dead": r.choice([0, 0, 1, 0xFF])} for i in range(n)]
    used = []
    for i in range(n):
        idx = r.choice([rows[i], rows[i], r.getrandbits(32), 0, 0xFFFFFFFF, 1 << 31]) if index is None else index
        used.append({"row": rows[i], "block": i, "index": idx, "prev": r.choice([0, r.randrange(1, ROWS + 1)]),
                     "free": r.choice([0, r.randrange(1, ROWS + 1)])})
    q = {"LbFile_GetPosition": positions(r)}
    if queues:
        q.update(queues)
    return {"op": "save", "tail": tail_values(r), "blocks": blocks, "used": used, "freehead": r.randrange(0, ROWS + 1),
            "mouse": r.choice([0, 0, 1]) if mouse is None else mouse, "queues": q}


def inject(r, c):
    """Now and then a failure at a random place: a write, a short write, a Serialise answer, an allocation."""
    q, n = c["queues"], len(c["blocks"])
    k = r.random()
    if k < 0.12:
        step = r.randrange(27 + 2 * n)
        q["LbFile_Write"] = [0] * step + [r.choice([-1, 1, 2, 0x7FFFFFFF])]
    elif k < 0.20:
        step = r.randrange(27 + 2 * n)
        q["LbFile_Write:xfer"] = [-1] * step + [r.choice([0, 1, 2, 3, 5, 0xFFFFFFFF])]
    elif k < 0.42:
        names = ["CControlManager", "CMacroAI", "CMap"]
        names += [CLASS_OF_KIND[b["kind"]] for b in c["blocks"] if b["kind"] in CLASS_OF_KIND]
        name = r.choice(names)
        before = 0
        if name in ("CControlManager", "CMacroAI", "CMap"):
            before = 0
        else:
            hits = [i for i, b in enumerate(c["blocks"]) if CLASS_OF_KIND.get(b["kind"]) == name]
            before = r.randrange(len(hits))
        q[name + "::Serialise"] = [0] * before + [r.choice(ANSWERS)]
    elif k < 0.55:
        q["new"] = [r.choice((0, 1)) for _ in range(3)]
    return c


def save_edges():
    """Every branch of the asm at least once: each kind alone, all handled kinds, each answer of each callee, each write step, the mouse."""
    r = random.Random(205101)
    for kind in KINDS_ALL:
        yield save_case(r, kinds=[kind], mouse=0)
    yield save_case(r, kinds=list(HANDLED), mouse=1)
    yield save_case(r, kinds=list(reversed(HANDLED)), mouse=0)
    yield save_case(r, kinds=[18, 1, 0, 3, 2, 4, 0x14, 5, 0xFF, 6], mouse=1)
    yield save_case(r, kinds=[16], mouse=1, index=0)
    yield save_case(r, kinds=[16, 16], mouse=1, index=0xFFFFFFFF)
    # a Serialise function of the things answering nonzero, at its first and at its second call
    for kind in HANDLED:
        for v in (1, -1, 0x10000, -0x80000000):
            name = CLASS_OF_KIND[kind] + "::Serialise"
            yield save_case(r, kinds=[kind, kind], queues={name: [v]})
            yield save_case(r, kinds=[kind, kind], queues={name: [0, v]})
    # the three parts of the world answering nonzero
    for name in ("CControlManager", "CMacroAI", "CMap"):
        for v in ANSWERS:
            yield save_case(r, kinds=[1, 3], queues={name + "::Serialise": [v]})
    # a write failing, ending short or long at every step (the version, the 25 fields, the head, then two writes per thing)
    for step in range(27 + 2 * 2):
        for fail in (-1, 1):
            yield save_case(r, kinds=[1, 12], queues={"LbFile_Write": [0] * step + [fail]})
        for x in (0, 1, 3, 5):
            yield save_case(r, kinds=[1, 12], queues={"LbFile_Write:xfer": [-1] * step + [x]})
    # the mouse: none or an old one, every combination of the three allocations answering or not
    for mouse in (0, 1):
        for combo in itertools.product((0, 1), repeat=3):
            yield save_case(r, kinds=[1], mouse=mouse, queues={"new": list(combo)})
    # the positions the traces print
    yield save_case(r, kinds=[1], queues={"LbFile_GetPosition": [0] * 11})
    yield save_case(r, kinds=[1], queues={"LbFile_GetPosition": [-1] * 11})
    yield save_case(r, kinds=[1], queues={"LbFile_GetPosition": [0x7FFFFFFF, 0] * 5 + [7]})
    yield save_case(r, kinds=[1], queues={"LbFile_GetPosition": [0, 0x7FFFFFFF] * 5 + [7]})


def save_random(r):
    return inject(r, save_case(r))


# ---- SAVEVAR of the two enums
NAMES = ["", "a", "mWorldState ", "thingmodel", "x" * 30, "tab\tname"]
VALUES = [0, 1, 2, 3, 0x13, 0x14, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x12345678, 0xDEADBEEF]


def sv_case(r, which, value=None, name=None, nolog=None, queues=None):
    q = {}
    if queues is not None:
        q.update(queues)
    elif r.random() < 0.3:
        k = r.random()
        if k < 0.4:
            q["LbFile_Write"] = [r.choice([-1, 1, 2])]
        else:
            q["LbFile_Write:xfer"] = [r.choice([0, 1, 2, 3, 5, 0xFFFFFFFF])]
    return {"op": "savevar", "which": which, "value": r.choice(VALUES + [r.getrandbits(32)]) if value is None else value,
            "name": r.choice(NAMES) if name is None else name, "nolog": r.choice([0, 0, 0, 1, 2, -1, 0x80000000]) if nolog is None else nolog,
            "queues": q}


def sv_edges(which):
    r = random.Random(205201 + which)
    for v in VALUES:
        yield sv_case(r, which, value=v, name="thingmodel", nolog=0, queues={})
    for name in NAMES:
        yield sv_case(r, which, name=name, nolog=0, queues={})
    for nolog in (0, 1, 2, -1, 0x80000000, 0x7FFFFFFF):
        yield sv_case(r, which, value=0x1234, nolog=nolog, queues={})
    for res in (-1, 1, 2, 0x7FFFFFFF):
        yield sv_case(r, which, value=5, name="mWorldState ", nolog=0, queues={"LbFile_Write": [res]})
    for x in (0, 1, 3, 5, 0xFFFFFFFF):
        yield sv_case(r, which, value=5, name="mWorldState ", nolog=0, queues={"LbFile_Write:xfer": [x]})


def sv_random(r, which):
    return sv_case(r, which)


# ---- the headquarters getters
def get_case(r, op, index=None, row_thing=None, row0=None, twist=None):
    """Three blocks: the answer's, the one in row 0 and the one in the twisted row. The world holds random bytes in its tail."""
    index = r.choice([0, 1, 2, 0x27FF, 0x27FE, r.randrange(ROWS), r.randrange(ROWS)]) if index is None else index
    other = r.randrange(ROWS)
    while other == index:
        other = r.randrange(ROWS)
    row_thing = r.choice([0, 1, 1]) if row_thing is None else row_thing
    row0 = r.choice([0, 2]) if row0 is None else row0
    twist = r.choice([False, False, True]) if twist is None else twist
    rows = {}
    if row0 and index != 0:
        rows[0] = row0
    rows[index] = row_thing
    if twist and other != 0:
        rows[other] = 3
    tail = tail_values(r)
    half = O_STAFF if op == "staff" else O_MECH
    tail[[off for off, width in TAIL].index(half)] = index
    return {"op": op, "tail": tail, "blocks": [{"id": r.getrandbits(16), "kind": r.randrange(256), "dead": r.randrange(2)} for _ in range(3)],
            "rows": rows, "twist": (half, other) if twist else (-1, 0), "queues": {}}


def get_edges(op):
    r = random.Random(205301 + (op == "mech"))
    for index in (0, 1, 0x27FF, 777):
        for row_thing in (0, 1):
            for row0 in (0, 2):
                for twist in (False, True):
                    yield get_case(r, op, index=index, row_thing=row_thing, row0=row0, twist=twist)


def get_random(r, op):
    return get_case(r, op)


# ---- the destructors
FLAGS = [-1, 0, 1, 2, 3, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18000, 0xFFFF8000, 0x7FFF0000, 0x80000000]


def dtor_case(r, op, nodes=None, flag=None):
    n = r.choice([0, 1, 2, 3, 4, 5]) if nodes is None else nodes
    return {"op": op, "nodes": n if op != "macro" else 0, "flag": r.choice(FLAGS + [r.getrandbits(32)]) if flag is None else flag,
            "words": [r.getrandbits(32) for _ in range(5)], "queues": {}}


def dtor_edges(op):
    r = random.Random(205401 + len(op))
    for nodes in ((0,) if op == "macro" else (0, 1, 2, 3, 5)):
        for flag in FLAGS:
            yield dtor_case(r, op, nodes=nodes, flag=flag)


def dtor_random(r, op):
    return dtor_case(r, op)


# ---- CThing's serialise constructor
def ctor_case(r):
    return {"op": "ctor", "blocks": [{}], "fill": r.randbytes(BLOCK), "queues": {}}


def ctor_edges():
    r = random.Random(205501)
    for fill in (b"\xa5" * BLOCK, bytes(BLOCK), b"\xff" * BLOCK, bytes(range(BLOCK))):
        c = ctor_case(r)
        c["fill"] = fill
        yield c


def ctor_random(r):
    return ctor_case(r)


TESTS = [
    FnTest("CWorld::Save", ppc, host, edge=save_edges, rnd=save_random),
    FnTest("SAVEVAR<CWorld::WorldState>", ppc, host, edge=lambda: sv_edges(0), rnd=lambda r: sv_random(r, 0)),
    FnTest("SAVEVAR<CThing::ThingModel>", ppc, host, edge=lambda: sv_edges(1), rnd=lambda r: sv_random(r, 1)),
    FnTest("CWorld::GetStaffHQ", ppc, host, edge=lambda: get_edges("staff"), rnd=lambda r: get_random(r, "staff")),
    FnTest("CWorld::GetMechanicHQ", ppc, host, edge=lambda: get_edges("mech"), rnd=lambda r: get_random(r, "mech")),
    FnTest("CMacroAI::~CMacroAI", ppc, host, edge=lambda: dtor_edges("macro"), rnd=lambda r: dtor_random(r, "macro")),
    FnTest("CMapAttribFileReader::~CMapAttribFileReader", ppc, host, edge=lambda: dtor_edges("attr"), rnd=lambda r: dtor_random(r, "attr")),
    FnTest("TbPtrList<CIndividualChunkHandler>::~TbPtrList", ppc, host, edge=lambda: dtor_edges("plist"),
           rnd=lambda r: dtor_random(r, "plist")),
    FnTest("CThing::CThing", ppc, host, edge=ctor_edges, rnd=ctor_random),
]
