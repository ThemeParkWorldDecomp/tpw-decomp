"""Resource-index tests: fields and file records compare as values, pointers as labels.
Unit v5L21 adds TbIResourceIndex::GetItem 0x1000383c (op 10, the same calls as GetResourceItemHandle)."""
import ctypes as C
import random
import struct
import tpwtest as T
import calllog as CL
import ltms_support as L
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_TbIResourceIndex"
VP, I32, U32 = C.c_void_p, C.c_int32, C.c_uint32
LABELS = ["__ct__16TbIResourceIndexFv", "GetResource__16TbIResourceIndexFPvPPv",
          "OpenResource__16TbIResourceIndexFPQ216TbIResourceIndex17resourceIndexItemPPv",
          "OpenResourceFile__16TbIResourceIndexFRQ33bfl10FileSystem5CFilePQ216TbIResourceIndex17resourceIndexItem",
          "Initialise__16TbIResourceIndexFPCcPvUl", "GetResourceType__16TbIResourceIndexFPv",
          "GetResourceItemHandle__16TbIResourceIndexFUl", "__ct__10TbResourceFv", "Close__16TbIResourceIndexFv",
          "__dt__16TbIResourceIndexFv", "GetItem__16TbIResourceIndexFUl"]


def F(lib, name, restype, *args):
    f = lib[name]
    f.restype, f.argtypes = restype, args
    return f


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.lt_s = L.files(e)
    e.lt_resource = L.resource(e)
    e.lt_resource_vt = e.u32(e.lt_resource)
    e.lt_factories = []
    def create(e):
        e.cl.rec("Create", (e.cl.lab(e.r(4)), e.cl.lab(e.r(5)), e.r(7), e.cl.lab(e.r(8))))
        L.word(e, e.r(6), e.lt_resource)
        L.word(e, e.r(3), e.cl.ret("Create"))
    for i in range(5):
        vt = T.persist_alloc(e, 12)
        L.word(e, vt + 8, T.fake_function(e, create))
        factory = T.persist_alloc(e, 4, struct.pack(">I", vt))
        e.lt_factories.append(factory)
        L.word(e, e.toc + 0xd7c + i * 4, factory)
    e.lt_s.real.update(e.func(x) for x in LABELS)
    def interlocked(e, increment):
        p = e.r(3)
        old = T.s32(e.u32(p))
        e.cl.rec("Increment" if increment else "Decrement", (old,))
        value = T.u32(old + (1 if increment else -1))
        L.word(e, p, value)
        e.set_r(3, value)
    e.lt_s.imp("InterlockedIncrement__10NS_MacDozeFPl", lambda e: interlocked(e, True))
    e.lt_s.imp("InterlockedDecrement__10NS_MacDozeFPl", lambda e: interlocked(e, False))
    def destructor(e):
        e.cl.rec("~Resource", (e.cl.lab(e.r(3)),))
        if T.s16(e.r(4)) > 0:
            e.cl.rec("delete", (e.cl.lab(e.r(3)),))
    L.word(e, e.lt_resource_vt + 16, T.fake_function(e, destructor))


def case(r):
    n = r.randint(1, 14)
    ids = sorted(r.sample(range(0, 0xffffffff), n))
    items = [(x, r.choice([0, 1, 2, 3, 4, 5, 6, 65535]), 240 + i * 8, r.randrange(2))
             for i, x in enumerate(ids)]
    rawids = ids[:]
    if n > 1 and r.random() < .3:
        rawids[r.randrange(1, n)] = ids[0]
    raw = b"".join(struct.pack("<IHHI", x, items[i][1], items[i][2], 0xa5a5a5a5)
                   for i, x in enumerate(rawids))
    return dict(items=items, raw=raw, slot=r.choice([-1] + list(range(n))),
                id=r.choice(ids + [0, 0xffffffff, r.getrandbits(32)]),
                queues={"StorageOpen": [r.choice([-1, 0, 2, -20])],
                        "OpenFile": [r.choice([-1, 0, 7])], "Create": [r.choice([-1, 0, 10])]},
                refs=r.getrandbits(32))


