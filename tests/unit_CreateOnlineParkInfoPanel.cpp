// Test library for the online park info panel (unit v5537; libtpw_unit_CreateOnlineParkInfoPanel.dylib,
// tests/test_CreateOnlineParkInfoPanel.py), main app:
//   src/game/ui/ui_onlineParkInfoPanel.cpp   CreateOnlineParkInfoPanel and the five globals of the panel
//
// Every callee is a recording fake with a PowerPC stub of the same name in the test module. The windows are fake objects (a vtable
// whose SetText slot is the fake, and a token naming the window in the log), the strings are fakes that keep their text in a heap
// buffer (the format fake understands %ls and %d), the sprite objects only carry a kind word. The globals of other files the unit
// reads are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <type_traits>

#include "calllog.h"
#include "platform/arena.h"

#include "Advisor.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "lobby.h"
#include "online.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;

InterfaceWindow* gOnlineParkInfoPanel = nullptr;   // 0x103961e4 (defined in OnlineParkInfoPanelMsg.cpp in the program)

void CreateOnlineParkInfoPanel(const CFSOnlinePark* park, InterfaceWindow* parent, void (*onCancel)(),
                               void (*onDownload)(int32_t, const void*), void* context);

// ---- globals of other files the unit reads or writes, defined here
CLBLobby* gLobby = nullptr;
CLBScreenSystem* gLobbyScreens = nullptr;
CFSGlobalMasterSave* gMasterSave = nullptr;
CFSOnlineSystem* gOnlineSystem = nullptr;
// gGame and gText have constructors that reach far; their storage is plain zeroed bytes under the program's symbol names.
alignas(16) unsigned char gGameBytes[sizeof(CGame)] asm("_gGame");
alignas(16) unsigned char gTextBytes[sizeof(CText)] asm("_gText");
#define gGameRef (*reinterpret_cast<CGame*>(gGameBytes))
#define gTextRef (*reinterpret_cast<CText*>(gTextBytes))
char16_t gUIMainConvertBuffer[0x4000];
int16_t gOnlineParkInfoScript[4] = {1, 2, 3, 4};

