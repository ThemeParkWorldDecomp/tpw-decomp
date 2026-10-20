// Test library for the island lobby's controller, the global master save and the ride names (libtpw_unit_IslandLobby.dylib; see
// tests/test_IslandLobby.py): src/UI_IslandLobby.cpp (CreateIslandLobbyController, ShowIslandLobbyController, UpdateIslandLobbyUI,
// UpdateIslandLobbyKeyDisplay, EnableIslandLobbyButtons, UpdateOutboxMailIcon), src/FS_MasterSave_global.cpp (the global master
// save, the park record, the park balance table's two virtual members), src/RideDB_names.cpp (the name builder and the ride
// database's GetTypeName) and src/InterfaceWindow_frame.cpp (SetFrame), all of unit w32. Real under them: the bullfrog strings (the
// theme map's keys), the 16-bit C library (swprintf, wcslen, wcsncpy) and the rectangles.
//
// Everything else is a fake that records through calllog: the window toolkit (Build, GetWindow, Show, GetPosition, the setters,
// the virtual Enable and SetText), the draw objects (Change), the text objects' constructors, UI_SendMessage, the font table, the
// particle emitters, the balance table (Init, UnInit and Load, whose answer and values the world scripts per theme), the trace
// output, the user info's constructor and the string table's FormatString (its text comes from the world).
//
// The world: the controller's nine windows (each with a draw object and a client draw object), the lobby window and the mail
// icon, four fonts, eight islands (the lobby's screen system reaches them through plain blocks: the unit never builds the screens'
// classes), eight themes whose balance files the world scripts, a ride database with six ride types and a string table. The
// Python module drives it through the operations of tpw_il_op and reads the state with tpw_il_dump.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "CBalanceTable.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "InterfaceWindow.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbFontRenderParams.h"
#include "TbITable.h"
#include "TbTextResourceManager.h"
#include "Text.h"
#include "UI_Base.h"
#include "calllog.h"
#include "outbox_mail.h"
#include "lobby.h"
#include "platform/msl.h"
#include "rsse.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "ui_textmanager.h"

using namespace calllog;

// ---- globals of other files
CText gText;
CLBLobby* gLobby;
CLBScreenSystem* gLobbyScreens;
CFSUserInfo* gUserInfo;
CFSGlobalMasterSave* gMasterSave;
CRideDatabase gRideDatabase;

InterfaceFont::InterfaceFont() {}   // the fake leaves the object as it finds it
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
Outbox::Outbox() {}
Outbox::~Outbox() {}
AddressBook::AddressBook() {}
AddressBook::~AddressBook() {}
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbStaticStringTemplate<char>::Format(const char*, ...) {}
int32_t IslandLobbyControllerMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
CLBLobby::CLBLobby() {}
void CLBLobby::Register(CLBLobbySystem*) {}
void CLBLobby::UnRegister(CLBLobbySystem*) {}

// (RideInfo::~RideInfo, 0x1015457c, is declared since unit w81 made ClearDatabase real: the ride records here are never destroyed by code under test)
RideInfo::~RideInfo() {}

