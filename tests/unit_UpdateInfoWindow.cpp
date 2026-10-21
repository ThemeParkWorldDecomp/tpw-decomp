// Test library for the ride list panel's info window (unit v5241; libtpw_unit_UpdateInfoWindow.dylib, tests/test_UpdateInfoWindow.py), main app:
//   src/game/ui/ui_ridepanel.cpp   InitInfoWindow, UpdateInfoWindow
//
// The callees outside the unit are fakes that record what the real ones read. The windows are fake objects whose virtual SetValue and
// SetText record (the Mac's go through __ptr_glue); the constructors of the text and gradient objects are fakes under their symbol names.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <new>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "ui_main.h"
#include "TPEngine.h"
#include "World.h"
#include "analyser.h"
#include "RideDB.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
int32_t gRideListType;
InterfaceWindow* gRideListPanel;
CWorld* gGameWorld;
char16_t gUIMainConvertBuffer[0x4000];
// gRideDatabase is a class object with a constructor that is not linked: its storage under its symbol name (only its address is used).
alignas(16) char gRideDatabaseStorage[0x10000] __asm__("_gRideDatabase");

namespace {

// Labels 1 to 12 are windows, 13 the ride record, 14 the world, 15 the analyser, 16 the string table, 17 the font, 18 the ride database.
void* gBlocks[32];

void MakeWindow(void* memory);

void* Block(int64_t label) {
    if (label <= 0 || label >= 32) return nullptr;
    if (gBlocks[label] == nullptr) {
        void* memory = tpw::ArenaAllocZeroed(label == 16 ? 0x1000 : 0x400);
        if (label >= 1 && label <= 12) MakeWindow(memory);
        gBlocks[label] = memory;
    }
    return gBlocks[label];
}

uint32_t Hash(const char16_t* text) {
    uint32_t h = 0;
    if (text == nullptr) return 0;
    for (; *text != 0; ++text) h = h * 31 + *text;
    return h;
}

}  // namespace

// A window with the vtable shape of InterfaceWindow (Itanium: the destructor takes two slots) whose SetValue and SetText record.
struct FakeWindowPlacement {
    virtual void DrawFunction() {}
    virtual void CalcClientArea() {}
    virtual ~FakeWindowPlacement() {}
    virtual int32_t WhatAmI() { return 0; }
    virtual void ClientDraw() {}
    virtual int16_t Enable(int16_t) { return 0; }
    virtual int32_t Minus(InterfaceWindow&) { return 0; }
    virtual int32_t SetValue(int32_t value) {
        ev("SetValue", {lab(this), value});
        return 0;
    }
    virtual int32_t GetValue() { return 0; }
    virtual int32_t SetText(char16_t* text) {
        ev("SetText", {lab(this), static_cast<int64_t>(Hash(text))});
        return 0;
    }
};

namespace {
void MakeWindow(void* memory) { new (memory) FakeWindowPlacement; }
}  // namespace

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return static_cast<InterfaceWindow*>(Block(ret("GetWindow", id, 0)));
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }

void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }

void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {lab(this)}, {narrow(text).c_str()}); }

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return static_cast<RideInfo*>(Block(13));
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return static_cast<CParkAnalyser*>(Block(15));
}

int32_t CParkAnalyser::NumberOfObjectInPark(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const {
    ev("NumberOfObjectInPark", {lab(this), id.mIndex});
    return static_cast<int32_t>(ret("NumberOfObjectInPark"));
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return static_cast<InterfaceFont*>(Block(17));
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<char*>(Block(16)) + id * 0x10);
}

void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) { ev("UIMain_IndentClient", {lab(window), a, b}); }

int swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    int32_t arg = 0;
    bool isInteger = format[0] == '%' && format[1] == 'd' && format[2] == 0;
    std::va_list args;
    va_start(args, format);
    if (isInteger) arg = va_arg(args, int);
    va_end(args);
    std::u16string out;
    if (isInteger) {
        for (char c : std::to_string(arg)) out.push_back(static_cast<char16_t>(c));
    } else {
        out = format;
    }
    if (size > 0) {
        if (out.size() > size - 1) out.resize(size - 1);
        std::memcpy(buffer, out.c_str(), (out.size() + 1) * 2);
    }
    ev("swprintf", {static_cast<int64_t>(size), isInteger ? 1 : 0, isInteger ? arg : 0, static_cast<int64_t>(Hash(format))});
    return static_cast<int>(out.size());
}

// The constructors of the draw objects (their classes are not linked): under the symbol the translation calls.
extern "C" void FakeSpriteConstructor(void* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
extern "C" void FakeSpriteConstructor(void* self, void* data) { ev("InterfaceTextObjectSprite", {newlab(self), lab(data)}); }

extern "C" void FakeGradientConstructor(void* self, void* mesh) __asm__("__ZN28InterfaceDrawEngineHGradientC1EPv");
extern "C" void FakeGradientConstructor(void* self, void* mesh) {
    ev("InterfaceDrawEngineHGradient", {newlab(self)}, {static_cast<const char*>(mesh)});
}

extern "C" {

uint64_t tpw_ui_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gRideListType", &gRideListType},
        {"gRideListPanel", &gRideListPanel},
        {"gGameWorld", &gGameWorld},
        {"buffer", gUIMainConvertBuffer},
        {"rdb", gRideDatabaseStorage},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ui_block(int32_t label) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Block(label))); }

uint64_t tpw_ui_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ui_layout(int64_t* out) {
    int32_t n = 0;
    alignas(16) static char storage[0x2000];
    RideInfo* ride = reinterpret_cast<RideInfo*>(storage);
    for (int32_t slot : {76, 100, 105})
        out[n++] = reinterpret_cast<char*>(&ride->Value(static_cast<RideBalanceSlot>(slot))) - storage;
    out[n++] = sizeof(gUIMainConvertBuffer);
    out[n++] = sizeof(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>);
    return n;
}

void tpw_ui_init() { InitInfoWindow(); }

void tpw_ui_update(uint64_t id) { UpdateInfoWindow(*reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>*>(id)); }

}  // extern "C"
