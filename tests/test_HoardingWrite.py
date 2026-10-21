"""Unit w208, the park's hoarding written to the theme's hoardings balance file and the static objects of LLCellSystem.cpp, against
libtpw_unit_HoardingWrite.dylib (tests/unit_HoardingWrite.cpp), main app ("SimTheme Park"), src/game/park/LLCellSystem.cpp:
  LLCC_WriteHoardingsLine 0x1009588C, LLCC_WriteHoardingsToBalanceFile 0x10095B20, LLCC_WriteCompleteHoardingsToBalanceFile 0x10095E54,
  __sinit_LLCellSystem_cpp 0x10096F04

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit are fakes on both sides that record their arguments and answer from the case: the bullfrog string members (functional: they keep
the text), get_theme_path, FS_AddDirectorySeparator (appends ":"), CTRACE, the click stack builder (it leaves the case's clicks and count
and records the byte gLLCC4818 holds), the file's Open (a hidden TbError answered from the queue) and Close, _itoa (the decimal digits) and
the access object behind the file: its Write (vtable slot +0x10) records the size and the bytes and answers from the queue.

The world of a writer case: the five strings the line writer puts together (the key prefix, ].X, ].Y, the line end and the separator), the two
file names of gTPGlobals, the theme path, the file's access object (or none), the Open answer, the hoarding extents, the click stack builder's
clicks and count word, and junk in the click stack, the copy and the copy's index, count and temporary click. The line writer's case is the
key string, the index and the value. The sinit case is junk over the statics it builds. What is compared: the answer, the event log (every write
the file receives), the byte gLLCC4818, the stack and its count, the copy, its index and count and the temporary click; the sinit's texts, the
nine offsets, the two words and the objects it registers for destruction (by position and by destructor).

No fused operations. Not tested: a click count above 0x400 (the original runs on over the words that follow the arrays); the writers' returns
of 0 after a failed row (the line writer always answers 1, so those branches are dead); the file's constructor and destructor bodies (faked).
"""
import ctypes as C
import random
import string
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_HoardingWrite"
MASK = 0xFFFFFFFF
INT_MIN = -0x80000000
INT_MAX = 0x7FFFFFFF

# ---- the unit's own functions (address, size from the MANIFEST); OWN runs as the original even where other modules left a stub
FN_LINE, FN_WRITE, FN_COMPLETE, FN_SINIT = 0x1009588C, 0x10095B20, 0x10095E54, 0x10096F04
SIZES = {FN_LINE: 580, FN_WRITE: 764, FN_COMPLETE: 764, FN_SINIT: 764}
OWN = set(SIZES)

# ---- the callees faked on both sides (the asm's bl targets that are not OWN)
GET_THEME_PATH = 0x1015139C
ADD_SEPARATOR = 0x10004BC0
CTRACE_FN = 0x1006FCBC
CONSTRUCT_STACK = 0x100936F4

# ---- the destructors the sinit registers (what r4 holds in __register_global_object)
DTOR_FILE, DTOR_STRING, DTOR_OFFSET = 0x103BCB68, 0x103BCD38, 0x102663D0
DTOR_KIND = {DTOR_FILE: 0, DTOR_STRING: 1, DTOR_OFFSET: 2}

