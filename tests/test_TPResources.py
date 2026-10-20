"""Differential tests for TPSettings.cpp's resources (src/TPSettings.cpp, include/TPResource.h;
Milestone 1 unit 6, part 2), against libtpw_unit_TPSettings.dylib (tests/unit_TPSettings.cpp's
fakes; see calllog.py), with test_FSSystem.py's PowerPC strings.

A case builds a resource (its strings, its storage list, a manager, the search state), the
storages (disc or .wad: the original's vtables, whose library members are fakes answering
from per-storage queues), a file, a directory entry, the strings passed and, for the
manager, its list of fake resources; it runs the function and compares the logs, the
result and the objects (a list as its count and its values head to tail and tail to head).
"""
import ctypes as C
import random

import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_TPSettings"

R, M = "CTPBaseResource::", "CTPResourceManager::"
SL, RL = "TbTList<CFileStorage*>::", "TbTList<CTPBaseResource*>::"
F_ = {
    R + "~CTPBaseResource": 0x1015F464, "TbPtrList<CFileStorage>::~TbPtrList": 0x1015F598,
    R + "AddDiscStorage": 0x1015F648, R + "AddWADStorage": 0x1015F780,
    "CWadFileStorage::~CWadFileStorage": 0x1015F8A0, R + "AddAllStorages": 0x1015F948,
    R + "AddLocalStorage": 0x1015F9E8, R + "AddRemoteStorage": 0x1015FBD4,
    R + "pGetCurrentSearchStorage": 0x1015FDF4, R + "pGetLastFoundFileStorage": 0x1015FE40,
    "strRemoveRootDirectory": 0x1015FE8C, "QualifyFilename": 0x10160078,
    R + "OpenFile(storage)": 0x101604C0, R + "OpenFile": 0x10160660, R + "FindFirst": 0x10160830,
    R + "FindNext": 0x10160A2C,
    "ctor:CTPSharedTextureResource": 0x10160B90, "ctor:CTPTextureResource": 0x10160CE8,
    "ctor:CTPSoundResource": 0x10160E68, "ctor:CTPAdvisorResource": 0x10160FB8,
    "ctor:CTPFontResource": 0x101613BC, "ctor:CTPSpriteResource": 0x1016150C, "ctor:CTPUIResource": 0x10161660,
    "ctor:CTPTerrainResource": 0x101617E0, "ctor:CTPLanguageResource": 0x10161950,
    "ctor:CTPFMVResource": 0x10161AA4, "CTPIntermediate::~CTPIntermediate": 0x10161BF4,
    "dtor:CTPSharedTextureResource": 0x10161C88, "dtor:CTPTextureResource": 0x10161DD4,
    "dtor:CTPSoundResource": 0x10161F34, "dtor:CTPAdvisorResource": 0x10162078,
    "dtor:CTPFontResource": 0x10162524, "dtor:CTPSpriteResource": 0x10162664, "dtor:CTPUIResource": 0x101627A8,
    "dtor:CTPTerrainResource": 0x1016292C, "dtor:CTPLanguageResource": 0x10162A98,
    "dtor:CTPFMVResource": 0x10162BDC,
    "CTPIntermediate::GetSamFile": 0x10162D1C, "CTPIntermediate::GetOnlineSamFile": 0x10162E24,
    "MakeFullFilename": 0x10162F44, R + "OpenQualifiedFile(storage)": 0x10162FE0,
    R + "OpenQualifiedFile": 0x101630D8, "CTPSoundResource::FindSoundFile": 0x101632AC,
    SL + "TbTList": 0x101648B4, SL + "~TbTList": 0x1016493C, SL + "Find": 0x10164A0C, SL + "RemoveAll": 0x10164AC0,
    SL + "RemoveHead": 0x10164B94, SL + "AddTail": 0x10164C88,
    M + "CTPResourceManager": 0x10164E84, M + "~CTPResourceManager": 0x10164EC8,
    "TbPtrList<CTPBaseResource>::~TbPtrList": 0x10164FB0, M + "RegisterResource": 0x10165054,
    M + "RegisterResource(shared)": 0x10165144, M + "UnregisterResource": 0x10165244, M + "OpenFile": 0x10165344,
    M + "FindResource": 0x10165448, M + "pGetCurrentResource": 0x10165538, M + "pSetCurrentResource": 0x10165580,
    RL + "TbTList": 0x101655D8, RL + "~TbTList": 0x10165640, RL + "Find": 0x101656F0, RL + "RemoveAll": 0x10165774,
    RL + "RemoveAt": 0x10165828, RL + "RemoveHead": 0x10165910, RL + "AddHead": 0x101659E8}
A = dict(gTPGlobals=0x103B74C8, gTPFileInfo=0x102B3458)
# class: vtable on the Mac, offsets of the storage members it adds
CLASSES = {"CTPSharedTextureResource": (0x102A7078, ()), "CTPTextureResource": (0x102A7050, (0x40,)),
           "CTPSoundResource": (0x102A703C, ()), "CTPAdvisorResource": (0x102A7014, (0x40, 0x48)),
           "CTPFontResource": (0x102A6F4C, ()), "CTPSpriteResource": (0x102A6F24, ()),
           "CTPUIResource": (0x102A6EFC, (0x40, 0x48)), "CTPTerrainResource": (0x102A6ECC, (0x40,)),
           "CTPLanguageResource": (0x102A6E88, ()), "CTPFMVResource": (0x102A6E60, ())}
VT_CLASS = {vt: cls for cls, (vt, _) in CLASSES.items()}
DISC_VT, WAD_VT = 0x10278BF0, 0x102A70D0
SLIST_VT, SPTR_VT, RLIST_VT, RPTR_VT = 0x102A7130, 0x102A7124, 0x102A7170, 0x102A7164
GLOBALS = dict(mSeparator=0x404C, mSearchPath=0x4060, mCurrentFolder=0x4074, mAllFiles=0x40D8,
               mSamExtension=0x4240, mOnlineSamExtension=0x4268, mOnlinePrefix=0x4290, mWadExtension=0x4394,
               mWadPattern=0x43E4, mWadFolderPattern=0x43F8)
LB = dict(resource=1, manager=2, file=3, info=4, s1=5, s2=6, s3=7, pattern=8, fileinfo=9, list=40, rlist=41,
          out=42, other=28)
