"""Unit v51006, CSaveLoad::SaveHeader, PostLoadOperations and PreLoadOperations, against libtpw_unit_CSaveLoad_SaveHeader_2.dylib
(tests/unit_CSaveLoad_SaveHeader_2.cpp), main app ("SimTheme Park"), src/SaveLoad_CSaveLoad_SaveHeader_2.cpp:
  CSaveLoad::SaveHeader 0x10159a08, CSaveLoad::PostLoadOperations 0x101588c4, CSaveLoad::PreLoadOperations 0x101587e4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit is a fake on both
sides that records its arguments: the language name, the string class (constructor, ==, destructor, Length), the 16-bit block swap, the
file write (its result and count come from queues; the bytes written are collected and compared), Serialise, and every call of the Pre and
PostLoadOperations (the world, map and control manager receivers as offsets from gGameWorld).

The world of a case: the language name text, the seven legal texts (a block of 640 units each, the table's pointer), the key word, the
ride record, the queues of results. What is compared: the answer, the event log, every byte written, the ride record, and for
PostLoadOperations the restart word (also seen by the clock's PostLoad event). No fused operations. Not tested: a Length above 0x280
(the original overflows its stack buffer).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CSaveLoad_SaveHeader_2"
MASK = 0xFFFFFFFF

F_SAVE, F_POST, F_PRE = 0x10159A08, 0x101588C4, 0x101587E4
SIZES = {F_SAVE: 1032, F_POST: 248, F_PRE: 172}
OWN = set(SIZES)

ADDR = dict(world=0x1037EBFC, gClock=0x1037F048, gText=0x1037FF18, gPlayerSettings=0x1038071C, gUIInterface=0x10396440,
            gRestartParkClocks=0x103BC170, gSaveFileKey=0x102A6CB4, gParkRequiredRides=0x1034CBB8, langbuf=0)
for _i in range(7):
    ADDR["legal%d" % _i] = 0x1037FE1C + 0x14 * _i
HOST_LAYOUT = [8]
HANDLE = 0x12345
HEADER = 0x4000

# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec(addr, name, spec="", ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret))


def world_rec(addr, name):
    def fn(e):
        e.cl.rec(name, (e.r(3) - e.u32(ADDR["world"]),))
    STUBS.internal(addr, fn)


# SaveHeader's callees
rec(0x1015E93C, "GetLanguageName", "L", lambda e, log: ADDR["langbuf"])
rec(0x102382D8, "StrCtor", "Ns")
rec(0x102382F0, "StrDtor", "L")
rec(0x10239898, "eq", "Ls", "b")
rec(0x10238818, "Length", "L", "i")


def stub_swap(e):
    ptr, count = e.r(3), T.s32(e.r(4))
    e.cl.rec("USwap", (count,))
    data = bytearray(e.read(ptr, 2 * count))
    for k in range(0, 2 * count, 2):
        data[k], data[k + 1] = data[k + 1], data[k]
    e.write(ptr, bytes(data))


def stub_write(e):
    L = e.cl
    dest, file, buf, length, tr = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
    err = T.s32(L.ret("werr"))
    mode = T.s32(L.ret("wmode"))
    L.rec("Write", (file & MASK, length, err, mode))
    e.blob.append(bytes(e.read(buf, length)))
    e.write(dest, struct.pack(">I", err & MASK))
    if mode < 4:
        e.write(tr, struct.pack(">I", [length, length - 1, 0, length + 1][mode] & MASK))


def stub_serialise(e):
    L = e.cl
    L.rec("Serialise", (1 if e.r(3) else 0, e.r(4) & MASK, T.s32(e.r(5)), T.s32(e.r(6)), e.r(7) & MASK))
    e.set_r(3, L.ret("Serialise") & MASK)


STUBS.internal(0x102398B0, stub_swap)
STUBS.internal(0x10238878, stub_write)
STUBS.internal(0x1015B8B0, stub_serialise)

# PreLoadOperations' callees
rec(0x10063CC0, "RestoreLevelHeightfield")
world_rec(0x1010136C, "DeleteMapInEngine")
rec(0x100916A8, "LLCC_ResetMap", "ii")
world_rec(0x1013E368, "DeleteEngineObjects")
world_rec(0x1013AC4C, "DeleteAllThings")
world_rec(0x1013AB48, "InitThingHeap")
rec(0x10032584, "StopRidingRideIn1stPerson")
rec(0x10025DB8, "BMP_FreeAll")
rec(0x10043994, "DestroyCoasterInstances")
rec(0x100D3708, "RSSE_Close")
rec(0x10008F00, "ADV_Kill", "i")
rec(0x101F00C0, "PreLevelLoad", "L")

# PostLoadOperations' callees
rec(0x10236F8C, "InitialiseWeather")
rec(0x1023706C, "InitialiseBlobShadow")
rec(0x10079F8C, "FLY_RestoreMeshes")
world_rec(0x1013E210, "RestoreEngineObjects")
world_rec(0x10101460, "UpdateMapInEngine")
world_rec(0x100FA32C, "CheckAllControlsInitialised")
rec(0x1005C59C, "RecalculateParkDarkening")


def stub_clock(e):
    e.cl.rec("ClockPostLoad", (e.cl.lab(e.r(3)), e.u32(ADDR["gRestartParkClocks"])))


STUBS.internal(0x10147678, stub_clock)
rec(0x100C6964, "TickRideAnims")
rec(0x101EEEB4, "RegenerateRideSigns")
rec(0x101F0124, "PostLevelLoad", "L")
rec(0x100BE03C, "PAR_SetLowSpec", "i")
rec(0x100DA884, "ESP_FixSpriteIDsFromDetail")
rec(0x100DDE38, "SOUND_SetPauseOn3DSoundsOnly", "i")


def setup(runner):
    T.install_std_stubs(runner.emu)
    ADDR["langbuf"] = T.persist_alloc(runner.emu, 64)


# ---------------------------------------------------------------------------------------------------------------- the two sides


def fixed_labels(g):
    d = {1: g("gText"), 2: g("gClock"), 3: g("gUIInterface")}
    for i in range(7):
        d[11 + i] = g("legal%d" % i)
    return d


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    fmt16 = ">H"

    def __init__(self, e, case):
        self.e = e
        e.blob = []
        fixed = {ADDR["gText"]: 1, ADDR["gClock"]: 2, ADDR["gUIInterface"]: 3}
        for i in range(7):
            fixed[ADDR["legal%d" % i]] = 11 + i
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def log(self):
        return self.L.log

    def blob(self):
        return b"".join(self.e.blob)

    def set_ptr(self, what, v):
        self.words(ADDR["world"] if what == 0 else ADDR["legal%d" % (what - 1)] + 8, [v])

    def call_save(self, header):
        r3, _, _ = T.call_ppc(self.e, F_SAVE, [0, HANDLE, header])
        return r3 & MASK

    def call_pre(self):
        T.call_ppc(self.e, F_PRE, [0])
        return 0

    def call_post(self):
        T.call_ppc(self.e, F_POST, [0])
        return 0


class Host(Side):
    fmt = "<I"
    fmt16 = "<H"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_sh_reset()
        CL.host_begin(lib, fixed_labels(self.g), case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sh_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sh_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def blob(self):
        buf = C.create_string_buffer(0x4000)
        n = self.lib.tpw_sh_blob(buf)
        return buf.raw[:n]

    def set_ptr(self, what, v):
        self.lib.tpw_sh_setptr(what, v)

    def call_save(self, header):
        return self.lib.tpw_sh_save(HANDLE, header) & MASK

    def call_pre(self):
        self.lib.tpw_sh_pre()
        return 0

    def call_post(self):
        self.lib.tpw_sh_post()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sh_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "setptr").argtypes = [C.c_int32, C.c_uint64]
    getattr(lib, p + "blob").restype, getattr(lib, p + "blob").argtypes = C.c_uint32, [C.c_void_p]
    getattr(lib, p + "save").restype, getattr(lib, p + "save").argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.set_ptr(0, case["world"])
    s.words(s.g("gRestartParkClocks"), [case["restart"]])
    s.words(s.g("gPlayerSettings") + 8, [case["detail"]])
    s.words(s.g("gSaveFileKey"), [case["key"]])
    s.write(s.g("gParkRequiredRides"), bytes(case["rides"]))
    s.write(s.g("langbuf"), case["lang"].ljust(64, b"\0"))
    for i in range(7):
        a = s.alloc(0x500)
        s.write(a, b"".join(struct.pack(s.fmt16, u) for u in case["texts"][i]))
        s.set_ptr(1 + i, a)


def execute(s, case):
    if case["op"] == "save":
        return s.call_save(case["header"])
    return s.call_pre() if case["op"] == "pre" else s.call_post()


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "blob": s.blob(), "rides": s.read(s.g("gParkRequiredRides"), 0x20),
            "restart": s.rd_word(s.g("gRestartParkClocks")), "lang": s.read(s.g("langbuf"), 64)}


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

NAMES = [b"English", b"Spanish", b"Italian", b"Swedish", b"German", b"French", b"Japanese"]
WERR = [0, 0, 0, 1, 0x7FFFFFFF, -1, -0x80000000]
LENGTHS = [0, 1, 2, 0x27F, 0x280]


def base(r, op):
    c = {"op": op, "header": 0, "world": r.randrange(0x10000000, 0x60000000, 16), "restart": r.choice([0, 1, 2, r.getrandbits(32)]),
         "detail": r.choice([0, 1, 2, MASK, 0x80000000, 0x100, r.getrandbits(32)]), "key": r.choice([0x01221985, r.getrandbits(32)]),
         "rides": [r.getrandbits(8) for _ in range(0x20)], "lang": r.choice(NAMES + [b"", b"english", b"Frenc", b"Japanese2"]),
         "texts": [[r.choice([0, r.getrandbits(16), r.getrandbits(16)]) for _ in range(0x280)] for _ in range(7)], "queues": {}}
    return c


def save_queues(r, c, k, length, werr, wmode, ser):
    c["queues"] = {"eq": [0] * k + [1] if k < 7 else [0], "Length": [length], "werr": werr, "wmode": wmode, "Serialise": [ser]}
    return c


def save_case(r, header=None, k=None, werr=None, wmode=None, ser=None, length=None):
    c = base(r, "save")
    c["header"] = r.choice([0, HEADER]) if header is None else header
    k = r.randrange(8) if k is None else k
    if r.random() < 0.6 and k < 7:
        c["lang"] = NAMES[k]
    if werr is None:
        werr = [r.choice(WERR) if r.random() < 0.15 else 0 for _ in range(6)]
    if wmode is None:
        wmode = [r.choice([1, 2, 3, 4]) if r.random() < 0.15 else r.choice([0, 0, 4]) for _ in range(6)]
    return save_queues(r, c, k, r.choice(LENGTHS + [r.randrange(0x281)]) if length is None else length, werr, wmode,
                       r.choice([0, 1, 2, -1, 0x80000000, 5]) if ser is None else ser)


def save_edges():
    r = random.Random(51006)
    for header in (0, HEADER):
        for k in range(8):
            for length in LENGTHS:
                yield save_case(r, header, k, [0] * 6, [0] * 6, 0, length)
        for ser in (0, 1, 2, -1, 0x80000000):
            yield save_case(r, header, 0, [0] * 6, [0] * 6, ser, 5)
        for stage in range(6):
            for err in (-1, -0x80000000, 0x7FFFFFFF):
                werr = [0] * 6
                werr[stage] = err
                yield save_case(r, header, r.randrange(8), werr, [0] * 6, 0)
            for mode in (1, 2, 3, 4):
                wmode = [0] * 6
                wmode[stage] = mode
                yield save_case(r, header, r.randrange(8), [0] * 6, wmode, 0)
        # a write that leaves the count alone is judged by the last count
        yield save_case(r, header, 1, [0] * 6, [0, 0, 0, 0, 4, 0], 0)
        yield save_case(r, header, 1, [0] * 6, [0, 0, 0, 4, 4, 0], 0)


def save_random(r):
    return save_case(r)


def post_edges():
    r = random.Random(7)
    for detail in (0, 1, 2, MASK, 0x80000000, 0x100, 0x7FFFFFFF):
        c = base(r, "post")
        c["detail"] = detail
        yield c


def post_random(r):
    return base(r, "post")


def pre_edges():
    r = random.Random(8)
    for _ in range(3):
        yield base(r, "pre")


def pre_random(r):
    return base(r, "pre")


TESTS = [
    FnTest("CSaveLoad::SaveHeader", ppc, host, edge=save_edges, rnd=save_random),
    FnTest("CSaveLoad::PostLoadOperations", ppc, host, edge=post_edges, rnd=post_random),
    FnTest("CSaveLoad::PreLoadOperations", ppc, host, edge=pre_edges, rnd=pre_random),
]
