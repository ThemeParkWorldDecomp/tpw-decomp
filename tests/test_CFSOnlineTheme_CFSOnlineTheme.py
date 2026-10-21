"""Unit v5670, CFSOnlineTheme's constructor and destructor, against libtpw_unit_CFSOnlineTheme_CFSOnlineTheme.dylib
(tests/unit_CFSOnlineTheme_CFSOnlineTheme.cpp), main app ("SimTheme Park"), src/game/online/FS_OnlineList.cpp:
  CFSOnlineTheme::CFSOnlineTheme 0x1017c57c, CFSOnlineTheme::~CFSOnlineTheme 0x1017c6e0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory (big-endian) for the original and
in the host library's memory for the translation. The string classes' constructors, destructors and operator=, UniStringToString and the
delete of the storage are fakes on both sides that record their arguments (objects as labels). The theme list (TbTList's AddTail, Find,
RemoveAt) runs for real on both sides; the list is filled through AddTail and read back node by node.

The world of a case: a theme owner with its list holding some other pointers (and, for the destructor, the theme once or twice at any
position or not at all), a ccThemeInfo with an id and three wide strings. What is compared: the event log, the theme's list pointer and id,
the vtable word (PowerPC side: the TOC value; the host's is the compiler's), the list's count and values. No fused operations. Not tested:
a NULL list owner or NULL info (the original reads them unconditionally).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineTheme_CFSOnlineTheme"
MASK = 0xFFFFFFFF

FN_CT, FN_DT = 0x1017C57C, 0x1017C6E0
SIZES = {FN_CT: 272, FN_DT: 196}
OWN = set(SIZES)
ADDTAIL = 0x1017DB04
VTABLE = 0x102A73EC
HOST_LAYOUT = [4, 8, 12, 16]


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_delete(e):
    """operator delete(void*): only the theme's storage is an event (the list frees its nodes through the same glue)."""
    L = e.cl
    lab = L.lab(e.r(3))
    if lab > 0:
        L.rec("Delete", (lab,))


