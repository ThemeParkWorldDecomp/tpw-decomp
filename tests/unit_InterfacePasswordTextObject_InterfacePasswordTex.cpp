// Test library for the number-window text objects' constructors and destructors, UIMenu::UIMenu and small functions (unit v51115;
// libtpw_unit_InterfacePasswordTextObject_InterfacePasswordTex.dylib, tests/test_InterfacePasswordTextObject_InterfacePasswordTex.py), main app:
//   src/ui_TextObjects_InterfacePasswordTextObject_InterfacePasswordTex.cpp
//   src/ui_DrawObjectText_ctor.cpp (the text sprite's real constructor)
//
// The callees outside the unit (the sprite constructor, the formatter, the sprite list, the engine data and the draw object destructors, the
// cursor position text object's constructor, the mesh render calls, the ride preview) are recording fakes. The classes between (draw object,
// sprite, text sprite) are defined here with empty bodies so that the vtables link.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#define TPW_UIMENU_CTOR_EXTERNAL
#include "calllog.h"
#include "platform/arena.h"
#include "ui_DrawObject.h"
#include "FS_OnlineLoginPanel.h"
#include "FillLenderList.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_menu.h"
#include "ui_mainmenu.h"
#include "MainWindowCallback.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"

using namespace calllog;

uint32_t gUIObjectStateColours[27];
uint32_t gUIObjectStateStrings[27];

namespace {
bool gMute = false;
const void* gObj = nullptr;
const void* gFmt = nullptr;
const void* gList = nullptr;
const void* gEngine = nullptr;
const void* gMesh = nullptr;

int64_t Lab(const void* p) {
    if (p == gObj) return 1;
    if (p == gFmt) return 2;
    if (p == gList) return 3;
    if (p == gEngine) return 4;
    if (p == gMesh) return 5;
    return -1;
}

void Ev(const char* name, std::initializer_list<int64_t> ints) {
    if (!gMute) ev(name, ints);
}
}  // namespace

// ---- the draw object chain, empty
InterfaceDrawObject::InterfaceDrawObject(void* data) { mData = data; }
InterfaceDrawObject::~InterfaceDrawObject() { Ev("DrawObjectDtor", {Lab(this)}); }
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

InterfaceDrawObjectSprite::InterfaceDrawObjectSprite(void* data) : InterfaceDrawObject(data) {
    Ev("SpriteCtor", {Lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(data))});
}
InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite() {}
void InterfaceDrawObjectSprite::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceDrawObjectSprite::Show(int16_t) {}
void InterfaceDrawObjectSprite::Refresh() {}
int32_t InterfaceDrawObjectSprite::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
void InterfaceDrawObjectSprite::UpdateTextures(char16_t*) {}
void InterfaceDrawObjectSprite::CreateTextures(char16_t*) {}

InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}
void InterfaceTextObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceTextObjectSprite::UpdateTextures(char16_t*) {}

InterfaceCursorPosTextObjectSprite::InterfaceCursorPosTextObjectSprite(void* data)
    : InterfaceTextObjectSprite((gMute = true, data)) {
    gMute = false;
    Ev("CursorPosCtor", {Lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(data))});
}
InterfaceCursorPosTextObjectSprite::~InterfaceCursorPosTextObjectSprite() {}
intptr_t InterfaceCursorPosTextObjectSprite::Query(uint32_t, intptr_t) { return 0; }

void InterfaceVisitorTimeTextObject::Change(uint32_t, DOChange*) {}

int32_t UIMenu_DefaultItemCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
uint32_t UIMenuHandlerHandle(int32_t (*handler)(InterfaceWindow*, int32_t, int32_t, int32_t)) {
    return handler == UIMenu_DefaultItemCallback ? 0x102677d8u : 0u;
}

