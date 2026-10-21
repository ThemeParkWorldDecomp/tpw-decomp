"""Unit v5L49: the weak library-glue callees of src/game/online/online.cpp against libtpw_unit_glue_callees.dylib, each run against the Mac's own
library code (not the main app: the glue leads out of it):

  online_parser_callee::WcsLen        wcslen, c_c++ 0x10033ff0 (glue 0x102389c8)
  wostream_callee::Wcslen             the same
  textobj_callee::WcsSet              NS_MacDoze::_wcsset, macdoze 0x1000392c (glue 0x102397f0)
  ignore_callee::SprintfWide          sprintf with "%ls", c_c++ 0x10027a78 (glue 0x10238578)

The launcher's generated stand-ins for these answered 0 and wrote nothing; every test has cases that fail on that. Compared: the answer,
and for WcsSet and SprintfWide the bytes of the text or buffer (a sprintf buffer starts as 0xa5 bytes, so what is written shows). Not
tested: a %ls text of 0x200 characters or more (the Mac's buffer is then unterminated and strlen reads on into its stack).
"""
import ctypes as C
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "c_c++ symbols shared"
SHIM_LIBRARY = "unit_glue_callees"
MASK = 0xFFFFFFFF

LIBC, MACDOZE = "c_c++ symbols shared", "macdoze symbols shared"
WCSLEN, SPRINTF = 0x10033FF0, 0x10027A78
WCSSET = 0x1000392C
BUFFER = 0x240   # bytes of the sprintf buffer compared (a %ls text here is below 0x200 characters)


class LibTest(FnTest):
    """A FnTest whose PowerPC side runs in another program's emulator."""

    def __init__(self, name, program, ppc, host, edge, rnd):
        super().__init__(name, ppc, host, edge=edge, rnd=rnd)
        self.program = program

    def run_case(self, runner, case):
        runner.use(self.program)
        return super().run_case(runner, case)


def wide_be(chars):
    return b"".join(struct.pack(">H", c) for c in chars) + b"\0\0"


def wide_host(chars):
    return (C.c_uint16 * (len(chars) + 1))(*chars, 0)


# ---- wcslen (both namespaces call the same function)

def chars_rnd(r, low_zero=True):
    n = r.choice([0, 1, 2, r.randrange(3, 40), r.randrange(40, 0x1FF)])
    out = []
    for _ in range(n):
        k = r.random()
        if k < 0.6:
            out.append(r.randrange(0x20, 0x7F))
        elif k < 0.8:
            out.append(r.randrange(0x80, 0x100))
        elif k < 0.9 or not low_zero:
            out.append(r.randrange(0x101, 0x10000) | 1)
        else:
            out.append(r.randrange(1, 0x100) << 8)   # a low byte of 0 (wcstombs stops there)
    return out


TEXTS = [[], [0x41], [0x41, 0x42, 0x43], [0x1234, 0xFFFF, 0x100], [0x100], [0x41, 0x200, 0x42], [0xFF, 0x80, 0x7F, 0x1],
         [0x68, 0x6F, 0x6D, 0x65, 0x5C, 0x2E], list(range(0x20, 0x7F)) * 3]


def wcslen_ppc(e, case):
    return {"answer": e.call(WCSLEN, [e.alloc(2 * len(case) + 2, wide_be(case))])}


def wcslen_host(entry):
    def host(lib, case):
        f = getattr(lib, entry)
        f.restype = C.c_uint32
        f.argtypes = [C.c_void_p]
        text = wide_host(case)   # (kept alive across the call)
        return {"answer": f(C.addressof(text))}
    return host


# ---- _wcsset: (text, c)

def wcsset_ppc(e, case):
    text, c = case
    p = e.alloc(2 * len(text) + 4, wide_be(text) + b"\x12\x34")
    answer = e.call(WCSSET, [p, c])
    return {"answer_is_text": int(answer == p), "text": list(struct.unpack(">%dH" % (len(text) + 2), e.read(p, 2 * len(text) + 4)))}


def wcsset_host(lib, case):
    text, c = case
    buf = (C.c_uint16 * (len(text) + 2))(*text, 0, 0x1234)
    lib.tpw_glue_wcsset.restype = None
    lib.tpw_glue_wcsset.argtypes = [C.c_void_p, C.c_uint16]
    lib.tpw_glue_wcsset(C.addressof(buf), c)
    return {"answer_is_text": 1, "text": list(buf)}   # (the host's answers nothing: every caller drops the answer)


def wcsset_edges():
    out = []
    for text in TEXTS:
        for c in (0x2A, 0x0, 0x1234, 0xFFFF):
            out.append((text, c))
    return out


def wcsset_rnd(r):
    return (chars_rnd(r), r.choice([0x2A, r.randrange(0x10000)]))


# ---- sprintf(buffer, "%ls", text): (text or None)

def sprintf_ppc(e, case):
    buf = e.alloc(BUFFER, b"\xa5" * BUFFER)
    fmt = e.alloc(4, b"%ls\0")
    text = 0 if case is None else e.alloc(2 * len(case) + 2, wide_be(case))
    e.call(SPRINTF, [buf, fmt, text])
    return {"buffer": e.read(buf, BUFFER)}


def sprintf_host(lib, case):
    buf = C.create_string_buffer(b"\xa5" * BUFFER, BUFFER)
    lib.tpw_glue_sprintf_wide.restype = None
    lib.tpw_glue_sprintf_wide.argtypes = [C.c_void_p, C.c_char_p, C.c_void_p]
    text = None if case is None else wide_host(case)
    lib.tpw_glue_sprintf_wide(C.addressof(buf), b"%ls", None if text is None else C.addressof(text))
    return {"buffer": buf.raw}


def sprintf_edges():
    return [None] + TEXTS + [list(range(1, 0x1FF)), [0x41] * 0x1FF, [0x141, 0x242, 0x100, 0x43]]


def sprintf_rnd(r):
    return chars_rnd(r)


TESTS = [
    LibTest("online_parser_callee::WcsLen (wcslen)", LIBC, wcslen_ppc, wcslen_host("tpw_glue_parser_wcslen"), lambda: TEXTS,
            lambda r: chars_rnd(r, low_zero=False)),
    LibTest("wostream_callee::Wcslen (wcslen)", LIBC, wcslen_ppc, wcslen_host("tpw_glue_ostream_wcslen"), lambda: TEXTS,
            lambda r: chars_rnd(r, low_zero=False)),
    LibTest("textobj_callee::WcsSet (NS_MacDoze::_wcsset)", MACDOZE, wcsset_ppc, wcsset_host, wcsset_edges, wcsset_rnd),
    LibTest("ignore_callee::SprintfWide (sprintf %ls)", LIBC, sprintf_ppc, sprintf_host, sprintf_edges, sprintf_rnd),
]
