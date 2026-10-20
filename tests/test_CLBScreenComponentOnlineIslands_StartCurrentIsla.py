"""Unit v51106, the online islands' small members and the globes' panel members, against
libtpw_unit_CLBScreenComponentOnlineIslands_StartCurrentIsla.dylib (tests/unit_CLBScreenComponentOnlineIslands_StartCurrentIsla.cpp), main app
("SimTheme Park"), src/CLBScreenComponentOnlineIslands_GotoNextIsland_CLBScreenComponentOnlineIslands_StartCurrentIsla.cpp:
  OnlineIslands: StartCurrentIslandZoom 0x100b4574, SetCurrentIsland 0x100b4690, GotoPreviousIsland 0x100b4790, HidePanels 0x100b4354,
  ShowPanels 0x100b42d4, ShowInfoOverlayPanel 0x100b4828, Hide 0x100b4170, the destructor 0x100b4be0, WindowMessagesFn 0x100b3bc8;
  Islands::WindowMessagesFn 0x100b1e3c; PublishGlobe: destructor 0x100af1c8, IsCityValid 0x100af148, HidePanels 0x100aef20,
  ShowPanels 0x100aeeb0, Tick 0x100aeb10; DownloadGlobe: IsCityValid 0x100aea58, HidePanels 0x100ae828, ShowPanels 0x100ae7b0,
  Tick 0x100ae318; OnlineGlobe::Register 0x100ae1b0.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, 8-byte pointers) for the translation. The callees outside the unit
(the lobby controllers, the sound fade, the mesh systems, the base destructor, the list removal and insertion, the deleting
destructors reached through the vtables, the virtual slots) are fakes on both sides that record the same events; the list functions run
faithfully on the Mac nodes (the host uses the real header templates).

The world of a case: a pool of objects (the screen, two lists, islands or globes or flags, the mesh systems, a city), the lists filled,
the screen's fields. What is compared: the answer, the event log, the screen's fields and both lists (count, ends, the objects in order)
and the islands' sound handles. No fused operations. Not tested: nothing is skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CLBScreenComponentOnlineIslands_StartCurrentIsla"
MASK = 0xFFFFFFFF

# ---- the unit's own functions
FN = dict(
    zoom=(0x100b4574, 12), setcur=(0x100b4690, 24), prev=(0x100b4790, 76), hidep=(0x100b4354, 36), showp=(0x100b42d4, 60),
    showinfo=(0x100b4828, 12), hide=(0x100b4170, 296), dtor=(0x100b4be0, 208), wmf=(0x100b3bc8, 188), wmfi=(0x100b1e3c, 188),
    pdtor=(0x100af1c8, 300), picv=(0x100af148, 44), phide=(0x100aef20, 44), pshow=(0x100aeeb0, 44), ptick=(0x100aeb10, 32),
    dicv=(0x100aea58, 28), dhide=(0x100ae828, 52), dshow=(0x100ae7b0, 52), dtick=(0x100ae318, 32), reg=(0x100ae1b0, 40),
)
OWN = set(a for a, _ in FN.values())

# ---- the callees faked on both sides
ADDR = dict(gLobbyMeshes=0x102e48d0, gLobbyFlyingMeshes=0x102e48d4)
GLUE = 0x1023baa8
OPDELETE = 0x10238218
REMOVEHEAD = (0x100b50e8, 0x100afb98, 0x100af7e0)
ADDTAIL = 0x100afd30
ISL_PREV = 0x100b2808

LAYOUT_NAMES = ["vis", "isl", "cur", "cam", "fe8", "l_count", "l_head", "l_tail", "n_next", "n_prev", "s1", "s2", "g_list", "g_flags",
                "c_value", "c_f60", "c_f64"]
MAC = dict(vis=4, isl=8, cur=0xC, cam=0x14, fe8=0xE8, l_count=0xC, l_head=4, l_tail=8, n_next=4, n_prev=8, s1=0x1C, s2=0x20,
           g_list=8, g_flags=0x6C, c_value=0x40, c_f60=0x60, c_f64=0x64)

NPOOL = 13
POOL_SIZE = 0x400
LAB0 = 20

VT = [0]       # the address of the persistent vtable tokens (set in setup)
K_OBJ, K_ISL, K_ONL = 0, 1, 2


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)

for _addr, _name, _spec in (
        (0x101e0858, "ShowCityLobbyController", ""), (0x101e08c0, "HideCityLobbyController", ""),
        (0x101e07f0, "RemoveCityLobbyController", ""), (0x101e2380, "ShowOnlineCityInfoOverlay", ""),
        (0x101e23e8, "HideOnlineCityInfoOverlay", ""), (0x1022b718, "ShowWorldLobbyController", ""),
        (0x1022b780, "HideWorldLobbyController", ""), (0x1022b7e8, "ShowWorldLobbySats", ""),
        (0x1022b85c, "HideWorldLobbySats", ""), (0x100de0fc, "SOUND_Fade", "i"),
        (0x100a835c, "HideHeightfield", "L"), (0x100a88a4, "DeleteMeshInstanceListEntries", "L"),
        (0x100ab8f0, "DeleteFlyingMeshGroupListEntries", "L"), (0x100acce0, "OnlineGlobeTick", "L"),
        (0x100b6c2c, "BaseDtor", "Li")):
    STUBS.internal(_addr, CL.recorder(_name, _spec))


def stub_isl_prev(e):
    """CLBScreenComponentIslands::GotoPreviousIsland, the base: a virtual call slot 0x44 on the host."""
    L = e.cl
    L.rec("vcall", (0x44, L.lab(e.r(3))))


STUBS.internal(ISL_PREV, stub_isl_prev)


def save_toc(e):
    """The glue keeps the caller's TOC at 0x14(sp); the callers reload r2 from there."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


