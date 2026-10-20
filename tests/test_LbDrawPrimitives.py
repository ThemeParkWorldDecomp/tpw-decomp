"""w242: remaining Bullfrog LbDraw front-end and sprite helpers."""
import ctypes as C
import random
import struct

import tpwtest as T
from tpwtest import FnTest

PROGRAM = "bullfrog symbols shared"
SHIM_LIBRARY = "unit_LbDrawPrimitives"
MASK = 0xFFFFFFFF

COMPACT = 0x1001A474
REMAP = 0x1001AAC4
RELEASE_VIEW, RELEASE_CLIP, GET_VIEW = 0x1002B478, 0x1002B4CC, 0x1002B520
SCALED, SCALE_DATA = 0x100439E4, 0x10043A5C
CURRENT, FLAGS, GHOST, FADE, FADEROW = 0x10074880, 0x10074834, 0x10074830, 0x1007482C, 0x10074828
BASE, ORIGIN, CLIP, VIEW, SCALE_STATE = 0x10074858, 0x1007485C, 0x10074860, 0x10074870, 0x10074884

WRAPPERS = [
    ("LbDraw_Pixel",0x1002A270,0,0),
    ("LbDraw_Rectangle(pointer)",0x1002A2EC,1,1),
    ("LbDraw_Rectangle(reference)",0x1002A378,2,1),
    ("LbDraw_RectangleOutline(pointer)",0x1002A448,3,2),
    ("LbDraw_RectangleOutline(reference)",0x1002A4DC,4,2),
    ("LbDraw_Line",0x1002A5B4,5,3),
    ("LbDraw_HorizontalLine",0x1002A69C,6,4),
    ("LbDraw_VerticalLine",0x1002A784,7,5),
    ("LbDraw_Circle",0x1002A86C,8,6),
    ("LbDraw_CircleOutline",0x1002A954,9,7),
    ("LbDraw_CircleFilled",0x1002AA44,10,8),
    ("LbDraw_Triangle",0x1002AD2C,11,9),
    ("LbDraw_ClearClipRect",0x1002AECC,12,10),
    ("LbDraw_RectangleFilled(pointer)",0x1002AF44,13,11),
    ("LbDraw_RectangleFilled(reference)",0x1002AFD8,14,11),
    ("LbDraw_PropText",0x1002B0B0,15,12),
    ("LbDraw_UnicodePropText",0x1002B134,16,13),
    ("LbDraw_Sprite",0x1001B630,17,14),
    ("LbDraw_OneColourSprite",0x1001AA30,18,15),
]
VTABLE_OFFSETS = [0x08, 0x0c, 0x10, 0x24, 0x28, 0x2c, 0x18, 0x1c,
                  0x20, 0x30, 0x34, 0x14, 0x38, 0x3c, 0x40, 0x44]


class Area(C.Structure):
    _fields_ = [("masks", C.c_uint8 * 16), ("depth", C.c_int32),
                ("pal", C.c_int32), ("data", C.c_void_p), ("width", C.c_int32),
                ("height", C.c_int32), ("stride", C.c_int32)]


def bind(lib):
    if getattr(lib,"ldp_bound",False): return
    VP=C.c_void_p
    lib.tpw_ldp_reset.argtypes=[]
    lib.tpw_ldp_event_count.restype=C.c_uint32
    lib.tpw_ldp_last_op.restype=C.c_int32
    lib.tpw_ldp_set_state.argtypes=[C.c_int32,VP,VP,VP,C.c_uint32,VP]
    lib.tpw_ldp_compact.argtypes=[VP,VP,C.c_uint32]
    lib.tpw_ldp_remap.argtypes=[C.c_int32,C.c_int32,VP,C.c_uint16,C.c_uint16,VP]
    lib.tpw_ldp_draw.argtypes=[C.c_int32,VP,VP,VP,VP]
    lib.tpw_ldp_release.argtypes=[C.c_int32]
    lib.tpw_ldp_view.restype=VP
    lib.tpw_ldp_scale.argtypes=[C.c_int32,C.c_int32,C.c_int32,VP,C.c_uint16,C.c_uint16,C.c_uint32,C.c_uint32]
    lib.tpw_ldp_scale_state.restype=VP
    lib.ldp_bound=True


def ppc_compact(e,c):
    d=e.alloc(c["count"]*3+16,b"\xCD"*(c["count"]*3+16)); p=e.alloc(1024,c["palette"])
    T.call_ppc(e,COMPACT,[d+8,p,c["count"]])
    return {"bytes":e.read(d,c["count"]*3+16)}