namespace {
struct Balance {
    int32_t ok;     // Load's answer
    int32_t cost;   // slot 3
    int32_t gate;   // slot 4
};

// ---- the world's constants (the same as the Python module's)
constexpr int kThemes = 8;
// (the first four are the game's themes, whose global.sam files give these costs and gate objects)
const char* const kThemeNames[kThemes] = {"hallow", "space", "jungle", "fantasy", "Hollywood", "alpha", "space2", "Alpha"};
const Balance kThemeBalance[kThemes] = {{1, 1, 2601}, {1, 5, 3601}, {1, 1, 1601}, {1, 3, 4601}, {1, 2, 1150}, {1, 2, 1150}, {1, 0, 1190}, {1, 2, 1180}};
constexpr int kScratch = kThemes;   // the park id of the scratch park record
constexpr int kRides = 6;

// The controller's windows, in the order of their labels (10 + index).
enum { kWinCtrl, kWinKeyCost, kWinDigit, kWinCount, kWinIcon, kWinText, kWinName, kWinButtonA, kWinButtonB, kWinMail, kWinLobby, kWindows };
const int32_t kWinIds[kWindows] = {0, 0x1e0ea, 0x1e0eb, 0x1e0ec, 0x1e0ed, 0x1e0ee, 0x1e0ef, 0x1e0f0, 0x1e0f1, 0xbf432, 0};
constexpr int kWindowLabel = 10, kDrawLabel = 30, kFontLabel = 60, kIslandLabel = 70, kTableLabel = 90;
constexpr int kIslands = 8;
constexpr int kFonts = 4;
constexpr int kBufferUnits = 0x90;   // the output buffer of the name operations (filled with 0xaaaa first)

InterfaceWindow* gWin[kWindows];
InterfaceFont gFonts[kFonts];
CLBIsland* gIslands[kIslands];
CLBScreenComponentThemeIslands* gThemeScreen;
CLBScreenSystem* gScreens;
CLBLobby* gLobbyObject;
Balance gBalance[kThemes];
CFSGlobalParkData* gScratchPark;
int32_t gOutboxWord;
int32_t gTop = 10, gBottom = 40;   // what GetPosition writes
char16_t gBuffer[kBufferUnits];
RideInfo gRideRecords[kRides];
std::u16string gRideNames[kRides];
std::u16string gStrings[2048];       // the string table (OBJECT_NAMES) by number
char gTableBlob[16];                 // the string table object (only its address is used)
int32_t gGameType;
int32_t gNextEmitter = 7;   // the handles PAR_InitEmitter gives out

// A fake draw object: records Change (the frame for flag 1).
class FakeDraw : public InterfaceDrawObject {
public:
    FakeDraw() : InterfaceDrawObject(nullptr) {}
    void Change(uint32_t flags, DOChange* change) override { ev("Change", {lab(this), flags, (flags & 1) != 0 ? change->mFrame : 0}); }
};
FakeDraw* gDraws[kWindows][2];

// The theme a balance file path belongs to ("data:levels:<theme>:global.sam"), or -1.
int ThemeOfPath(const char* path) {
    const char* prefix = "data:levels:";
    const char* suffix = ":global.sam";
    if (std::strncmp(path, prefix, std::strlen(prefix)) != 0) return -1;
    std::string name(path + std::strlen(prefix));
    const std::string tail(suffix);
    if (name.size() < tail.size() || name.compare(name.size() - tail.size(), tail.size(), tail) != 0) return -1;
    name.resize(name.size() - tail.size());
    for (int i = 0; i < kThemes; ++i)
        if (name == kThemeNames[i]) return i;
    return -1;
}

// A wide text as the recorders write it, in a buffer of the caller's (the fakes must not touch the heap: it is counted).
const char* NarrowInto(char (&out)[160], const char16_t* text) {
    if (text == nullptr) return "(null)";
    std::size_t n = 0;
    for (; text[n] != 0 && n < 159; ++n) out[n] = text[n] < 0x80 ? static_cast<char>(text[n]) : '?';
    out[n] = 0;
    return out;
}
}  // namespace

