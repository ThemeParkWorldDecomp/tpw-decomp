"""Unit w146, the save and litter half of MapDelta.cpp, against libtpw_unit_MapSave.dylib (tests/unit_MapSave.cpp), main app ("SimTheme Park"),
src/MapDelta_save.cpp (and src/MapDelta.cpp for the static construction):
  CMapCell::PlaceLitter 0x10103010, CMap::Serialise 0x10100e80, CMapCell::Serialise 0x10102a10, LOADVAR<CMapCell::LitterType> 0x10103af0,
  SAVEVAR<CMapCell::LitterType> 0x10103c30, CMacroAI::Serialise 0x10100890, __sinit_MapDelta_cpp 0x10103ec0 (the instances over the basic
  loaders are covered here through CMapCell::Serialise and in test_Serialise.py directly)

A case is a world and a call. The world is built twice by ONE piece of Python code: the map block (0x1db000 bytes, random bytes from the case's
seed, the cells' fields set by the case) in emulator memory (big-endian) for the original and in the host library's memory (little-endian) for
the translation. The serialise instances are REAL on both sides (the original's LOADVAR / SAVEVAR of unsigned char, short, unsigned int, long, the
thing id struct and the litter type run in the emulator; the host runs the int instances for the long ones), so the file is the fake: LbFile_Read
hands out the case's bytes in order (the queues replace the result and the transferred count), LbFile_Write records the bytes as hex,
LbFile_GetPosition answers from its queue. The three cell constructors run real on both sides. Every other callee is a fake that records its
arguments (objects as offsets from the block, -1 for a local) and answers from the case's queue: CCellBase, CTrackCell, CRegionEffectCell,
CStaffDB, CParkTime and CArrival Serialise, the three modified tests, the two cell resets, CTRACE, GAME_ASSERTMSG, ESP_SpawnScript (the data
block's nine words and the position's float bits), SCR_SetScriptPosition, PAR_InitEmitter, CWorld::SeedRand and GetRand, CMap::AddRegionalEffector.

What is compared: the answer, the log, the map cells' fields (decoded per side), the cells' shared bytes, the time stamps (decoded), all other bytes
of the block. HardwareRounding covers PlaceLitter (fctiwz of the emitter position, the fused multiply-add of the position).
Not tested: the destructor the Mac registers for each delta (the host has none); __register_global_object's node (only the object and the
destructor are compared).
"""
import ctypes as C
import hashlib
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_MapSave"
MASK = 0xFFFFFFFF

PLACE, CMAP, CELL, LOADLIT, SAVELIT, MACRO, SINIT = 0x10103010, 0x10100E80, 0x10102A10, 0x10103AF0, 0x10103C30, 0x10100890, 0x10103EC0
SIZES = {PLACE: 844, CMAP: 1096, CELL: 756, LOADLIT: 236, SAVELIT: 260, MACRO: 172, SINIT: 436,
         0x100F17C8: 228, 0x100F1A04: 236, 0x100F518C: 236, 0x100F5658: 252, 0x1000EB4C: 224, 0x1000E9D4: 240}
OWN = set(SIZES)
# run as the original as well (not translated here): the long and unsigned int instances and the constructors
REAL_EXTRA = {0x1000EE04, 0x1000F384, 0x100F44A4, 0x100F45BC, 0x10102948, 0x1012FB08, 0x10120C10}

ASSERT, WRITELOG, CTRACE = 0x1000DDE8, 0x1015B3DC, 0x1006FCBC
CCB, TRACK_SER, REGION_SER, STAFF_SER, PTIME_SER, ARRIVAL_SER = 0x100F49FC, 0x1012FBC0, 0x10120CFC, 0x10128FF4, 0x10111FE0, 0x100F2040
MAP_HAS, TRACK_HAS, REGION_EQ, MAP_RESET, TRACK_RESET = 0x101029C8, 0x1012FB74, 0x10120C7C, 0x1009CB48, 0x100A3A74
ADD_EFF, SEED, GETRAND, ESP, SETPOS, PAR, REGGLOBAL = 0x101016CC, 0x1013B244, 0x1013B1E4, 0x100DA18C, 0x100D932C, 0x100BEC30, 0x10239940