STUBS.imp("__ct__26TbDynamicStringTemplate<c>Fv", CL.recorder("CStrCtor", "N"))
STUBS.imp("__ct__26TbDynamicStringTemplate<w>Fv", CL.recorder("WStrCtor", "N"))
STUBS.imp("__dt__26TbDynamicStringTemplate<c>Fv", CL.recorder("CStrDtor", "L"))
STUBS.imp("__dt__26TbDynamicStringTemplate<w>Fv", CL.recorder("WStrDtor", "L"))
STUBS.imp("__as__26TbDynamicStringTemplate<w>FPCw", CL.recorder("WAssign", "LS"))
STUBS.internal(0x10005670, CL.recorder("Uni", "LL"))
STUBS.imp("__dl__FPv", stub_delete)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def ustr(self, text):
        a = self.alloc(2 * len(text) + 2)
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", ord(c)) for c in text))
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.lists = {}

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def ptr(self, a):
        return a

    def new_sys(self):
        lst = self.e.alloc(0x10)
        sys = self.e.alloc(0x34)
        self.words(sys + 0x2C, [lst])
        self.lists[sys] = lst
        return sys

    def alloc_theme(self):
        return self.e.alloc(0x48)

    def add(self, sys, p):
        T.call_ppc(self.e, ADDTAIL, [self.lists[sys], p])

    def set_list(self, theme, sys):
        self.words(theme + 4, [sys])

    def member(self, theme, k):
        return theme + (0xC, 0x20, 0x34)[k]

    def begin(self, labels):
        self.labels = labels
        self.L = self.e.cl = CL.PpcLog({a: n for a, n in labels.items()}, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if hasattr(self, "saved"):
            CL.Stubs.deactivate(self.e, self.saved)

    def log(self):
        return self.L.log

    def ctor(self, theme, info, sys):
        T.call_ppc(self.e, FN_CT, [theme, info, sys])

    def dtor(self, theme, flag):
        T.call_ppc(self.e, FN_DT, [theme, flag & MASK])

    def get(self, theme):
        return self.e.u32(theme + 4), self.e.u32(theme + 8)

    def vt(self, theme):
        return self.e.u32(theme) == VTABLE

    def listvals(self, sys):
        lst = self.lists[sys]
        count = self.e.u32(lst + 0xC)
        vals = []
        node = self.e.u32(lst + 4)
        while node and len(vals) < 40:
            vals.append(self.e.u32(node))
            node = self.e.u32(node + 4)
        return count, vals


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        bind(lib)

    def alloc(self, n):
        return self.lib.tpw_th_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def ptr(self, a):
        return self.lib.tpw_th_off(a)

    def new_sys(self):
        return self.lib.tpw_th_alloc_sys()

    def alloc_theme(self):
        return self.lib.tpw_th_alloc_theme()

    def add(self, sys, p):
        self.lib.tpw_th_add(sys, p)

    def set_list(self, theme, sys):
        self.lib.tpw_th_setlist(theme, sys)

    def member(self, theme, k):
        return self.lib.tpw_th_member(theme, k)

    def begin(self, labels):
        self.labels = labels
        CL.host_begin(self.lib, {n: a for a, n in labels.items()}, self.case["queues"])

    def done(self):
        pass

    def log(self):
        return CL.host_end(self.lib)

    def ctor(self, theme, info, sys):
        self.lib.tpw_th_ctor(theme, info, sys)

    def dtor(self, theme, flag):
        self.lib.tpw_th_dtor(theme, s32(flag))

    def get(self, theme):
        out = (C.c_uint64 * 2)()
        self.lib.tpw_th_get(theme, out)
        return out[0], out[1]

    def vt(self, theme):
        return True

    def listvals(self, sys):
        out = (C.c_uint64 * 40)()
        count = self.lib.tpw_th_list(sys, out, 40)
        n = 0
        vals = []
        # the host list is walked up to 40 nodes; the count says how many there are
        return count, [out[i] for i in range(min(count, 40))]


def bind(lib):
    if getattr(lib, "bound", False):
        return
    U64, U32, I32 = C.c_uint64, C.c_uint32, C.c_int32
    for name, res, args in (
        ("alloc", U64, [U32]), ("alloc_theme", U64, []), ("alloc_sys", U64, []), ("off", U32, [U64]),
        ("add", None, [U64, U64]), ("list", I32, [U64, C.POINTER(U64), I32]), ("setlist", None, [U64, U64]),
        ("get", None, [U64, C.POINTER(U64)]), ("member", U64, [U64, I32]), ("ctor", None, [U64, U64, U64]),
        ("dtor", None, [U64, I32]),
    ):
        f = getattr(lib, "tpw_th_" + name)
        f.restype, f.argtypes = res, args
    lib.tpw_th_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_th_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


def build(s, case):
    """Owner, list, theme and info; returns the objects and the label map."""
    objs = {}
    sys = objs["sys"] = s.new_sys()
    theme = objs["theme"] = 0 if case["null"] else s.alloc_theme()
    others = [s.alloc(4) for _ in range(case["nother"])]
    objs["fill"] = [theme if tok == "T" else others[tok] for tok in case["order"]]
    labels = {sys: 2}
    if theme:
        labels[theme] = 1
    for i, o in enumerate(others):
        labels[o] = 4 + i
    if case["op"] == "ct":
        words = [case["id"]] + [s.ptr(s.ustr(t)) for t in case["strs"]]
        info = objs["info"] = s.alloc(16)
        s.words(info, words)
        labels[info] = 3
    else:
        if theme:
            s.set_list(theme, sys)
            for k in range(3):
                labels[s.member(theme, k)] = 11 + k
    return objs, labels


def run(s, case):
    try:
        objs, labels = build(s, case)
        s.begin(labels)
        theme, sys = objs["theme"], objs["sys"]
        for p in objs["fill"]:     # the stubs are active (the allocator's fakes), the list's AddTail is the original's
            s.add(sys, p)
        s.log_mark = len(s.log()) if False else 0
        if case["op"] == "ct":
            s.ctor(theme, objs["info"], sys)
        else:
            s.dtor(theme, case["flag"])
        out = {"log": s.log()}
        out["list"] = (lambda cv: (cv[0], [labels.get(v, -9) for v in cv[1]]))(s.listvals(sys))
        if theme:
            lst, tid = s.get(theme)
            out["mList"] = labels.get(lst, -9)
            out["id"] = tid & MASK
            if case["op"] == "ct":
                out["vt"] = s.vt(theme)
        return out
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- cases

WORDS = ["", "a", "Theme", "Fun Land", "x" * 40]


def ct_case(r, nother=None, strs=None, tid=None):
    n = r.randrange(0, 4) if nother is None else nother
    return {"op": "ct", "queues": {}, "null": False, "nother": n, "order": list(range(n)),
            "id": r.choice([0, 1, 0xFFFFFFFF, 0x80000000, r.getrandbits(32)]) if tid is None else tid,
            "strs": [r.choice(WORDS) for _ in range(3)] if strs is None else strs}


def dt_case(r, nother=None, tpos=None, dup=False, flag=None, null=False):
    n = r.randrange(0, 4) if nother is None else nother
    order = list(range(n))
    if tpos is not None and tpos >= 0:
        order.insert(min(tpos, n), "T")
        if dup:
            order.append("T")
    return {"op": "dt", "queues": {}, "null": null, "nother": n, "order": order,
            "flag": r.choice([1, 0, 0xFFFF, 0x7FFF, 0x8000, 0x10001, 0x10000, 2]) if flag is None else flag}


def ct_edges():
    r = random.Random(5670)
    yield ct_case(r, 0, ["", "", ""], 0)
    yield ct_case(r, 1, ["a", "bc", "def"], 7)
    yield ct_case(r, 3, ["Theme", "", "x" * 40], 0xFFFFFFFF)


def dt_edges():
    r = random.Random(5671)
    yield dt_case(r, 0, 0, flag=1)
    yield dt_case(r, 0, 0, flag=0)
    yield dt_case(r, 3, 0, flag=1)
    yield dt_case(r, 3, 1, flag=1)
    yield dt_case(r, 3, 3, flag=0)
    yield dt_case(r, 2, 1, dup=True, flag=1)
    yield dt_case(r, 2, -1, flag=1)
    yield dt_case(r, 0, -1, flag=0)
    yield dt_case(r, 2, 0, flag=0)
    yield dt_case(r, 0, None, null=True, flag=1)
    for f in (1, 0, 0xFFFF, 0x7FFF, 0x8000, 0x10001, 0x10000, 2):
        yield dt_case(r, 1, 0, flag=f)


def ct_random(r):
    return ct_case(r)


def dt_random(r):
    return dt_case(r, tpos=r.choice([-1, 0, 1, 2, 3, 0]), dup=r.random() < 0.2)


TESTS = [
    FnTest("CFSOnlineTheme::CFSOnlineTheme", ppc, host, edge=ct_edges, rnd=ct_random),
    FnTest("CFSOnlineTheme::~CFSOnlineTheme", ppc, host, edge=dt_edges, rnd=dt_random),
]
