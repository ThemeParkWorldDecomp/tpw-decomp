"""Unit w69, the online system's tickers, against libtpw_unit_OnlineTick.dylib (tests/unit_OnlineTick.cpp), in the main app
("SimTheme Park"):
  src/FS_Online_tick.cpp  CFSOnlineSystem::TickNews 0x1017232c, TickCity 0x10173afc, TickUpload 0x10175b00, TickFTPDownload 0x10176da4,
                          CompleteLogin 0x10171c08, AreWeLoggedIn 0x1017178c

The harness is unit w68's (tests/test_OnlineInit.py, imported as OI: a case is a script on one online system built over filler bytes
by the original constructor, with "apply" steps that write a randomised state and "call" steps; after the constructor and after every
call the whole object is dumped, and the call logs are compared in order). The tickers are driven here by ops 12 to 16 (and
CompleteLogin by 32 + the index of its error in ERRORS); the constructor also reads the clock, so the clock queue starts with a value
for it.

On the PowerPC side the six functions, the w68 members they call (ReleaseLoginConnection is inlined on the Mac, so the original's
Auth.Release, Auth.dtor and UDebug events are compared against the host's call of the member), __ptr_glue (the login callback is a
function descriptor) and the string classes' virtuals run as the original's code. Recorders: News, City and Upload ::GetCurrentMessage and
::MonitorMessage (the id from a queue, the words done and total written from two more), the interface calls (CreateUIProgressBar,
UpdateUIProgressBar, FuckOffUIProgressBar, UpdateParkFindPanelProgress, AddUIError, RemoveOnlineLoginPanel, CreateOnlineOutboxPanel with
an answer queue), the job completions (CompleteNews, CompleteUpload, CompleteFTPDownload, LoggedInGetNews), the lists' GetList and the
five CompleteGetList (which sets the list's word 0xc from a queue) and the login callback.
"""
import itertools
import random
import struct

import calllog as CL
import test_OnlineInit as OI
import tpwtest as T

PROGRAM = OI.PROGRAM
SHIM_LIBRARY = "unit_OnlineTick"

MASK = OI.MASK
TICK_ADDR = {12: 0x1017232C, 13: 0x10173AFC, 14: 0x10175B00, 15: 0x10176DA4}
ARE_WE_LOGGED_IN, COMPLETE_LOGIN_OP = 0x1017178C, 32
COMPLETE_LOGIN = OI.COMPLETE_LOGIN
PTR_GLUE = 0x1023BAA8
ERRORS = [0, 1, -1, 7, 0x7FFFFFFF, -0x80000000, 0x100, 2]    # unit_OnlineTick.cpp's kErrors
NO_RECV = OI.NO_RECV
# The list slots of the online system, by offset (the spec carries the lists in this order).
CITY, TOP10, INCITY, BYPLAYER, BYID, INCITY2, THEME, SERVER, RIDE = range(9)
# The ids TickCity compares with, in its order, and the progress bar text each earns.
CITY_MATCH = [(CITY, 0xF9), (TOP10, 0xFA), (INCITY, 0xFB), (BYPLAYER, 0xFC), (INCITY2, 0xFD), (THEME, 0xFE), (SERVER, 0xFF), (RIDE, 0x100)]

# ---------------------------------------------------------------------------------------------------- the PowerPC side


def monitor_then(name):
    """MonitorMessage(this, id, &done, &total): writes the two words from the queues."""
    def then(e, L):
        OI.w32(e, e.r(5), L.ret(f"{name}.MonitorMessage.done"))
        OI.w32(e, e.r(6), L.ret(f"{name}.MonitorMessage.total"))
    return then


def complete_get_list_then(e, L):
    OI.w32(e, e.r(3) + 0xC, L.ret("CompleteGetList.set"))


