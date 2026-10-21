"""Unit w179, CCellBase and CTrackCell, against libtpw_unit_CellBase.dylib (tests/unit_CellBase.cpp), main app ("SimTheme Park"),
src/game/park/CellBase.cpp and src/game/buildings/TrainingCentre.cpp:
  CCellBase: ModifyNeighbours 0x10081608, GetNeighbours 0x100816e8, NumberOfNeighbours 0x10081744, RemoveDirection 0x1008184c,
  NumberOfHVNeighbours 0x10081890, GetDirection 0x10081930, NumberOfDirections 0x10081968, DecOverlapCounter 0x100819d8, SetOverlapCounter
  0x10081a1c, GetOverlapCounter 0x10081a5c, GetTrackCellEquiv 0x100f4844 (const) and 0x100f48c4, GetMapCellEquiv 0x100f4928 (const) and
  0x100f49a0, Serialise 0x100f49fc, SAVEVARSTRUCT / LOADVARSTRUCT<CELL_IndexType> 0x100f4d70 / 0x100f4f0c, LOADVAR / SAVEVAR<CELL_MODES>
  0x100f52a0 / 0x100f577c;
  CTrackCell: HasBeenModifiedFrom 0x1012fb74, Serialise 0x1012fbc0, GetNeighbouringCell(MapDelta, int) 0x1012fcb8 and (uint8, int) 0x1012fddc.

A case is a cell (0x28 bytes between two 0x40-byte guards of random bytes; the fields are numbers, packed per side) and a call. The cell is built
twice from ONE tuple of field values (big-endian in emulator memory, little-endian in the host library). The serialise instances over the basic
types are REAL on both sides, so the file is the fake: LbFile_Read hands out the case's bytes in order, LbFile_Write records the bytes as hex
(the queues replace the result and the transferred count). Every other callee is a fake that records its arguments: CWorldSavvy::GetMap (the
cell as an offset from the cell, answers the case's base), MapDelta::operator* (wrapping products), CMap::IsXYValid (queued answer),
CSaveLoad::WriteSerialiseLog, GAME_ASSERTMSG. Cell pointers answered are compared as the signed 32-bit offset from the base they were computed
from (0 for NULL).

What is compared: the answer, the log, the cell's fields (decoded per side) and the guards.
"""
import ctypes as C
import hashlib
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_CellBase"
MASK = 0xFFFFFFFF

# (op index, name, address, size)
OPS = [
    (0, "CCellBase::ModifyNeighbours", 0x10081608, 60), (1, "CCellBase::GetNeighbours", 0x100816E8, 44),
    (2, "CCellBase::NumberOfNeighbours", 0x10081744, 152), (3, "CCellBase::RemoveDirection", 0x1008184C, 16),
    (4, "CCellBase::NumberOfHVNeighbours", 0x10081890, 104), (5, "CCellBase::GetDirection", 0x10081930, 8),
    (6, "CCellBase::NumberOfDirections", 0x10081968, 60), (7, "CCellBase::DecOverlapCounter", 0x100819D8, 16),
    (8, "CCellBase::SetOverlapCounter", 0x10081A1C, 12), (9, "CCellBase::GetOverlapCounter", 0x10081A5C, 8),
    (10, "CCellBase::GetTrackCellEquiv const", 0x100F4844, 76), (11, "CCellBase::GetTrackCellEquiv", 0x100F48C4, 48),
    (12, "CCellBase::GetMapCellEquiv const", 0x100F4928, 68), (13, "CCellBase::GetMapCellEquiv", 0x100F49A0, 44),
    (14, "CCellBase::Serialise", 0x100F49FC, 836), (15, "SAVEVARSTRUCT<CELL_IndexType>", 0x100F4D70, 312),
    (16, "LOADVARSTRUCT<CELL_IndexType>", 0x100F4F0C, 260), (17, "LOADVAR<CELL_MODES>", 0x100F52A0, 236),
    (18, "SAVEVAR<CELL_MODES>", 0x100F577C, 260), (19, "CTrackCell::HasBeenModifiedFrom", 0x1012FB74, 8),
    (20, "CTrackCell::Serialise", 0x1012FBC0, 196), (21, "CTrackCell::GetNeighbouringCell MapDelta", 0x1012FCB8, 228),
    (22, "CTrackCell::GetNeighbouringCell direction", 0x1012FDDC, 412),
]
ADDR = {op: a for op, _, a, _ in OPS}
OWN = {a for _, _, a, _ in OPS}
# the basic instances the serialisers call (REAL on both sides)
REAL_EXTRA = {0x100F17C8, 0x100F1A04, 0x100F518C, 0x100F5658, 0x100F5074, 0x100F5530, 0x1000F02C, 0x1000F5DC, 0x100F53CC, 0x100F58C0}

