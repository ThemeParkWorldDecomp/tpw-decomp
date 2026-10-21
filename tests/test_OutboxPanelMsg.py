"""Unit v5239, the online outbox panel's message function and the function that sends and deletes the outbox, against
libtpw_unit_OutboxPanelMsg.dylib (tests/unit_OutboxPanelMsg.cpp), main app ("SimTheme Park"),
src/game/ui/ui_onlineOutbox.cpp:
  OutboxPanelMsg 0x101e458c, SendAndDeleteOutbox 0x101e4378

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory for the translation. The callees outside the unit (UI_SendMessage, the list control's
GetData/SetData/ChangeItem/UpdateDisplay, GetWindow, MarkOutboxEmail, the constructors, CanWeSendEmail, LoggedInGetNews, Outbox::Delete and
Send, UI_DialogBox, SetCallbackFunction, Show, AddUIError, DefaultDialogCallback and the rest) are fakes on both sides that record their
arguments (objects as labels) and answer from the case's queues.

The world of a case: the pointer globals (user info, online system, outbox window, sending window; the lobby is always there), the user
info's outbox list size (the Mac's word at 0x8c) and its word at 0x58, the list's entry count, the mail server text and port, the four
mail status words, the answers of the fakes. What is compared: the answer, the event log, the pointer globals (as labels), the four mail
status words, whether the outbox kept the callback vector (its word at 0x58).

No fused operations. Not tested: a NULL lobby (the original reads it unconditionally), a NULL list from GetWindow(0xee8) (read at 0x150),
the size passed to operator new (the same fakes record the constructors only), an uninitialised GetData result (the fake always writes).
"""
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OutboxPanelMsg"
MASK = 0xFFFFFFFF

FN_PANEL, FN_SEND = 0x101E458C, 0x101E4378
SIZES = {FN_PANEL: 1336, FN_SEND: 488}
OWN = set(SIZES)

ADDR = dict(
    gUserInfo=0x10380B34, gOnlineSystem=0x10380AB0, gLobby=0x102E4888, gOutboxWindow=0x10395E3C,
    gOutboxSendingWindow=0x10395E0C, gOutboxPreview=0x10395E10, gOnlineConfig=0x102B2B70,
    gMailStatusShownB=0x10395E00, gMailStatusShownA=0x10395E08, gMailStatusB=0x10395DFC, gMailStatusA=0x10395E04,
)
CALLBACK_VECTOR = 0x10268500
SENDING_MSG = 0x102684F8
STATUS = ["gMailStatusShownB", "gMailStatusShownA", "gMailStatusB", "gMailStatusA"]

LABEL = {"user": 1, "online": 3, "list": 4, "lobby": 5, "win": 6, "owin": 8, "lwin": 9, "child": 10, "sendwin": 11}
TOKENS = {1: "list", 2: "child", 3: "sendwin"}
BLK = {}


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def blk_or_raw(token):
    token &= MASK
    return BLK[TOKENS[token]] if token in TOKENS else token


