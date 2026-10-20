"""Differential tests for bullfrog LbLog.cpp. See difftest.py and tpwtest.py.

TbLog holds no pointers, so both sides use the same 0x134-byte image (big-endian words on
the PowerPC). Callees in other bullfrog files are fakes with the same behaviour on both
sides: LbFile_ToProjectFilename writes the case's project name (cut to size - 1) and
returns the case's result, LbFile_IsDirectory and LbFile_Delete answer from the case,
LbTime_GetTime/GetDate return the case's values; all of them log their calls. On the
PowerPC, LbString_strnzcpy is the original code (with strlen/memcpy stubs); on the host a
placeholder with the same behaviour.

Files: the PowerPC's fopen/fprintf/vfprintf/fclose are an in-memory file system (a name
with a directory part fails to open, as "nodir/..." does on the host) formatting with
tpwtest.c_format; the host uses the real C library in a temporary directory. Both sides
start from the case's files and compare their contents afterwards. Messages use only
what c_format supports (no floating point), and prefixes contain no conversion other
than "%%" (the original passes the prefix to fprintf as the format, with no arguments).
"""
import atexit
import ctypes
import os
import shutil
import struct
import tempfile

import tpwtest as T
SHIM_LIBRARY = "legacy_testshim"
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"

ADDR = dict(project=0x10001590, isdir=0x1000099c, delete=0x10000a20, time=0x100468e4, date=0x10046968)
HANDLE = 0x7C0FE000
NAMES = ["log.txt", "other.txt"]
VP, I32, U32, U64 = ctypes.c_void_p, ctypes.c_int32, ctypes.c_uint32, ctypes.c_uint64
TMP = tempfile.mkdtemp(prefix="tpw_lblog_", dir=os.environ.get("TPW_TMP"))
atexit.register(shutil.rmtree, TMP, True)


def F(lib, name, res, *args):
    f = lib[name]
    f.restype = res
    f.argtypes = list(args)
    return f


class TbLogC(ctypes.Structure):
    _fields_ = [("init", I32), ("filename", ctypes.c_ubyte * 0x104), ("prefix", ctypes.c_ubyte * 0x20),
                ("flags", U32), ("opened", I32), ("suspended", I32)]


def pack_log(o, big):
    order = ">" if big else "<"
    return (struct.pack(order + "i", o["init"]) + o["filename"] + o["prefix"] +
            struct.pack(order + "Iii", o["flags"], o["opened"], o["suspended"]))


def unpack_log(raw, big):
    order = ">" if big else "<"
    init, = struct.unpack(order + "i", raw[0:4])
    flags, opened, suspended = struct.unpack(order + "Iii", raw[0x128:0x134])
    return dict(init=init, filename=raw[4:0x108], prefix=raw[0x108:0x128], flags=flags, opened=opened,
                suspended=suspended)

# ----------------------------------------------------------------- PowerPC side


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.lb_log, e.lb_case, e.lb_files, e.lb_open = [], {}, {}, {}

    def project(e):
        res, out, size, name = e.r(3), e.r(4), e.r(5), e.r(6)
        e.lb_log.append((1, e.cstr(name), T.s32(size)))
        text = e.lb_case["project"].encode("latin-1")
        if size:
            text = text[:size - 1]
            e.write(out, text + b"\0")
        e.write(res, struct.pack(">i", e.lb_case["project_result"]))

    def isdir(e):
        e.lb_log.append((2, e.cstr(e.r(3)), 0))
        e.set_r(3, T.u32(e.lb_case["isdir"]))

    def delete(e):
        e.lb_log.append((3, e.cstr(e.r(4)), 0))
        e.write(e.r(3), struct.pack(">i", e.lb_case["delete_result"]))

    def get_time(e):
        e.lb_log.append((4, "", 0))
        e.write(e.r(3), struct.pack(">4i", *e.lb_case["time"]))

    def get_date(e):
        e.lb_log.append((5, "", 0))
        e.write(e.r(3), struct.pack(">4i", *e.lb_case["date"]))

    T.stub_internal(e, ADDR["project"], project)
    T.stub_internal(e, ADDR["isdir"], isdir)
    T.stub_internal(e, ADDR["delete"], delete)
    T.stub_internal(e, ADDR["time"], get_time)
    T.stub_internal(e, ADDR["date"], get_date)

    def fopen(e):
        name, mode = e.cstr(e.r(3)), e.cstr(e.r(4))
        if "/" in name:
            e.set_r(3, 0)
            return
        if mode == "w" or e.lb_files.get(name) is None:
            e.lb_files[name] = b""
        h = HANDLE + 0x10 * len(e.lb_open)
        e.lb_open[h] = name
        e.set_r(3, h)

    def write(e, h, text):
        name = e.lb_open[h]
        e.lb_files[name] += text.encode("latin-1")

    def fprintf(e):
        write(e, e.r(3), T.c_format(e, e.cstr(e.r(4)), 5))

    def vfprintf(e):
        write(e, e.r(3), T.c_format(e, e.cstr(e.r(4)), 3, va=e.r(5)))

    def fclose(e):
        e.lb_open[e.r(3)] = None
        e.set_r(3, 0)

    e.stub("fopen", fopen)
    e.stub("fprintf", fprintf)
    e.stub("vfprintf", vfprintf)
    e.stub("fclose", fclose)


