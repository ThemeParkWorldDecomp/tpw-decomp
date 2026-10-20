"""Unit v51058, twenty small functions of the main app ("SimTheme Park"), against libtpw_unit_ShowWorldLobbyController.dylib
(tests/unit_ShowWorldLobbyController.cpp), src/std_less_std_basic_string_operator_ShowWorldLobbyController.cpp:
  Show/Hide/RemoveWorldLobbyController, __sinit_ui_worldLobbyController_cpp, UI_2DMap_CallbackFn,
  FindParkOnlineParkInfoPanelCancelCallback, the Argument deque iterator +=, the wstring operator<, std::operator!=,
  __sinit_TPGlobals_cpp, the two BuddyInfo map value_compare operator(), __sinit_OnlinePerson_cpp, the const void* deque
  const_iterator +=, iterator --, operator[], ErrorMessageCallback, __sinit_ui_publishParkPanel_cpp, RemovePublishParkPanel,
  PostRequestUploadCallbackFunction.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses
(big-endian) for the original, and in the host library's memory (little-endian, 32-bit addressable) for the translation. The
callees outside the unit (UI_SendMessage, ClearAll, DialogBoxCallback, MapCallback, the nocase less, the wide string size, begin
and compare, the deque operator-=, the string and TPGlobals constructors, __register_global_object and the two virtual slots the
glue __ptr_glue reaches) are fakes on both sides that record their arguments and answer from the case's queues.

The world of a case: the globals the functions read, a screen block with the three screen pointers, a deque table (count, start,
block table) with a cursor (table, index, slot). What is compared: the answer, the globals written, the cursor words (the table as
a flag, the slot relative to the block table) and the event log. The code has no fused or float operation.
Not tested: operator+= with the count INT_MIN on the const void* deque (the original calls itself for ever).
"""
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ShowWorldLobbyController"
MASK = 0xFFFFFFFF

SHOW, HIDE, REMOVE, SINIT_W, MAP, FIND, ARGADD, LT, NE, SINIT_TP = 0x1022b718, 0x1022b780, 0x1022b6ac, 0x1022bacc, 0x1022bf74, 0x1022a9a8, 0x10229498, 0x10225e8c, 0x102255a0, 0x1023331c
VC_PK, VC_KP, SINIT_P, CADD, DEC, INDEX, ERRCB, SINIT_PUB, REMOVE_PUB, POSTREQ = 0x1021f000, 0x1021eed0, 0x10213548, 0x101f5508, 0x101f4c84, 0x101f2004, 0x101f1c54, 0x101ecfe8, 0x101ecf6c, 0x101ec9b8
SIZES = {SHOW: 56, HIDE: 56, REMOVE: 56, SINIT_W: 64, MAP: 68, FIND: 56, ARGADD: 152, LT: 140, NE: 24, SINIT_TP: 52, VC_PK: 68, VC_KP: 68,
         SINIT_P: 28, CADD: 408, DEC: 64, INDEX: 40, ERRCB: 60, SINIT_PUB: 60, REMOVE_PUB: 76, POSTREQ: 56}
OWN = set(SIZES)

# ---- the callees faked on both sides
UI_SENDMESSAGE, CLEARALL, DIALOGBOX, MAPCALLBACK, NOCASE_LESS = 0x101c9518, 0x100a7128, 0x101f1520, 0x1022c7e8, 0x10225d2c
STR_SIZE, STR_BEGIN, STR_COMPARE, CURSOR_SUB, STR_CTOR, TPG_CTOR, REGISTER, PTR_GLUE = 0x1022605c, 0x10225ff0, 0x101d5864, 0x101f5bdc, 0x10238800, 0x102328bc, 0x10239940, 0x1023baa8

ADDR = dict(sat=0x10396fec, screens=0x102e49b8, panel=0x10396278, errdlg=0x1039647c, map=0x1039742c, wtext=0x10396ff0, wnode=0x10397004,
            ptext=0x1039639c, pnode=0x103963b0, tpg=0x103b74c8, tpvec=0x10268a08, tpnode=0x103bc158, p20=0x10396d20, p24=0x10396d24,
            dtor=0x103bcbb8)
