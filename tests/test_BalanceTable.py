"""Differential test for src/game/balance/BalanceLoader.cpp (the balance table reader: Init, UnInit,
FreeStrings, Load in both forms, Parse, LocateBalanceTableRow, AssignValue in both forms,
InitialiseBalanceValues, CountBalanceDescriptionRows and the line readers) and
src/game/advisor/Advisor.cpp, against libtpw_unit_BalanceTable.dylib and libtpw_unit_GameAssert.dylib.

A table case is a script of operations on one table object that both sides run: the real
table classes (the six of balance_support.UNITS, with the real .sam files read from
ROOT/extracted at run time) or a synthetic table made of description rows the test writes.
After every operation the test compares the result, the row information Init made (as slot
numbers: the Mac's three words and the host's three pointers), every value slot (strings
and maps by content), the output texts and the call log: GAME_ASSERTMSG's messages, the
heap (LbMem_Alloc and LbMem_Free), the file calls, CExitError::SetError and, for a
synthetic table, the virtual calls in order. The original runs on the PowerPC side; its
callees are the fakes below.

Assumptions about code outside the unit (the same on both sides, none checked against the
real library): MSL's character classes are ASCII only in the C locale; __strtoul stops at
2^32 - 1 on overflow and the reader applies the sign to that; atoi saturates; atof reads
the plain decimal numbers the readers let through exactly as Python's float does.
"""
import ctypes as C
import pathlib
import random
import re
import struct

import calllog as CL
import tpwtest as T
from balance_support import UNITS
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_BalanceTable"

ROOT = pathlib.Path(__file__).resolve().parents[2]
EXTRACTED = ROOT / "extracted"

# The functions of the unit (all run as the original: other modules' stubs on them are lifted).
INIT, UNINIT, FREE_STRINGS = 0x1001EA28, 0x1001ED40, 0x1001EDB8
READ_ARRAY_ROW, READ_FIELD_NAMES, READ_INTEGER, READ_FLOAT = 0x1001EE20, 0x1001EF10, 0x1001F060, 0x1001F178
READ_STRING, RECOGNISE_DELIMIT, READ_MAP = 0x1001F2A4, 0x1001F414, 0x1001F4C0
LOCATE, ASSIGN_INT, ASSIGN_FLOAT, INITIALISE = 0x1001F8F8, 0x1001FB5C, 0x1001FC68, 0x1001FD7C
LOAD_PATH, LOAD_FILE, PARSE, COUNT = 0x10020134, 0x100202C0, 0x10020444, 0x10020FD0
OWN = (INIT, UNINIT, FREE_STRINGS, READ_ARRAY_ROW, READ_FIELD_NAMES, READ_INTEGER, READ_FLOAT, READ_STRING,
       RECOGNISE_DELIMIT, READ_MAP, LOCATE, ASSIGN_INT, ASSIGN_FLOAT, INITIALISE, LOAD_PATH, LOAD_FILE, PARSE, COUNT)
ASSERT = 0x1000DDE8                 # GAME_ASSERTMSG: a fake here, tested on its own
SET_ERROR = 0x1014B62C              # CExitError::SetError
EXIT_ERROR = 0x1037F1C0             # gExitError
SYMBOLS_A, SYMBOLS_B = 0x1028E730, 0x1028E814   # the two map symbol tables (19 and 16 keys)

TABLE_LABEL, FILE_LABEL, EXIT_LABEL = 1, 2, 3
# A group name of 64 characters fills Parse's 64-byte buffer to the last byte: the original's
# messages then run on into the next stack word (here one byte, 0x7d, of the line pointer) and
# the host's buffer ends there. The test removes that byte from the PowerPC side's messages.
GROUP_64 = "G" * 64
SENTINEL = 0x7A7A7A7A               # an output the reader did not write
NAMES = ("Hoardings", "Main", "Online", "Options", "Sound", "Resolution")

STUBS = None
STUBS_REAL_ASSERT = None
SYNTH_VT = 0
FILE_VT = 0
# Where Parse keeps the map error message pointer in its frame (r1 - 0x15d0 + 0xc3c from the
# stack pointer a call starts with), read by a failed map line even when never set.
PARSE_ERROR_SLOT = T.ppcemu.STACK_BASE + T.ppcemu.STACK_SIZE - 0x1000 - 0x400 - 0x15D0 + 0xC3C


def F(lib, name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype, fn.argtypes = restype, list(argtypes)
    return fn


# ================================================================== the tables


class Table:
    """What both sides build a table from: kind 0 to 5 a real class (balance_support.UNITS
    order), 6 a synthetic table. rows: the description rows as tuples (type, name, min
    integer, max integer, min float, max float, array count, flags), the terminator last;
    slots: the value slots including slot 0; beyond: the row every number past the terminator
    answers with (a synthetic table; the default is the terminator again)."""

    def __init__(self, kind, rows, slots, name, beyond=None):
        self.kind, self.rows, self.slots, self.name, self.beyond = kind, rows, slots, name, beyond

    @property
    def n(self):
        return len(self.rows) - 1       # rows with row information


def row(kind, name="", min_i=0, max_i=0, min_f=0.0, max_f=0.0, count=0, flags=0):
    return (kind, name, min_i, max_i, min_f, max_f, count, flags)


def pack_row(r, endian):
    return struct.pack(endian + "i32siiffiI", r[0], r[1].encode("latin-1"), r[2], r[3], r[4], r[5], r[6], r[7])


def unpack_row(raw):
    t, name, min_i, max_i, min_f, max_f, count, flags = struct.unpack(">i32siiffiI", raw)
    return (t, name.split(b"\0")[0].decode("latin-1"), min_i, max_i, min_f, max_f, count, flags)


REAL = []


def real_tables(e):
    out = []
    for i, u in enumerate(UNITS):
        rows = [unpack_row(e.read(u["table"] + 60 * k, 60)) for k in range(u["rows"])]
        out.append(Table(i, rows, u["count"], NAMES[i]))
    return out


# Every kind of row once, an array group (which must follow a heading directly: Init's reset
# reads a type 2 only at the start of a run) and a second plain group. Slots: 1 to 12 the
# first group, 13 to 36 the array (six rows of four elements), 37 its count, 38 the last group.
EDGE_ROWS = [
    row(0),
    row(4, "Plain"), row(5, "NonNeg"), row(6, "Range", -5, 10), row(7, "Real"), row(8, "NonNegReal"),
    row(9, "RealRange", 0, 0, -1.5, 2.5), row(10, "Text"),
    row(11, "Shape", flags=1), row(11, "Shape2", flags=2), row(11, "Shape0", flags=0), row(11, "ShapeX", flags=7),
    row(11, "ShapeNeg", flags=0xFFFFFFFF),
    row(1, "Scalars"),
    row(2), row(4, "ArrInt"), row(6, "ArrRange", 0, 100), row(7, "ArrFloat"), row(8, "ArrNonNeg"), row(10, "ArrText"),
    row(11, "ArrShape", flags=1), row(3, "Items", count=4),
    row(0),
    row(4, "Other"), row(1, "Misc"),
    row(12),
]
EDGE = Table(6, EDGE_ROWS, 42, "Edge")


# ================================================================== the PowerPC side


def ctype_map():
    """MSL's C-locale __ctype_map: 0x01 control, 0x04 white space, 0x08 punctuation, 0x10 digit,
    0x20 hex digit, 0x40 lower case, 0x80 upper case (as test_FSUserInfo writes it)."""
    table = bytearray(256)
    for c in range(128):
        ch = chr(c)
        if c < 0x20 or c == 0x7F:
            table[c] |= 0x01
        if ch in " \t\n\v\f\r":
            table[c] |= 0x04
        if 0x21 <= c <= 0x7E and not ch.isalnum():
            table[c] |= 0x08
        if ch.isdigit():
            table[c] |= 0x30
        if ch in "abcdefABCDEF":
            table[c] |= 0x20
        if "a" <= ch <= "z":
            table[c] |= 0x40
        if "A" <= ch <= "Z":
            table[c] |= 0x80
    return bytes(table)


def c_atoi(text):
    """atoi of the Mac's library: leading white space, a sign, digits; saturates."""
    m = re.match(rb"[ \t\n\v\f\r]*([+-]?)(\d*)", text)
    value = int(m.group(2) or b"0") * (-1 if m.group(1) == b"-" else 1)
    return max(-0x80000000, min(0x7FFFFFFF, value))


def c_atof(text):
    """atof for what read_float_value lets through: [-]digits[.digits], or less."""
    m = re.match(rb"-?(\d+\.?\d*|\.\d+)", text)
    return float(m.group(0)) if m else 0.0


def build_stubs():
    S = CL.Stubs()
    S.real.update(OWN)
    S.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "is"))
    S.internal(SET_ERROR, CL.recorder("CExitError::SetError", "Lius"))

    def alloc(e):
        # LbMem_Alloc: a block of 0xa5 (the queue "LbMem_Alloc" refuses with 0)
        size, L = e.r(3), e.cl
        if L.ret("LbMem_Alloc", 1) == 0:
            L.rec("LbMem_Alloc", (size, 0))
            e.set_r(3, 0)
            return
        block = e.alloc(max(size, 1), b"\xa5" * max(size, 1))
        L.rec("LbMem_Alloc", (size, L.newlab(block)))
        e.set_r(3, block)

    def free(e):
        L = e.cl
        L.rec("LbMem_Free", (L.lab(e.r(3)),))
        L.forget(e.r(3))

    def get_length(e):
        L = e.cl
        L.rec("LbFile_GetLength", (), (e.cstr(e.r(3))[:159],))
        e.set_r(3, T.u32(L.ret("LbFile_GetLength", -1)))

    def load(e):
        # LbFile_Load: the TbError comes back in the buffer in r3; the bytes fill the block
        L = e.cl
        hidden, name, buffer, capacity, length = e.r(3), e.cstr(e.r(4))[:159], e.r(5), e.r(6), e.r(7)
        L.rec("LbFile_Load", (capacity,), (name,))
        data = e.file_bytes[:capacity]
        if data:
            e.write(buffer, data)
        e.write(length, struct.pack(">i", len(data)))
        e.write(hidden, struct.pack(">i", L.ret("LbFile_Load", 0)))

    def strtoul(e):
        # __strtoul(10, max, read, &{text, 0}, &scanned, &negative, &overflow)
        text = e.cstr(e.u32(e.r(6)), limit=1 << 16).encode("latin-1")
        negative = text.startswith(b"-")
        digits = re.match(rb"\d*", text[1:] if negative else text).group(0)
        value = min(int(digits or b"0"), 0xFFFFFFFF)
        e.write(e.r(7), struct.pack(">i", len(digits)))
        e.write(e.r(8), struct.pack(">i", 1 if negative else 0))
        e.write(e.r(9), struct.pack(">i", 1 if len(digits) and int(digits) > 0xFFFFFFFF else 0))
        e.set_r(3, value)

    def strcmp(e):
        a, b = e.cstr(e.r(3), limit=1 << 16), e.cstr(e.r(4), limit=1 << 16)
        e.set_r(3, T.u32((a > b) - (a < b)))

    def strcat(e):
        # a null source adds nothing (the original's strcat of a null or unset message pointer
        # after a failed map is undefined; the host leaves the message as it is)
        dst, source = e.r(3), e.r(4)
        text = e.cstr(dst, limit=1 << 16) + (e.cstr(source, limit=1 << 16) if source else "")
        e.write(dst, text.encode("latin-1") + b"\0")

    S.imp("LbMem_Alloc__FUi", alloc)
    S.imp("LbMem_Free__FPv", free)
    S.imp("LbFile_GetLength__FPCc", get_length)
    S.imp("LbFile_Load__FPCcPvUlPl", load)
    S.imp("atoi", lambda e: e.set_r(3, T.u32(c_atoi(e.cstr(e.r(3), limit=1 << 16).encode("latin-1")))))
    S.imp("atof", lambda e: e.set_f1(c_atof(e.cstr(e.r(3), limit=1 << 16).encode("latin-1"))))
    S.imp("__strtoul", strtoul)
    S.imp("strcmp", strcmp)
    S.imp("strcat", strcat)
    return S


