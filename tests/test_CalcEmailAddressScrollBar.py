"""Unit v5017, the online outbox panel's helpers, against libtpw_unit_CalcEmailAddressScrollBar.dylib
(tests/unit_CalcEmailAddressScrollBar.cpp), main app ("SimTheme Park"), src/UI_IslandLobby_CalcEmailAddressScrollBar.cpp:
  FillOutboxList 0x101e378c, SetCurrentEmailInfo 0x101e3960, CalcEmailAddressScrollBar 0x101e3aac, UpdatePreview 0x101e3c94

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. Every callee outside the unit is a fake on both
sides that records its arguments (window lookups and calls, the outbox cursor, the list control, the text measuring, the string and
conversion functions, the bitmap and pixel classes, the JPEG reader); the numbers they answer come from the case (the "ctl" words).
The windows are fixed blocks (their vtable slots 0x24 and 0x2c are the SetValue and SetText virtual calls).

The world of a case: the user info (made by the original when the global is NULL: then its constructor fake fills the outbox words), the
outbox words read by Mac offset (the current mail at +8, the list at +0x10 with its mail nodes, the preview mail at +0x18, three numbers at
+0x1c), the gText table pointer, the screen size globals, the texture format, the preview size, the cursor. What is compared: the event
log, the postcard text buffer and the conversion buffer (whole), the cursor (relative to the outbox) and whether the user info was made.

No fused operations. Not tested: a screen height of 0 or below (the original divides by it), a count of mails that is huge (the loop of
Next calls), a window id the original cannot find (GetWindow's answer NULL is dereferenced).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CalcEmailAddressScrollBar"
MASK = 0xFFFFFFFF

FILL, SETCUR, CALC, PREV = 0x101E378C, 0x101E3960, 0x101E3AAC, 0x101E3C94
SIZES = {FILL: 428, SETCUR: 288, CALC: 440, PREV: 480}
OWN = set(SIZES)

GETWINDOW, GETCLIENTRECT, SHOW, SETMINMAX = 0x101DBE40, 0x101DBA4C, 0x101DC1EC, 0x101D6B44
GETCOS, FINISH, MEASURE, PTRGLUE = 0x101DBC98, 0x101DBD1C, 0x1018B8E4, 0x1023BAA8
CTOR_USER, FIRST, NEXT = 0x1017DE54, 0x10205718, 0x1020578C
MBWC, WCSCPY, WCSCAT, FORMAT = 0x10238770, 0x10238C80, 0x10239370, 0x10239730
ADDITEM, UPDATEDISPLAY = 0x101D3000, 0x101D2220
PB_CTOR, PB_DTOR, RESIZE = 0x100BF7C4, 0x100BF84C, 0x100C0050
UP_CTOR4, UP_CTOR6, UP_GETDATA, UP_CONVERT, UP_DTOR = 0x102393B8, 0x102390B8, 0x102393D0, 0x102390E8, 0x10239100
JPEG_CTOR, JPEG_DTOR, RA_CTOR, READJPEG = 0x102393A0, 0x102394C0, 0x10238E30, 0x101E538C

ADDR = dict(gUserInfo=0x10380B34, gText=0x1037FF18, gei=0x103BC920, gOutboxWindow=0x10395E3C, gOutboxCursor=0x10395E38,
            gOutboxPreview=0x10395E10, post=0x103952EC, conv=0x10380C48, height=0x10395BBA, width=0x10395BBC)
MAC_LAY = dict(ob=0x64, usize=0xD4, table=0x564, tex=4, ptr=4)
MARK_SETVALUE, MARK_SETTEXT = 0x7A24, 0x7A2C

WIN = []       # the Mac addresses of the five window blocks (persistent)
CUR = [None]   # the PowerPC side of the case being run


def s32(v):
    return T.s32(v)


def th(units):
    h = 17
    for u in units:
        h = (h * 31 + u) & MASK
    return len(units), h


def ch(data):
    h = 17
    for b in data:
        h = (h * 31 + b) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def rd16(e, a):
    out = []
    while True:
        chunk = struct.unpack(">32H", e.read(a + 2 * len(out), 64))
        for u in chunk:
            if u == 0:
                return out
            out.append(u)


def rdc(e, a):
    out = bytearray()
    while True:
        chunk = e.read(a + len(out), 32)
        for b in chunk:
            if b == 0:
                return bytes(out)
            out.append(b)


def wr16(e, a, units):
    e.write(a, struct.pack(">%dH" % (len(units) + 1), *units, 0))


def rec(e, name, *vals):
    e.cl.rec(name, tuple(vals))


def user_ob(e):
    ui = e.u32(ADDR["gUserInfo"])
    return ui + MAC_LAY["ob"]


def stub(addr):
    def deco(fn):
        if addr >= 0x10238000 and addr != PTRGLUE:
            # a glue import: the glue saves the caller's TOC pointer at 0x14(r1) and the caller reloads r2 from there afterwards
            def with_toc(e, fn=fn):
                e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
                return fn(e)
            STUBS.internal(addr, with_toc)
        else:
            STUBS.internal(addr, fn)
        return fn
    return deco


@stub(GETWINDOW)
def st_getwindow(e):
    wid = s32(e.r(4))
    rec(e, "GetWindow", e.cl.lab(e.r(3)), wid)
    index = {0xEE2: 1, 0xEE3: 2, 0xEE4: 3, 0xEE8: 4}.get(wid)
    e.set_r(3, WIN[index] if index is not None else 0)


@stub(GETCLIENTRECT)
def st_getclientrect(e):
    ctl = CUR[0].c["ctl"]
    rec(e, "GetClientRect", e.cl.lab(e.r(3)))
    e.write(e.r(4) + 4, struct.pack(">hhhh", T.s16(ctl[0]), T.s16(ctl[1]), T.s16(ctl[2]), T.s16(ctl[3])))
    e.set_r(3, 0)


@stub(SHOW)
def st_show(e):
    rec(e, "Show", e.cl.lab(e.r(3)), T.s16(e.r(4)))
    e.set_r(3, 0)


@stub(SETMINMAX)
def st_setminmax(e):
    rec(e, "SetMinMax", e.cl.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)))


@stub(GETCOS)
def st_getcos(e):
    ctl = CUR[0].c["ctl"]
    rec(e, "GetClientObjectSurface", e.cl.lab(e.r(3)))
    e.write(e.r(4), struct.pack(">I", ctl[5] & MASK))
    e.write(e.r(5), struct.pack(">I", ctl[6] & MASK))
    e.set_r(3, ctl[4] & MASK)


@stub(FINISH)
def st_finish(e):
    rec(e, "FinishWithClientObjectSurface", e.cl.lab(e.r(3)))


@stub(PTRGLUE)
def st_ptrglue(e):
    ctl = CUR[0].c["ctl"]
    marker = e.r(12)
    if marker == MARK_SETVALUE:
        rec(e, "SetValue", e.cl.lab(e.r(3)), s32(e.r(4)))
        e.set_r(3, ctl[13] & MASK)
    elif marker == MARK_SETTEXT:
        n, h = th(rd16(e, e.r(4)))
        rec(e, "SetText", e.cl.lab(e.r(3)), int(e.r(4) == ADDR["post"]), n, h)
        e.set_r(3, 1)
    else:
        raise AssertionError("unknown virtual call %x" % marker)


@stub(MEASURE)
def st_measure(e):
    ctl = CUR[0].c["ctl"]
    n, h = th(rd16(e, e.r(3)))
    rect = struct.unpack(">4i", e.read(e.r(4), 16))
    rec(e, "UI_MeasureText", int(e.r(3) == ADDR["post"]), n, h, *rect, T.s8(e.r(5)), T.s8(e.r(6)), T.s8(e.r(7)), s32(e.r(8)),
        int(e.r(9) != 0))
    if e.r(9):
        e.write(e.r(9), struct.pack(">4I", *[ctl[i] & MASK for i in (9, 10, 11, 12)]))


@stub(CTOR_USER)
def st_ctor_user(e):
    ctl = CUR[0].c["ctl"]
    rec(e, "CFSUserInfo")
    ob = e.r(3) + MAC_LAY["ob"]
    e.write(ob + 8, struct.pack(">I", ctl[14] & MASK))
    e.write(ob + 0x14, struct.pack(">I", ob + 0x10))
    e.write(ob + 0x18, struct.pack(">I", ctl[15] & MASK))
    e.write(ob + 0x1C, struct.pack(">3I", ctl[16] & MASK, ctl[17] & MASK, ctl[18] & MASK))


@stub(0x101C7448)
def st_cr_ctor(e):
    pass    # CoolRect::CoolRect: records nothing (the rectangle is filled by GetClientRect); the host runs the real one


@stub(0x101C74E0)
def st_cr_dtor(e):
    pass    # CoolRect::~CoolRect


@stub(FIRST)
def st_first(e):
    rec(e, "OutboxFirst", int(e.r(3) == user_ob(e) and e.u32(ADDR["gUserInfo"]) != 0))


@stub(NEXT)
def st_next(e):
    rec(e, "OutboxNext", int(e.r(3) == user_ob(e) and e.u32(ADDR["gUserInfo"]) != 0))
    e.set_r(3, e.cl.ret("Next") & 1)


@stub(MBWC)
def st_mbwc(e):
    src = rdc(e, e.r(5))
    size = s32(e.r(8))
    rec(e, "MultiByteToWideChar", e.r(3), e.r(4), ch(src), len(src), s32(e.r(6)), int(e.r(7) == ADDR["conv"]), size)
    n = 0
    units = []
    while n < len(src) and n < size - 1:
        units.append(src[n])
        n += 1
    wr16(e, e.r(7), units)
    e.set_r(3, n + 1)


@stub(WCSCPY)
def st_wcscpy(e):
    src = rd16(e, e.r(4))
    n, h = th(src)
    rec(e, "wcscpy", int(e.r(3) == ADDR["post"]), n, h)
    wr16(e, e.r(3), src)


@stub(WCSCAT)
def st_wcscat(e):
    src = rd16(e, e.r(4))
    n, h = th(src)
    rec(e, "wcscat", int(e.r(3) == ADDR["post"]), n, h)
    dest = rd16(e, e.r(3))
    wr16(e, e.r(3), dest + src)


@stub(FORMAT)
def st_format(e):
    table = e.u32(ADDR["gText"] + MAC_LAY["table"])
    tags = []
    for reg in (7, 8, 9):
        a = e.r(reg)
        tags.append((e.u32(a), e.u32(a + 8), s32(e.u32(a + 12))))
    rec(e, "FormatString", int(e.r(3) == table), e.r(5), e.r(6), *[x for tag in tags for x in tag])
    wr16(e, e.r(4), [0x41 + (tags[0][2] & 15), 0x41 + (tags[1][2] & 15), 0x41 + (tags[2][2] & 15)])
    e.set_r(3, 0)


@stub(ADDITEM)
def st_additem(e):
    w = struct.unpack(">4I", e.read(e.r(4), 16))
    t0, t1 = rd16(e, w[0]), rd16(e, w[1])
    rec(e, "AddItem", e.cl.lab(e.r(3)), int(w[0] == ADDR["conv"]), th(t0)[0], th(t0)[1], th(t1)[0], th(t1)[1], s32(w[2]), s32(w[3]),
        s32(e.r(5)), s32(e.r(6)), s32(e.r(7)))
    e.set_r(3, 0)


@stub(UPDATEDISPLAY)
def st_updatedisplay(e):
    rec(e, "UpdateDisplay", e.cl.lab(e.r(3)))


@stub(PB_CTOR)
def st_pb_ctor(e):
    ctl = CUR[0].c["ctl"]
    rec(e, "PostcardBitmap", e.r(4), e.r(5), e.r(6))
    e.write(e.r(3) + 0xC, struct.pack(">I", ctl[7] & MASK))


@stub(PB_DTOR)
def st_pb_dtor(e):
    rec(e, "~PostcardBitmap")


@stub(RESIZE)
def st_resize(e):
    ctl = CUR[0].c["ctl"]
    rec(e, "resize_into_buffer", int(e.r(4) == (ctl[4] & MASK)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7)), int(e.u32(e.r(3) + 0xC) != 0))
    e.set_r(3, 1)


def ordinal(e, a):
    ords = CUR[0].ords
    return ords.index(a) if a in ords else -1


@stub(UP_CTOR4)
def st_up4(e):
    CUR[0].ords.append(e.r(3))
    rec(e, "UPixels4", s32(e.r(4)), s32(e.r(5)), s32(e.r(6)), int(e.r(7) != 0))


@stub(UP_CTOR6)
def st_up6(e):
    ctl = CUR[0].c["ctl"]
    CUR[0].ords.append(e.r(3))
    rec(e, "UPixels6", s32(e.r(4)), int(e.r(5) == (ctl[7] & MASK)), s32(e.r(6)), s32(e.r(7)), s32(e.r(8)), int(e.r(9) != 0))


@stub(UP_DTOR)
def st_updtor(e):
    rec(e, "~UPixels", ordinal(e, e.r(3)))


@stub(UP_GETDATA)
def st_getdata(e):
    ctl = CUR[0].c["ctl"]
    rec(e, "GetData", ordinal(e, e.r(3)))
    e.set_r(3, ctl[8] & MASK)


@stub(UP_CONVERT)
def st_convert(e):
    rec(e, "Convert", ordinal(e, e.r(3)), ordinal(e, e.r(4)), e.r(5) & 0xFF)


@stub(JPEG_CTOR)
def st_jpeg_ctor(e):
    rec(e, "TbJPEGLoader")


@stub(JPEG_DTOR)
def st_jpeg_dtor(e):
    rec(e, "~TbJPEGLoader")


@stub(RA_CTOR)
def st_ra_ctor(e):
    rec(e, "TbRenderArea")


@stub(READJPEG)
def st_readjpeg(e):
    ctl = CUR[0].c["ctl"]
    rec(e, "read_JPEG_file", ch(rdc(e, e.r(3))), int(e.r(4) == (ctl[8] & MASK)))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    vt = bytearray(0x100)
    struct.pack_into(">I", vt, 0x24, MARK_SETVALUE)
    struct.pack_into(">I", vt, 0x2C, MARK_SETTEXT)
    vtable = T.persist_alloc(e, 0x100, bytes(vt))
    for _ in range(5):
        block = bytearray(0x200)
        struct.pack_into(">I", block, 0x118, vtable)
        WIN.append(T.persist_alloc(e, 0x200, bytes(block)))


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def sc(self, off):
        return off * (2 if self.fmt == "<I" else 1)

    def slot(self, base, off, v):
        # a Mac word: 4 bytes at the Mac offset on the PowerPC, an 8-byte slot at twice the offset on the host
        if self.fmt == "<I":
            self.write(base + 2 * off, struct.pack("<Q", v))
        else:
            self.words(base + off, [v])

    def u16s(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def write16(self, a, units):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(units), *units))

    def rd_word(self, a):
        return struct.unpack(self.fmt, self.read(a, 4))[0]


class Ppc(Side):
    fmt = ">I"
    lay = MAC_LAY

    def __init__(self, e, case):
        self.e = e
        self.c = case
        self.ords = []
        CUR[0] = self
        self.L = e.cl = CL.PpcLog({WIN[i]: i + 1 for i in range(5)}, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        if name == "gei":
            return self.e.u32(0x10260B84)    # the data import: the emulator's TOC slot points at the import's own storage
        return ADDR[name]

    def win(self, i):
        return WIN[i]

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def wptr(self, a, v):
        self.words(a, [v])

    def rptr(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def call(self, op):
        entry = {"fill": FILL, "setcur": SETCUR, "calc": CALC, "prev": PREV}[op]
        args = [self.c["number"] & MASK] if op == "setcur" else []
        T.call_ppc(self.e, entry, args)


LAY = {}


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        self.c = case
        bind(lib)
        self.lay = LAY
        lib.tpw_ob_reset()
        CL.host_begin(lib, {i + 1: lib.tpw_ob_addr(b"win%d" % i) for i in range(5)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ob_addr(name.encode())

    def win(self, i):
        return self.lib.tpw_ob_addr(b"win%d" % i)

    def alloc(self, n):
        return self.lib.tpw_ob_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def wptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        lib = self.lib
        if op == "fill":
            lib.tpw_ob_fill()
        elif op == "setcur":
            lib.tpw_ob_setcur(s32(self.c["number"]))
        elif op == "calc":
            lib.tpw_ob_calc()
        else:
            lib.tpw_ob_preview()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ob_addr.restype, lib.tpw_ob_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ob_alloc.restype, lib.tpw_ob_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ob_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ob_setcur.argtypes = [C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ob_layout(out)
    LAY.update(ob=out[0], usize=out[1], table=out[2], tex=out[3], ptr=8)
    assert n == 4
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, c):
    lay = s.lay
    s.write16(s.g("post"), c["post"])
    s.write16(s.g("conv"), [0x1111] * 0x4000)
    s.write(s.g("height"), struct.pack(s.fmt[0] + "h", c["H"]))
    s.write(s.g("width"), struct.pack(s.fmt[0] + "h", c["W"]))
    s.wptr(s.g("gOutboxWindow"), s.win(0))
    s.words(s.g("gOutboxPreview"), c["preview"])
    s.words(s.g("gei") + lay["tex"], [c["texfmt"]])
    s.wptr(s.g("gText") + lay["table"], c["table"])

    def entry(text):
        t = s.alloc(len(text) + 1)
        s.write(t, text + b"\0")
        e = s.alloc(0x40)
        s.slot(e, 0xC, t)
        return e

    e1, e2 = entry(c["t1"]), entry(c["t2"])
    ctl = list(c["ctl"])
    ctl[14], ctl[15] = e1 & MASK, e2 & MASK
    ctl[19], ctl[20] = e1 >> 32, e2 >> 32
    ctl[16:19] = c["nums"]
    c["ctl_final"] = ctl
    if s.c.get("host"):
        pass
    if c["user"] == "null":
        s.wptr(s.g("gUserInfo"), 0)
        s.wptr(s.g("gOutboxCursor"), 0)
    else:
        u = s.alloc(lay["usize"] + 0x80)
        ob = u + lay["ob"]
        sentinel = ob + s.sc(0x10)
        nodes = [s.alloc(0x40) for _ in c["mails"]]
        for i, node in enumerate(nodes):
            s.slot(node, 4, nodes[i + 1] if i + 1 < len(nodes) else sentinel)
            s.slot(node, 8, entry(c["mails"][i]))
        s.slot(ob, 8, e1)
        s.slot(ob, 0x14, nodes[0] if nodes else sentinel)
        s.slot(ob, 0x18, e2)
        for k, n in enumerate(c["nums"]):
            s.slot(ob, 0x1C + 4 * k, n)
        s.wptr(s.g("gUserInfo"), u)
        s.wptr(s.g("gOutboxCursor"), ob + s.sc(4) if c["op"] == "prev" else 0)
    if isinstance(s, Host):
        s.words(s.g("ctl"), ctl)


def observe(s, c):
    lay = s.lay
    if c["op"] == "prev":
        pass
    ui = s.rptr(s.g("gUserInfo"))
    cur = s.rptr(s.g("gOutboxCursor"))
    return {"log": s.log(), "post": s.u16s(s.g("post"), 0x200), "conv": s.u16s(s.g("conv"), 0x4000),
            "cursor": (((cur - ui - lay["ob"]) & MASK) // s.sc(1)) if cur else -1, "user": int(ui != 0)}


def run(s, c):
    try:
        build(s, c)
        s.c["ctl"] = c["ctl_final"]       # the PowerPC stubs read the finished words
        s.call(c["op"])
        return observe(s, c)
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def txt(r):
    n = r.randrange(0, 7)
    return bytes(r.randrange(0x80, 0x100) if r.random() < 0.1 else r.randrange(0x21, 0x7F) for _ in range(n))


def u32(r):
    return r.choice([1, 0x1000, 0x7FFFFFFF, 0x80000000, r.randrange(1, 1 << 32)])


def base(r, op, **kw):
    c = {"op": op}
    c["user"] = r.choice(["set", "set", "set", "null"])
    c["t1"], c["t2"] = txt(r), txt(r)
    c["nums"] = [r.choice([0, 1, MASK, r.randrange(0, 1 << 32), r.randrange(0, 100)]) for _ in range(3)]
    c["mails"] = [txt(r) for _ in range(r.randrange(0, 4))]
    c["post"] = [0x2222] * 0x1FF + [0]    # terminated: the measure and SetText fakes read it as a string
    c["H"] = r.choice([1, 2, 100, 768, 1024, 2048, 0x7FFF, r.randrange(1, 4000)])
    c["W"] = r.choice([1, 640, 1024, 0x7FFF, r.randrange(1, 4000)])
    c["table"] = u32(r)
    top = r.randrange(-500, 500)
    left = r.randrange(-500, 500)
    bottom = top + r.randrange(-10, 1500)
    right = left + r.randrange(-10, 2000)
    shown = bottom - top
    mode = r.random()
    if mode < 0.35:
        size_bottom = max(0, (shown * c["H"]) // 0x600 + r.choice([-1, 0, 0, 1, 2]))
    else:
        size_bottom = r.randrange(0, 30000)
    ctl = [0] * 32
    ctl[0:4] = [left & MASK, top & MASK, right & MASK, bottom & MASK]
    ctl[4] = r.choice([0, u32(r)])
    ctl[5], ctl[6] = r.randrange(0, 800), r.randrange(0, 800)
    ctl[7], ctl[8] = u32(r), u32(r)
    ctl[9:13] = [r.randrange(0, 50), r.randrange(0, 50), r.randrange(0, 5000), size_bottom]
    ctl[13] = r.choice([0, 1, 2, 0x10000, 0xFFFF, MASK, 0x20000])
    c["ctl"] = ctl
    c["texfmt"] = r.choice(range(-1, 9))
    c["preview"] = [0] * 7 + [r.randrange(0, 0x400), r.randrange(0, 0x400)]
    c["number"] = r.choice([0, 1, 2, 3, 0x01000002, 0x02000001, 0x03000003, 0x02000000])
    ones = r.randrange(0, 4)
    c["queues"] = {"Next": [1] * ones + [0]}
    c.update(kw)
    if op == "prev":
        c["user"] = "set"
    return c


def make(op):
    def rnd(r):
        return base(r, op)

    def edges():
        r = random.Random({"fill": 11, "setcur": 12, "calc": 13, "prev": 14}[op])
        if op == "fill":
            for user in ("null", "set"):
                for ones in range(4):
                    for mails in (0, 2):
                        c = base(r, op, user=user, queues={"Next": [1] * ones + [0]})
                        c["mails"] = [txt(r) for _ in range(mails)]
                        yield c
        elif op == "setcur":
            for user in ("null", "set"):
                for number in (0, 1, 2, 0x01000001, 0x03000002, 0x02000000):
                    for mails in (0, 1, 3):
                        c = base(r, op, user=user, number=number)
                        c["mails"] = [txt(r) for _ in range(mails)]
                        yield c
        elif op == "calc":
            for answer in (0, 1, 0x10000, MASK):
                for delta in (-1, 0, 1, 5):
                    c = base(r, op)
                    c["H"] = r.choice([1, 768, 2048])
                    shown = (c["ctl"][3] - c["ctl"][1]) & 0xFFFF
                    shown = shown - 0x10000 if shown >= 0x8000 else shown
                    c["ctl"][12] = max(0, (shown * c["H"]) // 0x600 + delta)
                    c["ctl"][13] = answer
                    yield c
        else:
            for fmt in range(-1, 9):
                for surface in (0, 0x5000):
                    c = base(r, op, texfmt=fmt)
                    c["ctl"][4] = surface
                    yield c

    return edges, rnd


TESTS = []
for _name, _op in (("FillOutboxList", "fill"), ("SetCurrentEmailInfo", "setcur"), ("CalcEmailAddressScrollBar", "calc"),
                   ("UpdatePreview", "prev")):
    _edges, _rnd = make(_op)
    TESTS.append(FnTest(_name, ppc, host, edge=_edges, rnd=_rnd))
