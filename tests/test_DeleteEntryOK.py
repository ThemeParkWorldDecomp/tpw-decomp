"""Unit v51043: the postcard address delete button, the postcard and research list row makers, ResearchPanelCategoryComplete and
InterfaceTimedMessages::Add, against libtpw_unit_DeleteEntryOK.dylib (tests/unit_DeleteEntryOK.cpp), main app
("SimTheme Park"), src/ui_Postcard_panel_CreateAddressBook_DeleteEntryOK.cpp:
  DeleteEntryOK 0x101b0368, LCFormat 0x101b0620, LCPostFormat 0x101b06d4, ResearchPanelCategoryComplete 0x101b5b4c,
  LCFormat 0x101b9ea8, InterfaceTimedMessages::Add 0x101c82ac

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation. The callees outside the unit (operator new, the
constructors, AddressBook::Remove, FillAddressList, the font table and the virtual called through __ptr_glue, the window calls, the research
lab, NameWindow, PercentComplete, IveGotTicks) are fakes on both sides that record their arguments and answer from the case's queues.

The world of a case: the globals (gUserInfo, gGameWorld, gResearchPanel, the screen height, the research row height and font index), the
timed messages object with its entries (laid out as the Mac's on the PowerPC side, as the header's on the host). What is compared: the answer
(objects as labels), the globals and the entries read back, the event log.

No fused operations. Not tested: a zero screen height (the original divides by it unchecked: undefined on the host); a NULL from operator new
(it never returns one).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_DeleteEntryOK"
MASK = 0xFFFFFFFF

F_DEL, F_LC1, F_LCPOST, F_RES, F_LC2, F_ADD = 0x101b0368, 0x101b0620, 0x101b06d4, 0x101b5b4c, 0x101b9ea8, 0x101c82ac
SIZES = {F_DEL: 144, F_LC1: 148, F_LCPOST: 156, F_RES: 136, F_LC2: 240, F_ADD: 224}
OWN = set(SIZES)

NEW, USERINFO_CTOR, REMOVE, FILLADDR = 0x102384b8, 0x1017de54, 0x10206728, 0x101aff60
GETFONT, PTRGLUE, WINCTOR, SPRITECTOR = 0x10180270, 0x1023baa8, 0x101d98d8, 0x1018d11c
SETCLIENT, SETFONT, SETCOLOUR, CREATEOBJ = 0x101db624, 0x101da118, 0x101da3ac, 0x101dc750
GETLAB, ISACTIVE, NEXTPROJ, NAMEWIN, PERCENT, TICKS = 0x1013ece8, 0x10123768, 0x101223e8, 0x101b3da8, 0x101b3398, 0x101cac3c

ADDR = dict(gUserInfo=0x10380b34, gGameWorld=0x1037ebfc, gResearchPanel=0x10395980, gUIRealScreenHeight=0x10395bba,
            gParkListFontIndex=0x102ad5dc, gResearchRowHeight=0x103959a0)
WIN = 0x30000000          # a window the case passes; never dereferenced (every window call is a fake)
LABOBJ = 0x1000           # what GetResearchLab answers
FONT = [0]                # the persistent font block on the PowerPC side (set in setup)
KNOWN_HOST = None


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)

def save_toc(e):
    """The call glue stores the caller's TOC pointer at 0x14(r1), where the code reloads it from after the call."""
    e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2) & MASK))


_new = CL.recorder("new", "u", ret=lambda e, log: e.alloc(max(e.r(3), 4)))


def stub_new(e):
    save_toc(e)
    _new(e)


STUBS.internal(NEW, stub_new)
STUBS.internal(USERINFO_CTOR, CL.recorder("CFSUserInfo", "N"))
STUBS.internal(FILLADDR, CL.recorder("FillAddressList", "i"))
STUBS.internal(WINCTOR, CL.recorder("WindowCtor", "NLiihhhh"))
STUBS.internal(CREATEOBJ, CL.recorder("CreateObject", "Liiihhhh"))
STUBS.internal(SETCLIENT, CL.recorder("SetClientDrawObject", "LL"))
STUBS.internal(SETFONT, CL.recorder("SetFont", "LL"))
STUBS.internal(SETCOLOUR, CL.recorder("SetFontColour", "Liiii"))
STUBS.internal(SPRITECTOR, CL.recorder("SpriteCtor", "Nu"))
STUBS.internal(GETLAB, CL.recorder("GetResearchLab", "u", ret=lambda e, log: LABOBJ))
STUBS.internal(ISACTIVE, CL.recorder("IsCategoryActive", "ui", ret="b"))
STUBS.internal(NEXTPROJ, CL.recorder("NextProject", "ui"))
STUBS.internal(NAMEWIN, CL.recorder("NameWindow", "i"))
STUBS.internal(PERCENT, CL.recorder("PercentComplete", "i"))
STUBS.internal(TICKS, CL.recorder("IveGotTicks", "", ret="i"))


