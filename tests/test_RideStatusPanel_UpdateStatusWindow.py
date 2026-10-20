"""Unit v5191, the ride status panel's UpdateStatusWindow, ShowUpgradePanel and UpdateWhichButtons, against
libtpw_unit_RideStatusPanel_UpdateStatusWindow.dylib (tests/unit_RideStatusPanel_UpdateStatusWindow.cpp), main app ("SimTheme Park"),
src/LCPostFormat_RideStatusPanel_UpdateStatusWindow.cpp:
  RideStatusPanel::UpdateStatusWindow 0x101b9ac4, RideStatusPanel::ShowUpgradePanel 0x101ba6e8, RideStatusPanel::UpdateWhichButtons 0x101b8cc8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (the ride object's
queries, the window members, the list control's members, the list filler, the font and string getters, the table's FormatString) are fakes
on both sides that record their arguments and answer from the case. The windows' virtual members (Enable, SetValue, SetText: slots
0x1c, 0x24, 0x2c of the table at +0x118) and the panel's (slot 0x2c of the table at +0x20) are recorded too: on the PowerPC side the
window blocks hold a table of TVectors that point at the real functions' addresses, which are stubbed.

The world of a case: the panel (state, the windows, the table, the three figures), the ride object (object type, the condition float), the
ride record's word at +0x9c, the windows by id (some missing), the strings, fonts and draw objects the getters answer, gStaffRoomPanelThing
and the table that gText holds at its resource 6. What is compared: the event log, the panel's fields (the windows as labels), the object's
type and condition and the upgrade script's first words.

No fused operations; the fctiwz of the condition is run under HardwareRounding (the values stay within +-1e9). The conversion the Mac does
through the runtime at 0x10237cd8 (a double sum to a 32-bit integer) is a fake on the PowerPC side (the low word of the truncation).
Not tested: a NULL status window id 0x3e23 (read at once), NULL windows in the button functions and the upgrade function (the Mac reads
their tables), a NULL ride object in UpdateStatusWindow and ShowUpgradePanel, a NULL main window in ShowUpgradePanel, the size of the
operator new (the host's object has another size).
"""
import ctypes as C
import random
import struct

from unicorn import UC_HOOK_CODE

import calllog as CL
import tpwtest as T
from tpwtest import FnTest
from test_DynamicMesh import HardwareRounding

SHIM_LIBRARY = "unit_RideStatusPanel_UpdateStatusWindow"
MASK = 0xFFFFFFFF

FN_STATUS, FN_UPGRADE, FN_BUTTONS = 0x101B9AC4, 0x101BA6E8, 0x101B8CC8
SIZES = {FN_STATUS: 936, FN_UPGRADE: 528, FN_BUTTONS: 992}
OWN = set(SIZES)

ADDR = dict(thing=0x103947A4, text=0x1037FF18, script=0x102AD4F0)
TEXT_RES6 = 0x564
TV_LCFORMAT, TV_LCPOST, TV_UPGMSG, VT_INTTAG = 0x10267DC8, 0x10267DC0, 0x10267DD0, 0x102A12B4
MAC_PANEL = dict(state=0x04, f08=0x08, win=0x0C, swin=0x10, ops=0x20, uwin=0x24, speed=0x2C, capacity=0x30, duration=0x34)
MAC_WIN_Z = 0xDC
HOST_LAYOUT_OBJ = [0x0E, 0x40, 0x1A4]
HOST = {}

NULLABLE = [0x3E21, 0x3E1B, 0x3E16, 0x3E18, 0x3E19, 0x3E17]
STATUS_IDS = NULLABLE + [0x3E23]
BUTTON_IDS = [0x3E38, 0x3E34, 0x3E2A, 0x3E36, 0x3E35, 0x3E2C, 0x3E37]
UP_IDS = [0x1F4E, 0x1F4D]
ALL_IDS = STATUS_IDS + BUTTON_IDS + UP_IDS
LABELS = dict(panel=1, obj=2, table=3, main=4, status=5, upgrade=6, built=7, font=8, draw=9, info=10)
CHILD_LABEL = {i: 20 + n for n, i in enumerate(ALL_IDS)}

CUR = {"ptr": {}, "obj": 0}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- rounding


