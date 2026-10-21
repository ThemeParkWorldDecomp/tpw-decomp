"""Unit v5363, the ride list panel's FillRideList and UpdateViewWindow, against libtpw_unit_FillRideList.dylib (tests/unit_FillRideList.cpp),
main app ("SimTheme Park"), src/game/ui/ui_ridepanel.cpp:
  FillRideList 0x101b5c14, UpdateViewWindow 0x101b5f1c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the window lookup, the list, the ride database, the control manager, the golden tickets, the analyser, the strings, the drawing,
the two rectangles, the render area, the text table and the virtual SetText of the label) are fakes on both sides that record their
arguments (pointers as labels, texts as a hash of their code units, colours as five bytes) and answer from the case's queues; a pointer
answer is a handle (the number of a block the case built, 0 for NULL).

The world of a case: the globals (world pointer, ride list panel and kind, ride database, balance word, text table pointer), the ride
records (id, load failed, UI type, the flag slot, the ticket price, the type number and string index, the z of the window), their object
controls, the profile grid and the surface sizes. What is compared: the log of every call and the kind the list remembers.

No fused operations. Not tested: a NULL ride in UpdateViewWindow where the original reads it (a non-negative id with no record), a NULL
object control or list item where the original dereferences, a NULL panel in UpdateViewWindow (the original passes NULL to GetWindow, which
reads it), profile counts above 16 (the original reads past the grid).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_FillRideList"
MASK = 0xFFFFFFFF

FN_FILL, FN_VIEW = 0x101B5C14, 0x101B5F1C
SIZES = {FN_FILL: 728, FN_VIEW: 1420}
OWN = set(SIZES)

# the callees faked on both sides
A_PARK, A_GETWINDOW, A_REMOVE, A_FIRST, A_NEXT = 0x1013EBC8, 0x101DBE40, 0x101D3CC8, 0x101547B4, 0x10154800
A_GETCTRL, A_BOUGHT, A_GETSTRING, A_GETTYPENAME = 0x100FA0B8, 0x100FBFEC, 0x10180468, 0x10154AF0
A_NUMINPARK, A_LAST, A_ADDITEM, A_UPDATE = 0x100EA52C, 0x100EA8B8, 0x101D3000, 0x101D2220
A_RIDEINFO, A_CLIENTRECT, A_SURFACE, A_FINISH = 0x10154868, 0x101DBA4C, 0x101DBC98, 0x101DBD1C
A_MAKEAREA, A_SETCUR, A_RECT, A_PREVIEW = 0x1018B360, 0x10238EA8, 0x1023AD98, 0x10181400
A_CONSTSTR, A_CONVERT, A_RENDERAREA, A_COOLCT, A_COOLDT = 0x10238B78, 0x101803CC, 0x10238E30, 0x101C7448, 0x101C74E0
A_PROFILE, A_PTRGLUE = 0x1008F880, 0x1023BAA8

WORLD = 0x30000000          # a pointer the code never dereferences (the fakes take addresses derived from it)
TVEC = 0x12345678           # the function descriptor the label window's table answers for SetText
ADDR = dict(gameworld=0x1037EBFC, rdb=0x1037F6C8, textptr=0x1037FF18 + 0x5A0, ltype=0x10395994, panel=0x10395998,
            bal=0x102B4568 + 0x5CC)
HOST_LAYOUT = [4, 0x10]

# block handles (also their labels)
H_PARK, H_PANEL, H_LIST, H_VIEW, H_PIC, H_LABEL, H_PROFILE, H_PIXELS, H_CONST, H_TABLE, H_VT, H_ARG = 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
RH = [16, 17, 18, 19]       # ride records
CH = [20, 21, 22, 23]       # object controls
BLOCKS = {h: 0x1000 for h in [H_PARK, H_PANEL, H_LIST, H_VIEW, H_PIC, H_LABEL, H_PROFILE, H_PIXELS, H_CONST, H_TABLE, H_VT, H_ARG] + CH}
BLOCKS.update({h: 0x4000 for h in RH})
L_RDB, L_CM, L_GT, L_WORLD = 40, 41, 42, 43
STR_KEYS = {0x86: 0, 0x87: 1, 0x89: 2, 0x1000: 3}

MAC_OFF = dict(id=0x0, cost=0xC4, typestr=0x90, flag=0x284, uitype=0x7A8, failed=0x804, typeno=0x78C, z=0xDC)

STUBS = CL.Stubs()
STUBS.real.update(OWN)
CUR = [None]


def s32(v):
    return T.s32(v)


def hash16(e, addr):
    if addr == 0:
        return -1
    h = 0
    n = 0
    while n < 130:
        u = struct.unpack(">H", e.read(addr + 2 * n, 2))[0]
        if u == 0:
            break
        h = (h * 31 + u) & MASK
        n += 1
    return h ^ (n << 32)


def ptr(h):
    return CUR[0].ptrs.get(h, 0)


def rec(e, name, *vals):
    e.cl.rec(name, tuple(vals))


def lab(e, v):
    return e.cl.lab(v & MASK)


def ret(e, key):
    return e.cl.ret(key)


def st_park(e):
    rec(e, "GetParkAnalyser", lab(e, e.r(3)))
    e.set_r(3, ptr(ret(e, "GetParkAnalyser")))


def st_getwindow(e):
    i = s32(e.r(4))
    rec(e, "GetWindow", lab(e, e.r(3)), i)
    e.set_r(3, ptr(ret(e, "GetWindow:%d" % i)))


def st_remove(e):
    rec(e, "RemoveAllItems", lab(e, e.r(3)))


def st_first(e):
    rec(e, "GetFirstRide", lab(e, e.r(3)))
    e.set_r(3, ptr(ret(e, "GetFirstRide")))


def st_next(e):
    rec(e, "GetNextRide", lab(e, e.r(3)))
    e.set_r(3, ptr(ret(e, "GetNextRide")))


def st_getctrl(e):
    i = e.r(4) >> 16
    rec(e, "GetObjectControl", lab(e, e.r(3)), i, s32(e.r(5)))
    e.set_r(3, ptr(ret(e, "GetObjectControl:%d" % i)))


def st_bought(e):
    i = e.r(4) >> 16
    rec(e, "HasBeenBought", lab(e, e.r(3)), i)
    e.set_r(3, 1 if ret(e, "HasBeenBought:%d" % i) != 0 else 0)


def st_getstring(e):
    i = s32(e.r(3))
    rec(e, "UIMain_GetString", i)
    e.set_r(3, CUR[0].strs.get(i, 0))


def st_typename(e):
    i = e.r(4) >> 16
    rec(e, "GetTypeName", lab(e, e.r(3)), i, e.r(6))
    e.write(e.r(5), struct.pack(">3H", (i ^ 0x5A5A) & 0xFFFF, 0x41, 0))


def st_numinpark(e):
    i = e.r(4) >> 16
    rec(e, "NumberOfObjectInPark", lab(e, e.r(3)), i)
    e.set_r(3, ret(e, "NumberOfObjectInPark:%d" % i) & MASK)


def st_last(e):
    back = s32(e.r(6))
    rec(e, "LastOfObjectTypeResearched", lab(e, e.r(4)), s32(e.r(5)), back)
    e.write(e.r(3), struct.pack(">H", ret(e, "Last:%d" % back) & 0xFFFF))


def st_additem(e):
    w = struct.unpack(">3I", e.read(e.r(4), 12))
    rec(e, "AddItem", lab(e, e.r(3)), hash16(e, w[0]), s32(w[1]), s32(w[2]), s32(e.r(5)), s32(e.r(6)), s32(e.r(7)))
    e.set_r(3, 0)


def st_update(e):
    rec(e, "UpdateDisplay", lab(e, e.r(3)))


def st_rideinfo(e):
    i = e.r(4) >> 16
    rec(e, "GetRideInfo", lab(e, e.r(3)), i)
    e.set_r(3, ptr(ret(e, "GetRideInfo:%d" % i)))


def st_clientrect(e):
    rec(e, "GetClientRect", lab(e, e.r(3)))
    e.set_r(3, 0)


def st_surface(e):
    rec(e, "GetClientObjectSurface", lab(e, e.r(3)))
    e.write(e.r(4), struct.pack(">i", s32(ret(e, "SurfW"))))
    e.write(e.r(5), struct.pack(">i", s32(ret(e, "SurfH"))))
    e.set_r(3, ptr(ret(e, "Surface")))


def st_finish(e):
    rec(e, "FinishWithClientObjectSurface", lab(e, e.r(3)))


def st_makearea(e):
    rec(e, "MakeRenderArea", lab(e, e.r(3)), lab(e, e.r(4)), s32(e.r(5)), s32(e.r(6)))


def st_setcur(e):
    rec(e, "LbDraw_SetCurrent", lab(e, e.r(3)))


def st_rect(e):
    box = struct.unpack(">4i", e.read(e.r(3), 16))
    c, d = e.r(4), e.r(5)
    rec(e, "LbDraw_RectangleFilled", *(list(box) + [c >> 24, (c >> 16) & 255, (c >> 8) & 255, c & 255, d >> 24]))


def st_preview(e):
    rec(e, "UIMain_ShowRidePreview", lab(e, e.r(3)), e.r(4), s32(e.r(5)))


def st_conststr(e):
    rec(e, "GetConstString", lab(e, e.r(3)), e.r(4))
    e.set_r(3, ptr(ret(e, "GetConstString:%d" % e.r(4))))


def st_convert(e):
    rec(e, "UIMain_ConvertMulti", lab(e, e.r(3)))
    e.set_r(3, CUR[0].strs[0x1000])


def st_renderarea(e):
    rec(e, "TbRenderArea", lab(e, e.r(3)))


def st_coolct(e):
    rec(e, "CoolRect", lab(e, e.r(3)))


def st_cooldt(e):
    rec(e, "~CoolRect", lab(e, e.r(3)))


def st_profile(e):
    rec(e, "LLCC_GetRideProfileInfo", lab(e, e.r(3)))
    e.set_r(3, ptr(ret(e, "Profile")))


def st_ptrglue(e):
    rec(e, "SetText", lab(e, e.r(3)), hash16(e, e.r(4)), 1 if e.r(12) == TVEC else 0)
    e.set_r(3, 1)


for _a, _f in [(A_PARK, st_park), (A_GETWINDOW, st_getwindow), (A_REMOVE, st_remove), (A_FIRST, st_first), (A_NEXT, st_next),
               (A_GETCTRL, st_getctrl), (A_BOUGHT, st_bought), (A_GETSTRING, st_getstring), (A_GETTYPENAME, st_typename),
               (A_NUMINPARK, st_numinpark), (A_LAST, st_last), (A_ADDITEM, st_additem), (A_UPDATE, st_update),
               (A_RIDEINFO, st_rideinfo), (A_CLIENTRECT, st_clientrect), (A_SURFACE, st_surface), (A_FINISH, st_finish),
               (A_MAKEAREA, st_makearea), (A_SETCUR, st_setcur), (A_RECT, st_rect), (A_PREVIEW, st_preview),
               (A_CONSTSTR, st_conststr), (A_CONVERT, st_convert), (A_RENDERAREA, st_renderarea), (A_COOLCT, st_coolct),
               (A_COOLDT, st_cooldt), (A_PROFILE, st_profile), (A_PTRGLUE, st_ptrglue)]:
    STUBS.internal(_a, _f)


def _glue(fn):
    """The import glue saves the caller's TOC at 20(r1) and the caller reloads it after the call: do the same."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


