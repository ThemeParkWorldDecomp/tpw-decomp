"""Unit w165, the script sprites and the script save and load, against libtpw_unit_ScriptSprites.dylib (tests/unit_ScriptSprites.cpp),
main app ("SimTheme Park"), src/game/Script.cpp, src/game/ScriptOps.cpp:
  ESP_ForceGoto 0x100da318, ESP_RURunningThis 0x100da46c, ESP_Spawn2DCopyOfScriptAt 0x100da590, ESP_Update2DScript 0x100da6d8,
  ESP_FixSpriteIDsFromDetail 0x100da884, the commands PK_SPAWN 0x100dc0e0, PK_SET_OBJ_ID 0x100dc160, PK_SET_OBJ_ACT 0x100dc1b8,
  PK_PutSpriteVAR 0x100dc214, PK_PutSpriteIM 0x100dc2d0, PK_Put3DSpriteIM 0x100dc384, PK_RemoveSprite 0x100dc418, PK_ResetScale 0x100dc45c,
  PK_ReduceScale 0x100dc494, PK_IncreaseScale 0x100dc4d8, PK_ResetRotation 0x100dc51c, PK_ReduceRotation 0x100dc558,
  PK_IncreaseRotation 0x100dc59c, SCR_SaveAllScripts 0x100d96b0, SCR_LoadAllScripts 0x100d9ccc and the file swaps USwapInplace32<int>
  0x100d9920, SSpriteGameData::Byteswap 0x100d9964, USwapBlock32<SLONGfloat> 0x100d9b00, USwapBlock32<long> 0x100d9b74, USwap32<long*>
  0x100d9bd0, USwapInplace32<long> 0x100d9c2c, USwap32<char*> 0x100d9c70.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that log their arguments and answer from the case's queues: the allocator (fresh blocks are filled with
0xA5 and read back; the queue "alloc" makes an allocation fail), the file read (a byte stream, short reads zero the rest) and write (the
chunks are compared), the swap helpers (the Mac's swap in place, the host's none; the copy and its delete are logged), AssignSlot,
SCR_SpawnScript, DrawHLSprite, ESP_GetNumberOfSprites and the sprites' two methods. The file is little-endian: bytes written by the Mac
(after its swaps) and by the host are the same, and bytes read are the same, so the comparisons use numeric words in each side's order.
The half word at block offset 0x108 (the sprite record's +0x38, swapped as 16 bits) is read and written as (half, byte, byte) on both sides.

Not tested: a handle outside the table for the functions that dereference it (the Mac reads address 0), a code argument of -1 or an
address outside the code array (the host answers 0 for any pointer outside the array), a custom start index above 3 (the Mac reads past its
start address table), GetCustomData leaving `first` or the action unwritten (the Mac uses uninitialised stack), PK_SPAWN with a code word
outside the array, a table size below 1 or the allocation of 0 bytes in the load, a failed allocation in the bad header path of the load
(the Mac passes NULL to memset), a sprite number outside the sprite array.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ScriptSprites"
MASK = 0xFFFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FN = {
    "ESP_ForceGoto": (0x100DA318, 300), "ESP_RURunningThis": (0x100DA46C, 248), "ESP_Spawn2DCopyOfScriptAt": (0x100DA590, 276),
    "ESP_Update2DScript": (0x100DA6D8, 220), "ESP_FixSpriteIDsFromDetail": (0x100DA884, 164),
    "SCR_SaveAllScripts": (0x100D96B0, 580), "SCR_LoadAllScripts": (0x100D9CCC, 784),
    "PK_SPAWN": (0x100DC0E0, 96), "PK_SET_OBJ_ID": (0x100DC160, 52), "PK_SET_OBJ_ACT": (0x100DC1B8, 92),
    "PK_PutSpriteVAR": (0x100DC214, 148), "PK_PutSpriteIM": (0x100DC2D0, 180), "PK_Put3DSpriteIM": (0x100DC384, 148),
    "PK_RemoveSprite": (0x100DC418, 28), "PK_ResetScale": (0x100DC45C, 56), "PK_ReduceScale": (0x100DC494, 28),
    "PK_IncreaseScale": (0x100DC4D8, 28), "PK_ResetRotation": (0x100DC51C, 28), "PK_ReduceRotation": (0x100DC558, 28),
    "PK_IncreaseRotation": (0x100DC59C, 28),
    "USwapInplace32Int": (0x100D9920, 12), "SpriteRecordByteswap": (0x100D9964, 364), "USwapBlock32SLONGfloat": (0x100D9B00, 36),
    "USwapBlock32Long": (0x100D9B74, 36), "USwap32LongPtr": (0x100D9BD0, 40), "USwapInplace32Long": (0x100D9C2C, 12),
    "USwap32CharPtr": (0x100D9C70, 40),
}
OWN = {a for a, _ in FN.values()}
PK_INDEX = {n: i for i, n in enumerate(["SPAWN", "SET_OBJ_ID", "SET_OBJ_ACT", "PutSpriteVAR", "PutSpriteIM", "Put3DSpriteIM", "RemoveSprite",
                                        "ResetScale", "ReduceScale", "IncreaseScale", "ResetRotation", "ReduceRotation", "IncreaseRotation"])}

# ---- the callees faked on both sides
GET_FRAMES, GET_CUSTOM = 0x1007B1F8, 0x1007B23C
DRAW_HL, NUM_SPRITES, ASSIGN_SLOT, SPAWN_SCRIPT = 0x1007B114, 0x1007C8B0, 0x100D8630, 0x100D924C
CODE = 0x1029F20C
CODE_WORDS = 883
START_TABLE = [0x1029FE58, 0x1029FEF0, 0x1029FF20, 0x1029FF50]
FILE = 0x1234

ADDR = dict(gScriptInitialised=0x102F7C28, gScriptTableSize=0x102F7C14, gScriptTable=0x102F7C18, gScriptCurrent=0x102F7C24,
            gScriptCodeBase=0x102F7C20, gScriptVars=0x102F7C1C, code=CODE)
GROUPS, SPRITES = 0x1029D368, 0x102DB520
HOST_LAYOUT = [0x118, 0x84, 0xD0, 0x114]
TABLE_LABEL, SPRITE_LABEL = 50, 200

SIG = {
    "ESP_ForceGoto": ("ic", "v"), "ESP_RURunningThis": ("ic", "i"), "ESP_Spawn2DCopyOfScriptAt": ("iffff", "i"),
    "ESP_Update2DScript": ("ii", "v"), "ESP_FixSpriteIDsFromDetail": ("", "v"), "SCR_SaveAllScripts": ("p", "v"),
    "SCR_LoadAllScripts": ("p", "i"),
    "USwapInplace32Int": ("p", "v"), "USwapInplace32Long": ("p", "v"), "USwapBlock32SLONGfloat": ("pi", "v"), "USwapBlock32Long": ("pi", "v"),
    "USwap32LongPtr": ("u", "u"), "USwap32CharPtr": ("u", "u"), "SpriteRecordByteswap": ("p", "v"),
}


def fb(x):
    """The single-precision bits of a Python number."""
    return struct.unpack(">I", struct.pack(">f", x))[0]


def fbx(x):
    """Single-precision bits of an FPR value; a NaN keeps its payload (a signalling NaN stays signalling, as a float argument on the host)."""
    if x != x:
        d = struct.unpack(">Q", struct.pack(">d", x))[0]
        return ((d >> 63) << 31) | 0x7F800000 | ((d >> 29) & 0x7FFFFF)
    return fb(x)


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


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
    if L.ret("alloc", 1) == 0:
        L.rec("LbMem_Alloc", (size, 0))
        e.set_r(3, 0)
        return
    a = e.alloc(max(size, 1), b"\xa5" * max(size, 1))
    L.fresh.append((a, size))
    L.rec("LbMem_Alloc", (size, L.newlab(a)))
    e.set_r(3, a)


def stub_free(e):
    e.cl.rec("LbMem_Free", (e.cl.lab(e.r(3)),))


def stub_read(e):
    # r3: the TbError's place, r4: the file, r5: the buffer, r6: the length, r7: bytes done. A short file zeroes the rest.
    L, buffer, length = e.cl, e.r(5), e.r(6)
    L.rec("LbFile_Read", (e.r(4), length, 1 if e.r(7) else 0))
    chunk = L.file_in[L.file_pos:L.file_pos + length]
    L.file_pos += length
    if length:
        e.write(buffer, chunk + bytes(length - len(chunk)))
    e.write(e.r(3), bytes(4))


def stub_write(e):
    L, buffer, length = e.cl, e.r(5), e.r(6)
    L.rec("LbFile_Write", (e.r(4), length, 1 if e.r(7) else 0))
    L.file_out.append(e.read(buffer, length) if length else b"")
    e.write(e.r(3), bytes(4))


def swap_words(data):
    return b"".join(data[i:i + 4][::-1] for i in range(0, len(data) - len(data) % 4, 4))


def stub_swap_block(e):
    words, count = e.r(3), s32(e.r(4))
    e.cl.rec("USwapBlock32", (count,))
    if count > 0:
        e.write(words, swap_words(e.read(words, 4 * count)))


def stub_swap_copy(e):
    L = e.cl
    words, count = e.r(3), s32(e.r(4))
    data = e.read(words, 4 * count) if count > 0 else b""
    a = e.alloc(max(4 * count, 4), swap_words(data) if count > 0 else bytes(4))
    L.rec("USwapCopy32", (count, e.u32(words) if count > 0 else 0, L.newlab(a)))
    e.set_r(3, a)


def stub_swap_delete(e):
    e.cl.rec("USwapDelete", (e.cl.lab(e.r(3)),))


def stub_assign(e):
    L = e.cl
    L.rec("AssignSlot", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("assign", 5) & MASK)


def stub_spawn(e):
    L = e.cl
    L.rec("SCR_SpawnScript", (e.r(3), s32(e.r(4)), fbx(e.f(1)), fbx(e.f(2)), fbx(e.f(3))))
    e.set_r(3, L.ret("spawn", 7) & MASK)


def stub_draw(e):
    L = e.cl
    L.rec("DrawHLSprite", (L.lab(e.r(3)),))


def stub_nsprites(e):
    L = e.cl
    L.rec("ESP_GetNumberOfSprites", (s32(e.r(3)),))
    e.set_r(3, L.ret("nsprites", 3) & MASK)


STUBS.internal(GET_FRAMES, stub_frames)
STUBS.internal(GET_CUSTOM, stub_custom)
STUBS.internal(ASSIGN_SLOT, stub_assign)
STUBS.internal(SPAWN_SCRIPT, stub_spawn)
STUBS.internal(DRAW_HL, stub_draw)
STUBS.internal(NUM_SPRITES, stub_nsprites)
STUBS.imp("LbMem_Alloc__FUi", stub_alloc)
STUBS.imp("LbMem_Free__FPv", stub_free)
STUBS.imp("LbFile_Read__FPvPvUlPUl", stub_read)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", stub_write)
STUBS.imp("_USwapBlock32__13SamsUtilitiesFPUll", stub_swap_block)
STUBS.imp("_USwapCopy32__13SamsUtilitiesFPUll", stub_swap_copy)
STUBS.imp("_USwapDelete__13SamsUtilitiesFPv", stub_swap_delete)
STUBS.imp("__ct__15SSpriteGameDataFv", lambda e: None)


def setup(runner):
    T.install_std_stubs(runner.emu)       # memset, memcpy


# ---------------------------------------------------------------------------------------------------------------- the two sides

BLOCK_WORDS = 70
SPECIAL_W = 66          # block offset 0x108: the sprite record's +0x38 half word
NSPRITES = 32
F_TEXT, F_SLOT, F_POS, F_START, F_LAST, F_BASE, F_STATE, F_TOP, F_STACK, F_F70, F_DEPTH, F_F78, F_NEXT, F_PERIOD, F_VARS, F_SPRITE, F_HAS = \
    0, 1, 2, 3, 4, 5, 6, 7, 8, 28, 29, 30, 31, 32, 33, 52, 69


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wr_block(self, a, ws):
        """A block's words; the word at SPECIAL_W is (half word, byte, byte) in this side's order."""
        ws = list(ws)
        special = ws[SPECIAL_W]
        raw = ws[SPECIAL_W + 1]
        ws[SPECIAL_W] = ws[SPECIAL_W + 1] = 0
        self.words(a, ws)
        self.write(a + 4 * SPECIAL_W, struct.pack(self.fmt[0] + "H", special & 0xFFFF) + bytes([(special >> 16) & 0xFF, (special >> 24) & 0xFF])
                   + struct.pack(">I", raw & MASK))

    def rd_block(self, a):
        ws = self.rd_words(a, BLOCK_WORDS)
        raw = self.read(a + 4 * SPECIAL_W, 4)
        ws[SPECIAL_W] = struct.unpack(self.fmt[0] + "H", raw[:2])[0] | raw[2] << 16 | raw[3] << 24
        ws[SPECIAL_W + 1] = struct.unpack(">I", self.read(a + 4 * SPECIAL_W + 4, 4))[0]
        return ws


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.fresh = []
        self.L.file_in, self.L.file_pos, self.L.file_out = b"", 0, []
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

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

    def group(self, i, first, count):
        name = self.e.alloc(8, b"grp%d\0" % i)
        self.words(GROUPS + 12 * i, [name, first, count])

    def sprite(self, i):
        a = self.e.alloc(16)
        self.words(SPRITES + 4 * i, [a])
        return a

    def fresh(self):
        return list(self.L.fresh)

    def file_in(self, data):
        self.L.file_in = data

    def file_out(self):
        return list(self.L.file_out)

    def log(self):
        return self.L.log

    def call(self, name, kinds, args):
        a = []
        for k, v in zip(kinds, args):
            a.append(T.F(bitsf(v)) if k == "f" else v & MASK)
        r3, _, _ = T.call_ppc(self.e, FN[name][0], a)
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_sg_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    @property
    def base(self):
        return self.lib.tpw_sg_base()

    def g(self, name):
        return self.lib.tpw_sg_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sg_alloc(max(n, 4))

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

    def group(self, i, first, count):
        self.lib.tpw_sg_group(i, first, count)

    def sprite(self, i):
        return self.lib.tpw_sg_sprite(i)

    def fresh(self):
        return [(self.lib.tpw_sg_fresh(i), self.lib.tpw_sg_fresh_size(i)) for i in range(self.lib.tpw_sg_nfresh())]

    def file_in(self, data):
        self.lib.tpw_sg_file_in(data, len(data))

    def file_out(self):
        return [C.string_at(self.lib.tpw_sg_out_data(i), self.lib.tpw_sg_out_length(i)) for i in range(self.lib.tpw_sg_out_count())]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, kinds, args):
        if name.startswith("PK_"):
            return self.lib.tpw_sg_PK(PK_INDEX[name[3:]])
        f = getattr(self.lib, "tpw_sg_" + name)
        r = f(*[(bitsf(v) if k == "f" else (v & MASK if k in "uc" else (s32(v) if k == "i" else v))) for k, v in zip(kinds, args)])
        return None if r is None else r & MASK


CT = {"i": C.c_int32, "u": C.c_uint32, "c": C.c_uint32, "f": C.c_float, "p": C.c_uint64}


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_sg_addr.restype, lib.tpw_sg_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sg_alloc.restype, lib.tpw_sg_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sg_base.restype = C.c_uint64
    lib.tpw_sg_nfresh.restype = C.c_int32
    lib.tpw_sg_fresh.restype, lib.tpw_sg_fresh.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_sg_fresh_size.restype, lib.tpw_sg_fresh_size.argtypes = C.c_uint32, [C.c_int32]
    lib.tpw_sg_sprite.restype, lib.tpw_sg_sprite.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_sg_group.argtypes = [C.c_int32] * 3
    lib.tpw_sg_file_in.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_sg_out_count.restype = C.c_int32
    lib.tpw_sg_out_length.restype, lib.tpw_sg_out_length.argtypes = C.c_int32, [C.c_int32]
    lib.tpw_sg_out_data.restype, lib.tpw_sg_out_data.argtypes = C.c_void_p, [C.c_int32]
    lib.tpw_cl_lab.restype, lib.tpw_cl_lab.argtypes = C.c_int32, [C.c_void_p]
    lib.tpw_sg_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sg_PK.argtypes = [C.c_int32]
    for name, (kinds, ret) in SIG.items():
        f = getattr(lib, "tpw_sg_" + name)
        f.argtypes = [CT[k] for k in kinds]
        f.restype = {"i": C.c_int32, "u": C.c_uint32}.get(ret)
    out = (C.c_int64 * 32)()
    n = lib.tpw_sg_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    w = case["w"]
    s.words(s.g("code"), (w["code"] + [0] * CODE_WORDS)[:CODE_WORDS])
    s.words(s.g("gScriptInitialised"), [w["init"]])
    s.words(s.g("gScriptTableSize"), [w["tsize"]])
    blocks = {}
    for i, img in enumerate(w["blocks"]):
        a = s.alloc(4 * BLOCK_WORDS)
        s.wr_block(a, img)
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
    if w["esp"]:
        for i, (first, count) in enumerate(w["groups"]):
            s.group(i, first, count)
        for i in range(NSPRITES):
            s.label(SPRITE_LABEL + i, s.sprite(i))
    if w["file"] is not None:
        s.file_in(w["file"])
    s.blocks = blocks
    s.table = table


def arg_values(s, case):
    """The case's arguments as words: ("b", label, offset) a block's address."""
    out = []
    for a in case["a"]:
        if isinstance(a, tuple):
            out.append(s.blocks[a[1]] + a[2] if a[1] else a[2])
        else:
            out.append(a)
    return out


def execute(s, case):
    name = case["fn"]
    if name.startswith("PK_"):
        kinds = ""
    else:
        kinds = SIG[name][0]
    r = s.call(name, kinds, arg_values(s, case))
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
    for a, size in fresh:
        if size == 4 * BLOCK_WORDS:
            known.append((len(known) + 1000, a))
    for name in ("gScriptInitialised", "gScriptTableSize", "gScriptCodeBase"):
        out[name] = s.rd_word(s.g(name))
    out["current"] = block_of(s, s.getptr("gScriptCurrent"), known)
    out["vars"] = block_of(s, s.getptr("gScriptVars"), known)
    tsize = s.rd_word(s.g("gScriptTableSize"))
    out["table"] = s.addr_label(s.getptr("gScriptTable"))
    out["entries"] = [s.entry_label(x) for x in s.rd_words(s.getptr("gScriptTable"), max(min(tsize, 256), 0))] if s.getptr("gScriptTable") else []
    out["oldtable"] = [s.entry_label(x) for x in s.rd_words(s.table, max(w["tsize"], 0))]
    out["blocks"] = {l: s.rd_block(a) for l, a in s.blocks.items()}
    out["fresh"] = [(size, s.rd_block(a) if size == 4 * BLOCK_WORDS else [s.entry_label(x) for x in s.rd_words(a, size // 4)])
                    for a, size in fresh]
    chunks = s.file_out()
    # the table chunk (the fourth write) holds Mac addresses or arena offsets: compare which words are zero
    out["out"] = [[1 if x else 0 for x in struct.unpack("<%dI" % (len(c) // 4), c)] if i == 3 else c for i, c in enumerate(chunks)]
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

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000, 0x7FA00000]


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
    """A script block's 70 words: random, with fields set by word index (f<index>)."""
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