def build_tick_stubs():
    S = OI.build_stubs()
    R = CL.recorder
    # CompleteLogin and the tickers are the unit's own code now; __ptr_glue calls the login callback
    S.internals.pop(COMPLETE_LOGIN)
    S.real.update(list(TICK_ADDR.values()) + [ARE_WE_LOGGED_IN, COMPLETE_LOGIN, PTR_GLUE])
    for cls in ("News", "City", "Upload"):
        mangled = {"News": "4News", "City": "4City", "Upload": "6Upload"}[cls]
        S.imp(f"GetCurrentMessage__{mangled}Fi", R(f"{cls}.GetCurrentMessage", "Li", ret="i"))
        S.imp(f"MonitorMessage__{mangled}FUlPiPi", R(f"{cls}.MonitorMessage", "LuLL", then=monitor_then(cls)))
    S.internal(0x101E01F0, R("CreateUIProgressBar", "i"))
    S.internal(0x101E0300, R("UpdateUIProgressBar", "i"))
    S.internal(0x101EA4E4, R("UpdateParkFindPanelProgress", "i"))
    S.internal(0x101E0294, R("FuckOffUIProgressBar", ""))
    S.internal(0x101F1DF0, R("AddUIError", "iL"))
    S.internal(0x101E2BE4, R("RemoveOnlineLoginPanel", "i"))
    S.internal(0x101E4F28, R("CreateOnlineOutboxPanel", "", ret="i"))
    S.internal(0x1017253C, R("CompleteNews", "L"))
    S.internal(0x10171EB8, R("LoggedInGetNews", "L"))
    S.internal(0x10175C20, R("CompleteUpload", "Li"))
    S.internal(0x10176E90, R("CompleteFTPDownload", "Li"))
    S.internal(0x1017A1C0, R("ThemeList.GetList", "L"))
    S.internal(0x1017ADA8, R("ServerList.GetList", "L"))
    S.internal(0x1017B938, R("RideList.GetList", "L"))
    S.internal(0x10178F54, R("InCity.GetList", "LiLL"))
    for addr, name in ((0x10173F98, "City"), (0x101741D4, "Park"), (0x1017488C, "Theme"), (0x1017464C, "Server"), (0x10174410, "Ride")):
        S.internal(addr, R(f"CompleteGetList.{name}", "Li", then=complete_get_list_then))
    return S


def setup(runner):
    OI.setup(runner)
    OI.STUBS = build_tick_stubs()


class TickSide(OI.Side):
    """OI.Side with the unit's members as calls."""

    def call(self, op):
        if op < 12:
            return super().call(op)
        self.track = True
        e = self.e
        if op in TICK_ADDR:
            T.call_ppc(e, TICK_ADDR[op], [self.obj])
        elif op == 16:
            r3 = T.call_ppc(e, ARE_WE_LOGGED_IN, [self.obj])[0]
            self.L.rec("AreWeLoggedIn.result", (OI.s32(r3),))
        else:
            T.call_ppc(e, COMPLETE_LOGIN, [self.obj, ERRORS[op - COMPLETE_LOGIN_OP] & MASK])
        self.track = False


def run_ppc(e, case):
    side = TickSide(e, case)
    try:
        side.system(case["traced"], case["fill"])
        dumps = [side.dump()]
        for step in case["steps"]:
            if step[0] == "apply":
                side.apply(step[1])
            else:
                side.call(step[1])
                dumps.append(side.dump())
        return dict(dumps=dumps, log=side.L.log)
    finally:
        side.finish()


# ---------------------------------------------------------------------------------------------------- the cases

MONITORS = [(0, 0), (5, 10), (100, 100), (1, 3), (-5, 10), (10, -5), (0, -1), (50, 0), (0x02000000, 7), (0x7FFFFFFF, 2),
            (-0x80000000, 3), (-1, 1), (99, 100), (200, 100)]
NEW_CLOCK_BASE = 0


def make_case(op, spec, r, queues=None):
    """One script: the state, then the call. The clock queue starts with the constructor's reading."""
    q = {k: list(v) for k, v in (queues or {}).items()}
    q["LbTime_GetClock"] = [r.getrandbits(32)] + q.get("LbTime_GetClock", [r.getrandbits(32)])
    return dict(mode="system", traced=False, fill=0xA5, steps=[("apply", spec), ("call", op)], queues=q)


def conns_with(index, on=1, others=0):
    c = [others] * 6
    c[index] = on
    return c


