// Test library for CFSOnlineSystem::ConnectUploadServer and ::LoggedInRequestFTPDownload (unit v5935;
// libtpw_unit_CFSOnlineSystem_ConnectUploadServer.dylib, tests/test_CFSOnlineSystem_ConnectUploadServer.py), main app:
//   src/FS_Online_init_CFSOnlineSystem_ConnectUploadServer.cpp
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queues. The strings are
// working fakes (a malloc'd buffer behind mData); an event names a text by a hash of its code units, the same function as the
// test module's. The globals of other files the unit reads (gTPGlobals, gTPFileInfo, gText, gOnlineConfig, gUserInfo) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <string>
#include <type_traits>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "LbFile.h"
#include "LbTime.h"
#include "TPGlobals.h"
#include "TPString.h"
#include "Text.h"
#include "platform/samsutils.h"

using namespace calllog;

// The arena's blocks are not 32-bit addresses here, so the world's objects are labelled by this table; objects the fakes make (newlab) by calllog.
static std::map<const void*, int64_t> gKnown;
static int64_t Lab(const void* p) {
    auto it = gKnown.find(p);
    if (it != gKnown.end()) return it->second;
    return static_cast<int64_t>(calllog::lab(p));
}
#define lab(p) Lab(p)

// ---- globals of other files, raw storage under the program's names (their constructors are not part of this unit)
alignas(16) unsigned char gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");
alignas(16) unsigned char gTPFileInfoStorage[sizeof(CSYS_TPFileInfo)] __asm__("_gTPFileInfo");
alignas(16) unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");
alignas(16) unsigned char gOnlineConfigStorage[sizeof(OnlineConfig)] __asm__("_gOnlineConfig");
CFSUserInfo* gUserInfo = nullptr;

namespace {

int gQuiet = 1;   // above 0: the fakes record nothing (world building)
struct QuietScope {
    QuietScope() { ++gQuiet; }
    ~QuietScope() { --gQuiet; }
};
inline bool Loud() { return gQuiet == 0; }

#define EV(name, ...)                          \
    do {                                       \
        if (Loud()) ev(name, {__VA_ARGS__});   \
    } while (0)

inline int64_t I(int64_t x) { return x; }

template<class C> size_t Len(const C* p) {
    size_t n = 0;
    if (p != nullptr)
        while (p[n] != 0) ++n;
    return n;
}

template<class C> int64_t HashUnits(const C* p, size_t n) {
    typedef typename std::conditional<sizeof(C) == 1, uint8_t, uint16_t>::type U;
    uint64_t h = n;
    for (size_t i = 0; i < n; ++i) h = (h * 131 + static_cast<U>(p[i]) + 1) & 0x7fffffff;
    return static_cast<int64_t>(h);
}

template<class C> int64_t HashText(const C* p) { return HashUnits(p, Len(p)); }
template<class C> int64_t HashStr(const TbStringBase<C>& s) { return HashUnits(s.mData, s.mLength > 0 ? static_cast<size_t>(s.mLength) : 0); }

template<class C> void Assign(TbStringBase<C>& s, const C* p, size_t n) {
    C* d = static_cast<C*>(std::malloc((n + 1) * sizeof(C)));
    if (n != 0) std::memcpy(d, p, n * sizeof(C));
    d[n] = 0;
    std::free(s.mData);
    s.mData = d;
    s.mLength = static_cast<int32_t>(n);
}

template<class C> void Init(TbDynamicStringTemplate<C>& s, const C* p, size_t n) {
    s.mData = nullptr;
    s.mCapacity = 0;
    s.mGrowBy = 0;
    Assign<C>(s, p, n);
}

template<class C> void Append(TbStringBase<C>& s, const C* p, size_t n) {
    size_t old = s.mLength > 0 ? static_cast<size_t>(s.mLength) : 0;
    C* d = static_cast<C*>(std::malloc((old + n + 1) * sizeof(C)));
    if (old != 0) std::memcpy(d, s.mData, old * sizeof(C));
    if (n != 0) std::memcpy(d + old, p, n * sizeof(C));
    d[old + n] = 0;
    std::free(s.mData);
    s.mData = d;
    s.mLength = static_cast<int32_t>(old + n);
}

// The fake objects of the world.
CFSOnlineServer* gUploadServer = nullptr;
CFSOnlineServer* gFTPServer = nullptr;
CFSOnlineRide* gRide = nullptr;

}  // namespace

