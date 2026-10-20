"""Differential tests for TPConfig.cpp, the quickload indexes and BuildGeometryTables
(src/TPConfig.cpp, src/Quickload.cpp, src/rsse.cpp; Milestone 1 unit 4), against
libtpw_unit_TPConfig.dylib (tests/unit_TPConfig.cpp's fakes; see calllog.py).

Every translated function is called directly; the unit's own callees run as themselves on
both sides. A case sets the same globals, player settings, search paths, quickload
indexes, files and return queues on both sides, runs the function and compares the call
logs, the result and what the unit writes: its globals, the screen size and centre, the
sine table, the search path, the indexes (counts, names and how the names are packed) and
the file name buffer. Blocks compare by label.

Allocation sizes: the PowerPC side records each MReleaseAllocF size as the host's
equivalent (pointers and index entries are larger there), chosen by the call site.
"""
import ctypes as C
import random
import struct

from unicorn import ppc_const as P

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_TPConfig"

F_ = dict(
    InitialiseGameStage1=0x100591A4, InitialiseGameStage2=0x10059254, InitialiseGameStage3=0x100592A8,
    ClosedownGame=0x10059670, DoesFileExist=0x10059714, FindFilePath=0x100597F8, TPSaveRegistry=0x1005999C,
    BuildGeometryTables=0x100C30A8, InitialiseQuickloading=0x1006736C, LoadQuickloadIndex=0x10067408,
    LoadQuickloadIndexes=0x100675D8, FreeQuickloadIndexes=0x100677C8, CloseQuickloading=0x100678C4,
    DoesQuickloadFilenameExist=0x1006795C, FindQuickloadFilename=0x10067A50, CheckIndexedFileExists=0x10067B50,
    SetupDirectoryForMeshLoad=0x100680FC)
SINIT = 0x100599C8
A = dict(
    gTPWFlags=0x102B2B4C, gGameOptions=0x102B2B48, gTPDialogOptions=0x103BC664, gTPDialogTextureMegabytes=0x103BC65C,
    gScreenWidth=0x102B2B54, gScreenHeight=0x102B2B50, gViewCentreX=0x102B2B6C, gViewCentreY=0x102B2B68,
    gScreenCentreX=0x102B2B64, gScreenCentreY=0x102B2B60, gPlayerSettings=0x1038071C, gResolutionBalance=0x103BC640,
    gTPGlobals=0x103B74C8, gThemeTextureFolder=0x103963CC, gFilePathBuffer=0x102C1798, gPathString=0x102C1B98,
    gPaths=0x102C1B9C, gPathLength=0x102C1BA0, gQuickloadTextureIndexes=0x102C7994, gQuickloadIndexes=0x102C7998,
    gQuickloadInitialised=0x102C799C, gQuickloadEnabled=0x102C79A0, gQuickloadPathCount=0x1029CF08)
SCALARS = ["gTPWFlags", "gGameOptions", "gTPDialogOptions", "gTPDialogTextureMegabytes", "gScreenWidth",
           "gScreenHeight", "gViewCentreX", "gViewCentreY", "gScreenCentreX", "gScreenCentreY", "gPathLength",
           "gQuickloadInitialised", "gQuickloadEnabled", "gQuickloadPathCount"]
# CPlayerSettings fields (Mac offsets), in tpw_cfg_set_settings order.
SETTINGS = [0x04, 0x78, 0x7C, 0x84, 0x8C, 0x90]
LABELS = dict(gPlayerSettings=1, gResolutionBalance=2, gTPGlobals=3, search=4, gFilePathBuffer=5, paths=9,
              folder_table=20, texture_table=21, file_arg=70, folder_arg=71, shared_arg=72, name_arg=73,
              result_slot=74, index_arg=80, data_arg=81)
PATH_LABEL = 10          # + search path number
ENTRY_LABEL = 30         # + 4 * path + 2 * textures: the names array; + 1: the text block
FILE_LABEL = 60          # + file number
FILE_HANDLE = 0x6E000000  # PowerPC handles (unmapped, never dereferenced): + 0x100 * file number
NAMES_SITES = (0x100592F4, 0x10067494)     # MReleaseAllocF call sites allocating pointer arrays
INDEX_SITES = (0x1006739C, 0x100673B8)     # ... allocating index entries
HOST = dict(index_size=24)

VP, I32, U32, I64 = C.c_void_p, C.c_int32, C.c_uint32, C.c_int64


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


def letter(c):
    return 0x41 <= c <= 0x5A or 0x61 <= c <= 0x7A


def digit(c):
    return 0x30 <= c <= 0x39


def punct(c):
    return 0x21 <= c <= 0x2F or 0x3A <= c <= 0x40 or 0x5B <= c <= 0x60 or 0x7B <= c <= 0x7E


def token_char(c):
    return letter(c) or digit(c) or punct(c)


def packed(names):
    return b"".join(n.encode("latin-1") + b"\0" for n in names)

# ======================================================================= PowerPC side