def monitor_queues(name, mon, ids):
    """Queues of a connection: the ids its GetCurrentMessage answers (one per call), the words MonitorMessage gives."""
    return {f"{name}.GetCurrentMessage": list(ids), f"{name}.MonitorMessage.done": [mon[0]], f"{name}.MonitorMessage.total": [mon[1]]}


def unsigned(v):
    return v & MASK


# ---- TickNews: the id is matched against the three request words in order
def news_match(r, kind):
    """(ids queue, the request words overrides) for one kind of match."""
    x = r.randrange(1, MASK)
    y = (x + 1 + r.randrange(1000)) & MASK or 1
    others = {"mNewsRequest1": y, "mNewsRequest2": y ^ 0x55, "mNewsRequest3": y ^ 0xAA}
    if kind == 0:
        return [x], dict(others, mNewsRequest1=x)
    if kind == 1:
        return [x], dict(others, mNewsRequest2=x)
    if kind == 2:
        return [x], dict(others, mNewsRequest3=x)
    if kind == 3:
        return [x], others
    if kind == 4:
        return [0, x], dict(others, mNewsRequest2=x)
    if kind == 5:
        return [0, 0], dict(others, mNewsRequest1=0)
    if kind == 6:
        return [x], dict(others, mNewsRequest1=x, mNewsRequest2=x, mNewsRequest3=x)
    if kind == 7:
        return [0, x], others
    if kind == 8:
        return [MASK], dict(mNewsRequest1=MASK, mNewsRequest2=MASK, mNewsRequest3=MASK)
    return [0, 0], dict(mNewsRequest1=MASK, mNewsRequest2=1, mNewsRequest3=2)


def news_edges():
    r = random.Random(1201)
    for news in (0, 1):
        for finished in (0, 1, 0x100, MASK):
            for dones in itertools.product((0, 1, MASK), repeat=3):
                ids, req = news_match(r, r.randrange(10))
                ov = dict(req, mNewsFinished=finished, mNewsDone1=dones[0], mNewsDone2=dones[1], mNewsDone3=dones[2])
                yield make_case(12, OI.full_spec(r, conns_with(1, news, r.randrange(2)), [NO_RECV] * 9, ov), r,
                                monitor_queues("News", r.choice(MONITORS), ids))
    # every kind of match with every kind of progress
    for kind in range(10):
        for mon in MONITORS:
            ids, req = news_match(r, kind)
            ov = dict(req, mNewsFinished=0, mNewsDone1=0, mNewsDone2=r.choice([0, 1]), mNewsDone3=r.choice([0, MASK]))
            yield make_case(12, OI.full_spec(r, conns_with(1), [NO_RECV] * 9, ov), r, monitor_queues("News", mon, ids))


def news_random(r):
    ids, req = news_match(r, r.randrange(10))
    ov = dict(req)
    for name in ("mNewsFinished", "mNewsDone1", "mNewsDone2", "mNewsDone3"):
        ov[name] = r.choice([0, 0, 1, MASK, r.getrandbits(32)])
    return make_case(12, OI.full_spec(r, OI.random_conns(r), [NO_RECV] * 9, ov), r,
                     monitor_queues("News", r.choice(MONITORS) if r.randrange(3) else (r.getrandbits(32), r.getrandbits(32)), ids))


# ---- TickUpload and TickFTPDownload
def upload_edges():
    r = random.Random(1202)
    for upload in (0, 1):
        for active in (0, 1, MASK, 0x100):
            for done in (0, 1, MASK, 0x100):
                for failed in (0, 1, MASK, 0x80000000):
                    ids = r.choice([[r.randrange(1, MASK)], [0, r.randrange(1, MASK)], [0, 0]])
                    ov = dict(mUploadActive=active, mUploadDone=done, mUploadFailed=failed)
                    yield make_case(14, OI.full_spec(r, conns_with(3, upload, r.randrange(2)), [NO_RECV] * 9, ov), r,
                                    monitor_queues("Upload", r.choice(MONITORS), ids))
    for mon in MONITORS:
        for ids in ([5], [0, 6], [0, 0]):
            ov = dict(mUploadActive=1, mUploadDone=0)
            yield make_case(14, OI.full_spec(r, conns_with(3), [NO_RECV] * 9, ov), r, monitor_queues("Upload", mon, ids))