namespace {

bool gLogging = false;
#define EV(name, ...)                       \
    do {                                    \
        if (gLogging) ev(name, {__VA_ARGS__}); \
    } while (0)

inline int64_t I(int64_t v) { return v; }

// The module writes arena addresses into 32-bit words (their low half): the arena sits inside one 4 GB window, so the high half
// is the arena base's.
inline uintptr_t FromLow32(uint32_t v) {
    if (v == 0) return 0;
    return (reinterpret_cast<uintptr_t>(tpw::ArenaBase()) & ~static_cast<uintptr_t>(0xFFFFFFFFu)) | v;
}

// The case's configuration (set by tpw_pi_prepare).
int32_t gCfgF60 = 0;
int32_t gCfgSwear = 0;
uint32_t gCfgLogin = 0;
int32_t gCfgZ = 0;

struct Sum {
    int64_t len;
    int64_t hash;
};

// Length and FNV-1a over the characters: how the log names a text.
template <class C>
Sum SumOf(const C* s) {
    if (s == nullptr) return {-1, 0};
    uint32_t h = 2166136261u;
    int64_t n = 0;
    for (; s[n] != 0; ++n) h = (h ^ static_cast<uint32_t>(static_cast<std::make_unsigned_t<C>>(s[n]))) * 16777619u;
    return {n, static_cast<int64_t>(h)};
}

// ---- the windows
struct FakeWin {
    void** vptr;
    uint32_t token;
    uint32_t pad;
};

void* gVt[64];
std::map<uint32_t, FakeWin*> gWins;
char gFonts[16 * 32];
char gDummyTable[16];
CFSOnlineCity gHostCity;

int32_t FakeSetText(FakeWin* self, char16_t* text) {
    Sum m = SumOf(text);
    EV("SetText", I(self->token), m.len, m.hash);
    return 1;
}

FakeWin* WinFor(uint32_t token) {
    auto it = gWins.find(token);
    if (it != gWins.end()) return it->second;
    FakeWin* w = new FakeWin{gVt, token, 0};
    gWins[token] = w;
    return w;
}

FakeWin* W(const InterfaceWindow* w) { return reinterpret_cast<FakeWin*>(const_cast<InterfaceWindow*>(w)); }
uint32_t Tok(const InterfaceWindow* w) { return W(w)->token; }
InterfaceWindow* AsWin(FakeWin* w) { return reinterpret_cast<InterfaceWindow*>(w); }

// ---- the text of a string by width
template <class C>
void PutStr(TbDynamicStringTemplate<C>* s, const std::basic_string<C>& v) {
    C* buf = new C[v.size() + 1];
    std::memcpy(buf, v.data(), v.size() * sizeof(C));
    buf[v.size()] = 0;
    s->mData = buf;
    s->mLength = static_cast<int32_t>(v.size());
    s->mCapacity = static_cast<int32_t>(v.size());
}

template <class C>
std::basic_string<C> GetStr(const C* p) {
    return p != nullptr ? std::basic_string<C>(p) : std::basic_string<C>();
}

std::u16string Widen(const std::string& s) { return std::u16string(s.begin(), s.end()); }

std::u16string FormatImpl(const char16_t* f, va_list ap) {
    std::u16string out;
    while (*f != 0) {
        if (*f != u'%') {
            out += *f++;
            continue;
        }
        ++f;
        if (*f == 0) break;
        if (*f == u'%') {
            out += u'%';
            ++f;
            continue;
        }
        if (*f == u'l') ++f;
        if (*f == 0) break;
        char16_t k = *f++;
        if (k == u's') {
            const char16_t* a = va_arg(ap, const char16_t*);
            if (a != nullptr) {
                out += a;
            } else {
                out += u"(null)";
            }
        } else if (k == u'd') {
            int v = va_arg(ap, int);
            out += Widen(std::to_string(v));
        } else {
            out += u'%';
            out += k;
        }
    }
    return out;
}

void CopyPark(CFSOnlinePark& d, const CFSOnlinePark& s) {
    d.mList = s.mList;
    d.mParkID = s.mParkID;
    d.mText0C.mLength = s.mText0C.mLength;
    d.mText0C.mData = s.mText0C.mData;
    d.mText20.mLength = s.mText20.mLength;
    d.mText20.mData = s.mText20.mData;
    d.mText34.mLength = s.mText34.mLength;
    d.mText34.mData = s.mText34.mData;
    d.mF48 = s.mF48;
    d.mF4C = s.mF4C;
    d.mF50 = s.mF50;
    d.mF54 = s.mF54;
    std::memcpy(d.mF58, s.mF58, sizeof d.mF58);
    d.mF78 = s.mF78;
    d.mText7C.mLength = s.mText7C.mLength;
    d.mText7C.mData = s.mText7C.mData;
    d.mF90 = s.mF90;
    d.mText94.mLength = s.mText94.mLength;
    d.mText94.mData = s.mText94.mData;
    d.mFA8 = s.mFA8;
    d.mFAA = s.mFAA;
    d.mFAC = s.mFAC;
    d.mFB0 = s.mFB0;
    d.mFB4 = s.mFB4;
    d.mFB8 = s.mFB8;
    d.mFBC = s.mFBC;
}

}  // namespace

// ---- constructors the unit does not link (symbol names given directly: the classes have vtables or members of their own)
void FakeSpriteCtor(void* self, void* data) asm("__ZN25InterfaceTextObjectSpriteC1EPv");
void FakeSpriteCtor(void* self, void* data) {
    *reinterpret_cast<uint32_t*>(self) = 1;
    EV("SpriteCtor", I(reinterpret_cast<uintptr_t>(data)));
}
void FakeTitleCtor(void* self, void* data) asm("__ZN24InterfaceTitleTextObjectC1EPv");
void FakeTitleCtor(void* self, void* data) {
    *reinterpret_cast<uint32_t*>(self) = 2;
    EV("TitleCtor", I(reinterpret_cast<uintptr_t>(data)));
}
void FakeOnlineSystemCtor(void* self) asm("__ZN15CFSOnlineSystemC1Ev");
void FakeOnlineSystemCtor(void* self) {
    (void)self;
    EV("OnlineSystemCtor", I(0));
}
void FakeMasterSaveCtor(void* self) asm("__ZN19CFSGlobalMasterSaveC1Ev");
void FakeMasterSaveCtor(void* self) {
    (void)self;
    EV("MasterSaveCtor", I(0));
}
CFSOnlinePark::CFSOnlinePark() {}
CFSOnlinePark::~CFSOnlinePark() {}

