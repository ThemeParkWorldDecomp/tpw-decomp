"""Unit w166, the ride scripts' save and rsse.cpp's swap helpers, against libtpw_unit_RideSave.dylib (tests/unit_RideSave.cpp), main app
("SimTheme Park"), src/rsse_save.cpp:
  RSSE_Save 0x100d45b0 and the eleven instances of the SamsUtilities swap templates USwapInplace32<int> 0x100d5324, <unsigned int> 0x100d5368,
  USwap32<RSSE_OBJECT*> 0x100d53b0, USwapInplace16<unsigned short> 0x100d5424, USwapInplace32<unsigned long> 0x100d546c,
  USwapInplace16<short> 0x100d54b4, USwap32<char*> 0x100d54f8, USwap32<RSSE_WALK*> 0x100d5554, USwap32<long*> 0x100d55c0,
  USwapInplace32<long> 0x100d561c, USwap32<void*> 0x100d5660.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian) for the
original, and in the host library's memory (little-endian, the arena) for the translation. The callees outside the unit are fakes on both sides that
log their arguments: LbFile_Write (the file, the length, whether a bytes-done pointer was given; the bytes are the chunks the test compares) and
the SamsUtilities copy, allocate and delete (the Mac's copy swaps the words, the host's does not; each block gets a label). The file is
little-endian: the Mac swaps what it writes, the host writes the same bytes by not swapping, so the chunks are equal byte for byte. The words
that are NOT swapped (the record's +0xae, +0xb8 to +0xbb and +0xe8 to +0xf3, a walk record's +0x1c, the +0x34 block and the name) are raw bytes on
both sides. The pointer words (the state's fifth, a record's links and pointers, an object's links) are addresses on the Mac and arena offsets
on the host: the chunks compare them as zero or not zero. The swap helpers are called on a value written natively on each side; the byte images
after the call are compared (the pointer ones answer the word: the Mac's register holds the swapped value, the host's the value).

Not tested: a negative table count (the byte count is the count shifted), a nonzero count with a NULL table (the Mac copies from address 0), a
script whose name pointer is NULL (strlen of 0), the thing id's swap (the host's build does not define TPW_SWAP_FILE_WORDS, so it is not called; the
Mac's Byteswap is run as the original and its effect is in the chunks), a big-endian host's swaps.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_RideSave"
MASK = 0xFFFFFFFF

FN = {
    "RSSE_Save": 0x100D45B0,
    "USwapInplace32Int": 0x100D5324, "USwapInplace32UInt": 0x100D5368, "USwap32ObjectPtr": 0x100D53B0, "USwapInplace16UShort": 0x100D5424,
    "USwapInplace32ULong": 0x100D546C, "USwapInplace16Short": 0x100D54B4, "USwap32CharPtr": 0x100D54F8, "USwap32WalkPtr": 0x100D5554,
    "USwap32LongPtr": 0x100D55C0, "USwapInplace32Long": 0x100D561C, "USwap32VoidPtr": 0x100D5660,
}
OWN = set(FN.values())
STATE = 0x102F7BEC
FILE = 0x1234
HOST_LAYOUT = [0xF4, 0x30, 0x34, 0x4C, 0x90, 0x1C, 0x20]

# The record's fields in the file's terms: w a swapped word, h a swapped half word, p a pointer word, rN N raw bytes.
RECORD = ([(0x00, "p"), (0x04, "p")] + [(o, "w") for o in range(0x08, 0x18, 4)] + [(o, "p") for o in range(0x18, 0x3C, 4)]
          + [(o, "w") for o in range(0x3C, 0x6C, 4)] + [(0x6C, "h"), (0x6E, "h")] + [(o, "w") for o in range(0x70, 0x80, 4)]
          + [(0x80, "w"), (0x84, "w"), (0x88, "h"), (0x8A, "h")] + [(o, "w") for o in range(0x8C, 0xA0, 4)]
          + [(0xA0, "w"), (0xA4, "w"), (0xA8, "w"), (0xAC, "h"), (0xAE, "r2"), (0xB0, "p"), (0xB4, "w"), (0xB8, "r4"), (0xBC, "w"),
             (0xC0, "h"), (0xC2, "h")] + [(o, "w") for o in range(0xC4, 0xDC, 4)]
          + [(0xDC, "h"), (0xDE, "h"), (0xE0, "w"), (0xE4, "h"), (0xE6, "h"), (0xE8, "r12")])
WALK = [(0, "h"), (2, "h"), (4, "h"), (6, "h"), (8, "w"), (0xC, "w"), (0x10, "w"), (0x14, "h"), (0x16, "h"), (0x18, "h"), (0x1A, "h"), (0x1C, "r4")]
OBJECT = [(0, "p"), (4, "p")] + [(o, "w") for o in range(8, 0x1C, 4)]
# The record's offsets of the counts (and of the tables' pointers).
COUNT = {"prog": 0x50, "stack": 0x54, "limbo": 0x58, "bounce": 0x64, "walk": 0x7C, "vars": 0x8C, "block": 0x90, "words": 0x4C}
PTR = {"prog": 0x18, "vars": 0x1C, "stack": 0x20, "limbo": 0x24, "bounce": 0x28, "walk": 0x2C, "words": 0x30, "block": 0x34, "name": 0x38, "objects": 0xB0}
POINTER_OFFSETS = [0, 4] + list(range(0x18, 0x3C, 4)) + [0xB0]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_write(e):
    # r3: the TbError's place, r4: the file, r5: the buffer, r6: the length, r7: bytes done.
    L, buffer, length = e.cl, e.r(5), e.r(6)
    L.rec("LbFile_Write", (e.r(4), length, 1 if e.r(7) else 0))
    L.chunks.append(e.read(buffer, length) if length else b"")
    e.write(e.r(3), bytes(4))


def swap_words(data):
    return b"".join(data[i:i + 4][::-1] for i in range(0, len(data) - len(data) % 4, 4))


def stub_swap_copy(e):
    L = e.cl
    words, count = e.r(3), T.s32(e.r(4))
    data = e.read(words, 4 * count) if count > 0 else b""
    a = e.alloc(max(4 * count, 4), swap_words(data) if count > 0 else bytes(4))
    L.rec("USwapCopy32", (count, e.u32(words) if count > 0 else 0, L.newlab(a)))
    e.set_r(3, a)


def stub_swap_delete(e):
    e.cl.rec("USwapDelete", (e.cl.lab(e.r(3)),))


def stub_swap_allocate(e):
    L, size = e.cl, e.r(3)
    a = e.alloc(max(size, 4), b"\xa5" * max(size, 4))
    L.rec("USwapAllocate", (size, L.newlab(a)))
    e.set_r(3, a)


STUBS.imp("LbFile_Write__FPvPCvUlPUl", stub_write)
STUBS.imp("_USwapCopy32__13SamsUtilitiesFPUll", stub_swap_copy)
STUBS.imp("_USwapDelete__13SamsUtilitiesFPv", stub_swap_delete)
STUBS.imp("_USwapAllocate__13SamsUtilitiesFUl", stub_swap_allocate)


def setup(runner):
    T.install_std_stubs(runner.emu)       # strlen


# ---------------------------------------------------------------------------------------------------------------- the two sides

class Side:
    """What the shared code needs from either memory."""

    def put(self, a, kind, value):
        """One field at `a`: a swapped word or pointer word, a swapped half word or raw bytes, in this side's order."""
        if kind in ("w", "p"):
            self.write(a, struct.pack(self.fmt, value & MASK))
        elif kind == "h":
            self.write(a, struct.pack(self.fmt[0] + "H", value & 0xFFFF))
        else:
            self.write(a, bytes(value))

    def get(self, a, kind):
        if kind in ("w", "p"):
            return struct.unpack(self.fmt, self.read(a, 4))[0]
        if kind == "h":
            return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]
        return self.read(a, int(kind[1:]))

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, {})
        self.L.chunks = []
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(max(n, 4))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptrword(self, a):
        return a

    def state(self, init, f04, f08, count, head):
        self.words(STATE, [init, f04, f08, count, self.ptrword(head)])

    def call_save(self):
        r3, _, _ = T.call_ppc(self.e, FN["RSSE_Save"], [FILE])
        return r3 & MASK

    def chunks(self):
        return list(self.L.chunks)

    def log(self):
        return self.L.log


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_rv_reset()
        CL.host_begin(lib, {}, {})

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_rv_alloc(max(n, 4))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptrword(self, a):
        return (a - self.lib.tpw_rv_base()) & MASK if a else 0

    def state(self, init, f04, f08, count, head):
        self.lib.tpw_rv_state(T.s32(init), T.s32(f04), T.s32(f08), T.s32(count), head)

    def call_save(self):
        return self.lib.tpw_rv_RSSE_Save(FILE) & MASK

    def chunks(self):
        return [C.string_at(self.lib.tpw_rv_out_data(i), self.lib.tpw_rv_out_length(i)) for i in range(self.lib.tpw_rv_out_count())]

    def log(self):
        return CL.host_end(self.lib)


