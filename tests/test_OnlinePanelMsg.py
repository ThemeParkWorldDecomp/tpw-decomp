"""Unit v5079, the online panel's message function, against libtpw_unit_OnlinePanelMsg.dylib (tests/unit_OnlinePanelMsg.cpp), main app
("SimTheme Park"), src/game/ui/ui_onlinePanel.cpp:
  OnlinePanelMsg 0x101e62d0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native 8 byte pointers) for the translation. Every callee outside the unit
(window calls, mouse, key tables, text, ride database, the ride hooks ...) is a fake on both sides that records its arguments and answers from
the case's queues; the three mouse mode constructors and operator new build real objects whose labels are 100, 101 ...

The world of a case: the panel / window / help globals, the cursor state and key flags, the cell and ride ids, the thing array with up to 8
rides (kind byte and flags halfword), the mouse and its mode, the screen size, the key tables and the text table. What is compared: the answer,
the call log, every global the function may write (as a label for pointers), the help text buffer.

CaptureKb's answer is read by the Mac code but the host declaration is void; the translation predicts it from gUIKeyboardWindow, so the host
world sets that global to agree with the answer the PowerPC fake gives. No fused operations. Not tested: a NULL thing at an index the code
reads (it dereferences it unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OnlinePanelMsg"
MASK = 0xFFFFFFFF

FN = 0x101E62D0
SIZES = {FN: 3552}
OWN = set(SIZES)

ADDR = dict(
    panel=0x10395E98, window=0x10395E94, e8c=0x10395E8C, e90=0x10395E90, target=0x10395E60, state=0x10395E64,
    kwin=0x10395E68, kpan=0x10395E69, block=0x10395E79, shown=0x10395E7A, noCell=0x102B0832, upId=0x102B0C88,
    noRide=0x102B0C8A, intr24=0x102DC1AC + 0x24, mouse=0x102DC1F4, mode=0x102DC1F0, w1e0=0x102DC1E0, w1e4=0x102DC1E4,
    cam=0x1037F22C, sho=0x1037F220, text7=0x1037FF18 + 0x568, width=0x10395BBC, height=0x10395BBA, buf=0x10380C48,
    kbwin=0,
)
for _i in range(8):
    ADDR["thing%d" % _i] = 0x1034CBFC + 0x14 * _i

# labels: windows 1..4, kid 5, key tables 6 7, text table 8, mouse 9, mode 10, rides 11..18
LABELS = {"W0": 1, "W1": 2, "W2": 3, "W3": 4, "K": 5, "CAM": 6, "SHO": 7, "TBL": 8, "MOUSE": 9, "MODE": 10}
for _i in range(8):
    LABELS["R%d" % _i] = 11 + _i

CUR = [None]


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


def u16s(e, a, n):
    """n UTF-16 characters from a, 0 from the first terminator on (the fakes' text record)."""
    if a == 0:
        return [0] * n
    raw = e.read(a, 2 * n)
    out, ended = [], False
    for i in range(n):
        ch = struct.unpack(">H", raw[2 * i:2 * i + 2])[0]
        if ended or ch == 0:
            ended = True
            out.append(0)
        else:
            out.append(ch)
    return out


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub(addr):
    def deco(fn):
        STUBS.internal(addr, fn)
        return fn
    return deco


def save_toc(e):
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


@stub(0x1018C7A4)
def f_move(e):
    e.cl.rec("MoveHelpText", (s16(e.r(3)), s16(e.r(4))))


def msg_stub(name):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
        e.set_r(3, 0)
    return f


STUBS.internal(0x101C9518, msg_stub("UI_SendMessage"))
STUBS.internal(0x101C9400, msg_stub("UI_PostMessage"))


def one_lab(name, addr, ret=None):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),))
        e.set_r(3, (L.ret(ret) & MASK) if ret else 0)
    STUBS.internal(addr, f)


one_lab("CaptureDevice", 0x101DCC10)
one_lab("UnCaptureDevice", 0x101DCC6C)
one_lab("GetAttr", 0x101DBB70, "GetAttr")
one_lab("GetDBInfoDummy", 0x0) if False else None


