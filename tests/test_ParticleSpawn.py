"""Unit w26b: the particle system's spawning, against libtpw_unit_ParticleSpawn.dylib (tests/unit_ParticleSpawn.cpp):
SpawnParticle, PAR_InitEmitter, PAR_InitEmitterLaunch, PAR_InitEffecter, DoSpecialEmitter and SpecialEmitter1 (src/game/Particles.cpp).

A case is a SCRIPT: a world (the pool's length, the system's running and low-spec state, the seed and serials, pokes into the
emitter and effecter definitions of the library) and a list of steps (start an emitter, start one with a launch vector, start an
effecter, spawn a particle, step the ring, poke a field). The same script runs on the PowerPC original (the six functions are the
original's own code; unit w23's PAR_ResetParticleSystem builds the chains and PAR_SetupSin the sines on both sides) and on the host
build, and after EVERY step both sides' whole state is compared: the step's answer, the header's words (the seed, the serials, the
particle count), the 120 emitters, the 20 effecters, the chain heads, the pool (with a margin of 0x100 bytes of 0xcc on each side to
catch a write outside it), and at the end the library and the sines (never written) and the call log. Only the callee of unit w26a,
ApplyParticleDensityAdjustment, is a fake (it records the emitter's number and lets a case set the new emitter's start count and
life from the queues "count" and "life").

The library is the real one: the test reads ROOT/extracted/.../Data/Particle/Tp2.plb at run time (never copied) and loads it once on
each side with unit w23's PAR_LoadLibrary (the original's on the PowerPC side). The records are compared field by field: the original
keeps them big-endian, the host little-endian, so each side's bytes are put in the host's order with a table of field widths (the same
widths as the library structs of include/rsse.h). A wrong table fails the comparison of the library, which is checked at the end of
every case.
"""
import ctypes as C
import os
import pathlib
import struct
from operator import itemgetter

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ParticleSpawn"

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
PLB = ROOT / "extracted" / "disc-hfs" / "Theme Park Data" / "data" / "Particle" / "Tp2.plb"
PLB_PATH = "Data:Particle:Tp2.plb"

# The main app's addresses.
SIN_TABLE, STATE, STATE_SIZE, LIB, LIB_SIZE = 0x102E49F4, 0x102EDD68, 0x9E68, 0x102E5208, 0x8B60
RUNNING = 0x102E51FC
PAR_BASE, PAR_SIZE = SIN_TABLE, STATE + STATE_SIZE - SIN_TABLE        # the run of globals the system owns
EMITTERS, EFFECTERS, HEADS = STATE + 0x2C, STATE + 0x962C, STATE + 0x9E4C
SPAWN, INIT, LAUNCH, EFFECTER = 0x100BBFD4, 0x100BEC30, 0x100BE7E4, 0x100BF248
DOSPECIAL, SPECIAL1, DENSITY = 0x100BF42C, 0x100BF490, 0x100BE654
PAR_RESET, PAR_LOAD, PAR_SIN = 0x100BDD64, 0x100BA3E0, 0x100BA324
SW16S, SW16U, SW32, SWBLOCK = 0x100BABB4, 0x100BAB6C, 0x100BAB28, 0x100BAACC
ASSERT_FN, OPEN_FN = 0x1000DDE8, 0x10003FD4
REAL = {SPAWN, INIT, LAUNCH, EFFECTER, DOSPECIAL, SPECIAL1, PAR_RESET, PAR_LOAD, PAR_SIN, SW16S, SW16U, SW32, SWBLOCK}
LABEL_POOL, LABEL_FILE = 9, 5
MARGIN = 0x100                      # bytes of 0xcc kept before and after the pool
MASK32 = 0xFFFFFFFF

# ------------------------------------------------------------------ the records' field widths


def _cells(spec):
    """spec: (offset, size, count) runs, each of `count` fields of `size` bytes from `offset`; returns [(offset, size)] and checks
    that the fields follow one another with no gap."""
    out = []
    for offset, size, count in spec:
        for i in range(count):
            out.append((offset + i * size, size))
    at = 0
    for offset, size in out:
        assert offset == at, f"gap at {at:#x}, next field at {offset:#x}"
        at = offset + size
    return out, at


# One emitter (a live one is a copy of its library definition) in the library structs' widths (include/rsse.h).
EMITTER_CELLS, EMITTER_SIZE = _cells([
    (0x00, 2, 1), (0x02, 2, 1), (0x04, 1, 2), (0x06, 2, 1), (0x08, 2, 1), (0x0A, 2, 1), (0x0C, 2, 1), (0x0E, 2, 1), (0x10, 2, 1),
    (0x12, 2, 1), (0x14, 4, 4), (0x24, 2, 4), (0x2C, 4, 9), (0x50, 2, 2), (0x54, 4, 4), (0x64, 2, 2), (0x68, 1, 8), (0x70, 4, 1),
    (0x74, 2, 2), (0x78, 4, 7), (0x94, 2, 2), (0x98, 4, 2), (0xA0, 2, 5), (0xAA, 1, 2), (0xAC, 4, 3), (0xB8, 2, 1), (0xBA, 1, 2),
    (0xBC, 2, 2), (0xC0, 1, 4), (0xC4, 2, 2), (0xC8, 4, 2), (0xD0, 2, 2), (0xD4, 4, 1), (0xD8, 4, 16), (0x118, 1, 16),
    (0x128, 4, 6)])
assert EMITTER_SIZE == 0x140
# One effecter (a copy of its library definition).
EFFECTER_CELLS, EFFECTER_SIZE = _cells([
    (0x00, 4, 2), (0x08, 2, 1), (0x0A, 1, 2), (0x0C, 4, 14), (0x44, 1, 2), (0x46, 2, 1), (0x48, 1, 2), (0x4A, 2, 1), (0x4C, 4, 1),
    (0x50, 1, 16), (0x60, 2, 2), (0x64, 1, 4)])
assert EFFECTER_SIZE == 0x68
PARTICLE_CELLS, PARTICLE_SIZE = _cells([(0x00, 4, 9), (0x24, 2, 8)])
assert PARTICLE_SIZE == 0x34
HEADS_CELLS, HEADS_SIZE = _cells([(0x00, 2, 5), (0x0A, 1, 18)])
assert HEADS_SIZE == 0x1C


def _permutation(cells, records, record_size):
    """The indices that put a run of big-endian records in the host's order: each field's bytes reversed."""
    perm = []
    for record in range(records):
        base = record * record_size
        for offset, size in cells:
            perm.extend(range(base + offset + size - 1, base + offset - 1, -1))
    return perm


PERMS = {}


def host_order(raw, name, cells, records, record_size):
    """`raw` (big-endian fields) in the host's byte order, by the field widths of `cells`."""
    key = (name, records)
    if key not in PERMS:
        PERMS[key] = itemgetter(*_permutation(cells, records, record_size))
    return bytes(PERMS[key](raw))


def emitters_order(raw):
    return host_order(raw, "emitters", EMITTER_CELLS, 120, 0x140)


def effecters_order(raw):
    return host_order(raw, "effecters", EFFECTER_CELLS, 20, 0x68)


def library_order(raw):
    """The library: 105 emitter definitions, then 20 effecter definitions."""
    return emitters_order_n(raw[:0x8340], 105) + host_order(raw[0x8340:], "libeff", EFFECTER_CELLS, 20, 0x68)


def emitters_order_n(raw, count):
    return host_order(raw, "libemit", EMITTER_CELLS, count, 0x140)


def sines_order(raw):
    return host_order(raw, "sines", [(0, 4)], 512, 4)


def pool_order(raw, count):
    return host_order(raw, "pool", PARTICLE_CELLS, count, 0x34) if count else b""