MAC_OFF = [0xc, 0x10, 0x14]                  # download globe, publish globe, advisor in the screen block
HOST_LAYOUT = [0x18, 0x20, 0x28]
LABELS = dict(map=20, wtext=21, wnode=22, ptext=23, pnode=24, tpg=25, tpvec=26, tpnode=27, dtor=28)
NOBJ = 10                                    # 0 window, 1 advisor, 2 globe A, 3 globe B, 4 panel, 5 functor, 6 other (no label), 7 cursor, 8 table, 9 blocks
OBJ_LABEL = {0: 1, 1: 2, 2: 3, 3: 4, 4: 5, 5: 6, 7: 8}

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def glue(e):
    L = e.cl
    L.rec(L.tvmap.get(e.r(12), "UnknownSlot"), (L.lab(e.r(3)),))


STUBS.internal(UI_SENDMESSAGE, CL.recorder("UI_SendMessage", "Liii", ret="i"))
STUBS.internal(CLEARALL, CL.recorder("ClearAll", "L"))
STUBS.internal(DIALOGBOX, CL.recorder("DialogBoxCallback", "Liii", ret="i"))
STUBS.internal(MAPCALLBACK, CL.recorder("MapCallback", "LLiii", ret="i"))
STUBS.internal(NOCASE_LESS, CL.recorder("NocaseWStringLess", "LL", ret="b"))
STUBS.internal(STR_SIZE, CL.recorder("Size", "L", ret="i"))
STUBS.internal(STR_BEGIN, CL.recorder("Begin", "L", ret="i"))
STUBS.internal(STR_COMPARE, CL.recorder("Compare", "Luuuu", ret="i"))
STUBS.internal(CURSOR_SUB, CL.recorder("CursorSub", "Li", ret=lambda e, log: e.r(3)))
def via_glue(fn):
    """An import reached through glue: the glue stores the caller's TOC pointer at 0x14(r1) (the caller reloads it afterwards)."""
    def run(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return run


STUBS.internal(STR_CTOR, via_glue(CL.recorder("StringCtor", "LS", ret=lambda e, log: e.r(3))))
STUBS.internal(TPG_CTOR, CL.recorder("TPGlobals", "L", ret=lambda e, log: e.r(3)))
STUBS.internal(REGISTER, via_glue(CL.recorder("RegisterGlobal", "LLL")))
STUBS.internal(PTR_GLUE, via_glue(glue))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def make_objects(self):
        self.o = [self.alloc(0x400) for _ in range(NOBJ)]
        self.T = self.o[8]
        self.blk = self.o[9]

    def ov(self, i):
        return 0 if i is None else self.o[i]

    def ptrs(self, a, vs):
        """Native pointers (the Mac's words, the host's eight bytes: its arena sits above 4 GB)."""
        self.write(a, b"".join(struct.pack(self.pfmt, v) for v in vs))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def canon(self, v):
        for i, a in enumerate(self.o):
            if a == v:
                return i
        return 0 if v == 0 else -1


class Ppc(Side):
    fmt = pfmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        fixed = {self.o[i]: n for i, n in OBJ_LABEL.items()}
        fixed.update({ADDR[k]: n for k, n in LABELS.items()})
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        vt = e.alloc(0x100)
        tva, tvb = e.alloc(8), e.alloc(8)
        e.write(vt + 0x4c, struct.pack(">I", tva))
        e.write(vt + 0x34, struct.pack(">I", tvb))
        for i in range(7):
            e.write(self.o[i], struct.pack(">I", vt))
        self.L.tvmap = {tva: "DownloadGlobeSlot4c", tvb: "PublishGlobeSlot34"}
        self.off = MAC_OFF
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

    def log(self):
        return self.L.log

    def call(self, addr, args):
        return T.call_ppc(self.e, addr, [a & MASK for a in args])[0] & MASK


class Host(Side):
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.make_objects()
        fixed = {n: self.o[i] for i, n in OBJ_LABEL.items()}
        fixed.update({n: self.g(k) for k, n in LABELS.items()})
        lib.tpw_wlc_sethigh(self.o[0] >> 32)       # the high half of every Mac-style 32-bit address the library reads
        CL.host_begin(lib, fixed, case["queues"])
        self.off = lib.layout

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_wlc_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_wlc_alloc(n)

    def read(self, a, n):
        import ctypes as C
        return C.string_at(a, n)

    def write(self, a, data):
        import ctypes as C
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, addr, args):
        fn = getattr(self.lib, "tpw_wlc_" + SYM[addr])
        return fn(*(list(args) + [0] * (5 - len(args)))) & 0xFFFFFFFFFFFFFFFF


