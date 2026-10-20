"""w103: the advisor request's constructors and SetScore, CAdvisor::CancelMessage, PlayLastTutorialMessage and GetTypePlaying,
and CWorld::GetAdvisor, original code on one side and src/AdvisorRequest.cpp and src/World_GetAdvisor.cpp on the other.

Run for real on both sides: the request constructors, Valid, the queue's CanIQueueThis and the two slot scans (unit w100) and the
CTimeSig reads they make. Recording fakes at their real addresses: the scoring constructor and queries (GetScoreFor also records the
request under construction), CClock::ReadFixed (its answer comes from the case; the clock classes are tested in test_GameClock),
ADV_Slap and GAME_ASSERTMSG. The scoring object's word at +0x32c (the balance table's ContextHelpScore) is planted by the case.

The static scoring object is built once per process: TPW_AR_FIRST picks the function whose first case runs with the Mac's guard byte at
0 (ctor, the default: the six-argument constructor; cancel; play); every other case runs with the guard at 1.
"""
import ctypes as C
import os
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AdvisorRequest"
PROGRAM = "SimTheme Park"
SIZE = 0x16F0
MASK = 0xFFFFFFFF
SCORING, GUARD = 0x102B3AC8, 0x102B4100
SETTINGS, WORLD, CLOCK = 0x1038071C, 0x1037EC0C, 0x1037F048
TABLE, TABLE_ENTRIES = 0x1034CBFC, 0x2800
NONE = 0x160
FN = dict(cancel=0, play=1, playing=2, ctor=3, default=4, copy=5, setscore=6, valid=7, advisor=8)
ADDR = dict(cancel=0x1000B048, play=0x1000B1C4, playing=0x1000C760, ctor=0x1000DEB4, default=0x1000A0B4,
            copy=0x1000DE30, setscore=0x1000E8CC, valid=0x1000E894, advisor=0x1013EAB0,
            can=0x1000B3D4, first=0x1000B740, lowest=0x1000B7E8)
QUERY = dict(score=0x1001019C, num=0x1001055C, maximum=0x10010654,
             time=0x10010758, once=0x10010864, cycling=0x10010980,
             category=0x10010A74, killed=0x10010B6C, mode=0x10010CC8,
             tutorial=0x10010E78)
FIRST = os.environ.get("TPW_AR_FIRST", "ctor")
STUBS = None
SCORES = [-0x80000000, -2, -1, 0, 1, 0x7FFFFFFF]
TIMES = [0, 1, 0x7FFFFFFF, 0x80000000, 0xFFFFFFF0, MASK]

SEED = [100]


def request(message=17, score=0, valid=1, force=0, seed=1, overridden=None):
    """All named fields and both padding regions are distinct, so a partial copy is visible."""
    return [seed & 0xFFFF, (seed + 1) & 0xFFFF, (seed + 2) & MASK, (seed + 3) & MASK, message & MASK,
            score & MASK, valid, force, (seed + 4) & 0xFF if overridden is None else overridden, (seed + 5) & 0xFF]


def base(fn, **changes):
    c = dict(fn=fn, message=17, turn=400, tutorial_on=1, guard=1, tutscore=0, advthing=0,
             stamp=0, response=0, said=0, kills=0, start=1000, duration=500, playing=17, clock=1200,
             queue=[request(message=i, valid=0, seed=10 + i) for i in range(8)],
             last=request(message=17, seed=60), request=request(seed=77), other=request(message=5, seed=90),
             args=[17, 0, 0, 0, 0, 0], rows=[],
             q=dict(category=[0], tutorial=[0], time=[0], once=[0], killed=[0], maximum=[255], cycling=[1],
                    num=[3], score=[0], ReadFixed=[1200]))
    c.update(changes)
    c["q"]["ReadFixed"] = [c["clock"]]
    return c


def reply(c, **values):
    c["q"].update({key: [value] for key, value in values.items()})
    return c


def pack_request(r, order):
    return struct.pack(order + "HHIIIIBBBB", *r)


