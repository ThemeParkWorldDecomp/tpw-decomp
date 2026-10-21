"""Unit v5064, the publish park panel and the online park list panel, against libtpw_unit_PublishPanelMsg.dylib
(tests/unit_PublishPanelMsg.cpp), main app ("SimTheme Park"), src/game/ui/ui_onlineParkListPanel.cpp, src/game/ui/ui_publishParkPanel.cpp:
  PublishPanelMsg 0x101eca2c, CreatePublishParkPanel 0x101eccc8, CreateOnlineParkListPanel 0x101ec4e0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the interface calls,
the online system's constructor, RequestUpload and GetList, the game type's constructor, the string's constructor and destructor, the
interface table's FormatString, the window's virtual SetText through __ptr_glue) is a fake on both sides that records its arguments
(objects as labels); the objects they answer come from the world (a table by label).

The world of a case: the windows (labels 1 to 11, with their z), the lobby, the screens and the advisor, the online system (or none), the
list the system holds, the park the player picked (its id and its name's label), the upload path, gText's table of strings, the panel
pointer. What is compared: the answer, the log, and the globals the functions write (the panel pointers, the online system, the close
function) as labels.

No fused operations. Not tested: the game type object's guard byte and value are the program's own state on each side (neither is written
by a case), which evolve the same way as the calls repeat; the vtable word of the string tag FormatString receives is not logged (the host
has no way to name an import's vtable).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_PublishPanelMsg"
MASK = 0xFFFFFFFF

FN_MSG, FN_PUBLISH, FN_LIST = 0x101ECA2C, 0x101ECCC8, 0x101EC4E0
SIZES = {FN_MSG: 608, FN_PUBLISH: 628, FN_LIST: 812}
OWN = set(SIZES)

# ---- the Mac's addresses of the globals; the host library's come from tpw_pp_addr(name)
ADDR = dict(gLobby=0x102E4888, gLobbyScreens=0x102E49B8, gOnlineSystem=0x10380AB0, selection=0x102E4864, uploadfile=0x102E4868,
            panel=0x10396278, desc=0x1039627C, pname=0x1039637C, pscript=0x102B11E6, lscript=0x102B1086, listpanel=0x10396234,
            listclose=0x10396230, gGame=0x1037F134, convbuf=0x10380C48, gText=0x1037FF18)
GLOBAL_LABEL = dict(pscript=70, lscript=71, desc=72, pname=73, convbuf=74, gGame=75)
# the TVectors the original passes: label -> address
TVEC = {60: 0x102685C0, 61: 0x102685C8, 62: 0x102685B8, 63: 0x102685B0, 64: 0x102685A8, 65: 0x102685A0, 66: 0x10270100, 67: 0x10270108}
TV_SETTEXT = 0x10271000          # what the fake window's vtable holds at 0x2c
GAMETYPE = 0x102B3AA0
HOST_LAYOUT = [0x18, 8]

WIN_OF_ID = {0x11E3: 3, 0x11E4: 4, 0x14BEB: 5, 0x10: 6, 0x11: 7, 0x12: 8, 0x13: 9, 0x14: 10}
STR_OF_ID = {0xDC: 30, 0xDA: 31, 0x126: 32, 0x127: 33, 0x128: 34, 0x129: 35, 0x12A: 36}
WORLD = {}                       # the current case's PowerPC objects by label


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def obj(label):
    return lambda e, log: WORLD[label] & MASK


def stub(addr, name, spec="", ret=None, then=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret=ret, then=then))


def window_ret(e, log):
    return WORLD.get(WIN_OF_ID.get(s32(e.r(4)), 0), 0) & MASK


def string_ret(e, log):
    return WORLD.get(STR_OF_ID.get(s32(e.r(3)), 0), 0) & MASK


def sys_then(e, log):
    e.write((e.r(3) + 0xE8) & MASK, struct.pack(">I", WORLD[13] & MASK))


def type_then(e, log):
    e.write(GAMETYPE, struct.pack(">I", e.r(3) & MASK))


def str_then(e, log):
    e.write((e.r(3) + 8) & MASK, struct.pack(">I", WORLD[42] & MASK))


stub(0x101C9518, "UI_SendMessage", "Liii")
stub(0x101C9400, "UI_PostMessage", "Liii")
stub(0x10182364, "UIMain_DisableAllKeyTables")
stub(0x10182304, "UIMain_EnableAllKeyTables")
stub(0x1018116C, "UIMain_DecideKbCapture")
stub(0x10180514, "UIMain_CheckPanel")
stub(0x101F1448, "DefaultDialogCallback", "Liii", ret="i")
stub(0x101F12C8, "Dialog", "LLi", ret=obj(11))
stub(0x10180270, "GetFontTableEntry", "i", ret=obj(41))
stub(0x10180468, "UIMain_GetString", "i", ret=string_ret)
stub(0x10180794, "UIMain_SetTitleText", "LiL")
stub(0x101818E4, "MAKECOLUMNBUTTON", "LLiiiii")
stub(0x101ECF6C, "RemovePublishParkPanel")
stub(0x1016F7D0, "CFSOnlineSystem", "N", then=sys_then)
stub(0x1016F5D8, "CGameType", "", ret="i", then=type_then)
stub(0x10174FE8, "RequestUpload", "LLiLLiLi")
stub(0x10178F54, "GetList", "LiLi")
stub(0x101DBE40, "GetWindow", "Li", ret=window_ret)
stub(0x101DCCEC, "CaptureKb", "Li")
stub(0x101DD8BC, "setZ", "Lh")
stub(0x101DB624, "SetClientDrawObject", "LL")
stub(0x101DA118, "SetFont", "LL")
stub(0x101DA2D8, "SetFontWrap", "Lii")
stub(0x101DA204, "SetFontJust", "Lii")
stub(0x101DEB78, "Build", "LLL", ret=obj(11))
stub(0x101CD7E0, "SetStaticTextArea", "LLi")
stub(0x101CDAC4, "SetCursorDrawObject", "LL")
stub(0x101D2864, "SetColumnFormatsAll", "LLL")
stub(0x101D2614, "SetColumnType", "Lhi")
stub(0x101D1EB4, "SetSelectionObject", "LL")
stub(0x100A7074, "Add", "Lii")
stub(0x1018F1D0, "CursorSprite", "Ni")
stub(0x10238740, "TbStrCtor", "N", then=str_then)
stub(0x10238710, "TbStrDtor", "L")
stub(0x1014A058, "GetCurrentThemeName", "L", ret=obj(43))
stub(0x1014A478, "GetLocalisedThemeName", "LLL")
stub(0x101CD2D4, "FindDrawObject", "", ret=obj(40))


def stub_settext(e):
    """__ptr_glue called for the window's slot 0x2c (SetText(window, text))."""
    L = e.cl
    if (e.r(12) & MASK) != TV_SETTEXT:
        raise AssertionError("the virtual call did not go through slot 0x2c: r12 = %#x" % (e.r(12) & MASK))
    L.rec("SetText", (L.lab(e.r(3) & MASK), L.lab(e.r(4) & MASK)))