def stub_glue(e):
    """__ptr_glue: the target in r12 is a token (kind, slot) from the persistent vtables."""
    L = e.cl
    save_toc(e)
    tgt = e.r(12)
    kind, slot = (tgt >> 8) & 0xFF, tgt & 0xFF
    this = e.r(3)
    if kind == K_OBJ:
        assert slot == 8
        L.rec("delete", (L.lab(this),))
    elif kind == K_ISL:
        L.rec("vcall", (slot, L.lab(this)))
    else:
        if slot == 0x2C:
            L.rec("vcall", (0x2C, L.lab(this)))
            e.set_r(3, L.ret("IsVisible") & MASK)
        elif slot == 0x34:                      # HidePanels of the online islands
            L.rec("HideCityLobbyController", ())
        elif slot == 0x44:                      # GotoPreviousIsland of the online islands (0x100b4790, modelled by its effects)
            if e.u32(this + 0x14) == 0:
                L.rec("vcall", (0x44, L.lab(this)))
                e.write(this + 0xE8, struct.pack(">I", 1))
                L.rec("ShowCityLobbyController", ())
        else:
            L.rec("vcall", (slot, L.lab(this)))


STUBS.internal(GLUE, stub_glue)


def stub_opdelete(e):
    L = e.cl
    save_toc(e)
    L.rec("delete", (L.lab(e.r(3)),))


STUBS.internal(OPDELETE, stub_opdelete)


def stub_removehead(e):
    """TbTList<..>::RemoveHead on the Mac nodes (data 0, next 4, prev 8; list: head 4, tail 8, count 0xc)."""
    lst = e.r(3)
    head = e.u32(lst + 4)
    if head == 0:
        e.set_r(3, 0)
        return
    nxt = e.u32(head + 4)
    e.write(lst + 4, struct.pack(">I", nxt))
    if nxt == 0:
        e.write(lst + 8, struct.pack(">I", 0))
    else:
        e.write(nxt + 8, struct.pack(">I", 0))
    e.write(lst + 0xC, struct.pack(">I", (e.u32(lst + 0xC) - 1) & MASK))
    e.set_r(3, e.u32(head))


for _a in REMOVEHEAD:
    STUBS.internal(_a, stub_removehead)


def stub_addtail(e):
    lst, value = e.r(3), e.r(4)
    node = e.alloc(16)
    tail = e.u32(lst + 8)
    e.write(node, struct.pack(">III", value & MASK, 0, tail))
    if tail == 0:
        e.write(lst + 4, struct.pack(">I", node))
    else:
        e.write(tail + 4, struct.pack(">I", node))
    e.write(lst + 8, struct.pack(">I", node))
    e.write(lst + 0xC, struct.pack(">I", (e.u32(lst + 0xC) + 1) & MASK))


STUBS.internal(ADDTAIL, stub_addtail)