def host_compact(lib,c):
    bind(lib); d=C.create_string_buffer(b"\xCD"*(c["count"]*3+16),c["count"]*3+16); p=C.create_string_buffer(c["palette"],1024)
    lib.tpw_ldp_compact(C.addressof(d)+8,p,c["count"])
    return {"bytes":d.raw}


def compact_case(r): return {"count":r.randrange(257),"palette":r.randbytes(1024)}


def sprite_stream(r,w,h):
    out=bytearray()
    for _ in range(h):
        x=0
        while x<w:
            n=min(w-x,r.randrange(1,8))
            if r.random()<.35: out.append((-n)&255)
            else: out.append(n); out.extend(r.randbytes(n))
            x+=n
        out.append(0)
    return bytes(out)


def remap_case(r):
    w,h=r.randrange(1,30),r.randrange(1,20)
    return {"seed":r.getrandbits(32),"size":(48,36),"sprite":(w,h,sprite_stream(r,w,h)),
            "xy":(r.randrange(-20,55),r.randrange(-15,42)),
            "clip":[r.randrange(0,8),r.randrange(0,8),r.randrange(35,49),r.randrange(27,37)],
            "flags":r.choice([0,1,2,3,8,9,10,11,0x10,0x11,0x12,0x13,0x18,0x19,0x1a,0x1b]),
            "map":r.randbytes(256),"ghost":r.randbytes(65536)}


def ppc_remap(e,c):
    r=random.Random(c["seed"]); width,height=c["size"]; stride=width+7
    pixels=e.alloc(stride*height+64,r.randbytes(stride*height+64)); data=e.alloc(len(c["sprite"][2]),c["sprite"][2])
    sprite=e.alloc(8,struct.pack(">IHH",data,c["sprite"][0],c["sprite"][1])); remap=e.alloc(256,c["map"]); ghost=e.alloc(65536,c["ghost"])
    area=e.alloc(40,bytes(40)); e.write(area+16,struct.pack(">iiIiii",8,1,pixels+32,width,height,stride))
    e.write(CURRENT,struct.pack(">I",area));e.write(ORIGIN,struct.pack(">I",pixels+32));e.write(CLIP,struct.pack(">4i",*c["clip"]));e.write(FLAGS,struct.pack(">I",c["flags"]));e.write(GHOST,struct.pack(">I",ghost))
    T.call_ppc(e,REMAP,[c["xy"][0]&MASK,c["xy"][1]&MASK,sprite,remap])
    return {"pixels":e.read(pixels,stride*height+64)}


def host_remap(lib,c):
    bind(lib);r=random.Random(c["seed"]);width,height=c["size"];stride=width+7
    pixels=C.create_string_buffer(r.randbytes(stride*height+64),stride*height+64); area=Area();area.depth=8;area.pal=1;area.data=C.addressof(pixels)+32;area.width=width;area.height=height;area.stride=stride
    clip=(C.c_int32*4)(*c["clip"]);ghost=C.create_string_buffer(c["ghost"],65536);data=C.create_string_buffer(c["sprite"][2]);remap=C.create_string_buffer(c["map"],256)
    lib.tpw_ldp_set_state(8,C.byref(area),clip,C.addressof(pixels)+32,c["flags"],ghost)
    lib.tpw_ldp_remap(c["xy"][0],c["xy"][1],data,c["sprite"][0],c["sprite"][1],remap)
    return {"pixels":pixels.raw}


def wrapper_case(r):
    return {"depth":r.choice([8,16,24,32]),"v":[r.randrange(-40,70) for _ in range(6)],
            "rect":[r.randrange(-30,65) for _ in range(4)],"clip":[2,3,44,35],
            "colour":r.randbytes(5),"null":r.random()<.15}


def colour_words(raw): return [int.from_bytes(raw[:4],"big"),raw[4]<<24]


