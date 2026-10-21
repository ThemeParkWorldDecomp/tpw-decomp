"""Unit v5097, the research panel's DoScroll, InitAll and ResearchPanelMsg, against libtpw_unit_DoScroll.dylib (tests/unit_DoScroll.cpp), main app
("SimTheme Park"), src/game/ui/ui_researchPanel.cpp:
  DoScroll 0x101b42d0, InitAll 0x101b4000, ResearchPanelMsg 0x101b45e0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (GetWindow, SetFrame, the anim timer calls, the research lab and analyser members, NameWindow, PercentComplete, ResearchSpeed,
RemoveResearchPanel, the sound calls, the UIMain calls and the window's SetValue virtual) are fakes on both sides that record their arguments
and answer from the case's queues.

The world of a case: the panel pointer, the world pointer, the sound handles, the gauge, ability and max words, the made flag, the lab's
work load word (0x1460) and a window per control id (or none, for InitAll). What is compared: the answer, those globals, the work load word and
the event log.

No fused operations. Not tested: a zero gauge or max ability (divw by zero is undefined), a NULL window where the original calls a virtual on it.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DoScroll"
MASK = 0xFFFFFFFF

FN_SCROLL, FN_INIT, FN_MSG = 0x101B42D0, 0x101B4000, 0x101B45E0
SIZES = {FN_SCROLL: 752, FN_INIT: 516, FN_MSG: 832}
OWN = set(SIZES)

ADDR = dict(world=0x1037EBFC, panel=0x10395980, scroll=0x10395960, soundB=0x10395964, soundA=0x10395968,
            gauge=0x1039596C, current=0x10395970, max=0x10395974, made=0x10395978)
HOST_LAYOUT = [0x1460]

GLUE = 0x1023BAA8
IDS = list(range(0xD2D0B, 0xD2D13))      # the nine windows: these eight ids, then any other
CUR = {}                                  # per case: id -> window address on the PPC side


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def win_for(e_or_cur, i):
    return CUR["wins"][i if 0xD2D0B <= i <= 0xD2D12 and False else (i - 0xD2D0B if 0xD2D0B <= i <= 0xD2D12 else 8)]


def ret_window(e, log):
    i = s32(e.r(4))
    return CUR["wins"][i - 0xD2D0B if 0xD2D0B <= i <= 0xD2D12 else 8] if log.ret("GW%x" % (i & MASK)) else 0


STUBS.internal(0x101DBE40, CL.recorder("GetWindow", "Li", ret=ret_window))
STUBS.internal(0x101DB7DC, CL.recorder("SetFrame", "Li"))
STUBS.internal(0x101DD5E0, CL.recorder("setAnimTimer", "Lii", ret="h"))
STUBS.internal(0x101DD6B8, CL.recorder("startAnimTimer", "Li"))
STUBS.internal(0x10122918, CL.recorder("SetEffort", "Liu"))
STUBS.internal(0x10122998, CL.recorder("SetWorkLoad", "Lu"))
STUBS.internal(0x101227FC, CL.recorder("GetEffort", "Lu", ret="i"))
STUBS.internal(0x1013EBC8, CL.recorder("GetParkAnalyser", "L", ret=lambda e, log: CUR["analyser"]))
STUBS.internal(0x1013ECE8, CL.recorder("GetResearchLab", "L", ret=lambda e, log: CUR["lab"]))
STUBS.internal(0x100E8DD8, CL.recorder("CurrentResearcherAbility", "L", ret="i"))
STUBS.internal(0x101B3DA8, CL.recorder("NameWindow", "i"))
STUBS.internal(0x101B3398, CL.recorder("PercentComplete", "i"))
STUBS.internal(0x101B381C, CL.recorder("ResearchSpeed", "i"))
STUBS.internal(0x101B5AE8, CL.recorder("RemoveResearchPanel", ""))
STUBS.internal(0x100DD380, CL.recorder("SOUND_SetParamValue", "iuu"))
STUBS.internal(0x100DE058, CL.recorder("SOUND_StopSoundEffect", "i"))
STUBS.internal(0x100DE0FC, CL.recorder("SOUND_FadeSoundEffect", "i"))
STUBS.internal(0x1018227C, CL.recorder("UIMain_CancelAdvisorMessage", "i"))
STUBS.internal(0x1018493C, CL.recorder("UIMain_DefaultPanelMsg", "Liii", ret="i"))
STUBS.internal(0x10180468, CL.recorder("UIMain_GetString", "i", ret="i"))
STUBS.internal(0x10180794, CL.recorder("UIMain_SetTitleText", "Liu"))
STUBS.internal(0x101802CC, CL.recorder("UIMain_UISound", "i", ret="i"))

_glue_rec = CL.recorder("SetValue", "Li")


def glue(e):
    """__ptr_glue calling a window's SetValue: the real glue saves the TOC at 0x14(r1) for the caller's reload."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
    _glue_rec(e)