def setup(runner):
    T.install_std_stubs(runner.emu)
    data = b""
    for k in range(3):
        data += b"".join(struct.pack(">I", 0x7E000000 | (k << 8) | s) for s in range(0, 0x80, 4))
    VT[0] = T.persist_alloc(runner.emu, len(data), data)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def lab(self, p):
        return 0 if p == 0 else self.rev.get(p, -1)

    def put_i(self, a, v):
        self.write(a, struct.pack(self.ifmt, v & MASK))

    def get_i(self, a):
        return struct.unpack(self.ifmt, self.read(a, 4))[0]

    def put_p(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def get_p(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def addtail(self, kind, lst, item):
        o = self.o
        node = self.alloc(32)
        tail = self.get_p(lst + o["l_tail"])
        self.put_p(node, item)
        self.put_p(node + o["n_prev"], tail)
        if tail == 0:
            self.put_p(lst + o["l_head"], node)
        else:
            self.put_p(tail + o["n_next"], node)
        self.put_p(lst + o["l_tail"], node)
        self.put_i(lst + o["l_count"], self.get_i(lst + o["l_count"]) + 1)


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    ifmt = ">I"
    pfmt = ">I"
    o = MAC

    def __init__(self, e, case):
        self.e = e
        self.pool = [e.alloc(POOL_SIZE) for _ in range(NPOOL)]
        self.rev = {a: LAB0 + i for i, a in enumerate(self.pool)}
        self.L = e.cl = CL.PpcLog(dict(self.rev), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def make(self, idx, kind, vt=None):
        if kind == 7:
            return
        k = K_OBJ if kind != 0 else vt
        self.put_p(self.pool[idx], VT[0] + 0x80 * k)

    def global_ptr(self, name, value):
        self.put_p(ADDR[name], value)

    def log(self):
        return self.L.log

    def call(self, op, args, ret):
        r3, _, _ = T.call_ppc(self.e, FN[op][0], [a & MASK for a in args])
        return r3 & MASK


class Host(Side):
    """The same on the host library."""
    ifmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.o = lib.off
        self.pool = [lib.tpw_oi_alloc(POOL_SIZE) for _ in range(NPOOL)]
        self.rev = {a: LAB0 + i for i, a in enumerate(self.pool)}
        CL.host_begin(lib, {LAB0 + i: a for i, a in enumerate(self.pool)}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_oi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def make(self, idx, kind, vt=None):
        if kind != 7:
            self.lib.tpw_oi_new(kind, self.pool[idx])

    def global_ptr(self, name, value):
        self.put_p(self.lib.tpw_oi_addr(name.encode()), value)

    def addtail(self, kind, lst, item):
        self.lib.tpw_oi_addtail(kind, lst, item)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op, args, ret):
        a = list(args) + [0] * (4 - len(args))
        return int(getattr(self.lib, "tpw_oi_" + HOSTFN[op])(*a)) & (MASK if ret != "p" else (1 << 64) - 1)


HOSTFN = dict(zoom="zoom", setcur="setcur", prev="prev", hidep="hidep", showp="showp", showinfo="showinfo", hide="hide", dtor="dtor",
              wmf="wmf", wmfi="wmfi", pdtor="pdtor", picv="picv", phide="phide", pshow="pshow", ptick="ptick", dicv="dicv",
              dhide="dhide", dshow="dshow", dtick="dtick", reg="reg")


def bind(lib):
    """The host library's entry points, typed once; the host offsets are read once."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_oi_addr.restype, lib.tpw_oi_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_oi_alloc.restype, lib.tpw_oi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_oi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_oi_new.restype, lib.tpw_oi_new.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_oi_addtail.restype, lib.tpw_oi_addtail.argtypes = None, [C.c_int32, C.c_uint64, C.c_uint64]
    for name in HOSTFN.values():
        f = getattr(lib, "tpw_oi_" + name)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64, C.c_int64, C.c_int64, C.c_int64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_oi_layout(out)
    assert n == len(LAYOUT_NAMES), n
    lib.off = dict(zip(LAYOUT_NAMES, list(out)[:n]))
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    w = c["w"]
    P = s.pool
    o = s.o
    scr, la, lb = P[0], P[1], P[2]
    s.global_ptr("gLobbyMeshes", P[9])
    s.global_ptr("gLobbyFlyingMeshes", P[10])
    if c["screen"] == "onl":
        s.make(0, 0, c["vt"])
        s.make(1, 4)
        for i in range(3):
            s.make(3 + i, 1)
        s.make(12, 1)
        for i, idx in enumerate((3, 4, 5, 12)):
            s.put_i(P[idx] + o["s1"], w["snd"][i][0])
            s.put_i(P[idx] + o["s2"], w["snd"][i][1])
        for i in range(w["nA"]):
            s.addtail(4, la, P[3 + i])
        s.put_i(scr + o["vis"], w["vis"])
        s.put_i(scr + o["cam"], w["cam"])
        s.put_i(scr + o["fe8"], w["fe8"])
        s.put_p(scr + o["isl"], la)
        cur = w["cur"]
        s.put_p(scr + o["cur"], 0 if cur is None else (P[12] if cur == "x" else P[3 + cur]))
    else:
        s.make(0, 7)
        s.make(1, 5)
        s.make(2, 6)
        for i in range(3):
            s.make(3 + i, 2)
            s.make(6 + i, 3)
        s.make(12, 3)
        for i in range(w["nA"]):
            s.addtail(5, la, P[3 + i])
        for i in range(w["nB"]):
            s.addtail(6, lb, P[6 + i])
        s.put_p(scr + o["g_list"], la)
        s.put_p(scr + o["g_flags"], lb)
        city = P[11]
        s.put_i(city + o["c_value"], w["city"][0])
        s.put_i(city + o["c_f60"], w["city"][1])
        s.put_i(city + o["c_f64"], w["city"][2])


def walk(s, lst):
    o = s.o
    out = [s.get_i(lst + o["l_count"]) & MASK, 1 if s.get_p(lst + o["l_head"]) == 0 else 0, 1 if s.get_p(lst + o["l_tail"]) == 0 else 0]
    n = s.get_p(lst + o["l_head"])
    guard = 0
    while n and guard < 8:
        out.append(s.lab(s.get_p(n)))
        n = s.get_p(n + o["n_next"])
        guard += 1
    return out


def resolve(s, d):
    if d[0] == "P":
        return s.pool[d[1]]
    if d[0] == "N":
        return 0
    return d[1]


def observe(s, c, answer):
    o = s.o
    P = s.pool
    scr = P[0]
    d = {"answer": answer, "log": s.log()}
    if c["op"] in ("dtor", "pdtor"):
        return d            # the objects are freed (the log names every free); nothing is read back from freed memory
    d["listA"] = walk(s, P[1])
    if c["screen"] == "onl":
        d["screen"] = [s.get_i(scr + o["vis"]) & MASK, s.get_i(scr + o["cam"]) & MASK, s.get_i(scr + o["fe8"]) & MASK,
                       s.lab(s.get_p(scr + o["cur"])), s.lab(s.get_p(scr + o["isl"]))]
        d["sounds"] = [(s.get_i(P[i] + o["s1"]) & MASK, s.get_i(P[i] + o["s2"]) & MASK) for i in (3, 4, 5, 12)]
    else:
        d["screen"] = [s.lab(s.get_p(scr + o["g_list"])), s.lab(s.get_p(scr + o["g_flags"]))]
        d["listB"] = walk(s, P[2])
    return d


def execute(s, c):
    op = c["op"]
    args = [resolve(s, a) for a in c["args"]]
    ret = c["ret"]
    r = s.call(op, args, ret)
    if ret == "p":
        return s.lab(r)
    return r if ret == "u" else 0


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

THIS = ("P", 0)
NULL = ("N",)
SCREEN = dict(zoom="onl", setcur="onl", prev="onl", hidep="onl", showp="onl", showinfo="onl", hide="onl", dtor="onl", wmf="onl",
              wmfi="onl", pdtor="glob", picv="glob", phide="glob", pshow="glob", ptick="glob", dicv="glob", dhide="glob",
              dshow="glob", dtick="glob", reg="glob")
RET = dict(dtor="p", pdtor="p", picv="u", dicv="u")

MESSAGES = [0x1000B, 0x10005, 0, 0x1000A, 0x1000C, -1, 5, 0x10006, 0x20005]
KEYS = [0x2500, 0x2700, 0xD, 0, 1, -1, 0x24FF, 0x2501, 0x26FF, 0x2701, 0xC, 0xE]
FLAGS = [0, 1, -1, 0x7FFF, 0x8000, 0x10000, 0x10001, -0x8000, 0xFFFF, 0x12345]


def world(r, **kw):
    w = dict(vis=r.choice([0, 1, 2, -1]), cam=r.choice([0, 0, 1, 2, 3, -1]), fe8=r.choice([0, 1, 0x7FFFFFFF, -5]),
             nA=r.choice([0, 1, 2, 3]), nB=r.choice([0, 1, 2, 3]), cur=r.choice([None, 0, 1, 2, "x"]),
             snd=[[r.choice([0, 1, 5, -1, 0x1234, 99]) for _ in range(2)] for _ in range(4)],
             city=(r.choice([-5, -1, 0, 1, 5, 100]), r.choice([0, 0, 1, -1, 0x100]), r.choice([-1, 0, 1, 5, 99, 100, 101])))
    w.update(kw)
    return w


def mkcase(op, r, w, args, vis=None):
    vt = K_ISL if op == "wmfi" else K_ONL
    q = {"IsVisible": vis if vis is not None else [r.choice([0, 1]), r.choice([0, 1])]}
    return {"op": op, "screen": SCREEN[op], "vt": vt, "w": w, "args": args, "ret": RET.get(op), "queues": q}


def arglists(op, r):
    if op == "setcur":
        return [[THIS, NULL], [THIS, ("P", 3)], [THIS, ("P", 4)], [THIS, ("P", 12)]]
    if op in ("dtor", "pdtor"):
        return [[THIS, ("I", f)] for f in FLAGS] + [[NULL, ("I", 0)], [NULL, ("I", 1)]]
    if op in ("wmf", "wmfi"):
        return [[THIS, ("I", m), ("I", a), ("I", r.choice([0, 1, -1, 7]))] for m in MESSAGES for a in KEYS]
    if op in ("picv", "dicv"):
        return [[THIS, ("P", 11)]]
    if op == "reg":
        return [[THIS, ("P", 12)], [THIS, ("P", 6)], [THIS, ("P", 7)]]
    return [[THIS]]


def edges(op):
    r = random.Random(sum(map(ord, op)) * 7919)
    ws = [world(r, vis=1, cam=0, nA=0, nB=0, cur=None), world(r, vis=1, cam=1, nA=1, nB=1, cur=0),
          world(r, vis=0, cam=2, nA=3, nB=3, cur=2), world(r, cam=-1, nA=2, nB=2, cur="x"),
          world(r, cam=0, nA=1, nB=0, cur=0, city=(0, 0, 1)), world(r, cam=0, nA=3, nB=1, cur=1, city=(1, 0, 1)),
          world(r, cam=0, nA=2, nB=2, cur=1, city=(5, 1, 6)), world(r, cam=3, nA=0, nB=2, cur=None, city=(-1, 0, 0))]
    if op in ("wmf", "wmfi"):
        ws = ws[:2]
    if op in ("dtor", "pdtor"):
        ws = ws[:5]
    for w in ws:
        for args in arglists(op, r):
            if op == "hide":
                for vis in ([0, 0], [1, 0], [1, 1], [0, 1]):
                    yield mkcase(op, r, w, args, vis)
            else:
                yield mkcase(op, r, w, args)


def rnd(op):
    def gen(r):
        args = r.choice(arglists(op, r))
        return mkcase(op, r, world(r), args)
    return gen


NAMES = [
    ("zoom", "CLBScreenComponentOnlineIslands::StartCurrentIslandZoom"),
    ("setcur", "CLBScreenComponentOnlineIslands::SetCurrentIsland"),
    ("prev", "CLBScreenComponentOnlineIslands::GotoPreviousIsland"),
    ("hidep", "CLBScreenComponentOnlineIslands::HidePanels"),
    ("showp", "CLBScreenComponentOnlineIslands::ShowPanels"),
    ("showinfo", "CLBScreenComponentOnlineIslands::ShowInfoOverlayPanel"),
    ("hide", "CLBScreenComponentOnlineIslands::Hide"),
    ("dtor", "CLBScreenComponentOnlineIslands::~CLBScreenComponentOnlineIslands"),
    ("wmf", "CLBScreenComponentOnlineIslands::WindowMessagesFn"),
    ("wmfi", "CLBScreenComponentIslands::WindowMessagesFn"),
    ("pdtor", "CLBScreenComponentPublishGlobe::~CLBScreenComponentPublishGlobe"),
    ("picv", "CLBScreenComponentPublishGlobe::IsCityValid"),
    ("phide", "CLBScreenComponentPublishGlobe::HidePanels"),
    ("pshow", "CLBScreenComponentPublishGlobe::ShowPanels"),
    ("ptick", "CLBScreenComponentPublishGlobe::Tick"),
    ("dicv", "CLBScreenComponentDownloadGlobe::IsCityValid"),
    ("dhide", "CLBScreenComponentDownloadGlobe::HidePanels"),
    ("dshow", "CLBScreenComponentDownloadGlobe::ShowPanels"),
    ("dtick", "CLBScreenComponentDownloadGlobe::Tick"),
    ("reg", "CLBScreenComponentOnlineGlobe::Register"),
]

TESTS = [FnTest(name, ppc, host, edge=(lambda op=op: edges(op)), rnd=rnd(op)) for op, name in NAMES]