for _a, _f in [(A_CONSTSTR, st_conststr), (A_RENDERAREA, st_renderarea), (A_SETCUR, st_setcur), (A_RECT, st_rect),
               (A_PTRGLUE, st_ptrglue)]:
    STUBS.internal(_a, _glue(_f))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def w32(self, a, v):
        self.words(a, [v])

    def put16(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "H", v & 0xFFFF))

    def put8(self, a, v):
        self.write(a, bytes([v & 255]))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]

    def blocks(self):
        self.blk = {h: self.alloc(n) for h, n in BLOCKS.items()}

    def fixed_labels(self):
        d = {h: self.blk[h] for h in BLOCKS}
        d[L_RDB] = self.g("rdb")
        d[L_CM] = WORLD + 0x1D943C
        d[L_GT] = WORLD + 0x2C0
        d[L_WORLD] = WORLD
        return d


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        CUR[0] = self
        self.blocks()
        self.ptrs = {0: 0}
        self.ptrs.update(self.blk)
        self.strs = {}
        for k, i in STR_KEYS.items():
            a = e.alloc(0x20)
            e.write(a, struct.pack(">3H", 0x6000 + k, 0x7000 + (k & 0xFF), 0))
            self.strs[k] = a
        self.L = e.cl = CL.PpcLog({a: h for h, a in self.fixed_labels().items()}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def off(self, name):
        return MAC_OFF[name]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def wptr(self, a, v):
        self.w32(a, v)

    def setup_vt(self, window):
        self.w32(window + 0x118, self.blk[H_VT])
        self.w32(self.blk[H_VT] + 0x2C, TVEC)

    def log(self):
        return self.L.log

    def fill(self, t, r):
        T.call_ppc(self.e, FN_FILL, [t & MASK, r & MASK])
        return 0

    def view(self, a):
        T.call_ppc(self.e, FN_VIEW, [a])
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks()
        for h, a in self.blk.items():
            lib.tpw_fr_setptr(h, a)
        CL.host_begin(lib, self.fixed_labels(), case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_fr_addr(name.encode())

    def off(self, name):
        return self.lib.tpw_fr_off(name.encode())

    def alloc(self, n):
        return self.lib.tpw_fr_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def setup_vt(self, window):
        self.lib.tpw_fr_setup_vt(window)

    def log(self):
        return CL.host_end(self.lib)

    def fill(self, t, r):
        self.lib.tpw_fr_fill(s32(t), s32(r))
        return 0

    def view(self, a):
        self.lib.tpw_fr_view(a)
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_fr_addr.restype, lib.tpw_fr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_fr_alloc.restype, lib.tpw_fr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_fr_off.restype, lib.tpw_fr_off.argtypes = C.c_int64, [C.c_char_p]
    lib.tpw_fr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_fr_setptr.restype, lib.tpw_fr_setptr.argtypes = None, [C.c_uint32, C.c_uint64]
    lib.tpw_fr_setup_vt.restype, lib.tpw_fr_setup_vt.argtypes = None, [C.c_uint64]
    lib.tpw_fr_fill.restype, lib.tpw_fr_fill.argtypes = None, [C.c_int32, C.c_int32]
    lib.tpw_fr_view.restype, lib.tpw_fr_view.argtypes = None, [C.c_uint64]
    lib.tpw_fr_init.restype, lib.tpw_fr_init.argtypes = None, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_fr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.tpw_fr_init()
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build_ride(s, h, d):
    a = s.blk[h]
    s.put16(a + s.off("id"), d["id"])
    s.w32(a + s.off("cost"), d["cost"])
    s.w32(a + s.off("typestr"), d["typestr"])
    s.w32(a + s.off("flag"), d["flag"])
    s.w32(a + s.off("uitype"), d["uitype"])
    s.w32(a + s.off("failed"), d["failed"])
    s.w32(a + s.off("typeno"), d["typeno"])


def build(s, case):
    s.wptr(s.g("gameworld"), WORLD)
    s.wptr(s.g("panel"), s.blk[H_PANEL] if case["panel"] else 0)
    s.w32(s.g("ltype"), case["ltype"])
    s.w32(s.g("bal"), case["balance"])
    s.wptr(s.g("textptr"), s.blk[H_TABLE])
    s.put16(s.blk[H_VIEW] + s.off("z"), case["z"])
    s.setup_vt(s.blk[H_LABEL])
    for i, d in enumerate(case["rides"]):
        build_ride(s, RH[i], d)
    for i, c in enumerate(case["ctrls"]):
        s.w32(s.blk[CH[i]] + 4, c["upg"])
        s.put8(s.blk[CH[i]] + 0x10, c["avail"])
    p = s.blk[H_PROFILE]
    s.words(p, case["profile"])
    s.put16(s.blk[H_ARG], case["kind"] & 0xFFFF)
    s.put16(s.blk[H_ARG] + 2, case["junk"])


def execute(s, case):
    if case["op"] == "fill":
        return s.fill(case["type"], case["refresh"])
    return s.view(s.blk[H_ARG])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "ltype": s.rd_word(s.g("ltype"))}


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


# ---------------------------------------------------------------------------------------------------------------- case generators


def new_ride(r, rid, uitype=None, p_pass=0.6):
    ok = r.random() < p_pass
    return {"id": rid, "failed": 0 if ok or r.random() < 0.6 else r.choice([1, -1, 0x100]),
            "uitype": uitype if (ok or r.random() < 0.5) and uitype is not None else r.choice([0, 1, 2, 3, 4, -1]),
            "flag": 0 if ok or r.random() < 0.6 else r.choice([1, -1, 7]),
            "cost": r.choice([0, 0, 1, 5, -3, 100, -1, 1 << 30]),
            "typestr": r.randrange(0, 200), "typeno": r.randrange(0, 1 << 32) if r.random() < 0.3 else r.randrange(0, 50)}


def base(r, op):
    c = {"op": op, "panel": 1, "ltype": r.randrange(-2, 6), "balance": r.choice([0, 7, -5, 123456, r.randrange(1 << 32)]),
         "z": r.choice([0, 1, -1, 5, 0x100, -300, 0x7FFF, -0x8000, r.randrange(-1000, 1000)]), "rides": [], "ctrls": [],
         "profile": [0] * 258, "kind": 0, "junk": r.randrange(0x10000), "type": 0, "refresh": 0}
    c["queues"] = {}
    return c


def fill_case(r, t=None, refresh=None, panel=1, lst=1, n=None, p_pass=0.6, last_at=None, bought=None, inpark=None):
    c = base(r, "fill")
    t = r.choice([0, 1, 2, 3, 3, r.randrange(-1, 6)]) if t is None else t
    n = r.randrange(0, 5) if n is None else n
    c["type"] = t
    c["refresh"] = r.choice([0, 1]) if refresh is None else refresh
    c["panel"] = panel
    ids = r.sample(range(1, 0x400), n)
    if n and r.random() < 0.1:
        ids[0] = 0xFFFF
    q = {"GetParkAnalyser": [H_PARK], "GetWindow:504": [H_LIST if lst else 0],
         "GetFirstRide": [RH[0] if n else 0], "GetNextRide": [RH[i] for i in range(1, n)] + [0]}
    for i in range(n):
        d = new_ride(r, ids[i], t, p_pass)
        c["rides"].append(d)
        c["ctrls"].append({"upg": r.choice([0, 5, -7, 1000, r.randrange(-(1 << 31), 1 << 31)]), "avail": r.choice([0, 1, 1, 1, 255])})
        q["GetObjectControl:%d" % ids[i]] = [CH[i]]
        q["HasBeenBought:%d" % ids[i]] = [r.choice([0, 1]) if bought is None else bought]
        q["NumberOfObjectInPark:%d" % ids[i]] = [r.choice([0, 0, 1, 3, -1]) if inpark is None else inpark]
    pool = ids + [0, r.randrange(0x400)]
    for b in range(3):
        q["Last:%d" % b] = [r.choice(pool)]
    if last_at is not None and n:
        q["Last:%d" % last_at[0]] = [ids[last_at[1] % n]]
    c["queues"] = q
    return c


def view_case(r, kind=None, view=1, surface=1, locked=None, rows=None, cols=None):
    c = base(r, "uvw")
    kind = r.choice([-1, -2, -3, 0, 1, 2, 0x64, 0x65, 0x66, 5, 0x1234, 0x7FFF, -0x8000, r.randrange(-300, 300)]) if kind is None else kind
    c["kind"] = kind
    ids = [r.randrange(1, 0x1000) for _ in range(4)]
    c["rides"] = []
    for i in range(4):
        d = new_ride(r, ids[i])
        if i == 0 and kind >= 0 and locked is not None:
            d["cost"] = r.choice([1, 5, 100]) if locked else r.choice([0, -2, 3])
        c["rides"].append(d)
    w = r.choice([0, 1, 7, 8, 9, 16, 33, 64, 100, 255, -1, -9, -17, r.randrange(-40, 300)])
    h = r.choice([0, 1, 7, 8, 9, 16, 33, 64, 100, 255, -1, -9, -17, r.randrange(-40, 300)])
    rows = r.choice([0, 1, 2, 3, 4, 6, 16]) if rows is None else rows
    cols = r.choice([0, 1, 2, 3, 4, 6, 16]) if cols is None else cols
    prof = [0] * 258
    for i in range(256):
        prof[i] = r.choice([0, 0, 1, 2, 3, 4, 5, -1, -2, 6, 100, r.randrange(-(1 << 31), 1 << 31)])
    prof[256], prof[257] = rows, cols
    c["profile"] = prof
    kid = kind & 0xFFFF
    q = {"GetWindow:490": [H_VIEW if view else 0], "GetWindow:491": [H_PIC], "GetWindow:492": [H_LABEL],
         "Surface": [H_PIXELS if surface else 0], "SurfW": [w], "SurfH": [h], "Profile": [H_PROFILE],
         "GetConstString:%d" % (c["rides"][0]["typestr"] & MASK): [H_CONST]}
    for i in range(4):
        q["GetConstString:%d" % c["rides"][i]["typestr"]] = [H_CONST]
    q["GetRideInfo:%d" % kid] = [RH[0] if (kind >= 0 or r.random() < 0.5) else 0]
    for i, k in ((1, 0x64), (2, 0x65), (3, 0x66)):
        q["GetRideInfo:%d" % k] = [RH[i]]
    for i in range(4):
        q["HasBeenBought:%d" % c["rides"][i]["id"]] = [r.choice([0, 1])]
    c["queues"] = q
    return c


def fill_edges():
    r = random.Random(5363)
    for t in (0, 1, 2, 3, -1, 4):
        for refresh in (0, 1):
            for n in (0, 1, 3):
                yield fill_case(r, t=t, refresh=refresh, n=n, p_pass=1.0)
    yield fill_case(r, panel=0)
    yield fill_case(r, lst=0)
    yield fill_case(r, t=3, lst=0)
    for k in range(3):
        for bought in (0, 1):
            yield fill_case(r, t=r.choice([0, 1, 2, 3]), n=3, p_pass=1.0, last_at=(k, k + 1), bought=bought, inpark=0)
    yield fill_case(r, n=4, p_pass=1.0, inpark=2)
    yield fill_case(r, n=4, p_pass=0.0)


def fill_random(r):
    return fill_case(r, p_pass=r.choice([0.3, 0.8, 1.0]))


def view_edges():
    r = random.Random(5364)
    for kind in (-1, -2, -3, 0, 1, 2, 0x64, 0x65, 0x66, 0x7FFF, -0x8000):
        for surface in (0, 1):
            for locked in (True, False):
                yield view_case(r, kind=kind, surface=surface, locked=locked, rows=3, cols=4)
    yield view_case(r, kind=3, view=0)
    yield view_case(r, kind=3, locked=False, rows=16, cols=16)
    yield view_case(r, kind=-1, locked=False, rows=0, cols=0)
    for _ in range(6):
        yield view_case(r, kind=4, locked=False, rows=r.randrange(1, 8), cols=r.randrange(1, 8))


def view_random(r):
    return view_case(r, locked=r.choice([None, True, False]))


TESTS = [
    FnTest("FillRideList", ppc, host, edge=fill_edges, rnd=fill_random),
    FnTest("UpdateViewWindow", ppc, host, edge=view_edges, rnd=view_random),
]
