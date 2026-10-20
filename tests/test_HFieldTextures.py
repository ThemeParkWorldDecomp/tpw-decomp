"""Differential test for src/HFieldTextures.cpp: the level height field's texture table
(BuildHFieldTextureTable, ParseLevelTCT, FreeHFieldTextureTable), the mesh's texture arrays
(InterceptAndInsertM3D2Textures, ReplaceTextures) and DetermineHFieldFlatness, against
libtpw_unit_HFieldTextures.dylib.

One scenario runs on both machines through the same code. A Side object hides the
differences: the byte order, how a Ptr32 field is written (an address on the PowerPC side,
an offset from the arena's base on the host), where memory comes from and how the table is
read (the table is the program's own object: 0x30 bytes of 32-bit fields on the Mac, native
pointers on the host). The scenario builds a mesh header (a file image) from a spec, runs a
list of operations and, after each, dumps text lines: the results, the table, the header
with its texture infos, textures, face sets and height field quads, and the raw bytes of
the blocks the functions made (pointer words masked, so that nothing outside the records
can change unnoticed). Pointers are shown as a label and an offset in the block they point
into. The two sets of lines and the two call logs must be equal.

What is a fake (recorded on both sides): the file functions, the game's allocator, the theme
name and path, FindFilePath and the case-blind compare. The text reader (GetString and its
character tests) is the real code on both sides, and so is the C library. Numbers in a
.tct that overflow a 32-bit int are included: the Mac's atoi saturates, so does the translation's.
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_HFieldTextures"

MASK = 0xFFFFFFFF
TABLE, ENTRIES, GAME = 0x1029CC2C, 0x1029CC14, 0x1037F134
BUILD, PARSE, FREE, FLATNESS, INTERCEPT, REPLACE = 0x100319C0, 0x10031220, 0x10031B10, 0x1005B9D0, 0x100B8350, 0x100B7E78
# The text reader (TPAdvisor.cpp): real code on both sides.
READER = [0x10030C08, 0x10030D28, 0x10030D80, 0x10030DEC, 0x10030E44, 0x10030EDC, 0x10031014]
OWN = [BUILD, PARSE, FREE, FLATNESS, INTERCEPT, REPLACE] + READER
TPFILE_OPEN, TPFILE_SIZE, TPFILE_READ, TPFILE_CLOSE = 0x10003FD4, 0x100044E8, 0x10004260, 0x10004354
ALLOC, DEALLOC = 0x10064BD4, 0x10064C54
THEME_NAME, THEME_PATH, FIND_FILE = 0x1014A058, 0x1015139C, 0x100597F8

# Labels of the objects the scenario builds (the blocks the code asks for get 100 and up).
L_GAME, L_HEADER, L_INFOS, L_TEXTURES, L_NAMES, L_FOLDER, L_OBJECT, L_NODES, L_HFIELD, L_HEIGHTS, L_QUADS = (
    1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19)
L_FACESETS = 20     # 20 + node
L_STRING = 40       # 40 + number: strings the fakes hand out
L_MAP, L_NEW_TEXTURES = 60, 61

SPACE = b" \t\r\n\x0b\x0c"


# ------------------------------------------------------------------ the C library, for the PowerPC side

def atoi(text):
    """C's atoi for the numbers the tests use (saturating like the Mac's)."""
    i = 0
    while i < len(text) and text[i] in SPACE:
        i += 1
    sign = 1
    if i < len(text) and text[i] in b"+-":
        sign = -1 if text[i] == ord("-") else 1
        i += 1
    value = 0
    while i < len(text) and 48 <= text[i] <= 57:
        value = value * 10 + text[i] - 48
        i += 1
    value *= sign
    return max(-0x80000000, min(0x7FFFFFFF, value))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ------------------------------------------------------------------ the two machines

class Side:
    """What a scenario needs of a machine (see the module comment)."""

    def new_text(self, text, label):
        data = text.encode("latin-1") + b"\0"
        address = self.new(len(data), label)
        self.write(address, data)
        return address

    def put(self, address, fmt, *values):
        self.write(address, struct.pack(self.order + fmt, *values))

    def get(self, address, fmt):
        return struct.unpack(self.order + fmt, self.read(address, struct.calcsize(self.order + fmt)))

    def where(self, address):
        """A pointer as the label of the block it points into and the offset there."""
        if address == 0:
            return "null"
        label, offset = self.locate(address)
        return f"L{label}+{offset}" if label else "ext"

    def pointer_text(self, value):
        return self.where(self.address(value))

    def text_at(self, value):
        """The text a Ptr32 field points to."""
        address = self.address(value)
        return self.cstr(address) if address else None


class PpcSide(Side):
    """The original code in the emulator; Ptr32 fields hold addresses."""

    order = ">"

    def __init__(self, emu):
        self.e = emu
        self.regions = []
        self.sizes = {}
        self.old = None
        self.saved = (emu.read(TABLE, 0x30), emu.read(ENTRIES, 0x18))

    def new(self, size, label):
        address = self.e.alloc(size + 16)
        self.regions.append((address, max(size, 1), label))
        self.sizes[label] = size
        return address

    def read(self, address, size):
        return self.e.read(address, size)

    def write(self, address, data):
        self.e.write(address, data)

    def cstr(self, address):
        return self.e.cstr(address, limit=1 << 16)

    def locate(self, address):
        for start, size, label in self.regions:
            if start <= address < start + size:
                return label, address - start
        return 0, 0

    def pointer(self, address):
        return address

    def address(self, value):
        return value

    def prepare(self, case):
        """The world the fakes answer from, and the table as the program starts."""
        e = self.e
        strings = case["strings"]
        self.text = case["text"]
        self.tail = case["tail"]
        self.theme = self.new(0x20, L_STRING)
        CL.tds_set(e, self.theme, strings["theme"])
        self.path = self.new_text(strings["path"], L_STRING + 1)
        self.found = self.new_text(strings["found"], L_STRING + 2) if strings["found"] is not None else 0
        self.search = self.new_text(strings["search"], L_STRING + 3) if strings["search"] is not None else 0
        names = self.saved[1]
        entries = struct.pack(">IiI", struct.unpack(">I", names[0:4])[0], 0, 0)
        entries += struct.pack(">IiI", struct.unpack(">I", names[12:16])[0], 0, 0)
        e.write(ENTRIES, entries)
        e.write(TABLE, struct.pack(">iIiiIiIIIIiI", case["categories"], ENTRIES, 0, case["bytes"], 0, 0, 0, 0, 0, 0, 0, 0))

    def start(self, case):
        e = self.e
        fixed = {GAME: L_GAME}
        queues = {"Open": case["open"], "Size": case["size"]}
        e.cl = self.log = CL.PpcLog(fixed, queues)
        st = CL.Stubs()
        st.real.update(OWN)

        # CGame::GetCurrentThemeName: the string object (its text pointer at +8).
        def theme_name(e):
            e.cl.rec("GetCurrentThemeName", (e.cl.lab(e.r(3)),))
            e.set_r(3, self.theme)

        # get_theme_path: a string.
        def theme_path(e):
            e.cl.rec("get_theme_path")
            e.set_r(3, self.path)

        # FindFilePath(name, flags, result): the file name for flag 1, else the search path.
        def find_file(e):
            e.cl.rec("FindFilePath", (e.r(4), int(e.r(5) != 0)), (e.cstr(e.r(3))[:159],))
            e.set_r(3, self.found if e.r(4) == 1 else self.search)

        # TPFile_Open: a handle (a new label) unless the queue "Open" says no.
        def open_file(e):
            e.cl.rec("TPFile_Open", (), (e.cstr(e.r(3))[:159], e.cstr(e.r(4))[:159]))
            if e.cl.ret("Open", 1) == 0:
                e.set_r(3, 0)
                return
            handle = e.alloc(16)
            e.cl.newlab(handle)
            e.set_r(3, handle)

        # TPFile_Size: the text's length unless the queue "Size" says otherwise.
        def file_size(e):
            e.cl.rec("TPFile_Size", (e.cl.lab(e.r(3)),))
            e.set_r(3, T.u32(e.cl.ret("Size", len(self.text))))

        # TPFile_Read: the text, zeros after it, then the tail (the slack of the block).
        def read_file(e):
            destination, size, count = e.r(3), e.r(4), e.r(5)
            e.cl.rec("TPFile_Read", (size, count, e.cl.lab(destination), e.cl.lab(e.r(6))))
            bytes_ = (size * count) & MASK
            if bytes_:
                e.write(destination, (self.text + bytes(bytes_))[:bytes_] + self.tail)
            e.set_r(3, count)

        # TPFile_Close.
        def close_file(e):
            e.cl.rec("TPFile_Close", (e.cl.lab(e.r(3)),))
            e.set_r(3, 0)

        # MReleaseAllocF: a zeroed block with room after it (none for size 0), kept in the regions.
        def allocate(e):
            size, flags = e.r(3), e.r(4)
            e.cl.rec("MReleaseAllocF", (size, flags))
            if size == 0:
                e.set_r(3, 0)
                return
            address = e.alloc(size + 16)
            label = e.cl.newlab(address)
            self.regions.append((address, size, label))
            self.sizes[label] = size
            e.set_r(3, address)

        # MReleaseFreeF: recorded; the memory stays readable.
        def release(e):
            e.cl.rec("MReleaseFreeF", (e.cl.lab(e.r(3)),))
            e.set_r(3, 0)

        # The case-blind compare (ASCII only), recorded.
        def stricmp(e):
            a, b = e.cstr(e.r(3), limit=1 << 16), e.cstr(e.r(4), limit=1 << 16)
            e.cl.rec("_stricmp", (), (a[:159], b[:159]))
            x, y = a.encode("latin-1").lower(), b.encode("latin-1").lower()
            e.set_r(3, T.u32((x > y) - (x < y)))

        # atoi, saturating like the Mac's.
        def number(e):
            e.set_r(3, T.u32(atoi(e.cstr(e.r(3), limit=64).encode("latin-1"))))

        # strncpy: the text cut at `count` bytes, padded with zeros.
        def strncpy(e):
            destination, source, count = e.r(3), e.r(4), e.r(5)
            data = e.cstr(source, limit=1 << 16).encode("latin-1")[:count]
            e.write(destination, (data + bytes(count))[:count])

        st.internal(THEME_NAME, theme_name)
        st.internal(THEME_PATH, theme_path)
        st.internal(FIND_FILE, find_file)
        st.internal(TPFILE_OPEN, open_file)
        st.internal(TPFILE_SIZE, file_size)
        st.internal(TPFILE_READ, read_file)
        st.internal(TPFILE_CLOSE, close_file)
        st.internal(ALLOC, allocate)
        st.internal(DEALLOC, release)
        st.imp("_stricmp__10NS_MacDozeFPCcPCc", stricmp)
        st.imp("atoi", number)
        st.imp("strncpy", strncpy)
        self.old = st.activate(e)

    def stop(self):
        CL.Stubs.deactivate(self.e, self.old)
        self.old = None
        return self.log.log

    def close(self):
        if self.old is not None:
            CL.Stubs.deactivate(self.e, self.old)
        self.e.write(TABLE, self.saved[0])
        self.e.write(ENTRIES, self.saved[1])

    def table(self):
        words = struct.unpack(">iIiiIiIIIIiI", self.e.read(TABLE, 0x30))
        keys = "count entries values bytes block stored infos oldInfos textures oldTextures added names".split()
        return dict(zip(keys, words))

    def entry(self, entries, i):
        name, count, values = struct.unpack(">IiI", self.e.read(entries + 12 * i, 12))
        return name, count, values

    def set_table(self, **fields):
        table = self.table()
        table.update(fields)
        self.e.write(TABLE, struct.pack(">iIiiIiIIIIiI", *table.values()))

    def call(self, address, *args):
        return T.call_ppc(self.e, address, list(args))[0]


class HostSide(Side):
    """The translation in the unit library; Ptr32 fields hold offsets from the arena's base."""

    order = "<"

    class Table(C.Structure):
        _fields_ = [("count", C.c_int32), ("entries", C.c_void_p), ("values", C.c_int32), ("bytes", C.c_int32),
                    ("block", C.c_void_p), ("stored", C.c_int32), ("infos", C.c_void_p), ("oldInfos", C.c_void_p),
                    ("textures", C.c_void_p), ("oldTextures", C.c_void_p), ("added", C.c_int32), ("names", C.c_void_p)]

    class Entry(C.Structure):
        _fields_ = [("name", C.c_void_p), ("count", C.c_int32), ("values", C.c_void_p)]

    def __init__(self, lib):
        self.lib = lib
        lib.tpw_hft_alloc.restype = C.c_void_p
        lib.tpw_hft_alloc.argtypes = [C.c_uint32, C.c_int32]
        lib.tpw_hft_arena_base.restype = C.c_void_p
        lib.tpw_hft_game.restype = C.c_void_p
        lib.tpw_hft_table.restype = C.c_void_p
        lib.tpw_hft_locate.restype = C.c_int32
        lib.tpw_hft_locate.argtypes = [C.c_void_p, C.POINTER(C.c_int64)]
        lib.tpw_hft_reset.argtypes = [C.c_char_p, C.c_char_p, C.c_char_p, C.c_char_p, C.c_char_p, C.c_int32, C.c_char_p,
                                      C.c_int32]
        lib.tpw_hft_table_reset.argtypes = [C.c_int32, C.c_int32]
        lib.tpw_hft_parse.argtypes = [C.c_void_p]
        lib.tpw_hft_intercept.argtypes = [C.c_void_p, C.c_void_p]
        lib.tpw_hft_replace.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_int32, C.c_uint32]
        lib.tpw_hft_build.argtypes = [C.c_void_p]
        lib.tpw_hft_flatness.argtypes = [C.c_void_p]
        self.base = lib.tpw_hft_arena_base()
        assert C.sizeof(self.Table) == lib.tpw_hft_table_size()

    def new(self, size, label):
        return self.lib.tpw_hft_alloc(size, label)

    def read(self, address, size):
        return C.string_at(address, size)

    def write(self, address, data):
        C.memmove(address, data, len(data))

    def cstr(self, address):
        return C.string_at(address).decode("latin-1")

    def locate(self, address):
        offset = C.c_int64()
        label = self.lib.tpw_hft_locate(address, C.byref(offset))
        return label, offset.value

    def pointer(self, address):
        return address - self.base if address else 0

    def address(self, value):
        return self.base + value if value else 0

    def prepare(self, case):
        strings = case["strings"]
        text = case["text"]
        self.lib.tpw_hft_reset(strings["theme"].encode("latin-1"), strings["path"].encode("latin-1"),
                               None if strings["found"] is None else strings["found"].encode("latin-1"),
                               None if strings["search"] is None else strings["search"].encode("latin-1"), text,
                               len(text), case["tail"], len(case["tail"]))
        self.lib.tpw_hft_table_reset(case["categories"], case["bytes"])
        self.table_object = self.Table.from_address(self.lib.tpw_hft_table())

    def start(self, case):
        CL.host_begin(self.lib, {L_GAME: self.lib.tpw_hft_game()}, {"Open": case["open"], "Size": case["size"]})

    def stop(self):
        return CL.host_end(self.lib)

    def close(self):
        pass

    def table(self):
        t = self.table_object
        return dict(count=t.count, entries=t.entries or 0, values=t.values, bytes=t.bytes, block=t.block or 0,
                    stored=t.stored, infos=t.infos or 0, oldInfos=t.oldInfos or 0, textures=t.textures or 0,
                    oldTextures=t.oldTextures or 0, added=t.added, names=t.names or 0)

    def entry(self, entries, i):
        entry = self.Entry.from_address(entries + C.sizeof(self.Entry) * i)
        return entry.name or 0, entry.count, entry.values or 0

    def set_table(self, **fields):
        t = self.table_object
        for key, value in fields.items():
            setattr(t, key, value)


