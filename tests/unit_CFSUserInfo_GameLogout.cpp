// Test library for CFSUserInfo::GameLogout (libtpw_unit_CFSUserInfo_GameLogout.dylib, tests/test_CFSUserInfo_GameLogout.py), main app:
//   src/FS_UserInfo_CFSUserInfo_GameLogout.cpp
// Every callee is a recording fake; the strings are fakes that only record.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_UserInfo.h"
#include "FS_MasterSave.h"
#include "TPGlobals.h"
#include "TPSettings.h"
#include "TPString.h"

using namespace calllog;

TPGlobals gTPGlobals;
CPlayerSettings gPlayerSettings;
CFSGlobalMasterSave* gMasterSave;

namespace {
int gQuiet = 0;
int gUniCalls = 0;
}  // namespace

// ---- the string class: every member the vtable names, all recording fakes
#define TB_FAKES(CH, NAME)                                                                                   \
    template <> TbDynamicStringTemplate<CH>::TbDynamicStringTemplate() {                                     \
        if (!gQuiet) {                                                                                       \
            int64_t a = newlab(this);                                                                        \
            ev(NAME "Ctor", {a});                                                                            \
        }                                                                                                    \
    }                                                                                                        \
    template <> TbDynamicStringTemplate<CH>::TbDynamicStringTemplate(const TbDynamicStringTemplate& other) { \
        int64_t a = newlab(this);                                                                            \
        ev(NAME "Copy", {a, lab(&other)});                                                                   \
    }                                                                                                        \
    template <> TbDynamicStringTemplate<CH>::~TbDynamicStringTemplate() { ev(NAME "Dtor", {lab(this)}); }    \
    template <> void TbDynamicStringTemplate<CH>::Format(const CH*, ...) {}                                  \
    template <> void TbDynamicStringTemplate<CH>::ResizeTo(uint32_t) {}                                      \
    template <> void TbDynamicStringTemplate<CH>::AllocBuffer(uint32_t) {}                                   \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbStringBase<CH>&) { return *this; } \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbDynamicStringTemplate& o) {      \
        ev(NAME "Assign", {lab(this), lab(&o)});                                                             \
        return *this;                                                                                        \
    }                                                                                                        \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH*) { return *this; }            \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH&) { return *this; }             \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const TbStringBase<CH>& t) {            \
        ev(NAME "Append", {lab(this), lab(&t)});                                                             \
        return *this;                                                                                        \
    }                                                                                                        \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH&) { return *this; }            \
    template <> TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH*) { return *this; }

TB_FAKES(char, "N")
TB_FAKES(char16_t, "W")

// ---- the other callees
static const char* const kUniTexts[] = {"slotpath", "settings", "extra"};

int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in) {
    ev("UniToStr", {lab(&out), lab(&in)});
    out.mData = const_cast<char*>(kUniTexts[gUniCalls < 2 ? gUniCalls : 2]);
    gUniCalls++;
    return 1;
}

TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnGameLoginSlotUsersPath(EFSUserInfoGameUser slot) {
    gQuiet++;
    TbDynamicStringTemplate<char16_t> r;
    gQuiet--;
    int64_t a = newlab(&r);
    ev("RetSlotPath", {a, lab(this), static_cast<int64_t>(slot)});
    return r;
}

TbDynamicStringTemplate<char16_t> CFSUserInfo::ReturnPathToSaveDirectory() {
    gQuiet++;
    TbDynamicStringTemplate<char16_t> r;
    gQuiet--;
    int64_t a = newlab(&r);
    ev("RetSavePath", {a, lab(this)});
    return r;
}

void CFSUserInfo::SaveGlobalMasterSave() { ev("SaveMaster", {lab(this)}); }

int32_t AddressBook::Save(const char* path) {
    ev("AddressBookSave", {lab(this)}, {path});
    return 0;
}

void Outbox::Save() { ev("OutboxSave", {lab(this)}); }

CFSGlobalMasterSave::~CFSGlobalMasterSave() { ev("MasterDtor", {lab(this)}); }   // the "delete" event follows from the recording operator delete

TPGlobals::TPGlobals() {}
TPGlobals::~TPGlobals() {}
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}

int32_t CPlayerSettings::SaveGlobal(TbDynamicStringTemplate<char>& path) {
    ev("SaveGlobal", {lab(this)}, {path.mData});
    return 1;
}

extern "C" {

uint64_t tpw_gl_addr(const char* name) {
    if (std::strcmp(name, "sep") == 0) return reinterpret_cast<uintptr_t>(&gTPGlobals.mWideSeparator);
    if (std::strcmp(name, "cfg") == 0) return reinterpret_cast<uintptr_t>(&gTPGlobals.mConfigFile);
    if (std::strcmp(name, "settings") == 0) return reinterpret_cast<uintptr_t>(&gPlayerSettings);
    return 0;
}

uint64_t tpw_gl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// resets the counters, sets the master save (a heap block the fake destructor does not touch) and answers its address
uint64_t tpw_gl_setup(int32_t master) {
    gQuiet = 0;
    gUniCalls = 0;
    gMasterSave = master ? static_cast<CFSGlobalMasterSave*>(::operator new(sizeof(CFSGlobalMasterSave))) : nullptr;
    return reinterpret_cast<uintptr_t>(gMasterSave);
}

int32_t tpw_gl_master_set() { return gMasterSave != nullptr; }

int32_t tpw_gl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CFSUserInfo, mGameUser));
    out[n++] = static_cast<int64_t>(offsetof(CFSUserInfo, mOutbox));
    out[n++] = static_cast<int64_t>(offsetof(CFSUserInfo, mAddressBook));
    out[n++] = static_cast<int64_t>(sizeof(CFSUserInfo));
    return n;
}

void tpw_gl_logout(uint64_t info) { reinterpret_cast<CFSUserInfo*>(info)->GameLogout(); }

}  // extern "C"
