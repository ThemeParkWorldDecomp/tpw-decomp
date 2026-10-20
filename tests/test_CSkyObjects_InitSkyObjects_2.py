"""Unit v51029, seven small functions, against libtpw_unit_CSkyObjects_InitSkyObjects_2.dylib (tests/unit_CSkyObjects_InitSkyObjects_2.cpp), main
app ("SimTheme Park"), src/SkyObjects_destroy_CSkyObjects_InitSkyObjects_2.cpp:
  CSkyObjects::InitSkyObjects 0x1015b688, CText::~CText 0x1015ee20, __sinit_RideDB_cpp 0x10157010, CSubscription::Save 0x10152b08,
  __sinit_keyboard_cpp 0x101512fc, InterfaceFontWrapper::GetWidth 0x10168c7c, InterfaceFontWrapper::LastCharIn 0x10168d38

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments and answer from the case's queues: the narrow string constructor, assignment
and appends (a one-buffer string, so the path the code builds is read back by CSky::LoadAdd's fake), the wide string constructor,
get_theme_path, FS_AddDirectorySeparator (appends "/"), CSky::LoadAdd, the font parameter, text manager and string destructors,
__destroy_arr, operator delete, std::ios_base::Init, __register_global_object, LbFile_Write, DEBUG_MESSAGE and the virtual character width
(the virtual call through __ptr_glue on the Mac is a stubbed InterfaceFont::GetCharWidth with a hand-built vtable).

The world of a case: the main balance table (count and six slots per sky object), the theme path, the "Sky" string global, the sky
object block; the CText block (or NULL) and the flag; the ride database prefilled with 0xA5; the two keyboard words; the subscription id
and the file reference; the text of the font calls with the answer queue. What is compared: the answer, the log, and the blocks and globals
read back. The host's sky objects hold four pointers (the case never uses more). The destroy glue is handed the Mac's element size
0x18 and count 0x34 on both sides. Not tested: the contents of the wide string (the Mac text is read from the binary on one side,
the host passes the literal u"gm"); a sky count above four.
No fused operations.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CSkyObjects_InitSkyObjects_2"
MASK = 0xFFFFFFFF

F_SKY, F_TEXT, F_RIDE, F_SAVE, F_KBD, F_GW, F_LC = 0x1015B688, 0x1015EE20, 0x10157010, 0x10152B08, 0x101512FC, 0x10168C7C, 0x10168D38
SIZES = {F_SKY: 328, F_TEXT: 136, F_RIDE: 140, F_SAVE: 124, F_KBD: 120, F_GW: 132, F_LC: 144}
OWN = set(SIZES)

# callees (glue and in-binary)
C_STR_CTOR, C_STR_CTORTEXT, C_STR_DTOR = 0x10238458, 0x102382D8, 0x102382F0
C_WSTR_CTORTEXT = 0x10238800
C_ASSIGN, C_APPEND_BASE, C_APPEND = 0x102391F0, 0x10238380, 0x10238398
C_THEME, C_ADDSEP, C_LOADADD = 0x1015139C, 0x10004BC0, 0x1023B080
C_PARAMS_DTOR, C_DESTROY_ARR, C_MANAGER_DTOR, C_DELETE = 0x1023BA10, 0x10238728, 0x1023BA28, 0x10238218
C_IOS_INIT, C_REGISTER, C_MEMSET = 0x1023B9B0, 0x10239940, 0x10238560
C_WRITE, C_DEBUG = 0x10238878, 0x1006FC90
C_CHARWIDTH = 0x10169264

FILE = 0x7A000000
ADDR = dict(geisky=0x103BC9C8, bal=0x102B4568, sky=0x103B74C8 + 0x4BA0, text=0x1037FF18, fontvec=0x102673C8, wide=0x1037F268,
            node_w=0x1037F27C, node_s=0x1037F25C, quick=0x1037F248, d_wstr=0x103BCBB8, d_str=0x103BCD38, ios=0x1037FE0C,
            node_i=0x1037FE10, node_r=0x1037FE00, db=0x1037F6C8, d_ios=0x103BCFF4, d_ride=0x102672A0, f28c=0x1037F28C, f288=0x1037F288)
MAC_OFF = dict(res=0xC, fonts=0x1C, params=0x4FC)
HL = []          # the host offsets (tpw_sky_layout)


def fb(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def canon_f(bits):
    bits &= MASK
    return 0x7FC00000 if (bits & 0x7F800000) == 0x7F800000 and (bits & 0x7FFFFF) else bits


def s32(v):
    return T.s32(v)


def S(x):
    return x.decode("latin1") if isinstance(x, (bytes, bytearray)) else x


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
W = {}


def cur(e, obj):
    return S(e.cstr(e.u32(obj + 8)))


def setstr(e, obj, text):
    e.write(e.u32(obj + 8), text.encode("latin1") + b"\0")


def r_ctor0(e, log):
    obj = e.r(3)
    e.write(obj + 8, struct.pack(">I", e.alloc(512)))
    setstr(e, obj, "")
    return obj


def r_assign(e, log):
    obj = e.r(3)
    setstr(e, obj, S(e.cstr(e.r(4))))
    return obj


def r_addsep(e, log):
    obj = e.r(3)
    setstr(e, obj, cur(e, obj) + "/")
    return obj


def r_append_base(e, log):
    obj = e.r(3)
    setstr(e, obj, cur(e, obj) + cur(e, e.r(4)))
    return obj


def r_append(e, log):
    obj = e.r(3)
    setstr(e, obj, cur(e, obj) + S(e.cstr(e.r(4))))
    return obj


def fnv(text):
    """FNV-1a of the text (the path goes into the log as a hash and a length; print() the text with judge case if it differs)."""
    h = 0x811C9DC5
    for ch in text.encode("latin1"):
        h = ((h ^ ch) * 0x01000193) & MASK
    return h


def stub_loadadd(e):
    """LoadAdd(sky, void** out, char* path, vec x y z (r6 r7 r8), float size (f1), int a (r10), int b (stack 0x38))."""
    L = e.cl
    sky = 1 if e.r(3) == e.u32(e.r(2) - 0x7140) else L.lab(e.r(3))      # gei_Sky is a data import: the slot holds its address
    L.rec("LoadAdd", (sky, canon_f(e.r(6)), canon_f(e.r(7)), canon_f(e.r(8)), canon_f(fb(e.f(1))), s32(e.r(10)),
                      s32(e.u32(e.r(1) + 0x38)), fnv(S(e.cstr(e.r(5)))), len(S(e.cstr(e.r(5))))))
    e.write(e.r(4), struct.pack(">I", L.ret("LoadOut") & MASK))
    e.set_r(3, L.ret("LoadAdd") & MASK)


def stub_write(e):
    """LbFile_Write(TbError* result, void* file, void* buffer, ulong length, ulong* transferred)."""
    L = e.cl
    data = e.read(e.r(5), 2)
    L.rec("LbFile_Write", (L.lab(e.r(4)), data[0], data[1], e.r(6) & MASK))
    e.write(e.r(7), struct.pack(">I", L.ret("Written") & MASK))


def glue(fn):
    """An import reached through glue: the glue saves the caller's TOC at 0x14(r1), which the code reloads after the call."""
    def wrapped(e):
        e.write(e.r(1) + 0x14, struct.pack(">I", e.r(2)))
        return fn(e)
    return wrapped