class CfgState:
    pass


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(F_.values())
    S.real.add(SINIT)

    def i(addr, name, spec="", ret=None, then=None):
        S.internal(addr, R(name, spec, ret, then))

    for sym, fn in CL.string_stubs().items():
        S.imp(sym, fn)

    def alloc(e):
        size, flags = e.r(3), e.r(4)
        site = e.uc.reg_read(P.UC_PPC_REG_LR) & 0xFFFFFFFF
        host = size // 4 * 8 if site in NAMES_SITES else size // 12 * HOST["index_size"] if site in INDEX_SITES else size
        e.cl.rec("MReleaseAllocF", (host & 0xFFFFFFFF, flags))
        ok = e.cl.ret("MReleaseAllocF", 1) != 0
        if size == 0 or not ok:
            e.set_r(3, 0)
            return
        block = e.alloc(max(size, 4))
        e.cl.newlab(block)
        e.set_r(3, block)

    S.internal(0x10064BD4, alloc)
    i(0x10064C54, "MReleaseFreeF", "L", ret=lambda e, L: 0)
    i(0x1005B634, "ResetWorldBoundingArea")
    i(0x1014A10C, "ConverttoCurrentTheme", "i")
    i(0x100B88F0, "GetFileAccessIndex", "", "i")
    i(0x100B8928, "SetFileAccessIndex", "i")
    i(0x100B98A4, "CheckFileExists", "s", "i")
    S.internal(0x100B89DC, lambda e: e.cl.rec("SetNewMeshPaths", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), e.cl.lab(e.r(6))),
                                              tuple(e.cstr(e.r(k))[:159] for k in (3, 4, 5))))

    def ctrace(e):
        e.cl.rec("CTRACE", (e.r(3) & 0xFFFFFFFF,), (T.c_format(e, e.cstr(e.r(4)), 5)[:159],))

    S.internal(0x1006FCBC, ctrace)

    def stricmp(e):
        a, b = e.cstr(e.r(3)), e.cstr(e.r(4))
        e.cl.rec("_stricmp", (), (a[:159], b[:159]))
        la, lb = a.lower(), b.lower()
        e.set_r(3, 0 if la == lb else T.u32(-1 if la < lb else 1))

    S.imp("_stricmp__10NS_MacDozeFPCcPCc", stricmp)

    def length(e):
        e.cl.rec("TbStringBase::Length", (e.cl.lab(e.r(3)),), (CL.tds_text(e, e.r(3)),))
        e.set_r(3, e.u32(e.r(3) + 0xC))

    S.imp("Length__15TbStringBase<c>CFv", length)

    # ---- the files
    def handle_file(e, h):
        k = (h - FILE_HANDLE) // 0x100
        return e.cfg.files[k] if h >= FILE_HANDLE and (h - FILE_HANDLE) % 0x100 == 0 and k < len(e.cfg.files) else None

    def file_open(e):
        name, mode = e.cstr(e.r(3)), e.cstr(e.r(4))
        e.cl.rec("TPFile_Open", (), (name[:159], mode[:159]))
        for k, (fname, _) in enumerate(e.cfg.files):
            if fname == name:
                e.set_r(3, FILE_HANDLE + 0x100 * k)
                return
        e.set_r(3, 0)

    def file_size(e):
        e.cl.rec("TPFile_Size", (e.cl.lab(e.r(3)),))
        f = handle_file(e, e.r(3))
        e.set_r(3, len(f[1]) if f else 0)

    def file_read(e):
        dest, size, count, h = e.r(3), e.r(4), e.r(5), e.r(6)
        e.cl.rec("TPFile_Read", (e.cl.lab(dest), size, count, e.cl.lab(h)))
        f = handle_file(e, h)
        if f is None:
            e.set_r(3, 0)
            return
        n = min(size * count, len(f[1]))
        if n:
            e.write(dest, f[1][:n])
        e.set_r(3, count)

    S.internal(0x10003FD4, file_open)
    S.internal(0x100044E8, file_size)
    S.internal(0x10004260, file_read)
    i(0x10004354, "TPFile_Close", "L", ret=lambda e, L: 0)

    # ---- the text reader (the same rules as unit_TPConfig.cpp's)
    def parse_state(e):
        p = e.r(3)
        return p, e.u32(p), e.u32(p + 4), e.u32(p + 8)

    def advance(e, p):
        w32(e, p, e.u32(p) + 1)
        w32(e, p + 4, e.u32(p + 4) + 1)

    def skip(e):
        p, cur, pos, size = parse_state(e)
        e.cl.rec("SkipWhitespace", (pos,))
        while True:
            cur, pos = e.u32(p), e.u32(p + 4)
            if pos >= size:
                e.set_r(3, 1)
                return
            if token_char(e.read(cur, 1)[0]):
                e.set_r(3, 0)
                return
            advance(e, p)

    def is_white(e):
        p, cur, pos, size = parse_state(e)
        e.cl.rec("IsWhiteSpace", (pos,))
        if e.cl.ret("IsWhiteSpace") != 0:
            e.set_r(3, 1)
            return
        e.set_r(3, 0 if token_char(e.read(cur, 1)[0]) else 1)

    def get_string(e):
        p, cur, pos, size = parse_state(e)
        text, limit = e.r(4), T.s32(e.r(5))
        flags = [T.s32(e.r(k)) for k in (6, 7, 8, 9)]
        e.cl.rec("GetString", (pos, limit) + tuple(flags))
        e.write(text, b"\0")
        n = 0
        if e.cl.ret("GetString") != 0:      # scripted: an empty name, nothing read
            e.set_r(3, 0)
            return
        if pos >= size:
            e.set_r(3, 1)
            return
        while n < limit - 1:
            cur, pos = e.u32(p), e.u32(p + 4)
            if pos >= size:
                e.set_r(3, 1)
                return
            ch = e.read(cur, 1)[0]
            ok = ((flags[0] and letter(ch)) or (flags[1] and digit(ch)) or (flags[2] and punct(ch))
                  or (flags[3] and ch in (0x23, 0x3B)))
            if not ok:
                e.set_r(3, 0)
                return
            e.write(text + n, bytes([ch, 0]))
            n += 1
            advance(e, p)
        e.set_r(3, 0)

    S.internal(0x10030C08, skip)
    S.internal(0x10030EDC, is_white)
    S.internal(0x10031014, get_string)
    return S