ASSERT, WRITELOG, GETMAP, MULT, ISXY = 0x1000DDE8, 0x1015B3DC, 0x10142098, 0x101037D0, 0x101012F4
G_WORLD, G_WORLDMAP, GSAVELOAD, G_FLAG = 0x1037EC0C, 0x1037EC08, 0x1037FEFC, 0x102E4818
FILE_HANDLE = 0x6E100000
LABEL_SAVELOAD, LABEL_FILE = 6, 8
DELTAS = [0x1034C2AC, 0x1034C298, 0x1034C284, 0x1034C270, 0x1034C25C, 0x1034C248, 0x1034C234, 0x1034C220]
DELTA_VALUES = [(0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1)]
GUARD = 0x40
CELL_BYTES = 0x28
HOST_LAYOUT = [0x28, 12, 4, 0x14, 8, 0x24, 12]
FIELDS = [("id", "H"), ("kind", "B"), ("f03", "B"), ("mesh", "I"), ("type", "i"), ("neigh", "B"), ("dir", "B"), ("flags", "H"),
          ("parent", "H"), ("f12", "H"), ("t0", "I"), ("t1", "I"), ("t2", "I"), ("ovl", "h"), ("hoard", "B"), ("f23", "B"),
          ("seg", "h"), ("f26", "H")]
FMT = "".join(f for _, f in FIELDS)
assert struct.calcsize("<" + FMT) == CELL_BYTES


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.update(REAL_EXTRA)
R = CL.recorder


def off(e, p):
    d = p - e.cb_cell
    return d if -GUARD <= d < CELL_BYTES + GUARD else -1


def lb_read(e):
    L, buf, length = e.cl, e.r(5), e.r(6)
    L.rec("LbFile_Read", (L.lab(e.r(4)), length))
    data = e.cb_file[e.cb_cur[0]:]
    taken = min(len(data), length)
    count = taken
    forced = L.ret("LbFile_Read:xfer", -1)
    if forced >= 0:
        count = forced
    e.write(e.r(7), (count & MASK).to_bytes(4, "big"))
    result = L.ret("LbFile_Read")
    if count == length and result >= 0:
        e.write(buf, (data[:taken] + bytes(length))[:length])
    e.cb_cur[0] += taken
    e.write(e.r(3), (result & MASK).to_bytes(4, "big"))


def lb_write(e):
    L, length = e.cl, e.r(6)
    L.rec("LbFile_Write", (L.lab(e.r(4)), length), (e.read(e.r(5), length).hex()[:150] if length else "",))
    forced = L.ret("LbFile_Write:xfer", -1)
    e.write(e.r(7), ((forced if forced >= 0 else length) & MASK).to_bytes(4, "big"))
    e.write(e.r(3), (L.ret("LbFile_Write") & MASK).to_bytes(4, "big"))


def stub_getmap(e):
    e.cl.rec("CWorldSavvy::GetMap", (off(e, e.r(3)),))
    e.set_r(3, e.cb_smap)


def stub_mult(e):
    # r3 the result record, r4 the delta (this), r5 the factor
    k = s32(e.r(5))
    x, y = struct.unpack(">ii", e.read(e.r(4), 8))
    e.cl.rec("MapDelta::operator*", (x, y, k))
    e.write(e.r(3), struct.pack(">ii", s32(x * k), s32(y * k)) + bytes(4))


def stub_isxy(e):
    e.cl.rec("CMap::IsXYValid", (s32(e.r(3)), s32(e.r(4))))
    e.set_r(3, T.u32(e.cl.ret("CMap::IsXYValid")))


