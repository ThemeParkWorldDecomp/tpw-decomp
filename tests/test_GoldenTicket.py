"""Unit w180 (main app), against libtpw_unit_GoldenTicket.dylib (tests/unit_GoldenTicket.cpp):

  src/game/park/GoldenTickets.cpp  CGoldenTicketControl::HowManyTickets 0x100fbdf8, SpendTicketOn 0x100fbea0, HasBeenBought 0x100fbfec, CanIAfford 0x100fc0c8,
                        CheckForLocal 0x100fc1d0, CheckForGlobal 0x100fc56c, CheckForSecret 0x100fc6c4, EarnedGoldenTicket 0x100fc7c4 (local),
                        0x100fc918 (global), 0x100fca5c (secret), TellIfFirst 0x100fd7f0
  src/game/park/GoldenTickets.cpp, src/game/ui/MessageCentre.cpp    CMsgAdvisor's constructor 0x1015190c, destructor 0x100fcea8 and GetType 0x100fd9cc

They run as the original on the PowerPC side (the signed 64 bit divide helper 0x10237e1c too, and the constructor, GetType and destructor of CMsgAdvisor
where TellIfFirst uses them) and as src/ on the host. Every other callee is a fake that records the same event on both sides: the master save (its
constructor and members), GetRideInfo, the game type's constructor, the park analyser's ten questions, GetParkAnalyser, GetBankAccount,
GetProfitThisYear, GetFunnyTimeSinceSessionStart, the three TellTheWorld, CTRACE, the advisor request's two constructors, the message centre
(operator new of 0x1d0 bytes, the array construction, PostMessage).

The world of a case: gMasterSave (NULL or a block), gMessageCentre (NULL or a block), gWorld (a block whose +0x2a0 is the park time), the twelve balance
slots 466 to 477, the ride record's golden ticket cost, the game type and its guard byte, and the fakes' answer queues (signed 64 bit values, the
last one repeats). What is compared: the answer (r3), the event log in order (every fake's receiver as a label, every argument), the master save and
centre pointers after the call (as labels), and for SpendTicketOn the game type and its guard. The advisor message's constructor is compared on a
poisoned block (flags, the eight request fields, the padding bytes, the bytes after the object, the vtable), its destructor with the Mac's delete flag.

No fused operations (the happiness compare is fsubs of an integer converted with the 0x43300000 trick, then fcmpo). Not tested: operator new
returning NULL for the master save (the original stores NULL and then dereferences it), a NULL ride record (dereferenced unconditionally), a NULL
world, and the host's CMsgAdvisor holding its request at 0x0c (the Mac's 0x08): the module reads both at their own offsets.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_GoldenTicket"
MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF
POISON = b"\xa5"

FN = dict(howmany=0x100FBDF8, spend=0x100FBEA0, bought=0x100FBFEC, afford=0x100FC0C8, local=0x100FC1D0, glob=0x100FC56C, secret=0x100FC6C4,
          earn_l=0x100FC7C4, earn_g=0x100FC918, earn_s=0x100FCA5C, tellfirst=0x100FD7F0, msg_ctor=0x1015190C, msg_dtor=0x100FCEA8, msg_type=0x100FD9CC)
HELPER = 0x10237E1C
OWN = set(FN.values()) | {HELPER}

GMASTER, GWORLD, GCENTRE, DATABASE, MAIN = 0x10380A8C, 0x1037EC0C, 0x1037F6C4, 0x1037F6C8, 0x102B4568
GAME_TYPE, GAME_GUARD = 0x102B3AA0, 0x102B3AA4
MSG_VTABLE = 0x102A09A4
CENTRE_CTOR, CENTRE_DTOR = 0x102657E8, 0x10267288
MONTH = 25920000000000

L_WORLD, L_CONTROL, L_MASTER, L_ANALYSER, L_BANK, L_CENTRE, L_PARK, L_DATABASE, L_RIDE = 1, 2, 3, 4, 5, 6, 7, 8, 9
# the host's offsets and sizes the module relies on, checked once against tpw_gt_layout: the park time in the world, the cost slot's offset in the
# ride record (RideInfo's balance values start at the record's +0x?? on the host: only checked to be 4 apart per slot), BalanceValue, CMsgAdvisor's request, CMsgAdvisor, the request
HOST_LAYOUT = None


def s32(v):
    return T.s32(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v & (1 << 63) else v


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def bitsf(b):
    return struct.unpack(">f", struct.pack(">I", b & MASK))[0]


def qn(q):
    return {k: [s64(v) for v in vs] for k, vs in q.items()}


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub(addr, name, ints, answer=None, then=None):
    """A fake: records name with ints(e) (a list built from the registers), answers r3 = answer(e) & MASK, then runs then(e)."""
    def fake(e):
        L = e.cl
        L.rec(name, ints(e))
        if then:
            then(e)
        if answer:
            e.set_r(3, answer(e) & MASK)
    STUBS.internal(addr, fake)


def lab(e, i):
    return e.cl.lab(e.r(i))


def ret(e, key):
    return e.cl.ret(key, 0)


def master_new(e):
    e.cl.rec("CFSGlobalMasterSave", [e.cl.newlab(e.r(3))])


STUBS.internal(0x101694DC, master_new)
stub(0x10169B24, "HowManyTickets", lambda e: [lab(e, 3)], lambda e: ret(e, "howmany"))
stub(0x10169C24, "HowManyTicketsIncludingSecrets", lambda e: [lab(e, 3)], lambda e: ret(e, "inclsec"))
stub(0x10169D68, "SpendingPower", lambda e: [lab(e, 3)], lambda e: ret(e, "power"))
stub(0x10169FE8, "MasterSpendTicketOn", lambda e: [lab(e, 3), e.r(4) & 0xFFFF, s32(e.r(5))], lambda e: 1 if ret(e, "mspend") & 0xFF else 0)
stub(0x1016A094, "MasterHasBeenBought", lambda e: [lab(e, 3), e.r(4) & 0xFFFF], lambda e: 1 if ret(e, "bought") & 0xFF else 0)
stub(0x1016A12C, "HaveTicketLocal", lambda e: [lab(e, 3), s32(e.r(4))], lambda e: ret(e, "have"))
stub(0x1016A1A4, "HaveTicketSecret", lambda e: [lab(e, 3), s32(e.r(4))], lambda e: ret(e, "have"))
stub(0x1016A200, "EarnedTicketLocal", lambda e: [lab(e, 3), s32(e.r(4))], lambda e: ret(e, "won"))
stub(0x1016A2A0, "EarnedTicketGlobal", lambda e: [lab(e, 3), s32(e.r(4)), s32(e.r(5))], lambda e: ret(e, "won"))
stub(0x1016A420, "EarnedTicketSecret", lambda e: [lab(e, 3), s32(e.r(4))], lambda e: ret(e, "won"))


def tell_nulls_master(e):
    if ret(e, "tellnull"):
        w32(e, GMASTER, 0)


for _a, _n in ((0x100FCBB0, "TellTheWorldLocal"), (0x100FCF1C, "TellTheWorldGlobal"), (0x100FD4F4, "TellTheWorldSecret")):
    stub(_a, _n, lambda e: [lab(e, 3), s32(e.r(4)), s32(e.r(5))], None, tell_nulls_master)


def stub_trace(e):
    text = e.cstr(e.r(4))[:159]
    e.cl.rec("CTRACE", [e.r(3) & MASK, s32(e.r(5))], [text])


STUBS.internal(0x1006FCBC, stub_trace)
stub(0x10154868, "GetRideInfo", lambda e: [lab(e, 3), e.r(4) >> 16], lambda e: e.cl.ride)
stub(0x1013EBC8, "GetParkAnalyser", lambda e: [lab(e, 3)], lambda e: e.cl.analyser)
stub(0x1013EDFC, "GetBankAccount", lambda e: [lab(e, 3)], lambda e: e.cl.bank)
stub(0x100F439C, "GetProfitThisYear", lambda e: [lab(e, 3)], lambda e: ret(e, "profit"))


def game_type_ctor(e):
    e.cl.rec("CGameType", [1])
    w32(e, e.r(3), e.cl.ctortype)


STUBS.internal(0x1016F5D8, game_type_ctor)


def funny(e):
    e.cl.rec("GetFunnyTimeSinceSessionStart", [lab(e, 4)])
    e.write(e.r(3), (ret(e, "span") & M64).to_bytes(8, "big"))


STUBS.internal(0x10112B14, funny)
stub(0x100E7994, "NumberOfVisitorsSinceBeginning", lambda e: [lab(e, 3)], lambda e: ret(e, "visitors"))
stub(0x100E7278, "NumberOfVisitorsInPark", lambda e: [lab(e, 3)], lambda e: ret(e, "inpark"))
stub(0x100EA064, "AllObjectsResearchedAndPresent", lambda e: [lab(e, 3)], lambda e: ret(e, "research") & 0xFF)
stub(0x100E79E8, "NumberOfVisitorsInLastMonths", lambda e: [lab(e, 3), s32(e.r(4))], lambda e: ret(e, "lastmonths"))
stub(0x100EEED0, "NumberOfCellsCoveredByObjects", lambda e: [lab(e, 3)], lambda e: ret(e, "cells"))
stub(0x100EECD8, "PercentageOfParkCoveredByCameras", lambda e: [lab(e, 3)], lambda e: ret(e, "cameras"))


def happiness(e):
    e.cl.rec("GetHappinessLevel", [lab(e, 3), e.r(4) & MASK])
    e.set_f1(bitsf(ret(e, "happy")))


STUBS.internal(0x100E4CD8, happiness)


def put_thing(e):
    e.write(e.r(4), struct.pack(">H", ret(e, "thing") & 0xFFFF))


def coaster(e):
    e.cl.rec("GetHighestCoaster", [lab(e, 3), e.r(5) >> 16])
    put_thing(e)
    e.set_r(3, ret(e, "coaster") & MASK)


def gokart(e):
    e.cl.rec("GetMostExcitingGokart", [lab(e, 3), e.u32(e.r(4) - 2) & 0xFFFF if False else struct.unpack(">H", e.read(e.r(4), 2))[0]])
    put_thing(e)
    e.set_r(3, ret(e, "gokart") & 0xFF)


def longest(e):
    e.cl.rec("GetLongestTrackRide", [lab(e, 3), struct.unpack(">H", e.read(e.r(4), 2))[0], s32(e.r(5))])
    put_thing(e)
    e.set_r(3, ret(e, "longest") & MASK)


STUBS.internal(0x100ECE94, coaster)
STUBS.internal(0x100EC484, gokart)
STUBS.internal(0x100ECA98, longest)


def request_ctor(e):
    dest = e.r(3)
    fields = [s32(e.r(4)), e.r(5) >> 16, e.r(6) & MASK, s32(e.r(7)), e.r(8) & 0xFF, s32(e.r(9))]
    e.cl.rec("RequestCtor", fields)
    e.write(dest, struct.pack(">H", e.r(5) >> 16))
    e.write(dest + 4, struct.pack(">IiiiBBB", e.r(6) & MASK, s32(e.r(7)), s32(e.r(4)), s32(e.r(9)), 1, e.r(8) & 0xFF, 0))


def request_copy(e):
    dest, src = e.r(3), e.r(4)
    ident = struct.unpack(">H", e.read(src, 2))[0]
    sec, resp, msg, score, valid, override, overridden = struct.unpack(">IiiiBBB", e.read(src + 4, 0x13))
    e.cl.rec("RequestCopy", [ident, sec, resp, msg, score, valid, override, overridden])
    e.write(dest, struct.pack(">H", ident))
    e.write(dest + 4, struct.pack(">IiiiBBB", sec, resp, msg, score, valid, override, overridden))


def post(e):
    L, msg = e.cl, e.r(4)
    vt = e.u32(msg)
    ident = struct.unpack(">H", e.read(msg + 8, 2))[0]
    sec, resp, rmsg, score, valid, override, overridden = struct.unpack(">IiiiBBB", e.read(msg + 12, 0x13))
    L.rec("PostMessage", [L.lab(e.r(3)), 5 if vt == MSG_VTABLE else -1, e.u32(msg + 4), ident, sec, resp, rmsg, score, valid, override, overridden])
    e.set_r(3, 1)


STUBS.internal(0x1000DEB4, request_ctor)
STUBS.internal(0x1000DE30, request_copy)
STUBS.internal(0x1015256C, post)


def op_new(e):
    size = e.r(3)
    fail = bool(e.cl.ret("newnull", 0)) if size == 0x1D0 else False
    if size == 0x1D0:
        e.cl.rec("new", [size, int(fail)])
    e.set_r(3, 0 if fail else e.alloc(size, POISON * size))


STUBS.imp("__nw__FUl", op_new)


def op_delete(e):
    p = e.r(3)
    if p:
        e.cl.deletes += 1
        if e.cl.lab(p) > 0:
            e.cl.rec("delete", (e.cl.lab(p),))
            e.cl.forget(p)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def construct(emu, call):
        p, ctor, dtor, size, count = (emu.r(i) for i in range(3, 8))
        if ctor == CENTRE_CTOR:
            emu.cl.rec("CMessageCentre", [emu.cl.newlab(p), int(ctor == CENTRE_CTOR), int(dtor == CENTRE_DTOR), size, count])

    T.stub_cb(e, "__construct_array", construct)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, qn(case.get("q", {})))
        self.L.deletes = 0
        e.news.clear()
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def block(self, name, size, fill=POISON):
        return self.e.alloc(size, fill * size)

    def begin(self, labels, case):
        self.L.labels.update({a: l for l, a in labels.items()})

    def prepare(self, c, B):
        e, L = self.e, self.L
        L.ride, L.analyser, L.bank, L.ctortype = B["ride"], B["analyser"], B["bank"], c.get("ctortype", 0)
        w32(e, GMASTER, B["master"] if c["msave"] else 0)
        w32(e, GWORLD, B["world"])
        w32(e, GCENTRE, B["centre"] if c["centre"] else 0)
        for k, v in enumerate(c["slots"]):
            w32(e, MAIN + 8 + 4 * (466 + k), v)
        w32(e, B["ride"] + 0xC4, c["cost"])
        if "type" in c:
            w32(e, GAME_TYPE, c["type"])
            e.write(GAME_GUARD, bytes([c["tguard"]]))

    def master(self):
        return self.L.lab(self.e.u32(GMASTER))

    def centre(self):
        return self.L.lab(self.e.u32(GCENTRE))

    def game(self):
        return {"type": self.e.u32(GAME_TYPE), "guard": 1 if self.e.read(GAME_GUARD, 1)[0] else 0}

    def call(self, name, args):
        import tpwtest
        r3 = tpwtest.call_ppc(self.e, FN[name], args)[0] & MASK
        return 0 if name in ("local", "glob", "secret", "tellfirst") else r3

    def log(self):
        return self.L.log

    def extra(self):
        return {"deletes": self.L.deletes, "news": len(self.e.news)}


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.case = case

    def done(self):
        pass

    def block(self, name, size, fill=POISON):
        return self.lib.tpw_gt_addr(name.encode()) if name else self.lib.tpw_gt_alloc(size)

    def begin(self, labels, case):
        CL.host_begin(self.lib, labels, qn(case.get("q", {})))

    def prepare(self, c, B):
        lib = self.lib
        slots = (C.c_int32 * 12)(*[s32(v) for v in c["slots"]])
        lib.tpw_gt_prepare(B["world"], B["master"] if c["msave"] else 0, B["centre"] if c["centre"] else 0, slots, s32(c["cost"]), s32(c.get("ctortype", 0)))
        if "type" in c and not c.get("first"):
            lib.tpw_gt_mode(s32(c["type"]))

    def master(self):
        return self.lib.tpw_gt_master()

    def centre(self):
        return self.lib.tpw_gt_centre()

    def game(self):
        return {"type": self.lib.tpw_gt_type() & MASK, "guard": 1}

    def call(self, name, args):
        lib = self.lib
        if name in ("local", "glob", "secret", "tellfirst"):
            self.voidcall(name)
            return 0
        if name in ("howmany",):
            return lib.tpw_gt_howmany() & MASK
        if name in ("spend", "bought", "afford"):
            return getattr(lib, "tpw_gt_" + name)(args[1] >> 16) & MASK
        if name in ("local", "glob", "secret"):
            lib.tpw_gt_check(("local", "glob", "secret").index(name))
            return 0
        if name.startswith("earn_"):
            which = "lgs".index(name[-1])
            return lib.tpw_gt_earned(which, s32(args[1]), s32(args[2]) if len(args) > 2 else 0) & MASK
        lib.tpw_gt_tellfirst()
        return 0

    def voidcall(self, name):
        lib = self.lib
        if name == "tellfirst":
            lib.tpw_gt_tellfirst()
        else:
            lib.tpw_gt_check(("local", "glob", "secret").index(name))

    def log(self):
        return CL.host_end(self.lib)

    def extra(self):
        h = list((C.c_int32 * 4).in_dll(self.lib, "tpw_cl_heap"))
        return {"deletes": 0, "news": 0}


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_gt_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_gt_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_gt_prepare.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64, C.POINTER(C.c_int32), C.c_int32, C.c_int32]
    lib.tpw_gt_master.restype = C.c_int64
    lib.tpw_gt_centre.restype = C.c_int64
    lib.tpw_gt_mode.argtypes = [C.c_int32]
    lib.tpw_gt_type.restype = C.c_int32
    lib.tpw_gt_howmany.restype = C.c_int32
    for n in ("spend", "bought", "afford"):
        getattr(lib, p + n).restype, getattr(lib, p + n).argtypes = C.c_uint32, [C.c_uint32]
    lib.tpw_gt_check.argtypes = [C.c_int32]
    lib.tpw_gt_earned.restype, lib.tpw_gt_earned.argtypes = C.c_uint32, [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_gt_msg_ctor.argtypes = [C.c_uint64, C.c_int32, C.c_char_p, C.POINTER(C.c_uint32)]
    lib.tpw_gt_msg_type.restype, lib.tpw_gt_msg_type.argtypes = C.c_int32, [C.c_uint32]
    lib.tpw_gt_msg_dtor.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_uint32)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_gt_layout(out)
    lay = list(out)[:n]
    assert lay[0] == 0x2A0 and lay[2] in (4, 8), lay
    lib.layout = lay
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world and the call


def build(s, c):
    B = {}
    B["control"] = s.block("control", 0x40)
    B["world"] = s.block("world", 0x400)
    B["master"] = s.block("master", 0x40)
    B["analyser"] = s.block("analyser", 0x40)
    B["bank"] = s.block("bank", 0x40)
    B["centre"] = s.block("centre", 0x1D0)
    B["ride"] = s.block("ride", 0x200)
    db = DATABASE if isinstance(s, Ppc) else s.lib.tpw_gt_addr(b"database")
    s.begin({L_WORLD: B["world"], L_CONTROL: B["control"], L_MASTER: B["master"], L_ANALYSER: B["analyser"], L_BANK: B["bank"], L_CENTRE: B["centre"],
             L_PARK: B["world"] + 0x2A0, L_DATABASE: db, L_RIDE: B["ride"]}, c)
    s.prepare(c, B)
    return B


def run(s, c):
    try:
        B = build(s, c)
        args = [B["control"]] + list(c.get("args", []))
        answer = s.call(c["fn"], args)
        out = {"answer": answer, "log": s.log(), "master": s.master(), "centre": s.centre()}
        if "type" in c:
            out.update(s.game())
        return out
    finally:
        s.done()


def ppc(e, c):
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)


# ---------------------------------------------------------------------------------------------------------------- case generators

I32S = [0, 1, 2, 3, 5, 10, 100, 0x7FFFFFFF, 0x80000000, MASK, MASK - 1, 0x1234]


def i32(r):
    return r.choice(I32S) if r.random() < 0.6 else r.getrandbits(32)


def near(r, v):
    """An answer near a balance slot's value, now and then far away."""
    k = r.random()
    if k < 0.7:
        return (s32(v) + r.choice([-1, 0, 1, 0, 1, -1, 2, -2])) & MASK
    return i32(r)


