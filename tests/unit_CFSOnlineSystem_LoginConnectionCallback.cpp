// Test library for the online system's login and news jobs (unit v5924; libtpw_unit_CFSOnlineSystem_LoginConnectionCallback.dylib,
// tests/test_CFSOnlineSystem_LoginConnectionCallback.py), main app:
//   src/game/online/FS_Online.cpp   LoginConnectionCallback, DoLogin, LoggedInGetNews,
//   NewsConnectionCallback, CompleteNews, gOnlineNewsGuid, a weak online_callee::Free and a weak FSOnlineNewsConnectionCallback.
//
// The callees outside the unit are fakes that record the arguments the real ones read (pointers as labels, texts as a hash of their
// units) and answer from the test's queues: the online library's Auth and News, the interface's progress bar, error dialog and news
// panel, the string and file classes, the balance table's Load, CText::GetLanguageId, CSYS_TPFileInfo::GetDataPath, the runtime's
// delete and free. The globals of other files the unit reads are raw storage under the program's symbol names (their constructors are
// not this unit's concern); the strings the world needs are made by tpw_cfsol_mkstr.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "Text.h"
#include "OnlineBalance.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "bfl_FileSystem.h"
#include "TbError.h"

using namespace calllog;
using bfl::FileSystem::CAccessItem;

// ---- globals of other files: raw storage under the program's symbol names (zero at start like the Mac's bss)
alignas(16) unsigned char gOnlineConfigStorage[sizeof(OnlineConfig)] __asm__("_gOnlineConfig");
alignas(16) unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");
alignas(16) unsigned char gTPFileInfoStorage[sizeof(CSYS_TPFileInfo)] __asm__("_gTPFileInfo");
alignas(16) unsigned char gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");
alignas(16) unsigned char gOnlineBalanceStorage[sizeof(COnlineBalance)] __asm__("_gOnlineBalance");
CFSOnlineSystem* gOnlineSystem = nullptr;

namespace {

int64_t I(uint64_t v) { return static_cast<int64_t>(v); }

int64_t FnvStep(uint32_t& h, uint32_t byte) {
    h ^= byte & 0xffu;
    h *= 16777619u;
    return h;
}
int64_t HashBytes(const void* p, size_t n) {
    uint32_t h = 2166136261u;
    const uint8_t* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) FnvStep(h, b[i]);
    return h;
}
int64_t HashText(const char* s) {
    if (s == nullptr) s = "";
    return HashBytes(s, std::strlen(s));
}
int64_t HashText(const char16_t* s) {
    uint32_t h = 2166136261u;
    if (s != nullptr)
        for (; *s != 0; ++s) {
            FnvStep(h, *s & 0xffu);
            FnvStep(h, (*s >> 8) & 0xffu);
        }
    return h;
}
size_t TextLen(const char* s) { return s ? std::strlen(s) : 0; }
size_t TextLen(const char16_t* s) {
    size_t n = 0;
    if (s != nullptr)
        while (s[n] != 0) ++n;
    return n;
}
const char* AssignName(const char*) { return "AssignA"; }
const char* AssignName(const char16_t*) { return "AssignW"; }

template <class C> void SetText(TbDynamicStringTemplate<C>* s, const C* text, size_t n) {
    C* buffer = static_cast<C*>(std::malloc((n + 1) * sizeof(C)));
    for (size_t i = 0; i < n; ++i) buffer[i] = text[i];
    buffer[n] = 0;
    if (s->mCapacity == 1 && s->mData != nullptr) std::free(s->mData);
    s->mData = buffer;
    s->mLength = static_cast<int32_t>(n);
    s->mCapacity = 1;
}

