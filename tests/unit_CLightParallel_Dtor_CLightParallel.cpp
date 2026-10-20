// Test library for CLightParallel::~CLightParallel and InterfaceImage::SetValue (unit v51050; libtpw_unit_CLightParallel_Dtor_CLightParallel.dylib),
// main app: src/Light_CLightParallel_Dtor_CLightParallel.cpp.
//
// Fakes: operator delete (records, never frees), InterfaceWindow::SetValue, and a draw object whose Change records.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "Light.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"

using namespace calllog;


void CLightParallel_Delete(void* block) { ev("delete", {lab(block)}); }

int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("WinSetValue", {lab(this), value});
    return 0;
}

bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

InterfaceDrawObject::InterfaceDrawObject(void* data) : mData(data) {}
InterfaceDrawObject::~InterfaceDrawObject() {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t) {}
void InterfaceDrawObject::Draw(int16_t, CoolRect*, int16_t, char16_t*) {}
void InterfaceDrawObject::Change(uint32_t, DOChange*) {}
intptr_t InterfaceDrawObject::Query(uint32_t, intptr_t) { return 0; }
void InterfaceDrawObject::Show(int16_t) {}
void InterfaceDrawObject::Refresh() {}
int32_t InterfaceDrawObject::GetWritableDataArea(int32_t*, int32_t*) { return 0; }
InterfaceDrawObject* InterfaceDrawObject::InstanceMe() { return this; }

namespace {
struct FakeDraw : InterfaceDrawObject {
    FakeDraw() : InterfaceDrawObject(nullptr) {}
    void Change(uint32_t flags, DOChange* change) override { ev("Change", {lab(this), static_cast<int64_t>(flags), change->mFrame}); }
};
}  // namespace

extern "C" {

uint64_t tpw_clp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vt1", gCLightParallelVt1},
        {"vt2", gCLightParallelVt2},
        {"vt3", gCLightParallelVt3},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0)
            return static_cast<uint32_t>(static_cast<const uint8_t*>(entry.address) - tpw::gArenaBase);   // an arena offset, as a Mac word
    return 0;
}

uint64_t tpw_clp_base() {
    tpw::ArenaBase();
    return reinterpret_cast<uintptr_t>(tpw::gArenaBase);
}

// An arena block: its offset (the word the Mac layout stores).
uint64_t tpw_clp_alloc(uint32_t size) { return tpw::ArenaOffset(tpw::ArenaAllocZeroed(size)); }

int32_t tpw_clp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceImage, mValue) - offsetof(InterfaceImage, mDrawObject));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceImage, mClientDrawObject) - offsetof(InterfaceImage, mDrawObject));
    return n;
}

uint64_t tpw_clp_newimage(uint64_t d1, uint64_t d2) {
    InterfaceImage* img = static_cast<InterfaceImage*>(tpw::ArenaAllocZeroed(sizeof(InterfaceImage)));
    img->mDrawObject = reinterpret_cast<InterfaceDrawObject*>(static_cast<uintptr_t>(d1));
    img->mClientDrawObject = reinterpret_cast<InterfaceDrawObject*>(static_cast<uintptr_t>(d2));
    return reinterpret_cast<uintptr_t>(img);
}

uint64_t tpw_clp_newdraw() {
    void* mem = tpw::ArenaAllocZeroed(sizeof(FakeDraw));
    return reinterpret_cast<uintptr_t>(new (mem) FakeDraw());
}

int32_t tpw_clp_value(uint64_t img) { return reinterpret_cast<InterfaceImage*>(static_cast<uintptr_t>(img))->mValue; }

uint64_t tpw_clp_dtor(uint64_t off, int32_t flag) {
    CLightParallel* self = off != 0 ? reinterpret_cast<CLightParallel*>(tpw::gArenaBase + off) : nullptr;
    return reinterpret_cast<uintptr_t>(CLightParallel_DtorMac(self, static_cast<int16_t>(flag)));
}

int32_t tpw_clp_setvalue(uint64_t img, int32_t value) {
    return reinterpret_cast<InterfaceImage*>(static_cast<uintptr_t>(img))->InterfaceImage::SetValue(value);
}

}  // extern "C"