STORAGE0, RESOURCE0, MEMBER0 = 10, 20, 30

VP, I32, I64 = C.c_void_p, C.c_int32, C.c_int64
F = FS.F
w32 = FS.w32

# ======================================================================= PowerPC side


class RsState:
    pass


def mid(text, start, count):
    return text[start:start + count]


def icmp_eq(a, b):
    return FS.icmp(a, b) == 0


def fill_info(e, info, kind, label, found):
    w32(e, info, 0x1000 + label)
    w32(e, info + 4, label * 3 + (1 if kind == "next" else 0))
    e.write(info + 8, bytes((label + k) & 0xFF for k in range(0x18)))
    if found:
        text = f"{kind}{label}".encode("latin-1") + b"\0"
        e.write(info + 0x24, text)
        w32(e, info + 0x20, info + 0x24 + (label % 3 if label > 0 else 0))


def build_stubs(emu):
    S = CL.Stubs()
    S.real.update(F_.values())
    FS.string_stubs(S)
    st = lambda e: e.rs   # noqa: E731

    def mid2(e):
        FS.s_write(e, e.r(3), mid(FS.s_read(e, e.r(4), False), e.r(5), e.r(6)), False, full=False)

    def mid1(e):
        FS.s_write(e, e.r(3), FS.s_read(e, e.r(4), False)[e.r(5):], False, full=False)

    def eq(e):
        e.set_r(3, 1 if FS.s_read(e, e.r(3), False) == FS.s_read(e, e.r(4), False) else 0)

    S.imp("Mid__15TbStringBase<c>CFUiUi", mid2)
    S.imp("Mid__15TbStringBase<c>CFUi", mid1)
    S.imp("__eq__15TbStringBase<c>CFRC15TbStringBase<c>", eq)

    def tpstreq(e):
        a, b = FS.s_read(e, e.r(3), False), FS.s_read(e, e.r(4), False)
        e.cl.rec("TPStrEq", (), (a[:159], b[:159]))
        e.set_r(3, 1 if icmp_eq(a, b) else 0)

    S.internal(0x100053F4, tpstreq)

    def add_separator(e):
        text = FS.s_read(e, e.r(3), False)
        e.cl.rec("FS_AddDirectorySeparator", (), (text[:159],))
        if not text.endswith(":"):
            FS.s_write(e, e.r(3), text + ":", False)

    S.internal(0x10004BC0, add_separator)

    def remote_path(e):
        e.cl.rec("CSYS_TPFileInfo::GetRemotePath", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.r(3) + 0x14)

    S.internal(0x10004E08, remote_path)

    # ---- the bullfrog storages and files
    S.imp("__ct__Q33bfl10FileSystem12CFileStorageFv", CL.recorder("CFileStorage", "N"))
    S.imp("__dt__Q33bfl10FileSystem12CFileStorageFv", CL.recorder("~CFileStorage", "L"))
    S.imp("__ct__Q33bfl10FileSystem5CFileFv", CL.recorder("CFile::CFile", "N"))
    S.imp("__dt__Q33bfl10FileSystem5CFileFv", CL.recorder("CFile::~CFile", "L"))

    def disc_open(e):
        L = e.cl
        L.rec("CDiscFileStorage::Open", (L.lab(e.r(4)),), (e.cstr(e.r(5))[:159],))
        w32(e, e.r(3), L.ret("CDiscFileStorage::Open", -1))

    def wad_open(e):
        L = e.cl
        L.rec("CWadFileStorage::Open", (L.lab(e.r(4)), e.r(6)), (e.cstr(e.r(5))[:159],))
        w32(e, e.r(3), L.ret("CWadFileStorage::Open", -1))

    S.imp("Open__Q33bfl10FileSystem16CDiscFileStorageFPCc", disc_open)
    S.imp("Open__Q33bfl10FileSystem15CWadFileStorageFPCcUl", wad_open)

    def find(kind):
        def stub(e):
            L, storage = e.cl, e.r(4)
            lab = L.lab(storage)
            if kind == "first":
                L.rec("CFileStorage::FindFirst", (lab,), (e.cstr(e.r(5))[:159],))
                info = e.r(6)
            else:
                L.rec("CFileStorage::FindNext", (lab,))
                info = e.r(5)
            result = T.s32(L.ret(f"CFileStorage::Find{kind.capitalize()}:{lab}", -1))
            fill_info(e, info, kind, lab, result >= 0)
            w32(e, e.r(3), result)
        return stub

    S.imp("FindFirst__Q33bfl10FileSystem12CFileStorageFPCcRQ33bfl10FileSystem9CFileInfo", find("first"))
    S.imp("FindNext__Q33bfl10FileSystem12CFileStorageFRQ33bfl10FileSystem9CFileInfo", find("next"))

    def storage_open(e):
        L = e.cl
        lab = L.lab(e.r(4))
        L.rec("CFileStorage::OpenFile", (lab, L.lab(e.r(5)), e.r(7), L.lab(e.r(8))), (e.cstr(e.r(6))[:159],))
        w32(e, e.r(3), L.ret(f"CFileStorage::OpenFile:{lab}", -1))

    def storage_full(e):
        L = e.cl
        lab = L.lab(e.r(4))
        name = FS.s_read(e, e.r(6), False)
        L.rec("CFileStorage::FullPath", (lab, L.lab(e.r(5))), (name[:159],))
        FS.s_write(e, e.r(5), f"full{lab}:{name}", False)
        w32(e, e.r(3), L.ret("CFileStorage::FullPath", 0))

    S.imp("OpenFile__Q33bfl10FileSystem12CFileStorageFRQ33bfl10FileSystem5CFilePCcUlPv", storage_open)
    S.imp("FullPath__Q33bfl10FileSystem12CFileStorageFR26TbDynamicStringTemplate<c>RC26TbDynamicStringTemplate<c>",
          storage_full)

    def op_delete(e):
        L, obj = e.cl, e.r(3)
        if obj and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    S.imp("__dl__FPv", op_delete)
    return S


