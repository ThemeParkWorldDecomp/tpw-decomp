"""Unit v5531, the tag windows' OverrideTag and TagDlgCallback, against libtpw_unit_OverrideTag.dylib (tests/unit_OverrideTag.cpp), main
app ("SimTheme Park"), src/game/ui/ui_tags.cpp:
  OverrideTag 0x10191d7c, TagDlgCallback 0x10191628

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. Every callee
outside the unit (getExtraParam, GetWindow, GetNext, SetClientDrawText, SetButton, SetDrawObject, FindDrawObject, setZ, startAnim,
stopAnim, CancelAdvisorMessage, PostMessage, SendMessage, RemoveSlideoutPanel, DoButton, CallDefaultWindowHandler, wcsncpy and the two
virtual calls Enable and SetText) is a fake on both sides that records its arguments and answers from the case's queues.

The world of a case: six windows (a Z each, a vtable with Enable and SetText), three destination buffers, a text, the parameter block,
the five window globals. What is compared: the answer, the globals, the three buffers, the event log.

No fused operations. Not tested: GetWindow answering NULL where the original reads the window's Z at once, a NULL destination buffer, a
NULL pending window when the dialog opens (the original calls startAnim on it, the fake never reads it, but that is not a real input).

A last test (unit v5L39), "the tag animations": the host's gTagSlideAnim and gTagShiftAnim (both defined in this file since v5L39) as the
library was loaded against the program image's frames (tests/imagedata.py), one case per frame.
"""
import ctypes as C
import random
import struct

import calllog as CL
import imagedata
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_OverrideTag"
MASK = 0xFFFFFFFF

FN_OT, FN_TD = 0x10191D7C, 0x10191628
SIZES = {FN_OT: 332, FN_TD: 1220}
OWN = set(SIZES)

C_GEP, C_GW, C_GN, C_SCDT, C_SB, C_SDO, C_FDO = 0x101DD030, 0x101DBE40, 0x101DBB38, 0x101DE130, 0x101904F8, 0x101DB474, 0x101CD2D4
C_SZ, C_SA, C_STA, C_CAM, C_PM, C_RSP, C_DB = 0x101DD8BC, 0x101DD08C, 0x101DD1E8, 0x1018227C, 0x101C9400, 0x101A83D8, 0x10190A60
C_SM, C_CDW, C_GLUE = 0x101C9518, 0x101DE318, 0x1023BAA8

ADDR = dict(gTagPanel=0x1038CD78, gTagPanelSource=0x1038CD80, gTagPanelPending=0x1038CD7C, gTagFirstWindow=0x103949D0,
            gAerialWindow=0x103949D4, anim=0x102A83B2, dobj=0x7000A000)
PTRIDX = {"ot": 2, "td": 1}
MODE = {"op": "ot", "s": None}
PER = {}


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def S():
    return MODE["s"]


def node(k):
    return S().nodes[k] if 1 <= k <= 6 else 0


def stub_gep(e):
    L = e.cl
    idx = T.s16(e.r(4) & 0xFFFF)
    L.rec("GEP", (L.lab(e.r(3)), idx))
    v = L.ret("GEP%d" % idx) & MASK
    if idx == PTRIDX[MODE["op"]]:
        v = S().bufs[v] if 1 <= v <= 3 else 0
    e.set_r(3, v)


def stub_gw(e):
    L = e.cl
    wid = s32(e.r(4))
    L.rec("GW", (L.lab(e.r(3)), wid))
    e.set_r(3, node(L.ret("GW%d" % (wid - 0x10D2E))))


def stub_gn(e):
    L = e.cl
    me = L.lab(e.r(3))
    L.rec("GN", (me,))
    e.set_r(3, node(L.ret("GN%d" % me)))


def stub_scdt(e):
    e.cl.rec("SCDT", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def stub_sb(e):
    e.cl.rec("SB", (e.cl.lab(e.r(3)), s32(e.r(4))))


def stub_sdo(e):
    e.cl.rec("SDO", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4))))


def stub_fdo(e):
    p = e.r(3)
    text = e.cstr(p)
    if isinstance(text, str):
        text = text.encode("latin1")
    e.cl.rec("FDO", (sum((i + 1) * c for i, c in enumerate(text)),))
    e.set_r(3, ADDR["dobj"])


def stub_sz(e):
    e.cl.rec("SZ", (e.cl.lab(e.r(3)), T.s16(e.r(4) & 0xFFFF)))


def stub_sa(e):
    e.cl.rec("SA", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), s32(e.r(5))))
    e.set_r(3, 0)


def stub_sta(e):
    e.cl.rec("STA", (e.cl.lab(e.r(3)),))
    e.set_r(3, 0)


def stub_cam(e):
    e.cl.rec("CAM", (s32(e.r(3)),))


