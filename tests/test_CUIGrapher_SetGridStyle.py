"""Unit v51056: 23 small functions, against libtpw_unit_CUIGrapher_SetGridStyle.dylib (tests/unit_CUIGrapher_SetGridStyle.cpp), main app
("SimTheme Park"), src/game/TPEmbed.cpp, src/game/online/bthread.cpp, src/game/save/FS_LoadSave.cpp, src/game/ui/UIGrapher.cpp, src/game/ui/ui_mainmenu.cpp:
  CUIGrapher::SetXRange 0x101fd798, SetYRange 0x101fd7d8, SetDrawArea 0x101fd818, SetXAxisStyle 0x101fd880, SetYAxisStyle 0x101fd8d4,
  SetGridStyle 0x101fd928, SetBGStyle 0x101fd97c; CFSLoadSave::LoadHidden 0x101fc47c, Save 0x101fc4f4, SaveHidden 0x101fc568, ~CFSLoadSave
  0x101fc374; __sinit_ui_mainmenu_cpp 0x101fc260; MainLobbyMenuPostOnlineLoginCallbackFn 0x101fb310, MainLobbyMenuConfirmQuit_OK 0x101fb1f4,
  BankruptGameMenuConfirmRestart_OK 0x101fadb8, MainGameMenuConfirmRestart_OK 0x101fad4c; __sinit_TPEmbed_cpp 0x10200bf0, bthread_create
  0x10200c34, bthread_mutex::bthread_mutex 0x10200ca4, bthread_cond::signal 0x10200fb4, std::vector<Argument>::~vector 0x102010f8,
  operator_new 0x10201850, Connection::~Connection 0x10201ec0.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (SaveInternal, LoadInternal, the list destructor, operator delete, ~CSock, the vector tear_down, SendQuit, GS_SetCurrentGameStatus,
HideMainMenu, UI_ByeByeBankrupt, the wide string constructor, __register_global_object, InitializeCriticalSection, SetEvent,
_beginthreadex) are fakes on both sides that record their arguments (pointers as labels, the first word of the objects the real callee would
read) and answer from the case's queues.

The world of a case: a random object block (grapher words and colour bytes, list / connection / vector / mutex / event words), the globals
(gLobby, the TPEmbed words, the main menu string and node) filled with random words. What is compared: the answer (r3 where the function has
one, `this` as an equality), the words of the block, the globals and the event log.

No fused operations. Not tested: a NULL gLobby (the original dereferences it); the store of the table word 0x102b1540 at +0 of CFSLoadSave
by its destructor (the host object keeps its own vptr there, so word 0 is not compared; words 1 to 3 are); the data of the literal at
0x1024f694 (only passed on as a label).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CUIGrapher_SetGridStyle"
MASK = 0xFFFFFFFF
P = "tpw_v51056_"

# ---- the callees faked on both sides
SAVE_INTERNAL, LOAD_INTERNAL = 0x101FC790, 0x101FC5E0
TBTLIST_DTOR, OP_DELETE, CSOCK_DTOR, TEAR_DOWN = 0x101FD130, 0x10238218, 0x10201FCC, 0x10201310
SEND_QUIT, GS_SET, HIDE_MENU, BYE_BYE = 0x100A643C, 0x10236E6C, 0x101FC140, 0x10187220
WSTRING_CTOR, REGISTER, INIT_CS, SET_EVENT, BEGIN_THREAD = 0x10238800, 0x10239940, 0x10239130, 0x1023A858, 0x1023A870

GP = 0x103B74C8
ADDR = dict(gTPGlobals=GP, ext4420=GP + 0x4420, ext44ac=GP + 0x44AC, gLobby=0x102E4888, embedA=0x10396AA0, embedB=0x10396AA4,
            mmText=0x10396A5C, mmNode=0x10396A70, mmLit=0x1024F694, mmDtor=0x103BCBB8, threadFn=0x10200123)
LABELS = {"ext4420": 2, "ext44ac": 3, "mmText": 4, "threadFn": 5, "mmLit": 6, "mmDtor": 7, "mmNode": 8}
HOST_LAYOUT = [0x44, 0x2C, 0x30, 0x35, 0x3A, 0x3F, 0, 8]


def s32(v):
    return T.s32(v)


def w0(e, p):
    return e.u32(p) if p else 0


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()


def stub_save(e):
    L = e.cl
    L.rec("SaveInternal", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5)), w0(e, e.r(3)), w0(e, e.r(4))))
    e.set_r(3, L.ret("SaveInternal") & MASK)


def stub_load(e):
    L = e.cl
    L.rec("LoadInternal", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5)), w0(e, e.r(3)), w0(e, e.r(4))))
    e.set_r(3, L.ret("LoadInternal") & MASK)


def stub_list_dtor(e):
    e.cl.rec("TbTListDtor", (e.cl.lab(e.r(3)), s32(e.r(4))))


def stub_delete(e):
    e.cl.rec("Delete", (e.cl.lab(e.r(3)),))


def stub_csock(e):
    e.cl.rec("CSockDtor", (e.cl.lab(e.r(3)), s32(e.r(4))))


def stub_tear(e):
    e.cl.rec("TearDown", (e.cl.lab(e.r(3)),))


def stub_quit(e):
    e.cl.rec("SendQuit", (e.cl.lab(e.r(3)), w0(e, e.r(3))))


def stub_gs(e):
    e.cl.rec("GS_SetCurrentGameStatus", (s32(e.r(3)),))


def stub_hide(e):
    e.cl.rec("HideMainMenu", ())


def stub_byebye(e):
    e.cl.rec("UI_ByeByeBankrupt", ())


def stub_wctor(e):
    e.cl.rec("WStringCtor", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def stub_register(e):
    L = e.cl
    L.rec("Register", (L.lab(e.r(3)), L.lab(e.r(4)), L.lab(e.r(5))))


def stub_initcs(e):
    L = e.cl
    L.rec("InitCS", (L.lab(e.r(3)),))
    e.write(e.r(3), struct.pack(">I", L.ret("InitCS") & MASK))


def stub_setevent(e):
    L = e.cl
    L.rec("SetEvent", (e.r(3) & MASK,))
    e.set_r(3, L.ret("SetEvent") & MASK)


def stub_thread(e):
    L = e.cl
    L.rec("BeginThread", (e.r(3) & MASK, e.r(4) & MASK, L.lab(e.r(5)), L.lab(e.r(6)), e.r(7) & MASK, 1 if e.r(8) else 0))
    if e.r(8):
        e.write(e.r(8), struct.pack(">I", L.ret("ThreadId") & MASK))
    e.set_r(3, L.ret("ThreadRet") & MASK)


def glue(fn):
    """A glue import saves the caller's TOC pointer at 0x14(r1) (the caller reloads r2 from there after the call)."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