def heads_order(raw):
    return host_order(raw, "heads", HEADS_CELLS, 1, 0x1C)


# ------------------------------------------------------------------ the library and the sines, made once on each side


def once(emu, key, action):
    done = emu.__dict__.setdefault("w26b_done", set())
    if key not in done:
        done.add(key)
        action(emu)


def loader_stubs(e):
    """The callees of the original's library loader, as in the unit w23 test: the file calls over the bytes of Tp2.plb."""
    st = CL.Stubs()
    st.real = set(REAL)

    def game_assert(e):
        e.cl.rec("GAME_ASSERTMSG", (T.s32(e.r(3)),), (e.cstr(e.r(4))[:159],))

    def tp_open(e):
        e.cl.position = 0
        e.set_r(3, e.w26b_file)

    def lb_read(e):
        # r3: the TbError's place, r4: the file, r5: the buffer, r6: the length, r7: bytes done
        L, buffer, length = e.cl, e.r(5), e.r(6)
        chunk = L.file[L.position:L.position + length]
        L.position += len(chunk)
        if length:
            e.write(buffer, chunk + bytes(length - len(chunk)))
        if e.r(7):
            e.write(e.r(7), struct.pack(">I", len(chunk)))
        e.write(e.r(3), bytes(4))

    def lb_seek(e):
        L, offset = e.cl, T.s32(e.r(4))
        L.position = max(0, min(len(L.file), L.position + offset))
        e.set_r(3, 0)

    def lb_close(e):
        e.write(e.r(3), bytes(4))

    def swap_block(e):
        a, n = e.r(3), T.s32(e.r(4))
        for i in range(max(n, 0)):
            e.write(a + 4 * i, e.read(a + 4 * i, 4)[::-1])

    st.imp("LbFile_Read__FPvPvUlPUl", lb_read)
    st.imp("LbFile_Seek__FPvl14TbFileSeekMode", lb_seek)
    st.imp("LbFile_Close__FPv", lb_close)
    st.imp("_USwapBlock32__13SamsUtilitiesFPUll", swap_block)
    st.internal(ASSERT_FN, game_assert)
    st.internal(OPEN_FN, tp_open)
    return st


def ppc_tables(e):
    """(library bytes, sine bytes) as the original leaves them in the emulator's memory: its own loader on the real Tp2.plb and its
    own PAR_SetupSin. Made once."""
    cached = e.__dict__.get("w26b_tables")
    if cached:
        return cached
    once(e, "std", T.install_std_stubs)
    if not hasattr(e, "w26b_file"):
        e.w26b_file = T.persist_alloc(e, 16)
    saved = e.read(PAR_BASE, PAR_SIZE)
    try:
        scratch = T.persist_alloc(e, 0x34)
        e.write(STATE, bytes(STATE_SIZE))
        e.write(STATE + 0x0C, struct.pack(">i", 1))
        e.write(STATE + 0x28, struct.pack(">I", scratch))
        e.write(RUNNING, struct.pack(">i", 1))
        e.cl = CL.PpcLog({e.w26b_file: LABEL_FILE})
        e.cl.file, e.cl.position = PLB.read_bytes(), 0
        st = loader_stubs(e)
        old = st.activate(e)
        try:
            path = e.alloc(len(PLB_PATH) + 1, PLB_PATH.encode("latin-1") + b"\0")
            result = T.s32(T.call_ppc(e, PAR_LOAD, [path])[0])
            assert result == 0, "the original's PAR_LoadLibrary could not load Tp2.plb"
            T.call_ppc(e, PAR_SIN, [])
        finally:
            CL.Stubs.deactivate(e, old)
        tables = (e.read(LIB, LIB_SIZE), e.read(SIN_TABLE, 0x800))
    finally:
        e.write(PAR_BASE, saved)
    e.w26b_tables = tables
    return tables


def host_region(lib, which):
    size = C.c_int32(0)
    address = lib.tpw_ps_region(which, C.byref(size))
    return address, size.value


def host_tables(lib):
    """(library bytes, sine bytes) of the host build: unit w23's loader on the same file. Made once."""
    cached = lib.__dict__.get("w26b_tables")
    if cached:
        return cached
    bind(lib)
    raw = PLB.read_bytes()
    scratch = C.create_string_buffer(0x34)
    result = lib.tpw_ps_load(raw, len(raw), scratch)
    assert result == 0, "the host's PAR_LoadLibrary could not load Tp2.plb"
    lib.tpw_ps_sin()
    out = (C.string_at(host_region(lib, 3)[0], LIB_SIZE), C.string_at(host_region(lib, 4)[0], 0x800))
    lib.w26b_tables = out
    lib.w26b_scratch = scratch
    return out


def bind(lib):
    if getattr(lib, "w26b_bound", False):
        return
    lib.w26b_bound = True
    c32, u32, vp = C.c_int32, C.c_uint32, C.c_void_p
    for name, res, args in [
            ("tpw_ps_load", c32, [C.c_char_p, c32, vp]), ("tpw_ps_sin", None, []), ("tpw_ps_region", vp, [c32, C.POINTER(c32)]),
            ("tpw_ps_begin", None, [c32, c32, vp, C.c_char_p, C.c_char_p]), ("tpw_ps_header", None, [vp]),
            ("tpw_ps_set_header", None, [c32, C.c_int64]), ("tpw_ps_mark", None, [c32]),
            ("tpw_ps_init", u32, [c32] * 4), ("tpw_ps_launch", u32, [c32] * 7), ("tpw_ps_effecter", u32, [c32] * 4),
            ("tpw_ps_spawn", c32, [c32]), ("tpw_ps_dospecial", None, [c32, c32]), ("tpw_ps_special", None, [c32, c32])]:
        fn = getattr(lib, name)
        fn.restype, fn.argtypes = res, args


# ------------------------------------------------------------------ cases


class Case(dict):
    """A script; it prints as its label."""

    def __repr__(self):
        return f"<{self['label']}>"


HEADER_WORDS = {   # name: (index in the unit's header list, Mac offset, struct format)
    "f00": (0, 0x00, ">i"), "ticks": (1, 0x04, ">i"), "lowspec": (2, 0x08, ">i"), "seed": (4, 0x10, ">i"),
    "f14": (5, 0x14, ">i"), "serial_e": (6, 0x18, ">h"), "serial_f": (7, 0x1A, ">h"), "density": (9, 0x20, ">i")}


def script(label, ops, maximum=20, running=1, header=None, library=(), queues=None, stale=None):
    """label; ops: the steps (see run_op_ppc); maximum: the pool's length; running: the system's running flag; header: words of
    the state set after the reset (HEADER_WORDS), the seed 1 and serials 1 otherwise; library: pokes into the definitions,
    ("L", type, offset, size, value) for an emitter's, ("LF", ...) for an effecter's; queues: the fake's results ("count",
    "life"); stale: a byte that fills every particle of the pool but its chain links."""
    return Case(label=label, ops=list(ops), maximum=maximum, running=running, header=dict(header or {}), library=list(library),
                queues=dict(queues or {}), stale=stale)


def region_ppc(which, pool):
    return {0: EMITTERS, 1: EFFECTERS, 3: LIB, 5: pool}[which]


def poke_target(where, index, offset):
    """(region, byte offset) of a poke: E an emitter, F an effecter, P a particle of the pool, L a library emitter definition, LF a
    library effecter definition."""
    if where == "E":
        return 0, index * 0x140 + offset
    if where == "F":
        return 1, index * 0x68 + offset
    if where == "P":
        return 5, index * 0x34 + offset
    if where == "L":
        return 3, index * 0x140 + offset
    if where == "LF":
        return 3, 0x8340 + index * 0x68 + offset
    raise ValueError(where)


