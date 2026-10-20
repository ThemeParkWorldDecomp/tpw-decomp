"""Unit w183 (main app), against libtpw_unit_GoldenTicketTell.dylib (tests/unit_GoldenTicketTell.cpp):

  src/GoldenTicket_tell.cpp  CGoldenTicketControl::TellTheWorld, the local 0x100fcbb0, global 0x100fcf1c and secret 0x100fd4f4 overloads

They run as the original on the PowerPC side (CMsgAdvisor's constructor 0x1015190c too) and as src/ on the host. Every other callee is a fake that
records the same event on both sides: CTRACE (level, the integer arguments the format takes, the text), the advisor request's two constructors,
TellIfFirst, the message centre (the nothrow operator new of 0x1d0 bytes, the array construction, PostMessage with the request's fields).

The world of a case: gMessageCentre (NULL or a block), the nothrow new's answer queue (NULL or a block), and the two arguments (type, won). What is
compared: the event log in order (every fake's receiver as a label, every argument) and the centre pointer after the call (as a label).
Not tested: the host's CMsgAdvisor holds its request at 0x0c (the Mac's 0x08): the fakes read both at their own offsets.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_GoldenTicketTell"
MASK = 0xFFFFFFFF
POISON = b"\xa5"

FN = dict(local=0x100FCBB0, glob=0x100FCF1C, secret=0x100FD4F4)
WHICH = dict(local=0, glob=1, secret=2)
GCENTRE = 0x1037F6C4
MSG_VTABLE = 0x102A09A4
CENTRE_CTOR, CENTRE_DTOR = 0x102657E8, 0x10267288
MSG_CTOR = 0x1015190C
L_CONTROL, L_CENTRE = 2, 6


def s32(v):
    return T.s32(v)


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(set(FN.values()) | {MSG_CTOR})


def stub_trace(e):
    """CTRACE: the level, the integer arguments the format takes (r5, r6) and the text, as the host fake records them."""
    text = e.cstr(e.r(4))[:159]
    first = second = 0
    if text.startswith("Won a"):
        first, second = s32(e.r(5)), s32(e.r(6))
    elif "%d" in text:
        first = s32(e.r(5))
    e.cl.rec("CTRACE", [e.r(3) & MASK, first, second], [text])


def tell_if_first(e):
    e.cl.rec("TellIfFirst", [e.cl.lab(e.r(3))])


def request_ctor(e):
    dest = e.r(3)
    fields = [s32(e.r(4)), e.r(5) >> 16, e.r(6) & MASK, s32(e.r(7)), e.r(8) & 0xFF, s32(e.r(9))]
    e.cl.rec("RequestCtor", fields)
    e.write(dest, struct.pack(">H", e.r(5) >> 16))
    e.write(dest + 4, struct.pack(">IiiiBBB", e.r(6) & MASK, s32(e.r(7)), s32(e.r(4)), s32(e.r(9)), 1, e.r(8) & 0xFF, 0))
    # r3 stays the destination: the constructor answers its object


def request_copy(e):
    dest, src = e.r(3), e.r(4)
    ident = struct.unpack(">H", e.read(src, 2))[0]
    sec, resp, msg, score, valid, override, overridden = struct.unpack(">IiiiBBB", e.read(src + 4, 0x13))
    e.cl.rec("RequestCopy", [ident, sec, resp, msg, score, valid, override, overridden])
    e.write(dest, struct.pack(">H", ident))
    e.write(dest + 4, struct.pack(">IiiiBBB", sec, resp, msg, score, valid, override, overridden))


def post(e):
    L, msg = e.cl, e.r(4)
    vt = e.u32(msg)
    ident = struct.unpack(">H", e.read(msg + 8, 2))[0]
    sec, resp, rmsg, score, valid, override, overridden = struct.unpack(">IiiiBBB", e.read(msg + 12, 0x13))
    L.rec("PostMessage", [L.lab(e.r(3)), 5 if vt == MSG_VTABLE else -1, e.u32(msg + 4), ident, sec, resp, rmsg, score, valid, override, overridden])
    e.set_r(3, 1)


def op_new(e):
    size = e.r(3)
    fail = bool(e.cl.ret("newnull", 0)) if size == 0x1D0 else False
    if size == 0x1D0:
        e.cl.rec("new", [size, int(fail)])
    e.set_r(3, 0 if fail else e.alloc(size, POISON * size))


STUBS.internal(0x1006FCBC, stub_trace)
STUBS.internal(0x100FD7F0, tell_if_first)
STUBS.internal(0x1000DEB4, request_ctor)
STUBS.internal(0x1000DE30, request_copy)
STUBS.internal(0x1015256C, post)
STUBS.imp("__nw__FUl", op_new)


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def construct(emu, call):
        p, ctor, dtor, size, count = (emu.r(i) for i in range(3, 8))
        if ctor == CENTRE_CTOR:
            emu.cl.rec("CMessageCentre", [emu.cl.newlab(p), int(ctor == CENTRE_CTOR), int(dtor == CENTRE_DTOR), size, count])

    T.stub_cb(e, "__construct_array", construct)


# ---------------------------------------------------------------------------------------------------------------- the two sides

def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_gtt_addr.restype, lib.tpw_gtt_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_gtt_prepare.argtypes = [C.c_uint64]
    lib.tpw_gtt_centre.restype = C.c_int64
    lib.tpw_gtt_tell.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.bound = True


def ppc(e, c):
    L = e.cl = CL.PpcLog({}, {"newnull": c["newnull"]})
    control = e.alloc(0x40, POISON * 0x40)
    centre = e.alloc(0x1D0, POISON * 0x1D0)
    L.labels.update({control: L_CONTROL, centre: L_CENTRE})
    w32(e, GCENTRE, centre if c["centre"] else 0)
    saved = STUBS.activate(e)
    try:
        T.call_ppc(e, FN[c["fn"]], [control, c["type"] & MASK, c["won"] & MASK])
        return {"log": L.log, "centre": L.lab(e.u32(GCENTRE))}
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, c):
    out = {}
    bind(lib)
    control = lib.tpw_gtt_addr(b"control")
    centre = lib.tpw_gtt_addr(b"centre")
    CL.host_begin(lib, {L_CONTROL: control, L_CENTRE: centre}, {"newnull": c["newnull"]})
    lib.tpw_gtt_prepare(centre if c["centre"] else 0)
    lib.tpw_gtt_tell(WHICH[c["fn"]], s32(c["type"]), s32(c["won"]))
    out["centre"] = lib.tpw_gtt_centre()
    out["log"] = CL.host_end(lib)
    return out


# ---------------------------------------------------------------------------------------------------------------- the cases

WONS = [-1, 0, 1, 2, 3, 4, 5, 0x7FFFFFFF, -0x80000000]
TYPES = [-1, 0, 1, 2, 3, 4, 5, 0x7FFFFFFF, -0x80000000]


def case(fn, won, type_, centre, newnull):
    return {"fn": fn, "won": won, "type": type_, "centre": centre, "newnull": newnull}


def edges(fn):
    def make():
        out = []
        for won in WONS:
            for type_ in TYPES:
                for centre, newnull in ((1, [0]), (0, [0]), (0, [1])):
                    out.append(case(fn, won, type_, centre, newnull))
        return out
    return make


def rnd(fn):
    def make(r):
        def pick(values):
            return r.choice(values) if r.random() < 0.7 else r.getrandbits(32) - (1 << 31)
        return case(fn, pick(WONS), pick(TYPES), r.choice([0, 1]), [r.choice([0, 1])])
    return make


TESTS = [
    FnTest("CGoldenTicketControl::TellTheWorld (local)", ppc, host, edge=edges("local"), rnd=rnd("local")),
    FnTest("CGoldenTicketControl::TellTheWorld (global)", ppc, host, edge=edges("glob"), rnd=rnd("glob")),
    FnTest("CGoldenTicketControl::TellTheWorld (secret)", ppc, host, edge=edges("secret"), rnd=rnd("secret")),
]
