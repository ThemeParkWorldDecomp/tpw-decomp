"""Unit v5565, the tag panel functions, against libtpw_unit_ShowTagPanel.dylib (tests/unit_ShowTagPanel.cpp), main app
("SimTheme Park"), src/ui_StatusPanel_SetButton_ShowTagPanel.cpp:
  ShowTagPanel 0x10190d0c, TagCallback 0x10190f04

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the window
members, the emitter calls, the panel calls, FindStaticTag, DoButton, the virtual Enable reached through __ptr_glue, CoolRect's
constructor and destructor) is a fake on both sides that records its arguments and answers from the case's queues. Windows are objects
numbered 1..9 (the label is the number); a fake that answers a window answers by number.

The world of a case: nine windows with the bytes at +0x134 and +0x135, the five window globals, the tag count, the ten static tags
(window, active flag), and the answers of the fakes. What is compared: the answer, the log, the globals, the tags, the window bytes.

No fused operations. Not tested: a FindStaticTag answer of -1 in the message 5 path (the original then indexes the table before its
start, over other globals).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ShowTagPanel"
MASK = 0xFFFFFFFF

FN_SHOW, FN_CB = 0x10190D0C, 0x10190F04
OWN = {FN_SHOW, FN_CB}

ADDR = dict(count=0x1038CD74, panel=0x1038CD78, pending=0x1038CD7C, source=0x1038CD80, tags=0x1038CD84,
            first=0x103949D0, aerial=0x103949D4, anim=0x102A83EA)
HOST_LAYOUT_PTR = 8
NOBJ = 9
MAC_WINDOW = 0x140
MAC_TAG, MAC_TAG_ACT = 0x208, 4


def s16(v):
    return T.s16(v)


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def objaddr(e, k):
    return e.objs[k] if 0 < k < len(e.objs) else 0


def mk(name, args, ret=None, addr=None):
    def f(e):
        L = e.cl
        L.rec(name, tuple(args(e, L)))
        if ret:
            ret(e, L)
    STUBS.internal(addr, f)


def rkey(base, n=None):
    return base if n is None else "%s%d" % (base, n)


def r_val(key):
    def f(e, L):
        e.set_r(3, L.ret(key) & MASK)
    return f


def r_obj(key_fn):
    def f(e, L):
        e.set_r(3, objaddr(e, L.ret(key_fn(e, L))))
    return f


def gep(e):
    L = e.cl
    idx = s16(e.r(4))
    L.rec("getExtraParam", (L.lab(e.r(3)), idx))
    e.set_r(3, L.ret(rkey("gep", idx)) & MASK)


STUBS.internal(0x101DD030, gep)
mk("setExtraParam", lambda e, L: (L.lab(e.r(3)), s16(e.r(4)), s32(e.r(5))), lambda e, L: e.set_r(3, 1), 0x101DCFD0)
mk("stopAnimTimer", lambda e, L: (L.lab(e.r(3)), s32(e.r(4))), None, 0x101DD754)
mk("PAR_GetEmitterStatus", lambda e, L: (s32(e.r(3)),), r_val("status"), 0x100BF110)
mk("PAR_KillEmitter", lambda e, L: (s32(e.r(3)),), None, 0x100BE1E8)
mk("LinkSlideoutPanel", lambda e, L: (L.lab(e.r(3)), s32(e.r(4))), None, 0x101A82EC)
mk("UIMain_AdvisorMessage", lambda e, L: (s32(e.r(3)),), None, 0x101821CC)
mk("CoolRect", lambda e, L: (), None, 0x101C7448)
mk("~CoolRect", lambda e, L: (), None, 0x101C74E0)
mk("PAR_SetEmitterPos", lambda e, L: (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))), None, 0x100BE150)
mk("PAR_SetEmitterMask", lambda e, L: (s32(e.r(3)), s32(e.r(4))), None, 0x100BE260)
mk("PAR_InitEmitter", lambda e, L: (s32(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))), r_val("init"), 0x100BEC30)
mk("RemoveSlideoutPanel", lambda e, L: (s16(e.r(3)),), None, 0x101A83D8)
mk("UIMain_UISound", lambda e, L: (s32(e.r(3)),), None, 0x101802CC)
mk("UIMain_MidWindowParticles", lambda e, L: (s32(e.r(3)), L.lab(e.r(4))), None, 0x10182404)
mk("FindStaticTag", lambda e, L: (L.lab(e.r(3)),), r_val("fst"), 0x10191CF4)
mk("DoButton", lambda e, L: (s32(e.r(3)), (e.r(4) >> 16) & 0xFFFF, s32(e.r(5))), None, 0x10190A60)
mk("Show", lambda e, L: (L.lab(e.r(3)), s16(e.r(4))), lambda e, L: e.set_r(3, 1), 0x101DC1EC)
mk("AdjustPosition", lambda e, L: (L.lab(e.r(3)), s16(e.r(4)), s16(e.r(5)), s16(e.r(6)), s16(e.r(7))), None, 0x101DB0E8)
mk("ShiftPosition", lambda e, L: (L.lab(e.r(3)), s16(e.r(4)), s16(e.r(5))), lambda e, L: e.set_r(3, 1), 0x101DA9D0)
mk("startAnim", lambda e, L: (L.lab(e.r(3)), 1 if e.r(4) == ADDR["anim"] else 2, s32(e.r(5))), lambda e, L: e.set_r(3, 1), 0x101DD08C)
mk("unlink", lambda e, L: (L.lab(e.r(3)),), None, 0x101DE084)
mk("GetVisibility", lambda e, L: (L.lab(e.r(3)),), lambda e, L: e.set_r(3, L.ret("vis") & MASK), 0x101DBD9C)
mk("GetNext", lambda e, L: (L.lab(e.r(3)),), None, 0x101DBB38)
mk("GetWindow", lambda e, L: (L.lab(e.r(3)), s32(e.r(4))), r_obj(lambda e, L: "getwin"), 0x101DBE40)
mk("GetParent", lambda e, L: (L.lab(e.r(3)),), r_obj(lambda e, L: "parent"), 0x101DBAFC)
mk("CallDefaultWindowHandler", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))), r_val("default"), 0x101DE318)
mk("UI_PostMessage", lambda e, L: (L.lab(e.r(3)), s32(e.r(4)), L.lab(e.r(5)), s32(e.r(6))), lambda e, L: e.set_r(3, 0), 0x101C9400)


def stub_getnext(e):
    L = e.cl
    lb = L.lab(e.r(3))
    L.rec("GetNext", (lb,))
    e.set_r(3, objaddr(e, L.ret(rkey("next", lb))))


STUBS.internal(0x101DBB38, stub_getnext)


def stub_getpos(e):
    L = e.cl
    L.rec("GetPosition", (L.lab(e.r(3)), s16(e.r(5))))
    rect = e.r(4)
    e.write(rect + 4, struct.pack(">hhhh", s16(L.ret("posl")), s16(L.ret("post")), s16(L.ret("posr")), s16(L.ret("posb"))))
    e.set_r(3, 1)


STUBS.internal(0x101DB884, stub_getpos)


def stub_getdims(e):
    L = e.cl
    L.rec("GetDims", (L.lab(e.r(3)), 1 if e.r(4) else 0, 1 if e.r(5) else 0, s16(e.r(6))))
    if e.r(5):
        e.write(e.r(5), struct.pack(">h", s16(L.ret("dims"))))
    e.set_r(3, 1)


STUBS.internal(0x101DBEB0, stub_getdims)


def stub_glue(e):
    """The virtual Enable(0) through the TVector glue: (this, on). The glue saves the TOC at 0x14(r1), the caller reloads it."""
    L = e.cl
    L.rec("Enable", (L.lab(e.r(3)), s16(e.r(4))))
    e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
    e.set_r(3, 1)


STUBS.internal(0x1023BAA8, stub_glue)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def label(self, addr):
        if addr == 0:
            return 0
        return self.rev.get(addr, -1)

    def setup_objs(self):
        self.rev = {self.objs[k]: k for k in range(1, NOBJ + 1)}


class Ppc(Side):
    fmt = ">I"
    psz = 4
    tagsz, tagact = MAC_TAG, MAC_TAG_ACT
    off134, off135 = 0x134, 0x135

    def __init__(self, e, case):
        self.e = e
        self.objs = [0] + [e.alloc(MAC_WINDOW) for _ in range(NOBJ)]
        e.objs = self.objs
        self.vt = e.alloc(0x40)
        self.setup_objs()
        self.L = e.cl = CL.PpcLog({self.objs[k]: k for k in range(1, NOBJ + 1)}, case["queues"])
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

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def init_window(self, k):
        self.ptr(self.objs[k] + 0x118, self.vt)

    def show(self, win):
        T.call_ppc(self.e, FN_SHOW, [win & MASK])
        return 0

    def callback(self, win, msg, a, b):
        with_r3 = T.call_ppc(self.e, FN_CB, [win & MASK, msg & MASK, a & MASK, b & MASK])[0]
        return with_r3 & MASK


class Host(Side):
    fmt = "<I"
    psz = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lay = lib.lay
        self.winsz, self.off134, self.off135, self.tagsz, self.tagact = lay[0], lay[1], lay[2], lay[3], lay[4]
        self.objs = [0] + [lib.tpw_stp_alloc(self.winsz + 16) for _ in range(NOBJ)]
        self.setup_objs()
        self.vt = self.g("vt")
        CL.host_begin(lib, {k: self.objs[k] for k in range(1, NOBJ + 1)}, case["queues"])
        tbl = self.g("objs")
        for k in range(1, NOBJ + 1):
            self.ptr(tbl + 8 * k, self.objs[k])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_stp_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def init_window(self, k):
        self.ptr(self.objs[k], self.vt)

    def show(self, win):
        self.lib.tpw_stp_show(win)
        return 0

    def callback(self, win, msg, a, b):
        return self.lib.tpw_stp_callback(win, s32(msg), s32(a), s32(b)) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_stp_addr.restype, lib.tpw_stp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_stp_alloc.restype, lib.tpw_stp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_stp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_stp_show.restype, lib.tpw_stp_show.argtypes = None, [C.c_uint64]
    lib.tpw_stp_callback.restype, lib.tpw_stp_callback.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_stp_layout(out)
    lib.lay = list(out)[:n]
    assert lib.lay[5] == HOST_LAYOUT_PTR, lib.lay
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    for k in range(1, NOBJ + 1):
        s.init_window(k)
        s.write(s.objs[k] + s.off134, bytes([case["w134"][k - 1]]))
        s.write(s.objs[k] + s.off135, bytes([case["w135"][k - 1]]))
    for name in ("panel", "pending", "source", "first", "aerial"):
        s.ptr(s.g(name), s.objs[case[name]] if case[name] else 0)
    s.words(s.g("count"), [case["count"]])
    base = s.g("tags")
    for i, (act, lb) in enumerate(case["tags"]):
        s.ptr(base + i * s.tagsz, s.objs[lb] if lb else 0)
        s.write(base + i * s.tagsz + s.tagact, struct.pack(s.fmt[0] + "h", s16(act)))


def execute(s, case):
    win = s.objs[1]
    if case["op"] == "show":
        return s.show(win)
    return s.callback(win, case["msg"], case["a"], case["b"])


def observe(s, case, answer):
    base = s.g("tags")
    tags = []
    for i in range(10):
        lb = s.label(s.rd_ptr(base + i * s.tagsz))
        act = struct.unpack(s.fmt[0] + "h", s.read(base + i * s.tagsz + s.tagact, 2))[0]
        tags.append((lb, act))
    flags = [(s.read(s.objs[k] + s.off134, 1)[0], s.read(s.objs[k] + s.off135, 1)[0]) for k in range(1, NOBJ + 1)]
    return {"answer": answer, "log": s.log(), "count": s.rd_words(s.g("count"), 1),
            "globals": [s.label(s.rd_ptr(s.g(n))) for n in ("panel", "pending", "source", "first", "aerial")],
            "tags": tags, "flags": flags}


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

POS = [0, -64, -128, -192, -384, 64, 32767, -32768, 1, -1, 100, -100, 3000, -3000]


def base(r, op, msg=0, a=0, b=0):
    q = {}
    for i in (2, 3, 5, 6, 8):
        q["gep%d" % i] = [r.choice([0, 1, -1, 7, r.randrange(-2 ** 31, 2 ** 31)])]
    q["gep6"] = [r.choice([0x1B, 0x1B, 0, 1, 5, r.randrange(-2 ** 31, 2 ** 31)])]
    q["gep9"] = [r.choice([-1, -1, -2, 0, 5, 0x7FFFFFFF, r.randrange(0, 100000), r.randrange(-2 ** 31, 2 ** 31)])]
    q["gep10"] = [r.choice([0, 1, 0x100])]
    q["status"] = [r.choice([0, 1, 2])]
    if op == "cb" and msg == 5:
        q["fst"] = [r.randrange(0, 10)]
    else:
        q["fst"] = [r.choice([-1, 0, 1, 5, 9])]
    q["dims"] = [r.choice([0, 20, -10, 32765, 32760, -32768, r.randrange(-300, 300)])]
    q["init"] = [r.choice([0, 1, 0x80000000, 0xFFFFFFFF, r.randrange(0, 2 ** 32)])]
    for k, n in (("posl", 0), ("post", 1), ("posr", 2), ("posb", 3)):
        q[k] = [r.choice(POS)]
    q["vis"] = [r.choice([0, 1, -1, 0x7FFF])]
    q["default"] = [r.choice([0, 1, -1, r.randrange(-2 ** 31, 2 ** 31)])]
    q["next1"] = [r.choice([0, 2, 2])]
    q["next2"] = [r.choice([0, 3])]
    for k in range(3, NOBJ + 1):
        q["next%d" % k] = [0]
    q["parent"] = [r.choice([0, 9])]
    q["getwin"] = [r.choice([0, 9, 9])]
    return {"op": op, "msg": msg, "a": a, "b": b, "queues": q,
            "panel": 4, "aerial": 7,
            "source": r.choice([0, 1, 1, 5]), "pending": r.choice([0, 1, 6]), "first": r.choice([0, 1, 1, 8]),
            "count": r.randrange(0, 20),
            "tags": [(r.choice([0, 1, 1, 0x100, -1]), r.randrange(0, NOBJ + 1)) for _ in range(10)],
            "w134": [r.choice([0, 1, 0x80, 0xFF]) for _ in range(NOBJ)],
            "w135": [r.choice([0, 1, 0x80, 0xFF]) for _ in range(NOBJ)]}


MESSAGES = [(5, [0, 1, 2]), (6, [0, 1]), (0x10, [0x80080, 0x80081, 0x80082, 0, 0x80, 0x100081]), (0x11, [0, 1, 5]), (0x13, [0, 3]),
            (0x14, [0, 1]), (0x10006, [0, 1, 2, -1]), (0, [0, 1]), (4, [0]), (7, [3]), (0xF, [0]), (0x12, [1]), (0x15, [0]),
            (0x10005, [1]), (0x10007, [0]), (0x20000, [0]), (-1, [0]), (0x14, [0])]


def cb_case(r, msg=None, a=None):
    if msg is None:
        msg = r.choice(MESSAGES)[0]
    if a is None:
        a = r.choice(dict(MESSAGES)[msg] + [r.randrange(-2 ** 31, 2 ** 31)])
    return base(r, "cb", msg, a, r.choice([0, 1, r.randrange(-2 ** 31, 2 ** 31)]))


def cb_edges():
    r = random.Random(5565)
    for msg, avals in MESSAGES:
        for a in avals:
            for _ in range(10):
                yield cb_case(r, msg, a)


def cb_random(r):
    return cb_case(r)


def show_case(r):
    return base(r, "show")


def show_edges():
    r = random.Random(5566)
    for _ in range(60):
        yield show_case(r)


def show_random(r):
    return show_case(r)


TESTS = [
    FnTest("ShowTagPanel", ppc, host, edge=show_edges, rnd=show_random),
    FnTest("TagCallback", ppc, host, edge=cb_edges, rnd=cb_random),
]
