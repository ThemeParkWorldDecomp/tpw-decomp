"""Unit v5218, Outbox::Send and Outbox::NextMail, against libtpw_unit_Outbox_Send.dylib (tests/unit_Outbox_Send.cpp), main app
("SimTheme Park"), src/game/online/email.cpp:
  Outbox::Send 0x102059fc, Outbox::NextMail 0x10205ed0

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian,
MSL strings as a word pointing to a rep whose word at +0xc is the text pointer, lists as size + sentinel + nodes) for the original, and in
the host library (native std::string / std::list for the outbox, 32-bit words for the e-mail) for the translation. Callees outside the
unit are fakes on both sides recording the same events: operator new / delete / delete[], Email::Email, CFSOnlineSystem::CFSOnlineSystem,
the five login getters, ReadMailData / DeleteMail / Save, the string assign / dtor / ctor and list erase / insert on the e-mail's words,
the attachment constructor and Email::Send. Pointers are logged as block label * 0x1000 + offset, texts as length and CRC-32. The
outbox's own strings and the three number lists are really assigned / removed on both sides (list::remove is a PowerPC stub that edits
the Mac list), and compared afterwards.

The world of a case: the outbox (lists, recipients, subject, attachment path, mF50, mF54, mF58, mEmail), the e-mail block with random
words (and an attachment with or without data), gOnlineSystem null or set, the login texts, the answer queues. What is compared: the
answer, the call log, the outbox strings / numbers / lists, the e-mail's words (the attachment pointer as a label) and gOnlineSystem.

No fused operations. Not tested: NextMail with an empty mF34 at entry (the original reads the sentinel's neighbouring word; the host
list has none), a login address longer than the 140 byte stack buffer (the original smashes its frame).
"""
import ctypes as C
import random
import struct
import zlib

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Outbox_Send"
MASK = 0xFFFFFFFF

FN_SEND, FN_NEXT = 0x102059fc, 0x10205ed0
SIZES = {FN_SEND: 1196, FN_NEXT: 892}
OWN = set(SIZES)

ADDR = dict(gOnlineSystem=0x10380ab0)
HOST_LAYOUT = [0x2c, 0x48, 8, 0xc, 0x18, 0x28, 0x4c]

ST = {"blocks": {}, "prep": {}, "login": (0, 0, 0), "ob": 0}


def s16(v):
    return T.s16(v)


def s32(v):
    return T.s32(v)


def crc(b):
    return zlib.crc32(bytes(b)) & MASK


def wh(a):
    a &= MASK
    if a == 0:
        return 0
    for i, (base, size) in ST["blocks"].items():
        if base <= a < base + size:
            return i * 0x1000 + (a - base)
    return -1


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)


def cbytes(e, a):
    out = bytearray()
    while len(out) < 4096:
        b = e.read(a + len(out), 1)[0]
        if b == 0:
            break
        out.append(b)
    return bytes(out)


def wbytes(e, a, n):
    """n UTF-16 units at a, as little-endian bytes (the host's memory order)."""
    return struct.pack("<%dH" % n, *struct.unpack(">%dH" % n, e.read(a, 2 * n))) if n else b""


def set_ppc_string(e, obj, data):
    buf = e.alloc(len(data) + 1, bytes(data) + b"\0")
    rep = e.alloc(0x10)
    e.write(rep + 0xC, struct.pack(">I", buf))
    e.write(obj, struct.pack(">I", rep))


def get_ppc_string(e, obj):
    return cbytes(e, e.u32(e.u32(obj) + 0xC))


def stub(addr):
    def deco(fn):
        STUBS.internal(addr, fn)
        return fn
    return deco


@stub(0x102384B8)
def st_new(e):
    size = e.r(3)
    e.cl.rec("new", (size,))
    e.set_r(3, ST["prep"].get(size) or e.alloc(size))


@stub(0x10238218)
def st_delete(e):
    e.cl.rec("delete", (wh(e.r(3)),))


@stub(0x10238638)
def st_deletea(e):
    e.cl.rec("deletea", (wh(e.r(3)),))


@stub(0x10202244)
def st_email_ctor(e):
    e.cl.rec("EmailCtor", (wh(e.r(3)),))


@stub(0x1016F7D0)
def st_sys_ctor(e):
    e.cl.rec("SysCtor", (wh(e.r(3)),))


@stub(0x10202670)
def st_attach_ctor(e):
    t = cbytes(e, e.r(4))
    e.cl.rec("AttachCtor", (wh(e.r(3)), len(t), crc(t)))


def login_getter(name, index):
    def fn(e):
        e.cl.rec(name, (wh(e.r(3)),))
        e.set_r(3, ST["login"][index])
    return fn