def ppc_index_entries(e, table, n):
    out = []
    for p in range(n):
        ent = table + 12 * p
        out.append(ppc_entry(e, ent))
    return out


def ppc_entry(e, ent):
    L = e.cl
    count, names, text = e.u32(ent), e.u32(ent + 4), e.u32(ent + 8)
    texts, offsets = [], []
    if names:
        for k in range(min(count, 64)):
            a = e.u32(names + 4 * k)
            texts.append(e.cstr(a, limit=300) if a else None)
            offsets.append(a - text if text and text <= a < text + 0x10000 else -1)
    return dict(count=count, names=L.lab(names), text=L.lab(text), texts=texts, offsets=offsets)


def ppc_begin(e, c):
    s = e.cfg = CfgState()
    fixed = {A["gPlayerSettings"]: LABELS["gPlayerSettings"], A["gResolutionBalance"]: LABELS["gResolutionBalance"],
             A["gTPGlobals"]: LABELS["gTPGlobals"], A["gTPGlobals"] + 0x4060: LABELS["search"],
             A["gFilePathBuffer"]: LABELS["gFilePathBuffer"]}
    s.files = [(name, bytes(data)) for name, data in c.get("files", [])]
    for k in range(len(s.files)):
        fixed[FILE_HANDLE + 0x100 * k] = FILE_LABEL + k
    g = c.get("g", {})
    for name in SCALARS:
        w32(e, A[name], g.get(name, 0))
    for off, v in zip(SETTINGS, c.get("settings", [0] * 6)):
        w32(e, A["gPlayerSettings"] + off, v)
    w32(e, A["gResolutionBalance"] + 0xC, c.get("resolution", 0))
    CL.tds_set(e, A["gTPGlobals"] + 0x4060, c.get("search", ""))
    header = c.get("sin_header", (0, 0, 0))
    e.write(A["gTPGlobals"], struct.pack(">III", *header))
    e.write(A["gTPGlobals"] + 0xC, struct.pack(">I", c.get("sin_fill", 0)) * 0x1000)
    e.write(A["gThemeTextureFolder"], c.get("texfolder", "textures").encode("latin-1")[:15].ljust(16, b"\0"))
    e.write(A["gFilePathBuffer"], c.get("pathbuf", "").encode("latin-1") + b"\0")
    # search paths
    s.paths = []
    paths = c.get("paths")
    if paths is None:
        w32(e, A["gPaths"], 0)
    else:
        arr = e.alloc(4 * max(len(paths), 1))
        fixed[arr] = LABELS["paths"]
        for k, text in enumerate(paths):
            a = CL.new_text(e, text) if text is not None else 0
            if a:
                fixed[a] = PATH_LABEL + k
            w32(e, arr + 4 * k, a)
            s.paths.append(a)
        w32(e, A["gPaths"], arr)
    w32(e, A["gPathString"], s.paths[0] if s.paths and c.get("pathstring", True) else 0)
    # quickload indexes: c["indexes"] = [(folder names or None, texture names or None) per path]
    indexes = c.get("indexes")
    for t, (name, label) in enumerate((("gQuickloadIndexes", "folder_table"), ("gQuickloadTextureIndexes", "texture_table"))):
        if indexes is None:
            w32(e, A[name], 0)
            continue
        table = e.alloc(12 * max(len(indexes), 1))
        fixed[table] = LABELS[label]
        for p, pair in enumerate(indexes):
            names = pair[t]
            ent = table + 12 * p
            if not names:
                e.write(ent, struct.pack(">III", 0, 0, 0))
                continue
            blob = packed(names)
            text = e.alloc(len(blob), blob)
            arr = e.alloc(4 * len(names))
            off = 0
            for k, n in enumerate(names):
                w32(e, arr + 4 * k, text + off)
                off += len(n.encode("latin-1")) + 1
            fixed[arr] = ENTRY_LABEL + 4 * p + 2 * t
            fixed[text] = ENTRY_LABEL + 4 * p + 2 * t + 1
            e.write(ent, struct.pack(">III", len(names), arr, text))
        w32(e, A[name], table)
    e.cl = CL.PpcLog(fixed, c.get("q", {}))