for _a, _f in ((SAVE_INTERNAL, stub_save), (LOAD_INTERNAL, stub_load), (TBTLIST_DTOR, stub_list_dtor), (OP_DELETE, glue(stub_delete)),
               (CSOCK_DTOR, stub_csock), (TEAR_DOWN, stub_tear), (SEND_QUIT, stub_quit), (GS_SET, stub_gs), (HIDE_MENU, stub_hide),
               (BYE_BYE, stub_byebye), (WSTRING_CTOR, glue(stub_wctor)), (REGISTER, glue(stub_register)), (INIT_CS, glue(stub_initcs)),
               (SET_EVENT, glue(stub_setevent)), (BEGIN_THREAD, glue(stub_thread))):
    STUBS.internal(_a, _f)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the operations
# name -> address, size, host function, host argument types, kind of world, argument builder (this, name block, abstract args) -> (ppc, host),
# answer kind. Abstract args: ints (a), or for the styles the five bytes.

U64, I32, U32 = C.c_uint64, C.c_int32, C.c_uint32
OPS = {}
SIZES = {}


def op(name, addr, size, cname, argtypes, restype, kind, args, ret):
    OPS[name] = dict(addr=addr, cname=cname, argtypes=argtypes, restype=restype, kind=kind, args=args, ret=ret)
    SIZES[addr] = size