// ---- the string fakes (the program's string library is not linked)
template<class C> TbDynamicStringTemplate<C>::TbDynamicStringTemplate() {
    Init<C>(*this, nullptr, 0);
    EV("StrCtor", I(sizeof(C)), I(0), HashStr<C>(*this));
}
template<class C> TbDynamicStringTemplate<C>::TbDynamicStringTemplate(const C* text) {
    Init<C>(*this, text, Len(text));
    EV("StrCtor", I(sizeof(C)), I(1), HashStr<C>(*this));
}
template<class C> TbDynamicStringTemplate<C>::TbDynamicStringTemplate(const TbDynamicStringTemplate& other) {
    Init<C>(*this, other.mData, other.mLength > 0 ? static_cast<size_t>(other.mLength) : 0);
    EV("StrCtor", I(sizeof(C)), I(2), HashStr<C>(*this));
}
template<class C> TbDynamicStringTemplate<C>::TbDynamicStringTemplate(const TbStringBase<C>& other) {
    Init<C>(*this, other.mData, other.mLength > 0 ? static_cast<size_t>(other.mLength) : 0);
    EV("StrCtor", I(sizeof(C)), I(3), HashStr<C>(*this));
}
template<class C> void TbDynamicStringTemplate<C>::Format(const C*, ...) {}
template<class C> void TbDynamicStringTemplate<C>::ResizeTo(uint32_t) {}
template<class C> void TbDynamicStringTemplate<C>::AllocBuffer(uint32_t) {}
template<class C> TbDynamicStringTemplate<C>::~TbDynamicStringTemplate() {
    EV("StrDtor", I(sizeof(C)), HashStr<C>(*this));
    std::free(this->mData);
    this->mData = nullptr;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>& other) {
    Assign<C>(*this, other.mData, other.mLength > 0 ? static_cast<size_t>(other.mLength) : 0);
    return *this;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate& other) {
    Assign<C>(*this, other.mData, other.mLength > 0 ? static_cast<size_t>(other.mLength) : 0);
    return *this;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C* text) {
    int64_t arg = HashText(text);
    size_t n = Len(text);
    C* copy = static_cast<C*>(std::malloc((n + 1) * sizeof(C)));
    if (n != 0) std::memcpy(copy, text, n * sizeof(C));
    copy[n] = 0;
    Assign<C>(*this, copy, n);
    std::free(copy);
    EV("StrAssign", I(sizeof(C)), I(1), HashStr<C>(*this), arg);
    return *this;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C& ch) {
    Assign<C>(*this, &ch, 1);
    return *this;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>& text) {
    int64_t arg = HashStr<C>(text);
    Append<C>(*this, text.mData, text.mLength > 0 ? static_cast<size_t>(text.mLength) : 0);
    EV("StrAppend", I(sizeof(C)), I(0), HashStr<C>(*this), arg);
    return *this;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C& ch) {
    Append<C>(*this, &ch, 1);
    return *this;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C* text) {
    Append<C>(*this, text, Len(text));
    EV("StrAppend", I(sizeof(C)), I(1), HashStr<C>(*this), HashText(text));
    return *this;
}
template class TbDynamicStringTemplate<char>;
template class TbDynamicStringTemplate<char16_t>;

template<class C> TbStringBase<C> TbStringBase<C>::Left(uint32_t count) const {
    size_t len = mLength > 0 ? static_cast<size_t>(mLength) : 0;
    size_t n = count < len ? count : len;
    C* d = static_cast<C*>(std::malloc((n + 1) * sizeof(C)));
    if (n != 0) std::memcpy(d, mData, n * sizeof(C));
    d[n] = 0;
    EV("Left", I(count), HashStr<C>(*this), HashUnits(d, n));
    return TbStringBase<C>(d, static_cast<uint32_t>(n));
}
template<class C> TbStringBase<C> TbStringBase<C>::Mid(uint32_t start) const {
    size_t len = mLength > 0 ? static_cast<size_t>(mLength) : 0;
    size_t from = start < len ? start : len;
    size_t n = len - from;
    C* d = static_cast<C*>(std::malloc((n + 1) * sizeof(C)));
    if (n != 0) std::memcpy(d, mData + from, n * sizeof(C));
    d[n] = 0;
    EV("Mid", I(start), HashStr<C>(*this), HashUnits(d, n));
    return TbStringBase<C>(d, static_cast<uint32_t>(n));
}
template TbStringBase<char> TbStringBase<char>::Left(uint32_t) const;
template TbStringBase<char> TbStringBase<char>::Mid(uint32_t) const;

