"""w123c: the death message and the message post, original code on one side and src/game/Thing.cpp, src/game/ui/MessageCentre.cpp, src/game/advisor/Advisor.cpp, src/game/park/BankAccount.cpp, src/game/save/BlockSave.cpp,
src/game/World.cpp and src/game/people/ParkTime.cpp on the other (tests/unit_DeathMessage.cpp).

Functions (main app): CMsgDeath::CMsgDeath 0x10152264, GetType 0x1012ec14, ~CMsgDeath 0x1012e984; CMessageCentre::PostMessage 0x1015256c (w107's
translation, tested again here with every callee faked); CWorld::AddToPendingDeleteList 0x1013b154 and GetRand 0x1013b1e4; CParkTime::GetSeason
0x101129e8 (the compiler's signed 64 bit divide 0x10237e1c runs as the original).

Fakes, recording the same event on both sides: the assertion, CThing::GetThingId and ReceiveMessage, the subscription tree (find, erase, the global
increment; begin is host-only, the Mac reads the word), the pending list's AddHead, the world accessor and TbTimeStamp::operator+ and GetTime. The
death message's own GetType is real; a message of another type is a test object whose GetType records and answers the case's type.

The scripted trees are chains: node i has an id and a next index; the delivery path starts at `first` (the tree's begin word on the Mac). A fake
find answers a scripted node of the slot; the fake erase takes a node off the delivery path; the fake increment follows the path. The assertion's
callback can change a table row or a node's id at a chosen call, which shows when each value is read. The worlds are not blocks of their Mac size:
the 0x80 bytes at world + 0x1da6f0 are real memory and the world pointer is that block's address minus 0x1da6f0 on both sides.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DeathMessage"
PROGRAM = "SimTheme Park"
MASK = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF
CTOR, GETTYPE, DTOR, POST, PENDING, SEASON, RAND = 0x10152264, 0x1012EC14, 0x1012E984, 0x1015256C, 0x1013B154, 0x101129E8, 0x1013B1E4
HELPER = 0x10237E1C
ASSERT, THING_ID, RECEIVE, FIND, ERASE, INCREMENT = 0x1000DDE8, 0x1012E1F8, 0x1012E470, 0x10152C58, 0x10152F3C, 0x10152DB0
ADD_HEAD, GET_WORLD = 0x10141AD4, 0x1014205C
TABLE, PENDING_LIST = 0x1034CBFC, 0x1034CBD8
DEATH_VTABLE = 0x102A438C
OWN = {CTOR, GETTYPE, DTOR, POST, PENDING, SEASON, RAND, HELPER}
ROW_TEXT = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n"
POISON = b"\xa5"
TAIL0 = 0x1DA6F0
START_WORD, TURN_WORD = (0x1DA708 - TAIL0) // 4, (0x1DA70C - TAIL0) // 4
CLOCK_AT = 0x40
CLOCK_SIZE = CLOCK_AT + 0x24 + 0x40
CLOCK_FIELDS = [(CLOCK_AT, 8), (CLOCK_AT + 0x1C, 4)]
TAIL_WORDS = [(4 * i, 4) for i in range(32)]
U32S = [0, 1, 2, 3, 4, 5, 15000, 0x7FFFFFFF, 0x80000000, MASK]
TICKS = [0, 1, MASK, 0x100000000, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000, M64, 0x01B21DD213814000]
IDS = [0, 1, 2, 0x7FFF, 0x8000, 0xFFFE, 0xFFFF, 0x1234, 0x5566]
MARK_FLAGS = 0xA5A5A5A5


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


def s64(v):
    v &= M64
    return v - (1 << 64) if v & (1 << 63) else v


def w32(e, address, value):
    e.write(address, struct.pack(">I", value & MASK))


def w16(e, address, value):
    e.write(address, struct.pack(">H", value & 0xFFFF))


def r16(e, address):
    return int.from_bytes(e.read(address, 2), "big")


def be64(e, address):
    return int.from_bytes(e.read(address, 8), "big")


# ----------------------------------------------------------------- shared plumbing

def op_delete(e):
    """operator delete: counted, recorded when the block is labelled (as tests/calllog.cpp does)."""
    p = e.r(3)
    if p:
        e.cl.deletes += 1
        if e.cl.lab(p) > 0:
            e.cl.rec("delete", (e.cl.lab(p),))
            e.cl.forget(p)


def new_log(e, queues=None):
    e.cl = CL.PpcLog({}, queues or {})
    e.cl.deletes = 0
    e.news.clear()
    return e.cl


def finish_ppc(e, **fields):
    out = dict(fields)
    out["log"] = e.cl.log
    out["deletes"] = e.cl.deletes
    out["news"] = len(e.news)
    return out


def stubs_with(internals, imports=None):
    s = CL.Stubs()
    s.real.update(OWN)
    s.imp("__dl__FPv", op_delete)
    for addr, fn in internals.items():
        s.internal(addr, fn)
    for name, fn in (imports or {}).items():
        s.imp(name, fn)
    return s


def run_stubbed(e, s, fn):
    saved = s.activate(e)
    try:
        return fn()
    finally:
        CL.Stubs.deactivate(e, saved)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def get_type(emu):
        emu.cl.rec("GetType", (emu.cl.lab(emu.r(3)),))
        emu.set_r(3, T.u32(emu.cl.msg_type))

    # A non-death message's vtable: slot 2 (+8) is GetType, called through the glue with a TVector like every virtual.
    tvec = T.fake_function(e, get_type)
    e.dm_vtable = T.persist_alloc(e, 0x20)
    e.write(e.dm_vtable + 8, struct.pack(">I", tvec))
    # The death vtable's GetType slot leads to the original function (the descriptor at +8 is read through the glue).
    descriptor = e.u32(DEATH_VTABLE + 8)
    assert e.u32(descriptor) == GETTYPE, hex(e.u32(descriptor))


def api(lib):
    if getattr(lib, "dm_ready", False):
        return lib
    P, i32, u32 = C.POINTER(C.c_int32), C.c_int32, C.c_uint32
    PU = C.POINTER(C.c_uint32)
    lib.tpw_dm_ctor.argtypes = [u32, PU]
    lib.tpw_dm_gettype.argtypes = [u32, u32, PU]
    lib.tpw_dm_dtor.argtypes = [i32, i32, PU]
    lib.tpw_dm_post_begin.argtypes = []
    lib.tpw_dm_set_thing.argtypes = [i32, i32, i32]
    lib.tpw_dm_set_chain.argtypes = [i32, i32, P, P, i32]
    lib.tpw_dm_mut.argtypes = [i32] * 5
    lib.tpw_dm_post.argtypes = [i32, i32, i32, PU]
    lib.tpw_dm_post.restype = i32
    lib.tpw_dm_chain.argtypes = [i32, P]
    lib.tpw_dm_row_label.argtypes = [i32]
    lib.tpw_dm_row_label.restype = i32
    lib.tpw_dm_pending_set.argtypes = [i32]
    lib.tpw_dm_pending.argtypes = [i32, u32]
    lib.tpw_dm_pending_count.restype = i32
    lib.tpw_dm_pending_links.restype = i32
    lib.tpw_dm_tail.argtypes = [i32]
    lib.tpw_dm_tail.restype = C.c_void_p
    lib.tpw_dm_clock.restype = C.c_void_p
    lib.tpw_dm_label_clock.argtypes = []
    lib.tpw_dm_rand.argtypes = [i32]
    lib.tpw_dm_rand.restype = u32
    lib.tpw_dm_season.restype = i32
    lib.dm_ready = True
    return lib


def heap(lib):
    return list((C.c_int32 * 4).in_dll(lib, "tpw_cl_heap"))


def host_log_start(lib, queues=None):
    api(lib)
    CL.host_begin(lib, {}, queues or {})
    return heap(lib)


def host_finish(lib, before, **fields):
    out = dict(fields)
    now = heap(lib)
    out["log"] = CL.host_end(lib)
    out["deletes"] = now[2] - before[2]
    out["news"] = now[0] - before[0]
    return out


# ----------------------------------------------------------------- CMsgDeath::CMsgDeath

def ctor_cases(r):
    out = []
    for ident in IDS:
        out.append(dict(id=ident, low=0))
        out.append(dict(id=ident, low=0xFFFF))
        out.append(dict(id=ident, low=r.getrandbits(16)))
    return out


def ctor_random(r):
    return dict(id=r.choice(IDS + [r.getrandbits(16)] * 4), low=r.getrandbits(16))


def ppc_ctor(e, c):
    new_log(e)
    obj = e.alloc(0x40, POISON * 0x40)
    ret = T.call_ppc(e, CTOR, [obj, (c["id"] << 16) | c["low"]])[0]
    return finish_ppc(e, same=int(ret == obj), flags=e.u32(obj + 4), ident=r16(e, obj + 8),
                      type=T.call_ppc(e, GETTYPE, [obj])[0], intact=int(e.read(obj + 0xA, 0x36) == POISON * 0x36),
                      vptr=int(e.u32(obj) == DEATH_VTABLE))


def host_ctor(lib, c):
    before = host_log_start(lib)
    out = (C.c_uint32 * 5)()
    lib.tpw_dm_ctor(c["id"], out)
    return host_finish(lib, before, same=1, flags=out[0], ident=out[1], type=out[2], intact=out[3], vptr=out[4])


# ----------------------------------------------------------------- CMsgDeath::GetType

def gettype_cases(r):
    out = []
    for flags in (0, 1, MASK, 0x80000000, MARK_FLAGS, 0x1B):
        for ident in (0, 0xFFFF, 0x1B, 0x8000):
            out.append(dict(flags=flags, id=ident))
    return out


def gettype_random(r):
    return dict(flags=r.getrandbits(32), id=r.getrandbits(16))


def ppc_gettype(e, c):
    new_log(e)
    obj = e.alloc(0x40, POISON * 0x40)
    e.write(obj, struct.pack(">IIHH", DEATH_VTABLE, c["flags"], c["id"], 0xA5A5))
    first = T.call_ppc(e, GETTYPE, [obj])[0]
    second = T.call_ppc(e, GETTYPE, [obj])[0]
    return finish_ppc(e, first=first, second=second, flags=e.u32(obj + 4), ident=r16(e, obj + 8),
                      rest=e.read(obj + 0xA, 0x36), vptr=e.u32(obj))


def host_gettype(lib, c):
    before = host_log_start(lib)
    out = (C.c_uint32 * 4)()
    lib.tpw_dm_gettype(c["flags"], c["id"], out)
    return host_finish(lib, before, first=out[0], second=out[1], flags=out[2], ident=out[3], rest=POISON * 0x36, vptr=DEATH_VTABLE)


# ----------------------------------------------------------------- CMsgDeath::~CMsgDeath

def dtor_cases(r):
    flags = [0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, MASK, 0x80000000, 0x12340001, 0x1234FFFF, 0x7FFF0000, 0xFFFF0001, 0xFFFF8000, 0x00010000]
    out = [dict(flag=f, present=1) for f in flags]
    out += [dict(flag=f, present=0) for f in (0, 1, 0x7FFF, 0x12340001, 0x8000, MASK)]
    return out


def dtor_random(r):
    return dict(flag=r.choice([r.getrandbits(32), r.getrandbits(16), r.getrandbits(16) | 0x80000000, 1, 0, MASK]), present=r.choice([1, 1, 1, 0]))


def ppc_dtor(e, c):
    new_log(e)
    obj = e.alloc(0x10, POISON * 0x10) if c["present"] else 0
    e.cl.labels[obj] = 3
    if obj:
        e.write(obj, struct.pack(">IIHH", 0x11111111, 0x11223344, 0x5566, 0xA5A5))
    ret = run_stubbed(e, stubs_with({}), lambda: T.call_ppc(e, DTOR, [obj, c["flag"]])[0])
    explicit = bool(obj) and s16(c["flag"]) <= 0
    return finish_ppc(e, same=int(ret == obj), vptr=int(obj == 0 or e.u32(obj) == DEATH_VTABLE),
                      flags=e.u32(obj + 4) if explicit else 0, ident=r16(e, obj + 8) if explicit else 0,
                      pad=r16(e, obj + 0xA) if explicit else 0)


def host_dtor(lib, c):
    before = host_log_start(lib)
    out = (C.c_uint32 * 2)()
    lib.tpw_dm_dtor(s32(c["flag"]), c["present"], out)
    explicit = c["present"] and s16(c["flag"]) <= 0
    return host_finish(lib, before, same=1, vptr=1, flags=out[0], ident=out[1], pad=0xA5A5 if explicit else 0)


# ----------------------------------------------------------------- CMessageCentre::PostMessage

ROW_POOL = [1, 2, 3, 5, 9, 0x10, 0x7F, 0x100, 0x400, 0x13FF, 0x2000, 0x27FF]


def mk_post(typ, things, dying=None, chains=None, found=None, idq=None, muts=None):
    """things: [(row, own id)] (a row of -1 is a thing outside the table); dying: the index of the dying thing (death messages);
    chains: {slot: (ids, path)} with path the node indexes (1 up) of the delivery path in order; found: {slot: node index}; idq: the
    values of GetThingId (a list); muts: [(call, kind, a, b, c)]."""
    return dict(type=typ, things=things, dying=dying, chains=chains or {}, found=found or {}, idq=idq or [], muts=muts or [])


def chain_arrays(ids, path):
    nexts = [0] * len(ids)
    for a, b in zip(path, path[1:]):
        nexts[a - 1] = b
    return nexts, (path[0] if path else 0)


def post_random(r, typ=None):
    typ = typ if typ is not None else r.choice([0x1B] * 6 + [0, 1, 5, 0x13, 0x1A, 0x1C, r.randrange(0x1D)])
    rows = r.sample(ROW_POOL, r.randint(3, 8))
    things = [(row, row if r.random() < 0.6 else r.getrandbits(16)) for row in rows]
    row0 = r.random() < 0.15
    if row0:
        things.append((0, 0))
    usable = list(range(len(things)))
    dying = None
    if typ == 0x1B:
        dying = r.choice([i for i in usable if things[i][0] != 0])
    slots = {typ} | set(r.sample(range(0x1D), r.randint(0, 4)))
    chains, found = {}, {}
    for slot in sorted(slots):
        n = r.choice([0, 1, 1, 2, 3, 4, 6, 8])
        ids = [things[r.choice(usable)][0] for _ in range(n)]
        order = list(range(1, n + 1))
        r.shuffle(order)
        path = order[:r.randint(0, n)] if r.random() < 0.4 else list(range(1, n + 1))
        chains[slot] = (ids, path)
    if typ == 0x1B:
        for slot in range(0x1D):
            if r.random() < 0.25:
                n = len(chains[slot][0]) if slot in chains else 0
                if n:
                    found[slot] = r.randint(1, n)
    idq = []
    if typ == 0x1B:
        own = things[dying][1]
        idq = [own if r.random() < 0.7 else r.getrandbits(16) for _ in range(0x1D)]
    muts = []
    if r.random() < 0.3:
        for _ in range(r.randint(1, 3)):
            call = r.randrange(4)
            if r.random() < 0.5:
                row = r.choice([t[0] for t in things] + [0])
                j = r.randrange(len(things)) if row != 0 or r.random() < 0.5 else -1
                muts.append((call, 0, row, j, 0))
            else:
                slot = r.choice(sorted(chains))
                if chains[slot][0]:
                    muts.append((call, 1, slot, r.randint(1, len(chains[slot][0])), things[r.choice(usable)][0]))
    return mk_post(typ, things, dying, chains, found, idq, muts)


def post_edges():
    r = random.Random(1237)
    out = []
    base = [(5, 5), (9, 5), (12, 12), (0x27FF, 0x27FF), (1, 1)]
    # non-death types: an empty tree, one node, several (in path order), at both ends of the type range
    for typ in (0, 1, 0x13, 0x1A, 0x1C):
        out.append(mk_post(typ, base, None, {typ: ([], [])}))
        out.append(mk_post(typ, base, None, {typ: ([5], [1])}))
        out.append(mk_post(typ, base, None, {typ: ([12, 5, 0x27FF, 1], [1, 2, 3, 4]), 0x1B: ([5, 9], [1, 2])}))
    # a death with no subscriptions, one found header, the first, the last, every header found
    idq = [9] * 29
    out.append(mk_post(0x1B, base, 1, {}, {}, idq))
    for slot in (0, 13, 27, 28):
        out.append(mk_post(0x1B, base, 1, {slot: ([5, 9], [1, 2])}, {slot: 2}, idq))
    chains = {s: ([5, 9, 12], [1, 2, 3]) for s in range(0x1D)}
    out.append(mk_post(0x1B, base, 1, chains, {s: 1 + s % 3 for s in range(0x1D)}, idq))
    # the dying thing's subscription to the death message is removed before the delivery (first, middle, last, alone)
    for path, k in (([1, 2, 3], 1), ([1, 2, 3], 2), ([1, 2, 3], 3), ([2], 2)):
        out.append(mk_post(0x1B, base, 1, {0x1B: ([5, 9, 12], path)}, {0x1B: k}, idq))
    # the message selects row 9, whose thing reports its own id 5; GetThingId answers a different id on each of the 29 calls
    out.append(mk_post(0x1B, [(9, 5), (5, 5), (12, 12)], 0, {0x1B: ([5, 9, 12], [1, 2, 3]), 3: ([5], [1])}, {3: 1}, [5] * 29))
    out.append(mk_post(0x1B, [(9, 5), (5, 5), (12, 12)], 0, {s: ([5, 9], [1, 2]) for s in range(0x1D)}, {s: 1 + s % 2 for s in range(0x1D)},
                       list(range(100, 129))))
    # begin == end (an empty path with nodes that are not on it), one node, several
    out.append(mk_post(0x1B, base, 1, {0x1B: ([5, 9], [])}, {0x1B: 1}, idq))
    out.append(mk_post(2, base, None, {2: ([5, 9], [])}))
    # the table's rows 0 and 0x27ff; row 0 occupied (the assertion fails before every delivery)
    out.append(mk_post(6, [(0x27FF, 0x27FF), (0, 0), (1, 1)], None, {6: ([0x27FF, 0, 1], [1, 2, 3])}))
    out.append(mk_post(0x1B, [(0x27FF, 0x27FF), (0, 0), (1, 1)], 0, {0x1B: ([0x27FF, 0, 1], [1, 2, 3])}, {0x1B: 2}, [0x27FF] * 29))
    # the assertion's callback: a row changes before the dying thing is read (call 0), a later node's id changes (call 1), a row changes before the
    # receiver is read (calls 1 and 2), row 0 is freed or occupied by the callback
    things = [(9, 9), (5, 5), (12, 12), (7, 7), (0, 0)]
    chains = {0x1B: ([5, 9, 12], [1, 2, 3])}
    out.append(mk_post(0x1B, things, 0, chains, {0x1B: 1}, [9] * 29, [(0, 0, 9, 3, 0)]))
    out.append(mk_post(0x1B, things, 0, chains, {0x1B: 1}, [9] * 29, [(1, 1, 0x1B, 3, 7)]))
    out.append(mk_post(0x1B, things, 0, chains, {0x1B: 1}, [9] * 29, [(1, 0, 12, 3, 0), (2, 0, 5, 1, 0)]))
    out.append(mk_post(0x1B, things, 0, chains, {}, [9] * 29, [(0, 0, 0, 4, 0)]))
    out.append(mk_post(0x1B, things, 0, chains, {}, [9] * 29, [(1, 0, 0, -1, 0)]))
    out.append(mk_post(4, things, None, {4: ([5, 9, 12], [1, 2, 3])}, {}, [], [(0, 0, 5, 2, 0), (1, 0, 9, 3, 0), (2, 1, 4, 3, 5)]))
    for _ in range(30):
        out.append(post_random(r))
    return out


def post_queues(c):
    q = {}
    if c["idq"]:
        q["id"] = list(c["idq"])
    for slot, index in c["found"].items():
        q["find:%d" % slot] = [index]
    return q


def ppc_post(e, c):
    L = new_log(e, post_queues(c))
    L.msg_type = c["type"]
    centre = e.alloc(0x1D0, POISON * 0x1D0)
    L.labels[centre] = 6
    state = {}      # slot -> dict(first, next{index: index}, addr{index: address}, rev{address: index}, end)
    for slot in range(0x1D):
        header = centre + 0x10 * slot
        ids, path = c["chains"].get(slot, ([], []))
        nexts, first = chain_arrays(ids, path)
        addr = {}
        for i, ident in enumerate(ids, 1):
            a = e.alloc(0x10, POISON * 0x10)
            e.write(a + 0xC, struct.pack(">HH", ident, 0xA5A5))
            addr[i] = a
            L.labels[a] = 200 + slot * 9 + i
        end = header + 8
        e.write(header + 4, struct.pack(">III", len(ids), 0, addr[first] if first else end))
        state[slot] = dict(first=first, next={i + 1: n for i, n in enumerate(nexts)}, addr=addr, rev={a: i for i, a in addr.items()}, end=end,
                           header=header)
    rows = {}
    for j, (row, own) in enumerate(c["things"]):
        a = e.alloc(0x10, struct.pack(">HB", own, 0) + POISON * 13)
        L.labels[a] = 10 + j
        rows[j] = a
    touched = {row for row, _ in c["things"] if row >= 0} | {m[2] for m in c["muts"] if m[1] == 0} | {0}
    table_value = {row: 0 for row in touched}
    for j, (row, _) in enumerate(c["things"]):
        if row >= 0:
            table_value[row] = rows[j]
    for row, value in table_value.items():
        w32(e, TABLE + 0x14 * row, value)
    if c["type"] == 0x1B:
        msg = e.alloc(0x20, POISON * 0x20)
        ident = c["things"][c["dying"]][0]
        T.call_ppc(e, CTOR, [msg, (ident << 16) | 0xBEEF])
        w32(e, msg + 4, MARK_FLAGS)
    else:
        msg = e.alloc(0x20, struct.pack(">IIHH", e.dm_vtable, MARK_FLAGS, 0xA5A5, 0xA5A5) + POISON * 0x14)
    L.labels[msg] = 2
    calls = [0]

    def assertion(emu):
        L.rec("GAME_ASSERTMSG", [emu.r(3) & 0xFF], [e.cstr(emu.r(4))[:159]])
        call = calls[0]
        calls[0] += 1
        for (mcall, kind, a, b, cc) in c["muts"]:
            if mcall != call:
                continue
            if kind == 0:
                value = rows[b] if b >= 0 else 0
                w32(e, TABLE + 0x14 * a, value)
            else:
                st = state[a]
                e.write(st["addr"][b] + 0xC, struct.pack(">H", cc))

    def thing_id(emu):
        thing = emu.r(4)
        answer = L.ret("id", r16(e, thing)) & 0xFFFF
        L.rec("GetThingId", [L.lab(thing), answer])
        w16(e, emu.r(3), answer)

    def receive(emu):
        L.rec("ReceiveMessage", [L.lab(emu.r(3)), L.lab(emu.r(4))])

    def slot_of(tree):
        return (tree - 4 - centre) // 16

    def find(emu):
        slot = slot_of(emu.r(4))
        L.rec("find", [slot, r16(e, emu.r(5))])
        index = L.ret("find:%d" % slot, 0)
        st = state[slot]
        w32(e, emu.r(3), st["addr"][index] if index else st["end"])

    def erase(emu):
        slot = slot_of(emu.r(3))
        st = state[slot]
        node = emu.r(4)
        L.rec("erase", [slot, L.lab(node)])
        index = st["rev"].get(node, 0)
        if index == 0:
            return
        if st["first"] == index:
            st["first"] = st["next"][index]
            w32(e, st["header"] + 0xC, st["addr"][st["first"]] if st["first"] else st["end"])
        else:
            for i in st["next"]:
                if st["next"][i] == index:
                    st["next"][i] = st["next"][index]

    def increment(emu):
        slot_iter = emu.r(3)
        node = e.u32(slot_iter)
        L.rec("increment", [L.lab(node)])
        for st in state.values():
            index = st["rev"].get(node)
            if index:
                nxt = st["next"][index]
                w32(e, slot_iter, st["addr"][nxt] if nxt else st["end"])
                return

    s = stubs_with({ASSERT: assertion, THING_ID: thing_id, RECEIVE: receive, FIND: find, ERASE: erase, INCREMENT: increment})

    def call():
        return T.call_ppc(e, POST, [centre, msg])[0]

    try:
        ret = run_stubbed(e, s, call)
        out = finish_ppc(e, ret=ret, flags=e.u32(msg + 4), ident=r16(e, msg + 8) if c["type"] == 0x1B else 0xA5A5,
                         chains={slot: dict(first=st["first"], next=sorted(st["next"].items()),
                                            begin=st["rev"].get(e.u32(st["header"] + 0xC), 0)) for slot, st in state.items()},
                         table={row: L.lab(e.u32(TABLE + 0x14 * row)) for row in sorted(touched)})
    finally:
        for row in touched:
            w32(e, TABLE + 0x14 * row, 0)
    return out


def host_post(lib, c):
    before = host_log_start(lib, post_queues(c))
    lib.tpw_dm_post_begin()
    touched = {row for row, _ in c["things"] if row >= 0} | {m[2] for m in c["muts"] if m[1] == 0} | {0}
    for j, (row, own) in enumerate(c["things"]):
        lib.tpw_dm_set_thing(j, row, own)
    for slot, (ids, path) in c["chains"].items():
        nexts, first = chain_arrays(ids, path)
        n = len(ids)
        lib.tpw_dm_set_chain(slot, n, (C.c_int32 * max(n, 1))(*ids), (C.c_int32 * max(n, 1))(*nexts), first)
    for call, kind, a, b, cc in c["muts"]:
        lib.tpw_dm_mut(call, kind, a, b, cc)
    out = (C.c_uint32 * 2)()
    if c["type"] == 0x1B:
        ret = lib.tpw_dm_post(0x1B, c["things"][c["dying"]][0], 1, out)
        ident = out[1]
    else:
        ret = lib.tpw_dm_post(c["type"], 0, 0, out)
        ident = 0xA5A5
    chains = {}
    for slot in range(0x1D):
        data = (C.c_int32 * 9)()
        lib.tpw_dm_chain(slot, data)
        chains[slot] = dict(first=data[0], next=[(i, data[i]) for i in range(1, 9)], begin=data[0])
    for slot, d in chains.items():
        count = len(c["chains"].get(slot, ([], []))[0])
        d["next"] = d["next"][:count]
    table = {row: lib.tpw_dm_row_label(row) for row in sorted(touched)}
    return host_finish(lib, before, ret=ret, flags=out[0], ident=ident, chains=chains, table=table)


# ----------------------------------------------------------------- CWorld::AddToPendingDeleteList

def pending_cases(r):
    out = []
    for receiver in (0, 1, 2):
        for ident in IDS:
            out.append(dict(receiver=receiver, id=ident, low=r.getrandbits(16), count=r.choice([0, 1, 5]), padd=0, repeat=1))
    for ident in (0, 0xFFFF, 0x8000):
        out.append(dict(receiver=1, id=ident, low=0xFFFF, count=3, padd=7, repeat=1))
        out.append(dict(receiver=0, id=ident, low=0, count=0, padd=1, repeat=3))
    out.append(dict(receiver=1, id=1, low=0, count=0x7FFFFFFF, padd=1, repeat=2))
    return out


def pending_random(r):
    return dict(receiver=r.randrange(3), id=r.choice(IDS + [r.getrandbits(16)] * 3), low=r.getrandbits(16),
                count=r.choice([0, 1, r.getrandbits(32)]), padd=r.choice([0, 0, 1, 0xFFFFFFFF, r.getrandbits(32)]), repeat=r.randint(1, 3))


def ppc_pending(e, c):
    L = new_log(e, {"padd": [c["padd"]]})
    L.labels[PENDING_LIST] = 5
    tail = e.alloc(0x80, POISON * 0x80)
    world = {0: 0, 1: tail - TAIL0, 2: 0x12345670}[c["receiver"]]
    e.write(PENDING_LIST, struct.pack(">IIII", 0x11112222, 0, 0, c["count"] & MASK))

    def add_head(emu):
        L.rec("AddHead", [L.lab(emu.r(3)), emu.r(4) >> 16])
        w32(e, emu.r(3) + 0xC, e.u32(emu.r(3) + 0xC) + L.ret("padd", 0))

    s = stubs_with({ADD_HEAD: add_head})

    def call():
        for _ in range(c["repeat"]):
            T.call_ppc(e, PENDING, [world, (c["id"] << 16) | c["low"]])

    run_stubbed(e, s, call)
    return finish_ppc(e, count=T.s32(e.u32(PENDING_LIST + 0xC)), links=int(e.u32(PENDING_LIST + 4) == 0 and e.u32(PENDING_LIST + 8) == 0),
                      vptr=e.u32(PENDING_LIST), tail=e.read(tail, 0x80) == POISON * 0x80)


def host_pending(lib, c):
    before = host_log_start(lib, {"padd": [c["padd"]]})
    lib.tpw_dm_pending_set(s32(c["count"]))
    for _ in range(c["repeat"]):
        lib.tpw_dm_pending(c["receiver"], c["id"])
    return host_finish(lib, before, count=lib.tpw_dm_pending_count(), links=lib.tpw_dm_pending_links(), vptr=0x11112222, tail=True)


# ----------------------------------------------------------------- CWorld::GetRand

INV = pow(0x19660D, -1, 1 << 32)


def pre_image(target):
    return ((target - 0x3C6EF35F) * INV) & MASK


def rand_cases(r):
    out = []
    for state in (0, 1, MASK, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFE, 0x3C6EF35F, 0x19660D):
        out.append(dict(seed=r.getrandbits(32), state=state, calls=1))
    for target in (0, 1, 2, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFF, 0xFFFFFFFE, 0x40000000):
        out.append(dict(seed=r.getrandbits(32), state=pre_image(target), calls=1))
        out.append(dict(seed=r.getrandbits(32), state=pre_image(target), calls=3))
    for state in (0, 1, 0x80000000, 12345):
        out.append(dict(seed=r.getrandbits(32), state=state, calls=6))
    return out


def rand_random(r):
    return dict(seed=r.getrandbits(32), state=r.getrandbits(32), calls=r.randint(1, 4))


def tail_bytes(c, little, state=None):
    rr = random.Random(c["seed"])
    words = [rr.getrandbits(32) for _ in range(32)]
    words[START_WORD] = c["state"] if state is None else state
    return struct.pack(("<" if little else ">") + "32I", *words)


def decode(data, fields, little):
    out = []
    mask = bytearray(data)
    for offset, width in fields:
        out.append(int.from_bytes(data[offset:offset + width], "little" if little else "big"))
        mask[offset:offset + width] = bytes(width)
    return out, bytes(mask)


def ppc_rand(e, c):
    new_log(e)
    tail = e.alloc(0x80, tail_bytes(c, False))
    answers = [T.call_ppc(e, RAND, [tail - TAIL0])[0] for _ in range(c["calls"])]
    return finish_ppc(e, answers=answers, tail=decode(e.read(tail, 0x80), TAIL_WORDS, False))


def host_rand(lib, c):
    before = host_log_start(lib)
    C.memmove(lib.tpw_dm_tail(0), tail_bytes(c, True), 0x80)
    answers = [lib.tpw_dm_rand(0) for _ in range(c["calls"])]
    return host_finish(lib, before, answers=answers, tail=decode(C.string_at(lib.tpw_dm_tail(0), 0x80), TAIL_WORDS, True))


# ----------------------------------------------------------------- CParkTime::GetSeason

MONTHS = [-0x80000000, -13, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8, 9, 11, 12, 13, 0x7FFFFFFF]


def mk_season(r, **kw):
    q = {"plus": [r.choice(TICKS) if r.random() < 0.5 else r.getrandbits(64)],
         "gtout": [r.choice(MONTHS + [r.randint(1, 12)] * 8)], "gterr": [r.choice([0, 1, M64, 0x7FFFFFFF, 0x80000000, r.getrandbits(32)])],
         "wsel": [r.randrange(2)], "wmut": [1 if r.random() < 0.2 else 0], "wturn": [r.getrandbits(32)], "wrate": [r.getrandbits(32)],
         "wstart": [r.getrandbits(64)]}
    c = dict(seed=r.getrandbits(32), rate=r.choice(U32S) if r.random() < 0.5 else r.getrandbits(32),
             turns=[r.choice(U32S) if r.random() < 0.5 else r.getrandbits(32) for _ in range(2)],
             start=r.choice(TICKS) if r.random() < 0.5 else r.getrandbits(64), q=q)
    c.update(kw)
    for key, value in c.pop("override", {}).items():
        c["q"][key] = value
    return c


def season_edges():
    r = random.Random(7123)
    out = []
    # every rate with every turn: products crossing bit 63, approaching it, and every remainder of the division by 4
    for rate in U32S:
        for turn in U32S:
            out.append(mk_season(r, rate=rate, turns=[turn, r.getrandbits(32)], override={"wsel": [0], "wmut": [0]}))
    for rate in (0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 15000, 3, 5, 1):
        for turn in (0xFFFFFFFE, 0xFFFFFFFD, 0x80000001, 0x7FFFFFFF, 0x55555555, 0xAAAAAAAA, 0x40000000):
            out.append(mk_season(r, rate=rate, turns=[r.getrandbits(32), turn], override={"wsel": [1], "wmut": [0]}))
    # every start and every answer of the addition
    for start in TICKS:
        for plus in TICKS:
            out.append(mk_season(r, start=start, override={"plus": [plus]}))
    # every month, and the error of GetTime (negative, zero, positive) ignored
    for month in MONTHS:
        out.append(mk_season(r, override={"gtout": [month]}))
    for err in (0, 1, M64, 0x80000000):
        for month in (1, 2, 3, 12):
            out.append(mk_season(r, override={"gterr": [err], "gtout": [month]}))
    # the world accessor changes the answered world's turn, the clock's rate and the clock's start before they are read
    for sel in (0, 1):
        for _ in range(4):
            out.append(mk_season(r, override={"wsel": [sel], "wmut": [1]}))
    return out


def clock_bytes(c, little):
    rr = random.Random(c["seed"] + 1)
    fill = bytearray(rr.randbytes(CLOCK_SIZE))
    order = "little" if little else "big"
    fill[CLOCK_AT:CLOCK_AT + 8] = (c["start"] & M64).to_bytes(8, order)
    fill[CLOCK_AT + 0x1C:CLOCK_AT + 0x20] = (c["rate"] & MASK).to_bytes(4, order)
    return bytes(fill)


def season_tail(c, k, little):
    rr = random.Random(c["seed"] * 3 + k)
    words = [rr.getrandbits(32) for _ in range(32)]
    words[TURN_WORD] = c["turns"][k]
    return struct.pack(("<" if little else ">") + "32I", *words)


def queues(q):
    return {k: [v & M64 for v in values] for k, values in q.items()}


def ppc_season(e, c):
    L = new_log(e, queues(c["q"]))
    block = e.alloc(CLOCK_SIZE, clock_bytes(c, False))
    clock = block + CLOCK_AT
    tails = [e.alloc(0x80, season_tail(c, k, False)) for k in range(2)]
    L.labels.update({tails[0] - TAIL0: 1, tails[1] - TAIL0: 2, clock: 3})

    def get_world(emu):
        L.rec("GetWorld", [L.lab(emu.r(3))])
        sel = L.ret("wsel", 0)
        if L.ret("wmut", 0):
            w32(e, tails[sel] + 4 * TURN_WORD, L.ret("wturn"))
            w32(e, clock + 0x1C, L.ret("wrate"))
            e.write(clock, (L.ret("wstart") & M64).to_bytes(8, "big"))
        emu.set_r(3, tails[sel] - TAIL0)

    def plus(emu):
        L.rec("TimeStamp+", [s64(be64(e, emu.r(4))), s64(be64(e, emu.r(5)))])
        e.write(emu.r(3), (L.ret("plus", 0) & M64).to_bytes(8, "big"))

    def get_time(emu):
        stamp = emu.r(4)
        outs = [emu.r(i) for i in range(5, 11)] + [emu.u32(emu.r(1) + 0x38), emu.u32(emu.r(1) + 0x3C)]
        L.rec("GetTime", [s64(be64(e, stamp))] + [L.lab(o) for o in outs])
        for o in outs:
            if o:
                w32(e, o, L.ret("gtout", 0))
        w32(e, emu.r(3), L.ret("gterr", 0))

    s = stubs_with({GET_WORLD: get_world}, {"__pl__11TbTimeStampCFRC10TbTimeDiff": plus, "GetTime__11TbTimeStampCFPiPiPiPiPiPiPiPi": get_time})
    answer = run_stubbed(e, s, lambda: T.call_ppc(e, SEASON, [clock])[0])
    fields = decode(e.read(block, CLOCK_SIZE), CLOCK_FIELDS, False)
    return finish_ppc(e, answer=s32(answer), clock=fields, tails=[decode(e.read(a, 0x80), TAIL_WORDS, False) for a in tails])


def host_season(lib, c):
    before = host_log_start(lib, queues(c["q"]))
    C.memmove(lib.tpw_dm_clock(), clock_bytes(c, True), CLOCK_SIZE)
    for k in range(2):
        C.memmove(lib.tpw_dm_tail(k), season_tail(c, k, True), 0x80)
    lib.tpw_dm_label_clock()
    answer = lib.tpw_dm_season()
    return host_finish(lib, before, answer=answer, clock=decode(C.string_at(lib.tpw_dm_clock(), CLOCK_SIZE), CLOCK_FIELDS, True),
                       tails=[decode(C.string_at(lib.tpw_dm_tail(k), 0x80), TAIL_WORDS, True) for k in range(2)])


# ----------------------------------------------------------------- the tests

TESTS = [
    FnTest("CMsgDeath::CMsgDeath", ppc_ctor, host_ctor, edge=lambda: ctor_cases(random.Random(1)), rnd=ctor_random),
    FnTest("CMsgDeath::GetType", ppc_gettype, host_gettype, edge=lambda: gettype_cases(random.Random(2)), rnd=gettype_random),
    FnTest("CMsgDeath::~CMsgDeath", ppc_dtor, host_dtor, edge=lambda: dtor_cases(random.Random(3)), rnd=dtor_random),
    FnTest("CMessageCentre::PostMessage", ppc_post, host_post, edge=post_edges, rnd=post_random),
    FnTest("CWorld::AddToPendingDeleteList", ppc_pending, host_pending, edge=lambda: pending_cases(random.Random(5)), rnd=pending_random),
    FnTest("CParkTime::GetSeason", ppc_season, host_season, edge=season_edges, rnd=lambda r: mk_season(r)),
    FnTest("CWorld::GetRand", ppc_rand, host_rand, edge=lambda: rand_cases(random.Random(7)), rnd=rand_random),
]