def stub_pm(e):
    e.cl.rec("PM", (e.cl.lab(e.r(3)), s32(e.r(4)), e.cl.lab(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def stub_sm(e):
    e.cl.rec("SM", (e.cl.lab(e.r(3)), s32(e.r(4)), e.cl.lab(e.r(5)), s32(e.r(6))))
    e.set_r(3, 0)


def stub_rsp(e):
    e.cl.rec("RSP", (T.s16(e.r(3) & 0xFFFF),))


def stub_db(e):
    e.cl.rec("DB", (s32(e.r(3)), (e.r(4) >> 16) & 0xFFFF, s32(e.r(5))))
    e.set_r(3, e.cl.ret("DB") & MASK)


def stub_cdw(e):
    L = e.cl
    L.rec("CDW", (L.lab(e.r(3)), s32(e.r(4)), s32(e.r(5)), s32(e.r(6))))
    e.set_r(3, L.ret("CD") & MASK)


def stub_glue(e):
    L = e.cl
    f = e.r(12)
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))      # the real glue saves the TOC pointer where the caller reloads it
    if f == 0xE1:
        L.rec("Enable", (L.lab(e.r(3)), T.s16(e.r(4) & 0xFFFF)))
    elif f == 0xE2:
        L.rec("SetText", (L.lab(e.r(3)), L.lab(e.r(4))))
    else:
        L.rec("BADGLUE", (f,))
    e.set_r(3, 0)


def stub_wcsncpy(e):
    L = e.cl
    dst, src, n = e.r(3), e.r(4), e.r(5)
    L.rec("wcsncpy", (L.lab(dst), L.lab(src), n))
    out = []
    for i in range(n):
        ch = struct.unpack(">H", e.read(src + 2 * i, 2))[0]
        if ch == 0:
            break
        out.append(ch)
    out += [0] * (n - len(out))
    e.write(dst, struct.pack(">%dH" % n, *out))
    e.set_r(3, dst)


for _a, _f in ((C_GEP, stub_gep), (C_GW, stub_gw), (C_GN, stub_gn), (C_SCDT, stub_scdt), (C_SB, stub_sb), (C_SDO, stub_sdo),
               (C_FDO, stub_fdo), (C_SZ, stub_sz), (C_SA, stub_sa), (C_STA, stub_sta), (C_CAM, stub_cam), (C_PM, stub_pm),
               (C_RSP, stub_rsp), (C_DB, stub_db), (C_SM, stub_sm), (C_CDW, stub_cdw), (C_GLUE, stub_glue)):
    STUBS.internal(_a, _f)
STUBS.imp("wcsncpy", stub_wcsncpy)


def setup(runner):
    T.install_std_stubs(runner.emu)
    e = runner.emu
    PER["nodes"] = [0] + [T.persist_alloc(e, 0x140) for _ in range(6)]
    PER["bufs"] = [0] + [T.persist_alloc(e, 0x200) for _ in range(3)]
    PER["txt"] = T.persist_alloc(e, 0x200)
    PER["params"] = T.persist_alloc(e, 0x40)
    PER["vt"] = T.persist_alloc(e, 0x40)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def h16(self, a, vals):
        self.write(a, struct.pack(self.fmt[0] + "%dH" % len(vals), *[v & 0xFFFF for v in vals]))

    def rd_h16(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dH" % n, self.read(a, 2 * n)))

    def ptr_ident(self, p):
        if p == 0:
            return 0
        for k in range(1, 7):
            if p == self.nodes[k]:
                return ("n", k)
        return ("x", p)


class Ppc(Side):
    fmt = ">I"
    P = dict(mF4=4, mText=8, mF1C=0x1C, mF20=0x20)
    ZOFF = 0xDC

    def __init__(self, e, case):
        self.e = e
        self.nodes, self.bufs, self.txt, self.params = PER["nodes"], PER["bufs"], PER["txt"], PER["params"]
        fixed = {ADDR["anim"]: 80, ADDR["dobj"]: 90, self.txt: 20}
        for k in range(1, 7):
            fixed[self.nodes[k]] = k
        for k in range(1, 4):
            fixed[self.bufs[k]] = 10 + k
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        MODE["s"] = self
        MODE["op"] = case["op"]
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def g(self, name):
        return ADDR[name]

    def putp(self, a, v):
        self.words(a, [v])

    def rdp(self, a):
        return self.rd_words(a, 1)[0]

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def vtable(self):
        self.words(PER["vt"] + 0x1C, [0xE1])
        self.words(PER["vt"] + 0x2C, [0xE2])
        for k in range(1, 7):
            self.words(self.nodes[k] + 0x118, [PER["vt"]])

    def call_ot(self):
        r3, _, _ = T.call_ppc(self.e, FN_OT, [self.params])
        return r3 & 0xFF

    def call_td(self, w, m, a, b):
        r3, _, _ = T.call_ppc(self.e, FN_TD, [w, m & MASK, a & MASK, b & MASK])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_ot_world()
        o = lib.tpw_ot_obj
        self.nodes = [0] + [o(0, k) for k in range(1, 7)]
        self.bufs = [0] + [o(1, k) for k in range(1, 4)]
        self.txt, self.params = o(2, 0), o(3, 0)
        fixed = {80: self.g("anim"), 90: self.g("dobj"), 20: self.txt}
        for k in range(1, 7):
            fixed[k] = self.nodes[k]
        for k in range(1, 4):
            fixed[10 + k] = self.bufs[k]
        CL.host_begin(lib, fixed, case["queues"])
        self.P = dict(zip(("mF4", "mText", "mF1C", "mF20"), HL[:4]))
        self.ZOFF = HL[5]

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ot_addr(name.encode())

    def putp(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def rdp(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def vtable(self):
        pass

    def call_ot(self):
        return self.lib.tpw_ot_override() & 0xFF

    def call_td(self, w, m, a, b):
        return self.lib.tpw_ot_dlg(w, s32(m), s32(a), s32(b)) & MASK


HL = []


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ot_addr.restype, lib.tpw_ot_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ot_obj.restype, lib.tpw_ot_obj.argtypes = C.c_uint64, [C.c_int32, C.c_int32]
    lib.tpw_ot_world.restype, lib.tpw_ot_world.argtypes = None, []
    lib.tpw_ot_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ot_override.restype, lib.tpw_ot_override.argtypes = C.c_uint32, []
    lib.tpw_ot_dlg.restype, lib.tpw_ot_dlg.argtypes = C.c_uint32, [C.c_uint64, C.c_int32, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ot_layout(out)
    HL[:] = list(out)[:n]
    assert HL[0] == 4 and HL[2] == 32 and HL[3] == 36, HL
    lib.bound = True


def build(s, case):
    s.vtable()
    for k in range(1, 7):
        s.h16(s.nodes[k] + s.ZOFF, [case["z"][k - 1]])
    for k in range(1, 4):
        s.h16(s.bufs[k], [0x1111 * k + i for i in range(0x100)])
    s.h16(s.txt, case["text"])
    for name, key in (("gTagPanel", "panel"), ("gTagPanelSource", "src"), ("gTagPanelPending", "pend"),
                      ("gTagFirstWindow", "first"), ("gAerialWindow", "aerial")):
        s.putp(s.g(name), s.nodes[case[key]] if case[key] else 0)
    if case["op"] == "ot":
        p = s.params
        s.words(p, [case["p"]["id"]])
        s.words(p + s.P["mF4"], [case["p"]["f4"]])
        s.putp(p + s.P["mText"], s.txt)
        s.h16(p + s.P["mF1C"], [case["p"]["f1c"]])
        s.words(p + s.P["mF20"], [case["p"]["f20"]])


def execute(s, case):
    if case["op"] == "ot":
        return s.call_ot()
    return s.call_td(s.nodes[case["self"]], *case["args"])


def observe(s, case, answer):
    lg = s.log()
    return {"answer": answer, "log": lg,
            "src": s.ptr_ident(s.rdp(s.g("gTagPanelSource"))), "pend": s.ptr_ident(s.rdp(s.g("gTagPanelPending"))),
            "panel": s.ptr_ident(s.rdp(s.g("gTagPanel"))), "bufs": [s.rd_h16(s.bufs[k], 0x100) for k in range(1, 4)]}


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
    z = [r.choice([0, 1, -1, 32767, 0x100, r.randrange(-300, 300)]) for _ in range(6)]
    text = [r.randrange(1, 0x10000) for _ in range(r.choice([0, 1, 5, 0xfe, 0xff, 0x100, 0x100]))]
    text = (text + [0] * 0x100)[:0x100] if len(text) < 0x100 else text[:0x100]
    if r.random() < 0.3:
        text[r.randrange(0, 0x100)] = 0
    c = {"op": op, "z": z, "text": text, "queues": {}, "panel": r.randrange(1, 7), "src": r.choice([0, r.randrange(1, 7)]),
         "pend": r.choice([0, r.randrange(1, 7)]), "first": 0, "aerial": r.choice([0, r.randrange(1, 7)])}
    return c


def ot_case(r, mode=None):
    c = base(r, "ot")
    n = r.choice([0, 1, 1, 2, 3, 5])
    chain = r.sample(range(1, 7), n)
    c["first"] = chain[0] if chain else 0
    q = c["queues"]
    for i, k in enumerate(chain):
        q["GN%d" % k] = [chain[i + 1] if i + 1 < n else 0]
    pid = r.choice([0, 1, 0x17F, 0xFFFFFFFF, r.randrange(0, 1 << 32)])
    f4 = pid if r.random() < 0.5 else r.choice([0x17F, 5, 0xFFFFFFFF, r.randrange(0, 1 << 32)])
    if mode == "same":
        f4 = pid
    if mode == "diff" and f4 == pid:
        f4 ^= 1
    f1c, f20 = r.randrange(0, 0x10000), r.choice([0, 0xFFFFFFFF, r.randrange(0, 1 << 32)])
    c["p"] = {"id": pid, "f4": f4, "f1c": f1c, "f20": f20}
    n1 = max(n, 1)

    def pick(match, wrong):
        return [match if r.random() < 0.7 else wrong() for _ in range(n1)]
    q["GEP1"] = pick(f4, lambda: r.choice([f4 ^ 1, r.randrange(0, 1 << 32)]))
    q["GEP3"] = pick(f1c | (r.randrange(0, 0x10000) << 16), lambda: r.randrange(0, 1 << 32) | 0)
    q["GEP3"] = [v if r.random() < 0.5 or (v & 0xFFFF) != f1c else v for v in q["GEP3"]]
    q["GEP8"] = pick(f20, lambda: r.choice([f20 ^ 0x100, r.randrange(0, 1 << 32)]))
    q["GEP2"] = [r.randrange(1, 4) for _ in range(n1)]
    q["GW4"] = [r.randrange(1, 7)]
    return c


def ot_edges():
    r = random.Random(11)
    for mode in ("same", "diff", None, None):
        for _ in range(12):
            yield ot_case(r, mode)
    c = ot_case(r, "same")
    c["first"], c["queues"] = 0, {}
    yield c


MSGS = [0x100, 0x1D, 0x1001D9, 0x1001D8, 0x11, 0x1001DA, 0x1001D7, 0xFF, 0x101, 0, 0x1001D9, 0x100, 0x100]
AS = [0x10D2E, 0x10D2F, 0x10D30, 0x10D31, 0x10D2D, 1, 0, 2, 0x10D32, -1]


def td_case(r, msg=None, a=None, db=None, src=None):
    c = base(r, "td")
    c["self"] = r.randrange(1, 7)
    msg = r.choice(MSGS) if msg is None else msg
    a = r.choice(AS) if a is None else a
    b = r.choice([0, 1, -1, r.randrange(-1000, 1000)])
    c["args"] = [msg, a, b]
    if msg == 0x1001D9:
        c["pend"] = r.randrange(1, 7)
    if src is not None:
        c["src"] = src
    q = c["queues"]
    q["GW0"], q["GW1"], q["GW2"] = [r.randrange(1, 7)], [r.randrange(1, 7)], [r.randrange(1, 7)]
    q["GW4"] = [r.choice([0, 0, r.randrange(1, 7)])]
    q["GEP1"] = [r.randrange(0, 4)]
    q["GEP2"] = [r.choice([0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x12345, r.randrange(0, 1 << 32)])]
    q["GEP3"] = [r.choice([0, 3, 0x1B, 27, r.randrange(0, 40), r.randrange(0, 1 << 32)])]
    q["GEP4"] = [r.choice([0x1B, 0x1B, 0, 5, r.randrange(0, 40), r.randrange(0, 1 << 32)])]
    q["GEP5"] = [r.choice([0, 7, 0xFFFFFFFF, r.randrange(0, 1 << 32)])]
    q["DB"] = [r.choice([-1, 0, 1, 2, 3, 100]) if db is None else db]
    q["CD"] = [r.choice([0, 1, -1, r.randrange(-1 << 31, 1 << 31)]) & MASK]
    return c


def td_edges():
    r = random.Random(12)
    for a in (0x10D2F, 0x10D30):
        for db in (-1, 0, 1, 2, 3):
            for src in (0, 2):
                yield td_case(r, 0x100, a, db, src)
    for m in MSGS + [0x1001D8, 0x1001D9]:
        for a in AS:
            yield td_case(r, m, a)
    for src in (0, 3):
        yield td_case(r, 0x100, 0x10D2E, None, src)
    yield td_case(r, 0x1D, 1)
    yield td_case(r, 0x1D, 0)
    yield td_case(r, 0x1001D8, 0, None, 0)
    yield td_case(r, 0x1001D8, 0, None, 4)


TESTS = [
    FnTest("OverrideTag", ppc, host, edge=ot_edges, rnd=lambda r: ot_case(r)),
    FnTest("TagDlgCallback", ppc, host, edge=td_edges, rnd=lambda r: td_case(r)),
    imagedata.table_test("the tag animations", [("slide", ADDR["anim"], "hhhh", 7), ("shift", 0x102A83EA, "hhhh", 15)]),
]