SWAP_CALLS = {
    "USwapInplace32Int": (4, "i"), "USwapInplace32UInt": (4, "i"), "USwapInplace32ULong": (4, "i"), "USwapInplace32Long": (4, "i"),
    "USwapInplace16UShort": (2, "i"), "USwapInplace16Short": (2, "i"),
    "USwap32ObjectPtr": (4, "p"), "USwap32CharPtr": (4, "p"), "USwap32WalkPtr": (4, "p"), "USwap32LongPtr": (4, "p"), "USwap32VoidPtr": (4, "p"),
}


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_rv_alloc.restype, lib.tpw_rv_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rv_base.restype = C.c_uint64
    lib.tpw_rv_state.argtypes = [C.c_int32] * 4 + [C.c_uint64]
    lib.tpw_rv_out_count.restype = C.c_int32
    lib.tpw_rv_out_length.restype, lib.tpw_rv_out_length.argtypes = C.c_int32, [C.c_int32]
    lib.tpw_rv_out_data.restype, lib.tpw_rv_out_data.argtypes = C.c_void_p, [C.c_int32]
    lib.tpw_rv_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rv_RSSE_Save.restype, lib.tpw_rv_RSSE_Save.argtypes = C.c_int32, [C.c_uint64]
    for name, (width, kind) in SWAP_CALLS.items():
        f = getattr(lib, "tpw_rv_" + name)
        if kind == "p":
            f.restype, f.argtypes = C.c_uint32, [C.c_uint32]
        else:
            f.restype, f.argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rv_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def write_fields(s, a, spec, values, pointers):
    """A record's fields from the case's values (by offset); the pointer words from `pointers` (offset -> address)."""
    for off, kind in spec:
        if kind == "p":
            s.put(a + off, "w", s.ptrword(pointers.get(off, 0)))
        else:
            s.put(a + off, kind, values[off])