def make_resource_vtable(emu):
    """The manager's fake resources: the destructor (slot 8) and OpenFile (slot 0xc) record."""
    vt = T.persist_alloc(emu, 0x40)

    def dtor(e):
        CL.recorder("~CTPBaseResource", "D")(e)

    def open_file(e):
        L = e.cl
        lab = L.lab(e.r(3))
        L.rec("CTPBaseResource::OpenFile", (lab, L.lab(e.r(4)), e.r(6)), (FS.s_read(e, e.r(5), False)[:159],))
        e.set_r(3, T.u32(L.ret(f"CTPBaseResource::OpenFile:{lab}", 0)))

    for off, fn in ((8, dtor), (0xC, open_file)):
        w32(emu, vt + off, T.fake_function(emu, fn))
    return vt


VT_RES = None


def ppc_list(e, vt, values):
    lst = e.alloc(0x10)
    nodes = [e.alloc(0xC) for _ in values]
    for k, (node, v) in enumerate(zip(nodes, values)):
        w32(e, node, v)
        w32(e, node + 4, nodes[k + 1] if k + 1 < len(nodes) else 0)
        w32(e, node + 8, nodes[k - 1] if k else 0)
    w32(e, lst, vt)
    w32(e, lst + 4, nodes[0] if nodes else 0)
    w32(e, lst + 8, nodes[-1] if nodes else 0)
    w32(e, lst + 0xC, len(values))
    return lst, nodes


def ppc_walk(e, lst):
    L = e.cl
    fwd, n = [], e.u32(lst + 4)
    while n and len(fwd) < 32:
        fwd.append(L.lab(e.u32(n)))
        n = e.u32(n + 4)
    bwd, n = [], e.u32(lst + 8)
    while n and len(bwd) < 32:
        bwd.append(L.lab(e.u32(n)))
        n = e.u32(n + 8)
    return dict(count=T.s32(e.u32(lst + 0xC)), fwd=fwd, bwd=bwd)


def ppc_info_write(e, info, c):
    i = c["info"]
    e.write(info, b"\0" * 0x128)
    w32(e, info, i["f00"])
    w32(e, info + 4, i["attr"])
    e.write(info + 8, bytes(i["f08"]))
    e.write(info + 0x24 + i["offset"], i["name"].encode("latin-1") + b"\0")
    w32(e, info + 0x20, info + 0x24 + i["offset"])


def ppc_info_read(e, info):
    name_ptr = e.u32(info + 0x20)
    return dict(words=[T.s32(e.u32(info)), T.s32(e.u32(info + 4)), T.s32(name_ptr - (info + 0x24))],
                f08=e.read(info + 8, 0x18), name=e.cstr(name_ptr, limit=0x104))


def ppc_begin(e, c):
    s = e.rs = RsState()
    e.heap_in_scratch = True
    fixed = {A["gTPFileInfo"]: LB["fileinfo"]}
    e.cl = CL.PpcLog(fixed, c.get("q", {}))
    L = e.cl
    for name, off in GLOBALS.items():
        FS.s_write(e, A["gTPGlobals"] + off, c["globals"][name], False)
    FS.s_write(e, A["gTPFileInfo"] + 0x14, c["remote"], False)
    # storages
    s.storages = []
    for k, wad in enumerate(c["storages"]):
        st = e.alloc(8)
        w32(e, st, WAD_VT if wad else DISC_VT)
        L.labels[st] = STORAGE0 + k
        s.storages.append(st)
    pick = lambda i: 0 if i is None else s.storages[i]   # noqa: E731
    # the manager (the resource's, or the one under test)
    s.manager = e.alloc(0xC)
    L.labels[s.manager] = LB["manager"]
    # the resource
    res = c["res"]
    s.res = e.alloc(0x50)
    L.labels[s.res] = LB["resource"]
    FS.s_write(e, s.res, res["name"], False)
    FS.s_write(e, s.res + 0x14, res["path"], False)
    w32(e, s.res + 0x28, VT_RES)
    s.list = 0
    if res["list"] is not None:
        s.list, _ = ppc_list(e, SPTR_VT, [0 if i == "null" else s.storages[i] for i in res["list"]])
        L.labels[s.list] = LB["list"]
    w32(e, s.res + 0x2C, s.list)
    w32(e, s.res + 0x30, s.manager if res["manager"] else 0)
    w32(e, s.res + 0x34, pick(res["cur"]))
    w32(e, s.res + 0x38, pick(res["last"]))
    s.pattern = 0
    if res["pattern"] is not None:
        s.pattern = FS.new_string(e, res["pattern"], False, LB["pattern"])
    w32(e, s.res + 0x3C, s.pattern)
    for off, label in c.get("members", {}).items():
        L.labels[s.res + off] = label
    # the manager's resources
    mg = c.get("mgr")
    s.resources = []
    s.rlist = 0
    if mg is not None:
        for k, name in enumerate(mg["names"]):
            r = e.alloc(0x40)
            FS.s_write(e, r, name, False)
            w32(e, r + 0x28, VT_RES)
            L.labels[r] = RESOURCE0 + k
            s.resources.append(r)
        s.other = e.alloc(0x40)
        FS.s_write(e, s.other, mg["other_name"], False)
        w32(e, s.other + 0x28, VT_RES)
        L.labels[s.other] = LB["other"]
        if mg["list"] is not None:
            s.rlist, _ = ppc_list(e, RPTR_VT, [0 if i == "null" else s.resources[i] for i in mg["list"]])
            L.labels[s.rlist] = LB["rlist"]
        rpick = lambda i: 0 if i is None else s.other if i == "other" else s.resources[i]   # noqa: E731
        w32(e, s.manager, s.rlist)
        w32(e, s.manager + 4, rpick(mg["cur"]))
        w32(e, s.manager + 8, rpick(mg["shared"]))
        s.rpick = rpick
    # the file, the entry, the strings passed
    s.file = e.alloc(8)
    L.labels[s.file] = LB["file"]
    s.info = e.alloc(0x128)
    L.labels[s.info] = LB["info"]
    ppc_info_write(e, s.info, c)
    s.strings = [FS.new_string(e, text, False, LB[k]) for k, text in zip(("s1", "s2", "s3"), c["strings"])]
    s.out = e.alloc(4)
    L.labels[s.out] = LB["out"]