def ppc_begin(e, c):
    e.lb_log, e.lb_case, e.lb_open = [], c, {}
    e.lb_files = {n: c["files"].get(n) for n in NAMES}
    return e.alloc(0x134, pack_log(c["log"], True))


def ppc_end(e, c, obj, ret):
    out = dict(log=unpack_log(e.read(obj, 0x134), True), calls=e.lb_log, files=dict(e.lb_files))
    if ret is not None:
        out["ret"] = ret
    return out


def ppc_string(e, s):
    return 0 if s is None else e.alloc(len(s) + 1, s.encode("latin-1") + b"\0")

# ----------------------------------------------------------------- host side


def host_begin(lib, c):
    I32.in_dll(lib, "tpw_bf_log_n").value = 0
    project = c.get("project", "").encode("latin-1")
    ctypes.memmove(ctypes.addressof(ctypes.c_char.in_dll(lib, "tpw_bf_project")), project + b"\0", len(project) + 1)
    I32.in_dll(lib, "tpw_bf_project_result").value = c.get("project_result", 0)
    I32.in_dll(lib, "tpw_bf_isdir").value = c.get("isdir", 0)
    I32.in_dll(lib, "tpw_bf_delete_result").value = c.get("delete_result", 0)
    (I32 * 4).in_dll(lib, "tpw_bf_time")[:] = c.get("time", [0] * 4)
    (I32 * 4).in_dll(lib, "tpw_bf_date")[:] = c.get("date", [0] * 4)
    for n in os.listdir(TMP):
        os.remove(os.path.join(TMP, n))
    for n, data in c["files"].items():
        if data is not None:
            with open(os.path.join(TMP, n), "wb") as f:
                f.write(data)
    log = TbLogC.from_buffer_copy(pack_log(c["log"], False))
    return log


def host_calls(lib):
    n = I32.in_dll(lib, "tpw_bf_log_n").value

    class Call(ctypes.Structure):
        _fields_ = [("id", I32), ("text", ctypes.c_char * 0x120), ("number", I32)]
    log = (Call * 16).in_dll(lib, "tpw_bf_log")
    return [(log[i].id, log[i].text.decode("latin-1"), log[i].number) for i in range(n)]


def host_end(lib, c, log, ret):
    files = {}
    for n in NAMES:
        p = os.path.join(TMP, n)
        files[n] = open(p, "rb").read() if os.path.exists(p) else None
    out = dict(log=unpack_log(bytes(log), False), calls=host_calls(lib), files=files)
    if ret is not None:
        out["ret"] = ret
    return out


def in_tmp(fn):
    cwd = os.getcwd()
    os.chdir(TMP)
    try:
        return fn()
    finally:
        os.chdir(cwd)

# ----------------------------------------------------------------- tests


def simple(mangled, sym, kind):
    """Functions of the log alone: kind "error" returns a TbError (r3 = buffer, r4 =
    log), "int" returns an int (r3 = log)."""
    def ppc(e, c):
        obj = ppc_begin(e, c)
        if kind == "error":
            res = e.alloc(4, b"\xA5" * 4)
            T.call_ppc(e, e.func(mangled), [T.I(res), T.I(obj)])
            return ppc_end(e, c, obj, T.s32(e.u32(res)))
        r3, _, _ = T.call_ppc(e, e.func(mangled), [T.I(obj)])
        return ppc_end(e, c, obj, T.s32(r3))

    def host(lib, c):
        log = host_begin(lib, c)
        r = F(lib, sym, I32, VP)(ctypes.byref(log))
        return host_end(lib, c, log, r)
    return ppc, host


def setup_ppc(e, c):
    obj = ppc_begin(e, c)
    res, name = e.alloc(4, b"\xA5" * 4), ppc_string(e, c["name"])
    T.call_ppc(e, e.func("LbLog_Setup__FP5TbLogPCcUl"), [T.I(res), T.I(obj), T.I(name), T.I(c["flags"])])
    return ppc_end(e, c, obj, T.s32(e.u32(res)))


def setup_host(lib, c):
    log = host_begin(lib, c)
    r = F(lib, "tpw_LbLog_Setup", I32, VP, ctypes.c_char_p, U32)(ctypes.byref(log), c["name"].encode("latin-1"),
                                                                  c["flags"])
    return host_end(lib, c, log, r)


