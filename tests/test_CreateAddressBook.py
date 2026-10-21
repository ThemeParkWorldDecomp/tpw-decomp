"""Unit v5012, the address book panel of the postcard screen, against libtpw_unit_CreateAddressBook.dylib (tests/unit_CreateAddressBook.cpp),
main app ("SimTheme Park"), src/game/ui/ui_Postcard.cpp:
  CreateAddressBook 0x101b07a4, RemoveAddressBookEntry 0x101b0420, NewEntryInAddressBook 0x101b0168

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee outside
the unit (the interface window, list and edit box members, the draw object constructors, the address book, the dialog box, the text table,
the string formatter, swprintf, wcschr, snprintf, the std::string constructor) is a fake on both sides that records its arguments (pointers
as labels, texts as hashes or strings) and answers from the case's queues; operator new is the harness's.

The world of a case: gPostcardPanel (with its Z), gAddressBookPanel (set or not), the windows the panel hands out (title 0x27, list 0x26,
edit box 0x25; the title and edit box carry a vtable whose SetText slot is checked on the PowerPC side), the list's current entry, gUserInfo
and gOnlineSystem (made or not), the address book entries as iterator cells, the park name, the edit text and the text table. What is
compared: the event log, the two text buffers (the edit text, the removal dialog's address), and which object each global holds after the call.

No fused operations. Not tested: the description script data (placeholder in the translation), an edit text without a terminator (the
buffer is 0x40 characters and the original reads on), non-ASCII text on the accepted path (snprintf of the Mac's %ls on a narrow buffer),
the second string of the edit box's start text (constants of the translation, read back through the logged SetText text).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CreateAddressBook"
MASK = 0xFFFFFFFF

FN_CREATE, FN_REMOVE, FN_NEW = 0x101B07A4, 0x101B0420, 0x101B0168
SIZES = {FN_CREATE: 696, FN_REMOVE: 464, FN_NEW: 468}
OWN = set(SIZES)

# the Mac's addresses of the globals and TVectors (the asm's TOC comments)
ADDR = dict(desc=0x102AC1E6, msgAB=0x10267D20, msgName=0x10267D18, lcf=0x10267D08, lcp=0x10267D00, delok=0x10267D10,
            editbuf=0x10394BA6, rembuf=0x10394B24, ppanel=0x103956FC, bpanel=0x103956F8, userinfo=0x10380B34, online=0x10380AB0,
            tslot=0x1037FF18 + 0x564)
FIXED = dict(desc=1, msgAB=2, msgName=3, lcf=4, lcp=5, delok=6, editbuf=7, rembuf=8)
OBJ = dict(P=10, B=11, T=12, Lc=13, E=14, UI=15, OS=17, TB=18, FTP=19, D1=20, D2=21)
SENT = 0x5E27F00D
HOST_LAYOUT = None          # read from the library: [list.mCurrentEntry, window.mZ, user.mAddressBook, sizeof node, node.mText]


def s32(v):
    return T.s32(v)


def fnv(seq):
    h = 2166136261
    for u in seq:
        h = ((h ^ u) * 16777619) & MASK
    return h


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def ucs(e, a, limit=0x200):
    out = []
    for i in range(limit):
        u = struct.unpack(">H", e.read(a + 2 * i, 2))[0]
        if u == 0:
            break
        out.append(u)
    return out


def nar(e, a, limit=0x200):
    out = []
    for i in range(limit):
        c = e.read(a + i, 1)[0]
        if c == 0:
            break
        out.append(c)
    return out


def wbytes(units):
    return b"".join(struct.pack(">H", u) for u in units) + b"\0\0"


def reg(addr, name, spec, ret=None):
    STUBS.internal(addr, CL.recorder(name, spec, ret=ret))


def q(key):
    return lambda e, L: L.ret(key) & MASK


reg(0x101DEB78, "Build", "LLL", q("Build"))
reg(0x1018FBE8, "InterfaceTitleTextObject", "Nu")
reg(0x101DBE40, "GetWindow", "Li", lambda e, L: L.ret("GetWindow:%d" % s32(e.r(4))) & MASK)
reg(0x101DB624, "SetClientDrawObject", "LL")
reg(0x10180270, "GetFontTableEntry", "u", lambda e, L: L.ret("Font:%d" % e.r(3)) & MASK)
reg(0x101DA118, "SetFont", "Lu")
reg(0x101DA204, "SetFontJust", "Lii")
reg(0x10180468, "UIMain_GetString", "i", lambda e, L: L.ret("GetString:%d" % s32(e.r(3))) & MASK)
reg(0x1018BD3C, "UI_TITLE_COLOUR", "L")
reg(0x101DD8BC, "setZ", "Lh")
reg(0x101D2864, "SetColumnFormatsAll", "LLL")
reg(0x101D2614, "SetColumnType", "Lhi")
reg(0x101CD2D4, "FindDrawObject", "s", q("Find"))
reg(0x101D1EB4, "SetSelectionObject", "LL")
reg(0x101AFF60, "FillAddressList", "i")
reg(0x1018F1D0, "InterfaceCursorPosTextObjectSprite", "Nu")
reg(0x101DA3AC, "SetFontColour", "Liiii")
reg(0x101DCBA8, "SetCallbackFunction", "LL")
reg(0x101CD7E0, "SetStaticTextArea", "LLi", q("SetStaticTextArea"))
reg(0x101CDAC4, "SetCursorDrawObject", "LL")
reg(0x101821CC, "UIMain_AdvisorMessage", "i")
reg(0x101D4C64, "SetCurrent", "Li")
reg(0x1017DE54, "CFSUserInfo", "N")
reg(0x1016F7D0, "CFSOnlineSystem", "N")
reg(0x10177654, "GetFTPDownloadParkInfo", "L", q("GetFTPDownloadParkInfo"))

_set_text = CL.recorder("SetText", "LS", ret=q("SetText"))


def stub_glue(e):
    """__ptr_glue: the virtual SetText through the window's vtable word at +0x118, slot +0x2c (r12 must hold the sentinel)."""
    if e.r(12) != SENT:
        raise AssertionError("the call did not go through the vtable slot +0x2c: r12 = %#x" % e.r(12))
    _set_text(e)