def ppc_observe(e, c):
    s, L = e.rs, e.cl
    out = {"strings": [FS.s_read(e, a, False) for a in s.strings]}
    if not c.get("freed"):
        r = s.res
        out["res"] = dict(name=FS.s_read(e, r, False), path=FS.s_read(e, r + 0x14, False),
                          fields=[L.lab(e.u32(r + o)) for o in (0x2C, 0x30, 0x34, 0x38, 0x3C)],
                          pattern=FS.s_read(e, e.u32(r + 0x3C), False) if e.u32(r + 0x3C) else None)
        lst = e.u32(r + 0x2C)
        out["list"] = ppc_walk(e, lst) if lst and not c.get("list_freed") else None
    out["info"] = ppc_info_read(e, s.info)
    out["manager"] = [L.lab(e.u32(s.manager + o)) for o in (0, 4, 8)] if not c.get("manager_freed") else None
    if c.get("mgr") is not None and not c.get("manager_freed"):
        rl = e.u32(s.manager)
        out["rlist"] = ppc_walk(e, rl) if rl else None
        out["res_managers"] = [L.lab(e.u32(r + 0x30)) for r in s.resources + [s.other]]
    out["out"] = L.lab(e.u32(s.out))
    return out

# ======================================================================= host side


class HostState:
    pass


def host_string(lib, text):
    return F(lib, "tpw_ps_new_string", VP, C.c_char_p)(text.encode("latin-1"))


def host_set_string(lib, obj, text):
    F(lib, "tpw_ps_set_string", None, VP, C.c_char_p)(obj, text.encode("latin-1"))


def host_text(lib, obj):
    buf = C.create_string_buffer(1024)
    F(lib, "tpw_rs_text", I32, VP, C.c_char_p, I32)(obj, buf, 1024)
    return buf.value.decode("latin-1")


def host_walk(lib, fn, lst):
    fwd, bwd = (I32 * 40)(), (I32 * 40)()
    count = F(lib, fn, I32, VP, VP, VP, I32)(lst, fwd, bwd, 32)
    take = lambda arr: [v for v in arr[:arr[:].index(-999)]]   # noqa: E731
    return dict(count=count, fwd=take(fwd), bwd=take(bwd))


def host_begin(lib, c):
    h = HostState()
    lab = lambda p: F(lib, "tpw_cl_lab", I32, VP)(p)   # noqa: E731
    h.lab = lab
    addr = lambda n: F(lib, "tpw_ps_addr", VP, C.c_char_p)(n.encode())   # noqa: E731
    h.fixed = {LB["fileinfo"]: addr("gTPFileInfo")}
    gstr = F(lib, "tpw_rs_tpglobals_string", VP, C.c_char_p)
    for name in GLOBALS:
        host_set_string(lib, gstr(name.encode()), c["globals"][name])
    host_set_string(lib, gstr(b"mRemotePath"), c["remote"])
    F(lib, "tpw_rs_set_fake_open", None, I32)(1 if c.get("mgr") is not None else 0)
    h.storages = []
    for k, wad in enumerate(c["storages"]):
        st = F(lib, "tpw_rs_new_storage", VP, I32)(1 if wad else 0)
        h.fixed[STORAGE0 + k] = st
        h.storages.append(st)
    pick = lambda i: None if i is None else h.storages[i]   # noqa: E731
    h.manager = F(lib, "tpw_rs_new_manager", VP)()
    h.fixed[LB["manager"]] = h.manager
    res = c["res"]
    if c.get("class"):
        h.res = F(lib, "tpw_rs_construct", VP, C.c_char_p, C.c_char_p, C.c_char_p)(
            c["class"].encode(), res["name"].encode("latin-1"), res["path"].encode("latin-1"))
        host_set_string(lib, F(lib, "tpw_rs_path", VP, VP)(h.res), res["path"])
    else:
        h.res = F(lib, "tpw_rs_new_resource", VP, C.c_char_p, C.c_char_p)(res["name"].encode("latin-1"),
                                                                          res["path"].encode("latin-1"))
    h.fixed[LB["resource"]] = h.res
    h.list = None
    if res["list"] is not None:
        h.list = F(lib, "tpw_rs_new_storage_list", VP)()
        for i in res["list"]:
            F(lib, "tpw_rs_storage_list_append", None, VP, VP)(h.list, None if i == "null" else h.storages[i])
        h.fixed[LB["list"]] = h.list
    h.pattern = None
    if res["pattern"] is not None:
        h.pattern = host_string(lib, res["pattern"])
        h.fixed[LB["pattern"]] = h.pattern
    fields = (VP * 5)(h.list, h.manager if res["manager"] else None, pick(res["cur"]), pick(res["last"]), h.pattern)
    F(lib, "tpw_rs_set_fields", None, VP, VP)(h.res, fields)
    for off, label in c.get("members", {}).items():
        h.fixed[label] = F(lib, "tpw_rs_member", VP, VP, I32)(h.res, off)
    mg = c.get("mgr")
    h.resources, h.rlist = [], None
    if mg is not None:
        new_res = F(lib, "tpw_rs_new_resource", VP, C.c_char_p, C.c_char_p)
        for k, name in enumerate(mg["names"]):
            r = new_res(name.encode("latin-1"), b"")
            F(lib, "tpw_rs_set_fields", None, VP, VP)(r, (VP * 5)())
            h.fixed[RESOURCE0 + k] = r
            h.resources.append(r)
        h.other = new_res(mg["other_name"].encode("latin-1"), b"")
        F(lib, "tpw_rs_set_fields", None, VP, VP)(h.other, (VP * 5)())
        h.fixed[LB["other"]] = h.other
        if mg["list"] is not None:
            h.rlist = F(lib, "tpw_rs_new_resource_list", VP)()
            for i in mg["list"]:
                F(lib, "tpw_rs_resource_list_append", None, VP, VP)(h.rlist,
                                                                    None if i == "null" else h.resources[i])
            h.fixed[LB["rlist"]] = h.rlist
        rpick = lambda i: None if i is None else h.other if i == "other" else h.resources[i]   # noqa: E731
        F(lib, "tpw_rs_set_manager", None, VP, VP, VP, VP)(h.manager, h.rlist, rpick(mg["cur"]), rpick(mg["shared"]))
        h.rpick = rpick
    h.file = F(lib, "tpw_rs_new_file", VP)()
    h.fixed[LB["file"]] = h.file
    h.info = F(lib, "tpw_rs_new_info", VP)()
    h.fixed[LB["info"]] = h.info
    i = c["info"]
    F(lib, "tpw_rs_set_info", None, VP, C.c_uint32, C.c_uint32, C.c_char_p, C.c_char_p, I32)(
        h.info, i["f00"] & 0xFFFFFFFF, i["attr"] & 0xFFFFFFFF, bytes(i["f08"]), i["name"].encode("latin-1"), i["offset"])
    h.strings = [host_string(lib, text) for text in c["strings"]]
    for k, p in zip(("s1", "s2", "s3"), h.strings):
        h.fixed[LB[k]] = p
    h.out = F(lib, "tpw_rs_alloc", VP, I32)(8)
    h.fixed[LB["out"]] = h.out
    return h


