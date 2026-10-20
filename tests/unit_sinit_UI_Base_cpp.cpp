// Test library for UI_ToggleHelpText and __sinit_UI_Base_cpp (unit v5295; libtpw_unit_sinit_UI_Base_cpp.dylib), main app:
//   src/UI_pump_sinit_UI_Base_cpp.cpp
//
// InterfaceHelpText's constructor and virtuals are fakes that record events (the class's vtable is made here), the clock's
// virtual is a stub, __register_global_object records its arguments. Globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "UI_Base.h"

using namespace calllog;

static bool gMute = false;

InterfaceHelpText* gUIHelpText;
InterfaceHelpText gUIDefaultHelpText;
clockTickFunction gUIDefaultClock;
clockTickFunction* gUIClock;

InterfaceHelpText::InterfaceHelpText() {
    if (!gMute) ev("InterfaceHelpText", {lab(this)});
}
InterfaceHelpText::~InterfaceHelpText() {}
void InterfaceHelpText::ShowHelpText() {}
void InterfaceHelpText::HideHelpText() {}
void InterfaceHelpText::Enable() { ev("Enable", {lab(this)}); }
void InterfaceHelpText::Disable() { ev("Disable", {lab(this)}); }
void InterfaceHelpText::Show() {}
void InterfaceHelpText::Hide() {}
void InterfaceHelpText::Update() {}
void InterfaceHelpText::OnMouseMoved(int16_t, int16_t) {}
void InterfaceHelpText::OnMouseMovedIn(InterfaceWindow*) {}
void InterfaceHelpText::OnMouseMovedOut() {}
void InterfaceHelpText::OnMouseDown() {}
void InterfaceHelpText::OnFrame() {}
void InterfaceHelpText::OnHideOrDelete(InterfaceWindow*) {}
void InterfaceHelpText::Init() {}
void InterfaceHelpText::DeInit() {}

uint32_t clockTickFunction::tickyticky() { return 0; }

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register_global_object", {lab(object), lab(destructor), lab(node)});
}

namespace {
InterfaceHelpText* TestObject() {
    static InterfaceHelpText* object = nullptr;
    if (!object) {
        gMute = true;
        object = new (tpw::ArenaAllocZeroed(64)) InterfaceHelpText();
        gMute = false;
    }
    return object;
}
}  // namespace

extern "C" {

uint64_t tpw_tsb_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"default", &gUIDefaultHelpText},
        {"dtor", reinterpret_cast<const void*>(&UI_DestroyDefaultHelpText)},
        {"node", gUIDefaultHelpTextNode},
        {"obj", TestObject()},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_tsb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_tsb_layout(int64_t* out) {
    (void)out;
    return 0;
}

// Makes the test object the handler with the given mEnabled.
void tpw_tsb_set_help(int32_t enabled) {
    InterfaceHelpText* object = TestObject();
    object->mEnabled = static_cast<int16_t>(enabled);
    gUIHelpText = object;
}

// The clock's vtable pointer: zeroed before the call, set (not 0) after.
void tpw_tsb_clock_clear() { std::memset(static_cast<void*>(&gUIDefaultClock), 0, sizeof(void*)); }
uint32_t tpw_tsb_clock_set() {
    void* vptr;
    std::memcpy(&vptr, static_cast<void*>(&gUIDefaultClock), sizeof(void*));
    return vptr != nullptr;
}

void tpw_tsb_toggle() { UI_ToggleHelpText(); }
void tpw_tsb_sinit() { __sinit_UI_Base_cpp(); }

}  // extern "C"