def slot_value(r):
    return r.choice([0, 1, 5, 10, 100, 1000, MASK, 0x7FFFFFFF, 0x80000000]) if r.random() < 0.8 else r.getrandbits(32)


def float_near(r, v):
    k = r.random()
    x = float(s32(v))
    if k < 0.5:
        return fb(x + r.choice([-1.0, 0.0, 1.0, 0.5, -0.5]))
    if k < 0.65:
        return r.choice([0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000, 0, 0x00000001, 0x7F7FFFFF])
    if k < 0.8:
        b = fb(x)
        return (b + r.choice([-1, 1])) & MASK
    return fb(r.uniform(-1e9, 1e9)) if r.random() < 0.5 else r.getrandbits(32)


def span_near(r, months):
    k = r.random()
    if k < 0.7:
        m = s32(months)
        return (r.choice([m - 1, m, m + 1, m, m + 1]) * MONTH + r.choice([0, 0, 1, -1, MONTH - 1, 12345])) & M64
    if k < 0.85:
        return r.choice([0, M64, 1 << 63, (1 << 63) - 1, 1 << 32, MASK, 0x7FFFFFFF * MONTH, (0x100000000 + 7) * MONTH]) & M64
    return r.getrandbits(64)


def have_q(r, n=8, pass_rate=0.7):
    return [r.choice([0, 0, 0, 0, 1, 0xFF, 0x100, 2]) if r.random() < pass_rate else r.choice([1, 2, 0xFF]) for _ in range(n)]


