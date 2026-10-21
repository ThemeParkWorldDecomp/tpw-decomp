"""Unit v5057, the park name panel, against libtpw_unit_ShowEditParkNamePanel.dylib (tests/unit_ShowEditParkNamePanel.cpp), main app
("SimTheme Park"), src/game/ui/ui_entryPricePanel.cpp:
  ShowEditParkNamePanel 0x1019deac, TwoEditsMsg 0x1019da6c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers) for the translation; the same code reads both back.
Every callee outside the unit is a fake on both sides that records its arguments (windows by the tag in their id, things by the first
word of their block, texts as length and first eight characters, new objects by label): Build, setZ, GetWindow, SetClientDrawObject,
the font and text setters, the edit box calls, GetSignName/SetSignName of the thing and of the master save, the constructors, the
cheat, the messages, the key table calls and the window's virtual SetText (called through a hand-made vtable at +0x118 on the Mac).
The window's default handler answers from a queue, FindDrawObject too.

The world of a case: the screen window (z), the panel global, the world's gates thing index, the thing array (entries 0 to 5), the master
save pointer, the cheat guard byte, the two text areas, the conversion buffer, the cheat texts and the texts the fakes answer.
What is compared: the answer, the event log, both text areas, the first characters of the conversion buffer, the panel global, the master
save pointer (null, the old one, a new one) and the guard byte.

No fused operations. Not tested: a GetWindow that answers NULL (the original dereferences it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ShowEditParkNamePanel"
MASK = 0xFFFFFFFF

FN_SHOW, FN_TWO = 0x1019DEAC, 0x1019DA6C
SIZES = {FN_SHOW: 904, FN_TWO: 1032}
OWN = set(SIZES)

ADDR = dict(bufA=0x10394782, bufB=0x10394764, conv=0x10380C48, panel=0x1039483C, screen=0x10394848, world=0x1037EBFC,
            thing=0x1034CBFC, master=0x10380A8C, guard=0x102DA940, cheatA=0x102A9940, cheatB=0x102A9944, script=0x102A9834,
            tvec=0x10267B18)
STRIDE_PPC = 0x14
GATES_OFF = 0x1DA732

# tags of the windows
ROOT, PANEL, TITLE, EDIT_A, EDIT_B, NAMEW, MSGW, STALE = 1, 2, 3, 4, 5, 6, 7, 9

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = {}


def s32(v):
    return T.s32(v)


def wid(e, p):
    return e.u32((p + 0x88) & MASK) if p else 0


def objtag(e, p):
    return e.u32(p) if p else 0


def whichbuf(p):
    return {ADDR["bufA"]: 1, ADDR["bufB"]: 2, ADDR["conv"]: 3}.get(p, 0)


def rd_text(e, a):
    out = []
    while True:
        c = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if c == 0:
            return out
        out.append(c)


def tx(e, a):
    """The length (at most 9) and the first eight characters; length -1 for NULL."""
    if not a:
        return (-1,) + (0,) * 8
    n = 0
    while n < 9 and struct.unpack(">H", e.read(a + 2 * n, 2))[0] != 0:
        n += 1
    cs = [struct.unpack(">H", e.read(a + 2 * i, 2))[0] for i in range(min(n, 8))]
    return (n,) + tuple(cs) + (0,) * (8 - len(cs))


def stub(addr):
    def deco(fn):
        STUBS.internal(addr, fn)
        return fn
    return deco


@stub(0x101DEB78)
def s_build(e):
    e.cl.rec("Build", (wid(e, e.r(3)), int(e.r(4) == ADDR["script"]), int(e.r(5) == ADDR["tvec"])))
    e.set_r(3, CUR["panel"])


@stub(0x101DD8BC)
def s_setz(e):
    e.cl.rec("setZ", (wid(e, e.r(3)), T.s16(e.r(4))))


@stub(0x101DBE40)
def s_getwindow(e):
    p, i = wid(e, e.r(3)), s32(e.r(4))
    e.cl.rec("GetWindow", (p, i))
    e.set_r(3, CUR["children"].get((p, i), 0))


@stub(0x101DB624)
def s_setclient(e):
    e.cl.rec("SetClientDrawObject", (wid(e, e.r(3)), e.cl.lab(e.r(4))))


@stub(0x10180270)
def s_getfont(e):
    n = e.r(3)
    e.cl.rec("GetFontTableEntry", (n,))
    e.set_r(3, 0x1000 + n * 16)


@stub(0x101DA118)
def s_setfont(e):
    e.cl.rec("SetFont", (wid(e, e.r(3)), e.r(4)))


@stub(0x101DA204)
def s_fontjust(e):
    e.cl.rec("SetFontJust", (wid(e, e.r(3)), s32(e.r(4)), s32(e.r(5))))


@stub(0x101DA3AC)
def s_fontcolour(e):
    e.cl.rec("SetFontColour", (wid(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))


@stub(0x10180468)
def s_getstring(e):
    e.cl.rec("UIMain_GetString", (s32(e.r(3)),))
    e.set_r(3, CUR["str"])


@stub(0x1018BD3C)
def s_titlecolour(e):
    e.cl.rec("UI_TITLE_COLOUR", (wid(e, e.r(3)),))


@stub(0x1000DDE8)
def s_assert(e):
    e.cl.rec("GAME_ASSERTMSG", (e.r(3) & 0xFF,))


@stub(0x101CD7E0)
def s_setstatic(e):
    e.cl.rec("SetStaticTextArea", (wid(e, e.r(3)), whichbuf(e.r(4)), s32(e.r(5))))
    e.set_r(3, 0)


@stub(0x101CD2D4)
def s_finddo(e):
    a = e.r(3)
    cs = []
    while len(cs) < 9 and e.read(a + len(cs), 1)[0] != 0:
        cs.append(e.read(a + len(cs), 1)[0])
    c8 = cs[:8] + [0] * (8 - len(cs[:8]))
    e.cl.rec("FindDrawObject", (len(cs),) + tuple(c8))
    e.set_r(3, e.cl.ret("FindDrawObject") & MASK)


@stub(0x101CDAC4)
def s_setcursor(e):
    e.cl.rec("SetCursorDrawObject", (wid(e, e.r(3)), e.r(4)))


@stub(0x101CE8A4)
def s_selectall(e):
    e.cl.rec("SelectAll", (wid(e, e.r(3)),))


def getsign(name, key):
    def f(e):
        e.cl.rec(name, (objtag(e, e.r(3)), whichbuf(e.r(4)), e.r(5)))
        text = rd_text(e, CUR[key])
        e.write(e.r(4), b"".join(struct.pack(">H", c) for c in text) + b"\0\0")
    return f


STUBS.internal(0x101107C4, getsign("GetSignNameA", "signA"))
STUBS.internal(0x101108DC, getsign("GetSignNameB", "signB"))


def plain(addr, name):
    STUBS.internal(addr, lambda e: e.cl.rec(name, ()))


plain(0x10182364, "UIMain_DisableAllKeyTables")
plain(0x10182304, "UIMain_EnableAllKeyTables")
plain(0x1018116C, "UIMain_DecideKbCapture")


@stub(0x101DCCEC)
def s_capturekb(e):
    e.cl.rec("CaptureKb", (wid(e, e.r(3)), s32(e.r(4))))


@stub(0x101DE248)
def s_settext(e):
    e.cl.rec("SetText", (wid(e, e.r(3)),) + tx(e, e.r(4)))
    e.set_r(3, 1)


def setsign(name):
    return lambda e: e.cl.rec(name, (objtag(e, e.r(3)),) + tx(e, e.r(4)))


STUBS.internal(0x10110948, setsign("SetSignNameA"))
STUBS.internal(0x10110B10, setsign("SetSignNameB"))


@stub(0x101EEB9C)
def s_regen(e):
    e.cl.rec("RegenerateRideSign", (objtag(e, e.r(3)), whichbuf(e.r(4)), whichbuf(e.r(5))))


STUBS.internal(0x1016B31C, lambda e: e.cl.rec("MasterSetSignNameA", (e.cl.lab(e.r(3)),) + tx(e, e.r(4))))
STUBS.internal(0x1016B388, lambda e: e.cl.rec("MasterSetSignNameB", (e.cl.lab(e.r(3)),) + tx(e, e.r(4))))


@stub(0x10146F5C)
def s_setcheat(e):
    e.cl.rec("SetCheatStatus", (e.cl.lab(e.r(3)), e.r(4) & 0xFF))


def msg(name):
    return lambda e: e.cl.rec(name, (wid(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))


STUBS.internal(0x101C9518, msg("UI_SendMessage"))
STUBS.internal(0x101C9400, msg("UI_PostMessage"))


@stub(0x101810FC)
def s_getobjname(e):
    e.cl.rec("UIMain_GetObjectName", (objtag(e, e.r(3)),))
    e.set_r(3, CUR["name"])


@stub(0x101DE318)
def s_default(e):
    e.cl.rec("CallDefaultWindowHandler", (wid(e, e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, e.cl.ret("CallDefaultWindowHandler") & MASK)


def s_wcslen(e):
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2)))
    e.set_r(3, len(rd_text(e, e.r(3))))


def s_wcscmp(e):
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2)))
    a, b = rd_text(e, e.r(3)), rd_text(e, e.r(4))
    n = 0
    while n < len(a) and n < len(b) and a[n] == b[n]:
        n += 1
    ca = a[n] if n < len(a) else 0
    cb = b[n] if n < len(b) else 0
    e.set_r(3, (ca - cb) & MASK)


STUBS.internal(0x102389C8, s_wcslen)
STUBS.internal(0x10238C98, s_wcscmp)


STUBS.internal(0x1018FBE8, CL.recorder("InterfaceTitleTextObject", "Ni"))
STUBS.internal(0x1018F1D0, CL.recorder("InterfaceCursorPosTextObjectSprite", "Ni"))
STUBS.internal(0x101694DC, CL.recorder("CFSGlobalMasterSave", "N"))
STUBS.internal(0x10146F24, CL.recorder("CCheat", "N"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def u32(self, a, v):
        self.write(a, struct.pack(self.E + "I", v & MASK))

    def r32(self, a):
        return struct.unpack(self.E + "I", self.read(a, 4))[0]

    def half(self, a, v):
        self.write(a, struct.pack(self.E + "H", v & 0xFFFF))

    def put16s(self, a, vals):
        self.write(a, struct.pack(self.E + "%dH" % len(vals), *vals))

    def get16s(self, a, n):
        return list(struct.unpack(self.E + "%dH" % n, self.read(a, 2 * n)))

    def byte(self, a, v):
        self.write(a, bytes([v & 0xFF]))

    def rbyte(self, a):
        return self.read(a, 1)[0]

    def ptr(self, a, v):
        self.write(a, struct.pack(self.E + self.PF, v & self.PM))

    def rptr(self, a):
        return struct.unpack(self.E + self.PF, self.read(a, self.PW))[0]

    def text(self, chars):
        a = self.alloc(48)
        self.put16s(a, list(chars) + [0])
        return a

    def window(self, tag, z=0):
        a = self.alloc(self.win_size)
        self.u32(a + self.tag_off, tag)
        self.half(a + self.z_off, z)
        self.ptr(a + self.vptr_off, self.vptr())
        return a


class Ppc(Side):
    E, PF, PW, PM = ">", "I", 4, MASK
    win_size, tag_off, z_off, vptr_off, stride = 0x140, 0x88, 0xDC, 0x118, STRIDE_PPC

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        CUR.clear()
        CUR["children"] = {}
        self._vptr = None

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

    def vptr(self):
        if self._vptr is None:
            vt = self.alloc(0x40)
            tv = self.alloc(8)
            self.u32(tv, 0x101DE248)
            self.u32(tv + 4, self.e.r(2))
            self.u32(vt + 0x2C, tv)
            self._vptr = vt
        return self._vptr

    def reset(self):
        pass

    def child(self, parent, i, addr):
        CUR["children"][(parent, i)] = addr

    def block(self, which, addr):
        CUR[{0: "panel", 1: "signA", 2: "signB", 3: "str", 4: "name"}[which]] = addr

    def call_show(self):
        T.call_ppc(self.e, FN_SHOW, [])
        return 0

    def call_two(self, win, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_TWO, [win & MASK, m & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    E, PF, PW, PM = "<", "Q", 8, (1 << 64) - 1

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.win_size = lib.lay[2]
        self.tag_off, self.z_off, self.vptr_off, self.stride = lib.lay[0], lib.lay[1], 0, lib.lay[3]
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v5057_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v5057_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def vptr(self):
        return self.g("vtbl")

    def reset(self):
        self.lib.tpw_v5057_reset()

    def child(self, parent, i, addr):
        self.lib.tpw_v5057_child(parent, i, addr)

    def block(self, which, addr):
        self.lib.tpw_v5057_block(which, addr)

    def call_show(self):
        self.lib.tpw_v5057_show()
        return 0

    def call_two(self, win, m, a, b):
        return self.lib.tpw_v5057_two(win, s32(m), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v5057_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    getattr(lib, p + "reset").argtypes = []
    getattr(lib, p + "child").argtypes = [C.c_int32, C.c_int32, C.c_uint64]
    getattr(lib, p + "block").argtypes = [C.c_int32, C.c_uint64]
    getattr(lib, p + "show").argtypes = []
    getattr(lib, p + "two").restype, getattr(lib, p + "two").argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    lay = list(out)[:n]
    # [mId, mZ, sizeof(InterfaceWindow), sizeof(CThingArrayEntry), offsetof(CWorld, mGatesThing)]
    assert lay[4] == GATES_OFF and lay[2] > lay[0] and lay[2] > lay[1] and lay[2] >= 8 and lay[3] >= 8, lay
    lib.lay = lay
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    s.reset()
    sa, sb = s.text(c["signA"]), s.text(c["signB"])
    s.block(1, sa)
    s.block(2, sb)
    s.block(3, s.text(c["str"]) if c["str"] is not None else 0)
    s.block(4, s.text(c["name"]) if c["name"] is not None else 0)
    wins = {t: s.window(t, c["z"] if t == ROOT else 0) for t in (ROOT, PANEL, TITLE, EDIT_A, EDIT_B, NAMEW, MSGW, STALE)}
    s.block(0, wins[PANEL])
    for parent, i, t in ((ROOT, 0x4F3AF, NAMEW), (PANEL, 0x3943E, TITLE), (PANEL, 0x3943C, EDIT_A), (PANEL, 0x3943D, EDIT_B),
                         (MSGW, 0x3943C, EDIT_A), (MSGW, 0x3943D, EDIT_B)):
        s.child(parent, i, wins[t])
    s.ptr(s.g("screen"), wins[ROOT])
    s.ptr(s.g("panel"), wins[STALE] if c["panelset"] else 0)
    blk = s.alloc(16)
    s.half(blk, c["gates"])
    s.ptr(s.g("world"), blk - GATES_OFF)
    for i in range(6):
        entry = s.g("thing") + i * s.stride
        if i == 0 and not c["entry0"]:
            s.ptr(entry, 0)
        else:
            o = s.alloc(16)
            s.u32(o, 0x700 + i)
            s.ptr(entry, o)
    pre = 0
    if c["master"]:
        pre = s.alloc(16)
        s.u32(pre, 0x900)
    s.ptr(s.g("master"), pre)
    s.byte(s.g("guard"), c["guard"])
    s.put16s(s.g("bufA"), c["bufA"])
    s.put16s(s.g("bufB"), c["bufB"])
    s.put16s(s.g("conv"), c["conv"])
    s.ptr(s.g("cheatA"), s.text(c["cheatA"]))
    s.ptr(s.g("cheatB"), s.text(c["cheatB"]))
    return {"pre": pre, "msgw": wins[MSGW]}


def observe(s, c, ctx, answer):
    p = s.rptr(s.g("panel"))
    m = s.rptr(s.g("master"))
    return {"answer": answer, "log": s.log(), "bufA": s.get16s(s.g("bufA"), 15), "bufB": s.get16s(s.g("bufB"), 15),
            "conv": s.get16s(s.g("conv"), 16), "panel": s.r32(p + s.tag_off) if p else 0,
            "master": 0 if m == 0 else (1 if m == ctx["pre"] else 2), "guard": s.rbyte(s.g("guard"))}


def run(s, c):
    try:
        ctx = build(s, c)
        if c["op"] == "show":
            ans = s.call_show()
        else:
            ans = s.call_two(ctx["msgw"], *c["args"])
        return observe(s, c, ctx, ans)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rstr(r, maxlen=8):
    out = []
    for _ in range(r.randint(0, maxlen)):
        out.append(r.choice([r.randrange(1, 8), r.randrange(0x20, 0x7F), r.randrange(1, 0x10000)]))
    return out


def scrambled(chars):
    s = list(chars) + [0] * (15 - len(chars))
    i = 0

    def wl():
        k = 0
        while s[k] != 0:
            k += 1
        return k

    while i < wl():
        s[i] ^= i
        i += 1
    return s[:wl()]


def buf15(r, chars):
    tail = [r.randrange(0, 0x10000) if r.random() < 0.3 else 0 for _ in range(15 - len(chars) - 1)]
    return list(chars) + [0] + tail


def base(r, op):
    bufa, bufb = rstr(r), rstr(r)
    ma, mb = r.random() < 0.5, r.random() < 0.5
    ca = scrambled(bufa) if ma else scrambled(bufa) + [0x41]
    cb = scrambled(bufb) if mb else rstr(r) + [0x42]
    return {"op": op,
            "queues": {"CallDefaultWindowHandler": [r.choice([0, 1, -1, r.randrange(0, 1 << 31)]) & MASK],
                       "FindDrawObject": [r.choice([0, 0x1234, r.randrange(1, 1 << 31)])]},
            "z": r.choice([0, -1, 5, 100, 32767, 32760, 32761, -32768, r.randrange(-32768, 32768)]),
            "panelset": r.random() < 0.15, "gates": r.choice([0, 1, 2, 3, 4, 5]), "entry0": r.random() < 0.15,
            "master": r.random() < 0.5, "guard": r.choice([0, 1, -1, 0x80, r.randrange(0, 256)]),
            "bufA": buf15(r, bufa), "bufB": buf15(r, bufb), "conv": [r.randrange(0, 0x10000) for _ in range(16)],
            "signA": rstr(r), "signB": rstr(r),
            "str": None if r.random() < 0.1 else rstr(r), "name": None if r.random() < 0.1 else rstr(r),
            "cheatA": ca, "cheatB": cb}


MSGS = [0x802, 0x803, 0x804, 0x100, 0x14, 0x101, 0x13, 0x801, 0x805, 0, -1, 0x7FFFFFFF, 0x15, 0x2000, 0xFF, 0x1000]
AS = [0x3943C, 0x3943D, 0x3943E, -1, -2, 0, 5, 0x3943B, 0x4F3AF, -3]


def two_case(r, m=None, a=None):
    c = base(r, "two")
    c["args"] = [r.choice(MSGS) if m is None else m, r.choice(AS) if a is None else a, r.choice([0, 1, -1, r.randrange(-1000, 1000)])]
    return c


def two_edges():
    r = random.Random(5057)
    for m in MSGS:
        for a in AS:
            yield two_case(r, m, a)
    for ma in (0, 1):
        for mb in (0, 1):
            for master in (0, 1):
                for guard in (0, 1, -1):
                    for gates in (0, 3):
                        for entry0 in (0, 1):
                            c = two_case(r, 0x100, -1)
                            bufa, bufb = rstr(r), rstr(r)
                            c["bufA"], c["bufB"] = buf15(r, bufa), buf15(r, bufb)
                            c["cheatA"] = scrambled(bufa) if ma else scrambled(bufa) + [1]
                            c["cheatB"] = scrambled(bufb) if mb else [7]
                            c.update(master=master, guard=guard, gates=gates, entry0=entry0)
                            yield c
    # a text with a position that scrambles to zero in the middle (the length is asked again on every pass)
    for texts in ([1], [0x41, 1], [0x41, 0x42, 2, 0x44], [0, 0x41], [1, 1, 2, 3]):
        c = two_case(r, 0x100, -1)
        c["bufA"], c["bufB"] = buf15(r, texts), buf15(r, texts)
        c["cheatA"], c["cheatB"] = scrambled(texts), scrambled(texts)
        yield c


def two_random(r):
    if r.random() < 0.6:
        return two_case(r, 0x100, -1)
    return two_case(r)


def show_case(r):
    c = base(r, "show")
    c["args"] = []
    return c


def show_edges():
    r = random.Random(1019)
    for panelset in (0, 1):
        for gates in range(6):
            for entry0 in (0, 1):
                c = show_case(r)
                c.update(panelset=panelset, gates=gates, entry0=entry0)
                yield c
    for z in (0, -1, 32767, 32760, 32761, 32762, -32768, -7, -8):
        c = show_case(r)
        c.update(z=z, panelset=0)
        yield c
    for fdo in (0, 0x1234):
        c = show_case(r)
        c.update(panelset=0)
        c["queues"]["FindDrawObject"] = [fdo]
        yield c


def show_random(r):
    return show_case(r)


TESTS = [
    FnTest("ShowEditParkNamePanel", ppc, host, edge=show_edges, rnd=show_random),
    FnTest("TwoEditsMsg", ppc, host, edge=two_edges, rnd=two_random),
]
