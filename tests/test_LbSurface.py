"""w231: 31 real bullfrog surface entry points against native C++.
Imported drawing methods are scripted recording boundaries. Internal surface calls
and TbMaskInfo::Set remain real. Compare all logical fields, original padding,
guards, descriptor/output storage, globals and ordered calls. Unsupported ClearRect
depths have an uninitialized fill word and are excluded. Partial output overlap,
invalid pointers and platform rendering are outside this bounded differential test.
"""
import ctypes as C
import random
import calllog as CL
import tpwtest as T
from tpwtest import FnTest

PROGRAM = 'bullfrog symbols shared'
SHIM_LIBRARY = 'unit_LbSurface'
MASK = 0xffffffff
LOST = 0x887601c2
ENTRY = [0x100666ac,0x10066780,0x1006682c,0x10066888,0x100668bc,0x100668fc,
 0x1006693c,0x100669c4,0x10066a5c,0x10066af4,0x10066b34,0x10066bd4,0x10066c20,
 0x10066cb8,0x10066d00,0x10066d44,0x10066dcc,0x10066e84,0x10066f80,0x10066fc0,
 0x1006700c,0x1006710c,0x10067178,0x10067214,0x100674e0,0x10067608,0x10067688,
 0x10067900,0x10067c3c,0x10067d90,0x10067df0]
NAMES = ['MapToRenderArea','GetPixelFormat','SetCurrent','GetCurrent','GetOriginPtr',
 'GetDrawFlags','SetDrawFlags','SetDrawFlagsOn','SetDrawFlagsOff','GetFadeTable',
 'SetFadeTable','GetFadeStep','SetFadeStep','GetFadeColour','GetGhostTable','SetGhostTable',
 'CalculateSurfaceArea','GetRenderArea','SetSysLock','RegisterDirectDraw',
 'RegisterDirectDrawSurface','_InitialiseSurface','_CheckAndRestoreSurface','ClearRect',
 'Create','Delete','Blt','StretchBlt','Lock','Unlock','GetDDSurface']
GLOBALS = [0x10074834,0x10074830,0x1007482c,0x10074828,0x100748e0,0x100748e4,0x10074880,0x10074070]
MAC_LAYOUT = [80,0,4,8,12,16,24,28,32,36,40,40,24,28,32,36,108,36,40,72,104]
RESULTS = {20,23,24,25,26,27,28}
POINTERS = {0,1,3,4,9,14,17,30}
SCALARS = {5,11,13,22}


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib,'lsf_bound',False):
        return
    for name,ret,args in [('alloc',C.c_uint64,[C.c_uint32]),('global',C.c_uint64,[C.c_uint]),
      ('layout',C.c_uint,[C.POINTER(C.c_uint32)]),('run',C.c_uint64,[C.POINTER(C.c_uint64)])]:
        f=getattr(lib,'tpw_lsf_'+name); f.restype,f.argtypes=ret,args
    data=(C.c_uint32*32)()
    n=lib.tpw_lsf_layout(data)
    lib.lsf_layout=list(data)[:n]
    assert lib.lsf_layout == [112,0,8,16,24,32,40,48,52,56,64,48,24,32,36,40,120,40,48,80,112],lib.lsf_layout
    lib.lsf_bound=True


