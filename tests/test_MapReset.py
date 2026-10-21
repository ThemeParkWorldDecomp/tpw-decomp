"""Differential tests for the map reset and the attribute map (unit w112c, main app): LLCC_ResetMap, CMapCell::Reset, CTrackCell::Reset,
HFieldCellStatus, CCellBase::RemoveNeighbour and ModifyDirection, CMap::LoadAttribMap, the chunk file reader (AddChunkHandler, both
ReadFile, the destructor), the handler virtuals CIndividualChunkHandler::IsThisMyChunk and CMapChunkHandler::ParseChunk, the four
TbTList<CIndividualChunkHandler*> members and the script check table (SCR_InitCheckTable, SCR_CloseCheckTable).

The functions run as the original on the PowerPC side and as src/game/park/CellBase.cpp, src/game/park/LLCellSystem.cpp, src/game/park/MapCell.cpp, src/game/park/TPHField.cpp, src/game/park/TrackCell.cpp, src/game/World.cpp, src/game/park/Map.cpp, src/game/save/ChunkFileReader.cpp and src/game/Script.cpp on the
host (libtpw_unit_MapReset.dylib, tests/unit_MapReset.cpp). Everything they call outside those files is a fake that records the same
event on both sides (see calllog.py): the three untranslated cell members CalculateNeighbours, SetHFieldTile and ModifyType (the
cell's index or label and a hash of its bytes at the call; the cases can make a callee write into the cell), TPFile_Open, the file
object (GetSize, Close, Read answering the case's queues), LbMem_Alloc and LbMem_Free, DEBUG_MESSAGE and CExitError::SetError, and
the heap (operator new, new[], delete, delete[]; the sizes of list nodes differ, so only the kind is recorded, and the size of an
array). The handlers of the ReadFile cases are fakes (IsThisMyChunk answers from the ids the case gives, ParseChunk records its
offset into the file buffer, length and first words); LoadAttribMap and ParseChunk run the real handler.

Cells are built from field values (big endian on the PowerPC side, little endian on the host), so the same Python values mean the
same on both sides; the cell is compared field by field afterwards, with guard cells next to it. LLCC_ResetMap works on the world's
whole cell arrays (raw storage on the host): their hash and the first cells are compared, and the fakes' log shows the order.

Unit v5L39: an image test of the script data as loaded (tests/imagedata.py): the code array (its size checks gScriptCodeWordsEnd), the
command list with its ending 0 and gScriptCommandFirst against its TOC word. The other cases write their own.
"""
import ctypes as C
import hashlib
import random
import struct

import calllog as CL
import imagedata
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_MapReset"
PROGRAM = "SimTheme Park"

# ------------------------------------------------------------------ addresses
RESET_MAP, MAP_RESET, TRACK_RESET, HFIELD_STATUS = 0x100916A8, 0x1009CB48, 0x100A3A74, 0x1005B7A8
REMOVE_NEIGHBOUR, MODIFY_DIRECTION, LOAD_ATTRIB = 0x10081678, 0x10081810, 0x1013E5AC
ADD_HANDLER, READ_PATH, READ_FILE, READER_DTOR = 0x100F98C8, 0x100F9D20, 0x100F9940, 0x1013E754
LIST_CTOR, LIST_DTOR, ADD_HEAD, REMOVE_ALL = 0x101412A8, 0x10141320, 0x100F9DD0, 0x101413E0
SCR_INIT, SCR_CLOSE, IS_MY_CHUNK, PARSE_CHUNK = 0x100D8F0C, 0x100D900C, 0x100F9C98, 0x1010276C
OWN = {RESET_MAP, MAP_RESET, TRACK_RESET, HFIELD_STATUS, REMOVE_NEIGHBOUR, MODIFY_DIRECTION, LOAD_ATTRIB, ADD_HANDLER, READ_PATH,
       READ_FILE, READER_DTOR, LIST_CTOR, LIST_DTOR, ADD_HEAD, REMOVE_ALL, SCR_INIT, SCR_CLOSE, IS_MY_CHUNK, PARSE_CHUNK}
CALC_NEIGHBOURS, SET_TILE, MODIFY_TYPE = 0x10099998, 0x1009CDB4, 0x100A0828
TPFILE_OPEN, SET_ERROR, DEBUG_MESSAGE = 0x10003FD4, 0x1014B62C, 0x1006FC90

GAME_WORLD, FLAG_BYTE, HEADER_WORD, BALANCE = 0x1037EBFC, 0x102E4818, 0x102C6C7C, 0x102B4568
EXIT_ERROR = 0x1037F1C0
TABLE_VAR = 0x102F7C10
SLOT_START, SLOT_END, SLOT_LIST, SLOT_FIRST = 0x10261140, 0x10261128, 0x10261118, 0x1026111C
VT_READER, VT_LIST = 0x102A471C, 0x102A4738

MAP_OFF, TRACK_OFF, WORLD_BYTES = 0x2D8, 0x1102D8, 0x1B02D8
SINGLE = 3                                  # the single-cell tests use cell 3 of each array
LABEL_MAP, LABEL_TRACK, LABEL_FILE, LABEL_OBJECT, LABEL_HANDLER = 1, 2, 7, 20, 11     # handlers are 11 to 14

# The Mac's layout of the cells (struct formats without padding; the 28 byte tail of a map cell is never touched).
MAP_FMT = "HBBIiBBHHH3IHBBHBB28s"
TRACK_FMT = "HBBIiBBHHH3IHBBHH"
FMT = {"map": MAP_FMT, "track": TRACK_FMT}
SIZE = {"map": 0x44, "track": 0x28}
LAYOUT = [0, 2, 4, 8, 0xC, 0xD, 0xE, 0x10, 0x14, 0x1C, 0x20, 0x22, 0x24, 0x26, 0x3C, 0x24, 0x44, 0x28]
ID_END, ID_MAP = 0x20444E45, 0x2050414D
MAGIC = 0x4D325054            # "TP2M" as the file has it

FNV_OFFSET, FNV_PRIME, MASK62 = 1469598103934665603, 1099511628211, (1 << 62) - 1


