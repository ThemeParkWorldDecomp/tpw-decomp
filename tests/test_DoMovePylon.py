"""Unit v5026, dragging a pylon of the track editor, against libtpw_unit_DoMovePylon.dylib (tests/unit_DoMovePylon.cpp), main app
("SimTheme Park"), src/game/rides/CoasterEdit.cpp:
  StartMovePylon 0x10057738, DoMovePylon 0x100579cc

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (FlagGenerateTrack,
GetPylonAtCell, MoveBrace, CalculateBraceCost, RoundUp, the bank account's methods, the LLCC_ functions, LockPointedAtRide,
DisableCurrentPylonEditState, Assign3DObjectToAIWorld, CalculateBraceHeight, SetTrackPreview2ndBrace, SetPotentialPylonArea,
CActionRecorder::RecordAction) is a fake on both sides that records its arguments (pointers as labels; out pointers as present or not,
and the floats they point at) and answers from the case's queues. The float to unsigned glue at 0x10237cd8 runs as the original on the
PowerPC side and is cw_cvt_fp2unsigned on the host.

The world of a case: the unit's globals, the editor's coaster flags, the dragged brace with its pylon record, the pylon the cell holds,
the marked braces and their chain, the recorder's recording word. What is compared: the answer, every global, the marked braces' flags,
the coaster's flags and the event log. Structure fields sit at the Mac's offsets on the PowerPC side and at the host's (taken from
the library's layout export) on the host side; pointers are 4 bytes there and 8 here.

No fused operations. Not tested: a marked second brace with no first one, and a chain of marked braces that does not reach the other
mark (the original dereferences NULL there).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DoMovePylon"
MASK = 0xFFFFFFFF

FN_START, FN_DO = 0x10057738, 0x100579CC
SIZES = {FN_START: 604, FN_DO: 1024}
OWN = set(SIZES)

ADDR = dict(
    w16bc=0x102C16BC, pz=0x102C16C4, px=0x102C16C8, second=0x102C176C, orig=0x102C1774, hgt=0x102C1778, mbrace=0x102C177C,
    mark2=0x102C1780, mark1=0x102C1784, edit=0x102B68B0, ext=0x102B74A8, e16e0=0x102C16E0, cost=0x102C1770, flags=0x102C1794,
    hh=0x102C6C64, hw=0x102C6C68, world=0x1037EBFC, rec=0x1037EC44,
)
WORDS = ["w16bc", "pz", "px", "ext", "e16e0", "flags", "hh", "hw"]
FLOATS = ["second", "hgt", "cost"]
PTRS = ["orig", "mbrace", "mark2", "mark1", "edit", "world"]

# label, size of every block (allocated once, rewritten every case)
BLK = {
    "edit": (1, 0x200), "brace": (2, 0x200), "pylon": (3, 0x80), "data": (4, 0x80), "orig": (5, 0x80), "owner": (6, 0x100),
    "bank": (7, 0x40), "mark1": (8, 0x200), "mark2": (9, 0x200), "mid": (10, 0x200), "prevb": (11, 0x200), "nextb": (12, 0x200),
    "world": (13, 0x40),
}
REC_LABEL = 14
LAB2NAME = {v[0]: k for k, v in BLK.items()}

MAC = dict(b_flags=8, b_prev=0x64, b_next=0x68, b_w70=0x70, b_data=0x100, d_flags=4, d_x=0xC, d_z=0x10, d_f14=0x14, d_f18=0x18,
           d_f30=0x30, d_owner=0x34, o_f70=0x70, i_b3c=0x3C, r_rec=0x24)
LAYOUT_NAMES = ["b_flags", "b_prev", "b_next", "b_w70", "b_data", "d_flags", "d_x", "d_z", "d_f14", "d_f18", "d_f30", "d_owner", "o_f70",
                "i_b3c", "r_rec"]
HOST = {}
HB = {}          # host blocks: name -> address
PB = {}          # PowerPC blocks: name -> address


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def canon_f(bits):
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def R(e, name, *vals):
    e.cl.rec(name, tuple(int(v) & MASK for v in vals))


def LB(e, a):
    return e.cl.lab(a & MASK)


def blk_of(label):
    n = LAB2NAME.get(label)
    return PB[n] if n else 0


def rdf(e, a):
    return canon_f(struct.unpack(">I", e.read(a, 4))[0]) if a else 0


def st_flag(e):
    R(e, "FlagGenerateTrack", LB(e, e.r(3)), e.r(4))


def st_getpylon(e):
    R(e, "GetPylonAtCell", e.r(3), e.r(4), 1 if e.r(5) else 0)
    e.set_r(3, blk_of(e.cl.ret("GetPylonAtCell")))


def st_move(e):
    sp = e.r(1)
    d = struct.unpack(">I", e.read(sp + 0x38, 4))[0]
    R(e, "MoveBrace", LB(e, e.r(3)), LB(e, e.r(4)), LB(e, e.r(5)), e.r(6), e.r(7), 1 if e.r(8) else 0, 1 if e.r(9) else 0, e.r(10), d)
    if e.r(8):
        e.write(e.r(8), struct.pack(">I", e.cl.ret("MoveBrace_a") & MASK))
    if e.r(9):
        e.write(e.r(9), struct.pack(">I", e.cl.ret("MoveBrace_b") & MASK))


def st_cost(e):
    vals = [LB(e, e.r(3)), e.r(4), LB(e, e.r(5))]
    for i in (6, 7, 8, 9):
        p = e.r(i)
        vals += [1 if p else 0, rdf(e, p)]
    R(e, "CalculateBraceCost", *vals)
    e.set_f1(bitsf(e.cl.ret("CalculateBraceCost")))


def st_roundup(e):
    R(e, "RoundUp", canon_f(fb(e.f(1))))
    e.set_r(3, e.cl.ret("RoundUp") & MASK)


def st_bank(e):
    R(e, "GetBankAccount", LB(e, e.r(3)))
    e.set_r(3, PB["bank"])


def st_deposit(e):
    R(e, "MakeDeposit", LB(e, e.r(3)), e.r(4))


def st_balance(e):
    R(e, "GetBalance", LB(e, e.r(3)))
    e.set_r(3, 0)


def st_withdraw(e):
    R(e, "MakeWithdrawal", LB(e, e.r(3)), e.r(4))


def st_plain(name):
    return lambda e: R(e, name)


def st_start(e):
    R(e, "LLCC_StartPylonMove", e.r(3), e.r(4), e.r(5))


def st_lock(e):
    R(e, "LockPointedAtRide", e.r(3))


def st_assign(e):
    R(e, "Assign3DObjectToAIWorld", LB(e, e.r(3)), e.r(4), e.r(5), e.r(6), e.r(7))


def st_height(e):
    R(e, "CalculateBraceHeight", LB(e, e.r(3)))
    e.set_f1(bitsf(e.cl.ret("CalculateBraceHeight")))


def st_preview(e):
    R(e, "SetTrackPreview2ndBrace", LB(e, e.r(3)), e.r(4))


def st_record(e):
    R(e, "RecordAction", LB(e, e.r(3)), e.r(4), e.r(5), e.r(6), e.r(7))


for addr, fn in [
    (0x10040F08, st_flag), (0x1003F954, st_getpylon), (0x10054978, st_move), (0x10058924, st_cost), (0x1004DA1C, st_roundup),
    (0x1013EDFC, st_bank), (0x100F32EC, st_deposit), (0x100F32B4, st_balance), (0x100F33B0, st_withdraw),
    (0x1008F1A4, st_plain("LLCC_AbortPylonMove")), (0x1008F2F4, st_plain("LLCC_ResetPylonMove")), (0x1008F0B4, st_start),
    (0x10067288, st_lock), (0x10053B58, st_plain("DisableCurrentPylonEditState")), (0x1005F53C, st_assign), (0x1003E6E0, st_height),
    (0x10053A04, st_preview), (0x10041A58, st_plain("SetPotentialPylonArea")), (0x1014445C, st_record),
]:
    STUBS.internal(addr, fn)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for name, (lab, size) in BLK.items():
        PB[name] = T.persist_alloc(runner.emu, size)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def setptr(self, a, name):
        v = self.blocks[name] if name else 0
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psize))[0]

    def pname(self, v):
        for n, a in self.blocks.items():
            if a == v:
                return n
        return v


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psize = 4
    O = MAC

    def __init__(self, e, case):
        self.e = e
        self.blocks = PB
        fixed = {a: BLK[n][0] for n, a in PB.items()}
        fixed[ADDR["rec"]] = REC_LABEL
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call_start(self, brace):
        r3, _, _ = T.call_ppc(self.e, FN_START, [self.blocks[brace] if brace else 0])
        return r3 & 0xFF

    def call_do(self, x, z):
        r3, _, _ = T.call_ppc(self.e, FN_DO, [x & MASK, z & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = HOST
        self.blocks = HB
        fixed = {BLK[n][0]: a for n, a in HB.items()}
        fixed[REC_LABEL] = self.g("rec")
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dmp_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_start(self, brace):
        return self.lib.tpw_dmp_start(self.blocks[brace] if brace else 0) & 0xFF

    def call_do(self, x, z):
        return self.lib.tpw_dmp_do(x & MASK, z & MASK) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_dmp_addr.restype, lib.tpw_dmp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_dmp_alloc.restype, lib.tpw_dmp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_dmp_setblock.restype, lib.tpw_dmp_setblock.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_dmp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_dmp_start.restype, lib.tpw_dmp_start.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_dmp_do.restype, lib.tpw_dmp_do.argtypes = C.c_uint32, [C.c_uint32, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_dmp_layout(out)
    assert n == len(LAYOUT_NAMES), n
    HOST.update(dict(zip(LAYOUT_NAMES, list(out)[:n])))
    # what the Mac-offset-free parts of the layout must satisfy
    assert HOST["b_flags"] == 8 and HOST["d_flags"] == 4 and HOST["d_x"] == 0xC and HOST["d_f30"] == 0x30 and HOST["i_b3c"] > 0x3C - 1
    for name, (lab, size) in BLK.items():
        HB[name] = lib.tpw_dmp_alloc(size)
        lib.tpw_dmp_setblock(lab, HB[name])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def wr_pylon(s, name, d):
    a = s.blocks[name]
    O = s.O
    s.words(a + O["d_flags"], [d["flags"]])
    s.words(a + O["d_x"], [d["x"]])
    s.words(a + O["d_z"], [d["z"]])
    s.words(a + O["d_f14"], [d["f14"]])
    s.words(a + O["d_f18"], [d["f18"]])
    s.words(a + O["d_f30"], [d["f30"]])
    s.setptr(a + O["d_owner"], "owner")


def wr_node(s, name, num, flags, prev):
    a = s.blocks[name]
    s.words(a, [num])
    s.words(a + s.O["b_flags"], [flags])
    s.setptr(a + s.O["b_prev"], prev)


def build(s, case):
    B = s.blocks
    O = s.O
    for name, (lab, size) in BLK.items():
        s.write(B[name], bytes(size))
    g = case["g"]
    for n in WORDS + FLOATS:
        s.words(s.g(n), [g[n]])
    for n, v in case["ptr"].items():
        s.setptr(s.g(n), v)
    s.words(s.g("rec") + O["r_rec"], [case["rec"]])
    s.words(B["edit"], [case["editflags"]])
    s.words(B["edit"] + O["i_b3c"], [case["b3c"]])
    b = case["brace"]
    a = B["brace"]
    s.words(a, [b["num"]])
    s.words(a + O["b_flags"], [b["flags"]])
    s.words(a + O["b_w70"], [b["w70"]])
    s.setptr(a + O["b_prev"], b["prev"])
    s.setptr(a + O["b_next"], b["next"])
    s.setptr(a + O["b_data"], "data")
    wr_pylon(s, "data", case["data"])
    wr_pylon(s, "pylon", case["pylon"])
    wr_pylon(s, "orig", case["data"])
    s.words(B["owner"] + O["o_f70"], [case["owner"]])
    m = case["marks"]
    if m["same"]:
        wr_node(s, "mark1", m["n1"], m["f1"], None)
    else:
        if m["n1"] <= m["n2"]:
            lo, hi = "mark1", "mark2"
        else:
            lo, hi = "mark2", "mark1"
        wr_node(s, "mark1", m["n1"], m["f1"], None)
        wr_node(s, "mark2", m["n2"], m["f2"], None)
        wr_node(s, "mid", 0, m["fm"], hi)
        wr_node(s, lo, m["n1"] if lo == "mark1" else m["n2"], m["f1"] if lo == "mark1" else m["f2"], "mid" if m["usemid"] else hi)


def execute(s, case):
    if case["op"] == "start":
        return s.call_start(case["args"][0])
    return s.call_do(*case["args"])


def observe(s, case, answer):
    O = s.O
    B = s.blocks
    o = {"answer": answer, "log": s.log()}
    for n in WORDS:
        o[n] = s.rd_word(s.g(n))
    for n in FLOATS:
        o[n] = canon_f(s.rd_word(s.g(n)))
    for n in PTRS:
        o[n] = s.pname(s.rptr(s.g(n)))
    o["editflags"] = s.rd_word(B["edit"])
    for n in ("mark1", "mark2", "mid", "brace"):
        o[n + "_flags"] = s.rd_word(B[n] + O["b_flags"])
    return o


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice([0, 0x80000000, fb(1.0), fb(-1.0), fb(3.0e9), fb(2147483648.0), fb(4294967296.0)])
    if k < 0.7:
        return fb(r.uniform(-5000, 5000))
    return fb(r.uniform(-1e6, 1e6))


def ri(r):
    return r.choice([0, 1, 100, r.randrange(0, 20000), (-r.randrange(1, 1000)) & MASK, r.getrandbits(32)])


def rw(r):
    return r.choice([0, 1, 2, 0xFFFFFFFF, r.getrandbits(32), r.randrange(0, 40)])


def pylon_rec(r):
    return {"flags": r.choice([0, 2, 1, 3, 4, r.getrandbits(32)]), "x": r.choice([0, 5, r.randrange(0, 300), 0xFFFFFFFF, 0xFFFFFFF0]),
            "z": r.choice([0, 7, r.randrange(0, 300), 0xFFFFFFFE]), "f14": rw(r), "f18": rw(r), "f30": r.choice([0, 1, 2, 3, 0xFFFFFFFF, 0x80000000])}


def base(r, op):
    data = pylon_rec(r)
    pylon = pylon_rec(r)
    if r.random() < 0.5:
        pylon["f14"], pylon["f18"] = data["f14"], data["f18"]
    c = {
        "op": op,
        "g": {"w16bc": rw(r), "pz": r.choice([0, 3, 7, 0xFFFFFFFF]), "px": r.choice([0, 3, 5, 0xFFFFFFFF]), "ext": rw(r),
              "e16e0": r.choice([2, 2, 2, 0, 1, 3]), "flags": r.getrandbits(32) | r.choice([0, 0x100]), "hh": r.choice([1, 0, 50, 256, r.randrange(1, 400)]),
              "hw": r.choice([1, 0, 60, 256, r.randrange(1, 400)]), "second": rf(r), "hgt": rf(r), "cost": rf(r)},
        "ptr": {"orig": r.choice([None, "orig"]), "mbrace": r.choice(["brace", "brace", "brace", None]), "mark1": r.choice([None, "mark1", "nextb"]),
                "mark2": None, "edit": "edit", "world": "world"},
        "rec": r.choice([0, 0, 0, 1, 77]), "editflags": r.getrandbits(32), "b3c": r.getrandbits(32),
        "brace": {"num": r.getrandbits(32), "flags": r.getrandbits(32), "w70": r.getrandbits(32), "prev": r.choice([None, "prevb"]),
                  "next": r.choice([None, "nextb"])},
        "data": data, "pylon": pylon, "owner": r.getrandbits(32),
        "marks": {"same": False, "n1": r.choice([0, 5, 9, r.getrandbits(32)]), "n2": r.choice([0, 5, 9, r.getrandbits(32)]),
                  "f1": r.getrandbits(32), "f2": r.getrandbits(32), "fm": r.getrandbits(32), "usemid": r.random() < 0.5},
        "queues": {"GetPylonAtCell": [r.choice([3, 3, 0])], "CalculateBraceCost": [rf(r), rf(r), rf(r)], "RoundUp": [ri(r), ri(r)],
                   "CalculateBraceHeight": [rf(r)], "MoveBrace_a": [rf(r)], "MoveBrace_b": [rf(r)]},
        "args": [],
    }
    if r.random() < 0.4:
        c["ptr"]["mark2"] = "mark2" if r.random() < 0.8 else "mark1"
        c["ptr"]["mark1"] = "mark1"
        if c["ptr"]["mark2"] == "mark1":
            c["marks"]["same"] = True
    return c


def do_case(r, **kw):
    c = base(r, "do")
    c["args"] = [r.choice([c["g"]["px"], r.randrange(0, 10), r.getrandbits(32)]), r.choice([c["g"]["pz"], r.randrange(0, 10), r.getrandbits(32)])]
    for k, v in kw.items():
        if k in ("x", "z"):
            c["args"][0 if k == "x" else 1] = v
        elif k in c["g"]:
            c["g"][k] = v
        elif k in c["ptr"]:
            c["ptr"][k] = v
        else:
            c[k] = v
    if c["ptr"]["mark2"] and not c["ptr"]["mark1"]:
        c["ptr"]["mark1"] = "mark1"
    return c


def do_edges():
    r = random.Random(5026)
    yield do_case(r, e16e0=0)
    yield do_case(r, e16e0=1, rec=1)
    yield do_case(r, e16e0=2, mbrace=None)
    yield do_case(r, e16e0=2, mbrace=None, rec=1)
    for rec in (0, 1):
        for b3c in (0, 1):
            for same in (True, False):
                for q in (3, 0):
                    c = do_case(r, e16e0=2, mbrace="brace", rec=rec, b3c=b3c)
                    c["queues"]["GetPylonAtCell"] = [q]
                    c["args"] = [c["g"]["px"], c["g"]["pz"]] if same else [c["g"]["px"] ^ 1, c["g"]["pz"]]
                    if same:
                        c["pylon"]["f14"], c["pylon"]["f18"] = c["data"]["f14"], c["data"]["f18"]
                    yield c
    for fl in (0, 2):
        for dx in (0, 1):
            c = do_case(r, e16e0=2, mbrace="brace", rec=0)
            c["queues"]["GetPylonAtCell"] = [3]
            c["pylon"]["f14"], c["pylon"]["f18"] = c["data"]["f14"], c["data"]["f18"] ^ dx
            c["pylon"]["flags"] = fl
            yield c
            c = do_case(r, e16e0=2, mbrace="brace", rec=0)
            c["queues"]["GetPylonAtCell"] = [3]
            c["pylon"]["f14"], c["pylon"]["f18"] = c["data"]["f14"] ^ dx, c["data"]["f18"]
            c["pylon"]["flags"] = fl
            yield c
    for same in (False, True):
        for usemid in (False, True):
            for n1, n2 in ((1, 2), (2, 1), (4, 4)):
                c = do_case(r, e16e0=2, mbrace="brace", mark1="mark1", mark2="mark1" if same else "mark2", rec=0)
                c["marks"].update(same=same, usemid=usemid, n1=n1, n2=n2)
                yield c


def do_random(r):
    return do_case(r)


def start_case(r, **kw):
    c = base(r, "start")
    c["ptr"]["mbrace"] = r.choice([None, "brace"])
    c["ptr"]["orig"] = r.choice([None, "orig"])
    c["args"] = ["brace" if r.random() < 0.9 else None]
    c["data"]["x"] = r.choice([0, 5, 59, 60, 61, 0xFFFFFFFF, 0x80000000, r.randrange(0, 500), 0x7FFFFFFF])
    c["data"]["z"] = r.choice([0, 7, 49, 50, 51, 0xFFFFFFF0, r.randrange(0, 500), 0x7FFFFFFF])
    for k, v in kw.items():
        if k in c["data"]:
            c["data"][k] = v
        elif k in c["g"]:
            c["g"][k] = v
        elif k in c["brace"]:
            c["brace"][k] = v
        else:
            c[k] = v
    return c


def start_edges():
    r = random.Random(5027)
    yield start_case(r, args=[None])
    for fl in (0, 2, 1, 3):
        for f30 in (0, 1, 2, 0xFFFFFFFF):
            yield start_case(r, args=["brace"], flags=fl, f30=f30)
    for prev in (None, "prevb"):
        for nxt in (None, "nextb"):
            yield start_case(r, args=["brace"], flags=0, prev=prev, next=nxt)
    for hw, hh in ((60, 50), (1, 1), (0, 0), (100, 100)):
        for x, z in ((0, 0), (59, 49), (60, 50), (61, 51), (0xFFFFFFFF, 0xFFFFFFFF), (0x7FFFFFFF, 0x7FFFFFFF), (0x80000000, 0x80000000), (5, 7)):
            yield start_case(r, args=["brace"], flags=0, hw=hw, hh=hh, x=x, z=z)


def start_random(r):
    return start_case(r)


TESTS = [
    FnTest("StartMovePylon", ppc, host, edge=start_edges, rnd=start_random),
    FnTest("DoMovePylon", ppc, host, edge=do_edges, rnd=do_random),
]