class Side:
    def write(self,a,b):
        if self.e: self.e.write(a,b)
        else: C.memmove(a,b,len(b))

    def read(self,a,n):
        return self.e.read(a,n) if self.e else C.string_at(a,n)

    def put(self,a,v,n=4):
        self.write(a,(v & ((1<<(n*8))-1)).to_bytes(n,self.endian))

    def word(self,a,n=4):
        return int.from_bytes(self.read(a,n),self.endian)

    def pointer(self,a):
        return self.word(a,self.ps)

    def lab(self,a):
        return 0 if not a else self.labels.get(a,-1)

    def row_label(self,p,base):
        if not p: return (0,0)
        # Surface rows originate in table 0; global rows in table 1. Keep
        # provenance across an external table-pointer mutation, and compare the
        # original 32-bit offset even when a shift produces a wrapped pointer.
        return (1,(p-base)&MASK)

    def block(self,n):
        a=(self.e.alloc(n+32) if self.e else self.lib.tpw_lsf_alloc(n+32))+16
        self.write(a-16,b'\xb3'*16); self.write(a,b'\xa5'*n); self.write(a+n,b'\xc7'*16)
        self.blocks.append((a,n))
        return a

    def area_init(self,a,r,depth):
        for i,v in enumerate([*self.c['masks'],depth,r.getrandbits(32)]): self.put(a+4*i,v)
        self.put(a+self.lo[12],self.tables[2],self.ps)
        for off in self.lo[13:16]: self.put(a+off,r.getrandbits(32))

    def desc_init(self,a,r,index):
        for off in range(0,36,4): self.put(a+off,r.getrandbits(32))
        self.put(a,108)
        self.put(a+self.lo[17],0 if self.c['null_data'] else self.tables[2+index%2],self.ps)
        for off in range(self.lo[18],self.lo[19]+32,4): self.put(a+off,r.getrandbits(32))
        self.put(a+self.lo[19]+12,self.c['depths'][index%3])
        for i,v in enumerate(self.c['masks']): self.put(a+self.lo[19]+16+4*i,v)
        self.put(a+self.lo[20],r.getrandbits(32))

    def __init__(self,e,lib,c):
        self.e,self.lib,self.c=e,lib,c
        if lib: bind(lib)
        self.lo=MAC_LAYOUT if e else lib.lsf_layout
        self.ps,self.endian=(4,'big') if e else (8,'little')
        self.blocks=[]; self.labels={}; self.di=0
        r=random.Random(c['seed']); z=self.lo
        self.globals=GLOBALS if e else [lib.tpw_lsf_global(i) for i in range(8)]
        self.s=self.block(z[0]); self.other=self.block(z[0]); self.src=self.s if c['alias'] else self.other
        self.dd=[self.block(16) for _ in range(3)]
        self.tables=[self.block(8192) for _ in range(4)]
        for i,p in enumerate(self.tables): self.write(p,bytes((j*17+i*31)&255 for j in range(8192)))
        self.area=self.block(z[11]); self.fmt=self.block(24)
        self.rects=[self.block(16) for _ in range(2)]
        self.out=self.block(self.ps); self.desc=self.block(z[16]); self.descriptors=self.block(z[16]*3)
        self.colour=self.block(5); self.result=self.block(4)
        self.write(self.colour,c['colour'])
        fixed=[self.s,self.other,*self.dd,*self.tables,self.area,self.fmt,*self.rects,self.out,self.desc,self.colour,
          self.globals[7],self.s+z[10],self.other+z[10]]
        self.labels={p:i+1 for i,p in enumerate(fixed)}
        for s in [self.s,self.other]:
            self.put(s,r.getrandbits(32))
            for off,p in [(z[2],self.tables[1]),(z[3],self.tables[0]),(z[4],self.tables[0]+c['row']),
              (z[6],self.globals[7]),(z[9],0 if c['null_dd'] else self.dd[0 if s==self.s or c['shared_dd'] else 1])]:
                self.put(s+off,p,self.ps)
            self.write(s+z[5],r.randbytes(8))
            self.put(s+z[7],r.getrandbits(32)); self.put(s+z[8],r.getrandbits(32))
            self.area_init(s+z[10],r,c['depths'][0])
        self.area_init(self.area,r,c['depths'][0])
        for i in range(6): self.put(self.fmt+i*4,r.getrandbits(32))
        for rect in self.rects:
            for i in range(4): self.put(rect+4*i,r.getrandbits(32))
        self.put(self.out,self.tables[3],self.ps)
        self.desc_init(self.desc,r,0)
        for i in range(3): self.desc_init(self.descriptors+i*z[16],r,i)
        self.put(self.result,0xacab1357)
        self.put(self.globals[0],r.getrandbits(32)); self.put(self.globals[4],c['sys'],1)
        for i,p in [(1,self.tables[3]),(2,self.tables[1]),(3,self.tables[1]+256),
          (5,self.dd[2]),(6,[0,self.s+z[10],self.other+z[10]][c['current']])]: self.put(self.globals[i],p,self.ps)
        self.q=[0]*24
        self.q[:13]=[c['op'],self.s,self.src,self.s+z[10] if c['out_alias'] else self.area,
          self.s+z[10] if c['out_alias'] else self.fmt,
          0 if c['null_dest'] else self.rects[0],0 if c['null_source'] else self.rects[1],
          0 if c['null_out'] else (self.s+z[10]+z[12] if c['out_alias'] else self.out),
          self.desc,c['a'] & MASK,c['b'] & MASK,c['flags'],self.dd[1]]
        self.q[13:23]=[self.other+z[10],self.tables[3],self.dd[0],0 if c['null_table'] else self.tables[2],
          self.colour,self.descriptors,self.s,c['mutate'],self.dd[2],c['init_flags']]
        self.q[23]=self.tables[0]
        self.before=[(a,self.read(a,n)) for a,n in self.blocks]
        if e:
            e.lsf=self; self.L=CL.PpcLog(self.labels,c['q']); e.cl=self.L; self.saved=STUBS.activate(e)
        else:
            CL.host_begin(lib,{v:k for k,v in self.labels.items()},c['q'])

    def rect_event(self,name,p):
        self.L.rec(name,(1,*[T.s32(self.word(p+4*i)) for i in range(4)]) if p else (0,))

    def snapshot(self,s):
        z=self.lo
        row=self.pointer(s+z[4])
        self.L.rec('surface',(self.lab(s),self.word(s),*[self.lab(self.pointer(s+z[i])) for i in [2,3]],
          (row-self.tables[0])&MASK if row else 0,self.lab(self.pointer(s+z[6])),
          self.word(s+z[7]),self.word(s+z[8]),self.lab(self.pointer(s+z[9])),int(row==0)))

    def mutate(self,event):
        if self.c['mutate']!=event: return
        self.put(self.s+self.lo[9],self.dd[2],self.ps)
        self.put(self.globals[6],self.q[13],self.ps); self.put(self.globals[2],self.q[14],self.ps)

    def fill(self,d):
        self.write(d,self.read(self.descriptors+min(self.di,2)*108,108)); self.di+=1

    def area_observe(self,p):
        z=self.lo
        return [*[self.word(p+4*i) for i in range(6)],self.lab(self.pointer(p+z[12])),
          *[self.word(p+o) for o in z[13:16]]]

    def surface_observe(self,p):
        z=self.lo; table=self.pointer(p+z[3]); row=self.pointer(p+z[4])
        return [self.word(p),self.lab(self.pointer(p+z[2])),self.lab(table),
          self.row_label(row,self.tables[0]),self.read(p+z[5],8),self.lab(self.pointer(p+z[6])),
          self.word(p+z[7]),self.word(p+z[8]),self.lab(self.pointer(p+z[9])),self.area_observe(p+z[10])]

    def observe(self,answer):
        z=self.lo; row=self.pointer(self.globals[3]); table=self.pointer(self.globals[2])
        untouched=[self.read(a,len(b))==b for a,b in self.before if a in self.tables+self.dd+self.rects+[self.desc,self.descriptors,self.colour]]
        return dict(answer=self.lab(answer) if self.c['op'] in POINTERS else answer & MASK,
          surfaces=[self.surface_observe(self.s),self.surface_observe(self.other)],area=self.area_observe(self.area),
          format=[self.word(self.fmt+4*i) for i in range(6)],out=self.lab(self.pointer(self.out)),
          globals=[self.word(self.globals[0]),self.lab(self.pointer(self.globals[1])),self.lab(table),
            self.row_label(row,self.tables[1]),self.word(self.globals[4],1),self.lab(self.pointer(self.globals[5])),self.lab(self.pointer(self.globals[6]))],
          guards=[self.read(a-16,16)+self.read(a+n,16) for a,n in self.blocks],untouched=untouched,
          log=self.L.log if self.e else CL.host_end(self.lib))