STUBS.internal(ASSERT, lambda e: e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)),), (e.cstr(e.r(4))[:159],)))
STUBS.internal(WRITELOG, R("CSaveLoad::WriteSerialiseLog", "Lisi"))
STUBS.internal(GETMAP, stub_getmap)
STUBS.internal(MULT, stub_mult)
STUBS.internal(ISXY, stub_isxy)
STUBS.imp("LbFile_Read__FPvPvUlPUl", lb_read)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", lb_write)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Ppc:
    order = ">"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({GSAVELOAD: LABEL_SAVELOAD, FILE_HANDLE: LABEL_FILE}, case["queues"])
        e.cb_file, e.cb_cur = case["file"], [0]
        e.cb_smap = case["smap"]
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)
        self.e.cl = None

    def build(self, raw, c):
        e = self.e
        self.base = e.alloc(len(raw), raw)
        self.cell = e.cb_cell = self.base + GUARD
        e.write(G_FLAG, bytes([c["flag"]]))
        e.write(G_WORLD, c["world"].to_bytes(4, "big"))
        e.write(G_WORLDMAP, c["wmap"].to_bytes(4, "big"))
        for a, (x, y) in zip(DELTAS, DELTA_VALUES):
            e.write(a, struct.pack(">iiI", x, y, 0))
        self.name = e.alloc(len(c["name"]) + 1, c["name"].encode() + b"\0")

    def read(self):
        return self.e.read(self.base, GUARD + CELL_BYTES + GUARD)

    def log(self):
        return self.L.log

    def call(self, op, c):
        e, a, b, k, cell = self.e, c["a"], c["b"], c["c"], self.cell
        w = {0: [cell, a & 0xFF], 3: [cell, a & 0xFF], 8: [cell, a & MASK], 19: [cell, cell],
             14: [cell, FILE_HANDLE, a & MASK, b & MASK, k & MASK], 20: [cell, FILE_HANDLE, a & MASK, b & MASK, k & MASK],
             15: [FILE_HANDLE, cell + 0x14, self.name, a & MASK], 16: [FILE_HANDLE, cell + 0x14, self.name, a & MASK],
             17: [FILE_HANDLE, cell + 8, self.name, a & MASK], 18: [FILE_HANDLE, cell + 8, self.name, a & MASK],
             22: [cell, a & 0xFF, b & MASK]}
        if op == 21:
            delta = e.alloc(12, struct.pack(">iiI", a, b, 0))
            words = [cell, delta, k & MASK]
        else:
            words = w.get(op, [cell])
        return T.call_ppc(e, ADDR[op], words, max_insns=50_000_000, timeout_us=300_000_000)[0] & MASK


