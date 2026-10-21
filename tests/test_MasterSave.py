"""Differential tests for the master save file (unit w89b), against libtpw_unit_MasterSave.dylib (tests/unit_MasterSave.cpp):

  src/game/save/FS_MasterSave.cpp   CFSGlobalMasterSave: Save (0x1016a584), Load (0x1016a6c8), Serialise (0x1016aa08)
  src/game/save/FS_MasterSave.cpp   CFSGlobalParkData: the loader constructor (0x1016bb04) and Serialise (0x1016bd9c)

A case is a world, one call and the fake file's bytes. The world: the master save under test (the flat fields, the set of bought things
and the theme records), optionally a second master save, and what the global gMasterSave points to (nothing, the object under test or the
other one). The call: Save(path), Load(path), Serialise(file, save, version) of either class, or the park's constructor. After it the test
compares the answer, the calls the fakes recorded, every master save and record in the world (and the one Load makes when gMasterSave was
empty), where the fake file's read position stands, and operator new's and delete's counts.

Real on both sides: the unit's functions, the master save and park record of units w32 and w89 (ResetGlobalData, ClearThemeMap,
IsSwearFilterOn, SetSwearFilter), and the container code (the Metrowerks list, set and map code run as the original's on the PowerPC side,
the standard containers on the host). Fakes that record the same event on both sides: the serialisation instances of bool, int,
unsigned int, unsigned short, char and the wide character (they read and write the one fake file, little-endian, and answer one shared
queue "VAR" so that the k-th call of any kind can be made to fail), CPlayerSettings::SerialiseLocal (its own test is test_TPSettings),
LbFile_Open and LbFile_Close, CTRACE, SwearFilter::Init, CSYS_TPFileInfo::GetDataPath, CText::GetLanguageName, the balance table.
"""
import ctypes as C
import struct

import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_MasterSave"

# ---- the unit's functions and the neighbours that run as the original
UNIT = dict(save=0x1016A584, load=0x1016A6C8, serialise=0x1016AA08, pd_ctor=0x1016BB04, pd_serialise=0x1016BD9C)
NEIGHBOURS = dict(ms_ctor=0x101694DC, ms_reset=0x10169A88, swear_set=0x1016B3F4, swear_get=0x1016B438, clear_map=0x1016B6DC,
                  pd_dtor=0x1016BCD4)
# the Metrowerks container code under them (the list ctor, the tree's constructors, destroy, find, insert and rotations), real
REAL_HELPERS = [0x10152DB0, 0x10153B24, 0x10153DEC, 0x10153EFC, 0x1016B168, 0x1016CD78, 0x1016CF68, 0x1016D1F8, 0x1016D4E8,
                0x1016D63C, 0x1016D784, 0x1016DC80, 0x1016DFD0, 0x1016E0B8, 0x1016E37C, 0x1016E65C, 0x1016E8B0, 0x1016E988,
                0x1016E9C4, 0x1016EAF0, 0x1016EB98, 0x1016F080, 0x1016F164]
MAP_INSERT, SET_INSERT = 0x1016E0B8, 0x1016E9C4
# the fakes (in-binary functions)
FAKES = dict(bal_init=0x1001EA28, bal_uninit=0x1001ED40, bal_load=0x10020134, ctrace=0x1006FCBC, serialise_local=0x10166B7C,
             swear_init=0x1020D1BC, data_path=0x10004EC4, language=0x1015E93C)
# the serialisation instances: kind -> (load address, save address, size in bytes)
VARS = {"bool": (0x1000F144, 0x1000F70C, 1), "int": (0x1000EF18, 0x1000F4B0, 4), "uint": (0x100F44A4, 0x100F45BC, 4),
        "ushort": (0x100F5074, 0x100F5530, 2), "char": (0x1016D2C8, 0x1016D3D4, 1), "wchar": (0x10111844, 0x10111A98, 2)}
# the globals
G_MASTER = 0x10380A8C
SETTINGS, TEXT, FILE_INFO = 0x1038071C, 0x1037FF18, 0x102B3458
FILE_HANDLE = 0x6E100000           # the PowerPC side's file (never dereferenced)
LAB = dict(master=1, other=2, settings=3, text=4, fileinfo=5, file=8)

FN = dict(save=0, load=1, serialise=2, pd_serialise=3, pd_ctor=4)
MINUS = 0xFFFFFFFF
PARK_VARS = 6 + 8 + 66 + 2   # the calls of one record's Serialise


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MINUS))


def hexs(data):
    return data.hex()


# ======================================================================= PowerPC side


class Ppc:
    pass


