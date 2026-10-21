"""The constructors of the things a new level makes (unit w122a, main app), against libtpw_unit_ThingCtors.dylib (tests/unit_ThingCtors.cpp):

  src/game/Thing.cpp, src/game/advisor/Advisor.cpp, src/game/buildings/MechanicHQ.cpp, src/game/buildings/ResearchLab.cpp, src/game/buildings/StaffHQ.cpp, src/game/park/Weather.cpp, src/game/ui/vector2d.cpp   CThing::CThing (0x1012de58), CStaffHQ::CStaffHQ (0x1012a7fc), CMechanicHQ::CMechanicHQ (0x1010521c),
                       CAdvisor::CAdvisor (0x10009f9c), CTagSystem::CTagSystem (0x1012c57c), CResearchLab::CResearchLab (0x10121ad0),
                       CWeather::CWeather (0x10137da0), CUIMsgReceiver::CUIMsgReceiver (0x101305b0)
  include/ThingParts.h, include/ThingReceivers.h (inline)   the six leaf part constructors: CTrainingCentre (0x1012ffb0), CWeatherForecast
                       (0x10139674), CStaffHQ::StrikeType (0x1012a950), AdvisorHistory (0x1000a06c, the loop in CAdvisor::CAdvisor),
                       CResearchProject (0x10123abc) and CResearchCategory (0x101242a0). They run as the original on the PowerPC side (the
                       __construct_array stub calls the original element constructors) and are exercised by the cases of the constructors
                       that build them.

The constructors run as the original on the PowerPC side and as src/ on the host. Every callee outside them is a fake that records the same
event on both sides: CWorld::AddThingToThingList (the world pointer, the thing and its first twelve bytes by field; it answers the case's
queue "row"), CThing::GetThingId, CMapCell::InsertThing (the cell as an offset from the map's base), TRACE, CMessageCentre::Subscribe (the
centre, the thing, the type, the thing's twelve bytes and the weather's wind word), CWeather::FindNewQuality (the queue "quality"),
CAdvisor::Init, CTagSystem::Init, CRain::GameLoad, operator new (the lazy message centre; the queue "newnull" makes it fail) and the 29
headers of a new centre (one event, as __construct_array's call on the Mac).

Each case builds the object over fill bytes (0x00, 0xa5, 0xff or random) with guard bytes after it. After the call the test compares the call
log, the four globals (gWorld, gWorldMap, gMessageCentre and gei_Rain, by label or raw low 32 bits), the bytes the constructor writes (by value:
big-endian on the Mac, native on the host) and that every other byte of the object and its guard still holds the fill.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from ppcemu import ImportCalled
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ThingCtors"

# The functions of the unit, by op number (the host's tpw_tc_call uses the same numbers).
THING, STAFF, MECH, ADVISOR, TAG, LAB, WEATHER, UIMSG = range(8)
ADDR = [0x1012DE58, 0x1012A7FC, 0x1010521C, 0x10009F9C, 0x1012C57C, 0x10121AD0, 0x10137DA0, 0x101305B0]
LEAVES = [0x1012FFB0, 0x10139674, 0x1012A950, 0x1000A06C, 0x101242A0, 0x10123ABC, 0x1000A0B4]
OWN = set(ADDR) | set(LEAVES)        # these run as the original (stubs other modules left on them are lifted)
SIZE = [0xC, 0x68, 0xE, 0x16F0, 0x80E, 0x146C, 0x68, 0xC]
MODEL = [None, 9, 10, 0xB, 0xC, 0xE, 0xF, 0x11]
# The message types each class subscribes to, in order (the blocks that test the global before each Subscribe).
SUBS = {STAFF: [0xC, 0xE], MECH: [7], WEATHER: [1, 2], UIMSG: [0x12, 0xA, 7, 8, 0x1B, 0xF, 0x13, 0x1C]}
GUARD = 0x40
MASK = 0xFFFFFFFF

G_WORLD, G_MAP, G_CENTRE = 0x1037EC0C, 0x1037EC08, 0x1037F6C4     # gWorld, gWorldMap, gMessageCentre (gei_Rain: a data import)
WIDE_CTOR_TV, WIDE_DTOR_TV = 0x102657E8, 0x10267288              # the centre's __construct_array tables
CENTRE_SIZE = 0x1D0

ROWS = [0, 1, 5, 0x27FF, 0x2800, 0x8000, 0xFFFF]
QUALITY = [0, 1, 99, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF]
IDXS = [1, 0, 2, 0x7F, 0x80, 0x81, 0xFF, 0x100, 0x3FFF, 0x4000, 0x4001, 0x7FFF, 0x8000, 0x8001, 0xFFFF]
MODELS = list(range(0x15)) + [0x7F, 0x80, 0xFF, 0x100, 0x101, 0x1FF, 0xFFFFFFFF]
MAPS = [0, 1, 0x44, 0x1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFBC, 0xFFFFFFFE, 0xFFFFFFFF, 0x10000000]
FILLS = [0x00, 0xA5, 0xFF, "rand"]
MODES = ("null", "block", "rand")


def setup(runner):
    T.install_std_stubs(runner.emu)


# ------------------------------------------------------------------ the objects and their checked bytes (Mac offsets)


def thing_fields():
    """(offset, width) of the scalars CThing::CThing writes."""
    return [(0, 2), (2, 1), (3, 1), (4, 1), (5, 1), (6, 1), (7, 1), (8, 2), (0xA, 2)]


def fields_of(op):
    """(offset, width) of every scalar the constructor writes; every other byte of the object and the guard must keep the fill."""
    f = thing_fields()
    if op == STAFF:
        f += [(0xC + 4 * i, 4) for i in range(5)] + [(0x20, 1)] + [(0x24 + 4 * i, 4) for i in range(15)] + [(0x60, 2), (0x64, 4)]
    elif op == MECH:
        f += [(0xC, 2)]
    elif op == ADVISOR:
        f += [(0xC, 4), (0x10, 4)] + [(0x28 + 0x18 * i, 1) for i in range(8)] + [(0xD4, 4), (0xDC, 4)]
        for i in range(0x15F):
            b = 0xE0 + 0x10 * i
            f += [(b, 4), (b + 4, 4), (b + 8, 1), (b + 0xC, 4)]
        f += [(0x16E4, 1)]
    elif op == LAB:
        for i in range(5):
            b = 0x140C + 0x10 * i
            f += [(b + 8, 4), (b + 0xC, 1)]
    elif op == WEATHER:
        f += [(0xC, 4), (0x10, 4), (0x14, 4), (0x18, 4), (0x1C, 4), (0x20, 4), (0x24, 4), (0x2C, 1), (0x30, 4), (0x34, 4), (0x38, 1),
              (0x3C, 4), (0x40, 4), (0x44, 4), (0x54, 4), (0x5C, 4), (0x60, 4), (0x64, 1)]
    return f


def block_report(buf, fill, op, little):
    """The scalars by value and the first byte outside them that differs from the fill (-1: none)."""
    fields = fields_of(op)
    order = "little" if little else "big"
    values = [int.from_bytes(buf[o:o + w], order) for o, w in fields]
    a, b = bytearray(buf), bytearray(fill)
    for o, w in fields:
        a[o:o + w] = bytes(w)
        b[o:o + w] = bytes(w)
    first = -1
    if a != b:
        first = next(i for i in range(len(a)) if a[i] != b[i])
    return {"fields": values, "untouched": first}


# ------------------------------------------------------------------ the case data both sides share


def make_fill(mode, rng, n):
    return rng.randbytes(n) if mode == "rand" else bytes([mode]) * n


def prepare(c):
    rng = random.Random(c["seed"])
    return {"fill": make_fill(c["fill"], rng, SIZE[c["op"]] + GUARD),
            "world": rng.getrandbits(32) | 0x01000000,       # the words used when a pointer is "rand" (never a block's address)
            "rain": rng.getrandbits(32) | 0x01000000,
            "low": rng.getrandbits(16)}                      # the stale low half of r4 on the PowerPC side


def canon_ppc(L, word):
    label = L.lab(word)
    return label if label >= 0 else (1 << 33) | word


# ------------------------------------------------------------------ the PowerPC side


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def ppc_case(e, c, d):
    op = c["op"]
    L = CL.PpcLog({}, c.get("q"))
    e.cl = L
    fill = d["fill"]
    block = e.alloc(len(fill), fill)
    world = e.alloc(16, b"\xa5" * 16)
    centre = e.alloc(CENTRE_SIZE, b"\xa5" * CENTRE_SIZE)
    rain = e.alloc(16, b"\xa5" * 16)
    fresh = e.alloc(CENTRE_SIZE, b"\xa5" * CENTRE_SIZE)          # the block operator new gives when it does not fail
    L.labels.update({world: 1, block: 2, centre: 3, rain: 4})
    words = {"null": 0, "block": None, "rand": None}
    world_word = {"null": 0, "block": world, "rand": d["world"]}[c["world"]]
    rain_word = {"null": 0, "block": rain, "rand": d["rain"]}[c["rain"]]
    centre_word = centre if c["centre"] == "existing" else 0
    map_word = c["map"]
    rain_cell = e.data_import("gei_Rain")

    def snap(a):
        # the thing's first twelve bytes by field: (id, kind, dead), the four cell bytes, the two list links (all big-endian words)
        return [e.u32(a), e.u32(a + 4), e.u32(a + 8)]

    def add_thing(emu):
        thing = emu.r(5)
        L.rec("AddThingToThingList", [L.lab(emu.r(4)), L.lab(thing)] + snap(thing))
        emu.write(emu.r(3), (L.ret("row", 0) & 0xFFFF).to_bytes(2, "big"))

    def thing_id(emu):
        thing = emu.r(4)
        L.rec("GetThingId", [L.lab(thing), int.from_bytes(emu.read(thing, 2), "big")] + snap(thing))
        emu.write(emu.r(3), emu.read(thing, 2))

    def insert_thing(emu):
        L.rec("InsertThing", [T.s32((emu.r(3) - map_word) & MASK), emu.r(4) >> 16])

    def subscribe(emu):
        thing = emu.r(4)
        wind = emu.u32(thing + 0x5C) if emu.read(thing + 2, 1)[0] == 0xF else 0
        L.rec("Subscribe", [L.lab(emu.r(3)), L.lab(thing), T.s32(emu.r(5))] + snap(thing) + [wind])

    def quality(emu):
        L.rec("CWeather::FindNewQuality", [L.lab(emu.r(3))])
        emu.set_r(3, L.ret("quality", 0) & MASK)

    def advisor_init(emu):
        L.rec("CAdvisor::Init", [L.lab(emu.r(3))])
        emu.set_r(3, 1)

    def game_load(emu):
        L.rec("CRain::GameLoad", [L.lab(emu.r(3))])
        emu.set_r(3, 1)

    def op_new(emu):
        fail = L.ret("newnull", 0) != 0
        L.rec("new", [emu.r(3), int(fail)])
        emu.set_r(3, 0 if fail else fresh)

    def construct(emu, call):
        base, ctor, dtor, size, count = emu.r(3), emu.r(4), emu.r(5), emu.r(6), emu.r(7)
        if ctor == WIDE_CTOR_TV:
            # the centre's 29 headers: one event, the tables and sizes checked (the host's first header logs the same)
            L.rec("CMessageCentre", [L.newlab(base), int(ctor == WIDE_CTOR_TV), int(dtor == WIDE_DTOR_TV), size, count])
            return
        for i in range(count):
            call(ctor, base + i * size, 1)

    st = CL.Stubs()
    st.real = set(OWN)
    st.internal(0x1013B054, add_thing)
    st.internal(0x1012E1F8, thing_id)
    st.internal(0x10102D34, insert_thing)
    st.internal(0x1006FCFC, CL.recorder("TRACE", "sii"))
    st.internal(0x10152388, subscribe)
    st.internal(0x10138E70, quality)
    st.internal(0x1000A13C, advisor_init)
    st.internal(0x1012C6B8, lambda emu: L.rec("CTagSystem::Init", [L.lab(emu.r(3))]))
    st.imp("GameLoad__5CRainFv", game_load)
    st.imp("__nw__FUl", op_new)
    saved = st.activate(e)
    out = {}
    try:
        T.stub_cb(e, "__construct_array", construct)
        w32(e, G_WORLD, world_word)
        w32(e, G_MAP, map_word)
        w32(e, G_CENTRE, centre_word)
        w32(e, rain_cell, rain_word)
        if op == THING:
            args = [block, ((c["idx"] & 0xFFFF) << 16) | d["low"], c["model"] & MASK]
        else:
            args = [block]
        T.call_ppc(e, ADDR[op], args, max_insns=200_000_000, timeout_us=60_000_000)
        out["globals"] = [canon_ppc(L, e.u32(a)) for a in (G_WORLD, G_MAP, G_CENTRE, rain_cell)]
        out.update(block_report(e.read(block, len(fill)), fill, op, False))
    finally:
        CL.Stubs.deactivate(e, saved)
    out["log"] = L.log
    return out


# ------------------------------------------------------------------ the host side


def host_case(lib, c, d):
    op = c["op"]
    fill = d["fill"]
    lib.tpw_tc_load.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_tc_load(fill, len(fill))
    for name in ("tpw_tc_block", "tpw_tc_world_block", "tpw_tc_centre_block", "tpw_tc_rain_block"):
        getattr(lib, name).restype = C.c_void_p
    block, world, centre, rain = (lib.tpw_tc_block(), lib.tpw_tc_world_block(), lib.tpw_tc_centre_block(), lib.tpw_tc_rain_block())
    world_word = {"null": 0, "block": world, "rand": d["world"]}[c["world"]]
    rain_word = {"null": 0, "block": rain, "rand": d["rain"]}[c["rain"]]
    centre_word = centre if c["centre"] == "existing" else 0
    lib.tpw_tc_set_globals.argtypes = [C.c_uint64] * 4
    lib.tpw_tc_set_globals(world_word, c["map"], centre_word, rain_word)
    lib.tpw_tc_call.argtypes = [C.c_int32, C.c_uint32, C.c_uint32]
    out = {}
    CL.host_begin(lib, {1: world, 2: block, 3: centre, 4: rain}, c.get("q"))
    try:
        lib.tpw_tc_call(op, c["idx"] & 0xFFFF, c["model"] & MASK)
    finally:
        log = CL.host_end(lib)
    state = (C.c_int64 * 4)()
    lib.tpw_tc_get_globals.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_tc_get_globals(state)
    out["globals"] = list(state)
    buf = C.create_string_buffer(len(fill))
    lib.tpw_tc_dump.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_tc_dump(buf, len(fill))
    out.update(block_report(buf.raw, fill, op, True))
    out["log"] = log
    return out


class ThingTest(FnTest):
    """One constructor: the case data is made once and handed to both sides."""

    def run_case(self, runner, case):
        emu, lib = runner.emu, runner.lib
        emu.reset_scratch()
        data = prepare(case)
        try:
            p = self.ppc(emu, case, data)
        except ImportCalled as ex:
            return [f"PPC side called unstubbed import {ex}"]
        h = self.host(lib, case, data)
        errors = T.same_values(p, h, "", False)
        if not errors and "log" in p:
            first = CL.first_difference(p["log"], h["log"])
            if first:
                errors.append(first)
        return errors


# ------------------------------------------------------------------ cases


def mk(op, r, **kw):
    """A random case; the keyword arguments override the parts an edge case pins."""
    n_subs = len(SUBS.get(op, [])) or 1
    c = {"op": op, "seed": r.getrandbits(32), "fill": r.choice(FILLS),
         "idx": 1, "model": MODEL[op] if MODEL[op] is not None else r.choice(MODELS),
         "world": r.choice(MODES), "rain": r.choice(MODES), "centre": r.choice(("none", "none", "existing")),
         "map": r.choice(MAPS) if r.random() < 0.6 else r.getrandbits(32),
         "q": {"row": [r.choice(ROWS) if r.random() < 0.7 else r.getrandbits(16)],
               "quality": [r.choice(QUALITY) if r.random() < 0.7 else r.getrandbits(32)],
               "newnull": [r.choice((0, 0, 1)) for _ in range(n_subs)]}}
    c.update(kw)
    return c


def rnd_of(op):
    def rnd(r):
        if op == THING:
            idx = r.choice(IDXS) if r.random() < 0.5 else r.getrandbits(16)
            model = r.choice(MODELS) if r.random() < 0.6 else r.getrandbits(32)
            return mk(op, r, idx=idx, model=model)
        return mk(op, r)
    return rnd


def pins(nulls):
    """The queue "newnull" for a class with len(nulls) subscribe blocks."""
    return {"newnull": list(nulls)}


def fail_patterns(n):
    """The newnull queues worth running for n subscribe blocks: no failure, the first k fail, every block fails, alternate."""
    patterns = [[0] * n]
    patterns += [[1] * k + [0] for k in range(1, n + 1)]
    patterns += [[1] * n, [i % 2 for i in range(n)], [(i + 1) % 2 for i in range(n)]]
    if n == 2:
        patterns += [[0, 1], [1, 1]]
    return patterns


def thing_edges():
    r = random.Random(1000)
    cases = []
    for idx in IDXS:
        for fill in FILLS:
            cases.append(mk(THING, r, idx=idx, fill=fill))
    for model in MODELS:
        cases.append(mk(THING, r, model=model))
    for row in ROWS:
        for idx in (1, 0, 0x8001):
            cases.append(mk(THING, r, idx=idx, q={"row": [row]}))
    for world in MODES:
        for base in MAPS:
            for idx in (1, 0, 0xFFFF):
                cases.append(mk(THING, r, idx=idx, world=world, map=base))
    return cases


def derived_edges(op):
    def edges():
        r = random.Random(2000 + op)
        cases = []
        subs = SUBS.get(op, [])
        for fill in FILLS:
            if not subs:
                for world in MODES:
                    cases.append(mk(op, r, fill=fill, world=world))
                continue
            for centre in ("none", "existing"):
                for pattern in fail_patterns(len(subs)):
                    cases.append(mk(op, r, fill=fill, centre=centre, q={"newnull": pattern, "row": [r.choice(ROWS)]}))
        if op == WEATHER:
            for quality in QUALITY:
                for rain in MODES:
                    cases.append(mk(op, r, rain=rain, q={"quality": [quality], "newnull": [0, 0]}))
        if op == ADVISOR:
            for row in ROWS:
                cases.append(mk(op, r, q={"row": [row]}))
        return cases
    return edges


def test(name, op, edges):
    return ThingTest(name, ppc_case, host_case, edge=edges, rnd=rnd_of(op))


TESTS = [
    test("CThing::CThing", THING, thing_edges),
    test("CStaffHQ::CStaffHQ", STAFF, derived_edges(STAFF)),
    test("CMechanicHQ::CMechanicHQ", MECH, derived_edges(MECH)),
    test("CAdvisor::CAdvisor", ADVISOR, derived_edges(ADVISOR)),
    test("CTagSystem::CTagSystem", TAG, derived_edges(TAG)),
    test("CResearchLab::CResearchLab", LAB, derived_edges(LAB)),
    test("CWeather::CWeather", WEATHER, derived_edges(WEATHER)),
    test("CUIMsgReceiver::CUIMsgReceiver", UIMSG, derived_edges(UIMSG)),
]
