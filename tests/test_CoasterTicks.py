"""Unit w137, the coasters' per-frame ticks, against libtpw_unit_CoasterTicks.dylib (tests/unit_CoasterTicks.cpp), main app
("SimTheme Park"), src/game/rides/CoasterEdit.cpp, src/game/rides/TPCoaster.cpp: CoasterTickPreRender 0x10043028, CoasterTickPostRender 0x10043200 and CoasterUserInputTick
0x100554b4.

Every callee is a recording fake on both sides. The ticks' world is up to four coaster instances (flags, state, the type's word, the id,
the trains' count and first words), the edit instance, gCoasterCurrent and its data pointer; the dump is the instances' flags, the
current instance, the data pointer, the word at 0x102b7310 and the call log (each call also shows gCoasterCurrent and the flags).
The input tick's world is the editor's drag state (the flags, the anchor, the scales, the last deltas, the pointer, the yaw, the brace,
the targets and their bases, mins and maxs, the saved points, the cost, the grid cell, the rides); the dump is every place it writes."""
import ctypes as C
import math
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_CoasterTicks"
MASK = 0xFFFFFFFF
fb = CL.fbits


def f32(x):
    return struct.unpack(">f", struct.pack(">f", x))[0]


def bf(bits):
    return struct.unpack(">f", struct.pack(">I", bits & MASK))[0]


G = dict(instances=0x102B68B4, current=0x102B68A8, data=0x102B68A4, edit=0x102B68B0, w7310=0x102B7310, recorder=0x1037EC44,
         yaw=0x102B5BF4, pxy=0x102B5A94, pointed=0x102C6CE4, world=0x102C6CF8, tpg=0x103B74C8, flags=0x102C1794, brace=0x102C178C,
         anchor=0x102C16EC, sx=0x102C16F4, sy=0x102C16F8, ldx=0x102C16E4, ldy=0x102C16E8, cost=0x102C1770, cellx=0x102C16D0,
         cellz=0x102C16CC, prev=0x102C16C0, w16e0=0x102C16E0)
SAVED = [0x102C1738, 0x102C172C, 0x102C1708, 0x102C16FC]
TARGET = [0x102C1768, 0x102C1764, 0x102C1760, 0x102C175C]
BOUNDS = [0x102C1748, 0x102C1758, 0x102C1750, 0x102C1744, 0x102C1754, 0x102C174C,
          0x102C1718, 0x102C1728, 0x102C1720, 0x102C1714, 0x102C1724, 0x102C171C]
CALLS = dict(Link=0x1003F9FC, Unlink=0x1003FEDC, UV=0x100428D4, Sounds=0x1004702C, Loading=0x10049FBC, RideTick=0x1004C4F0,
             Generate=0x10050FCC, Record=0x101441FC, Flag=0x10040F08, Round=0x1004DA1C, DataChange=0x10052318, Multi=0x10054EA8,
             Cost=0x10058924, Session=0x10091D50, Rot=0x100C121C)
PRE, POST, INPUT = 0x10043028, 0x10043200, 0x100554B4
K_INSTANCE, K_EDIT, K_BRACE, K_RECORDER, K_RIDE, K_TARGET = 11, 21, 22, 99, 31, 41
SIN = [fb(math.sin(2 * math.pi * i / 4096)) for i in range(4096)]
SIN_SCALE = fb(4096 / (2 * math.pi))


def ws(e, a, v): e.write(a, struct.pack(">" + "I" * len(v), *[x & MASK for x in v]))


def rd(e, a): return e.u32(a)


