// Test library for seven small main-app functions (unit v51029; libtpw_unit_CSkyObjects_InitSkyObjects_2.dylib,
// tests/test_CSkyObjects_InitSkyObjects_2.py), main app:
//   src/game/keyboard.cpp, src/game/park/SkyObjects.cpp, src/game/rides/RideDB.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/MessageCentre.cpp   InitSkyObjects, CText_Destroy, the two initialisers, Save, GetWidth, LastCharIn
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The strings
// are a one-buffer fake (the narrow one keeps its text in gStringBuffer so the path the code builds can be read back). The globals of
// other files the unit uses are defined here (zero at start like the Mac's bss).
// The wrapper's GetCharWidth is a recording fake here (the Mac's GetWidth and LastCharIn call it through the stubbed virtual), not
// the header's weak stand-in.
#define TPW_V51039_STRONG
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "SkyObjects.h"
#include "MainBalance.h"
#include "tb_sky.h"
#include "TPGlobals.h"
#include "Main.h"
#include "FS_System.h"
#include "Text.h"
#include "TPEngine.h"
#include "MslSubscriptionTree.h"
#include "InterfaceFontWrapper.h"
#include "TbDynamicStringTemplate.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "Debug.h"
#include "v51029_decls.h"

using namespace calllog;

// ---- globals of other files, defined here
CMainBalance gMainBalance;
TPGlobals gTPGlobals;
CSky gei_Sky;
CRideDatabase gRideDatabase;
TbStringBase<char> gQuickSaveSuffix;

namespace {
char gStringBuffer[512];
char* gThemePath = nullptr;
alignas(16) char gTextBuffer[0x4000];   // stands for gText (0x1037ff18) and for the font wrapper object
}  // namespace

CMainBalance::~CMainBalance() {}
void CBalanceTable::Init() {}
const BalanceDescription* CMainBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CMainBalance::GetBalanceValuesStart() { return mValues; }
CSky::CSky() {}
CSky::~CSky() {}

namespace {
// A float's bits with every NaN made one value (the two processors make different NaNs).
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}
}  // namespace

// ---- string fakes (narrow: a text kept in one buffer; wide: only the constructor from text is recorded)
template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate() {
    ev("StrCtor", {}, {});
    mLength = 0;
    mData = gStringBuffer;
    mCapacity = 0;
    mGrowBy = 0;
    gStringBuffer[0] = 0;
}
template<> TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    ev("StrCtorText", {lab(this)}, {text});
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
}
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() { ev("StrDtor", {-1}, {}); }
template<> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template<> void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template<> void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate<char>&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char* text) {
    ev("StrAssign", {}, {text});
    std::strcpy(gStringBuffer, text);
    return *this;
}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>& text) {
    ev("StrAppendBase", {}, {text.mData});
    std::strcat(gStringBuffer, text.mData);
    return *this;
}
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template<> TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char* text) {
    ev("StrAppend", {}, {text});
    std::strcat(gStringBuffer, text);
    return *this;
}

template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() {}
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("WStrCtorText", {lab(this)}, {narrow(text).c_str()});
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
}
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template<> void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }

TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}

// ---- the other fakes
int32_t CSky::LoadAdd(void** out, char* path, D3DVECTOR position, float size, int32_t a, int32_t b) {
    uint32_t hash = 0x811C9DC5u;
    for (const char* p = path; *p; ++p) hash = (hash ^ static_cast<uint8_t>(*p)) * 0x01000193u;
    ev("LoadAdd", {lab(this), CanonBits(position.x), CanonBits(position.y), CanonBits(position.z), CanonBits(size), a, b,
                   hash, static_cast<int64_t>(std::strlen(path))}, {});
    *out = reinterpret_cast<void*>(static_cast<uintptr_t>(ret("LoadOut")));
    return static_cast<int32_t>(ret("LoadAdd"));
}

char* get_theme_path() {
    ev("ThemePath", {}, {});
    return gThemePath;
}

void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    ev("AddSep", {}, {path.mData});
    std::strcat(path.mData, "/");
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    const uint8_t* bytes = static_cast<const uint8_t*>(buffer);
    ev("LbFile_Write", {lab(file), bytes[0], bytes[1], length}, {});
    *transferred = static_cast<uint32_t>(ret("Written"));
    return TbError(0);
}

void DEBUG_MESSAGE(const char* message, uint32_t flags) { ev("DebugMessage", {flags}, {message}); }

extern "C" void __destroy_arr(void* array, void* destructor, int32_t size, int32_t count) {
    ev("DestroyArr", {lab(array), lab(destructor), size, count}, {});
}
extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("Register", {lab(object), lab(destructor), lab(node)}, {});
}

void IosBaseInitConstruct(void* object) { ev("IosInit", {lab(object)}, {}); }
void CText_OperatorDelete(void* object) { ev("OperatorDelete", {lab(object)}, {}); }
void UIMain_DestroyWString(void*) {}
void UIMain_DestroyString(void*) {}

