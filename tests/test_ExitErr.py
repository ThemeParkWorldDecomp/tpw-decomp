"""Differential tests for the exit error report (unit w97), against libtpw_unit_ExitErr.dylib (tests/unit_ExitErr.cpp):

  src/ExitErr_report.cpp   CExitError::ReportAnyError (0x1014b6c8), HandleEngineErrors (0x1014bc38),
                           ConvertEngineErrors (0x1014bd04)

A case is the object gExitError (error, type, value, the 0x50 text bytes), the text system's mInitialised word, the text the UI
string table's FormatString writes, and the records the engine's error stack answers (level, code, in the order Pop gives them).
After the call the test compares the answer, the object (every word and all 0x50 text bytes) and the calls the fakes recorded:
FormatString (table, size, string id), Pop (once per record and once more for NULL) and MessageBoxW (owner, flags, the length of
the message, a hash of the whole 256-character buffer, a hash of the title, and both texts). swprintf and wcslen are the platform's
on the host and a stub that formats "%ls (%hs)" on the PowerPC side (any other format or argument shows as a changed message).

The original's message buffer is uninitialised stack (a type that gets no string shows what was there); the host zeroes it, so the
PowerPC side's stack below the call is zeroed before each case. A text that fills all 0x50 bytes without a terminator is not tested:
the original's swprintf would read on into the next global.
"""
import ctypes as C
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_ExitErr"

REPORT, HANDLE, CONVERT = 0x1014B6C8, 0x1014BC38, 0x1014BD04
EXIT_ERROR = 0x1037F1C0       # gExitError: error +0, type +4, value +8, text +0xc (0x50 bytes)
TEXT = 0x1037FF18             # gText: mInitialised +0, the string table pointer +0x558
GEI_TOC_SLOT = 0x102614EC     # the TOC slot of the import gei_ErrorHandler
MASK = 0xFFFFFFFF
BASE = 0xB0754000
LABEL_TABLE = 5
TITLE = "Theme Park World"
OP = dict(report=0, handle=1, convert=2)


def s32(v):
    return T.s32(v & MASK)


def fnv(units):
    """The C++ side's Hash: FNV-1a over 16-bit characters, low byte first."""
    h = 2166136261
    for u in units:
        h = ((h ^ (u & 0xFF)) * 16777619) & MASK
        h = ((h ^ (u >> 8)) * 16777619) & MASK
    return h


def units(e, a, count):
    return list(struct.unpack(">%dH" % count, e.read(a, 2 * count)))


def wide_bytes(text):
    return b"".join(struct.pack(">H", ord(ch)) for ch in text) + b"\0\0"


def text_bytes(text, size=0x50):
    """The 0x50 bytes of mText for a text (latin-1), zero-padded."""
    return list(text.encode("latin-1")[:size].ljust(size, b"\0"))


# ======================================================================= the PowerPC side


def ppc(e, case):
    op, init, error, typ, value, text, fmt, records, code = case
    e.cl = CL.PpcLog({})
    table = e.alloc(64)
    e.cl.labels[table] = LABEL_TABLE
    gei = e.u32(GEI_TOC_SLOT)
    block = e.alloc(8 * max(len(records), 1))
    for i, (level, rcode) in enumerate(records):
        e.write(block + 8 * i, struct.pack(">II", level & MASK, rcode & MASK))
    state = dict(next=0)

    def format_string(e):
        this, out, size, sid = e.r(3), e.r(4), e.r(5), e.r(6)
        e.cl.rec("FormatString", (e.cl.lab(this), size, sid))
        if size == 0:
            e.set_r(3, 0)
            return
        kept = fmt[:0x1FF][:size - 1]
        e.write(out, wide_bytes(kept))
        e.set_r(3, len(kept))

    def pop(e):
        e.cl.rec("Pop", (int(e.r(3) == gei),))
        if state["next"] < len(records):
            e.set_r(3, block + 8 * state["next"])
            state["next"] += 1
        else:
            e.set_r(3, 0)

    def swprintf(e):
        out, size, fmt_at, first, second = e.r(3), e.r(4), e.r(5), e.r(6), e.r(7)
        format_text = ""
        a = fmt_at
        while True:
            ch = struct.unpack(">H", e.read(a, 2))[0]
            if ch == 0:
                break
            format_text += chr(ch)
            a += 2
        if format_text != "%ls (%hs)" or first != out:
            result = "<<swprintf called differently>>"
        else:
            wide = ""
            a = first
            while True:
                ch = struct.unpack(">H", e.read(a, 2))[0]
                if ch == 0:
                    break
                wide += chr(ch)
                a += 2
            narrow = bytearray()
            a = second
            while e.read(a, 1) != b"\0":
                narrow += e.read(a, 1)
                a += 1
            result = wide + " (" + "".join(chr(b) for b in narrow) + ")"
        if size:
            e.write(out, wide_bytes(result[:size - 1]))
        e.set_r(3, T.u32(len(result) if size and len(result) <= size - 1 else -1))

    def message_box(e):
        owner, text_at, title_at, flags = e.r(3), e.r(4), e.r(5), e.r(6)
        buffer = units(e, text_at, 0x100)
        length = buffer.index(0) if 0 in buffer else 0x100
        title = units(e, title_at, 17)
        e.cl.rec("MessageBoxW", (e.cl.lab(owner), flags, length, fnv(buffer), fnv(title)),
                 (CL.wide_text(e, text_at), CL.wide_text(e, title_at)))
        e.set_r(3, 1)

    st = CL.Stubs()
    st.imp("FormatString__8TbITableCFPwUiUle", format_string)
    st.imp("Pop__13CErrorHandlerFv", pop)
    st.imp("swprintf", swprintf)
    st.imp("MessageBoxW__10NS_MacDozeFP8GrafPortPCwPCwUi", message_box)
    saved_obj = e.read(EXIT_ERROR, 0x5C)
    saved_text = e.read(TEXT, 4), e.read(TEXT + 0x558, 4)
    old = st.activate(e)
    try:
        e.write(EXIT_ERROR, struct.pack(">III", error & MASK, typ & MASK, value & MASK) + bytes(text))
        e.write(TEXT, struct.pack(">I", init & MASK))
        e.write(TEXT + 0x558, struct.pack(">I", table))
        # the buffers the code leaves uninitialised: the stack below the call is zero, as the host's buffer is
        sp = T.ppcemu.STACK_BASE + T.ppcemu.STACK_SIZE - 0x1000 - 0x400
        e.write(sp - 0x1000, b"\0" * 0x1000)
        if op == OP["convert"]:
            answer = T.call_ppc(e, CONVERT, [EXIT_ERROR, code & MASK])[0]
        else:
            T.call_ppc(e, REPORT if op == OP["report"] else HANDLE, [EXIT_ERROR])
            answer = 0
        words = struct.unpack(">III", e.read(EXIT_ERROR, 12))
        result = [s32(answer), s32(words[0]), s32(words[1]), s32(words[2]), list(e.read(EXIT_ERROR + 12, 0x50))]
        return result, e.cl.log
    finally:
        CL.Stubs.deactivate(e, old)
        e.write(EXIT_ERROR, saved_obj)
        e.write(TEXT, saved_text[0])
        e.write(TEXT + 0x558, saved_text[1])


