"""Original-binary differential tests for the 18 ui_iterators.cpp functions (unit w227).

The original searches, dispatchers, wraps and callback glue run real. Recording fakes
answer the used-list links, thing id and object queries. Cases compare return labels or
counts, every callee argument and call order, all guards, all read fields, callback state
and the two file initialisers. Fakes can replace gGameWorld after every call; callbacks
can truncate staff links and change kind/flags. No invalid nonzero used-list rows or
cyclic chains are supplied. The empty backward walk's GetThingId(NULL) is faked as zero.
"""
import ctypes as C
import random
import struct

import calllog as CL
import ppcemu
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_UIIterators"
TABLE, WORLD = 0x1034CBFC, 0x1037EBFC
ASSERT, HEAD, NEXT, PREV = 0x1000DDE8, 0x1013ADB0, 0x1013AE08, 0x1013AEB0
GETID, UP, GETDB = 0x1012E1F8, 0x1011050C, 0x10107CD4
ADDRS = [0x101F7DB0, 0x101F8AA0, 0x101F8BFC, 0x101F8D30, 0x101F8E88, 0x101F8FE0,
         0x101F9154, 0x101F9268, 0x101F9858, 0x101F9A28, 0x101F9BD0, 0x101F9D9C,
         0x101F9F68, 0x101FA150, 0x101FA2D8, 0x101FA778, 0x101FA800, 0x101FA88C]
NAMES = ["UI_IterateThingList", "NextObjectOfType", "NextUpgrade", "NextNonTrackUpgrade", "NextBog",
         "NextFeatureNotBogOrStaffRoom", "NextStaffRoom", "UI_IterateThingNext", "PreviousObjectOfType",
         "PreviousUpgrade", "PreviousNonTrackUpgrade", "PreviousBog", "PreviousFeatureNotBogOrStaffRoom",
         "PreviousStaffRoom", "UI_IterateThingPrevious", "UI_WrapIterateThingNext", "UI_WrapIterateThingPrevious",
         "__sinit_ui_iterators_cpp"]
BITS = [1 << i for i in range(16)]
TYPES = [0, -1, -0x80000000, 0x7FFFFFFF, 3, 5, 7, 9, 0x11, 0x41, 0x81, 0x8001, 0xFFFF, 0x10000]
TYPES += sorted({v + delta for v in BITS for delta in [-1, 0, 1]})
HEAD_OFFSETS = [0x1DA73C, 0x1DA73E, 0x1DA744, 0x1DA740, 0x1DA742]
SLOTS, GUARD, THING_SIZE, RIDE_SIZE, WORLD_SIZE = 8, 16, 0x210, 0x800, 0x1DA748
L_WORLD, L_THING, L_RIDE = 1, 10, 30


def be16(e, p, v):
    e.write(p, struct.pack(">H", v & 0xFFFF))


def be32(e, p, v):
    e.write(p, struct.pack(">I", v & 0xFFFFFFFF))


def half(e, p):
    return struct.unpack(">H", e.read(p, 2))[0]


def setup(runner):
    T.install_std_stubs(runner.emu)

    # A persistent TVector lets the real __ptr_glue call the per-case recorder.
    def callback(e):
        a, c, log = e.uii_a, e.uii_c, e.cl
        thing = e.r(3)
        log.rec("Callback", [log.lab(thing)])
        if c["mode"] & 1:
            be16(e, thing + 0x20C, 0)
        if c["mode"] & 2:
            e.write(thing + 2, b"\x01")
        if c["mode"] & 4:
            be16(e, thing + 0x2E, half(e, thing + 0x2E) ^ 3)
        swap_world(e, a, c)
        e.set_r(3, T.u32(c["answer"]))

    runner.emu.uii_callback = T.fake_function(runner.emu, callback)


def swap_world(e, a, c):
    if c["swap"]:
        be32(e, WORLD, a["worlds"][1] if e.u32(WORLD) == a["worlds"][0] else a["worlds"][0])


def links(c):
    nxt, prev = [0] * SLOTS, [0] * SLOTS
    order = c["order"]
    for pos, i in enumerate(order):
        nxt[i] = c["ids"][order[pos + 1]] if pos + 1 < len(order) else 0
        prev[i] = c["ids"][order[pos - 1]] if pos else 0
    return nxt, prev