G_WORLD, G_WORLDMAP = 0x1037EC0C, 0x1037EC08
G_GAMETYPE, G_GAMETYPE_GUARD = 0x102B3AA0, 0x102B3AA4
GSAVELOAD = 0x1037FEFC
FILE_HANDLE = 0x6E100000
LABEL_SAVELOAD, LABEL_FILE = 6, 8
DELTAS = [0x1034C2AC, 0x1034C298, 0x1034C284, 0x1034C270, 0x1034C25C, 0x1034C248, 0x1034C234, 0x1034C220]
DELTA_VALUES = [(0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1)]
DESTRUCTOR = 0x102663D0

HOST_LAYOUT = [0x24, 0x26, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x40, 0x44, 0x28, 10]
BLK = 0x1DB000
TRACKS, STAMPS, EFFECTS = 0x110000, 0x1B0000, 0x1B1104
STAMP_BYTES = 0x1104
MAPCELLS = 0x4000
CELL_FMT = "HB33sHBBiiHHIiIhH"      # id, kind, [shared bytes 3..0x23], who, status, byte 0x27, script, lit, coll, f32, time, emit, f3c, pylon, f42
# the cell fields a case sets: (offset, width, key); the random prefill covers the rest
CELLF = [(0, 2, "id"), (0x24, 2, "who"), (0x26, 1, "st"), (0x28, 4, "script"), (0x2C, 4, "lit"), (0x30, 2, "coll"), (0x32, 2, "f32"),
         (0x34, 4, "time"), (0x38, 4, "emit"), (0x40, 2, "pylon"), (0x42, 2, "f42")]


def s32(v):
    return T.s32(v)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


class ROUNDING(HardwareRounding):
    """HardwareRounding aimed at PlaceLitter (fused multiply-add and fctiwz)."""

    def __enter__(self):
        self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=PLACE, end=PLACE + SIZES[PLACE] - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.update(REAL_EXTRA)
R = CL.recorder


def off(e, p):
    d = p - e.mw_blk
    return d if -0x400 <= d < BLK else -1


def count_args(fmt):
    n, i = 0, 0
    while i < len(fmt):
        if fmt[i] == "%":
            if i + 1 < len(fmt) and fmt[i + 1] == "%":
                i += 2
                continue
            n += 1
        i += 1
    return n


def stub_ctrace(e):
    fmt = e.cstr(e.r(4))
    n = min(count_args(fmt), 3)
    e.cl.rec("CTRACE", [e.r(3) & MASK] + [e.r(5 + i) & MASK for i in range(n)], [fmt[:159]])


def lb_read(e):
    # r3 the result record, r4 the file, r5 the buffer, r6 the length, r7 the count
    L, buf, length = e.cl, e.r(5), e.r(6)
    L.rec("LbFile_Read", (L.lab(e.r(4)), length))
    data = e.mw_file[e.mw_cur[0]:]
    taken = min(len(data), length)
    count = taken
    forced = L.ret("LbFile_Read:xfer", -1)
    if forced >= 0:
        count = forced
    e.write(e.r(7), (count & MASK).to_bytes(4, "big"))
    result = L.ret("LbFile_Read")
    if count == length and result >= 0:
        e.write(buf, (data[:taken] + bytes(length))[:length])
    e.mw_cur[0] += taken
    e.write(e.r(3), (result & MASK).to_bytes(4, "big"))


def lb_write(e):
    L, length = e.cl, e.r(6)
    L.rec("LbFile_Write", (L.lab(e.r(4)), length), (e.read(e.r(5), length).hex()[:150] if length else "",))
    forced = L.ret("LbFile_Write:xfer", -1)
    e.write(e.r(7), ((forced if forced >= 0 else length) & MASK).to_bytes(4, "big"))
    e.write(e.r(3), (L.ret("LbFile_Write") & MASK).to_bytes(4, "big"))


def lb_pos(e):
    e.cl.rec("LbFile_GetPosition", (e.cl.lab(e.r(3)),))
    e.set_r(3, T.u32(e.cl.ret("LbFile_GetPosition")))