STUBS.internal(GLUE, glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def wr_ptr(self, a, v):
        """A native pointer global: a Mac word on the PowerPC side, the host's 8 bytes on the host."""
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]


class Ppc(Side):
    fmt = ">I"
    pfmt = ">I"
    lab_off = 0x1460

    def __init__(self, e, case):
        self.e = e
        self.panel, self.world, self.analyser, self.lab = (e.alloc(0x2000) for _ in range(4))
        self.wins = []
        for _ in range(9):
            w = e.alloc(0x130)
            table = e.alloc(0x40)
            e.write(w + 0x118, struct.pack(">I", table))
            e.write(table + 0x24, struct.pack(">I", 0x1000))
            self.wins.append(w)
        CUR["wins"], CUR["analyser"], CUR["lab"] = self.wins, self.analyser, self.lab
        fixed = {self.panel: 1, self.world: 2, self.analyser: 3, self.lab: 4}
        fixed.update({w: 10 + i for i, w in enumerate(self.wins)})
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

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [x & MASK for x in args])
        return r3 & MASK if entry == FN_MSG else 0


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        out = (C.c_uint64 * 13)()
        lib.tpw_rp_objects(out)
        self.panel, self.world, self.analyser, self.lab = out[0], out[1], out[2], out[3]
        self.wins = list(out[4:13])
        self.lab_off = HOST_LAYOUT[0]
        fixed = {1: self.panel, 2: self.world, 3: self.analyser, 4: self.lab}
        fixed.update({10 + i: w for i, w in enumerate(self.wins)})
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_rp_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry, args):
        if entry == FN_SCROLL:
            self.lib.tpw_rp_scroll(s32(args[0]), s32(args[1]))
            return 0
        if entry == FN_INIT:
            self.lib.tpw_rp_init()
            return 0
        return self.lib.tpw_rp_msg(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_rp_addr.restype, lib.tpw_rp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_rp_alloc.restype, lib.tpw_rp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_rp_objects.argtypes = [C.POINTER(C.c_uint64)]
    lib.tpw_rp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_rp_scroll.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_rp_msg.restype, lib.tpw_rp_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_rp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    s.wr_ptr(s.g("world"), s.world)
    s.wr_ptr(s.g("panel"), s.panel if case["panel"] else 0)
    for k in ("scroll", "soundA", "soundB", "gauge", "current", "max"):
        s.words(s.g(k), [case[k]])
    s.write(s.g("made"), bytes([case["made"]]))
    s.words(s.lab + s.lab_off, [case["load"]])


def execute(s, case):
    op = case["op"]
    if op == "scroll":
        return s.call(FN_SCROLL, case["args"])
    if op == "init":
        return s.call(FN_INIT, [])
    return s.call(FN_MSG, [s.panel] + case["args"])


def observe(s, case, answer):
    w = s.rd_ptr(s.g("panel"))
    d = {"answer": answer, "log": s.log(), "panel": 1 if w == s.panel else w,
         "made": s.read(s.g("made"), 1)[0], "load": s.rd_word(s.lab + s.lab_off)}
    for k in ("scroll", "soundA", "soundB", "gauge", "current", "max"):
        d[k] = s.rd_word(s.g(k))
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


def base(r, op, init=False):
    q = {"GW%x" % i: [1 if not init or r.random() < 0.7 else 0] for i in range(0xD2D0B, 0xD2D13)}
    for i in (0xD2D00, 0xD2D13):
        q["GW%x" % i] = [1]
    q.update({"GetEffort": [r.randrange(0, 150)], "setAnimTimer": [r.choice([0, 1, -1, 0x10000, 5])],
              "CurrentResearcherAbility": [r.randrange(-20, 130), r.randrange(-20, 130)],
              "UIMain_DefaultPanelMsg": [r.randrange(-5, 5)], "UIMain_GetString": [r.randrange(1, 1 << 30)],
              "UIMain_UISound": [r.randrange(0, 1000)]})
    cur = r.randrange(-20, 130)
    if op == "msg" and r.random() < 0.3:
        cur = q["CurrentResearcherAbility"][0]
    return {"op": op, "queues": q, "panel": r.random() < 0.9 or init, "scroll": r.choice([0, r.randrange(1, 500)]),
            "soundA": r.randrange(0, 500), "soundB": r.randrange(0, 500), "gauge": r.choice([1, 2, 100, 1024, r.randrange(1, 3000)]),
            "current": cur & MASK, "max": r.choice([1, 3, 100, r.randrange(1, 3000)]), "made": r.choice([0, 1, 0xFF]),
            "load": r.randrange(0, 200) | (0 if r.random() < 0.8 else 1 << 31), "args": []}


def val(r):
    return r.choice([0, 1, -1, 100, 1023, 1024, 2000, -2000, r.randrange(-5000, 5000), r.randrange(-(1 << 31), 1 << 31)])


def scroll_case(r, i=None, v=None):
    c = base(r, "scroll")
    i = r.choice(IDS + [0xD2D0A, 0xD2D13, 0, -1, 0xD2D11]) if i is None else i
    c["args"] = [i, val(r) if v is None else v]
    if r.random() < 0.3 and c["args"][0] == 0xD2D10:
        c["args"][1] = c["gauge"] + r.choice([-1, 0, 1])
    return c


def scroll_edges():
    r = random.Random(11)
    for i in IDS + [0xD2D0A, 0xD2D13, 0, -1]:
        for v in (0, 1, -1, 1023, 1024):
            yield scroll_case(r, i, v)
    for g in (1, 100, 1024, 77):
        for d in (-1, 0, 1, 2):
            c = scroll_case(r, 0xD2D10, 0)
            c["gauge"] = g
            c["args"][1] = g + d
            yield c
    for v in (-1, -1023, -1024, -1025, 1, 1023, 1025, -(1 << 31) + 1, (1 << 31) - 1):
        c = scroll_case(r, 0xD2D10, v)
        c["gauge"] = 1 << 30
        yield c


def init_case(r):
    return base(r, "init", True)


def init_edges():
    r = random.Random(12)
    for i in range(20):
        yield init_case(r)
    for k in range(5):
        c = init_case(r)
        for j, key in enumerate("GW%x" % x for x in range(0xD2D0B, 0xD2D10)):
            c["queues"][key] = [1 if j == k else 0]
        yield c
    c = init_case(r)
    c["panel"] = True
    for key in c["queues"]:
        if key.startswith("GW"):
            c["queues"][key] = [0]
    yield c


MSGS = [0x1E, 5, 0x10, 0x14, 0x15, 0x100, 0x800, 0x801, 0x802, 0x13, 0x16, 0x1F, 0x20, 0, 0xF, 0x11, 0x7FF, 0x803, 0x101, 0x1D, 0x4, 0x6, 0x12]


def msg_case(r, m=None, a=None, b=None):
    c = base(r, "msg")
    m = r.choice(MSGS) if m is None else m
    if a is None:
        a = r.choice([0, -1, -2, -3, 1, 0xD2D10, 0xD2D0B, 0xD2D0F, 0xD2D11, r.randrange(-5, 5)])
    c["args"] = [m, a, val(r) if b is None else b]
    if m == 0x800 and a == 0xD2D10:
        c["args"][2] = r.choice([0, c["gauge"], c["gauge"] + 1, 5])
    return c


def msg_edges():
    r = random.Random(13)
    for m in MSGS:
        for a in (0, -1, -2, -3, 1, 0xD2D10, 0xD2D0B):
            yield msg_case(r, m, a, r.choice([0, 5, -7]))
    for k in range(6):
        c = msg_case(r, 0x1E, 0)
        if k < 2:
            c["current"] = c["queues"]["CurrentResearcherAbility"][0] & MASK
        yield c
    for a in IDS:
        yield msg_case(r, 0x800, a, 300)


TESTS = [
    FnTest("DoScroll", ppc, host, edge=scroll_edges, rnd=lambda r: scroll_case(r)),
    FnTest("InitAll", ppc, host, edge=init_edges, rnd=init_case),
    FnTest("ResearchPanelMsg", ppc, host, edge=msg_edges, rnd=lambda r: msg_case(r)),
]
