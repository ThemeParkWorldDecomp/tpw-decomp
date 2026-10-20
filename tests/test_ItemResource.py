"""Item resources, including InitItem and the sole virtual destructor.

Strings are functional and unrecorded. All other external calls record arguments and
return queued results. RegisterItem runs the original list constructor and AddHead.
"""
import ctypes as C
import itertools
import random

import calllog as CL
import test_FSSystem as FS
import test_TPResources as RS
import tpwtest as T

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_ItemResource"
VP, I32, I64 = C.c_void_p, C.c_int32, C.c_int64
F, w32 = FS.F, FS.w32
ADDRESSES = [0x101612CC, 0x10162470, 0x10163584, 0x10163494, 0x10163924,
             0x10163D88, 0x10163E98, 0x10163FDC, 0x10164120, 0x10164238,
             0x10164374, 0x10164574]
NAMES = ["CTPItemResource::CTPItemResource", "CTPItemResource::~CTPItemResource",
         "CTPItemResource::InitItem", "CTPItemListResource::RegisterItem",
         "CTPItemListResource::FindItem", "CTPItemResource::GetSamFile",
         "CTPItemResource::GetEasySamFile", "CTPItemResource::GetOnlineSamFile",
         "CTPItemResource::GetCoasterSamFile", "CTPItemResource::GetTextFile",
         "CTPItemResource::HasSounds", "CTPItemResource::HasParticles"]
GLOBALS = [0x4164, 0x4178, 0x418C, 0x4240, 0x4268, 0x427C, 0x4290,
           0x42A4, 0x42B8, 0x4394, 0x45B0]
GTP, GTEXT, ITEM_VT, LIST_VT = 0x103B74C8, 0x1037FF18, 0x102A6F68, 0x102A6F90
RESULTS = [-1, 0, 1, 7, 0x7FFFFFFF, -0x80000000, -2]
TEXTS = ["", "a", "MiXeD", "folder:Ride", "pRide_01_asset.wad", "a_b_c",
         "a_b_c_d", "p_1_2", "P_1_2", "_", "__", "p__", "_x_", "x_", "x" * 121, "\xe9Ride"]
STUBS = None