# ------------------------------------------------------------------ the mesh header

def build_header(S, spec):
    """A mesh header (a file image) from a spec: textures (name, flags, frames, ...), nodes
    (each a list of face sets, each the number of a texture or None) and a height field
    (quads, or a grid with heights)."""
    textures = spec["textures"]
    h = S.new(0xB8, L_HEADER)
    S.write(h, spec["pattern"][:0xB8])
    infos = S.new(max(len(textures), 1) * 0x10, L_INFOS)
    records = S.new(max(len(textures), 1) * 8, L_TEXTURES)
    names = S.new(max(len(textures), 1) * 0x14, L_NAMES)
    folder = S.new_text(spec["folder"], L_FOLDER)
    obj = S.new(16, L_OBJECT)
    for i, texture in enumerate(textures):
        name = names + 0x14 * i
        S.write(name, texture["name"].encode("latin-1")[:0x13].ljust(0x14, b"\0"))
        S.put(infos + 0x10 * i, "IIBBHI", texture["flags"], S.pointer(folder), texture["f08"][0], texture["f08"][1],
              texture["frames"], S.pointer(name))
        S.put(records + 8 * i, "HHI", texture["f00"], texture["f02"], S.pointer(obj) if texture["object"] else 0)
    S.put(h + 0x36, "H", len(textures))
    S.put(h + 0x50, "II", S.pointer(records), S.pointer(infos))
    nodes = spec["nodes"]
    block = S.new(max(len(nodes), 1) * 0xA0, L_NODES)
    for n, face_sets in enumerate(nodes):
        node = block + 0xA0 * n
        S.write(node, spec["pattern"][0x100 + 0xA0 * n:0x100 + 0xA0 * (n + 1)])
        sets = S.new(max(len(face_sets), 1) * 0x10, L_FACESETS + min(n, 19))
        for k, texture in enumerate(face_sets):
            pointer = S.pointer(records + 8 * texture) if texture is not None else 0
            S.put(sets + 0x10 * k, "IIII", pointer, 0x1000 + k, 0x2000 + n, 0x3000 + k * n)
        S.put(node + 0x5A, "H", len(face_sets))
        S.put(node + 0x6C, "I", S.pointer(sets))
    S.put(h + 0x44, "H", len(nodes))
    S.put(h + 0x70, "I", S.pointer(block) if nodes else 0)
    field = spec["hfield"]
    S.put(h + 0x6C, "I", 0)
    if field is not None:
        quads = field["quads"]
        grid = field["grid"]
        heights = field["heights"]
        hf = S.new(0x30, L_HFIELD)
        S.write(hf, spec["pattern"][0x3D0:0x400])
        height_block = S.new(max(len(heights), 1) * 4, L_HEIGHTS)
        for i, value in enumerate(heights):
            S.put(height_block + 4 * i, "f", value)
        quad_block = S.new(max(len(quads), 1) * 4, L_QUADS)
        for i, (first, second) in enumerate(quads):
            S.put(quad_block + 4 * i, "HH", first, second)
        S.put(hf + 4, "HH", len(heights) & 0xFFFF, len(quads))
        S.put(hf + 0x18, "II", grid[0], grid[1])
        S.put(hf + 0x28, "II", S.pointer(height_block), S.pointer(quad_block))
        S.put(h + 0x6C, "I", S.pointer(hf))
    return h


