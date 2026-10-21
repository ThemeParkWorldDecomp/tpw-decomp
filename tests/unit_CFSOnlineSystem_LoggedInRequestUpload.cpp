// Test library for the online upload job and the city unpublish request (unit v5043; libtpw_unit_CFSOnlineSystem_LoggedInRequestUpload.dylib,
// tests/test_CFSOnlineSystem_LoggedInRequestUpload.py), main app:
//   src/game/online/FS_Online.cpp   CFSOnlineSystem::UnpublishPark, RequestUpload, LoggedInRequestUpload,
//                                                                  UploadServerConnectionCallback, ReturnUploadFilename
//
// The callees outside the unit are fakes that record what the real ones read (the same event names and arguments as the module's PowerPC
// stubs): the debug log, the string class (every member the unit reaches), operator new / delete / free, the City and Upload connections,
// the theme list, the other upload job steps, the user info, the text class. The globals of other files the unit reads are defined here:
// the big ones (the globals, the text, the online configuration) are raw storage under the program's symbol names, so that no
// constructor of theirs has to run or be linked.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "LbTime.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "Text.h"
#include "platform/msl.h"
#include "platform/samsutils.h"

using namespace calllog;

// ---- globals of other files, defined here
CFSOnlineSystem* gOnlineSystem = nullptr;
CFSUserInfo* gUserInfo = nullptr;
OnlineWideStringGlobal gUploadNameTail;
// raw zeroed storage under the program's symbol names (larger than the classes)
__asm__(
    ".comm _gText,0x10000,6\n"
    ".comm _gTPGlobals,0x100000,6\n"
    ".comm _gOnlineConfig,0x1000,6\n");

namespace {

// Events are not recorded while the test sets the world up.
bool gMute = false;

inline const char* Txt(const char* text) { return text; }
// The narrowed text lives in one buffer reserved once, so the fakes' own temporaries never reach the logged free.
const std::string& Narrow(const char16_t* text) {
    static std::string out = [] {
        std::string s;
        s.reserve(4096);
        return s;
    }();
    out.clear();
    if (text != nullptr)
        for (; *text != 0 && out.size() < 4000; ++text) out.push_back(static_cast<char>(*text));
    return out;
}
inline const std::string& Txt(const char16_t* text) { return Narrow(text); }
inline const char* Cs(const char* text) { return text; }
inline const char* Cs(const std::string& text) { return text.c_str(); }

uint32_t Hash16(const uint16_t* values, int count, bool untilNul) {
    uint32_t h = 0;
    for (int i = 0; i < count; ++i) {
        if (untilNul && values[i] == 0) break;
        h = h * 31u + values[i];
    }
    return h;
}

uint32_t Hash16(const char16_t* values, int count, bool untilNul) {
    uint32_t h = 0;
    for (int i = 0; i < count; ++i) {
        if (untilNul && values[i] == 0) break;
        h = h * 31u + static_cast<uint16_t>(values[i]);
    }
    return h;
}

}  // namespace

// ---- the runtime: operator new / delete and free (this library does not link the arena, which defines the program's operator new)
static void* Zalloc(std::size_t size) { return std::calloc(1, size); }
// (calllog.cpp records operator new / delete as "operator_new" / "operator_delete" itself)
extern "C" void free(void* pointer) {
    // (the harness's operator delete calls free on a block it has already forgotten: that one is not the program's free)
    if (!gMute && lab(pointer) != -1) ev("free", {static_cast<int64_t>(lab(pointer))});
}

// ---- the debug log and the C library
namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
}  // namespace SamsUtilities

char16_t* wcscpy(char16_t* destination, const char16_t* source) {
    ev("wcscpy", {}, {Narrow(source).c_str()});
    char16_t* out = destination;
    while ((*out++ = *source++) != 0) {
    }
    return destination;
}