def a3(t, n, a):
    return [t] + [v & MASK for v in a], [t] + [s32(v) for v in a]


def style(t, n, a):
    return [t, (a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3], a[4] << 24], [t] + list(a)


def tn(t, n, a):
    return [t, n], [t, n]


def flag(t, n, a):
    return [t, a[0] & MASK], [t, s32(a[0])]


def one(t, n, a):
    return [t], [t]


def none(t, n, a):
    return [], []


def ctx(t, n, a):
    return [a[0] & MASK], [a[0] & MASK]


op("CUIGrapher::SetXRange", 0x101FD798, 16, "x_range", [U64, I32, I32, I32], None, "grapher", a3, "none")
op("CUIGrapher::SetYRange", 0x101FD7D8, 16, "y_range", [U64, I32, I32, I32], None, "grapher", a3, "none")
op("CUIGrapher::SetDrawArea", 0x101FD818, 52, "draw_area", [U64, I32, I32, I32, I32], None, "grapher", a3, "none")
op("CUIGrapher::SetXAxisStyle", 0x101FD880, 28, "x_axis", [U64] + [U32] * 5, None, "grapher", style, "none")
op("CUIGrapher::SetYAxisStyle", 0x101FD8D4, 28, "y_axis", [U64] + [U32] * 5, None, "grapher", style, "none")
op("CUIGrapher::SetGridStyle", 0x101FD928, 28, "grid", [U64] + [U32] * 5, None, "grapher", style, "none")
op("CUIGrapher::SetBGStyle", 0x101FD97C, 28, "bg", [U64] + [U32] * 5, None, "grapher", style, "none")
op("CFSLoadSave::SaveHidden", 0x101FC568, 44, "save_hidden", [U64, U64], None, "fs", tn, "none")
op("CFSLoadSave::Save", 0x101FC4F4, 44, "save", [U64, U64], None, "fs", tn, "none")
op("CFSLoadSave::LoadHidden", 0x101FC47C, 44, "load_hidden", [U64, U64], None, "fs", tn, "none")
op("CFSLoadSave::~CFSLoadSave", 0x101FC374, 108, "fs_dtor", [U64, I32], U64, "fsdtor", flag, "this")
op("__sinit_ui_mainmenu_cpp", 0x101FC260, 60, "sinit_mainmenu", [], None, "mainmenu", none, "none")
op("MainLobbyMenuPostOnlineLoginCallbackFn", 0x101FB310, 4, "post_login", [I32, U64], None, "plain",
   lambda t, n, a: ([a[0] & MASK, a[1] & MASK], [s32(a[0]), a[1] & MASK]), "none")
op("MainLobbyMenuConfirmQuit_OK", 0x101FB1F4, 48, "quit_ok", [U64], I32, "lobby", ctx, "int")
op("BankruptGameMenuConfirmRestart_OK", 0x101FADB8, 60, "bankrupt_ok", [U64], I32, "plain", ctx, "int")
op("MainGameMenuConfirmRestart_OK", 0x101FAD4C, 52, "main_ok", [U64], I32, "plain", ctx, "int")
op("__sinit_TPEmbed_cpp", 0x10200BF0, 28, "sinit_embed", [], None, "embed", none, "none")
op("bthread_create", 0x10200C34, 64, "thread_create", [I32, U64], U32, "plain",
   lambda t, n, a: ([0x10200123 if a[0] else 0, a[1] & MASK], [a[0], a[1] & MASK]), "int")
