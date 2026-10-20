"""Unit w163, the script interpreter's core, against libtpw_unit_ScriptInterp.dylib (tests/unit_ScriptInterp.cpp), main app
("SimTheme Park"), src/ScriptInterp.cpp:
  AssignSlot 0x100d8630, InitScript 0x100d8768, DoScript 0x100d88b4, SCR_Error 0x100d8740, SCR_Push 0x100d8cdc, SCR_Pop 0x100d8d34,
  SCR_SpawnScript 0x100d924c, SCR_SetScriptPosition 0x100d932c, SCR_GetScriptPosition 0x100d93b8, SCR_SetVariable 0x100d9514,
  SCR_SetFloatVariable 0x100d95a0, SCR_SetServiceInterval 0x100d9630, SCR_WhichScriptHasThisSprite 0x100da7e0, ESP_InitScript 0x100da008,
  ESP_SpawnScript 0x100da18c and the commands PK_GOTO 0x100da980, PK_NOP 0x100da95c, PK_END 0x100db6f0, PK_WAIT 0x100db714, PK_SETTEXT
  0x100db748, PK_SETVAR 0x100db79c, PK_PUSHVAR 0x100db48c, PK_POPVAR 0x100db508, PK_GOSUB 0x100daea0, PK_RETURN 0x100daf6c.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses
(big-endian) for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back.
The callees outside the unit are fakes on both sides that log their arguments and answer from the case's queues: CTRACE and
GAME_ASSERTMSG, the clock read, the allocator (LbMem_Alloc, LbMem_Free; the fresh blocks are filled with 0xA5 and read back), the two
sprite methods, and the 32 PK_ commands of other units (each logs the current script's label, position and state; the queue "pk" then
sets the state, moves the position or makes another script current).

The world of a case: the script system's globals, a table of 32-bit words (host: arena offsets, Mac: addresses, compared by label),
script blocks of random words (the same on both sides), the code array (883 words, the case's program at the front) with its check
table, the sprite groups and sprites. What is compared: the answer, the globals, the table (by label), every block word by word, the
blocks the allocator made, the buffers of the getters and the event log. No fused operations and no fctiwz: no rounding hooks.

Not tested: a NULL script block (AssignSlot, SCR_SpawnScript with a failed allocation: the Mac crashes), a code word that is not a
descriptor (DoScript aborts on the host, the Mac calls garbage), a negative or out-of-array position or code pointer (the Mac reads
whatever data lies there, the host answers 0), a custom data index of 4 or more (the Mac reads past its start address table), the
SCR_Error calls of GOSUB and RETURN (SCR_Error is an own empty function: nothing shows), SCR_CodeAddress of a pointer outside the array.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ScriptInterp"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FN = {
    "AssignSlot": (0x100D8630, 228), "InitScript": (0x100D8768, 280), "DoScript": (0x100D88B4, 276), "SCR_Error": (0x100D8740, 4),
    "SCR_Push": (0x100D8CDC, 56), "SCR_Pop": (0x100D8D34, 64), "SCR_SpawnScript": (0x100D924C, 180),
    "SCR_SetScriptPosition": (0x100D932C, 92), "SCR_GetScriptPosition": (0x100D93B8, 140), "SCR_SetVariable": (0x100D9514, 96),
    "SCR_SetFloatVariable": (0x100D95A0, 96), "SCR_SetServiceInterval": (0x100D9630, 80),
    "SCR_WhichScriptHasThisSprite": (0x100DA7E0, 112), "ESP_InitScript": (0x100DA008, 312), "ESP_SpawnScript": (0x100DA18C, 332),
    "PK_GOTO": (0x100DA980, 56), "PK_NOP": (0x100DA95C, 4), "PK_END": (0x100DB6F0, 4), "PK_WAIT": (0x100DB714, 20),
    "PK_SETTEXT": (0x100DB748, 48), "PK_SETVAR": (0x100DB79C, 168), "PK_PUSHVAR": (0x100DB48C, 88), "PK_POPVAR": (0x100DB508, 96),
    "PK_GOSUB": (0x100DAEA0, 172), "PK_RETURN": (0x100DAF6C, 108),
}
OWN = {a for a, _ in FN.values()}

# ---- the callees faked on both sides
CTRACE, GAME_ASSERTMSG, CLOCK_READ = 0x1006FCBC, 0x1000DDE8, 0x101472F4
GET_FRAMES, GET_CUSTOM = 0x1007B1F8, 0x1007B23C
# the 32 commands of other units, in descriptor order (index of the descriptor: (word - 0x10266958) / 8)
DESC = ["GOTO", "IF", "ENDIF", "ELSE", "GOSUB", "RETURN", "DO", "PUSHVAR", "POPVAR", "LOOP", "REPEAT", "UNTIL", "END", "WAIT", "NOP",
        "SETTEXT", "SETVAR", "ADD", "SUB", "AND", "MUL", "MULVAR", "DIV", "DIVVAR", "MOD", "MODVAR", "ADDVAR", "COPYVAR", "RANDOM",
        "SPAWN", "SET_OBJ_ID", "SET_OBJ_ACT", "PutSpriteVAR", "PutSpriteIM", "Put3DSpriteIM", "RemoveSprite", "ResetScale",
        "ReduceScale", "IncreaseScale", "ResetRotation", "ReduceRotation", "IncreaseRotation"]
DESC_FN = [0x100DA980, 0x100DA9D8, 0x100DADC8, 0x100DAE38, 0x100DAEA0, 0x100DAF6C, 0x100DB588, 0x100DB48C, 0x100DB508, 0x100DB620,
           0x100DAFF8, 0x100DB060, 0x100DB6F0, 0x100DB714, 0x100DA95C, 0x100DB748, 0x100DB79C, 0x100DB864, 0x100DB93C, 0x100DBDC0,
           0x100DBA14, 0x100DBAEC, 0x100DBBA0, 0x100DBC80, 0x100DBD38, 0x100DBE5C, 0x100DBEEC, 0x100DBF9C, 0x100DC01C, 0x100DC0E0,
           0x100DC160, 0x100DC1B8, 0x100DC214, 0x100DC2D0, 0x100DC384, 0x100DC418, 0x100DC45C, 0x100DC494, 0x100DC4D8, 0x100DC51C,
           0x100DC558, 0x100DC59C]
OWN_DESC = {n for n, a in zip(DESC, DESC_FN) if a in OWN}
FAKE_PK = [(n, a) for n, a in zip(DESC, DESC_FN) if a not in OWN]
DESC_BASE = 0x10266958
CODE = 0x1029F20C
CODE_WORDS = 883
END_WORD = DESC_BASE + 12 * 8

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_sj_addr(name)
ADDR = dict(gScriptInitialised=0x102F7C28, gScriptF7C2C=0x102F7C2C, gScriptTableSize=0x102F7C14, gScriptTable=0x102F7C18,
            gScriptCheckTable=0x102F7C10, gScriptCurrent=0x102F7C24, gScriptCodeBase=0x102F7C20, gScriptVars=0x102F7C1C, code=CODE)
GROUPS, SPRITES = 0x1029D368, 0x102DB520
# ---- the host's sizes and offsets the module relies on, checked once against tpw_sj_layout (catches a header that moved)
HOST_LAYOUT = [0x118, 0x84, 0xD0, 0x114]
TABLE_LABEL, SPRITE_LABEL = 50, 200

# the call signatures: C types of the arguments (i int32, u uint32, f float, p pointer, c code address) and the answer (i or v)
SIG = {
    "AssignSlot": ("p", "i"), "InitScript": ("pciuuu".replace("u", "f").replace("pcif", "pcif"), "v"), "DoScript": ("p", "i"),
    "SCR_Error": ("", "v"), "SCR_Push": ("i", "v"), "SCR_Pop": ("", "i"), "SCR_SpawnScript": ("cifff", "i"),
    "SCR_SetScriptPosition": ("iffff".replace("ffff", "fffi"), "v"), "SCR_GetScriptPosition": ("ippp" + "p", "v"),
    "SCR_SetVariable": ("iui", "v"), "SCR_SetFloatVariable": ("iuf", "v"), "SCR_SetServiceInterval": ("ii", "v"),
    "SCR_WhichScriptHasThisSprite": ("p", "i"), "ESP_InitScript": ("pciiifffii", "v"), "ESP_SpawnScript": ("ciiifff", "i"),
}
SIG["InitScript"] = ("pcifff", "v")
SIG["PK"] = ("i", "v")
PK_INDEX = {"GOTO": 0, "NOP": 1, "END": 2, "WAIT": 3, "SETTEXT": 4, "SETVAR": 5, "PUSHVAR": 6, "POPVAR": 7, "GOSUB": 8, "RETURN": 9}


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cur_state(e):
    """(address, mPos, mState) of the Mac's current script."""
    cur = e.u32(ADDR["gScriptCurrent"])
    return cur, s32(e.u32(cur + 8)), s32(e.u32(cur + 0x18))


