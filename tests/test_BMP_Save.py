"""Unit v5630, the advisor save BMP_Save (writes the bump world as chunks to a file), against libtpw_unit_BMP_Save.dylib
(tests/unit_BMP_Save.cpp), main app ("SimTheme Park"), src/game/rides/BumperRide.cpp:
  BMP_Save 0x10022464

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit, each a fake on both sides that records its arguments and answers from the case's queues: NS_MacDoze::GlobalAlloc
(flags, size; hands out the case's pool blocks in order, or NULL), LbFile_GetPosition (the file), LbFile_Write (the file, the length and
the buffer's bytes as big-endian words, the file's bytes on both sides; answers the TbError), Write_CloseChunk and
Write_BumpToChunk (the chunk's six fields, the bump). strlen is the standard stub (the PowerPC side) and the C library's (the host).

The world of a case: the global 0x102b56c0 (magic, ride records, their live count, bumps, their live count, the table), a block of ride
records (two records before the array so table entries can point before it, unaligned too), their lists (0xbc, 0xc4, 0xc8), the bumps
(flags, owner, collision object, table index -1 or an entry, their 0x30 list) and the table (one entry before it for index -1). What is
compared: the answer, the event log, the pool blocks (every chunk's six words, the file field as a label) and the global.

No fused operations. Not tested: a NULL from GlobalAlloc after the first two chunks (the original then reads and writes through the NULL
chunk), counts larger than the live records or bumps present (the walks then run past the arrays), NULL collision objects or table
rides (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_BMP_Save"
MASK = 0xFFFFFFFF

# ---- the unit's own function (address, size from the MANIFEST)
FN_SAVE = 0x10022464
SIZES = {FN_SAVE: 5068}
OWN = set(SIZES)

# ---- the callees faked on both sides (glue addresses for the imports, in-binary addresses for the others)
GLOBAL_ALLOC = 0x10239610
LBFILE_GETPOSITION = 0x102398E0
LBFILE_WRITE = 0x10238878
WRITE_CLOSECHUNK = 0x10021E44
WRITE_BUMPTOCHUNK = 0x10021960
STRLEN = 0x10238650

# ---- the Mac's addresses of the globals; the host library's come from tpw_bs_addr(name)
ADDR = dict(world=0x102B56C0)
TEXTS = (0x1023CBFA, 0x1023CC03)   # the build time and date the original writes
# ---- the host's offsets and sizes the module relies on, checked once against tpw_bs_layout
HOST_LAYOUT = [0x18, 0x04, 0x0C, 0x24, 0x20, 0x28, 0x24, 0x14, 0x10, 0x1C, 0x10, 0x98, 0x9C]

MAGIC = 0x4A454647
PRE = 2            # ride records before the array
POOL = 80          # chunk blocks per case
FILE_LABEL, POOL_LABEL, BUMP_LABEL = 1, 200, 300


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_global_alloc(e):
    """InternalHandle* NS_MacDoze::GlobalAlloc(uint32_t flags, uint32_t size): the next pool block, NULL when the queue says 0."""
    L = e.cl
    L.rec("GlobalAlloc", (e.r(3), e.r(4)))
    if L.ret("GlobalAlloc") == 0:
        e.set_r(3, 0)
        return
    side = e.bs_side
    if side.pool_next >= len(side.pool):
        L.rec("POOL_EXHAUSTED", ())
        e.set_r(3, 0)
        return
    e.set_r(3, side.pool[side.pool_next])
    side.pool_next += 1


def stub_get_position(e):
    """int32_t LbFile_GetPosition(void* file)."""
    L = e.cl
    L.rec("LbFile_GetPosition", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("LbFile_GetPosition") & MASK)


def buffer_words(data, text, fmt):
    data = data + b"\0" * (-len(data) % 4)
    return list(struct.unpack((">" if text else fmt[0]) + "%dI" % (len(data) // 4), data))


def stub_write(e):
    """TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred): the result through r3."""
    L = e.cl
    buf, length = e.r(5), e.r(6)
    text = buf in TEXTS
    words = buffer_words(e.read(buf, min(length, 12)), text, ">")
    L.rec("LbFile_Write", tuple([L.lab(e.r(4)), length, 1 if text else 0] + words))
    e.write(e.r(3), struct.pack(">I", L.ret("LbFile_Write") & MASK))


def chunk_fields(e, c):
    L = e.cl
    w = list(struct.unpack(">6I", e.read(c, 0x18)))
    return [s32(w[0]), L.lab(w[1]), w[2], w[3], s32(w[4]), s32(w[5])]


def stub_close_chunk(e):
    """int32_t Write_CloseChunk(tabBMPCHUNK* chunk)."""
    L = e.cl
    c = e.r(3)
    L.rec("Write_CloseChunk", tuple([L.lab(c)] + chunk_fields(e, c)))
    e.set_r(3, L.ret("Write_CloseChunk") & MASK)


def stub_bump_to_chunk(e):
    """uint32_t Write_BumpToChunk(tabBMPCHUNK* chunk, tagBUMP& bump)."""
    L = e.cl
    c = e.r(3)
    L.rec("Write_BumpToChunk", tuple([L.lab(c), L.lab(e.r(4))] + chunk_fields(e, c)))
    e.set_r(3, L.ret("Write_BumpToChunk") & MASK)


def stub_strlen(e):
    """size_t strlen(const char*): C semantics (the call goes through the glue at 0x10238650)."""
    a, n = e.r(3), 0
    while e.read(a + n, 1) != b"\0":
        n += 1
    e.set_r(3, n)


STUBS.internal(STRLEN, stub_strlen)
STUBS.internal(GLOBAL_ALLOC, stub_global_alloc)
STUBS.internal(LBFILE_GETPOSITION, stub_get_position)
STUBS.internal(LBFILE_WRITE, stub_write)
STUBS.internal(WRITE_CLOSECHUNK, stub_close_chunk)
STUBS.internal(WRITE_BUMPTOCHUNK, stub_bump_to_chunk)


def setup(runner):
    T.install_std_stubs(runner.emu)       # strlen


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.saved = None
        e.bs_side = self

    def begin(self, labels, pool):
        self.pool, self.pool_next = pool, 0
        self.L = self.e.cl = CL.PpcLog(dict(labels), self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def ptr(self, a):
        return a

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, file):
        r3, _, _ = T.call_ppc(self.e, FN_SAVE, [file])
        return s32(r3)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        lib.tpw_bs_alloc(4)                  # the arena is reserved by its first allocation
        self.base = lib.tpw_bs_arena_base()

    def begin(self, labels, pool):
        arr = (C.c_uint64 * len(pool))(*pool)
        self.lib.tpw_bs_pool(arr, len(pool))
        CL.host_begin(self.lib, {lab: a for a, lab in labels.items()}, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bs_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bs_alloc(n)

    def ptr(self, a):
        return a - self.base if a else 0

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, file):
        return self.lib.tpw_bs_save(file)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_bs_addr.restype, lib.tpw_bs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_bs_alloc.restype, lib.tpw_bs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_bs_arena_base.restype, lib.tpw_bs_arena_base.argtypes = C.c_uint64, []
    lib.tpw_bs_pool.restype, lib.tpw_bs_pool.argtypes = None, [C.POINTER(C.c_uint64), C.c_int32]
    lib.tpw_bs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_bs_save.restype, lib.tpw_bs_save.argtypes = C.c_int32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_bs_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_list(s, nodes, size, next_word):
    """A linked list of nodes (each a list of words); answers the first node's address (0 for none)."""
    addrs = [s.alloc(size) for _ in nodes]
    for i, words in enumerate(nodes):
        ws = list(words)
        ws[next_word] = s.ptr(addrs[i + 1]) if i + 1 < len(addrs) else 0
        s.words(addrs[i], ws)
    return addrs[0] if addrs else 0


