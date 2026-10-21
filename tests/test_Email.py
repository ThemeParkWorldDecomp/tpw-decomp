"""Unit w90, against libtpw_unit_Email.dylib (tests/unit_Email.cpp), in the main app ("SimTheme Park"):
  src/game/online/email.cpp   Outbox::~Outbox 0x10203ba8, Outbox::Load 0x10203ce0, AddressBook::Load 0x10206b2c
  src/game/ui/IME.cpp  IME::CIME::Activate 0x101feab0, UnActivate 0x101feae8, SetMaxCompositionLength 0x101feb18
(LbFile_Copy 0x10000ddc, the unit's seventh function, is in bullfrog and its cases are in test_LbFile.py.)

The two loaders run the original on the PowerPC side with the MSL library as it is in the binary: the strings (assign, append,
the constructors), the lists (erase, insert, sort, unique, clear), operator==, ios_base::good and setstate, basic_ios::tie and the
real basic_istream::getline, which reads the stream object this test lays out (the ios sub-object, its state word, the file
buffer's get area). Only the pieces under the stream are stand-ins, because they need the C library's FILE and the locale
that the dump does not initialise: the ifstream constructor (it lays out the stream), basic_filebuf::open and close (they serve a
file of the case's table into the get area, and record "open" with the name and "close"), the three destructors of the stream
parts, ios_base::getloc, use_facet<ctype> (a facet whose widen answers its argument) and __ptr_glue (the facet's widen and the
buffer's underflow, which answers the end of the file). The host side runs the translation with the platform's file calls faked
over the same table (they record "open" and "close" as well). Every case compares the call logs (open, close, CreateDirectory,
~Email, delete), the answer and every member of the object, read back through the MSL layouts (a string is a pointer to a
representation of length, capacity, count and data pointer; a list is its size, then a sentinel linked to itself).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_Email"
MASK = 0xFFFFFFFF

OUTBOX_CTOR, OUTBOX_DTOR, OUTBOX_LOAD = 0x10203940, 0x10203BA8, 0x10203CE0
BOOK_CTOR, BOOK_LOAD = 0x102064CC, 0x10206B2C
IME_ACTIVATE, IME_UNACTIVATE, IME_SETMAX = 0x101FEAB0, 0x101FEAE8, 0x101FEB18
# MSL pieces called to set a case up (the real functions)
STR_DEFAULT_CTOR, STR_CSTR_CTOR, STR_ASSIGN_CSTR = 0x1007363C, 0x10002338, 0x101B2440
LIST_NUM_INSERT, LIST_STR_INSERT = 0x102092F8, 0x101B25E0
# pieces under the stream (stand-ins)
IFSTREAM_CTOR, FILEBUF_OPEN, FILEBUF_CLOSE = 0x1020801C, 0x10071C44, 0x10071B54
FILEBUF_DTOR, ISTREAM_DTOR, BASIC_IOS_DTOR = 0x100700E4, 0x102043A8, 0x1006FF50
GETLOC, USE_FACET, LOCALE_DTOR, PTR_GLUE = 0x10072A34, 0x10073934, 0x10071F34, 0x1023BAA8
EMAIL_DTOR = 0x1020248C
COMPLAIN = 0x102B2B94           # gOnlineConfig.mComplainEmail
WIDEN_TV, UNDERFLOW_TV, UFLOW_TV = 0x7C0FE100, 0x7C0FE200, 0x7C0FE300   # the facet's and the buffer's functions (never executed)

LABEL_OUTBOX, LABEL_EMAIL = 1, 2

# the outbox's members: field number (the host's accessor), Mac offset, kind
OUTBOX_STRINGS = [(0, 0x00), (1, 0x04), (2, 0x08), (3, 0x18), (4, 0x50)]
OUTBOX_NUMBERS = [(6, 0x28), (7, 0x34), (8, 0x40)]
OUTBOX_LIST_OF_STRINGS = (5, 0x0C)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def F(lib, name, restype, *argtypes):
    fn = getattr(lib, name)
    fn.restype = restype
    fn.argtypes = list(argtypes)
    return fn


# ---------------------------------------------------------------- the MSL layouts

def msl_text(e, string):
    """The text of the MSL string at `string`: a pointer to (length, capacity, count, data pointer)."""
    rep = e.u32(string)
    return e.read(e.u32(rep + 12), e.u32(rep)).decode("latin-1")


def msl_list(e, head, value):
    """The values of the MSL list at `head` (size, then a sentinel node at +4 of (previous, next); a node is
    (previous, next, value at +8)) and whether its links and its size are consistent."""
    sentinel = head + 4
    size = e.u32(head)
    values, previous, node, ok = [], sentinel, e.u32(head + 8), True
    while node != sentinel and len(values) < 5000:
        if e.u32(node) != previous:
            ok = False
        values.append(value(e, node + 8))
        previous, node = node, e.u32(node + 4)
    if e.u32(sentinel) != previous:
        ok = False
    return values, int(ok and size == len(values))


def number_value(e, address):
    return e.u32(address)


# ---------------------------------------------------------------- the PowerPC side's world

class World:
    """One case on the PowerPC side: the files, the log, the facet and the buffer's table."""

    def __init__(self, e, case, fixed=None):
        self.e = e
        e.heap_in_scratch = True
        e.alloc_log.clear()
        self.L = CL.PpcLog(fixed or {}, case.get("q"))
        e.cl = self.L
        e.w90 = self
        self.files = case.get("files", {})
        self.facet = e.alloc(8)
        self.facet_table = e.alloc(0x40)
        w32(e, self.facet, self.facet_table)
        w32(e, self.facet_table + 0x1C, WIDEN_TV)
        self.buffer_table = e.alloc(0x40)
        w32(e, self.buffer_table + 0x28, UNDERFLOW_TV)
        w32(e, self.buffer_table + 0x2C, UFLOW_TV)
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def call(self, address, *args):
        return T.call_ppc(self.e, address, [a & MASK for a in args])[0]