// ---- the callees
int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    size_t n = in.mLength > 0 ? static_cast<size_t>(in.mLength) : 0;
    char* d = static_cast<char*>(std::malloc(n + 1));
    for (size_t i = 0; i < n; ++i) d[i] = static_cast<char>(in.mData[i] & 0xff);
    d[n] = 0;
    Assign<char>(out, d, n);
    std::free(d);
    EV("UniStringToString", HashStr<char16_t>(in), HashStr<char>(out));
    return 0;
}

int32_t StringToUniString(TbDynamicStringTemplate<char16_t>& out, const TbDynamicStringTemplate<char>& in) {
    size_t n = in.mLength > 0 ? static_cast<size_t>(in.mLength) : 0;
    char16_t* d = static_cast<char16_t*>(std::malloc((n + 1) * 2));
    for (size_t i = 0; i < n; ++i) d[i] = static_cast<uint8_t>(in.mData[i]);
    d[n] = 0;
    Assign<char16_t>(out, d, n);
    std::free(d);
    EV("StringToUniString", HashStr<char>(in), HashStr<char16_t>(out));
    return 0;
}

void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    EV("FS_AddDirectorySeparator", HashStr<char>(path));
    if (path.mLength == 0 || path.mData[path.mLength - 1] != '/') Append<char>(path, "/", 1);
}

uint8_t LbFile_Exists(const char* name) {
    EV("LbFile_Exists", HashText(name));
    return static_cast<uint8_t>(ret("exists"));
}

TbError LbFile_CreateDirectory(const char* name) {
    EV("LbFile_CreateDirectory", HashText(name));
    return TbError(0);
}

void DebugStr(const unsigned char* message) { EV("DebugStr", I(message[0]), HashUnits(message + 1, message[0])); }

uint32_t LbTime_GetClock() {
    EV("LbTime_GetClock");
    return static_cast<uint32_t>(ret("clock"));
}

uint32_t CText::GetLanguageId() const {
    EV("GetLanguageId");
    return static_cast<uint32_t>(ret("lang"));
}

const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetLocalPath() const {
    EV("GetLocalPath", lab(this));
    return mLocalPath;
}

namespace SamsUtilities {
void UDebug(const char* format, ...) { EV("UDebug", HashText(format)); }
}  // namespace SamsUtilities

