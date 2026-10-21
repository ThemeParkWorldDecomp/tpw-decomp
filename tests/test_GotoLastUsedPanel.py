"""Unit v5188, the toolbar's jump to the last used panel and the cash window's update, against libtpw_unit_GotoLastUsedPanel.dylib
(tests/unit_GotoLastUsedPanel.cpp), main app ("SimTheme Park"), src/ui_mainpanel2_msg_GotoLastUsedPanel.cpp:
  GotoLastUsedPanel 0x101a587c, DoCashWin 0x101a5ca8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (MainMenuUp, the Create
panels, the bank, the window members, the mouse, the cell system, operator new, abs and the virtual calls through __ptr_glue) are fakes on
both sides that record their arguments and answer from the case. The CoolRect constructor and destructor run as the real code on both sides.

The world of a case: the last-panel globals and MainMenuUp's answer; the cash globals, the colour table (random, or the initial data of the
image and of the host's initialiser), the balance, the mouse and mouse mode, the session cost, which children exist. What is compared:
the event log and every global written.

No fused operations. Not tested: abs is a pure function and is computed, not logged; a NULL child 0x31 or 0x32 (read unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_GotoLastUsedPanel"
MASK = 0xFFFFFFFF

FN_GOTO, FN_CASH = 0x101A587C, 0x101A5CA8
SIZES = {FN_GOTO: 380, FN_CASH: 996}
OWN = set(SIZES)

ADDR = dict(cashword=0x103949AC, colouridx=0x103949A0, initdone=0x103949A4, sessioncost=0x103949A8, fontcur=0x102AAD28,
            colours=0x102AAD2C, mouse1e0=0x102DC1E0, mouse1e4=0x102DC1E4, lastfin=0x103949B8, lastpark=0x103949BC,
            lastlists=0x103949C0, mode=0x102DC1F0, mouse=0x102DC1F4, worldref=0x102A460C)
HOST_LAYOUT = []

SLOT_SETVALUE, SLOT_GETMODE = 0x2424, 0x2C2C        # what the fake vtables hold at +0x24 and +0x2c; the glue stub tells the calls by r12


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)
OBJ = {}        # label -> Mac address of the current case


def save_toc(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))      # what the glue stores; the caller reloads r2 from there


def rec_stub(name, fn=None):
    def stub(e):
        L = e.cl
        L.rec(name, fn(e, L) if fn else ())
    return stub


def ret_stub(name, fn=None, key=None):
    def stub(e):
        L = e.cl
        L.rec(name, fn(e, L) if fn else ())
        e.set_r(3, L.ret(key or name) & MASK)
    return stub


STUBS.internal(0x101FC1EC, ret_stub("MainMenuUp"))
STUBS.internal(0x101B8434, rec_stub("CreateRideListPanel", lambda e, L: (s32(e.r(3)), (e.r(4) >> 16) & 0xFFFF)))
STUBS.internal(0x101A1C88, rec_stub("CreateHireListPanel", lambda e, L: (s32(e.r(3)),)))
STUBS.internal(0x101ACA78, rec_stub("CreateParkStatusPanel"))
STUBS.internal(0x10199630, rec_stub("CreateAllStaffPanel", lambda e, L: (s32(e.r(3)), L.lab(e.r(4)))))
STUBS.internal(0x101983C4, rec_stub("CreateAllItemsPanel", lambda e, L: (s32(e.r(3)), L.lab(e.r(4)))))
STUBS.internal(0x101952BC, rec_stub("CreateAllKidsPanel", lambda e, L: (L.lab(e.r(3)),)))
STUBS.internal(0x101A001C, rec_stub("CreateFinancePanel"))
STUBS.internal(0x101A4DE4, rec_stub("CreateLoansPanel"))
STUBS.internal(0x101BE440, rec_stub("CreateStaffCostsPanel"))
STUBS.internal(0x1019D660, rec_stub("CreateEntryPricePanel"))


def stub_bank(e):
    e.cl.rec("GetBankAccount", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0x5000)


STUBS.internal(0x1013EDFC, stub_bank)
STUBS.internal(0x100F32B4, ret_stub("GetBalance", lambda e, L: (L.lab(e.r(3)),), "balance"))
STUBS.internal(0x101DA3AC, rec_stub("SetFontColour", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7)))))


def stub_abs(e):
    v = s32(e.r(3))
    e.set_r(3, (-v if v < 0 else v) & MASK)
    save_toc(e)


STUBS.internal(0x10238BC0, stub_abs)


def stub_glue(e):
    L = e.cl
    slot = e.r(12)
    if slot == SLOT_SETVALUE:
        L.rec("SetValue", (L.lab(e.r(3)), s32(e.r(4))))
    else:
        assert slot == SLOT_GETMODE, hex(slot)
        L.rec("GetMode", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("GetMode") & MASK)
    save_toc(e)


STUBS.internal(0x1023BAA8, stub_glue)


def put_rect(e, rect, key, L):
    a, b = L.ret(key) & MASK, L.ret(key) & MASK
    e.write(rect + 4, struct.pack(">HHHH", a >> 16, a & 0xFFFF, b >> 16, b & 0xFFFF))


def stub_size(e):
    L = e.cl
    L.rec("GetSizeOfClientDrawObject", (L.lab(e.r(3)),))
    put_rect(e, e.r(4), "GetSize", L)


def stub_getwindow(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    present = L.ret({0x30: "GW30", 0x31: "GW31", 0x32: "GW32"}[wid])
    e.set_r(3, OBJ[wid - 0x30 + 2] if present else 0)


def stub_setposition(e):
    L = e.cl
    x, y = e.r(4), e.r(5)
    xv = struct.unpack(">h", e.read(x, 2))[0] if x else 0x10000
    yv = struct.unpack(">h", e.read(y, 2))[0] if y else 0x10000
    L.rec("SetPosition", (L.lab(e.r(3)), xv, yv))
    e.set_r(3, 1)


def stub_getposition(e):
    L = e.cl
    L.rec("GetPosition", (L.lab(e.r(3)), T.s16(e.r(5))))
    put_rect(e, e.r(4), "GetPosition", L)
    e.set_r(3, 1)


def stub_new(e):
    size = e.r(3)
    e.cl.rec("new", (size,))
    e.set_r(3, e.alloc(size if size else 4))
    save_toc(e)


STUBS.internal(0x101DBBA8, stub_size)
STUBS.internal(0x101DBE40, stub_getwindow)
STUBS.internal(0x101DB1B0, stub_setposition)
STUBS.internal(0x101DB884, stub_getposition)
STUBS.internal(0x101DB7DC, rec_stub("SetFrame", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)))))
STUBS.internal(0x101DC1EC, rec_stub("Show", lambda e, L: (L.lab(e.r(3)), T.s16(e.r(4)))))
STUBS.internal(0x102384B8, stub_new)
STUBS.internal(0x1007EA78, CL.recorder("CMMDefault", "N"))
STUBS.internal(0x1007E2E0, rec_stub("AssignMouseMode", lambda e, L: (L.lab(e.r(3)), L.lab(e.r(4)))))
STUBS.internal(0x10058B30, ret_stub("IsCoasterLoftMode"))
STUBS.internal(0x10091D98, ret_stub("LLCC_SystemGetSessionCost", None, "cost"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# labels: 1 the cash window, 2..4 the children 0x30..0x32, 5 the mouse mode, 6 the mouse, 7 the world
class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def plab(self, v):
        """A pointer as a label: 0 is 0, a known object its label, anything else (made by the code) -1."""
        return 0 if v == 0 else self.lbl.get(v, -1)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        vt = e.alloc(0x40)
        self.write(vt + 0x24, struct.pack(">I", SLOT_SETVALUE))
        objs = {}
        for lab in (1, 2, 3, 4):
            w = e.alloc(0x120)
            self.write(w + 0x118, struct.pack(">I", vt))
            objs[lab] = w
        vtm = e.alloc(0x40)
        self.write(vtm + 0x2C, struct.pack(">I", SLOT_GETMODE))
        objs[5] = e.alloc(0x10)
        self.write(objs[5], struct.pack(">I", vtm))
        objs[6] = e.alloc(0x10)
        objs[7] = e.alloc(0x10)
        OBJ.clear()
        OBJ.update(objs)
        self.objs = objs
        self.lbl = {a: l for l, a in objs.items()}
        self.L = e.cl = CL.PpcLog({a: l for l, a in objs.items()}, case["queues"])
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

    def setptr(self, name, label):
        addr = self.e.u32(ADDR["worldref"]) if name == "world" else ADDR[name]
        self.words(addr, [self.objs[label] if label else 0])
        if name == "world":
            self.worldslot = addr

    def getptr(self, name):
        return self.plab(self.rd_words(self.worldslot if name == "world" else ADDR[name], 1)[0])

    def call_goto(self, t):
        T.call_ppc(self.e, FN_GOTO, [t & MASK])

    def call_cash(self):
        T.call_ppc(self.e, FN_CASH, [self.objs[1]])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        objs = {}
        for lab, size in ((1, 0x20), (2, 0x20), (3, 0x20), (4, 0x20), (6, 0x10), (7, 0x10)):
            objs[lab] = lib.tpw_gcw_alloc(size)
        objs[5] = lib.tpw_gcw_addr(b"fakemode")
        self.objs = objs
        self.lbl = {a: l for l, a in objs.items()}
        lib.tpw_gcw_setwindows(objs[2], objs[3], objs[4])
        CL.host_begin(lib, {l: a for l, a in objs.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_gcw_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setptr(self, name, label):
        self.lib.tpw_gcw_setptr({"mouse": 0, "mode": 1, "world": 2}[name], self.objs[label] if label else 0)

    def getptr(self, name):
        return self.plab(self.lib.tpw_gcw_getptr({"mouse": 0, "mode": 1, "world": 2}[name]))

    def call_goto(self, t):
        self.lib.tpw_gcw_goto(s32(t))

    def call_cash(self):
        self.lib.tpw_gcw_cash(self.objs[1])


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_gcw_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "setptr").argtypes = [C.c_int32, C.c_uint64]
    getattr(lib, p + "getptr").restype, getattr(lib, p + "getptr").argtypes = C.c_uint64, [C.c_int32]
    getattr(lib, p + "setwindows").argtypes = [C.c_uint64] * 3
    getattr(lib, p + "goto").argtypes = [C.c_int32]
    getattr(lib, p + "cash").argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


INITIAL = {}    # (side, emulator or library) -> the colour table's initial data, read before the first case writes it
GLOBALS = ("cashword", "colouridx", "initdone", "sessioncost", "fontcur", "mouse1e0", "mouse1e4", "lastfin", "lastpark", "lastlists")


def build(s, case):
    g = case["g"]
    for name in GLOBALS:
        if name == "initdone":
            s.write(s.g(name), bytes([g[name]]))
        else:
            s.words(s.g(name), [g[name]])
    key = (type(s).__name__, id(s.e if isinstance(s, Ppc) else s.lib))
    if key not in INITIAL:
        INITIAL[key] = s.read(s.g("colours"), 24)          # the table as the image (or the host's initialiser) has it, before any case
    s.write(s.g("colours"), INITIAL[key] if g["colours"] is None else bytes(g["colours"]))
    s.setptr("world", 7)
    s.setptr("mouse", 6 if g["hasmouse"] else 0)
    s.setptr("mode", 5 if g["hasmode"] else 0)


def observe(s):
    out = {"log": s.log(), "colours": s.read(s.g("colours"), 24), "mouse": s.getptr("mouse"), "mode": s.getptr("mode"),
           "world": s.getptr("world")}
    for name in GLOBALS:
        out[name] = s.read(s.g(name), 1)[0] if name == "initdone" else s.rd_words(s.g(name), 1)[0]
    return out


def run(s, case):
    try:
        build(s, case)
        if case["op"] == "goto":
            s.call_goto(case["type"])
        else:
            s.call_cash()
        return observe(s)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- generators

BALANCES = [0, 1, -1, 2, 999, 1000, 1001, 1998, 1999, 2000, 2998, 2999, 3000, 4999, 5000, 5998, 5999, 6000, 6001, 100000, -999, -1000,
            -1001, -100000, 0x7FFFFFFF, -0x80000000, 0x7FFFFC17, 0x7FFFFC18]
COSTS = [-999, -9999, 0, 5, -5, 100, -998]


def world(r, op):
    g = dict(cashword=r.choice([0, 7, -3]), colouridx=r.randrange(-1, 8), initdone=r.choice([0, 1, 0x80, 0xFF, 5]),
             sessioncost=r.choice(COSTS), fontcur=r.choice([999, -1, 0, 1, 2, 3, 4, 5]), mouse1e0=r.choice([0, 5]), mouse1e4=r.choice([0, 6]),
             lastfin=r.randrange(0, 13), lastpark=r.randrange(0, 9), lastlists=r.randrange(0, 4),
             colours=[r.randrange(256) for _ in range(24)], hasmouse=r.choice([0, 1]), hasmode=r.choice([0, 1]))
    q = {"MainMenuUp": [r.choice([0, 1])], "balance": [r.choice(BALANCES)], "GetMode": [r.choice([0, 1, 2, 3, 4, 5, 8, 9, -1, 0x7FFFFFFF])],
         "IsCoasterLoftMode": [r.choice([0, 1])], "cost": [r.choice(COSTS)],
         "GetSize": [r.getrandbits(32), r.getrandbits(32)], "GetPosition": [r.getrandbits(32) for _ in range(4)],
         "GW30": [1], "GW31": [1], "GW32": [1]}
    return {"op": op, "g": g, "queues": q}


def fix(c):
    """A missing child 0x30 only where the balance path (which uses it without a test) is not taken."""
    g, q = c["g"], c["queues"]
    if q["balance"][0] != g["cashword"]:
        q["GW30"] = [1]
    return c


def goto_case(r, t=None, lists=None, park=None, fin=None, menu=None):
    c = world(r, "goto")
    c["type"] = r.choice([-1, 0, 1, 2, 3, 4, 5]) if t is None else t
    if lists is not None:
        c["g"]["lastlists"] = lists
    if park is not None:
        c["g"]["lastpark"] = park
    if fin is not None:
        c["g"]["lastfin"] = fin
    if menu is not None:
        c["queues"]["MainMenuUp"] = [menu]
    return c


def goto_edges():
    r = random.Random(5188)
    for t in range(-1, 6):
        for v in range(-1, 13):
            yield goto_case(r, t, v % 4, v, v, 0)
        yield goto_case(r, t, 1, 3, 7, 1)


def goto_random(r):
    return goto_case(r)


def cash_case(r, balance=None, **kw):
    c = world(r, "cash")
    if balance is not None:
        c["queues"]["balance"] = [balance]
    for k, v in kw.items():
        if k in c["g"]:
            c["g"][k] = v
        else:
            c["queues"][k] = v
    return fix(c)


def cash_edges():
    r = random.Random(1885)
    for b in BALANCES:
        for cw in (b, b ^ 1):
            yield cash_case(r, b, cashword=cw)
    for b in (-5, 0, 1500, 7000):
        for fc in (-1, 999, 0, 1, 2, 3, 4, 5):
            yield cash_case(r, b, cashword=1, fontcur=fc)
    for b in (-1000, 0, 1, 1000, 1001, 2001, 3001, 4000, 4001, 100000):       # the initial table: each balance band's colour
        yield cash_case(r, b, cashword=b ^ 1, fontcur=999, colours=None)
    for mode in (-1, 0, 1, 2, 3, 4, 5, 7, 8, 9):
        for loft in (0, 1):
            for cost in COSTS:
                for sc in COSTS:
                    for w30 in (0, 1):
                        yield cash_case(r, 5, cashword=5, hasmode=1, sessioncost=sc, GetMode=[mode], IsCoasterLoftMode=[loft],
                                        cost=[cost], GW30=[w30])
    for hm in (0, 1):
        for hmode in (0, 1):
            for init in (0, 1, 0xFF, 0x80):
                yield cash_case(r, 5, cashword=5, hasmouse=hm, hasmode=hmode, initdone=init)


def cash_random(r):
    c = cash_case(r)
    if r.random() < 0.3:
        c["g"]["cashword"] = c["queues"]["balance"][0]
        c = fix(c)
    return c


TESTS = [
    FnTest("GotoLastUsedPanel", ppc, host, edge=goto_edges, rnd=goto_random),
    FnTest("DoCashWin", ppc, host, edge=cash_edges, rnd=cash_random),
]