def fnv(data):
    h = FNV_OFFSET
    for b in data:
        h = ((h ^ b) * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h & MASK62


def pack_cell(cell, kind, order):
    return struct.pack(order + FMT[kind], *cell)


def unpack_cell(data, kind, order):
    return struct.unpack(order + FMT[kind], data)


def canon_bytes(data, kind, order):
    """A run of cells re-packed little endian (the host's own bytes), for hashing."""
    if order == "<":
        return bytes(data)
    return b"".join(struct.pack("<" + FMT[kind], *c) for c in struct.iter_unpack(">" + FMT[kind], bytes(data)))


def snap(e, address, kind):
    """The hash the fakes record: the cell's bytes re-packed little endian."""
    return fnv(canon_bytes(e.read(address, SIZE[kind]), kind, ">"))


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


# ------------------------------------------------------------------ cells and fields

def rand_cell(r, kind, cell_id=None, attr=None, ctype=None, kind_byte=None):
    """A map or track cell of random field values; the id, attribute byte, mode and kind byte can be given."""
    def rb():
        return r.getrandbits(8)

    def rh():
        return r.getrandbits(16)

    def rw():
        return r.getrandbits(32)
    cid = rh() if cell_id is None else cell_id
    if kind_byte is None:
        kind_byte = r.choice([1, 2, 2, 0, 3, 0xFF]) if kind == "track" else r.choice([1, 1, 2, 0, 0xFF])
    mode = ctype if ctype is not None else r.choice([0, 1, 2, 5, 7, 0x1E, r.getrandbits(31), -r.getrandbits(31)])
    head = [cid, kind_byte, rb(), rw(), mode, rb(), rb(), rh(), rh(), rh(), rw(), rw(), rw(), rh(), rb(), rb(), rh()]
    if kind == "map":
        return tuple(head + [rb() if attr is None else attr, rb(), r.randbytes(28)])
    return tuple(head + [rh()])


def attr_of(cell):
    return cell[17]


def field_cell_id(x, y):
    return (x + (y << 7) + 1) & 0xFFFF


def make_field(r, x, y, want):
    """(mode, width, height, quads) for a cell at (x, y): want says what HFieldCellStatus must answer.
    in_clear / in_flag: inside the field with the quad's bit 0 clear / set; out_x / out_y: outside; none: no header; nofield: a header
    without a field."""
    if want == "none":
        return (0, 0, 0, [])
    if want == "nofield":
        return (1, 0, 0, [])
    if want in ("in_clear", "in_flag"):
        width, height = x + 1 + r.randrange(3), y + 1 + r.randrange(3)
    elif want == "out_x":
        width, height = max(x - r.randrange(1, 3), 0) if x > 0 else 0, y + 1 + r.randrange(3)
    else:
        width, height = x + 1 + r.randrange(3), max(y - r.randrange(1, 3), 0) if y > 0 else 0
    count = width * height
    if count > 4000:
        return make_field(r, x % 8, y % 8, want)
    quads = [(r.getrandbits(16), r.getrandbits(16)) for _ in range(count)]
    if want in ("in_clear", "in_flag"):
        i = x + y * width
        first = quads[i][0] | 1 if want == "in_flag" else quads[i][0] & 0xFFFE
        quads[i] = (first, quads[i][1])
    return (2, width, height, quads)


def ppc_field(e, field):
    mode, width, height, quads = field
    header = 0
    if mode >= 1:
        header = e.alloc(0x100)
        if mode == 2:
            block = e.alloc(0x30)
            data = e.alloc(4 * max(len(quads), 1) + 16, b"".join(struct.pack(">HH", a, b) for a, b in quads))
            w32(e, block + 0x18, width)
            w32(e, block + 0x1C, height)
            w32(e, block + 0x2C, data)
            w32(e, header + 0x6C, block)
    w32(e, HEADER_WORD, header)


def host_field(lib, field):
    mode, width, height, quads = field
    halves = (C.c_uint16 * max(2 * len(quads), 2))(*[v for q in quads for v in q])
    lib.tpw_mr_field.argtypes = [C.c_int32, C.c_uint32, C.c_uint32, C.c_int32, C.c_void_p]
    lib.tpw_mr_field(mode, width, height, len(quads), C.addressof(halves))


# ------------------------------------------------------------------ the PowerPC fakes

VT = {}
STUBS = None


def cell_id(e, address):
    """The label of a single cell, else its index in the world's arrays (track cells from 0x10000)."""
    lab = e.cl.lab(address)
    if lab != -1:
        return lab
    w = e.world
    if w:
        if w + MAP_OFF <= address < w + MAP_OFF + 0x110000:
            return (address - w - MAP_OFF) // 0x44
        if w + TRACK_OFF <= address < w + WORLD_BYTES:
            return 0x10000 + (address - w - TRACK_OFF) // 0x28
    return -1


def make_vtables(emu):
    """Persistent fake vtables: the file (Close +0xc, Read +0x10, GetSize +0x1c) and the handlers (IsThisMyChunk +8, ParseChunk +0xc)."""
    def getsize(e):
        L = e.cl
        L.rec("CFile::GetSize", (L.lab(e.r(3)),))
        e.set_r(3, T.u32(L.ret("size", e.fcase["length"])))

    def close(e):
        L = e.cl
        L.rec("CFile::Close", (L.lab(e.r(3)),))

    def read(e):
        L = e.cl
        this, dest, size = e.r(3), e.r(4), e.r(5)
        L.rec("CFile::Read", (L.lab(this), L.lab(dest), size))
        n = min(size, e.fcase["length"], e.bufcap)
        if dest and n:
            e.write(dest, e.fcase["data"][:n])
        e.set_r(3, T.u32(L.ret("read", T.s32(size))))

    def is_mine(e):
        L = e.cl
        this, chunk = e.r(3), e.r(4)
        claimed = chunk in e.claims.get(this, ())
        L.rec("IsThisMyChunk", (L.lab(this), chunk, int(claimed)))
        e.set_r(3, int(claimed))

    def parse(e):
        L = e.cl
        this, data, size = e.r(3), e.r(4), e.r(5)
        offset = data - e.buf if e.buf else -1
        words = (0, 0)
        if offset >= 0 and offset + 8 <= e.bufcap:
            words = struct.unpack("<II", e.read(data, 8))
        L.rec("ParseChunk", (L.lab(this), offset, size, words[0], words[1]))

    def vtable(slots):
        vt = T.persist_alloc(emu, 0x40)
        for off, fn in slots.items():
            w32(emu, vt + off, T.fake_function(emu, fn))
        return vt
    VT["file"] = vtable({0xC: close, 0x10: read, 0x1C: getsize})
    VT["handler"] = vtable({0x8: is_mine, 0xC: parse})
    VT["file_object"] = T.persist_alloc(emu, 16)
    w32(emu, VT["file_object"], VT["file"])
    VT["handlers"] = [T.persist_alloc(emu, 16) for _ in range(4)]
    for h in VT["handlers"]:
        w32(emu, h + 4, VT["handler"])


def build_stubs():
    S = CL.Stubs()
    S.real.update(OWN)

    def calc(e):
        L = e.cl
        this = e.r(3)
        L.rec("CalculateNeighbours", (cell_id(e, this), T.s32(e.r(4)), T.s32(e.r(5)), T.s32(e.r(6)), e.r(7), snap(e, this, "map")))
        flags = L.ret("cn flags", -1)
        if flags >= 0:
            e.write(this + 0xE, struct.pack(">H", flags & 0xFFFF))
        mode = L.ret("cn type", -1)
        if mode >= 0:
            w32(e, this + 8, mode)

    def tile(e):
        L = e.cl
        L.rec("SetHFieldTile", (cell_id(e, e.r(3)), snap(e, e.r(3), "map")))

    def modify_type(e):
        L = e.cl
        this = e.r(3)
        L.rec("ModifyType", (cell_id(e, this), T.s32(e.r(4)), snap(e, this, "track")))
        mode = L.ret("mt type", -1)
        if mode >= 0:
            w32(e, this + 8, mode)

    def open_file(e):
        L = e.cl
        L.rec("TPFile_Open", (), (e.cstr(e.r(3))[:159], e.cstr(e.r(4))[:159]))
        e.set_r(3, VT["file_object"] if L.ret("open", 0) != 0 else 0)

    def set_error(e):
        L = e.cl
        text = e.cstr(e.r(6))[:159] if e.r(6) else "(null)"
        L.rec("SetError", (1 if e.r(3) == EXIT_ERROR else 0, T.s32(e.r(4)), e.r(5)), (text,))

    def message(e):
        e.cl.rec("DEBUG_MESSAGE", (e.r(4),), (e.cstr(e.r(3))[:159],))

    def alloc(e):
        L = e.cl
        size = e.r(3)
        if L.ret("alloc", 1) == 0:
            L.rec("LbMem_Alloc", (size, 0))
            e.set_r(3, 0)
            return
        e.bufcap = min(size, 0x4000) + 0x100
        e.buf = e.alloc(e.bufcap)
        L.rec("LbMem_Alloc", (size, L.newlab(e.buf)))
        e.set_r(3, e.buf)

    def free(e):
        L = e.cl
        L.rec("LbMem_Free", (L.lab(e.r(3)),))
        L.forget(e.r(3))

    def op_new(e):
        e.cl.rec("new")
        e.set_r(3, e.alloc(max(e.r(3), 4)))

    def op_new_arr(e):
        e.cl.rec("new[]", (e.r(3),))
        e.set_r(3, e.alloc(max(e.r(3), 4)))

    def op_delete(e):
        # The host's calllog allocator logs a labelled block (the case's reader) with its label, then the plain event.
        block = e.r(3)
        if block:
            label = e.cl.lab(block)
            if label > 0:
                e.cl.rec("delete", (label,))
                e.cl.forget(block)
            e.cl.rec("delete")

    def op_delete_arr(e):
        if e.r(3):
            e.cl.rec("delete[]")

    S.internal(CALC_NEIGHBOURS, calc)
    S.internal(SET_TILE, tile)
    S.internal(MODIFY_TYPE, modify_type)
    S.internal(TPFILE_OPEN, open_file)
    S.internal(SET_ERROR, set_error)
    S.internal(DEBUG_MESSAGE, message)
    S.imp("LbMem_Alloc__FUi", alloc)
    S.imp("LbMem_Free__FPv", free)
    S.imp("__nw__FUl", op_new)
    S.imp("__nwa__FUl", op_new_arr)
    S.imp("__dl__FPv", op_delete)
    S.imp("__dla__FPv", op_delete_arr)
    return S


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    make_vtables(runner.emu)
    STUBS = build_stubs()


# ------------------------------------------------------------------ running one case on each side

class PpcCase:
    """Per-case PowerPC state: the log, the world, the file and the handlers' ids."""

    def __init__(self, e, c):
        self.e = e
        e.cl = CL.PpcLog({}, c.get("q"))
        e.world = 0
        e.buf = 0
        e.bufcap = 0
        e.claims = {}
        e.fcase = {"length": 0, "data": b""}
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)


def ppc_world(e, c, size=WORLD_BYTES):
    """The world's storage with the case's cell arrays; gGameWorld points to it."""
    world = e.alloc(size)
    e.world = world
    for key, off, kind in (("map_cells", MAP_OFF, "map"), ("track_cells", TRACK_OFF, "track")):
        cells = c.get(key)
        if cells:
            e.write(world + off, b"".join(pack_cell(t, kind, ">") for t in cells))
    w32(e, GAME_WORLD, world)
    return world


def host_world(lib, c):
    lib.tpw_mr_world_setup()
    lib.tpw_mr_load.argtypes = [C.c_uint32, C.c_char_p, C.c_uint32]
    lib.tpw_mr_load(0, bytes(WORLD_BYTES), WORLD_BYTES)
    for key, off, kind in (("map_cells", MAP_OFF, "map"), ("track_cells", TRACK_OFF, "track")):
        cells = c.get(key)
        if cells:
            data = b"".join(pack_cell(t, kind, "<") for t in cells)
            lib.tpw_mr_load(off, data, len(data))


def dump_array(buf, kind, order, count, little_first):
    """The first cells decoded, and the hash of the whole run (little endian bytes)."""
    size = SIZE[kind]
    first = [unpack_cell(buf[i * size:(i + 1) * size], kind, order) for i in range(little_first)]
    return {"first": first, "hash": hashlib.md5(canon_bytes(buf, kind, order)).hexdigest()}


