"""w230: six real vehicle entry points, scalar/file/runtime recording fakes on both sides.
Every field, padding byte, guard and conditional clear is observed. Failed file reads compare the
written prefix as bytes and check the untouched suffix against its own pre-read bytes; successfully
converted and untouched words compare semantically. Startup uses the source InitVehicleGlobals body,
seven actual native types, independent callback/node labels and boundary snapshots. No float operations.
No out-of-allocation list appends, destructors, OOM, or out-of-scope original helpers are executed.
"""
import ctypes as C
import random
import struct
import calllog as CL
import tpwtest as T
from tpwtest import FnTest
SHIM_LIBRARY = 'unit_VehicleSerialise'
MASK = 0xffffffff
ENTRY = [0x10130eb4, 0x10134a74, 0x10134bc0, 0x10134cfc, 0x10134e20, 0x10134f5c]
OWN = set(ENTRY)
GLOBALS = [0x1034ca90,0x1034ca8c,0x1034ca60,0x1034ca74,0x1037fefc,0x102a4478,0x1034ca58,0x1034ca5c]
OBJECTS = [0x1034ca28,0x1034c9f8,0x1034c9c8,0x1034c998,0x1034c968,0x1034c938,0x1034c908]
VTABLES = [0x102a4574,0x102a4554,0x102a4534,0x102a44f4,0x102a4514,0x102a44d4,0x102a44b4]
CALLBACKS = [0x10266d20,0x10266d18,0x10266cd0,0x10266d10,0x10266d08,0x10266d00,0x10266cf8,0x10266cf0,0x10266ce8,0x10266ce0]
NODES = [0,0x1034ca80,0x1034ca68]+[o+0x24 for o in OBJECTS]
FIELDS = [(0x40,'force',0),(0x38,'formation_pos',0),(0x20,'local_xaxis',0),(0x28,'local_yaxis',0),
 (0,'mass',1),(0x18,'max_force',1),(0x1c,'max_speed',1),(0xb8,'mCantReachDest',2),(0xb4,'nav_mode',5),
 (0x58,'path_buffer_count',2),(0x5c,'path_count',2),(0x60,'path_finished',3),(0xac,'path_last_progress',2),
 (0xb0,'path_stuck_buffer',4),(0xa4,'path_subpath_dist',2),(0xa8,'path_tail_dist',2),(0x4c,'path_target_pos',0),
 (0x48,'path_timestamp',4),(0x54,'path_total_count',1),(0xa0,'path_total_dist',2),(8,'pos',0),(4,'radius',1)]
for p in range(5):
    FIELDS.extend([(0x64+8*p,'subpath_buffer[p]',0),(0x8c+4*p,'subpath_dist[p]',2)])
FIELDS.append((0x10,'vel',0))
FILE = 0x12345678


def setup(runner):
    T.install_std_stubs(runner.emu)


def bind(lib):
    if getattr(lib,'vsr_bound',False):
        return
    specs = {'alloc':(C.c_uint64,[C.c_uint32]), 'addr':(C.c_uint64,[C.c_int]),
      'object':(C.c_uint64,[C.c_int]), 'node':(C.c_uint64,[C.c_int]), 'type':(C.c_int,[C.c_int]),
      'prepare':(None,[C.c_uint64,C.c_uint64]), 'file':(None,[C.c_uint64,C.c_uint32,C.c_uint64]),
      'serialise':(C.c_uint32,[C.c_uint64,C.c_uint64,C.c_int32,C.c_int32,C.c_uint32]),
      'template':(C.c_uint32,[C.c_int,C.c_uint64,C.c_uint64,C.c_char_p,C.c_int32]),
      'init':(None,[]), 'layout':(C.c_int,[C.POINTER(C.c_int64)])}
    for name,(rest,args) in specs.items():
        f = getattr(lib,'tpw_vsr_'+name)
        f.restype,f.argtypes = rest,args
    out = (C.c_int64*16)()
    n = lib.tpw_vsr_layout(out)
    assert list(out)[:n] == [0xd0,0xc8,16,24,40,8],list(out)[:n]
    lib.vsr_bound = True