def atoi_text(text):
    """MSL's atoi (strtol in base 10): white space, a sign, digits; a number beyond the 32-bit limits gives the limit."""
    i = 0
    while i < len(text) and text[i] in " \t\n\v\f\r":
        i += 1
    sign = 1
    if i < len(text) and text[i] in "+-":
        sign = -1 if text[i] == "-" else 1
        i += 1
    j = i
    while j < len(text) and text[j] in "0123456789":
        j += 1
    value = sign * (int(text[i:j]) if j > i else 0)
    return max(-0x80000000, min(0x7FFFFFFF, value))


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder

    def ifstream_ctor(e):
        # the stream object: +0 the pointer to its ios sub-object (at +0x4c), +4 the character count, +8 the vptr, the file buffer
        # at +0xc (0x40 bytes), the ios sub-object (0x48 bytes: +0x24 its buffer, +0x34 the state, +0x38 the exception mask,
        # +0x3c the tied stream); state good, nothing tied, no exceptions
        stream, w = e.r(3), e.w90
        e.write(stream, b"\0" * 0x94)
        w32(e, stream, stream + 0x4C)
        w32(e, stream + 0x4C + 0x24, stream + 0xC)
        w32(e, stream + 0xC, w.buffer_table)
        e.set_r(3, stream)

    def filebuf_open(e):
        buffer, name, mode = e.r(3), e.cstr(e.r(4)), e.r(5)
        w = e.w90
        w.L.rec("open", (), (name[:159],))
        if mode != 8:
            w.L.rec("open: mode", (mode,))
        data = w.files.get(name)
        if data is None:
            e.set_r(3, 0)
            return
        block = e.alloc(max(len(data), 1), data if data else None)
        w32(e, buffer + 8, block)                    # the get area: next and end
        w32(e, buffer + 0xC, block + len(data))
        w32(e, buffer + 0x24, 1)                     # open
        e.set_r(3, buffer)

    def filebuf_close(e):
        buffer = e.r(3)
        if e.u32(buffer + 0x24) == 0:
            e.set_r(3, 0)
            return
        e.w90.L.rec("close")
        w32(e, buffer + 0x24, 0)
        e.set_r(3, buffer)

    def use_facet(e):
        e.set_r(3, e.w90.facet)

    def ptr_glue(e):
        function = e.r(12)
        w32(e, e.r(1) + 0x14, e.r(2))     # the glue saves the TOC register where its caller restores it from
        if function == WIDEN_TV:
            e.set_r(3, e.r(4) & 0xFF)
        elif function == UNDERFLOW_TV:
            e.set_r(3, MASK)
        else:
            e.w90.L.rec("glue: unknown function", (function,))
            e.set_r(3, MASK)

    def atoi(e):
        e.set_r(3, T.u32(atoi_text(e.cstr(e.r(3), limit=256))))

    def memcmp(e):
        # the C library's memcmp (an import): unsigned bytes, the sign of the first difference
        a, b, n = e.r(3), e.r(4), e.r(5)
        left, right = (e.read(a, n), e.read(b, n)) if n else (b"", b"")
        e.set_r(3, T.u32(0 if left == right else (-1 if left < right else 1)))

    def delete(e):
        L, obj = e.cl, e.r(3)
        if obj and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    S.internal(IFSTREAM_CTOR, ifstream_ctor)
    S.internal(FILEBUF_OPEN, filebuf_open)
    S.internal(FILEBUF_CLOSE, filebuf_close)
    for address in (FILEBUF_DTOR, ISTREAM_DTOR, BASIC_IOS_DTOR, GETLOC, LOCALE_DTOR):
        S.internal(address, lambda e: None)
    S.internal(USE_FACET, use_facet)
    S.internal(PTR_GLUE, ptr_glue)
    S.internal(EMAIL_DTOR, R("~Email", "D"))
    S.imp("atoi", atoi)
    S.imp("memcmp", memcmp)
    S.imp("CreateDirectory__10NS_MacDozeFPCcPv", R("CreateDirectory", "sL", ret="i"))
    S.imp("__dl__FPv", delete)
    return S