STUBS.internal(0x101718B8, login_getter("LoginEmailAddress", 0))
STUBS.internal(0x10171834, login_getter("LoginCustomerName", 1))
STUBS.internal(0x10171878, login_getter("LoginPassword", 2))


@stub(0x101718FC)
def st_player(e):
    e.cl.rec("LoginPlayerID", (wh(e.r(3)),))
    e.set_r(3, e.cl.ret("PlayerID") & MASK)


@stub(0x1017193C)
def st_customer(e):
    e.cl.rec("LoginCustomerID", (wh(e.r(3)),))
    e.set_r(3, e.cl.ret("CustomerID") & MASK)


@stub(0x101B2440)
def st_assign(e):
    n = e.r(5)
    t = e.read(e.r(4), n)
    if e.r(3) == ST["ob"] + 0x50:
        set_ppc_string(e, e.r(3), t)         # the outbox's own string: really assigned, not logged (the host assigns natively)
        return
    e.cl.rec("StringAssign", (wh(e.r(3)), n, crc(t)))


@stub(0x101B2510)
def st_wassign(e):
    n = e.r(5)
    e.cl.rec("WStringAssign", (wh(e.r(3)), n, crc(wbytes(e, e.r(4), n))))


@stub(0x1000221C)
def st_sdtor(e):
    e.cl.rec("StringDtor", (wh(e.r(3)), s16(e.r(4))))


@stub(0x102084D8)
def st_erase(e):
    e.cl.rec("ListErase", (wh(e.r(4)), e.r(5), wh(e.r(6))))


@stub(0x10002338)
def st_sctor(e):
    t = cbytes(e, e.r(4))
    e.cl.rec("StringCtor", (len(t), crc(t)))
    e.write(e.r(3), struct.pack(">I", e.alloc(len(t) + 1, t + b"\0")))


@stub(0x101B25E0)
def st_insert(e):
    t = cbytes(e, e.u32(e.r(6)))
    e.cl.rec("ListInsert", (wh(e.r(4)), wh(e.r(5)), len(t), crc(t)))


@stub(0x102049FC)
def st_read(e):
    e.cl.rec("ReadMailData", (wh(e.r(3)), s32(e.r(4))))
    e.set_r(3, e.cl.ret("ReadMailData") & 0xFF)


@stub(0x1020638C)
def st_delmail(e):
    e.cl.rec("DeleteMail", (wh(e.r(3)), s32(e.r(4))))


@stub(0x1020461C)
def st_save(e):
    e.cl.rec("Save", (wh(e.r(3)),))


@stub(0x10202EE4)
def st_send(e):
    server = cbytes(e, e.r(4))
    extra = cbytes(e, e.r(6)) if e.r(6) else b""
    e.cl.rec("EmailSend", (wh(e.r(3)), s16(e.r(5)), 1 if e.r(6) else 0, wh(e.r(7)), len(server), crc(server), len(extra), crc(extra)))
    e.set_r(3, e.cl.ret("EmailSend") & MASK)


@stub(0x10208F44)
def st_remove(e):
    lst, val = e.r(3), e.u32(e.r(4))
    sentinel = lst + 4
    node = e.u32(lst + 8)
    size = e.u32(lst)
    while node != sentinel:
        nxt = e.u32(node + 4)
        if e.u32(node + 8) == val:
            prev = e.u32(node)
            e.write(prev + 4, struct.pack(">I", nxt))
            e.write(nxt, struct.pack(">I", prev))
            size -= 1
        node = nxt
    e.write(lst, struct.pack(">I", size))


@stub(0x10238650)
def st_strlen(e):
    e.set_r(3, len(cbytes(e, e.r(3))))


@stub(0x102389C8)
def st_wcslen(e):
    a, n = e.r(3), 0
    while e.read(a + 2 * n, 2) != b"\0\0":
        n += 1
    e.set_r(3, n)


