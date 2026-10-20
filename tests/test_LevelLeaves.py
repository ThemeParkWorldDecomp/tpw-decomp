"""The leaves under LevelInit2 and LevelDeInit (unit w112a, main app; CHField::ProcTextureRange from the engine): the cell system's
four mesh loaders (LLCM_LoadQMeshes, LLCM_LoadKTrackMeshes, LLCM_LoadWTrackMeshes, LLCM_LoadHoardingMeshes) and their four
destroyers, LLCM_SetIllegalPlacement, LLCC_SystemResetClickStack, LLCC_ResetBuyLandList, CreateExtrudeFaceStorage,
SetupObjectInstanceTable, DMLoadTextures, DMFreeTextures and the height field's ProcTextureRange.

Every callee outside the 16 functions is a fake on both sides that records the same event (the directory set-up, LoadRideType,
GetUniqueID, get_theme_path, DestroyRideInstance, the allocator). The functions run as the original on the PowerPC side and as src/
on the host. Every global and every entry of the four mesh tables is filled with random words before a call and compared after
(entry by entry: the type word, the name as an index into one pool of strings both sides share, and the four bytes at +8), so a
missing, extra or wrong-width store shows. The first test dumps the initialised data (the four tables, the two size pairs and
the two static texture sets with their entries) before anything changes it, so the host's data matches the image.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LevelLeaves"
MAIN = "SimTheme Park"
ENGINE = "engine symbols shared"

# Entry points (main app unless noted).
LOAD_Q, LOAD_K, LOAD_W, LOAD_H = 0x10081A98, 0x10081DD0, 0x10082174, 0x1008255C
DESTROY_Q, DESTROY_K, DESTROY_W, DESTROY_H = 0x10081BC8, 0x10081F18, 0x100822BC, 0x10082688
SET_ILLEGAL, RESET_CLICKS, RESET_BUY = 0x10088934, 0x10091DD4, 0x10096190
EXTRUDE, OBJECT_TABLE, DM_LOAD, DM_FREE = 0x1004E60C, 0x101EF1B0, 0x1003013C, 0x10030178
PROC_RANGE = 0x1000DB80            # engine
OWN = {LOAD_Q, LOAD_K, LOAD_W, LOAD_H, DESTROY_Q, DESTROY_K, DESTROY_W, DESTROY_H, SET_ILLEGAL, RESET_CLICKS, RESET_BUY,
       EXTRUDE, OBJECT_TABLE, DM_LOAD, DM_FREE}
OPS = {0: LOAD_Q, 1: LOAD_K, 2: LOAD_W, 3: LOAD_H, 4: DESTROY_Q, 5: DESTROY_K, 6: DESTROY_W, 7: DESTROY_H, 8: SET_ILLEGAL,
       9: RESET_CLICKS, 10: RESET_BUY, 11: EXTRUDE, 12: OBJECT_TABLE, 13: DM_LOAD, 14: DM_FREE}
# The callees (addresses in the main program).
SETUP_DIR, LOAD_RIDE_TYPE, GET_UNIQUE_ID, GET_THEME_PATH, DESTROY_INSTANCE, ALLOC = (
    0x100680FC, 0x10068FD0, 0x1006FC5C, 0x1015139C, 0x1006A3EC, 0x10064BD4)

TABLES = [(0x1029DB48, 8), (0x1029DBA8, 20), (0x1029DC98, 25), (0x1029DDC4, 28)]
SENTINEL = 0x6A5A0010
BLOCK_LABEL = 20                   # labels 20 to 23: the allocator's blocks
Q_PAIR_LABEL, TRACK_PAIR_LABEL = 1, 2
OBJ_LABEL = 6                      # labels 6 to 8: the three counters of SetupObjectInstanceTable

# Global -> (PowerPC address, words, width in bytes, kind). kind: w words, p pointers (4 bytes there, 8 on the host).
STATE = {
    "gLLCMQSize": (0x102DCEA8, 2, 4, "w"), "gLLCMTrackSize": (0x102DC220, 2, 4, "w"),
    "gLLCMIllegalPlacement": (0x102DC218, 1, 4, "w"), "gLLCCClickStackCount": (0x102E47C0, 1, 4, "w"),
    "gExtrudeFaceStoreCount": (0x102B74B8, 1, 4, "w"), "gExtrudeFaceStoreB": (0x102B74B4, 1, 4, "p"),
    "gExtrudeFaceStoreA": (0x102B74B0, 1, 4, "p"),
}

# The pool of names the tables can point at (indices shared by both sides). The theme texts and the shapes of names below.
POOL = ["", "-", "-x", "a.md2", "n" * 40 + ".md2", "quedead.md2", "gk_trckQ.md2", "ho_fos1.md2", "wr_trckQ.md2", "x-", "--",
        "gk_trckH_U.md2", "Q", "Data:Generic:Dynamic:Textures"]
EMPTY, DASH, DASH_X, NORMAL_FIRST = 0, 1, 2, 3
NORMAL = [3, 4, 5, 6, 7, 8, 9, 11, 12]
DASHES = [1, 2, 10]


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & 0xFFFFFFFF))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ------------------------------------------------------------------ state on both sides


def ppc_get(e, names, lab):
    out = {}
    for name in names:
        addr, n, width, kind = STATE[name]
        values = []
        for i in range(n):
            word = int.from_bytes(e.read(addr + i * width, width), "big")
            values.append(lab(word) if kind == "p" else word)
        out[name] = values
    return out


def ppc_set(e, pre, tokens):
    for name, values in pre.items():
        addr, n, width, kind = STATE[name]
        for i, v in enumerate(values):
            if isinstance(v, str):
                v = tokens[v]
            e.write(addr + i * width, (v & ((1 << (8 * width)) - 1)).to_bytes(width, "big"))


def host_addr(lib, name):
    fn = lib.tpw_ll_addr
    fn.restype, fn.argtypes = C.c_void_p, [C.c_char_p]
    return fn(name.encode())


def host_set(lib, pre, tokens):
    for name, values in pre.items():
        _, n, width, kind = STATE[name]
        a = host_addr(lib, name)
        hw = 8 if kind == "p" else width
        for i, v in enumerate(values):
            if isinstance(v, str):
                v = tokens[v]
            C.memmove(a + i * hw, (v & ((1 << (8 * hw)) - 1)).to_bytes(hw, "little"), hw)


def host_get(lib, names):
    lab = lib.tpw_cl_lab
    lab.restype, lab.argtypes = C.c_int32, [C.c_void_p]
    out = {}
    for name in names:
        _, n, width, kind = STATE[name]
        a = host_addr(lib, name)
        hw = 8 if kind == "p" else width
        values = []
        for i in range(n):
            word = int.from_bytes(C.string_at(a + i * hw, hw), "little")
            values.append(lab(word or None) if kind == "p" else word)
        out[name] = values
    return out


def prefill(r, **fixed):
    """Random words (or a sentinel pointer or one of the allocator's blocks) for every global the case does not fix."""
    pre = {}
    for name, (_, n, width, kind) in STATE.items():
        if name in fixed:
            pre[name] = list(fixed[name])
        elif kind == "p":
            pre[name] = [r.choice(["blk0", "blk1", "blk2", "blk3", SENTINEL, SENTINEL + 0x10]) for _ in range(n)]
        else:
            pre[name] = [r.getrandbits(8 * width) for _ in range(n)]
    return pre


# The four tables: a list of entries (type, name index or -1, b8, b9, b10, b11) per table.


def rnd_name(r):
    x = r.random()
    if x < 0.2:
        return -1
    if x < 0.35:
        return r.choice(DASHES)
    if x < 0.4:
        return EMPTY
    return r.choice(NORMAL)


def rnd_entry(r, name=None):
    return (r.getrandbits(32), rnd_name(r) if name is None else name, r.getrandbits(8), r.getrandbits(8), r.getrandbits(8),
            r.getrandbits(8))


def rnd_tables(r, names=None, table=None):
    """Random entries everywhere; `names` (a list) fixes the names of table `table`."""
    out = [[rnd_entry(r) for _ in range(n)] for _, n in TABLES]
    if names is not None:
        out[table] = [rnd_entry(r, name) for name in names]
    return out


def ppc_set_tables(e, tables, pool):
    for (base, n), entries in zip(TABLES, tables):
        for i, (typ, name, b8, b9, b10, b11) in enumerate(entries):
            a = base + 12 * i
            w32(e, a, typ)
            w32(e, a + 4, 0 if name < 0 else pool[name])
            e.write(a + 8, bytes([b8, b9, b10, b11]))


def ppc_get_tables(e, pool):
    by_address = {a: i for i, a in enumerate(pool)}
    out = []
    for base, n in TABLES:
        entries = []
        for i in range(n):
            a = base + 12 * i
            ptr = e.u32(a + 4)
            idx = -1 if ptr == 0 else by_address.get(ptr, -2)
            text = e.cstr(ptr)[:159] if idx == -2 else ""
            entries.append((e.u32(a), idx, text) + tuple(e.read(a + 8, 4)))
        out.append(entries)
    return out


def host_set_tables(lib, tables):
    fn = lib.tpw_ll_set_entry
    fn.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.c_int32, C.c_int32, C.c_int32]
    for t, entries in enumerate(tables):
        for i, (typ, name, b8, b9, b10, b11) in enumerate(entries):
            fn(t, i, typ, name, b8, b9, b10, b11)