STUBS = None


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    STUBS = build_stubs()


# ---------------------------------------------------------------- setting up and reading the PowerPC objects

def set_string(w, address, text):
    e = w.e
    raw = text.encode("latin-1")
    w.call(STR_ASSIGN_CSTR, address, e.alloc(len(raw) + 1, raw + b"\0"), len(raw))


def make_string(w, text):
    """A new MSL string object holding `text`."""
    e = w.e
    address = e.alloc(8)
    w.call(STR_DEFAULT_CTOR, address, e.alloc(4))
    if text:
        set_string(w, address, text)
    return address


def push_number(w, head, value):
    e = w.e
    w.call(LIST_NUM_INSERT, e.alloc(4), head, head + 4, e.alloc(4, struct.pack(">I", value & MASK)))


def push_string(w, head, text):
    e = w.e
    w.call(LIST_STR_INSERT, e.alloc(4), head, head + 4, make_string(w, text))


def new_outbox(w, pre, label=True):
    e = w.e
    obj = e.alloc(0x60, b"\xa5" * 0x60)
    if label:
        w.L.labels[obj] = LABEL_OUTBOX
    w.call(OUTBOX_CTOR, obj)
    for field, offset in OUTBOX_STRINGS:
        if field in pre:
            set_string(w, obj + offset, pre[field])
    for text in pre.get(5, []):
        push_string(w, obj + 0x0C, text)
    for field, offset in OUTBOX_NUMBERS:
        for value in pre.get(field, []):
            push_number(w, obj + offset, value)
    if 9 in pre:
        e.write(obj + 0x54, struct.pack(">H", pre[9]))
    if 10 in pre:
        w32(e, obj + 0x58, pre[10])
    if pre.get("email"):
        email = e.alloc(76, b"\xa5" * 76)
        w.L.labels[email] = LABEL_EMAIL
        w32(e, obj + 0x5C, email)
    return obj


