"""w233: the real 6892-byte original circle outline, with no callee fakes.
Compare complete padded framebuffers and guard bytes. All area/table/global
storage is checked unchanged. Native offsets are exported independently.
Ghost tables are deliberately non-idempotent so duplicate octant visits matter.
Bounded domain: positive radii <=96, disjoint drawing globals/tables/framebuffers;
also test signed-negative radius words (the original signed loop skips them).
"""
import ctypes as C
import random
import tpwtest as T
from tpwtest import FnTest
import calllog as CL

PROGRAM='bullfrog symbols shared'
SHIM_LIBRARY='unit_RenderContext8Circle'
ENTRY=0x1002c7dc
GLOBALS=[0x10074880,0x10074860,0x1007485c,0x10074834,0x10074830,0x10074828]
STUBS=CL.Stubs()
STUBS.real.add(ENTRY)
MASK=0xffffffff
GHOST=bytes((i*29+(i>>8)*17+(i^(i>>8)))&255 for i in range(65536))


def bind(lib):
    if getattr(lib,'rcircle_bound',False): return
    for name,ret,args in [('global',C.c_uint64,[C.c_uint]),('layout',C.c_uint32,[C.c_uint]),
      ('run',None,[C.c_int32,C.c_int32,C.c_uint32,C.c_void_p])]:
        f=getattr(lib,'tpw_rcircle_'+name); f.restype,f.argtypes=ret,args
    lib.rcircle_layout=[lib.tpw_rcircle_layout(i) for i in range(5)]
    assert lib.rcircle_layout==[48,24,32,36,40],lib.rcircle_layout
    lib.rcircle_bound=True


def run(e,lib,c):
    if lib: bind(lib)
    ps,endian=(4,'big') if e else (8,'little')
    layout=[40,24,28,32,36] if e else lib.rcircle_layout
    globals=GLOBALS if e else [lib.tpw_rcircle_global(i) for i in range(6)]
    keep=[]
    def read(a,n): return e.read(a,n) if e else C.string_at(a,n)
    def write(a,b):
        if e: e.write(a,b)
        else: C.memmove(a,b,len(b))
    def put(a,v,n=4): write(a,(v&((1<<(n*8))-1)).to_bytes(n,endian))
    def alloc(b):
        if e: return e.alloc(len(b),b)
        buf=C.create_string_buffer(b,len(b)); keep.append(buf); return C.addressof(buf)
    r=random.Random(c['seed'])
    width,height,pad=c['size']; stride=width+pad
    raw=r.randbytes(stride*height+128)
    pixels=alloc(raw)
    origin=pixels+64+c['origin_x']+(height-1)*stride if c['negative_stride'] else pixels+64+c['origin_x']
    ghost=alloc(b'\xb6'*32+GHOST+b'\xd9'*32)
    fade=alloc(b'\xa4'*32+bytes((i*43+c['seed'])&255 for i in range(256))+b'\xc5'*32)
    area=alloc(b'\xa5'*layout[0]); colour=alloc(c['colour']); obj=alloc(b'\xf1'*16)
    for i in range(6): put(area+i*4,r.getrandbits(32))
    put(area+layout[1],pixels+64,ps)
    put(area+layout[2],width); put(area+layout[3],height)
    put(area+layout[4],-stride if c['negative_stride'] else stride)
    for i,p in [(0,area),(2,origin),(4,ghost+32),(5,fade+32)]: put(globals[i],p,ps)
    for i,v in enumerate(c['clip']): put(globals[1]+i*4,v)
    put(globals[3],c['flags'])
    protected=[(area,layout[0]),(ghost,65536+64),(fade,256+64),(colour,5),(obj,16)]
    protected += [(p,16 if i==1 else 4 if i==3 else ps) for i,p in enumerate(globals)]
    before=[read(a,n) for a,n in protected]
    saved=STUBS.activate(e) if e else None
    try:
        if e:
            T.call_ppc(e,ENTRY,[obj,c['x']&MASK,c['y']&MASK,c['radius'],
                int.from_bytes(c['colour'][:4],'big'),c['colour'][4]<<24])
        else: lib.tpw_rcircle_run(c['x'],c['y'],c['radius'],colour)
        return dict(pixels=read(pixels,len(raw)),unchanged=[read(a,n)==b for (a,n),b in zip(protected,before)])
    finally:
        if e: STUBS.deactivate(e,saved)


def base(r):
    width,height=r.randrange(1,65),r.randrange(1,49)
    ox=r.randrange(min(width,5))
    left=r.randrange(-ox,width-ox); right=r.randrange(left,width-ox+1)
    top=r.randrange(height); bottom=r.randrange(top,height+1)
    return dict(seed=r.getrandbits(32),size=(width,height,r.randrange(8)),origin_x=ox,
      clip=[left,top,right,bottom],negative_stride=bool(r.randrange(2)),
      flags=r.getrandbits(32),x=r.randrange(-96,width+96),y=r.randrange(-96,height+96),
      radius=r.randrange(97),colour=r.randbytes(5))


def edges():
    r=random.Random(233); out=[]
    for flags in range(64):
        for radius in [0,1,2,3,7,16,31]:
            for x,y in [(0,0),(20,16),(39,31),(-1,16),(40,16)]:
                c=base(r); c.update(size=(40,32,7),origin_x=0,clip=[0,0,40,32],
                    flags=flags,x=x,y=y,radius=radius)
                out.append(c)
        for clip in [[5,5,5,10],[5,5,10,5],[10,10,5,5],[-3,0,37,32]]:
            c=base(r); c.update(size=(40,32,7),origin_x=3,clip=clip,flags=flags,x=5,y=5,radius=8)
            out.append(c)
        for radius in [0x80000000,0x80000001,0xffffffff]:
            c=base(r); c.update(flags=flags,radius=radius); out.append(c)
        for x,y in [(-0x80000000,0),(0x7fffffff,1),(0,-0x80000000),(1,0x7fffffff)]:
            c=base(r); c.update(flags=flags,x=x,y=y,radius=3); out.append(c)
    return out


TESTS=[FnTest('RenderContext8::LbDraw_CircleOutline',lambda e,c:run(e,None,c),
    lambda lib,c:run(None,lib,c),edge=edges,rnd=base)]
