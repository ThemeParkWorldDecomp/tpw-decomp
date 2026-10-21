"""Unit v5968, SaveCoasters, against libtpw_unit_SaveCoasters.dylib (tests/unit_SaveCoasters.cpp), main app
("SimTheme Park"), src/game/rides/TPCoaster.cpp:
  SaveCoasters 0x10044280

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian) for
the original, and in the host library's memory at the host structs' offsets (little-endian, native pointers) for the translation. The
callees outside the unit are fakes on both sides: LbFile_Write (records the file, the length and the first 36 bytes of the buffer; answers
the transferred count by a mode from the queue "Write"), SaveTrainsAndCars (records the labels), UDebug (records the text). The file swaps
(USwapInplace16/32/Float) run as the original on the Mac (they swap there) and are the host's real ones (nothing without
TPW_SWAP_FILE_WORDS: the host is little-endian like the file), so the two sides write the same bytes.

The world of a case: the list of coaster instances, each with a list of braces (their data blocks and heights), the three object table
counters. What is compared: the answer, the counters, every brace's count word (swapped in place and back), the event log.

No fused operations. The heights are the brace's collision list (mCollisions on the host, unit v5L20): each entry's first word is
written. Not tested: a list of more than 8 (CheckTrackToTrackCollisions stops at 8).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SaveCoasters"
MASK = 0xFFFFFFFF

FN = 0x10044280
SIZES = {FN: 2156}
OWN = set(SIZES)

UDEBUG = 0x10000E40
SAVE_TRAINS = 0x10043A18
WRITE_GLUE = 0x10238878
SWAPS = (0x10043DB8, 0x10043E44, 0x10021BB4)

ADDR = dict(gCoasterInstances=0x102B68B4, gObjectTableLowest=0x102B7304, gObjectTableFirstFree=0x102B7308, gObjectTableCount=0x102B730C)

# Mac offsets (the listing): name -> offset; "p" marks a pointer (4 bytes on the Mac, 8 on the host)
INST = ["mFlags", "mState", "mTypeID", "mEntranceID", "mObjectInstanceID", "mNext", "mX", "mZ", "mF2C", "mBlock3C", "mLastBrace", "mFF0",
        "mTrainCount", "mF140"]
INST_MAC = dict(mFlags=0, mState=8, mTypeID=0xC, mEntranceID=0x10, mObjectInstanceID=0x14, mNext=0x18, mX=0x1C, mZ=0x20, mF2C=0x2C,
                mBlock3C=0x3C, mLastBrace=0x54, mFF0=0xF0, mTrainCount=0xF4, mF140=0x140)
BRACE = ["mF00", "mFlags", "mPrev", "mWord70", "mFB0", "mFB4", "mFD8", "mFDC", "mAngle", "mFE8w", "mFF8", "mData"]
BRACE_MAC = dict(mF00=0, mFlags=8, mPrev=0x64, mWord70=0x70, mFB0=0xB0, mFB4=0xB4, mFD8=0xD8, mFDC=0xDC, mAngle=0xE0, mFE8w=0xE8, mFF8=0xF8,
                 mData=0x100)
DATA = ["mFlags", "mCellX", "mCellZ"]
DATA_MAC = dict(mFlags=4, mCellX=0xC, mCellZ=0x10)
MAC_SIZE = dict(inst=0x14C, brace=0x130, data=0x40)

HOST = {}          # filled by bind from tpw_sc_layout


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.update(SWAPS)


def stub_write(e):
    """LbFile_Write(TbError* sret, file, buffer, length, uint32_t* transferred)."""
    L = e.cl
    file_, buf, n, tp = e.r(4), e.r(5), e.r(6), e.r(7)
    data = e.read(buf, min(n, 36)).ljust(36, b"\0")
    words = struct.unpack(">9I", data)
    L.rec("Write", (L.lab(file_), n) + words)
    mode = L.ret("Write")
    t = {0: n, 1: 0, 2: n * 2, 3: n + 1, 4: n - 1}[mode] & MASK
    if tp:
        e.write(tp, struct.pack(">I", t))
    if e.r(3):
        e.write(e.r(3), struct.pack(">I", 0))


STUBS.internal(WRITE_GLUE, stub_write)
STUBS.internal(UDEBUG, CL.recorder("UDebug", "s"))
STUBS.internal(SAVE_TRAINS, CL.recorder("SaveTrainsAndCars", "LL", ret="i"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def off(self, struct_, name):
        return self.OFF[struct_][name]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psize = 4
    OFF = dict(inst=INST_MAC, brace=BRACE_MAC, data=DATA_MAC)

    def __init__(self, e, case):
        self.e = e
        self.case = case
        e.cl = None
        self.saved = None

    def begin(self, fixed):
        self.L = self.e.cl = CL.PpcLog(fixed, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def size(self, s):
        return MAC_SIZE[s]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, file_):
        r3, _, _ = T.call_ppc(self.e, FN, [file_ & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        self.OFF = HOST["off"]

    def begin(self, fixed):
        CL.host_begin(self.lib, fixed, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sc_addr(name.encode())

    def size(self, s):
        return HOST["size"][s]

    def alloc(self, n):
        return self.lib.tpw_sc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, file_):
        return self.lib.tpw_sc_save(file_) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sc_addr.restype, lib.tpw_sc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sc_alloc.restype, lib.tpw_sc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sc_save.restype, lib.tpw_sc_save.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 40)()
    n = lib.tpw_sc_layout(out)
    v = list(out)[:n]
    ni, nb, nd = len(INST), len(BRACE), len(DATA)
    assert n == ni + 1 + nb + 1 + nd + 1, n
    i = 0
    off = {}
    off["inst"] = dict(zip(INST, v[i:i + ni])); i += ni
    isz = v[i]; i += 1
    off["brace"] = dict(zip(BRACE, v[i:i + nb])); i += nb
    bsz = v[i]; i += 1
    off["data"] = dict(zip(DATA, v[i:i + nd])); i += nd
    dsz = v[i]
    HOST["off"] = off
    HOST["size"] = dict(inst=isz, brace=bsz, data=max(dsz, 0x40))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Instances (the list in order), each with its braces from the last brace backwards (mLastBrace, then mPrev)."""
    s.words(s.g("gObjectTableCount"), [case["count"]])
    s.words(s.g("gObjectTableFirstFree"), [case["first"]])
    s.words(s.g("gObjectTableLowest"), [case["lowest"]])
    s.file = s.alloc(16)
    s.insts = []
    s.braces = []
    nxt = 0
    labels = {s.file: 1}
    for k in reversed(range(len(case["insts"]))):
        d = case["insts"][k]
        a = s.alloc(s.size("inst"))
        o = lambda n: a + s.off("inst", n)
        for n in ("mFlags", "mState", "mTypeID", "mEntranceID", "mObjectInstanceID", "mX", "mZ", "mF2C", "mBlock3C", "mFF0", "mTrainCount"):
            s.words(o(n), [d[n]])
        s.words(o("mF140"), [d["mF140"]])
        s.ptr(o("mNext"), nxt)
        prev = 0
        for b in d["braces"]:                       # listed from the first visited; built from the end so that mPrev chains
            pass
        head = 0
        for b in reversed(d["braces"]):
            ba = s.alloc(s.size("brace"))
            bo = lambda n: ba + s.off("brace", n)
            da = s.alloc(s.size("data"))
            s.words(da + s.off("data", "mFlags"), [b["dflags"]])
            s.words(da + s.off("data", "mCellX"), [b["cx"]])
            s.words(da + s.off("data", "mCellZ"), [b["cz"]])
            s.words(bo("mF00"), [b["number"], b["w4"]])
            s.words(bo("mFlags"), [b["flags"]])
            s.ptr(bo("mPrev"), head)
            s.words(bo("mWord70"), [b["w70"]])
            s.words(bo("mFB0"), [len(b["heights"])])
            for i, hv in enumerate(b["heights"]):
                ha = s.alloc(4)
                s.words(ha, [hv])
                s.ptr(bo("mFB4") + i * s.psize, ha)
            s.words(bo("mFD8"), [b["fd8"]])
            s.words(bo("mFDC"), [b["fdc"]])
            s.words(bo("mAngle"), [b["angle"]])
            s.words(bo("mFE8w"), [b["fe8"]])
            s.words(bo("mFF8"), [b["ff8"]])
            s.ptr(bo("mData"), da)
            s.braces.append(ba)
            head = ba
        s.ptr(o("mLastBrace"), head)
        s.insts.append(a)
        labels[a] = 10 + k
        nxt = a
    s.ptr(s.g("gCoasterInstances"), nxt)
    s.labels = labels


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log(),
           "count": s.rd_word(s.g("gObjectTableCount")), "first": s.rd_word(s.g("gObjectTableFirstFree")),
           "lowest": s.rd_word(s.g("gObjectTableLowest"))}
    out["fb0"] = [s.rd_word(b + s.off("brace", "mFB0")) for b in s.braces]
    return out


