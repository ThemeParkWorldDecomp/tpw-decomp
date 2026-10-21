#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "DrawState.h"
#include "LbDraw.h"
#include "LbDrawPrimitives.h"
#include "LbDrawText.h"
#include "LbPalette.h"
#include "LbRect.h"
#include "LbSurface.h"
#include "RenderContext8.h"
#include "RenderContextDepthCircles.h"

struct TbPalette { uint8_t bytes[1024]; };
TbPalette _lbGlobalPalette{};
TbRenderArea* _lbpCurrentRA{};
TbRect _lbCurrentViewPort{};
TbRect _lbCurrentClipRect{};
uint8_t* _lbCurrentOriginPtr{};
RenderContext* _lbpRenderBase{};
RenderContext _lbRenderContext8{8};
RenderContext _lbRenderContext16{16};
RenderContext _lbRenderContext24{24};
RenderContext _lbRenderContext32{32};
TbPixelCaps _lbCurrentPixelCaps{};
uint32_t _lbCurrentDrawFlags{};
uint8_t* _lbCurrentGhostTable{};
uint8_t* _lbpCurrentFadeTable{};
uint8_t* _lbpCurrentFadeRow{};
int32_t gLbSpriteDrawState[10]{};

namespace {
struct Event { int32_t op; int64_t a[8]; };
Event events[1024];
uint32_t eventCount;

void Add(int32_t op, int64_t a=0, int64_t b=0, int64_t c=0, int64_t d=0,
         int64_t e=0, int64_t f=0, int64_t g=0, int64_t h=0) {
    events[eventCount++] = {op, {a,b,c,d,e,f,g,h}};
}
// The first eight characters of a text, big-endian, zero-padded.
int64_t TextWord(const char* t) {
    uint64_t w=0; bool end=false;
    for(int i=0;i<8;++i){ uint8_t ch=end?0:uint8_t(t[i]); if(!ch) end=true; w=w<<8|ch; }
    return int64_t(w);
}
int64_t ColourWord(TbColour c) {
    return int64_t(c.mAlpha) << 32 | int64_t(c.mRed) << 24 |
           int64_t(c.mGreen) << 16 | int64_t(c.mBlue) << 8 | c.mIndex;
}
}

TbColour::TbColour() : mAlpha(0), mRed(0), mGreen(0), mBlue(0), mIndex(0) {}

void LbRect_Normalise(TbRect* r) {
    if (r->left > r->right) { int32_t x=r->left; r->left=r->right; r->right=x; }
    if (r->top > r->bottom) { int32_t x=r->top; r->top=r->bottom; r->bottom=x; }
}
int32_t LbRect_Width(const TbRect* r) { return r->right-r->left; }
int32_t LbRect_Height(const TbRect* r) { return r->bottom-r->top; }
void LbDraw_SetViewPort(const TbRect* r) { Add(100, r ? r->left : -1); }
void LbDraw_SetClipRect(const TbRect* r) { Add(101, r ? r->left : -1); }
uint8_t LbPalette_FindColour(const TbPalette*, uint8_t r, uint8_t g, uint8_t b) {
    Add(102,r,g,b); return static_cast<uint8_t>(r ^ g ^ b);
}
void TbPixelCaps::Set(const TbPixFormat& f) {
    mRed.Set(f.mRedMask,23); mGreen.Set(f.mGreenMask,15);
    mBlue.Set(f.mBlueMask,7); mAlpha.Set(f.mAlphaMask,31);
}
void TbMaskInfo::Set(uint32_t mask, uint32_t top) {
    mMask=mask; mShiftRight=0; mShiftLeft=static_cast<int16_t>(top);
}
const TbRenderArea& TbSurface::GetRenderArea() const { return mArea; }
// The library's out-of-line constructor (0x10016630, unit w21's src/bullfrog/TbRenderArea.cpp): the masks are left alone.
TbRenderArea::TbRenderArea() { mDepth=0; mPalettised=0; mData=nullptr; mWidth=0; mPictureHeight=0; mStride=0; }
TbResult TbRenderArea::SetupClientRA(TbRenderArea& out, const TbRect& r) const {
    out=*this; out.mData=mData+r.top*mStride+r.left*((mDepth+7)/8);
    out.mWidth=r.right-r.left; out.mPictureHeight=r.bottom-r.top; return {0};
}