def ser_stub(name):
    def stub(e):
        L = e.cl
        L.rec(name, (off(e, e.r(3)), L.lab(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & MASK))
        e.set_r(3, T.u32(L.ret(name)))
    return stub


def u16(e, p):
    return int.from_bytes(e.read(p, 2), "big")


def stub_map_has(e):
    other = e.r(4)
    e.cl.rec("CMapCell::HasBeenModifiedFrom", (off(e, e.r(3)), off(e, other), u16(e, other), e.read(other + 0x26, 1)[0]))
    e.set_r(3, T.u32(e.cl.ret("CMapCell::HasBeenModifiedFrom")))


def stub_track_has(e):
    other = e.r(4)
    e.cl.rec("CTrackCell::HasBeenModifiedFrom", (off(e, e.r(3)), off(e, other), u16(e, other)))
    e.set_r(3, T.u32(e.cl.ret("CTrackCell::HasBeenModifiedFrom")))


def stub_region_eq(e):
    e.cl.rec("CRegionEffectCell::operator==", (off(e, e.r(3)), off(e, e.r(4))))
    e.set_r(3, 1 if e.cl.ret("CRegionEffectCell::operator==") != 0 else 0)


def stub_map_reset(e):
    p = e.r(3)
    e.cl.rec("CMapCell::Reset", (off(e, p), e.r(4) & 0xFF, u16(e, p), e.read(p + 0x26, 1)[0]))


def stub_track_reset(e):
    p = e.r(3)
    e.cl.rec("CTrackCell::Reset", (off(e, p), u16(e, p)))


def stub_esp(e):
    L = e.cl
    words = struct.unpack(">9I", e.read(e.r(3), 36))
    L.rec("ESP_SpawnScript", list(words) + [s32(e.r(4)), s32(e.r(5)), s32(e.r(6))])
    L.rec("ESP_SpawnScript.pos", (fb(e.f(1)), fb(e.f(2)), fb(e.f(3))))
    e.set_r(3, T.u32(L.ret("ESP_SpawnScript")))


def stub_setpos(e):
    e.cl.rec("SCR_SetScriptPosition", (s32(e.r(3)), fb(e.f(1)), fb(e.f(2)), fb(e.f(3)), s32(e.r(7))))


def stub_par(e):
    e.cl.rec("PAR_InitEmitter", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, T.u32(e.cl.ret("PAR_InitEmitter")))


def stub_add_eff(e):
    e.cl.rec("CMap::AddRegionalEffector", (off(e, e.r(3)), s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF))


def stub_seed(e):
    e.cl.rec("CWorld::SeedRand", (off(e, e.r(3)), e.r(4) & MASK))


def stub_getrand(e):
    e.cl.rec("CWorld::GetRand", (off(e, e.r(3)),))
    e.set_r(3, T.u32(e.cl.ret("CWorld::GetRand")))


def stub_register(e):
    e.mw_registered.append((e.r(3), e.r(4)))
    e.write(e.r(1) + 0x14, (e.r(2) & MASK).to_bytes(4, "big"))     # the glue saves the TOC pointer where the caller reloads it


STUBS.internal(ASSERT, lambda e: e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4))[:159],)))
STUBS.internal(WRITELOG, R("CSaveLoad::WriteSerialiseLog", "Lisi"))
STUBS.internal(CTRACE, stub_ctrace)
STUBS.internal(CCB, ser_stub("CCellBase::Serialise"))
STUBS.internal(TRACK_SER, ser_stub("CTrackCell::Serialise"))
STUBS.internal(REGION_SER, ser_stub("CRegionEffectCell::Serialise"))
STUBS.internal(STAFF_SER, ser_stub("CStaffDB::Serialise"))
STUBS.internal(PTIME_SER, ser_stub("CParkTime::Serialise"))
STUBS.internal(ARRIVAL_SER, ser_stub("CArrival::Serialise"))
STUBS.internal(MAP_HAS, stub_map_has)
STUBS.internal(TRACK_HAS, stub_track_has)
STUBS.internal(REGION_EQ, stub_region_eq)
STUBS.internal(MAP_RESET, stub_map_reset)
STUBS.internal(TRACK_RESET, stub_track_reset)
STUBS.internal(ADD_EFF, stub_add_eff)
STUBS.internal(SEED, stub_seed)
STUBS.internal(GETRAND, stub_getrand)
STUBS.internal(ESP, stub_esp)
STUBS.internal(SETPOS, stub_setpos)
STUBS.internal(PAR, stub_par)
STUBS.internal(REGGLOBAL, stub_register)
STUBS.imp("LbFile_Read__FPvPvUlPUl", lb_read)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", lb_write)
STUBS.imp("LbFile_GetPosition__FPv", lb_pos)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, width, v):
        self.write(a, (v & ((1 << (8 * width)) - 1)).to_bytes(width, self.byte))

    def get(self, a, width):
        return int.from_bytes(self.read(a, width), self.byte)


