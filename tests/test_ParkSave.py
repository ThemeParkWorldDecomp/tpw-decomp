"""Differential tests for unit w158, the park analyser's value and history save templates, against libtpw_unit_ParkSave.dylib
(tests/unit_ParkSave.cpp), main app ("SimTheme Park"), src/game/park/analyser.cpp:
  CParkAnalyserVal<int>::Serialise 0x100ef278, CParkAnalyserVal<float>::Serialise 0x100ef4a8,
  CHistory<unsigned char,50> 0x100ef644, <int,5> 0x100efbbc, <int,50> 0x100efe8c, <float,144> 0x100f0414, <int,144> 0x100f0754,
  <int,30> 0x10110fec, <unsigned int,30> 0x10111560 (Serialise) and CHistory<ObjectId,3>::SerialiseStruct 0x100f0b80

A case is an object image and a call. The image is built twice from one description: in emulator memory (big-endian) for the original and in the host
library's memory (little-endian) for the translation, with 0x40 guard bytes on each side. Every LOADVAR / SAVEVAR / LOADVARSTRUCT / SAVEVARSTRUCT callee is
a fake on both sides: it logs its kind, the file's label, the value's offset from the object, noLog and the name text, answers from the queue "ans" (the
case's failing call answers 1; a call past the cap always does, so a growing loop ends) and, for a load, stores the next value of the queue "val" in its
own size and the side's byte order (-1: leaves the value alone).

Compared: the answer, the call log, every typed field of the image (mTemp, the elements, the indexes, the flag, decoded by size) and every other byte of the
object and its guards. A load that lands on a field of another size cannot give the same bytes in the two byte orders; the generator stores there only
values whose bytes read the same both ways (the flag's byte and the padding and guards), or nothing (a 1 or 2 byte store into half of a 4 byte index).
No fused operations. Not tested: elements that overwrite half of an index with a value (the byte orders disagree on which half).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ParkSave"
MASK = 0xFFFFFFFF
INT_MIN, INT_MAX = -0x80000000, 0x7FFFFFFF
GUARD = 0x40
FILE = 0x1000          # the file argument (a label on both sides)

# ---- the callees faked on both sides: address -> (event name, loads, size of the value)
CALLEES = {
    0x1000EF18: ("LOADVAR<int>", True, 4), 0x1000F4B0: ("SAVEVAR<int>", False, 4),
    0x1000F144: ("LOADVAR<bool>", True, 1), 0x1000F70C: ("SAVEVAR<bool>", False, 1),
    0x100F44A4: ("LOADVAR<unsigned int>", True, 4), 0x100F45BC: ("SAVEVAR<unsigned int>", False, 4),
    0x100F17C8: ("LOADVAR<unsigned char>", True, 1), 0x100F1A04: ("SAVEVAR<unsigned char>", False, 1),
    0x100F16B4: ("LOADVAR<float>", True, 4), 0x100F18D8: ("SAVEVAR<float>", False, 4),
    0x100F1300: ("LOADVARSTRUCT<ObjectId>", True, 2), 0x100F1188: ("SAVEVARSTRUCT<ObjectId>", False, 2),
}

# ---- the unit's functions: key -> (address, size, host entry, element size, count N, kind)
INSTANCES = {
    "val_int": (0x100EF278, 276, "tpw_ps_val_int", 4, 0, "val"),
    "val_float": (0x100EF4A8, 276, "tpw_ps_val_float", 4, 0, "val"),
    "uchar50": (0x100EF644, 584, "tpw_ps_uchar50", 1, 50, "hist"),
    "int5": (0x100EFBBC, 592, "tpw_ps_int5", 4, 5, "hist"),
    "int50": (0x100EFE8C, 592, "tpw_ps_int50", 4, 50, "hist"),
    "float144": (0x100F0414, 592, "tpw_ps_float144", 4, 144, "hist"),
    "int144": (0x100F0754, 592, "tpw_ps_int144", 4, 144, "hist"),
    "ring3": (0x100F0B80, 592, "tpw_ps_ring3", 2, 3, "hist"),
    "int30": (0x10110FEC, 592, "tpw_ps_int30", 4, 30, "hist"),
    "uint30": (0x10111560, 592, "tpw_ps_uint30", 4, 30, "hist"),
}
OWN = {v[0] for v in INSTANCES.values()}
NAMES = {
    "val_int": "CParkAnalyserVal<int>::Serialise", "val_float": "CParkAnalyserVal<float>::Serialise",
    "uchar50": "CHistory<unsigned char,50>::Serialise", "int5": "CHistory<int,5>::Serialise", "int50": "CHistory<int,50>::Serialise",
    "float144": "CHistory<float,144>::Serialise", "int144": "CHistory<int,144>::Serialise",
    "ring3": "CHistory<ObjectId,3>::SerialiseStruct", "int30": "CHistory<int,30>::Serialise", "uint30": "CHistory<unsigned int,30>::Serialise",
}


def up(v, a):
    return (v + a - 1) // a * a


def layout(key):
    """The Mac's (and the host's) layout of an instance: object size and the typed fields (offset, size) in order."""
    _, _, _, ts, n, kind = INSTANCES[key]
    if kind == "val":
        return 0xC, [(0, 4), (4, 4), (8, 1)], None
    eo = ts
    ce = up(eo + n * ts, 4)
    ne, wr = ce + 4, ce + 8
    size = up(wr + 1, 4)
    fields = [(0, ts)] + [(eo + i * ts, ts) for i in range(n)] + [(ce, 4), (ne, 4), (wr, 1)]
    return size, fields, (eo, ce, ne, wr)


# the host's offsets in tpw_ps_layout's order: mData, mCurrentEntry, mNumEntries, mWrappedAround, sizeof per ring, then the two values
RING_ORDER = ["uchar50", "int5", "int50", "float144", "int144", "ring3", "int30", "uint30"]
HOST_LAYOUT = []
for _k in RING_ORDER:
    _size, _f, (_eo, _ce, _ne, _wr) = layout(_k)
    HOST_LAYOUT += [_eo, _ce, _ne, _wr, _size]
HOST_LAYOUT += [4, 8, 0xC, 4, 8, 0xC]


def s32(v):
    return T.s32(v)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def make_stub(name, loads, size):
    def stub(e):
        L = e.cl
        value = e.r(4)
        L.rec(name, (L.lab(e.r(3)), value - L.base, s32(e.r(6))), (e.cstr(e.r(5))[:159],))
        answer = L.ret("ans", 0)
        stored = L.ret("val", -1)
        if loads and stored != -1:
            e.write(value, (stored & ((1 << (8 * size)) - 1)).to_bytes(size, "big"))
        e.set_r(3, answer & MASK)
    return stub


for _addr, (_name, _loads, _size) in CALLEES.items():
    STUBS.internal(_addr, make_stub(_name, _loads, _size))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, data):
        self.write(a, data)


class Ppc(Side):
    order = "big"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({FILE: 1}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def set_base(self, a):
        self.L.base = a

    def log(self):
        return self.L.log

    def call(self, key, obj, case):
        r3, _, _ = T.call_ppc(self.e, INSTANCES[key][0], [obj, FILE, case["save"] & MASK, case["version"] & MASK, case["unused"] & MASK])
        return r3 & MASK


class Host(Side):
    order = "little"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: FILE}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_ps_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def set_base(self, a):
        self.lib.tpw_ps_base(a)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, key, obj, case):
        fn = getattr(self.lib, INSTANCES[key][2])
        return fn(obj, s32(case["save"]), s32(case["version"]), case["unused"] & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ps_alloc.restype, lib.tpw_ps_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ps_base.restype, lib.tpw_ps_base.argtypes = None, [C.c_uint64]
    lib.tpw_ps_layout.argtypes = [C.POINTER(C.c_int64)]
    for key, spec in INSTANCES.items():
        fn = getattr(lib, spec[2])
        fn.restype, fn.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_uint32]
    out = (C.c_int64 * 64)()
    n = lib.tpw_ps_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, (list(out)[:n], HOST_LAYOUT)
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """The object with its guards: the case's raw bytes everywhere, then each typed field in the side's byte order."""
    size, fields, _ = layout(case["key"])
    block = s.alloc(size + 2 * GUARD)
    obj = block + GUARD
    s.write(block, case["raw"])
    for (off, sz), v in zip(fields, case["fields"]):
        s.write(obj + off, (v & ((1 << (8 * sz)) - 1)).to_bytes(sz, s.order))
    s.set_base(obj)
    return block, obj


def observe(s, case, block, answer):
    size, fields, _ = layout(case["key"])
    image = bytearray(s.read(block, size + 2 * GUARD))
    decoded = []
    for off, sz in fields:
        a = GUARD + off
        decoded.append(int.from_bytes(bytes(image[a:a + sz]), s.order))
        image[a:a + sz] = b"\0" * sz
    return {"answer": answer, "log": s.log(), "fields": decoded, "raw": bytes(image)}


def run(s, case):
    try:
        block, obj = build(s, case)
        return observe(s, case, block, s.call(case["key"], obj, case))
    finally:
        s.done()


def ppc(key):
    return lambda e, case: run(Ppc(e, case), case)


def host(key):
    return lambda lib, case: run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def classify(key, c):
    """Where the load of call c lands and what may be stored: ("free", size), ("pal", size) or ("none", size)."""
    ts, n, kind = INSTANCES[key][3], INSTANCES[key][4], INSTANCES[key][5]
    osz, fields, geo = layout(key)
    if kind == "val":
        return {0: ("free", 4), 1: ("free", 1)}.get(c, ("none", 1))
    eo, ce, ne, wr = geo
    dest = [(ce, 4), (ne, 4), (wr, 1), (0, ts)][c] if c < 4 else (eo + (c - 4) * ts, ts)
    if dest in set(fields):
        return "free", dest[1]
    for off, sz in fields:
        if dest[0] < off + sz and off < dest[0] + dest[1] and (off, sz) != (wr, 1):
            return "none", dest[1]
    return "pal", dest[1]


def pal(r, size):
    if size == 1:
        return r.randrange(256)
    if size == 2:
        b = r.choice([0, 0xFF, r.randrange(256)])
        return b * 0x101
    b0, b1 = r.choice([0, 0xFF, r.randrange(256)]), r.choice([0, 0x80, 0xFF, r.randrange(256)])
    return b0 | (b1 << 8) | (b1 << 16) | (b0 << 24)


def word(r, n):
    return r.choice([0, 1, 2, n - 1, n, n + 1, n + 2, n + 3, MASK, 0x80000000, INT_MAX, r.getrandbits(32), r.getrandbits(32), r.randrange(0, 300)]) & MASK


def value_for(r, key, c, forced=None):
    kind, size = classify(key, c)
    if kind == "none":
        return -1
    if kind == "pal":
        return pal(r, size)
    v = word(r, INSTANCES[key][4]) if forced is None else forced
    return v & ((1 << (8 * size)) - 1)


def make_case(r, key, save=None, version=None, fail=None, ne_image=None, ne_load="r", ce_load=None, tail=None, density=0.2):
    ts, n, kind = INSTANCES[key][3], INSTANCES[key][4], INSTANCES[key][5]
    size, fields, geo = layout(key)
    save = r.choice([0, 0, 1, 1, 2, -1]) if save is None else save
    version = r.choice([0, 0, 0, 1, 1, 1, -1, 2, INT_MIN, INT_MAX]) if version is None else version
    vals = [r.getrandbits(8 * sz) if r.random() < 0.7 else r.choice([0, (1 << (8 * sz)) - 1]) for _, sz in fields]
    if kind == "hist":
        eo, ce, ne, wr = geo
        ne_i = len(fields) - 2
        if ne_image is None:
            ne_image = r.choice([n, n, 0, 1, -1, INT_MIN, n - 1, n + 1, r.randrange(0, n + 2), INT_MAX]) if r.random() < 0.8 else r.randrange(0, n + 1)
        vals[ne_i] = ne_image & MASK
    cap = 4 + n + 11 if kind == "hist" else 2
    calls = cap + 1
    ans = [0] * calls
    ans[cap] = 1
    if fail is not None and fail < cap:
        ans[fail] = 1
    elif fail is None and r.random() < 0.3:
        ans[r.randrange(0, min(cap, 8))] = 1
    val = [-1] * calls
    for c in range(calls):
        if r.random() < density:
            val[c] = value_for(r, key, c)
    if kind == "hist":
        if ne_load == "r":
            val[1] = value_for(r, key, 1) if r.random() < 0.6 else -1
        elif ne_load is not None:
            val[1] = ne_load & ((1 << 32) - 1)
        if ce_load is not None:
            val[0] = ce_load
        if tail is not None:     # the loads at i = N and i = N + 1 (the index words for the 4 byte rings)
            for off, v in tail:
                if 4 + n + off < calls and classify(key, 4 + n + off)[0] == "free":
                    val[4 + n + off] = v & MASK
    val[cap] = -1
    raw = bytes(r.getrandbits(8) for _ in range(size + 2 * GUARD))
    return {"key": key, "save": save, "version": version, "fields": vals, "raw": raw, "unused": r.getrandbits(32),
            "queues": {"ans": ans, "val": val}}


def edges(key):
    r = random.Random(0x158 + INSTANCES[key][0])
    n = INSTANCES[key][4]
    kind = INSTANCES[key][5]
    for save in (0, 1, 2, -1):
        for version in (-1, 0, 1, 2, INT_MIN, INT_MAX):
            yield make_case(r, key, save, version, ne_image=n, ne_load=None, density=0.0)
    if kind == "val":
        for save in (0, 1):
            for fail in (None, 0, 1):
                for _ in range(3):
                    yield make_case(r, key, save, r.choice([0, 1]), fail)
        return
    for save in (0, 1):
        for fail in (0, 1, 2, 3, 4, n + 3, 4 + n - 1):
            for ne_image in (n, 0, 1, -1, INT_MIN, INT_MAX):
                yield make_case(r, key, save, r.choice([0, 1]), fail, ne_image, None, density=0.1)
        for ne_load in (0, 1, n, n - 1, n + 1, n + 2, n + 3, MASK):
            for ce_load in (None, 7):
                yield make_case(r, key, 0, 0, None, n, ne_load, ce_load, density=0.0)
        for tail in ([(0, 5), (1, 0)], [(0, 0), (1, n + 6)], [(0, 3), (1, 2)], [(0, MASK), (1, n + 1)], [(1, 1)], [(0, 1), (1, MASK)]):
            for ne_image in (n, n + 1, n + 2, n + 3):
                yield make_case(r, key, 0, 1, None, ne_image, None, None, tail, density=0.05)


def rnd(key):
    return lambda r: make_case(r, key)


TESTS = [FnTest(NAMES[k], ppc(k), host(k), edge=(lambda k=k: edges(k)), rnd=rnd(k)) for k in INSTANCES]
