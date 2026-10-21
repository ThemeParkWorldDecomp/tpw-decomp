// Fakes and test entry points for unit w40, the lobby's remaining leaves (libtpw_unit_LobbyLeaves.dylib, driven by test_LobbyLeaves.py for
// the main app and test_LobbyLeavesLtms.py for the ltms library; see calllog.h):
//   src/game/ui/ui_main.cpp     GetFontTableEntry 0x10180270, UI_CreateVersionNumber 0x101874d0
//   src/game/ui/ui_utils.cpp     UI_MeasureText 0x1018b8e4
//   src/game/online/lobby.cpp     LB_Lobby_MailIconMessagesFn 0x100a59ec
//   src/game/online/FS_Online.cpp CGameType::CGameType 0x1016f5d8, CGameType::SetGameType 0x1016f6a0
//   src/ltms/StaticUnicodeBuffer.cpp (ltms) AppendMB x2, AppendUnicode x2, Close, the destructor, with src/ltms/StaticUnicodeBuffer.cpp, src/ltms/TbFontRenderParams.cpp, src/ltms/TbIResourceIndex.cpp for the converter
// Every callee outside the unit is a fake that records the same event as the PowerPC side's stub (names, labels, values, texts). The labels
// the main app's cases use: 1 the render parameters inside gText, 3 the multibyte buffer UIMain_ConvertString answers, 5 gText, 10 + n the
// font table entry n (0..51), 62 + n the font object n (0..51), 30 the root window, 31 the lobby's window, 32 the window CreateObject
// makes, 33 the lobby object, 40 the online system a case starts with, 41 the mail icon window. The ltms cases label the converter 4.
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <new>

#include "CStaticBuffer.h"
#include "Debug.h"
#include "FS_Online.h"
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "InterfaceWindow.h"
#include "TPMain.h"
#include "TbFontRenderParams.h"
#include "TbGraphicFile.h"
#include "TbResource.h"
#include "TbTextFormatter.h"
#include "TbTextResourceManager.h"
#include "Text.h"
#include "UI_Base.h"
#include "calllog.h"
#include "lobby.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

using calllog::ev;
using calllog::lab;
using calllog::newlab;
using calllog::ret;

namespace {

// A UTF-16 text as the log keeps it (ASCII kept, anything else '?', at most 159 characters), in a buffer of the caller's: the heap traffic
// the test compares must not include the fakes' own strings.
void Narrow(const char16_t* text, char (&out)[160]) {
    if (text == nullptr) {
        std::strcpy(out, "(null)");
        return;
    }
    uint32_t n = 0;
    for (; text[n] != 0 && n < 159; ++n) out[n] = text[n] < 0x80 ? static_cast<char>(text[n]) : '?';
    out[n] = '\0';
}

// The same for multibyte text.
void NarrowBytes(const unsigned char* text, char (&out)[160]) {
    if (text == nullptr) {
        std::strcpy(out, "(null)");
        return;
    }
    uint32_t n = 0;
    for (; text[n] != 0 && n < 159; ++n) out[n] = text[n] < 0x80 ? static_cast<char>(text[n]) : '?';
    out[n] = '\0';
}

// What the PowerPC side's fonts are: arbitrary addresses, never dereferenced, labelled 62 + number.
uintptr_t FontToken(int index) { return 0x20000000u + 0x40u * static_cast<uintptr_t>(index); }

}  // namespace

// ---- the game text object (the font table), with the constructors of its members faked as elsewhere
InterfaceFont::InterfaceFont() {}   // the fake leaves the object as it finds it
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::TbFontRenderParams() {}
TbFontRenderParams::~TbFontRenderParams() {}
// The member TbFontRender of the formatter (src/ltms/TbFontRender.cpp is not part of this library).
TbFontRender::TbFontRender() : mMethod(nullptr) {}
TbFontRender::~TbFontRender() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
CText gText;