def host_begin(lib, c, fixed):
    lib.tpw_mr_addr.restype = C.c_void_p
    lib.tpw_mr_addr.argtypes = [C.c_char_p]
    CL.host_begin(lib, fixed, c.get("q"))


def host_fixed(lib, **names):
    out = {}
    for label, name in names.items():
        out[int(label[1:])] = lib.tpw_mr_addr(name.encode())
    return out


def call_host(lib, op, a=0, b=0, cc=0):
    lib.tpw_mr_call.restype = C.c_int64
    lib.tpw_mr_call.argtypes = [C.c_int32, C.c_int64, C.c_int64, C.c_int64]
    return lib.tpw_mr_call(op, a, b, cc)


# ------------------------------------------------------------------ the single cell tests

OP_RESETMAP, OP_MAPRESET, OP_TRACKRESET, OP_HFIELD, OP_REMOVE, OP_MODDIR, OP_LOADATTRIB, OP_ADDHANDLER, OP_READPATH, OP_READFILE = range(10)
OP_RDTOR, OP_LCTOR, OP_LDTOR, OP_ADDHEAD, OP_REMOVEALL, OP_SCRINIT, OP_SCRCLOSE, OP_ISMINE, OP_PARSE = range(10, 19)

CELL_ADDR = {"map": (MAP_OFF + SINGLE * 0x44, MAP_RESET, LABEL_MAP), "track": (TRACK_OFF + SINGLE * 0x28, TRACK_RESET, LABEL_TRACK)}
GUARD = 0x20


def cell_ppc(e, c):
    p = PpcCase(e, c)
    try:
        kind = c["kind"]
        size = SIZE[kind]
        block = e.alloc(size + 2 * GUARD, b"\x5a" * GUARD + pack_cell(c["cell"], kind, ">") + b"\x5a" * GUARD)
        cell = block + GUARD
        e.cl.labels[cell] = LABEL_MAP if kind == "map" else LABEL_TRACK
        ppc_field(e, c["field"])
        e.write(FLAG_BYTE, bytes([c["flag"]]))
        op = c["op"]
        if op == OP_MAPRESET:
            T.call_ppc(e, MAP_RESET, [cell, c["arg"]])
        elif op == OP_TRACKRESET:
            T.call_ppc(e, TRACK_RESET, [cell])
        elif op == OP_REMOVE:
            T.call_ppc(e, REMOVE_NEIGHBOUR, [cell, c["arg"]])
        else:
            T.call_ppc(e, MODIFY_DIRECTION, [cell, c["arg"]])
        out = {"cell": unpack_cell(e.read(cell, size), kind, ">"), "flag": e.read(FLAG_BYTE, 1)[0], "log": e.cl.log,
               "guard": e.read(block, GUARD) + e.read(cell + size, GUARD) == b"\x5a" * (2 * GUARD)}
    finally:
        p.done()
        w32(e, HEADER_WORD, 0)
        e.write(FLAG_BYTE, b"\0")
    return out


def cell_host(lib, c):
    kind = c["kind"]
    size = SIZE[kind]
    offset = CELL_ADDR[kind][0]
    lib.tpw_mr_world_setup()
    lib.tpw_mr_load.argtypes = [C.c_uint32, C.c_char_p, C.c_uint32]
    lib.tpw_mr_load(offset - GUARD, b"\x5a" * GUARD + pack_cell(c["cell"], kind, "<") + b"\x5a" * GUARD, size + 2 * GUARD)
    host_field(lib, c["field"])
    lib.tpw_mr_set_globals.argtypes = [C.c_uint32, C.c_int32, C.c_int32]
    lib.tpw_mr_set_globals(c["flag"], 0, 0)
    host_begin(lib, c, host_fixed(lib, l1="map1", l2="track1"))
    try:
        call_host(lib, c["op"], c["arg"], 1 if kind == "track" else 0)
    finally:
        log = CL.host_end(lib)
    buf = C.create_string_buffer(size + 2 * GUARD)
    lib.tpw_mr_dump.argtypes = [C.c_uint32, C.c_void_p, C.c_uint32]
    lib.tpw_mr_dump(offset - GUARD, buf, size + 2 * GUARD)
    raw = buf.raw
    lib.tpw_mr_get_flag.restype = C.c_uint32
    out = {"cell": unpack_cell(raw[GUARD:GUARD + size], kind, "<"), "flag": lib.tpw_mr_get_flag(), "log": log,
           "guard": raw[:GUARD] + raw[GUARD + size:] == b"\x5a" * (2 * GUARD)}
    host_field(lib, (0, 0, 0, []))
    return out


class MrTest(FnTest):
    """The result dicts are compared for equality first (a million-event log is slow to walk), then described."""

    def run_case(self, runner, case):
        emu, lib = runner.emu, runner.lib
        emu.reset_scratch()
        try:
            p = self.ppc(emu, case)
        except T.ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(lib, case)
        if p == h:
            return []
        out = []
        for key in sorted(set(p) | set(h)):
            if p.get(key) != h.get(key):
                if key == "log":
                    out.append("log: " + CL.first_difference(p[key], h[key]))
                else:
                    out += T.same_values(p.get(key), h.get(key), key + ".", False)[:3]
        return out or ["results differ"]


def queues_for(r, flags=True):
    """Random answers for the fakes that may write into the cell they are called for."""
    q = {}
    if r.random() < 0.3:
        q["cn flags"] = [r.choice([0x20, 0x40, 0x0, 0xFFFF, r.getrandbits(16)])]
    if r.random() < 0.25:
        q["cn type"] = [r.choice([0, 1, 2, 5, 7, 0x1E, r.getrandbits(31)])]
    if r.random() < 0.3:
        q["mt type"] = [r.choice([0, 1, 7, 0x1E, r.getrandbits(31)])]
    return q


IDS = [1, 2, 3, 0x7F, 0x80, 0x81, 0x100, 0x3FFF, 0x4000, 0x423F, 0x8000, 0xFFFF, 0, 5, 130, 131]
WANTS = ["in_clear", "in_flag", "out_x", "out_y", "none", "nofield"]


def field_for(r, cid, want):
    x, y = (cid - 1) & 0x7F, ((cid - 1) & 0xFFFFFFFF) >> 7
    if want in ("in_clear", "in_flag", "out_y") and y > 2000:
        want = "out_x" if x > 0 else "none"
    if y > 60 or x > 60:
        return make_field(r, x % 5, y % 5, want) if want in ("in_clear", "in_flag") else make_field(r, x, min(y, 60), want)
    return make_field(r, x, y, want)


def reset_case(r, kind, cid=None, attr=None, want=None, full=None, flag=None, quiet=False):
    cid = r.choice(IDS) if cid is None else cid
    want = r.choice(WANTS) if want is None else want
    cell = rand_cell(r, kind, cell_id=cid, attr=attr)
    return {"op": OP_MAPRESET if kind == "map" else OP_TRACKRESET, "kind": kind, "cell": cell, "field": field_for(r, cid, want),
            "arg": r.choice([0, 1]) if full is None else full, "flag": r.choice([0, 1, 2, 0xFF]) if flag is None else flag,
            "q": {} if quiet else queues_for(r)}


def edge_map_reset():
    r = random.Random(100)
    cases = []
    for attr in range(256):
        for k, want in enumerate(WANTS):
            cid = [2, 130, 0x7F, 1, 5, 0x423F][k]
            cases.append(reset_case(r, "map", cid=cid, attr=attr, want=want, full=attr & 1 ^ (k & 1)))
    for cid in IDS:
        for want in WANTS:
            for full in (0, 1):
                cases.append(reset_case(r, "map", cid=cid, want=want, full=full))
    for attr in (0x08, 0x09, 0x0A, 0x80, 0x88, 0xFF, 0x00):          # the callee writing flags and mode into the cell
        for flags in (0x20, 0x40, 0xFFFF):
            for want in ("in_clear", "in_flag"):
                c = reset_case(r, "map", cid=2, attr=attr, want=want, full=1)
                c["q"] = {"cn flags": [flags], "cn type": [r.choice([3, 4, 9])]}
                c["cell"] = c["cell"][:7] + (0,) + c["cell"][8:]
                cases.append(c)
    return cases


def edge_track_reset():
    r = random.Random(101)
    cases = []
    for cid in IDS:
        for want in WANTS:
            for flag in (0, 1):
                for kind_byte in (2, 1):
                    c = reset_case(r, "track", cid=cid, want=want, flag=flag)
                    c["cell"] = c["cell"][:1] + (kind_byte,) + c["cell"][2:]
                    cases.append(c)
    return cases


def rnd_map_reset(r):
    return reset_case(r, "map")


def rnd_track_reset(r):
    return reset_case(r, "track")


def remove_case(r):
    kind = r.choice(["map", "track"])
    return {"op": OP_REMOVE, "kind": kind, "cell": rand_cell(r, kind, kind_byte=r.choice([0, 1, 2, 2, 3, 0xFF])),
            "field": (0, 0, 0, []), "arg": r.choice([0, 1, 0x0F, 0x80, 0xFF, r.getrandbits(8)]),
            "flag": r.choice([0, 1, 2, 0xFF]), "q": {}}


