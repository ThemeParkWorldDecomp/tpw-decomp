"""Unit v5550, the online park info panel's messages and the park find panel, against libtpw_unit_OnlineParkInfoPanelMsg.dylib
(tests/unit_OnlineParkInfoPanelMsg.cpp), main app ("SimTheme Park"), src/OnlineParkInfoPanelMsg.cpp:
  OnlineParkInfoPanelMsg 0x101ea7f0, GeneralParkDownloadCompleteCallback 0x101ea734, VisitParkPostDownloadCallbackFn 0x101ea684,
  __sinit_ui_onlineParkFindPanel_cpp 0x101ea5d8, UpdateParkFindPanelProgress 0x101ea4e4, RemoveOnlineParkFindPanel 0x101ea468

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (GetParent, GetWindow,
GetData, ChangeItem, UI_SendMessage, CaptureKb, the default window handler, the online system's calls, the screens' virtual calls, the
function pointer globals, swprintf, the string constructor and __register_global_object) are fakes on both sides that record their
arguments (pointers as labels, texts as a hash of their 16-bit units) and answer from the case.

The world of a case: the lobby (window, state), the screen system (three screens), the root window, the online system pointer, the find
panel, the info panel pointer, the park block's id, the two function pointer globals, what GetParent answers (NULL, a block whose first
byte is nonzero or zero, the lobby window, the root window) and the queues of the fakes. What is compared: the answer, the event log, the
pointer globals (as set / not set), the lobby state and the start of the conversion buffer.

Not fused operations. Not tested: the guard byte 0 of GeneralParkDownloadCompleteCallback (the host's function-local static cannot be put
back to "not constructed" between cases; the host object is made before every case, so only the guard nonzero path is compared), a NULL
post download function (the original calls through it unchecked), and the first column of the item UpdateParkFindPanelProgress passes (a
stack word the original does not set).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OnlineParkInfoPanelMsg"
MASK = 0xFFFFFFFF

FN = dict(msg=0x101EA7F0, general=0x101EA734, visit=0x101EA684, sinit=0x101EA5D8, update=0x101EA4E4, removefind=0x101EA468)
SIZES = {0x101EA7F0: 896, 0x101EA734: 124, 0x101EA684: 116, 0x101EA5D8: 116, 0x101EA4E4: 192, 0x101EA468: 76}
OWN = set(SIZES)

# ---- the callees faked on both sides
GETPARENT, CAPTUREKB, DECIDEKB, SENDMSG, REMOVEINFO = 0x101DBAFC, 0x101DCCEC, 0x1018116C, 0x101C9518, 0x101EBA64
OPNEW, SYSCTOR, REQFTP, UNPUBLISH, PTRGLUE, CALLDEFAULT = 0x102384B8, 0x1016F7D0, 0x10175DFC, 0x10174CA0, 0x1023BAA8, 0x101DE318
GTCTOR, SETGT, CLEARALL, CHANGEITEM, GETDATA, GETWINDOW = 0x1016F5D8, 0x1016F6A0, 0x100A7128, 0x101D3660, 0x101D41F0, 0x101DBE40
STRCTOR, REGISTER, SWPRINTF = 0x10238800, 0x10239940, 0x10239160
DESTROY_WSTRING = 0x103BCBB8
VISIT_TVECTOR = 0x10268598

ADDR = dict(gLobby=0x102E4888, gLobbyScreens=0x102E49B8, gOnlineSystem=0x10380AB0, gUIRootWindow=0x10395C24, find=0x103960C8,
            info=0x103961E4, ctx=0x1039610C, fnpost=0x10396110, fnclose=0x10396114, park=0x10396118, strA=0x103960CC,
            nodeA=0x103960E0, strB=0x103960EC, nodeB=0x10396100, buf=0x10380C48, gametype=0x102B3AA0, guard=0x102B3AA4,
            pool=0x1024F01E)
MAC_OFF = dict(lw=4, ls=0x14, on=8, dl=0xC, adv=0x14, pid=8)

# the blocks of a case and their labels
BLOCKS = [("win", 1), ("parentp", 2), ("lobbywin", 3), ("rootwin", 4), ("lobby", 5), ("screens", 6), ("scr_online", 7), ("scr_dl", 8),
          ("scr_adv", 9), ("list", 10), ("sys", 11), ("findwin", 12)]
GLOBAL_LABELS = [("park", 13), ("gametype", 14), ("strA", 15), ("nodeA", 16), ("strB", 17), ("nodeB", 18)]

CUR = {}


def s32(v):
    return T.s32(v)


def wide_hash(e, addr):
    """The hash of a wide string at a Mac address (the same code as Hash16 in the unit library)."""
    if addr == 0:
        return 0
    h = 0x811C9DC5
    for i in range(0x4000):
        u = struct.unpack(">H", e.read(addr + 2 * i, 2))[0]
        if u == 0:
            break
        h = ((h ^ u) * 16777619) & MASK
    return h


def glue(fn):
    """A fake of a glue import or __ptr_glue: the real glue saves the caller's TOC at 0x14(r1), which the code reloads after the call."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        return fn(e)
    return wrapped


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_getparent(e):
    L = e.cl
    L.rec("GetParent", (L.lab(e.r(3)),))
    e.set_r(3, CUR["parent"])