# ======================================================================= the host side


def host(lib, case):
    op, init, error, typ, value, text, fmt, records, code = case
    CL.host_begin(lib, {})
    fn = lib.tpw_ee_run
    fn.argtypes = [C.c_int32, C.c_uint32, C.c_int32, C.c_int32, C.c_uint32, C.c_char_p, C.c_char_p, C.POINTER(C.c_int32),
                   C.c_int32, C.c_int32, C.POINTER(C.c_int32), C.POINTER(C.c_uint8)]
    fn.restype = C.c_int32
    pairs = (C.c_int32 * (2 * max(len(records), 1)))()
    for i, (level, rcode) in enumerate(records):
        pairs[2 * i] = s32(level)
        pairs[2 * i + 1] = s32(rcode)
    out_ints = (C.c_int32 * 3)()
    out_text = (C.c_uint8 * 0x50)()
    wide = fmt.encode("utf-16-le") + b"\0\0"
    answer = fn(op, init & MASK, s32(error), s32(typ), value & MASK, bytes(text).ljust(0x50, b"\0"), wide, pairs, len(records),
                s32(code), out_ints, out_text)
    log = CL.host_end(lib)
    return [answer, out_ints[0], out_ints[1], out_ints[2], list(out_text)], log


# ======================================================================= the cases

SHORT = "disk full"
LONG = "x" * 0x4F
TEXTS = [b"", SHORT.encode(), LONG.encode(), b"\x80abc", b"a\xe9\xff!", b"\xffz", b"\x01", b"(%s %ls %d)"]
FORMATS = ["", "Out of memory", "A" * 0xFF, "B" * 0xFE, "C" * 0x100, "D" * 0x180, "é中€ text", "x (y)", "%s %d"]
STATIC_CODES = [BASE + n for n in range(0x17)] + [BASE + 0x18, BASE + 0x100, BASE + 0x101, BASE + 0x102]


def case(op, typ=0, error=1, init=1, value=0, text=b"", fmt="message", records=(), code=0):
    return [op, init, error, typ, value, bytes(text).ljust(0x50, b"\0"), fmt, list(records), code]


def report_edges():
    types = list(range(0, 0x2D)) + [0x7FFFFFFF, 0x80000000, MASK, 0xFFFFFFFE, 0x100, 0x2B2B2B2B]
    for t in types:
        for text in (b"", SHORT.encode()):
            yield case(OP["report"], t, text=text, fmt="msg %d" % t)
    for t in (0, 1, 12, 13, 14, 0x2A, 0x2B):
        for error, init in ((0, 0), (0, 1), (1, 0), (2, 1), (1, 0x100), (MASK, 1), (0x100, 0x80000000), (0x1000000, 1)):
            yield case(OP["report"], t, error=error, init=init, text=SHORT.encode(), records=[(3, BASE)])
    for t in (0, 5, 13, 0x2A, 0x2B, 0x7FFFFFFF):
        for text in TEXTS:
            for fmt in FORMATS:
                yield case(OP["report"], t, text=text, fmt=fmt, value=0x1234)
    # type 12 asks the engine: records of both levels, nothing pending, a text that stays
    for records in ([], [(3, BASE)], [(2, BASE), (3, BASE + 1)], [(3, BASE + 2), (3, BASE + 3)], [(3, BASE + 0x100), (2, BASE + 4)],
                    [(2, BASE + 5)], [(4, BASE + 6), (0, BASE + 7), (-1, BASE + 8)]):
        for text in (b"", SHORT.encode()):
            yield case(OP["report"], 12, text=text, records=records, value=77, fmt="Unexpected")