def host_get_tables(lib):
    fn = lib.tpw_ll_get_entry
    fn.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int64), C.c_char_p]
    out = []
    for t, (_, n) in enumerate(TABLES):
        entries = []
        for i in range(n):
            vals = (C.c_int64 * 6)()
            text = C.create_string_buffer(160)
            fn(t, i, vals, text)
            shown = text.value.decode("latin-1") if vals[1] == -2 else ""
            entries.append((vals[0], vals[1], shown) + tuple(vals[2:6]))
        out.append(entries)
    return out


# ------------------------------------------------------------------ the case runner


class Ppc:
    """The PowerPC side of one case: the fakes, the pool, the blocks and the labels."""

    def __init__(self, e, c):
        self.e = e
        e.reset_scratch()
        self.c = c
        self.L = L = CL.PpcLog({}, c.get("q"))
        e.cl = L
        self.blocks = [e.alloc(64, b"\xa5" * 64) for _ in range(4)]
        for i, b in enumerate(self.blocks):
            L.labels[b] = BLOCK_LABEL + i
        L.labels[0x102DCEA8] = Q_PAIR_LABEL
        L.labels[0x102DC220] = TRACK_PAIR_LABEL
        self.tokens = {"blk%d" % i: b for i, b in enumerate(self.blocks)}
        self.theme = e.alloc(1200)
        e.write(self.theme, c.get("theme", "").encode("latin-1") + b"\0")
        self.pool = []
        for text in POOL:
            self.pool.append(e.alloc(len(text) + 1, text.encode("latin-1") + b"\0"))

    def install(self):
        e, L = self.e, self.L
        st = CL.Stubs()
        st.real = set(OWN)

        def tail(text):
            return text[-150:] if len(text) > 150 else text

        def setup_dir(e):
            file, folder, shared = e.r(3), e.cstr(e.r(4), limit=1 << 16), e.r(5)
            L.rec("SetupDirectoryForMeshLoad", [shared, len(folder), int(file != 0)],
                  [e.cstr(file)[:159] if file else "(null)", folder[:159], tail(folder)])
            e.set_r(3, T.u32(L.ret("setup", 1)))

        def load_ride_type(e):
            folder, name, a, b = e.cstr(e.r(3), limit=1 << 16), e.r(4), e.r(5), e.r(6)
            sign2, d, x = [e.u32(e.r(1) + 0x38 + 4 * i) for i in range(3)]
            L.rec("LoadRideType", [L.lab(a), L.lab(b), T.s32(e.u32(a)), T.s32(e.u32(a + 4)), e.r(7), e.r(8), e.r(9),
                                   int(e.r(10) != 0), int(sign2 != 0), T.s32(d), T.s32(x), len(folder)],
                  [e.cstr(name)[:159] if name else "(null)", folder[:159], tail(folder)])
            e.set_r(3, T.u32(L.ret("RideType", 7)))

        def unique_id(e):
            L.rec("GetUniqueID", [e.r(3), e.r(4)])
            e.set_r(3, (e.r(3) + e.r(4)) & 0xFFFFFFFF)

        def theme_path(e):
            L.rec("get_theme_path", [e.s32(a) for a in (0x102DCEA8, 0x102DCEAC, 0x102DC220, 0x102DC224)])
            e.set_r(3, self.theme)

        def alloc(e):
            L.rec("MReleaseAllocF", [e.r(3), e.r(4), e.u32(0x102B74B8), L.lab(e.u32(0x102B74B4)), L.lab(e.u32(0x102B74B0))])
            which = L.ret("alloc", 0)
            e.set_r(3, 0 if which < 0 else self.blocks[which & 3])

        st.internal(SETUP_DIR, setup_dir)
        st.internal(LOAD_RIDE_TYPE, load_ride_type)
        st.internal(GET_UNIQUE_ID, unique_id)
        st.internal(GET_THEME_PATH, theme_path)
        st.internal(DESTROY_INSTANCE, CL.recorder("DestroyRideInstance", "u"))
        st.internal(ALLOC, alloc)
        self.saved = st.activate(e)

    def finish(self):
        CL.Stubs.deactivate(self.e, self.saved)