def stub_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, CUR["list"])


def stub_getdata(e):
    L = e.cl
    L.rec("GetData", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">i", s32(L.ret("GetData"))))
    e.set_r(3, 0)


def stub_changeitem(e):
    L = e.cl
    text = e.u32(e.r(5) + 4)
    L.rec("ChangeItem", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8)), 1 if text == ADDR["buf"] else 0,
                         wide_hash(e, text)))


def stub_swprintf(e):
    L = e.cl
    buf = e.r(3)
    value = s32(e.r(6))
    L.rec("swprintf", (1 if buf == ADDR["buf"] else 0, e.r(4) & MASK, wide_hash(e, e.r(5)), value))
    text = str(value)
    e.write(buf, text.encode("utf-16-be") + b"\0\0")
    e.set_r(3, len(text))


def stub_opnew(e):
    L = e.cl
    L.rec("operator_new", (e.r(3) & MASK,))
    e.set_r(3, e.alloc(e.r(3)))


def stub_reqftp(e):
    L = e.cl
    L.rec("RequestFTPDownload", (L.lab(e.r(3)), 1 if e.r(4) == ADDR["park"] else 0, 1 if e.r(5) == VISIT_TVECTOR else 0, e.r(6) & MASK))


def stub_strctor(e):
    L = e.cl
    L.rec("TbDynamicString", (L.lab(e.r(3)), wide_hash(e, e.r(4))))


def stub_register(e):
    L = e.cl
    L.rec("register", (L.lab(e.r(3)), 1 if e.r(4) == DESTROY_WSTRING else 0, L.lab(e.r(5))))


def stub_ptrglue(e):
    L = e.cl
    name = CUR["desc"].get(e.r(12))
    if name == "ScreenIsVisible":
        L.rec(name, (L.lab(e.r(3)),))
        e.set_r(3, L.ret(name) & MASK)
    elif name in ("OnlineIslandsSlot30", "DownloadGlobeSlot4c"):
        L.rec(name, (L.lab(e.r(3)),))
    elif name == "InfoClose":
        L.rec(name, ())
    elif name == "PostDownload":
        L.rec(name, (s32(e.r(3)), e.r(4) & MASK))
    else:
        raise AssertionError("ptr_glue through an unknown descriptor %x" % e.r(12))


