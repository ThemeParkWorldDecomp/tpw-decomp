"""Unit v5060, the online lobby's interface set-up and the bankrupt window's message function, against libtpw_unit_InitOnlineUI.dylib
(tests/unit_InitOnlineUI.cpp), main app ("SimTheme Park"), src/lobby_exit_InitOnlineUI.cpp:
  InitOnlineUI 0x101864b0, BankruptWindowMsg 0x101868c0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the message
flush, UpdateLights, the map's Link, the online panel, the window methods, the CoolRect constructor and destructor, the ticks, the
default handler, the key tables, the help text ...) is a fake on both sides that records its arguments and answers from the case.

The world of a case: the interface light's words 0x04..0x57, the tooltips and RMB-cancel settings bytes, the windows (root, bumhole, first
person, bankrupt), the UI clock and its stored tick, the key-table flag, the answers of the fakes (including the rectangle GetPosition
writes). What is compared: the answer, the light's words, the first person and bankrupt window globals, the clock global, the key-table
flag and the event log.

No fused operations. Not tested: a NULL root or bumhole window (the original calls through them unconditionally), a NULL answer of Build
(it is used unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_InitOnlineUI"
MASK = 0xFFFFFFFF

FN_INIT, FN_BK = 0x101864B0, 0x101868C0
OWN = {FN_INIT, FN_BK}

ADDR = dict(settings=0x1038071C, light=0x1038CC78, fp=0x103949E0, bump=0x103949DC, bank=0x1038CC58, ticks=0x10380B78,
            kt=0x1038CC54, root=0x10395C24, mapwho=0x103BC908)
TV_FIRSTPERSON, TV_BANKRUPT = 0x102676A0, 0x10267690
SCRIPT_FP, SCRIPT_BK, ANIM_BK = 0x102A76DA, 0x102A7756, 0x102A7914
# The host's CLight and CoolRect carry a wider (8 byte) pointer at 0, so their fields sit 4 bytes later than the Mac's (0x04 -> 8, 0x2c -> 48,
# 0x54 -> 88; CoolRect 8 -> 12, 0xa -> 14); the settings object has the Mac's offsets (0x36, 0x39).
HOST_LAYOUT = [8, 48, 88, 54, 57, 12, 14]

BLOCKS = ["root", "bump", "fp", "bank", "cret", "bret", "self", "base", "mw"]     # labels 1..9
CUR = {}


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cbflag(v):
    return 1 if v == TV_FIRSTPERSON else 2 if v == TV_BANKRUPT else 3 if v else 0


def simple(addr, name):
    def fn(e):
        e.cl.rec(name, ())
    STUBS.internal(addr, fn)


simple(0x101C9E48, "Flush")
STUBS.imp("UpdateLights__7CSystemFv", lambda e: e.cl.rec("UpdateLights", ()))
simple(0x101E8F08, "CreateOnlinePanel")
simple(0x101CAAAC, "EnableHelp")
simple(0x101CAA4C, "DisableHelp")
simple(0x10032584, "StopRiding")
simple(0x101A8AC0, "ToggleGameWindow")
simple(0x10151010, "EnableAllKeytables")
simple(0x101C7448, "CoolRect")
simple(0x101C74E0, "~CoolRect")


def st_link(e):
    L = e.cl
    L.rec("Link",(L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5))))
    e.set_r(3, L.ret("Link") & MASK)


def st_create(e):
    L = e.cl
    L.rec("CreateObject", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s16(e.r(7)), s16(e.r(8)), s16(e.r(9)), s16(e.r(10))))
    e.set_r(3, CUR["cret"] if L.ret("CreateObject") else 0)


def st_setcb(e):
    L = e.cl
    L.rec("SetCallback", (L.lab(e.r(3)), cbflag(e.r(4))))


def st_cursor(e):
    L = e.cl
    L.rec("setCursor", (L.lab(e.r(3)), s16(e.r(4))))


def st_build(e):
    L = e.cl
    script = 1 if e.r(4) == SCRIPT_FP else 2 if e.r(4) == SCRIPT_BK else 3 if e.r(4) else 0
    L.rec("Build", (L.lab(e.r(3)), script, cbflag(e.r(5))))
    e.set_r(3, CUR["bret"] if L.ret("Build") else 0)


def st_getpos(e):
    L = e.cl
    L.rec("GetPosition", (L.lab(e.r(3)), s16(e.r(5))))
    e.write(e.r(4) + 8, struct.pack(">hh", s16(L.ret("GPRight")), s16(L.ret("GPBottom"))))
    e.set_r(3, L.ret("GetPosition") & MASK)


def st_pivot(e):
    L = e.cl
    L.rec("setPivot", (L.lab(e.r(3)), s16(e.r(4)), s16(e.r(5))))


def st_anim(e):
    L = e.cl
    L.rec("startAnim", (L.lab(e.r(3)), 1 if e.r(4) == ANIM_BK else 2 if e.r(4) else 0, s32(e.r(5))))
    e.set_r(3, L.ret("startAnim") & MASK)


def st_default(e):
    L = e.cl
    L.rec("Default", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("Default") & MASK)


def st_ticks(e):
    e.set_r(3, e.cl.ret("Ticks") & MASK)


def st_send(e):
    L = e.cl
    L.rec("SendMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("SendMessage") & MASK)


STUBS.imp("Link__7CMapWhoFP12CMapLinkBasei", st_link)
STUBS.internal(0x101DC750, st_create)
STUBS.internal(0x101DCBA8, st_setcb)
STUBS.internal(0x101DD7F0, st_cursor)
STUBS.internal(0x101DEB78, st_build)
STUBS.internal(0x101DB884, st_getpos)
STUBS.internal(0x101DDD34, st_pivot)
STUBS.internal(0x101DD08C, st_anim)
STUBS.internal(0x101DE318, st_default)
STUBS.internal(0x101CAC3C, st_ticks)
STUBS.internal(0x101C9518, st_send)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wptr(self, a, v):
        """A pointer global: the Mac's word, the host's full 64-bit pointer."""
        self.write(a, struct.pack(self.pfmt, v))

    def rptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def name_of(self, v):
        for n, a in self.blocks.items():
            if a == v:
                return n
        return "other" if v else "null"


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    lo = 4

    def __init__(self, e, case):
        self.e = e
        self.blocks = {n: e.alloc(0x40) for n in BLOCKS}
        self.blocks["light"] = e.alloc(0x60)
        CUR.update(self.blocks)
        fixed = {self.blocks[n]: i + 1 for i, n in enumerate(BLOCKS)}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "mapwho":     # a data import: the TOC slot 0x102607e4 holds the harness's address for gei_MapWho
            return self.e.u32(0x102607E4)
        return ADDR[name]

    def light(self):
        return self.blocks["light"]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def init(self):
        with_ = T.call_ppc(self.e, FN_INIT, [])
        return 0

    def bk(self, msg, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_BK, [self.blocks["self"], msg & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"
    lo = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks = {n: lib.tpw_ou_alloc(0x40) for n in BLOCKS}
        lib.tpw_ou_setwin(0, self.blocks["cret"])
        lib.tpw_ou_setwin(1, self.blocks["bret"])
        self.blocks["base"] = lib.tpw_ou_addr(b"lightbase")
        fixed = {i + 1: self.blocks[n] for i, n in enumerate(BLOCKS)}
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ou_addr(name.encode())

    def light(self):
        return self.g("light")

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def init(self):
        self.lib.tpw_ou_InitOnlineUI()
        return 0

    def bk(self, msg, a, b):
        return self.lib.tpw_ou_BankruptWindowMsg(self.blocks["self"], s32(msg), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ou_addr.restype, lib.tpw_ou_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ou_alloc.restype, lib.tpw_ou_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ou_setwin.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_ou_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ou_BankruptWindowMsg.restype = C.c_uint32
    lib.tpw_ou_BankruptWindowMsg.argtypes = [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ou_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    b = s.blocks
    is_ppc = isinstance(s, Ppc)
    lt = s.light()
    s.words(lt + s.lo, case["light"])
    if is_ppc:
        s.words(lt, [b["base"]])
        s.words(s.g("light"), [lt])
    s.wptr(s.g("mapwho"), b["mw"])
    s.wptr(s.g("root"), b["root"])
    s.wptr(s.g("bump"), b["bump"])
    s.wptr(s.g("fp"), b["fp"] if case["fp"] else 0)
    s.wptr(s.g("bank"), b["bank"] if case["bank"] else 0)
    s.words(s.g("ticks"), [case["ticks"]])
    s.write(s.g("kt"), bytes([case["kt"]]))
    s.write(s.g("settings") + 0x34, bytes(case["settings"]))


def observe(s, case, answer):
    return {
        "answer": answer,
        "log": s.log(),
        "light": s.rd_words(s.light() + s.lo, 21),
        "fp": s.name_of(s.rptr(s.g("fp"))),
        "bank": s.name_of(s.rptr(s.g("bank"))),
        "ticks": s.rd_word(s.g("ticks")),
        "kt": s.read(s.g("kt"), 1),
    }


def run(s, case):
    try:
        build(s, case)
        if case["op"] == "init":
            ans = s.init()
        else:
            ans = s.bk(*case["args"])
        return observe(s, case, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


MSGS = [0x10006, 0x15, 0x14, 0x13, 0x16, 0x1000B, 0x10005, 0x10007, 0x1000A, 0x1000C, 0, 1, -1, 0x7FFFFFFF]
AS = [1, 0, 2, 0x1B, 0x1A, -1, 0x100000000 - 0x1B]
BS = [0, 0x10000, 0xFF0000, 0x1000000, 0x8000FFFF, 0xFFFF, 0x7F0000, 0x80000000, 0xFFFFFFFF]


def base(r, op):
    base_ticks = r.choice([0, 100, 0x7FFFFFF0, 0xFFFFF000, r.getrandbits(32)])
    delta = r.choice([1999, 2000, 2001, 2002, -1, 0, 5000, 0x7FFFFFFF, 0x80000000, 0x80000001, r.getrandbits(32)])
    return {
        "op": op,
        "light": [r.getrandbits(32) for _ in range(21)],
        "fp": r.choice([0, 1]),
        "bank": r.choice([0, 1, 1]),
        "ticks": base_ticks,
        "kt": r.choice([0, 1, 7]),
        "settings": [r.choice([0, 1]) for _ in range(7)],       # bools: always 0 or 1
        "queues": {
            "Ticks": [(base_ticks + delta) & MASK],
            "Link": [r.getrandbits(32)],
            "CreateObject": [r.choice([0, 1, 1])],
            "Build": [1],
            "GPRight": [r.choice([0, 1, -1, 640, 0x7FFF, -0x8000, r.randrange(-300, 300)]) & 0xFFFF],
            "GPBottom": [r.choice([0, 1, -1, 480, 0x7FFF, -0x8000, r.randrange(-300, 300)]) & 0xFFFF],
            "GetPosition": [r.getrandbits(32)],
            "startAnim": [r.getrandbits(32)],
            "Default": [r.getrandbits(32)],
            "SendMessage": [r.getrandbits(32)],
        },
    }


def init_case(r):
    return base(r, "init")


def init_edges():
    r = random.Random(60601)
    for tips in (0, 1):
        for co in (0, 1):
            c = init_case(r)
            c["settings"][2] = tips
            c["queues"]["CreateObject"] = [co]
            yield c


def init_random(r):
    return init_case(r)


def bk_case(r, msg=None, a=None, b=None):
    c = base(r, "bk")
    c["args"] = [r.choice(MSGS) if msg is None else msg, r.choice(AS) if a is None else a, r.choice(BS) if b is None else b]
    if r.random() < 0.3:
        c["args"][2] = r.getrandbits(32)
    return c


def bk_edges():
    r = random.Random(60602)
    for msg in MSGS:
        for a in AS:
            for b in (0, 0x10000, 0x1000000):
                yield bk_case(r, msg, a, b)
    for delta in (1999, 2000, 2001, -1, 0x7FFFFFFF, 0x80000000, 0x80000001):
        for bank in (0, 1):
            for rmb in (0, 1):
                c = bk_case(r, 0x10006, 1, 0)
                c["queues"]["Ticks"] = [(c["ticks"] + delta) & MASK]
                c["bank"] = bank
                c["settings"][5] = rmb
                yield c


def bk_random(r):
    c = bk_case(r)
    if r.random() < 0.5:
        c["settings"][5] = 1
        c["bank"] = 1
    return c


TESTS = [
    FnTest("InitOnlineUI", ppc, host, edge=init_edges, rnd=init_random),
    FnTest("BankruptWindowMsg", ppc, host, edge=bk_edges, rnd=bk_random),
]
