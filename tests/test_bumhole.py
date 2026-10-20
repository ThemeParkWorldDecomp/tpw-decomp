"""Unit v5143, the bumhole window's message function, ToggleGameWindow and SetOnOffText, against libtpw_unit_bumhole.dylib
(tests/unit_bumhole.cpp), main app ("SimTheme Park"), src/ui_mainpanel2_window_bumhole.cpp:
  bumhole 0x101a886c, ToggleGameWindow 0x101a8ac0, SetOnOffText 0x101a8e60

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit (the window and key table members,
the camera messages, CheckShifts, the main menu, the postcard, UIMain_GetString, the string functions wcsncpy and wcscat) are fakes on both
sides that record their arguments and answer from the case. The windows' virtual calls (the destructor at +0x10 and SetText at +0x2c of the
table behind +0x118 of the Mac object) go through __ptr_glue: the PowerPC stub tells them apart by the target and records "dtor" and
"SetText" (length and checksum of the text); on the host the fake windows are InterfaceWindow objects whose virtuals are fakes.

The world of a case: the three key tables, the world's state word (+0x1da738), the first person flags, the windows (game, bumhole, root),
the toggle flag, the string buffer. What is compared: the answer, those globals, the buffer and the event log.

No fused operations. Not tested: strings of 31 or more characters as the first string of SetOnOffText (the original copies 31 characters
without a terminator and then reads the uninitialised stack).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_bumhole"
MASK = 0xFFFFFFFF

FN_BUMHOLE, FN_TOGGLE, FN_ONOFF = 0x101A886C, 0x101A8AC0, 0x101A8E60
SIZES = {FN_BUMHOLE: 544, FN_TOGGLE: 308, FN_ONOFF: 304}
OWN = set(SIZES)

CALLEES = dict(
    CaptureDevice=0x101DCC10, UnCaptureDevice=0x101DCC6C, Camera=0x100331C4, CheckShifts=0x1022A3B4, KeyUp=0x1014FEF0,
    KeyDown=0x1014FDEC, Search=0x1014FD68, CreateMainMenu=0x101FC05C, UI_DoPostcard=0x101B232C, CallDefault=0x101DE318,
    CreateObject=0x101DC750, SetCallback=0x101DCBA8, CaptureKb=0x101DCCEC, Show=0x101DC1EC, DecideKb=0x1018116C,
    GetWindow=0x101DBE40, GetString=0x10180468, SetState=0x101CB1D0, ptr_glue=0x1023BAA8,
)
BUMHOLE_TVECTOR = 0x10267C30

ADDR = dict(camera=0x1037F22C, game=0x1037F230, shortcuts=0x1037F220, world=0x1037EBFC, bumhole=0x103949DC, gamewin=0x103949E4,
            uiroot=0x10395C24, flags=0x102B5A40, toggled=0x10394984, onoffroot=0x10394A14, buffer=0x10380C48)
HOST_LAYOUT = [0x1DA738]

NAMES = ["root", "game", "A", "B", "C", "kcamera", "kgame", "kshort"]
LABEL = {n: 20 + i for i, n in enumerate(NAMES)}
WINDOWS = ["root", "game", "A", "B", "C"]
TABLE = ["", "A", "B", "C"]          # the objects the fakes answer with by index (0 is NULL)
VT_DTOR, VT_SETTEXT = 0xD0000001, 0xD0000002

PPCOBJ = {}
BUFWORDS = 0x80


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


def gs_text(sid, n):
    out = []
    for i in range(n):
        x = sid * 7 + i * 13
        rem = x - 255 * int(x / 255)         # C's remainder (the sign of the dividend)
        out.append((rem + 1) & 0xFFFF)
    return out


def checksum(chars):
    h = 0
    for c in chars:
        h = (h * 31 + c) & 0x7FFFFFFF
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def wide(e, addr):
    out = []
    while len(out) < 0x400:
        c = struct.unpack(">H", e.read(addr + 2 * len(out), 2))[0]
        if c == 0:
            break
        out.append(c)
    return out


def put_wide(e, addr, chars):
    e.write(addr, b"".join(struct.pack(">H", c) for c in chars) + b"\0\0")


def tbl(e, i):
    return PPCOBJ[TABLE[i]] if i else 0


def mk(name, fn):
    STUBS.internal(CALLEES[name], fn)


def st_capture(e):
    e.cl.rec("CaptureDevice", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


def st_uncapture(e):
    e.cl.rec("UnCaptureDevice", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


def st_camera(e):
    e.cl.rec("Camera", (s32(e.r(3)), s32(e.r(4)), s32(e.r(5))))


def st_shifts(e):
    e.cl.rec("CheckShifts", ())
    e.set_r(3, e.cl.ret("CheckShifts") & MASK)


def st_key(name):
    def fn(e):
        e.cl.rec(name, (e.cl.lab(e.r(3)), s16(e.r(4)), s16(e.r(5))))
        e.set_r(3, 0)
    return fn


def st_search(e):
    e.cl.rec("Search", (e.cl.lab(e.r(3)), s16(e.r(4)), s16(e.r(5))))
    e.set_r(3, e.cl.ret("Search") & MASK)


def st_mainmenu(e):
    e.cl.rec("CreateMainMenu", (s32(e.r(3)),))


def st_postcard(e):
    e.cl.rec("UI_DoPostcard", ())


def st_default(e):
    e.cl.rec("CallDefault", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, e.cl.ret("CallDefault") & MASK)


def st_create(e):
    e.cl.rec("CreateObject", (e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s16(e.r(7)), s16(e.r(8)), s16(e.r(9)),
                              s16(e.r(10))))
    e.set_r(3, tbl(e, e.cl.ret("CreateObject") & 3))


def st_setcb(e):
    e.cl.rec("SetCallback", (e.cl.lab(e.r(3)), 1 if e.r(4) == BUMHOLE_TVECTOR else 0))


def st_capkb(e):
    e.cl.rec("CaptureKb", (e.cl.lab(e.r(3)), s32(e.r(4))))


def st_show(e):
    e.cl.rec("Show", (e.cl.lab(e.r(3)), s16(e.r(4))))
    e.set_r(3, 0)


def st_decide(e):
    e.cl.rec("DecideKb", ())


def st_getwindow(e):
    e.cl.rec("GetWindow", (e.cl.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, tbl(e, e.cl.ret("GetWindow") & 3))


def st_getstring(e):
    sid = s32(e.r(3))
    e.cl.rec("GetString", (sid,))
    n = s32(e.cl.ret("GetString") & MASK)
    if n < 0:
        e.set_r(3, 0)
        return
    put_wide(e, ADDR["buffer"], gs_text(sid, n))
    e.set_r(3, ADDR["buffer"])


def st_setstate(e):
    e.cl.rec("SetState", (e.cl.lab(e.r(3)), T.s8(e.r(4)), T.s8(e.r(5))))
    e.set_r(3, 0)


def st_glue(e):
    target = e.r(12)
    if target == VT_DTOR:
        e.cl.rec("dtor", (e.cl.lab(e.r(3)),))
    elif target == VT_SETTEXT:
        chars = wide(e, e.r(4))
        e.cl.rec("SetText", (e.cl.lab(e.r(3)), len(chars), checksum(chars)))
        e.set_r(3, 1)
    else:
        raise AssertionError("virtual call to %x" % target)


def imp_wcsncpy(e):
    dst, src, n = e.r(3), e.r(4), e.r(5)
    chars = wide(e, src)[:n]
    chars += [0] * (n - len(chars))
    e.write(dst, b"".join(struct.pack(">H", c) for c in chars))
    e.set_r(3, dst)


def imp_wcscat(e):
    dst, src = e.r(3), e.r(4)
    end = dst + 2 * len(wide(e, dst))
    put_wide(e, end, wide(e, src))
    e.set_r(3, dst)


for _n, _f in (("CaptureDevice", st_capture), ("UnCaptureDevice", st_uncapture), ("Camera", st_camera), ("CheckShifts", st_shifts),
               ("KeyUp", st_key("KeyUp")), ("KeyDown", st_key("KeyDown")), ("Search", st_search), ("CreateMainMenu", st_mainmenu),
               ("UI_DoPostcard", st_postcard), ("CallDefault", st_default), ("CreateObject", st_create), ("SetCallback", st_setcb),
               ("CaptureKb", st_capkb), ("Show", st_show), ("DecideKb", st_decide), ("GetWindow", st_getwindow),
               ("GetString", st_getstring), ("SetState", st_setstate), ("ptr_glue", st_glue)):
    mk(_n, _f)
STUBS.imp("wcsncpy", imp_wcsncpy)
STUBS.imp("wcscat", imp_wcscat)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    vt = T.persist_alloc(e, 0x40)
    e.write(vt + 0x10, struct.pack(">II", VT_DTOR, 0))
    e.write(vt + 0x2C, struct.pack(">I", VT_SETTEXT))
    for n in WINDOWS:
        a = T.persist_alloc(e, 0x200)
        e.write(a + 0x118, struct.pack(">I", vt))
        PPCOBJ[n] = a
    for n in ("kcamera", "kgame", "kshort"):
        PPCOBJ[n] = T.persist_alloc(e, 0x40)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def label(self, v):
        return 0 if v == 0 else self.rev.get(v, -1)

    def rd_ptr(self, a):
        return self.label(self.rd_ptr_raw(a))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.objs = dict(PPCOBJ)
        self.rev = {a: LABEL[n] for n, a in self.objs.items()}
        self.L = e.cl = CL.PpcLog({a: LABEL[n] for n, a in self.objs.items()}, case["queues"])
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

    def wptr(self, a, v):
        self.words(a, [v])

    def rd_ptr_raw(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def call(self, op, args):
        entry = {"bh": FN_BUMHOLE, "tg": FN_TOGGLE, "oo": FN_ONOFF}[op]
        if op == "bh":
            args = [self.objs["root"]] + list(args[1:])
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK if op == "bh" else 0      # the other two answer nothing


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.objs = {}
        for n in WINDOWS:
            self.objs[n] = lib.tpw_bh_newwin()
        for n in ("kcamera", "kgame", "kshort"):
            self.objs[n] = lib.tpw_bh_alloc(0x40)
        for i in range(1, 4):
            lib.tpw_bh_setobj(i, self.objs[TABLE[i]])
        self.rev = {a: LABEL[n] for n, a in self.objs.items()}
        CL.host_begin(lib, {LABEL[n]: a for n, a in self.objs.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_bh_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_bh_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr_raw(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args):
        if op == "bh":
            return self.lib.tpw_bh_bumhole(self.objs["root"], *[s32(a) for a in args[1:]]) & MASK
        if op == "tg":
            self.lib.tpw_bh_toggle()
            return 0
        self.lib.tpw_bh_onoff(*[s32(a) for a in args])
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_bh_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "newwin").restype, getattr(lib, p + "newwin").argtypes = C.c_uint64, []
    getattr(lib, p + "setobj").restype, getattr(lib, p + "setobj").argtypes = None, [C.c_int32, C.c_uint64]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "bumhole").restype = C.c_int32
    getattr(lib, p + "bumhole").argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    getattr(lib, p + "toggle").restype, getattr(lib, p + "toggle").argtypes = None, []
    getattr(lib, p + "onoff").restype = None
    getattr(lib, p + "onoff").argtypes = [C.c_int32] * 6
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    blk = s.alloc(0x100)
    s.wptr(s.g("world"), blk - 0x1DA700)
    s.words(blk + 0x38, [c["world"]])
    s.wptr(s.g("camera"), s.objs["kcamera"])
    s.wptr(s.g("game"), s.objs["kgame"])
    s.wptr(s.g("shortcuts"), s.objs["kshort"])
    s.wptr(s.g("gamewin"), s.objs["game"] if c["game"] else 0)
    s.wptr(s.g("bumhole"), s.objs["A"] if c["bum"] else 0)
    s.wptr(s.g("uiroot"), s.objs["root"])
    s.wptr(s.g("onoffroot"), s.objs["root"])
    s.words(s.g("flags"), [c["flags"]])
    s.words(s.g("toggled"), [c["toggled"]])
    s.write(s.g("buffer"), struct.pack(s.fmt[0] + "%dH" % len(c["halves"]), *c["halves"]))
    s.win = s.objs["root"]


def execute(s, c):
    return s.call(c["op"], c["args"])


def observe(s, c, answer):
    return {"answer": answer, "log": s.log(), "bumhole": s.rd_ptr(s.g("bumhole")), "toggled": s.rd_words(s.g("toggled"), 1),
            "buffer": list(struct.unpack(s.fmt[0] + "%dH" % (2 * BUFWORDS), s.read(s.g("buffer"), 4 * BUFWORDS))), "flags": s.rd_words(s.g("flags"), 1)}


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, execute(s, c))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

MSGS = [0x10003, 0x10004, 0x10005, 0x10006, 0x10009, 0x1000A, 0x1000B, 0x1000C, 0, -1, 0x7FFFFFFF]
AVALS = [1, 0, 0x1B, -1, 0x10, 0x11, 0x7FFF, 0x8000, 0x12345, 2]
BVALS = [0, 0x1B0000, 0x10000, 0xFF0000, 0x1FF0000, 0x7FFFFFFF, -1, 0x3C0000]


def base(r, op):
    return {
        "op": op, "world": r.choice([4, 4, 0, 1, 2]), "flags": r.choice([0, 0x16, 2, 4, 0x10, 1, 0x20, 0xFFFFFFFF]),
        "toggled": r.choice([0, 1, 1, 0, 2]), "game": r.random() < 0.8, "bum": r.random() < 0.5,
        "halves": [r.randrange(1, 0x10000) for _ in range(2 * BUFWORDS)],
        "queues": {
            "Search": [r.choice([4, 0xF, 0, 7, 0x10004, 0x1000F, -1]), r.choice([4, 0xF, 0, 7, 0x1000F, 0x20000F, -1])],
            "CheckShifts": [r.choice([0, 1, 2, 0x8001, 0x12345, -1, 0x7FFF])],
            "CallDefault": [r.choice([0, 1, -1, 12345])],
            "CreateObject": [r.choice([0, 1, 2, 3])],
            "GetWindow": [r.choice([0, 1, 2, 3])],
            "GetString": [r.randrange(0, 31)],
        },
    }


def bh_case(r, m=None, a=None, b=None):
    c = base(r, "bh")
    m = r.choice(MSGS) if m is None else m
    a = r.choice(AVALS) if a is None else a
    b = (r.choice(BVALS) if r.random() < 0.7 else r.randrange(-2**31, 2**31)) if b is None else b
    c["args"] = [0, m, a, b]
    return c


def bh_edges():
    r = random.Random(11)
    for m in MSGS:
        for a in AVALS:
            for b in BVALS[:3]:
                yield bh_case(r, m, a, b)
    for _ in range(60):
        c = bh_case(r, 0x1000B, r.choice([0x1B, 0x10, 0]), r.choice([0, 0x10000, 0x1B0000]))
        yield c


def tg_case(r):
    c = base(r, "tg")
    c["args"] = []
    return c


def tg_edges():
    r = random.Random(12)
    for game in (False, True):
        for toggled in (0, 1, 2, -1):
            for flags in (0, 0x16, 2, 4, 0x10, 1, 0x20):
                for bum in (False, True):
                    for obj in (0, 1):
                        c = tg_case(r)
                        c.update(game=game, toggled=toggled & MASK, flags=flags, bum=bum)
                        c["queues"]["CreateObject"] = [obj]
                        yield c


def oo_case(r, window=None, text=None, f=None, s=None, b=None):
    c = base(r, "oo")
    c["args"] = [r.choice([0, 1, 5, 0x10000, -1]) if window is None else window, r.choice([0, 1, 0x20, 0x14C, 77, -3]) if text is None else text,
                 r.randrange(2) if f is None else f, r.randrange(2) if s is None else s, r.randrange(2) if b is None else b,
                 r.choice([0, 3, -1, 99]) if True else 0]
    c["queues"]["GetWindow"] = [r.choice([0, 1, 2, 3]), r.choice([1, 2, 3]), r.choice([0, 1, 2, 3])]
    c["queues"]["GetString"] = [r.choice([0, 1, 29, 30, r.randrange(0, 31)]), r.choice([0, 1, 50, 100, r.randrange(0, 101)])]
    return c


def oo_edges():
    r = random.Random(13)
    for f in (0, 1):
        for s in (0, 1):
            for b in (0, 1):
                for _ in range(3):
                    yield oo_case(r, None, None, f, s, b)
    for n1 in (0, 30, 29):
        for n2 in (0, 100):
            c = oo_case(r)
            c["queues"]["GetString"] = [n1, n2]
            yield c


TESTS = [
    FnTest("bumhole", ppc, host, edge=bh_edges, rnd=bh_case),
    FnTest("ToggleGameWindow", ppc, host, edge=tg_edges, rnd=tg_case),
    FnTest("SetOnOffText", ppc, host, edge=oo_edges, rnd=oo_case),
]