def stub_format(e):
    """TbITable::FormatString(this, out, size, id, tag*): the tag's magic, id and text."""
    L = e.cl
    tag = e.r(7) & MASK
    L.rec("FormatString", (L.lab(e.r(3) & MASK), L.lab(e.r(4) & MASK), e.r(5) & MASK, e.r(6) & MASK, e.u32(tag), e.u32(tag + 8),
                           L.lab(e.u32(tag + 12))))


STUBS.internal(0x1023BAA8, stub_settext)
STUBS.internal(0x10239730, stub_format)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def name(self, value):
        """A pointer as its label: 0 for NULL, -2 for an object made during the call."""
        return self.known.get(value, 0 if value == 0 else -2)

    def make_world(self, case):
        w = {}
        for label in range(1, 12):
            w[label] = self.window(label, case["z"][label])
        for label in (12, 18, 30, 31, 32, 33, 34, 35, 36, 40, 41, 42, 43, 13, 16, 45, 46):
            w[label] = self.blk(label, 0x40 if label != 12 else 0x404)
        w[16] = self.blk(16, 0x40)
        w[12] = self.system(12, w[13])
        w[14] = self.lobby(14, w[2])
        w[15] = self.screens(15, w[16])
        w[44] = self.city(44, case["cityid"], w[45])
        return w

    def build(self, case, w):
        self.setptr(self.g("gLobby"), w[14])
        self.setptr(self.g("gLobbyScreens"), w[15])
        self.setptr(self.g("gOnlineSystem"), w[12] if case["sys"] else 0)
        self.setptr(self.g("selection"), w[44])
        self.setptr(self.g("uploadfile") + 8, w[46])
        self.setptr(self.g("panel"), w[case["panel"]] if case["panel"] else 0)
        self.setptr(self.g("listpanel"), w[case["listpanel"]] if case["listpanel"] else 0)
        self.setptr(self.g("listclose"), self.fnaddr(case["closeold"]) if case["closeold"] else 0)
        self.settable(w[18])


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.objs = {}
        WORLD.clear()
        self.w = self.make_world(case)
        WORLD.update(self.w)
        self.known = {addr: label for label, addr in self.objs.items()}
        for name, label in GLOBAL_LABEL.items():
            self.known[ADDR[name]] = label
        for label, addr in TVEC.items():
            self.known[addr] = label
        self.L = e.cl = CL.PpcLog(dict(self.known), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def fnaddr(self, label):
        return TVEC[label]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def setptr(self, a, v):
        self.write(a, struct.pack(">I", v & MASK))

    def rdptr(self, a):
        return self.rd_words(a, 1)[0]

    def settable(self, table):
        self.setptr(ADDR["gText"] + 0x564, table)

    def log(self):
        return self.L.log

    def reg(self, label, a):
        self.objs[label] = a
        return a

    def window(self, label, z):
        a = self.reg(label, self.e.alloc(0x140))
        vt = self.e.alloc(0x40)
        self.words(vt + 0x2C, [TV_SETTEXT])
        self.setptr(a + 0x118, vt)
        self.write(a + 0xDC, struct.pack(">H", z & 0xFFFF))
        return a

    def blk(self, label, size):
        return self.reg(label, self.e.alloc(size))

    def lobby(self, label, win):
        a = self.blk(label, 0x40)
        self.setptr(a + 4, win)
        return a

    def screens(self, label, adv):
        a = self.blk(label, 0x40)
        self.setptr(a + 0x14, adv)
        return a

    def system(self, label, parklist):
        a = self.blk(label, 0x404)
        self.setptr(a + 0xE8, parklist)
        return a

    def city(self, label, cid, name):
        a = self.blk(label, 0x20)
        self.words(a + 8, [cid])
        self.setptr(a + 0x14, name)
        return a

    def call_msg(self, msg, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_MSG, [self.w[1], msg & MASK, a & MASK, b & MASK])
        return s32(r3)

    def call_publish(self):
        T.call_ppc(self.e, FN_PUBLISH, [])
        return 0

    def call_list(self, closefn):
        T.call_ppc(self.e, FN_LIST, [self.fnaddr(closefn) if closefn else 0])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.objs = {}
        self.w = self.make_world(case)
        self.known = {addr: label for label, addr in self.objs.items()}
        fixed = dict(self.objs)
        for name, label in GLOBAL_LABEL.items():
            fixed[label] = self.g(name)
        for label in TVEC:
            fixed[label] = lib.tpw_pp_fn(label)
        for label, addr in fixed.items():
            self.known[addr] = label
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pp_addr(name.encode())

    def fnaddr(self, label):
        return self.lib.tpw_pp_fn(label)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rdptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def settable(self, table):
        self.lib.tpw_pp_settable(table)

    def log(self):
        return CL.host_end(self.lib)

    def reg(self, label, a):
        self.objs[label] = a
        return a

    def window(self, label, z):
        return self.reg(label, self.lib.tpw_pp_window(label, z))

    def blk(self, label, size):
        return self.reg(label, self.lib.tpw_pp_blk(label, size))

    def lobby(self, label, win):
        return self.reg(label, self.lib.tpw_pp_lobby(label, win))

    def screens(self, label, adv):
        return self.reg(label, self.lib.tpw_pp_screens(label, adv))

    def system(self, label, parklist):
        return self.reg(label, self.lib.tpw_pp_system(label, parklist))

    def city(self, label, cid, name):
        return self.reg(label, self.lib.tpw_pp_city(label, cid, name))

    def call_msg(self, msg, a, b):
        return s32(self.lib.tpw_pp_msg(self.w[1], s32(msg), s32(a), s32(b)))

    def call_publish(self):
        self.lib.tpw_pp_publish()
        return 0

    def call_list(self, closefn):
        self.lib.tpw_pp_list(self.fnaddr(closefn) if closefn else 0)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, u32, i32 = C.c_uint64, C.c_uint32, C.c_int32
    sig = {
        "addr": (u64, [C.c_char_p]), "alloc": (u64, [u32]), "fn": (u64, [i32]), "blk": (u64, [i32, u32]),
        "window": (u64, [i32, i32]), "lobby": (u64, [i32, u64]), "screens": (u64, [i32, u64]), "system": (u64, [i32, u64]),
        "city": (u64, [i32, i32, u64]), "settable": (None, [u64]), "msg": (i32, [u64, i32, i32, i32]), "publish": (None, []),
        "list": (None, [u64]),
    }
    for name, (res, args) in sig.items():
        f = getattr(lib, "tpw_pp_" + name)
        f.restype, f.argtypes = res, args
    lib.tpw_pp_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_pp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def execute(s, case):
    if case["op"] == "msg":
        return s.call_msg(*case["args"])
    if case["op"] == "publish":
        return s.call_publish()
    return s.call_list(case["closefn"])


def observe(s, case, answer):
    return {
        "answer": answer,
        "log": s.log(),
        "panel": s.name(s.rdptr(s.g("panel"))),
        "sys": s.name(s.rdptr(s.g("gOnlineSystem"))),
        "listpanel": s.name(s.rdptr(s.g("listpanel"))),
        "listclose": s.name(s.rdptr(s.g("listclose"))),
    }


def run(s, case):
    try:
        s.build(case, s.w)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def zs(r):
    return {label: r.choice([0, 1, 0x100, 0x7FFD, 0x7FFE, 0x7FFF, -1, -4, -0x8000, r.randrange(-300, 300)]) for label in range(1, 12)}


def base(r, op):
    return {
        "op": op, "z": zs(r), "sys": r.choice([0, 1]), "panel": r.choice([0, 11, 1]), "listpanel": r.choice([0, 11]),
        "closefn": r.choice([0, 66, 67]), "closeold": r.choice([0, 66, 67]),
        "cityid": r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, r.randrange(-1000, 1000)]) & MASK,
        "queues": {"DefaultDialogCallback": [r.choice([0, 1, -1, r.randrange(-1000, 1000)])],
                   "CGameType": [r.choice([0, 1, 2, 3, -1, 0x7FFFFFFF])]},
    }