def build_stubs():
    s = CL.Stubs()
    s.real.update(ADDRESSES + [0x10164610, 0x10164D9C])
    FS.string_stubs(s)

    def ctor(e):
        e.cl.rec("CFileStorage", (e.cl.newlab(e.r(3)),))
        w32(e, e.r(3), RS.DISC_VT)
        w32(e, e.r(3) + 4, 0)

    def copy(e):
        access = e.u32(e.r(4) + 4)
        e.cl.rec("CFileStorage::copy", (e.cl.newlab(e.r(3)), e.cl.lab(e.r(4)), e.cl.lab(access)))
        w32(e, e.r(3), RS.DISC_VT)
        w32(e, e.r(3) + 4, access)

    def assign(e):
        access = e.u32(e.r(4) + 4)
        e.cl.rec("CFileStorage::assign", (e.cl.lab(e.r(3)), e.cl.lab(e.r(4)), e.cl.lab(access)))
        w32(e, e.r(3) + 4, access)
        # The reference result stays in r3.

    def dtor(e):
        e.cl.rec("~CFileStorage", (e.cl.lab(e.r(3)), e.cl.lab(e.u32(e.r(3) + 4))))

    def storage_open(wad):
        def stub(e):
            ints = (e.cl.lab(e.r(4)), e.r(6)) if wad else (e.cl.lab(e.r(4)),)
            e.cl.rec("CWadFileStorage::Open" if wad else "CDiscFileStorage::Open", ints,
                     (e.cstr(e.r(5), limit=400)[:159],))
            w32(e, e.r(4) + 4, e.ir["access"])
            w32(e, e.r(3), e.cl.ret("Storage::Open", -1))
        return stub

    def find(e):
        label, info = e.cl.lab(e.r(4)), e.r(6)
        ptr = e.u32(info + 0x20)
        e.cl.rec("CFileStorage::FindFirst", (label, T.s32(ptr - info - 0x24)),
                 (e.cstr(e.r(5), limit=400)[:159], e.cstr(ptr, limit=0x104)[:159]))
        result = T.s32(e.cl.ret(f"CFileStorage::FindFirst:{label}", -1))
        RS.fill_info(e, info, "first", label, result >= 0)
        w32(e, e.r(3), result)

    def open_file(e):
        label = e.cl.lab(e.r(4))
        name = e.cstr(e.r(6), limit=800)
        e.cl.rec("CFileStorage::OpenFile", (label, e.cl.lab(e.r(5)), e.r(7), e.cl.lab(e.r(8))),
                 tuple(name[k:k + 159] for k in (0, 159, 318)))
        w32(e, e.r(5) + 4, e.ir["access"])
        w32(e, e.r(3), e.cl.ret(f"CFileStorage::OpenFile:{label}", -1))

    def language(e):
        e.cl.rec("CText::GetLanguageName", (e.cl.lab(e.r(3)),))
        e.set_r(3, CL.new_text(e, e.ir["language"]))

    def equal(e):
        a, b = FS.s_read(e, e.r(3), False), FS.s_read(e, e.r(4), False)
        e.cl.rec("TPStrEq", (), (a[:159], b[:159]))
        e.set_r(3, int(FS.icmp(a, b) == 0))

    def remove_extension(e):
        ext, obj = FS.s_read(e, e.r(3), False), e.r(4)
        name = FS.s_read(e, obj, False)
        e.cl.rec("RemoveExtension", (), (ext[:159], name[:159]))
        if len(ext) <= len(name) and FS.icmp(name[len(name) - len(ext):], ext) == 0:
            FS.s_write(e, obj, name[:len(name) - len(ext)], False)
        e.set_r(3, obj)

    def wad_file(e):
        e.cl.rec("FS_IsWadFile", (e.cl.lab(e.r(3)),), (e.cstr(e.u32(e.r(3) + 0x20), limit=400)[:159],))
        e.set_r(3, T.u32(e.cl.ret("FS_IsWadFile", 0)))

    def search(reverse):
        def stub(e):
            name = FS.s_read(e, e.r(3), False)
            needle = e.cstr(e.r(4), limit=400)
            start = e.r(5)
            if reverse:
                index = -1 if len(needle) >= len(name) else name.rfind(needle, 0, min(start + len(needle), len(name)))
            else:
                index = name.find(needle, min(start, len(name)))
            e.set_r(3, T.u32(index))
        return stub

    def eq_text(e):
        e.set_r(3, int(FS.s_read(e, e.r(3), False) == e.cstr(e.r(4))))

    def new(e):
        if e.ir["fail_new"]:
            e.ir["fail_new"] = False
            e.set_r(3, 0)
        else:
            size = e.r(3)
            block = e.alloc(size, b"\xa5" * size)
            e.alloc_log.append(("new", block, size))
            e.set_r(3, block)

    def delete(e):
        block = e.r(3)
        if block:
            e.alloc_log.append(("delete", block, 0))
            if e.cl.lab(block) > 0:
                e.cl.rec("delete", (e.cl.lab(block),))
                e.cl.forget(block)

    s.imp("__ct__Q33bfl10FileSystem12CFileStorageFv", ctor)
    s.imp("__ct__Q33bfl10FileSystem12CFileStorageFRCQ33bfl10FileSystem12CFileStorage", copy)
    s.imp("__as__Q33bfl10FileSystem12CFileStorageFRCQ33bfl10FileSystem12CFileStorage", assign)
    s.imp("__dt__Q33bfl10FileSystem12CFileStorageFv", dtor)
    s.imp("Open__Q33bfl10FileSystem16CDiscFileStorageFPCc", storage_open(False))
    s.imp("Open__Q33bfl10FileSystem15CWadFileStorageFPCcUl", storage_open(True))
    s.imp("FindFirst__Q33bfl10FileSystem12CFileStorageFPCcRQ33bfl10FileSystem9CFileInfo", find)
    s.imp("OpenFile__Q33bfl10FileSystem12CFileStorageFRQ33bfl10FileSystem5CFilePCcUlPv", open_file)
    s.imp("Find__15TbStringBase<c>CFPCcUi", search(False))
    s.imp("FindReverse__15TbStringBase<c>CFPCcUi", search(True))
    s.imp("__eq__15TbStringBase<c>CFPCc", eq_text)
    s.imp("__nw__FUl", new)
    s.imp("__dl__FPv", delete)
    s.internal(0x1015E93C, language)
    s.internal(0x100053F4, equal)
    s.internal(0x10005524, remove_extension)
    s.internal(0x10004AB8, wad_file)
    return s


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    FS.make_vtables(runner.emu)
    STUBS = build_stubs()