STUBS.internal(0x1023BAA8, stub_glue)


def stub_getdata(e):
    L = e.cl
    L.rec("GetData", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">i", s32(L.ret("GetDataValue"))))
    e.set_r(3, L.ret("GetData") & MASK)


STUBS.internal(0x101D41F0, stub_getdata)


def stub_first(e):
    L = e.cl
    L.rec("First", (L.lab(e.r(3) - 0xC4),))
    e.set_r(3, L.ret("First") & MASK)


def stub_next(e):
    L = e.cl
    L.rec("Next", (L.lab(e.r(3) - 0xC4),))
    e.set_r(3, L.ret("Next") & MASK)


def stub_add(e):
    L = e.cl
    p = e.u32(e.r(4))
    txt = nar(e, p)
    L.rec("Add", (L.lab(e.r(3) - 0xC4), fnv(txt), len(txt)))


STUBS.internal(0x10206534, stub_first)
STUBS.internal(0x10206594, stub_next)
STUBS.internal(0x102065F8, stub_add)


def stub_strctor(e):
    this = e.r(3)
    txt = bytes(nar(e, e.r(4)))
    p = e.alloc(len(txt) + 1, txt + b"\0")
    e.write(this, struct.pack(">I", p))
    e.set_r(3, this)


STUBS.internal(0x10002338, stub_strctor)
STUBS.internal(0x1000221C, lambda e: None)


def stub_addto(e):
    L = e.cl
    L.rec("AddToAddressField", (L.lab(e.r(3)), fnv(ucs(e, e.r(3)))))


STUBS.internal(0x101B0E98, stub_addto)


def stub_swprintf(e):
    L = e.cl
    dest, n, fmt, arg = e.r(3), e.r(4), e.r(5), e.r(6)
    txt = nar(e, arg)
    L.rec("swprintf", (L.lab(dest), n, fnv(ucs(e, fmt)), fnv(txt), len(txt)))
    out = txt[:max(n - 1, 0)]
    e.write(dest, wbytes(out))
    e.set_r(3, len(out))