def edge_remove():
    r = random.Random(102)
    cases = []
    for flag in (0, 1, 2, 0xFF):
        for kind_byte in (0, 1, 2, 3, 0xFF):
            for mask in (0, 1, 0x0F, 0x80, 0xFF):
                c = remove_case(r)
                c["flag"], c["arg"] = flag, mask
                c["cell"] = c["cell"][:1] + (kind_byte,) + c["cell"][2:]
                cases.append(c)
    return cases


def moddir_case(r):
    kind = r.choice(["map", "track"])
    return {"op": OP_MODDIR, "kind": kind, "cell": rand_cell(r, kind), "field": (0, 0, 0, []),
            "arg": r.getrandbits(8), "flag": 0, "q": {}}


def edge_moddir():
    r = random.Random(103)
    return [dict(moddir_case(r), arg=v) for v in range(256)]


# ------------------------------------------------------------------ HFieldCellStatus

def hfield_ppc(e, c):
    p = PpcCase(e, c)
    try:
        ppc_field(e, c["field"])
        res = T.s32(T.call_ppc(e, HFIELD_STATUS, [T.u32(c["x"]), T.u32(c["y"])])[0])
    finally:
        p.done()
        w32(e, HEADER_WORD, 0)
    return {"result": res}


def hfield_host(lib, c):
    host_field(lib, c["field"])
    res = call_host(lib, OP_HFIELD, c["x"], c["y"])
    host_field(lib, (0, 0, 0, []))
    return {"result": res}


def hfield_case(r, w=None, h=None, x=None, y=None, mode=2, bit=None):
    w = r.choice([0, 1, 2, 3, 4, 5, 8]) if w is None else w
    h = r.choice([0, 1, 2, 3, 4, 5, 8]) if h is None else h
    pick = [0, 1, 2, 3, 4, 5, 7, -1, 0x7FFFFFFF, -0x80000000, 0x7F, 128, w - 1, w, w + 1, h - 1, h, h + 1]
    x = r.choice(pick) if x is None else x
    y = r.choice(pick) if y is None else y
    quads = []
    for _ in range(w * h):
        v = r.choice([0, 1, 2, 3, 0x800, 0x801, 0xFFFE, 0xFFFF, r.getrandbits(16)]) if bit is None else (r.getrandbits(16) & 0xFFFE) | bit
        quads.append((v, r.getrandbits(16)))
    return {"field": (mode, w, h, quads), "x": x, "y": y}


def edge_hfield():
    r = random.Random(110)
    cases = [hfield_case(r, mode=0), hfield_case(r, mode=1), hfield_case(r, 0, 0, 0, 0), hfield_case(r, 1, 1, 0, 0, bit=0),
             hfield_case(r, 1, 1, 0, 0, bit=1)]
    for w, h in ((1, 1), (2, 3), (4, 4), (3, 2), (8, 1), (1, 8), (5, 5)):
        for x in (0, w - 1, w, w + 1, -1, 0x7FFFFFFF, -0x80000000, 0x7F):
            for y in (0, h - 1, h, h + 1, -1, 0x7FFFFFFF, -0x80000000):
                cases.append(hfield_case(r, w, h, x, y))
    for bit in (0, 1):                                  # the quad read is the one at x + y * width
        for x, y in ((0, 0), (1, 0), (0, 1), (2, 2), (3, 1)):
            cases.append(hfield_case(r, 4, 4, x, y, bit=bit))
    # a width whose product with y wraps in 32 bits: the Mac's address arithmetic wraps, so quad x + y is read
    for y in range(8):
        for x in (0, 1, 3):
            c = hfield_case(r, 1, 1, x, y)
            quads = [(r.getrandbits(16), r.getrandbits(16)) for _ in range(16)]
            c["field"] = (2, 0x40000001, 8, quads)
            cases.append(c)
    return cases


def rnd_hfield(r):
    return hfield_case(r, mode=r.choice([2, 2, 2, 2, 1, 0]))


# ------------------------------------------------------------------ LLCC_ResetMap

def world_cells(r, fill):
    """Map and track cells for the first `fill` places: ids mostly place + 1, attributes and everything else random."""
    maps, tracks = [], []
    for i in range(fill):
        cid = i + 1 if r.random() < 0.9 else r.getrandbits(16)
        maps.append(rand_cell(r, "map", cell_id=cid, kind_byte=1 if r.random() < 0.8 else None))
        tracks.append(rand_cell(r, "track", cell_id=cid, kind_byte=2 if r.random() < 0.8 else None))
    return maps, tracks


def resetmap_case(r, width, height, fill=None, want=None):
    reach = max(width, 0) + (max(height, 1) << 7) if width > 0 and height > 0 else 8
    fill = min(0x4000, reach + 40) if fill is None else fill
    maps, tracks = world_cells(r, fill)
    want = r.choice(WANTS) if want is None else want
    mode = {"in_clear": 2, "in_flag": 2, "out_x": 2, "out_y": 2, "none": 0, "nofield": 1}[want]
    if mode == 2:
        w, h = r.choice([(8, 8), (4, 130), (130, 4), (128, 128), (3, 3)]) if r.random() < 0.5 else (r.randrange(1, 12), r.randrange(1, 12))
        quads = [(r.choice([0, 1, r.getrandbits(16)]), r.getrandbits(16)) for _ in range(w * h)]
        field = (2, w, h, quads)
    else:
        field = (mode, 0, 0, [])
    return {"op": OP_RESETMAP, "w": width, "h": height, "map_cells": maps, "track_cells": tracks, "field": field,
            "flag": r.choice([0, 1, 5]), "q": queues_for(r) if r.random() < 0.3 else {}}


def resetmap_ppc(e, c):
    p = PpcCase(e, c)
    try:
        world = ppc_world(e, c)
        ppc_field(e, c["field"])
        e.write(FLAG_BYTE, bytes([c["flag"]]))
        T.call_ppc(e, RESET_MAP, [T.u32(c["w"]), T.u32(c["h"])], max_insns=800_000_000, timeout_us=600_000_000)
        out = {"log": e.cl.log, "flag": e.read(FLAG_BYTE, 1)[0],
               "map": dump_array(e.read(world + MAP_OFF, 0x110000), "map", ">", 0x4000, 24),
               "track": dump_array(e.read(world + TRACK_OFF, 0xA0000), "track", ">", 0x4000, 24),
               "rest": hashlib.md5(e.read(world, MAP_OFF)).hexdigest()}
    finally:
        p.done()
        w32(e, HEADER_WORD, 0)
        e.write(FLAG_BYTE, b"\0")
        w32(e, GAME_WORLD, 0)
    return out


def resetmap_host(lib, c):
    host_world(lib, c)
    host_field(lib, c["field"])
    lib.tpw_mr_set_globals.argtypes = [C.c_uint32, C.c_int32, C.c_int32]
    lib.tpw_mr_set_globals(c["flag"], 0, 0)
    host_begin(lib, c, {})
    try:
        call_host(lib, OP_RESETMAP, c["w"], c["h"])
    finally:
        log = CL.host_end(lib)
    lib.tpw_mr_dump.argtypes = [C.c_uint32, C.c_void_p, C.c_uint32]

    def dump(off, n):
        buf = C.create_string_buffer(n)
        lib.tpw_mr_dump(off, buf, n)
        return buf.raw
    lib.tpw_mr_get_flag.restype = C.c_uint32
    out = {"log": log, "flag": lib.tpw_mr_get_flag(), "map": dump_array(dump(MAP_OFF, 0x110000), "map", "<", 0x4000, 24),
           "track": dump_array(dump(TRACK_OFF, 0xA0000), "track", "<", 0x4000, 24),
           "rest": hashlib.md5(dump(0, MAP_OFF)).hexdigest()}
    host_field(lib, (0, 0, 0, []))
    return out


def edge_resetmap():
    r = random.Random(120)
    sizes = [(0, 0), (0, 5), (5, 0), (-3, 4), (4, -3), (1, 1), (2, 3), (7, 5), (127, 128), (128, 128), (200, 3), (128, 1), (1, 128),
             (129, 2), (0x7FFFFFFF, 0), (0, 300), (-0x80000000, 5), (3, -0x80000000)]
    cases = []
    for w, h in sizes:
        if w > 0 and h > 0 and w * h > 20000:
            continue
        big = w * h > 4000
        c = resetmap_case(r, w, h, fill=0x4000 if big else None)
        if big and (w, h) == (127, 128):
            c["w"], c["h"] = 127, 128
        cases.append(c)
    return cases


def rnd_resetmap(r):
    w, h = r.randrange(0, 12), r.randrange(0, 8)
    if r.random() < 0.15:
        w, h = r.choice([(r.randrange(100, 140), r.randrange(1, 3)), (r.randrange(1, 5), r.randrange(100, 120))])
    return resetmap_case(r, w, h)


# ------------------------------------------------------------------ the chunk reader, the list and the handlers

def chunk(chunk_id, data=b"", length=None):
    return struct.pack("<II", chunk_id, len(data) if length is None else length) + data


def file_bytes(magic=MAGIC, chunks=(), tail=None, pad=0):
    head = struct.pack("<I", magic) + (tail if tail is not None else bytes(32))
    return head + b"".join(chunks) + bytes(pad)