// ---- the window toolkit
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t)
    : mDrawObject(nullptr), mClientDrawObject(nullptr) {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
InterfaceWindow::~InterfaceWindow() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t on) {
    ev("Window.Enable", {lab(this), on});
    return 0;
}
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 1; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t* text) {
    char buffer[160];
    ev("Window.SetText", {lab(this)}, {NarrowInto(buffer, text)});
    return 1;
}
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Window.Show", {lab(this), on});
    return 1;
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("Window.GetWindow", {lab(this), id});
    for (int i = 0; i < kWindows; ++i)
        if (kWinIds[i] == id && id != 0) return gWin[i];
    return nullptr;
}
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Window.Build", {lab(this), description == gIslandLobbyWindowData, callback == IslandLobbyControllerMsg});
    return gWin[kWinCtrl];
}
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("Window.GetPosition", {lab(this), subtract});
    rect->mLeft = 3;
    rect->mTop = static_cast<int16_t>(gTop);
    rect->mRight = 9;
    rect->mBottom = static_cast<int16_t>(gBottom);
    return 1;
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("Window.SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("Window.SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetClientDrawText(char16_t* text) {
    char buffer[160];
    ev("Window.SetClientDrawText", {lab(this)}, {NarrowInto(buffer, text)});
}
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("Window.SetFontJust", {lab(this), horizontal, vertical}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("Window.SetFontColour", {lab(this), red, green, blue, alpha});
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

// ---- the draw objects: the base class's members are never used
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

// ---- the sprite draw object (w30b), the base of the text objects since w35c: never used here, but the vtables need its members
InterfaceDrawObjectSprite::InterfaceDrawObjectSprite(void* data) : InterfaceDrawObject(data) {}
InterfaceDrawObjectSprite::~InterfaceDrawObjectSprite() {}
void InterfaceDrawObjectSprite::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceDrawObjectSprite::Show(int16_t) {}
void InterfaceDrawObjectSprite::Refresh() {}
int32_t InterfaceDrawObjectSprite::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
void InterfaceDrawObjectSprite::UpdateTextures(char16_t*) {}
void InterfaceDrawObjectSprite::CreateTextures(char16_t*) {}

// ---- the text objects (made by `new`; the constructors give them labels). The original's hooks replace each constructor
// whole, so the title object's construction must not also log the text sprite's constructor, which C++ runs first as its
// base: the flag silences it for that one construction (rewritten at the w35c merge, 2026-10-12, for the real class shapes).
namespace { bool gInTitleTextCtor = false; }
InterfaceTextObjectSprite::InterfaceTextObjectSprite(void* data) : InterfaceDrawObjectSprite(data) {
    if (!gInTitleTextCtor) ev("TextSprite.ctor", {newlab(this), data != nullptr});
}
InterfaceTextObjectSprite::~InterfaceTextObjectSprite() {}
void InterfaceTextObjectSprite::Change(uint32_t, DOChange*) {}
void InterfaceTextObjectSprite::UpdateTextures(char16_t*) {}
InterfaceTitleTextObject::InterfaceTitleTextObject(void* data) : InterfaceTextObjectSprite((gInTitleTextCtor = true, data)) {
    gInTitleTextCtor = false;
    ev("TitleText.ctor", {newlab(this), data != nullptr});
}
InterfaceTitleTextObject::~InterfaceTitleTextObject() {}
void InterfaceTitleTextObject::UpdateTextures(char16_t*) {}

// ---- the members those classes own by value (the engine's sprite surface and list of w33b, the ltms text formatter and font
// renderer of w35a; TbFontRenderParams is faked above): never used here, but their constructors and destructors must exist to
// link. None logs: the original's hooks replace the text objects' constructors whole, so none of this runs there either.
SSpriteGameData::SSpriteGameData() {}
SSpriteEngineData::SSpriteEngineData() {}
SSpriteEngineData::~SSpriteEngineData() {}
CMapLinkBase::~CMapLinkBase() {}
CSpriteList::CSpriteList() {}
CSpriteList::~CSpriteList() {}
int32_t CSpriteList::Prepare() { return 1; }
int32_t CSpriteList::GetLinkType() { return 0; }
void* CSpriteList::GetData() { return nullptr; }
TbFontRender::TbFontRender() : mMethod(nullptr) {}
TbFontRender::~TbFontRender() {}
TbTextFormatter::TbTextFormatter() {}
TbTextFormatter::~TbTextFormatter() {}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return index < static_cast<uint32_t>(kFonts) ? &gFonts[index] : nullptr;
}

// ---- the particles, the trace output, the game type, the user info
uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return static_cast<uint32_t>(gNextEmitter++);
}
void PAR_KillEmitter(int32_t handle) { ev("PAR_KillEmitter", {handle}); }

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    // (the trace lines of this unit that take an argument take one text)
    const char* text = std::strstr(format, "%s") != nullptr ? va_arg(args, const char*) : nullptr;
    va_end(args);
    if (text != nullptr) ev("CTRACE", {level}, {format, text});
    else ev("CTRACE", {level}, {format});
}

CGameType::CGameType() : mType(static_cast<GameType>(gGameType)) {}

CFSUserInfo::CFSUserInfo() {
    ev("CFSUserInfo.ctor", {newlab(this)});
    outbox_mail::Set(mOutbox, gOutboxWord);
}