def host_observe(lib, h, c):
    lab = h.lab
    out = {"strings": [host_text(lib, p) for p in h.strings]}
    if not c.get("freed"):
        v = (VP * 5)()
        F(lib, "tpw_rs_get_fields", None, VP, VP)(h.res, v)
        out["res"] = dict(name=host_text(lib, F(lib, "tpw_rs_name", VP, VP)(h.res)),
                          path=host_text(lib, F(lib, "tpw_rs_path", VP, VP)(h.res)),
                          fields=[lab(v[k]) for k in range(5)],
                          pattern=host_text(lib, v[4]) if v[4] else None)
        out["list"] = host_walk(lib, "tpw_rs_storage_list_walk", v[0]) if v[0] and not c.get("list_freed") else None
    words, f08, name = (I32 * 3)(), (C.c_uint8 * 0x18)(), C.create_string_buffer(0x110)
    F(lib, "tpw_rs_get_info", None, VP, VP, VP, C.c_char_p, I32)(h.info, words, f08, name, 0x110)
    out["info"] = dict(words=list(words), f08=bytes(f08), name=name.value.decode("latin-1"))
    if not c.get("manager_freed"):
        m = (VP * 3)()
        F(lib, "tpw_rs_get_manager", None, VP, VP)(h.manager, m)
        out["manager"] = [lab(m[k]) for k in range(3)]
    else:
        out["manager"] = None
    if c.get("mgr") is not None and not c.get("manager_freed"):
        out["rlist"] = host_walk(lib, "tpw_rs_resource_list_walk", m[0]) if m[0] else None
        mans = []
        for r in h.resources + [h.other]:
            v = (VP * 5)()
            F(lib, "tpw_rs_get_fields", None, VP, VP)(r, v)
            mans.append(lab(v[1]))
        out["res_managers"] = mans
    out["out"] = lab(C.c_void_p.from_address(h.out).value)
    return out

# ======================================================================= running a case


STUBS = None
VOID = {R + "~CTPBaseResource", R + "AddDiscStorage", R + "AddWADStorage", R + "AddAllStorages", R + "AddLocalStorage",
        R + "AddRemoteStorage", "MakeFullFilename", M + "pSetCurrentResource", M + "CTPResourceManager",
        M + "~CTPResourceManager",
        "CTPIntermediate::~CTPIntermediate", "CWadFileStorage::~CWadFileStorage"}
LABEL_RESULT = {R + "pGetCurrentSearchStorage", R + "pGetLastFoundFileStorage", "strRemoveRootDirectory",
                M + "pGetCurrentResource"}


def setup(runner):
    global STUBS, VT_RES
    T.install_std_stubs(runner.emu)
    if "c" not in FS.VT:
        FS.make_vtables(runner.emu)
    if VT_RES is None:
        VT_RES = make_resource_vtable(runner.emu)
    STUBS = build_stubs(runner.emu)


class RsTest(FS.FsTest):
    pass


def fn_test(name, args, edge, rnd):
    """args(c, side) -> the argument values (`this` included); side is ("ppc", e) or
    ("host", lib, h): objects are labels' addresses, other values ints."""
    t = RsTest(name, None, None, edge=edge, rnd=rnd)
    void = name in VOID or name.startswith("ctor:") or name.startswith("dtor:")

    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            ppc_begin(e, c)
            a = args(c, ("ppc", e))
            r3 = T.call_ppc(e, F_[name], [T.u32(v) for v in a])[0]
            res = None if void else e.cl.lab(r3) if name in LABEL_RESULT else T.s32(r3)
            out = ppc_observe(e, c)
            if name.startswith("ctor:"):
                out["made"] = ppc_made(e, c)
            out["log"] = e.cl.log
            out["result"] = res
        finally:
            CL.Stubs.deactivate(e, saved)
            e.cl = None
        t._p = out
        return out

    def host(lib, c):
        h = host_begin(lib, c)
        a = args(c, ("host", lib, h))
        CL.host_begin(lib, h.fixed, c.get("q", {}))
        vals = (I64 * 8)(*[int(v or 0) for v in a], *([0] * (8 - len(a))))
        res = F(lib, "tpw_rs_call", I64, C.c_char_p, VP)(name.encode(), vals)
        log = CL.host_end(lib)
        out = host_observe(lib, h, c)
        if name.startswith("ctor:"):
            out["made"] = host_made(lib, h, c)
        out["log"] = log
        out["result"] = None if void else int(res)
        t._last = (t._p, out)
        return out

    t.ppc, t.host = ppc, host
    return t


def ppc_made(e, c):
    """What a constructor made of the block (the case's `made` object)."""
    s, L = e.rs, e.cl
    obj = s.made
    lst = e.u32(obj + 0x2C)
    return dict(cls=VT_CLASS.get(e.u32(obj + 0x28), hex(e.u32(obj + 0x28))), name=FS.s_read(e, obj, False),
                path=FS.s_read(e, obj + 0x14, False), ptrlist=e.u32(lst) == SPTR_VT if lst else None,
                list=ppc_walk(e, lst) if lst else None,
                fields=[L.lab(e.u32(obj + o)) for o in (0x30, 0x34, 0x38, 0x3C)])


def host_made(lib, h, c):
    obj = h.made
    buf = C.create_string_buffer(64)
    F(lib, "tpw_rs_class", I32, VP, C.c_char_p, I32)(obj, buf, 64)
    v = (VP * 5)()
    F(lib, "tpw_rs_get_fields", None, VP, VP)(obj, v)
    return dict(cls=buf.value.decode(), name=host_text(lib, F(lib, "tpw_rs_name", VP, VP)(obj)),
                path=host_text(lib, F(lib, "tpw_rs_path", VP, VP)(obj)), ptrlist=True if v[0] else None,
                list=host_walk(lib, "tpw_rs_storage_list_walk", v[0]) if v[0] else None,
                fields=[h.lab(v[k]) for k in range(1, 5)])