class Host:
    order = "<"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_cbase_set_file(case["file"], len(case["file"]))
        CL.host_begin(lib, {LABEL_SAVELOAD: lib.tpw_cbase_addr(b"gSaveLoad"), LABEL_FILE: lib.tpw_cbase_addr(b"file")}, case["queues"])

    def done(self):
        pass

    def build(self, raw, c):
        self.base = self.lib.tpw_cbase_addr(b"cellblock")
        C.memmove(self.base, raw, len(raw))
        self.cell = self.base + GUARD
        self.lib.tpw_cbase_set_globals(c["flag"], c["world"], c["wmap"], c["smap"])
        self.name = C.create_string_buffer(c["name"].encode())

    def read(self):
        return C.string_at(self.base, GUARD + CELL_BYTES + GUARD)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, c):
        return self.lib.tpw_cbase_call(op, self.cell, c["a"], c["b"], c["c"], C.addressof(self.name)) & 0xFFFFFFFFFFFFFFFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cbase_addr.restype, lib.tpw_cbase_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cbase_set_globals.argtypes = [C.c_uint32, C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_cbase_set_file.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_cbase_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cbase_call.restype = C.c_int64
    lib.tpw_cbase_call.argtypes = [C.c_uint32, C.c_uint64, C.c_int64, C.c_int64, C.c_int64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cbase_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- running a case

WORLD_OPS = {21, 22}
VOID = {0, 3, 7, 8}
PTR_BASE = {10: "smap", 11: "wmap", 12: "smap", 13: "wmap", 21: "world", 22: "world"}


def run(s, c):
    try:
        raw = bytes(c["guard_a"]) + struct.pack(s.order + FMT, *[c["fields"][k] for k, _ in FIELDS]) + bytes(c["guard_b"])
        s.build(raw, c)
        op = c["op"]
        r = s.call(op, c)
        if op in PTR_BASE:
            if r & 0xFFFFFFFF == 0 and r < (1 << 32):
                ans = ("null",)
            else:
                ans = ("ptr", s32(r - c[PTR_BASE[op]]))
        elif op in VOID:
            ans = 0
        elif op in (1, 5):
            ans = r & 0xFF
        else:
            ans = s32(r)
        out = {"answer": ans, "log": s.log()}
        data = s.read()
        out["guards"] = hashlib.md5(data[:GUARD] + data[GUARD + CELL_BYTES:]).hexdigest()
        out["cell"] = struct.unpack(s.order + FMT, data[GUARD:GUARD + CELL_BYTES])
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

NAMES = ["mType", "mTileData", "x", "a long name 123"]


def rfield(r, fmt):
    bits = 8 * struct.calcsize(fmt)
    v = r.getrandbits(bits)
    if fmt.islower():
        v = v - (1 << bits) if v >= 1 << (bits - 1) else v
    return v


def case(r, op, fields=None, a=0, b=0, c=0, flag=None, queues=None, file=None, name=None, bases=None):
    f = {k: rfield(r, fm) for k, fm in FIELDS}
    f.update(fields or {})
    world = r.choice([0x28000000, 0x2A000000, 0x1F000000])
    wmap = r.choice([0x30000000, 0x31100000, 0x20000000])
    smap = r.choice([0x38000000, 0x3A000000, 0x21000000])
    if file is None:
        file = r.randbytes(r.choice([0, 3, 40, 48, 48, 48])) if r.random() < 0.85 else r.randbytes(r.randrange(0, 48))
    return {"op": op, "fields": f, "a": a, "b": b, "c": c, "flag": r.choice([0, 1, 0x80, 0xFF]) if flag is None else flag,
            "queues": queues or {}, "file": file, "name": r.choice(NAMES) if name is None else name,
            "world": world, "wmap": wmap, "smap": smap, "guard_a": r.randbytes(GUARD), "guard_b": r.randbytes(GUARD)}


BYTES = [0, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0xFF, 0x55, 0xAA, 0x0F, 0xF0, 0x5, 0x14]


def mask_case(r, op, flag=None, kind=None, mask=None):
    kind = r.choice([0, 1, 2, 2, 3, 0xFF]) if kind is None else kind
    mask = r.choice(BYTES + [r.getrandbits(8)]) if mask is None else mask
    return case(r, op, {"kind": kind, "neigh": r.choice(BYTES + [r.getrandbits(8)]), "hoard": r.choice(BYTES + [r.getrandbits(8)]),
                        "dir": r.choice(BYTES + [r.getrandbits(8)])}, a=mask, flag=flag)


def mask_edges(op):
    def gen():
        r = random.Random(1790 + op)
        for flag in (0, 1, 0x80, 0xFF):
            for kind in (0, 1, 2, 3, 0xFF):
                for mask in (0, 0xFF, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80):
                    yield mask_case(r, op, flag, kind, mask)
        for _ in range(40):
            yield mask_case(r, op)
    return gen


def mask_random(op):
    return lambda r: mask_case(r, op)


OVL = [0, 1, -1, 0x7FFF, -0x8000, 0x1234, -0x1234, 2, -2]


def ovl_case(r, op):
    arg = r.choice([0, 1, -1, 0x7FFF, 0x8000, 0x12345, -0x10000, -0x80000000, 0x7FFFFFFF, r.getrandbits(32) - (1 << 31)])
    return case(r, op, {"ovl": r.choice(OVL + [rfield(r, "h")])}, a=arg)


def ovl_edges(op):
    def gen():
        r = random.Random(1800 + op)
        for o in OVL:
            for arg in (0, 1, -1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x12345, -0x10000, -0x80000000, 0x7FFFFFFF):
                yield case(r, op, {"ovl": o}, a=arg)
        for _ in range(20):
            yield ovl_case(r, op)
    return gen


IDS = [0, 1, 2, 0x7F, 0x80, 0x81, 0xFF, 0x100, 0x3FFF, 0x4000, 0x4001, 0x8000, 0xFFFF, 0x1234]


def equiv_case(r, op, cid=None):
    return case(r, op, {"id": r.choice(IDS + [r.getrandbits(16)]) if cid is None else cid})


def equiv_edges(op):
    def gen():
        r = random.Random(1810 + op)
        for cid in IDS:
            yield equiv_case(r, op, cid)
        for _ in range(20):
            yield equiv_case(r, op)
    return gen


def equiv_random(op):
    return lambda r: equiv_case(r, op)


def fail_queues(r, save, step, kind):
    key = "LbFile_Write" if save else "LbFile_Read"
    k = r.choice(["res", "short", "long"]) if kind is None else kind
    if k == "res":
        return {key: [0] * step + [r.choice([-1, 1, -0x80000000, 0x7FFFFFFF])]}
    if k == "short":
        return {key + ":xfer": [-1] * step + [r.choice([0, 1, 3])]}
    return {key + ":xfer": [-1] * step + [r.choice([5, 9, 0xFFFFFFFF])]}


VERSIONS = [-1, 0, 1, 2, 3, 100, -5, -0x80000000, 0x7FFFFFFF]


def ser_case(r, op, save=None, version=None, step=None, kind=None):
    save = r.choice([0, 1, 5, -1]) if save is None else save
    version = r.choice([0, 1, 1, 0, -1, 2, 3, 100, -5]) if version is None else version
    q = fail_queues(r, 1 if save else 0, step, kind) if step is not None else {}
    if r.random() < 0.3:
        q["CMap::IsXYValid"] = [1]
    return case(r, op, a=save, b=version, c=r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]), queues=q,
                file=r.randbytes(50) if r.random() < 0.85 else None)