def ppc_case(e, c):
    p = Ppc(e, c)
    op = c["op"]
    p.install()
    out = {}
    try:
        ppc_set(e, c["pre"], p.tokens)
        ppc_set_tables(e, c["tables"], p.pool)
        a, b = c.get("args", (0, 0))
        args = [T.u32(a), T.u32(b)] if op in (8, 11) else []
        if op == 12:
            obj = e.alloc(12, b"\0" * 12)
            for i, v in enumerate(c["obj"]):
                w32(e, obj + 4 * i, v)
                p.L.labels[obj + 4 * i] = OBJ_LABEL + i
            args = [obj, obj + 4, obj + 8]
        if op in (13, 14):
            ts = e.alloc(0x20, b"\0" * 0x20)
            flags, count, folder, entries, rest = c["set"]
            entries_addr = e.alloc(0x20, b"\xa5" * 0x20)
            w32(e, ts, flags)
            e.write(ts + 4, struct.pack(">H", count))
            w32(e, ts + 8, 0 if folder < 0 else p.pool[folder])
            w32(e, ts + 0x10, entries_addr if entries else 0)
            for off, v in zip((0x0C, 0x14, 0x18, 0x1C), rest):
                w32(e, ts + off, v)
            p.L.labels[ts] = 30
            args = [ts]
        r3 = T.call_ppc(e, OPS[op], args)[0]
        out["result"] = r3 if op in (13, 14) else 0
        out["state"] = ppc_get(e, STATE, p.L.lab)
        out["tables"] = ppc_get_tables(e, p.pool)
        if op == 12:
            out["obj"] = [e.u32(obj + 4 * i) for i in range(3)]
        if op in (13, 14):
            ptr = e.u32(ts + 8)
            eptr = e.u32(ts + 0x10)
            out["set"] = [e.u32(ts), struct.unpack(">H", e.read(ts + 4, 2))[0], -1 if ptr == 0 else (1 if ptr in p.pool else -2),
                          0 if eptr == 0 else (1 if eptr == entries_addr else -2)]
            out["rest"] = [e.u32(ts + off) for off in (0x0C, 0x14, 0x18, 0x1C)]
    finally:
        p.finish()
    out["log"] = p.L.log
    return out