def stubs_for(e, log, a, c):
    nxt, prev = links(c)

    def assertion(emu):
        log.rec("GAME_ASSERTMSG", [T.s32(emu.r(3))], [emu.cstr(emu.r(4))])
        swap_world(e, a, c)

    def head(emu):
        log.rec("GetUsedThingHead", [log.lab(emu.r(4))])
        be16(e, emu.r(3), c["ids"][c["order"][0]] if c["order"] else 0)
        swap_world(e, a, c)

    def step(name, values):
        def fn(emu):
            log.rec(name, [log.lab(emu.r(4)), log.lab(emu.r(5))])
            be16(e, emu.r(3), values[a["things"].index(emu.r(5))])
            swap_world(e, a, c)
        return fn

    def getid(emu):
        log.rec("GetThingId", [log.lab(emu.r(4))])
        be16(e, emu.r(3), half(e, emu.r(4)) if emu.r(4) else 0)
        swap_world(e, a, c)

    def upgrade(emu):
        log.rec("UpgradesWhat", [log.lab(emu.r(4))])
        be16(e, emu.r(3), c["things"][a["things"].index(emu.r(4))][2])
        swap_world(e, a, c)

    def getdb(emu):
        log.rec("GetDBInfo", [log.lab(emu.r(3))])
        emu.set_r(3, a["rides"][a["things"].index(emu.r(3))])
        swap_world(e, a, c)

    stubs = CL.Stubs()
    stubs.real = set(ADDRS)
    for address, fn in [(ASSERT, assertion), (HEAD, head), (NEXT, step("GetNextUsedThing", nxt)),
                        (PREV, step("GetPrevUsedThing", prev)), (GETID, getid), (UP, upgrade), (GETDB, getdb)]:
        stubs.internal(address, fn)
    return stubs


def build_ppc(e, c):
    def block(size):
        return e.alloc(size + 2 * GUARD, b"\xa5" * (size + 2 * GUARD)) + GUARD

    a = dict(worlds=[block(WORLD_SIZE) for _ in range(2)], things=[block(THING_SIZE) for _ in range(SLOTS)],
             rides=[block(RIDE_SIZE) for _ in range(SLOTS)])
    fixed = {p: L_WORLD + i for i, p in enumerate(a["worlds"])}
    fixed.update({p: L_THING + i for i, p in enumerate(a["things"])})
    fixed.update({p: L_RIDE + i for i, p in enumerate(a["rides"])})
    log = CL.PpcLog(fixed)
    e.cl, e.uii_a, e.uii_c = log, a, c
    be32(e, WORLD, a["worlds"][0])
    for i, (kind, flags, up, ui, upgrade_type) in enumerate(c["things"]):
        p = a["things"][i]
        be16(e, p, c["ids"][i])
        e.write(p + 2, bytes([kind]))
        be16(e, p + 0x2E, flags)
        be16(e, p + 0x20C, c["staff"][i])
        be32(e, a["rides"][i] + 0x7A8, ui)
        be32(e, a["rides"][i] + 0x288, upgrade_type)
    for ident in range(16):
        slot = c["rows"].get(ident, -1)
        be32(e, TABLE + 0x14 * ident, a["things"][slot] if slot >= 0 else 0)
    for i, world in enumerate(a["worlds"]):
        for offset, ident in zip(HEAD_OFFSETS, c["heads"][i]):
            be16(e, world + offset, ident)
    return a, log, stubs_for(e, log, a, c)


def observe_ppc(e, a, log):
    fields, guards = [], []
    for p in a["things"]:
        fields.append([half(e, p), e.read(p + 2, 1)[0], half(e, p + 0x2E), half(e, p + 0x20C)])
        raw = bytearray(e.read(p - GUARD, THING_SIZE + 2 * GUARD))
        for offset, size in [(0, 2), (2, 1), (0x2E, 2), (0x20C, 2)]:
            raw[GUARD + offset:GUARD + offset + size] = b"\0" * size
        guards.append(bytes(raw))
    for p in a["rides"]:
        guards += [e.read(p - GUARD, GUARD), e.read(p + RIDE_SIZE, GUARD)]
    for p in a["worlds"]:
        guards += [e.read(p - GUARD, GUARD), e.read(p + WORLD_SIZE, GUARD)]
    return dict(fields=fields, guards=guards, heads=[[half(e, p + off) for off in HEAD_OFFSETS] for p in a["worlds"]],
                rides=[[e.s32(p + 0x7A8), e.s32(p + 0x288)] for p in a["rides"]], world=log.lab(e.u32(WORLD)))


def query_ppc(which):
    def fn(e, c):
        a, log, stubs = build_ppc(e, c)
        thing = a["things"][c["start"]] if c["start"] >= 0 else 0
        if which == 0:
            args = [T.I(c["type"]), T.I(e.uii_callback)]
        elif which in [1, 8, 7, 14, 15, 16]:
            args = [T.I(c["type"]), T.I(thing)]
        else:
            args = [T.I(thing)]
        saved = stubs.activate(e)
        try:
            result = T.call_ppc(e, ADDRS[which], args)[0]
        finally:
            CL.Stubs.deactivate(e, saved)
        return dict(ret=T.s32(result) if which == 0 else log.lab(result), log=log.log, **observe_ppc(e, a, log))
    return fn