STUBS.internal(C_STR_CTOR, glue(CL.recorder("StrCtor", "", ret=r_ctor0)))
STUBS.internal(C_STR_CTORTEXT, glue(CL.recorder("StrCtorText", "Ls")))
STUBS.internal(C_STR_DTOR, glue(CL.recorder("StrDtor", "xi")))
STUBS.internal(C_WSTR_CTORTEXT, glue(CL.recorder("WStrCtorText", "LS")))
STUBS.internal(C_ASSIGN, glue(CL.recorder("StrAssign", "xs", ret=r_assign)))
STUBS.internal(C_APPEND_BASE, glue(CL.recorder("StrAppendBase", "xT", ret=r_append_base)))
STUBS.internal(C_APPEND, glue(CL.recorder("StrAppend", "xs", ret=r_append)))
STUBS.internal(C_THEME, CL.recorder("ThemePath", "", ret=lambda e, log: W["theme"]))
STUBS.internal(C_ADDSEP, CL.recorder("AddSep", "T", ret=r_addsep))
STUBS.internal(C_LOADADD, glue(stub_loadadd))
STUBS.internal(C_PARAMS_DTOR, glue(CL.recorder("ParamsDtor", "Li")))
STUBS.internal(C_DESTROY_ARR, glue(CL.recorder("DestroyArr", "LLii")))
STUBS.internal(C_MANAGER_DTOR, glue(CL.recorder("ManagerDtor", "Li")))
STUBS.internal(C_DELETE, glue(CL.recorder("OperatorDelete", "L")))
STUBS.internal(C_IOS_INIT, glue(CL.recorder("IosInit", "L")))
STUBS.internal(C_REGISTER, glue(CL.recorder("Register", "LLL")))
STUBS.internal(C_WRITE, glue(stub_write))
STUBS.internal(C_DEBUG, CL.recorder("DebugMessage", "su"))
STUBS.internal(C_CHARWIDTH, CL.recorder("CharWidth", "Li", ret="i"))


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def halves(self, a, hs):
        self.write(a, b"".join(struct.pack(self.fmt[0] + "H", h & 0xFFFF) for h in hs))

    def cstr(self, text):
        a = self.alloc(len(text) + 1)
        self.write(a, text.encode("latin1") + b"\0")
        return a

    def text(self, chars, pad=8):
        """UTF-16 text with `pad` nonzero characters before it; answers the address of the first character."""
        a = self.alloc(2 * (len(chars) + pad + 10))
        self.halves(a, [0x41 + i for i in range(pad)] + chars + [0] * 10)
        return a + 2 * pad

    def label_map(self):
        g, o = self.g, self.off()
        return {g("geisky"): 1, g("text") + o["params"]: 2, g("text") + o["fonts"]: 3, g("text") + o["res"]: 4, g("text"): 5,
                g("fontvec"): 6, FILE: 7, g("wide"): 8, g("node_w"): 9, g("node_s"): 10, g("quick"): 11, g("d_wstr"): 12,
                g("d_str"): 13, g("ios"): 14, g("node_i"): 15, g("node_r"): 16, g("db"): 17, g("d_ios"): 18, g("d_ride"): 19}