# ---- the Mac's addresses of the globals (the asm's TOC comments); the host library's come from tpw_hwr_addr(name)
GLOBALS = 0x103B74C8        # gTPGlobals; its members at 0x40EC and 0x4100 are the two file names
ADDR = dict(
    file=0x102E058C,        # the CDiscFile; its word at +4 is the access object
    header=0x102E056C,      # "#ThemeParkWorld - Hoardings.sam", never written
    note=0x102E054C,        # "#DO NOT EDIT BY HAND", never written
    prefix=0x102E052C,      # "HoardingClicks["
    keyx=0x102E050C,        # "].X"
    keyy=0x102E04EC,        # "].Y"
    eol=0x102E04CC,         # "\n"
    sep=0x102E04AC,         # "\t"
    name1=GLOBALS + 0x40EC,
    name2=GLOBALS + 0x4100,
    right=0x1029E03C,
    bottom=0x1029E040,
    temp=0x102E07B0,        # the click the walk hands out (8 bytes before the copy)
    copy=0x102E07B8,        # 0x400 clicks of 8 bytes
    copy_index=0x102E27B8,  # the two words that follow the copy array
    copy_count=0x102E27BC,
    stack=0x102E27C0,       # 0x400 clicks of 8 bytes
    stack_count=0x102E47C0,
    g4818=0x102E4818,       # a byte
    offsets=0x102DE0E4,     # nine (x, y) pairs of words
    word481c=0x102E481C,
    word4820=0x102E4820,
)
SINIT_STRINGS = ["header", "note", "prefix", "keyx", "keyy", "eol", "sep"]   # in the order the sinit builds them
SINIT_ORDER = [ADDR["file"]] + [ADDR[n] for n in SINIT_STRINGS] + [ADDR["offsets"] + 8 * k for k in range(9)]
# ---- the host's offsets and sizes the module relies on, checked once against tpw_hwr_layout (catches a header that moved)
HOST_LAYOUT = [8, 4, 0x2000]

s32 = T.s32


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)
R = CL.recorder
ACCESS = {}       # the persistent access object and its vtable, made once by setup


def string_then_empty(e, L):
    CL.tds_set(e, e.r(3), "")


def string_then_text(e, L):
    CL.tds_set(e, e.r(3), e.cstr(e.r(4)))


def string_then_append(e, L):
    CL.tds_set(e, e.r(3), CL.tds_text(e, e.r(3)) + CL.tds_text(e, e.r(4)))


# the bullfrog string members the writers use: functional (they keep the text) and recorded
STUBS.imp("__ct__26TbDynamicStringTemplate<c>Fv", R("TbString()", "N", then=string_then_empty))
STUBS.imp("__ct__26TbDynamicStringTemplate<c>FPCc", R("TbString(char*)", "Ns", then=string_then_text))
STUBS.imp("__dt__26TbDynamicStringTemplate<c>Fv", R("~TbString", "L"))
STUBS.imp("__as__26TbDynamicStringTemplate<c>FPCc", R("TbString=", "Ls", then=string_then_text))
STUBS.imp("__apl__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>", R("TbString+=", "LT", then=string_then_append))


def stub_get_theme_path(e):
    """char* get_theme_path(): the case's text, in fresh memory."""
    e.cl.rec("get_theme_path")
    e.set_r(3, CL.new_text(e, e.hw["theme"]))


def stub_add_separator(e):
    """FS_AddDirectorySeparator(TbString& path): records the path, ends it with ':'."""
    e.cl.rec("FS_AddDirectorySeparator", (), (CL.tds_text(e, e.r(3)),))
    CL.tds_set(e, e.r(3), CL.tds_text(e, e.r(3)) + ":")


def stub_construct_stack(e):
    """LLCC_SystemConstructHoardingClickStack(): records gLLCC4818, leaves the case's clicks in the stack and its count word."""
    e.cl.rec("ConstructHoardingClickStack", (e.read(ADDR["g4818"], 1)[0],))
    for k, (x, y) in enumerate(e.hw["clicks"]):
        e.write(ADDR["stack"] + 8 * k, struct.pack(">II", x & MASK, y & MASK))
    w32(e, ADDR["stack_count"], e.hw["count"])


def stub_itoa(e):
    """char* _itoa(int value, char* buffer, int radix): the signed decimal digits (the writers only ask for radix 10)."""
    value, buffer, radix = s32(e.r(3)), e.r(4), s32(e.r(5))
    e.cl.rec("_itoa", (value, radix))
    e.write(buffer, (str(value) + "\0").encode())
    e.set_r(3, buffer)