def stub_pk(name):
    def stub(e):
        L = e.cl
        cur, pos, state = cur_state(e)
        L.rec("PK_" + name, (L.lab(cur), pos, state))
        v = L.ret("pk", 0)
        st = {1: 2, 2: 3, 3: 4}.get(v & 3)
        if st:
            e.write(cur + 0x18, struct.pack(">I", st))
        e.write(cur + 8, struct.pack(">I", (pos + ((v >> 2) & 15)) & MASK))
        if (v >> 6) & 1:
            e.write(ADDR["gScriptCurrent"], struct.pack(">I", L.switch))
    return stub


def stub_trace(e):
    e.cl.rec("CTRACE", (e.r(3),), (e.cstr(e.r(4))[:159],))


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4))[:159],))


def stub_clock(e):
    e.cl.rec("Clock")
    e.set_r(3, e.cl.ret("clock", 0) & MASK)


def stub_frames(e):
    L = e.cl
    L.rec("GetActionFrames", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("frames", 0) & 0xFF)


def stub_custom(e):
    L = e.cl
    L.rec("GetCustomData", (L.lab(e.r(3)), s32(e.r(4))))
    mask = L.ret("cdmask", 15)
    for bit, key, reg in ((1, "cd1", 5), (2, "cd2", 6), (4, "cd3", 7), (8, "cd4", 8)):
        if mask & bit:
            e.write(e.r(reg), struct.pack(">I", L.ret(key, 0) & MASK))


def stub_alloc(e):
    L = e.cl
    size = e.r(3)
    a = e.alloc(max(size, 1), b"\xa5" * max(size, 1))
    L.fresh.append((a, size))
    L.rec("LbMem_Alloc", (size, L.newlab(a)))
    e.set_r(3, a)


def stub_free(e):
    e.cl.rec("LbMem_Free", (e.cl.lab(e.r(3)),))


