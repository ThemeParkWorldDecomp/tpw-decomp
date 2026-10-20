"""Unit w68, the online system's construction, against libtpw_unit_OnlineInit.dylib (tests/unit_OnlineInit.cpp), in the main app
("SimTheme Park"):
  src/FS_Online_init.cpp  CFSOnlineSystem::CFSOnlineSystem 0x1016f7d0, InitLogin 0x101719f8, ReleaseLoginConnection 0x10171b68,
                          InitNews 0x10172238, ReleaseNewsConnection 0x1017249c, InitCity 0x101735cc, ReleaseCityServerConnection
                          0x10174ac8, InitUpload 0x10175a18, ReleaseUploadServerConnection 0x1017598c, InitFTPDownload 0x10176cec,
                          ReleaseFTPDownloadServerConnection 0x101770dc, TickLogin 0x10171adc
  src/FS_OnlineList.cpp   CFSOnlineSystemThemeList 0x10179a28, CFSOnlineSystemServerList 0x1017a604, CFSOnlineSystemRideList
                          0x1017b1ac constructors, CFSOnlinePark::CFSOnlinePark() 0x1017bf30

A case is a script on one online system: the object is made over filler bytes (the constructor, traced for the constructor cases),
and the steps are "apply" (write a randomised state into the object: every word and string, the connections, the stored answers of the
nine lists) and "call" (run one member). After the constructor and after every call the whole object is dumped as a list of integers:
every member in the order of the Mac's offsets (pointers normalised to 0, the index of an allocation the code made, the label of a test
object, or -1; a string as its length and a hash of its text), then the nine list objects, their typed lists and the park the system
made, then the heap traffic (operator new and delete counts). The PowerPC side reads the Mac object at the same offsets; the host side
walks the same members (tests/unit_OnlineInit.cpp checks the member order against SCHEMA below).

On the PowerPC side the sixteen functions, the three list constructors and the TbTList constructors run as the original's code. The
online library's Auth, News, City and Upload members and destructors, the FTP client's virtual destructor, the debug log (0x10000e40), the
clock, CompleteLogin, the five "cc" info destructors and the runtime's __destroy_new_array are recorders; operator new fills its blocks
with 0xa5 (so does the host). The strings are the functional fakes of tests/test_FSSystem.py.
"""
import ctypes as C
import random
import struct

import calllog as CL
import test_FSSystem as FS
import tpwtest as T
from tpwtest import FnTest

PROGRAM = "SimTheme Park"
SHIM_LIBRARY = "unit_OnlineInit"

MASK = 0xFFFFFFFF
ADDR = dict(ctor=0x1016F7D0, InitLogin=0x101719F8, ReleaseLogin=0x10171B68, InitNews=0x10172238, ReleaseNews=0x1017249C,
            InitCity=0x101735CC, ReleaseCity=0x10174AC8, InitUpload=0x10175A18, ReleaseUpload=0x1017598C, InitFTP=0x10176CEC,
            ReleaseFTP=0x101770DC, TickLogin=0x10171ADC, Theme=0x10179A28, Server=0x1017A604, Ride=0x1017B1AC, Park=0x1017BF30)
TBLIST_CTORS = [0x101779D8, 0x10177C6C, 0x10177F00, 0x1017CCE4, 0x1017D008, 0x1017D340]
UDEBUG, COMPLETE_LOGIN = 0x10000E40, 0x10171C08
INFO_DTORS = {"ccParkInfo": 0x1016FF48, "ccCityInfo": 0x101703E4, "ccRideInfo": 0x10173110, "ccServerInfo": 0x101731DC,
              "ccThemeInfo": 0x101732E4}
SYSTEM_SIZE = 0x404

# The final vtable of each class (the kind numbers of unit_OnlineInit.cpp's KindOf).
KIND = {0x102A769C: 1, 0x102A75B4: 2, 0x102A75E8: 3, 0x102A7650: 4, 0x102A761C: 5, 0x102A7524: 6, 0x102A74B0: 7, 0x102A743C: 8,
        0x102A7344: 9, 0x102A72FC: 10, 0x102A7558: 11, 0x102A74E4: 12, 0x102A7470: 13, 0x102A738C: 14, 0x102A7400: 15}