STUBS=CL.Stubs()
STUBS.real.update(ENTRY+[0x10016264])


def desc(e):
    s=e.lsf; p=e.r(4)
    s.L.rec('desc',(s.lab(e.r(3)),s.word(p)))
    if s.c['op'] in (20,24): s.L.rec('desc_flags',(s.word(p+4),))
    if s.c['op']==24: s.L.rec('desc_zero',(int(s.read(p,108)==b'\0\0\0\x6c\0\0\x10\x09'+b'\0'*100),))
    s.snapshot(s.s); s.snapshot(s.src); s.mutate(1); s.fill(p)
    e.set_r(3,s.L.ret('desc') & MASK)


def lock(e):
    s=e.lsf; p=e.r(5)
    s.L.rec('lock',(s.lab(e.r(3)),s.lab(e.r(4)),s.word(p),e.r(6),s.lab(e.r(7))))
    s.mutate(4); s.fill(p); e.set_r(3,s.L.ret('lock') & MASK)


def simple(name,event=0,extra=False):
    def fake(e):
        s=e.lsf; s.L.rec(name,(s.lab(e.r(3)),s.lab(e.r(4))) if extra else (s.lab(e.r(3)),))
        if event: s.mutate(event)
        e.set_r(3,s.L.ret(name)&MASK)
    return fake