def base(r, fn, nblocks=None, tsize=None, density=0.7, **kw):
    """The world every case starts from: the system on, the table with entries pointing at blocks."""
    nb = nblocks if nblocks is not None else r.randint(3, 6)
    ts = tsize if tsize is not None else r.randint(2, 12)
    w = {"code": [], "init": r.choice([1, 1, 1, 2, 0xFFFFFFFF, 0x80000000]), "tsize": ts, "blocks": [img(r) for _ in range(nb)],
         "esp": False, "groups": [(0, 0)] * 5, "codebase": CODE, "current": 0, "file": None}
    w["entries"] = [r.randint(1, nb) if r.random() < 0.25 else 0] + [r.randint(1, nb) if r.random() < density else 0 for _ in range(max(ts - 1, 0))]
    w["entries"] = w["entries"][:max(ts, 0)]
    w.update(kw)
    return {"fn": fn, "w": w, "a": [], "queues": {}}


def esp_world(r, c):
    """Sprite sets 0 to 4 with a first sprite and a count; every block names a set and a sprite inside the 32."""
    w = c["w"]
    w["esp"] = True
    w["groups"] = [(r.randrange(0, 20), r.choice([0, 1, 3, 5, 8])) for _ in range(5)]
    for b in w["blocks"]:
        b[F_VARS + 10] = r.randrange(0, 5)
        b[F_VARS + 11] = r.randrange(0, 12)
    c["queues"]["frames"] = [r.randrange(0, 256)]
    q = c["queues"]
    q["cd1"], q["cd2"], q["cd3"], q["cd4"] = [r.randrange(0, 4)], [rw(r)], [rw(r)], [rw(r)]
    q["cdmask"] = [r.choice([15, 15, 3, 7, 11])]