def ppc_observe(e, c):
    L = e.cl
    out = {name: e.u32(A[name]) for name in SCALARS}
    out["sin_header"] = list(struct.unpack(">III", e.read(A["gTPGlobals"], 12)))
    if c.get("observe_sin"):
        out["sin"] = e.read(A["gTPGlobals"] + 0xC, 0x4000)
    paths = e.u32(A["gPaths"])
    out["paths"] = L.lab(paths)
    out["path_entries"] = [L.lab(e.u32(paths + 4 * k)) for k in range(c.get("path_entries", 1))] if paths else []
    ps = e.u32(A["gPathString"])
    out["pathstring"] = (L.lab(ps), e.cstr(ps, limit=300) if ps else None)
    out["pathbuf"] = e.cstr(A["gFilePathBuffer"], limit=0x400)
    n = c.get("g", {}).get("gQuickloadPathCount", 0)
    for name in ("gQuickloadIndexes", "gQuickloadTextureIndexes"):
        table = e.u32(A[name])
        out[name] = (L.lab(table), ppc_index_entries(e, table, min(max(n, 0), 4)) if table else [])
    return out

# ======================================================================= host side


class HostIndex(C.Structure):
    _fields_ = [("count", C.c_uint32), ("names", C.POINTER(C.c_void_p)), ("text", C.c_void_p)]


class HostState:
    pass


def host_entry(lib, ent):
    lab = F(lib, "tpw_cl_lab", I32, VP)
    count = ent.count
    names = C.cast(ent.names, C.c_void_p).value
    text = ent.text
    texts, offsets = [], []
    if names:
        for k in range(min(count, 64)):
            a = ent.names[k] or 0
            texts.append(C.string_at(a).decode("latin-1")[:300] if a else None)
            offsets.append(a - text if text and text <= a < text + 0x10000 else -1)
    return dict(count=count, names=lab(names), text=lab(text), texts=texts, offsets=offsets)


def host_begin(lib, c):
    h = HostState()
    h.keep = []
    addr = lambda n: F(lib, "tpw_cfg_addr", VP, C.c_char_p)(n.encode())   # noqa: E731
    fixed = {LABELS["gPlayerSettings"]: addr("gPlayerSettings"), LABELS["gResolutionBalance"]: addr("gResolutionBalance"),
             LABELS["gTPGlobals"]: addr("gTPGlobals"), LABELS["search"]: F(lib, "tpw_cfg_search_path", VP)(),
             LABELS["gFilePathBuffer"]: addr("gFilePathBuffer")}
    F(lib, "tpw_cfg_clear_files", None)()
    for k, (name, data) in enumerate(c.get("files", [])):
        blob = bytes(data)
        F(lib, "tpw_cfg_set_file", None, I32, C.c_char_p, C.c_char_p, I32)(k, name.encode("latin-1"), blob, len(blob))
        fixed[FILE_LABEL + k] = F(lib, "tpw_cfg_file_handle", VP, I32)(k)
    g = c.get("g", {})
    for name in SCALARS:
        C.c_uint32.from_address(addr(name)).value = g.get(name, 0) & 0xFFFFFFFF
    F(lib, "tpw_cfg_set_settings", None, VP)((I32 * 6)(*[T.s32(v) for v in c.get("settings", [0] * 6)]))
    F(lib, "tpw_cfg_set_resolution", None, I32)(T.s32(c.get("resolution", 0)))
    F(lib, "tpw_cfg_set_search_path", None, C.c_char_p)(c.get("search", "").encode("latin-1"))
    header = c.get("sin_header", (0, 0, 0))
    (C.c_uint32 * 3).from_address(F(lib, "tpw_cfg_sin_header", VP)())[:] = list(header)
    (C.c_uint32 * 0x1000).from_address(F(lib, "tpw_cfg_sin_table", VP)())[:] = [c.get("sin_fill", 0)] * 0x1000
    tex = c.get("texfolder", "textures").encode("latin-1")[:15].ljust(16, b"\0")
    C.memmove(addr("gThemeTextureFolder"), tex, 16)
    buf = c.get("pathbuf", "").encode("latin-1") + b"\0"
    C.memmove(addr("gFilePathBuffer"), buf, len(buf))
    paths = c.get("paths")
    h.paths = []
    if paths is None:
        C.c_void_p.from_address(addr("gPaths")).value = None
    else:
        strings = [C.create_string_buffer(p.encode("latin-1")) if p is not None else None for p in paths]
        arr = (C.c_void_p * max(len(paths), 1))(*[C.addressof(s) if s is not None else None for s in strings])
        h.keep += [s for s in strings if s is not None] + [arr]
        fixed[LABELS["paths"]] = C.addressof(arr)
        for k, s in enumerate(strings):
            if s is not None:
                fixed[PATH_LABEL + k] = C.addressof(s)
            h.paths.append(C.addressof(s) if s is not None else None)
        C.c_void_p.from_address(addr("gPaths")).value = C.addressof(arr)
    C.c_void_p.from_address(addr("gPathString")).value = h.paths[0] if h.paths and c.get("pathstring", True) else None
    indexes = c.get("indexes")
    fill = F(lib, "tpw_cfg_fill_index", None, VP, C.c_char_p, I32, I32, VP)
    for t, (name, label) in enumerate((("gQuickloadIndexes", "folder_table"), ("gQuickloadTextureIndexes", "texture_table"))):
        if indexes is None:
            C.c_void_p.from_address(addr(name)).value = None
            continue
        table = (HostIndex * max(len(indexes), 1))()
        h.keep.append(table)
        fixed[LABELS[label]] = C.addressof(table)
        for p, pair in enumerate(indexes):
            names = pair[t] or []
            blob = packed(names)
            blocks = (C.c_void_p * 2)()
            fill(C.addressof(table[p]), blob, len(blob), len(names), blocks)
            if names:
                fixed[ENTRY_LABEL + 4 * p + 2 * t] = blocks[0]
                fixed[ENTRY_LABEL + 4 * p + 2 * t + 1] = blocks[1]
        C.c_void_p.from_address(addr(name)).value = C.addressof(table)
    CL.host_begin(lib, fixed, c.get("q", {}))
    h.lib = lib
    return h