def upload_random(r):
    ov = {name: r.choice([0, 0, 1, MASK, r.getrandbits(32)]) for name in ("mUploadActive", "mUploadDone", "mUploadFailed")}
    ids = r.choice([[r.getrandbits(32)], [0, r.getrandbits(32)], [0, 0]])
    return make_case(14, OI.full_spec(r, OI.random_conns(r), [NO_RECV] * 9, ov), r,
                     monitor_queues("Upload", r.choice(MONITORS) if r.randrange(3) else (r.getrandbits(32), r.getrandbits(32)), ids))


def ftp_edges():
    r = random.Random(1203)
    for buffer_, client in itertools.product((0, 1), repeat=2):
        for active in (0, 1, MASK, 0x100):
            for done in (0, 1, MASK, 0x100):
                for result in (0, 1, MASK, 0x80000000):
                    conns = [r.randrange(2), r.randrange(2), r.randrange(2), r.randrange(2), buffer_, client]
                    ov = dict(mFTPActive=active, mFTPDone=done, mFTPResult=result)
                    yield make_case(15, OI.full_spec(r, conns, [NO_RECV] * 9, ov), r)
    for total, done in MONITORS + [(1, 0), (3, 1), (7, 0x02000000), (0x7FFFFFFF, 0x7FFFFFFF), (100, -1)]:
        ov = dict(mFTPActive=1, mFTPDone=0)
        yield make_case(15, OI.full_spec(r, [0, 0, 0, 0, 1, 1], [NO_RECV] * 9, ov, ftp=[total & MASK, done & MASK]), r)


def ftp_random(r):
    ov = {name: r.choice([0, 0, 1, MASK, r.getrandbits(32)]) for name in ("mFTPActive", "mFTPDone", "mFTPResult")}
    conns = OI.random_conns(r)
    if r.randrange(2):
        conns[4] = conns[5] = 1
    return make_case(15, OI.full_spec(r, conns, [NO_RECV] * 9, ov), r)


# ---- TickCity
def list_words(r, req_ids, f0c, f10):
    """The ten words of each of the nine lists: the request id, five random words, f0c at 0xc, f10 at 0x10, the rest random."""
    out = []
    for i in range(9):
        w = [r.choice([0, 1, MASK, r.getrandbits(32)]) for _ in range(10)]
        w[0] = req_ids[i]
        w[2] = f0c[i]
        w[3] = f10[i]
        out.append(w)
    return out


def distinct_ids(r):
    base = r.randrange(0x100, MASK - 100)
    ids = [base + k for k in range(9)]
    r.shuffle(ids)
    return ids


def city_case(r, wanted=1, city=1, f0c=None, f10=None, match=None, ids=None, mon=None, completes=(0,), clock_delta=None, request_ids=None,
              second=False, clock=None):
    """One TickCity case. match: the list (by index) whose request id the current message gets (None: none of them, 'byid': the
    by-park-id list's, which is not compared); ids: the queue of GetCurrentMessage answers (default: the matching id once)."""
    req = request_ids or distinct_ids(r)
    f0c = f0c or [r.choice([0, 1, MASK]) for _ in range(9)]
    f10 = f10 or [r.choice([0, 1, MASK]) for _ in range(9)]
    if match is None:
        current = r.randrange(1, MASK)
        while current in req:
            current = r.randrange(1, MASK)
    elif match == "byid":
        current = req[BYID]
    else:
        current = req[match]
    queue_ids = ids if ids is not None else ([current, current] if not second else [0, current])
    clock = r.getrandbits(32) if clock is None else clock
    ov = {"mCityListsWanted": wanted, "mCityClock": clock}
    delta = clock_delta if clock_delta is not None else r.choice([0, 1, 359999, 360000, 360001, 1000000, -1, -360001, 0x7FFFFFFF])
    q = monitor_queues("City", mon or r.choice(MONITORS), queue_ids)
    q["CompleteGetList.set"] = list(completes)
    q["LbTime_GetClock"] = [unsigned(clock + delta)]
    spec = OI.full_spec(r, conns_with(2, city, r.randrange(2)), [NO_RECV] * 9, ov, list_words=list_words(r, req, f0c, f10))
    return make_case(13, spec, r, q)


