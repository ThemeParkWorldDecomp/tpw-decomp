// Test library for AddressBook::Save (unit v5009; libtpw_unit_AddressBook_Save.dylib, tests/test_AddressBook_Save.py), main app:
//   src/email_load_AddressBook_Save.cpp   AddressBook::Save
//
// The stream callees (ofstream ctor, open, operator<<, close, the dtors, __throw) are recording fakes; they answer from the case
// passed to the wrapper. The fake throw returns, like the PowerPC fake.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <new>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"

using namespace calllog;

namespace {
struct CaseData {
    uint32_t state, rdbuf, mask, openRet, flag, closeRet;
} gCase;
SaveIos* gIos = nullptr;
}  // namespace

void SaveStreamCtor(SaveStream* s) {
    ev("ofstream", {0});
    s->ios = static_cast<SaveIos*>(tpw::ArenaAllocZeroed(sizeof(SaveIos)));
    s->ios->mState = gCase.state;
    s->ios->mRdbuf = gCase.rdbuf;
    s->ios->mMask = gCase.mask;
    s->filebufOpen = 0;
    s->impl = nullptr;
    gIos = s->ios;
}
bool SaveStreamOpen(SaveStream* s, const char* name) {
    ev("open", {0x10}, {name});
    s->filebufOpen = gCase.flag;
    return gCase.openRet != 0;
}
void SaveStreamWrite(SaveStream*, const char* text) { ev("out", {0}, {text}); }
bool SaveStreamClose(SaveStream*) {
    ev("close", {0});
    return gCase.closeRet != 0;
}
void SaveStreamDestroy(SaveStream*) {
    ev("filebuf_dtor", {0});
    ev("ostream_dtor", {0});
    ev("ios_dtor", {0});
}
void SaveStreamThrow(const char* what) { ev("throw", {0}, {what}); }

extern "C" {

uint64_t tpw_save_addr(const char*) { return 0; }

uint64_t tpw_save_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_save_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(SaveIos, mRdbuf));
    out[n++] = static_cast<int64_t>(offsetof(SaveIos, mState));
    out[n++] = static_cast<int64_t>(offsetof(SaveIos, mMask));
    out[n++] = static_cast<int64_t>(sizeof(SaveIos));
    return n;
}

uint64_t tpw_save_ios() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gIos)); }

// blob: n names, each ended by '\n'.
int32_t tpw_save_run(const char* path, const char* blob, int32_t n, uint32_t state, uint32_t rdbuf, uint32_t mask, uint32_t openRet,
                     uint32_t flag, uint32_t closeRet) {
    gCase = {state, rdbuf, mask, openRet, flag, closeRet};
    gIos = nullptr;
    alignas(AddressBook) static char storage[sizeof(AddressBook)];
    AddressBook* book = reinterpret_cast<AddressBook*>(storage);
    new (&book->mAddresses) std::list<std::string>();
    const char* p = blob;
    for (int32_t i = 0; i < n; ++i) {
        const char* e = std::strchr(p, '\n');
        book->mAddresses.push_back(std::string(p, e - p));
        p = e + 1;
    }
    int32_t r = book->Save(path);
    book->mAddresses.~list();
    return r;
}

}  // extern "C"
