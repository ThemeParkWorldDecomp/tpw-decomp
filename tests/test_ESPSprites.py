"""Unit w33: the ESP sprite loader (HLSprite.cpp of the main app): ESP_CSprite's constructor, GetActionFrames, GetCustomData and
LoadFromESPFile, ESP_SortFileList, ESP_FindESPFiles, ESP_LoadSprites, ESP_GetParticleSprite and ESP_InitialiseAndLoadSprites
(src/game/HLSprite.cpp), against libtpw_unit_ESPSprites.dylib (tests/unit_ESPSprites.cpp).

A case is a world and a script of operations. The world (listings of directory entries by search pattern, the files of three
storages, the theme name, the options' answers, queued answers for the callees, the globals, the height field) is built on both
sides from the same Python values; the operations run the original on the PowerPC side and the translation on the host side,
and after each one the observable state is written as numbers (the globals, the file list with its directory entries, the sprite
table with every field of every sprite and of its sprite file, the sprite lists, the counts) and compared in order, with the
call log of every callee outside the unit (the directory search, the resource manager, the storages and files, CSpriteFileIO,
CSpriteList::Init, CMapWho::Link, the options, the theme name, TPStrEq and TPStrGtr, FS_AddDirectorySeparator, CTRACE, and the
allocations of the game's objects). The bullfrog strings are functional and unrecorded on both sides.

The PowerPC side writes the Mac's big-endian globals at their real addresses and puts them back after the case (other modules read
them). The .ESP files of the cases are the 46 shipped files (read from ROOT/extracted at run time, never copied) and files the test
builds: every action pattern, with and without the custom blocks, cut at every length that matters, with a wrong signature.
"""
import ctypes as C
import pathlib
import random
import struct

import calllog as CL
import tpwtest as T

SHIM_LIBRARY = "unit_ESPSprites"

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
ESP_DIR = ROOT / "extracted" / "wads" / "esprites.wad.d"

MASK = 0xFFFFFFFF

# ---- the originals under test and the callees the PowerPC side fakes (addresses in the main app)
ENTRY = {"init": 0x1007ACE0, "ctor": 0x1007B150, "frames": 0x1007B1F8, "custom": 0x1007B23C, "load": 0x1007B354,
         "sort": 0x1007BA5C, "find": 0x1007BB80, "loadsprites": 0x1007C63C, "particle": 0x1007CEFC}
FAKED = {"TPStrEq": 0x100053F4, "TPStrGtr": 0x10005354, "FS_AddDirectorySeparator": 0x10004BC0, "CTRACE": 0x1006FCBC,
         "GetCurrentThemeName": 0x1014A058, "GetNumKidSprites": 0x1015F20C, "GetNumStaffSprites": 0x1015F30C,
         "FindFirst": 0x10160830, "FindNext": 0x10160A2C, "pGetCurrentSearchStorage": 0x1015FDF4, "FindResource": 0x10165448}

# ---- the globals (the scalars in the order of the unit's table)
SCALARS = [("draw_enabled", 0x1029D364, 1), ("third_person", 0x1029D360, 4), ("first_person", 0x102DBD3C, 4),
           ("f20", 0x102DBD20, 4), ("f34", 0x102DBD34, 4), ("f88", 0x102DC088, 4), ("map_width", 0x102DBD30, 4),
           ("map_height", 0x102DBD2C, 4), ("cells_wide", 0x102DBD28, 4), ("cells_high", 0x102DBD24, 4),
           ("semitransparent", 0x102DB1E0, 1), ("sprite_count", 0x102DB1E4, 4), ("total_sprites", 0x102DB1E8, 4),
           ("file_count", 0x102DABDC, 4), ("compressed", 0x1029D310, 1), ("hot_x", None, 4), ("hot_y", None, 4)]
SCALAR_NAMES = [s[0] for s in SCALARS]
DEFAULT_SCALARS = {"draw_enabled": 1, "third_person": 1, "compressed": 1}
FILES = 0x102DABE0            # 0x80 entries of 12 bytes
PATH = 0x102DB1EC             # 0x200 bytes
FILEINFO = 0x102DB3EC         # 0x128 bytes
TABLE = 0x102DB520            # 0x200 pointers
GROUPS = 0x1029D368           # 14 x (name, first, count)
KID_FILES = 0x1029D410        # four pointers to the preferred kid files' names
INSTANCES = 0x102DBDBC        # 16 words
GLOBAL_INSTANCES = 0x102DBD44
LISTS = 0x102DBDFC            # 16 lists of 0x28 bytes
GLOBAL_LIST = 0x102DBD48
TPGLOBALS = 0x103B74C8
GAME = 0x1037F134
HEIGHTFIELD_PTR = 0x102C6C7C
STRINGS = {0x42E0: ".tps", 0x42F4: ".tpc", 0x4308: ".fps", 0x431C: ".fpc", 0x4588: "*.ESP", 0x4664: "Sprites"}

# labels shared with the unit
L_GAME, L_MAPWHO, L_MANAGER, L_RESOURCE, L_STORAGE, L_INFO, L_SLOT, L_LINKBASE = 1, 2, 3, 4, 5, 10, 20, 30

# ---- operations
OP_INIT, OP_LOADSPRITES, OP_FIND, OP_SORT, OP_CTOR, OP_LOAD, OP_FRAMES, OP_CUSTOM, OP_PARTICLE = range(9)


# A 32-bit word as a signed number.
def s32(v):
    return T.s32(v)


# ================================================================================================ files
def esp_bytes(actions, custom=None, first_person=1, f206=0, magic=b"ESP_FILE", version=b"2.00", datafile=b"SPR_XX.TPS", cut=None):
    """An .ESP file: signature, version, the data file's name (0x100 bytes), two bytes, 16 actions of (u16 first, u8 frames,
    u8 views) and, when `custom` is given, 16 custom bytes."""
    data = magic[:8].ljust(8, b"\0") + version[:4].ljust(4, b"\0") + datafile[:0x100].ljust(0x100, b"\0")
    data += bytes([first_person & 0xFF, f206 & 0xFF])
    for first, frames, views in actions:
        data += struct.pack("<HBB", first & 0xFFFF, frames & 0xFF, views & 0xFF)
    if custom is not None:
        data += bytes(c & 0xFF for c in custom)
    return data if cut is None else data[:cut]


# Sixteen actions of one of five kinds: random, small, all empty, all at the limits, a regular pattern.
def random_actions(r, kind=None):
    kind = kind if kind is not None else r.randrange(5)
    acts = []
    for i in range(16):
        if kind == 0:
            acts.append((r.randrange(0x10000), r.randrange(256), r.randrange(256)))
        elif kind == 1:
            acts.append((r.randrange(40), r.choice([0, 1, 2, 4, 8]), r.choice([0, 0, 5])))
        elif kind == 2:
            acts.append((0, 0, 0))
        elif kind == 3:
            acts.append((0xFFFF, 255, 255))
        else:
            acts.append((i * 8, r.choice([0, 8]), r.choice([0, 1])))
    return acts


# Sixteen custom bytes: none, random, small values, or a fixed pattern with some zeros.
def random_custom(r):
    k = r.randrange(4)
    if k == 0:
        return [0] * 16
    if k == 1:
        return [r.randrange(256) for _ in range(16)]
    if k == 2:
        return [r.choice([0, 1, 2, 255]) for _ in range(16)]
    return [1, 2, 3, 4, 0, 6, 7, 8, 9, 10, 11, 12, 0, 14, 15, 16]


_real_files = []


def real_files():
    """The shipped sprite sets as (file name, the .ESP file's bytes, the first bytes of its .TPC file), read once."""
    if not _real_files and ESP_DIR.exists():
        for esp in sorted(ESP_DIR.rglob("*.ESP")):
            tpc = esp.with_suffix(".TPC")
            head = tpc.read_bytes()[:16] if tpc.exists() else b"\0" * 16
            _real_files.append((esp.name, esp.read_bytes(), head))
    return _real_files


# ================================================================================================ the world
def patterns(theme, folder):
    """The two search patterns ESP_FindESPFiles builds for `folder` (FS_AddDirectorySeparator on the theme first)."""
    def with_sep(text):
        return text if text.endswith(":") else text + ":"
    return "generic:" + folder + ":*.ESP", with_sep(theme) + folder + ":*.ESP"


# An empty world: the theme, no listings or files, no queues, the globals at their start values.
def new_world(theme="Hallow"):
    return {"theme": theme, "listings": {}, "files": [], "queues": {}, "scalars": {}, "hfield": (64, 64), "display_flags": 0,
            "groups": {}, "path": "", "infos": {}}


# A file of the world: in storage `storage`, under `name`, opening with `result` (a negative result is a failure).
def add_file(world, storage, name, data, result=0):
    world["files"].append((storage, name, result, bytes(data)[:360]))


# Eight random words: the first words of a directory entry.
def words(r):
    return [r.getrandbits(32) for _ in range(8)]


# A directory entry of a listing: the name buffer, where mName points in it, the storage it is found in and its words.
def entry(buffer, offset=0, storage=0, w=None):
    return (buffer, offset, storage, w if w is not None else [0x11111111 * (i + 1) & MASK for i in range(8)])


def data_names(buffer):
    """The data file names LoadFromESPFile asks for: the buffer without its extension and one of the four extensions."""
    stem = buffer[:-4] if len(buffer) >= 4 else buffer
    return {ext: stem + ext for ext in (".tps", ".tpc", ".fps", ".fpc")}


# ================================================================================================ the PowerPC side
class State:
    """Per-case state of the PowerPC fakes."""

    def __init__(self, e, world):
        self.world = world
        self.streams = {}                 # file object address -> [bytes, position]
        self.listing = None
        self.cursor = 0
        self.current_storage = 0
        self.storage_objs = []
        self.info_objs = []
        self.slot_addr = [None] * 4
        self.theme_obj = 0
        self.cl = None

    def lab(self, a):
        return self.cl.lab(a)