def stub_disc_open(e):
    """TbError CDiscFile::Open(const char* name, uint mode): the hidden result in r3, this in r4; the answer is the queue's."""
    e.cl.rec("CDiscFile::Open", (e.cl.lab(e.r(4)), e.r(6)), (e.cstr(e.r(5))[:159],))
    w32(e, e.r(3), e.cl.ret("CDiscFile::Open", -1))


def stub_register(e):
    """__register_global_object(object, destructor, node): kept for the sinit's comparison."""
    e.hw["regs"].append((e.r(3), e.r(4)))


def access_write(e):
    """The access object's Write (vtable slot +0x10): this, source, size; records the size and the bytes, answers from the queue."""
    L = e.cl
    size = e.r(5)
    data = e.read(e.r(4), min(size, 159)).decode("latin-1")
    L.rec("Access::Write", (size,), (data,))
    e.set_r(3, L.ret("Access::Write", 0) & MASK)


STUBS.internal(GET_THEME_PATH, stub_get_theme_path)
STUBS.internal(ADD_SEPARATOR, stub_add_separator)
STUBS.internal(CTRACE_FN, R("CTRACE", "us"))
STUBS.internal(CONSTRUCT_STACK, stub_construct_stack)
STUBS.imp("_itoa__10NS_MacDozeFiPci", stub_itoa)
STUBS.imp("__ct__Q33bfl10FileSystem9CDiscFileFv", R("CDiscFile", "N"))
STUBS.imp("Close__Q33bfl10FileSystem5CFileFv", R("CFile::Close", "L"))
STUBS.imp("Open__Q33bfl10FileSystem9CDiscFileFPCcUi", stub_disc_open)
STUBS.imp("__register_global_object", stub_register)


def setup(runner):
    """Once: the standard C library stubs, and the access object with a fake vtable whose Write slot is a Python function."""
    e = runner.emu
    T.install_std_stubs(e)
    vtable = T.persist_alloc(e, 0x40)
    w32(e, vtable + 0x10, T.fake_function(e, access_write))
    ACCESS["object"] = T.persist_alloc(e, 0x10, struct.pack(">IIII", vtable, 0, 0, 0))


# ---------------------------------------------------------------------------------------------------------------- the two sides

TEXT_ID = dict(prefix=0, keyx=1, keyy=2, eol=3, sep=4, name1=5, name2=6, theme=7, key=8)   # tpw_hwr_set_text's ids


class Side:
    """What the shared code needs from either memory."""

    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class QuietLog(CL.PpcLog):
    """A log that keeps no events but still answers labels and queues: a walk of 0x400 clicks makes more events than the host's log holds,
    so those cases (quiet=1) compare the state alone."""

    def rec(self, *args, **kwargs):
        pass


