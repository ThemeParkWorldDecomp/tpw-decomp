"""The lobby's chat satellite and the world lobby window's messages, against libtpw_unit_ProcessGetChatSatellitePlayers.dylib
(tests/unit_ProcessGetChatSatellitePlayers.cpp), main app ("SimTheme Park"), src/game/ui/ui_worldLobbyController.cpp:
  ChatServerCallbackFn 0x1022a44c, ProcessGetChatSatellitePlayers 0x1022a668, WorldLobbyControllerMsg 0x1022aa20

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian, 8-byte pointers) for the translation; the same code reads both back.
The callees outside the unit (operator new, the online system's constructor, Instance, ReturnParkInfo, the Chat calls, the strings, the
text object, UpdateSatellite, the globe, the advisor, the find panel, getExtraParam, CallDefaultWindowHandler and the virtual Show, Hide
and Slot50 of the lobby screens) are fakes on both sides that record their arguments and answer from the case's queues. Texts are logged
as a crc32 of the characters.

The world of a case: the six chat satellite globals, the online system (built, or made by the constructor fake from the same numbers),
the five lists' flags and the park list's count, the configuration's chat server and port, the lobby screens (four objects with a
vtable), the satellite parent pointer, an argument vector (the elements' commands). What is compared: the answer, the globals, where the
pointer globals point (null, an object of the world, or a new one) and the event log.

No fused operations. Not tested: a NULL gOnlineSystem or gLobbyScreens read by the original when it does not make one (gLobbyScreens
is never made), a count above the five of the globals' tables (the original clamps it to five).
"""
import ctypes as C
import random
import struct
import zlib

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ProcessGetChatSatellitePlayers"
MASK = 0xFFFFFFFF

FN_CB, FN_PROC, FN_MSG = 0x1022A44C, 0x1022A668, 0x1022AA20
SIZES = {FN_CB: 440, FN_PROC: 776, FN_MSG: 972}
OWN = set(SIZES)

ADDR = dict(wanted=0x10396FE8, chat=0x10396FE4, finished=0x10396FB4, answers=0x10396FB8, values=0x10396FBC, ids=0x10396FD0,
            sysptr=0x10380AB0, cfg=0x102B2B70, screens=0x102E49B8, satparent=0x10396FEC)
TV_FIND, TV_CHATCB = 0x102689B0, 0x102689B8
TV_SHOW, TV_HIDE, TV_SLOT = 0x10268F00, 0x10268F08, 0x10268F10
TVNAME = {TV_SHOW: "Show", TV_HIDE: "Hide", TV_SLOT: "Slot50"}

LAYOUT_NAMES = ["city", "parks", "theme_l", "server_l", "f28", "list", "count", "parkid", "scr_theme", "scr_dl", "scr_pub", "scr_adv",
                "vsize", "vdata", "argsize"]
MAC_OFF = [0xE0, 0xF4, 0xF8, 0xFC, 0x28, 0x2C, 0xC, 8, 4, 0xC, 0x10, 0x14, 4, 8, 12]
OBJECTS = ["window", "sys", "screens", "theme", "dl", "pub", "adv", "chat0", "cityl", "parkl", "themel", "serverl", "table", "vt"]
LABELS = {"window": 1, "sys": 2, "screens": 3, "theme": 4, "dl": 5, "pub": 6, "adv": 7, "chat0": 8, "parkl": 11}


def s32(v):
    return T.s32(v)


def crc(text):
    if not isinstance(text, bytes):
        text = text.encode("latin-1")
    return zlib.crc32(text) & 0x7FFFFFFF


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def glue(fn):
    """The glue saves r2 at 0x14(r1) and the caller reloads it after the call: do the save the stubbed glue does not."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        return fn(e)
    return wrapped


def cstr(e, a):
    s = e.cstr(a)
    return s if isinstance(s, bytes) else s.encode("latin-1")


def mac_string(e, text):
    """A Mac string object's word: a rep {len, cap, refs, data*} and its characters."""
    data = e.alloc(len(text) + 1)
    e.write(data, text + b"\0")
    rep = e.alloc(16)
    e.write(rep, struct.pack(">IIII", len(text), len(text), 1, data))
    return rep