def host_case(lib, c):
    op = c["op"]
    blocks = host_addr(lib, "gBlocks")
    fixed = {BLOCK_LABEL + i: blocks + 64 * i for i in range(4)}
    fixed[Q_PAIR_LABEL] = host_addr(lib, "gLLCMQSize")
    fixed[TRACK_PAIR_LABEL] = host_addr(lib, "gLLCMTrackSize")
    objs = host_addr(lib, "gObjWords")
    for i in range(3):
        fixed[OBJ_LABEL + i] = objs + 4 * i
    fixed[30] = host_addr(lib, "gTestSet")
    CL.host_begin(lib, fixed, c.get("q"))
    lib.tpw_ll_begin.argtypes = [C.c_char_p]
    lib.tpw_ll_begin(c.get("theme", "").encode("latin-1"))
    pool = lib.tpw_ll_set_pool
    pool.argtypes = [C.c_int32, C.c_char_p]
    for i, text in enumerate(POOL):
        pool(i, text.encode("latin-1"))
    tokens = {"blk%d" % i: blocks + 64 * i for i in range(4)}
    out = {}
    try:
        host_set(lib, c["pre"], tokens)
        host_set_tables(lib, c["tables"])
        a, b = c.get("args", (0, 0))
        if op == 12:
            lib.tpw_ll_set_obj.argtypes = [C.POINTER(C.c_uint32)]
            lib.tpw_ll_set_obj((C.c_uint32 * 3)(*c["obj"]))
        if op in (13, 14):
            flags, count, folder, entries, rest = c["set"]
            lib.tpw_ll_set_testset.argtypes = [C.c_uint32, C.c_int32, C.c_int32, C.c_int32]
            lib.tpw_ll_set_testset(flags, count, folder, entries)
        lib.tpw_ll_call.restype = C.c_int64
        lib.tpw_ll_call.argtypes = [C.c_int32, C.c_uint32, C.c_uint32]
        r = lib.tpw_ll_call(op, T.u32(a), T.u32(b))
        out["result"] = r if op == 13 else (1 if op == 14 else 0)    # DMFreeTextures answers 1 on the Mac (void here)
        out["state"] = host_get(lib, STATE)
        out["tables"] = host_get_tables(lib)
        if op == 12:
            words = (C.c_uint32 * 3)()
            lib.tpw_ll_get_obj.argtypes = [C.POINTER(C.c_uint32)]
            lib.tpw_ll_get_obj(words)
            out["obj"] = list(words)
        if op in (13, 14):
            dump = (C.c_int64 * 4)()
            lib.tpw_ll_get_testset.argtypes = [C.POINTER(C.c_int64)]
            lib.tpw_ll_get_testset(dump)
            out["set"] = list(dump)
            out["rest"] = [v & 0xFFFFFFFF for v in c["set"][4]]
    finally:
        out["log"] = CL.host_end(lib)
    return out


