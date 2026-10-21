"""Unit v51048, eight small main-app functions, against libtpw_unit_CGame_GetLocalisedThemeName_3.dylib
(tests/unit_CGame_GetLocalisedThemeName_3.cpp), main app ("SimTheme Park"), src/game/ActionRecorder.cpp, src/game/Game.cpp, src/game/people/ParkTime.cpp, src/game/ui/InterfaceBar.cpp, src/game/ui/UI_Base.cpp, src/game/ui/ui_visitorStatus.cpp:
  CGame::GetLocalisedThemeName 0x1014a478 (narrow theme name), CGame::TogglePause 0x10149a74, __sinit_ActionRecorder_cpp 0x10146e9c,
  TbTList<IDX..>::RemoveAll 0x10141f1c, CParkTime::GetFunnyTimeForGameTurns 0x10112928, InterfaceBar::SetValue 0x101c887c,
  InterfaceBar::operator- 0x101c89a0, UI_PutDeferredMessage 0x101c95f8

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation (objects whose host layout differs, the game, the
park time and the windows, are made by the library's own constructors-by-offset). The callees outside the unit (the wide string's
constructors and destructor, StringToUniString, the wide GetLocalisedThemeName, the UI and clock pause, the sound and advisor pauses,
InterfaceTimedMessages::Add, the recorder constructor, __register_global_object, operator delete, and the virtual calls through __ptr_glue:
WhatAmI and the draw objects' Change) are fakes on both sides that record their arguments and answer from the case.

What is compared: the answers, the objects and globals the call may change, the event log. Not tested: nothing skipped; the bool argument
of TogglePause is 0 or 1 (the caller's register would carry more bits into the stored byte).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CGame_GetLocalisedThemeName_3"
MASK = 0xFFFFFFFF

FN_NAME, FN_TOGGLE, FN_SINIT, FN_REMOVEALL = 0x1014A478, 0x10149A74, 0x10146E9C, 0x10141F1C
FN_FUNNY, FN_SETVALUE, FN_MINUS, FN_PUT = 0x10112928, 0x101C887C, 0x101C89A0, 0x101C95F8
SIZES = {FN_NAME: 136, FN_TOGGLE: 212, FN_SINIT: 88, FN_REMOVEALL: 100, FN_FUNNY: 132, FN_SETVALUE: 188, FN_MINUS: 128, FN_PUT: 120}
OWN = set(SIZES)

MAGIC_CHANGE = 0x7A000001      # the draw objects' vtable slot 0x14 (Change)
MAGIC_WHAT = 0x7A000002        # the windows' vtable slot 0x14 (WhatAmI)

ADDR = dict(ui=0x10396440, clock=0x1037F048, recorder=0x1037EC44, string=0x1037F024, string_node=0x1037F038,
            recorder_node=0x1037F018, dtor_w=0x103BCBB8, dtor_rec=0x10266E58, timed=0x10395BFC)


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
STUBS.internal(0x100057A0, CL.recorder("StringToUniString", "LL", ret="i"))
def glue(fn):
    """A cross-TOC glue call saves r2 at 0x14(r1); the caller reloads it afterwards."""
    def run_glue(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        return fn(e)
    return run_glue


STUBS.internal(0x10238740, glue(CL.recorder("CtorW", "N")))
STUBS.internal(0x10238800, glue(CL.recorder("CtorWText", "LS")))
STUBS.internal(0x10238710, glue(CL.recorder("DtorW", "Li")))
STUBS.internal(0x1014A570, CL.recorder("GetLocalisedThemeNameW", "LLL"))
STUBS.internal(0x101EF940, CL.recorder("UIInterface_TogglePause", "L"))
STUBS.internal(0x10147450, CL.recorder("Clock_TogglePause", "L"))
STUBS.internal(0x100DD5D8, CL.recorder("SOUND_UnPause", ""))
STUBS.internal(0x100DD548, CL.recorder("SOUND_Pause", ""))
STUBS.internal(0x100DDE38, CL.recorder("SOUND_SetPauseOn3DSoundsOnly", "i"))
STUBS.internal(0x10007DD4, CL.recorder("ADV_UnPause", ""))
STUBS.internal(0x10007D54, CL.recorder("ADV_Pause", ""))
STUBS.internal(0x101C82AC, CL.recorder("Add", "LLiiiu"))
STUBS.internal(0x10143658, CL.recorder("ActionRecorder_ctor", "L"))
STUBS.internal(0x10239940, glue(CL.recorder("Register", "LLL")))


def stub_delete(e):
    """operator delete(void*): records the id the node holds."""
    e.cl.rec("delete", (e.cl.lab(e.r(3)),))


def stub_ptr_glue(e):
    """__ptr_glue: the virtual call; r12 is the slot's word (a magic number in these worlds)."""
    L = e.cl
    r12 = e.r(12)
    if r12 == MAGIC_CHANGE:
        L.rec("Change", (L.lab(e.r(3)), e.r(4), s32(e.u32(e.r(5)))))
    else:
        assert r12 == MAGIC_WHAT, hex(r12)
        L.rec("WhatAmI", (L.lab(e.r(3)),))
        e.set_r(3, L.ret("WhatAmI") & MASK)