def build(s, case):
    """Writes the case's state and scripts through Side (the same code for both memories); the offsets are the Mac's."""
    records = []
    for sc in case["scripts"]:
        records.append(s.alloc(0xF4))
    for i, sc in enumerate(case["scripts"]):
        ptr = {0: records[i + 1] if i + 1 < len(records) else 0, 4: records[i - 1] if i else 0}
        for name in ("prog", "stack", "vars", "limbo", "bounce", "words"):
            ws = sc[name]
            ptr[PTR[name]] = 0
            if ws or sc["dummy"]:
                a = s.alloc(4 * len(ws))
                s.words(a, ws)
                ptr[PTR[name]] = a
        walk = sc["walk"]
        if walk or sc["dummy"]:
            a = s.alloc(0x20 * len(walk))
            for k, vals in enumerate(walk):
                write_fields(s, a + 0x20 * k, WALK, vals, {})
            ptr[PTR["walk"]] = a
        ptr[PTR["block"]] = 0
        if sc["block"] or sc["dummy"]:
            a = s.alloc(len(sc["block"]))
            s.write(a, sc["block"])
            ptr[PTR["block"]] = a
        a = s.alloc(len(sc["name"]) + 1)
        s.write(a, sc["name"] + b"\0")
        ptr[PTR["name"]] = a
        objs = [s.alloc(0x1C) for _ in sc["objects"]]
        for k, vals in enumerate(sc["objects"]):
            write_fields(s, objs[k], OBJECT, vals, {0: objs[k + 1] if k + 1 < len(objs) else 0, 4: objs[k - 1] if k else 0})
        ptr[PTR["objects"]] = objs[0] if objs else 0
        write_fields(s, records[i], RECORD, sc["rec"], ptr)
    s.state(case["init"], case["f04"], case["f08"], case["count"], records[0] if records else 0)
    s.records = records