// ---- CText::IsHebrew and the font render parameters
bool CText::IsHebrew() const {
    ev("IsHebrew", {lab(this)});
    return ret("IsHebrew") != 0;
}
TbError TbFontRenderParams::SetFont(TbIFont* font) {
    ev("SetFont", {lab(this), lab(font)});
    return TbError(0);
}

// ---- the text formatter. The one inside an InterfaceTextObjectSprite is constructed silently (the original's constructor of that class
// is one fake; see InterfaceTextObjectSprite below).
namespace {
bool gSilentFormatter = false;
}
TbTextFormatter::TbTextFormatter() {
    if (!gSilentFormatter) ev("TbTextFormatter::TbTextFormatter", {newlab(this)});
}
TbTextFormatter::~TbTextFormatter() { ev("TbTextFormatter::~TbTextFormatter", {lab(this)}); }
TbError TbTextFormatter::SetHorizontalJustification(eJustification justification) {
    ev("SetHorizontalJustification", {lab(this), justification});
    return TbError(0);
}
TbError TbTextFormatter::SetVerticalJustification(eJustification justification) {
    ev("SetVerticalJustification", {lab(this), justification});
    return TbError(0);
}
// Measure writes the extent it is given from the case's queue "Measure" (four values).
TbError TbTextFormatter::Measure(const TbRect& rect, const unsigned char* text, TbFontRenderParams& params, TbRect* extent) {
    char shown[160];
    NarrowBytes(text, shown);
    ev("Measure", {lab(this), lab(&rect), rect.left, rect.top, rect.right, rect.bottom, lab(&params), lab(text), lab(extent)}, {shown});
    if (extent != nullptr) {
        extent->left = static_cast<int32_t>(ret("Measure"));
        extent->top = static_cast<int32_t>(ret("Measure"));
        extent->right = static_cast<int32_t>(ret("Measure"));
        extent->bottom = static_cast<int32_t>(ret("Measure"));
    }
    return TbError(0);
}

// ---- UIMain_ConvertString: the low byte of each character, in the multibyte buffer (label 3)
namespace {
uint8_t gMultibyte[0x4000];
}
uint8_t* UIMain_ConvertString(const char16_t* text) {
    char shown[160];
    Narrow(text, shown);
    ev("UIMain_ConvertString", {}, {shown});
    uint32_t n = 0;
    for (; text != nullptr && text[n] != 0 && n < sizeof gMultibyte - 1; ++n) gMultibyte[n] = static_cast<uint8_t>(text[n]);
    gMultibyte[n] = 0;
    return gMultibyte;
}

// ---- the interface's windows (raw storage, never constructed: the fakes only record), the lobby object and the globals
namespace {
alignas(64) unsigned char gWindowStore[4][2048];    // 0 the root window, 1 the lobby's, 2 the one CreateObject makes, 3 the mail icon's
alignas(64) unsigned char gLobbyStore[sizeof(CLBLobby)];
alignas(64) unsigned char gOnlineStore[sizeof(CFSOnlineSystem)];
static_assert(sizeof(InterfaceWindow) <= sizeof gWindowStore[0], "the window store is too small");
InterfaceWindow* Window(int index) { return reinterpret_cast<InterfaceWindow*>(gWindowStore[index]); }
}  // namespace
InterfaceWindow* gUIRootWindow;
CLBLobby* gLobby;
CFSOnlineSystem* gOnlineSystem;
uint32_t gGameOptions;

InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return ret("CreateObject") != 0 ? Window(2) : nullptr;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("Window.SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) {
    char shown[160];
    Narrow(text, shown);
    ev("SetClientDrawText", {lab(this)}, {shown});
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

// ---- the draw objects. The version label's text object is made by `new`; the original's constructor of it is one fake, so the formatter
// the C++ object holds (and the base classes) record nothing here.
InterfaceDrawObject::InterfaceDrawObject(void*)
    : mData(nullptr), mShown(0), mFrameBase(0), mFrame(0), mFrameCount(0), mInLibrary(0), mResizable(0), mF28(0), mF2A(0), mF2C(0),
      mF2E(0), mF30(0), mF32(0), mDepth(0) {}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }
InterfaceDrawObjectSprite::InterfaceDrawObjectSprite(void* data) : InterfaceDrawObject((gSilentFormatter = true, data)) {}
InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite() {}
void InterfaceDrawObjectSprite::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceDrawObjectSprite::Show(int16_t) {}
void InterfaceDrawObjectSprite::Refresh() {}
int32_t InterfaceDrawObjectSprite::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
void InterfaceDrawObjectSprite::UpdateTextures(char16_t*) {}
void InterfaceDrawObjectSprite::CreateTextures(char16_t*) {}
// The members those classes own by value (the rectangles are src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp's; the engine's sprite surface and list): never used here, but their constructors
// and destructors must exist to link.
SSpriteGameData::SSpriteGameData() {}
SSpriteEngineData::SSpriteEngineData() {}
SSpriteEngineData::~SSpriteEngineData() {}
CMapLinkBase::~CMapLinkBase() {}
CSpriteList::CSpriteList() {}
CSpriteList::~CSpriteList() {}
int32_t CSpriteList::Prepare() { return 1; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }
// The silence starts with the sprite base (above) and ends in the text object's own body: its member formatter is built in between.
InterfaceTextObjectSprite::InterfaceTextObjectSprite(void* data) : InterfaceDrawObjectSprite(data) {
    gSilentFormatter = false;
    ev("InterfaceTextObjectSprite", {newlab(this), lab(data)});
}
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}
void InterfaceTextObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceTextObjectSprite::UpdateTextures(char16_t*) {}

// ---- the online system, the outbox panel, the interface's strings and the dialog box
CFSOnlineSystem::CFSOnlineSystem() { ev("CFSOnlineSystem", {newlab(this)}); }
int32_t CFSOnlineSystem::AreWeLoggedIn() {
    ev("AreWeLoggedIn", {lab(this)});
    return static_cast<int32_t>(ret("AreWeLoggedIn"));
}
int32_t CreateOnlineOutboxPanel() {
    ev("CreateOnlineOutboxPanel");
    return 0;
}
namespace {
char16_t gStringText[260];   // UIMain_GetString's answer, set by the case
bool gStringPresent;
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return gStringPresent ? gStringText : nullptr;
}
InterfaceWindow* UI_DialogBox(dlgboxparams& params, dlgbutton button1, dlgcallback callback1, dlgbutton button2, dlgcallback callback2,
                  dlgbutton button3, dlgcallback callback3, bool flag) {
    char shown[160];
    Narrow(params.mText, shown);
    ev("UI_DialogBox", {params.mRed, params.mGreen, params.mBlue, params.mValue8, params.mValueC, button1, button2, button3,
                        callback1 != nullptr, callback2 != nullptr, callback3 != nullptr, flag ? 1 : 0}, {shown});
    return nullptr;
}

// ---- the game type's assertion
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// ---- Sam's Utilities' strings and version (platform code): the version's text is the case's
namespace {
char gVersionText[400];
}
SamsUtilities::UString::UString() {
    mLength = 0;
    mText = nullptr;
    if (calllog::on()) ev("UString", {newlab(this)});
}
SamsUtilities::UString::~UString() { ev("~UString", {lab(this)}); }
SamsUtilities::UString& SamsUtilities::UString::operator=(const UString& other) {
    ev("UString=", {lab(this), lab(&other)});
    mLength = other.mLength;
    mText = other.mText;
    return *this;
}
SamsUtilities::UResourceId::UResourceId(const UString&, int32_t id) : mType(), mId(id) {}
const SamsUtilities::UResourceId SamsUtilities::UVersion::kDefaultVersionResource(SamsUtilities::UString(), 1);
SamsUtilities::UVersion::UVersion(uint16_t major, uint8_t minor, uint8_t fix, uint16_t build)
    : mMajor(major), mMinor(minor), mFix(fix), mStage(0x80), mBuild(build), mRegion(0), mText(), mComment() {
    ev("UVersion", {newlab(this), major, minor, fix, build});
}
void SamsUtilities::UVersion::LoadFromResource(const UResourceId& resource, uint8_t create) {
    ev("LoadFromResource", {lab(this), &resource == &kDefaultVersionResource ? 1 : 0, create});
    mText.mLength = static_cast<uint32_t>(std::strlen(gVersionText));
    mText.mText = gVersionText;
}

