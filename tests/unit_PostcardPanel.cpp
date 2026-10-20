// w102: recording boundaries and a six-window tree; the rectangle and render-area lifetimes are real.
#include <cstdlib>
#include <cstring>
#include <new>
#include "calllog.h"
#include "ui_Postcard.h"
#include "Postcard.h"
#include "ui_main.h"
#include "ui_controls.h"
#include "ui_ErrorDisplay.h"
#include "UIInterface.h"
#include "UI_pump.h"
#include "Game.h"
#include "FS_Online.h"
#include "Text.h"
#include "InterfaceFontWrapper.h"
#include "TbITable.h"
#include "tb_ddraw.h"
#include "tb_resource_texture.h"
#include "TbGraphicFile.h"
#include "fake_strings.h"
#include "fake_string_slots.h"
using namespace calllog;

namespace {
void* blocks[64];
int blockCount;
int allocCount;
int failAt;
uint32_t lastMacSize;
void* fixtureCard;
InterfaceWindow* windows[6];
uint32_t present;
int32_t surfaceWidth;
int32_t surfaceHeight;
uint8_t pixels[64];
uint8_t surface[64];
char16_t strings[4][16] = {u"title", u"address", u"message", u"subject"};
char16_t errorText[] = u"bad address";
int fontToken;
int cursorToken;
int tableToken;

// Avoid allocations in the logging fakes themselves.
void TextEvent(const char* name, std::initializer_list<int64_t> args, const char16_t* text) {
    char out[160];
    int n = 0;
    if (text == nullptr) std::strcpy(out, "(null)");
    else {
        while (n < 159 && text[n]) {
            out[n] = text[n] < 128 ? static_cast<char>(text[n]) : '?';
            ++n;
        }
        out[n] = 0;
    }
    ev(name, args, {out});
}
}

// Named allocation events use Mac object sizes while the allocated storage uses native sizes.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    uint32_t macSize = size == sizeof(Postcard) ? 0x70 : size == sizeof(CFSOnlineSystem) ? 0x404 :
                       size == sizeof(InterfaceCursorPosTextObjectSprite) ? 0x19c : 0xdc;
    lastMacSize = macSize;
    ++allocCount;
    if (allocCount == failAt) {
        ev("new", {macSize, 0});
        return nullptr;
    }
    void* p = std::malloc(size);
    std::memset(p, 0xa5, size);
    blocks[blockCount++] = p;
    ev("new", {macSize, newlab(p)});
    ++tpw_cl_heap[0];
    return p;
}

// Keep deleted labelled blocks until fixture teardown so stale pointers cannot acquire a new label.
bool KeepBlock(void* p, bool) {
    if (p == fixtureCard) return true;
    for (int i = 0; i < blockCount; ++i) if (blocks[i] == p) return true;
    return false;
}
#define TPW_RELEASE_OBSERVER KeepBlock
#include "calllog.cpp"

CGame gGame;
CGame::CGame() {}
CGame::~CGame() {}
CMapLinkBase::~CMapLinkBase() {}
CTextureSettings gei_TextureSettings;
CTextureSettings::CTextureSettings() {}
CDirectDraw* gei_DDraw = reinterpret_cast<CDirectDraw*>(0x20002000);
CFSOnlineSystem* gOnlineSystem;
InterfaceWindow* gUIRootWindow;
InterfaceWindow* gGameWindow;
InterfaceWindow* gOnlineWindow;
UIInterface gUIInterface;
int32_t gPostcardDelay;
int32_t gPostcardWanted;
CText gText;
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* f) : mFont(f) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRender::TbFontRender() : mMethod(nullptr) {}
TbFontRender::~TbFontRender() {}
TbTextFormatter::TbTextFormatter() {}
TbTextFormatter::~TbTextFormatter() {}

