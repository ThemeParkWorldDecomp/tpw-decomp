// Test library for the FTP download job of the online system (unit v5655; libtpw_unit_CFSOnlineSystem_CompleteFTPDownload.dylib,
// tests/test_CFSOnlineSystem_CompleteFTPDownload.py), main app:
//   src/FS_Online_init_CFSOnlineSystem_CompleteFTPDownload.cpp   CompleteFTPDownload, FTPDownloadCallbackFn,
//                                                                RequestNextFileForFTPDownload, Instance
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queue. The strings are
// fakes with a text buffer each (the events carry the texts, so a wrong string or a wrong order shows). The program's globals
// (gOnlineConfig, gText, gTPGlobals, gTPFileInfo) are raw zeroed storage under the program's symbol names: nothing constructs them.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "FS_Online.h"
#include "FS_System.h"
#include "LbTime.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPString.h"
#include "Text.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

using namespace calllog;

// ev with the texts as std::string (the log takes C strings).
static void ev(const char* name, std::initializer_list<int64_t> ints, std::initializer_list<std::string> texts) {
    std::string t[3];
    size_t n = 0;
    for (const std::string& x : texts)
        if (n < 3) t[n++] = x;
    if (n == 0) calllog::ev(name, ints);
    else if (n == 1) calllog::ev(name, ints, {t[0].c_str()});
    else if (n == 2) calllog::ev(name, ints, {t[0].c_str(), t[1].c_str()});
    else calllog::ev(name, ints, {t[0].c_str(), t[1].c_str(), t[2].c_str()});
}

// ---- globals of other files, defined here
CFSOnlineSystem* gOnlineSystem = nullptr;
alignas(16) unsigned char gOnlineConfigRaw[sizeof(OnlineConfig)] __asm__("_gOnlineConfig");
alignas(16) unsigned char gTextRaw[sizeof(CText)] __asm__("_gText");
alignas(16) unsigned char gTPGlobalsRaw[sizeof(TPGlobals)] __asm__("_gTPGlobals");
alignas(16) unsigned char gTPFileInfoRaw[sizeof(CSYS_TPFileInfo)] __asm__("_gTPFileInfo");

namespace {

CFSOnlineServer* gServer = nullptr;
CFSOnlineCity* gCityInfo = nullptr;
TbDynamicStringTemplate<char16_t>* gItem = nullptr;

std::string Txt(const char* s) { return s ? std::string(s) : std::string(); }
std::string Txt(const char16_t* s) { return s ? narrow(s) : std::string(); }

template <class Char> void Put(Char* data, const std::string& t) {
    size_t n = t.size() > 250 ? 250 : t.size();
    for (size_t i = 0; i < n; i++) data[i] = static_cast<Char>(static_cast<unsigned char>(t[i]));
    data[n] = 0;
}

template <class Char> const char* Kind(const char* c, const char* w) { return sizeof(Char) == 1 ? c : w; }

void FtpCallback(int32_t failed, const void* context) {
    ev("Callback", {failed, static_cast<int32_t>(reinterpret_cast<uintptr_t>(context))});
}

}  // namespace

// ---- the string fakes (explicitly instantiated below: the members the unit's source uses and the virtual ones)
template <class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate() {
    mLength = 0;
    mCapacity = 250;
    mGrowBy = 0;
    mData = static_cast<Char*>(std::calloc(256, sizeof(Char)));
    ev(Kind<Char>("CCtor", "WCtor"), {});
}
template <class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbDynamicStringTemplate& other) {
    mLength = 0;
    mCapacity = 250;
    mGrowBy = 0;
    mData = static_cast<Char*>(std::calloc(256, sizeof(Char)));
    std::string t = Txt(other.mData);
    Put(mData, t);
    mLength = static_cast<uint32_t>(t.size());
    ev(Kind<Char>("CCopy", "WCopy"), {}, {t});
}
template <class Char> TbDynamicStringTemplate<Char>::~TbDynamicStringTemplate() {
    ev(Kind<Char>("CDtor", "WDtor"), {}, {Txt(mData)});
}
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char* text) {
    std::string t = Txt(text);
    ev(Kind<Char>("CAssign", "WAssign"), {}, {t});
    Put(mData, t);
    mLength = static_cast<uint32_t>(t.size());
    return *this;
}
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char& ch) {
    ev(Kind<Char>("CAppendChar", "WAppendChar"), {static_cast<int64_t>(ch)});
    std::string t = Txt(mData);
    t.push_back(static_cast<char>(ch));
    Put(mData, t);
    mLength = static_cast<uint32_t>(t.size());
    return *this;
}
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char* text) {
    std::string add = Txt(text);
    ev(Kind<Char>("CAppendText", "WAppendText"), {}, {add});
    std::string t = Txt(mData) + add;
    Put(mData, t);
    mLength = static_cast<uint32_t>(t.size());
    return *this;
}
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const TbStringBase<Char>& other) {
    std::string add = Txt(static_cast<const TbDynamicStringTemplate<Char>&>(other).mData);
    ev(Kind<Char>("CAppendStr", "WAppendStr"), {}, {add});
    std::string t = Txt(mData) + add;
    Put(mData, t);
    mLength = static_cast<uint32_t>(t.size());
    return *this;
}
// Never reached by the unit's source: placeholders so that the class's vtable links.
template <class Char> void TbDynamicStringTemplate<Char>::Format(const Char*, ...) {}
template <class Char> void TbDynamicStringTemplate<Char>::ResizeTo(uint32_t) {}
template <class Char> void TbDynamicStringTemplate<Char>::AllocBuffer(uint32_t) {}
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbStringBase<Char>&) { return *this; }
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbDynamicStringTemplate&) { return *this; }
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char&) { return *this; }
template class TbDynamicStringTemplate<char>;
template class TbDynamicStringTemplate<char16_t>;