class Ppc(Side):
    fmt = ">I"
    PS = 4

    def __init__(self, e, case):
        self.e = e
        W.clear()
        self.L = e.cl = CL.PpcLog(self.label_map(), case["queues"])
        self.saved = STUBS.activate(e)

    def off(self):
        return MAC_OFF

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

    def ptrs(self, a, vs):
        self.words(a, vs)

    def rd_ptrs(self, a, n):
        return self.rd_words(a, n)

    def setbal(self, slot, v):
        self.e.write(ADDR["bal"] + 8 + 4 * slot, struct.pack(">I", v & MASK))

    def settheme(self, a):
        W["theme"] = a

    def sky_name(self, a):
        self.e.write(ADDR["sky"] + 8, struct.pack(">I", a))

    def vtable(self, obj):
        """The font object's vptr at +0x10 -> a table whose slot +0x14 holds a TVector of the stubbed character width."""
        tv = self.alloc(8)
        self.words(tv, [C_CHARWIDTH, self.e.r(2)])
        vt = self.alloc(0x40)
        self.words(vt + 0x14, [tv])
        self.words(obj + 0x10, [vt])

    def call(self, op, a):
        if op == "sky":
            T.call_ppc(self.e, F_SKY, [a[0]])
            return 0
        if op == "text":
            return T.call_ppc(self.e, F_TEXT, [a[0], a[1] & MASK])[0] & MASK
        if op == "ride":
            T.call_ppc(self.e, F_RIDE, [])
            return 0
        if op == "kbd":
            T.call_ppc(self.e, F_KBD, [])
            return 0
        if op == "save":
            return s32(T.call_ppc(self.e, F_SAVE, [a[0], a[1]])[0])
        return s32(T.call_ppc(self.e, F_GW if op == "gw" else F_LC, [a[0], a[1], a[2] & MASK, a[3] & MASK])[0])


