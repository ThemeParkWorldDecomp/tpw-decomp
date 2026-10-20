"""Unit v51123: the FTP post-login callbacks, the login getters, Login, Logout, the park/server/theme CompleteGetList instances, two
std::__tree<unsigned short> helpers and two status panel members, against libtpw_unit_CFSOnlineSystem_GotoPlayerByIDFTPPostLoginCallba.dylib
(tests/unit_CFSOnlineSystem_GotoPlayerByIDFTPPostLoginCallba.cpp), main app ("SimTheme Park"),
src/FS_Online_init_CFSOnlineSystem_GotoPlayerByIDFTPPostLoginCallba.cpp:
  GotoPlayerByIDFTPPostLoginCallback 0x10176180, LoggedInGotoPlayerRequestFTPDownloadByID 0x10176390, RequestFTPPostLoginCallback 0x10175f24,
  GetChildLock 0x10171980, GetLoginCustomerID 0x1017193c, GetLoginPlayerID 0x101718fc, GetLoginEmailAddress 0x101718b8,
  GetLoginPassword 0x10171878, GetLoginCustomerName 0x10171834, CanWeSendEmail 0x101717cc, Logout 0x10171398, LoginRASCancelled 0x10171114,
  Login 0x101710ac, the CompleteGetList instances 0x101741d4 (park), 0x1017464c (server), 0x1017488c (theme), the tree constructor 0x1016eaf0,
  the node deallocate 0x1016e8b0, BogStatusPanel::Remove 0x1019ad1c, FeatureStatusPanel::InitViewWindow 0x1019e2d4.

A case is a world and a call. The world is built by Python code twice: in emulator memory at the Mac's offsets (big-endian) for the original,
and on the host by the library's one entry point tpw_v51123_do, which builds the host objects (native layout) from the same values. The
callees outside the unit (CompleteFTPDownload, LoggedInRequestFTPDownload, CompleteLogin, InitLogin, StopProcessing, the system
constructor, GetList of the by-id list, CreateOnlineLoginPanel, CloseWindow, AddUIError, RemoveHead, __ptr_glue, operator new/delete,
__destroy_new_array) are fakes on both sides that record the same events. The callback pointers the unit passes (TVectors on the Mac) are
recorded as a flag: equal to the expected function or not. The ptr_glue calls are recorded as StoreReceivedList (vtable +0x14), DeleteRecord
(record vtable +8, flag 1) and ListCallback (the list's callback word, result, context, list). The online system's operator new is recorded
with the Mac size (the host object is bigger); __destroy_new_array is recorded with a check of its TVector.

What is compared: the answers, the event log, the words Login stores, the list's words, the received list's words and the list count after
CompleteGetList, gOnlineSystem as a label, the tree header words. No fused operations. Not tested: a NULL system for the getters (the
original reads through it), a NULL comparator pointer for the tree constructor (it reads a byte), a received list with more than one info
(the destroy-array fake is a single event on the Mac, one element on the host).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystem_GotoPlayerByIDFTPPostLoginCallba"
MASK = 0xFFFFFFFF

# ---- the unit's own functions
FN = dict(
    goto_cb=(0x10176180, 60), lgoto=(0x10176390, 56), req_cb=(0x10175F24, 64), child=(0x10171980, 8), cust=(0x1017193C, 8),
    player=(0x101718FC, 8), email=(0x101718B8, 8), pwd=(0x10171878, 8), name=(0x10171834, 8), cansend=(0x101717CC, 48),
    logout=(0x10171398, 60), rasc=(0x10171114, 40), login=(0x101710AC, 48), cgl_theme=(0x1017488C, 468), cgl_server=(0x1017464C, 468),
    cgl_park=(0x101741D4, 468), tree=(0x1016EAF0, 44), dealloc=(0x1016E8B0, 40), bog=(0x1019AD1C, 36), feature=(0x1019E2D4, 4),
)
OWN = set(a for a, _ in FN.values())

# ---- the Mac's addresses of the globals and the TVectors
G_ONLINE_SYSTEM = 0x10380AB0
G_TREE_WORD = 0x102A722C
TV_GETLIST_CB = 0x10267450
TV_LOGIN_FAIL = 0x102674B0
TV_INFOS = {0: 0x10267508, 1: 0x10267490, 2: 0x10267498}   # park, server, theme
STORE_MARK, DTOR_MARK = 0x70000014, 0x70000008
ADDR = {}
LABELS = dict(sys=1, parks=2, list=3, mlist=4, recv=5, infos=6, rec0=7, rec1=8, rec2=9, panel=10, tree=11, node=12)
CUR = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rec(name, spec):
    def fn(e):
        L = e.cl
        L.rec(name, tuple(f(L, e) for f in spec))
    return fn


def lab(r):
    return lambda L, e: L.lab(e.r(r))


def i32(r):
    return lambda L, e: s32(e.r(r))


STUBS.internal(0x10176E90, rec("CompleteFTPDownload", [lab(3), i32(4)]))
STUBS.internal(0x10176418, rec("LoggedInRequestFTPDownload", [lab(3), lab(4)]))
STUBS.internal(0x10171C08, rec("CompleteLogin", [lab(3), i32(4)]))
STUBS.internal(0x101719F8, rec("InitLogin", [lab(3)]))
STUBS.internal(0x10171404, rec("StopProcessing", [lab(3)]))
STUBS.internal(0x1016F7D0, CL.recorder("SystemCtor", "N"))
STUBS.internal(0x10179794, rec("GetList", [lab(3), i32(4), lambda L, e: 1 if (e.r(5) & MASK) == TV_GETLIST_CB else 0, lab(6)]))
STUBS.internal(0x101E27BC, rec("CreateOnlineLoginPanel", [lambda L, e: 1 if (e.r(3) & MASK) == TV_LOGIN_FAIL else 0]))
STUBS.internal(0x10193774, rec("CloseWindow", [lab(3)]))
STUBS.internal(0x101F1DF0, rec("AddUIError", [i32(3), lab(4)]))


def stub_removehead(e):
    L = e.cl
    lst = e.r(3) & MASK
    L.rec("RemoveHead", (L.lab(lst),))
    k = L.ret("RemoveHead")
    e.write(lst + 0xC, struct.pack(">i", e.s32(lst + 0xC) - 1))
    e.set_r(3, [0, ADDR["rec0"], ADDR["rec1"], ADDR["rec2"]][k])


for _a in (0x10177B98, 0x1017782C, 0x10177904):
    STUBS.internal(_a, stub_removehead)


def stub_new(e):
    n = e.r(3) & MASK
    e.set_r(3, e.alloc(max(n, 4)))


def stub_delete(e):
    L = e.cl
    if L.lab(e.r(3)):   # the shim logs the delete of a labelled object as "delete"
        L.rec("delete", (L.lab(e.r(3)),))


def stub_destroy_array(e):
    L = e.cl
    L.rec("DestroyInfos", (L.lab(e.r(3)),))
    if (e.r(4) & MASK) != TV_INFOS[CUR["kind"]]:
        L.rec("BADVECTOR", (s32(e.r(4)),))


def stub_ptr_glue(e):
    L = e.cl
    target = e.r(12) & MASK
    if target == STORE_MARK:
        L.rec("StoreReceivedList", (L.lab(e.r(3)), L.lab(e.r(4))))
    elif target == DTOR_MARK:
        L.rec("DeleteRecord", (L.lab(e.r(3)), s32(e.r(4))))
    else:
        L.rec("ListCallback", (s32(target), s32(e.r(3)), s32(e.r(4)), L.lab(e.r(5))))


STUBS.internal(0x102384B8, stub_new)
STUBS.internal(0x10238218, stub_delete)
STUBS.internal(0x10238AD0, stub_destroy_array)
STUBS.internal(0x1023BAA8, stub_ptr_glue)


def setup(runner):
    e = runner.emu
    for name, size in (("sys", 0x420), ("parks", 0x40), ("list", 0x40), ("mlist", 0x10), ("recv", 0x10), ("infos", 0x40), ("rec0", 0x10),
                       ("rec1", 0x10), ("rec2", 0x10), ("panel", 0x40), ("tree", 0x20), ("node", 0x10), ("vtlist", 0x20), ("vtrec", 0x10)):
        ADDR[name] = T.persist_alloc(e, size)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"
    ppc = True

    def __init__(self, e, case):
        self.e = e
        CUR.clear()
        CUR.update(case)
        self.L = e.cl = CL.PpcLog({ADDR[n]: v for n, v in LABELS.items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK


class Host(Side):
    fmt = "<I"
    ppc = False

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {}
        if case["op"] == "cgl":
            self.do(7, [case["kind"]] + case["p"])    # the host objects exist before the labels are given
        if case["op"] == "dealloc":
            self.do(14, [])
        for n, v in LABELS.items():
            fixed[v] = self.g(n)
        CL.host_begin(lib, fixed, case["queues"])

    def do(self, op, p):
        arr = (C.c_int64 * 32)(*p)
        self.out = (C.c_int64 * 32)()
        self.n = self.lib.tpw_v51123_do(op, arr, self.out)
        return self.n

    def outs(self):
        return list(self.out)[:self.n]

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v51123_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_v51123_addr.restype, lib.tpw_v51123_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_v51123_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_v51123_do.restype = C.c_int64
    lib.tpw_v51123_do.argtypes = [C.c_int32, C.POINTER(C.c_int64), C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_v51123_layout(out)
    assert list(out)[:n] == [], list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the operations

HOSTOP = dict(goto_cb=0, lgoto=1, req_cb=2, get=3, logout=4, rasc=5, login=6)
ENTRY = {k: v[0] for k, v in FN.items()}


def sys_build(s, c):
    """The online system with the by-id park list at 0xf0 and the word at 0x3a8 (the Mac block, zeroed)."""
    sysa = s.g("sys")
    s.words(sysa, [0] * 0x101)
    s.words(sysa + 0xF0, [s.g("parks") if c.get("park") else 0])
    s.words(sysa + 0x3A8, [c.get("f3a8", 0)])
    return sysa


def op_basic(s, c):
    op = c["op"]
    if s.ppc:
        sysa = sys_build(s, c)
        args = {"goto_cb": [sysa, c["result"], c["ctx"]], "lgoto": [sysa], "req_cb": [sysa, c["result"], c["ctx"]],
                "logout": [sysa], "rasc": [sysa]}[op]
        s.call(ENTRY[op], args)
    else:
        p = {"goto_cb": [c.get("result", 0), c.get("ctx", 0), c.get("f3a8", 0), 1 if c.get("park") else 0],
             "lgoto": [c.get("f3a8", 0), 1 if c.get("park") else 0], "req_cb": [c.get("result", 0), c.get("ctx", 0)], "logout": [],
             "rasc": []}[op]
        s.do(HOSTOP[op], p)
    return {"log": s.log()}


GETFN = ["child", "cust", "player", "email", "pwd", "name", "cansend"]
GETOFF = dict(child=0x58, cust=0x38, player=0x34, email=0x44, pwd=0x28, name=0x14, logged=0x60)


def op_get(s, c):
    v = c["vals"]   # child, cust, player, email, pwd, name, logged
    which = c["which"]
    if s.ppc:
        sysa = sys_build(s, c)
        for key, val in zip(("child", "cust", "player", "email", "pwd", "name", "logged"), v):
            s.words(sysa + GETOFF[key], [val])
        answer = s.call(ENTRY[GETFN[which]], [sysa])
    else:
        answer = s.do(3, [which] + v) & MASK
    return {"answer": answer, "log": s.log()}


def op_login(s, c):
    if s.ppc:
        sysa = sys_build(s, c)
        s.call(ENTRY["login"], [sysa, c["cb"], c["ctx"]])
        words = s.rd_words(sysa + 4, 2)
    else:
        s.do(6, [c["cb"], c["ctx"]])
        words = [x & MASK for x in s.outs()]
    return {"words": words, "log": s.log()}


def cgl_build_ppc(s, c):
    kind, p = c["kind"], c["p"]
    mF08, mF14, mF18, mF1C, mF20, mF24, mF28, count, rp, rcount, ip, rF08, ref, sysp = p
    A = s.g
    s.words(A("vtlist"), [0, 0, 0, 0, 0, STORE_MARK, 0, 0])
    s.words(A("vtrec"), [0, 0, DTOR_MARK, 0])
    for n in ("rec0", "rec1", "rec2"):
        s.words(A(n), [A("vtrec"), 0, 0, 0])
    s.words(A("sys"), [0] * 0x101)
    s.words(A("mlist"), [0, 0, 0, count])
    s.words(A("recv"), [0] * 4)
    if rp:
        s.words(A("recv"), [rcount, A("infos") if ip else 0, rF08, ref])
    s.words(A("infos"), [0x11111111] * 16)
    s.words(A("list"), [A("vtlist"), 0, mF08, 0, 0, mF14, mF18, mF1C, mF20, mF24, mF28, A("mlist"), A("recv") if rp else 0])
    s.words(G_ONLINE_SYSTEM, [A("sys") if sysp else 0])


def op_cgl(s, c):
    kind = c["kind"]
    if s.ppc:
        cgl_build_ppc(s, c)
        s.call(ENTRY[("cgl_park", "cgl_server", "cgl_theme")[kind]], [s.g("list"), c["error"]])
        L = s.L
        a = lambda n: s.rd_word(s.g(n))
        lst, recv = s.g("list"), s.g("recv")
        state = [s32(s.rd_word(lst + 0x18)), s32(s.rd_word(lst + 0x24)), s32(s.rd_word(lst + 0x28)), L.lab(s.rd_word(lst + 0x30)),
                 s32(s.rd_word(recv + 0xC)), L.lab(s.rd_word(recv + 4)), s32(s.rd_word(recv + 8)), s32(s.rd_word(s.g("mlist") + 0xC)),
                 L.lab(s.rd_word(G_ONLINE_SYSTEM))]
    else:
        s.do(8, [kind, c["error"]])
        n = s.do(9, [kind])
        state = s.outs()
        assert n == 9
    log = s.log()
    if c["p"][12] == 1 and "StoreReceivedList" in str(log):
        # the received list was freed by the unit (the allocator is the shim's): its words are not observable
        state = state[:4] + [None, None, None] + state[7:]
    return {"state": state, "log": log}


def op_tree(s, c):
    w = c["words"]   # three garbage words and the global word
    if s.ppc:
        tree, node = s.g("tree"), s.g("node")
        s.words(tree, w[:3] + [0x5A5A5A5A])
        s.words(G_TREE_WORD, [w[3]])
        s.write(node, bytes([c["byte"]]) + b"\0\0\0")
        s.call(ENTRY["tree"], [tree, node])
        r = s.rd_words(tree, 3)
        state = [r[0], r[1], 1 if r[2] - tree == 4 else 0]   # points at the second word
    else:
        s.do(10, w)
        o = s.outs()
        state = [o[0], o[1], 1 if o[2] == 8 else 0]
    return {"state": [x & MASK for x in state], "log": s.log()}


def op_dealloc(s, c):
    if s.ppc:
        s.call(ENTRY["dealloc"], [s.g("tree"), s.g("node"), c["count"]])
    else:
        s.do(11, [c["count"]])
    return {"log": s.log()}


def op_panel(s, c):
    pa = s.g("panel")
    s.words(pa, c["panel"])
    if s.ppc:
        s.call(ENTRY[c["op"]], [pa])
    else:
        s.do(12 if c["op"] == "bog" else 13, [])
    return {"panel": s.rd_words(pa, 8), "log": s.log()}


OPFN = dict(goto_cb=op_basic, lgoto=op_basic, req_cb=op_basic, logout=op_basic, rasc=op_basic, get=op_get, login=op_login, cgl=op_cgl,
            tree=op_tree, dealloc=op_dealloc, bog=op_panel, feature=op_panel)


def run(s, c):
    try:
        return OPFN[c["op"]](s, c)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def rw(r):
    return r.choice([0, 1, 0x20, 0x1F, 0x21, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, r.getrandbits(32), r.getrandbits(32), r.randrange(0, 0x100)])


def base(r, op, **kw):
    c = {"op": op, "queues": {"RemoveHead": [r.randrange(0, 4) for _ in range(3)]}}
    c.update(kw)
    return c


def basic_case(r, op, result=None, park=None):
    return base(r, op, result=r.choice([0, 1, -1, 5, 0x7FFFFFFF]) & MASK if result is None else result, ctx=r.getrandbits(32),
                f3a8=rw(r), park=r.choice([0, 1]) if park is None else park)


def mk_basic(op):
    def edges():
        r = random.Random(11)
        for result in (0, 1, 0xFFFFFFFF, 0x80000000):
            for park in (0, 1):
                yield basic_case(r, op, result, park)

    return edges, lambda r: basic_case(r, op)


def get_case(r, which, vals=None):
    if vals is None:
        vals = [rw(r) for _ in range(7)]
        vals[6] = r.choice([0, 1, 0xFFFFFFFF, rw(r)])
    return base(r, "get", which=which, vals=vals, f3a8=0, park=0)


def mk_get(which):
    def edges():
        r = random.Random(22 + which)
        for child in (0, 0x20, 0x1F, 0x21, 0xFFFFFFDF, 0xFFFFFFFF, 0x40, 0x80000000):
            for logged in (0, 1, 0x100, 0xFFFFFFFF):
                yield get_case(r, which, [child, rw(r), rw(r), rw(r), rw(r), rw(r), logged])

    return edges, lambda r: get_case(r, which)


def login_case(r):
    return base(r, "login", cb=r.choice([0, 1, 0xFFFFFFFF, r.getrandbits(32)]), ctx=r.choice([0, 0xFFFFFFFF, r.getrandbits(32)]), f3a8=0, park=0)


def cgl_case(r, kind, **kw):
    cb = r.choice([0, r.randrange(1, 0x6FFFFFFF)])
    p = dict(mF08=r.choice([0, 1, 7]), mF14=r.choice([0, 1, 3]), mF18=r.choice([0, 0, 5, 0x1234]), mF1C=cb, mF20=r.getrandbits(32),
             mF24=r.randrange(0, 100), mF28=r.choice([0, 1, 2]), count=r.choice([0, 1, 2, 3]), rp=r.choice([0, 1]), rcount=r.randrange(0, 9),
             ip=r.choice([0, 1]), rF08=r.randrange(0, 1000), ref=r.choice([1, 1, 2, 0, -1 & MASK]), sysp=r.choice([0, 1]))
    p.update(kw)
    error = p.pop("error", r.choice([0, 0, 1, 5]))
    order = ["mF08", "mF14", "mF18", "mF1C", "mF20", "mF24", "mF28", "count", "rp", "rcount", "ip", "rF08", "ref", "sysp"]
    return base(r, "cgl", kind=kind, error=error, p=[p[k] for k in order], f3a8=0, park=0)


def mk_cgl(kind):
    def edges():
        r = random.Random(33 + kind)
        for error in (0, 1):
            for mF18 in (0, 5):
                for mF14 in (0, 1):
                    for rp in (0, 1):
                        for mF08 in (0, 1):
                            yield cgl_case(r, kind, error=error, mF18=mF18, mF14=mF14, rp=rp, mF08=mF08)
        for ref in (1, 2, 0):
            for ip in (0, 1):
                for sysp in (0, 1):
                    for mF28 in (0, 1):
                        yield cgl_case(r, kind, error=0, mF18=0, mF14=1, rp=1, ref=ref, ip=ip, sysp=sysp, mF28=mF28, mF08=1)
        for count in (0, 1, 2, 3):
            yield cgl_case(r, kind, error=0, mF18=0, mF14=1, count=count)

    return edges, lambda r: cgl_case(r, kind)


def tree_case(r):
    return base(r, "tree", words=[r.getrandbits(32) for _ in range(4)], byte=r.randrange(256), f3a8=0, park=0)


def tree_edges():
    r = random.Random(44)
    for w3 in (0, 1, 0xFFFFFFFF, 0x10380AB0):
        c = tree_case(r)
        c["words"][3] = w3
        yield c


def dealloc_case(r):
    return base(r, "dealloc", count=r.choice([0, 1, 5, 0xFFFFFFFF, r.getrandbits(32)]), f3a8=0, park=0)


def panel_case(r, op):
    return base(r, op, panel=[r.getrandbits(32) for _ in range(8)], f3a8=0, park=0)


def edges_of(gen, n, seed):
    def edges():
        r = random.Random(seed)
        for _ in range(n):
            yield gen(r)

    return edges


def T_(name, edges, rnd):
    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


def basic_test(name, op):
    edges, rnd = mk_basic(op)
    return T_(name, edges, rnd)


def get_test(name, which):
    edges, rnd = mk_get(which)
    return T_(name, edges, rnd)


def cgl_test(name, kind):
    edges, rnd = mk_cgl(kind)
    return T_(name, edges, rnd)


TESTS = [
    basic_test("CFSOnlineSystem::GotoPlayerByIDFTPPostLoginCallback", "goto_cb"),
    basic_test("CFSOnlineSystem::LoggedInGotoPlayerRequestFTPDownloadByID", "lgoto"),
    basic_test("CFSOnlineSystem::RequestFTPPostLoginCallback", "req_cb"),
    get_test("CFSOnlineSystem::GetChildLock", 0),
    get_test("CFSOnlineSystem::GetLoginCustomerID", 1),
    get_test("CFSOnlineSystem::GetLoginPlayerID", 2),
    get_test("CFSOnlineSystem::GetLoginEmailAddress", 3),
    get_test("CFSOnlineSystem::GetLoginPassword", 4),
    get_test("CFSOnlineSystem::GetLoginCustomerName", 5),
    get_test("CFSOnlineSystem::CanWeSendEmail", 6),
    basic_test("CFSOnlineSystem::Logout", "logout"),
    basic_test("CFSOnlineSystem::LoginRASCancelled", "rasc"),
    T_("CFSOnlineSystem::Login", edges_of(login_case, 8, 55), login_case),
    cgl_test("CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>,ccThemeList>::CompleteGetList", 2),
    cgl_test("CFSOnlineSystemList<TbPtrList<CFSOnlineServer>,ccServerList>::CompleteGetList", 1),
    cgl_test("CFSOnlineSystemList<TbPtrList<CFSOnlinePark>,ccParkList>::CompleteGetList", 0),
    T_("std::__tree<unsigned_short,std::less<unsigned_short>,std::allocator<unsigned_short>>::__tree", tree_edges, tree_case),
    T_("std::allocator<std::__tree<unsigned_short,std::less<unsigned_short>,std::allocator<unsigned_short>>::node>::deallocate",
       edges_of(dealloc_case, 6, 66), dealloc_case),
    T_("BogStatusPanel::Remove", edges_of(lambda r: panel_case(r, "bog"), 4, 77), lambda r: panel_case(r, "bog")),
    T_("FeatureStatusPanel::InitViewWindow", edges_of(lambda r: panel_case(r, "feature"), 4, 88), lambda r: panel_case(r, "feature")),
]