def city_edges():
    r = random.Random(1204)
    # no city connection; the wanted flag; each list alone pending in each of the four states of its two words
    for wanted in (0, 1, MASK, 0x100):
        yield city_case(r, wanted=wanted, city=0)
        yield city_case(r, wanted=wanted, f0c=[0] * 9, f10=[0] * 9)
    for state in itertools.product((0, 1), repeat=2):
        for i in range(9):
            f0c, f10 = [0] * 9, [0] * 9
            f0c[i], f10[i] = state
            for completes in ((0,), (1,), (MASK,)):
                yield city_case(r, f0c=f0c, f10=f10, completes=completes, wanted=r.randrange(2))
    # all lists in one state, the completion starting another request for every list or none
    for state in itertools.product((0, 1, MASK), repeat=2):
        for completes in ((0,), (1,), (0, 1), (1, 0, 0), (0, 0, 0, 0, 0, 0, 0, 0, 1)):
            yield city_case(r, f0c=[state[0]] * 9, f10=[state[1]] * 9, completes=completes)
    # the clock: nothing pending, the idle time around the limit (a signed compare on a wrapping difference)
    for delta in (0, 1, 359999, 360000, 360001, 360002, 1000000, 0x7FFFFFFF, 0x80000000, 0x80000001, -1, -360000, -360001, MASK):
        yield city_case(r, f0c=[0] * 9, f10=[r.choice([0, 1]) for _ in range(9)], clock_delta=delta, wanted=0)
    # the same with the clock readings chosen: the difference is the 32-bit wrapped one, compared signed
    for clock, delta in ((0x10, 0x80000000), (0x7FFFFFF0, 0x80000000), (0xF0000000, 0x20000000), (0xF0000000, 0x30000000),
                         (0xFFFFFFF0, 0x20), (0xFFFFFFF0, 360020), (0xFFFFFFF0, 360000 + 0xF), (0xFFFFFFF0, 360000 + 0x10), (0, 0x7FFFFFFF),
                         (0x80000000, 0x80000000), (0x80000000, 360001), (5, 0xFFFFFFFF), (0x7FFFFFFF, 360000), (0x7FFFFFFF, 360001)):
        yield city_case(r, f0c=[0] * 9, f10=[r.choice([0, 1]) for _ in range(9)], clock_delta=delta, clock=clock, wanted=0)
    # the match: each of the eight compared lists, the by-park-id list (not compared), none; the first message zero or not
    for match in list(range(9)) + [None, "byid"]:
        for second in (False, True):
            for mon in MONITORS:
                yield city_case(r, f0c=[1] * 9, f10=[0] * 9, match=None if match == BYID else match, second=second, mon=mon)
    # ids shared between lists: the first in the order wins
    for first, second in itertools.combinations([i for i, _ in CITY_MATCH], 2):
        req = distinct_ids(r)
        req[second] = req[first]
        yield city_case(r, f0c=[1] * 9, f10=[0] * 9, match=first, request_ids=req)
    yield city_case(r, f0c=[1] * 9, f10=[0] * 9, request_ids=[0] * 9, ids=[0, 0])
    yield city_case(r, f0c=[1] * 9, f10=[0] * 9, request_ids=[MASK] * 9, ids=[MASK])
    yield city_case(r, f0c=[1] * 9, f10=[0] * 9, request_ids=[MASK] * 9, ids=[0, MASK])
    # the by-player-name list's progress also goes to the park find panel
    for mon in MONITORS:
        yield city_case(r, f0c=[1] * 9, f10=[0] * 9, match=BYPLAYER, mon=mon)