def stub_new(e):
    L = e.cl
    size = e.r(3)
    L.rec("OperatorNew", (size,))
    if size == 0xC and L.ret("newnull"):
        e.set_r(3, 0)
    else:
        e.set_r(3, e.alloc(size))


def fill_system(e, log):
    system = e.r(3)
    L = e.cl
    lists = []
    for key in ("c_e0", "c_f4", "c_f8", "c_fc"):
        block = e.alloc(0x100)
        e.write(block + 0x28, struct.pack(">I", L.ret(key) & MASK))
        if key == "c_f4":
            table = e.alloc(0x40)
            e.write(table + 0xC, struct.pack(">I", L.ret("c_cnt") & MASK))
            e.write(block + 0x2C, struct.pack(">I", table))
        lists.append(block)
    e.write(system + 0xE0, struct.pack(">I", lists[0]))
    e.write(system + 0xF4, struct.pack(">I", lists[1]))
    e.write(system + 0xF8, struct.pack(">I", lists[2]))
    e.write(system + 0xFC, struct.pack(">I", lists[3]))
    return system


def stub_instance(e):
    e.cl.rec("Instance", ())
    e.set_r(3, e.u32(ADDR["sysptr"]))


def stub_park_info(e):
    L = e.cl
    index = s32(e.r(4))
    L.rec("ReturnParkInfo", (L.lab(e.r(3)), index))
    block = e.alloc(0x40)
    e.write(block + 8, struct.pack(">I", L.ret("park:%d" % index) & MASK))
    e.set_r(3, block)


def stub_connect(e):
    L = e.cl
    L.rec("ChatConnect", (L.lab(e.r(3)), crc(cstr(e, e.r(4))), e.r(5) & 0xFFFF, L.lab(e.r(6)), e.r(7)))
    e.set_r(3, L.ret("connect") & MASK)


def stub_udebug(e):
    e.cl.rec("UDebug", (crc(cstr(e, e.r(3))),))


def stub_dynctor(e):
    e.cl.rec("DynCtor", ())


def stub_format(e):
    L = e.cl
    value = s32(e.r(5))
    L.rec("Format", (crc(cstr(e, e.r(4))), value))
    buffer = e.alloc(16)
    e.write(buffer, b"%d\0" % value)
    e.write(e.r(3) + 8, struct.pack(">I", buffer))


def stub_string_ctor(e):
    L = e.cl
    text = cstr(e, e.r(4))
    L.rec("StringCtor", (crc(text),))
    e.write(e.r(3), struct.pack(">I", mac_string(e, text)))


def stub_room_info(e):
    L = e.cl
    rep = e.u32(e.r(4))
    text = cstr(e, e.u32(rep + 0xC))
    L.rec("RoomInfo", (L.lab(e.r(3)), crc(text), e.r(5)))


def stub_string_dtor(e):
    e.cl.rec("StringDtor", (T.s16(e.r(4)),))


def stub_dyndtor(e):
    e.cl.rec("DynDtor", ())


def stub_release(e):
    L = e.cl
    L.rec("ChatRelease", (L.lab(e.r(3)),))


def stub_update(e):
    e.cl.rec("UpdateSatellite", (s32(e.r(3)), s32(e.r(4))))


def stub_strval(e):
    L = e.cl
    L.rec("StrVal", (e.u32(e.r(4) + 8),))
    e.write(e.r(3), struct.pack(">I", mac_string(e, b"%d" % s32(L.ret("strval")))))


def stub_substr(e):
    e.cl.rec("StrSub", (s32(e.r(5)), s32(e.r(6))))
    e.write(e.r(3), struct.pack(">I", e.u32(e.r(4))))