// ---- the strings (the fakes of TbDynamicStringTemplate)
template <class C>
TbDynamicStringTemplate<C>::TbDynamicStringTemplate() {
    mGrowBy = 0;
    EV("StrCtor16", I(0));
    PutStr<C>(this, std::basic_string<C>());
}

template <class C>
TbDynamicStringTemplate<C>::TbDynamicStringTemplate(const C* text) {
    mGrowBy = 0;
    Sum m = SumOf(text);
    EV(sizeof(C) == 2 ? "StrCtorPtr16" : "StrCtor8", m.len, m.hash);
    PutStr<C>(this, GetStr(text));
}

template <class C>
TbDynamicStringTemplate<C>::~TbDynamicStringTemplate() {
    EV(sizeof(C) == 2 ? "StrDtor16" : "StrDtor8", I(0));
}

template <class C>
void TbDynamicStringTemplate<C>::Format(const C* fmt, ...) {
    if constexpr (sizeof(C) == 2) {
        va_list ap;
        va_start(ap, fmt);
        std::u16string out = FormatImpl(reinterpret_cast<const char16_t*>(fmt), ap);
        va_end(ap);
        Sum f = SumOf(fmt);
        Sum r = SumOf(out.c_str());
        EV("StrFormat", f.len, f.hash, r.len, r.hash);
        PutStr<C>(this, out);
    }
}

template <class C>
void TbDynamicStringTemplate<C>::ResizeTo(uint32_t) {}

template <class C>
void TbDynamicStringTemplate<C>::AllocBuffer(uint32_t) {}

template <class C>
TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>& other) {
    PutStr<C>(this, GetStr(other.mData));
    return *this;
}

template <class C>
TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate& other) {
    PutStr<C>(this, GetStr(other.mData));
    return *this;
}

template <class C>
TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C* text) {
    Sum m = SumOf(text);
    EV("StrAssign", m.len, m.hash);
    PutStr<C>(this, GetStr(text));
    return *this;
}

template <class C>
TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C& ch) {
    PutStr<C>(this, std::basic_string<C>(1, ch));
    return *this;
}

template <class C>
TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>& text) {
    PutStr<C>(this, GetStr(mData) + GetStr(text.mData));
    return *this;
}

template <class C>
TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C& ch) {
    PutStr<C>(this, GetStr(mData) + ch);
    return *this;
}

template <class C>
TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C* text) {
    Sum m = SumOf(text);
    EV("StrAppend", m.len, m.hash);
    PutStr<C>(this, GetStr(mData) + GetStr(text));
    return *this;
}

template class TbDynamicStringTemplate<char16_t>;
template class TbDynamicStringTemplate<char>;

// ---- the park
CFSOnlinePark& CFSOnlinePark::operator=(const CFSOnlinePark& other) {
    EV("ParkAssign", I(other.mF48), I(other.mF4C));
    CopyPark(*this, other);
    return *this;
}

// ---- the windows
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    EV("Build", I(Tok(this)), I(description == gOnlineParkInfoScript), I(callback == OnlineParkInfoPanelMsg));
    return AsWin(WinFor(2));
}

void InterfaceWindow::setZ(int16_t z) { EV("SetZ", I(Tok(this)), I(z)); }

int32_t InterfaceWindow::ShiftPosition(int16_t dx, int16_t dy) {
    EV("Shift", I(Tok(this)), I(dx), I(dy));
    return 0;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    EV("GetWindow", I(Tok(this)), I(id));
    return AsWin(WinFor(static_cast<uint32_t>(id)));
}