def ppc_item(e, name, path, label, owner):
    item = e.alloc(0x38, b"\xa5" * 0x38)
    FS.s_write(e, item, name, False)
    FS.s_write(e, item + 0x14, path, False)
    w32(e, item + 0x28, RS.WAD_VT if e.ir["wad_vt"] else RS.DISC_VT)
    w32(e, item + 0x2C, 0)
    w32(e, item + 0x30, ITEM_VT)
    w32(e, item + 0x34, owner)
    e.cl.labels[item] = label
    return item


def ppc_begin(e, c):
    e.heap_in_scratch = True
    e.ir = dict(language=c["language"], wad_vt=c["wad_vt"], fail_new=c["fail"])
    e.cl = CL.PpcLog({GTEXT: 8}, c["q"])
    for off, text in c["globals"].items():
        FS.s_write(e, GTP + off, text, False)
    access = e.ir["access"] = e.alloc(8)
    e.cl.labels[access] = 9
    resource = e.ir["resource"] = e.alloc(0x50, b"\xa5" * 0x50)
    e.cl.labels[resource] = 2
    item = e.ir["item"] = ppc_item(e, c["name"], c["path"], 1, access)
    w32(e, item + 0x2C, access if c["initial_access"] else 0)
    e.cl.labels[item + 0x28] = 10
    info = e.ir["info"] = e.alloc(0x128, b"\0" * 0x128)
    e.write(info + 0x24, b"x" * c["offset"] + c["info"].encode("latin-1") + b"\0")
    w32(e, info + 0x20, info + 0x24 + c["offset"])
    e.cl.labels[info] = 4
    e.ir["folder"] = FS.new_string(e, c["folder"], False, 5)
    e.ir["query"] = FS.new_string(e, c["query"], False, 6)
    file = e.ir["file"] = e.alloc(8, b"\0" * 8)
    e.cl.labels[file] = 3
    items = e.ir["items"] = [ppc_item(e, name, "", 20 + k, access) for k, name in enumerate(c["names"])]
    lst = RS.ppc_list(e, LIST_VT, items[:c["existing"]])[0] if c["has_list"] else 0
    w32(e, resource + 0x4C, lst)
    if c["op"] == 0:
        e.write(item, b"\xa5" * 0x38)
    e.alloc_log = []


def ppc_observe(e, c):
    s, L = e.ir, e.cl
    item = s["item"]
    # Neither side reads destroyed strings as live objects.
    state = None if c["op"] == 1 else dict(name=FS.s_read(e, item, False), path=FS.s_read(e, item + 0x14, False),
                                           owner=L.lab(e.u32(item + 0x34)), access=L.lab(e.u32(item + 0x2C)))
    lst = e.u32(s["resource"] + 0x4C)
    return dict(item=state, list=RS.ppc_walk(e, lst) if lst else None,
                owners=[L.lab(e.u32(p + 0x34)) for p in s["items"]],
                file=L.lab(e.u32(s["file"] + 4)))


