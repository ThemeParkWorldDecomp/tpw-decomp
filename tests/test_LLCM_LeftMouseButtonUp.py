"""Unit v5883, the cell system's left button release (each edit mode finishes its click or drag), against
libtpw_unit_LLCM_LeftMouseButtonUp.dylib (tests/unit_LLCM_LeftMouseButtonUp.cpp), main app ("SimTheme Park"),
src/game/park/LLCellMouse.cpp:
  LLCM_LeftMouseButtonUp 0x10084ca4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee is a
fake on both sides that records its arguments (cells as offsets from the world block, objects as labels) and answers from the case's
queues; the mode tests answer from the case's mode, the line drawers may move the end cell, the start finders write their out cells.

The world of a case: the world block's first 1026 map cells (type) and track cells (id, mesh word, parent), from a seed; three ride
objects (cell, rotation, track ride, whole flag) in the thing array (entry 0 empty or not); the selected peep, the queue start and
track start cells (their ids), the advisor and the bank (labels); every word the function reads or writes. What is compared: those words
(pointers as labels), each object's whole flag (+0x28), every track cell's mesh word, the event log.

No fused operations; the one fctiwz converts 10 * a cell coordinate (small, exact). Not tested: a NULL current object in the modes that
read it unconditionally (0xb, 0x10, 0xe, 0x13, 0x15 and the ride modes' track part), cells outside the 1026 written (the coordinates stay
in 0..127 by 0..7, the moves flip the lowest bit only).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCM_LeftMouseButtonUp"
MASK = 0xFFFFFFFF

# ---- the unit's own function (address, size from the MANIFEST)
FN_UP = 0x10084CA4
SIZES = {FN_UP: 14520}
OWN = set(SIZES)

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_llcmup_addr(name)
W32 = dict(x0=0x102E4804, y0=0x102E4800, DB30=0x102DDB30, DE0B8=0x102DE0B8, DB40=0x102DDB40, PrevA=0x1029E214, PrevB=0x1029E218,
           E288=0x1029E288, E28C=0x1029E28C, DB44=0x102DDB44, **{"9F18": 0x1029DF18, "9F1C": 0x1029DF1C, "9F20": 0x1029DF20},
           FBX=0x1029E02C, FBY=0x1029E030, E034=0x1029E034, E038=0x1029E038, E0DC=0x102DE0DC, **{"9F14": 0x1029DF14},
           DB50=0x102DDB50, DB54=0x102DDB54, **{"47C8": 0x102E47C8, "9F28": 0x1029DF28, "9F2C": 0x1029DF2C}, Revise=0x1029DF30,
           C214=0x102DC214, C208=0x102DC208, Width=0x102E47FC, Height=0x102E47F8, HoardRight=0x1029E03C, HoardBottom=0x1029E040,
           **{"9F44": 0x1029DF44, "9F48": 0x1029DF48})
W16 = {"DB68": 0x102DDB68, "9F24": 0x1029DF24, "9F26": 0x1029DF26, "DB58": 0x102DDB58, "DB5A": 0x102DDB5A}
W8 = {"4818": 0x102E4818, "E0CC": 0x102DE0CC, "DB34": 0x102DDB34, "E090": 0x102DE090, "TCF": 0x102DDB35}
PTRS = {"DB64": 0x102DDB64, "DB4C": 0x102DDB4C, "DB48": 0x102DDB48}
ADDR = dict(W32, **W16, **W8, **PTRS, DB6C=0x102DDB6C, World=0x1037EBFC, Rec=0x1037EC68, Bank3=0x102F7CA0, Things=0x1034CBFC)
ILLEGAL_TABLE = 0x1029DF4C

# ---- the host's offsets and sizes the module relies on, checked once against tpw_llcmup_layout
#   CObject mRotation, mF24, mF28; sizeof CThingArrayEntry; CThing mCellX, mCellY; CObject mBase; CCellBase mCellId, mMeshInstance,
#   mType, mParentID; sizeof CTrackCell
HOST_LAYOUT = [0x10, 0x24, 0x28, 48, 4, 6, 0, 0, 4, 8, 0x10, 0x28]

MAP0 = 0x2D8                  # the map cells in the world block (0x44 bytes each, index - 1)
TRACK0 = 0x2D8 + 0x110000     # the track cells (0x28 bytes each)
NCELLS = 1026
WORLD_SIZE = TRACK0 + NCELLS * 0x28 + 0x40
OBJ_SIZE = 0x100

# labels: the three rides, the selected peep, the queue start and track start cells, the advisor, the bank
OBJA, OBJB, OBJC, PEEP, SQ, ST, ADVISOR, BANK = range(1, 9)
LABELS = (OBJA, OBJB, OBJC, PEEP, SQ, ST, ADVISOR, BANK)

PPC = {}      # the PowerPC side's persistent blocks: "world" and the labels


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ofs(p):
    return s32((p - PPC["world"]) & MASK)


def internal(addr):
    def put(fn):
        STUBS.internal(addr, fn)
        return fn
    return put


def simple(addr, name, conv, answer=None):
    """A fake that records its argument registers (r3 up) through conv and answers None (r3 left), a queue (`answer` a key) or a
    function of the emulator."""
    def fn(e):
        L = e.cl
        L.rec(name, tuple(c(e, e.r(3 + i)) for i, c in enumerate(conv)))
        if answer is None:
            return
        if callable(answer):
            e.set_r(3, answer(e) & MASK)
        else:
            e.set_r(3, L.ret(answer) & MASK)
    STUBS.internal(addr, fn)


I = lambda e, v: s32(v)
U = lambda e, v: v & MASK
B = lambda e, v: v & 0xFF
HU = lambda e, v: v & 0xFFFF
HS = lambda e, v: T.s16(v)
WH = lambda e, v: (v >> 16) & 0xFFFF
LB = lambda e, v: e.cl.lab(v)
OF = lambda e, v: ofs(v)


STUBS.internal(0x1006FCBC, CL.recorder("CTRACE", "us"))
STUBS.internal(0x1000DDE8, CL.recorder("GAME_ASSERTMSG", "is"))
simple(0x10083180, "MouseMoved", [I, I, HU])
simple(0x10088970, "Illegal", [], "Illegal")
simple(0x10091568, "IsMode", [I], lambda e: 1 if s32(e.r(3)) == e.cl.ret("mode") else 0)
STUBS.internal(0x10143FF8, lambda e: e.cl.rec("Rec3", (s32(e.r(4)), e.r(5) & 0xFF, e.r(6) & 0xFF)))
STUBS.internal(0x101440E0, lambda e: e.cl.rec("Rec4", (s32(e.r(4)), e.r(5) & 0xFF, e.r(6) & 0xFF, e.r(7) & 0xFFFF)))
simple(0x101EFF4C, "Peep", [], lambda e: PPC[PEEP] if e.cl.ret("Peep") else 0)
simple(0x10088590, "SetCurrentObject", [B])
simple(0x100DD9FC, "SOUND", [I, I, I, I, I, I], lambda e: 0)
simple(0x1009B158, "MapCT", [OF, I, I], "MapCT")
simple(0x10091DD4, "Reset", [])
simple(0x10091E5C, "Add", [I, I], lambda e: 1)


@internal(0x1000DEB4)
def stub_advreq(e):
    e.cl.rec("AdvReq", (s32(e.r(4)), (e.r(5) >> 16) & 0xFFFF, e.r(6) & MASK, s32(e.r(7)), e.r(8) & 0xFF, s32(e.r(9))))


simple(0x1013EAB0, "GetAdvisor", [], lambda e: PPC[ADVISOR])
simple(0x1000AE48, "QueueRequest", [LB], lambda e: 0)
simple(0x10091E18, "Entries", [], "Entries")
simple(0x100914F4, "GetMode", [], "GetMode")
simple(0x1010952C, "RecalcBOQ", [LB])
simple(0x10090E18, "SetMode", [I, B])
simple(0x100911F0, "SetModeNR", [I, B])
simple(0x100A3574, "IsTrack", [OF], "IsTrack")
simple(0x100A367C, "IsTrackX", [OF], "IsTrackX")
simple(0x100A36CC, "StartBlk", [OF], "StartBlk")
simple(0x100A399C, "Seg", [OF], lambda e: T.s16(e.cl.ret("Seg")))
simple(0x100A1594, "TrackCT", [OF, I, I, B], "TrackCT")
simple(0x1012FDDC, "Nbr", [OF, B, I], lambda e: (e.r(3) + 0x28) if e.cl.ret("Nbr") else 0)
simple(0x100F49A0, "MapEquiv", [OF], lambda e: PPC["world"] + MAP0 + ((ofs(e.r(3)) - TRACK0) // 0x28) * 0x44)
simple(0x10081810, "ModDir", [OF, B])
simple(0x10081608, "ModNbr", [OF, B])
simple(0x10102500, "OPPOSITE", [B], lambda e: (((e.r(3) & 0xFF) << 4) | ((e.r(3) & 0xFF) >> 4)) & 0xFF)
simple(0x100270E0, "FreeTrack", [I])
simple(0x10090DE0, "RideRot", [], "RideRot")
simple(0x1008A248, "PlaceRide", [I, I, I, I, I, I], "PlaceRide")
simple(0x10082CF0, "Rebuild", [I, I])
simple(0x1008CE04, "BuildDB", [B], "BuildDB")
simple(0x100888FC, "GetCurrent", [], lambda e: e.u32(ADDR["DB6C"]))
simple(0x1008C7B0, "TrackAnim", [LB, B])
simple(0x1008CAD8, "TrackAnimRev", [LB, B])
simple(0x1010AAAC, "Openable", [LB], "Openable")
simple(0x1010ACDC, "Reopen", [LB])
simple(0x10090D40, "SetRideRot", [I, B])
simple(0x10091A08, "RideQueue", [], "RideQueue")
simple(0x10091AD0, "RideTrack", [], "RideTrack")
simple(0x1008FA34, "FixBridges", [LB, B, B, B], lambda e: e.cl.ret("FixBr") & 0xFF)
simple(0x1009024C, "FixBridgesRev", [LB, B, B, B], lambda e: 0)
simple(0x1009668C, "HoardImage", [])
simple(0x1009464C, "HoardLegal", [I, I, I, I, B], "HoardLegal")
simple(0x100961C0, "ReviseHoarding", [I, I, I, I])
simple(0x10091D98, "Cost", [], "Cost")
simple(0x1013EDFC, "GetBank", [], lambda e: PPC[BANK])
simple(0x100F33B0, "Withdraw", [LB, U])
simple(0x101A8C58, "Cursor", [HS])
simple(0x10096A0C, "Clear", [I, I, I, I], "Clear")
simple(0x1008EBC0, "Kill", [I, I, I, I], lambda e: 1)
simple(0x100892C4, "DeleteRide", [I, I, I])
simple(0x101278F0, "Patrol", [LB, WH, WH])
simple(0x10092644, "QueueStack", [LB], lambda e: 0)
simple(0x10092E44, "TrackStack", [LB])
simple(0x10091F5C, "FixFirst", [B])
simple(0x100936F4, "HoardStack", [])
simple(0x10057F80, "CoasterClick", [I])
simple(0x10052A30, "PlacePylon", [U, U, I, OF], lambda e: 0)
simple(0x100427D0, "EndTrack", [I])
simple(0x100579CC, "MovePylon", [U, U], "MovePylon")
simple(0x10057DF4, "AbortPylon", [])
simple(0x10088934, "SetIllegal", [I])
simple(0x1008DDFC, "Trace", [OF, OF, I], "Trace")


def _move(e, key, pa, pb):
    if e.cl.ret(key):
        e.write(pa, struct.pack(">I", (e.u32(pa) ^ 1) & MASK))
        e.write(pb, struct.pack(">I", (e.u32(pb) ^ 1) & MASK))


def line(name):
    def fn(e):
        L = e.cl
        L.rec(name, (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), e.s32(e.r(6)), e.s32(e.r(7))))
        _move(e, name + "Move", e.r(6), e.r(7))
        e.set_r(3, L.ret(name) & MASK)
    return fn


STUBS.internal(0x1009C0D0, line("DrawLine"))
STUBS.internal(0x100A30F8, line("DrawTrackLine"))


def offset_fake(name):
    def fn(e):
        L = e.cl
        pa, pb = e.r(3), e.r(4)
        L.rec(name, (e.s32(pa), e.s32(pb), s32(e.r(5))))
        _move(e, name + "Move", pa, pb)
        e.set_r(3, L.ret(name) & 0xFF)
    return fn


STUBS.internal(0x1008C620, offset_fake("OffStart"))
STUBS.internal(0x1008C6D8, offset_fake("OffEnd"))


@internal(0x10103A04)
def stub_thing_id(e):
    L = e.cl
    L.rec("ThingId", (ofs(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("ThingId") & 0xFFFF))


@internal(0x1008C128)
def stub_start_queue(e):
    L = e.cl
    pa, pb = e.r(4), e.r(5)
    L.rec("StartQueue", (e.r(3) & MASK, e.s32(pa), e.s32(pb), 1 if e.r(3) & MASK else 0))
    e.write(pa, struct.pack(">I", L.ret("SQX") & MASK))
    e.write(pb, struct.pack(">I", L.ret("SQY") & MASK))
    e.set_r(3, PPC[SQ])


@internal(0x1008C320)
def stub_start_track(e):
    L = e.cl
    pa, pb = e.r(4), e.r(5)
    L.rec("StartTrack", (e.r(3) & MASK, e.s32(pa), e.s32(pb), s32(e.r(6)), 1 if e.r(3) & MASK else 0))
    e.write(pa, struct.pack(">I", L.ret("STX") & MASK))
    e.write(pb, struct.pack(">I", L.ret("STY") & MASK))
    e.set_r(3, PPC[ST])


@internal(0x100889AC)
def stub_tcb(e):
    L = e.cl
    p1, p2, p3, p4 = e.r(5), e.r(6), e.r(7), e.r(8)
    L.rec("TCB", (s32(e.r(3)), s32(e.r(4)), e.s32(p1), e.s32(p2), e.s32(p3), e.s32(p4)))
    m = L.ret("TCB")
    v3, v4 = e.u32(p1) ^ m, e.u32(p2) ^ m
    e.write(p3, struct.pack(">I", v3 & MASK))
    e.write(p4, struct.pack(">I", v4 & MASK))


@internal(0x100828E4)
def stub_cover(e):
    L = e.cl
    pa, pb = e.r(3), e.r(4)
    L.rec("Cover", (e.s32(pa), e.s32(pb)))
    _move(e, "CovMove", pa, pb)


def stub_abs(e):
    e.set_r(3, abs(s32(e.r(3))) & MASK)


STUBS.imp("abs", stub_abs)


def setup(runner):
    T.install_std_stubs(runner.emu)
    e = runner.emu
    PPC["world"] = T.persist_alloc(e, WORLD_SIZE)
    for lab in LABELS:
        PPC[lab] = T.persist_alloc(e, OBJ_SIZE)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_half(self, a):
        return struct.unpack(self.fmt[0] + "H", self.read(a, 2))[0]

    def label_of(self, p, low32=False):
        if p == 0:
            return 0
        for lab in LABELS:
            if (self.blk(lab) & MASK if low32 else self.blk(lab)) == p:
                return lab
        return -1


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    PSIZE = 4
    STRIDE = 0x14

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({PPC[lab]: lab for lab in LABELS}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def blk(self, lab):
        return PPC[lab]

    def world(self):
        return PPC["world"]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def wptr(self, a, p):
        self.write(a, struct.pack(">I", p & MASK))

    def rptr(self, a):
        return struct.unpack(">I", self.read(a, 4))[0]

    def set_current(self, p):
        self.words(ADDR["DB6C"], [p])

    def log(self):
        return self.L.log

    def call(self, x, y, shift, flag):
        T.call_ppc(self.e, FN_UP, [x & MASK, y & MASK, shift & MASK, flag & MASK])


class Host(Side):
    """The same on the host library."""
    fmt = "<I"
    PSIZE = 8
    STRIDE = 48

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        if not hasattr(lib, "blocks"):
            lib.blocks = {"world": lib.tpw_llcmup_alloc(WORLD_SIZE)}
            for lab in LABELS:
                lib.blocks[lab] = lib.tpw_llcmup_alloc(OBJ_SIZE)
        b = lib.blocks
        lib.tpw_llcmup_blocks(b[PEEP], b[SQ], b[ST], b[ADVISOR], b[BANK])
        CL.host_begin(lib, {lab: b[lab] for lab in LABELS}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_llcmup_addr(name.encode())

    def blk(self, lab):
        return self.lib.blocks[lab]

    def world(self):
        return self.lib.blocks["world"]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, p):
        self.write(a, struct.pack("<Q", p))

    def rptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def set_current(self, p):
        self.words(self.g("DB6C"), [p])
        self.wptr(self.g("CurObj"), p)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, x, y, shift, flag):
        self.lib.tpw_llcmup_call(s32(x), s32(y), shift & MASK, s32(flag))


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    p = "tpw_llcmup_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "blocks").restype, getattr(lib, p + "blocks").argtypes = None, [C.c_uint64] * 5
    getattr(lib, p + "call").restype, getattr(lib, p + "call").argtypes = None, [C.c_int32, C.c_int32, C.c_uint32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_world(s, seed):
    rr = random.Random(seed)
    e = s.fmt[0]
    maps = bytearray(NCELLS * 0x44)
    tracks = bytearray(NCELLS * 0x28)
    for i in range(NCELLS):
        struct.pack_into(e + "I", maps, i * 0x44 + 8, rr.choice([0, 1, 4, 7]))
        struct.pack_into(e + "H", tracks, i * 0x28 + 0, rr.randint(1, 1024))
        struct.pack_into(e + "I", tracks, i * 0x28 + 4, rr.getrandbits(32))
        struct.pack_into(e + "H", tracks, i * 0x28 + 0x10, rr.randint(1, 1024))
    w = s.world()
    s.write(w + MAP0, bytes(maps))
    s.write(w + TRACK0, bytes(tracks))


def build(s, case):
    """Writes the case's globals and blocks through Side's writers: the same code for both memories (offsets are the Mac's)."""
    build_world(s, case["wseed"])
    gl = case["g"]
    for name in W32:
        s.words(s.g(name), [gl[name]])
    for name in W16:
        s.half(s.g(name), gl[name])
    for name in W8:
        s.write(s.g(name), bytes([gl[name] & 0xFF]))
    s.words(s.g("Rec"), [gl["Rec"]])
    s.words(s.g("Bank3"), [gl["Bank3"]])
    s.wptr(s.g("World"), s.world())
    for lab, (cx, cy, rot, f24, f28) in zip((OBJA, OBJB, OBJC), case["objs"]):
        a = s.blk(lab)
        s.write(a, bytes(OBJ_SIZE))
        s.half(a + 0, lab)
        s.write(a + 4, bytes([cx]))
        s.write(a + 6, bytes([cy]))
        s.words(a + 0x10, [rot])
        s.words(a + 0x24, [f24])
        s.words(a + 0x28, [f28])
    s.half(s.blk(SQ), case["sq_id"])
    s.half(s.blk(ST), case["st_id"])
    things = s.g("Things")
    for i, lab in enumerate([OBJA if case["thing0"] else 0, OBJA, OBJB, OBJC]):
        s.wptr(things + i * s.STRIDE, s.blk(lab) if lab else 0)
    for name in PTRS:
        lab = case["ptr"][name]
        s.wptr(s.g(name), s.blk(lab) if lab else 0)
    cur = case["ptr"]["DB6C"]
    s.set_current(s.blk(cur) if cur else 0)


def observe(s, case):
    """Everything the call may change, read back the same way on both sides; pointers as labels."""
    out = {"log": s.log()}
    out["w32"] = [s32(s.rd_word(s.g(n))) for n in W32]
    out["w16"] = [s.rd_half(s.g(n)) for n in W16]
    out["w8"] = [s.read(s.g(n), 1)[0] for n in W8]
    out["ptrs"] = [s.label_of(s.rptr(s.g(n))) for n in PTRS]
    out["cur"] = s.label_of(s.rd_word(s.g("DB6C")), low32=True)
    out["f28"] = [s32(s.rd_word(s.blk(lab) + 0x28)) for lab in (OBJA, OBJB, OBJC)]
    tracks = s.read(s.world() + TRACK0, NCELLS * 0x28)
    out["mesh"] = [struct.unpack_from(s.fmt[0] + "I", tracks, i * 0x28 + 4)[0] for i in range(NCELLS)]
    return out


def run(s, case):
    try:
        build(s, case)
        s.call(*case["args"])
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    if case.get("show"):
        table = struct.unpack(">55I", e.read(ILLEGAL_TABLE, 220))
        names = {0x1008853C: "0", 0x100884E4: "1", 0x100884F8: "2"}
        print("illegal table", "".join(names.get(w, "?") for w in table), [hex(w) for w in set(table) if w not in names])
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

MODES = [0, 1, 3, 4, 0x3B, 0xB, 0x10, 0x19, 0x39, 0x3A, 0x33, 0x32, 0x34, 0x35, 0x36, 0x14, 0xE, 0x13, 0x1A, 0x15, 0x16, 0x38, 2, 0x40]
NULL_SAFE = {0, 1, 3, 0x14, 0x15, 0x16, 0x19, 0x32, 0x33, 0x34, 0x35, 0x36, 0x39, 0x3A, 0x38, 2, 0x40}
ROTS = [0, 0x5A, 0xB4, 0x10E, 7]


def bits(r, n, choices=(0, 1)):
    return [r.choice(choices) for _ in range(n)]


def make_case(r, mode=None):
    m = r.choice(MODES) if mode is None else mode
    x = r.randrange(0, 128)
    y = r.randrange(0, 8)
    if r.random() < 0.1:
        x = y = 0
    if r.random() < 0.4:
        x0 = y0 = -1
    else:
        x0 = x if r.random() < 0.5 else r.randrange(0, 128)
        y0 = y if r.random() < 0.6 else r.randrange(0, 8)
    g = {
        "x0": x0, "y0": y0, "DB30": r.randrange(0, 4), "DE0B8": r.randrange(0, 9), "DB40": r.randrange(0, 3),
        "PrevA": r.randrange(0, 0x100), "PrevB": r.randrange(0, 0x100), "E288": r.randrange(0, 0x100), "E28C": r.randrange(0, 0x100),
        "DB44": r.randrange(0, 3), "9F18": r.randrange(0, 128), "9F1C": r.randrange(0, 8), "9F20": r.choice(ROTS),
        "FBX": r.randrange(0, 128), "FBY": r.randrange(0, 8), "E034": r.randrange(0, 128), "E038": r.randrange(0, 8),
        "E0DC": r.choice([0, 0, 1]), "9F14": r.randrange(-1, 3), "DB50": r.randrange(0, 8), "DB54": r.randrange(0, 128),
        "47C8": r.randrange(0, 3), "9F28": r.randrange(0, 128), "9F2C": r.randrange(0, 8), "Revise": r.randrange(0, 3),
        "C214": r.choice([0, 1]), "C208": r.choice([0, 1]), "Width": r.randrange(4, 131), "Height": r.randrange(4, 11),
        "HoardRight": r.randrange(0, 128), "HoardBottom": r.randrange(0, 8), "9F44": r.randrange(0, 4), "9F48": r.randrange(0, 4),
        "DB68": r.randrange(1, 1025), "9F24": r.choice([0xFFFF, r.randrange(0, 6)]), "9F26": r.choice([0xFFFF, r.randrange(0, 6)]),
        "DB58": r.randrange(1, 1025), "DB5A": r.randrange(1, 1025), "4818": r.choice([0, 0, 1]), "E0CC": r.randrange(0, 256),
        "DB34": r.choice([0, 1]), "E090": r.choice([0, 1]), "TCF": r.choice([0, 1]), "Rec": r.choice([0, 0, 1]),
        "Bank3": r.randrange(0, 1000),
    }
    q = {
        "mode": [m],
        "Illegal": [r.choice([0, 0, 0, 1]), r.choice([0, 0, 0, 1])],
        "Peep": [r.choice([0, 1])],
        "MapCT": bits(r, 3, (0, 1, 2)),
        "Entries": bits(r, 3, (0, 1, 2)),
        "DrawLine": bits(r, 12, (0, 1, 1)),
        "DrawLineMove": bits(r, 12, (0, 0, 1)),
        "DrawTrackLine": bits(r, 24, (0, 1, 1)),
        "DrawTrackLineMove": bits(r, 24, (0, 0, 1)),
        "GetMode": [r.choice([0x16, 0x16, 4, 1, 3, 0xB, 0x36, 0x37, 0x3A, 0x40, -1, r.randrange(0, 0x3B)]) for _ in range(4)],
        "RideRot": [r.choice(ROTS)],
        "RideQueue": [r.choice([0, 1, 1])],
        "RideTrack": [r.choice([0, 1, 2, 3]) for _ in range(5)],
        "PlaceRide": [r.getrandbits(32)],
        "SQX": [r.randrange(0, 128)], "SQY": [r.randrange(0, 8)],
        "STX": [r.randrange(0, 128)], "STY": [r.randrange(0, 8)],
        "OffStart": [r.randrange(0, 256)], "OffStartMove": bits(r, 2), "OffEnd": [r.randrange(0, 256)], "OffEndMove": bits(r, 2),
        "BuildDB": [r.choice([0, 1, 5])],
        "Openable": [r.choice([0, 1])],
        "IsTrack": [r.choice([0, 1])], "IsTrackX": bits(r, 2), "StartBlk": [r.choice([0, 0, 1])],
        "Seg": [r.randrange(-3, 4) for _ in range(3)],
        "TrackCT": [r.choice([0, 1, 9, 9]) for _ in range(2)],
        "ThingId": [r.randrange(1, 4)],
        "Nbr": [r.choice([0, 1])], "FixBr": [r.choice([0, 1])], "Trace": [r.choice([-1, 0, 1])], "TCB": [r.choice([0, 1])],
        "HoardLegal": [r.choice([0, 1])], "Cost": [r.randrange(0, 5000)], "Clear": [r.choice([0, 1])], "CovMove": [r.choice([0, 1])],
        "MovePylon": [r.getrandbits(32)],
    }
    cur = r.choice([OBJA, OBJB, OBJC])
    safe = m in NULL_SAFE or (m in (4, 0x3B) and set(q["RideTrack"]) <= {0, 3} and g["E0DC"] == 0)
    if safe and r.random() < 0.3:
        cur = 0
    objs = [[r.randrange(0, 128), r.randrange(0, 8), r.choice(ROTS), r.randrange(0, 50), r.randrange(0, 3)] for _ in range(3)]
    return {
        "op": "up",
        "args": [x, y, r.choice([0, 0xC, 1, 0x100]), r.choice([0, 0, 0, 1])],
        "wseed": r.getrandbits(32),
        "g": g,
        "ptr": {"DB64": r.choice([0, PEEP]), "DB4C": r.choice([0, SQ]), "DB48": r.choice([0, ST]), "DB6C": cur},
        "objs": objs,
        "thing0": r.choice([0, 0, 1]),
        "sq_id": r.randrange(1, 1025),
        "st_id": r.randrange(1, 1025),
        "queues": q,
    }


def up_edges():
    """Every mode forty times (each with the branches its queues pick), the first case printing the illegal path's table."""
    r = random.Random(5883)
    first = True
    for m in MODES:
        for _ in range(40):
            c = make_case(r, m)
            if first:
                c["show"] = 1
                first = False
            yield c


def up_random(r):
    return make_case(r)


TESTS = [
    FnTest("LLCM_LeftMouseButtonUp", ppc, host, edge=up_edges, rnd=up_random),
]