STUBS.internal(CTRACE, stub_trace)
STUBS.internal(GAME_ASSERTMSG, stub_assert)
STUBS.internal(CLOCK_READ, stub_clock)
STUBS.internal(GET_FRAMES, stub_frames)
STUBS.internal(GET_CUSTOM, stub_custom)
for _name, _addr in FAKE_PK:
    STUBS.internal(_addr, stub_pk(_name))
STUBS.imp("LbMem_Alloc__FUi", stub_alloc)
STUBS.imp("LbMem_Free__FPv", stub_free)


def setup(runner):
    T.install_std_stubs(runner.emu)       # memset, memcpy


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.fresh = []
        self.L.switch = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def reset(self):
        pass

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def label(self, l, a):
        self.L.labels[a] = l

    def entry_word(self, a):
        return a

    def entry_label(self, w):
        return self.L.lab(w)

    def addr_label(self, a):
        return self.L.lab(a)

    def setptr(self, name, a):
        self.write(self.g(name), struct.pack(">I", a))

    def getptr(self, name):
        return self.rd_word(self.g(name))

    def setswitch(self, a):
        self.L.switch = a

    def group(self, i, first, count):
        name = self.e.alloc(8, b"grp%d\0" % i)
        self.words(GROUPS + 12 * i, [name, first, count])

    def sprite(self, i):
        a = self.e.alloc(16)
        self.words(SPRITES + 4 * i, [a])
        return a

    def fresh(self):
        return list(self.L.fresh)

    def log(self):
        return self.L.log

    def call(self, name, kinds, args):
        a = []
        for k, v in zip(kinds, args):
            a.append(T.F(bitsf(v)) if k == "f" else v & MASK)
        if name.startswith("PK_"):
            addr = FN[name][0]
        else:
            addr = FN[name][0]
        r3, _, _ = T.call_ppc(self.e, addr, a)
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_sj_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    @property
    def base(self):
        return self.lib.tpw_sj_base()

    def reset(self):
        pass

    def g(self, name):
        return self.lib.tpw_sj_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sj_alloc(max(n, 4))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def label(self, l, a):
        self.lib.tpw_cl_label(l, a)

    def entry_word(self, a):
        return (a - self.base) & MASK if a else 0

    def entry_label(self, w):
        return self.lib.tpw_cl_lab(self.base + w) if w else 0

    def addr_label(self, a):
        return self.lib.tpw_cl_lab(a) if a else 0

    def setptr(self, name, a):
        self.write(self.g(name), struct.pack("<Q", a))

    def getptr(self, name):
        return struct.unpack("<Q", self.read(self.g(name), 8))[0]

    def setswitch(self, a):
        self.setptr("switch", a)

    def group(self, i, first, count):
        self.lib.tpw_sj_group(i, first, count)

    def sprite(self, i):
        return self.lib.tpw_sj_sprite(i)

    def fresh(self):
        return [(self.lib.tpw_sj_fresh(i), self.lib.tpw_sj_fresh_size(i)) for i in range(self.lib.tpw_sj_nfresh())]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, kinds, args):
        if name.startswith("PK_"):
            return self.lib.tpw_sj_PK(PK_INDEX[name[3:]])
        f = getattr(self.lib, "tpw_sj_" + name)
        r = f(*[(bitsf(v) if k == "f" else (v & MASK if k in "uc" else (s32(v) if k == "i" else v))) for k, v in zip(kinds, args)])
        return None if r is None else r & MASK


CT = {"i": C.c_int32, "u": C.c_uint32, "c": C.c_uint32, "f": C.c_float, "p": C.c_uint64}


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_sj_addr.restype, lib.tpw_sj_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sj_alloc.restype, lib.tpw_sj_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sj_base.restype = C.c_uint64
    lib.tpw_sj_nfresh.restype = C.c_int32
    lib.tpw_sj_fresh.restype, lib.tpw_sj_fresh.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_sj_fresh_size.restype, lib.tpw_sj_fresh_size.argtypes = C.c_uint32, [C.c_int32]
    lib.tpw_sj_sprite.restype, lib.tpw_sj_sprite.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_sj_group.argtypes = [C.c_int32] * 3
    lib.tpw_cl_lab.restype, lib.tpw_cl_lab.argtypes = C.c_int32, [C.c_void_p]
    lib.tpw_sj_layout.argtypes = [C.POINTER(C.c_int64)]
    for name, (kinds, ret) in SIG.items():
        f = getattr(lib, "tpw_sj_" + name)
        f.argtypes = [CT[k] for k in kinds]
        f.restype = C.c_int32 if ret == "i" else None
    out = (C.c_int64 * 32)()
    n = lib.tpw_sj_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

# word indexes of the script block's fields
F_TEXT, F_SLOT, F_POS, F_START, F_LAST, F_BASE, F_STATE, F_TOP, F_STACK, F_F70, F_DEPTH, F_F78, F_NEXT, F_PERIOD, F_VARS, F_SPRITE, F_HAS = \
    0, 1, 2, 3, 4, 5, 6, 7, 8, 28, 29, 30, 31, 32, 33, 52, 69
BLOCK_WORDS = 70
NSPRITES = 32