// ---- the members' fakes
SSpriteGameData::SSpriteGameData() {}
SSpriteEngineData::SSpriteEngineData() {}
SSpriteEngineData::~SSpriteEngineData() { Ev("EngineDataDtor", {Lab(this)}); }
CSpriteList::CSpriteList() {}
CMapLinkBase::~CMapLinkBase() {}
int32_t CSpriteList::Prepare() { return 0; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }
CSpriteList::~CSpriteList() { Ev("SpriteListDtor", {Lab(this)}); }
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRender::TbFontRender() {}
TbFontRender::~TbFontRender() {}
TbTextFormatter::TbTextFormatter() { Ev("FormatterCtor", {Lab(this)}); }
TbTextFormatter::~TbTextFormatter() { Ev("FormatterDtor", {Lab(this)}); }

int32_t DMEnableMeshRender(tag_sDMeshSet* set) {
    Ev("DMEnable", {Lab(set)});
    return 0;
}
int32_t DMDisableMeshRender(tag_sDMeshSet* set) {
    Ev("DMDisable", {Lab(set)});
    return 0;
}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 0; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }

// The bar's vtable is emitted by the unit's source (DrawFunction is its key function): the other members are never called.
InterfaceBar::~InterfaceBar() {}
int32_t InterfaceBar::WhatAmI() { return 0; }
int32_t InterfaceBar::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceBar::SetValue(int32_t) { return 0; }
int32_t InterfaceBar::GetValue() { return 0; }

void UIMain_DestroyRidePreview() { Ev("DestroyRidePreview", {}); }

extern "C" {

uint64_t tpw_vi_addr(const char* name) {
    if (std::strcmp(name, "colours") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gUIObjectStateColours));
    return 0;
}

uint64_t tpw_vi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// which: 0 Password, 1 NullCash, 2 Percent, 3 VisitorNum, 4 VisitorTime, 5 Rank, 6 Cash (unit v5L16). out: the style bytes the constructor sets.
void tpw_vi_label(uint64_t mem);
void tpw_vi_ctor(int32_t which, uint64_t mem, uint32_t data, int64_t* out) {
    tpw_vi_label(mem);
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(mem));
    void* d = reinterpret_cast<void*>(static_cast<uintptr_t>(data));
    InterfaceTextObjectSprite* t = nullptr;
    switch (which) {
        case 0: std::memset(p, 0xAA, sizeof(InterfacePasswordTextObject)); t = new (p) InterfacePasswordTextObject(d); break;
        case 1: std::memset(p, 0xAA, sizeof(InterfaceNullCashTextObject)); t = new (p) InterfaceNullCashTextObject(d); break;
        case 2: std::memset(p, 0xAA, sizeof(InterfacePercentTextObject)); t = new (p) InterfacePercentTextObject(d); break;
        case 3: std::memset(p, 0xAA, sizeof(InterfaceVisitorNumTextObject)); t = new (p) InterfaceVisitorNumTextObject(d); break;
        case 4: std::memset(p, 0xAA, sizeof(InterfaceVisitorTimeTextObject)); t = new (p) InterfaceVisitorTimeTextObject(d); break;
        case 6: std::memset(p, 0xAA, sizeof(InterfaceCashTextObject)); t = new (p) InterfaceCashTextObject(d); break;
        default: std::memset(p, 0xAA, sizeof(InterfaceRankTextObject)); t = new (p) InterfaceRankTextObject(d); break;
    }
    out[0] = static_cast<uint8_t>(t->mJustify);
    out[1] = static_cast<uint8_t>(t->mVJustify);
    out[2] = t->mWrap;
    out[3] = t->mF0DF;
    out[4] = static_cast<uint8_t>(t->mFontId);
    out[5] = t->mColourMode;
    out[6] = static_cast<uint16_t>(t->mOffsetX);
    out[7] = static_cast<uint16_t>(t->mOffsetY);
    out[8] = t->TextColour()[0];
    out[9] = t->TextColour()[1];
    out[10] = t->TextColour()[2];
    out[11] = t->TextColour()[3];
}

