"""Unit v51069: Advisor.cpp's static initialiser (its two save templates are lead unit v5L05's), the bump ride leaves, the lobby, camera and coaster leaves,
against libtpw_unit_sinit_Advisor_cpp.dylib (tests/unit_sinit_Advisor_cpp.cpp), main app ("SimTheme Park"),
src/game/TPCamera.cpp, src/game/advisor/Advisor.cpp, src/game/rides/BumperRide.cpp, src/game/rides/CoasterQueue.cpp, src/game/rides/CoasterTrain.cpp, src/game/rides/TPCoaster.cpp:
  __sinit_Advisor_cpp 0x1000fff0, FindBuoy 0x10025960,
  FreeColObjectsForRide 0x10027424, GetDataNodeWorldMatrix 0x1002addc, GetRideFromID 0x1002ae38, GetFinishLine 0x1002d340,
  SetBuoySequence 0x1002d390, BMPFile_Read 0x1002ffc4, __arraydtor$3112 0x10032300, GetFirstPersonRideID 0x10032b64,
  ResetCameraPosition 0x10032bc4, GetNthCoasterTrackStart 0x100458e8, __sinit_TPCoaster_cpp 0x100459f4, GetCoasterInstanceID 0x10049120,
  DestroyQueue 0x10049228, IsEndOfSampleBuffer 0x1004b050, DoImmediatePhysicsFeedback 0x1004d2ec, GetCoasterPeepStats 0x1004d920

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Structures the Mac code reads by offset with
32-bit pointers (the ride record, buoys, collision objects) keep the Mac layout on both sides (Ptr32: the host's words are offsets from the
arena base); the coaster instance, brace, spline, mesh and queue have native pointers on the host, so their offsets come from the library.
The callees outside the unit are fakes on both sides that record their arguments: the string constructor, __register_global_object,
WriteSerialiseLog, GAME_ASSERTMSG, LbFile_Read, LbFile_Write (answers, counts and the bytes read come from the case's queues),
MReleaseFreeF, __destroy_arr.

The world of a case: the globals each function reads, the lists and blocks it walks. What is compared: the answer, the globals and
blocks it writes, the event log.

No fused operations. Not tested: lists with no terminator (GetFinishLine, GetNthCoasterTrackStart and the rest walk off the end, like the
original), a NULL ride or instance (read unconditionally), the text of the global wide string at 0x1023c316 (only its address is logged),
the string destructor (registered, never run).
"""
import ctypes as C
import random
import struct

from unicorn.ppc_const import UC_PPC_REG_2

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_sinit_Advisor_cpp"
MASK = 0xFFFFFFFF
PFX = "tpw_sinitadv_"

# (address, size, name, PPC argument kinds, host argument kinds, result kind, host entry); kinds: P pointer, i int32, u uint32, f float, v none
FNS = [
    (0x1000fff0, 60, "sinit_adv", "", "", "v", "sinit_adv"),
    (0x10025960, 64, "findbuoy", "Pii", "Pii", "i", "findbuoy"),
    (0x10027424, 52, "freecol", "P", "P", "v", "freecol"),
    (0x1002addc, 32, "datanode", "PP", "PP", "P", "datanode"),
    (0x1002ae38, 20, "ridefrom", "P", "P", "P", "ridefrom"),
    (0x1002d340, 28, "finish", "P", "P", "P", "finish"),
    (0x1002d390, 64, "setseq", "P", "P", "v", "setseq"),
    (0x1002ffc4, 48, "bmpread", "PPPuP", "PPuP", "i", "bmpread"),
    (0x10032300, 52, "arraydtor", "", "", "v", "arraydtor"),
    (0x10032b64, 52, "fpid", "", "", "u", "fpid"),
    (0x10032bc4, 24, "resetcam", "", "", "i", "resetcam"),
    (0x100458e8, 64, "nth", "uP", "uP", "P", "nth"),
    (0x100459f4, 28, "sinit_coaster", "", "", "v", "sinit_coaster"),
    (0x10049120, 52, "coasterid", "i", "i", "i", "coasterid"),
    (0x10049228, 40, "destroyq", "P", "P", "v", "destroyqueue"),
    (0x1004b050, 28, "eos", "Pf", "Pf", "i", "eos"),
    (0x1004d2ec, 4, "dopf", "P", "P", "v", "dopf"),
    (0x1004d920, 32, "peep", "uPP", "uPP", "v", "peep"),
]
SIG = {f[2]: f for f in FNS}
OWN = {f[0] for f in FNS}