class Side:
    def write(self,a,b):
        if self.e:
            self.e.write(a,b)
        else:
            C.memmove(a,b,len(b))

    def read(self,a,n):
        return self.e.read(a,n) if self.e else C.string_at(a,n)

    def alloc(self,n):
        return self.e.alloc(n) if self.e else self.lib.tpw_vsr_alloc(n)

    def word(self,a):
        return int.from_bytes(self.read(a,4),self.endian)

    def put(self,a,w,n=4):
        self.write(a,(w & ((1<<(8*n))-1)).to_bytes(n,self.endian))

    def ptr(self,a):
        return int.from_bytes(self.read(a,self.ps),self.endian)

    def label(self,p):
        return 0 if p == 0 else self.labels.get(p,-1)

    def block(self,n,r):
        a = self.alloc(n+32)+16
        self.write(a-16,r.randbytes(16))
        self.write(a+n,r.randbytes(16))
        self.guarded.append((a,n))
        return a

    def type(self,i):
        return VTABLES.index(self.word(self.objects[i])) if self.e else self.lib.tpw_vsr_type(i)

    def rec(self,name,ints=(),texts=()):
        self.L.rec(name,ints,texts)

    def snapshot(self,callback,object,node):
        self.rec('register',(self.label(object),callback,self.label(node)))
        if callback == 1:
            data,top,end = [self.ptr(self.globals[3]+i*self.ps) for i in range(3)]
            self.rec('list_snapshot',(self.label(data),(top-self.data)//self.es,(end-self.data)//self.es))
        elif callback == 2:
            self.rec('sum_snapshot',(self.word(self.globals[2]),self.word(self.globals[2]+4)))
        else:
            index = self.objects.index(object)
            self.rec('object_snapshot',(self.label(object),self.type(index)),(self.read(object+self.nameoff,32).hex(),))
        self.rec('node_before',(self.label(node),),(self.read(node,12).hex(),))
        self.write(node,bytes([0x80+callback])*12)

    def __init__(self,e,lib,c):
        self.e,self.lib,self.c = e,lib,c
        if lib:
            bind(lib)
        self.endian,self.ps,self.es,self.nameoff = ('big',4,8,4) if e else ('little',8,16,8)
        self.guarded,self.failed = [],[]
        r = random.Random(c['seed'])
        self.globals = GLOBALS if e else [lib.tpw_vsr_addr(i) for i in range(8)]
        self.raw = self.block(0x58,r)
        self.data = self.block(10*self.es,r)
        self.write(self.raw,r.randbytes(0x58))
        self.raw_before = self.read(self.raw,0x58)
        # The array fake leaves entries untouched; each entry is mapped independently.
        for i in range(10):
            self.put(self.data+i*self.es,0x76540000+i)
            self.put(self.data+i*self.es+(4 if e else 8),0,self.ps)
            if not e:
                self.write(self.data+i*self.es+4,b'\xa1\xb2\xc3\xd4')
        if lib:
            lib.tpw_vsr_prepare(self.raw,self.data)
        self.objects = OBJECTS if e else [lib.tpw_vsr_object(i) for i in range(7)]
        self.nodes = NODES if e else [0]+[lib.tpw_vsr_node(i) for i in range(1,10)]
        for i,o in enumerate(self.objects):
            self.write(o+self.nameoff,r.randbytes(32))
            if e:
                self.put(o,0xdead0000+i)
        for node in self.nodes[1:]:
            self.write(node,r.randbytes(12))
        self.veh = self.block(0xc8 if e else 0xd0,r)
        self.target = self.block(8,r)
        self.write(self.target,r.randbytes(8))
        for off in range(0,0xc8,4):
            self.put(self.veh+off,(r.getrandbits(24)<<8) | off)
        self.write(self.veh+0x60,bytes([c.get('flag',2),0xa1,0xb2,0xc3]))
        self.put(self.veh+0x58,c.get('count',0))
        self.put(self.veh+0x54,c.get('count',0))
        self.put(self.veh+0x30,self.target if e else 0xaabbccdd)
        self.dead_before = self.word(self.veh+0x30)
        if lib:
            self.put(self.veh+0xc8,self.target,8)
        self.value = self.block(8,r)
        for i,w in enumerate(c.get('words',[0x12345678,0xabcdef01])):
            self.put(self.value+i*4,w)
        for i,g in enumerate(self.globals[:2]):
            self.put(g,0xabcdef00+i)
        self.put(self.globals[2],0x11223344)
        self.put(self.globals[2]+4,0xaabbccdd)
        self.put(self.globals[6],0xfedcba98)
        self.put(self.globals[7],0x76543210)
        for i in range(3):
            self.put(self.globals[3]+i*self.ps,self.raw,self.ps)
        self.labels = {FILE:1,self.veh:2,self.target:3,self.raw:4,self.data:5,self.globals[3]:6,self.globals[2]:7,self.globals[4]:8}
        self.labels.update({o:20+i for i,o in enumerate(self.objects)})
        self.labels.update({node:40+i for i,node in enumerate(self.nodes) if i})
        self.file = c['file']
        self.cursor = 0
        if e:
            self.L = e.cl = CL.PpcLog(self.labels,c['q'])
            e.vsr = self
            self.saved = STUBS.activate(e)
        else:
            CL.host_begin(lib,{v:k for k,v in self.labels.items()},c['q'])
            data = self.alloc(len(self.file))
            self.write(data,self.file)
            lib.tpw_vsr_file(data,len(self.file),self.veh if c['op'] == 0 else 0)

    def normalized(self,a,n):
        values = []
        for off in range(0,n,4):
            p = a+off
            match = next(((dest,length,prefix,before) for dest,length,prefix,before in self.failed if dest <= p < dest+length),None)
            if match:
                dest,length,prefix,before = match
                ix = p-dest
                written = max(0,min(4,prefix-ix))
                raw = self.read(p,4)
                values.append(('raw',raw[:written],raw[written:] == before[ix+written:ix+4]))
            else:
                values.append(self.word(p))
        return values

    def observe(self,answer):
        # Native read failures are reconstructed from the same deterministic status/transfer/prefix queues and file-call order.
        if not self.e:
            self.failed = self.host_failed
        words = [off for off in range(0,0xc8,4) if off not in (0x30,0x60)]
        normalized = self.normalized(self.veh,0xc8)
        vehicle = [normalized[off//4] for off in words]
        guards = [self.read(a-16,16)+self.read(a+n,16) for a,n in self.guarded]
        entries = [(self.word(self.data+i*self.es),self.label(self.ptr(self.data+i*self.es+(4 if self.e else 8)))) for i in range(10)]
        pointers = [self.ptr(self.globals[3]+i*self.ps) for i in range(3)]
        header = [(p-self.data)//self.es for p in pointers] if self.c['op'] == 5 else [self.label(p) for p in pointers]
        log = self.L.log if self.e else CL.host_end(self.lib)
        return dict(answer=answer,vehicle=vehicle,flag=self.read(self.veh+0x60,4),target=self.label(self.word(self.veh+0x30) if self.e else self.ptr(self.veh+0xc8)),
          dead_state=0 if self.word(self.veh+0x30)==0 else 1 if self.word(self.veh+0x30)==self.dead_before else 2,
          target_bytes=self.read(self.target,8),
          value=self.normalized(self.value,8),guards=guards,entries=entries,header=header,
          raw_unchanged=self.read(self.raw,0x58)==self.raw_before,
          native_padding_unchanged=self.e is not None or all(self.read(self.data+i*self.es+4,4)==b'\xa1\xb2\xc3\xd4' for i in range(10)),
          avoidance=[self.word(self.globals[6]),self.word(self.globals[7])],
          zero=self.word(self.globals[0]),one=self.word(self.globals[1]),sum=[self.word(self.globals[2]),self.word(self.globals[2]+4)],
          names=[self.read(o+self.nameoff,32) for o in self.objects],
          types=[self.type(i) for i in range(7)] if self.c['op']==5 else [],
          table=[self.label(self.ptr(self.globals[5]+i*self.ps)) for i in range(8)],nodes=[self.read(p,12) for p in self.nodes[1:]],log=log)


STUBS = CL.Stubs()
STUBS.real.update(OWN)


def scalar(kind,save):
    def fake(e):
        s,L = e.vsr,e.cl
        p = e.r(4)
        width = 1 if kind == 3 else 4
        word = int.from_bytes(e.read(p,width),'big')
        answer,loaded = T.s32(L.ret('scalar_result')),L.ret('loaded') & MASK
        if not save:
            e.write(p,(loaded & ((1<<(width*8))-1)).to_bytes(width,'big'))
        L.rec('scalar',(kind,int(save),s.label(e.r(3)),p-s.veh,T.s32(e.r(6)),word,word if save else loaded & ((1<<(width*8))-1),answer),(e.cstr(e.r(5)),))
        e.set_r(3,answer & MASK)
    return fake


for kind,load,save in [(1,0x1000ee04,0x1000f384),(2,0x1000ef18,0x1000f4b0),(3,0x1000f144,0x1000f70c),(4,0x1000f02c,0x1000f5dc)]:
    STUBS.internal(load,scalar(kind,False))
    STUBS.internal(save,scalar(kind,True))
STUBS.internal(0x1000dde8,CL.recorder('assert','is'))
STUBS.internal(0x1015b3dc,CL.recorder('log','Li si'.replace(' ','')))


def file_fake(save):
    def fake(e):
        s,L = e.vsr,e.cl
        result,buf,length = e.r(3),e.r(5),e.r(6)
        status = T.s32(L.ret('status'))
        transfer = L.ret('transfer',length) & MASK
        if save:
            L.rec('write',(s.label(e.r(4)),length,status,transfer),(e.read(buf,length).hex(),))
        else:
            prefix = min(length,L.ret('prefix',length)&MASK)
            L.rec('read',(s.label(e.r(4)),buf-s.veh if s.c['op']==0 else 0,length,status,transfer,prefix))
            before = e.read(buf,length)
            e.write(buf,s.file[s.cursor:s.cursor+prefix])
            s.cursor += length
            if status < 0 or transfer != length:
                s.failed.append((buf,length,prefix,before))
        s.put(e.r(7),transfer)
        s.put(result,status)
    return fake
STUBS.imp('LbFile_Read__FPvPvUlPUl',file_fake(False))
STUBS.imp('LbFile_Write__FPvPCvUlPUl',file_fake(True))


def allocate(e):
    s = e.vsr
    s.rec('allocate',(e.r(3),s.word(s.globals[0]),s.word(s.globals[1])))
    e.set_r(3,s.raw)


def construct(e):
    s = e.vsr
    s.rec('construct',(s.label(e.r(3)),CALLBACKS.index(e.r(4)),s.label(e.r(5)),e.r(6),e.r(7)))
    e.set_r(3,s.data)


def register(e):
    s = e.vsr
    s.snapshot(CALLBACKS.index(e.r(4)),e.r(3),e.r(5))


def copy_name(e):
    s = e.vsr
    obj = e.r(3)-4
    text = e.cstr(e.r(4))
    index = s.objects.index(obj)
    s.rec('strcpy',(s.label(obj),4,s.type(index)),(text,))
    e.write(e.r(3),text.encode()+b'\0')
STUBS.imp('__nwa__FUl',allocate)
STUBS.imp('__construct_new_array',construct)
STUBS.imp('__register_global_object',register)
STUBS.imp('strcpy',copy_name)


def host_failure_regions(s,c):
    """Track failed file destinations from real helper invocation indexes, retaining actual pre-read memory bytes."""
    s.host_failed = []
    if c['op'] == 5 or c.get('save',0):
        return
    fields = FIELDS if c['op']==0 and c['version'] in (0,1) else [(0,'',0 if c['op'] in (1,2) else 5)] if c['op'] != 0 else []
    q = c['q']
    fi,si = 0,0
    def queued(key,index,default):
        values=q.get(key,[])
        return values[min(index,len(values)-1)] if values else default
    for offset,name,kind in fields:
        if kind in (0,5):
            length = 8 if kind==0 else 4
            status = T.s32(queued('status',fi,0))
            transfer = queued('transfer',fi,length)&MASK
            prefix = min(length,queued('prefix',fi,length)&MASK)
            if status < 0 or transfer != length:
                address = (s.veh+offset) if c['op']==0 else s.value
                s.host_failed.append((address,length,prefix,s.read(address,length)))
                break
            fi+=1
        else:
            if queued('scalar_result',si,0):
                break
            si+=1


def execute(s,c):
    op = c['op']
    if not s.e:
        host_failure_regions(s,c)
    if s.e:
        if op==0:
            args=[s.veh,FILE,c['save']&MASK,c['version']&MASK,c['unused']]
        elif op==5:
            args=[]
        else:
            name=s.alloc(len(c['name'])+1)
            s.write(name,c['name'].encode()+b'\0')
            args=[FILE,s.value,name,c['nolog']&MASK]
        return T.call_ppc(s.e,ENTRY[op],args)[0]&MASK if op!=5 else (T.call_ppc(s.e,ENTRY[op],args) and 0)
    if op==0:
        return s.lib.tpw_vsr_serialise(s.veh,FILE,c['save'],c['version'],c['unused'])
    if op==5:
        s.lib.tpw_vsr_init()
        return 0
    return s.lib.tpw_vsr_template(op-1,FILE,s.value,c['name'].encode(),c['nolog'])


def run(e,lib,c):
    s=Side(e,lib,c)
    try:
        return s.observe(execute(s,c))
    finally:
        if e:
            CL.Stubs.deactivate(e,s.saved)


def base(r,op=0):
    return dict(op=op,seed=r.getrandbits(32),file=r.randbytes(512),q={'loaded':[r.getrandbits(32) for _ in range(20)]},
      save=0 if op in (2,3) else 1,version=0,unused=r.getrandbits(32),flag=r.choice([0,1,2,255]),count=r.choice([0,-1,0x7fffffff]),
      words=[r.getrandbits(32),r.getrandbits(32)],name=r.choice(['','force','nav_mode','n'*150]),nolog=0)


def fail(c,index,answer):
    _,_,kind=FIELDS[index]
    fi=sum(k in (0,5) for _,_,k in FIELDS[:index])
    si=index-fi
    if kind in (0,5):
        c['q']['status']=[0]*fi+[answer]
        if answer > 0:
            c['q']['transfer']=[8 if k==0 else 4 for _,_,k in FIELDS[:index] if k in (0,5)]+[7 if kind==0 else 3]
        c['q']['prefix']=[8 if k==0 else 4 for _,_,k in FIELDS[:index] if k in (0,5)]+[3 if kind==0 else 2]
    else:
        c['q']['scalar_result']=[0]*si+[answer]
    return c


def ser_edges():
    r=random.Random(230)
    cases=[]
    for save in (0,1,-1,2,-0x80000000):
        for version in (-0x80000000,-1,0,1,2,0x7fffffff):
            cases.append(base(r)|dict(save=save,version=version))
    for save in (0,1):
        for version in (0,1):
            for flag in (0,1,2,255):
                for count in (0,-1,0x7fffffff):
                    c=base(r)|dict(save=save,version=version,flag=flag,count=count)
                    c['q']['loaded'][6]=flag
                    cases.append(c)
            for answer in (-7,9):
                for index in range(33):
                    cases.append(fail(base(r)|dict(save=save,version=version),index,answer))
            cases.append(base(r)|dict(save=save,version=version))
    return cases


def ser_random(r):
    c=base(r)|dict(save=r.choice([0,1,-1,2,-0x80000000]),version=r.choice([0,0,1,1,-1,2]))
    return fail(c,r.randrange(33),r.choice([-3,5])) if r.random()<0.6 else c


def template_edges(op):
    r=random.Random(230+op)
    size=8 if op in (1,2) else 4
    cases=[]
    for nolog in (0,1,-1,2,0x100):
        for status in (-0x80000000,-1,0,1,0x7fffffff):
            for transfer in (0,size-1,size,size+1,MASK):
                for prefix in (0,1,size-1,size):
                    if status>=0 and transfer==size and prefix != size:
                        continue
                    c=base(r,op)|dict(nolog=nolog)
                    c['q'].update(status=[status],transfer=[transfer],prefix=[prefix])
                    cases.append(c)
    for word in (0,1,0x7fffffff,0x80000000,MASK,0x12345678):
        cases.append(base(r,op)|dict(words=[word,word^0xaabbccdd]))
    return cases


def template_random(r,op):
    c=base(r,op)|dict(nolog=r.choice([0,1,-1,2,0x100]))
    size=8 if op in (1,2) else 4
    status=r.choice([-1,0,1,0x7fffffff])
    transfer=r.choice([0,size-1,size,size,size+1,MASK])
    prefix=size if status>=0 and transfer==size else r.randrange(size+1)
    c['q'].update(status=[status],transfer=[transfer],prefix=[prefix])
    return c


def init_edges():
    return [base(random.Random(i),5) for i in range(20)]


def test(name,op,edge,rnd):
    return FnTest(name,lambda e,c:run(e,None,c),lambda lib,c:run(None,lib,c),edge=edge,rnd=rnd)
TESTS=[test('Vehicle::Serialise',0,ser_edges,ser_random)]
for op,name in enumerate(['SAVEVARSTRUCT<Vector2d>','LOADVARSTRUCT<Vector2d>','LOADVAR<NavMode>','SAVEVAR<NavMode>'],1):
    TESTS.append(test(name,op,lambda op=op:template_edges(op),lambda r,op=op:template_random(r,op)))
TESTS.append(test('__sinit_vehicle_cpp',5,init_edges,lambda r:base(r,5)))