def setup(runner):
    pass


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def u16s(self, vals):
        a = self.alloc(2 * len(vals) + 2)
        self.write(a, struct.pack(self.fmt[0] + "%dH" % (len(vals) + 1), *(list(vals) + [0])))
        return a

    def cstr(self, data):
        a = self.alloc(len(data) + 1)
        self.write(a, bytes(data) + b"\0")
        return a

    def reg(self, index, base, size):
        self.blocks[index] = (base, size)
        self.on_reg(index, base, size)

    def lab(self, a):
        a &= 0xFFFFFFFFFFFFFFFF
        if a == 0:
            return 0
        for i, (base, size) in self.blocks.items():
            if base <= a < base + size:
                return i * 0x1000 + (a - base)
        return -1


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.blocks = ST["blocks"] = {}
        ST["prep"] = {}
        self.L = e.cl = CL.PpcLog({}, case["queues"])
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

    def on_reg(self, index, base, size):
        pass

    def ptr(self, a):
        return a

    def unptr(self, w):
        return w

    def read_ptr(self, a):
        return self.e.u32(a)

    def write_ptr(self, a, v):
        self.write(a, struct.pack(">I", v))

    # the outbox in the Mac's layout
    def new_outbox(self):
        ob = self.alloc(0x60)
        ST["ob"] = ob
        for off in (0x0C, 0x28, 0x34, 0x40):
            self.words(ob + off, [0, ob + off + 4, ob + off + 4])
        return ob

    def ob_str(self, ob, which, data):
        set_ppc_string(self.e, ob + which, data)

    def ob_rcpts(self, ob, texts):
        reps = []
        for t in texts:
            rep = self.alloc(0x10)
            self.write(rep + 0xC, struct.pack(">I", self.cstr(t)))
            reps.append(rep)
        self.ob_list(ob, 0x0C, reps)

    def ob_list(self, ob, which, vals):
        lst = ob + which
        prev = lst + 4
        for v in vals:
            node = self.alloc(12)
            self.words(node, [prev, lst + 4, v])
            self.words(prev + 4, [node])
            prev = node
        self.words(lst + 4, [prev])
        self.words(lst, [len(vals)])

    def ob_n(self, ob, which, v):
        if which == 0x54:
            self.write(ob + 0x54, struct.pack(">H", v & 0xFFFF))
        else:
            self.words(ob + which, [v])

    def prep(self, email, sys, att):
        ST["prep"] = {0x4C: email, 0x404: sys, 0xC: att}

    def login(self, a, b, c):
        ST["login"] = (a, b, c)

    def rd_list(self, ob, which):
        lst = ob + which
        size = self.e.u32(lst)
        vals, node = [], self.e.u32(lst + 8)
        while node != lst + 4 and len(vals) < 64:
            vals.append(self.e.u32(node + 8))
            node = self.e.u32(node + 4)
        return [size, vals]

    def rd_str(self, ob, which):
        return get_ppc_string(self.e, ob + which)

    def rd_n(self, ob, which):
        if which == 0x54:
            return struct.unpack(">H", self.read(ob + 0x54, 2))[0]
        return self.e.u32(ob + which)

    def call_send(self, ob, server, port, extra):
        r3, _, _ = T.call_ppc(self.e, FN_SEND, [ob, server, port & MASK, extra])
        return r3 & MASK

    def call_next(self, ob, email):
        r3, _, _ = T.call_ppc(self.e, FN_NEXT, [ob, email])
        return r3 & MASK


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.blocks = {}
        for i in range(1, 8):
            lib.tpw_ob_block(i, 0, 0)
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_ob_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_ob_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def on_reg(self, index, base, size):
        self.lib.tpw_ob_block(index, base, size)

    def ptr(self, a):
        return self.lib.tpw_ob_ptr32(a)

    def unptr(self, w):
        return self.lib.tpw_ob_unptr32(w)

    def read_ptr(self, a):
        return struct.unpack("<Q", self.read(a, 8))[0]

    def write_ptr(self, a, v):
        self.write(a, struct.pack("<Q", v))

    def new_outbox(self):
        return self.lib.tpw_ob_new_outbox()

    def ob_str(self, ob, which, data):
        self.lib.tpw_ob_set_str(ob, which, bytes(data))

    def ob_rcpts(self, ob, texts):
        for t in texts:
            self.lib.tpw_ob_rcpt_push(ob, bytes(t))

    def ob_list(self, ob, which, vals):
        for v in vals:
            self.lib.tpw_ob_list_push(ob, which, v)

    def ob_n(self, ob, which, v):
        self.lib.tpw_ob_set_n(ob, which, v)

    def prep(self, email, sys, att):
        self.lib.tpw_ob_prep(email, sys, att)

    def login(self, a, b, c):
        self.lib.tpw_ob_login(a, b, c)

    def rd_list(self, ob, which):
        out = (C.c_uint32 * 64)()
        n = self.lib.tpw_ob_list_read(ob, which, out, 64)
        return [n, list(out)[:min(n, 64)]]

    def rd_str(self, ob, which):
        buf = C.create_string_buffer(256)
        self.lib.tpw_ob_get_str(ob, which, buf, 256)
        return buf.value

    def rd_n(self, ob, which):
        return self.lib.tpw_ob_get_n(ob, which) & (0xFFFF if which == 0x54 else 0xFFFFFFFFFFFFFFFF)

    def call_send(self, ob, server, port, extra):
        return self.lib.tpw_ob_Send(ob, server, s32(port), extra) & MASK

    def call_next(self, ob, email):
        return self.lib.tpw_ob_NextMail(ob, email) & MASK


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ob_addr.restype, lib.tpw_ob_addr.argtypes = C.c_uint64, [C.c_char_p]
    lib.tpw_ob_alloc.restype, lib.tpw_ob_alloc.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ob_ptr32.restype, lib.tpw_ob_ptr32.argtypes = C.c_uint32, [C.c_uint64]
    lib.tpw_ob_unptr32.restype, lib.tpw_ob_unptr32.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_ob_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ob_new_outbox.restype = C.c_uint64
    lib.tpw_ob_outbox_size.restype = C.c_uint32
    lib.tpw_ob_block.argtypes = [C.c_int32, C.c_uint64, C.c_uint32]
    lib.tpw_ob_prep.argtypes = [C.c_uint64] * 3
    lib.tpw_ob_login.argtypes = [C.c_uint64] * 3
    lib.tpw_ob_set_str.argtypes = [C.c_uint64, C.c_int32, C.c_char_p]
    lib.tpw_ob_get_str.restype, lib.tpw_ob_get_str.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.c_char_p, C.c_int32]
    lib.tpw_ob_rcpt_push.argtypes = [C.c_uint64, C.c_char_p]
    lib.tpw_ob_list_push.argtypes = [C.c_uint64, C.c_int32, C.c_uint32]
    lib.tpw_ob_list_read.restype, lib.tpw_ob_list_read.argtypes = C.c_int32, [C.c_uint64, C.c_int32, C.POINTER(C.c_uint32), C.c_int32]
    lib.tpw_ob_set_n.argtypes = [C.c_uint64, C.c_int32, C.c_uint64]
    lib.tpw_ob_get_n.restype, lib.tpw_ob_get_n.argtypes = C.c_uint64, [C.c_uint64, C.c_int32]
    lib.tpw_ob_Send.restype, lib.tpw_ob_Send.argtypes = C.c_int32, [C.c_uint64, C.c_uint64, C.c_int32, C.c_uint64]
    lib.tpw_ob_NextMail.restype, lib.tpw_ob_NextMail.argtypes = C.c_int32, [C.c_uint64, C.c_uint64]
    out = (C.c_int64 * 32)()
    n = lib.tpw_ob_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

