"""The online globes' list of globes against libtpw_unit_CLBGlobe_Register.dylib (tests/unit_CLBGlobe_Register.cpp), main app
("SimTheme Park"), include/lobby.h and src/game/online/lobby_globe.cpp:
  Register 0x100adff4 (slot 0x38), UnRegister 0x100ae064 (slot 0x3c), CLBGlobe::~CLBGlobe 0x100ac534, the online, download and publish
  globes' Hide 0x100ada68, 0x100ae63c, 0x100aece8 and destructors 0x100aca4c, 0x100af330, 0x100af1c8.

The real bodies run on both sides, and everything is reached through vtables: on the PowerPC side the slot words of the Mac's own vtables and
the real __ptr_glue (so the real list functions, the real ~CLBGlobe and the real slot 0x3c run), on the host side the C++ virtual calls on the
real classes. The slots were empty stand-ins for a long time and a recording fake could not tell, so no slot is faked here.

A case is a screen and a script of operations on it: reg and unreg (the two slots), del (a globe through slot 8 of its vtable with the delete
flag), clear (DeleteGlobeListEntries), hide (slot 0x28), vis (the word IsVisible reads, as a Show would leave it), destroy (slot 8 of the
screen with the delete flag, always last). After each operation the visible word, the two sounds, the display's flags, the list (count, then its
entries by label from the head and from the tail) and the blocks that operator new and operator delete handled are compared; at the end
so is the log (the labelled blocks deleted, the base destructor, the controllers, the meshes, the sound, SetPublishing).

Not tested: the city flags' list (empty here); the host's Show is covered by test_CLBScreenComponentOnlineGlobe_Show_2, which proves it calls slot 0x38.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CLBGlobe_Register"
MASK = 0xFFFFFFFF

VT_ONLINE, VT_DOWNLOAD, VT_GLOBE, VT_GLOBELIST, VT_FLAGLIST = 0x1029E830, 0x1029E7DC, 0x1029E8E8, 0x1029E8C0, 0x1029E898
VT_PUBLISH_WORD = 0x10261050   # the word the publish globe's constructor stores
REGISTER, UNREGISTER, GLOBE_DTOR, IS_VISIBLE = 0x100ADFF4, 0x100AE064, 0x100AC534, 0x100A6FB8
HIDES = (0x100ADA68, 0x100AE63C, 0x100AECE8)
DTORS = (0x100ACA4C, 0x100AF330, 0x100AF1C8)
CLEAR = 0x100ADF48
GLUE, BASE_DTOR = 0x1023BAA8, 0x100B6C2C
REMOVE_WORLD, REMOVE_OVERLAY, DELETE_MESH, FADE, SETPUB = 0x1022B6AC, 0x101E2314, 0x100A88A4, 0x100DE0FC, 0x10174E4C
G_MESHES, G_SYS, G_DISPLAY_SLOT = 0x102E48D0, 0x10380AB0, 0x1026067C

SCREEN_LABEL, MESH_LABEL, SYS_LABEL, FIRST_GLOBE = 1, 4, 5, 10
TABLES = {}    # class index -> the screen's vtable

STUBS = CL.Stubs()
STUBS.real.update({REGISTER, UNREGISTER, GLOBE_DTOR, IS_VISIBLE, CLEAR, GLUE, 0x100AF910, 0x100AF9B0, 0x100AFA1C, 0x100AFAC0, 0x100AFB98, 0x100AFC60})
STUBS.real.update(HIDES)
STUBS.real.update(DTORS)


def op_new(e):
    size = e.r(3)
    e.set_r(3, e.alloc(max(size, 4)))
    e.cl.news += 1


def op_delete(e):
    block = e.r(3)
    if block != 0:
        e.cl.dels += 1
        if e.cl.lab(block) > 0:
            e.cl.rec("delete", (e.cl.lab(block),))


def stub_setpub(e):
    e.cl.rec("SetPublishing", (e.cl.lab(e.r(3)), T.s32(e.r(4)), int(e.r(5) != 0), int(e.r(6) != 0)))


STUBS.imp("__nw__FUl", op_new)
STUBS.imp("__dl__FPv", op_delete)   # through the real glue 0x10238218, which keeps r2 for its caller
STUBS.internal(BASE_DTOR, lambda e: e.cl.rec("baseDtor", (e.cl.lab(e.r(3)),)))
STUBS.internal(REMOVE_WORLD, lambda e: e.cl.rec("RemoveWorldLobbyController", ()))
STUBS.internal(REMOVE_OVERLAY, lambda e: e.cl.rec("RemoveOnlineCityInfoOverlay", ()))
STUBS.internal(DELETE_MESH, lambda e: e.cl.rec("DeleteMesh", (e.cl.lab(e.r(3)),)))
STUBS.internal(FADE, lambda e: e.cl.rec("Fade", (T.s32(e.r(3)),)))
STUBS.internal(SETPUB, stub_setpub)


def slot(e, table, offset):
    return e.u32(e.u32(table + offset))


def setup(runner):
    T.install_std_stubs(runner.emu)
    e = runner.emu
    TABLES.update({0: VT_ONLINE, 1: VT_DOWNLOAD, 2: e.u32(VT_PUBLISH_WORD)})
    # The slots are taken from the Mac's own tables: all three classes share the base's 0x38, 0x3c and 0x2c.
    for cls, table in TABLES.items():
        assert slot(e, table, 0x38) == REGISTER, hex(slot(e, table, 0x38))
        assert slot(e, table, 0x3C) == UNREGISTER, hex(slot(e, table, 0x3C))
        assert slot(e, table, 0x2C) == IS_VISIBLE, hex(slot(e, table, 0x2C))
        assert slot(e, table, 0x28) == HIDES[cls], hex(slot(e, table, 0x28))
        assert slot(e, table, 8) == DTORS[cls], hex(slot(e, table, 8))
    assert slot(e, VT_GLOBE, 8) == GLOBE_DTOR, hex(slot(e, VT_GLOBE, 8))
    STUBS.real.add(slot(e, VT_FLAGLIST, 8))


class Ppc:
    def __init__(self, e, case):
        self.e = e
        self.table = TABLES[case["cls"]]
        self.screen = e.alloc(0x7C)
        self.glist, self.flist, self.mesh, self.sys = e.alloc(16), e.alloc(16), e.alloc(16), e.alloc(16)
        self.globes = [e.alloc(0x18) for _ in range(case["globes"])]
        labels = {self.screen: SCREEN_LABEL, self.mesh: MESH_LABEL, self.sys: SYS_LABEL}
        labels.update({g: FIRST_GLOBE + i for i, g in enumerate(self.globes)})
        self.L = e.cl = CL.PpcLog(labels, {})
        self.L.news = self.L.dels = 0
        self.saved = STUBS.activate(e)
        self.words(self.screen, [self.table, case["vis"], self.glist])
        self.words(self.screen + 0x6C, [self.flist, 0, case["f74"], case["f78"]])
        self.words(self.glist, [VT_GLOBELIST, 0, 0, 0])
        self.words(self.flist, [VT_FLAGLIST, 0, 0, 0])
        for g in self.globes:
            self.words(g, [VT_GLOBE, 0, self.screen])
        self.words(G_MESHES, [self.mesh])
        self.words(G_SYS, [self.sys])
        self.display = e.u32(G_DISPLAY_SLOT)
        self.words(self.display, [case["dflags"]])

    def words(self, a, ws):
        self.e.write(a, b"".join(struct.pack(">I", w & MASK) for w in ws))

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def ptr(self, index):
        return self.globes[index] if index >= 0 else 0

    def vcall(self, table, offset, *args):
        T.call_ppc(self.e, slot(self.e, table, offset), [a & MASK for a in args])

    def do(self, op):
        name = op[0]
        if name == "reg":
            self.vcall(self.table, 0x38, self.screen, self.ptr(op[1]))
        elif name == "unreg":
            self.vcall(self.table, 0x3C, self.screen, self.ptr(op[1]))
        elif name == "del":
            self.vcall(VT_GLOBE, 8, self.ptr(op[1]), 1)
        elif name == "clear":
            T.call_ppc(self.e, CLEAR, [self.screen])
        elif name == "hide":
            self.vcall(self.table, 0x28, self.screen)
        elif name == "vis":
            self.words(self.screen + 4, [op[1]])
        elif name == "destroy":
            self.vcall(self.table, 8, self.screen, 1)

    def state(self):
        e = self.e
        L = self.L
        s32 = lambda a: T.s32(e.u32(a))
        out = [s32(self.screen + 4), s32(self.screen + 0x74), s32(self.screen + 0x78), s32(self.display), s32(self.glist + 0xC)]
        node = e.u32(self.glist + 4)
        while node:
            out.append(L.lab(e.u32(node)))
            node = e.u32(node + 4)
        out.append(-1)
        node = e.u32(self.glist + 8)
        while node:
            out.append(L.lab(e.u32(node)))
            node = e.u32(node + 8)
        return out

    def heap(self):
        return [self.L.news, self.L.dels]

    def log(self):
        return self.L.log


class Host:
    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.screen = lib.tpw_rg_screen(case["cls"], T.s32(case["vis"]), T.s32(case["f74"]), T.s32(case["f78"]), case["dflags"])
        self.globes = [lib.tpw_rg_globe(self.screen) for _ in range(case["globes"])]
        fixed = {SCREEN_LABEL: self.screen, MESH_LABEL: lib.tpw_rg_addr(b"mesh"), SYS_LABEL: lib.tpw_rg_addr(b"sys")}
        fixed.update({FIRST_GLOBE + i: g for i, g in enumerate(self.globes)})
        CL.host_begin(lib, fixed, {})
        self.counters = (C.c_int32 * 4).in_dll(lib, "tpw_cl_heap")
        self.base = [self.counters[0], self.counters[2]]

    def done(self):
        pass

    def ptr(self, index):
        return self.globes[index] if index >= 0 else 0

    def do(self, op):
        name, lib = op[0], self.lib
        if name == "reg":
            lib.tpw_rg_reg(self.screen, self.ptr(op[1]))
        elif name == "unreg":
            lib.tpw_rg_unreg(self.screen, self.ptr(op[1]))
        elif name == "del":
            lib.tpw_rg_del(self.ptr(op[1]))
        elif name == "clear":
            lib.tpw_rg_clear(self.screen)
        elif name == "hide":
            lib.tpw_rg_hide(self.screen)
        elif name == "vis":
            lib.tpw_rg_visible(self.screen, T.s32(op[1]))
        elif name == "destroy":
            lib.tpw_rg_destroy(self.screen)

    def state(self):
        out = (C.c_int32 * 256)()
        n = self.lib.tpw_rg_read(self.screen, out)
        return list(out)[:n]

    def heap(self):
        return [self.counters[0] - self.base[0], self.counters[2] - self.base[1]]

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, s32 = C.c_uint64, C.c_int32
    lib.tpw_rg_addr.restype, lib.tpw_rg_addr.argtypes = u64, [C.c_char_p]
    lib.tpw_rg_screen.restype, lib.tpw_rg_screen.argtypes = u64, [s32, s32, s32, s32, C.c_uint32]
    lib.tpw_rg_globe.restype, lib.tpw_rg_globe.argtypes = u64, [u64]
    for name, args in (("reg", [u64, u64]), ("unreg", [u64, u64]), ("del", [u64]), ("clear", [u64]), ("hide", [u64]),
                       ("destroy", [u64]), ("visible", [u64, s32])):
        fn = getattr(lib, "tpw_rg_" + name)
        fn.restype, fn.argtypes = None, args
    lib.tpw_rg_read.restype, lib.tpw_rg_read.argtypes = s32, [u64, C.POINTER(s32)]
    lib.bound = True


def run(side, case):
    try:
        steps = []
        for op in case["script"]:
            side.do(op)
            steps.append((op, side.heap(), None if op[0] == "destroy" else side.state()))
        return {"steps": steps, "log": side.log()}
    finally:
        side.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


def world(cls, r, script, globes=6, vis=1):
    return {"cls": cls, "globes": globes, "vis": vis, "f74": r.getrandbits(32), "f78": r.getrandbits(32),
            "dflags": r.choice([0, 0x02000000, MASK, r.getrandbits(32)]), "script": script}


def edges_for(cls):
    def edges():
        r = random.Random(7300 + cls)
        reg = lambda *ids: [("reg", i) for i in ids]
        # The game's life: Show registers one globe, Hide deletes it, Show again, the lobby's end destroys the screen.
        yield world(cls, r, reg(0) + [("hide",)])
        yield world(cls, r, reg(0) + [("hide",), ("vis", 1)] + reg(1) + [("hide",), ("vis", 1)] + reg(2) + [("destroy",)])
        yield world(cls, r, reg(0) + [("destroy",)])
        yield world(cls, r, [("destroy",)])
        # A hidden screen deletes nothing; the second Hide finds nothing to do.
        yield world(cls, r, reg(0, 1) + [("hide",), ("destroy",)], vis=0)
        yield world(cls, r, reg(0, 1) + [("hide",), ("hide",)])
        # ~CLBGlobe takes a globe off the list wherever it is, and leaves the rest alone.
        yield world(cls, r, reg(0) + [("del", 0)])
        yield world(cls, r, reg(0, 1, 2) + [("del", 0), ("del", 2), ("del", 1)])
        yield world(cls, r, reg(0, 1, 2, 3) + [("del", 2), ("hide",)])
        yield world(cls, r, [("del", 0)])
        # UnRegister: found at the head, in the middle, at the tail, twice, never there, on an empty list.
        yield world(cls, r, [("unreg", 0)])
        yield world(cls, r, reg(0) + [("unreg", 1), ("unreg", 0), ("unreg", 0)])
        yield world(cls, r, reg(0, 1, 2) + [("unreg", 1), ("unreg", 0), ("unreg", 2)])
        # The same globe twice: the first node goes.
        yield world(cls, r, reg(0, 1, 0) + [("unreg", 0), ("unreg", 0), ("unreg", 1)])
        # An empty entry: the loops step over NULL.
        yield world(cls, r, reg(-1, 0, -1) + [("unreg", -1), ("hide",)])
        yield world(cls, r, reg(-1) + [("clear",), ("destroy",)])
        yield world(cls, r, reg(0, 1, 2, 3, 4, 5) + [("clear",), ("clear",)])
    return edges


def random_for(cls):
    def rnd(r):
        globes = r.randrange(1, 7)
        first_vis = vis = r.choice([0, 1, 1, 1, 5, MASK])
        script, live, entries = [], list(range(globes)), []
        for _ in range(r.randrange(1, 24)):
            kind = r.choice(["reg", "reg", "reg", "unreg", "del", "dup", "null", "clear", "hide", "vis"])
            if kind == "reg" and [g for g in live if g not in entries]:
                g = r.choice([g for g in live if g not in entries])
                script.append(("reg", g))
                entries.append(g)
            elif kind == "unreg" and live:
                g = r.choice(live)
                script.append(("unreg", g))
                if g in entries:
                    entries.remove(g)
            elif kind == "del" and live:
                g = r.choice(live)
                script.append(("del", g))
                live.remove(g)
                if g in entries:
                    entries.remove(g)
            elif kind == "dup" and live:
                # Twice on the list only for a moment: a second node must not outlive the globe.
                g = r.choice(live)
                if g in entries:
                    script += [("reg", g), ("unreg", g)]
                else:
                    script += [("reg", g), ("reg", g), ("unreg", g), ("unreg", g)]
            elif kind == "null":
                script.append(("reg", -1))
                entries.append(-1)
            elif kind == "clear" or (kind == "hide" and vis != 0):
                script.append((kind,))
                live = [g for g in live if g not in entries]
                entries = []
                if kind == "hide":
                    vis = 0
            elif kind == "hide":
                script.append(("hide",))
            elif kind == "vis":
                vis = r.choice([0, 1])
                script.append(("vis", vis))
        if r.random() < 0.5:
            script.append(("destroy",))
        return world(cls, r, script, globes, first_vis)
    return rnd


TESTS = [FnTest(name, ppc, host, edge=edges_for(cls), rnd=random_for(cls))
         for cls, name in enumerate(("CLBScreenComponentOnlineGlobe list of globes", "CLBScreenComponentDownloadGlobe list of globes",
                                     "CLBScreenComponentPublishGlobe list of globes"))]
