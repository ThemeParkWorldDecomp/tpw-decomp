"""Shared controlled callees for the main-app Clock/Cheat differential tests.

These are boundary fakes, not translations of the time, message or file libraries.
Time calls XOR every word of the addressed subobject and return an independent word;
constructors are controlled too. File reads may change a byte even on failure, and
the aggregate status is independent of the transferred count. Both sides record
arguments, subobject state before calls and the complete call/allocator order.
"""
import ctypes
import struct

import tpwtest as T

VP, I32, U32, U8 = ctypes.c_void_p, ctypes.c_int32, ctypes.c_uint32, ctypes.c_uint8
CLOCK, CENTRE, MSG_VTABLE = 0x1037F048, 0x1037F6C4, 0x102A4D5C

# id, label, object words, has file argument
CALLEES = [
    (1, "ReadSaveableTime__13CSaveableTimeFv", 17, False),
    (2, "ReadSaveableFixedTime__18CSaveableFixedTimeFv", 8, False),
    (3, "Pause__13CPausableTimeFv", 11, False),
    (4, "Pause__18CPausableFixedTimeFv", 6, False),
    (5, "UnPause__13CPausableTimeFv", 11, False),
    (6, "UnPause__18CPausableFixedTimeFv", 6, False),
    (7, "TogglePause__13CPausableTimeFv", 11, False),
    (8, "TogglePause__18CPausableFixedTimeFv", 6, False),
    (9, "GetPauseFlag__13CPausableTimeFv", 11, False),
    (10, "Save__13CSaveableTimeFPv", 17, True),
    (11, "Save__18CSaveableFixedTimeFPv", 8, True),
    (12, "Load__13CSaveableTimeFPv", 17, True),
    (13, "Load__18CSaveableFixedTimeFPv", 8, True),
    (14, "AdjustTime__13CSaveableTimeFv", 17, False),
    (15, "AdjustTime__18CSaveableFixedTimeFv", 8, False),
    (16, "__ct__13CVariableTimeFv", 8, False),
    (17, "__ct__10CFixedTimeFv", 3, False),
]


def F(lib, name, result, *args):
    f = lib[name]
    f.restype, f.argtypes = result, list(args)
    return f


class Event(ctypes.Structure):
    _fields_ = [("kind", I32), ("allocBefore", I32), ("object", VP), ("argument", VP),
                ("value", U32), ("count", U32), ("words", U32 * 17), ("context", U32 * 25),
                ("text", ctypes.c_char * 64)]


def host_config(lib, c):
    VP.in_dll(lib, "tpw_clock_context").value = None
    for name, value in (("tpw_clock_log_on", 0), ("tpw_alloc_log_on", 0), ("tpw_alloc_n", 0),
                        ("tpw_clock_log_n", 0), ("tpw_cheat_io_i", 0)):
        I32.in_dll(lib, name).value = value
    for name, typ, values in (
            ("tpw_clock_returns", I32, c.get("returns", [0] * 32)),
            ("tpw_clock_masks", U32, c.get("masks", [0] * 32)),
            ("tpw_cheat_subscription_words", U32, c.get("sub", [0] * 4)),
            ("tpw_cheat_io_count", U32, c.get("counts", [1, 1])),
            ("tpw_cheat_io_result", U32, c.get("results", [1, 1])),
            ("tpw_cheat_io_data", U8, c.get("data", [0, 0])),
            ("tpw_cheat_io_write", U8, c.get("writes", [1, 1]))):
        a = (typ * len(values)).in_dll(lib, name)
        a[:] = values
    U32.in_dll(lib, "tpw_cheat_msg_flags").value = c.get("flags", 0)
    U32.in_dll(lib, "tpw_cheat_post_mask").value = c.get("post_mask", 0)


def host_start(lib):
    I32.in_dll(lib, "tpw_clock_log_on").value = 1
    I32.in_dll(lib, "tpw_alloc_log_on").value = 1


def host_stop(lib):
    I32.in_dll(lib, "tpw_clock_log_on").value = 0
    I32.in_dll(lib, "tpw_alloc_log_on").value = 0


def label(a, bases):
    if not a:
        return None
    for name, start, size in bases:
        if start <= a < start + size:
            return (name, a - start)
    return ("other", a)


def host_trace(lib, bases):
    """Merge allocator calls with placeholder calls using each event's allocBefore."""
    n = I32.in_dll(lib, "tpw_clock_log_n").value
    events = (Event * 256).in_dll(lib, "tpw_clock_log")
    an = I32.in_dll(lib, "tpw_alloc_n").value
    ak = (I32 * 1024).in_dll(lib, "tpw_alloc_kind")
    ap = (ctypes.c_uint64 * 1024).in_dll(lib, "tpw_alloc_ptr")
    az = (ctypes.c_uint64 * 1024).in_dll(lib, "tpw_alloc_size")
    out, ai = [], 0

    def alloc_until(end):
        nonlocal ai
        while ai < end:
            out.append(("new" if ak[ai] == 1 else "delete", label(ap[ai], bases), az[ai]))
            ai += 1

    message = next((events[i].object for i in range(n) if events[i].kind == 18), None)
    if message:
        bases = bases + [("message", message, 1)]
    for i in range(n):
        e = events[i]
        alloc_until(e.allocBefore)
        obj, arg = label(e.object, bases), label(e.argument, bases)
        if e.kind <= 17:
            out.append((e.kind, obj, arg, list(e.words[:e.count]), list(e.context)))
        elif e.kind == 18:
            out.append((18, obj, e.value))
        elif e.kind == 19:
            out.append((19, obj))
        elif e.kind == 20:
            out.append((20, obj, arg, e.value, list(e.words[:2])))
        elif e.kind == 21:
            out.append((21, bytes(e.text).decode()))
        elif e.kind == 22:
            out.append((22, obj, arg, e.count))
        elif e.kind == 23:
            # Save uses one temporary byte, rather than an address inside the object.
            arg = ("temporary", 0) if arg and arg[0] == "other" else arg
            out.append((23, obj, arg, e.value, e.count))
    alloc_until(an)
    return out