EMAIL_WORDS = 0x4C // 4


def build(s, case):
    ob = s.new_outbox()
    s.reg(1, ob, 0x60 if isinstance(s, Ppc) else s.lib.tpw_ob_outbox_size())
    email = s.alloc(0x4C)
    s.reg(2, email, 0x4C)
    sysb = s.alloc(0x404)
    s.reg(3, sysb, 0x404)
    att1 = s.alloc(0xC)
    s.reg(4, att1, 0xC)
    data = s.alloc(16)
    s.reg(5, data, 16)
    att2 = s.alloc(0xC)
    s.reg(6, att2, 0xC)
    s.prep(email, sysb, att2)
    ws = list(case["email_words"])
    ws[0x2C // 4] = s.ptr(att1) if case["att"] else 0
    s.words(email, ws)
    s.words(att1, [case["att_words"][0], case["att_words"][1], s.ptr(data) if case["att_data"] else 0])
    s.login(s.u16s(case["addr"]), s.u16s(case["name"]), s.u16s(case["pass"]))
    s.ob_str(ob, 0x08, case["subject"])
    s.ob_str(ob, 0x18, case["path"])
    s.ob_str(ob, 0x50, case["f50"])
    s.ob_rcpts(ob, case["rcpts"])
    for which in (0x28, 0x34, 0x40):
        s.ob_list(ob, which, case["lists"][which])
    s.ob_n(ob, 0x54, case["f54"])
    s.ob_n(ob, 0x58, case["f58"])
    s.ob_n(ob, 0x5C, email if case["has_email"] else 0)
    s.write_ptr(s.g("gOnlineSystem"), sysb if case["sys"] else 0)
    s.ctx = (ob, email)


def execute(s, case):
    ob, email = s.ctx
    if case["op"] == "send":
        server = s.cstr(case["server"])
        extra = s.cstr(case["extra"]) if case["extra"] is not None else 0
        return s.call_send(ob, server, case["port"], extra)
    return s.call_next(ob, email)


def observe(s, case, answer):
    ob, email = s.ctx
    ws = s.rd_words(email, EMAIL_WORDS)
    ws[0x2C // 4] = s.lab(s.unptr(ws[0x2C // 4]))
    return {
        "answer": answer,
        "log": s.log(),
        "email": ws,
        "f50": s.rd_str(ob, 0x50),
        "f54": s.rd_n(ob, 0x54),
        "f58": s.rd_n(ob, 0x58),
        "mEmail": s.lab(s.rd_n(ob, 0x5C)),
        "l28": s.rd_list(ob, 0x28),
        "l34": s.rd_list(ob, 0x34),
        "l40": s.rd_list(ob, 0x40),
        "sys": s.lab(s.read_ptr(s.g("gOnlineSystem"))),
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


# ---------------------------------------------------------------------------------------------------------------- case generators

ALNUM = b"abcdefghijklmnopqrstuvwxyz0123456789@.-_"


def rtext(r, lo=0, hi=12):
    return bytes(r.choice(ALNUM) for _ in range(r.randint(lo, hi)))


def rwide(r, lo=0, hi=8, ascii_only=False):
    out = []
    for _ in range(r.randint(lo, hi)):
        low = r.randrange(1, 256)
        high = 0 if ascii_only else r.choice([0, 0, 0, 1, 0x30, 0xFF])
        out.append(high << 8 | low)
    return out


def raddr(r):
    a = rwide(r, 0, 40)
    if a and r.random() < 0.15:
        a[r.randrange(len(a))] = r.choice([0x100, 0x4200])        # a zero low byte ends the copy early
    return a


def base(r, op):
    lists = {0x28: [r.randint(1, 3) for _ in range(r.randint(0, 4))],
             0x40: [r.randint(1, 3) for _ in range(r.randint(0, 4))],
             0x34: [r.randint(1, 3) for _ in range(r.randint(1, 4))]}
    return {
        "op": op,
        "sys": r.choice([0, 1]), "has_email": r.choice([0, 1]), "att": r.choice([0, 1]), "att_data": r.choice([0, 1]),
        "email_words": [r.choice([0, 1, r.randrange(1 << 32)]) for _ in range(EMAIL_WORDS)],
        "att_words": [r.randrange(1 << 32), r.randrange(1 << 32)],
        "addr": raddr(r), "name": rwide(r), "pass": rwide(r),
        "subject": rtext(r), "path": rtext(r), "f50": rtext(r), "rcpts": [rtext(r, 0, 10) for _ in range(r.randint(0, 3))],
        "lists": lists, "f54": r.randrange(1 << 16), "f58": r.choice([0, 1, -1, r.randrange(1 << 31)]) & MASK,
        "server": rtext(r, 0, 16), "port": r.choice([0, 25, -1, 0x7FFF, 0x8000, r.randrange(1 << 16)]),
        "extra": r.choice([None, rtext(r, 0, 8)]),
        "queues": {"ReadMailData": [r.choice([0, 1, 1])], "EmailSend": [r.choice([0, 1])],
                   "PlayerID": [r.choice([0, -1, r.randrange(1 << 31)])], "CustomerID": [r.choice([0, -1, r.randrange(1 << 31)])]},
    }


def send_case(r):
    return base(r, "send")


def send_edges():
    r = random.Random(5218)
    for k in range(128):
        c = base(r, "send")
        c["sys"], c["has_email"], c["att"], c["att_data"] = k & 1, (k >> 1) & 1, (k >> 2) & 1, (k >> 3) & 1
        c["queues"]["ReadMailData"] = [(k >> 4) & 1]
        c["queues"]["EmailSend"] = [(k >> 5) & 1]
        if (k >> 6) & 1:
            c["lists"][0x34] = []
        yield c
    c = base(r, "send")
    c["addr"], c["rcpts"], c["has_email"], c["att"] = [], [], 1, 1
    yield c


def next_case(r):
    return base(r, "next")


def next_edges():
    r = random.Random(10205)
    for k in range(64):
        c = base(r, "next")
        c["sys"] = k & 1
        c["queues"]["ReadMailData"] = [(k >> 1) & 1]
        c["att"] = (k >> 2) & 1
        c["lists"][0x34] = [[7], [7, 7], [7, 8], [7, 8, 7, 9], [7, 9][::-1], [7, 7, 8]][(k >> 3) % 6]
        c["lists"][0x28] = [7, 1, 7]
        c["lists"][0x40] = [2, 7] if (k >> 5) & 1 else []
        yield c


TESTS = [
    FnTest("Outbox::Send", ppc, host, edge=send_edges, rnd=send_case),
    FnTest("Outbox::NextMail", ppc, host, edge=next_edges, rnd=next_case),
]
