"""Unit v5182, the current object's track end and the track edit mode's start, against libtpw_unit_LLCM_SetCurrentObject.dylib
(tests/unit_LLCM_SetCurrentObject.cpp), main app ("SimTheme Park"), src/game/park/LLCellMouse.cpp:
  LLCM_SetCurrentObject 0x10088590, LLCM_StartTrackEditMode 0x10088a64

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (IsMode, the two track end offsets, the selected object getter and setter, GetUpgradeParent, the assertion, GetRideInfo,
RecordAction, GetDBInfo, the ride and mode setters, CoverUpgrade, GetMode, DrawTrackLine, the click stack builder, GetSegmentNumber,
ConnectionType and TraceTrack) are fakes on both sides that record their arguments and answer from the case's queues. Cells are logged by
their index from the world (the world is a fake base address, never dereferenced); objects by label.

The world of a case: every global the two functions read or write (random start values, so that a store that does not happen is seen),
four objects (cell bytes, type, rotation, the +0x28 word), the ride record words (+0x40, +0x9c, +0x288) and the thing table rows 0 and the
upgrade parent. What is compared: the event log and every global afterwards (the selected object as a label, the track cell as its index,
the floats as bits).

No fused operations. Not tested: a thing table row past 0x2800 (the original reads past the table), a NULL ride record (the original
dereferences it), a NULL selected object in LLCM_StartTrackEditMode (read unconditionally by the original).

Unit v5L28: the host ride record is the host RideInfo (8-byte balance values behind a 64-bit table header), so the host side writes each
ride word at the host offset of the same balance slot (tpw_slc_rideoff).

Unit v5L39: an image test of gLLCCE02A as loaded (tests/imagedata.py); the other cases write their own value.
"""
import ctypes as C
import random
import struct

import calllog as CL
import imagedata
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LLCM_SetCurrentObject"
MASK = 0xFFFFFFFF

FN_SET, FN_START = 0x10088590, 0x10088a64
SIZES = {FN_SET: 832, FN_START: 1372}
OWN = set(SIZES)

CALLEES = dict(
    IsMode=0x10091568, OffsetEnd=0x1008C6D8, OffsetStart=0x1008C620, GetSelectedObject=0x101EFFA4, GetUpgradeParent=0x1010DA64,
    Assert=0x1000DDE8, GetRideInfo=0x10154868, SetSelectedObject=0x101F0000, RecordAction=0x101441FC, GetDBInfo=0x10107CD4,
    SetRideQueue=0x10091994, SetRideTrack=0x10091A5C, GetRideTrack=0x10091AD0, SetModeNR=0x100911F0, CoverUpgrade=0x100828E4,
    GetMode=0x100914F4, DrawTrackLine=0x100A30F8, ConstructClick=0x10092E44, GetSegment=0x100A399C, ConnectionType=0x100A1594,
    TraceTrack=0x1008DDFC)

# name -> (Mac address, kind); kinds: w word, h halfword, b byte, f float bits, p pointer
GLOBALS = dict(
    df18=(0x1029DF18, "w"), df1c=(0x1029DF1C, "w"), df20=(0x1029DF20, "w"), df24=(0x1029DF24, "h"), df26=(0x1029DF26, "h"),
    e028=(0x1029E028, "h"), e02a=(0x1029E02A, "h"), e02c=(0x1029E02C, "w"), e030=(0x1029E030, "w"), e034=(0x1029E034, "w"),
    e038=(0x1029E038, "w"), hoardR=(0x1029E03C, "w"), hoardB=(0x1029E040, "w"), dc210=(0x102DC210, "w"), dc214=(0x102DC214, "w"),
    dc218=(0x102DC218, "w"), db48=(0x102DDB48, "p"), db50=(0x102DDB50, "w"), db54=(0x102DDB54, "w"), db58=(0x102DDB58, "h"),
    db5a=(0x102DDB5A, "h"), db5c=(0x102DDB5C, "h"), db68=(0x102DDB68, "h"), db6c=(0x102DDB6C, "w"), e0cc=(0x102DE0CC, "b"),
    g4800=(0x102E4800, "w"), g4804=(0x102E4804, "w"), f9bc0=(0x102F9BC0, "f"), f9bc4=(0x102F9BC4, "f"), world=(0x1037EBFC, "p"))
