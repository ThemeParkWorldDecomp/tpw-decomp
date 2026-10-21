"""Unit v5639, the online islands screen's CreateParks, Show and ProcessCancel, against
libtpw_unit_CLBScreenComponentOnlineIslands_CreateParks.dylib (tests/unit_CLBScreenComponentOnlineIslands_CreateParks.cpp), main app
("SimTheme Park"), src/game/online/lobby_island.cpp:
  CLBScreenComponentOnlineIslands::CreateParks 0x100b4880, Show 0x100b3ec8, ProcessCancel 0x100b3cd0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, native pointers) for the translation; the same code reads both back (the
members are reached by a per-side offset table; the host's comes from the library). The callees outside the unit are fakes on both sides
that record their arguments: the screen's two virtual calls (IsVisible, slot 0x30), the lobby view/meshes/advisor calls, the online
system (the lazily made one included), the sound calls, the string class (a string's text is its hash in the log), the theme names and
the gate names (the fakes derive them from the theme number), ParseThemeDescriptor.

The world of a case: the screen (camera state, current/last island, flags), its islands and their sounds, the lobby globals (view, meshes,
advisor, park selection, sound table), the online system (absent, or present with a theme list, a top-10 list and a busy word) and the park
list. What is compared: the answer of ProcessCancel, the screen's words, the islands' sound words, the view's words, the online system
global and what a made system holds, and the event log.

No fused operations. Not tested: CreateParks with an empty island list (the original reads the list head unchecked), ProcessCancel with
camera state 2 and no current island (the same unchecked read).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CLBScreenComponentOnlineIslands_CreateParks"
MASK = 0xFFFFFFFF

FN_CREATE, FN_SHOW, FN_CANCEL = 0x100B4880, 0x100B3EC8, 0x100B3CD0
SIZES = {FN_CREATE: 440, FN_SHOW: 620, FN_CANCEL: 272}
OWN = set(SIZES)

# callees (the asm's bl targets)
A_CREATE_CITY, A_SHOW_CITY = 0x101E05E4, 0x101E0858
A_CLEARALL, A_SHOWHEIGHT = 0x100A7128, 0x100A82B0
A_AMBIENT, A_DIRECTIONAL, A_FOGON, A_FOGSET = 0x100A9428, 0x100A94F8, 0x100A992C, 0x100A99B4
A_SYSCTOR, A_STOP, A_GETLIST = 0x1016F7D0, 0x10171404, 0x1017925C
A_SNDEVENT, A_SNDTICK, A_PLAYANIM = 0x100DD9FC, 0x100DD668, 0x100A7D28
A_THEMENAME, A_GATES, A_PARSE, A_UNISTR = 0x1017A588, 0x1014A768, 0x100B5D54, 0x100057A0
A_CTOR_W, A_CTOR_C, A_DTOR_C, A_DTOR_W, A_GLUE = 0x10238740, 0x102382D8, 0x102382F0, 0x10238710, 0x1023BAA8
TV_CALLBACK = 0x10266778
MAGIC_VIS, MAGIC_30 = 0xA000002C, 0xA0000030

ADDR = dict(gOnlineSystem=0x10380AB0, gLobbyScreens=0x102E49B8, gLobbyMeshes=0x102E48D0, gLobbyView=0x102E48CC, gLobby=0x102E4888,
            gOnlineParkSelection=0x102E4864, gLobbyIslandSounds=0x102F7CC0, gGame=0x1037F134)

NAMES = ["scr_vis", "scr_islands", "scr_cur", "scr_last", "scr_cam", "scr_fe4", "list_head", "isl_meshB", "isl_set", "isl_s1", "isl_s2",
         "park_theme", "sys_top10", "sys_theme", "sys_f384", "scrs_adv", "view_fov", "view_up", "view_right", "view_down",
         "lobby_globals", "glob_fov", "sel_id", "size_sys", "size_park", "size_screen"]
MAC = dict(scr_vis=4, scr_islands=8, scr_cur=0xC, scr_last=0x10, scr_cam=0x14, scr_fe4=0xE4, list_head=4, isl_meshB=8, isl_set=0x18,
           isl_s1=0x1C, isl_s2=0x20, park_theme=0x4C, sys_top10=0xE4, sys_theme=0xF8, sys_f384=0x384, scrs_adv=0x14, view_fov=4,
           view_up=0x28, view_right=0x2C, view_down=0x30, lobby_globals=0x10, glob_fov=4, sel_id=8)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def s32(v):
    return T.s32(v)


def hash_units(units):
    h = 0
    for u in units:
        h = (h * 31 + u) & MASK
    return h


CUR = {}          # the PowerPC side of the running case (the constructor fake of the online system writes into it)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def reg(addr, fn):
    STUBS.internal(addr, fn)


def wide_units(e, a):
    out = []
    while a:
        u = struct.unpack(">H", e.read(a, 2))[0]
        if u == 0:
            break
        out.append(u)
        a += 2
    return out


def cunits(e, a):
    return [ord(c) for c in e.cstr(a)] if a else []


def simple(addr, name, spec):
    reg(addr, CL.recorder(name, spec))


simple(A_CREATE_CITY, "CreateCityLobbyController", "")
simple(A_SHOW_CITY, "ShowCityLobbyController", "")
simple(A_CLEARALL, "ClearAll", "L")
simple(A_SHOWHEIGHT, "ShowHeightfield", "L")
simple(A_FOGON, "SetFogOn", "L")
simple(A_STOP, "StopProcessing", "L")
simple(A_SNDTICK, "SOUND_Tick", "")
simple(A_PLAYANIM, "PlayAnim", "Lii")
simple(A_DTOR_C, "StrDtorC", "L")
simple(A_DTOR_W, "StrDtorW", "L")

_ctor_w = CL.recorder("StrCtorW", "N")
_ctor_c = CL.recorder("StrCtorC", "N")
_sysctor = CL.recorder("NewOnlineSystem", "N")


def stub_ctor_w(e):
    _ctor_w(e)


def stub_ctor_c(e):
    _ctor_c(e)
    e.cl.rec("StrText", (hash_units(cunits(e, e.r(4))),))
    e.write(e.r(3) + 8, struct.pack(">I", e.r(4)))        # mData: the host's fake keeps the pointer too


def stub_sysctor(e):
    _sysctor(e)
    s = CUR["s"]
    a = e.r(3)
    for name, v in (("sys_theme", s.w["themeList"]), ("sys_top10", s.w["top10"])):
        s.wp(a + s.O[name], v)
    s.wi(a + s.O["sys_f384"], s.w["f384"])


def stub_ambient(e):
    L = e.cl
    L.rec("SetGlobalAmbientLight", (L.lab(e.r(3)), s32(e.r(4)), e.r(5) & MASK))


def stub_directional(e):
    L = e.cl
    color = e.u32(e.r(5)) if e.r(5) else 0
    L.rec("SetGlobalDirectionalLight", (L.lab(e.r(3)), s32(e.r(4)), color, 1 if e.r(6) else 0, s32(e.r(7))))


def stub_fogset(e):
    L = e.cl
    L.rec("SetFogSettings", (L.lab(e.r(3)), e.r(4) & MASK, fb(e.f(1)), fb(e.f(2))))


def stub_getlist(e):
    L = e.cl
    L.rec("GetList", (L.lab(e.r(3)), s32(e.r(4)), 1 if e.r(5) == TV_CALLBACK else 0, L.lab(e.r(6))))


def stub_sndevent(e):
    L = e.cl
    L.rec("SOUND_SendEvent", tuple(s32(e.r(i)) for i in range(3, 9)))
    e.set_r(3, L.ret("SOUND_SendEvent") & MASK)


def stub_themename(e):
    L = e.cl
    idx = s32(e.r(4))
    L.rec("ReturnOnlineThemeName", (L.lab(e.r(3)), idx))
    e.set_r(3, e.alloc(16, ("T%d" % idx).encode() + b"\0"))


def put_wide(e, out, text):
    buf = e.alloc(2 * (len(text) + 1), text.encode("utf-16-be") + b"\0\0")
    e.write(out + 8, struct.pack(">I", buf))


def stub_unistr(e):
    L = e.cl
    out, inn = e.r(3), e.r(4)
    L.rec("StringToUniString", (L.lab(out), L.lab(inn)))
    put_wide(e, out, e.cstr(e.u32(inn + 8)))
    e.set_r(3, 0)


def stub_gates(e):
    L = e.cl
    game, theme, g1, g2 = e.r(3), e.r(4), e.r(5), e.r(6)
    units = wide_units(e, e.u32(theme + 8))
    L.rec("GetLocalisedThemeGateNames", (L.lab(game), L.lab(theme), L.lab(g1), L.lab(g2), hash_units(units)))
    text = "".join(chr(u) for u in units)
    put_wide(e, g1, text + "g1")
    put_wide(e, g2, text + "g2")


def stub_parse(e):
    L = e.cl
    L.rec("ParseThemeDescriptor", (L.lab(e.r(3)), L.lab(e.r(5)), s32(e.r(6)), hash_units(cunits(e, e.r(4))),
                                    hash_units(wide_units(e, e.r(7))), hash_units(wide_units(e, e.r(8)))))


def stub_glue(e):
    L = e.cl
    tv = e.r(12)
    e.write(e.r(1) + 20, struct.pack(">I", e.r(2)))       # the real glue saves the caller's TOC for its `lwz r2, 20(r1)`
    if tv == MAGIC_VIS:
        L.rec("IsVisible", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("IsVisible") & MASK)
    elif tv == MAGIC_30:
        L.rec("Slot30", (L.lab(e.r(3)),))
    else:
        raise AssertionError("unexpected __ptr_glue target %x" % tv)


def stub_new(e):
    size = e.r(3)
    assert size == 0x404, size          # the online system: the only allocation of these functions
    e.set_r(3, e.alloc(size))


for _a, _f in ((0x102384B8, stub_new), (A_CTOR_W, stub_ctor_w), (A_CTOR_C, stub_ctor_c), (A_SYSCTOR, stub_sysctor), (A_AMBIENT, stub_ambient),
               (A_DIRECTIONAL, stub_directional), (A_FOGSET, stub_fogset), (A_GETLIST, stub_getlist), (A_SNDEVENT, stub_sndevent),
               (A_THEMENAME, stub_themename), (A_UNISTR, stub_unistr), (A_GATES, stub_gates), (A_PARSE, stub_parse),
               (A_GLUE, stub_glue)):
    reg(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def wi(self, a, v):
        self.write(a, struct.pack(self.ifmt, v & MASK))

    def ri(self, a):
        return struct.unpack(self.ifmt, self.read(a, 4))[0]

    def wp(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rp(self, a):
        return struct.unpack(self.pfmt, self.read(a, self.pw))[0]


class Ppc(Side):
    ifmt, pfmt, pw = ">I", ">I", 4
    O = MAC

    def __init__(self, e, case):
        self.e = e
        self.saved = STUBS.activate(e)
        CUR["s"] = self

    def begin(self, fixed, case):
        self.L = self.e.cl = CL.PpcLog(fixed, case["queues"])

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

    def mk_screen(self):
        a = self.alloc(0x100)
        vt = self.alloc(0x80)
        self.wi(vt + 0x2C, MAGIC_VIS)
        self.wi(vt + 0x30, MAGIC_30)
        self.wi(a, vt)
        return a

    def prepare(self, themeList, top10, f384):
        pass

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, args)
        return r3 & MASK


class Host(Side):
    ifmt, pfmt, pw = "<I", "<Q", 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = lib.O

    def begin(self, fixed, case):
        CL.host_begin(self.lib, {lab: a for a, lab in fixed.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_oi_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_oi_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def mk_screen(self):
        return self.alloc(0x400)

    def prepare(self, themeList, top10, f384):
        self.lib.tpw_oi_prepare(themeList, top10, s32(f384))

    def call(self, fn, args):
        lib = self.lib
        if fn == FN_CREATE:
            lib.tpw_oi_create_parks(args[0], args[1])
            return 0
        if fn == FN_SHOW:
            lib.tpw_oi_show(args[0])
            return 0
        return lib.tpw_oi_process_cancel(args[0]) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_oi_addr.restype, lib.tpw_oi_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_oi_alloc.restype, lib.tpw_oi_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_oi_prepare.argtypes = [C.c_uint64, C.c_uint64, C.c_int32]
    lib.tpw_oi_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_oi_create_parks.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_oi_show.argtypes = [C.c_uint64]
    lib.tpw_oi_process_cancel.restype, lib.tpw_oi_process_cancel.argtypes = C.c_uint32, [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_oi_layout(out)
    assert n == len(NAMES), n
    lib.O = dict(zip(NAMES, list(out)))
    # the structures the module allocates are never smaller than the host's
    assert lib.O["size_sys"] <= 0x2000 and lib.O["size_park"] <= 0x400 and lib.O["size_screen"] <= 0x400, lib.O
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def make_list(s, items):
    """A TbPtrList of the given addresses (header, nodes of data/next/prev)."""
    pw = s.pw
    head = s.alloc(0x40)
    nodes = [s.alloc(3 * pw) for _ in items]
    for i, (n, it) in enumerate(zip(nodes, items)):
        s.wp(n, it)
        s.wp(n + pw, nodes[i + 1] if i + 1 < len(nodes) else 0)
        s.wp(n + 2 * pw, nodes[i - 1] if i > 0 else 0)
    s.wp(head + s.O["list_head"], nodes[0] if nodes else 0)
    return head


def build(s, case):
    A, O = s.alloc, s.O
    w = s.w = {}
    screen = w["screen"] = s.mk_screen()
    w["themeList"], w["top10"] = A(0x100), A(0x100)
    w["f384"] = case["f384"]
    advisor, meshes, view = A(0x100), A(0x100), A(0x100)
    islands = []
    for i, isl in enumerate(case["islands"]):
        a = A(0x100)
        mesh = A(0x100)
        s.wp(a + O["isl_meshB"], mesh)
        s.wi(a + O["isl_set"], isl["set"])
        s.wi(a + O["isl_s1"], isl["s1"])
        s.wi(a + O["isl_s2"], isl["s2"])
        islands.append((a, mesh))
    parks = []
    for t in case["parks"]:
        p = A(0x400)
        s.wi(p + O["park_theme"], t)
        parks.append(p)
    sys_a = 0
    if case["sys"]:
        sys_a = A(0x2000)
        s.wp(sys_a + O["sys_theme"], w["themeList"])
        s.wp(sys_a + O["sys_top10"], w["top10"])
        s.wi(sys_a + O["sys_f384"], case["f384"])
    w["sys"] = sys_a
    w["islands"] = [a for a, _ in islands]
    list_islands = make_list(s, w["islands"])
    list_parks = make_list(s, parks)
    w["parks"] = list_parks
    # ids for the observation
    ids = s.ids = {w["themeList"]: "theme", w["top10"]: "top10", 0: "null"}
    for i, a in enumerate(w["islands"]):
        ids[a] = "I%d" % i
    fixed = {screen: 1, w["themeList"]: 3, w["top10"]: 4, advisor: 5, meshes: 6, view: 7, s.g("gGame"): 8}
    if sys_a:
        fixed[sys_a] = 2
    for i, p in enumerate(parks):
        fixed[p] = 10 + i
    for i, (_, m) in enumerate(islands):
        fixed[m] = 30 + i
    s.begin(fixed, case)
    s.prepare(w["themeList"], w["top10"], case["f384"])

    def cur(idx):
        return w["islands"][idx] if idx >= 0 else 0

    s.wi(screen + O["scr_vis"], case["vis"])
    s.wp(screen + O["scr_islands"], list_islands)
    s.wp(screen + O["scr_cur"], cur(case["cur"]))
    s.wp(screen + O["scr_last"], cur(case["last"]))
    s.wi(screen + O["scr_cam"], case["cam"])
    s.wi(screen + O["scr_fe4"], case["fe4"])
    # globals
    s.wp(s.g("gOnlineSystem"), sys_a)
    scrs = A(0x100)
    s.wp(scrs + O["scrs_adv"], advisor)
    s.wp(s.g("gLobbyScreens"), scrs)
    s.wp(s.g("gLobbyMeshes"), meshes)
    s.wp(s.g("gLobbyView"), view)
    for name, v in zip(("view_fov", "view_up", "view_right", "view_down"), case["view"]):
        s.wi(view + O[name], v)
    w["view"] = view
    lobby, glob = A(0x100), A(0x100)
    s.wp(lobby + O["lobby_globals"], glob)
    s.wi(glob + O["glob_fov"], case["fov"])
    s.wp(s.g("gLobby"), lobby)
    sel = A(0x40)
    s.wi(sel + O["sel_id"], case["selid"])
    s.wp(s.g("gOnlineParkSelection"), sel)
    table = A(0x40)
    for i, v in enumerate(case["sounds"]):
        s.wi(table + 4 * i, v)
    s.wp(s.g("gLobbyIslandSounds"), table)


def execute(s, case):
    w = s.w
    if case["op"] == "create":
        return s.call(FN_CREATE, [w["screen"], w["parks"]])
    if case["op"] == "show":
        return s.call(FN_SHOW, [w["screen"]])
    return s.call(FN_CANCEL, [w["screen"]])


def observe(s, case, answer):
    O, w = s.O, s.w
    screen = w["screen"]
    pid = lambda a: s.ids.get(a, "other")
    out = {"answer": answer if case["op"] == "cancel" else None, "log": s.log()}
    out["screen"] = [s.ri(screen + O["scr_vis"]), pid(s.rp(screen + O["scr_cur"])), pid(s.rp(screen + O["scr_last"])),
                     s.ri(screen + O["scr_cam"]), s.ri(screen + O["scr_fe4"]), pid(s.rp(screen + O["scr_islands"])) if False else 0]
    out["islands"] = [[s.ri(a + O["isl_s1"]), s.ri(a + O["isl_s2"])] for a in w["islands"]]
    out["view"] = [s.ri(w["view"] + O[n]) for n in ("view_fov", "view_up", "view_right", "view_down")]
    sysp = s.rp(s.g("gOnlineSystem"))
    if sysp == 0:
        out["sys"] = "null"
    else:
        out["sys"] = "pre" if sysp == w["sys"] else "new"
        out["sysfields"] = [pid(s.rp(sysp + O["sys_theme"])), pid(s.rp(sysp + O["sys_top10"])), s.ri(sysp + O["sys_f384"])]
    return out


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

SPECIAL = [0, 0x80000000, 0x7FC00000, 0x7F800000, 0xFF800000, 0x00000001, 0x3F800000, 0xBF800000, 0x7F7FFFFF, 0x00800000]


def rw(r):
    return r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32), r.getrandbits(32), r.randrange(0, 1000)])


def rf(r):
    k = r.random()
    if k < 0.15:
        return r.choice(SPECIAL)
    return fb(r.uniform(-500, 500))


def base(r, op, **over):
    nisl = r.choice([1, 2, 3])
    c = {"op": op, "sys": r.choice([0, 1, 1]), "f384": r.choice([0, 0, 1, 7, -1]),
         "islands": [{"set": r.randrange(0, 4), "s1": rw(r), "s2": rw(r)} for _ in range(nisl)],
         "cur": r.choice([-1] + list(range(nisl))), "last": r.choice([-1] + list(range(nisl))),
         "cam": r.choice([0, 0, 1, 2, 2, 3, -1]), "vis": r.choice([0, 1, 5]), "fe4": r.choice([0, 1, 2]),
         "parks": [r.randrange(0, 6) for _ in range(r.randrange(0, 5))],
         "view": [rw(r) for _ in range(4)], "fov": rf(r), "selid": r.choice([0, -1, r.randrange(1, 100)]),
         "sounds": [rw(r) for _ in range(8)],
         "queues": {"IsVisible": [r.choice([0, 0, 1, -1, 3]), r.choice([0, 0, 1, -1])],
                    "SOUND_SendEvent": [rw(r), rw(r)]}}
    c.update(over)
    if op == "cancel" and c["cam"] == 2 and c["cur"] < 0:
        c["cur"] = 0
    return c


def create_cases(r):
    for n in range(0, 5):
        for sysm in (0, 1):
            yield base(r, "create", parks=[r.randrange(0, 6) for _ in range(n)], sys=sysm)


def show_cases(r):
    for vis in ([1, 0], [0, 1], [0, 0], [-1, 0], [0, -3]):
        for sysm in (0, 1):
            for cur in (-1, 0):
                yield base(r, "show", sys=sysm, cur=cur, queues={"IsVisible": vis, "SOUND_SendEvent": [rw(r), rw(r)]})


def cancel_cases(r):
    for cam in (0, 1, 2, 3, -1):
        for sysm in (0, 1):
            for f in (0, 1, -1):
                yield base(r, "cancel", cam=cam, sys=sysm, f384=f)


def make_edges(gen, seed):
    def edges():
        yield from gen(random.Random(seed))
    return edges


def make_rnd(op):
    def rnd(r):
        return base(r, op)
    return rnd


TESTS = [
    FnTest("CLBScreenComponentOnlineIslands::CreateParks", ppc, host, edge=make_edges(create_cases, 11), rnd=make_rnd("create")),
    FnTest("CLBScreenComponentOnlineIslands::Show", ppc, host, edge=make_edges(show_cases, 12), rnd=make_rnd("show")),
    FnTest("CLBScreenComponentOnlineIslands::ProcessCancel", ppc, host, edge=make_edges(cancel_cases, 13), rnd=make_rnd("cancel")),
]