def stub_getwindow(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GetWindow", (L.lab(e.r(3)), wid))
    e.set_r(3, blk_or_raw(L.ret("GetWindow:%d" % wid)))


def stub_getdata(e):
    L = e.cl
    pos = s32(e.r(4))
    L.rec("GetData", (L.lab(e.r(3)), pos))
    e.write(e.r(5), struct.pack(">I", L.ret("GetData:%d" % pos) & MASK))
    e.set_r(3, 1)


def stub_changeitem(e):
    L = e.cl
    item = e.r(5)
    L.rec("ChangeItem", (L.lab(e.r(3)), s32(e.r(4)), s32(e.u32(item + 8)), s32(e.u32(item + 12)), s32(e.r(6)), s32(e.r(7)),
                         s32(e.r(8))))


def stub_dialogbox(e):
    L = e.cl
    p = e.r(3)
    rgb = e.read(p + 4, 3)
    L.rec("UI_DialogBox", (e.u32(p), rgb[0] | (rgb[1] << 8) | (rgb[2] << 16), s32(e.u32(p + 8)), s32(e.u32(p + 12)), s32(e.r(4)), e.r(5), s32(e.r(6)),
                           e.r(7), s32(e.r(8)), e.r(9), e.r(10) & 0xFF))
    e.set_r(3, blk_or_raw(L.ret("UI_DialogBox")))


def stub_setcallback(e):
    L = e.cl
    L.rec("SetCallbackFunction", (L.lab(e.r(3)), 1 if e.r(4) == SENDING_MSG else 0))


for addr, fn in (
    (0x101C9518, CL.recorder("UI_SendMessage", "Liii")),
    (0x10239028, CL.recorder("FreeMemory", "L")),
    (0x1018116C, CL.recorder("UIMain_DecideKbCapture", "")),
    (0x101DBE40, stub_getwindow),
    (0x101D41F0, stub_getdata),
    (0x101E3960, CL.recorder("SetCurrentEmailInfo", "i")),
    (0x101E3AAC, CL.recorder("CalcEmailAddressScrollBar", "")),
    (0x101E3C94, CL.recorder("UpdatePreview", "")),
    (0x101E3E98, CL.recorder("ColumnClick", "i")),
    (0x101D3660, stub_changeitem),
    (0x101D3FCC, CL.recorder("SetData", "Lii")),
    (0x101D2220, CL.recorder("UpdateDisplay", "L")),
    (0x1017DE54, CL.recorder("CFSUserInfo", "N")),
    (0x101E4004, CL.recorder("MarkOutboxEmail", "i", ret="i")),
    (0x1016F7D0, CL.recorder("CFSOnlineSystem", "N")),
    (0x101717CC, CL.recorder("CanWeSendEmail", "L", ret="i")),
    (0x101F1DF0, CL.recorder("AddUIError", "iL")),
    (0x10171EB8, CL.recorder("LoggedInGetNews", "L")),
    (0x101F1448, CL.recorder("DefaultDialogCallback", "Liii", ret="i")),
    (0x10180468, CL.recorder("UIMain_GetString", "i", ret="i")),
    (0x10206278, CL.recorder("OutboxDelete", "L")),
    (0x102059FC, CL.recorder("OutboxSend", "LshL", ret="b")),
    (0x101F1800, stub_dialogbox),
    (0x101DCBA8, stub_setcallback),
    (0x101DC1EC, CL.recorder("Show", "Lh")),
):
    STUBS.internal(addr, fn)


def setup(runner):
    T.install_std_stubs(runner.emu)
    for name in LABEL:
        BLK[name] = T.persist_alloc(runner.emu, 0x2000)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def label_of(self, value):
        if value == 0:
            return 0
        for name, label in LABEL.items():
            if value == self.blk(name):
                return label
        return 99


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {BLK[n]: LABEL[n] for n in LABEL}
        fixed[BLK["user"] + 0x64] = 2
        fixed[ADDR["gOutboxPreview"]] = 7
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def blk(self, name):
        return BLK[name]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def world(self, size, count):
        self.write(BLK["user"], bytes(0x200))
        self.write(BLK["list"], bytes(0x200))
        self.write(BLK["lobby"], bytes(0x40))
        self.words(BLK["user"] + 0x8C, [size])
        self.words(BLK["list"] + 0x150, [count])
        self.words(BLK["lobby"] + 4, [BLK["lwin"]])

    def set_ptr(self, gname, value):
        self.words(ADDR[gname], [value])

    def get_ptr(self, gname):
        return self.rd_word(ADDR[gname])

    def server(self, text, port):
        chars = self.e.alloc(len(text) + 1, text.encode() + b"\0")
        rep = self.e.alloc(0x20)
        self.words(rep + 0xC, [chars])
        self.words(ADDR["gOnlineConfig"], [rep])
        self.write(ADDR["gOnlineConfig"] + 0x18, struct.pack(">H", port & 0xFFFF))

    def callback_set(self, user):
        if user == 0:
            return MASK
        word = self.rd_word(user + 0xBC)
        return 0 if word == 0 else 1 if word == CALLBACK_VECTOR else word

    def call_send(self, send):
        T.call_ppc(self.e, FN_SEND, [send])
        return 0

    def call_panel(self, window, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_PANEL, [window, m & MASK, a & MASK, b & MASK])
        return s32(r3)


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        fixed = {LABEL[n]: self.blk(n) for n in LABEL}
        fixed[2] = self.g("outbox")
        fixed[7] = self.g("gOutboxPreview")
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ob_addr(name.encode())

    def blk(self, name):
        return self.lib.tpw_ob_addr(name.encode())

    def read(self, a, n):
        import ctypes as C
        return C.string_at(a, n)

    def write(self, a, data):
        import ctypes as C
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def world(self, size, count):
        self.lib.tpw_ob_world(size & MASK, s32(count))

    def set_ptr(self, gname, value):
        self.lib.tpw_ob_setptr(gname.encode(), value)

    def get_ptr(self, gname):
        return self.lib.tpw_ob_getptr(gname.encode())

    def server(self, text, port):
        self.lib.tpw_ob_server(text.encode(), port)

    def callback_set(self, user):
        return self.lib.tpw_ob_callback_set(user)

    def call_send(self, send):
        self.lib.tpw_ob_send(send)
        return 0

    def call_panel(self, window, m, a, b):
        return self.lib.tpw_ob_panel(window, s32(m), s32(a), s32(b))


def bind(lib):
    import ctypes as C
    if getattr(lib, "bound", False):
        return
    lib.tpw_ob_addr.restype, lib.tpw_ob_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ob_setptr.restype, lib.tpw_ob_setptr.argtypes = None, [C.c_char_p, C.c_uint64]
    lib.tpw_ob_getptr.restype, lib.tpw_ob_getptr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ob_world.restype, lib.tpw_ob_world.argtypes = None, [C.c_uint32, C.c_int32]
    lib.tpw_ob_callback_set.restype, lib.tpw_ob_callback_set.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ob_server.restype, lib.tpw_ob_server.argtypes = None, [C.c_char_p, C.c_int32]
    lib.tpw_ob_send.restype, lib.tpw_ob_send.argtypes = None, [C.c_int32]
    lib.tpw_ob_panel.restype, lib.tpw_ob_panel.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.world(case["lsize"], case["count"])
    s.set_ptr("gUserInfo", s.blk("user") if case["user"] else 0)
    s.set_ptr("gOnlineSystem", s.blk("online") if case["online"] else 0)
    s.set_ptr("gLobby", s.blk("lobby"))
    s.set_ptr("gOutboxWindow", s.blk("owin") if case["owin"] else 0)
    s.set_ptr("gOutboxSendingWindow", s.blk("sendwin") if case["sendwin"] else 0)
    for name, v in zip(STATUS, case["mail"]):
        s.words(s.g(name), [v])
    s.server(*case["server"])


def execute(s, case):
    if case["op"] == "send":
        return s.call_send(case["send"])
    return s.call_panel(s.blk("win"), *case["msg"])


def observe(s, case, answer):
    user = s.get_ptr("gUserInfo")
    return {
        "answer": answer,
        "log": s.log(),
        "gUserInfo": s.label_of(user),
        "gOnlineSystem": s.label_of(s.get_ptr("gOnlineSystem")),
        "gOutboxWindow": s.label_of(s.get_ptr("gOutboxWindow")),
        "gOutboxSendingWindow": s.label_of(s.get_ptr("gOutboxSendingWindow")),
        "status": [s.rd_word(s.g(n)) for n in STATUS],
        "callback": s.callback_set(user if user else 0),
    }


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

FLAGS = [0, 0x01000000, 0x02000000, 0x03000000, 0xFFFFFFFF, 0xFCFFFFFF, 0x00FFFFFF, 0x12345678]
SERVERS = [("", 25), ("mail.example.org", 110), ("x", 0x7FFF), ("smtp", -1), ("", -32768), ("host", 0)]


def base(r, op):
    q = {}
    for pos in range(5):
        q["GetData:%d" % pos] = [r.choice(FLAGS) if r.random() < 0.7 else r.getrandbits(32)]
    q["GetWindow:%d" % 0xEE8] = [1]
    q["GetWindow:%d" % 0xBF432] = [r.choice([0, 2])]
    q["MarkOutboxEmail"] = [r.choice([-2, -1, 0, 0, 1, 2, 3]) for _ in range(4)]
    q["CanWeSendEmail"] = [r.choice([0, 1])]
    q["OutboxSend"] = [r.choice([0, 1])]
    q["UI_DialogBox"] = [r.choice([0, 3])]
    q["UIMain_GetString"] = [0x7000 + r.randrange(100)]
    q["DefaultDialogCallback"] = [r.choice([0, 1, -1, r.randrange(1000)])]
    return {
        "op": op, "queues": q, "user": r.choice([0, 1]), "online": r.choice([0, 1]), "owin": r.choice([0, 1]),
        "sendwin": r.choice([0, 1]), "lsize": r.choice([0, 0, 2]), "count": r.randrange(0, 5),
        "server": r.choice(SERVERS), "mail": [r.choice([0, -1, 1, 0x12345678]) for _ in range(4)],
    }


def panel_case(r, msg, **over):
    c = base(r, "panel")
    c["msg"] = msg
    if msg[0] == 0x401:
        c["queues"]["GetWindow:%d" % msg[1]] = [r.choice([0, 1])]
    c.update(over)
    return c


def panel_edges():
    r = random.Random(5239)
    for m in (0x14, 0x15, 0x404, 0x0, 0x13, 0x16, 0xFF, 0x101, 0x400, 0x402, 0x403, 0x405, 0x1000, -1):
        yield panel_case(r, (m, r.randrange(-3, 5), r.choice([-1, 0, 7])))
    for a in (0xEE5, 0xEE6, 0xEE7, 0xEE4, 0xEE8, 0, -2, 0x1234):
        for count in (0, 1, 3):
            yield panel_case(r, (0x100, a, 0), count=count)
    for a in (0xEE8, 7):
        for b in (-1, 0, 2, 4):
            for tok in (0, 1):
                c = panel_case(r, (0x401, a, b))
                c["queues"]["GetWindow:%d" % a] = [tok]
                yield c
    for user in (0, 1):
        for online in (0, 1):
            for marks in ([0], [1], [-1], [2, -2], [1, 1, 1], [-1, -1, 1]):
                for can in (0, 1):
                    for count in (0, 3):
                        c = panel_case(r, (0x100, -1, 0), user=user, online=online, count=count)
                        c["queues"]["MarkOutboxEmail"] = marks
                        c["queues"]["CanWeSendEmail"] = [can]
                        yield c
    for srv in SERVERS:
        for sent in (0, 1):
            for lsize in (0, 2):
                for user in (0, 1):
                    c = panel_case(r, (0x100, -1, 0), server=srv, lsize=lsize, user=user, count=2)
                    c["queues"]["MarkOutboxEmail"] = [1]
                    c["queues"]["CanWeSendEmail"] = [1]
                    c["queues"]["OutboxSend"] = [sent]
                    yield c


def panel_random(r):
    m = r.choice([0x100, 0x100, 0x100, 0x401, 0x404, 0x14, 0x15, r.randrange(0, 0x500)])
    a = r.choice([0xEE5, 0xEE6, 0xEE7, -1, -1, 0xEE8, 7, r.randrange(-5, 0x1000)]) if m == 0x100 else r.choice([0xEE8, 7, 0x10, -1])
    return panel_case(r, (m, a, r.choice([-1, 0, 1, 3, r.randrange(-100, 100)])))


def send_case(r, send=None, **over):
    c = base(r, "send")
    c["send"] = r.choice([0, 1]) if send is None else send
    c.update(over)
    return c


def send_edges():
    r = random.Random(4378)
    for send in (0, 1):
        for user in (0, 1):
            for srv in SERVERS:
                for sent in (0, 1):
                    for lsize in (0, 2):
                        c = send_case(r, send, user=user, server=srv, lsize=lsize)
                        c["queues"]["OutboxSend"] = [sent]
                        yield c


def send_random(r):
    return send_case(r)


TESTS = [
    FnTest("OutboxPanelMsg", ppc, host, edge=panel_edges, rnd=panel_random),
    FnTest("SendAndDeleteOutbox", ppc, host, edge=send_edges, rnd=send_random),
]
