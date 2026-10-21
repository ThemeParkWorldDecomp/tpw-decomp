"""Unit v51118, small stand-ins of the main app, against libtpw_unit_CTimeSig_GetTimeSecond.dylib (tests/unit_CTimeSig_GetTimeSecond.cpp),
src/game/park/SkyObjects.cpp, src/game/save/SaveLoad.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/MessageCentre.cpp (CTimeSig's three members are inline in include/TimeSig.h):
  CTimeSig::GetTimeSecond 0x1015f0e8, TurnsSinceLastUpdate 0x1015f070, SetTimeSig 0x1015f02c, CSkyObjects::~CSkyObjects 0x1015b61c,
  CSaveLoad::PreSaveOperations 0x1015b428, WriteSerialiseLog 0x1015b3dc, CloseLogfile 0x1015b3a4, OpenLogfile 0x1015b370,
  InterfaceFontWrapper::GetHeight 0x10168c04, the destructors of std::set<unsigned short> 0x101696f8, std::list<std::wstring> 0x1016985c,
  CRideDatabase 0x10154660, CRideBalance 0x101544ec, std::pair 0x1016b168, the constructors of CMsgCheatMode 0x10151ec8,
  CMsgResearchCategoryUpdated 0x10151d94, std::list<std::wstring> 0x1016d63c, the park theme tree 0x1016e37c, and the allocator
  functions deallocate 0x1016dc80 and destroy 0x1016dfd0.

A case is one block of random bytes (0x200) written to both memories, an object in it, and a call. Callees outside the unit (operator
delete, the string destructor, CWorldSavvy::GetWorld, CClock::PreSave, CBalanceTable::UnInit, CRideDatabase::ClearDatabase, the list's clear,
the set tree's destroy, the font's virtual at +0x28) are fakes on both sides that record their arguments. Compared: the answer (r3; the
functions the host declares void are compared where r3 still holds this), the log, the object's words and bytes, the rest of the block.

Not compared: the vtable words of the messages and of CRideBalance (the host's are native; the CRideBalance store of the base vtable at +4
is compared as "stored or not"), and r3 of the void functions whose r3 is a callee's leftover. There are no fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CTimeSig_GetTimeSecond"
MASK = 0xFFFFFFFF

# (name, address, size)
KINDS = [
    ("CTimeSig::GetTimeSecond", 0x1015f0e8, 12),
    ("CTimeSig::TurnsSinceLastUpdate", 0x1015f070, 64),
    ("CTimeSig::SetTimeSig", 0x1015f02c, 24),
    ("CSkyObjects::~CSkyObjects", 0x1015b61c, 68),
    ("CSaveLoad::PreSaveOperations", 0x1015b428, 40),
    ("CSaveLoad::WriteSerialiseLog", 0x1015b3dc, 8),
    ("CSaveLoad::CloseLogfile", 0x1015b3a4, 8),
    ("CSaveLoad::OpenLogfile", 0x1015b370, 8),
    ("InterfaceFontWrapper::GetHeight", 0x10168c04, 64),
    ("std::set<unsigned_short,std::less<unsigned_short>,std::allocator<unsigned_short>>::~set", 0x101696f8, 116),
    ("std::list<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>,"
     "std::allocator<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>>>::~list", 0x1016985c, 92),
    ("CRideDatabase::~CRideDatabase", 0x10154660, 92),
    ("CRideBalance::~CRideBalance", 0x101544ec, 100),
    ("std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>::~pair", 0x1016b168, 96),
    ("CMsgCheatMode::CMsgCheatMode", 0x10151ec8, 32),
    ("CMsgResearchCategoryUpdated::CMsgResearchCategoryUpdated", 0x10151d94, 32),
    ("std::list<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>,"
     "std::allocator<std::basic_string<wchar_t,std::char_traits<wchar_t>,std::allocator<wchar_t>>>>::list", 0x1016d63c, 44),
    ("std::allocator<std::__tree<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>,"
     "std::map<TbDynamicStringTemplate<char>,CFSGlobalParkData*,std::less<TbDynamicStringTemplate<char>>,"
     "std::allocator<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>>::value_compare,"
     "std::allocator<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>>::node>::deallocate", 0x1016dc80, 40),
    ("std::allocator<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>::destroy", 0x1016dfd0, 52),
    ("std::__tree<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>,"
     "std::map<TbDynamicStringTemplate<char>,CFSGlobalParkData*,std::less<TbDynamicStringTemplate<char>>,"
     "std::allocator<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>>::value_compare,"
     "std::allocator<std::pair<const_TbDynamicStringTemplate<char>,CFSGlobalParkData*>>>::__tree", 0x1016e37c, 44),
]
# indices (see tpw_v51118_call)
(GETSEC, TURNS, SETSIG, SKY, PRESAVE, WRITELOG, CLOSELOG, OPENLOG, GETHEIGHT, SETDTOR, LISTDTOR, RIDEDB, RIDEBAL, PAIRDTOR,
 CHEAT, RESEARCH, LISTCTOR, DEALLOC, DESTROY, TREECTOR) = range(20)
SIZES = {a: s for _, a, s in KINDS}
OWN = set(SIZES)
DTORS = (SKY, SETDTOR, LISTDTOR, RIDEDB, RIDEBAL, PAIRDTOR)
NOANS = (SETSIG, PRESAVE, DEALLOC, DESTROY)
TURN_OFF = 0x1da70c

# callees
OP_DELETE, STR_DTOR, GET_WORLD, CLOCK_PRESAVE = 0x10238218, 0x102382f0, 0x1014205c, 0x10147608
LIST_CLEAR, TREE_DESTROY, CLEAR_DB, UNINIT, PTR_GLUE = 0x1016d4e8, 0x1016e65c, 0x101546e8, 0x1001ed40, 0x1023baa8

ADDR = dict(gWorld=0x1037ec0c, gClock=0x1037f048, gWStringListCtorWord=0x102a723c, gParkTreeCtorWord=0x102a7234)

# block layout (Mac and host alike)
OBJ, NODE, AUX, VT, REST, WORLD = 0x00, 0x40, 0x80, 0xC0, 0x100, 0x1F0

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def _rec(name, f):
    def stub(e):
        f(e, e.cl)
    return stub


def st_delete(e):
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def st_strdtor(e):
    e.cl.rec("StrDtor", (e.cl.lab(e.r(3)), s32(e.r(4))))


def st_getworld(e):
    e.cl.rec("GetWorld", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.u32(ADDR["gWorld"]))


def st_presave(e):
    e.cl.rec("PreSave", (e.cl.lab(e.r(3)),))


def st_clear(e):
    e.cl.rec("ListClear", (e.cl.lab(e.r(3)),))


def st_treedestroy(e):
    e.cl.rec("SetTreeDestroy", (e.cl.lab(e.r(3)), e.r(4) & MASK))


def st_cleardb(e):
    e.cl.rec("ClearDatabase", (e.cl.lab(e.r(3)),))


def st_uninit(e):
    e.cl.rec("UnInit", (e.cl.lab(e.r(3)),))


def st_spacing(e):
    e.cl.rec("GetVerticalSpacing", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.cl.ret("GetVerticalSpacing") & 0xFF)


for _a, _f in ((OP_DELETE, st_delete), (STR_DTOR, st_strdtor), (GET_WORLD, st_getworld), (CLOCK_PRESAVE, st_presave),
               (LIST_CLEAR, st_clear), (TREE_DESTROY, st_treedestroy), (CLEAR_DB, st_cleardb), (UNINIT, st_uninit),
               (PTR_GLUE, st_spacing)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def layout_blocks(self):
        self.O, self.N, self.A, self.VT, self.W = self.blk + OBJ, self.blk + NODE, self.blk + AUX, self.blk + VT, self.blk + WORLD

    def labels(self):
        return {self.O: 1, self.A: 2, self.g("gClock"): 3, self.N: 4}


class Ppc(Side):
    fmt = ">I"
    psz = 4
    pmask = MASK
    off = dict(flags=4, cheat=8, research=8, font=0x14, tcount=0, troot=4, tslot8=8, tbegin=0xC)

    def __init__(self, e, case):
        self.e = e
        self.blk = e.alloc(0x200)
        self.layout_blocks()
        self.L = e.cl = CL.PpcLog({a: n for a, n in self.labels().items()}, case["queues"])
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

    def ptr(self, a, v):
        self.words(a, [v & MASK])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def call(self, index, args):
        r3, _, _ = T.call_ppc(self.e, KINDS[index][1], [x & MASK for x in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    psz = 8
    pmask = (1 << 64) - 1

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blk = lib.tpw_v51118_alloc(0x200)
        self.layout_blocks()
        lay = lib.layout
        self.off = dict(flags=lay[0], cheat=lay[1], research=lay[2], font=lay[3], tcount=lay[4], troot=lay[5], tslot8=lay[6], tbegin=lay[7])
        CL.host_begin(lib, {n: a for a, n in self.labels().items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v51118_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v51118_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v & self.pmask))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, index, args):
        a = list(args) + [0] * (4 - len(args))
        return self.lib.tpw_v51118_call(index, *[x & ((1 << 64) - 1) for x in a]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_v51118_addr.restype, lib.tpw_v51118_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_v51118_alloc.restype, lib.tpw_v51118_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v51118_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_v51118_call.restype = C.c_uint64
    lib.tpw_v51118_call.argtypes = [C.c_int32, C.c_uint64, C.c_uint64, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_v51118_layout(out)
    lib.layout = list(out)[:n]
    assert lib.layout[11] == TURN_OFF, lib.layout
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    k = case["kind"]
    s.write(s.blk, case["fill"])
    if k in (GETSEC, TURNS, SETSIG):
        s.words(s.O, [case["stamp"]])
    if k in (TURNS, SETSIG):
        s.ptr(s.g("gWorld"), (s.W - TURN_OFF) & s.pmask)
        s.words(s.W, [case["turn"]])
    if k == GETHEIGHT:
        s.ptr(s.O + s.off["font"], s.A if case["font"] else 0)
        s.words(s.A, [s.VT])
        s.words(s.VT + 0x28, [0x1000])
    if k == SETDTOR:
        s.words(s.O + 4, [case["root"]])
    if k == LISTCTOR:
        s.words(s.g("gWStringListCtorWord"), [case["gl"]])
    if k == TREECTOR:
        s.words(s.g("gParkTreeCtorWord"), [case["gl"]])
        s.write(s.N, bytes([case["cmp"]]))


def execute(s, case):
    k = case["kind"]
    o = s.O if case["obj"] else 0
    node = s.N if case["node"] else 0
    args = {
        GETSEC: (s.O,), TURNS: (s.O,), SETSIG: (s.O,), PRESAVE: (s.O,), CLOSELOG: (s.O,), GETHEIGHT: (s.O,), LISTCTOR: (s.O,),
        SKY: (o, case["flag"]), SETDTOR: (o, case["flag"]), LISTDTOR: (o, case["flag"]), RIDEDB: (o, case["flag"]),
        RIDEBAL: (o, case["flag"]), PAIRDTOR: (o, case["flag"]),
        WRITELOG: (s.O, case["size"], case["name"], case["state"]), OPENLOG: (s.O, case["state"]),
        CHEAT: (s.O, case["v"]), RESEARCH: (s.O, case["cat"]),
        DEALLOC: (s.O, node, case["count"]), DESTROY: (s.O, node), TREECTOR: (s.O, s.N),
    }[k]
    return s.call(k, args)


def observe(s, case, ans):
    k = case["kind"]
    o = s.O
    out = {"log": s.log(), "rest": s.read(s.blk + REST, WORLD - REST)}
    if k not in NOANS:
        out["ans"] = ((ans - s.blk) & MASK if ans else 0) if k in DTORS + (CHEAT, RESEARCH, LISTCTOR, TREECTOR) else ans
    if k == LISTCTOR:
        out["list"] = [s.rd_word(o), (s.rd_word(o + 4) - (o + 4)) & MASK, (s.rd_word(o + 8) - (o + 4)) & MASK]
        out["body"] = s.read(o + 12, 0x74)
    elif k == TREECTOR:
        f = s.off
        out["tree"] = [s.rd_word(o + f["tcount"]), s.rd_ptr(o + f["troot"]), s.read(o + f["tslot8"], 1),
                       (s.rd_ptr(o + f["tbegin"]) - (o + f["troot"])) & s.pmask]
        out["tail"] = s.read(o + 0x20, 0x60)
    elif k in (CHEAT, RESEARCH):
        out["flags"] = s.rd_word(o + s.off["flags"])
        out["payload"] = s.read(o + s.off["cheat"], 1) if k == CHEAT else s.rd_word(o + s.off["research"])
        out["tail"] = s.read(o + 0x10, 0x70)
    elif k == RIDEBAL:
        out["head"] = s.read(o, 4)
        out["vt"] = int(s.rd_word(o + 4) == 0x102a5d2c) if isinstance(s, Ppc) else int(bool(case["obj"]))
        out["body"] = s.read(o + 8, 0x38)
    elif k == GETHEIGHT:
        pass
    else:
        if k in (GETSEC, TURNS, SETSIG):
            out["head"] = s.rd_word(o)      # the stamp was written as a word
        else:
            out["head"] = s.read(o, 4)
        if k == SETDTOR:
            out["root"] = s.rd_word(o + 4)
            out["body"] = s.read(o + 8, 0x38)
        else:
            out["body"] = s.read(o + 4, 0x3c)
    return out


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


# ---------------------------------------------------------------------------------------------------------------- cases

WORDS = [0, 1, 2, 3, 4, 5, 7, 8, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFC, 0xFFFFFFFF]
FLAG_LOW = [0, 1, 2, 0x7FFF, 0x8000, 0xFFFF]


def rword(r):
    return r.choice(WORDS) if r.random() < 0.5 else r.getrandbits(32)


def rflag(r):
    low = r.choice(FLAG_LOW) if r.random() < 0.7 else r.getrandbits(16)
    return ((r.getrandbits(16) << 16) if r.random() < 0.5 else 0) | low


def mk(r, kind, **over):
    c = {"kind": kind, "obj": 1, "fill": bytes(r.getrandbits(8) for _ in range(0x200)),
         "queues": {"GetVerticalSpacing": [r.choice([0, 1, 0x7F, 0x80, 0xFF, r.randrange(256)])]},
         "stamp": rword(r), "turn": rword(r), "flag": rflag(r), "font": r.choice([0, 1]), "root": r.choice([0, rword(r)]),
         "node": r.choice([0, 1]), "count": rword(r), "v": r.choice([0, 1]), "cat": rword(r), "size": rword(r), "state": r.choice([0, 1]),
         "name": rword(r), "cmp": r.randrange(256), "gl": rword(r)}
    c.update(over)
    return c


def edges(kind):
    r = random.Random(5000 + kind)
    if kind in DTORS:
        for obj in (0, 1):
            for low in FLAG_LOW:
                for root in (0, 0x1000, 0xFFFFFFFF):
                    yield mk(r, kind, obj=obj, flag=low, root=root)
                    yield mk(r, kind, obj=obj, flag=(r.getrandbits(16) << 16) | low, root=root)
    elif kind in (GETSEC, TURNS, SETSIG):
        for stamp in WORDS:
            for turn in WORDS:
                yield mk(r, kind, stamp=stamp, turn=turn)
    elif kind == GETHEIGHT:
        for font in (0, 1):
            for sp in (0, 1, 0x7F, 0x80, 0xFF):
                yield mk(r, kind, font=font, queues={"GetVerticalSpacing": [sp]})
    elif kind in (CHEAT,):
        for v in (0, 1):
            yield mk(r, kind, v=v)
    elif kind == DEALLOC:
        for node in (0, 1):
            for count in (0, 1, 0xFFFFFFFF):
                yield mk(r, kind, node=node, count=count)
    elif kind == DESTROY:
        for node in (0, 1):
            yield mk(r, kind, node=node)
    elif kind in (LISTCTOR, TREECTOR):
        for gl in WORDS:
            yield mk(r, kind, gl=gl, cmp=r.choice([0, 1, 0x7F, 0x80, 0xFF]))
    elif kind == RESEARCH:
        for cat in WORDS:
            yield mk(r, kind, cat=cat)
    elif kind in (WRITELOG, OPENLOG):
        for state in (0, 1, 0xFFFFFFFF):
            yield mk(r, kind, state=state)
    else:
        yield mk(r, kind)
        yield mk(r, kind)


def rnd(kind):
    def gen(r):
        return mk(r, kind, obj=int(r.random() < 0.85))
    return gen


TESTS = [FnTest(name, ppc, host, edge=(lambda k=i: edges(k)), rnd=rnd(i)) for i, (name, _a, _s) in enumerate(KINDS)]