def mask_chunks(chunks):
    """The chunks with the pointer words reduced to zero or one (an address on the Mac, an arena offset on the host)."""
    out = []
    for i, chunk in enumerate(chunks):
        c = bytearray(chunk)
        offsets = []
        if i == 2:
            offsets = [16]
        elif len(c) == 0xF4:
            offsets = POINTER_OFFSETS
        elif len(c) == 0x1C:
            offsets = [0, 4]
        for o in offsets:
            c[o:o + 4] = b"\0\0\0\1" if any(c[o:o + 4]) else bytes(4)
        out.append(bytes(c))
    return out


def observe(s, case, answer):
    """The answer, the chunks, the call log and the records read back (the save must not change them)."""
    out = {"answer": answer, "chunks": mask_chunks(s.chunks()), "log": s.log()}
    recs = []
    for a in s.records:
        recs.append([s.get(a + off, kind) if kind != "p" else (1 if s.get(a + off, "w") else 0) for off, kind in RECORD])
    out["records"] = recs
    return out


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call_save())
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rw(r):
    """A random 32-bit word, often small or an edge."""
    k = r.random()
    if k < 0.25:
        return r.randrange(0, 40)
    if k < 0.35:
        return r.choice([MASK, 0x80000000, 0x7FFFFFFF, 0x80000001, 0x12345678])
    return r.getrandbits(32)


def rfields(r, spec):
    vals = {}
    for off, kind in spec:
        if kind == "w":
            vals[off] = rw(r)
        elif kind == "h":
            vals[off] = r.choice([0, 0xFFFF, 0x8000, 0x7FFF, r.getrandbits(16), r.getrandbits(16)])
        elif kind[0] == "r":
            vals[off] = bytes(r.getrandbits(8) for _ in range(int(kind[1:])))
    return vals


def script_case(r, counts=None):
    """A script: the record's fields random, then the counts and the tables (each 0 to 6 entries; counts overrides by table name)."""
    def n():
        return r.choice([0, 0, 1, 2, 3, 4, 5, 6])
    c = {k: n() for k in ("prog", "stack", "vars", "limbo", "bounce", "walk", "words", "objects")}
    c["block"] = r.choice([0, 0, 1, 3, 4, 7, 12, 20])
    c["name"] = r.choice([0, 1, 2, 5, 10, 20])
    c.update(counts or {})
    rec = rfields(r, RECORD)
    sc = {
        "prog": [rw(r) for _ in range(c["prog"])], "stack": [rw(r) for _ in range(c["stack"])], "vars": [rw(r) for _ in range(c["vars"])],
        "words": [rw(r) for _ in range(c["words"])], "limbo": [rw(r) for _ in range(2 * c["limbo"])],
        "bounce": [rw(r) for _ in range(4 * c["bounce"])], "walk": [rfields(r, WALK) for _ in range(c["walk"])],
        "block": bytes(r.getrandbits(8) for _ in range(c["block"])), "name": bytes(r.randrange(1, 256) for _ in range(c["name"])),
        "objects": [rfields(r, OBJECT) for _ in range(c["objects"])], "dummy": r.random() < 0.3, "rec": rec,
    }
    for name, off in COUNT.items():
        rec[off] = {"prog": c["prog"], "stack": c["stack"], "vars": c["vars"], "limbo": c["limbo"], "bounce": c["bounce"], "walk": c["walk"],
                    "words": c["words"], "block": c["block"]}[name]
    return sc