def vtables(e):
    """The fake file and storage vtables (persistent): CFile Close at 0xc and Read at 0x10; CFileStorage OpenFile at 0x14 and the
    deleting destructor at 0x34. They find the case's state in e.esp."""
    if hasattr(e, "esp_vtables"):
        return e.esp_vtables
    file_vt = T.persist_alloc(e, 0x40)
    storage_vt = T.persist_alloc(e, 0x40)

    def file_close(emu):
        S = emu.esp
        S.cl.rec("CFile.Close", (S.lab(emu.r(3)),))

    def read(emu, this, dest, size):
        S = emu.esp
        S.cl.rec("CFile.Read", (S.lab(this), size))
        stream = S.streams.get(this)
        data, pos = (stream[0], stream[1]) if stream else (b"", 0)
        remaining = max(len(data) - pos, 0)
        count = min(size, remaining)
        chunk = data[pos:pos + count]
        if stream:
            stream[1] = pos + count
        if dest and count > 0:
            emu.write(dest, chunk)
        forced = S.cl.ret("Read", -2)
        return (count if forced == -2 else forced), chunk

    def file_read(emu):
        got, _ = read(emu, emu.r(3), emu.r(4), emu.r(5))
        emu.set_r(3, T.u32(got))

    def open_file(emu):
        S = emu.esp
        ret_buf, this, file, name, mode, data = (emu.r(i) for i in range(3, 9))
        text = emu.cstr(name)
        ident = emu.u32(this + 4)
        S.cl.rec("OpenFile", (S.lab(this), ident, S.lab(file), mode, int(data != 0)), (text[:159],))
        found = None
        for storage, fname, result, bytes_ in S.world["files"]:
            if storage == ident and fname == text:
                found = (result, bytes_)
                break
        result = found[0] if found else -1
        forced = S.cl.ret("OpenFile", -9)
        if forced != -9:
            result = forced
        if result >= 0:
            S.streams[file] = [found[1] if found else b"", 0]
        emu.write(ret_buf, struct.pack(">i", result))

    def storage_dtor(emu):
        S = emu.esp
        this, flag = emu.r(3), T.s16(emu.r(4))
        S.cl.rec("CFileStorage.dtor", (S.lab(this),))
        if flag > 0:
            if S.lab(this) > 0:
                S.cl.rec("delete", (S.lab(this),))
            S.cl.forget(this)

    e.write(file_vt + 0xC, struct.pack(">I", T.fake_function(e, file_close)))
    e.write(file_vt + 0x10, struct.pack(">I", T.fake_function(e, file_read)))
    e.write(storage_vt + 0x14, struct.pack(">I", T.fake_function(e, open_file)))
    e.write(storage_vt + 0x34, struct.pack(">I", T.fake_function(e, storage_dtor)))
    e.esp_vtables = (file_vt, storage_vt, read)
    return e.esp_vtables


# A bullfrog string in the emulator's memory.
def tds_new(e, text):
    obj = e.alloc(0x14)
    CL.tds_set(e, obj, text)
    return obj


def build_stubs(e, S):
    """The PowerPC fakes of a case: the bullfrog strings (functional), the C library, the allocator and the callees."""
    file_vt, storage_vt, read = vtables(e)
    st = CL.Stubs()
    st.real = set(ENTRY.values())
    rec = lambda name, ints=(), texts=(): S.cl.rec(name, tuple(ints), tuple(texts))
    text = lambda a: CL.tds_text(e, a)

    # -- the strings
    for name, fn in CL.string_stubs().items():
        st.imp(name, fn)
    st.imp("__ct__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>", lambda emu: CL.tds_set(emu, emu.r(3), text(emu.r(4))))
    st.imp("__as__26TbDynamicStringTemplate<c>FPCc", lambda emu: CL.tds_set(emu, emu.r(3), emu.cstr(emu.r(4))))
    st.imp("__as__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>", lambda emu: CL.tds_set(emu, emu.r(3), text(emu.r(4))))
    st.imp("Length__15TbStringBase<c>CFv", lambda emu: emu.set_r(3, len(text(emu.r(3)))))

    def left(emu):
        # TbStringBase::Left(count): the first `count` characters (all of them when count is not smaller), as a 16-byte view
        out, this, count = emu.r(3), emu.r(4), emu.r(5)
        t = text(this)
        part = t if count >= len(t) else t[:count]
        emu.write(out, struct.pack(">IIII", CL.FAKE_VT, len(part), CL.new_text(emu, part), 0))
    st.imp("Left__15TbStringBase<c>CFUi", left)

    def strcat(emu):
        dest = emu.r(3)
        emu.write(dest, (emu.cstr(dest, limit=1 << 20) + emu.cstr(emu.r(4), limit=1 << 20)).encode("latin-1") + b"\0")
    st.imp("strcat", strcat)

    # -- the allocator: the game's objects get a label and 0xa5 in every byte; other sizes are not expected
    def op_new(emu):
        size = emu.r(3)
        a = emu.alloc(max(size, 4))
        if size in (0x260, 0x128, 8, 0x10):
            emu.write(a, b"\xa5" * size)
            S.cl.newlab(a)
            rec("new", (size,))
        emu.set_r(3, a)

    def op_delete(emu):
        p = emu.r(3)
        if p:
            if S.lab(p) > 0:
                rec("delete", (S.lab(p),))
            S.cl.forget(p)
    st.imp("__nw__FUl", op_new)
    st.imp("__dl__FPv", op_delete)

    # -- the file layer
    def file_ctor(emu):
        obj = emu.r(3)
        emu.write(obj, struct.pack(">II", file_vt, 0))
        rec("CFile.ctor", (S.lab(obj),))
        S.streams[obj] = [b"", 0]
    st.imp("__ct__Q33bfl10FileSystem5CFileFv", file_ctor)

    def storage_ctor(emu):
        obj = emu.r(3)
        emu.write(obj, struct.pack(">II", storage_vt, 0))
        rec("CFileStorage.ctor", (S.lab(obj),))
    st.imp("__ct__Q33bfl10FileSystem12CFileStorageFv", storage_ctor)

    def storage_assign(emu):
        dst, src = emu.r(3), emu.r(4)
        ident = emu.u32(src + 4)
        rec("CFileStorage.assign", (S.lab(dst), S.lab(src), ident))
        emu.write(dst + 4, struct.pack(">I", ident))
    st.imp("__as__Q33bfl10FileSystem12CFileStorageFRCQ33bfl10FileSystem12CFileStorage", storage_assign)

    # -- the engine
    def load(emu):
        this, file, flags = emu.r(3), emu.r(4), T.s32(emu.r(5))
        rec("CSpriteFileIO.Load", (S.lab(this), S.lab(file), flags))
        got, chunk = read(emu, file, 0, 4)
        head = list(chunk) + [0xEE] * (4 - len(chunk))
        rec("CSpriteFileIO.Head", (T.s32(got), *head))
        count = S.cl.ret("LoadCount", 3)
        records = S.cl.ret("LoadRecords", 0x40000000)
        emu.write(this, struct.pack(">iI", T.s32(count), records & MASK))
        emu.set_r(3, T.u32(S.cl.ret("Load", 1)))
    st.imp("Load__13CSpriteFileIOFPvi", load)
    st.imp("FlagAsSemitransparent__13CSpriteFileIOFv", lambda emu: rec("CSpriteFileIO.FlagAsSemitransparent", (S.lab(emu.r(3)),)))

    def list_init(emu):
        this, count = emu.r(3), T.s32(emu.r(4))
        rec("CSpriteList.Init", (S.lab(this), count))
        emu.set_r(3, T.u32(S.cl.ret("Init", 0x70000000 + S.lab(this) * 0x100)))
    st.imp("Init__11CSpriteListFi", list_init)

    def link_range(emu):
        this, a, b, c, d, link, lst = emu.r(3), *(T.s32(emu.r(i)) for i in (4, 5, 6, 7)), emu.r(8), T.s32(emu.r(9))
        rec("CMapWho.Link", (S.lab(this), a, b, c, d, S.lab(link), lst))
        emu.set_r(3, 1)
    st.imp("Link__7CMapWhoFiiiiP12CMapLinkBasei", link_range)

    def link_list(emu):
        rec("CMapWho.LinkList", (S.lab(emu.r(3)), S.lab(emu.r(4)), T.s32(emu.r(5))))
        emu.set_r(3, 1)
    st.imp("Link__7CMapWhoFP12CMapLinkBasei", link_list)

    # -- the main app's callees (in the binary)
    def compare(a, b):
        x, y = a.lower(), b.lower()
        return (x > y) - (x < y)

    def tp_eq(emu):
        a, b = text(emu.r(3)), text(emu.r(4))
        rec("TPStrEq", (), (a[:159], b[:159]))
        emu.set_r(3, int(compare(a, b) == 0))

    def tp_gtr(emu):
        a, b = text(emu.r(3)), text(emu.r(4))
        rec("TPStrGtr", (), (a[:159], b[:159]))
        emu.set_r(3, int(compare(a, b) > 0))

    def add_separator(emu):
        path = emu.r(3)
        t = text(path)
        rec("FS_AddDirectorySeparator", (), (t[:159],))
        if not t.endswith(":"):
            CL.tds_set(emu, path, t + ":")

    def ctrace(emu):
        rec("CTRACE", (emu.r(3),), (emu.cstr(emu.r(4)), emu.cstr(emu.r(5))))

    def theme(emu):
        rec("GetCurrentThemeName", (S.lab(emu.r(3)),))
        emu.set_r(3, S.theme_obj)

    def kids(emu):
        rec("GetNumKidSprites")
        emu.set_r(3, T.u32(S.cl.ret("GetNumKidSprites", 3)))

    def staff(emu):
        rec("GetNumStaffSprites")
        emu.set_r(3, T.u32(S.cl.ret("GetNumStaffSprites", 2)))

    def give(emu, info):
        buffer, offset, storage, w = S.listing[S.cursor]
        emu.write(info, struct.pack(">8I", *w))
        emu.write(info + 0x24, buffer.encode("latin-1") + b"\0")
        emu.write(info + 0x20, struct.pack(">I", info + 0x24 + offset))
        S.current_storage = storage

    def find_first(emu):
        this, pattern, info = emu.r(3), emu.r(4), emu.r(5)
        t = text(pattern)
        rec("FindFirst", (S.lab(this),), (t[:159],))
        S.listing = S.world["listings"].get(t)
        S.cursor = 0
        found = 0
        if S.listing:
            give(emu, info)
            found = 1
        forced = S.cl.ret("FindFirst", -9)
        emu.set_r(3, T.u32(found if forced == -9 else forced))

    def find_next(emu):
        this, info = emu.r(3), emu.r(4)
        rec("FindNext", (S.lab(this),))
        found = 0
        if S.listing and S.cursor + 1 < len(S.listing):
            S.cursor += 1
            give(emu, info)
            found = 1
        forced = S.cl.ret("FindNext", -9)
        emu.set_r(3, T.u32(found if (forced == -9 or found == 0) else forced))

    def current_storage(emu):
        rec("pGetCurrentSearchStorage", (S.lab(emu.r(3)),))
        emu.set_r(3, S.storage_objs[S.current_storage])

    def find_resource(emu):
        this, name, out = emu.r(3), emu.r(4), emu.r(5)
        rec("FindResource", (S.lab(this),), (text(name)[:159],))
        result = S.cl.ret("FindResource", 1)
        if result != 0:
            emu.write(out, struct.pack(">I", S.resource))
        emu.set_r(3, T.u32(result))

    for key, fn in (("TPStrEq", tp_eq), ("TPStrGtr", tp_gtr), ("FS_AddDirectorySeparator", add_separator), ("CTRACE", ctrace),
                    ("GetCurrentThemeName", theme), ("GetNumKidSprites", kids), ("GetNumStaffSprites", staff),
                    ("FindFirst", find_first), ("FindNext", find_next), ("pGetCurrentSearchStorage", current_storage),
                    ("FindResource", find_resource)):
        st.internal(FAKED[key], fn)
    return st