STUBS.internal(GETPARENT, stub_getparent)
STUBS.internal(CAPTUREKB, CL.recorder("CaptureKb", "Li"))
STUBS.internal(DECIDEKB, CL.recorder("UIMain_DecideKbCapture", ""))
STUBS.internal(SENDMSG, CL.recorder("UI_SendMessage", "Liii"))
STUBS.internal(REMOVEINFO, CL.recorder("RemoveOnlineParkInfoPanel", ""))
STUBS.internal(OPNEW, glue(stub_opnew))
STUBS.internal(SYSCTOR, CL.recorder("CFSOnlineSystem", "N"))
STUBS.internal(REQFTP, stub_reqftp)
STUBS.internal(UNPUBLISH, CL.recorder("UnpublishPark", "Li"))
STUBS.internal(PTRGLUE, glue(stub_ptrglue))
STUBS.internal(CALLDEFAULT, CL.recorder("CallDefaultWindowHandler", "Liii", ret="i"))
STUBS.internal(GTCTOR, CL.recorder("CGameType", "L"))
STUBS.internal(SETGT, CL.recorder("SetGameType", "Li"))
STUBS.internal(CLEARALL, CL.recorder("ClearAll", "L"))
STUBS.internal(CHANGEITEM, stub_changeitem)
STUBS.internal(GETDATA, stub_getdata)
STUBS.internal(GETWINDOW, stub_getwindow)
STUBS.internal(STRCTOR, glue(stub_strctor))
STUBS.internal(REGISTER, glue(stub_register))
STUBS.internal(SWPRINTF, glue(stub_swprintf))


