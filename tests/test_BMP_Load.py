"""Unit v5L09, the advisor load, against libtpw_unit_BMP_Load.dylib (tests/unit_BMP_Load.cpp), main app ("SimTheme Park"),
src/game/rides/BumperRide.cpp:
  BMP_Load 0x10023850
with the chunk reader of unit v51017 (Read_OpenChunk, Read_DataFromChunk, Read_BumpFromChunk and the swap helpers), which runs as the
original on the PowerPC side and as src/game/rides/BumperRide.cpp on the host.

A case is a world, a save file and a call. The world is built twice by ONE piece of Python code (emulator memory, big-endian, at the
Mac's addresses for the original; the host library's memory for the translation): the bump world at 0x102b56c0 (its magic, the ride
records, the free list of link nodes), 256 ride records of 0xd0 bytes, bumps of 0xac bytes, link nodes, collision objects, the ride
anim sets of gRideAnimSets and a ride info block. The file is ONE little-endian byte stream, the same bytes on both sides, in BMP_Save's
format (a save chunk, then chunks 2 to 9; the Mac swaps every word it reads, the host's words are already in order).

The callees outside the unit are fakes on both sides that record their arguments: the file functions (BMPFile_Read copies from the
stream, failing a 4-byte read at a position the case marks; LbFile_GetPosition answers a base plus the position; LbFile_Seek moves it),
GlobalAlloc (the case's chunk blocks, or 0) and GlobalFree, memset, TRACE (the formatted text), BMP_CreateRide and the ride setters,
BMP_TrackSectionPlaceSection, the peep list adds, SetBuoySequence, FindBuoy, BMP_StartBump, PointInAnyColObject, GetRideInfo,
RestoreRideInstance, FindDataNodeIndexByID and __fpclassifyf (1 for a NaN, else 4; a NaN is recorded as 0x7fc00000).

What is compared: the answer, the log, the world's words, the ride records (all of 0..15, word 0x50 of every one), the bumps, the link
nodes and the chunk blocks; the pointer words (the world's rides and free list, a ride's 0x58 and 0xcc, a bump's 0x30, 0x98 and 0x9c,
a node's 0x10, a chunk's file) as the label of the block they point at.

No fused operations. Not tested: a save chunk whose block is not allocated (the Mac closes the uninitialised stack slot); a failed
8-byte or 0xac-byte read (the Mac's pair is stack garbage, the bump's old words are swapped); a tag 9 chunk with the free list empty or
the ride's bump NULL, and a bump with no owner (the Mac writes through NULL); GetRideInfo answering NULL; a bump's sub-ride index
past 7 (the Mac reads the words after the ride info's mSubRideTypes, the host's layout differs past them).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_Load"
MASK = 0xFFFFFFFF

FN_LOAD = 0x10023850
SIZES = {FN_LOAD: 8432,
         # the chunk reader (unit v51017) and its swap helpers run as the original
         0x10021FAC: 216, 0x100220BC: 220, 0x100221D4: 592,
         0x10021C6C: 40, 0x10021CE0: 40, 0x10021BF8: 40, 0x10021D48: 40, 0x10021DB8: 12, 0x10021DFC: 12, 0x10021BB4: 12}
OWN = set(SIZES)

WORLD, SETS, RIDEDB = 0x102B56C0, 0x102C79A4, 0x1037F6C8
MAGIC = 0x4A454647
HOST_LAYOUT = [0x18, 0x98, 2728, 2824]   # the host's RideInfo has native pointers: mSubRideTypes is at 2728, the Mac's 0x7b8
STREAM_BASE = 0x1000

NRIDES, NUSED, NFULL = 256, 8, 16     # ride records; the ones the chunks name; the ones compared whole
NBUMPS = 14                           # 0..7 the rides' bumps, the rest for BMP_StartBump
NCOLS, NLINKS, NSETS = 4, 12, 8       # links: the free list takes the first case["free"], the last two are held by bumps
KINDS = [-4, -5, -7, -8, -9, -10, -12, -13, -1, -6, -11, -3, 3]    # never 0: an id read as 0 must not name ride 0
L_FILE, L_WORLD, L_RIDEDB, L_INFO = 1, 2, 5, 6
L_BUMP, L_COL, L_LINK, L_SET, L_RIDE, L_POOL = 10, 30, 40, 60, 100, 360    # the host's labels stop at 511


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STREAM = {"data": b"", "pos": 0}


def toc(fn):
    """The glue of a real call saves the TOC in 0x14(r1) and the original reloads r2 from there: the fake does the save."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", 0x10267CC0))
        return fn(e)
    return wrapped