class ROUNDING(HardwareRounding):
    def __enter__(self):
        for begin, size in SIZES.items():
            self.hooks.append(self.e.uc.hook_add(UC_HOOK_CODE, self.step, begin=begin, end=begin + size - 1))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec(name, spec, ret=None):
    return CL.recorder(name, spec, ret)


def ptr_of(key):
    return lambda e, log: CUR["ptr"].get(key, 0)


def stub_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, CUR["ptr"].get(s32(e.r(4)), 0))


def stub_sum(e):
    L = e.cl
    L.rec("GetSumOfLast", (s32(e.r(3) - CUR["obj"]), s32(e.r(5))))
    e.write(e.r(4), struct.pack(">d", float(L.ret("GetSumOfLast"))))
    e.set_r(3, 0)


def stub_cvt(e):
    """0x10237cd8, the runtime's double to 32-bit integer: the low word of the truncation."""
    e.set_r(3, int(e.f(1)) & MASK)


def stub_format(e):
    L = e.cl
    tag = e.r(7)
    magic, vt, tid, value, flag = [e.u32(tag + 4 * i) for i in range(5)]
    # the vtable word is not compared: the emulated TOC slot it comes from is not initialised data
    L.rec("FormatString", (L.lab(e.r(3)), e.r(5) & MASK, e.r(6) & MASK, magic, 1, tid, s32(value), s32(flag)))
    text = "%d,%d" % (s32(value), s32(flag))
    e.write(e.r(4), text.encode("utf-16-be") + b"\0\0")
    e.set_r(3, 0)


def stub_build(e):
    L = e.cl
    L.rec("Build", (L.lab(e.r(3)), 1 if e.r(4) == ADDR["script"] else 0, 1 if e.r(5) == TV_UPGMSG else 0))
    e.set_r(3, CUR["ptr"].get(-5, 0))


def stub_columns(e):
    L = e.cl
    L.rec("SetColumnFormatsAll", (L.lab(e.r(3)), 1 if e.r(4) == TV_LCFORMAT else 0, 1 if e.r(5) == TV_LCPOST else 0))


