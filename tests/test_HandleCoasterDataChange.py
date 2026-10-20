"""Unit v5054, the coaster editor's data change and the loft drag toggle, against libtpw_unit_HandleCoasterDataChange.dylib
(tests/unit_HandleCoasterDataChange.cpp), main app ("SimTheme Park"), src/HandleCoasterDataChange.cpp:
  HandleCoasterDataChange 0x10052318, ToggleLoftPylonState 0x10052ac4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers, so its own field offsets) for the translation. Every
callee outside the unit (GetHeightForInterval, DoImmediatePhysicsFeedback, FlagGenerateTrack, SetupBraceAnimFrameData,
UpdatePylonStackMeshes, FlagCollisionUpdateTrackAroundBrace, RecordAction, CalculateBraceCost, GetIntervalForHeight,
GetIntervalForHeightDelta, CalculateBraceHeight, LockPointedAtRide, RoundUp, GetBankAccount, MakeDeposit, GetBalance, MakeWithdrawal) is a
fake on both sides that records its arguments (objects as labels, floats as bits) and answers from the case. The runtime's double to
unsigned conversion (0x10237cd8) runs as the original; the cost before the drag is kept in [0, 2^31).

The world of a case: the edit instance, the dragged brace, a neighbour brace, a second brace in the pylon's brace array, the pylon record
and its control, the globals of the drag. What is compared: the answer, the globals and fields the functions write, the event log.

No fused operations. Not tested: a NULL dragged brace with the drag flag set (the original reads it unconditionally), a pylon brace
index above 3.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_HandleCoasterDataChange"
MASK = 0xFFFFFFFF

FN_CHANGE, FN_TOGGLE = 0x10052318, 0x10052ac4
SIZES = {FN_CHANGE: 300, FN_TOGGLE: 876}
OWN = set(SIZES)

CALLEES = dict(
    GetHeightForInterval=0x10040eb8, DoImmediatePhysicsFeedback=0x1004d2ec, FlagGenerateTrack=0x10040f08,
    SetupBraceAnimFrameData=0x1003bb44, UpdatePylonStackMeshes=0x1003d958, FlagCollisionUpdateTrackAroundBrace=0x10040fa4,
    RecordAction=0x10144704, CalculateBraceCost=0x10058924, GetIntervalForHeight=0x10040e10,
    GetIntervalForHeightDelta=0x10040e64, CalculateBraceHeight=0x1003e6e0, LockPointedAtRide=0x10067288, RoundUp=0x1004da1c,
    GetBankAccount=0x1013edfc, MakeDeposit=0x100f32ec, GetBalance=0x100f32b4, MakeWithdrawal=0x100f33b0)
FLOAT_CONVERT = 0x10237cd8

ADDR = dict(edit=0x102b68b0, input=0x102c1794, drag=0x102c178c, anchor=0x102c16ec, scaley=0x102c16f8, lastdy=0x102c16e8,
            vt1=0x102c1768, vt2=0x102c1764, ht1=0x102c1760, ht2=0x102c175c, base1=0x102c1748, min1=0x102c1758, max1=0x102c1750,
            base2=0x102c1744, cost=0x102c1770, ptrxy=0x102b5a94, world=0x1037ebfc, recorder=0x1037ec44, floor=0x1029ce80)

# names of the host layout export, in order
LAYOUT_NAMES = ["inst_flags", "inst_type", "inst_f40", "inst_prev", "inst_f118", "inst_f11c", "inst_size",
                "br_num", "br_flags", "br_prev", "br_next", "br_fd8", "br_fe8", "br_data", "br_size",
                "py_flags", "py_ctl", "py_count", "py_braces", "ct_flags", "ct_f68", "ct_f6c", "ct_size", "rec_rec", "rec_size"]
MAC_OFF = dict(inst_flags=0, inst_type=4, inst_f40=0x40, inst_prev=0x60, inst_f118=0x118, inst_f11c=0x11c,
               br_num=0, br_flags=8, br_prev=0x64, br_next=0x68, br_fd8=0xd8, br_fe8=0xe8, br_data=0x100,
               py_flags=4, py_ctl=0x18, py_count=0x30, py_braces=0x34, ct_flags=0, ct_f68=0x68, ct_f6c=0x6c, rec_rec=0x24)
SAME = ["inst_flags", "br_num", "br_flags", "py_flags", "py_ctl", "ct_flags", "ct_f68", "ct_f6c"]
HOFF = {}

# the objects of a case: name -> (label, size)
OBJECTS = dict(inst=(1, 0x400), brace=(2, 0x300), b2=(3, 0x300), nbr=(4, 0x300), pyl=(5, 0x100), ctl=(6, 0x200), bank=(7, 0x40),
               world=(8, 0x40), type=(9, 0x40))
REC_LABEL = 10
CUR = {}


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
STUBS.real.add(FLOAT_CONVERT)


def ffb(e, n=1):
    return canon_f(fb(e.f(n)))


def ret_f(e, key):
    e.set_f1(bitsf(e.cl.ret(key)))


def st_get_height(e):
    e.cl.rec("GetHeightForInterval", (ffb(e), e.cl.lab(e.r(4))))
    ret_f(e, "GetHeightForInterval")


def st_physics(e):
    e.cl.rec("DoImmediatePhysicsFeedback", (e.cl.lab(e.r(3)),))


def st_flag_gen(e):
    e.cl.rec("FlagGenerateTrack", (e.cl.lab(e.r(3)), s32(e.r(4))))


def st_setup(e):
    e.cl.rec("SetupBraceAnimFrameData", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def st_stack(e):
    e.cl.rec("UpdatePylonStackMeshes", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), s32(e.r(5)), s32(e.r(6))))


def st_flag_coll(e):
    e.cl.rec("FlagCollisionUpdateTrackAroundBrace", (e.cl.lab(e.r(3)),))


def st_record(e):
    e.cl.rec("RecordAction", (e.cl.lab(e.r(3)), s32(e.r(4)), ffb(e), e.r(6) & 0xFFFF))


def st_cost(e):
    L = e.cl
    L.rec("CalculateBraceCost", (L.lab(e.r(3)), s32(e.r(4)), L.lab(e.r(5)), L.lab(e.r(6)), L.lab(e.r(7)), L.lab(e.r(8)), L.lab(e.r(9))))
    ret_f(e, "CalculateBraceCost")


def st_ifh(e):
    e.cl.rec("GetIntervalForHeight", (ffb(e), e.cl.lab(e.r(4))))
    ret_f(e, "GetIntervalForHeight")


def st_ifhd(e):
    e.cl.rec("GetIntervalForHeightDelta", (ffb(e), e.cl.lab(e.r(4))))
    ret_f(e, "GetIntervalForHeightDelta")


def st_height(e):
    e.cl.rec("CalculateBraceHeight", (e.cl.lab(e.r(3)),))
    ret_f(e, "CalculateBraceHeight")


def st_lock(e):
    e.cl.rec("LockPointedAtRide", (s32(e.r(3)),))


def st_roundup(e):
    e.cl.rec("RoundUp", (ffb(e),))
    e.set_r(3, e.cl.ret("RoundUp") & MASK)


def st_bank(e):
    e.cl.rec("GetBankAccount", (e.cl.lab(e.r(3)),))
    e.set_r(3, CUR["bank"])


def st_deposit(e):
    e.cl.rec("MakeDeposit", (e.cl.lab(e.r(3)), e.r(4) & MASK))


def st_balance(e):
    e.cl.rec("GetBalance", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


def st_withdraw(e):
    e.cl.rec("MakeWithdrawal", (e.cl.lab(e.r(3)), e.r(4) & MASK))


for _name, _fn in (("GetHeightForInterval", st_get_height), ("DoImmediatePhysicsFeedback", st_physics),
                   ("FlagGenerateTrack", st_flag_gen), ("SetupBraceAnimFrameData", st_setup),
                   ("UpdatePylonStackMeshes", st_stack), ("FlagCollisionUpdateTrackAroundBrace", st_flag_coll),
                   ("RecordAction", st_record), ("CalculateBraceCost", st_cost), ("GetIntervalForHeight", st_ifh),
                   ("GetIntervalForHeightDelta", st_ifhd), ("CalculateBraceHeight", st_height), ("LockPointedAtRide", st_lock),
                   ("RoundUp", st_roundup), ("GetBankAccount", st_bank), ("MakeDeposit", st_deposit),
                   ("GetBalance", st_balance), ("MakeWithdrawal", st_withdraw)):
    STUBS.internal(CALLEES[_name], _fn)


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

    def off(self, name):
        return self.offsets[name]

    def put(self, obj, name, words):
        self.words(self.o[obj] + self.off(name), words)

    def getw(self, obj, name):
        return self.rd_word(self.o[obj] + self.off(name))

    def setp(self, a, value):
        self.write(a, struct.pack(self.pfmt, value))

    def getp(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.psize))[0]

    def objp(self, obj, name, target):
        self.setp(self.o[obj] + self.off(name), self.o[target] if target else 0)

    def make_objects(self):
        self.o = {}
        for name, (label, size) in OBJECTS.items():
            a = self.alloc(size)
            self.write(a, bytes(size))
            self.o[name] = a


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    psize = 4
    offsets = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        CUR["bank"] = self.o["bank"]
        fixed = {self.o[n]: OBJECTS[n][0] for n in OBJECTS}
        fixed[ADDR["recorder"]] = REC_LABEL
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

    def call_change(self, flags, brace, inst, a, b):
        T.call_ppc(self.e, FN_CHANGE, [flags & MASK, self.o[brace], self.o[inst], a & MASK, b & MASK])
        return 0

    def call_toggle(self):
        r3, _, _ = T.call_ppc(self.e, FN_TOGGLE, [])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.offsets = HOFF
        self.make_objects()
        lib.tpw_hcdc_setbank(self.o["bank"])
        fixed = {OBJECTS[n][0]: self.o[n] for n in OBJECTS}
        fixed[REC_LABEL] = self.g("recorder")
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hcdc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_hcdc_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call_change(self, flags, brace, inst, a, b):
        self.lib.tpw_hcdc_change(flags & MASK, self.o[brace], self.o[inst], s32(a), s32(b))
        return 0

    def call_toggle(self):
        return self.lib.tpw_hcdc_toggle() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_hcdc_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "setbank").argtypes = [C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "change").argtypes = [C.c_uint32, C.c_uint64, C.c_uint64, C.c_int32, C.c_int32]
    getattr(lib, p + "toggle").restype = C.c_int32
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert n == len(LAYOUT_NAMES), n
    for name, v in zip(LAYOUT_NAMES, list(out)[:n]):
        HOFF[name] = v
    for name in SAME:
        assert HOFF[name] == MAC_OFF[name], (name, HOFF[name])
    assert HOFF["inst_size"] <= OBJECTS["inst"][1] and HOFF["br_size"] <= OBJECTS["brace"][1]
    assert HOFF["ct_size"] <= OBJECTS["ctl"][1]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    # instance
    s.put("inst", "inst_flags", [c["iflags"]])
    s.put("inst", "inst_f40", [c["f40"]])
    s.objp("inst", "inst_type", "type" if c["type_set"] else None)
    s.objp("inst", "inst_prev", {0: None, 1: "brace", 2: "nbr"}[c["prev_sel"]])
    s.put("inst", "inst_f118", [c["f118"]])
    s.put("inst", "inst_f11c", [c["f11c"]])
    # braces
    for name, num, flags, fd8, fe8 in (("brace", c["num"], c["bflags"], c["fd8"], c["fe8"]), ("nbr", c["nnum"], 0, 0, c["nfe8"])):
        s.put(name, "br_num", [num])
        s.put(name, "br_flags", [flags])
        s.put(name, "br_fd8", [fd8])
        s.put(name, "br_fe8", [fe8])
    s.objp("brace", "br_data", "pyl")
    s.objp("nbr", "br_data", "pyl" if c["ndata"] else None)
    s.objp("brace", "br_prev", "nbr" if c["links"] & 1 else None)
    s.objp("brace", "br_next", "nbr" if c["links"] & 2 else None)
    # pylon record and control
    s.put("pyl", "py_flags", [c["pflags"]])
    s.objp("pyl", "py_ctl", "ctl")
    s.put("pyl", "py_count", [c["idx"]])
    for i, sel in enumerate(c["arr"]):
        s.setp(s.o["pyl"] + s.off("py_braces") + s.psize * i, s.o[{0: "b2", 1: "brace"}[sel]] if sel != 2 else 0)
    s.put("ctl", "ct_flags", [c["cflags"]])
    s.put("ctl", "ct_f68", [c["f68"]])
    s.put("ctl", "ct_f6c", [c["f6c"]])
    # globals
    s.setp(s.g("edit"), s.o["inst"] if c["edit_set"] else 0)
    s.words(s.g("input"), [c["input"]])
    s.setp(s.g("drag"), s.o["brace"] if c["drag_set"] else 0)
    s.words(s.g("anchor"), c["anchor"])
    s.words(s.g("scaley"), [c["scaley"]])
    s.words(s.g("lastdy"), [c["lastdy"]])
    for n in ("vt1", "vt2", "ht1", "ht2"):
        s.setp(s.g(n), 0)
    s.words(s.g("base1"), [c["base1"]])
    s.words(s.g("min1"), [c["min1"]])
    s.words(s.g("max1"), [c["max1"]])
    s.words(s.g("base2"), [c["base2"]])
    s.words(s.g("cost"), [c["cost"]])
    s.words(s.g("ptrxy"), c["ptrxy"])
    s.words(s.g("floor"), [c["floor"]])
    s.setp(s.g("world"), s.o["world"])
    s.words(s.g("recorder") + s.off("rec_rec"), [c["recording"]])


def execute(s, c):
    if c["op"] == "change":
        return s.call_change(*c["args"])
    return s.call_toggle()


def plabel(s, p):
    if p == 0:
        return 0
    if p == s.o["brace"] + s.off("br_fe8"):
        return 1
    return 2


def observe(s, c, answer):
    d = {"answer": answer, "log": s.log(),
         "iflags": s.getw("inst", "inst_flags"), "f118": canon_f(s.getw("inst", "inst_f118")), "f11c": canon_f(s.getw("inst", "inst_f11c")),
         "input": s.rd_word(s.g("input")), "bflags": s.getw("brace", "br_flags"),
         "anchor": s.rd_words(s.g("anchor"), 2)}
    for n in ("scaley", "lastdy", "base1", "min1", "max1", "base2", "cost", "floor"):
        d[n] = canon_f(s.rd_word(s.g(n)))
    for n in ("vt1", "vt2", "ht1", "ht2"):
        d[n] = plabel(s, s.getp(s.g(n)))
    d["fe8"] = s.getw("brace", "br_fe8")
    d["recording"] = s.rd_word(s.g("recorder") + s.off("rec_rec"))
    return d


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, execute(s, c))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rf(r):
    k = r.random()
    if k < 0.08:
        return r.choice(SPECIAL)
    if k < 0.6:
        return fb(r.uniform(-8, 8))
    if k < 0.9:
        return fb(r.uniform(-300, 300))
    return fb(r.uniform(-1, 1) * 10 ** r.uniform(-8, 8))


def rcost(r):
    k = r.random()
    if k < 0.15:
        return 0
    if k < 0.6:
        return fb(r.uniform(0, 1e5))
    if k < 0.9:
        return fb(float(r.randrange(0, 100000)))
    return r.choice([fb(1.0), fb(0.5), fb(2147483520.0), fb(0.999)])


def base(r, op):
    return {
        "op": op,
        "queues": {"GetHeightForInterval": [rf(r)], "CalculateBraceCost": [rf(r)], "GetIntervalForHeight": [rf(r)],
                   "GetIntervalForHeightDelta": [rf(r)], "CalculateBraceHeight": [rf(r)], "RoundUp": [r.randrange(0, 1 << 31)]},
        "iflags": r.choice([0, 0x800, 0x2000, 0x2800, r.getrandbits(32)]), "f40": r.choice([0, 1, r.getrandbits(32)]),
        "type_set": r.random() < 0.8, "prev_sel": r.choice([0, 1, 2]), "f118": rf(r), "f11c": rf(r),
        "num": r.getrandbits(32), "bflags": r.choice([0, 0x40, 0x80, 0xc0, 0x40, r.getrandbits(32)]), "fd8": rf(r), "fe8": rf(r),
        "nnum": r.getrandbits(32), "nfe8": rf(r), "ndata": r.choice([0, 1]), "links": r.randrange(4),
        "pflags": r.choice([0, 2, 1, 3, r.getrandbits(32)]), "idx": r.randrange(4), "arr": [r.randrange(3) for _ in range(3)],
        "cflags": r.choice([0, 0x20, 0x4000, 0x4020, r.getrandbits(32)]), "f68": rf(r), "f6c": rf(r),
        "edit_set": r.random() < 0.9, "input": r.choice([0, 1, 0x40, 0x3f, 0x3e00, 0x2001, 3, 4, 0x1e00, 0x401, 0x3e01, r.getrandbits(32)]),
        "drag_set": r.random() < 0.9, "anchor": [r.getrandbits(32), r.getrandbits(32)], "scaley": rf(r), "lastdy": rf(r),
        "base1": rf(r), "min1": rf(r), "max1": rf(r), "base2": rf(r), "cost": rcost(r), "ptrxy": [rf(r), rf(r)],
        "floor": rf(r), "recording": r.choice([0, 1, 2]),
    }


def fix_toggle(c):
    """The original reads the dragged brace unconditionally when the drag flag is set: keep the brace there."""
    if (c["input"] & 1) or c["drag_set"] is False and False:
        c["drag_set"] = True
    return c


# ---- HandleCoasterDataChange
def change_case(r, flags=None, prev=None, iflags=None, bflags=None, cflags=None, b=None, fe8=None):
    c = base(r, "change")
    if flags is None:
        flags = r.choice([0, 1, 4, 5, 2, 3, 6, 7, 0x10, r.getrandbits(32)])
    if prev is not None:
        c["prev_sel"] = prev
    if iflags is not None:
        c["iflags"] = iflags
    if bflags is not None:
        c["bflags"] = bflags
    if cflags is not None:
        c["cflags"] = cflags
    if fe8 is not None:
        c["fe8"] = fe8
    else:
        c["fe8"] = r.choice([fb(-1.0), 0x80000000, 0, fb(0.5), fb(1.0), fb(2.0), 0x7FC00000, fb(0.999), fb(1.0001), rf(r)])
    c["args"] = [flags, "brace", "inst", r.getrandbits(32), r.choice([0, 1, MASK, r.getrandbits(32)]) if b is None else b]
    return c


def change_edges():
    r = random.Random(5054)
    for flags in (0, 1, 4, 5, 2, 3):
        for prev in (0, 1, 2):
            for iflags in (0, 0x800):
                for bflags in (0, 0x40):
                    for cflags in (0, 0x4000):
                        for b in (0, 1):
                            yield change_case(r, flags, prev, iflags, bflags, cflags, b)
    for fe8 in (fb(-1.0), 0x80000000, 0, fb(1.0), fb(1.0000001), 0x7FC00000, 0x7F800000, 0xFF800000, fb(0.5)):
        yield change_case(r, 1, 1, 0, 0, 0, 0, fe8)


def change_random(r):
    return change_case(r)


# ---- ToggleLoftPylonState
def toggle_case(r, edit=None, f40=None, inp=None, cflags=None, pflags=None, bflags=None, rec=None, drag=None):
    c = base(r, "toggle")
    c["args"] = []
    if edit is not None:
        c["edit_set"] = edit
    if f40 is not None:
        c["f40"] = f40
    elif c["edit_set"]:
        c["f40"] = r.choice([1, 1, 1, 0, 5])
    if inp is not None:
        c["input"] = inp
    if cflags is not None:
        c["cflags"] = cflags
    if pflags is not None:
        c["pflags"] = pflags
    if bflags is not None:
        c["bflags"] = bflags
    if rec is not None:
        c["recording"] = rec
    if drag is not None:
        c["drag_set"] = drag
    if c["input"] & 1:
        c["drag_set"] = True
    return c


def toggle_edges():
    r = random.Random(5055)
    yield toggle_case(r, edit=False)
    yield toggle_case(r, edit=True, f40=0)
    yield toggle_case(r, edit=True, f40=1, inp=0, drag=False)
    yield toggle_case(r, edit=True, f40=1, inp=4, drag=False)
    for inp in (0, 1, 4, 0x3f, 0x3e00, 0x3e01, 0x400, 0x2001):
        for cflags in (0, 0x20):
            for pflags in (0, 2):
                for bflags in (0, 0x40, 0xc0):
                    for rec in (0, 1):
                        yield toggle_case(r, edit=True, f40=1, inp=inp, cflags=cflags, pflags=pflags, bflags=bflags, rec=rec, drag=True)


def toggle_random(r):
    return toggle_case(r)


TESTS = [
    FnTest("HandleCoasterDataChange", ppc, host, edge=change_edges, rnd=change_random),
    FnTest("ToggleLoftPylonState", ppc, host, edge=toggle_edges, rnd=toggle_random),
]