def base(r, fn, **kw):
    slots = [slot_value(r) for _ in range(12)]
    c = {"fn": fn, "msave": r.choice([1, 1, 1, 0]), "centre": r.choice([1, 0]), "slots": slots, "cost": i32(r), "args": [],
         "q": {"howmany": [i32(r)], "inclsec": [r.choice([1, 1, 1, 0, 2, MASK, i32(r)])], "power": [i32(r)], "mspend": [r.choice([0, 1])],
               "bought": [r.choice([0, 1])], "have": have_q(r), "won": [r.choice([0, 1, 2, 3, 4, MASK, i32(r)]) for _ in range(6)],
               "tellnull": [r.choice([0, 0, 1])], "visitors": [near(r, slots[0])], "inpark": [near(r, slots[1]), near(r, slots[3])],
               "happy": [float_near(r, slots[2])], "research": [r.choice([0, 1, 2, 0xFF])], "profit": [near(r, slots[4])],
               "lastmonths": [near(r, slots[5])], "span": [span_near(r, slots[6])], "coaster": [near(r, slots[7])],
               "gokart": [r.choice([slots[8] & 0xFF, (slots[8] + 1) & 0xFF, (slots[8] - 1) & 0xFF, r.getrandbits(8)])], "longest": [near(r, slots[9])],
               "cells": [near(r, slots[11])], "cameras": [r.choice([100, 100, 99, 101, 0, MASK, 0x164, i32(r)])],
               "thing": [r.getrandbits(16) for _ in range(4)], "newnull": [r.choice([0, 0, 0, 1])]}}
    c.update(kw)
    return c


