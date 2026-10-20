// Test library for the tag system's text formatter (unit v5626; libtpw_unit_CTagSystem_FormatTagText.dylib,
// tests/test_CTagSystem_FormatTagText.py), main app:
//   src/ThingLoop_CTagSystem_FormatTagText.cpp   CTagSystem::FormatTagText, gTagNoThingId, gTagNoRideType
//
// The callees outside the unit are fakes that record what the real one reads and answer from the test's queues:
// TbITable::FormatString (the table, the buffer, the size, the text id, then each tag it takes: as many as the queue
// "nargs:<id>" says, the count the real one finds in the text; writes "s<id>" into the buffer), swprintf, TRACE,
// GAME_ASSERTMSG and both CRideDatabase::GetTypeName. The globals of other files the unit reads (gText, gMainBalance,
// gRideDatabase, gThingArray) are defined here, zero at start like the Mac's bss; the three objects as raw storage under
// the program's symbol, as their constructors live in files this library does not compile.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "Debug.h"
#include "MainBalance.h"
#include "TPEngine.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingMessages.h"
#include "ThingReceivers.h"
#include "World.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here (the program's definitions are in files this library does not compile)
CThingArrayEntry gThingArray[kThingArraySize];
alignas(16) unsigned char gTextStorage[sizeof(CText)] __asm__("_gText");
alignas(16) unsigned char gMainBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");
alignas(16) unsigned char gRideDatabaseStorage[sizeof(CRideDatabase)] __asm__("_gRideDatabase");

namespace {

CText& Text() { return *reinterpret_cast<CText*>(gTextStorage); }
CMainBalance& Balance() { return *reinterpret_cast<CMainBalance*>(gMainBalanceStorage); }

char16_t gNameBuffer[32];

void WriteText(char16_t* out, char tag, uint32_t id) {
    char text[24];
    std::snprintf(text, sizeof text, "%c%u", tag, id);
    size_t i = 0;
    for (; text[i] != 0; ++i) out[i] = static_cast<char16_t>(text[i]);
    out[i] = 0;
}

void RecordTag(TbVTag* tag) {
    if (auto* number = dynamic_cast<TbIntegerVTag*>(tag)) {
        ev("vtag", {1, tag->mMagic, tag->mId, number->mValue}, {""});
    } else if (auto* text = dynamic_cast<TbUniStringVTag*>(tag)) {
        if (text->mText == nullptr)
            ev("vtag", {2, tag->mMagic, tag->mId, 0}, {""});
        else
            ev("vtag", {2, tag->mMagic, tag->mId, 1}, {narrow(text->mText).c_str()});
    } else {
        ev("vtag", {-1, tag->mMagic, tag->mId, 0}, {""});
    }
}

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    int64_t count = ret("nargs", id, 0);
    ev("FormatString", {lab(this), lab(out), size, id, count});
    va_list arguments;
    va_start(arguments, id);
    for (int64_t i = 0; i < count; ++i) RecordTag(va_arg(arguments, TbVTag*));
    va_end(arguments);
    WriteText(out, 's', id);
    return 0;
}

int32_t swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    ev("swprintf", {lab(buffer), size}, {narrow(format).c_str()});
    return 0;
}

void TRACE(const char* format, ...) { ev("TRACE", {}, {format}); }

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

const char16_t* CRideDatabase::GetTypeName(uint16_t id) {
    ev("GetTypeName", {lab(this), id});
    if (ret("GetTypeName") == 0) return nullptr;
    WriteText(gNameBuffer, 'T', id);
    return gNameBuffer;
}

void CRideDatabase::GetTypeName(uint16_t id, char16_t* buffer, uint32_t size) {
    ev("GetTypeName3", {lab(this), id, size});
    WriteText(buffer, 'N', id);
}

// The tags' key functions (the real ones are in src/TbUniStringVTag*.cpp); never called, here for the vtables.
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }
int32_t TbUniStringVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_tag_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"thing0", &gThingArray[0].mThing},
        {"noThing", &gTagNoThingId},
        {"noRide", &gTagNoRideType},
        {"table560", &Text().mResourcePointers[5]},
        {"table590", &Text().mResourcePointers[17]},
        {"bal750", &Balance().mValues[(0x750 - 8) / 4].mInteger},
        {"bal754", &Balance().mValues[(0x754 - 8) / 4].mInteger},
        {"bal760", &Balance().mValues[(0x760 - 8) / 4].mInteger},
        {"bal764", &Balance().mValues[(0x764 - 8) / 4].mInteger},
        {"bal768", &Balance().mValues[(0x768 - 8) / 4].mInteger},
        {"ridedb", gRideDatabaseStorage},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_tag_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on: the first five are checked against the Mac's, the challenge's are the host's own.
int32_t tpw_tag_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CTagSystem, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(TagTableElement, mTextId));
    out[n++] = static_cast<int64_t>(offsetof(TagTableElement, mFormat));
    out[n++] = static_cast<int64_t>(sizeof(CTagSystem));
    out[n++] = static_cast<int64_t>(offsetof(CThingArrayEntry, mThing));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(sizeof(CMsgChallenge));
    out[n++] = static_cast<int64_t>(offsetof(CMsgChallenge, mPrize));
    out[n++] = static_cast<int64_t>(offsetof(CMsgChallenge, mTargetTime));
    out[n++] = static_cast<int64_t>(offsetof(CMsgChallenge, mTargetVal));
    out[n++] = static_cast<int64_t>(offsetof(CMsgChallenge, mDays));
    out[n++] = static_cast<int64_t>(offsetof(CMsgChallenge, mValue));
    out[n++] = static_cast<int64_t>(offsetof(CMsgChallenge, mObj));
    out[n++] = static_cast<int64_t>(offsetof(CMsgChallenge, mObj2));
    out[n++] = static_cast<int64_t>(offsetof(CMsgChallenge, mStaff));
    return n;
}

// ---- the function: the thing id as its 16 bits
uint32_t tpw_tag_format(uint64_t self, uint64_t element, uint32_t thing, uint32_t value, uint64_t challenge) {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(thing);
    return static_cast<uint32_t>(reinterpret_cast<CTagSystem*>(static_cast<uintptr_t>(self))
                                     ->FormatTagText(reinterpret_cast<const TagTableElement*>(static_cast<uintptr_t>(element)), id, value,
                                                     reinterpret_cast<const CMsgChallenge*>(static_cast<uintptr_t>(challenge))));
}

}  // extern "C"