KIND = {k: v[1] for k, v in GLOBALS.items()}
THINGS = 0x1034CBFC
HOST_LAYOUT = [48, 0, 4, 6, 0xE, 0x10, 0x28]
WORLD = {"ppc": 0x30000000, "host": 0x40000000}
NOBJ = 4
OBJ_PPC = []
INFO = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cell(e, p):
    d = s32((p - WORLD["ppc"] - 0x1102D8) & MASK)
    return d // 0x28 if d % 0x28 == 0 else ("odd", d)


def ptr_of(e, k):
    return OBJ_PPC[k - 1] if k > 0 else 0


def st_ismode(e):
    e.cl.rec("IsMode", (s32(e.r(3)),))
    e.set_r(3, e.cl.ret("IsMode") & MASK)


def make_offset(name, fx, fy):
    def fn(e):
        x, y, rot = s32(e.u32(e.r(3))), s32(e.u32(e.r(4))), s32(e.r(5))
        e.cl.rec(name, (x, y, rot))
        e.write(e.r(3), struct.pack(">I", fx(x, rot) & MASK))
        e.write(e.r(4), struct.pack(">I", fy(y, rot) & MASK))
        e.set_r(3, e.cl.ret(name) & 0xFF)
    return fn


def st_getsel(e):
    e.cl.rec("GetSelectedObject", ())
    e.set_r(3, ptr_of(e, e.cl.ret("GetSelectedObject")))


def st_setsel(e):
    e.cl.rec("SetSelectedObject", (e.cl.lab(e.r(4)),))


def st_gup(e):
    e.cl.rec("GetUpgradeParent", (e.cl.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", e.cl.ret("GetUpgradeParent") & 0xFFFF))


def st_assert(e):
    text = e.cstr(e.r(4))
    total = sum((b if isinstance(b, int) else ord(b)) * (i + 1) for i, b in enumerate(text))
    e.cl.rec("GAME_ASSERTMSG", (s32(e.r(3)), total))


def st_getride(e):
    e.cl.rec("GetRideInfo", ((e.r(4) >> 16) & 0xFFFF,))
    e.set_r(3, INFO["ppc"])


def st_record(e):
    e.cl.rec("RecordAction", (s32(e.r(4)), e.r(5) & 0xFFFF))


def st_getdb(e):
    e.cl.rec("GetDBInfo", (e.cl.lab(e.r(3)),))
    e.set_r(3, INFO["ppc"] if e.cl.ret("GetDBInfo") else 0)


def st_setq(e):
    e.cl.rec("SetRideQueue", (s32(e.r(3)),))


def st_sett(e):
    e.cl.rec("SetRideTrack", (s32(e.r(3)),))


def st_gett(e):
    e.cl.rec("GetRideTrack", ())
    e.set_r(3, e.cl.ret("GetRideTrack") & MASK)


def st_setmode(e):
    e.cl.rec("SetModeNR", (s32(e.r(3)), e.r(4) & 0xFF))


def st_cover(e):
    x, y = s32(e.u32(e.r(3))), s32(e.u32(e.r(4)))
    e.cl.rec("CoverUpgrade", (x, y))
    e.write(e.r(3), struct.pack(">I", (x + 1) & MASK))
    e.write(e.r(4), struct.pack(">I", (y - 2) & MASK))


def st_getmode(e):
    e.cl.rec("GetMode", ())
    e.set_r(3, e.cl.ret("GetMode") & MASK)


def st_draw(e):
    e.cl.rec("DrawTrackLine", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.u32(e.r(6))), s32(e.u32(e.r(7)))))


def st_construct(e):
    e.cl.rec("ConstructTrackClickStack", (e.cl.lab(e.r(3)),))


def st_seg(e):
    e.cl.rec("GetSegmentNumber", (cell(e, e.r(3)),))
    e.set_r(3, e.cl.ret("GetSegmentNumber") & 0xFFFF)       # not extended: the original's extsh does that


