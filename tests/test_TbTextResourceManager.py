"""Manager tests with independent index fakes, plus queued file operations."""
import ctypes as C
import random
import struct
import tpwtest as T
import calllog as CL
import ltms_support as L
from tpwtest import FnTest

PROGRAM = "ltms symbols shared"
SHIM_LIBRARY = "unit_TbTextResourceManager"
VP, I32, U32 = C.c_void_p, C.c_int32, C.c_uint32
LABELS = ["Open__21TbTextResourceManagerFPCc", "SetLanguage__21TbTextResourceManagerFPCc",
          "LoadResourceIndex__21TbTextResourceManagerFPCc",
          "GetResource__21TbTextResourceManagerFPP14TbIMBToUnicodeUl",
          "GetResource__21TbTextResourceManagerFPP14TbIUnicodeToMBUl",
          "GetResource__21TbTextResourceManagerFPP8TbITableUl",
          "GetResource__21TbTextResourceManagerFPP7TbIFontUl",
          "__ct__21TbTextResourceManagerFv", "Close__21TbTextResourceManagerFv",
          "__dt__21TbTextResourceManagerFv"]


def F(lib, name, restype, *args):
    f = lib[name]
    f.restype, f.argtypes = restype, args
    return f


def setup(runner):
    e = runner.emu
    T.install_std_stubs(e)
    e.lt_s = L.files(e)
    e.lt_resource = L.resource(e)
    e.lt_s.real.update(e.func(x) for x in LABELS)

    def index(e):
        e.cl.rec("Storage", (e.cl.newlab(e.r(3) + 16),))
        e.cl.rec("Index", (e.cl.newlab(e.r(3)),))
        e.lt_new_index = e.r(3)

    def initialise(e):
        e.cl.rec("Initialise", (e.cl.lab(e.r(4)), e.r(7)), (e.cstr(e.r(5)),))
        L.word(e, e.r(3), e.cl.ret("Initialise"))

    def find(e):
        e.cl.rec("Find", (e.cl.lab(e.r(3)), e.r(4)))
        e.set_r(3, e.lt_item if e.cl.ret("Handle", 1) else 0)

    def type(e):
        e.cl.rec("Type", (e.cl.lab(e.r(3)), int(e.r(4) != 0)))
        e.set_r(3, e.cl.ret("Type", 6) & 65535)

    def get(e):
        e.cl.rec("Get", (e.cl.lab(e.r(4)), int(e.r(5) != 0)))
        L.word(e, e.r(6), e.lt_resource if e.cl.ret("Output", 1) else 0)
        L.word(e, e.r(3), e.cl.ret("Get"))

    for label, fn in (("__ct__16TbIResourceIndexFv", index),
                      ("Initialise__16TbIResourceIndexFPCcPvUl", initialise),
                      ("GetResourceItemHandle__16TbIResourceIndexFUl", find),
                      ("GetResourceType__16TbIResourceIndexFPv", type),
                      ("GetResource__16TbIResourceIndexFPvPPv", get)):
        e.lt_s.internal(e.func(label), fn)


def case(r):
    count = r.choice([0, 1, 4, 7])
    payload = r.randbytes(max(12 * count, 8))
    bytes = struct.pack("<II", r.choice([0x49524642, 0x49524642, 0x49524642, 0]), count) + payload
    return dict(opened=r.choice([0, 0, 1, -1, -2147483648]), old=r.randrange(2), id=r.getrandbits(32),
                bytes=bytes, queues={
                    "StorageOpen": [r.choice([0, 1, -1]), r.choice([0, -1])],
                    "OpenFile": [r.choice([0, 3, -1])], "FullPath": [r.choice([0, 4, -1])],
                    "Implementation": [r.choice([0, 1, 1, 1])],
                    "Read": [r.choice([0, 7, 8, 8, 8]), r.choice([len(payload), len(payload), 0])],
                    "Initialise": [r.choice([0, 2, -1])], "Handle": [r.randrange(2)],
                    "Type": [r.choice([0, 1, 3, 4, 6, 65535])], "Get": [r.choice([0, 17, -20])],
                    "Output": [r.randrange(2)]})