def synthetic_vtable(e):
    """A persistent vtable whose two slots answer for a synthetic table: description rows
    from emu.bt (set per case), the values from the object's own value slots."""
    def description(emu):
        row_number = T.s32(emu.r(4))
        emu.cl.rec("GetBalanceDescription", (row_number,))
        last = emu.bt["n"] - 1
        if row_number > last and emu.bt["beyond"]:
            emu.set_r(3, emu.bt["beyond"])
        else:
            emu.set_r(3, emu.bt["desc"] + 60 * (last if row_number < 0 or row_number > last else row_number))

    def values(emu):
        emu.cl.rec("GetBalanceValuesStart", ())
        emu.set_r(3, emu.r(3) + 8)

    vt = T.persist_alloc(e, 0x10)
    e.write(vt + 8, struct.pack(">II", T.fake_function(e, description), T.fake_function(e, values)))
    return vt


def file_vtable(e):
    """The vtable of the open file the Load overload with a file reads (slot 0x10 Read, 0x1c
    GetSize), answering from the case's queues."""
    def read(emu):
        L, size = emu.cl, emu.r(5)
        L.rec("CFile::Read", (L.lab(emu.r(3)), size))
        data = emu.file_bytes[:size]
        if data:
            emu.write(emu.r(4), data)
        emu.set_r(3, T.u32(L.ret("CFile::Read", size)))

    def get_size(emu):
        L = emu.cl
        L.rec("CFile::GetSize", (L.lab(emu.r(3)),))
        emu.set_r(3, T.u32(L.ret("CFile::GetSize", 0)))

    vt = T.persist_alloc(e, 0x40)
    e.write(vt + 0x10, struct.pack(">I", T.fake_function(e, read)))
    e.write(vt + 0x1C, struct.pack(">I", T.fake_function(e, get_size)))
    return vt


def setup(runner):
    global STUBS, STUBS_REAL_ASSERT, SYNTH_VT, FILE_VT
    e = runner.emu
    T.install_std_stubs(e)
    e.write(e.data_import("__ctype_map"), ctype_map())
    e.write(e.data_import("__wctype_map"), b"".join(struct.pack(">H", v) for v in ctype_map()))
    REAL[:] = real_tables(e)
    SYNTH_VT, FILE_VT = synthetic_vtable(e), file_vtable(e)
    STUBS = build_stubs()
    STUBS_REAL_ASSERT = CL.Stubs()
    STUBS_REAL_ASSERT.real.add(ASSERT)


def slot_index(address, base):
    return -1 if address == 0 else (address - base) // 4


def decode_shape(data, endian):
    """A map block as numbers: width, height and each cell's value, flag and padding."""
    width, height = struct.unpack(endian + "ii", data[:8])
    cells = []
    for i in range(400):
        value, flag, pad = struct.unpack(endian + "iB3s", data[8 + 8 * i:16 + 8 * i])
        cells.append((value, flag, pad))
    return (width, height, cells)


def pointer_slots(t, rows):
    """The slots that hold a string (10) or a map (11): from the row information and the
    array capacities in the description rows."""
    out = {}
    for i in range(t.n):
        kind, value, stride = t.rows[i][0], rows[i][0], rows[i][1]
        if kind not in (10, 11) or value < 0:
            continue
        capacity = 1
        if stride != 0:
            for j in range(i + 1, len(t.rows)):
                if t.rows[j][0] == 3:
                    capacity = t.rows[j][6]
                    break
        for k in range(max(capacity, 1)):
            if 0 <= value + k * stride < t.slots:
                out[value + k * stride] = kind
    return out


def ppc_make(e, t):
    """The table as raw memory: row pointer and slots all 0xa5, the vtable of its class."""
    if t.kind < 6:
        vtable = UNITS[t.kind]["vt"]
    else:
        vtable = SYNTH_VT
        desc = e.alloc(60 * len(t.rows), b"".join(pack_row(r, ">") for r in t.rows))
        beyond = e.alloc(60, pack_row(t.beyond, ">")) if t.beyond else 0
        e.bt = {"desc": desc, "n": len(t.rows), "beyond": beyond}
    return e.alloc(8 + 4 * t.slots, struct.pack(">II", 0xA5A5A5A5, vtable) + b"\xa5" * (4 * t.slots))


def ppc_state(e, obj, t):
    rows_ptr = e.u32(obj)
    if rows_ptr in (0, 0xA5A5A5A5):
        rows = [(-3, -3, -3)] * t.n
    else:
        rows = []
        for i in range(t.n):
            words = struct.unpack(">3I", e.read(rows_ptr + 12 * i, 12))
            if words == (0xA5A5A5A5,) * 3:
                rows.append((-2, -2, -2))
            else:
                rows.append((slot_index(words[0], obj + 8), T.s32(words[1]), slot_index(words[2], obj + 8)))
    kinds = pointer_slots(t, rows)
    slots = []
    for i, word in enumerate(struct.unpack(">%dI" % t.slots, e.read(obj + 8, 4 * t.slots))):
        if i in kinds and word not in (0, 0xA5A5A5A5):
            slots.append(e.cstr(word, limit=1 << 16) if kinds[i] == 10 else decode_shape(e.read(word, 0xC88), ">"))
        else:
            slots.append(None if i in kinds and word == 0 else word)
    return dict(rows=rows, slots=slots)


def ppc_seed(e, obj, op):
    _, slot, kind, text, value = op
    address = obj + 8 + 4 * slot
    if kind == 0:
        e.write(address, struct.pack(">i", value))
    elif kind == 1:
        block = e.alloc(len(text) + 1, text.encode("latin-1") + b"\0")
        e.cl.newlab(block)
        e.write(address, struct.pack(">I", block))
    else:
        block = e.alloc(0xC88, bytes([value & 0xFF]) * 0xC88)
        e.cl.newlab(block)
        e.write(address, struct.pack(">I", block))


def ppc_op(e, st, op):
    name, obj, t = op[0], st["obj"], st["table"]
    ret, out = None, {}
    if name == "init":
        T.call_ppc(e, INIT, [obj])
    elif name == "uninit":
        T.call_ppc(e, UNINIT, [obj])
    elif name == "free":
        T.call_ppc(e, FREE_STRINGS, [obj])
    elif name == "initialise":
        T.call_ppc(e, INITIALISE, [obj, op[1]])
    elif name == "count":
        ret = T.s32(T.call_ppc(e, COUNT, [obj])[0])
    elif name == "seed":
        ppc_seed(e, obj, op)
    elif name == "locate":
        _, group, field, in_array, index = op
        out_row = e.alloc(4, struct.pack(">i", SENTINEL))
        ret = T.s32(T.call_ppc(e, LOCATE, [obj, CL.new_text(e, group), CL.new_text(e, field), out_row, in_array, index])[0])
        out["row"] = e.s32(out_row)
    elif name == "assign_int":
        _, slot, row_number, value = op
        ret = T.s32(T.call_ppc(e, ASSIGN_INT, [obj, obj + 8 + 4 * slot, row_number, value])[0])
    elif name == "assign_float":
        _, slot, row_number, bits = op
        value = struct.unpack(">f", struct.pack(">I", bits))[0]
        ret = T.s32(T.call_ppc(e, ASSIGN_FLOAT, [obj, obj + 8 + 4 * slot, row_number, T.F(value)])[0])
    elif name == "parse":
        _, source, text, mode = op
        buffer = e.alloc(len(text) + 1, text + b"\0")
        e.write(PARSE_ERROR_SLOT, b"\0\0\0\0")
        ret = T.s32(T.call_ppc(e, PARSE, [obj, CL.new_text(e, source), buffer, mode])[0])
        out["text"] = e.read(buffer, len(text) + 1)
    elif name == "load_path":
        _, path, mode = op
        e.write(PARSE_ERROR_SLOT - 0x60, b"\0\0\0\0")
        ret = T.s32(T.call_ppc(e, LOAD_PATH, [obj, CL.new_text(e, path), mode])[0])
    elif name == "load_file":
        _, source, mode = op
        e.write(PARSE_ERROR_SLOT - 0x60, b"\0\0\0\0")
        ret = T.s32(T.call_ppc(e, LOAD_FILE, [obj, CL.new_text(e, source), st["file"], mode])[0])
    else:
        raise ValueError(name)
    log = [(n, i, tuple(x.replace(GROUP_64 + "}", GROUP_64) for x in texts)) for n, i, texts in e.cl.log[st["mark"]:]]
    st["mark"] = len(e.cl.log)
    result = dict(op=name, ret=ret, log=log, **out)
    result.update(ppc_state(e, obj, t))
    return result


def ppc_run(e, case):
    saved = STUBS.activate(e)
    try:
        t = case["table"]
        obj = ppc_make(e, t)
        file = e.alloc(8, struct.pack(">II", FILE_VT, 0))
        e.cl = CL.PpcLog({obj: TABLE_LABEL, file: FILE_LABEL, EXIT_ERROR: EXIT_LABEL}, case.get("queues"))
        e.file_bytes = case.get("file", b"")
        st = dict(obj=obj, table=t, file=file, mark=0)
        return dict(ops=[ppc_op(e, st, op) for op in case["ops"]])
    finally:
        CL.Stubs.deactivate(e, saved)


# ================================================================== the host side


def host_make(lib, t):
    rows = b"".join(pack_row(r, "<") for r in t.rows)
    buffer = C.create_string_buffer(rows, len(rows))
    beyond = C.create_string_buffer(pack_row(t.beyond, "<"), 60) if t.beyond else None
    return F(lib, "tpw_bt_make", C.c_void_p, C.c_int32, C.c_void_p, C.c_int32, C.c_int32, C.c_void_p)(
        t.kind, buffer, len(t.rows), t.slots, beyond)