def bind(lib):
    if getattr(lib, "_uii_bound", False):
        return
    lib.tpw_uii_addr.argtypes = [C.c_int32]
    lib.tpw_uii_addr.restype = C.c_void_p
    lib.tpw_uii_layout.argtypes = [C.c_int32]
    lib.tpw_uii_layout.restype = C.c_uint32
    for name, count in [("reset", 4), ("thing", 8), ("links", 3), ("row", 2), ("head", 3), ("call", 3)]:
        getattr(lib, "tpw_uii_" + name).argtypes = [C.c_int32] * count
    lib.tpw_uii_call.restype = C.c_int32
    lib.tpw_uii_sinit.argtypes = [C.POINTER(C.c_uint32)]
    assert lib.tpw_uii_layout(8) == 0x2E
    lib._uii_bound = True


def query_host(which):
    def fn(lib, c):
        bind(lib)
        lib.tpw_uii_reset(c["ids"][c["order"][0]] if c["order"] else 0, c["answer"], c["mode"], c["swap"])
        nxt, prev = links(c)
        for i, (kind, flags, up, ui, upgrade_type) in enumerate(c["things"]):
            lib.tpw_uii_thing(i, c["ids"][i], kind, flags, c["staff"][i], up, ui, upgrade_type)
            lib.tpw_uii_links(i, nxt[i], prev[i])
        for ident, slot in c["rows"].items():
            lib.tpw_uii_row(ident, slot)
        for i in range(2):
            for j, ident in enumerate(c["heads"][i]):
                lib.tpw_uii_head(i, j, ident)
        worlds = [lib.tpw_uii_addr(i) for i in range(2)]
        things = [lib.tpw_uii_addr(2 + i) for i in range(SLOTS)]
        rides = [lib.tpw_uii_addr(10 + i) for i in range(SLOTS)]
        fixed = {L_WORLD + i: p for i, p in enumerate(worlds)}
        fixed.update({L_THING + i: p for i, p in enumerate(things)})
        fixed.update({L_RIDE + i: p for i, p in enumerate(rides)})
        CL.host_begin(lib, fixed)
        try:
            result = lib.tpw_uii_call(which, c["type"], c["start"])
        finally:
            log = CL.host_end(lib)
        fields, guards = [], []
        for p in things:
            fields.append([C.c_uint16.from_address(p).value, C.c_uint8.from_address(p + 2).value,
                           C.c_uint16.from_address(p + 0x2E).value, C.c_uint16.from_address(p + 0x20C).value])
            raw = bytearray(C.string_at(p - GUARD, THING_SIZE + 2 * GUARD))
            for offset, size in [(0, 2), (2, 1), (0x2E, 2), (0x20C, 2)]:
                raw[GUARD + offset:GUARD + offset + size] = b"\0" * size
            guards.append(bytes(raw))
        for p in rides:
            guards += [C.string_at(p - GUARD, GUARD), C.string_at(p + lib.tpw_uii_layout(6), GUARD)]
        for p in worlds:
            guards += [C.string_at(p - GUARD, GUARD), C.string_at(p + lib.tpw_uii_layout(0), GUARD)]
        return dict(ret=result, log=log, fields=fields, guards=guards,
                    heads=[[C.c_uint16.from_address(p + lib.tpw_uii_layout(j + 1)).value for j in range(5)] for p in worlds],
                    rides=[[C.c_int32.from_address(p + lib.tpw_uii_layout(7)).value, C.c_int32.from_address(p + lib.tpw_uii_layout(9)).value] for p in rides],
                    world=1 + worlds.index(lib.tpw_uii_addr(18)))
    return fn


class Base(FnTest):
    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        try:
            p = self.ppc(runner.emu, case)
        except ppcemu.ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(runner.lib, case)
        difference = CL.first_difference(p.pop("log"), h.pop("log"))
        errors = T.same_values(p, h, "", False)
        if difference:
            errors.append(difference)
        return errors


def case(n=8, **kw):
    # kind, flags, UpgradesWhat, UI type, UpgradeType, with hits for each family.
    things = [(1, 0, 0, 0, 0), (4, 0, 0, 0, 0), (3, 0, 0, 0, 0), (3, 0, 0, 1, 0),
              (3, 0, 0, 2, 0), (3, 1, 0, 3, 0), (3, 2, 0, 3, 0), (3, 0, 1, 3, 2)]
    ids = [3, 1, 7, 2, 14, 4, 6, 11]
    staff = [ids[i + 1] if i + 1 < n else 0 for i in range(SLOTS)]
    c = dict(things=things, ids=ids, rows={ids[i]: i for i in range(n)}, order=list(range(n)), start=-1,
             staff=staff, heads=[[ids[0] if n else 0] * 5, [ids[n - 1] if n else 0] * 5],
             type=0xFFFF, answer=-1, mode=0, swap=0)
    c.update(kw)
    return c