op("bthread_mutex::bthread_mutex", 0x10200CA4, 52, "mutex_ctor", [U64], U64, "mutex", one, "this")
op("bthread_cond::signal", 0x10200FB4, 44, "cond_signal", [U64], I32, "cond", one, "int")
op("std::vector<Argument,std::allocator<Argument>>::~vector", 0x102010F8, 92, "vec_dtor", [U64, I32], None, "vec", flag, "none")
op("operator_new", 0x10201850, 8, "op_new", [U32, U64], U64, "plain",
   lambda t, n, a: ([a[0] & MASK, a[1] & MASK], [a[0] & MASK, a[1] & MASK]), "where")
op("Connection::~Connection", 0x10201EC0, 104, "conn_dtor", [U64, I32], U64, "conn", flag, "this")
STUBS.real.update(SIZES)

# kind -> (block size in bytes, words to fill, observed words from, count)
BLOCKS = {"grapher": (0x50, 12), "fs": (0x20, 4), "fsdtor": (0x20, 4), "conn": (0x40, 8), "vec": (0x10, 3), "mutex": (0x10, 4),
          "cond": (0x10, 1), "lobby": (0x10, 4)}


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {ADDR[n]: lab for n, lab in LABELS.items()}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def setptr(self, a, v):
        self.words(a, [v])

    def log(self):
        return self.L.log

    def call(self, name, args):
        r3, _, _ = T.call_ppc(self.e, OPS[name]["addr"], args)
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {lab: self.g(n) for n, lab in LABELS.items()}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return getattr(self.lib, P + "addr")(name.encode())

    def alloc(self, n):
        return getattr(self.lib, P + "alloc")(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def setptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, name, args):
        o = OPS[name]
        r = getattr(self.lib, P + o["cname"])(*args)
        return None if r is None else r