// ---- swprintf: only a plain %s (a narrow string) is formatted, as the original's format needs; anything else is copied as it stands. The
// log gets the format and the whole formatted text; the buffer gets it cut to the room like the C library's (size - 1 characters and a
// terminator, -1 when it did not fit).
int32_t swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    char16_t text[512];
    uint32_t n = 0;
    for (const char16_t* p = format; *p != 0 && n < 500; ++p) {
        if (p[0] == u'%' && p[1] == u's') {
            const char* narrow = va_arg(arguments, const char*);
            ++p;
            for (; narrow != nullptr && *narrow != 0 && n < 500; ++narrow) text[n++] = static_cast<unsigned char>(*narrow);
        } else {
            text[n++] = *p;
        }
    }
    text[n] = 0;
    va_end(arguments);
    char shownFormat[160];
    char shownText[160];
    Narrow(format, shownFormat);
    Narrow(text, shownText);
    ev("swprintf", {newlab(buffer), size}, {shownFormat, shownText});
    if (size == 0) return -1;
    const uint32_t written = n < size - 1 ? n : size - 1;
    for (uint32_t i = 0; i < written; ++i) buffer[i] = text[i];
    buffer[written] = 0;
    return n < size ? static_cast<int32_t>(n) : -1;
}

// ---- TbResource's virtual members: the string buffer's converter is a TbIMBToUnicode (src/ltms/StaticUnicodeBuffer.cpp, src/ltms/TbFontRenderParams.cpp, src/ltms/TbIResourceIndex.cpp), and releasing it is
// what the tests watch
int32_t TbResource::Release() {
    ev("Release", {lab(this)});
    return 0;
}
void TbResource::AddRef() {}
int32_t TbResource::ManagerRelease() { return 0; }
void TbResource::ManagerAddRef() {}

namespace {
// The array the string buffer under test writes into: eight guard cells either side of up to 40 cells, all 0x9999 at the start.
constexpr int kGuard = 8;
constexpr int kMaxCells = 40;
char16_t gCells[kGuard + kMaxCells + kGuard];
// The heap block the buffer under test is constructed in, made before the test counts the heap traffic.
void* gBufferBlock;
}  // namespace