def var_result(e):
    """The answer of a serialisation instance that got as far as answering: the case's failure value at the call number fail_at, else 0."""
    s = e.mv
    n = s.var_calls
    s.var_calls += 1
    return s.fail_value if n == s.fail_at else 0


def build_stubs(emu):
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(UNIT.values())
    S.real.update(NEIGHBOURS.values())
    S.real.update(REAL_HELPERS)
    FS.string_stubs(S)
    S.imp("__lt__15TbStringBase<c>CFRC15TbStringBase<c>",
          lambda e: e.set_r(3, int(FS.s_read(e, e.r(3), False) < FS.s_read(e, e.r(4), False))))
    S.internal(FAKES["bal_init"], lambda e: e.cl.rec("Balance.Init", (e.cl.newlab(e.r(3)),)))
    S.internal(FAKES["bal_uninit"], R("Balance.UnInit", "L"))

    # ---- the fake file and the serialisation instances
    def read_file(e, size):
        s = e.mv
        data = s.file[s.pos:s.pos + size]
        s.pos += len(data)
        return data

    def lb_open(e):
        L = e.cl
        L.rec("LbFile_Open", (e.r(6),), (e.cstr(e.r(5))[:159],))
        result = L.ret("LbFile_Open")
        w32(e, e.r(4), FILE_HANDLE if result >= 0 else 0)
        w32(e, e.r(3), result)

    def lb_close(e):
        e.cl.rec("LbFile_Close", (e.cl.lab(e.r(4)),))
        w32(e, e.r(3), 0)

    S.imp("LbFile_Open__FRPvPCcUl", lb_open)
    S.imp("LbFile_Close__FPv", lb_close)

    def fake_load(kind, size):
        def stub(e):
            L = e.cl
            L.rec(f"LOADVAR<{kind}>", (L.lab(e.r(3)), T.s32(e.r(6))), (e.cstr(e.r(5))[:159],))
            data = read_file(e, size)
            if len(data) != size:
                e.set_r(3, 1)
                return
            e.write(e.r(4), data[::-1])      # the file is little-endian, the PowerPC side's memory big-endian
            e.set_r(3, T.u32(var_result(e)))
        return stub

    def fake_save(kind, size):
        def stub(e):
            L = e.cl
            data = e.read(e.r(4), size)[::-1]
            L.rec(f"SAVEVAR<{kind}>", (L.lab(e.r(3)), T.s32(e.r(6))), (e.cstr(e.r(5))[:159], hexs(data)))
            e.set_r(3, T.u32(var_result(e)))
        return stub

    for kind, (load, save, size) in VARS.items():
        S.internal(load, fake_load(kind, size))
        S.internal(save, fake_save(kind, size))

    def serialise_local(e):
        L = e.cl
        L.rec("CPlayerSettings::SerialiseLocal", (L.lab(e.r(3)), L.lab(e.r(4)), T.s32(e.r(5)), e.r(6)))
        e.set_r(3, T.u32(L.ret("SerialiseLocal")))

    S.internal(FAKES["serialise_local"], serialise_local)
    S.internal(FAKES["ctrace"], lambda e: e.cl.rec("CTRACE", (e.r(3),), (T.c_format(e, e.cstr(e.r(4)), 5)[:159],)))

    def swear_init(e):
        L = e.cl
        L.rec("SwearFilter::Init", (L.lab(e.r(3)), e.r(6) & 0xFF), (e.cstr(e.r(4))[:159], e.cstr(e.r(5))[:159]))
        answer = 1 if L.ret("SwearFilter::Init") else 0
        if L.ret("SwearFilter::ClearGlobal"):
            w32(e, G_MASTER, 0)       # Init tore the global down (the host side does the same)
        e.set_r(3, answer)

    S.internal(FAKES["swear_init"], swear_init)

    def data_path(e):
        e.cl.rec("CSYS_TPFileInfo::GetDataPath", (e.cl.lab(e.r(3)),))
        obj = e.alloc(0x14)
        FS.s_write(e, obj, e.mv.data_path, False)
        e.set_r(3, obj)

    def language(e):
        e.cl.rec("CText::GetLanguageName", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.mv.language)

    S.internal(FAKES["data_path"], data_path)
    S.internal(FAKES["language"], language)
    return S