template <class C> void AppendText(TbDynamicStringTemplate<C>* s, const C* text, size_t n) {
    size_t old = TextLen(static_cast<const C*>(s->mData));
    C* buffer = static_cast<C*>(std::malloc((old + n + 1) * sizeof(C)));
    for (size_t i = 0; i < old; ++i) buffer[i] = s->mData[i];
    for (size_t i = 0; i < n; ++i) buffer[old + i] = text[i];
    buffer[old + n] = 0;
    if (s->mCapacity == 1 && s->mData != nullptr) std::free(s->mData);
    s->mData = buffer;
    s->mLength = static_cast<int32_t>(old + n);
    s->mCapacity = 1;
}

CAccessItem* gAccess = nullptr;          // what a CDiscFile made in this test holds (null: no access object)
bool gAccessOn = false;
TbDynamicStringTemplate<char>* gDataPath = nullptr;   // what GetDataPath answers

}  // namespace

// ---- the string class (the program's is another unit): every virtual defined so the vtable exists; the events are the module's stubs'
template <class C> TbDynamicStringTemplate<C>::TbDynamicStringTemplate() {
    this->mLength = 0;
    this->mData = nullptr;
    this->mCapacity = 0;
    mGrowBy = 0;
}
template <class C> TbDynamicStringTemplate<C>::TbDynamicStringTemplate(const TbDynamicStringTemplate& other) {
    this->mLength = 0;
    this->mData = nullptr;
    this->mCapacity = 0;
    mGrowBy = 0;
    ev("StrCopy", {lab(this), HashText(static_cast<const C*>(other.mData))});
    SetText(this, static_cast<const C*>(other.mData), TextLen(static_cast<const C*>(other.mData)));
}
template <class C> TbDynamicStringTemplate<C>::~TbDynamicStringTemplate() {
    ev("StrDtor", {lab(this)});
    if (this->mCapacity == 1 && this->mData != nullptr) std::free(this->mData);
}
template <class C> void TbDynamicStringTemplate<C>::Format(const C*, ...) {}
template <class C> void TbDynamicStringTemplate<C>::ResizeTo(uint32_t) {}
template <class C> void TbDynamicStringTemplate<C>::AllocBuffer(uint32_t) {}
template <class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>&) { return *this; }
template <class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate&) { return *this; }
template <class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C* text) {
    ev(AssignName(text), {lab(this), HashText(text)});
    SetText(this, text, TextLen(text));
    return *this;
}
template <class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C&) { return *this; }
template <class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>& text) {
    const C* data = static_cast<const C*>(text.mData);
    ev("StrAppend", {lab(this), HashText(data)});
    AppendText(this, data, TextLen(data));
    return *this;
}
template <class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C&) { return *this; }
template <class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C*) { return *this; }
template <class C> int32_t TbStringBase<C>::Length() const { return mLength; }
template class TbDynamicStringTemplate<char>;
template class TbDynamicStringTemplate<char16_t>;
template int32_t TbStringBase<char>::Length() const;
template int32_t TbStringBase<char16_t>::Length() const;

