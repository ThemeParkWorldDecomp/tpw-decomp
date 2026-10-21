"""w107: the message centre's post and the subscription tree, original code on one side and src/game/advisor/Advisor.cpp, src/game/park/BankAccount.cpp, src/game/save/BlockSave.cpp, src/game/ui/MessageCentre.cpp,
src/game/Thing.cpp, src/game/MslSupport.cpp on the other (tests/unit_MessageCentre.cpp).

Functions (main app): CMsgEvent constructor 0x10151df8, destructor 0x100f39cc and GetType 0x100f4470; CSubscriptionHeader constructor
0x1000a3f8 and destructor 0x101522e0; CMessageCentre::PostMessage 0x1015256c; CThing::ReceiveMessage 0x1012e470 and GetThingId 0x1012e1f8;
the MSL tree std::__tree<CSubscription>: constructor 0x1000ff04, destroy 0x1000fae0, find 0x10152c58, clear 0x10152e64, erase 0x10152f3c,
the allocator's deallocate 0x1000fd84 and destroy 0x1000feb0; and the shared std::__red_black_tree: increment 0x10152db0, balance_erase
0x10153290, rotate_left 0x10153dec, rotate_right 0x10153efc.

Trees are built by hand in both memories from one description (five numbers per node: id, red, left, right, parent; the root's parent is
the end node) and compared by a walk from the root: count, root, begin and every reachable node's links and colour. The host keeps no
begin word (it derives the leftmost node), so begin is compared wherever the original keeps it up to date (everything except the bare
balance_erase and rotations, which are compared node by node). Real on both sides: the whole tree code. Fakes: GAME_ASSERTMSG, the
message's GetType (a vtable slot on the PowerPC side), the eighteen ReceiveMessage targets, and operator delete (labelled blocks only).
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_MessageCentre"
PROGRAM = "SimTheme Park"
MASK = 0xFFFFFFFF
END = 255
TABLE = 0x1034CBFC          # gThingArray, five words per entry
ASSERT = 0x1000DDE8         # GAME_ASSERTMSG
A = dict(msg_ctor=0x10151DF8, msg_dtor=0x100F39CC, msg_type=0x100F4470, hdr_ctor=0x1000A3F8, hdr_dtor=0x101522E0,
         post=0x1015256C, receive=0x1012E470, thing_id=0x1012E1F8, tree_ctor=0x1000FF04, destroy=0x1000FAE0,
         dealloc=0x1000FD84, adestroy=0x1000FEB0, find=0x10152C58, increment=0x10152DB0, clear=0x10152E64,
         erase=0x10152F3C, balance_erase=0x10153290, rotl=0x10153DEC, rotr=0x10153EFC)
RECEIVERS = {1: ("CPerson", 0x10116E24), 3: ("CObject", 0x10107BA0), 4: ("CMechanic", 0x10104508),
             5: ("CHandyman", 0x100FF3C4), 6: ("CEntertainer", 0x100FB448), 7: ("CGuard", 0x100FDEC0),
             8: ("CResearcher", 0x101212E4), 9: ("CStaffHQ", 0x1012A9E8), 10: ("CMechanicHQ", 0x10105988),
             11: ("CAdvisor", 0x1000C874), 12: ("CTagSystem", 0x1012C90C), 13: ("CParkAnalyser", 0x100E41BC),
             14: ("CResearchLab", 0x10123144), 15: ("CWeather", 0x1013855C), 16: ("CBankAccount", 0x100F3564),
             17: ("CUIMsgReceiver", 0x10130998), 18: ("COnlinePerson", 0x10212FB0), 19: ("CChallengeHQ", 0x100F75E4)}
VTABLE_WORD = 0x102A05A0    # CMsgEvent's vtable, written by its constructor and destructor
POISON = b"\xa5"
EDGE_IDS = [0, 1, 2, 3, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0x8001, 0xFFFE, 0xFFFF]


# ----------------------------------------------------------------- trees (plain Python)

def rb_insert(ids):
    """A red-black tree by CLRS insertion of ids in order (valid colours): {"nodes": [[id, red, left, right, parent]], "root"}."""
    key, left, right, parent, red = [0], [0], [0], [0], [0]
    state = {"root": 0}

    def rotate_left(x):
        y = right[x]
        right[x] = left[y]
        if left[y]:
            parent[left[y]] = x
        parent[y] = parent[x]
        if parent[x] == 0:
            state["root"] = y
        elif x == left[parent[x]]:
            left[parent[x]] = y
        else:
            right[parent[x]] = y
        left[y] = x
        parent[x] = y

    def rotate_right(x):
        y = left[x]
        left[x] = right[y]
        if right[y]:
            parent[right[y]] = x
        parent[y] = parent[x]
        if parent[x] == 0:
            state["root"] = y
        elif x == right[parent[x]]:
            right[parent[x]] = y
        else:
            left[parent[x]] = y
        right[y] = x
        parent[x] = y

    for k in ids:
        n = len(key)
        key.append(k)
        left.append(0)
        right.append(0)
        parent.append(0)
        red.append(1)
        y, x = 0, state["root"]
        while x:
            y = x
            x = left[x] if k < key[x] else right[x]
        parent[n] = y
        if y == 0:
            state["root"] = n
        elif k < key[y]:
            left[y] = n
        else:
            right[y] = n
        z = n
        while parent[z] and red[parent[z]]:
            zp = parent[z]
            gp = parent[zp]
            if zp == left[gp]:
                uncle = right[gp]
                if uncle and red[uncle]:
                    red[zp] = red[uncle] = 0
                    red[gp] = 1
                    z = gp
                else:
                    if z == right[zp]:
                        z = zp
                        rotate_left(z)
                    red[parent[z]] = 0
                    red[parent[parent[z]]] = 1
                    rotate_right(parent[parent[z]])
            else:
                uncle = left[gp]
                if uncle and red[uncle]:
                    red[zp] = red[uncle] = 0
                    red[gp] = 1
                    z = gp
                else:
                    if z == left[zp]:
                        z = zp
                        rotate_right(z)
                    red[parent[z]] = 0
                    red[parent[parent[z]]] = 1
                    rotate_left(parent[parent[z]])
        red[state["root"]] = 0
    return {"nodes": [[key[i], red[i], left[i], right[i], parent[i]] for i in range(1, len(key))], "root": state["root"]}


def bst(ids, reds):
    """An unbalanced search tree with arbitrary colours (for the functions that do not rebalance)."""
    t = rb_insert([])
    nodes = []
    root = 0
    for i, k in enumerate(ids, 1):
        nodes.append([k, reds[i - 1], 0, 0, 0])
        if root == 0:
            root = i
            continue
        x = root
        while True:
            side = 2 if k < nodes[x - 1][0] else 3
            if nodes[x - 1][side] == 0:
                nodes[x - 1][side] = i
                nodes[i - 1][4] = x
                break
            x = nodes[x - 1][side]
    t["nodes"], t["root"] = nodes, root
    return t


def perfect(ids, red_depths=()):
    """The perfectly balanced search tree of a sorted id list of 2**k - 1 ids, nodes of the listed depths red (valid when no two
    adjacent depths are red: every path then has the same number of black nodes)."""
    nodes = []

    def build(lo, hi, parent, depth):
        if lo > hi:
            return 0
        mid = (lo + hi) // 2
        nodes.append([ids[mid], 1 if depth in red_depths else 0, 0, 0, parent])
        me = len(nodes)
        nodes[me - 1][2] = build(lo, mid - 1, me, depth + 1)
        nodes[me - 1][3] = build(mid + 1, hi, me, depth + 1)
        return me

    root = build(0, len(ids) - 1, 0, 0)
    return {"nodes": nodes, "root": root}


def check_rb(t):
    """Raises unless t is a valid red-black tree with a search order."""
    nodes, root = t["nodes"], t["root"]
    if not nodes:
        return
    assert not nodes[root - 1][1]

    def walk(i, lo, hi):
        if i == 0:
            return 1
        k, red, l, r, _ = nodes[i - 1]
        assert lo < k < hi
        for c in (l, r):
            if c:
                assert nodes[c - 1][4] == i
                assert not (red and nodes[c - 1][1])
        hl, hr = walk(l, lo, k), walk(r, k, hi)
        assert hl == hr
        return hl + (0 if red else 1)

    walk(root, -1, 1 << 20)


def ids_of(r, n, limit=0xFFFF):
    """n distinct ids: a mix of edge values and random numbers."""
    pool = set()
    while len(pool) < n:
        if r.random() < 0.3:
            v = r.choice(EDGE_IDS)
        else:
            v = r.randint(0, limit)
        if v <= limit:
            pool.add(v)
    out = sorted(pool)
    r.shuffle(out)
    return out


def rb_random(r, n, limit=0xFFFF):
    t = rb_insert(ids_of(r, n, limit))
    check_rb(t)
    return t


def edge_trees():
    r = __import__("random").Random(107)
    trees = [rb_insert([])]
    for ids in ([0x8000], [5, 3], [5, 7], [5, 3, 7], [0, 0xFFFF], [0xFFFF, 0], [0x7FFF, 0x8000, 0x8001],
                [4, 2, 6, 1, 3, 5, 7], [8, 4, 12, 2, 6, 10, 14, 1, 3, 5, 7, 9, 11, 13, 15],
                list(range(1, 11)), list(range(10, 0, -1)), [0x10 * i for i in range(1, 12)]):
        trees.append(rb_insert(ids))
    # perfectly balanced trees in every valid colouring (the all-black ones and the ones with red levels cannot come from inserts)
    for ids in ([0x10, 0x8000, 0xFFFF], [1, 2, 3, 4, 5, 6, 7], [0x100 * i for i in range(1, 16)]):
        deepest = len(ids).bit_length() - 1
        for red in sorted({(), (1,), (2,), (3,), (1, 3), (deepest,)}):
            if max(red, default=0) <= deepest and all(abs(a - b) != 1 for a in red for b in red):
                trees.append(perfect(ids, red))
    for n in (4, 6, 9, 12, 16, 21, 28, 35):
        trees.append(rb_random(r, n))
    for t in trees:
        check_rb(t)
    return trees


TREES = edge_trees()


def flat(t):
    return [v for node in t["nodes"] for v in node]


def label_base(slot):
    return 200 + min(slot, 29) * 40


# ----------------------------------------------------------------- the PowerPC side

class Ctx:
    """A centre block (headers 0..28 at +0x10 each) and the standalone headers 29 and 100, with nodes built from tree descriptions."""

    def __init__(self, e):
        self.e = e
        self.centre = e.alloc(0x1D0, POISON * 0x1D0)
        e.cl.labels[self.centre] = 1
        self.hdrs, self.addr, self.rev, self.n = {}, {}, {}, {}
        for s in range(29):
            self.hdrs[s] = self.centre + 0x10 * s
            self.empty(s)

    def header(self, slot):
        if slot not in self.hdrs:
            self.hdrs[slot] = self.e.alloc(0x10, POISON * 0x10)
            self.addr[slot], self.rev[slot], self.n[slot] = {}, {}, 0
            if slot == 100:
                self.e.cl.labels[self.hdrs[slot]] = 3
        return self.hdrs[slot]

    def empty(self, slot):
        h = self.header(slot)
        self.e.write(h, POISON * 4 + struct.pack(">III", 0, 0, h + 8))
        self.addr[slot], self.rev[slot], self.n[slot] = {}, {}, 0

    def build(self, slot, tree, count=None):
        e = self.e
        h = self.header(slot)
        nodes = tree["nodes"]
        end = h + 8      # the tree is at +4; its end node is the root word
        addrs = {i: e.alloc(0x10, POISON * 0x10) for i in range(1, len(nodes) + 1)}
        self.addr[slot], self.rev[slot], self.n[slot] = addrs, {a: i for i, a in addrs.items()}, len(nodes)
        for i, (ident, red, left, right, parent) in enumerate(nodes, 1):
            e.cl.labels[addrs[i]] = label_base(slot) + i
            e.write(addrs[i], struct.pack(">IIIHH", addrs[left] if left else 0, addrs[right] if right else 0,
                                          (addrs[parent] if parent else end) | red, ident, 0xA5A5))
        root = addrs[tree["root"]] if tree["root"] else 0
        begin = end
        if root:
            begin = root
            while e.u32(begin):
                begin = e.u32(begin)
        e.write(h + 4, struct.pack(">III", len(nodes) if count is None else count, root, begin))
        return h

    def idx(self, slot, a):
        if a == 0:
            return 0
        if a == self.hdrs[slot] + 8:
            return END
        return self.rev[slot].get(a, -1)

    def dump(self, slot, mode):
        """mode 0/1: preorder walk (1 adds begin), 2: header words, 3: every built node (see tests/unit_MessageCentre.cpp)."""
        e, h = self.e, self.hdrs[slot]
        count, root, begin = e.u32(h + 4), e.u32(h + 8), e.u32(h + 12)
        out = [count, self.idx(slot, root)]
        if mode == 2:
            return out + [0, 0]
        out.append(self.idx(slot, begin) if mode == 1 else 0)

        def record(i, a):
            left, right, parent, ident = struct.unpack(">IIIH", e.read(a, 14))
            return [i, self.idx(slot, left), self.idx(slot, right), self.idx(slot, parent & ~1), parent & 1, ident]

        if mode == 3:
            out.append(self.n[slot])
            for i in range(1, self.n[slot] + 1):
                out += record(i, self.addr[slot][i])
            return out
        stack, rows = ([root] if root else []), []
        while stack and len(rows) < 40:
            a = stack.pop()
            row = record(self.idx(slot, a), a)
            rows.append(row)
            left, right = e.u32(a), e.u32(a + 4)
            if right:
                stack.append(right)
            if left:
                stack.append(left)
        out.append(len(rows))
        for row in rows:
            out += row
        return out


def op_delete(e):
    """operator delete: counted, recorded when the block is labelled (as tests/calllog.cpp does)."""
    p = e.r(3)
    if p:
        e.cl.deletes += 1
        if e.cl.lab(p) > 0:
            e.cl.rec("delete", (e.cl.lab(p),))
            e.cl.forget(p)


def make_stubs():
    s = CL.Stubs()
    s.real.update(A.values())
    s.imp("__dl__FPv", op_delete)
    s.internal(ASSERT, CL.recorder("GAME_ASSERTMSG", "bs"))
    for name, addr in RECEIVERS.values():
        s.internal(addr, CL.recorder(name + "::ReceiveMessage", "LL"))
    return s


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.mc_stubs = make_stubs()

    def get_type(emu):
        emu.cl.rec("GetType", (emu.cl.lab(emu.r(3)),))
        emu.set_r(3, T.u32(emu.cl.msg_type))

    # The message's vtable: slot 2 (+8) is GetType, called through the glue with a TVector like every virtual.
    tvec = T.fake_function(e, get_type)
    e.mc_vtable = T.persist_alloc(e, 0x20)
    e.write(e.mc_vtable + 8, struct.pack(">I", tvec))


def ppc_case(e):
    """Fresh log and counters for a case; returns the stub state to restore."""
    e.cl = CL.PpcLog({}, first_new=900)
    e.cl.deletes = 0
    e.cl.msg_type = 0
    e.news.clear()
    return e.mc_stubs.activate(e)


def ppc_run(fn):
    def run(e, c):
        saved = ppc_case(e)
        try:
            return fn(e, c)
        finally:
            CL.Stubs.deactivate(e, saved)
    return run


def done(e, **fields):
    out = dict(fields)
    out["log"] = e.cl.log
    out["deletes"] = e.cl.deletes
    out["news"] = len(e.news)
    return out


# ----------------------------------------------------------------- the host side

def api(lib):
    if getattr(lib, "mc_ready", False):
        return lib
    P, i32 = C.POINTER(C.c_int32), C.c_int32
    lib.tpw_mc_begin.argtypes = []
    lib.tpw_mc_build.argtypes = [i32, i32, i32, P, i32, i32]
    lib.tpw_mc_dump.argtypes = [i32, P, i32]
    lib.tpw_mc_dump.restype = i32
    lib.tpw_mc_tree_op.argtypes = [i32, i32, i32, i32]
    lib.tpw_mc_tree_op.restype = i32
    lib.tpw_mc_header_ctor.argtypes = []
    lib.tpw_mc_tree_ctor.argtypes = []
    lib.tpw_mc_header_dtor.argtypes = [i32, i32]
    lib.tpw_mc_event_ctor.argtypes = [i32, C.POINTER(C.c_uint32)]
    lib.tpw_mc_event_dtor.argtypes = [i32, i32]
    lib.tpw_mc_allocator_op.argtypes = [i32, C.c_uint32]
    lib.tpw_mc_set_thing.argtypes = [i32] * 5
    lib.tpw_mc_get_thing_id.argtypes = [i32]
    lib.tpw_mc_get_thing_id.restype = C.c_uint32
    lib.tpw_mc_receive.argtypes = [i32]
    lib.tpw_mc_post.argtypes = [i32, i32]
    lib.tpw_mc_post.restype = i32
    lib.mc_ready = True
    return lib


def heap(lib):
    return list((C.c_int32 * 4).in_dll(lib, "tpw_cl_heap"))


def hbegin(lib):
    api(lib)
    CL.host_begin(lib, {}, first_new=900)
    lib.tpw_mc_begin()


def hbuild(lib, slot, tree, count=None):
    n = len(tree["nodes"])
    values = flat(tree) or [0]
    lib.tpw_mc_build(slot, n, tree["root"], (C.c_int32 * len(values))(*values), label_base(slot), n if count is None else count)


def hdump(lib, slot, mode):
    out = (C.c_int32 * 400)()
    n = lib.tpw_mc_dump(slot, out, mode)
    result = list(out[:n])
    result[0] &= MASK       # the count is a 32-bit unsigned word
    return result


def hfinish(lib, before, **fields):
    out = dict(fields)
    now = heap(lib)
    out["log"] = CL.host_end(lib)
    out["deletes"] = now[2] - before[2]
    out["news"] = now[0] - before[0]
    return out


def host_run(fn):
    """Starts a host case (labels, centre of empty headers); fn(lib, c, finish) builds, runs and calls finish(**fields)."""
    def run(lib, c):
        hbegin(lib)
        state = {}

        def start():
            state["before"] = heap(lib)

        return fn(lib, c, start, lambda **fields: hfinish(lib, state["before"], **fields))
    return run


# ----------------------------------------------------------------- CMsgEvent

def s16(v):
    return T.s16(v)


def event_cases(r):
    return [dict(type=t & MASK) for t in (0, 1, MASK, 0x7FFFFFFF, 0x80000000, 0x13, 0x1B, 10, 0x12345678)] + \
           [dict(type=r.getrandbits(32)) for _ in range(5)]


def ppc_event_ctor(e, c):
    obj = e.alloc(0x40, POISON * 0x40)
    T.call_ppc(e, A["msg_ctor"], [obj, c["type"]])
    return dict(flags=e.u32(obj + 4), event=e.u32(obj + 8), intact=int(e.read(obj + 0xC, 0x34) == POISON * 0x34))


def host_event_ctor(lib, c):
    api(lib)
    out = (C.c_uint32 * 4)()
    lib.tpw_mc_event_ctor(C.c_int32(T.s32(c["type"])), out)
    return dict(flags=out[0], event=out[1], intact=out[2])


def ppc_event_type(e, c):
    obj = e.alloc(0x10, POISON * 0x10)
    T.call_ppc(e, A["msg_ctor"], [obj, c["type"]])
    return dict(type=T.call_ppc(e, A["msg_type"], [obj])[0])


def host_event_type(lib, c):
    api(lib)
    out = (C.c_uint32 * 4)()
    lib.tpw_mc_event_ctor(C.c_int32(T.s32(c["type"])), out)
    return dict(type=out[3])


def dtor_cases(r):
    flags = [0, 1, MASK, 0x7FFF, 0x8000, 0xFFFF, 0x12340001, 0x1234FFFF, 0x12348000, 0x00010000, 0x7FFF0000, 5, 0xFFFF8000]
    out = [dict(flag=f, present=1) for f in flags]
    out += [dict(flag=f, present=0) for f in (1, 0x7FFF, 0x12340001)]
    out += [dict(flag=r.choice(flags + [r.getrandbits(32)]), present=1) for _ in range(6)]
    return out


def ppc_event_dtor(e, c):
    obj = e.alloc(0x10, POISON * 0x10) if c["present"] else 0
    e.cl.labels[obj] = 3
    if obj:
        e.write(obj, struct.pack(">III", 0x11111111, 0x22222222, 0x33333333))
    ret = T.call_ppc(e, A["msg_dtor"], [obj, c["flag"]])[0]
    tail = e.read(obj + 4, 8) if obj and s16(c["flag"]) <= 0 else b""
    return done(e, same=int(ret == obj), vptr=int(obj == 0 or e.u32(obj) == VTABLE_WORD), tail=tail)


def host_event_dtor(lib, c, start, finish):
    start()
    lib.tpw_mc_event_dtor(s16(c["flag"]), c["present"])
    # the object is gone (positive flag) or destroyed in place: only the log, the vtable reset and the untouched words are comparable
    tail = struct.pack(">II", 0x22222222, 0x33333333) if c["present"] and s16(c["flag"]) <= 0 else b""
    return finish(same=1, vptr=1, tail=tail)


# ----------------------------------------------------------------- the headers

def ppc_header_ctor(e, c):
    ctx = Ctx(e)
    h = ctx.header(29)
    e.write(h, bytes([c["poison"]]) * 0x10)
    ret = T.call_ppc(e, A["hdr_ctor"], [h])[0]
    return done(e, same=int(ret == h), word0=e.u32(h), state=ctx.dump(29, 1))


def host_header_ctor(lib, c, start, finish):
    start()
    lib.tpw_mc_header_ctor()
    return finish(same=1, word0=c["poison"] * 0x01010101, state=hdump(lib, 29, 1))


def ppc_tree_ctor(e, c):
    ctx = Ctx(e)
    h = ctx.header(29)
    e.write(h, bytes([c["poison"]]) * 0x10)
    junk = e.alloc(8, bytes([c["byte"]]) * 8)
    T.call_ppc(e, A["tree_ctor"], [h + 4, junk, junk + 1])
    return done(e, word0=e.u32(h), state=ctx.dump(29, 1))


def host_tree_ctor(lib, c, start, finish):
    start()
    lib.tpw_mc_tree_ctor()
    return finish(word0=c["poison"] * 0x01010101, state=hdump(lib, 29, 1))


def ctor_cases(r):
    return [dict(poison=p, byte=b) for p, b in ((0xA5, 0), (0, 0xFF), (0xFF, 1), (0x5A, 0xA5))] + \
           [dict(poison=r.getrandbits(8), byte=r.getrandbits(8)) for _ in range(4)]


def header_dtor_cases(r):
    out = []
    for t in TREES:
        out.append(dict(tree=t, flag=0))
    for t in TREES[:9] + TREES[-3:]:
        for flag in (1, MASK, 0x12348000, 0x7FFF, 0x1234FFFF, 0xFFFF):
            out.append(dict(tree=t, flag=flag))
    out.append(dict(tree=None, flag=1))
    out.append(dict(tree=None, flag=0x12340001))
    return out


def header_dtor_random(r):
    return dict(tree=rb_random(r, r.randint(0, 30)), flag=r.choice([0, 1, MASK, 0x7FFF, 0x8000, r.getrandbits(32)]))


def ppc_header_dtor(e, c):
    ctx = Ctx(e)
    heapheader = s16(c["flag"]) > 0
    if c["tree"] is None:
        h = 0
    else:
        slot = 100 if heapheader else 0
        h = ctx.build(slot, c["tree"])
    ret = T.call_ppc(e, A["hdr_dtor"], [h, c["flag"]])[0]
    state = ctx.dump(slot, 2) if h and not heapheader else []
    return done(e, same=int(ret == h), state=state)


def host_header_dtor(lib, c, start, finish):
    heapheader = s16(c["flag"]) > 0
    if c["tree"] is None:
        slot = -1
    else:
        slot = 100 if heapheader else 0
        hbuild(lib, slot, c["tree"])
    start()
    lib.tpw_mc_header_dtor(slot, s16(c["flag"]))
    state = hdump(lib, slot, 2) if slot == 0 else []
    return finish(same=1, state=state)


# ----------------------------------------------------------------- tree operations

def ppc_tree(e, tree, slot=0):
    ctx = Ctx(e)
    ctx.build(slot, tree)
    return ctx, ctx.hdrs[slot]


def node_cases(r, extra=None):
    """(tree, node index) for every node of the edge trees."""
    out = []
    for t in TREES:
        for i in range(1, len(t["nodes"]) + 1):
            out.append(dict(tree=t, node=i, **(extra or {})))
    return out


def random_node(r):
    t = rb_random(r, r.randint(1, 34))
    return dict(tree=t, node=r.randint(1, len(t["nodes"])))


def ppc_destroy(e, c):
    ctx, h = ppc_tree(e, c["tree"])
    T.call_ppc(e, A["destroy"], [h + 4, ctx.addr[0][c["node"]]])
    return done(e)


def host_destroy(lib, c, start, finish):
    hbuild(lib, 0, c["tree"])
    start()
    lib.tpw_mc_tree_op(4, 0, c["node"], 0)
    return finish()


def ppc_erase(e, c):
    ctx, h = ppc_tree(e, c["tree"])
    T.call_ppc(e, A["erase"], [h + 4, ctx.addr[0][c["node"]]])
    return done(e, state=ctx.dump(0, 1))


def host_erase(lib, c, start, finish):
    hbuild(lib, 0, c["tree"])
    start()
    lib.tpw_mc_tree_op(2, 0, c["node"], 0)
    return finish(state=hdump(lib, 0, 1))


def ppc_balance_erase(e, c):
    ctx, h = ppc_tree(e, c["tree"])
    T.call_ppc(e, A["balance_erase"], [ctx.addr[0][c["node"]], ctx.addr[0][c["tree"]["root"]]])
    return done(e, state=ctx.dump(0, 3))


def host_balance_erase(lib, c, start, finish):
    hbuild(lib, 0, c["tree"])
    start()
    lib.tpw_mc_tree_op(5, 0, c["node"], c["tree"]["root"])
    return finish(state=hdump(lib, 0, 3))


def rotate_cases(r, left):
    """Nodes that have the child the rotation lifts, with the root word holding the node itself, the root or another node."""
    out = []
    side = 3 if left else 2
    for t in TREES:
        for i, node in enumerate(t["nodes"], 1):
            if node[side]:
                others = [j for j in range(1, len(t["nodes"]) + 1) if j != i]
                for word in (i, t["root"]) + ((others[0],) if others else ()):
                    out.append(dict(tree=t, node=i, word=word))
    return out


def rotate_random(left):
    def make(r):
        while True:
            t = rb_random(r, r.randint(2, 30))
            side = 3 if left else 2
            ok = [i for i, n in enumerate(t["nodes"], 1) if n[side]]
            if ok:
                i = r.choice(ok)
                return dict(tree=t, node=i, word=r.choice([i, t["root"], r.randint(1, len(t["nodes"]))]))
    return make


def ppc_rotate(which):
    def run(e, c):
        ctx, h = ppc_tree(e, c["tree"])
        slot = e.alloc(8, struct.pack(">II", ctx.addr[0][c["word"]], 0xA5A5A5A5))
        T.call_ppc(e, A[which], [ctx.addr[0][c["node"]], slot])
        return done(e, word=ctx.idx(0, e.u32(slot)), tail=e.u32(slot + 4), state=ctx.dump(0, 3))
    return run


def host_rotate(op):
    def run(lib, c, start, finish):
        hbuild(lib, 0, c["tree"])
        start()
        word = lib.tpw_mc_tree_op(op, 0, c["node"], c["word"])
        return finish(word=word, tail=0xA5A5A5A5, state=hdump(lib, 0, 3))
    return run


def find_cases(r):
    out = []
    for t in TREES:
        ids = [n[0] for n in t["nodes"]]
        keys = set(ids) | {0, 1, 0xFFFF, 0x8000, 0x7FFF}
        for k in ids:
            keys.update(v for v in (k - 1, k + 1) if 0 <= v <= 0xFFFF)
        for k in sorted(keys):
            out.append(dict(tree=t, key=k))
    return out


def find_random(r):
    t = rb_random(r, r.randint(0, 30))
    ids = [n[0] for n in t["nodes"]]
    key = r.choice(ids) if ids and r.random() < 0.5 else r.choice([r.getrandbits(16), r.choice(EDGE_IDS)])
    if ids and r.random() < 0.2:
        key = min(0xFFFF, max(0, r.choice(ids) + r.choice((-1, 1))))
    return dict(tree=t, key=key)


def ppc_find(e, c):
    ctx, h = ppc_tree(e, c["tree"])
    out = e.alloc(8, POISON * 8)
    key = e.alloc(4, struct.pack(">HH", c["key"], 0xA5A5))
    ret = T.call_ppc(e, A["find"], [out, h + 4, key])[0]
    return done(e, found=ctx.idx(0, e.u32(out)), tail=e.u32(out + 4), same=int(ret == out), state=ctx.dump(0, 1))


def host_find(lib, c, start, finish):
    hbuild(lib, 0, c["tree"])
    start()
    found = lib.tpw_mc_tree_op(0, 0, c["key"], 0)
    return finish(found=found, tail=0xA5A5A5A5, same=1, state=hdump(lib, 0, 1))


def ppc_increment(e, c):
    ctx, h = ppc_tree(e, c["tree"])
    slot = e.alloc(8, struct.pack(">II", ctx.addr[0][c["node"]], 0xA5A5A5A5))
    T.call_ppc(e, A["increment"], [slot])
    return done(e, next=ctx.idx(0, e.u32(slot)), tail=e.u32(slot + 4), state=ctx.dump(0, 1))


def host_increment(lib, c, start, finish):
    hbuild(lib, 0, c["tree"])
    start()
    nxt = lib.tpw_mc_tree_op(1, 0, c["node"], 0)
    return finish(next=nxt, tail=0xA5A5A5A5, state=hdump(lib, 0, 1))


def bst_trees(r, count):
    """Unbalanced trees with random colours (increment and find read the colour bit only through the mask)."""
    out = []
    for _ in range(count):
        n = r.randint(1, 20)
        out.append(bst(ids_of(r, n), [r.randint(0, 1) for _ in range(n)]))
    return out


def increment_cases(r):
    out = node_cases(r)
    for t in bst_trees(__import__("random").Random(5), 6):
        out += [dict(tree=t, node=i) for i in range(1, len(t["nodes"]) + 1)]
    return out


def increment_random(r):
    if r.random() < 0.5:
        t = rb_random(r, r.randint(1, 34))
    else:
        n = r.randint(1, 25)
        t = bst(ids_of(r, n), [r.randint(0, 1) for _ in range(n)])
    return dict(tree=t, node=r.randint(1, len(t["nodes"])))


def find_cases_more(r):
    out = find_cases(r)
    for t in bst_trees(__import__("random").Random(6), 5):
        ids = [n[0] for n in t["nodes"]]
        out += [dict(tree=t, key=k) for k in ids]
        out += [dict(tree=t, key=min(0xFFFF, k + 1)) for k in ids]
    return out


def clear_cases(r):
    out = [dict(tree=t, count=None) for t in TREES]
    # a tree without a root keeps whatever count the header holds
    out += [dict(tree=TREES[0], count=7), dict(tree=TREES[0], count=0xFFFFFFFF)]
    out += [dict(tree=t, count=99) for t in TREES[1:4]]
    return out


def clear_random(r):
    return dict(tree=rb_random(r, r.randint(0, 34)), count=r.choice([None, None, r.getrandbits(32)]))


def ppc_clear(e, c):
    ctx = Ctx(e)
    h = ctx.build(0, c["tree"], c["count"])
    T.call_ppc(e, A["clear"], [h + 4])
    return done(e, state=ctx.dump(0, 1))


def host_clear(lib, c, start, finish):
    hbuild(lib, 0, c["tree"], None if c["count"] is None else T.s32(c["count"]))
    start()
    lib.tpw_mc_tree_op(3, 0, 0, 0)
    return finish(state=hdump(lib, 0, 1))


def allocator_cases(op):
    counts = (0, 1, 2, 0xFFFFFFFF, 0x12345678) if op == 0 else (0, 1, 0x7FFFFFFF)
    return lambda: [dict(op=op, count=n) for n in counts]


def allocator_random(op):
    return lambda r: dict(op=op, count=r.getrandbits(32))


def ppc_allocator(e, c):
    node = e.alloc(0x10, POISON * 0x10)
    e.cl.labels[node] = 4
    alloc = e.alloc(8, POISON * 8)
    if c["op"] == 0:
        T.call_ppc(e, A["dealloc"], [alloc, node, c["count"]])
    else:
        T.call_ppc(e, A["adestroy"], [alloc, node])
    return done(e, node=e.read(node, 0x10), alloc=e.read(alloc, 8))


def host_allocator(lib, c, start, finish):
    start()
    lib.tpw_mc_allocator_op(c["op"], c["count"])
    return finish(node=POISON * 0x10, alloc=POISON * 8)


# ----------------------------------------------------------------- the things

def thing_cases(r):
    return [dict(own=o, kind=k) for o, k in ((0, 0), (1, 1), (0xFFFF, 0xFF), (0x8000, 0x13), (0x27FF, 0x14), (0x1234, 5))] + \
           [dict(own=r.getrandbits(16), kind=r.getrandbits(8)) for _ in range(4)]


def ppc_thing_id(e, c):
    thing = e.alloc(0x20, struct.pack(">HB", c["own"], c["kind"]) + POISON * 0x1D)
    out = e.alloc(8, POISON * 8)
    T.call_ppc(e, A["thing_id"], [out, thing])
    value = struct.unpack(">H", e.read(out, 2))[0]
    return dict(value=value, tail=e.read(out + 2, 6), thing=e.read(thing, 0x20))


def host_thing_id(lib, c):
    hbegin(lib)
    lib.tpw_mc_set_thing(0, -1, c["kind"], c["own"], 10)
    return dict(value=lib.tpw_mc_get_thing_id(0), tail=POISON * 6, thing=struct.pack(">HB", c["own"], c["kind"]) + POISON * 0x1D)


def receive_cases(r):
    out = [dict(kind=k, own=0x100 + k) for k in range(0x16)]
    out += [dict(kind=k, own=0xFFFF) for k in (0x80, 0xFF, 0x7F, 0x14, 0x13, 0x12, 0x20)]
    return out


def receive_random(r):
    return dict(kind=r.choice([r.randrange(0x16), r.randrange(0x16), r.getrandbits(8)]), own=r.getrandbits(16))


def ppc_receive(e, c):
    thing = e.alloc(0x10, struct.pack(">HB", c["own"], c["kind"]) + POISON * 0xD)
    msg = e.alloc(0x10, POISON * 0x10)
    e.cl.labels[thing] = 10
    e.cl.labels[msg] = 2
    T.call_ppc(e, A["receive"], [thing, msg])
    return done(e, thing=e.read(thing, 0x10))


def host_receive(lib, c, start, finish):
    lib.tpw_mc_set_thing(0, -1, c["kind"], c["own"], 10)
    start()
    lib.tpw_mc_receive(0)
    return finish(thing=struct.pack(">HB", c["own"], c["kind"]) + POISON * 0xD)


# ----------------------------------------------------------------- PostMessage

def post_case(r, typ=None, dying=None):
    typ = r.choice([0x1B] * 5 + [0, 1, 5, 0xA, 0x1A, 0x1C, 0x13, 0x10, r.randrange(0x1D)]) if typ is None else typ
    zero_thing = r.random() < 0.15
    pool = ids_of(r, r.randint(1, 10), 0x27FF)
    if not zero_thing and 0 in pool:
        pool.remove(0)
    slots = {typ} | set(r.sample(range(0x1D), r.randint(0, 4)))
    headers, subscribed = {}, set()
    for slot in slots:
        k = r.choice([0, 1, 1, 2, 3, 4, 5, 8]) if slot != typ else r.choice([0, 1, 1, 2, 3, 4, 6, 10])
        chosen = r.sample(pool, min(k, len(pool)))
        subscribed.update(chosen)
        headers[slot] = rb_insert(chosen)
        check_rb(headers[slot])
    own = {}
    dying_id = 0
    if typ == 0x1B:
        dying_id = (dying if dying is not None else (r.choice(sorted(subscribed)) if subscribed and r.random() < 0.7
                                                       else r.randint(1, 0x27FF)))
        if dying_id == 0:
            dying_id = 1
        subscribed.add(dying_id)
        # the thing reports its own id, usually the table index, sometimes another subscribed id
        own[dying_id] = dying_id if r.random() < 0.7 or not subscribed else r.choice(sorted(subscribed))
    things = []
    for i in sorted(subscribed):
        things.append((i, r.randrange(0x16), own.get(i, i)))
    if zero_thing:
        things = [t for t in things if t[0] != 0]
        things.append((0, r.randrange(0x16), 0))
    return dict(type=typ, dying=dying_id, headers=headers, things=things)


def post_edges(r):
    out = []
    # one subscriber of each kind, one message each
    for kind in range(0x16):
        out.append(dict(type=3, dying=0, headers={3: rb_insert([7])}, things=[(7, kind, 7)]))
    # 0x1b with ids subscribed in 0, 1 and 3 headers
    t = rb_insert([5, 9, 12])
    for hs in ([], [0x1B], [0x1B, 4], [0x1B, 4, 0x1C], [0, 0x1B, 0x1C]):
        headers = {s: rb_insert([5, 9, 12]) for s in hs}
        headers.setdefault(0x1B, rb_insert([]))
        out.append(dict(type=0x1B, dying=9, headers=headers, things=[(5, 1, 5), (9, 3, 9), (12, 11, 12)]))
    out.append(dict(type=0x1B, dying=9, headers={0x1B: rb_insert([9])}, things=[(9, 4, 9)]))
    # the dying thing's own id differs from the table index: its OWN id is the one erased
    out.append(dict(type=0x1B, dying=9, headers={0x1B: rb_insert([5, 9, 12]), 2: rb_insert([5, 9])},
                    things=[(5, 1, 5), (9, 3, 5), (12, 11, 12)]))
    # table entry 0 allocated: the assert reports (before every delivery, with the same text)
    out.append(dict(type=2, dying=0, headers={2: rb_insert([1, 2])}, things=[(0, 3, 0), (1, 5, 1), (2, 6, 2)]))
    out.append(dict(type=0x1B, dying=4, headers={0x1B: rb_insert([4, 0, 8])}, things=[(0, 3, 0), (4, 7, 4), (8, 8, 8)]))
    # other types do not touch the subscriptions; the extremes of the type range
    for typ in (0, 0x1A, 0x1C, 0x13):
        out.append(dict(type=typ, dying=9, headers={typ: rb_insert([5, 9, 12]), 0x1B: rb_insert([5, 9, 12])},
                        things=[(5, 1, 5), (9, 3, 9), (12, 11, 12)]))
    # unsigned id order in the tree (ids above 0x7fff do not occur in the thing table; the order of delivery is by id)
    out.append(dict(type=6, dying=0, headers={6: rb_insert([0x27FF, 1, 0x1000, 2, 0x2000])},
                    things=[(i, 1 + (i % 17), i) for i in (0x27FF, 1, 0x1000, 2, 0x2000)]))
    for _ in range(30):
        out.append(post_case(r))
    return out


def ppc_post(e, c):
    ctx = Ctx(e)
    for slot, tree in c["headers"].items():
        ctx.build(slot, tree)
    addrs = []
    for j, (entry, kind, own) in enumerate(c["things"]):
        a = e.alloc(0x10, struct.pack(">HB", own, kind) + POISON * 13)
        e.cl.labels[a] = 10 + j
        addrs.append(a)
    msg = e.alloc(0x10, struct.pack(">IIHH", e.mc_vtable, 0xA5A5A5A5, c["dying"], 0xA5A5) + POISON * 4)
    e.cl.labels[msg] = 2
    e.cl.msg_type = c["type"]
    try:
        for (entry, _, _), a in zip(c["things"], addrs):
            e.write(TABLE + 0x14 * entry, struct.pack(">I", a))
        ret = T.call_ppc(e, A["post"], [ctx.centre, msg])[0]
    finally:
        for entry, _, _ in c["things"]:
            e.write(TABLE + 0x14 * entry, b"\0\0\0\0")
    return done(e, ret=ret, state={s: ctx.dump(s, 1) for s in sorted(c["headers"])}, msg=e.read(msg + 4, 4))


def host_post(lib, c, start, finish):
    for slot, tree in c["headers"].items():
        hbuild(lib, slot, tree)
    for j, (entry, kind, own) in enumerate(c["things"]):
        lib.tpw_mc_set_thing(j, entry, kind, own, 10 + j)
    start()
    ret = lib.tpw_mc_post(c["type"], c["dying"])
    return finish(ret=ret, state={s: hdump(lib, s, 1) for s in sorted(c["headers"])}, msg=POISON * 4)


# ----------------------------------------------------------------- the tests

TESTS = [
    FnTest("CMsgEvent::CMsgEvent", ppc_run(ppc_event_ctor), host_event_ctor,
           edge=lambda: event_cases(__import__("random").Random(1)), rnd=lambda r: dict(type=r.getrandbits(32))),
    FnTest("CMsgEvent::GetType", ppc_run(ppc_event_type), host_event_type,
           edge=lambda: event_cases(__import__("random").Random(2)), rnd=lambda r: dict(type=r.getrandbits(32))),
    FnTest("CMsgEvent::~CMsgEvent", ppc_run(ppc_event_dtor), host_run(host_event_dtor),
           edge=lambda: dtor_cases(__import__("random").Random(3)),
           rnd=lambda r: dict(flag=r.choice([r.getrandbits(32), r.getrandbits(16), 1, 0, MASK]), present=1)),
    FnTest("CSubscriptionHeader::CSubscriptionHeader", ppc_run(ppc_header_ctor), host_run(host_header_ctor),
           edge=lambda: ctor_cases(__import__("random").Random(4)), rnd=lambda r: dict(poison=r.getrandbits(8), byte=0)),
    FnTest("std::__tree::__tree", ppc_run(ppc_tree_ctor), host_run(host_tree_ctor),
           edge=lambda: ctor_cases(__import__("random").Random(5)), rnd=lambda r: dict(poison=r.getrandbits(8), byte=r.getrandbits(8))),
    FnTest("CSubscriptionHeader::~CSubscriptionHeader", ppc_run(ppc_header_dtor), host_run(host_header_dtor),
           edge=lambda: header_dtor_cases(None), rnd=header_dtor_random),
    FnTest("std::__tree::destroy", ppc_run(ppc_destroy), host_run(host_destroy),
           edge=lambda: node_cases(None), rnd=random_node),
    FnTest("std::allocator::deallocate", ppc_run(ppc_allocator), host_run(host_allocator),
           edge=allocator_cases(0), rnd=allocator_random(0)),
    FnTest("std::allocator::destroy", ppc_run(ppc_allocator), host_run(host_allocator),
           edge=allocator_cases(1), rnd=allocator_random(1)),
    FnTest("std::__tree::find", ppc_run(ppc_find), host_run(host_find),
           edge=lambda: find_cases_more(None), rnd=find_random),
    FnTest("std::__red_black_tree::increment", ppc_run(ppc_increment), host_run(host_increment),
           edge=lambda: increment_cases(None), rnd=increment_random),
    FnTest("std::__tree::clear", ppc_run(ppc_clear), host_run(host_clear),
           edge=lambda: clear_cases(None), rnd=clear_random),
    FnTest("std::__tree::erase", ppc_run(ppc_erase), host_run(host_erase),
           edge=lambda: node_cases(None), rnd=random_node),
    FnTest("std::__red_black_tree::balance_erase", ppc_run(ppc_balance_erase), host_run(host_balance_erase),
           edge=lambda: node_cases(None), rnd=random_node),
    FnTest("std::__red_black_tree::rotate_left", ppc_run(ppc_rotate("rotl")), host_run(host_rotate(6)),
           edge=lambda: rotate_cases(None, True), rnd=rotate_random(True)),
    FnTest("std::__red_black_tree::rotate_right", ppc_run(ppc_rotate("rotr")), host_run(host_rotate(7)),
           edge=lambda: rotate_cases(None, False), rnd=rotate_random(False)),
    FnTest("CThing::GetThingId", ppc_run(ppc_thing_id), host_thing_id,
           edge=lambda: thing_cases(__import__("random").Random(6)), rnd=lambda r: dict(own=r.getrandbits(16), kind=r.getrandbits(8))),
    FnTest("CThing::ReceiveMessage", ppc_run(ppc_receive), host_run(host_receive),
           edge=lambda: receive_cases(None), rnd=receive_random),
    FnTest("CMessageCentre::PostMessage", ppc_run(ppc_post), host_run(host_post),
           edge=lambda: post_edges(__import__("random").Random(7)), rnd=post_case),
]