class World:
    """The PowerPC side of one case: builds the world, calls, dumps."""

    def __init__(self, e, case):
        self.e = e
        s = e.mv = Ppc()
        e.heap_in_scratch = True
        s.file = bytes(case["file"])
        s.pos = 0
        s.var_calls = 0
        s.fail_at = case.get("fail_at", -1)
        s.fail_value = case.get("fail_value", 1)
        s.data_path = case["data_path"]
        s.language = e.alloc(len(case["language"]) + 1, case["language"].encode("latin-1") + b"\0")
        e.cl = CL.PpcLog({}, case.get("q", {}))
        for name, addr in (("settings", SETTINGS), ("text", TEXT), ("fileinfo", FILE_INFO), ("file", FILE_HANDLE)):
            e.cl.labels[addr] = LAB[name]
        self.this = self.other = self.park = self.block = 0
        w32(e, G_MASTER, 0)
        if case.get("master") is not None:
            self.this = self.make_master(case["master"])
            e.cl.labels[self.this] = LAB["master"]
        if case.get("global") == 2:
            self.other = self.make_master(case["other"])
            e.cl.labels[self.other] = LAB["other"]
        w32(e, G_MASTER, {0: 0, 1: self.this, 2: self.other}.get(case.get("global", 1), 0))
        if case.get("park") is not None:
            self.park = self.make_park(case["park"])
        self.block_name = 0
        if case["fn"] == "pd_ctor":
            self.block = e.alloc(0xBC, b"\xa5" * 0xBC)
        e.alloc_log = []

    # ---- building
    def make_park(self, t):
        e = self.e
        block = e.alloc(0xBC, b"\xa5" * 0xBC)
        e.write(block, bytes(t["tickets"]))
        e.write(block + 6, bytes(t["awards"]))
        for i, v in enumerate(t["scores"]):
            w32(e, block + 0xC + 4 * i, v)
        w32(e, block + 0x1C, 0)
        FS.s_write(e, block + 0x20, t["name"], False)
        e.write(block + 0x34, struct.pack(">33H", *t["sa"]))
        e.write(block + 0x76, struct.pack(">33H", *t["sb"]))
        e.write(block + 0xB8, bytes([t["flag"], t["research"]]))
        return block

    def make_master(self, m):
        e = self.e
        block = e.alloc(0x4C, b"\xa5" * 0x4C)
        T.call_ppc(e, NEIGHBOURS["ms_ctor"], [block])
        e.write(block + 0x18, bytes(m["g"]))
        e.write(block + 0x26, bytes(m["s"]))
        w32(e, block + 0x1C, m["spent"])
        w32(e, block + 0x20, m["extra"])
        e.write(block + 0x24, bytes([m["easy"], m["swear"]]))
        e.write(block + 0x48, bytes([m["first"]]))
        for park in m["parks"]:
            record = self.make_park(park)
            pair = e.alloc(0x18)
            FS.s_write(e, pair, park["name"], False)
            w32(e, pair + 0x14, record)
            T.call_ppc(e, MAP_INSERT, [e.alloc(0x10), block + 0x34, pair])
        for thing in m["bought"]:
            cell = e.alloc(4)
            e.write(cell, struct.pack(">H", thing))
            T.call_ppc(e, SET_INSERT, [e.alloc(0x10), block + 0x28, cell])
        return block

    # ---- the call
    def call(self, case):
        e = self.e
        fn, a = case["fn"], case.get("args", [])
        if fn in ("save", "load"):
            path = CL.new_text(e, case["path"])
            r3 = T.call_ppc(e, UNIT[fn], [self.this, path])[0]
        elif fn == "serialise":
            r3 = T.call_ppc(e, UNIT[fn], [self.this, FILE_HANDLE, a[0], a[1]])[0]
        elif fn == "pd_serialise":
            r3 = T.call_ppc(e, UNIT[fn], [self.park, FILE_HANDLE, a[0], a[1]])[0]
        else:
            name = FS.new_string(e, case["path"], False)
            r3 = T.call_ppc(e, UNIT[fn], [self.block, e.alloc(4), name])[0]
            return 0
        return T.s32(r3)

    # ---- the dump (the C++ file's order)
    def walk(self, root):
        """The nodes of a Metrowerks tree in order (left, node, right)."""
        e, out = self.e, []

        def go(node):
            if node:
                go(e.u32(node))
                out.append(node)
                go(e.u32(node + 4))
        go(root)
        return out

    @staticmethod
    def name(text, length):
        return [length] + [ord(c) for c in text[:24]] + [0] * (24 - len(text[:24]))

    def dump_park(self, p):
        e = self.e
        v = list(e.read(p, 6)) + list(e.read(p + 6, 4)) + list(struct.unpack(">4i", e.read(p + 0xC, 16)))
        v += [int(e.u32(p + 0x1C) != 0), e.read(p + 0xB8, 1)[0], e.read(p + 0xB9, 1)[0]]
        v += self.name(FS.s_read(e, p + 0x20, False), e.u32(p + 0x2C))
        v += list(struct.unpack(">33H", e.read(p + 0x34, 66))) + list(struct.unpack(">33H", e.read(p + 0x76, 66)))
        return v

    def dump_master(self, m):
        e = self.e
        v = list(e.read(m + 0x18, 4)) + list(e.read(m + 0x26, 2)) + [T.s32(e.u32(m + 0x1C)), T.s32(e.u32(m + 0x20))]
        v += [e.read(m + 0x24, 1)[0], e.read(m + 0x25, 1)[0], e.read(m + 0x48, 1)[0]]
        bought = [struct.unpack(">H", e.read(node + 0xC, 2))[0] for node in self.walk(e.u32(m + 0x2C))]
        v += [len(bought)] + bought[:40]
        nodes = self.walk(e.u32(m + 0x38))
        v += [len(nodes)]
        for node in nodes:
            v += self.name(FS.s_read(e, node + 0xC, False), e.u32(node + 0x18)) + self.dump_park(e.u32(node + 0x20))
        return v

    def dump(self):
        e = self.e
        now = e.u32(G_MASTER)
        kind = 0 if now == 0 else 1 if now == self.this else 2 if self.other and now == self.other else 3
        v = [kind]
        if self.this:
            v += self.dump_master(self.this)
        if self.other:
            v += self.dump_master(self.other)
        if kind == 3:
            v += self.dump_master(now)
        if self.park:
            v += self.dump_park(self.park)
        if self.block:
            v += self.dump_park(self.block)
        v.append(e.mv.pos)
        return v

    def heap(self):
        log = self.e.alloc_log
        return [sum(1 for k in log if k[0] == "new"), sum(1 for k in log if k[0] == "delete")]


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    if "c" not in FS.VT:
        FS.make_vtables(runner.emu)
    STUBS = build_stubs(runner.emu)


