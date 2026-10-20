"""Unit v51065, the online lists' small members, the park lists' GetList (top ten, by park id), the login callbacks, the city list's and the
theme list base's destructors, the park instance of LoggedInGetList, the five ChildLocked bodies and the FTP getters, against
libtpw_unit_CFSOnlineSystemCityList_ReturnCityInfo.dylib (tests/unit_CFSOnlineSystemCityList_ReturnCityInfo.cpp), main app
("SimTheme Park"), src/FS_Online_CFSOnlineSystemParkList_InWorldByPlayerName_Dtor_CFSOnlineSystemParkList_InCity_GetList_CFSOnlineSystemCityList_StoreReceivedList_CFSOnlineSystemCityList_ReturnCityInfo.cpp:
  CFSOnlineSystemCityList::ReturnCityInfo 0x10178b3c, ~CFSOnlineSystemCityList 0x10170e20, PostLoginCallbackFn_Park 0x10178c74,
  LoggedInGetList (park) 0x10178ce4, ChildLocked x5, ReturnParkInfo 0x101791d8, InCityTop10::GetList 0x1017925c, PostLoginCallbackFn_City
  0x101783a4, InWorldByParkID::GetList 0x10179794, ~CFSOnlineSystemList (theme) 0x10179b08, GetFTPDownloadParkInfo 0x10177654,
  ReturnFTPDownloadLocalPath 0x10177608, ReturnOnlineThemeName 0x1017a588, allocator<unsigned short>::destroy 0x1016e988.

A case is a world and a call. The world is built twice by ONE piece of Python code: in emulator memory at the Mac's addresses (big-endian)
for the original, and in the host library's memory (little-endian) for the translation; the same code reads both back. The callees outside
the unit (the online system, the city server connection, the list virtuals reached through __ptr_glue, the info destructors reached through
__destroy_new_array, the record destructors, RemoveHead, AddUIError) are fakes on both sides that record their arguments and answer from
the case's queues. Objects are named by labels the test assigns (list 1, its typed list 2, city token 50, online system 60, stored answer
70, new online system 99, records 10 + index), so no address enters a log.

The world of a case: the list object with its words, its typed list with the records, the stored answer, the online system global and the
queues. What is compared: the answer, the list's words, the global, the stored answer's count and the event log (operator new and delete
are not events: the shim's arena owns them, so the freeing is seen through the cleared pointers, the counts and the info destructors).

The two destructors: the stored answer released, the records RemoveHead hands out (the queue names them, NULL included) destroyed through
their virtual destructor, the two words cleared and the typed list destroyed through its own (a silent pseudo code on the Mac, the
host's TbTList destructor), for flags 0, 1 and -1 (the delete itself is not an event).

No fused operations. Not tested: a NULL list (the original reads it unconditionally); a NULL `this` for the destructors (the Mac
tests it, the host's caller does).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CFSOnlineSystemCityList_ReturnCityInfo"
MASK = 0xFFFFFFFF

F_RETURN_CITY, F_DTOR_CITY, F_PL_PARK, F_LOGGED = 0x10178B3C, 0x10170E20, 0x10178C74, 0x10178CE4
F_CL_PARK, F_CL_CITY, F_CL_THEME, F_CL_SERVER, F_CL_RIDE = 0x10178EE8, 0x101786AC, 0x1017A154, 0x1017AD38, 0x1017B8CC
F_PARK_INFO, F_TOP10, F_PL_CITY, F_BYID, F_DTOR_THEME = 0x101791D8, 0x1017925C, 0x101783A4, 0x10179794, 0x10179B08
F_FTP_PARK, F_FTP_PATH, F_THEME_NAME, F_DESTROY = 0x10177654, 0x10177608, 0x1017A588, 0x1016E988
SIZES = {F_RETURN_CITY: 52, F_DTOR_CITY: 300, F_PL_PARK: 60, F_LOGGED: 412, F_CL_PARK: 8, F_CL_CITY: 8, F_CL_THEME: 8, F_CL_SERVER: 8,
         F_CL_RIDE: 8, F_PARK_INFO: 60, F_TOP10: 352, F_PL_CITY: 60, F_BYID: 352, F_DTOR_THEME: 288, F_FTP_PARK: 8, F_FTP_PATH: 8,
         F_THEME_NAME: 52, F_DESTROY: 4}
OWN = set(SIZES)

# ---- the callees faked on both sides
C_AREWE, C_LOGIN, C_LOGOUT, C_INSTANCE, C_RETCONN, C_ADDUI = 0x1017178C, 0x101710AC, 0x10171398, 0x10178618, 0x1017346C, 0x101F1DF0
C_CTOR, C_NEW, C_DELETE, C_DESTROYARR, C_GLUE = 0x1016F7D0, 0x102384B8, 0x10238218, 0x10238AD0, 0x1023BAA8
C_COMPLETE_PARK, C_COMPLETE_CITY, C_LOGGED_CITY = 0x101741D4, 0x10173F98, 0x10178414
C_RH_CITY, C_RH_THEME = 0x10177E2C, 0x10177904     # TbTList<CFSOnlineCity*, ...>::RemoveHead, the theme instance
TV_PARK_CALLBACK = 0x10267628           # the TVector of PostLoginCallbackFn_Park in the image: Login's callback and a valid callback to call
ADDR = dict(gOnlineSystem=0x10380AB0)
HOST_LAYOUT = [48, 56, 64, 8]

# pseudo code addresses of the fake TVectors the list vtables hold (__ptr_glue reads the code word at r12)
M_CHILD, M_REQUEST, M_SILENT = 0xF0000001, 0xF0000002, 0xF0000006
M_RECDTOR, M_LISTDTOR = 0xF0000007, 0xF0000008      # a record's and the typed list's virtual destructors (slot 8)
ID_LIST, ID_MLIST, ID_TOKEN, ID_SYS, ID_RECV, ID_NEWSYS, ID_REC = 1, 2, 50, 60, 70, 99, 10


def s32(v):
    return T.s32(v)


def lab(e, v):
    v &= MASK
    return 0 if v == 0 else e.ids.get(v, -1)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def s_arewe(e):
    e.cl.rec("AreWeLoggedIn", (lab(e, e.r(3)),))
    e.set_r(3, e.cl.ret("AreWeLoggedIn") & MASK)


def s_login(e):
    tv_code = struct.unpack(">I", e.read(e.r(4), 4))[0]
    e.cl.rec("Login", (lab(e, e.r(3)), 1 if tv_code == F_PL_PARK else 0, lab(e, e.r(5))))


def s_logout(e):
    e.cl.rec("Logout", (lab(e, e.r(3)),))


def s_instance(e):
    e.cl.rec("Instance", ())
    e.set_r(3, struct.unpack(">I", e.read(ADDR["gOnlineSystem"], 4))[0])


def s_retconn(e):
    e.cl.rec("ReturnConnection", (lab(e, e.r(3)),))
    e.set_r(3, e.token if e.cl.ret("ReturnConnection") != 0 else 0)


def s_addui(e):
    e.cl.rec("AddUIError", (s32(e.r(3)), lab(e, e.r(4))))


def s_ctor(e):
    e.cl.rec("CFSOnlineSystem_ctor", ())
    e.ids[e.r(3) & MASK] = ID_NEWSYS


def s_new(e):
    e.set_r(3, e.alloc(max(e.r(3), 4)))


def s_nothing(e):
    pass


def s_destroyarr(e):
    count = struct.unpack(">I", e.read((e.r(3) & MASK) - 4, 4))[0]
    for _ in range(count):
        e.cl.rec("~info", (e.info_kind,))


def s_complete_park(e):
    e.cl.rec("CompleteGetList", (lab(e, e.r(3)), s32(e.r(4))))


def s_complete_city(e):
    e.cl.rec("CompleteGetList_City", (lab(e, e.r(3)), s32(e.r(4))))


def s_logged_city(e):
    e.cl.rec("LoggedInGetList_City", (lab(e, e.r(3)),))


def s_glue(e):
    """__ptr_glue: r12 is a TVector (code, rtoc); the fake TVectors carry pseudo codes, the callback is the real Park TVector."""
    code = struct.unpack(">I", e.read(e.r(12), 4))[0]
    if code == M_CHILD:
        e.cl.rec("ChildLocked", (lab(e, e.r(3)),))
        e.set_r(3, e.cl.ret("ChildLocked") & MASK)
    elif code == M_SILENT:
        e.set_r(3, 0)
    elif code == M_REQUEST:
        e.cl.rec("Request", (lab(e, e.r(3)), lab(e, e.r(4)), e.r(5) & MASK))
        e.set_r(3, e.cl.ret("Request") & MASK)
    elif code == M_RECDTOR:
        e.cl.rec("RecordDtor", (lab(e, e.r(3)),))
    elif code == M_LISTDTOR:
        pass
    elif code == F_PL_PARK:
        e.cl.rec("Callback", (e.r(12) & MASK, s32(e.r(3)), e.r(4) & MASK, lab(e, e.r(5))))
    else:
        raise AssertionError("glue code %x" % code)


def s_removehead(e):
    """One node fewer (the count at 0x0c) and the record the queue names: an index into the case's records, 0 for NULL."""
    header = e.r(3) & MASK
    e.cl.rec("RemoveHead", (lab(e, header),))
    count = struct.unpack(">I", e.read(header + 0xC, 4))[0]
    if count > 0:
        e.write(header + 0xC, struct.pack(">I", count - 1))
    index = e.cl.ret("RemoveHead") & 15
    e.set_r(3, e.removals[index - 1] if 0 < index <= len(e.removals) else 0)


