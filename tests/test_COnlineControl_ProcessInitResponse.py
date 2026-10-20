"""Unit v5248, the chat client's answer to the init request, against libtpw_unit_COnlineControl_ProcessInitResponse.dylib
(tests/unit_COnlineControl_ProcessInitResponse.cpp), main app ("SimTheme Park"), src/COnlineControl_..._COnlineControl_ProcessInitResponse.cpp:
  COnlineControl::ProcessInitResponse 0x10214e80

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (UDebug, operator new, the system and control constructors, GetFTPDownloadParkInfo, the string constructors and destructors,
GetAvatarID, SetPlayerData, ReturnCityInfo, CreateRoom, LockChatPanelOff, AddUIError, the mutex, AddContact, StartingPopulationForOnline,
LoadBuddy, LoadIgnore) are fakes on both sides that record their arguments (pointers as a class number) and answer from the case. The strings
the code builds hold the hash of their text in their first word, so CreateRoom's log shows which string went where. sprintf is the C
library's on both sides.

The world of a case: the argument vector whose first element holds the command number (every number 0..0x36 and some past the table), the
object under test with random bytes and a chat word, gOnlineControl (absent, an own block, or the object under test), gOnlineSystem (absent
or a block with a word at 0xe0), the park and city info blocks, the table word of gText at 0x598, gGameWorld. What is compared: the log, the
three globals as classes, the object under test, the control and the system as bytes.

No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_COnlineControl_ProcessInitResponse"
MASK = 0xFFFFFFFF

FN = 0x10214E80
SIZES = {FN: 1648}
OWN = set(SIZES)

ADDR = dict(gOnlineControl=0x10396D28, gOnlineSystem=0x10380AB0, gGameWorld=0x1037EBFC, gTextTable=0x1037FF18 + 0x598)
HOST_LAYOUT = [0xE8C, 0x10, 8]

STATE = {}
DEBUG = {"done": False}


def s32(v):
    return T.s32(v)


def fnv(units):
    h = 2166136261
    for u in units:
        h = ((h ^ u) * 16777619) & MASK
    return h


def cbytes(e, a):
    s = e.cstr(a)
    return s.encode("latin1") if isinstance(s, str) else bytes(s)


def wunits(e, a):
    out = []
    while len(out) < 200:
        u = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if u == 0:
            break
        out.append(u)
    return out


def cls(e, p):
    p &= MASK
    if p == 0:
        return 0
    if p == STATE["this"]:
        return 1
    if p == e.u32(ADDR["gOnlineControl"]):
        return 2
    if p == e.u32(ADDR["gOnlineSystem"]):
        return 3
    if p == e.u32(ADDR["gGameWorld"]):
        return 4
    if p == STATE.get("last_new", 0):
        return 5
    if p == STATE["park"]:
        return 6
    if p == STATE["city"]:
        return 7
    return 99


def word(e, a):
    return e.u32(a & MASK)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def save_toc(e):
    """A glue function stores the caller's TOC pointer at 0x14(r1) and the caller reloads it afterwards."""
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))


def stub_udebug(e):
    e.cl.rec("UDebug", (fnv(cbytes(e, e.r(3))),))


def stub_new(e):
    save_toc(e)
    n = e.r(3) & MASK
    e.cl.rec("new", (n,))
    n = max(n, 4)
    a = e.alloc(n)
    e.write(a, bytes(n))
    STATE["last_new"] = a
    e.set_r(3, a)


def stub_sysctor(e):
    e.cl.rec("SysCtor", (cls(e, e.r(3)),))


def stub_ftp(e):
    e.cl.rec("GetFTP", (cls(e, e.r(3)),))
    e.set_r(3, STATE["park"])


def stub_strctor(e):
    h = fnv(cbytes(e, e.r(4)))
    e.write(e.r(3) & MASK, struct.pack(">I", h))
    e.cl.rec("StrCtor", (h,))


def stub_sprintf(e):
    """sprintf(buffer, "%d", value): the library's, as on the host (no event)."""
    save_toc(e)
    text = (cbytes(e, e.r(4) & MASK).decode("latin1") % s32(e.r(5))).encode("latin1") + b"\0"
    e.write(e.r(3) & MASK, text)


def stub_avatar(e):
    e.cl.rec("GetAvatarID", (cls(e, e.r(3)),))
    e.set_r(3, e.cl.ret("GetAvatarID") & MASK)


def stub_setplayer(e):
    save_toc(e)
    e.cl.rec("SetPlayerData", (e.r(3) & MASK, e.r(4) & MASK, cls(e, e.r(5))))


