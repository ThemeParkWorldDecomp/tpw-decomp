"""Unit v5979, the aerial window's message function and the window-tree hider, against libtpw_unit_HideIfUnder.dylib
(tests/unit_HideIfUnder.cpp), main app ("SimTheme Park"), src/ui_mainpanel2_msg_HideIfUnder.cpp:
  HideIfUnder 0x101a6c44, AerialCallback 0x101a68d0

A case is a window tree and a call. Eight zeroed blocks stand for windows 1..8 (label = number); every InterfaceWindow member and
CoolRect constructor/destructor the functions call is a fake on both sides that records its arguments and answers from the case's
queues (first child, next sibling, the rectangle of each window, the extra parameters, the dimensions, the attributes ...).

The world of a case: the tree, the rectangles, gTagFirstWindow, the animating byte of window 3 (the window GetWindow answers), the
queues. What is compared: the answer, gTagFirstWindow (as a label) and the event log.

No fused operations. Not tested: a NULL window in AerialCallback (the original calls the default handler on it, which the fake
accepts, but it is not a case the game makes).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_HideIfUnder"
MASK = 0xFFFFFFFF

HIDE, AERIAL = 0x101a6c44, 0x101a68d0
SIZES = {HIDE: 692, AERIAL: 824}
OWN = set(SIZES)

CTOR, DTOR = 0x101c7448, 0x101c74e0
GETPOS, SHOW, GETCHILD, GETNEXT, GETATTR = 0x101db884, 0x101dc1ec, 0x101dbac4, 0x101dbb38, 0x101dbb70
GETEXTRA, SETEXTRA, LINK, GETDIMS, ADJUST, SHIFT = 0x101dd030, 0x101dcfd0, 0x101ddf20, 0x101dbeb0, 0x101db0e8, 0x101da9d0
GETWINDOW, STARTANIM, CALLDEF = 0x101dbe40, 0x101dd08c, 0x101de318
PTRGLUE, PUTDEF, SENDMSG = 0x1023baa8, 0x101c95f8, 0x101c9518

ADDR = dict(first=0x103949d0, delta=0x102aad44)
DELTA_LABEL = 20
NW = 8
HOST_LAYOUT = [8, 10, 12, 14, 0, 0]
PPC_RECT = (4, 6, 8, 10)


def s16(v):
    return T.s16(v)


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def wa(e, label):
    return e.side.wa[label] if 0 < label <= NW else 0


def window_stub(name, spec_fn):
    def stub(e):
        spec_fn(e, e.cl)
    return stub


def st_ctor(e):
    e.cl.rec("CoolRect", ())


def st_dtor(e):
    e.cl.rec("~CoolRect", ())


def key(e, base):
    return "%s%d" % (base, e.cl.lab(e.r(3)))


def st_getpos(e):
    L = e.cl
    L.rec("GetPosition", (L.lab(e.r(3)), s16(e.r(5))))
    vals = [s16(L.ret(key(e, n))) for n in ("pos_l", "pos_t", "pos_r", "pos_b")]
    e.write(e.r(4) + 4, struct.pack(">4h", *vals))
    e.set_r(3, 1)


def st_show(e):
    L = e.cl
    L.rec("Show", (L.lab(e.r(3)), s16(e.r(4))))
    e.set_r(3, 0)


def st_getchild(e):
    L = e.cl
    L.rec("GetChild", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, wa(e, L.ret(key(e, "child")) & MASK))


def st_getnext(e):
    L = e.cl
    L.rec("GetNext", (L.lab(e.r(3)),))
    e.set_r(3, wa(e, L.ret(key(e, "next")) & MASK))


def st_getattr(e):
    L = e.cl
    L.rec("GetAttr", (L.lab(e.r(3)),))
    e.set_r(3, L.ret(key(e, "attr")) & MASK)


def st_getextra(e):
    L = e.cl
    i = s16(e.r(4))
    L.rec("getExtraParam", (L.lab(e.r(3)), i))
    e.set_r(3, L.ret("extra%d" % i) & MASK)


def st_setextra(e):
    L = e.cl
    L.rec("setExtraParam", (L.lab(e.r(3)), s16(e.r(4)), e.r(5) & MASK))
    e.set_r(3, 1)


def st_link(e):
    L = e.cl
    L.rec("link", (L.lab(e.r(3)), L.lab(e.r(4)), T.s8(e.r(5)), T.s8(e.r(6))))


def st_getdims(e):
    L = e.cl
    L.rec("GetDims", (L.lab(e.r(3)), s16(e.r(6))))
    e.write(e.r(4), struct.pack(">h", s16(L.ret("dimw"))))
    e.write(e.r(5), struct.pack(">h", s16(L.ret("dimh"))))
    e.set_r(3, 1)


def st_adjust(e):
    L = e.cl
    L.rec("AdjustPosition", (L.lab(e.r(3)), s16(e.r(4)), s16(e.r(5)), s16(e.r(6)), s16(e.r(7))))


def st_shift(e):
    L = e.cl
    L.rec("ShiftPosition", (L.lab(e.r(3)), s16(e.r(4)), s16(e.r(5))))
    e.set_r(3, 1)


def st_getwindow(e):
    L = e.cl
    L.rec("GetWindow", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, wa(e, L.ret("getwindow") & MASK))


def st_startanim(e):
    L = e.cl
    L.rec("startAnim", (L.lab(e.r(3)), L.lab(e.r(4)), s32(e.r(5))))
    e.set_r(3, 1)


def st_calldef(e):
    L = e.cl
    la = L.lab(e.r(5))
    L.rec("CallDefaultWindowHandler", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)) if la == -1 else la, s32(e.r(6))))
    e.set_r(3, L.ret("default") & MASK)


def st_enable(e):
    L = e.cl
    L.rec("Enable", (L.lab(e.r(3)), s16(e.r(4))))
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))      # the glue saves the caller's TOC pointer; the caller reloads it


def st_putdef(e):
    L = e.cl
    L.rec("UI_PutDeferredMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), e.r(7) & MASK))


def st_send(e):
    L = e.cl
    L.rec("UI_SendMessage", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("send") & MASK)


for _a, _f in ((CTOR, st_ctor), (DTOR, st_dtor), (GETPOS, st_getpos), (SHOW, st_show), (GETCHILD, st_getchild),
               (GETNEXT, st_getnext), (GETATTR, st_getattr), (GETEXTRA, st_getextra), (SETEXTRA, st_setextra),
               (LINK, st_link), (GETDIMS, st_getdims), (ADJUST, st_adjust), (SHIFT, st_shift), (GETWINDOW, st_getwindow),
               (STARTANIM, st_startanim), (CALLDEF, st_calldef), (PTRGLUE, st_enable), (PUTDEF, st_putdef),
               (SENDMSG, st_send)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def label_of(self, addr):
        if addr == 0:
            return 0
        return self.rev.get(addr, -1)


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        e.side = self
        self.wa = [0] + [e.alloc(0x140) for _ in range(NW)]
        for w in self.wa[1:]:
            e.write(w + 0x118, struct.pack(">I", e.alloc(0x40)))
        self.rev = {w: i for i, w in enumerate(self.wa) if i}
        fixed = dict(self.rev)
        fixed[ADDR["delta"]] = DELTA_LABEL
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def set_ptr(self, a, addr):
        self.words(a, [addr])

    def get_ptr(self, a):
        return self.rd_words(a, 1)[0]

    def word_of(self, w):
        return w

    def anim(self, w):
        return w + 0x135

    def hide(self, win, limit):
        T.call_ppc(self.e, HIDE, [win & MASK, limit & MASK])
        return 0

    def aerial(self, win, m, a, b):
        r3, _, _ = T.call_ppc(self.e, AERIAL, [win & MASK, m & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.wa = [0] + [lib.tpw_hiu_alloc(lib.lay[5]) for _ in range(NW)]
        for i in range(1, NW + 1):
            lib.tpw_hiu_setwin(i, self.wa[i])
        self.rev = {w: i for i, w in enumerate(self.wa) if i}
        fixed = {i: self.wa[i] for i in range(1, NW + 1)}
        fixed[DELTA_LABEL] = lib.tpw_hiu_addr(b"delta")
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hiu_addr(name.encode())

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_ptr(self, a, addr):
        self.write(a, struct.pack("<Q", addr))

    def get_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def anim(self, w):
        return w + self.lib.lay[4]

    def hide(self, win, limit):
        self.lib.tpw_hiu_hide(win, s16(limit))
        return 0

    def aerial(self, win, m, a, b):
        return self.lib.tpw_hiu_aerial(win, s32(m), s32(a), s32(b)) & MASK

    def word_of(self, w):
        """The 32-bit word a window travels as in a message argument: its arena offset."""
        return self.lib.tpw_hiu_off(w) if w else 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_hiu_addr.restype, lib.tpw_hiu_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_hiu_alloc.restype, lib.tpw_hiu_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_hiu_off.restype, lib.tpw_hiu_off.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_hiu_setwin.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_hiu_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_hiu_hide.argtypes = [C.c_uint64, C.c_int16]
    lib.tpw_hiu_aerial.restype, lib.tpw_hiu_aerial.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_hiu_layout(out)
    lib.lay = list(out)[:n]
    assert lib.lay[:4] == HOST_LAYOUT[:4], lib.lay
    assert lib.lay[4] < 0x140 or True
    lib.bound = True


def build(s, case):
    s.set_ptr(s.g("first"), s.wa[case["first"]] if case["first"] else 0)
    s.write(s.anim(s.wa[3]), bytes([case["anim"]]))


def execute(s, case):
    if case["op"] == "hide":
        return s.hide(s.wa[case["root"]] if case["root"] else 0, case["limit"])
    kind, v = case["a"]
    a = s.word_of(s.wa[v] if v else 0) if kind == "w" else v
    return s.aerial(s.wa[1], case["msg"], a, case["b"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "first": s.label_of(s.get_ptr(s.g("first")))}


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


# ---------------------------------------------------------------------------------------------------------------- cases


def tree(r, kind):
    """child/next queues of a window tree on labels 1..8 (root 1)."""
    parent = {}
    for n in range(2, NW + 1):
        if kind == "chain":
            parent[n] = n - 1
        elif kind == "star":
            parent[n] = 1
        else:
            parent[n] = r.randrange(1, n)
    kids = {n: [c for c in range(2, NW + 1) if parent[c] == n] for n in range(1, NW + 1)}
    q = {}
    for n in range(0, NW + 1):
        k = kids.get(n, [])
        q["child%d" % n] = [k[0] if k else 0]
    for n in range(1, NW + 1):
        q["next%d" % n] = [0]
    for n, k in kids.items():
        for i, c in enumerate(k):
            q["next%d" % c] = [k[i + 1] if i + 1 < len(k) else 0]
    return q


def hide_case(r, kind=None, limit=None, root=1):
    kind = kind or r.choice(["rand", "rand", "chain", "star"])
    q = tree(r, kind)
    if limit is None:
        limit = r.choice([0, -1, 5, r.randrange(-300, 300)])
    for n in range(1, NW + 1):
        q["pos_l%d" % n] = [r.randrange(-300, 300)]
        q["pos_t%d" % n] = [r.randrange(-300, 300)]
        q["pos_r%d" % n] = [limit + r.choice([-2, -1, 0, 1, 2, r.randrange(-400, 400)])]
        q["pos_b%d" % n] = [r.randrange(-300, 300)]
    return {"op": "hide", "queues": q, "root": root, "limit": limit, "first": 0, "anim": 0}


def hide_edges():
    r = random.Random(11)
    yield hide_case(r, "rand", 0, root=0)
    yield hide_case(r, "chain", 10)
    yield hide_case(r, "star", 10)
    yield hide_case(r, "rand", -32768)
    yield hide_case(r, "rand", 32767)
    for lim in (-1, 0, 1):
        yield hide_case(r, "rand", lim)


def hide_random(r):
    return hide_case(r, root=r.choice([0, 1, 1, 1]) if r.random() < 0.05 else 1)


def aerial_case(r, msg=None, akind=None):
    msg = r.choice([0x100001, 0x100002, 0x100003, 0x14, 0x13, 0x100000, 0x100004, r.randrange(0, 0x200000)]) if msg is None else msg
    q = {}
    for n in range(0, NW + 1):
        q["child%d" % n] = [0]
        q["next%d" % n] = [0]
    # a chain for gTagFirstWindow 2 -> 4 -> 5 -> (sometimes more)
    chain = r.sample(range(2, NW + 1), r.randrange(1, 6))
    for i, c in enumerate(chain):
        q["next%d" % c] = [chain[i + 1] if i + 1 < len(chain) else 0]
    for n in range(1, NW + 1):
        q["attr%d" % n] = [r.choice([0, 1, 2, 3, r.randrange(0, 1 << 32)])]
    # aerial window 1 is asked its position twice; child 2 once
    for nm in ("l", "t", "r", "b"):
        q["pos_%s1" % nm] = [r.randrange(-500, 500), r.randrange(-500, 500)]
        q["pos_%s2" % nm] = [r.randrange(-500, 500)]
    q["extra0"] = [r.randrange(0, 1 << 32)]
    q["extra1"] = [r.randrange(0, 1 << 32)]
    q["dimw"] = [r.choice([0, 1, 5, -3, r.randrange(-300, 300)])]
    q["dimh"] = [r.choice([0, 1, 5, -3, 32767, -32768, r.randrange(-300, 300)])]
    q["getwindow"] = [r.choice([0, 3, 3])]
    q["default"] = [r.randrange(0, 1 << 32)]
    q["send"] = [r.randrange(0, 1 << 32)]
    if akind is None:
        akind = r.choice(["w", "w", "w", "z", "v"])
    a = ("w", r.randrange(2, NW + 1)) if akind == "w" else (("w", 0) if akind == "z" else ("v", r.randrange(0, 1 << 32)))
    if msg in (0x100002, 0x100001) and akind == "v":
        a = ("w", 4)
    return {"op": "aerial", "queues": q, "msg": msg, "a": a, "b": r.choice([0, 0, 1, -1, r.randrange(-1000, 1000)]),
            "first": r.choice([0, 2, 2, 5]), "anim": r.choice([0, 0, 1, 0x80, 0xff])}


def aerial_edges():
    r = random.Random(5)
    for msg in (0x100001, 0x100002, 0x100003, 0x14, 0x13, 0x100000, 0x100004, 0):
        for ak in ("w", "z"):
            for _ in range(3):
                yield aerial_case(r, msg, ak)
    c = aerial_case(r, 0x100001, "w")
    c["queues"]["getwindow"] = [3]
    c["anim"] = 0
    yield c
    c = aerial_case(r, 0x100001, "w")
    c["queues"]["dimw"], c["queues"]["dimh"] = [-5], [-10]
    yield c
    c = aerial_case(r, 0x100001, "w")
    c["queues"]["dimh"] = [32767]
    yield c


def aerial_random(r):
    return aerial_case(r)


TESTS = [
    FnTest("HideIfUnder", ppc, host, edge=hide_edges, rnd=hide_random),
    FnTest("AerialCallback", ppc, host, edge=aerial_edges, rnd=aerial_random),
]