def prefix_ppc(e, c):
    obj = ppc_begin(e, c)
    res = e.alloc(4, b"\xA5" * 4)
    T.call_ppc(e, e.func("LbLog_SetPrefix__FP5TbLogPCc"), [T.I(res), T.I(obj), T.I(ppc_string(e, c["prefix"]))])
    return ppc_end(e, c, obj, T.s32(e.u32(res)))


def prefix_host(lib, c):
    log = host_begin(lib, c)
    p = None if c["prefix"] is None else c["prefix"].encode("latin-1")
    r = F(lib, "tpw_LbLog_SetPrefix", I32, VP, ctypes.c_char_p)(ctypes.byref(log), p)
    return host_end(lib, c, log, r)


def lblog_ppc(e, c):
    obj = ppc_begin(e, c)
    res = e.alloc(4, b"\xA5" * 4)
    args = [T.I(ppc_string(e, v) if k == "s" else v) for k, v in c["args"]]
    T.call_ppc(e, e.func("LbLog__FP5TbLogPCce"), [T.I(res), T.I(obj), T.I(ppc_string(e, c["fmt"]))] + args)
    return ppc_end(e, c, obj, T.s32(e.u32(res)))


def lblog_host(lib, c):
    log = host_begin(lib, c)
    keep = []
    vals = []
    for k, v in c["args"]:
        if k == "s":
            b = ctypes.create_string_buffer(v.encode("latin-1"))
            keep.append(b)
            vals.append(ctypes.addressof(b))
        else:
            vals.append(T.u32(v))
    vals += [0] * (6 - len(vals))
    f = F(lib, "tpw_LbLog", I32, VP, ctypes.c_char_p, *[U64] * 6)
    r = in_tmp(lambda: f(ctypes.byref(log), c["fmt"].encode("latin-1"), *vals))
    return host_end(lib, c, log, r)

# ----------------------------------------------------------------- cases


def text(r, n, alphabet="abcXYZ019 _-.:\\/"):
    return "".join(r.choice(alphabet) for _ in range(n))


def cstr_field(s, size, r=None):
    raw = s.encode("latin-1")[:size - 1] + b"\0"
    tail = r.randbytes(size - len(raw)) if r else bytes(size - len(raw))
    return raw + tail


def rlog(r, filename=None, prefix=None):
    return dict(init=r.choice([0, 1, 1, 1, 5, -1]), filename=cstr_field(filename or r.choice(["log.txt", "other.txt",
                                                                                              "nodir/log.txt"]), 0x104, r),
                prefix=cstr_field(prefix if prefix is not None else rprefix(r), 0x20, r),
                flags=r.getrandbits(6) | (r.getrandbits(32) & ~0x3F if r.random() < 0.3 else 0),
                opened=r.choice([0, 0, 1, 2]), suspended=r.choice([0, 0, 0, 1, 7]))


def rprefix(r):
    k = r.random()
    if k < 0.4:
        return ""
    if k < 0.8:
        return text(r, r.randint(1, 12), "abc[]<> 019:") + " "
    return "100%% " + text(r, r.randint(0, 5))


FORMATS = [
    ("hello\n", ""), ("%d\n", "i"), ("value=%d name=%s\n", "is"), ("%x %X %u %c|\n", "iiii"),
    ("[%5d] [%-5d] [%05d] [%.3s]\n", "iiis"), ("%s%s%s\n", "sss"), ("100%%\n", ""), ("no newline", ""),
    ("a=%i b=%i c=%i d=%i e=%i f=%i\n", "iiiiii"), ("%s: %d of %d\n", "sii"), ("", ""),
]


def rargs(r, kinds):
    out = []
    for k in kinds:
        if k == "s":
            out.append(("s", text(r, r.randint(0, 10), "abcdefXYZ 0123")))
        else:
            out.append(("i", T.rint(r) if k == "i" and r.random() < 0.7 else r.randint(32, 126)))
    return out


def rtime(r):
    return [r.choice([0, 5, 9, 10, 23, 59, -1, -12, 100, 12345]) if r.random() < 0.6 else T.s32(r.getrandbits(32))
            for _ in range(4)]


def rfiles(r):
    return {n: r.choice([None, b"", b"existing line\n", r.randbytes(r.randint(1, 20))]) for n in NAMES}


def lblog_case(r):
    fmt, kinds = r.choice(FORMATS)
    return dict(log=rlog(r), fmt=fmt, args=rargs(r, kinds), time=rtime(r), date=rtime(r), files=rfiles(r))


