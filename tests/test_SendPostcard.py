"""Unit v5884, the postcard panel's sending half, against libtpw_unit_SendPostcard.dylib (tests/unit_SendPostcard.cpp), main app
("SimTheme Park"), src/ui_Postcard_panel_SendPostcard.cpp:
  IllCallYouBack 0x101b0ca4, AddressesValid 0x101b0d88, AddToAddressField 0x101b0e98, SendPostcard 0x101b0f9c

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees
outside the unit (AddUIError, UIMain_GetString, GetWindow, the virtual SetText, CopyFile, the e-mail, string, list, login and message
centre callees, operator new, __construct_array) and the C string functions (sprintf, strtok, swprintf, wcs*) are fakes on both sides;
the ones that matter record their arguments (texts as FNV-1a hashes, pointers as labels), the others are pure and have the C semantics.

The world of a case: the text globals (temp path, To text, address), the counter, the three lazily made objects (present or not), the
online configuration (mail server and port), the login strings, the answer queues. What is compared: the answer, the event log, the
text globals, the counter, the jpg path buffer, the labels of the three global pointers and the whole Email block.

No fused operations. Not tested: strings longer than the original's stack buffers (it overflows them).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_SendPostcard"
MASK = 0xFFFFFFFF

FN_ILL, FN_VALID, FN_ADDTO, FN_SEND = 0x101B0CA4, 0x101B0D88, 0x101B0E98, 0x101B0F9C
SIZES = {FN_ILL: 152, FN_VALID: 232, FN_ADDTO: 216, FN_SEND: 1236}
OWN = set(SIZES)

ADDR = dict(gTempPath=0x10394E26, gPostcardAddress=0x1039522A, gPostcardText5da5=0x103952EC, gPostcardJpgPath=0x10394C26,
            gPostcardCounter=0x10394B18, gPostcard=0x103956F4, gPostcardPanel=0x103956FC, gOnlineSystem=0x10380AB0,
            gUserInfo=0x10380B34, gMessageCentre=0x1037F6C4, gOnlineConfig=0x102B2B70)
VEC_ILL, VEC_CTOR, VEC_DTOR = 0x10267CF8, 0x102657E8, 0x10267288
HOST_LAYOUT = [0x4C, 0x10, 0x14, 4, 8, 0x18, 0x20, 0x24, 0x28, 0x48, 0xC0, 0x400]

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


def s16(v):
    return T.s16(v)


def fnv(units):
    h = 2166136261
    for u in units:
        h ^= u
        h = (h * 16777619) & MASK
    return h


def cbytes(e, a):
    out = b""
    while True:
        chunk = e.read(a + len(out), 32)
        i = chunk.find(b"\0")
        if i >= 0:
            return out + chunk[:i]
        out += chunk


def wunits(e, a):
    out = []
    while True:
        chunk = struct.unpack(">16H", e.read(a + 2 * len(out), 32))
        for i, u in enumerate(chunk):
            if u == 0:
                return out + list(chunk[:i])
        out += list(chunk)


def put_w(e, a, units):
    e.write(a, struct.pack(">%dH" % (len(units) + 1), *(list(units) + [0])))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes


def rec(e, name, *vals):
    e.cl.rec(name, tuple(vals))


def off(e, a):
    return (a - e.cl.email) & MASK


def f_add_ui_error(e):
    rec(e, "AddUIError", s32(e.r(3)), e.cl.lab(e.r(4)))


def f_getstring(e):
    rec(e, "UIMain_GetString", s32(e.r(3)))
    e.set_r(3, e.cl.strbuf + 0x300)


def f_wcscmp(e):
    a, b = wunits(e, e.r(3)), wunits(e, e.r(4))
    rec(e, "wcscmp", fnv(a), fnv(b))
    e.set_r(3, (0 if a == b else (-1 if a < b else 1)) & MASK)


def f_wcstok(e):
    L = e.cl
    s, d = e.r(3), wunits(e, e.r(4))
    rec(e, "wcstok", int(s == 0), fnv(d))
    if s == 0:
        s = L.wtok
    if s == 0:
        e.set_r(3, 0)
        return

    def rd(a):
        return struct.unpack(">H", e.read(a, 2))[0]
    while rd(s) != 0 and rd(s) in d:
        s += 2
    if rd(s) == 0:
        L.wtok = 0
        e.set_r(3, 0)
        return
    start = s
    while rd(s) != 0 and rd(s) not in d:
        s += 2
    if rd(s) == 0:
        L.wtok = 0
    else:
        e.write(s, b"\0\0")
        L.wtok = s + 2
    e.set_r(3, start)


def f_wcschr(e):
    a, c = e.r(3), e.r(4) & 0xFFFF
    while True:
        u = struct.unpack(">H", e.read(a, 2))[0]
        if u == c:
            e.set_r(3, a)
            return
        if u == 0:
            e.set_r(3, 0)
            return
        a += 2


def f_wcsstr(e):
    a, t, n = e.r(3), wunits(e, e.r(3)), wunits(e, e.r(4))
    for i in range(len(t) + 1):
        if t[i:i + len(n)] == n:
            e.set_r(3, a + 2 * i)
            return
    e.set_r(3, 0)


def f_wcscpy(e):
    d, s = e.r(3), wunits(e, e.r(4))
    put_w(e, d, s)


def f_wcscat(e):
    d, s = e.r(3), wunits(e, e.r(4))
    put_w(e, d + 2 * len(wunits(e, d)), s)


def f_wcslen(e):
    e.set_r(3, len(wunits(e, e.r(3))))


def f_strlen(e):
    e.set_r(3, len(cbytes(e, e.r(3))))


def f_swprintf(e):
    buf, size, args = e.r(3), e.r(4), 6
    f = wunits(e, e.r(5))
    out = []
    i = 0
    while i < len(f):
        if f[i] != ord("%"):
            out.append(f[i])
            i += 1
        elif f[i + 1:i + 3] == [ord("l"), ord("s")]:
            out += wunits(e, e.r(args))
            args += 1
            i += 3
        elif f[i + 1:i + 2] == [ord("s")]:
            out += list(cbytes(e, e.r(args)))
            args += 1
            i += 2
        else:
            out.append(f[i])
            i += 1
    if len(out) < size:
        put_w(e, buf, out)
        e.set_r(3, len(out))
    else:
        if size > 0:
            put_w(e, buf, out[:size - 1])
        e.set_r(3, MASK)


def f_sprintf(e):
    dst, fmt, args = e.r(3), cbytes(e, e.r(4)), 5
    out = b""
    i = 0
    while i < len(fmt):
        if fmt[i:i + 1] != b"%":
            out += fmt[i:i + 1]
            i += 1
        elif fmt[i + 1:i + 2] == b"s":
            out += cbytes(e, e.r(args))
            args += 1
            i += 2
        elif fmt[i + 1:i + 3] == b"ls":
            out += bytes(u & 0xFF for u in wunits(e, e.r(args)))
            args += 1
            i += 3
        elif fmt[i:i + 4] == b"%08d":
            out += b"%08d" % s32(e.r(args))
            args += 1
            i += 4
        else:
            out += fmt[i:i + 1]
            i += 1
    e.write(dst, out + b"\0")
    rec(e, "Sprintf", fnv(fmt), fnv(out))
    e.set_r(3, len(out))


def f_strtok(e):
    L = e.cl
    s, delim = e.r(3), cbytes(e, e.r(4))
    if s == 0:
        s = L.tokpos
    if s == 0:
        e.set_r(3, 0)
        return

    def rd(a):
        return e.read(a, 1)[0]
    while rd(s) != 0 and rd(s) in delim:
        s += 1
    if rd(s) == 0:
        L.tokpos = 0
        e.set_r(3, 0)
        return
    start = s
    while rd(s) != 0 and rd(s) not in delim:
        s += 1
    if rd(s) == 0:
        L.tokpos = 0
    else:
        e.write(s, b"\0")
        L.tokpos = s + 1
    e.set_r(3, start)


def f_copyfile(e):
    rec(e, "CopyFile", fnv(cbytes(e, e.r(3))), fnv(cbytes(e, e.r(4))), s32(e.r(5)))
    e.set_r(3, 0)


def f_new(e):
    size = e.r(3)
    rec(e, "New", size)
    e.set_r(3, e.alloc(max(size, 4)))


_email_ctor = CL.recorder("EmailCtor", "N")


def f_email_ctor(e):
    e.cl.email = e.r(3)
    _email_ctor(e)


def f_string_assign(e):
    n = e.r(5)
    rec(e, "StringAssign", off(e, e.r(3)), fnv(e.read(e.r(4), n)) if n else fnv(b""), n)


def f_string_ctor(e):
    text = cbytes(e, e.r(4))
    e.cl.strs[e.r(3)] = text
    rec(e, "StringCtor", fnv(text))


def f_list_insert(e):
    rec(e, "ListInsert", off(e, e.r(4)), off(e, e.r(5)), fnv(e.cl.strs.get(e.r(6), b"")))


def f_string_dtor(e):
    rec(e, "StringDtor", fnv(e.cl.strs.pop(e.r(3), b"")), s16(e.r(4)))


def f_wstring_assign(e):
    n = e.r(5)
    units = list(struct.unpack(">%dH" % n, e.read(e.r(4), 2 * n))) if n else []
    rec(e, "WStringAssign", off(e, e.r(3)), fnv(units), n)


def f_login(name, delta):
    def f(e):
        rec(e, name, e.cl.lab(e.r(3)))
        e.set_r(3, e.cl.strbuf + delta)
    return f


def f_ret(name, key):
    def f(e):
        rec(e, name, e.cl.lab(e.r(3)))
        e.set_r(3, e.cl.ret(key) & MASK)
    return f


def f_attach(e):
    rec(e, "AttachData", e.cl.lab(e.r(3)), fnv(cbytes(e, e.r(4))))
    e.set_r(3, e.cl.ret("AttachData") & MASK)


def f_send(e):
    rec(e, "EmailSend", e.cl.lab(e.r(3)), fnv(cbytes(e, e.r(4))), s16(e.r(5)), e.cl.lab(e.r(6)), e.cl.lab(e.r(7)))
    e.set_r(3, e.cl.ret("Send") & MASK)


def f_sendtooutbox(e):
    info = e.u32(ADDR["gUserInfo"])
    rec(e, "SendToOutbox", e.cl.lab(e.r(3)), int(e.r(4) == (info + 0x64) & MASK))
    e.set_r(3, 0)


def f_msgevent(e):
    e.write(e.r(3) + 8, struct.pack(">I", e.r(4)))
    rec(e, "CMsgEvent", s32(e.r(4)))


def f_postmessage(e):
    rec(e, "PostMessage", e.cl.lab(e.r(3)), s32(e.u32(e.r(4) + 8)))
    e.set_r(3, 1)


def f_construct_array(e):
    ids = {VEC_CTOR: 1, VEC_DTOR: 0}
    rec(e, "ConstructArray", 1 if e.r(4) == VEC_CTOR else 0, 2 if e.r(5) == VEC_DTOR else 0, s32(e.r(6)), s32(e.r(7)))


def f_getwindow(e):
    rec(e, "GetWindow", e.cl.lab(e.r(3)), s32(e.r(4)))
    e.set_r(3, e.cl.window)


def f_windowsettext(e):
    rec(e, "WindowSetText", e.cl.lab(e.r(3)), fnv(wunits(e, e.r(4))))


def f_postcardsave(e):
    rec(e, "PostcardSave", e.cl.lab(e.r(3)), fnv(cbytes(e, e.r(4))))


def with_toc(fn):
    """The glue stores the caller's TOC at 0x14(r1) and the caller reloads r2 from there after the call."""
    def f(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return f


for _a, _f in [
    (0x101F1DF0, f_add_ui_error), (0x10180468, f_getstring), (0x10238C98, f_wcscmp), (0x1023A078, f_wcstok),
    (0x10238C68, f_wcschr), (0x1023A1E0, f_wcsstr), (0x10238C80, f_wcscpy), (0x10239370, f_wcscat), (0x102389C8, f_wcslen),
    (0x10238650, f_strlen), (0x10239160, f_swprintf), (0x10238578, f_sprintf), (0x1023A180, f_strtok), (0x1023A168, f_copyfile),
    (0x102384B8, f_new), (0x10202244, f_email_ctor), (0x101B2440, f_string_assign), (0x10002338, f_string_ctor),
    (0x101B25E0, f_list_insert), (0x1000221C, f_string_dtor), (0x101B2510, f_wstring_assign),
    (0x101718B8, f_login("LoginEmail", 0)), (0x10171834, f_login("LoginName", 0x100)), (0x10171878, f_login("LoginPassword", 0x200)),
    (0x101718FC, f_ret("LoginPlayerID", "PlayerID")), (0x1017193C, f_ret("LoginCustomerID", "CustomerID")),
    (0x10202950, f_attach), (0x10202EE4, f_send), (0x10203010, f_sendtooutbox), (0x10151DF8, f_msgevent),
    (0x1015256C, f_postmessage), (0x10238758, f_construct_array), (0x101DBE40, f_getwindow), (0x1023BAA8, f_windowsettext),
    (0x100C0C20, f_postcardsave), (0x1017178C, f_ret("AreWeLoggedIn", "AreWeLoggedIn")),
]:
    STUBS.internal(_a, with_toc(_f))
STUBS.internal(0x1016F7D0, with_toc(CL.recorder("OnlineSystemCtor", "N")))
STUBS.internal(0x1017DE54, with_toc(CL.recorder("UserInfoCtor", "N")))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

OBJECTS = ["card", "panel", "system", "info", "centre", "window"]    # labels 1 to 6


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def w16(self, a, units, total):
        units = list(units) + [0] * (total - len(units))
        self.write(a, struct.pack(self.fmt[0] + "%dH" % total, *units))

    def rd16(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.obj = {n: e.alloc(0x400) for n in OBJECTS}
        self.vt = e.alloc(0x40)
        self.words(self.obj["window"] + 0x118, [self.vt])
        self.L = e.cl = CL.PpcLog({self.obj[n]: i + 1 for i, n in enumerate(OBJECTS)}, case["queues"])
        self.L.email = 0
        self.L.strs = {}
        self.L.tokpos = 0
        self.L.wtok = 0
        self.saved = CL.STUBS_ACTIVATE(e) if False else STUBS.activate(e)

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

    def extras(self, strbuf):
        self.L.strbuf = strbuf
        self.L.window = self.obj["window"]

    def pointers(self, flags):
        o = self.obj
        for name, key, on in (("gPostcard", "card", True), ("gPostcardPanel", "panel", True), ("gOnlineSystem", "system", flags[0]),
                              ("gUserInfo", "info", flags[1]), ("gMessageCentre", "centre", flags[2])):
            self.words(ADDR[name], [o[key] if on else 0])

    def config(self, server, port):
        rep = self.alloc(0x10)
        data = self.alloc(len(server) + 1)
        self.write(data, server + b"\0")
        self.words(rep + 0xC, [data])
        self.words(ADDR["gOnlineConfig"], [rep])
        self.write(ADDR["gOnlineConfig"] + 0x18, struct.pack(">h", port))

    def labs(self):
        return [self.L.lab(self.rd_word(ADDR[n])) for n in ("gOnlineSystem", "gUserInfo", "gMessageCentre")]

    def email_words(self):
        if not self.L.email:
            return []
        w = self.rd_words(self.L.email, 0x13)
        w[0x12] = 1 if w[0x12] == VEC_ILL else w[0x12]
        return w

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK if entry == FN_VALID else 0     # the others answer nothing


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_sp_reset()
        self.obj = {n: lib.tpw_sp_alloc(0x400) for n in OBJECTS}
        CL.host_begin(lib, {i + 1: self.obj[n] for i, n in enumerate(OBJECTS)}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sp_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sp_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def extras(self, strbuf):
        self.lib.tpw_sp_set_extras(strbuf, self.obj["window"])

    def pointers(self, flags):
        o = self.obj
        self.lib.tpw_sp_set_ptrs(o["card"], o["panel"], o["system"] if flags[0] else 0, o["info"] if flags[1] else 0,
                                 o["centre"] if flags[2] else 0)

    def config(self, server, port):
        self.lib.tpw_sp_set_config(server, port)

    def labs(self):
        out = (C.c_int64 * 3)()
        self.lib.tpw_sp_labs(out)
        return list(out)

    def email_words(self):
        p = self.lib.tpw_sp_email()
        if not p:
            return []
        w = list(struct.unpack("<19I", C.string_at(p, 0x4C)))
        vec = self.lib.tpw_sp_addr(b"illvec") & MASK
        w[0x12] = 1 if w[0x12] == vec else w[0x12]
        return w

    def call(self, entry, args):
        lib = self.lib
        if entry == FN_ILL:
            lib.tpw_sp_illcall(s32(args[0]), s32(args[1]))
            return 0
        if entry == FN_VALID:
            return lib.tpw_sp_valid()
        if entry == FN_ADDTO:
            lib.tpw_sp_addto(args[0])
            return 0
        lib.tpw_sp_send()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_sp_addr.restype, lib.tpw_sp_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sp_alloc.restype, lib.tpw_sp_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sp_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sp_set_ptrs.argtypes = [C.c_uint64] * 5
    lib.tpw_sp_set_extras.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_sp_set_config.argtypes = [C.c_char_p, C.c_int32]
    lib.tpw_sp_labs.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sp_email.restype = C.c_uint64
    lib.tpw_sp_illcall.argtypes = [C.c_int32, C.c_int32]
    lib.tpw_sp_valid.restype = C.c_uint32
    lib.tpw_sp_addto.argtypes = [C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sp_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def u16s(text):
    return [ord(c) for c in text]


def build(s, case):
    s.w16(s.g("gTempPath"), [], 0)
    s.write(s.g("gTempPath"), case["temp"].ljust(0x200, b"\0"))
    s.write(s.g("gPostcardJpgPath"), bytes(0x200))
    s.words(s.g("gPostcardCounter"), [case["counter"]])
    s.w16(s.g("gPostcardText5da5"), u16s(case["text"]), 0x200)
    s.w16(s.g("gPostcardAddress"), u16s(case["addr"]), 0x60)
    strbuf = s.alloc(0x400)
    s.w16(strbuf, u16s(case["email"]), 0x80)
    s.w16(strbuf + 0x100, u16s(case["name"]), 0x80)
    s.w16(strbuf + 0x200, u16s(case["pw"]), 0x80)
    s.w16(strbuf + 0x300, u16s(case["ui"]), 0x80)
    s.extras(strbuf)
    s.pointers(case["objs"])
    s.config(case["server"], case["port"])
    arg = s.alloc(0x200)
    s.w16(arg, u16s(case["arg"]), 0x100)
    case["_arg"] = arg


def execute(s, case):
    op = case["op"]
    if op == "ill":
        return s.call(FN_ILL, case["args"])
    if op == "valid":
        return s.call(FN_VALID, [])
    if op == "addto":
        return s.call(FN_ADDTO, [case["_arg"]])
    return s.call(FN_SEND, [])


def observe(s, case, answer):
    labs = s.labs()
    return {"answer": answer, "labs": labs, "log": s.log(),
            "counter": s.rd_word(s.g("gPostcardCounter")),
            "jpg": s.read(s.g("gPostcardJpgPath"), 0x200),
            "temp": s.read(s.g("gTempPath"), 0x200),
            "text": s.rd16(s.g("gPostcardText5da5"), 0x200),
            "addr": s.rd16(s.g("gPostcardAddress"), 0x60),
            "email": s.email_words()}


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

SEPS = [";", ", ", ",", " ", "\n", ";;", " ; "]
LOGIN_EMAILS = ["player@x.com", "", "a\u0141b", "a\u0100b", "caf\u00e9@x.y", "z" * 100]
NAMES = ["", "Bob", "J\u00e9r\u00f4me \u4e2d", "n" * 40]
TEMPS = [b"", b"/tmp/", b"C:\\Temp\\", b"a" * 90, b"\xe9t\xe9:"]
ADDR_TEXTS = ["", "me@a.b", "Bob <b@x.y>", "z" * 90]


def rtok(r):
    local = r.choice(["a", "bob", "x.y", "", "q"])
    at = "@" if r.random() < 0.8 else ""
    dom = r.choice(["b.com", "c.d", "e.", "f", ".g", "h.i.j", "k.l"])
    return local + at + dom


def raddrs(r, maxn=5):
    n = r.randrange(0, maxn + 1)
    s = r.choice(["", ";", " ", "\n"]) if r.random() < 0.2 else ""
    s += r.choice(SEPS).join(rtok(r) for _ in range(n))
    return s + r.choice(["", "", ";"])


def rint(r):
    return r.choice([0, 1, -1, 0x7FFFFFFF, -0x80000000, r.randrange(0, 1 << 31), r.randrange(1, 100)])


def base(r, op):
    return {"op": op, "temp": r.choice(TEMPS), "counter": r.choice([0, 1, 7, 12345, 99999999, 0x7FFFFFFF, -1, -12]),
            "text": raddrs(r), "addr": r.choice(ADDR_TEXTS), "email": r.choice(LOGIN_EMAILS), "name": r.choice(NAMES),
            "pw": r.choice(NAMES), "ui": r.choice(["", "Enter address", "x@y.z"]),
            "objs": (r.random() < 0.5, r.random() < 0.5, r.random() < 0.5),
            "server": r.choice([b"", b"mail.example.com", b"m", b"\xe9x"]), "port": r.choice([0, 25, 110, -1, 32767, -32768, 0x19]),
            "arg": r.choice(["a@b.c", "x@y.z", "me@a.b", ""]),
            "args": [0, 0],
            "queues": {"AreWeLoggedIn": [r.choice([0, 1, 5, -1])], "Send": [r.choice([0, 1])], "PlayerID": [rint(r)],
                       "CustomerID": [rint(r)], "AttachData": [r.choice([0, 1])]}}


# ---- IllCallYouBack
def ill_case(r, code=None, err=None):
    c = base(r, "ill")
    c["args"] = [r.choice([0, 1, -1, 2, 7]) if code is None else code, r.randrange(-3, 9) if err is None else err]
    return c


def ill_edges():
    r = random.Random(11)
    for code in (0, 1, -1):
        for err in range(-3, 9):
            yield ill_case(r, code, err)


# ---- AddressesValid
VALID_TEXTS = ["", "a@b.c", "a@b.c;d@e.fg", "a@b.", "@b.c", "a@.c", "a@b.c, x", "a@b.c\nd@e.f", ";;", " ; ,", "a@b", "ab.c@",
               "a@b.c;;d@e.f;", "a.b@c", "a@bc.", "a@b.c.", ".@.", "a@@b.c", "x@y.z\n"]


def valid_case(r, text=None):
    c = base(r, "valid")
    c["text"] = raddrs(r) if text is None else text
    return c


def valid_edges():
    r = random.Random(12)
    for t in VALID_TEXTS:
        yield valid_case(r, t)
    yield valid_case(r, "a@b.c;" * 40)
    yield valid_case(r, "a@b.c;" * 40 + "bad")


# ---- AddToAddressField
def addto_case(r, text=None, arg=None, ui=None):
    c = base(r, "addto")
    if text is not None:
        c["text"] = text
    if arg is not None:
        c["arg"] = arg
    if ui is not None:
        c["ui"] = ui
    return c


def addto_edges():
    r = random.Random(13)
    for text, arg, ui in [("", "a@b.c", "Enter"), ("Enter", "a@b.c", "Enter"), ("x@y.z;", "a@b.c", "Enter"), ("x@y.z;", "x@y.z", "Enter"),
                          ("x@y.z;", "", "Enter"), ("x@y.z;", "y", "Enter"), ("", "", ""), ("", "q" * 70, ""), ("x@y.z;", "q" * 70, "Enter"),
                          ("abc", "abc", "abc"), ("a@b.c;d", "a@b.c;d@e", "Enter")]:
        yield addto_case(r, text, arg, ui)


def addto_random(r):
    c = addto_case(r)
    c["text"] = r.choice([c["ui"], "", raddrs(r, 3)])
    c["arg"] = r.choice([rtok(r), c["text"][:5], c["text"][2:9], "me@a.b"])
    return c


# ---- SendPostcard
def send_case(r, **kw):
    c = base(r, "send")
    c["text"] = raddrs(r, 4)
    c.update(kw)
    q = c["queues"]
    for k, v in kw.items():
        if k.startswith("q_"):
            q[k[2:]] = [v]
    return c


def send_edges():
    r = random.Random(14)
    for logged in (0, 1):
        for objs in [(a, b, d) for a in (0, 1) for b in (0, 1) for d in (0, 1)]:
            for send in (0, 1):
                yield send_case(r, objs=objs, q_AreWeLoggedIn=logged, q_Send=send)
    for text in ["", ";;", "a@b.c", "a@b.c;d@e.f, g@h.i\nj@k.l", " ;a@b.c; "]:
        yield send_case(r, text=text, q_AreWeLoggedIn=1)
        yield send_case(r, text=text, q_AreWeLoggedIn=0)
    for server, port in [(b"", 25), (b"", 0), (b"smtp.x.y", 587), (b"s", -5)]:
        yield send_case(r, server=server, port=port, q_AreWeLoggedIn=1)


TESTS = [
    FnTest("IllCallYouBack", ppc, host, edge=ill_edges, rnd=ill_case),
    FnTest("AddressesValid", ppc, host, edge=valid_edges, rnd=valid_case),
    FnTest("AddToAddressField", ppc, host, edge=addto_edges, rnd=addto_random),
    FnTest("SendPostcard", ppc, host, edge=send_edges, rnd=send_case),
]