def host_log(lib):
    n = C.c_int32.in_dll(lib, "tpw_cl_n").value
    log = (CL.TpwCallEvent * 4096).in_dll(lib, "tpw_cl_log")
    out = []
    for i in range(min(n, 4096)):
        ev = log[i]
        out.append((ev.name.decode("latin-1"), tuple(ev.ints[k] for k in range(ev.nints)),
                    tuple(ev.texts[k].value.decode("latin-1") for k in range(ev.ntexts))))
    return out


def host_state(lib, h, t):
    rows = (C.c_int64 * max(3 * t.n, 1))()
    slots = (C.c_uint64 * t.slots)()
    F(lib, "tpw_bt_dump", None, C.c_void_p, C.c_int32, C.c_int32, C.c_void_p, C.c_void_p)(h, t.n, t.slots, rows, slots)
    info = [tuple(rows[3 * i:3 * i + 3]) for i in range(t.n)]
    kinds = pointer_slots(t, info)
    out = []
    for i in range(t.slots):
        if i in kinds and slots[i] not in (0, 0xA5A5A5A5A5A5A5A5):
            out.append(C.string_at(slots[i]).decode("latin-1") if kinds[i] == 10 else decode_shape(C.string_at(slots[i], 0xC88), "<"))
        else:
            out.append(None if i in kinds and slots[i] == 0 else slots[i] & 0xFFFFFFFF)
    return dict(rows=info, slots=out)


def host_op(lib, h, t, op, st):
    name = op[0]
    ret, out = None, {}
    call = lambda fn, restype, *argtypes: F(lib, "tpw_bt_" + fn, restype, C.c_void_p, *argtypes)   # noqa: E731
    if name in ("init", "uninit", "free"):
        call({"init": "init", "uninit": "uninit", "free": "free_strings"}[name], None)(h)
    elif name == "initialise":
        call("initialise", None, C.c_int32)(h, op[1])
    elif name == "count":
        ret = call("count", C.c_int32)(h)
    elif name == "seed":
        _, slot, kind, text, value = op
        call("seed", None, C.c_int32, C.c_int32, C.c_char_p, C.c_int32)(h, slot, kind, text.encode("latin-1"), value)
    elif name == "locate":
        _, group, field, in_array, index = op
        out_row = C.c_int32(SENTINEL)
        ret = call("locate", C.c_int32, C.c_char_p, C.c_char_p, C.c_void_p, C.c_int32, C.c_int32)(
            h, group.encode("latin-1"), field.encode("latin-1"), C.byref(out_row), in_array, index)
        out["row"] = out_row.value
    elif name == "assign_int":
        _, slot, row_number, value = op
        ret = call("assign_int", C.c_int32, C.c_int32, C.c_int32, C.c_int32)(h, slot, row_number, value)
    elif name == "assign_float":
        _, slot, row_number, bits = op
        ret = call("assign_float", C.c_int32, C.c_int32, C.c_int32, C.c_uint32)(h, slot, row_number, bits)
    elif name == "parse":
        _, source, text, mode = op
        buffer = C.create_string_buffer(text, len(text) + 1)
        ret = call("parse", C.c_int32, C.c_char_p, C.c_void_p, C.c_int32)(h, source.encode("latin-1"), buffer, mode)
        out["text"] = bytes(buffer.raw[:len(text) + 1])
    elif name == "load_path":
        _, path, mode = op
        ret = call("load_path", C.c_int32, C.c_char_p, C.c_int32)(h, path.encode("latin-1"), mode)
    elif name == "load_file":
        _, source, mode = op
        ret = call("load_file", C.c_int32, C.c_char_p, C.c_int32)(h, source.encode("latin-1"), mode)
    else:
        raise ValueError(name)
    log = host_log(lib)[st["mark"]:]
    st["mark"] += len(log)
    if name == "init":
        log = scale_row_allocation(lib, t, log)
    result = dict(op=name, ret=ret, log=log, **out)
    result.update(host_state(lib, h, t))
    return result


def scale_row_allocation(lib, t, log):
    """Init's first allocation is the row information: 12 bytes a row on the Mac, the host's
    BalanceRowInfo (native pointers) a row here. Report it in the Mac's size."""
    size = F(lib, "tpw_bt_row_info_size", C.c_int32)()
    out = list(log)
    for i, event in enumerate(out):
        if event[0] == "LbMem_Alloc":
            if event[1][0] == size * t.n:
                out[i] = (event[0], (12 * t.n,) + tuple(event[1][1:]), event[2])
            break
    return out


def host_run(lib, case):
    t = case["table"]
    h = host_make(lib, t)
    try:
        F(lib, "tpw_bt_set_file_text", None, C.c_char_p, C.c_int32)(case.get("file", b""), len(case.get("file", b"")))
        CL.host_begin(lib, {TABLE_LABEL: h, FILE_LABEL: F(lib, "tpw_bt_file", C.c_void_p)(),
                            EXIT_LABEL: F(lib, "tpw_bt_exit_error", C.c_void_p)()}, case.get("queues"))
        st = dict(mark=0)
        return dict(ops=[host_op(lib, h, t, op, st) for op in case["ops"]])
    finally:
        CL.host_end(lib)
        F(lib, "tpw_bt_destroy", None, C.c_void_p)(h)


# ================================================================== cases: tables and lines


def layout(t):
    """The slot numbering Init gives a table, as a model for the generators: row -> (slot,
    stride, count slot or None, capacity), and the number of slots used."""
    out, slot, stride, count, count_slot = {}, 1, 0, 0, None
    for i in range(t.n):
        kind = t.rows[i][0]
        if kind == 3:
            slot += (count - 1) * stride + 1
            stride, count, count_slot = 0, 0, None
        elif kind == 2:
            end = i + 1
            while end < t.n and t.rows[end][0] != 3 and not 0 <= t.rows[end][0] <= 2:
                end += 1
            stride, count = end - i - 1, t.rows[end][6]
            count_slot = slot + stride * count
        elif 4 <= kind <= 11:
            out[i] = (slot, stride, count_slot, count if count_slot is not None else 1)
            slot += 1
    return out, slot


def table_groups(t):
    """The groups a table reads: (name, members, array, capacity); a member is (name, type, row)."""
    groups, members = [], []
    for i in range(t.n):
        kind = t.rows[i][0]
        if kind in (0, 2):
            members = []
        elif kind in (1, 3):
            groups.append((t.rows[i][1], members, kind == 3, t.rows[i][6]))
            members = []
        elif 4 <= kind <= 11:
            members.append((t.rows[i][1], kind, t.rows[i]))
    return groups


def random_member(r, name):
    kind = r.choice([4, 4, 5, 5, 6, 6, 7, 8, 9, 9, 10, 10, 11])
    if kind == 6:
        low = r.choice([-10, -3, 0, 1])
        return row(6, name, low, low + r.randint(1, 20))
    if kind == 9:
        low = r.choice([-2.5, -1.0, 0.0, 0.5])
        return row(9, name, 0, 0, low, low + r.choice([0.5, 1.0, 4.0]))
    if kind == 11:
        return row(11, name, flags=r.choice([1, 2, 1, 2, 0, 3]))
    return row(kind, name)


MEMBER_NAMES = ["Alpha", "Beta", "Gamma", "Delta", "Eps", "Zeta", "Eta", "Theta"]


def random_table(r):
    """One to three groups, each plain (a blank row, members, heading) or an array group (start,
    members, end with a capacity of one to four). Init's reset leaves out the first row of
    a run, so a plain group starts with a blank row and an array group follows the previous
    heading directly, as in the game's tables."""
    rows = []
    for g in range(r.randint(1, 3)):
        array = r.random() < 0.45
        if array:
            rows.append(row(2))
        else:
            rows.append(row(0))
        for name in r.sample(MEMBER_NAMES, r.randint(1, 5)):
            rows.append(random_member(r, name))
        rows.append(row(3, "Grp%d" % g, count=r.randint(1, 4)) if array else row(1, "Grp%d" % g))
    rows.append(row(12))
    t = Table(6, rows, 0, "Random")
    t.slots = layout(t)[1] + 3
    return t


def odd_tables():
    """Tables with rows the real ones do not have (all end in a type 3 for every type 2, or
    the reader would run off the rows): a group missing its end, a zero-length array, an empty
    table, unknown types in the middle, an array of capacity zero, a table of one row."""
    shapes = {
        "no end": [row(2), row(4, "A"), row(0), row(4, "B"), row(3, "G", count=2), row(12)],
        "stray start": [row(2), row(4, "A"), row(2), row(4, "B"), row(3, "G", count=3), row(3, "H", count=2), row(12)],
        "empty array": [row(2), row(3, "G", count=3), row(4, "A"), row(1, "H"), row(12)],
        "capacity zero": [row(2), row(4, "A"), row(3, "G", count=0), row(4, "B"), row(1, "H"), row(12)],
        "blank before start": [row(0), row(2), row(4, "A"), row(5, "B"), row(3, "G", count=3), row(12)],
        "blank inside": [row(0), row(4, "A"), row(0), row(4, "B"), row(1, "H"), row(12)],
        "array first": [row(2), row(4, "A"), row(10, "B"), row(3, "G", count=3), row(0), row(5, "C"), row(1, "H"), row(12)],
        "runs off": [row(0), row(2), row(4, "A"), row(5, "B"), row(12)],
        "unknown types": [row(0), row(4, "A"), row(13, "X"), row(-1, "Y"), row(5, "B"), row(1, "G"), row(12)],
        "only the end": [row(12)],
        "blank and end": [row(0), row(12)],
        "headings only": [row(1, "A"), row(1, "B"), row(3, "C", count=1), row(12)],
        "no blank first": [row(4, "A"), row(7, "B"), row(10, "C"), row(1, "G"), row(12)],
        "wide array": [row(0), row(2), row(4, "A"), row(5, "B"), row(10, "C"), row(3, "G", count=40), row(12)],
    }
    for name, rows in shapes.items():
        # (a group with no end sends the reset past the rows: what is there is a type 3)
        t = Table(6, rows, 0, name, beyond=row(3, "Beyond", count=2) if name == "runs off" else None)
        t.slots = layout(t)[1] + 3
        yield t


def seed_ops(r, t, density=1.0):
    """Garbage in value slots (integers, strings, maps) of the slots the model says are used."""
    ops = []
    slots, _ = layout(t)
    for i, (slot, stride, count_slot, capacity) in sorted(slots.items()):
        for k in range(min(capacity, 6)):
            if r.random() > density:
                continue
            kind = t.rows[i][0]
            if kind == 10:
                ops.append(("seed", slot + k * stride, 1, r.choice(["old", "", "a longer old string"]), 0))
            elif kind == 11:
                ops.append(("seed", slot + k * stride, 2, "", r.randrange(1, 255)))
            else:
                ops.append(("seed", slot + k * stride, 0, "", r.choice([T.rint(r), 1, -1, 77])))
        if count_slot is not None and r.random() < density:
            ops.append(("seed", count_slot, 0, "", r.randrange(0, 9)))
    return ops