def object_bytes(c, order):
    b = bytearray(b"\xa5" * SIZE)
    for i, r in enumerate(c["queue"]):
        b[0x14 + 24 * i:0x14 + 24 * (i + 1)] = pack_request(r, order)
    for i in range(351):
        p = 0xE0 + 16 * i
        struct.pack_into(order + "II", b, p, i * 4, i)
        b[p + 8] = i & 0xFF
        struct.pack_into(order + "I", b, p + 12, i * 7)
    p = 0xE0 + c["message"] * 16
    struct.pack_into(order + "II", b, p, c["stamp"] & MASK, c["response"] & MASK)
    b[p + 8] = c["said"]
    struct.pack_into(order + "I", b, p + 12, c["kills"] & MASK)
    struct.pack_into(order + "II", b, 0xD8, c["start"] & MASK, c["duration"] & MASK)
    b[0x16D0:0x16E8] = pack_request(c["last"], order)
    struct.pack_into(order + "I", b, 0x16EC, c["playing"] & MASK)
    return bytes(b)


def dump_object(b, order):
    """Typed fields are numbers; unknown bytes and padding remain bytes."""
    out = [list(b[:0x14])]
    for i in range(8):
        out.append(list(struct.unpack_from(order + "HHIIIIBBBB", b, 0x14 + 24 * i)))
    out.append(list(b[0xD4:0xD8]))
    out.append(list(struct.unpack_from(order + "II", b, 0xD8)))
    for i in range(351):
        p = 0xE0 + 16 * i
        out.append(list(struct.unpack_from(order + "II", b, p)) + list(b[p + 8:p + 12])
                   + [struct.unpack_from(order + "I", b, p + 12)[0]])
    out.append(list(struct.unpack_from(order + "HHIIIIBBBB", b, 0x16D0)))
    out.append(list(b[0x16E8:0x16EC]))
    out.append(struct.unpack_from(order + "I", b, 0x16EC)[0])
    return out


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    s = CL.Stubs()
    s.real.update(ADDR.values())
    s.real.update([0x1015F0E8, 0x1015F124])
    for key, address in QUERY.items():
        if key == "score":
            continue
        spec = "Lii" if key == "killed" else "Li"
        s.internal(address, CL.recorder(key, spec, ret="i"))

    def score(e):
        # The request under construction is the one the case planted at the address of label 6.
        r, L = e.cl.request, e.cl
        L.rec("score", (L.lab(e.r(3)), T.s32(e.r(4)), e.read(r + 0x14, 1)[0], struct.unpack(">H", e.read(r, 2))[0],
                        struct.unpack(">I", e.read(r + 4, 4))[0], struct.unpack(">i", e.read(r + 0xC, 4))[0],
                        e.read(r + 0x16, 1)[0]))
        e.set_r(3, T.u32(L.ret("score")))

    def ctor(e):
        e.cl.rec("Scoring.Ctor", (e.cl.lab(e.r(3)),))

    def world(e):
        e.cl.rec("GetWorld", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.u32(WORLD))

    s.internal(QUERY["score"], score)
    s.internal(0x10010054, ctor)
    s.internal(0x1014205C, world)
    s.internal(0x1014733C, CL.recorder("ReadFixed", "L", ret="i"))
    s.internal(0x10008DD4, CL.recorder("ADV_Slap"))
    s.internal(0x1000DDE8, CL.recorder("GAME_ASSERTMSG", "bs"))
    # Runtime destructor registration is supplied by the host compiler.
    s.imp("__register_global_object", lambda e: None)
    STUBS = s


