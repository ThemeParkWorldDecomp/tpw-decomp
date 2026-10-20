// Unit v51047: std::basic_istream<char>::basic_istream 0x1020a4c0, CUIGrapher::Clear 0x101fd9d0 and its constructor 0x101fd6e8,
// CFSLoadSave::GetNextSaveFileDetails 0x101fcd08, CUI2DMapSprite::~CUI2DMapSprite 0x10230ea4, SetVisibility 0x10230f78 and SetScale
// 0x10231094, the program entry 0x102381a4 and InterfaceKnob::operator- 0x101d038c.
#include <cstddef>
#include <cstdint>

#include "FS_LoadSave.h"
#include "FinanceGraphPanelMsg.h"
#include "TPMain.h"
#include "TPString.h"
#include "TbTList.h"
#include "UI_Base.h"
#include "email.h"
#include "tb_ezsprite.h"
#include "ui_controls.h"
#include "ui_main.h"

namespace {

uint32_t A(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

// The vtable block of CUI2DMapSprite (TOC slot -0x5e64, initial word 0x102b2950).
char gCUI2DMapSpriteVt[0x20];   // 0x102b2958

// ---- std::basic_istream<char>::basic_istream(streambuf*) 0x1020a4c0. The object is the Mac's word layout (MacIfstream); r4 is the
// "with virtual base" flag, tested as a signed halfword.
namespace ifstream_char_callee {
void IstreamCtor(MacIfstream* self, int32_t flag, uint32_t streambuf) {
    if (static_cast<int16_t>(flag) != 0) {
        MacOstreamIos* base = reinterpret_cast<MacOstreamIos*>(reinterpret_cast<uint8_t*>(self) + 0xc);
        self->mIos.set(base);
        base->mVtbl = A(gIosBaseVt);
        base->mVtbl = A(gOstreamVt1);
    }
    self->mVtable = A(gIfstreamVtblC);
    MacOstreamIos* ios = self->mIos.get();
    ios->mVtbl = A(gIfstreamVtblC) + 0xc;
    ios->mVbaseOffset = static_cast<int32_t>(A(reinterpret_cast<uint8_t*>(self) + 0xc) - A(ios));
    self->mGcount = 0;
    ios = self->mIos.get();
    OstreamIosInit(ios, streambuf);
    ios->mF3c = 0;
    ios->mFill = static_cast<uint8_t>(OstreamWiden(ios, ' '));
}
}  // namespace ifstream_char_callee

// ---- CUIGrapher
void CUIGrapher::Clear() {
    TbRect rect;
    rect.left = mF08;
    rect.top = mF0C;
    rect.right = mF10;
    rect.bottom = mF14;
    TbColour colour;
    colour.mAlpha = mBG[0];
    colour.mRed = mBG[1];
    colour.mGreen = mBG[2];
    colour.mBlue = mBG[3];
    colour.mIndex = mBG[4];
    LbDraw_RectangleFilled(&rect, colour);
}

void CUIGrapher_Construct(CUIGrapher* self) {
    for (int i = 0; i < 5; i++) {
        self->mXAxis[i] = 0;
        self->mYAxis[i] = 0;
        self->mGrid[i] = 0;
        self->mBG[i] = 0;
    }
    self->mF00 = 0;
    self->mF04 = 0;
    self->mF2C = 0;
    self->mF28 = 0;
    self->mF24 = 0;
    self->mF20 = 0;
    self->mF1C = 0;
    self->mF18 = 0;
    self->mF14 = 0;
    self->mF10 = 0;
    self->mF0C = 0;
    self->mF08 = 0;
}

// ---- CFSLoadSave::GetNextSaveFileDetails 0x101fcd08: the entry AFTER the first one equal to `previous` (the node after the match is
// read before the comparison), NULL when the match is the last or there is none.
TFSLoadSaveSaveFileDetails* CFSLoadSave::GetNextSaveFileDetails(const TFSLoadSaveSaveFileDetails* previous) {
    CNode* node = mHead;
    while (node != nullptr) {
        TFSLoadSaveSaveFileDetails* data = node->mData;
        node = node->mNext;
        if (TPUniStrEq(previous->mName, data->mName)) break;
    }
    if (node == nullptr) return nullptr;
    return node->mData;
}

// ---- CUI2DMapSprite. The Mac's destructor takes a hidden delete flag; the host's does not (see CUI2DMapSpriteDtor).
CUI2DMapSprite::~CUI2DMapSprite() {
    *reinterpret_cast<uint32_t*>(this) = A(gCUI2DMapSpriteVt);
    auto* list = *reinterpret_cast<TbTList<CUI2DMapSprite*, CUI2DMapSprite*>**>(gUI2DMap.mBytes + 0x20080);
    POSITION position = list->Find(this);
    if (position != nullptr) list->RemoveAt(position);
    gei_EZSprite.DeleteSpriteInstance(mSprite);
}

// The Mac's destructor with its delete flag: NULL is left alone, a flag above 0 (signed halfword) frees the object. Answers this.
CUI2DMapSprite* CUI2DMapSpriteDtor(CUI2DMapSprite* self, int16_t flags) {
    if (self != nullptr) {
        self->~CUI2DMapSprite();
        if (flags > 0) UI2DMapDelete(self);
    }
    return self;
}

void CUI2DMapSprite::SetVisibility(int32_t visible) {
    if (mVisible == visible) return;
    mVisible = visible;
    uint8_t* details = static_cast<uint8_t*>(gei_EZSprite.GetSpriteDetails(mSprite));
    uint32_t flags = *reinterpret_cast<uint32_t*>(details + 0x10);
    if (mVisible != 0)
        flags &= ~1u;
    else
        flags |= 1u;
    *reinterpret_cast<uint32_t*>(details + 0x10) = flags;
}

void CUI2DMapSprite::SetScale(float scale) {
    uint8_t* details = static_cast<uint8_t*>(gei_EZSprite.GetSpriteDetails(mSprite));
    uint8_t* inner = Ptr32<uint8_t>{*reinterpret_cast<uint32_t*>(details + 0x28)}.get();   // a Ptr32 word of the detail block
    float width = static_cast<float>(static_cast<double>(*reinterpret_cast<uint16_t*>(inner + 0xc)));
    float scaled = scale * width;
    *reinterpret_cast<float*>(details + 0x2c) = scaled * 0.0625f;
}

// ---- the program entry
void entry() {
    RuntimeInit();
    ThemeParkWorldMain();
    RuntimeExit(0);
}

// ---- InterfaceKnob::operator- 0x101d038c: the difference of the values when both controls are of one kind, else 0.
int32_t InterfaceKnob::operator-(InterfaceWindow& other) {
    int32_t mine = WhatAmI();
    int32_t theirs = other.WhatAmI();
    if (theirs == mine) return mValue - static_cast<InterfaceKnob&>(other).mValue;
    return 0;
}
