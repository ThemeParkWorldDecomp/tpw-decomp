"""Unit v51052, small functions, against libtpw_unit_CStaffHQ_PickUpStaff.dylib (tests/unit_CStaffHQ_PickUpStaff.cpp), main app
("SimTheme Park"), src/ParkMessages_CStaffHQ_DropStaff_CStaffHQ_Serialise_CStaffHQ_PickUpStaff.cpp: the 23 functions of FNS below.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (CThing::FinalDestroy,
CThing::Serialise, SCR_KillScript, operator delete, the wide string constructor, __register_global_object) are fakes on both sides that
record their arguments; CThing::Serialise answers from the case's queue.

The world of a case: random object words, the eight static initialiser words, an optional NULL object. What is compared: the answer, the
event log, the whole object (every word, the halfword of the staff HQ) and the eight globals. The Mac's destructors also store vtable
words at +0 (checked on the PowerPC side against the expected constant, since the host classes are not built from those vtables), so the
comparison of the object skips word 0 of those. No fused operations. Not tested: nothing skipped.
"""
import random
import struct
import ctypes as C

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CStaffHQ_PickUpStaff"
MASK = 0xFFFFFFFF

# (name, address, size, kind, object bytes, vptr stored by the original)
FNS = [
    ("CStaffHQ::PickUpStaff", 0x1012c100, 16, "void", 0x68, None),
    ("CStaffHQ::StopStrikeInProgress", 0x1012be24, 20, "void", 0x68, None),
    ("CStaffHQ::IsStriking", 0x1012bde0, 16, "int", 0x68, None),
    ("CStaffHQ::FinalDestroy", 0x1012a998, 36, "void", 0x68, None),
    ("__sinit_StaffHQ_cpp", 0x1012c538, 28, "void", 0, None),
    ("CTagSystem::FinalDestroy", 0x1012c5f8, 36, "void", 0x10, None),
    ("CTagSystem::Serialise", 0x1012c64c, 56, "int", 0x10, None),
    ("__sinit_Thing_cpp", 0x1012ec48, 28, "void", 0, None),
    ("CThoughts::EventHistType::EventHistType", 0x1012ed48, 4, "void", 0x10, None),
    ("CThoughts::FinalDestroy", 0x1012ed84, 48, "void", 0x90, None),
    ("__sinit_TrainingCentre_cpp", 0x10130564, 28, "void", 0, None),
    ("CUIMsgReceiver::FinalDestroy", 0x101308d0, 36, "void", 0x10, None),
    ("CUIMsgReceiver::Serialise", 0x10130928, 56, "int", 0x10, None),
    ("Vector2d::~Vector2d", 0x10130db4, 68, "ptr", 0x10, None),
    ("__sinit_vector2d_cpp", 0x10130e70, 28, "void", 0, None),
    ("__sinit_ResearchLab_cpp", 0x101247c4, 60, "void", 0, None),
    ("CMsgResearchCategoryUpdated::GetType", 0x10124510, 8, "int", 0x10, None),
    ("CMsgResearchCategoryUpdated::~CMsgResearchCategoryUpdated", 0x1012344c, 76, "ptr", 0x10, 0x102a113c),
    ("Follow_Path::~Follow_Path", 0x101351ec, 88, "ptr", 0x40, 0x102a4590),
    ("Avoid_Walls::~Avoid_Walls", 0x1013526c, 88, "ptr", 0x40, 0x102a4590),
    ("Repel_Obstacle::~Repel_Obstacle", 0x101352ec, 88, "ptr", 0x40, 0x102a4590),
    ("Avoid_Obstacle::~Avoid_Obstacle", 0x10135370, 88, "ptr", 0x40, 0x102a4590),
    ("Separation::~Separation", 0x101353f4, 88, "ptr", 0x40, 0x102a4590),
]
OWN = {f[1]: f[2] for f in FNS}
DTORS = {13, 17, 18, 19, 20, 21, 22}
SERIALISE = {6, 12}

CTHING_FD, CTHING_SER, KILL, DELETE, STRCTOR, REGISTER = 0x1012e9f8, 0x1012e010, 0x100d9478, 0x10238218, 0x10238800, 0x10239940
ADDR = dict(staffH=0x1034c6e8, staffL=0x1034c6ec, thingH=0x1034c6f0, thingL=0x1034c6f4, trainH=0x1034c6f8, trainL=0x1034c6fc,
            vecH=0x1034c700, vecL=0x1034c704, rlstr=0x1034c4b8, rlnode=0x1034c4cc, glue=0x103bcbb8)
GLOBALS = ["staffH", "staffL", "thingH", "thingL", "trainH", "trainL", "vecH", "vecL"]
HOST_LAYOUT = [0x24, 0x60, 0x64, 0x68]

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(CTHING_FD, CL.recorder("CThingFinalDestroy", "L"))
STUBS.internal(CTHING_SER, CL.recorder("CThingSerialise", "LLiiu", ret="i"))
STUBS.internal(KILL, CL.recorder("SCR_KillScript", "i"))