SYM = {SHOW: "Show", HIDE: "Hide", REMOVE: "Remove", SINIT_W: "SinitWorld", MAP: "Map", FIND: "Find", ARGADD: "ArgIter", LT: "Lt", NE: "Ne",
       SINIT_TP: "SinitTP", VC_PK: "VcPK", VC_KP: "VcKP", SINIT_P: "SinitPerson", CADD: "CursorAdd", DEC: "Dec", INDEX: "Index",
       ERRCB: "ErrCb", SINIT_PUB: "SinitPub", REMOVE_PUB: "RemovePub", POSTREQ: "PostReq"}


def bind(lib):
    import ctypes as C
    if getattr(lib, "bound", False):
        return
    lib.tpw_wlc_addr.restype, lib.tpw_wlc_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_wlc_alloc.restype, lib.tpw_wlc_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_wlc_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_wlc_sethigh.argtypes = [C.c_uint64]
    for name in SYM.values():
        f = getattr(lib, "tpw_wlc_" + name)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64] * 5
    out = (C.c_int64 * 32)()
    n = lib.tpw_wlc_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.layout = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

KIND = {SHOW: "void", HIDE: "void", REMOVE: "void", SINIT_W: "void", MAP: "int", FIND: "void", ARGADD: "argadd", LT: "bool", NE: "bool",
        SINIT_TP: "void", VC_PK: "bool", VC_KP: "bool", SINIT_P: "void", CADD: "void", DEC: "void", INDEX: "index", ERRCB: "int",
        SINIT_PUB: "void", REMOVE_PUB: "void", POSTREQ: "void"}


def build(s, c):
    s.ptrs(s.g("sat"), [s.ov(c["sat"])])
    s.ptrs(s.g("panel"), [s.ov(c["panel"])])
    s.ptrs(s.g("errdlg"), [s.ov(c["errdlg"])])
    s.words(s.g("p20"), [c["p20"]])
    s.words(s.g("p24"), [c["p24"]])
    scr = s.alloc(0x100)
    s.ptrs(s.g("screens"), [scr])
    for off, i in zip(s.off, c["screens"]):
        s.ptrs(scr + off, [s.ov(i)])
    cap, w4, start = c["tbl"]
    s.words(s.T, [cap, w4, start, s.blk])
    cur = s.o[7]
    s.words(cur, [s.T, c["idx"], s.blk + 4 * c["slot"]])
    s.words(s.o[0] + 0xc, [c["wa"]])
    s.words(s.o[1] + 0xc, [c["wb"]])


def execute(s, c):
    op = c["op"]
    k = KIND[op]
    if op in (MAP, ERRCB):
        a = [s.ov(c["w"]), c["msg"], c["a"], c["b"]]
    elif op == LT:
        a = [s.ov(c["x"]), s.ov(c["y"])]
    elif op in (VC_PK, VC_KP):
        a = [s.ov(c["x"]), s.ov(c["y"]), s.ov(c["z"])]
    elif op == NE:
        a = [s.o[0], s.o[1]]
    elif op in (ARGADD, CADD):
        a = [s.o[7], c["n"]]
    elif op == DEC:
        a = [s.o[7]]
    elif op == INDEX:
        a = [s.T, c["n"]]
    elif op == POSTREQ:
        a = [c["a"], s.ov(c["x"])]
    else:
        a = []
    r = s.call(op, a)
    if k == "argadd":
        return int((r & MASK) == (s.o[7] & MASK))
    if k == "index":
        return (r - s.blk) & MASK
    if k == "bool":
        return r & 0xFF
    if k == "int":
        return r & MASK
    return 0


def observe(s, c, answer):
    cur = s.rd_words(s.o[7], 3)
    return {"answer": answer, "log": s.log(), "sat": s.canon(s.rd_ptr(s.g("sat"))), "panel": s.canon(s.rd_ptr(s.g("panel"))),
            "errdlg": s.canon(s.rd_ptr(s.g("errdlg"))), "p20": s.rd_word(s.g("p20")), "p24": s.rd_word(s.g("p24")),
            "cur": [int(cur[0] == (s.T & MASK)), cur[1], (cur[2] - s.blk) & MASK], "tbl": s.rd_words(s.T, 4)[:3],
            "tblblk": int(s.rd_word(s.T + 12) == (s.blk & MASK)), "texts": s.rd_words(s.g("wtext"), 12) if False else 0}


