"""Unit v51076, the UIInterface one-call forwarders and the small CodeWarrior deque pieces, against
libtpw_unit_std___cdeque_pop_front.dylib (tests/unit_std___cdeque_pop_front.cpp), main app, src/game/ui/UIInterface.cpp:
  __cdeque<const void*>::pop_front 0x101f25e8, max_size 0x101f492c, iterator::iterator 0x101f2dd8, std::deque<errorthingy>::deque 0x101f5158,
  __cdeque<errorthingy*>::resize 0x101f5044 and reserve 0x101f50d4, and 20 UIInterface forwarders (0x101ef84c .. 0x101f1270).

A case is a world and a call, built by one piece of Python on both memories. The callees outside the unit are fakes on both sides that
record their arguments (pointers as labels: 0 NULL, 1 object A, 2 object B) and answer from the case (UI_HelpTextEnabled).

The world of a case: random bytes in gPlayerSettings (0x9c), the error deque's first-word global, a cdeque header or iterator words.
What is compared: the event log, gPlayerSettings, the global, the blocks and the answers. No fused operations. Not tested: nothing skipped.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_std___cdeque_pop_front"
MASK = 0xFFFFFFFF

# (test name, address, size, op index, argument kinds: p pointer, s signed, u unsigned)
UI = [
    ("UIInterface::ShowVersionTag", 0x101f1270, 36, 0, ""),
    ("UIInterface::Congratulations", 0x101f120c, 40, 1, "p"),
    ("UIInterface::Bankrupt", 0x101f11b0, 40, 2, "p"),
    ("UIInterface::ChangeCoasterButtonsEnabled", 0x101f0f94, 40, 3, "u"),
    ("UIInterface::ChangeCoasterButtonsDown", 0x101f0f2c, 40, 4, "u"),
    ("UIInterface::EndCoasterEdit", 0x101f0ed4, 36, 5, ""),
    ("UIInterface::OnlineUpdateBuddy", 0x101f0bc8, 44, 6, "ps"),
    ("UIInterface::OnlineRemoveBuddy", 0x101f0b68, 40, 7, "p"),
    ("UIInterface::OnlineAddBuddy", 0x101f0b08, 44, 8, "ps"),
    ("UIInterface::OnlineUpdateContact", 0x101f0aa0, 44, 9, "ps"),
    ("UIInterface::OnlineRemoveContact", 0x101f0a40, 40, 10, "p"),
    ("UIInterface::OnlineAddContact", 0x101f09dc, 44, 11, "ps"),
    ("UIInterface::OnlineReceiveStr", 0x101f0978, 44, 12, "ps"),
    ("UIInterface::ToggleHelpText", 0x101f0908, 60, 13, ""),
    ("UIInterface::Screen_Research", 0x101f08b0, 36, 14, ""),
    ("UIInterface::Screen_EntryPrice", 0x101f0758, 36, 15, ""),
    ("UIInterface::SwitchGameWindow", 0x101ef9fc, 36, 16, ""),
    ("UIInterface::ShowFirstPersonWindow", 0x101ef994, 44, 17, "s"),
    ("UIInterface::TogglePause", 0x101ef940, 36, 18, ""),
    ("UIInterface::DeInitOnline", 0x101ef84c, 36, 19, ""),
]
POP, MAXS, ITER, DEQ, ERES, ERSV = 0x101f25e8, 0x101f492c, 0x101f2dd8, 0x101f5158, 0x101f5044, 0x101f50d4
OWN = {a for _, a, _, _, _ in UI} | {POP, MAXS, ITER, DEQ, ERES, ERSV}

# callees: (event name, address, argument kinds)
CALLEES = [
    ("ShowVersionNumberTag", 0x101928dc, ""), ("UI_Congratulations", 0x1018724c, "p"), ("UI_Bankrupt", 0x10186f34, "p"),
    ("CoasterButtonsEnabled", 0x1019cd6c, "u"), ("CoasterButtonsDown", 0x1019cd00, "u"), ("EndCoasterEditMode", 0x1019cad0, ""),
    ("_OnlineUpdateBuddy", 0x101e8524, "ps"), ("_OnlineRemoveBuddy", 0x101e845c, "p"), ("_OnlineAddBuddy", 0x101e839c, "ps"),
    ("_OnlineUpdateContact", 0x101e81f4, "ps"), ("_OnlineRemoveContact", 0x101e812c, "p"), ("_OnlineAddContact", 0x101e8068, "ps"),
    ("OnlineReceiveString", 0x101e7dd8, "ps"), ("UI_ToggleHelpText", 0x101ca9d0, ""), ("UI_HelpTextEnabled", 0x101cab08, ""),
    ("CreateResearchPanel", 0x101b4f74, ""), ("CreateEntryPricePanel", 0x1019d660, ""), ("ToggleGameWindow", 0x101a8ac0, ""),
    ("UI_SwitchGameWindow", 0x101a8cd8, "s"), ("TogglePauseUI", 0x10186854, ""), ("DeInitOnlineUI", 0x10186620, ""),
    ("CdequeResize", 0x101f2a10, "pup"), ("CdequeReserve", 0x101f2730, "pu"),
]

ADDR = dict(objA=0x10396800, objB=0x10396900, settings=0x1038071c, mapsize=0x102b1428)
SET_SIZE = 0x9c
HOST_LAYOUT = [0x36, SET_SIZE, 16]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def mkstub(name, kinds):
    def stub(e):
        L = e.cl
        vals = []
        for i, k in enumerate(kinds):
            v = e.r(3 + i)
            vals.append(L.lab(v) if k == "p" else (T.s32(v) if k == "s" else v & MASK))
        L.rec(name, tuple(vals))
        if name == "UI_HelpTextEnabled":
            e.set_r(3, L.ret("help") & MASK)
    return stub


for _n, _a, _k in CALLEES:
    STUBS.internal(_a, mkstub(_n, _k))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["objA"]: 1, ADDR["objB"]: 2}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def ptr(self, sym):
        return ADDR["obj" + sym] if sym else 0

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, addr, args):
        r3, _, _ = T.call_ppc(self.e, addr, [a & MASK for a in args])
        return r3 & MASK

    def itercopy(self, d, i, p):
        dest, src = self.alloc(12), self.alloc(12)
        self.words(dest, [0xDDDDDDDD] * 3)
        self.words(src, [d, i, p])
        r = self.call(ITER, [dest, src])
        return [int(r == dest)] + self.rd_words(dest, 3) + self.rd_words(src, 3)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("objA"), 2: self.g("objB")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v_addr(name.encode())

    def ptr(self, sym):
        return self.g("obj" + sym) if sym else 0

    def alloc(self, n):
        return self.lib.tpw_v_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def itercopy(self, d, i, p):
        out = (C.c_uint32 * 6)()
        r = self.lib.tpw_v_itercopy(d & MASK, i & MASK, p & MASK, out)
        return [r] + list(out)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_v_addr.restype, lib.tpw_v_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_v_alloc.restype, lib.tpw_v_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_v_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_v_ui.restype, lib.tpw_v_ui.argtypes = None, [C.c_int32, C.c_uint64, C.c_uint64]
    lib.tpw_v_pop_front.restype, lib.tpw_v_pop_front.argtypes = None, [C.c_uint64]
    lib.tpw_v_max_size.restype, lib.tpw_v_max_size.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_v_itercopy.restype, lib.tpw_v_itercopy.argtypes = C.c_uint32, [C.c_uint32, C.c_uint32, C.c_uint32, C.POINTER(C.c_uint32)]
    lib.tpw_v_dequector.restype, lib.tpw_v_dequector.argtypes = None, [C.c_uint64]
    lib.tpw_v_ecd_resize.restype, lib.tpw_v_ecd_resize.argtypes = None, [C.c_uint64, C.c_uint32, C.c_uint64]
    lib.tpw_v_ecd_reserve.restype, lib.tpw_v_ecd_reserve.argtypes = None, [C.c_uint64, C.c_uint32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_v_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def argval(s, kind, v):
    return s.ptr(v) if kind == "p" else v & MASK


def call_ui(s, spec, args):
    _, addr, _, op, kinds = spec
    vals = [argval(s, k, v) for k, v in zip(kinds, args)]
    if isinstance(s, Ppc):
        s.call(addr, [0] + vals)
    else:
        vals += [0, 0]
        s.lib.tpw_v_ui(op, vals[0], vals[1])


def build(s, case):
    s.write(s.g("settings"), case["settings"])
    s.words(s.g("mapsize"), [case["mapsize"]])


def execute(s, case):
    op = case["op"]
    out = {}
    if op in UI_BY_NAME:
        call_ui(s, UI_BY_NAME[op], case["args"])
    elif op == "pop_front":
        b = s.alloc(16)
        s.words(b, case["words"])
        if isinstance(s, Ppc):
            s.call(POP, [b])
        else:
            s.lib.tpw_v_pop_front(b)
        out["block"] = s.rd_words(b, 4)
    elif op == "max_size":
        b = s.alloc(16)
        s.words(b, case["words"])
        out["answer"] = s.call(MAXS, [b]) if isinstance(s, Ppc) else s.lib.tpw_v_max_size(b)
        out["block"] = s.rd_words(b, 4)
    elif op == "iterator":
        out["iter"] = s.itercopy(*case["words"])
    elif op == "deque":
        b = s.alloc(0x1c + 8)
        s.words(b, case["words"])
        if isinstance(s, Ppc):
            s.call(DEQ, [b, 0])
        else:
            s.lib.tpw_v_dequector(b)
        out["block"] = s.rd_words(b, 9)
    elif op == "resize":
        n, v = case["args"]
        if isinstance(s, Ppc):
            s.call(ERES, [s.ptr("A"), n, s.ptr(v)])
        else:
            s.lib.tpw_v_ecd_resize(s.ptr("A"), n & MASK, s.ptr(v))
    elif op == "reserve":
        n = case["args"][0]
        if isinstance(s, Ppc):
            s.call(ERSV, [s.ptr("A"), n])
        else:
            s.lib.tpw_v_ecd_reserve(s.ptr("A"), n & MASK)
    return out


def observe(s, case, out):
    out = dict(out)
    out["log"] = s.log()
    out["settings"] = s.read(s.g("settings"), SET_SIZE)
    out["mapsize"] = s.rd_words(s.g("mapsize"), 1)
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


UI_BY_NAME = {u[0]: u for u in UI}
PTRS = [None, "A", "B"]
SVALS = [0, 1, -1, 2, 0x7FFFFFFF, -0x80000000, 0x100, 0x12345678]
UVALS = [0, 1, 0x80000000, 0xFFFFFFFF, 0xFF, 0x12345678]
WORDS = [0, 1, 2, 3, 0x10, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF, 0x12345678]


def base(r, op):
    return {"op": op, "queues": {"help": [r.choice([0, 1])]}, "settings": bytes(r.randrange(256) for _ in range(SET_SIZE)),
            "mapsize": r.choice([0x40, 0, r.getrandbits(32)])}


def vals(r, kind, edge=None):
    if kind == "p":
        return r.choice(PTRS) if edge is None else edge
    if kind == "s":
        return r.choice(SVALS) if edge is None else edge
    return r.choice(UVALS) if edge is None else edge


KIND_EDGES = {"p": PTRS, "s": SVALS, "u": UVALS}


def ui_tests(spec):
    name, _, _, _, kinds = spec

    def case(r, args):
        c = base(r, name)
        c["args"] = args
        return c

    def edges():
        r = random.Random(len(name))
        if not kinds:
            for h in (0, 1):
                c = case(r, [])
                c["queues"] = {"help": [h]}
                yield c
        elif len(kinds) == 1:
            for v in KIND_EDGES[kinds]:
                yield case(r, [v])
        else:
            for a in PTRS:
                for b in SVALS:
                    yield case(r, [a, b])

    def rnd(r):
        return case(r, [vals(r, k) for k in kinds])

    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


def words_case(r, op, words):
    c = base(r, op)
    c["words"] = words
    return c


def pop_words(r, cap=None):
    cap = r.choice([0, 1, 2, 5, 0x10, 0xFFFFFFFF]) if cap is None else cap
    return [cap, r.choice([0, 1, 3, cap, r.getrandbits(32)]), r.choice([0, cap - 1, cap, cap + 1, 2 * cap, r.getrandbits(32)]) & MASK,
            r.getrandbits(32)]


def pop_edges():
    r = random.Random(11)
    for cap in (0, 1, 2, 5, 0x10, 0xFFFFFFFF):
        for st in (0, cap - 2, cap - 1, cap, cap + 1, 0xFFFFFFFF):
            for size in (0, 1, 3):
                yield words_case(r, "pop_front", [cap & MASK, size, st & MASK, r.getrandbits(32)])


def pop_random(r):
    return words_case(r, "pop_front", pop_words(r, r.randrange(1, 40)) if r.random() < 0.5 else pop_words(r))


def max_edges():
    r = random.Random(12)
    for w in WORDS:
        yield words_case(r, "max_size", [w, 1, 2, 3])


def max_random(r):
    return words_case(r, "max_size", [r.getrandbits(32) for _ in range(4)])


def iter_edges():
    r = random.Random(13)
    for w in WORDS:
        yield words_case(r, "iterator", [w, WORDS[-1 - WORDS.index(w)], w])


def iter_random(r):
    return words_case(r, "iterator", [r.getrandbits(32) for _ in range(3)])


def deque_edges():
    r = random.Random(14)
    for m in (0, 0x40, 1, 0xFFFFFFFF):
        c = words_case(r, "deque", [r.getrandbits(32) for _ in range(9)])
        c["mapsize"] = m
        yield c


def deque_random(r):
    return words_case(r, "deque", [r.getrandbits(32) for _ in range(9)])


def ecd_resize_case(r, n=None, v=None):
    c = base(r, "resize")
    c["args"] = [r.choice(UVALS) if n is None else n, r.choice(PTRS) if v is None else v]
    return c


def ecd_reserve_case(r, n=None):
    c = base(r, "reserve")
    c["args"] = [r.choice(UVALS) if n is None else n]
    return c


def resize_edges():
    r = random.Random(15)
    for n in UVALS:
        for v in PTRS:
            yield ecd_resize_case(r, n, v)


def reserve_edges():
    r = random.Random(16)
    for n in UVALS:
        yield ecd_reserve_case(r, n)


TESTS = [ui_tests(u) for u in UI] + [
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::pop_front", ppc, host, edge=pop_edges, rnd=pop_random),
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::max_size", ppc, host, edge=max_edges, rnd=max_random),
    FnTest("std::__cdeque<const_void*,std::allocator<const_void*>>::iterator::iterator", ppc, host, edge=iter_edges, rnd=iter_random),
    FnTest("std::deque<errorthingy,std::allocator<errorthingy>>::deque", ppc, host, edge=deque_edges, rnd=deque_random),
    FnTest("std::__cdeque<errorthingy*,std::allocator<errorthingy*>>::resize", ppc, host, edge=resize_edges, rnd=ecd_resize_case),
    FnTest("std::__cdeque<errorthingy*,std::allocator<errorthingy*>>::reserve", ppc, host, edge=reserve_edges, rnd=ecd_reserve_case),
]