def handle_edges():
    base_records = [[], [(3, BASE)], [(2, BASE)], [(3, BASE + 1), (2, BASE + 2)], [(2, BASE + 1), (3, BASE + 2)],
                    [(3, BASE + 1), (3, BASE + 2)], [(3, BASE + 2), (3, BASE + 1)], [(1, BASE), (0, BASE), (4, BASE), (-1, BASE)],
                    [(3, 0)], [(3, -1)], [(3, 0x7FFFFFFF)], [(3, -0x80000000)], [(0x100000003, BASE), (3, BASE + 3)]]
    for records in base_records:
        for text in (b"", SHORT.encode()):
            for init in (1, 0):
                yield case(OP["handle"], 5, error=0, init=init, value=0x55, text=text, records=records, fmt="Crash")
    for code in STATIC_CODES + [BASE - 1, BASE + 0x17, BASE + 0x19, BASE + 0xFF, BASE + 0x103, 0]:
        yield case(OP["handle"], 1, error=7, value=9, records=[(3, code)], fmt="engine")
    for n in (1, 3, 63, 64, 65, 70):
        yield case(OP["handle"], 1, records=[(2 + (i % 2), BASE + (i % 0x17)) for i in range(n)], fmt="many")
    yield case(OP["handle"], 13, error=1, init=1, value=3, text=LONG.encode(), records=[(3, BASE + 5)], fmt="F" * 0xFF)


def convert_edges():
    for code in range(BASE - 8, BASE + 0x110):
        yield case(OP["convert"], code=code)
    for code in (0, 1, -1, -0x80000000, 0x7FFFFFFF, 0x80000000, MASK, BASE - 0x10000, BASE + 0x10000, 0xB0753FFF, 0xB0750000,
                 0xB0760000, 0xB075FFFF, 0x30754000, 0x30754018, 0xB0754000 + 0x100000000 - 0x100000000, 0x7075400E):
        yield case(OP["convert"], code=code)


def random_text(r):
    n = r.choice([0, 0, 1, 5, 20, 0x4F])
    return bytes(r.choice([r.randrange(1, 0x80), r.randrange(1, 0x100)]) for _ in range(n))


def random_format(r):
    n = r.choice([0, 1, 12, 100, 0xFE, 0xFF, 0x100, 0x150])
    return "".join(chr(r.choice([r.randrange(0x20, 0x7F), r.randrange(0x80, 0x3000)])) for _ in range(n))


def random_records(r):
    n = r.choice([0, 0, 1, 2, 3, 5, 64, 66])
    return [(r.choice([3, 3, 2, 2, 0, 1, 4, r.getrandbits(32)]), r.choice([BASE + r.randrange(0x20), BASE + r.choice([0x100, 0x101, 0x102]),
                                                                             r.getrandbits(32)])) for _ in range(n)]


def rnd_report(r):
    typ = r.choice([r.randrange(0x2B), r.randrange(0x2B), r.randrange(0x2B), 12, 13, r.randrange(0x2B, 0x40), r.getrandbits(32)])
    return case(OP["report"], typ, r.choice([1, 1, 1, 0, 2, r.getrandbits(32)]), r.choice([1, 1, 1, 0, r.getrandbits(32)]),
                r.getrandbits(32), random_text(r), random_format(r), random_records(r))


def rnd_handle(r):
    return case(OP["handle"], r.randrange(0x2D), r.choice([0, 1, r.getrandbits(32)]), r.choice([1, 1, 0, r.getrandbits(32)]),
                r.getrandbits(32), random_text(r), random_format(r), random_records(r))


def rnd_convert(r):
    code = r.choice([BASE + r.randrange(-0x10, 0x120), BASE + r.randrange(-0x10, 0x120), r.getrandbits(32),
                     BASE + r.choice([-1, 0, 0x16, 0x17, 0x18, 0x19, 0xFF, 0x100, 0x101, 0x102, 0x103, 0x7FFFFFFF])])
    return case(OP["convert"], r.randrange(0x2D), r.choice([0, 1]), 1, 0, b"", "", [], code & MASK)


TESTS = [
    FnTest("CExitError::ConvertEngineErrors", ppc, host, edge=convert_edges, rnd=rnd_convert),
    FnTest("CExitError::HandleEngineErrors", ppc, host, edge=handle_edges, rnd=rnd_handle),
    FnTest("CExitError::ReportAnyError", ppc, host, edge=report_edges, rnd=rnd_report),
]