def bind(lib):
    if getattr(lib, "bound", False):
        return
    getattr(lib, P + "addr").restype, getattr(lib, P + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, P + "alloc").restype, getattr(lib, P + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, P + "layout").argtypes = [C.POINTER(C.c_int64)]
    for o in OPS.values():
        f = getattr(lib, P + o["cname"])
        f.argtypes, f.restype = o["argtypes"], o["restype"]
    out = (C.c_int64 * 32)()
    n = getattr(lib, P + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def run(s, c):
    try:
        o = OPS[c["op"]]
        kind = o["kind"]
        w, a = c["w"], c["a"]
        this = name = 0
        nwords = 0
        if kind in BLOCKS:
            size, nwords = BLOCKS[kind]
            blk = s.alloc(size)
            s.words(blk, w[:nwords])
            if kind == "grapher":
                s.write(blk + 0x30, bytes(c["bytes"]))
            this = 0 if c["null"] else blk
            if kind == "fs":
                name = s.alloc(0x20)
                s.words(name, w[4:8])
            if kind == "cond":
                s.words(blk, [c["handle"]])
            if kind == "lobby":
                s.setptr(s.g("gLobby"), blk)
        if kind == "embed":
            s.words(s.g("embedA"), [w[0]])
            s.words(s.g("embedB"), [w[1]])
        if kind == "mainmenu":
            s.words(s.g("mmText"), w[:5])
            s.words(s.g("mmNode"), w[5:8])
        ppc_args, host_args = o["args"](this, name, a)
        answer = s.call(c["op"], ppc_args if isinstance(s, Ppc) else host_args)
        if o["ret"] == "none":
            answer = None
        elif o["ret"] == "int":
            answer = answer & MASK
        elif o["ret"] == "this":
            answer = answer == this
        elif o["ret"] == "where":
            answer = answer == (a[1] & MASK)
        obs = {"answer": answer, "log": s.log()}
        if kind == "grapher":
            obs["w"] = s.rd_words(this, 12)
            obs["b"] = s.read(this + 0x30, 0x14)
        elif kind == "fsdtor":
            obs["w"] = s.rd_words(blk, 4)[1:]
        elif kind in ("fs", "conn", "vec", "mutex", "lobby"):
            obs["w"] = s.rd_words(blk, nwords)
        elif kind == "cond":
            obs["w"] = s.rd_words(this, 1)
        elif kind == "embed":
            obs["a"], obs["b"] = s.rd_words(s.g("embedA"), 1), s.rd_words(s.g("embedB"), 1)
        elif kind == "mainmenu":
            obs["t"], obs["n"] = s.rd_words(s.g("mmText"), 5), s.rd_words(s.g("mmNode"), 3)
        return obs
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

INTS = [0, 1, -1, 2, 0x7FFFFFFF, -0x80000000, 0x12345678, -0x12345678, 100, -100]
FLAGS = [-1, 0, 1, 2, 0x7FFF, -0x8000, 0x8000, 0x10000, 0x18000, 0x10001, 0xFFFF0001, 0x7FFF0000, 0xFFFFFFFF]
QKEYS = ["SaveInternal", "LoadInternal", "InitCS", "SetEvent", "ThreadId", "ThreadRet"]


def base(r, name):
    return {"op": name, "queues": {k: [r.getrandbits(32)] for k in QKEYS}, "w": [r.getrandbits(32) for _ in range(12)],
            "bytes": [r.getrandbits(8) for _ in range(0x14)], "null": False, "a": [], "handle": r.getrandbits(32)}


def rint(r):
    return r.choice(INTS) if r.random() < 0.4 else s32(r.getrandbits(32))


def rbyte(r):
    return r.choice([0, 1, 0x7F, 0x80, 0xFF]) if r.random() < 0.4 else r.getrandbits(8)


def gen_args(name, r, edge):
    """The abstract argument lists of a function: edge=True the fixed boundary ones, else one random one."""
    o = OPS[name]
    kind, cname = o["kind"], o["cname"]
    if cname in ("x_range", "y_range", "draw_area"):
        n = 4 if cname == "draw_area" else 3
        if edge:
            return [[v] * n for v in INTS] + [[INTS[(i + k) % len(INTS)] for k in range(n)] for i in range(len(INTS))]
        return [[rint(r) for _ in range(n)]]
    if kind == "grapher":
        if edge:
            return [[0] * 5, [255] * 5, [0x80, 1, 2, 3, 4], [1, 0, 0, 0, 0], [0, 0, 0, 0, 1], [0, 0, 0, 0xFF, 0], [0, 255, 0, 0, 0xFF]]
        return [[rbyte(r) for _ in range(5)]]
    if cname in ("fs_dtor", "vec_dtor", "conn_dtor"):
        if edge:
            return [[f] for f in FLAGS]
        return [[r.choice(FLAGS) if r.random() < 0.5 else r.getrandbits(32)]]
    if cname == "post_login":
        return [[0, 0], [-1, 1], [5, 0x12345678]] if edge else [[rint(r), r.getrandbits(32)]]
    if cname in ("quit_ok", "bankrupt_ok", "main_ok"):
        return [[0], [1], [0xFFFFFFFF]] if edge else [[r.getrandbits(32)]]
    if cname == "thread_create":
        return [[0, 0], [1, 0], [0, 7], [1, 7]] if edge else [[r.randrange(2), r.choice([0, r.getrandbits(32)])]]
    if cname == "op_new":
        return [[0, 0], [4, 0x12345678], [0xFFFFFFFF, 0xFFFFFFFF], [1, 0x80000000]] if edge else [[r.getrandbits(32), r.getrandbits(32)]]
    return [[]]


def cases(name, edge):
    r = random.Random(sum(map(ord, name)) + (0 if edge else 1000))
    for a in gen_args(name, r, edge):
        for null in ((False, True) if edge and OPS[name]["kind"] in ("fsdtor", "conn", "vec") else (False,)):
            c = base(r, name)
            c["a"], c["null"] = a, null
            yield c


def make_test(name):
    def edges():
        return cases(name, True)

    def rnd(r):
        c = next(cases(name, False)) if False else None
        c = base(r, name)
        c["a"] = gen_args(name, r, False)[0]
        c["null"] = OPS[name]["kind"] in ("fsdtor", "conn", "vec") and r.random() < 0.15
        return c

    return FnTest(name, ppc, host, edge=edges, rnd=rnd)


TESTS = [make_test(n) for n in OPS]