class Ppc(Side):
    order = ">"
    byte = "big"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({GSAVELOAD: LABEL_SAVELOAD, FILE_HANDLE: LABEL_FILE}, case["queues"])
        e.mw_file, e.mw_cur, e.mw_registered, e.mw_blk = case["file"], [0], [], 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.cl = None

    def block(self):
        self.blk = self.e.mw_blk = self.e.alloc(BLK, bytes(BLK))
        self.put(G_WORLD, 4, self.blk - 0x2D8)
        self.put(G_WORLDMAP, 4, self.blk)

    def gametype(self, t):
        self.put(G_GAMETYPE_GUARD, 1, 1)
        self.put(G_GAMETYPE, 4, t)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, addr, words, rounding=False):
        if rounding:
            with ROUNDING(self.e):
                return T.call_ppc(self.e, addr, words, max_insns=200_000_000, timeout_us=900_000_000)[0] & MASK
        return T.call_ppc(self.e, addr, words, max_insns=400_000_000, timeout_us=1_800_000_000)[0] & MASK

    def cell(self, idx, save, version, unused):
        return s32(self.call(CELL, [self.blk + idx * 0x44, FILE_HANDLE, save & MASK, version & MASK, unused & MASK]))

    def map(self, save, version, unused):
        return s32(self.call(CMAP, [self.blk, FILE_HANDLE, save & MASK, version & MASK, unused & MASK]))

    def macro(self, save, version, unused):
        return s32(self.call(MACRO, [(self.blk - 0x2D8) & MASK, FILE_HANDLE, save & MASK, version & MASK, unused & MASK]))

    def litter(self, idx, typ):
        self.call(PLACE, [self.blk + idx * 0x44, typ & MASK], rounding=True)

    def sinit(self):
        self.write(DELTAS[-1], bytes(0x1034C2B8 - DELTAS[-1]))
        for a in DELTAS:
            self.write(a, bytes(12))
        self.call(SINIT, [])
        out = []
        for a in DELTAS:
            raw = self.read(a, 12)
            out += [struct.unpack(">i", raw[0:4])[0], struct.unpack(">i", raw[4:8])[0], raw[8]]
        reg = [(a, d) for a, d in self.e.mw_registered]
        return out, reg