# -------------------------------------------------------------------------------------------------- memory
def regions():
    """Everything the case writes in the main image, to be put back afterwards."""
    r = [(0x1029D310, 4), (0x1029D360, 8), (GROUPS, 14 * 12), (0x102DABDC, 4), (FILES, 0x600), (0x102DB1E0, 12),
         (PATH, 0x200), (FILEINFO, 0x138), (TABLE, 0x800), (0x102DBD20, 0x50), (INSTANCES, 0x40), (LISTS, 0x280), (0x102DC088, 4),
         (HEIGHTFIELD_PTR, 4), (GAME + 0x34, 4), (KID_FILES, 16)]
    r += [(TPGLOBALS + off, 0x14) for off in STRINGS]
    return r


# Where the display (gei_Display, a data import) lives in the emulator.
def display_addr(e):
    return e.data_import("gei_Display")


# Where the map's pointer (gei_MapWho, a data import) lives in the emulator.
def mapwho_addr(e):
    return e.data_import("gei_MapWho")


# Writes one of the scalar globals (by name) in the emulator's memory.
def write_scalar(e, name, value):
    for n, addr, size in SCALARS:
        if n != name:
            continue
        if addr is None:
            addr = display_addr(e) + (0x93C if n == "hot_x" else 0x940)
        e.write(addr, struct.pack(">I" if size == 4 else ">B", value & (MASK if size == 4 else 0xFF)))
        return
    raise KeyError(name)


# Reads one of the scalar globals (by name) from the emulator's memory.
def read_scalar(e, name):
    for n, addr, size in SCALARS:
        if n != name:
            continue
        if addr is None:
            addr = display_addr(e) + (0x93C if n == "hot_x" else 0x940)
        return e.u32(addr) if size == 4 else e.read(addr, 1)[0]
    raise KeyError(name)


# -------------------------------------------------------------------------------------------------- the world, PowerPC side
def ppc_setup(e, S):
    """Writes the world and the start state of the globals; returns the label table."""
    world = S.world
    w = lambda a, data: e.write(a, data)
    # the scalars and the tables at their start values
    for name, addr, size in SCALARS:
        write_scalar(e, name, world["scalars"].get(name, DEFAULT_SCALARS.get(name, 0)))
    w(FILES, b"\0" * 0x600)
    w(TABLE, b"\0" * 0x800)
    w(PATH, world["path"].encode("latin-1").ljust(0x200, b"\0"))
    w(FILEINFO, b"\0" * 0x128)
    w(FILEINFO + 0x20, struct.pack(">I", FILEINFO + 0x24))
    for i in range(14):
        first, count = world["groups"].get(i, (0, 0))
        w(GROUPS + 12 * i + 4, struct.pack(">ii", first, count))
    w(INSTANCES, b"\0" * 0x40)
    w(GLOBAL_INSTANCES, b"\0" * 4)
    for i in range(16):
        w(LISTS + 0x28 * i, struct.pack(">II", LISTS + 0x28 * i + 0x20, 0))     # the constructor's pointer to the link base
    w(GLOBAL_LIST, struct.pack(">II", GLOBAL_LIST + 0x20, 0))
    # the game's strings
    for off, text in STRINGS.items():
        CL.tds_set(e, TPGLOBALS + off, text)
    # the display (flags at +0) and the map
    e.write(display_addr(e), struct.pack(">I", world["display_flags"]))
    mapwho = e.alloc(0x80)
    e.write(mapwho_addr(e), struct.pack(">I", mapwho))
    header = e.alloc(0xB0)
    field = e.alloc(0x30)
    e.write(header + 0x6C, struct.pack(">I", field))
    width, height = world["hfield"]
    e.write(field + 0x18, struct.pack(">II", width & MASK, height & MASK))
    e.write(HEIGHTFIELD_PTR, struct.pack(">I", header))
    manager = e.alloc(0x40)
    e.write(GAME + 0x34, struct.pack(">I", manager))
    S.resource = e.alloc(0x40)
    S.theme_obj = tds_new(e, world["theme"])
    # three storages (identity at +4), four scratch directory entries
    file_vt, storage_vt, _ = vtables(e)
    S.storage_objs = []
    for i in range(3):
        a = e.alloc(8)
        e.write(a, struct.pack(">II", storage_vt, i))
        S.storage_objs.append(a)
    S.info_objs = []
    for i in range(4):
        a = e.alloc(0x128)
        e.write(a, b"\0" * 0x128)
        e.write(a + 0x20, struct.pack(">I", a + 0x24))
        S.info_objs.append(a)
        if i in world["infos"]:
            set_info(e, a, *world["infos"][i])
    fixed = {GAME: L_GAME, mapwho: L_MAPWHO, manager: L_MANAGER, S.resource: L_RESOURCE}
    for i, a in enumerate(S.storage_objs):
        fixed[a] = L_STORAGE + i
    for i, a in enumerate(S.info_objs):
        fixed[a] = L_INFO + i
    for i in range(16):
        fixed[LISTS + 0x28 * i + 0x20] = L_LINKBASE + i          # the link base and the list are one address on the host
        fixed[LISTS + 0x28 * i] = L_LINKBASE + i
    fixed[GLOBAL_LIST + 0x20] = L_LINKBASE + 16
    fixed[GLOBAL_LIST] = L_LINKBASE + 16
    return fixed


# Fills a directory entry in memory: its eight words, its name buffer and its name pointer.
def set_info(e, a, buffer, offset, w):
    e.write(a, struct.pack(">8I", *w))
    e.write(a + 0x24, buffer.encode("latin-1") + b"\0")
    e.write(a + 0x20, struct.pack(">I", a + 0x24 + offset))


def sprite_image(e, S, values, name):
    """The 0x260-byte image of a sprite (see tpw_esp_install for the order of `values`); returns the address."""
    v = list(values)
    a = e.alloc(0x260)
    e.write(a, b"\xa5" * 0x260)
    k = 0
    loaded, fp, f206, semi, records, customcount = v[0:6]
    e.write(a + 0x204, bytes([loaded & 0xFF, fp & 0xFF, f206 & 0xFF, semi & 0xFF]))
    e.write(a + 0x208, struct.pack(">I", records & MASK))
    e.write(a + 0x20C, struct.pack(">H", customcount & 0xFFFF))
    e.write(a + 0x20E, bytes(x & 0xFF for x in v[6:22]))
    e.write(a + 0x21E, struct.pack(">H", v[22] & 0xFFFF))
    for i in range(16):
        first, frames, views = v[23 + 3 * i: 26 + 3 * i]
        e.write(a + 0x220 + 4 * i, struct.pack(">HBB", first & 0xFFFF, frames & 0xFF, views & 0xFF))
    e.write(a + 4, name.encode("latin-1") + b"\0")
    present = v[71]
    e.write(a, struct.pack(">I", 0))
    io = 0
    if present:
        io = e.alloc(0x10)
        e.write(io, struct.pack(">iIII", T.s32(v[72]), v[73] & MASK, v[74] & MASK, v[75] & MASK))
        e.write(a, struct.pack(">I", io))
    return a, io