def masked(S, address, size, ranges):
    """The bytes of a record as hex, but for the byte ranges that hold fields the dump shows
    in their own way (numbers wider than a byte and pointers differ between the machines)."""
    data = bytearray(S.read(address, size))
    for start, length in ranges:
        data[start:start + length] = bytes(length)
    return data.hex()


def dump_infos(S, lines, tag, infos, textures, count):
    """Every field of `count` texture infos and textures."""
    for i in range(count):
        flags, folder, a, b, frames, name = S.get(infos + 0x10 * i, "IIBBHI")
        name_address = S.address(name)
        name_bytes = S.read(name_address, 0x14).hex() if name_address else None
        lines.append(f"{tag} info {i} flags={flags:#x} folder={S.pointer_text(folder)} {S.text_at(folder)!r} f08={a},{b} "
                     f"frames={frames} name={S.where(name_address)} {name_bytes}")
        f00, f02, obj = S.get(textures + 8 * i, "HHI")
        lines.append(f"{tag} texture {i} {f00:#x} {f02:#x} {S.pointer_text(obj)}")


def dump_header(S, h, lines, tag):
    """The header, its texture infos, textures, face sets and quads."""
    raw = masked(S, h, 0xB8, [(0x36, 2), (0x44, 2), (0x4C, 4), (0x50, 8), (0x6C, 8)])
    count, nodes = S.get(h + 0x36, "H")[0], S.get(h + 0x44, "H")[0]
    textures, infos, field, node_array = (S.address(S.get(h + o, "I")[0]) for o in (0x50, 0x54, 0x6C, 0x70))
    lines.append(f"{tag} header count={count} nodes={nodes} textures={S.where(textures)} infos={S.where(infos)} "
                 f"field={S.where(field)} nodeArray={S.where(node_array)} raw={raw}")
    dump_infos(S, lines, tag, infos, textures, count)
    for n in range(nodes):
        node = node_array + 0xA0 * n
        sets = S.address(S.get(node + 0x6C, "I")[0])
        number, = S.get(node + 0x5A, "H")
        lines.append(f"{tag} node {n} sets={number} at {S.where(sets)} raw={masked(S, node, 0xA0, [(0x5A, 2), (0x6C, 4)])}")
        for k in range(number):
            texture, *others = S.get(sets + 0x10 * k, "IIII")
            texture = S.address(texture)
            same_block = texture and S.locate(texture)[0] == S.locate(textures)[0]
            relative = (texture - textures) // 8 if same_block and (texture - textures) % 8 == 0 else None
            lines.append(f"{tag} node {n} set {k} texture={S.where(texture)} rel={relative} others={others}")
    if field:
        height_count, quad_count = S.get(field + 4, "HH")
        grid = S.get(field + 0x18, "II")
        quads = S.address(S.get(field + 0x2C, "I")[0])
        words = S.get(quads, "H" * (2 * quad_count)) if quad_count else ()
        lines.append(f"{tag} field counts={height_count},{quad_count} grid={grid} raw={masked(S, field, 0x30, [(4, 4), (0x18, 8), (0x28, 8)])} "
                     f"quads={list(words)}")