def setup(runner):
    T.install_std_stubs(runner.emu)
    e = runner.emu
    for off in (0xA, 0x4D, 0x5B):
        a = ADDR["pool"] + off
        text = b""
        while e.read(a + len(text), 2) != b"\0\0" and len(text) < 200:
            text += e.read(a + len(text), 2)
        print("pool +0x%x: %r" % (off, text.decode("utf-16-be")))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def ptr(self, a, v):
        """A pointer field: a word on the Mac, a native 8 byte pointer on the host."""
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def text(self, a, n):
        raw = self.read(a, 2 * n)
        out = []
        for i in range(n):
            u = struct.unpack(self.enc, raw[2 * i:2 * i + 2])[0]
            if u == 0:
                break
            out.append(u)
        return out


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    enc = ">H"
    off = MAC_OFF

    def __init__(self, e, case):
        self.e = e
        self.o = {n: e.alloc(0x100) for n, _ in BLOCKS}
        self.v = {n: e.alloc(0x100) for n in ("vt_online", "vt_dl", "d_vis", "d_s30", "d_s4c", "d_close", "d_post")}
        fixed = {self.o[n]: lab for n, lab in BLOCKS}
        fixed.update({ADDR[n]: lab for n, lab in GLOBAL_LABELS})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def setup_calls(self, case, parent):
        v = self.v
        self.words(self.o["scr_online"], [v["vt_online"]])   # (PowerPC only)
        self.words(v["vt_online"] + 0x2C, [v["d_vis"]])
        self.words(v["vt_online"] + 0x30, [v["d_s30"]])
        self.words(self.o["scr_dl"], [v["vt_dl"]])
        self.words(v["vt_dl"] + 0x4C, [v["d_s4c"]])
        CUR["desc"] = {v["d_vis"]: "ScreenIsVisible", v["d_s30"]: "OnlineIslandsSlot30", v["d_s4c"]: "DownloadGlobeSlot4c",
                       v["d_close"]: "InfoClose", v["d_post"]: "PostDownload"}
        CUR["parent"] = parent
        CUR["list"] = self.o["list"]
        self.words(ADDR["fnclose"], [v["d_close"] if case["close"] else 0])
        self.words(ADDR["fnpost"], [v["d_post"] if case["post"] else 0])
        self.write(ADDR["guard"], bytes([case["guard"]]))

    def call(self, op, case):
        args = {"msg": lambda: [self.o["win"], case["msg"] & MASK, case["a"] & MASK, case["b"] & MASK],
                "general": lambda: [case["error"] & MASK, case["ctxarg"]],
                "visit": lambda: [case["error"] & MASK, case["ctxarg"]],
                "sinit": lambda: [], "update": lambda: [case["percent"] & MASK], "removefind": lambda: []}[op]()
        r3, _, _ = T.call_ppc(self.e, FN[op], args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    enc = "<H"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off = lib.off
        self.o = {n: lib.tpw_opi_alloc(0x100) for n, _ in BLOCKS}
        fixed = {lab: self.o[n] for n, lab in BLOCKS}
        fixed.update({lab: self.g(n) for n, lab in GLOBAL_LABELS})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_opi_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def setup_calls(self, case, parent):
        self.lib.tpw_opi_set(0, parent)
        self.lib.tpw_opi_set(1, self.o["list"])
        self.lib.tpw_opi_setfn(1 if case["close"] else 0, 1 if case["post"] else 0)

    def call(self, op, case):
        lib = self.lib
        if op == "msg":
            return lib.tpw_opi_msg(self.o["win"], case["msg"], case["a"], case["b"]) & MASK
        if op == "general":
            lib.tpw_opi_general(case["error"], case["ctxarg"])
        elif op == "visit":
            lib.tpw_opi_visit(case["error"], case["ctxarg"])
        elif op == "sinit":
            lib.tpw_opi_sinit()
        elif op == "update":
            lib.tpw_opi_update(case["percent"])
        else:
            lib.tpw_opi_removefind()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_opi_addr.restype, lib.tpw_opi_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_opi_alloc.restype, lib.tpw_opi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_opi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_opi_set.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_opi_setfn.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_opi_msg.restype, lib.tpw_opi_msg.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_opi_general.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_opi_visit.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_opi_update.argtypes = [C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_opi_layout(out)
    v = list(out)[:n]
    lib.off = dict(lw=v[0], ls=v[1], on=v[2], dl=v[3], adv=v[4], pid=v[5])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o, off = s.o, s.off
    parent = {0: 0, 1: o["parentp"], 2: o["parentp"], 3: o["lobbywin"], 4: o["rootwin"]}[case["parent"]]
    root = {"rootwin": o["rootwin"], "lobbywin": o["lobbywin"], "null": 0, "parentblock": o["parentp"]}[case["root"]]
    s.ptr(s.g("gLobby"), o["lobby"])
    s.ptr(o["lobby"] + off["lw"], o["lobbywin"])
    s.words(o["lobby"] + off["ls"], [case["state"]])
    s.ptr(s.g("gLobbyScreens"), o["screens"])
    s.ptr(o["screens"] + off["on"], o["scr_online"])
    s.ptr(o["screens"] + off["dl"], o["scr_dl"])
    s.ptr(o["screens"] + off["adv"], o["scr_adv"])
    s.ptr(s.g("gUIRootWindow"), root)
    s.ptr(s.g("gOnlineSystem"), o["sys"] if case["sys"] else 0)
    s.ptr(s.g("find"), o["findwin"] if case["findset"] else 0)
    s.ptr(s.g("info"), o["win"])
    s.ptr(s.g("ctx"), case["ctx"])
    s.words(s.g("park") + off["pid"], [case["parkid"]])
    s.write(o["parentp"], bytes([case["pbyte"] if case["parent"] == 1 else 0]))
    s.write(s.g("buf"), b"\0" * 128)
    s.setup_calls(case, parent)


def observe(s, case, answer):
    o, off = s.o, s.off
    return {"answer": answer, "log": s.log(),
            "sys": int(s.rd_ptr(s.g("gOnlineSystem")) != 0), "info": int(s.rd_ptr(s.g("info")) != 0),
            "find": int(s.rd_ptr(s.g("find")) != 0), "state": s.rd_word(o["lobby"] + off["ls"]),
            "buf": s.text(s.g("buf"), 32)}


def run(s, case):
    try:
        build(s, case)
        answer = s.call(case["op"], case)
        return observe(s, case, answer if case["op"] == "msg" else None)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def world(r, op, **kw):
    w = dict(op=op, parent=r.choice([0, 1, 2, 3, 4]), pbyte=r.choice([1, 0x10, 0x80, 0xFF]),
             root=r.choice(["rootwin", "lobbywin", "null", "parentblock"]), sys=r.choice([0, 1]), close=r.choice([0, 1]), post=1,
             findset=r.choice([0, 1, 1]), state=r.randrange(0, 4), parkid=r.choice([r.randrange(-3, 200000), 0, -1]),
             ctx=r.choice([0, 5, 0x12345678]), guard=r.choice([1, 0x80, 0xFF, 0x7F]), msg=0, a=0, b=0, error=0, ctxarg=0, percent=0,
             queues={"GetData": [r.choice([-1, -1, 0, 5, 0x7FFFFFFF, -2])], "ScreenIsVisible": [r.choice([0, 1, 2, -1])],
                     "CallDefaultWindowHandler": [r.randrange(-5, 1000)]})
    w.update(kw)
    return w


MSG_PAIRS = [(0x100, 0xE592E), (0x100, 0xE592F), (0x100, 0xE592D), (0x100, 0xE5930), (0x100, 0xE5931), (0x100, -2), (0x100, -1),
             (0x100, 0), (0x100, 0x1B), (0x14, 0), (0x14, 7), (0x13, 0), (0x15, 0), (0x16, 0), (0x0, 0), (0x1, 0), (0xFF, -2),
             (0x101, -2), (0x1000B, 0x1B), (0x1000B, 0x1A), (0x1000B, -2), (0x1000A, 0x1B), (0x1000C, 0x1B), (0x10000, 0x1B),
             (0x7FFFFFFF, 0), (-1, 0), (0x400, 0xE592E)]


def msg_case(r, msg, a):
    return world(r, "msg", msg=msg, a=a, b=r.choice([0, -1, r.randrange(-1000, 1000)]))


def msg_edges():
    r = random.Random(5501)
    for msg, a in MSG_PAIRS:
        for parent in range(5):
            yield msg_case(r, msg, a) | {"parent": parent}
        for _ in range(3):
            yield msg_case(r, msg, a)


def msg_random(r):
    if r.random() < 0.8:
        msg, a = r.choice(MSG_PAIRS)
    else:
        msg, a = r.choice([0x100, 0x14, 0x1000B, r.randrange(-5, 0x20)]), r.choice([-2, 0xE592E, 0xE592F, 0x1B, r.randrange(-5, 0x20)])
    return msg_case(r, msg, a)


def general_edges():
    r = random.Random(5502)
    for error in (0, 1, -1, 5, 0x7FFFFFFF, -0x80000000):
        for guard in (1, 0x80, 0xFF):
            yield world(r, "general", error=error, guard=guard, ctxarg=r.choice([0, 7]))


def general_random(r):
    return world(r, "general", error=r.choice([0, 0, 1, -1, r.randrange(-100, 100)]), ctxarg=r.choice([0, 7]))


def visit_edges():
    r = random.Random(5503)
    for error in (0, 1, -1, 5, 0x7FFFFFFF, -0x80000000):
        for ctx in (0, 5, 0x12345678):
            yield world(r, "visit", error=error, ctx=ctx, ctxarg=r.choice([0, 7]))


def visit_random(r):
    return world(r, "visit", error=r.choice([0, 0, 1, -1, r.randrange(-100, 100)]), ctxarg=r.choice([0, 7]))


def sinit_edges():
    r = random.Random(5504)
    yield world(r, "sinit")
    yield world(r, "sinit")


def sinit_random(r):
    return world(r, "sinit")


def update_edges():
    r = random.Random(5505)
    for findset in (0, 1):
        for data in (-1, 0, -2, 5, 0x7FFFFFFF):
            for percent in (0, 1, -1, 50, 100, 12345, 0x7FFFFFFF, -0x80000000):
                w = world(r, "update", findset=findset, percent=percent)
                w["queues"]["GetData"] = [data]
                yield w


def update_random(r):
    return world(r, "update", percent=r.choice([r.randrange(-5, 101), r.randrange(-2 ** 31, 2 ** 31)]))


def removefind_edges():
    r = random.Random(5506)
    for findset in (0, 1):
        for _ in range(3):
            yield world(r, "removefind", findset=findset)


def removefind_random(r):
    return world(r, "removefind")


TESTS = [
    FnTest("OnlineParkInfoPanelMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    FnTest("GeneralParkDownloadCompleteCallback", ppc, host, edge=general_edges, rnd=general_random),
    FnTest("VisitParkPostDownloadCallbackFn", ppc, host, edge=visit_edges, rnd=visit_random),
    FnTest("__sinit_ui_onlineParkFindPanel_cpp", ppc, host, edge=sinit_edges, rnd=sinit_random),
    FnTest("UpdateParkFindPanelProgress", ppc, host, edge=update_edges, rnd=update_random),
    FnTest("RemoveOnlineParkFindPanel", ppc, host, edge=removefind_edges, rnd=removefind_random),
]