def read_outbox(e, obj):
    """Every member as the host's accessor renders it."""
    out, links = {}, 1
    for field, offset in OUTBOX_STRINGS:
        out[field] = msl_text(e, obj + offset)
    values, ok = msl_list(e, obj + 0x0C, msl_text)
    out[5] = values
    links &= ok
    for field, offset in OUTBOX_NUMBERS:
        values, ok = msl_list(e, obj + offset, number_value)
        out[field] = values
        links &= ok
    out[9] = struct.unpack(">H", e.read(obj + 0x54, 2))[0]
    out[10] = T.s32(e.u32(obj + 0x58))
    out[11] = int(e.u32(obj + 0x5C) != 0)
    out["links"] = links
    return out


def host_outbox(lib, obj):
    """The same, read through the host's accessors."""
    get = F(lib, "tpw_email_outbox_get", C.c_int32, C.c_void_p, C.c_int32, C.c_char_p, C.c_int32)
    out = {}
    buffer = C.create_string_buffer(1 << 16)

    def text(field):
        n = get(obj, field, buffer, 1 << 16)
        return buffer.raw[:n].decode("latin-1")

    for field, _ in OUTBOX_STRINGS:
        out[field] = text(field)
    out[5] = text(5).split("\x01")[:-1]
    for field, _ in OUTBOX_NUMBERS:
        out[field] = [int(v) for v in text(field).split(",")] if text(field) else []
    out[9] = int(text(9))
    out[10] = int(text(10))
    out[11] = int(text(11))
    out["links"] = 1
    return out


def host_set_outbox(lib, obj, pre):
    put = F(lib, "tpw_email_outbox_set", None, C.c_void_p, C.c_int32, C.c_char_p, C.c_int32)
    for field, _ in OUTBOX_STRINGS:
        if field in pre:
            raw = pre[field].encode("latin-1")
            put(obj, field, raw, len(raw))
    if 5 in pre:
        raw = "".join(item + "\x01" for item in pre[5]).encode("latin-1")
        put(obj, 5, raw, len(raw))
    for field, _ in OUTBOX_NUMBERS:
        if field in pre:
            raw = ",".join(str(v & MASK) for v in pre[field]).encode()
            put(obj, field, raw, len(raw))
    for field in (9, 10):
        if field in pre:
            raw = str(pre[field]).encode()
            put(obj, field, raw, len(raw))


def host_files(lib, files):
    F(lib, "tpw_email_files_clear", None)()
    put = F(lib, "tpw_email_file", None, C.c_char_p, C.c_char_p, C.c_int32)
    for name, data in files.items():
        put(name.encode("latin-1"), data, len(data))


def host_start(lib, case, fixed=None):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    host_files(lib, case.get("files", {}))
    CL.host_begin(lib, fixed or {}, case.get("q"))


# ---------------------------------------------------------------- Outbox::Load

def outbox_load_ppc(e, case):
    w = World(e, case)
    try:
        obj = new_outbox(w, case["pre"], label=False)
        path = e.alloc(len(case["path"]) + 1, case["path"].encode("latin-1") + b"\0")
        before = e.read(obj + 0x1C, 12) + e.read(obj + 0x4C, 4)
        answer = w.call(OUTBOX_LOAD, obj, path)
        out = read_outbox(e, obj)
        out["untouched"] = int(before == e.read(obj + 0x1C, 12) + e.read(obj + 0x4C, 4))
        return dict(answer=answer, out=out, log=w.L.log)
    finally:
        w.done()


def outbox_load_host(lib, case):
    obj = F(lib, "tpw_email_new_outbox", C.c_void_p)()
    host_set_outbox(lib, obj, case["pre"])
    if case["pre"].get("email"):
        F(lib, "tpw_email_outbox_set_email", None, C.c_void_p, C.c_void_p)(obj, F(lib, "tpw_email_new_email", C.c_void_p)())
    host_start(lib, case)
    answer = F(lib, "tpw_email_outbox_load", C.c_int32, C.c_void_p, C.c_char_p)(obj, case["path"].encode("latin-1"))
    log = CL.host_end(lib)
    out = host_outbox(lib, obj)
    out["untouched"] = 1
    F(lib, "tpw_email_free", None, C.c_void_p, C.c_int32)(obj, 0)
    return dict(answer=answer, out=out, log=log)