def stub_atoi(e):
    text = cstr(e, e.r(3))
    try:
        value = int(text)
    except ValueError:
        value = 0
    e.set_r(3, value & MASK)


def stub_extra(e):
    L = e.cl
    L.rec("GetExtraParam", (L.lab(e.r(3)), s32(e.r(4))))
    e.set_r(3, L.ret("extra") & MASK)


def stub_default(e):
    L = e.cl
    L.rec("CallDefault", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("default") & MASK)


def stub_processing(e):
    L = e.cl
    L.rec("AreWeProcessing", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("processing") & MASK)


def globe(name):
    def stub(e):
        L = e.cl
        L.rec(name, (L.lab(e.r(3)),))
    return stub


def stub_ptr_glue(e):
    L = e.cl
    L.rec(TVNAME[e.r(12)], (L.lab(e.r(3)),))


def stub_find_panel(e):
    L = e.cl
    L.rec("CreateParkFind", (L.lab(e.r(3)),))


def stub_advisor(e):
    L = e.cl
    L.rec("AdvisorAdd", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5))))


STUBS.internal(0x102384B8, glue(stub_new))
STUBS.internal(0x1016F7D0, CL.recorder("SystemCtor", "N", ret=fill_system))
STUBS.internal(0x10178618, stub_instance)
STUBS.internal(0x101791D8, stub_park_info)
STUBS.internal(0x1023A9F0, glue(CL.recorder("ChatCtor", "N")))
STUBS.internal(0x1023AA08, glue(stub_connect))
STUBS.internal(0x10000E40, stub_udebug)
STUBS.internal(0x10238458, glue(stub_dynctor))
STUBS.internal(0x1023AA20, glue(stub_format))
STUBS.internal(0x10002338, stub_string_ctor)
STUBS.internal(0x1023AA38, glue(stub_room_info))
STUBS.internal(0x1000221C, stub_string_dtor)
STUBS.internal(0x102382F0, glue(stub_dyndtor))
STUBS.internal(0x1023AA50, glue(stub_release))
STUBS.internal(0x1022B8D0, stub_update)
STUBS.internal(0x10217684, stub_strval)
STUBS.internal(0x101B2918, stub_substr)
STUBS.internal(0x10238590, glue(stub_atoi))
STUBS.internal(0x101DD030, stub_extra)
STUBS.internal(0x101DE318, stub_default)
STUBS.internal(0x10171498, stub_processing)
for _addr, _name in ((0x100ADB54, "GlobeLeft"), (0x100ADBA8, "GlobeRight"), (0x100ADBFC, "GlobeUp"), (0x100ADC50, "GlobeDown"),
                     (0x100ADCA4, "GlobeZoomIn"), (0x100ADD2C, "GlobeZoomOut")):
    STUBS.internal(_addr, globe(_name))
