"""Unit v5375, the item list resource's two OpenFile overrides and its destructor, against
libtpw_unit_CTPItemListResource_OpenFile.dylib (tests/unit_CTPItemListResource_OpenFile.cpp), main app, src/game/TPResource.cpp:
  CTPItemListResource::OpenFile 0x10163b90 (name, mode), CTPItemListResource::OpenFile 0x101639f0 (name, storage, mode),
  CTPItemListResource::~CTPItemListResource 0x101621f4

A case is a world and a call. The world is built twice by shared Python code (the Mac's big-endian objects in emulator memory, the host's
real objects through helper functions of the library). The callees outside the unit are fakes on both sides recording the same events:
the string constructors, assignment and destructor (every string is named by a tag kept in its data pointer), QualifyFilename,
CTPBaseResource::OpenQualifiedFile, the file storage's destructor, and the virtual calls made through __ptr_glue (the storage's OpenFile,
the deleting destructors of items, lists, storages and strings). TbTList::RemoveHead is emulated on the PowerPC side and real on the host.
The destructor's inlined base destructors are written in the library as fakes that make the original's events.

The world of a case: the resource (items list with 0 to 4 items, the 0x48 word, the last found storage, the search pattern, the storage
list), the global search path string, the file and the name; the queues give QualifyFilename's, the storage open's and
OpenQualifiedFile's answers. What is compared: the answer, the event log, the resource's fields and both lists.

No fused operations. Not tested: a NULL item in the list for OpenFile (the original reads it).
"""
import ctypes as C
import random
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_CTPItemListResource_OpenFile"
MASK = 0xFFFFFFFF

FN_OPEN1, FN_OPEN2, FN_DTOR = 0x10163B90, 0x101639F0, 0x101621F4
SIZES = {FN_OPEN1: 396, FN_OPEN2: 272, FN_DTOR: 424}
OWN = set(SIZES)

STR_CTOR, STR_COPY, STR_ASSIGN, STR_DTOR = 0x10238458, 0x10238308, 0x10238470, 0x102382F0
QUALIFY, OQF, GLUE, ST_DTOR, OP_DELETE = 0x10160078, 0x101630D8, 0x1023BAA8, 0x10238428, 0x10238218
RH_ITEMS, RH_STORAGES = 0x101647DC, 0x10164B94

ADDR = dict(searchpath=0x103B74C8 + 0x4060)
HOST_LAYOUT = []
# the Mac offsets of the resource's fields by id (the host library has the same ids)
OFF = {0: 0x4C, 1: 0x48, 2: 0x38, 3: 0x3C, 4: 0x2C, 5: 0x44, 6: 0x1C, 7: 0x08}
TV_ITEM, TV_ILIST, TV_OPEN, TV_STORAGE, TV_STR, TV_SLIST = 0x5001, 0x5002, 0x5003, 0x5004, 0x5005, 0x5006


def s32(v):
    return T.s32(v)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def tag(e, a):
    return e.u32(a + 8)


def new_tag(e):
    e.cl.ctr += 1
    return e.cl.ctr - 1