def build(s, case):
    """Writes the case's globals and blocks through Side.words: the same code for both memories (offsets are the Mac's)."""
    w = case["w"]
    s.words(s.g("code"), (w["code"] + [0] * CODE_WORDS)[:CODE_WORDS])
    if w["nocheck"]:
        s.setptr("gScriptCheckTable", 0)
    else:
        chk = bytes((w["check"] + [0] * CODE_WORDS)[:CODE_WORDS])
        ca = s.alloc(CODE_WORDS)
        s.write(ca, chk)
        s.setptr("gScriptCheckTable", ca)
    s.words(s.g("gScriptInitialised"), [w["init"]])
    s.words(s.g("gScriptF7C2C"), [w["cap"]])
    s.words(s.g("gScriptTableSize"), [w["tsize"]])
    blocks = {}
    for i, img in enumerate(w["blocks"]):
        a = s.alloc(4 * BLOCK_WORDS)
        s.words(a, img)
        s.label(i + 1, a)
        blocks[i + 1] = a
    tsize = w["tsize"]
    table = s.alloc(4 * max(tsize, 1))
    s.label(TABLE_LABEL, table)
    s.words(table, [s.entry_word(blocks[l]) if l else 0 for l in w["entries"]][:max(tsize, 0)])
    s.setptr("gScriptTable", table)
    s.words(s.g("gScriptCodeBase"), [w["codebase"]])
    cur = w["current"]
    s.setptr("gScriptCurrent", blocks[cur] if cur else 0)
    s.setptr("gScriptVars", blocks[cur] + 4 * F_VARS if cur else 0)
    if w["switch"]:
        s.setswitch(blocks[w["switch"]])
    if w["esp"]:
        for i, (first, count) in enumerate(w["groups"]):
            s.group(i, first, count)
        for i in range(NSPRITES):
            s.label(SPRITE_LABEL + i, s.sprite(i))
    buf = s.alloc(16)
    s.words(buf, [0xA5A5A5A5] * 4)
    s.buf = buf
    s.blocks = blocks
    s.table = table


def arg_values(s, case):
    """The case's arguments as words: ("b", label, offset) a block's address, ("buf", slot) a slot of the buffer (0 = NULL)."""
    out = []
    for a in case["a"]:
        if isinstance(a, tuple):
            if a[0] == "b":
                out.append(s.blocks[a[1]] + a[2] if a[1] else a[2])
            elif a[0] == "buf":
                out.append(s.buf + 4 * a[1] if a[1] is not None else 0)
            else:
                raise ValueError(a)
        else:
            out.append(a)
    return out


def execute(s, case):
    """The call."""
    name = case["fn"]
    kinds = "" if name.startswith("PK_") else SIG[name][0]
    if name.startswith("PK_"):
        kinds = "i"
    args = arg_values(s, case)
    if name.startswith("PK_"):
        args = [0]
    r = s.call(name, kinds, args)
    if name.startswith("PK_") or SIG[name][1] == "v":
        return None
    return r


def block_of(s, addr, known):
    """A pointer into one of the blocks as (label, offset), else (0, the address if it is 0 else -1)."""
    for label, base in known:
        if base <= addr < base + 4 * BLOCK_WORDS:
            return (label, addr - base)
    return (0, 0 if addr == 0 else -1)


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides; pointers as labels."""
    w = case["w"]
    out = {"answer": answer}
    known = list(s.blocks.items())
    fresh = s.fresh()
    tables = []
    for a, size in fresh:
        if size == 4 * BLOCK_WORDS:
            known.append((len(known) + 1000, a))
    for name in ("gScriptInitialised", "gScriptF7C2C", "gScriptTableSize", "gScriptCodeBase"):
        out[name] = s.rd_word(s.g(name))
    out["current"] = block_of(s, s.getptr("gScriptCurrent"), known)
    out["vars"] = block_of(s, s.getptr("gScriptVars"), known)
    tsize = s.rd_word(s.g("gScriptTableSize"))
    out["table"] = s.addr_label(s.getptr("gScriptTable"))
    out["entries"] = [s.entry_label(x) for x in s.rd_words(s.getptr("gScriptTable"), max(min(tsize, 256), 0))] if s.getptr("gScriptTable") else []
    out["oldtable"] = [s.entry_label(x) for x in s.rd_words(s.table, max(w["tsize"], 0))]
    out["blocks"] = {l: s.rd_words(a, BLOCK_WORDS) for l, a in s.blocks.items()}
    out["fresh"] = [(size, s.rd_words(a, BLOCK_WORDS) if size == 4 * BLOCK_WORDS else [s.entry_label(x) for x in s.rd_words(a, size // 4)])
                    for a, size in fresh]
    out["buf"] = s.rd_words(s.buf, 4)
    out["log"] = s.log()
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    """A float's bits: mostly ordinary numbers of every scale, now and then a special value."""
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rw(r):
    """A random 32-bit word, often small."""
    k = r.random()
    if k < 0.3:
        return r.randrange(0, 40)
    if k < 0.4:
        return r.choice([MASK, 0x80000000, 0x7FFFFFFF, 0x80000001])
    return r.getrandbits(32)


def img(r, **ov):
    """A script block's 70 words: random, with fields set by name (F_xxx lower case without the prefix) or word index."""
    words = [rw(r) for _ in range(BLOCK_WORDS)]
    words[F_STATE] = r.choice([1, 1, 2, 3, 4, rw(r)])
    words[F_TOP] = r.choice([0x14, 0x14, 0x13, 1, 0, 5])
    words[F_DEPTH] = r.choice([0, 0, 1, 3, 10, 11, 5])
    words[F_HAS] = r.choice([0, 1, 1, 5])
    words[F_BASE] = CODE
    words[F_POS] = r.randrange(0, 6)
    for k, v in ov.items():
        words[int(k[1:])] = v & MASK
    return words


def code_addr(k):
    return CODE + 4 * k