class Ppc(Side):
    """One case on the PowerPC side: the fakes active, a fresh log, memory in the emulator's image."""
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = (QuietLog if case.get("quiet") else CL.PpcLog)({ADDR["file"]: 1}, case["queues"])
        e.hw = dict(theme="", key=0, clicks=[], count=0, regs=[])
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

    def set_text(self, name, text):
        """The string of the case: a static object, a member of gTPGlobals, the theme path get_theme_path answers, or the line writer's key."""
        if name == "theme":
            self.e.hw["theme"] = text
        elif name == "key":
            self.e.hw["key"] = self.e.alloc(0x14)
            CL.tds_set(self.e, self.e.hw["key"], text)
        else:
            CL.tds_set(self.e, ADDR[name], text)

    def set_access(self, on):
        w32(self.e, ADDR["file"] + 4, ACCESS["object"] if on else 0)

    def plan(self, clicks, count):
        self.e.hw["clicks"] = clicks
        self.e.hw["count"] = count

    def call_line(self, index, value):
        """int LLCC_WriteHoardingsLine(const TbString& key, int index, int value)."""
        r3, _, _ = T.call_ppc(self.e, FN_LINE, [self.e.hw["key"], index & MASK, value & MASK])
        return r3 & MASK

    def call_writer(self, fn):
        """int LLCC_WriteHoardingsToBalanceFile() and its Complete twin."""
        r3, _, _ = T.call_ppc(self.e, fn, [])
        return r3 & MASK

    def call_sinit(self):
        T.call_ppc(self.e, FN_SINIT, [])

    def scramble(self, seed):
        """Junk over every static the sinit builds, so a word it forgets to write shows."""
        self.words(ADDR["offsets"], junk(seed, 18))
        self.words(ADDR["word481c"], junk(seed + 1, 2))
        for name in SINIT_STRINGS:
            self.words(ADDR[name], junk(seed + 2, 5))

    def registered(self):
        """The sinit's registrations as (position in the construction order, destructor kind) lists."""
        regs = self.e.hw["regs"]
        objects = [SINIT_ORDER.index(o) if o in SINIT_ORDER else -1 for o, _ in regs]
        kinds = [DTOR_KIND.get(d, -1) for _, d in regs]
        return objects, kinds

    def sinit_state(self):
        """The seven strings (each text followed by '|'), the nine offsets as 18 signed words, the two words."""
        texts = "".join(CL.tds_text(self.e, ADDR[n]) + "|" for n in SINIT_STRINGS)
        offsets = [s32(w) for w in self.rd_words(ADDR["offsets"], 18)]
        return texts, offsets, [self.rd_word(ADDR["word481c"]), self.rd_word(ADDR["word4820"])]