def dump_text_area(S, lines, tag, address, start):
    """The raw bytes of a block the code asked for from `start` on (size known from the
    allocator): the part with the names, which has no multi-byte numbers."""
    if not address:
        lines.append(f"{tag} block null")
        return
    label, offset = S.locate(address)
    size = S.size_of(label)
    length = max(size - start, 0)
    lines.append(f"{tag} text {S.where(address)} size={size} from={start} {S.read(address + start, length).hex()}")


def dump_table(S, lines, tag):
    """The table: fields, categories, records, and the blocks."""
    t = S.table()
    lines.append(f"{tag} table count={t['count']} values={t['values']} bytes={t['bytes']} stored={t['stored']} added={t['added']}")
    lines.append(f"{tag} table pointers entries={S.where(t['entries'])} block={S.where(t['block'])} infos={S.where(t['infos'])} "
                 f"oldInfos={S.where(t['oldInfos'])} textures={S.where(t['textures'])} oldTextures={S.where(t['oldTextures'])} "
                 f"names={S.where(t['names'])}")
    records = 0
    for i in range(min(max(t["count"], 0), 8)):
        name, count, values = S.entry(t["entries"], i)
        lines.append(f"{tag} entry {i} {S.cstr(name)!r} count={count} values={S.where(values)}")
        if values and 0 < count < 2000:
            for j in range(count):
                number, text, index, pad = S.get(values + 12 * j, "iIHH")
                lines.append(f"{tag} value {i}.{j} number={number} text={S.pointer_text(text)} {S.text_at(text)!r} "
                             f"index={index} pad={pad}")
        records += max(count, 0)
    # What lies after the records of the table's block, and of the block of new infos and textures.
    if t["block"] and S.locate(t["block"])[0]:
        dump_text_area(S, lines, tag + " values", t["block"], 12 * records)
    if t["infos"] and S.locate(t["infos"])[0]:
        dump_text_area(S, lines, tag + " infos", t["infos"], 0x18 * (t["added"] + t["stored"]))
    if t["names"] and S.locate(t["names"])[0]:
        dump_text_area(S, lines, tag + " names", t["names"], 0)


# ------------------------------------------------------------------ the scenario

def scenario(S, case):
    """Builds the world, runs the case's operations and returns the dump and the call log."""
    lines = []
    results = []
    S.prepare(case)
    header = build_header(S, case["header"]) if case["header"] is not None else 0
    # The header's own texture arrays, which the functions must leave as they were.
    old_arrays = None
    if header:
        old_arrays = (S.address(S.get(header + 0x54, "I")[0]), S.address(S.get(header + 0x50, "I")[0]), S.get(header + 0x36, "H")[0])
    objects = {}
    objects["file"] = S.new_text("level.tct", L_STRING + 10)
    objects["folder"] = S.new_text(case["folder"], L_STRING + 11)
    if case["map"] is not None:
        count = max(len(case["map"]), 1)
        objects["map"] = S.new(count * 4, L_MAP)
        for i, value in enumerate(case["map"]):
            S.put(objects["map"] + 4 * i, "i", value)
        objects["textures"] = S.new(case["new_textures"] * 8, L_NEW_TEXTURES)
    S.start(case)
    try:
        for op in case["ops"]:
            if op == "parse":
                results.append((op, S.call_parse(objects["file"])))
            elif op == "intercept":
                results.append((op, S.call_intercept(header, objects["folder"])))
            elif op == "replace":
                S.call_replace(header, objects["map"], objects["textures"], case["kept"], case["total"])
            elif op == "build":
                results.append((op, S.call_build(header)))
            elif op == "free":
                if "pointers" in case:
                    pointers = {}
                    for key, label in case["pointers"].items():
                        pointers[key] = S.new(32, label)
                    S.set_table(**pointers)
                S.call_free()
            elif op == "flatness":
                S.call_flatness(header)
            lines.append(f"== {op} {results[-1][1] if results and results[-1][0] == op else ''}")
            dump_table(S, lines, op)
            if header:
                dump_header(S, header, lines, op)
                dump_infos(S, lines, op + " old", *old_arrays)
            if "map" in objects and op == "replace":
                dump_text_area(S, lines, "textures", objects["textures"], 0)
    finally:
        log = S.stop()
    return dict(lines=lines, log=log)


def ppc(e, case):
    S = PpcSide(e)
    S.size_of = lambda label: S.sizes[label]
    S.call_parse = lambda file: S.call(PARSE, file, TABLE)
    S.call_intercept = lambda h, folder: S.call(INTERCEPT, h, TABLE, folder)
    S.call_replace = lambda h, m, t, kept, total: S.call(REPLACE, h, m, t, kept & MASK, total & MASK)
    S.call_build = lambda h: S.call(BUILD, h)
    S.call_free = lambda: S.call(FREE)
    S.call_flatness = lambda h: S.call(FLATNESS, h)
    try:
        return scenario(S, case)
    finally:
        S.close()