def ppc_run(e, case):
    saved = STUBS.activate(e)
    try:
        w = World(e, case)
        result = w.call(case)
        out = dict(result=result, log=e.cl.log, dump=w.dump(), heap=w.heap())
    finally:
        CL.Stubs.deactivate(e, saved)
        e.cl = None
    return out

# ======================================================================= host side


def flat_park(t):
    name = t["name"].encode("latin-1")
    return ([len(name)] + list(name) + t["tickets"] + t["awards"] + t["scores"] + [t["flag"], t["research"]] + t["sa"] + t["sb"])


def flat_master(m):
    v = m["g"] + m["s"] + [m["spent"], m["extra"], m["easy"], m["swear"], m["first"], len(m["bought"])] + m["bought"]
    v.append(len(m["parks"]))
    for park in m["parks"]:
        v += flat_park(park)
    return v


def host_run(lib, case):
    lib.tpw_mv_reset.argtypes = [C.c_char_p, C.c_char_p, C.c_char_p, C.c_int32, C.c_int64, C.c_int64]
    lib.tpw_mv_build.argtypes = [C.POINTER(C.c_int64), C.c_int32]
    lib.tpw_mv_build_park.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_mv_call.argtypes = [C.POINTER(C.c_int64), C.c_char_p]
    lib.tpw_mv_call.restype = C.c_int64
    lib.tpw_mv_dump.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_mv_heap.argtypes = [C.POINTER(C.c_int64)]
    CL.host_begin(lib, {}, case.get("q", {}))
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    data = bytes(case["file"])
    lib.tpw_mv_reset(case["data_path"].encode("latin-1"), case["language"].encode("latin-1"), data, len(data),
                  case.get("fail_at", -1), case.get("fail_value", 1))
    if case.get("master") is not None:
        flat = flat_master(case["master"]) + (flat_master(case["other"]) if case.get("global") == 2 else [])
        lib.tpw_mv_build((C.c_int64 * len(flat))(*flat), case.get("global", 1))
    if case.get("park") is not None:
        flat = flat_park(case["park"])
        lib.tpw_mv_build_park((C.c_int64 * len(flat))(*flat))
    if case["fn"] == "pd_ctor":
        lib.tpw_mv_build_block()
    before = (C.c_int64 * 4)()
    lib.tpw_mv_heap(before)
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1
    a = case.get("args", [])
    call = (C.c_int64 * 3)(FN[case["fn"]], *([x for x in a] + [0, 0])[:2])
    result = lib.tpw_mv_call(call, case.get("path", "").encode("latin-1"))
    log = CL.host_end(lib)
    after = (C.c_int64 * 4)()
    lib.tpw_mv_heap(after)
    out = (C.c_int64 * 8002)()
    lib.tpw_mv_dump(out)
    return dict(result=T.s32(result) if case["fn"] != "pd_ctor" else 0, log=log, dump=list(out[1:1 + out[0]]),
                heap=[after[0] - before[0], after[2] - before[2]])