def glue(fn):
    """A glue import saves the TOC at 0x14(r1) and the caller reloads it from there: do the same."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        fn(e)
    return wrapped


STUBS.internal(0x10239160, glue(stub_swprintf))


def stub_snprintf(e):
    """snprintf(char buffer, n, "%ls", wide): the narrow text (the tests keep it ASCII)."""
    dest, n, arg = e.r(3), e.r(4), e.r(6)
    txt = [u & 0xFF for u in ucs(e, arg)][:max(n - 1, 0)]
    e.write(dest, bytes(txt) + b"\0")
    e.set_r(3, len(txt))


STUBS.internal(0x10239628, glue(stub_snprintf))


def stub_wcschr(e):
    a, c = e.r(3), e.r(4) & 0xFFFF
    for i in range(0x400):
        u = struct.unpack(">H", e.read(a + 2 * i, 2))[0]
        if u == c:
            e.set_r(3, a + 2 * i)
            return
        if u == 0:
            break
    e.set_r(3, 0)


STUBS.internal(0x10238C68, glue(stub_wcschr))


def stub_format(e):
    L = e.cl
    this, out, size, id_, tag = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
    magic, tid, tp = e.u32(tag), e.u32(tag + 8), e.u32(tag + 0xC)
    txt = ucs(e, tp)
    L.rec("FormatString", (L.lab(this), size, id_, magic, tid, fnv(txt), len(txt)))
    units = ([ord(c) for c in "F%d:" % id_] + txt)[:max(size - 1, 0)]
    e.write(out, wbytes(units))
    e.set_r(3, len(units))


STUBS.internal(0x10239730, glue(stub_format))


def stub_dialog(e):
    L = e.cl
    p = e.r(3)
    txt = ucs(e, e.u32(p))
    rgb = e.read(p + 4, 3)
    b1 = s32(e.r(4))
    valuec = e.s32(p + 0xC) if b1 == 2 else 0
    L.rec("UI_DialogBox", (fnv(txt), (rgb[0] << 16) | (rgb[1] << 8) | rgb[2], e.s32(p + 8), valuec, b1, L.lab(e.r(5)), s32(e.r(6)),
                           L.lab(e.r(7)), s32(e.r(8)), L.lab(e.r(9)), e.r(10) & 0xFF))
    e.set_r(3, 0)


STUBS.internal(0x101F1800, stub_dialog)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def ptr(self, a, v):
        self.write(a, struct.pack(self.e_ + ("Q" if self.psize == 8 else "I"), v))

    def rd_ptr(self, a):
        return struct.unpack(self.e_ + ("Q" if self.psize == 8 else "I"), self.read(a, self.psize))[0]

    def i32(self, a, v):
        self.write(a, struct.pack(self.e_ + "i", s32(v)))

    def i16(self, a, v):
        self.write(a, struct.pack(self.e_ + "h", T.s16(v)))

    def units(self, a, text):
        self.write(a, b"".join(struct.pack(self.e_ + "H", u) for u in text) + b"\0\0")

    def rd_units(self, a, n):
        return list(struct.unpack(self.e_ + "%dH" % n, self.read(a, 2 * n)))

    def cstr(self, text):
        a = self.alloc(len(text) + 1)
        self.write(a, bytes(text) + b"\0")
        return a

    def wstr(self, text):
        a = self.alloc(2 * len(text) + 2)
        self.units(a, text)
        return a

    def win(self, vtable=False):
        w = self.alloc(0x400)
        if vtable:
            self.set_vptr(w)
        return w


class Ppc(Side):
    e_ = ">"
    psize = 4
    off_cur, off_z, off_text = 0x14C, 0xDC, 0xC

    def __init__(self, e, case):
        self.e = e
        self.saved = None

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

    def set_vptr(self, w):
        vt = self.alloc(0x40)
        self.ptr(vt + 0x2C, SENT)
        self.ptr(w + 0x118, vt)

    def begin(self, labels, queues):
        self.L = self.e.cl = CL.PpcLog({addr: lab for addr, lab in labels}, queues)
        self.saved = STUBS.activate(self.e)

    def log(self):
        return self.L.log

    def call(self, entry):
        T.call_ppc(self.e, entry, [])


class Host(Side):
    e_ = "<"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.off_cur, self.off_z = lib.hl[0], lib.hl[1]
        self.off_text = lib.hl[4]

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ab_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ab_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def set_vptr(self, w):
        self.ptr(w, self.lib.tpw_ab_vptr())

    def begin(self, labels, queues):
        CL.host_begin(self.lib, {lab: addr for addr, lab in labels}, queues)

    def log(self):
        return CL.host_end(self.lib)

    def call(self, entry):
        {FN_CREATE: self.lib.tpw_ab_create, FN_REMOVE: self.lib.tpw_ab_remove, FN_NEW: self.lib.tpw_ab_new}[entry]()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    for name in ("addr", "alloc", "vptr"):
        getattr(lib, "tpw_ab_" + name).restype = C.c_uint64
    lib.tpw_ab_addr.argtypes = [C.c_char_p]
    lib.tpw_ab_alloc.argtypes = [C.c_uint32]
    lib.tpw_ab_layout.argtypes = [C.POINTER(C.c_int64)]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ab_layout(out)
    lib.hl = list(out)[:n]
    assert lib.hl[4] == 0xC, lib.hl      # AddressBookNode keeps the Mac's text offset
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's world; answers (the named blocks, the queues)."""
    W = {}
    op = case["op"]
    s.units(s.g("editbuf"), [0] * 0x3F)
    s.units(s.g("rembuf"), [0] * 0x3F)
    for name in ("ppanel", "bpanel", "userinfo", "online", "tslot"):
        s.ptr(s.g(name), 0)
    W["TB"] = s.alloc(0x40)
    s.ptr(s.g("tslot"), W["TB"])
    W["B"] = s.win()
    qs = {}
    if op == "create":
        W["P"] = s.win()
        s.i16(W["P"] + s.off_z, case["z"])
        s.ptr(s.g("ppanel"), W["P"])
        if case["panel_set"]:
            s.ptr(s.g("bpanel"), W["B"])
        W["T"], W["Lc"], W["E"] = s.win(True), s.win(), s.win(True)
        W["D1"], W["D2"] = s.alloc(16), s.alloc(16)
        qs = {"Build": [W["B"]], "GetWindow:39": [W["T"]], "GetWindow:38": [W["Lc"]], "GetWindow:37": [W["E"]],
              "Font:5": [case["fonts"][0]], "Font:7": [case["fonts"][1]], "GetString:212": [s.wstr(case["title"])],
              "Find": [W["D1"], W["D2"]], "SetText": [case["settext"]], "SetStaticTextArea": [case["sta"]]}
    elif op == "remove":
        s.ptr(s.g("bpanel"), W["B"])
        W["Lc"] = s.win()
        s.i32(W["Lc"] + s.off_cur, case["cur"])
        if case["ui"]:
            W["UI"] = s.alloc(0x400)
            s.ptr(s.g("userinfo"), W["UI"])
        if case["os"]:
            W["OS"] = s.alloc(0x400)
            s.ptr(s.g("online"), W["OS"])
        data = case["data"]
        W["FTP"] = s.alloc(0x100)
        s.ptr(W["FTP"] + 0x9C, s.wstr(case["park"]))
        slots = []
        if data >= 0:
            for i in range(data + 1):
                node = s.alloc(0x20)
                s.ptr(node + s.off_text, s.cstr(case["names"][i % len(case["names"])]))
                slot = s.alloc(8)
                s.ptr(slot, node)
                slots.append(slot)
        qs = {"GetWindow:38": [W["Lc"]], "GetData": [case["gd"]], "GetDataValue": [data], "GetFTPDownloadParkInfo": [W["FTP"]]}
        if slots:
            qs["First"] = [slots[0]]
            qs["Next"] = slots[1:] or [slots[0]]
    else:
        s.ptr(s.g("bpanel"), W["B"])
        W["Lc"] = s.win()
        if case["ui"]:
            W["UI"] = s.alloc(0x400)
            s.ptr(s.g("userinfo"), W["UI"])
        s.units(s.g("editbuf"), case["text"])
        qs = {"GetWindow:38": [W["Lc"]]}
    labels = [(s.g(k), v) for k, v in FIXED.items()] + [(W[k], OBJ[k]) for k in W if k in OBJ]
    return W, labels, qs


