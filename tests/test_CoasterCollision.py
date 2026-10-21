"""Unit v5L20, the coaster collision system, against libtpw_unit_CoasterCollision.dylib (tests/unit_CoasterCollision.cpp), main app
("SimTheme Park"), src/game/rides/CoasterCollision.cpp, src/game/rides/CoasterPylon.cpp:
  GetPylonAtCell 0x1003f954, GetTrackBounds 0x10047634, RecalcTrackBounds 0x10047818, GenerateSamplesInBoundArea 0x100478b8,
  DoProperChunkCollision 0x10047c64, DotProduct3D 0x100484b0, CheckTrackToTrackCollisions 0x10048510,
  RemoveAllCollisionsFromBrace 0x10048a64, CheckTrackToAllTracksCollisions 0x10048cd0, CheckAllTrackToTrackCollisions 0x10048e00,
  DoCollisionChecking 0x10048f64, RemoveBraceFromCollisionSystem 0x10048fc4, FlagRecalcAllCoasterCollisions 0x10049078

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's offsets (big-endian)
for the original, and in the host library's memory at the host structs' offsets (native pointers) for the translation. The world: coaster
types (the cross section's pairs), instances with their samples, braces (flags, sample, pylon data with a control's radius, box, collision
list), pylon records, the height field's row stride, the engine's sine table (1024 entries), the collision globals (the two chunk blocks,
the rings with stale contents, the corners, samples and types). The tracks are built to cross (two legs at right angles) so that the
cross section test answers both ways.

The callees: NormaliseVector runs as the original on the PowerPC (with the C library's sqrt) and as a copy of the host's translation in the
library (not recorded); MReleaseAllocF records the size and the flags (zeroed block, labelled) and MReleaseFreeF the block's label.
acos and sqrt are the C library's on both sides.

What is compared: the answer, every brace's flags, count, 8 list slots (as labels), box; every instance's reference count; the chunk
blocks; the rings and the corner, sample and type globals (as indices); the event log. NaNs are compared as one value.

Not tested: rings of more than 6 vertices (the Mac's 0x90-byte rings run into the next globals), a list count above 8 (the Mac's list is
8 words), chunk blocks smaller than a section's samples (the original writes past them).
"""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CoasterCollision"
MASK = 0xFFFFFFFF

FNS = dict(GetPylonAtCell=(0x1003F954, 124), GetTrackBounds=(0x10047634, 372), RecalcTrackBounds=(0x10047818, 96),
           GenerateSamplesInBoundArea=(0x100478B8, 800), DoProperChunkCollision=(0x10047C64, 2052), DotProduct3D=(0x100484B0, 40),
           CheckTrackToTrackCollisions=(0x10048510, 1196), RemoveAllCollisionsFromBrace=(0x10048A64, 528),
           CheckTrackToAllTracksCollisions=(0x10048CD0, 208), CheckAllTrackToTrackCollisions=(0x10048E00, 280),
           DoCollisionChecking=(0x10048F64, 32), RemoveBraceFromCollisionSystem=(0x10048FC4, 84),
           FlagRecalcAllCoasterCollisions=(0x10049078, 92))
SIZES = {a: n for a, n in FNS.values()}
OWN = set(SIZES)
NORMALISE = 0x1002AD34
ALLOC = 0x10064BD4
FREE = 0x10064C54

ADDR = dict(gCollisionChunksA=0x102B7460, gCollisionChunksB=0x102B745C, gCollisionBlockA=0x102B7468, gCollisionBlockB=0x102B7464,
            gCollisionSampleA=0x102B7330, gCollisionSampleB=0x102B732C, gCollisionTypeA=0x102B7458, gCollisionTypeB=0x102B7454,
            gCollisionRingB=0x102B7334, gCollisionRingA=0x102B73C4, gCollisionCorner0=0x102B7328, gCollisionCorner1=0x102B7324,
            gCollisionCorner2=0x102B7320, gCollisionCorner3=0x102B731C, gCoasterInstances=0x102B68B4, gHeightfieldHeader=0x102C6C7C,
            gTPGlobals=0x103B74C8)
CORNERS = ("gCollisionCorner0", "gCollisionCorner1", "gCollisionCorner2", "gCollisionCorner3")

# name lists of the host layout (tpw_ccol_layout), in its order; "#" is a sizeof
LAYOUT = [("inst", ["mType", "mNext", "mPylons", "mLastBrace", "mSamples", "mSegmentSpan", "mCollisionRefs", "#"]),
          ("brace", ["mNumber", "mFlags", "mPrev", "mNext", "mFB0", "mCollisions", "mCollisionBounds", "mSample", "mData", "#"]),
          ("data", ["mFlags", "mControl", "#"]),
          ("sample", ["mPos", "mYaw", "#"]),
          ("type", ["mPairs", "mPairCount", "#"]),
          ("pylon", ["mFlags", "mIndex", "mNext", "#"]),
          ("header", ["#"]),
          ("hfield", ["mF18", "#"]),
          ("control", ["#"])]