# Runs a case on the PowerPC side: builds the world, runs each operation, dumps after it; the globals are put back after.
def run_ppc(e, case):
    world = case["world"]
    saved = [(a, e.read(a, n)) for a, n in regions()]
    saved += [(display_addr(e), e.read(display_addr(e), 4)), (display_addr(e) + 0x93C, e.read(display_addr(e) + 0x93C, 8)),
              (mapwho_addr(e), e.read(mapwho_addr(e), 4))]
    S = State(e, world)
    try:
        fixed = ppc_setup(e, S)
        S.cl = CL.PpcLog(fixed, world["queues"])
        e.cl = S.cl
        e.esp = S
        st = build_stubs(e, S)
        old = st.activate(e)
        try:
            results, dumps = [], []
            for op in case["ops"]:
                results.append(ppc_op(e, S, op))
                dumps.append(ppc_dump(e, S))
        finally:
            CL.Stubs.deactivate(e, old)
        return {"results": results, "dumps": dumps, "log": S.cl.log}
    finally:
        for a, data in saved:
            e.write(a, data)
        e.esp = None


# Calls one of the originals under test.
def call(e, name, args):
    return T.call_ppc(e, ENTRY[name], args)


# One operation on the PowerPC side; the answer as a list of numbers.
def ppc_op(e, S, op):
    kind = op[0]
    cl = S.cl
    if kind == "init":
        cl.rec("op", (OP_INIT, op[1], op[2], 0), ("",))
        r3, _, _ = call(e, "init", [op[1], op[2]])
        return [r3]
    if kind == "loadsprites":
        cl.rec("op", (OP_LOADSPRITES, op[1], 0, 0), ("",))
        r3, _, _ = call(e, "loadsprites", [op[1]])
        return [r3 & 0xFF]
    if kind == "find":
        cl.rec("op", (OP_FIND, 0, 0, 0), (op[1],))
        name = tds_new(e, op[1])
        r3, _, _ = call(e, "find", [S.resource, name])
        return [T.s32(r3)]
    if kind == "sort":
        cl.rec("op", (OP_SORT, 0, 0, 0), ("",))
        call(e, "sort", [])
        return []
    if kind == "ctor":
        cl.rec("op", (OP_CTOR, op[1], 0, 0), ("",))
        a = e.alloc(0x260)
        e.write(a, b"\xa5" * 0x260)
        S.slot_addr[op[1]] = a
        S.cl.labels[a] = L_SLOT + op[1]
        call(e, "ctor", [a])
        return []
    if kind == "load":
        cl.rec("op", (OP_LOAD, op[1], op[2], op[3]), ("",))
        r3, _, _ = call(e, "load", [S.slot_addr[op[1]], S.info_objs[op[2]], S.storage_objs[op[3]]])
        return [r3 & 0xFF]
    if kind == "frames":
        cl.rec("op", (OP_FRAMES, op[1], op[2], 0), ("",))
        r3, _, _ = call(e, "frames", [S.slot_addr[op[1]], op[2]])
        return [r3]
    if kind == "custom":
        cl.rec("op", (OP_CUSTOM, op[1], op[2], 0), ("",))
        outs = [e.alloc(4) for _ in range(4)]
        for a in outs:
            e.write(a, struct.pack(">I", 0x5A5A5A5A))
        call(e, "custom", [S.slot_addr[op[1]], op[2], outs[0], outs[1], outs[2], outs[3]])
        return [e.u32(a) for a in outs]
    if kind == "particle":
        cl.rec("op", (OP_PARTICLE, op[1], op[2], op[3]), ("",))
        r3, _, _ = call(e, "particle", [op[1], op[2], op[3]])
        return record_index(T.s32((r3 - op[4]) & MASK), MAC_RECORD_SIZE)
    # ---- changes to the world that call no game code
    if kind == "set":
        write_scalar(e, op[1], op[2])
        return []
    if kind == "make_file":
        _, index, buffer, offset, storage, w, loaded = op
        info = e.alloc(0x128)
        e.write(info, b"\xa5" * 0x128)
        set_info(e, info, buffer, offset, w)
        copy = e.alloc(8)
        e.write(copy, struct.pack(">II", vtables(e)[1], storage))
        cl.newlab(info)
        cl.newlab(copy)
        e.write(FILES + 12 * index, struct.pack(">IIB", info, copy, loaded) + b"\0\0\0")
        return []
    if kind == "install":
        _, slot, table, name, values = op
        a, io = sprite_image(e, S, values, name)
        if io:
            cl.newlab(io)
        if table >= 0:
            cl.newlab(a)
            e.write(TABLE + 4 * table, struct.pack(">I", a))
        else:
            S.slot_addr[slot] = a
            cl.labels[a] = L_SLOT + slot
        return []
    if kind == "group":
        e.write(GROUPS + 12 * op[1] + 4, struct.pack(">ii", op[2], op[3]))
        return []
    if kind == "kidfile":
        text = op[2].encode("latin-1") + b"\0"
        a = e.alloc(len(text))
        e.write(a, text)
        e.write(KID_FILES + 4 * op[1], struct.pack(">I", a))
        return []
    raise KeyError(kind)


# -------------------------------------------------------------------------------------------------- the dump, PowerPC side
def pack_text(text, limit):
    text = text[:limit]
    out = [len(text)]
    for i in range(0, len(text), 8):
        word = 0
        for k, ch in enumerate(text[i:i + 8]):
            word |= ord(ch) << (8 * k)
        out.append(word - (1 << 64) if word >= 1 << 63 else word)
    return out


# The text up to the first zero byte, or `limit` bytes.
def cstr_limited(data, limit):
    end = data.find(b"\0")
    if end < 0 or end > limit:
        end = limit
    return data[:end].decode("latin-1")


# A directory entry as numbers (the unit's DumpInfo): its label and, if there is one, its words, name offset and buffer.
def dump_info(e, S, a, out):
    out.append(S.lab(a))
    if a == 0:
        return
    out.append(e.u32(a))
    out.append(e.u32(a + 4))
    out.extend(struct.unpack(">6I", e.read(a + 8, 24)))
    out.append(s32(e.u32(a + 0x20) - (a + 0x24)))
    out.extend(pack_text(cstr_limited(e.read(a + 0x24, 0x104), 0x104), 0x104))


# A sprite as numbers (the unit's DumpSprite): its label and, if it is a live one, every field and its sprite file's words.
def dump_sprite(e, S, a, out):
    out.append(S.lab(a))
    if S.lab(a) <= 0:
        return                      # none, or a table entry left pointing at a sprite that did not load and was deleted
    image = e.read(a, 0x260)
    out.extend(image[0x204:0x208])
    out.append(struct.unpack(">I", image[0x208:0x20C])[0])
    out.append(struct.unpack(">H", image[0x20C:0x20E])[0])
    out.extend(image[0x20E:0x21E])
    out.append(struct.unpack(">H", image[0x21E:0x220])[0])
    for k in range(16):
        first, frames, views = struct.unpack(">HBB", image[0x220 + 4 * k: 0x224 + 4 * k])
        out.extend((first, frames, views))
    out.extend(pack_text(cstr_limited(image[4:0x204], 0x200), 0x200))
    io = struct.unpack(">I", image[0:4])[0]
    out.append(S.lab(io))
    if io:
        out.extend(struct.unpack(">4I", e.read(io, 16)))


# The state as numbers, in the order of the unit's tpw_esp_dump.
def ppc_dump(e, S):
    out = []
    for name, addr, size in SCALARS:
        out.append(read_scalar(e, name))
    out.append(e.u32(display_addr(e)))
    out.extend(pack_text(cstr_limited(e.read(PATH, 0x200), 0x200), 0x200))
    for i in range(14):
        out.extend(struct.unpack(">ii", e.read(GROUPS + 12 * i + 4, 8)))
    out.extend(e.u32(INSTANCES + 4 * i) for i in range(16))
    out.append(e.u32(GLOBAL_INSTANCES))
    out.extend(e.u32(LISTS + 0x28 * i + 4) for i in range(16))
    out.append(e.u32(GLOBAL_LIST + 4))
    files = e.read(FILES, 0x600)
    for i in range(0x80):
        info, storage, loaded = struct.unpack(">IIB", files[12 * i: 12 * i + 9])
        out.append(S.lab(storage) if info else -9)     # a freed storage's address can be a new object's on the host
        out.append(loaded)
        dump_info(e, S, info, out)
    count = s32(read_scalar(e, "sprite_count"))
    count = max(0, min(count, 0x200))
    table = e.read(TABLE, 0x800)
    for i in range(count):
        dump_sprite(e, S, struct.unpack(">I", table[4 * i: 4 * i + 4])[0] if i < 0x200 else 0, out)
    for i in range(4):
        if S.slot_addr[i] is not None:
            dump_sprite(e, S, S.slot_addr[i], out)
    for i in range(4):
        dump_info(e, S, S.info_objs[i], out)
    return out


# ================================================================================================ the host side
class Rec(C.Structure):
    _fields_ = [("label", C.c_char * 40), ("value", C.c_int64)]


