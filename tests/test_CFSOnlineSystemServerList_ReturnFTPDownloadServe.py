"""Unit v51110, the server and ride lists' search members, destructors and LoggedInGetList, the FS_OnlineList static initialiser and three
one-liners, against libtpw_unit_CFSOnlineSystemServerList_ReturnFTPDownloadServe.dylib
(tests/unit_CFSOnlineSystemServerList_ReturnFTPDownloadServe.cpp), main app ("SimTheme Park"),
src/game/online/FS_Online.cpp, src/game/online/FS_OnlineList.cpp, src/game/ui/ui_main.cpp:
  ReturnFTPDownloadServer 0x1017b0b4, ReturnUploadServer 0x1017b134, ~CFSOnlineSystemServerList 0x1017a908,
  ride base ~CFSOnlineSystemList 0x1017b288, ~CFSOnlineSystemRideList 0x1017b4a4, server LoggedInGetList 0x1017ab30,
  ride LoggedInGetList 0x1017b6c8, ReturnRideInfo 0x1017bacc, __sinit_FS_OnlineList_cpp 0x1017ddb4, CFSOnlineRide::GetFilePath 0x10176c90,
  CFSOnlineSystem::GetAvatarID 0x101719bc, UI_ByeByeBankrupt 0x10187220

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at persistent addresses (big-endian)
for the original, and in the host library's arena (little-endian objects built through shims) for the translation. The callees outside the
unit are fakes on both sides that record the same events: the list's RemoveHead (answers an item of the case and counts the list down),
the record and list destructors reached through the vtable glue (flag 1), __destroy_new_array, operator delete, the online system's
constructor, Instance (answers the global), Logout and city connection, AddUIError, the list callback, the string constructor and
__register_global_object. The lists' virtual slots ChildLocked and RequestListFromServer are fakes too (a vtable of fake code addresses on
the PowerPC side, a derived class on the host).

The world of a case: the list's ten words, its TbPtrList count and nodes, the stored answer (present or not, with infos or not, the
reference count), up to four records, the online system global. What is compared: the answer (the record found, as an index), the call
log, the list's words, count and stored-answer state and the system global.

No fused operations. Not tested: a NULL list or NULL mList (the original reads them unconditionally); a destructor on a NULL this (the
original returns without work; the host cannot call a member on NULL). The delete flag above 0 frees the object through the same fake on
the host (the compiler's deleting destructor, written in the shim): the PowerPC side runs the original's flag test.
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemServerList_ReturnFTPDownloadServe"
MASK = 0xFFFFFFFF

F_FTP, F_UP, F_SRVDTOR, F_RIDEBASE, F_RIDEDTOR = 0x1017b0b4, 0x1017b134, 0x1017a908, 0x1017b288, 0x1017b4a4
F_SRVLOGIN, F_RIDELOGIN, F_RIDEINFO, F_SINIT = 0x1017ab30, 0x1017b6c8, 0x1017bacc, 0x1017ddb4
F_FILEPATH, F_AVATAR, F_BANKRUPT = 0x10176c90, 0x101719bc, 0x10187220
SIZES = {F_FTP: 52, F_UP: 52, F_SRVDTOR: 300, F_RIDEBASE: 288, F_RIDEDTOR: 300, F_SRVLOGIN: 412, F_RIDELOGIN: 412, F_RIDEINFO: 64,
         F_SINIT: 116, F_FILEPATH: 40, F_AVATAR: 8, F_BANKRUPT: 4}
OWN = set(SIZES)

# glue and in-binary callees (faked on both sides)
G_DESTROY_ARRAY, G_DELETE, G_STRCTOR, G_REGISTER = 0x10238ad0, 0x10238218, 0x10238800, 0x10239940
I_SYSCTOR, I_INSTANCE, I_LOGOUT, I_CITY, I_ADDERR = 0x1016f7d0, 0x10178618, 0x10171398, 0x1017346c, 0x101f1df0
I_REMOVE_RIDE, I_REMOVE_SERVER = 0x10177758, 0x1017782c
# code addresses the fake vtables point at (functions of the binary that this module never runs)
C_CHILD, C_REQ, C_CALLBACK, C_ITEM, C_LIST = 0x1017afec, 0x1017af40, 0x1017c7d0, 0x1017c464, 0x1017bf30

ADDR = dict(sys=0x10380ab0, str1=0x10380b14, str2=0x10380af4, node1=0x10380b28, node2=0x10380b08)
HOST_LAYOUT = [140]

P = {}      # persistent PowerPC objects, made by setup


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def stub_removehead(e):
    L = e.cl
    lst = e.r(3)
    L.rec("RemoveHead", (L.lab(lst),))
    idx = L.ret("RemoveHead")
    cnt = e.u32(lst + 0xC)
    e.write(lst + 0xC, struct.pack(">I", (cnt - 1) & MASK))
    e.set_r(3, P["items"][idx - 1] if idx else 0)


def stub_callback(e):
    L = e.cl
    L.rec("Callback", (e.r(12) & MASK, s32(e.r(3)), s32(e.r(4)), L.lab(e.r(5))))


STUBS.internal(I_SYSCTOR, CL.recorder("SystemCtor", "N"))
STUBS.internal(I_INSTANCE, CL.recorder("Instance", "", ret=lambda e, log: e.u32(ADDR["sys"])))
STUBS.internal(I_LOGOUT, CL.recorder("Logout", "L"))
STUBS.internal(I_CITY, CL.recorder("CityConn", "L", ret="i"))
STUBS.internal(I_ADDERR, CL.recorder("AddUIError", "iL"))
STUBS.internal(C_CHILD, CL.recorder("ChildLocked", "L", ret="i"))
STUBS.internal(C_REQ, CL.recorder("Request", "LLu", ret="i"))
STUBS.internal(C_CALLBACK, stub_callback)
STUBS.internal(C_ITEM, CL.recorder("DestroyItem", "Li"))
STUBS.internal(C_LIST, CL.recorder("DestroyList", "Li"))
STUBS.internal(I_REMOVE_RIDE, stub_removehead)
STUBS.internal(I_REMOVE_SERVER, stub_removehead)
STUBS.internal(G_DESTROY_ARRAY, CL.recorder("DestroyArray", "Lu"))
STUBS.internal(G_DELETE, CL.recorder("Delete", "L"))
STUBS.imp("__ct__26TbDynamicStringTemplate<w>FPCw", CL.recorder("StringCtor", "LS"))
STUBS.imp("__register_global_object", CL.recorder("Register", "LxL"))


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)

    def tv(code):
        return T.persist_alloc(e, 8, struct.pack(">II", code, 0))

    def vt(slots):
        data = b"".join(struct.pack(">I", w) for w in slots)
        return T.persist_alloc(e, len(data), data)

    P["list_vt"] = vt([0, 0, 0, tv(C_CHILD), tv(C_REQ), 0])
    P["blk_vt"] = vt([0, 0, tv(C_LIST)])
    P["item_vt"] = vt([0, 0, tv(C_ITEM)])
    P["cb"] = tv(C_CALLBACK)
    P["list"] = T.persist_alloc(e, 0x40)
    P["blk"] = T.persist_alloc(e, 0x10)
    P["recv"] = T.persist_alloc(e, 0x10)
    P["infos"] = T.persist_alloc(e, 0x10)
    P["sys"] = T.persist_alloc(e, 0x80)
    P["items"] = [T.persist_alloc(e, 0x80) for _ in range(4)]
    P["nodes"] = [T.persist_alloc(e, 0x10) for _ in range(5)]
    P["strobj"] = [T.persist_alloc(e, 0x20) for _ in range(2)]


# ---------------------------------------------------------------------------------------------------------------- the two sides


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def text(self, s):
        data = s.encode("utf-16-be" if self.fmt[0] == ">" else "utf-16-le") + b"\0\0"
        a = self.alloc(len(data))
        self.write(a, data)
        return a


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        fixed = {P["list"]: 1, P["blk"]: 2, P["recv"]: 3, P["infos"]: 4, P["sys"]: 5, 0x7000: 7,
                 ADDR["str1"]: 31, ADDR["str2"]: 32, ADDR["node1"]: 33, ADDR["node2"]: 34}
        for i, a in enumerate(P["items"]):
            fixed[a] = 11 + i
        self.L = e.cl = CL.PpcLog(fixed, case["queues"])
        self.saved = STUBS.activate(e)
        self.obj = P["list"]

    def done(self):
        CL.Stubs.deactivate(self.e, self.saved)

    def alloc(self, n):
        return self.e.alloc(n)

    def read(self, a, n):
        return self.e.read(a, n)

    def write(self, a, data):
        self.e.write(a, data)

    def log(self):
        return self.L.log

    def init_list(self):
        self.words(P["list"], [P["list_vt"]] + [0] * 12)
        self.words(P["list"] + 0x2C, [P["blk"], 0])
        self.words(P["blk"], [P["blk_vt"], 0, 0, 0])

    def set_fields(self, w):
        self.words(self.obj + 4, w)

    def get_fields(self):
        return self.rd_words(self.obj + 4, 10)

    def set_count(self, n):
        self.words(P["blk"] + 0xC, [n])

    def get_count(self):
        return self.rd_word(P["blk"] + 0xC)

    def set_nodes(self, idxs):
        n = len(idxs)
        for i, idx in enumerate(idxs):
            nxt = P["nodes"][i + 1] if i + 1 < n else 0
            prv = P["nodes"][i - 1] if i else 0
            self.words(P["nodes"][i], [P["items"][idx - 1], nxt, prv])
        self.words(P["blk"], [P["blk_vt"], P["nodes"][0] if n else 0, P["nodes"][n - 1] if n else 0, n])

    def set_item(self, idx, kind, a, b, text):
        addr = P["items"][idx - 1]
        self.words(addr, [P["item_vt"]])
        if kind == 0:
            self.words(addr + 0xC, [a])
        else:
            self.words(addr + 0x44, [a])
            self.words(addr + 0x64, [b])
        self.words(addr + 0x54, [text])

    def set_recv(self, has, has_infos, ref):
        self.words(P["recv"], [0, P["infos"] if has_infos else 0, 0, ref])
        self.words(self.obj + 0x30, [P["recv"] if has else 0])

    def get_recv(self):
        return [1 if self.rd_word(self.obj + 0x30) else 0, s32(self.rd_word(P["recv"] + 0xC)), 1 if self.rd_word(P["recv"] + 4) else 0]

    def set_sys(self, preset):
        self.words(ADDR["sys"], [P["sys"] if preset else 0])

    def get_sys(self):
        v = self.rd_word(ADDR["sys"])
        return 0 if v == 0 else 1 if v == P["sys"] else 2

    def item_index(self, ptr):
        return 0 if ptr == 0 else P["items"].index(ptr) + 1 if ptr in P["items"] else -1

    def call(self, addr, args):
        return T.call_ppc(self.e, addr, [a & MASK for a in args])[0] & MASK

    def call_walk(self, which, args):
        if which == "ftp":
            return self.call(F_FTP, [self.obj])
        if which == "up":
            return self.call(F_UP, [self.obj])
        return self.call(F_RIDEINFO, [self.obj] + list(args))

    def call_dtor(self, variant, flag):
        self.call((F_SRVDTOR, F_RIDEDTOR, F_RIDEBASE)[variant], [self.obj, flag])

    def call_login(self, kind):
        self.call(F_SRVLOGIN if kind == 0 else F_RIDELOGIN, [self.obj])

    def call_filepath(self, idx):
        self.call(F_FILEPATH, [self.alloc(0x40), P["items"][idx - 1]])

    def call_avatar(self, v):
        self.words(P["sys"] + 0x5C, [v])
        return self.call(F_AVATAR, [P["sys"]])

    def call_bankrupt(self):
        self.call(F_BANKRUPT, [])

    def call_sinit(self):
        self.call(F_SINIT, [])


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        kind = case["kind"]
        self.obj = lib.tpw_svl_new_list(kind, case["live"])
        self.blk = lib.tpw_svl_blk(self.obj)
        self.recv = lib.tpw_svl_alloc(32)
        self.infos = lib.tpw_svl_alloc(16)
        self.sys = lib.tpw_svl_alloc(0x40)
        self.items = []
        for i in range(4):
            a = lib.tpw_svl_item(kind)
            self.items.append(a)
            lib.tpw_svl_set_slot(i + 1, a)
        fixed = {1: self.obj, 2: self.blk, 3: self.recv, 4: self.infos, 5: self.sys, 7: 0x7000,
                 31: lib.tpw_svl_addr(b"str1"), 32: lib.tpw_svl_addr(b"str2"), 33: lib.tpw_svl_addr(b"node1"), 34: lib.tpw_svl_addr(b"node2")}
        for i, a in enumerate(self.items):
            fixed[11 + i] = a
        CL.host_begin(lib, fixed, case["queues"])

    def done(self):
        pass

    def alloc(self, n):
        return self.lib.tpw_svl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def init_list(self):
        pass

    def set_fields(self, w):
        self.lib.tpw_svl_set_fields(self.obj, (C.c_int32 * 10)(*[s32(x) for x in w]))

    def get_fields(self):
        out = (C.c_int32 * 10)()
        self.lib.tpw_svl_get_fields(self.obj, out)
        return [x & MASK for x in out]

    def set_count(self, n):
        self.lib.tpw_svl_set_count(self.obj, n)

    def get_count(self):
        return self.lib.tpw_svl_get_count(self.obj) & MASK

    def set_nodes(self, idxs):
        arr = (C.c_uint64 * max(1, len(idxs)))(*[self.items[i - 1] for i in idxs])
        self.lib.tpw_svl_set_nodes(self.obj, len(idxs), arr)

    def set_item(self, idx, kind, a, b, text):
        self.lib.tpw_svl_item_set(kind, self.items[idx - 1], s32(a), s32(b))
        if kind == 1:
            self.lib.tpw_svl_item_text(self.items[idx - 1], text)

    def set_recv(self, has, has_infos, ref):
        self.lib.tpw_svl_set_recv(self.obj, self.recv, self.infos, has, has_infos, s32(ref))

    def get_recv(self):
        out = (C.c_int32 * 3)()
        self.lib.tpw_svl_get_recv(self.obj, self.recv, out)
        return list(out)

    def set_sys(self, preset):
        self.lib.tpw_svl_set_sys(self.sys if preset else 0)

    def get_sys(self):
        v = self.lib.tpw_svl_get_sys()
        return 0 if v == 0 else 1 if v == self.sys else 2

    def item_index(self, ptr):
        return 0 if ptr == 0 else self.items.index(ptr) + 1 if ptr in self.items else -1

    def call_walk(self, which, args):
        if which == "ftp":
            return self.lib.tpw_svl_ftp(self.obj)
        if which == "up":
            return self.lib.tpw_svl_upload(self.obj)
        return self.lib.tpw_svl_rideinfo(self.obj, s32(args[0]), s32(args[1]))

    def call_dtor(self, variant, flag):
        self.lib.tpw_svl_dtor(variant, self.obj, s32(flag))

    def call_login(self, kind):
        self.lib.tpw_svl_login(kind, self.obj)

    def call_filepath(self, idx):
        self.lib.tpw_svl_filepath(self.items[idx - 1])

    def call_avatar(self, v):
        return self.lib.tpw_svl_avatar(v & MASK)

    def call_bankrupt(self):
        self.lib.tpw_svl_bankrupt()

    def call_sinit(self):
        self.lib.tpw_svl_sinit()


def bind(lib):
    if getattr(lib, "bound", False):
        return
    U64, I32, U32, PI = C.c_uint64, C.c_int32, C.c_uint32, C.POINTER(C.c_int32)

    def sig(name, res, *args):
        f = getattr(lib, "tpw_svl_" + name)
        f.restype, f.argtypes = res, list(args)

    sig("addr", U64, C.c_char_p)
    sig("alloc", U64, U32)
    sig("layout", I32, C.POINTER(C.c_int64))
    sig("new_list", U64, I32, I32)
    sig("blk", U64, U64)
    sig("set_fields", None, U64, PI)
    sig("get_fields", None, U64, PI)
    sig("set_count", None, U64, I32)
    sig("get_count", I32, U64)
    sig("set_nodes", None, U64, I32, C.POINTER(C.c_uint64))
    sig("item", U64, I32)
    sig("set_slot", None, I32, U64)
    sig("item_set", None, I32, U64, I32, I32)
    sig("item_text", None, U64, U64)
    sig("set_recv", None, U64, U64, U64, I32, I32, I32)
    sig("get_recv", None, U64, U64, PI)
    sig("set_sys", None, U64)
    sig("get_sys", U64)
    sig("ftp", U64, U64)
    sig("upload", U64, U64)
    sig("rideinfo", U64, U64, I32, I32)
    sig("dtor", None, I32, U64, I32)
    sig("login", None, I32, U64)
    sig("filepath", None, U64)
    sig("avatar", U32, U32)
    sig("bankrupt", None)
    sig("sinit", None)
    out = (C.c_int64 * 32)()
    n = lib.tpw_svl_layout(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world


def build(s, case):
    s.init_list()
    kind = case["kind"]
    w = list(case["w"])
    if case["op"] == "login":
        w[6] = P["cb"] if case["cb"] else 0
    s.set_fields(w)
    for i, (a, b) in enumerate(case["ia"]):
        s.set_item(i + 1, kind, a, b, s.text(case["text"]) if i == 0 else 0)
    s.set_nodes(case["nodes"])
    if case["n"] is not None:
        s.set_count(case["n"])
    s.set_recv(*case["recv"])
    s.set_sys(case["sys"])


def execute(s, case):
    op = case["op"]
    if op == "walk":
        return s.item_index(s.call_walk(case["which"], case["args"]))
    if op == "dtor":
        s.call_dtor(case["variant"], case["flag"])
    elif op == "login":
        s.call_login(case["kind"])
    elif op == "filepath":
        s.call_filepath(1)
    elif op == "avatar":
        return s.call_avatar(case["value"])
    elif op == "bankrupt":
        s.call_bankrupt()
    elif op == "sinit":
        s.call_sinit()
    return None


def observe(s, case, answer):
    return {"answer": answer, "log": s.log(), "fields": s.get_fields(), "count": s.get_count(), "recv": s.get_recv(), "sys": s.get_sys()}


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


# ---------------------------------------------------------------------------------------------------------------- cases


def base(op, **kw):
    c = {"op": op, "kind": 0, "live": 0, "w": [0] * 10, "nodes": [], "ia": [(0, 0)] * 4, "n": 0, "recv": (0, 0, 0), "sys": 0,
         "flag": 0, "text": "ab", "args": (0, 0), "variant": 0, "which": "ftp", "value": 0, "cb": 0,
         "queues": {"RemoveHead": [0], "ChildLocked": [0], "CityConn": [0], "Request": [0]}}
    c.update(kw)
    return c


def rword(r):
    return r.choice([0, 1, 2, -1, 0x7FFFFFFF, 0x80000000, 0x64, r.randrange(0, 1 << 32)]) & MASK


SERVER_VALUES = [1, 0x64, 0, 2, -1, 0x65, 1, 0x64]
RIDE_VALUES = [0, 1, 2, -1]


def walk_case(r, which, nodes=None):
    kind = 1 if which == "ride" else 0
    if nodes is None:
        nodes = [r.randrange(1, 5) for _ in range(r.randrange(0, 6))]
    if kind == 0:
        ia = [(r.choice(SERVER_VALUES) & MASK, 0) for _ in range(4)]
    else:
        ia = [(r.choice(RIDE_VALUES) & MASK, r.choice(RIDE_VALUES) & MASK) for _ in range(4)]
    args = (r.choice(RIDE_VALUES) & MASK, r.choice(RIDE_VALUES) & MASK)
    return base("walk", kind=kind, which=which, nodes=nodes, ia=ia, args=args, n=len(nodes))


def walk_edges(which):
    def gen():
        r = random.Random(101)
        for nodes in ([], [1], [1, 2], [2, 2], [1, 2, 3, 4, 1], [3, 4, 3]):
            for _ in range(4):
                yield walk_case(r, which, nodes)
    return gen


def walk_random(which):
    return lambda r: walk_case(r, which)


def dtor_case(r, variant, n=None, recv=None, flag=None):
    n = r.randrange(0, 5) if n is None else n
    items = [r.randrange(0, 5) for _ in range(n)]
    if recv is None:
        recv = (r.choice([0, 1, 1]), r.choice([0, 1]), r.choice([1, 1, 2, 0, -1, 5]))
    if flag is None:
        flag = r.choice([0, 1, -1, 0x10000, 0x8000, 0x7FFF, 0x18001, 2])
    w = [rword(r) for _ in range(10)]
    return base("dtor", kind=0 if variant == 0 else 1, variant=variant, w=w, n=n, recv=recv, flag=flag & MASK,
                queues={"RemoveHead": items or [0], "ChildLocked": [0], "CityConn": [0], "Request": [0]})


def dtor_edges(variant):
    def gen():
        r = random.Random(202 + variant)
        for n in (0, 1, 3):
            for recv in ((0, 0, 0), (1, 1, 1), (1, 0, 1), (1, 1, 2), (1, 1, 0), (1, 1, -1)):
                for flag in (0, 1):
                    yield dtor_case(r, variant, n, recv, flag)
        for flag in (-1, 0x10000, 0x8000, 0x7FFF, 0x18001):
            yield dtor_case(r, variant, 2, (1, 1, 1), flag)
    return gen


def login_case(r, kind, locked=None, city=None, cb=None, w08=None):
    w = [rword(r) for _ in range(10)]
    w[1] = r.choice([0, 1, 5]) if w08 is None else w08
    cbv = r.choice([0, 1]) if cb is None else cb
    return base("login", kind=kind, live=1, w=w, cb=cbv, sys=r.choice([0, 1]),
                queues={"RemoveHead": [0], "ChildLocked": [r.choice([0, 0, 1, 5, -1]) if locked is None else locked],
                        "CityConn": [r.choice([0, 0x7000]) if city is None else city], "Request": [rword(r)]})


def login_edges(kind):
    def gen():
        r = random.Random(303 + kind)
        for locked in (0, 1, -1):
            for city in (0, 0x7000):
                for cb in (0, 1):
                    for w08 in (0, 1):
                        yield login_case(r, kind, locked, city, cb, w08)
    return gen


def filepath_case(r):
    text = "".join(r.choice("abcXYZ019 _/:") for _ in range(r.randrange(0, 8)))
    return base("filepath", kind=1, text=text, ia=[(0, 0)] * 4)


def filepath_edges():
    r = random.Random(404)
    for text in ("", "a", "path/to/file.tpw", "xyz"):
        c = filepath_case(r)
        c["text"] = text
        yield c


def avatar_case(r):
    return base("avatar", value=rword(r))


def avatar_edges():
    for v in (0, 1, 0x80000000, MASK, 0x12345678):
        yield base("avatar", value=v)


def const_edges(op):
    def gen():
        yield base(op)
    return gen


TESTS = [
    FnTest("CFSOnlineSystemServerList::ReturnFTPDownloadServer", ppc, host, edge=walk_edges("ftp"), rnd=walk_random("ftp")),
    FnTest("CFSOnlineSystemServerList::ReturnUploadServer", ppc, host, edge=walk_edges("up"), rnd=walk_random("up")),
    FnTest("CFSOnlineSystemServerList::~CFSOnlineSystemServerList", ppc, host, edge=dtor_edges(0), rnd=lambda r: dtor_case(r, 0)),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineRide>,ccRideList>::~CFSOnlineSystemList", ppc, host, edge=dtor_edges(2),
           rnd=lambda r: dtor_case(r, 2)),
    FnTest("CFSOnlineSystemRideList::~CFSOnlineSystemRideList", ppc, host, edge=dtor_edges(1), rnd=lambda r: dtor_case(r, 1)),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineServer>,ccServerList>::LoggedInGetList", ppc, host, edge=login_edges(0),
           rnd=lambda r: login_case(r, 0)),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineRide>,ccRideList>::LoggedInGetList", ppc, host, edge=login_edges(1),
           rnd=lambda r: login_case(r, 1)),
    FnTest("CFSOnlineSystemRideList::ReturnRideInfo", ppc, host, edge=walk_edges("ride"), rnd=walk_random("ride")),
    FnTest("__sinit_FS_OnlineList_cpp", ppc, host, edge=const_edges("sinit"), rnd=lambda r: base("sinit")),
    FnTest("CFSOnlineRide::GetFilePath", ppc, host, edge=filepath_edges, rnd=filepath_case),
    FnTest("CFSOnlineSystem::GetAvatarID", ppc, host, edge=avatar_edges, rnd=avatar_case),
    FnTest("UI_ByeByeBankrupt", ppc, host, edge=const_edges("bankrupt"), rnd=lambda r: base("bankrupt")),
]
