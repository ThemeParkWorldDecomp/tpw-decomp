"""w100: original queue code, Valid and TimeSig run on both sides.

Scoring construction and all ten queries are recording fakes at their real addresses.
GetWorld is a recording fake returning gWorld. The static's first construction is
tested by putting its first call first, once per process (TPW_AQ_FIRST selects the
reachable first-use site). Later cases have guard 1; no host guard is reset.
The whole CAdvisor, including every state entry and padding byte, input request,
world turn, tutorial setting, guard and ordered fake arguments are compared.
"""
import ctypes as C
import os
import struct

import calllog as CL
import tpwtest as T
from tpwtest import FnTest

SHIM_LIBRARY = "unit_AdvisorQueue"
PROGRAM = "SimTheme Park"
SIZE = 0x16F0
MASK = 0xFFFFFFFF
SCORING, GUARD = 0x102B3AC8, 0x102B4100
SETTINGS, WORLD = 0x1038071C, 0x1037EC0C
FN = dict(queue=0, can=1, first=2, lowest=3)
ADDR = dict(queue=0x1000AE48, can=0x1000B3D4, first=0x1000B740, lowest=0x1000B7E8)
QUERY = dict(score=0x1001019C, num=0x1001055C, maximum=0x10010654,
             time=0x10010758, once=0x10010864, cycling=0x10010980,
             category=0x10010A74, killed=0x10010B6C, mode=0x10010CC8,
             tutorial=0x10010E78)
FIRST = os.environ.get("TPW_AQ_FIRST", "queue")
STUBS = None
SCORES = [-0x80000000, -2, -1, 0, 1, 0x7FFFFFFF]


def request(message=17, score=0, valid=1, force=0, seed=1):
    """All named fields and padding are distinct, so a partial copy is visible."""
    return [seed & 0xFFFF, (seed + 1) & 0xFFFF, (seed + 2) & MASK,
            (seed + 3) & MASK, message, score & MASK, valid, force,
            (seed + 4) & 0xFF, (seed + 5) & 0xFF]


def base(fn, **changes):
    c = dict(fn=fn, message=17, force=0, turn=400, tutorial_on=1, guard=1,
             stamp=0, response=0, said=0, kills=0,
             queue=[request(message=i, valid=0, seed=10 + i) for i in range(8)],
             request=request(), q=dict(category=[0], tutorial=[0], time=[0],
                                       once=[0], killed=[0], maximum=[255],
                                       cycling=[1], num=[3]))
    c.update(changes)
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
    b[0x16D0:0x16E8] = pack_request(request(message=350, seed=999), order)
    return bytes(b)


def dump_object(b, order):
    """Typed fields are numbers; unknown bytes and padding remain bytes."""
    out = [list(b[:0x14])]
    for i in range(8):
        out.append(list(struct.unpack_from(order + "HHIIIIBBBB", b, 0x14 + 24 * i)))
    out.append(list(b[0xD4:0xE0]))
    for i in range(351):
        p = 0xE0 + 16 * i
        out.append(list(struct.unpack_from(order + "II", b, p)) + list(b[p + 8:p + 12])
                   + [struct.unpack_from(order + "I", b, p + 12)[0]])
    out.append(list(struct.unpack_from(order + "HHIIIIBBBB", b, 0x16D0)))
    out.append(list(b[0x16E8:]))
    return out


def setup(runner):
    global STUBS
    T.install_std_stubs(runner.emu)
    s = CL.Stubs()
    s.real.update(ADDR.values())
    s.real.update([0x1000E894, 0x1015F0E8, 0x1015F124])
    for key, address in QUERY.items():
        spec = "Lii" if key == "killed" else "Li"
        s.internal(address, CL.recorder(key, spec, ret="i"))

    def ctor(e):
        e.cl.rec("Scoring.Ctor", (e.cl.lab(e.r(3)),))

    def world(e):
        e.cl.rec("GetWorld", (e.cl.lab(e.r(3)),))
        e.set_r(3, e.u32(WORLD))

    s.internal(0x10010054, ctor)
    s.internal(0x1014205C, world)
    # Runtime destructor registration is supplied by the host compiler.
    s.imp("__register_global_object", lambda e: None)
    STUBS = s