lib_ready = False

# ======================================================================= case generation

NAMES = ["hallow", "space", "jungle", "fantasy", "Hollywood", "alpha", "Alpha", "space2", "a", "ab", "b", "zzz", "x" * 30]
BOOLS = [0, 1, 1, 0, 2, 255]
SIGN_UNITS = [ord(c) for c in "ParkSign-ab01 "] + [0x2D, 0x100, 0x8000, 0xFFFF]
INTS = [0, 1, -1, 2, 100, -100, 0x7FFFFFFF, -0x80000000, 0x12345678, 3, 1000]
THINGS = [0, 1, 2, 100, 1150, 0x7FFF, 0x8000, 0xFFFF, 7, 1190, 3601]
DATA_PATHS = ["HD:TPW:data", "", "HD:TPW:data:", "a b"]
LANGUAGES = ["English", "", "Francais", "Deutsch"]


def rnd_park(r, name):
    text = [r.choice(SIGN_UNITS) for _ in range(r.randrange(0, 34))]
    sa = (text + [0] * 33)[:33]
    text = [r.choice(SIGN_UNITS) for _ in range(r.randrange(0, 34))]
    sb = (text + [0] * 33)[:33]
    if r.random() < 0.3:
        sa = [u if r.random() < 0.7 else r.choice(SIGN_UNITS) for u in sa]
        sb = [u if r.random() < 0.7 else r.choice(SIGN_UNITS) for u in sb]
    return dict(name=name, tickets=[r.choice(BOOLS) for _ in range(6)], awards=[r.choice(BOOLS) for _ in range(4)],
                scores=[r.choice(INTS + [r.randrange(-1000, 1000)]) for _ in range(4)], flag=r.choice(BOOLS),
                research=r.choice(BOOLS), sa=sa, sb=sb)


def rnd_master(r, nparks=None, names=None):
    count = r.randrange(0, 5) if nparks is None else nparks
    pool = list(names or NAMES)
    r.shuffle(pool)
    count = min(count, len(pool))
    return dict(g=[r.choice(BOOLS) for _ in range(4)], s=[r.choice(BOOLS) for _ in range(2)], spent=r.choice(INTS),
                extra=r.choice(INTS), easy=r.choice(BOOLS), swear=r.choice(BOOLS), first=r.choice(BOOLS),
                bought=sorted(set(r.choice(THINGS + [r.randrange(65536)]) for _ in range(r.randrange(0, 8)))),
                parks=[rnd_park(r, pool[i]) for i in range(count)])


def blank_park(name):
    return dict(name=name, tickets=[0] * 6, awards=[0] * 4, scores=[0] * 4, flag=0, research=0, sa=[0] * 33, sb=[0] * 33)


def blank_master(names=()):
    return dict(g=[0] * 4, s=[0] * 2, spent=0, extra=0, easy=0, swear=1, first=1, bought=[], parks=[blank_park(n) for n in names])


# ---- the file format (little-endian): what Serialise writes and reads

def le(value, size):
    return (value & ((1 << (8 * size)) - 1)).to_bytes(size, "little")


def enc_park(t):
    out = bytes(t["tickets"])
    for i in range(4):
        out += bytes([t["awards"][i]]) + le(t["scores"][i], 4)
    for i in range(33):
        out += le(t["sa"][i], 2) + le(t["sb"][i], 2)
    return out + bytes([t["flag"], t["research"]])


def enc_body(m, count=None, bought_count=None):
    """The body of a master save file (the player settings' bytes are not in it: their serialiser is a fake)."""
    out = bytes(m["g"]) + bytes(m["s"]) + le(m["spent"], 4) + le(m["extra"], 4) + bytes([m["easy"], m["swear"], m["first"]])
    out += le(len(m["parks"]) if count is None else count, 4)
    for t in m["parks"]:
        raw = t["name"].encode("latin-1")
        out += le(len(raw), 4) + raw + enc_park(t)
    out += le(len(m["bought"]) if bought_count is None else bought_count, 4)
    for thing in m["bought"]:
        out += le(thing, 2)
    return out


def var_calls(m):
    """How many serialisation calls Serialise makes on a master save."""
    return 11 + sum(1 + len(t["name"]) + PARK_VARS for t in m["parks"]) + 1 + len(m["bought"])


def fail_at(r, calls, chance=0.45):
    """The case keys that make the k-th serialisation call (of any kind) answer a failure, or none."""
    if r.random() < chance:
        return dict(fail_at=r.randrange(0, calls + 2), fail_value=r.choice([1, 1, 1, -1, 5]))
    return {}