def ppc_run(e, c):
    saved = STUBS.activate(e)
    touched = []
    try:
        obj = e.alloc(SIZE, object_bytes(c, ">"))
        req = e.alloc(24, pack_request(c["request"], ">"))
        other = e.alloc(24, pack_request(c["other"], ">"))
        world = e.alloc(0x1DA748, b"\0" * 0x1DA748)
        things = [e.alloc(16, b"\0" * 16) for _ in range(4)]
        e.write(world + 0x1DA70C, struct.pack(">I", c["turn"] & MASK))
        e.write(world + 0x1DA71C, struct.pack(">H", c["advthing"]))
        e.write(WORLD, struct.pack(">I", world))
        e.write(SETTINGS + 0x35, bytes([c["tutorial_on"]]))
        e.write(GUARD, bytes([c["guard"]]))
        e.write(SCORING + 0x32C, struct.pack(">I", c["tutscore"] & MASK))
        for index, kind, word1 in c["rows"]:
            e.write(TABLE + 0x14 * index, struct.pack(">IIIII", things[kind - 1] if kind else 0, word1, 0, 0, 0))
            touched.append(index)
        fixed = {obj: 2, SCORING: 3, obj + 0xE0 + c["message"] * 16: 4, CLOCK: 5, req: 6, other: 7, world: 12}
        fixed.update({t: 8 + i for i, t in enumerate(things)})
        e.cl = CL.PpcLog(fixed, c["q"])
        e.cl.request = req
        fn, a = c["fn"], c["args"]
        if fn == "cancel":
            args = [obj, c["message"] & MASK]
        elif fn in ("play", "playing"):
            args = [obj]
        elif fn == "ctor":
            args = [req, a[0] & MASK, ((a[1] & 0xFFFF) << 16) | (c["junk"] & 0xFFFF), a[2] & MASK, a[3] & MASK, a[4], a[5] & MASK]
        elif fn == "default":
            args = [req]
        elif fn == "copy":
            args = [req, other]
        elif fn == "setscore":
            args = [req, a[0] & MASK]
        elif fn == "valid":
            args = [req]
        else:
            args = [world]
        r3 = T.call_ppc(e, ADDR[fn], args)[0]
        if fn == "valid":
            result = r3 & 0xFF
        elif fn == "advisor":
            result = e.cl.lab(r3)
        elif fn in ("cancel", "play", "playing"):
            result = T.s32(r3)
        else:
            result = 0
        return dict(result=result, log=e.cl.log, object=dump_object(e.read(obj, SIZE), ">"),
                    request=list(struct.unpack(">HHIIIIBBBB", e.read(req, 24))),
                    other=list(struct.unpack(">HHIIIIBBBB", e.read(other, 24))),
                    globals=[e.u32(world + 0x1DA70C), e.read(SETTINGS + 0x35, 1)[0], e.read(GUARD, 1)[0],
                             struct.unpack(">H", e.read(world + 0x1DA71C, 2))[0]])
    finally:
        for index in touched:
            e.write(TABLE + 0x14 * index, b"\0" * 20)
        CL.Stubs.deactivate(e, saved)
        e.cl = None


def host_run(lib, c):
    lib.tpw_ar_reset.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_uint32, C.c_int32, C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_ar_row.argtypes = [C.c_int32, C.c_int32, C.c_uint32]
    lib.tpw_ar_call.argtypes = [C.c_int32] + [C.c_int32] * 6
    lib.tpw_ar_call.restype = C.c_int32
    lib.tpw_ar_dump.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.POINTER(C.c_uint32)]
    CL.host_begin(lib, {}, c["q"])
    obj = C.create_string_buffer(object_bytes(c, "<"), SIZE)
    req = C.create_string_buffer(pack_request(c["request"], "<"), 24)
    other = C.create_string_buffer(pack_request(c["other"], "<"), 24)
    lib.tpw_ar_reset(obj, req, other, c["turn"], c["tutorial_on"], c["message"], T.s32(c["tutscore"]), c["advthing"])
    lib.tpw_ar_clear_rows()
    for index, kind, word1 in c["rows"]:
        lib.tpw_ar_row(index, kind, word1)
    fn, a = c["fn"], c["args"]
    if fn == "cancel":
        args = [c["message"]]
    elif fn == "ctor":
        args = [a[0], a[1] & 0xFFFF, T.s32(a[2]), T.s32(a[3]), a[4], T.s32(a[5])]
    elif fn == "setscore":
        args = [T.s32(a[0])]
    else:
        args = []
    result = lib.tpw_ar_call(FN[fn], *(args + [0] * (6 - len(args))))
    glob = (C.c_uint32 * 4)()
    lib.tpw_ar_dump(obj, req, other, glob)
    if fn in ("default", "copy", "setscore", "ctor"):
        result = 0
    return dict(result=result, log=CL.host_end(lib), object=dump_object(obj.raw, "<"),
                request=list(struct.unpack("<HHIIIIBBBB", req.raw)), other=list(struct.unpack("<HHIIIIBBBB", other.raw)),
                globals=list(glob))


def first_case(fn):
    """The case that makes the static scoring object (its guard byte is 0 on the Mac side)."""
    if fn == "ctor":
        c = ctor_case(17, 3, 0x12, 5, 1, -1, 0)
    elif fn == "cancel":
        c = cancel_case(count=0, tutorial=1, category=1, match=1, playing="idle")
    else:
        c = play_case(mask=0, tutscore=7)
    c["guard"] = 0
    return c


