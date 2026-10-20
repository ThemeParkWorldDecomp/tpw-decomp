"""Unit w62, the lobby's small leaves part 4, against libtpw_unit_LobbyLeaves4.dylib (tests/unit_LobbyLeaves4.cpp), in the main app
("SimTheme Park"):
  src/lobby_globe.cpp  CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe 0x100ac90c, CLBScreenComponentDownloadGlobe::Init
                       0x100ae2d0, CLBScreenComponentPublishGlobe::Init 0x100aeac8
  src/FS_Online.cpp    CFSOnlineSystem::Tick 0x10170f80, StopProcessing 0x10171404
  src/email.cpp        Outbox::Outbox 0x10203940, AddressBook::AddressBook 0x102064cc, bthread_mutex::lock 0x10200d8c, unlock 0x10200de0
  src/Game.cpp         CGame::DeInit 0x1014965c
  src/InGame.cpp       CInGame::Init 0x1014b2bc

Every callee outside the unit is a fake that records the same event on both sides: the screen base class's constructor (it records and
sets the visible word; the real one also registers the screen with the lobby, which this test has not), the nesting new of the globe's two
lists (its answers come from the case's queue "New"), the nine online members, FuckOffUIProgressBar, ExitLevel (which can clear gMouse or put
another mouse there, queue "Exit"), AssignMouseMode, CInGame::Start and the two critical section calls (with the handle's address as a
label). Operator delete records a labelled object's deletion. The constructors of the outbox and the address book run the original's MSL
constructors on the PowerPC side; the host's members are empty by construction, so the test compares the plain words, the bytes the
constructor leaves alone and whether every string and list is empty (a string's representation has length 0 and an empty buffer; a list
has size 0 and a sentinel linked to itself).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyLeaves4"
MASK = 0xFFFFFFFF

GLOBE_CTOR, DOWNLOAD_INIT, PUBLISH_INIT = 0x100AC90C, 0x100AE2D0, 0x100AEAC8
BASE_CTOR = 0x100B6BA4
GLOBE_VTABLE, GLOBE_LIST_VTABLE, FLAG_LIST_VTABLE = 0x1029E830, 0x1029E8C0, 0x1029E898
ONLINE_TICK, ONLINE_STOP = 0x10170F80, 0x10171404
ONLINE_MEMBERS = {   # the nine members and the progress bar the two functions call
    "CFSOnlineSystem.InitNews": 0x10172238, "CFSOnlineSystem.InitCity": 0x101735CC, "CFSOnlineSystem.InitUpload": 0x10175A18,
    "CFSOnlineSystem.InitFTPDownload": 0x10176CEC, "CFSOnlineSystem.TickLogin": 0x10171ADC, "CFSOnlineSystem.TickNews": 0x1017232C,
    "CFSOnlineSystem.TickCity": 0x10173AFC, "CFSOnlineSystem.TickUpload": 0x10175B00, "CFSOnlineSystem.TickFTPDownload": 0x10176DA4}
PROGRESS_BAR = 0x101E0294
OUTBOX_CTOR, BOOK_CTOR = 0x10203940, 0x102064CC
LOCK, UNLOCK = 0x10200D8C, 0x10200DE0
ENTER, LEAVE = "EnterCriticalSection__10NS_MacDozeFPPv", "LeaveCriticalSection__10NS_MacDozeFPPv"
DEINIT, EXIT_LEVEL, ASSIGN_MOUSE_MODE, G_MOUSE = 0x1014965C, 0x101496EC, 0x1007E2E0, 0x102DC1F4
INGAME_INIT, INGAME_START = 0x1014B2BC, 0x1014B304
GLOBE_BYTES, GLOBE_WORDS = 0x7C, 31
ONLINE_BYTES = 0x404


def ws(e, a, words):
    e.write(a, struct.pack(">" + "I" * len(words), *[w & MASK for w in words]))


def rs(e, a, n):
    return list(struct.unpack(">" + "I" * n, e.read(a, 4 * n)))


def F(lib, name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype = restype
    fn.argtypes = list(argtypes)
    return fn


STUBS = None


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder

    def base_ctor(e):
        e.cl.rec("Screen.ctor", (e.cl.newlab(e.r(3)),))
        ws(e, e.r(3) + 4, [0])   # the visible word

    def new(e):
        L = e.cl
        L.rec("operator_new")
        L.sizes.append(e.r(3))
        e.set_r(3, 0 if L.ret("New", 1) == 0 else e.alloc(max(e.r(3), 4)))

    def delete(e):
        L, obj = e.cl, e.r(3)
        if obj and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    def exit_level(e):
        L = e.cl
        L.rec("ExitLevel", (L.lab(e.r(3)),))
        action = L.ret("Exit", 0)
        if action == 1:
            ws(e, G_MOUSE, [0])
        elif action == 2:
            ws(e, G_MOUSE, [L.spare])

    S.internal(BASE_CTOR, base_ctor)
    S.imp("__nw__FUl", new)
    S.imp("__dl__FPv", delete)
    for name, address in ONLINE_MEMBERS.items():
        S.internal(address, R(name, "L"))
    S.internal(PROGRESS_BAR, R("FuckOffUIProgressBar", ""))
    S.internal(EXIT_LEVEL, exit_level)
    S.internal(ASSIGN_MOUSE_MODE, R("AssignMouseMode", "LL"))
    S.internal(INGAME_START, R("CInGame.Start", ""))
    S.imp(ENTER, R("EnterCriticalSection", "L"))
    S.imp(LEAVE, R("LeaveCriticalSection", "L"))
    return S


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = build_stubs()


class Ppc:
    """One case on the PowerPC side: the fakes active, a fresh log, the heap in the scratch area."""

    def __init__(self, e, case):
        self.e = e
        e.heap_in_scratch = True
        e.cl = self.L = CL.PpcLog({}, case.get("q"))
        self.L.sizes = []
        self.L.spare = 0
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MASK for a in args])[0]


def host_start(lib, case, fixed=None):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    CL.host_begin(lib, fixed or {}, case.get("q"))


# ---------------------------------------------------------------- the globe: the constructor and the two Inits

def list_ppc(e, L, pointer, vtable):
    """[made, vtable right, head NULL, tail NULL, count] of a TbTList at `pointer` (vtable, head, tail, count)."""
    if pointer == 0:
        return [0, 0, 0, 0, 0]
    v, head, tail, count = rs(e, pointer, 4)
    return [1, int(v == vtable), int(head == 0), int(tail == 0), count]


def globe_ppc(e, case):
    p = Ppc(e, case)
    try:
        kind, words = case["kind"], case["words"]
        obj = e.alloc(GLOBE_BYTES, b"\0" * GLOBE_BYTES)
        ws(e, obj, [0] + words[1:])
        p.L.labels[obj] = 0
        p.call([GLOBE_CTOR, DOWNLOAD_INIT, PUBLISH_INIT][kind], obj)
        w = rs(e, obj, GLOBE_WORDS)
        # (the Inits leave the pointer words as they were: only the constructor's lists are looked at)
        lists = [list_ppc(e, p.L, w[2] if kind == 0 else 0, GLOBE_LIST_VTABLE), list_ppc(e, p.L, w[27] if kind == 0 else 0, FLAG_LIST_VTABLE)]
        vtable_ok = int(w[0] == GLOBE_VTABLE) if kind == 0 else 1
        w[0] = 0
        if kind == 0:   # pointers compare as made-flags (the host's are 64-bit)
            w[2], w[27] = int(w[2] != 0), int(w[27] != 0)
        return dict(words=w, lists=lists, vtable=vtable_ok, sizes_ok=int(all(s == 0x10 for s in p.L.sizes)), log=p.L.log)
    finally:
        p.done()


def globe_host(lib, case):
    host_start(lib, case)
    out = (C.c_uint32 * (GLOBE_WORDS + 10))()
    F(lib, "tpw_ll4_globe", None, C.c_int32, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32))(
        case["kind"], (C.c_uint32 * GLOBE_WORDS)(*[w & MASK for w in case["words"]]), out)
    log = CL.host_end(lib)
    values = list(out)
    return dict(words=values[:GLOBE_WORDS], lists=[values[GLOBE_WORDS:GLOBE_WORDS + 5], values[GLOBE_WORDS + 5:]], vtable=1, sizes_ok=1,
                log=log)


def random_words(r):
    kind = r.randrange(4)
    if kind == 0:
        return [0xA5A5A5A5] * GLOBE_WORDS
    if kind == 1:
        return [r.getrandbits(32) for _ in range(GLOBE_WORDS)]
    if kind == 2:
        return [r.choice([0, 1, MASK, 0x70, 0x80000000, 0x7FFFFFFF]) for _ in range(GLOBE_WORDS)]
    return [0x01010101 * (i + 1) & MASK for i in range(GLOBE_WORDS)]


def globe_case(kind, words, new):
    return dict(kind=kind, words=words, q={"New": new})


def globe_edges():
    r = random.Random(62)
    for new in ([1, 1], [0, 1], [1, 0], [0, 0]):
        for words in ([0xA5A5A5A5] * GLOBE_WORDS, [0] * GLOBE_WORDS, [MASK] * GLOBE_WORDS, random_words(r)):
            yield globe_case(0, words, new)
    for kind in (1, 2):
        for words in ([0] * GLOBE_WORDS, [1] * GLOBE_WORDS, [MASK] * GLOBE_WORDS, [0xA5A5A5A5] * GLOBE_WORDS, random_words(r)):
            yield globe_case(kind, words, [1, 1])
        for value in (0, 1, MASK, 0x7C500000):
            words = [0xA5A5A5A5] * GLOBE_WORDS
            words[28] = value
            yield globe_case(kind, words, [1, 1])


def globe_random(r):
    kind = r.choice([0, 0, 0, 1, 2])
    return globe_case(kind, random_words(r), [r.choice([0, 1, 1]), r.choice([0, 1, 1])])


# ---------------------------------------------------------------- the online system: Tick and StopProcessing

def online_ppc(e, case):
    p = Ppc(e, case)
    try:
        fill = case["fill"]
        obj = e.alloc(ONLINE_BYTES, bytes([fill]) * ONLINE_BYTES)
        p.L.labels[obj] = 1
        intact = []
        for which in case["ops"]:
            p.call(ONLINE_TICK if which == 0 else ONLINE_STOP, obj)
            intact.append(int(e.read(obj, ONLINE_BYTES) == bytes([fill]) * ONLINE_BYTES))
        return dict(intact=intact, log=p.L.log)
    finally:
        p.done()


def online_host(lib, case):
    host_start(lib, case)
    run = F(lib, "tpw_ll4_online", C.c_int32, C.c_int32, C.c_int32)
    intact = [run(which, case["fill"]) for which in case["ops"]]
    return dict(intact=intact, log=CL.host_end(lib))


def online_case(ops, fill=0xA5):
    return dict(ops=ops, fill=fill)


def online_edges():
    for ops in ([0], [1], [0, 1], [1, 0], [0, 0], [1, 1], [0, 1, 0, 1], [1, 0, 0, 1, 1]):
        yield online_case(ops)
        yield online_case(ops, 0)
        yield online_case(ops, 0xFF)


def online_random(r, only=None):
    ops = [r.randrange(2) if only is None else only for _ in range(r.randrange(1, 9))]
    return online_case(ops, r.choice([0, 0xA5, 0xFF, r.randrange(256)]))


# ---------------------------------------------------------------- the e-mail containers

def string_empty(e, address):
    """An MSL string at `address`: a pointer to a representation of (length, capacity, count, buffer)."""
    rep = e.u32(address)
    buffer = e.u32(rep + 12)
    return int(e.u32(rep) == 0 and buffer != 0 and e.read(buffer, 1) == b"\0")


def list_empty(e, address):
    """An MSL list at `address`: its size, then a sentinel that links to itself."""
    return int(e.u32(address) == 0 and e.u32(address + 4) == address + 4 and e.u32(address + 8) == address + 4)


def outbox_ppc(e, case):
    p = Ppc(e, case)
    try:
        fill = case["fill"]
        obj = e.alloc(0x60, bytes([fill]) * 0x60)
        p.call(OUTBOX_CTOR, obj)
        short = struct.unpack(">H", e.read(obj + 0x54, 2))[0]
        pad = e.read(obj + 0x56, 2)
        unwritten = e.read(obj + 0x1C, 12) + e.read(obj + 0x4C, 4)
        words = [short, e.u32(obj + 0x58), e.u32(obj + 0x5C)] + list(struct.unpack("<IIII", unwritten))
        flags = [string_empty(e, obj + o) for o in (0x00, 0x04, 0x08, 0x18, 0x50)]
        flags += [list_empty(e, obj + o) for o in (0x0C, 0x28, 0x34, 0x40)]
        # (the MSL string constructors allocate their representations: five of 16 and 4 bytes; the host's empty strings do not)
        log = [event for event in p.L.log if event[0] != "operator_new"]
        return dict(words=words, flags=flags, pad=int(pad == bytes([fill]) * 2), log=log)
    finally:
        p.done()


def outbox_host(lib, case):
    host_start(lib, case)
    out = (C.c_uint32 * 16)()
    F(lib, "tpw_ll4_outbox", None, C.c_int32, C.POINTER(C.c_uint32))(case["fill"], out)
    values = list(out)
    return dict(words=values[:7], flags=values[7:], pad=1, log=CL.host_end(lib))


def book_ppc(e, case):
    p = Ppc(e, case)
    try:
        fill = case["fill"]
        obj = e.alloc(0x10, bytes([fill]) * 0x10)
        p.call(BOOK_CTOR, obj)
        # the list is empty, and the word at +0xc takes the word at +8 (the MSL list's sentinel bookkeeping: no host counterpart)
        return dict(empty=list_empty(e, obj), relation=int(e.u32(obj + 0xC) == e.u32(obj + 8)), log=p.L.log)
    finally:
        p.done()


def book_host(lib, case):
    host_start(lib, case)
    empty = F(lib, "tpw_ll4_book", C.c_int32, C.c_int32)(case["fill"])
    return dict(empty=empty, relation=1, log=CL.host_end(lib))


def fill_edges():
    for fill in (0xA5, 0, 0xFF, 1, 0x80, 0x19):
        yield dict(fill=fill)


def fill_random(r):
    return dict(fill=r.choice([0xA5, 0, 0xFF, r.randrange(256)]))


# ---------------------------------------------------------------- the mutex

def mutex_ppc(e, case):
    p = Ppc(e, case)
    try:
        objs = [e.alloc(16, b"\xa5" * 16), e.alloc(16, b"\xa5" * 16)]
        for i, handle in enumerate(case["handles"]):
            ws(e, objs[i], [handle])
            p.L.labels[objs[i]] = i + 1
        returns = []
        for which, lock in case["ops"]:
            returns.append(p.call(LOCK if lock else UNLOCK, objs[which]))
        # (the original loads 0 into r3 on the way out of both; nothing reads it)
        return dict(handles=[e.u32(o) for o in objs], zero=int(all(r == 0 for r in returns)), log=p.L.log)
    finally:
        p.done()


def mutex_host(lib, case):
    address = F(lib, "tpw_ll4_mutex", C.c_void_p, C.c_int32)
    set_handle = F(lib, "tpw_ll4_mutex_set", None, C.c_int32, C.c_uint32)
    get_handle = F(lib, "tpw_ll4_mutex_get", C.c_uint32, C.c_int32)
    run = F(lib, "tpw_ll4_mutex_run", None, C.c_int32, C.c_int32)
    for i, handle in enumerate(case["handles"]):
        set_handle(i, handle)
    host_start(lib, case, {1: address(0), 2: address(1)})
    for which, lock in case["ops"]:
        run(which, lock)
    return dict(handles=[get_handle(0), get_handle(1)], zero=1, log=CL.host_end(lib))


def mutex_case(handles, ops):
    return dict(handles=handles, ops=ops)


def mutex_edges():
    for handles in ([0, 0], [0, 5], [0x7C500000, 0], [MASK, 1]):
        for ops in ([(0, 1)], [(0, 0)], [(1, 1)], [(1, 0)], [(0, 1), (0, 0)], [(0, 1), (1, 1), (1, 0), (0, 0)], [(1, 0), (1, 0), (0, 1)]):
            yield mutex_case(handles, ops)


def mutex_random(r):
    return mutex_case([r.choice([0, 1, MASK, r.getrandbits(32)]) for _ in range(2)],
                      [(r.randrange(2), r.randrange(2)) for _ in range(r.randrange(1, 8))])


# ---------------------------------------------------------------- CGame::DeInit

MOUSE_A, MOUSE_B = 0x7C600000, 0x7C600100


def deinit_ppc(e, case):
    p = Ppc(e, case)
    try:
        game = e.alloc(0x40)
        first = e.alloc(8, b"\xa5" * 8) if case["mouse"] else 0
        second = e.alloc(8, b"\xa5" * 8) if case["spare"] else 0
        p.L.labels[game] = 1
        for label, obj in ((2, first), (3, second)):
            if obj:
                p.L.labels[obj] = label
        p.L.spare = second
        ws(e, G_MOUSE, [first])
        p.call(DEINIT, game)
        return dict(out=[p.L.lab(e.u32(G_MOUSE)), int(first != 0 and p.L.lab(first) > 0), int(second != 0 and p.L.lab(second) > 0)],
                    mouse_word=e.u32(G_MOUSE), log=p.L.log)
    finally:
        p.done()


def deinit_host(lib, case):
    host_start(lib, case)
    out = (C.c_int32 * 3)()
    F(lib, "tpw_ll4_deinit", None, C.c_int32, C.c_int32, C.POINTER(C.c_int32))(case["mouse"], case["spare"], out)
    return dict(out=list(out), mouse_word=0, log=CL.host_end(lib))


def deinit_case(mouse, spare, exit_action):
    return dict(mouse=mouse, spare=spare, q={"Exit": [exit_action]})


def deinit_edges():
    for mouse in (0, 1):
        for spare in (0, 1):
            for action in (0, 1, 2):
                yield deinit_case(mouse, spare, action)


def deinit_random(r):
    return deinit_case(r.randrange(2), r.randrange(2), r.choice([0, 0, 1, 2]))


# ---------------------------------------------------------------- CInGame::Init

def ingame_ppc(e, case):
    p = Ppc(e, case)
    try:
        for _ in range(case["times"]):
            p.call(INGAME_INIT)
        return dict(log=p.L.log)
    finally:
        p.done()


def ingame_host(lib, case):
    host_start(lib, case)
    F(lib, "tpw_ll4_ingame", None, C.c_int32)(case["times"])
    return dict(log=CL.host_end(lib))


TESTS = [
    FnTest("CLBScreenComponentOnlineGlobe::CLBScreenComponentOnlineGlobe, DownloadGlobe::Init, PublishGlobe::Init", globe_ppc, globe_host,
           edge=globe_edges, rnd=globe_random),
    FnTest("CFSOnlineSystem::Tick and StopProcessing", online_ppc, online_host, edge=online_edges, rnd=online_random),
    FnTest("Outbox::Outbox", outbox_ppc, outbox_host, edge=fill_edges, rnd=fill_random),
    FnTest("AddressBook::AddressBook", book_ppc, book_host, edge=fill_edges, rnd=fill_random),
    FnTest("bthread_mutex::lock and unlock", mutex_ppc, mutex_host, edge=mutex_edges, rnd=mutex_random),
    FnTest("CGame::DeInit", deinit_ppc, deinit_host, edge=deinit_edges, rnd=deinit_random),
    FnTest("CInGame::Init", ingame_ppc, ingame_host, edge=lambda: [dict(times=n) for n in range(5)], rnd=lambda r: dict(times=r.randrange(1, 5))),
]