def host_observe(lib, c):
    addr = lambda n: F(lib, "tpw_cfg_addr", VP, C.c_char_p)(n.encode())   # noqa: E731
    lab = F(lib, "tpw_cl_lab", I32, VP)
    out = {name: C.c_uint32.from_address(addr(name)).value for name in SCALARS}
    out["sin_header"] = list((C.c_uint32 * 3).from_address(F(lib, "tpw_cfg_sin_header", VP)()))
    if c.get("observe_sin"):
        words = (C.c_uint32 * 0x1000).from_address(F(lib, "tpw_cfg_sin_table", VP)())
        out["sin"] = struct.pack(">4096I", *words)
    paths = C.c_void_p.from_address(addr("gPaths")).value
    out["paths"] = lab(paths)
    out["path_entries"] = ([lab(C.c_void_p.from_address(paths + 8 * k).value) for k in range(c.get("path_entries", 1))]
                           if paths else [])
    ps = C.c_void_p.from_address(addr("gPathString")).value
    out["pathstring"] = (lab(ps), C.string_at(ps).decode("latin-1")[:300] if ps else None)
    out["pathbuf"] = C.string_at(addr("gFilePathBuffer")).decode("latin-1")[:0x400]
    n = c.get("g", {}).get("gQuickloadPathCount", 0)
    for name in ("gQuickloadIndexes", "gQuickloadTextureIndexes"):
        table = C.c_void_p.from_address(addr(name)).value
        entries = []
        if table:
            arr = (HostIndex * 4).from_address(table)
            entries = [host_entry(lib, arr[p]) for p in range(min(max(n, 0), 4))]
        out[name] = (lab(table), entries)
    return out

# ======================================================================= running a case


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = build_stubs(runner.emu)
    HOST["index_size"] = F(runner.lib, "tpw_cfg_index_size", I32)()
    assert HOST["index_size"] == C.sizeof(HostIndex)


class CfgTest(FnTest):
    def run_case(self, runner, case):
        mism = super().run_case(runner, case)
        if mism and any(m.startswith("log") for m in mism) and getattr(self, "_last", None):
            p, h = self._last
            return [f"log: {CL.first_difference(p['log'], h['log'])}"] + [m for m in mism if not m.startswith("log")][:3]
        return mism


def fn_test(name, args=None, result="int", edge=None, rnd=None, post=None):
    """args(c, side) -> argument values; side is ("ppc", e) or ("host", lib, h).
    result: "int", "label" (a pointer, compared by label) or None (void).
    post(c, side) -> an extra observation after the call."""
    t = CfgTest(name, None, None, edge=edge, rnd=rnd)

    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            ppc_begin(e, c)
            a = args(c, ("ppc", e)) if args else []
            r3 = T.call_ppc(e, F_[name], [T.u32(v) for v in a])[0]
            out = ppc_observe(e, c)
            out["log"] = e.cl.log
            out["post"] = post(c, ("ppc", e)) if post else None
            out["result"] = (T.s32(r3) if result == "int" else e.cl.lab(r3) if result == "label" else None)
        finally:
            CL.Stubs.deactivate(e, saved)
            e.cl = None
        t._p = out
        return out

    def host(lib, c):
        h = host_begin(lib, c)
        a = args(c, ("host", lib, h)) if args else []
        vals = (I64 * 8)(*([int(v) for v in a] + [0] * (8 - len(a))))
        res = F(lib, "tpw_cfg_call", I32, C.c_char_p, VP)(name.encode(), vals)
        log = CL.host_end(lib)
        out = host_observe(lib, c)
        out["log"] = log
        out["post"] = post(c, ("host", lib, h)) if post else None
        out["result"] = res if result in ("int", "label") else None
        t._last = (t._p, out)
        return out

    t.ppc, t.host = ppc, host
    return t

# ======================================================================= arguments


def text_arg(side, text, label):
    """A C string argument (NULL for None) with a fixed label."""
    if text is None:
        return 0
    if side[0] == "ppc":
        e = side[1]
        a = CL.new_text(e, text)
        e.cl.labels[a] = label
        return a
    lib, h = side[1], side[2]
    buf = C.create_string_buffer(text.encode("latin-1"), max(len(text.encode("latin-1")) + 1, 0x400))
    h.keep.append(buf)
    lib.tpw_cl_label(label, C.addressof(buf))
    return C.addressof(buf)