def host(lib, case):
    S = HostSide(lib)
    lib.tpw_hft_size.restype = C.c_int64
    lib.tpw_hft_size.argtypes = [C.c_int32]
    S.size_of = lambda label: lib.tpw_hft_size(label)
    S.call_parse = lambda file: lib.tpw_hft_parse(file)
    S.call_intercept = lambda h, folder: lib.tpw_hft_intercept(h, folder)
    S.call_replace = lambda h, m, t, kept, total: lib.tpw_hft_replace(h, m, t, kept, total & MASK)
    S.call_build = lambda h: lib.tpw_hft_build(h)
    S.call_free = lambda: lib.tpw_hft_free()
    S.call_flatness = lambda h: lib.tpw_hft_flatness(h)
    try:
        return scenario(S, case)
    finally:
        S.close()


# ------------------------------------------------------------------ cases

NAMES = ["jpa_squ1.tga", "jpa_end1.tga", "baseblue.tga", "dark.tga", "jpa_que4.tga", "grnarrow.tga", "a", "x.y",
         "Jpa_Str1.TGA", "nineteen_chars__.tga", "twenty_chars_long_.tga", "much_longer_than_twenty_chars.tga"]
# Names that start with something other than a letter: a number's file name is whatever
# punctuation or digit follows its white space.
PUNCT_NAMES = ["_a.tga", "[b", "{c}", "}d", "|e", "~f", "`g", "^h", "\\i", "]j", "@k", ":l", ";m", "#n", "!o", "/p", "9q.tga",
               "-r", "+s", "'t", ".u", "\x80v", "\x7fw"]
HEADINGS = ["PathTex", "QueueTex", "pathtex", "QUEUETEX", "Water", "PathTex2", "Path", "PathTex  ", "PathTex#c", "q"]


def pattern(r):
    """Bytes for the parts of a header that no function should touch."""
    return r.randbytes(0x400)


def tct_text(r, noise=0.1):
    """A texture correspondence table: headings and numbered lines, comments, blank lines,
    and now and then something the reader should skip."""
    end = r.choice(["\r\n", "\r\n", "\n", "\r", "\n\n"])
    lines = []
    for _ in range(r.randrange(0, 4)):
        lines.append(r.choice(["#", "# comment", ";x", "#\t Theme Park 2", "#0\tcentre.tga", " # indented"]))
    for _ in range(r.randrange(0, 5)):
        lines.append(r.choice(HEADINGS))
        for _ in range(r.randrange(0, 6)):
            number = r.choice([str(r.randrange(0, 30)), "-%d" % r.randrange(1, 9), "+%d" % r.randrange(0, 9), "007", "12-3", "1.5",
                               str(r.randrange(0, 10 ** 9))])
            gap = r.choice(["\t", " ", "  \t", "\t\t"])
            lines.append(number + gap + r.choice(NAMES + PUNCT_NAMES))
            if r.random() < noise:
                lines.append(r.choice(["", "   ", "# 14 has be put back", ";", "@@@", "-5", "3", "7\t", "x" * r.randrange(1, 9),
                                       "".join(chr(r.randrange(1, 256)) for _ in range(r.randrange(1, 12)))]))
        if r.random() < 0.3:
            lines.append("")
    text = end.join(lines)
    if r.random() < 0.7:
        text += end
    return text.encode("latin-1")


def random_bytes(r):
    """Anything: every character class, in runs."""
    pieces = []
    for _ in range(r.randrange(1, 40)):
        pieces.append(bytes([r.randrange(0, 256)]) * r.randrange(1, 4))
        if r.random() < 0.2:
            pieces.append(b"\n")
    return b"".join(pieces)


def strings_of(r):
    return dict(theme=r.choice(["fantasy", "Jungle", "space", "x", "Hallow"]),
                path=r.choice(["data:levels:fantasy", "Data::levels:jungle", "d"]),
                found=r.choice([None, "data:levels:fantasy:Terrain:fantasy.tct", "a.tct"]),
                search=r.choice([None, "Disc:Theme Park Data:", "Disc:Theme Park Data", "x:", "y"]))


def random_header(r, names, minimum=0):
    """A header spec whose textures are named from `names` (case changed now and then)."""
    n = r.randrange(minimum, 9)
    textures = []
    for _ in range(n):
        name = r.choice(names) if names and r.random() < 0.65 else r.choice(["unknown%d.tga" % r.randrange(100), "extra", "z"])
        if r.random() < 0.3:
            name = name.swapcase()
        textures.append(dict(name=name, flags=r.getrandbits(32), frames=r.randrange(0, 4), f08=(r.getrandbits(8), r.getrandbits(8)),
                             f00=r.getrandbits(16), f02=r.getrandbits(16), object=r.randrange(2)))
    nodes = []
    for _ in range(r.randrange(0, 4)):
        nodes.append([r.choice([None] + list(range(n))) if n else None for _ in range(r.randrange(0, 4))])
    field = None
    if r.random() < 0.7:
        quads = [(r.getrandbits(16), r.randrange(n)) for _ in range(r.randrange(0, 6))] if n else []
        field = dict(quads=quads, grid=(r.randrange(0, 5), r.randrange(0, 5)), heights=[])
    return dict(textures=textures, nodes=nodes, hfield=field, pattern=pattern(r), folder=r.choice(["Orig:Folder", "", "o"]))


def names_in(text):
    """File names a text is likely to define (the words after a tab), for header textures."""
    return [w for line in text.decode("latin-1").splitlines() for w in line.split("\t")[1:] if w]


def case(ops, text=b"", header=None, folder="Data:Levels:Fantasy:Terrain:", strings=None, categories=2, bytes_=0, open_=(1,),
         size=(), mapping=None, kept=0, total=0, new_textures=1, pointers=None, tail=b""):
    c = dict(ops=list(ops), text=text, header=header, folder=folder, strings=strings or dict(
        theme="fantasy", path="data:levels:fantasy", found="data:levels:fantasy:Terrain:fantasy.tct", search="Disc:Theme Park Data:"),
        categories=categories, bytes=bytes_, open=list(open_), size=list(size), map=mapping, kept=kept, total=total,
        new_textures=new_textures, tail=tail)
    if pointers is not None:
        c["pointers"] = pointers
    return c


FANTASY = (b"#\r\n#\tTheme Park 2 Jungle Theme\r\n#\tTexture Correspondance Table\r\n#\r\n\r\nPathTex\r\n0\tjpa_squ1.tga\r\n"
           b"1\tjpa_end1.tga\r\n2\tjpa_str1.tga\r\n21\tdark.tga\r\n\r\nQueueTex\r\n0\tjpa_que4.tga\r\n1\tjpa_que3.tga\r\n"
           b"\r\n#Water\r\n#0\tcentre.tga\r\n")