# ------------------------------------------------------------------ the initial data (the first test)


def ppc_image(e, c):
    e.reset_scratch()
    pool = []
    out = {"tables": ppc_get_tables(e, pool)}
    out["words"] = [e.u32(a) for a in (0x102DCEA8, 0x102DCEAC, 0x102DC220, 0x102DC224, 0x102DC218, 0x102E47C0, 0x102B74B0,
                                       0x102B74B4, 0x102B74B8)]
    sets = []
    for base in (0x1029CF58, 0x102A02FC):
        count = struct.unpack(">H", e.read(base + 4, 2))[0]
        folder, entries = e.u32(base + 8), e.u32(base + 0x10)
        header = [e.u32(base), count, e.cstr(folder)[:63] if folder else "(none)", int(entries != 0)]
        rows = []
        for i in range(count):
            a = entries + 0x10 * i
            rows.append((e.u32(a), e.cstr(e.u32(a + 4))[:31], e.u32(a + 8), e.u32(a + 12)))
        sets.append((header, rows))
    out["sets"] = sets
    return out


def host_image(lib, c):
    out = {"tables": host_get_tables(lib)}
    zero = [host_get(lib, [n])[n] for n in ("gLLCMQSize", "gLLCMTrackSize", "gLLCMIllegalPlacement", "gLLCCClickStackCount",
                                            "gExtrudeFaceStoreCount")]
    # words in the PowerPC order: the Q pair, the track pair, the illegal word, the click stack, the extruder's A, B, count
    out["words"] = [zero[0][0], zero[0][1], zero[1][0], zero[1][1], zero[2][0], zero[3][0],
                    int(host_get(lib, ["gExtrudeFaceStoreA"])["gExtrudeFaceStoreA"][0] != 0),
                    int(host_get(lib, ["gExtrudeFaceStoreB"])["gExtrudeFaceStoreB"][0] != 0), zero[4][0]]
    sets = []
    for which in (0, 1):
        vals = (C.c_int64 * (4 + 60))()
        names = C.create_string_buffer(64 + 20 * 32)
        lib.tpw_ll_dump_set.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.c_char_p]
        lib.tpw_ll_dump_set(which, vals, names)
        raw = names.raw
        count = vals[1]
        folder = raw[:64].split(b"\0")[0].decode("latin-1") if vals[2] else "(none)"
        header = [vals[0], count, folder, int(vals[3])]
        rows = []
        for i in range(count):
            nm = raw[64 + 32 * i:96 + 32 * i].split(b"\0")[0].decode("latin-1")
            rows.append((vals[4 + 3 * i], nm, vals[5 + 3 * i], vals[6 + 3 * i]))
        sets.append((header, rows))
    out["sets"] = sets
    return out


# ------------------------------------------------------------------ CHField::ProcTextureRange (engine)


def ppc_engine(e, c):
    e.reset_scratch()
    obj = e.alloc(0x250, b"\xa5" * 0x250)
    T.call_ppc(e, PROC_RANGE, [obj] + [T.u32(v) for v in c["args"]])
    return {"words": [e.u32(obj + off) for off in (0x234, 0x238, 0x23C, 0x240, 0x244, 0x248)]}


def host_engine(lib, c):
    lib.tpw_ll_proc_range.argtypes = [C.POINTER(C.c_int32), C.POINTER(C.c_uint32)]
    out = (C.c_uint32 * 6)()
    lib.tpw_ll_proc_range((C.c_int32 * 4)(*[T.s32(v) for v in c["args"]]), out)
    return {"words": list(out)}