def run(s, case):
    try:
        W, labels, qs = build(s, case)
        s.begin(labels, qs)
        s.call({"create": FN_CREATE, "remove": FN_REMOVE, "new": FN_NEW}[case["op"]])
        known = {v: k for k, v in W.items()}

        def nm(a):
            return None if a == 0 else known.get(a, "new")

        return {"log": s.log(), "rembuf": s.rd_units(s.g("rembuf"), 0x40), "editbuf": s.rd_units(s.g("editbuf"), 0x40),
                "bpanel": nm(s.rd_ptr(s.g("bpanel"))), "userinfo": nm(s.rd_ptr(s.g("userinfo"))),
                "online": nm(s.rd_ptr(s.g("online")))}
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

TITLES = ["Address Book", "", "A", "Adresses", "x" * 40]


def rtext(r, alphabet, lo, hi):
    return [ord(r.choice(alphabet)) for _ in range(r.randrange(lo, hi + 1))]


def create_case(r, panel_set=None, z=None):
    return {"op": "create", "panel_set": (r.random() < 0.2) if panel_set is None else panel_set,
            "z": r.choice([0, 1, 5, -10, 0x7FF5, 0x7FFF, -0x8000, T.rint(r) & 0xFFFF]) if z is None else z,
            "title": [ord(c) for c in r.choice(TITLES)], "fonts": [r.randrange(1, 1 << 32), r.randrange(1, 1 << 32)],
            "settext": r.choice([0, 1]), "sta": r.choice([0, 1, 0x3F])}