def plain_header(r, text):
    return random_header(r, names_in(text), minimum=1)


# --- ParseLevelTCT

def parse_edges():
    yield case(["parse"], FANTASY)
    yield case(["parse"], b"")
    yield case(["parse"], b"#")
    yield case(["parse"], b"# comment\n; another\n")
    yield case(["parse"], b"PathTex")
    yield case(["parse"], b"PathTex\n1")
    yield case(["parse"], b"PathTex\n1 a")
    yield case(["parse"], b"PathTex\n1\ta.tga\n2\tb.tga")
    yield case(["parse"], b"PathTex\n1\ta.tga\n2\tb.tga\n")
    yield case(["parse"], b"PathTex\r\n-1\tNeg.tga\r\n+2\tPlus.tga\r\n", bytes_=0)
    yield case(["parse"], b"water\n1\ta.tga\nPathTex\n2\tb.tga\n")
    yield case(["parse"], FANTASY, bytes_=500)
    yield case(["parse"], FANTASY, open_=(0,))
    yield case(["parse"], FANTASY, open_=(1, 0))
    yield case(["parse"], FANTASY, size=(0,))
    yield case(["parse"], FANTASY, size=(20,))
    yield case(["parse"], FANTASY, size=(len(FANTASY) + 40,))
    yield case(["parse"], FANTASY, categories=0)
    yield case(["parse"], FANTASY, categories=1)
    yield case(["parse", "parse"], FANTASY)
    yield case(["parse"], bytes(range(256)))
    yield case(["parse"], b"PathTex\n" + bytes(range(256)) + b"\n1\tx.tga\n")
    yield case(["parse"], b"PathTex\n1\t" + b"n" * 300 + b"\n2\tb.tga\n")
    yield case(["parse"], b"PathTex\n123456789012\tb.tga\n")
    yield case(["parse"], b"PathTex\n1\t#.tga ;x\n")
    yield case(["parse"], b"\n\n\n   \t\t\r\r  PathTex  \n  5   a.tga   \n")
    yield case(["parse"], b"PathTex\n!1\tx\n@\n[\n{\n~\n`\n")
    yield case(["parse"], b"PathTex\n\x80\x81\xff 1\ta\n\x7f\x00 2\tb\n")
    yield case(["parse"], b"PathTex2\n1\ta\nPathTex\n2\tb\n")
    for text in (b"7", b"7\t", b"PathTex\n7", b"PathTex\n7\t", b"PathTex\n7\ta", b"x", b"x y"):
        yield case(["parse"], text)
    for tail in (b"", b"\n", b"\r", b"x", b"#", b"1"):
        yield case(["parse"], FANTASY.rstrip(b"\r\n") + tail)
    # The text reader looks one byte past the end: whatever lies there counts.
    for text in (b"PathTex", b"PathTex\n1", b"PathTex\n1\ta", b"PathTex\n1\tab", b"# c", b"@", b"PathTex\n1\ta\n#", b"PathTex\n1\ta\n@",
                 b"PathTex\n1\t", b"PathTex\n1 ", b"x", b"1", b"-"):
        for tail in (b"\n", b"x", b"1", b"#", b" ", b"@", b"\n\n", b"\xff", b"-", b"\0"):
            yield case(["parse"], text, tail=tail)
    for text in (b"PathTex\n1\ta\n2\t_b", b"PathTex\n1\t{", b"PathTex\n1\t~x\n", b"PathTex\n1\t[x\n", b"PathTex\n1\t\x80_x\n"):
        yield case(["parse"], text)


def parse_random(r):
    kind = r.random()
    if kind < 0.7:
        text = tct_text(r)
    elif kind < 0.85:
        text = random_bytes(r)
    else:
        text = tct_text(r, noise=0.5)
    c = case(["parse"], text, categories=r.choice([2, 2, 2, 1, 0]), bytes_=r.choice([0, 0, 0, 77]),
             open_=r.choice([(1,), (1,), (1,), (1, 0), (0,)]),
             tail=r.choice([b"", b"", b"", b"\n", b"x", b"1", b"#", b" ", b"\n\n", b"@", b"\xff"]))
    if r.random() < 0.1:
        c["size"] = [r.randrange(0, len(text) + 30)]
    return c


# --- InterceptAndInsertM3D2Textures

def intercept_case(r, text, header=None):
    folder = r.choice(["Data:Levels:Fantasy:Terrain:", "", "a:", "Disc:Theme Park Data:data:levels:fantasy:Terrain:"])
    return case(["parse", "intercept"], text, header=header or plain_header(r, text), folder=folder,
                categories=r.choice([2, 2, 2, 1, 0]), open_=r.choice([(1,), (1,), (1,), (1, 0)]))


def header_with(r, order):
    """A header whose textures have exactly these names, in this order, with face sets and
    quads that use them."""
    n = len(order)
    spec = random_header(r, [], minimum=1)
    spec["textures"] = [dict(name=name, flags=r.getrandbits(32), frames=r.randrange(0, 4), f08=(r.getrandbits(8), r.getrandbits(8)),
                             f00=r.getrandbits(16), f02=r.getrandbits(16), object=r.randrange(2)) for name in order]
    spec["nodes"] = [[r.choice([None] + list(range(n))) for _ in range(3)], [k % n for k in range(4)]]
    spec["hfield"] = dict(quads=[(r.getrandbits(16), r.randrange(n)) for _ in range(5)], grid=(1, 1), heights=[])
    return spec


def intercept_edges():
    r = random.Random(12)
    both = FANTASY
    pool = ["jpa_squ1.tga", "jpa_end1.tga", "dark.tga", "jpa_que4.tga", "jpa_que3.tga", "nothere.tga", "JPA_SQU1.TGA"]
    # the last texture found in the first category, found in the second, not found
    for order in (["jpa_squ1.tga"], ["jpa_que4.tga"], ["nothere.tga"], ["nothere.tga", "jpa_squ1.tga"],
                  ["jpa_squ1.tga", "nothere.tga"], ["jpa_que4.tga", "jpa_squ1.tga", "x", "JPA_END1.tga"],
                  ["jpa_squ1.tga", "jpa_que4.tga"], ["x", "y", "z"], pool):
        yield case(["parse", "intercept"], both, header=header_with(r, order))
    yield intercept_case(r, both)
    yield case(["parse", "intercept"], FANTASY, header=header_with(r, pool), categories=1)
    yield case(["parse", "intercept"], FANTASY, header=header_with(r, pool), categories=0)
    yield case(["parse", "intercept"], b"", header=header_with(r, pool))
    yield case(["parse", "intercept"], FANTASY, header=header_with(r, pool), open_=(1, 0))
    yield case(["parse", "intercept"], FANTASY, header=header_with(r, pool), open_=(0,))
    yield case(["intercept"], FANTASY, header=header_with(r, pool))
    yield case(["parse", "intercept"], b"PathTex\n1\t" + b"n" * 40 + ".tga\n".encode() + b"QueueTex\n2\tnineteen_chars__.tga\n",
               header=header_with(r, ["x", "nineteen_chars__.tga"]))
    yield case(["parse", "intercept"], b"PathTex\n1\ta.tga\n2\tb.tga\nQueueTex\n3\tc.tga\n4\td.tga\n",
               header=header_with(r, ["c.tga", "b.tga", "A.tga", "d.tga", "e.tga"]))
    yield case(["parse", "intercept"], b"QueueTex\n3\tc.tga\nPathTex\n1\ta.tga\n",
               header=header_with(r, ["a.tga", "c.tga"]))