def slot_arg(side, present, value=0x5A5A5A5A):
    """An int32 slot (FindFilePath's result) or NULL."""
    if not present:
        return 0
    if side[0] == "ppc":
        e = side[1]
        a = e.alloc(4)
        w32(e, a, value)
        e.cl.labels[a] = LABELS["result_slot"]
        e.cfg.slot = a
        return a
    lib, h = side[1], side[2]
    slot = C.c_int32(T.s32(value))
    h.keep.append(slot)
    h.slot = slot
    lib.tpw_cl_label(LABELS["result_slot"], C.addressof(slot))
    return C.addressof(slot)


def slot_value(c, side):
    if side[0] == "ppc":
        e = side[1]
        return T.s32(e.u32(e.cfg.slot)) if getattr(e.cfg, "slot", None) else None
    h = side[2]
    return h.slot.value if getattr(h, "slot", None) is not None else None

# ======================================================================= case generators


NAME_POOL = ["Coaster.wad", "coaster.WAD", "track01.md2", "Tex.tga", "tex", "Bouncy.bmp", "a.b.c", "x.y:z", "ride",
             "RIDE", "Big Wheel.wad", "#hash", "semi;colon", "dot.", ".hidden", "folder:file.wad", "old.:file",
             "dir.:a.:b", "Old.:File"]


def rnd_names(r, n=None):
    n = r.randint(0, 6) if n is None else n
    return [r.choice(NAME_POOL) if r.random() < 0.7 else
            "".join(r.choice("abcXYZ019._-:#") for _ in range(r.randint(1, 12))) for _ in range(n)]


def rnd_indexes(r, npaths):
    if r.random() < 0.1:
        return None
    return [(rnd_names(r) if r.random() < 0.85 else None, rnd_names(r) if r.random() < 0.85 else None)
            for _ in range(npaths)]


def base(r, **extra):
    npaths = r.choice([1, 1, 1, 2, 3])
    c = dict(g=dict(gTPWFlags=r.getrandbits(32), gGameOptions=r.getrandbits(32), gTPDialogOptions=r.getrandbits(32),
                    gTPDialogTextureMegabytes=r.getrandbits(32), gScreenWidth=r.choice([640, 800, 0, -5, 0x12345]),
                    gScreenHeight=r.choice([480, 600, 0, -7, 0x54321]), gViewCentreX=r.getrandbits(32),
                    gViewCentreY=r.getrandbits(32), gScreenCentreX=r.getrandbits(32), gScreenCentreY=r.getrandbits(32),
                    gPathLength=r.getrandbits(32), gQuickloadInitialised=r.choice([0, 1, 7]),
                    gQuickloadEnabled=r.choice([0, 1, 1, 9]), gQuickloadPathCount=npaths),
             settings=[r.choice([0, 1, 2, 3, 4, -1]), r.getrandbits(32), r.getrandbits(32), r.randint(-2, 9),
                       r.getrandbits(32), r.choice([0, 1, 1, 2, -1])],
             resolution=r.choice([0, 0, 1, 2, 3, 4, 5, 6, 7, 8, -1]), search=r.choice(["", "", "HD:TPW", "x" * 40]),
             sin_header=(r.getrandbits(32), r.getrandbits(32), r.getrandbits(32)), sin_fill=r.getrandbits(32),
             texfolder=r.choice(["textures", "stexture"]), pathbuf=r.choice(["", "old"]),
             paths=[r.choice(["", "HD:Games:TPW", "CD"]) for _ in range(npaths)], pathstring=r.random() < 0.8,
             path_entries=npaths, q={}, files=[])
    c["indexes"] = rnd_indexes(r, npaths)
    if c["indexes"] is None:
        c["g"]["gQuickloadEnabled"] = 0       # no tables: nothing may read them
    c.update(extra)
    return c


def gen(make, n=40, seed=5):
    def edge():
        r = random.Random(seed)
        return [make(r) for _ in range(n)]
    return edge, make


TESTS = []


def add(name, gen_fn, args=None, result="int", post=None):
    edge, rnd = gen(gen_fn)
    TESTS.append(fn_test(name, args, result, edge, rnd, post))


# ---- start-up stages
def g_stage1(r):
    return base(r)


SIZE_BITS = (0x20000000, 0x40000000, 0x80000000, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100)


def g_stage3(r):
    """One of the four ways to the screen size: the custom options' bits, Resolution.sam,
    a Resolution.sam value the game does not know (the old size stays, so the centre comes
    from whatever is there, converted as unsigned) and the player's choice."""
    c = base(r)
    mode = r.choice(["custom", "custom", "table", "unknown", "player"])
    if mode == "custom":
        c["settings"][5] = 1
        kind = r.random()
        if kind < 0.5:       # one size bit (or none), in a window or full screen
            c["g"]["gGameOptions"] = r.choice(SIZE_BITS + (0,)) | r.choice([0, 1])
        elif kind < 0.85:    # several: the first in the original's order wins
            opts = 0
            for bit in SIZE_BITS + (0x1,):
                if r.random() < 0.25:
                    opts |= bit
            c["g"]["gGameOptions"] = opts
        else:
            c["g"]["gGameOptions"] = r.getrandbits(32)
    else:
        c["settings"][5] = r.choice([0, 0, 2, -1])
        if mode == "table":
            c["resolution"] = r.randint(1, 7)
        elif mode == "unknown":
            c["resolution"] = r.choice([8, 9, 100, 0x7FFFFFFF])
            c["g"]["gScreenWidth"] = r.choice([-5, -1, 0x80000001, 640, 0])
            c["g"]["gScreenHeight"] = r.choice([-7, -1, 0x80000001, 480, 0])
        else:
            c["resolution"] = r.choice([0, -1, -100])
            c["settings"][0] = r.choice([0, 1, 2, 3, 4, -1, 7])
    c["path_entries"] = 1
    return c


