"""Differential tests for engine tb_ezsprite.cpp. See difftest.py and tpwtest.py.

CEZSprite drives classes of untranslated files (tb_starfield.h: CSpriteList,
SSpriteEngineData, SSpriteGameData; MapWho.h: CMapWho, CMapLinkBase). Both sides log
their calls: stub_internal on the PowerPC side, weak placeholders in
tests/shim_placeholders.cpp on the host. On the PowerPC side the CSpriteList "constructor"
lays the object out as the real one does (+0 pointer to the CMapLinkBase at +0x20, +0x1c
vtable pointer) with a fake vtable whose destructor slot logs the call, so `delete mList`
goes through the original's virtual call. Destructors called with a positive flag also
log the operator delete the real one would make.

Objects, arrays and allocations carry labels; every log compares as labels. Sizes of
allocated objects compare as the class constructed on the block (host classes with
pointers are larger). The engine allocator callbacks are the fakes of test_tb_capture
(blocks filled with 0xcd; free clears the caller's pointer as the game's MacFree does).
Asserts compare on both sides (msl_assertion_failed on the host).
"""
import ctypes
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "engine symbols shared"

MALLOC_FN, FREE_FN, MAPWHO_PTR, ERRORS, EZSPRITE = 0x10051f70, 0x10051f74, 0x100af618, 0x10052b44, 0x10052d48
FAKE_MAPWHO = 0x7C0F0000
ADDR = dict(sprite_ctor=0x1002e0e0, sprite_dtor=0x1002e138, sprite_init=0x1002e810, sprite_close=0x1002f00c,
            inst_ctor=0x1002f144, list_ctor=0x1003108c, list_init=0x10031378, link=0x10017b48, unlink=0x10017c58)
SIZES = {0x28: "CSpriteList", 0x18: "SSpriteEngineData", 0x44: "SSpriteGameData"}
VP, U32, I32 = ctypes.c_void_p, ctypes.c_uint32, ctypes.c_int32


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f

# ----------------------------------------------------------------- PowerPC fakes