def handle_for(r, c):
    """A handle whose table entry holds a block (1 .. tsize - 1), else 1."""
    w = c["w"]
    good = [i for i, l in enumerate(w["entries"]) if i > 0 and l]
    return r.choice(good) if good else None


def with_entry(r, c):
    """Makes sure the table has an entry with a block and answers its handle."""
    w = c["w"]
    h = handle_for(r, c)
    if h is None:
        w["tsize"] = max(w["tsize"], 2)
        w["entries"] = [0, 1] + [0] * (w["tsize"] - 2)
        h = 1
    return h


# ---- ESP_ForceGoto / ESP_RURunningThis
def goto_case(r, fn, custom=None, match=None):
    c = base(r, fn)
    esp_world(r, c)
    h = with_entry(r, c)
    w = c["w"]
    blk = w["blocks"][w["entries"][h] - 1]
    blk[F_BASE] = r.choice([CODE, CODE, CODE + 4 * r.randrange(0, 5), 0x1000])
    is_custom = r.random() < 0.5 if custom is None else custom
    if is_custom:
        code = r.choice([0, 1, 2, 3])
        start = START_TABLE[c["queues"]["cd1"][0]]
    else:
        code = code_addr(r.choice([0, 1, 7, 100, CODE_WORDS - 1, r.randrange(0, CODE_WORDS)]))
        start = code
    if fn == "ESP_RURunningThis":
        if (r.random() < 0.5) if match is None else match:
            blk[F_START] = (start - CODE) // 4
        else:
            blk[F_START] = r.choice([(start - CODE) // 4 + r.choice([-1, 1, 2, 4]), r.randrange(0, 900), rw(r)]) & MASK
    c["a"] = [h if r.random() < 0.9 else r.choice([0, w["tsize"], w["tsize"] + 1, -1, 0x7FFFFFFF, 0x80000000]), code]
    return c


def goto_edges(fn):
    def gen():
        r = random.Random(2001 + len(fn))
        for init in (1, 0):
            for code in (0, 1, 2, 3, CODE, CODE + 4 * 787, CODE + 4 * 882):
                c = goto_case(r, fn, custom=code <= 3, match=True)
                c["w"]["init"] = init
                c["a"][1] = code
                yield c
        for first in range(4):
            c = goto_case(r, fn, custom=True, match=True)
            c["queues"]["cd1"] = [first]
            c["queues"]["cdmask"] = [15]
            yield c
        for h in (0, -1):
            c = goto_case(r, fn, custom=r.random() < 0.5)
            c["a"][0] = h
            yield c
        c = goto_case(r, fn)
        c["w"]["tsize"], c["w"]["entries"] = 1, [0]
        c["a"][0] = 1
        yield c
        for match in (True, False):
            yield goto_case(r, fn, custom=False, match=match)
    return gen


# ---- ESP_Spawn2DCopyOfScriptAt
def copy_case(r, fail=None):
    c = base(r, "ESP_Spawn2DCopyOfScriptAt")
    h = with_entry(r, c)
    w = c["w"]
    failed = (r.random() < 0.15) if fail is None else fail
    if failed:
        c["queues"]["alloc"] = [0]
        h = r.choice([h, 0, -1, w["tsize"] + 3])
    c["queues"]["assign"] = [r.choice([5, 0, 1, 99, MASK])]
    c["a"] = [h, rf(r), rf(r), rf(r), rf(r)]
    w["blocks"][w["entries"][1 if h not in range(len(w["entries"])) or not w["entries"][h] else h] - 1][F_VARS + 16] = r.choice(
        [0, MASK, 0x50000, 0x200, 0x50200, rw(r), 0x10000, 0xFFFFFDFF])
    return c


def copy_edges():
    r = random.Random(2010)
    for init in (0, 1):
        yield with_init(copy_case(r, False), init)
    yield copy_case(r, True)
    for v in (0, MASK, 0x200, 0x50000):
        c = copy_case(r, False)
        h = c["a"][0]
        c["w"]["blocks"][c["w"]["entries"][h] - 1][F_VARS + 16] = v
        yield c


def with_init(c, init):
    c["w"]["init"] = init
    return c


# ---- ESP_Update2DScript
def update_case(r):
    c = base(r, "ESP_Update2DScript")
    h1 = with_entry(r, c)
    w = c["w"]
    good = [i for i, l in enumerate(w["entries"]) if i > 0 and l]
    h2 = r.choice(good)
    c["a"] = [h1, h2]
    return c


def update_edges():
    r = random.Random(2020)
    for _ in range(3):
        c = update_case(r)
        c["a"][1] = c["a"][0]
        yield c
    c = update_case(r)
    c["w"]["init"] = 0
    yield c
    c = update_case(r)
    for b in c["w"]["blocks"]:
        b[F_VARS + 16] = MASK
        b[F_VARS + 7] = 0x7FA00000
        b[F_VARS + 9] = 0x7F800001
    yield c


# ---- ESP_FixSpriteIDsFromDetail
def fix_case(r):
    c = base(r, "ESP_FixSpriteIDsFromDetail", density=r.choice([0.6, 1.0]))
    for b in c["w"]["blocks"]:
        b[F_VARS + 10] = r.randrange(0, 14)
        b[F_VARS + 11] = r.choice([rw(r), r.randrange(0, 40), MASK, 0x80000000, 0x7FFFFFFF, r.randrange(0, 3)])
    c["queues"]["nsprites"] = [r.choice([3, 1, 2, 7, 10, 5, 100, MASK, 0x7FFFFFFF, 0x80000000, 0]) for _ in range(r.randint(1, 6))]
    return c


def fix_edges():
    r = random.Random(2030)
    for n in (1, 2, 3, 0, MASK, 0x80000000, 0x7FFFFFFF):
        c = fix_case(r)
        c["queues"]["nsprites"] = [n]
        yield c
    c = fix_case(r)
    c["w"]["init"] = 0
    yield c
    c = fix_case(r)
    c["w"]["blocks"][0][F_VARS + 11] = 0x80000000
    c["queues"]["nsprites"] = [MASK]
    yield c


# ---- the commands, called directly
def pk_case(r, name):
    c = base(r, "PK_" + name, nblocks=2)
    w = c["w"]
    w["current"] = 1
    pos = r.randrange(0, 40)
    d = r.choice([0, 0, 1, 4])
    w["codebase"] = CODE + 4 * d
    oppos = pos + d
    ops = {
        "SPAWN": [r.choice([code_addr(r.randrange(0, CODE_WORDS)), code_addr(r.choice([0, 1, 5, 40, 882])), 0])],
        "SET_OBJ_ID": [rw(r)], "SET_OBJ_ACT": [rw(r)],
        "PutSpriteVAR": [r.choice([r.randrange(0, 37), 13, 10, 11, 12, 14])],
        "PutSpriteIM": [r.choice([MASK, MASK, 0, 5, 0x7FFFFFFF, rw(r)])],
        "Put3DSpriteIM": [r.choice([MASK, 0, 5, 0x7FFFFFFF, rw(r)])],
    }.get(name, [])
    w["code"] = [rw(r)] * oppos + ops + [rw(r)] * 4
    blk = img(r, f2=pos, f5=w["codebase"])
    for i in range(7, 19):
        blk[F_VARS + i] = rw(r)
    for i in (1, 2, 3, 8, 9):
        blk[F_VARS + i] = rf(r)
    blk[F_SPRITE + 10] = rw(r)
    w["blocks"][0] = blk
    if name == "PutSpriteVAR" and r.random() < 0.5:
        blk[F_VARS + ops[0]] = MASK
    c["queues"]["spawn"] = [r.choice([7, 0, MASK])]
    return c


def pk_edges(name):
    def gen():
        r = random.Random(2100 + sum(map(ord, name)))
        for _ in range(3):
            yield pk_case(r, name)
        if name in ("PutSpriteIM", "Put3DSpriteIM"):
            for v in (MASK, 0, 1, 0xFFFFFFFE):
                c = pk_case(r, name)
                c["w"]["code"][c["w"]["blocks"][0][F_POS] + (c["w"]["codebase"] - CODE) // 4] = v
                yield c
        if name == "PutSpriteVAR":
            for idx in (0, 13, 36):
                for val in (MASK, 0, 7):
                    c = pk_case(r, name)
                    pos = c["w"]["blocks"][0][F_POS] + (c["w"]["codebase"] - CODE) // 4
                    c["w"]["code"][pos] = idx
                    c["w"]["blocks"][0][F_VARS + idx] = val
                    yield c
        if name in ("ResetScale", "ReduceScale", "IncreaseScale", "ResetRotation", "ReduceRotation", "IncreaseRotation"):
            for v in SPECIAL + [fb(0.01), fb(0.005), fb(1.0), fb(-0.01), 0x3C23D70A]:
                c = pk_case(r, name)
                c["w"]["blocks"][0][F_VARS + (8 if "Scale" in name else 9)] = v
                yield c
    return gen


# ---- SCR_SaveAllScripts
def save_case(r, tsize=None, init=None):
    c = base(r, "SCR_SaveAllScripts", tsize=tsize, density=r.choice([0.3, 0.7, 1.0]))
    c["a"] = [FILE]
    if init is not None:
        c["w"]["init"] = init
    for b in c["w"]["blocks"]:
        b[SPECIAL_W] = r.getrandbits(32)
    return c


def save_edges():
    r = random.Random(2200)
    for init in (1, 0):
        yield save_case(r, 4, init)
    for ts in (0, 1, 2, 3, 12):
        yield save_case(r, ts)
    c = save_case(r, 5)
    c["w"]["entries"] = [0, 0, 0, 0, 0]
    yield c


# ---- SCR_LoadAllScripts
def record(r, patch=None):
    ws = [rw(r) for _ in range(BLOCK_WORDS)]
    if patch == "hit":
        ws[F_POS], ws[F_LAST] = 0x61, 0x5F
    elif patch == "near":
        ws[F_POS], ws[F_LAST] = r.choice([(0x61, 0x5E), (0x60, 0x5F), (0x5F, 0x5F), (0x61, 0x61), (0x62, 0x5F)])
    return struct.pack("<%dI" % BLOCK_WORDS, *ws)


def load_case(r, tsize=None, tag=0x53435054, size=0x118, init=None, short=None, fail=None):
    c = base(r, "SCR_LoadAllScripts", density=r.choice([0.4, 0.8, 1.0]))
    c["a"] = [FILE]
    w = c["w"]
    if init is not None:
        w["init"] = init
    ts = tsize if tsize is not None else r.choice([1, 2, 3, 5, 8, 12, 40])
    table = [0] + [r.choice([0, r.randint(1, MASK)]) if r.random() < 0.7 else r.randrange(1, 9) for _ in range(ts - 1)]
    table[0] = r.choice([0, 0, r.getrandbits(32)])
    data = struct.pack("<III", tag, size, ts) + struct.pack("<%dI" % ts, *table)
    for i in range(1, ts):
        if table[i]:
            data += record(r, r.choice([None, None, "hit", "near", "hit"]))
    if short is not None:
        data = data[:short]
    elif r.random() < 0.05:
        data = data[:r.randrange(0, len(data))]
    w["file"] = data
    if fail is not None:
        c["queues"]["alloc"] = [1] * fail + [0]
    elif r.random() < 0.15 and tag == 0x53435054 and size == 0x118:
        c["queues"]["alloc"] = [1] * r.randrange(0, 4) + [0]
    return c


def load_edges():
    r = random.Random(2300)
    yield load_case(r, init=0)
    yield load_case(r, tag=0x53435055)
    yield load_case(r, tag=0)
    yield load_case(r, size=0x117)
    yield load_case(r, size=0x119)
    yield load_case(r, size=0)
    for ts in (1, 2, 3, 12):
        yield load_case(r, ts, fail=None)
    yield load_case(r, 4, fail=0)
    yield load_case(r, 6, fail=1)
    yield load_case(r, 6, fail=3)
    yield load_case(r, 3, short=0)
    yield load_case(r, 3, short=8)
    yield load_case(r, 3, short=14)
    c = load_case(r, 3)
    c["w"]["tsize"], c["w"]["entries"] = 1, [0]
    yield c
    c = load_case(r, 4)
    c["w"]["entries"] = [1] * c["w"]["tsize"]
    c["w"]["entries"][0] = 1
    yield c


# ---- the file swaps, called on raw bytes (the same on both sides; each side reads them in its own order)
def swap_case(r, fn):
    n = {"SpriteRecordByteswap": 0x44, "USwapBlock32SLONGfloat": 0x13 * 4, "USwapBlock32Long": 0x14 * 4}.get(fn, 4)
    kind = r.random()
    if kind < 0.15:
        raw = bytes([r.choice([0, 0xFF, 0x80, 0x7F, 1])] * n)
    else:
        raw = bytes(r.getrandbits(8) for _ in range(n))
    count = {"USwapBlock32SLONGfloat": r.choice([0x13, 0x13, 1, 0, 5]), "USwapBlock32Long": r.choice([0x14, 0x14, 1, 0, 3])}.get(fn, 0)
    return {"fn": fn, "raw": raw, "count": count, "queues": {}}


def swap_edges(fn):
    def gen():
        r = random.Random(2400 + len(fn))
        for fill in (0, 0xFF, 0x80, 1):
            c = swap_case(r, fn)
            c["raw"] = bytes([fill]) * len(c["raw"])
            yield c
        c = swap_case(r, fn)
        c["raw"] = bytes(range(1, len(c["raw"]) + 1))
        yield c
    return gen


def run_swap(s, case):
    try:
        s.L_log = None
        fn, raw = case["fn"], case["raw"]
        a = s.alloc(len(raw) + 8)
        s.write(a, raw + bytes([0xEE] * 8))
        out = {}
        if fn in ("USwap32LongPtr", "USwap32CharPtr"):
            out["ret"] = s.call(fn, "u", [s.rd_word(a)])
        elif fn.startswith("USwapBlock32"):
            s.call(fn, "pi", [a, case["count"]])
        else:
            s.call(fn, "p", [a])
        n = len(raw) // 4
        if fn == "SpriteRecordByteswap":
            out["words"] = s.rd_words(a, 14)
            out["half"] = struct.unpack(s.fmt[0] + "H", s.read(a + 0x38, 2))[0]
            out["tail"] = s.read(a + 0x3A, 6)
            out["last"] = s.rd_words(a + 0x40, 1)
        elif fn.startswith("USwapBlock32"):
            k = max(min(case["count"], n), 0)
            out["words"] = s.rd_words(a, k)
            out["rest"] = s.read(a + 4 * k, len(raw) - 4 * k)
        elif fn.startswith("USwapInplace"):
            out["words"] = s.rd_words(a, n)
        out["guard"] = s.read(a + len(raw), 8)
        out["log"] = s.log()
        return out
    finally:
        s.done()


def ppc_swap(e, case):
    return run_swap(Ppc(e, case), case)


def host_swap(lib, case):
    return run_swap(Host(lib, case), case)


PKS = list(PK_INDEX)
TESTS = [
    FnTest("ESP_ForceGoto", ppc, host, edge=goto_edges("ESP_ForceGoto"), rnd=lambda r: goto_case(r, "ESP_ForceGoto")),
    FnTest("ESP_RURunningThis", ppc, host, edge=goto_edges("ESP_RURunningThis"), rnd=lambda r: goto_case(r, "ESP_RURunningThis")),
    FnTest("ESP_Spawn2DCopyOfScriptAt", ppc, host, edge=copy_edges, rnd=copy_case),
    FnTest("ESP_Update2DScript", ppc, host, edge=update_edges, rnd=update_case),
    FnTest("ESP_FixSpriteIDsFromDetail", ppc, host, edge=fix_edges, rnd=fix_case),
] + [FnTest("PK_" + n, ppc, host, edge=pk_edges(n), rnd=(lambda n: lambda r: pk_case(r, n))(n)) for n in PKS] + [
    FnTest("SCR_SaveAllScripts", ppc, host, edge=save_edges, rnd=save_case),
    FnTest("SCR_LoadAllScripts", ppc, host, edge=load_edges, rnd=load_case),
] + [FnTest(n, ppc_swap, host_swap, edge=swap_edges(n), rnd=(lambda n: lambda r: swap_case(r, n))(n))
     for n in ("USwapInplace32Int", "USwapInplace32Long", "USwapBlock32SLONGfloat", "USwapBlock32Long", "USwap32LongPtr", "USwap32CharPtr",
               "SpriteRecordByteswap")]