class EngineTest(FnTest):
    """A test of a function of the engine's program: the runner is switched to its emulator for the case and back."""

    def run_case(self, runner, case):
        main = runner.program
        runner.use(ENGINE)
        try:
            return FnTest.run_case(self, runner, case)
        finally:
            runner.use(main)


# ------------------------------------------------------------------ cases

EDGE_WORDS = [0, 1, 2, 3, 4, 5, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0x80000001,
              0xFFFFFFFE, 0xFFFFFFFF]
THEMES = ["", "a", "data:levels:Jungle", "with space:and:colons", "100% %s %d", "x" * 100, "y" * 140, "z" * 1000,
          "data:levels:Halloween:" + "h" * 130]
SETUPS = [[1], [0] * 8, [0, 0, 1], [0, 1], [-1], [5], [0, 0, -3], [0, 0, 0, 0, 0, 0, 0, 0, 2], [0], [0x80000000, 0]]


def rnd_word(r):
    return r.choice(EDGE_WORDS) if r.random() < 0.5 else r.getrandbits(32)


def s32w(v):
    return T.s32(v & 0xFFFFFFFF)


def case(op, r, args=(0, 0), names=None, table=None, **extra):
    c = {"op": op, "args": tuple(args), "pre": prefill(r), "tables": rnd_tables(r, names, table), "q": {}, "theme": ""}
    c.update(extra)
    return c


def rnd_setup(r):
    if r.random() < 0.5:
        return list(r.choice(SETUPS))
    return [r.choice([0, 0, 1, -1, 5, 7]) for _ in range(r.randrange(1, 10))]


def rnd_types(r, n=40):
    return [rnd_word(r) for _ in range(n)]


def rnd_theme(r):
    if r.random() < 0.5:
        return r.choice(THEMES)
    return "".join(r.choice("abcdefghijklmnopqrstuvwxyz:._-% 09") for _ in range(r.randrange(0, 200)))


# --- the loaders
def q_names(r, kind=None):
    kind = kind if kind is not None else r.randrange(4)
    if kind == 0:
        return [5, 5, 3, 3, 3, 3, 3, 3]
    if kind == 1:
        return [r.choice(NORMAL + DASHES + [EMPTY]) for _ in range(8)]
    if kind == 2:
        return [-1 if r.random() < 0.3 else r.choice(NORMAL + DASHES + [EMPTY]) for _ in range(8)]
    return [r.choice([5, 3]) for _ in range(8)]


def q_case(r, names=None, setup=None, theme=None, b9=None):
    names = names if names is not None else q_names(r)
    c = case(0, r, names=names, table=0, theme=theme if theme is not None else rnd_theme(r),
             q={"setup": setup if setup is not None else rnd_setup(r), "RideType": rnd_types(r)})
    if b9 is not None:
        c["tables"][0] = [(t, n, r.getrandbits(8), b9[i % len(b9)], r.getrandbits(8), r.getrandbits(8))
                          for i, (t, n, _, _, _, _) in enumerate(c["tables"][0])]
    return c


def q_edges(r):
    yield q_case(r, names=[5, 5, 3, 3, 3, 3, 3, 3], setup=[1], theme="data:levels:Jungle", b9=[1, 1, 0, 0, 0, 0, 0, 0])
    for value in (0, 1, 2, 0x7F, 0x80, 0xFF):
        for i in range(8):
            flags = [0] * 8
            flags[i] = value
            yield q_case(r, setup=[1], theme="t", b9=flags)
    for setup in SETUPS:
        yield q_case(r, setup=setup)
    for theme in THEMES:
        yield q_case(r, theme=theme)
    for i in range(8):
        names = [3] * 8
        names[i] = -1
        yield q_case(r, names=names)
        names[i] = EMPTY
        yield q_case(r, names=names)
        names[i] = DASH
        yield q_case(r, names=names)
    yield q_case(r, names=[-1] * 8, setup=[0, 0, 1])


def track_names(r, count, kind=None, allow_null_first=True):
    kind = kind if kind is not None else r.randrange(5)
    pool = NORMAL + NORMAL + DASHES + [EMPTY, -1, -1, -1]
    if kind == 0:
        return [r.choice(pool) for _ in range(count)]
    if kind == 1:
        names = [r.choice(pool) for _ in range(count)]
        names[0] = r.choice(NORMAL)
        return names
    if kind == 2:
        names = [-1] * count
        names[0] = r.choice(NORMAL)
        return names
    if kind == 3:
        return [DASH] * count
    base = [NORMAL[i % len(NORMAL)] if i % 3 else -1 for i in range(count)]
    base[0] = DASH if r.random() < 0.5 else r.choice(NORMAL)
    return base