def base(r, fn, nblocks=None, tsize=None, density=0.5, **kw):
    """The world every case starts from: the system on, the table with entries pointing at blocks, the queues short."""
    nb = nblocks if nblocks is not None else r.randint(3, 6)
    ts = tsize if tsize is not None else r.randint(1, 12)
    w = {"code": [], "check": [], "nocheck": False, "init": r.choice([1, 1, 1, 2, 0xFFFFFFFF, 0x80000000]), "cap": 0x32, "tsize": ts,
         "blocks": [img(r) for _ in range(nb)], "esp": False, "groups": [(0, 0)] * 5, "codebase": CODE, "current": 0, "switch": 0}
    # the table: entry 0 is never used by the code; the entries point at blocks 2..nb (block 1 is the one the case calls with)
    w["entries"] = [r.randint(2, nb) if r.random() < 0.25 else 0] + [r.randint(2, nb) if r.random() < density else 0 for _ in range(max(ts - 1, 0))]
    w["entries"] = w["entries"][:max(ts, 0)]
    w.update(kw)
    return {"fn": fn, "w": w, "a": [], "queues": {"clock": [rw(r) & 0x7FFFFFFF]}}


def table_full(c):
    w = c["w"]
    w["entries"] = [w["entries"][0] if w["entries"] else 0] + [2 + (i % (len(w["blocks"]) - 1)) for i in range(max(w["tsize"] - 1, 0))]
    w["entries"] = w["entries"][:max(w["tsize"], 0)]


# ---- AssignSlot
def assign_case(r, tsize=None, full=False, empty_at=None):
    c = base(r, "AssignSlot", tsize=tsize, density=r.choice([0.2, 0.6, 0.9, 1.0]))
    if full:
        table_full(c)
    if empty_at is not None and c["w"]["tsize"] > 1:
        c["w"]["entries"][empty_at % c["w"]["tsize"] or 1] = 0
    c["a"] = [("b", 1, 0)]
    return c


def assign_edges():
    r = random.Random(1001)
    for ts in (0, 1, 2, 3, 12):
        yield assign_case(r, ts, full=True)
    for ts in (2, 3, 6, 12):
        yield assign_case(r, ts, full=True, empty_at=1)
        yield assign_case(r, ts, full=True, empty_at=ts - 1)
    yield assign_case(r, 6)


# ---- InitScript
def init_case(r, k=None):
    c = base(r, "InitScript")
    k = r.choice([0, 1, 5, 40, CODE_WORDS - 1, r.randrange(0, CODE_WORDS)]) if k is None else k
    c["a"] = [("b", 1, 0), code_addr(k), rw(r), rf(r), rf(r), rf(r)]
    c["queues"]["clock"] = [r.choice([0, 0x7FFFFFFF, MASK, 0x80000000, rw(r)])]
    return c


def init_edges():
    r = random.Random(1002)
    for k in (0, 1, CODE_WORDS - 1):
        yield init_case(r, k)


# ---- DoScript: programs
def program(r, n, loops=True, fakes=True):
    """n instructions from the descriptor set: (code words, check bytes), the operands with check byte 0 (as the real check table has).
    GOTO and GOSUB targets are aligned addresses inside the first 60 words."""
    code, check = [], []
    own = ["GOTO", "GOSUB", "RETURN", "PUSHVAR", "POPVAR", "END", "WAIT", "NOP", "SETTEXT", "SETVAR"]
    if not loops:
        own = own[2:]
    for _ in range(n):
        name = r.choice(own + [d for _, d in [(0, x) for x in DESC] if d not in own][:0] + (["IF", "ADD", "SPAWN", "DO", "LOOP"] if fakes else []))
        if name == "END" and r.random() < 0.7:
            name = "NOP"
        k = DESC.index(name)
        code.append(DESC_BASE + 8 * k)
        check.append(1 if r.random() > 0.03 else 0)
        ops = []
        if name in ("GOTO", "GOSUB"):
            ops = [code_addr(r.randrange(0, 60))]
        elif name in ("PUSHVAR", "POPVAR"):
            ops = [r.randrange(7, 19)]
        elif name == "SETTEXT":
            ops = [rw(r)]
        elif name == "SETVAR":
            ops = [r.randrange(7, 19), r.randrange(0, 60)]
        code.extend(ops)
        check.extend([0] * len(ops))
    return code, check


def do_case(r, straight=False, calm=False, **over):
    c = base(r, "DoScript", nblocks=r.randint(2, 4))
    w = c["w"]
    code, check = program(r, r.randint(1, 14), loops=not straight)
    w["code"], w["check"] = code, check
    w["cap"] = r.choice([0x80000000, MASK, 0x7FFFFFFF, 40]) if straight else r.choice([1, 2, 3, 5, 0x32, 0x32, 20])
    w["blocks"][0] = img(r, f2=0, f3=r.choice([0, 0, 2]), f5=r.choice([CODE, CODE, CODE + 4 * r.randrange(0, 3)]),
                          f6=r.choice([1, 1, 2, 3, 4, 0, 7]), f7=r.choice([0x14, 0x14, 0x10, 0x2, 0]), f29=r.choice([0, 0, 2, 9, 10, 11]))
    w["blocks"][1][F_POS] = r.randrange(0, 5)
    for b in w["blocks"]:
        # stack and variables hold positions a RETURN may pop: small, so that the Mac never reads outside its tables
        for i in list(range(F_STACK, F_STACK + 20)) + list(range(F_VARS + 7, F_VARS + 19)):
            b[i] = r.randrange(0, 60)
    w["switch"] = 2
    w["current"] = r.choice([0, 1, 2])
    w["codebase"] = r.choice([0x1000, CODE, CODE])
    q = [0] * r.randint(0, 6)
    for _ in range(r.randint(0, 4)):
        q.insert(r.randrange(0, len(q) + 1), r.choice([1, 2, 3, 4, 8, 5 + 4 * r.randrange(0, 4), 64, 64 + 4, 67]))
    if calm:
        # a long run: no state changes by the fakes, a high cap, the first block running
        q = [v & 0x3C for v in q] + [0]
        w["cap"] = r.choice([20, 0x32, 100]) if not straight else w["cap"]
        w["blocks"][0][F_STATE] = r.choice([1, 1, 2, 4])
        w["blocks"][0][F_POS] = 0
        w["blocks"][0][F_BASE] = CODE
        w["codebase"] = CODE
    c["queues"]["pk"] = q or [0]
    c["a"] = [("b", 1, 0)]
    for k, v in over.items():
        w[k] = v
    return c