STUBS.internal(0x1023BAA8, glue(stub_ptr_glue))
STUBS.internal(0x101E9FF8, stub_find_panel)
STUBS.internal(0x100A7074, stub_advisor)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def ptr(self, a, v):
        self.write(a, struct.pack(self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.pfmt, self.read(a, struct.calcsize(self.pfmt)))[0]

    def make_objects(self):
        self.o = {name: self.alloc(0x400) for name in OBJECTS}

    def kind(self, v):
        """Where a pointer global points: null, an object of the world, or something new."""
        if v == 0:
            return "null"
        for name, a in self.o.items():
            if a == v:
                return name
        return "new"


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"
    pfmt = ">I"
    O = dict(zip(LAYOUT_NAMES, MAC_OFF))

    def __init__(self, e, case):
        self.e = e
        self.make_objects()
        fixed = {self.o[n]: lab for n, lab in LABELS.items()}
        fixed[TV_FIND] = 9
        fixed[TV_CHATCB] = 10
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
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

    def log(self):
        return self.L.log

    def vtable(self):
        vt = self.o["vt"]
        self.words(vt + 0x24, [TV_SHOW])
        self.words(vt + 0x28, [TV_HIDE])
        self.words(vt + 0x50, [TV_SLOT])
        return vt

    def set_cfg(self, text, port):
        cfg = ADDR["cfg"]
        self.write(cfg + 4, struct.pack(">I", mac_string(self.e, text)))
        self.write(cfg + 0x1A, struct.pack(">H", port & 0xFFFF))

    def call(self, op, args):
        entry = {"cb": FN_CB, "proc": FN_PROC, "msg": FN_MSG}[op]
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK if op == "msg" else 0


class Host(Side):
    """The same on the host library."""
    fmt = "<I"
    pfmt = "<Q"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.O = lib.layout
        self.make_objects()
        fixed = {lab: self.o[n] for n, lab in LABELS.items()}
        fixed[9] = self.g("findcb")
        fixed[10] = self.g("chatcb")
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_cs_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_cs_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def vtable(self):
        return self.g("vtable")

    def set_cfg(self, text, port):
        self.lib.tpw_cs_setcfg(text, port)

    def call(self, op, args):
        if op == "cb":
            self.lib.tpw_cs_cb(s32(args[0]), args[2])
            return 0
        if op == "proc":
            self.lib.tpw_cs_proc()
            return 0
        return self.lib.tpw_cs_msg(args[0], s32(args[1]), s32(args[2]), s32(args[3])) & MASK


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_cs_addr.restype, lib.tpw_cs_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_cs_alloc.restype, lib.tpw_cs_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_cs_setcfg.restype, lib.tpw_cs_setcfg.argtypes = None, [C.c_char_p, C.c_int32]
    lib.tpw_cs_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_cs_proc.restype, lib.tpw_cs_proc.argtypes = None, []
    lib.tpw_cs_cb.restype, lib.tpw_cs_cb.argtypes = None, [C.c_int32, C.c_uint64]
    lib.tpw_cs_msg.restype, lib.tpw_cs_msg.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_cs_layout(out)
    assert n == len(LAYOUT_NAMES), n
    lib.layout = dict(zip(LAYOUT_NAMES, list(out)[:n]))
    assert lib.layout["argsize"] == 12, lib.layout
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    """Writes the case's globals and blocks through Side.words/ptr: the same code for both memories (offsets are per side)."""
    O, o = s.O, s.o
    s.words(s.g("wanted"), [case["wanted"]])
    s.words(s.g("finished"), [case["finished"]])
    s.words(s.g("answers"), [case["answers"]])
    s.words(s.g("values"), case["values"])
    s.words(s.g("ids"), case["ids"])
    s.ptr(s.g("chat"), o["chat0"] if case["chat0"] else 0)
    s.ptr(s.g("sysptr"), o["sys"] if case["sys"] else 0)
    s.ptr(s.g("satparent"), o["window"] if case["sat"] else 0)
    s.ptr(s.g("screens"), o["screens"])
    sysb = o["sys"]
    f28 = case["f28"]
    for field, name, flag in (("city", "cityl", f28[0]), ("parks", "parkl", f28[1]), ("theme_l", "themel", f28[2]),
                              ("server_l", "serverl", f28[3])):
        s.ptr(sysb + O[field], o[name])
        s.words(o[name] + O["f28"], [flag])
    s.ptr(o["parkl"] + O["list"], o["table"])
    s.words(o["table"] + O["count"], [case["count"]])
    vt = s.vtable()
    for field, name in (("scr_theme", "theme"), ("scr_dl", "dl"), ("scr_pub", "pub"), ("scr_adv", "adv")):
        s.ptr(o["screens"] + O[field], o[name])
    for name in ("theme", "dl", "pub", "adv"):
        s.ptr(o[name], vt)
    s.set_cfg(case["server"], case["port"])


def execute(s, case):
    """The call."""
    op = case["op"]
    if op == "cb":
        vec = s.alloc(0x20)
        data = s.alloc(12 * 6)
        for k, cmd in enumerate(case["cmds"]):
            s.words(data + 12 * k + 8, [cmd])
        s.words(vec + s.O["vsize"], [case["vsize"]])
        s.ptr(vec + s.O["vdata"], data)
        return s.call("cb", [case["id"], 0, vec])
    if op == "proc":
        return s.call("proc", [])
    return s.call("msg", [s.o["window"], case["m"], case["a"], case["b"]])


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides; pointers as kinds."""
    lg = s.log()
    return {"answer": answer, "log": lg,
            "wanted": s.rd_words(s.g("wanted"), 1), "finished": s.rd_words(s.g("finished"), 1),
            "answers": s.rd_words(s.g("answers"), 1), "values": s.rd_words(s.g("values"), 5), "ids": s.rd_words(s.g("ids"), 5),
            "chat": s.kind(s.rd_ptr(s.g("chat"))), "sys": s.kind(s.rd_ptr(s.g("sysptr"))),
            "sat": s.kind(s.rd_ptr(s.g("satparent")))}


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

SERVERS = [b"chat.example.com", b"", b"a", b"192.168.0.1"]
PORTS = [0, 80, 6667, 0x7FFF, -1, 0xFFFF, -32768]


def queues(c):
    q = {"connect": [c["connect"]], "processing": [c["processing"]], "extra": [c["extra"]], "default": [c["default"]],
         "strval": [c["strval"]], "newnull": c["newnull"], "c_e0": [c["f28"][0]], "c_f4": [c["f28"][1]], "c_f8": [c["f28"][2]],
         "c_fc": [c["f28"][3]], "c_cnt": [c["count"]]}
    for i in range(5):
        q["park:%d" % i] = [c["parks"][i]]
    return q


def world(r, op, **kw):
    """The world every case starts from: random globals; the fakes' answer queues cover every branch."""
    c = {"op": op,
         "wanted": r.choice([0, 1, 1, 1, 2, -1]), "finished": r.choice([0, 0, 1, 5]), "answers": r.choice([0, 1, 3, 4, 7, -2]),
         "values": [r.choice([0, 1, 7, -1, r.randrange(0, 50)]) for _ in range(5)],
         "ids": [r.randrange(-2, 6) for _ in range(5)],
         "chat0": r.random() < 0.5, "sys": r.random() < 0.75, "sat": r.random() < 0.5,
         "f28": [r.choice([1, 1, 0, 0x100, -1]) for _ in range(4)],
         "count": r.choice([-1, 0, 1, 2, 3, 4, 5, 5, 6, 9, 0x7FFFFFFF]),
         "server": r.choice(SERVERS), "port": r.choice(PORTS),
         "connect": r.choice([0, 1, 1, 2, -1]), "processing": r.choice([0, 0, 1, 5]), "extra": r.choice([0, 1, 0x12345678]),
         "default": r.choice([0, 1, -1, r.randrange(-1000, 1000)]),
         "newnull": r.choice([[0], [0], [1, 0], [1]]),
         "parks": [r.randrange(-5, 3000) for _ in range(5)],
         "strval": r.randrange(-3, 7),
         "id": 100, "vsize": 5, "cmds": [0] * 6, "m": 0, "a": 0, "b": 0}
    c.update(kw)
    c["queues"] = queues(c)
    return c


# ---- ChatServerCallbackFn
def cb_case(r, **kw):
    c = world(r, "cb")
    c["id"] = r.choice([100, 100, 100, 99, 101, 0, -3, -4, -5, -6, 7, -100, 0x7FFFFFFF])
    c["cmds"] = [r.choice([7, 7, 0x2C, 0x2C, 0, 8, 0x2B, 0x2D, 0xFFFF]) if k == 0 else r.choice([0, 1, 5, 0x7FFFFFFF, r.randrange(0, 1000)])
                 for k in range(6)]
    c["vsize"] = r.choice([0, 1, 2, 3, 5])
    match = r.randrange(-1, 5)
    c["strval"] = c["ids"][match] if match >= 0 else r.randrange(-3, 7)
    c.update(kw)
    c["queues"] = queues(c)
    return c


def cb_edges():
    r = random.Random(7001)
    for id_ in (100, 99, 101, -3, -4, -5, -6, 0, 7):
        for cmd0 in (7, 0x2C, 0, 8):
            for vsize in (0, 1, 2, 5):
                yield cb_case(r, id=id_, vsize=vsize, answers=r.choice([0, 3, 4]), finished=r.choice([0, 1]),
                              cmds=[cmd0, 11, 12, 13, r.randrange(0, 100), 15])
    for match in range(-1, 5):
        for cmd0 in (7, 0x2C):
            ids = [3, 4, 3, 5, 4]
            yield cb_case(r, id=100, vsize=2, ids=ids, strval=(ids[match] if match >= 0 else 99), cmds=[cmd0, 1, 2, 3, 77, 5],
                          values=[1, 2, 3, 4, 5], answers=0)
    for answers in (-1, 0, 3, 4, 5):
        yield cb_case(r, id=100, answers=answers, finished=0, cmds=[0, 0, 0, 0, 0, 0])


def cb_random(r):
    return cb_case(r)


# ---- ProcessGetChatSatellitePlayers
def proc_case(r, **kw):
    c = world(r, "proc")
    c.update(kw)
    c["queues"] = queues(c)
    return c


def proc_edges():
    r = random.Random(7002)
    n = 0
    for wanted in (0, 1):
        for sys in (False, True):
            for parks_flag in (0, 1):
                for count in (-1, 0, 1, 4, 5, 6, 9):
                    for connect in (0, 1):
                        for finished in (0, 1):
                            f28 = [1, parks_flag, 1, 1]
                            yield proc_case(r, wanted=wanted, sys=sys, f28=f28, count=count, connect=connect, finished=finished,
                                            newnull=[[0], [1, 0], [1]][n % 3])
                            n += 1


def proc_random(r):
    return proc_case(r)


# ---- WorldLobbyControllerMsg
def msg_case(r, **kw):
    c = world(r, "msg")
    c["m"] = r.choice([0x100, 0x100, 0x100, 0x14, 0x10008, 0x10009, 0x1000A, 0x10007, 0xFF, 0x101, 0, -1, 0x7FFFFFFF])
    c["a"] = r.choice([0x16000, 0x16001, 0x16002, 0x16003, 0x16004, 0x16005, 0x16006, 0x16007, 0x15FFF, 0, -1])
    c["b"] = r.choice([0, 1, 2, -1, 0x7FFFFFFF])
    c.update(kw)
    c["queues"] = queues(c)
    return c


def msg_edges():
    r = random.Random(7003)
    for m in (0x100, 0x14, 0x10008, 0x10009, 0x10007, 0x1000A, 0xFF, 0x101, 0x13):
        for a in (0x16000, 0x16001, 0x16002, 0x16003, 0x16004, 0x16005, 0x16006, 0x16007, 0x15FFF):
            for b in (0, 1, 2, -1):
                for extra in (0, 1):
                    yield msg_case(r, m=m, a=a, b=b, extra=extra, f28=[1, 1, 1, 1], processing=0, sys=True)
    for processing in (0, 1):
        for sys in (False, True):
            for mask in range(8):
                f28 = [1 if mask & 1 else 0, 1, 1 if mask & 2 else 0, 1 if mask & 4 else 0]
                yield msg_case(r, m=0x100, a=0x16003, b=0, processing=processing, sys=sys, f28=f28)
    for sat in (False, True):
        yield msg_case(r, m=0x14, a=0, b=0, sat=sat)


def msg_random(r):
    return msg_case(r)


TESTS = [
    FnTest("ChatServerCallbackFn", ppc, host, edge=cb_edges, rnd=cb_random),
    FnTest("ProcessGetChatSatellitePlayers", ppc, host, edge=proc_edges, rnd=proc_random),
    FnTest("WorldLobbyControllerMsg", ppc, host, edge=msg_edges, rnd=msg_random),
]