// ---- the balance table: Init and UnInit are recorded; Load answers per theme and fills the values the game reads
void CBalanceTable::Init() { ev("Balance.Init", {newlab(this)}); }
void CBalanceTable::UnInit() { ev("Balance.UnInit", {lab(this)}); }
int32_t CBalanceTable::Load(const char* path, LoadMode mode) {
    ev("Balance.Load", {lab(this), mode}, {path});
    const int theme = ThemeOfPath(path);
    if (theme < 0 || gBalance[theme].ok == 0) return 0;
    auto* table = static_cast<CGlobalParkDataBalance*>(this);
    table->mValues[3].mInteger = gBalance[theme].cost;
    table->mValues[4].mInteger = gBalance[theme].gate;
    return 1;
}

// ---- the string table: FormatString writes the world's text for the number
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("FormatString", {lab(this), size, id});
    const std::u16string& text = id < 2048 ? gStrings[id] : gStrings[0];
    if (size == 0) return 0;
    const uint32_t count = text.size() < size - 1 ? static_cast<uint32_t>(text.size()) : size - 1;
    for (uint32_t i = 0; i < count; ++i) out[i] = text[i];
    out[count] = 0;
    return static_cast<int32_t>(count);
}

// (the real array is src/ui_main.cpp's, which this library does not link)
char16_t gUIMainConvertBuffer[0x4000];

extern "C" {

// Names the world's fixed objects for the call log and sets the globals as the Mac's data has them.
void tpw_il_reset() {
    for (int i = 0; i < kWindows; ++i) {
        if (gWin[i] == nullptr) {
            gWin[i] = new InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0);
            if (i < kWinMail) {
                gDraws[i][0] = new FakeDraw;
                gDraws[i][1] = new FakeDraw;
            }
        }
        gWin[i]->mDrawObject = i < kWinMail ? gDraws[i][0] : nullptr;
        gWin[i]->mClientDrawObject = i < kWinMail ? gDraws[i][1] : nullptr;
        tpw_cl_label(kWindowLabel + i, gWin[i]);
        if (i < kWinMail) {
            tpw_cl_label(kDrawLabel + 2 * i, gDraws[i][0]);
            tpw_cl_label(kDrawLabel + 2 * i + 1, gDraws[i][1]);
        }
    }
    for (int i = 0; i < kFonts; ++i) tpw_cl_label(kFontLabel + i, &gFonts[i]);
    tpw_cl_label(kTableLabel, gTableBlob);
    // the lobby, its screens and islands: plain blocks (only their data members are read)
    if (gLobbyObject == nullptr) {
        gLobbyObject = new CLBLobby;
        gScreens = static_cast<CLBScreenSystem*>(std::calloc(1, sizeof(CLBScreenSystem)));
        gThemeScreen = static_cast<CLBScreenComponentThemeIslands*>(std::calloc(1, sizeof(CLBScreenComponentThemeIslands)));
        for (int i = 0; i < kIslands; ++i) {
            gIslands[i] = static_cast<CLBIsland*>(std::calloc(1, sizeof(CLBIsland)));
            new (&gIslands[i]->mName) TbDynamicStringTemplate<char>(kThemeNames[i]);
        }
    }
    static const char16_t* const kTitles[kIslands] = {u"Spooky", u"Space Zone", u"Jungle", u"Hollywood", u"Fantasy", u"Alpha Isle", u"", u"A long island title"};
    for (int i = 0; i < kIslands; ++i) {
        new (&gIslands[i]->mTitle) TbDynamicStringTemplate<char16_t>(kTitles[i]);
        tpw_cl_label(kIslandLabel + i, gIslands[i]);
    }
    gLobbyObject->mWindow = gWin[kWinLobby];
    gLobby = gLobbyObject;
    gScreens->mThemeIslands = gThemeScreen;
    gLobbyScreens = gScreens;
    gThemeScreen->mCurrent = nullptr;
    gIslandLobbyController = nullptr;
    gIslandLobbyKeyEmitter = 0;
    gIslandLobbyKeysShown = 0;
    gMasterSave = nullptr;
    gUserInfo = nullptr;
    gScratchPark = nullptr;
    gOutboxWord = 0;
    gGameType = 0;
    TheGameType().mType = kGameTypeNormal;
    gNextEmitter = 7;
    gTop = 10;
    gBottom = 40;
    for (int i = 0; i < kThemes; ++i) gBalance[i] = kThemeBalance[i];
    // the ride database: six ride types, nothing cached
    std::memset(&gRideDatabase, 0, sizeof gRideDatabase);
    gRideDatabase.mRideCount = 0;
    tpw_cl_label(80, &gText);
    gText.mResourcePointers[20] = gTableBlob;
    std::memset(gUIMainConvertBuffer, 0, sizeof gUIMainConvertBuffer);
}