def do_edges():
    r = random.Random(1003)
    # state 3 answers 0; PK_END first; a 0 check byte first; cap 1; WAIT in the middle; GOTO loops against the cap; state 4 by a fake
    def mk(code, check, cap=0x32, state=1, pos=0, **kw):
        c = do_case(r, straight=True)
        w = c["w"]
        w["code"], w["check"], w["cap"] = code, check, cap
        w["blocks"][0][F_STATE], w["blocks"][0][F_POS], w["blocks"][0][F_BASE] = state, pos, CODE
        w["current"], w["codebase"] = 0, CODE
        c["queues"]["pk"] = [0]
        w.update(kw)
        return c
    d = lambda n: DESC_BASE + 8 * DESC.index(n)
    yield mk([d("NOP")], [1], state=3)
    yield mk([d("END"), d("NOP")], [1, 1])
    yield mk([d("NOP"), d("NOP")], [0, 1])
    yield mk([d("NOP"), d("NOP"), d("NOP")], [1, 1, 1], cap=1)
    yield mk([d("NOP"), d("WAIT"), d("NOP")], [1, 1, 1])
    yield mk([d("GOTO"), CODE, d("NOP")], [1, 0, 1], cap=7)
    yield mk([d("GOTO"), CODE + 8, d("NOP"), d("NOP")], [1, 0, 1, 1], cap=0)
    yield mk([d("ADD"), d("NOP"), d("NOP")], [1, 1, 1], pk=None)
    q = mk([d("ADD"), d("NOP"), d("NOP")], [1, 1, 1])
    q["queues"]["pk"] = [3]
    yield q
    q = mk([d("ADD"), d("NOP"), d("NOP")], [1, 1, 1])
    q["queues"]["pk"] = [1]
    yield q
    q = mk([d("ADD"), d("NOP"), d("NOP")], [1, 1, 1], cap=MASK)
    yield q
    q = mk([d("NOP"), d("NOP"), d("NOP")], [1, 1, 1], cap=0x80000000)
    yield q
    q = mk([d("ADD"), d("NOP"), d("NOP")], [1, 1, 1], switch=2)
    q["queues"]["pk"] = [64]
    yield q
    yield mk([d("NOP")], [1], pos=-1 + 1, codebase=CODE + 4)
    yield mk([d("NOP"), d("NOP")], [1, 1], state=2)
    yield mk([d("NOP"), d("NOP")], [1, 1], state=0x12345)


# ---- the stack
def stack_case(r, fn):
    c = base(r, fn, nblocks=2)
    w = c["w"]
    w["current"] = 1
    w["blocks"][0] = img(r, f7=r.choice([0, 1, 2, 0x13, 0x14, 0x15, 0x16, 0x20, -1, -5, r.randrange(-6, 0x30)]))
    c["a"] = [rw(r)] if fn == "SCR_Push" else []
    return c


def stack_edges(fn):
    def gen():
        r = random.Random(1004 if fn == "SCR_Push" else 1005)
        for top in (0, 1, 2, 0x12, 0x13, 0x14, 0x15, 0x16, -1, -6):
            c = stack_case(r, fn)
            c["w"]["blocks"][0][F_TOP] = top & MASK
            yield c
    return gen


def error_case(r):
    return base(r, "SCR_Error")


# ---- handles
def handle(r, ts):
    return r.choice([0, 1, ts - 1, ts, ts + 1, -1, -0x80000000, 0x7FFFFFFF, r.randrange(0, ts + 2), r.randrange(1, max(ts, 2))]) & MASK


def setter_case(r, fn, h=None, init=None):
    c = base(r, fn, density=r.choice([0.5, 0.9, 1.0]))
    ts = c["w"]["tsize"]
    if init is not None:
        c["w"]["init"] = init
    h = handle(r, ts) if h is None else h & MASK
    if fn == "SCR_SetScriptPosition":
        c["a"] = [h, rf(r), rf(r), rf(r), rw(r)]
    elif fn == "SCR_GetScriptPosition":
        c["a"] = [h] + [("buf", r.choice([None, i])) for i in range(4)]
        c["a"] = [h] + [("buf", i if r.random() < 0.75 else None) for i in range(4)]
    elif fn == "SCR_SetVariable":
        c["a"] = [h, r.choice([0, 1, 18, 19, 0x12, 0x13, MASK, r.randrange(0, 25), 0x80000000]), rw(r)]
    elif fn == "SCR_SetFloatVariable":
        c["a"] = [h, r.choice([0, 1, 18, 19, MASK, r.randrange(0, 25), 0x80000000]), rf(r)]
    else:
        c["a"] = [h, rw(r)]
    return c