// Global game type guard is native; each fixture chooses its stored type before logging begins.
CGameType::CGameType() : mType(kGameTypeNormal) {}
void CGame::Pause(ePauseFlag f, bool sounds) { ev("Pause", {lab(this), f, sounds, mF3C}); }
void CGame::Unpause() { ev("Unpause", {lab(this), mF3C}); }
CFSOnlineSystem::CFSOnlineSystem() { ev("Online.Ctor", {lab(this)}); }
int32_t CFSOnlineSystem::CanWeSendEmail() {
    ev("CanEmail", {lab(this)});
    return ret("CanEmail");
}
void AddUIError(int32_t error, TbITable* table) { ev("Error", {error, lab(table)}); }
PostcardBitmap::~PostcardBitmap() {}
Postcard::Postcard() { ev("Postcard.Ctor", {lab(this)}); }
Postcard::~Postcard() { ev("Postcard.Dtor", {lab(this)}); }
int32_t Postcard::load_sources() {
    ev("Load", {lab(this)});
    return 1;
}
void Postcard::generate() { ev("Generate", {lab(this)}); }
void Postcard::generate_preview_into_buffer(uint8_t* out, int32_t w, int32_t h, int32_t f) {
    ev("Preview", {lab(this), lab(out), w, h, f});
    surface[0] = static_cast<uint8_t>(f);
}

// All 31 words of the aggregate return are recorded; the native pointer is represented by its fixture label.
DDSURFACEDESC2 CDirectDraw::LockBackbuffer() {
    ev("Lock", {lab(this)});
    DDSURFACEDESC2 d{};
    uint32_t* first = &d.dwSize;
    for (int i = 0; i < 9; ++i) first[i] = 0x12340000 + i;
    d.lpSurface = pixels;
    uint32_t* rest = &d.ddckCKDestOverlay.dwColorSpaceLowValue;
    for (int i = 0; i < 21; ++i) rest[i] = 0x1234000a + i;
    return d;
}
int32_t Postcard::grab(_DDSURFACEDESC2& arg) {
    auto& d = reinterpret_cast<DDSURFACEDESC2&>(arg);
    const uint32_t* first = &d.dwSize;
    ev("Grab", {lab(this)});
    for (int i = 0; i < 9; ++i) ev("Desc", {i, first[i]});
    ev("Desc", {9, lab(d.lpSurface)});
    const uint32_t* rest = &d.ddckCKDestOverlay.dwColorSpaceLowValue;
    for (int i = 0; i < 21; ++i) ev("Desc", {i + 10, rest[i]});
    return 1;
}
int32_t CDirectDraw::UnlockBackbuffer() {
    ev("Unlock", {lab(this)});
    return 1;
}
uint32_t NS_MacDoze::GetTempPath(uint32_t n, char* out) {
    ev("Temp", {n, lab(out)});
    std::strcpy(out, "temp:postcard");
    return 13;
}
void UIMain_UISound(int32_t id) { ev("Sound", {id}); }
bool UIMain_CheckPanel() {
    ev("CheckPanel");
    return true;
}
void UIMain_SetPanel(InterfaceWindow* w) { ev("SetPanel", {lab(w)}); }
void UIMain_SetTitleText(InterfaceWindow* w, int32_t id, char16_t* s) { TextEvent("Title", {lab(w), id}, s); }
void UIMain_AdvisorMessage(AdvisorMessage id) { ev("Advisor", {id}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage id) { ev("CancelAdvisor", {id}); }
void UIMain_GetFullyOutOfFirstPerson() { ev("OutOfFirstPerson"); }
int32_t UIMain_DefaultPanelMsg(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) {
    ev("DefaultPanel", {lab(w), m, a, b});
    return ret("DefaultPanel");
}
void UIMain_DisableAllKeyTables() { ev("DisableKeys"); }
void UIMain_EnableAllKeyTables() { ev("EnableKeys"); }
void UIMain_DecideKbCapture() { ev("DecideKb"); }
void UI_HideCursor() { ev("HideCursor"); }
void UI_ShowCursor() { ev("ShowCursor"); }
void BTN_HighLight_Kill() { ev("KillHighlight"); }
void ADV_Kill(int32_t id) { ev("KillAdvisor", {id}); }
void ChatPrePostCard() { ev("ChatPre"); }
void ChatGoFullScreen() { ev("ChatFull"); }
void ChatReinstateAfterPostcard() { ev("ChatReinstate"); }
int32_t GameWindowToggle_Hidden() {
    ev("Hidden");
    return ret("Hidden");
}
void ToggleGameWindow() { ev("Toggle"); }
int32_t UI_SendMessage(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) {
    ev("Send", {lab(w), m, a, b});
    return ret("Send");
}
void CreateAddressBook() { ev("AddressBook"); }
int32_t AddressesValid() {
    ev("Valid");
    return ret("Valid");
}
void SendPostcard() { ev("SendPostcard"); }
void UI_DrawPostcardText(char16_t* text, void* p, int32_t w, int32_t h,
                         SamsUtilities::URectangle::LongRect rect, uint32_t colour, int32_t mode) {
    TextEvent("DrawText", {lab(text), lab(p), w, h, rect.left, rect.top, rect.right, rect.bottom, colour, mode}, text);
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("String", {id});
    return id == 0xd6 ? errorText : strings[id - 0xd0];
}
InterfaceFont* GetFontTableEntry(uint32_t id) {
    ev("FontEntry", {id});
    return reinterpret_cast<InterfaceFont*>(&fontToken);
}
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDraw", {}, {name});
    return reinterpret_cast<InterfaceDrawObject*>(&cursorToken);
}
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("Format", {lab(this), lab(out), size, id});
    const char16_t* text = id == 0xd9 ? u"default message" : u"default address";
    int n = 0;
    while (text[n] && n + 1 < static_cast<int>(size)) {
        out[n] = text[n];
        ++n;
    }
    out[n] = 0;
    return n;
}
InterfaceWindow* UI_DialogBox(dlgboxparams& p, dlgbutton b1, dlgcallback c1, dlgbutton b2,
                              dlgcallback c2, dlgbutton b3, dlgcallback c3, bool flag) {
    TextEvent("Dialog", {p.mRed, p.mGreen, p.mBlue, p.mValue8, b1, lab(reinterpret_cast<void*>(c1)), b2,
                         lab(reinterpret_cast<void*>(c2)), b3, lab(reinterpret_cast<void*>(c3)), flag}, p.mText);
    return nullptr;
}

