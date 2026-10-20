"""Call-log tests: the Python half of tests/calllog.h.

A unit's callees are fakes on both sides that record events (name, integers, texts). The
PowerPC side builds its fakes from compact specs (Stubs below); the host side's are C++
in tests/unit_<Name>.cpp. A case runs the function under test on both sides with the
same fixed labels and return queues and compares the logs.

Stubs are installed for one case at a time and the emulator's previous stubs restored
afterwards, so other test modules sharing the emulator never see them.

Argument codes of a spec, read left to right (GPRs from r3, FPRs from f1; a float also
uses up a GPR slot, as the PowerOpen convention places it):
  L  label of a pointer          N  new label for a pointer (constructors)
  i  signed 32-bit               u  unsigned 32-bit
  h  signed 16-bit               H  unsigned 16-bit         b  8-bit
  w  high half of the word (a one-short struct passed by value)
  f  float (recorded as its single-precision bit pattern)
  s  C string                    S  UTF-16 string (big-endian, narrowed)
  T  TbDynamicStringTemplate<char> text      W  TbDynamicStringTemplate<wchar_t> text
  x  skip a GPR
  D  destructor: label of `this`, then the flag GPR; a positive flag also records
     "delete" and forgets the label (the host's operator delete does the same)
"""
import ctypes as C
import struct

import tpwtest as T

FAKE_VT = 0x7C0FE000      # vptr of the PowerPC side's fake strings (never called)
FIRST_NEW = 100           # first label for objects made during a case


class TpwCallEvent(C.Structure):
    _fields_ = [("name", C.c_char * 48), ("nints", C.c_int32), ("ntexts", C.c_int32),
                ("ints", C.c_int64 * 12), ("texts", (C.c_char * 160) * 3)]


def fbits(x):
    return struct.unpack(">I", struct.pack(">f", x))[0]


def narrow(text):
    return "".join(c if ord(c) < 0x80 else "?" for c in text)[:159]


# ----------------------------------------------------------------- PowerPC side


class PpcLog:
    """Per-case state of the PowerPC fakes: the log, labels and return queues."""

    def __init__(self, fixed, queues=None, first_new=FIRST_NEW):
        self.log = []
        self.labels = {a: l for a, l in fixed.items() if a}
        self.next = first_new
        self.queues = {k: list(v) for k, v in (queues or {}).items()}
        self.pos = {}

    def lab(self, a):
        return 0 if a == 0 else self.labels.get(a, -1)

    def newlab(self, a):
        self.forget(a)
        label = self.next
        self.next += 1
        self.labels[a] = label
        return label

    def forget(self, a):
        self.labels.pop(a, None)

    def ret(self, key, dflt=0):
        q = self.queues.get(key)
        if not q:
            return dflt
        i = self.pos.get(key, 0)
        if i < len(q):
            self.pos[key] = i + 1
            return q[i]
        return q[-1]

    def rec(self, name, ints=(), texts=()):
        self.log.append((name, tuple(ints), tuple(texts)))


def tds_text(e, obj):
    """TbDynamicStringTemplate<char>: data pointer at +8."""
    if obj == 0:
        return "(null)"
    d = e.u32(obj + 8)
    return e.cstr(d)[:159] if d else ""


def wide_text(e, a, limit=159):
    out = []
    while a and len(out) < limit:
        ch = struct.unpack(">H", e.read(a + 2 * len(out), 2))[0]
        if ch == 0:
            break
        out.append(chr(ch))
    return narrow("".join(out))


def wds_text(e, obj):
    if obj == 0:
        return "(null)"
    return wide_text(e, e.u32(obj + 8))


def new_text(e, text):
    b = text.encode("latin-1")
    return e.alloc(len(b) + 1, b + b"\0", alignment=4)


def new_wide(e, text):
    b = b"".join(struct.pack(">H", ord(ch)) for ch in text) + b"\0\0"
    return e.alloc(len(b), b, alignment=4)


def tds_set(e, obj, text):
    e.write(obj, struct.pack(">IIIII", FAKE_VT, 0, new_text(e, text), len(text), 0))


def wds_set(e, obj, text):
    e.write(obj, struct.pack(">IIIII", FAKE_VT, 0, new_wide(e, text), len(text), 0))


def string_stubs():
    """Functional bullfrog strings (unrecorded): construction, assignment, appending."""
    def ctor0(e):
        tds_set(e, e.r(3), "")

    def ctor(e):
        tds_set(e, e.r(3), e.cstr(e.r(4)) if e.r(4) else "")

    def assign(e):
        tds_set(e, e.r(3), tds_text(e, e.r(4)))

    def append(e):
        tds_set(e, e.r(3), tds_text(e, e.r(3)) + tds_text(e, e.r(4)))

    def wctor0(e):
        wds_set(e, e.r(3), "")

    def wctor(e):
        wds_set(e, e.r(3), wide_text(e, e.r(4)))

    return {
        "__ct__26TbDynamicStringTemplate<c>Fv": ctor0,
        "__ct__26TbDynamicStringTemplate<c>FPCc": ctor,
        "__dt__26TbDynamicStringTemplate<c>Fv": lambda e: None,
        "__as__26TbDynamicStringTemplate<c>FRC26TbDynamicStringTemplate<c>": assign,
        "__apl__26TbDynamicStringTemplate<c>FRC15TbStringBase<c>": append,
        "__ct__26TbDynamicStringTemplate<w>Fv": wctor0,
        "__ct__26TbDynamicStringTemplate<w>FPCw": wctor,
        "__dt__26TbDynamicStringTemplate<w>Fv": lambda e: None,
    }