def pack(order, size, value):
    return struct.pack(order + {1: "B", 2: "H", 4: "I"}[size], value & ((1 << (8 * size)) - 1))


class Walk:
    """What both sides track while a script runs: the emitter the last start made ("last") and whether the system is in low-spec mode
    (a start that answers 0 made nothing; with low-spec off, 0 is the first emitter of serial 0)."""

    def __init__(self, case):
        self.emitter = 0
        self.lowspec = case["header"].get("lowspec", 0)

    def resolve(self, value):
        return self.emitter if value == "last" else value

    def started(self, result):
        number = result & 0xFFFF
        if result != MASK32 and not (result == 0 and self.lowspec) and number < 120:
            self.emitter = number


# ------------------------------------------------------------------ the PowerPC side


def density_stub(e):
    """The fake of unit w26a's ApplyParticleDensityAdjustment: records the emitter, sets the start count and the life from the queues."""
    L = e.cl
    number = T.s32(e.r(3))
    L.rec("ApplyParticleDensityAdjustment", (number,))
    base = EMITTERS + number * 0x140
    count = struct.unpack(">i", e.read(base + 0x60, 4))[0]
    e.write(base + 0x60, struct.pack(">i", T.s32(L.ret("count", count))))
    life = struct.unpack(">i", e.read(base + 0x20, 4))[0]
    e.write(base + 0x20, struct.pack(">i", T.s32(L.ret("life", life))))


def snapshot_ppc(e, case, pool, result):
    L = e.cl

    def s32(a):
        return struct.unpack(">i", e.read(a, 4))[0]

    def s16(a):
        return struct.unpack(">h", e.read(a, 2))[0]

    sines = e.u32(STATE + 0x1C)
    total = max(case["maximum"], 0) * 0x34
    around = e.read(pool - MARGIN, total + 2 * MARGIN)
    return {
        "result": result,
        "header": [s32(STATE), s32(STATE + 4), s32(STATE + 8), s32(STATE + 0xC), s32(STATE + 0x10), s32(STATE + 0x14),
                   s16(STATE + 0x18), s16(STATE + 0x1A), 1 if sines == SIN_TABLE else 0 if sines == 0 else 2, s32(STATE + 0x20),
                   s32(RUNNING), L.lab(e.u32(STATE + 0x28))],
        "heads": heads_order(e.read(HEADS, 0x1C)),
        "emitters": emitters_order(e.read(EMITTERS, 0x9600)),
        "effecters": effecters_order(e.read(EFFECTERS, 0x820)),
        "pool": pool_order(around[MARGIN:MARGIN + total], max(case["maximum"], 0)),
        "margins": around[:MARGIN] + around[MARGIN + total:],
    }


def begin_ppc(e, case, pool):
    library, sines = ppc_tables(e)
    e.write(STATE, bytes(STATE_SIZE))
    e.write(STATE + 0x0C, struct.pack(">i", case["maximum"]))
    e.write(STATE + 0x1C, struct.pack(">I", SIN_TABLE))
    e.write(STATE + 0x28, struct.pack(">I", pool))
    e.write(RUNNING, struct.pack(">i", case["running"]))
    e.write(LIB, library)
    e.write(SIN_TABLE, sines)
    for where, index, offset, size, value in case["library"]:
        region, at = poke_target(where, index, offset)
        e.write(region_ppc(region, pool) + at, pack(">", size, value))
    T.call_ppc(e, PAR_RESET, [])
    for name, value in case["header"].items():
        _, offset, fmt = HEADER_WORDS[name]
        e.write(STATE + offset, struct.pack(fmt, T.s16(value) if fmt == ">h" else T.s32(value)))
    if case["stale"] is not None:
        stale_pool_ppc(e, case, pool)


def stale_pool_ppc(e, case, pool):
    raw = bytearray(e.read(pool, case["maximum"] * 0x34))
    for i in range(case["maximum"]):
        raw[i * 0x34:i * 0x34 + 0x30] = bytes([case["stale"]]) * 0x30
    e.write(pool, bytes(raw))


def run_op_ppc(e, case, op, walk, pool):
    kind = op[0]
    call = T.call_ppc
    if kind == "init":
        result = call(e, INIT, [T.u32(v) for v in op[1:5]])[0]
        walk.started(result)
        return result
    if kind == "launch":
        result = call(e, LAUNCH, [T.u32(v) for v in op[1:8]])[0]
        walk.started(result)
        return result
    if kind == "effecter":
        return call(e, EFFECTER, [T.u32(v) for v in op[1:5]])[0]
    if kind == "spawn":
        return T.s32(call(e, SPAWN, [T.u32(walk.resolve(op[1]))])[0])
    if kind == "special":
        call(e, SPECIAL1, [T.u32(walk.resolve(op[1])), T.u32(op[2])])
        return None
    if kind == "dospecial":
        call(e, DOSPECIAL, [T.u32(walk.resolve(op[1])), T.u32(op[2])])
        return None
    if kind == "poke":
        _, where, index, offset, size, value = op
        region, at = poke_target(where, walk.resolve(index), offset)
        e.write(region_ppc(region, pool) + at, pack(">", size, value))
        return None
    if kind == "hdr":
        _, name, value = op
        if name == "lowspec":
            walk.lowspec = value
        _, offset, fmt = HEADER_WORDS[name]
        e.write(STATE + offset, struct.pack(fmt, T.s16(value) if fmt == ">h" else T.s32(value)))
        return None
    raise ValueError(op)


def spawn_ppc(e, case):
    once(e, "std", T.install_std_stubs)
    tables = ppc_tables(e)
    saved = e.read(PAR_BASE, PAR_SIZE)
    try:
        total = case["maximum"] * 0x34 + 2 * MARGIN
        region = e.alloc(total, b"\xcc" * total)
        pool = region + MARGIN
        e.cl = CL.PpcLog({pool: LABEL_POOL}, case["queues"])
        st = CL.Stubs()
        st.real = set(REAL)
        st.internal(DENSITY, density_stub)
        old = st.activate(e)
        try:
            begin_ppc(e, case, pool)
            walk = Walk(case)
            steps = []
            for i, op in enumerate(case["ops"]):
                e.cl.rec("step", (i,))
                steps.append(snapshot_ppc(e, case, pool, run_op_ppc(e, case, op, walk, pool)))
        finally:
            CL.Stubs.deactivate(e, old)
        return {"steps": steps, "log": e.cl.log, "heap": [0, 0, 0, 0], "library": library_order(e.read(LIB, LIB_SIZE)),
                "sines": sines_order(e.read(SIN_TABLE, 0x800))}
    finally:
        e.write(PAR_BASE, saved)


# ------------------------------------------------------------------ the host side


def run_op_host(lib, case, op, walk, pool_address):
    kind = op[0]
    if kind == "init":
        result = lib.tpw_ps_init(*[T.s32(v) for v in op[1:5]])
        walk.started(result)
        return result
    if kind == "launch":
        result = lib.tpw_ps_launch(*[T.s32(v) for v in op[1:8]])
        walk.started(result)
        return result
    if kind == "effecter":
        return lib.tpw_ps_effecter(*[T.s32(v) for v in op[1:5]])
    if kind == "spawn":
        return lib.tpw_ps_spawn(T.s32(walk.resolve(op[1])))
    if kind == "special":
        lib.tpw_ps_special(T.s32(walk.resolve(op[1])), T.s32(op[2]))
        return None
    if kind == "dospecial":
        lib.tpw_ps_dospecial(T.s32(walk.resolve(op[1])), T.s32(op[2]))
        return None
    if kind == "poke":
        _, where, index, offset, size, value = op
        region, at = poke_target(where, walk.resolve(index), offset)
        C.memmove(host_base(lib, region) + at, pack("<", size, value), size)
        return None
    if kind == "hdr":
        _, name, value = op
        if name == "lowspec":
            walk.lowspec = value
        lib.tpw_ps_set_header(HEADER_WORDS[name][0], value)
        return None
    raise ValueError(op)


