"""Unit v5074, the ride status panel's upgrade list, against libtpw_unit_FillUpgradeBox.dylib (tests/unit_FillUpgradeBox.cpp), main app
("SimTheme Park"), src/game/ui/ui_RideStatus.cpp:
  FillUpgradeBox 0x101ba128, UpgradePanelMsg 0x101ba4e0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the ride database walk, the control lookup, the game type constructor, the object's two queries, the list control's four
methods, the string table, wcstok, the panel's four methods, the window's three methods, UI_SendMessage) are fakes on both sides that record
their arguments (texts as a hash of the UTF-16 units) and answer from the case; GetTypeName, FormatString and UIMain_GetString write a text
into gUIMainConvertBuffer, wcstok is a real tokenizer on both sides.

The world of a case: the game type (the guard byte of its static is set, so its constructor never runs on the PowerPC side), the world and
text-table pointers, up to 6 rides (record words +0, +0x284, +0x804), their controls, the object (type, upgrade level), the object's ride
record (the three upgrade cost words), the three explanation texts. What is compared: the answer, the whole event log, the first 0x100
characters of gUIMainConvertBuffer.

No fused operations. Not tested: the game type constructor path (the host static's guard cannot be reset between cases and the PowerPC
guard is set); a ride or object type with no control (the original dereferences NULL).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FillUpgradeBox"
MASK = 0xFFFFFFFF

FN_FILL, FN_MSG = 0x101BA128, 0x101BA4E0
SIZES = {FN_FILL: 880, FN_MSG: 460}
OWN = set(SIZES)

# callees (in-binary, then glue)
C_REMOVEALL, C_GETFIRST, C_GETNEXT, C_GETCTL = 0x101D3CC8, 0x101547B4, 0x10154800, 0x100FA0B8
C_GETTYPENAME, C_ADDITEM, C_GETDBINFO, C_REQUPG = 0x10154AF0, 0x101D3000, 0x10107CD4, 0x1010C9C0
C_GETSTRING, C_UPDATEDISPLAY, C_GETWINDOW, C_GETDATA = 0x10180468, 0x101D2220, 0x101DBE40, 0x101D41F0
C_UPGRADE, C_UPDATESTATUS, C_REMOVE, C_BYEBYE = 0x101BBB88, 0x101B9AC4, 0x101BC1A0, 0x101BA930
C_SENDMSG, C_EXTRAPARAM, C_DEFAULT = 0x101C9518, 0x101DD030, 0x101DE318
C_FORMAT, C_WCSTOK = 0x10239730, 0x1023A078

ADDR = dict(gametype=0x102B3AA0, guard=0x102B3AA4, gw=0x1037EBFC, text=0x1037FF18, buffer=0x10380C48, panel=0x103959A4)
HOST_LAYOUT = [0xE, 0x4C, 0x448, 4, 0x10, 0x14, 0x20, 0]

# fake addresses (never dereferenced by the unit's code) and their labels
LIST, WIN, CHILD, EXTRA, TABLE, WORLD = 0x70000100, 0x70000200, 0x70000300, 0x70000400, 0x70000500, 0x70010000
BUF = ADDR["buffer"]
ST = {}          # the PowerPC side's tables (the host's are in the library): the same slot numbers as unit_FillUpgradeBox.cpp


def s32(v):
    return T.s32(v)


def fnv(units):
    h = 2166136261
    for c in units:
        h = ((h ^ c) * 16777619) & MASK
    return h


def utf16_at(e, addr):
    """The UTF-16 units of the string at addr (BE), up to the NUL."""
    out = []
    for i in range(0x4000):
        c = struct.unpack(">H", e.read(addr + 2 * i, 2))[0]
        if c == 0:
            break
        out.append(c)
    return out


def hash_at(e, addr):
    return 0xFFFFFFFF if addr == 0 else fnv(utf16_at(e, addr))


def put_text(e, addr, text):
    e.write(addr, text.encode("utf-16-be") + b"\x00\x00")


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub(addr):
    def deco(fn):
        STUBS.internal(addr, fn)
        return fn
    return deco


@stub(C_REMOVEALL)
def st_removeall(e):
    e.cl.rec("RemoveAllItems", (e.cl.lab(e.r(3)),))


@stub(C_GETFIRST)
def st_getfirst(e):
    e.cl.rec("GetFirstRide", (0,))
    ST[100] = 0
    e.set_r(3, ST[2] if ST[1] > 0 else 0)


@stub(C_GETNEXT)
def st_getnext(e):
    e.cl.rec("GetNextRide", (0,))
    ST[100] += 1
    e.set_r(3, ST[2 + ST[100]] if ST[100] < ST[1] else 0)


@stub(C_GETCTL)
def st_getctl(e):
    ident = (e.r(4) >> 16) & 0xFFFF
    e.cl.rec("GetObjectControl", (s32(e.r(3) - WORLD), ident, s32(e.r(5))))
    found = 0
    for i in range(ST[18]):
        if ST[20 + i] == ident:
            found = ST[36 + i]
            break
    e.set_r(3, found)


@stub(C_GETTYPENAME)
def st_gettypename(e):
    ident, buf, size = (e.r(4) >> 16) & 0xFFFF, e.r(5), e.r(6)
    e.cl.rec("GetTypeName", (ident, size, 1 if buf == BUF else 0))
    put_text(e, buf, "T%04x" % ident)


@stub(C_ADDITEM)
def st_additem(e):
    item = e.r(4)
    e.cl.rec("AddItem", (e.cl.lab(e.r(3)), hash_at(e, e.u32(item)), s32(e.u32(item + 4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))
    e.set_r(3, 0)


@stub(C_GETDBINFO)
def st_getdbinfo(e):
    e.cl.rec("GetDBInfo", (0,))
    e.set_r(3, ST[0])


@stub(C_REQUPG)
def st_requpg(e):
    e.cl.rec("RequiresUpgrade", (0,))
    e.set_r(3, e.cl.ret("RequiresUpgrade") & MASK)


@stub(C_GETSTRING)
def st_getstring(e):
    ident = s32(e.r(3))
    e.cl.rec("UIMain_GetString", (ident,))
    put_text(e, BUF, ST["texts"][(ident - 0x1A) & 3])
    e.set_r(3, BUF)


@stub(C_FORMAT)
def st_format(e):
    tag = e.r(7)
    this, out, size, ident = e.r(3), e.r(4), e.r(5), e.r(6)
    magic, tid, value = e.u32(tag), e.u32(tag + 8), s32(e.u32(tag + 0xC))
    e.cl.rec("FormatString", (e.cl.lab(this), 1 if out == BUF else 0, size, ident, magic, tid, value))
    put_text(e, out, "F%d_%d" % (ident, value))
    e.set_r(3, 0)


@stub(C_WCSTOK)
def st_wcstok(e):
    string, delim = e.r(3), e.r(4)
    e.cl.rec("wcstok", (hash_at(e, string), hash_at(e, delim)))
    delims = set(utf16_at(e, delim))
    p = string if string != 0 else ST.get("saved", 0)
    if p == 0:
        e.set_r(3, 0)
        return

    def at(a):
        return struct.unpack(">H", e.read(a, 2))[0]
    while at(p) != 0 and at(p) in delims:
        p += 2
    if at(p) == 0:
        ST["saved"] = 0
        e.set_r(3, 0)
        return
    start = p
    while at(p) != 0 and at(p) not in delims:
        p += 2
    if at(p) != 0:
        e.write(p, b"\x00\x00")
        ST["saved"] = p + 2
    else:
        ST["saved"] = p
    e.set_r(3, start)


@stub(C_GETWINDOW)
def st_getwindow(e):
    e.cl.rec("GetWindow", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, CHILD if e.cl.ret("GetWindow") else 0)


@stub(C_GETDATA)
def st_getdata(e):
    e.cl.rec("GetData", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">i", s32(e.cl.ret("GetData"))))
    e.set_r(3, 0)


@stub(C_UPGRADE)
def st_upgrade(e):
    e.cl.rec("Upgrade", (e.cl.lab(e.r(3)), s32(e.r(4)), e.r(5) & 0xFF))
    e.set_r(3, e.cl.ret("Upgrade") & MASK)


@stub(C_UPDATEDISPLAY)
def st_updatedisplay(e):
    e.cl.rec("UpdateDisplay", (e.cl.lab(e.r(3)),))


@stub(C_UPDATESTATUS)
def st_updatestatus(e):
    e.cl.rec("UpdateStatusWindow", (e.cl.lab(e.r(3)),))


@stub(C_REMOVE)
def st_remove(e):
    e.cl.rec("Remove", (e.cl.lab(e.r(3)),))


@stub(C_BYEBYE)
def st_byebye(e):
    e.cl.rec("UpgradePanelByeBye", (e.cl.lab(e.r(3)),))


@stub(C_SENDMSG)
def st_sendmsg(e):
    e.cl.rec("UI_SendMessage", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


@stub(C_EXTRAPARAM)
def st_extra(e):
    e.cl.rec("getExtraParam", (e.cl.lab(e.r(3)), T.s16(e.r(4))))
    e.set_r(3, EXTRA)


@stub(C_DEFAULT)
def st_default(e):
    e.cl.rec("CallDefaultWindowHandler", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, e.cl.ret("Default") & MASK)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def w16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def w8(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def rd16(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    fmt = ">I"
    off_fail = 0x804
    ride_size = 0x900

    @staticmethod
    def slot(s):
        return 0xC + 4 * s

    def __init__(self, e, case):
        self.e = e
        ST.clear()
        self.L = e.cl = CL.PpcLog({LIST: 1, ADDR["panel"]: 2, WIN: 3, CHILD: 4, EXTRA: 5, TABLE: 6}, case["queues"])
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

    def ptr(self, slot, value):
        ST[slot] = value

    def text(self, i, s):
        ST.setdefault("texts", [""] * 4)[i] = s

    def world(self):
        self.words(ADDR["gw"], [WORLD])
        self.words(ADDR["text"] + 0x564, [TABLE])
        self.write(ADDR["guard"], b"\x01")

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return 0 if fn == FN_FILL else r3 & MASK      # FillUpgradeBox is void


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off_fail = lib.host_off_fail
        self.ride_size = max(0x900, lib.host_ride_size)
        self.slot = lambda s: lib.host_base + lib.host_stride * s
        CL.host_begin(lib, {1: LIST, 2: self.g("panel"), 3: WIN, 4: CHILD, 5: EXTRA, 6: TABLE}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fub_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fub_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr(self, slot, value):
        self.lib.tpw_fub_setptr(slot, value)

    def text(self, i, s):
        self.lib.tpw_fub_text(i, s.encode())

    def world(self):
        self.ptr(90, WORLD)
        self.ptr(91, TABLE)

    def call(self, fn, args):
        if fn == FN_FILL:
            self.lib.tpw_fub_fill(s32(args[0]), args[1], s32(args[2]), args[3])
            return 0
        return self.lib.tpw_fub_msg(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fub_addr.restype, lib.tpw_fub_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fub_alloc.restype, lib.tpw_fub_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fub_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fub_setptr.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_fub_text.argtypes = [C.c_int32, C.c_char_p]
    lib.tpw_fub_fill.argtypes = [C.c_int32, C.c_uint64, C.c_int32, C.c_uint64]
    lib.tpw_fub_msg.restype, lib.tpw_fub_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_fub_layout(out)
    got = list(out)[:n]
    # ride slot offsets are reported as addresses of a NULL record: the offsets themselves
    assert got[:len(HOST_LAYOUT)] == HOST_LAYOUT, got
    s158, s107, s139, lib.host_off_fail, lib.host_ride_size, lib.host_text_off = got[len(HOST_LAYOUT):len(HOST_LAYOUT) + 6]
    # the host's balance values are 8 bytes each: offset of slot s is base + stride * s
    lib.host_stride = (s139 - s107) // 32
    lib.host_base = s107 - 107 * lib.host_stride
    assert lib.host_base + lib.host_stride * 158 == s158
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.world()
    s.words(s.g("gametype"), [case["gt"]])
    for i, t in enumerate(case["texts"]):
        s.text(i, t)
    s.write(s.g("buffer"), b"".join(struct.pack(s.fmt[0] + "H", 0x5A) for _ in range(0x100)))
    if case["op"] == "fill":
        rides = case["rides"]
        s.ptr(1, len(rides))
        ctl_ids = []
        ctls = dict(case["ctls"])
        for i, ride in enumerate(rides):
            a = s.alloc(s.ride_size)
            s.w16(a, ride["id"])
            s.words(a + s.slot(158), [ride["kind"]])
            s.words(a + s.off_fail, [ride["fail"]])
            s.ptr(2 + i, a)
        for i, (ident, (avail, cost, ua)) in enumerate(ctls.items()):
            c = s.alloc(0x20)
            s.w16(c, ident)
            s.words(c + 4, [cost])
            s.w8(c + 0x10, avail)
            s.words(c + 0x14, [ua])
            s.ptr(20 + i, ident)
            s.ptr(36 + i, c)
        s.ptr(18, len(ctls))
        db = s.alloc(s.ride_size)
        for lv in range(4):
            s.words(db + s.slot(107 + 16 * lv), [case["upg"][lv]])
        s.ptr(0, db)
        obj = s.alloc(0x448)
        s.w16(obj + 0xE, case["otype"])
        s.w8(obj + 0x4C, case["level"])
        case["_obj"] = obj
    else:
        s.ptr(96, CHILD)
        s.ptr(97, EXTRA)


def execute(s, case):
    if case["op"] == "fill":
        return s.call(FN_FILL, [case["kind"], LIST, case["clear"], case["_obj"]])
    return s.call(FN_MSG, [WIN] + case["args"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "buffer": s.rd16(s.g("buffer"), 0x100)}


def run(s, case):
    try:
        case = dict(case)
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

TEXTS = ["", "one", "one\ntwo", "\n\nx\n\ny\n", "a\n", "\n", "l1\nl2\nl3\nl4", "\nz"]


def fill_case(r, **over):
    kind = r.choice([0, 1, 2, -1, 5])
    nr = r.choice([0, 1, 2, 3, 4, 6])
    ids = r.sample(range(1, 0xFFFF), nr + 1)
    rides = []
    ctls = {}
    for i in range(nr):
        rides.append(dict(id=ids[i], kind=kind if r.random() < 0.6 else r.choice([0, 1, 2, 7]), fail=r.choice([0, 0, 0, 1, 0x100])))
        ctls[ids[i]] = (r.choice([0, 1, 1, 0x80, 0xFF]), r.randrange(-100, 100000), r.choice([0, 1, 2, 3]))
    otype = r.choice(ids) if r.random() < 0.3 else ids[nr]
    if otype not in ctls:
        ctls[otype] = (r.choice([0, 1]), r.randrange(0, 5000), r.choice([0, 1, 2, 3]))
    c = dict(op="fill", kind=kind, clear=r.choice([0, 1, -1]), gt=r.choice([0, 0, 1, 2]), level=r.choice([0, 1, 2, 3, 255]),
             rides=rides, ctls=ctls, otype=otype, upg=[r.randrange(-50, 100000) for _ in range(4)],
             texts=[r.choice(TEXTS) for _ in range(4)], queues={"RequiresUpgrade": [r.choice([0, 1])]})
    c.update(over)
    c["queues"] = dict(c["queues"])
    return c


def fill_edges():
    r = random.Random(5074)
    for gt in (0, 1, 2):
        for level in (0, 1, 2, 3, 255):
            for req in (0, 1):
                for clear in (0, 1):
                    yield fill_case(r, gt=gt, level=level, clear=clear, queues={"RequiresUpgrade": [req]})
    for nr in range(1, 7):
        yield fill_case(r, gt=0, level=0, queues={"RequiresUpgrade": [0]})


def fill_random(r):
    return fill_case(r)


def msg_case(r, msg=None, a=None, b=None):
    msg = r.choice([0x100, 0x14, 0x15, 0x13, 0, -1, 0x101, 0x400, 0x400, 0x400, 0x3FF, 0x401, 0x16]) if msg is None else msg
    a = r.choice([-2, -1, 0, 5, 0x1234]) if a is None else a
    b = r.choice([-1, 0, 1, 7, -5]) if b is None else b
    return dict(op="msg", args=[msg, a, b], gt=0, texts=["", "", "", ""],
                queues={"GetWindow": [r.choice([0, 1])], "GetData": [r.choice([-0x1ED2, -0x32C7, 5, 0, -3, 1, -1])],
                        "Upgrade": [r.choice([0, 1])], "Default": [r.randrange(-5, 5)]})


def msg_edges():
    r = random.Random(7450)
    for msg in (0x100, 0x14, 0x15, 0x13, 0, -1, 0x101, 0x3FF, 0x400, 0x401):
        for a in (-2, 0):
            for b in (-1, 3):
                yield msg_case(r, msg, a, b)
    for win in (0, 1):
        for data in (-0x1ED2, -0x32C7, 4, 0, -3):
            for up in (0, 1):
                c = msg_case(r, 0x400, 9, 2)
                c["queues"] = {"GetWindow": [win], "GetData": [data], "Upgrade": [up], "Default": [3]}
                yield c


def msg_random(r):
    return msg_case(r)


# ---------------------------------------------------------------------------------------------------------------- LCPostFormat
# LCPostFormat 0x101B9FB8 is OnlineParkListPanelLCPostFormat in src/game/ui/ui_RideStatus.cpp. The case is the font table index global (0x102AD5DC);
# the window and its first child are fakes that record; the child's vtable slot 0x24 is the fake SetValue (a call through any other slot logs
# WrongVirtual). Compared: the whole event log.

FN_LCP = 0x101B9FB8
STUBS.real.add(FN_LCP)
FONT_BASE, FAKE_SETVALUE, FONT_INDEX = 0x70000600, 0x70000700, 0x102AD5DC
LCP = {}

STUBS.internal(0x1018D11C, CL.recorder("TextObjectCtor", "Nu"))
STUBS.internal(0x1018E594, CL.recorder("CashObjectCtor", "Nu"))


@stub(0x10180270)
def st_font(e):
    e.cl.rec("GetFontTableEntry", (e.r(3),))
    e.set_r(3, FONT_BASE + 0x10 * e.r(3))


@stub(0x101DB624)
def st_setclient(e):
    e.cl.rec("SetClientDrawObject", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


@stub(0x101DA118)
def st_setfont(e):
    e.cl.rec("SetFont", (e.cl.lab(e.r(3)), s32(e.r(4) - FONT_BASE)))


@stub(0x101DA3AC)
def st_setcolour(e):
    e.cl.rec("SetFontColour", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))


@stub(0x101DA204)
def st_setjust(e):
    e.cl.rec("SetFontJust", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


@stub(0x101DBAC4)
def st_getchild(e):
    e.cl.rec("GetChild", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, LCP["child"])


@stub(0x1023BAA8)
def st_ptrglue(e):
    if e.r(12) == FAKE_SETVALUE:
        e.cl.rec("SetValue", (e.cl.lab(e.r(3)), s32(e.r(4)), 1))
    else:
        e.cl.rec("WrongVirtual", (e.cl.lab(e.r(3)), s32(e.r(4))))


def lcp_ppc(e, case):
    child = e.alloc(0x200)
    vt = e.alloc(0x80)
    e.write(vt + 0x24, struct.pack(">I", FAKE_SETVALUE))
    e.write(child + 0x118, struct.pack(">I", vt))
    LCP["child"] = child
    e.write(FONT_INDEX, struct.pack(">I", case["font"]))
    e.cl = log = CL.PpcLog({WIN: 1, child: 2}, {})
    saved = STUBS.activate(e)
    try:
        T.call_ppc(e, FN_LCP, [WIN])
    finally:
        CL.Stubs.deactivate(e, saved)
    return {"log": log.log}


def lcp_host(lib, case):
    bind(lib)
    lib.tpw_fub_child.restype = C.c_uint64
    lib.tpw_fub_lc.argtypes = [C.c_uint64]
    child = lib.tpw_fub_child()
    C.c_uint32.from_address(lib.tpw_fub_addr(b"fontindex")).value = case["font"]
    CL.host_begin(lib, {1: WIN, 2: child}, {})
    lib.tpw_fub_lc(WIN)
    return {"log": CL.host_end(lib)}


def lcp_edges():
    for f in (0, 1, 7, 8, 0x7F, 0xFFFF):
        yield {"font": f}


def lcp_random(r):
    return {"font": r.choice([7, r.randrange(0, 40), r.getrandbits(16)])}


TESTS = [
    FnTest("LCPostFormat", lcp_ppc, lcp_host, edge=lcp_edges, rnd=lcp_random),
    FnTest("FillUpgradeBox", ppc, host, edge=fill_edges, rnd=fill_random),
    FnTest("UpgradePanelMsg", ppc, host, edge=msg_edges, rnd=msg_random),
]
