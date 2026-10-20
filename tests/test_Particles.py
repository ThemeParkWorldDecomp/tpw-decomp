"""Unit w26a: the particle system's core, second half (PAR_CloseParticleSystem, PAR_Free, PAR_LoadLibEmitter, PAR_KillLibEmitter,
PAR_Tick, ApplyParticleDensityAdjustment, PAR_GetEmitterStatus, PAR_GetEffecterStatus, the emitter killers and setters,
PAR_GetEmitterBoundingBox, PAR_FreeDrawingList), against libtpw_unit_Particles.dylib (tests/unit_Particles.cpp).

A case is a script of calls (the "ops") on a particle system the case prepares in the same way on both sides: the state, the
library and the pool are filled with one byte, then "pokes" put values into fields (by their Mac offsets, big-endian on the
PowerPC side and in the host's order on the host side), a live emitter can be made as PAR_InitEmitter makes it (a copy of a
library definition) and the case can start from the REAL particle library: PAR_InitParticleSystem and PAR_LoadLibrary (unit w23,
translated, real on both sides) read ROOT/extracted/.../Data/Particle/Tp2.plb at run time (never copied into the repository; a
missing file skips those cases with a message).

After the ops, everything the calls can touch is compared: the state's header, the globals, the emitters, the effecters, the chain
heads, the library, the pool, the sine table, the answers, the bounding boxes written and the call log (the heap traffic, the file
reads, the byte swaps and DoEmitters, DoEffecters, DoAParticle of unit w26c, which PAR_Tick calls and which are fakes on both
sides: they can change the state when they run, from queues the case fills, so that the order of PAR_Tick's reads shows).

The state's records (emitters, effecters, chain heads, pool) are in the host's order and the PowerPC side's are big-endian: the
test turns the host's bytes into the PowerPC order field by field with the widths of the library layout (include/rsse.h,
PARLibraryEmitter and PARLibraryEffecter: the same layout as a live emitter and effecter). The library itself needs no turning:
the unit library is built with TPW_SWAP_FILE_WORDS, like unit ParticlesInit's, so that the loader's swaps really swap there and
the records are in the original's order (so PAR_LoadLibEmitter's swaps are compared for real).
"""
import ctypes as C
import random
import struct

import calllog as CL
import test_ParticlesInit as PI
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Particles"

STATE, STATE_SIZE, LIB, LIB_SIZE, SIN_TABLE = PI.STATE, PI.STATE_SIZE, PI.LIB, PI.LIB_SIZE, PI.SIN_TABLE
RUNNING, BUSY, DRAWLIST, W_A, W_B = PI.RUNNING, PI.BUSY, PI.DRAWLIST, PI.W_A, PI.W_B
BALANCE = 0x102E49F0                      # gParAllocBalance, the word before the table of sines
SAVED_SIZE = STATE + STATE_SIZE - BALANCE
DUMMY_TV = PI.DUMMY_TV
PAR_INIT, PAR_LOAD = PI.PAR_INIT, PI.PAR_LOAD
EMITTERS, EFFECTERS, HEADS = STATE + 0x2C, STATE + 0x962C, STATE + 0x9E4C
LABEL_FILE, LABEL_POOL, LABEL_LIST, LABEL_OTHER = 5, 9, 11, 12
MARGIN_POOL = 0x100                       # bytes of 0xcc kept before and after a pool the case makes
NO_CHANGE = 0x10000                       # a queue value that leaves the state alone (tests/unit_Particles.cpp)

# The functions under test (main app).
FUNCTIONS = {
    "close": 0x100BDF84, "free": 0x100BAC60, "load_emitter": 0x100BBC54, "kill_lib": 0x100BBF78, "tick": 0x100BE564,
    "density": 0x100BE654, "emitter_status": 0x100BF110, "effecter_status": 0x100BF1B0, "kill_emitter": 0x100BE1E8,
    "kill_2d": 0x100BE494, "launch": 0x100BE070, "pos": 0x100BE150, "mask": 0x100BE260, "life": 0x100BE2E8,
    "scale": 0x100BE36C, "random_pos": 0x100BE3F4, "bbox": 0x100BA1E0, "free_list": 0x100BA298,
}
# What the original calls that the test stands in for.
LBMEM_FREE_IMPORT = "LbMem_Free__FPv"
DO_EMITTERS, DO_EFFECTERS, DO_PARTICLE, TP_READ = 0x100BCBB4, 0x100BD1F0, 0x100BD63C, 0x10004260
REAL = set(PI.PAR_REAL) | set(FUNCTIONS.values())


# ----------------------------------------------------------------- the host's bytes in the original's order

def _runs(*parts):
    """A layout as (offset, width) runs of fields that the loader swaps; the rest are plain bytes."""
    out = []
    for part in parts:
        out.extend(part)
    return out


def _words(start, stop):
    return [(offset, 4) for offset in range(start, stop, 4)]


# One emitter definition or live emitter (PARLibraryEmitter): the order of the loader's swaps.
EMITTER_RUNS = _runs(
    [(0x00, 2), (0x02, 2), (0x06, 2), (0x08, 2), (0x0A, 2), (0x0C, 2), (0x0E, 2), (0x10, 2), (0x12, 2)],
    _words(0x14, 0x24), [(0x24, 2), (0x26, 2), (0x28, 2), (0x2A, 2)], _words(0x2C, 0x50), [(0x50, 2), (0x52, 2)],
    _words(0x54, 0x64), [(0x64, 2), (0x66, 2)], [(0x70, 4), (0x74, 2), (0x76, 2)], _words(0x78, 0x94),
    [(0x94, 2), (0x96, 2)], _words(0x98, 0xA0), [(0xA0, 2), (0xA2, 2), (0xA4, 2), (0xA6, 2), (0xA8, 2)],
    _words(0xAC, 0xB8), [(0xB8, 2), (0xBC, 2), (0xBE, 2), (0xC4, 2), (0xC6, 2)], _words(0xC8, 0xD0),
    [(0xD0, 2), (0xD2, 2)], _words(0xD4, 0x118), _words(0x128, 0x140))
# One effecter definition or live effecter (PARLibraryEffecter).
EFFECTER_RUNS = _runs([(0x00, 4), (0x04, 4), (0x08, 2)], _words(0x0C, 0x44), [(0x46, 2), (0x4A, 2), (0x4C, 4)],
                      [(0x60, 2), (0x62, 2)])
PARTICLE_RUNS = [(0x30, 2), (0x32, 2)]
HEAD_RUNS = [(0, 2), (2, 2), (4, 2), (6, 2), (8, 2)]
SINE_RUNS = [(0, 4)]


def swap_runs(data, runs, stride):
    """The fields of every `stride` bytes of `data` reversed (the host's order to the PowerPC's and back)."""
    out = bytearray(data)
    for base in range(0, len(out) - stride + 1, stride):
        for offset, width in runs:
            out[base + offset:base + offset + width] = out[base + offset:base + offset + width][::-1]
    return bytes(out)


def library_to_ppc(data):
    """The host's library bytes in the original's order. The unit library swaps like the Mac, so only the 1000 that
    PAR_LoadLibrary writes into a zero 0xa2 halfword (in the host's own order) is turned (test_ParticlesInit.fix_library; no
    record the cases hand to PAR_LoadLibEmitter holds the pair that it would mistake for it)."""
    return PI.fix_library(data)


# ----------------------------------------------------------------- describing a case

EMITTER_FIELDS = {
    "inuse": (0x00, 2), "paused": (0x02, 2), "twod": (0x05, 1), "first": (0x06, 2), "count": (0x08, 2), "serial": (0x0A, 2),
    "posx": (0x14, 4), "posy": (0x18, 4), "posz": (0x1C, 4), "life": (0x20, 4), "mask": (0x2A, 2),
    "launchx": (0x38, 4), "launchy": (0x3C, 4), "launchz": (0x40, 4), "randx": (0x44, 4), "randy": (0x48, 4),
    "randz": (0x4C, 4), "dword": (0x60, 4), "dshort": (0x64, 2), "db0": (0x68, 1), "db1": (0x69, 1), "db2": (0x6A, 1),
    "db3": (0x6B, 1), "scale": (0xA2, 2), "lscale": (0xB0, 4), "next": (0xD0, 2), "prev": (0xD2, 2),
    "minx": (0x128, 4), "miny": (0x12C, 4), "minz": (0x130, 4), "maxx": (0x134, 4), "maxy": (0x138, 4), "maxz": (0x13C, 4),
}
EFFECTER_FIELDS = {"serial": (0x08, 2), "twod": (0x45, 1), "life": (0x4C, 4), "next": (0x60, 2), "prev": (0x62, 2)}
HEAD_FIELDS = {"emitters_used": (0, 2), "effecters_used": (2, 2), "emitters_free": (4, 2), "effecters_free": (6, 2),
               "particles_free": (8, 2)}
