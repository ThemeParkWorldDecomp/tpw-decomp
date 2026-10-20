// Test library for NameWindow and TankButton (unit v5539; libtpw_unit_NameWindow.dylib, tests/test_NameWindow.py), main app:
//   src/DoScroll_NameWindow.cpp, src/CreateResearchPanel_RideMsg.cpp
//
// The callees outside the unit are recording fakes; the globals of other files are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "TPEngine.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"

using namespace calllog;

void TankButton(int32_t which, InterfaceWindow* window, int32_t flag);

CWorld* gGameWorld;
InterfaceWindow* gResearchPanel;
// gText is zero storage under its own symbol: CText's member constructors live in files this library does not compile.
alignas(16) char gTextStorage[sizeof(CText)] asm("_gText");
CRideDatabase gRideDatabase;

static CResearchLab* gLabPtr;
static InterfaceWindow* gParentPtr;
static InterfaceWindow* gChildPtr;

static const char16_t kUnknown[] = {u'U', u'n', u'k', 0};

// The tags' key functions (vtables); never called, FormatString is a fake.
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }
int32_t TbUniStringVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

CResearchLab* CWorld::GetResearchLab() {
    ev("GetResearchLab", {lab(this)});
    return gLabPtr;
}

tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CResearchLab::GetCurrentProject(ResearchCat category) const {
    ev("GetCurrentProject", {lab(this), static_cast<int32_t>(category)});
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret("GetCurrentProject"));
    return t;
}

int32_t CResearchLab::GetEstimatedCostForCurrentProject(ResearchCat category) const {
    ev("GetEstimatedCostForCurrentProject", {lab(this), static_cast<int32_t>(category)});
    return static_cast<int32_t>(ret("GetEstimatedCostForCurrentProject"));
}

void CResearchLab::NextProject(ResearchCat category) { ev("NextProject", {lab(this), static_cast<int32_t>(category)}); }
void CResearchLab::PreviousProject(ResearchCat category) { ev("PreviousProject", {lab(this), static_cast<int32_t>(category)}); }

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return ret("GetRideInfo") != 0 ? reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(0x1000)) : nullptr;
}

void CRideDatabase::GetTypeName(uint16_t id, char16_t* buffer, uint32_t size) {
    ev("GetTypeName", {lab(this), id, static_cast<int64_t>(size)});
    buffer[0] = static_cast<char16_t>(0x4e + (id & 7));
    buffer[1] = 0x58;
    buffer[2] = 0;
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return this == gResearchPanel ? gParentPtr : gChildPtr;
}

void InterfaceWindow::SetClientDrawText(char16_t* text) {
    auto s = narrow(static_cast<const char16_t*>(text));
    ev("SetClientDrawText", {static_cast<int64_t>(lab(this))}, {s.c_str()});
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    return kUnknown;
}

void ResearchSpeed(int32_t group) { ev("ResearchSpeed", {group}); }
void PercentComplete(int32_t group) { ev("PercentComplete", {group}); }

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list ap;
    va_start(ap, id);
    TbUniStringVTag* a = va_arg(ap, TbUniStringVTag*);
    TbIntegerVTag* b = va_arg(ap, TbIntegerVTag*);
    va_end(ap);
    ev("FormatString", {lab(this), static_cast<int64_t>(size), static_cast<int64_t>(id), static_cast<int64_t>(a->mMagic),
                        static_cast<int64_t>(a->mId), static_cast<int64_t>(b->mMagic), static_cast<int64_t>(b->mId),
                        static_cast<int64_t>(b->mValue), a->mText[0], a->mText[1], a->mText[2]});
    out[0] = 0x46;
    out[1] = static_cast<char16_t>(0x30 + (b->mValue & 7));
    out[2] = 0;
    return 0;
}

extern "C" {

uint64_t tpw_nw_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},          {"panel", &gResearchPanel},        {"table", &gText.mResourcePointers[6]},
        {"ridedb", &gRideDatabase},      {"labptr", &gLabPtr},              {"parentptr", &gParentPtr},
        {"childptr", &gChildPtr},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_nw_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_nw_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>));
    return n;
}

void tpw_nw_name(int32_t group) { NameWindow(group); }
void tpw_nw_tank(int32_t group, uint64_t window, int32_t flag) {
    TankButton(group, reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), flag);
}

}  // extern "C"
