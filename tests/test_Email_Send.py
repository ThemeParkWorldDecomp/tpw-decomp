"""Unit v5129, the e-mail object, against libtpw_unit_Email_Send.dylib (tests/unit_Email_Send.cpp), main app ("SimTheme Park"),
src/Email_Send.cpp:
  Email::~Email 0x1020248c, Email::Reset 0x10202588, Email::AttachData 0x10202950, Email::Send 0x10202ee4

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (operator new and delete, Mail's constructor and Connect, Attachment's constructor and destructor, the string and list members,
WSACleanup, the pointer glue) are fakes on both sides that record their arguments; strlen runs for real on the PowerPC side.

The world of a case: the 0x4c byte e-mail object (random words, the attachment and mail pointers either 0 or a block made before the call),
three strings for the arguments, the answer queues for operator new (NULL or a block) and Mail::Connect. What is compared: the answer, the
event log and every byte of the object (pointers to blocks as labels: none, the block made before, the Nth block operator new gave).

No fused operations. Not tested: nothing is skipped; the object pointer is NULL only for the destructor (the only one that tests it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Email_Send"
MASK = 0xFFFFFFFF

FN_DT, FN_RESET, FN_ATTACH, FN_SEND = 0x1020248C, 0x10202588, 0x10202950, 0x10202EE4
SIZES = {FN_DT: 216, FN_RESET: 196, FN_ATTACH: 120, FN_SEND: 252}
OWN = set(SIZES)

OP_NEW, OP_MAILCTOR, OP_MAILCONNECT = 0x102384B8, 0x10239520, 0x102395C8
OP_ATTCTOR, OP_ATTDTOR, OP_PTRGLUE = 0x10202670, 0x10202894, 0x1023BAA8
OP_STRASSIGN, OP_STRDTOR, OP_WSTRDTOR = 0x101B2440, 0x1000221C, 0x101468F8
OP_LISTERASE, OP_LISTCLEAR, OP_WSA, OP_DELETE = 0x102084D8, 0x1017FC04, 0x102386F8, 0x10238218

HOST_LAYOUT = [0x4C, 4, 0x10, 0x20, 0x2C, 0x34, 0x38, 0x3D, 0x44, 0x48]

NEWS = []          # the blocks the PowerPC side's operator new handed out in this case

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_new(e):
    L = e.cl
    size = e.r(3) & MASK
    L.rec("new", (size,))
    if L.ret("new") & MASK:
        a = e.alloc(size)
        NEWS.append(a)
        e.set_r(3, a)
    else:
        e.set_r(3, 0)


def stub_ptrglue(e):
    e.cl.rec("PtrGlue", (e.r(12) & MASK, T.s32(e.r(3)), T.s32(e.r(4))))


STUBS.internal(OP_NEW, stub_new)
STUBS.internal(OP_MAILCTOR, CL.recorder("MailCtor", "N"))
STUBS.internal(OP_MAILCONNECT, CL.recorder("MailConnect", "LsHuLs", ret="i"))
STUBS.internal(OP_ATTCTOR, CL.recorder("AttachCtor", "Ns"))
STUBS.internal(OP_ATTDTOR, CL.recorder("AttachDtor", "Li"))
STUBS.internal(OP_PTRGLUE, stub_ptrglue)
STUBS.internal(OP_STRASSIGN, CL.recorder("StringAssign", "Lsu"))
STUBS.internal(OP_STRDTOR, CL.recorder("StringDtor", "Li"))
STUBS.internal(OP_WSTRDTOR, CL.recorder("WStringDtor", "Li"))
STUBS.internal(OP_LISTERASE, CL.recorder("ListErase", "xLuL"))
STUBS.internal(OP_LISTCLEAR, CL.recorder("ListClear", "L"))
STUBS.internal(OP_WSA, CL.recorder("WSACleanup", ""))
STUBS.internal(OP_DELETE, CL.recorder("OperatorDelete", "L"))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def make(self):
        """The blocks every case has: the object, the attachment and mail it may hold, the three argument strings."""
        self.email = self.alloc(0x4C)
        self.pre_att = self.alloc(16)
        self.pre_mail = self.alloc(16)
        self.outbox = self.alloc(16)
        self.s1 = self.alloc(64)
        self.s2 = self.alloc(64)
        self.s3 = self.alloc(64)

    def fixed(self):
        e = self.email
        return {e: 1, e + 4: 2, e + 8: 3, e + 0x10: 4, e + 0x14: 5, e + 0x20: 6, e + 0x24: 7, e + 0x38: 8, self.pre_att: 20,
                self.pre_mail: 21}


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        del NEWS[:]
        self.make()
        self.L = e.cl = CL.PpcLog(self.fixed(), case["queues"])
        self.saved = STUBS.activate(e)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def pw(self, addr):
        return addr

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def news(self):
        return list(NEWS)

    def call(self, entry, args):
        r3, _, _ = T.call_ppc(self.e, entry, [a & MASK for a in args])
        return r3 & MASK

    def destruct(self, obj, flag):
        return self.call(FN_DT, [obj, flag])

    def reset(self):
        self.call(FN_RESET, [self.email])

    def attach(self, path):
        return self.call(FN_ATTACH, [self.email, path]) & 0xFF

    def send(self, server, port, extra, outbox):
        return self.call(FN_SEND, [self.email, server, port, extra, outbox]) & 0xFF


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_email_reset()
        self.base = lib.tpw_email_base()
        self.make()
        CL.host_begin(lib, {label: addr for addr, label in self.fixed().items()}, case["queues"])

    def done(self):
        pass

    def pw(self, addr):
        return addr - self.base

    def alloc(self, n):
        return self.lib.tpw_email_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def news(self):
        out = []
        for i in range(8):
            a = self.lib.tpw_email_new(i)
            if a:
                out.append(a)
        return out

    def destruct(self, obj, flag):
        return self.lib.tpw_email_destruct(obj, T.s16(flag))

    def reset(self):
        self.lib.tpw_email_reset_fn(self.email)

    def attach(self, path):
        return self.lib.tpw_email_attach(self.email, path) & 0xFF

    def send(self, server, port, extra, outbox):
        return self.lib.tpw_email_send(self.email, server, T.s16(port), extra, outbox) & 0xFF


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_email_alloc.restype, lib.tpw_email_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_email_new.restype, lib.tpw_email_new.argtypes = C.c_uint64, [C.c_int32]
    lib.tpw_email_reset.restype, lib.tpw_email_reset.argtypes = None, []
    lib.tpw_email_base.restype, lib.tpw_email_base.argtypes = C.c_uint64, []
    lib.tpw_email_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_email_destruct.restype, lib.tpw_email_destruct.argtypes = C.c_uint64, [C.c_uint64, C.c_int16]
    lib.tpw_email_reset_fn.restype, lib.tpw_email_reset_fn.argtypes = None, [C.c_uint64]
    lib.tpw_email_attach.restype, lib.tpw_email_attach.argtypes = C.c_uint32, [C.c_uint64, C.c_uint64]
    lib.tpw_email_send.restype = C.c_uint32
    lib.tpw_email_send.argtypes = [C.c_uint64, C.c_uint64, C.c_int16, C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_email_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

PTR_WORDS = (11, 13)    # indices of the words at +0x2c (attachment) and +0x34 (mail)


def build(s, case):
    ws = list(case["words"])
    ws[11] = s.pw(s.pre_att) if case["att"] else 0
    ws[13] = s.pw(s.pre_mail) if case["mail"] else 0
    ws[17] = 0
    for i in range(19):
        if i != 15:
            s.words(s.email + 4 * i, [ws[i]])
    s.write(s.email + 0x3C, bytes(case["b3c"]))
    for a, t in ((s.s1, case["t1"]), (s.s2, case["t2"]), (s.s3, case["t3"])):
        s.write(a, t.encode() + b"\0")


def execute(s, case):
    op = case["op"]
    if op == "dt":
        obj = 0 if case["null"] else s.email
        r = s.destruct(obj, case["flag"])
        return 0 if r == 0 else (1 if r == s.email else 2)
    if op == "reset":
        s.reset()
        return 0
    if op == "attach":
        return s.attach(s.s1)
    return s.send(s.s2, case["port"], s.s3, s.outbox if case["outbox"] else 0)


def ptr_id(s, w):
    if w == 0:
        return 0
    # a pointer word is the address on the PowerPC side and the arena offset on the host
    if w == s.pw(s.pre_att):
        return "att"
    if w == s.pw(s.pre_mail):
        return "mail"
    if w == s.pw(s.outbox):
        return "outbox"
    news = [s.pw(a) for a in s.news()]
    return "new%d" % news.index(w) if w in news else "other"


def observe(s, case, answer):
    ws = s.rd_words(s.email, 19)
    b3c = list(s.read(s.email + 0x3C, 4))
    ws[15] = 0
    for i in PTR_WORDS + (17,):
        ws[i] = ptr_id(s, ws[i])
    return {"answer": answer, "log": s.log(), "words": ws, "b3c": b3c, "news": len(s.news())}


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

TEXTS = ["", "a", "smtp.example.com", "mail.maxis.com", "x" * 40]
WORDS = [0, 1, 2, 0x12345678, 0xFFFFFFFF, 0x80000000, 0x102084D8, 0x7FFF]


def base(r, op):
    return {"op": op,
            "queues": {"new": [r.choice([0, 1, 1])], "MailConnect": [r.choice([0, 0, 1, -1, 5])]},
            "words": [r.choice(WORDS) for _ in range(19)],
            "b3c": [r.choice([0, 1, 0xFF, 0x7F]) for _ in range(4)],
            "att": r.choice([0, 1]), "mail": r.choice([0, 1]),
            "t1": r.choice(TEXTS), "t2": r.choice(TEXTS), "t3": r.choice(TEXTS),
            "null": False, "flag": 0, "port": 0, "outbox": 0}


def dt_case(r, flag=None, null=None, att=None):
    c = base(r, "dt")
    c["flag"] = r.choice([-1, 0, 1, 2, 0x7FFF, -0x8000]) if flag is None else flag
    c["null"] = (r.random() < 0.15) if null is None else null
    if att is not None:
        c["att"] = att
    return c


def dt_edges():
    r = random.Random(5129)
    for flag in (-0x8000, -1, 0, 1, 2, 0x7FFF):
        for null in (False, True):
            yield dt_case(r, flag, null)


def dt_random(r):
    return dt_case(r)


def reset_case(r, att=None):
    c = base(r, "reset")
    if att is not None:
        c["att"] = att
    return c


def reset_edges():
    r = random.Random(5130)
    for att in (0, 1):
        for _ in range(2):
            yield reset_case(r, att)


def reset_random(r):
    return reset_case(r)


def attach_case(r, new=None):
    c = base(r, "attach")
    if new is not None:
        c["queues"]["new"] = [new]
    return c


def attach_edges():
    r = random.Random(5131)
    for new in (0, 1):
        for _ in range(3):
            yield attach_case(r, new)


def attach_random(r):
    return attach_case(r)


def send_case(r, size=None, cb=None, mail=None, outbox=None, new=None, conn=None):
    c = base(r, "send")
    ws = c["words"]
    ws[1] = r.choice([0, 1, 3, 0xFFFFFFFF]) if size is None else size
    ws[18] = r.choice([0, 0x10268718, 0x1234]) if cb is None else cb
    ws[16], ws[17] = r.choice(WORDS), r.choice(WORDS)
    if mail is not None:
        c["mail"] = mail
    if new is not None:
        c["queues"]["new"] = [new]
    if conn is not None:
        c["queues"]["MailConnect"] = [conn]
    c["port"] = r.choice([0, 1, -1, 25, 110, 0x7FFF, -0x8000, 0xFFFF, 0x8000])
    c["outbox"] = r.choice([0, 1]) if outbox is None else outbox
    return c


def send_edges():
    r = random.Random(5132)
    for size in (0, 1):
        for cb in (0, 0x1234):
            for outbox in (0, 1):
                yield send_case(r, size, cb, 1, outbox)
    for mail in (0, 1):
        for new in (0, 1):
            for conn in (0, 1, -1):
                yield send_case(r, 2, 0, mail, 0, new, conn)


def send_random(r):
    return send_case(r)


TESTS = [
    FnTest("Email::~Email", ppc, host, edge=dt_edges, rnd=dt_random),
    FnTest("Email::Reset", ppc, host, edge=reset_edges, rnd=reset_random),
    FnTest("Email::AttachData", ppc, host, edge=attach_edges, rnd=attach_random),
    FnTest("Email::Send", ppc, host, edge=send_edges, rnd=send_random),
]
