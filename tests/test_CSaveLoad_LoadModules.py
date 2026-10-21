"""Unit v5206, CSaveLoad::LoadModules, against libtpw_unit_CSaveLoad_LoadModules.dylib (tests/unit_CSaveLoad_LoadModules.cpp), main app
("SimTheme Park"), src/game/save/SaveLoad.cpp:
  CSaveLoad::LoadModules 0x101589f0 (4,052 bytes)

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. Every callee is a fake on both sides that records its arguments
(the file handle, the module objects and the global gSaveLoad are labels) and answers from the case's queues: the position of the file
(GetPosition), the tag and the transferred count of each LbFile_Read (ReadTag, ReadCount: the file's bytes of a 4 byte read), and the
answer of every module loader. The Mac's operator new and __construct_array (the message centre made on first use) log the 29 header
constructions, as the host's fake CSubscriptionHeader constructor does; the shared scoring singleton behind TheScoring() is the unit
library's fake, which logs the constructor and the registration the Mac code inlines behind the guard byte 0x102b4100 (so the guard
byte compared here is the fake's).

The world of a case: the load type, gGameWorld (a block), gMessageCentre (null or a block), the guard bytes of the static CCheat and of
the scoring singleton, and the queues. What is compared: the answer, the event log, gMessageCentre as a label (null, the block it
held, a new object), both guard bytes. UDebug is varargs and the host fake cannot read its text; the PowerPC stub logs an extra
event unless the text is "read failed".

No fused operations. Not tested: an operator new that answers NULL (the original never gets one).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CSaveLoad_LoadModules"
MASK = 0xFFFFFFFF

FN = 0x101589F0
SIZES = {FN: 4052}
OWN = set(SIZES)

ADDR = dict(gSaveLoad=0x1037FEFC, gActionRecorder=0x1037EC44, gClock=0x1037F048, gVanillaTime=0x10380A04, gUIInterface=0x10396440,
            gGameWorld=0x1037EBFC, gMessageCentre=0x1037F6C4, gGameWinCheat=0x102DA934, gGameWinCheatGuard=0x102DA940,
            gScoringStorage=0x102B3AC8, gScoringGuard=0x102B4100)
HOST_LAYOUT = [0x1D0, 12]

# the module tags, in the order the function reads them (the file's little endian words)
TAGS = [0x57524C44, 0x53505343, 0x50415254, 0x4D455353, 0x434C4F4B, 0x56414E54, 0x47535953, 0x52535953, 0x5452414B, 0x464C5952,
        0x52535345, 0x4B414D45, 0x434F4153, 0x41445653, 0x534F554E, 0x43485453, 0x41445343]

# answer keys: (value that goes on, "nz" when a nonzero answer is success)
OK_NONZERO = ["CWorld_Load", "CClock_Load", "CVanillaTime_Load", "GAMESYSTEM_Load", "LoadRides", "BMP_Load", "LoadCamera",
              "LoadCoasters", "CCheat_Load", "CAdvisorScoring_Load"]
OK_ZERO = ["ActionSerialise", "SCR_LoadAllScripts", "PAR_LoadStatus", "CMessageCentre_Load", "FLY_Load", "RSSE_Load"]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def keep_toc(fn):
    """The glue of an import saves the TOC pointer in the caller's frame (the code reloads r2 from 0x14(r1) after the call)."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


def stub_udebug(e):
    e.cl.rec("UDebug", ())
    if e.cstr(e.r(3)) != "read failed":
        e.cl.rec("UDebug_other_text", ())


def stub_read(e):
    """LbFile_Read(TbError* hidden r3, file r4, buffer r5, length r6, transferred r7)."""
    L = e.cl
    L.rec("LbFile_Read", (L.lab(e.r(4)), T.s32(e.r(6))))
    tag = L.ret("ReadTag") & MASK
    count = L.ret("ReadCount") & MASK
    e.write(e.r(5), struct.pack("<I", tag))
    e.write(e.r(7), struct.pack(">I", count))
    e.write(e.r(3), struct.pack(">I", 0))


def load_by_ref(name):
    """X::Load(void*& file): this r3, the address of the handle r4."""
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), L.lab(e.u32(e.r(4)))))
        e.set_r(3, L.ret(name) & MASK)
    return stub