def key(e):
    s=e.lsf; s.L.rec('key',(s.lab(e.r(3)),e.r(4),s.word(e.r(5)),s.word(e.r(5)+4)))
    e.set_r(3,s.L.ret('key')&MASK)


def blt(e):
    s=e.lsf
    s.L.rec('blt',(s.lab(e.r(3)),s.lab(e.r(5)),e.r(7)))
    s.rect_event('dest_rect',e.r(4)); s.rect_event('source_rect',e.r(6))
    p=e.r(8)
    if s.c['op']==23: s.L.rec('fill',(s.word(p),s.word(p+80)))
    else:
        for i in range(25): s.L.rec('fx',(i,s.word(p+4*i)))
    e.set_r(3,s.L.ret('blt')&MASK)


def fast(e):
    s=e.lsf; s.L.rec('fast',(s.lab(e.r(3)),e.r(4),e.r(5),s.lab(e.r(6)),e.r(8)))
    s.rect_event('source_rect',e.r(7)); e.set_r(3,s.L.ret('blt')&MASK)


def create(e):
    s=e.lsf; p=e.r(4)
    s.L.rec('create',(s.lab(e.r(3)),*[s.word(p+o) for o in [0,4,8,12,104]],int(e.r(5)==s.s+36),s.lab(e.r(6))))
    s.put(e.r(5),s.q[12]); s.write(p,b'\xa7'*108); e.set_r(3,s.L.ret('create')&MASK)


def zero(e):
    s=e.lsf; s.L.rec('zero',(e.r(4),)); s.write(e.r(3),bytes(e.r(4)))


def copy(e):
    s=e.lsf; s.rect_event('copy',e.r(4))
    for i in range(4): s.put(e.r(3)+4*i,s.word(e.r(4)+4*i))


def glue(fn):
    def call(e):
        # Import glue saves the caller TOC before the caller's lwz r2,20(r1).
        e.lsf.put(e.r(1)+20,e.r(2))
        fn(e)
    return call


for a,f in [(0x1006ba6c,desc),(0x1006bc7c,lock),(0x1006bc64,simple('unlock',extra=True)),
 (0x1006bc94,simple('lost',5)),(0x1006bcac,simple('restore',6)),(0x1006bcc4,key),
 (0x1006bcdc,blt),(0x1006bcf4,fast),(0x1006bd24,create),(0x1006bc4c,simple('addref',2)),
 (0x1006bd0c,simple('release',3)),(0x1006bd3c,zero),(0x1002b690,CL.recorder('current','L')),
 (0x100298d0,copy),(0x10067e70,CL.recorder('error','i'))]: STUBS.internal(a,glue(f) if a>=0x1006b000 else f)


def execute(s):
    q=s.q; op=q[0]
    if not s.e: return s.lib.tpw_lsf_run((C.c_uint64*24)(*q))
    args={0:[q[3],q[1]],1:[q[1],q[4]],2:[q[1]],3:[],4:[q[1]],5:[q[1]],
      6:[q[1],q[9]],7:[q[1],q[9]],8:[q[1],q[9]],9:[q[1]],10:[q[1],q[16]],11:[q[1]],
      12:[q[1],q[9]],13:[q[1],q[9]],14:[q[1]],15:[q[1],q[16]],16:[q[1],q[8]],17:[q[1]],
      18:[q[9]],19:[q[15]],20:[s.result,q[12],q[1]],21:[q[1],q[9],q[10],q[11],q[22]],22:[q[1]],
      23:[s.result,q[1],q[5],int.from_bytes(s.c['colour'][:4],'big'),s.c['colour'][4]<<24,q[11]],
      24:[s.result,q[9],q[10],q[11],q[1]],25:[s.result,q[1]],
      26:[s.result,q[1],q[9],q[10],q[2],q[6],q[11]],27:[s.result,q[1],q[5],q[2],q[6],q[11]],
      28:[s.result,q[1],q[7]],29:[q[1]],30:[q[1],q[9]]}[op]
    result=T.call_ppc(s.e,ENTRY[op],args)[0]&MASK
    return s.word(s.result) if op in RESULTS else result if op in POINTERS|SCALARS else 0