def arg_id(r):
    return [((r.choice([0, 1, 0xFFFF, 0x8000, 0x1234]) if r.random() < 0.5 else r.getrandbits(16)) << 16) | r.choice([0, r.getrandbits(16)])]


def with_q(c, **q):
    c["q"] = dict(c["q"], **q)
    return c


def mk(fn):
    """The random-case maker of a function: the base world plus the function's arguments."""
    def make(r):
        c = base(r, fn)
        if fn in ("spend", "bought", "afford"):
            c["args"] = arg_id(r)
        if fn == "spend":
            c.update(type=r.choice([0, 1, 2, MASK, 0x80000000, 0x7FFFFFFF, 3]), tguard=r.choice([1, 0x7F, 0x80, 0xFF]), ctortype=r.choice([0, 1, 2, MASK]))
        if fn.startswith("earn_"):
            c["args"] = [i32(r) if r.random() < 0.5 else r.randrange(6)] + ([i32(r)] if fn == "earn_g" else [])
        return c
    return make


# ---- edge cases
def edges_howmany():
    r = random.Random(18001)
    return [with_q(base(r, "howmany", msave=m), howmany=[v]) for m in (1, 0) for v in (0, 1, MASK, 0x7FFFFFFF, 0x80000000, 5)]


def edges_spend():
    r = random.Random(18002)
    first = base(r, "spend", type=0, tguard=0, ctortype=0, first=True, args=arg_id(r))
    first = with_q(first, power=[10000], bought=[0])
    first["cost"] = 5
    out = [first]
    for t in (0, 1, 2, MASK, 0x80000000, 0x7FFFFFFF, 3):
        for g in (1, 0x7F, 0x80, 0xFF):
            c = base(r, "spend", type=t, tguard=g, ctortype=0, args=arg_id(r))
            out.append(c)
    for cost, power in ((5, 5), (5, 4), (5, 6), (MASK, 0), (0, MASK), (0x7FFFFFFF, 0x80000000), (0x80000000, 0x7FFFFFFF), (0, 0)):
        for m in (1, 0):
            for ms in (0, 1):
                c = with_q(base(r, "spend", type=0, tguard=1, ctortype=0, args=arg_id(r), msave=m), power=[power], mspend=[ms])
                c["cost"] = cost
                out.append(c)
    return out