// ---- the other fakes
namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {}, {Txt(format)}); }
}  // namespace SamsUtilities

void AddUIError(int32_t error, TbITable* table) { ev("AddUIError", {error, lab(table)}); }
uint32_t LbTime_GetClock() {
    ev("GetClock", {});
    return static_cast<uint32_t>(ret("GetClock"));
}
uint32_t CText::GetLanguageId() const {
    ev("GetLanguageId", {});
    return static_cast<uint32_t>(ret("GetLanguageId"));
}
void FSOnlineCityServerConnectionCallback(int32_t, int32_t, void*, void*) {}

City::City() { ev("CityCtor", {newlab(this)}); }
City::~City() {
    ev("CityDtor", {lab(this), 1});
    forget(this);
}
void City::Release() { ev("CityRelease", {lab(this)}); }
int32_t City::Connect(char* server, char* unused, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*)) {
    ev("CityConnect", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(unused)), port}, {Txt(server)});
    ev("ConnectCb", {callback == FSOnlineCityServerConnectionCallback ? 1 : 0});
    return static_cast<int32_t>(ret("CityConnect"));
}
void City::SetSessionInfo(uint16_t language, int32_t a, int32_t b, char16_t* password) {
    ev("CitySetSession", {lab(this), language, a, b}, {Txt(password)});
}
void City::MarkParkVisited(void* context, int32_t park, int32_t arg) {
    ev("CityMark", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(context)), park, arg});
}

void CFSOnlineSystem::Logout() { ev("Logout", {lab(this)}); }
void CFSOnlineSystem::ReleaseFTPDownloadServerConnection() { ev("ReleaseFTP", {lab(this)}); }
CFSOnlineSystem::CFSOnlineSystem() { ev("SysCtor", {newlab(this)}); }

int32_t FTPClient::Download(char* remote, char* local) {
    ev("FTPDownload", {lab(this)}, {Txt(remote), Txt(local)});
    return 0;
}
int32_t FTPClient::Disconnect() {
    ev("FTPDisconnect", {lab(this)});
    return 0;
}

int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    std::string t = Txt(in.mData);
    ev("UniToStr", {}, {t});
    Put(out.mData, t);
    out.mLength = static_cast<uint32_t>(t.size());
    return 0;
}
int32_t StringToUniString(TbDynamicStringTemplate<char16_t>& out, const TbDynamicStringTemplate<char>& in) {
    std::string t = Txt(in.mData);
    ev("StrToUni", {}, {t});
    Put(out.mData, t);
    out.mLength = static_cast<uint32_t>(t.size());
    return 0;
}
void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path) {
    std::string t = Txt(path.mData);
    ev("AddSeparator", {}, {t});
    Put(path.mData, t + "/");
    path.mLength = static_cast<uint32_t>(t.size() + 1);
}
const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetLocalPath() const {
    ev("GetLocalPath", {});
    return mLocalPath;
}
CFSOnlineCity* park_info_callee::ReturnCityInfo(CFSOnlineSystemCityList* list, int32_t city) {
    ev("ReturnCityInfo", {lab(list), city});
    return gCityInfo;
}
CFSOnlineServer* ftpdl_callee::ReturnFTPDownloadServer(CFSOnlineSystemServerList* list) {
    ev("ReturnFTPDownloadServer", {lab(list)});
    return gServer;
}
TbDynamicStringTemplate<char16_t>* ftpdl_callee::RemoveHead(TbPtrList<TbDynamicStringTemplate<char16_t>>* list) {
    ev("RemoveHead", {lab(list)});
    return gItem;
}

