"""Differential test for src/game/online/lobby_parser.cpp (CLBLobbyGlobals::ParseLobbyDescriptor and
LB_ReadTbStringUntilNext), against libtpw_unit_LobbyParser.dylib.

A case is the text of a lobby description file. Both sides parse it into an object whose
memory starts as 0xa5 and the test compares the object's 26 numbers bit for bit and the
file and memory calls. The C library's atof, atoi and strncmp are the PowerPC side's
stand-ins (Python); files in the cases keep to plain decimal numbers, which every atof
reads the same. A matched line always has its brackets and commas: the original reads on
without limit when one is missing.
"""
import ctypes as C
import random
import re
import struct

import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_LobbyParser"

PARSE, READ = 0x100B55B8, 0x100B5C78
GLOBALS_VTABLE = 0x1029E3F0
FOLDER, NAME = 0x102E4978, 0x102E4958
FILE = 0x7C0FD400
NUMBER = re.compile(r"\s*[+-]?(\d+\.?\d*([eE][+-]?\d+)?|\.\d+([eE][+-]?\d+)?)")
INTEGER = re.compile(r"\s*[+-]?\d+")


def c_atof(text):
    found = NUMBER.match(text)
    return float(found.group(0)) if found else 0.0


def c_atoi(text):
    found = INTEGER.match(text)
    return int(found.group(0)) if found else 0


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update((PARSE, READ))
    FS.string_stubs(S)
    S.imp("__apl__26TbDynamicStringTemplate<c>FRCc",
          lambda e: FS.s_write(e, e.r(3), FS.s_read(e, e.r(3), False) + e.read(e.r(4), 1).decode("latin-1"), False))
    text = lambda e, a: e.read(a, 600).split(b"\0")[0]   # noqa: E731

    def strncmp(e):
        a, b = text(e, e.r(3))[:e.r(5)], text(e, e.r(4))[:e.r(5)]
        e.set_r(3, T.u32((a > b) - (a < b)))

    S.imp("strncmp", strncmp)
    S.imp("atof", lambda e: e.set_f1(c_atof(text(e, e.r(3)).decode("latin-1"))))
    S.imp("atoi", lambda e: e.set_r(3, T.u32(max(-0x80000000, min(0x7FFFFFFF, c_atoi(text(e, e.r(3)).decode("latin-1")))))))

    def alloc(e):
        size = e.r(3)
        block = e.alloc(size + 1)
        e.cl.rec("LbMem_Alloc", (size, e.cl.newlab(block)))
        e.set_r(3, block)

    def free(e):
        e.cl.rec("LbMem_Free", (e.cl.lab(e.r(3)),))
        e.cl.forget(e.r(3))

    S.imp("LbMem_Alloc__FUi", alloc)
    S.imp("LbMem_Free__FPv", free)
    S.internal(0x10003FD4, lambda e: (e.cl.rec("TPFile_Open", (), (e.cstr(e.r(3))[:159], e.cstr(e.r(4))[:159])), e.set_r(3, FILE)))
    S.internal(0x100043EC, CL.recorder("TPFile_Seek", "Luu", lambda e, L: 0))
    S.internal(0x100044E8, CL.recorder("TPFile_Size", "L", lambda e, L: len(e.lp_text)))

    def read(e):
        e.cl.rec("TPFile_Read", (e.cl.lab(e.r(3)), e.r(4), e.r(5), e.cl.lab(e.r(6))))
        if e.lp_text:
            e.write(e.r(3), e.lp_text)
        e.set_r(3, e.r(5))

    S.internal(0x10004260, read)
    S.internal(0x10004354, CL.recorder("TPFile_Close", "L", lambda e, L: 0))
    return S


STUBS = None


def setup(runner):
    global STUBS
    e = runner.emu
    T.install_std_stubs(e)
    if "c" not in FS.VT:
        FS.make_vtables(e)
    STUBS = build_stubs(e)