def edges_bought():
    r = random.Random(18003)
    return [with_q(base(r, "bought", msave=m, args=arg_id(r)), bought=[b]) for m in (1, 0) for b in (0, 1, 2, 0x100)]


def edges_afford():
    r = random.Random(18004)
    out = []
    for cost, power in ((5, 5), (5, 4), (5, 6), (MASK, 0), (0, MASK), (0x7FFFFFFF, 0x80000000), (0x80000000, 0x7FFFFFFF), (0, 0), (MASK, MASK)):
        for m in (1, 0):
            c = with_q(base(r, "afford", msave=m, args=arg_id(r)), power=[power])
            c["cost"] = cost
            out.append(c)
    return out


def local_case(r, block, delta, sub=0, **kw):
    """Only block `block` is reachable (every earlier ticket is held); its values sit `delta` from the balance slots."""
    c = base(r, "local")
    slots = c["slots"] = [10, 20, 30, 40, 50, 60, 70, 0, 0, 0, 0, 0]
    have = [1] * block + [0] + [0] * 5
    q = {"have": have, "won": [r.choice([0, 1, 2, 4])], "tellnull": [r.choice([0, 1])]}
    q.update(visitors=[10 + delta], inpark=[20 + delta, 40 + delta * (1 if sub == 0 else 0) + (1 if sub == 2 else 0)], happy=[fb(30.0 + delta * 0.5)],
             research=[[0, 1, 2, 0xFF][delta + 1] if block == 3 else 0], profit=[50 + delta], lastmonths=[60 + delta],
             span=[(70 + (delta if sub == 0 else sub - 1)) * MONTH + r.choice([0, 5])])
    if block == 2 and sub == 1:
        q["happy"] = [fb(30.0)]
    c["q"] = dict(c["q"], **q)
    c.update(kw)
    return c