# ---------------------------------------------------------------- AddressBook::Load

def book_texts_ppc(e, book):
    values, ok = msl_list(e, book, msl_text)
    return values, ok


def book_load_ppc(e, case):
    w = World(e, case)
    try:
        obj = e.alloc(0x10, b"\xa5" * 0x10)
        w.call(BOOK_CTOR, obj)
        for text in case["pre"]:
            push_string(w, obj, text)
        # the complaint address: a string in the online configuration (zeroed in the image)
        w.call(STR_DEFAULT_CTOR, COMPLAIN, e.alloc(4))
        if case["complain"]:
            set_string(w, COMPLAIN, case["complain"])
        path = e.alloc(len(case["path"]) + 1, case["path"].encode("latin-1") + b"\0")
        answer = w.call(BOOK_LOAD, obj, path)
        values, ok = book_texts_ppc(e, obj)
        return dict(answer=answer, list=values, links=ok, complain=msl_text(e, COMPLAIN), log=w.L.log)
    finally:
        w.done()


def book_load_host(lib, case):
    obj = F(lib, "tpw_email_new_book", C.c_void_p)()
    raw = "".join(item + "\x01" for item in case["pre"]).encode("latin-1")
    F(lib, "tpw_email_book_set", None, C.c_void_p, C.c_char_p, C.c_int32)(obj, raw, len(raw))
    complain = case["complain"].encode("latin-1")
    F(lib, "tpw_email_complain", None, C.c_char_p, C.c_int32)(complain, len(complain))
    host_start(lib, case)
    answer = F(lib, "tpw_email_book_load", C.c_int32, C.c_void_p, C.c_char_p)(obj, case["path"].encode("latin-1"))
    log = CL.host_end(lib)
    buffer = C.create_string_buffer(1 << 16)
    n = F(lib, "tpw_email_book_get", C.c_int32, C.c_void_p, C.c_char_p, C.c_int32)(obj, buffer, 1 << 16)
    values = buffer.raw[:n].decode("latin-1").split("\x01")[:-1]
    F(lib, "tpw_email_free", None, C.c_void_p, C.c_int32)(obj, 1)
    return dict(answer=answer, list=values, links=1, complain=case["complain"], log=log)


# ---------------------------------------------------------------- Outbox::~Outbox

def dtor_ppc(e, case):
    w = World(e, case)
    try:
        obj = new_outbox(w, case["pre"])
        w.call(OUTBOX_DTOR, obj, case["flag"])
        return dict(log=w.L.log, email_word=int(e.u32(obj + 0x5C) != 0) if case["flag"] <= 0 and case["check_word"] else 0)
    finally:
        w.done()


def dtor_host(lib, case):
    obj = F(lib, "tpw_email_new_outbox", C.c_void_p)()
    host_set_outbox(lib, obj, case["pre"])
    fixed = {LABEL_OUTBOX: obj}
    if case["pre"].get("email"):
        email = F(lib, "tpw_email_new_email", C.c_void_p)()
        F(lib, "tpw_email_outbox_set_email", None, C.c_void_p, C.c_void_p)(obj, email)
        fixed[LABEL_EMAIL] = email
    host_start(lib, case, fixed)
    F(lib, "tpw_email_outbox_destroy", None, C.c_void_p, C.c_int32)(obj, case["flag"])
    log = CL.host_end(lib)
    word = 0
    if case["flag"] <= 0:
        if case["check_word"]:
            word = host_outbox(lib, obj)[11]
        F(lib, "tpw_email_release", None, C.c_void_p)(obj)
    return dict(log=log, email_word=word)


# ---------------------------------------------------------------- the IME stubs

