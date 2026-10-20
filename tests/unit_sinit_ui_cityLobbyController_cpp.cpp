// Test library for the static initialiser of ui_cityLobbyController.cpp (libtpw_unit_sinit_ui_cityLobbyController_cpp.dylib).
// The string constructor and __register_global_object are recording fakes; the string pool is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorScoring.h"
#include "TbDynamicStringTemplate.h"

using namespace calllog;

extern const char gCityLobbyStringPool[0x100];
const char gCityLobbyStringPool[0x100] = {0};
void __sinit_ui_cityLobbyController_cpp();
void* CityLobbyStringStorage(int which);
void* CityLobbyNodeStorage(int which);

// Only so the library links: the launcher wrappers in the src file name the real string members; never called here.
extern "C" void FakeRealStringCtor(void*, const char16_t*) __asm__("__ZN23TbDynamicStringTemplateIDsEC1EPKDs");
extern "C" void FakeRealStringCtor(void*, const char16_t*) {}
extern "C" void FakeRealStringDtor(void*) __asm__("__ZN23TbDynamicStringTemplateIDsED1Ev");
extern "C" void FakeRealStringDtor(void*) {}

extern "C" void CityLobbyStringDestructor(void*, int16_t) {}

extern "C" void* CityLobbyStringConstruct(void* self, const char16_t* text) {
    ev("Ctor", {lab(self), static_cast<int64_t>(reinterpret_cast<const char*>(text) - gCityLobbyStringPool)});
    return self;
}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("Register", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_sinit_addr(const char* name) {
    if (std::strcmp(name, "pool") == 0) return reinterpret_cast<uintptr_t>(gCityLobbyStringPool);
    if (std::strcmp(name, "strA") == 0) return reinterpret_cast<uintptr_t>(CityLobbyStringStorage(0));
    if (std::strcmp(name, "strB") == 0) return reinterpret_cast<uintptr_t>(CityLobbyStringStorage(1));
    if (std::strcmp(name, "nodeA") == 0) return reinterpret_cast<uintptr_t>(CityLobbyNodeStorage(0));
    if (std::strcmp(name, "nodeB") == 0) return reinterpret_cast<uintptr_t>(CityLobbyNodeStorage(1));
    if (std::strcmp(name, "dtor") == 0) return reinterpret_cast<uintptr_t>(&CityLobbyStringDestructor);
    return 0;
}

void tpw_sinit_run() { __sinit_ui_cityLobbyController_cpp(); }

}  // extern "C"