class Host(Side):
    order = "<"
    byte = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_mw_set_file(case["file"], len(case["file"]))
        CL.host_begin(lib, {LABEL_SAVELOAD: lib.tpw_mw_addr(b"gSaveLoad"), LABEL_FILE: lib.tpw_mw_addr(b"file")}, case["queues"])

    def done(self):
        pass

    def block(self):
        self.blk = self.lib.tpw_mw_block()

    def gametype(self, t):
        self.lib.tpw_mw_set_gametype(t)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def cell(self, idx, save, version, unused):
        return self.lib.tpw_mw_cell_serialise(self.blk + idx * 0x44, s32(save), s32(version), unused & MASK)

    def map(self, save, version, unused):
        return self.lib.tpw_mw_map_serialise(self.blk, s32(save), s32(version), unused & MASK)

    def macro(self, save, version, unused):
        return self.lib.tpw_mw_macro_serialise(self.blk - 0x2D8, s32(save), s32(version), unused & MASK)

    def litter(self, idx, typ):
        self.lib.tpw_mw_place_litter(self.blk + idx * 0x44, typ & MASK)

    def sinit(self):
        out = (C.c_int32 * 24)()
        self.lib.tpw_mw_deltas(out)
        return list(out), [(a, DESTRUCTOR) for a in DELTAS]


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_mw_addr.restype, lib.tpw_mw_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_mw_block.restype, lib.tpw_mw_block.argtypes = C.c_uint64, []
    lib.tpw_mw_set_file.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_mw_set_gametype.argtypes = [C.c_int32]
    lib.tpw_mw_deltas.argtypes = [C.POINTER(C.c_int32)]
    lib.tpw_mw_layout.argtypes = [C.POINTER(C.c_int64)]
    for name in ("cell", "map", "macro"):
        f = getattr(lib, "tpw_mw_%s_serialise" % name)
        f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_mw_place_litter.argtypes = [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_mw_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    if c["op"] == "sinit":
        return
    s.block()
    raw = bytearray(random.Random(c["seed"]).randbytes(BLK))
    # the map cells and the time stamps hold numbers (the same on both sides); everything else is raw bytes
    le, mine = struct.Struct("<" + CELL_FMT), struct.Struct(s.order + CELL_FMT)
    for i, t in enumerate(le.iter_unpack(bytes(raw[:TRACKS]))):
        mine.pack_into(raw, i * 0x44, *t)
    words = struct.unpack("<%dI" % (STAMP_BYTES // 4), bytes(raw[STAMPS:STAMPS + STAMP_BYTES]))
    raw[STAMPS:STAMPS + STAMP_BYTES] = struct.pack(s.order + "%dI" % (STAMP_BYTES // 4), *words)
    s.write(s.blk, bytes(raw))
    s.gametype(c["gt"])
    for idx, f in c["cells"].items():
        for off_, width, key in CELLF:
            if key in f:
                s.put(s.blk + idx * 0x44 + off_, width, f[key])


def execute(s, c):
    op, a = c["op"], c["args"]
    if op == "cell":
        return s.cell(*a)
    if op == "map":
        return s.map(*a)
    if op == "macro":
        return s.macro(*a)
    if op == "litter":
        s.litter(*a)
        return 0
    return s.sinit()


def observe(s, c, answer):
    out = {"answer": answer, "log": s.log()}
    if c["op"] == "sinit":
        return out
    data = s.read(s.blk, BLK)
    st = struct.Struct(s.order + CELL_FMT)
    cells = hashlib.md5()
    for t in st.iter_unpack(data[:TRACKS]):
        cells.update(struct.pack("<HB33sHBBiiHHIiIhH", *t))
    out["cells"] = cells.hexdigest()
    out["stamps"] = hashlib.md5(struct.pack("<%dI" % (STAMP_BYTES // 4), *struct.unpack(s.order + "%dI" % (STAMP_BYTES // 4),
                                                                                       data[STAMPS:STAMPS + STAMP_BYTES]))).hexdigest()
    out["rest"] = hashlib.md5(data[TRACKS:STAMPS] + data[STAMPS + STAMP_BYTES:]).hexdigest()
    return out


def run(s, c):
    try:
        build(s, c)
        r = execute(s, c)
        if c["op"] == "sinit":
            deltas, reg = r
            out = observe(s, c, 0)
            out["deltas"], out["registered"] = deltas, reg
            return out
        return observe(s, c, r)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

VERSIONS = [-1, 0, 1, 2, 3, -0x80000000, 0x7FFFFFFF]


def base(r, op, args, **kw):
    c = {"op": op, "args": args, "seed": r.getrandbits(32), "file": b"", "queues": {}, "cells": {}, "gt": 0}
    c.update(kw)
    return c


def cell_file(r, n=1):
    return bytes(r.getrandbits(8) for _ in range(23 * n))


def fail_queues(r, save, step, kind):
    """A failure at step `step` (0 to 7 for the cell): the result, a short count, or a count past the end."""
    key = "LbFile_Write" if save else "LbFile_Read"
    k = r.choice(["res", "short", "long", "pos"]) if kind is None else kind
    if k == "res":
        return {key: [0] * step + [r.choice([-1, 1, -0x80000000, 0x7FFFFFFF])]}
    if k == "short":
        return {key + ":xfer": [-1] * step + [r.choice([0, 1, 3])]}
    if k == "long":
        return {key + ":xfer": [-1] * step + [r.choice([5, 9, 0xFFFFFFFF])]}
    return {}


def cell_case(r, save=None, version=None, step=None, kind=None):
    save = r.choice([0, 1]) if save is None else save
    version = r.choice([0, 1, 1, 0, -1, 2, 3, 100, -5]) if version is None else version
    q = {}
    if step is not None:
        q.update(fail_queues(r, save, step, kind))
    if r.random() < 0.3:
        q["CCellBase::Serialise"] = [r.choice([0, 1, -1, 5])]
    idx = r.choice([0, 1, 127, 128, 0x3FFF, r.randrange(MAPCELLS)])
    c = base(r, "cell", [idx, save, version, r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)])], queues=q)
    c["file"] = cell_file(r) if r.random() < 0.9 else cell_file(r)[:r.randrange(0, 23)]
    cells = {}
    if r.random() < 0.7:
        cells[idx] = {k: r.getrandbits(8 * w) for _, w, k in CELLF if r.random() < 0.7}
    c["cells"] = cells
    return c


def cell_edges():
    r = random.Random(1461)
    for save in (0, 1):
        for version in VERSIONS:
            yield cell_case(r, save, version)
        for version in (0, 1):
            for step in range(8):
                for kind in ("res", "short", "long"):
                    yield cell_case(r, save, version, step, kind)
        for answer in (0, 1, -1, 7):
            c = cell_case(r, save, 0)
            c["queues"]["CCellBase::Serialise"] = [answer]
            yield c


def cell_random(r):
    return cell_case(r, step=r.choice([None, None, 0, 1, 2, 3, 4, 5, 6, 7]))


def macro_case(r, save=None, version=None, fail=None):
    q = {}
    names = ["CStaffDB::Serialise", "CParkTime::Serialise", "CArrival::Serialise"]
    for i, n in enumerate(names):
        q[n] = [r.choice([0, 1, -1, 5, 0x7FFFFFFF]) if fail is not None and i == fail else (0 if fail is not None else r.choice([0, 0, 0, 1, 2]))]
    return base(r, "macro", [r.choice([0, 1]) if save is None else save, r.choice([0, 1, 2, -1]) if version is None else version,
                             r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)])], queues=q)


