"""Unit v5209: the fly file loader, the mesh restore and the object file loader, against libtpw_unit_FLY_Load.dylib
(tests/unit_FLY_Load.cpp), main app ("SimTheme Park"), src/FLY_Load.cpp:
  FLY_Load 0x10079aa8, FLY_RestoreMeshes 0x10079f8c, OBJECT_Load 0x1007a0b8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit are fakes on both sides that record their arguments (a text as its length and byte sum) and answer from the case: the
file functions (one byte stream), LbMem_Alloc, FLY_Close, GAME_ASSERTMSG, GetRideInfo, RestoreRideInstance, the sound calls and
PAR_InitEmitter. The Mac's word swaps (_USwapBlock32, USwapInplace32<long>, Byteswap, USwapBlockStruct<FLYBOY>) put a little-endian file
into the processor's order: on the host there is nothing to do, so the PowerPC side runs the real ones except _USwapBlock32 (a glue
import: reverses the words) and USwapBlockStruct<FLYBOY> (faked as doing nothing; its region of a base is compared as raw bytes), and the
results are compared as values.

The world of a case: FLY_Load: the byte stream of a fly file, the globals (initialised flag, bases table, zones); FLY_RestoreMeshes: the
bases (random words, the active flags, the ride words, the thing id), the thing table and the ride record; OBJECT_Load: the sound banks,
a stream of records. What is compared: the answer, the globals, the sine table, the bases, the event log.

Fused operations: FLY_Load's sine table (fmadd, fctiwz) runs under HardwareRounding. Not tested: a table of bases longer than 100 words
(the PowerPC would overrun into the next global, the host's table is twice as wide); a table length that is not a multiple of 4 (the
Mac's words are big-endian, the host's are not); a base smaller than 0x60 bytes; a thing id past the thing table.
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_FLY_Load"
MASK = 0xFFFFFFFF

FN_LOAD, FN_RESTORE, FN_OBJECT = 0x10079AA8, 0x10079F8C, 0x1007A0B8
SIZES = {FN_LOAD: 1220, FN_RESTORE: 260, FN_OBJECT: 756}
OWN = set(SIZES)

ADDR = dict(gFlyActive=0x102DAAF4, gFlyInitialised=0x102DAAF8, gFlySinTable=0x102DAAFC, gFlyBases=0x102DA964,
            gFlyZoneA=0x1029D2B4, gFlyZoneB=0x1029D2B0, gFlyZoneC=0x1029D2B8, gFlyZoneD=0x1029D2AC,
            gSoundBanks=0x102F7C94, gThingArray=0x1034CBFC)
RIDE_DB = 0x1037F6C8
HOST_LAYOUT = [48, 0, 2728, 0x122C, 8]     # the host's RideInfo has native pointers: mSubRideTypes is at 2728, the Mac's 0x7b8
FILE_HANDLE = 0x1234
OPEN_HANDLE = 0x5678
BASE_SIZE = 0x122C


def s32(v):
    return T.s32(v)


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at the unit's functions only."""

    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


def tsig(v):
    if isinstance(v, str):
        v = v.encode("latin1")
    return (len(v), sum(v))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
FILE = {"data": b"", "pos": 0}


def st_close_fly(e):
    e.cl.rec("FLY_Close", ())


def st_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),) + tsig(e.cstr(e.r(4))))


def st_read(e):
    file, buf, ln, tr = e.r(4), e.r(5), e.r(6), e.r(7)
    e.cl.rec("LbFile_Read", (file & MASK, ln & MASK, 1 if tr else 0))
    d = FILE["data"][FILE["pos"]:FILE["pos"] + ln]
    FILE["pos"] += len(d)
    if d:
        e.write(buf, d)
    if tr:
        e.write(tr, struct.pack(">I", len(d)))


def st_open(e):
    ref, name, mode = e.r(4), e.r(5), e.r(6)
    e.cl.rec("LbFile_Open", (mode & MASK,) + tsig(e.cstr(name)))
    e.write(ref, struct.pack(">I", OPEN_HANDLE if e.cl.ret("LbFile_Open") else 0))


def st_seek(e):
    e.cl.rec("LbFile_Seek", (e.r(3) & MASK, s32(e.r(4)), e.r(5) & MASK))
    FILE["pos"] += s32(e.r(4))
    e.set_r(3, 0)


def st_close(e):
    e.cl.rec("LbFile_Close", (e.r(4) & MASK,))


def st_alloc(e):
    size = e.r(3) & MASK
    e.cl.rec("LbMem_Alloc", (size,))
    a = e.alloc(size)
    e.write(a, bytes(size))
    e.set_r(3, a)


def st_swap(e):
    a, n = e.r(3), s32(e.r(4))
    if n > 0:
        d = e.read(a, 4 * n)
        e.write(a, b"".join(d[i:i + 4][::-1] for i in range(0, 4 * n, 4)))