def edges_local():
    r = random.Random(18005)
    out = []
    for b in range(6):
        for d in (-1, 0, 1):
            for sub in (0, 1, 2):
                out.append(local_case(r, b, d, sub, msave=1))
    out.append(local_case(r, 0, 1, msave=0))
    for hv in (0, 1, 0xFF, 0x100, 2):
        c = local_case(r, 0, 1, msave=1)
        c["q"]["have"] = [hv] * 6
        out.append(c)
    for h in (0x7FC00000, 0x7F800000, 0xFF800000, 0x80000000, 0):
        c = local_case(r, 2, 1, msave=1)
        c["q"]["happy"] = [h]
        out.append(c)
    # all six pass, the master save emptied by TellTheWorld after each win
    for tn in (0, 1):
        c = local_case(r, 0, 1, msave=1)
        c["q"].update(have=[0] * 6, won=[1, 2, 3, 1, 2, 3], tellnull=[tn], visitors=[11], inpark=[21, 41], happy=[fb(31.0)], research=[1], profit=[51], lastmonths=[61],
                      span=[71 * MONTH])
        out.append(c)
    return out


def global_case(r, passing, **kw):
    c = base(r, "glob")
    c["slots"] = [0] * 7 + [100, 50, 20, 0, 30]
    q = dict(coaster=[100 + (1 if passing & 1 else -1 + 0)], gokart=[50 + (1 if passing & 2 else 0)], longest=[20 + (1 if passing & 4 else 0)],
             cells=[30 + (1 if passing & 8 else 0)], thing=[0x1111, 0x2222, 0x3333], won=[r.choice([1, 2, 3, 4])], tellnull=[0])
    c["q"] = dict(c["q"], **q)
    c.update(kw)
    return c


