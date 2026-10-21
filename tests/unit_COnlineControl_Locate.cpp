// Test library for COnlineControl's chat commands (unit v5023; libtpw_unit_COnlineControl_Locate.dylib, tests/test_COnlineControl_Locate.py), main app:
//   src/game/online/online.cpp   Locate, Page, Goto, BlackMark, BuddyOn
//
// The callees (strings, Argument, the argument vector, the buddy map's find, DoCommand) are fakes that record what the real ones read and
// write marker words into the objects they build, so that the right object reaching the right call shows in the log.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"

using namespace calllog;

namespace {

uint32_t Rd(const void* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

void Wr(void* p, uint32_t v) { std::memcpy(p, &v, 4); }

}  // namespace

namespace online_callee {

void StringCtor(void* self) {
    ev("StrCtor", {});
    Wr(self, 0xB6);
}

void WStringCtor(void* self) {
    ev("WStrCtor", {});
    Wr(self, 0xA5);
}

void StringDtor(void* self, int16_t flag) { ev("StrDtor", {flag, Rd(self)}); }

void WStringDtor(void* self, int16_t flag) { ev("WStrDtor", {flag, Rd(self)}); }

void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("WCopy", {lab(const_cast<void*>(other)), pos, count});
    Wr(self, 0xE9);
}

void NocaseWStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("NCopy", {lab(const_cast<void*>(other)), pos, count});
    Wr(self, 0xF1);
}

void NocaseWStringDtor(void* self, int16_t flag) { ev("NDtor", {flag, Rd(self)}); }

void* BuddyTreeFind(void* tree, const void* key) {
    ev("Find", {lab(tree), Rd(key)});
    const int64_t answer = ret("Find");
    return static_cast<uint8_t*>(tree) + (answer ? 4 : 8);
}

void ArgumentCtor(void* self, const void* text) {
    ev("ArgCtor", {Rd(text)});
    Wr(self, 0xC7);
    Wr(static_cast<uint8_t*>(self) + 4, 0xD8);
    Wr(static_cast<uint8_t*>(self) + 8, 0x77);
}

void ArgumentVectorPushBack(void* vec, const void* argument) {
    const uint8_t* a = static_cast<const uint8_t*>(argument);
    ev("PushBack", {Rd(vec), Rd(a), Rd(a + 4), Rd(a + 8)});
}

void ArgumentVectorTearDown(void* vec) {
    const uint8_t* v = static_cast<const uint8_t*>(vec);
    ev("TearDown", {Rd(v), Rd(v + 4), Rd(v + 8)});
}

}  // namespace online_callee

void COnlineControl::DoCommand(void* arguments) {
    const uint8_t* v = static_cast<const uint8_t*>(arguments);
    ev("DoCommand", {lab(this), Rd(v), Rd(v + 4), Rd(v + 8)});
}

extern "C" {

uint64_t tpw_oc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_oc_Locate(uint64_t self, uint64_t name) {
    reinterpret_cast<COnlineControl*>(self)->Locate(*reinterpret_cast<std::u16string*>(name));
}
void tpw_oc_Page(uint64_t self, uint64_t name) {
    reinterpret_cast<COnlineControl*>(self)->Page(*reinterpret_cast<std::u16string*>(name));
}
void tpw_oc_Goto(uint64_t self, uint64_t name) {
    reinterpret_cast<COnlineControl*>(self)->Goto(*reinterpret_cast<std::u16string*>(name));
}
void tpw_oc_BlackMark(uint64_t self, uint64_t name) {
    reinterpret_cast<COnlineControl*>(self)->BlackMark(*reinterpret_cast<std::u16string*>(name));
}
void tpw_oc_BuddyOn(uint64_t self, uint64_t name) {
    reinterpret_cast<COnlineControl*>(self)->BuddyOn(*reinterpret_cast<std::u16string*>(name));
}

}  // extern "C"