def host_base(lib, region):
    return host_region(lib, region)[0]


def snapshot_host(lib, case, pool_buffer, result):
    header = (C.c_int64 * 12)()
    lib.tpw_ps_header(header)
    total = max(case["maximum"], 0) * 0x34
    around = pool_buffer.raw
    return {
        "result": result,
        "header": list(header),
        "heads": C.string_at(host_region(lib, 2)[0], 0x1C),
        "emitters": C.string_at(host_base(lib, 0), 0x9600),
        "effecters": C.string_at(host_base(lib, 1), 0x820),
        "pool": around[MARGIN:MARGIN + total],
        "margins": around[:MARGIN] + around[MARGIN + total:],
    }


def spawn_host(lib, case):
    library, sines = host_tables(lib)
    total = case["maximum"] * 0x34 + 2 * MARGIN
    pool_buffer = C.create_string_buffer(b"\xcc" * total, total)
    pool_address = C.addressof(pool_buffer) + MARGIN
    CL.host_begin(lib, {LABEL_POOL: pool_address}, case["queues"])
    lib.tpw_ps_begin(case["running"], case["maximum"], pool_address, library, sines)
    for where, index, offset, size, value in case["library"]:
        region, at = poke_target(where, index, offset)
        C.memmove(host_base(lib, region) + at, pack("<", size, value), size)
    for name, value in case["header"].items():
        lib.tpw_ps_set_header(HEADER_WORDS[name][0], value)
    if case["stale"] is not None:
        for i in range(case["maximum"]):
            C.memmove(pool_address + i * 0x34, bytes([case["stale"]]) * 0x30, 0x30)
    walk = Walk(case)
    steps = []
    counters = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
    heap_before = list(counters)
    for i, op in enumerate(case["ops"]):
        lib.tpw_ps_mark(i)
        result = run_op_host(lib, case, op, walk, pool_address)
        steps.append(snapshot_host(lib, case, pool_buffer, result))
    log = CL.host_end(lib)
    # None of the six functions allocates (the original calls no allocator): the heap traffic of the script is none.
    heap = [after - before for after, before in zip(counters, heap_before)]
    return {"steps": steps, "log": log, "heap": heap, "library": C.string_at(host_base(lib, 3), LIB_SIZE),
            "sines": C.string_at(host_base(lib, 4), 0x800)}


# ------------------------------------------------------------------ the test object


def describe_difference(message):
    """Adds the record and field to a message about the emitters, effecters or pool bytes."""
    import re
    m = re.match(r"(.*)(emitters|effecters|pool): bytes differ at (\d+)", message)
    if not m:
        return message
    kind, at = m.group(2), int(m.group(3))
    size = {"emitters": 0x140, "effecters": 0x68, "pool": 0x34}[kind]
    return f"{message}  [{kind[:-1] if kind != 'pool' else 'particle'} {at // size} +{at % size:#x}]"


class ScriptTest(FnTest):
    def run_case(self, runner, case):
        return [describe_difference(m) for m in super().run_case(runner, case)]


# ------------------------------------------------------------------ the definitions the cases poke

# Offsets in an emitter definition (include/rsse.h, PAREmitter).
E_KIND, E_2D, E_PARTICLES, E_SPECIAL, E_JITTER = 0x04, 0x05, 0x08, 0x0E, 0x12
E_POS, E_LIFE, E_VELADD, E_LAUNCH, E_SPREAD = 0x14, 0x20, 0x2C, 0x38, 0x44
E_VELSPREAD, E_ANGLE, E_COLOUR, E_SPHERE, E_COUNT, E_RINGS = 0x50, 0x54, 0x58, 0x5C, 0x60, 0x68
E_ADDVEL, E_SPIN, E_PARAM, E_LIFEBASE, E_ACCEL = 0x6C, 0x6D, 0x74, 0x78, 0x84
E_UNSIGNEDY, E_SHELL, E_TURN, E_LAUNCHSCALE, E_LINK = 0xA0, 0xA8, 0xAC, 0xB0, 0xB4
E_LINKOFFSET, E_LINKEFF, E_KEEPDENSITY, E_LOWSPEC, E_BOX, E_RADIUS_OFFSET, E_RADIUS, E_LIFE0, E_COLOURS = \
    0xB8, 0xBA, 0xC0, 0xC1, 0xC2, 0xC4, 0xCC, 0xD4, 0xD8
F_LOWSPEC = 0x44            # an effecter definition's low-spec byte
BLANK = 103                 # a definition with no name and no links: the cases give it what they need


def put(type_, offset, size, value):
    return ("L", type_, offset, size, value)


def put_effecter(type_, offset, size, value):
    return ("LF", type_, offset, size, value)


def definition(type_=BLANK, **fields):
    """Pokes that make definition `type_` plain, then give it `fields` (name: value or (value, size)). The plain definition: no
    spread, no sphere, no link, no special, a life of 60, the launch scale 1024, not marked to skip in low-spec, density kept."""
    base = {"spread": (0, 0, 0), "box": 0, "shell": 0, "unsignedy": 0, "turn": 0, "radius": 0, "angle": 0, "colour": 0, "sphere": 0,
            "velspread": 0, "addvel": 0, "spin": 0, "lifebase": 60, "launch": (0, 0, 0), "veladd": (0, 0, 0), "jitter": 0,
            "link": -1, "linkoffset": 0, "linkeff": 0, "special": 0, "scale": 1024, "lowspec": 0, "keepdensity": 0, "twod": 0,
            "param": 7, "kind": 3, "accel": (0, 0, 0), "rings": (0, 0, 0, 0), "count": 0}
    unknown = set(fields) - set(base)
    assert not unknown, unknown
    base.update(fields)
    f = base
    pokes = [
        put(type_, E_SPREAD, 4, f["spread"][0]), put(type_, E_SPREAD + 4, 4, f["spread"][1]), put(type_, E_SPREAD + 8, 4, f["spread"][2]),
        put(type_, E_BOX, 1, f["box"]), put(type_, E_SHELL, 2, f["shell"]), put(type_, E_UNSIGNEDY, 2, f["unsignedy"]),
        put(type_, E_TURN, 4, f["turn"]), put(type_, E_RADIUS_OFFSET, 2, f["radius"]), put(type_, E_ANGLE, 4, f["angle"]),
        put(type_, E_COLOUR, 4, f["colour"]), put(type_, E_SPHERE, 4, f["sphere"]), put(type_, E_VELSPREAD, 2, f["velspread"]),
        put(type_, E_ADDVEL, 1, f["addvel"]), put(type_, E_SPIN, 1, f["spin"]), put(type_, E_LIFEBASE, 4, f["lifebase"]),
        put(type_, E_JITTER, 2, f["jitter"]), put(type_, E_LINK, 4, f["link"]), put(type_, E_LINKOFFSET, 2, f["linkoffset"]),
        put(type_, E_LINKEFF, 1, f["linkeff"]), put(type_, E_SPECIAL, 2, f["special"]), put(type_, E_LAUNCHSCALE, 4, f["scale"]),
        put(type_, E_LOWSPEC, 1, f["lowspec"]), put(type_, E_KEEPDENSITY, 1, f["keepdensity"]), put(type_, E_2D, 1, f["twod"]),
        put(type_, E_PARAM, 2, f["param"]), put(type_, E_KIND, 1, f["kind"]), put(type_, E_COUNT, 4, f["count"]),
        put(type_, E_LIFE, 4, 40),
    ]
    for i in range(3):
        pokes += [put(type_, E_LAUNCH + 4 * i, 4, f["launch"][i]), put(type_, E_VELADD + 4 * i, 4, f["veladd"][i]),
                  put(type_, E_ACCEL + 4 * i, 4, f["accel"][i])]
    for i in range(4):
        pokes.append(put(type_, E_RINGS + i, 1, f["rings"][i]))
    pokes += [put(type_, E_COLOURS + 4 * i, 4, 0x11110000 * (i % 15 + 1) + i) for i in range(16)]
    return pokes


