"""Unit v5522, the game and lobby menus' handlers, against libtpw_unit_MainGameMenuConfirmPublish_OK.dylib
(tests/unit_MainGameMenuConfirmPublish_OK.cpp), main app ("SimTheme Park"), src/game/ui/ui_mainmenu.cpp:
  MainGameMenuConfirmPublish_OK 0x101fabd8, MainGameMenuCB 0x101fae30, MainLobbyMenuConfirmLogout_OK 0x101fb258,
  MainLobbyMenuCB 0x101fb354

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the panels, the dialog, the online system, the key table, the save, the lobby screens' virtual calls) are fakes on both sides that
record their arguments and answer from the case's queues. The jump tables at 0x102b14a0 and 0x102b1508 are read from the original's image.

The world of a case: the online system and user info (present or not), the lobby return flag, the key table, the world, the screen system
with five tagged screens (the virtual calls go through a fake vtable whose slots are markers), the theme name. What is compared: the answer,
the return flag, whether the online system and user info were kept or made, and the event log.

No fused operations. Not tested: nothing is left out; the Mac's stack temporaries (the string and the save header) are not read by any fake
except through the marker word the string fakes pass. The publish globe's virtual slot 0x34 is a fake that records the screen.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_MainGameMenuConfirmPublish_OK"
MASK = 0xFFFFFFFF

FN_PUB, FN_GCB, FN_LOUT, FN_LCB = 0x101FABD8, 0x101FAE30, 0x101FB258, 0x101FB354
SIZES = {FN_PUB: 316, FN_GCB: 904, FN_LOUT: 128, FN_LCB: 976}
OWN = set(SIZES)

ADDR = dict(sys=0x10380AB0, userinfo=0x10380B34, retflag=0x102E4860, screens=0x102E49B8, keytable=0x1037F220, world=0x1037EBFC,
            upload=0x102E4868, gsave=0x1037FEFC, ggame=0x1037F134)
TVEC = {0x10268638: 1, 0x10268630: 2, 0x10268640: 3, 0x10268648: 4, 0x10268620: 5, 0x10268618: 6, 0x10268610: 7}
MAC_SCREEN = dict(themeIslands=0x04, onlineIslands=0x08, downloadGlobe=0x0C, publishGlobe=0x10, advisor=0x14)
TAGS = dict(themeIslands=1, onlineIslands=2, downloadGlobe=3, publishGlobe=4, advisor=5)
HOST_LAYOUT_COUNT = 6


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk(name, spec, answer=False):
    """A recording fake: spec codes per argument register: L label, i s32, h s16."""
    def fn(e):
        L = e.cl
        vals = []
        for k, c in enumerate(spec):
            r = e.r(3 + k)
            vals.append(L.lab(r) if c == "L" else s32(r) if c == "i" else T.s16(r))
        L.rec(name, tuple(vals))
        if answer:
            e.set_r(3, L.ret(name) & MASK)
    return fn


def hash_c(data):
    return sum(c * (i + 1) for i, c in enumerate(data)) & MASK


for addr, name, spec, ans in [
    (0x101FC140, "HideMainMenu", "", False), (0x101A37AC, "CreateLoadPanel", "i", False), (0x101A3994, "CreateSavePanel", "i", False),
    (0x101A9FB8, "CreateOptionsPanel", "", False), (0x10187220, "UI_ByeByeBankrupt", "", False), (0x101AE9E0, "CreateLoginPanel", "", False),
    (0x10236E6C, "GS_SetCurrentGameStatus", "i", False), (0x1018AB78, "UIMenu_DefaultCallback", "Liii", True),
    (0x10171498, "AreWeProcessing", "L", True), (0x10171398, "Logout", "L", False), (0x10171404, "StopProcessing", "L", False),
    (0x1017F61C, "GameLogout", "L", False), (0x1014FD68, "Search", "Lhh", True)]:
    STUBS.internal(addr, mk(name, spec, ans))
STUBS.internal(0x1016F7D0, CL.recorder("CFSOnlineSystem::ctor", "N"))
STUBS.internal(0x1017DE54, CL.recorder("CFSUserInfo::ctor", "N"))


def stub_get_string(e):
    L = e.cl
    i = e.r(3) & MASK
    L.rec("UIMain_GetString", (s32(i),))
    e.set_r(3, e.alloc(8, struct.pack(">HHH", 0x53, i & 0xFFFF, 0)))


def stub_dialog(e):
    L = e.cl
    p = e.r(3)
    t = e.read(e.u32(p), 4)
    text = (struct.unpack(">H", t[0:2])[0] << 16) | struct.unpack(">H", t[2:4])[0]
    rb = e.read(p + 4, 3)
    rgb = (rb[0] << 16) | (rb[1] << 8) | rb[2]
    cb = [TVEC.get(e.r(k) & MASK, 0 if (e.r(k) & MASK) == 0 else -9) for k in (5, 7, 9)]
    buttons = ((e.r(4) & 0xFF) << 16) | ((e.r(6) & 0xFF) << 8) | (e.r(8) & 0xFF)
    cbs = ((cb[0] & 0xFF) << 16) | ((cb[1] & 0xFF) << 8) | (cb[2] & 0xFF)
    L.rec("UI_DialogBox", (text, rgb, s32(e.u32(p + 8)), s32(e.u32(p + 12)), buttons, cbs, e.r(10) & 0xFF))
    e.set_r(3, 0)


def stub_login(e):
    cb = e.r(4) & MASK
    e.cl.rec("Login", (e.cl.lab(e.r(3)), TVEC.get(cb, 0 if cb == 0 else -9), s32(e.r(5))))


def stub_add(e):
    e.cl.rec("Add", (s32(e.u32((e.r(3) + 0x20) & MASK)), s32(e.r(4)), s32(e.r(5))))


def stub_return_upload(e):
    seed = e.cl.ret("UploadSeed")
    e.write(e.r(3), struct.pack(">I", (seed + 1) & MASK))
    e.cl.rec("ReturnUploadFilename", (e.cl.lab(e.r(4)),))


def stub_unistr(e):
    seed = e.cl.ret("UploadSeed") & MASK
    marker = e.u32(e.r(4))
    ptr = e.alloc(64, b"park%u.dat\0" % seed)
    e.write(e.r(3) + 8, struct.pack(">I", ptr))
    e.cl.rec("UniStringToString", (marker,))


def stub_wdtor(e):
    e.write((e.r(1) + 20) & MASK, struct.pack(">I", e.r(2) & MASK))   # the glue saves the TOC where the caller reloads it
    e.cl.rec("WideStringDtor", (e.u32(e.r(3)), T.s16(e.r(4))))


def stub_save(e):
    path = e.cstr(e.r(4))
    path = path if isinstance(path, bytes) else path.encode()
    e.cl.rec("Save", (e.cl.lab(e.r(3)), hash_c(path), s32(e.r(6))))
    e.set_r(3, 1)


def stub_convert(e):
    seed = e.cl.ret("ConvertDL") & MASK
    out = e.r(4)
    e.write(out, b"".join(struct.pack(">H", (seed * (i + 1) + i) & 0xFFFF) for i in range(256)))
    e.cl.rec("ConvertDL", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


def stub_theme(e):
    e.cl.rec("GetCurrentThemeName", (e.cl.lab(e.r(3)),))
    e.set_r(3, e.themeobj)


def stub_publishing(e):
    text = e.cstr(e.r(5))
    text = text if isinstance(text, bytes) else text.encode()
    words = struct.unpack(">256H", e.read(e.r(6), 512))
    h = sum(w * (i + 1) for i, w in enumerate(words)) & MASK
    e.cl.rec("SetPublishing", (e.cl.lab(e.r(3)), s32(e.r(4)), hash_c(text), h))


def stub_glue(e):
    L = e.cl
    slot = e.r(12) & 0xFF
    tag = e.u32((e.r(3) + 0x20) & MASK)
    name = {0x24: "Show", 0x28: "Hide", 0x2C: "IsVisible", 0x34: "Slot34"}[slot]
    L.rec(name, (s32(tag),))
    if slot == 0x2C:
        e.set_r(3, L.ret("IsVisible:%d" % tag) & MASK)


for addr, fn in [(0x101F1800, stub_dialog), (0x10180468, stub_get_string), (0x101710AC, stub_login), (0x100A7074, stub_add),
                 (0x10175608, stub_return_upload), (0x10005670, stub_unistr), (0x10238710, stub_wdtor), (0x10157868, stub_save),
                 (0x1014113C, stub_convert), (0x1014A058, stub_theme), (0x10174E4C, stub_publishing), (0x1023BAA8, stub_glue)]:
    STUBS.internal(addr, fn)


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


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["gsave"]: 1, ADDR["ggame"]: 2}, case["queues"])
        self.saved = STUBS.activate(e)
        self.soff = MAC_SCREEN

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

    def ptr(self, a, v):
        self.words(a, [v])

    def comp(self, tag):
        b = self.alloc(0x40)
        vt = self.alloc(0x60)
        self.words(vt, [0xF0000000 | (4 * i) for i in range(0x18)])
        self.words(b, [vt])
        self.words(b + 0x20, [tag])
        return b

    def set_theme(self, text):
        tb = self.alloc(16)
        tx = self.e.alloc(len(text) + 1, text + b"\0")
        self.words(tb + 8, [tx])
        self.e.themeobj = tb

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("gsave"), 2: self.g("ggame")}, case["queues"])
        self.soff = lib.soff

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_mm_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_mm_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def comp(self, tag):
        b = self.alloc(0x80)
        self.words(b + 0x20, [tag])
        return b

    def set_theme(self, text):
        self.write(self.g("themetext"), text + b"\0")

    def call(self, fn, args):
        lib = self.lib
        if fn == FN_PUB:
            return lib.tpw_mm_publish() & MASK
        if fn == FN_LOUT:
            return lib.tpw_mm_logout_ok() & MASK
        f = lib.tpw_mm_game_cb if fn == FN_GCB else lib.tpw_mm_lobby_cb
        return f(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_mm_addr.restype, lib.tpw_mm_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_mm_alloc.restype, lib.tpw_mm_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_mm_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_mm_publish.restype, lib.tpw_mm_publish.argtypes = C.c_uint32, []
    lib.tpw_mm_logout_ok.restype, lib.tpw_mm_logout_ok.argtypes = C.c_uint32, []
    for f in (lib.tpw_mm_game_cb, lib.tpw_mm_lobby_cb):
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_mm_layout(out)
    assert n == HOST_LAYOUT_COUNT, n
    lib.soff = dict(zip(["themeIslands", "onlineIslands", "downloadGlobe", "publishGlobe", "advisor"], list(out)[:5]))
    assert list(out)[5] == 8, list(out)[5]      # OnlineUploadFile::mPath at 0x08 on both sides
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    pre = s.pre = {}
    pre["sys"] = s.alloc(0x40) if case["sys"] else 0
    s.ptr(s.g("sys"), pre["sys"])
    pre["user"] = s.alloc(0x40) if case["user"] else 0
    s.ptr(s.g("userinfo"), pre["user"])
    s.words(s.g("retflag"), [case["retflag"]])
    s.ptr(s.g("keytable"), s.alloc(0x40))
    s.ptr(s.g("world"), s.alloc(0x40))
    s.words(s.g("upload"), [0, 0])
    s.ptr(s.g("upload") + 8, 0)
    sc = s.alloc(0x80)
    for name, tag in TAGS.items():
        s.ptr(sc + s.soff[name], s.comp(tag))
    s.ptr(s.g("screens"), sc)
    s.set_theme(case["theme"])


FNS = {"pub": FN_PUB, "gcb": FN_GCB, "lout": FN_LOUT, "lcb": FN_LCB}


def execute(s, case):
    return s.call(FNS[case["op"]], case["args"])


def kept(s, name, pre):
    w = s.rd_word(s.g(name))
    return 0 if w == 0 else (1 if w == pre & MASK else 2)


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "retflag": s.rd_word(s.g("retflag")), "sys": kept(s, "sys", s.pre["sys"]),
            "user": kept(s, "userinfo", s.pre["user"])}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def base(r, op, args):
    q = {"AreWeProcessing": [r.choice([0, 1])], "Search": [r.choice([0, 1, 0x10000, 0x20000, 0xFFFF, -65536, 0x1234])],
         "UIMenu_DefaultCallback": [r.randrange(-5, 100)], "ConvertDL": [r.randrange(0, 70000)], "UploadSeed": [r.randrange(0, 100000)]}
    for tag in range(1, 6):
        q["IsVisible:%d" % tag] = [r.choice([0, 1])]
    return {"op": op, "queues": q, "args": args, "sys": r.random() < 0.5, "user": r.random() < 0.5, "retflag": r.choice([0, 0, 5]),
            "theme": r.choice([b"fantasy", b"jungle", b"", b"space1"])}


WINDOW = 0x1234
MSGS = [0x100001, 0x1000B, 0x100002, 0x100000, 0x1000C, 0, -1, 0x7FFFFFFF, -0x80000000, 0x10000B]
AS = list(range(-1, 0x22)) + [0x41, 0x100, 0x7FFFFFFF, -0x80000000, 0xFFFF]
BS = [0, 0x10000, 0xFF0000, 0x1230000, 0xFFFFFFFF, 0x1FF0000, 0x5]


def cb_case(r, op, msg=None, a=None, b=None):
    msg = r.choice(MSGS) if msg is None else msg
    if a is None:
        a = r.choice(AS) if r.random() < 0.8 else r.randrange(-300, 300)
    b = r.choice(BS) if b is None else b
    return base(r, op, [WINDOW, msg & MASK, a & MASK, b & MASK])


def cb_edges(op, seed):
    def gen():
        r = random.Random(seed)
        for a in AS:
            for _ in range(2):
                yield cb_case(r, op, 0x100001, a)
        for msg in MSGS[2:]:
            for a in (0x1B, 0x41, 0, -1, 9, 0x13):
                yield cb_case(r, op, msg, a)
        for a in (0x1B, 0x41, 0, 0x20, -1):
            for b in BS:
                for _ in range(2):
                    yield cb_case(r, op, 0x1000B, a, b)
    return gen


def cb_random(op):
    def rnd(r):
        if r.random() < 0.5:
            return cb_case(r, op, 0x100001, r.randrange(-2, 0x24))
        return cb_case(r, op, None)
    return rnd


def simple_edges(op, seed):
    def gen():
        r = random.Random(seed)
        for _ in range(8):
            yield base(r, op, [0])
    return gen


def simple_random(op):
    return lambda r: base(r, op, [0])


TESTS = [
    FnTest("MainGameMenuConfirmPublish_OK", ppc, host, edge=simple_edges("pub", 11), rnd=simple_random("pub")),
    FnTest("MainGameMenuCB", ppc, host, edge=cb_edges("gcb", 12), rnd=cb_random("gcb")),
    FnTest("MainLobbyMenuConfirmLogout_OK", ppc, host, edge=simple_edges("lout", 13), rnd=simple_random("lout")),
    FnTest("MainLobbyMenuCB", ppc, host, edge=cb_edges("lcb", 14), rnd=cb_random("lcb")),
]