#define METHODS(Class, base) \
void Class::LbDraw_Pixel(int32_t x,int32_t y,TbColour c){Add(base+0,x,y,ColourWord(c));}\
void Class::LbDraw_Rectangle(const TbRect&r,TbColour c){Add(base+1,r.left,r.top,r.right,r.bottom,ColourWord(c));}\
void Class::LbDraw_RectangleOutline(const TbRect&r,TbColour c){Add(base+2,r.left,r.top,r.right,r.bottom,ColourWord(c));}\
void Class::LbDraw_Line(int32_t x,int32_t y,int32_t a,int32_t b,TbColour c){Add(base+3,x,y,a,b,ColourWord(c));}\
void Class::LbDraw_HorizontalLine(int32_t x,int32_t y,int32_t n,TbColour c){Add(base+4,x,y,n,ColourWord(c));}\
void Class::LbDraw_VerticalLine(int32_t x,int32_t y,int32_t n,TbColour c){Add(base+5,x,y,n,ColourWord(c));}\
void Class::LbDraw_Circle(int32_t x,int32_t y,uint32_t r,TbColour c){Add(base+6,x,y,r,ColourWord(c));}\
void Class::LbDraw_CircleOutline(int32_t x,int32_t y,uint32_t r,TbColour c){Add(base+7,x,y,r,ColourWord(c));}\
void Class::LbDraw_CircleFilled(int32_t x,int32_t y,uint32_t r,TbColour c){Add(base+8,x,y,r,ColourWord(c));}\
void Class::LbDraw_Triangle(int32_t x,int32_t y,int32_t a,int32_t b,int32_t d,int32_t e,TbColour c){Add(base+9,x,y,a,b,d,e,ColourWord(c));}\
void Class::LbDraw_ClearClipWindow(TbColour c){Add(base+10,ColourWord(c));}\
void Class::LbDraw_RectangleFilled(const TbRect&r,TbColour c){Add(base+11,r.left,r.top,r.right,r.bottom,ColourWord(c));}\
void Class::LbDraw_PropText(int32_t x,int32_t y,const char*t,TbColour c){Add(base+12,x,y,TextWord(t),ColourWord(c));}\
void Class::LbDraw_UnicodePropText(int32_t x,int32_t y,const char16_t*t,TbColour c){Add(base+13,x,y,reinterpret_cast<uintptr_t>(t),ColourWord(c));}\
void Class::LbDraw_Sprite(int32_t x,int32_t y,const TbSprite*s){Add(base+14,x,y,reinterpret_cast<uintptr_t>(s));}\
void Class::LbDraw_OneColourSprite(int32_t x,int32_t y,const TbSprite*s,TbColour c){Add(base+15,x,y,reinterpret_cast<uintptr_t>(s),ColourWord(c));}

METHODS(RenderContext8, 0)
METHODS(RenderContext16, 20)
METHODS(RenderContext24, 40)
METHODS(RenderContext32, 60)
#undef METHODS