class Host(Side):
    """The same on the host library."""
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_hwr_reset()
        lib.tpw_hwr_quiet(1 if case.get("quiet") else 0)
        CL.host_begin(lib, {1: self.g("file")}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_hwr_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_hwr_alloc(n)       # zeroed, 32-bit addressable (platform/arena.cpp)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_text(self, name, text):
        self.lib.tpw_hwr_set_text(TEXT_ID[name], text.encode("latin-1"))

    def set_access(self, on):
        self.lib.tpw_hwr_set_access(1 if on else 0)

    def plan(self, clicks, count):
        flat = [s32(v) for click in clicks for v in click]
        words = (C.c_int32 * max(1, len(flat)))(*flat)
        self.lib.tpw_hwr_plan(s32(count), len(clicks), words)

    def call_line(self, index, value):
        return self.lib.tpw_hwr_line(s32(index), s32(value)) & MASK

    def call_writer(self, fn):
        call = self.lib.tpw_hwr_write if fn == FN_WRITE else self.lib.tpw_hwr_write_complete
        return call() & MASK

    def call_sinit(self):
        pass                                    # the statics were built when the library loaded

    def scramble(self, seed):
        pass

    def registered(self):
        """The definition order: the file, the seven strings, the nine offsets."""
        return list(range(17)), [0] + [1] * 7 + [2] * 9

    def sinit_state(self):
        texts = C.create_string_buffer(512)
        words = (C.c_int32 * 20)()
        self.lib.tpw_hwr_sinit_state(texts, words)
        return texts.value.decode("latin-1"), list(words)[:18], [words[18] & MASK, words[19] & MASK]


def bind(lib):
    """The host library's entry points, typed once; the layout check runs here."""
    if getattr(lib, "bound", False):
        return
    lib.tpw_hwr_addr.restype, lib.tpw_hwr_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_hwr_alloc.restype, lib.tpw_hwr_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_hwr_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_hwr_set_text.argtypes = [C.c_int32, C.c_char_p]
    lib.tpw_hwr_set_access.argtypes = [C.c_int32]
    lib.tpw_hwr_quiet.argtypes = [C.c_int32]
    lib.tpw_hwr_plan.argtypes = [C.c_int32, C.c_int32, C.POINTER(C.c_int32)]
    lib.tpw_hwr_sinit_state.argtypes = [C.c_char_p, C.POINTER(C.c_int32)]
    lib.tpw_hwr_line.restype, lib.tpw_hwr_line.argtypes = C.c_uint32, [C.c_int32, C.c_int32]
    lib.tpw_hwr_write.restype, lib.tpw_hwr_write.argtypes = C.c_uint32, []
    lib.tpw_hwr_write_complete.restype, lib.tpw_hwr_write_complete.argtypes = C.c_uint32, []
    out = (C.c_int64 * 32)()
    n = lib.tpw_hwr_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

EXTREME = [0, 1, -1, 2, -2, INT_MAX, INT_MIN, 0x200, 0x100, 0xFF, -0x100, 100, -100, 0x7FFF, 0x8000, -0x8000]
KEY_NAMES = ["prefix", "keyx", "keyy", "eol", "sep"]
STD_TEXT = dict(prefix="HoardingClicks[", keyx="].X", keyy="].Y", eol="\n", sep="\t")
PRINTABLE = string.ascii_letters + string.digits + " .:/[]_-#=\\"


def rv(r):
    """A 32-bit value as a signed number: an extreme, a small one or any."""
    k = r.random()
    if k < 0.25:
        return r.choice(EXTREME)
    if k < 0.6:
        return r.randrange(-300, 300)
    return s32(r.getrandbits(32))


def junk(seed, n):
    """n words of junk from a seed (the stack and the copy are filled with it before every call)."""
    r = random.Random(seed)
    return [rv(r) & MASK for _ in range(n)]


def clicks_of(case):
    r = random.Random(case["click_seed"])
    return [(rv(r), rv(r)) for _ in range(case["nclicks"])]


def build(s, case):
    """Writes the case's globals and blocks through Side: the same code for both memories (offsets are the Mac's)."""
    s.words(s.g("stack"), junk(case["stack_seed"], 0x800))
    s.words(s.g("copy"), junk(case["copy_seed"], 0x800))
    s.words(s.g("temp"), case["temp0"])
    s.words(s.g("copy_index"), [case["index0"]])
    s.words(s.g("copy_count"), [case["count0"]])
    s.words(s.g("stack_count"), [case["stack_count0"]])
    s.words(s.g("right"), [case["right"]])
    s.words(s.g("bottom"), [case["bottom"]])
    s.write(s.g("g4818"), bytes([case["g4818"]]))
    for name in KEY_NAMES + ["name1", "name2", "theme", "key"]:
        s.set_text(name, case[name])
    s.set_access(case["access"])
    s.plan(clicks_of(case), case["count"])


def execute(s, case):
    """The call."""
    if case["op"] == "line":
        return s.call_line(case["index"], case["value"])
    return s.call_writer(FN_WRITE if case["op"] == "write" else FN_COMPLETE)


def observe(s, case, answer):
    """Everything the call may change, read back the same way on both sides."""
    return {
        "answer": answer,
        "log": s.log(),
        "g4818": s.read(s.g("g4818"), 1)[0],
        "copy_index": s.rd_word(s.g("copy_index")),
        "copy_count": s.rd_word(s.g("copy_count")),
        "temp": s.rd_words(s.g("temp"), 2),
        "stack_count": s.rd_word(s.g("stack_count")),
        "stack": s.rd_words(s.g("stack"), 0x800),
        "copy": s.rd_words(s.g("copy"), 0x800),
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


def run_sinit(s, case):
    """The sinit: junk over the statics, the call, then the statics and the registrations read back."""
    try:
        s.scramble(case["seed"])
        s.call_sinit()
        texts, offsets, words = s.sinit_state()
        objects, kinds = s.registered()
        s.log()                                 # the sinit's own events are not compared (the host's statics were built at load)
        return {"texts": texts, "offsets": offsets, "words": words, "objects": objects, "kinds": kinds}
    finally:
        s.done()


def sinit_ppc(e, case):
    return run_sinit(Ppc(e, case), case)


def sinit_host(lib, case):
    return run_sinit(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators


def text(r, lo, hi):
    return "".join(r.choice(PRINTABLE) for _ in range(r.randint(lo, hi)))


def base(r, op):
    """The world every case starts from: texts mostly standard, an open file mostly, junk everywhere else."""
    c = {"op": op}
    for name in KEY_NAMES:
        c[name] = STD_TEXT[name] if r.random() < 0.6 else text(r, 0, 10)
    if r.random() < 0.15:
        c["eol"] = r.choice(["", "\r\n", "\n\n", " "])
    if r.random() < 0.15:
        c["sep"] = r.choice(["", "\t\t", ", ", " "])
    c["name1"] = "hoardings.sam" if r.random() < 0.5 else text(r, 0, 14)
    c["name2"] = "endhoardings.sam" if r.random() < 0.5 else text(r, 0, 14)
    if c["name1"] == c["name2"]:
        c["name2"] += "x"
    c["theme"] = r.choice(["", "Themes:Jungle", "hd:Themes:Space", "Halloween:"]) if r.random() < 0.6 else text(r, 0, 40)
    c["key"] = r.choice(["", STD_TEXT["keyx"], STD_TEXT["keyy"]]) if r.random() < 0.5 else text(r, 1, 30)
    c["opened"] = r.choice([0, 1, 2, 5, 100, 1 << 20, INT_MAX]) if r.random() < 0.8 else r.choice([-1, -2, -100, INT_MIN, INT_MIN + 1])
    c["access"] = 1 if r.random() < 0.85 else 0
    c["right"] = rv(r)
    c["bottom"] = rv(r)
    c["count"] = r.randrange(0, 12) if r.random() < 0.7 else r.choice([r.randrange(-5, 0), r.randrange(0, 70)])
    c["nclicks"] = c["count"] if 0 <= c["count"] <= 0x400 and r.random() < 0.75 else r.randrange(0, 6)
    c["click_seed"] = r.getrandbits(30)
    c["stack_seed"] = r.getrandbits(30)
    c["copy_seed"] = r.getrandbits(30)
    c["index0"] = rv(r)
    c["count0"] = rv(r)
    c["temp0"] = [rv(r) & MASK, rv(r) & MASK]
    c["stack_count0"] = rv(r) & MASK
    c["g4818"] = r.choice([0, 1, 0x55, 0xFF, r.randrange(256)])
    c["index"] = rv(r)
    c["value"] = rv(r)
    answers = [r.choice([0, 0, 0, -1, 5, 1 << 20, INT_MIN]) for _ in range(r.randrange(0, 5))]
    c["queues"] = {"CDiscFile::Open": [c["opened"]], "Access::Write": answers}
    return c


def finish(c):
    """The queue of the Open answer follows the case's field (an edge sets the field after the base was made)."""
    c["queues"]["CDiscFile::Open"] = [c["opened"]]
    return c


# ---- the writers
OPEN_EDGES = [-1, -2, INT_MIN, INT_MIN + 1, -100, 0, 1, 5, INT_MAX]
COUNT_EDGES = [-3, -2, -1, 0, 1, 2, 3, 4, 5, 10, 40, 70, 100]
EXTENT_EDGES = [(0, 0), (-1, -1), (INT_MIN, INT_MAX), (INT_MAX, INT_MIN), (1, -1), (0x200, 0x100)]


def writer_case(r, op, **fields):
    c = base(r, op)
    c.update(fields)
    return finish(c)


def writer_edges(op, seed):
    r = random.Random(seed)
    for opened in OPEN_EDGES:
        yield writer_case(r, op, opened=opened, access=1, count=r.randrange(0, 4), nclicks=2)
    for access in (1, 0):
        for count in COUNT_EDGES:
            yield writer_case(r, op, opened=1, access=access, count=count, nclicks=count if count > 0 else r.randrange(0, 3))
    for count in (5, 12):
        for nclicks in (0, 1, count - 1):
            yield writer_case(r, op, opened=1, access=1, count=count, nclicks=nclicks)
    for right, bottom in EXTENT_EDGES:
        yield writer_case(r, op, opened=1, access=1, count=3, nclicks=3, right=right, bottom=bottom)
    for g4818 in (0, 1, 0x55, 0xFF):
        yield writer_case(r, op, opened=1, access=1, count=2, nclicks=2, g4818=g4818)
    for index0 in (0, 1, 0x3FF, 0x400, 0x401, -1, INT_MIN, INT_MAX):
        yield writer_case(r, op, opened=1, access=1, count=2, nclicks=2, index0=index0, count0=index0 ^ 0x55, stack_count0=index0)
    empties = [dict(prefix=""), dict(keyx=""), dict(keyy=""), dict(eol=""), dict(sep=""), dict(name1=""), dict(name2=""), dict(theme=""),
               dict(prefix="", keyx="", keyy="", eol="", sep="", name1="", name2="", theme="")]
    for fields in empties:
        yield writer_case(r, op, opened=1, access=1, count=2, nclicks=2, **fields)
    yield writer_case(r, op, opened=1, access=1, count=2, nclicks=2, name1="hoardings.sam", name2="endhoardings.sam", theme="Themes:Jungle")
    yield writer_case(r, op, opened=1, access=1, count=2, nclicks=2, name1="a", name2="b", theme="x" * 60)
    for answers in ([-1] * 12, [5, 0, -1, 7], [INT_MIN, INT_MAX]):
        yield writer_case(r, op, opened=1, access=1, count=2, nclicks=2, queues={"CDiscFile::Open": [1], "Access::Write": answers})
    for count in (0x3FF, 0x400):
        yield writer_case(r, op, opened=1, access=0, count=count, nclicks=count, quiet=1)


def edges_of(op, seed):
    return lambda: list(writer_edges(op, seed))


def random_of(op):
    return lambda r: writer_case(r, op)


# ---- the line writer
INDEX_EDGES = [-1, 0, 1, 9, 10, 99, 100, 12345, -2, -10, -1000000, INT_MIN, INT_MAX]
VALUE_EDGES = [0, 1, -1, 0x200, 10, 99999, -123456, INT_MIN, INT_MAX]
KEY_EDGES = ["", "].X", "].Y", "abc", "k" * 30]


def line_case(r, **fields):
    c = base(r, "line")
    c.update(fields)
    return finish(c)


def line_edges():
    r = random.Random(20801)
    for index in INDEX_EDGES:
        for value in VALUE_EDGES:
            yield line_case(r, index=index, value=value, access=1)
    for key in KEY_EDGES:
        yield line_case(r, key=key, index=3, value=4, access=1)
        yield line_case(r, key=key, index=-1, value=4, access=1)
    for index in (-1, 0, 5):
        yield line_case(r, index=index, value=7, access=0)
    for answers in ([-1] * 8, [5, 0, -1, 7, 9, 11], [INT_MIN]):
        yield line_case(r, index=12, value=-34, access=1, queues={"CDiscFile::Open": [1], "Access::Write": answers})
    for name in KEY_NAMES:
        yield line_case(r, index=1, value=2, access=1, **{name: ""})
    yield line_case(r, index=1, value=2, access=1, prefix="P", keyx="X", keyy="Y", eol="E", sep="S", key="K")


def line_random(r):
    return line_case(r)


# ---- the sinit
def sinit_case(r):
    return {"op": "sinit", "queues": {}, "seed": r.getrandbits(30)}


def sinit_edges():
    yield sinit_case(random.Random(20803))


def sinit_random(r):
    return sinit_case(r)


TESTS = [
    FnTest("LLCC_WriteHoardingsLine", ppc, host, edge=line_edges, rnd=line_random),
    FnTest("LLCC_WriteHoardingsToBalanceFile", ppc, host, edge=edges_of("write", 20805), rnd=random_of("write")),
    FnTest("LLCC_WriteCompleteHoardingsToBalanceFile", ppc, host, edge=edges_of("complete", 20807), rnd=random_of("complete")),
    FnTest("__sinit_LLCellSystem_cpp", sinit_ppc, sinit_host, edge=sinit_edges, rnd=sinit_random),
]