def lblog_edges():
    out = []
    for flags in range(64):
        for opened in (0, 1):
            log = dict(init=1, filename=cstr_field("log.txt", 0x104), prefix=cstr_field("[p] ", 0x20), flags=flags,
                       opened=opened, suspended=0)
            out.append(dict(log=log, fmt="msg %d %s\n", args=[("i", -42), ("s", "abc")], time=[1, 2, 3, 4],
                            date=[31, 12, 1999, 5], files={"log.txt": b"old\n", "other.txt": None}))
    for init, susp, name in ((0, 0, "log.txt"), (1, 1, "log.txt"), (1, 0, "nodir/log.txt"), (-1, 0, "other.txt")):
        log = dict(init=init, filename=cstr_field(name, 0x104), prefix=cstr_field("", 0x20), flags=0x3F, opened=0,
                   suspended=susp)
        out.append(dict(log=log, fmt="x\n", args=[], time=[0] * 4, date=[0] * 4, files={"log.txt": None, "other.txt": None}))
    return out


PROJECTS = ["log.txt", "dir\\log.txt", "dir/log.txt", "Mac HD:TPW:log.txt", "a\\b/c:d.txt", "a:b/c\\d.txt",
            "\\log.txt", "/log.txt", ":log.txt", "", "dir\\", "x" * 300 + "\\y.txt", "x" * 250 + ":" + "y" * 30]


def setup_case(r):
    project = r.choice(PROJECTS) if r.random() < 0.6 else text(r, r.randint(0, 40))
    return dict(log=dict(init=T.s32(r.getrandbits(32)), filename=r.randbytes(0x104), prefix=r.randbytes(0x20),
                         flags=r.getrandbits(32), opened=T.s32(r.getrandbits(32)), suspended=T.s32(r.getrandbits(32))),
                name=text(r, r.randint(0, 20)), flags=r.getrandbits(32), project=project,
                project_result=r.choice([0, 0, 0, -1, 1, 0x7FFFFFFF, -0x80000000]),
                isdir=r.choice([0x10, 0x10, 0, 1, -1]), files={})


def setup_edges():
    out = []
    for p in PROJECTS:
        for pr in (0, -1, 1):
            for isdir in (0, 0x10):
                out.append(dict(log=dict(init=7, filename=bytes([0x41]) * 0x104, prefix=bytes([0x42]) * 0x20,
                                         flags=0xDEADBEEF, opened=9, suspended=9),
                                name="game.log", flags=0x3F, project=p, project_result=pr, isdir=isdir, files={}))
    return out


def prefix_case(r):
    k = r.random()
    if k < 0.15:
        p = None
    elif k < 0.5:
        p = text(r, r.choice([0, 1, 29, 30, 31, 32, 33, 40]))
    else:
        p = text(r, r.randint(0, 40))
    return dict(log=rlog(r), prefix=p, files={})


def delete_case(r):
    return dict(log=rlog(r), delete_result=r.choice([0, -1, 1, 0x7FFFFFFF, -0x80000000, -2]), files={})


def state_case(r):
    return dict(log=rlog(r), files={})


def state_edges():
    return [dict(log=dict(init=i, filename=cstr_field("log.txt", 0x104), prefix=cstr_field("", 0x20), flags=3,
                          opened=o, suspended=s), files={}, delete_result=d)
            for i in (0, 1, -1) for s in (0, 1) for o in (0, 1) for d in (0, -1)]


TESTS = [
    FnTest("LbLog_Setup", setup_ppc, setup_host, edge=setup_edges, rnd=setup_case),
    FnTest("LbLog", lblog_ppc, lblog_host, edge=lblog_edges, rnd=lblog_case),
    FnTest("LbLog_Close", *simple("LbLog_Close__FP5TbLog", "tpw_LbLog_Close", "error"), edge=state_edges, rnd=state_case),
    FnTest("LbLog_SetPrefix", prefix_ppc, prefix_host,
           edge=lambda: [dict(log=dict(e["log"]), prefix=p, files={}) for e in state_edges()
                         for p in (None, "", "x" * 30, "y" * 31, "z" * 32, "w" * 40)], rnd=prefix_case),
    FnTest("LbLog_Delete", *simple("LbLog_Delete__FP5TbLog", "tpw_LbLog_Delete", "error"), edge=state_edges,
           rnd=delete_case),
    FnTest("LbLog_Suspend", *simple("LbLog_Suspend__FP5TbLog", "tpw_LbLog_Suspend", "error"), edge=state_edges,
           rnd=state_case),
    FnTest("LbLog_Restart", *simple("LbLog_Restart__FP5TbLog", "tpw_LbLog_Restart", "error"), edge=state_edges,
           rnd=state_case),
    FnTest("LbLog_IsInitialised", *simple("LbLog_IsInitialised__FP5TbLog", "tpw_LbLog_IsInitialised", "int"),
           edge=state_edges, rnd=state_case),
    FnTest("LbLog_IsSuspended", *simple("LbLog_IsSuspended__FP5TbLog", "tpw_LbLog_IsSuspended", "int"),
           edge=state_edges, rnd=state_case),
]