def edges_global():
    r = random.Random(18006)
    out = [global_case(r, p, msave=1) for p in range(16)]
    out.append(global_case(r, 15, msave=0))
    out.append(with_q(global_case(r, 15, msave=1), tellnull=[1]))
    for v in (MASK, 0x80000000, 0x7FFFFFFF, 0):
        for key in ("coaster", "longest", "cells"):
            out.append(with_q(global_case(r, 0, msave=1), **{key: [v]}))
    for g in (255, 0, 128, 51, 50):
        out.append(with_q(global_case(r, 0, msave=1), gokart=[g]))
    for won in (0, 1, 4, MASK):
        out.append(with_q(global_case(r, 15, msave=1), won=[won]))
    return out


def edges_secret():
    r = random.Random(18007)
    out = []
    for have in (0, 1, 0xFF, 0x100, 2):
        for cam in (100, 99, 101, 0x164, 0, MASK):
            out.append(with_q(base(r, "secret", msave=1), have=[have], cameras=[cam], won=[1]))
    for won in (0, 1, 3):
        for tn in (0, 1):
            for m in (1, 0):
                out.append(with_q(base(r, "secret", msave=m), have=[0], cameras=[100], won=[won], tellnull=[tn]))
    return out


def edges_earned(fn):
    def edges():
        r = random.Random(18010 + ord(fn[-1]))
        out = []
        for won in (0, 1, 2, 3, 4, MASK, 0x80000000, 0x100):
            for tn in (0, 1):
                for m in (1, 0):
                    c = with_q(base(r, fn, msave=m), won=[won], tellnull=[tn], howmany=[r.choice([0, 3, MASK])])
                    c["args"] = [r.choice([0, 1, 2, 3, 5, MASK]) & MASK] + ([r.choice([0, 1, MASK, 0x7FFFFFFF, 0x80000000, 77])] if fn == "earn_g" else [])
                    out.append(c)
        return out
    return edges


def edges_tellfirst():
    r = random.Random(18020)
    out = []
    for inclsec in (0, 1, 2, MASK, 0x80000000, 3):
        for m in (1, 0):
            for ce in (1, 0):
                for nn in ([0], [1], [1, 0], [0, 1]):
                    out.append(with_q(base(r, "tellfirst", msave=m, centre=ce), inclsec=[inclsec], newnull=nn))
    return out


# ---------------------------------------------------------------------------------------------------------------- the advisor message

def request_bytes(r, little):
    fields = (r.getrandbits(16), r.getrandbits(32), r.getrandbits(32) - (1 << 31), r.choice([0x137, 0x13B, 5, r.getrandbits(32) - (1 << 31)]),
              r.choice([-1, 0, 7, r.getrandbits(32) - (1 << 31)]), r.choice([0, 1, 2, 0xFF]), r.choice([0, 1, 0xC3]), r.choice([0, 1, 0x80]))
    e = "<" if little else ">"
    return struct.pack(e + "H2BIiiiBBBB", fields[0], 0xC3, 0xC3, fields[1], fields[2], fields[3], fields[4], fields[5], fields[6], fields[7], 0xC3)


def ctor_case(r):
    return {"seed": r.getrandbits(32)}


def ctor_ppc(e, c):
    r = random.Random(c["seed"])
    e.cl = L = CL.PpcLog({}, {})
    L.deletes = 0
    e.news.clear()
    blk = e.alloc(0x40, POISON * 0x40)
    req = e.alloc(0x18, request_bytes(r, False))
    s = STUBS.activate(e)
    try:
        import tpwtest
        ret_ = tpwtest.call_ppc(e, FN["msg_ctor"], [blk, req])[0]
    finally:
        CL.Stubs.deactivate(e, s)
    raw = e.read(blk + 8, 0x18)
    ident = struct.unpack(">H", raw[0:2])[0]
    sec, resp, msg, score, valid, override, overridden = struct.unpack(">IiiiBBB", raw[4:0x17])
    out = [e.u32(blk + 4), ident, sec, resp & MASK, msg & MASK, score & MASK, valid, override, overridden, raw[2], raw[3], raw[0x17],
           1 if e.read(blk + 0x20, 0x20) == POISON * 0x20 else 0, 1 if e.u32(blk) == MSG_VTABLE else 0]
    return {"same": int(ret_ == blk), "out": out, "log": L.log}


def ctor_host(lib, c):
    bind(lib)
    r = random.Random(c["seed"])
    CL.host_begin(lib, {}, {})
    blk = lib.tpw_gt_alloc(0x40)
    C.memset(blk, 0xA5, 0x40)
    out = (C.c_uint32 * 16)()
    lib.tpw_gt_msg_ctor(blk, 0x40, request_bytes(r, True), out)
    return {"same": 1, "out": list(out)[:14], "log": CL.host_end(lib)}


def type_ppc(e, c):
    e.cl = L = CL.PpcLog({}, {})
    obj = e.alloc(0x20, POISON * 0x20)
    w32(e, obj + 4, c["flags"])
    s = STUBS.activate(e)
    try:
        import tpwtest
        r3 = tpwtest.call_ppc(e, FN["msg_type"], [obj])[0] & MASK
    finally:
        CL.Stubs.deactivate(e, s)
    return {"type": r3, "log": L.log}


