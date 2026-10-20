// Unit w231: recording imports; all 31 surface bodies and TbMaskInfo::Set are real.
#include <cstddef>
#include <cstring>
#include "LbSurface.h"
#include "LbDraw.h"
#include "LbPalette.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "calllog.h"
using namespace calllog;
using DDRectangle = SamsUtilities::URectangle::LongRect;
struct TbPalette { uint8_t bytes[1024]; };
TbPalette _lbGlobalPalette;
TbRenderArea* _lbpCurrentRA;
namespace {
uint64_t q[24];
unsigned descIndex;
template<class T> T* ptr(unsigned i) { return reinterpret_cast<T*>(q[i]); }
void RectEvent(const char* name, const DDRectangle* p) {
    if (p) ev(name,{1,p->left,p->top,p->right,p->bottom});
    else ev(name,{0});
}
void Snapshot(TbSurface* s) {
    const uint32_t rowOffset=s->mFadeRow ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s->mFadeRow)-q[23]) : 0;
    ev("surface",{lab(s),s->mDrawFlags,lab(s->mGhostTable),lab(s->mFadeTable),rowOffset,
        lab(s->mPalette),s->mF1C,s->mF20,lab(s->mDDSurface),s->mFadeRow==nullptr});
}
void Mutate(unsigned event) {
    if (q[20] != event) return;
    ptr<TbSurface>(19)->mDDSurface = ptr<IDirectDrawSurface>(21);
    _lbpCurrentRA = ptr<TbRenderArea>(13);
    _lbpCurrentFadeTable = ptr<uint8_t>(14);
}
void Fill(DDSURFACEDESC* d) {
    const unsigned i = descIndex < 2 ? descIndex++ : 2;
    std::memcpy(d,ptr<DDSURFACEDESC>(18)+i,sizeof *d);
}
}
int32_t IDirectDrawSurface::GetSurfaceDesc(DDSURFACEDESC* d) {
    ev("desc",{lab(this),d->dwSize});
    if (q[0] == 20 || q[0] == 24) ev("desc_flags",{d->dwFlags});
    if (q[0] == 24) {
        DDSURFACEDESC expected{}; expected.dwSize=108; expected.dwFlags=0x1009;
        ev("desc_zero",{std::memcmp(&expected,d,sizeof expected)==0});
    }
    Snapshot(ptr<TbSurface>(1)); Snapshot(ptr<TbSurface>(2));
    Mutate(1); Fill(d);
    return static_cast<int32_t>(ret("desc"));
}
int32_t IDirectDrawSurface::Lock(DDRectangle* r,DDSURFACEDESC* d,uint32_t flags,NS_MacDoze::InternalHandle* event) {
    ev("lock",{lab(this),lab(r),d->dwSize,flags,lab(event)});
    Mutate(4); Fill(d); return static_cast<int32_t>(ret("lock"));
}
int32_t IDirectDrawSurface::Unlock(void* r) { ev("unlock",{lab(this),lab(r)}); return static_cast<int32_t>(ret("unlock")); }
int32_t IDirectDrawSurface::IsLost() { ev("lost",{lab(this)}); Mutate(5); return static_cast<int32_t>(ret("lost")); }
int32_t IDirectDrawSurface::Restore() { ev("restore",{lab(this)}); Mutate(6); return static_cast<int32_t>(ret("restore")); }
int32_t IDirectDrawSurface::SetColorKey(uint32_t flags,DDCOLORKEY* k) {
    ev("key",{lab(this),flags,k->dwColorSpaceLowValue,k->dwColorSpaceHighValue});
    return static_cast<int32_t>(ret("key"));
}
int32_t IDirectDrawSurface::Blt(DDRectangle* d,IDirectDrawSurface* s,DDRectangle* r,uint32_t flags,DDBLTFX* fx) {
    ev("blt",{lab(this),lab(s),flags}); RectEvent("dest_rect",d); RectEvent("source_rect",r);
    if(q[0]==23) ev("fill",{fx->dwSize,fx->words[19]});
    else {
        const auto* w=reinterpret_cast<const uint32_t*>(fx);
        for(unsigned i=0;i<25;++i) ev("fx",{i,w[i]});
    }
    return static_cast<int32_t>(ret("blt"));
}
int32_t IDirectDrawSurface::BltFast(uint32_t x,uint32_t y,IDirectDrawSurface* s,DDRectangle* r,uint32_t flags) {
    ev("fast",{lab(this),x,y,lab(s),flags}); RectEvent("source_rect",r);
    return static_cast<int32_t>(ret("blt"));
}
int32_t IDirectDraw::CreateSurface(DDSURFACEDESC* d,IDirectDrawSurface** out,NS_MacDoze::IUnknown* outer) {
    ev("create",{lab(this),d->dwSize,d->dwFlags,d->dwHeight,d->dwWidth,d->dwCaps,
        out==&ptr<TbSurface>(1)->mDDSurface,lab(outer)});
    *out=ptr<IDirectDrawSurface>(12);
    // Dirty the entire descriptor to prove Create's subsequent ZeroMemory call.
    std::memset(d,0xa7,sizeof *d);
    return static_cast<int32_t>(ret("create"));
}
uint32_t NS_MacDoze::IUnknown::AddRef() { ev("addref",{lab(this)}); Mutate(2); return static_cast<uint32_t>(ret("addref")); }
uint32_t NS_MacDoze::IUnknown::Release() { ev("release",{lab(this)}); Mutate(3); return static_cast<uint32_t>(ret("release")); }
void NS_MacDoze::ZeroMemory(void* p,uint32_t n) {
    ev("zero",{n==sizeof(DDSURFACEDESC)?108:n}); std::memset(p,0,n);
}
void LbDraw_SetCurrent(TbSurface* s) { ev("current",{lab(s)}); }
TbRect* LbRect_Copy(TbRect* d,const TbRect* s) {
    RectEvent("copy",reinterpret_cast<const DDRectangle*>(s));
    d->left=s->left; d->top=s->top; d->right=s->right; d->bottom=s->bottom; return d;
}
void OS_dd_error(int32_t code) { ev("error",{code}); }
extern "C" {
uint64_t tpw_lsf_alloc(uint32_t n) { return reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(n)); }
uint64_t tpw_lsf_global(unsigned i) {
    const void* p[]={&_lbCurrentDrawFlags,&_lbCurrentGhostTable,&_lbpCurrentFadeTable,&_lbpCurrentFadeRow,
        &gSurfaceSysLockDisabled,&gSurfaceDirectDraw,&_lbpCurrentRA,&_lbGlobalPalette};
    return reinterpret_cast<uintptr_t>(p[i]);
}
unsigned tpw_lsf_layout(uint32_t* out) {
    const uint32_t v[]={sizeof(TbSurface),offsetof(TbSurface,mDrawFlags),offsetof(TbSurface,mGhostTable),
        offsetof(TbSurface,mFadeTable),offsetof(TbSurface,mFadeRow),offsetof(TbSurface,mColour),
        offsetof(TbSurface,mPalette),offsetof(TbSurface,mF1C),offsetof(TbSurface,mF20),offsetof(TbSurface,mDDSurface),
        offsetof(TbSurface,mArea),sizeof(TbRenderArea),offsetof(TbRenderArea,mData),offsetof(TbRenderArea,mWidth),
        offsetof(TbRenderArea,mPictureHeight),offsetof(TbRenderArea,mStride),sizeof(DDSURFACEDESC),
        offsetof(DDSURFACEDESC,lpSurface),offsetof(DDSURFACEDESC,ddckCKDestOverlay),
        offsetof(DDSURFACEDESC,ddpfPixelFormat),offsetof(DDSURFACEDESC,dwCaps)};
    std::memcpy(out,v,sizeof v); return sizeof(v)/sizeof(*v);
}
uint64_t tpw_lsf_run(const uint64_t* config) {
    std::memcpy(q,config,sizeof q); descIndex=0;
    auto* s=ptr<TbSurface>(1); auto* src=ptr<TbSurface>(2);
    auto* dr=ptr<TbRect>(5); auto* sr=ptr<TbRect>(6);
    const uint32_t a=q[9],b=q[10],flags=q[11];
    switch(q[0]) {
    case 0: return reinterpret_cast<uintptr_t>(&LbSurface_MapToRenderArea(*ptr<TbRenderArea>(3),s));
    case 1: return reinterpret_cast<uintptr_t>(&LbSurface_GetPixelFormat(s,*ptr<TbPixFormat>(4)));
    case 2: LbSurface_SetCurrent(s); break;
    case 3: return reinterpret_cast<uintptr_t>(LbSurface_GetCurrent());
    case 4: return reinterpret_cast<uintptr_t>(LbSurface_GetOriginPtr(s));
    case 5: return LbSurface_GetDrawFlags(s);
    case 6: LbSurface_SetDrawFlags(s,a); break;
    case 7: LbSurface_SetDrawFlagsOn(s,a); break;
    case 8: LbSurface_SetDrawFlagsOff(s,a); break;
    case 9: return reinterpret_cast<uintptr_t>(LbSurface_GetFadeTable(s));
    case 10: LbSurface_SetFadeTable(s,ptr<uint8_t>(16)); break;
    case 11: return static_cast<uint32_t>(LbSurface_GetFadeStep(s));
    case 12: LbSurface_SetFadeStep(s,a); break;
    case 13: return LbSurface_GetFadeColour(s,a);
    case 14: return reinterpret_cast<uintptr_t>(LbSurface_GetGhostTable(s));
    case 15: LbSurface_SetGhostTable(s,ptr<uint8_t>(16)); break;
    case 16: s->CalculateSurfaceArea(*ptr<DDSURFACEDESC>(8)); break;
    case 17: return reinterpret_cast<uintptr_t>(&s->GetRenderArea());
    case 18: LbSurface_SetSysLock(a); break;
    case 19: LbSurface_RegisterDirectDraw(ptr<IDirectDraw>(15)); break;
    case 20: return static_cast<uint32_t>(LbSurface_RegisterDirectDrawSurface(ptr<IDirectDrawSurface>(12),s).mCode);
    case 21: _InitialiseSurface(s,a,b,flags,q[22]); break;
    case 22: return _CheckAndRestoreSurface(s);
    case 23: return static_cast<uint32_t>(LbSurface_ClearRect(s,dr,*ptr<TbColour>(17),flags).mCode);
    case 24: return static_cast<uint32_t>(LbSurface_Create(a,b,flags,s).mCode);
    case 25: return static_cast<uint32_t>(LbSurface_Delete(s).mCode);
    case 26: return static_cast<uint32_t>(LbSurface_Blt(s,a,b,src,sr,flags).mCode);
    case 27: return static_cast<uint32_t>(LbSurface_StretchBlt(s,dr,src,sr,flags).mCode);
    case 28: return static_cast<uint32_t>(LbSurface_Lock(s,ptr<uint8_t*>(7)).mCode);
    case 29: LbSurface_Unlock(s); break;
    case 30: return reinterpret_cast<uintptr_t>(LbSurface_GetDDSurface(s,a));
    }
    return 0;
}
}
