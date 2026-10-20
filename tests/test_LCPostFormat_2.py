"""Unit v5096, the online outbox panel, against libtpw_unit_LCPostFormat_2.dylib (tests/unit_LCPostFormat_2.cpp), main app
("SimTheme Park"), src/LCPostFormat_2.cpp:
  LCFormat 0x101e4bdc (OutboxLCFormat), LCPostFormat 0x101e4d20 (OutboxLCPostFormat), CreateOnlineOutboxPanel 0x101e4f28

A case is a world and a call. The world is built twice: in emulator memory at the Mac's addresses for the original, and through the host
library's world function for the translation. Every callee outside the unit is a fake on both sides that records its arguments (windows
are made-up numbers that are never dereferenced; the objects the code makes are labelled by their constructor fakes; the TVectors of the
callbacks and the window description by fixed labels). The font's height comes from the original's own virtual call on the PowerPC side
(a TVector in the fake font object points at the real GetHeight) and from GetHeight on the host.

The world of a case: the font's height, the screen's height (not 0: divw by 0 is undefined), the user info (none or one with a number of
mails), the window global, and what the user info's constructor fake gives its mail list. What is compared: the answer (whether a window was
made, or the result), the window global, which user info the global names, the event log.

No fused operations. Not tested: a screen height of 0 (the divide by zero, undefined on the host).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LCPostFormat_2"
MASK = 0xFFFFFFFF

FN_FORMAT, FN_POST, FN_CREATE = 0x101E4BDC, 0x101E4D20, 0x101E4F28
SIZES = {FN_FORMAT: 292, FN_POST: 468, FN_CREATE: 952}
OWN = set(SIZES)

ADDR = dict(gUserInfo=0x10380B34, gOutboxWindow=0x10395E3C, gLobbyScreens=0x102E49B8, gUIRealScreenHeight=0x10395BBA)
TV = {"msg": 0x102684F0, "addr": 0x102684E8, "fmt": 0x102684E0, "post": 0x102684D8, "data": 0x102B05C4}
GET_HEIGHT = 0x10168FC0

FONT = {}          # filled by setup: the persistent fake font (and what hangs off it) in emulator memory

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def fake(addr, name, spec, ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret=ret))


def u(v):
    return v & MASK


fake(0x10180270, "GetFont", "u", ret=lambda e, L: FONT["font"])
fake(0x101D98D8, "WindowCtor", "Nuiihhhh")
fake(0x1018D11C, "TextSpriteCtor", "Nu")
fake(0x1018C978, "DrawSpriteCtor", "Nu")
fake(0x101DC750, "CreateObject", "Liiihhhh")
fake(0x101DB624, "SetClientDrawObject", "uL")
fake(0x101DA3AC, "SetFontColour", "uiiii")
fake(0x101DA118, "SetFont", "uL")
fake(0x101DE130, "SetClientDrawText", "uu")
fake(0x101DBAC4, "GetChild", "ui", ret=lambda e, L: u(e.r(3) + 0x100))
fake(0x101DBB38, "GetNext", "u", ret=lambda e, L: u(e.r(3) + 0x10))
fake(0x101DA204, "SetFontJust", "uii")
fake(0x101DA2D8, "SetFontWrap", "uii")
fake(0x101DB474, "SetDrawObject", "uu")
fake(0x101DBE40, "GetWindow", "ui", ret=lambda e, L: u(0x71000000 + T.s32(e.r(4))))
fake(0x101DCBA8, "SetCallbackFunction", "uL")
fake(0x101DCCEC, "CaptureKb", "ui")
fake(0x101D2864, "SetColumnFormatsAll", "uLL")
fake(0x101D2614, "SetColumnType", "uhi")
fake(0x101D1EB4, "SetSelectionObject", "uu")
fake(0x101F12C8, "Dialog", "LLi", ret=lambda e, L: 0x70000000)
fake(0x10180468, "UIMain_GetString", "i", ret=lambda e, L: u(0x74000000 + T.s32(e.r(3))))
fake(0x10180794, "UIMain_SetTitleText", "uiu")
fake(0x101818E4, "MAKECOLUMNBUTTON", "uuiiiii")
fake(0x101E378C, "FillOutboxList", "")
fake(0x100A7074, "AdvisorAdd", "uii")
fake(0x10238FF8, "AllocMemory", "")


def find_draw_object(e):
    L = e.cl
    s = e.cstr(e.r(3))
    b = s if isinstance(s, (bytes, bytearray)) else s.encode("latin-1")
    L.rec("FindDrawObject", (len(b) * 1000003 + sum(b),))
    e.set_r(3, u(0x72000000 + len(b)))


STUBS.internal(0x101CD2D4, find_draw_object)

_user_ctor = CL.recorder("UserCtor", "N")


def user_ctor(e):
    _user_ctor(e)
    e.write(e.r(3) + 0x8C, struct.pack(">I", e.cl.ret("ctorMail") & MASK))   # the mail list's size word


STUBS.internal(0x1017DE54, user_ctor)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    tv = T.persist_alloc(e, 8, struct.pack(">II", GET_HEIGHT, e.r(2) & MASK))
    vt = T.persist_alloc(e, 0x20, b"\0" * 0xC + struct.pack(">I", tv) + b"\0" * 0x10)
    font = T.persist_alloc(e, 0x40, b"\0" * 0x10 + struct.pack(">I", vt) + b"\0" * 0x2C)
    FONT["font"] = font


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {TV["msg"]: 1, TV["addr"]: 2, TV["fmt"]: 3, TV["post"]: 4, TV["data"]: 5, FONT["font"]: 6}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def world(self, c):
        e = self.e
        self.pre = 0
        if c["user"]:
            self.pre = e.alloc(0xD4)
            self.write(self.pre + 0x8C, struct.pack(">I", c["mail"]))
        self.words(ADDR["gUserInfo"], [self.pre])
        self.words(ADDR["gOutboxWindow"], [c["owin"]])
        self.write(ADDR["gUIRealScreenHeight"], struct.pack(">h", T.s16(c["screen"])))
        self.write(FONT["font"] + 6, struct.pack(">h", T.s16(c["fh"])))
        scr = e.alloc(0x20)
        self.words(scr + 0x14, [0x75000000])
        self.words(ADDR["gLobbyScreens"], [scr])

    def state(self):
        e = self.e
        w, ui = e.u32(ADDR["gOutboxWindow"]), e.u32(ADDR["gUserInfo"])
        return [w, 0 if ui == 0 else (1 if ui == self.pre else 2)]

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        g = lambda n: lib.tpw_lc_addr(n.encode())
        CL.host_begin(lib, {1: g("OutboxPanelMsg"), 2: g("AdressesTextPanelMsg"), 3: g("OutboxLCFormat"),
                            4: g("OutboxLCPostFormat"), 5: g("data"), 6: g("font")}, case["queues"])

    def done(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def world(self, c):
        self.lib.tpw_lc_world(c["user"], c["mail"], c["owin"], T.s16(c["fh"]), T.s16(c["screen"]))

    def state(self):
        out = (C.c_int64 * 4)()
        n = self.lib.tpw_lc_state(out)
        return [out[0] & MASK, out[1]][:n]

    def call(self, fn, args):
        if fn == FN_FORMAT:
            return self.lib.tpw_lc_format() & MASK
        if fn == FN_POST:
            self.lib.tpw_lc_post(args[0] & MASK)
            return 0
        return self.lib.tpw_lc_create() & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lc_addr.restype, lib.tpw_lc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lc_world.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.c_int32, C.c_int32]
    lib.tpw_lc_state.restype, lib.tpw_lc_state.argtypes = C.c_int32, [C.POINTER(C.c_int64)]
    lib.tpw_lc_format.restype = C.c_uint64
    lib.tpw_lc_post.argtypes = [C.c_uint32]
    lib.tpw_lc_create.restype = C.c_int32
    lib.bound = True


def run(s, case):
    try:
        s.world(case)
        fn = case["fn"]
        r = s.call(fn, case["args"])
        if fn == FN_FORMAT:
            r = 1 if r else 0
        elif fn == FN_POST:
            r = 0   # a void function: r3 is whatever the last call left
        return {"answer": r, "log": s.log(), "state": s.state()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def base(r, fn):
    return {"fn": fn, "args": [], "user": r.choice([0, 1]), "mail": r.choice([0, 1, 2, 5]), "owin": r.choice([0, 0x7E000000 + r.randrange(100)]),
            "fh": r.choice([0, 1, 12, 16, 24, 0x7FFF, 0x8000, 0xFFFF, r.randrange(65536)]),
            "screen": r.choice([1, 2, 480, 768, 1536, 0xFFFF, 0x7FFF, 0x8000, r.randrange(1, 4000)]),
            "queues": {"ctorMail": [r.choice([0, 1, 3])]}}


def format_case(r, fh=None, screen=None):
    c = base(r, FN_FORMAT)
    if fh is not None:
        c["fh"] = fh
    if screen is not None:
        c["screen"] = screen
    return c


def format_edges():
    r = random.Random(11)
    for fh, screen in ((16, 768), (0, 768), (0xFFFF, 768), (0x8000, 1), (0x7FFF, 1), (1, 0x7FFF), (24, 0xFFFF), (3, 0x8000), (100, 1536), (17, 1000)):
        yield format_case(r, fh, screen)


def format_random(r):
    return format_case(r)


def post_case(r, w=None):
    c = base(r, FN_POST)
    c["args"] = [r.choice([0x73000000, 0x10, 0xFFFF0000, 0x73000000 + r.randrange(1000)]) if w is None else w]
    return c


def post_edges():
    r = random.Random(12)
    for w in (0x73000000, 0x10, 0):
        yield post_case(r, w)


def post_random(r):
    return post_case(r)


def create_case(r, user=None, mail=None, ctor=None):
    c = base(r, FN_CREATE)
    if user is not None:
        c["user"] = user
    if mail is not None:
        c["mail"] = mail
    if ctor is not None:
        c["queues"]["ctorMail"] = [ctor]
    return c


def create_edges():
    r = random.Random(13)
    for user, mail, ctor in ((0, 0, 0), (0, 0, 1), (0, 0, 3), (1, 0, 0), (1, 1, 0), (1, 7, 0), (1, 0, 2)):
        yield create_case(r, user, mail, ctor)


def create_random(r):
    return create_case(r)


TESTS = [
    FnTest("LCFormat", ppc, host, edge=format_edges, rnd=format_random),
    FnTest("LCPostFormat", ppc, host, edge=post_edges, rnd=post_random),
    FnTest("CreateOnlineOutboxPanel", ppc, host, edge=create_edges, rnd=create_random),
]