def make_stubs():
    st = CL.Stubs()
    A = CALLS

    def tick(name, train=False):
        def stub(e):
            L = e.cl
            inst = e.r(3)
            extra = -1
            if train:
                extra = (e.r(4) - rd(e, inst + 0x128)) // 0x80
            L.rec(name, [L.lab(inst), L.lab(rd(e, G["current"])), rd(e, inst), extra])
        return stub

    def record(e):
        L = e.cl
        cur = rd(e, G["current"])
        L.rec("RecordAction", [L.lab(e.r(3)), T.s32(e.r(4)), e.r(5) & 0xFFFF, L.lab(cur), rd(e, cur) if cur else -1])

    def flag(e):
        e.cl.rec("FlagGenerateTrack", [e.cl.lab(e.r(3)), T.s32(e.r(4))])

    def round_up(e):
        e.cl.rec("RoundUp", [fb(e.f(1))])
        e.set_r(3, e.cl.ret("RoundUp", 0) & MASK)

    def change(e):
        L = e.cl
        L.rec("HandleCoasterDataChange", [e.r(3), L.lab(e.r(4)), L.lab(e.r(5)), T.s32(e.r(6)), T.s32(e.r(7))])

    def multi(e):
        L = e.cl
        L.rec("HandleMultiBracePreviewChange", [L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5)), T.s32(e.r(6)), e.r(7), e.r(8)])

    def cost(e):
        L = e.cl
        L.rec("CalculateBraceCost", [L.lab(e.r(3)), T.s32(e.r(4)), L.lab(e.r(5))] + [L.lab(e.r(i)) for i in (6, 7, 8, 9)])
        e.set_f1(bf(L.ret("Cost", 0)))

    def session(e):
        e.cl.rec("LLCC_SystemAddToSessionCost", [T.s32(e.r(3))])

    def rot(e):
        L = e.cl
        p = e.r(3)
        L.rec("GetVectorRotationAroundYAxisFromZ", [L.lab(p), rd(e, p), rd(e, p + 4), rd(e, p + 8)])
        e.set_f1(bf(L.ret("Rot", 0)))

    for key, name, fn in [("Link", "LinkPreviewBrace", tick("LinkPreviewBrace")), ("Unlink", "UnLinkPreviewBrace", tick("UnLinkPreviewBrace")),
                          ("UV", "", tick("CoasterUVAnimTick")), ("Sounds", "", tick("TickTrainSounds", True)),
                          ("Loading", "", tick("HandleTrainLoading", True)), ("RideTick", "", tick("CoasterCarRideTrackTick")),
                          ("Generate", "", tick("GenerateTrack")), ("Record", "", record), ("Flag", "", flag), ("Round", "", round_up),
                          ("DataChange", "", change), ("Multi", "", multi), ("Cost", "", cost), ("Session", "", session), ("Rot", "", rot)]:
        st.internal(A[key], fn)
    return st


STUBS = make_stubs()


def setup(runner):
    T.install_std_stubs(runner.emu)


def host_bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ct_table.argtypes = [C.POINTER(C.c_uint32)]
    lib.tpw_ct_table((C.c_uint32 * 4096)(*SIN))
    lib.bound = True