def case(t, ops, **extra):
    return dict(table=t, ops=list(ops), **extra)


def any_table(r):
    k = r.random()
    if k < 0.45:
        return random_table(r)
    if k < 0.75:
        return r.choice(REAL)
    return EDGE


# ---------------------------------------------------------------- text of balance files

INT_GOOD = ["0", "1", "5", "-3", "42", "100", "-5", "9", "10", "7", "2", "3"]
INT_ODD = ["1.5", "abc", "", "-", "+4", "2147483647", "4294967296", "99999999999", "-2147483648", "-99999999999",
           "12abc", "007", "-0", "4294967295", "-4294967295", "2147483648", "0x10", "--5", "5-"]
FLOAT_GOOD = ["0", "1.5", "-2.25", ".5", "5.", "-0", "3", "0.0001", "100.75", "-1.5", "2.5", "0.0", "1", "-0.5", "2.4"]
FLOAT_ODD = ["1.2.3", "abc", "", "-", ".", "1e5", "--1", "1.5f", "9" * 120, "1." + "5" * 95, "-.", "-.5", "..", "0.5.",
             "1.5.", "inf", "nan", "1.2.3.4", "...", ".1.2.3"]
STRING_GOOD = ['"text"', '""', '"two words"', '"a # b"', '"tab\there"', '"%s %d"', '"daphne.eagames.co.uk"']
STRING_ODD = ["bare", '"unterminated', '"', '""""', ' "padded"', '"x" "y"', "'single'", '"bad\x80char"']
SYMBOLS = {1: ".*Q@+#8624NESWD><OX", 2: ".^_[]JF7L=HCUn3O"}
JUNK = ["", "# comment", "   ", "\t# tab comment", "Nope.Field 1", "Grp0 5", "Grp0.", "Grp0.Alpha", "Grp0.Alpha=5",
        "1abc.X 1", ".X 1", "Grp0[.X 1", "Grp0[1.X 1", "Grp0[1]X 1", "Grp0[1].", "Grp0[x].Y 1", "Grp0[-1].Alpha 1",
        "Grp0[99999999999].Alpha 1", "#", "##", " # x", "Grp0.Alpha.Alpha.Alpha 1 2 3", "Grp0.Alpha .Beta 1 2",
        "A" * 63 + ".X 1", "Grp0." + "B" * 70 + " 1", "Grp0.Alpha\t1", "Grp0.Alpha\n1", "Grp0\t.Alpha 1"]


def int_token(r, row_):
    if r.random() < 0.12:
        return r.choice(INT_ODD)
    kind = row_[0]
    if kind == 6 and r.random() < 0.6:
        return str(r.choice([row_[2], row_[3] - 1, row_[3], row_[2] - 1, r.randint(row_[2], row_[3] - 1)]))
    if kind == 5 and r.random() < 0.3:
        return str(r.choice([-1, 0, 1]))
    return r.choice(INT_GOOD)


def float_token(r, row_):
    if r.random() < 0.12:
        return r.choice(FLOAT_ODD)
    if row_[0] == 9 and r.random() < 0.6:
        return repr(r.choice([row_[4], row_[5], row_[4] - 0.5, row_[5] - 0.25, (row_[4] + row_[5]) / 2]))
    if row_[0] == 8 and r.random() < 0.3:
        return r.choice(["-0.5", "0", "-0.0", "0.5"])
    return r.choice(FLOAT_GOOD)


def map_block(r, flags):
    """The lines after a map field's own line: opening marker, rows, closing marker; with
    a mistake in one case in three."""
    keys = SYMBOLS.get(flags, SYMBOLS[1])
    rows = ["".join(r.choice(keys + "  ") for _ in range(r.randint(0, 7))) for _ in range(r.randint(1, 4))]
    opening, closing = r.choice(["---", "---", "-----", "--- map", "---------"]), r.choice(["---", "---", "----", "--- end"])
    k = r.random()
    if k < 0.18:
        rows[r.randrange(len(rows))] += r.choice(["Z", "z", "!", "\x80"])
    elif k < 0.24:
        rows.append(r.choice(keys) * r.choice([20, 21, 25]))
    elif k < 0.30:
        rows = [r.choice(keys) * r.randint(1, 5)] * r.choice([20, 21, 22, 23])
    elif k < 0.38:
        closing = None
    elif k < 0.44:
        opening = r.choice(["--", "-", "x---", ""])
    elif k < 0.48:
        rows = []
    text = opening + "\n" + "".join(line + "\n" for line in rows)
    return text if closing is None else text + closing + "\n"


def field_line(r, t, groups):
    """One field line of a balance file (several lines for a map)."""
    if not groups or r.random() < 0.08:
        return r.choice(JUNK)
    name, members, array, capacity = r.choice(groups)
    if not members:
        return r.choice(JUNK)
    maps = [m for m in members if m[1] == 11]
    if maps and r.random() < 0.7:
        chosen = [r.choice(maps)]
    else:
        count = r.randint(1, min(len(members), 3))
        first = r.randrange(len(members) - count + 1)
        chosen = members[first:first + count]
    prefix = name
    if array:
        prefix += "[%d]" % r.choice([r.randrange(max(capacity, 1)), r.randrange(max(capacity, 1)), capacity, capacity + 3, 0])
    elif r.random() < 0.04:
        prefix += "[0]"
    head = prefix + "." + ".".join(m[0] for m in chosen)
    sep = r.choice([" ", "\t", "  ", " \t "])
    if chosen[0][1] == 11 and len(chosen) == 1:
        return head + "\n" + map_block(r, chosen[0][2][7])
    tokens = []
    for _, kind, row_ in chosen:
        if kind in (7, 8, 9):
            tokens.append(float_token(r, row_))
        elif kind == 10:
            tokens.append(r.choice(STRING_GOOD) if r.random() < 0.85 else r.choice(STRING_ODD))
        elif kind == 11:
            tokens.append(r.choice(INT_GOOD))
        else:
            tokens.append(int_token(r, row_))
    if r.random() < 0.05:
        tokens = tokens[:-1]
    line = head + sep + sep.join(tokens)
    if r.random() < 0.3:
        line += r.choice(["  # note", "\tsome words", " 5", "   "])
    return line


def balance_text(r, t, lines=None):
    groups = table_groups(t)
    ending = r.choice(["\r\n", "\r\n", "\n"])
    out = []
    for _ in range(lines if lines is not None else r.randint(0, 12)):
        out.append(field_line(r, t, groups))
        if r.random() < 0.25:
            out.append(r.choice(["", "", "# a comment", "\t", "   # more"]))
    text = ending.join(out)
    if out and r.random() < 0.85:
        text += ending
    return text.replace("\r\n", "\n").replace("\n", ending).encode("latin-1")


def parse_case(r):
    t = any_table(r)
    ops = [("init",)]
    if r.random() < 0.3:
        ops += seed_ops(r, t, 0.6)
    ops.append(("parse", "test.sam", balance_text(r, t), r.choice([0, 0, 0, 1])))
    if r.random() < 0.3:
        ops.append(("parse", "second.sam", balance_text(r, t), r.choice([0, 1, 1])))
    queues = {"LbMem_Alloc": [1] * r.randrange(1, 4) + [0]} if r.random() < 0.1 else None
    return case(t, ops, queues=queues)


# ================================================================== cases: the table's functions


def init_edge():
    r = random.Random(11)
    for t in REAL:
        yield case(t, [("init",)])
        yield case(t, [("init",)], queues={"LbMem_Alloc": [0]})
    yield case(EDGE, [("init",)])
    yield case(EDGE, [("init",)], queues={"LbMem_Alloc": [0]})
    for t in odd_tables():
        yield case(t, [("init",)])
    # a second Init on a table that holds strings drops them without freeing (construction)
    for t in (EDGE, REAL[3], REAL[2]):
        yield case(t, [("init",)] + seed_ops(r, t) + [("init",)])
    yield case(EDGE, [("init",), ("uninit",), ("init",)])


def init_random(r):
    t = r.choice([random_table(r), random_table(r), any_table(r)])
    ops = [("init",)]
    queues = {"LbMem_Alloc": [0]} if r.random() < 0.08 else None
    if queues is None and r.random() < 0.2:
        ops += seed_ops(r, t, 0.5) + [("init",)]
    return case(t, ops, queues=queues)


def uninit_edge():
    r = random.Random(12)
    for t in [EDGE] + REAL + list(odd_tables())[:5]:
        yield case(t, [("init",)] + seed_ops(r, t) + [("uninit",)])
        yield case(t, [("init",), ("uninit",), ("uninit",)])
    yield case(EDGE, [("init",), ("uninit",)], queues={"LbMem_Alloc": [0]})
    yield case(EDGE, [("init",)] + seed_ops(r, EDGE) + [("free",), ("uninit",)])


def uninit_random(r):
    t = r.choice([random_table(r), random_table(r), any_table(r)])
    ops = [("init",)] + seed_ops(r, t, r.choice([0.3, 1.0])) + [("uninit",)]
    if r.random() < 0.3:
        ops.append(("uninit",))
    return case(t, ops)


def free_edge():
    r = random.Random(13)
    for t in [EDGE] + REAL + list(odd_tables())[:5]:
        yield case(t, [("init",)] + seed_ops(r, t) + [("free",)])
        yield case(t, [("init",), ("uninit",), ("free",)])
    yield case(EDGE, [("init",), ("free",)], queues={"LbMem_Alloc": [0]})
    yield case(EDGE, [("init",)] + seed_ops(r, EDGE) + [("free",), ("free",)])


def free_random(r):
    t = r.choice([random_table(r), random_table(r), any_table(r)])
    ops = [("init",)] + seed_ops(r, t, r.choice([0.3, 1.0])) + [("free",)]
    if r.random() < 0.3:
        ops.append(("uninit",))
        ops.append(("free",))
    return case(t, ops)


def initialise_edge():
    r = random.Random(14)
    for t in [EDGE] + REAL + list(odd_tables())[:5]:
        for kind in (0, 1):
            yield case(t, [("init",)] + seed_ops(r, t) + [("initialise", kind)])
    for kind in (2, -1, 7):
        yield case(EDGE, [("init",)] + seed_ops(r, EDGE) + [("initialise", kind)])


def initialise_random(r):
    t = r.choice([random_table(r), random_table(r), any_table(r)])
    return case(t, [("init",)] + seed_ops(r, t, r.choice([0.3, 1.0])) + [("initialise", r.choice([0, 1, 0, 1, 2, -1]))])


def count_edge():
    for t in [EDGE] + REAL + list(odd_tables()):
        yield case(t, [("count",)])