// ---- the string class: every virtual of both instances (the vtable needs them) and the constructors; the ones the unit calls record
template <class Char>
TbDynamicStringTemplate<Char>::TbDynamicStringTemplate() {
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
    if (!gMute) ev("StrCtor", {static_cast<int64_t>(newlab(this))});
}
template <class Char>
TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const Char*) {
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
}
template <class Char>
TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbDynamicStringTemplate& other) {
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
    if (!gMute) ev("StrCopy", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(&other))});
}
template <class Char>
TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbStringBase<Char>&) {
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
}
template <class Char>
TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const Char&) {
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
}
template <class Char>
void TbDynamicStringTemplate<Char>::FormatV(const Char*, va_list) {}
template <class Char>
void TbDynamicStringTemplate<Char>::Format(const Char* format, ...) {
    if (!gMute) ev("Format", {static_cast<int64_t>(lab(this))}, {Cs(Txt(format))});
}
template <class Char>
void TbDynamicStringTemplate<Char>::ResizeTo(uint32_t) {}
template <class Char>
void TbDynamicStringTemplate<Char>::AllocBuffer(uint32_t) {}
template <class Char>
TbDynamicStringTemplate<Char>::~TbDynamicStringTemplate() {
    if (!gMute) ev("StrDtor", {static_cast<int64_t>(lab(this)), -1});
}
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbStringBase<Char>&) {
    return *this;
}
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbDynamicStringTemplate& other) {
    if (!gMute) ev("StrAssign", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(&other))});
    return *this;
}
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char*) {
    return *this;
}
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char&) {
    return *this;
}
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const TbStringBase<Char>& text) {
    if (!gMute) ev("StrAppend", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(&text))});
    return *this;
}
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char&) {
    return *this;
}
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char*) {
    return *this;
}

template TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate();
template TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const TbDynamicStringTemplate<char16_t>&);
template TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate();
template TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate<char16_t>&);
template TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&);

// ---- the online system's other steps
void CFSOnlineSystem::InitUpload() { ev("InitUpload", {static_cast<int64_t>(lab(this))}); }
int32_t CFSOnlineSystem::ConnectUploadServer() {
    ev("ConnectUploadServer", {static_cast<int64_t>(lab(this))});
    return static_cast<int32_t>(ret("ConnectUploadServer"));
}
void CFSOnlineSystem::CompleteUpload(int32_t failed) {
    ev("CompleteUpload", {static_cast<int64_t>(lab(this)), failed});
}
void CFSOnlineSystem::RequestUploadPostLoginCallback(int32_t result, const void* context) {
    ev("RequestUploadPostLoginCallback", {static_cast<int64_t>(lab(this)), result, static_cast<int64_t>(lab(context))});
}
// The constructor, under its Itanium names (both of them), because the members of the class have constructors this library does not link.
extern "C" __attribute__((used)) void* tpw_system_ctor_impl(void* self) {
    ev("CFSOnlineSystem_ctor", {static_cast<int64_t>(newlab(self))});
    return self;
}
__asm__(
    ".text\n.p2align 2\n.globl __ZN15CFSOnlineSystemC1Ev\n.globl __ZN15CFSOnlineSystemC2Ev\n__ZN15CFSOnlineSystemC1Ev:\n"
    "__ZN15CFSOnlineSystemC2Ev:\n    b _tpw_system_ctor_impl\n");
// (arm64 Apple constructors return this)
extern "C" __attribute__((used)) void* tpw_userinfo_ctor_impl(void* self) {
    ev("CFSUserInfo_ctor", {static_cast<int64_t>(newlab(self))});
    return self;
}
__asm__(
    ".text\n.p2align 2\n.globl __ZN11CFSUserInfoC1Ev\n.globl __ZN11CFSUserInfoC2Ev\n__ZN11CFSUserInfoC1Ev:\n__ZN11CFSUserInfoC2Ev:\n"
    "    b _tpw_userinfo_ctor_impl\n");

TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnUsersPathWithTheme() {
    ev("ReturnUsersPathWithTheme", {static_cast<int64_t>(lab(this))});
    return TbDynamicStringTemplate<char16_t>(u"");
}