def ppc_wrapper(e,c,entry,op,slot):
    calls=[]
    def fake(em): calls.append(1)
    vtable=e.alloc(0x48,bytes(0x48)); e.write(vtable+VTABLE_OFFSETS[slot],struct.pack(">I",T.fake_function(e,fake)))
    obj=e.alloc(4,struct.pack(">I",vtable));e.write(BASE,struct.pack(">I",obj));e.write(CLIP,struct.pack(">4i",*c["clip"]))
    rect=e.alloc(16,struct.pack(">4i",*c["rect"])); text=e.alloc(8,b"hello\0\0\0"); sprite=e.alloc(8,struct.pack(">IHH",text,3,2)); cw=colour_words(c["colour"]);v=c["v"]
    if op==0:args=[v[0],v[1]]+cw
    elif op in (1,3,13):args=[0 if c["null"] else rect]+cw
    elif op in (2,4,14):args=[rect]+cw
    elif op==5:args=v[:4]+cw
    elif op in (6,7,8,9,10):args=v[:3]+cw
    elif op==11:args=v[:6]+cw
    elif op==12:args=cw
    elif op==15:args=[v[0],v[1],text]+cw
    elif op==16:args=[v[0],v[1],text]+cw
    elif op==17:args=[v[0],v[1],sprite]
    else:args=[v[0],v[1],sprite]+cw
    T.call_ppc(e,entry,[a&MASK if isinstance(a,int) else a for a in args])
    return {"calls":len(calls),"op":slot if calls else -1}


def host_wrapper(lib,c,op,slot):
    bind(lib);lib.tpw_ldp_reset();area=Area();clip=(C.c_int32*4)(*c["clip"]);lib.tpw_ldp_set_state(c["depth"],C.byref(area),clip,None,0,None)
    vals=(C.c_int32*6)(*c["v"]);rect=(C.c_int32*4)(*c["rect"]);colour=C.create_string_buffer(c["colour"],5);text=C.create_string_buffer(b"hello\0\0\0")
    rp=None if c["null"] and op in (1,3,13) else rect
    lib.tpw_ldp_draw(op,vals,rp,text,colour)
    n=lib.tpw_ldp_event_count();return {"calls":n,"op":lib.tpw_ldp_last_op()%20 if n else -1}


def ppc_release(e,c,op):
    calls=[]
    target=0x1002B1C0 if op==0 else 0x1002B2C4
    T.stub_internal(e,target,lambda em:calls.append(em.r(3)))
    T.call_ppc(e,RELEASE_VIEW if op==0 else RELEASE_CLIP,[])
    return {"calls":calls}


def host_release(lib,c,op):
    bind(lib);lib.tpw_ldp_reset();lib.tpw_ldp_release(op)
    return {"calls":[0] if lib.tpw_ldp_event_count()==1 else []}


def ppc_getview(e,c):
    r3,_,_=T.call_ppc(e,GET_VIEW,[]);return {"offset":r3-VIEW}


def host_getview(lib,c): bind(lib);return {"offset":0 if lib.tpw_ldp_view() else -1}


def scale_case(r):
    return {"stride":r.randrange(1,500),"depth":r.choice([8,16,24,32]),"clip":[r.randrange(-8,8),r.randrange(-8,8),r.randrange(8,40),r.randrange(8,40)],"flags":r.randrange(0x20),"base":0x44000000,
            "args":[r.getrandbits(16) for _ in range(6)]}


def ppc_scale(e,c,scaled):
    area=e.alloc(40,bytes(40));e.write(area+16,struct.pack(">iiIiii",c["depth"],0,c["base"],1,1,c["stride"]));e.write(CURRENT,struct.pack(">I",area));e.write(ORIGIN,struct.pack(">I",c["base"]));e.write(CLIP,struct.pack(">4i",*c["clip"]));e.write(FLAGS,struct.pack(">I",c["flags"]));e.write(GHOST,struct.pack(">I",0x45000000));e.write(FADE,struct.pack(">I",0x46000000));e.write(FADEROW,struct.pack(">I",0x46000100));e.write(SCALE_STATE,b"\xA5"*40)
    if scaled:
        data=e.alloc(4,b"xxxx");s=e.alloc(8,struct.pack(">IHH",data,c["args"][2],c["args"][3]));args=[c["args"][0],c["args"][1],s,c["args"][4],c["args"][5]];entry=SCALED
    else:args=c["args"];entry=SCALE_DATA
    T.call_ppc(e,entry,args);return {"state":list(struct.unpack(">10I",e.read(SCALE_STATE,40)))}