// ---- the file classes
bfl::FileSystem::CFile::CFile() { mImplementation = nullptr; }
bfl::FileSystem::CFile::~CFile() {}
void bfl::FileSystem::CFile::Close() { ev("FileClose", {lab(this)}); }
int32_t bfl::FileSystem::CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t bfl::FileSystem::CFile::GetSize() { return 0; }
int32_t bfl::FileSystem::CFile::GetPos() { return 0; }
void* bfl::FileSystem::CFile::MapFile() { return nullptr; }
TbError bfl::FileSystem::CFile::Flush() { return TbError(0); }
int32_t bfl::FileSystem::CFile::GetMacFileReference() { return 0; }
bfl::FileSystem::CDiscFile::CDiscFile() {
    mImplementation = gAccessOn ? gAccess : nullptr;
    ev("FileCtor", {lab(this)});
}
bfl::FileSystem::CDiscFile::~CDiscFile() { ev("FileDtor", {lab(this)}); }
TbError bfl::FileSystem::CDiscFile::Open(const char* name, uint32_t mode) {
    ev("FileOpen", {lab(this), HashText(name), I(mode)});
    return TbError(0);
}
bfl::FileSystem::CAccessItem::CAccessItem() {
    mNext = nullptr;
    mReferences = 0;
}
bfl::FileSystem::CAccessItem::~CAccessItem() {}
int32_t bfl::FileSystem::CAccessItem::Read(void*, uint32_t) { return 0; }
int32_t bfl::FileSystem::CAccessItem::Write(const void* source, uint32_t size) {
    ev("FileWrite", {lab(this), HashBytes(source, size), I(size)});
    return static_cast<int32_t>(ret("FileWrite"));
}
int32_t bfl::FileSystem::CAccessItem::Seek(int32_t, uint32_t) { return 0; }
int32_t bfl::FileSystem::CAccessItem::GetSize() { return 0; }
int32_t bfl::FileSystem::CAccessItem::GetPos() { return 0; }
const uint8_t* bfl::FileSystem::CAccessItem::MapFile(int32_t, int32_t) { return nullptr; }
void bfl::FileSystem::CAccessItem::UnmapFile(const uint8_t*) {}
TbError bfl::FileSystem::CAccessItem::Flush() { return TbError(0); }
int32_t bfl::FileSystem::CAccessItem::GetMacFileReference() { return 0; }
void bfl::FileSystem::CAccessItem::Close() {}
void bfl::FileSystem::CAccessItem::AttachAccessClass(CAccessItem*) {}
_LbSemaphore* bfl::FileSystem::CAccessItem::GetSemaphore() { return nullptr; }

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {HashText(format)}); }
}  // namespace SamsUtilities

Auth::Auth() {
    newlab(this);
    ev("AuthCtor", {lab(this)});
}
int32_t Auth::Connect(char* server, char* unused, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*)) {
    ev("AuthConnect", {lab(this), HashText(server), I(reinterpret_cast<uintptr_t>(unused)), I(port),
                       lab(reinterpret_cast<const void*>(callback))});
    return static_cast<int32_t>(ret("AuthConnect"));
}
void Auth::Login(uint16_t language, char16_t* name, char16_t* password, void* context) {
    ev("AuthLogin", {lab(this), I(language), HashText(name), HashText(password), I(reinterpret_cast<uintptr_t>(context))});
}
News::News() {
    newlab(this);
    ev("NewsCtor", {lab(this)});
}
News::~News() { ev("NewsDtor", {lab(this), 1}); }
void News::Release() { ev("NewsRelease", {lab(this)}); }
int32_t News::Connect(char* server, char* unused, uint16_t port, void (*callback)(int32_t, int32_t, void*, void*)) {
    ev("NewsConnect", {lab(this), HashText(server), I(reinterpret_cast<uintptr_t>(unused)), I(port),
                       lab(reinterpret_cast<const void*>(callback))});
    return static_cast<int32_t>(ret("NewsConnect"));
}
uint32_t News::GetGameServerDetails(uint16_t language, NS_MacDoze::_GUID* guid, void* context) {
    ev("GetGameServerDetails", {lab(this), I(language), lab(guid), I(reinterpret_cast<uintptr_t>(context))});
    return static_cast<uint32_t>(ret("GetGameServerDetails"));
}
uint32_t News::GetGameNews(uint16_t language, void* guid, void* context) {
    ev("GetGameNews", {lab(this), I(language), lab(guid), I(reinterpret_cast<uintptr_t>(context))});
    return static_cast<uint32_t>(ret("GetGameNews"));
}
uint32_t News::GetServerNews(uint16_t language, void* context) {
    ev("GetServerNews", {lab(this), I(language), I(reinterpret_cast<uintptr_t>(context))});
    return static_cast<uint32_t>(ret("GetServerNews"));
}

uint32_t CText::GetLanguageId() const {
    ev("GetLanguageId", {});
    return static_cast<uint32_t>(ret("GetLanguageId"));
}
const TbDynamicStringTemplate<char>& CSYS_TPFileInfo::GetDataPath() const {
    ev("GetDataPath", {lab(this)});
    return *gDataPath;
}
int32_t CBalanceTable::Load(const char* path, LoadMode mode) {
    ev("BalanceLoad", {lab(this), HashText(path), I(static_cast<int32_t>(mode))});
    return static_cast<int32_t>(ret("BalanceLoad"));
}