def track_case(op, r, count, names=None, setup=None, theme=None):
    table = op if op < 4 else op - 4
    names = names if names is not None else track_names(r, count)
    return case(op, r, names=names, table=table, theme=theme if theme is not None else rnd_theme(r),
                q={"setup": setup if setup is not None else rnd_setup(r), "RideType": rnd_types(r)})


def track_edges(op, count):
    def edges(r):
        yield track_case(op, r, count, names=[DASH] + [NORMAL[i % 9] if i % 3 else -1 for i in range(1, count)], setup=[1])
        for kind in range(5):
            yield track_case(op, r, count, names=track_names(r, count, kind))
        yield track_case(op, r, count, names=[DASH] * count)
        yield track_case(op, r, count, names=[DASH_X] * count)
        yield track_case(op, r, count, names=[EMPTY] * count)
        yield track_case(op, r, count, names=[-1] * count)
        yield track_case(op, r, count, names=[DASH, DASH_X, EMPTY] + [3] * (count - 3))
        yield track_case(op, r, count, names=[DASH, 3] + [-1] * (count - 2), setup=[0, 0, 1])
        yield track_case(op, r, count, names=[DASH, -1] + [3] * (count - 2))
        yield track_case(op, r, count, names=[4] + [-1] * (count - 1))
        for setup in SETUPS:
            yield track_case(op, r, count, setup=setup)
        for theme in THEMES:
            yield track_case(op, r, count, theme=theme)
        for i in range(count):
            names = [3] * count
            names[i] = -1
            yield track_case(op, r, count, names=names)
            names[i] = DASH
            yield track_case(op, r, count, names=names)
    return edges


def track_random(op, count):
    return lambda r: track_case(op, r, count)


# --- the destroyers
def destroy_case(op, r, count, names=None):
    table = op - 4
    names = names if names is not None else [rnd_name(r) for _ in range(count)]
    c = case(op, r, names=names, table=table)
    types = [r.choice([0, 1, 7, 0xFFFFFFFF, r.getrandbits(32)]) for _ in range(count)]
    c["tables"][table] = [(types[i], n, b8, b9, b10, b11) for i, (_, n, b8, b9, b10, b11) in enumerate(c["tables"][table])]
    return c


def destroy_edges(op, count):
    def edges(r):
        for kind in ([-1], [DASH], [3], [DASH_X], [EMPTY], [DASH, 3], [-1, DASH, 3, 10]):
            yield destroy_case(op, r, count, [kind[i % len(kind)] for i in range(count)])
        yield destroy_case(op, r, count, [DASH] + [3] * (count - 1))
        for i in range(count):
            names = [-1] * count
            names[i] = 3
            yield destroy_case(op, r, count, names)
            names[i] = DASH
            yield destroy_case(op, r, count, names)
    return edges


# --- the one-word leaves
def plain_edges(op, words=None):
    def edges(r):
        for v in (words or [0]):
            c = case(op, r, args=(v, 0))
            yield c
        for _ in range(4):
            yield case(op, r)
    return edges


def placement_edges(r):
    for v in (0, 1, 2, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000):
        yield case(8, r, args=(v, 0))


def placement_random(r):
    return case(8, r, args=(rnd_word(r), 0))


def clicks_edges(r):
    for v in (0, 1, 0x400, 0x401, 0xFFFFFFFF):
        yield click_case(r, v)


def click_case(r, v=None):
    c = case(9, r)
    if v is not None:
        c["pre"]["gLLCCClickStackCount"] = [v]
    return c


def click_random(r):
    return click_case(r)


# --- CreateExtrudeFaceStorage
EXTRUDE_WORDS = [0, 1, 2, 3, 0x0FFFFFFF, 0x10000000, 0x10000001, 0x24924924, 0x24924925, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF]


def extrude_case(r, a, b, alloc=None):
    alloc = alloc if alloc is not None else [r.choice([0, 1, 2, 3, -1]) for _ in range(2)]
    c = case(11, r, args=(a, b))
    c["q"] = {"alloc": alloc}
    return c