def create_edges():
    r = random.Random(5012)
    yield create_case(r, panel_set=True)
    for z in (0, 1, -10, 0x7FF5, 0x7FF6, 0x7FFF, -0x8000, -1):
        yield create_case(r, panel_set=False, z=z)


def create_random(r):
    return create_case(r)


def remove_case(r, cur=None, data=None):
    cur = r.choice([-1, -2, 0, 1, 2, 5, 9]) if cur is None else cur
    data = r.choice([-1, -2, -0x80000000, 0, 1, 2, 3, 6, 11]) if data is None else data
    return {"op": "remove", "cur": cur, "data": data, "gd": r.choice([0, 1]), "ui": r.choice([0, 1]), "os": r.choice([0, 1]),
            "park": rtext(r, "abcXYZ 1.-", 0, 20), "names": [rtext(r, "abc@.xyz12", 0, 20) for _ in range(r.randrange(1, 14))]}


def remove_edges():
    r = random.Random(5013)
    for cur in (-0x80000000, -2, -1):
        yield remove_case(r, cur=cur)
    for data in (-0x80000000, -2, -1, 0, 1, 2, 5):
        for ui in (0, 1):
            for os_ in (0, 1):
                c = remove_case(r, cur=r.choice([0, 3]), data=data)
                c["ui"], c["os"] = ui, os_
                yield c
    c = remove_case(r, cur=0, data=3)
    c["names"] = [[], list(b"x" * 63), list(b"y" * 70), list(b"ok")]
    yield c


def remove_random(r):
    return remove_case(r)


def new_case(r, text=None, ui=None):
    return {"op": "new", "text": text if text is not None else rtext(r, "ab@.1", 0, 20),
            "ui": r.choice([0, 1]) if ui is None else ui}


def new_edges():
    r = random.Random(5014)
    for t in ("", "@", "a@", "a@b", "a@b.", "a@.b", "a@b.c", "@a.b", "a.b@c", "a@@b.c", "a@b.c.d", ".@", "@.", "@..", "x@y.z", "a.@bc",
              "ab@cd.", "a" * 20 + "@" + "b" * 20 + "." + "c" * 20, "a@" + "b" * 60, "a" * 61 + "@.", "a" * 55 + "@b.cde"):
        for ui in (0, 1):
            yield new_case(r, [ord(c) for c in t], ui)
    yield new_case(r, [0x20AC, 0x3042, ord("."), 0x3044], 1)       # the refusal dialog takes any text


def new_random(r):
    return new_case(r)


TESTS = [
    FnTest("CreateAddressBook", ppc, host, edge=create_edges, rnd=create_random),
    FnTest("RemoveAddressBookEntry", ppc, host, edge=remove_edges, rnd=remove_random),
    FnTest("NewEntryInAddressBook", ppc, host, edge=new_edges, rnd=new_random),
]