void CreateUIProgressBar(int32_t text) { ev("CreateUIProgressBar", {I(text)}); }
void AddUIError(int32_t error, TbITable* table) { ev("AddUIError", {I(error), I(reinterpret_cast<uintptr_t>(table))}); }
void CreateOnlineNewsPanel() { ev("CreateOnlineNewsPanel", {}); }
void SetInitialOnlineValues() { ev("SetInitialOnlineValues", {}); }
namespace ftp_callee {
void OperatorDelete(void* p) { ev("OperatorDelete", {lab(p)}); }
}  // namespace ftp_callee
namespace online_callee {
void Free(void* p) { ev("Free", {lab(p)}); }
}  // namespace online_callee

CFSOnlineSystem::CFSOnlineSystem() {}
void CFSOnlineSystem::ReleaseLoginConnection() { ev("ReleaseLoginConnection", {lab(this)}); }
void CFSOnlineSystem::ReleaseNewsConnection() { ev("ReleaseNewsConnection", {lab(this)}); }
void CFSOnlineSystem::CompleteLogin(int32_t result) { ev("CompleteLogin", {lab(this), I(result)}); }
void FSOnlineLoginConnectionCallback(int32_t, int32_t, void*, void*) {}

namespace {
void KeptCallback(int32_t error, void* context) { ev("KeptCallback", {I(error), I(reinterpret_cast<uintptr_t>(context))}); }

CFSOnlineSystem* Sys(uint64_t p) { return reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(p)); }
template <class T> T* At(uint64_t p) { return reinterpret_cast<T*>(static_cast<uintptr_t>(p)); }
int64_t Off(const void* base, const void* member) { return static_cast<const char*>(member) - static_cast<const char*>(base); }
}  // namespace

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_cfsol_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"guid", &gOnlineNewsGuid},
        {"cbLogin", reinterpret_cast<const void*>(&FSOnlineLoginConnectionCallback)},
        {"cbNews", reinterpret_cast<const void*>(&FSOnlineNewsConnectionCallback)},
        {"table", gOnlineBalanceStorage},
        {"fileinfo", gTPFileInfoStorage},
        {"config", gOnlineConfigStorage},
        {"keptcb", reinterpret_cast<const void*>(&KeptCallback)},
        {"separator", &reinterpret_cast<TPGlobals*>(gTPGlobalsStorage)->mSeparator},
        {"override", &reinterpret_cast<TPGlobals*>(gTPGlobalsStorage)->mF413C},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_cfsol_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offsets of the system's members, in the order of the test's FIELDS, then the string layout.