class Host(Side):
    fmt = "<I"
    PS = 8

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        CL.host_begin(lib, {label: addr for addr, label in self.label_map().items()}, case["queues"])

    def off(self):
        return dict(res=HL[0], fonts=HL[1], params=HL[2])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_sky_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_sky_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def ptrs(self, a, vs):
        self.write(a, b"".join(struct.pack("<Q", v) for v in vs))

    def rd_ptrs(self, a, n):
        return list(struct.unpack("<%dQ" % n, self.read(a, 8 * n)))

    def setbal(self, slot, v):
        self.lib.tpw_sky_setbal(slot, v)

    def settheme(self, a):
        self.lib.tpw_sky_settheme(a)

    def sky_name(self, a):
        self.write(self.g("sky") + HL[11], struct.pack("<Q", a))

    def vtable(self, obj):
        pass

    def call(self, op, a):
        lib = self.lib
        if op == "sky":
            lib.tpw_sky_init(a[0])
            return 0
        if op == "text":
            return lib.tpw_sky_text(a[0], a[1] & MASK)
        if op == "ride":
            lib.tpw_sky_ride()
            return 0
        if op == "kbd":
            lib.tpw_sky_kbd()
            return 0
        if op == "save":
            return lib.tpw_sky_save(a[0], a[1])
        f = lib.tpw_sky_width if op == "gw" else lib.tpw_sky_lastchar
        return f(a[0], a[1], s32(a[2]), s32(a[3]))


def bind(lib):
    if getattr(lib, "bound", False):
        return
    ad = lib.tpw_sky_addr
    ad.restype, ad.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_sky_alloc.restype, lib.tpw_sky_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_sky_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_sky_setbal.argtypes = [C.c_int32, C.c_uint64]
    lib.tpw_sky_settheme.argtypes = [C.c_uint64]
    lib.tpw_sky_init.argtypes = [C.c_uint64]
    lib.tpw_sky_text.restype, lib.tpw_sky_text.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_sky_save.restype, lib.tpw_sky_save.argtypes = C.c_int32, [C.c_uint64, C.c_uint64]
    for n in ("width", "lastchar"):
        f = getattr(lib, "tpw_sky_" + n)
        f.restype, f.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_int32, C.c_int32]
    out = (C.c_int64 * 32)()
    n = lib.tpw_sky_layout(out)
    HL[:] = list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    op = case["op"]
    if op == "sky":
        objs = case["objs"]
        s.setbal(439, case["count"])
        for i, o in enumerate(objs):
            b = 440 + 6 * i
            for k in range(4):
                s.setbal(b + k, o["vec"][k])
            s.setbal(b + 4, o["flag"])
            s.setbal(b + 5, s.cstr(o["name"]))
        s.settheme(s.cstr(case["theme"]))
        s.sky_name(s.cstr(case["skyname"]))
        case["this"] = s.alloc(8 * 4)
        s.ptrs(case["this"], [0x11110000 + i for i in range(4)])
    elif op == "text":
        s.write(s.g("text"), bytes(0x5A8))
        case["this"] = s.g("text") if case["notnull"] else 0
    elif op == "ride":
        db = s.g("db")
        s.write(db, b"\xA5" * (0x738 if s.PS == 4 else HL[10]))
        s.words(s.g("ios"), [0x55555555] * 4)
    elif op == "kbd":
        s.words(s.g("f28c"), [0x55555555])
        s.words(s.g("f288"), [0x66666666])
    elif op == "save":
        case["this"] = s.alloc(8)
        s.halves(case["this"], [case["id"]])
        cell = case["cell"] = s.alloc(8)
        s.ptrs(cell, [FILE])
    else:
        obj = s.g("text")
        s.write(obj, bytes(0x40))
        s.vtable(obj)
        case["this"] = obj
        case["txt"] = s.text(case["chars"])