# Gives the unit library's entry points their argument and result types (once).
def host_bind(lib):
    if getattr(lib, "esp_bound", False):
        return
    u32p, i64p = C.POINTER(C.c_uint32), C.POINTER(C.c_int64)
    sig = {"tpw_esp_reset": ([], None), "tpw_esp_theme": ([C.c_char_p], None), "tpw_esp_listing": ([C.c_char_p], None),
           "tpw_esp_entry": ([C.c_char_p, C.c_int32, C.c_int32, u32p], None),
           "tpw_esp_file": ([C.c_int32, C.c_char_p, C.c_int32, C.c_char_p, C.c_int32], None),
           "tpw_esp_set_hfield": ([C.c_int32, C.c_int32], None), "tpw_esp_set_display_flags": ([C.c_uint32], None),
           "tpw_esp_set_global": ([C.c_char_p, C.c_int64], None), "tpw_esp_set_group": ([C.c_int32] * 3, None),
           "tpw_esp_set_path_buffer": ([C.c_char_p], None), "tpw_esp_set_kid_file": ([C.c_int32, C.c_char_p], None), "tpw_esp_label_fixed": ([], None),
           "tpw_esp_make_file": ([C.c_int32, C.c_char_p, C.c_int32, C.c_int32, u32p, C.c_int32], None),
           "tpw_esp_set_info": ([C.c_int32, C.c_char_p, C.c_int32, u32p], None),
           "tpw_esp_install": ([C.c_int32, C.c_int32, C.c_char_p, i64p], None),
           "tpw_esp_op": ([C.c_int32, C.c_int64, C.c_int64, C.c_int64, C.c_char_p], C.c_int64),
           "tpw_esp_out": ([C.c_int32], C.c_int64), "tpw_esp_record_stride": ([], C.c_int32), "tpw_esp_dump": ([], C.c_int32), "tpw_esp_dump_data": ([], C.c_void_p)}
    for name, (args, res) in sig.items():
        fn = getattr(lib, name)
        fn.argtypes = args
        fn.restype = res
    lib.esp_bound = True


# ESP_GetParticleSprite answers a pointer into the file's array of records. The Mac's record is 0x18 bytes and the host's
# SSpriteEngineData 0x20 (the padding is named), so the two sides are compared as the record's INDEX in the array and the
# bytes left over when the offset from the array's base is not a whole number of records (always 0 for a right answer).
MAC_RECORD_SIZE = 0x18


def record_index(offset, size):
    index, extra = divmod(offset, size)
    return [index, extra]


# Eight words as a C array.
def u32array(values):
    return (C.c_uint32 * 8)(*[v & MASK for v in values])


def host_events(lib):
    """The call log so far, then cleared (the unit's log holds 4096 events)."""
    n = C.c_int32.in_dll(lib, "tpw_cl_n")
    log = (CL.TpwCallEvent * 4096).in_dll(lib, "tpw_cl_log")
    out = []
    for i in range(min(n.value, 4096)):
        ev = log[i]
        out.append((ev.name.decode("latin-1"), tuple(ev.ints[k] for k in range(ev.nints)),
                    tuple(ev.texts[k].value.decode("latin-1") for k in range(ev.ntexts))))
    if n.value > 4096:
        out.append(("OVERFLOW", (n.value,), ()))
    n.value = 0
    return out


# The unit's dump: the numbers and their labels.
def host_dump(lib):
    n = lib.tpw_esp_dump()
    records = (Rec * n).from_address(lib.tpw_esp_dump_data())
    return [r.value for r in records], [r.label.decode("latin-1") for r in records]


# Runs a case on the host side: the same world and operations through the unit library.
def run_host(lib, case):
    world = case["world"]
    host_bind(lib)
    lib.tpw_esp_reset()
    CL.host_begin(lib, {}, world["queues"])
    lib.tpw_esp_label_fixed()
    lib.tpw_esp_theme(world["theme"].encode("latin-1"))
    for pattern, entries in world["listings"].items():
        lib.tpw_esp_listing(pattern.encode("latin-1"))
        for buffer, offset, storage, w in entries:
            lib.tpw_esp_entry(buffer.encode("latin-1"), offset, storage, u32array(w))
    for storage, name, result, data in world["files"]:
        lib.tpw_esp_file(storage, name.encode("latin-1"), result, data, len(data))
    lib.tpw_esp_set_hfield(world["hfield"][0], world["hfield"][1])
    lib.tpw_esp_set_display_flags(world["display_flags"])
    for name in SCALAR_NAMES:
        lib.tpw_esp_set_global(name.encode(), world["scalars"].get(name, DEFAULT_SCALARS.get(name, 0)))
    for i in range(14):
        first, count = world["groups"].get(i, (0, 0))
        lib.tpw_esp_set_group(i, first, count)
    lib.tpw_esp_set_path_buffer(world["path"].encode("latin-1"))
    for index, (buffer, offset, w) in world["infos"].items():
        lib.tpw_esp_set_info(index, buffer.encode("latin-1"), offset, u32array(w))
    results, dumps, labels, log = [], [], [], []
    for op in case["ops"]:
        results.append(host_op(lib, op))
        log += host_events(lib)
        values, names = host_dump(lib)
        dumps.append(values)
        labels.append(names)
    return {"results": results, "dumps": dumps, "log": log, "labels": labels}


# One operation on the host side; the answer in the shape ppc_op gives.
def host_op(lib, op):
    kind = op[0]
    run = lib.tpw_esp_op
    if kind == "init":
        run(OP_INIT, op[1], op[2], 0, b"")
        return [1]
    if kind == "loadsprites":
        return [run(OP_LOADSPRITES, op[1], 0, 0, b"") & 0xFF]
    if kind == "find":
        return [T.s32(run(OP_FIND, 0, 0, 0, op[1].encode("latin-1")))]
    if kind == "sort":
        run(OP_SORT, 0, 0, 0, b"")
        return []
    if kind == "ctor":
        run(OP_CTOR, op[1], 0, 0, b"")
        return []
    if kind == "load":
        return [run(OP_LOAD, op[1], op[2], op[3], b"") & 0xFF]
    if kind == "frames":
        return [run(OP_FRAMES, op[1], op[2], 0, b"")]
    if kind == "custom":
        run(OP_CUSTOM, op[1], op[2], 0, b"")
        return [lib.tpw_esp_out(i) & MASK for i in range(4)]
    if kind == "particle":
        offset = T.s32((run(OP_PARTICLE, op[1], op[2], op[3], b"") + 0x40000000 - op[4]) & MASK)
        return record_index(offset, lib.tpw_esp_record_stride())
    if kind == "set":
        lib.tpw_esp_set_global(op[1].encode(), op[2])
        return []
    if kind == "make_file":
        _, index, buffer, offset, storage, w, loaded = op
        lib.tpw_esp_make_file(index, buffer.encode("latin-1"), offset, storage, u32array(w), loaded)
        return []
    if kind == "install":
        _, slot, table, name, values = op
        lib.tpw_esp_install(slot, table, name.encode("latin-1"), (C.c_int64 * len(values))(*values))
        return []
    if kind == "group":
        lib.tpw_esp_set_group(op[1], op[2], op[3])
        return []
    if kind == "kidfile":
        lib.tpw_esp_set_kid_file(op[1], op[2].encode("latin-1"))
        return []
    raise KeyError(kind)


# ================================================================================================ the comparison
class Case(dict):
    """A case: {"world", "ops"}; it prints as its script, short enough for a failure line."""

    def __repr__(self):
        ops = " ".join(op[0] + ("(" + ",".join(str(x) for x in op[1:4] if not isinstance(x, (list, tuple))) + ")" if len(op) > 1 else "")
                       for op in self["ops"][:12])
        world = self["world"]
        return "<%s | %d listings, %d files, queues %s>" % (ops, len(world["listings"]), len(world["files"]), sorted(world["queues"]))


class EspTest:
    """One group of cases: world + script, the original on the PowerPC side and the translation on the host side."""

    def __init__(self, name, edge, rnd):
        self.name, self._edge, self._rnd = name, edge, rnd

    def edge_cases(self):
        return [Case(c) for c in self._edge()]

    def random_case(self, r):
        return Case(self._rnd(r))

    def run_case(self, runner, case):
        e, lib = runner.emu, runner.lib
        e.reset_scratch()
        try:
            p = run_ppc(e, case)
        except T.ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = run_host(lib, case)
        return compare(case, p, h)


# The differences between the two sides: results, call log, then the first state that differs.
def compare(case, p, h):
    out = []
    ops = case["ops"]
    for k, (pr, hr) in enumerate(zip(p["results"], h["results"])):
        if pr != hr:
            out.append(f"op {k} {ops[k][0]}{ops[k][1:3]}: result ppc={pr} host={hr}")
    first = CL.first_difference(p["log"], h["log"])
    if first:
        out.append("call log: " + first)
    for k, (pd, hd) in enumerate(zip(p["dumps"], h["dumps"])):
        if len(pd) != len(hd):
            out.append(f"after op {k} {ops[k][0]}: dump length ppc={len(pd)} host={len(hd)}")
            continue
        bad = [i for i in range(len(pd)) if pd[i] != hd[i]]
        for i in bad[:3]:
            out.append(f"after op {k} {ops[k][0]}: {h['labels'][k][i]} ppc={pd[i]:#x} host={hd[i]:#x}")
        if bad:
            break
    return out


# The standard C library fakes (strcpy and the like) for the PowerPC side.
def setup(runner):
    T.install_std_stubs(runner.emu)


# ================================================================================================ the cases
def sprite_values(r, kind=None):
    """The values tpw_esp_install takes (76), for a sprite made by hand."""
    kind = kind if kind is not None else r.randrange(4)
    acts = random_actions(r, r.randrange(5))
    custom = random_custom(r) if kind != 3 else [0xA5] * 16
    v = [r.choice([0, 1]), r.choice([0, 1]), r.randrange(256), r.choice([0, 1]), r.randrange(0x1000), sum(1 for k in range(4) if custom[4 * k] != 0)]
    v += custom
    v.append(sum(1 for a in acts if a[1] != 0))
    for a in acts:
        v += list(a)
    v += [1, r.randrange(0, 40), 0x40000000, r.getrandbits(32), r.getrandbits(32)]
    return v