def first_edges(fn, own):
    if FIRST == fn:
        yield first_case(fn)
    yield from own()


# ----------------------------------------------------------------- the six-argument constructor and the small ones


def ctor_case(message, thing, secondary, response, override, score, reply_score, seed=None, junk=0x1234):
    SEED[0] += 7
    c = base("ctor", args=[message, thing, secondary, response, override, score],
             request=request(message=0xDEADBEEF, seed=seed or SEED[0], valid=0x55, overridden=0x66), junk=junk)
    return reply(c, score=reply_score)


def ctor_edges():
    for score in (-0x80000000, -2, -1, 0, 1, 5, 0x7FFFFFFF):
        for reply_score in (-1, 0, 123, 0x7FFFFFFF, -0x80000000):
            for override in (0, 1):
                yield ctor_case(17, 0x1234, 0x5678, 3, override, score, reply_score)
    for message in (0, 1, 350, 0x15F, 0x160, 0x7FFFFFFF, -1):
        for response in (-1, 0, 5, 0x7FFFFFFF, -0x80000000):
            yield ctor_case(message, 0x1111, MASK, response, 1, -1, 42)
            yield ctor_case(message, 0x2222, 0, response, 0, 9, 42)
    for thing in (0, 1, 0x7FFF, 0x8000, 0xFFFF):
        for junk in (0, 0xFFFF, 0x1234):
            for secondary in (0, 1, 0x80000000, MASK):
                yield ctor_case(33, thing, secondary, -1, 0, -1, 77, junk=junk)
    # A request that was valid, scored and flagged before: every field is rewritten.
    for valid in (0, 1, 255):
        c = ctor_case(40, 9, 8, 7, 1, 3, 0)
        c["request"] = request(message=1, score=2, valid=valid, force=3, seed=500, overridden=4)
        yield c


def ctor_random(r):
    SEED[0] += 1
    c = ctor_case(r.choice([r.randrange(351), T.s32(r.getrandbits(32))]), r.getrandbits(16), r.getrandbits(32),
                  T.s32(r.getrandbits(32)), r.randrange(2), r.choice(SCORES + [T.s32(r.getrandbits(32))]),
                  r.choice(SCORES), seed=r.getrandbits(32), junk=r.getrandbits(16))
    c["request"] = request(message=r.getrandbits(32), score=r.getrandbits(32), valid=r.choice([0, 1, 255]),
                           force=r.getrandbits(8), seed=r.getrandbits(32), overridden=r.getrandbits(8))
    return c


def default_case(valid, seed):
    return base("default", request=request(valid=valid, force=3, seed=seed, overridden=4))


def default_edges():
    for valid in range(256):
        yield default_case(valid, valid + 3)


def default_random(r):
    return base("default", request=request(message=r.getrandbits(32), score=r.getrandbits(32), valid=r.getrandbits(8),
                                           force=r.getrandbits(8), seed=r.getrandbits(32), overridden=r.getrandbits(8)))


def copy_case(source, dest):
    return base("copy", other=source, request=dest)


def copy_edges():
    for valid in (0, 1, 255):
        for force in (0, 1, 255):
            for overridden in (0, 1, 255):
                yield copy_case(request(message=350, score=-5, valid=valid, force=force, seed=300, overridden=overridden),
                                request(message=1, score=9, valid=1 - (valid != 0), force=7, seed=700, overridden=8))
    for score in SCORES:
        for message in (0, 17, 0x160):
            yield copy_case(request(message=message, score=score, seed=1234), request(seed=4321, valid=0, force=1))
    # The padding of the destination (+2..+3 and +0x17) must keep its own bytes (poisoned differently from the source's).
    yield copy_case(request(seed=0x1111), request(seed=0xEEEE))


def copy_random(r):
    def one():
        return request(message=r.getrandbits(32), score=r.getrandbits(32), valid=r.choice([0, 1, 255]),
                       force=r.getrandbits(8), seed=r.getrandbits(32), overridden=r.getrandbits(8))
    return copy_case(one(), one())


def setscore_case(score, overridden, valid=1):
    SEED[0] += 3
    return base("setscore", args=[score], request=request(valid=valid, score=0x1357, seed=SEED[0], overridden=overridden))


def setscore_edges():
    for score in SCORES:
        for overridden in (0, 1, 255):
            for valid in (0, 1):
                yield setscore_case(score, overridden, valid)


