"""Unit v5530: Outbox::Save and Outbox::WriteMailData, against libtpw_unit_Outbox_WriteMailData.dylib (tests/unit_Outbox_WriteMailData.cpp), main
app ("SimTheme Park"), src/email_load_Outbox_Send_Outbox_WriteMailData.cpp:
  Outbox::Save 0x1020461c, Outbox::WriteMailData 0x10205154

A case is a world and a call. The Outbox is built twice by one piece of Python code: in emulator memory at the Mac's offsets (strings as a
word pointing to a block whose word at +0xc points to the characters, lists as a size word and a ring of nodes) for the original, and in the
host library's std::string / std::list for the translation. The callees outside the unit are fakes on both sides that record the same events:
the stream constructor, open, operator<<, close and the three destructors (one SaveStreamDestroy), the throw (with its message text), time,
localtime (the seconds it was given) and MoveFile. The string functions (constructor, assign, append, destructor) and the list functions
(insert, sort, unique) are modelled on the PowerPC side by Python stubs that do what the library does; the host uses std::string and std::list.

The world of a case: the strings mF00, mF04, mF08, mF18, the mail text list mF0C, the lists mF28, mF34, mF40, the three date words, and the
queues: the stream's initial state, mask and buffer, whether open answers and whether the file is open, close, time, the tm words, MoveFile.
What is compared: the answer of WriteMailData, the event log and every field above read back.

No fused operations. Not tested: the Mac's list size word disagreeing with its nodes (the host list has only one truth).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_Outbox_WriteMailData"
MASK = 0xFFFFFFFF

FN_SAVE, FN_WRITE = 0x1020461C, 0x10205154
SIZES = {FN_SAVE: 956, FN_WRITE: 1432}
OWN = set(SIZES)

STR_CTOR, STR_ASSIGN, STR_APPEND, STR_DTOR = 0x1007363C, 0x10001C80, 0x1020100C, 0x1000221C
OF_CTOR, FB_OPEN, FB_CLOSE, FB_DTOR, OS_DTOR, IOS_DTOR = 0x10071E34, 0x10071C44, 0x10071B54, 0x100700E4, 0x10070000, 0x1006FF50
OPERATOR_LS = 0x10207150
REFPTR_CTOR = 0x100026AC
THROW, TIME, LOCALTIME, MOVEFILE = 0x10238680, 0x1023A198, 0x1023A1B0, 0x1023A1C8
LIST_INSERT, LIST_SORT, LIST_UNIQUE = 0x102092F8, 0x10208C04, 0x10208DCC

HOST_LAYOUT = [0x24, 0x34, 0x38]


def s32(v):
    return T.s32(v)


# ---------------------------------------------------------------------------------------------------------------- Mac-memory models

def m_set_str(e, obj, text):
    """The Mac string at obj: a word to a block whose word at +0xc points at the characters."""
    data = text if isinstance(text, bytes) else text.encode("latin1")
    chars = e.alloc(len(data) + 1, data + b"\0")
    rep = e.alloc(0x10)
    e.write(rep + 0xC, struct.pack(">I", chars))
    e.write(obj, struct.pack(">I", rep))


def m_get_str(e, obj):
    return e.cstr(e.u32(e.u32(obj) + 0xC))


def m_set_list(e, base, vals, setter=None):
    """A Mac list at base: size word, then the sentinel (prev +4, next +8); a node is prev, next, value."""
    sent = base + 4
    nodes = [e.alloc(12) for _ in vals]
    ring = [sent] + nodes + [sent]
    for i, n in enumerate(nodes):
        e.write(n, struct.pack(">II", ring[i], ring[i + 2]))
        if setter:
            setter(n + 8, vals[i])
        else:
            e.write(n + 8, struct.pack(">I", vals[i] & MASK))
    first = nodes[0] if nodes else sent
    last = nodes[-1] if nodes else sent
    e.write(base, struct.pack(">I", len(vals)))
    e.write(sent, struct.pack(">II", last, first))


def m_nodes(e, base):
    sent = base + 4
    out = []
    n = e.u32(sent + 4)
    while n != sent and len(out) < 100:
        out.append(n)
        n = e.u32(n + 4)
    return out


def m_get_list(e, base):
    return [e.u32(n + 8) for n in m_nodes(e, base)]


# ---------------------------------------------------------------------------------------------------------------- the PowerPC fakes

STUBS = CL.Stubs()
STUBS.real.update(OWN)

_write = CL.recorder("SaveStreamWrite", "xs")
_close = CL.recorder("SaveStreamClose", "x")
_open = CL.recorder("SaveStreamOpen", "xs")
_move = CL.recorder("MoveFile", "ss")
_throw = CL.recorder("SaveStreamThrow", "s")
_localtime = CL.recorder("LocalTime", "u")


def st_ctor(e):
    L = e.cl
    L.rec("SaveStreamCtor", ())
    stream = e.r(3)
    ios = stream + 0x48
    e.write(stream, struct.pack(">I", ios))
    e.write(stream + 8 + 0x24, struct.pack(">I", 0))          # the filebuf's word at +0x24
    e.write(ios + 0x24, struct.pack(">I", L.ret("rdbuf") & MASK))
    e.write(ios + 0x34, struct.pack(">I", L.ret("state") & MASK))
    e.write(ios + 0x38, struct.pack(">I", L.ret("mask") & MASK))


def st_open(e):
    L = e.cl
    fb = e.r(3)
    _open(e)
    e.write(fb + 0x24, struct.pack(">I", L.ret("opened") & MASK))
    e.set_r(3, L.ret("open") & MASK)


def st_close(e):
    _close(e)
    e.set_r(3, e.cl.ret("close") & MASK)


def st_write(e):
    r3 = e.r(3)
    _write(e)
    e.set_r(3, r3)


def st_destroy(e):
    e.cl.rec("SaveStreamDestroy", ())


def st_throw(e):
    e.set_r(3, e.u32(e.r(4) + 4))
    _throw(e)


def st_refptr(e):
    e.write(e.r(3), struct.pack(">I", e.r(4)))


def st_time(e):
    e.cl.rec("Time", ())
    e.set_r(3, e.cl.ret("Time") & MASK)


def st_localtime(e):
    L = e.cl
    secs = e.u32(e.r(3))
    e.set_r(3, secs)
    _localtime(e)
    tm = e.alloc(0x24)
    e.write(tm + 0xC, struct.pack(">III", L.ret("tm_mday") & MASK, L.ret("tm_mon") & MASK, L.ret("tm_year") & MASK))
    e.set_r(3, tm)


def st_movefile(e):
    _move(e)
    e.set_r(3, e.cl.ret("MoveFile") & MASK)


def s_ctor(e):
    m_set_str(e, e.r(3), "")


def s_assign(e):
    this, src, pos, n = e.r(3), e.r(4), e.r(5), e.r(6)
    text = m_get_str(e, src)
    text = text[pos:] if n == MASK else text[pos:pos + n]
    m_set_str(e, this, text)


def s_append(e):
    this, p, n = e.r(3), e.r(4), e.r(5)
    m_set_str(e, this, m_get_str(e, this) + e.read(p, n).decode("latin1"))


def l_insert(e):
    base, pos, vptr = e.r(4), e.r(5), e.r(6)
    node = e.alloc(12)
    prev = e.u32(pos)
    e.write(node, struct.pack(">II", prev, pos))
    e.write(node + 8, struct.pack(">I", e.u32(vptr)))
    e.write(prev + 4, struct.pack(">I", node))
    e.write(pos, struct.pack(">I", node))
    e.write(base, struct.pack(">I", e.u32(base) + 1))


def l_sort(e):
    base = e.r(3)
    nodes = m_nodes(e, base)
    vals = sorted(e.u32(n + 8) for n in nodes)
    for n, v in zip(nodes, vals):
        e.write(n + 8, struct.pack(">I", v))


def l_unique(e):
    base = e.r(3)
    nodes = m_nodes(e, base)
    keep = []
    for n in nodes:
        if keep and e.u32(keep[-1] + 8) == e.u32(n + 8):
            continue
        keep.append(n)
    sent = base + 4
    ring = [sent] + keep + [sent]
    for i, n in enumerate(keep):
        e.write(n, struct.pack(">II", ring[i], ring[i + 2]))
    e.write(sent, struct.pack(">II", keep[-1] if keep else sent, keep[0] if keep else sent))
    e.write(base, struct.pack(">I", len(keep)))


for _a, _f in ((OF_CTOR, st_ctor), (FB_OPEN, st_open), (FB_CLOSE, st_close), (OPERATOR_LS, st_write), (FB_DTOR, st_destroy),
               (OS_DTOR, lambda e: None), (IOS_DTOR, lambda e: None), (THROW, st_throw), (REFPTR_CTOR, st_refptr), (TIME, st_time),
               (LOCALTIME, st_localtime), (MOVEFILE, st_movefile), (STR_CTOR, s_ctor), (STR_ASSIGN, s_assign), (STR_APPEND, s_append),
               (STR_DTOR, lambda e: None), (LIST_INSERT, l_insert), (LIST_SORT, l_sort), (LIST_UNIQUE, l_unique)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


# ---------------------------------------------------------------------------------------------------------------- the two sides

STR_OFFS = (0x00, 0x04, 0x08, 0x18)


class Ppc:
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.L = e.cl = CL.PpcLog({}, case["queues"])
        self.saved = STUBS.activate(e)
        self.obj = e.alloc(0x60)

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def set_str(self, which, text):
        m_set_str(self.e, self.obj + STR_OFFS[which], text)

    def get_str(self, which):
        return m_get_str(self.e, self.obj + STR_OFFS[which])

    def set_mails(self, texts):
        m_set_list(self.e, self.obj + 0x0C, texts, setter=lambda a, t: m_set_str(self.e, a, t))

    def get_mails(self):
        return [m_get_str(self.e, n + 8) for n in m_nodes(self.e, self.obj + 0x0C)]

    def set_list(self, which, vals):
        m_set_list(self.e, self.obj + (0x28, 0x34, 0x40)[which], vals)

    def get_list(self, which):
        return m_get_list(self.e, self.obj + (0x28, 0x34, 0x40)[which])

    def set_f1c(self, words):
        self.e.write(self.obj + 0x1C, struct.pack(">III", *[w & MASK for w in words]))

    def f1c(self):
        return list(struct.unpack(">III", self.e.read(self.obj + 0x1C, 12)))

    def log(self):
        return self.L.log

    def call(self, op):
        r3, _, _ = T.call_ppc(self.e, FN_WRITE if op == "write" else FN_SAVE, [self.obj])
        return r3 & MASK if op == "write" else 0


class Host:
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_ow_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def set_str(self, which, text):
        self.lib.tpw_ow_set_str(which, text.encode("latin1"))

    def get_str(self, which):
        return self.lib.tpw_ow_get_str(which).decode("latin1")

    def set_mails(self, texts):
        for t in texts:
            self.lib.tpw_ow_add_mail_text(t.encode("latin1"))

    def get_mails(self):
        return [self.lib.tpw_ow_mail_text(i).decode("latin1") for i in range(self.lib.tpw_ow_mail_count())]

    def set_list(self, which, vals):
        for v in vals:
            self.lib.tpw_ow_push(which, v & MASK)

    def get_list(self, which):
        return [self.lib.tpw_ow_value(which, i) for i in range(self.lib.tpw_ow_count(which))]

    def set_f1c(self, words):
        buf = (C.c_uint8 * 12)(*struct.pack("<III", *[w & MASK for w in words]))
        self.lib.tpw_ow_set_f1c(buf)

    def f1c(self):
        buf = (C.c_uint8 * 12)()
        self.lib.tpw_ow_f1c(buf)
        return list(struct.unpack("<III", bytes(buf)))

    def log(self):
        return CL.host_end(self.lib)

    def call(self, op):
        if op == "write":
            return self.lib.tpw_ow_write_mail_data() & MASK
        self.lib.tpw_ow_save()
        return 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    lib.tpw_ow_layout.argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_ow_set_str.argtypes = [C.c_int32, C.c_char_p]
    lib.tpw_ow_get_str.restype, lib.tpw_ow_get_str.argtypes = C.c_char_p, [C.c_int32]
    lib.tpw_ow_add_mail_text.argtypes = [C.c_char_p]
    lib.tpw_ow_mail_count.restype = C.c_int32
    lib.tpw_ow_mail_text.restype, lib.tpw_ow_mail_text.argtypes = C.c_char_p, [C.c_int32]
    lib.tpw_ow_push.argtypes = [C.c_int32, C.c_uint32]
    lib.tpw_ow_count.restype, lib.tpw_ow_count.argtypes = C.c_int32, [C.c_int32]
    lib.tpw_ow_value.restype, lib.tpw_ow_value.argtypes = C.c_uint32, [C.c_int32, C.c_int32]
    lib.tpw_ow_f1c.argtypes = [C.POINTER(C.c_uint8)]
    lib.tpw_ow_set_f1c.argtypes = [C.POINTER(C.c_uint8)]
    lib.tpw_ow_write_mail_data.restype = C.c_int32
    out = (C.c_int64 * 32)()
    n = lib.tpw_ow_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

def build(s, case):
    for i in range(4):
        s.set_str(i, case["strs"][i])
    s.set_mails(case["mails"])
    for i in range(3):
        s.set_list(i, case["lists"][i])
    s.set_f1c(case["f1c"])


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "strs": [s.get_str(i) for i in range(4)], "mails": s.get_mails(),
            "lists": [s.get_list(i) for i in range(3)], "f1c": s.f1c()}


def run(s, case):
    try:
        build(s, case)
        return observe(s, case, s.call(case["op"]))
    finally:
        s.done()


def ppc(e, case):
    return run(Ppc(e, case), case)


def host(lib, case):
    return run(Host(lib, case), case)


# ---------------------------------------------------------------------------------------------------------------- case generators

WORDS = ["", "a", "path", "Mail:Outbox", "x%dy", "hello world", "user@host.net", "%s%08x", "Subject line"]
NUMS = [0, 1, 2, 5, 9, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE]


def rl(r, n=None):
    n = r.randrange(0, 5) if n is None else n
    return [r.choice(NUMS + [r.randrange(0, 20), r.randrange(0, 1 << 32)]) for _ in range(n)]


def base(r, op, **kw):
    q = {
        "open": [kw.get("open", r.choice([1, 1, 1, 0]))],
        "opened": [kw.get("opened", r.choice([1, 1, 1, 0]))],
        "close": [kw.get("close", r.choice([1, 1, 1, 0]))],
        "rdbuf": [kw.get("rdbuf", r.choice([0, 1]))],
        "state": [kw.get("state", r.choice([0, 0, 1, 4, 5]))],
        "mask": [kw.get("mask", r.choice([0, 0, 1, 4, 5, 7]))],
        "Time": [r.randrange(0, 1 << 31)],
        "tm_mday": [r.choice([1, 15, 31, r.randrange(0, 32)])],
        "tm_mon": [r.choice([0, 11, r.randrange(0, 12)])],
        "tm_year": [r.choice([0, 70, 126, r.randrange(0, 300), MASK])],
        "MoveFile": [r.choice([0, 1])],
    }
    lists = [rl(r), rl(r), rl(r)]
    if "l28" in kw:
        lists[0] = kw["l28"]
    return {"op": op, "queues": q, "strs": [r.choice(WORDS), r.choice(WORDS), r.choice(WORDS), r.choice(WORDS)],
            "mails": [r.choice(WORDS) for _ in range(r.randrange(0, 4))], "lists": lists,
            "f1c": [r.randrange(0, 1 << 32) for _ in range(3)]}


def edges(op):
    def gen():
        r = random.Random(0x5530 + len(op))
        for o in (1, 0):
            for opened in (1, 0):
                for cl in (1, 0):
                    for mask in (0, 1, 4, 5):
                        for rdbuf in (0, 1):
                            yield base(r, op, open=o, opened=opened, close=cl, mask=mask, rdbuf=rdbuf, state=r.choice([0, 1, 4]))
        for l28 in ([], [0], [5], [0x7FFFFFFF], [0xFFFFFFFF], [1, 2, 3], [3, 2, 1]):
            yield base(r, op, open=1, opened=1, close=1, l28=l28)
    return gen


def rnd(op):
    return lambda r: base(r, op)


TESTS = [
    FnTest("Outbox::Save", ppc, host, edge=edges("save"), rnd=rnd("save")),
    FnTest("Outbox::WriteMailData", ppc, host, edge=edges("write"), rnd=rnd("write")),
]
