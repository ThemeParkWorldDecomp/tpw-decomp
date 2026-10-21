"""Unit v5628, CSaveLoad::SaveModules: writes the park's modules to the save file, each followed by its four-character tag, against
libtpw_unit_CSaveLoad_SaveModules.dylib (tests/unit_CSaveLoad_SaveModules.cpp), main app ("SimTheme Park"),
src/game/save/SaveLoad.cpp:
  CSaveLoad::SaveModules 0x1015a190

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides that records its arguments (objects and files as labels, the tag write's four bytes as a big-endian word, the trace
texts and values) and answers from the case's queues: the file position, the module savers, the tag writes' TbError. The savers that take
the file by reference may replace it with the second test file. operator new and __construct_array (the message centre made in place)
are recorded as "news" on the PowerPC side, and the host's `new CMessageCentre` as the same sizes; the constructor of each of its 29
headers is an event with a new label on both sides. The scoring singleton's guard code is inlined in the original and is TheScoring() in
the translation; its fake repeats the original's guard (constructor and destructor registration as events).

The world of a case: the call's type, gGameWorld, gMessageCentre (NULL or an object), the guard bytes of the static CCheat and of the
scoring object, the 17 tag test words, and the queues; `fail` picks the step that fails (the steps before it succeed). What is compared:
the answer, the event log, gMessageCentre (NULL, the same object or a new one), both guard bytes and the news.

No fused operations. Not tested: a NULL gGameWorld (the original calls CWorld::Save on it), a NULL answer of operator new (the host's
operator new does not answer NULL).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CSaveLoad_SaveModules"
MASK = 0xFFFFFFFF

FN = 0x1015a190
OWN = {FN}

# ---- the Mac's addresses of the globals (the asm's TOC comments)
ADDR = dict(
    gSaveLoad=0x1037fefc, gActionRecorder=0x1037ec44, gClock=0x1037f048, gVanillaTime=0x10380a04, gGameWinCheat=0x102da934,
    scoring=0x102b3ac8, gUIInterface=0x10396440, node=0x1037ff04, dtor=0x102657e0, gGameWorld=0x1037ebfc, gMessageCentre=0x1037f6c4,
    cheatGuard=0x102da940, scoringGuard=0x102b4100, tagWords=0x102a6cbc,
)
SUB_CTOR, SUB_DTOR = 0x102657e8, 0x10267288      # the TVectors of CSubscriptionHeader's constructor and destructor
P = {}                                             # file1, file2, world, mc: persistent blocks of the emulator (setup)
HOST_LAYOUT = [0x1d0, 0x10, 12]
LABELS = ["file1", "file2", "gSaveLoad", "gActionRecorder", "world", "mc", "gClock", "gVanillaTime", "gGameWinCheat", "scoring",
          "gUIInterface", "node", "dtor"]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
NEWS = []


def ret_this(e, log):
    return e.r(3)


def stub_get_position(e):
    L = e.cl
    L.rec("LbFile_GetPosition", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("LbFile_GetPosition") & MASK)


def stub_write(e):
    """TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred): the TbError in r3's buffer."""
    L = e.cl
    L.rec("LbFile_Write", (L.lab(e.r(4)), e.u32(e.r(5)), s32(e.r(6)), L.lab(e.r(7))))
    e.write(e.r(3), struct.pack(">I", L.ret("LbFile_Write") & MASK))


def stub_new(e):
    NEWS.append(["new", e.r(3)])
    e.set_r(3, e.alloc(max(e.r(3), 4)))


REC_SUB = CL.recorder("CSubscriptionHeader", "N")


def stub_construct_array(e):
    """__construct_array(array, ctor, dtor, size, count): each element's constructor as an event with a new label."""
    p, ctor, dtor, size, count = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
    NEWS.append(["construct", ctor, dtor, size, count])
    for i in range(count):
        e.set_r(3, (p + size * i) & MASK)
        REC_SUB(e)
    e.set_r(3, p)


def by_ref_save(name):
    """int32_t X::Save(void*& file) const: records the object and the file; may hand back the second file; answers the queue."""
    def fn(e):
        L = e.cl
        slot = e.r(4)
        L.rec(name, (L.lab(e.r(3)), L.lab(e.u32(slot))))
        if L.ret(name + ".file"):
            e.write(slot, struct.pack(">I", P["file2"]))
        e.set_r(3, L.ret(name) & MASK)
    return fn