extern "C" {

// ---- GetFontTableEntry and UI_MeasureText: the game text object's resolution group and its 52 font slots (0: no font object, otherwise
// the font number 1..52, its object labelled 62 + number - 1) with the fixed labels listed at the top.
void tpw_ll_text_world(int32_t group, const int32_t* fonts) {
    gText.mResolutionGroup = static_cast<uint32_t>(group);
    std::memset(gMultibyte, 0, sizeof gMultibyte);
    for (int i = 0; i < 52; ++i) {
        gText.mFonts[i].mFont = fonts[i] == 0 ? nullptr : reinterpret_cast<TbIFont*>(FontToken(fonts[i] - 1));
        if (fonts[i] != 0) tpw_cl_label(62 + fonts[i] - 1, gText.mFonts[i].mFont);
        tpw_cl_label(10 + i, &gText.mFonts[i]);
    }
    tpw_cl_label(1, &gText.mRenderParams);
    tpw_cl_label(3, gMultibyte);
    tpw_cl_label(5, &gText);
}

// GetFontTableEntry(index): the number of the font table entry it answers, or -1 for NULL.
int64_t tpw_ll_font_entry(uint32_t index) {
    InterfaceFont* entry = GetFontTableEntry(index);
    if (entry == nullptr) return -1;
    return static_cast<int64_t>(static_cast<InterfaceFontWrapper*>(entry) - gText.mFonts);
}

// UI_MeasureText. text may be NULL, extent may be NULL (else four words).
void tpw_ll_measure(const char16_t* text, const int32_t* rect, int32_t font, int32_t horizontal, int32_t vertical, int32_t wrap,
                    int32_t* extent) {
    const TbRect box = {rect[0], rect[1], rect[2], rect[3]};
    UI_MeasureText(text, box, static_cast<int8_t>(font), static_cast<int8_t>(horizontal), static_cast<int8_t>(vertical), wrap,
                   reinterpret_cast<TbRect*>(extent));
}

// The multibyte buffer's first `n` bytes.
void tpw_ll_mb(uint8_t* out, int32_t n) { std::memcpy(out, gMultibyte, static_cast<size_t>(n)); }

// ---- UI_CreateVersionNumber: the root window, the lobby with its window, the screen's size and the version resource's text.
void tpw_ll_version_world(int32_t screenWidth, int32_t screenHeight, const char* versionText) {
    gUIRootWindow = Window(0);
    gLobby = reinterpret_cast<CLBLobby*>(gLobbyStore);
    gLobby->mWindow = Window(1);
    gUIRealScreenWidth = static_cast<int16_t>(screenWidth);
    gUIRealScreenHeight = static_cast<int16_t>(screenHeight);
    std::strncpy(gVersionText, versionText, sizeof gVersionText - 1);
    gVersionText[sizeof gVersionText - 1] = '\0';
    tpw_cl_label(30, Window(0));
    tpw_cl_label(31, Window(1));
    tpw_cl_label(32, Window(2));
    tpw_cl_label(33, gLobbyStore);
}
void tpw_ll_version(int32_t show) { UI_CreateVersionNumber(show != 0); }

// ---- LB_Lobby_MailIconMessagesFn: the online system (label 40) there already or not, the interface string 0x1d9 there or not, and the
// icon's window (label 41). Answers the handler's result and the label of the online system afterwards.
void tpw_ll_mail_world(int32_t onlineExists, int32_t hasString, const char16_t* text) {
    gOnlineSystem = onlineExists != 0 ? reinterpret_cast<CFSOnlineSystem*>(gOnlineStore) : nullptr;
    if (onlineExists != 0) tpw_cl_label(40, gOnlineStore);
    tpw_cl_label(41, Window(3));
    gStringPresent = hasString != 0;
    uint32_t n = 0;
    for (; text[n] != 0 && n < 259; ++n) gStringText[n] = text[n];
    gStringText[n] = 0;
}
int32_t tpw_ll_mail(int32_t message, int32_t a, int32_t b, int32_t* onlineLabel) {
    const int32_t answer = LB_Lobby_MailIconMessagesFn(Window(3), message, a, b);
    *onlineLabel = lab(gOnlineSystem);
    return answer;
}

// ---- CGameType: op 0 constructs one over a block of 0xa5 with the option word `options`, op 1 calls SetGameType(type) on one whose type
// is 0x12345678. Answers the type and the option word afterwards.
void tpw_ll_gametype(int32_t op, int32_t type, uint32_t options, int32_t* typeOut, uint32_t* optionsOut) {
    gGameOptions = options;
    alignas(CGameType) unsigned char store[sizeof(CGameType)];
    std::memset(store, 0xa5, sizeof store);
    CGameType* object = reinterpret_cast<CGameType*>(store);
    if (op == 0) {
        new (object) CGameType();
    } else {
        object->mType = static_cast<GameType>(0x12345678);
        object->SetGameType(static_cast<GameType>(type));
    }
    *typeOut = static_cast<int32_t>(object->mType);
    *optionsOut = gGameOptions;
}

// ---- CStaticUnicodeBuffer. Makes the heap block the buffer lives in (call before the heap counters are cleared).
void tpw_ll_buf_prepare() {
    if (gBufferBlock == nullptr) gBufferBlock = ::operator new(sizeof(CStaticUnicodeBuffer));
}

// Runs a script on a buffer over `cells` cells (at most 40) of the guarded array: the cursor starts `cursorStart` cells in (negative:
// NULL), the overflow flag as given, and the converter (label 4, first lead byte, `count` entries of `table`) is attached or not. An
// operation is five words: the code (0 AppendMB(bytes), 1 AppendMB(halfword), 2 AppendUnicode(text, count), 3 AppendUnicode(character), 4
// Close, 5 the destructor, then the object is gone if the flag is positive as a halfword; 6, 7 and 8 set the cursor (a cell number or
// 0xffffffff), the overflow flag and whether a converter is held, directly), then its arguments (bytes: pool offset and
// size; text: pool offset and count; the others one word). After each operation `out` gets four state words (the cursor in cells from
// the start or 0xffffffff, the end, whether a converter is held, the overflow flag) and then every cell of the array including the
// guards. The state words of the destructor's turn are zero (the object's own words are dead after it).
void tpw_ll_buf_run(int32_t cells, int32_t cursorStart, int32_t overflow, int32_t hasConverter, int32_t firstLead, int32_t count,
                    const uint16_t* table, const uint32_t* ops, int32_t nops, const uint8_t* bytePool, const uint16_t* widePool,
                    uint32_t* out) {
    for (int i = 0; i < kGuard + cells + kGuard; ++i) gCells[i] = 0x9999;
    char16_t* base = gCells + kGuard;
    TbIMBToUnicode converter(static_cast<uint8_t>(firstLead), static_cast<uint16_t>(count), const_cast<uint16_t*>(table));
    tpw_cl_label(4, &converter);
    if (gBufferBlock == nullptr) gBufferBlock = ::operator new(sizeof(CStaticUnicodeBuffer));
    CStaticUnicodeBuffer* buffer = new (gBufferBlock) CStaticUnicodeBuffer;
    buffer->mCursor = cursorStart < 0 ? nullptr : base + cursorStart;
    buffer->mEnd = base + cells;
    buffer->mConverter = hasConverter != 0 ? &converter : nullptr;
    buffer->mOverflow = overflow != 0;
    for (int32_t i = 0; i < nops; ++i) {
        const uint32_t* op = ops + 5 * i;
        bool gone = false;
        switch (op[0]) {
        case 0:
            buffer->AppendMB(bytePool + op[1], op[2]);
            break;
        case 1:
            buffer->AppendMB(static_cast<uint16_t>(op[1]));
            break;
        case 2:
            buffer->AppendUnicode(reinterpret_cast<const char16_t*>(widePool) + op[1], op[2]);
            break;
        case 3:
            buffer->AppendUnicode(static_cast<char16_t>(op[1]));
            break;
        case 4:
            buffer->Close();
            break;
        case 6:
            buffer->mCursor = op[1] == 0xffffffffu ? nullptr : base + op[1];
            break;
        case 7:
            buffer->mOverflow = op[1] != 0;
            break;
        case 8:
            buffer->mConverter = op[1] != 0 ? &converter : nullptr;
            break;
        default: {
            const bool freed = static_cast<int16_t>(op[1]) > 0;
            buffer->~CStaticUnicodeBuffer();
            if (freed) {
                ::operator delete(gBufferBlock);
                gBufferBlock = nullptr;
            }
            gone = true;
            break;
        }
        }
        uint32_t* record = out + static_cast<size_t>(i) * (4 + kGuard + kMaxCells + kGuard);
        record[0] = gone ? 0 : buffer->mCursor == nullptr ? 0xffffffffu : static_cast<uint32_t>(buffer->mCursor - base);
        record[1] = gone ? 0 : static_cast<uint32_t>(buffer->mEnd - base);
        record[2] = gone ? 0 : buffer->mConverter != nullptr ? 1 : 0;
        record[3] = gone ? 0 : buffer->mOverflow ? 1 : 0;
        for (int c = 0; c < kGuard + cells + kGuard; ++c) record[4 + c] = gCells[c];
        if (gone) break;
    }
}

}  // extern "C"