# ======================================================================= case generators


NAMES = ["Data", "data", "Sound", "ui", "", "Advisor", "fmv", "DATA", "sound"]
TEXTS = ["", ".", "./", ".:", ".:data", ":.:sounds:a.wav", "data:file.sam", "Data:Sub:file", "file", "::x",
         "..:a", ".wad*", "data.wad*", "data.wad*:", "x:.wad*", ".:.:data:x.sam", "DATA:FILE", "data:", ":"]


def rnd_text(r):
    if r.random() < 0.6:
        return r.choice(TEXTS)
    return "".join(r.choice(".:aDb*w") for _ in range(r.randint(0, 9)))


def rnd_globals(r):
    g = dict(mSeparator=":", mSearchPath="", mCurrentFolder=".", mAllFiles="*", mSamExtension=".sam",
             mOnlineSamExtension=".sam", mOnlinePrefix="Online_", mWadExtension=".wad", mWadPattern=".wad*",
             mWadFolderPattern=".wad*:")
    if r.random() < 0.15:
        g["mWadFolderPattern"] = r.choice([".wad*", "wad*", ".WAD*", ""])
    if r.random() < 0.1:
        g["mSearchPath"] = r.choice(["?", "search"])
    # (never empty: strRemoveRootDirectory would never stop skipping empty matches)
    if r.random() < 0.08:
        g["mSeparator"] = "::"
    if r.random() < 0.08:
        g["mCurrentFolder"] = r.choice(["..", "D"])
    return g


def rnd_info(r):
    return dict(f00=r.getrandbits(32), attr=r.getrandbits(32), f08=[r.getrandbits(8) for _ in range(0x18)],
                name=r.choice(["", "x", "old.sam", "entry"]), offset=r.choice([0, 0, 1, 5]))


def base(r, n_storages=None, **extra):
    n = r.randint(0, 4) if n_storages is None else n_storages
    storages = [r.random() < 0.4 for _ in range(n)]
    order = list(range(n))
    r.shuffle(order)
    in_list = order[:r.randint(0, n)] if r.random() < 0.3 else order
    has_list = r.random() < 0.9
    res = dict(name=r.choice(NAMES), path=r.choice(["Data:", "data", "", "Sound:", ".:data:", "data.wad*"]),
               list=in_list if has_list else None, manager=True,
               cur=None, last=r.choice([None] + list(range(n))) if n else None,
               pattern=r.choice([None, None, "*.sam", "old*"]))
    if has_list and in_list and r.random() < 0.7:
        res["cur"] = r.choice(in_list)
    c = dict(globals=rnd_globals(r), remote=r.choice(["HD:Remote", "CD:", "", "CD:TPW:"]), storages=storages,
             res=res, info=rnd_info(r), strings=[rnd_text(r), rnd_text(r), rnd_text(r)], q={})
    c.update(extra)
    return c


def storage_queues(r, c, labels, keys=("FindFirst", "FindNext", "OpenFile")):
    q = c["q"]
    for lab in labels:
        for k in keys:
            q[f"CFileStorage::{k}:{lab}"] = [r.choice([-1, -1, 0, 1, -5]) for _ in range(3)]


def all_labels(c):
    return [STORAGE0 + k for k in range(len(c["storages"]))] + list(range(100, 106))


def gen(make, n=60, seed=6):
    def edge():
        rr = random.Random(seed)
        return [make(rr) for _ in range(n)]
    return edge, make


TESTS = []


def add(name, make, args, n=60):
    edge, rnd = gen(make, n)
    TESTS.append(fn_test(name, args, edge, rnd))


def obj(side, key):
    """The address of a case object on either side."""
    if side[0] == "ppc":
        s = side[1].rs
        return {"res": s.res, "manager": s.manager, "file": s.file, "info": s.info, "out": s.out,
                "s1": s.strings[0], "s2": s.strings[1], "s3": s.strings[2]}[key]
    h = side[2]
    return {"res": h.res, "manager": h.manager, "file": h.file, "info": h.info, "out": h.out,
            "s1": h.strings[0], "s2": h.strings[1], "s3": h.strings[2]}[key]


def storage(side, i):
    if i is None:
        return 0
    return side[1].rs.storages[i] if side[0] == "ppc" else side[2].storages[i]


def resource(side, i):
    return side[1].rs.rpick(i) if side[0] == "ppc" else side[2].rpick(i)


# ---- the base resource
def with_null(r, values):
    """Sometimes a NULL value in a list (the destructors skip deleting it)."""
    if values is not None and r.random() < 0.3:
        values.insert(r.randint(0, len(values)), "null")


def g_dtor(r):
    c = base(r)
    with_null(r, c["res"]["list"])
    c["flag"] = r.choice([0, 1, 1, -1])
    c["freed"] = c["flag"] > 0
    c["list_freed"] = True
    return c


add(R + "~CTPBaseResource", g_dtor, lambda c, side: [obj(side, "res"), c["flag"]])


def g_add(kind):
    def make(r):
        c = base(r)
        c["q"]["CDiscFileStorage::Open"] = [r.choice([0, -1, 2, -3]) for _ in range(3)]
        c["q"]["CWadFileStorage::Open"] = [r.choice([0, -1, 2, -3]) for _ in range(3)]
        if c["res"]["list"] is None:
            c["res"]["list"] = []
        return c
    return make


for nm in ("AddDiscStorage", "AddWADStorage", "AddAllStorages", "AddLocalStorage", "AddRemoteStorage"):
    add(R + nm, g_add(nm), lambda c, side: [obj(side, "res"), obj(side, "s1")])

add(R + "pGetCurrentSearchStorage", lambda r: base(r), lambda c, side: [obj(side, "res")], n=20)
add(R + "pGetLastFoundFileStorage", lambda r: base(r), lambda c, side: [obj(side, "res")], n=20)
add("strRemoveRootDirectory", lambda r: base(r), lambda c, side: [obj(side, "s1")], n=120)
add("QualifyFilename", lambda r: base(r), lambda c, side: [obj(side, "s1"), obj(side, "s2"), obj(side, "s3")], n=200)