NO_SPAWN = {"count": [0]}               # the fake sets every new emitter's start count to 0


# ------------------------------------------------------------------ SpawnParticle

def spawn_edges():
    cases = []
    cases.append(script("plain, one particle", [("init", BLANK, 160, 320, -480), ("spawn", "last")], 8, library=definition(),
                        queues=NO_SPAWN))
    # The position: three kinds of offset, with and without the vertical turn, an unsigned height, each pattern of spreads.
    spreads = [(0, 0, 0), (13, 0, 0), (0, 17, 0), (0, 0, 19), (23, 29, 31), (-23, -29, -31)]
    for shape in ("box", "disc", "shell"):
        for unsignedy in (0, 1):
            for turn in (0, 700):
                for n, spread in enumerate(spreads):
                    fields = dict(spread=spread, box=int(shape == "box"), shell=int(shape == "shell"), unsignedy=unsignedy,
                                  turn=turn, radius=5 if n % 2 else 0, angle=1200 + 7 * n, param=-3, kind=-2)
                    cases.append(script(f"position {shape} y{unsignedy} turn{turn} spread{n}",
                                        [("init", BLANK, 1000, -2000, 3000), ("spawn", "last"), ("spawn", "last")], 8,
                                        library=definition(**fields), queues=NO_SPAWN))
    # The velocity, the colour, the spin and the life.
    n = 0
    for sphere in (0, 18, -5):
        for velspread in (0, 12, -9):
            for addvel in (0, 1):
                for colour in (0, 1, -3):
                    for spin in (0, 1):
                        n += 1
                        if (n * 7) % 5 > 1:         # about two cases in five: every value of every field is met
                            continue
                        lifebase = (0, 3, 60, -40, 7)[n % 5]
                        fields = dict(sphere=sphere, velspread=velspread, addvel=addvel, colour=colour, spin=spin,
                                      lifebase=lifebase, launch=(5, -6, 7), veladd=(100, -200, 300), spread=(0, 0, 0))
                        cases.append(script(f"velocity sphere{sphere} spread{velspread} add{addvel} colour{colour} spin{spin} "
                                            f"life{lifebase}", [("init", BLANK, 0, 0, 0), ("spawn", "last"), ("spawn", "last")], 8,
                                            library=definition(**fields), queues=NO_SPAWN))
    # The pool: empty from the start, used up, one past.
    plain = definition(spread=(5, 6, 7), velspread=4)
    cases.append(script("pool of 1, two spawns", [("init", BLANK, 0, 0, 0), ("spawn", "last"), ("spawn", "last")], 1, library=plain,
                        queues=NO_SPAWN))
    cases.append(script("pool of 3, four spawns", [("init", BLANK, 0, 0, 0)] + [("spawn", "last")] * 4, 3, library=plain,
                        queues=NO_SPAWN))
    cases.append(script("pool of 2, the start makes five", [("init", BLANK, 0, 0, 0), ("spawn", "last")], 2, library=plain,
                        queues={"count": [5]}))
    cases.append(script("pool of 12, start makes ten, two emitters", [("init", BLANK, 0, 0, 0), ("init", BLANK, 16, 16, 16),
                                                                       ("spawn", "last"), ("spawn", "last"), ("spawn", "last")], 12,
                        library=plain, queues={"count": [10, 1]}))
    # Left over in the pool and the emitter: what the spawn writes and what it leaves.
    cases.append(script("stale pool", [("init", BLANK, 0, 0, 0), ("spawn", "last")], 4, library=plain, queues=NO_SPAWN, stale=0xA5))
    cases.append(script("counters wrap", [("init", BLANK, 0, 0, 0), ("poke", "E", "last", E_PARTICLES, 2, 0xFFFF),
                                          ("hdr", "f00", 0x7FFFFFFF), ("spawn", "last")], 4, library=plain, queues=NO_SPAWN))
    for seed in (0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x12345678):
        cases.append(script(f"seed {seed:#x}", [("init", BLANK, 5, 6, 7), ("spawn", "last")], 4,
                            library=definition(spread=(300, 400, 500), velspread=100, sphere=0, lifebase=80),
                            header={"seed": seed}, queues=NO_SPAWN))
    # The real definitions, each once.
    for t in (1, 2, 3, 4, 15, 19, 26, 35, 44, 48, 56, 60, 75, 87, 90, 97, 98, 99):
        cases.append(script(f"type {t}", [("init", t, 3200, 160, -4800), ("spawn", "last"), ("spawn", "last")], 60))
    return cases


def random_definition(r, type_):
    """Pokes that give a definition random field values from the sets the code branches on."""
    spread_set = [0, 0, 0, 1, 3, 9, 100, 312, -4, -1000, 0x7FFFFFFF, -0x7FFFFFFF]
    angle_set = [0, 8, 100, 2048, -1000, 4095, 0x40000000, -0x40000000, 7, -3]
    fields = dict(
        spread=tuple(r.choice(spread_set) for _ in range(3)), box=r.choice((0, 0, 1, -1, 0x80)), shell=r.choice((0, 0, 1, -1, 256)),
        unsignedy=r.choice((0, 0, 1, -1)), turn=r.choice(angle_set), radius=r.choice((0, 0, 5, -300, 625)),
        angle=r.choice(angle_set), colour=r.choice((0, 0, 1, -3, 2)), sphere=r.choice((0, 0, 1, 18, 62, -5, 250)),
        velspread=r.choice((0, 0, 12, -7, 50, 1)), addvel=r.choice((0, 1, -1)), spin=r.choice((0, 1, -1)),
        lifebase=r.choice((0, 1, 3, 4, 8, 60, 100, -40, 0x7FFFFFFF)), launch=tuple(r.choice((0, 5, -6, 2000, -70000)) for _ in range(3)),
        veladd=tuple(r.choice((0, 100, -200, 300000)) for _ in range(3)), param=r.randint(-32768, 32767), kind=r.randint(-128, 127),
        jitter=r.choice((0, -1, -1, 1, 10, 55)), scale=r.choice((1024, 1024, 0, 512, -1024, 3000)),
        twod=r.choice((0, 0, 1)), accel=tuple(r.choice((0, 0, 4, -4)) for _ in range(3)), keepdensity=r.choice((0, 0, 1)),
        rings=tuple(r.choice((0, 1, 2, 3, 5, -1)) for _ in range(4)))
    return definition(type_, **fields)


POOLS = (1, 2, 3, 5, 12, 40)
SERIALS = (0, 1, 5, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF)
SEEDS = (0, 1, 2, 12345, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF)


def random_world(r):
    """The world of a random case: the pool, the seed and serials, low-spec."""
    header = {"seed": r.choice(SEEDS) if r.random() < 0.5 else r.getrandbits(32), "serial_e": r.choice(SERIALS),
              "serial_f": r.choice((0, 1, 7, 0xFFFF)), "f00": r.choice((0, 0, 5, 0x7FFFFFFF)),
              "lowspec": r.choice((0, 0, 0, 1))}
    return r.choice(POOLS), header