def host_item(lib, name, path, owner):
    block = F(lib, "tpw_ir_block", VP)()
    info = F(lib, "tpw_ir_info", VP, C.c_char_p, I32)(b"", 0)
    folder = F(lib, "tpw_ir_string", VP, C.c_char_p)(b"")
    item = F(lib, "tpw_ir_construct", VP, VP, VP, VP)(block, info, folder)
    F(lib, "tpw_ir_set", None, VP, C.c_char_p, C.c_char_p, VP)(item, name.encode("latin-1"), path.encode("latin-1"), owner)
    return item


def host_begin(lib, c):
    h = {}
    for off, text in c["globals"].items():
        F(lib, "tpw_ir_globals", None, I32, C.c_char_p)(off, text.encode("latin-1"))
    F(lib, "tpw_ir_language", None, C.c_char_p)(c["language"].encode("latin-1"))
    h["access"] = F(lib, "tpw_ir_global", VP, I32)(1)
    h["item"] = host_item(lib, c["name"], c["path"], h["access"])
    F(lib, "tpw_ir_access", None, VP, I32)(h["item"], int(c["initial_access"]))
    h["info"] = F(lib, "tpw_ir_info", VP, C.c_char_p, I32)(c["info"].encode("latin-1"), c["offset"])
    h["folder"] = F(lib, "tpw_ir_string", VP, C.c_char_p)(c["folder"].encode("latin-1"))
    h["query"] = F(lib, "tpw_ir_string", VP, C.c_char_p)(c["query"].encode("latin-1"))
    h["file"] = F(lib, "tpw_ir_file", VP)()
    h["items"] = [host_item(lib, name, "", h["access"]) for name in c["names"]]
    lst = F(lib, "tpw_ir_list", VP)() if c["has_list"] else None
    if lst:
        for item in h["items"][:c["existing"]]:
            F(lib, "tpw_ir_append", None, VP, VP)(lst, item)
    h["resource"] = F(lib, "tpw_ir_resource", VP, VP)(lst)
    if c["op"] == 0:
        h["item"] = F(lib, "tpw_ir_block", VP)()
    # Getting the member address does not require a constructed object.
    fixed = {1: h["item"], 2: h["resource"], 3: h["file"], 4: h["info"], 5: h["folder"],
             6: h["query"], 8: F(lib, "tpw_ir_global", VP, I32)(0), 9: h["access"],
             10: F(lib, "tpw_ir_member", VP, VP, I32)(h["item"], 2)}
    fixed.update({20 + k: item for k, item in enumerate(h["items"])})
    CL.host_begin(lib, fixed, c["q"])
    h["heap"] = (I32 * 4).in_dll(lib, "tpw_cl_heap")
    for k in range(4):
        h["heap"][k] = 0
    return h


def host_observe(lib, h, c):
    member = F(lib, "tpw_ir_member", VP, VP, I32)
    label = F(lib, "tpw_cl_lab", I32, VP)
    text = F(lib, "tpw_ir_text", C.c_char_p, VP)
    item = h["item"]
    state = None if c["op"] == 1 else dict(name=text(member(item, 0)).decode("latin-1"),
                                           path=text(member(item, 1)).decode("latin-1"),
                                           owner=label(member(item, 3)), access=label(member(item, 4)))
    walk = (I64 * 40)()
    n = F(lib, "tpw_ir_walk", I32, VP, VP)(h["resource"], walk)
    count = int(walk[0]) if n else 0
    lst = dict(count=count, fwd=list(walk)[1:1 + count], bwd=list(walk)[1 + count:1 + 2 * count]) if n else None
    return dict(item=state, list=lst, owners=[label(member(p, 3)) for p in h["items"]],
                file=label(F(lib, "tpw_ir_file_access", VP, VP)(h["file"])))