GLOBAL_FIELDS = {"running": 0, "busy": 1, "balance": 2, "word_a": 3, "word_b": 4}
HEADER_FIELDS = {"f00": (0x00, 4), "ticks": (0x04, 4), "lowspec": (0x08, 4), "f10": (0x10, 4), "f14": (0x14, 4),
                 "f18": (0x18, 2), "f1a": (0x1A, 2), "density": (0x20, 4)}


class PCase(dict):
    """A particle case; it prints as its label."""

    def __repr__(self):
        return f"<{self['label']}>"


def pcase(label, ops, prep=(), **kw):
    """ops: the calls, (name, args...), in order. prep: the steps that put the state in a condition (below). fill: the byte
    every part of the state, the library and the sines holds first; running: the running flag; pool: None, a list of
    (next, prev) links for a pool of the case, or 'state' (the pool the system made); drawlist: None or 'own' (a block of the
    case); file: the bytes the file calls serve; queues: the fakes' result queues."""
    case = dict(label=label, ops=list(ops), prep=list(prep), fill=0xA5, running=1, maximum=100, pool=None, drawlist=None,
                file=b"", queues={})
    case.update(kw)
    return PCase(case)


def poke(region, offset, width, value):
    """Prep step: write `value` in `width` bytes at `offset` of a region (0 the header, 1 the emitters, 2 the effecters,
    3 the chain heads, 4 the library, 5 the pool, 6 a global)."""
    return ("poke", region, offset, width, value)


def emitter(index, **fields):
    return [poke(1, index * 0x140 + EMITTER_FIELDS[name][0], EMITTER_FIELDS[name][1], value) for name, value in fields.items()]


def effecter(index, **fields):
    return [poke(2, index * 0x68 + EFFECTER_FIELDS[name][0], EFFECTER_FIELDS[name][1], value) for name, value in fields.items()]


def heads(**fields):
    return [poke(3, HEAD_FIELDS[name][0], HEAD_FIELDS[name][1], value) for name, value in fields.items()]


def header(**fields):
    return [poke(0, HEADER_FIELDS[name][0], HEADER_FIELDS[name][1], value) for name, value in fields.items()]


def globals_(**fields):
    return [poke(6, GLOBAL_FIELDS[name], 4, value) for name, value in fields.items()]


def library(index, offset, width, value):
    return [poke(4, index * 0x140 + offset, width, value)]


def emitter_chain(indices):
    """Pokes that chain the emitters `indices` as the emitters in use (the chain heads and the links)."""
    out = heads(emitters_used=indices[0] if indices else -1)
    for position, index in enumerate(indices):
        out += emitter(index, prev=indices[position - 1] if position else -1,
                       next=indices[position + 1] if position + 1 < len(indices) else -1)
    return out


def effecter_chain(indices):
    out = heads(effecters_used=indices[0] if indices else -1)
    for position, index in enumerate(indices):
        out += effecter(index, prev=indices[position - 1] if position else -1,
                        next=indices[position + 1] if position + 1 < len(indices) else -1)
    return out


def from_record(record, emitter_index):
    """Prep step: a live emitter made as PAR_InitEmitter makes one, a copy of a library definition (`record`: the 0x140 bytes
    as the library file has them, which is the host's order)."""
    return [("record", bytes(record), emitter_index)]


def real_start(density=1024, maximum=100):
    """Prep step: the game's own start-up: PAR_InitParticleSystem, then PAR_LoadLibrary on Tp2.plb."""
    return [("real", density, maximum)]


def handle(index, serial):
    return ((serial & 0xFFFF) << 16) | (index & 0xFFFF)


def particle_chain(order):
    """Links for a pool: the particles of `order` chained one after the other; the others stand alone."""
    pool = {}
    for position, index in enumerate(order):
        pool[index] = (order[position + 1] if position + 1 < len(order) else -1, order[position - 1] if position else -1)
    return pool


def pool_links(count, *chains):
    """A pool of `count` particles with the chains given as lists of indices (the rest: -1, -1)."""
    links = [(-1, -1)] * count
    for order in chains:
        for index, link in particle_chain(order).items():
            links[index] = link
    return links


# ----------------------------------------------------------------- the PowerPC side

PLB_DATA = PI.PLB.read_bytes() if PI.PLB.is_file() else None


class PpcWorld:
    """The blocks a case needs in emulator memory and their labels."""

    def __init__(self, emu, case):
        self.case = case
        self.other = emu.alloc(16, b"\xcc" * 16)
        self.list_block = emu.alloc(0xAF0, b"\xcc" * 0xAF0) if case["drawlist"] == "own" else 0
        self.pool = 0
        if isinstance(case["pool"], list):
            total = 2 * MARGIN_POOL + len(case["pool"]) * 0x34
            self.pool = emu.alloc(total, b"\xcc" * total) + MARGIN_POOL
        self.box = emu.alloc(24, b"\xa5" * 24)
        self.fixed = {emu.w26a_file: LABEL_FILE, self.other: LABEL_OTHER}
        if self.pool:
            self.fixed[self.pool] = LABEL_POOL
        if self.list_block:
            self.fixed[self.list_block] = LABEL_LIST

    def pointer(self, emu, kind):
        """The value of a pointer argument: None-like 'null', the case's 'other' block, the system's drawing list 'list', the
        pool or the file object."""
        if kind == "null":
            return 0
        if kind == "other":
            return self.other
        if kind == "list":
            return emu.u32(DRAWLIST)
        if kind == "pool":
            return self.pool
        return emu.w26a_file


def poke_address(region, offset, world):
    if region == 0:
        return STATE + offset
    if region == 1:
        return EMITTERS + offset
    if region == 2:
        return EFFECTERS + offset
    if region == 3:
        return HEADS + offset
    if region == 4:
        return LIB + offset
    if region == 5:
        return world.pool + offset
    return {0: RUNNING, 1: BUSY, 2: BALANCE, 3: W_A, 4: W_B}[offset]