// Window boundaries model the already translated field updates and record every passed argument.
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
InterfaceWindow::~InterfaceWindow() {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }
InterfaceEditBox::InterfaceEditBox(InterfaceWindow* p, int32_t a, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b)
    : InterfaceWindow(p, a, id, l, t, r, b) {}
InterfaceEditBox::~InterfaceEditBox() {}
void InterfaceEditBox::DrawFunction() {}
void InterfaceEditBox::CalcClientArea() {}
int32_t InterfaceEditBox::WhatAmI() { return 5; }
void InterfaceEditBox::ClientDraw() {}
int32_t InterfaceEditBox::SetText(char16_t* text) {
    TextEvent("SetText", {lab(this)}, text);
    if (mBuffer != nullptr) {
        int n = 0;
        while (text && text[n] && n < mMaxLength) {
            mBuffer[n] = text[n];
            ++n;
        }
        mBuffer[n] = 0;
    }
    return 1;
}
int32_t InterfaceEditBox::SetStaticTextArea(char16_t* text, int32_t n) {
    ev("TextArea", {lab(this), lab(text), n, text[0], text[n]});
    mBuffer = text;
    mMaxLength = n;
    return 1;
}
void InterfaceEditBox::SetCursorDrawObject(InterfaceDrawObject* draw) {
    ev("CursorDraw", {lab(this), lab(draw)});
    mCursorDrawObject = draw;
}
InterfaceWindow* InterfaceWindow::Build(int16_t* script, InterfaceWindowCallback cb) {
    ev("Build", {lab(this), lab(script), cb == PostcardPanelMsg ? 1 : -1});
    return ret("Build") ? windows[1] : nullptr;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    for (auto* child = mChild; child; child = child->mNext) if (child->mId == id) return child;
    return nullptr;
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback cb) {
    ev("Callback", {lab(this), cb == PostcardPreviewMsg ? 2 : -1});
    mCallback = cb;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* draw) {
    ev("ClientDraw", {lab(this), lab(draw)});
    mClientDrawObject = draw;
}
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("Font", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontColour(int32_t r, int32_t g, int32_t b, int32_t a) { ev("Colour", {lab(this), r, g, b, a}); }
void InterfaceWindow::SetFontWrap(int32_t a, int32_t b) { ev("Wrap", {lab(this), a, b}); }
void InterfaceWindow::SetFontJust(int32_t a, int32_t b) { ev("Just", {lab(this), a, b}); }
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    mAttributes = (mAttributes & ~1u) | (on != 0);
    return 1;
}
int32_t InterfaceWindow::CaptureDevice() {
    ev("Capture", {lab(this)});
    return 1;
}
int32_t InterfaceWindow::UnCaptureDevice() {
    ev("UnCapture", {lab(this)});
    return 1;
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t m, int32_t a, int32_t b) {
    ev("Default", {lab(this), m, a, b});
    return ret("Default");
}
uint8_t* InterfaceWindow::GetClientObjectSurface(int32_t* w, int32_t* h) {
    ev("Surface", {lab(this), lab(w), lab(h)});
    *w = surfaceWidth;
    *h = surfaceHeight;
    return ret("Surface") ? surface : nullptr;
}
void InterfaceWindow::FinishWithClientObjectSurface() { ev("Finish", {lab(this)}); }