def st_nothing(e):
    pass


def st_par(e):
    e.cl.rec("PAR_InitEmitter", tuple(s32(e.r(i)) for i in range(3, 7)))
    e.set_r(3, e.cl.ret("PAR_InitEmitter") & MASK)


def st_event(e):
    e.cl.rec("SOUND_SendEvent", tuple(s32(e.r(i)) for i in range(3, 9)))
    e.set_r(3, e.cl.ret("SOUND_SendEvent") & MASK)


def st_event_ext(e):
    e.cl.rec("SOUND_SendEventExt", tuple(s32(e.r(i)) for i in range(3, 10)))
    e.set_r(3, e.cl.ret("SOUND_SendEventExt") & MASK)


def st_rect(e):
    e.cl.rec("SOUND_SetRect", tuple(s32(e.r(i)) for i in range(3, 8)))


def st_ride_info(e):
    e.cl.rec("GetRideInfo", (1 if e.r(3) == RIDE_DB else 0, (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, e.ride_block)


def st_restore(e):
    sp = e.r(1)
    e.cl.rec("RestoreRideInstance", tuple(e.r(i) & MASK for i in range(3, 11)) + (e.u32(sp + 0x38),))
    e.set_r(3, e.cl.ret("RestoreRideInstance") & MASK)


STUBS.internal(0x100783EC, st_close_fly)
STUBS.internal(0x1000DDE8, st_assert)
STUBS.imp("LbFile_Read__FPvPvUlPUl", st_read)
STUBS.imp("LbFile_Open__FPPvPCcUl", st_open)
STUBS.imp("LbFile_Seek__FPvl14TbFileSeekMode", st_seek)
STUBS.imp("LbFile_Close__FPv", st_close)
STUBS.imp("LbMem_Alloc__FUi", st_alloc)
STUBS.imp("_USwapBlock32__13SamsUtilitiesFPUll", st_swap)
STUBS.internal(0x10079740, st_nothing)
STUBS.internal(0x100BEC30, st_par)
STUBS.internal(0x100DD9FC, st_event)
STUBS.internal(0x100DDD08, st_event_ext)
STUBS.internal(0x100DE828, st_rect)
STUBS.internal(0x10154868, st_ride_info)
STUBS.internal(0x1006A140, st_restore)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def w16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def r16(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def wptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psz))[0]

    def bases(self, i):
        return self.g("gFlyBases") + i * self.psz


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psz = 4
    thing_stride = 0x14
    sub_off = 0x7B8

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        a = self.e.alloc(n)
        self.e.write(a, bytes(n))
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_file(self, data):
        FILE["data"], FILE["pos"] = data, 0

    def set_ride_block(self, a):
        self.e.ride_block = a

    def call(self, fn, args):
        if fn == FN_LOAD:
            with ROUNDING(self.e):
                r3, _, _ = T.call_ppc(self.e, fn, args)
        else:
            T.call_ppc(self.e, fn, args)
            return 0           # a void function
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psz = 8
    thing_stride = 48
    sub_off = 2728

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fly_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fly_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_file(self, data):
        self.lib.tpw_fly_setfile(data, len(data))

    def set_ride_block(self, a):
        self.write(self.g("gRideBlock"), struct.pack("<Q", a))

    def call(self, fn, args):
        if fn == FN_LOAD:
            return self.lib.tpw_fly_load(args[0]) & MASK
        if fn == FN_RESTORE:
            self.lib.tpw_fly_restore()
            return 0
        self.lib.tpw_fly_object(args[0])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fly_addr.restype, lib.tpw_fly_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fly_alloc.restype, lib.tpw_fly_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fly_setfile.restype, lib.tpw_fly_setfile.argtypes = None, [C.c_char_p, C.c_uint32]
    lib.tpw_fly_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fly_load.restype, lib.tpw_fly_load.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_fly_restore.restype, lib.tpw_fly_restore.argtypes = None, []
    lib.tpw_fly_object.restype, lib.tpw_fly_object.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fly_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def le(*words):
    return b"".join(struct.pack("<I", w & MASK) for w in words)


def be(*words):
    return b"".join(struct.pack(">I", w & MASK) for w in words)


def loaded(case):
    return case["magic"] == 0x5F594C46 and case["version"] != 0


def build(s, case):
    op = case["op"]
    s.set_file(case.get("file", b""))
    if op == "load":
        s.words(s.g("gFlyActive"), [case["active"]])
        s.words(s.g("gFlyInitialised"), [1 if case["init"] else 0])
        s.wptr(s.g("gFlySinTable"), 0)
        for i in range(100):
            s.wptr(s.bases(i), case["preset"][i])
        for name, v in zip(("gFlyZoneA", "gFlyZoneB", "gFlyZoneC", "gFlyZoneD"), case["zones"]):
            s.words(s.g(name), [v])
    elif op == "restore":
        s.set_ride_block(blk := s.alloc(0x1000))
        s.words(blk + s.sub_off, [case["sub"]])
        for i in range(100):
            s.wptr(s.bases(i), 0)
        for tid in range(8):
            ent = s.g("gThingArray") + tid * s.thing_stride
            if tid in case["things"]:
                t = s.alloc(0x20)
                s.w16(t + 0xE, case["things"][tid])
                s.wptr(ent, t)
            else:
                s.wptr(ent, 0)
        for i, b in case["bases"].items():
            a = s.alloc(BASE_SIZE)
            s.words(a, b["fill"])
            s.w16(a + 0x58, b["id"])
            for j, (active, ride) in enumerate(b["slots"]):
                s.words(a + 0xDC + j * 0xE4, [active])
                s.words(a + 0x11C + j * 0xE4, [ride])
            s.wptr(s.bases(i), a)
    else:
        s.words(s.g("gSoundBanks"), case["banks"])


def execute(s, case):
    op = case["op"]
    if op == "load":
        return s.call(FN_LOAD, [FILE_HANDLE])
    if op == "restore":
        return s.call(FN_RESTORE, [])
    p = case["path"]
    a = 0
    if p is not None:
        a = s.alloc(len(p) + 1)
        s.write(a, p.encode("latin1") + b"\0")
    return s.call(FN_OBJECT, [a])


def observe(s, case, answer):
    d = {"answer": answer, "log": s.log()}
    op = case["op"]
    if op == "load":
        d["flags"] = [s.rd_word(s.g("gFlyActive")), s.rd_word(s.g("gFlyInitialised"))]
        d["zones"] = [s.rd_word(s.g(n)) for n in ("gFlyZoneA", "gFlyZoneB", "gFlyZoneC", "gFlyZoneD")]
        tab = s.rptr(s.g("gFlySinTable"))
        if loaded(case) and not case["init"]:
            d["table"] = s.rd_words(tab, 0x1000)
        else:
            d["tablePtr"] = tab & MASK
        bs = case["bs"]
        for i in range(100):
            p = s.rptr(s.bases(i))
            if loaded(case) and i in case["alloc"]:
                d["base%d" % i] = (s.rd_words(p, 22), s.r16(p + 0x58), s.read(p + 0x5A, bs - 0x5A))
            else:
                d["base%d" % i] = p & MASK
    elif op == "restore":
        for i, b in case["bases"].items():
            p = s.rptr(s.bases(i))
            w = s.rd_words(p, BASE_SIZE // 4)
            w[0x58 // 4] = None
            d["base%d" % i] = (w, s.r16(p + 0x58))
    return d


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

MARKS = [0, 0x01010101, 0x7E7E7E7E, 0x02020202]


def fly_case(r, good=None, version=None, init=None, nw=None, density=None, bs=None):
    good = r.random() < 0.85 if good is None else good
    magic = 0x5F594C46 if good else r.choice([0, 0x464C595F, r.getrandbits(32)])
    version = r.choice([1, 2, r.getrandbits(32) | 1, 1, 1, 0]) if version is None else version
    init = r.random() < 0.4 if init is None else init
    nw = r.choice([0, 1, 2, 5, 20, 99, 100, r.randrange(0, 101)]) if nw is None else nw
    density = r.choice([0.0, 0.03, 0.1, 0.5, 1.0]) if density is None else density
    if bs is None:
        bs = r.choice([0x60, 0x64, BASE_SIZE, r.randrange(0x60, 0x300) & ~3])
        if density >= 0.5:
            bs = min(bs, 0x100)
    preset = [r.choice(MARKS) if r.random() < 0.5 else 0 for _ in range(100)]
    filemarks = [r.choice(MARKS[1:]) if r.random() < density else 0 for _ in range(nw)]
    final = (list(preset) if init else [0] * 100)
    final[:nw] = filemarks
    alloc = [i for i in range(1, 100) if final[i]]
    zones = [r.getrandbits(32) for _ in range(4)]
    file = le(magic, version, 4 * nw) + le(*filemarks) + le(*[r.getrandbits(32) for _ in range(4)])
    file += le(*[r.getrandbits(32) for _ in range(5)], bs)
    for _ in alloc:
        file += bytes(r.getrandbits(8) for _ in range(bs))
    file = file[:12] + file[12:]
    # the stream: magic, version, length, marks, zones D B A C, five words, the size, the bases
    zone_bytes = le(*[r.getrandbits(32) for _ in range(4)])
    file = le(magic, version, 4 * nw) + le(*filemarks) + zone_bytes + le(*[r.getrandbits(32) for _ in range(5)], bs)
    for _ in alloc:
        file += bytes(r.getrandbits(8) for _ in range(bs))
    zd, zb, za, zc = struct.unpack("<4I", zone_bytes)
    return {"op": "load", "magic": magic, "version": version, "init": init, "active": r.choice([0, 1, 7]), "preset": preset,
            "zones": zones, "file": file, "bs": bs, "alloc": alloc, "queues": {}}


def fly_edges():
    r = random.Random(5201)
    yield fly_case(r, good=False)
    yield fly_case(r, good=False, init=True)
    yield fly_case(r, good=True, version=0)
    yield fly_case(r, good=True, version=0, init=True)
    for init in (False, True):
        for nw in (0, 1, 2, 100):
            for dens in (0.0, 0.1, 1.0):
                yield fly_case(r, good=True, version=1, init=init, nw=nw, density=dens, bs=BASE_SIZE if dens < 1 else 0x60)


def fly_random(r):
    return fly_case(r)


def rest_case(r, empty=False):
    thing0 = r.random() < 0.7
    things = {tid: r.getrandbits(16) for tid in range(1, 8)}
    if thing0:
        things[0] = r.getrandbits(16)
    ids = list(things)
    bases = {}
    for i in ([] if empty else r.sample(range(1, 100), r.choice([0, 1, 2, 5, 30]))):
        slots = [(r.choice([0, 0, 1, r.getrandbits(32)]), r.getrandbits(32)) for _ in range(20)]
        bases[i] = {"id": r.choice(ids), "slots": slots, "fill": [r.getrandbits(32) for _ in range(BASE_SIZE // 4)]}
    if r.random() < 0.3:
        bases[99] = {"id": r.choice(ids), "slots": [(1, r.getrandbits(32))] * 20, "fill": [r.getrandbits(32) for _ in range(BASE_SIZE // 4)]}
    return {"op": "restore", "things": things, "bases": bases, "sub": r.getrandbits(32),
            "queues": {"RestoreRideInstance": [r.getrandbits(32) for _ in range(40)]}}


def rest_edges():
    r = random.Random(5202)
    yield rest_case(r, empty=True)
    for _ in range(6):
        yield rest_case(r)


def rest_random(r):
    return rest_case(r)


def rec_words(r):
    typ = r.choice([-1, 0, 1, 2, 3, 3, 4, 1, 5])

    def val():
        return r.choice([0, 1, -1, 1023, 1024, 1025, -1023, -1024, -1025, 2047, -2047, r.randrange(-1 << 31, 1 << 31),
                         r.randrange(-100000, 100000), 0x3FFFFFFF, 0x40000000, 0x7FFFFFFF, -0x80000000])
    w = [typ, r.choice([0, 1, 5]), r.getrandbits(16)] + [val() for _ in range(12)]
    return w


def obj_case(r, path=True, magic=None, openok=None, count=None, rsize=None):
    p = "obj/level%d.bin" % r.randrange(100) if path else None
    magic = 0x5F4A424F if magic is None else magic
    openok = r.random() < 0.9 if openok is None else openok
    count = r.choice([0, 1, 2, 3, 6, -1, 8]) if count is None else count
    rsize = r.choice([0x3C, 0x3C, 0x10, 0x28, 0x50, 0x3D, 0x3B, 0x70, 0, 4]) if rsize is None else rsize
    file = be(magic, count, rsize)       # the object file is the Mac's own, big-endian
    for _ in range(max(count, 0)):
        w = rec_words(r)
        raw = be(*w) + bytes(max(rsize - 60, 0))
        file += raw[:rsize]
    return {"op": "object", "path": p, "file": file, "banks": [r.getrandbits(32) for _ in range(8)],
            "queues": {"LbFile_Open": [1 if openok else 0], "SOUND_SendEvent": [r.getrandbits(32) for _ in range(8)],
                       "SOUND_SendEventExt": [0], "PAR_InitEmitter": [0]}}


def obj_edges():
    r = random.Random(5203)
    yield obj_case(r, path=False)
    yield obj_case(r, openok=False)
    yield obj_case(r, magic=0)
    yield obj_case(r, count=0)
    yield obj_case(r, count=-1)
    for rs in (0x3C, 0x3D, 0x3B, 0x10, 0x50):
        yield obj_case(r, count=6, rsize=rs, openok=True)


def obj_random(r):
    return obj_case(r)


TESTS = [
    FnTest("FLY_Load", ppc, host, edge=fly_edges, rnd=fly_random),
    FnTest("FLY_RestoreMeshes", ppc, host, edge=rest_edges, rnd=rest_random),
    FnTest("OBJECT_Load", ppc, host, edge=obj_edges, rnd=obj_random),
]