int32_t InterfaceWindow::Show(int16_t on) {
    EV("Show", I(Tok(this)), I(on));
    return 0;
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) {
    EV("SetClientDrawObject", I(Tok(this)), I(*reinterpret_cast<uint32_t*>(object)));
}

void InterfaceWindow::SetFont(InterfaceFont* font) {
    EV("SetFont", I(Tok(this)), I((reinterpret_cast<char*>(font) - gFonts) / 16));
}

void InterfaceWindow::SetClientDrawText(char16_t* text) {
    Sum m = SumOf(text);
    EV("SetClientDrawText", I(Tok(this)), m.len, m.hash);
}

void InterfaceWindow::SetFontWrap(int32_t horizontal, int32_t vertical) {
    EV("SetFontWrap", I(Tok(this)), I(horizontal), I(vertical));
}

void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) {
    EV("SetFontJust", I(Tok(this)), I(horizontal), I(vertical));
}

void InterfaceWindow::CaptureKb(int32_t on) { EV("CaptureKb", I(Tok(this)), I(on)); }

int32_t UI_GetHighestZ(InterfaceWindow* window, CoolRect* rect) {
    EV("HighestZ", I(window != nullptr), I(rect != nullptr));
    return gCfgZ;
}

void UI_TITLE_COLOUR(InterfaceWindow* window) { EV("UI_TITLE_COLOUR", I(Tok(window))); }

InterfaceFont* GetFontTableEntry(uint32_t index) {
    EV("GetFont", I(index));
    return reinterpret_cast<InterfaceFont*>(gFonts + 16 * index);
}

const char16_t* UIMain_GetString(int32_t id) {
    EV("GetString", I(id));
    std::u16string s = u"S" + Widen(std::to_string(id));
    char16_t* buf = new char16_t[s.size() + 1];
    std::memcpy(buf, s.c_str(), (s.size() + 1) * sizeof(char16_t));
    return buf;
}

int32_t OnlineParkInfoPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// ---- the online system
namespace park_info_callee {
CFSOnlineCity* ReturnCityInfo(CFSOnlineSystemCityList*, int32_t city) {
    EV("ReturnCityInfo", I(city));
    gHostCity.mF60 = gCfgF60;
    return &gHostCity;
}
}  // namespace park_info_callee

char* CFSOnlineSystemThemeList::ReturnOnlineThemeName(int32_t theme) {
    EV("ThemeName", I(theme));
    static char buf[64];
    std::snprintf(buf, sizeof buf, "theme%d", theme);
    return buf;
}

void CGame::GetLocalisedThemeName(const TbDynamicStringTemplate<char>& theme, TbDynamicStringTemplate<char16_t>& out) const {
    Sum m = SumOf(theme.mData);
    EV("LocalisedThemeName", I(this == &gGameRef), m.len, m.hash);
    std::u16string s = u"L:";
    for (const char* p = theme.mData; p != nullptr && *p != 0; ++p) s += static_cast<char16_t>(static_cast<unsigned char>(*p));
    PutStr<char16_t>(&out, s);
}

namespace online_callee {
const char16_t* LoginCustomerName(CFSOnlineSystem* sys) {
    EV("LoginName", I(sys != nullptr));
    return reinterpret_cast<const char16_t*>(FromLow32(gCfgLogin));
}
}  // namespace online_callee

bool CFSGlobalMasterSave::IsSwearFilterOn() const {
    EV("IsSwearFilterOn", I(this != nullptr));
    return gCfgSwear != 0;
}

bool SwearFilter::Filter(std::u16string& text, bool replace) {
    Sum m = SumOf(text.c_str());
    EV("Filter", I(this != nullptr), m.len, m.hash, I(replace));
    bool changed = false;
    for (char16_t& c : text) {
        if (c == u'a') {
            c = u'*';
            changed = true;
        }
    }
    return changed;
}

uint32_t wcslen(const char16_t* text) {
    uint32_t n = 0;
    while (text[n] != 0) ++n;
    return n;
}