# The ops of tpw_oi_call.
OPS = dict(InitLogin=1, ReleaseLogin=2, InitNews=3, ReleaseNews=4, InitCity=5, ReleaseCity=6, InitUpload=7, ReleaseUpload=8,
           InitFTP=9, ReleaseFTP=10, TickLogin=11)
TEXTS = ["", "a", "Hello", "xyz 123", "A longer text than the others, 40 chars!"]
LABEL_CONNECTION, LABEL_HOLDER, LABEL_ARRAY, LABEL_INFO, LABEL_LIST = 10, 20, 30, 50, 300
LIST_OFFSETS = [0xE0, 0xE4, 0xE8, 0xEC, 0xF0, 0xF4, 0xF8, 0xFC, 0x100]
INFO_ELEMENT = 8     # the size of one info on the PowerPC side's fake arrays (the runtime reads it from the cookie)

# ---------------------------------------------------------------------------------------------------- the schema


# The words unit w69 named (the others are mFxxx).
RENAMED = {0x324: "mUploadActive", 0x328: "mUploadDone", 0x32C: "mUploadFailed", 0x38C: "mFTPActive", 0x390: "mFTPDone", 0x394: "mFTPResult"}


def build_schema():
    """(offset, kind, name) in the order of unit_OnlineInit.cpp's BuildSlots; kinds: w word, p pointer, f the login callback (a function
    pointer: 0, the test's fake, or other), x a context word held in a pointer, W wide string, S narrow string, L inline list."""
    s = []

    def add(off, kind, name):
        s.append((off, kind, name))

    def pad(off, name, words):
        for i in range(words):
            add(off + 4 * i, "w", f"{name}[{i}]")

    add(0x00, "p", "mLogin")
    add(0x04, "f", "mLoginCallback")
    add(0x08, "x", "mLoginContext")
    add(0x0C, "W", "mLoginName")
    add(0x20, "W", "mLoginPassword")
    add(0x34, "w", "mF34")
    add(0x38, "w", "mF38")
    add(0x3C, "W", "mLoginEmail")
    add(0x50, "w", "mF50")
    pad(0x54, "mF54", 3)
    add(0x60, "w", "mLoggedIn")
    add(0x64, "w", "mLoginDone")
    add(0x68, "w", "mLoginErrorID")
    add(0x6C, "p", "mNews")
    add(0x70, "w", "mF70")
    add(0x74, "w", "mNewsRequest1")
    add(0x78, "w", "mNewsDone1")
    add(0x7C, "S", "mNewsText")
    add(0x90, "w", "mNewsRequest2")
    add(0x94, "w", "mNewsDone2")
    add(0x98, "W", "mF98")
    add(0xAC, "w", "mNewsRequest3")
    add(0xB0, "w", "mNewsDone3")
    add(0xB4, "W", "mFB4")
    add(0xC8, "f", "mLoginCallbackKept")
    add(0xCC, "x", "mLoginContextKept")
    add(0xD0, "w", "mNewsFinished")
    add(0xD4, "p", "mCity")
    add(0xD8, "w", "mCityClock")
    add(0xDC, "w", "mCityListsWanted")
    for off, name in zip(LIST_OFFSETS, ["mCityList", "mParksTop10", "mParksInCity", "mParksByPlayer", "mParksByID", "mParksInCity2",
                                         "mThemeList", "mServerList", "mRideList"]):
        add(off, "p", name)
    add(0x104, "p", "mUpload")
    add(0x108, "w", "mF108")
    add(0x10C, "S", "mF10C")
    pad(0x120, "mF120", 0x200 // 4)
    for off in (0x320, 0x324, 0x328, 0x32C):
        add(off, "w", RENAMED.get(off, f"mF{off:X}"))
    add(0x330, "S", "mF330")
    add(0x344, "w", "mF344")
    add(0x348, "W", "mF348")
    add(0x35C, "W", "mF35C")
    for off in (0x370, 0x374, 0x378, 0x37C, 0x380, 0x384, 0x388, 0x38C, 0x390, 0x394, 0x398):
        add(off, "w", RENAMED.get(off, f"mF{off:X}"))
    add(0x39C, "p", "mF39C")
    add(0x3A0, "p", "mFTP")
    add(0x3A4, "p", "mFTPPark")
    add(0x3A8, "w", "mF3A8")
    add(0x3AC, "S", "mF3AC")
    add(0x3C0, "w", "mF3C0")
    add(0x3C4, "w", "mF3C4")
    add(0x3C8, "L", "mF3C8")
    pad(0x3D8, "mF3D8", 1)
    add(0x3DC, "S", "mF3DC")
    add(0x3F0, "w", "mFTPParkPending")
    pad(0x3F4, "mF3F4", 4)
    return s


SCHEMA = build_schema()
NAME_INDEX = {name: i for i, (_, _, name) in enumerate(SCHEMA)}
# The values the spec carries for each slot (words and strings only), by slot name.
SPEC_SLOTS = [(off, kind, name) for off, kind, name in SCHEMA if kind in "wWSfx"]

# ---------------------------------------------------------------------------------------------------- the PowerPC side

STUBS = None
FTP_VTABLE = 0
CALLBACK = 0       # the function descriptor of the fake login callback


def setup(runner):
    global STUBS, FTP_VTABLE, CALLBACK
    e = runner.emu
    T.install_std_stubs(e)
    if not FS.VT:
        FS.make_vtables(e)
    # The login callback the system's callback words may hold: records its two arguments (error, context).
    CALLBACK = T.fake_function(e, CL.recorder("LoginCallback", "ii"))

    def destroy_new_array(em, call):
        # The runtime's __destroy_new_array(first, destructor): the element size and count are the cookie before the array; the
        # destructor runs from the last element to the first (flag -1), then the block is freed.
        first, dtor_vector = em.r(3), em.r(4)
        if first:
            size, count = struct.unpack(">II", em.read(first - 8, 8))
            for i in reversed(range(count)):
                call(dtor_vector, first + i * size, MASK)
            # (the array's label is kept apart from its first element's: both live at `first`)
            label = em.oi.arrays.pop(first, 0)
            if label > 0:
                em.cl.rec("delete", (label,))
        return None

    T.stub_cb(e, "__destroy_new_array", destroy_new_array)
    FTP_VTABLE = T.persist_alloc(e, 0x10)
    FS.w32(e, FTP_VTABLE + 8, T.fake_function(e, deleting_dtor("FTP.dtor")))
    STUBS = build_stubs()


def deleting_dtor(name):
    """A destructor taking the delete flag (r4): records like calllog's "D", and a positive flag counts as an operator delete."""
    record = CL.recorder(name, "D")

    def stub(e):
        obj, flag = e.r(3), T.s16(e.r(4))
        record(e)
        if flag > 0 and obj:
            e.oi.heap[1] += 1
    return stub


def build_stubs():
    S = CL.Stubs()
    R = CL.recorder
    S.real.update(ADDR.values())
    S.real.update(TBLIST_CTORS)
    FS.string_stubs(S)

    def op_new(e):
        side, size = e.oi, max(e.r(3), 4)
        block = e.alloc(size, b"\xa5" * size)
        if side.track:
            side.allocs.append(block)
        side.heap[0] += 1
        e.set_r(3, block)

    def op_delete(e):
        side, L, obj = e.oi, e.cl, e.r(3)
        if obj:
            side.heap[1] += 1
        if obj and L.lab(obj) > 0:
            L.rec("delete", (L.lab(obj),))
            L.forget(obj)

    S.imp("__nw__FUl", op_new)
    S.imp("__dl__FPv", op_delete)
    S.imp("LbTime_GetClock__Fv", R("LbTime_GetClock", "", "i"))
    S.imp("Release__4AuthFv", R("Auth.Release", "L"))
    S.imp("__dt__4AuthFv", deleting_dtor("Auth.dtor"))
    S.imp("Release__4NewsFv", R("News.Release", "L"))
    S.imp("__dt__4NewsFv", deleting_dtor("News.dtor"))
    S.imp("Release__4CityFv", R("City.Release", "L"))
    S.imp("__dt__4CityFv", deleting_dtor("City.dtor"))
    S.imp("Release__6UploadFv", R("Upload.Release", "L"))
    S.internal(UDEBUG, R("UDebug", "s"))
    S.internal(COMPLETE_LOGIN, R("CompleteLogin", "Li"))
    for name, addr in INFO_DTORS.items():
        S.internal(addr, R(f"{name}.dtor", "L"))
    return S


def w32(e, a, v):
    e.write(a, struct.pack(">I", v & MASK))


def s32(v):
    return T.s32(v & MASK)


def fnv(units):
    h = 2166136261
    for u in units:
        h = ((h ^ (u & 0xFFFF)) * 16777619) & MASK
    return h


class Side:
    """One case on the PowerPC side: the log, the allocations the code made, the heap counters, the online system."""

    def __init__(self, e, case):
        self.e = e
        e.oi = self
        e.cl = self.L = CL.PpcLog({}, case.get("queues"))
        self.allocs, self.heap, self.track = [], [0, 0], False
        self.arrays = {}
        self.obj = 0
        self.saved = STUBS.activate(e)

    def finish(self):
        CL.Stubs.deactivate(self.e, self.saved)

    # ---- reading
    def u32(self, a):
        return self.e.u32(a)

    def norm(self, a):
        if a == 0:
            return 0
        if a in self.allocs:
            return 1000 + self.allocs.index(a)
        lab = self.L.lab(a)
        return lab if lab > 0 else -1

    def kind(self, a):
        return KIND.get(self.u32(a), -1)

    def callback(self, a):
        return 0 if a == 0 else 1 if a == CALLBACK else -1

    def string(self, a, wide):
        text = FS.s_read(self.e, a, wide)
        return [s32(self.u32(a + 0xC)), fnv([ord(ch) for ch in text])]

    def tblist(self, a, out):
        out += [self.kind(a), self.norm(self.u32(a + 4)), self.norm(self.u32(a + 8)), s32(self.u32(a + 0xC))]

    def dump_list(self, a, out):
        if a == 0:
            return
        kind = self.kind(a)
        out.append(kind)
        for off in range(4, 0x2C, 4):
            out.append(s32(self.u32(a + off)))
        out.append(self.norm(self.u32(a + 0x2C)))
        out.append(self.norm(self.u32(a + 0x30)))
        if self.u32(a + 0x2C):
            self.tblist(self.u32(a + 0x2C), out)
        if kind in (2, 4, 5):
            out.append(s32(self.u32(a + 0x34)))
        elif kind == 3:
            out += self.string(a + 0x34, True)

    def dump_park(self, a, out):
        if a == 0:
            return
        e = self.e
        out += [self.kind(a), self.norm(self.u32(a + 4)), s32(self.u32(a + 8))]
        for off in (0x0C, 0x20, 0x34):
            out += self.string(a + off, True)
        out += [s32(self.u32(a + off)) for off in (0x48, 0x4C, 0x50, 0x54)]
        out += [s32(self.u32(a + 0x58 + 4 * i)) for i in range(8)]
        out.append(s32(self.u32(a + 0x78)))
        out += self.string(a + 0x7C, False)
        out.append(s32(self.u32(a + 0x90)))
        out += self.string(a + 0x94, True)
        out += [T.s16(struct.unpack(">H", e.read(a + off, 2))[0]) for off in (0xA8, 0xAA, 0xAC, 0xAE)]
        out += [s32(self.u32(a + off)) for off in (0xB0, 0xB4, 0xB8, 0xBC)]

    def dump(self):
        out = [self.heap[0], self.heap[1]]
        self.heap = [0, 0]
        if self.obj_kind == "system":
            a = self.obj
            for off, kind, name in SCHEMA:
                if kind == "w":
                    out.append(s32(self.u32(a + off)))
                elif kind == "p":
                    out.append(self.norm(self.u32(a + off)))
                elif kind == "f":
                    out.append(self.callback(self.u32(a + off)))
                elif kind == "x":
                    out.append(s32(self.u32(a + off)))
                elif kind == "W":
                    out += self.string(a + off, True)
                elif kind == "S":
                    out += self.string(a + off, False)
                else:
                    self.tblist(a + off, out)
            for off in LIST_OFFSETS:
                self.dump_list(self.u32(a + off), out)
            self.dump_park(self.u32(a + 0x3A4), out)
        elif self.obj_kind in (1, 2, 3):
            self.dump_list(self.obj, out)
        else:
            self.dump_park(self.obj, out)
        return out

    # ---- building
    def system(self, traced, fill):
        e = self.e
        self.obj_kind = "system"
        self.obj = e.alloc(SYSTEM_SIZE, bytes([fill]) * SYSTEM_SIZE)
        self.L.labels[self.obj] = 1
        self.allocs, self.heap, self.track = [], [0, 0], True
        del self.L.log[:]
        T.call_ppc(e, ADDR["ctor"], [self.obj])
        self.track = False
        if not traced:
            self.heap = [0, 0]
            del self.L.log[:]

    def block(self, kind, fill):
        e = self.e
        self.obj_kind = kind
        size = {1: 0x34, 2: 0x34, 3: 0x34, 4: 0xC0}[kind]
        self.obj = e.alloc(size, bytes([fill]) * size)
        self.L.labels[self.obj] = 1
        self.allocs, self.heap, self.track = [], [0, 0], True
        del self.L.log[:]
        T.call_ppc(e, {1: ADDR["Theme"], 2: ADDR["Server"], 3: ADDR["Ride"], 4: ADDR["Park"]}[kind], [self.obj])
        self.track = False

    def connection(self, index, vtable=0):
        e = self.e
        size = 0x40 if index == 5 else 16      # (the FTP client has progress words at 0x18 and 0x1c)
        block = e.alloc(size, b"\xa5" * size)
        if vtable:
            w32(e, block, vtable)
        self.L.labels[block] = LABEL_CONNECTION + index
        return block

    def received(self, list_addr, index, recv):
        kind, count, infos, refs, word = recv
        e = self.e
        w32(e, list_addr + 0x30, 0)
        if kind == 0:
            return
        holder = e.alloc(0x10, b"\xa5" * 0x10)
        array = 0
        if infos >= 0:
            base = e.alloc(8 + infos * INFO_ELEMENT, b"\xa5" * (8 + infos * INFO_ELEMENT))
            w32(e, base, INFO_ELEMENT)
            w32(e, base + 4, infos)
            array = base + 8
            self.arrays[array] = LABEL_ARRAY + index
            for k in range(infos):
                self.L.labels[array + k * INFO_ELEMENT] = LABEL_INFO + 4 * index + k
        e.write(holder, struct.pack(">IIII", count & MASK, array, word & MASK, refs & MASK))
        self.L.labels[holder] = LABEL_HOLDER + index
        w32(e, list_addr + 0x30, holder)

    def apply(self, spec):
        e, a = self.e, self.obj
        it = iter(spec)
        for off, kind, name in SPEC_SLOTS:
            v = next(it)
            if kind in "wx":
                w32(e, a + off, v)
            elif kind == "f":
                w32(e, a + off, CALLBACK if v else 0)
            else:
                FS.s_write(e, a + off, TEXTS[v % len(TEXTS)], kind == "W")
        for off, index, vt in ((0x00, 0, 0), (0x6C, 1, 0), (0xD4, 2, 0), (0x104, 3, 0), (0x39C, 4, 0), (0x3A0, 5, FTP_VTABLE)):
            w32(e, a + off, self.connection(index, vt) if next(it) else 0)
        for index, off in enumerate(LIST_OFFSETS):
            lst = self.u32(a + off)
            self.L.labels[lst] = LABEL_LIST + index
            for k in range(4, 0x2C, 4):
                w32(e, lst + k, next(it))
            self.received(lst, index, [next(it) for _ in range(5)])
        total, done = next(it), next(it)
        if self.u32(a + 0x3A0):
            w32(e, self.u32(a + 0x3A0) + 0x18, total)
            w32(e, self.u32(a + 0x3A0) + 0x1C, done)
        self.heap = [0, 0]

    def call(self, op):
        self.track = True
        name = {v: k for k, v in OPS.items()}[op]
        T.call_ppc(self.e, ADDR[name], [self.obj])
        self.track = False


def run_ppc(e, case):
    side = Side(e, case)
    try:
        dumps = []
        if case["mode"] == "system":
            side.system(case["traced"], case["fill"])
            dumps.append(side.dump())
            for step in case["steps"]:
                if step[0] == "apply":
                    side.apply(step[1])
                else:
                    side.call(step[1])
                    dumps.append(side.dump())
        else:
            side.block(case["kind"], case["fill"])
            dumps.append(side.dump())
        return dict(dumps=dumps, log=side.L.log)
    finally:
        side.finish()


# ---------------------------------------------------------------------------------------------------- the host side


def host_lib(lib):
    F = FS.F
    return dict(
        system=F(lib, "tpw_oi_system", None, C.c_int32, C.c_int32),
        block=F(lib, "tpw_oi_block", None, C.c_int32, C.c_int32),
        apply=F(lib, "tpw_oi_apply", None, C.POINTER(C.c_int64)),
        call=F(lib, "tpw_oi_call", None, C.c_int32),
        dump=F(lib, "tpw_oi_dump", C.c_int32, C.POINTER(C.c_int64), C.c_int32))


def host_dump(fns):
    out = (C.c_int64 * 4096)()
    n = fns["dump"](out, 4096)
    assert n <= 4096, n
    return list(out[:n])


def run_host(lib, case):
    fns = host_lib(lib)
    CL.host_begin(lib, {}, case.get("queues"))
    dumps = []
    if case["mode"] == "system":
        fns["system"](1 if case["traced"] else 0, case["fill"])
        dumps.append(host_dump(fns))
        for step in case["steps"]:
            if step[0] == "apply":
                values = (C.c_int64 * len(step[1]))(*[s32(v) for v in step[1]])
                fns["apply"](values)
            else:
                fns["call"](step[1])
                dumps.append(host_dump(fns))
    else:
        fns["block"](case["kind"], case["fill"])
        dumps.append(host_dump(fns))
    return dict(dumps=dumps, log=CL.host_end(lib))


class OnlineTest(FnTest):
    """FnTest that also checks, once, that the host's slot table is the schema, and names the first differing dump entry."""
    checked = False

    def run_case(self, runner, case):
        if not OnlineTest.checked:
            OnlineTest.checked = True
            buf = C.create_string_buffer(1 << 16)
            runner.lib.tpw_oi_slots(buf, 1 << 16)
            host = buf.value.decode("latin-1").split("\n")[:-1]
            mine = [f"{kind}:{name}" for _, kind, name in SCHEMA]
            if host != mine:
                i = next((i for i, (a, b) in enumerate(zip(host, mine)) if a != b), min(len(host), len(mine)))
                return [f"slot table differs at {i}: host {host[i:i + 2]} schema {mine[i:i + 2]} ({len(host)}/{len(mine)} slots)"]
        return super().run_case(runner, case)


# ---------------------------------------------------------------------------------------------------- the cases


def spec_words(r, overrides=None):
    """The spec for tpw_oi_apply: a random value for every word and string slot, the six connection choices, the nine lists."""
    overrides = overrides or {}
    spec = []
    for off, kind, name in SPEC_SLOTS:
        if name in overrides:
            spec.append(overrides[name])
        elif kind in "wx":
            spec.append(r.choice([0, 0, 1, MASK, r.getrandbits(32), r.getrandbits(32)]))
        elif kind == "f":
            spec.append(r.randrange(2))
        else:
            spec.append(r.randrange(len(TEXTS)))
    return spec


def full_spec(r, conns, recvs=None, overrides=None, list_words=None, ftp=None):
    """conns: six 0/1 choices (login, news, city, upload, the buffer at 0x39c, the FTP client); recvs: nine received-answer tuples
    (kind, count, infos, refs, word) or None for a random one; ftp: the FTP client's total and done words."""
    spec = spec_words(r, overrides)
    spec += list(conns)
    for i in range(9):
        spec += list_words[i] if list_words else [r.choice([0, 1, MASK, r.getrandbits(32)]) for _ in range(10)]
        spec += list(recvs[i]) if recvs and recvs[i] is not None else random_recv(r)
    spec += list(ftp) if ftp else [r.choice([0, 1, 100, MASK, r.getrandbits(32)]), r.choice([0, 1, 50, MASK, r.getrandbits(32)])]
    return spec


NO_RECV = (0, 0, -1, 0, 0)


def random_recv(r):
    kind = r.choice([0, 1, 1, 1])
    return [kind, r.getrandbits(32), r.choice([-1, 0, 1, 2, 3]), r.choice([1, 1, 1, 2, 0, MASK, 3, 0x7FFFFFFF]), r.getrandbits(32)]


def system_case(steps, traced=False, fill=0xA5, clock=None, r=None):
    queues = {"LbTime_GetClock": [clock if clock is not None else (r.getrandbits(32) if r else 0x1234)]}
    return dict(mode="system", traced=traced, fill=fill, steps=steps, queues=queues)


def one_op_case(op, spec, r):
    return system_case([("apply", spec), ("call", OPS[op])], clock=r.getrandbits(32))


def random_conns(r):
    return [r.randrange(2) for _ in range(6)]


def op_edges(op):
    def edges():
        r = random.Random(sum(map(ord, op)))
        for conns in ([0] * 6, [1] * 6, [1, 0, 0, 0, 0, 0], [0, 1, 0, 0, 0, 0], [0, 0, 1, 0, 0, 0], [0, 0, 0, 1, 0, 0],
                      [0, 0, 0, 0, 1, 0], [0, 0, 0, 0, 0, 1], [0, 0, 0, 0, 1, 1]):
            yield one_op_case(op, full_spec(r, conns), r)
        # a quiet object with nothing connected and every word zero or all ones
        for fill in (0x00, 0xFF):
            overrides = {name: (0 if fill == 0 else MASK) for _, kind, name in SPEC_SLOTS if kind == "w"}
            yield one_op_case(op, full_spec(r, [1, 1, 1, 1, 1, 1], overrides=overrides), r)
    return edges


def op_random(op):
    def rnd(r):
        return one_op_case(op, full_spec(r, random_conns(r)), r)
    return rnd


def city_edges():
    r = random.Random(77)
    # each list alone with an answer: one reference left (freed), two (only counted down), zero (goes to -1), no array, empty array
    for index in range(9):
        for recv in ((1, 5, 2, 1, 7), (1, 5, 3, 2, 7), (1, 5, 1, 0, 7), (1, 5, -1, 1, 7), (1, 5, 0, 1, 7), (1, 0, 3, MASK, 7),
                     (1, 5, 1, 0x7FFFFFFF, 7), (1, 5, 1, 0x80000000, 7)):
            recvs = [NO_RECV] * 9
            recvs[index] = recv
            yield one_op_case("InitCity", full_spec(r, random_conns(r), recvs), r)
    for recv in ((1, 5, 2, 1, 7), (1, 5, -1, 1, 7), (1, 5, 3, 2, 7)):
        yield one_op_case("InitCity", full_spec(r, [1] * 6, [recv] * 9), r)
    yield one_op_case("InitCity", full_spec(r, [0] * 6, [NO_RECV] * 9), r)
    yield one_op_case("InitCity", full_spec(r, [0, 0, 1, 0, 0, 0], [NO_RECV] * 9), r)


def city_random(r):
    recvs = [random_recv(r) if r.randrange(3) else NO_RECV for _ in range(9)]
    return one_op_case("InitCity", full_spec(r, random_conns(r), recvs), r)


def tick_edges():
    r = random.Random(5)
    for conns in ([0] * 6, [1, 0, 0, 0, 0, 0]):
        for done in (0, 1, 2, MASK, 0x80000000, 0x100):
            for logged in (0, 1):
                overrides = {"mLoginDone": done, "mLoggedIn": logged}
                yield one_op_case("TickLogin", full_spec(r, conns, overrides=overrides), r)


def tick_random(r):
    overrides = {"mLoginDone": r.choice([0, 1, MASK, r.getrandbits(32)])}
    return one_op_case("TickLogin", full_spec(r, random_conns(r), overrides=overrides), r)


def ctor_edges():
    for fill in (0xA5, 0x00, 0xFF, 0x5A):
        for clock in (0, 1, MASK, 0x12345678):
            yield system_case([], traced=True, fill=fill, clock=clock)


def ctor_random(r):
    return system_case([], traced=True, fill=r.choice([0xA5, 0xA5, 0x00, 0xFF, r.getrandbits(8)]), clock=r.getrandbits(32))


def block_edges(kind):
    def edges():
        for fill in (0xA5, 0x00, 0xFF, 0x5A, 0x01):
            yield dict(mode="block", kind=kind, fill=fill, queues={})
    return edges


def block_random(kind):
    return lambda r: dict(mode="block", kind=kind, fill=r.choice([0xA5, 0x00, 0xFF, r.getrandbits(8)]), queues={})


def script_case(r):
    """Random scripts: the Init* and Release* members in any order with fresh random state applied between them."""
    steps = []
    for _ in range(r.randrange(2, 9)):
        if r.randrange(3):
            recvs = [random_recv(r) if r.randrange(4) == 0 else NO_RECV for _ in range(9)]
            steps.append(("apply", full_spec(r, random_conns(r), recvs)))
        steps.append(("call", r.choice(list(OPS.values()))))
    return system_case(steps, traced=r.randrange(2) == 0, fill=r.choice([0xA5, 0x00]), clock=r.getrandbits(32))


def script_edges():
    r = random.Random(11)
    for op in OPS:
        yield system_case([("apply", full_spec(r, [1] * 6)), ("call", OPS[op]), ("call", OPS[op])], clock=3)
    yield system_case([("apply", full_spec(r, [1] * 6))] + [("call", OPS[op]) for op in OPS], clock=9)


TESTS = [OnlineTest("CFSOnlineSystem::CFSOnlineSystem 0x1016f7d0", run_ppc, run_host, edge=ctor_edges, rnd=ctor_random)]
for _op, _name in (("InitLogin", "CFSOnlineSystem::InitLogin 0x101719f8"), ("ReleaseLogin", "ReleaseLoginConnection 0x10171b68"),
                   ("InitNews", "InitNews 0x10172238"), ("ReleaseNews", "ReleaseNewsConnection 0x1017249c")):
    TESTS.append(OnlineTest(_name, run_ppc, run_host, edge=op_edges(_op), rnd=op_random(_op)))
TESTS.append(OnlineTest("InitCity 0x101735cc", run_ppc, run_host, edge=city_edges, rnd=city_random))
for _op, _name in (("ReleaseCity", "ReleaseCityServerConnection 0x10174ac8"), ("InitUpload", "InitUpload 0x10175a18"),
                   ("ReleaseUpload", "ReleaseUploadServerConnection 0x1017598c"), ("InitFTP", "InitFTPDownload 0x10176cec"),
                   ("ReleaseFTP", "ReleaseFTPDownloadServerConnection 0x101770dc")):
    TESTS.append(OnlineTest(_name, run_ppc, run_host, edge=op_edges(_op), rnd=op_random(_op)))
TESTS.append(OnlineTest("TickLogin 0x10171adc", run_ppc, run_host, edge=tick_edges, rnd=tick_random))
for _kind, _name in ((1, "CFSOnlineSystemThemeList 0x10179a28"), (2, "CFSOnlineSystemServerList 0x1017a604"),
                     (3, "CFSOnlineSystemRideList 0x1017b1ac"), (4, "CFSOnlinePark::CFSOnlinePark() 0x1017bf30")):
    TESTS.append(OnlineTest(_name, run_ppc, run_host, edge=block_edges(_kind), rnd=block_random(_kind)))
TESTS.append(OnlineTest("scripts over the Init* and Release* members", run_ppc, run_host, edge=script_edges, rnd=script_case))