def g_open(r):
    c = base(r)
    storage_queues(r, c, all_labels(c))
    c["res"]["manager"] = True
    if r.random() < 0.5:   # names inside the folder
        c["strings"][1] = c["res"]["path"] + r.choice(["file.sam", ":x", "", "sub:y"])
    c["storage_arg"] = r.randrange(len(c["storages"])) if c["storages"] else None
    c["mode"] = r.choice([0x80000001, 0, 7])
    return c


add(R + "OpenFile(storage)", g_open,
    lambda c, side: [obj(side, "res"), obj(side, "file"), obj(side, "s2"), storage(side, c["storage_arg"]), c["mode"]])
add(R + "OpenFile", g_open, lambda c, side: [obj(side, "res"), obj(side, "file"), obj(side, "s2"), c["mode"]])


def g_find(r):
    c = base(r)
    storage_queues(r, c, all_labels(c))
    return c


add(R + "FindFirst", g_find, lambda c, side: [obj(side, "res"), obj(side, "s1"), obj(side, "info")], n=120)


def g_next(r):
    c = base(r)
    storage_queues(r, c, all_labels(c))
    res = c["res"]
    # without a pattern only when no storage follows the current one (FindNext would use it)
    if res["cur"] is not None and res["pattern"] is None and (res["list"][-1] != res["cur"] or r.random() < 0.5):
        res["pattern"] = r.choice(["*.sam", "a*"])
    return c


add(R + "FindNext", g_next, lambda c, side: [obj(side, "res"), obj(side, "info")], n=120)


def g_sam(r):
    c = base(r, n_storages=r.randint(1, 3))
    storage_queues(r, c, all_labels(c))
    c["class"] = "CTPAdvisorResource"
    c["storage_arg"] = r.randrange(len(c["storages"]))
    return c


for nm in ("CTPIntermediate::GetSamFile", "CTPIntermediate::GetOnlineSamFile"):
    add(nm, g_sam, lambda c, side: [obj(side, "res"), storage(side, c["storage_arg"]), obj(side, "file")], n=30)
add("MakeFullFilename", lambda r: base(r), lambda c, side: [obj(side, "s1"), obj(side, "s2")], n=20)
add(R + "OpenQualifiedFile(storage)", g_sam,
    lambda c, side: [obj(side, "res"), obj(side, "file"), obj(side, "s2"), storage(side, c["storage_arg"])], n=30)
add(R + "OpenQualifiedFile", g_find, lambda c, side: [obj(side, "res"), obj(side, "file"), obj(side, "s2")], n=120)


def g_sound(r):
    c = g_find(r)
    c["class"] = "CTPSoundResource"
    return c


add("CTPSoundResource::FindSoundFile", g_sound, lambda c, side: [obj(side, "res"), obj(side, "s1"), obj(side, "s2")],
    n=120)


# ---- the subclasses
def g_ctor(r):
    c = base(r)
    c["strings"][0] = r.choice(NAMES)
    c["strings"][1] = r.choice(["Data", "Data:", "", "data:sub", ":"])
    return c


def ctor_args(c, side):
    if side[0] == "ppc":
        e = side[1]
        block = e.alloc(0x50)
        e.cl.labels[block] = 50
        e.rs.made = block
    else:
        lib, h = side[1], side[2]
        block = F(lib, "tpw_rs_alloc", VP, I32)(512)
        h.fixed[50] = block
        h.made = block
    return [block, obj(side, "s1"), obj(side, "s2")]


def g_class_dtor(cls):
    def make(r):
        c = base(r)
        with_null(r, c["res"]["list"])
        c["class"] = cls
        c["flag"] = r.choice([0, 1, 1, -1])
        c["freed"] = c["flag"] > 0
        c["list_freed"] = True
        c["members"] = {off: MEMBER0 + k for k, off in enumerate(CLASSES[cls][1])}
        return c
    return make


def dtor_args(c, side):
    if side[0] == "ppc":
        e = side[1]
        w32(e, e.rs.res + 0x28, CLASSES[c["class"]][0])
    return [obj(side, "res"), c["flag"]]


for cls in CLASSES:
    add("ctor:" + cls, g_ctor, ctor_args, n=20)
    add("dtor:" + cls, g_class_dtor(cls), dtor_args, n=30)


def g_intermediate(r):
    c = g_class_dtor("CTPAdvisorResource")(r)
    c["class"] = "TestIntermediate"   # (the host's: see tests/unit_TPSettings.cpp)
    c["members"] = {0x40: MEMBER0}
    return c


add("CTPIntermediate::~CTPIntermediate", g_intermediate, lambda c, side: [obj(side, "res"), c["flag"]], n=30)


def g_wad_dtor(r):
    c = base(r, n_storages=1)
    c["storages"] = [True]
    c["res"]["list"] = []
    c["res"]["cur"] = c["res"]["last"] = None
    c["flag"] = r.choice([0, 1, -1])
    return c


add("CWadFileStorage::~CWadFileStorage", g_wad_dtor, lambda c, side: [storage(side, 0), c["flag"]], n=10)


# ---- the manager
def g_mgr(r, **extra):
    c = base(r, n_storages=0)
    k = r.randint(0, 4)
    names = [r.choice(NAMES) for _ in range(k)]
    order = list(range(k))
    r.shuffle(order)
    has = r.random() < 0.85
    pool = [None] + list(range(k)) + ["other"]
    c["mgr"] = dict(names=names, other_name=r.choice(NAMES), list=order if has else None,
                    cur=r.choice(pool), shared=r.choice(pool))
    for lab in [RESOURCE0 + i for i in range(k)] + [LB["other"]]:
        c["q"][f"CTPBaseResource::OpenFile:{lab}"] = [r.choice([0, 1, 2, -1])]
    c.update(extra)
    return c


def g_mgr_dtor(r):
    c = g_mgr(r)
    with_null(r, c["mgr"]["list"])
    c["flag"] = r.choice([0, 1, -1])
    c["manager_freed"] = c["flag"] > 0
    return c


add(M + "CTPResourceManager", lambda r: g_mgr(r), lambda c, side: [obj(side, "manager")], n=10)
add(M + "~CTPResourceManager", g_mgr_dtor, lambda c, side: [obj(side, "manager"), c["flag"]], n=40)