def stub_remove(e):
    base = e.u32(ADDR["gUserInfo"])
    e.cl.rec("Remove", (s32(e.r(3) - base), s32(e.r(4))))
    e.set_r(3, 0)


def stub_getfont(e):
    e.cl.rec("GetFontTableEntry", (e.r(3) & MASK,))
    e.set_r(3, FONT[0])


def stub_glue(e):
    L = e.cl
    save_toc(e)
    L.rec("FontVirt", (L.lab(e.r(3)),))
    e.set_r(3, L.ret("FontVirt") & MASK)


STUBS.internal(REMOVE, stub_remove)
STUBS.internal(GETFONT, stub_getfont)
STUBS.internal(PTRGLUE, stub_glue)


def setup(runner):
    e = runner.emu
    FONT[0] = T.persist_alloc(e, 0x40)
    e.write(FONT[0] + 0x10, struct.pack(">I", FONT[0]))    # the table pointer at +0x10 points back into readable memory


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def set_h(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + "h", T.s16(v)))

    def set_ptr(self, a, v):
        self.write(a, struct.pack(self.fmt[0] + self.pfmt, v))

    def rd_ptr(self, a):
        return struct.unpack(self.fmt[0] + self.pfmt, self.read(a, self.psize))[0]

    def make_tm(self, tm):
        """The timed messages object and its entries; tm = (count, entries or None); entries are (window, msg, a, b, time)."""
        lay = self.TM
        obj = self.alloc(lay["size"])
        count, entries = tm
        self.write(obj + lay["count"], struct.pack(self.fmt, count & MASK))
        if entries is not None:
            block = self.alloc(max(1, len(entries)) * lay["stride"])
            for i, (w, m, a, b, t) in enumerate(entries):
                base = block + i * lay["stride"]
                self.set_ptr(base + lay["w"], w)
                self.write(base + lay["m"], struct.pack(self.fmt, m & MASK))
                self.write(base + lay["a"], struct.pack(self.fmt, a & MASK))
                self.write(base + lay["b"], struct.pack(self.fmt, b & MASK))
                self.write(base + lay["t"], struct.pack(self.fmt, t & MASK))
            self.set_ptr(obj + lay["ents"], block)
        return obj

    def read_tm(self, obj, n):
        lay = self.TM
        out = [self.rd_word(obj + lay["count"]), self.rd_ptr(obj + lay["ents"]) != 0]
        block = self.rd_ptr(obj + lay["ents"])
        for i in range(n if block else 0):
            base = block + i * lay["stride"]
            out.append((self.rd_ptr(base + lay["w"]) & MASK, self.rd_word(base + lay["m"]), self.rd_word(base + lay["a"]),
                        self.rd_word(base + lay["b"]), self.rd_word(base + lay["t"])))
        return out


class Ppc(Side):
    fmt = ">I"
    pfmt = "I"
    psize = 4
    TM = dict(size=8, count=0, ents=4, stride=0x14, w=0, m=4, a=8, b=0xc, t=0x10)

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({WIN: 7, FONT[0]: 9}, case["queues"])
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

    def lab(self, v):
        return self.L.lab(v & MASK)

    def call(self, entry, args):
        return T.call_ppc(self.e, entry, [a & MASK for a in args])[0] & MASK

    def call_add(self, obj, args):
        return T.s16(self.call(F_ADD, [obj] + args))