namespace NS_MacDoze {
int32_t _wcsicmp(const char16_t* a, const char16_t* b) {
    for (;; ++a, ++b) {
        char16_t x = *a, y = *b;
        if (x >= u'A' && x <= u'Z') x = static_cast<char16_t>(x + 32);
        if (y >= u'A' && y <= u'Z') y = static_cast<char16_t>(y + 32);
        if (x != y) return x < y ? -1 : 1;
        if (x == 0) return 0;
    }
}
}  // namespace NS_MacDoze

// The tag's key function: defining it here makes the library emit the tag's vtable (the table fake never calls it).
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

// ---- the table and the advisor
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list ap;
    va_start(ap, id);
    TbIntegerVTag* a = va_arg(ap, TbIntegerVTag*);
    TbIntegerVTag* b = va_arg(ap, TbIntegerVTag*);
    TbIntegerVTag* c = va_arg(ap, TbIntegerVTag*);
    va_end(ap);
    EV("FormatString", I(this != nullptr), I(size), I(id), I(a->mMagic), I(a->mId), I(a->mValue), I(b->mMagic), I(b->mId),
       I(b->mValue), I(c->mMagic), I(c->mId), I(c->mValue));
    std::u16string s = u"FS" + Widen(std::to_string(id));
    TbIntegerVTag* tags[3] = {a, b, c};
    for (TbIntegerVTag* t : tags)
        s += u":" + Widen(std::to_string(static_cast<int64_t>(t->mId))) + u"=" + Widen(std::to_string(static_cast<int32_t>(t->mValue)));
    std::memcpy(out, s.c_str(), (s.size() + 1) * sizeof(char16_t));
    return static_cast<int32_t>(s.size());
}

void CLBScreenComponentAdvisor::Add(AdvisorResponse response, int32_t clearFirst) {
    EV("AdvisorAdd", I(response.mId), I(clearFirst));
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_pi_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineParkInfoPanel", &gOnlineParkInfoPanel},
        {"gOnlineParkInfoCancel", &gOnlineParkInfoCancel},
        {"gOnlineParkInfoDownload", &gOnlineParkInfoDownload},
        {"gOnlineParkInfoContext", &gOnlineParkInfoContext},
        {"gOnlineSystem", &gOnlineSystem},
        {"gMasterSave", &gMasterSave},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_pi_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_pi_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(char16_t));
    return n;
}

// Starts a case: the fake world (windows, lobby, advisor, table, city) and the configuration the fakes answer from.
void tpw_pi_prepare(int32_t f60, int32_t swear, uint32_t login, int32_t z) {
    gLogging = false;
    gCfgF60 = f60;
    gCfgSwear = swear;
    gCfgLogin = login;
    gCfgZ = z;
    gWins.clear();
    std::memset(gVt, 0, sizeof gVt);
    int32_t (InterfaceWindow::*pm)(char16_t*) = &InterfaceWindow::SetText;
    uintptr_t raw[2];
    static_assert(sizeof(pm) == sizeof(raw), "member pointer size");
    std::memcpy(raw, &pm, sizeof pm);
    // arm64 Itanium ABI: the pointer word is the vtable byte offset, the low bit of the adjustment marks a virtual function
    gVt[raw[0] / sizeof(void*)] = reinterpret_cast<void*>(&FakeSetText);
    gLobby = static_cast<CLBLobby*>(tpw::ArenaAllocZeroed(sizeof(CLBLobby)));
    gLobby->mWindow = AsWin(WinFor(1));
    gLobbyScreens = static_cast<CLBScreenSystem*>(tpw::ArenaAllocZeroed(sizeof(CLBScreenSystem)));
    gLobbyScreens->mAdvisor = static_cast<CLBScreenComponentAdvisor*>(tpw::ArenaAllocZeroed(sizeof(CLBScreenComponentAdvisor)));
    gTextRef.mResourcePointers[6] = gDummyTable;
    CFSOnlinePark& g = gOnlineParkInfoPark;
    g.mParkID = 0;
    g.mText0C.mLength = 0;
    g.mText0C.mData = nullptr;
    g.mText20.mLength = 0;
    g.mText20.mData = nullptr;
    g.mText34.mLength = 0;
    g.mText34.mData = nullptr;
    g.mText94.mLength = 0;
    g.mText94.mData = nullptr;
    g.mF48 = g.mF4C = g.mF50 = g.mF54 = 0;
    g.mFA8 = g.mFAA = g.mFAC = 0;
    g.mFB8 = g.mFBC = 0;
}