def setscore_random(r):
    c = setscore_case(r.choice(SCORES + [T.s32(r.getrandbits(32))]), r.getrandbits(8), r.getrandbits(8))
    c["request"] = request(message=r.getrandbits(32), score=r.getrandbits(32), valid=r.getrandbits(8),
                           force=r.getrandbits(8), seed=r.getrandbits(32), overridden=r.getrandbits(8))
    return c


def valid_edges():
    for valid in range(256):
        yield base("valid", request=request(valid=valid, seed=valid))


def valid_random(r):
    return base("valid", request=request(message=r.getrandbits(32), score=r.getrandbits(32), valid=r.getrandbits(8),
                                         force=r.getrandbits(8), seed=r.getrandbits(32), overridden=r.getrandbits(8)))


# ----------------------------------------------------------------- GetTypePlaying


def playing_case(start, duration, now, typeplaying=17):
    return base("playing", start=start, duration=duration, clock=now, playing=typeplaying)


def playing_edges():
    for start in (0, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFF0):
        for duration in TIMES:
            end = (start + duration) & MASK
            for now in (end - 1, end, end + 1, 0, MASK, 0x7FFFFFFF, 0x80000000, start):
                for typeplaying in (17, NONE):
                    yield playing_case(start, duration, now & MASK, typeplaying)


def playing_random(r):
    start, duration = r.choice(TIMES + [r.getrandbits(32)]), r.choice(TIMES + [r.getrandbits(32)])
    end = (start + duration) & MASK
    return playing_case(start, duration, r.choice([(end + d) & MASK for d in (-2, -1, 0, 1, 2)] + [r.getrandbits(32)]),
                        r.choice([0, 17, 350, NONE, r.getrandbits(32)]))


# ----------------------------------------------------------------- CancelMessage


def cancel_case(count=0, tutorial=0, category=0, match=0, playing="idle", message=17, mask=None, kills=3, said=1,
                lastvalid=1, other_messages=False):
    """count slots from 0 are valid and carry the message; mask (if given) says which slots are. tutorial: the last request is valid;
    match: it carries the message; playing: idle (the clock is at the end), same (the message is being said) or other."""
    SEED[0] += 11
    c = base("cancel", message=message, kills=kills, said=said)
    slots = []
    for i in range(8):
        on = (mask >> i) & 1 if mask is not None else int(i < count)
        # Slots that are not on carry the message too if they are invalid (they must stay as they are), else another one.
        msg = message if (on or (not other_messages)) else message + 1
        slots.append(request(message=msg, score=i, valid=(255 if (i & 1 and on) else on), seed=SEED[0] + i))
    c["queue"] = slots
    c["last"] = request(message=message if match else message + 1, score=44, valid=lastvalid if tutorial else 0, seed=SEED[0] + 50)
    c["start"], c["duration"], c["playing"] = 1000, 500, (message if playing == "same" else message ^ 1)
    c["clock"] = 1200 if playing in ("same", "other") else 1500
    return reply(c, category=category)


def cancel_edges():
    for count in range(9):
        for tutorial in (0, 1):
            for category in (0, 1, 2):
                for match in (0, 1):
                    for playing in ("idle", "same", "other"):
                        yield cancel_case(count, tutorial, category, match, playing)
    for mask in range(256):
        for playing in ("idle", "same"):
            yield cancel_case(mask=mask, tutorial=mask & 1, category=1, match=(mask >> 1) & 1, playing=playing,
                              other_messages=bool(mask & 0x80))
    for kills in (0, 1, -1, 0x7FFFFFFF, 0x80000000, MASK):
        for said in (0, 1, 255):
            for message in (0, 1, 349, 350):
                yield cancel_case(count=2, tutorial=1, category=1, match=1, playing="same", message=message, kills=kills, said=said)
    for lastvalid in (1, 255):
        yield cancel_case(count=1, tutorial=1, category=1, match=1, lastvalid=lastvalid)
    # The clock exactly at the end and one before, with the same type playing.
    for now in (1499, 1500, 1501):
        c = cancel_case(count=0, tutorial=0, playing="same")
        c["clock"] = now
        c["q"]["ReadFixed"] = [now]
        yield c