def ime_ppc(e, case):
    w = World(e, case)
    try:
        which = case["which"]
        pattern = b"\xa5" * 16
        this = e.alloc(0x160, b"\x5a" * 0x160)
        result = e.alloc(16, pattern)
        if which == 0:
            w.call(IME_ACTIVATE, result, this, case["arg"])
        elif which == 1:
            w.call(IME_UNACTIVATE, this)
        else:
            w.call(IME_SETMAX, this, case["arg"])
        quiet = int(e.read(result, 16) == pattern and e.read(this, 0x160) == b"\x5a" * 0x160 and not w.L.log)
        return dict(quiet=quiet)
    finally:
        w.done()


def ime_host(lib, case):
    words = (C.c_int32 * 4)(*[-1] * 4)
    host_start(lib, case)
    F(lib, "tpw_email_ime", None, C.c_int32, C.c_int32, C.POINTER(C.c_int32))(case["which"], case["arg"], words)
    log = CL.host_end(lib)
    # (the Mac writes nothing into the record; the host answers zeros: both are "nothing to see")
    return dict(quiet=int(not log and (case["which"] != 0 or list(words) == [0, 0, 0, 0])))


# ---------------------------------------------------------------- cases

FOLDER = ":outbox"
OUTBOX_FILE = ":outbox:outbox.dat"       # the path plus ":outbox" plus ":outbox.dat"
BOOK_FILE = ":addrbook.dat"
PATHS = ["users:1player", "TPW:Theme Park Data:users:2bob", "", "a", "x y:z", "A" * 150]
COMPLAINS = ["support@feral.co.uk", "", "zzz@z", "A", "m@m.com", "\xe9mail@x"]


def digits(r, n):
    return "".join(r.choice("0123456789") for _ in range(n))


def number_line(r):
    k = r.random()
    if k < 0.45:
        return str(r.randrange(0, 100000))
    if k < 0.55:
        return str(-r.randrange(1, 100000))
    if k < 0.60:
        return "+%d" % r.randrange(0, 1000)
    if k < 0.66:
        return r.choice(["0", "1", "-1", "2147483647", "2147483648", "-2147483648", "-2147483649", "4294967295", "4294967296",
                         "99999999999999999999", "-99999999999999999999", "000123", "00"])
    if k < 0.74:
        return r.choice([" 12", "  7 ", "\t5", "12abc", "abc", "1 2", "- 3", "+", "-", "1.5", "0x1f", "7,8", "12\r", "\x0b9"])
    if k < 0.78:
        return digits(r, r.choice([100, 126, 127, 128, 129, 200]))
    return str(r.randrange(1, 20))


def outbox_text(r):
    blocks = [[number_line(r) for _ in range(r.choice([0, 0, 1, 2, 3, 5]))] for _ in range(3)]
    lines = []
    for block in blocks:
        lines += block
        if r.random() < 0.9:
            lines.append("")
    if r.random() < 0.15:
        lines.insert(r.randrange(len(lines) + 1), "")
    if r.random() < 0.15:
        lines += [number_line(r) for _ in range(r.randrange(1, 4))]
    text = "\n".join(lines)
    if r.random() < 0.7:
        text += "\n"
    if r.random() < 0.1:
        text = text.replace("\n", "\r\n")
    return text.encode("latin-1")


def pre_outbox(r):
    if r.random() < 0.4:
        return {}
    pre = {}
    for field, _ in OUTBOX_STRINGS:
        if r.random() < 0.7:
            pre[field] = r.choice(["", "old", "x" * 40, "folder:name", "\xe9"])
    if r.random() < 0.6:
        pre[5] = [r.choice(["a@b", "yy", "zz"]) for _ in range(r.randrange(0, 4))]
    for field, _ in OUTBOX_NUMBERS:
        if r.random() < 0.7:
            pre[field] = [r.choice([0, 1, 7, 4000000000, MASK]) for _ in range(r.randrange(0, 4))]
    if r.random() < 0.5:
        pre[9] = r.choice([0, 25, 0xFFFF, 1234])
        pre[10] = r.choice([0, -1, 77, 0x7FFFFFFF])
    if r.random() < 0.4:
        pre["email"] = 1
    return pre


