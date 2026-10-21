"""Unit v5957, the publish and download globes' members, against libtpw_unit_CLBScreenComponentPublishGlobe_WindowMessagesFn.dylib
(tests/unit_CLBScreenComponentPublishGlobe_WindowMessagesFn.cpp), main app ("SimTheme Park"),
src/game/online/lobby_globe.cpp:
  CLBScreenComponentPublishGlobe::WindowMessagesFn 0x100aeb6c, Show 0x100aec3c, Hide 0x100aece8, CreateCities 0x100aef90,
  HandleOnlineBacktrack 0x100af094; CLBScreenComponentDownloadGlobe::HandleOnlineBacktrack 0x100ae9c0, CreateCities 0x100ae8a0

A case is a world and a call. The world is built twice: in emulator memory at the Mac's addresses (big-endian, the objects with
vtables whose slots point at descriptors of stubbed addresses, so that __ptr_glue runs for real) for the original, and in the host
library (C++ objects made by the library's exports) for the translation. Every callee outside the unit is a fake on both sides that
records its arguments: the base class's window messages, the panel, the lobby controller, the overlay, OnlineGlobe::Show, ClearAll,
Add, the two RemoveHead, the mesh clean-up, SOUND_FadeSoundEffect, the online system's constructor and SetPublishing, the flag
constructor, the virtual members (IsVisible, IsCityWanted, the theme islands' Show and EnterCurrentIsland, the download globe's
Hide) and the deleting destructors of the globes and flags the lists give back. Hide of the publish globe runs for real when
HandleOnlineBacktrack calls it (it is in the unit).

The world of a case: the globe (visible word, the word at 0x70 pointing at a city flag, the sound words, the word at 0x68, the two
lists' counts), the answer scripts of the two RemoveHead (objects of a pool, or NULL), the IsVisible and IsCityWanted queues, the
screens (theme islands, advisor), the meshes, the online system (none or one), the display flags and the park selection word.
What is compared: the event log, the globe's words, the display flags, the online system (none, the old one, a new one) and the
park selection (unchanged, or which city).

No fused operations. Not tested: nothing (operator new never fails, so its NULL branches are dead).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CLBScreenComponentPublishGlobe_WindowMessagesFn"
MASK = 0xFFFFFFFF

F_WMF, F_SHOW, F_HIDE, F_CC, F_BT = 0x100AEB6C, 0x100AEC3C, 0x100AECE8, 0x100AEF90, 0x100AF094
D_BT, D_CC = 0x100AE9C0, 0x100AE8A0
SIZES = {F_WMF: 132, F_SHOW: 112, F_HIDE: 396, F_CC: 164, F_BT: 104, D_BT: 72, D_CC: 192}
OWN = set(SIZES)

# callees (bl targets) and the virtual targets the vtables point at
OG_WMF, PANEL, CWLC, COCI, OG_SHOW, CLEARALL = 0x100AD54C, 0x101ECCC8, 0x1022B038, 0x101E20F0, 0x100AD788, 0x100A7128
RWLC, RH_G, RH_F, DMIL, FADE, ROCI = 0x1022B6AC, 0x100AFB98, 0x100AF7E0, 0x100A88A4, 0x100DE0FC, 0x101E2314
SYS_CTOR, SETPUB, FLAG_CTOR, ADD = 0x1016F7D0, 0x10174E4C, 0x100AC5C8, 0x100A7074
IS_VISIBLE, PUB_WANTED, DL_WANTED, DL_HIDE = 0x100A6FB8, 0x100AF1C8, 0x100AF330, 0x100AE63C
THEME_SHOW, THEME_ENTER, DELETE = 0x100B2CC4, 0x100B346C, 0x100AC86C

G_SCREENS, G_MESHES, G_SYS, G_DISPLAY, G_SEL = 0x102E49B8, 0x102E48D0, 0x10380AB0, 0x10165634,0x102E4864
SENT = 0xBEEF
TOC_DISPLAY = 0x1026067C

LAB = dict(this=1, gl=2, fl=3, screens=4, advisor=5, theme=6, meshes=7, sys=8, f70=9)
for i in range(3):
    LAB["g%d" % i] = 10 + i
    LAB["f%d" % i] = 13 + i
for i in range(5):
    LAB["c%d" % i] = 20 + i


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mk(name, spec, retkey=None):
    def f(e):
        L = e.cl
        args = []
        for k, ch in enumerate(spec):
            v = e.r(3 + k)
            if ch == "L":
                args.append(L.lab(v))
            elif ch == "i":
                args.append(s32(v))
            elif ch == "b":
                args.append(v & 0xFF)
            else:
                args.append(v & MASK)
        L.rec(name, tuple(args))
        if retkey:
            e.set_r(3, L.ret(retkey) & MASK)
    return f


def stub_remove_head(e):
    """TbTList::RemoveHead on real nodes (word 0 the data, 4 the next, 8 the previous; the list's head at 4, tail at 8, count at
    0xc): NULL when empty, else the head's data, the count one less. The host runs the generic inline one of TbTList.h."""
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