def cancel_random(r):
    message = r.randrange(351)
    c = cancel_case(count=0, tutorial=r.randrange(2), category=r.choice([0, 1, 1, 2]), match=r.randrange(2),
                    playing=r.choice(["idle", "same", "other"]), message=message, kills=r.getrandbits(32),
                    said=r.choice([0, 1, 255]), lastvalid=r.choice([1, 255]))
    c["queue"] = [request(message=r.choice([message, message, r.randrange(351)]), score=r.choice(SCORES), valid=r.choice([0, 1, 1, 255]),
                          force=r.getrandbits(8), seed=r.getrandbits(32), overridden=r.getrandbits(8)) for _ in range(8)]
    c["start"], c["duration"] = r.getrandbits(32), r.getrandbits(32)
    end = (c["start"] + c["duration"]) & MASK
    c["clock"] = r.choice([(end + d) & MASK for d in (-1, 0, 1)] + [r.getrandbits(32)])
    c["playing"] = r.choice([message, message, r.randrange(351), NONE])
    if c["playing"] == NONE:
        c["playing"] = message ^ 1
    c["q"]["ReadFixed"] = [c["clock"]]
    return c


# ----------------------------------------------------------------- PlayLastTutorialMessage


def play_case(mask=0, scores=None, tutscore=7, lastvalid=1, message=17, last_score=3, tutorial_on=1, tutorial=0, maximum=255, cycling=1,
              stamp=0, minimum=0, turn=400, dup=0, num=3, response=0, category=1, force_bit=0, once=0, killed=0, said=0, kills=0):
    """mask: which queue slots are valid. scores: their scores (default the slot number). dup: how many valid slots carry the message."""
    SEED[0] += 13
    c = base("play", message=message, tutscore=tutscore, tutorial_on=tutorial_on, stamp=stamp, turn=turn, response=response,
             said=said, kills=kills)
    scores = scores or list(range(8))
    c["queue"] = [request(message=message if i < dup else 100 + i, score=scores[i], valid=(255 if i & 1 else 1) if (mask >> i) & 1 else 0,
                          force=i & 1, seed=SEED[0] + i) for i in range(8)]
    c["last"] = request(message=message, score=last_score, valid=lastvalid, force=force_bit, seed=SEED[0] + 40, overridden=0)
    return reply(c, category=category, tutorial=tutorial, maximum=maximum, cycling=cycling, time=minimum, num=num, once=once,
                 killed=killed)


def play_edges():
    yield play_case(lastvalid=0)
    yield play_case(lastvalid=0, mask=255)
    for mask in range(256):
        yield play_case(mask=mask, tutscore=100 if mask != 255 else 3)
    # A full queue against the new score (the balance table's word): lower, equal and higher scores in the lowest slot.
    for tut in SCORES:
        for low in SCORES:
            scores = [0x7FFFFFFF - i for i in range(8)]
            scores[3] = low
            yield play_case(mask=255, scores=scores, tutscore=tut)
            scores = [low] * 8
            yield play_case(mask=255, scores=scores, tutscore=tut)
    # Admission refused by each gate that still applies (the force bit is always set): tutorial, time, duplicates, cycling.
    for tutorial_on in (0, 1):
        for tutorial in (0, 1):
            yield play_case(mask=3, tutorial_on=tutorial_on, tutorial=tutorial)
    for stamp, turn, minimum in ((4, 400, 100), (4, 400, 99), (4, 400, 98), (4, 404, 1), (400, 4, 5), (0, 400, 5), (MASK, 3, 1)):
        yield play_case(mask=3, stamp=stamp, turn=turn, minimum=minimum)
    for dup in range(9):
        for maximum in (0, 1, 2, 7, 8, 9, 255, 256):
            yield play_case(mask=(1 << dup) - 1 if dup else 0, dup=dup, maximum=maximum)
    for cycling in (-1, 0, 1):
        for num in (0, 1, 3):
            for response in (num - 1, num, 0):
                yield play_case(mask=1, cycling=cycling, num=num, response=T.s32(response))
    for category in (0, 1, 2):
        yield play_case(mask=5, category=category)
    for force_bit in (0, 1, 7):
        yield play_case(mask=5, force_bit=force_bit)
    # The request is forced (mOverrideOnlyOnce set to 1 first): "say only once" and the kill count never refuse it, whatever the
    # request carried before and whatever the message state holds.
    for force_bit in (0, 1):
        for once in (0, 1):
            for killed in (0, 1):
                for said in (0, 1):
                    yield play_case(mask=1, force_bit=force_bit, once=once, killed=killed, said=said, kills=5)
    for message in (0, 1, 349, 350):
        yield play_case(mask=1, message=message)