def host_scale(lib,c,scaled):
    bind(lib);area=Area();area.depth=c["depth"];area.data=c["base"];area.stride=c["stride"];clip=(C.c_int32*4)(*c["clip"]);lib.tpw_ldp_set_state(c["depth"],C.byref(area),clip,c["base"],c["flags"],0x45000000)
    # Fade state is only compared when its flag is clear in the generated cases below.
    state=(C.c_int32*10).from_address(lib.tpw_ldp_scale_state());C.memset(state,0xA5,40);a=c["args"];data=C.create_string_buffer(b"xxxx")
    lib.tpw_ldp_scale(int(scaled),a[0],a[1],data,a[2],a[3],a[4],a[5]);return {"state":[x&MASK for x in state]}


TESTS=[
    FnTest("LbPalette_ToCompact",ppc_compact,host_compact,rnd=compact_case),
    FnTest("LbDraw_RemappedSprite",ppc_remap,host_remap,rnd=remap_case),
]
for name,entry,op,slot in WRAPPERS:
    TESTS.append(FnTest(name,lambda e,c,en=entry,o=op,s=slot:ppc_wrapper(e,c,en,o,s),
                        lambda l,c,o=op,s=slot:host_wrapper(l,c,o,s),rnd=wrapper_case))
TESTS += [
    FnTest("LbDraw_ReleaseViewPort",lambda e,c:ppc_release(e,c,0),lambda l,c:host_release(l,c,0),rnd=lambda r:{}),
    FnTest("LbDraw_ReleaseClipRect",lambda e,c:ppc_release(e,c,1),lambda l,c:host_release(l,c,1),rnd=lambda r:{}),
    FnTest("LbDraw_GetViewPort",ppc_getview,host_getview,rnd=lambda r:{}),
    FnTest("LbDraw_ScaledSprite",lambda e,c:ppc_scale(e,c,True),lambda l,c:host_scale(l,c,True),rnd=scale_case),
    FnTest("LbDraw_SetSpriteScalingData",lambda e,c:ppc_scale(e,c,False),lambda l,c:host_scale(l,c,False),rnd=scale_case),
]


# ---- bfl::draw::CDraw (Draw.cpp), LbDraw_SetCurrent(TbSurface*) and LbDraw_Palette
CD_CTOR, CD_DTOR, CD_SET_AREA, CD_SET_SURFACE = 0x100651B8, 0x1006524C, 0x100652BC, 0x1006552C
CD_SET_CLIP, CD_SET_VIEW, CD_CLEAR_CLIP, CD_CLEAR_VIEW = 0x1006560C, 0x100656F0, 0x1006591C, 0x100659A4
CD_GETS = [0x10065868, 0x100658A4, 0x100658E0]
CD_CONTEXTS = {0x100748DC: 8, 0x100748D8: 16, 0x100748D4: 24, 0x100748D0: 32}
LB_CONTEXTS = {0x10074824: 8, 0x10074820: 16, 0x1007481C: 24, 0x10074818: 32}
SETUP_CLIENT, GET_AREA, OP_DELETE = 0x10016928, 0x10066E84, 0x1006B2D4
LB_SET_CURRENT, LB_SET_VIEW, PIXCAPS_SET, PIXCAPS = 0x1002B690, 0x1002B1C0, 0x100162E8, 0x10074838
PALETTE_FN, GLOBAL_PALETTE, FIND_COLOUR, RECT_FILLED, PROP_TEXT = 0x1002AB34, 0x10074070, 0x1001A5C8, 0x1002AF44, 0x1002B0B0
CD_SIZE = 0x870
U32 = struct.Struct(">I")


def bind_cd(lib):
    bind(lib)
    if getattr(lib, "cd_bound", False): return
    VP = C.c_void_p
    lib.tpw_cd_construct.argtypes = []
    lib.tpw_cd_destroy.argtypes = [C.c_int32]; lib.tpw_cd_destroy.restype = C.c_int32
    lib.tpw_cd_field.argtypes = [C.c_int32]; lib.tpw_cd_field.restype = VP
    lib.tpw_cd_context.argtypes = [C.c_int32]; lib.tpw_cd_context.restype = VP
    lib.tpw_cd_set_area.argtypes = [VP]; lib.tpw_cd_set_surface.argtypes = [VP]
    lib.tpw_cd_rect_op.argtypes = [C.c_int32, VP]
    lib.tpw_cd_get.argtypes = [C.c_int32]; lib.tpw_cd_get.restype = C.c_int32
    lib.tpw_ldp_set_current.argtypes = [C.c_int32, VP, VP]
    lib.tpw_ldp_globals.argtypes = [VP]
    lib.tpw_ldp_set_globals.argtypes = [C.c_uint64, C.c_uint64, C.c_uint32]
    lib.tpw_ldp_palette.restype = VP
    lib.tpw_ldp_flags.restype = C.c_uint32
    lib.tpw_ldp_events.restype = VP
    lib.tpw_ldp_palette_draw.argtypes = [C.c_int32, C.c_int32, C.c_uint32, C.c_uint32, C.c_int32]
    lib.cd_bound = True