uint32_t CText::GetLanguageId() const {
    ev("GetLanguageId", {static_cast<int64_t>(lab(this))});
    return static_cast<uint32_t>(ret("GetLanguageId"));
}
uint32_t LbTime_GetClock() {
    ev("LbTime_GetClock", {});
    return static_cast<uint32_t>(ret("LbTime_GetClock"));
}
uint8_t CFSOnlineSystemThemeList::ReturnOnlineThemeID(const char* name) {
    ev("ReturnOnlineThemeID", {static_cast<int64_t>(lab(this))}, {name});
    return static_cast<uint8_t>(ret("ReturnOnlineThemeID"));
}

// ---- the online library's connections
City::City() { ev("City_ctor", {static_cast<int64_t>(newlab(this))}); }
int32_t City::Connect(char* server, char*, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*)) {
    ev("City_Connect", {static_cast<int64_t>(lab(this)), 0, port, static_cast<int64_t>(lab(reinterpret_cast<void*>(callback)))}, {server});
    return static_cast<int32_t>(ret("City_Connect"));
}
void City::SetSessionInfo(uint16_t language, int32_t a, int32_t b, char16_t* password) {
    ev("City_SetSessionInfo", {static_cast<int64_t>(lab(this)), language, a, b}, {Narrow(password).c_str()});
}
void City::UnpublishPark(void* context, int32_t park, int32_t user) {
    ev("City_UnpublishPark", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(context)), park, user});
}
void City::Release() { ev("City_Release", {static_cast<int64_t>(lab(this))}); }
City::~City() { ev("City_dtor", {static_cast<int64_t>(lab(this)), 1}); }

void Upload::UploadParkRequest(char* file, ccParkFileInfo* info, void* context) {
    ev("UploadParkRequest", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(context))}, {file});
    uint32_t nameHash = Hash16(info->mName, 20, true);
    uint32_t descriptionHash = Hash16(info->mDescription, 256, true);
    uint32_t wordsHash = Hash16(info->mWords, 256, false);
    ev("ParkFileInfo", {info->mF00, info->mThemeID, info->mFlag, nameHash, descriptionHash, wordsHash});
}
void Upload::UploadParkFile(char* file, void* context) {
    ev("UploadParkFile", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(context))}, {file});
}

// ---- the callbacks the system registers (only their addresses matter) and the login panel
void CreateOnlineLoginPanel(void (*onFailure)()) {
    ev("CreateOnlineLoginPanel", {static_cast<int64_t>(lab(reinterpret_cast<void*>(onFailure)))});
}
void FSOnlineLoginRASFailureCallback() {}
void UploadPostLoginCallbackFn(int32_t, void*) {}
void FSOnlineCityServerConnectionCallback(int32_t, int32_t, void*, void*) {}

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_v_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineSystem", &gOnlineSystem},
        {"gUserInfo", &gUserInfo},
        {"gText", &gText},
        {"gTextRes", &gText.mResourcePointers[6]},
        {"wideSep", &gTPGlobals.mWideSeparator},
        {"f4498", &gTPGlobals.mF4498},
        {"tail", &gUploadNameTail.mString},
        {"cbUpload", reinterpret_cast<const void*>(&UploadPostLoginCallbackFn)},
        {"cbRas", reinterpret_cast<const void*>(&FSOnlineLoginRASFailureCallback)},
        {"cbCity", reinterpret_cast<const void*>(&FSOnlineCityServerConnectionCallback)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_v_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Zalloc(size))); }

// A zeroed online system whose string members are made (their vtables are the host's), unlogged.
uint64_t tpw_v_newsys() {
    bool muted = gMute;
    gMute = true;
    CFSOnlineSystem* system = static_cast<CFSOnlineSystem*>(Zalloc(sizeof(CFSOnlineSystem)));
    new (&system->mLoginPassword) TbDynamicStringTemplate<char16_t>();
    new (&system->mF10C) TbDynamicStringTemplate<char>();
    new (&system->mF330) TbDynamicStringTemplate<char>();
    new (&system->mF348) TbDynamicStringTemplate<char16_t>();
    new (&system->mF35C) TbDynamicStringTemplate<char16_t>();
    gMute = muted;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(system));
}