def extrude_edges(r):
    for a in EXTRUDE_WORDS:
        for b in EXTRUDE_WORDS:
            yield extrude_case(r, a, b, [0, 1])
    for alloc in ([-1, -1], [-1, 2], [2, -1], [3, 3], [1, 0]):
        yield extrude_case(r, 5, 7, alloc)
        yield extrude_case(r, 0, 0, alloc)


def extrude_random(r):
    return extrude_case(r, rnd_word(r), rnd_word(r))


# --- SetupObjectInstanceTable
def obj_case(r):
    c = case(12, r)
    c["obj"] = [rnd_word(r) for _ in range(3)]
    return c


def obj_edges(r):
    for obj in ([0, 0, 0], [1, 1, 1], [0xFFFFFFFF] * 3, [5, 6, 7], [0, 0xFFFFFFFF, 0], [0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFE]):
        c = case(12, r)
        c["obj"] = obj
        yield c


# --- DMLoadTextures and DMFreeTextures
def set_case(op, r, flags=None):
    c = case(op, r)
    c["set"] = (flags if flags is not None else rnd_word(r), r.choice([0, 4, 20, 0xFFFF, r.getrandbits(16)]),
                r.choice([-1, 13, 3]), r.choice([0, 1]), [r.getrandbits(32) for _ in range(4)])
    return c


def set_edges(op):
    def edges(r):
        for flags in (0, 1, 2, 3, 0x80000000, 0x80000001, 0xFFFFFFFE, 0xFFFFFFFF, 0xA5A5A5A5, 0x40000000, 0x7FFFFFFF):
            yield set_case(op, r, flags)
    return edges


def set_random(op):
    return lambda r: set_case(op, r)


# --- CHField::ProcTextureRange
RANGE_WORDS = [0, 1, 0xFFFFFFFF, 0x7F, 0x80, 0x80000000, 0x7FFFFFFF]


def range_edges():
    for args in ((1, 2, 3, 4), (0x11111111, 0x22222222, 0x33333333, 0x44444444), (0, 0, 0, 0), (0, 0, 0x7F, 0x7F),
                 (0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 1)):
        yield {"args": args}
    for a in RANGE_WORDS:
        for pos in range(4):
            args = [0x10, 0x20, 0x30, 0x40]
            args[pos] = a
            yield {"args": tuple(args)}


def range_random(r):
    return {"args": tuple(rnd_word(r) for _ in range(4))}


def image_edges():
    yield {}


def image_random(r):
    return {}


def make(name, op, edge_fn, rnd_fn, seed):
    return FnTest(name, ppc_case, host_case, edge=lambda: list(edge_fn(random.Random(seed))), rnd=rnd_fn)


TESTS = [
    FnTest("initial data", ppc_image, host_image, edge=image_edges, rnd=image_random),
    make("LLCM_LoadQMeshes", 0, q_edges, q_case, 201),
    make("LLCM_LoadKTrackMeshes", 1, track_edges(1, 20), track_random(1, 20), 202),
    make("LLCM_LoadWTrackMeshes", 2, track_edges(2, 25), track_random(2, 25), 203),
    make("LLCM_LoadHoardingMeshes", 3, track_edges(3, 28), track_random(3, 28), 204),
    make("LLCM_DestroyQMeshes", 4, destroy_edges(4, 8), lambda r: destroy_case(4, r, 8), 205),
    make("LLCM_DestroyKTrackMeshes", 5, destroy_edges(5, 20), lambda r: destroy_case(5, r, 20), 206),
    make("LLCM_DestroyWTrackMeshes", 6, destroy_edges(6, 25), lambda r: destroy_case(6, r, 25), 207),
    make("LLCM_DestroyHoardingMeshes", 7, destroy_edges(7, 28), lambda r: destroy_case(7, r, 28), 208),
    make("LLCM_SetIllegalPlacement", 8, placement_edges, placement_random, 209),
    make("LLCC_SystemResetClickStack", 9, clicks_edges, click_random, 210),
    make("LLCC_ResetBuyLandList", 10, plain_edges(10), lambda r: case(10, r), 211),
    make("CreateExtrudeFaceStorage", 11, extrude_edges, extrude_random, 212),
    make("SetupObjectInstanceTable", 12, obj_edges, obj_case, 213),
    make("DMLoadTextures", 13, set_edges(13), set_random(13), 214),
    make("DMFreeTextures", 14, set_edges(14), set_random(14), 215),
    EngineTest("CHField::ProcTextureRange", ppc_engine, host_engine, edge=range_edges, rnd=range_random),
]