def intercept_random(r):
    text = tct_text(r, noise=0.05) if r.random() < 0.9 else FANTASY
    return intercept_case(r, text)


# --- ReplaceTextures

def replace_case(r):
    """A header whose textures are numbered 0..n-1, a map and a block of new textures big
    enough for every place the map can give."""
    n = r.randrange(1, 8)
    kept = r.randrange(0, 5)
    mapping = [r.choice([r.randrange(1, 9), -r.randrange(0, 6)]) for _ in range(n)]
    usable = [i for i, m in enumerate(mapping) if m != 0]
    spec = random_header(r, [], minimum=n)
    spec["textures"] = spec["textures"][:n]
    for node in spec["nodes"]:
        for k in range(len(node)):
            node[k] = r.choice(usable) if node[k] is not None and usable else None
    if spec["hfield"] is not None:
        spec["hfield"]["quads"] = [(r.getrandbits(16), r.randrange(n)) for _ in range(r.randrange(0, 6))]
    places = [m + kept - 1 if m > 0 else -m - 1 for m in mapping]
    needed = 1 + max([places[i] for i in usable] + [0])
    return case(["replace"], header=spec, mapping=mapping, kept=kept, total=r.getrandbits(32), new_textures=needed)


def replace_edges():
    r = random.Random(13)
    spec = random_header(r, [], minimum=3)
    spec["textures"] = spec["textures"][:3]
    spec["nodes"] = [[0, 1, 2, None], [], [2, 2]]
    spec["hfield"] = dict(quads=[(0xFFFF, 0), (1, 1), (2, 2), (3, 0)], grid=(1, 1), heights=[])
    yield case(["replace"], header=spec, mapping=[1, 2, 3], kept=0, new_textures=4)
    yield case(["replace"], header=spec, mapping=[1, 2, 3], kept=2, new_textures=6)
    yield case(["replace"], header=spec, mapping=[-1, -2, -3], kept=3, new_textures=4)
    yield case(["replace"], header=spec, mapping=[-3, 2, -1], kept=1, new_textures=4)
    yield case(["replace"], header=spec, mapping=[5, 1, -1], kept=2, total=7, new_textures=8)
    nofield = dict(spec, hfield=None)
    yield case(["replace"], header=nofield, mapping=[1, 2, -1], kept=2, new_textures=4)
    nonodes = dict(spec, nodes=[], hfield=dict(quads=[(1, 2), (2, 0)], grid=(1, 1), heights=[]))
    yield case(["replace"], header=nonodes, mapping=[1, -1, 2], kept=1, new_textures=4)
    empty = dict(spec, nodes=[[]], hfield=dict(quads=[], grid=(1, 1), heights=[]))
    yield case(["replace"], header=empty, mapping=[1, -1, 2], kept=1, new_textures=4)


# --- BuildHFieldTextureTable and FreeHFieldTextureTable

def build_case(r):
    text = tct_text(r, noise=0.05) if r.random() < 0.85 else FANTASY
    strings = strings_of(r)
    header = random_header(r, names_in(text), minimum=0 if r.random() < 0.2 else 1)
    ops = ["build"] + (["free"] if r.random() < 0.5 else [])
    return case(ops, text, header=header, strings=strings, categories=r.choice([2, 2, 2, 1, 0]),
                open_=r.choice([(1,), (1,), (1, 0)]))


def build_edges():
    r = random.Random(14)
    pool = ["jpa_squ1.tga", "dark.tga", "jpa_que4.tga", "zz.tga"]
    base = dict(theme="fantasy", path="data:levels:fantasy", found="data:levels:fantasy:Terrain:fantasy.tct",
                search="Disc:Theme Park Data:")
    for found in (None, base["found"]):
        for search in (None, "Disc:Theme Park Data:", "Disc:Theme Park Data", "C"):
            strings = dict(base, found=found, search=search)
            yield case(["build"], FANTASY, header=header_with(r, pool), strings=strings)
            yield case(["build", "free"], FANTASY, header=header_with(r, pool), strings=strings)
    yield case(["build"], FANTASY, header=dict(random_header(r, []), textures=[], nodes=[], hfield=None))
    yield case(["build", "free", "build", "free"], FANTASY, header=header_with(r, pool))
    yield case(["build"], FANTASY, header=header_with(r, pool), strings=dict(base, theme="x", path="p"))


def free_cases():
    yield case(["free"])
    yield case(["free"], pointers=dict(names=L_STRING + 20, infos=L_STRING + 21, block=L_STRING + 22))
    yield case(["free"], pointers=dict(infos=L_STRING + 21))
    yield case(["free"], pointers=dict(block=L_STRING + 22))
    yield case(["free"], pointers=dict(names=L_STRING + 20))


def free_random(r):
    keys = [k for k in ("names", "infos", "block") if r.random() < 0.6]
    labels = dict(names=L_STRING + 20, infos=L_STRING + 21, block=L_STRING + 22)
    return case(["free"], pointers={k: labels[k] for k in keys})


# --- DetermineHFieldFlatness

def f32(x):
    return T.f32(x)


def flatness_case(r, grid=None, heights=None, quads=None):
    width, height = grid or (r.randrange(0, 6), r.randrange(0, 6))
    count = (width + 1) * (height + 1)
    if heights is None:
        kind = r.random()
        if kind < 0.3:
            heights = [f32(r.uniform(-50, 50)) for _ in range(count)]
        elif kind < 0.6:
            a, b, c = r.uniform(-3, 3), r.uniform(-3, 3), r.uniform(-20, 20)
            heights = [f32(a * x + b * y + c) for y in range(height + 1) for x in range(width + 1)]
        elif kind < 0.8:
            heights = [f32(r.choice([0.0, 0.0, 0.0, 1.0, -1.0, 10.0, r.uniform(-1, 1)])) for _ in range(count)]
        elif kind < 0.9:
            heights = [r.choice([0.0, -0.0, 1.0, 1e20, -1e20, 3.4e38, 1e-30, math.inf, -math.inf, math.nan]) if r.random() < 0.3
                       else f32(r.uniform(-5, 5)) for _ in range(count)]
        else:
            heights = [f32(r.gauss(0, 0.4)) for _ in range(count)]
    quads = quads or [(r.getrandbits(16), r.getrandbits(16)) for _ in range(max(width * height, 0))]
    spec = dict(textures=[], nodes=[], hfield=dict(quads=quads, grid=(width, height), heights=heights), pattern=pattern(r),
                folder="f")
    return case(["flatness"], header=spec)


