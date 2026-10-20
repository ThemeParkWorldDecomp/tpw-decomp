"""w123a: seven integer-only park-loop entry points against the original PPC code.

Every outside callee records its logical arguments and state on both sides. Fakes
can change the kind after PostMessage, id after FinalDestroy/assert, cell head
during assert, and gGameWorld after each weather provider call. Full object,
cell, table, world, balance and centre storage is checked after each script.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ThingLoop"
MASK = 0xffffffff
ADDR = [0x1012e230, 0x1012e728, 0x10102d34, 0x10152388, 0x10138e70, 0x1000a13c, 0x1012c6b8]
SIZES = [12, 12, 12, 12, 0x68, 0x16f0, 0x80e]
ROWS = [0, 1, 2, 5, 0x27ff]
CLASSES = ["CPerson", "CObject", "CMechanic", "CHandyman", "CEntertainer", "CGuard", "CResearcher",
           "CStaffHQ", "CMechanicHQ", "CAdvisor", "CTagSystem", "CParkAnalyser", "CResearchLab", "CWeather",
           "CBankAccount", "CUIMsgReceiver", "COnlinePerson", "CChallengeHQ"]
UPDATE = [0x1011f7d0, 0x1010d634, 0x101049e0, 0x100ff6b4, 0x100fb6b0, 0x100fe344, 0x10121340,
          None, None, 0x1000a8c8, None, 0x100e4188, None, 0x10138670, None, None, 0x10212ff0, 0x100f6b98]
MODEL = [0x1011fdb4, 0x1010daa0, 0x10104928, 0x100ffca8, 0x100fba44, 0x100fe4e8, 0x101213fc,
         None, None, None, None, None, None, None, None, None, 0x10213080, None]
FINAL = [0x10116da4, 0x10107828, 0x101043b0, 0x100ff2e0, 0x100fb600, 0x100fdd60, 0x1012123c,
         0x1012a998, 0x101052f4, 0x1000a0ec, 0x1012c5f8, 0x100e6fd0, 0x10121b7c, 0x10137f3c,
         0x100f2cb8, 0x101308d0, 0x10212f58, 0x100f6b60]
FIELDS = [(0, 2), (2, 1), (3, 1), (4, 1), (5, 1), (6, 1), (7, 1), (8, 2), (10, 2),
          (0x10, 4), (0xd4, 4), (0xdc, 4), (0xd8, 4)]
TABLE, WORLD, GAMEWORLD, CENTRE = 0x1034cbfc, 0x1037ec0c, 0x1037ebfc, 0x1037f6c4
GAME_TYPE, GAME_GUARD, BALANCE, SENTINEL = 0x102b3aa0, 0x102b3aa4, 0x102b4568, 0x102a0b24


def setup(runner):
    T.install_std_stubs(runner.emu)


def write32(e, address, value):
    e.write(address, struct.pack(">I", value & MASK))


def write16(e, address, value):
    e.write(address, struct.pack(">H", value & 0xffff))


def read16(e, address):
    return int.from_bytes(e.read(address, 2), "big")


def native_bytes(data, fields):
    out = bytearray(data)
    for offset, width in fields:
        out[offset:offset + width] = out[offset:offset + width][::-1]
    return bytes(out)


def report(data, fields, little=False):
    # Normalise scalars by value and retain every other byte, including guards.
    out = bytearray(data)
    values = []
    for offset, width in fields:
        values.append(int.from_bytes(out[offset:offset + width], "little" if little else "big"))
        out[offset:offset + width] = bytes(width)
    return values, bytes(out)


def prepare(case):
    r = random.Random(case["seed"])
    fill = lambda n: r.randbytes(n) if case["fill"] == "random" else bytes([case["fill"]]) * n
    block = bytearray(fill(0x1730))
    block[0:2] = case["id"].to_bytes(2, "big")
    block[2] = case["kind"]
    block[3] = case["dead"]
    cell = bytearray(fill(0x84))
    cell[0x24:0x26] = case["head"].to_bytes(2, "big")
    things = [fill(0x4c) for _ in ROWS]
    balance = [r.getrandbits(32) for _ in range(504)]
    for season, values in enumerate(case["weather"]):
        balance[287 + 3 * season:290 + 3 * season] = values
    return dict(block=bytes(block), cell=bytes(cell), things=things, balance=balance,
                worlds=[fill(0x2e0), fill(0x2e0)], low=r.getrandbits(16))


def ppc_case(e, c, d):
    L = CL.PpcLog({}, c["q"])
    e.cl = L
    block = e.alloc(0x1730, d["block"])
    worlds = [e.alloc(0x2e0, data) for data in d["worlds"]]
    centre_base = e.alloc(0x1f0, b"\xa5" * 0x1f0)
    centre = centre_base + 16
    fresh = e.alloc(0x1d0, b"\xa5" * 0x1d0)
    cell = e.alloc(0x84, d["cell"])
    things = [e.alloc(0x4c, data) for data in d["things"]]
    L.labels.update({block: 2, worlds[0]: 1, centre: 3, worlds[1]: 4, cell: 5})
    L.labels.update({address: 10 + i for i, address in enumerate(things)})
    ai = [0]
    death_state = [None, False]

    def snap(address):
        return e.u32(address)

    def change_world():
        next_world = L.ret("worldnext", -1)
        if next_world >= 0:
            write32(e, GAMEWORLD, worlds[next_world])

    def death_restored():
        if death_state[1]:
            message = death_state[0]
            L.rec("DeathRestored", [int(e.u32(message) == 0x102a438c), e.u32(message + 4), read16(e, message + 8)])

    def assertmsg(emu):
        if c["op"] == 1:
            death_restored()
        L.rec("GAME_ASSERTMSG", [T.s32(emu.r(3)), snap(block), read16(e, cell + 0x24)],
              [e.cstr(emu.r(4))[:159]])
        if c["op"] == 2 and ai[0] < 2:
            value = c["assertheads"][ai[0]]
            ai[0] += 1
            if value >= 0:
                write16(e, cell + 0x24, value)
        if c["op"] == 1 and c["finalid"] >= 0:
            write16(e, block, c["finalid"])

    def turn(name, final=False):
        def fake(emu):
            if final:
                death_restored()
            address = emu.r(3)
            L.rec(name, [L.lab(address), snap(address)])
            if final and c["finalid"] >= 0:
                write16(e, address, c["finalid"])
        return fake

    def game_type(emu):
        L.rec("CGameType", [1])
        write32(e, emu.r(3), c["type"])

    def thing_id(emu):
        thing = emu.r(4)
        answer = L.ret("id", read16(e, thing)) & 0xffff
        L.rec("GetThingId", [L.lab(thing), snap(thing), answer])
        write16(e, emu.r(3), answer)

    def death(emu):
        address, logical_id = emu.r(3), emu.r(4) >> 16
        L.rec("CMsgDeath", [logical_id])
        write32(e, address, 0x102a438c)
        write32(e, address + 4, 0)
        write16(e, address + 8, logical_id)

    def post(emu):
        msg = emu.r(4)
        L.rec("PostMessage", [L.lab(emu.r(3)), 27, e.u32(msg + 4), read16(e, msg + 8), snap(block)])
        death_state[:] = [msg, bool(L.ret("poisonmsg", 0))]
        if death_state[1]:
            write32(e, msg, 0)
            write32(e, msg + 4, 0x87654321)
            write16(e, msg + 8, 0x8001)
        if c["postkind"] >= 0:
            e.write(block + 2, bytes([c["postkind"]]))
        emu.set_r(3, L.ret("post", 1) & MASK)

    def pending(emu):
        death_restored()
        L.rec("Pending", [L.lab(emu.r(3)), emu.r(4) >> 16, snap(block)])

    def rand(emu):
        answer = L.ret("rand", 0) & MASK
        L.rec("GetRand", [L.lab(emu.r(3)), answer])
        change_world()
        emu.set_r(3, answer)

    def season(emu):
        answer = L.ret("season", 0)
        L.rec("GetSeason", [L.lab((emu.r(3) - 0x2a0) & MASK), 0x2a0, answer])
        change_world()
        emu.set_r(3, answer & MASK)

    def insert(emu):
        centre_word = e.u32(CENTRE)
        slot = T.s32((emu.r(4) - centre_word - 4) & MASK) // 16
        inserted, node = L.ret("inserted", 0), L.ret("node", 0)
        L.rec("insert_one", [L.lab(centre_word), slot, read16(e, emu.r(5)), node, inserted,
                             snap(block)] + [e.u32(block + offset) for offset in (0x10, 0xd4, 0xdc, 0xd8)])
        write32(e, emu.r(3), node)
        e.write(emu.r(3) + 4, bytes([inserted]))
        if L.ret("clearcentre", 0):
            write32(e, CENTRE, 0)

    def new(emu):
        fail = int(L.ret("newnull", 0) != 0)
        L.rec("new", [emu.r(3), fail])
        emu.set_r(3, 0 if fail else fresh)

    def construct(emu, call):
        L.rec("CMessageCentre", [L.newlab(emu.r(3)), int(emu.r(4) == 0x102657e8),
                                  int(emu.r(5) == 0x10267288), emu.r(6), emu.r(7)])

    st = CL.Stubs()
    st.real = set(ADDR)
    for names, addresses, final in (("UpdateTurn", UPDATE, False), ("ModelState", MODEL, False), ("FinalDestroy", FINAL, True)):
        for cls, address in zip(CLASSES, addresses):
            if address:
                st.internal(address, turn(cls + "::" + names, final))
    st.internal(0x1000dde8, assertmsg)
    st.internal(0x1006fcbc, CL.recorder("CTRACE", "is"))
    st.internal(0x1016f5d8, game_type)
    st.internal(0x1012e1f8, thing_id)
    st.internal(0x10152264, death)
    st.internal(0x1015256c, post)
    st.internal(0x1013b154, pending)
    st.internal(0x1013b1e4, rand)
    st.internal(0x101129e8, season)
    st.internal(0x101530e4, insert)
    st.imp("__nw__FUl", new)
    saved = st.activate(e)
    try:
        T.stub_cb(e, "__construct_array", construct)
        e.write(TABLE, b"\xa5" * (0x2800 * 20))
        for row, thing in zip(ROWS, things):
            write32(e, TABLE + row * 20, thing if row or c["rowzero"] else 0)
        initial_table = e.read(TABLE, 0x2800 * 20)
        write32(e, WORLD, worlds[0])
        write32(e, GAMEWORLD, worlds[c["gameworld"]])
        write32(e, CENTRE, centre if c["existing"] else 0)
        write32(e, GAME_TYPE, c["type"])
        e.write(GAME_GUARD, bytes([0 if c.get("first") else c["guard"]]))
        write16(e, SENTINEL, c["sentinel"])
        balance_bytes = struct.pack(">504I", *d["balance"])
        e.write(BALANCE + 8, balance_bytes)
        answers = []
        steps = []
        script = c["script"] if c["op"] == 2 else [c["arg"]] * c["repeat"]
        for argument in script:
            args = [block]
            if c["op"] == 2:
                args = [cell, (argument << 16) | d["low"]]
            if c["op"] == 3:
                args = [e.u32(CENTRE), block, argument & MASK]
            answer = T.call_ppc(e, ADDR[c["op"]], args)
            if c["op"] in (3, 4, 5):
                answers.append(answer[0] & MASK)
            steps.append(dict(block=report(e.read(block, 0x1730), FIELDS),
                              cell=report(e.read(cell, 0x84), [(0x24, 2)]),
                              things=[report(e.read(t, 0x4c), FIELDS[:9]) for t in things],
                              table_ok=e.read(TABLE, len(initial_table)) == initial_table))
        out = dict(answers=answers, block=report(e.read(block, 0x1730), FIELDS),
                   cell=report(e.read(cell, 0x84), [(0x24, 2)]),
                   things=[report(e.read(t, 0x4c), FIELDS[:9]) for t in things],
                   world_ok=[e.read(w, 0x2e0) == data for w, data in zip(worlds, d["worlds"])],
                   balance_ok=e.read(BALANCE + 8, len(balance_bytes)) == balance_bytes,
                   table_ok=e.read(TABLE, len(initial_table)) == initial_table,
                   centre_ok=e.read(centre_base, 0x1f0) == b"\xa5" * 0x1f0,
                   fresh_ok=e.read(fresh, 0x1d0) == b"\xa5" * 0x1d0,
                   centre=L.lab(e.u32(CENTRE)), gameworld=L.lab(e.u32(GAMEWORLD)),
                   log=L.log, steps=steps)
        if c["op"] == 0:
            out["type"] = T.s32(e.u32(GAME_TYPE))
            # The native compiler guard cannot be rewritten, but every PPC byte is checked.
            expected = 1 if c.get("first") else c["guard"]
            out["guard_ok"] = e.read(GAME_GUARD, 1)[0] == expected
        return out
    finally:
        CL.Stubs.deactivate(e, saved)


def host_case(lib, c, d):
    lib.tpw_tl_pointer.argtypes = [C.c_int32]
    lib.tpw_tl_pointer.restype = C.c_void_p
    lib.tpw_tl_copy.argtypes = [C.c_int32, C.c_void_p, C.c_int32, C.c_int32]
    lib.tpw_tl_config.argtypes = [C.c_int32] * 9
    lib.tpw_tl_balance.argtypes = [C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_tl_row.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_tl_call.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_tl_call.restype = C.c_uint32
    lib.tpw_tl_config(c["op"], c["existing"], c["type"], c["postkind"], c["finalid"],
                      *c["assertheads"], c["sentinel"], c["gameworld"])

    def load(which, data):
        lib.tpw_tl_copy(which, C.c_char_p(data), len(data), 1)

    def dump(which, n):
        data = C.create_string_buffer(n)
        lib.tpw_tl_copy(which, data, n, 0)
        return data.raw

    load(0, native_bytes(d["block"], FIELDS))
    load(3, native_bytes(d["cell"], [(0x24, 2)]))
    for i, data in enumerate(d["things"]):
        load(10 + i, native_bytes(data, FIELDS[:9]))
        lib.tpw_tl_row(ROWS[i], i if i or c["rowzero"] else -1)
    for i, data in enumerate(d["worlds"]):
        load(1 if i == 0 else 4, data)
    values = (C.c_uint32 * 504)(*d["balance"])
    lib.tpw_tl_balance(values, 1)
    lib.tpw_tl_snapshot_table()
    if not c.get("first"):
        lib.tpw_tl_mode(c["type"])
    labels = {2: lib.tpw_tl_pointer(0), 1: lib.tpw_tl_pointer(1), 3: lib.tpw_tl_pointer(2),
              4: lib.tpw_tl_pointer(4), 5: lib.tpw_tl_pointer(3)}
    labels.update({10 + i: lib.tpw_tl_pointer(10 + i) for i in range(5)})
    CL.host_begin(lib, labels, c["q"])
    answers = []
    steps = []
    try:
        script = c["script"] if c["op"] == 2 else [c["arg"]] * c["repeat"]
        for argument in script:
            answer = lib.tpw_tl_call(c["op"], argument)
            if c["op"] in (3, 4, 5):
                answers.append(answer)
            steps.append(dict(block=report(dump(0, 0x1730), FIELDS, True),
                              cell=report(dump(3, 0x84), [(0x24, 2)], True),
                              things=[report(dump(10 + i, 0x4c), FIELDS[:9], True) for i in range(5)],
                              table_ok=lib.tpw_tl_table_guard() == -1))
    finally:
        log = CL.host_end(lib)
    lib.tpw_tl_balance(values, 0)
    out = dict(answers=answers, block=report(dump(0, 0x1730), FIELDS, True),
               cell=report(dump(3, 0x84), [(0x24, 2)], True),
               things=[report(dump(10 + i, 0x4c), FIELDS[:9], True) for i in range(5)],
               world_ok=[dump(1 if i == 0 else 4, 0x2e0) == data for i, data in enumerate(d["worlds"])],
               balance_ok=list(values) == d["balance"] and lib.tpw_tl_balance_guard() == -1,
               table_ok=lib.tpw_tl_table_guard() == -1, centre_ok=bool(lib.tpw_tl_centre_guard()),
               fresh_ok=bool(lib.tpw_tl_fresh_guard()),
               centre=lib.tpw_tl_centre(), gameworld=lib.tpw_tl_gameworld(), log=log, steps=steps)
    if c["op"] == 0:
        out["type"] = lib.tpw_tl_type()
        out["guard_ok"] = True
    return out


class LoopTest(FnTest):
    def run_case(self, runner, case):
        runner.emu.reset_scratch()
        data = prepare(case)
        p = ppc_case(runner.emu, case, data)
        h = host_case(runner.lib, case, data)
        return T.same_values(p, h, "", False)


def mk(op, r, **kw):
    weather = [[r.getrandbits(32), r.choice([0xffffffff, 0x80000000, 0, 1, 2, 99, 0x40000000, 0x7fffffff]),
                r.choice([0, 1, 99, 100, 101, 0x80000000, MASK])] for _ in range(4)]
    seasons = [r.randrange(4) for _ in range(5)]
    # No C++ remainder with a zero denominator: the third season is independent
    # of the signed branch test, so make only its zero tolerance nonzero.
    if weather[seasons[2]][1] in (0, 0x80000000):
        weather[seasons[2]][1] = 1
    case = dict(op=op, seed=r.getrandbits(32), fill=r.choice([0, 0xa5, 0xff, "random"]),
                id=r.getrandbits(16), kind=r.randrange(256), dead=r.choice([0, 1, 0xff]),
                type=r.choice([0, 1, 2, -1, -2147483648, 2147483647]), guard=r.choice([1, 0x7f, 0x80, 0xff]),
                existing=r.randrange(2), gameworld=r.randrange(2), rowzero=r.randrange(2),
                postkind=r.choice([-1, 0, 1, 2, 19, 20, 255]), finalid=r.choice([-1, 0, 1, 0x8000, 0xffff]),
                head=r.choice(ROWS), sentinel=0, assertheads=[-1, -1], script=[r.choice(ROWS[1:])], repeat=1,
                arg=r.choice(list(range(31)) + [-1, 2147483647]), weather=weather,
                q=dict(inserted=[r.randrange(2) for _ in range(7)], node=[r.getrandbits(32) for _ in range(7)],
                       id=[r.getrandbits(16) for _ in range(7)], rand=[r.getrandbits(32), r.getrandbits(32)],
                       season=seasons, worldnext=[r.choice([-1, 0, 1]) for _ in range(7)], newnull=[0]))
    if op == 3:
        case["existing"] = 1
    case.update(kw)
    return case


def edges(op):
    def make():
        r = random.Random(12300 + op)
        result = []
        if op == 0:
            result.append(mk(op, r, kind=11, type=1, first=True))
            result += [mk(op, r, kind=kind) for kind in range(256)]
            result += [mk(op, r, kind=kind, type=mode, guard=guard) for kind in (11, 18, 19)
                       for mode in (0, 1, 2, -1, -2147483648, 2147483647) for guard in (1, 0x7f, 0x80, 0xff)]
        elif op == 1:
            result += [mk(op, r, kind=kind, postkind=-1, existing=existing, finalid=-1, id=idx, dead=dead)
                       for kind in list(range(21)) + [0x7f, 0x80, 0xff] for existing in (0, 1)
                       for idx, dead in ((0, 0), (1, 1), (0x27ff, 0xff), (0x8000, 0), (0xffff, 1))]
            result += [mk(op, r, kind=kind, postkind=nextkind, finalid=idx) for kind in (0, 1, 19, 255)
                       for nextkind in (0, 1, 2, 19, 255) for idx in (0, 0x8000, 0xffff)]
            result.append(mk(op, r, kind=1, postkind=-1, repeat=2, finalid=-1))
            result.append(mk(op, r, existing=0, q={"newnull": [1]}))
            result += [mk(op, r, kind=kind, postkind=-1, q={"poisonmsg": [1]}) for kind in (0, 1, 2, 19, 20, 255)]
        elif op == 2:
            result += [mk(op, r, head=head, sentinel=sentinel, script=[idx], assertheads=heads,
                          rowzero=1 if head == 0 and sentinel != 0 else r.randrange(2))
                       for head, sentinel in ((0, 0), (1, 0), (2, 0), (0x27ff, 0), (2, 2), (5, 5), (0, 2))
                       for idx in ROWS[1:] for heads in ([-1, -1], [-1, 5], [2, 1])]
            for seq in ([1], [1, 2], [1, 2, 5, 1, 0x27ff], [1, 1]):
                result.append(mk(op, r, head=0, script=seq))
        elif op == 3:
            result += [mk(op, r, arg=typ, id=idx, q={"inserted": [inserted], "node": [node], "id": [idx]})
                       for typ in (-1, 0, 1, 28, 29, 30, 2147483647) for idx in (0, 0x7fff, 0x8000, 0xffff)
                       for inserted, node in ((0, 0x12345678), (1, 0))]
            result += [mk(op, r, arg=typ, existing=0) for typ in (29, 30, 2147483647)]
        elif op == 4:
            vals = [0, 1, 99, 100, 101, 0x7fffffff, 0x80000000, MASK]
            for chance in (0, 1, 99, 100, 101, 0x80000000, MASK):
                for draw in vals + [max(0, chance - 1) % 100, chance % 100, (chance + 1) % 100]:
                    result.append(mk(op, r, weather=[[99, 2, chance]] * 4,
                                     q={"season": [0], "rand": [draw, MASK]}))
            for tolerance in (MASK, 0x80000000, 0, 1, 2, 99, 0x40000000, 0x7fffffff):
                for average in vals:
                    result.append(mk(op, r, weather=[[average, tolerance, 0]] * 4,
                                     q={"season": [0], "rand": [0, MASK]}))
            result.append(mk(op, r, weather=[[10, 1, 0], [20, 7, 0], [MASK, 4, 0], [40, 3, 0]],
                             q={"season": [0, 1, 2, 3, 0], "rand": [0, MASK], "worldnext": [1, 0, 1, 0, 1, 0, 1]}))
        else:
            n = 7 if op == 5 else 6
            for fill in (0, 0xa5, 0xff, "random"):
                for existing in (0, 1):
                    for failed in range(n + 1):
                        result.append(mk(op, r, fill=fill, existing=existing,
                                         q={"newnull": [1] * failed + [0], "inserted": [0, 1], "node": [0, MASK]}))
            result.append(mk(op, r, existing=0, q={"newnull": [1], "inserted": [0]}))
            result += [mk(op, r, existing=existing,
                          q={"clearcentre": [1], "newnull": [0, 1, 0], "inserted": [0, 1]})
                       for existing in (0, 1)]
        return result
    return make


TESTS = [LoopTest(name, ppc_case, host_case, edge=edges(op), rnd=lambda r, op=op: mk(op, r))
         for op, name in enumerate(("CThing::Process", "CThing::KillMe", "CMapCell::InsertThing",
                                    "CMessageCentre::Subscribe", "CWeather::FindNewQuality", "CAdvisor::Init", "CTagSystem::Init"))]