void FSOnlineCityServerConnectionCallback(int32_t, int32_t, void*, void*) {}
namespace ftpreq_callee {
void UploadConnectionCallback(int32_t, int32_t, void*, void*) {}

CFSOnlineServer* ReturnUploadServer(CFSOnlineSystemServerList* list) {
    EV("ReturnUploadServer", lab(list));
    return gUploadServer;
}

CFSOnlineRide* ReturnRideInfo(CFSOnlineSystemRideList* list, int32_t theme, int32_t index) {
    EV("ReturnRideInfo", lab(list), I(theme), I(index));
    return gRide;
}

// The text is "R" and the ride id in decimal.
TbDynamicStringTemplate<char16_t> GetFilePath(const CFSOnlineRide* ride) {
    EV("GetFilePath", lab(ride));
    char16_t text[16];
    int n = 0;
    text[n++] = u'R';
    char digits[12];
    int count = 0;
    uint32_t value = static_cast<uint32_t>(ride->mRideID);
    do {
        digits[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (count > 0) text[n++] = static_cast<char16_t>(digits[--count]);
    text[n] = 0;
    QuietScope quiet;
    return TbDynamicStringTemplate<char16_t>(static_cast<const char16_t*>(text));
}

void UserInfoCtor(CFSUserInfo* self) {
    newlab(self);
    EV("UserInfoCtor", lab(self));
}
}  // namespace ftpreq_callee

namespace ftpdl_callee {
CFSOnlineServer* ReturnFTPDownloadServer(CFSOnlineSystemServerList* list) {
    EV("ReturnFTPDownloadServer", lab(list));
    return gFTPServer;
}
TbDynamicStringTemplate<char16_t>* RemoveHead(TbPtrList<TbDynamicStringTemplate<char16_t>>* list) {
    EV("RemoveHead", I(list->mCount));
    return list->RemoveHead();
}
}  // namespace ftpdl_callee

// The text is "S:online".
TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnOnlinePath() {
    EV("ReturnOnlinePath", lab(this));
    QuietScope quiet;
    return TbDynamicStringTemplate<char16_t>(static_cast<const char16_t*>(u"S:online"));
}

CFSOnlinePark& CFSOnlinePark::operator=(const CFSOnlinePark& other) {
    EV("ParkAssign", lab(this), lab(&other), I(other.mParkID));
    return *this;
}

void CFSOnlineSystem::ReleaseUploadServerConnection() { EV("ReleaseUploadServerConnection", lab(this)); }
void CFSOnlineSystem::ReleaseFTPDownloadServerConnection() { EV("ReleaseFTPDownloadServerConnection", lab(this)); }
void CFSOnlineSystem::CompleteFTPDownload(int32_t failed) { EV("CompleteFTPDownload", lab(this), I(failed)); }

// ---- the online library's classes
Upload::Upload() {
    newlab(this);
    EV("Upload", lab(this));
}
int32_t Upload::Connect(char* server, char* unused, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*)) {
    EV("UploadConnect", lab(this), HashText(server), I(unused != nullptr), I(port), I(callback == ftpreq_callee::UploadConnectionCallback));
    return static_cast<int32_t>(ret("connect"));
}
void Upload::SetSessionInfo(int16_t language, int32_t a, int32_t b, char16_t* password) {
    EV("UploadSetSessionInfo", lab(this), I(language), I(a), I(b), HashText(password));
}

City::City() {
    newlab(this);
    EV("City", lab(this));
}
int32_t City::Connect(char* server, char* unused, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*)) {
    EV("CityConnect", lab(this), HashText(server), I(unused != nullptr), I(port), I(callback == FSOnlineCityServerConnectionCallback));
    return static_cast<int32_t>(ret("cityconnect"));
}
void City::SetSessionInfo(uint16_t language, int32_t a, int32_t b, char16_t* password) {
    EV("CitySetSessionInfo", lab(this), I(language), I(a), I(b), HashText(password));
}
void City::Release() { EV("CityRelease", lab(this)); }
City::~City() { EV("CityDtor", lab(this)); }
void City::MarkParkVisited(void* context, int32_t park, int32_t arg) {
    EV("MarkParkVisited", lab(this), I(context != nullptr), I(park), I(arg));
}

FTPClient::~FTPClient() {}
FTPClient* FTPClient::Construct(CFTPControl* control, bool threaded) {
    newlab(this);
    uint32_t word = 0;
    if (control != nullptr) std::memcpy(&word, control, 4);
    EV("FTPClient", lab(this), I(control != nullptr), I(word), I(threaded));
    return this;
}
int32_t FTPClient::Connect(char* a, char* b, char* c, char* server) {
    EV("FTPClientConnect", lab(this), HashText(a), HashText(b), HashText(c), I(server != nullptr));
    return 0;
}

namespace {
void FakeCallback(int32_t result, const void* context) {
    EV("Callback", I(result), I(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context))));
}

// ---- the offsets of the host layout, by the module's names
struct FieldEntry {
    const char* name;
    int64_t offset;
};

template<class T, class F> int64_t OffsetOf(T* base, F* field) {
    return reinterpret_cast<char*>(field) - reinterpret_cast<char*>(base);
}

