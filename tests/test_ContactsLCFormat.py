"""Unit v5279, the online contacts and messages lists, against libtpw_unit_ContactsLCFormat.dylib (tests/unit_ContactsLCFormat.cpp), main app
("SimTheme Park"), src/ContactsLCFormat.cpp:
  ContactsLCEntryHandler 0x101e7648, MessagesLCEntryHandler 0x101e776c, ContactsLCFormat 0x101e7a58, ContactsLCPostFormat 0x101e7b34,
  _OnlineAddContact 0x101e8068, _OnlineRemoveContact 0x101e812c, _OnlineUpdateContact 0x101e81f4

A case is a world and a call, built twice by one piece of Python code (emulator memory at the Mac's addresses, host library memory). The callees
outside the unit (GetFontTableEntry, the font's height virtual, the window and list member functions, operator new and the constructors) are
fakes on both sides that record their arguments; objects are named by labels.

The world of a case: the font index, the screen height, the two colour tables, the busy flag, gOnlinePanel (set or NULL). What is compared: the
answer (the handlers' default-handler answer, the format function's window as a label), the globals read back and the event log.
No fused operations. Not tested: a screen height of 0 (division by zero, undefined on the host).

A last test (unit v5L39), "the list colours": the host's two colour tables as the library was loaded against the program image's bytes
(tests/imagedata.py), one case per colour. The cases above write their own tables; before v5L39 the host had only the first colour of each.
"""
import ctypes as C
import random
import struct

import calllog as CL
import imagedata
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ContactsLCFormat"
MASK = 0xFFFFFFFF

FN = dict(conh=0x101e7648, msgh=0x101e776c, fmt=0x101e7a58, post=0x101e7b34, add=0x101e8068, rem=0x101e812c, upd=0x101e81f4)
SIZES = {0x101e7648: 224, 0x101e776c: 164, 0x101e7a58: 180, 0x101e7b34: 168, 0x101e8068: 152, 0x101e812c: 156, 0x101e81f4: 184}
OWN = set(SIZES)

ADDR = dict(fontIdx=0x102b0c4c, msgcol=0x102b0c58, concol=0x102b0c78, busy=0x10395e7c, screenH=0x10395bba, panel=0x10395e98)
HOST_LAYOUT = []

# PPC addresses of the objects (windows are never dereferenced; the font and the texts are persistent memory made by setup)
OBJ = dict(winP=0x30000100, winA=0x30000200, winB=0x30000300, winW=0x30000400, font=0, text1=0, text2=0, cb=0x10268548)
LABELS = dict(font=5, winP=10, winA=11, winB=12, winW=20, text1=31, text2=32, cb=40)

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s32(v):
    return T.s32(v)