def count_random(r):
    return case(r.choice([random_table(r), any_table(r)]), [("count",)])


def locate_names(r, t):
    """A group, a field, whether an element and the index: mostly real members, sometimes not."""
    groups = table_groups(t)
    if not groups or r.random() < 0.05:
        return r.choice(["Nope", "", "Grp0", "Items"]), r.choice(["Nope", "", "Plain"]), r.randrange(2), r.randrange(-1, 4)
    name, members, array, capacity = r.choice(groups)
    field = r.choice(members)[0] if members and r.random() < 0.8 else r.choice(MEMBER_NAMES + ["Plain", "Other", "Nope"])
    if r.random() < 0.15:
        name = r.choice(groups)[0]
    in_array = int(array) if r.random() < 0.85 else 1 - int(array)
    index = r.choice([0, 1, capacity - 1, capacity, capacity + 1, -1, 0, 2, 1 << 30, -(1 << 31)])
    return name, field, in_array, index


def locate_edge():
    for t in [EDGE] + REAL + list(odd_tables()):
        for name, members, array, capacity in table_groups(t):
            for field, _, _ in members:
                yield case(t, [("locate", name, field, int(array), 0)])
                if array:
                    yield case(t, [("locate", name, field, 1, capacity - 1)])
                    yield case(t, [("locate", name, field, 1, capacity)])
                    yield case(t, [("locate", name, field, 1, -1)])
                    yield case(t, [("locate", name, field, 0, 0)])
                else:
                    yield case(t, [("locate", name, field, 1, 0)])
            yield case(t, [("locate", name, "Nope", int(array), 0)])
            yield case(t, [("locate", name, "", int(array), 0)])
    yield case(EDGE, [("locate", "Scalars", "ArrInt", 0, 0)])
    yield case(EDGE, [("locate", "Items", "Plain", 1, 0)])
    yield case(EDGE, [("locate", "Misc", "Plain", 0, 0)])
    yield case(EDGE, [("locate", "Nope", "Plain", 0, 0)])
    yield case(EDGE, [("locate", "", "", 0, 0)])


def locate_random(r):
    t = r.choice([random_table(r), random_table(r), any_table(r)])
    group, field, in_array, index = locate_names(r, t)
    return case(t, [("locate", group, field, in_array, index)])


INTS = [0, 1, -1, 2, 5, 9, 10, 11, -5, -6, 100, 99, 7, 1 << 30, -(1 << 31), (1 << 31) - 1, 50, 500, 1000, 2000, 2001, 3200, 3201]


def sample_rows(t):
    """Every row of a small table, every fifth of a big one (and the first few)."""
    return [i for i in range(len(t.rows)) if len(t.rows) <= 40 or i < 8 or i % 5 == 0]


def assign_int_edge():
    for t in [EDGE] + REAL + list(odd_tables())[:6]:
        for i in sample_rows(t):
            low, high = t.rows[i][2], t.rows[i][3]
            for value in sorted({0, 1, -1, low - 1, low, high - 1, high, high + 1, 5, 10, -5, -6, (1 << 31) - 1, -(1 << 31)}):
                yield case(t, [("assign_int", 1, i, T.s32(value))])


def assign_int_random(r):
    t = r.choice([random_table(r), any_table(r)])
    i = r.randrange(len(t.rows))
    low, high = t.rows[i][2], t.rows[i][3]
    value = r.choice([r.choice(INTS), low, low - 1, high, high - 1, T.rint(r), r.randint(low - 3, high + 3)])
    return case(t, [("assign_int", r.randrange(1, t.slots), i, T.s32(value)), ("assign_int", 1, r.randrange(len(t.rows)), T.s32(r.choice(INTS)))])


FLOATS = [0.0, -0.0, 1.5, -1.5, 2.5, -2.5, 0.5, -0.5, 2.4999998, -1.5000001, 1e-30, -1e-30, 1e30, -1e30, 3.0, 100.0,
          float("inf"), float("-inf"), 0.25, 4.0, 1.0, -1.0, -2.0, 0.75, 1.25]
FLOAT_BITS = [struct.unpack(">I", struct.pack(">f", f))[0] for f in FLOATS] + [0x7FC00000, 0xFFC00000, 0x7FC00001, 0x00000001, 0x80000001]


def quiet(bits):
    """A signaling NaN cannot reach the PowerPC function (a float argument arrives as a double,
    which quiets it): make it quiet."""
    return bits | 0x00400000 if (bits & 0x7F800000) == 0x7F800000 and bits & 0x007FFFFF else bits


def assign_float_edge():
    for t in [EDGE] + REAL + list(odd_tables())[:6]:
        for i in sample_rows(t):
            low, high = t.rows[i][4], t.rows[i][5]
            extra = [struct.unpack(">I", struct.pack(">f", v))[0] for v in (low, high, low - 0.5, high - 0.25)]
            for bits in FLOAT_BITS + extra:
                yield case(t, [("assign_float", 1, i, bits)])


def assign_float_random(r):
    t = r.choice([random_table(r), any_table(r)])
    i = r.randrange(len(t.rows))
    bits = quiet(r.choice([r.choice(FLOAT_BITS), r.getrandbits(32), struct.unpack(">I", struct.pack(">f", T.rfloat(r)))[0]]))
    return case(t, [("assign_float", r.randrange(1, t.slots), i, bits)])


# ================================================================== cases: parsing


def parse_case_from(t, text, mode=0, pre=(), **extra):
    return case(t, [("init",)] + list(pre) + [("parse", "test.sam", text, mode)], **extra)


# The mistakes and the oddities of a file, written for the edge table.
EDGE_TEXTS = [
    "", "\n", "   ", "# only a comment", "# c\n# d\n", "\n\n\nScalars.Plain 5\n", "Scalars.Plain 5", "Scalars.Plain 5\n",
    "Scalars.Plain\t-7 trailing words\n", "Scalars.Plain 5\r\nScalars.NonNeg 6\r\n", "Scalars.Plain 5\rScalars.NonNeg 6\r",
    "Scalars.Plain 1.5\n", "Scalars.Plain abc\n", "Scalars.Plain\n", "Scalars.Plain", "Scalars.Plain 4294967296\n",
    "Scalars.Plain -4294967296\n", "Scalars.Plain 99999999999\n", "Scalars.Plain 2147483648\n", "Scalars.Plain -2147483648\n",
    "Scalars.Plain 4294967295\n", "Scalars.Plain +5\n", "Scalars.Plain -\n", "Scalars.Plain 12abc\n", "Scalars.Plain 5 6\n",
    "Scalars.NonNeg -1\n", "Scalars.NonNeg 0\n", "Scalars.NonNeg -0\n",
    "Scalars.Range 9\n", "Scalars.Range 10\n", "Scalars.Range -5\n", "Scalars.Range -6\n",
    "Scalars.Real 1.5\n", "Scalars.Real .5\n", "Scalars.Real 5.\n", "Scalars.Real -0\n", "Scalars.Real 1.2.3\n",
    "Scalars.Real 1e5\n", "Scalars.Real 3\n", "Scalars.Real x\n", "Scalars.Real " + "1" * 120 + "\n",
    "Scalars.Real " + "1" * 99 + "\n", "Scalars.Real " + "1" * 100 + "\n", "Scalars.Real -" + "1" * 99 + "\n", "Scalars.Real -\n",
    "Scalars.Real .\n", "Scalars.Real -.5\n", "Scalars.Real 0.1\n", "Scalars.Real 123456.789\n", "Scalars.Real 3.4028235e38\n",
    "Scalars.NonNegReal -0.5\n", "Scalars.NonNegReal -0.0\n", "Scalars.NonNegReal 0.5\n",
    "Scalars.RealRange 2.5\n", "Scalars.RealRange 2.4\n", "Scalars.RealRange -1.5\n", "Scalars.RealRange -1.6\n",
    'Scalars.Text "hello"\n', "Scalars.Text hello\n", 'Scalars.Text "unterminated\n', 'Scalars.Text ""\n', 'Scalars.Text "a # b"\n',
    'Scalars.Text "x"\nScalars.Text "yy"\n', 'Scalars.Text\n"after a line end"\n', 'Scalars.Text "last"', 'Scalars.Text "x" "y"\n',
    'Items[0].ArrText "a"\nItems[1].ArrText "b"\nItems[0].ArrText "c"\n', 'Items[3].ArrText "d"\n', 'Items[4].ArrText "e"\n',
    "Items[1].ArrText bare\n", 'Items[1].ArrText "unterminated\n', "Items[1].ArrText\n",
    "Items[0].ArrInt 5\n", "Items[3].ArrInt 5\n", "Items[4].ArrInt 5\n", "Items[-1].ArrInt 5\n", "Items[x].ArrInt 5\n",
    "Items[2]ArrInt 5\n", "Items[2].ArrInt\n", "Items[2]\n", "Items[12345678901].ArrInt 1\n", "Items[2].\n", "Items[ 2].ArrInt 1\n",
    "Items.ArrInt 5\n", "Scalars[0].Plain 1\n", "Items[1].ArrInt.ArrRange.ArrFloat 5 6 7.5\n", "Items[2].ArrRange 100\n",
    "Items[2].ArrRange 99\n", "Items[2].ArrRange -1\n", "Items[1].ArrFloat 2.5\nItems[3].ArrFloat 1\nItems[1].ArrInt 9\n",
    "Items[3].ArrInt 1\nItems[0].ArrInt 2\n", "Items[0].ArrFloat 1.2.3\n", "Items[0].ArrFloat x\n", "Items[0].ArrInt 1.5\n",
    "Scalars.Plain.NonNeg.Range 1 2 3\n", "Scalars.Plain.NonNeg 1\n", "Scalars.Plain.NonNeg 1\n2\n", "Scalars.Plain.NonNeg 1 -2\n",
    "Misc.Other 3\n", "Misc.Nope 3\n", "Nope.Other 3\n", "Scalars.Other 3\n", "Misc.Plain 3\n", "Other.Misc 3\n",
    "Scalars.Plain 1 # comment\nScalars.NonNeg 2 more words\n", "  Scalars.Plain 5\n", "\tScalars.Plain 5\n",
    "Scalars. Plain 5\n", "Scalars.Plain.5\n", "Scalars.Plain. 5\n", "Scalars.Plain..NonNeg 5 6\n", "Scalars Plain 5\n", "Scalars\n",
    ".Plain 5\n", "5.Plain 5\n", "Scalars-Plain 5\n", "Scalars.Plain=5\n", "Scalars.1Plain 5\n", "Scalars.Pl-ain 5\n",
    "A" * 63 + ".X 1\n", "Scalars." + "A" * 63 + " 1\n", "Scalars." + "A" * 64 + " 1\n", "Scalars." + "A" * 65 + " 1\n",
    "Scalars." + ".".join("N%d" % i for i in range(15)) + ".Plain 1\n", "Scalars." + ".".join("N%d" % i for i in range(16)) + "\n",
    "Scalars." + ".".join("N%d" % i for i in range(16)) + "..\n", "Scalars." + ".".join("N%d" % i for i in range(17)) + " 1\n",
    "Scalars." + ".".join("N%d" % i for i in range(16)) + ". 1\n", "Scalars.Plain\n\n\n\nScalars.NonNeg 4\n",
    # maps
    "Scalars.Shape\n---\n.*Q\n@+#\n---\n", "Scalars.Shape\n---\n.*Q@+#8624NESWD><OX\n---\nScalars.Plain 4\nScalars.NonNeg 5\n",
    "Scalars.Shape\n---\n---\n", "Scalars.Shape\n---\n" + ".\n" * 20 + "---\n", "Scalars.Shape\n---\n" + ".\n" * 21 + "---\n",
    "Scalars.Shape\n---\n" + "." * 20 + "\n---\n", "Scalars.Shape\n---\n" + "." * 21 + "\n---\n", "Scalars.Shape\n---\nZ\n---\n",
    "Scalars.Shape\n--\n.\n---\n", "Scalars.Shape\n---\n.\n", "Scalars.Shape\n---\n.\n--\n", "Scalars.Shape\n", "Scalars.Shape",
    "Scalars.Shape\n\n", "Scalars.Shape2\n---\n.^_[]JF7L=HCUn3O\n.\n---\n", "Scalars.Shape2\n---\n*\n---\n",
    "Scalars.Shape.Plain\n---\n.\n---\n5\n", "Scalars.Shape.Plain 5\n", "Scalars.Shape 5\n",
    "Scalars.Shape\n---- note\n  . * Q  \n\n.\n--- end\nafter\nScalars.Plain 3\n",
    "Items[2].ArrShape\n---\n.*\n---\n", "Items[1].ArrShape\n---\n.\n---\n\nItems[1].ArrShape\n---\n*\n---\n",
    "Items[2].ArrShape\n---\n.\n---\n\nItems[1].ArrShape\n---\n*\n---\n\nItems[1].ArrShape\n---\n+\n---\n",
    "Items[4].ArrShape\n---\n.\n---\n", "Items.ArrShape\n---\n.\n---\n",
    "Scalars.Shape\n---\n.\n---\n\nScalars.Shape\n---\nQ\n---\n",
    "\n\n  Scalars.Shape\n---\nQ\n---\n", "Scalars.Shape\r\n---\r\n.*\r\n---\r\n",
    "Scalars.Shape0\n---\n.\n---\n", "Scalars.ShapeX\n---\n.\n---\n", "Scalars.ShapeNeg\n---\n.\n---\n",
    "Scalars.Shape\n---\n.*", "Scalars.Shape\n---\n.\n.*", "Scalars.Shape\n---\n.\n---",
    "Items[1].ArrNonNeg -1\n", "Items[1].ArrNonNeg 3\n", "Items[1].ArrNonNeg 0\n", "Items[1].ArrNonNeg -0.5\nItems[3].ArrNonNeg 2\n",
    GROUP_64 + ".X 1\n", GROUP_64 + "x 1\n", GROUP_64 + "[1].X 1\n",
]