extern "C" {
uint32_t tpw_ldp_event_count() { return eventCount; }
const Event* tpw_ldp_events() { return events; }
int32_t tpw_ldp_last_op() { return eventCount ? events[eventCount-1].op : -1; }
void tpw_ldp_reset() { eventCount=0; std::memset(events,0,sizeof(events)); }

void tpw_ldp_set_state(int32_t depth, TbRenderArea* area, const TbRect* clip,
                       uint8_t* origin, uint32_t flags, uint8_t* ghost) {
    _lbpCurrentRA=area; _lbCurrentClipRect=*clip; _lbCurrentOriginPtr=origin;
    _lbCurrentDrawFlags=flags; _lbCurrentGhostTable=ghost;
    _lbpRenderBase=depth==16?&_lbRenderContext16:depth==24?&_lbRenderContext24:
                   depth==32?&_lbRenderContext32:&_lbRenderContext8;
}

void tpw_ldp_compact(uint8_t*d,const uint8_t*p,uint32_t n){LbPalette_ToCompact(d,reinterpret_cast<const TbPalette*>(p),n);}
void tpw_ldp_remap(int32_t x,int32_t y,const uint8_t*data,uint16_t w,uint16_t h,const uint8_t*map){TbSprite s{data,w,h};LbDraw_RemappedSprite(x,y,&s,map);}

void tpw_ldp_draw(int32_t op,const int32_t*v,const TbRect*r,const void*p,const TbColour*c){
    switch(op){
    case 0:LbDraw_Pixel(v[0],v[1],*c);break;
    case 1:LbDraw_Rectangle(r,*c);break; case 2:LbDraw_Rectangle(*r,*c);break;
    case 3:LbDraw_RectangleOutline(r,*c);break; case 4:LbDraw_RectangleOutline(*r,*c);break;
    case 5:LbDraw_Line(v[0],v[1],v[2],v[3],*c);break;
    case 6:LbDraw_HorizontalLine(v[0],v[1],v[2],*c);break;
    case 7:LbDraw_VerticalLine(v[0],v[1],v[2],*c);break;
    case 8:LbDraw_Circle(v[0],v[1],uint32_t(v[2]),*c);break;
    case 9:LbDraw_CircleOutline(v[0],v[1],uint32_t(v[2]),*c);break;
    case 10:LbDraw_CircleFilled(v[0],v[1],uint32_t(v[2]),*c);break;
    case 11:LbDraw_Triangle(v[0],v[1],v[2],v[3],v[4],v[5],*c);break;
    case 12:LbDraw_ClearClipRect(*c);break;
    case 13:LbDraw_RectangleFilled(r,*c);break; case 14:LbDraw_RectangleFilled(*r,*c);break;
    case 15:LbDraw_PropText(v[0],v[1],static_cast<const char*>(p),*c);break;
    case 16:LbDraw_UnicodePropText(v[0],v[1],static_cast<const char16_t*>(p),*c);break;
    case 17:{TbSprite s{static_cast<const uint8_t*>(p),3,2};LbDraw_Sprite(v[0],v[1],&s);break;}
    case 18:{TbSprite s{static_cast<const uint8_t*>(p),3,2};LbDraw_OneColourSprite(v[0],v[1],&s,*c);break;}
    }
}

void tpw_ldp_release(int32_t op){if(op==0)LbDraw_ReleaseViewPort();else LbDraw_ReleaseClipRect();}
TbRect* tpw_ldp_view(){return LbDraw_GetViewPort();}
void tpw_ldp_scale(int32_t op,int32_t x,int32_t y,const uint8_t*d,uint16_t w,uint16_t h,uint32_t dw,uint32_t dh){TbSprite s{d,w,h};if(op)LbDraw_ScaledSprite(x,y,&s,dw,dh);else LbDraw_SetSpriteScalingData(x,y,w,h,dw,dh);}
int32_t* tpw_ldp_scale_state(){return gLbSpriteDrawState;}

// ---- CDraw (Draw_state.cpp), LbDraw_SetCurrent and LbDraw_Palette
alignas(16) static unsigned char gDrawStore[sizeof(bfl::draw::CDraw)];
static bfl::draw::CDraw& Draw(){return *reinterpret_cast<bfl::draw::CDraw*>(gDrawStore);}
void tpw_cd_construct(){std::memset(gDrawStore,0xA5,sizeof(gDrawStore));new(gDrawStore) bfl::draw::CDraw();}
int32_t tpw_cd_destroy(int32_t flag){Draw().~CDraw();return int16_t(flag)>0;}
void* tpw_cd_field(int32_t which){
    auto& d=Draw();
    switch(which){case 0:return &d.mRenderContext;case 1:return &d.mSurface;case 2:return &d.mCurrent;
    case 3:return &d.mViewPort;case 4:return &d.mClipRect;default:return &d.mPaletteDirty;}
}
void* tpw_cd_context(int32_t depth){
    using namespace bfl::draw;
    return depth==8?&gDrawContext8:depth==16?&gDrawContext16:depth==24?&gDrawContext24:&gDrawContext32;
}
void tpw_cd_set_area(TbRenderArea* a){Draw().SetCurrent(a);}
void tpw_cd_set_surface(const TbRenderArea* a){
    if(!a){Draw().SetCurrent(static_cast<TbSurface*>(nullptr));return;}
    static TbSurface surface; surface.mArea=*a; Draw().SetCurrent(&surface);
}
void tpw_cd_rect_op(int32_t op,const TbRect* r){
    if(op==0)Draw().SetClipRect(*r);else if(op==1)Draw().SetViewPort(*r);
    else if(op==2)Draw().ClearClipRect();else Draw().ClearViewPort();
}
int32_t tpw_cd_get(int32_t which){
    auto& d=Draw();
    if(which==0)return &d.GetCurrent()==&d.mCurrent?8:-1;
    if(which==1)return &d.GetClipRect()==&d.mClipRect?0x58:-1;
    return &d.GetViewPort()==&d.mViewPort?0x30:-1;
}
// LbDraw_SetCurrent: the surface's four words and area; answers the globals it sets.
static TbSurface gSurface;
void tpw_ldp_set_current(int32_t null,const uint32_t* words,const TbRenderArea* a){
    tpw_ldp_reset();
    if(null){LbDraw_SetCurrent(static_cast<TbSurface*>(nullptr));return;}
    gSurface.mDrawFlags=words[0];
    gSurface.mGhostTable=reinterpret_cast<uint8_t*>(uintptr_t(words[1]));
    gSurface.mFadeTable=reinterpret_cast<uint8_t*>(uintptr_t(words[2]));
    gSurface.mFadeRow=reinterpret_cast<uint8_t*>(uintptr_t(words[3]));
    gSurface.mArea=*a;
    LbDraw_SetCurrent(&gSurface);
}
void tpw_ldp_globals(uint64_t* out){
    out[0]=_lbpCurrentRA==nullptr?0:_lbpCurrentRA==&gSurface.mArea?1:2;
    out[1]=uintptr_t(_lbCurrentOriginPtr); out[2]=_lbCurrentDrawFlags; out[3]=uintptr_t(_lbCurrentGhostTable);
    out[4]=uintptr_t(_lbpCurrentFadeTable); out[5]=uintptr_t(_lbpCurrentFadeRow);
    out[6]=_lbpRenderBase==&_lbRenderContext8?8:_lbpRenderBase==&_lbRenderContext16?16:
           _lbpRenderBase==&_lbRenderContext24?24:_lbpRenderBase==&_lbRenderContext32?32:0;
    out[7]=_lbCurrentPixelCaps.mRed.mMask;
}
void tpw_ldp_set_globals(uint64_t base,uint64_t origin,uint32_t flags){
    _lbpRenderBase=base==16?&_lbRenderContext16:base==24?&_lbRenderContext24:base==32?&_lbRenderContext32:
                   base==8?&_lbRenderContext8:nullptr;
    _lbCurrentOriginPtr=reinterpret_cast<uint8_t*>(uintptr_t(origin)); _lbCurrentDrawFlags=flags;
    _lbpCurrentRA=nullptr; _lbCurrentPixelCaps.mRed.mMask=0;
}
uint8_t* tpw_ldp_palette(){return reinterpret_cast<uint8_t*>(&_lbGlobalPalette);}
uint32_t tpw_ldp_flags(){return _lbCurrentDrawFlags;}
void tpw_ldp_palette_draw(int32_t x,int32_t y,uint32_t w,uint32_t h,int32_t labels){LbDraw_Palette(x,y,w,h,labels);}
}