def pre_outbox_edge(index):
    pre = {0: "old", 1: "keep04", 2: "keep08", 3: "keep18", 4: "keep50", 5: ["q@q", "r@r"], 6: [1, 2], 7: [3], 8: [4, 5, 6],
           9: 25, 10: -5}
    return pre if index % 2 == 0 else {}


def outbox_case(path, text, present=True, q=None, pre=None, decoy=False):
    files = {}
    if text is not None:
        files[path + (OUTBOX_FILE if present else ":outbox:other.dat")] = text
    if decoy:
        files[path + ":outbox.dat"] = b"1\n"
        files[path + ":outbox"] = b"2\n"
    return dict(path=path, files=files, pre=pre if pre is not None else {}, q={"CreateDirectory": q if q is not None else [1]})


def outbox_edges():
    blocks = [b"1\n2\n3\n\n4\n5\n\n6\n7\n8\n\n", b"", b"\n", b"\n\n\n", b"1\n", b"1", b"1\n\n2\n\n3", b"1\n\n2\n\n3\n", b"\n1\n2\n",
              b"1\n\n\n2\n3\n", b"1\n2\n3\n\n", b"1\n2\n3\n4", b"5\n6\n\n7\n8\n\n9\n10\n\n11\n12\n\n13\n",
              b"-1\n+2\n 3\n4 \nabc\n\n99999999999\n-99999999999\n4294967295\n2147483648\n\n0\n", b"1\r\n2\r\n\r\n3\r\n",
              b"a" * 126 + b"\n\n", b"1" * 127 + b"\n2\n\n3\n", b"1" * 128 + b"\n2\n\n3\n", b"1" * 129 + b"\n2\n\n3\n",
              b"1" * 127, b"1" * 128, b"1\n" + b"2" * 300 + b"\n3\n\n4\n", b"1\n2\n\n" + b"3" * 128 + b"\n4\n\n5\n",
              b"1\x002\n3\n\n", b"\x00\n1\n\n", b"1\n2\n\n3\n4\n\n5\n6\n\n7\n8\n"]
    for i, text in enumerate(blocks):
        yield outbox_case(PATHS[i % len(PATHS)], text, pre=pre_outbox_edge(i))
    for q in ([1], [0], [-1], [2], [7]):
        yield outbox_case(PATHS[0], None, q=q, pre=pre_outbox_edge(0))
        yield outbox_case("TPW:users:3", None, q=q, pre=pre_outbox_edge(1))
    yield outbox_case(PATHS[0], b"1\n", present=False, pre=pre_outbox_edge(0))
    yield outbox_case(PATHS[0], b"1\n2\n", decoy=True, pre=pre_outbox_edge(1))
    yield outbox_case(PATHS[0], None, decoy=True, q=[1])
    yield outbox_case("", b"1\n2\n\n3\n\n4\n")


def outbox_random(r):
    path = r.choice(PATHS)
    present = r.random() < 0.85
    case = outbox_case(path, outbox_text(r) if present else None, q=[r.choice([0, 1, 1, -1, 5])], pre=pre_outbox(r),
                       decoy=r.random() < 0.1)
    return case


def address_line(r):
    k = r.random()
    if k < 0.5:
        return "%s@%s.com" % (r.choice(["amy", "bob", "Cat", "dan", "eve", "bob", "amy"]), r.choice(["x", "y", "mail", "z"]))
    if k < 0.58:
        return ""
    if k < 0.63:
        return r.choice([" ", "  amy@x.com", "amy@x.com ", "\t"])
    if k < 0.70:
        return r.choice(["\xe9mile@x.com", "\xff@x", "\x80", "z\xe9", "zz", "a\x7f", "\xe9", "e"])
    if k < 0.75:
        return r.choice(["support@feral.co.uk", "m@m.com", "zzz@z", "A"])
    if k < 0.80:
        return r.choice(["a" * 126, "a" * 127, "a" * 128, "a" * 200, "b" * 127])
    return "%s%d@q.org" % (r.choice("abcxyz"), r.randrange(0, 5))