def edge_parse_cases():
    for text in EDGE_TEXTS:
        yield parse_case_from(EDGE, text.encode("latin-1"))
    for text in EDGE_TEXTS[::7]:
        yield parse_case_from(EDGE, text.encode("latin-1"), 1)
    yield parse_case_from(EDGE, b"Scalars.Plain 5\nScalars.Text \"\x80\xff\"\nScalars.Plain \xe9\n")
    yield parse_case_from(EDGE, b"Scalars.Pl\x00ain 5\nScalars.Plain 6\n")
    yield parse_case_from(EDGE, b"\x0b\x0cScalars.Plain 7\n")
    yield parse_case_from(EDGE, b"\xa0Scalars.Plain 7\n")
    # a second file on top of the first: mode 1 keeps what was read, mode 0 starts again;
    # strings and maps that are replaced are freed
    first = b'Scalars.Plain 5\nScalars.Text "one"\nItems[2].ArrText "two"\nItems[1].ArrInt 4\nScalars.Shape\n---\nQ\n---\n'
    second = b'Scalars.NonNeg 9\nScalars.Text "three"\nItems[2].ArrText "four"\nScalars.Shape\n---\n*\n---\n'
    for mode in (0, 1):
        yield case(EDGE, [("init",), ("parse", "a.sam", first, 0), ("parse", "b.sam", second, mode)])
    yield case(EDGE, [("init",), ("parse", "a.sam", first, 0), ("parse", "b.sam", b"Scalars.Plain x\n", 0), ("parse", "c.sam", second, 1)])
    # seeded strings and maps are freed by mode 0 and replaced by a value
    yield case(EDGE, [("init",)] + seed_ops(random.Random(3), EDGE) + [("parse", "c.sam", second, 0)])
    yield case(EDGE, [("init",)] + seed_ops(random.Random(3), EDGE) + [("parse", "c.sam", second, 1)])
    # allocations refused: the string, then the map (the first allocation is Init's)
    yield case(EDGE, [("init",), ("parse", "d.sam", b'Scalars.Text "abc"\n', 0)], queues={"LbMem_Alloc": [1, 0]})
    yield case(EDGE, [("init",), ("parse", "d.sam", b"Scalars.Shape\n---\n.\n---\n", 0)], queues={"LbMem_Alloc": [1, 0]})
    yield case(EDGE, [("init",), ("parse", "d.sam", b'Scalars.Text "abc"\nScalars.Text "def"\n', 0)], queues={"LbMem_Alloc": [1, 1, 0]})
    yield case(EDGE, [("init",), ("parse", "a-very-long-name-" * 6 + ".sam", b"Scalars.Plain x\n", 0)])


# The tables of the game, with the files the game loads into them: (kind, base names in
# the order they are read, then the mode of the second).
OWN_FILES = {0: ("hoardings.sam", "endhoardings.sam"), 1: ("standard.sam", "online_standard.sam", "easy_standard.sam", "challenges.sam"),
             2: ("online.sam", "onlineoverride.sam"), 3: ("high.sam", "med.sam", "low.sam"), 4: ("sound.sam",), 5: ("_resolution.sam",)}


def real_files():
    """(kind, path below the extracted game folder, bytes) for every distinct file the game loads
    into one of the six tables, read now (nothing is copied into the repository)."""
    found, seen = [], set()
    for kind, names in OWN_FILES.items():
        for path in sorted(EXTRACTED.rglob("*.sam")) if EXTRACTED.exists() else []:
            if path.name.lower() in names and "wads" not in path.parts:
                data = path.read_bytes()
                if (kind, data) not in seen:
                    seen.add((kind, data))
                    found.append((kind, str(path.relative_to(EXTRACTED)), data))
    return found


SMALL = 6000     # the files above this are read once, whole: the Main table takes seconds over one


def real_file_cases():
    r = random.Random(31)
    files = real_files()
    for kind, name, data in files:
        yield case(REAL[kind], [("init",), ("parse", name, data, 0)])
        if len(data) <= SMALL:
            yield case(REAL[kind], [("init",), ("parse", name, data, 1)])
            for _ in range(2):
                yield case(REAL[kind], [("init",), ("parse", name, data[:r.randrange(len(data))], 0)])
    smallest = {}
    for kind, name, data in files:
        key = (kind, pathlib.PurePath(name).name.lower())
        if key not in smallest or len(data) < len(smallest[key]):
            smallest[key] = data
    # a second file over the first: the level's own over the standard, the online override
    for kind, base, over in ((1, "standard.sam", "online_standard.sam"), (2, "online.sam", "onlineoverride.sam"), (0, "hoardings.sam", "endhoardings.sam")):
        if (kind, base) in smallest and (kind, over) in smallest:
            for mode in (0, 1):
                yield case(REAL[kind], [("init",), ("parse", base, smallest[(kind, base)], 0), ("parse", over, smallest[(kind, over)], mode)])
    # every table asked to read a file that is not its own
    firsts = {}
    for kind, name, data in files:
        if len(data) <= SMALL:
            firsts.setdefault(kind, (name, data))
    for kind, (name, data) in firsts.items():
        for other in range(6):
            if other != kind:
                yield case(REAL[other], [("init",), ("parse", name, data[:3000], 0)])


def parse_edge():
    for c in edge_parse_cases():
        yield c
    for c in real_file_cases():
        yield c


# ---------------------------------------------------------------- Load

LOAD_TEXT = b'Scalars.Plain 5\r\nItems[1].ArrInt 7\r\nScalars.Text "hi"\r\n'
BAD_TEXT = b"Scalars.Plain 5\r\nScalars.Plain x\r\nScalars.NonNeg 4\r\n"


def load_path_case(t, text, mode, length=None, load_result=0, alloc=None, file=None):
    queues = {"LbFile_GetLength": [len(text) if length is None else length], "LbFile_Load": [load_result]}
    if alloc is not None:
        queues["LbMem_Alloc"] = alloc
    return case(t, [("init",), ("load_path", "data:test.sam", mode)], queues=queues, file=text if file is None else file)


def load_path_edge():
    for mode in (0, 1):
        yield load_path_case(EDGE, LOAD_TEXT, mode)
        yield load_path_case(EDGE, BAD_TEXT, mode)
        yield load_path_case(EDGE, LOAD_TEXT, mode, length=-1)
        yield load_path_case(EDGE, LOAD_TEXT, mode, length=0)
        yield load_path_case(EDGE, LOAD_TEXT, mode, alloc=[1, 0])
        yield load_path_case(EDGE, LOAD_TEXT, mode, load_result=-1)
        yield load_path_case(EDGE, LOAD_TEXT, mode, load_result=5)
        yield load_path_case(EDGE, LOAD_TEXT, mode, length=len(LOAD_TEXT) - 7)
        yield load_path_case(EDGE, LOAD_TEXT, mode, length=len(LOAD_TEXT) + 3)
        yield load_path_case(EDGE, LOAD_TEXT, mode, length=1)
    for kind, name, data in [f for f in real_files() if len(f[2]) <= SMALL][:12]:
        yield load_path_case(REAL[kind], data, 0)
    yield load_path_case(REAL[1], b"x" * 40 + b"\n" + b"y" * 5000, 0)


def load_path_random(r):
    t = any_table(r)
    text = balance_text(r, t)
    length = r.choice([len(text)] * 4 + [-1, 0, len(text) + 2, max(len(text) - 3, 0)])
    return load_path_case(t, text, r.choice([0, 1]), length=length, load_result=r.choice([0, 0, 0, -1, 3]),
                          alloc=[1, 0] if r.random() < 0.1 else None)


def load_file_case(t, text, mode, size=None, read=None, alloc=None):
    queues = {"CFile::GetSize": [len(text) if size is None else size]}
    if read is not None:
        queues["CFile::Read"] = [read]
    if alloc is not None:
        queues["LbMem_Alloc"] = alloc
    return case(t, [("init",), ("load_file", "ride.sam", mode)], queues=queues, file=text)