def g_sin(r):
    c = base(r)
    c["observe_sin"] = True
    c["size"] = r.choice([0, 1, 2, 3, 4, 7, 8, 16, 100, 256, 1000, 0x800, 0x1000, r.randint(0, 0x1000)])
    return c


def g_close(r):
    c = base(r, observe_sin=True)
    if r.random() < 0.2:
        c["paths"] = None
    return c


add("InitialiseGameStage1", g_stage1, result=None)
add("InitialiseGameStage2", lambda r: base(r, observe_sin=True), result=None)
add("InitialiseGameStage3", g_stage3, result=None)
add("ClosedownGame", g_close, result=None)
add("TPSaveRegistry", g_stage1, result=None)
add("BuildGeometryTables", g_sin, lambda c, s: [c["size"]], result=None)


# ---- file lookup
def lookup_name(r, c):
    """A name to look up: one of the indexed names (with its case changed or a folder in
    front) or another."""
    pool = [n for pair in (c.get("indexes") or []) for names in pair if names for n in names]
    if pool and r.random() < 0.6:
        n = r.choice(pool)
        n = n.upper() if r.random() < 0.3 else n
    else:
        n = r.choice(NAME_POOL)
    if r.random() < 0.4:
        n = r.choice(["HD:", "Data:Rides:", ":"]) + n
    return n


def g_lookup(r):
    c = base(r)
    c["name"] = lookup_name(r, c)
    c["flags"] = r.getrandbits(4)
    c["slot"] = r.random() < 0.8
    c["q"] = {"GetFileAccessIndex": [r.randrange(c["g"]["gQuickloadPathCount"])],
              "CheckFileExists": [r.choice([0, 0, 1, 2])]}
    return c


add("DoesFileExist", g_lookup, lambda c, s: [text_arg(s, c["name"], LABELS["name_arg"]), c["flags"]])


def g_find(r):
    c = g_lookup(r)
    if r.random() < 0.05:
        c["name"] = None
    return c


add("FindFilePath", g_find,
    lambda c, s: [text_arg(s, c["name"], LABELS["name_arg"]), c["flags"], slot_arg(s, c["slot"])],
    result="label", post=slot_value)


def g_index_lookup(r):
    c = g_lookup(r)
    c["textures"] = r.choice([0, 1, 8, -1])
    c["path"] = r.randrange(c["g"]["gQuickloadPathCount"])
    return c


add("DoesQuickloadFilenameExist", g_index_lookup,
    lambda c, s: [text_arg(s, c["name"], LABELS["name_arg"]), c["textures"], c["path"]])
add("FindQuickloadFilename", g_index_lookup,
    lambda c, s: [text_arg(s, c["name"], LABELS["name_arg"]), c["textures"]])


def g_indexed(r):
    c = g_index_lookup(r)
    if r.random() < 0.5:
        base_name = c["name"].rsplit(".", 1)[0] if "." in c["name"] else c["name"]
        c["name"] = base_name + r.choice([".tga", ".wad", ".", "", ".x:y", ".:x", ".:"])
    return c


add("CheckIndexedFileExists", g_indexed,
    lambda c, s: [text_arg(s, c["name"], LABELS["name_arg"]), c["path"], c["textures"]])


# ---- the indexes
def g_init_quick(r):
    c = base(r)
    c["g"]["gQuickloadPathCount"] = r.choice([0, 1, 1, 2, 3])
    c["q"] = {"MReleaseAllocF": [r.choice([1, 1, 1, 0]), r.choice([1, 1, 0])]}
    return c


def index_text(r):
    """A qickload.txt: names separated by white space (and the odd control character)."""
    parts = []
    for _ in range(r.randint(0, 8)):
        parts.append(r.choice(NAME_POOL + ["x" * r.randint(1, 300)]) if r.random() < 0.9 else "\x01\x7f\xe9")
        parts.append(r.choice([" ", "\n", "\r\n", "\t", "  \n ", "\x00", ""]))
    if r.random() < 0.3:
        parts.insert(0, r.choice([" ", "\n\n", "\t "]))
    return "".join(parts).encode("latin-1")


add("InitialiseQuickloading", g_init_quick, result=None)


def g_load_index(r):
    c = base(r)
    c["data"] = index_text(r)
    c["size"] = len(c["data"]) if r.random() < 0.85 else r.randint(0, len(c["data"]))
    c["q"] = {"IsWhiteSpace": r.choice([[0], [0], [1, 0], [0, 0, 1, 0]]),
              "GetString": r.choice([[0], [0], [1, 0], [0, 1, 0], [0, 0, 1, 1, 0]])}
    c["index_init"] = [r.getrandbits(32) for _ in range(3)]
    return c