def particle_stubs(e, case):
    """The callees, recorded in the order they are called (the host's fakes record the same)."""
    st = CL.Stubs()
    st.real = set(REAL)

    def mem_alloc(e):
        # r3: the size. A block of 2 MB or more is not made (nothing touches it): an address of its own.
        L, size = e.cl, e.r(3)
        refuse = L.ret("LbMem_Alloc fails") != 0 or size >= 0x40000000
        if size >= 0x200000:
            L.big = getattr(L, "big", 0) + 1
            block = 0 if refuse else 0x6F000000 + 0x1000 * L.big
        else:
            block = 0 if refuse else e.alloc(max(size, 1), b"\xa5" * max(size, 1))
        L.rec("LbMem_Alloc", (size, L.newlab(block) if block else 0))
        e.set_r(3, block)

    def mem_free(e):
        e.cl.rec("LbMem_Free", (e.cl.lab(e.r(3)),))

    def game_assert(e):
        e.cl.rec("GAME_ASSERTMSG", (T.s32(e.r(3)),), (e.cstr(e.r(4))[:159],))

    def tp_open(e):
        L = e.cl
        L.rec("TPFile_Open", (), (e.cstr(e.r(3))[:159], e.cstr(e.r(4))[:159]))
        L.position = 0
        e.set_r(3, e.w26a_file if L.ret("TPFile_Open", 1) != 0 else 0)

    def tp_read(e):
        # r3: the destination, r4: the item size, r5: the count, r6: the file. What the file has goes in, the rest of the
        # buffer is zeroed; the answer is the number of whole items.
        L, dest, size, count = e.cl, e.r(3), e.r(4), e.r(5)
        inside = LIB <= dest < LIB + LIB_SIZE
        L.rec("TPFile_Read", ((dest - LIB) // 0x140 if inside else -1, size, count, L.lab(e.r(6))))
        length = (size * count) & 0xFFFFFFFF
        chunk = L.file[L.position:L.position + length]
        L.position += len(chunk)
        if length:
            e.write(dest, chunk + bytes(length - len(chunk)))
        e.set_r(3, len(chunk) // size if size else 0)

    def lb_read(e):
        # r3: the TbError's place, r4: the file, r5: the buffer, r6: the length, r7: bytes done.
        L, buffer, length = e.cl, e.r(5), e.r(6)
        L.rec("LbFile_Read", (L.lab(e.r(4)), length))
        chunk = L.file[L.position:L.position + length]
        L.position += len(chunk)
        if length:
            e.write(buffer, chunk + bytes(length - len(chunk)))
        if e.r(7):
            e.write(e.r(7), struct.pack(">I", len(chunk)))
        e.write(e.r(3), bytes(4))

    def lb_seek(e):
        L, offset = e.cl, T.s32(e.r(4))
        L.rec("LbFile_Seek", (L.lab(e.r(3)), offset, e.r(5)))
        L.position = max(0, min(len(L.file), L.position + offset))
        e.set_r(3, 0)

    def lb_close(e):
        e.cl.rec("LbFile_Close", (e.cl.lab(e.r(4)),))
        e.write(e.r(3), bytes(4))

    def swap_block(e):
        # The PowerPC side is big-endian: the file's words really swap. The event says whether the buffer is in the library.
        a, n = e.r(3), T.s32(e.r(4))
        inside = LIB <= a < LIB + LIB_SIZE
        e.cl.rec("SwapBlock32", (n, 1 if inside else 0))
        for i in range(max(n, 0)):
            e.write(a + 4 * i, e.read(a + 4 * i, 4)[::-1])

    def do_emitters(e):
        # The fake of DoEmitters can leave the chain of emitters in use changed.
        L = e.cl
        L.rec("DoEmitters")
        head = L.ret("DoEmitters head", NO_CHANGE)
        if head != NO_CHANGE:
            e.write(HEADS, struct.pack(">h", T.s16(head)))

    def do_effecters(e):
        e.cl.rec("DoEffecters")

    def do_particle(e):
        # The fake of DoAParticle can change the link of the particle it was called for.
        L, particle = e.cl, T.s32(e.r(3))
        L.rec("DoAParticle", (particle,))
        link = L.ret(f"DoAParticle next:{particle}", NO_CHANGE)
        if link != NO_CHANGE:
            e.write(e.u32(STATE + 0x28) + particle * 0x34 + 0x30, struct.pack(">h", T.s16(link)))

    st.imp("LbMem_Alloc__FUi", mem_alloc)
    st.imp(LBMEM_FREE_IMPORT, mem_free)
    st.imp("LbFile_Read__FPvPvUlPUl", lb_read)
    st.imp("LbFile_Seek__FPvl14TbFileSeekMode", lb_seek)
    st.imp("LbFile_Close__FPv", lb_close)
    st.imp("_USwapBlock32__13SamsUtilitiesFPUll", swap_block)
    st.internal(PI.ASSERT_FN, game_assert)
    st.internal(PI.OPEN_FN, tp_open)
    st.internal(TP_READ, tp_read)
    st.internal(DO_EMITTERS, do_emitters)
    st.internal(DO_EFFECTERS, do_effecters)
    st.internal(DO_PARTICLE, do_particle)
    return st


def pool_bytes(links, big):
    """A pool of the case in the byte order of a side: 0xcc everywhere but the two links of every particle."""
    order = ">" if big else "<"
    out = bytearray()
    for link_next, link_prev in links:
        out += b"\xcc" * 0x30 + struct.pack(order + "hh", link_next, link_prev)
    return bytes(out)


def prepare_ppc(e, case, world):
    fill = case["fill"]
    e.write(STATE, bytes([fill]) * STATE_SIZE)
    e.write(LIB, bytes([fill]) * LIB_SIZE)
    e.write(SIN_TABLE, bytes([fill]) * 0x800)
    e.write(RUNNING, struct.pack(">i", case["running"]))
    e.write(BUSY, struct.pack(">i", fill))
    e.write(DRAWLIST, struct.pack(">I", world.list_block))
    e.write(W_A, struct.pack(">I", 0xA5A5A5A5))
    e.write(W_B, struct.pack(">I", 0xA5A5A5A5))
    e.write(BALANCE, bytes(4))
    e.write(STATE + 0x0C, struct.pack(">i", case["maximum"]))
    e.write(STATE + 0x28, struct.pack(">I", world.pool))
    if isinstance(case["pool"], list):
        total = len(case["pool"]) * 0x34
        e.write(world.pool - MARGIN_POOL, b"\xcc" * (2 * MARGIN_POOL + total))
        e.write(world.pool, pool_bytes(case["pool"], True))


def prep_ppc(e, case, world):
    """The prep steps of a case, on the PowerPC side."""
    for step in case["prep"]:
        if step[0] == "poke":
            _, region, offset, width, value = step
            e.write(poke_address(region, offset, world), (value & ((1 << (8 * width)) - 1)).to_bytes(width, "big"))
        elif step[0] == "record":
            e.write(EMITTERS + step[2] * 0x140, swap_runs(step[1], EMITTER_RUNS, 0x140))
        else:
            _, density, maximum = step
            e.write(RUNNING, bytes(4))
            e.cl.file, e.cl.position = PLB_DATA, 0
            T.call_ppc(e, PAR_INIT, [0, T.u32(density), T.u32(maximum)])
            path = e.alloc(32, b"Data:Particle:Tp2.plb\0")
            T.call_ppc(e, PAR_LOAD, [path])
            e.cl.file, e.cl.position = case["file"], 0


def ppc_op(e, world, op, args):
    """One call, on the PowerPC side: the answer (or None)."""
    address = FUNCTIONS[op]
    if op == "tick":
        return T.s32(T.call_ppc(e, address, [])[0])
    if op == "load_emitter":
        return T.s32(T.call_ppc(e, address, [world.pointer(e, args[0])])[0])
    if op in ("emitter_status", "effecter_status"):
        return T.call_ppc(e, address, [T.u32(args[0])])[0]
    if op in ("close", "kill_2d"):
        call = []
    elif op in ("free", "free_list"):
        call = [world.pointer(e, args[0])]
    elif op == "bbox":
        call = [T.u32(args[0]), world.box, world.box + (0 if args[1] == "same" else 12)]
    else:
        call = [T.u32(value) for value in args]
    T.call_ppc(e, address, call)
    return None


def snapshot_ppc(e, case, world):
    """Everything the calls may have touched, as numbers and bytes in one shape for both sides (the host's records are turned
    into the PowerPC's order)."""
    L = e.cl

    def s32(a):
        return struct.unpack(">i", e.read(a, 4))[0]

    def s16(a):
        return struct.unpack(">h", e.read(a, 2))[0]

    sines, callback, pool = e.u32(STATE + 0x1C), e.u32(STATE + 0x24), e.u32(STATE + 0x28)
    out = {
        "header": [s32(STATE), s32(STATE + 4), s32(STATE + 8), s32(STATE + 0xC), s32(STATE + 0x10), s32(STATE + 0x14),
                   s16(STATE + 0x18), s16(STATE + 0x1A), 1 if sines == SIN_TABLE else 0 if sines == 0 else 2,
                   s32(STATE + 0x20), 0 if callback == 0 else 1 if callback == DUMMY_TV else 2, L.lab(pool)],
        "globals": [s32(RUNNING), s32(BUSY), L.lab(e.u32(DRAWLIST)), e.u32(W_A), e.u32(W_B), s32(BALANCE)],
        "emitters": e.read(EMITTERS, 0x9600),
        "effecters": e.read(EFFECTERS, 0x820),
        "heads": e.read(HEADS, 0x1C),
        "library": e.read(LIB, LIB_SIZE),
        "sines": e.read(SIN_TABLE, 0x800),
    }
    if isinstance(case["pool"], list):
        out["pool"] = e.read(world.pool - MARGIN_POOL, 2 * MARGIN_POOL + len(case["pool"]) * 0x34)
    elif case["pool"] == "state" and pool != 0 and s32(STATE + 0xC) > 0:
        out["pool"] = e.read(pool, s32(STATE + 0xC) * 0x34)
    return out


def particle_ppc(e, case):
    PI.once(e, "std", T.install_std_stubs)
    if not hasattr(e, "w26a_file"):
        e.w26a_file = T.persist_alloc(e, 16)
    saved = e.read(BALANCE, SAVED_SIZE)
    try:
        world = PpcWorld(e, case)
        e.cl = CL.PpcLog(world.fixed, case["queues"])
        e.cl.file, e.cl.position = case["file"], 0
        prepare_ppc(e, case, world)
        st = particle_stubs(e, case)
        old = st.activate(e)
        try:
            prep_ppc(e, case, world)
            answers = [ppc_op(e, world, op[0], op[1:]) for op in case["ops"]]
        finally:
            CL.Stubs.deactivate(e, old)
        out = snapshot_ppc(e, case, world)
        out.update(answers=answers, box=list(struct.unpack(">6i", e.read(world.box, 24))), log=e.cl.log)
        return out
    finally:
        e.write(BALANCE, saved)


# ----------------------------------------------------------------- the host side

HOST_FUNCTIONS = {
    "tick": ("tpw_pa_tick", C.c_int32, ()), "close": ("tpw_pa_close", None, ()), "free": ("tpw_pa_free", None, (C.c_void_p,)),
    "load_emitter": ("tpw_pa_load_emitter", C.c_int32, (C.c_void_p,)), "kill_lib": ("tpw_pa_kill_lib", None, (C.c_uint32,)),
    "density": ("tpw_pa_density", None, (C.c_int32,)), "emitter_status": ("tpw_pa_emitter_status", C.c_int32, (C.c_uint32,)),
    "effecter_status": ("tpw_pa_effecter_status", C.c_int32, (C.c_uint32,)),
    "kill_emitter": ("tpw_pa_kill_emitter", None, (C.c_int32,)), "kill_2d": ("tpw_pa_kill_2d", None, ()),
    "launch": ("tpw_pa_launch", None, (C.c_int32,) * 4), "pos": ("tpw_pa_pos", None, (C.c_int32,) * 4),
    "mask": ("tpw_pa_mask", None, (C.c_int32,) * 2), "life": ("tpw_pa_life", None, (C.c_int32,) * 2),
    "scale": ("tpw_pa_scale", None, (C.c_int32,) * 2), "random_pos": ("tpw_pa_random_pos", None, (C.c_int32,) * 4),
    "bbox": ("tpw_pa_bbox", None, (C.c_uint32, C.c_void_p, C.c_void_p)), "free_list": ("tpw_pa_free_list", None, (C.c_void_p,)),
}


def bind(lib):
    if getattr(lib, "pa_bound", False):
        return
    lib.pa_bound = True
    c32, vp = C.c_int32, C.c_void_p
    for name, restype, argtypes in HOST_FUNCTIONS.values():
        fn = getattr(lib, name)
        fn.restype, fn.argtypes = restype, list(argtypes)
    for name, restype, argtypes in (
            ("tpw_pa_prepare", None, (c32, c32, c32, vp)), ("tpw_pa_poke", None, (c32, c32, c32, C.c_int64)),
            ("tpw_pa_poke_bytes", None, (c32, c32, C.c_char_p, c32)), ("tpw_pa_set_file", None, (C.c_char_p, c32)),
            ("tpw_pa_file_label", None, ()), ("tpw_pa_file_object", vp, ()), ("tpw_pa_init", None, (vp, c32, c32)), ("tpw_pa_load", c32, (C.c_char_p,)),
            ("tpw_pa_header", None, (vp,)), ("tpw_pa_globals", None, (vp,)), ("tpw_pa_drawing_list", vp, ()),
            ("tpw_pa_region", vp, (c32, C.POINTER(c32))), ("tpw_cl_lab", c32, (vp,))):
        fn = getattr(lib, name)
        fn.restype, fn.argtypes = restype, list(argtypes)


class HostWorld:
    """The buffers a case needs on the host and their labels."""

    def __init__(self, case):
        self.other = C.create_string_buffer(b"\xcc" * 16, 16)
        self.list_block = C.create_string_buffer(b"\xcc" * 0xAF0, 0xAF0) if case["drawlist"] == "own" else None
        self.pool_buffer = None
        if isinstance(case["pool"], list):
            total = 2 * MARGIN_POOL + len(case["pool"]) * 0x34
            self.pool_buffer = C.create_string_buffer(b"\xcc" * total, total)
        self.box = C.create_string_buffer(b"\xa5" * 24, 24)
        self.fixed = {LABEL_OTHER: C.addressof(self.other)}
        if self.pool_buffer is not None:
            self.fixed[LABEL_POOL] = C.addressof(self.pool_buffer) + MARGIN_POOL
        if self.list_block is not None:
            self.fixed[LABEL_LIST] = C.addressof(self.list_block)

    @property
    def pool_address(self):
        return C.addressof(self.pool_buffer) + MARGIN_POOL if self.pool_buffer is not None else 0

    def pointer(self, lib, kind):
        """The value of a pointer argument (see PpcWorld.pointer)."""
        if kind == "null":
            return None
        if kind == "other":
            return C.addressof(self.other)
        if kind == "list":
            return lib.tpw_pa_drawing_list()
        if kind == "pool":
            return self.pool_address or None
        return lib.tpw_pa_file_object()


def host_region(lib, which):
    size = C.c_int32(0)
    address = lib.tpw_pa_region(which, C.byref(size))
    return address, size.value


def snapshot_host(lib, case, world):
    header, globs = (C.c_int64 * 12)(), (C.c_int64 * 8)()
    lib.tpw_pa_header(header)
    lib.tpw_pa_globals(globs)

    def region(which):
        address, size = host_region(lib, which)
        return C.string_at(address, size) if address and size > 0 else b""

    out = {
        "header": list(header), "globals": list(globs)[:6],
        "emitters": swap_runs(region(0), EMITTER_RUNS, 0x140), "effecters": swap_runs(region(1), EFFECTER_RUNS, 0x68),
        "heads": swap_runs(region(2), HEAD_RUNS, 0x1C), "library": library_to_ppc(region(3)),
        "sines": swap_runs(region(5), SINE_RUNS, 4),
    }
    if isinstance(case["pool"], list):
        raw = world.pool_buffer.raw
        body = swap_runs(raw[MARGIN_POOL:len(raw) - MARGIN_POOL], PARTICLE_RUNS, 0x34)
        out["pool"] = raw[:MARGIN_POOL] + body + raw[len(raw) - MARGIN_POOL:]
    elif case["pool"] == "state":
        pool = region(4)
        if pool:
            out["pool"] = swap_runs(pool, PARTICLE_RUNS, 0x34)
    return out


def prep_host(lib, case, world):
    """The prep steps of a case, on the host side."""
    for step in case["prep"]:
        if step[0] == "poke":
            _, region, offset, width, value = step
            lib.tpw_pa_poke(region, offset, width, value & ((1 << (8 * width)) - 1))
        elif step[0] == "record":
            lib.tpw_pa_poke_bytes(1, step[2] * 0x140, step[1], len(step[1]))
        else:
            _, density, maximum = step
            lib.tpw_pa_poke(6, 0, 4, 0)
            lib.tpw_pa_set_file(PLB_DATA, len(PLB_DATA))
            lib.tpw_pa_init(None, T.s32(density), T.s32(maximum))
            lib.tpw_pa_load(b"Data:Particle:Tp2.plb")
            lib.tpw_pa_set_file(case["file"], len(case["file"]))


def host_op(lib, world, op, args):
    """One call, on the host side: the answer (or None)."""
    name, restype, argtypes = HOST_FUNCTIONS[op]
    fn = getattr(lib, name)
    if op == "tick":
        return fn()
    if op == "load_emitter":
        return fn(world.pointer(lib, args[0]))
    if op in ("emitter_status", "effecter_status"):
        return fn(T.u32(args[0]))
    if op in ("close", "kill_2d"):
        fn()
    elif op in ("free", "free_list"):
        fn(world.pointer(lib, args[0]))
    elif op == "bbox":
        address = C.addressof(world.box)
        fn(T.u32(args[0]), address, address + (0 if args[1] == "same" else 12))
    elif op == "kill_lib":
        fn(T.u32(args[0]))
    else:
        fn(*[T.s32(value) for value in args])
    return None


def particle_host(lib, case):
    bind(lib)
    world = HostWorld(case)
    CL.host_begin(lib, world.fixed, case["queues"])
    lib.tpw_pa_file_label()
    lib.tpw_pa_prepare(case["fill"], case["running"], case["maximum"], world.pool_address or None)
    if world.list_block is not None:
        lib.tpw_pa_poke(6, 5, 8, C.addressof(world.list_block))
    if isinstance(case["pool"], list):
        raw = bytearray(world.pool_buffer.raw)
        raw[MARGIN_POOL:MARGIN_POOL + len(case["pool"]) * 0x34] = pool_bytes(case["pool"], False)
        C.memmove(world.pool_buffer, bytes(raw), len(raw))
    lib.tpw_pa_set_file(case["file"], len(case["file"]))
    prep_host(lib, case, world)
    answers = [host_op(lib, world, op[0], op[1:]) for op in case["ops"]]
    log = CL.host_end(lib)
    out = snapshot_host(lib, case, world)
    out.update(answers=answers, box=list(struct.unpack("<6i", world.box.raw)), log=log)
    return out


# ----------------------------------------------------------------- the cases

FILLS = (0x00, 0xA5, 0xFF)
PLB_RECORDS = [PLB_DATA[8 + i * 0x140:8 + (i + 1) * 0x140] for i in range(105)] if PLB_DATA else []
INT_EDGES = [0, 1, -1, 2, -2, 15, 16, 17, -15, -16, -17, 1023, 1024, 1025, -1023, -1024, -1025, 0x7FFFFFFF, -0x80000000,
             0x40000000, 0x7FFF, -0x8000, 0xFFFF, 0x10000, 0x12345678, -0x12345678]
LAUNCH_SCALES = [0, 1, -1, 512, 1000, 1024, 1025, 2048, -1024, 0x7FFFFFFF, -0x80000000, 0x10000]
SHORT_EDGES = [0, 1, -1, 2, 5, 100, 1000, 1024, 0x4000, 0x7FFF, -0x8000, -2]
BYTE_EDGES = [0, 1, 2, 3, 0x20, 0x40, 0x7F, 0x80, 0x81, 0xC0, 0xFE, 0xFF]
DENSITIES = [0, 1, 2, 100, 511, 512, 1000, 1023, 1024, 1025, 1030, 2047, 2048, 4096, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, -1024]
WORD_EDGES = [0, 1, 2, 5, 33, 100, 1000, 1023, 1024, 1025, 0x100000, 0x7FFFFF, 0x7FFFFFFF, -1, -0x80000000, -1024]


def clean_record(raw):
    """A record for PAR_LoadLibEmitter: the pair that the 1000 PAR_LoadLibrary writes would be mistaken for (see
    library_to_ppc) is changed."""
    record = bytearray(raw)
    if record[0xA2:0xA4] == b"\x03\xe8":
        record[0xA2] = 4
    return bytes(record)


def random_record(r, name=None):
    """A random emitter definition; `name`: its name's first byte when the case wants one (0: an empty name)."""
    record = bytearray(r.randbytes(0x140))
    if name is not None:
        record[0x118] = name
    return clean_record(record)


def case_fill(r):
    return r.choice(FILLS)


def int_value(r):
    return r.choice((r.choice(INT_EDGES), r.randint(-0x80000000, 0x7FFFFFFF), r.randint(-4096, 4096)))


# ---- PAR_Free and PAR_FreeDrawingList

def free_edges():
    out = []
    for kind in ("null", "other", "list"):
        for drawlist in (None, "own"):
            out.append(pcase(f"free {kind}, drawing list {drawlist}", [("free", kind)], drawlist=drawlist))
    out.append(pcase("free the pool", [("free", "pool")], pool=pool_links(3)))
    out.append(pcase("free twice", [("free", "other"), ("free", "other")]))
    return out


def free_random(r):
    return pcase("random free", [("free", r.choice(("null", "other", "list", "pool")))], fill=case_fill(r),
                 running=r.choice((0, 1)), drawlist=r.choice((None, "own")), pool=r.choice((None, pool_links(2))))


def free_list_edges():
    out = []
    for running in (0, 1, 2, -1):
        for kind in ("null", "other", "list"):
            for drawlist in (None, "own"):
                for busy in (0, 1, 0xA5):
                    out.append(pcase(f"free list {kind}, running {running}, list {drawlist}, busy {busy}",
                                     [("free_list", kind)], running=running, drawlist=drawlist, prep=globals_(busy=busy)))
    out.append(pcase("free list twice", [("free_list", "list"), ("free_list", "list")], drawlist="own", prep=globals_(busy=1)))
    out.append(pcase("free list, then another block", [("free_list", "list"), ("free_list", "other")], drawlist="own",
                     prep=globals_(busy=1)))
    return out


def free_list_random(r):
    return pcase("random free list", [("free_list", r.choice(("null", "other", "list", "list")))], fill=case_fill(r),
                 running=r.choice((0, 1, 1, 1, 2)), drawlist=r.choice((None, "own", "own")),
                 prep=globals_(busy=r.choice((0, 1, 0xA5, r.getrandbits(32)))))


# ---- PAR_CloseParticleSystem

def close_edges():
    out = []
    for running in (0, 1, 2, -1, 0x10000):
        for pool in (None, pool_links(4)):
            for drawlist in (None, "own"):
                for balance in (0, 1, -1):
                    out.append(pcase(f"close running {running}, pool {pool is not None}, list {drawlist}, balance {balance}",
                                     [("close",)], running=running, pool=pool, drawlist=drawlist,
                                     prep=globals_(balance=balance) + header(f00=0x1234, ticks=77)))
    for fill in FILLS:
        out.append(pcase(f"close fill {fill:#x}", [("close",)], fill=fill, pool=pool_links(2), drawlist="own"))
    out.append(pcase("close twice", [("close",), ("close",)], pool=pool_links(2), drawlist="own", prep=globals_(balance=3)))
    out.append(pcase("close balance big", [("close",)], pool=pool_links(2), drawlist="own", prep=globals_(balance=0x7FFFFFFF)))
    for density, maximum in ((1024, 100), (500, 150), (0, 7000)):
        out.append(pcase(f"close after the real start {density}/{maximum}", [("close",)], pool="state",
                         prep=real_start(density, maximum)))
        out.append(pcase(f"close twice after the real start {density}/{maximum}", [("close",), ("close",)], pool="state",
                         prep=real_start(density, maximum)))
        out.append(pcase(f"close after the real start, unbalanced {density}/{maximum}", [("close",)], pool="state",
                         prep=real_start(density, maximum) + globals_(balance=2)))
    return out


def close_random(r):
    return pcase("random close", [("close",)] * r.choice((1, 1, 2)), fill=case_fill(r), running=r.choice((0, 1, 1, 1, 2)),
                 pool=r.choice((None, pool_links(r.randint(1, 5)))), drawlist=r.choice((None, "own")),
                 prep=globals_(balance=r.choice((0, 0, 0, r.getrandbits(32)))) + header(f00=r.getrandbits(32)))


# ---- PAR_LoadLibEmitter and PAR_KillLibEmitter

def load_case(label, file, ops=(("load_emitter", "file"),), fill=0x00, running=1, names=None, prep=(), **kw):
    """names: {library slot: the first byte of its name}."""
    pokes = []
    for slot, byte in (names or {}).items():
        pokes += library(slot, 0x118, 1, byte)
    return pcase(label, list(ops), prep=pokes + list(prep), fill=fill, running=running, file=file, **kw)


def load_edges():
    r = random.Random(2601)
    out = []
    sample = PLB_RECORDS[3] if PLB_RECORDS else random_record(r, 0x41)
    if not PLB_RECORDS:
        print(f"test_Particles: {PI.PLB} is missing; the cases on the real library are skipped")
    for fill in FILLS:
        out.append(load_case(f"load, not running, fill {fill:#x}", sample, running=0, fill=fill))
    out.append(load_case("load, running flag 2", sample, running=2))
    out.append(load_case("load, running flag -1", sample, running=-1))
    for slot in (0, 1, 2, 50, 103, 104):
        out.append(load_case(f"load into the only free slot {slot}", sample, fill=0xA5, names={slot: 0}))
    out.append(load_case("load, slots 5 and 9 free", sample, fill=0xA5, names={5: 0, 9: 0}))
    out.append(load_case("load, slots 103 and 104 free", sample, fill=0xFF, names={103: 0, 104: 0}))
    out.append(load_case("load, every slot free", sample, fill=0x00))
    for fill in (0xA5, 0xFF):
        out.append(load_case(f"load, library full of {fill:#x}", sample, fill=fill))
    for byte in (0, 1, 0x41, 0x7F, 0x80, 0xA5, 0xFF):
        out.append(load_case(f"load, slot 0 name starts with {byte:#x}", sample, fill=0, names={0: byte}))
    # the name is read from its first byte only: an empty first byte with more text behind it is a free slot
    out.append(load_case("load, slot 0 name is empty then text", sample, fill=0xA5, names={0: 0, 1: 0}, prep=[
        poke(4, 0x119, 4, 0x41424344)]))
    # every real definition
    for index, record in enumerate(PLB_RECORDS):
        out.append(load_case(f"load real definition {index}", record, fill=(0x00, 0xA5, 0xFF)[index % 3],
                             names={index % 105: 0} if index % 3 else None))
    # short files and long files
    for length in (0, 1, 2, 3, 4, 0x117, 0x118, 0x13F, 0x140, 0x141, 0x280):
        padded = (sample + sample + sample)[:length]
        out.append(load_case(f"load, file of {length} bytes", padded, fill=0x00))
    # a script: loads, kills and loads again
    first, second, third = random_record(r, 0x41), random_record(r, 0x42), random_record(r, 0x43)
    load = ("load_emitter", "file")
    out.append(load_case("load three in a row", first + second + third, ops=[load, load, load]))
    out.append(load_case("load a fourth from a file of three", first + second + third, ops=[load] * 4))
    out.append(load_case("load, kill the slot, load again", first + second + third,
                         ops=[load, load, ("kill_lib", 0), load, ("kill_lib", 1), ("kill_lib", 1), load]))
    out.append(load_case("load a definition with an empty name twice", random_record(r, 0) + random_record(r, 0), ops=[load] * 2))
    out.append(load_case("load a definition with an empty name, then one with a name",
                         random_record(r, 0) + random_record(r, 0x41), ops=[load] * 3))
    out.append(load_case("load, full library, kill, load", first + second, fill=0xA5,
                         ops=[load, ("kill_lib", 70), load, load, ("kill_lib", 70), load]))
    out.append(load_case("load, a read-only record: slot 0 and the second record into slot 1", first + second, ops=[load, load],
                         names={0: 0}))
    # on the game's own library
    for density, maximum in ((1024, 100), (400, 300)):
        out.append(load_case(f"load into the real library, full {density}", sample, pool="state",
                             prep=real_start(density, maximum)))
        out.append(load_case(f"load into the real library after a kill {density}", first + second + third + sample, pool="state",
                             ops=[("kill_lib", 7), load, load, ("kill_lib", 104), ("kill_lib", 0), load, load],
                             prep=real_start(density, maximum)))
    if PLB_RECORDS:
        out.append(load_case("load the whole real list into a cleared library", b"".join(PLB_RECORDS), ops=[load] * 106))
        out.append(load_case("load the whole real list back into the real library", b"".join(PLB_RECORDS), pool="state",
                             ops=[("kill_lib", i) for i in range(105)] + [load] * 106, prep=real_start(1024, 100)))
    return out


def load_random(r):
    load = ("load_emitter", "file")
    ops, records = [], []
    for _ in range(r.randint(1, 5)):
        if r.random() < 0.7:
            ops.append(load)
            records.append(random_record(r, r.choice((0, None, None, None))) if r.random() < 0.6 or not PLB_RECORDS
                           else r.choice(PLB_RECORDS))
        else:
            ops.append(("kill_lib", r.choice((0, 1, r.randint(0, 104), r.randint(0, 110), 104, 105, r.getrandbits(32)))))
    data = b"".join(records)
    if r.random() < 0.15:
        data = data[:r.randrange(len(data) + 1)]
    fill = case_fill(r)
    names = {r.randrange(105): r.choice((0, 0, 0x41, 0x80, 0xFF)) for _ in range(r.randint(0, 8))}
    return load_case("random load", data, ops=ops, fill=fill, running=int(r.random() < 0.95), names=names)


def kill_lib_edges():
    out = []
    for running in (0, 1, 2, -1):
        for fill in FILLS:
            for slot in (0, 1, 52, 103, 104, 105, 106, 200, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
                out.append(pcase(f"kill library slot {slot:#x}, running {running}, fill {fill:#x}", [("kill_lib", slot)],
                                 running=running, fill=fill))
    out.append(pcase("kill library slot twice", [("kill_lib", 4), ("kill_lib", 4)], fill=0xA5))
    out.append(pcase("kill every slot", [("kill_lib", i) for i in range(106)], fill=0xFF))
    out.append(pcase("kill a slot of the real library", [("kill_lib", 12)], pool="state", prep=real_start(1024, 100)))
    return out


def kill_lib_random(r):
    return pcase("random kill library slot", [("kill_lib", r.choice((r.randrange(110), r.getrandbits(32), r.randrange(104))))
                                              for _ in range(r.randint(1, 4))],
                 fill=case_fill(r), running=r.choice((0, 1, 1, 1)))


# ---- PAR_GetEmitterStatus and PAR_GetEffecterStatus

def status_edges(op, count):
    out = []
    chains = emitter if op == "emitter_status" else effecter
    for fill in FILLS:
        for running in (0, 1, 2):
            for index in (0, 1, 2, count - 1, count, count + 1, 125, 0x7FFF, 0xFFFF):
                for serial in (0, 1, 0xA5A5, 0xFFFF, 0x1234):
                    pokes = chains(index, serial=serial) if index < count else []
                    for delta, tag in ((0, "match"), (1, "serial+1"), (-1, "serial-1")):
                        shifted = handle(index, serial + delta)
                        out.append(pcase(f"{op} index {index:#x} serial {serial:#x} {tag}, running {running}, fill {fill:#x}",
                                         [(op, shifted)], prep=pokes, fill=fill, running=running))
    for handle_value in (0, 0x10000, 0x20000, 0xFFFF0000, 0xFFFFFFFF, 0x00010001, 0x80000000, 0x7FFFFFFF):
        for fill in FILLS:
            out.append(pcase(f"{op} handle {handle_value:#x}, fill {fill:#x}", [(op, handle_value)], fill=fill))
    # one past the last record is another table's memory: the range check alone refuses it
    if op == "emitter_status":
        # (the pair of equal bytes reads the same in either order: the poke lands in the effecters, which are not swapped there)
        out.append(pcase("emitter_status 120 whose 'serial' matches", [(op, handle(120, 0x5555))],
                         prep=[poke(1, 120 * 0x140 + 0x0A, 2, 0x5555)]))
        out.append(pcase("emitter_status handle 0, emitter 0 serial 0", [(op, 0)], fill=0x00))
        out.append(pcase("emitter_status handle 1, emitter 1 serial 0", [(op, 1)], fill=0x00))
    else:
        out.append(pcase("effecter_status 20 whose 'serial' matches", [(op, handle(20, 0x55))], prep=heads(particles_free=0x55)))
        out.append(pcase("effecter_status handle 0, effecter 0 serial 0", [(op, 0)], fill=0x00))
        out.append(pcase("effecter_status handle 1, effecter 1 serial 0", [(op, 1)], fill=0x00))
    # a dead emitter (not in use) with the right serial still answers 1
    if op == "emitter_status":
        out.append(pcase("emitter_status of an emitter not in use", [(op, handle(9, 9))], prep=emitter(9, serial=9, inuse=0)))
    # after the real start
    out.append(pcase(f"{op} after the real start", [(op, handle(3, 0)), (op, handle(3, 1)), (op, 0)], pool="state",
                     prep=real_start(1024, 100)))
    return out


def status_random(op, count):
    chains = emitter if op == "emitter_status" else effecter

    def make(r):
        index = r.choice((r.randrange(count), r.randrange(count), r.randrange(count + 8), r.randrange(0x10000)))
        serial = r.choice((r.getrandbits(16), 0, 0xFFFF, r.getrandbits(16)))
        value = handle(index, serial if r.random() < 0.6 else r.getrandbits(16))
        if r.random() < 0.1:
            value = r.getrandbits(32)
        return pcase(f"random {op}", [(op, value)], prep=chains(index, serial=serial) if index < count else [],
                     fill=case_fill(r), running=r.choice((0, 1, 1, 1, 2)))
    return make


# ---- the setters and the killer

SETTER_ARITY = {"launch": 3, "pos": 3, "random_pos": 3, "mask": 1, "life": 1, "scale": 1, "kill_emitter": 0}


def setter_edges(op):
    out = []
    arity = SETTER_ARITY[op]
    r = random.Random(sum(map(ord, op)))

    def values(k):
        return [INT_EDGES[(k + j * 7) % len(INT_EDGES)] for j in range(arity)]
    fields = dict(lscale=1024) if op == "launch" else {}
    for index in (0, 1, 7, 118, 119):
        for serial in (0, 1, 0x1234, 0xFFFF):
            for k in range(0, len(INT_EDGES), 5):
                out.append(pcase(f"{op} emitter {index} serial {serial:#x} #{k}", [(op, handle(index, serial), *values(k))],
                                 prep=emitter(index, serial=serial, **fields)))
    # scale edges of the launch vector
    if op == "launch":
        for lscale in LAUNCH_SCALES:
            for k in range(0, len(INT_EDGES), 3):
                out.append(pcase(f"launch scale {lscale} #{k}", [(op, handle(4, 4), *values(k))],
                                 prep=emitter(4, serial=4, lscale=lscale)))
    # a dead handle does nothing
    for fill in FILLS:
        for running in (0, 1):
            out.append(pcase(f"{op}, fill {fill:#x}, running {running}", [(op, handle(2, 2), *values(3))],
                             prep=emitter(2, serial=2), fill=fill, running=running))
    out.append(pcase(f"{op}, wrong serial", [(op, handle(2, 3), *values(3))], prep=emitter(2, serial=2)))
    out.append(pcase(f"{op}, handle 0 on emitter 0 with serial 0", [(op, 0, *values(5))], fill=0x00))
    out.append(pcase(f"{op}, handle 0 on emitter 0 with serial 0, running 0", [(op, 0, *values(5))], fill=0x00, running=0))
    out.append(pcase(f"{op}, index out of range", [(op, handle(120, 0x5555), *values(2))],
                     prep=[poke(1, 120 * 0x140 + 0x0A, 2, 0x5555)]))
    out.append(pcase(f"{op}, index 0xFFFF", [(op, handle(0xFFFF, 1), *values(2))]))
    out.append(pcase(f"{op}, emitter not in use", [(op, handle(6, 6), *values(4))], prep=emitter(6, serial=6, inuse=0)))
    out.append(pcase(f"{op} twice, two emitters", [(op, handle(6, 6), *values(4)), (op, handle(7, 8), *values(9)),
                                                   (op, handle(6, 6), *values(1))],
                     prep=emitter(6, serial=6) + emitter(7, serial=8)))
    return out


def setter_random(op):
    arity = SETTER_ARITY[op]

    def make(r):
        index = r.choice((r.randrange(120), r.randrange(120), 0, 119, r.randrange(0x10000)))
        serial = r.getrandbits(16)
        steps = []
        pokes = emitter(index, serial=serial, lscale=r.choice(LAUNCH_SCALES + [r.getrandbits(32)])) if index < 120 else []
        for _ in range(r.randint(1, 3)):
            value = handle(index, serial) if r.random() < 0.7 else r.getrandbits(32)
            steps.append((op, value, *[int_value(r) for _ in range(arity)]))
        return pcase(f"random {op}", steps, prep=pokes, fill=case_fill(r), running=r.choice((0, 1, 1, 1)))
    return make


# ---- PAR_KillAndHideAll2DObjects

def kill_2d_case(label, emitters, effecters, flags=None, running=1, fill=0xA5, **kw):
    """emitters, effecters: lists of (index, the 2D byte) in the order of their chains."""
    prep = emitter_chain([i for i, _ in emitters]) + effecter_chain([i for i, _ in effecters])
    for index, flag in emitters:
        prep += emitter(index, twod=flag, life=1000 + index, mask=index + 7)
    for index, flag in effecters:
        prep += effecter(index, twod=flag, life=2000 + index)
    return pcase(label, [("kill_2d",)], prep=prep + list(kw.pop("extra", [])), fill=fill, running=running, **kw)


def kill_2d_edges():
    out = []
    for running in (0, 1, 2):
        out.append(kill_2d_case(f"kill 2d, running {running}", [(5, 1), (9, 0), (2, 1)], [(3, 1), (0, 0)], running=running))
    out.append(kill_2d_case("kill 2d, no emitters, no effecters", [], []))
    out.append(kill_2d_case("kill 2d, no emitters", [], [(4, 1)]))
    out.append(kill_2d_case("kill 2d, no effecters", [(4, 1)], []))
    for flag in (0, 1, 2, 0x7F, 0x80, 0xFF):
        out.append(kill_2d_case(f"kill 2d, flag {flag:#x}", [(1, flag), (2, flag)], [(1, flag)]))
    out.append(kill_2d_case("kill 2d, the last emitter and the last effecter", [(119, 1)], [(19, 1)]))
    out.append(kill_2d_case("kill 2d, the first emitter and the first effecter", [(0, 1)], [(0, 1)]))
    out.append(kill_2d_case("kill 2d, an emitter outside the chain is left alone", [(5, 1)], [(3, 1)],
                            extra=emitter(8, twod=1, life=77, mask=5) + effecter(8, twod=1, life=88)))
    out.append(kill_2d_case("kill 2d, many", [(i, i % 2) for i in range(0, 120, 3)], [(i, (i + 1) % 2) for i in range(0, 20, 2)]))
    out.append(kill_2d_case("kill 2d, all", [(i, 1) for i in range(120)], [(i, 1) for i in range(20)]))
    for fill in FILLS:
        out.append(kill_2d_case(f"kill 2d, fill {fill:#x}", [(2, 1), (4, 0)], [(1, 1)], fill=fill))
    out.append(pcase("kill 2d after the real start", [("kill_2d",)], pool="state", prep=real_start(1024, 100)))
    out.append(pcase("kill 2d twice", [("kill_2d",), ("kill_2d",)],
                     prep=emitter_chain([3]) + emitter(3, twod=1, life=5, mask=9) + effecter_chain([2]) + effecter(2, twod=1)))
    return out


def kill_2d_random(r):
    emitters = [(i, r.choice((0, 0, 1, 1, 0x80, 0xFF, 0x7F))) for i in r.sample(range(120), r.randint(0, 8))]
    effecters = [(i, r.choice((0, 1, 1, 0x80))) for i in r.sample(range(20), r.randint(0, 5))]
    return kill_2d_case("random kill 2d", emitters, effecters, running=r.choice((0, 1, 1, 1)), fill=case_fill(r))


# ---- ApplyParticleDensityAdjustment

def density_case(label, density, index=3, fill=0xA5, **fields):
    return pcase(label, [("density", index)], prep=header(density=density) + emitter(index, **fields), fill=fill)


def density_edges():
    out = []
    for density in DENSITIES:
        for k, byte in enumerate(BYTE_EDGES):
            out.append(density_case(f"density {density:#x} bytes {byte:#x}", density, db0=byte, db1=BYTE_EDGES[(k + 3) % 12],
                                    db2=BYTE_EDGES[(k + 6) % 12], db3=BYTE_EDGES[(k + 9) % 12],
                                    dshort=SHORT_EDGES[k], dword=WORD_EDGES[k]))
        for short in SHORT_EDGES:
            out.append(density_case(f"density {density:#x} short {short}", density, dshort=short, db0=0, db1=0, db2=0, db3=0,
                                    dword=0))
        for word in WORD_EDGES:
            out.append(density_case(f"density {density:#x} word {word}", density, dword=word, dshort=0, db0=0, db1=0, db2=0,
                                    db3=0))
    for index in (0, 1, 118, 119):
        out.append(density_case(f"density, emitter {index}", 512, index=index, db0=100, db1=7, db2=0x90, db3=1, dshort=30,
                                dword=2000))
    for fill in FILLS:
        out.append(density_case(f"density, fill {fill:#x}", 700, fill=fill))
    out.append(pcase("density twice", [("density", 3), ("density", 3)], prep=header(density=300) + emitter(
        3, db0=100, db1=50, db2=3, db3=200, dshort=1000, dword=100000)))
    out.append(pcase("density on two emitters", [("density", 3), ("density", 4)], prep=header(density=300) + emitter(
        3, db0=100, db1=50, db2=3, db3=200, dshort=1000, dword=100000) + emitter(
        4, db0=1, db1=2, db2=3, db3=4, dshort=5, dword=6)))
    for index, record in enumerate(PLB_RECORDS):
        for density in (1024, 300, 2048):
            out.append(pcase(f"density {density} on real definition {index}", [("density", index % 120)],
                             prep=header(density=density) + from_record(record, index % 120), fill=0x00))
    return out


def density_random(r):
    density = r.choice((r.choice(DENSITIES), r.randrange(0, 2200), r.getrandbits(32)))
    index = r.randrange(120)
    fields = dict(db0=r.choice((0, r.randrange(256))), db1=r.choice((0, r.randrange(256))), db2=r.choice((0, r.randrange(256))),
                  db3=r.choice((0, r.randrange(256))), dshort=r.choice((0, r.choice(SHORT_EDGES), r.getrandbits(16))),
                  dword=r.choice((0, r.choice(WORD_EDGES), r.getrandbits(32), r.randrange(1, 5000))))
    return density_case("random density", density, index, case_fill(r), **fields)


# ---- PAR_GetEmitterBoundingBox

def bbox_edges():
    out = []
    corners = dict(minx=0x100, miny=-0x100, minz=0x7FFFFFF, maxx=0x7FFFFFFF, maxy=-0x80000000, maxz=0x12345678)
    for running in (0, 1, 2):
        for index in (0, 1, 60, 119, 120, 121, 125):
            for alias in ("apart", "same"):
                out.append(pcase(f"bbox {index}, {alias}, running {running}", [("bbox", index, alias)],
                                 prep=emitter(index % 120, **corners) if index < 120 else [], running=running))
    for value in INT_EDGES:
        out.append(pcase(f"bbox, corner {value}", [("bbox", 5, "apart")],
                         prep=emitter(5, minx=value, miny=value // 2, minz=-value, maxx=value + 1, maxy=value ^ 5, maxz=value)))
    for fill in FILLS:
        out.append(pcase(f"bbox, fill {fill:#x}", [("bbox", 9, "apart")], fill=fill))
    out.append(pcase("bbox, two in a row", [("bbox", 5, "apart"), ("bbox", 6, "same")],
                     prep=emitter(5, minx=1, maxz=2) + emitter(6, miny=3, maxx=4)))
    out.append(pcase("bbox after the real start", [("bbox", 0, "apart"), ("bbox", 119, "apart")], pool="state",
                     prep=real_start(1024, 100)))
    return out


def bbox_random(r):
    index = r.choice((r.randrange(120), r.randrange(126)))
    corners = {name: int_value(r) for name in ("minx", "miny", "minz", "maxx", "maxy", "maxz")}
    return pcase("random bbox", [("bbox", index, r.choice(("apart", "apart", "same")))],
                 prep=emitter(index, **corners) if index < 120 else [], fill=case_fill(r), running=r.choice((0, 1, 1, 1)))


# ---- PAR_Tick

def tick_pool(r, count):
    """A pool of `count` particles with disjoint chains whose indices only grow: (links, chains)."""
    indices = list(range(count))
    r.shuffle(indices)
    chains, at = [], 0
    for _ in range(r.randint(1, 4)):
        size = r.randint(0, min(6, count - at))
        chains.append(sorted(indices[at:at + size]))
        at += size
    return pool_links(count, *chains), chains


def tick_case(label, emitters, chains, count, ticks=1, balance=0, running=1, fill=0xFF, nexts=None, head=None, repeat=1, **kw):
    """emitters: [(emitter index, paused, its first particle, serial)] in the order of the chain in use; chains: the particle
    chains of the pool (lists of indices); nexts: {particle: the link DoAParticle leaves it}; head: what DoEmitters leaves
    the chain head (None: it leaves it alone)."""
    prep = emitter_chain([e[0] for e in emitters]) + header(ticks=ticks) + globals_(balance=balance)
    for index, paused, first, serial in emitters:
        prep += emitter(index, paused=paused, first=first, serial=serial)
    queues = {f"DoAParticle next:{p}": [value] for p, value in (nexts or {}).items()}
    if head is not None:
        queues["DoEmitters head"] = [head]
    return pcase(label, [("tick",)] * repeat, prep=prep, pool=pool_links(count, *chains), maximum=count, running=running,
                 fill=fill, queues=queues, **kw)


def tick_edges():
    out = []
    chains = [[0, 1, 2], [4, 5], [7]]
    emitters = [(3, 0, 0, 3), (9, 1, 4, 9), (60, 0, 4, 60), (119, 0, 7, 119), (0, 0, -1, 0)]
    for running in (0, 1, 2, -1):
        out.append(tick_case(f"tick, running {running}", emitters, chains, 10, running=running))
    for ticks in (-3, -1, 0, 1, 2, 3, 100, 0x7FFFFFFE, 0x7FFFFFFF, -0x80000000):
        out.append(tick_case(f"tick, count {ticks}", emitters, chains, 10, ticks=ticks))
    out.append(tick_case("tick, no emitters", [], chains, 10, ticks=5))
    out.append(tick_case("tick, one emitter, no particles", [(4, 0, -1, 4)], chains, 10, ticks=5))
    out.append(tick_case("tick, one emitter, first particle -5", [(4, 0, -5, 4)], chains, 10, ticks=5))
    out.append(tick_case("tick, one emitter, first particle 0x8000", [(4, 0, 0x8000, 4)], chains, 10, ticks=5))
    out.append(tick_case("tick, paused 0x100", [(4, 0x100, 0, 4)], chains, 10, ticks=5))
    out.append(tick_case("tick, paused -1", [(4, -1, 0, 4)], chains, 10, ticks=5))
    out.append(tick_case("tick, paused 0x8000", [(4, -0x8000, 0, 4)], chains, 10, ticks=5))
    out.append(tick_case("tick, all paused", [(4, 1, 0, 4), (5, 2, 4, 5)], chains, 10, ticks=5))
    out.append(tick_case("tick, the last particle of a chain is the one before the margin", [(4, 0, 0, 4)], [[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]],
                         10, ticks=5))
    for balance in (0, 1, -1, 0x7FFFFFFF):
        out.append(tick_case(f"tick, balance {balance}", emitters, chains, 10, ticks=5, balance=balance))
    out.append(tick_case("tick, balance answered when not running", emitters, chains, 10, ticks=5, balance=9, running=0))
    # DoAParticle changes the link of the particle it is given: the walk goes on from the link read before the call
    out.append(tick_case("tick, DoAParticle cuts the chain", emitters, chains, 10, ticks=5, nexts={0: -1, 5: -1}))
    out.append(tick_case("tick, DoAParticle relinks the chain", emitters, chains, 10, ticks=5, nexts={0: 2, 1: 7, 4: 9}))
    out.append(tick_case("tick, DoAParticle changes every link", emitters, chains, 10, ticks=5,
                         nexts={0: 9, 1: 8, 2: 7, 4: 6, 5: 5 + 1, 7: 8}))
    # DoEmitters changes the chain head
    for head in (-1, 3, 9, 60, 119, 0):
        out.append(tick_case(f"tick, DoEmitters leaves the head {head}", emitters, chains, 10, ticks=5, head=head))
    out.append(tick_case("tick, DoEmitters leaves the head, first tick only counts", emitters, chains, 10, ticks=0, head=-1))
    out.append(tick_case("tick, three ticks", emitters, chains, 10, ticks=0, repeat=3, nexts={1: 7}))
    out.append(tick_case("tick, a lone emitter among filler", [(60, 0, 7, 60)], chains, 10, ticks=9, fill=0xA5))
    for fill in (0x00, 0xA5, 0xFF):
        out.append(tick_case(f"tick, fill {fill:#x}", [(2, 0, 4, 2)], chains, 10, ticks=9, fill=fill))
    # the game's own system: the pool's free chain holds every particle, an emitter that starts it walks them all
    out.append(pcase("tick after the real start: counts only", [("tick",)] * 3, pool="state", prep=real_start(1024, 100)))
    out.append(pcase("tick after the real start: an emitter walks the free chain", [("tick",)] * 2, pool="state",
                     prep=real_start(1024, 100) + header(ticks=1) + emitter_chain([5]) + emitter(5, paused=0, first=0, serial=5)))
    out.append(pcase("tick after the real start: the free chain, DoAParticle frees as it goes", [("tick",)], pool="state",
                     prep=real_start(1024, 30) + header(ticks=1) + emitter_chain([5]) + emitter(5, paused=0, first=0, serial=5),
                     queues={"DoAParticle next:3": [-1], "DoAParticle next:10": [20]}))
    out.append(pcase("tick after the real start, closed", [("tick",), ("close",), ("tick",)], pool="state",
                     prep=real_start(1024, 100) + header(ticks=3)))
    return out


def tick_random(r):
    count = r.choice((6, 12, 24))
    links, chains = tick_pool(r, count)
    k = r.randint(0, 5)
    indices = r.sample(range(120), k)
    emitters = []
    for index in indices:
        chain = r.choice(chains)
        first = (chain[0] if chain else -1) if r.random() < 0.8 else r.choice((-1, -2, 0x8000, -0x8000))
        emitters.append((index, r.choice((0, 0, 0, 1, -1, 0x100)), first, r.getrandbits(16)))
    nexts = {}
    for chain in chains:
        for position, particle in enumerate(chain):
            if r.random() < 0.3:
                later = [q for q in range(particle + 1, count)]
                nexts[particle] = r.choice([-1] + later)
    head = r.choice((None, None, -1)) if not indices else r.choice((None, None, -1, r.choice(indices)))
    return tick_case("random tick", emitters, chains, count, ticks=r.choice((-1, 0, 1, 1, 2, 3, 99, 0x7FFFFFFF)),
                     balance=r.choice((0, 0, 7, -1)), running=r.choice((0, 1, 1, 1, 1)), fill=r.choice((0x00, 0xA5, 0xFF)),
                     nexts=nexts, head=head, repeat=r.choice((1, 1, 2, 3)))


# ---- the test list

def status_test(name, op, count):
    return FnTest(name, particle_ppc, particle_host, edge=lambda: status_edges(op, count), rnd=status_random(op, count))


def setter_test(name, op):
    return FnTest(name, particle_ppc, particle_host, edge=lambda: setter_edges(op), rnd=setter_random(op))


TESTS = [
    FnTest("PAR_CloseParticleSystem", particle_ppc, particle_host, edge=close_edges, rnd=close_random),
    FnTest("PAR_Free", particle_ppc, particle_host, edge=free_edges, rnd=free_random),
    FnTest("PAR_LoadLibEmitter", particle_ppc, particle_host, edge=load_edges, rnd=load_random),
    FnTest("PAR_KillLibEmitter", particle_ppc, particle_host, edge=kill_lib_edges, rnd=kill_lib_random),
    FnTest("PAR_Tick", particle_ppc, particle_host, edge=tick_edges, rnd=tick_random),
    FnTest("ApplyParticleDensityAdjustment", particle_ppc, particle_host, edge=density_edges, rnd=density_random),
    status_test("PAR_GetEmitterStatus", "emitter_status", 120),
    status_test("PAR_GetEffecterStatus", "effecter_status", 20),
    setter_test("PAR_KillEmitter", "kill_emitter"),
    FnTest("PAR_KillAndHideAll2DObjects", particle_ppc, particle_host, edge=kill_2d_edges, rnd=kill_2d_random),
    setter_test("PAR_SetEmitterLaunchVector", "launch"),
    setter_test("PAR_SetEmitterPos", "pos"),
    setter_test("PAR_SetEmitterMask", "mask"),
    setter_test("PAR_SetEmitterLife", "life"),
    setter_test("PAR_SetEmitterScale", "scale"),
    setter_test("PAR_SetEmitterRandomPos", "random_pos"),
    FnTest("PAR_GetEmitterBoundingBox", particle_ppc, particle_host, edge=bbox_edges, rnd=bbox_random),
    FnTest("PAR_FreeDrawingList", particle_ppc, particle_host, edge=free_list_edges, rnd=free_list_random),
]