def ppc_run(e, c):
    saved = STUBS.activate(e)
    try:
        obj = e.alloc(SIZE, object_bytes(c, ">"))
        req = e.alloc(24, pack_request(c["request"], ">"))
        world = e.alloc(0x1DA748, b"\0" * 0x1DA748)
        e.write(world + 0x1DA70C, struct.pack(">I", c["turn"] & MASK))
        e.write(WORLD, struct.pack(">I", world))
        e.write(SETTINGS + 0x35, bytes([c["tutorial_on"]]))
        e.write(GUARD, bytes([c["guard"]]))
        e.cl = CL.PpcLog({obj: 2, SCORING: 3, obj + 0xE0 + c["message"] * 16: 4}, c["q"])
        args = [obj]
        if c["fn"] == "queue":
            args.append(req)
        elif c["fn"] == "can":
            args += [c["message"], c["force"]]
        result = T.s32(T.call_ppc(e, ADDR[c["fn"]], args)[0])
        return dict(result=result, log=e.cl.log, object=dump_object(e.read(obj, SIZE), ">"),
                    request=list(struct.unpack(">HHIIIIBBBB", e.read(req, 24))),
                    globals=[e.u32(world + 0x1DA70C), e.read(SETTINGS + 0x35, 1)[0], e.read(GUARD, 1)[0]])
    finally:
        CL.Stubs.deactivate(e, saved)
        e.cl = None


def host_run(lib, c):
    lib.tpw_aq_reset.argtypes = [C.c_void_p, C.c_void_p, C.c_uint32, C.c_int32, C.c_int32]
    lib.tpw_aq_call.argtypes = [C.c_int32, C.c_int32, C.c_int32]
    lib.tpw_aq_call.restype = C.c_int32
    lib.tpw_aq_dump.argtypes = [C.c_void_p, C.c_void_p, C.POINTER(C.c_uint32)]
    CL.host_begin(lib, {}, c["q"])
    obj = C.create_string_buffer(object_bytes(c, "<"), SIZE)
    req = C.create_string_buffer(pack_request(c["request"], "<"), 24)
    lib.tpw_aq_reset(obj, req, c["turn"], c["tutorial_on"], c["message"])
    result = lib.tpw_aq_call(FN[c["fn"]], c["message"], c["force"])
    glob = (C.c_uint32 * 3)()
    lib.tpw_aq_dump(obj, req, glob)
    return dict(result=result, log=CL.host_end(lib), object=dump_object(obj.raw, "<"),
                request=list(struct.unpack("<HHIIIIBBBB", req.raw)), globals=list(glob))


def first_case():
    """These are all reachable sites of initial construction in this unit."""
    c = base("can", guard=0, force=1)
    if FIRST == "tutorial":
        c["tutorial_on"] = 0
    elif FIRST == "time":
        c["stamp"] = 4
    elif FIRST == "once":
        c["force"] = 0
    elif FIRST == "queue":
        c["fn"] = "queue"
    return c


def queue_edges():
    if FIRST == "queue":
        yield first_case()
    for n in range(9):
        for category in (0, 1, 2):
            c = reply(base("queue"), category=category)
            c["queue"] = [request(message=i, score=i - 3, valid=int(i < n), seed=40 + i) for i in range(8)]
            yield c
    for valid in (0, 1, 255):
        for old in SCORES:
            for new in SCORES:
                for force in (0, 1):
                    c = base("queue")
                    c["queue"] = [request(message=i, score=old, seed=i + 20) for i in range(8)]
                    c["request"] = request(score=new, valid=valid, force=force, seed=12345)
                    yield c
    # Category copy precedes tutorial, duplicate, once, kill and time refusals.
    for reason in ("tutorial", "maximum", "once", "killed", "time"):
        c = reply(base("queue", tutorial_on=0, stamp=4, said=255), category=1)
        reply(c, **{reason: 1 if reason != "maximum" else 0})
        if reason == "time":
            reply(c, time=100)
        yield c
    # Request aliases the chosen slot or tutorial copy are left for integration review.