def run(s, c):
    try:
        build(s, c)
        return observe(s, c, execute(s, c))
    finally:
        s.done()


def ppc(e, c):
    return run(Ppc(e, c), c)


def host(lib, c):
    return run(Host(lib, c), c)


# ---------------------------------------------------------------------------------------------------------------- case generators

PICK = [None, 0, 1, 2, 3, 4, 5, 6]
WORDS = [0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x1234, 0x10000]


def base(r, op):
    return {"op": op,
            "queues": {"UI_SendMessage": [r.randrange(-3, 4)], "DialogBoxCallback": [r.choice([0, 1, -1, r.randrange(-1000, 1000)])],
                       "MapCallback": [r.choice([0, 1, -1, r.randrange(-1000, 1000)])], "NocaseWStringLess": [r.choice([0, 1])],
                       "Size": [r.randrange(0, 50), r.randrange(0, 50)], "Begin": [r.choice([0, 0x1000, r.getrandbits(32)])],
                       "Compare": [r.choice([-1, 0, 1, -5, 5, s32(0x80000000), 0x7FFFFFFF, r.randrange(-100, 100)])]},
            "sat": r.choice(PICK), "panel": r.choice(PICK), "errdlg": r.choice(PICK), "p20": r.choice(WORDS + [r.getrandbits(32)]),
            "p24": r.choice(WORDS + [r.getrandbits(32)]), "screens": [r.choice(PICK[1:]), r.choice(PICK[1:]), r.choice(PICK)],
            "tbl": [4, 0, 0], "idx": 0, "slot": 0, "wa": r.choice(WORDS + [r.getrandbits(32)]), "wb": r.choice(WORDS + [r.getrandbits(32)]),
            "w": r.choice(PICK), "msg": r.choice([0, 1, 0x14, 0x15, r.randrange(0, 40)]), "a": r.choice([0, 1, -1, r.randrange(-99, 99)]),
            "b": r.choice([0, 1, -1, r.randrange(-99, 99)]), "x": r.choice(PICK), "y": r.choice(PICK), "z": r.choice(PICK), "n": 0}


def deque_world(r, c, n=None, avoid_min=True):
    cap = r.choice([1, 2, 3, 4, 8, 16, r.randrange(1, 40)])
    start = r.choice([0, 1, cap - 1, cap, r.randrange(0, cap + 2)])
    idx = r.choice([0, 1, cap - 1, cap, r.randrange(0, cap + 3)])
    t = start + idx
    slot = r.choice([0, 1, cap, r.randrange(-2, cap + 3)])
    c["tbl"] = [cap, r.getrandbits(32) if r.random() < 0.2 else cap * 3, start]
    c["idx"], c["slot"] = idx, slot
    if n is None:
        n = r.choice([0, 1, 2, -1, -2, cap - t + r.choice([-1, 0, 1]), -t + r.choice([-1, 0, 1]), r.randrange(-3 * cap, 3 * cap),
                      r.randrange(-3 * cap, 3 * cap), 0x7FFFFFFF, -0x7FFFFFFF, 0x7FFFFFF0, cap, -cap, t, -t - 1, cap - t, cap - t - 1])
    if avoid_min and n == -0x80000000:
        n = 0
    c["n"] = n
    return c


def gen(op, fill, seed, edges):
    def e():
        r = random.Random(seed)
        for k in range(edges):
            yield fill(r, base(r, op), k)

    def rnd(r):
        return fill(r, base(r, op), r.randrange(1000))
    return e, rnd


def f_plain(r, c, k):
    c["sat"] = [None, 0, 6][k % 3] if k < 3 else c["sat"]
    return c