def host_run(lib, entry, desc, queues):
    host_bind(lib)
    fn = getattr(lib, entry)
    fn.restype = C.c_int32
    fn.argtypes = [C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    values = (C.c_int64 * len(desc))(*desc)
    out = (C.c_int64 * 512)()
    CL.host_begin(lib, {}, queues)
    n = fn(values, out)
    log = CL.host_end(lib)
    return [int(out[i]) for i in range(n)], log


# ---------------------------------------------------------------------------------------------------------------- the ticks


def ticks_ppc(e, case):
    d = iter(case["d"])
    op, n, edit_choice, cur_choice, data_word, w7310 = [next(d) for _ in range(6)]
    fixed = {}
    insts = []
    for i in range(n):
        flags, state, tword, ident, count = [next(d) for _ in range(5)]
        words = [next(d) for _ in range(3)]
        c = e.alloc(0x140, bytes(0x140))
        tw = e.alloc(16, bytes(16))
        trains = e.alloc(0x180, bytes(0x180))
        ws(e, tw, [tword])
        for t in range(3):
            ws(e, trains + 0x80 * t, [words[t]])
        ws(e, c, [flags, tw, state])
        ws(e, c + 0x14, [ident])
        ws(e, c + 0x40, [1])
        ws(e, c + 0xF4, [count])
        ws(e, c + 0x128, [trains])
        insts.append(c)
        fixed[c] = K_INSTANCE + i
    for i in range(n - 1):
        ws(e, insts[i] + 0x18, [insts[i + 1]])
    other = e.alloc(0x140, bytes(0x140))
    fixed[other] = K_EDIT
    fixed[G["recorder"]] = K_RECORDER
    e.cl = CL.PpcLog(fixed, case.get("q"))
    ws(e, G["instances"], [insts[0] if n else 0])
    ws(e, G["edit"], [0 if edit_choice == 0 else other if edit_choice == 5 else insts[edit_choice - 1]])
    ws(e, G["current"], [0 if cur_choice == 0 else insts[cur_choice - 1]])
    ws(e, G["data"], [data_word])
    ws(e, G["w7310"], [w7310])
    saved = STUBS.activate(e)
    try:
        T.call_ppc(e, PRE if op == 0 else POST, [])
        out = [rd(e, c) for c in insts]
        cur = rd(e, G["current"])
        out.append(insts.index(cur) + 1 if cur in insts else (0 if cur == 0 else -1))
        data = rd(e, G["data"])
        res = -1
        for i, c in enumerate(insts):
            if data == c + 0x3C:
                res = i + 1
        out.append(-2 if data == data_word else res)
        out.append(rd(e, G["w7310"]))
        ed = rd(e, G["edit"])
        out.append(insts.index(ed) + 1 if ed in insts else (0 if ed == 0 else -1))
        return out, e.cl.log
    finally:
        CL.Stubs.deactivate(e, saved)


def ticks_host(lib, case):
    return host_run(lib, "tpw_ct_ticks", case["d"], case.get("q"))


def ticks_desc(op, insts, edit=0, cur=0, data=0x4000, w7310=7):
    d = [op, len(insts), edit, cur, data, w7310]
    for flags, state, tword, ident, count, words in insts:
        d += [flags, state, tword, ident, count] + (list(words) + [0, 0, 0])[:3]
    return {"d": d}


FLAG_BITS = [1, 4, 0x2000, 2, 0x100, 0x8000, 0x40000000]
STATE_BITS = [2, 4, 8, 0x40, 1, 0x10, 0x100]


def rnd_inst(r):
    flags = 0
    for b in FLAG_BITS:
        if r.random() < 0.5:
            flags |= b
    state = 0
    for b in STATE_BITS:
        if r.random() < 0.5:
            state |= b
    tword = r.choice([0, 4, 5, 0xFFFFFFFF, r.getrandbits(32)])
    count = r.choice([0, 1, 2, 3, 3])
    words = [r.choice([0, 2, 2, 3, r.getrandbits(32)]) for _ in range(3)]
    return (flags, state, tword, r.randint(0, 0xFFFFF), count, words)


def ticks_edges(op):
    def make():
        out = [ticks_desc(op, [])]
        for flags in (0, 1, 5, 0x2001, 0x2005, 4, 0x2000):
            for edit in (0, 1, 5):
                out.append(ticks_desc(op, [(flags, 0xE, 4, 77, 2, [2, 0, 3])], edit=edit, cur=0))
        for state in (0, 2, 4, 8, 0xC, 0x4C, 0x40, 0xE, 0x6):
            for t in (0, 4):
                out.append(ticks_desc(op, [(0x3, state, t, 5, 3, [2, 1, 2])]))
        out.append(ticks_desc(op, [(1, 0xC, 4, 1, 0, []), (1, 0xC, 4, 2, 1, [2]), (0, 0xC, 0, 3, 3, [2, 2, 2])], edit=2, cur=3))
        return out
    return make


def ticks_rnd(op, seed):
    def make(r):
        n = r.randint(0, 4)
        return ticks_desc(op, [rnd_inst(r) for _ in range(n)], edit=r.choice([0, 5] + list(range(1, n + 1))), cur=r.randint(0, n),
                          data=r.choice([0, 0x4000, r.getrandbits(32)]), w7310=r.getrandbits(32))
    return make


# ---------------------------------------------------------------------------------------------------------------- the input tick

PTR_BITS = [1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000]


def fl(r, lo=-40.0, hi=40.0):
    return fb(f32(r.uniform(lo, hi)))


def input_desc(r, edit=None, flags=None, equal=None):
    if flags is None:
        flags = 0
        for b in PTR_BITS:
            if r.random() < 0.4:
                flags |= b
    edit = r.choice([0, 1, 2, 2, 2, 2]) if edit is None else edit
    ax, ay, sx, sy = r.uniform(-5, 5), r.uniform(-5, 5), r.uniform(0.5, 30), r.uniform(0.5, 30)
    px, py = r.uniform(-5, 5), r.uniform(-5, 5)
    ax, ay, sx, sy, px, py = [f32(v) for v in (ax, ay, sx, sy, px, py)]
    ldx = f32(sx * f32(ax - px)) if r.random() < 0.4 else f32(r.uniform(-50, 50))
    ldy = f32(sy * f32(ay - py)) if r.random() < 0.4 else f32(r.uniform(-50, 50))
    yaw = r.choice([0.0, f32(r.uniform(-4, 4)), f32(r.uniform(-4, 4))])
    d = [edit, flags, fb(ax), fb(ay), fb(sx), fb(sy), fb(ldx), fb(ldy), fb(px), fb(py), fb(yaw)]
    d += [r.choice([0, 1, 1, 1]), fl(r, -3, 3), r.choice([0, 2, 0, 1, 3])]
    for _ in range(6):
        lo = r.uniform(-30, 0)
        hi = r.uniform(0, 30)
        base = r.uniform(-30, 30)
        d += [fb(f32(base)), fb(f32(lo)), fb(f32(hi))] if False else []
    bounds = []
    for _ in range(4):
        bounds.append((f32(r.uniform(-30, 30)), f32(r.uniform(-30, 5)), f32(r.uniform(-5, 30))))
    # the order the unit reads: V base1 min1 max1, V base2 min2 max2, H base1 min1 max1, H base2 min2 max2
    for base, lo, hi in bounds:
        d += [fb(base), fb(lo), fb(hi)]
    d += [fb(f32(r.uniform(-30, 30))) for _ in range(0)]
    for _ in range(2):
        pass
    for _ in range(12):
        d.append(fl(r))
    for t in range(4):
        d += [1 if t in (0, 2) else r.choice([1, 1, 0]), fl(r), fl(r), fl(r)]
    d += [fl(r, -1000, 1000), r.choice([0, 3, -5, 12345]), r.choice([0, 7, -9, 777]), r.randint(0, 3), r.randint(0, 3)]
    d += [fl(r, -200, 200), fl(r, -20, 20), fl(r, -200, 200)]
    d += [r.choice([0, 0, 1]), r.choice([0, 1, 3, 2]), 0xFFF, SIN_SCALE]
    return d


def input_rnd(r):
    q = {"Cost": [fb(f32(r.uniform(-500, 500)))], "RoundUp": [r.choice([r.randint(-1000, 1000), r.randint(-3, 3)])],
         "Rot": [fb(f32(r.uniform(-3, 3))), fb(f32(r.uniform(-3, 3)))]}
    return {"d": input_desc(r), "q": q}


def input_edges():
    out = []
    r = random.Random(1371)
    for edit in (0, 1):
        out.append({"d": input_desc(r, edit=edit, flags=0x3F), "q": {}})
    for flags in (0, 0x3F, 0x1E00, 0x400, 0x800, 0x200, 0x1000, 0x1400, 0xA00, 0x1400 | 0x2000, 0xA00 | 0x2000, 0x180, 0x100, 0x80, 0x1, 0x20):
        for k in range(3):
            out.append({"d": input_desc(r, edit=2, flags=flags),
                        "q": {"Cost": [fb(f32(r.uniform(-500, 500)))], "RoundUp": [r.choice([-40, 40, 0, 12345678])],
                              "Rot": [fb(f32(r.uniform(-3, 3)))]}})
    return out


def input_ppc(e, case):
    d = iter(case["d"])
    fixed = {}
    edit_mode, flags = next(d), next(d)
    fl_ = lambda: next(d)
    anchor = [fl_(), fl_()]
    sx, sy, ldx, ldy = fl_(), fl_(), fl_(), fl_()
    pxy = [fl_(), fl_()]
    yaw = fl_()
    brace_mode, angle, bflags = next(d), fl_(), next(d)
    bounds = [fl_() for _ in range(12)]
    saved = [[fl_() for _ in range(3)] for _ in range(4)]
    targets = []
    for t in range(4):
        targets.append((next(d), [fl_() for _ in range(3)]))
    cost, cx, cz, prev, pointed = fl_(), next(d), next(d), next(d), next(d)
    world = [fl_(), fl_(), fl_()]
    rec, w16e0, mask, scale = next(d), next(d), next(d), next(d)
    ws(e, G["tpg"], [0x1000, mask, scale])
    ws(e, G["tpg"] + 0xC, SIN)
    ws(e, G["flags"], [flags])
    ws(e, G["anchor"], anchor)
    ws(e, G["sx"], [sx, sy])
    ws(e, G["ldx"], [ldx, ldy])
    ws(e, G["pxy"], pxy)
    ws(e, G["yaw"], [yaw])
    for a, v in zip(BOUNDS, bounds):
        ws(e, a, [v])
    for a, s in zip(SAVED, saved):
        ws(e, a, s)
    taddr = []
    for t, (present, vals) in enumerate(targets):
        a = e.alloc(16, bytes(16))
        ws(e, a, vals)
        taddr.append(a)
        ws(e, TARGET[t], [a if present else 0])
        if present:
            fixed[a] = K_TARGET + t
    ws(e, G["cost"], [cost])
    ws(e, G["cellx"], [cx])
    ws(e, G["cellz"], [cz])
    ws(e, G["w16e0"], [w16e0])
    rides = [e.alloc(16, bytes(16)) for _ in range(3)]
    for i, a in enumerate(rides):
        fixed[a] = K_RIDE + i
    ws(e, G["prev"], [rides[prev - 1] if prev else 0])
    ws(e, G["pointed"], [rides[pointed - 1] if pointed else 0])
    ws(e, G["world"], world)
    ws(e, G["recorder"] + 0x24, [rec])
    fixed[G["recorder"]] = K_RECORDER
    data = e.alloc(16, bytes(16))
    ws(e, data + 4, [bflags])
    brace = e.alloc(0x110, bytes(0x110))
    ws(e, brace + 0xE0, [angle])
    ws(e, brace + 0x100, [data])
    ws(e, G["brace"], [brace if brace_mode else 0])
    if brace_mode:
        fixed[brace] = K_BRACE
    edit = e.alloc(0x140, bytes(0x140))
    ws(e, edit + 0x40, [0x55 if edit_mode == 2 else 0])
    fixed[edit] = K_EDIT
    ws(e, G["edit"], [edit if edit_mode else 0])
    e.cl = CL.PpcLog(fixed, case.get("q"))
    saved_stubs = STUBS.activate(e)
    try:
        T.call_ppc(e, INPUT, [])
        out = [rd(e, G["flags"])] + [rd(e, G["anchor"]), rd(e, G["anchor"] + 4), rd(e, G["ldx"]), rd(e, G["ldy"])]
        for a in SAVED:
            out += [rd(e, a + 4 * k) for k in range(3)]
        for t in range(4):
            p = rd(e, TARGET[t])
            out.append(1 if p else 0)
            out += [rd(e, taddr[t] + 4 * k) for k in range(3)]
        out += [T.s32(rd(e, G["cellx"])), T.s32(rd(e, G["cellz"])), e.cl.lab(rd(e, G["prev"])), e.cl.lab(rd(e, G["pointed"]))]
        return out, e.cl.log
    finally:
        CL.Stubs.deactivate(e, saved_stubs)


def input_host(lib, case):
    return host_run(lib, "tpw_ct_input", case["d"], case.get("q"))


TESTS = [
    FnTest("CoasterTickPreRender", ticks_ppc, ticks_host, edge=ticks_edges(0), rnd=ticks_rnd(0, 1371)),
    FnTest("CoasterTickPostRender", ticks_ppc, ticks_host, edge=ticks_edges(1), rnd=ticks_rnd(1, 1372)),
    FnTest("CoasterUserInputTick", input_ppc, input_host, edge=input_edges, rnd=input_rnd),
]