STUBS.internal(0x10238218, stub_delete)
STUBS.internal(0x1023BAA8, stub_ptr_glue)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.case = case
        self.labels = []
        self.saved = None

    def begin(self):
        fixed = {addr: lab for lab, addr in self.labels}
        self.L = self.e.cl = CL.PpcLog(fixed, self.case["queues"])
        self.saved = STUBS.activate(self.e)

    def done(self):
        if self.saved is not None:
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

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK

    # objects
    def mk_draw(self):
        a = self.alloc(0x40)
        vt = self.alloc(0x20)
        self.words(vt + 0x14, [MAGIC_CHANGE])
        self.words(a + 0x38, [vt])
        return a

    def mk_bar(self, value, d1, d2):
        a = self.alloc(0x160)
        vt = self.alloc(0x20)
        self.words(vt + 0x14, [MAGIC_WHAT])
        self.words(a + 0x94, [d1, d2])
        self.words(a + 0x118, [vt])
        self.words(a + 0x140, [value])
        return a

    def bar_value(self, a):
        return s32(self.rd_word(a + 0x140))

    def mk_park(self, rate):
        a = self.alloc(0x30)
        self.words(a + 0x1C, [rate])
        return a

    def mk_game(self, f1c, f20, f3c):
        a = self.alloc(0x40)
        self.words(a + 0x1C, [f1c])
        self.write(a + 0x20, bytes([f20 & 0xFF]))
        self.words(a + 0x3C, [f3c])
        return a

    def game_get(self, a):
        return [s32(self.rd_word(a + 0x1C)), self.read(a + 0x20, 1)[0], s32(self.rd_word(a + 0x3C))]

    def put_timed(self, v):
        self.words(ADDR["timed"], [v])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.case = case
        self.labels = []
        bind(lib)

    def begin(self):
        CL.host_begin(self.lib, {lab: addr for lab, addr in self.labels}, self.case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_v51048_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_v51048_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    # objects
    def mk_draw(self):
        return self.lib.tpw_v51048_mk_draw()

    def mk_bar(self, value, d1, d2):
        return self.lib.tpw_v51048_mk_bar(s32(value), d1, d2)

    def bar_value(self, a):
        return self.lib.tpw_v51048_bar_value(a)

    def mk_park(self, rate):
        return self.lib.tpw_v51048_mk_park(rate & MASK)

    def mk_game(self, f1c, f20, f3c):
        return self.lib.tpw_v51048_mk_game(s32(f1c), f20 & 0xFF, s32(f3c))

    def game_get(self, a):
        out = (C.c_int32 * 3)()
        self.lib.tpw_v51048_game_get(a, out)
        return list(out)

    def put_timed(self, v):
        self.write(self.g("timed"), struct.pack("<Q", v))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_v51048_"

    def sig(name, res, args):
        f = getattr(lib, p + name)
        f.restype, f.argtypes = res, args

    u64, i32, u32 = C.c_uint64, C.c_int32, C.c_uint32
    sig("addr", u64, [C.c_char_p])
    sig("alloc", u64, [u32])
    sig("mk_draw", u64, [])
    sig("mk_bar", u64, [i32, u64, u64])
    sig("bar_value", i32, [u64])
    sig("mk_park", u64, [u32])
    sig("mk_game", u64, [i32, u32, i32])
    sig("game_get", None, [u64, C.POINTER(i32)])
    sig("name", None, [u64, u64, u64])
    sig("toggle", None, [u64, i32])
    sig("sinit", None, [])
    sig("mk_node", u64, [u32, u64, u64])
    sig("removeall", None, [u64, u64, i32, C.POINTER(u32)])
    sig("funny", C.c_int64, [u64, u32])
    sig("setvalue", i32, [u64, i32])
    sig("minus", i32, [u64, u64])
    sig("put", i32, [u64, i32, i32, i32, u32])
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world and the calls


def w_name(s, c):
    game, theme, out = s.alloc(0x40), s.alloc(0x20), s.alloc(0x20)
    s.labels += [(10, game), (11, theme), (12, out)]
    s.begin()
    if isinstance(s, Ppc):
        s.call(FN_NAME, [game, theme, out])
    else:
        s.lib.tpw_v51048_name(game, theme, out)
    return {}


def w_toggle(s, c):
    game = s.mk_game(c["f1c"], c["f20"], c["f3c"])
    s.labels += [(10, game), (20, s.g("ui")), (21, s.g("clock"))]
    s.begin()
    if isinstance(s, Ppc):
        s.call(FN_TOGGLE, [game, c["all"]])
    else:
        s.lib.tpw_v51048_toggle(game, c["all"])
    return {"game": s.game_get(game)}


def w_sinit(s, c):
    s.labels += [(1, s.g("recorder")), (2, s.g("string")), (3, s.g("string_node")), (4, s.g("recorder_node")),
                 (5, s.g("dtor_w")), (6, s.g("dtor_rec"))]
    s.begin()
    if isinstance(s, Ppc):
        s.call(FN_SINIT, [])
    else:
        s.lib.tpw_v51048_sinit()
    return {}


def w_removeall(s, c):
    ids = c["ids"]
    ppc_side = isinstance(s, Ppc)
    if ppc_side:
        nodes = [s.alloc(10) for _ in ids]
        for i, (a, nid) in enumerate(zip(nodes, ids)):
            s.half(a, nid)
            s.words(a + 2, [nodes[i + 1] if i + 1 < len(nodes) else 0])   # packed: next at +2, previous at +6
            s.words(a + 6, [nodes[i - 1] if i > 0 else 0])
    else:   # the host's CNodes come from the heap (the translation frees them), built from the tail
        nodes = [0] * len(ids)
        for i in range(len(ids) - 1, -1, -1):
            nodes[i] = s.lib.tpw_v51048_mk_node(ids[i], nodes[i + 1] if i + 1 < len(ids) else 0, 0)
        for i in range(1, len(ids)):
            s.write(nodes[i] + 16, struct.pack("<Q", nodes[i - 1]))
    head = nodes[0] if nodes else 0
    tail = c["tail"]
    s.labels += [(50 + i, a) for i, a in enumerate(nodes)]
    s.begin()
    if isinstance(s, Ppc):
        obj = s.alloc(16)
        s.words(obj, [0x12345678, head, tail, c["count"]])
        s.call(FN_REMOVEALL, [obj])
        after = s.rd_words(obj + 4, 3)
    else:
        out = (C.c_uint32 * 3)()
        s.lib.tpw_v51048_removeall(head, tail, s32(c["count"]), out)
        after = list(out)
    return {"after": after}


def w_funny(s, c):
    park = s.mk_park(c["rate"])
    s.begin()
    if isinstance(s, Ppc):
        res = s.alloc(8)
        s.call(FN_FUNNY, [res, park, c["turns"]])
        hi, lo = s.rd_words(res, 2)
        ticks = (hi << 32) | lo
    else:
        ticks = s.lib.tpw_v51048_funny(park, c["turns"] & MASK) & 0xFFFFFFFFFFFFFFFF
    return {"ticks": ticks}


def w_setvalue(s, c):
    d1 = s.mk_draw() if c["d1"] else 0
    d2 = s.mk_draw() if c["d2"] else 0
    bar = s.mk_bar(c["value"], d1, d2)
    s.labels += [(30, bar)] + ([(31, d1)] if d1 else []) + ([(32, d2)] if d2 else [])
    s.begin()
    if isinstance(s, Ppc):
        ans = s.call(FN_SETVALUE, [bar, c["arg"]])
    else:
        ans = s.lib.tpw_v51048_setvalue(bar, s32(c["arg"])) & MASK
    return {"answer": ans, "value": s.bar_value(bar)}


def w_minus(s, c):
    a = s.mk_bar(c["va"], 0, 0)
    b = s.mk_bar(c["vb"], 0, 0)
    s.labels += [(30, a), (31, b)]
    s.begin()
    if isinstance(s, Ppc):
        ans = s.call(FN_MINUS, [a, b])
    else:
        ans = s.lib.tpw_v51048_minus(a, b) & MASK
    return {"answer": ans, "va": s.bar_value(a), "vb": s.bar_value(b)}


def w_put(s, c):
    timed = s.alloc(0x10) if c["timed"] else 0
    window = s.alloc(0x10) if c["window"] else 0
    s.put_timed(timed)
    s.labels += ([(40, timed)] if timed else []) + ([(41, window)] if window else [])
    s.begin()
    if isinstance(s, Ppc):
        ans = s.call(FN_PUT, [window, c["msg"], c["a"], c["b"], c["time"]])
    else:
        ans = s.lib.tpw_v51048_put(window, s32(c["msg"]), s32(c["a"]), s32(c["b"]), c["time"] & MASK) & MASK
    return {"answer": ans}


WORLDS = {"name": w_name, "toggle": w_toggle, "sinit": w_sinit, "removeall": w_removeall, "funny": w_funny,
          "setvalue": w_setvalue, "minus": w_minus, "put": w_put}


def run(s, case):
    try:
        res = WORLDS[case["op"]](s, case)
        res["log"] = s.log()
        return res
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

I32 = [0, 1, -1, 2, 3, 4, 0x3FF, 0x400, 0x401, 0x7FFFFFFF, -0x80000000, 5, 100, -100]
U32 = [0, 1, 2, 3, 4, 5, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 15000, 0x10000, 0xFFFF, 0x40000000]


def ri(r):
    return r.choice(I32) if r.random() < 0.5 else s32(r.getrandbits(32))


def ru(r):
    return r.choice(U32) if r.random() < 0.5 else r.getrandbits(32)


def mk(op, r, **kw):
    q = {}
    c = {"op": op, "queues": q}
    c.update(kw)
    return c


# ---- name
def name_case(r, conv=None):
    conv = r.choice([0, 1, -1, 5]) if conv is None else conv
    return mk("name", r, queues=None) | {"queues": {"StringToUniString": [conv]}}


def name_edges():
    r = random.Random(1)
    for v in (0, 1, -1, 7):
        yield name_case(r, v)


# ---- toggle
def toggle_case(r, f1c=None, f20=None, f3c=None, allv=None):
    return {"op": "toggle", "queues": {},
            "f1c": r.choice([0, 1, 2, -1, r.getrandbits(32)]) if f1c is None else f1c,
            "f20": r.choice([0, 1, 2, 0x80, 0xFF, r.getrandbits(8)]) if f20 is None else f20,
            "f3c": r.choice([1, 1, 0, 2, -1, r.getrandbits(32)]) if f3c is None else f3c,
            "all": r.choice([0, 1]) if allv is None else allv}


def toggle_edges():
    r = random.Random(2)
    for f3c in (0, 1, 2, -1):
        for f1c in (0, 1, 2, -1):
            for f20 in (0, 1, 0x80):
                for allv in (0, 1):
                    yield toggle_case(r, f1c, f20, f3c, allv)


# ---- sinit
def sinit_case(r):
    return {"op": "sinit", "queues": {}}


def sinit_edges():
    yield sinit_case(None)


# ---- removeall
def removeall_case(r, n=None):
    n = r.choice([0, 1, 2, 3, 5, 9]) if n is None else n
    return {"op": "removeall", "queues": {}, "ids": [r.getrandbits(16) for _ in range(n)],
            "tail": r.choice([0, r.getrandbits(32)]), "count": r.choice([0, n, r.getrandbits(32)])}


def removeall_edges():
    r = random.Random(3)
    for n in (0, 1, 2, 3, 7):
        yield removeall_case(r, n)


# ---- funny
def funny_case(r, turns=None, rate=None):
    return {"op": "funny", "queues": {}, "turns": ru(r) if turns is None else turns, "rate": ru(r) if rate is None else rate}


def funny_edges():
    r = random.Random(4)
    for turns in (0, 1, 3, 4, 5, 7, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
        for rate in (0, 1, 15000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF):
            yield funny_case(r, turns, rate)


# ---- setvalue
def setvalue_case(r, value=None, arg=None, d1=None, d2=None):
    value = ri(r) if value is None else value
    if arg is None:
        arg = value if r.random() < 0.2 else ri(r)
    return {"op": "setvalue", "queues": {}, "value": value, "arg": arg,
            "d1": r.choice([0, 1]) if d1 is None else d1, "d2": r.choice([0, 1]) if d2 is None else d2}


def setvalue_edges():
    r = random.Random(5)
    for d1, d2 in ((0, 0), (1, 0), (0, 1), (1, 1)):
        for arg in (0, 1, -1, 0x3FF, 0x400, 0x401, 0x7FFFFFFF, -0x80000000, 50):
            yield setvalue_case(r, 7, arg, d1, d2)
        yield setvalue_case(r, 50, 50, d1, d2)
        yield setvalue_case(r, 0x400, 0x500, d1, d2)
        yield setvalue_case(r, 0, -5, d1, d2)


# ---- minus
def minus_case(r, same=None):
    same = r.choice([0, 1]) if same is None else same
    k = r.choice([9, 1, 0, -1, 5])
    return {"op": "minus", "queues": {"WhatAmI": [k, k if same else k + 1]}, "va": ri(r), "vb": ri(r)}


def minus_edges():
    r = random.Random(6)
    for same in (1, 0):
        for _ in range(3):
            yield minus_case(r, same)
    c = minus_case(r, 1)
    c["va"], c["vb"] = -0x80000000, 1
    yield c
    c = minus_case(r, 1)
    c["va"], c["vb"] = 0x7FFFFFFF, -1
    yield c


# ---- put
def put_case(r, timed=None, window=None, msg=None):
    return {"op": "put", "queues": {}, "timed": r.choice([0, 1, 1]) if timed is None else timed,
            "window": r.choice([0, 1, 1]) if window is None else window,
            "msg": r.choice([0, 1, 2, 3, 4, 5, -1, 0x7FFFFFFF, -0x80000000, r.getrandbits(32)]) if msg is None else msg,
            "a": ri(r), "b": ri(r), "time": ru(r)}


def put_edges():
    r = random.Random(7)
    for timed in (0, 1):
        for window in (0, 1):
            for msg in (-1, 0, 1, 2, 3, 4, 5, 0x7FFFFFFF, -0x80000000):
                yield put_case(r, timed, window, msg)


TESTS = [
    FnTest("CGame::GetLocalisedThemeName", ppc, host, edge=name_edges, rnd=name_case),
    FnTest("CGame::TogglePause", ppc, host, edge=toggle_edges, rnd=toggle_case),
    FnTest("__sinit_ActionRecorder_cpp", ppc, host, edge=sinit_edges, rnd=sinit_case),
    FnTest("TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId,unsigned_short>,tag_IDX_TYPE<IDXTYPEUniqueNameThingId,unsigned_short>>::RemoveAll",
           ppc, host, edge=removeall_edges, rnd=removeall_case),
    FnTest("CParkTime::GetFunnyTimeForGameTurns", ppc, host, edge=funny_edges, rnd=funny_case),
    FnTest("InterfaceBar::SetValue", ppc, host, edge=setvalue_edges, rnd=setvalue_case),
    FnTest("InterfaceBar::operator-", ppc, host, edge=minus_edges, rnd=minus_case),
    FnTest("UI_PutDeferredMessage", ppc, host, edge=put_edges, rnd=put_case),
]