// The ride types of a case: ride k has the number `id` and a name of `kind`: 0 none (a NULL pointer), 1 the loader's
// "MISSING STRING", 2 the given text.
void tpw_il_ride(int32_t k, int32_t id, int32_t kind, const char16_t* text) {
    gRideRecords[k].mId = static_cast<uint16_t>(id);
    gRideNames[k] = text != nullptr ? text : u"";
    gRideRecords[k].mName = kind == 0 ? nullptr : kind == 1 ? gRideMissingName : gRideNames[k].c_str();
    gRideDatabase.mRides[k] = &gRideRecords[k];
    gRideDatabase.mRideCount = k + 1;
}

// A string of the string table.
void tpw_il_string(int32_t id, const char16_t* text) { gStrings[id] = text; }

// The record of ride k (for the dump).
void* tpw_il_ride_record(int32_t k) { return &gRideRecords[k]; }

// One operation of the script (the numbers are the Python module's); a[] are its arguments. `out` receives the answer: out[0] is
// the number of values, then the values.
void tpw_il_op(int32_t op, const int64_t* a, int64_t* out);
void tpw_il_dump(int64_t* out);
void tpw_il_heap(int64_t* out) {
    out[0] = tpw_cl_heap[0];
    out[1] = tpw_cl_heap[1];
    out[2] = tpw_cl_heap[2];
    out[3] = tpw_cl_heap[3];
}

}  // extern "C"

namespace {
// The answer and the dump are built in a fixed array and copied out (no heap traffic).
int64_t gOutBuf[4096];
std::size_t gOutCount;
struct OutVector {
    void clear() { gOutCount = 0; }
    std::size_t size() const { return gOutCount; }
    int64_t operator[](std::size_t i) const { return gOutBuf[i]; }
};
OutVector gOut;
void Put(int64_t value) {
    if (gOutCount < 4000) gOutBuf[gOutCount++] = value;
}
void PutText16(const char16_t* text, int units) {
    bool ended = false;
    for (int i = 0; i < units; ++i) {
        if (!ended && text[i] == 0) ended = true;
        Put(ended ? 0 : text[i]);
    }
}
void PutText8(const char* text, int units) {
    bool ended = false;
    for (int i = 0; i < units; ++i) {
        if (!ended && text[i] == 0) ended = true;
        Put(ended ? 0 : static_cast<unsigned char>(text[i]));
    }
}
void Flush(int64_t* out) {
    out[0] = static_cast<int64_t>(gOut.size());
    for (std::size_t i = 0; i < gOut.size(); ++i) out[1 + i] = gOut[i];
}

CFSGlobalParkData* ParkOf(int which) {
    if (which == kScratch) return gScratchPark;
    if (which < 0 || which >= kThemes || gMasterSave == nullptr) return nullptr;
    const auto found = gMasterSave->mThemes.find(TbDynamicStringTemplate<char>(kThemeNames[which]));
    return found == gMasterSave->mThemes.end() ? nullptr : found->second;
}

void PutPark(const CFSGlobalParkData* park) {
    Put(park != nullptr);
    if (park == nullptr) return;
    for (int i = 0; i < 6; ++i) Put(park->mTickets[i]);
    for (int i = 0; i < 4; ++i) Put(park->mAwards[i]);
    Put(park->mBalance != nullptr);
    Put(park->mHasSignNames);
    Put(park->mAllResearchCompleted);
    PutText8(park->mName.mData, 12);
    PutText16(park->mSignNameA, 12);
    PutText16(park->mSignNameB, 12);
}

int RideIndex(const RideInfo* ride) {
    for (int i = 0; i < kRides; ++i)
        if (ride == &gRideRecords[i]) return i;
    return -1;
}

void FillBuffer() {
    for (char16_t& unit : gBuffer) unit = 0xaaaa;
}
void PutBuffer() { for (int i = 0; i < kBufferUnits; ++i) Put(gBuffer[i]); }

const char16_t* const kSignTexts[] = {u"", u"Spooky", u"Park-", u"Hollow-", u"Big-", u"Fun Land", u"a", u"Twenty-nine characters long tex",
                                 u"0123456789012345678901234567890-", u"-"};
constexpr int kSigns = 10;
}  // namespace

