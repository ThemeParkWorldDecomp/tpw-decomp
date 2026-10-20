"""Unit v5311, the feature status panel, against libtpw_unit_FeatureStatusPanel_UpdateStatusWindow.dylib
(tests/unit_FeatureStatusPanel_UpdateStatusWindow.cpp), main app ("SimTheme Park"), src/FeatureStatusPanel_UpdateStatusWindow.cpp:
  UpdateStatusWindow 0x1019e728, InitWindow 0x1019e85c, UpdateWindow 0x1019e958, RideIt 0x1019ea60, UpdateViewWindow 0x1019e374,
  Show 0x1019edc4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit are fakes on both sides that record their arguments and answer from the case's queues: CObject::GetDBInfo,
GetCurrentResidualValue and GoFirstPerson, CWorld::GetParkAnalyser, CParkAnalyser::NumberOfObjectInPark, InterfaceWindow::GetWindow, Show
and GetClientRect, the window's virtual SetValue (slot 0x24) and SetText (slot 0x2c; on the Mac through the table at window +0x118 and
__ptr_glue, on the host real objects), CoolRect's constructor and destructor, UIMain_ShowRidePreview, UIMain_AdvisorMessage and
UIStatusPanel::FillObjectList, InitEditBox and OpenWindow. The panel's own virtual members (table at +0x20, slots 0x1c, 0x20, 0x34,
0x3c) are fakes too (on the Mac TVectors, on the host blocks holding a function pointer).

The world of a case: the panel block (windows at +0x0c, +0x10, +0x14, +0x1c, table at +0x20), the windows (id at the Mac's +0x88, z at
+0xdc), the object gStaffRoomPanelThing (halfwords at +0x0e and +0x2e), the ride record's type number (+0x78c), gGameWorld (a fake
pointer) and gFeatureStatusThing. What is compared: the log of events (arguments included) and gFeatureStatusThing.

No fused operations. Not tested: a NULL gStaffRoomPanelThing for the functions that read the object unconditionally; a NULL answer of
GetWindow for the ids the original never tests (0x1584 and the others it dereferences or passes on).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FeatureStatusPanel_UpdateStatusWindow"
MASK = 0xFFFFFFFF

# ---- the unit's own functions
F_STATUS, F_INIT, F_UPDATE, F_RIDE, F_VIEW, F_SHOW = 0x1019E728, 0x1019E85C, 0x1019E958, 0x1019EA60, 0x1019E374, 0x1019EDC4
SIZES = {F_STATUS: 244, F_INIT: 136, F_UPDATE: 208, F_RIDE: 80, F_VIEW: 164, F_SHOW: 76}
OWN = set(SIZES)

# ---- the callees faked on both sides
GET_DBINFO, RESIDUAL, GO_FIRST, NUM_OBJ = 0x10107CD4, 0x1010FFC8, 0x1010ED8C, 0x100EA52C
GET_ANALYSER = 0x1013EBC8
CR_CTOR, CR_DTOR, CLIENT_RECT, GET_WINDOW, SHOW_WIN = 0x101C7448, 0x101C74E0, 0x101DBA4C, 0x101DBE40, 0x101DC1EC
ADVISOR, RIDE_PREVIEW, FILL_LIST, INIT_EDIT, OPEN_WINDOW = 0x101821CC, 0x10181400, 0x10192A90, 0x10193DCC, 0x101935A0
SET_VALUE, SET_TEXT = 0x101DFE84, 0x101DE248                       # the window's virtual members (slots 0x24 and 0x2c)
SLOT_CODE = {0x1C: 0x10192C28, 0x20: 0x10192DE0, 0x34: 0x10193138, 0x3C: 0x101938D8}   # the panel's virtual members (any free code)

# ---- the Mac's addresses of the globals
ADDR = dict(thing=0x103947A4, world=0x1037EBFC, target=0x1039486C, script=0x102A9948, cb=0x10267B40)
FAKE_WORLD, FAKE_ANALYSER = 0x5000, 0x6000
# ---- the host's offsets the module relies on
HOST_LAYOUT = [0x24, 0x20]          # after the host's offset of RideInfo::mRideTypeNumber (the first number layout answers)

KID_IDS = [0x157C, 0x157D, 0x1581, 0x1584, 0x1586]


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def winid(e, a):
    return -1 if a == 0 else e.u32(a + 0x88)


def mk(name, fn):
    def stub(e):
        fn(e, e.cl)
    return stub


def st_dbinfo(e, L):
    L.rec("GetDBInfo", (0,))
    e.set_r(3, e.ctx["info"])


def st_residual(e, L):
    L.rec("Residual", (0,))
    e.set_r(3, L.ret("Residual") & MASK)


def st_gofirst(e, L):
    L.rec("GoFirstPerson", (s32(e.r(4)),))


def st_numobj(e, L):
    L.rec("NumberOfObjectInPark", (e.r(3) & MASK, (e.r(4) >> 16) & 0xFFFF))
    e.set_r(3, L.ret("NumberOfObjectInPark") & MASK)


def st_analyser(e, L):
    L.rec("GetParkAnalyser", (e.r(3) & MASK,))
    e.set_r(3, FAKE_ANALYSER)


def st_crctor(e, L):
    L.rec("CoolRect", (0,))


def st_crdtor(e, L):
    L.rec("~CoolRect", (0,))


def st_clientrect(e, L):
    L.rec("GetClientRect", (winid(e, e.r(3)),))
    vals = [s16(L.ret(k)) for k in ("rl", "rt", "rr", "rb")]
    e.write(e.r(4) + 4, struct.pack(">4h", *vals))
    e.set_r(3, 1)


def st_getwindow(e, L):
    wid = s32(e.r(4))
    L.rec("GetWindow", (winid(e, e.r(3)), wid))
    e.set_r(3, e.ctx["kids"].get(wid, 0))


def st_show(e, L):
    L.rec("Show", (winid(e, e.r(3)), s16(e.r(4))))
    e.set_r(3, 1)


def st_advisor(e, L):
    L.rec("AdvisorMessage", (s32(e.r(3)),))


def st_preview(e, L):
    left, top, right, bottom = struct.unpack(">4h", e.read(e.r(3) + 4, 8))
    L.rec("ShowRidePreview", (left, top, right, bottom, e.r(4) & MASK, s32(e.r(5))))


def st_fill(e, L):
    L.rec("FillObjectList", (e.r(4) & MASK,))
    e.set_r(3, L.ret("FillObjectList") & MASK)


def st_edit(e, L):
    L.rec("InitEditBox", (s32(e.r(4)),))


def st_open(e, L):
    L.rec("OpenWindow", (L.lab(e.r(4)), L.lab(e.r(5)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8))))


def st_setvalue(e, L):
    L.rec("SetValue", (winid(e, e.r(3)), s32(e.r(4))))
    e.set_r(3, 0)


def st_settext(e, L):
    L.rec("SetText", (winid(e, e.r(3)), e.r(4) & MASK))
    e.set_r(3, 0)


def st_slot(slot):
    name = "Slot%02x" % slot

    def f(e, L):
        L.rec(name, (0,))
        e.set_r(3, L.ret(name) & MASK)
    return f


for addr, fn in [(GET_DBINFO, st_dbinfo), (RESIDUAL, st_residual), (GO_FIRST, st_gofirst), (NUM_OBJ, st_numobj),
                 (GET_ANALYSER, st_analyser), (CR_CTOR, st_crctor), (CR_DTOR, st_crdtor), (CLIENT_RECT, st_clientrect),
                 (GET_WINDOW, st_getwindow), (SHOW_WIN, st_show), (ADVISOR, st_advisor), (RIDE_PREVIEW, st_preview),
                 (FILL_LIST, st_fill), (INIT_EDIT, st_edit), (OPEN_WINDOW, st_open), (SET_VALUE, st_setvalue),
                 (SET_TEXT, st_settext)] + [(SLOT_CODE[k], st_slot(k)) for k in SLOT_CODE]:
    STUBS.internal(addr, mk(addr, fn))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def gptr(self, name, v):
        """A global pointer: a word on the Mac, a full native pointer on the host (the arena is not 32-bit addressable)."""
        if self.fmt == ">I":
            self.words(self.g(name), [v])
        else:
            self.write(self.g(name), struct.pack("<Q", v))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["script"]: 3, ADDR["cb"]: 4}, case["queues"])
        e.ctx = {"kids": {}, "info": 0}
        self.saved = STUBS.activate(e)
        # the window's table of virtual members (+0x118): SetValue at +0x24, SetText at +0x2c, each a TVector
        self.wvt = self.alloc(0x40)
        for off, code in ((0x24, SET_VALUE), (0x2C, SET_TEXT)):
            tv = self.alloc(8)
            self.words(tv, [code, 0])
            self.words(self.wvt + off, [tv])

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

    def ride_off(self):
        return 0x78C

    def ptr(self, a):
        return a

    def mkwin(self, wid, z):
        a = self.alloc(0x140)
        self.words(a + 0x88, [wid])
        self.write(a + 0xDC, struct.pack(">h", z))
        self.words(a + 0x118, [self.wvt])
        return a

    def slot(self, slot):
        tv = self.alloc(8)
        self.words(tv, [SLOT_CODE[slot], 0])
        return tv

    def setkid(self, wid, a):
        self.e.ctx["kids"][wid] = a

    def setinfo(self, a):
        self.e.ctx["info"] = a

    def call(self, entry, args):
        T.call_ppc(self.e, entry, [a & MASK for a in args])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_fsp_reset()
        CL.host_begin(lib, {3: self.g("script"), 4: self.g("cb")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fsp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fsp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ride_off(self):
        return self.lib.ride_off

    def ptr(self, a):
        return self.lib.tpw_fsp_off(a)

    def mkwin(self, wid, z):
        return self.lib.tpw_fsp_mkwin(wid, z)

    def slot(self, slot):
        return self.lib.tpw_fsp_slot(slot)

    def setkid(self, wid, a):
        self.lib.tpw_fsp_setkid(wid, a)

    def setinfo(self, a):
        self.lib.tpw_fsp_setinfo(a)

    def call(self, entry, args):
        name = {F_STATUS: "status", F_INIT: "init", F_UPDATE: "update", F_RIDE: "ride", F_VIEW: "view", F_SHOW: "show"}[entry]
        getattr(self.lib, "tpw_fsp_" + name)(*[a & 0xFFFFFFFFFFFFFFFF for a in args])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_fsp_"
    lib.tpw_fsp_addr.restype, lib.tpw_fsp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fsp_alloc.restype, lib.tpw_fsp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fsp_off.restype, lib.tpw_fsp_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_fsp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fsp_reset.argtypes = []
    lib.tpw_fsp_setkid.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_fsp_setinfo.argtypes = [C.c_uint64]
    lib.tpw_fsp_mkwin.restype, lib.tpw_fsp_mkwin.argtypes = C.c_uint64, [C.c_int32, C.c_int32]
    lib.tpw_fsp_slot.restype, lib.tpw_fsp_slot.argtypes = C.c_uint64, [C.c_int32]
    for n in ("status", "init", "update", "ride", "view"):
        getattr(lib, p + n).argtypes = [C.c_uint64]
    lib.tpw_fsp_show.argtypes = [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fsp_layout(out)
    assert list(out)[1:n] == HOST_LAYOUT, list(out)[:n]
    lib.ride_off = out[0]            # the host's RideInfo is laid out otherwise (a vtable in the balance table)
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.gptr("world", FAKE_WORLD)
    s.gptr("target", case["target"])
    # the object
    obj = s.alloc(0x40)
    s.half(obj + 0xE, case["objid"])
    s.half(obj + 0x2E, case["flags"])
    s.gptr("thing", obj if case["has_obj"] else 0)
    info = s.alloc(0x2000)
    s.words(info + s.ride_off(), [case["ridetype"]])
    s.setinfo(info)
    # the windows
    main = s.mkwin(1, 0)
    status = s.mkwin(2, 0)
    view = s.mkwin(3, case["z"])
    text = s.mkwin(4, 0)
    for wid in KID_IDS:
        if wid == 0x1586 and not case["kid1586"]:
            continue
        s.setkid(wid, s.mkwin(wid, 0))
    # the panel and its table
    table = s.alloc(0x40)
    for slot in (0x1C, 0x20, 0x34, 0x3C):
        s.words(table + slot, [s.ptr(s.slot(slot))])
    panel = s.alloc(0x30)
    s.words(panel + 0x0C, [s.ptr(main) if case["has_main"] else 0])
    s.words(panel + 0x10, [s.ptr(status) if case["has_status"] else 0])
    s.words(panel + 0x14, [s.ptr(view) if case["has_view"] else 0])
    s.words(panel + 0x1C, [s.ptr(text)])
    s.words(panel + 0x20, [s.ptr(table)])
    return panel


def run(s, case):
    try:
        panel = build(s, case)
        op = case["op"]
        entry = {"status": F_STATUS, "init": F_INIT, "update": F_UPDATE, "ride": F_RIDE, "view": F_VIEW, "show": F_SHOW}[op]
        args = [panel] + ([case["thing"]] if op == "show" else [])
        s.call(entry, args)
        return {"log": s.log(), "target": s.rd_words(s.g("target"), 1)}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def base(r, op):
    def w():
        return r.choice([0, 1, 2, 0x7FFFFFFF, 0x80000000, MASK, r.getrandbits(32)])
    return {
        "op": op,
        "queues": {
            "Residual": [w()], "NumberOfObjectInPark": [w()], "FillObjectList": [r.choice([-1, 0, 1, 2, 3, 40, w()])],
            "Slot1c": [w()], "Slot20": [w()], "Slot34": [w()], "Slot3c": [w()],
            "rl": [s16(r.getrandbits(16))], "rt": [s16(r.getrandbits(16))], "rr": [s16(r.getrandbits(16))], "rb": [s16(r.getrandbits(16))],
        },
        "target": w(), "thing": r.choice([0, 1, 0x1234ABCD, r.getrandbits(32)]),
        "ridetype": w(), "objid": r.choice([0, 1, 0xFFFF, r.getrandbits(16)]),
        "flags": r.choice([0, 0x10, 0xFFEF, 0xFFFF, 0x8010, r.getrandbits(16)]),
        "z": r.choice([0, 1, -1, 0x7FFF, -0x8000, 0x100, r.randrange(-300, 300)]),
        "has_obj": 1, "has_main": 1, "has_status": r.choice([0, 1, 1]), "has_view": r.choice([0, 1, 1]),
        "kid1586": r.choice([0, 1, 1]),
    }


def gen(op, r, **kw):
    c = base(r, op)
    c.update(kw)
    return c


def edges_for(op, seed):
    def edges():
        r = random.Random(seed)
        for has_status in (0, 1):
            for has_view in (0, 1):
                for kid in (0, 1):
                    for flags in (0, 0x10, 0xFFEF, 0xFFFF):
                        yield gen(op, r, has_status=has_status, has_view=has_view, kid1586=kid, flags=flags)
        if op == "view":
            for has_obj in (0, 1):
                yield gen(op, r, has_obj=has_obj, has_view=1)
        if op == "init":
            for fill in (-1, 0, 1, 2, 3):
                c = gen(op, r)
                c["queues"]["FillObjectList"] = [fill]
                yield c
        if op == "view":
            for z in (0, -10, -11, 0x7FFF, -0x8000):
                yield gen(op, r, z=z, has_view=1, has_obj=1)
        if op == "show":
            for thing in (0, 1, 0xFFFFFFFF):
                yield gen(op, r, thing=thing)
    return edges


def rnd_for(op):
    def f(r):
        return gen(op, r)
    return f


TESTS = [FnTest(name, ppc, host, edge=edges_for(op, 100 + i), rnd=rnd_for(op))
         for i, (name, op) in enumerate([("FeatureStatusPanel::UpdateStatusWindow", "status"),
                                         ("FeatureStatusPanel::InitWindow", "init"),
                                         ("FeatureStatusPanel::UpdateWindow", "update"),
                                         ("FeatureStatusPanel::RideIt", "ride"),
                                         ("FeatureStatusPanel::UpdateViewWindow", "view"),
                                         ("FeatureStatusPanel::Show", "show")])]