# A fresh sprite and four hand-made ones, asked every action and block.
def sprite_case_edges():
    r = random.Random(11)
    # a fresh sprite (the constructor over 0xa5): every action, every block
    ops = [("ctor", 0)] + [("frames", 0, a) for a in range(16)] + [("custom", 0, b) for b in range(-2, 7)]
    yield {"world": new_world(), "ops": ops}
    # sprites of known contents
    for kind in range(4):
        ops = [("install", 0, -1, "SPR_TEST", sprite_values(r, kind))] + [("frames", 0, a) for a in range(16)] \
              + [("custom", 0, b) for b in (-1, 0, 1, 2, 3, 4, 0x7FFFFFFF, -0x80000000)]
        yield {"world": new_world(), "ops": ops}


# Hand-made or fresh sprites asked random actions and blocks.
def sprite_case_random(r):
    ops = []
    for slot in range(r.randrange(1, 4)):
        if r.random() < 0.3:
            ops.append(("ctor", slot))
        else:
            ops.append(("install", slot, -1, "SPR_%d" % r.randrange(100), sprite_values(r)))
        for _ in range(r.randrange(1, 6)):
            if r.random() < 0.5:
                ops.append(("frames", slot, r.randrange(16)))
            else:
                ops.append(("custom", slot, r.choice([-1, 0, 1, 2, 3, 4, r.randrange(-5, 9)])))
    return {"world": new_world(), "ops": ops}


NAMES = ["SPR_AA", "SPR_BI", "SPR_KI", "SPR_TA", "SPR_SU", "SPR_ZZ", "spr_mx", "Spr_Q", "SPR_BE", "SPR_CH", "SPR_FR", "SPR_SA",
         "SPR_bi", "spr_ki", "SPR_B", "Z", "abc"]
EXTS = [".ESP", ".ESP", ".esp", ".Esp"]
FOLDERS = ["kids", "kidsheads", "costumes", "costumeheads", "entertainers", "handymen", "mechanics", "guards", "researchers",
           "thoughts", "balloons", "litter", "specialfx", "particles"]
THEMES = ["Hallow", "Space", "Fantasy", "Jungle", "Lost Kingdom", "", "Hallow:", "X"]
STAFF = ["handymen", "mechanics", "guards", "researchers"]


# True with probability p.
def maybe(r, p):
    return r.random() < p


def esp_for_entry(r, real_chance=0.3):
    """The bytes of an .ESP file for a directory entry and the first bytes of its data file."""
    if maybe(r, real_chance) and real_files():
        _, data, head = r.choice(real_files())
        return data, head[:8]
    custom = random_custom(r) if maybe(r, 0.6) else None
    data = esp_bytes(random_actions(r), custom, first_person=r.choice([0, 1, 1, 7]), f206=r.randrange(256),
                     magic=r.choice([b"ESP_FILE", b"ESP_FILE", b"BAD"]))
    if maybe(r, 0.15):
        data = data[:r.choice([0, 1, 7, 8, 12, 268, 269, 270, 271, 300, 333, 334, 335, 340])]
    return data, bytes(r.randrange(256) for _ in range(8))


def make_entry(world, r, storage, taken, missing=0.1, prefix=0.3, name=None):
    """A directory entry (and the files of its sprite set in `storage`); `taken` collects the names."""
    name = name or (r.choice(NAMES) + r.choice(EXTS))
    prefixed = maybe(r, prefix)
    buffer = ("dir:" + name) if prefixed else name
    offset = 4 if prefixed else 0
    esp, head = esp_for_entry(r)
    if not maybe(r, missing):
        add_file(world, storage, buffer, esp, result=r.choice([0, 0, 0, 5]))
    for ext, data_name in data_names(buffer).items():
        if not maybe(r, missing):
            add_file(world, storage, data_name, head)
    taken.append(buffer)
    return entry(buffer, offset, storage, words(r))


def add_folder(world, r, folder, generic, themed, missing=0.1, prefix=0.3, kids=False):
    """Listings for the folder's two patterns with `generic` and `themed` entries."""
    generic_pattern, theme_pattern = patterns(world["theme"], folder)
    for pattern, count in ((generic_pattern, generic), (theme_pattern, themed)):
        if count <= 0:
            continue
        taken = []
        entries = []
        for _ in range(count):
            nm = None
            if kids and maybe(r, 0.5):
                nm = r.choice(["SPR_BI.ESP", "SPR_KI.ESP", "SPR_TA.ESP", "SPR_SU.ESP", "spr_bi.esp", "SPR_KI.esp"])
            entries.append(make_entry(world, r, r.randrange(3), taken, missing, prefix, nm))
        # listings are kept as given (the sorter puts them in order); a pattern both lists share only keeps the later one
        world["listings"][pattern] = entries


# Queued answers for some callees (forced search answers, failed reads and opens, Load answers, counts), with some probability each.
def random_queues(r, world, kinds=("FindFirst", "FindNext", "Read", "OpenFile", "Load", "LoadCount")):
    q = world["queues"]
    if "FindFirst" in kinds and maybe(r, 0.1):
        q["FindFirst"] = [r.choice([-9, -9, 2, 0, -1]) for _ in range(r.randrange(1, 4))] + [-9]
    if "FindNext" in kinds and maybe(r, 0.1):
        q["FindNext"] = [r.choice([-9, 7, 0]) for _ in range(r.randrange(1, 4))] + [-9]
    if "Read" in kinds and maybe(r, 0.12):
        q["Read"] = [r.choice([-2, -2, -2, -1, 0, 1]) for _ in range(r.randrange(1, 80))] + [-2]
    if "OpenFile" in kinds and maybe(r, 0.12):
        q["OpenFile"] = [r.choice([-9, -9, -1, 0, 3]) for _ in range(r.randrange(1, 12))] + [-9]
    if "Load" in kinds and maybe(r, 0.2):
        q["Load"] = [r.choice([1, 1, 0, 2, -1]) for _ in range(r.randrange(1, 8))] + [1]
    if "LoadCount" in kinds and maybe(r, 0.3):
        q["LoadCount"] = [r.choice([0, 1, 5, 40, 0x7FFFFFFF]) for _ in range(r.randrange(1, 6))] + [3]
    if "Init" in kinds and maybe(r, 0.2):
        q["Init"] = [r.choice([0, 0x1234, 0x70000000]) for _ in range(r.randrange(1, 18))] + [0x70000000]
    if "FindResource" in kinds and maybe(r, 0.15):
        q["FindResource"] = [r.choice([1, 0, 2, -1]) for _ in range(r.randrange(1, 3))] + [1]


# Random flags, options and start counts for a case.
def flags_for(r, world, count=None):
    s = world["scalars"]
    s["first_person"] = r.choice([0, 1, 1, 5])
    s["compressed"] = r.choice([1, 1, 0, 2])
    s["semitransparent"] = r.choice([0, 1, 7])
    s["draw_enabled"] = r.choice([0, 1])
    s["third_person"] = r.choice([0, 1])
    if count is not None:
        s["sprite_count"] = count
    world["queues"]["GetNumKidSprites"] = [r.choice([0, 1, 2, 3, 5, 99, -1])]
    world["queues"]["GetNumStaffSprites"] = [r.choice([0, 1, 2, 4, 99, -1])]
    world["path"] = r.choice(["", "stale path text"])


# ---------------------------------------------------------------------------------------------------------- sort
def sort_world(r, n, mode):
    world = new_world()
    ops = [("set", "file_count", n)]
    base = sorted(set(r.choice(NAMES) + r.choice(EXTS) for _ in range(max(n, 1) * 2)))
    names = []
    for i in range(n):
        if mode == "sorted":
            names.append("SPR_%03d.ESP" % i)
        elif mode == "reversed":
            names.append("SPR_%03d.ESP" % (n - i))
        elif mode == "equal":
            names.append(r.choice(["spr_a.esp", "SPR_A.ESP", "Spr_A.esp"]))
        else:
            names.append(r.choice(base))
    for i, nm in enumerate(names):
        prefixed = maybe(r, 0.3)
        buffer = ("zz:" + nm) if prefixed else nm
        ops.append(("make_file", i, buffer, 3 if prefixed else 0, r.randrange(3), words(r), r.randrange(2)))
    ops.append(("sort",))
    return {"world": world, "ops": ops}


# The sorter on 0 to 0x80 entries, sorted, reversed, with equal names, and a random one.
def sort_edges():
    r = random.Random(21)
    for n, mode in ((0, "sorted"), (1, "sorted"), (2, "sorted"), (2, "reversed"), (3, "reversed"), (8, "reversed"), (6, "equal"),
                    (0x80, "sorted"), (0x40, "sorted"), (12, "random"), (12, "random")):
        yield sort_world(r, n, mode)
    yield sort_world(r, 20, "random")


# A random list for the sorter.
def sort_random(r):
    return sort_world(r, r.randrange(0, 18), r.choice(["random", "random", "sorted", "reversed", "equal"]))


# ---------------------------------------------------------------------------------------------------------- LoadFromESPFile
def load_world(r, storage_count=3, real=True):
    world = new_world(r.choice(THEMES))
    flags_for(r, world)
    taken = []
    infos = {}
    for i in range(r.randrange(1, 5)):
        e = make_entry(world, r, r.randrange(storage_count), taken, missing=0.15, prefix=0.4)
        infos[i] = (e[0], e[1], e[3])
    world["infos"] = infos
    random_queues(r, world, ("Read", "OpenFile", "Load", "LoadCount"))
    return world, infos


# A random LoadFromESPFile script.
def load_case(r):
    world, infos = load_world(r)
    ops = [("ctor", 0)]
    if maybe(r, 0.2):
        ops.append(("ctor", 1))
    for _ in range(r.randrange(1, 4)):
        slot = r.randrange(2) if len(ops) > 1 and ops[1][0] == "ctor" else 0
        info = r.choice(list(infos))
        # the storage the entry was made for most of the time, another one sometimes
        storage = r.randrange(3)
        ops.append(("load", slot, info, storage))
        if maybe(r, 0.3):
            ops.append(("frames", slot, r.randrange(16)))
            ops.append(("custom", slot, r.randrange(4)))
    return {"world": world, "ops": ops}