def save_case(r, init=None, n=None, counts=None):
    init = init if init is not None else r.choice([1, 1, 1, 0, 2, MASK, 0x80000000])
    n = n if n is not None else r.choice([0, 1, 1, 2, 3, 4])
    return {"init": init, "f04": rw(r), "f08": rw(r), "count": rw(r), "scripts": [script_case(r, counts) for _ in range(n)]}


def save_rnd(r):
    return save_case(r)


def save_edges():
    import random
    r = random.Random(7)
    zero = {k: 0 for k in ("prog", "stack", "vars", "limbo", "bounce", "walk", "words", "objects", "block", "name")}
    full = {k: 6 for k in ("prog", "stack", "vars", "limbo", "bounce", "walk", "words", "objects")}
    full.update({"block": 20, "name": 20})
    cases = [save_case(r, 0, 0), save_case(r, 0, 3), save_case(r, 1, 0), save_case(r, MASK, 0)]
    cases += [save_case(r, 1, 1, zero), save_case(r, 1, 2, zero), save_case(r, 1, 1, full), save_case(r, 1, 4, full)]
    for name in ("prog", "stack", "vars", "limbo", "bounce", "walk", "words", "objects", "block"):
        only = dict(zero)
        only[name] = 3 if name != "block" else 5
        cases.append(save_case(r, 1, 1, only))
        only = dict(full)
        only[name] = 0
        cases.append(save_case(r, 1, 2, only))
    return cases


# ---- the swap helpers

SWAP_EDGES = [0, 1, MASK, 0x80000000, 0x7FFFFFFF, 0x7FFF, 0x8000, 0xFFFF, 0x12345678, 0x00FF00FF]


def swap_case(r, fn):
    width = SWAP_CALLS[fn][0]
    v = r.choice(SWAP_EDGES) if r.random() < 0.2 else r.getrandbits(8 * width)
    return {"fn": fn, "value": v & ((1 << (8 * width)) - 1)}


def swap_edges(fn):
    width = SWAP_CALLS[fn][0]
    return lambda: [{"fn": fn, "value": v & ((1 << (8 * width)) - 1)} for v in SWAP_EDGES]


def run_swap(s, case):
    try:
        fn, v = case["fn"], case["value"]
        width, kind = SWAP_CALLS[fn]
        out = {}
        if kind == "p":
            if isinstance(s, Ppc):
                r3, _, _ = T.call_ppc(s.e, FN[fn], [v])
                out["image"] = struct.pack(">I", r3 & MASK)
            else:
                out["image"] = struct.pack("<I", getattr(s.lib, "tpw_rv_" + fn)(v) & MASK)
        else:
            a = s.alloc(16)
            s.write(a, bytes([0xEE] * 16))
            s.put(a, "w" if width == 4 else "h", v)
            if isinstance(s, Ppc):
                T.call_ppc(s.e, FN[fn], [a])
            else:
                getattr(s.lib, "tpw_rv_" + fn)(a)
            out["image"] = s.read(a, 16)
        return out
    finally:
        s.done()


def ppc_swap(e, case):
    return run_swap(Ppc(e, case), case)


def host_swap(lib, case):
    return run_swap(Host(lib, case), case)


TESTS = [FnTest("RSSE_Save", ppc, host, edge=save_edges, rnd=save_rnd)] + [
    FnTest(n, ppc_swap, host_swap, edge=swap_edges(n), rnd=(lambda n: lambda r: swap_case(r, n))(n)) for n in SWAP_CALLS]