def type_host(lib, c):
    bind(lib)
    CL.host_begin(lib, {}, {})
    t = lib.tpw_gt_msg_type(c["flags"]) & MASK
    CL.host_end(lib)
    return {"type": t, "log": []}


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def heap(lib):
    return list((C.c_int32 * 4).in_dll(lib, "tpw_cl_heap"))


def dtor_ppc(e, c):
    e.cl = L = CL.PpcLog({}, {})
    L.deletes = 0
    obj = e.alloc(0x40, POISON * 0x40) if c["present"] else 0
    L.labels[obj] = 3
    if obj:
        e.write(obj, struct.pack(">II", 0x11111111, 0x11223344))
    s = CL.Stubs()
    s.real.update(OWN)
    s.imp("__dl__FPv", op_delete)
    saved = s.activate(e)
    try:
        import tpwtest
        ret_ = tpwtest.call_ppc(e, FN["msg_dtor"], [obj, c["flag"]])[0]
    finally:
        CL.Stubs.deactivate(e, saved)
    explicit = bool(obj) and s16(c["flag"]) <= 0
    return {"same": int(ret_ == obj), "vptr": int(obj == 0 or e.u32(obj) != 0x11111111), "flags": e.u32(obj + 4) if explicit else 0, "log": L.log,
            "deletes": L.deletes}


def dtor_host(lib, c):
    bind(lib)
    CL.host_begin(lib, {}, {})
    before = heap(lib)
    out = (C.c_uint32 * 2)()
    lib.tpw_gt_msg_dtor(s32(c["flag"]), c["present"], out)
    now = heap(lib)
    explicit = c["present"] and s16(c["flag"]) <= 0
    return {"same": 1, "vptr": 1, "flags": out[0] if explicit else 0, "log": [x for x in CL.host_end(lib) if not str(x).startswith(("('Request", "['Request", "Request"))], "deletes": now[2] - before[2]}


def dtor_cases():
    flags = [0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, MASK, 0x80000000, 0x12340001, 0x1234FFFF, 0x7FFF0000, 0xFFFF0001, 0xFFFF8000, 0x00010000]
    return [dict(flag=f, present=1) for f in flags] + [dict(flag=f, present=0) for f in (0, 1, 0x7FFF, 0x12340001, 0x8000, MASK)]


def dtor_random(r):
    return dict(flag=r.choice([r.getrandbits(32), r.getrandbits(16), r.getrandbits(16) | 0x80000000, 1, 0, MASK]), present=r.choice([1, 1, 1, 0]))


def type_cases():
    return [dict(flags=f) for f in (0, 1, MASK, 0x80000000, 0x11223344)]


def type_random(r):
    return dict(flags=r.getrandbits(32))


# The tests run in this order: SpendTicketOn first, so that its first edge case is the first thing that reaches the game type object (the static is built once per process).
TESTS = [
    FnTest("CGoldenTicketControl::SpendTicketOn", ppc, host, edge=edges_spend, rnd=mk("spend")),
    FnTest("CGoldenTicketControl::HowManyTickets", ppc, host, edge=edges_howmany, rnd=mk("howmany")),
    FnTest("CGoldenTicketControl::HasBeenBought", ppc, host, edge=edges_bought, rnd=mk("bought")),
    FnTest("CGoldenTicketControl::CanIAfford", ppc, host, edge=edges_afford, rnd=mk("afford")),
    FnTest("CGoldenTicketControl::CheckForLocal", ppc, host, edge=edges_local, rnd=mk("local")),
    FnTest("CGoldenTicketControl::CheckForGlobal", ppc, host, edge=edges_global, rnd=mk("glob")),
    FnTest("CGoldenTicketControl::CheckForSecret", ppc, host, edge=edges_secret, rnd=mk("secret")),
    FnTest("CGoldenTicketControl::EarnedGoldenTicket (local)", ppc, host, edge=edges_earned("earn_l"), rnd=mk("earn_l")),
    FnTest("CGoldenTicketControl::EarnedGoldenTicket (global)", ppc, host, edge=edges_earned("earn_g"), rnd=mk("earn_g")),
    FnTest("CGoldenTicketControl::EarnedGoldenTicket (secret)", ppc, host, edge=edges_earned("earn_s"), rnd=mk("earn_s")),
    FnTest("CGoldenTicketControl::TellIfFirst", ppc, host, edge=edges_tellfirst, rnd=mk("tellfirst")),
    FnTest("CMsgAdvisor::CMsgAdvisor", ctor_ppc, ctor_host, edge=lambda: [ctor_case(random.Random(k)) for k in range(6)], rnd=ctor_case),
    FnTest("CMsgAdvisor::GetType", type_ppc, type_host, edge=type_cases, rnd=type_random),
    FnTest("CMsgAdvisor::~CMsgAdvisor", dtor_ppc, dtor_host, edge=dtor_cases, rnd=dtor_random),
]