def load_file_edge():
    for mode in (0, 1):
        yield load_file_case(EDGE, LOAD_TEXT, mode)
        yield load_file_case(EDGE, BAD_TEXT, mode)
        yield load_file_case(EDGE, LOAD_TEXT, mode, size=0)
        yield load_file_case(EDGE, LOAD_TEXT, mode, size=-1)
        yield load_file_case(EDGE, LOAD_TEXT, mode, size=-100)
        yield load_file_case(EDGE, LOAD_TEXT, mode, alloc=[1, 0])
        yield load_file_case(EDGE, LOAD_TEXT, mode, read=len(LOAD_TEXT) - 1)
        yield load_file_case(EDGE, LOAD_TEXT, mode, read=0)
        yield load_file_case(EDGE, LOAD_TEXT, mode, read=-1)
        yield load_file_case(EDGE, LOAD_TEXT, mode, read=len(LOAD_TEXT) + 5)
        yield load_file_case(EDGE, LOAD_TEXT, mode, size=len(LOAD_TEXT) + 3)
        yield load_file_case(EDGE, LOAD_TEXT, mode, size=len(LOAD_TEXT) - 5)
    for kind, name, data in [f for f in real_files() if len(f[2]) <= SMALL][:12]:
        yield load_file_case(REAL[kind], data, 0)


def load_file_random(r):
    t = any_table(r)
    text = balance_text(r, t)
    size = r.choice([len(text)] * 4 + [0, -1, len(text) + 2, max(len(text) - 3, 1)])
    return load_file_case(t, text, r.choice([0, 1]), size=size, read=r.choice([None, None, None, 0, -1, len(text), max(size - 1, 0)]),
                          alloc=[1, 0] if r.random() < 0.1 else None)


# ================================================================== the line readers


def run_ppc_reader(e, case_, body):
    """A reader on the PowerPC side: the text, a cell holding the cursor, then body(e, case,
    cursor cell) for the outputs. The cursor comes back as an offset."""
    saved = STUBS.activate(e)
    try:
        e.cl = CL.PpcLog({EXIT_ERROR: EXIT_LABEL}, case_.get("queues"))
        text = case_["text"]
        base = e.alloc(len(text) + 1, text + b"\0")
        cursor = e.alloc(4, struct.pack(">I", base + case_["start"]))
        out = body(e, case_, cursor)
        out["cursor"] = e.u32(cursor) - base
        out["log"] = e.cl.log
        return out
    finally:
        CL.Stubs.deactivate(e, saved)


def run_host_reader(lib, case_, body):
    """The same on the host: body(lib, case, buffer) returns the outputs and the cursor offset."""
    CL.host_begin(lib, {EXIT_LABEL: F(lib, "tpw_bt_exit_error", C.c_void_p)()}, case_.get("queues"))
    try:
        buffer = C.create_string_buffer(case_["text"], len(case_["text"]) + 1)
        out = body(lib, case_, buffer)
    finally:
        log = CL.host_end(lib)
    out["log"] = log
    return out


def reader_test(name, ppc_body, host_body, edge, rnd):
    return FnTest(name, lambda e, c: run_ppc_reader(e, c, ppc_body), lambda lib, c: run_host_reader(lib, c, host_body),
                  edge=edge, rnd=rnd)


def cell(e, value=SENTINEL):
    return e.alloc(4, struct.pack(">I", value))


# read_array_row(cursor, &index)
def ppc_array_row(e, c, cursor):
    index = cell(e)
    ok = T.s32(T.call_ppc(e, READ_ARRAY_ROW, [cursor, index])[0])
    return dict(ok=ok, index=e.s32(index))


def host_array_row(lib, c, buffer):
    index, end = C.c_int32(SENTINEL), C.c_int32()
    ok = F(lib, "tpw_bt_read_array_row", C.c_int32, C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p)(
        buffer, c["start"], C.byref(index), C.byref(end))
    return dict(ok=ok, index=index.value, cursor=end.value)


# read_field_names(cursor, names, &count)
def ppc_field_names(e, c, cursor):
    names, count = e.alloc(1024, b"\xa5" * 1024), cell(e)
    ok = T.s32(T.call_ppc(e, READ_FIELD_NAMES, [cursor, names, count])[0])
    return dict(ok=ok, count=e.s32(count), names=e.read(names, 1024))


def host_field_names(lib, c, buffer):
    names, count, end = C.create_string_buffer(b"\xa5" * 1024, 1024), C.c_int32(SENTINEL), C.c_int32()
    ok = F(lib, "tpw_bt_read_field_names", C.c_int32, C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p)(
        buffer, c["start"], names, C.byref(count), C.byref(end))
    return dict(ok=ok, count=count.value, names=bytes(names.raw), cursor=end.value)


# read_integer_value(cursor, &value)
def ppc_integer(e, c, cursor):
    value = cell(e)
    ok = T.s32(T.call_ppc(e, READ_INTEGER, [cursor, value])[0])
    return dict(ok=ok, value=e.s32(value))


def host_integer(lib, c, buffer):
    value, end = C.c_int32(SENTINEL), C.c_int32()
    ok = F(lib, "tpw_bt_read_integer_value", C.c_int32, C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p)(
        buffer, c["start"], C.byref(value), C.byref(end))
    return dict(ok=ok, value=value.value, cursor=end.value)


# read_float_value(cursor, &value)
def ppc_float(e, c, cursor):
    value = cell(e, 0xA5A5A5A5)
    ok = T.s32(T.call_ppc(e, READ_FLOAT, [cursor, value])[0])
    return dict(ok=ok, value=e.u32(value))


def host_float(lib, c, buffer):
    value, end = C.c_uint32(0xA5A5A5A5), C.c_int32()
    ok = F(lib, "tpw_bt_read_float_value", C.c_int32, C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p)(
        buffer, c["start"], C.byref(value), C.byref(end))
    return dict(ok=ok, value=value.value, cursor=end.value)


# read_string(cursor, &string)
def ppc_string(e, c, cursor):
    value = cell(e, 0xA5A5A5A5)
    ok = T.s32(T.call_ppc(e, READ_STRING, [cursor, value])[0])
    pointer = e.u32(value)
    return dict(ok=ok, value=None if pointer == 0 else e.cstr(pointer, limit=1 << 16))


def host_string(lib, c, buffer):
    value, end = C.c_void_p(0xA5A5A5A5), C.c_int32()
    ok = F(lib, "tpw_bt_read_string", C.c_int32, C.c_void_p, C.c_int32, C.c_void_p, C.c_void_p)(
        buffer, c["start"], C.byref(value), C.byref(end))
    return dict(ok=ok, value=None if not value.value else C.string_at(value.value).decode("latin-1"), cursor=end.value)


# recognise_delimit(cursor)
def ppc_delimit(e, c, cursor):
    return dict(ok=T.s32(T.call_ppc(e, RECOGNISE_DELIMIT, [cursor])[0]))


def host_delimit(lib, c, buffer):
    end = C.c_int32()
    ok = F(lib, "tpw_bt_recognise_delimit", C.c_int32, C.c_void_p, C.c_int32, C.c_void_p)(buffer, c["start"], C.byref(end))
    return dict(ok=ok, cursor=end.value)


# read_map_internal(cursor, &shape, &error, keys, count)
def ppc_map(e, c, cursor):
    shape, error = cell(e, 0xA5A5A5A5), cell(e, 0xA5A5A5A5)
    keys, count = (SYMBOLS_A, 19) if c["keys"] == 1 else (SYMBOLS_B, 16)
    ok = T.s32(T.call_ppc(e, READ_MAP, [cursor, shape, error, keys, count])[0])
    block, message = e.u32(shape), e.u32(error)
    return dict(ok=ok, shape=decode_shape(e.read(block, 0xC88), ">") if ok and block else None,
                error=e.cstr(message) if message not in (0, 0xA5A5A5A5) else "")


def host_map(lib, c, buffer):
    shape, error, end = C.c_void_p(), C.create_string_buffer(256), C.c_int32()
    ok = F(lib, "tpw_bt_read_map", C.c_int32, C.c_void_p, C.c_int32, C.c_int32, C.c_void_p, C.c_void_p, C.c_void_p)(
        buffer, c["start"], c["keys"], C.byref(shape), error, C.byref(end))
    return dict(ok=ok, shape=decode_shape(C.string_at(shape.value, 0xC88), "<") if ok and shape.value else None,
                error=error.value.decode("latin-1"), cursor=end.value)


def reader_case(text, start=0, **extra):
    if isinstance(text, str):
        text = text.encode("latin-1")
    return dict(text=text, start=min(start, len(text)), **extra)


def starts(text, r=None):
    """The start offsets worth trying in a text: its beginning, and a random place."""
    return [0] if r is None or len(text) < 2 else [0, r.randrange(len(text) + 1)]


def alphabet_text(r, alphabet, longest):
    return "".join(r.choice(alphabet) for _ in range(r.randrange(longest + 1)))


ARRAY_ROWS = ["[0].", "[12].F", "[12]", "[12]x", "[a].", "[]", "[-1].", "[99999999999].", "[ 7].", "[7 ].", "[7]", "x", "", "[7].",
              "[007].", "[2147483647].", "[2147483648].", "[1].X [2].Y", "[5].\n", "[5]..", "[5]. ", "[0]", "[", "[1", "[1.", "7].", "[3]]."]
# (The reader looks at the character after the cursor, so the cursor is never at the end of the text.)


def array_row_edge():
    for text in ARRAY_ROWS:
        if text:
            yield reader_case(text)
    yield reader_case("ab[4].c", 2)
    yield reader_case("ab[4].c", 3)


def array_row_random(r):
    text = alphabet_text(r, "[]..0123456789-x A", 12)
    if r.random() < 0.5:
        text = "[" + str(r.randrange(1000)) + r.choice(["]", "]", "]x", "", " ]"]) + r.choice([".", ".", "", "x"]) + alphabet_text(r, "AB. ", 3)
    text = text or "["
    return reader_case(text, r.randrange(len(text)) if r.random() < 0.3 else 0)


NAME_SEPARATORS = [".", ".", " ", "\t", "\n", "..", ". ", " .", "-", "=", ""]


def field_names_edge():
    for text in ["A", "A ", "A.", "A.B", "A.B ", "A B C 1", "A.B.C.D", "1A.B", ".A", "A..B", "A-B", "A.B..", "A\tB", "A\n5", "",
                 " A", "A1.B2.C3 v", "a b", "A.", "A. ", "A.B.", "Aé.B", "A_B.C"]:
        yield reader_case(text)
    for count in (1, 2, 14, 15, 16, 17, 18):
        for ending in (" 5", ".", "..", ". 5", ".x 5", "", "\n"):
            yield reader_case(".".join("N%d" % i for i in range(count)) + ending)
            yield reader_case(" ".join("N%d" % i for i in range(count)) + ending)
    for length in (62, 63, 64, 65, 70):
        yield reader_case("A" * length + ".B 5")
        yield reader_case("A" * length + " 5")
        yield reader_case("x.%s.z " % ("B" * length))
        yield reader_case("Q" * length)