// Sprite constructor boundaries are replaced whole, as on the PowerPC side.
InterfaceDrawObject::InterfaceDrawObject(void*) {}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }
SSpriteGameData::SSpriteGameData() {}
SSpriteEngineData::SSpriteEngineData() {}
SSpriteEngineData::~SSpriteEngineData() {}
CSpriteList::CSpriteList() {}
CSpriteList::~CSpriteList() {}
int32_t CSpriteList::Prepare() { return 0; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }
InterfaceDrawObjectSprite::InterfaceDrawObjectSprite(void* p) : InterfaceDrawObject(p) {
    if (on() && lastMacSize == 0xdc) ev("Sprite.Ctor", {lab(this), lab(p)});
}
InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite() {}
void InterfaceDrawObjectSprite::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceDrawObjectSprite::Show(int16_t) {}
void InterfaceDrawObjectSprite::Refresh() {}
int32_t InterfaceDrawObjectSprite::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
void InterfaceDrawObjectSprite::UpdateTextures(char16_t*) {}
void InterfaceDrawObjectSprite::CreateTextures(char16_t*) {}
InterfaceTextObjectSprite::InterfaceTextObjectSprite(void* p) : InterfaceDrawObjectSprite(p) {}
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}
void InterfaceTextObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceTextObjectSprite::UpdateTextures(char16_t*) {}
InterfaceCursorPosTextObjectSprite::InterfaceCursorPosTextObjectSprite(void* p) : InterfaceTextObjectSprite(p) {
    ev("Cursor.Ctor", {lab(this), lab(p)});
}
InterfaceCursorPosTextObjectSprite::~InterfaceCursorPosTextObjectSprite() {}
intptr_t InterfaceCursorPosTextObjectSprite::Query(uint32_t, intptr_t) { return 0; }

namespace {
alignas(InterfaceWindow) unsigned char windowStore[3][sizeof(InterfaceWindow)];
alignas(InterfaceEditBox) unsigned char editStore[3][sizeof(InterfaceEditBox)];
alignas(Postcard) unsigned char postcardStore[sizeof(Postcard)];
alignas(CFSOnlineSystem) unsigned char onlineStore[sizeof(CFSOnlineSystem)];
}