int64_t FieldOffset(const char* name) {
    static unsigned char sysBuffer[sizeof(CFSOnlineSystem)];
    static unsigned char parkBuffer[sizeof(CFSOnlinePark)];
    static unsigned char serverBuffer[sizeof(CFSOnlineServer)];
    static unsigned char rideBuffer[sizeof(CFSOnlineRide)];
    CFSOnlineSystem* sys = reinterpret_cast<CFSOnlineSystem*>(sysBuffer);
    CFSOnlinePark* park = reinterpret_cast<CFSOnlinePark*>(parkBuffer);
    CFSOnlineServer* server = reinterpret_cast<CFSOnlineServer*>(serverBuffer);
    CFSOnlineRide* ride = reinterpret_cast<CFSOnlineRide*>(rideBuffer);
    const FieldEntry entries[] = {
        {"sys.mF34", OffsetOf(sys, &sys->mF34)},
        {"sys.mF38", OffsetOf(sys, &sys->mF38)},
        {"sys.mF58", OffsetOf(sys, &sys->mF54[4])},
        {"sys.mCity", OffsetOf(sys, &sys->mCity)},
        {"sys.mCityClock", OffsetOf(sys, &sys->mCityClock)},
        {"sys.mCityListsWanted", OffsetOf(sys, &sys->mCityListsWanted)},
        {"sys.mServerList", OffsetOf(sys, &sys->mServerList)},
        {"sys.mRideList", OffsetOf(sys, &sys->mRideList)},
        {"sys.mUpload", OffsetOf(sys, &sys->mUpload)},
        {"sys.mFTPActive", OffsetOf(sys, &sys->mFTPActive)},
        {"sys.mF398", OffsetOf(sys, &sys->mF398)},
        {"sys.mF39C", OffsetOf(sys, &sys->mF39C)},
        {"sys.mFTP", OffsetOf(sys, &sys->mFTP)},
        {"sys.mFTPPark", OffsetOf(sys, &sys->mFTPPark)},
        {"sys.mF3C0", OffsetOf(sys, &sys->mF3C0)},
        {"sys.mF3C4", OffsetOf(sys, &sys->mF3C4)},
        {"sys.mF3D8", OffsetOf(sys, &sys->mF3D8[0])},
        {"sys.mF3DC", OffsetOf(sys, &sys->mF3DC)},
        {"sys.mFTPParkPending", OffsetOf(sys, &sys->mFTPParkPending)},
        {"sys.mLoginPassword", OffsetOf(sys, &sys->mLoginPassword)},
        {"sys.mLoginEmail", OffsetOf(sys, &sys->mLoginEmail)},
        {"park.mParkID", OffsetOf(park, &park->mParkID)},
        {"park.mF4C", OffsetOf(park, &park->mF4C)},
        {"park.mF58", OffsetOf(park, &park->mF58[0])},
        {"park.mF78", OffsetOf(park, &park->mF78)},
        {"park.mText7C", OffsetOf(park, &park->mText7C)},
        {"server.mText24", OffsetOf(server, &server->mText24)},
        {"server.mF3C", OffsetOf(server, &server->mF3C)},
        {"ride.mRideID", OffsetOf(ride, &ride->mRideID)},
    };
    for (const FieldEntry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return entry.offset;
    return -1;
}

struct GlobalsInit {
    GlobalsInit() {
        new (&gTPFileInfo.mLocalPath) TbDynamicStringTemplate<char>();
        new (&gTPGlobals.mDataFolder) TbDynamicStringTemplate<char>();
        new (&gTPGlobals.mF4484) TbDynamicStringTemplate<char>();
        new (&gTPGlobals.mWideSeparator) TbDynamicStringTemplate<char16_t>();
        new (&gOnlineConfig_().mCityServer) std::string();
        --gQuiet;
    }
    static OnlineConfig& gOnlineConfig_() { return *reinterpret_cast<OnlineConfig*>(gOnlineConfigStorage); }
};
GlobalsInit gInit;

template<class C> void SetString(void* object, const uint16_t* units, int32_t n) {
    QuietScope quiet;
    C* d = static_cast<C*>(std::malloc((static_cast<size_t>(n) + 1) * sizeof(C)));
    for (int32_t i = 0; i < n; ++i) d[i] = static_cast<C>(units[i]);
    d[n] = 0;
    Assign<C>(*static_cast<TbStringBase<C>*>(static_cast<TbDynamicStringTemplate<C>*>(object)), d, static_cast<size_t>(n));
    std::free(d);
}

}  // namespace

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_cu_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gTPFileInfo", &gTPFileInfo},
        {"gLocalPath", &gTPFileInfo.mLocalPath},
        {"gDataFolder", &gTPGlobals.mDataFolder},
        {"gWideSeparator", &gTPGlobals.mWideSeparator},
        {"gF4484", &gTPGlobals.mF4484},
        {"gUserInfo", &gUserInfo},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cu_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int64_t tpw_cu_off(const char* name) { return FieldOffset(name); }

