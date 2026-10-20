"""Unit v5183, the load, save and hire panels, against libtpw_unit_LoadPanelMsg.dylib (tests/unit_LoadPanelMsg.cpp), main app
("SimTheme Park"), src/LoadPanelMsg.cpp:
  LoadPanelMsg 0x101a2bdc, UI_HirePanelStateChange 0x101a2a14, SavePanelMsg 0x101a2df4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit (the panel removers,
the key-capture calls, Unpause, GetWindow, the list control, the staff database, the save file list, Load, Save, the default callback,
UI_PostMessage, UI_DialogBox, the string class, FormatString, wcscmp and the window's SetText virtual) is a fake on both sides that records
its arguments (strings as a hash of their characters, objects as labels) and answers from the case's queues.

The world of a case: the panel flags, gGame's word at 0x3c, the hire panel's window and type, the staff block, the save file list (names),
the name box text, the answers of the fakes. What is compared: the answer, the three flags, gGame's word and the event log.

No fused operations. Not tested: a count that runs past the save file list (the original dereferences NULL), a missing second staff answer
after the first (the original dereferences NULL).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_LoadPanelMsg"
MASK = 0xFFFFFFFF

FN_LOAD, FN_HIRE, FN_SAVE = 0x101A2BDC, 0x101A2A14, 0x101A2DF4
SIZES = {FN_LOAD: 352, FN_HIRE: 408, FN_SAVE: 808}
OWN = set(SIZES)

REMOVE_LOAD, REMOVE_SAVE = 0x101A3934, 0x101A3BE4
DECIDE_KB, ENABLE_KEYS = 0x1018116C, 0x10182304
UNPAUSE, GET_WINDOW, GET_DATA = 0x101499D0, 0x101DBE40, 0x101D41F0
GET_FIRST, GET_NEXT, LOAD, SAVE = 0x101FC97C, 0x101FCD08, 0x101FC408, 0x101FC4F4
DEFAULT_CB, FIND_DATA, GET_STAFF = 0x101F1448, 0x101D4384, 0x101292F8
REMOVE_ITEM, GET_NAME, GET_SALARY = 0x101D39F0, 0x10128768, 0x10128E1C
ADD_ITEM, UPDATE_DISPLAY, PTR_GLUE = 0x101D3000, 0x101D2220, 0x1023BAA8
WCSCMP, FORMAT_STRING, STR_CTOR, STR_DTOR = 0x10238C98, 0x10239730, 0x10238800, 0x10238710
DIALOG_BOX, POST_MESSAGE = 0x101F1800, 0x101C9400

ROOT, WORLD, TBL = 0x20001000, 0x20002000, 0x20003000
SAVE_CB_TVECTOR = 0x10267BE8
ADDR = dict(world=0x1037EBFC, game=0x1037F134, game3c=0x1037F134 + 0x3C, loadflag=0x10394974, saveflag=0x10394970,
            hirewin=0x10394924, hiretype=0x10394920, name=0x1039492C, tbl=0x1037FF18 + 0x564, loadsave=0x10396A7C)
HOST_LAYOUT = [0]

STATE = {}


def s32(v):
    return T.s32(v)


def u16s(e, a):
    if a == 0:
        return None
    out = []
    for i in range(200):
        c = struct.unpack(">H", e.read(a + 2 * i, 2))[0]
        if c == 0:
            break
        out.append(c)
    return out


def hsh(chars):
    if chars is None:
        return MASK
    h = 0
    for c in chars:
        h = (h * 31 + c) & MASK
    return h


def hstr(e, a):
    return hsh(u16s(e, a))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def plain(addr, name, nargs_labels=0):
    def fn(e):
        L = e.cl
        L.rec(name, tuple(L.lab(e.r(3)) for _ in range(nargs_labels)))
    STUBS.internal(addr, fn)


plain(REMOVE_LOAD, "RemoveLoadPanel")
plain(REMOVE_SAVE, "RemoveSavePanel")
plain(DECIDE_KB, "DecideKbCapture")
plain(ENABLE_KEYS, "EnableAllKeyTables")
plain(UNPAUSE, "Unpause", 1)
plain(UPDATE_DISPLAY, "UpdateDisplay", 1)


def s_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, STATE["win"] if L.ret("GetWindow") else 0)


def s_getdata(e):
    L = e.cl
    L.rec("GetData", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">i", s32(L.ret("GetData"))))
    e.set_r(3, 0)


def s_finddata(e):
    L = e.cl
    L.rec("FindData", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("FindData") & MASK)


def s_removeitem(e):
    L = e.cl
    L.rec("RemoveItem", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, 0)


def s_additem(e):
    L = e.cl
    item = e.r(4)
    L.rec("AddItem", (L.lab(e.r(3)), hstr(e, e.u32(item)), s32(e.u32(item + 4)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))
    e.set_r(3, 0)


def s_getstaff(e):
    L = e.cl
    L.rec("GetStaff", (L.lab(e.r(3)), e.r(4) & MASK))
    e.set_r(3, STATE["staff"] if L.ret("GetStaff") else 0)


def s_getname(e):
    L = e.cl
    L.rec("GetName", (L.lab(e.r(3)), s32(e.r(5))))
    e.write(e.r(4), struct.pack(">3H", 0x41 + (L.ret("GetName") & 15), 0x42, 0))


def s_getsalary(e):
    L = e.cl
    L.rec("GetSalary", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("GetSalary") & MASK)


def s_first(e):
    L = e.cl
    L.rec("GetFirst", (L.lab(e.r(3)),))
    d = STATE["dets"]
    e.set_r(3, d[0] if d else 0)


def s_next(e):
    L = e.cl
    d = STATE["dets"]
    p = e.r(4)
    i = d.index(p) if p in d else -1
    L.rec("GetNext", (L.lab(e.r(3)), i))
    e.set_r(3, d[i + 1] if 0 <= i < len(d) - 1 else 0)


def s_load(e):
    L = e.cl
    d = STATE["dets"]
    p = e.r(4)
    i = d.index(p) if p in d else -1
    L.rec("Load", (L.lab(e.r(3)), i, hstr(e, e.u32(p + 8))))
    e.set_r(3, 0)


def s_save(e):
    L = e.cl
    L.rec("Save", (L.lab(e.r(3)), hstr(e, e.u32(e.r(4) + 8))))


def s_default(e):
    L = e.cl
    L.rec("Default", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("Default") & MASK)


def s_post(e):
    L = e.cl
    L.rec("PostMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def s_dialog(e):
    L = e.cl
    p = e.r(3)
    rgb = e.read(p + 4, 3)
    L.rec("DialogBox", (hstr(e, e.u32(p)), rgb[0], rgb[1], rgb[2], s32(e.u32(p + 8)), s32(e.r(4)), int(e.r(5) != 0), s32(e.r(6)),
                        int(e.r(7) == SAVE_CB_TVECTOR), s32(e.r(8)), int(e.r(9) != 0), e.r(10) & 0xFF))
    e.set_r(3, 0)


def s_wcscmp(e):
    L = e.cl
    a, b = u16s(e, e.r(3)), u16s(e, e.r(4))
    L.rec("wcscmp", (hsh(a), hsh(b)))
    e.set_r(3, (0 if a == b else (1 if a > b else MASK)))


def s_format(e):
    L = e.cl
    t = e.r(7)
    L.rec("FormatString", (L.lab(e.r(3)), e.r(5) & MASK, e.r(6) & MASK, e.u32(t), e.u32(t + 8), hstr(e, e.u32(t + 12))))
    e.write(e.r(4), struct.pack(">3H", 0x41 + (L.ret("fmt") & 15), 0x42, 0))
    e.set_r(3, 0)


def s_ctor(e):
    L = e.cl
    L.rec("StrCtor", (hstr(e, e.r(4)),))
    e.write(e.r(3) + 8, struct.pack(">I", e.r(4)))


def s_dtor(e):
    e.cl.rec("StrDtor", (s32(e.r(4)),))


def s_ptrglue(e):
    L = e.cl
    L.rec("SetText", (L.lab(e.r(3)), int(e.r(12) == STATE["tv"]), hstr(e, e.r(4))))


for _a, _f in ((GET_WINDOW, s_getwindow), (GET_DATA, s_getdata), (FIND_DATA, s_finddata), (REMOVE_ITEM, s_removeitem),
               (ADD_ITEM, s_additem), (GET_STAFF, s_getstaff), (GET_NAME, s_getname), (GET_SALARY, s_getsalary),
               (GET_FIRST, s_first), (GET_NEXT, s_next), (LOAD, s_load), (SAVE, s_save), (DEFAULT_CB, s_default),
               (POST_MESSAGE, s_post), (DIALOG_BOX, s_dialog), (WCSCMP, s_wcscmp), (FORMAT_STRING, s_format),
               (STR_CTOR, s_ctor), (STR_DTOR, s_dtor), (PTR_GLUE, s_ptrglue)):
    STUBS.internal(_a, _f)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    win = T.persist_alloc(e, 0x200)
    vt = T.persist_alloc(e, 0x100)
    tv = T.persist_alloc(e, 0x10)
    STATE.update(win=win, vt=vt, tv=tv, staff=T.persist_alloc(e, 0x20))
    e.write(win + 0x118, struct.pack(">I", vt))
    e.write(vt + 0x2C, struct.pack(">I", tv))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def text(self, t):
        n = len(t)
        a = self.alloc(2 * n + 2)
        self.write(a, struct.pack(self.fmt[0] + "%dH" % (n + 1), *([ord(c) for c in t] + [0])))
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ROOT: 1, STATE["win"]: 2, STATE["staff"]: 3, WORLD: 4, ADDR["loadsave"]: 5, ADDR["game"]: 6,
                                   TBL: 7}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name] if name in ADDR else STATE[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def dets(self, names):
        STATE["dets"] = []
        for t in names:
            obj = self.alloc(16)
            self.words(obj + 8, [self.text(t)])
            STATE["dets"].append(obj)

    def call(self, op, args):
        fn = {"load": FN_LOAD, "save": FN_SAVE, "hire": FN_HIRE}[op]
        r3, _, _ = T.call_ppc(self.e, fn, [ROOT & MASK] + [a & MASK for a in args] if op != "hire" else [a & MASK for a in args])
        return 0 if op == "hire" else r3 & MASK   # UI_HirePanelStateChange is void


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: ROOT, 2: self.g("win"), 3: self.g("staff"), 4: WORLD, 5: self.g("loadsave"), 6: self.g("game"), 7: TBL},
                      case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_lp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_lp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def dets(self, names):
        self.lib.tpw_lp_ndet(len(names))
        for i, t in enumerate(names):
            self.lib.tpw_lp_setdetail(i, self.text(t))

    def call(self, op, args):
        if op == "hire":
            self.lib.tpw_lp_hire(s32(args[0]))
            return 0
        f = self.lib.tpw_lp_load if op == "load" else self.lib.tpw_lp_save
        return f(ROOT, s32(args[0]), s32(args[1]), s32(args[2])) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_lp_addr.restype, lib.tpw_lp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_lp_alloc.restype, lib.tpw_lp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_lp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_lp_ndet.argtypes = [C.c_int32]
    lib.tpw_lp_setdetail.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_lp_hire.argtypes = [C.c_int32]
    for n in ("load", "save"):
        f = getattr(lib, "tpw_lp_" + n)
        f.restype, f.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_lp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.words(s.g("loadflag"), [case["loadflag"]])
    s.words(s.g("saveflag"), [case["saveflag"]])
    s.words(s.g("game3c"), [case["g3c"]])
    s.words(s.g("world"), [WORLD])
    s.words(s.g("tbl"), [TBL])
    s.words(s.g("hirewin"), [case["hirewin"]])
    s.words(s.g("hiretype"), [case["hiretype"]])
    s.words(s.g("staff"), [case["stafftype"], 0, 0, 0])
    codes = [ord(c) for c in case["gname"]]
    s.write(s.g("name"), struct.pack(s.fmt[0] + "34H", *(codes + [0] * (34 - len(codes)))))
    s.dets(case["names"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "loadflag": s.rd_word(s.g("loadflag")), "saveflag": s.rd_word(s.g("saveflag")),
            "g3c": s.rd_word(s.g("game3c"))}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"], case["args"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

STRS = ["a", "b", "ab", "", "ba", "abc"]


def base(r, op, args):
    names = [r.choice(STRS) for _ in range(r.randrange(1, 7))]
    gname = r.choice(STRS)
    if r.random() < 0.4:
        names[r.randrange(len(names))] = gname
    q = {"GetWindow": [r.choice([1, 1, 0]), 1], "Default": [r.randrange(-3, 5)], "GetName": [r.randrange(0, 40)],
         "GetSalary": [r.randrange(0, 1000)], "fmt": [r.randrange(0, 40)], "FindData": [r.choice([-1, -7, 0, 1, 5])]}
    st = r.choice([1, 1, 0])
    q["GetStaff"] = [st, 1 if st else r.choice([0, 1])]
    q["GetStaff"] = [st, 1] if st else [0, 0]
    return {"op": op, "args": args, "queues": q, "names": names, "gname": gname, "loadflag": r.choice([0, 7]), "saveflag": r.choice([0, 9]),
            "g3c": r.choice([0, 5]), "hirewin": r.choice([0, ROOT]), "hiretype": r.choice([1, 2, -1]),
            "stafftype": r.choice([1, 2, 0xFFFFFFFF])}


def list_count(r, c, lo):
    c["queues"]["GetData"] = [r.randrange(lo, len(c["names"]))]
    return c


def load_case(r, m=None):
    m = r.choice([5, 0x14, 0x100, 0x400, 0x400, 0x400, 0x401, 0x13, 0x15, 0x99, 0x101, 0]) if m is None else m
    a = r.choice([-2, -1, 0, 1, 0x23, 0x100]) if m != 0x400 else r.choice([5, 0x23bce2, -1])
    b = r.choice([-1, 0, 3, 7])
    return list_count(r, base(r, "load", [m, a, b]), -1)


def save_case(r, m=None):
    m = r.choice([5, 0x14, 0x100, 0x100, 0x100, 0x400, 0x400, 0x803, 0x802, 0x801, 0x804, 0x805, 0x806, 0x401, 0x13, 0x15, 0x7FF, 0x99]) if m is None else m
    a = r.choice([-1, -1, -2, 0, -3, 5]) if m != 0x400 else r.choice([5, 0x23bce2])
    b = r.choice([-1, 0, 3])
    c = base(r, "save", [m, a, b])
    return list_count(r, c, 0)


def hire_case(r):
    c = base(r, "hire", [r.choice([0, 1, 5, 0x1f, 0xFFFFFFFF])])
    c["hiretype"] = r.choice([1, 2, -1])
    if r.random() < 0.5:
        c["hiretype"] = s32(c["stafftype"])
    c["hirewin"] = r.choice([ROOT, ROOT, ROOT, 0])
    return c


def load_edges():
    r = random.Random(11)
    for m, a, b in ((5, 0, 0), (0x14, 0, 0), (0x100, -2, 0), (0x100, -1, 0), (0x100, 0, 0), (0x400, 5, -1), (0x400, 5, 0), (0x400, 5, 3),
                    (0x401, 0, 0), (0x13, 0, 0), (0x15, 0, 0), (0x99, 0, 0), (0x101, 0, 0), (0, 0, 0), (4, 0, 0), (-1, 0, 0)):
        for gw in (0, 1):
            c = list_count(r, base(r, "load", [m, a, b]), -1)
            c["queues"]["GetWindow"] = [gw]
            yield c
    for count in (-1, 0, 1, 2):
        c = base(r, "load", [0x400, 5, 1])
        c["names"] = ["a", "b", "ab", "ba"]
        c["queues"]["GetWindow"] = [1]
        c["queues"]["GetData"] = [count]
        yield c


def save_edges():
    r = random.Random(12)
    for m in (5, 0x14, 0x13, 0x15, 0x100, 0x400, 0x401, 0x7FF, 0x801, 0x802, 0x803, 0x804, 0x805, 0x806, 0, 4):
        for a in (-1, -2, -3, 0):
            for gw in (0, 1):
                c = save_case(r, m)
                c["args"][1] = a if m == 0x100 else (5 if m == 0x400 else a)
                c["queues"]["GetWindow"] = [gw, 1]
                yield c
    for found in (0, 1):
        for gname in ("a", "", "ab"):
            c = base(r, "save", [0x100, -1, 0])
            c["gname"] = gname
            c["names"] = ["b", gname if found else "zz", "ba"]
            yield c
    for count in (0, 1, 2):
        for b in (-1, 4):
            c = base(r, "save", [0x400, 5, b])
            c["names"] = ["a", "b", "ab", "ba"]
            c["queues"]["GetWindow"] = [1, 1]
            c["queues"]["GetData"] = [count]
            yield c


def hire_edges():
    r = random.Random(13)
    for hw in (0, ROOT):
        for fd in (-1, -7, 0, 3):
            for gs in ([0, 0], [1, 1]):
                for eq in (0, 1):
                    c = hire_case(r)
                    c["hirewin"] = hw
                    c["queues"]["FindData"] = [fd]
                    c["queues"]["GetStaff"] = gs
                    c["hiretype"] = s32(c["stafftype"]) if eq else s32(c["stafftype"]) + 1
                    yield c
    c = hire_case(r)
    c["queues"]["GetWindow"] = [0]
    yield c


TESTS = [
    FnTest("LoadPanelMsg", ppc, host, edge=load_edges, rnd=load_case),
    FnTest("UI_HirePanelStateChange", ppc, host, edge=hire_edges, rnd=hire_case),
    FnTest("SavePanelMsg", ppc, host, edge=save_edges, rnd=save_case),
]