def st_read(e):
    """BMPFile_Read(TbError* hidden r3, file r4, buffer r5, size r6, long* r7)."""
    L = e.cl
    size = e.r(6) & MASK
    L.rec("BMPFile_Read", (L.lab(e.r(4)), size))
    pos = STREAM["pos"]
    if size == 4 and L.ret("fail:%d" % pos, 0):
        e.write(e.r(3), struct.pack(">I", MASK))
        return
    data = STREAM["data"]
    n = min(size, len(data) - pos if pos < len(data) else 0)
    if n:
        e.write(e.r(5), data[pos:pos + n])
    STREAM["pos"] = pos + n
    e.write(e.r(7), struct.pack(">I", n))
    e.write(e.r(3), struct.pack(">I", 0))


def st_getpos(e):
    e.cl.rec("LbFile_GetPosition", (e.cl.lab(e.r(3)),))
    e.set_r(3, (STREAM_BASE + STREAM["pos"]) & MASK)


def st_seek(e):
    e.cl.rec("LbFile_Seek", (e.cl.lab(e.r(3)), s32(e.r(4)), e.r(5) & MASK))
    STREAM["pos"] = (e.r(4) - STREAM_BASE) & MASK
    e.set_r(3, 0)


def st_memset(e):
    n = e.r(5) & MASK
    e.write(e.r(3), bytes([e.r(4) & 0xFF]) * n)


def st_trace(e):
    text = e.cstr(e.r(3))
    if "%d" in text:
        text = text.replace("%d", str(s32(e.r(4))))
    e.cl.rec("TRACE", (), (text[:159],))


def st_fpclassify(e):
    x = e.f(1)
    e.cl.rec("fpclassifyf", (0x7FC00000 if x != x else CL.fbits(x),))
    e.set_r(3, 1 if x != x else 4)


def st_restore(e):
    L = e.cl
    L.rec("RestoreRideInstance", (e.r(3) & MASK, e.r(4) & MASK, e.r(5) & MASK, L.lab(e.r(6)), e.r(7) & MASK, L.lab(e.r(8)),
                                  s32(e.r(9)), s32(e.r(10)), s32(e.u32(e.r(1) + 0x38))))
    e.set_r(3, L.ret("RestoreRideInstance") & MASK)