int32_t tpw_cfsol_layout(int64_t* out) {
    CFSOnlineSystem* s = reinterpret_cast<CFSOnlineSystem*>(0x10000);
    const void* b = s;
    int32_t n = 0;
    out[n++] = Off(b, &s->mLogin);
    out[n++] = Off(b, &s->mLoginName);
    out[n++] = Off(b, &s->mLoginPassword);
    out[n++] = Off(b, &s->mF34);
    out[n++] = Off(b, &s->mF38);
    out[n++] = Off(b, &s->mLoginEmail);
    out[n++] = Off(b, &s->mF50);
    out[n++] = Off(b, &s->mF54[0]);
    out[n++] = Off(b, &s->mF54[4]);
    out[n++] = Off(b, &s->mF54[8]);
    out[n++] = Off(b, &s->mLoggedIn);
    out[n++] = Off(b, &s->mLoginDone);
    out[n++] = Off(b, &s->mLoginErrorID);
    out[n++] = Off(b, &s->mNews);
    out[n++] = Off(b, &s->mF70);
    out[n++] = Off(b, &s->mNewsRequest1);
    out[n++] = Off(b, &s->mNewsDone1);
    out[n++] = Off(b, &s->mNewsText);
    out[n++] = Off(b, &s->mNewsRequest2);
    out[n++] = Off(b, &s->mNewsDone2);
    out[n++] = Off(b, &s->mF98);
    out[n++] = Off(b, &s->mNewsRequest3);
    out[n++] = Off(b, &s->mNewsDone3);
    out[n++] = Off(b, &s->mFB4);
    out[n++] = Off(b, &s->mLoginCallbackKept);
    out[n++] = Off(b, &s->mLoginContextKept);
    out[n++] = Off(b, &s->mNewsFinished);
    out[n++] = static_cast<int64_t>(sizeof(CFSOnlineSystem));
    TbDynamicStringTemplate<char>* t = reinterpret_cast<TbDynamicStringTemplate<char>*>(0x10000);
    out[n++] = Off(t, &t->mLength);
    out[n++] = Off(t, &t->mData);
    out[n++] = static_cast<int64_t>(sizeof(TbDynamicStringTemplate<char>));
    OnlineConfig* c = reinterpret_cast<OnlineConfig*>(0x10000);
    out[n++] = Off(c, &c->mLoginPort);
    out[n++] = Off(c, &c->mNewsPort);
    return n;
}

// A string object made at `obj` (kind 0: char, 1: wide) holding `len` units at `data` (not owned).
void tpw_cfsol_mkstr(uint64_t obj, int32_t kind, uint64_t data, int32_t len) {
    if (kind == 0) {
        auto* s = new (At<void>(obj)) TbDynamicStringTemplate<char>();
        s->mData = At<char>(data);
        s->mLength = len;
    } else {
        auto* s = new (At<void>(obj)) TbDynamicStringTemplate<char16_t>();
        s->mData = At<char16_t>(data);
        s->mLength = len;
    }
}

// The servers and ports of the online configuration (Mac: strings made from the two texts, int16 ports).
void tpw_cfsol_cfg(const char* login, int32_t loginPort, const char* news, int32_t newsPort) {
    OnlineConfig* c = reinterpret_cast<OnlineConfig*>(gOnlineConfigStorage);
    new (&c->mLoginServer) std::string(login);
    new (&c->mNewsServer) std::string(news);
    c->mLoginPort = static_cast<int16_t>(loginPort);
    c->mNewsPort = static_cast<int16_t>(newsPort);
}

// A block from the real operator new: a News object the system holds when the case wants one (CompleteNews deletes it).
uint64_t tpw_cfsol_rawnew(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(::operator new(size))); }

// 0: the string GetDataPath answers (an object made by mkstr); 1: whether a CDiscFile holds an access object.
void tpw_cfsol_ctl(int32_t op, uint64_t value) {
    if (op == 0) gDataPath = At<TbDynamicStringTemplate<char>>(value);
    if (op == 1) {
        if (gAccess == nullptr) gAccess = new CAccessItem();
        gAccessOn = value != 0;
    }
}
uint64_t tpw_cfsol_acc() {
    if (gAccess == nullptr) gAccess = new CAccessItem();
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gAccess));
}

// ---- the functions, one wrapper each with C types only
void tpw_cfsol_login_cb(uint64_t self, int32_t a, int32_t b, uint64_t c, uint64_t d) {
    Sys(self)->LoginConnectionCallback(a, b, At<void>(c), At<void>(d));
}
void tpw_cfsol_dologin(uint64_t self, uint64_t name, uint64_t password) {
    Sys(self)->DoLogin(At<char16_t>(name), At<char16_t>(password));
}
void tpw_cfsol_getnews(uint64_t self) { Sys(self)->LoggedInGetNews(); }
void tpw_cfsol_news_cb(uint64_t self, int32_t a, int32_t b, uint64_t c, uint64_t d) {
    Sys(self)->NewsConnectionCallback(a, b, At<void>(c), At<void>(d));
}
void tpw_cfsol_completenews(uint64_t self) { Sys(self)->CompleteNews(); }

}  // extern "C"