def spawn_random(r):
    maximum, header = random_world(r)
    library = []
    if r.random() < 0.8:
        library += random_definition(r, BLANK)
        t = BLANK
    else:
        t = r.randrange(105)
    ops = [("init", t, r.randint(-100000, 100000), r.randint(-100000, 100000), r.randint(-100000, 100000))]
    if r.random() < 0.3:
        ops.append(("init", t, r.randint(-100000, 100000), 5, r.randint(-100000, 100000)))
    ops += [("spawn", "last")] * r.randint(1, 3)
    queues = {"count": [r.choice((0, 0, 1, 2, 5))]} if r.random() < 0.8 else {}
    header["lowspec"] = 0
    return script("random spawn", ops, maximum, header=header, library=library, queues=queues,
                  stale=r.choice((None, None, 0, 0xA5, 0xFF)))


# ------------------------------------------------------------------ PAR_InitEmitter and PAR_InitEmitterLaunch

CHILD = 102                 # a second blank definition: the child of a link


def start(kind, t, x=1600, y=-800, z=2400, vector=(0, 0, 0)):
    """A step that starts an emitter: 'init' or 'launch' (with a launch vector)."""
    return ("init", t, x, y, z) if kind == "init" else ("launch", t, x, y, z) + tuple(vector)


def starter_edges(kind):
    cases = []
    vec = (3000, -2048, 77) if kind == "launch" else (0, 0, 0)
    # Every definition of the library once (with its real start count, child and effecter).
    for t in range(105):
        cases.append(script(f"{kind} type {t}", [start(kind, t, 1600 + t, -800, 2400 - 7 * t, vec)], 60))
    # A type out of range, the system not running, low-spec.
    for t in (-1, 105, 106, 0x7FFFFFFF, -0x80000000):
        cases.append(script(f"{kind} type {t}", [start(kind, t, vector=vec)], 8))
    cases.append(script(f"{kind} not running", [start(kind, 3, vector=vec)], 8, running=0))
    for t in (0, 3, 4, 35, 60, 84, 86):
        for lowspec in (0, 1):
            cases.append(script(f"{kind} type {t} lowspec {lowspec}", [start(kind, t, vector=vec)], 60, header={"lowspec": lowspec}))
    cases.append(script(f"{kind} lowspec, definition not marked", [start(kind, BLANK, vector=vec)], 8, header={"lowspec": 1},
                        library=definition(lowspec=0), queues=NO_SPAWN))
    cases.append(script(f"{kind} lowspec, definition marked", [start(kind, BLANK, vector=vec)], 8, header={"lowspec": 1},
                        library=definition(lowspec=-1), queues=NO_SPAWN))
    # The fake of the density adjustment: called unless the definition says to keep the density, and it sets the count and the life.
    for keep in (0, 1):
        for queues in ({}, {"count": [3], "life": [90]}, {"count": [0], "life": [0]}, {"life": [-5]}):
            cases.append(script(f"{kind} keepdensity {keep} {queues}", [start(kind, BLANK, 33, 47, -65, vec)] , 8,
                                library=definition(keepdensity=keep, count=2), queues=queues))
    # Positions: shifted right by 4, rounding down; and the bounding box.
    for x, y, z in ((0, 0, 0), (15, 16, 17), (-1, -15, -16), (-17, 0x7FFFFFFF, -0x80000000), (90000, 0, 7000)):
        cases.append(script(f"{kind} position {x},{y},{z}", [start(kind, BLANK, x, y, z, vec)], 8, library=definition(),
                            queues=NO_SPAWN))
    for twod in (0, 1, -1):
        cases.append(script(f"{kind} 2D {twod}", [start(kind, BLANK, vector=vec)], 8, library=definition(twod=twod), queues=NO_SPAWN))
    for accel in ((0, 0, 0), (1, 0, 0), (0, -1, 0), (0, 0, 5)):
        cases.append(script(f"{kind} accel {accel}", [start(kind, BLANK, vector=vec)], 8, library=definition(accel=accel),
                            queues=NO_SPAWN))
    # The serial and the chains: wrap, a second and a third emitter, then every emitter used up.
    for serial in (0, 1, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF):
        cases.append(script(f"{kind} serial {serial:#x}", [start(kind, BLANK, vector=vec)] * 3, 8, header={"serial_e": serial},
                            library=definition(), queues=NO_SPAWN))
    cases.append(script(f"{kind} all emitters used", [start(kind, BLANK, 16 * i, 0, 0, vec) for i in range(122)], 2,
                        library=definition(), queues=NO_SPAWN))
    # The start count: the loop, with the pool bigger and smaller than it.
    for count in (0, 1, 2, 7, 25):
        for maximum in (1, 3, 40):
            cases.append(script(f"{kind} count {count} pool {maximum}", [start(kind, BLANK, vector=vec)], maximum,
                                library=definition(spread=(9, 9, 9), velspread=3), queues={"count": [count]}))
    # The launch vector (PAR_InitEmitterLaunch) and the jitter (PAR_InitEmitter), with the other's field set as well.
    for scale in (1024, 0, 512, -1024, 3000, 1, 1023):
        for v in ((0, 0, 0), (1, -1, 1024), (1023, -1023, 1025), (0x7FFFFFFF, -0x80000000, 77777), (-5, 5, -2049)):
            cases.append(script(f"{kind} scale {scale} vector {v}", [start(kind, BLANK, vector=v)], 8,
                                library=definition(scale=scale, launch=(11, 22, 33), veladd=(1, 2, 3), jitter=9), queues=NO_SPAWN))
    for jitter in (0, 1, -1, 10, 32767, -32768):
        for seed in (1, 0x12345678):
            cases.append(script(f"{kind} jitter {jitter} seed {seed:#x}", [start(kind, BLANK, vector=vec)], 8, header={"seed": seed},
                                library=definition(jitter=jitter, veladd=(100, -200, 300)), queues=NO_SPAWN))
    # The special emitter: 1 starts the ring (the radius is 0), any other nonzero value does nothing.
    for special in (0, 1, 2, -1):
        cases.append(script(f"{kind} special {special}", [start(kind, BLANK, vector=vec)], 8,
                            library=definition(special=special, count=3, velspread=64, rings=(3, 2, 1, 0), spread=(5, 5, 5)),
                            queues={"count": [3]}))
    # The linked emitter or effecter.
    for linkoffset in (0, 1):
        cases.append(script(f"{kind} child, offset {linkoffset}", [start(kind, BLANK, 160, 320, 480, vec)], 12,
                            library=definition(link=CHILD, linkoffset=linkoffset) + definition(CHILD, spread=(3, 3, 3)) +
                            [put(CHILD, E_POS, 4, 1000), put(CHILD, E_POS + 4, 4, 2000), put(CHILD, E_POS + 8, 4, 3000),
                             put(CHILD, E_VELADD, 4, 10), put(CHILD, E_VELADD + 4, 4, -20), put(CHILD, E_VELADD + 8, 4, 30)],
                            queues={"count": [2, 1]}))
    cases.append(script(f"{kind} child that names itself", [start(kind, BLANK, vector=vec)], 12,
                        library=definition(link=BLANK, linkoffset=1), queues=NO_SPAWN))
    cases.append(script(f"{kind} child that names the parent, which names it", [start(kind, BLANK, vector=vec)], 4,
                        library=definition(link=CHILD, linkoffset=1) + definition(CHILD, link=BLANK, linkoffset=0), queues=NO_SPAWN))
    cases.append(script(f"{kind} child out of range", [start(kind, BLANK, vector=vec)], 12,
                        library=definition(link=0x10000, linkoffset=0), queues=NO_SPAWN))
    cases.append(script(f"{kind} child, lowspec makes none", [start(kind, BLANK, vector=vec)], 12, header={"lowspec": 1},
                        library=definition(link=CHILD, linkoffset=1) + definition(CHILD, lowspec=1), queues=NO_SPAWN))
    cases.append(script(f"{kind} child, all emitters used", [start(kind, BLANK, 16 * i, 0, 0, vec) for i in range(121)], 2,
                        library=definition(link=CHILD, linkoffset=1) + definition(CHILD), queues=NO_SPAWN))
    for link in (0, 5, 19, 20, 0x7FFFFFFF):
        cases.append(script(f"{kind} effecter {link}", [start(kind, BLANK, 160, 320, 480, vec)], 12,
                            library=definition(link=link, linkeff=1), queues=NO_SPAWN))
    cases.append(script(f"{kind} effecter, no room", [("effecter", 3, 0, 0, 0)] * 20 + [start(kind, BLANK, vector=vec)], 12,
                        library=definition(link=4, linkeff=1), queues=NO_SPAWN))
    # An effecter link with an offset: the original marks the effecter by its whole handle (see MarkEffecterLinked); the serial
    # steps that put the byte inside the state are the ones where it is 0, 8192, 16384 and 24576 (the others cannot be run).
    for serial in (0, 8192, 16384, 24576):
        cases.append(script(f"{kind} effecter with offset, serial {serial}", [start(kind, BLANK, 160, 320, 480, vec)], 12,
                            header={"serial_f": serial},
                            library=definition(link=6, linkeff=1, linkoffset=1) + [put_effecter(6, 0x18, 4, 160),
                                                                                   put_effecter(6, 0x1C, 4, -32),
                                                                                   put_effecter(6, 0x20, 4, 48)],
                            queues=NO_SPAWN))
    cases.append(script(f"{kind} effecter with offset, handle negative", [start(kind, BLANK, 160, 320, 480, vec)], 12,
                        header={"serial_f": 0x8000}, library=definition(link=6, linkeff=1, linkoffset=1), queues=NO_SPAWN))
    # A real library emitter whose child is also in the library, in low-spec too.
    for t in (4, 51, 60, 62, 75, 98):
        for lowspec in (0, 1):
            cases.append(script(f"{kind} type {t} with child, lowspec {lowspec}", [start(kind, t, vector=vec)], 80,
                                header={"lowspec": lowspec}))
    return cases