MAC = dict(inst=dict(mType=4, mNext=0x18, mPylons=0x4C, mLastBrace=0x54, mSamples=0x9C, mSegmentSpan=0xA0, mCollisionRefs=0x140),
           brace=dict(mNumber=0, mFlags=8, mPrev=0x64, mNext=0x68, mFB0=0xB0, mCollisions=0xB4, mCollisionBounds=0x98, mSample=0xFC,
                      mData=0x100),
           data=dict(mFlags=4, mControl=0x18), sample=dict(mPos=4, mYaw=0x10), type=dict(mPairs=0x9C, mPairCount=0xA0),
           pylon=dict(mFlags=4, mIndex=8, mNext=0x28), header=dict(mHField=0x6C), hfield=dict(mF18=0x18), control={})
MAC_SIZE = dict(inst=0x14C, brace=0x130, data=0x40, sample=0x38, type=0xA8, pylon=0x48, header=0x70, hfield=0x30, control=0x128)
HOST = {}
SIN_N = 1024
CHUNK = 0x1C


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bf(w):
    return struct.unpack(">f", struct.pack(">I", w & MASK))[0]


def canon(w):
    return 0x7FC00000 if (w & 0x7F800000) == 0x7F800000 and (w & 0x7FFFFF) else w


SIN_TABLE = [fb(math.sin(2 * math.pi * i / SIN_N)) for i in range(SIN_N)]
SIN_SCALE = fb(SIN_N / (2 * math.pi))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.real.add(NORMALISE)


def stub_alloc(e):
    L = e.cl
    size, flags = e.r(3), e.r(4)
    L.rec("MReleaseAllocF", (size, flags))
    a = e.alloc(max(size, 4))
    e.write(a, bytes(max(size, 4)))
    L.newlab(a)
    e.set_r(3, a)


def stub_free(e):
    L = e.cl
    L.rec("MReleaseFreeF", (L.lab(e.r(3)),))
    e.set_r(3, 0)