ADDR = dict(colList=0x102b56c0, fpFlags=0x102b5a40, fpSet=0x102b5a28, fpInst=0x102b5a3c, camA=0x102b5aa4, coasters=0x102b68b4,
            objTable=0x102b6f04, rideSets=0x102c79a4, w7314=0x102b7314, w7318=0x102b7318, store=0x102b56fc, dtorVec=0x10266010,
            sinitStr=0x102b3aa8, sinitNode=0x102b3abc, sinitText=0x1023c316, sinitDtor=0x103bcbb8, saveLoad=0x1037fefc)
LABELS = {"sinitStr": 1, "sinitText": 2, "sinitDtor": 3, "sinitNode": 4, "saveLoad": 5, "store": 6, "dtorVec": 7}
FAKE_BLOCKS = {0x1000: 8, 0x2000: 9}     # block pointers that are only passed along, never read

MAC = dict(psz=4, mesh_f28=0x28, tbl_ent=4, ent_size=0x14, ent_mat=4, q_block=4, i_next=0x18, i_cobj=0x28, i_brace=0x54, i_script=0x24,
           i_oid=0x14, i_len=0xa8, i_ra=0x100, i_rb=0x104, br_f90=0x90, sp_a=4)
HOST_NAMES = ["psz", "mesh_f28", "tbl_ent", "ent_size", "ent_mat", "q_block", "i_next", "i_cobj", "i_brace", "i_script", "i_oid",
              "i_len", "i_ra", "i_rb", "br_f90", "sp_a", "i_size", "br_size"]
MIRRORED = [0x1c, 4, 0x14, 0x20, 0x1c]    # tabBUOY size, index, next; tabCOLOBJECT size; tabCOLLIST count

KT = {"P": C.c_uint64, "i": C.c_int32, "u": C.c_uint32, "f": C.c_float}
RT = {"P": C.c_uint64, "i": C.c_int32, "u": C.c_uint32, "v": None}


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


s32 = T.s32

# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_ctor(e):
    L = e.cl
    L.rec("TbDynamicString", (L.lab(e.r(3)), L.lab(e.r(4))))