def stub_new(e):
    if e.r(3) != 0x1D0:
        e.cl.rec("new_bad_size", (e.r(3),))
    e.set_r(3, e.alloc(0x1D0))


def stub_construct_array(e):
    """__construct_array(array r3, constructor r4, destructor r5, size r6, count r7): the headers' constructor on each element."""
    L = e.cl
    if (e.r(4), e.r(5), e.r(6), e.r(7)) != (0x102657E8, 0x10267288, 0x10, 0x1D):
        L.rec("construct_array_bad", (e.r(4), e.r(5), e.r(6), e.r(7)))
    for i in range(e.r(7)):
        L.rec("SubscriptionHeader", (L.lab(e.r(3) + i * e.r(6)),))


def stub_register(e):
    L = e.cl
    L.rec("register_global_object", (L.lab(e.r(3)), e.r(4), e.r(5)))


for addr, spec, name, ret in [
    (0x1015B370, "Li", "OpenLogfile", None), (0x1015B3A4, "L", "CloseLogfile", None),
    (0x101439DC, "LLiiu", "ActionSerialise", "i"), (0x1006FC90, "su", "DEBUG_MESSAGE", None), (0x1006FCBC, "usi", "CTRACE", None),
    (0x1000DDE8, "is", "GAME_ASSERTMSG", None), (0x1013C9F0, "LL", "CWorld_Load", "i"), (0x100D9CCC, "L", "SCR_LoadAllScripts", "i"),
    (0x100BB7D8, "L", "PAR_LoadStatus", "i"), (0x100BE494, "", "PAR_KillAndHide", None), (0x10147588, "LL", "CClock_Load", "i"),
    (0x10168770, "LL", "CVanillaTime_Load", "i"), (0x10236B30, "L", "GAMESYSTEM_Load", "i"), (0x1006B878, "L", "LoadRides", "i"),
    (0x10023850, "L", "BMP_Load", "i"), (0x10079AA8, "L", "FLY_Load", "i"), (0x100D56BC, "L", "RSSE_Load", "i"),
    (0x10036218, "L", "LoadCamera", "i"), (0x10044D68, "L", "LoadCoasters", "i"), (0x10009CB8, "L", "ADV_Load", None),
    (0x100DE48C, "L", "SOUND_LoadStatus", None), (0x10146F24, "L", "CCheat_ctor", None), (0x10010054, "L", "CAdvisorScoring_ctor", None),
    (0x101F0284, "LL", "UIInterface_Load", None),
]:
    STUBS.internal(addr, CL.recorder(name, spec, ret))
STUBS.internal(0x10000E40, stub_udebug)
STUBS.internal(0x10152840, load_by_ref("CMessageCentre_Load"))
STUBS.internal(0x101470F8, load_by_ref("CCheat_Load"))
STUBS.internal(0x100111F0, load_by_ref("CAdvisorScoring_Load"))
# the glue imports
STUBS.internal(0x102398E0, keep_toc(CL.recorder("GetPosition", "L", "i")))
STUBS.internal(0x10238848, keep_toc(stub_read))
STUBS.internal(0x102384B8, keep_toc(stub_new))
STUBS.internal(0x10238758, keep_toc(stub_construct_array))
STUBS.internal(0x10239940, keep_toc(stub_register))


def setup(runner):
    pass


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def set_ptr(self, a, value):
        """A pointer variable: 4 bytes on the Mac, 8 on the host (the arena lies above 4 GB)."""
        self.write(a, struct.pack(self.pfmt, value))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def make_blocks(self):
        self.file = self.alloc(0x40)
        self.world = self.alloc(0x400)
        self.mc = self.alloc(0x1D0)

    def labels(self):
        """label -> address, for the objects the log names (the same numbers on both sides)."""
        return {3: self.file, 2: self.world, 1: self.mc, 10: self.g("gSaveLoad"), 11: self.g("gClock"), 12: self.g("gVanillaTime"),
                13: self.g("gUIInterface"), 14: self.g("gGameWinCheat"), 15: self.g("gScoringStorage"), 16: self.g("gActionRecorder")}


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_blocks()
        self.L = e.cl = CL.PpcLog({a: n for n, a in self.labels().items()}, case["queues"])
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

    def call(self, case):
        r3, _, _ = T.call_ppc(self.e, FN, [ADDR["gSaveLoad"], self.file, case["type"] & MASK])
        return T.s32(r3)


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_blocks()
        CL.host_begin(lib, self.labels(), case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_lm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, case):
        return self.lib.tpw_lm_run(self.file, case["type"])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lm_addr.restype, lib.tpw_lm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lm_alloc.restype, lib.tpw_lm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lm_run.restype, lib.tpw_lm_run.argtypes = C.c_int32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lm_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.set_ptr(s.g("gGameWorld"), s.world)
    s.set_ptr(s.g("gMessageCentre"), s.mc if case["mc"] else 0)
    s.write(s.g("gGameWinCheatGuard"), bytes([case["cheatguard"]]))
    s.write(s.g("gScoringGuard"), bytes([case["scoreguard"]]))