# LoadFromESPFile on every shipped file, on files cut at every length that matters, with every extension, on failures and short names.
def load_edges():
    r = random.Random(31)
    # every shipped file, loaded in its own storage, with and without first person sprites
    for name, data, head in real_files()[:46]:
        world = new_world("Space")
        flags_for(r, world)
        world["scalars"]["first_person"] = r.choice([0, 1])
        world["scalars"]["compressed"] = 1
        buffer = name
        add_file(world, 0, buffer, data)
        for ext, dn in data_names(buffer).items():
            add_file(world, 0, dn, head)
        world["infos"] = {0: (buffer, 0, words(r))}
        yield {"world": world, "ops": [("ctor", 0), ("load", 0, 0, 0), ("frames", 0, 3), ("custom", 0, 0), ("custom", 0, 2)]}
    # synthetic files of every length that matters (the reads that come short), custom blocks or not
    full = esp_bytes(random_actions(r, 1), random_custom(r))
    for cut in (0, 1, 8, 12, 268, 269, 270, 271, 272, 333, 334, 335, 340, 349, 350, 400):
        for fp in (0, 1):
            world = new_world("Fantasy")
            flags_for(r, world)
            world["scalars"]["first_person"] = fp
            data = (full + bytes(60))[:cut]
            add_file(world, 1, "SPR_QQ.ESP", data)
            for ext, dn in data_names("SPR_QQ.ESP").items():
                add_file(world, 1, dn, b"\x01\x02\x03\x04\x05")
            world["infos"] = {0: ("SPR_QQ.ESP", 0, words(r))}
            yield {"world": world, "ops": [("ctor", 0), ("load", 0, 0, 1)]}
    # the flags: the four extensions, a file with and without first person sprites
    for fp in (0, 1):
        for comp in (0, 1):
            for has in (0, 1):
                world = new_world("Jungle")
                world["scalars"].update(first_person=fp, compressed=comp, semitransparent=has)
                esp = esp_bytes(random_actions(r, 1), random_custom(r), first_person=has)
                add_file(world, 0, "SPR_FF.ESP", esp)
                for ext, dn in data_names("SPR_FF.ESP").items():
                    add_file(world, 0, dn, b"\x01\x02\x03\x04" + bytes([ord(ext[1]), ord(ext[2])]))
                world["infos"] = {0: ("SPR_FF.ESP", 0, words(r))}
                yield {"world": world, "ops": [("ctor", 0), ("load", 0, 0, 0)]}
    # failures: the first file does not open (-1, and 0 is a success), the second does not open, Load answers 0, 2, -1
    for queue in ({"OpenFile": [-1, -9]}, {"OpenFile": [-9, -1]}, {"OpenFile": [3, 0]}, {"Load": [0]}, {"Load": [2]}, {"Load": [-1]},
                  {"Read": [-1, -2]}, {"Read": [-2] * 53 + [-1, -2]}, {"Read": [-2] * 53 + [0, -2]}, {"Read": [-2] * 53 + [2, -2]},
                  {"Read": [-2] * 54 + [-1, -2]}, {"LoadCount": [0, 3]}):
        world = new_world("Hallow")
        flags_for(r, world)
        world["scalars"]["first_person"] = 0
        add_file(world, 0, "SPR_FF.ESP", esp_bytes(random_actions(r, 1), random_custom(r)))
        for ext, dn in data_names("SPR_FF.ESP").items():
            add_file(world, 0, dn, b"\x01\x02\x03\x04")
        world["infos"] = {0: ("SPR_FF.ESP", 0, words(r))}
        world["queues"].update(queue)
        yield {"world": world, "ops": [("ctor", 0), ("load", 0, 0, 0)]}
    # a name pointer into the buffer, a short name, no storage that has the file
    world = new_world("Hallow")
    flags_for(r, world)
    add_file(world, 2, "dir:sub:SPR_NP.ESP", esp_bytes(random_actions(r, 4), None))
    for ext, dn in data_names("dir:sub:SPR_NP.ESP").items():
        add_file(world, 2, dn, b"\x09\x08\x07\x06")
    world["infos"] = {0: ("dir:sub:SPR_NP.ESP", 8, words(r)), 1: ("abc", 0, words(r)), 2: ("ab", 1, words(r)), 3: ("", 0, words(r))}
    yield {"world": world, "ops": [("ctor", 0), ("load", 0, 0, 2), ("load", 0, 1, 2), ("load", 0, 2, 2), ("load", 0, 3, 0), ("load", 0, 0, 0)]}


# ---------------------------------------------------------------------------------------------------------- FindESPFiles
def find_world(r, folder=None, kids_bias=0.5):
    theme = r.choice(THEMES)
    world = new_world(theme)
    folder = folder or r.choice(FOLDERS + ["Kids", "KIDSHEADS", "Handymen", "RESEARCHERS", "Particles", "x", "kid"])
    kids = folder.lower() in ("kids", "kidsheads")
    flags_for(r, world, count=0 if maybe(r, 0.85) else r.choice([0x1F8, 0x1FC, 0x1FD, 0x1FE, 0x1FF, 5]))
    generic = r.choice([0, 1, 2, 3, 5, 7])
    themed = r.choice([0, 0, 1, 2, 4])
    add_folder(world, r, folder, generic, themed, kids=kids and maybe(r, kids_bias), missing=r.choice([0.0, 0.1, 0.3]))
    random_queues(r, world)
    return world, folder


# A random ESP_FindESPFiles script.
def find_case(r):
    world, folder = find_world(r)
    ops = [("find", folder)]
    # (a second search after the table is full would store past the Mac's table, on top of other globals: not tried)
    if maybe(r, 0.2) and world["scalars"]["sprite_count"] < 0x100:
        ops.append(("find", r.choice(FOLDERS)))
    return {"world": world, "ops": ops}


# ESP_FindESPFiles on every folder kind and cap, with the table and the list filling, and forced search answers.
def find_edges():
    r = random.Random(41)
    for folder in ("kids", "kidsheads", "KIDS", "handymen", "mechanics", "guards", "researchers", "costumes", "particles", "x"):
        for limit in (None, 0, 1, 2, 99):
            world = new_world("Hallow")
            flags_for(r, world, count=0)
            world["scalars"]["first_person"] = 0
            world["scalars"]["compressed"] = 1
            if limit is not None:
                world["queues"]["GetNumKidSprites"] = [limit]
                world["queues"]["GetNumStaffSprites"] = [limit]
            add_folder(world, r, folder, 5, 2, missing=0.0, prefix=0.2, kids=folder.lower() in ("kids", "kidsheads"))
            yield {"world": world, "ops": [("find", folder)]}
    # two of the preferred names alike: the second pass meets the files the first loaded (and a name nothing has)
    for names in (("SPR_BI.ESP", "SPR_BI.ESP", "SPR_TA.ESP", "SPR_SU.ESP"), ("spr_ki.esp", "SPR_KI.ESP", "SPR_KI.ESP", "NOPE.ESP")):
        for limit in (1, 2, 99):
            world = new_world("Hallow")
            flags_for(r, world, count=0)
            world["queues"]["GetNumKidSprites"] = [limit]
            gp, tp = patterns("Hallow", "kids")
            taken = []
            world["listings"][gp] = [make_entry(world, r, 0, taken, 0.0, 0.0, nm) for nm in ("SPR_BI.ESP", "SPR_KI.ESP", "spr_ki.esp", "SPR_TA.ESP")]
            world["listings"][tp] = [make_entry(world, r, 1, taken, 0.0, 0.0, "SPR_BI.esp")]
            ops = [("kidfile", i, nm) for i, nm in enumerate(names)] + [("find", "kids")]
            yield {"world": world, "ops": ops}
    # the table fills up
    for count in (0x1FF, 0x1FE, 0x1FD, 0x1FC):
        for folder in ("kids", "handymen", "costumes"):
            world = new_world("Space")
            flags_for(r, world, count=count)
            world["queues"]["GetNumKidSprites"] = [50]
            world["queues"]["GetNumStaffSprites"] = [50]
            add_folder(world, r, folder, 4, 3, missing=0.2, kids=folder == "kids")
            yield {"world": world, "ops": [("find", folder)]}
    # the file list fills up (0x7f, 0x80, 0x81, 0x90 entries; none of them has a file, so each load fails at once)
    for n in (0x7F, 0x80, 0x81, 0x90):
        world = new_world("Fantasy")
        flags_for(r, world, count=0)
        gp, tp = patterns("Fantasy", "costumes")
        world["listings"][gp] = [entry("SPR_%03d.ESP" % i, 0, i % 3, words(r)) for i in range(n)]
        world["listings"][tp] = [entry("SPR_%03d.ESP" % (n + i), 0, 1, words(r)) for i in range(4)]
        yield {"world": world, "ops": [("find", "costumes")]}
    # forced answers of the search, no listing, an empty theme name
    for queue in ({"FindFirst": [2, -9]}, {"FindFirst": [0, -9]}, {"FindFirst": [-9, 2]}, {"FindNext": [7, 7, -9]}, {"FindNext": [0, -9]},
                  {"FindFirst": [-1, -1, -9]}):
        world = new_world("Jungle")
        flags_for(r, world, count=0)
        add_folder(world, r, "guards", 3, 3, missing=0.0)
        world["queues"].update(queue)
        yield {"world": world, "ops": [("find", "guards")]}
    world = new_world("")
    flags_for(r, world, count=0)
    add_folder(world, r, "litter", 2, 2, missing=0.0)
    yield {"world": world, "ops": [("find", "litter")]}