def run(s, case):
    try:
        build(s, case)
        s.begin(s.labels if isinstance(s, Host) is False else None)
        return None
    finally:
        pass


def run_case(s, case):
    try:
        build(s, case)
        if isinstance(s, Host):
            s.begin({lab: addr for addr, lab in s.labels.items()})
        else:
            s.begin(s.labels)
        ans = s.call(s.file)
        return observe(s, case, ans)
    finally:
        s.done()


def ppc(e, case):
    return run_case(Ppc(e, case), case)


def host(lib, case):
    return run_case(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 1, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice(SPECIAL)
    return fb(r.uniform(-300, 300))


def rbits(r, mask):
    """A word with each bit of mask set at random (the rest clear), or all/none of them now and then."""
    k = r.random()
    if k < 0.1:
        return mask
    if k < 0.2:
        return 0
    return r.getrandbits(32) & mask


INST_FLAG_BITS = 0x20 | 0x40 | 0x80 | 0x100 | 0x200 | 0x400 | 0x800
BRACE_FLAG_BITS = 0x1 | 0x2 | 0x4 | 0x80 | 0x40 | 0x100 | 0x10000 | 0x20000 | 0x40000 | 0x80000 | 0x200000 | 0x400000


def brace(r, number=None, hidden=None, nh=None):
    nh = r.choice([0, 1, 2, 3, 5]) if nh is None else nh
    n = r.choice([2, 2, 0, 1, 3, 7]) if number is None else number
    h = r.choice([True, False]) if hidden is None else hidden
    return dict(number=n, w4=r.getrandbits(32) if r.random() < 0.7 else r.choice([0, 0xFF, 0x1FF, MASK]),
                flags=rbits(r, BRACE_FLAG_BITS) | (r.getrandbits(32) & ~BRACE_FLAG_BITS & 0x3F000000 if r.random() < 0.3 else 0),
                w70=r.getrandbits(32), heights=[r.getrandbits(32) for _ in range(nh)],
                fd8=rf(r), fdc=rf(r), angle=rf(r), fe8=rf(r), ff8=rf(r),
                dflags=(0x10 if h else 0) | (r.getrandbits(32) & ~0x10 if r.random() < 0.5 else 0),
                cx=r.getrandbits(32), cz=r.getrandbits(32))


def instance(r, nb=None):
    nb = r.choice([0, 1, 2, 3]) if nb is None else nb
    return dict(mFlags=rbits(r, INST_FLAG_BITS) | (r.getrandbits(32) & ~INST_FLAG_BITS if r.random() < 0.3 else 0),
                mState=r.getrandbits(32), mTypeID=r.getrandbits(32), mEntranceID=r.getrandbits(32), mObjectInstanceID=r.getrandbits(32),
                mX=r.getrandbits(32), mZ=r.getrandbits(32), mF2C=rf(r), mBlock3C=rbits(r, 3) | (r.getrandbits(32) & ~3 if r.random() < 0.3 else 0),
                mFF0=r.getrandbits(32), mTrainCount=r.getrandbits(32), mF140=r.getrandbits(32),
                braces=[brace(r) for _ in range(nb)])


def world(r, ni=None, fail=None, mode=None):
    ni = r.choice([0, 1, 1, 2, 3]) if ni is None else ni
    if fail is None:
        fail = r.randrange(0, 50) if r.random() < 0.4 else None
    if fail is None:
        q = [0]
    else:
        q = [0] * fail + [mode if mode is not None else r.choice([1, 2, 4])]
    return {"count": r.getrandbits(32), "first": r.getrandbits(32), "lowest": r.getrandbits(32),
            "insts": [instance(r) for _ in range(ni)], "queues": {"Write": q}}


def edges():
    r = random.Random(968)
    yield world(r, ni=0)
    yield world(r, ni=1, fail=None)
    for fail in range(0, 14):
        for mode in (1, 2, 4):
            yield world(r, ni=2, fail=fail, mode=mode)
    yield world(r, ni=1, fail=3, mode=3)          # one item too many by a unit: the quotient is still 1
    yield world(r, ni=2, fail=0, mode=3)
    # flags all set and all clear
    for allset in (True, False):
        w = world(r, ni=1, fail=None)
        i = w["insts"][0]
        i["mFlags"] = INST_FLAG_BITS if allset else 0
        i["mBlock3C"] = 3 if allset else 0
        i["braces"] = [brace(r, number=n, hidden=False, nh=h) for n, h in ((2, 5), (1, 0), (2, 0), (3, 2))]
        for b in i["braces"]:
            b["flags"] = BRACE_FLAG_BITS if allset else 0
        yield w
    # hidden braces (data flag 0x10) of kind 2 and of another kind
    w = world(r, ni=2, fail=None)
    for i in w["insts"]:
        i["braces"] = [brace(r, number=n, hidden=True, nh=3) for n in (2, 1)] + [brace(r, number=2, hidden=False, nh=2)]
    yield w
    w = world(r, ni=1, fail=None)
    w["insts"][0]["braces"] = [brace(r, number=2, hidden=False, nh=5) for _ in range(3)]
    yield w
    w = world(r, ni=1, fail=None)
    w["insts"][0]["braces"] = [brace(r, number=2, hidden=False, nh=8) for _ in range(2)]
    yield w
    for fail in range(0, 30, 2):
        w = world(r, ni=1, fail=fail, mode=1)
        w["insts"][0]["braces"] = [brace(r, number=2, hidden=False, nh=3) for _ in range(3)]
        yield w


def rnd(r):
    return world(r)


TESTS = [
    FnTest("SaveCoasters", ppc, host, edge=edges, rnd=rnd),
]