extern "C" {

void tpw_il_op(int32_t op, const int64_t* a, int64_t* out) {
    gOut.clear();
    const int which = static_cast<int>(a[0]);
    switch (op) {
        case 1: CreateIslandLobbyController(); break;
        case 2: ShowIslandLobbyController(); break;
        case 3: UpdateIslandLobbyUI(); break;
        case 4: UpdateIslandLobbyKeyDisplay(); break;
        case 5: EnableIslandLobbyButtons(a[0] != 0); break;
        case 6: UpdateOutboxMailIcon(); break;
        case 7: gWin[which]->SetFrame(static_cast<int32_t>(a[1])); break;
        case 10: gMasterSave = new CFSGlobalMasterSave; break;
        case 11: gMasterSave->ResetGlobalData(); break;
        case 12: Put(gMasterSave->HowManyKeys()); break;
        case 13: {
            const TbDynamicStringTemplate<char> name(kThemeNames[which]);
            CFSGlobalParkData* park = gMasterSave->GetInfoOn(name);
            Put(park == nullptr);
            Put(park != nullptr && park == ParkOf(which));
            break;
        }
        case 14: {
            const TbDynamicStringTemplate<char> name(kThemeNames[which]);
            CFSGlobalParkData* park = gMasterSave->CreateNewTheme(name);
            Put(park == nullptr);
            Put(park != nullptr && park == ParkOf(which));
            break;
        }
        case 15: {
            const TbDynamicStringTemplate<char> name(kThemeNames[which]);
            gScratchPark = new CFSGlobalParkData(name);
            break;
        }
        case 16: {
            CFSGlobalParkData* park = ParkOf(which);
            switch (a[1]) {
                case 0: Put(park->HowManyTickets()); break;
                case 1: Put(park->KeyCostToEnter()); break;
                case 2: Put(park->CheckBalanceFile()); break;
                default:
                    FillBuffer();
                    park->GetFullName(gBuffer, static_cast<uint32_t>(a[2]));
                    PutBuffer();
                    break;
            }
            break;
        }
        case 20:
            FillBuffer();
            Put(gRideDatabase.mNames.GetNameA(static_cast<int32_t>(a[0]), gBuffer, static_cast<int32_t>(a[1]), a[2] != 0));
            PutBuffer();
            break;
        case 21:
            FillBuffer();
            Put(gRideDatabase.mNames.GetNameB(static_cast<int32_t>(a[0]), gBuffer, static_cast<int32_t>(a[1])));
            PutBuffer();
            break;
        case 22:
            FillBuffer();
            Put(gRideDatabase.mNames.GetName(static_cast<int32_t>(a[0]), gBuffer, static_cast<int32_t>(a[1])));
            PutBuffer();
            break;
        case 23:
            FillBuffer();
            gRideDatabase.GetTypeName(static_cast<uint16_t>(a[0]), gBuffer, static_cast<uint32_t>(a[1]));
            PutBuffer();
            break;
        case 24: {
            // a balance table object made as the inline constructor makes it (its Init is recorded), then the two virtual members
            CGlobalParkDataBalance* table = new CGlobalParkDataBalance;
            const BalanceDescription* row = table->GetBalanceDescription(which);
            Put(row->mType);
            PutText8(row->mName, 24);
            Put(row->mMinInteger);
            Put(row->mMaxInteger);
            std::uint32_t bits;
            std::memcpy(&bits, &row->mMinFloat, 4);
            Put(bits);
            std::memcpy(&bits, &row->mMaxFloat, 4);
            Put(bits);
            Put(row->mArrayCount);
            Put(row->mFlags);
            Put(table->GetBalanceValuesStart() == table->mValues);
            break;
        }
        case 30: gGameType = static_cast<int32_t>(a[0]); TheGameType().mType = static_cast<GameType>(a[0]); break;
        case 31: gThemeScreen->mCurrent = which < 0 ? nullptr : gIslands[which]; break;
        case 32:
            // 0 no screen system, 1 no theme screen, 2 and 3 the theme screen
            gLobbyScreens = which == 0 ? nullptr : gScreens;
            gScreens->mThemeIslands = which == 1 ? nullptr : gThemeScreen;
            break;
        case 33:
            gOutboxWord = which;
            if (gUserInfo != nullptr) outbox_mail::Set(gUserInfo->mOutbox, static_cast<int32_t>(which));
            break;
        case 34: gBalance[which] = {static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3])}; break;
        case 35: gMasterSave = nullptr; break;
        case 36: gUserInfo = nullptr; break;
        case 37:
            if (gMasterSave != nullptr) {
                for (int i = 0; i < 4; ++i) gMasterSave->mGlobalTickets[i] = (a[0] >> i) & 1;
                for (int i = 0; i < 2; ++i) gMasterSave->mSecretTickets[i] = (a[1] >> i) & 1;
                gMasterSave->mExtraKeys = static_cast<int32_t>(a[2]);
                gMasterSave->mTicketsSpent = static_cast<int32_t>(a[3]);
            }
            break;
        case 38: {
            CFSGlobalParkData* park = ParkOf(which);
            if (park != nullptr) {
                for (int i = 0; i < 6; ++i) park->mTickets[i] = (a[1] >> i) & 1;
                for (int i = 0; i < 4; ++i) park->mAwards[i] = (a[2] >> i) & 1;
            }
            break;
        }
        case 39: {
            CFSGlobalParkData* park = ParkOf(which);
            if (park != nullptr) {
                const char16_t* first = kSignTexts[a[1] % kSigns];
                const char16_t* second = kSignTexts[a[2] % kSigns];
                wcsncpy(park->mSignNameA, first, 0x21);
                wcsncpy(park->mSignNameB, second, 0x21);
                park->mHasSignNames = static_cast<uint8_t>(a[3]);
            }
            break;
        }
        case 40: gIslandLobbyKeysShown = static_cast<int32_t>(a[0]); break;
        case 41: gIslandLobbyKeyEmitter = static_cast<int32_t>(a[0]); break;
        case 42: gTop = static_cast<int32_t>(a[0]); gBottom = static_cast<int32_t>(a[1]); break;
        case 43:
            gWin[which]->mDrawObject = a[1] != 0 ? gDraws[which][0] : nullptr;
            gWin[which]->mClientDrawObject = a[2] != 0 ? gDraws[which][1] : nullptr;
            break;
        case 44: gLobbyObject->mWindow = a[0] != 0 ? gWin[kWinLobby] : nullptr; break;
        case 45: gIslandLobbyController = nullptr; break;
        case 46: break;   // (the game type's guard byte: the host's function-local static cannot be re-armed; the PowerPC side's constructor stand-in sets the same type)
        default: break;
    }
    Flush(out);
}