template <class T> static uint64_t A(T* p) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)); }

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_ftpdl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineSystem", &gOnlineSystem},
        {"slashA", &gFTPDownloadSlashA},
        {"slashB", &gFTPDownloadSlashB},
        {"slashC", &gFTPDownloadSlashC},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ftpdl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The address of a field the module sets or reads by the Mac's offset: object kind 0 the system, 1 a park, 2 a server, 3 a city
// record, 4 the globals.
uint64_t tpw_ftpdl_faddr(int32_t obj, uint64_t base, int32_t id) {
    switch (obj) {
    case 0: {
        CFSOnlineSystem* s = reinterpret_cast<CFSOnlineSystem*>(base);
        switch (id) {
        case 0: return A(&s->mF34);
        case 1: return A(&s->mF38);
        case 2: return A(&s->mLoginPassword.mData);
        case 3: return A(&s->mCity);
        case 4: return A(&s->mCityClock);
        case 5: return A(&s->mCityListsWanted);
        case 6: return A(&s->mCityList);
        case 7: return A(&s->mServerList);
        case 8: return A(&s->mF384);
        case 9: return A(&s->mF388);
        case 10: return A(&s->mFTPDone);
        case 11: return A(&s->mFTPResult);
        case 12: return A(&s->mF398);
        case 13: return A(&s->mFTP);
        case 14: return A(&s->mFTPPark);
        case 15: return A(&s->mF3C0);
        case 16: return A(&s->mF3C4);
        case 17: return A(&s->mF3C8.mCount);
        case 18: return A(&s->mF3D8[0]);
        case 19: return A(&s->mF3DC.mData);
        case 20: return A(&s->mFTPParkPending);
        case 21: return A(&s->mF3C8);
        }
        break;
    }
    case 1: {
        CFSOnlinePark* p = reinterpret_cast<CFSOnlinePark*>(base);
        switch (id) {
        case 0: return A(&p->mParkID);
        case 1: return A(&p->mF48);
        case 2: return A(&p->mText7C.mData);
        }
        break;
    }
    case 2: return A(&reinterpret_cast<CFSOnlineServer*>(base)->mText6C.mData);
    case 3: return A(&reinterpret_cast<CFSOnlineCity*>(base)->mFtpName);
    case 4: {
        switch (id) {
        case 0: return A(&reinterpret_cast<CSYS_TPFileInfo*>(gTPFileInfoRaw)->mLocalPath.mData);
        case 1: return A(&reinterpret_cast<TPGlobals*>(gTPGlobalsRaw)->mDataFolder.mData);
        case 2: return A(&reinterpret_cast<OnlineConfig*>(gOnlineConfigRaw)->mCityPort);
        }
        break;
    }
    }
    return 0;
}

void tpw_ftpdl_set_cityserver(const char* text) { reinterpret_cast<OnlineConfig*>(gOnlineConfigRaw)->mCityServer = text; }

// A heap string for the list head (made before the log starts).
uint64_t tpw_ftpdl_item_new(const char* text) {
    auto* item = new TbDynamicStringTemplate<char16_t>();
    Put(item->mData, Txt(text));
    return A(item);
}

void tpw_ftpdl_world(uint64_t server, uint64_t cityInfo, uint64_t item) {
    gServer = reinterpret_cast<CFSOnlineServer*>(server);
    gCityInfo = reinterpret_cast<CFSOnlineCity*>(cityInfo);
    gItem = reinterpret_cast<TbDynamicStringTemplate<char16_t>*>(item);
}

uint32_t tpw_ftpdl_cbword() { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&FtpCallback)); }

void tpw_ftpdl_complete(uint64_t sys, int32_t failed) {
    reinterpret_cast<CFSOnlineSystem*>(sys)->CompleteFTPDownload(failed);
}
void tpw_ftpdl_callback(uint64_t sys, int32_t state, int32_t error) {
    reinterpret_cast<CFSOnlineSystem*>(sys)->FTPDownloadCallbackFn(state, error);
}
void tpw_ftpdl_next(uint64_t sys) { reinterpret_cast<CFSOnlineSystem*>(sys)->RequestNextFileForFTPDownload(); }
uint64_t tpw_ftpdl_instance() { return A(CFSOnlineSystem::Instance()); }

}  // extern "C"