def st_conn(e):
    e.cl.rec("ConnectionType", (cell(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), e.r(6) & 0xFF))
    e.set_r(3, e.cl.ret("ConnectionType") & MASK)


def st_trace(e):
    e.cl.rec("TraceTrack", (cell(e, e.r(3)), cell(e, e.r(4)), s32(e.r(5))))


STUBS.internal(CALLEES["IsMode"], st_ismode)
STUBS.internal(CALLEES["OffsetEnd"], make_offset("OffsetEnd", lambda x, r: x ^ 0x15, lambda y, r: y + r))
STUBS.internal(CALLEES["OffsetStart"], make_offset("OffsetStart", lambda x, r: x + r + 1, lambda y, r: y - 3))
STUBS.internal(CALLEES["GetSelectedObject"], st_getsel)
STUBS.internal(CALLEES["SetSelectedObject"], st_setsel)
STUBS.internal(CALLEES["GetUpgradeParent"], st_gup)
STUBS.internal(CALLEES["Assert"], st_assert)
STUBS.internal(CALLEES["GetRideInfo"], st_getride)
STUBS.internal(CALLEES["RecordAction"], st_record)
STUBS.internal(CALLEES["GetDBInfo"], st_getdb)
STUBS.internal(CALLEES["SetRideQueue"], st_setq)
STUBS.internal(CALLEES["SetRideTrack"], st_sett)
STUBS.internal(CALLEES["GetRideTrack"], st_gett)
STUBS.internal(CALLEES["SetModeNR"], st_setmode)
STUBS.internal(CALLEES["CoverUpgrade"], st_cover)
STUBS.internal(CALLEES["GetMode"], st_getmode)
STUBS.internal(CALLEES["DrawTrackLine"], st_draw)
STUBS.internal(CALLEES["ConstructClick"], st_construct)
STUBS.internal(CALLEES["GetSegment"], st_seg)
STUBS.internal(CALLEES["ConnectionType"], st_conn)
STUBS.internal(CALLEES["TraceTrack"], st_trace)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for _ in range(NOBJ):
        OBJ_PPC.append(T.persist_alloc(runner.emu, 0x40))
    INFO["ppc"] = T.persist_alloc(runner.emu, 0x800)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, kind, v):
        c = {"w": "I", "f": "I", "h": "H", "b": "B", "p": "I" if self.psz == 4 else "Q"}[kind]
        m = {"I": MASK, "H": 0xFFFF, "B": 0xFF, "Q": (1 << 64) - 1}[c]
        self.write(a, struct.pack(self.fmt + c, v & m))

    def get(self, a, kind):
        c = {"w": "I", "f": "I", "h": "H", "b": "B", "p": "I" if self.psz == 4 else "Q"}[kind]
        return struct.unpack(self.fmt + c, self.read(a, struct.calcsize(c)))[0]

    def cellidx(self, v):
        if v == 0:
            return None
        d = v - self.world - 0x1102D8
        return d // 0x28 if d % 0x28 == 0 else ("odd", d)

    def objlabel(self, v):
        if v == 0:
            return 0
        for i, a in enumerate(self.objs):
            if a & MASK == v:
                return i + 1
        return ("raw", v)