def city_random(r):
    states = [(r.choice([0, 0, 1, MASK]), r.choice([0, 1, 1, MASK])) for _ in range(9)]
    f0c = [s[0] for s in states]
    if r.randrange(4) == 0:
        f0c = [0] * 9
    req = distinct_ids(r) if r.randrange(4) else [r.choice([0, MASK, 7, 8]) for _ in range(9)]
    match = r.choice([None] + list(range(9)))
    ids = r.choice([None, [0, req[match] if match is not None else 5], [0, 0], [r.getrandbits(32)]])
    return city_case(r, wanted=r.choice([0, 1, 1, MASK]), city=r.choice([0, 1, 1, 1]), f0c=f0c, f10=[s[1] for s in states],
                     match=match, request_ids=req, ids=ids, completes=[r.choice([0, 0, 1, MASK]) for _ in range(r.randrange(1, 10))],
                     mon=r.choice(MONITORS) if r.randrange(3) else (r.getrandbits(32), r.getrandbits(32)))


# ---- CompleteLogin and AreWeLoggedIn
def login_case(r, error_index, error_id, logged_in, login, callback, outbox, ctx=None, done=None):
    ov = {"mLoginErrorID": error_id, "mLoggedIn": logged_in, "mLoginCallback": callback,
          "mLoginContext": r.getrandbits(32) if ctx is None else ctx}
    if done is not None:
        ov["mLoginDone"] = done
    conns = conns_with(0, login, r.randrange(2))
    return make_case(COMPLETE_LOGIN_OP + error_index, OI.full_spec(r, conns, [NO_RECV] * 9, ov), r,
                     {"CreateOnlineOutboxPanel": [outbox]})


def login_edges():
    r = random.Random(1205)
    for error in range(len(ERRORS)):
        for error_id in (0, 5, MASK, 0x80000000):
            for logged_in in (0, 1, MASK):
                for login, callback, outbox in itertools.product((0, 1), (0, 1), (0, 1)):
                    if error >= 3 and (error_id, logged_in) != (5, 1) and r.randrange(4):
                        continue
                    yield login_case(r, error, error_id, logged_in, login, callback, outbox)
    for outbox in (0, 1, MASK, 5, 0x100):
        yield login_case(r, 0, 3, 1, 1, 1, outbox)
    for ctx in (0, 1, MASK, 0x12345678, 0x80000000):
        yield login_case(r, 1, 0, 1, 1, 1, 0, ctx=ctx)


def login_random(r):
    return login_case(r, r.randrange(len(ERRORS)) if r.randrange(2) else 0, r.choice([0, 0, 5, MASK, r.getrandbits(32)]),
                      r.choice([0, 1, 1, MASK, r.getrandbits(32)]), r.randrange(2), r.randrange(2), r.choice([0, 0, 1, MASK]))


def are_edges():
    r = random.Random(1206)
    for value in (0, 1, MASK, 0x80000000, 0x100, 0x7FFFFFFF, 0xFFFF0000, 2):
        yield make_case(16, OI.full_spec(r, OI.random_conns(r), [NO_RECV] * 9, {"mLoggedIn": value}), r)


def are_random(r):
    return make_case(16, OI.full_spec(r, OI.random_conns(r), [NO_RECV] * 9, {"mLoggedIn": r.choice([0, 1, MASK, r.getrandbits(32)])}), r)


TESTS = [
    OI.OnlineTest("CFSOnlineSystem::TickNews 0x1017232c", run_ppc, OI.run_host, edge=news_edges, rnd=news_random),
    OI.OnlineTest("CFSOnlineSystem::TickCity 0x10173afc", run_ppc, OI.run_host, edge=city_edges, rnd=city_random),
    OI.OnlineTest("CFSOnlineSystem::TickUpload 0x10175b00", run_ppc, OI.run_host, edge=upload_edges, rnd=upload_random),
    OI.OnlineTest("CFSOnlineSystem::TickFTPDownload 0x10176da4", run_ppc, OI.run_host, edge=ftp_edges, rnd=ftp_random),
    OI.OnlineTest("CFSOnlineSystem::CompleteLogin 0x10171c08", run_ppc, OI.run_host, edge=login_edges, rnd=login_random),
    OI.OnlineTest("CFSOnlineSystem::AreWeLoggedIn 0x1017178c", run_ppc, OI.run_host, edge=are_edges, rnd=are_random),
]