def list_view_ppc(e, obj):
    """count, the nodes from the head as (handler label, index of the previous node) and the index of the tail node."""
    L = e.cl
    head, tail, count = e.u32(obj + 4), e.u32(obj + 8), T.s32(e.u32(obj + 0xC))
    nodes = []
    node = head
    while node and len(nodes) < 16:
        nodes.append(node)
        node = e.u32(node + 4)
    out = [count, len(nodes)]
    for n in nodes:
        prev = e.u32(n + 8)
        out += [L.lab(e.u32(n)), nodes.index(prev) if prev in nodes else -1]
    out.append(nodes.index(tail) if tail in nodes else -1)
    return out


def list_view_host(lib):
    lib.tpw_mr_list.argtypes = [C.c_void_p, C.c_void_p]
    buf = (C.c_int64 * 40)()
    lib.tpw_mr_list(lib.tpw_mr_addr(b"object"), buf)
    n = buf[1]
    return [buf[0], buf[1]] + [buf[2 + k] for k in range(2 * n)] + [buf[2 + 2 * n]]


def claims_for(c):
    return {i: tuple(ids) for i, ids in c.get("claims", {}).items()}


def ppc_object(e, c):
    """The reader or list of a case: built with the original's list constructor and AddHead, then the log is cleared."""
    kind = c["kind"]
    obj = e.alloc(0x40, b"\xa5" * 0x40)
    T.call_ppc(e, LIST_CTOR, [obj])
    if kind == 1:
        w32(e, obj, VT_READER)
        w32(e, obj + 0x10, c["magic"])
    e.cl.labels[obj] = LABEL_OBJECT
    for i, h in enumerate(VT["handlers"]):
        e.cl.labels[h] = LABEL_HANDLER + i
        w32(e, h, 0x1000 + i)
        w32(e, h + 8, 0)
    e.claims = {VT["handlers"][i]: ids for i, ids in claims_for(c).items()}
    for i in c.get("handlers", []):
        T.call_ppc(e, ADD_HEAD, [obj, VT["handlers"][i]])
    e.cl.log.clear()
    return obj


def host_object(lib, c):
    lib.tpw_mr_object.restype = C.c_void_p
    lib.tpw_mr_object.argtypes = [C.c_int32, C.c_uint32, C.c_void_p, C.c_int32]
    idx = (C.c_int32 * max(len(c.get("handlers", [])), 1))(*c.get("handlers", []))
    lib.tpw_mr_object(c["kind"], c.get("magic", 0), C.addressof(idx), len(c.get("handlers", [])))
    lib.tpw_mr_set_claims.argtypes = [C.c_int32, C.c_void_p, C.c_int32]
    for i in range(4):
        ids = c.get("claims", {}).get(i, ())
        arr = (C.c_uint32 * 4)(*ids)
        lib.tpw_mr_set_claims(i, C.addressof(arr), len(ids))
    lib.tpw_mr_set_file.argtypes = [C.c_char_p, C.c_uint32]
    lib.tpw_mr_set_file(c.get("file", b""), len(c.get("file", b"")))


def reader_fixed(lib):
    fixed = {LABEL_FILE: lib.tpw_mr_addr(b"file"), LABEL_OBJECT: lib.tpw_mr_addr(b"object")}
    for i in range(4):
        fixed[LABEL_HANDLER + i] = lib.tpw_mr_addr(b"h%d" % i)
    return fixed


def reader_ppc(e, c):
    p = PpcCase(e, c)
    try:
        e.fcase = {"length": len(c.get("file", b"")), "data": c.get("file", b"")}
        e.cl.labels[VT["file_object"]] = LABEL_FILE
        obj = ppc_object(e, c)
        op = c["op"]
        res = None
        name = c.get("name")
        text = e.alloc(len(name) + 1, name.encode("latin-1") + b"\0") if name is not None else 0
        path = e.alloc(len(c["path"]) + 1, c["path"].encode("latin-1") + b"\0")
        if op == OP_ADDHANDLER:
            T.call_ppc(e, ADD_HANDLER, [obj, VT["handlers"][c["arg"]]])
        elif op == OP_READPATH:
            res = T.s32(T.call_ppc(e, READ_PATH, [obj, path])[0])
        elif op == OP_READFILE:
            res = T.s32(T.call_ppc(e, READ_FILE, [obj, VT["file_object"], text])[0])
        elif op == OP_RDTOR:
            T.call_ppc(e, READER_DTOR, [obj if c["arg"] else 0, T.u32(c["flag"])])
        elif op == OP_LDTOR:
            T.call_ppc(e, LIST_DTOR, [obj if c["arg"] else 0, T.u32(c["flag"])])
        elif op == OP_ADDHEAD:
            T.call_ppc(e, ADD_HEAD, [obj, VT["handlers"][c["arg"]]])
        elif op == OP_REMOVEALL:
            T.call_ppc(e, REMOVE_ALL, [obj])
        out = {"result": res, "log": e.cl.log}
        if not (op in (OP_RDTOR, OP_LDTOR) and c["flag"] > 0 and c["arg"]):
            out["list"] = list_view_ppc(e, obj)
            if c["kind"] == 1:
                out["magic"] = e.u32(obj + 0x10)
    finally:
        p.done()
    return out


def reader_host(lib, c):
    host_object(lib, c)
    name = c.get("name")
    text = C.create_string_buffer(name.encode("latin-1")) if name is not None else None
    path = C.create_string_buffer(c["path"].encode("latin-1"))
    host_begin(lib, c, reader_fixed(lib))
    op = c["op"]
    res = None
    try:
        if op == OP_ADDHANDLER:
            call_host(lib, OP_ADDHANDLER, c["arg"])
        elif op == OP_READPATH:
            res = call_host(lib, OP_READPATH, C.addressof(path))
        elif op == OP_READFILE:
            res = call_host(lib, OP_READFILE, C.addressof(text) if text else 0)
        elif op == OP_RDTOR:
            call_host(lib, OP_RDTOR, 1 if c["arg"] else 0, c["flag"])
        elif op == OP_LDTOR:
            call_host(lib, OP_LDTOR, 1 if c["arg"] else 0, c["flag"])
        elif op == OP_ADDHEAD:
            call_host(lib, OP_ADDHEAD, c["arg"])
        elif op == OP_REMOVEALL:
            call_host(lib, OP_REMOVEALL)
    finally:
        log = CL.host_end(lib)
    out = {"result": res, "log": log}
    if not (op in (OP_RDTOR, OP_LDTOR) and c["flag"] > 0 and c["arg"]):
        out["list"] = list_view_host(lib)
        if c["kind"] == 1:
            lib.tpw_mr_magic.restype = C.c_uint32
            out["magic"] = lib.tpw_mr_magic()
    return out


# ------------------------------------------------------------------ the reader cases

def reader_case(op, **kw):
    c = {"op": op, "kind": 1, "magic": MAGIC, "handlers": [], "claims": {}, "file": b"", "name": None, "path": "HD:TPW:Terrain:Base.map",
         "q": {}, "arg": 0, "flag": 0}
    c.update(kw)
    return c


def edge_readfile():
    r = random.Random(130)
    cases = []

    def add(data, **kw):
        cases.append(reader_case(OP_READFILE, file=data, **kw))
    claim0 = {0: [ID_MAP]}
    end = chunk(ID_END)
    # the sizes around the header, the size and read failures
    for n in (0, 1, 3, 4, 0x24, 0x2B, 0x2C, 0x2D, 0x30):
        add((struct.pack("<I", MAGIC) + bytes(n))[:n], handlers=[0], claims=claim0, name="ReadFile")
    add(file_bytes(chunks=[end]), q={"size": [-1]}, name="size.map")
    add(file_bytes(chunks=[end]), q={"size": [-2]}, name=None)
    add(file_bytes(chunks=[end]), q={"size": [0x7FFFFFFF], "read": [0]}, name="big.map")
    add(file_bytes(chunks=[end]), q={"size": [0x80000000 - 0x100], "read": [5]}, name="big.map")
    add(file_bytes(chunks=[end]), q={"alloc": [0]}, name="nomem.map")
    for delta in (-1, 1, 0x1000):
        add(file_bytes(chunks=[end]), q={"read": [0x2C + delta]}, name="short.map")
    for value in (0, -1, 0x7FFFFFFF):
        add(file_bytes(chunks=[end]), q={"read": [value]}, name="short.map")
    # the magic
    for magic in (MAGIC ^ 1, MAGIC ^ 0x80000000, 0x504F3254 if False else 0x504C324D, 0, 0xFFFFFFFF):
        add(file_bytes(magic=magic, chunks=[end]), handlers=[0], claims=claim0)
    # the chunks
    add(file_bytes(chunks=[end]), handlers=[0], claims=claim0)
    add(file_bytes(chunks=[end]))
    add(file_bytes(chunks=[chunk(ID_END, b"\1\2\3\4")]), handlers=[0], claims=claim0)
    add(file_bytes(chunks=[chunk(ID_MAP, b"abcdefgh"), end]), handlers=[0], claims=claim0)
    add(file_bytes(chunks=[chunk(ID_MAP, b"abcdefgh"), end]), handlers=[0, 1], claims={0: [ID_MAP], 1: [ID_MAP]})
    add(file_bytes(chunks=[chunk(ID_MAP, b"abcdefgh"), end]), handlers=[0, 1, 2], claims={0: [ID_MAP], 1: [5], 2: [ID_MAP, 5]})
    add(file_bytes(chunks=[chunk(0x41424344, b"xy"), chunk(ID_MAP, b"123"), end]), handlers=[0, 1], claims={0: [ID_MAP], 1: [0x41424344]})
    add(file_bytes(chunks=[chunk(0x41424344, b"xyz"), end]), handlers=[0], claims=claim0)
    add(file_bytes(chunks=[chunk(0x44434241, bytes(range(20))), end]), handlers=[0, 1, 2, 3], claims={})
    add(file_bytes(chunks=[chunk(ID_MAP, b"\1" * 7), chunk(ID_MAP, b"\2" * 5), end]), handlers=[0], claims=claim0)
    # a chunk whose length reaches exactly the end, and one byte past it
    add(file_bytes(chunks=[chunk(ID_MAP, b"abcd")]), handlers=[0], claims=claim0)
    add(file_bytes(chunks=[chunk(ID_MAP, b"abcd", length=5)]), handlers=[0], claims=claim0)
    add(file_bytes(chunks=[chunk(ID_MAP, b"abcd", length=3), end]), handlers=[0], claims=claim0)
    add(file_bytes(chunks=[chunk(ID_MAP, b"abcd", length=0x7FFFFFFF), end]), handlers=[0], claims=claim0)
    add(file_bytes(chunks=[chunk(ID_MAP, b"abcd"), end, chunk(ID_MAP, b"zz")]), handlers=[0], claims=claim0)
    # a trailing header exactly at the end, bytes too few for one, and an END that is the last bytes
    for pad in range(0, 10):
        add(file_bytes(chunks=[chunk(ID_MAP, b"abcd")], pad=pad), handlers=[0], claims=claim0)
        add(file_bytes(chunks=[chunk(ID_MAP, b"abcd"), end], pad=pad), handlers=[0], claims=claim0)
    # lengths that wrap the 32 bit sum: the walk goes on from a low offset (it ends on the claimed length of the header's bytes)
    tail = struct.pack("<I", 0x00FFFFFF) + bytes(28)
    for k in (0, 0x25, 0x26, 0x28, 0x2A, 0x2B):
        length = (k - 0x2C) & 0xFFFFFFFF
        for claims in ({}, claim0):
            add(file_bytes(tail=tail, chunks=[chunk(ID_MAP, length=length)], pad=r.choice([0, 3, 9])), handlers=[0], claims=claims)
    # (a length of 0xFFFFFFF0 walks back to the same chunk for ever on the Mac too: not a case)
    # names and the exit error value
    add(file_bytes(chunks=[end]), q={"size": [-1]}, name="x" * 200)
    add(file_bytes(chunks=[end]), q={"alloc": [0]}, name="")
    return cases