def play_random(r):
    message = r.randrange(351)
    c = play_case(mask=r.getrandbits(8), message=message, tutscore=r.choice(SCORES + [T.s32(r.getrandbits(32))]),
                  lastvalid=r.choice([0, 1, 1, 1, 255]), last_score=r.choice(SCORES), tutorial_on=r.randrange(2),
                  tutorial=r.randrange(2), maximum=r.choice([0, 1, 2, 8, 255, 256]), cycling=r.choice([-1, 0, 1]),
                  stamp=r.choice([0, 4, r.getrandbits(32)]), minimum=r.choice([0, 1, 100, r.getrandbits(32)]),
                  turn=r.getrandbits(32), dup=r.randrange(9), num=r.choice([0, 1, 3]),
                  response=r.choice([-1, 0, 1, 2]), category=r.choice([0, 1, 2]), force_bit=r.getrandbits(8),
                  once=r.randrange(2), killed=r.randrange(2), said=r.choice([0, 1, 255]), kills=r.getrandbits(32))
    c["queue"] = [request(message=r.choice([message, r.randrange(351)]), score=r.choice(SCORES), valid=r.choice([0, 1, 255]),
                          force=r.getrandbits(8), seed=r.getrandbits(32), overridden=r.getrandbits(8)) for _ in range(8)]
    return c


# ----------------------------------------------------------------- GetAdvisor


def advisor_case(advthing, rows):
    return base("advisor", advthing=advthing, rows=rows)


def advisor_edges():
    for index in (0, 1, 2, 7, 8, 0x100, 0x1234, 0x27FF):
        for kind in (0, 1, 2, 3, 4):
            for row0 in (None, 1, 4):
                rows = [(index, kind, 0xAAAA0000 + index)] if index else []
                if row0 is not None:
                    rows = [r for r in rows if r[0] != 0] + [(0, row0, 0x55)]
                yield advisor_case(index, rows)
    # Neighbouring rows are not mistaken for the advisor's (the stride is five words).
    yield advisor_case(7, [(6, 1, 1), (7, 2, 2), (8, 3, 3)])
    yield advisor_case(1, [(1, 0, 9), (0, 0, 0)])


def advisor_random(r):
    index = r.choice([0, 1, r.randrange(TABLE_ENTRIES)])
    rows = [(index, r.randrange(5), r.getrandbits(32))] if index else []
    if r.randrange(4) == 0:
        rows.append((0, r.randrange(1, 5), r.getrandbits(32)))
    for _ in range(r.randrange(3)):
        extra = r.randrange(1, TABLE_ENTRIES)
        if extra != index:
            rows.append((extra, r.randrange(5), r.getrandbits(32)))
    return advisor_case(index, rows)


def test(name, fn, edges, rnd):
    return FnTest(name, ppc_run, host_run, edge=lambda: list(first_edges(fn, edges)) if fn in ("ctor", "cancel", "play") else list(edges()),
                  rnd=rnd)


CTOR = test("CAdvisorRequest::CAdvisorRequest(AdvisorMessage, tag_IDX_TYPE, unsigned long, int, bool, int)", "ctor", ctor_edges, ctor_random)
CANCEL = test("CAdvisor::CancelMessage", "cancel", cancel_edges, cancel_random)
PLAY = test("CAdvisor::PlayLastTutorialMessage", "play", play_edges, play_random)
FIRSTS = dict(ctor=CTOR, cancel=CANCEL, play=PLAY)
TESTS = [FIRSTS[FIRST]] + [t for t in (CTOR, CANCEL, PLAY) if t is not FIRSTS[FIRST]] + [
    test("CAdvisor::GetTypePlaying", "playing", playing_edges, playing_random),
    test("CAdvisorRequest::CAdvisorRequest()", "default", default_edges, default_random),
    test("CAdvisorRequest::CAdvisorRequest(const CAdvisorRequest&)", "copy", copy_edges, copy_random),
    test("CAdvisorRequest::SetScore", "setscore", setscore_edges, setscore_random),
    test("CAdvisorRequest::Valid", "valid", valid_edges, valid_random),
    test("CWorld::GetAdvisor", "advisor", advisor_edges, advisor_random),
]