// A world object: 0 the online system, 1 a park, 2 a server, 3 a ride, 4 anything else (a list, a city, a user info).
uint64_t tpw_cu_make(int32_t kind) {
    QuietScope quiet;
    void* object = nullptr;
    switch (kind) {
        case 0: {
            CFSOnlineSystem* sys = static_cast<CFSOnlineSystem*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineSystem)));
            new (&sys->mLoginPassword) TbDynamicStringTemplate<char16_t>();
            new (&sys->mLoginEmail) TbDynamicStringTemplate<char16_t>();
            new (&sys->mF3DC) TbDynamicStringTemplate<char>();
            new (&sys->mF3C8) TbPtrList<TbDynamicStringTemplate<char16_t>>();
            object = sys;
            break;
        }
        case 1: {
            CFSOnlinePark* park = static_cast<CFSOnlinePark*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlinePark)));
            new (&park->mText7C) TbDynamicStringTemplate<char>();
            object = park;
            break;
        }
        case 2: {
            CFSOnlineServer* server = static_cast<CFSOnlineServer*>(tpw::ArenaAllocZeroed(sizeof(CFSOnlineServer)));
            new (&server->mText24) TbDynamicStringTemplate<char16_t>();
            object = server;
            break;
        }
        case 3: object = tpw::ArenaAllocZeroed(sizeof(CFSOnlineRide)); break;
        default: object = tpw::ArenaAllocZeroed(64); break;
    }
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object));
}

// The objects the fakes answer: 0 the upload server, 1 the FTP download server, 2 the ride.
void tpw_cu_world(int32_t which, uint64_t object) {
    if (which == 0) gUploadServer = reinterpret_cast<CFSOnlineServer*>(object);
    if (which == 1) gFTPServer = reinterpret_cast<CFSOnlineServer*>(object);
    if (which == 2) gRide = reinterpret_cast<CFSOnlineRide*>(object);
}

// Sets a string object (made by tpw_cu_make or by the unit) to the code units, without a record.
void tpw_cu_setstr(uint64_t object, int32_t wide, const uint16_t* units, int32_t n) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(object));
    if (wide != 0) SetString<char16_t>(p, units, n);
    else SetString<char>(p, units, n);
}

int64_t tpw_cu_strhash(uint64_t object, int32_t wide) {
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(object));
    if (wide != 0) return HashStr<char16_t>(*static_cast<TbDynamicStringTemplate<char16_t>*>(p));
    return HashStr<char>(*static_cast<TbDynamicStringTemplate<char>*>(p));
}

void tpw_cu_setcity(const char* text, int32_t port) {
    OnlineConfig& config = *reinterpret_cast<OnlineConfig*>(gOnlineConfigStorage);
    config.mCityServer = text;
    config.mCityPort = static_cast<int16_t>(port);
}

void tpw_cu_list_add(uint64_t sys, const uint16_t* units, int32_t n) {
    QuietScope quiet;
    CFSOnlineSystem* system = reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(sys));
    TbDynamicStringTemplate<char16_t>* s = new TbDynamicStringTemplate<char16_t>();
    SetString<char16_t>(s, units, n);
    system->mF3C8.AddTail(s);
}

// The list's strings as hashes; answers the count.
int32_t tpw_cu_list_dump(uint64_t sys, int64_t* out, int32_t max) {
    CFSOnlineSystem* system = reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(sys));
    int32_t n = 0;
    POSITION position = system->mF3C8.GetHeadPosition();
    while (position != nullptr && n < max) {
        TbDynamicStringTemplate<char16_t>* s = system->mF3C8.GetNext(position);
        out[n++] = HashStr<char16_t>(*s);
    }
    return n;
}

int32_t tpw_cu_list_count(uint64_t sys) { return reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(sys))->mF3C8.mCount; }

// A pointer field of the system: its label, its address.
int64_t tpw_cu_fieldlab(uint64_t sys, const char* name) {
    char* base = reinterpret_cast<char*>(static_cast<uintptr_t>(sys));
    void* p = *reinterpret_cast<void**>(base + FieldOffset(name));
    return lab(p);
}
uint64_t tpw_cu_fieldptr(uint64_t sys, const char* name) {
    char* base = reinterpret_cast<char*>(static_cast<uintptr_t>(sys));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(*reinterpret_cast<void**>(base + FieldOffset(name))));
}
void tpw_cu_known(uint64_t p, int64_t label) {
    if (p == 0) gKnown.clear();
    else gKnown[reinterpret_cast<const void*>(static_cast<uintptr_t>(p))] = label;
}
int64_t tpw_cu_lab(uint64_t p) { return lab(reinterpret_cast<void*>(static_cast<uintptr_t>(p))); }

uint32_t tpw_cu_callback_word() { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&FakeCallback)); }

int32_t tpw_cu_connect(uint64_t sys) { return reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(sys))->ConnectUploadServer(); }

void tpw_cu_request(uint64_t sys, uint64_t park) {
    reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(sys))
        ->LoggedInRequestFTPDownload(reinterpret_cast<const CFSOnlinePark*>(static_cast<uintptr_t>(park)));
}

}  // extern "C"