def random_file(r):
    chunks = []
    ids = [ID_MAP, ID_END, 0x41424344, r.getrandbits(32), 5]
    for _ in range(r.choice([0, 1, 1, 2, 3, 4])):
        cid = r.choice(ids)
        data = r.randbytes(r.choice([0, 0, 1, 3, 4, 8, 13, 20]))
        length = None
        if r.random() < 0.12:
            length = max(len(data) + r.choice([-2, -1, 1, 2, 9, 1000]), 0)
        chunks.append(chunk(cid, data, length))
    if r.random() < 0.75:
        chunks.append(chunk(ID_END))
    if r.random() < 0.2:
        chunks.append(chunk(r.choice(ids), r.randbytes(3)))
    magic = MAGIC if r.random() < 0.88 else r.choice([MAGIC ^ r.getrandbits(32) | 1, 0, 0x504C324D])
    return file_bytes(magic=magic, chunks=chunks, tail=r.randbytes(32) if r.random() < 0.3 else None, pad=r.choice([0, 0, 1, 7, 8, 9]))


def rnd_readfile(r):
    handlers = r.sample(range(4), r.choice([0, 1, 1, 2, 3, 4]))
    claims = {i: [r.choice([ID_MAP, ID_END, 0x41424344, 5]) for _ in range(r.choice([0, 1, 1, 2]))] for i in range(4)}
    q = {}
    roll = r.random()
    if roll < 0.04:
        q["size"] = [r.choice([-1, -5, 0, 0x2B, 0x7FFFFFFF])]
        q["read"] = [r.choice([0, 3, -1])]
    elif roll < 0.08:
        q["alloc"] = [0]
    elif roll < 0.13:
        q["read"] = [r.choice([0, 5, -1, 0x30, 0x100000])]
    return reader_case(OP_READFILE, file=random_file(r), handlers=handlers, claims=claims, magic=MAGIC,
                       name=r.choice([None, "base.map", "", "n" * 70]), q=q)


def edge_readpath():
    r = random.Random(131)
    cases = []
    for opened in (0, 1):
        for path in ("HD:TPW:Terrain:Base.map", "", "x", "a" * 120):
            f = file_bytes(chunks=[chunk(ID_MAP, b"1234"), chunk(ID_END)])
            cases.append(reader_case(OP_READPATH, file=f, path=path, handlers=[0], claims={0: [ID_MAP]}, q={"open": [opened]}))
    cases.append(reader_case(OP_READPATH, file=b"", path="short", handlers=[0], q={"open": [1], "size": [-1]}))
    cases.append(reader_case(OP_READPATH, file=file_bytes(), path="p", q={"open": [1], "alloc": [0]}))
    return cases


def rnd_readpath(r):
    c = rnd_readfile(r)
    c["op"] = OP_READPATH
    c["path"] = r.choice(["HD:TPW:Terrain:Base.map", "", "x", "a b.map", "z" * 100])
    c["q"] = dict(c["q"], open=[r.choice([0, 1, 1, 1])])
    return c


def edge_addhandler():
    r = random.Random(132)
    cases = []
    for have in ([], [0], [0, 1], [3, 2, 1], [0, 1, 2, 3]):
        for arg in (0, 1, 2, 3):
            cases.append(reader_case(OP_ADDHANDLER, handlers=have, arg=arg))
    return cases


def rnd_addhandler(r):
    return reader_case(OP_ADDHANDLER, handlers=[r.randrange(4) for _ in range(r.randrange(0, 6))], arg=r.randrange(4),
                       magic=r.getrandbits(32))


def edge_reader_dtor():
    cases = []
    for flag in (0, 1, -1, 0x7FFF, -0x8000, 2, 0x100):
        for have in ([], [0], [0, 1, 2]):
            cases.append(reader_case(OP_RDTOR, handlers=have, flag=flag, arg=1))
    cases.append(reader_case(OP_RDTOR, handlers=[0], flag=1, arg=0))
    cases.append(reader_case(OP_RDTOR, handlers=[0], flag=0, arg=0))
    cases.append(reader_case(OP_RDTOR, handlers=[0], flag=-1, arg=0))
    return cases


def rnd_reader_dtor(r):
    return reader_case(OP_RDTOR, handlers=[r.randrange(4) for _ in range(r.randrange(0, 5))], flag=r.choice([0, 1, -1, 2, r.randrange(-0x8000, 0x8000)]),
                       arg=1 if r.random() < 0.9 else 0, magic=r.getrandbits(32))


def list_case(op, **kw):
    return reader_case(op, kind=0, **kw)


def edge_list_dtor():
    cases = []
    for flag in (0, 1, -1, 0x7FFF, -0x8000, 2):
        for have in ([], [0], [0, 1, 2, 3]):
            cases.append(list_case(OP_LDTOR, handlers=have, flag=flag, arg=1))
    cases.append(list_case(OP_LDTOR, handlers=[1], flag=1, arg=0))
    cases.append(list_case(OP_LDTOR, handlers=[1], flag=0, arg=0))
    return cases


def rnd_list_dtor(r):
    return list_case(OP_LDTOR, handlers=[r.randrange(4) for _ in range(r.randrange(0, 5))],
                     flag=r.choice([0, 1, -1, 2, r.randrange(-0x8000, 0x8000)]), arg=1 if r.random() < 0.9 else 0)


def edge_addhead():
    return [list_case(OP_ADDHEAD, handlers=have, arg=arg) for have in ([], [0], [0, 1], [0, 1, 2, 3], [3, 3, 2]) for arg in (0, 1, 3)]


def rnd_addhead(r):
    return list_case(OP_ADDHEAD, handlers=[r.randrange(4) for _ in range(r.randrange(0, 7))], arg=r.randrange(4))


def edge_removeall():
    return [list_case(OP_REMOVEALL, handlers=have) for have in ([], [0], [0, 1], [0, 1, 2, 3], [2, 2, 2, 2, 2])]


def rnd_removeall(r):
    return list_case(OP_REMOVEALL, handlers=[r.randrange(4) for _ in range(r.randrange(0, 9))])


# The list constructor runs on a block of one byte value; its three fields and nothing else change (checked over 0x10 bytes).
def lctor_ppc(e, c):
    p = PpcCase(e, c)
    try:
        block = e.alloc(0x40, bytes([c["fill"]]) * 0x40)
        T.call_ppc(e, LIST_CTOR, [block])
        out = {"words": [e.u32(block + 4), e.u32(block + 8), e.u32(block + 0xC)], "vt": e.u32(block) == VT_LIST,
               "rest": e.read(block + 0x10, 0x30) == bytes([c["fill"]]) * 0x30, "log": e.cl.log}
    finally:
        p.done()
    return out