def run(e,lib,c):
    s=Side(e,lib,c)
    try: return s.observe(execute(s))
    finally:
        if e: STUBS.deactivate(e,s.saved)


def base(r,op):
    masks=r.choice([[0xff0000,0xff00,0xff,0xff000000],[0xf800,0x7e0,0x1f,0],
      [0x7c00,0x3e0,0x1f,0x8000],[0,0,0,0],[0x00ff0000,0xff,0xff000000,0xff00]])
    c=dict(op=op,seed=r.getrandbits(32),depths=[r.choice([8,16,24,32]) for _ in range(3)],
      masks=masks,colour=r.randbytes(5),row=r.randrange(0,4096),a=r.getrandbits(32),b=r.getrandbits(32),
      flags=r.getrandbits(32),init_flags=r.getrandbits(32),sys=r.choice([0,1,2,255]),current=r.randrange(3),
      alias=r.choice([False,True]),shared_dd=r.choice([False,True]),out_alias=r.choice([False,True]),
      null_data=r.choice([False,True]),null_dest=r.choice([False,True]),null_source=r.choice([False,True]),
      null_out=r.choice([False,True]),null_table=r.choice([False,True]),null_dd=False,mutate=0,
      q={'desc':[r.choice([-1,0,1]) for _ in range(3)],'lock':[r.choice([0,1,-1,LOST]) for _ in range(2)],
         'lost':[r.choice([0,1,-1,LOST]) for _ in range(2)],'restore':[r.choice([0,1,-1]) for _ in range(2)],
         'create':[r.choice([0,0,1,-1,LOST])],'blt':[r.choice([0,0,1,-1,LOST])],'key':[-1],
         'addref':[r.getrandbits(32)],'release':[r.getrandbits(32)],'unlock':[r.getrandbits(32)]})
    if op==12: c['a']=r.choice([0,1,2,31,0x1000000,0xffffffff,0x80000000])
    if op==13: c['a']=r.randrange(256)
    if op==21: c['flags']=r.choice([0,8,16,24,32,MASK])
    if op==30: c['a']=r.choice([0,1,MASK,0x80000000]); c['null_dd']=r.choice([False,True])
    return c


def edges(op):
    r=random.Random(23100+op); out=[]
    # Explicit branches and signed status boundaries, independently of random cases.
    for depth in [8,16,24,32]:
        for status in [-0x80000000,-1,0,1,0x7fffffff,LOST]:
            c=base(r,op); c['depths']=[depth]*3; c['q']['desc']=[status]*3
            c['q']['lock']=[status,0]; c['q']['create']=[status]; c['q']['blt']=[status]
            out.append(c)
    if op in [16,17,20,21,24,28]:
        for depth in [0,1,7,15,48,MASK]:
            c=base(r,op); c['depths']=[depth]*3; c['q'].update(desc=[0],create=[0],lock=[0])
            if op==21: c['flags']=depth
            out.append(c)
    if op==28:
        for first in [0,1,-1,LOST]:
            for lost in [0,1,-1,LOST]:
                for restored in [0,1,-1]:
                    for second in [0,1,-1,LOST]:
                        c=base(r,op); c['q'].update(lock=[first,second],lost=[lost],restore=[restored]); out.append(c)
    if op in [6,7,8,10,12,15,17,25,26,27,28,30]:
        for event in [1,2,3,4,5,6]:
            c=base(r,op); c['mutate']=event; c['q'].update(desc=[0,1],lock=[LOST,0],lost=[LOST],restore=[0],blt=[LOST])
            out.append(c)
    if op in [23,26,27]:
        for flags in [0,1,0x10,0x800,0x8000,0x1000000,0x80008001,MASK]:
            for alias in [False,True]:
                c=base(r,op); c.update(flags=flags,alias=alias); c['q']['desc']=[0,1]; out.append(c)
    return out


TESTS=[FnTest(NAMES[op],lambda e,c:run(e,None,c),lambda lib,c:run(None,lib,c),
  edge=lambda op=op:edges(op),rnd=lambda r,op=op:base(r,op)) for op in range(31)]