def f_map(r, c, k):
    c["w"] = PICK[k % len(PICK)]
    c["msg"] = [0x14, 0x14, 0x13, 0, 0x15, 1][k % 6]
    if c["msg"] == 0x14:
        c["errdlg"] = PICK[(k // 2) % len(PICK)]
    return c


def f_lt(r, c, k):
    c["x"], c["y"] = PICK[k % len(PICK)], PICK[(k + 3) % len(PICK)]
    return c


def f_vc(r, c, k):
    c["x"], c["y"], c["z"] = PICK[k % len(PICK)], PICK[(k + 1) % len(PICK)], PICK[(k + 5) % len(PICK)]
    return c


def f_ne(r, c, k):
    c["wa"] = WORDS[k % len(WORDS)]
    c["wb"] = [c["wa"], WORDS[(k + 1) % len(WORDS)], c["wa"] ^ 1, c["wa"] ^ 0x80000000][k % 4]
    return c


def f_cadd(r, c, k):
    return deque_world(r, c, None, True)


def f_argadd(r, c, k):
    n = [-0x80000000, -1, 1, 0, 5, -5][k] if k < 6 else None
    return deque_world(r, c, n, False)


def f_dec(r, c, k):
    deque_world(r, c, 0)
    c["slot"] = [0, 1, 0, 2, c["tbl"][0]][k % 5]
    return c


def f_index(r, c, k):
    deque_world(r, c, 0)
    cap, _, start = c["tbl"]
    c["n"] = r.choice([0, 1, cap - start, cap - start - 1, cap - start + 1, cap, cap - 1, 2 * cap, r.randrange(0, 3 * cap + 1), 0xFFFFFFFF])
    return c


def f_post(r, c, k):
    c["a"] = r.choice([0, 1, -1, 5])
    c["x"] = r.choice(PICK)
    return c


TESTS = []


def add(name, op, fill, seed, edges):
    e, rnd = gen(op, fill, seed, edges)
    TESTS.append(FnTest(name, ppc, host, edge=e, rnd=rnd))


add("ShowWorldLobbyController", SHOW, f_plain, 1, 9)
add("HideWorldLobbyController", HIDE, f_plain, 2, 9)
add("RemoveWorldLobbyController", REMOVE, f_plain, 3, 9)
add("__sinit_ui_worldLobbyController_cpp", SINIT_W, f_plain, 4, 4)
add("UI_2DMap_CallbackFn", MAP, f_map, 5, 24)
add("FindParkOnlineParkInfoPanelCancelCallback", FIND, f_plain, 6, 9)
add("std::__cdeque<std::vector<Argument,std::allocator<Argument>>*,std::allocator<std::vector<Argument,std::allocator<Argument>>*>>::iterator::operator+=",
    ARGADD, f_argadd, 7, 40)
add("__lt<w,Q23std14char_traits<w>,Q23std12allocator<w>>__3stdFRCQ23std59basic_string<w,Q23std14char_traits<w>,Q23std12allocator<w>>RCQ23std59basic_string<w,Q23std14char_traits<w>,Q23std12allocator<w>>",
    LT, f_lt, 8, 16)
add("std::operator!=", NE, f_ne, 9, 16)
add("__sinit_TPGlobals_cpp", SINIT_TP, f_plain, 10, 4)
add("std::map<nocase_string,BuddyInfo,std::less<nocase_string>,std::allocator<std::pair<const_nocase_string,BuddyInfo>>>::value_compare::operator()@0x1021f000",
    VC_PK, f_vc, 11, 16)
add("std::map<nocase_string,BuddyInfo,std::less<nocase_string>,std::allocator<std::pair<const_nocase_string,BuddyInfo>>>::value_compare::operator()@0x1021eed0",
    VC_KP, f_vc, 12, 16)
add("__sinit_OnlinePerson_cpp", SINIT_P, f_plain, 13, 4)
add("std::__cdeque<const_void*,std::allocator<const_void*>>::const_iterator::operator+=", CADD, f_cadd, 14, 60)
add("std::__cdeque<const_void*,std::allocator<const_void*>>::iterator::operator--", DEC, f_dec, 15, 30)
add("std::__cdeque<const_void*,std::allocator<const_void*>>::operator[]", INDEX, f_index, 16, 40)
add("ErrorMessageCallback", ERRCB, f_map, 17, 24)
add("__sinit_ui_publishParkPanel_cpp", SINIT_PUB, f_plain, 18, 4)
add("RemovePublishParkPanel", REMOVE_PUB, f_plain, 19, 16)
add("PostRequestUploadCallbackFunction", POSTREQ, f_post, 20, 16)