STUBS.internal(0x10180270, CL.recorder("GetFontTableEntry", "u", ret=lambda e, L: OBJ["font"]))
STUBS.internal(0x10168fc0, CL.recorder("GetHeight", "L", ret=lambda e, L: L.ret("GetHeight") & MASK))
STUBS.internal(0x101dcba8, CL.recorder("SetCallbackFunction", "LL"))
STUBS.internal(0x101db624, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(0x101da118, CL.recorder("SetFont", "LL"))
STUBS.internal(0x101da3ac, CL.recorder("SetFontColour", "Liiii"))
STUBS.internal(0x101de318, CL.recorder("CallDefaultWindowHandler", "Liii", ret="i"))
STUBS.internal(0x1018d11c, CL.recorder("SpriteCtor", "N"))
STUBS.internal(0x101d98d8, CL.recorder("WindowCtor", "NLiihhhh"))
STUBS.internal(0x101d2220, CL.recorder("UpdateDisplay", "L"))
STUBS.internal(0x101d3fcc, CL.recorder("SetData", "Lii"))
STUBS.internal(0x101d39f0, CL.recorder("RemoveItem", "Li", ret=lambda e, L: 0))
STUBS.internal(0x101d44b8, CL.recorder("FindString", "LLi", ret="i"))


def st_new(e):
    L = e.cl
    L.rec("OperatorNew", (e.r(3) & MASK,))
    e.set_r(3, 0 if L.ret("OperatorNewNull") else e.alloc(e.r(3)))


STUBS.internal(0x102384b8, st_new)


def st_getwindow(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    e.set_r(3, {0xc3123: OBJ["winA"], 0xc3124: OBJ["winB"]}.get(wid, 0))


STUBS.internal(0x101dbe40, st_getwindow)


def st_additem(e):
    L = e.cl
    text = e.u32(e.r(4))
    L.rec("AddItem", (L.lab(e.r(3)), L.lab(text), s32(e.r(5)), s32(e.r(6)), s32(e.r(7))))
    e.set_r(3, 0)


STUBS.internal(0x101d3000, st_additem)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    tv = T.persist_alloc(e, 8, struct.pack(">II", 0x10168fc0, 0))
    vt = T.persist_alloc(e, 0x20, b"\0" * 0xc + struct.pack(">I", tv) + b"\0" * 0x10)
    OBJ["font"] = T.persist_alloc(e, 0x20, b"\0" * 0x10 + struct.pack(">I", vt) + b"\0" * 0xc)
    OBJ["text1"] = T.persist_alloc(e, 16, b"\0" * 16)
    OBJ["text2"] = T.persist_alloc(e, 16, b"\0" * 16)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def half(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "h", v))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {OBJ[k]: LABELS[k] for k in LABELS}
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def obj(self, name):
        return OBJ[name] if name else 0

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def set_panel(self, on):
        self.words(ADDR["panel"], [OBJ["winP"] if on else 0])

    def call(self, op, args):
        r3, _, _ = T.call_ppc(self.e, FN[op], [a & MASK for a in args])
        if op == "fmt":
            return self.L.lab(r3 & MASK)
        if op in ("conh", "msgh"):
            return s32(r3)
        return 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {LABELS[k]: lib.tpw_cl_addr(k.encode()) for k in LABELS}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cl_addr(name.encode())

    def obj(self, name):
        return self.lib.tpw_cl_addr(name.encode()) if name else 0

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_panel(self, on):
        self.lib.tpw_cl_setpanel(1 if on else 0)

    def call(self, op, args):
        r = getattr(self.lib, "tpw_cl_" + op)(*args)
        return r if op in ("fmt", "conh", "msgh") else 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_cl_addr.restype, lib.tpw_cl_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cl_alloc.restype, lib.tpw_cl_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cl_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cl_setpanel.argtypes = [C.c_int32]
    lib.tpw_cl_fmt.restype = C.c_int64
    lib.tpw_cl_post.argtypes = [C.c_uint64]
    for n in ("conh", "msgh"):
        f = getattr(lib, "tpw_cl_" + n)
        f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_cl_add.argtypes = [C.c_uint64, C.c_int32]
    lib.tpw_cl_rem.argtypes = [C.c_uint64]
    lib.tpw_cl_upd.argtypes = [C.c_uint64, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


def build(s, case):
    s.words(s.g("fontIdx"), [case["fontidx"]])
    s.half(s.g("screenH"), case["screen"])
    s.write(s.g("msgcol"), bytes(case["msgcol"]))
    s.words(s.g("concol"), case["concol"])
    s.words(s.g("busy"), [case["busy"]])
    s.set_panel(case["panel"])


def execute(s, case):
    op, a = case["op"], case["args"]
    if op == "fmt":
        return s.call("fmt", [])
    if op == "post":
        return s.call("post", [s.obj("winW")])
    if op in ("conh", "msgh"):
        return s.call(op, [s.obj("winW")] + a)
    if op == "rem":
        return s.call("rem", [s.obj(a[0])])
    return s.call(op, [s.obj(a[0]), a[1]])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "fontIdx": s.rd_words(s.g("fontIdx"), 1), "msgcol": s.read(s.g("msgcol"), 32),
            "concol": s.rd_words(s.g("concol"), 4), "busy": s.rd_words(s.g("busy"), 1)}


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


def base(r, op):
    return {"op": op, "args": [],
            "queues": {"CallDefaultWindowHandler": [r.choice([0, 1, -1, r.randrange(-1000, 1000)])],
                       "GetHeight": [r.choice([0, 1, 12, -5, 32767, 0x12345, -0x8000, 0x8000, r.randrange(-100, 100)])],
                       "FindString": [r.choice([-1, -2, 0, 1, 5, 0x7FFFFFFF, r.randrange(-3, 10)])],
                       "OperatorNewNull": [r.choice([0, 0, 0, 1])]},
            "panel": r.choice([0, 1, 1]), "busy": r.choice([0, 0, 1, 0x100, -1]),
            "fontidx": r.choice([7, 0, 5, r.randrange(0, 13)]),
            "screen": r.choice([600, 480, 1, -1, 768, r.randrange(1, 2000), -r.randrange(1, 2000)]),
            "msgcol": [r.randrange(256) for _ in range(32)], "concol": [r.getrandbits(32) for _ in range(4)]}


TEXTS = [None, "text1", "text2"]


def make(r, op):
    c = base(r, op)
    if op in ("conh", "msgh"):
        c["args"] = [r.choice([0x403, 0x403, 0x1e, 0x402, 0x404, 0, -1, r.getrandbits(32)]) & MASK, r.getrandbits(32),
                     r.choice([r.randrange(0, 16), r.getrandbits(32), 0xFFFFFFFF])]
    elif op in ("add", "upd"):
        c["args"] = [r.choice(TEXTS), r.choice([0, -1, 1, r.getrandbits(32)])]
    elif op == "rem":
        c["args"] = [r.choice(TEXTS)]
    return c


def edges(op, seed):
    def gen():
        r = random.Random(seed)
        if op in ("conh", "msgh"):
            for msg in (0x403, 0x1e, 0x402, 0x404, 0, -1):
                for b in range(16) if msg == 0x403 else (0, 7):
                    c = make(r, op)
                    c["args"] = [msg & MASK, 0x1234, b]
                    yield c
        elif op in ("fmt", "post"):
            for null in (0, 1):
                for screen in (600, 1, -1):
                    for h in (0, 1, 12, -5, 32767, 0x12345, -0x8000):
                        c = make(r, op)
                        c["queues"]["OperatorNewNull"] = [null]
                        c["queues"]["GetHeight"] = [h]
                        c["screen"] = screen
                        yield c
        else:
            for panel in (0, 1):
                for busy in (0, 1, -1):
                    for fs in (-2, -1, 0, 1, 99):
                        c = make(r, op)
                        c["panel"], c["busy"] = panel, busy
                        c["queues"]["FindString"] = [fs]
                        yield c
    return gen


def rnd(op):
    return lambda r: make(r, op)


TESTS = [FnTest(n, ppc, host, edge=edges(op, i), rnd=rnd(op))
         for i, (n, op) in enumerate([("ContactsLCFormat", "fmt"), ("ContactsLCPostFormat", "post"),
                                      ("MessagesLCEntryHandler", "msgh"), ("ContactsLCEntryHandler", "conh"),
                                      ("_OnlineAddContact", "add"), ("_OnlineRemoveContact", "rem"),
                                      ("_OnlineUpdateContact", "upd")], 1)]
TESTS.append(imagedata.table_test("the list colours", [("msgcol", ADDR["msgcol"], "BBBB", 8), ("concol", ADDR["concol"], "I", 4)]))