def with_toc(fn):
    """The import glue saves the caller's TOC pointer at 0x14(r1); the caller reloads it after the call."""
    def wrapped(e):
        e.write((e.r(1) + 0x14) & MASK, struct.pack(">I", e.r(2) & MASK))
        fn(e)
    return wrapped


for _addr, _fn in ((C_AREWE, s_arewe), (C_LOGIN, s_login), (C_LOGOUT, s_logout), (C_INSTANCE, s_instance), (C_RETCONN, s_retconn),
                   (C_ADDUI, s_addui), (C_CTOR, s_ctor), (C_COMPLETE_PARK, s_complete_park), (C_COMPLETE_CITY, s_complete_city),
                   (C_LOGGED_CITY, s_logged_city), (C_RH_CITY, s_removehead), (C_RH_THEME, s_removehead)):
    STUBS.internal(_addr, _fn)
for _addr, _fn in ((C_NEW, s_new), (C_DELETE, s_nothing), (C_DESTROYARR, s_destroyarr), (C_GLUE, s_glue)):
    STUBS.internal(_addr, with_toc(_fn))


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]

    def lbl(self, v):
        v &= MASK
        return 0 if v == 0 else self.ids.get(v, ID_NEWSYS)

    def set_global(self, name, value):
        self.words(self.g(name), [value])

    def get_global(self, name):
        return self.rd_word(self.g(name))


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.ids = e.ids = {}
        e.alloc_fn = e.alloc
        e.info_kind = {0: 1, 1: 2, 2: 3}[case["rk"]]
        e.token = 0
        e.removals = []
        self.L = e.cl = CL.PpcLog({ADDR["gOnlineSystem"]: 0}, case["queues"])
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

    def label(self, addr, n):
        self.ids[addr & MASK] = n

    def call(self, fn, args):
        r3, _, _ = T.call_ppc(self.e, fn, [a & MASK for a in args])
        return r3 & MASK

    # -- the world
    def new_list(self, kind):
        obj = self.alloc(0x40)
        vtable = self.alloc(0x20)
        tv_child, tv_req = self.alloc(8), self.alloc(8)
        self.words(tv_child, [M_CHILD if kind == 4 else M_SILENT, 0])
        self.words(tv_req, [M_REQUEST, 0])
        self.words(vtable + 0xC, [tv_child, tv_req])
        self.words(obj, [vtable])
        return obj

    def fake_vtable(self, code):
        """A vtable whose destructor slot (8) holds a TVector with the pseudo code `code`."""
        vtable, tv = self.alloc(0x10), self.alloc(8)
        self.words(tv, [code, 0])
        self.words(vtable + 8, [tv])
        return vtable

    def getw(self, obj, off):
        return self.rd_word(obj + off)

    def setw(self, obj, off, v):
        self.words(obj + off, [v])

    def set_mlist(self, obj, rk, recs):
        """The typed list with its records; returns the records' addresses (record i has the id/name given)."""
        header = self.alloc(0x10)
        nodes, addrs = [], []
        rec_vtable = self.fake_vtable(M_RECDTOR)
        for rec in recs:
            r = self.alloc(0x40)
            self.words(r, [rec_vtable])
            self.words(r + 8, [rec["id"]])
            self.words(r + 0x14, [rec.get("name", 0)])
            addrs.append(r)
            nodes.append(self.alloc(12))
        for i, n in enumerate(nodes):
            self.words(n, [addrs[i], nodes[i + 1] if i + 1 < len(nodes) else 0, nodes[i - 1] if i else 0])
        self.words(header, [self.fake_vtable(M_LISTDTOR), nodes[0] if nodes else 0, nodes[-1] if nodes else 0, len(nodes)])
        self.setw(obj, 0x2C, header)
        self.label(header, ID_MLIST)
        return addrs

    def new_recv(self, n, ref, has):
        r = self.alloc(0x10)
        infos = 0
        if has:
            base = self.alloc(4 + 8 * max(n, 1))
            self.words(base, [n])
            infos = base + 4
        self.words(r, [n, infos, 0, ref])
        return r

    def recv_ref(self, r):
        return self.rd_word(r + 0xC)

    def new_sys(self):
        return self.alloc(0x404)

    def sys_set(self, sysobj, park, data):
        self.words(sysobj + 0x3A4, [park])
        self.words(sysobj + 0x3E4, [data])

    def set_token(self, addr):
        self.e.token = addr

    def set_removals(self, addrs):
        self.e.removals = addrs


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        self.ids = {}
        lib.tpw_crl_reset()
        CL.host_begin(lib, {}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_crl_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_crl_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def label(self, addr, n):
        self.ids[addr & MASK] = n
        self.lib.tpw_crl_label(addr, n)

    # The host's heap addresses are wider than the Mac's words: pointers travel whole, the logs and labels use the low word.
    def set_global(self, name, value):
        self.lib.tpw_crl_set_sys_global(value)

    def get_global(self, name):
        return struct.unpack("<Q", self.read(self.g(name), 8))[0]

    NAMES = {F_RETURN_CITY: "city_info", F_PARK_INFO: "park_info", F_THEME_NAME: "theme_name", F_FTP_PARK: "ftp_park",
             F_FTP_PATH: "ftp_path", F_DESTROY: "destroy", F_PL_PARK: "pl_park", F_PL_CITY: "pl_city", F_LOGGED: "logged_in",
             F_TOP10: "top10", F_BYID: "by_id", F_DTOR_CITY: "dtor_city", F_DTOR_THEME: "dtor_theme"}

    def call(self, fn, args):
        if fn in (F_CL_PARK, F_CL_CITY, F_CL_THEME, F_CL_SERVER, F_CL_RIDE):
            which = (F_CL_PARK, F_CL_CITY, F_CL_THEME, F_CL_SERVER, F_CL_RIDE).index(fn)
            return self.lib.tpw_crl_f_child_locked(which, args[0], 0, 0) & MASK
        f = getattr(self.lib, "tpw_crl_f_" + self.NAMES[fn])
        return f(*(list(args) + [0] * 4)[:4])

    # -- the world (the same as Ppc's)
    def new_list(self, kind):
        return self.lib.tpw_crl_new_list(kind)

    def getw(self, obj, off):
        v = self.lib.tpw_crl_getw(obj, off)
        return v if off in (0x2C, 0x30) else v & MASK

    def setw(self, obj, off, v):
        self.lib.tpw_crl_setw(obj, off, v if off in (0x2C, 0x30) else v & MASK)

    def set_mlist(self, obj, rk, recs):
        header = self.getw(obj, 0x2C)
        addrs = []
        for rec in recs:
            r = self.lib.tpw_crl_new_rec(rk, rec["id"], rec.get("name", 0))
            self.lib.tpw_crl_add_tail(rk, header, r)
            addrs.append(r)
        self.label(header, ID_MLIST)
        return addrs

    def new_recv(self, n, ref, has):
        return self.lib.tpw_crl_new_recv(self.rk, n, ref, 1 if has else 0)

    def recv_ref(self, r):
        return self.lib.tpw_crl_recv_ref(r) & MASK

    def new_sys(self):
        return self.lib.tpw_crl_new_sys()

    def sys_set(self, sysobj, park, data):
        self.lib.tpw_crl_sys_set(sysobj, park, data)

    def set_token(self, addr):
        self.lib.tpw_crl_set_city(addr)

    def set_removals(self, addrs):
        for i, a in enumerate(addrs):
            self.lib.tpw_crl_set_rec(i + 1, a)


def bind(lib):
    if getattr(lib, "bound", False):
        return
    p = "tpw_crl_"
    getattr(lib, p + "addr").restype, getattr(lib, p + "addr").argtypes = C.c_uint64, [C.c_char_p]
    getattr(lib, p + "alloc").restype, getattr(lib, p + "alloc").argtypes = C.c_uint64, [C.c_uint32]
    getattr(lib, p + "layout").argtypes = [C.POINTER(C.c_int64)]
    lib.tpw_crl_label.argtypes = [C.c_uint64, C.c_int64]
    lib.tpw_crl_new_list.restype, lib.tpw_crl_new_list.argtypes = C.c_uint64, [C.c_uint32]
    lib.tpw_crl_getw.restype, lib.tpw_crl_getw.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64]
    lib.tpw_crl_setw.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_crl_set_count.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_crl_add_tail.argtypes = [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_crl_new_rec.restype, lib.tpw_crl_new_rec.argtypes = C.c_uint64, [C.c_uint64, C.c_uint64, C.c_uint64]
    lib.tpw_crl_new_recv.restype, lib.tpw_crl_new_recv.argtypes = C.c_uint64, [C.c_uint64] * 4
    lib.tpw_crl_recv_ref.restype, lib.tpw_crl_recv_ref.argtypes = C.c_uint64, [C.c_uint64]
    lib.tpw_crl_new_sys.restype = C.c_uint64
    lib.tpw_crl_sys_set.argtypes = [C.c_uint64] * 3
    lib.tpw_crl_set_city.argtypes = [C.c_uint64]
    lib.tpw_crl_set_rec.argtypes = [C.c_uint64, C.c_uint64]
    lib.tpw_crl_set_sys_global.argtypes = [C.c_uint64]
    for name in ("city_info", "park_info", "theme_name", "ftp_park", "ftp_path", "child_locked", "destroy", "dtor_city", "dtor_theme",
                 "pl_park", "pl_city", "logged_in", "top10", "by_id"):
        f = getattr(lib, p + "f_" + name)
        f.restype, f.argtypes = C.c_uint64, [C.c_uint64] * 4
    out = (C.c_int64 * 32)()
    n = getattr(lib, p + "layout")(out)
    assert list(out)[:n] == HOST_LAYOUT, list(out)[:n]
    lib.bound = True


# ---------------------------------------------------------------------------------------------------------------- the world

def build(s, case):
    s.rk = case["rk"]
    obj = 0
    if case["kind"] is not None:
        obj = s.new_list(case["kind"])
        s.label(obj, ID_LIST)
        for off, v in case["f"].items():
            s.setw(obj, off, v)
        addrs = s.set_mlist(obj, case["rk"], case["recs"])
        for i, a in enumerate(addrs):
            s.label(a, ID_REC + i)
        s.recs = addrs
        if case["recv"] is not None:
            n, ref, has = case["recv"]
            r = s.new_recv(n, ref, has)
            s.label(r, ID_RECV)
            s.setw(obj, 0x30, r)
            s.recv = r
        else:
            s.recv = 0
    s.obj = obj
    token = s.alloc(16)
    s.label(token, ID_TOKEN)
    s.set_token(token)
    sysobj = 0
    if case["sys"]:
        sysobj = s.new_sys()
        s.label(sysobj, ID_SYS)
    s.set_global("gOnlineSystem", sysobj)
    s.sysobj = sysobj
    if case["op"] in ("dtor_city", "dtor_theme"):
        s.set_removals(s.recs)
    if case["op"] in ("ftp_park", "ftp_path"):
        s.sys_set(s.sysobj, case["args"][1], case["args"][2])
    s.fargs = case["args"]


def execute(s, case):
    op, fn = case["op"], case["fn"]
    a = case["args"]
    if op == "ftp_park" or op == "ftp_path":
        return s.call(fn, [s.sysobj])
    if op in ("pl_park", "pl_city"):
        return s.call(fn, [a[0], s.obj])
    if op in ("city_info", "park_info", "theme_name"):
        return s.call(fn, [s.obj, a[0]])
    if op in ("top10", "by_id"):
        return s.call(fn, [s.obj, a[0], a[1], a[2]])
    if op == "logged_in":
        return s.call(fn, [s.obj])
    if op == "child":
        return s.call(fn, [s.obj or 0x1000])
    if op == "destroy":
        s.call(fn, [0, 0])
        return 0
    if op in ("dtor_city", "dtor_theme"):
        return s.call(fn, [s.obj, a[0]])
    raise AssertionError(op)


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    op = case["op"]
    if op in ("city_info", "park_info"):
        out["answer"] = s.lbl(answer)
    if op in ("destroy", "pl_park", "pl_city", "logged_in", "top10", "by_id", "dtor_city", "dtor_theme"):
        out["answer"] = 0                # void functions: r3 is whatever the last call left
    if case["kind"] is not None and op not in ("child",):
        words = []
        for off in range(4, 0x38 if case["kind"] in (2, 3) else 0x34, 4):
            v = s.getw(s.obj, off)
            if op == "dtor_city" and off == 0x2C:
                # v5L17: the host's city list destructor clears the list pointer after the delete (host only: the base destructor runs
                # after it, test_FS_OnlineList_dtors runs that chain); the Mac's keeps the freed pointer. Not compared.
                words.append("list")
                continue
            words.append(s.lbl(v) if off in (0x2C, 0x30) else v)
        out["words"] = words
        if case["recv"] is not None:
            out["ref"] = s.recv_ref(s.recv)
    out["sys"] = s.lbl(s.get_global("gOnlineSystem"))
    return out


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

def base(r, op, fn, kind, rk):
    return {"op": op, "fn": fn, "kind": kind, "rk": rk, "f": {}, "recs": [], "recv": None, "sys": r.random() < 0.5, "args": [],
            "queues": {"AreWeLoggedIn": [r.choice([0, 1])], "ReturnConnection": [r.choice([0, 1])], "ChildLocked": [r.choice([0, 0, 1])],
                       "Request": [r.randrange(0, 1 << 31)], "RemoveHead": []}}


def rid(r):
    return r.choice([0, 1, 2, 3, 5, -1, r.randrange(-50, 50)]) & MASK


def walk_case(r, op, fn, kind, rk, probe=None):
    c = base(r, op, fn, kind, rk)
    n = r.choice([0, 1, 2, 3, 5])
    ids = [r.randrange(0, 6) for _ in range(n)]
    c["recs"] = [{"id": i, "name": r.randrange(1, 1 << 32)} for i in ids]
    if probe is None:
        probe = r.choice(ids + [9]) if ids else 9
    c["args"] = [probe & MASK]
    return c


def walk_tests(op, fn, kind, rk, seed):
    def edges():
        r = random.Random(seed)
        for n in (0, 1, 2, 3, 4):
            for pick in range(max(n, 1) + 1):
                c = base(r, op, fn, kind, rk)
                ids = [(i * 3 + pick) % 4 for i in range(n)]
                c["recs"] = [{"id": i, "name": 0x1000 + 7 * i + k} for k, i in enumerate(ids)]
                c["args"] = [(ids[pick] if pick < n else 77) & MASK]
                if op == "park_info":
                    c["args"] = [pick if n else 0]
                if op == "park_info" and pick == n:
                    c["args"] = [-1 & MASK]
                yield c
        yield walk_case(r, op, fn, kind, rk, probe=0x80000000)

    def rnd(r):
        c = walk_case(r, op, fn, kind, rk)
        if op == "park_info":
            c["args"] = [r.choice([-1, 0, 1, 2, 3, 4, 7]) & MASK]
        return c
    return edges, rnd


def recv_state(r):
    return r.choice([None, (0, 1, False), (1, 1, True), (3, 1, True), (2, 2, True), (2, 3, False), (0, 0, True), (4, 1, False)])


def getlist_case(r, op, fn, kind, rk=1):
    c = base(r, op, fn, kind, rk)
    c["f"] = {off: r.choice([0, 1, r.randrange(1, 1 << 31)]) for off in (0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x34)}
    c["recv"] = recv_state(r)
    c["args"] = [r.choice([0, 1, 5, -1, r.randrange(1 << 32)]) & MASK, r.choice([0, TV_PARK_CALLBACK]),
                 r.choice([0, r.randrange(1 << 32)])]
    return c


def getlist_tests(op, fn, kind, seed):
    def edges():
        r = random.Random(seed)
        for logged in (0, 1):
            for sysflag in (False, True):
                for recv in (None, (1, 1, True), (2, 2, True), (0, 1, False)):
                    c = getlist_case(r, op, fn, kind)
                    c["queues"]["AreWeLoggedIn"] = [logged]
                    c["sys"] = sysflag
                    c["recv"] = recv
                    c["f"][0x1C] = 0 if r.random() < 0.5 else TV_PARK_CALLBACK
                    yield c

    def rnd(r):
        return getlist_case(r, op, fn, kind)
    return edges, rnd


def logged_case(r):
    c = base(r, "logged_in", F_LOGGED, 4, 1)
    c["f"] = {off: r.choice([0, 1, r.randrange(1, 1 << 31)]) for off in (0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x20, 0x24, 0x28, 0x34)}
    c["f"][0x1C] = r.choice([0, TV_PARK_CALLBACK])
    c["f"][0x08] = r.choice([0, 1])
    c["recv"] = recv_state(r)
    c["recs"] = [{"id": i} for i in range(r.choice([0, 2]))]
    return c


def logged_edges():
    r = random.Random(51065)
    for child in (0, 1):
        for conn in (0, 1):
            for logout in (0, 1):
                for cb in (0, TV_PARK_CALLBACK):
                    for sysflag in (False, True):
                        c = logged_case(r)
                        c["queues"].update({"ChildLocked": [child], "ReturnConnection": [conn]})
                        c["f"][0x08] = logout
                        c["f"][0x1C] = cb
                        c["sys"] = sysflag
                        yield c


def pl_case(r, op, fn, kind):
    c = base(r, op, fn, kind, 1)
    c["args"] = [r.choice([0, 1, -1, 5, r.randrange(1 << 32)]) & MASK]
    if op == "pl_park":
        c["f"] = {0x08: r.choice([0, 1]), 0x1C: r.choice([0, TV_PARK_CALLBACK]), 0x20: r.randrange(1 << 32), 0x24: r.randrange(1 << 32)}
    return c


def pl_tests(op, fn, kind, seed):
    def edges():
        r = random.Random(seed)
        for err in (0, 1, -1 & MASK, 0x4E):
            for conn in (0, 1):
                c = pl_case(r, op, fn, kind)
                c["args"] = [err]
                c["queues"]["ReturnConnection"] = [conn]
                c["queues"]["ChildLocked"] = [r.choice([0, 1])]
                yield c

    def rnd(r):
        return pl_case(r, op, fn, kind)
    return edges, rnd


def ftp_case(r, op, fn):
    c = base(r, op, fn, None, 1)
    c["args"] = [0, r.choice([0, r.randrange(1 << 32)]), r.choice([0, r.randrange(1 << 32)])]
    c["sys"] = True
    return c


def ftp_tests(op, fn, seed):
    def edges():
        r = random.Random(seed)
        for park in (0, 1, 0xFFFFFFFF, 0x12345678):
            c = ftp_case(r, op, fn)
            c["args"] = [0, park, park ^ 0x5555AAAA]
            yield c

    def rnd(r):
        return ftp_case(r, op, fn)
    return edges, rnd


def child_tests(fn, kind, seed):
    def mk(r):
        c = base(r, "child", fn, kind, 1)
        return c

    def edges():
        r = random.Random(seed)
        for _ in range(3):
            yield mk(r)
    return edges, mk


def destroy_case(r):
    c = base(r, "destroy", F_DESTROY, None, 1)
    return c


def destroy_edges():
    r = random.Random(1016)
    for _ in range(2):
        yield destroy_case(r)


def dtor_case(r, op, fn, kind, rk):
    c = base(r, op, fn, kind, rk)
    c["f"] = {off: r.choice([0, 1, r.randrange(1, 1 << 31)]) for off in range(4, 0x2C, 4)}
    n = r.choice([0, 1, 2, 3, 5])
    c["recs"] = [{"id": r.randrange(0, 6), "name": r.randrange(1, 1 << 32)} for _ in range(n)]
    c["recv"] = recv_state(r)
    c["args"] = [r.choice([0, 1, 2, -1, 0x10000]) & MASK]
    c["queues"]["RemoveHead"] = [r.choice([0] + list(range(1, n + 1)) * 3) for _ in range(n)]
    return c


def dtor_tests(op, fn, kind, rk, seed):
    def edges():
        r = random.Random(seed)
        for n in (0, 1, 3):
            for recv in (None, (1, 1, True), (2, 2, True), (1, 1, False)):
                for flag in (0, 1, -1 & MASK):
                    c = dtor_case(r, op, fn, kind, rk)
                    c["recs"] = [{"id": i, "name": 0x2000 + i} for i in range(n)]
                    c["queues"]["RemoveHead"] = [n - i if (i + flag) % 4 else 0 for i in range(n)]
                    c["recv"] = recv
                    c["args"] = [flag]
                    yield c

    def rnd(r):
        return dtor_case(r, op, fn, kind, rk)
    return edges, rnd


city_e, city_r = walk_tests("city_info", F_RETURN_CITY, 0, 0, 101)
park_e, park_r = walk_tests("park_info", F_PARK_INFO, 5, 1, 102)
theme_e, theme_r = walk_tests("theme_name", F_THEME_NAME, 6, 2, 103)
top_e, top_r = getlist_tests("top10", F_TOP10, 2, 104)
byid_e, byid_r = getlist_tests("by_id", F_BYID, 3, 105)
plp_e, plp_r = pl_tests("pl_park", F_PL_PARK, 4, 106)
plc_e, plc_r = pl_tests("pl_city", F_PL_CITY, 0, 107)
fp_e, fp_r = ftp_tests("ftp_park", F_FTP_PARK, 108)
fa_e, fa_r = ftp_tests("ftp_path", F_FTP_PATH, 109)
clp_e, clp_r = child_tests(F_CL_PARK, 4, 110)
clc_e, clc_r = child_tests(F_CL_CITY, 0, 111)
clt_e, clt_r = child_tests(F_CL_THEME, 1, 112)
cls_e, cls_r = child_tests(F_CL_SERVER, 0, 113)
clr_e, clr_r = child_tests(F_CL_RIDE, 0, 114)
dtc_e, dtc_r = dtor_tests("dtor_city", F_DTOR_CITY, 0, 0, 115)
dtt_e, dtt_r = dtor_tests("dtor_theme", F_DTOR_THEME, 1, 2, 116)

TESTS = [
    FnTest("CFSOnlineSystemCityList::ReturnCityInfo", ppc, host, edge=city_e, rnd=city_r),
    FnTest("PostLoginCallbackFn_Park", ppc, host, edge=plp_e, rnd=plp_r),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlinePark>,ccParkList>::LoggedInGetList", ppc, host, edge=logged_edges,
           rnd=lambda r: logged_case(r)),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlinePark>,ccParkList>::ChildLocked", ppc, host, edge=clp_e, rnd=clp_r),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineCity>,ccCityList>::ChildLocked", ppc, host, edge=clc_e, rnd=clc_r),
    FnTest("CFSOnlineSystemParkList_InCity::ReturnParkInfo", ppc, host, edge=park_e, rnd=park_r),
    FnTest("CFSOnlineSystemParkList_InCityTop10::GetList", ppc, host, edge=top_e, rnd=top_r),
    FnTest("PostLoginCallbackFn_City", ppc, host, edge=plc_e, rnd=plc_r),
    FnTest("CFSOnlineSystemParkList_InWorldByParkID::GetList", ppc, host, edge=byid_e, rnd=byid_r),
    FnTest("CFSOnlineSystem::GetFTPDownloadParkInfo", ppc, host, edge=fp_e, rnd=fp_r),
    FnTest("CFSOnlineSystem::ReturnFTPDownloadLocalPath", ppc, host, edge=fa_e, rnd=fa_r),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>,ccThemeList>::ChildLocked", ppc, host, edge=clt_e, rnd=clt_r),
    FnTest("CFSOnlineSystemThemeList::ReturnOnlineThemeName", ppc, host, edge=theme_e, rnd=theme_r),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineServer>,ccServerList>::ChildLocked", ppc, host, edge=cls_e, rnd=cls_r),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineRide>,ccRideList>::ChildLocked", ppc, host, edge=clr_e, rnd=clr_r),
    FnTest("std::allocator<unsigned_short>::destroy", ppc, host, edge=destroy_edges, rnd=destroy_case),
    FnTest("CFSOnlineSystemCityList::~CFSOnlineSystemCityList", ppc, host, edge=dtc_e, rnd=dtc_r),
    FnTest("CFSOnlineSystemList<TbPtrList<CFSOnlineTheme>,ccThemeList>::~CFSOnlineSystemList", ppc, host, edge=dtt_e, rnd=dtt_r),
]