def stub_register(e):
    L = e.cl
    L.rec("Register", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


def stub_read(e):
    L = e.cl
    n = e.r(6) & MASK
    L.rec("LbFile_Read", (L.lab(e.r(4)), n))
    word = L.ret("ReadWord") & MASK
    count = L.ret("ReadCount") & MASK
    result = L.ret("ReadResult") & MASK
    if n and result == 0:
        e.write(e.r(5), struct.pack("<I", word)[:min(n, 4)])
    e.write(e.r(7), struct.pack(">I", count))
    e.write(e.r(3), struct.pack(">I", result))


def stub_write(e):
    L = e.cl
    n = e.r(6) & MASK
    data = e.read(e.r(5), min(n, 4)) if n else b""
    word = int.from_bytes(data, "little")     # the file's bytes (the original swaps to file order; the host fake logs the native word)
    L.rec("LbFile_Write", (L.lab(e.r(4)), n, word))
    e.write(e.r(7), struct.pack(">I", L.ret("WriteCount") & MASK))
    e.write(e.r(3), struct.pack(">I", L.ret("WriteResult") & MASK))


def glue(fn):
    """The cross-TOC glue the stub replaces saved r2 in the caller's frame; the caller reloads it after the call."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        return fn(e)
    return wrapped


STUBS.internal(0x10238800, glue(stub_ctor))
STUBS.internal(0x10239940, glue(stub_register))
STUBS.internal(0x1015b3dc, glue(CL.recorder("WriteSerialiseLog", "Lisi")))
STUBS.internal(0x1000dde8, glue(CL.recorder("GAME_ASSERTMSG", "is")))
STUBS.internal(0x10238848, glue(stub_read))
STUBS.internal(0x10238878, glue(stub_write))
STUBS.internal(0x10064c54, glue(CL.recorder("MReleaseFreeF", "L")))
STUBS.internal(0x10238728, glue(CL.recorder("__destroy_arr", "LLii")))


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

    def wp(self, a, v):
        self.write(a, struct.pack(self.pfmt, v & self.pmask))

    def rp(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.lay["psz"]))[0]

    def p32(self, addr):
        return 0 if addr == 0 else addr - self.base

    def a32(self, w):
        return 0 if w == 0 else w + self.base

    def rel(self, addr, block, unit=1):
        return None if addr == 0 else (addr - block) // unit

    def rel32(self, w, block):
        return self.rel(self.a32(w), block)

    def cstr(self, text):
        a = self.alloc(len(text) + 1)
        self.write(a, text.encode())
        return a

    def done(self):
        pass


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    pmask = MASK
    base = 0
    lay = MAC
    ppc = True

    def __init__(self, e, case):
        self.e = e
        fixed = {ADDR[n]: l for n, l in LABELS.items()}
        fixed.update(FAKE_BLOCKS)
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, key, *args):
        addr, _, _, pk, _, rk, _ = SIG[key]
        ia = [T.F(a) if k == "f" else a & MASK for k, a in zip(pk, args)]
        self.e.uc.reg_write(UC_PPC_REG_2, 0x1003abf0)
        r3, _, _ = T.call_ppc(self.e, addr, ia)
        return s32(r3) if rk == "i" else (r3 & MASK if rk in "uP" else None)


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    pmask = (1 << 64) - 1
    ppc = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.lay = lib.lay
        getattr(lib, PFX + "alloc")(16)        # the arena is reserved by its first allocation
        self.base = getattr(lib, PFX + "base")()
        fixed = {l: self.g(n) for n, l in LABELS.items()}
        fixed.update({l: a for a, l in FAKE_BLOCKS.items()})
        CL.host_begin(lib, fixed, case["queues"])

    def g(self, name):
        return getattr(self.lib, PFX + "addr")(name.encode())

    def alloc(self, n):
        return getattr(self.lib, PFX + "alloc")(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, key, *args):
        return getattr(self.lib, PFX + SIG[key][6])(*args)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    a = getattr(lib, PFX + "addr")
    a.restype, a.argtypes = C.c_uint64, [C.c_char_p]
    a = getattr(lib, PFX + "alloc")
    a.restype, a.argtypes = C.c_uint64, [C.c_uint32]
    a = getattr(lib, PFX + "base")
    a.restype, a.argtypes = C.c_uint64, []
    getattr(lib, PFX + "layout").argtypes = [C.POINTER(C.c_int64)]
    for _, _, name, _, hk, rk, hostname in FNS:
        fn = getattr(lib, PFX + hostname)
        fn.argtypes = [KT[k] for k in hk]
        fn.restype = RT[rk]
    out = (C.c_int64 * 64)()
    n = getattr(lib, PFX + "layout")(out)
    vals = list(out)[:n]
    assert vals[:5] == MIRRORED, vals
    lib.lay = dict(zip(HOST_NAMES, vals[5:]))
    assert lib.lay["psz"] == 8 and lib.lay["i_size"] <= 0x800 and lib.lay["br_size"] <= 0x800, lib.lay
    lib.base_value = getattr(lib, PFX + "base")()
    lib.bound = True


def make(build, execute, observe):
    def run(s, case):
        try:
            ctx = build(s, case)
            return observe(s, case, ctx, execute(s, case, ctx))
        finally:
            s.done()

    return (lambda e, case: run(Ppc(e, case), case)), (lambda lib, case: run(Host(lib, case), case))


def new_case(r, **kw):
    c = {"queues": {}}
    c.update(kw)
    return c


def nothing(s, case):
    return {}


# ---------------------------------------------------------------------------------------------------------------- __sinit_Advisor_cpp


def sinit_adv_observe(s, case, ctx, ans):
    return {"log": s.log(), "node": s.read(s.g("sinitNode"), 8)}


def sinit_adv_edges():
    yield new_case(random.Random(1))


ppc_sinit_adv, host_sinit_adv = make(nothing, lambda s, c, x: s.call("sinit_adv"), sinit_adv_observe)

FILE = 0x1234

# ---------------------------------------------------------------------------------------------------------------- buoys


def buoy_list(r, n, finish=False, setseq=False):
    xs = [r.randrange(0, 4) for _ in range(n)]
    bs = [{"flags": r.getrandbits(3) | (r.getrandbits(29) << 3 if r.random() < 0.3 else 0), "index": r.choice([0, 1, 2, -1, 77, r.getrandbits(32)]),
           "x": xs[i], "y": r.randrange(0, 3)} for i in range(n)]
    if finish and n:
        bs[r.randrange(n)]["flags"] |= 4
    if setseq and n:
        bs[r.randrange(n)]["flags"] &= ~2
    return bs


def buoy_build(s, case):
    bs = case["buoys"]
    n = len(bs)
    ride = s.alloc(0xd0)
    blk = s.alloc(max(n, 1) * 0x1c)
    for i, b in enumerate(bs):
        nxt = blk + (i + 1) * 0x1c if i + 1 < n else 0
        s.words(blk + i * 0x1c, [b["flags"], b["index"], b["x"], b["y"], 0, s.p32(nxt), 0])
    s.words(ride + 0xa8, [s.p32(blk if n else 0)])
    return {"ride": ride, "blk": blk}


def find_case(r, n=None):
    n = r.randrange(0, 6) if n is None else n
    c = new_case(r, buoys=buoy_list(r, n))
    if n and r.random() < 0.6:
        b = r.choice(c["buoys"])
        c["xy"] = (b["x"], b["y"])
    else:
        c["xy"] = (r.randrange(0, 5), r.randrange(0, 4))
    return c


def find_edges():
    r = random.Random(3)
    for n in (0, 1, 2, 3, 5):
        for _ in range(4):
            yield find_case(r, n)
    # duplicates: the first match wins
    c = find_case(r, 3)
    for b in c["buoys"]:
        b["x"], b["y"], b["flags"] = 1, 1, 0
    c["xy"] = (1, 1)
    yield c
    # x matches, y does not
    c = find_case(r, 3)
    for b in c["buoys"]:
        b["x"], b["y"] = 2, 1
    c["xy"] = (2, 2)
    yield c
    c["xy"] = (3, 1)
    yield c


ppc_find, host_find = make(buoy_build, lambda s, c, x: s.call("findbuoy", x["ride"], *c["xy"]),
                           lambda s, c, x, a: {"ans": a, "log": s.log()})


def finish_obs(s, case, ctx, ans):
    return {"ans": s.rel(ans, ctx["blk"], 0x1c), "log": s.log()}


def finish_case(r, n=None):
    n = r.randrange(1, 6) if n is None else n
    return new_case(r, buoys=buoy_list(r, n, finish=True))


def finish_edges():
    r = random.Random(4)
    for n in (1, 2, 3, 5):
        for _ in range(3):
            yield finish_case(r, n)
    c = finish_case(r, 3)
    for b in c["buoys"]:
        b["flags"] = 0
    c["buoys"][0]["flags"] = 4
    yield c
    for b in c["buoys"]:
        b["flags"] = 4
    yield c
    for b in c["buoys"]:
        b["flags"] = 0
    c["buoys"][2]["flags"] = 0xFFFFFFFB | 4
    yield c


ppc_finish, host_finish = make(buoy_build, lambda s, c, x: s.call("finish", x["ride"]), finish_obs)


def setseq_case(r, n=None):
    n = r.randrange(1, 6) if n is None else n
    return new_case(r, buoys=buoy_list(r, n, finish=True, setseq=True))


def setseq_obs(s, case, ctx, ans):
    return {"seq": s.rel32(s.rd_word(ctx["ride"] + 0xa4), ctx["blk"]) if s.rd_word(ctx["ride"] + 0xa4) else None,
            "flags": [s.rd_word(ctx["blk"] + i * 0x1c) for i in range(len(case["buoys"]))], "log": s.log()}


def setseq_edges():
    r = random.Random(5)
    for n in (1, 2, 3, 5):
        for _ in range(4):
            yield setseq_case(r, n)
    # the finish line is the last buoy: the walk wraps to the first
    c = setseq_case(r, 3)
    for b, f in zip(c["buoys"], (0, 0, 4)):
        b["flags"] = f
    yield c
    c["buoys"][0]["flags"] = 2          # first skipped
    yield c
    c["buoys"][0]["flags"] = 3
    c["buoys"][1]["flags"] = 2
    yield c
    c["buoys"][0]["flags"] = 6          # the finish line itself is the only one without flag 2 left
    c["buoys"][1]["flags"] = 2
    c["buoys"][2]["flags"] = 2 | 4
    c["buoys"][2]["flags"] = 6
    c["buoys"][0]["flags"] = 4
    yield c


ppc_setseq, host_setseq = make(buoy_build, lambda s, c, x: s.call("setseq", x["ride"]), setseq_obs)

# ---------------------------------------------------------------------------------------------------------------- collision objects


def freecol_case(r, m=None):
    m = r.randrange(0, 5) if m is None else m
    return new_case(r, count=r.choice([0, 1, 7, -1, 0x7FFFFFFF, r.getrandbits(32)]),
                    cols=[[r.getrandbits(32) | 1 for _ in range(5)] + [0, r.getrandbits(32), r.getrandbits(32)] for _ in range(m + 1)], m=m)


def freecol_build(s, case):
    m = case["m"]
    ride = s.alloc(0xd0)
    blk = s.alloc((m + 1) * 0x20)
    for i, col in enumerate(case["cols"]):
        ws = list(col)
        ws[5] = s.p32(blk + (i + 1) * 0x20) if i + 1 < m else 0       # the last one (index m) is not in the ride's list
        s.words(blk + i * 0x20, ws)
    s.words(ride + 0xc0, [s.p32(blk if m else 0)])
    s.words(s.g("colList") + 0x1c, [case["count"]])
    return {"ride": ride, "blk": blk}


def freecol_obs(s, case, ctx, ans):
    cols = []
    for i in range(case["m"] + 1):
        ws = s.rd_words(ctx["blk"] + i * 0x20, 8)
        ws[5] = s.rel32(ws[5], ctx["blk"])
        cols.append(ws)
    return {"count": s.rd_word(s.g("colList") + 0x1c), "cols": cols, "head": s.rel32(s.rd_word(ctx["ride"] + 0xc0), ctx["blk"]),
            "log": s.log()}


def freecol_edges():
    r = random.Random(6)
    for m in (0, 1, 2, 4):
        for _ in range(3):
            yield freecol_case(r, m)


ppc_freecol, host_freecol = make(freecol_build, lambda s, c, x: s.call("freecol", x["ride"]), freecol_obs)

# ---------------------------------------------------------------------------------------------------------------- GetDataNodeWorldMatrix


def datanode_case(r, n=None, idx=None):
    n = r.randrange(1, 6) if n is None else n
    return new_case(r, n=n, idx=r.randrange(0, n) if idx is None else idx, ents=[[r.getrandbits(32) for _ in range(5)] for _ in range(n)],
                    tw=r.getrandbits(32))


def datanode_build(s, case):
    lay = s.lay
    n = case["n"]
    mesh = s.alloc(0x800)
    table = s.alloc(0x40)
    ents = s.alloc(n * lay["ent_size"])
    mats = s.alloc(n * 0x40)
    s.wp(mesh + lay["mesh_f28"], table)
    s.words(table, [case["tw"]])
    s.wp(table + lay["tbl_ent"], ents)
    for k, w in enumerate(case["ents"]):
        e = ents + k * lay["ent_size"]
        s.words(e, [w[0]])
        s.wp(e + lay["ent_mat"], mats + k * 0x40)
    cell = s.alloc(8)
    s.wp(cell, mesh)
    idx = s.alloc(8)
    s.words(idx, [case["idx"]])
    return {"cell": cell, "idx": idx, "mats": mats, "mesh": mesh}


def datanode_obs(s, case, ctx, ans):
    return {"ans": s.rel(ans, ctx["mats"], 0x40), "idx": s.rd_word(ctx["idx"]), "mesh": s.rel(s.rp(ctx["cell"]), ctx["mesh"]),
            "log": s.log()}


def datanode_edges():
    r = random.Random(7)
    for n in (1, 2, 5):
        for idx in sorted({0, n - 1}):
            yield datanode_case(r, n, idx)


ppc_datanode, host_datanode = make(datanode_build, lambda s, c, x: s.call("datanode", x["cell"], x["idx"]), datanode_obs)

# ---------------------------------------------------------------------------------------------------------------- GetRideFromID


def ride_case(r, idx=None):
    return new_case(r, id=r.choice([0, 1, 2, 0x7FF, r.randrange(0, 0x800)]) if idx is None else idx)


def ride_build(s, case):
    cell = s.alloc(8)
    s.words(cell, [case["id"]])
    return {"cell": cell}


def ride_obs(s, case, ctx, ans):
    return {"ans": s.rel(ans, s.g("rideSets"), s.lay["psz"]), "id": s.rd_word(ctx["cell"]), "log": s.log()}


def ride_edges():
    r = random.Random(8)
    for i in (0, 1, 2, 3, 0x100, 0x7FF, 0x1000, 0x10000):
        yield ride_case(r, i)


ppc_ride, host_ride = make(ride_build, lambda s, c, x: s.call("ridefrom", x["cell"]), ride_obs)

# ---------------------------------------------------------------------------------------------------------------- BMPFile_Read


def bmp_case(r, size=None, res=None, cnt=None):
    c = new_case(r, size=r.choice([0, 1, 3, 4, 5, 8, 16]) if size is None else size)
    c["queues"] = {"ReadWord": [r.getrandbits(32)], "ReadCount": [r.choice([0, 4, 3, 16, 77]) if cnt is None else cnt],
                   "ReadResult": [r.choice([0, 1, -1, 5, -0x80000000]) if res is None else res]}
    c["tr0"] = r.getrandbits(32)
    return c


def bmp_build(s, case):
    buf = s.alloc(32)
    s.write(buf, bytes([0xAA] * 32))
    tr = s.alloc(8)
    s.words(tr, [case["tr0"]])
    return {"buf": buf, "tr": tr}


def bmp_exec(s, case, ctx):
    if s.ppc:
        out = s.alloc(8)
        s.words(out, [0x5A5A5A5A])
        s.call("bmpread", out, FILE, ctx["buf"], case["size"], ctx["tr"])
        return s32(s.rd_word(out))
    return s.call("bmpread", FILE, ctx["buf"], case["size"], ctx["tr"])


def bmp_obs(s, case, ctx, ans):
    return {"ans": ans, "tr": s.rd_word(ctx["tr"]), "buf": s.read(ctx["buf"], 32), "log": s.log()}


def bmp_edges():
    r = random.Random(9)
    for size in (0, 1, 3, 4, 5, 16):
        for res in (0, -1, 1):
            yield bmp_case(r, size, res)


ppc_bmp, host_bmp = make(bmp_build, bmp_exec, bmp_obs)

# ---------------------------------------------------------------------------------------------------------------- __arraydtor$3112


def arraydtor_edges():
    yield new_case(random.Random(10))


ppc_arraydtor, host_arraydtor = make(nothing, lambda s, c, x: s.call("arraydtor"), lambda s, c, x, a: {"log": s.log()})

# ---------------------------------------------------------------------------------------------------------------- camera


def fp_case(r, flags=None, setp=None, inst=None):
    return new_case(r, flags=r.getrandbits(32) if flags is None else flags, set=r.choice([0, 0x1000, 0x2000]) if setp is None else setp,
                    inst=r.choice([0, 1, 0x1234, r.getrandbits(32)]) if inst is None else inst)


def fp_build(s, case):
    s.words(s.g("fpFlags"), [case["flags"]])
    s.wp(s.g("fpSet"), case["set"])
    s.words(s.g("fpInst"), [case["inst"]])
    return {}


def fp_obs(s, case, ctx, ans):
    return {"ans": ans, "flags": s.rd_word(s.g("fpFlags")), "set": s.rp(s.g("fpSet")), "inst": s.rd_word(s.g("fpInst")), "log": s.log()}


def fp_edges():
    r = random.Random(11)
    for flags in (0, 4, 0xB, 0xFFFFFFFB, 0xFFFFFFFF, 0x16):
        for setp in (0, 0x1000):
            yield fp_case(r, flags, setp, 0x77)


ppc_fp, host_fp = make(fp_build, lambda s, c, x: s.call("fpid"), fp_obs)


def cam_case(r, v=None):
    return new_case(r, v=r.getrandbits(32) if v is None else v)


def cam_build(s, case):
    s.words(s.g("camA"), [case["v"]])
    return {}


def cam_edges():
    r = random.Random(12)
    for v in (0, 0x100, 0xFFFFFFFF, 0x80, 0x200, 0xFFFFFEFF):
        yield cam_case(r, v)


ppc_cam, host_cam = make(cam_build, lambda s, c, x: s.call("resetcam"), lambda s, c, x, a: {"ans": a, "cam": s.rd_word(s.g("camA")), "log": s.log()})

# ---------------------------------------------------------------------------------------------------------------- coaster lists


def coaster_case(r, k=None, **kw):
    k = r.randrange(0, 5) if k is None else k
    c = new_case(r, k=k, script=[r.choice([0, 1, 2, 3, -1, r.getrandbits(32)]) for _ in range(k)],
                 oid=[r.choice([0, 5, r.getrandbits(32)]) for _ in range(k)], cobj=[0x5000 + 0x100 * i for i in range(k)],
                 ctl=[0x7000 + 0x10 * i for i in range(k)])
    c.update(kw)
    return c


def coaster_build(s, case):
    lay = s.lay
    k = case["k"]
    insts = [s.alloc(0x800) for _ in range(k)]
    for i, a in enumerate(insts):
        s.wp(a + lay["i_next"], insts[i + 1] if i + 1 < k else 0)
        s.words(a + lay["i_script"], [case["script"][i]])
        s.words(a + lay["i_oid"], [case["oid"][i]])
        s.wp(a + lay["i_cobj"], case["cobj"][i])
        brace = s.alloc(0x800)
        spline = s.alloc(0x40)
        s.wp(a + lay["i_brace"], brace)
        s.wp(brace + lay["br_f90"], spline)
        s.wp(spline + lay["sp_a"], case["ctl"][i])
    s.wp(s.g("coasters"), insts[0] if k else 0)
    cell = s.alloc(8)
    s.wp(cell, 0x99)
    return {"insts": insts, "cell": cell}


def nth_case(r, k=None, index=None, nullctl=None):
    c = coaster_case(r, r.randrange(1, 5) if k is None else k)
    c["index"] = r.randrange(0, c["k"]) if index is None else index
    c["nullctl"] = r.random() < 0.3 if nullctl is None else nullctl
    return c


def nth_exec(s, case, ctx):
    return s.call("nth", case["index"], 0 if case["nullctl"] else ctx["cell"])


def nth_obs(s, case, ctx, ans):
    return {"ans": ans, "cell": s.rp(ctx["cell"]), "log": s.log()}


def nth_edges():
    r = random.Random(13)
    for k in (1, 2, 4):
        for index in sorted({0, k - 1}):
            for nullctl in (False, True):
                yield nth_case(r, k, index, nullctl)


ppc_nth, host_nth = make(coaster_build, nth_exec, nth_obs)


def cid_case(r, k=None):
    c = coaster_case(r, k)
    c["handle"] = r.choice(c["script"] + [0, 1, 2, -1, 9]) if c["k"] else r.choice([0, 1, -1])
    return c


def cid_edges():
    r = random.Random(14)
    for k in (0, 1, 3, 4):
        for _ in range(4):
            yield cid_case(r, k)
    c = cid_case(r, 3)
    c["script"] = [4, 4, 4]
    c["handle"] = 4
    yield c


ppc_cid, host_cid = make(coaster_build, lambda s, c, x: s.call("coasterid", c["handle"]), lambda s, c, x, a: {"ans": a, "log": s.log()})

# ---------------------------------------------------------------------------------------------------------------- __sinit_TPCoaster_cpp


def sinit_c_case(r):
    return new_case(r, a=r.getrandbits(32), b=r.getrandbits(32))


def sinit_c_build(s, case):
    s.words(s.g("w7314"), [case["a"]])
    s.words(s.g("w7318"), [case["b"]])
    return {}


def sinit_c_edges():
    r = random.Random(15)
    yield sinit_c_case(r)
    c = sinit_c_case(r)
    c["a"], c["b"] = 0, 0
    yield c


ppc_sinit_c, host_sinit_c = make(sinit_c_build, lambda s, c, x: s.call("sinit_coaster"),
                                 lambda s, c, x, a: {"w7314": s.rd_word(s.g("w7314")), "w7318": s.rd_word(s.g("w7318")), "log": s.log()})

# ---------------------------------------------------------------------------------------------------------------- DestroyQueue


def dq_case(r, block=None):
    return new_case(r, block=r.choice([0, 0x1000, 0x2000, 0x3000]) if block is None else block, w=r.getrandbits(32))


def dq_build(s, case):
    q = s.alloc(0x40)
    s.words(q, [case["w"]])
    s.wp(q + s.lay["q_block"], case["block"])
    return {"q": q}


def dq_edges():
    r = random.Random(16)
    for b in (0, 0x1000, 0x2000, 0x3000):
        yield dq_case(r, b)


ppc_dq, host_dq = make(dq_build, lambda s, c, x: s.call("destroyq", x["q"]),
                       lambda s, c, x, a: {"w": s.rd_word(x["q"]), "block": s.rp(x["q"] + s.lay["q_block"]), "log": s.log()})

# ---------------------------------------------------------------------------------------------------------------- IsEndOfSampleBuffer

F_EDGES = [0, 0x80000000, 0x3F800000, 0xBF800000, 0x7F800000, 0xFF800000, 0x7FC00000, 0x00000001, 0x80000001, 0x40000000, 0x3F800001]


def rf(r):
    k = r.random()
    if k < 0.2:
        return r.choice(F_EDGES)
    return fb(r.uniform(-500, 500))


def eos_case(r, length=None, pos=None):
    return new_case(r, length=rf(r) if length is None else length, pos=rf(r) if pos is None else pos)


def eos_build(s, case):
    a = s.alloc(0x800)
    s.words(a + s.lay["i_len"], [case["length"]])
    return {"a": a}


def eos_edges():
    r = random.Random(17)
    for length in F_EDGES:
        for pos in F_EDGES:
            yield eos_case(r, length, pos)
    yield eos_case(r, fb(100.0), fb(100.0))
    yield eos_case(r, fb(100.0), fb(99.99))
    yield eos_case(r, fb(100.0), fb(100.01))


ppc_eos, host_eos = make(eos_build, lambda s, c, x: s.call("eos", x["a"], bitsf(c["pos"])),
                         lambda s, c, x, a: {"ans": a, "len": s.rd_word(x["a"] + s.lay["i_len"]), "log": s.log()})

# ---------------------------------------------------------------------------------------------------------------- DoImmediatePhysicsFeedback


def dopf_case(r):
    return new_case(r, w=[r.getrandbits(32) for _ in range(4)])


def dopf_build(s, case):
    a = s.alloc(0x800)
    s.words(a, case["w"])
    return {"a": a}


ppc_dopf, host_dopf = make(dopf_build, lambda s, c, x: s.call("dopf", x["a"]),
                           lambda s, c, x, a: {"w": s.rd_words(x["a"], 4), "log": s.log()})


def dopf_edges():
    yield dopf_case(random.Random(18))


# ---------------------------------------------------------------------------------------------------------------- GetCoasterPeepStats


def peep_case(r, slot=None):
    return new_case(r, slot=r.randrange(0, 0x100) if slot is None else slot, ra=rf(r), rb=rf(r), other=rf(r))


def peep_build(s, case):
    lay = s.lay
    a = s.alloc(0x800)
    s.words(a + lay["i_ra"], [case["ra"]])
    s.words(a + lay["i_rb"], [case["rb"]])
    s.words(a + lay["i_ra"] + 8, [case["other"]])
    s.wp(s.g("objTable") + case["slot"] * lay["psz"], a)
    out = s.alloc(16)
    s.words(out, [0xDEADBEEF, 0xFEEDF00D])
    return {"a": a, "out": out}


def peep_obs(s, case, ctx, ans):
    return {"a": s.rd_word(ctx["a"] + s.lay["i_ra"]), "b": s.rd_word(ctx["a"] + s.lay["i_rb"]),
            "out": [canon_f(w) for w in s.rd_words(ctx["out"], 2)], "log": s.log()}


def peep_edges():
    r = random.Random(19)
    for slot in (0, 1, 0xFF):
        yield peep_case(r, slot)
    for v in F_EDGES:
        c = peep_case(r, 3)
        c["ra"] = v
        c["rb"] = v ^ 0x80000000
        yield c


ppc_peep, host_peep = make(peep_build, lambda s, c, x: s.call("peep", c["slot"], x["out"], x["out"] + 4), peep_obs)

# ---------------------------------------------------------------------------------------------------------------- TESTS


def rnd(gen):
    return lambda r: gen(r)


TESTS = [
    FnTest("__sinit_Advisor_cpp", ppc_sinit_adv, host_sinit_adv, edge=sinit_adv_edges, rnd=lambda r: new_case(r)),
    FnTest("FindBuoy", ppc_find, host_find, edge=find_edges, rnd=find_case),
    FnTest("FreeColObjectsForRide", ppc_freecol, host_freecol, edge=freecol_edges, rnd=freecol_case),
    FnTest("GetDataNodeWorldMatrix", ppc_datanode, host_datanode, edge=datanode_edges, rnd=datanode_case),
    FnTest("GetRideFromID", ppc_ride, host_ride, edge=ride_edges, rnd=ride_case),
    FnTest("GetFinishLine", ppc_finish, host_finish, edge=finish_edges, rnd=finish_case),
    FnTest("SetBuoySequence", ppc_setseq, host_setseq, edge=setseq_edges, rnd=setseq_case),
    FnTest("BMPFile_Read", ppc_bmp, host_bmp, edge=bmp_edges, rnd=bmp_case),
    FnTest("__arraydtor$3112", ppc_arraydtor, host_arraydtor, edge=arraydtor_edges, rnd=lambda r: new_case(r)),
    FnTest("GetFirstPersonRideID", ppc_fp, host_fp, edge=fp_edges, rnd=fp_case),
    FnTest("ResetCameraPosition", ppc_cam, host_cam, edge=cam_edges, rnd=cam_case),
    FnTest("GetNthCoasterTrackStart", ppc_nth, host_nth, edge=nth_edges, rnd=nth_case),
    FnTest("__sinit_TPCoaster_cpp", ppc_sinit_c, host_sinit_c, edge=sinit_c_edges, rnd=sinit_c_case),
    FnTest("GetCoasterInstanceID", ppc_cid, host_cid, edge=cid_edges, rnd=cid_case),
    FnTest("DestroyQueue", ppc_dq, host_dq, edge=dq_edges, rnd=dq_case),
    FnTest("IsEndOfSampleBuffer", ppc_eos, host_eos, edge=eos_edges, rnd=eos_case),
    FnTest("DoImmediatePhysicsFeedback", ppc_dopf, host_dopf, edge=dopf_edges, rnd=dopf_case),
    FnTest("GetCoasterPeepStats", ppc_peep, host_peep, edge=peep_edges, rnd=peep_case),
]
