"""Unit v5055, the address book panel's message function and its list filler, against libtpw_unit_AddressBookMsg.dylib
(tests/unit_AddressBookMsg.cpp), main app ("SimTheme Park"), src/game/ui/ui_Postcard.cpp:
  AddressBookMsg 0x101afb34, FillAddressList 0x101aff60

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the window lookup and its default handler, the list control's members, the constructors, the address book cursor, the string
and printf imports, the dialog helpers) are fakes on both sides that record their arguments; the edit box's SetText is the virtual call
(the PowerPC side goes through __ptr_glue).

The world of a case: the address book as a linked list of nodes (+0 the next node, +0xc the text) ending in an empty text, the
user info and online system pointers (NULL or made), the park info's folder text, the game type object and its guard byte, the removal
dialog's buffer and the panel pointer. What is compared: the answer, the globals above, the removal buffer and the event log.

No fused operations. Not tested: an index past the address book (the cursor would walk off the list), a NULL panel in FillAddressList
(it calls the window lookup on NULL, which the fake would take but the host should not).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AddressBookMsg"
MASK = 0xFFFFFFFF

FN_MSG, FN_FILL = 0x101AFB34, 0x101AFF60
SIZES = {FN_MSG: 820, FN_FILL: 480}
OWN = set(SIZES)

# the callees
C_CANCEL, C_REMOVE_BOOK, C_REMOVE_ENTRY = 0x1018227C, 0x101B0A84, 0x101B0420
C_GETWINDOW, C_GETDATA, C_DEFAULT = 0x101DBE40, 0x101D41F0, 0x101DE318
C_USERINFO, C_ONLINE, C_PARKINFO, C_GAMETYPE = 0x1017DE54, 0x1016F7D0, 0x10177654, 0x1016F5D8
C_FIRST, C_NEXT, C_ADDTO, C_PTRGLUE = 0x10206534, 0x10206594, 0x101B0E98, 0x1023BAA8
C_ADDITEM, C_REMOVEALL, C_UPDATE, C_GETSTRING = 0x101D3000, 0x101D3CC8, 0x101D2220, 0x10180468

ADDR = dict(gUserInfo=0x10380B34, gOnlineSystem=0x10380AB0, gPanelPtr=0x103956F8, removeName=0x10394B24,
            gtype=0x102B3AA0, gguard=0x102B3AA4, fmt=0x1024E366)
HOST_LAYOUT = [0xC]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def kind(a):
    if a == 0:
        return 0
    return 1 if a == ADDR["removeName"] else 2


def codes16(e, a):
    out = []
    if a != 0:
        for i in range(8):
            v = struct.unpack(">H", e.read(a + 2 * i, 2))[0]
            out.append(v)
            if v == 0:
                break
    return tuple(out + [0] * (8 - len(out)))


def narrow(e, a, limit=0x100):
    out = []
    while len(out) < limit:
        v = e.read(a + len(out), 1)[0]
        if v == 0:
            break
        out.append(v)
    return out


def codes8(e, a):
    out = narrow(e, a, 8)
    return tuple(out + [0] * (8 - len(out)))


def stub_cancel(e):
    e.cl.rec("Cancel", (s32(e.r(3)),))


def stub_remove_book(e):
    e.cl.rec("RemoveAddressBook", (1,))


def stub_remove_entry(e):
    e.cl.rec("RemoveAddressBookEntry", (1,))


def stub_getwindow(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    if wid == 0x25:
        r = ADDR["edit"]
    elif wid == 0x26:
        r = ADDR["list"]
    else:
        r = ADDR["list"] if L.ret("GetWindow") else 0
    e.set_r(3, r)


def stub_default(e):
    L = e.cl
    L.rec("CallDefault", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("CallDefault") & MASK)


def stub_getdata(e):
    L = e.cl
    L.rec("GetData", (L.lab(e.r(3)), s32(e.r(4))))
    e.write(e.r(5), struct.pack(">I", L.ret("GetData") & MASK))


def stub_additem(e):
    L = e.cl
    text = e.u32(e.r(4))
    L.rec("AddItem", (L.lab(e.r(3)), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))) + codes16(e, text))


def stub_removeall(e):
    e.cl.rec("RemoveAllItems", (e.cl.lab(e.r(3)),))


def stub_update(e):
    e.cl.rec("UpdateDisplay", (e.cl.lab(e.r(3)),))


def stub_parkinfo(e):
    e.cl.rec("ParkInfo", (e.cl.lab(e.r(3)),))
    e.set_r(3, ADDR["parkinfo"])


def stub_gametype(e):
    L = e.cl
    L.rec("CGameType", (1,))
    e.write(e.r(3), struct.pack(">I", L.ret("GameType") & MASK))


def book_flag(e):
    return 1 if e.r(3) == e.u32(ADDR["gUserInfo"]) + 0xC4 else 0


def stub_first(e):
    e.cl.rec("First", (book_flag(e),))
    e.write(ADDR["cursor"], struct.pack(">I", e.u32(ADDR["head"])))
    e.set_r(3, ADDR["cursor"])


def stub_next(e):
    e.cl.rec("Next", (book_flag(e),))
    e.write(ADDR["cursor"], struct.pack(">I", e.u32(e.u32(ADDR["cursor"]))))
    e.set_r(3, ADDR["cursor"])


def stub_addto(e):
    e.cl.rec("AddToAddress", (kind(e.r(3)),) + codes16(e, e.r(3)))


def stub_ptrglue(e):
    e.cl.rec("SetText", (e.cl.lab(e.r(3)), kind(e.r(4))) + codes16(e, e.r(4)))
    e.set_r(3, 1)


def stub_getstring(e):
    e.cl.rec("GetString", (s32(e.r(3)),))
    e.set_r(3, ADDR["str121"])


def stub_swprintf(e):
    L = e.cl
    buf, size, fmt, text = e.r(3), s32(e.r(4)), e.r(5), e.r(6)
    f = struct.unpack(">HH", e.read(fmt, 4))
    L.rec("swprintf", (kind(buf), size, f[0], f[1]) + codes8(e, text))
    chars = narrow(e, text)
    n = 0
    if size > 0:
        out = chars[:size - 1]
        e.write(buf, b"".join(struct.pack(">H", c) for c in out) + b"\0\0")
        n = len(out)
    e.set_r(3, n)


def stub_wcslen(e):
    a = e.r(3)
    e.cl.rec("wcslen", codes16(e, a))
    n = 0
    while struct.unpack(">H", e.read(a + 2 * n, 2))[0] != 0:
        n += 1
    e.set_r(3, n)


for addr, fn in ((C_CANCEL, stub_cancel), (C_REMOVE_BOOK, stub_remove_book), (C_REMOVE_ENTRY, stub_remove_entry),
                 (C_GETWINDOW, stub_getwindow), (C_GETDATA, stub_getdata), (C_DEFAULT, stub_default),
                 (C_PARKINFO, stub_parkinfo), (C_GAMETYPE, stub_gametype), (C_FIRST, stub_first), (C_NEXT, stub_next),
                 (C_ADDTO, stub_addto), (C_PTRGLUE, stub_ptrglue), (C_ADDITEM, stub_additem),
                 (C_REMOVEALL, stub_removeall), (C_UPDATE, stub_update), (C_GETSTRING, stub_getstring)):
    STUBS.internal(addr, fn)
STUBS.internal(C_USERINFO, CL.recorder("CFSUserInfo", "N"))
STUBS.internal(C_ONLINE, CL.recorder("CFSOnlineSystem", "N"))
STUBS.imp("swprintf", stub_swprintf)
STUBS.imp("wcslen", stub_wcslen)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    vtable = T.persist_alloc(e, 0x100)
    ADDR["edit"] = T.persist_alloc(e, 0x140, bytes(0x118) + struct.pack(">I", vtable) + bytes(0x140 - 0x11C))
    for name, size in (("list", 0x140), ("parent", 0x140), ("panel", 0x140), ("parkinfo", 0x100), ("str121", 0x80),
                       ("head", 4), ("cursor", 4)):
        ADDR[name] = T.persist_alloc(e, size)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def u16s(self, a, vals):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(vals), *vals))

    def rd_u16s(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))


class Ppc(Side):
    fmt = ">I"
    guard_pad = b""

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({ADDR["list"]: 1, ADDR["edit"]: 2, ADDR["parent"]: 3, ADDR["panel"]: 4}, case["queues"])
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

    def ptr(self, a, v):
        self.words(a, [v])

    def rd_ptr(self, a):
        return self.rd_word(a)

    def log(self):
        return self.L.log

    def call_msg(self, msg, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_MSG, [ADDR["parent"], msg & MASK, a & MASK, b & MASK])
        return s32(r3)

    def call_fill(self, clear):
        T.call_ppc(self.e, FN_FILL, [clear & MASK])
        return 0


class Host(Side):
    fmt = "<I"
    guard_pad = b""

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {1: self.g("list"), 2: self.g("edit"), 3: self.g("parent"), 4: self.g("panel")}, case["queues"])

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

    def ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rd_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def log(self):
        return CL.host_end(self.lib)

    def call_msg(self, msg, a, b):
        return s32(self.lib.tpw_ab_msg(self.g("parent"), s32(msg), s32(a), s32(b)))

    def call_fill(self, clear):
        self.lib.tpw_ab_fill(s32(clear))
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_ab_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ab_msg.restype, lib.tpw_ab_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_ab_fill.restype, lib.tpw_ab_fill.argtypes = None, [C.c_int32]
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

OTHER = [ord(c) for c in "Other"]


def build(s, case):
    # the address book: nodes (+0 next, +0xc text pointer), a last one with an empty text
    texts = list(case["book"]) + [[]]
    nodes = [s.alloc(0x20) for _ in texts]
    for i, t in enumerate(texts):
        text = s.alloc(len(t) + 1)
        s.write(text, bytes(t) + b"\0")
        s.ptr(nodes[i], nodes[i + 1] if i + 1 < len(nodes) else 0)
        s.ptr(nodes[i] + 0xC, text)
    s.ptr(s.g("head"), nodes[0])
    # user info and online system
    s.ptr(s.g("gUserInfo"), s.alloc(0x100) if case["user"] else 0)
    s.ptr(s.g("gOnlineSystem"), s.alloc(0x100) if case["online"] else 0)
    # the park info's folder text at +0x9c
    park = case["park"]
    s.ptr(s.g("parkinfo") + 0x9C, 0)
    if park is not None:
        a = s.alloc(2 * len(park) + 2)
        s.u16s(a, list(park) + [0])
        s.ptr(s.g("parkinfo") + 0x9C, a)
    s.u16s(s.g("str121"), OTHER + [0])
    s.words(s.g("gtype"), [case["mtype"]])
    s.write(s.g("gguard"), bytes([case["guard"]]) + s.guard_pad)
    s.u16s(s.g("removeName"), case["name"])
    s.ptr(s.g("gPanelPtr"), s.g("panel"))


def execute(s, case):
    if case["op"] == "msg":
        return s.call_msg(*case["args"])
    return s.call_fill(*case["args"])


def observe(s, case, answer):
    panel = s.rd_ptr(s.g("gPanelPtr"))
    return {"answer": answer, "log": s.log(),
            "panel": 0 if panel == 0 else (1 if panel == s.g("panel") else 2),
            "name": s.rd_u16s(s.g("removeName"), 0x40),
            "user": s.rd_ptr(s.g("gUserInfo")) != 0, "online": s.rd_ptr(s.g("gOnlineSystem")) != 0,
            "mtype": s.rd_word(s.g("gtype")), "guard": s.read(s.g("gguard"), 1)[0]}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, execute(s, case))
    finally:
        s.done()


LAST = {}


def ppc(e, case):
    LAST["p"] = run(Ppc(e, case), case)
    return LAST["p"]


def host(lib, case):
    h = run(Host(lib, case), case)
    p = LAST.get("p")
    if p is not None and p != h:
        for k in p:
            if p[k] != h[k]:
                print("DIFF", k, "ppc", p[k] if k != "log" else p[k][:12], "host", h[k] if k != "log" else h[k][:12])
    return h


# ---------------------------------------------------------------------------------------------------------------- case generators


def rstr(r, hi=8):
    n = r.randrange(1, hi + 1)
    top = 0x7F if r.random() < 0.8 else 0xFF
    return [r.randrange(1, top + 1) for _ in range(n)]


def base(r, op, k=None):
    k = r.randrange(0, 5) if k is None else k
    park = r.choice([None, [], rstr(r, 3), [0x41 + i for i in range(12)]])
    return {"op": op, "book": [rstr(r) for _ in range(k)],
            "user": r.choice([0, 1]), "online": r.choice([0, 1]), "park": park,
            "mtype": r.choice([0, 1, 1, 2, 5, -1]), "guard": r.choice([0, 1]),
            "name": [r.randrange(0, 0x10000) for _ in range(0x40)],
            "queues": {"GetWindow": [r.choice([0, 1])], "CallDefault": [r.choice([0, 1, -1, 77])], "GameType": [r.choice([0, 1, 1, 2])]}}


def msg_case(r, msg=None, a=None, b=None, n=None, win=None):
    c = base(r, "msg")
    k = len(c["book"])
    if msg is None:
        msg = r.choice([0x100, 0x14, 0x400, 0x401, 0x400, 0x401, 0, 0x13, 0x15, 0x101, 0x3FF, 0x402, -1, 0x7FFFFFFF])
    if a is None:
        a = r.choice([-1, 0x24, 5, 0x23, 0x25, 0x26]) if msg == 0x100 else r.choice([7, 100, 1])
    if b is None:
        b = r.choice([-1, 0, 3, 99])
    if n is None:
        n = r.choice([-1, -5, -0x80000000, 0x7FFFFFFF * 0]) if r.random() < 0.25 else r.randrange(0, k + 1)
    c["queues"]["GetData"] = [n]
    if win is not None:
        c["queues"]["GetWindow"] = [win]
    c["args"] = [msg, a, b]
    return c


def fill_case(r, clear=None):
    c = base(r, "fill")
    c["args"] = [r.choice([0, 1, 2, -1]) if clear is None else clear]
    return c


def msg_edges():
    r = random.Random(5055)
    for msg in (0x100, 0x14, 0, 0x13, 0x15, 0xFF, 0x101, 0x3FF, 0x402, 0x7FFFFFFF, -1):
        for a in (-1, 0x24, 5):
            yield msg_case(r, msg, a)
    for msg in (0x400, 0x401):
        for win in (0, 1):
            for b in (-1, 0, 2):
                for n in (-1, -0x80000000, 0, 1, 3):
                    c = msg_case(r, msg, 7, b, n, win)
                    k = max(len(c["book"]), 4)
                    while len(c["book"]) < k:
                        c["book"].append(rstr(r))
                    yield c
    for user in (0, 1):
        for online in (0, 1):
            for park in (None, [], [0x41]):
                c = msg_case(r, 0x401, 7, 1, -1, 1)
                c.update(user=user, online=online, park=park)
                yield c
                c = msg_case(r, 0x400, 7, 1, 0, 1)
                c.update(user=user, online=online, park=park)
                c["book"] = c["book"] or [rstr(r)]
                yield c


def msg_random(r):
    return msg_case(r)


def fill_edges():
    r = random.Random(50551)
    for k in (0, 1, 3):
        for clear in (0, 1, -1):
            for mtype in (0, 1, 2, 5):
                for guard in (0, 1):
                    for park in (None, [], [0x41, 0x42]):
                        c = fill_case(r, clear)
                        c["book"] = [rstr(r) for _ in range(k)]
                        c.update(mtype=mtype, guard=guard, park=park)
                        c["queues"]["GameType"] = [mtype if mtype in (0, 1, 2) else 1]
                        yield c
    for user in (0, 1):
        for online in (0, 1):
            c = fill_case(r, 1)
            c.update(user=user, online=online, mtype=1, guard=1, park=[0x41])
            yield c


def fill_random(r):
    return fill_case(r)


TESTS = [
    FnTest("AddressBookMsg", ppc, host, edge=msg_edges, rnd=msg_random),
    FnTest("FillAddressList", ppc, host, edge=fill_edges, rnd=fill_random),
]