def book_text(r):
    lines = [address_line(r) for _ in range(r.choice([0, 1, 2, 3, 5, 8, 12]))]
    text = "\n".join(lines)
    if lines and r.random() < 0.75:
        text += "\n"
    if r.random() < 0.1:
        text = text.replace("\n", "\r\n")
    return text.encode("latin-1")


def book_case(path, text, pre, complain):
    files = {}
    if text is not None:
        files[path + BOOK_FILE] = text
    return dict(path=path, files=files, pre=pre, complain=complain)


def book_edges():
    texts = [b"", b"\n", b"\n\n", b"a@b\n", b"a@b", b"b@b\na@b\nc@b\n", b"b@b\nb@b\nb@b\n", b"a@b\n\nb@b\n", b"support@feral.co.uk\n",
             b"A\na\nB\nb\n", b"\xe9\nz\n\xff\nZ\n\x80\n", b"a\xe9\na\x7f\naz\n", b"x" * 126 + b"\n", b"x" * 127 + b"\ny\n",
             b"x" * 128 + b"\ny\n", b"y\n" + b"x" * 300 + b"\nz\n", b"x" * 127, b"x" * 128, b"b\r\na\r\n", b"b\x00c\na\n",
             b"zzz@z\nzzz@z\nsupport@feral.co.uk\nsupport@feral.co.uk\n", b"m\nm\nn\nn\nm\n", b" \n  \n \n", b"\n" * 5 + b"q\n"]
    for i, text in enumerate(texts):
        for complain in (COMPLAINS[0], COMPLAINS[i % len(COMPLAINS)]):
            yield book_case(PATHS[i % len(PATHS)], text, [] if i % 3 else ["pre@z", "pre@a", "pre@a"], complain)
    for pre in ([], ["x@x"], ["support@feral.co.uk"], ["b", "a", "b", "a"], ["", ""]):
        for complain in COMPLAINS:
            yield book_case(PATHS[0], None, pre, complain)
    decoy = book_case(PATHS[1], None, ["k"], COMPLAINS[0])
    decoy["files"] = {PATHS[1] + ":addrbook.dat.bak": b"a\n", PATHS[1] + ":addrbook": b"b\n"}
    yield decoy


def book_random(r):
    path = r.choice(PATHS)
    present = r.random() < 0.9
    pre = [address_line(r) for _ in range(r.choice([0, 0, 1, 2, 4]))]
    return book_case(path, book_text(r) if present else None, pre, r.choice(COMPLAINS))


DTOR_FLAGS = [-1, 0, 1, 2, 0x7FFF, -0x8000, 5]


def dtor_case(flag, email, check_word, pre=None):
    pre = dict(pre if pre is not None else {})
    if email:
        pre["email"] = 1
    return dict(flag=flag, pre=pre, check_word=check_word)


def dtor_edges():
    for flag in DTOR_FLAGS:
        for email in (0, 1):
            for pre in ({}, pre_outbox_edge(0)):
                yield dtor_case(flag, email, 1, pre)


def dtor_random(r):
    return dtor_case(r.choice(DTOR_FLAGS), r.randrange(2), r.randrange(2), pre_outbox(r))


def ime_edges():
    for which in (0, 1, 2):
        for arg in (0, 1, 0x7FFFFFFF, -1):
            yield dict(which=which, arg=arg)


def ime_random(r):
    return dict(which=r.randrange(3), arg=r.choice([0, 1, -1, r.getrandbits(32) - 0x80000000, 0x160]))


TESTS = [
    FnTest("Outbox::Load", outbox_load_ppc, outbox_load_host, edge=outbox_edges, rnd=outbox_random),
    FnTest("AddressBook::Load", book_load_ppc, book_load_host, edge=book_edges, rnd=book_random),
    FnTest("Outbox::~Outbox", dtor_ppc, dtor_host, edge=dtor_edges, rnd=dtor_random),
    FnTest("IME::CIME::Activate, UnActivate, SetMaxCompositionLength", ime_ppc, ime_host, edge=ime_edges, rnd=ime_random),
]
