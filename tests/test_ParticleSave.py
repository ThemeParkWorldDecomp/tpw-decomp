"""Unit w168, the particle system's save and load, against libtpw_unit_ParticleSave.dylib (tests/unit_ParticleSave.cpp), main app
("SimTheme Park"), src/game/Particles.cpp:
  PAR_SaveStatus 0x100BACAC, PAR_LoadStatus 0x100BB7D8
(the swap helpers and tagPARTICLE::Byteswap are tests/test_ParticleSwap.py's).

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The state, the library and the pool are made
as bytes in the Mac's order (the "canonical" image: the state's 0x9e68 bytes, the library's 0x8b60, the pool's records of 0x34) and put
into each memory with the fields turned by the runs tables of test_Particles.py. The callees outside the unit (the allocator, the file
reads and writes, the three SamsUtilities copy calls) are fakes on both sides that record their arguments; the allocator answers from the
case's queue "alloc" (0 fails) and a new block holds 0xa5 bytes; a read serves the case's stream (little-endian, as a file) and is zero
past its end.

Compared: the answer, the call log, the written stream byte for byte (save; the image's three pointer words, which the host writes as 0
and the Mac as its own pointers, at stream offsets 0x28, 0x30 and 0x34, are zeroed on both sides), the state's bytes (the pointer words
masked: the host's header holds native pointers, so the pool's pointer is compared as a label, and the sine table's and height callback's
as "unchanged", only where the answer is 0, because after a failed load the Mac holds the file's words and the host keeps its own), the
library and the pool (when the load answered 0). No fused operations, no floats. Not tested: a maximum of particles of 0 or below
(the original crashes or asserts), a pool size word above the allocated block (it overruns). The library is built without
TPW_SWAP_FILE_WORDS, so a swap that PAR_SaveStatus or PAR_LoadStatus forgot cannot show here on a little-endian host (the helpers'
own field lists are tests/test_ParticleSwap.py's).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
import test_Particles as TP

SHIM_LIBRARY = "unit_ParticleSave"
MASK = 0xFFFFFFFF

SAVE, LOAD = 0x100BACAC, 0x100BB7D8
ASSERT = 0x1000DDE8
SIZES = {SAVE: 1084, LOAD: 1108}
OWN = set(SIZES)

STATE, LIB, RUNNING = 0x102EDD68, 0x102E5208, 0x102E51FC
STATE_SIZE, LIB_SIZE, PARTICLE = 0x9E68, 0x8B60, 0x34
MAGIC = 0x5054434C
FILE = 0x4321
SIN_MARK, CB_MARK = 0x11110000, 0x22220000        # the host's sine table and callback pointers, set before the call
HOST_LAYOUT = [0x2C + 0, 0x9E4C + 0, 0x9E68 + 0, 0x140, 0x68, 0x8B60, 0x34]

HEADER_RUNS = [(0, 4), (4, 4), (8, 4), (0xC, 4), (0x10, 4), (0x14, 4), (0x18, 2), (0x1A, 2), (0x1C, 4), (0x20, 4), (0x24, 4), (0x28, 4)]
PARTICLE_RUNS = [(o, 4) for o in range(0, 0x24, 4)] + [(o, 2) for o in range(0x24, 0x34, 2)]


def state_runs(b):
    """The canonical (big-endian) state image with every field turned: the little-endian image the file and the host use."""
    out = bytearray(b)
    out[0:0x2C] = TP.swap_runs(bytes(b[0:0x2C]), HEADER_RUNS, 0x2C)
    out[0x2C:0x962C] = TP.swap_runs(bytes(b[0x2C:0x962C]), TP.EMITTER_RUNS, 0x140)
    out[0x962C:0x9E4C] = TP.swap_runs(bytes(b[0x962C:0x9E4C]), TP.EFFECTER_RUNS, 0x68)
    out[0x9E4C:0x9E56] = TP.swap_runs(bytes(b[0x9E4C:0x9E56]), TP.HEAD_RUNS, 10)
    return bytes(out)


def library_runs(b):
    return TP.swap_runs(b[:0x8340], TP.EMITTER_RUNS, 0x140) + TP.swap_runs(b[0x8340:], TP.EFFECTER_RUNS, 0x68)


def pool_runs(b):
    return TP.swap_runs(b, PARTICLE_RUNS, PARTICLE)


def mask_pointers(le):
    """A little-endian state image with the three pointer words (+0x1c, +0x24, +0x28) zeroed."""
    out = bytearray(le)
    for o in (0x1C, 0x24, 0x28):
        out[o:o + 4] = bytes(4)
    return bytes(out)


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_alloc(e):
    L = e.cl
    size = e.r(3)
    if L.ret("alloc", 1) == 0:
        L.rec("LbMem_Alloc", (size, 0))
        e.set_r(3, 0)
        return
    a = e.alloc(max(size, 1), b"\xa5" * max(size, 1))
    L.rec("LbMem_Alloc", (size, L.newlab(a)))
    e.set_r(3, a)


def stub_free(e):
    e.cl.rec("LbMem_Free", (e.cl.lab(e.r(3)),))


def stub_read(e):
    L, buffer, length = e.cl, e.r(5), e.r(6)
    L.rec("LbFile_Read", (e.r(4), length, 1 if e.r(7) else 0))
    chunk = L.file_in[L.file_pos:L.file_pos + length]
    L.file_pos += length
    if length:
        e.write(buffer, chunk + bytes(length - len(chunk)))
    e.write(e.r(3), bytes(4))


def stub_write(e):
    L, buffer, length = e.cl, e.r(5), e.r(6)
    L.rec("LbFile_Write", (e.r(4), length, 1 if e.r(7) else 0))
    L.file_out.append(e.read(buffer, length) if length else b"")
    e.write(e.r(3), bytes(4))


def swap_words(data):
    return b"".join(data[i:i + 4][::-1] for i in range(0, len(data) - len(data) % 4, 4))


def stub_swap_block(e):
    words, count = e.r(3), s32(e.r(4))
    e.cl.rec("USwapBlock32", (count,))
    if count > 0:
        e.write(words, swap_words(e.read(words, 4 * count)))


def stub_swap_copy(e):
    L = e.cl
    words, count = e.r(3), s32(e.r(4))
    data = e.read(words, 4 * count) if count > 0 else b""
    a = e.alloc(max(4 * count, 4), swap_words(data) if count > 0 else bytes(4))
    L.rec("USwapCopy32", (count, e.u32(words) if count > 0 else 0, L.newlab(a)))
    e.set_r(3, a)


def stub_swap_delete(e):
    e.cl.rec("USwapDelete", (e.cl.lab(e.r(3)),))


def stub_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),))


STUBS.internal(ASSERT, stub_assert)
STUBS.imp("LbMem_Alloc__FUi", stub_alloc)
STUBS.imp("LbMem_Free__FPv", stub_free)
STUBS.imp("LbFile_Read__FPvPvUlPUl", stub_read)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", stub_write)
STUBS.imp("_USwapBlock32__13SamsUtilitiesFPUll", stub_swap_block)
STUBS.imp("_USwapCopy32__13SamsUtilitiesFPUll", stub_swap_copy)
STUBS.imp("_USwapDelete__13SamsUtilitiesFPv", stub_swap_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)       # memset, memcpy


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.L.file_in, self.L.file_pos, self.L.file_out = case.get("stream", b""), 0, []
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(max(n, 4))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def label(self, l, a):
        self.L.labels[a] = l

    def set_running(self, v):
        self.words(RUNNING, [v])

    def set_state(self, img, pool):
        b = bytearray(img)
        b[0x28:0x2C] = struct.pack(">I", pool)
        self.write(STATE, bytes(b))

    def get_state(self):
        return state_runs(self.read(STATE, STATE_SIZE))

    def ptrs(self, case):
        w = lambda o: struct.unpack(">I", self.read(STATE + o, 4))[0]
        return w(0x28), w(0x1C) == struct.unpack(">I", case["state"][0x1C:0x20])[0], w(0x24) == struct.unpack(">I", case["state"][0x24:0x28])[0]

    def pool_label(self, p):
        return self.L.lab(p) if p else 0

    def set_lib(self, lib):
        self.write(LIB, lib)

    def get_lib(self):
        return library_runs(self.read(LIB, LIB_SIZE))

    def get_pool(self, p, n):
        return pool_runs(self.read(p, n))

    def written(self):
        return b"".join(self.L.file_out)

    def log(self):
        return self.L.log

    def call(self, case):
        if case["op"] == "save":
            r3, _, _ = T.call_ppc(self.e, SAVE, [FILE, case["wp"] & MASK])
        else:
            r3, _, _ = T.call_ppc(self.e, LOAD, [FILE])
        return s32(r3)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])
        data = case.get("stream", b"")
        lib.tpw_px_file_in(data, len(data))

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_px_alloc(max(n, 4))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def label(self, l, a):
        self.lib.tpw_cl_label(l, a)

    def set_running(self, v):
        self.words(self.lib.tpw_px_addr(b"running"), [v])

    def set_state(self, img, pool):
        self.lib.tpw_px_state_set(state_runs(img))
        self.lib.tpw_px_set_ptrs(pool, SIN_MARK, CB_MARK)

    def get_state(self):
        out = (C.c_uint8 * STATE_SIZE)()
        self.lib.tpw_px_state_get(out)
        return bytes(out)

    def ptrs(self, case):
        out = (C.c_uint64 * 3)()
        self.lib.tpw_px_get_ptrs(out)
        return out[0], out[1] == SIN_MARK, out[2] == CB_MARK

    def pool_label(self, p):
        return self.lib.tpw_cl_lab(p) if p else 0

    def set_lib(self, lib):
        self.write(self.lib.tpw_px_addr(b"library"), library_runs(lib))

    def get_lib(self):
        return self.read(self.lib.tpw_px_addr(b"library"), LIB_SIZE)

    def get_pool(self, p, n):
        return self.read(p, n)

    def written(self):
        n = self.lib.tpw_px_out_length()
        out = (C.c_uint8 * max(n, 1))()
        self.lib.tpw_px_out_data(out)
        return bytes(out)[:n]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, case):
        if case["op"] == "save":
            return self.lib.tpw_px_save(FILE, case["wp"])
        return self.lib.tpw_px_load(FILE)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_px_addr.restype, lib.tpw_px_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_px_alloc.restype, lib.tpw_px_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_px_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_px_state_set.argtypes = [C.c_char_p]
    lib.tpw_px_state_get.argtypes = [C.POINTER(C.c_uint8)]
    lib.tpw_px_set_ptrs.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_px_get_ptrs.argtypes = [C.POINTER(C.c_uint64)]
    lib.tpw_px_file_in.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_px_out_length.restype = C.c_int32
    lib.tpw_px_out_data.argtypes = [C.POINTER(C.c_uint8)]
    lib.tpw_px_save.restype, lib.tpw_px_save.argtypes = C.c_int32, [C.c_uint64, C.c_int32]
    lib.tpw_px_load.restype, lib.tpw_px_load.argtypes = C.c_int32, [C.c_uint64]
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    lib.tpw_cl_lab.restype, lib.tpw_cl_lab.argtypes = C.c_int32, [C.c_void_p]
    out = (C.c_int64 * 32)()
    n = lib.tpw_px_layout(out)
    got = list(out)[:n]
    # [offsetof mEmitters, offsetof mEmittersUsed, sizeof PARState] are the host's own; the distances and the sizes are the Mac's (the struct is padded to 8)
    assert got[1] - got[0] == 0x9E4C - 0x2C and 0 <= got[2] - got[0] - (STATE_SIZE - 0x2C) < 8 and got[3:] == HOST_LAYOUT[3:], got
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


POOL_LABEL = 5


def build(s, case):
    s.set_running(case["running"])
    state = case["state"]
    max_p = struct.unpack(">i", state[0xC:0x10])[0]
    pool = s.alloc(max(max_p, 1) * PARTICLE)
    s.label(POOL_LABEL, pool)
    s.write(pool, case["pool"] if s.fmt == ">I" else pool_runs(case["pool"]))
    s.set_state(state, pool)
    s.set_lib(case["lib"])
    return pool


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log(), "state": mask_pointers(s.get_state()), "lib": s.get_lib()}
    pool, same_sin, same_cb = s.ptrs(case)
    if case["op"] == "save":
        w = bytearray(s.written())
        for o in (0x28, 0x30, 0x34):
            if len(w) >= o + 4:
                w[o:o + 4] = bytes(4)
        out["written"] = bytes(w)
    if answer == 0:
        out["pool"] = s.pool_label(pool)
        out["sin"], out["callback"] = same_sin, same_cb
        if pool:
            n = struct.unpack("<i", out["state"][0xC:0x10])[0]
            out["poolbytes"] = s.get_pool(pool, max(n, 0) * PARTICLE)
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


class PCase(dict):
    def __repr__(self):
        return self["label"]


def rbytes(r, n):
    return r.randbytes(n)


def be_state(r, max_p, chain=None):
    """A canonical state: random bytes with the maximum in place and, with `chain` (a list of emitter numbers), the emitters-in-use chain."""
    b = bytearray(rbytes(r, STATE_SIZE))
    b[0xC:0x10] = struct.pack(">i", max_p)
    if chain is not None:
        for i, idx in enumerate(chain):
            nxt = chain[i + 1] if i + 1 < len(chain) else -1
            b[0x2C + idx * 0x140 + 0xD0:0x2C + idx * 0x140 + 0xD2] = struct.pack(">h", nxt)
        b[0x9E4C:0x9E4E] = struct.pack(">h", chain[0] if chain else -1)
    return bytes(b)


def maxes(r):
    return r.choice([1, 2, 3, 4, r.randrange(1, 301), r.randrange(1, 301)])


def base(r, op, label, running=1):
    m = maxes(r)
    return PCase(op=op, label=label, running=running, queues={}, wp=0, state=be_state(r, m), lib=rbytes(r, LIB_SIZE),
                 pool=rbytes(r, m * PARTICLE), max=m)


def save_case(r, label, running=1, wp=1, alloc=None):
    c = base(r, "save", label, running)
    c["wp"] = wp
    if alloc is not None:
        c["queues"] = {"alloc": alloc}
    return c


def chain_of(r, k):
    return r.sample(range(120), k)


def file_stream(r, c, magic=MAGIC, flag=1, ssize=STATE_SIZE, lsize=LIB_SIZE, psize=None, parts=8):
    """The little-endian file: magic, flag, state size, state, library size, library, pool size, pool (cut after `parts` of them)."""
    pool = c["file_pool"]
    if psize is None:
        psize = len(pool)
    items = [struct.pack("<I", magic), struct.pack("<I", flag), struct.pack("<I", ssize), state_runs(c["file_state"]),
             struct.pack("<I", lsize), library_runs(c["file_lib"]), struct.pack("<I", psize), pool_runs(pool)[:max(psize, 0) if psize < len(pool) else len(pool)]]
    return b"".join(items[:parts])


def load_case(r, label, running=1, k=None, fmax=None, full=False, **kw):
    c = base(r, "load", label, running)
    fm = maxes(r) if fmax is None else fmax
    k = r.randrange(0, 6) if k is None else k
    c["file_state"] = be_state(r, fm, chain_of(r, k))
    c["file_lib"] = rbytes(r, LIB_SIZE)
    c["file_pool"] = rbytes(r, fm * PARTICLE)
    if not full and "psize" not in kw:
        kw["psize"] = 0
    c["stream"] = file_stream(r, c, **kw)
    return c


def save_edges():
    r = random.Random(1681)
    yield save_case(r, "not running", running=0, wp=1)
    yield save_case(r, "not running no particles", running=0, wp=0)
    for wp in (0, 1):
        yield save_case(r, "run wp%d" % wp, wp=wp)
    for m in (1, 2, 3):
        c = save_case(r, "run max%d" % m)
        c["state"] = be_state(r, m)
        c["pool"] = rbytes(r, m * PARTICLE)
        yield c
    for q in ([0], [1, 0], [1, 1, 0], [1, 1, 1]):
        yield save_case(r, "alloc %s" % q, alloc=q)
        yield save_case(r, "alloc nopool %s" % q, alloc=q, wp=0)


def save_random(r):
    return save_case(r, "random", running=r.choice([1, 1, 1, 0]), wp=r.choice([0, 1]),
                     alloc=r.choice([None, None, None, [0], [1, 0], [1, 1, 0]]))


def load_edges():
    r = random.Random(1682)
    yield load_case(r, "not running", running=0)
    yield load_case(r, "wrong magic", magic=0x5054434D)
    yield load_case(r, "flag 0", flag=0)
    yield load_case(r, "state size", ssize=0x9E6C)
    yield load_case(r, "library size", lsize=0x8B64)
    yield load_case(r, "empty file", parts=0)
    for parts in range(1, 8):
        yield load_case(r, "cut at %d" % parts, parts=parts)
    for fm in (1, 2, 3, 4):
        for k in (0, 1, 5):
            yield load_case(r, "empty pool max%d chain%d" % (fm, k), fmax=fm, k=k)
    yield load_case(r, "full pool", full=True)
    yield load_case(r, "full pool max1", full=True, fmax=1)
    yield load_case(r, "pool smaller", full=True, psize=0x34)
    yield load_case(r, "flag 2", flag=2)


def load_random(r):
    kind = r.random()
    if kind < 0.1:
        return load_case(r, "random cut", parts=r.randrange(0, 8))
    if kind < 0.2:
        return load_case(r, "random bad size", **r.choice([{"ssize": r.randrange(1 << 32)}, {"lsize": r.randrange(1 << 32)}]))
    return load_case(r, "random", running=r.choice([1, 1, 1, 0]), full=r.random() < 0.5)


TESTS = [
    FnTest("PAR_SaveStatus", ppc, host, edge=save_edges, rnd=save_random),
    FnTest("PAR_LoadStatus", ppc, host, edge=load_edges, rnd=load_random),
]