def lab_int(name, addr, ret=None):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s32(e.r(4))))
        e.set_r(3, (L.ret(ret) & MASK) if ret else 0)
    STUBS.internal(addr, f)


lab_int("UnCaptureKb", 0x101DCDB0)
lab_int("CaptureKb", 0x101DCCEC, "CaptureKb")
lab_int("GoFirstPerson", 0x1010ED8C)
lab_int("DoubleClick", 0x1007E248) if False else None


@stub(0x101DB884)
def f_getpos(e):
    e.cl.rec("GetPosition", (e.cl.lab(e.r(3)), s16(e.r(5))))
    e.set_r(3, 0)


@stub(0x101DBE40)
def f_getwin(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, CUR[0].obj["K"])


@stub(0x101DD7F0)
def f_setcursor(e):
    e.cl.rec("setCursor", (e.cl.lab(e.r(3)), s16(e.r(4))))


@stub(0x101DE318)
def f_default(e):
    L = e.cl
    L.rec("CallDefaultWindowHandler", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("CallDefault") & MASK)


@stub(0x100331C4)
def f_will(e):
    e.cl.rec("Will_HandleCameraMessages", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5))))


@stub(0x101E5E08)
def f_avatar(e):
    e.cl.rec("UpdateAvatarPos", ())


@stub(0x10089210)
def f_llcr(e):
    L = e.cl
    L.rec("LLCR_OverRide", (s32(e.r(4)), s32(e.r(5))))
    e.write(e.r(3), struct.pack(">H", L.ret("LLCR") & 0xFFFF))


@stub(0x1000DDE8)
def f_assert(e):
    s = e.cstr(e.r(4))
    if isinstance(s, str):
        s = s.encode("latin1")
    h = 0
    for ch in s:
        h = (h * 31 + ch) & 0x7FFFFFFF
    e.cl.rec("GAME_ASSERTMSG", (e.r(3) & 0xFF, h))


@stub(0x1011050C)
def f_upg(e):
    L = e.cl
    L.rec("UpgradesWhat", (L.lab(e.r(4)),))
    e.write(e.r(3), struct.pack(">H", L.ret("Upgrades") & 0xFFFF))


@stub(0x10107CD4)
def f_dbinfo(e):
    L = e.cl
    L.rec("GetDBInfo", (L.lab(e.r(3)),))
    info = CUR[0].obj["INFO"]
    e.write(info + 0x7A8, struct.pack(">I", L.ret("UIType") & MASK))
    e.set_r(3, info)


@stub(0x1007E2E0)
def f_assign(e):
    L = e.cl
    L.rec("AssignMouseMode", (L.lab(e.r(3)), L.lab(e.r(4))))


@stub(0x1007DE84)
def f_ingame(e):
    e.cl.rec("ProcessIngameMouse", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & MASK))


@stub(0x1007E248)
def f_dbl(e):
    L = e.cl
    L.rec("DoubleClick", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


STUBS.internal(0x1007EA78, CL.recorder("CMMDefault", "N"))
STUBS.internal(0x1007FAAC, CL.recorder("CMMOnline", "N"))
STUBS.internal(0x1007FE70, CL.recorder("CMMFirstPersonTarget", "N"))


@stub(0x1018C694)
def f_help(e):
    e.cl.rec("SetHelpTextString", tuple(u16s(e, e.r(3), 8)))


@stub(0x10239730)
def f_format(e):
    L = e.cl
    save_toc(e)
    this, out, size, fid = e.r(3), e.r(4), e.r(5) & MASK, e.r(6) & MASK
    magic, tag_id, text = 0, 0, 0
    if fid == 0x244:
        tag = e.r(7)
        magic = 1 if e.u32(tag) != 0 else 0
        tag_id = e.u32(tag + 8)
        text = e.u32(tag + 12)
    chars = u16s(e, text, 4)
    L.rec("FormatString", (L.lab(this), 1 if out == ADDR["buf"] else 0, size, fid, magic, tag_id, *chars))
    e.write(out, struct.pack(">3H", 0x41 + (fid & 15), 0x30 + (1 if magic else 0), 0))
    e.set_r(3, 0)


@stub(0x101810FC)
def f_name(e):
    L = e.cl
    L.rec("UIMain_GetObjectName", (L.lab(e.r(3)),))
    e.set_r(3, CUR[0].obj["NAME"])


@stub(0x1006CE28)
def f_hi(e):
    e.cl.rec("HighlightRide", ())


@stub(0x1006D0F8)
def f_unhi(e):
    e.cl.rec("UnhighlightRide", ())


@stub(0x1022A3B4)
def f_shifts(e):
    e.cl.rec("CheckShifts", ())
    e.set_r(3, e.cl.ret("CheckShifts") & MASK)


def key_stub(name, ret=None):
    def f(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)), s16(e.r(4)), s16(e.r(5))))
        e.set_r(3, (L.ret(ret) & MASK) if ret else 0)
    return f