def setter_edges(fn):
    def gen():
        r = random.Random(1010 + len(fn))
        for init in (1, 0):
            for ts in (1, 2, 5):
                c = setter_case(r, fn, init=init)
                c["w"]["tsize"] = ts
                c["w"]["entries"] = ([0] + [2] * (ts - 1))[:ts]
                for h in (0, 1, ts - 1, ts, ts + 1, -1, -0x80000000):
                    c2 = setter_case(r, fn, h=h, init=init)
                    c2["w"]["tsize"] = ts
                    c2["w"]["entries"] = [2] * ts
                    yield c2
        if fn in ("SCR_SetVariable", "SCR_SetFloatVariable"):
            for idx in (0x12, 0x13, 0x14, MASK, 0x80000000, 0x7FFFFFFF):
                c = setter_case(r, fn, h=1, init=1)
                c["w"]["tsize"], c["w"]["entries"] = 3, [0, 2, 3]
                c["a"][1] = idx
                yield c
    return gen


# ---- SCR_WhichScriptHasThisSprite
def which_case(r):
    c = base(r, "SCR_WhichScriptHasThisSprite", density=r.choice([0.6, 1.0]))
    nb = len(c["w"]["blocks"])
    target = r.randint(1, nb)
    off = r.choice([0xD0, 0xD0, 0xD0, 0xD4, 0x0, 0xCC])
    c["a"] = [("b", target, off)] if r.random() < 0.9 else [("b", 0, r.choice([0, 0x1234]))]
    return c


def which_edges():
    r = random.Random(1020)
    for hasflag in (0, 1):
        for init in (1, 0):
            c = which_case(r)
            c["w"]["init"] = init
            c["w"]["tsize"] = 4
            c["w"]["blocks"] = [img(r, f69=hasflag), img(r, f69=hasflag), img(r, f69=1)]
            c["w"]["entries"] = [0, 2, 3, 3]
            c["a"] = [("b", 3, 0xD0)]
            yield c
            c2 = which_case(r)
            c2["w"]["tsize"], c2["w"]["entries"], c2["a"] = 4, [0, 2, 0, 3], [("b", 3, 0xD0)]
            c2["w"]["blocks"] = [img(r, f69=hasflag), img(r, f69=hasflag), img(r, f69=hasflag)]
            yield c2
    c = which_case(r)
    c["a"] = [("b", 0, 0)]
    yield c


# ---- SCR_SpawnScript
def spawn_case(r):
    c = base(r, "SCR_SpawnScript", density=r.choice([0.3, 0.7, 1.0]))
    c["a"] = [code_addr(r.choice([0, 3, 77, r.randrange(0, CODE_WORDS)])), rw(r), rf(r), rf(r), rf(r)]
    return c


def spawn_edges():
    r = random.Random(1030)
    for init in (0, 1):
        for full in (False, True):
            c = spawn_case(r)
            c["w"]["init"] = init
            if full:
                table_full(c)
            yield c
    c = spawn_case(r)
    c["w"]["tsize"], c["w"]["entries"] = 1, [0]
    yield c


# ---- ESP_InitScript / ESP_SpawnScript
def esp_world(r, c):
    w = c["w"]
    w["esp"] = True
    w["groups"] = [(r.randrange(0, 12), r.choice([0, 1, 3, 5, 8, -2])) for _ in range(5)]
    c["queues"]["frames"] = [r.randrange(0, 256)]


def espinit_case(r):
    c = base(r, "ESP_InitScript")
    esp_world(r, c)
    st, idx = r.randrange(0, 5), r.randrange(0, 8)
    c["a"] = [("b", 1, 0), code_addr(r.choice([0, 9, r.randrange(0, CODE_WORDS)])), st, idx, rw(r), rf(r), rf(r), rf(r), rw(r), rw(r)]
    c["queues"]["clock"] = [r.choice([0, 7, rw(r) & 0x7FFFFFFF])]
    return c


def espinit_edges():
    r = random.Random(1040)
    for st in range(5):
        c = espinit_case(r)
        c["a"][2], c["a"][3] = st, 0
        yield c


def espspawn_case(r, custom=None, count=None):
    c = base(r, "ESP_SpawnScript", density=r.choice([0.3, 0.8, 1.0]))
    esp_world(r, c)
    st, idx = r.randrange(0, 5), r.randrange(0, 8)
    if count is not None:
        c["w"]["groups"][st] = (c["w"]["groups"][st][0], count)
    elif r.random() < 0.7:
        c["w"]["groups"][st] = (c["w"]["groups"][st][0], r.randint(1, 8))
    code = r.choice([0, 1, 2, 3]) if (custom if custom is not None else r.random() < 0.6) else code_addr(r.randrange(0, CODE_WORDS))
    c["a"] = [code, st, idx, rw(r), rf(r), rf(r), rf(r)]
    q = c["queues"]
    q["cd1"], q["cd2"], q["cd3"], q["cd4"] = [r.randrange(0, 4)], [rw(r)], [rw(r)], [rw(r)]
    q["cdmask"] = [r.choice([15, 15, 15, 1, 3, 5, 9, 7, 13])]
    return c


