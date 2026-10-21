// Test library for CreateRestartParkSaveGame (unit v5549; libtpw_unit_CreateRestartParkSaveGame.dylib, tests/test_CreateRestartParkSaveGame.py), main app:
//   src/game/GameSys.cpp   CreateRestartParkSaveGame
//
// The callees outside the unit (the file, the string and the user info classes, UniStringToString, SaveHidden) are recording fakes that
// log the same events as the module's PowerPC stubs. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "GameSys.h"
#include "FS_LoadSave.h"
#include "FS_UserInfo.h"
#include "TPGlobals.h"
#include "TPString.h"
#include "TbDynamicStringTemplate.h"
#include "TbError.h"
#include "bfl_FileSystem.h"

using namespace calllog;

// ---- globals of other files
CFSUserInfo* gUserInfo;
CFSLoadSave gLoadSave;
// TPGlobals has members whose constructors live in other files; the storage is plain bytes under the same symbol, zero like the Mac's bss.
alignas(16) char gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");

namespace {

const char* const kNames[] = {"", "restart", "data:users:a:land:restart.INTS", "x"};

int64_t Hash(const char* s) {
    uint32_t h = 0;
    for (; *s; ++s) h = h * 31u + static_cast<uint8_t>(*s);
    return h & 0x7fffffffu;
}

int64_t L(const void* p) { return static_cast<int64_t>(lab(const_cast<void*>(p))); }

}  // namespace

// ---- the fakes
// Members of CFSUserInfo (its fake constructor builds them); never used here.
AddressBook::AddressBook() {}
AddressBook::~AddressBook() {}
Outbox::Outbox() {}
Outbox::~Outbox() {}

CFSUserInfo::CFSUserInfo() {
    newlab(this);
    ev("CFSUserInfo", {L(this)});
}

// Strings: the narrow constructor logs, the wide default constructor does not (the temporary of ReturnUsersPathWithTheme stays unlabeled).
template <class Char>
TbDynamicStringTemplate<Char>::TbDynamicStringTemplate() {
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
    if (sizeof(Char) == 1) {
        newlab(this);
        ev("SCtor", {L(this)});
    }
}
template <class Char>
TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbDynamicStringTemplate<Char>& other) {
    mLength = 0;
    mData = nullptr;
    mCapacity = 0;
    mGrowBy = 0;
    newlab(this);
    ev("WCopy", {L(this), L(&other)});
}
template <class Char>
TbDynamicStringTemplate<Char>::~TbDynamicStringTemplate() {
    ev(sizeof(Char) == 1 ? "SDtor" : "WDtor", {L(this)});
}
template <class Char>
TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const TbStringBase<Char>& text) {
    ev("WAppend", {L(this), L(&text)});
    return *this;
}
template <class Char> void TbDynamicStringTemplate<Char>::Format(const Char*, ...) {}
template <class Char> void TbDynamicStringTemplate<Char>::ResizeTo(uint32_t) {}
template <class Char> void TbDynamicStringTemplate<Char>::AllocBuffer(uint32_t) {}
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbStringBase<Char>&) { return *this; }
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbDynamicStringTemplate<Char>&) { return *this; }
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char*) { return *this; }
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char&) { return *this; }
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char&) { return *this; }
template <class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char*) { return *this; }

template class TbDynamicStringTemplate<char16_t>;
template class TbDynamicStringTemplate<char>;

TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnUsersPathWithTheme() {
    ev("Users", {L(this)});
    return TbDynamicStringTemplate<char16_t>();
}

void CFSLoadSave::SaveHidden(const TbDynamicStringTemplate<char16_t>& name) {
    ev("SaveHidden", {L(this), L(&name)});
}

int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    ev("Uni", {L(&out), L(&in)});
    int64_t idx = static_cast<int64_t>(ret("Uni"));
    out.mData = const_cast<char*>(kNames[idx]);
    out.mLength = static_cast<int32_t>(std::strlen(kNames[idx]));
    return 0;
}

// files
namespace bfl {
namespace FileSystem {
CFile::CFile() : mImplementation(nullptr) {}
CFile::~CFile() {}
void CFile::Close() {
    ev("Close", {L(this)});
}
int32_t CFile::Seek(uint32_t, uint32_t) { return 0; }
int32_t CFile::GetSize() { return 0; }
int32_t CFile::GetPos() { return 0; }
void* CFile::MapFile() { return nullptr; }
TbError CFile::Flush() { return TbError(0); }
int32_t CFile::GetMacFileReference() { return 0; }
CDiscFile::CDiscFile() {
    newlab(this);
    ev("DiscCtor", {L(this)});
}
CDiscFile::~CDiscFile() {
    ev("DiscDtor", {L(this)});
}
TbError CDiscFile::Open(const char* name, uint32_t mode) {
    ev("Open", {L(this), Hash(name), static_cast<int64_t>(mode)});
    return TbError(static_cast<int32_t>(ret("Open")));
}
}  // namespace FileSystem
}  // namespace bfl

extern "C" {

uint64_t tpw_crp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gUserInfo", &gUserInfo},
        {"sep", &gTPGlobals.mWideSeparator},
        {"restart", &gTPGlobals.mRestartName},
        {"ints", &gTPGlobals.mF44AC},
        {"loadsave", &gLoadSave},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_crp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_crp_layout(int64_t* out) {
    int32_t n = 0;
    (void)out;   // nothing is written by Mac offset; the members are found by address (host offsets differ: native pointers)
    return n;
}

void tpw_crp_call() { CreateRestartParkSaveGame(); }

}  // extern "C"