def put(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def s_ctor(e):
    t = new_tag(e)
    put(e, e.r(3) + 8, t)
    e.cl.rec("StrNew", (t,))


def s_copy(e):
    t = new_tag(e)
    put(e, e.r(3) + 8, t)
    e.cl.rec("StrCopy", (t, tag(e, e.r(4))))


def s_assign(e):
    e.cl.rec("StrAssign", (tag(e, e.r(3)), tag(e, e.r(4))))
    put(e, e.r(3) + 8, tag(e, e.r(4)))


def s_dtor(e):
    e.cl.rec("StrDtor", (tag(e, e.r(3)), s32(T.s16(e.r(4)))))


def qualify(e):
    t = new_tag(e)
    e.cl.rec("Qualify", (tag(e, e.r(3)), tag(e, e.r(4)), t))
    put(e, e.r(5) + 8, t)
    e.set_r(3, e.cl.ret("Qualify") & MASK)


def oqf(e):
    L = e.cl
    L.rec("OpenQualified", (L.lab(e.r(3)), L.lab(e.r(4)), tag(e, e.r(5))))
    e.set_r(3, L.ret("oq") & MASK)


def st_dtor(e):
    e.cl.rec("StorageDtor", (e.u32(e.r(3) + 4), s32(T.s16(e.r(4)))))


def op_delete(e):
    e.cl.rec("OperatorDelete", (e.cl.lab(e.r(3)),))


def remove_head(e):
    lst = e.r(3)
    head = e.u32(lst + 4)
    if head == 0:
        e.set_r(3, 0)
        return
    nxt = e.u32(head + 4)
    put(e, lst + 4, nxt)
    if nxt == 0:
        put(e, lst + 8, 0)
    else:
        put(e, nxt + 8, 0)
    put(e, lst + 12, e.u32(lst + 12) - 1)
    e.set_r(3, e.u32(head))


def glue(e):
    L = e.cl
    tv = e.r(12)
    flag = e.r(4) & 0xFFFF
    if tv == TV_ITEM:
        L.rec("ItemDelete", (tag(e, e.r(3)), flag))
    elif tv == TV_ILIST:
        L.rec("ListDelete", (1, e.u32(e.r(3) + 12), flag))
    elif tv == TV_SLIST:
        L.rec("ListDelete", (2, e.u32(e.r(3) + 12), flag))
    elif tv == TV_OPEN:
        L.rec("StorageOpen", (e.u32(e.r(4) + 4), L.lab(e.r(5)), e.r(6), e.r(7), e.r(8)))
        put(e, e.r(3), L.ret("open") & MASK)
    elif tv == TV_STORAGE:
        L.rec("StorageDelete", (e.u32(e.r(3) + 4), flag))
    elif tv == TV_STR:
        L.rec("StrDelete", (tag(e, e.r(3)), flag))
    else:
        raise AssertionError("unknown vcall %x" % tv)


for _a, _f in ((STR_CTOR, s_ctor), (STR_COPY, s_copy), (STR_ASSIGN, s_assign), (STR_DTOR, s_dtor), (QUALIFY, qualify), (OQF, oqf),
               (GLUE, glue), (ST_DTOR, st_dtor), (OP_DELETE, op_delete), (RH_ITEMS, remove_head), (RH_STORAGES, remove_head)):
    STUBS.internal(_a, _f)


def setup(runner):
    T.install_std_stubs(runner.emu)


class Side:
    def words(self, a, ws):
        self.write(a, b"".join(struct.pack(self.fmt, w & MASK) for w in ws))

    def rd_words(self, a, n):
        return list(struct.unpack(self.fmt[0] + "%dI" % n, self.read(a, 4 * n)))

    def rd_word(self, a):
        return self.rd_words(a, 1)[0]


class Ppc(Side):
    fmt = ">I"

    def __init__(self, e, case):
        self.e = e
        self.res = e.alloc(0x50)
        self.file = e.alloc(0x40)
        self.L = e.cl = CL.PpcLog({self.res: 1, self.file: 7}, case["queues"])
        self.L.ctr = 0x10000
        self.saved = STUBS.activate(e)
        self.vt_item = self.vt(2, TV_ITEM)
        self.vt_ilist = self.vt(2, TV_ILIST)
        self.vt_slist = self.vt(2, TV_SLIST)
        self.vt_str = self.vt(5, TV_STR)
        self.vt_stor = self.vt(5, TV_OPEN)
        self.words(self.vt_stor + 4 * 13, [TV_STORAGE])

    def vt(self, slot, tv):
        a = self.e.alloc(0x40)
        self.words(a + 4 * slot, [tv])
        return a

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

    def set_tag(self, a, t):
        self.words(a + 8, [t])

    def str_new(self, t):
        a = self.alloc(0x14)
        self.words(a, [self.vt_str])
        self.set_tag(a, t)
        return a

    def storage_new(self, t):
        a = self.alloc(8)
        self.words(a, [self.vt_stor, t])
        return a

    def item_new(self, t):
        a = self.alloc(0x38)
        self.words(a + 8, [t])
        self.words(a + 0x14 + 8, [t + 1])
        self.words(a + 0x28, [self.vt_stor, t + 2, self.vt_item])
        return a

    def list_new(self, kind):
        a = self.alloc(16)
        self.words(a, [self.vt_ilist if kind == 1 else self.vt_slist])
        return a

    def list_add(self, kind, lst, obj):
        node = self.alloc(12)
        tail = self.rd_word(lst + 8)
        self.words(node, [obj, 0, tail])
        self.words((tail + 4) if tail else (lst + 4), [node])
        self.words(lst + 8, [node])
        self.words(lst + 12, [self.rd_word(lst + 12) + 1])

    def list_dump(self, kind, lst):
        out = [self.rd_word(lst + 12)]
        node = self.rd_word(lst + 4)
        while node:
            d = self.rd_word(node)
            out.append(0 if d == 0 else self.rd_word(d + (8 if kind == 1 else 4)))
            node = self.rd_word(node + 4)
        return out

    def res_set(self, i, v):
        self.words(self.res + OFF[i], [v])

    def res_get(self, i):
        v = self.rd_word(self.res + OFF[i])
        if i == 2:
            return self.rd_word(v + 4) if v else 0
        if i in (0, 3, 4):
            return 1 if v else 0
        return v

    def open1(self, res, file, name, mode):
        return T.call_ppc(self.e, FN_OPEN1, [res, file, name, mode])[0] & MASK

    def open2(self, res, file, name, storage, mode):
        return T.call_ppc(self.e, FN_OPEN2, [res, file, name, storage, mode])[0] & MASK

    def dtor(self, res, flag):
        r3 = T.call_ppc(self.e, FN_DTOR, [res, flag & MASK])[0] & MASK
        return 1 if r3 == res else 0


class Host(Side):
    fmt = "<I"

    def __init__(self, lib, case):
        self.lib = lib
        bind(lib)
        lib.tpw_il_reset()
        self.res = lib.tpw_il_res_new()
        self.file = lib.tpw_il_alloc(0x40)
        CL.host_begin(lib, {1: self.res, 7: self.file}, case["queues"])

    def done(self):
        pass

    def g(self, name):
        return self.lib.tpw_il_addr(name.encode())

    def alloc(self, n):
        return self.lib.tpw_il_alloc(n)

    def read(self, a, n):
        return C.string_at(a, n)

    def write(self, a, data):
        C.memmove(a, data, len(data))

    def log(self):
        return CL.host_end(self.lib)

    def set_tag(self, a, t):
        self.lib.tpw_il_set_tag(a, t)

    def str_new(self, t):
        return self.lib.tpw_il_str_new(t)

    def storage_new(self, t):
        return self.lib.tpw_il_storage_new(t)

    def item_new(self, t):
        return self.lib.tpw_il_item_new(t)

    def list_new(self, kind):
        return self.lib.tpw_il_list_new(kind)

    def list_add(self, kind, lst, obj):
        self.lib.tpw_il_list_add(kind, lst, obj)

    def list_dump(self, kind, lst):
        out = (C.c_int64 * 16)()
        n = self.lib.tpw_il_list_dump(kind, lst, out, 16)
        return list(out)[:n]

    def res_set(self, i, v):
        self.lib.tpw_il_res_set(self.res, i, v)

    def res_get(self, i):
        return self.lib.tpw_il_res_get(self.res, i)

    def open1(self, res, file, name, mode):
        return self.lib.tpw_il_open1(res, file, name, mode) & MASK

    def open2(self, res, file, name, storage, mode):
        return self.lib.tpw_il_open2(res, file, name, storage, mode) & MASK

    def dtor(self, res, flag):
        r = self.lib.tpw_il_dtor(res, T.s16(flag))
        return 1 if r == res else 0


def bind(lib):
    if getattr(lib, "bound", False):
        return
    u64, u32, i32, i16, p = C.c_uint64, C.c_uint32, C.c_int32, C.c_int16, C.POINTER(C.c_int64)
    sigs = {
        "addr": (u64, [C.c_char_p]), "alloc": (u64, [u32]), "layout": (i32, [p]), "reset": (None, []),
        "set_tag": (None, [u64, u32]), "str_new": (u64, [u32]), "storage_new": (u64, [u32]), "item_new": (u64, [u32]),
        "list_new": (u64, [i32]), "list_add": (None, [i32, u64, u64]), "list_dump": (i32, [i32, u64, p, i32]),
        "res_new": (u64, []), "res_set": (None, [u64, i32, u64]), "res_get": (u64, [u64, i32]),
        "open1": (u32, [u64, u64, u64, u32]), "open2": (u32, [u64, u64, u64, u64, u32]), "dtor": (u64, [u64, i16]),
    }
    for name, (res, args) in sigs.items():
        fn = getattr(lib, "tpw_il_" + name)
        fn.restype, fn.argtypes = res, args
    out = (C.c_int64 * 32)()
    n = lib.tpw_il_layout(out)
    assert list(out)[:n] == HOST_LAYOUT
    lib.bound = True


def build(s, case):
    s.set_tag(s.g("searchpath"), 0x400)
    s.res_set(1, case["f48"])
    s.res_set(2, s.storage_new(0x900) if case["last"] else 0)
    s.res_set(5, 0x700)
    s.res_set(6, 0x300)
    s.res_set(7, 0x200)
    s.lists = []
    lst = 0
    if case["list"]:
        lst = s.list_new(1)
        for i, present in enumerate(case["items"]):
            s.list_add(1, lst, s.item_new(0x1000 + 0x10 * i) if present else 0)
        s.lists.append((1, lst))
    s.res_set(0, lst)
    sl = 0
    if case["slist"]:
        sl = s.list_new(2)
        for i, present in enumerate(case["stors"]):
            s.list_add(2, sl, s.storage_new(0x800 + 0x10 * i) if present else 0)
        s.lists.append((2, sl))
    s.res_set(4, sl)
    s.res_set(3, s.str_new(0x600) if case["pattern"] else 0)
    s.nm = s.str_new(0x500)
    s.st = s.storage_new(0xA00)


def execute(s, case):
    op = case["op"]
    if op == "open1":
        return s.open1(s.res, s.file, s.nm, case["mode"])
    if op == "open2":
        return s.open2(s.res, s.file, s.nm, s.st, case["mode"])
    return s.dtor(0 if case["null"] else s.res, case["flag"])


def observe(s, case, answer):
    out = {"answer": answer, "log": s.log()}
    for i in range(5):
        out["f%d" % i] = s.res_get(i)
    out["lists"] = [s.list_dump(k, l) for k, l in s.lists]
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


def mk(op, items=(1, 1), qual=(1,), opens=(0,), oq=(1,), list_=True, f48=0x7777, last=False, mode=0, pattern=False,
       slist=False, stors=(), flag=1, null=False):
    return {"op": op, "queues": {"Qualify": list(qual), "open": list(opens), "oq": list(oq)}, "list": list_, "items": list(items),
            "f48": f48, "last": last, "mode": mode, "pattern": pattern, "slist": slist, "stors": list(stors), "flag": flag,
            "null": null}


def rnd_case(r, op):
    n = r.choice([0, 1, 1, 2, 3, 4])
    return mk(op, items=[1 if op != "dtor" else r.choice([1, 1, 0]) for _ in range(n)],
              qual=[r.choice([0, 1, 1, 2, -1]) for _ in range(max(1, n))],
              opens=[r.choice([-1, 0, 1, 5, -7]) for _ in range(max(1, n))], oq=[r.choice([0, 1, 2, -1])],
              list_=r.random() < 0.85, f48=r.choice([0, 0x7777]), last=r.random() < 0.5, mode=r.choice([0, 1, 0x80000001, r.getrandbits(32)]),
              pattern=r.random() < 0.6, slist=r.random() < 0.6, stors=[r.choice([1, 1, 0]) for _ in range(r.choice([0, 1, 2, 3]))],
              flag=r.choice([0, 1, -1, 2, 0x7FFF, -0x8000, 0x18000, 0x10001, 0x10000]), null=r.random() < 0.1)


def open_edges(op):
    for list_ in (False, True):
        for n in (0, 1, 2, 3):
            yield mk(op, items=[1] * n, list_=list_, qual=[1], opens=[-1])
            for k in range(n):
                yield mk(op, items=[1] * n, list_=list_, qual=[1], opens=[-1] * k + [0])
                yield mk(op, items=[1] * n, list_=list_, qual=[0] * k + [1], opens=[-1] * k + [0], last=True)
                yield mk(op, items=[1] * n, list_=list_, qual=[0, 1, 2], opens=[1, -1, 0], oq=[0])
                yield mk(op, items=[1] * n, list_=list_, qual=[1] * (k + 1) + [2], opens=[-1] * k + [2], f48=0)
            yield mk(op, items=[1] * n, list_=list_, qual=[0], opens=[0], oq=[0], f48=0)
            yield mk(op, items=[1] * n, list_=list_, qual=[2], opens=[0], oq=[1])
            yield mk(op, items=[1] * n, list_=list_, qual=[-1], opens=[0], oq=[2])
            yield mk(op, items=[1] * n, list_=list_, qual=[1], opens=[0], oq=[-1], mode=0x80000001)


def dtor_edges():
    for null in (False, True):
        for flag in (0, 1, -1, 2, 0x10001, 0x18000):
            for list_ in (False, True):
                for items in ((), (1,), (1, 1, 1), (0,), (1, 0, 1)):
                    for slist, stors in ((False, ()), (True, ()), (True, (1,)), (True, (1, 0, 1)), (True, (0,))):
                        yield mk("dtor", items=items, list_=list_, slist=slist, stors=stors, flag=flag, null=null,
                                 pattern=len(items) % 2 == 0, f48=0x7777)
    yield mk("dtor", pattern=True, slist=True, stors=(1, 1), flag=1)


def open1_edges():
    return open_edges("open1")


def open2_edges():
    return open_edges("open2")


TESTS = [
    FnTest("CTPItemListResource::OpenFile", ppc, host, edge=open1_edges, rnd=lambda r: rnd_case(r, "open1")),
    FnTest("CTPItemListResource::OpenFile", ppc, host, edge=open2_edges, rnd=lambda r: rnd_case(r, "open2")),
    FnTest("CTPItemListResource::~CTPItemListResource", ppc, host, edge=dtor_edges, rnd=lambda r: rnd_case(r, "dtor")),
]