STUBS.internal(0x1002FFC4, toc(st_read))
STUBS.internal(0x102398E0, toc(st_getpos))
STUBS.internal(0x10239B98, toc(st_seek))
STUBS.internal(0x10238560, toc(st_memset))
STUBS.internal(0x1006FCFC, toc(st_trace))
STUBS.internal(0x10239268, toc(st_fpclassify))
STUBS.internal(0x10239610, toc(CL.recorder("GlobalAlloc", "uu", ret="i")))
STUBS.internal(0x10238B18, toc(CL.recorder("GlobalFree", "L", ret=lambda e, L: 0)))
STUBS.internal(0x1002756C, toc(CL.recorder("BMP_CreateRide", "iiiiii", ret="i")))
STUBS.internal(0x10026C98, toc(CL.recorder("BMP_SetRidePerformance", "ii")))
STUBS.internal(0x10026BE4, toc(CL.recorder("BMP_SetRideModelSelection", "iii")))
STUBS.internal(0x10026B14, toc(CL.recorder("BMP_SetRideDuration", "ii", ret="i")))
STUBS.internal(0x1002EF98, toc(CL.recorder("BMP_TrackSectionPlaceSection", "iiiiiui", ret="i")))
STUBS.internal(0x1002E268, toc(CL.recorder("BMP_AddPeepToRideLeavingList", "iu")))
STUBS.internal(0x1002E070, toc(CL.recorder("BMP_AddPeepToRideJoiningList", "iu", ret="i")))
STUBS.internal(0x1002D390, toc(CL.recorder("SetBuoySequence", "L")))
STUBS.internal(0x10025960, toc(CL.recorder("FindBuoy", "Lii", ret="i")))
STUBS.internal(0x1002EAAC, toc(CL.recorder("BMP_StartBump", "iii", ret="i")))
STUBS.internal(0x1002BA94, toc(CL.recorder("PointInAnyColObject", "ii", ret="i")))
STUBS.internal(0x10154868, toc(CL.recorder("GetRideInfo", "Lw", ret="i")))
STUBS.internal(0x1006A140, toc(st_restore))
STUBS.internal(0x101FFDC8, toc(CL.recorder("FindDataNodeIndexByID", "Luu", ret="i")))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def make(self, case):
        self.file = self.alloc(16)
        self.rides = self.alloc(NRIDES * 0xD0)
        self.bumps = [self.alloc(0xAC) for _ in range(NBUMPS)]
        self.cols = [self.alloc(16) for _ in range(NCOLS)]
        self.links = [self.alloc(0x14) for _ in range(NLINKS)]
        self.sets = [self.alloc(16) for _ in range(NSETS)]
        self.info = self.alloc(self.info_size)
        self.pool = [self.alloc(0x18) for _ in range(case["pool"])]
        self.labels = {self.file: L_FILE, self.world: L_WORLD, self.ridedb: L_RIDEDB, self.info: L_INFO}
        self.labels.update({self.ride(i): L_RIDE + i for i in range(NRIDES)})
        for base, blocks in ((L_BUMP, self.bumps), (L_COL, self.cols), (L_LINK, self.links), (L_SET, self.sets), (L_POOL, self.pool)):
            self.labels.update({a: base + i for i, a in enumerate(blocks)})
        # the label of the block a pointer word points at
        self.plabel = {self.ptrword(a): l for a, l in self.labels.items() if a != self.world and a != self.ridedb}

    def ride(self, i):
        return self.rides + i * 0xD0

    def lbl(self, v):
        return 0 if v == 0 else self.plabel.get(v, ("raw", v))

    def queues(self, case):
        q = {k: list(v) for k, v in case["queues"].items()}
        allocs = [0 if i == case["alloc_fail"] else a for i, a in enumerate(self.pool)]
        q["GlobalAlloc"] = allocs + [0]
        q["BMP_StartBump"] = [0 if b < 0 else self.bumps[b] for b in case["queues"]["BMP_StartBump"]]
        q["PointInAnyColObject"] = [0 if c < 0 else self.cols[c] for c in case["queues"]["PointInAnyColObject"]]
        q["GetRideInfo"] = [self.info]
        return q