extern "C" {
void tpw_card_reset(const int32_t* a) {
    tpw_cl_on = 0;
    for (int i = 0; i < blockCount; ++i) std::free(blocks[i]);
    blockCount = 0;
    allocCount = 0;
    failAt = a[21];
    for (int i = 0; i < 3; ++i) {
        std::memset(windowStore[i], 0, sizeof(windowStore[i]));
        windows[i] = new (windowStore[i]) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0);
        std::memset(editStore[i], 0, sizeof(editStore[i]));
        windows[i + 3] = new (editStore[i]) InterfaceEditBox(nullptr, 0, 0, 0, 0, 0, 0);
    }
    present = a[4];
    windows[1]->mChild = nullptr;
    InterfaceWindow* last = nullptr;
    for (int i = 2; i < 6; ++i) {
        windows[i]->mId = i == 2 ? 0x5d9f : 0x5da0 + i;
        windows[i]->mNext = nullptr;
        if (present & (1 << (i - 2))) {
            if (last) last->mNext = windows[i];
            else windows[1]->mChild = windows[i];
            last = windows[i];
        }
    }
    TheGameType().mType = static_cast<GameType>(a[0]);
    gOnlineSystem = a[3] ? reinterpret_cast<CFSOnlineSystem*>(onlineStore) : nullptr;
    gPostcard = a[2] ? new (postcardStore) Postcard : nullptr;
    fixtureCard = postcardStore;
    if (gPostcard) {
        gPostcard->mCard.mWidth = a[18];
        gPostcard->mCard.mHeight = a[19];
        gPostcard->mCard.mPixels = a[20] ? pixels : nullptr;
    }
    gPostcardPanel = a[1] ? windows[1] : nullptr;
    gUIRootWindow = windows[0];
    gGameWindow = windows[0];
    gOnlineWindow = windows[0];
    gGame.mF3C = a[17];
    gei_TextureSettings.mFormat = a[5];
    surfaceWidth = a[6];
    surfaceHeight = a[7];
    gPreviewDragging = a[8];
    gPreviewLastX = a[9];
    gPreviewLastY = a[10];
    gPreviewOffsetX = a[11];
    gPreviewOffsetY = a[12];
    gMessageEdited = a[13];
    gAddressEdited = a[14];
    gTempPathFetched = a[15];
    gPostcardSent = a[16];
    gPostcardWanted = a[22];
    gPostcardDelay = a[23];
    std::memset(gPostcardMessage, 0x41, sizeof(gPostcardMessage));
    std::memset(gPostcardAddress, 0x42, sizeof(gPostcardAddress));
    std::memset(gPostcardText5da5, 0x43, sizeof(gPostcardText5da5));
    gPostcardMessage[3] = 0;
    gPostcardAddress[4] = 0;
    gPostcardText5da5[5] = 0;
    std::memset(gTempPath, 0x44, sizeof(gTempPath));
    gTempPath[7] = 0;
    std::memset(surface, 0x77, sizeof(surface));
    gText.mResourcePointers[6] = &tableToken;
    for (int i = 0; i < 6; ++i) tpw_cl_label(10 + i, windows[i]);
    tpw_cl_label(20, postcardStore);
    tpw_cl_label(21, onlineStore);
    tpw_cl_label(22, &gGame);
    tpw_cl_label(23, gei_DDraw);
    tpw_cl_label(24, &fontToken);
    tpw_cl_label(25, &cursorToken);
    tpw_cl_label(26, &tableToken);
    tpw_cl_label(30, gPostcardMessage);
    tpw_cl_label(31, gPostcardAddress);
    tpw_cl_label(32, gPostcardText5da5);
    tpw_cl_label(33, gTempPath);
    tpw_cl_label(34, surface);
    tpw_cl_label(35, pixels);
    tpw_cl_label(36, gPostcardPanelDescription);
    std::memset(tpw_cl_heap, 0, sizeof(tpw_cl_heap));
    tpw_cl_on = 1;
}
int32_t tpw_card_op(int32_t op, int32_t message, int32_t a, int32_t b) {
    switch (op) {
    case 0:
        UI_DoPostcard();
        break;
    case 1:
        UI_ActuallyDoPostcard();
        break;
    case 2:
        CreatePostcardPanel();
        break;
    case 3: return PostcardPanelMsg(windows[1], message, a, b);
    case 4:
        RemovePostcardPanel();
        break;
    case 5:
        RefreshPreviewWindow(windows[2]);
        break;
    case 6: return PostcardPreviewMsg(windows[2], message, a, b);
    case 7: return TakePostCard();
    case 8:
        gUIInterface.DoPostcard();
        break;
    }
    return 0;
}
int32_t tpw_card_dump(int64_t* out) {
    int n = 0;
    out[n++] = lab(gPostcard);
    out[n++] = lab(gPostcardPanel);
    out[n++] = lab(gOnlineSystem);
    out[n++] = gPostcardSent;
    out[n++] = gPreviewDragging;
    out[n++] = gPreviewLastX;
    out[n++] = gPreviewLastY;
    out[n++] = gPreviewOffsetX;
    out[n++] = gPreviewOffsetY;
    out[n++] = gMessageEdited;
    out[n++] = gAddressEdited;
    out[n++] = gTempPathFetched;
    out[n++] = gGame.mF3C;
    out[n++] = gPostcardWanted;
    out[n++] = gPostcardDelay;
    for (int i = 0; i < 6; ++i) {
        out[n++] = windows[i]->mAttributes;
        out[n++] = lab(windows[i]->mClientDrawObject);
        out[n++] = windows[i]->mCallback == PostcardPreviewMsg ? 2 : 0;
        if (i >= 3) {
            auto* box = static_cast<InterfaceEditBox*>(windows[i]);
            out[n++] = lab(box->mBuffer);
            out[n++] = box->mMaxLength;
            out[n++] = lab(box->mCursorDrawObject);
        }
    }
    for (char16_t c : gPostcardMessage) out[n++] = c;
    for (char16_t c : gPostcardAddress) out[n++] = c;
    for (char16_t c : gPostcardText5da5) out[n++] = c;
    for (unsigned char c : gTempPath) out[n++] = c;
    for (unsigned char c : surface) out[n++] = c;
    for (int v : tpw_cl_heap) out[n++] = v;
    return n;
}
}