def setup(runner):
    emu = runner.emu
    T.install_std_stubs(emu)
    emu.heap_in_scratch = True
    e = emu

    def malloc(e):
        size = e.r(3)
        e.ez_log.append(("malloc", size // 4))     # in pointer-sized entries (4 bytes here)
        if e.ez_malloc_fail:
            e.set_r(3, 0)
            return
        a = e.alloc(max(size, 4), bytes([0xCD]) * size if size else None)
        e.ez_labels[a] = "m%d" % len([x for x in e.ez_labels.values() if x.startswith("m")])
        e.ez_caps[e.ez_labels[a]] = size // 4
        e.set_r(3, a)

    def free(e):
        ref = e.r(3)
        e.ez_log.append(("free", e.ez_label(e.u32(ref))))
        e.write(ref, b"\0\0\0\0")

    emu.ez_tv = dict(malloc=T.fake_function(emu, malloc), free=T.fake_function(emu, free))

    def list_dtor(e):
        this, flag = e.r(3), T.s16(e.r(4))
        e.ez_calls.append((41, this))
        if flag > 0:
            e.alloc_log.append(("delete", this, 0))

    emu.ez_list_vtable = T.persist_alloc(emu, 0x20)
    emu.write(emu.ez_list_vtable + 8, struct.pack(">I", T.fake_function(emu, list_dtor)))

    def stub(name, fn):
        T.stub_internal(emu, ADDR[name], fn)

    stub("sprite_ctor", lambda e: e.ez_calls.append((20, e.r(3))))

    def sprite_dtor(e):
        e.ez_calls.append((21, e.r(3)))
        if T.s16(e.r(4)) > 0:
            e.alloc_log.append(("delete", e.r(3), 0))

    stub("sprite_dtor", sprite_dtor)

    def sprite_init(e):
        i = e.ez_init_i
        e.ez_init_i += 1
        e.ez_names.append(e.cstr(e.r(4)))
        e.ez_calls.append((22, e.r(3), i))
        res = e.ez_init[min(i, len(e.ez_init) - 1)] if e.ez_init else 1
        e.set_r(3, T.u32(res))

    stub("sprite_init", sprite_init)
    stub("sprite_close", lambda e: (e.ez_calls.append((23, e.r(3))), e.set_r(3, 1)))
    stub("inst_ctor", lambda e: e.ez_calls.append((30, e.r(3))))

    def list_ctor(e):
        this = e.r(3)
        e.ez_calls.append((40, this))
        e.write(this, struct.pack(">I", this + 0x20))
        e.write(this + 0x1c, struct.pack(">I", e.ez_list_vtable))

    stub("list_ctor", list_ctor)

    def list_init(e):
        this, n = e.r(3), T.s32(e.r(4))
        e.ez_calls.append((42, this, n))
        if e.ez_list_fail:
            e.set_r(3, 0)
            return
        a = e.alloc(max(4 * max(n, 0), 4) + 4 * 64)
        e.ez_labels[a] = "pool%d" % e.ez_pool
        e.ez_caps[e.ez_labels[a]] = max(n, 0) + 64
        e.ez_pool += 1
        e.set_r(3, a)

    stub("list_init", list_init)

    def link(ident):
        def fn(e):
            e.ez_calls.append((ident, e.r(3), e.r(4), T.s32(e.r(5))))
            e.set_r(3, T.u32(e.ez_link_result))
        return fn

    stub("link", link(11))
    stub("unlink", link(12))

# ----------------------------------------------------------------- the two sides
# A state: dict(flags, list (label or None), counts [maxI, maxS, nSprites, nInstances],
# sprites: list of labels in the sprite array (None = no array), instances: same, cap:
# array capacities, inst_fields: label -> (f10, f24, sprite label, [f2c, x, y, z])).


class PpcSide:
    def __init__(self, emu, c):
        self.emu = emu
        emu.ez_labels, emu.ez_log, emu.ez_calls, emu.ez_names, emu.ez_caps = {}, [], [], [], {}
        emu.alloc_log, emu.asserts = [], []
        emu.ez_init, emu.ez_init_i = c.get("init_results", []), 0
        emu.ez_list_fail, emu.ez_malloc_fail = c.get("list_fail", False), c.get("malloc_fail", False)
        emu.ez_link_result, emu.ez_pool = c.get("link_result", 1), 0
        emu.ez_label = lambda a: None if a == 0 else emu.ez_labels.get(a, "?%x" % a)
        emu.write(MALLOC_FN, struct.pack(">I", emu.ez_tv["malloc"]))
        emu.write(FREE_FN, struct.pack(">I", emu.ez_tv["free"]))
        emu.write(MAPWHO_PTR, struct.pack(">I", FAKE_MAPWHO if c.get("mapwho", True) else 0))
        emu.write(ERRORS, struct.pack(">128ii", *[0] * 128, c.get("errors", 0)))
        self.objs = {}

    def obj(self, label, kind):
        e = self.emu
        size = {"list": 0x28, "sprite": 0x18, "inst": 0x44}[kind]
        a = e.alloc(size, bytes([0x5A]) * size)
        if kind == "list":
            e.write(a, struct.pack(">I", a + 0x20))
            e.write(a + 0x1c, struct.pack(">I", e.ez_list_vtable))
        e.ez_labels[a] = label
        self.objs[label] = a
        return a

    def array(self, label, items, cap):
        e = self.emu
        a = e.alloc(4 * max(cap, 1), struct.pack(">%dI" % cap, *([self.objs[x] if x else 0 for x in items] +
                                                                 [0] * (cap - len(items)))))
        e.ez_labels[a] = label
        e.ez_caps[label] = cap
        return a

    def build(self, st, at=None):
        e = self.emu
        for lab, kind in st["objects"]:
            self.obj(lab, kind)
        for lab, fields in st["inst_fields"].items():
            a = self.objs[lab]
            f10, f24, spr, fl = fields
            e.write(a + 0x10, struct.pack(">i3f", f10, *fl[1:]))
            e.write(a + 0x24, struct.pack(">iIf", f24, self.objs[spr] if spr else 0, fl[0]))
        if st["list"]:
            e.write(self.objs[st["list"]] + 4, struct.pack(">i", st["list_count"]))
        inst = self.array("instarr", st["instances"], st["cap"][0]) if st["instances"] is not None else 0
        spr = self.array("sprarr", st["sprites"], st["cap"][1]) if st["sprites"] is not None else 0
        this = at if at is not None else e.alloc(0x20)
        e.write(this, struct.pack(">IIIIiiii", st["flags"], self.objs[st["list"]] if st["list"] else 0, inst, spr,
                                  *st["counts"]))
        e.ez_labels[this] = "self"
        return this

    def observe(self, this, ret):
        e = self.emu
        lab = e.ez_label
        news = {}
        for kind, a, size in e.alloc_log:
            if kind == "new":
                ctor = next((c[0] for c in e.ez_calls if c[1] == a and c[0] in (20, 30, 40)), None)
                name = {20: "sprite", 30: "inst", 40: "list"}.get(ctor, "?")
                e.ez_labels.setdefault(a, "new_%s%d" % (name, len(news)))
                news[a] = SIZES.get(size, size)
        allocs = []
        for kind, a, size in e.alloc_log:
            allocs.append(("new", news[a], lab(a)) if kind == "new" else ("delete", lab(a)))
        flags, lst, inst, spr, *counts = struct.unpack(">IIIIiiii", e.read(this, 0x20))

        def arr(a, n):
            n = max(0, min(n, 16, e.ez_caps.get(lab(a), 0)))   # never past the block
            return None if a == 0 else [("uninit" if x == 0xCDCDCDCD else lab(x))
                                        for x in struct.unpack(">%dI" % n, e.read(a, 4 * n))]
        out = dict(ret=ret(self) if callable(ret) else ret, flags=flags, list=lab(lst), counts=counts, inst=lab(inst),
                   spr=lab(spr), inst_items=arr(inst, counts[3]), spr_items=arr(spr, counts[2]), allocs=allocs)
        out["list_count"] = T.s32(e.u32(lst + 4)) if lst else None
        gone = {x[1] for x in allocs if x[0] == "delete"}
        objs = {}
        for a, name in list(e.ez_labels.items()):
            if name.startswith(("inst", "new_inst")) and not name.startswith("instarr") and name not in gone:
                f10, x, y, z = struct.unpack(">i3f", e.read(a + 0x10, 16))
                f24, sp, f2c = struct.unpack(">iIf", e.read(a + 0x24, 12))
                objs[name] = (f10, f24, lab(sp), [f2c, x, y, z] if name.startswith("inst") else [f2c])
        out["inst_fields"] = objs
        out["calls"] = [(c[0], "mapwho" if c[1] == FAKE_MAPWHO else lab(c[1])) +
                        tuple(("base:" + str(lab(x - 0x20)) if x and lab(x - 0x20) and c[0] in (11, 12) and i == 0
                               else (None if x == 0 and c[0] in (11, 12) and i == 0 else x))
                              for i, x in enumerate(c[2:])) for c in e.ez_calls]
        out["log"] = list(e.ez_log)
        out["names"] = list(e.ez_names)
        out["errors"] = struct.unpack(">i", e.read(ERRORS + 0x200, 4))[0], struct.unpack(">ii", e.read(ERRORS, 8))
        out["asserts"] = list(e.asserts)
        return out


class HostSide:
    def __init__(self, lib, c):
        self.lib = lib
        self.labels, self.objs, self.keep, self.caps = {}, {}, [], {}
        F(lib, "tpw_calls_reset", None)()
        F(lib, "tpw_sprites_reset", None)()
        F(lib, "tpw_cap_install", None)()
        F(lib, "tpw_cap_reset", None)()
        T.host_asserts(lib, reset=True)
        init = c.get("init_results", [])
        (I32 * 16).in_dll(lib, "tpw_sprite_init")[:len(init)] = init
        I32.in_dll(lib, "tpw_sprite_init_n").value = len(init)
        I32.in_dll(lib, "tpw_spritelist_init_fail").value = 1 if c.get("list_fail") else 0
        I32.in_dll(lib, "tpw_cap_malloc_fail_at").value = 0 if c.get("malloc_fail") else -1
        I32.in_dll(lib, "tpw_mapwho_result").value = c.get("link_result", 1)
        F(lib, "tpw_mapwho_set", None, ctypes.c_size_t)(FAKE_MAPWHO if c.get("mapwho", True) else 0)
        errs = ctypes.addressof(ctypes.c_char.in_dll(lib, "gei_ErrorHandler"))
        ctypes.memmove(errs, struct.pack("<128ii", *[0] * 128, c.get("errors", 0)), 0x204)
        self.errs = errs

    def lab(self, a):
        return None if not a else self.labels.get(a, "?%x" % a)

    def obj(self, label, kind):
        a = F(self.lib, {"list": "tpw_new_list", "sprite": "tpw_new_sprite", "inst": "tpw_new_instance"}[kind], VP)()
        self.labels[a] = label
        self.objs[label] = a
        return a

    def build(self, st, this=None):
        lib = self.lib
        for lab, kind in st["objects"]:
            self.obj(lab, kind)
        for lab, fields in st["inst_fields"].items():
            f10, f24, spr, fl = fields
            F(lib, "tpw_instance_set", None, VP, ctypes.POINTER(I32), VP, ctypes.POINTER(ctypes.c_float))(
                self.objs[lab], (I32 * 2)(f10, f24), self.objs[spr] if spr else None, (ctypes.c_float * 4)(*fl))
        if st["list"]:
            F(lib, "tpw_list_count", I32, VP, I32, I32)(self.objs[st["list"]], st["list_count"], 1)

        def array(label, items, cap):
            arr = (VP * max(cap, 1))(*([self.objs[x] if x else None for x in items] + [None] * (cap - len(items))))
            self.keep.append(arr)
            self.labels[ctypes.addressof(arr)] = label
            self.caps[label] = cap
            return ctypes.addressof(arr)
        inst = array("instarr", st["instances"], st["cap"][0]) if st["instances"] is not None else None
        spr = array("sprarr", st["sprites"], st["cap"][1]) if st["sprites"] is not None else None
        this = this if this is not None else F(lib, "tpw_ez_store", VP)()
        F(lib, "tpw_ez_set", None, VP, U32, VP, VP, VP, ctypes.POINTER(I32))(
            this, st["flags"], self.objs[st["list"]] if st["list"] else None, inst, spr, (I32 * 4)(*st["counts"]))
        self.labels[this] = "self"
        self.bases = {F(lib, "tpw_list_base", VP, VP)(self.objs[x]): x for x, kind in st["objects"] if kind == "list"}
        # the setup's constructor calls and allocations are not part of the case
        F(lib, "tpw_calls_reset", None)()
        F(lib, "tpw_alloc_reset", None)()
        I32.in_dll(lib, "tpw_alloc_log_on").value = 1
        return this

    def observe(self, this, ret):
        lib = self.lib
        I32.in_dll(lib, "tpw_alloc_log_on").value = 0
        flags, ptrs, counts = U32(), (VP * 3)(), (I32 * 4)()
        F(lib, "tpw_ez_get", None, VP, ctypes.POINTER(U32), ctypes.POINTER(VP), ctypes.POINTER(I32))(
            this, ctypes.byref(flags), ptrs, counts)
        # allocations first: they name the new objects
        n = I32.in_dll(lib, "tpw_alloc_n").value
        kinds = (I32 * 1024).in_dll(lib, "tpw_alloc_kind")
        aptr = (ctypes.c_uint64 * 1024).in_dll(lib, "tpw_alloc_ptr")
        rows = ((ctypes.c_uint64 * 8) * 64).in_dll(lib, "tpw_calls")
        self.new_bases = {rows[i][2]: rows[i][1] for i in range(I32.in_dll(lib, "tpw_calls_n").value) if rows[i][0] == 40}
        calls = host_calls(lib)
        allocs, news = [], {}
        for i in range(n):
            a = aptr[i]
            if kinds[i] in (1, 2):
                ctor = next((cl[0] for cl in calls if cl[1] == a and cl[0] in (20, 30, 40)), None)
                name = {20: "sprite", 30: "inst", 40: "list"}.get(ctor, "?")
                self.labels.setdefault(a, "new_%s%d" % (name, len(news)))
                news[a] = {20: "SSpriteEngineData", 30: "SSpriteGameData", 40: "CSpriteList"}.get(ctor, "?")
                allocs.append(("new", news[a], self.lab(a)))
            else:
                allocs.append(("delete", self.lab(a)))
        # the fake allocator's blocks
        cn = I32.in_dll(lib, "tpw_cap_log_n").value
        ck = (I32 * 64).in_dll(lib, "tpw_cap_log_kind")
        cs = (ctypes.c_uint64 * 64).in_dll(lib, "tpw_cap_log_size")
        cp = (VP * 64).in_dll(lib, "tpw_cap_log_ptr")
        log, k = [], 0
        for i in range(cn):
            if ck[i] == 1:
                if cp[i]:
                    self.labels[cp[i]] = "m%d" % k
                    self.caps["m%d" % k] = cs[i] // ctypes.sizeof(VP)
                    k += 1
                log.append(("malloc", cs[i] // ctypes.sizeof(VP)))   # in pointer-sized entries
            else:
                log.append(("free", self.lab(cp[i])))
        pool = (VP * 64 * 4).in_dll(lib, "tpw_spritelist_pool")
        for i in range(4):
            self.labels.setdefault(ctypes.addressof(pool[i]), "pool%d" % i)
            self.caps["pool%d" % i] = 64
        lab = self.lab
        lst, inst, spr = ptrs

        def arr(a, nn):
            nn = max(0, min(nn, 16, self.caps.get(lab(a), 0)))   # never past the block
            return None if not a else [("uninit" if x == 0xCDCDCDCDCDCDCDCD else lab(x))
                                       for x in (VP * nn).from_address(a)] if nn else ([] if a else None)
        out = dict(ret=ret(self) if callable(ret) else ret, flags=flags.value, list=lab(lst), counts=list(counts),
                   inst=lab(inst), spr=lab(spr), inst_items=arr(inst, counts[3]), spr_items=arr(spr, counts[2]))
        out["list_count"] = F(lib, "tpw_list_count", I32, VP, I32, I32)(lst, 0, 0) if lst else None
        objs = {}
        gone = {x[1] for x in allocs if x[0] == "delete"}
        for a, name in list(self.labels.items()):
            if name.startswith(("inst", "new_inst")) and not name.startswith("instarr") and name not in gone:
                ints, sp, fl = (I32 * 2)(), VP(), (ctypes.c_float * 4)()
                F(lib, "tpw_instance_get", None, VP, ctypes.POINTER(I32), ctypes.POINTER(VP),
                  ctypes.POINTER(ctypes.c_float))(a, ints, ctypes.byref(sp), fl)
                fls = [T.f32(x) for x in fl]
                objs[name] = (ints[0], ints[1], lab(sp.value), fls if name.startswith("inst") else fls[:1])
        out["inst_fields"] = objs
        out["allocs"] = allocs
        bases = dict(self.bases)
        for b, a in self.new_bases.items():       # lists built during the call (logged by the constructor)
            bases[b] = self.lab(a)
        out["calls"] = [(cl[0], "mapwho" if cl[1] == FAKE_MAPWHO else lab(cl[1])) +
                        tuple(("base:" + bases[x] if x in bases and cl[0] in (11, 12) and i == 0
                               else (None if x == 0 and cl[0] in (11, 12) and i == 0 else x))
                              for i, x in enumerate(cl[2:])) for cl in calls]
        out["log"] = log
        names = ((ctypes.c_char * 64) * 16).in_dll(lib, "tpw_sprite_names")
        out["names"] = [names[i].value.decode("latin-1") for i in range(I32.in_dll(lib, "tpw_sprite_init_i").value)]
        out["errors"] = struct.unpack("<i", ctypes.string_at(self.errs + 0x200, 4))[0], \
            struct.unpack("<ii", ctypes.string_at(self.errs, 8))
        out["asserts"] = T.host_asserts(lib, reset=True)
        return out


ARGS = {11: 2, 12: 2, 20: 0, 21: 0, 22: 1, 23: 0, 30: 0, 40: 0, 41: 0, 42: 1}


def host_calls(lib):
    n = I32.in_dll(lib, "tpw_calls_n").value
    rows = ((ctypes.c_uint64 * 8) * 64).in_dll(lib, "tpw_calls")
    out = []
    for i in range(n):
        r = list(rows[i])
        k = ARGS.get(r[0], 0)
        args = [r[2 + j] for j in range(k)]
        if r[0] in (11, 12):
            args = [r[2], T.s32(r[3])]
        elif r[0] == 42:
            args = [T.s32(r[2])]
        out.append((r[0], r[1]) + tuple(args))
    return out

# ----------------------------------------------------------------- cases


def rstate(r, inited=None):
    """A consistent CEZSprite: a list with instances, a sprite array, and counts."""
    inited = r.random() < 0.85 if inited is None else inited
    if not inited:
        return dict(flags=r.choice([0, 1]) if r.random() < 0.3 else 0, list=None, list_count=0,
                    counts=[0, T.s32(T.rint(r)), 0, 0], sprites=None, instances=None, cap=[0, 0],
                    objects=[], inst_fields={})
    nspr, ninst = r.randint(0, 4), r.randint(0, 5)
    maxs = nspr + r.choice([0, 0, 1, 3])
    maxi = ninst + r.choice([0, 1, 4])
    objects = [("list", "list")] + [("spr%d" % i, "sprite") for i in range(nspr)] + \
              [("inst%d" % i, "inst") for i in range(ninst)]
    fields = {"inst%d" % i: (T.s32(T.rint(r)), T.s32(T.rint(r)), r.choice([None] + ["spr%d" % j for j in range(nspr)]),
                              [T.f32(r.uniform(-100, 100)) for _ in range(4)]) for i in range(ninst)}
    return dict(flags=r.getrandbits(32) if r.random() < 0.3 else r.choice([0, 1]), list="list",
                list_count=r.choice([ninst, ninst, 0, 99]), counts=[maxi, maxs, nspr, ninst],
                sprites=["spr%d" % i for i in range(nspr)], instances=["inst%d" % i for i in range(ninst)],
                cap=[max(maxi, 1) + 8, max(maxs, 1) + 8], objects=objects, inst_fields=fields)


def rcase(r):
    st = rstate(r)
    k = r.random()
    return dict(st=st, init_results=[r.choice([1, 1, 1, 0, 7]) for _ in range(3)],
                list_fail=r.random() < 0.1, malloc_fail=r.random() < 0.1, link_result=r.choice([1, 1, 0, -1]),
                mapwho=r.random() < 0.85, errors=r.choice([0, 1, 63, 64]),
                max=(r.randint(0, 6), r.choice([0, 0, r.randint(0, 6)])),
                name="sprites/%s%d.spr" % (r.choice(["star", "flag", "smoke"]), r.randint(0, 99)),
                target=r.choice(["spr0", "spr1", "inst0", "inst1", "inst3", "list", None, "other"]),
                pos=[T.f32(r.uniform(-500, 500)) for _ in range(3)], k=k)


def target_addr(side, c):
    t = c["target"]
    if t is None:
        return 0
    if t == "other":
        return 0x7C0E1230
    return side.objs.get(t, 0x7C0E1240)


def method_test(name, label, host_fn, args=lambda side, c: [], res=I32, argtypes=(), ret=lambda side, v: v,
                rnd=rcase, edge=None):
    def ppc(emu, c):
        s = PpcSide(emu, c)
        this = s.build(c["st"])
        r3 = T.call_ppc(emu, emu.func(label), [this] + args(s, c))[0]
        return s.observe(this, lambda side: ret(side, r3))

    def host(lib, c):
        s = HostSide(lib, c)
        this = s.build(c["st"])
        fn = F(lib, host_fn, res, VP, *argtypes)
        v = fn(this, *args(s, c))
        return s.observe(this, lambda side: ret(side, v))
    return FnTest(name, ppc, host, rnd=rnd, edge=edge)


def ptr_ret(side, v):
    if isinstance(side, PpcSide):
        return side.emu.ez_label(v) if v else None
    return side.lab(v) if v else None


def s32_ret(side, v):
    return T.s32(v or 0)


def pos_args(side, c):
    if isinstance(side, PpcSide):
        return [target_addr(side, c), T.SVal("pos", "3f", c["pos"])]
    buf = (ctypes.c_float * 3)(*c["pos"])
    side.keep.append(buf)
    return [target_addr(side, c), ctypes.addressof(buf)]


def name_args(side, c):
    if isinstance(side, PpcSide):
        return [side.emu.alloc(64, c["name"].encode() + b"\0")]
    b = ctypes.create_string_buffer(c["name"].encode())
    side.keep.append(b)
    return [b]


def inited_case(r):
    c = rcase(r)
    c["st"] = rstate(r, inited=True)
    return c


def inited_edges():
    return [c for c in edge_states() if c["st"]["list"]]


def dup_case(kind):
    """An initialised state whose sprite (or instance) array holds one entry twice, and
    that entry as the target: the search direction decides which slot goes."""
    def gen(r):
        c = inited_case(r)
        st = c["st"]
        items = st["sprites"] if kind == "sprite" else st["instances"]
        if items and r.random() < 0.6:
            dup = r.choice(items)
            items.insert(r.randint(0, len(items)), dup)
            st["counts"][2 if kind == "sprite" else 3] = len(items)
            st["counts"][1 if kind == "sprite" else 0] = max(st["counts"][1 if kind == "sprite" else 0], len(items))
            c["target"] = dup
        return c
    return gen


def init_case(r):
    c = rcase(r)
    if r.random() < 0.7:
        c["st"] = rstate(r, inited=False)
    return c


def ctor_ppc(emu, c):
    s = PpcSide(emu, c)
    a = emu.alloc(0x20, c["junk"])
    r3 = T.call_ppc(emu, emu.func("__ct__9CEZSpriteFv"), [a])[0]
    return dict(obj=struct.unpack(">8I", emu.read(a, 0x20)), ret_this=r3 == a)


def ctor_host(lib, c):
    a = F(lib, "tpw_ez_store", VP)()
    j = struct.unpack(">8I", c["junk"])
    F(lib, "tpw_ez_set", None, VP, U32, VP, VP, VP, ctypes.POINTER(I32))(
        a, j[0], j[1] or None, j[2] or None, j[3] or None, (I32 * 4)(*[T.s32(x) for x in j[4:]]))
    F(lib, "tpw_CEZSprite_ctor", None, VP)(a)
    flags, ptrs, counts = U32(), (VP * 3)(), (I32 * 4)()
    F(lib, "tpw_ez_get", None, VP, ctypes.POINTER(U32), ctypes.POINTER(VP), ctypes.POINTER(I32))(
        a, ctypes.byref(flags), ptrs, counts)
    junk = struct.unpack(">8I", c["junk"])
    # mMaxSprites (0x14) is not initialised: it keeps what was there
    return dict(obj=(flags.value, ptrs[0] or 0, ptrs[1] or 0, ptrs[2] or 0, T.u32(counts[0]), T.u32(counts[1]),
                     T.u32(counts[2]), T.u32(counts[3])), ret_this=True) if counts[1] == T.s32(junk[5]) else \
        dict(obj="mMaxSprites changed", ret_this=True)


def dtor_ppc(emu, c):
    s = PpcSide(emu, c)
    this = s.build(c["st"])
    r3 = T.call_ppc(emu, emu.func("__dt__9CEZSpriteFv"), [this, T.u32(T.s16(c["flag"]))])[0]
    out = s.observe(this, r3 == this)
    return out


def dtor_host(lib, c):
    s = HostSide(lib, c)
    this = s.build(c["st"])
    freed = F(lib, "tpw_CEZSprite_dtor", I32, VP, ctypes.c_int16)(this, T.s16(c["flag"]))
    out = s.observe(this, True)
    if freed:
        out["allocs"] = out["allocs"] + [("delete", "self")]
    return out


def dtor_case(r):
    c = rcase(r)
    c["flag"] = r.choice([-1, 0, 1, 2, 0x7fff, -0x8000])
    return c


def sinit_ppc(emu, c):
    emu.write(EZSPRITE, c["junk"])
    emu.registered = []
    T.call_ppc(emu, emu.func("__sinit_tb_ezsprite_cpp"), [])
    dt = emu.func("__dt__9CEZSpriteFv")
    reg = [(obj == EZSPRITE, emu.u32(fn) == dt, rec - obj) for obj, fn, rec in emu.registered]
    return dict(obj=struct.unpack(">8I", emu.read(EZSPRITE, 0x20)), reg=reg)


def sinit_host(lib, c):
    g = F(lib, "tpw_ez_global", VP)()
    junk = struct.unpack(">8I", c["junk"])
    F(lib, "tpw_ez_set", None, VP, U32, VP, VP, VP, ctypes.POINTER(I32))(
        g, junk[0], junk[1] or None, junk[2] or None, junk[3] or None, (I32 * 4)(*[T.s32(x) for x in junk[4:]]))
    F(lib, "tpw_sinit_tb_ezsprite", None)()
    flags, ptrs, counts = U32(), (VP * 3)(), (I32 * 4)()
    F(lib, "tpw_ez_get", None, VP, ctypes.POINTER(U32), ctypes.POINTER(VP), ctypes.POINTER(I32))(
        g, ctypes.byref(flags), ptrs, counts)
    # C++ registers the destructor itself: the expected record is the object, its
    # destructor and a 12-byte chain record right after the 0x20-byte object
    return dict(obj=(flags.value, ptrs[0] or 0, ptrs[1] or 0, ptrs[2] or 0) + tuple(T.u32(x) for x in counts),
                reg=[(True, True, 0x20)])


def edge_states():
    import random
    r = random.Random(11)
    out = []
    for k in range(6):
        c = rcase(r)
        c["st"] = rstate(r, inited=k > 0)
        out.append(c)
    return out


TESTS = [
    FnTest("CEZSprite::CEZSprite", ctor_ppc, ctor_host, rnd=lambda r: dict(junk=r.randbytes(0x20)),
           edge=lambda: [dict(junk=b"\xff" * 0x20)]),
    FnTest("CEZSprite::~CEZSprite", dtor_ppc, dtor_host, rnd=dtor_case),
    method_test("CEZSprite::Init", "Init__9CEZSpriteFii", "tpw_CEZSprite_Init",
                args=lambda s, c: [T.u32(c["max"][0]), T.u32(c["max"][1])], argtypes=[I32, I32], ret=s32_ret,
                rnd=init_case, edge=edge_states),
    method_test("CEZSprite::Close", "Close__9CEZSpriteFv", "tpw_CEZSprite_Close", ret=s32_ret, edge=edge_states),
    method_test("CEZSprite::Enable", "Enable__9CEZSpriteFv", "tpw_CEZSprite_Enable", ret=s32_ret, edge=edge_states),
    method_test("CEZSprite::Disable", "Disable__9CEZSpriteFv", "tpw_CEZSprite_Disable", ret=s32_ret,
                edge=edge_states),
    method_test("CEZSprite::LoadSprite", "LoadSprite__9CEZSpriteFPc", "tpw_CEZSprite_LoadSprite", args=name_args,
                res=VP, argtypes=[ctypes.c_char_p], ret=ptr_ret, rnd=inited_case, edge=inited_edges),
    method_test("CEZSprite::DeleteSprite", "DeleteSprite__9CEZSpriteFPv", "tpw_CEZSprite_DeleteSprite",
                args=lambda s, c: [target_addr(s, c)], argtypes=[VP], ret=s32_ret, rnd=dup_case("sprite"),
                edge=edge_states),
    method_test("CEZSprite::CreateSpriteInstance", "CreateSpriteInstance__9CEZSpriteFPv",
                "tpw_CEZSprite_CreateSpriteInstance", args=lambda s, c: [target_addr(s, c)], res=VP, argtypes=[VP],
                ret=ptr_ret, rnd=inited_case, edge=inited_edges),
    method_test("CEZSprite::SetSpritePosition", "SetSpritePosition__9CEZSpriteFPv10_D3DVECTOR",
                "tpw_CEZSprite_SetSpritePosition", args=pos_args, argtypes=[VP, VP], ret=s32_ret, edge=edge_states),
    method_test("CEZSprite::DeleteSpriteInstance", "DeleteSpriteInstance__9CEZSpriteFPv",
                "tpw_CEZSprite_DeleteSpriteInstance", args=lambda s, c: [target_addr(s, c)], argtypes=[VP],
                ret=s32_ret, rnd=dup_case("inst"), edge=edge_states),
    method_test("CEZSprite::GetSpriteDetails", "GetSpriteDetails__9CEZSpriteFPv", "tpw_CEZSprite_GetSpriteDetails",
                args=lambda s, c: [target_addr(s, c)], res=VP, argtypes=[VP], ret=ptr_ret),
    FnTest("__sinit_tb_ezsprite_cpp", sinit_ppc, sinit_host, rnd=lambda r: dict(junk=r.randbytes(0x20))),
]