TbFontRenderParams::~TbFontRenderParams() { ev("ParamsDtor", {lab(this), -1}, {}); }
TbTextResourceManager::~TbTextResourceManager() { ev("ManagerDtor", {lab(this), -1}, {}); }
InterfaceFont::~InterfaceFont() {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}

int32_t InterfaceFont::GetCharWidth(char16_t character) {
    ev("CharWidth", {lab(this), character}, {});
    return static_cast<int32_t>(ret("CharWidth"));
}

int32_t InterfaceFontWrapper::GetCharWidth(char16_t character) {
    ev("CharWidth", {lab(this), character}, {});
    return static_cast<int32_t>(ret("CharWidth"));
}

extern "C" {

// The host address of a global (or function) by the module's name for it.
uint64_t tpw_sky_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"geisky", &gei_Sky},
        {"bal", &gMainBalance},
        {"sky", &gTPGlobals.mF4BA0},
        {"text", gTextBuffer},
        {"fontvec", reinterpret_cast<const void*>(&FontWrapperVectorDestroy)},
        {"wide", gKeyboardWideString},
        {"node_w", gKeyboardWideNode},
        {"node_s", gKeyboardQuickNode},
        {"quick", &gQuickSaveSuffix},
        {"d_wstr", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
        {"d_str", reinterpret_cast<const void*>(&UIMain_DestroyString)},
        {"ios", gRideDBIosInit},
        {"node_i", gRideDBIosNode},
        {"node_r", gRideDBNode},
        {"db", &gRideDatabase},
        {"d_ios", reinterpret_cast<const void*>(&IosBaseInitDestroy)},
        {"d_ride", reinterpret_cast<const void*>(&RideDatabaseDestroy)},
        {"f28c", &gKeyboardF28C},
        {"f288", &gKeyboardF288},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sky_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

#define OFS(type, member) \
    static_cast<int64_t>(reinterpret_cast<char*>(&reinterpret_cast<type*>(gTextBuffer)->member) - gTextBuffer)

// The host offsets the module needs: CText parts, the ride database parts, TbStringBase::mData.
int32_t tpw_sky_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = OFS(CText, mResources);
    out[n++] = OFS(CText, mFonts);
    out[n++] = OFS(CText, mRenderParams);
    out[n++] = static_cast<int64_t>(sizeof(CText));
    out[n++] = static_cast<int64_t>(sizeof(reinterpret_cast<CRideDatabase*>(gTextBuffer)->mRides));
    out[n++] = OFS(CRideDatabase, mRideCount);
    out[n++] = OFS(CRideDatabase, mNames);
    out[n++] = OFS(CRideDatabase, mF72C) + 4;
    out[n++] = OFS(CRideDatabase, mCachedId);
    out[n++] = OFS(CRideDatabase, mCachedRide);
    out[n++] = static_cast<int64_t>(sizeof(CRideDatabase));
    out[n++] = OFS(TbStringBase<char>, mData);
    return n;
}

void tpw_sky_setbal(int32_t slot, uint64_t value) { gMainBalance.mValues[slot].mBits = static_cast<uintptr_t>(value); }
void tpw_sky_settheme(uint64_t text) { gThemePath = reinterpret_cast<char*>(static_cast<uintptr_t>(text)); }

void tpw_sky_init(uint64_t self) { reinterpret_cast<CSkyObjects*>(static_cast<uintptr_t>(self))->InitSkyObjects(); }
uint64_t tpw_sky_text(uint64_t self, int32_t flag) {
    return reinterpret_cast<uintptr_t>(CText_Destroy(reinterpret_cast<CText*>(static_cast<uintptr_t>(self)), static_cast<int16_t>(flag)));
}
void tpw_sky_ride() { __sinit_RideDB_cpp(); }
void tpw_sky_kbd() { __sinit_keyboard_cpp(); }
int32_t tpw_sky_save(uint64_t self, uint64_t file) {
    return reinterpret_cast<CSubscription*>(static_cast<uintptr_t>(self))->SaveResult(*reinterpret_cast<void**>(static_cast<uintptr_t>(file)));
}
int32_t tpw_sky_width(uint64_t self, uint64_t text, int32_t first, int32_t last) {
    return reinterpret_cast<InterfaceFontWrapper*>(static_cast<uintptr_t>(self))
        ->GetWidth(reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(text)), static_cast<int16_t>(first), static_cast<int16_t>(last));
}
int32_t tpw_sky_lastchar(uint64_t self, uint64_t text, int32_t first, int32_t last) {
    return reinterpret_cast<InterfaceFontWrapper*>(static_cast<uintptr_t>(self))
        ->LastCharIn(reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(text)), static_cast<int16_t>(first), static_cast<int16_t>(last));
}

}  // extern "C"