# An area as a dict: masks (16 raw bytes), depth, pal, data, width, height, stride.
def area_bytes(a):
    return a["masks"] + struct.pack(">iiIiii", a["depth"], a["pal"], a["data"], a["width"], a["height"], a["stride"])


def area_from_bytes(b):
    d, p, data, w, h, st = struct.unpack(">iiIiii", b[16:40])
    return {"masks": bytes(b[:16]), "depth": d, "pal": p, "data": data, "width": w, "height": h, "stride": st}


def area_to_host(a, out):
    # The masks are four words: big-endian on the Mac, native on the host.
    C.memmove(C.addressof(out.masks), struct.pack("=4I", *struct.unpack(">4I", a["masks"])), 16)
    out.depth, out.pal, out.data = a["depth"], a["pal"], a["data"] or None
    out.width, out.height, out.stride = a["width"], a["height"], a["stride"]


def area_from_host(x):
    return {"masks": struct.pack(">4I", *struct.unpack("=4I", bytes(x.masks))), "depth": x.depth, "pal": x.pal, "data": (x.data or 0) & MASK,
            "width": x.width, "height": x.height, "stride": x.stride}


MASK_SETS = [bytes(16), bytes.fromhex("0000f800000007e0000000ff00000000"), bytes(range(16))]


def rand_area(r):
    return {"masks": r.choice(MASK_SETS), "depth": r.choice([8, 16, 24, 32, 8, 16, 15, 0, 7]),
            "pal": r.choice([0, 0, 1, 2, -1]), "data": r.randrange(0x1000, 0x100000),
            "width": r.randrange(-4, 700), "height": r.randrange(-4, 500), "stride": r.randrange(0, 3000)}


def variant(r, a):
    b = dict(a)
    k = r.randrange(6)
    if k == 1: b["masks"] = r.choice(MASK_SETS)
    elif k == 2: b["depth"] = r.choice([8, 16, 24, 32, 15])
    elif k == 3: b["pal"] = r.choice([0, 1, 3])
    elif k == 4: b.update(data=r.randrange(0x1000, 0x100000), width=r.randrange(1, 700), height=r.randrange(1, 500))
    elif k == 5: b = rand_area(r)
    return b


def rand_rect(r):
    return [r.randrange(-50, 400) for _ in range(4)]


def cd_case(r):
    cur = rand_area(r)
    return {"ctx": r.choice([0, 8, 16, 24, 32]), "cur": cur, "view": variant(r, cur), "clip": rand_rect(r),
            "dirty": r.choice([0, 0, 1, 7]), "arg": variant(r, cur), "rect": rand_rect(r), "null": r.random() < .1,
            "flag": r.choice([0, -1, 1, 0x7fff, -0x8000])}


def ppc_cd_load(e, c):
    obj = e.alloc(CD_SIZE, b"\xA5" * CD_SIZE)
    ctx = {v: k for k, v in CD_CONTEXTS.items()}.get(c["ctx"], 0)
    e.write(obj, U32.pack(ctx) + U32.pack(0))
    e.write(obj + 8, area_bytes(c["cur"])); e.write(obj + 0x30, area_bytes(c["view"]))
    e.write(obj + 0x58, struct.pack(">4i", *c["clip"])); e.write(obj + 0x868, U32.pack(c["dirty"]))
    return obj


def ppc_cd_state(e, obj):
    ctx = U32.unpack(e.read(obj, 4))[0]
    return {"ctx": CD_CONTEXTS.get(ctx, 0 if ctx == 0 else hex(ctx)), "surface": U32.unpack(e.read(obj + 4, 4))[0],
            "cur": area_from_bytes(e.read(obj + 8, 40)), "view": area_from_bytes(e.read(obj + 0x30, 40)),
            "clip": list(struct.unpack(">4i", e.read(obj + 0x58, 16))), "dirty": U32.unpack(e.read(obj + 0x868, 4))[0]}