def can_edges():
    if FIRST != "queue":
        yield first_case()
    for on in (0, 1):
        for tutorial in (0, 1):
            for force in (0, 1):
                yield reply(base("can", tutorial_on=on, force=force), tutorial=tutorial)
    for stamp in (0, 1, 3, 4, 400, 404, MASK):
        for turn in (0, 3, 4, 396, 400, 404, MASK):
            for minimum in (0, 1, 2, 0x7FFFFFFF, 0x80000000, MASK):
                yield reply(base("can", stamp=stamp, turn=turn), time=minimum)
    for force in (0, 1):
        for once in (0, 1):
            for said in (0, 1, 255):
                for killed in (0, 1):
                    for kills in (-0x80000000, -1, 0, 1, 0x7FFFFFFF):
                        yield reply(base("can", force=force, said=said, kills=kills), once=once, killed=killed)
    for n in range(9):
        for maximum in (0, 1, 2, 7, 8, 9, 255, 256, 257, 511, MASK):
            c = reply(base("can"), maximum=maximum)
            c["queue"] = [request(message=17, valid=int(i < n), seed=i + 10) for i in range(8)]
            yield c
    for cycling in (-1, 0, 1):
        for num in (-0x80000000, -1, 0, 1, 3, 0x7FFFFFFF):
            for response in (T.s32(num - 1), num, -1, 0):
                yield reply(base("can", response=response), cycling=cycling, num=num)
    for message in (0, 1, 349, 350):
        yield base("can", message=message)


def scan_edges(fn):
    for mask in range(256):
        c = base(fn)
        c["queue"] = [request(message=i, score=SCORES[i % len(SCORES)],
                              valid=(255 if i % 2 else 1) if mask & (1 << i) else 0,
                              seed=i + 80) for i in range(8)]
        yield c
    for score in SCORES:
        c = base(fn)
        c["queue"] = [request(message=i, score=score, seed=i + 80) for i in range(8)]
        yield c


def random_case(r, fn):
    c = base(fn, message=r.randrange(351), force=r.randrange(2),
             turn=r.getrandbits(32), tutorial_on=r.randrange(2),
             stamp=r.choice([0, 1, 3, 4, r.getrandbits(32)]),
             response=r.choice([-1, 0, 1, 2, r.getrandbits(32)]),
             said=r.choice([0, 1, 255]), kills=r.getrandbits(32))
    c["queue"] = [request(message=r.choice([c["message"], r.randrange(351)]),
                          score=r.choice(SCORES + [T.s32(r.getrandbits(32))]),
                          valid=r.choice([0, 1, 255]), seed=r.getrandbits(32)) for _ in range(8)]
    c["request"] = request(message=c["message"], score=r.choice(SCORES),
                           valid=r.choice([0, 1, 255]), force=c["force"], seed=r.getrandbits(32))
    reply(c, category=r.choice([0, 1, 2]), tutorial=r.randrange(2),
          time=r.choice([0, 1, MASK, r.getrandbits(32)]), once=r.randrange(2),
          killed=r.randrange(2), maximum=r.choice([0, 1, 8, 255, 256, 257, 511, MASK]),
          cycling=r.choice([-1, 0, 1]), num=r.choice([-1, 0, 1, 3, 0x7FFFFFFF]))
    return c


def test(fn, edges):
    return FnTest("CAdvisor::" + dict(queue="QueueRequest", can="CanIQueueThis",
                  first="FindFirstInvalidSlotInQueue", lowest="FindLowestScoringSlotInQueue")[fn],
                  ppc_run, host_run, edge=lambda: list(edges()), rnd=lambda r: random_case(r, fn))


QUEUE_TEST = test("queue", queue_edges)
CAN_TEST = test("can", can_edges)
TESTS = ([QUEUE_TEST, CAN_TEST] if FIRST == "queue" else [CAN_TEST, QUEUE_TEST]) + [
    test("first", lambda: scan_edges("first")), test("lowest", lambda: scan_edges("lowest"))]