def ser_edges(op, steps):
    def gen():
        r = random.Random(1820 + op)
        for save in (0, 1):
            for version in VERSIONS:
                yield ser_case(r, op, save, version)
            for version in (0, 1):
                for step in range(steps):
                    for kind in ("res", "short", "long"):
                        yield ser_case(r, op, save, version, step, kind)
        for save in (5, -1, 0x80000000):
            yield ser_case(r, op, save, 1)
        for n in range(0, 40, 3):
            yield case(r, op, a=0, b=1, file=r.randbytes(n))
    return gen


def ser_random(op, steps):
    return lambda r: ser_case(r, op, step=r.choice([None, None] + list(range(steps))))


def inst_case(r, op, step=None, kind=None, nolog=None):
    save = op in (15, 18)
    q = fail_queues(r, 1 if save else 0, 0, kind) if step is not None else {}
    n = r.choice([0, 1, 2, -1, 0x7FFFFFFF]) if nolog is None else nolog
    return case(r, op, a=n, queues=q, file=r.randbytes(r.choice([0, 3, 4, 11, 12, 13, 20])))


def inst_edges(op):
    def gen():
        r = random.Random(1830 + op)
        for nolog in (0, 1, 2, -1):
            yield inst_case(r, op, None, None, nolog)
            for kind in ("res", "short", "long"):
                yield inst_case(r, op, 0, kind, nolog)
        for _ in range(20):
            yield inst_case(r, op)
    return gen


def inst_random(op):
    return lambda r: inst_case(r, op, step=r.choice([None, None, 0]))


def has_case(r):
    return case(r, 19)


def has_edges():
    r = random.Random(1840)
    for _ in range(6):
        yield has_case(r)


XY = [0, 1, -1, 2, -2, 3, 5, 0x7F, 0x80, -0x80, 0x10000, -0x10000, 0x7FFFFFFF, -0x80000000, 0x40000000]
OFFS = [0, 1, -1, 2, 3, -3, 0x80, 0x10000, 0x7FFFFFFF, -0x80000000, 0x40000000]
CORNER_IDS = [1, 2, 0x80, 0x7F, 0x81, 0x4000, 0x3FFF, 0x3F81, 0, 0xFFFF, 0x4001, 0x1234]