def host_cd_load(lib, c):
    bind_cd(lib); lib.tpw_cd_construct()
    C.c_void_p.from_address(lib.tpw_cd_field(0)).value = lib.tpw_cd_context(c["ctx"]) if c["ctx"] else None
    C.c_void_p.from_address(lib.tpw_cd_field(1)).value = None
    area_to_host(c["cur"], Area.from_address(lib.tpw_cd_field(2)))
    area_to_host(c["view"], Area.from_address(lib.tpw_cd_field(3)))
    C.memmove(lib.tpw_cd_field(4), (C.c_int32 * 4)(*c["clip"]), 16)
    C.c_uint32.from_address(lib.tpw_cd_field(5)).value = c["dirty"]


def host_cd_state(lib):
    ctx = C.c_void_p.from_address(lib.tpw_cd_field(0)).value
    code = 0 if not ctx else next((d for d in (8, 16, 24, 32) if lib.tpw_cd_context(d) == ctx), hex(ctx))
    surface = C.c_void_p.from_address(lib.tpw_cd_field(1)).value or 0
    return {"ctx": code, "surface": surface & MASK,
            "cur": area_from_host(Area.from_address(lib.tpw_cd_field(2))),
            "view": area_from_host(Area.from_address(lib.tpw_cd_field(3))),
            "clip": list((C.c_int32 * 4).from_address(lib.tpw_cd_field(4))),
            "dirty": C.c_uint32.from_address(lib.tpw_cd_field(5)).value}