def stub_wctor(e):
    h = fnv(wunits(e, e.r(4) & MASK))
    e.write(e.r(3) & MASK, struct.pack(">I", h))
    e.cl.rec("WStrCtor", (h,))


def stub_city(e):
    e.cl.rec("ReturnCityInfo", (e.r(3) & MASK, e.r(4) & MASK))
    e.set_r(3, STATE["city"])


def stub_room(e):
    save_toc(e)
    e.cl.rec("CreateRoom", (e.r(3) & MASK, word(e, e.r(4)), word(e, e.r(5)), word(e, e.r(6)), word(e, e.r(7)), e.r(8) & MASK, cls(e, e.r(9))))


def stub_wdtor(e):
    e.cl.rec("WStrDtor", (word(e, e.r(3)), e.r(4) & 0xFFFF))


def stub_dtor(e):
    e.cl.rec("StrDtor", (word(e, e.r(3)), e.r(4) & 0xFFFF))


def stub_lockoff(e):
    e.cl.rec("LockOff", ())


def stub_uierror(e):
    e.cl.rec("AddUIError", (e.r(3) & MASK, e.r(4) & MASK))


def stub_ctrlctor(e):
    e.cl.rec("CtrlCtor", (cls(e, e.r(3)),))


def stub_lock(e):
    e.cl.rec("lock", (cls(e, (e.r(3) - 0x1C) & MASK),))


def stub_unlock(e):
    e.cl.rec("unlock", (cls(e, (e.r(3) - 0x1C) & MASK),))


def stub_addcontact(e):
    e.cl.rec("AddContact", (cls(e, e.r(3)), (e.r(4) - e.r(3)) & MASK, e.r(5) & MASK))


def stub_startpop(e):
    e.cl.rec("StartingPop", (cls(e, e.r(3)),))


def stub_loadbuddy(e):
    e.cl.rec("LoadBuddy", (cls(e, e.r(3)), fnv(cbytes(e, e.r(4)))))


def stub_loadignore(e):
    e.cl.rec("LoadIgnore", (cls(e, e.r(3)), fnv(cbytes(e, e.r(4)))))


for addr, fn in ((0x10238578, stub_sprintf), (0x10000E40, stub_udebug), (0x102384B8, stub_new), (0x1016F7D0, stub_sysctor), (0x10177654, stub_ftp),
                 (0x10002338, stub_strctor), (0x101719BC, stub_avatar), (0x1023B0F8, stub_setplayer), (0x10146A14, stub_wctor),
                 (0x10178B3C, stub_city), (0x1023B110, stub_room), (0x101468F8, stub_wdtor), (0x1000221C, stub_dtor),
                 (0x101E55EC, stub_lockoff), (0x101F1DF0, stub_uierror), (0x10213870, stub_ctrlctor), (0x10200D8C, stub_lock),
                 (0x10200DE0, stub_unlock), (0x1021B950, stub_addcontact), (0x1013B88C, stub_startpop), (0x1021D004, stub_loadbuddy),
                 (0x1021DB98, stub_loadignore)):
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

    def wstr(self, a, text):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", ord(ch)) for ch in text + "\0"))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        STATE.clear()
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def ptr(self, a, v):
        self.words(a, [v])

    def rdptr(self, a):
        return self.rd_word(a)

    def off(self, a):
        return a

    def log(self):
        return self.L.log

    def run(self, this, vec, park, city):
        STATE.update(this=this, park=park, city=city)
        if not DEBUG["done"]:
            DEBUG["done"] = True
            e = self.e
            tbl = [e.u32(0x102B1B68 + 4 * i) for i in range(0x37)]
            print("TABLE", " ".join("%d:%x" % (i, t) for i, t in enumerate(tbl) if t != 0x10215480))
            for off in (0x00, 0x1A, 0xB2, 0xE5, 0xE8, 0x112, 0x144, 0x177, 0x1D0, 0x1FE, 0x243, 0x276, 0x2AB, 0x2D8):
                print("STR %x %r" % (off, cbytes(e, 0x10250E7F + off)))
            print("WSTR", wunits(e, 0x1025179C + 0x12), wunits(e, 0x1025179C))
        T.call_ppc(self.e, FN, [this, vec])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_pir_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_pir_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rdptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def off(self, a):
        return self.lib.tpw_pir_offset(a)

    def log(self):
        return CL.host_end(self.lib)

    def run(self, this, vec, park, city):
        self.lib.tpw_pir_run(this, vec, park, city)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_pir_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "offset").restype, getattr(lib, p + "offset").argtypes = C.c_uint32, [C.c_uint64]
    getattr(lib, p + "run").restype, getattr(lib, p + "run").argtypes = None, [C.c_uint64] * 4
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    o = s.objs = {}
    this = o["this"] = s.alloc(0xE8C)
    s.write(this, bytes(case["filler"]))
    s.words(this, [case["chat"]])
    park = s.alloc(0x60)
    city = s.alloc(0x20)
    pname = s.alloc(64)
    cname = s.alloc(64)
    s.wstr(pname, case["pname"])
    s.wstr(cname, case["cname"])
    s.words(park + 8, [case["pid"]])
    s.words(park + 0x14, [s.off(pname)])
    s.words(park + 0x48, [case["cityid"]])
    s.words(city + 0x14, [s.off(cname)])
    o["park"], o["city"] = park, city
    for name in ("gOnlineControl", "gOnlineSystem", "gGameWorld"):
        s.ptr(s.g(name), 0)
    if case["ctrl"] == "this":
        s.ptr(s.g("gOnlineControl"), this)
    elif case["ctrl"] == "pre":
        o["ctrl_pre"] = a = s.alloc(0xE8C)
        s.write(a, bytes(case["ctrlfill"]))
        s.ptr(s.g("gOnlineControl"), a)
    if case["sys"] == "pre":
        o["sys_pre"] = a = s.alloc(0x404)
        s.write(a, bytes(case["sysfill"]))
        s.words(a + 0xE0, [case["list"]])
        s.ptr(s.g("gOnlineSystem"), a)
    if case["world"]:
        s.ptr(s.g("gGameWorld"), s.alloc(0x40))
    s.ptr(s.g("gTextTable"), case["table"])
    vec = s.alloc(16)
    data = s.alloc(0x30)
    s.ptr(vec + 8, data)
    s.words(data + 8, [case["cmd"]])
    o["vec"] = vec