def field_names_random(r):
    parts = []
    for _ in range(r.randint(1, 18)):
        name = r.choice("abcXYZ_ 1") if r.random() < 0.05 else r.choice("abcXYZ")
        parts.append(name + alphabet_text(r, "abc012XYZ", r.choice([0, 3, 8, 70])))
    text = ""
    for name in parts:
        text += name + r.choice(NAME_SEPARATORS)
    text += r.choice(["", "5", "x", "."])
    return reader_case(text, r.choice(starts(text, r)))


SPACES = ["", " ", "\t", "\n", " \n ", "\r\n", "\x0b\x0c"]


def integer_edge():
    for token in INT_GOOD + INT_ODD:
        for space in ("", " ", "\n\t "):
            for tail in ("", " rest", "\nnext", "x", ".5", "#", "\t", "\r"):
                yield reader_case(space + token + tail)
    yield reader_case("   ")
    yield reader_case("-")
    yield reader_case("- 5")
    yield reader_case("5\x80")
    yield reader_case("\xa05")
    yield reader_case("4294967295 ")
    yield reader_case("4294967296 ")
    yield reader_case("-2147483649 ")


def integer_random(r):
    token = r.choice(INT_GOOD + INT_ODD) if r.random() < 0.5 else alphabet_text(r, "0123456789-+.x ", 12)
    text = r.choice(SPACES) + token + r.choice(["", " ", "\n", "x", " 7", "\r", ".", "#"])
    return reader_case(text, r.choice(starts(text, r)))


def float_edge():
    for token in FLOAT_GOOD + FLOAT_ODD + INT_GOOD[:4] + ["1" * 98, "1" * 99, "1" * 100, "-" + "1" * 98, "-" + "1" * 99, "." + "1" * 98,
                                                           "." + "1" * 99, "1" + "." + "1" * 97, "1" * 50 + "." + "1" * 50]:
        for space in ("", " ", "\n\t "):
            for tail in ("", " rest", "\nnext", "x", "#", "\t", "\r"):
                yield reader_case(space + token + tail)
    yield reader_case("   ")
    yield reader_case("-")
    yield reader_case("1.5\x80")


def float_random(r):
    token = r.choice(FLOAT_GOOD + FLOAT_ODD) if r.random() < 0.5 else alphabet_text(r, "0123456789-.. x", 12)
    text = r.choice(SPACES) + token + r.choice(["", " ", "\n", "x", " 7", "\r", ".", "#"])
    return reader_case(text, r.choice(starts(text, r)))


STRINGS = ['"abc"', ' "abc" x', '"abc', '"abc\n"', '""', "abc", "", '"', '"a"b"c"', '"\x80"', ' \n "after newline"', '"x"\n"y"', '  "spaces  inside"  ',
           '"tab\t"', '"a\rb"', '\t"t"', '"\xe9\xff"', '"' + "z" * 300 + '"']


def string_edge():
    for text in STRINGS:
        yield reader_case(text)
        yield reader_case(text, queues={"LbMem_Alloc": [0]})
    yield reader_case('"abc"', 1)
    yield reader_case('x"abc"', 1)


def string_random(r):
    text = r.choice(STRINGS) if r.random() < 0.4 else alphabet_text(r, ' "ab\n#', 10)
    return reader_case(text, r.choice(starts(text, r)), queues={"LbMem_Alloc": [0]} if r.random() < 0.15 else None)


DELIMITERS = ["---", "----", "---\n", "---\nabc", "---abc\ndef", "--", "-", "", "- - -", "--x", "-----------\n\n", "---\r\n", "x---", " ---", "---\n\n",
              "--- text with spaces\nnext", "---\0abc", "-\n--", "---\n---\n"]


def delimit_edge():
    for text in DELIMITERS:
        yield reader_case(text)
    yield reader_case("x---\n", 1)
    yield reader_case("--\n---\n", 3)


def delimit_random(r):
    text = r.choice(["", "-", "--", "---", "----", "--x", "---x"]) + alphabet_text(r, "-x \n", 8)
    return reader_case(text, r.choice(starts(text, r)))


MAP_LINES = ["Scalars.Shape\n", "  Scalars.Shape  \n", "\n\nScalars.Shape\n", "Scalars.Shape\r\n", "Scalars.Shape x y z\n"]


def map_text(r, keys):
    return r.choice(MAP_LINES) + map_block(r, keys)


def map_edge():
    for keys in (1, 2):
        for text in ["Scalars.Shape\n---\n.*Q\n@+#\n---\n", "Scalars.Shape\n---\n---\n", "Scalars.Shape\n---\n" + ".\n" * 20 + "---\n",
                     "Scalars.Shape\n---\n" + ".\n" * 21 + "---\n", "Scalars.Shape\n---\n" + "." * 20 + "\n---\n", "Scalars.Shape\n---\n" + "." * 21 + "\n---\n",
                     "Scalars.Shape\n---\nZ\n---\n", "Scalars.Shape\n--\n.\n---\n", "Scalars.Shape\n---\n.\n", "Scalars.Shape\n---\n.\n--\n",
                     "Scalars.Shape\n", "Scalars.Shape", "", "\n", "Scalars.Shape\n\n", "Scalars.Shape\n---\n .*Q@ +#\n 8624\n---\nafter\n",
                     "Scalars.Shape\n---\n" + SYMBOLS[keys] + "\n" + SYMBOLS[keys][::-1] + "\n---\nx", "Scalars.Shape\n---\n\n\n.\n\n---\n",
                     "Scalars.Shape\n---\n.\n.\n.\n---", "Scalars.Shape\n---- one\n\x80\n---\n", "Scalars.Shape\n---\n" + ("." * 20 + "\n") * 20 + "---\n",
                     "Scalars.Shape\n---\n   \n   \n.\n---\n", "\x80Scalars.Shape\n---\n.\n---\n", "\xff\n---\n.\n---\n",
                     "Scalars.Shape\n---\n.*", "Scalars.Shape\n---\n.\n.*", "Scalars.Shape\n---\n.\n---", "Scalars.Shape\n---\n.   "]:
            yield reader_case(text, keys=keys)
            yield reader_case(text, keys=keys, queues={"LbMem_Alloc": [0]})


def map_random(r):
    keys = r.choice([1, 2])
    text = map_text(r, keys)
    return reader_case(text, r.choice(starts(text, r)), keys=keys, queues={"LbMem_Alloc": [0]} if r.random() < 0.12 else None)


# ================================================================== GAME_ASSERTMSG


class AssertTest:
    """GAME_ASSERTMSG (0x1000dde8) against src/game/advisor/Advisor.cpp: the original spills r5 to r10
    into the caller's frame (the variable argument registers) and returns; the host returns.
    The case compares that both return, that the canary survives, and that the spill is r5
    to r10 as passed (the PowerPC side) against the host's arguments."""

    def __init__(self):
        self.name = "GAME_ASSERTMSG"
        self.edges = [(0, "x", 1, 2, 3, 4, 5), (1, "", 0, 0, 0, 0, 0), (-1, "%s %d", -1, -2, -3, -4, -5), (0x7FFFFFFF, "y", 7, 8, 9, 10, 11)]

    def edge_cases(self):
        return [dict(args=a) for a in self.edges]

    def random_case(self, r):
        return dict(args=(T.rint(r), r.choice(["", "msg", "%d%s"]), T.rint(r), T.rint(r), T.rint(r), T.rint(r), T.rint(r)))

    def run_case(self, runner, case_):
        e, lib = runner.emu, runner.shim("unit_GameAssert")
        e.reset_scratch()
        condition, text, a, b, c, d, f = case_["args"]
        saved = STUBS_REAL_ASSERT.activate(e)
        try:
            address = CL.new_text(e, text)
            sp = T.ppcemu.STACK_BASE + T.ppcemu.STACK_SIZE - 0x1000 - 0x400
            canary = 0x13572468
            e.write(sp + 0x20, struct.pack(">6I", *([0xA5A5A5A5] * 6)))
            r3 = T.call_ppc(e, ASSERT, [condition, address, a, b, c, d, f])[0]
            spill = list(struct.unpack(">6I", e.read(sp + 0x20, 24)))
            ppc = dict(returned=True, r3_unchanged=r3 == T.u32(condition), spill=spill, canary=canary)
        finally:
            CL.Stubs.deactivate(e, saved)
        call = F(lib, "tpw_ga_call", C.c_int32, C.c_int32, C.c_char_p, *([C.c_int32] * 5))
        result = call(condition, text.encode("latin-1"), a, b, c, d, f)
        host = dict(returned=True, r3_unchanged=True, spill=[T.u32(v) for v in (a, b, c, d, f)] + [spill[5]], canary=result)
        return T.same_values(ppc, host)


# ================================================================== the tests

TABLE_TESTS = [
    ("CBalanceTable::CountBalanceDescriptionRows", count_edge, count_random),
    ("CBalanceTable::Init", init_edge, init_random),
    ("CBalanceTable::UnInit", uninit_edge, uninit_random),
    ("CBalanceTable::FreeStrings", free_edge, free_random),
    ("CBalanceTable::InitialiseBalanceValues", initialise_edge, initialise_random),
    ("CBalanceTable::LocateBalanceTableRow", locate_edge, locate_random),
    ("CBalanceTable::AssignValue(long*, long, long)", assign_int_edge, assign_int_random),
    ("CBalanceTable::AssignValue(float*, long, float)", assign_float_edge, assign_float_random),
    ("CBalanceTable::Parse", parse_edge, parse_case),
    ("CBalanceTable::Load(const char*, LoadMode)", load_path_edge, load_path_random),
    ("CBalanceTable::Load(const char*, CFile&, LoadMode)", load_file_edge, load_file_random),
]

TESTS = [FnTest(name, ppc_run, host_run, edge=edge, rnd=rnd) for name, edge, rnd in TABLE_TESTS] + [
    reader_test("read_array_row", ppc_array_row, host_array_row, array_row_edge, array_row_random),
    reader_test("read_field_names", ppc_field_names, host_field_names, field_names_edge, field_names_random),
    reader_test("read_integer_value", ppc_integer, host_integer, integer_edge, integer_random),
    reader_test("read_float_value", ppc_float, host_float, float_edge, float_random),
    reader_test("read_string", ppc_string, host_string, string_edge, string_random),
    reader_test("recognise_delimit", ppc_delimit, host_delimit, delimit_edge, delimit_random),
    reader_test("read_map_internal", ppc_map, host_map, map_edge, map_random),
    AssertTest(),
]