A_NOTIFY = 0x101BC1A0
STUBS.internal(0x10107CD4, rec("GetDBInfo", "L", ptr_of(-1)))
STUBS.internal(0x101DBE40, stub_getwindow)
STUBS.internal(0x100F1000, stub_sum)
STUBS.internal(0x10237CD8, stub_cvt)
STUBS.internal(0x10107F50, rec("GetObjectAge", "L", "i"))
STUBS.internal(0x10239730, stub_format)
STUBS.internal(0x101DE130, rec("SetClientDrawText", "LS"))
STUBS.internal(0x1010CBA0, rec("GetProspectiveExcitementLevel", "Liii", "i"))
STUBS.internal(0x1010B1CC, rec("GetProspectiveReliability", "Lfff", "i"))
STUBS.internal(0x10108024, rec("GetRemainingLife", "L", "i"))
STUBS.internal(0x1010FFC8, rec("GetCurrentResidualValue", "L", "i"))
STUBS.internal(0x10180B84, rec("UIMain_GetObjectState", "Li", "i"))
STUBS.internal(0x101BB494, rec("UpdateKnobsWindow", "L"))
STUBS.internal(0x101BA128, rec("FillUpgradeBox", "iLiL"))
STUBS.internal(0x101DC1EC, rec("Show", "Lh", "i"))
STUBS.internal(0x1010CAE8, rec("IsClosed", "L", "i"))
STUBS.internal(0x1010AAAC, rec("IsOpenable", "L", "i"))
STUBS.internal(0x1010AB98, rec("IsCloseable", "L", "i"))
STUBS.internal(0x101CB1D0, rec("SetState", "Lii", "i"))
STUBS.internal(0x1008F330, rec("LLCC_IsRideMovable", "L", "i"))
STUBS.internal(0x101DEB78, stub_build)
STUBS.internal(0x10194608, rec("EnableNameClick", "Li"))
STUBS.internal(0x101DD8BC, rec("setZ", "Lh"))
STUBS.internal(0x101DCFD0, rec("setExtraParam", "LhL", "i"))
STUBS.internal(0x1018FBE8, rec("TitleTextCtor", "Nu"))
STUBS.internal(0x101DB624, rec("SetClientDrawObject", "LL"))
STUBS.internal(0x10180270, rec("GetFontTableEntry", "i", ptr_of(-3)))
STUBS.internal(0x101DA118, rec("SetFont", "LL"))
STUBS.internal(0x101DA204, rec("SetFontJust", "Lii"))
STUBS.internal(0x10180468, rec("UIMain_GetString", "i", ptr_of(-2)))
STUBS.internal(0x1018BD3C, rec("UI_TITLE_COLOUR", "L"))
STUBS.internal(0x101D2864, stub_columns)
STUBS.internal(0x101D2614, rec("SetColumnType", "Lhi"))
STUBS.internal(0x101CD2D4, rec("FindDrawObject", "s", ptr_of(-4)))
STUBS.internal(0x101D1EB4, rec("SetSelectionObject", "LL"))
# the windows' virtual members and the panel's
STUBS.internal(0x101DC320, rec("Enable", "Lh"))
STUBS.internal(0x101DFE84, rec("SetValue", "Li"))
STUBS.internal(0x101DE248, rec("SetText", "LS"))
STUBS.internal(A_NOTIFY, rec("Notify", "L"))


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

    def wstr(self, text):
        data = text.encode("utf-16-be" if self.fmt[0] == ">" else "utf-16-le") + b"\0\0"
        a = self.alloc(len(data) + 4)
        self.write(a, data)
        return a

    def setf(self, panel, name, v):
        self.write(panel + self.poff(name), struct.pack(self.fmt, v & MASK))

    def getf(self, panel, name):
        return self.rd_word(panel + self.poff(name))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        CUR["ptr"] = {}
        self.toc = e.r(2)
        self.vt = self.table({0x1C: 0x101DC320, 0x24: 0x101DFE84, 0x2C: 0x101DE248})
        self.opst = self.table({0x2C: A_NOTIFY})
        self.world = make_world(self, case)
        fixed = {a: lab for a, lab in self.world["labels"]}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def table(self, slots):
        base = self.alloc(0x40)
        for off, fn in slots.items():
            tv = self.alloc(8)
            self.words(tv, [fn, self.toc])
            self.words(base + off, [tv])
        return base

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def poff(self, name):
        return MAC_PANEL[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def newwindow(self):
        a = self.alloc(0x120)
        self.words(a + 0x118, [self.vt])
        return a

    def setptr(self, key, addr):
        CUR["ptr"][key] = addr
        if key == -6:
            CUR["obj"] = addr

    def clearptrs(self):
        CUR["ptr"] = {}

    def setp(self, panel, name, ptr):
        self.words(panel + MAC_PANEL[name], [ptr])

    def getp(self, panel, name):
        return self.rd_word(panel + MAC_PANEL[name])

    def set_ops(self, panel):
        self.words(panel + MAC_PANEL["ops"], [self.opst])

    def set_z(self, win, z):
        self.write(win + MAC_WIN_Z, struct.pack(">h", z))

    def set_global_ptr(self, name, ptr):
        self.words(ADDR[name], [ptr])

    def set_text_table(self, ptr):
        self.words(ADDR["text"] + TEXT_RES6, [ptr])

    def panel_size(self):
        return 0x40

    def call(self, fn, panel):
        with ROUNDING(self.e):
            T.call_ppc(self.e, fn, [panel])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_rsp_clearptrs()
        self.world = make_world(self, case)
        CL.host_begin(lib, {lab: a for a, lab in self.world["labels"]}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rsp_addr(name.encode())

    def poff(self, name):
        return HOST[name]

    def alloc(self, n):
        return self.lib.tpw_rsp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def newwindow(self):
        return self.lib.tpw_rsp_newwindow()

    def setptr(self, key, addr):
        self.lib.tpw_rsp_setptr(key, addr)

    def setp(self, panel, name, ptr):
        self.write(panel + HOST[name], struct.pack("<Q", ptr))

    def getp(self, panel, name):
        return struct.unpack("<Q", self.read(panel + HOST[name], 8))[0]

    def set_ops(self, panel):
        self.setp(panel, "ops", self.g("ops"))

    def set_z(self, win, z):
        self.write(win + HOST["z"], struct.pack("<h", z))

    def set_global_ptr(self, name, ptr):
        self.write(self.g(name), struct.pack("<Q", ptr))

    def set_text_table(self, ptr):
        self.write(self.g("text") + HOST["res6"], struct.pack("<Q", ptr))

    def panel_size(self):
        return HOST["size"] + 16

    def call(self, fn, panel):
        {FN_STATUS: self.lib.tpw_rsp_update_status, FN_UPGRADE: self.lib.tpw_rsp_show_upgrade, FN_BUTTONS: self.lib.tpw_rsp_update_buttons}[fn](panel)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_rsp_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "newwindow").restype, getattr(lib, p + "newwindow").argtypes = C.c_uint64, []
    getattr(lib, p + "setptr").restype, getattr(lib, p + "setptr").argtypes = None, [C.c_int32, C.c_uint64]
    getattr(lib, p + "clearptrs").restype, getattr(lib, p + "clearptrs").argtypes = None, []
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    for name in ("update_status", "show_upgrade", "update_buttons"):
        getattr(lib, p + name).restype, getattr(lib, p + name).argtypes = None, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    v = list(out)[:n]
    assert v[:3] == HOST_LAYOUT_OBJ, v
    keys = ["state", "f08", "win", "swin", "ops", "uwin", "speed", "capacity", "duration", "size", "res6", "z"]
    HOST.update(dict(zip(keys, v[4:])))
    HOST["objsize"] = v[3]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def make_world(s, case):
    """Builds the case's memory through the side's methods (same code for both) and answers the labels."""
    w = {}
    labels = []
    objsize = 0x500
    obj = s.alloc(objsize + 16)
    panel = s.alloc(s.panel_size())
    info = s.alloc(0x400)
    table = s.alloc(0x40)
    font, draw = s.alloc(16), s.alloc(16)
    main = s.newwindow()
    status = s.newwindow()
    upgrade = s.newwindow()
    built = s.newwindow()
    children = {i: s.newwindow() for i in ALL_IDS}
    # the object
    s.write(obj + 0x0E, struct.pack(s.fmt[0] + "H", case["objtype"]))
    s.words(obj + 0x40, [case["f40"]])
    s.words(info + 0x9C, [case["info9c"]])
    # the panel
    s.setf(panel, "state", case["state"])
    s.setf(panel, "f08", case["f08"])
    s.setf(panel, "speed", case["speed"])
    s.setf(panel, "capacity", case["capacity"])
    s.setf(panel, "duration", case["duration"])
    s.setp(panel, "win", main)
    s.setp(panel, "swin", status if case["has_status"] else 0)
    s.setp(panel, "uwin", upgrade if case["has_upgrade"] else 0)
    s.set_ops(panel)
    s.set_z(main, case["z"])
    # the globals
    s.set_global_ptr("thing", obj if case["has_obj"] else 0)
    s.set_text_table(table)
    # what the fakes answer
    s.setptr(-1, info)
    s.setptr(-2, s.wstr(case["text"]))
    s.setptr(-3, font)
    s.setptr(-4, draw)
    s.setptr(-5, built)
    s.setptr(-6, obj)
    for i, a in children.items():
        if i not in case["missing"]:
            s.setptr(i, a)
    roles = dict(panel=panel, obj=obj, table=table, main=main, status=status, upgrade=upgrade, built=built, font=font, draw=draw, info=info)
    for name, a in roles.items():
        labels.append((a, LABELS[name]))
    for i, a in children.items():
        labels.append((a, CHILD_LABEL[i]))
    w["labels"] = labels
    w["rev"] = {a: lab for a, lab in labels}
    w["panel"], w["obj"] = panel, obj
    w["script"] = s.g("script")
    return w


def execute(s, case):
    s.call({"status": FN_STATUS, "upgrade": FN_UPGRADE, "buttons": FN_BUTTONS}[case["op"]], s.world["panel"])


def observe(s, case):
    w = s.world
    panel = w["panel"]
    rev = w["rev"]

    def lab(a):
        return 0 if a == 0 else rev.get(a, -1)

    return {
        "log": s.log(),
        "state": s.getf(panel, "state"),
        "f08": s.getf(panel, "f08"),
        "speed": s.getf(panel, "speed"),
        "capacity": s.getf(panel, "capacity"),
        "duration": s.getf(panel, "duration"),
        "win": lab(s.getp(panel, "win")),
        "swin": lab(s.getp(panel, "swin")),
        "uwin": lab(s.getp(panel, "uwin")),
        "objtype": struct.unpack(s.fmt[0] + "H", s.read(w["obj"] + 0x0E, 2))[0],
        "f40": s.rd_word(w["obj"] + 0x40),
        "script": list(struct.unpack(s.fmt[0] + "2h", s.read(w["script"], 4))),
    }


def run(s, case):
    try:
        execute(s, case)
        return observe(s, case)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def rf40(r):
    k = r.random()
    if k < 0.2:
        return r.choice([0.0, 24.9, 25.0, 100.0, 99.99, 255.0, 255.9, 256.5, 1000.0, -1.0, -0.5, -300.7, 1e9, -1e9, 0.5])
    if k < 0.7:
        return r.uniform(-10, 120)
    return r.uniform(-1e6, 1e6)


def ri32(r):
    k = r.random()
    if k < 0.3:
        return r.choice([0, 1, -1, 100, 255, 256, 0x7FFFFFFF, -0x80000000, 0x01000001, -0x01000001])
    if k < 0.6:
        return r.randrange(-1000, 1000)
    return r.randrange(-0x80000000, 0x80000000)


def rfig(r):
    return r.choice([0, 0x7F, 0xFF, 0x100, 0x1FF, 0x80, 99, r.randrange(0, 256), r.randrange(0, 1 << 32), r.randrange(0, 1 << 32)])


STATES = [0, 1, 2, 3, 4, 5, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x19, 0x1A, 0x1B, 7, 0x7FFFFFFF]


def base(r, op):
    f40 = rf40(r)
    state = r.choice(STATES)
    c = {
        "op": op,
        "state": state & MASK,
        "f08": r.choice([0, 0, 1, 0x100, MASK]),
        "speed": ri32(r) & MASK,
        "capacity": ri32(r) & MASK,
        "duration": ri32(r) & MASK,
        "objtype": r.choice([0, 1, 0x1234, 0xFFFF, r.randrange(0, 0x10000)]),
        "f40": fb(f40),
        "info9c": r.choice([0, 1, 2, MASK, r.randrange(0, 1 << 32)]),
        "z": r.choice([0, 1, -1, 0x7FEF, -0x8000, 0x7FFF, r.randrange(-0x8000, 0x8000)]),
        "has_status": r.random() < 0.85,
        "has_upgrade": r.random() < 0.5,
        "has_obj": True,
        "missing": [],
        "text": r.choice(["Upgrade", "", "A rather longer title text"]),
    }
    if op == "status":
        c["missing"] = [i for i in NULLABLE + [0x1F4D] if r.random() < 0.2]
    obj_state = r.choice([state, state, (state + 1) & MASK, r.choice(STATES)])
    c["queues"] = {
        "GetObjectAge": [ri32(r) & MASK],
        "GetSumOfLast": [r.choice([0, 1, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, r.randrange(0, 1 << 32)])],
        "GetProspectiveExcitementLevel": [rfig(r)],
        "GetProspectiveReliability": [rfig(r)],
        "GetRemainingLife": [rfig(r)],
        "GetCurrentResidualValue": [rfig(r)],
        "UIMain_GetObjectState": [obj_state & MASK],
        "IsClosed": [r.choice([0, 1]) for _ in range(3)],
        "IsOpenable": [r.choice([0, 1])],
        "IsCloseable": [r.choice([0, 1])],
        "LLCC_IsRideMovable": [r.choice([0, 1, 0x100, 0x101, 0x1FF, 0xFF])],
    }
    return c


def with_(c, **kw):
    c = dict(c)
    q = dict(c["queues"])
    for k, v in kw.pop("q", {}).items():
        q[k] = v
    c["queues"] = q
    c.update(kw)
    return c


def status_edges():
    r = random.Random(5191)
    b = base(r, "status")
    b = with_(b, missing=[], has_status=True, has_upgrade=True, state=5, q={"UIMain_GetObjectState": [5]})
    yield b
    yield with_(b, q={"UIMain_GetObjectState": [6]})
    yield with_(b, has_upgrade=False, q={"UIMain_GetObjectState": [6]})
    yield with_(b, has_status=False)
    yield with_(b, has_status=False, has_upgrade=False, q={"UIMain_GetObjectState": [3]}, state=3)
    for i in NULLABLE + [0x1F4D]:
        yield with_(b, missing=[i])
    yield with_(b, missing=NULLABLE)
    for age in (0, 1, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 5, 0xFFFFFFFB, 1000000):
        yield with_(b, q={"GetObjectAge": [age]})
    for f in (0.0, -0.0, 0.5, 24.99, 25.0, 100.0, 255.9, 256.0, 256.5, 511.9, -1.5, 1e9, -1e9, 2147483000.0, -2147483000.0):
        yield with_(b, f40=fb(f))
    for v in (0, 0xFF, 0x100, 0x1FF, 0x7F, 0xFFFFFFFF, 0x80000000, 0x12345678):
        yield with_(b, q={"GetProspectiveExcitementLevel": [v], "GetProspectiveReliability": [v], "GetRemainingLife": [v],
                          "GetCurrentResidualValue": [v]})
    for v in (0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 12345678901 & MASK):
        yield with_(b, q={"GetSumOfLast": [v]})
    for v in (0, 1, MASK, 0x7FFFFFFF, 0x80000000, 0x01000001, 0xFEFFFFFF, 16777217):
        yield with_(b, speed=v, capacity=v ^ 1, duration=(v + 3) & MASK)
    for st in STATES:
        yield with_(b, state=st & MASK, q={"UIMain_GetObjectState": [(st + 1) & MASK]})
    for ocl in range(8):
        yield with_(b, q={"UIMain_GetObjectState": [6], "IsClosed": [(ocl >> k) & 1 for k in range(3)]})


def status_random(r):
    return base(r, "status")


def upgrade_edges():
    r = random.Random(5192)
    b = with_(base(r, "upgrade"), has_upgrade=False, f08=0, missing=[])
    yield b
    yield with_(b, has_upgrade=True)
    yield with_(b, f08=1)
    yield with_(b, f08=MASK)
    yield with_(b, has_upgrade=True, f08=1)
    for z in (0, 1, -1, 0x7FEF, 0x7FEE, -0x8000, 0x7FFF, -0x11):
        yield with_(b, z=z)
    for t in (0, 1, 0xFFFF, 0x8000):
        yield with_(b, objtype=t)
    yield with_(b, text="")


def upgrade_random(r):
    return base(r, "upgrade")


def buttons_edges():
    r = random.Random(5193)
    b = with_(base(r, "buttons"), has_obj=True, missing=[])
    yield with_(b, has_obj=False)
    for st in (0, 3, 0x19, 0x1A, 0x11, 0x12, 0x13, 0x14, 0x15, 0x10, 0x16, 7, MASK, 0x18, 0x1B):
        for closed in range(8):
            for opn in (0, 1):
                for cls in (0, 1):
                    if st not in (0, 3, 0x19, 0x1A, 0x13) and (closed not in (0, 7) or opn != cls):
                        continue
                    yield with_(b, state=st, info9c=r.choice([0, 1]),
                                q={"IsClosed": [(closed >> k) & 1 for k in range(3)], "IsOpenable": [opn], "IsCloseable": [cls]})
    for v in (0, 1, 0x100, 0x101, 0xFF, 0x1FF, MASK):
        yield with_(b, state=0, q={"LLCC_IsRideMovable": [v]})
    for v in (0, 1, 2, MASK, 0x80000000, 0x100):
        yield with_(b, state=0, info9c=v)


def buttons_random(r):
    c = base(r, "buttons")
    c["has_obj"] = r.random() < 0.9
    return c


TESTS = [
    FnTest("RideStatusPanel::UpdateStatusWindow", ppc, host, edge=status_edges, rnd=status_random),
    FnTest("RideStatusPanel::ShowUpgradePanel", ppc, host, edge=upgrade_edges, rnd=upgrade_random),
    FnTest("RideStatusPanel::UpdateWhichButtons", ppc, host, edge=buttons_edges, rnd=buttons_random),
]