def stub_delete(e):
    L = e.cl
    L.rec("delete", (L.lab(e.r(3)), s32(e.r(4))))
    L.rec("delete", (L.lab(e.r(3)),))   # the deleting destructor's operator delete (the host's shim logs it)


def stub_panel(e):
    L = e.cl
    L.rec("CreatePublishParkPanel", (L.lab(e.u32(G_SEL)),))


for addr, fn in (
    (OG_WMF, mk("OnlineGlobeWindowMessagesFn", "Liii")),
    (PANEL, stub_panel),
    (CWLC, mk("CreateWorldLobbyController", "b")),
    (COCI, mk("CreateOnlineCityInfoOverlay", "")),
    (OG_SHOW, mk("OnlineGlobeShow", "L")),
    (CLEARALL, mk("ClearAll", "L")),
    (RWLC, mk("RemoveWorldLobbyController", "")),
    (RH_G, stub_remove_head),
    (RH_F, stub_remove_head),
    (DMIL, mk("DeleteMeshInstanceListEntries", "L")),
    (FADE, mk("SOUND_FadeSoundEffect", "i")),
    (ROCI, mk("RemoveOnlineCityInfoOverlay", "")),
    (SYS_CTOR, CL.recorder("CFSOnlineSystem", "N")),
    (SETPUB, mk("SetPublishing", "Liuu")),
    (FLAG_CTOR, CL.recorder("CLBGlobeCityFlag", "NLL")),
    (ADD, mk("Add", "Lii")),
    (IS_VISIBLE, mk("IsVisible", "L", "IsVisible")),
    (PUB_WANTED, mk("IsCityWanted", "LL", "IsCityWanted")),
    (DL_WANTED, mk("IsCityWanted", "LL", "IsCityWanted")),
    (DL_HIDE, mk("DownloadHide", "L")),
    (THEME_SHOW, mk("ThemeShow", "L")),
    (THEME_ENTER, mk("ThemeEnter", "L")),
    (DELETE, stub_delete),
):
    STUBS.internal(addr, fn)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def lab_of(self, addr):
        for name, a in self.A.items():
            if a == addr:
                return LAB[name]
        return -1


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.toc = e.r(2)
        self.A = {}
        self.make(case)
        self.L = e.cl = CL.PpcLog({a: LAB[n] for n, a in self.A.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def mklist(self, entries, prefix):
        """A list of real nodes holding the pool objects (0: NULL data)."""
        lst = self.obj(0x10)
        nodes = [self.alloc(12) for _ in entries]
        for k, i in enumerate(entries):
            self.words(nodes[k], [self.A[prefix + str(i - 1)] if i else 0, nodes[k + 1] if k + 1 < len(nodes) else 0,
                                  nodes[k - 1] if k else 0])
        self.words(lst + 4, [nodes[0] if nodes else 0, nodes[-1] if nodes else 0, len(nodes)])
        return lst

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def desc(self, code):
        a = self.alloc(8)
        self.words(a, [code, self.toc])
        return a

    def obj(self, size, vt=0):
        a = self.alloc(size)
        if vt:
            self.words(a, [vt])
        return a

    def make(self, case):
        A = self.A
        vtg = self.alloc(0x10)
        self.words(vtg + 8, [self.desc(DELETE)])
        for i in range(3):
            A["g%d" % i] = self.obj(0x18, vtg)
            A["f%d" % i] = self.obj(0x10, vtg)
        A["gl"] = self.mklist(case["gscript"], "g")
        A["fl"] = self.mklist(case["fscript"], "f")
        for i in range(5):
            A["c%d" % i] = self.obj(0x40)
        A["f70"] = self.obj(0x10, vtg)
        self.words(A["f70"] + 0xC, [A["c4"]])
        vt = self.alloc(0x60)
        wanted = PUB_WANTED if case["kind"] == 0 else DL_WANTED
        self.words(vt + 0x28, [self.desc(F_HIDE if case["kind"] == 0 else DL_HIDE)])
        self.words(vt + 0x2C, [self.desc(IS_VISIBLE)])
        self.words(vt + 0x48, [self.desc(wanted)])
        this = A["this"] = self.alloc(0x7C)
        self.words(this, [vt, case["vis"], A["gl"]])
        self.words(this + 0x68, [case["f68"], A["fl"], A["f70"] if case["hasf70"] else 0, case["f74"], case["f78"]])
        vtt = self.alloc(0x60)
        self.words(vtt + 0x24, [self.desc(THEME_SHOW)])
        self.words(vtt + 0x50, [self.desc(THEME_ENTER)])
        A["theme"] = self.obj(0x100, vtt)
        A["advisor"] = self.obj(0x30)
        A["screens"] = self.alloc(0x20)
        self.words(A["screens"] + 4, [A["theme"]])
        self.words(A["screens"] + 0x14, [A["advisor"]])
        A["meshes"] = self.obj(0x10)
        A["sys"] = self.obj(0x404)
        self.words(G_SCREENS, [A["screens"]])
        self.words(G_MESHES, [A["meshes"]])
        self.words(G_SYS, [A["sys"] if case["sys"] else 0])
        self.disp = self.e.u32(TOC_DISPLAY)   # the import's slot: where the emulator keeps gei_Display
        self.words(self.disp, [case["gflags"]])
        self.words(G_SEL, [SENT])

    def city_list(self, n):
        lst = self.alloc(0x10)
        nodes = []
        for i in range(n):
            nodes.append(self.alloc(12))
        for i in range(n):
            self.words(nodes[i], [self.A["c%d" % i], nodes[i + 1] if i + 1 < n else 0, nodes[i - 1] if i else 0])
        self.words(lst + 4, [nodes[0] if n else 0, nodes[-1] if n else 0, n])
        return lst

    def call(self, op, lst, m, a, b):
        this = self.A["this"]
        ent = dict(wmf=F_WMF, show=F_SHOW, hide=F_HIDE, pcc=F_CC, pbt=F_BT, dcc=D_CC, dbt=D_BT)[op]
        args = [this]
        if op == "wmf":
            args += [m & MASK, a & MASK, b & MASK]
        if op in ("pcc", "dcc"):
            args.append(lst)
        T.call_ppc(self.e, ent, args)

    def fields(self):
        this = self.A["this"]
        r = self.rd_word
        return [r(this + 4), 1 if r(this + 0x70) else 0, r(this + 0x74), r(this + 0x78), r(this + 0x68),
                r(self.A["gl"] + 0xC), r(self.A["fl"] + 0xC)]

    def glob(self):
        v = self.rd_word(G_SYS)
        sel = self.rd_word(G_SEL)
        return [self.rd_word(self.disp), 0 if v == 0 else (1 if v == self.A["sys"] else 2),
                -2 if sel == SENT else self.lab_of(sel)]


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_pg_reset()
        self.A = {}
        self.make(case)
        CL.host_begin(lib, {LAB[n]: a for n, a in self.A.items()}, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_pg_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def make(self, case):
        lib, A = self.lib, self.A
        A["this"] = lib.tpw_pg_make(case["kind"])
        A["gl"] = lib.tpw_pg_make(2)
        A["fl"] = lib.tpw_pg_make(3)
        lib.tpw_pg_link_globe(A["this"], A["gl"], A["fl"])
        for i in range(3):
            A["g%d" % i] = lib.tpw_pg_make(4)
            A["f%d" % i] = lib.tpw_pg_make(5)
        for i in case["gscript"]:
            lib.tpw_pg_node(0, A["gl"], A["g%d" % (i - 1)] if i else 0)
        for i in case["fscript"]:
            lib.tpw_pg_node(1, A["fl"], A["f%d" % (i - 1)] if i else 0)
        for i in range(5):
            A["c%d" % i] = lib.tpw_pg_make(6)
        A["f70"] = lib.tpw_pg_make(12)
        lib.tpw_pg_link_flag(A["f70"], A["c4"])
        this = A["this"]
        lib.tpw_pg_set(this, 0, case["vis"])
        lib.tpw_pg_set(this, 2, case["f74"])
        lib.tpw_pg_set(this, 3, case["f78"])
        lib.tpw_pg_set(this, 4, case["f68"])
        lib.tpw_pg_set(this, 1, A["f70"] if case["hasf70"] else 0)
        A["theme"] = lib.tpw_pg_make(9)
        A["advisor"] = lib.tpw_pg_make(8)
        A["screens"] = lib.tpw_pg_make(7)
        lib.tpw_pg_link_screens(A["screens"], A["theme"], A["advisor"])
        A["meshes"] = lib.tpw_pg_make(10)
        A["sys"] = lib.tpw_pg_make(11)
        lib.tpw_pg_globals(A["screens"], A["meshes"], A["sys"] if case["sys"] else 0, case["gflags"], SENT)

    def city_list(self, n):
        arr = (C.c_uint64 * 8)(*[self.A["c%d" % i] for i in range(n)])
        return self.lib.tpw_pg_list(n, arr)

    def call(self, op, lst, m, a, b):
        L, this = self.lib, self.A["this"]
        if op == "wmf":
            L.tpw_pg_wmf(this, s32(m), s32(a), s32(b))
        elif op in ("pcc", "dcc"):
            getattr(L, "tpw_pg_" + op)(this, lst)
        else:
            getattr(L, "tpw_pg_" + {"show": "show", "hide": "hide", "pbt": "pbt", "dbt": "dbt"}[op])(this)

    def fields(self):
        g, this = self.lib.tpw_pg_get, self.A["this"]
        return [g(this, 0) & MASK, 1 if g(this, 1) & MASK else 0, g(this, 2) & MASK, g(this, 3) & MASK, g(this, 4) & MASK,
                g(this, 5) & MASK, g(this, 6) & MASK]

    def glob(self):
        sel = self.lib.tpw_pg_getglob(2)
        return [self.lib.tpw_pg_getglob(0), self.lib.tpw_pg_getglob(1), -2 if sel == SENT else self.lab_of(sel)]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, i32, i64, u32 = C.c_uint64, C.c_int32, C.c_int64, C.c_uint32

    def f(name, res, *args):
        fn = getattr(lib, "tpw_pg_" + name)
        fn.restype, fn.argtypes = res, list(args)

    f("alloc", u64, u32)
    f("reset", None)
    f("make", u64, i32)
    f("link_globe", None, u64, u64, u64)
    f("link_flag", None, u64, u64)
    f("link_screens", None, u64, u64, u64)
    f("set", None, u64, i32, i64)
    f("get", i64, u64, i32)
    f("globals", None, u64, u64, u64, u32, u64)
    f("getglob", u64, i32)
    f("node", None, i32, u64, u64)
    f("list", u64, i32, C.POINTER(C.c_uint64))
    f("wmf", None, u64, i32, i32, i32)
    for n in ("show", "hide", "pbt", "dbt"):
        f(n, None, u64)
    f("pcc", None, u64, u64)
    f("dcc", None, u64, u64)
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def run(s, case):
    try:
        lst = s.city_list(case["ncity"])
        s.call(case["op"], lst, *case["args"])
        return {"log": s.log(), "fields": s.fields(), "glob": s.glob()}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- the cases


def script(r):
    items = r.sample([1, 2, 3], r.randrange(0, 4))
    if r.random() < 0.3:
        items.insert(r.randrange(0, len(items) + 1), 0)
    return items


def base(r, op, kind):
    return {"op": op, "kind": kind,
            "queues": {"IsVisible": [r.choice([0, 1, 2, -1]), r.choice([0, 1, -5])],
                       "IsCityWanted": [r.choice([0, 1, 3, -1]) for _ in range(5)]},
            "vis": r.choice([0, 1, 7]), "hasf70": r.randrange(2), "f74": r.randrange(-5, 100), "f78": r.randrange(-5, 100),
            "f68": r.choice([0, 1, 5]), "gscript": script(r), "fscript": script(r), "sys": r.randrange(2), "gflags": r.getrandbits(32),
            "ncity": r.randrange(0, 5), "args": (0x10005, 0, r.randrange(-3, 3))}


def mod(c, **kw):
    c.update(kw)
    return c


def wmf_cases(r):
    for m in (0x10005, 0x10004, 0x10006, 0x5, 0, -1, 0x10005 + 0x10000):
        for a in (0, 1, -1):
            for h in (0, 1):
                yield mod(base(r, "wmf", 0), hasf70=h, args=(m, a, r.randrange(-3, 3)))


def wmf_rnd(r):
    c = base(r, "wmf", 0)
    c["args"] = (r.choice([0x10005, 0x10005, 0x10004, r.getrandbits(32)]), r.choice([0, 0, 1, r.getrandbits(32)]), r.getrandbits(32))
    return c


def show_cases(r):
    for v in (0, 1, 2, -1, 0x100):
        yield mod(base(r, "show", 0), queues={"IsVisible": [v]})


def show_rnd(r):
    return mod(base(r, "show", 0), queues={"IsVisible": [r.choice([0, 0, 1, -1, r.getrandbits(32)])]})


def hide_cases(r, op="hide"):
    for v1 in (0, 1, 2, -1):
        for v2 in (0, 1, -1):
            for sys in (0, 1):
                yield mod(base(r, op, 0), queues={"IsVisible": [v1, v2]}, sys=sys)
    for gc in range(0, 4):
        for fc in range(0, 4):
            yield mod(base(r, op, 0), queues={"IsVisible": [1, 1]}, gscript=r.sample([1, 2, 3], gc), fscript=r.sample([1, 2, 3], fc))
    yield mod(base(r, op, 0), queues={"IsVisible": [1, 1]}, gscript=[1, 0, 2], fscript=[0, 3, 0])
    yield mod(base(r, op, 0), queues={"IsVisible": [1, 1]}, gscript=[], fscript=[])
    yield mod(base(r, op, 0), queues={"IsVisible": [1, 1]}, gflags=0xFFFFFFFF)
    yield mod(base(r, op, 0), queues={"IsVisible": [1, 1]}, gflags=0x02000000)
    yield mod(base(r, op, 0), queues={"IsVisible": [1, 1]}, gflags=0)


def hide_rnd(r, op="hide"):
    return mod(base(r, op, 0), queues={"IsVisible": [r.choice([0, 1, 1, 1, -1]), r.choice([0, 1, 1, -1])]})


def cc_cases(r, op, kind):
    for n in range(0, 5):
        for pat in ([0], [1], [1, 0, 1, 0, 1], [0, 1, 0, 1, 0], [1, 1, 1, 1, 1], [3, -1, 0, 2, 1]):
            yield mod(base(r, op, kind), ncity=n, queues={"IsCityWanted": pat, "IsVisible": [0]})


def cc_rnd(r, op, kind):
    return base(r, op, kind)


def bt_cases(r, op, kind):
    for v1 in (0, 1):
        for v2 in (0, 1):
            for sys in (0, 1):
                yield mod(base(r, op, kind), queues={"IsVisible": [v1, v2]}, sys=sys)
    yield mod(base(r, op, kind), queues={"IsVisible": [1, 1]}, gscript=[2, 1, 3], fscript=[3, 1])


def bt_rnd(r, op, kind):
    return mod(base(r, op, kind), queues={"IsVisible": [r.choice([0, 1, 1]), r.choice([0, 1, 1])]})


def gen(fn, seed, *extra):
    def edges():
        return fn(random.Random(seed), *extra)
    return edges


def rnd_of(fn, *extra):
    return lambda r: fn(r, *extra)


P = "CLBScreenComponentPublishGlobe::"
D = "CLBScreenComponentDownloadGlobe::"
TESTS = [
    FnTest(P + "WindowMessagesFn", ppc, host, edge=gen(wmf_cases, 11), rnd=wmf_rnd),
    FnTest(P + "Show", ppc, host, edge=gen(show_cases, 12), rnd=show_rnd),
    FnTest(P + "Hide", ppc, host, edge=gen(hide_cases, 13), rnd=hide_rnd),
    FnTest(P + "CreateCities", ppc, host, edge=gen(cc_cases, 14, "pcc", 0), rnd=rnd_of(cc_rnd, "pcc", 0)),
    FnTest(P + "HandleOnlineBacktrack", ppc, host, edge=gen(bt_cases, 15, "pbt", 0), rnd=rnd_of(bt_rnd, "pbt", 0)),
    FnTest(D + "HandleOnlineBacktrack", ppc, host, edge=gen(bt_cases, 16, "dbt", 1), rnd=rnd_of(bt_rnd, "dbt", 1)),
    FnTest(D + "CreateCities", ppc, host, edge=gen(cc_cases, 17, "dcc", 1), rnd=rnd_of(cc_rnd, "dcc", 1)),
]