def ppc_setup_client(e):
    # The same as the shim's fake of TbRenderArea::SetupClientRA(TbRenderArea&, const TbRect&).
    res, this, client, rp = e.r(3), e.r(4), e.r(5), e.r(6)
    a = area_from_bytes(e.read(this, 40)); l, t, rr, b = struct.unpack(">4i", e.read(rp, 16))
    a["data"] = (a["data"] + t * a["stride"] + l * ((a["depth"] + 7) // 8)) & MASK
    a["width"], a["height"] = rr - l, b - t
    e.write(client, area_bytes(a)); e.write(res, U32.pack(0))


def ppc_cd_setup(e):
    T.stub_internal(e, SETUP_CLIENT, ppc_setup_client)
    T.stub_internal(e, GET_AREA, lambda em: em.set_r(3, (em.r(3) + 0x28) & MASK))


def ppc_cd_ctor(e, c):
    obj = e.alloc(CD_SIZE, b"\xA5" * CD_SIZE); r3, _, _ = T.call_ppc(e, CD_CTOR, [obj])
    return {"self": r3 == obj, "state": ppc_cd_state(e, obj)}


def host_cd_ctor(lib, c):
    bind_cd(lib); lib.tpw_cd_construct(); return {"self": True, "state": host_cd_state(lib)}


def ppc_cd_dtor(e, c):
    deleted = []
    T.stub_internal(e, OP_DELETE, lambda em: deleted.append(em.r(3)))
    obj = ppc_cd_load(e, c); r3, _, _ = T.call_ppc(e, CD_DTOR, [obj, c["flag"] & MASK])
    return {"self": r3 == obj, "deleted": deleted == [obj], "state": ppc_cd_state(e, obj)}


def host_cd_dtor(lib, c):
    host_cd_load(lib, c); state = host_cd_state(lib); deleted = bool(lib.tpw_cd_destroy(c["flag"]))
    return {"self": True, "deleted": deleted, "state": state}


def ppc_cd_area(e, c):
    ppc_cd_setup(e); obj = ppc_cd_load(e, c)
    a = 0 if c["null"] else e.alloc(40, area_bytes(c["arg"]))
    T.call_ppc(e, CD_SET_AREA, [obj, a]); return ppc_cd_state(e, obj)


def host_cd_area(lib, c):
    host_cd_load(lib, c); a = Area(); area_to_host(c["arg"], a)
    lib.tpw_cd_set_area(None if c["null"] else C.byref(a)); return host_cd_state(lib)


def ppc_cd_surface(e, c):
    ppc_cd_setup(e); obj = ppc_cd_load(e, c)
    s = 0 if c["null"] else e.alloc(0x50, bytes(0x28) + area_bytes(c["arg"]))
    T.call_ppc(e, CD_SET_SURFACE, [obj, s]); return ppc_cd_state(e, obj)


def host_cd_surface(lib, c):
    host_cd_load(lib, c); a = Area(); area_to_host(c["arg"], a)
    lib.tpw_cd_set_surface(None if c["null"] else C.byref(a)); return host_cd_state(lib)


def ppc_cd_rect(e, c, op):
    ppc_cd_setup(e); obj = ppc_cd_load(e, c); rect = e.alloc(16, struct.pack(">4i", *c["rect"]))
    entry = [CD_SET_CLIP, CD_SET_VIEW, CD_CLEAR_CLIP, CD_CLEAR_VIEW][op]
    T.call_ppc(e, entry, [obj, rect] if op < 2 else [obj]); return ppc_cd_state(e, obj)


def host_cd_rect(lib, c, op):
    host_cd_load(lib, c); lib.tpw_cd_rect_op(op, (C.c_int32 * 4)(*c["rect"])); return host_cd_state(lib)


def ppc_cd_get(e, c, which):
    obj = ppc_cd_load(e, c); r3, _, _ = T.call_ppc(e, CD_GETS[which], [obj]); return {"offset": r3 - obj}


def host_cd_get(lib, c, which):
    host_cd_load(lib, c); return {"offset": lib.tpw_cd_get(which)}


def lbcur_case(r):
    return {"null": r.random() < .1, "words": [r.getrandbits(32) for _ in range(4)], "area": rand_area(r),
            "base": r.choice([8, 16, 24, 32]), "origin": r.randrange(0x1000, 0x100000), "flags": r.getrandbits(32)}


def ppc_lbcur(e, c):
    calls = []
    T.stub_internal(e, GET_AREA, lambda em: em.set_r(3, (em.r(3) + 0x28) & MASK))
    T.stub_internal(e, LB_SET_VIEW, lambda em: calls.append(("view", em.r(3))))
    def caps(em):
        same = em.r(4) == U32.unpack(em.read(CURRENT, 4))[0]
        calls.append(("caps", em.r(3) == PIXCAPS, U32.unpack(em.read(em.r(4), 4))[0] if same else "other area"))
    T.stub_internal(e, PIXCAPS_SET, caps)
    base = {v: k for k, v in LB_CONTEXTS.items()}[c["base"]]
    e.write(BASE, U32.pack(base)); e.write(ORIGIN, U32.pack(c["origin"])); e.write(FLAGS, U32.pack(c["flags"]))
    e.write(CURRENT, U32.pack(0))
    s = 0 if c["null"] else e.alloc(0x50, struct.pack(">4I", *c["words"]) + bytes(0x18) + area_bytes(c["area"]))
    T.call_ppc(e, LB_SET_CURRENT, [s])
    cur = U32.unpack(e.read(CURRENT, 4))[0]
    g = [0 if cur == 0 else 1 if cur == s + 0x28 else 2, U32.unpack(e.read(ORIGIN, 4))[0], U32.unpack(e.read(FLAGS, 4))[0],
         U32.unpack(e.read(GHOST, 4))[0], U32.unpack(e.read(FADE, 4))[0], U32.unpack(e.read(FADEROW, 4))[0],
         LB_CONTEXTS.get(U32.unpack(e.read(BASE, 4))[0], 0)]
    return {"globals": g, "calls": calls}


def host_lbcur(lib, c):
    bind_cd(lib); lib.tpw_ldp_set_globals(c["base"], c["origin"], c["flags"])
    words = (C.c_uint32 * 4)(*c["words"]); a = Area(); area_to_host(c["area"], a)
    lib.tpw_ldp_set_current(int(c["null"]), words, C.byref(a))
    out = (C.c_uint64 * 8)(); lib.tpw_ldp_globals(out)
    g = [int(out[0])] + [int(x) & MASK for x in out[1:6]] + [int(out[6])]
    n = lib.tpw_ldp_event_count(); ev = (Event * n).from_address(lib.tpw_ldp_events())
    calls = [("view", 0) if x.a[0] == -1 else ("view", "rect") for x in ev if x.op == 100]
    if not c["null"]: calls.append(("caps", True, int(out[7]) & MASK))
    return {"globals": g, "calls": calls}


class Event(C.Structure):
    _fields_ = [("op", C.c_int32), ("a", C.c_int64 * 8)]


def palette_case(r):
    return {"x": r.randrange(-50, 300), "y": r.randrange(-50, 300), "w": r.randrange(0, 40), "h": r.randrange(0, 30),
            "labels": r.choice([0, 1, 1, 7]), "palette": r.randbytes(1024), "flags": r.getrandbits(32)}


def text_word(b):
    b = b.split(b"\0")[0][:8]
    return int.from_bytes(b.ljust(8, b"\0"), "big")


def ppc_palette(e, c):
    log = []
    def find(em):
        log.append(("find", em.r(4), em.r(5), em.r(6))); em.set_r(3, em.r(4) ^ em.r(5) ^ em.r(6))
    def rect(em):
        p = em.r(3); log.append(("rect",) + struct.unpack(">4i", em.read(p, 16)) + ((em.r(4) << 8) | (em.r(5) >> 24),
                                  U32.unpack(em.read(FLAGS, 4))[0]))
    def text(em):
        x, y = struct.unpack(">2i", struct.pack(">2I", em.r(3), em.r(4)))
        log.append(("text", x, y, text_word(em.read(em.r(5), 9)), (em.r(6) << 8) | (em.r(7) >> 24)))
    T.stub_internal(e, FIND_COLOUR, find); T.stub_internal(e, RECT_FILLED, rect); T.stub_internal(e, PROP_TEXT, text)
    e.write(GLOBAL_PALETTE, c["palette"]); e.write(FLAGS, U32.pack(c["flags"]))
    T.call_ppc(e, PALETTE_FN, [c["x"] & MASK, c["y"] & MASK, c["w"], c["h"], c["labels"]])
    return {"log": log, "flags": U32.unpack(e.read(FLAGS, 4))[0]}


def host_palette(lib, c):
    bind_cd(lib); lib.tpw_ldp_reset()
    area = Area(); big = (C.c_int32 * 4)(-0x40000000, -0x40000000, 0x40000000, 0x40000000)
    lib.tpw_ldp_set_state(8, C.byref(area), big, None, c["flags"], None)
    C.memmove(lib.tpw_ldp_palette(), c["palette"], 1024)
    lib.tpw_ldp_palette_draw(c["x"], c["y"], c["w"], c["h"], c["labels"])
    n = lib.tpw_ldp_event_count(); ev = (Event * n).from_address(lib.tpw_ldp_events())
    log = []
    for x in ev:
        a = [int(v) for v in x.a]
        if x.op == 102: log.append(("find", a[0], a[1], a[2]))
        elif x.op % 20 == 11: log.append(("rect", a[0], a[1], a[2], a[3], a[4], 0))
        elif x.op % 20 == 12: log.append(("text", a[0], a[1], a[2] & 0xFFFFFFFFFFFFFFFF, a[3]))
        else: log.append(("other", x.op))
    return {"log": log, "flags": lib.tpw_ldp_flags()}


TESTS += [
    FnTest("CDraw::CDraw", ppc_cd_ctor, host_cd_ctor, rnd=lambda r: {}),
    FnTest("CDraw::~CDraw", ppc_cd_dtor, host_cd_dtor, rnd=cd_case),
    FnTest("CDraw::SetCurrent(TbRenderArea*)", ppc_cd_area, host_cd_area, rnd=cd_case),
    FnTest("CDraw::SetCurrent(TbSurface*)", ppc_cd_surface, host_cd_surface, rnd=cd_case),
    FnTest("CDraw::SetClipRect", lambda e, c: ppc_cd_rect(e, c, 0), lambda l, c: host_cd_rect(l, c, 0), rnd=cd_case),
    FnTest("CDraw::SetViewPort", lambda e, c: ppc_cd_rect(e, c, 1), lambda l, c: host_cd_rect(l, c, 1), rnd=cd_case),
    FnTest("CDraw::ClearClipRect", lambda e, c: ppc_cd_rect(e, c, 2), lambda l, c: host_cd_rect(l, c, 2), rnd=cd_case),
    FnTest("CDraw::ClearViewPort", lambda e, c: ppc_cd_rect(e, c, 3), lambda l, c: host_cd_rect(l, c, 3), rnd=cd_case),
    FnTest("CDraw::GetCurrent", lambda e, c: ppc_cd_get(e, c, 0), lambda l, c: host_cd_get(l, c, 0), rnd=cd_case),
    FnTest("CDraw::GetClipRect", lambda e, c: ppc_cd_get(e, c, 1), lambda l, c: host_cd_get(l, c, 1), rnd=cd_case),
    FnTest("CDraw::GetViewPort", lambda e, c: ppc_cd_get(e, c, 2), lambda l, c: host_cd_get(l, c, 2), rnd=cd_case),
    FnTest("LbDraw_SetCurrent(TbSurface*)", ppc_lbcur, host_lbcur, rnd=lbcur_case),
    FnTest("LbDraw_Palette", ppc_palette, host_palette, rnd=palette_case),
]


def setup(runner): T.install_std_stubs(runner.emu)