def ppc_config(emu, c):
    emu.clock_case, emu.clock_trace, emu.clock_bases, emu.clock_io_i = c, [], [], 0
    emu.clock_context = None
    emu.alloc_log, emu.deletes, emu.news, emu.registered = [], [], [], []


def ppc_label(e, a):
    return label(a, e.clock_bases)


def time_stub(kind, words, file_arg):
    def run(e):
        obj, file = e.r(3), e.r(4) if file_arg else 0
        values = list(struct.unpack(">%dI" % words, e.read(obj, words * 4)))
        context = list(struct.unpack(">25I", e.read(e.clock_context, 100))) if e.clock_context else [0] * 25
        e.clock_trace.append((kind, ppc_label(e, obj), ppc_label(e, file), values, context))
        mask = e.clock_case.get("masks", [0] * 32)[kind]
        e.write(obj, struct.pack(">%dI" % words, *[v ^ mask for v in values]))
        e.set_r(3, T.u32(e.clock_case.get("returns", [0] * 32)[kind]))
    return run


def msg_ctor(e):
    obj, status = e.r(3), e.r(4)
    e.clock_bases.append(("message", obj, 1))
    e.clock_trace.append((18, ppc_label(e, obj), status))
    e.write(obj, struct.pack(">II", MSG_VTABLE, e.clock_case.get("flags", 0)) + bytes([status]))
    e.set_r(3, obj)


def subscription_ctor(e):
    obj = e.r(3)
    e.clock_trace.append((19, ppc_label(e, obj)))
    e.write(obj, struct.pack(">4I", *e.clock_case.get("sub", [0] * 4)))
    e.set_r(3, obj)


def post(e):
    obj, msg = e.r(3), e.r(4)
    assert e.u32(msg) == MSG_VTABLE, "PostMessage received the wrong vtable"
    e.clock_trace.append((20, ppc_label(e, obj), ppc_label(e, msg), e.read(msg + 8, 1)[0],
                          [e.u32(msg + 4), 0x15]))
    mask = e.clock_case.get("post_mask", 0)
    values = struct.unpack(">116I", e.read(obj, 0x1D0))
    e.write(obj, struct.pack(">116I", *[v ^ mask for v in values]))
    e.set_r(3, T.u32(e.clock_case.get("returns", [0] * 32)[20]))


def op_new(e):
    size = e.r(3)
    a = e.alloc(max(size, 4), b"\xcd" * max(size, 4))
    e.clock_bases.append(("centre", a, size))
    e.clock_trace.append(("new", ppc_label(e, a), size))
    e.set_r(3, a)


def op_delete(e):
    e.clock_trace.append(("delete", ppc_label(e, e.r(3)), 0))
    e.deletes.append(e.r(3))


def construct(e, call):
    block, ctor, dtor, size, count = (e.r(i) for i in range(3, 8))
    # Validate all runtime-helper arguments even though native C++ constructs the
    # array directly. Each element constructor is compared in the ordered trace.
    assert e.u32(ctor) == e.func("__ct__19CSubscriptionHeaderFv")
    assert e.u32(dtor) == e.func("__dt__19CSubscriptionHeaderFv")
    assert (size, count) == (0x10, 0x1D)
    for i in range(count):
        call(ctor, block + i * size, 1)


def io_stub(write):
    def run(e):
        result, file, buf, size, transferred = (e.r(i) for i in range(3, 8))
        i, c = e.clock_io_i, e.clock_case
        e.clock_io_i += 1
        if write:
            where = ppc_label(e, buf)
            where = ("temporary", 0) if where and where[0] == "other" else where
            e.clock_trace.append((23, ppc_label(e, file), where, e.read(buf, 1)[0], size))
        else:
            e.clock_trace.append((22, ppc_label(e, file), ppc_label(e, buf), size))
            if c.get("writes", [1, 1])[i]:
                e.write(buf, bytes([c.get("data", [0, 0])[i]]))
        e.write(transferred, struct.pack(">I", c.get("counts", [1, 1])[i]))
        e.write(result, struct.pack(">I", c.get("results", [1, 1])[i]))
    return run


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    for kind, name, words, file_arg in CALLEES:
        T.stub_internal(e, name, time_stub(kind, words, file_arg))
    T.stub_internal(e, "__ct__13CMsgCheatModeFb", msg_ctor)
    T.stub_internal(e, "__ct__19CSubscriptionHeaderFv", subscription_ctor)
    T.stub_internal(e, "PostMessage__14CMessageCentreFRC4CMsg", post)
    T.stub_internal(e, "UDebug__13SamsUtilitiesFe",
                    lambda e: e.clock_trace.append((21, e.cstr(e.r(3)))))
    e.stub("__nw__FUl", op_new)
    e.stub("__dl__FPv", op_delete)
    T.stub_cb(e, "__construct_array", construct)
    e.stub("LbFile_Read__FPvPvUlPUl", io_stub(False))
    e.stub("LbFile_Write__FPvPCvUlPUl", io_stub(True))