def base_case(r, fn, **extra):
    case = dict(fn=fn, data_path=r.choice(DATA_PATHS), language=r.choice(LANGUAGES), file=b"", q={}, global_=1)
    case.update(extra)
    case["global"] = case.pop("global_")
    return case


def local_queue(r, q):
    if r.random() < 0.1:
        q["SerialiseLocal"] = [r.choice([1, 1, -1, 7])]
    return q


def corrupt(r, data):
    """A file cut short or extended (a changed byte could make a name's length huge)."""
    k = r.random()
    if k < 0.25:
        return data[:r.randrange(0, len(data) + 1)]
    if k < 0.3:
        return data + bytes(r.randrange(256) for _ in range(r.randrange(1, 9)))
    return data


VERSIONS = [0xC, 0xC, 0xC, 0xB, 0, 0xD, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 1, 0xC]


# ---- Serialise (save)

def ser_save_case(r):
    m = rnd_master(r)
    case = base_case(r, "serialise", master=m, args=[r.choice([1, 1, 2, -1, 0x100, 0x80000000]), r.choice(VERSIONS)])
    case.update(fail_at(r, var_calls(m)))
    case["q"] = local_queue(r, {})
    return case


def ser_save_edges():
    names = ["space", "hallow", "Alpha", "alpha"]
    m = rnd_master(__import__("random").Random(11), 2, ["space", "hallow"])
    m["bought"] = [3, 1, 2]
    yield dict(fn="serialise", master=blank_master(), args=[1, 0xC], file=b"", q={}, data_path="", language="", **{"global": 1})
    yield dict(fn="serialise", master=blank_master(names), args=[1, 0xC], file=b"", q={}, data_path="", language="", **{"global": 1})
    for k in range(var_calls(m) + 3):
        yield dict(fn="serialise", master=m, args=[1, 0xC], file=b"", fail_at=k, data_path="", language="",
                   **{"global": 1})
    for fail in (1, -1):
        yield dict(fn="serialise", master=m, args=[1, 0xC], file=b"", q={"SerialiseLocal": [fail]}, data_path="", language="",
                   **{"global": 1})
    for save in (1, 2, -1, 256, 0x80000000, 0xFFFFFFFF):
        yield dict(fn="serialise", master=m, args=[save, 0xC], file=b"", q={}, data_path="", language="", **{"global": 1})


# ---- Serialise (load)

def loadable(r, names=None):
    return rnd_master(r, names=names)


def ser_load_case(r):
    initial = rnd_master(r, names=r.choice([None, NAMES[:4]]))
    wanted = loadable(r, names=r.choice([None, NAMES[:4], NAMES[2:8]]))
    count = None
    bought_count = None
    if r.random() < 0.06:
        count = r.choice([0, -1, len(wanted["parks"]) + 1, 70000])
    if r.random() < 0.06:
        bought_count = r.choice([0, -1, len(wanted["bought"]) + 1])
    if wanted["parks"] and r.random() < 0.1:
        wanted["parks"].append(dict(wanted["parks"][0]))   # a repeated name
    data = enc_body(wanted, count, bought_count)
    case = base_case(r, "serialise", master=initial, args=[0, r.choice(VERSIONS)], file=corrupt(r, data))
    case.update(fail_at(r, var_calls(wanted), 0.3))
    case["q"] = local_queue(r, {})
    return case