// Sets the labelled addresses of an object of the text sprite family.
void tpw_vi_label(uint64_t mem) {
    InterfaceTextObjectSprite* t = reinterpret_cast<InterfaceTextObjectSprite*>(static_cast<uintptr_t>(mem));
    gObj = t;
    gFmt = &t->mFormatter;
    gList = &t->mList;
    gEngine = &t->mEngineData;
}

// which: 0 Cash, 1 NullCash, 2 Percent, 3 VisitorNum, 4 VisitorTime. Answers the object; the delete flag is the caller's (> 0 deletes).
uint64_t tpw_vi_dtor(int32_t which, uint64_t mem, int32_t flag) {
    tpw_vi_label(mem);
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(mem));
    switch (which) {
        case 0: static_cast<InterfaceCashTextObject*>(p)->InterfaceCashTextObject::~InterfaceCashTextObject(); break;
        case 1: static_cast<InterfaceNullCashTextObject*>(p)->InterfaceNullCashTextObject::~InterfaceNullCashTextObject(); break;
        case 2: static_cast<InterfacePercentTextObject*>(p)->InterfacePercentTextObject::~InterfacePercentTextObject(); break;
        case 3: static_cast<InterfaceVisitorNumTextObject*>(p)->InterfaceVisitorNumTextObject::~InterfaceVisitorNumTextObject(); break;
        default: static_cast<InterfaceVisitorTimeTextObject*>(p)->InterfaceVisitorTimeTextObject::~InterfaceVisitorTimeTextObject(); break;
    }
    if (static_cast<int16_t>(flag) > 0) Ev("OperatorDelete", {Lab(p)});
    return mem;
}

// The password text object's constructor: the log only.
void tpw_vi_password(uint64_t mem, uint32_t data) {
    int64_t out[12];
    tpw_vi_label(mem);
    tpw_vi_ctor(0, mem, data, out);
}

uint32_t tpw_vi_ctor_label(uint64_t mem) {
    tpw_vi_label(mem);
    return 0;
}

int32_t tpw_vi_show(uint64_t mem, int32_t initial, int32_t shown) {
    InterfaceDrawEngineHGradient* g = reinterpret_cast<InterfaceDrawEngineHGradient*>(static_cast<uintptr_t>(mem));
    gMesh = &g->mMeshSet;
    g->mShown = static_cast<int16_t>(initial);
    g->InterfaceDrawEngineHGradient::Show(static_cast<int16_t>(shown));
    return g->mShown;
}

void tpw_vi_menu(uint64_t mem, int64_t* out) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(mem));
    std::memset(p, 0xAA, sizeof(UIMenu));
    UIMenu* m = new (p) UIMenu();
    out[0] = m->mF00;
    out[1] = static_cast<uint32_t>(m->mF04);
    out[2] = m->mF08;
    out[3] = static_cast<uint32_t>(m->mShown);
    out[4] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(m->mWindow));
}

uint32_t tpw_vi_colour(int32_t state) { return UIMain_GetObjectStateTextColour(state); }

// which: 0 TextBox::ClientDraw, 1 Bar::DrawFunction, 2 DeInitStatusWindow, 3 DeInitWindow, 4 ApplyChanges, 5 DeInitViewWindow.
void tpw_vi_nop(int32_t which, uint64_t mem) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(mem));
    switch (which) {
        case 0: static_cast<InterfaceTextBox*>(p)->InterfaceTextBox::ClientDraw(); break;
        case 1: static_cast<InterfaceBar*>(p)->InterfaceBar::DrawFunction(); break;
        case 2: static_cast<BogStatusPanel*>(p)->DeInitStatusWindow(); break;
        case 3: static_cast<BogStatusPanel*>(p)->DeInitWindow(); break;
        case 4: static_cast<BogStatusPanel*>(p)->ApplyChanges(); break;
        default: static_cast<BogStatusPanel*>(p)->DeInitViewWindow(); break;
    }
}

}  // extern "C"