def build(s, case):
    """Writes the world through Side.words; answers the labels {address: label}, the pool and the file."""
    w = case["world"]
    file = s.alloc(16)
    pool = [s.alloc(0x18) for _ in range(POOL)]
    labels = {file: FILE_LABEL}
    for i, p in enumerate(pool):
        labels[p] = POOL_LABEL + i

    rides = w["rides"]
    block = s.alloc((PRE + len(rides)) * 0xD0)
    base = block + PRE * 0xD0
    s.words(block, w["pre"])
    for i, ride in enumerate(rides):
        ws = list(ride["words"])
        ws[0] = ride["kind"]
        ws[0xBC // 4] = s.ptr(build_list(s, ride["tracks"], 0x28, 9))
        ws[0xC4 // 4] = s.ptr(build_list(s, ride["c4"], 0x14, 4))
        ws[0xC8 // 4] = s.ptr(build_list(s, ride["c8"], 0x14, 4))
        s.words(base + i * 0xD0, ws)

    table_block = s.alloc((len(w["table"]) + 1) * 0x1C)
    table = table_block + 0x1C
    s.words(table_block, w["table_pre"])
    for i, entry in enumerate(w["table"]):
        ws = list(entry["words"])
        ws[4] = s.ptr(base + entry["ride"])
        s.words(table + i * 0x1C, ws)

    bumps = w["bumps"]
    bump_block = s.alloc(max(1, len(bumps)) * 0xAC) if bumps else 0
    for i, bump in enumerate(bumps):
        a = bump_block + i * 0xAC
        labels[a] = BUMP_LABEL + i
        col = s.alloc(12)
        s.words(col, bump["col"])
        ws = list(bump["words"])
        ws[0] = bump["flags"]
        ws[0x30 // 4] = s.ptr(build_list(s, bump["links"], 0x14, 4))
        ws[0x7C // 4] = bump["index"]
        ws[0x98 // 4] = s.ptr(col)
        ws[0x9C // 4] = s.ptr(base + bump["owner"]) if bump["owner"] is not None else 0
        s.words(a, ws)

    g = s.g("world")
    s.world_words = [w["magic"], w["w04"], 0 if w["rides_null"] else s.ptr(base), w["ride_count"], s.ptr(bump_block),
                     w["bump_count"], w["w18"], w["w1c"], s.ptr(table)]
    s.words(g, s.world_words)
    s.labels, s.pool_addrs, s.file = labels, pool, file
    s.begin(labels, pool)


def execute(s, case):
    return s.call(s.file)


def observe(s, case, answer):
    fw = s.ptr(s.file)
    chunks = []
    for p in s.pool_addrs:
        ws = s.rd_words(p, 6)
        ws[1] = FILE_LABEL if ws[1] == fw and fw else (0 if ws[1] == 0 else -1)
        chunks.append(ws)
    world = s.rd_words(s.g("world"), 9)
    for i in (2, 4, 8):                  # the pointer words: unchanged or not (the addresses differ between the memories)
        world[i] = "same" if world[i] == s.world_words[i] & MASK else "changed"
    return {"answer": answer, "log": s.log(), "chunks": chunks, "world": world}


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


def rw(r, n):
    return [r.getrandbits(32) for _ in range(n)]


def rval(r):
    return r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, r.randrange(-1000, 1000), r.getrandbits(32)]) & MASK


def gen_world(r, nrides=None, kinds=None, ride_count=None, nbumps=None, magic=MAGIC, rides_null=False, lists=True, bump_count=None,
              index=None, table_ride=None):
    if nrides is None:
        nrides = r.randrange(0, 5)
    rides = []
    for i in range(nrides):
        kind = kinds[i] if kinds is not None else (0 if r.random() < 0.3 else rval(r) or 1)
        rides.append(dict(words=rw(r, 52), kind=kind,
                          tracks=[rw(r, 10) for _ in range(r.randrange(0, 4) if lists else 0)],
                          c4=[rw(r, 5) for _ in range(r.randrange(0, 3) if lists else 0)],
                          c8=[rw(r, 5) for _ in range(r.randrange(0, 3) if lists else 0)]))
    live = [i for i, ride in enumerate(rides) if ride["kind"] != 0]
    if ride_count is None:
        ride_count = len(live) if r.random() < 0.8 else r.randrange(0, len(live) + 1)
    ntable = r.randrange(1, 4)
    lo, hi = -PRE * 0xD0, max(nrides, 0) * 0xD0

    def ride_ref():
        if table_ride is not None:
            return table_ride
        if r.random() < 0.5 and nrides:
            return r.randrange(0, nrides) * 0xD0
        if r.random() < 0.3:
            return r.randrange(-PRE, max(nrides, 1)) * 0xD0 if nrides else r.randrange(-PRE, 0) * 0xD0
        return r.randrange(lo, hi, 4)

    table = [dict(words=rw(r, 7), ride=ride_ref()) for _ in range(ntable)]
    if nbumps is None:
        nbumps = r.randrange(0, 7)
    bumps = []
    for i in range(nbumps):
        flags = r.getrandbits(32)
        flags = (flags | 1) if r.random() < 0.65 else (flags & ~1)
        k = r.random()
        if k < 0.6 and live:
            owner = r.choice(live) * 0xD0
        elif k < 0.75 and nrides:
            owner = r.randrange(0, nrides) * 0xD0
        elif k < 0.85:
            owner = -0xD0
        else:
            owner = None
        idx = index if index is not None else (-1 if r.random() < 0.3 else r.randrange(0, ntable))
        bumps.append(dict(words=rw(r, 43), flags=flags & MASK, owner=owner, col=rw(r, 3), index=idx & MASK,
                          links=[rw(r, 5) for _ in range(r.randrange(0, 3) if lists else 0)]))
    flagged = sum(1 for b in bumps if b["flags"] & 1)
    if bump_count is None:
        bump_count = flagged if r.random() < 0.8 else r.randrange(0, flagged + 1)
    return dict(magic=magic, w04=r.getrandbits(32), w18=r.getrandbits(32), w1c=r.getrandbits(32), rides_null=rides_null,
                pre=rw(r, PRE * 52), rides=rides, ride_count=ride_count, table_pre=rw(r, 7), table=table, bumps=bumps,
                bump_count=min(bump_count, flagged))


QUEUE = 48   # values per answer queue (the last one repeats)


def queues(r, alloc=None, writes=None):
    if writes is None:
        k = r.random()
        if k < 0.15:
            writes = [0]
        elif k < 0.25:
            writes = [-1]
        else:
            writes = [r.choice([0, 0, 0, 1, 12, -1, -2, 0x7FFFFFFF, -0x80000000]) for _ in range(QUEUE)]
    return {"GlobalAlloc": alloc if alloc is not None else ([1] if r.random() < 0.9 else [r.choice([0, 1]), r.choice([0, 1]), 1]),
            "LbFile_GetPosition": [r.choice([0, -1, r.randrange(0, 1 << 20), r.getrandbits(32) - (1 << 31)]) for _ in range(60)],
            "LbFile_Write": writes,
            "Write_CloseChunk": [r.getrandbits(32) - (1 << 31) for _ in range(60)],
            "Write_BumpToChunk": [r.choice([0, 0xAC, r.getrandbits(32)]) for _ in range(10)]}


def save_case(r, alloc=None, writes=None, **kw):
    return {"world": gen_world(r, **kw), "queues": queues(r, alloc, writes)}


def save_edges():
    r = random.Random(5630)
    for magic in (0, MAGIC - 1, MAGIC + 1, MAGIC ^ 0x10000, MAGIC ^ 0x80000000):
        yield save_case(r, magic=magic)
    yield save_case(r, alloc=[0])                      # the save chunk fails: answers 0
    yield save_case(r, alloc=[1, 0, 1])                # the time chunk fails: only the save chunk closes
    yield save_case(r, alloc=[0, 1])
    yield save_case(r, rides_null=True)
    yield save_case(r, nrides=0)
    yield save_case(r, nrides=3, kinds=[0, 0, 0], ride_count=0)
    yield save_case(r, nrides=1, kinds=[5], lists=False, nbumps=0)
    for writes in ([0], [-1], [-0x80000000], [0x7FFFFFFF], [-1, 0], [0, -1]):
        yield save_case(r, writes=writes, nrides=3, nbumps=5)
    yield save_case(r, nrides=4, kinds=[1, 2, 3, 4], ride_count=2)
    yield save_case(r, nrides=4, kinds=[0, 7, 0, 9])
    for index in (-1, 0):
        yield save_case(r, nrides=3, kinds=[1, 2, 3], nbumps=6, index=index)
    for ref in (-PRE * 0xD0, -0xD0, -0xD0 - 4, -0xD0 + 4, -4, 0, 4, 0xCC, 0xD0, 0xD4, 2 * 0xD0 - 4):
        yield save_case(r, nrides=3, kinds=[0x12, 0x80000001, 0xFFFFFFFF], nbumps=3, index=0, table_ride=ref)
    for _ in range(10):
        yield save_case(r, nrides=4, nbumps=6)


def save_random(r):
    return save_case(r)


TESTS = [
    FnTest("BMP_Save", ppc, host, edge=save_edges, rnd=save_random),
]