def msg_case(r, message=None, a=None, b=None):
    c = base(r, "msg")
    ints = [0, 1, -1, 2, -2, -3, 0x11E3, 0x11E4, 0x11E5, r.randrange(-100, 100), r.randrange(-0x80000000, 0x7FFFFFFF)]
    msgs = [0x100, 0xFF, 0x101, 0x14, 0x15, 0x13, 0x16, 0x801, 0x802, 0x803, 0x804, 0x805, 0, -1, 0x7FF, 0x806, 0x10, 0x12]
    c["args"] = [r.choice(msgs + [r.randrange(-0x80000000, 0x7FFFFFFF)]) if message is None else message,
                 r.choice(ints) if a is None else a, r.choice(ints) if b is None else b]
    return c


def msg_edges():
    r = random.Random(5064)
    for message in (0x100, 0xFF, 0x101, 0x13, 0x14, 0x15, 0x16, 0x801, 0x802, 0x803, 0x804, 0x805, 0x806, 0, -1, 0x7FFFFFFF, -0x80000000):
        for a in (-1, -2, -3, 0, 1, 0x11E3, 0x11E4, 0x7FFFFFFF, -0x80000000):
            c = msg_case(r, message, a)
            c["sys"] = (a == -1) and (message == 0x100) and ((a + message) & 1) == 0 or r.choice([0, 1])
            yield c
    for sysnull in (0, 1):
        for typ in (0, 1, 2, 3, -1):
            c = msg_case(r, 0x100, -1)
            c["sys"], c["queues"]["CGameType"] = sysnull, [typ]
            yield c


def msg_random(r):
    return msg_case(r)


def publish_case(r):
    return base(r, "publish")


def publish_edges():
    r = random.Random(5065)
    for z in (0, 1, 0x7FFB, 0x7FFC, 0x7FFD, 0x7FFF, -1, -4, -5, -0x8000):
        c = publish_case(r)
        c["z"][11] = z
        yield c
    yield publish_case(r)


def list_case(r):
    return base(r, "list")


def list_edges():
    r = random.Random(5066)
    for sysnull in (0, 1):
        for closefn in (0, 66, 67):
            for cid in (0, 1, -1, 0x7FFFFFFF, -0x80000000):
                c = list_case(r)
                c["sys"], c["closefn"], c["cityid"] = sysnull, closefn, cid & MASK
                yield c


TESTS = [
    FnTest("PublishPanelMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    FnTest("CreatePublishParkPanel", ppc, host, edge=publish_edges, rnd=publish_case),
    FnTest("CreateOnlineParkListPanel", ppc, host, edge=list_edges, rnd=list_case),
]