// The state after an operation: the controller's globals, the master save with its theme map in order, the scratch park, the
// name builder's buffers, the output buffer, the ride database's cache and the converted-text buffer.
void tpw_il_dump(int64_t* out) {
    gOut.clear();
    Put(gIslandLobbyController != nullptr);
    Put(gIslandLobbyKeyEmitter);
    Put(gIslandLobbyKeysShown);
    Put(gMasterSave != nullptr);
    Put(gUserInfo != nullptr);
    if (gMasterSave != nullptr) {
        for (int i = 0; i < 4; ++i) Put(gMasterSave->mGlobalTickets[i]);
        Put(gMasterSave->mTicketsSpent);
        Put(gMasterSave->mExtraKeys);
        Put(gMasterSave->mEasyMode);
        Put(gMasterSave->mSwearFilter);
        Put(gMasterSave->mSecretTickets[0]);
        Put(gMasterSave->mSecretTickets[1]);
        Put(gMasterSave->mFirstTimePlayer);
        Put(static_cast<int64_t>(gMasterSave->mThemes.size()));
        for (const auto& entry : gMasterSave->mThemes) {
            PutText8(entry.first.mData, 12);
            PutPark(entry.second);
        }
    }
    PutPark(gScratchPark);
    PutText16(gRideDatabase.mNames.mPartA, 24);
    PutText16(gRideDatabase.mNames.mPartB, 24);
    PutText16(gRideDatabase.mNames.mWhole, 24);
    Put(gRideDatabase.mCachedId);
    Put(RideIndex(gRideDatabase.mCachedRide));
    PutText16(gUIMainConvertBuffer, 40);
    Flush(out);
}

}  // extern "C"