STUBS.internal(ALLOC, stub_alloc)
STUBS.internal(FREE, stub_free)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.PW))[0]

    def zalloc(self, n):
        a = self.alloc(n)
        self.write(a, bytes(n))
        return a


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    PW = 4
    OFF = MAC

    def __init__(self, e, case):
        self.e = e
        self.case = case
        e.cl = None
        self.saved = None

    def begin(self, fixed):
        self.L = self.e.cl = CL.PpcLog(fixed, {})
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
            CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def size(self, s):
        return MAC_SIZE[s]

    def alloc(self, n):
        return self.e.alloc(max(n, 4))

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def set_hfield(self, header, field):
        self.ptr(header + MAC["header"]["mHField"], field)

    def type_word(self, t):
        return t + MAC["type"]["mPairs"]       # the Mac keeps the address of the type's pairs and count

    def type_of(self, v):
        return v - MAC["type"]["mPairs"] if v else 0

    def log(self):
        return self.L.log

    def call(self, fn, args):
        r3, _, f1 = T.call_ppc(self.e, FNS[fn][0], [a & MASK for a in args])
        return fb(f1) if fn == "DotProduct3D" else r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    PW = 8

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)
        self.OFF = HOST["off"]

    def begin(self, fixed):
        CL.host_begin(self.lib, fixed, {})

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ccol_addr(name.encode())

    def size(self, s):
        return max(HOST["size"][s], MAC_SIZE[s])

    def alloc(self, n):
        return self.lib.tpw_ccol_alloc(max(n, 4))

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def set_hfield(self, header, field):
        self.lib.tpw_ccol_set_hfield(header, field)

    def type_word(self, t):
        return t

    def type_of(self, v):
        return v

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        lib = self.lib
        f = dict(GetPylonAtCell=lib.tpw_ccol_pylon_at, GetTrackBounds=lib.tpw_ccol_bounds, RecalcTrackBounds=lib.tpw_ccol_recalc_bounds,
                 GenerateSamplesInBoundArea=lib.tpw_ccol_generate, DoProperChunkCollision=lib.tpw_ccol_proper,
                 DotProduct3D=lib.tpw_ccol_dot, CheckTrackToTrackCollisions=lib.tpw_ccol_track_to_track,
                 RemoveAllCollisionsFromBrace=lib.tpw_ccol_remove_all, CheckTrackToAllTracksCollisions=lib.tpw_ccol_to_all,
                 CheckAllTrackToTrackCollisions=lib.tpw_ccol_check_all, DoCollisionChecking=lib.tpw_ccol_do_checking,
                 RemoveBraceFromCollisionSystem=lib.tpw_ccol_remove_brace, FlagRecalcAllCoasterCollisions=lib.tpw_ccol_flag_recalc)[fn]
        if fn == "GetPylonAtCell":
            return f(args[0] & MASK, args[1] & MASK, args[2])
        r = f(*args)
        return 0 if r is None else r & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, u32 = C.c_uint64, C.c_uint32
    lib.tpw_ccol_addr.restype, lib.tpw_ccol_addr.argtypes = u64, [C.c_char_p]
    lib.tpw_ccol_alloc.restype, lib.tpw_ccol_alloc.argtypes = u64, [u32]
    lib.tpw_ccol_set_hfield.restype, lib.tpw_ccol_set_hfield.argtypes = None, [u64, u64]
    lib.tpw_ccol_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ccol_pylon_at.restype, lib.tpw_ccol_pylon_at.argtypes = u64, [u32, u32, u64]
    for name, n in (("bounds", 4), ("recalc_bounds", 1), ("generate", 6), ("track_to_track", 6), ("remove_all", 2), ("to_all", 2),
                    ("check_all", 1), ("do_checking", 1), ("remove_brace", 2), ("flag_recalc", 1)):
        f = getattr(lib, "tpw_ccol_" + name)
        f.restype, f.argtypes = None, [u64] * n
    lib.tpw_ccol_proper.restype, lib.tpw_ccol_proper.argtypes = C.c_int32, [u64, u64]
    lib.tpw_ccol_dot.restype, lib.tpw_ccol_dot.argtypes = u32, [u64, u64]
    out = (C.c_int64 * 64)()
    n = lib.tpw_ccol_layout(out)
    v = list(out)[:n]
    assert n == sum(len(names) for _, names in LAYOUT), n
    off, size, i = {}, {}, 0
    for s, names in LAYOUT:
        off[s] = {}
        for nm in names:
            if nm == "#":
                size[s] = v[i]
            else:
                off[s][nm] = v[i]
            i += 1
    off["header"] = {}
    HOST["off"] = off
    HOST["size"] = size
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, w):
    o = s.OFF
    lab = {}                 # address -> label
    s.lab = lab
    # the sine table
    tg = s.g("gTPGlobals")
    s.words(tg, [SIN_N, SIN_N - 1, SIN_SCALE])
    s.words(tg + 0xC, SIN_TABLE)
    # the height field
    header = s.zalloc(s.size("header"))
    field = s.zalloc(s.size("hfield"))
    s.words(field + o["hfield"]["mF18"], [w["stride"], 0])
    s.set_hfield(header, field)
    s.ptr(s.g("gHeightfieldHeader"), header)
    # types
    s.types = []
    for k, t in enumerate(w["types"]):
        a = s.zalloc(s.size("type"))
        pairs = s.zalloc(max(8 * len(t["pairs"]) // 2, 8))
        s.words(pairs, t["pairs"])
        s.ptr(a + o["type"]["mPairs"], pairs)
        s.words(a + o["type"]["mPairCount"], [t["count"]])
        s.types.append(a)
        lab[a] = 50 + k
    # instances (built from the last so that mNext chains in order)
    s.insts = [None] * len(w["insts"])
    s.braces = {}
    s.samples = []           # (instance index, base, count)
    nxt = 0
    ss = s.size("sample")
    for k in reversed(range(len(w["insts"]))):
        d = w["insts"][k]
        a = s.zalloc(s.size("inst"))
        io = o["inst"]
        s.ptr(a + io["mType"], s.types[d["type"]])
        s.ptr(a + io["mNext"], nxt)
        s.words(a + io["mSegmentSpan"], [d["span"]])
        s.words(a + io["mCollisionRefs"], [d["refs"]])
        smp = s.zalloc(ss * max(len(d["samples"]), 1))
        for i, (x, y, z, yaw, pitch, bank) in enumerate(d["samples"]):
            s.words(smp + i * ss + o["sample"]["mPos"], [x, y, z])
            s.words(smp + i * ss + o["sample"]["mYaw"], [yaw, pitch, bank])
        s.ptr(a + io["mSamples"], smp)
        s.samples.append((k, smp, len(d["samples"])))
        # pylon records
        head = 0
        for p in reversed(d["pylons"]):
            pa = s.zalloc(s.size("pylon"))
            s.words(pa + o["pylon"]["mFlags"], [p["flags"]])
            s.words(pa + o["pylon"]["mIndex"], [p["index"]])
            s.ptr(pa + o["pylon"]["mNext"], head)
            head = pa
            lab[pa] = 1000 + 100 * k + len(lab) % 100
        s.ptr(a + io["mPylons"], head)
        # pylon data
        datas = []
        for dd in d["datas"]:
            da = s.zalloc(s.size("data"))
            s.words(da + o["data"]["mFlags"], [dd["flags"]])
            if dd["radius"] is not None:
                ctl = s.zalloc(s.size("control"))
                s.words(ctl + 0x58, [dd["radius"]])
                s.ptr(da + o["data"]["mControl"], ctl)
            datas.append(da)
        # braces: index 0 is mLastBrace, mPrev runs to the higher indices
        bl = [s.zalloc(s.size("brace")) for _ in d["braces"]]
        for j, b in enumerate(d["braces"]):
            ba = bl[j]
            s.braces[(k, j)] = ba
            lab[ba] = 100 + 16 * k + j
        for j, b in enumerate(d["braces"]):
            ba, bo = bl[j], o["brace"]
            s.words(ba + bo["mNumber"], [b["number"]])
            s.words(ba + bo["mFlags"], [b["flags"]])
            s.ptr(ba + bo["mPrev"], bl[j + 1] if j + 1 < len(bl) else 0)
            s.ptr(ba + bo["mNext"], bl[j - 1] if j > 0 else 0)
            s.words(ba + bo["mFB0"], [b["fb0"]])
            s.words(ba + bo["mCollisionBounds"], b["bounds"])
            s.words(ba + bo["mSample"], [b["sample"]])
            s.ptr(ba + bo["mData"], datas[b["data"]])
        s.ptr(a + io["mLastBrace"], bl[0] if bl else 0)
        s.insts[k] = a
        lab[a] = 10 + k
        nxt = a
    # the lists (after every brace exists): (instance, brace) references or 0
    for k, d in enumerate(w["insts"]):
        for j, b in enumerate(d["braces"]):
            ba = s.braces[(k, j)]
            for i, ref in enumerate(b["list"]):
                s.ptr(ba + s.OFF["brace"]["mCollisions"] + i * s.PW, s.braces[tuple(ref)] if ref else 0)
    s.ptr(s.g("gCoasterInstances"), nxt if w["linked"] else 0)
    # the collision globals
    s.chunks = []
    for nm in ("gCollisionChunksA", "gCollisionChunksB"):
        a = s.zalloc(CHUNK * w["chunks"]) if w["chunks"] else 0
        s.ptr(s.g(nm), a)
        s.chunks.append(a)
    for nm in ("gCollisionBlockA", "gCollisionBlockB", "gCollisionSampleA", "gCollisionSampleB") + CORNERS:
        s.ptr(s.g(nm), 0)
    s.ptr(s.g("gCollisionTypeA"), 0)
    s.ptr(s.g("gCollisionTypeB"), 0)
    s.words(s.g("gCollisionRingB"), w["rings"][:36])
    s.words(s.g("gCollisionRingA"), w["rings"][36:])
    g = w.get("glob")
    if g:
        ia, i0 = g["sampleA"]
        ib, j0 = g["sampleB"]
        s.ptr(s.g("gCollisionSampleA"), s.samples[len(w["insts"]) - 1 - ia][1] + i0 * ss)
        s.ptr(s.g("gCollisionSampleB"), s.samples[len(w["insts"]) - 1 - ib][1] + j0 * ss)
        s.ptr(s.g("gCollisionTypeA"), s.type_word(s.types[g["typeA"]]))
        s.ptr(s.g("gCollisionTypeB"), s.type_word(s.types[g["typeB"]]))
    for c in s.chunks:
        if c:
            for i, ch in enumerate(w["chunk_init"][:w["chunks"]]):
                s.words(c + i * CHUNK, ch)
    lab[s.chunks[0]] = 3
    lab[s.chunks[1]] = 4
    lab.pop(0, None)


def rd_floats(s, a, n):
    return [canon(v) for v in s.rd_words(a, n)]


def observe(s, w, answer, extra):
    o = s.OFF
    out = {"answer": answer, "log": s.log()}
    out.update(extra)
    for (k, j), ba in sorted(s.braces.items()):
        bo = o["brace"]
        out["b%d.%d" % (k, j)] = dict(flags=s.rd_word(ba + bo["mFlags"]), fb0=s.rd_word(ba + bo["mFB0"]),
                                      list=[s.lab.get(s.rd_ptr(ba + bo["mCollisions"] + i * s.PW), "raw")
                                            for i in range(8)],
                                      bounds=rd_floats(s, ba + bo["mCollisionBounds"], 6))
    for k, a in enumerate(s.insts):
        out["refs%d" % k] = s.rd_word(a + o["inst"]["mCollisionRefs"])
    for n, c in enumerate(s.chunks):
        if c:
            rows = []
            for i in range(w["chunks"]):
                v = s.rd_words(c + i * CHUNK, 7)
                rows.append([v[0]] + [canon(x) for x in v[1:]])
            out["chunks%d" % n] = rows
    out["rings"] = rd_floats(s, s.g("gCollisionRingB"), 36) + rd_floats(s, s.g("gCollisionRingA"), 36)
    rb = s.g("gCollisionRingB")
    rings = {rb + 12 * i: i for i in range(12)}
    rings.update({s.g("gCollisionRingA") + 12 * i: 12 + i for i in range(12)})
    out["corners"] = [rings.get(s.rd_ptr(s.g(nm)), "raw") if s.rd_ptr(s.g(nm)) else -1 for nm in CORNERS]
    ss = s.size("sample")

    def sample_of(v):
        if v == 0:
            return -1
        for k, base, n in s.samples:
            if base <= v < base + ss * max(n, 1) and (v - base) % ss == 0:
                return (k, (v - base) // ss)
        return "raw"
    out["samples"] = [sample_of(s.rd_ptr(s.g("gCollisionSampleA"))), sample_of(s.rd_ptr(s.g("gCollisionSampleB")))]
    out["types"] = [s.lab.get(s.type_of(s.rd_ptr(s.g(nm))), -1) for nm in ("gCollisionTypeA", "gCollisionTypeB")]
    return out


def run_case(s, case):
    w = case
    try:
        build(s, w)
        fn = w["fn"]
        args = []
        extra = {}
        box = out = 0
        a = w["args"]
        if fn == "GetPylonAtCell":
            out = s.zalloc(4) if a["out"] else 0
            if out:
                s.words(out, [0xDEADBEEF])
            args = [a["x"], a["z"], out]
        elif fn in ("GetTrackBounds", "GenerateSamplesInBoundArea"):
            box = s.zalloc(24)
            s.words(box, a["box"])
            args = [s.insts[a["inst"]], s.braces[(a["inst"], a["a"])], s.braces[(a["inst"], a["b"])], box]
            if fn == "GenerateSamplesInBoundArea":
                out = s.zalloc(4)
                s.words(out, [0xDEADBEEF])
                args += [s.chunks[a["which"]], out]
        elif fn == "DoProperChunkCollision":
            args = [s.chunks[0], s.chunks[1]]
        elif fn == "DotProduct3D":
            p = s.zalloc(12)
            q = s.zalloc(12)
            s.words(p, a["p"])
            s.words(q, a["q"])
            args = [p, q]
        elif fn == "CheckTrackToTrackCollisions":
            args = [s.insts[a["ia"]], s.insts[a["ib"]]] + [s.braces[tuple(r)] for r in a["braces"]]
        elif fn == "RemoveBraceFromCollisionSystem":
            args = [s.braces[(a["inst"], a["b"])], s.insts[a["inst"]]]
        elif fn in ("RemoveAllCollisionsFromBrace", "CheckTrackToAllTracksCollisions"):
            args = [s.insts[a["inst"]], s.braces[(a["inst"], a["b"])]]
        else:
            args = [s.insts[a["inst"]]]
        s.begin({addr: lb for addr, lb in s.lab.items()} if isinstance(s, Host) else dict(s.lab))
        ans = s.call(fn, args)
        if fn == "GetPylonAtCell":
            ans = s.lab.get(ans, "raw") if ans else 0
            extra["out"] = s.rd_word(out) if out else None
        elif fn == "DotProduct3D":
            ans = canon(ans)
        elif fn in ("GetTrackBounds", "GenerateSamplesInBoundArea"):
            ans = 0
            extra["box"] = rd_floats(s, box, 6)
            if fn == "GenerateSamplesInBoundArea":
                extra["count"] = s.rd_word(out)
        elif fn != "DoProperChunkCollision":
            ans = 0
        return observe(s, w, ans, extra)
    finally:
        s.done()


def ppc(e, case):
    return run_case(Ppc(e, case), case)


def host(lib, case):
    return run_case(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 1, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r, lo=-300.0, hi=300.0):
    return r.choice(SPECIAL) if r.random() < 0.08 else fb(r.uniform(lo, hi))


def ring(r, n, size=None):
    """n pairs (x, y) round a centre: a polygon of radius `size`."""
    size = size if size is not None else r.uniform(0.3, 1.5)
    ph = r.uniform(0, 2 * math.pi)
    out = []
    for i in range(n):
        t = ph + 2 * math.pi * i / max(n, 1) + r.uniform(-0.2, 0.2)
        out += [fb(size * math.cos(t)), fb(size * math.sin(t))]
    return out


def leg(r, start, direction, n, step):
    """n samples from start along direction (unit), the angles the rings face it with, a little noise."""
    dx, dy, dz = direction
    yaw = math.atan2(dx, dz)
    pitch = -math.asin(max(-1.0, min(1.0, dy)))
    bank = r.uniform(-0.5, 0.5) if r.random() < 0.5 else 0.0
    out = []
    for i in range(n):
        jit = lambda: r.uniform(-0.05, 0.05) if r.random() < 0.5 else 0.0
        out.append((fb(start[0] + i * step * dx + jit()), fb(start[1] + i * step * dy + jit()), fb(start[2] + i * step * dz + jit()),
                    fb(yaw + jit()), fb(pitch + jit()), fb(bank + jit())))
    return out


def crossing_samples(r, n1=None, n2=None):
    """Two legs at right angles crossing near the origin: along z, then along x (offset a little in y and z)."""
    n1 = n1 or r.randint(4, 10)
    n2 = n2 or r.randint(4, 10)
    s1, s2 = r.uniform(0.8, 2.5), r.uniform(0.8, 2.5)
    off = (r.uniform(-1.5, 1.5), r.uniform(-1.5, 1.5)) if r.random() < 0.7 else (r.uniform(-6, 6), r.uniform(-6, 6))
    a = leg(r, (0.0, 0.0, -s1 * (n1 - 1) / 2), (0.0, 0.0, 1.0), n1, s1)
    th = r.uniform(-0.4, 0.4)
    b = leg(r, (-s2 * (n2 - 1) / 2 * math.cos(th), off[0], off[1] + s2 * (n2 - 1) / 2 * math.sin(th)),
            (math.cos(th), 0.0, -math.sin(th)), n2, s2)
    return a, b


def box_of(samples, lo, hi, radius):
    pts = [tuple(bf(v) for v in smp[:3]) for smp in samples[lo:hi + 1]]
    mn = [min(p[k] for p in pts) - radius for k in range(3)]
    mx = [max(p[k] for p in pts) + radius for k in range(3)]
    return [fb(v) for v in mn + mx]


def mk_brace(r, sample, data, flags=None, number=None, bounds=None):
    return dict(number=number if number is not None else r.choice([0, 1, 2, 3, 7, r.getrandbits(32)]),
                flags=flags if flags is not None else rflags(r), sample=sample, data=data,
                fb0=0, list=[0] * 8, bounds=bounds if bounds is not None else [rf(r) for _ in range(6)])


BRACE_BITS = 0x10000 | 0x200000 | 0x400000 | 0x1 | 0x40


def rflags(r):
    k = r.random()
    if k < 0.1:
        return BRACE_BITS
    if k < 0.2:
        return 0
    v = r.getrandbits(32) & BRACE_BITS
    if r.random() < 0.3:
        v |= r.getrandbits(32) & 0x3F00FF00
    return v


def mk_data(r):
    k = r.random()
    rad = None if k < 0.15 else (r.choice([0, 0x80000000, 0x7FC00000]) if k < 0.2 else fb(r.uniform(0.0, 1.5)))
    return dict(flags=r.choice([0, 1, 1, 2, 3, r.getrandbits(32)]), radius=rad)


def mk_instance(r, samples, nbr=None, ndata=None, ty=0, pylons=None):
    """Braces at increasing samples (brace 0 is mLastBrace at the first sample); each brace's box covers its section to mPrev."""
    n = len(samples)
    nbr = nbr or r.randint(2, min(6, n))
    cuts = sorted(r.sample(range(1, n - 1), min(nbr - 2, max(n - 2, 0)))) if n > 2 else []
    idx = [0] + cuts + [n - 1]
    idx = idx[:nbr] if len(idx) >= nbr else idx
    ndata = ndata or (len(idx) if r.random() < 0.6 else r.randint(1, 3))     # own pylon data per brace, or shared blocks
    datas = [mk_data(r) for _ in range(ndata)]
    braces = []
    for j, si in enumerate(idx):
        d = r.randrange(ndata)
        rad = datas[d]["radius"]
        radius = bf(rad) if rad is not None and math.isfinite(bf(rad)) else 0.0
        if j + 1 < len(idx) and r.random() < 0.85:
            bounds = box_of(samples, si, idx[j + 1], radius)
        else:
            bounds = [rf(r, -20, 20) for _ in range(6)]
        braces.append(mk_brace(r, si, d, number=j if r.random() < 0.8 else None, bounds=bounds))
    span = max([idx[j + 1] - idx[j] for j in range(len(idx) - 1)] + [1]) + 1
    return dict(type=ty, samples=samples, datas=datas, braces=braces, span=span + r.choice([0, 0, 1, 3]),
                refs=r.choice([0, 0, 5, r.getrandbits(32)]), pylons=pylons or [])


def fill_lists(r, w, k, density=0.4, full=False):
    """Random collision lists among the instance's braces (each a valid brace of the instance)."""
    braces = w["insts"][k]["braces"]
    nb = len(braces)
    for j, b in enumerate(braces):
        if r.random() < density or full:
            cnt = 8 if full else r.randint(0, 8)
            lst = [[k, r.randrange(nb)] for _ in range(cnt)]
            if r.random() < 0.3 and cnt:
                lst[r.randrange(cnt)] = [k, j]
            b["fb0"] = cnt
            b["list"] = lst + [[k, r.randrange(nb)] if r.random() < 0.3 else 0 for _ in range(8 - cnt)]


def mk_types(r, na=None, nb=None):
    na = na if na is not None else r.choice([3, 4, 4, 5, 6])
    nb = nb if nb is not None else (na if r.random() < 0.6 else r.randint(0, 6))
    return [dict(pairs=pad(r, ring(r, na)), count=na), dict(pairs=pad(r, ring(r, nb)), count=nb)]


def pad(r, pairs):
    """Six pairs in all: a ring is built with type A's count from either type's pairs, so the ones past a count are read."""
    return pairs + [rf(r, -2, 2) for _ in range(12 - len(pairs))]


def mk_world(r, fn, insts, types, args, chunks=0, glob=None, linked=True, stride=None):
    return dict(fn=fn, insts=insts, types=types, args=args, chunks=chunks, glob=glob, linked=linked,
                stride=stride if stride is not None else r.choice([64, 128, 256, r.getrandbits(16)]),
                rings=[rf(r, -5, 5) for _ in range(72)], chunk_init=[[r.getrandbits(32)] + [rf(r) for _ in range(6)] for _ in range(64)])


def one_track_world(r, fn, args_fn, lists=0.4, recalc=None):
    a, b = crossing_samples(r)
    types = mk_types(r, nb=None)
    inst = mk_instance(r, a + b, nbr=r.randint(3, 8))
    if r.random() < 0.6:
        for br in inst["braces"]:
            br["flags"] &= ~0x10000
    w = mk_world(r, fn, [inst], types, None, chunks=0)
    fill_lists(r, w, 0, lists)
    if recalc is not None:
        for br in inst["braces"]:
            if r.random() < recalc:
                br["flags"] |= 0x200000
            if r.random() < 0.15:
                br["flags"] |= 0x10000
            elif r.random() < 0.7:
                br["flags"] &= ~0x10000
    w["args"] = args_fn(w)
    return w


# ---- per function


def w_pylon(r, edge=None):
    ninst = r.randint(0, 3)
    stride = r.choice([64, 128, 7, 0, r.getrandbits(16)])
    x, z = r.randrange(0, 64), r.randrange(0, 64)
    if r.random() < 0.1:
        x, z = r.getrandbits(32), r.getrandbits(32)
    idx = (x + z * stride) & MASK
    insts = []
    for k in range(ninst):
        pyl = []
        for _ in range(r.randint(0, 4)):
            pyl.append(dict(flags=r.choice([0, 0, 1, 4, 5, 2, 8, r.getrandbits(32)]),
                            index=idx if r.random() < 0.4 else r.choice([idx + 1, idx - 1, r.getrandbits(32)]) & MASK))
        smp = leg(r, (0, 0, 0), (0, 0, 1), 2, 1.0)
        inst = mk_instance(r, smp, nbr=2)
        inst["pylons"] = pyl
        insts.append(inst)
    w = mk_world(r, "GetPylonAtCell", insts, mk_types(r), dict(x=x, z=z, out=r.random() < 0.7), stride=stride)
    w["linked"] = r.random() < 0.95
    return w


def w_bounds(r):
    a, b = crossing_samples(r)
    inst = mk_instance(r, a + b, nbr=r.randint(2, 6))
    n = len(inst["braces"])
    i = r.randrange(n)
    j = r.randrange(n)
    if r.random() < 0.7 and i + 1 < n:
        j = i + 1
    if r.random() < 0.05 and i != j and inst["braces"][i]["sample"] > 0:     # a difference of -1: only the first sample
        inst["braces"][j]["sample"] = inst["braces"][i]["sample"] - 1
    elif inst["braces"][j]["sample"] < inst["braces"][i]["sample"]:
        i, j = j, i
    return mk_world(r, "GetTrackBounds", [inst], mk_types(r), dict(inst=0, a=i, b=j, box=[rf(r) for _ in range(6)]))


def w_recalc(r):
    w = one_track_world(r, "RecalcTrackBounds", lambda w: dict(inst=0))
    return w


def w_generate(r):
    a, b = crossing_samples(r)
    inst = mk_instance(r, a + b, nbr=r.randint(2, 6))
    n = len(inst["braces"])
    i = r.randrange(n - 1) if n > 1 else 0
    j = min(i + (1 if r.random() < 0.7 else r.randint(0, 2)), n - 1)
    rad = r.uniform(0, 3)
    smp = inst["samples"]
    if r.random() < 0.7:
        lo = r.randrange(len(smp))
        box = box_of(smp, lo, min(lo + r.randint(0, 3), len(smp) - 1), rad)
    else:
        box = [rf(r, -10, 10) for _ in range(6)]
    seg = inst["braces"][j]["sample"] - inst["braces"][i]["sample"]
    return mk_world(r, "GenerateSamplesInBoundArea", [inst], mk_types(r), dict(inst=0, a=i, b=j, box=box, which=r.randrange(2)),
                    chunks=max(seg, 1) + 1)


def w_proper(r, na=None, nb=None):
    a, b = crossing_samples(r, 2, 2) if r.random() < 0.8 else (leg(r, (0, 0, 0), (0, 0, 1), 2, 1.5),
                                                              leg(r, (r.uniform(-3, 3), r.uniform(-3, 3), 0), (0, 0, 1), 2, 1.5))
    types = mk_types(r, na, nb)
    if r.random() < 0.6:
        sz = r.uniform(0.5, 2.0)
        types[0]["pairs"] = pad(r, ring(r, types[0]["count"], sz))
        types[1]["pairs"] = pad(r, ring(r, types[1]["count"], sz * r.uniform(0.4, 1.0)))
    ia = mk_instance(r, a, nbr=2)
    ib = mk_instance(r, b, nbr=2, ty=1)
    return mk_world(r, "DoProperChunkCollision", [ia, ib], types, {}, chunks=1,
                    glob=dict(sampleA=(0, 0), sampleB=(1, 0), typeA=0, typeB=1))


def w_dot(r):
    return mk_world(r, "DotProduct3D", [], mk_types(r), dict(p=[rf(r, -50, 50) for _ in range(3)], q=[rf(r, -50, 50) for _ in range(3)]))


def w_track_to_track(r):
    a, b = crossing_samples(r)
    types = mk_types(r)
    same = r.random() < 0.4
    if same:
        inst = mk_instance(r, a + b, nbr=r.randint(4, 8))
        insts = [inst]
        n = len(inst["braces"])
        p = r.randrange(n - 1)
        q = r.randrange(n - 1)
        refs = [[0, p], [0, p + 1], [0, q], [0, q + 1]]
        if r.random() < 0.3:
            inst["braces"][q + 1]["data"] = inst["braces"][p]["data"]
    else:
        ia = mk_instance(r, a, nbr=r.choice([2, 2, 3, 4]))
        ib = mk_instance(r, b, nbr=r.choice([2, 2, 3, 4]), ty=1)
        insts = [ia, ib]
        p = r.randrange(len(ia["braces"]) - 1)
        q = r.randrange(len(ib["braces"]) - 1)
        refs = [[0, p], [0, p + 1], [1, q], [1, q + 1]]
    if r.random() < 0.7:
        insts[refs[2][0]]["braces"][refs[2][1]]["flags"] &= ~0x10000
    span = max(i["span"] for i in insts) + 2
    w = mk_world(r, "CheckTrackToTrackCollisions", insts, types, dict(ia=refs[0][0], ib=refs[2][0], braces=refs), chunks=span)
    for k in range(len(insts)):
        fill_lists(r, w, k, 0.3)
    # now and then a full list on one side
    if r.random() < 0.2:
        k, j = refs[r.choice([0, 2])]
        b0 = insts[k]["braces"][j]
        b0["fb0"] = r.choice([7, 8])
        b0["list"] = [[k, 0]] * b0["fb0"] + [0] * (8 - b0["fb0"])
    return w


def w_remove_all(r, fn="RemoveAllCollisionsFromBrace"):
    def args(w):
        return dict(inst=0, b=r.randrange(len(w["insts"][0]["braces"])))
    w = one_track_world(r, fn, args, lists=0.7)
    b = w["insts"][0]["braces"][w["args"]["b"]]
    if r.random() < 0.3:
        fill_lists(r, w, 0, 1.0, full=r.random() < 0.5)
    if r.random() < 0.4:
        b["flags"] |= 0x400000
    elif r.random() < 0.6:
        b["flags"] &= ~0x400000
    return w


def w_to_all(r):
    def args(w):
        n = len(w["insts"][0]["braces"])
        return dict(inst=0, b=r.randrange(n - 1))   # a brace with a brace before it
    w = one_track_world(r, "CheckTrackToAllTracksCollisions", args, lists=0.3)
    w["insts"][0]["span"] += 1
    w["chunks"] = len(w["insts"][0]["samples"]) + 1     # the blocks CheckAllTrackToTrackCollisions would have allocated
    return w


def w_check_all(r, fn="CheckAllTrackToTrackCollisions"):
    return one_track_world(r, fn, lambda w: dict(inst=0), lists=0.3, recalc=r.choice([0.0, 0.5, 1.0]))


def w_flag(r):
    return one_track_world(r, "FlagRecalcAllCoasterCollisions", lambda w: dict(inst=0), lists=0.6)


def w_remove_brace(r):
    return w_remove_all(r, "RemoveBraceFromCollisionSystem")


GEN = dict(GetPylonAtCell=w_pylon, GetTrackBounds=w_bounds, RecalcTrackBounds=w_recalc, GenerateSamplesInBoundArea=w_generate,
           DoProperChunkCollision=w_proper, DotProduct3D=w_dot, CheckTrackToTrackCollisions=w_track_to_track,
           RemoveAllCollisionsFromBrace=w_remove_all, CheckTrackToAllTracksCollisions=w_to_all,
           CheckAllTrackToTrackCollisions=w_check_all, DoCollisionChecking=lambda r: w_check_all(r, "DoCollisionChecking"),
           RemoveBraceFromCollisionSystem=w_remove_brace, FlagRecalcAllCoasterCollisions=w_flag)


def edges_for(fn):
    def edges():
        r = random.Random(520 + list(FNS).index(fn))
        n = 40
        for i in range(n):
            if fn == "DoProperChunkCollision" and i < 12:
                yield w_proper(r, na=[3, 4, 6, 4, 4, 6, 0, 1, 2, 5, 6, 3][i], nb=[3, 4, 6, 6, 2, 3, 4, 0, 2, 5, 1, 6][i])
                continue
            if fn == "DotProduct3D" and i < len(SPECIAL):
                w = w_dot(r)
                w["args"]["p"][i % 3] = SPECIAL[i]
                yield w
                continue
            yield GEN[fn](r)
    return edges


def rnd_for(fn):
    return lambda r: GEN[fn](r)


TESTS = [FnTest(fn, ppc, host, edge=edges_for(fn), rnd=rnd_for(fn)) for fn in FNS]