STUBS.imp("LbFile_GetPosition__FPv", stub_get_position)
STUBS.imp("LbFile_Write__FPvPCvUlPUl", stub_write)
STUBS.imp("__nw__FUl", stub_new)
STUBS.imp("__construct_array", stub_construct_array)
STUBS.imp("__register_global_object", CL.recorder("__register_global_object", "LLL"))
STUBS.internal(0x1015b370, CL.recorder("CSaveLoad::OpenLogfile", "Li"))
STUBS.internal(0x1015b3a4, CL.recorder("CSaveLoad::CloseLogfile", "L"))
STUBS.internal(0x101439dc, CL.recorder("CActionRecorder::Serialise", "LLiiu", ret="i"))
STUBS.internal(0x1006fcbc, CL.recorder("CTRACE", "isi"))
STUBS.internal(0x1006fc90, CL.recorder("DEBUG_MESSAGE", "su"))
STUBS.internal(0x1013bf18, CL.recorder("CWorld::Save", "LL", ret="i"))
STUBS.internal(0x100d96b0, CL.recorder("SCR_SaveAllScripts", "L"))
STUBS.internal(0x100bacac, CL.recorder("PAR_SaveStatus", "Li", ret="i"))
STUBS.internal(0x10152758, by_ref_save("CMessageCentre::Save"))
STUBS.internal(0x10147508, CL.recorder("CClock::Save", "LL", ret="i"))
STUBS.internal(0x101686a4, CL.recorder("CVanillaTime::Save", "LL", ret="i"))
STUBS.internal(0x1023678c, CL.recorder("GAMESYSTEM_Save", "L", ret="i"))
STUBS.internal(0x1006acf8, CL.recorder("SaveRides", "L", ret="i"))
STUBS.internal(0x10022464, CL.recorder("BMP_Save", "L", ret="i"))
STUBS.internal(0x10079248, CL.recorder("FLY_Save", "L", ret="i"))
STUBS.internal(0x100d45b0, CL.recorder("RSSE_Save", "L", ret="i"))
STUBS.internal(0x10035f4c, CL.recorder("SaveCamera", "L", ret="i"))
STUBS.internal(0x10044280, CL.recorder("SaveCoasters", "L", ret="i"))
STUBS.internal(0x10009788, CL.recorder("ADV_Save", "L"))
STUBS.internal(0x100de1a0, CL.recorder("SOUND_SaveStatus", "L"))
STUBS.internal(0x10146f24, CL.recorder("CCheat::CCheat", "L", ret=ret_this))
STUBS.internal(0x101471d0, by_ref_save("CCheat::Save"))
STUBS.internal(0x10010054, CL.recorder("CAdvisorScoring::CAdvisorScoring", "L", ret=ret_this))
STUBS.internal(0x10011294, by_ref_save("CAdvisorScoring::Save"))
STUBS.internal(0x101f02ec, CL.recorder("UIInterface::Save", "LL"))