def glue(fn):
    """An import reached through glue: the glue saves the caller's TOC pointer at 0x14(r1) (the listing reloads it afterwards)."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        return fn(e)
    return wrapped


STUBS.internal(DELETE, glue(CL.recorder("OperatorDelete", "L")))
STUBS.internal(STRCTOR, glue(CL.recorder("StrCtor", "LS", ret=lambda e, log: e.r(3))))
STUBS.internal(REGISTER, glue(CL.recorder("RegisterGlobal", "LLL")))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def h16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_h16(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {ADDR["rlstr"]: 1, ADDR["glue"]: 2, ADDR["rlnode"]: 3} if case["fn"] == 15 else {}
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

    def call(self, fn, args):
        return T.call_ppc(self.e, FNS[fn][1], [a & MASK for a in args])[0] & MASK

    def check(self, case):
        vp = FNS[case["fn"]][5]
        if vp is not None and self.obj:
            assert self.rd_word(self.obj) == vp, hex(self.rd_word(self.obj))


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {1: self.g("rlstr"), 2: self.g("glue"), 3: self.g("rlnode")} if case["fn"] == 15 else {}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v51052_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v51052_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, fn, args):
        a = [x & 0xFFFFFFFF for x in args] + [0] * 5
        a[0] = args[0] if args else 0     # the object is a full host pointer
        if fn in SERIALISE:
            a[1] = args[1]
        return self.lib.tpw_v51052_call(fn, *a[:5])

    def check(self, case):
        pass


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v51052_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "call").restype = C.c_uint64
    getattr(lib, p + "call").argtypes = [C.c_int32] + [C.c_uint64] * 5
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    for n, v in zip(GLOBALS, case["g"]):
        s.words(s.g(n), [v])
    s.obj = 0
    s.file = 0
    fn = case["fn"]
    size = FNS[fn][4]
    if size and not case["null"]:
        s.obj = s.alloc(size)
        if fn <= 3:
            s.words(s.obj, case["words"][:24])
            s.h16(s.obj + 0x60, case["h60"])
            s.words(s.obj + 0x64, [case["words"][25]])
        else:
            s.words(s.obj, case["words"][:size // 4])
    if fn in SERIALISE and case["file"]:
        s.file = s.alloc(16)


def execute(s, case):
    fn = case["fn"]
    a = case.get("a", [])
    if fn == 0:
        r = s.call(fn, [s.obj, a[0] << 16 if isinstance(s, Ppc) else a[0]])
    elif fn in SERIALISE:
        r = s.call(fn, [s.obj, s.file] + a)
    elif fn in DTORS:
        r = s.call(fn, [s.obj] + a)
    elif fn in (1, 2):
        r = s.call(fn, [s.obj, a[0]])
    elif FNS[fn][4]:
        r = s.call(fn, [s.obj])
    else:
        r = s.call(fn, [])
    kind = FNS[fn][3]
    if kind == "void":
        return None
    if kind == "ptr":
        return r == s.obj
    return r & MASK


def observe(s, case, ans):
    fn = case["fn"]
    out = {"ans": ans, "log": s.log(), "g": [s.rd_word(s.g(n)) for n in GLOBALS]}
    size = FNS[fn][4]
    if s.obj:
        first = 1 if FNS[fn][5] is not None else 0
        if fn <= 3:
            out["words"] = s.rd_words(s.obj, 24)
            out["h60"] = s.rd_h16(s.obj + 0x60)
            out["w64"] = s.rd_word(s.obj + 0x64)
        else:
            out["words"] = s.rd_words(s.obj + 4 * first, size // 4 - first)
    return out


def run(s, case):
    try:
        build(s, case)
        ans = execute(s, case)
        s.check(case)
        return observe(s, case, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def mk(r, fn, **over):
    size = FNS[fn][4]
    c = {"fn": fn, "g": [r.getrandbits(32) for _ in GLOBALS], "null": False, "file": r.choice([0, 1]),
         "h60": r.getrandbits(16), "words": [r.getrandbits(32) for _ in range(max(size // 4, 26))],
         "queues": {"CThingSerialise": [r.choice([0, 1, 2, MASK, 0x100, 0x10000, r.getrandbits(32)])]}}
    if fn == 0:
        c["a"] = [r.choice([0, 1, 0xFFFF, 0x8000, r.getrandbits(16)])]
    elif fn in (1, 2):
        c["a"] = [r.randint(-3, 5) & MASK]
    elif fn in SERIALISE:
        c["a"] = [r.choice([0, 1, MASK, r.getrandbits(32)]) for _ in range(3)]
    elif fn in DTORS:
        c["a"] = [r.choice([0, 1, 2, MASK, 0x7FFF, 0x8000, 0xFFFF, 0x10001, 0x18000, r.getrandbits(32)])]
        c["null"] = r.random() < 0.15
    elif fn == 9:
        c["words"][0x84 // 4] = r.choice([0, 1, MASK, 0x80000000, r.getrandbits(32)])
    c.update(over)
    return c


def edges(fn):
    r = random.Random(1000 + fn)
    if fn in DTORS:
        for flag in (0, 1, 2, MASK, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x10001, 0x18000):
            for null in (False, True):
                yield mk(r, fn, a=[flag], null=null)
    elif fn == 9:
        for h in (0, 1, MASK, 0x80000000):
            c = mk(r, fn)
            c["words"][0x84 // 4] = h
            yield c
    elif fn in SERIALISE:
        for ret in (0, 1, 2, MASK):
            for f in (0, 1):
                yield mk(r, fn, file=f, queues={"CThingSerialise": [ret]})
    elif fn in (1, 2):
        for t in range(-3, 6):
            yield mk(r, fn, a=[t & MASK])
    elif fn == 0:
        for i in (0, 1, 0xFFFF, 0x8000):
            yield mk(r, fn, a=[i])
    else:
        yield mk(r, fn)


TESTS = [FnTest(FNS[i][0], ppc, host, edge=(lambda i=i: edges(i)), rnd=(lambda r, i=i: mk(r, i))) for i in range(len(FNS))]