def execute(s, case):
    o = s.objs
    s.run(o["this"], o["vec"], o["park"], o["city"])
    return 0


def kl(s, v):
    o = s.objs
    if v == 0:
        return "null"
    for k in ("this", "ctrl_pre", "sys_pre"):
        if v == o.get(k):
            return k
    return "new"


def observe(s, case, answer):
    o = s.objs
    g = lambda n: s.rd_word(s.g(n))
    cv, sv = s.rdptr(s.g("gOnlineControl")), s.rdptr(s.g("gOnlineSystem"))
    d = {"log": s.log(), "ctrl": kl(s, cv), "sys": kl(s, sv), "world": g("gGameWorld") != 0, "table": g("gTextTable"),
         "this0": s.rd_word(o["this"]), "thisb": s.read(o["this"] + 4, 0xDC)}
    d["ctrlb"] = s.read(cv + 4, 0x3C) if cv else None   # the word at 0 is the chat word (a pointer on the Mac): read as a word
    d["sysb"] = s.read(sv, 0x20) if sv else None
    d["sysw"] = s.rd_word(sv + 0xE0) if sv else None
    return d


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

ALPHA = "abcxyz019 _%"


def rstr(r):
    return "".join(r.choice(ALPHA) for _ in range(r.randrange(0, 9)))


def base(r, cmd, ctrl=None, sys=None):
    return {"op": "run", "cmd": cmd,
            "ctrl": r.choice(["none", "pre", "this"]) if ctrl is None else ctrl,
            "sys": r.choice(["none", "pre"]) if sys is None else sys,
            "chat": r.getrandbits(32), "list": r.getrandbits(32), "pid": r.getrandbits(32), "cityid": r.getrandbits(32),
            "pname": rstr(r), "cname": rstr(r), "table": r.choice([0, r.getrandbits(32)]), "world": r.random() < 0.8,
            "filler": bytes(r.randrange(256) for _ in range(0xE0)), "ctrlfill": bytes(r.randrange(256) for _ in range(0x40)),
            "sysfill": bytes(r.randrange(256) for _ in range(0x20)),
            "queues": {"GetAvatarID": [r.getrandbits(32)]}}


def edges():
    r = random.Random(5248)
    cmds = list(range(0x37)) + [0x37, 0x38, 0x100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
    for cmd in cmds:
        for ctrl in ("none", "pre", "this"):
            for sys in ("none", "pre"):
                yield base(r, cmd, ctrl, sys)


def rnd(r):
    cmd = r.choice([r.randrange(0x37), r.randrange(0x37), r.randrange(0x30, 0x60), r.getrandbits(32)])
    return base(r, cmd)


TESTS = [
    FnTest("COnlineControl::ProcessInitResponse", ppc, host, edge=edges, rnd=rnd),
]