class Ppc(Side):
    fmt = ">I"
    info_size = 0x808
    sub_off = 0x7B8
    world = WORLD
    ridedb = RIDEDB

    def __init__(self, e, case):
        self.e = e
        self.make(case)
        self.L = e.cl = CL.PpcLog({a: l for a, l in self.labels.items()}, self.queues(case))
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

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

    def ptrword(self, a):
        return a

    def set_sets(self, addresses):
        self.words(SETS, addresses)

    def set_stream(self, data):
        STREAM["data"], STREAM["pos"] = data, 0

    def call(self):
        r3, _, _ = T.call_ppc(self.e, FN_LOAD, [self.file])
        return s32(r3)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.info_size = lib.layout[3]
        self.sub_off = lib.layout[2]
        self.world = lib.tpw_bl_addr(b"world")
        self.ridedb = lib.tpw_bl_addr(b"ridedb")
        self.make(case)
        CL.host_begin(lib, {l: a for a, l in self.labels.items()}, self.queues(case))

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_bl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptrword(self, a):
        return self.lib.tpw_bl_offset(a)

    def set_sets(self, addresses):
        self.write(self.lib.tpw_bl_addr(b"sets"), b"".join(struct.pack("<Q", a) for a in addresses))

    def set_stream(self, data):
        self.lib.tpw_bl_stream(data, len(data), STREAM_BASE)

    def call(self):
        return self.lib.tpw_bl_load(self.file)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bl_addr.restype, lib.tpw_bl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bl_alloc.restype, lib.tpw_bl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bl_offset.restype, lib.tpw_bl_offset.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_bl_stream.restype, lib.tpw_bl_stream.argtypes = None, [C.c_char_p, C.c_uint32, C.c_uint32]
    lib.tpw_bl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bl_load.restype, lib.tpw_bl_load.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bl_layout(out)
    lib.layout = list(out)[:n]
    assert lib.layout == HOST_LAYOUT, lib.layout
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    r = random.Random(case["seed"])
    kinds = case["kinds"]
    held = [s.links[NLINKS - 2], s.links[NLINKS - 1]]

    world = [r.getrandbits(32) for _ in range(15)]
    world[0] = case["magic"]
    world[2] = s.ptrword(s.rides)
    free = case["free"]
    world[13] = s.ptrword(s.links[0]) if free else 0
    s.words(s.world, world)

    for i in range(NRIDES):
        if i < NFULL:
            w = [r.getrandbits(32) for _ in range(52)]
            w[0x58 // 4] = s.ptrword(s.bumps[i]) if i < NUSED and r.random() < 0.5 else 0
            w[0xCC // 4] = s.ptrword(s.bumps[i]) if i < NUSED else 0
            w[0x10 // 4] = r.getrandbits(16)
        else:
            w = [0] * 52
            w[0x50 // 4] = r.getrandbits(32)
        w[0] = kinds[i] & MASK if i < NUSED else 0x7F
        s.words(s.ride(i), w)

    for i, b in enumerate(s.bumps):
        w = [r.getrandbits(32) for _ in range(43)]
        w[8 // 4] = r.randrange(NSETS)
        w[0x30 // 4] = s.ptrword(r.choice(held)) if r.random() < 0.4 else 0
        w[0x98 // 4] = s.ptrword(s.cols[r.randrange(NCOLS)]) if r.random() < 0.5 else 0
        w[0x9C // 4] = s.ptrword(s.ride(i % NUSED))
        s.words(b, w)

    for i, a in enumerate(s.links):
        w = [r.getrandbits(32) for _ in range(5)]
        w[4] = s.ptrword(s.links[i + 1]) if i + 1 < free else 0
        s.words(a, w)

    for a in s.cols + s.sets:
        s.words(a, [r.getrandbits(32) for _ in range(4)])
    s.set_sets(s.sets)
    s.words(s.info + s.sub_off, [r.getrandbits(32) for _ in range(8)])
    s.set_stream(case["stream"])


def observe(s, answer):
    world = s.rd_words(s.world, 15)
    for k in (2, 13):
        world[k] = s.lbl(world[k])
    rides = s.rd_words(s.rides, NRIDES * 52)
    full = []
    for i in range(NFULL):
        w = rides[i * 52:(i + 1) * 52]
        for k in (0x58 // 4, 0xCC // 4):
            w[k] = s.lbl(w[k])
        full.append(w)
    bumps = []
    for b in s.bumps:
        w = s.rd_words(b, 43)
        for k in (0x30 // 4, 0x98 // 4, 0x9C // 4):
            w[k] = s.lbl(w[k])
        bumps.append(w)
    links = []
    for a in s.links:
        w = s.rd_words(a, 5)
        w[4] = s.lbl(w[4])
        links.append(w)
    chunks = []
    for a in s.pool:
        w = s.rd_words(a, 6)
        w[1] = s.lbl(w[1])
        chunks.append(w)
    return {"answer": answer, "log": s.log(), "world": world, "rides": full, "f50": rides[0x50 // 4::52], "bumps": bumps,
            "links": links, "chunks": chunks}


def run(s, case):
    try:
        build(s, case)
        return observe(s, s.call())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the save files


def le(*words):
    return b"".join(struct.pack("<I", w & MASK) for w in words)


class File:
    """A save file being written: the chunks, the positions of the word reads (for failures), the answers the fakes give."""

    def __init__(self, r, kinds):
        self.r, self.kinds = r, kinds
        self.body = b""
        self.words = []
        self.q = {"BMP_CreateRide": [], "BMP_StartBump": [], "PointInAnyColObject": [], "RestoreRideInstance": [],
                  "FindDataNodeIndexByID": [], "FindBuoy": [], "BMP_SetRideDuration": [], "BMP_TrackSectionPlaceSection": [],
                  "BMP_AddPeepToRideJoiningList": []}
        self.links = 0
        self.fresh = NUSED
        self.count = 0

    def rid(self, i=None, good=True):
        r = self.r
        if i is None:
            i = r.randrange(NUSED)
        if not good:
            return r.choice([(((self.kinds[i] << 8) | i) ^ 0x100) & MASK, r.getrandbits(32) | 0x10000, 0x7E00 | i])
        return ((self.kinds[i] << 8) | i) & MASK

    def chunk(self, tag, payload, word_at=(), dsize=None, extra=b""):
        """One chunk: the header (tag, data size, physical size), the payload and bytes past the data. word_at: the payload offsets
        of its word reads."""
        start = 12 + len(self.body) + 12     # the save chunk's header comes first
        self.words += [start + o for o in word_at]
        d = 12 + len(payload) if dsize is None else dsize
        self.count += 1
        self.body += le(tag, d, 12 + len(payload) + len(extra)) + payload + extra

    def time(self):
        self.chunk(2, b"17:56:39Jan  8 2001\0")

    def ride(self, i=None, created=True):
        r = self.r
        i = r.randrange(NUSED) if i is None else i
        rid = self.rid(i, r.random() < 0.9)
        self.q["BMP_CreateRide"].append(rid if created else r.choice([0, rid ^ 1, -1]))
        ws = [rid, r.randrange(-900, 900), r.randrange(-900, 900), r.randrange(4), r.getrandbits(16), r.randrange(8), r.randrange(4),
              r.randrange(4), r.randrange(200), r.getrandbits(32)]
        self.q["BMP_SetRideDuration"].append(r.getrandbits(8))
        self.chunk(3, le(*ws), (0, 12, 16, 20, 24, 28, 32, 36))

    def track(self):
        r = self.r
        self.q["BMP_TrackSectionPlaceSection"].append(r.getrandbits(8))
        self.chunk(4, le(self.rid(), r.randrange(40), r.randrange(-900, 900), r.randrange(-900, 900)), (0, 4))

    def bump(self, i=None, good=True, start=None, nan=None, flags=None, target=None, cols=None):
        r = self.r
        i = r.randrange(NUSED) if i is None else i
        rid = self.rid(i, good)
        if good:
            if start is None:
                start = -1 if r.random() < 0.1 else (self.fresh if self.fresh < NBUMPS else r.randrange(NBUMPS))
            if start >= NUSED:
                self.fresh += 1
            self.q["BMP_StartBump"].append(start)
            if start >= 0:
                if cols is None:
                    cols = [r.choice([-1, 0, 1, 2, 3]), r.choice([-1, 0, 1, 2, 3])]
                self.q["PointInAnyColObject"] += cols
                self.q["RestoreRideInstance"] += [r.randrange(NSETS), r.randrange(NSETS)]
                self.q["FindBuoy"].append(r.getrandbits(8))
        w = [r.getrandbits(32) for _ in range(43)]
        w[0] = flags if flags is not None else r.choice([0, 4, 0x2000, 0x02000000, 0x00400000, 0x02402004, 0x00402000, r.getrandbits(32)])
        w[1] = r.randrange(8)
        w[0x34 // 4] = r.randrange(-900, 900) & MASK
        w[0x38 // 4] = r.randrange(-900, 900) & MASK
        if nan is None:
            nan = r.random() < 0.3
        w[0xA0 // 4] = r.choice([0x7FC00000, 0x7FA00001, 0xFFC00000]) if nan else r.choice([0, 0x3F800000, 0xC0490FDB, r.getrandbits(31)])
        if target is None:
            target = 0 if r.random() < 0.3 else (r.getrandbits(16) | 1 if r.random() < 0.2 else self.rid())
        tail = [r.randrange(-900, 900), r.randrange(-900, 900), target, r.randrange(-900, 900), r.randrange(-900, 900)]
        self.chunk(5, le(rid) + le(*w) + le(*tail), (0,) + tuple(4 + 0xAC + 4 * k for k in range(5)))

    def buoys(self, i=None, good=True):
        self.chunk(6, le(self.rid(i, good)), (0,))

    def peeps(self, tag):
        r = self.r
        if tag == 8:
            self.q["BMP_AddPeepToRideJoiningList"].append(r.choice([0, 1]))
        self.chunk(tag, le(self.rid(), r.getrandbits(32)), (0, 4))

    def link(self, i=None, good=True):
        r = self.r
        if good:
            self.links += 1
            self.q["FindDataNodeIndexByID"].append(r.getrandbits(8))
        self.chunk(9, le(self.rid(i, good), r.getrandbits(32), r.getrandbits(32)), (0, 4, 8))

    def unknown(self, tag):
        self.chunk(tag, le(*[self.r.getrandbits(32) for _ in range(3)]))

    def save(self, tag=1, psize=None, truncate=None, dsize=None):
        p = 12 + len(self.body) if psize is None else psize
        d = p if dsize is None else dsize
        data = le(tag, d, p) + self.body
        if truncate is not None:
            data = data[:truncate]
        return data


def case_of(r, f, data, magic=MAGIC, alloc_fail=-1, fails=(), free=None, kinds=None):
    q = dict(f.q)
    for k in fails:
        q["fail:%d" % k] = [1]
    return {"seed": r.getrandbits(32), "kinds": kinds or f.kinds, "magic": magic, "stream": data, "queues": q,
            "alloc_fail": alloc_fail, "pool": min(f.count + 3, 150), "free": NLINKS - 2 if free is None else free}


def new_file(r):
    return File(r, [r.choice(KINDS) for _ in range(NUSED)])


def bmp_save_file(r, f):
    """A file in the order BMP_Save writes it: the build time, then each ride's record, track sections, bumps, buoys, peeps and links."""
    f.time()
    for i in r.sample(range(NUSED), r.randrange(1, 4)):
        f.ride(i)
        for _ in range(r.randrange(3)):
            f.track()
        for _ in range(r.randrange(1, 3)):
            f.bump(i)
        f.buoys(i)
        for _ in range(r.randrange(2)):
            f.peeps(7)
        for _ in range(r.randrange(2)):
            f.peeps(8)
        for _ in range(r.randrange(3)):
            if f.links < NLINKS - 2:
                f.link(i)
    return f


def random_file(r, f, n):
    for _ in range(n):
        k = r.random()
        if k < 0.1:
            f.time()
        elif k < 0.25:
            f.ride(created=r.random() < 0.85)
        elif k < 0.35:
            f.track()
        elif k < 0.6:
            f.bump(good=r.random() < 0.9)
        elif k < 0.7:
            f.buoys(good=r.random() < 0.9)
        elif k < 0.8:
            f.peeps(r.choice([7, 8]))
        elif f.links < NLINKS - 2:
            f.link(good=r.random() < 0.85)
        else:
            f.time()
    return f


def edges():
    r = random.Random(5109)
    # no file, an empty save, a save chunk that is not one, a dead world
    f = new_file(r)
    yield case_of(r, f, b"")
    yield case_of(r, f, f.save())
    yield case_of(r, f, f.save(psize=12, dsize=40))
    for tag in (0, 2, 3, 9, 10, 0xFFFFFFFF):
        yield case_of(r, f, f.save(tag=tag))
    f = bmp_save_file(r, new_file(r))
    yield case_of(r, f, f.save(), magic=0)
    yield case_of(r, f, f.save(), magic=MAGIC + 1)
    # one chunk of each kind
    for make in (lambda f: f.time(), lambda f: f.ride(), lambda f: f.ride(created=False), lambda f: f.track(), lambda f: f.bump(),
                 lambda f: f.bump(good=False), lambda f: f.bump(start=-1), lambda f: f.buoys(), lambda f: f.buoys(good=False),
                 lambda f: f.peeps(7), lambda f: f.peeps(8), lambda f: f.link(), lambda f: f.link(good=False),
                 lambda f: f.unknown(1), lambda f: f.unknown(0), lambda f: f.unknown(10), lambda f: f.unknown(0x80000003)):
        f = new_file(r)
        make(f)
        yield case_of(r, f, f.save())
    # every buoy kind
    for kind in KINDS:
        f = File(r, [kind] * NUSED)
        f.buoys(0)
        yield case_of(r, f, f.save())
    # the high records (kind 0x7F): an id's index past 0xCE checks the division by 0xD0 that makes a record's id
    f = new_file(r)
    for i in (0xCE, 0xCF, 0xE0, 0xFF):
        f.chunk(6, le((0x7F << 8) | i), (0,))
    yield case_of(r, f, f.save())
    # the bump's branches: the flags, the heading, the target, the collision objects
    for flags in (0, 4, 0x2000, 0x02000000, 0x00400000, 0x02402004, 0xFFFFFFFF):
        for nan in (False, True):
            f = new_file(r)
            f.bump(0, start=NUSED, nan=nan, flags=flags, target=0 if flags & 1 else None, cols=[-1, 2] if nan else [1, -1])
            yield case_of(r, f, f.save())
    for cols in ([-1, -1], [0, 1], [-1, 3]):
        f = new_file(r)
        f.bump(1, start=1, cols=cols, target=0)
        yield case_of(r, f, f.save())
    # links: each from the free list onto the ride's bump, after the bump's own load
    f = new_file(r)
    f.bump(2, start=NUSED)
    for _ in range(4):
        f.link(2)
    f.link(3)
    yield case_of(r, f, f.save())
    yield case_of(r, f, f.save(), free=5)
    # whole files as BMP_Save writes them
    for _ in range(6):
        f = bmp_save_file(r, new_file(r))
        yield case_of(r, f, f.save())
    # the save's size off, the file cut short, a chunk's data size off
    f = bmp_save_file(r, new_file(r))
    data = f.save()
    yield case_of(r, f, f.save(psize=len(data) + 12))
    yield case_of(r, f, f.save(psize=len(data) - 12))
    for cut in (8, 20, 40, len(data) // 2, len(data) - 3):
        yield case_of(r, f, f.save(truncate=cut))
    for dsize in (12, 16, 30, 0x200):
        f = new_file(r)
        f.chunk(3, le(f.rid(0), 1, 2, 3, 4, 5, 6, 7, 8, 9), (0, 12, 16, 20, 24, 28, 32, 36), dsize=dsize)
        f.q["BMP_CreateRide"].append(f.rid(0))
        f.chunk(5, le(f.rid(1)) + bytes(0xAC) + le(1, 2, 0, 3, 4), (0, 0xB0, 0xB4, 0xB8, 0xBC, 0xC0), dsize=dsize + 0x40)
        f.q["BMP_StartBump"].append(NUSED)
        f.q["PointInAnyColObject"] += [-1, -1]
        f.chunk(9, le(f.rid(1), 5, 6), (0, 4, 8), dsize=dsize, extra=b"junk")
        f.links += 1
        yield case_of(r, f, f.save())
    # an allocation failing at each chunk
    f = bmp_save_file(r, new_file(r))
    data = f.save()
    for k in (1, 2, 3, 5, 8):
        yield case_of(r, f, data, alloc_fail=k)
    # a word read failing at each place
    for _ in range(4):
        f = bmp_save_file(r, new_file(r))
        data = f.save()
        for k in f.words[:: max(1, len(f.words) // 8)]:
            yield case_of(r, f, data, fails=[k])


def rnd(r):
    f = new_file(r)
    if r.random() < 0.5:
        bmp_save_file(r, f)
    else:
        random_file(r, f, r.randrange(1, 14))
    if r.random() < 0.1:
        f.unknown(r.choice([0, 1, 10, r.getrandbits(32)]))
    k = r.random()
    if k < 0.08:
        data = f.save(psize=r.randrange(0, 600))
    elif k < 0.15:
        data = f.save(truncate=r.randrange(0, len(f.body) + 13))
    else:
        data = f.save()
    fails = r.sample(f.words, min(len(f.words), r.choice([0, 0, 0, 1, 2]))) if f.words else []
    alloc_fail = r.randrange(1, 20) if r.random() < 0.1 else -1
    return case_of(r, f, data, magic=MAGIC if r.random() < 0.97 else r.getrandbits(32), alloc_fail=alloc_fail, fails=fails)


TESTS = [FnTest("BMP_Load", ppc, host, edge=edges, rnd=rnd)]