def lctor_host(lib, c):
    lib.tpw_mr_storage.restype = C.c_void_p
    lib.tpw_mr_storage(c["fill"])
    host_begin(lib, c, {})
    try:
        call_host(lib, OP_LCTOR)
    finally:
        log = CL.host_end(lib)
    lib.tpw_mr_storage_state.restype = C.c_int32
    state = lib.tpw_mr_storage_state(c["fill"])
    return {"words": lctor_words(lib), "vt": (state & 2) != 0, "rest": (state & 1) != 0, "log": log}


def lctor_words(lib):
    lib.tpw_mr_storage_words.argtypes = [C.c_void_p]
    buf = (C.c_uint64 * 3)()
    lib.tpw_mr_storage_words(C.addressof(buf))
    return [int(buf[0]), int(buf[1]), int(buf[2])]


# ------------------------------------------------------------------ CIndividualChunkHandler::IsThisMyChunk

def ismine_ppc(e, c):
    p = PpcCase(e, c)
    try:
        obj = e.alloc(16, struct.pack(">IIII", c["id"], 0x102A46F0, 0, 0))
        res = T.call_ppc(e, IS_MY_CHUNK, [obj, T.u32(c["chunk"])])[0]
    finally:
        p.done()
    return {"result": res}


def ismine_host(lib, c):
    lib.tpw_mr_set_handler_id.argtypes = [C.c_int32, C.c_uint32]
    lib.tpw_mr_set_handler_id(0, c["id"])
    return {"result": call_host(lib, OP_ISMINE, c["chunk"]) & 0xFFFFFFFF}


def ismine_case(r, a=None, b=None):
    a = r.choice([ID_MAP, ID_END, 0, 1, 0xFFFFFFFF, 0x80000000, r.getrandbits(32)]) if a is None else a
    b = r.choice([a, a, a ^ 1, (a + 1) & 0xFFFFFFFF, (a - 1) & 0xFFFFFFFF, a ^ 0x80000000, a ^ 0x01000000, r.getrandbits(32)]) if b is None else b
    return {"id": a, "chunk": b, "q": {}}


def edge_ismine():
    r = random.Random(140)
    cases = []
    for a in (ID_MAP, ID_END, 0, 1, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF):
        for b in (a, a ^ 1, (a + 1) & 0xFFFFFFFF, (a - 1) & 0xFFFFFFFF, a ^ 0x80000000, a ^ 0x00010000, ID_MAP):
            cases.append(ismine_case(r, a, b))
    return cases


def rnd_ismine(r):
    return ismine_case(r)


# ------------------------------------------------------------------ CMapChunkHandler::ParseChunk and CMap::LoadAttribMap

def attr_data(r, w, h, extra=0):
    """A MAP chunk's data: the two extents, five unused words, w * h bytes (x-major: h rows of w bytes)."""
    return struct.pack("<II", w, h) + r.randbytes(20) + r.randbytes(w * h + extra)


def parse_case(r, w=None, h=None, xs=None, ys=None, fill=None):
    w = r.choice([0, 1, 2, 5, 8, 12, 20, 130, 140]) if w is None else w
    h = r.choice([0, 1, 2, 3, 5, 9, 130, 140]) if h is None else h
    if w * h > 4000:
        w, h = min(w, 140), min(h, 25)
    pick = [0, 1, 2, 3, 5, -1, -2, -3, -5, 127, 128, 126, 129, -127, -128, 100, -100, 0x7FFFFFFF, -0x80000000]
    xs = r.choice(pick) if xs is None else xs
    ys = r.choice(pick) if ys is None else ys
    fill = r.choice([0, 300, 900]) if fill is None else fill
    maps, _ = world_cells(r, fill) if fill else ([], [])
    return {"w": w, "h": h, "xs": xs, "ys": ys, "map_cells": maps, "data": attr_data(r, w, h), "q": {}}


def set_balance_ppc(e, c):
    w32(e, BALANCE + 0x544, c["xs"])
    w32(e, BALANCE + 0x548, c["ys"])


def parse_ppc(e, c):
    p = PpcCase(e, c)
    try:
        world = ppc_world(e, c, MAP_OFF + 0x110000)
        set_balance_ppc(e, c)
        data = e.alloc(len(c["data"]) + 8, c["data"])
        handler = e.alloc(16, struct.pack(">III", ID_MAP, 0x102A0B10, world + MAP_OFF))
        T.call_ppc(e, PARSE_CHUNK, [handler, data, len(c["data"])])
        out = {"map": dump_array(e.read(world + MAP_OFF, 0x110000), "map", ">", 0x4000, 24), "log": e.cl.log}
    finally:
        p.done()
        w32(e, BALANCE + 0x544, 0)
        w32(e, BALANCE + 0x548, 0)
        w32(e, GAME_WORLD, 0)
    return out


def map_dump_host(lib):
    lib.tpw_mr_dump.argtypes = [C.c_uint32, C.c_void_p, C.c_uint32]
    buf = C.create_string_buffer(0x110000)
    lib.tpw_mr_dump(MAP_OFF, buf, 0x110000)
    return dump_array(buf.raw, "map", "<", 0x4000, 24)


def parse_host(lib, c):
    host_world(lib, c)
    lib.tpw_mr_set_globals.argtypes = [C.c_uint32, C.c_int32, C.c_int32]
    lib.tpw_mr_set_globals(0, c["xs"], c["ys"])
    data = C.create_string_buffer(c["data"], len(c["data"]) + 8)
    host_begin(lib, c, {})
    try:
        call_host(lib, OP_PARSE, C.addressof(data), ID_MAP, len(c["data"]))
    finally:
        log = CL.host_end(lib)
    lib.tpw_mr_set_globals(0, 0, 0)
    return {"map": map_dump_host(lib), "log": log}


def edge_parse():
    r = random.Random(150)
    cases = []
    for w, h in ((0, 0), (1, 1), (2, 3), (3, 2), (5, 5), (130, 3), (3, 130), (128, 128), (130, 20)):
        for xs, ys in ((0, 0), (1, 2), (-1, -2), (127, 127), (128, 128), (-127, -127), (126, 3), (3, 126)):
            cases.append(parse_case(r, w, h, xs, ys, fill=300))
    for w, h in ((8, 8), (4, 130)):
        for xs, ys in ((0x7FFFFFFF, 0), (0, 0x7FFFFFFF), (-0x80000000, 0), (0, -0x80000000), (-0x80000000, -0x80000000)):
            cases.append(parse_case(r, w, h, xs, ys, fill=100))
    return cases


def rnd_parse(r):
    return parse_case(r)


def loadattrib_case(r):
    c = parse_case(r, w=r.choice([0, 1, 3, 6, 11, 20]), h=r.choice([0, 1, 2, 4, 9]), fill=r.choice([0, 300]))
    chunks = []
    roll = r.random()
    if roll < 0.8:
        chunks.append(chunk(ID_MAP, c["data"]))
    elif roll < 0.9:
        chunks.append(chunk(0x41424344, b"abc"))
    if r.random() < 0.9:
        chunks.append(chunk(ID_END))
    magic = MAGIC if r.random() < 0.9 else 0x504C324D
    c["file"] = file_bytes(magic=magic, chunks=chunks, pad=r.choice([0, 0, 3]))
    c["path"] = r.choice(["HD:TPW:Terrain:Base.map", "x"])
    q = {"open": [1 if r.random() < 0.92 else 0]}
    roll = r.random()
    if roll < 0.04:
        q["size"] = [-1]
    elif roll < 0.08:
        q["alloc"] = [0]
    elif roll < 0.12:
        q["read"] = [r.choice([0, 3, -1])]
    c["q"] = q
    return c


def edge_loadattrib():
    r = random.Random(151)
    cases = [loadattrib_case(r) for _ in range(30)]
    for k in range(6):
        c = loadattrib_case(r)
        c["q"] = {"open": [1]}
        c["file"] = file_bytes(chunks=[chunk(ID_MAP, attr_data(r, 4 + k, 3 + k)), chunk(ID_END)])
        c["xs"], c["ys"] = r.choice([(0, 0), (1, 1), (-1, 2)])
        cases.append(c)
    return cases


def loadattrib_ppc(e, c):
    p = PpcCase(e, c)
    try:
        world = ppc_world(e, c, MAP_OFF + 0x110000)
        set_balance_ppc(e, c)
        e.fcase = {"length": len(c["file"]), "data": c["file"]}
        e.cl.labels[VT["file_object"]] = LABEL_FILE
        path = e.alloc(len(c["path"]) + 1, c["path"].encode("latin-1") + b"\0")
        res = T.s32(T.call_ppc(e, LOAD_ATTRIB, [world + MAP_OFF, path])[0])
        out = {"result": res, "map": dump_array(e.read(world + MAP_OFF, 0x110000), "map", ">", 0x4000, 24), "log": e.cl.log}
    finally:
        p.done()
        w32(e, BALANCE + 0x544, 0)
        w32(e, BALANCE + 0x548, 0)
        w32(e, GAME_WORLD, 0)
    return out


def loadattrib_host(lib, c):
    host_world(lib, c)
    lib.tpw_mr_set_globals.argtypes = [C.c_uint32, C.c_int32, C.c_int32]
    lib.tpw_mr_set_globals(0, c["xs"], c["ys"])
    lib.tpw_mr_set_file.argtypes = [C.c_char_p, C.c_uint32]
    lib.tpw_mr_set_file(c["file"], len(c["file"]))
    path = C.create_string_buffer(c["path"].encode("latin-1"))
    host_begin(lib, c, {LABEL_FILE: lib.tpw_mr_addr(b"file")})
    try:
        res = call_host(lib, OP_LOADATTRIB, C.addressof(path))
    finally:
        log = CL.host_end(lib)
    lib.tpw_mr_set_globals(0, 0, 0)
    return {"result": res, "map": map_dump_host(lib), "log": log}