def macro_edges():
    r = random.Random(1462)
    for save in (0, 1):
        for fail in (None, 0, 1, 2):
            for v in (0, 1):
                yield macro_case(r, save, v, fail)
        for answers in ([0, 0, 0], [1, 1, 1], [0, 1, 0], [0, 0, 3]):
            c = macro_case(r, save, 1)
            for n, a in zip(["CStaffDB::Serialise", "CParkTime::Serialise", "CArrival::Serialise"], answers):
                c["queues"][n] = [a]
            yield c


def macro_random(r):
    return macro_case(r)


def litter_case(r, typ=None, gt=None, lit=None, cid=None, rands=None):
    typ = r.choice(list(range(10)) + [0xFFFFFFFF, 0x80000000, 0x100, r.getrandbits(32)]) if typ is None else typ
    gt = r.choice([0, 0, 0, 1, 2, 3, -1]) if gt is None else gt
    lit = r.choice([0, 0, 0, 0, 1, 5, 0x80000000, -1]) if lit is None else lit
    cid = r.choice([1, 2, 127, 128, 129, 0x4000, 0xFFFF, 0, 0x1234, 0x2001, r.randrange(1, 0x4001)]) if cid is None else cid
    rands = [r.choice([0, 8, 9, 17, 0xFFFFFFFF, 0x80000000, r.getrandbits(32), r.randrange(100)]) for _ in range(2)] if rands is None else rands
    idx = r.choice([0, 1, 5, 0x3FFF, r.randrange(MAPCELLS)])
    q = {"CWorld::GetRand": rands, "ESP_SpawnScript": [r.choice([0, 1, -1, r.getrandbits(31), -r.getrandbits(31)])],
         "PAR_InitEmitter": [r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)])]}
    cell = {"id": cid, "lit": lit, "script": r.getrandbits(32), "coll": r.getrandbits(16), "emit": r.getrandbits(32)}
    return base(r, "litter", [idx, typ], queues=q, gt=gt, cells={idx: cell})


def litter_edges():
    r = random.Random(1463)
    for typ in list(range(10)) + [0xFFFFFFFF, 0x80000000, 0x100]:
        for gt in (0, 1):
            yield litter_case(r, typ, gt, 0)
        yield litter_case(r, typ, 0, 3)
        for rands in ([0, 0], [8, 8], [9, 17], [0xFFFFFFFF, 0x80000000], [7, 1]):
            yield litter_case(r, typ, 2, 0, rands=rands)
    for cid in (0, 1, 2, 127, 128, 129, 255, 256, 0x4000, 0x4001, 0xFFFF, 0x1234):
        yield litter_case(r, 8, 0, 0, cid)
        yield litter_case(r, 7, 0, 0, cid)
        yield litter_case(r, 3, 0, 0, cid)
    for gt in (-1, 0, 1, 2, 3, 0x100, 0x80000000):
        yield litter_case(r, 8, gt, 0)