def edges(which):
    def fn():
        out = []
        arg_types = TYPES if which in [0, 7, 14, 15, 16] else [-1, 0, 1, 2, 3, 4] if which in [1, 8] else [0]
        for typ in arg_types:
            for n in [0, 1, 8]:
                for start in [-1, 0, 3, 7]:
                    out.append(case(n, type=typ, start=start))
        # Each field boundary and short circuit, both an isolated candidate and several hits.
        for kind in list(range(10)) + [0xFF]:
            for up in [0, 1, 0x8000, 0xFFFF]:
                for ui in [-1, 0, 1, 2, 3, 4]:
                    t = (kind, 3, up, ui, 2)
                    out.append(case(1, things=[t] * SLOTS, type=0xFFFF if which == 0 else 3))
        for flags in [0, 1, 2, 3, 4, 0x8000, 0xFFFF]:
            for upgrade_type in [0, 1, 2, 3, -1, 0x7FFFFFFF]:
                for up in [0, 1]:
                    out.append(case(things=[(3, flags, up, 3, upgrade_type)] * SLOTS, type=0xFFFF if which == 0 else 3))
        # Row zero occupied makes each assertion false without stopping the walk.
        c = case(type=0xFFFF if which == 0 else 1)
        c["rows"][0] = 0
        out.append(c)
        for swap in [0, 1]:
            for mode in range(8):
                for answer in [0, 1, -1, -0x80000000, 0x7FFFFFFF]:
                    out.append(case(type=0xFFFF if which == 0 else 1, swap=swap, mode=mode, answer=answer))
        # A forward staff selection may fetch an empty table row and return null.
        if which in [7, 15]:
            for typ in [2, 4, 8, 16, 32]:
                out.append(case(type=typ, start=0, staff=[15] + [0] * 7))
        return out
    return fn


def random_case(which):
    def fn(r):
        n = r.randrange(SLOTS + 1)
        order = r.sample(range(SLOTS), n)
        ids = r.sample(range(1, 16), SLOTS)
        # Chains always progress through increasing slots, even when the used order differs.
        staff = [r.choice([0] + ids[i + 1:]) for i in range(SLOTS)]
        rows = {ids[i]: i for i in range(SLOTS)}
        if r.randrange(4) == 0:
            rows[0] = r.randrange(SLOTS)
        typ = r.choice(TYPES + BITS * 3)
        if which == 0:
            typ = r.choice([r.getrandbits(32), 0xFFFF, 0x1FFFF, -1] + BITS)
        elif which in [1, 8]:
            typ = r.choice([-0x80000000, -1, 0, 1, 2, 3, 4, 0x7FFFFFFF])
        return case(things=[(r.choice(list(range(10)) + [3] * 8 + [0xFF]), r.choice([0, 1, 2, 3, 4, 0xFFFF]),
                             r.choice([0, 0, 0, 1, 0x8000, 0xFFFF]), r.randrange(-1, 5), r.randrange(-1, 4)) for _ in range(SLOTS)],
                    ids=ids, order=order, rows=rows, start=r.randrange(-1, SLOTS), staff=staff,
                    heads=[[r.choice([0] + ids) for _ in range(5)] for _ in range(2)],
                    type=typ, answer=T.s32(r.getrandbits(32)), mode=r.randrange(8), swap=r.randrange(2))
    return fn


def sinit_ppc(e, c):
    be32(e, 0x10396A4C, c["a"])
    be32(e, 0x10396A50, c["b"])
    T.call_ppc(e, ADDRS[17], [])
    return dict(words=[e.u32(0x10396A4C), e.u32(0x10396A50)])


def sinit_host(lib, c):
    bind(lib)
    out = (C.c_uint32 * 2)()
    lib.tpw_uii_sinit(out)
    return dict(words=list(out))


TESTS = [Base(NAMES[i], query_ppc(i), query_host(i), edge=edges(i), rnd=random_case(i)) for i in range(17)]
TESTS.append(FnTest(NAMES[17], sinit_ppc, sinit_host, edge=lambda: [dict(a=0, b=0), dict(a=0xFFFFFFFF, b=0xFFFFFFFF),
                    dict(a=1, b=2), dict(a=0x10000, b=0)], rnd=lambda r: dict(a=r.getrandbits(32), b=r.getrandbits(32))))