def label_of(s, value):
    return 0 if value == 0 else (1 if value == s.mc else 2)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "mc": label_of(s, s.rd_ptr(s.g("gMessageCentre"))),
            "cheatguard": s.read(s.g("gGameWinCheatGuard"), 1), "scoreguard": s.read(s.g("gScoringGuard"), 1)}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

def positions(r):
    p = r.choice([0, 100, 0x7FFFFFF0, 0xFFFFFFF0, r.randrange(0, 1 << 32)])
    out = []
    for _ in range(60):
        out.append(p & MASK)
        p += r.choice([0, 1, 4, 100, r.randrange(0, 5000), -7 & MASK])
    return out


def base(r, typ=None):
    q = {}
    for k in OK_NONZERO:
        q[k] = [1] if k == "CVanillaTime_Load" else [r.choice([1, 1, 2, MASK, 0x80000000])]
    for k in OK_ZERO:
        q[k] = [0]
    q["GetPosition"] = positions(r)
    q["ReadTag"] = list(TAGS)
    q["ReadCount"] = [r.choice([4, 4, 5, 6, 7]) for _ in TAGS]
    return {"type": r.choice([2, 2, 2, 0, 3, -1]) if typ is None else typ, "queues": q, "mc": r.choice([0, 1]),
            "cheatguard": r.choice([0, 1]), "scoreguard": r.choice([0, 1])}


def fail_answer(c, key, r):
    if key in OK_NONZERO:
        c["queues"][key] = [0]
    else:
        c["queues"][key] = [r.choice([1, 2, MASK, 0x100, 0x80000000])]


def bad_tag(c, i, r):
    tags = c["queues"]["ReadTag"]
    tags[i] = r.choice([tags[i] ^ 1, tags[i] ^ 0x80000000, tags[i] + 0x10000, 0, MASK, tags[i] ^ 0x100]) & MASK


def bad_count(c, i, r):
    c["queues"]["ReadCount"][i] = r.choice([0, 1, 3, 8, 9, 16, MASK, 0x80000000, 0x104])


def random_mods(c, r):
    kind = r.random()
    if kind < 0.3:
        return
    if kind < 0.5:
        fail_answer(c, r.choice(OK_NONZERO + OK_ZERO), r)
    elif kind < 0.75:
        bad_tag(c, r.randrange(len(TAGS)), r)
    else:
        bad_count(c, r.randrange(len(TAGS)), r)


def edges():
    r = random.Random(20606)
    for typ in (2, 1, 0, 3, -1, 0x100000001 & MASK):
        c = base(r, typ)
        yield c
    for mc in (0, 1):
        for cg in (0, 1):
            for sg in (0, 1):
                c = base(r, 2)
                c["mc"], c["cheatguard"], c["scoreguard"] = mc, cg, sg
                yield c
    for key in OK_NONZERO + OK_ZERO:
        for mc in (0, 1):
            c = base(r, 2)
            c["mc"] = mc
            fail_answer(c, key, r)
            yield c
    for i in range(len(TAGS)):
        for _ in range(2):
            c = base(r, 2)
            bad_tag(c, i, r)
            yield c
        for count in (0, 3, 4, 7, 8):
            c = base(r, 2)
            c["queues"]["ReadCount"][i] = count
            yield c
    c = base(r, 1)
    fail_answer(c, "ActionSerialise", r)
    yield c


def rnd(r):
    c = base(r)
    for _ in range(r.choice([0, 1, 1, 2])):
        random_mods(c, r)
    return c


TESTS = [
    FnTest("CSaveLoad::LoadModules", ppc, host, edge=edges, rnd=rnd),
]