uint32_t tpw_pi_token(uint64_t p) { return reinterpret_cast<FakeWin*>(static_cast<uintptr_t>(p))->token; }

// The panel's park copy: the numbers and the texts as (length, hash).
int32_t tpw_pi_park_dump(int64_t* out) {
    const CFSOnlinePark& p = gOnlineParkInfoPark;
    int32_t n = 0;
    out[n++] = static_cast<uint32_t>(p.mParkID);
    out[n++] = static_cast<uint32_t>(p.mF48);
    out[n++] = static_cast<uint32_t>(p.mF4C);
    out[n++] = static_cast<uint32_t>(p.mF50);
    out[n++] = static_cast<uint32_t>(p.mF54);
    out[n++] = static_cast<uint16_t>(p.mFA8);
    out[n++] = static_cast<uint16_t>(p.mFAA);
    out[n++] = static_cast<uint16_t>(p.mFAC);
    out[n++] = static_cast<uint32_t>(p.mFB8);
    out[n++] = static_cast<uint32_t>(p.mFBC);
    const char16_t* texts[4] = {p.mText0C.mData, p.mText20.mData, p.mText34.mData, p.mText94.mData};
    for (const char16_t* t : texts) {
        Sum m = SumOf(t);
        out[n++] = m.len;
        out[n++] = m.hash;
    }
    return n;
}

// The call. `block` is the park in Mac layout (Mac-offset words): the host copy is made from it.
void tpw_pi_call(uint64_t block, int32_t hasParent, uint32_t cancel, uint32_t download, uint32_t context) {
    const uint8_t* b = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(block));
    auto word = [&](uint32_t off) {
        uint32_t v;
        std::memcpy(&v, b + off, 4);
        return v;
    };
    auto half = [&](uint32_t off) {
        uint16_t v;
        std::memcpy(&v, b + off, 2);
        return v;
    };
    auto ptr = [&](uint32_t off) { return reinterpret_cast<char16_t*>(FromLow32(word(off))); };
    CFSOnlinePark hp;
    hp.mParkID = static_cast<int32_t>(word(0x08));
    hp.mText0C.mLength = static_cast<int32_t>(word(0x10));
    hp.mText0C.mData = ptr(0x14);
    hp.mText20.mLength = static_cast<int32_t>(word(0x24));
    hp.mText20.mData = ptr(0x28);
    hp.mText34.mLength = static_cast<int32_t>(word(0x38));
    hp.mText34.mData = ptr(0x3c);
    hp.mF48 = static_cast<int32_t>(word(0x48));
    hp.mF4C = static_cast<int32_t>(word(0x4c));
    hp.mF50 = static_cast<int32_t>(word(0x50));
    hp.mF54 = static_cast<int32_t>(word(0x54));
    hp.mText94.mLength = static_cast<int32_t>(word(0x98));
    hp.mText94.mData = ptr(0x9c);
    hp.mFA8 = static_cast<int16_t>(half(0xa8));
    hp.mFAA = static_cast<int16_t>(half(0xaa));
    hp.mFAC = static_cast<int16_t>(half(0xac));
    hp.mFB8 = static_cast<int32_t>(word(0xb8));
    hp.mFBC = static_cast<int32_t>(word(0xbc));
    InterfaceWindow* parent = hasParent != 0 ? AsWin(WinFor(3)) : nullptr;
    gLogging = true;
    CreateOnlineParkInfoPanel(&hp, parent, reinterpret_cast<void (*)()>(static_cast<uintptr_t>(cancel)),
                              reinterpret_cast<void (*)(int32_t, const void*)>(static_cast<uintptr_t>(download)),
                              reinterpret_cast<void*>(static_cast<uintptr_t>(context)));
    gLogging = false;
}

}  // extern "C"