def g_reg(r):
    c = g_mgr(r)
    k = len(c["mgr"]["names"])
    c["arg"] = r.choice([None] + list(range(k)) + ["other", "other"]) if r.random() < 0.95 else None
    if c["arg"] is None:
        c["arg"] = "other"
    return c


for nm in ("RegisterResource", "RegisterResource(shared)", "pSetCurrentResource"):
    add(M + nm, g_reg, lambda c, side: [obj(side, "manager"), resource(side, c["arg"])], n=60)


def g_unreg(r):
    c = g_reg(r)
    if c["mgr"]["list"] is not None and r.random() < 0.2:   # removing a NULL value deletes nothing
        with_null(r, c["mgr"]["list"])
        if "null" in c["mgr"]["list"]:
            c["arg"] = None
    return c


add(M + "UnregisterResource", g_unreg, lambda c, side: [obj(side, "manager"), resource(side, c["arg"])], n=80)
add(M + "OpenFile", lambda r: g_mgr(r), lambda c, side: [obj(side, "manager"), obj(side, "file"), obj(side, "s2")])


def g_findres(r):
    c = g_mgr(r)
    names = c["mgr"]["names"]
    c["strings"][1] = r.choice(names + [n.upper() for n in names] + ["nothing", ""]) if names else r.choice(NAMES)
    return c


add(M + "FindResource", g_findres, lambda c, side: [obj(side, "manager"), obj(side, "s2"), obj(side, "out")], n=80)
add(M + "pGetCurrentResource", lambda r: g_mgr(r), lambda c, side: [obj(side, "manager")], n=20)


# ---- the list instances
def list_test(name, op, storages):
    """A list of 0-5 values (storages 10.. or resources 20..) under test on both sides."""
    vt = SPTR_VT if storages else RPTR_VT
    first = STORAGE0 if storages else RESOURCE0

    def make(r):
        c = base(r, n_storages=0)
        n = r.randint(0, 5)
        if op == "RemoveAt":
            n = max(n, 1)
        c["values"] = [r.randrange(6) for _ in range(n)]
        c["arg"] = r.randrange(7)          # a value (6: one not in the pool) or a node index
        if op == "RemoveAt":
            c["arg"] = r.randrange(n)
        c["flag"] = r.choice([0, 1, -1])
        c["list_freed"] = op in ("~TbTList", "~TbPtrList") and c["flag"] > 0
        return c

    def ppc(e, c):
        saved = STUBS.activate(e)
        try:
            ppc_begin(e, c)
            L = e.cl
            pool = []
            for k in range(7):
                v = e.alloc(8)
                L.labels[v] = first + k
                pool.append(v)
            lst, nodes = ppc_list(e, vt, [pool[i] for i in c["values"]])
            L.labels[lst] = LB["list"]
            if op == "TbTList":
                e.write(lst, b"\0" * 0x10)
                a = [lst]
            elif op in ("~TbTList", "~TbPtrList"):
                a = [lst, c["flag"]]
            elif op in ("Find", "AddTail", "AddHead"):
                a = [lst, pool[c["arg"]]]
            elif op == "RemoveAt":
                cell = e.alloc(4)
                w32(e, cell, nodes[c["arg"]])
                a = [lst, cell]
            else:
                a = [lst]
            r3 = T.call_ppc(e, F_[name], [T.u32(v) for v in a])[0]
            if op == "Find":
                res = nodes.index(r3) if r3 in nodes else (-1 if r3 == 0 else -2)
            elif op in ("RemoveHead", "RemoveAt"):
                res = L.lab(r3)
            else:
                res = None
            out = dict(log=e.cl.log, result=res,
                       list=None if c["list_freed"] else ppc_walk(e, lst))
            if op == "TbTList":
                out["vt"] = e.u32(lst) == (SLIST_VT if storages else RLIST_VT)
        finally:
            CL.Stubs.deactivate(e, saved)
            e.cl = None
        return out

    def host(lib, c):
        fixed = {}
        pool = []
        for k in range(7):
            v = F(lib, "tpw_rs_alloc", VP, I32)(16)
            fixed[first + k] = v
            pool.append(v)
        if storages:
            lst = F(lib, "tpw_rs_new_storage_list", VP)()
            for i in c["values"]:
                F(lib, "tpw_rs_storage_list_append", None, VP, VP)(lst, pool[i])
        else:
            lst = F(lib, "tpw_rs_new_resource_list", VP)()
            for i in c["values"]:
                F(lib, "tpw_rs_resource_list_append", None, VP, VP)(lst, pool[i])
        fixed[LB["list"]] = lst
        if op == "TbTList":
            a = [lst]
        elif op in ("~TbTList", "~TbPtrList"):
            a = [lst, c["flag"]]
        elif op in ("Find", "AddTail", "AddHead"):
            a = [lst, pool[c["arg"]]]
        elif op == "RemoveAt":
            a = [lst, c["arg"]]
        else:
            a = [lst]
        CL.host_begin(lib, fixed, c.get("q", {}))
        vals = (I64 * 8)(*[int(v or 0) for v in a], *([0] * (8 - len(a))))
        res = F(lib, "tpw_rs_list_call", I64, C.c_char_p, VP)(name.encode(), vals)
        log = CL.host_end(lib)
        walk = "tpw_rs_storage_list_walk" if storages else "tpw_rs_resource_list_walk"
        out = dict(log=log, result=int(res) if op in ("Find", "RemoveHead", "RemoveAt") else None,
                   list=None if c["list_freed"] else host_walk(lib, walk, lst))
        if op == "TbTList":
            out["vt"] = True
        return out

    edge, rnd = gen(make, 60)
    return FS.FsTest(name, ppc, host, edge=edge, rnd=rnd)


for nm in ("TbTList", "~TbTList", "Find", "RemoveAll", "RemoveHead", "AddTail"):
    TESTS.append(list_test(SL + nm, nm, True))
for nm in ("TbTList", "~TbTList", "Find", "RemoveAll", "RemoveAt", "RemoveHead", "AddHead"):
    TESTS.append(list_test(RL + nm, nm, False))
TESTS.append(list_test("TbPtrList<CFileStorage>::~TbPtrList", "~TbPtrList", True))
TESTS.append(list_test("TbPtrList<CTPBaseResource>::~TbPtrList", "~TbPtrList", False))