def litter_random(r):
    return litter_case(r)


# ---- the map

def status_data(r, statuses, version=0):
    """The file of a map load: for each cell its status byte, then the map cell's 23 bytes if bit 1 is set."""
    out = bytearray()
    for st in statuses:
        out.append(st)
        if st & 1:
            out += cell_file(r)
    return bytes(out)


def map_load_case(r, statuses=None, version=None, fail_at=None, extra_q=None, truncate=None):
    version = r.choice([0, 1]) if version is None else version
    if statuses is None:
        statuses = []
    full = list(statuses) + [0] * (MAPCELLS - len(statuses))
    data = status_data(r, full)
    if truncate is not None:
        data = data[:truncate]
    q = dict(extra_q or {})
    if fail_at is not None:
        q["LbFile_Read"] = [0] * fail_at + [r.choice([-1, -2])]
    return base(r, "map", [0, version, r.choice([0, 1, r.getrandbits(32)])], queues=q, file=data)


def sparse(r, n, p):
    return [1 if r.random() < p else 0 for _ in range(n)]


def map_save_case(r, version=None, mapq=None, trackq=None, eqq=None, extra_q=None, fail_write_at=None, positions=None):
    version = r.choice([0, 1]) if version is None else version
    q = {
        "CMapCell::HasBeenModifiedFrom": mapq if mapq is not None else [0],
        "CTrackCell::HasBeenModifiedFrom": trackq if trackq is not None else [0],
        "CRegionEffectCell::operator==": eqq if eqq is not None else [1],
        "LbFile_GetPosition": positions if positions is not None else [r.randrange(0, 1000), r.randrange(1000, 70000)],
    }
    q.update(extra_q or {})
    if fail_write_at is not None:
        q["LbFile_Write"] = [0] * fail_write_at + [r.choice([-1, -2])]
    return base(r, "map", [1, version, r.choice([0, 1, r.getrandbits(32)])], queues=q)


COMBO_MAP = [0, 1, 0, 1, 0, 1, 0, 1]
COMBO_TRACK = [0, 0, 1, 1, 0, 0, 1, 1]
COMBO_EQ = [1, 1, 1, 1, 0, 0, 0, 0]       # equal: bit 4 clear