def model_dot(heights):
    """The dot product of a cell's two normals in plain double arithmetic (to find where it
    crosses the threshold)."""
    h00, h10, h01, h11 = heights

    def normal(ten, dy1, dy2):
        a = (ten, dy1, 0.0)
        b = (0.0, dy2, ten)
        la = math.sqrt(a[0] ** 2 + a[1] ** 2)
        lb = math.sqrt(b[1] ** 2 + b[2] ** 2)
        a = tuple(v / la for v in a)
        b = tuple(v / lb for v in b)
        return (b[1] * a[2] - b[2] * a[1], b[2] * a[0] - b[0] * a[2], b[0] * a[1] - b[1] * a[0])

    n1 = normal(10.0, h10 - h00, h01 - h00)
    n2 = normal(-10.0, h01 - h11, h10 - h11)
    return sum(x * y for x, y in zip(n1, n2))


def crossing(h00, b):
    """The step a (the cell h00, h00 + a, h00 + b, h00 + a + b: a plane) at which the dot
    product of the normals falls below the threshold."""
    low, high = 0.0, 100.0
    for _ in range(100):
        mid = (low + high) / 2
        if model_dot((h00, h00 + mid, h00 + b, h00 + mid + b)) > 0.9999:
            low = mid
        else:
            high = mid
    return low


# Cells (h00, h10, h01, h11) whose dot product lies so close to the threshold that fusing
# the last two multiply-adds, or squaring the steps in double, decides differently. Found
# with a model of the arithmetic (libm's fmaf and fma; single-precision roundings from
# struct) by walking h10 through the neighbouring single-precision values where the
# threshold is crossed, for several corners and slopes, and keeping the cells where a
# variant (x term unfused, z term unfused, squares in double) answered differently.
BOUNDARY_CELLS = [
    (-7.25, -3.008052110671997, -7.0, -2.7480521202087402),
    (-7.25, -5.2410054206848145, -6.75, -4.7310051918029785),
    (-7.25, -6.254844665527344, -6.25, -5.244844436645508),
    (-7.25, -6.585014343261719, -5.75, -5.075014114379883),
    (-7.25, -6.735499858856201, -5.25, -4.745500087738037),
    (0.0, 4.242011547088623, 0.25, 4.502011775970459),
    (0.0, 2.0086863040924072, 0.5, 2.518686294555664),
    (-7.25, -5.240910053253174, -6.75, -4.730909824371338),
    (-7.25, -6.234756946563721, -6.25, -5.244757175445557),
    (-7.25, -6.747694492340088, -5.25, -4.737694263458252),
    (-7.25, -6.908676624298096, -4.25, -3.898676633834839),
    (-7.25, -7.026268482208252, -2.25, -2.026268482208252),
    (0.0, 0.5146550536155701, 2.0, 2.504655122756958),
    (1.5, 2.0146446228027344, 3.5, 4.004644393920898),
    (-7.25, -3.0081489086151123, -7.0, -2.7481489181518555),
    (-7.25, -5.241123199462891, -6.75, -4.731122970581055),
    (-7.25, -6.254532814025879, -6.25, -5.244532585144043),
    (-7.25, -6.735276699066162, -5.25, -4.745276927947998),
    (-7.25, -6.908787250518799, -4.25, -3.898787260055542),
    (0.0, 4.241850852966309, 0.25, 4.5018510818481445),
    (0.0, 2.0445125102996826, 0.5, 2.5445125102996826),
]


def flatness_edges():
    r = random.Random(16)
    yield flatness_case(r, grid=(0, 0), heights=[1.0], quads=[])
    yield flatness_case(r, grid=(0, 3), heights=[0.0] * 4, quads=[])
    yield flatness_case(r, grid=(3, 0), heights=[0.0] * 4, quads=[])
    yield flatness_case(r, grid=(1, 1), heights=[0.0, 0.0, 0.0, 0.0], quads=[(0, 0)])
    yield flatness_case(r, grid=(1, 1), heights=[0.0, 1.0, 1.0, 5.0], quads=[(0x0800, 7)])
    yield flatness_case(r, grid=(2, 2), heights=[0.0] * 9, quads=[(0xFFFF, 1)] * 4)
    yield flatness_case(r, grid=(2, 1), heights=[0.0, 0.0, 0.0, 0.0, 0.0, 9.0], quads=[(1, 2), (3, 4)])
    for nan in (math.nan, math.inf, -math.inf):
        for corner in range(4):
            heights = [0.0] * 4
            heights[corner] = nan
            yield flatness_case(r, grid=(1, 1), heights=heights, quads=[(0, 0)])
    # around the threshold: the step along one axis just either side of where the dot is 0.9999
    for corner, b in ((0.0, 0.5), (0.0, 1.0), (5.0, 2.0), (-3.0, 4.0)):
        step = crossing(corner, b)
        for scale in (0.5, 0.9, 0.99, 0.999, 0.9999, 0.99999, 1.0, 1.00001, 1.0001, 1.001, 1.01, 1.1, 2.0):
            heights = [f32(corner), f32(corner + step * scale), f32(corner + b), f32(corner + step * scale + b)]
            yield flatness_case(r, grid=(1, 1), heights=heights, quads=[(0x0001, 0)])
    for cell in BOUNDARY_CELLS:
        yield flatness_case(r, grid=(1, 1), heights=list(cell), quads=[(0x0001, 0)])
    # a whole grid of such cells
    heights = [BOUNDARY_CELLS[k % len(BOUNDARY_CELLS)][k // len(BOUNDARY_CELLS) % 4] for k in range(16)]
    yield flatness_case(r, grid=(3, 3), heights=heights, quads=[(k, k) for k in range(9)])


def flatness_random(r):
    return flatness_case(r)


TESTS = [
    FnTest("ParseLevelTCT", ppc, host, edge=parse_edges, rnd=parse_random),
    FnTest("InterceptAndInsertM3D2Textures", ppc, host, edge=intercept_edges, rnd=intercept_random),
    FnTest("ReplaceTextures", ppc, host, edge=replace_edges, rnd=replace_case),
    FnTest("BuildHFieldTextureTable", ppc, host, edge=build_edges, rnd=build_case),
    FnTest("FreeHFieldTextureTable", ppc, host, edge=free_cases, rnd=free_random),
    FnTest("DetermineHFieldFlatness", ppc, host, edge=flatness_edges, rnd=flatness_random),
]