def link_pokes(r, type_, other):
    """Random links for a synthetic definition: an emitter or an effecter, with or without an offset (never an effecter with an
    offset: the original's byte then lands wherever the serial puts it)."""
    kind = r.choice(("none", "none", "emitter", "emitter", "effecter"))
    if kind == "none":
        return definition(type_, link=-1)
    if kind == "emitter":
        offset = r.choice((0, 1))
        link = r.choice((other, other, type_, r.randrange(105))) if offset else r.choice((other, type_, r.randrange(105), 105, 5000))
        return definition(type_, link=link, linkoffset=offset)
    return definition(type_, link=r.choice((0, 3, 19, 20, 99)), linkeff=1, linkoffset=0)


def starter_random(kind):
    def make(r):
        maximum, header = random_world(r)
        library, steps = [], []
        synthetic = r.random() < 0.6
        if synthetic:
            library += random_definition(r, BLANK) + link_pokes(r, BLANK, CHILD)
            library += random_definition(r, CHILD) if r.random() < 0.5 else definition(CHILD, spread=(3, 0, 5))
            if r.random() < 0.5:
                library += link_pokes(r, CHILD, BLANK)
            library += [put(CHILD, E_POS, 4, r.randint(-5000, 5000)), put(CHILD, E_SPREAD, 4, r.choice((0, 7))),
                        put(BLANK, E_SPECIAL, 2, r.choice((0, 0, 0, 1, 2)))]
            library += [put(BLANK, E_LOWSPEC, 1, r.choice((0, 0, -1))), put(CHILD, E_LOWSPEC, 1, r.choice((0, 0, -1)))]
        for _ in range(r.randint(1, 3)):
            t = BLANK if synthetic and r.random() < 0.8 else r.choice((r.randrange(105), r.randrange(105), -1, 105, 0x7FFFFFFF))
            steps.append(start(kind, t, r.randint(-100000, 100000), r.randint(-100000, 100000), r.randint(-100000, 100000),
                               tuple(r.choice((0, 1, -1, 1024, -3000, r.getrandbits(31), -r.getrandbits(31))) for _ in range(3))))
        steps += [("spawn", "last")] * r.randint(0, 2)
        queues = {}
        if r.random() < 0.7:
            queues["count"] = [r.choice((0, 0, 1, 2, 5, 12))]
        if r.random() < 0.2:
            queues["life"] = [r.choice((0, 40, -3, 200))]
        if r.random() < 0.1:
            return script(f"random {kind}", steps, maximum, running=0, header=header, library=library, queues=queues)
        return script(f"random {kind}", steps, maximum, header=header, library=library, queues=queues,
                      stale=r.choice((None, None, 0xA5)))
    return make


# ------------------------------------------------------------------ PAR_InitEffecter

def effecter_edges():
    cases = []
    for t in range(20):
        cases.append(script(f"effecter type {t}", [("effecter", t, 1600 + t, -800 - 3 * t, 2400)], 4))
    for t in (-1, 20, 21, 0x7FFFFFFF, -0x80000000):
        cases.append(script(f"effecter type {t}", [("effecter", t, 1, 2, 3)], 4))
    cases.append(script("effecter not running", [("effecter", 3, 1, 2, 3)], 4, running=0))
    for lowspec in (0, 1):
        for flag in (0, 1, -1, 0x7F):
            cases.append(script(f"effecter lowspec {lowspec} flag {flag}", [("effecter", 3, 5, 6, 7), ("effecter", 4, 5, 6, 7)], 4,
                                header={"lowspec": lowspec}, library=[put_effecter(3, F_LOWSPEC, 1, flag)]))
    for x, y, z in ((0, 0, 0), (15, 16, 17), (-1, -15, -16), (-17, 0x7FFFFFFF, -0x80000000), (90000, 0, 7000)):
        cases.append(script(f"effecter position {x},{y},{z}", [("effecter", 5, x, y, z)], 4))
    for serial in (0, 1, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF):
        cases.append(script(f"effecter serial {serial:#x}", [("effecter", 7, 1, 2, 3)] * 3, 4, header={"serial_f": serial}))
    cases.append(script("effecter all used", [("effecter", i % 20, 16 * i, 0, 0) for i in range(22)], 4))
    cases.append(script("effecter after emitters", [("init", BLANK, 1, 2, 3), ("effecter", 5, 4, 5, 6), ("init", BLANK, 7, 8, 9)], 4,
                        library=definition(), queues=NO_SPAWN))
    cases.append(script("effecter, stale links", [("effecter", 2, 0, 0, 0), ("poke", "F", 1, 0x62, 2, 0x7777), ("effecter", 3, 0, 0, 0),
                                                  ("effecter", 4, 0, 0, 0)], 4))
    return cases


def effecter_random(r):
    maximum, header = random_world(r)
    library = []
    for _ in range(r.randint(0, 3)):
        library.append(put_effecter(r.randrange(20), F_LOWSPEC, 1, r.choice((0, 1, -1, 0x7F))))
    steps = []
    for _ in range(r.choice((1, 1, 2, 3, 5, 22))):
        t = r.choice((r.randrange(20), r.randrange(20), r.randrange(20), -1, 20, 0x7FFFFFFF))
        steps.append(("effecter", t, r.randint(-100000, 100000), r.randint(-100000, 100000), r.randint(-100000, 100000)))
    return script("random effecter", steps, maximum, running=r.choice((1, 1, 1, 1, 0)), header=header, library=library)


# ------------------------------------------------------------------ DoSpecialEmitter and SpecialEmitter1

RING = 104                  # a definition made into an expanding ring