def nb_case(r, cid=None, dx=None, dy=None, off_=None, ans=None):
    cid = r.choice(CORNER_IDS + [r.randrange(1, 0x4001)]) if cid is None else cid
    dx = r.choice(XY + [r.randrange(-3, 4)]) if dx is None else dx
    dy = r.choice(XY + [r.randrange(-3, 4)]) if dy is None else dy
    off_ = r.choice(OFFS + [r.randrange(-3, 4)]) if off_ is None else off_
    answer = r.choice([0, 1, 1, -1, 2, 0x100]) if ans is None else ans
    return case(r, 21, {"id": cid}, a=dx, b=dy, c=off_, queues={"CMap::IsXYValid": [answer]})


def nb_edges():
    r = random.Random(1850)
    for cid in CORNER_IDS:
        for (dx, dy) in ((1, 0), (0, 1), (-1, 0), (0, -1), (1, 1), (-1, -1), (0, 0)):
            for off_ in (1, 2, -1, 0):
                for ans in (0, 1):
                    yield nb_case(r, cid, dx, dy, off_, ans)
    for ans in (0, 1, -1, 2, 0x100, 0x7FFFFFFF, -0x80000000):
        yield nb_case(r, ans=ans)
    for dx in XY:
        for off_ in OFFS:
            yield nb_case(r, None, dx, r.choice(XY), off_)


def nb_random(r):
    return nb_case(r)


def dir_case(r, d=None, off_=None, ans=None):
    d = r.choice([1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, r.getrandbits(8)]) if d is None else d
    off_ = r.choice(OFFS + [r.randrange(-3, 4)]) if off_ is None else off_
    answer = r.choice([0, 1, 1, 0, 5]) if ans is None else ans
    return case(r, 22, {"id": r.choice(CORNER_IDS + [r.randrange(1, 0x4001)])}, a=d, b=off_, queues={"CMap::IsXYValid": [answer]})


def dir_edges():
    r = random.Random(1860)
    for d in range(256):
        for ans in (0, 1):
            yield dir_case(r, d, r.choice([1, 2, -1, 0, 3]), ans)
    for cid in CORNER_IDS:
        for d in (1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80):
            yield dir_case(r, d, 1, 1) if cid == 0 else case(r, 22, {"id": cid}, a=d, b=2, queues={"CMap::IsXYValid": [1]})


def dir_random(r):
    return dir_case(r)


TESTS = []
for _op, _name, _a, _s in OPS:
    if _op in (0, 1, 2, 3, 4, 5, 6):
        TESTS.append(FnTest(_name, ppc, host, edge=mask_edges(_op), rnd=mask_random(_op)))
    elif _op in (7, 8, 9):
        TESTS.append(FnTest(_name, ppc, host, edge=ovl_edges(_op), rnd=lambda r, o=_op: ovl_case(r, o)))
    elif _op in (10, 11, 12, 13):
        TESTS.append(FnTest(_name, ppc, host, edge=equiv_edges(_op), rnd=equiv_random(_op)))
    elif _op == 14:
        TESTS.append(FnTest(_name, ppc, host, edge=ser_edges(_op, 9), rnd=ser_random(_op, 9)))
    elif _op == 20:
        TESTS.append(FnTest(_name, ppc, host, edge=ser_edges(_op, 10), rnd=ser_random(_op, 10)))
    elif _op in (15, 16, 17, 18):
        TESTS.append(FnTest(_name, ppc, host, edge=inst_edges(_op), rnd=inst_random(_op)))
    elif _op == 19:
        TESTS.append(FnTest(_name, ppc, host, edge=has_edges, rnd=has_case))
    elif _op == 21:
        TESTS.append(FnTest(_name, ppc, host, edge=nb_edges, rnd=nb_random))
    else:
        TESTS.append(FnTest(_name, ppc, host, edge=dir_edges, rnd=dir_random))