class Ppc(Side):
    fmt = ">"
    psz = 4
    stride = 20
    side = "ppc"

    def __init__(self, e, case):
        self.e = e
        self.objs = OBJ_PPC
        self.info = INFO["ppc"]
        self.world = WORLD["ppc"]
        self.L = e.cl = CL.PpcLog({a: i + 1 for i, a in enumerate(OBJ_PPC)}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return THINGS if name == "things" else GLOBALS[name][0]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def roff(self, off):
        return off

    def call_set(self, flag):
        T.call_ppc(self.e, FN_SET, [flag & MASK])

    def call_start(self, x, y, unused):
        T.call_ppc(self.e, FN_START, [x & MASK, y & MASK, unused & MASK])


class Host(Side):
    fmt = "<"
    psz = 8
    stride = 48
    side = "host"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.objs = lib.objs
        self.info = lib.info
        self.world = WORLD["host"]
        CL.host_begin(lib, {i + 1: a for i, a in enumerate(lib.objs)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_slc_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def roff(self, off):
        return self.lib.tpw_slc_rideoff(off)

    def call_set(self, flag):
        self.lib.tpw_slc_set_current(flag)

    def call_start(self, x, y, unused):
        self.lib.tpw_slc_start(s32(x), s32(y), unused & MASK)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_slc_addr.restype, lib.tpw_slc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_slc_alloc.restype, lib.tpw_slc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_slc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_slc_setup.argtypes = [C.POINTER(C.c_uint64), C.c_uint32, C.c_uint64]
    lib.tpw_slc_set_current.argtypes = [C.c_int32]
    lib.tpw_slc_start.argtypes = [C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_slc_rideoff.restype, lib.tpw_slc_rideoff.argtypes = C.c_int64, [C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_slc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.objs = [lib.tpw_slc_alloc(0x40) for _ in range(NOBJ)]
    lib.info = lib.tpw_slc_alloc(0x800)
    arr = (C.c_uint64 * NOBJ)(*lib.objs)
    lib.tpw_slc_setup(arr, NOBJ, lib.info)
    print("objs", [hex(a) for a in lib.objs], hex(lib.info))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.put(s.g("world"), "p", s.world)
    for name, v in case["g"].items():
        if name == "db6c":
            v = s.objs[v - 1] if v else 0
            if s.side == "host":      # the host keeps the whole pointer beside the 32-bit word
                s.put(s.g("db6cp"), "p", v)
        s.put(s.g(name), KIND[name], v)
    for i, o in enumerate(case["objs"]):
        a = s.objs[i]
        s.write(a, bytes(0x40))
        s.put(a + 4, "b", o["cx"])
        s.put(a + 6, "b", o["cy"])
        s.put(a + 0xE, "h", o["type"])
        s.put(a + 0x10, "w", o["rot"])
        s.put(a + 0x28, "w", o["f28"])
    s.write(s.info, bytes(0x800))
    for off, v in case["info"].items():
        s.put(s.info + s.roff(off), "w", v)
    th = s.g("things")
    s.put(th, "p", s.objs[case["thing0"] - 1] if case["thing0"] else 0)
    for idx, k in case["things"].items():
        s.put(th + idx * s.stride, "p", s.objs[k - 1] if k else 0)


def execute(s, case):
    if case["op"] == "set":
        s.call_set(case["flag"])
    else:
        s.call_start(*case["args"])


def observe(s, case):
    out = {"log": s.log()}
    for name, kind in KIND.items():
        if name == "world":
            continue
        v = s.get(s.g(name), kind)
        if name == "db48":
            v = s.cellidx(v)
        elif name == "db6c":
            v = s.objlabel(v)
        out[name] = v
    return out


def run(s, case):
    try:
        build(s, case)
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def pick(r, *opts):
    return r.choice(opts)


def rword(r):
    return pick(r, -1, 0, 1, 2, 5, r.randrange(0, 200), r.randrange(0, 0x8000), 0xFFFF, T.s32(r.getrandbits(32)))


def rcell(r):
    return pick(r, -1, 0, 1, 3, r.randrange(0, 128), r.randrange(0, 128), 0x1FF, 0xFFFF, 0x10000, r.randrange(0, 600))


def rseg(r):
    return pick(r, -3, -2, -1, 0, 1, 2, 3, r.randrange(-0x8000, 0x8000))


def base(r, op):
    g = dict(df18=rcell(r), df1c=rcell(r), df20=rword(r), df24=pick(r, 0xFFFF, 0xFFFF, r.getrandbits(16), r.randrange(0, 5), 0x8000),
             df26=pick(r, 0xFFFF, r.getrandbits(16), r.randrange(0, 5)), e028=pick(r, 0, r.getrandbits(16)), e02a=pick(r, 0xFFFF, r.getrandbits(16)),
             e02c=rword(r), e030=rword(r), e034=rword(r), e038=rword(r), hoardR=pick(r, -1, rcell(r), rcell(r)), hoardB=rcell(r),
             dc210=rword(r), dc214=rword(r), dc218=rword(r), db48=0, db50=rword(r), db54=rword(r), db58=r.getrandbits(16),
             db5a=pick(r, r.getrandbits(16), r.randrange(0, 600)), db5c=r.getrandbits(16), db68=r.getrandbits(16), db6c=0,
             e0cc=r.getrandbits(8), g4800=rword(r), g4804=rword(r), f9bc0=r.getrandbits(32), f9bc4=r.getrandbits(32))
    objs = [dict(cx=pick(r, r.getrandbits(8), r.randrange(0, 128)), cy=pick(r, r.getrandbits(8), r.randrange(0, 128)), type=r.getrandbits(16),
                 rot=pick(r, 0, 90, 180, 270, -1, rword(r)), f28=pick(r, 0, 1, 2, -1)) for _ in range(NOBJ)]
    # a start value of db68 equal to an object's cell id now and then (LLCM_SetCurrentObject compares the new id with the old one)
    if r.random() < 0.35:
        o = r.choice(objs)
        g["db68"] = (o["cx"] + (o["cy"] << 7) + 1) & 0xFFFF
    if op == "set":
        g["db6c"] = r.randrange(0, NOBJ + 1)
    else:
        g["db6c"] = r.randrange(1, NOBJ + 1)
        if r.random() < 0.12:
            g["e02a"] = g["db68"]
    p2 = pick(r, 0, 1, r.randrange(0, 0x2800), 0x27FF)
    q = {"IsMode": [pick(r, 0, 1, 2) for _ in range(4)], "GetSelectedObject": [pick(r, 0, 1, 2, 3, 4, 4)],
         "GetUpgradeParent": [g["e028"] if r.random() < 0.35 else r.getrandbits(16), p2], "GetDBInfo": [r.randrange(0, 2)],
         "GetRideTrack": [pick(r, -1, 0, 1, 2, 3, 4, 5, 6)], "GetMode": [pick(r, 0, 5, 0x10, 0x100, 0x1FF, rword(r))],
         "GetSegmentNumber": [rseg(r), rseg(r), rseg(r)], "ConnectionType": [pick(r, 9, 9, 0, 8, rword(r))],
         "OffsetStart": [pick(r, 0, 1, 0xFF, r.randrange(0, 0x200))], "OffsetEnd": [r.randrange(0, 0x100)]}
    info = {0x40: pick(r, 0, 1, rword(r)), 0x9C: pick(r, -1, 0, 1, 2, 3, 4, 5, rword(r)), 0x288: pick(r, 1, 1, 0, 2, rword(r))}
    c = {"op": op, "queues": q, "g": g, "objs": objs, "info": info, "thing0": pick(r, 0, 0, 0, r.randrange(0, NOBJ + 1)),
         "things": {p2: r.randrange(0, NOBJ + 1)}, "flag": 0, "args": (0, 0, 0)}
    return c


def set_case(r, force=None):
    c = base(r, "set")
    c["flag"] = pick(r, 0, 1, 1, 0x100, 0x101)
    apply_force(c, force)
    return c


def start_case(r, force=None):
    c = base(r, "start")
    obj = c["objs"][c["g"]["db6c"] - 1]
    x0, y0 = c["g"]["df18"], c["g"]["df1c"]
    dx = pick(r, r.randrange(-9, 10), r.randrange(-9, 10), r.randrange(-9, 10), rword(r))
    dy = pick(r, r.randrange(-9, 10), r.randrange(-9, 10), r.randrange(-9, 10), rword(r))
    c["args"] = (s32(x0 + obj["rot"] + 1 + dx), s32(y0 - 3 + dy), pick(r, 0, 1, r.getrandbits(16)))
    apply_force(c, force)
    return c


def apply_force(c, force):
    apply_force_1(c, force)
    # the original dereferences the upgrade thing it selects: a ride record that selects it needs a thing in that row
    p2 = c["queues"]["GetUpgradeParent"][1]
    if c["info"][0x288] == 1 and c["things"].get(p2, 0) == 0:
        c["things"][p2] = 1


def apply_force_1(c, force):
    if not force:
        return
    c["g"].update(force.get("g", {}))
    for k, v in force.get("q", {}).items():
        c["queues"][k] = v
    for k, v in force.get("info", {}).items():
        c["info"][k] = v
    if "skip" in force:
        c["g"]["e02a"] = c["g"]["db68"]
    if "args" in force:
        c["args"] = force["args"]
    if "f28" in force:
        for o in c["objs"]:
            o["f28"] = force["f28"]
    if "p1same" in force:
        c["queues"]["GetUpgradeParent"][0] = c["g"]["e028"] if force["p1same"] else (c["g"]["e028"] ^ 1)
    if "thing0" in force:
        c["thing0"] = force["thing0"]
    if "flag" in force:
        c["flag"] = force["flag"]


def set_edges():
    r = random.Random(5182)
    forces = []
    for ism in ([1], [0, 1], [0, 0], [2, 0]):
        for right in (-1, 5, 0xFFFF, 0):
            forces.append(dict(q={"IsMode": ism}, g={"hoardR": right, "hoardB": r.choice([0, 3, 0x1FF, -1])}))
    for sel in (0, 1, 2):
        for same in (True, False):
            for ride in (1, 0, 2):
                for thing0 in (0, 1):
                    forces.append(dict(q={"IsMode": [0], "GetSelectedObject": [sel]}, p1same=same, info={0x288: ride}, thing0=thing0,
                                       flag=r.choice([0, 1])))
    for flag in (0, 1):
        for db5c_same in (True, False):
            forces.append(dict(q={"IsMode": [0], "GetSelectedObject": [1]}, flag=flag, p1same=True, g={"db68": 0x1234 if db5c_same else 0}))
    for f in forces:
        yield set_case(r, f)
    for _ in range(40):
        yield set_case(r, dict(q={"IsMode": [0]}, p1same=r.random() < 0.5))


def start_edges():
    r = random.Random(10088)
    for track in (-1, 0, 1, 2, 3, 4, 5):
        for ism in ([0], [1]):
            for dbi in (0, 1):
                yield start_case(r, dict(q={"GetRideTrack": [track], "IsMode": ism, "GetDBInfo": [dbi]}, info={0x40: pick(r, 0, 7)}))
    yield start_case(r, dict(skip=True))
    yield start_case(r, dict(skip=True, q={"GetRideTrack": [2], "IsMode": [0]}))
    for f28 in (0, 1):
        for df24 in (0xFFFF, 0, 3):
            yield start_case(r, dict(f28=f28, g={"df24": df24}))
    for segs in ([1, 2, 5], [2, 1, 5], [3, 3, 1], [-4, -5, 0], [0x7FFF, -0x8000, 0x7FFF], [-0x8000, 0x7FFF, -0x8000]):
        for conn in (9, 8):
            for df24, df26 in ((3, 1), (3, 9), (0x7FFF, 0x7FFF), (0, 0xFFFF), (0x8001, 0x7FFF)):
                yield start_case(r, dict(f28=1, g={"df24": df24, "df26": df26}, q={"GetSegmentNumber": segs, "ConnectionType": [conn]}))
    for dx in (-9, -8, -7, -2, -1, 0, 1, 2):
        for dy in (-9, -8, -3, -1, 0, 1):
            c = start_case(r, dict(f28=r.choice([0, 1]), g={"df24": r.choice([0xFFFF, 4])}))
            obj = c["objs"][c["g"]["db6c"] - 1]
            c["args"] = (s32(c["g"]["df18"] + obj["rot"] + 1 + dx), s32(c["g"]["df1c"] - 3 + dy), 0)
            yield c
    for g4 in (0, -1, 0x7FFFFFFF, -0x80000000, 16777217, 12345):
        yield start_case(r, dict(f28=0, g={"g4800": g4, "g4804": -g4 if g4 > -0x80000000 else 1}))


def set_random(r):
    return set_case(r)


def start_random(r):
    return start_case(r)


TESTS = [
    FnTest("LLCM_SetCurrentObject", ppc, host, edge=set_edges, rnd=set_random),
    FnTest("LLCM_StartTrackEditMode", ppc, host, edge=start_edges, rnd=start_random),
    imagedata.table_test("the cell id word", [("e02a", 0x1029E02A, "H", 1)]),
]