STUBS.internal(0x1014FDEC, key_stub("KeyDown"))
STUBS.internal(0x1014FEF0, key_stub("KeyUp"))
STUBS.internal(0x1014FD68, key_stub("Search", "Search"))


@stub(0x101FC05C)
def f_menu(e):
    e.cl.rec("CreateMainMenu", (s32(e.r(3)),))


@stub(0x101B232C)
def f_postcard(e):
    e.cl.rec("UI_DoPostcard", ())


STUBS.internal(0x101C7448, lambda e: None)     # CoolRect::CoolRect, not logged (the host runs the real one)
STUBS.internal(0x101C74E0, lambda e: None)     # CoolRect::~CoolRect


@stub(0x1023BAA8)
def f_ptrglue(e):
    """The virtual call through the TVector: only CMouseMode::GetMode (vtable +0x2c) is called."""
    L = e.cl
    save_toc(e)
    L.rec("GetMode", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetMode") & MASK)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def put(self, a, f, v):
        self.write(a, struct.pack(self.e + f, v))

    def rd(self, a, f):
        v = struct.unpack(self.e + f, self.read(a, struct.calcsize(self.e + f)))
        return v[0] if len(v) == 1 else list(v)

    def nm(self, p):
        if p == 0:
            return "null"
        return self.names.get(p, "new")


def objects(s):
    """The per-case blocks, allocated before the log so the labels can name them."""
    o = {}
    for n in ("W0", "W1", "W2", "W3", "K", "CAM", "SHO", "TBL", "MOUSE"):
        o[n] = s.alloc(16)
    for i in range(8):
        o["R%d" % i] = s.alloc(s.ride_size)
    return o


class Ppc(Side):
    e = ">"
    ptrf = "I"
    off_kind, off_flags, off_ui = 2, 0x2E, 0x7A8
    ride_size = 0x40

    def __init__(self, e, case):
        self.em = e
        self.obj = objects(self)
        self.obj["INFO"] = e.alloc(0x800)
        self.obj["NAME"] = e.alloc(16)
        e.write(self.obj["NAME"], struct.pack(">5H", 0x52, 0x69, 0x64, 0x65, 0))
        vt = e.alloc(0x40)
        mode = e.alloc(16)
        e.write(mode, struct.pack(">I", vt))
        self.obj["MODE"] = mode
        self.names = {self.obj[n]: n for n in LABELS}
        CUR[0] = self
        fixed = {self.obj[n]: LABELS[n] for n in LABELS}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.em, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.em.alloc(n)

    def read(self, a, n):
        return self.em.read(a, n)

    def write(self, a, data):
        self.em.write(a, data)

    def log(self):
        return self.L.log

    def kbwin(self, took, win):
        pass

    def call(self, w, m, a, b):
        r3, _, _ = T.call_ppc(self.em, FN, [w & MASK, m & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    e = "<"
    ptrf = "Q"
    ride_size = 0x2000

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off_kind, self.off_flags, self.off_ui = lib.layout
        self.obj = objects(self)
        self.obj["MODE"] = lib.tpw_op_mode()
        self.names = {self.obj[n]: n for n in LABELS}
        lib.tpw_op_setkid(self.obj["K"])
        CL.host_begin(lib, {LABELS[n]: self.obj[n] for n in LABELS}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_op_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_op_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def kbwin(self, took, win):
        self.put(self.g("kbwin"), "Q", 0 if took else win)

    def call(self, w, m, a, b):
        return self.lib.tpw_op_msg(w, s32(m), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_op_addr.restype, lib.tpw_op_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_op_alloc.restype, lib.tpw_op_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_op_mode.restype, lib.tpw_op_mode.argtypes = C.c_uint64, []
    lib.tpw_op_setkid.restype, lib.tpw_op_setkid.argtypes = None, [C.c_uint64]
    lib.tpw_op_msg.restype, lib.tpw_op_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_op_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_op_layout(out)
    lib.layout = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o, g = s.obj, s.g
    P = s.ptrf

    def win(i):
        return o["W%d" % i] if i is not None else 0

    for n, k in (("panel", "panel"), ("window", "owin"), ("e8c", "e8c"), ("e90", "e90")):
        s.put(g(n), P, win(case[k]))
    s.put(g("target"), P, o["R%d" % case["target"]] if case["target"] is not None else 0)
    s.put(g("state"), "i", s32(case["state"]))
    for n in ("kwin", "kpan", "block", "shown"):
        s.put(g(n), "B", case[n])
    for n in ("noCell", "upId", "noRide", "intr24"):
        s.put(g(n), "H", case[n])
    s.put(g("mouse"), P, o["MOUSE"] if case["mouse"] else 0)
    s.put(g("mode"), P, o["MODE"] if case["mode"] else 0)
    s.put(g("w1e0"), "I", case["w1e0"])
    s.put(g("w1e4"), "I", case["w1e4"])
    s.put(g("width"), "h", s16(case["width"]))
    s.put(g("height"), "h", s16(case["height"]))
    s.put(g("cam"), P, o["CAM"])
    s.put(g("sho"), P, o["SHO"])
    s.put(g("text7"), P, o["TBL"])
    s.write(g("buf"), b"\0" * 16)
    for i in range(8):
        kind, flags = case["rides"][i]
        r = o["R%d" % i]
        s.put(r + s.off_kind, "B", kind)
        s.put(r + s.off_flags, "H", flags)
        s.put(g("thing%d" % i), P, 0 if (i == 0 and not case["thing0"]) else r)
    took = (case["queues"]["CaptureKb"][0] & 0xFFFF) != 0
    s.kbwin(took, o["W%d" % case["w"]])


def observe(s, case, answer):
    g = s.g
    d = {"answer": answer, "log": s.log()}
    for n in ("panel", "window", "e8c", "e90", "target", "mouse", "mode", "cam", "sho", "text7"):
        d[n] = s.nm(s.rd(g(n), s.ptrf))
    for n, f in (("state", "i"), ("w1e0", "I"), ("w1e4", "I"), ("kwin", "B"), ("kpan", "B"), ("block", "B"), ("shown", "B"),
                 ("noCell", "H"), ("upId", "H"), ("noRide", "H"), ("intr24", "H"), ("width", "h"), ("height", "h")):
        d[n] = s.rd(g(n), f)
    d["buf"] = s.rd(g("buf"), "8H")
    d["rides"] = [(s.rd(s.obj["R%d" % i] + s.off_kind, "B"), s.rd(s.obj["R%d" % i] + s.off_flags, "H")) for i in range(8)]
    return d


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(s.obj["W%d" % case["w"]], case["msg"], case["a"], case["b"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

MSGS = [0x14, 0x11, 0x10005, 0x10004, 0x10003, 0x10002, 0x10001, 0x10000, 0x1E, 0x1000A, 0x10007, 0x10008, 0x10009, 0x1000B, 0x1000C,
        0x10006, 0x1D, 0x1F, 0, 0x13, 0x15, 0x7FFFFFFF, -1, 0x10010]
KEYS = [0x2F, 0xD, 9, 0x73, 0x72, 0x74, 0, 1, 0xF, 0x10, 0x30, 0xC, 0x7FFF, -1]
A_FOR = {
    0x11: [1, 0, 2, -1],
    0x10005: [0, 1, 0x10, 0x11, 0x12, -1, 0x10000, 2],
    0x10004: [1, 0, 2], 0x10003: [0, 0x10, 0x11, -1],
    0x10002: [0, 1], 0x10001: [0, 1], 0x10000: [0], 0x1E: [0, 1],
    0x10007: [0, 1, -1], 0x1000A: KEYS, 0x1000B: KEYS,
}


def pick_b(r):
    return r.choice([0, 0x10000, 0x00FF0000, r.randrange(1 << 32), r.randrange(1 << 16), r.randrange(1 << 32) & 0xFF00FFFF])


def world(r, msg, a, b):
    c = {"msg": msg, "a": a, "b": b, "w": r.randrange(4)}

    def widx():
        k = r.random()
        if k < 0.35:
            return c["w"]
        if k < 0.45:
            return None
        return r.randrange(4)

    c["panel"], c["owin"], c["e8c"], c["e90"] = widx(), widx(), widx(), widx()
    c["rides"] = [(r.choice([3, 3, 3, 2, 0]), r.choice([0, 0x10, 0x10, 0xFFEF, r.randrange(65536)])) for _ in range(8)]
    c["thing0"] = r.random() < 0.7
    c["target"] = r.choice([None] + list(range(8)))
    c["state"] = r.choice([0, 1, 2, 3, 4, 2, 5, -1])
    for n in ("kwin", "kpan"):
        c[n] = r.choice([0, 1, 1, 0x80])
    c["block"] = r.choice([0, 0, 1, 0xFF])
    c["shown"] = r.choice([0, 1, 1, 0xFF])
    c["noCell"] = r.choice([0, 0, 0xFFFF, 5])
    c["upId"] = r.choice([0, 1, 0xFFFF])
    c["noRide"] = r.choice([0, 0, 0xFFFF, 3])
    c["intr24"] = r.choice([c["noCell"], 0, 1, 129, 0xFFFF, r.randrange(65536), r.randrange(65536)])
    c["mouse"] = r.random() < 0.5
    c["mode"] = r.random() < 0.8
    c["w1e0"], c["w1e4"] = r.randrange(1 << 32), r.randrange(1 << 32)
    c["width"] = r.choice([0, 1024, 2048, 1280, -7, r.randrange(-32768, 32768)])
    c["height"] = r.choice([0, 768, 1536, 1024, -9, r.randrange(-32768, 32768)])
    found = c["noRide"] if r.random() < 0.3 else r.randrange(8)
    if not c["thing0"] and found == 0 and found != c["noRide"]:
        found = 1
    c["queues"] = {
        "GetMode": [r.choice([9, 9, 1, 7, 0])],
        "GetAttr": [r.choice([0, 1, 1, 3, 2, r.randrange(1 << 32)])],
        "CallDefault": [r.randrange(1 << 32)],
        "CheckShifts": [r.choice([0, 1, 0xFFFF, r.randrange(65536)])],
        "Search": [r.choice([0, 0xF, 0x10, 1, 5, 0x1000F, 0x10000, 0xFFFF])],
        "CaptureKb": [r.choice([0, 1, 0x10000, 0xFFFF])],
        "LLCR": [found],
        "Upgrades": [c["upId"] if r.random() < 0.7 else r.choice([0, 1, 2, 0xFFFF])],
        "UIType": [r.choice([0, 3, 1, 0xFFFFFFFF]), r.choice([0, 3, 1])],
    }
    return c


def edges():
    r = random.Random(79)
    for m in MSGS:
        for a in A_FOR.get(m, [0, 1, -1]):
            for _ in range(3):
                yield world(r, m, a, pick_b(r))
    for _ in range(300):
        yield world(r, 0x1E, 0, pick_b(r))
    for _ in range(80):
        yield world(r, 0x10005, r.choice([0, 0, 1, 0x10]), pick_b(r))
    for _ in range(40):
        yield world(r, 0x1000B, r.choice(KEYS), r.choice([0, 0x10000, 0, 0x20000]))
    for _ in range(40):
        yield world(r, 0x14, 0, 0)


def rnd(r):
    m = r.choice(MSGS + [0x1E, 0x1E, 0x10005, 0x1000B, 0x1000B, 0x1000A, 0x14, 0x11])
    a = r.choice(A_FOR.get(m, [0, 1, -1]))
    return world(r, m, a, pick_b(r))


TESTS = [
    FnTest("OnlinePanelMsg", ppc, host, edge=edges, rnd=rnd),
]