def recorder(name, spec="", ret=None, then=None):
    """A PowerPC fake that records `name` with the arguments `spec` describes (see the
    module comment). ret: None, "i" (the queue `name`, 32-bit), "b" (8-bit), "h" (16-bit)
    or a function (e, log) -> value for r3. then(e, log) runs after recording."""
    def stub(e):
        L = e.cl
        ints, texts = [], []
        g, f = 3, 1
        delete = None
        for code in spec:
            if code == "f":
                ints.append(fbits(e.f(f)))
                f += 1
                g += 1
                continue
            v = e.r(g)
            g += 1
            if code == "L":
                ints.append(L.lab(v))
            elif code == "N":
                ints.append(L.newlab(v))
            elif code == "i":
                ints.append(T.s32(v))
            elif code == "u":
                ints.append(v & 0xFFFFFFFF)
            elif code == "h":
                ints.append(T.s16(v))
            elif code == "H":
                ints.append(v & 0xFFFF)
            elif code == "b":
                ints.append(v & 0xFF)
            elif code == "w":
                ints.append((v >> 16) & 0xFFFF)
            elif code == "s":
                texts.append(e.cstr(v)[:159] if v else "(null)")
            elif code == "S":
                texts.append(wide_text(e, v) if v else "(null)")
            elif code == "T":
                texts.append(tds_text(e, v))
            elif code == "W":
                texts.append(wds_text(e, v))
            elif code == "x":
                pass
            elif code == "D":
                ints.append(L.lab(v))
                flag = T.s16(e.r(g))
                g += 1
                delete = v if flag > 0 else None
            else:
                raise ValueError(f"bad spec code {code!r}")
        L.rec(name, ints, texts)
        if delete is not None:
            if L.lab(delete) > 0:
                L.rec("delete", (L.lab(delete),))
            L.forget(delete)
        if ret == "i":
            e.set_r(3, T.u32(L.ret(name)))
        elif ret == "b":
            e.set_r(3, L.ret(name) & 0xFF)
        elif ret == "h":
            e.set_r(3, T.u32(L.ret(name)))
        elif callable(ret):
            v = ret(e, L)
            if v is not None:
                e.set_r(3, T.u32(v))
        if then is not None:
            then(e, L)
    return stub


class Stubs:
    """The PowerPC fakes of a unit: imports by name, in-binary functions by address.
    `real` lists in-binary addresses that must run as the original (the unit's own
    functions), so stubs other modules left on them are lifted during a case."""

    def __init__(self):
        self.imports, self.internals, self.real = {}, {}, set()

    def imp(self, name, fn):
        self.imports[name] = fn

    def internal(self, addr, fn):
        self.internals[addr] = fn

    def activate(self, emu):
        if not hasattr(emu, "internal_stubs"):
            emu.internal_stubs, emu.internal_hooks = {}, {}
        saved = (dict(emu.stubs), dict(emu.internal_stubs))
        emu.stubs.update(self.imports)
        for addr, fn in self.internals.items():
            if addr in emu.internal_hooks:
                emu.internal_stubs[addr] = fn
            else:
                T.stub_internal(emu, addr, fn)
        for addr in self.real:
            emu.internal_stubs.pop(addr, None)
        return saved

    @staticmethod
    def deactivate(emu, saved):
        emu.stubs.clear()
        emu.stubs.update(saved[0])
        emu.internal_stubs.clear()
        emu.internal_stubs.update(saved[1])


# ----------------------------------------------------------------- host side


def host_begin(lib, fixed, queues=None, first_new=FIRST_NEW):
    """fixed: label -> host address; queues: key -> list of values."""
    lib.tpw_cl_reset.argtypes = [C.c_int32]
    lib.tpw_cl_label.argtypes = [C.c_int32, C.c_void_p]
    lib.tpw_cl_queue.argtypes = [C.c_char_p, C.c_int64]
    lib.tpw_cl_reset(first_new)
    for label, p in fixed.items():
        if p:
            lib.tpw_cl_label(label, p)
    for key, values in (queues or {}).items():
        for v in values:
            lib.tpw_cl_queue(key.encode(), v)
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 1


def host_end(lib):
    """Stops recording and returns the log in the PowerPC side's shape."""
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    n = C.c_int32.in_dll(lib, "tpw_cl_n").value
    try:
        capacity = C.c_int32.in_dll(lib, "tpw_cl_capacity").value
    except ValueError:
        capacity = 4096
    log = (TpwCallEvent * capacity).in_dll(lib, "tpw_cl_log")
    out = []
    for i in range(min(n, capacity)):
        ev = log[i]
        out.append((ev.name.decode("latin-1"), tuple(ev.ints[k] for k in range(ev.nints)),
                    tuple(ev.texts[k].value.decode("latin-1") for k in range(ev.ntexts))))
    if n > capacity:
        out.append(("OVERFLOW", (n,), ()))
    return out


def first_difference(a, b):
    """A short description of where two logs part, for failure messages."""
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return f"event {i}: ppc={x} host={y} (previous: {a[i - 1] if i else None})"
    if len(a) != len(b):
        i = min(len(a), len(b))
        extra = a[i] if len(a) > i else b[i]
        return f"lengths {len(a)}/{len(b)}; first extra event {i}: {extra} on the {'ppc' if len(a) > i else 'host'} side"
    return None