// The online configuration's city server and port.
void tpw_v_setcity(const char* server, uint32_t port) {
    new (&gOnlineConfig.mCityServer) std::string(server);
    gOnlineConfig.mCityPort = static_cast<int16_t>(port);
}

// The host offsets of the members the module touches, in the module's order; the last is sizeof(CFSOnlineSystem).
int32_t tpw_v_layout(int64_t* out) {
    CFSOnlineSystem* s = reinterpret_cast<CFSOnlineSystem*>(0x10000);
    auto off = [&](const void* member) { return static_cast<int64_t>(reinterpret_cast<const char*>(member) - reinterpret_cast<const char*>(s)); };
    int32_t n = 0;
    out[n++] = off(&s->mLoginCallback);
    out[n++] = off(&s->mLoginContext);
    out[n++] = off(&s->mLoginPassword.mData);
    out[n++] = off(&s->mF34);
    out[n++] = off(&s->mF38);
    out[n++] = off(&s->mF54[4]);
    out[n++] = off(&s->mLoggedIn);
    out[n++] = off(&s->mCity);
    out[n++] = off(&s->mCityClock);
    out[n++] = off(&s->mCityListsWanted);
    out[n++] = off(&s->mThemeList);
    out[n++] = off(&s->mUpload);
    out[n++] = off(&s->mF10C.mData);
    out[n++] = off(&s->mF120[0]);
    out[n++] = off(&s->mF320);
    out[n++] = off(&s->mUploadActive);
    out[n++] = off(&s->mUploadDone);
    out[n++] = off(&s->mUploadFailed);
    out[n++] = off(&s->mF330);
    out[n++] = off(&s->mF330.mData);
    out[n++] = off(&s->mF344);
    out[n++] = off(&s->mF348);
    out[n++] = off(&s->mF348.mData);
    out[n++] = off(&s->mF35C);
    out[n++] = off(&s->mF35C.mData);
    out[n++] = off(&s->mF370);
    out[n++] = off(&s->mF374);
    out[n++] = off(&s->mF378);
    out[n++] = off(&s->mF37C);
    out[n++] = off(&s->mF380);
    out[n++] = static_cast<int64_t>(sizeof(CFSOnlineSystem));
    return n;
}

// ---- the functions
void tpw_v_unpublish(uint64_t system, int32_t park) { reinterpret_cast<CFSOnlineSystem*>(system)->UnpublishPark(park); }
void tpw_v_request(uint64_t system, uint64_t file, int32_t f344, uint64_t name, uint64_t description, int32_t f370, uint64_t callback,
                   uint64_t context) {
    reinterpret_cast<CFSOnlineSystem*>(system)->RequestUpload(
        reinterpret_cast<const char*>(file), f344, reinterpret_cast<const char16_t*>(name),
        reinterpret_cast<const char16_t*>(description), f370, reinterpret_cast<void (*)(int32_t, const void*)>(callback),
        reinterpret_cast<const void*>(context));
}
void tpw_v_loggedin(uint64_t system) { reinterpret_cast<CFSOnlineSystem*>(system)->LoggedInRequestUpload(); }
void tpw_v_callback(uint64_t system, int32_t result, int32_t unused, uint64_t unused2, uint64_t context) {
    reinterpret_cast<CFSOnlineSystem*>(system)->UploadServerConnectionCallback(result, unused, reinterpret_cast<void*>(unused2),
                                                                                reinterpret_cast<void*>(context));
}
// The answer is built in place in `out` (a block the test made), as the Mac's hidden result pointer.
void tpw_v_filename(uint64_t system, uint64_t out) {
    new (reinterpret_cast<void*>(out)) TbDynamicStringTemplate<char16_t>(reinterpret_cast<CFSOnlineSystem*>(system)->ReturnUploadFilename());
}

}  // extern "C"
