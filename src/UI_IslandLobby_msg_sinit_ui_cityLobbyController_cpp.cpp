// 0x101e0cec (116 bytes): static initialiser of ui_cityLobbyController.cpp. Builds two global dynamic wide strings from
// literals in the file's string pool (pool + 0x3d and pool + 0x4b) and registers each with its destructor for exit.
#include <cstddef>
#include <cstdint>
#include <new>

#include "AdvisorScoring.h"
#include "TbDynamicStringTemplate.h"

// The three externs (string pool, destructor, constructor 0x10238800) are declared in AdvisorScoring.h.

// The launcher's definitions of the two glue names; the unit test library overrides them with recording fakes. The real string
// members are reached through their symbols (weak, so the test library links without the string sources).
extern "C" __attribute__((weak)) void RealCityLobbyStringCtor(void* self, const char16_t* text) __asm__("__ZN23TbDynamicStringTemplateIDsEC1EPKDs");
extern "C" __attribute__((weak)) void RealCityLobbyStringDtor(void* self) __asm__("__ZN23TbDynamicStringTemplateIDsED1Ev");
extern "C" __attribute__((weak)) void* CityLobbyStringConstruct(void* self, const char16_t* text) {
    RealCityLobbyStringCtor(self, text);
    return self;
}
extern "C" __attribute__((weak)) void CityLobbyStringDestructor(void* object, int16_t deleteFlag) {
    (void)deleteFlag;
    RealCityLobbyStringDtor(object);
}

alignas(8) static unsigned char gCityLobbyStringA[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x10395c94
alignas(8) static unsigned char gCityLobbyNodeA[16];                                            // 0x10395ca8
alignas(8) static unsigned char gCityLobbyStringB[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x10395c74
alignas(8) static unsigned char gCityLobbyNodeB[16];                                            // 0x10395c88

void* CityLobbyStringStorage(int which) { return which == 0 ? (void*)gCityLobbyStringA : (void*)gCityLobbyStringB; }
void* CityLobbyNodeStorage(int which) { return which == 0 ? (void*)gCityLobbyNodeA : (void*)gCityLobbyNodeB; }

void __sinit_ui_cityLobbyController_cpp() {
    void* a = CityLobbyStringConstruct(gCityLobbyStringA, reinterpret_cast<const char16_t*>(gCityLobbyStringPool + 0x3d));
    __register_global_object(a, reinterpret_cast<void*>(&CityLobbyStringDestructor), gCityLobbyNodeA);
    void* b = CityLobbyStringConstruct(gCityLobbyStringB, reinterpret_cast<const char16_t*>(gCityLobbyStringPool + 0x4b));
    __register_global_object(b, reinterpret_cast<void*>(&CityLobbyStringDestructor), gCityLobbyNodeB);
}