def make_test(op):
    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            ppc_begin(e, c)
            s = e.ir
            snapshots, results = [], []
            if op == 3:
                for k in range(c["calls"]):
                    result = T.call_ppc(e, ADDRESSES[op], [s["resource"], s["items"][c["existing"] + k]])[0]
                    results.append(T.s32(result))
                    snapshots.append(ppc_observe(e, c))
            else:
                args = [s["item"], s["info"], s["folder"]] if op in (0, 2) else (
                    [s["item"], c["flag"]] if op == 1 else [s["resource"], s["query"]] if op == 4 else
                    [s["item"], s["file"]] if 5 <= op <= 9 else [s["item"]])
                result = T.call_ppc(e, ADDRESSES[op], [T.u32(a) for a in args])[0]
                results.append(0 if op in (0, 1, 2) else T.s32(result))
                if op == 0:
                    assert e.u32(s["item"] + 0x30) == ITEM_VT
                    assert e.u32(s["item"] + 0x34) == 0xA5A5A5A5
                snapshots.append(ppc_observe(e, c))
            if op == 3:
                lst = e.u32(s["resource"] + 0x4C)
                if lst:
                    assert e.u32(lst) == LIST_VT
            heap = [sum(kind == key for kind, _, _ in e.alloc_log) for key in ("new", "new[]", "delete", "delete[]")]
            out = dict(results=results, snapshots=snapshots, heap=heap, log=e.cl.log)
        finally:
            CL.Stubs.deactivate(e, saved)
            e.cl = None
        return out

    def host(lib, c):
        h = host_begin(lib, c)
        snapshots, results = [], []
        if op == 3:
            for k in range(c["calls"]):
                result = F(lib, "tpw_ir_register", I32, VP, VP, I32)(h["resource"], h["items"][c["existing"] + k], int(c["fail"] and k == 0))
                results.append(result)
                snapshots.append(host_observe(lib, h, c))
        else:
            if op == 0:
                F(lib, "tpw_ir_construct", VP, VP, VP, VP)(h["item"], h["info"], h["folder"])
                assert F(lib, "tpw_ir_owner_untouched", I32, VP)(h["item"]) == 1
                result = 0
            elif op == 2:
                F(lib, "tpw_ir_init", None, VP, VP, VP)(h["item"], h["info"], h["folder"])
                result = 0
            elif op == 4:
                result = F(lib, "tpw_ir_find", I32, VP, VP)(h["resource"], h["query"])
            else:
                result = F(lib, "tpw_ir_call", I32, I32, VP, VP, I32)(op, h["item"], h["file"], T.s16(c["flag"]))
            results.append(result)
            snapshots.append(host_observe(lib, h, c))
        heap = list(h["heap"])
        log = CL.host_end(lib)
        return dict(results=results, snapshots=snapshots, heap=heap, log=log)

    return T.FnTest(NAMES[op], ppc, host, edge=lambda: edges(op), rnd=lambda r: random_case(r, op))


def base(op):
    return dict(op=op, name="Ride", path="original:path", info="pRide_01_asset.wad", folder="folder.wad", offset=0,
                globals={off: f"g{off:x}" for off in GLOBALS}, language="English", wad_vt=False,
                names=[], query="", existing=0, has_list=False, calls=1, fail=False, flag=-1,
                initial_access=False,
                q={"FS_IsWadFile": [0], "Storage::Open": [-1], "CFileStorage::OpenFile:10": [0],
                   "CFileStorage::FindFirst:10": [0, 0, 0]})


def init_case(op, name, folder, wad, result):
    c = base(op)
    c.update(info=name, folder=folder)
    c["globals"][0x4394] = ".wad"
    c["q"]["FS_IsWadFile"] = [wad]
    c["q"]["Storage::Open"] = [result]
    return c