def setup(runner):
    T.install_std_stubs(runner.emu)
    for name in ("file1", "file2", "world"):
        P[name] = T.persist_alloc(runner.emu, 16)
    P["mc"] = T.persist_alloc(runner.emu, 0x1d0)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def byte(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def rd_byte(self, a):
        return self.read(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({self.g(name): i + 1 for i, name in enumerate(LABELS)}, case["queues"])
        self.saved = STUBS.activate(e)
        del NEWS[:]

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return P[name] if name in P else ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def log(self):
        return self.L.log

    def news(self, case, state):
        return [list(x) for x in NEWS]

    def call(self, file, type_):
        r3, _, _ = T.call_ppc(self.e, FN, [ADDR["gSaveLoad"], file & MASK, type_ & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {i + 1: self.g(name) for i, name in enumerate(LABELS)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sm_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def news(self, case, state):
        """The host's `new CMessageCentre`: the object's size, then its 29 headers of the header's size (layout-checked)."""
        if state != "new":
            return []
        return [["new", HOST_LAYOUT[0]], ["construct", SUB_CTOR, SUB_DTOR, HOST_LAYOUT[1], HOST_LAYOUT[0] // HOST_LAYOUT[1]]]

    def call(self, file, type_):
        return self.lib.tpw_sm_save_modules(file, s32(type_)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_sm_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "save_modules").restype, getattr(lib, p + "save_modules").argtypes = C.c_uint32, [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.ptr(s.g("gGameWorld"), s.g("world"))
    s.ptr(s.g("gMessageCentre"), s.g("mc") if case["mc"] else 0)
    s.byte(s.g("cheatGuard"), case["cheatGuard"])
    s.byte(s.g("scoringGuard"), case["scoringGuard"])
    s.words(s.g("tagWords"), case["tags"])


def observe(s, case, answer):
    mc = s.rd_ptr(s.g("gMessageCentre"))
    state = "null" if mc == 0 else "orig" if mc == s.g("mc") else "new"
    return {"answer": answer, "log": s.log(), "mc": state, "news": s.news(case, state),
            "cheatGuard": s.rd_byte(s.g("cheatGuard")), "scoringGuard": s.rd_byte(s.g("scoringGuard"))}


def run(s, case):
    try:
        build(s, case)
        answer = s.call(s.g(case["file"]), case["type"])
        return observe(s, case, answer)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

# The steps in the order of the asm: a saver ("ok0": succeeds with 0, "nz": succeeds with nonzero, "bool": 0 or 1) or a tag write.
STEPS = [("CActionRecorder::Serialise", "ok0"), ("CWorld::Save", "nz"), 0, 1, ("PAR_SaveStatus", "ok0"), 2,
         ("CMessageCentre::Save", "ok0"), 3, ("CClock::Save", "nz"), 4, ("CVanillaTime::Save", "bool"), 5, ("GAMESYSTEM_Save", "nz"), 6,
         ("SaveRides", "nz"), 7, ("BMP_Save", "nz"), 8, ("FLY_Save", "ok0"), 9, ("RSSE_Save", "ok0"), 10, ("SaveCamera", "nz"), 11,
         ("SaveCoasters", "nz"), 12, 13, 14, ("CCheat::Save", "nz"), 15, ("CAdvisorScoring::Save", "nz"), 16]


def nonzero(r):
    return r.choice([1, -1, 2, 0x100, -0x80000000, 0x7FFFFFFF, r.randrange(1, 1 << 31), -r.randrange(1, 1 << 31)])


def nonneg(r):
    return r.choice([0, 4, 0x7FFFFFFF, r.randrange(0, 1 << 31)])


def neg(r):
    return r.choice([-1, -0x80000000, -r.randrange(1, 1 << 31)])


def word_nz(r):
    return r.choice([1, 0x80000000, 0xFFFFFFFF, r.randrange(1, 1 << 32)])


def outcome(r, kind, good):
    if kind == "ok0":
        return 0 if good else nonzero(r)
    if kind == "bool":
        return 1 if good else 0
    return nonzero(r) if good else 0


def tag_pair(r, good):
    """(TbError, test word): passes when (result >= 0) == (word == 0)."""
    flip = r.random() < 0.25
    if good:
        return (neg(r), word_nz(r)) if flip else (nonneg(r), 0)
    return (nonneg(r), word_nz(r)) if flip else (neg(r), 0)


def make_case(r, fail=None, type_=None, mc=None, cheat_guard=None, scoring_guard=None, swaps=None):
    if fail is None:
        fail = r.choice([len(STEPS)] * 3 + list(range(len(STEPS) + 1)))
    queues = {"LbFile_GetPosition": [s32(r.randrange(1 << 32)) if r.random() < 0.2 else r.randrange(0, 1 << 20) for _ in range(48)]}
    writes = [0] * 17
    tags = [0] * 17
    for i, step in enumerate(STEPS):
        good = i < fail if i != fail else False
        if i > fail:
            good = r.random() < 0.7
        if isinstance(step, int):
            writes[step], tags[step] = tag_pair(r, good)
        else:
            queues[step[0]] = [outcome(r, step[1], good)]
    queues["LbFile_Write"] = writes
    if swaps is None:
        swaps = [r.random() < 0.3 for _ in range(3)]
    for name, swap in zip(("CMessageCentre::Save", "CCheat::Save", "CAdvisorScoring::Save"), swaps):
        queues[name + ".file"] = [1 if swap else 0]
    if type_ is None:
        type_ = r.choice([0, 2, 2, 0, 1, -1, 3, 0x80000001])
    return {"type": type_ & MASK, "file": "file1", "queues": queues, "tags": tags,
            "mc": r.random() < 0.6 if mc is None else mc,
            "cheatGuard": r.choice([0, 1, 1, 0x80, 0xFF]) if cheat_guard is None else cheat_guard,
            "scoringGuard": r.choice([0, 1, 1, 0x80, 0xFF]) if scoring_guard is None else scoring_guard}


def edges():
    r = random.Random(5628)
    for type_ in (1, 0, 2, -1, 3, 0x80000001):
        yield make_case(r, fail=len(STEPS), type_=type_)
        yield make_case(r, fail=0, type_=type_)
    for fail in range(len(STEPS) + 1):
        for _ in range(2):
            yield make_case(r, fail=fail, type_=r.choice([0, 2]))
    for mc in (False, True):
        for cg in (0, 1, 0x80, 0xFF):
            for sg in (0, 1, 0x80, 0xFF):
                yield make_case(r, fail=len(STEPS), type_=2, mc=mc, cheat_guard=cg, scoring_guard=sg)
    for swaps in ([True, False, False], [False, True, False], [False, False, True], [True, True, True]):
        yield make_case(r, fail=len(STEPS), type_=0, swaps=swaps)
    # the message centre made, then its save failing; the cheat made, then its save failing
    yield make_case(r, fail=6, type_=2, mc=False)
    yield make_case(r, fail=28, type_=2, cheat_guard=0)
    yield make_case(r, fail=30, type_=2, scoring_guard=0)


def rnd(r):
    return make_case(r)


TESTS = [
    FnTest("CSaveLoad::SaveModules", ppc, host, edge=edges, rnd=rnd),
]