def ser_load_edges():
    import random
    r = random.Random(12)
    wanted = rnd_master(r, 2, ["space", "hallow"])
    wanted["bought"] = [5, 6]
    data = enc_body(wanted)
    initial = blank_master(["jungle"])
    common = dict(fn="serialise", master=initial, args=[0, 0xC], q={}, data_path="", language="", **{"global": 1})
    yield dict(common, file=data)
    for k in range(var_calls(wanted) + 3):
        yield dict(common, file=data, fail_at=k)
    for cut in range(len(data) + 1):
        yield dict(common, file=data[:cut])
    for fail in (1, -1):
        yield dict(common, file=data, q={"SerialiseLocal": [fail]})
    # a name that is already in the map: the load stops after the trace line
    yield dict(common, master=blank_master(["space"]), file=data)
    yield dict(common, master=blank_master(["hallow"]), file=data)
    dup = rnd_master(r, 1, ["space"])
    dup["parks"].append(dict(dup["parks"][0]))
    yield dict(common, file=enc_body(dup))
    # counts: none, negative, more than there is
    yield dict(common, file=enc_body(wanted, 0))
    yield dict(common, file=enc_body(wanted, -1))
    yield dict(common, file=enc_body(wanted, 3))
    yield dict(common, file=enc_body(wanted, bought_count=-2))
    yield dict(common, file=enc_body(wanted, bought_count=0))
    yield dict(common, file=enc_body(wanted, bought_count=3))
    # an empty name, an embedded NUL (the name ends there), a name with high characters
    raw = enc_body(blank_master(["a"]))
    start = 21   # the first name's length field: 17 bytes of flags and counts, then the theme count
    yield dict(common, file=raw[:start] + le(0, 4) + raw[start + 5:])     # the name's length 0 and no characters
    yield dict(common, file=raw[:start] + le(3, 4) + b"a\0b" + raw[start + 5:])
    yield dict(common, file=raw[:start] + le(2, 4) + b"\xe9\xff" + raw[start + 5:])
    # the old state is merged: parks and things already there
    yield dict(common, master=rnd_master(r, 3, ["hallow", "jungle", "x"]), file=enc_body(blank_master(["space"])))
    mixed = blank_master(["space"])
    mixed["bought"] = [3, 5, 7]
    full = blank_master()
    full["bought"] = [5, 6, 7, 8]
    yield dict(common, master=full, file=enc_body(mixed))
    yield dict(common, master=blank_master(), file=enc_body(blank_master()))
    yield dict(common, master=blank_master(), file=b"")
    for version in VERSIONS:
        yield dict(common, file=data, args=[0, version])


# ---- Save(path)

def save_case(r):
    m = rnd_master(r)
    case = base_case(r, "save", master=m, path=r.choice(["HD:TPW:save:master.sav", "", "p"]))
    case.update(fail_at(r, var_calls(m) + 1))
    case["q"] = local_queue(r, {})
    if r.random() < 0.15:
        case["q"]["LbFile_Open"] = [r.choice([-1, -1, -5])]
    elif r.random() < 0.3:
        case["q"]["LbFile_Open"] = [r.choice([0, 1, 7])]
    return case


def save_edges():
    import random
    r = random.Random(13)
    m = rnd_master(r, 2, ["space", "hallow"])
    m["bought"] = [3, 1]
    common = dict(fn="save", master=m, file=b"", data_path="", language="", path="HD:save.sav", **{"global": 1})
    yield dict(common, q={})
    yield dict(common, master=blank_master(), q={})
    for k in range(var_calls(m) + 4):
        yield dict(common, fail_at=k)
    for result in (-1, -2, 0, 1):
        yield dict(common, q={"LbFile_Open": [result]})
    for fail in (1, -1):
        yield dict(common, q={"SerialiseLocal": [fail]})
    yield dict(common, path="", q={"LbFile_Open": [-1]})
    yield dict(common, path="a%sb%d", q={"LbFile_Open": [-1]})


# ---- Load(path)

def load_case(r):
    initial = rnd_master(r, names=r.choice([None, NAMES[:3]]))
    wanted = loadable(r, names=r.choice([None, NAMES[:4]]))
    version = r.choice(VERSIONS)
    data = le(version, 4) + enc_body(wanted)
    other = rnd_master(r, 0)
    case = base_case(r, "load", master=initial, other=other, path=r.choice(["HD:TPW:save:master.sav", "", "q"]),
                     file=corrupt(r, data))
    case["global"] = r.choice([1, 1, 1, 0, 2])
    case.update(fail_at(r, var_calls(wanted) + 1, 0.3))
    case["q"] = local_queue(r, {})
    if r.random() < 0.12:
        case["q"]["LbFile_Open"] = [-1]
    case["q"]["SwearFilter::Init"] = [r.choice([0, 1, 1, 7])]
    return case