class Host(Side):
    fmt = "<I"
    pfmt = "Q"
    psize = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.TM = lib.tm
        CL.host_begin(lib, {7: WIN, 9: self.g("font")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_dwe_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_dwe_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def lab(self, v):
        return self.lib.tpw_dwe_lab(v)

    def call(self, entry, args):
        lib = self.lib
        if entry == F_DEL:
            return lib.tpw_dwe_delete(args[0] & MASK)
        if entry == F_LC1:
            return lib.tpw_dwe_lcformat1()
        if entry == F_LCPOST:
            lib.tpw_dwe_lcpost(args[0] & MASK)
            return 0
        if entry == F_RES:
            lib.tpw_dwe_research(s32(args[0]))
            return 0
        return lib.tpw_dwe_lcformat2()

    def call_add(self, obj, args):
        w, m, a, b, t = args
        return T.s16(self.lib.tpw_dwe_add(obj, w & MASK, s32(m), s32(a), s32(b), t & MASK))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    c = C
    lib.tpw_dwe_addr.restype, lib.tpw_dwe_addr.argtypes = c.c_uint64, [c.c_char_p]
    lib.tpw_dwe_alloc.restype, lib.tpw_dwe_alloc.argtypes = c.c_uint64, [c.c_uint32]
    lib.tpw_dwe_lab.restype, lib.tpw_dwe_lab.argtypes = c.c_int64, [c.c_uint64]
    lib.tpw_dwe_layout.argtypes = [c.POINTER(c.c_int64)]
    lib.tpw_dwe_delete.restype, lib.tpw_dwe_delete.argtypes = c.c_uint32, [c.c_uint32]
    lib.tpw_dwe_lcformat1.restype, lib.tpw_dwe_lcformat1.argtypes = c.c_uint64, []
    lib.tpw_dwe_lcpost.restype, lib.tpw_dwe_lcpost.argtypes = None, [c.c_uint32]
    lib.tpw_dwe_research.restype, lib.tpw_dwe_research.argtypes = None, [c.c_int32]
    lib.tpw_dwe_lcformat2.restype, lib.tpw_dwe_lcformat2.argtypes = c.c_uint64, []
    lib.tpw_dwe_add.restype = c.c_int32
    lib.tpw_dwe_add.argtypes = [c.c_uint64, c.c_uint32, c.c_int32, c.c_int32, c.c_int32, c.c_uint32]
    out = (c.c_int64 * 32)()
    n = lib.tpw_dwe_layout(out)
    v = list(out)[:n]
    lib.tm = dict(size=v[0], count=v[1], ents=v[2], stride=v[3], w=v[4], m=v[5], a=v[6], b=v[7], t=v[8])
    lib.bound = True


def build(s, case):
    s.set_ptr(s.g("gUserInfo"), case["ui"])
    s.set_ptr(s.g("gGameWorld"), case["world"])
    s.set_ptr(s.g("gResearchPanel"), case["panel"])
    s.set_h(s.g("gUIRealScreenHeight"), case["screen"])
    s.words(s.g("gResearchRowHeight"), [case["rowh"]])
    s.words(s.g("gParkListFontIndex"), [case["fontidx"]])
    return s.make_tm(case["tm"]) if case["tm"] is not None else 0


def execute(s, case, obj):
    op = case["op"]
    if op == "del":
        return s.call(F_DEL, case["args"])
    if op == "lc1":
        return s.lab(s.call(F_LC1, []))
    if op == "lcpost":
        s.call(F_LCPOST, [WIN])
        return 0
    if op == "res":
        s.call(F_RES, case["args"])
        return 0
    if op == "lc2":
        return s.lab(s.call(F_LC2, []))
    return s.call_add(obj, case["args"])


def observe(s, case, obj, answer):
    ui = s.rd_ptr(s.g("gUserInfo"))
    ui_obs = (s.lab(ui) if ui else 0, int(ui == case["ui"]))     # before the log: host_end forgets the labels
    out = {"answer": answer, "log": s.log(), "ui": ui_obs,
           "rowh": s.rd_word(s.g("gResearchRowHeight")), "world": s.rd_ptr(s.g("gGameWorld")) & MASK,
           "panel": s.rd_ptr(s.g("gResearchPanel")) & MASK}
    if case["tm"] is not None:
        out["tm"] = s.read_tm(obj, len(case["tm"][1]) if case["tm"][1] is not None else 0)
    return out


def run(s, case):
    try:
        obj = build(s, case)
        return observe(s, case, obj, execute(s, case, obj))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


HEIGHTS = [0, 1, 16, 20, -1, -5, 32767, -32768, 0x7fff, 0xffff, 0x10000 + 12]
SCREENS = [1, -1, 100, 768, 1536, 2048, 3, -7, 32767, -32768]
TICKS_V = [0, 1, 5, 59999, 0xFFFFFFF0, 0xFFFFFFFF, 0x80000000]


def base(r, op):
    return {"op": op, "args": [], "tm": None,
            "queues": {"FontVirt": [r.choice(HEIGHTS + [r.randrange(-40000, 40000)])],
                       "IveGotTicks": [r.choice(TICKS_V + [r.getrandbits(32)])],
                       "IsCategoryActive": [r.choice([0, 1])]},
            "ui": r.choice([0, 0x40000000, 0x40000000 + r.randrange(0, 0x1000)]),
            "world": r.choice([0, 0x12345678, r.getrandbits(32)]), "panel": r.choice([0, 0x22222222, r.getrandbits(32)]),
            "screen": r.choice(SCREENS + [r.randrange(1, 5000), -r.randrange(1, 5000)]),
            "rowh": r.choice([0, 0, 0x38, 1, -1, r.randrange(-100, 100000)]),
            "fontidx": r.choice([0, 7, 12, r.getrandbits(32)])}


POSITIONS = [0, 1, 2, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000, 100]


def del_case(r, ui=None, pos=None):
    c = base(r, "del")
    if ui is not None:
        c["ui"] = ui
    c["args"] = [r.choice(POSITIONS + [r.getrandbits(32)]) if pos is None else pos]
    return c


def del_edges():
    r = random.Random(101)
    for ui in (0, 0x40000000):
        for pos in (0, 1, 0xFFFFFFFF, 0x80000000):
            yield del_case(r, ui, pos)


def lc1_case(r):
    return base(r, "lc1")


def lc1_edges():
    r = random.Random(102)
    for h in HEIGHTS:
        for sc in (1, -1, 768, -768):
            c = lc1_case(r)
            c["queues"]["FontVirt"] = [h]
            c["screen"] = sc
            yield c


def lcpost_case(r):
    return base(r, "lcpost")


def lcpost_edges():
    yield lcpost_case(random.Random(103))


def res_case(r, cat=None, active=None, panel=None):
    c = base(r, "res")
    c["args"] = [r.choice([0, 1, 2, 3, 5, 0xFFFFFFFF, r.getrandbits(32)]) if cat is None else cat]
    if active is not None:
        c["queues"]["IsCategoryActive"] = [active]
    if panel is not None:
        c["panel"] = panel
    return c


def res_edges():
    r = random.Random(104)
    for active in (0, 1, 0x100, 0x1ff):
        for panel in (0, 1):
            for cat in (0, 3, 0xFFFFFFFF):
                yield res_case(r, cat, active, panel)


def lc2_case(r):
    return base(r, "lc2")


def lc2_edges():
    r = random.Random(105)
    for rowh in (0, 0x38, -1, 1):
        for h in (0, 20, -5, 32767):
            c = lc2_case(r)
            c["rowh"] = rowh
            c["queues"]["FontVirt"] = [h]
            c["screen"] = 768
            yield c


def add_case(r, count=None, ents=None, time=None, mk=None):
    c = base(r, "add")
    n = r.randrange(0, 7)
    if ents is None:
        ents = None if r.random() < 0.1 else [
            (r.choice([0, 0x30000000, r.getrandbits(32)]), r.getrandbits(32), r.getrandbits(32), r.getrandbits(32),
             r.choice([0, 0, 1, 59999, 60000, r.getrandbits(32)])) for _ in range(n)]
    if count is None:
        count = len(ents) if ents is not None else r.choice([0, 1, 3, -1])
        if ents is not None and r.random() < 0.2:
            count = r.choice([-1, 0, max(0, len(ents) - 1), -5])
    c["tm"] = (count, ents)
    t = r.choice([0, 1, 59999, 60000, 60001, 0xFFFFFFFF, 0x80000000, r.randrange(1, 60000), r.getrandbits(32)]) if time is None else time
    c["args"] = [r.choice([0, WIN, r.getrandbits(32)]), r.getrandbits(32), r.getrandbits(32), r.getrandbits(32), t]
    return c


def add_edges():
    r = random.Random(106)
    free = (0x31, 1, 2, 3, 0)
    used = (0x32, 4, 5, 6, 10)
    for t in (0, 1, 59999, 60000, 60001, 0xFFFFFFFF):
        yield add_case(r, 2, [used, free], t)
        yield add_case(r, 2, [free, used], t)
    yield add_case(r, 3, [used, used, used], 5)
    yield add_case(r, 0, [free], 5)
    yield add_case(r, -1, [free], 5)
    yield add_case(r, 2, [free, free, free], 5)        # only the first two are in the count
    yield add_case(r, 5, None, 5)                       # no entries array
    yield add_case(r, 1, [free], 5)
    yield add_case(r, 2, [used, used], 5)


TESTS = [
    FnTest("DeleteEntryOK", ppc, host, edge=del_edges, rnd=del_case),
    FnTest("LCFormat@0x101b0620", ppc, host, edge=lc1_edges, rnd=lc1_case),
    FnTest("LCPostFormat", ppc, host, edge=lcpost_edges, rnd=lcpost_case),
    FnTest("ResearchPanelCategoryComplete", ppc, host, edge=res_edges, rnd=res_case),
    FnTest("LCFormat@0x101b9ea8", ppc, host, edge=lc2_edges, rnd=lc2_case),
    FnTest("InterfaceTimedMessages::Add", ppc, host, edge=add_edges, rnd=add_case),
]