def load_index_args(c, side):
    data = c["data"]
    if side[0] == "ppc":
        e = side[1]
        idx = e.alloc(12, struct.pack(">III", *c["index_init"]))
        e.cl.labels[idx] = LABELS["index_arg"]
        blob = e.alloc(max(len(data), 1), data + b"\0" * (max(len(data), 1) - len(data)))
        e.cl.labels[blob] = LABELS["data_arg"]
        e.cfg.index = idx
        return [idx, blob, c["size"]]
    lib, h = side[1], side[2]
    idx = HostIndex()
    h.keep.append(idx)
    h.index = idx
    lib.tpw_cl_label(LABELS["index_arg"], C.addressof(idx))
    blob = C.create_string_buffer(data, max(len(data), 1))
    h.keep.append(blob)
    lib.tpw_cl_label(LABELS["data_arg"], C.addressof(blob))
    return [C.addressof(idx), C.addressof(blob), c["size"]]


def load_index_post(c, side):
    if side[0] == "ppc":
        e = side[1]
        return ppc_entry(e, e.cfg.index)
    return host_entry(side[1], side[2].index)


add("LoadQuickloadIndex", g_load_index, load_index_args, result=None, post=load_index_post)


def quick_files(r, c, folder, path):
    """Files for LoadQuickloadIndexes / SetupDirectoryForMeshLoad, and an allocation queue
    that may fail a file's data block (never an index block)."""
    root = c["paths"][path]
    names = [f"{root}:{folder}:qickload.txt", f"{root}:{folder}:{c['texfolder']}:qickload.txt"]
    files, q = [], []
    for k, name in enumerate(names):
        if r.random() < 0.75:
            data = index_text(r)
            files.append((name if r.random() < 0.95 else name.upper(), data))
            fail = r.random() < 0.15
            q.append(0 if fail else 1)
            if not fail and data:
                q += [1, 1]
    return files, q + [1]


def g_load_indexes(r):
    c = base(r)
    c["folder"] = r.choice(["Rides", "Rides:Coaster", "LOBBY", "x"])
    c["path"] = r.randrange(c["g"]["gQuickloadPathCount"])
    c["files"], c["q"]["MReleaseAllocF"] = quick_files(r, c, c["folder"], c["path"])
    if c["indexes"] is None:
        if r.random() < 0.5:
            # no tables: the first path's (NULL) entries are skipped, so no files may load
            c["path"], c["files"] = 0, []
        else:
            c["indexes"] = [(None, None)] * c["g"]["gQuickloadPathCount"]
    c["q"]["IsWhiteSpace"] = [0]
    return c


add("LoadQuickloadIndexes", g_load_indexes, lambda c, s: [text_arg(s, c["folder"], LABELS["folder_arg"]), c["path"]],
    result=None)
add("FreeQuickloadIndexes", base, result=None)
add("CloseQuickloading", base, result=None)


def g_setup(r):
    c = base(r)
    c["folder"] = r.choice(["Rides", "Rides:", "Rides:Coaster", "LOBBY:", None])
    c["file"] = r.choice(["coaster.wad", "Lobby.wad", "a", "x.md2"])
    c["shared"] = r.choice(["sharetex", "ssharete", ""])
    found = r.random() < 0.75 and c["folder"] is not None
    c["q"]["CheckFileExists"] = [1 if found else 0]
    if found and r.random() < 0.1:
        # found in a NULL search path: no root, so no mesh paths (and no index loading)
        c["paths"][0] = None
        c["g"]["gQuickloadInitialised"] = 0
    elif found:
        c["files"], c["q"]["MReleaseAllocF"] = quick_files(r, c, c["folder"], 0)
    if c["indexes"] is None:
        c["indexes"] = [(None, None)] * c["g"]["gQuickloadPathCount"]
    c["q"]["IsWhiteSpace"] = [0]
    return c


add("SetupDirectoryForMeshLoad", g_setup,
    lambda c, s: [text_arg(s, c["file"], LABELS["file_arg"]), text_arg(s, c["folder"], LABELS["folder_arg"]),
                  text_arg(s, c["shared"], LABELS["shared_arg"])])


# ---- the static initialiser
TEXTS = [0x102C1BE4 + 0x14 * k for k in range(16)] + [0x102C1BC4, 0x102C1BA4]


def sinit_ppc(e, c):
    saved = STUBS.activate(e)
    try:
        e.cl = CL.PpcLog({})
        regs = []
        e.stubs["__register_global_object"] = lambda em: regs.append((em.r(3), em.r(4)))
        for a in TEXTS:
            e.write(a, b"\0" * 0x14)
        T.call_ppc(e, SINIT, [])
        texts = [CL.tds_text(e, a) for a in TEXTS]
        tds_dtor = e.u32(e.toc - 0x7C1C)      # the TVector of TbDynamicStringTemplate<char>'s destructor
    finally:
        CL.Stubs.deactivate(e, saved)
        e.cl = None
    return dict(texts=texts, registrations_ok=regs == [(a, tds_dtor) for a in TEXTS])


def sinit_host(lib, c):
    out = (C.c_char * (96 * 18))()
    F(lib, "tpw_cfg_sinit_texts", None, VP)(out)
    return dict(texts=[out.raw[96 * i:96 * (i + 1)].split(b"\0")[0].decode("latin-1") for i in range(18)],
                registrations_ok=True)


TESTS.append(FnTest("__sinit_TPConfig_cpp", sinit_ppc, sinit_host, edge=lambda: [{}], rnd=lambda r: {}))