# ------------------------------------------------------------------ the script check table

def script_case(r, n=None, extra=None, op=OP_SCRINIT, prev=None):
    n = r.choice([0, 1, 2, 3, 4, 5, 6, 40, 100, 883, r.randrange(0, 200)]) if n is None else n
    base = r.choice([0x10266958, 0x7FFFFFF0, 0xFFFFFFE0, 0x1000, 0x80000000 - 4, r.getrandbits(32) & 0xFFFFFF00])
    first = r.choice([base, base, base + 8, max(base - 16, 0), r.getrandbits(32)]) & 0xFFFFFFFF
    commands = [(base + r.choice([0, 8, 16, 24, 0x138, 4, 200]) * r.choice([1, 1, 1, 0])) & 0xFFFFFFFF for _ in range(r.choice([0, 1, 2, 5, 42]))]
    commands = [v if v else 4 for v in commands]
    pool = [v for v in commands + [first]]
    lo, hi = min(pool), max(pool)
    edge = [lo, hi, (lo - 1) & 0xFFFFFFFF, (hi + 1) & 0xFFFFFFFF, 0, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0x80000001]
    words = []
    for _ in range(n):
        roll = r.random()
        if roll < 0.35:
            words.append(r.choice(edge))
        elif roll < 0.7 and hi >= lo:
            words.append(r.randint(lo, hi))
        else:
            words.append(r.getrandbits(32))
    return {"op": op, "first": first, "commands": commands, "words": words, "extra": r.choice([0, 0, 1, 2, 3]) if extra is None else extra,
            "prev": r.random() < 0.5 if prev is None else prev, "q": {}}


def edge_script_init():
    r = random.Random(160)
    cases = []
    for n in (0, 1, 2, 3, 4, 5, 63, 64, 100, 883, 900):
        for extra in (0, 1, 2, 3):
            cases.append(script_case(r, n, extra))
    for prev in (False, True):
        for n in (0, 7):
            cases.append(script_case(r, n, 0, prev=prev))
    c = script_case(r, 20, 0)                                       # the default word alone, no list
    c["commands"] = []
    cases.append(c)
    c = script_case(r, 10, 0)                                       # the signed boundary: unsigned compares
    c["first"], c["commands"] = 0x7FFFFFFF, [0x80000001, 0xFFFFFFFF, 1]
    c["words"] = [0, 1, 2, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000, 0x80000001, 0x80000002, 0xFFFFFFFE, 0xFFFFFFFF]
    cases.append(c)
    c = script_case(r, 8, 0)
    c["first"], c["commands"] = 0, [0xFFFFFFFF]
    c["words"] = [0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF, 5, 6]
    cases.append(c)
    c = script_case(r, 8, 0)
    c["first"], c["commands"] = 0xFFFFFFFF, [3, 0x80000000]
    c["words"] = [0, 2, 3, 4, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFF]
    cases.append(c)
    return cases


def rnd_script_init(r):
    return script_case(r)


def script_ppc(e, c):
    p = PpcCase(e, c)
    saved = [e.u32(a) for a in (SLOT_START, SLOT_END, SLOT_LIST, SLOT_FIRST, TABLE_VAR)]
    try:
        words = e.alloc(4 * len(c["words"]) + 16, b"".join(struct.pack(">I", v) for v in c["words"]))
        commands = e.alloc(4 * len(c["commands"]) + 16, b"".join(struct.pack(">I", v) for v in c["commands"]) + bytes(4))
        w32(e, SLOT_START, words)
        w32(e, SLOT_END, words + 4 * len(c["words"]) + c["extra"])
        w32(e, SLOT_LIST, commands)
        w32(e, SLOT_FIRST, c["first"])
        w32(e, TABLE_VAR, e.alloc(8) if c["prev"] else 0)
        T.call_ppc(e, SCR_INIT if c["op"] == OP_SCRINIT else SCR_CLOSE, [])
        table = e.u32(TABLE_VAR)
        out = {"present": table != 0, "table": list(e.read(table, len(c["words"]))) if table and c["op"] == OP_SCRINIT else [], "log": e.cl.log}
    finally:
        p.done()
        for a, v in zip((SLOT_START, SLOT_END, SLOT_LIST, SLOT_FIRST, TABLE_VAR), saved):
            w32(e, a, v)
    return out


def script_host(lib, c):
    lib.tpw_mr_script.argtypes = [C.c_void_p, C.c_int32, C.c_int32, C.c_void_p, C.c_int32, C.c_uint32]
    words = (C.c_uint32 * max(len(c["words"]), 1))(*c["words"])
    commands = (C.c_uint32 * max(len(c["commands"]), 1))(*c["commands"])
    lib.tpw_mr_script(C.addressof(words), c["extra"], len(c["words"]), C.addressof(commands), len(c["commands"]), c["first"])
    lib.tpw_mr_table_set(1 if c["prev"] else 0)
    host_begin(lib, c, {})
    try:
        call_host(lib, c["op"])
    finally:
        log = CL.host_end(lib)
    n = len(c["words"])
    buf = C.create_string_buffer(max(n, 1))
    lib.tpw_mr_table_get.restype = C.c_int32
    lib.tpw_mr_table_get.argtypes = [C.c_void_p, C.c_int32]
    present = lib.tpw_mr_table_get(buf, n)
    lib.tpw_mr_table_set(0)
    return {"present": present != 0, "table": list(buf.raw[:n]) if present and c["op"] == OP_SCRINIT else [], "log": log}


def edge_script_close():
    return [script_case(random.Random(161), 6, 0, op=OP_SCRCLOSE, prev=p) for p in (False, True)]


def rnd_script_close(r):
    return script_case(r, r.randrange(0, 20), 0, op=OP_SCRCLOSE)


# ------------------------------------------------------------------ the cell layout

def layout_ppc(e, c):
    return {"offsets": LAYOUT}


def layout_host(lib, c):
    buf = (C.c_int32 * len(LAYOUT))()
    lib.tpw_mr_layout.argtypes = [C.c_void_p]
    lib.tpw_mr_layout(C.addressof(buf))
    return {"offsets": list(buf)}


# ------------------------------------------------------------------ the tests

def t(name, ppc, host, edge, rnd):
    return MrTest(name, ppc, host, edge=edge, rnd=rnd)


TESTS = [
    t("LLCC_ResetMap", resetmap_ppc, resetmap_host, edge_resetmap, rnd_resetmap),
    t("CMapCell::Reset", cell_ppc, cell_host, edge_map_reset, rnd_map_reset),
    t("CTrackCell::Reset", cell_ppc, cell_host, edge_track_reset, rnd_track_reset),
    t("HFieldCellStatus", hfield_ppc, hfield_host, edge_hfield, rnd_hfield),
    t("CCellBase::RemoveNeighbour", cell_ppc, cell_host, edge_remove, remove_case),
    t("CCellBase::ModifyDirection", cell_ppc, cell_host, edge_moddir, moddir_case),
    t("CMap::LoadAttribMap", loadattrib_ppc, loadattrib_host, edge_loadattrib, loadattrib_case),
    t("CChunkFileReader::AddChunkHandler", reader_ppc, reader_host, edge_addhandler, rnd_addhandler),
    t("CChunkFileReader::ReadFile(const char*)", reader_ppc, reader_host, edge_readpath, rnd_readpath),
    t("CChunkFileReader::ReadFile(CFile&, char*)", reader_ppc, reader_host, edge_readfile, rnd_readfile),
    t("CChunkFileReader::~CChunkFileReader", reader_ppc, reader_host, edge_reader_dtor, rnd_reader_dtor),
    t("TbTList::TbTList", lctor_ppc, lctor_host, lambda: [{"fill": f, "q": {}} for f in (0, 0xA5, 0xFF, 0x5A)],
      lambda r: {"fill": r.getrandbits(8), "q": {}}),
    t("TbTList::~TbTList", reader_ppc, reader_host, edge_list_dtor, rnd_list_dtor),
    t("TbTList::AddHead", reader_ppc, reader_host, edge_addhead, rnd_addhead),
    t("TbTList::RemoveAll", reader_ppc, reader_host, edge_removeall, rnd_removeall),
    t("SCR_InitCheckTable", script_ppc, script_host, edge_script_init, rnd_script_init),
    t("SCR_CloseCheckTable", script_ppc, script_host, edge_script_close, rnd_script_close),
    t("CIndividualChunkHandler::IsThisMyChunk", ismine_ppc, ismine_host, edge_ismine, rnd_ismine),
    t("CMapChunkHandler::ParseChunk", parse_ppc, parse_host, edge_parse, rnd_parse),
    t("cell layout", layout_ppc, layout_host, lambda: [{"q": {}}], lambda r: {"q": {}}),
    imagedata.table_test("the script data", [("code", 0x1029F20C, "I", 883), ("commands", 0x1029FFEC, "I", 43), ("first", 0x1026111C, "I", 1)]),
]