def edges(op):
    if op in (0, 2):
        out = [init_case(op, name, folder, wad, result)
                for name in TEXTS + ["pRide_01_asset.WAD", "pRide_01", "p_a_b_c", "p__suffix", "a___", "__.wad"]
                for folder in ("", "folder:Ride", "folder.WAD") for wad in (0, 1)
                for result in (-1, 0, -0x80000000, 0x7FFFFFFF)]
        if op == 2:
            for wad in (0, 1):
                for result in RESULTS:
                    c = init_case(op, "pRide_1_asset.wad", "folder.wad", wad, result)
                    c["initial_access"] = True
                    out.append(c)
        return out
    if op == 1:
        out = []
        for flag in (-32768, -1, 0, 1, 32767, 0x10000, 0xFFFF):
            c = base(op)
            c["flag"] = flag
            out.append(c)
        return out
    if op == 3:
        out = []
        for has_list, existing in [(False, 0)] + [(True, n) for n in (0, 1, 2, 5)]:
            for calls in (1, 2):
                c = base(op)
                c.update(has_list=has_list, existing=existing, calls=calls,
                         names=[f"item{k}" for k in range(existing + calls)])
                out.append(c)
        for calls in (1, 2):
            c = base(op)
            c.update(fail=True, calls=calls, names=[f"item{k}" for k in range(calls)])
            out.append(c)
        return out
    if op == 4:
        out = [base(op)]
        for names in ([], [""], ["One"], ["One", "Two", "Three"], ["same", "SAME"], ["\xe9Ride", "RIDE"], ["x" * 180]):
            for query in ["", "absent"] + names + [n.swapcase() for n in names]:
                c = base(op)
                c.update(names=names, existing=len(names), has_list=True, query=query)
                out.append(c)
        return out
    if op == 10:
        out = []
        for results in itertools.product([-1, 0, 5, -0x80000000, 0x7FFFFFFF], repeat=3):
            c = base(op)
            c["q"]["CFileStorage::FindFirst:10"] = list(results)
            out.append(c)
        return out
    out = []
    for result in RESULTS:
        for name in ("", "a", "folder:ride", "x" * 121):
            for blank in (False, True):
                c = base(op)
                c.update(name=name, wad_vt=blank)
                if blank:
                    c["globals"] = {off: "" for off in GLOBALS}
                    c["language"] = ""
                c["q"]["CFileStorage::OpenFile:10"] = [result]
                c["q"]["CFileStorage::FindFirst:10"] = [result]
                out.append(c)
    return out


def random_text(r):
    if r.random() < 0.6:
        return r.choice(TEXTS)
    return "".join(r.choice("abCD_p: .19") for _ in range(r.randrange(0, 140)))


def random_case(r, op):
    c = base(op)
    c.update(name=random_text(r), path=random_text(r), info=random_text(r), folder=random_text(r),
             language=random_text(r), offset=r.randrange(3), wad_vt=bool(r.randrange(2)),
             initial_access=bool(r.randrange(2)))
    c["globals"] = {off: random_text(r) for off in GLOBALS}
    c["globals"][0x4394] = r.choice([".wad", ".WAD", ".other", ""])
    c["q"] = {"FS_IsWadFile": [r.choice([0, 1, -1])], "Storage::Open": [r.choice(RESULTS)],
              "CFileStorage::OpenFile:10": [r.choice(RESULTS)],
              "CFileStorage::FindFirst:10": [r.choice(RESULTS) for _ in range(3)]}
    if op == 1:
        c["flag"] = r.choice([-32768, -1, 0, 1, 7, 32767, 0x10000, 0xFFFF])
    if op in (3, 4):
        c.update(existing=r.randrange(7), has_list=bool(r.randrange(2)), calls=r.randrange(1, 3))
        if not c["has_list"]:
            c["existing"] = 0
        count = c["existing"] + (c["calls"] if op == 3 else 0)
        c["names"] = [random_text(r) for _ in range(count)]
        c["fail"] = op == 3 and not c["has_list"] and r.random() < 0.2
        c["query"] = r.choice(c["names"]).swapcase() if c["names"] and r.random() < 0.65 else random_text(r)
    return c


TESTS = [make_test(op) for op in range(12)]