def parse_ppc(e, case):
    saved = STUBS.activate(e)
    try:
        e.lp_text = case["text"]
        L = e.cl = CL.PpcLog({FILE: 1})
        FS.s_write(e, FOLDER, "data:lobby:", False)
        FS.s_write(e, NAME, "lobby.txt", False)
        obj = e.alloc(0x6C, struct.pack(">I", GLOBALS_VTABLE) + b"\xa5" * 0x68)
        T.call_ppc(e, PARSE, [obj])
        return dict(numbers=list(struct.unpack(">26I", e.read(obj + 4, 0x68))), log=L.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def parse_host(lib, case):
    file_at = FS.F(lib, "tpw_lp_file", C.c_void_p)
    CL.host_begin(lib, {1: file_at()})
    out = (C.c_uint32 * 26)()
    try:
        FS.F(lib, "tpw_lp_parse", None, C.c_char_p, C.c_uint32, C.POINTER(C.c_uint32))(case["text"], len(case["text"]), out)
    finally:
        log = CL.host_end(lib)
    return dict(numbers=list(out), log=log)


def read_ppc(e, case):
    saved = STUBS.activate(e)
    try:
        e.cl = CL.PpcLog({})
        text = case["text"].encode("latin-1")
        base = e.alloc(len(text) + 1, text + b"\0")
        cursor = e.alloc(4, struct.pack(">I", base + case["start"]))
        result = e.alloc(0x14)
        T.call_ppc(e, READ, [result, cursor, ord(case["delimiter"])])
        return [FS.s_read(e, result, False), e.u32(cursor) - base]
    finally:
        CL.Stubs.deactivate(e, saved)


def read_host(lib, case):
    text = C.create_string_buffer(case["text"].encode("latin-1"))
    out = C.create_string_buffer(600)
    end = FS.F(lib, "tpw_lp_read", C.c_int32, C.c_char_p, C.c_int32, C.c_int32, C.c_char_p)(text, case["start"], ord(case["delimiter"]), out)
    return [out.value.decode("latin-1"), end]


REAL = ("ISLANDFOV(100)\r\nSPINSPEED(0.02)\r\nSPINRADIUS(70)\r\nVERTICALOFFSET(20)\r\nGLOBERADIUSOUT(475)\r\nGLOBERADIUSIN(200)\r\n"
        + "".join("ISLANDCAMERAPOSITION(%d,%d,%d);\r\n" % (i, x, y) for i, (x, y) in enumerate(
            ((400, 400), (600, 400), (600, 600), (400, 600), (400, 200), (800, 400), (600, 800), (200, 600), (600, 200), (400, 800)))))
KEYWORDS = ["ISLANDFOV(", "SPINSPEED(", "SPINRADIUS(", "VERTICALOFFSET(", "GLOBERADIUSOUT(", "GLOBERADIUSIN("]
VALUES = ["100", "0.02", "70", "-3.5", ".5", "1e2", "0", "", "abc", " 12", "12abc", "475.25", "-0", "+7", "1.", "007", "3.0e-2",
          "99999999", "0.1"]


def number(r):
    return r.choice(VALUES)


def line(r):
    k = r.random()
    if k < 0.5:
        return r.choice(KEYWORDS) + number(r) + ")" + r.choice(["", ";", " # note"])
    if k < 0.8:
        return "ISLANDCAMERAPOSITION(%s,%s,%s)%s" % (r.choice(["0", "1", "5", "9", "10", "-1", "3", " 2", "x", "99", "4.7"]),
                                                    number(r), number(r), r.choice(["", ";"]))
    return r.choice(["", "ISLAND(1,2)", "islandfov(3)", "# comment", "SPINSPEEDX(4)", " ISLANDFOV(9)", "ISLANDFOV", "x(", "SPIN",
                     "FLYINGMESH(a,b)", "GLOBERADIUS(5)"])


def parse_case(r):
    ending = r.choice(["\r\n", "\r\n", "\n"])
    lines = [line(r) for _ in range(r.randrange(0, 14))]
    text = ending.join(lines) + (ending if r.random() < 0.9 or not lines else "")
    return dict(text=text.encode("latin-1"))


def parse_edge():
    r = random.Random(21)
    yield dict(text=REAL.encode("latin-1"))
    yield dict(text=REAL.replace("\r\n", "\n").encode("latin-1"))
    yield dict(text=b"")
    yield dict(text=b"\n")
    yield dict(text=b"\n\n\nISLANDFOV(1)\n\n")
    yield dict(text=b"ISLANDFOV(7)")                    # no line feed at the end: the zero after the block ends the line
    yield dict(text=b"ISLANDFOV(5)\rSPINSPEED(3)\rSPINRADIUS(2)\r")      # carriage returns alone end no line
    yield dict(text=b"SPINSPEED(3)\rISLANDFOV(5)\r\nSPINRADIUS(2)\rVERTICALOFFSET(4)\n")
    for keyword in KEYWORDS:
        for value in VALUES:
            yield dict(text=("%s%s)\r\n" % (keyword, value)).encode("latin-1"))
        yield dict(text=(keyword[:-1] + "\n" + keyword.lower() + "5)\n" + "X" + keyword + "5)\n").encode("latin-1"))
    for index in ("0", "9", "10", "-1", "11", "5", "", "a", " 3", "2.9"):
        yield dict(text=("ISLANDCAMERAPOSITION(%s,12.5,-7)\n" % index).encode("latin-1"))
    yield dict(text=b"ISLANDCAMERAPOSITION(1,2,3)\nISLANDCAMERAPOSITION(1,4,5)\nISLANDFOV(1)\nISLANDFOV(2)\n")
    for _ in range(80):
        yield parse_case(r)


def read_case(r):
    alphabet = "abc,)( 1.5-;x"
    text = "".join(r.choice(alphabet) for _ in range(r.randrange(1, 30)))
    delimiter = r.choice(",)(;")
    text += delimiter                                   # there is always one to find
    return dict(text=text, start=r.randrange(len(text)), delimiter=delimiter)


def read_edge():
    for text, start, delimiter in ((")", 0, ")"), ("abc)", 0, ")"), ("abc)", 2, ")"), ("a,b,c)", 0, ","), ("a,b,c)", 2, ","),
                                   ("a,b,c)", 4, ")"), (",,", 0, ","), ("12.5);", 0, ")"), ("x" * 200 + ")", 0, ")")):
        yield dict(text=text, start=start, delimiter=delimiter)


TESTS = [
    FnTest("LB_ReadTbStringUntilNext", read_ppc, read_host, edge=read_edge, rnd=read_case),
    FnTest("CLBLobbyGlobals::ParseLobbyDescriptor", parse_ppc, parse_host, edge=parse_edge, rnd=parse_case),
]