def load_edges():
    import random
    r = random.Random(14)
    wanted = rnd_master(r, 2, ["space", "hallow"])
    wanted["swear"] = 1
    wanted["bought"] = [4]
    body = enc_body(wanted)
    data = le(0xC, 4) + body
    initial = rnd_master(r, 2, ["jungle", "alpha"])
    other = blank_master()
    other["swear"] = 1
    common = dict(fn="load", master=initial, other=other, path="HD:save.sav", file=data, q={}, data_path="HD:TPW:data",
                  language="English", **{"global": 1})
    yield dict(common)
    for version in VERSIONS + [0xA, 0xB, 0xC, 0xD, 0xFFFFFFFE]:
        yield dict(common, file=le(version, 4) + body)
    for cut in range(0, 40):
        yield dict(common, file=data[:cut])
    for k in range(var_calls(wanted) + 3):
        yield dict(common, fail_at=k)
    for fail in (1, -1):
        yield dict(common, q={"SerialiseLocal": [fail]})
    for result in (-1, 0, 3):
        yield dict(common, q={"LbFile_Open": [result]})
    for where in (0, 1, 2):
        for swear in (0, 1, 2, 255):
            for ok in (0, 1):
                mine = dict(wanted, swear=swear)
                yield dict(common, file=le(0xC, 4) + enc_body(mine), **{"global": where},
                           q={"SwearFilter::Init": [ok]})
                yield dict(common, file=le(0xC, 4) + enc_body(mine, count=1), **{"global": where},
                           q={"SwearFilter::Init": [ok]})
    # Init answers false and empties gMasterSave: the second gMasterSave == NULL test (0x1016a920) makes a new master save, which
    # gets the filter switched off (the inlined constructor at 0x1016a924..0x1016a9a8, dead in the game)
    for where in (1, 2):
        for ok in (0, 1):
            for swear in (0, 1, 255):
                mine = dict(wanted, swear=swear)
                other_swear = dict(other, swear=swear)
                yield dict(common, file=le(0xC, 4) + enc_body(mine), other=other_swear, **{"global": where},
                           q={"SwearFilter::Init": [ok], "SwearFilter::ClearGlobal": [1]})
    for path, language in (("", ""), ("D", "L"), ("HD:TPW:data:", "English")):
        yield dict(common, data_path=path, language=language)
    yield dict(common, master=blank_master(), q={}, file=le(0xC, 4) + enc_body(blank_master()))
    yield dict(common, path="")
    yield dict(common, path="%s%d", q={"LbFile_Open": [-1]})


# ---- the park record's Serialise

def pd_case(r, save):
    t = rnd_park(r, r.choice(NAMES))
    data = enc_park(rnd_park(r, "n"))
    case = base_case(r, "pd_serialise", park=t, args=[r.choice([1, 2, -1]) if save else 0, r.choice(VERSIONS)],
                     file=data if save else corrupt(r, data))
    case.update(fail_at(r, PARK_VARS))
    return case


def pd_edges(save):
    import random
    r = random.Random(15)
    t = rnd_park(r, "space")
    data = enc_park(rnd_park(r, "other"))
    common = dict(fn="pd_serialise", park=t, args=[1 if save else 0, 0xC], file=data, q={}, data_path="", language="",
                  **{"global": 1}, master=None)
    yield dict(common)
    yield dict(common, park=blank_park("a"))
    for k in range(PARK_VARS + 3):
        yield dict(common, fail_at=k)
    if not save:
        for cut in range(len(data) + 1):
            yield dict(common, file=data[:cut])
        yield dict(common, file=data + b"\1\2\3")
    for version in (0, 0xB, 0xC, 0xFFFFFFFF):
        yield dict(common, args=[1 if save else 0, version])
    for flag in ((2, -1, 0x100, 0x80000000) if save else ()):
        yield dict(common, args=[flag, 0xC])


# ---- the park record's loader constructor

def ctor_case(r):
    name = r.choice(NAMES + [""])
    return base_case(r, "pd_ctor", path=name, master=None)


def ctor_edges():
    for name in ["", "a", "space", "x" * 30, "x" * 200, "with space", "\xe9t\xe9"]:
        yield dict(fn="pd_ctor", path=name, master=None, file=b"", q={}, data_path="", language="", **{"global": 1})


TESTS = []
TESTS.append(FnTest("CFSGlobalMasterSave::Serialise (save)", ppc_run, host_run, edge=lambda: list(ser_save_edges()), rnd=ser_save_case))
TESTS.append(FnTest("CFSGlobalMasterSave::Serialise (load)", ppc_run, host_run, edge=lambda: list(ser_load_edges()), rnd=ser_load_case))
TESTS.append(FnTest("CFSGlobalMasterSave::Save", ppc_run, host_run, edge=lambda: list(save_edges()), rnd=save_case))
TESTS.append(FnTest("CFSGlobalMasterSave::Load", ppc_run, host_run, edge=lambda: list(load_edges()), rnd=load_case))
TESTS.append(FnTest("CFSGlobalParkData::Serialise (save)", ppc_run, host_run, edge=lambda: list(pd_edges(True)),
                    rnd=lambda r: pd_case(r, True)))
TESTS.append(FnTest("CFSGlobalParkData::Serialise (load)", ppc_run, host_run, edge=lambda: list(pd_edges(False)),
                    rnd=lambda r: pd_case(r, False)))
TESTS.append(FnTest("CFSGlobalParkData::CFSGlobalParkData(CSerialise&, name)", ppc_run, host_run, edge=lambda: list(ctor_edges()),
                    rnd=ctor_case))