def execute(s, case):
    op = case["op"]
    if op == "sky":
        return s.call(op, [case["this"]])
    if op == "text":
        return s.call(op, [case["this"], case["flag"]])
    if op in ("ride", "kbd"):
        return s.call(op, [])
    if op == "save":
        return s.call(op, [case["this"], case["cell"]])
    return s.call(op, [case["this"], case["txt"], case["first"], case["last"]])


def observe(s, case, answer):
    op = case["op"]
    r = {"log": s.log()}
    if op == "sky":
        r["objects"] = s.rd_ptrs(case["this"], 4)
    elif op == "text":
        r["same"] = 1 if answer == case["this"] else 0
    elif op == "ride":
        db = s.g("db")
        if s.PS == 4:
            rides, cnt, mid, idp, cp = (0, 0x528), 0x528, (0x52C, 0x730), 0x730, 0x734
        else:
            rides, cnt, mid, idp, cp = (0, HL[4]), HL[5], (HL[6], HL[7]), HL[8], HL[9]
        r["rides_zero"] = s.read(db + rides[0], rides[1]) == bytes(rides[1])
        r["count"] = s.rd_words(db + cnt, 1)
        r["names_intact"] = s.read(db + mid[0], mid[1] - mid[0]) == b"\xA5" * (mid[1] - mid[0])
        r["cached_id"] = struct.unpack(s.fmt[0] + "H", s.read(db + idp, 2))[0]
        r["cached_ride"] = s.rd_ptrs(db + cp, 1)
        r["ios"] = s.rd_words(s.g("ios"), 4)
    elif op == "kbd":
        r["words"] = s.rd_words(s.g("f28c"), 1) + s.rd_words(s.g("f288"), 1)
    else:
        r["answer"] = answer
    if op in ("save",):
        r["answer"] = answer
    return r


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


def rf(r):
    k = r.random()
    if k < 0.1:
        return r.choice([0, 0x80000000, 0x7FC00000, 0x7F800000, 0x3F800000, 0xBF800000])
    return fb(r.uniform(-300, 300))


NAMES = ["a.bmp", "clouds.tga", "x", "", "sun_big.bmp"]
THEMES = ["Data", "C:\\Park", "", "dir/", "Themes:Jungle"]


def sky_case(r, count=None):
    q = {"LoadAdd": [r.choice([0, 1]) for _ in range(4)], "LoadOut": [r.randrange(1, 0xFFFF) for _ in range(4)]}
    objs = [{"vec": [rf(r), rf(r), rf(r), rf(r)], "flag": r.choice([0, 1, r.randrange(2, 99), MASK]), "name": r.choice(NAMES)}
            for _ in range(4)]
    return {"op": "sky", "queues": q, "objs": objs, "count": r.choice([0, 1, 2, 3, 4, -1]) if count is None else count,
            "theme": r.choice(THEMES), "skyname": r.choice(["Sky", "s", ""])}


def sky_edges():
    r = random.Random(11)
    for n in (0, -1, 1, 2, 3, 4):
        yield sky_case(r, n)
    c = sky_case(r, 4)
    c["queues"]["LoadAdd"] = [1]
    yield c
    c = sky_case(r, 4)
    c["queues"]["LoadAdd"] = [0]
    yield c
    c = sky_case(r, 4)
    c["queues"]["LoadAdd"] = [1, 0, 1, 0]
    yield c


def text_case(r, notnull=None, flag=None):
    return {"op": "text", "queues": {}, "notnull": r.choice([True, False]) if notnull is None else notnull,
            "flag": r.choice([0, 1, -1, 0x7FFF, 0x8000, 0x10000, 0x18000, 0xFFFF8000, r.randrange(1 << 32)]) if flag is None else flag}