# ---------------------------------------------------------------------------------------------------------- ESP_LoadSprites
def loadsprites_world(r, budget=20):
    world = new_world(r.choice(THEMES))
    flags_for(r, world, count=r.choice([0, 0, 3]))
    world["scalars"]["total_sprites"] = r.choice([0, 9])
    left = budget
    for folder in FOLDERS:
        if left <= 0 or not maybe(r, 0.5):
            continue
        g, t = r.choice([0, 1, 2]), r.choice([0, 0, 1])
        left -= g + t
        add_folder(world, r, folder, g, t, missing=0.1, kids=folder in ("kids", "kidsheads") and maybe(r, 0.5))
    random_queues(r, world, ("FindFirst", "FindNext", "OpenFile", "Load", "FindResource"))
    return world


# A random ESP_LoadSprites script.
def loadsprites_case(r):
    world = loadsprites_world(r)
    ops = [("loadsprites", r.choice([0, 1, 1]))]
    if maybe(r, 0.2):
        ops.append(("loadsprites", r.choice([0, 1])))
    return {"world": world, "ops": ops}


# ESP_LoadSprites with the resource found, missing and answering other numbers, for both arguments.
def loadsprites_edges():
    r = random.Random(51)
    for flag in (0, 1, 2):
        for resource in (1, 0, 2, -1):
            world = new_world("Hallow")
            flags_for(r, world, count=4)
            world["queues"]["FindResource"] = [resource]
            for folder in ("kids", "particles", "guards", "litter"):
                add_folder(world, r, folder, 2, 1, missing=0.0, kids=folder == "kids")
            yield {"world": world, "ops": [("loadsprites", flag)]}


# ---------------------------------------------------------------------------------------------------------- ESP_InitialiseAndLoadSprites
HFIELDS = [(64, 64), (0, 0), (1, 1), (3, 3), (4, 4), (5, 5), (7, 8), (63, 65), (128, 100), (256, 256), (-1, -1), (-4, -5), (-7, 9),
           (0x7FFFFFFF, 0x7FFFFFFF), (-0x80000000, 3), (1000, 3), (2, 1000)]


# A world for ESP_InitialiseAndLoadSprites: listings, a height field and random start values for what it sets.
def init_world(r):
    world = loadsprites_world(r, budget=8)
    world["hfield"] = r.choice(HFIELDS) if maybe(r, 0.7) else (r.randrange(-300, 300), r.randrange(-300, 300))
    world["display_flags"] = r.choice([0, 0x410100, 0x1000000, 0xFFFFFFFF, r.getrandbits(32)])
    world["scalars"]["f20"] = r.getrandbits(32)
    world["scalars"]["f34"] = r.getrandbits(32)
    world["scalars"]["f88"] = r.getrandbits(32)
    world["scalars"]["hot_x"] = r.getrandbits(32)
    world["scalars"]["hot_y"] = r.getrandbits(32)
    for k in ("map_width", "map_height", "cells_wide", "cells_high"):
        world["scalars"][k] = r.getrandbits(32)
    random_queues(r, world, ("Init",))
    return world


# A random ESP_InitialiseAndLoadSprites script.
def init_case(r):
    world = init_world(r)
    ops = [("init", r.choice([0, 1]), r.choice([0, 1]))]
    if maybe(r, 0.25):
        ops.append(("init", r.choice([0, 1]), r.choice([0, 1])))
    return {"world": world, "ops": ops}


# ESP_InitialiseAndLoadSprites on 17 height field shapes and all four argument pairs, and with the resource missing.
def init_edges():
    r = random.Random(61)
    for hf in HFIELDS:
        for first, lobby in ((0, 0), (1, 1), (0, 1), (1, 0)):
            world = new_world("Space")
            flags_for(r, world, count=0)
            world["hfield"] = hf
            world["scalars"].update(f20=0xDEADBEEF, f34=0xCAFEF00D, f88=0x3F800000, hot_x=0x40490FDB, hot_y=0xC0000000, draw_enabled=0)
            world["display_flags"] = 0x410100
            add_folder(world, r, "particles", 2, 1, missing=0.0)
            for folder in ("kids", "costumes"):
                add_folder(world, r, folder, 1, 1, missing=0.0)
            yield {"world": world, "ops": [("init", first, lobby)]}
    world = new_world("Space")
    flags_for(r, world, count=0)
    world["queues"]["Init"] = [0, 0x55, 0]
    world["queues"]["FindResource"] = [0]
    yield {"world": world, "ops": [("init", 0, 0)]}


# ---------------------------------------------------------------------------------------------------------- ESP_GetParticleSprite
def particle_world(r, sets=None):
    world = new_world("Hallow")
    first = r.choice([0, 0, 1, 3, 17])
    sets = sets or r.randrange(1, 4)
    world["groups"] = {13: (first, sets)}
    world["scalars"]["sprite_count"] = first + sets + r.choice([0, 0, 2])
    ops, bases = [], []
    for k in range(sets):
        v = sprite_values(r, r.randrange(3))
        v[-5:] = [1, r.choice([0, 1, 2, 5, 16, 40, 100]), 0x40000000 + r.choice([0, 0x18, 0x1000]), r.getrandbits(32), r.getrandbits(32)]
        ops.append(("install", 0, first + k, "SPR_P%d" % k, v))
        bases.append(v[-3])
    return world, ops, first, sets, bases


# A random ESP_GetParticleSprite script.
def particle_case(r):
    world, ops, first, sets, bases = particle_world(r)
    for _ in range(r.randrange(2, 9)):
        sprite = r.randrange(0, 16 * sets)
        frame = r.choice([0, 1, 2, 3, 7, 39, 40, 41, -1, -5, r.randrange(-3, 50)])
        variant = r.choice([0, 1, 2, 7, 8, 9, 15, -1, -8, r.randrange(-20, 40)])
        ops.append(("particle", sprite, frame, variant, bases[sprite // 16]))
    return {"world": world, "ops": ops}


# ESP_GetParticleSprite on actions with and without views, frames, first records and record counts.
def particle_edges():
    r = random.Random(71)
    for views in (0, 1):
        for frames in (0, 1, 4):
            for first_frame in (0, 3, 0xFFFF):
                for count in (0, 1, 8, 40):
                    world = new_world("Hallow")
                    world["groups"] = {13: (2, 1)}
                    world["scalars"]["sprite_count"] = 3
                    v = sprite_values(r, 0)
                    for a in range(16):
                        v[23 + 3 * a: 26 + 3 * a] = [first_frame if a % 2 else a, frames if a % 3 else 0, views]
                    v[-5:] = [1, count, 0x40000000, 0x11111111, 0x22222222]
                    ops = [("install", 0, 2, "SPR_P", v)]
                    for sprite in (0, 1, 2, 5, 15):
                        for frame, variant in ((0, 0), (1, 3), (5, 7), (-1, 0), (0, 8), (count, 0), (count - 1, 0), (2, -1)):
                            ops.append(("particle", sprite, frame, variant, 0x40000000))
                    yield {"world": world, "ops": ops}


# ---------------------------------------------------------------------------------------------------------- scripts
def script_case(r):
    """A script of operations on one world: the loads, the table, the lists and the getters in a mixed order."""
    world = loadsprites_world(r, budget=10)
    world["hfield"] = r.choice(HFIELDS[:10])
    random_queues(r, world, ("Init",))
    folders = [f for f in FOLDERS]
    first = r.choice([0, 2])
    ops = []
    extra, sets = [], 0
    for _ in range(r.randrange(2, 7)):
        k = r.randrange(9)
        if k == 0:
            ops.append(("find", r.choice(folders)))
        elif k == 1:
            ops.append(("loadsprites", r.randrange(2)))
        elif k == 2:
            ops.append(("init", r.randrange(2), r.randrange(2)))
        elif k == 3:
            ops.append(("ctor", 0))
            infos = list(world["infos"]) or [0]
            if not world["infos"]:
                buffer = "SPR_ZQ.ESP"
                add_file(world, 0, buffer, esp_bytes(random_actions(r), random_custom(r)))
                for ext, dn in data_names(buffer).items():
                    add_file(world, 0, dn, b"\x01\x02\x03")
                world["infos"] = {0: (buffer, 0, words(r))}
            ops.append(("load", 0, 0, 0))
        elif k == 4:
            ops.append(("install", 1, -1, "SPR_X", sprite_values(r)))
            ops.append(("frames", 1, r.randrange(16)))
            ops.append(("custom", 1, r.randrange(4)))
        elif k == 5:
            n = r.randrange(0, 6)
            ops.append(("set", "file_count", n))
            for i in range(n):
                ops.append(("make_file", i, r.choice(NAMES) + ".ESP", 0, r.randrange(3), words(r), 0))
            ops.append(("sort",))
        elif k == 6:
            ops.append(("set", "sprite_count", r.choice([0, 1, 7])))
        elif k == 7:
            ops.append(("set", "first_person", r.randrange(2)))
        else:
            ops.append(("set", "compressed", r.randrange(2)))
    return {"world": world, "ops": ops}


# Three random scripts as edge cases.
def script_edges():
    r = random.Random(81)
    for _ in range(3):
        yield script_case(r)


TESTS = [
    EspTest("ESP_CSprite constructor, GetActionFrames, GetCustomData", sprite_case_edges, sprite_case_random),
    EspTest("ESP_SortFileList", sort_edges, sort_random),
    EspTest("ESP_CSprite::LoadFromESPFile", load_edges, load_case),
    EspTest("ESP_FindESPFiles", find_edges, find_case),
    EspTest("ESP_LoadSprites", loadsprites_edges, loadsprites_case),
    EspTest("ESP_InitialiseAndLoadSprites", init_edges, init_case),
    EspTest("ESP_GetParticleSprite", particle_edges, particle_case),
    EspTest("scripts of mixed operations", script_edges, script_case),
]