def espspawn_edges():
    r = random.Random(1041)
    for code in (0, 1, 2, 3, 5, 6):
        for cnt in (0, 1, 3):
            yield espspawn_case(r, custom=code <= 3, count=cnt) if code <= 3 else espspawn_case(r, custom=False, count=cnt)
    for init in (0,):
        c = espspawn_case(r)
        c["w"]["init"] = init
        yield c
    for first in range(4):
        c = espspawn_case(r, custom=True, count=2)
        c["queues"]["cd1"] = [first]
        c["queues"]["cdmask"] = [15]
        yield c


# ---- the commands, called directly
def pk_case(r, name):
    c = base(r, "PK_" + name, nblocks=2)
    w = c["w"]
    w["current"] = 1
    pos = r.randrange(0, 40)
    ops = {
        "GOTO": [r.choice([code_addr(r.randrange(0, 80)), rw(r), CODE - 4, CODE + 1, CODE + 2])],
        "SETTEXT": [rw(r)],
        "SETVAR": [r.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 18, r.randrange(0, 37)]), r.choice([rw(r), r.randrange(0, 100), 0x7FFFFFFF, 0x80000000, 0x01000001, 0xFEFFFFFF, MASK])],
        "PUSHVAR": [r.randrange(0, 37)], "POPVAR": [r.randrange(0, 37)],
        "GOSUB": [r.choice([code_addr(r.randrange(0, 80)), rw(r), CODE + 2])], "RETURN": [], "NOP": [], "END": [], "WAIT": [],
    }[name]
    w["codebase"] = r.choice([CODE, CODE, CODE + 4 * r.randrange(0, 5)])
    w["oppos"] = pos + (w["codebase"] - CODE) // 4      # where the operands lie in the array
    w["code"] = [rw(r)] * w["oppos"] + ops + [rw(r)] * 4
    w["blocks"][0] = img(r, f2=pos, f7=r.choice([0x14, 0x13, 0x10, 1, 0, 2, 0x15, rw(r) % 0x18]), f29=r.choice([0, 1, 9, 10, 11, 12, -1 & MASK, -2 & MASK, 3]))
    return c


def pk_edges(name):
    def gen():
        r = random.Random(1050 + sum(map(ord, name)))
        for _ in range(3):
            yield pk_case(r, name)
        if name in ("GOSUB", "RETURN"):
            for depth in (-1, 0, 1, 9, 10, 11, 12):
                for top in (0x14, 0x13, 1, 0):
                    c = pk_case(r, name)
                    c["w"]["blocks"][0][F_DEPTH] = depth & MASK
                    c["w"]["blocks"][0][F_TOP] = top
                    yield c
        if name == "SETVAR":
            for idx in range(0, 9):
                for val in (0, 1, 0x7FFFFFFF, 0x80000000, 0x01000001, MASK):
                    c = pk_case(r, name)
                    pos = c["w"]["oppos"]
                    c["w"]["code"][pos], c["w"]["code"][pos + 1] = idx, val
                    yield c
    return gen


def rnd_of(make):
    return lambda r: make(r)


TESTS = [
    FnTest("AssignSlot", ppc, host, edge=assign_edges, rnd=lambda r: assign_case(r, full=r.random() < 0.3)),
    FnTest("InitScript", ppc, host, edge=init_edges, rnd=init_case),
    FnTest("DoScript", ppc, host, edge=do_edges, rnd=lambda r: do_case(r, straight=r.random() < 0.25, calm=r.random() < 0.4)),
    FnTest("SCR_Error", ppc, host, rnd=error_case),
    FnTest("SCR_Push", ppc, host, edge=stack_edges("SCR_Push"), rnd=lambda r: stack_case(r, "SCR_Push")),
    FnTest("SCR_Pop", ppc, host, edge=stack_edges("SCR_Pop"), rnd=lambda r: stack_case(r, "SCR_Pop")),
    FnTest("SCR_SpawnScript", ppc, host, edge=spawn_edges, rnd=spawn_case),
    FnTest("SCR_SetScriptPosition", ppc, host, edge=setter_edges("SCR_SetScriptPosition"), rnd=lambda r: setter_case(r, "SCR_SetScriptPosition")),
    FnTest("SCR_GetScriptPosition", ppc, host, edge=setter_edges("SCR_GetScriptPosition"), rnd=lambda r: setter_case(r, "SCR_GetScriptPosition")),
    FnTest("SCR_SetVariable", ppc, host, edge=setter_edges("SCR_SetVariable"), rnd=lambda r: setter_case(r, "SCR_SetVariable")),
    FnTest("SCR_SetFloatVariable", ppc, host, edge=setter_edges("SCR_SetFloatVariable"), rnd=lambda r: setter_case(r, "SCR_SetFloatVariable")),
    FnTest("SCR_SetServiceInterval", ppc, host, edge=setter_edges("SCR_SetServiceInterval"), rnd=lambda r: setter_case(r, "SCR_SetServiceInterval")),
    FnTest("SCR_WhichScriptHasThisSprite", ppc, host, edge=which_edges, rnd=which_case),
    FnTest("ESP_InitScript", ppc, host, edge=espinit_edges, rnd=espinit_case),
    FnTest("ESP_SpawnScript", ppc, host, edge=espspawn_edges, rnd=espspawn_case),
] + [FnTest("PK_" + n, ppc, host, edge=pk_edges(n), rnd=(lambda n: lambda r: pk_case(r, n))(n))
     for n in ("GOTO", "NOP", "END", "WAIT", "SETTEXT", "SETVAR", "PUSHVAR", "POPVAR", "GOSUB", "RETURN")]