def data(c):
    b = bytearray(512)
    for i in range(len(c["items"])):
        b[240 + i * 8:240 + i * 8 + 7] = f"r{i:04d}:".encode() + b"\0"
    return bytes(b)


def ppc(e, c, op):
    L.word(e, e.lt_resource, e.lt_resource_vt)
    arr = e.alloc(len(c["items"]) * 12)
    for i, (id, type, off, cached) in enumerate(c["items"]):
        e.write(arr + i * 12, struct.pack(">IHHI", id, type, off, e.lt_resource if cached else 0))
    # Resource filenames use the index table base, so append names at their offsets.
    base = e.alloc(512, data(c))
    e.write(base, e.read(arr, len(c["items"]) * 12))
    arr = base
    obj = e.alloc(24, struct.pack(">6I", e.toc + 0xdc0, c["refs"], arr,
                                arr + (len(c["items"]) - 1) * 12, e.toc + 0xdf8, 0))
    if op == 8 and c["slot"] < 0:
        L.word(e, obj + 8, 0)
    fixed = {1: obj, 2: obj + 16, 3: e.lt_resource}
    fixed.update({10 + i: x for i, x in enumerate(e.lt_factories)})
    saved = L.install(e, e.lt_s, fixed, c["queues"])
    out = e.alloc(4, struct.pack(">I", 7))
    result = e.alloc(4)
    handle = 0 if c["slot"] < 0 else arr + c["slot"] * 12
    raw = e.alloc(len(c["raw"]) + 4, c["raw"] + b"\0" * 4)
    e.cl.labels[raw] = 4
    path = CL.new_text(e, "folder:")
    try:
        if op in (0, 7, 8, 9):
            flag = (c["id"] & 3) - 1 if op == 9 else -1
            T.call_ppc(e, e.func(LABELS[op]), [e.lt_resource if op == 7 else obj, T.u32(flag)])
            value = 0
        elif op in (1, 2):
            T.call_ppc(e, e.func(LABELS[op]), [result, obj, handle, out])
            value = T.s32(e.u32(result))
        elif op == 3:
            file = e.alloc(8)
            T.call_ppc(e, e.func("__ct__Q33bfl10FileSystem5CFileFv"), [file])
            T.call_ppc(e, e.func(LABELS[op]), [result, obj, file, handle])
            value = T.s32(e.u32(result))
            T.call_ppc(e, e.func("__dt__Q33bfl10FileSystem5CFileFv"), [file, 0xffff])
        elif op == 4:
            T.call_ppc(e, e.func(LABELS[op]), [result, obj, path, raw, len(c["items"])])
            value = T.s32(e.u32(result))
        else:
            v = T.call_ppc(e, e.func(LABELS[op]), [obj, handle if op == 5 else c["id"]])[0]
            value = v if op == 5 else (int((v - arr) // 12) if v else -1)
        state = [int(e.u32(obj + 8) != 0), e.u32(obj + 4)]
        if op not in (0, 9) and (op != 4 or e.u32(obj + 8)):
            if op == 4 and c["queues"]["StorageOpen"][0] >= 0:
                arr = raw
            for i in range(len(c["items"])):
                id, typ, off, ptr = struct.unpack(">IHHI", e.read(arr + i * 12, 12))
                state += [id, typ, off, e.cl.lab(ptr)]
        if op == 7:
            state.append(e.u32(e.lt_resource + 4))
        first_for_last = raw if op == 4 and c["queues"]["StorageOpen"][0] >= 0 else base
        last = (e.u32(obj + 12) - first_for_last) // 12 if e.u32(obj + 12) else -2
        return dict(result=value, out=e.cl.lab(e.u32(out)), state=state, last=last, log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, c, op):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    obj = F(lib, "tpw_index_new", VP)()
    values = [v for item in c["items"] for v in item]
    buf = C.create_string_buffer(data(c))
    F(lib, "tpw_index_seed", None, VP, C.POINTER(U32), U32, VP, U32)(
        obj, (U32 * len(values))(*values), len(c["items"]), buf, len(buf.raw) - 1)
    fixed = {1: obj, 2: F(lib, "tpw_index_storage", VP, VP)(obj),
             3: F(lib, "tpw_index_resource", VP)()}
    fixed.update({10 + i: F(lib, "tpw_index_factory", VP, I32)(i) for i in range(5)})
    CL.host_begin(lib, fixed, dict(c["queues"], References=[c["refs"]]))
    raw = C.create_string_buffer(c["raw"])
    out = I32()
    if op == 9:
        state = (U32 * 3)()
        F(lib, "tpw_index_destroy", None, VP, I32, C.POINTER(U32))(obj, (c["id"] & 3) - 1, state)
        return dict(result=0, out=-1, state=list(state[:2]), last=T.s32(state[2]), log=CL.host_end(lib))
    value = F(lib, "tpw_index_call", I32, VP, I32, I32, U32, VP, U32, C.POINTER(I32))(
        obj, op, c["slot"], c["id"], raw, len(c["items"]), C.byref(out))
    log = CL.host_end(lib)
    state = (U32 * (2 + 4 * len(c["items"])))()
    F(lib, "tpw_index_read", None, VP, C.POINTER(U32))(obj, state)
    observed = list(state[:2]) if op == 0 or (op == 4 and state[0] == 0) else list(state)
    if op == 7:
        observed.append(1)
    return dict(result=value, out=out.value, state=observed,
                last=F(lib, "tpw_index_last", I32, VP)(obj), log=log)


def valid(r, op):
    c = case(r)
    if op in (2, 3):
        c["slot"] = r.randrange(len(c["items"]))
    return c


def edges(op):
    cases = []
    r = random.Random(71)
    for type in (0, 1, 2, 3, 4, 5, 6, 65535):
        for cached in (0, 1):
            c = valid(r, op)
            c["slot"] = 0
            c["items"][0] = (0, type, 240, cached)
            c["id"] = 0
            c["queues"] = {"StorageOpen": [0], "OpenFile": [0], "Create": [0]}
            c["raw"] = b"".join(struct.pack("<IHHI", id, typ, off, 0xa5a5a5a5)
                                  for id, typ, off, _ in c["items"])
            cases.append(c)
    return cases


TESTS = [FnTest(name, lambda e, c, op=i: ppc(e, c, op), lambda l, c, op=i: host(l, c, op),
                rnd=lambda r, op=i: valid(r, op), edge=lambda op=i: edges(op)) for i, name in enumerate([
    "TbIResourceIndex::TbIResourceIndex", "TbIResourceIndex::GetResource",
    "TbIResourceIndex::OpenResource", "TbIResourceIndex::OpenResourceFile",
    "TbIResourceIndex::Initialise", "TbIResourceIndex::GetResourceType",
    "TbIResourceIndex::GetResourceItemHandle", "TbResource::TbResource", "TbIResourceIndex::Close",
    "TbIResourceIndex::~TbIResourceIndex", "TbIResourceIndex::GetItem"])]


def resource_ppc(e, c, op):
    L.word(e, e.lt_resource, e.lt_resource_vt)
    L.word(e, e.lt_resource + 4, c)
    saved = L.install(e, e.lt_s, {3: e.lt_resource}, {})
    try:
        label = ["AddRef", "Release", "ManagerAddRef", "ManagerRelease"][op] + "__10TbResourceFv"
        value = T.call_ppc(e, e.func(label), [e.lt_resource])[0]
        return dict(result=T.s32(value) if op in (1, 3) else 0,
                    after=0 if op in (1, 3) and c == 1 else T.s32(e.u32(e.lt_resource + 4)), log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def resource_host(lib, c, op):
    CL.host_begin(lib, {})
    after = I32()
    value = F(lib, "tpw_resource_call", I32, I32, I32, C.POINTER(I32))(op, c, C.byref(after))
    return dict(result=value, after=after.value, log=CL.host_end(lib))


TESTS += [FnTest("TbResource::" + name, lambda e, c, op=i: resource_ppc(e, c, op),
                 lambda l, c, op=i: resource_host(l, c, op),
                 edge=lambda: [0, 1, 2, -1, -2147483648, 2147483647], rnd=lambda r: T.s32(r.getrandbits(32)))
          for i, name in enumerate(["AddRef", "Release", "ManagerAddRef", "ManagerRelease"])]
