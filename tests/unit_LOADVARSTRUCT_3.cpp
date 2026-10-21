// Test library for the action history load, the tag table lookup and the tag system's message receiver (unit v5960;
// libtpw_unit_LOADVARSTRUCT_3.dylib, tests/test_LOADVARSTRUCT_3.py), main app:
//   src/MsgDeath_CThoughts_Serialise_LOADVARSTRUCT_3.cpp   LOADVARSTRUCT<CThoughts::EventHistType>, CTagTable::GetElement,
//   CTagSystem::ReceiveMessage; it defines gTagTable and gTbErrorZero.
//
// The callees outside the unit are fakes that record what the real ones read: WriteSerialiseLog, LbFile_Read, GAME_ASSERTMSG, CTRACE,
// TRACE, UDebug, FormatTagText, UIInterface::MessageTag and the messages' GetType. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "CMsgDeath.h"
#include "ThingReceivers.h"
#include "ThingMessages.h"
#include "Serialise.h"
#include "SaveLoad.h"
#include "LbFile.h"
#include "Debug.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "platform/samsutils.h"
#include "image_copies.h"

// The tables as loaded, for the image test (unit v5L39).
TPW_IMAGE_COPY(tags, gTagTable);

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CSaveLoad gSaveLoad;
UIInterface gUIInterface;

static uint64_t gTestThis;

// ---- the messages: test subclasses answering the case's type
CMsgChallenge::~CMsgChallenge() {}
int32_t CMsgChallenge::GetType() const { return 0x19; }

namespace {

template<class B> struct Fake : B {
    int32_t GetType() const override {
        ev("GetType", {0});
        return static_cast<int32_t>(ret("type"));
    }
};

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("WriteSerialiseLog", {lab(this), size, static_cast<int32_t>(state)}, {name});
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)), length});
    uint32_t data = static_cast<uint32_t>(ret("data"));
    uint8_t* bytes = static_cast<uint8_t*>(buffer);
    bytes[0] = static_cast<uint8_t>(data >> 24);
    bytes[1] = static_cast<uint8_t>(data >> 16);
    bytes[2] = static_cast<uint8_t>(data >> 8);
    bytes[3] = static_cast<uint8_t>(data);
    *transferred = static_cast<uint32_t>(ret("count"));
    return TbError(static_cast<int32_t>(ret("result")));
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t id = va_arg(args, int32_t);
    va_end(args);
    ev("CTRACE", {level, id}, {format});
}

void TRACE(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t type = va_arg(args, int32_t);
    va_end(args);
    ev("TRACE", {type}, {format});
}

namespace SamsUtilities {
void UDebug(const char* format, ...) { ev("UDebug", {}, {format}); }
}  // namespace SamsUtilities

int32_t CTagSystem::FormatTagText(const TagTableElement* element, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing, uint32_t value,
                                  const CMsgChallenge* challenge) {
    int64_t index = element ? (reinterpret_cast<const uint8_t*>(element) - reinterpret_cast<const uint8_t*>(gTagTable)) / 0x20 : -1;
    int64_t self = reinterpret_cast<uint64_t>(this) == gTestThis ? 1 : -1;
    ev("FormatTagText", {self, index, thing.mIndex, value, challenge ? 1 : 0});
    return 1;
}

void UIInterface::MessageTag(CTagParams& p) {
    int64_t text = p.mText ? static_cast<int64_t>(reinterpret_cast<uint64_t>(p.mText) - gTestThis) : -9999;
    ev("MessageTag", {lab(this), p.mId, p.mF4, text, p.mTime, p.mFrame, p.mF14, p.mF18, p.mF1C, p.mF20, p.mShow});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_x_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"table", &gTagTable[0]},
        {"tbzero", &gTbErrorZero},
        {"saveload", &gSaveLoad},
        {"ui", &gUIInterface},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_x_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_x_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TagTableElement));
    out[n++] = static_cast<int64_t>(sizeof(gTagTable));
    out[n++] = static_cast<int64_t>(sizeof(CThoughts::EventHistType));
    out[n++] = static_cast<int64_t>(sizeof(CTagSystem));
    out[n++] = static_cast<int64_t>(offsetof(CTagSystem, mF0C));
    return n;
}

// A test message: kind 0 CMsgTagShow, 1 CMsgTagSimple, 2 CMsgChallenge.
uint64_t tpw_x_msg(int32_t kind, int32_t a, uint32_t thing, int32_t time, uint32_t value) {
    if (kind == 0) {
        auto* m = new (tpw::ArenaAllocZeroed(sizeof(Fake<CMsgTagShow>))) Fake<CMsgTagShow>();
        m->mTag = a;
        m->mThing = static_cast<uint16_t>(thing);
        m->mTime = time;
        m->mValue = value;
        return reinterpret_cast<uintptr_t>(static_cast<CMsg*>(m));
    }
    if (kind == 1) {
        auto* m = new (tpw::ArenaAllocZeroed(sizeof(Fake<CMsgTagSimple>))) Fake<CMsgTagSimple>();
        m->mCode = a;
        return reinterpret_cast<uintptr_t>(static_cast<CMsg*>(m));
    }
    auto* m = new (tpw::ArenaAllocZeroed(sizeof(Fake<CMsgChallenge>))) Fake<CMsgChallenge>();
    m->mTagId = a;
    return reinterpret_cast<uintptr_t>(static_cast<CMsg*>(m));
}

// ---- the functions, one wrapper each with C types only
int32_t tpw_x_load(uint64_t file, uint64_t buffer, uint64_t name, int32_t noLog) {
    return LOADVARSTRUCT<CThoughts::EventHistType>(reinterpret_cast<void*>(file), reinterpret_cast<CThoughts::EventHistType*>(buffer),
                                                   reinterpret_cast<const char*>(name), noLog);
}

uint64_t tpw_x_get(int32_t id) {
    static const CTagTable table;
    return reinterpret_cast<uintptr_t>(table.GetElement(id));
}

void tpw_x_recv(uint64_t self, uint64_t message) {
    gTestThis = self;
    reinterpret_cast<CTagSystem*>(self)->ReceiveMessage(*reinterpret_cast<const CMsg*>(message));
}

}  // extern "C"