def map_edges():
    r = random.Random(1464)
    # loads: nothing but status bytes, the full 0x4000 cells
    for v in (0, 1):
        yield map_load_case(r, None, v)
    yield map_load_case(r, [1, 2, 4, 7, 3, 5, 6, 0, 7, 1], 0)
    yield map_load_case(r, [7, 7, 0, 4, 2, 1], 1)
    yield map_load_case(r, [2, 2, 2], 0, extra_q={"CTrackCell::Serialise": [0, 1]})
    yield map_load_case(r, [4, 4, 4], 1, extra_q={"CRegionEffectCell::Serialise": [0, 0, 5]})
    yield map_load_case(r, [1, 1, 1], 1, extra_q={"CCellBase::Serialise": [1]})
    for v in (-1, 2, 3, 0x7FFFFFFF, -0x80000000):
        yield map_load_case(r, [1, 2, 4, 7], v)
    for k in (0, 1, 2, 5, 40):
        yield map_load_case(r, [1, 2, 4, 7, 3, 5, 6, 0] * 6, 0, fail_at=k)
    yield map_load_case(r, [1, 2, 4, 7], 0, fail_at=16383)
    yield map_load_case(r, [1, 2, 4, 7], 1, extra_q={"LbFile_Read:xfer": [-1, -1, -1, 0]})
    yield map_load_case(r, [1, 2, 4, 7], 1, truncate=10)
    yield map_load_case(r, [1], 1, truncate=7)
    yield map_load_case(r, [1], 1, truncate=0)
    c = map_load_case(r, [1, 1], 0)
    c["file"] = c["file"][:30] + b"\x00" * 3
    yield c
    # saves: all cells default, then the eight combinations, then dense heads
    for v in (0, 1):
        yield map_save_case(r, v)
    yield map_save_case(r, 0, COMBO_MAP + [0], COMBO_TRACK + [0], COMBO_EQ + [1])
    yield map_save_case(r, 1, COMBO_MAP + sparse(r, 600, 0.01), COMBO_TRACK + sparse(r, 600, 0.01), COMBO_EQ + [1] * 600)
    yield map_save_case(r, 0, [1] * 30 + [0], [1] * 30 + [0], [0] * 30 + [1])
    yield map_save_case(r, 1, [1] * 7 + [0], [0] * 3 + [1], [1] * 20 + [0])
    yield map_save_case(r, 0, [0], [0], [1] * 100 + [0] * 28 + [1])
    yield map_save_case(r, 0, [1], [1], [0], fail_write_at=40)   # every cell has everything (stopped by a failing write)
    for v in (-1, 2, 3, 0x7FFFFFFF, -0x80000000):
        yield map_save_case(r, v)
    for positions in ([0, 0], [5], [100, 99], [-1, 1], [0x7FFFFFFF, -0x80000000], [0, 0x4000 * 3]):
        yield map_save_case(r, 1, positions=positions)
    for k in (0, 1, 2, 3, 5, 9, 14, 100):
        yield map_save_case(r, 0, COMBO_MAP + [0], COMBO_TRACK + [0], COMBO_EQ + [1], fail_write_at=k)
    yield map_save_case(r, 1, [1], [1], [0], extra_q={"LbFile_Write:xfer": [-1, -1, -1, 0]})
    yield map_save_case(r, 1, COMBO_MAP + [0], COMBO_TRACK + [0], COMBO_EQ + [1], extra_q={"CTrackCell::Serialise": [0, 1]})
    yield map_save_case(r, 0, COMBO_MAP + [0], COMBO_TRACK + [0], COMBO_EQ + [1], extra_q={"CRegionEffectCell::Serialise": [0, 0, 1]})
    yield map_save_case(r, 0, COMBO_MAP + [0], COMBO_TRACK + [0], COMBO_EQ + [1], extra_q={"CCellBase::Serialise": [1]})
    yield map_save_case(r, 1, [1, 0, 0, 0], [0], [1], extra_q={"LbFile_Write": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1]})


def map_random(r):
    mode = r.random()
    if mode < 0.45:
        n = r.randrange(0, 30)
        statuses = [r.choice([0, 1, 2, 3, 4, 5, 6, 7, 0, 0]) for _ in range(n)]
        extra = {}
        if r.random() < 0.3:
            extra["CTrackCell::Serialise"] = [r.choice([0, 0, 1])]
        if r.random() < 0.3:
            extra["CRegionEffectCell::Serialise"] = [r.choice([0, 0, 1])]
        full = r.random() < 0.08
        fail = None if full else r.randrange(0, 60)
        return map_load_case(r, statuses, r.choice([0, 1, 1, -1, 2, 7]), fail_at=fail, extra_q=extra)
    n = r.randrange(1, 30)
    full = r.random() < 0.08
    extra = {}
    if r.random() < 0.3:
        extra["CTrackCell::Serialise"] = [r.choice([0, 0, 1])]
    if r.random() < 0.3:
        extra["CRegionEffectCell::Serialise"] = [r.choice([0, 0, 1])]
    return map_save_case(r, r.choice([0, 1, 1, -1, 2, 9]), sparse(r, n, 0.4) + [0], sparse(r, n, 0.4) + [0], [r.choice([0, 1]) for _ in range(n)] + [1],
                         extra_q=extra, fail_write_at=None if full else r.randrange(0, 3 * n + 3))


def sinit_edges():
    yield base(random.Random(1465), "sinit", [])


def sinit_random(r):
    return base(r, "sinit", [])


TESTS = [
    FnTest("CMapCell::Serialise", ppc, host, edge=cell_edges, rnd=cell_random),
    FnTest("CMacroAI::Serialise", ppc, host, edge=macro_edges, rnd=macro_random),
    FnTest("CMapCell::PlaceLitter", ppc, host, edge=litter_edges, rnd=litter_random),
    FnTest("CMap::Serialise", ppc, host, edge=map_edges, rnd=map_random),
    FnTest("__sinit_MapDelta_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
]