def text_edges():
    r = random.Random(12)
    for nn in (False, True):
        for fl in (0, 1, -1, 0x7FFF, 0x8000, 0x10000, 0x10001, 0xFFFF0000):
            yield text_case(r, nn, fl & MASK)


def ride_case(r):
    return {"op": "ride", "queues": {}}


def kbd_case(r):
    return {"op": "kbd", "queues": {}}


def save_case(r, w=None, i=None):
    return {"op": "save", "queues": {"Written": [r.choice([2, 2, 0, 1, 3, MASK]) if w is None else w]},
            "id": r.randrange(0x10000) if i is None else i}


def save_edges():
    r = random.Random(14)
    for w in (2, 0, 1, 3, MASK, 0x10002):
        for i in (0, 0x1234, 0xFFFF, 0x00FF, 0xFF00):
            yield save_case(r, w, i)


def widths(r, n):
    return [r.choice([r.randrange(-3, 30), r.randrange(-3, 30), 0x10005, -0x10003, 0x7FFF, 0x8000, 40000, MASK]) & MASK for _ in range(n)]


def font_case(r, op, first=None, last=None, chars=None):
    n = r.randrange(0, 12)
    if chars is None:
        chars = [r.choice([0x41, 0x7F, 0x100, 0xFFFF, 1, 0x61]) for _ in range(n)]
        if r.random() < 0.4 and chars:
            chars[r.randrange(len(chars))] = 0
    n = len(chars)
    f = r.choice([0, 0, 1, 2, -1, -3, n // 2]) if first is None else first
    l = r.choice([n, n + 1, n - 1, 0, 1, 3, -1, 0x7FFF, 0x10000 + 5, 20, 40, 100]) if last is None else last
    return {"op": op, "queues": {"CharWidth": widths(r, 14)}, "chars": chars, "first": f & MASK, "last": l & MASK}


def gw_edges():
    r = random.Random(15)
    for ch, f, l in (([], 0, 0), ([0x41], 0, 1), ([0x41, 0x42, 0x43], 0, 3), ([0x41, 0, 0x43], 0, 3), ([0x41, 0x42, 0x43], 1, 3),
                     ([0x41, 0x42], 2, 1), ([0x41, 0x42], -2, 2), ([0x41, 0x42, 0x43], 0, 2), ([0x41, 0x42], 0, 0x10001)):
        yield font_case(r, "gw", f, l, ch)


def lc_edges():
    r = random.Random(16)
    for ch, f, l in (([], 0, 0), ([0x41], 0, 1), ([0x41, 0x42, 0x43], 0, 10), ([0x41, 0, 0x43], 0, 100), ([0x41, 0x42, 0x43], 1, 5),
                     ([0x41, 0x42], 0, 0), ([0x41, 0x42], 0, -1), ([0x41, 0x42], -2, 4), ([0x41, 0x42, 0x43], 0, 0x18003)):
        yield font_case(r, "lc", f, l, ch)


TESTS = [
    FnTest("CSkyObjects::InitSkyObjects", ppc, host, edge=sky_edges, rnd=sky_case),
    FnTest("CText::~CText", ppc, host, edge=text_edges, rnd=text_case),
    FnTest("__sinit_RideDB_cpp", ppc, host, edge=lambda: iter([ride_case(None)]), rnd=ride_case),
    FnTest("CSubscription::Save", ppc, host, edge=save_edges, rnd=save_case),
    FnTest("__sinit_keyboard_cpp", ppc, host, edge=lambda: iter([kbd_case(None)]), rnd=kbd_case),
    FnTest("InterfaceFontWrapper::GetWidth", ppc, host, edge=gw_edges, rnd=lambda r: font_case(r, "gw")),
    FnTest("InterfaceFontWrapper::LastCharIn", ppc, host, edge=lc_edges, rnd=lambda r: font_case(r, "lc")),
]