def ring(**more):
    fields = dict(special=1, velspread=96, rings=(3, 2, 1, 0), lifebase=40, launch=(0, 30, 0), spread=(0, 0, 0))
    fields.update(more)
    return definition(RING, **fields)


def special_edges(direct):
    cases = []
    step = "special" if direct else "dospecial"
    # The start: the radius is cleared and the emitter is marked in use.
    for start_flag in (1, 2, -1):
        cases.append(script(f"{step} start {start_flag}", [("init", RING, 160, 320, 480), ("poke", "E", "last", E_RADIUS, 4, 5000),
                                                           (step, "last", start_flag)], 10, library=ring(), queues=NO_SPAWN))
    # Steps of the ring: the radius grows by mVelSpread, the count of particles by the life stage.
    for velspread in (96, 0, 1, 300, -50, -4000):
        cases.append(script(f"{step} ring spread {velspread}",
                            [("init", RING, 160, 320, 480)] + [(step, "last", 0)] * 3, 150, library=ring(velspread=velspread),
                            queues=NO_SPAWN))
    # The life stage: the ratio of the life to the life at the start, in quarters.
    for life in (50, 40, 39, 30, 29, 20, 19, 10, 9, 5, 0, -3, -40):
        cases.append(script(f"{step} life {life}", [("init", RING, 160, 320, 480), ("poke", "E", "last", E_LIFE, 4, life),
                                                    (step, "last", 0), (step, "last", 0)], 150,
                            library=ring(rings=(7, 5, 3, 2)), queues={"count": [0], "life": [40]}))
    for life_at_start in (0, 1, -40, 3):
        cases.append(script(f"{step} life at start {life_at_start}",
                            [("init", RING, 160, 320, 480), ("poke", "E", "last", E_LIFE0, 4, life_at_start),
                             (step, "last", 0), (step, "last", 0)], 150, library=ring(rings=(7, 5, 3, 2)), queues=NO_SPAWN))
    for rings in ((0, 0, 0, 0), (1, 1, 1, 1), (-1, 3, 3, -1), (5, 127, -128, 0), (2, 2, 2, 2)):
        cases.append(script(f"{step} rings {rings}", [("init", RING, 160, 320, 480), ("poke", "E", "last", E_RADIUS, 4, 600),
                                                      (step, "last", 0)], 150, library=ring(rings=rings), queues=NO_SPAWN))
    # In use or not, and the radius it starts from.
    for in_use in (1, 0, -1, 32767):
        cases.append(script(f"{step} in use {in_use}", [("init", RING, 160, 320, 480), ("poke", "E", "last", 0, 2, in_use),
                                                        (step, "last", 0)], 60, library=ring(), queues=NO_SPAWN))
    for radius in (0, 127, 128, 255, 256, 3000, -1, -128, -129, -5000):
        cases.append(script(f"{step} radius {radius}", [("init", RING, 160, 320, 480), ("poke", "E", "last", E_RADIUS, 4, radius),
                                                        (step, "last", 0)], 150, library=ring(rings=(2, 2, 2, 2)), queues=NO_SPAWN))
    # The particle: the life, the angle's sines, the colour, left alone fields (the pool is stale).
    for lifebase in (0, 1, 2, 3, 60, -1, -60, 0x7FFFFFFF):
        cases.append(script(f"{step} particle life {lifebase}", [("init", RING, 1, 2, 3), (step, "last", 0)], 60,
                            library=ring(lifebase=lifebase, param=-9, kind=-1), queues=NO_SPAWN, stale=0xA5))
    for seed in (0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
        cases.append(script(f"{step} seed {seed:#x}", [("init", RING, 160, 320, 480), (step, "last", 0), (step, "last", 0)], 150,
                            header={"seed": seed}, library=ring(), queues=NO_SPAWN))
    # The pool: used up, one particle, and an emitter that already has particles.
    for maximum in (1, 2, 7):
        cases.append(script(f"{step} pool {maximum}", [("init", RING, 160, 320, 480), (step, "last", 0), (step, "last", 0)], maximum,
                            library=ring(), queues=NO_SPAWN))
    cases.append(script(f"{step} after other emitters", [("init", BLANK, 0, 0, 0), ("init", RING, 160, 320, 480), ("spawn", 0),
                                                         (step, "last", 0), ("init", RING, 16, 32, 48), (step, 2, 0)], 60,
                        library=ring() + definition(spread=(5, 5, 5)), queues=NO_SPAWN))
    cases.append(script(f"{step} counters", [("init", RING, 160, 320, 480), ("poke", "E", "last", E_PARTICLES, 2, 0xFFFE),
                                             ("hdr", "f00", 0x7FFFFFFE), (step, "last", 0)], 60, library=ring(), queues=NO_SPAWN))
    # DoSpecialEmitter only calls SpecialEmitter1 for the value 1.
    if not direct:
        for special in (0, 1, 2, -1, 0x100, 0x10001):
            for flag in (0, 1):
                cases.append(script(f"dospecial special {special} flag {flag}",
                                    [("init", RING, 160, 320, 480), ("poke", "E", "last", E_SPECIAL, 2, special),
                                     ("poke", "E", "last", E_RADIUS, 4, 700), ("dospecial", "last", flag)], 150, library=ring(),
                                    queues=NO_SPAWN))
    return cases


def special_random(direct):
    step = "special" if direct else "dospecial"

    def make(r):
        maximum, header = random_world(r)
        header["lowspec"] = 0
        fields = dict(velspread=r.choice((0, 1, 40, 96, 300, -20, -3000)), rings=tuple(r.choice((0, 1, 2, 3, 5, -1, 127, -128))
                                                                                      for _ in range(4)),
                      lifebase=r.choice((0, 1, 3, 40, 100, -7)), launch=(0, r.choice((0, 30, -50)), 0),
                      param=r.randint(-32768, 32767), kind=r.randint(-128, 127))
        steps = [("init", RING, r.randint(-5000, 5000), r.randint(-5000, 5000), r.randint(-5000, 5000))]
        for _ in range(r.randint(1, 3)):
            if r.random() < 0.6:
                steps.append(("poke", "E", "last", E_RADIUS, 4, r.choice((0, 100, 128, 640, 4000, -300))))
            if r.random() < 0.6:
                steps.append(("poke", "E", "last", E_LIFE, 4, r.choice((50, 40, 30, 20, 10, 0, -4))))
            if r.random() < 0.2:
                steps.append(("poke", "E", "last", 0, 2, r.choice((0, 1, -1))))
            if not direct and r.random() < 0.3:
                steps.append(("poke", "E", "last", E_SPECIAL, 2, r.choice((0, 1, 2, -1))))
            steps.append((step, "last", r.choice((0, 0, 0, 1))))
        return script(f"random {step}", steps, maximum, header=header, library=ring(**fields), queues={"count": [0], "life": [40]},
                      stale=r.choice((None, 0xA5)))
    return make


# ------------------------------------------------------------------ the tests

TESTS = [
    ScriptTest("SpawnParticle", spawn_ppc, spawn_host, spawn_edges, spawn_random),
    ScriptTest("PAR_InitEmitter", spawn_ppc, spawn_host, lambda: starter_edges("init"), starter_random("init")),
    ScriptTest("PAR_InitEmitterLaunch", spawn_ppc, spawn_host, lambda: starter_edges("launch"), starter_random("launch")),
    ScriptTest("PAR_InitEffecter", spawn_ppc, spawn_host, effecter_edges, effecter_random),
    ScriptTest("DoSpecialEmitter", spawn_ppc, spawn_host, lambda: special_edges(False), special_random(False)),
    ScriptTest("SpecialEmitter1", spawn_ppc, spawn_host, lambda: special_edges(True), special_random(True)),
]