def ppc(e, c, op):
    e.lt_bytes, e.lt_pos, e.lt_new_index = c["bytes"], 0, 0
    obj = e.alloc(16, struct.pack(">4I", e.lt_resource if c["old"] else 0, e.toc + 0xdf8, 0, T.u32(c["opened"])))
    e.lt_item = e.alloc(12)
    saved = L.install(e, e.lt_s, {1: obj, 2: obj + 4, 3: e.lt_resource}, c["queues"])
    out = e.alloc(4, struct.pack(">I", 7))
    result = e.alloc(4)
    path = CL.new_text(e, "language" if op == 1 else "folder:")
    try:
        if op in (7, 9):
            T.call_ppc(e, e.func(LABELS[op]), [obj, T.u32((c["id"] & 3) - 1) if op == 9 else 0xffff])
            value = 0
        else:
            args = [result, obj]
            if op < 3:
                args += [path]
            elif op < 7:
                args += [out, c["id"]]
            T.call_ppc(e, e.func(LABELS[op]), args)
            value = T.s32(e.u32(result))
        return dict(result=value, out=e.cl.lab(e.u32(out)),
                    state=[T.s32(e.u32(obj + 12)), e.cl.lab(e.u32(obj))], log=e.cl.log)
    finally:
        CL.Stubs.deactivate(e, saved)


def host(lib, c, op):
    C.c_int32.in_dll(lib, "tpw_cl_on").value = 0
    obj = F(lib, "tpw_manager_new", VP)()
    bytes = C.create_string_buffer(c["bytes"])
    F(lib, "tpw_manager_seed", None, VP, I32, I32, VP, U32)(obj, c["opened"], c["old"], bytes, len(c["bytes"]))
    CL.host_begin(lib, {1: obj, 2: F(lib, "tpw_manager_storage", VP, VP)(obj),
                        3: F(lib, "tpw_manager_resource", VP)()}, c["queues"])
    out = I32()
    value = F(lib, "tpw_manager_call", I32, VP, I32, U32, C.POINTER(I32))(obj, op, c["id"], C.byref(out))
    log = CL.host_end(lib)
    state = (I32 * 2)()
    F(lib, "tpw_manager_read", None, VP, C.POINTER(I32))(obj, state)
    return dict(result=value, out=out.value, state=list(state), log=log)


TESTS = [FnTest(name, lambda e, c, op=i: ppc(e, c, op), lambda l, c, op=i: host(l, c, op), rnd=case)
         for i, name in enumerate([
    "TbTextResourceManager::Open", "TbTextResourceManager::SetLanguage", "TbTextResourceManager::LoadResourceIndex",
    "TbTextResourceManager::GetResource(MBToUnicode)", "TbTextResourceManager::GetResource(UnicodeToMB)",
    "TbTextResourceManager::GetResource(Table)", "TbTextResourceManager::GetResource(Font)",
    "TbTextResourceManager::TbTextResourceManager", "TbTextResourceManager::Close", "TbTextResourceManager::~TbTextResourceManager"])]


def edges():
    r = random.Random(84)
    cases = []
    for opened in (0, 1, -1):
        for old in (0, 1):
            for type in (0, 1, 3, 4, 6):
                c = case(r)
                c.update(opened=opened, old=old, bytes=struct.pack("<II", 0x49524642, 1) + bytes(12))
                c["queues"] = {"StorageOpen": [0], "OpenFile": [0], "FullPath": [0],
                               "Read": [8, 12], "Initialise": [0], "Handle": [1], "Type": [type], "Get": [7]}
                cases.append(c)
    return cases


for test in TESTS:
    test._edge = edges
