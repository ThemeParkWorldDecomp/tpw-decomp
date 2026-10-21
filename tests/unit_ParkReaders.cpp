// Differential library for unit w125, the frame readers of the things: CParkAnalyser::HowBusyIsPark, NumberOfVisitorsInPark, CountNeighboursOfType
// (src/game/park/analyser.cpp) and InterfaceNumberTextObject::Change, InterfaceCashAndGTickTextObject::Change (src/game/ui/InterfaceTextObjectSprite.cpp).
//
// Every callee is a fake that records the same event as the PowerPC side's stub: GAME_ASSERTMSG (the condition and the text), the world accessor (its
// receiver), CWorld::GetUsedThingHead and GetNextUsedThing (the world, and the thing), CPeep::IsInsidePark, CParkAnalyserVal<int>::Get and Set,
// CParkAnalyser::GetVisitorsInPark, InterfaceTextObjectSprite::Change (flags, the change and, for flag 0x100, the text), swprintf (format, size and the
// numbers) and TbITable::FormatString (table, size, id and the integer tag). The answers come from queues.
//
// The world is NOT a block of its Mac size: only the 0x80 bytes at world + 0x1da6f0 are real memory (gTail), and the world pointer is that block's address
// minus 0x1da6f0, on both sides. The things are small raw blocks (gThings) whose fields are set through the host's member names; the thing table
// (gThingArray), the map (gCells, 0x44 bytes a cell) and the text object's string table pointer are the unit's own globals. Pointers are reported as call-log labels.
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "calllog.h"
#include "ThingParts2.h"
#include "ui_DrawObject.h"
#include "World.h"
#include "WorldSavvy.h"
#include "analyser.h"
#include "Debug.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "platform/msl.h"

using namespace calllog;

// only a place for the string table's pointer (the symbol of gText, so that the library does not need the text object's fonts)
alignas(16) char gTextBlock[sizeof(CText)] __asm__("_gText");

namespace {

constexpr int32_t kTail = 0x1da6f0;
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kAnalyserSize = 0x1f0b8 + 0x40;
constexpr int32_t kCellCount = 0x4000;
constexpr int32_t kThingSlots = 8;

alignas(16) unsigned char gTail[kTailSize];
alignas(16) unsigned char gAnalyser[kAnalyserSize];
alignas(16) unsigned char gThings[kThingSlots][0x400];
alignas(16) unsigned char gCells[kCellCount * 0x44];
alignas(16) unsigned char gTableBlock[0x40];
alignas(16) unsigned char gObject[0x400];       // the text object (never touched: only its address is reported)
alignas(16) DOChange gChange;                   // the change a text object is given (its style holds the text "pass", for the flags that go on to the sprite unchanged)
DOTextStyle gStyle;
char16_t gPassText[] = u"pass";

CWorld* WorldPtr() { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail) - kTail); }
CText& Text() { return *reinterpret_cast<CText*>(gTextBlock); }
CParkAnalyser* Analyser() { return reinterpret_cast<CParkAnalyser*>(gAnalyser); }

}  // namespace

// ---- the unit's globals (the launcher gets zero definitions)
CThingArrayEntry gThingArray[kThingArraySize];
CMap* gWorldMap;

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {lab(this)});
    return *WorldPtr();
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetUsedThingHead() const {
    ev("GetUsedThingHead", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("head", 0));
    return result;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::GetNextUsedThing(const CThing* thing) const {
    ev("GetNextUsedThing", {lab(this), lab(thing)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = static_cast<uint16_t>(ret("next", 0));
    return result;
}

int32_t CPeep::IsInsidePark() const {
    ev("IsInsidePark", {lab(this)});
    return static_cast<int32_t>(ret("inside", 0));
}

template <typename T>
T CParkAnalyserVal<T>::Get() const {
    ev("ValGet", {lab(this)});
    return static_cast<T>(ret("get", 0));
}
template <typename T>
void CParkAnalyserVal<T>::Set(T value) {
    ev("ValSet", {lab(this), static_cast<int64_t>(value)});
}
template int CParkAnalyserVal<int>::Get() const;
template void CParkAnalyserVal<int>::Set(int);

int32_t CParkAnalyser::GetVisitorsInPark(uint32_t arg) const {
    ev("GetVisitorsInPark", {lab(this), static_cast<int64_t>(arg)});
    return static_cast<int32_t>(ret("gvip", 0));
}

// The sprite's Change: the object, the flags, the change's label and, for the text flag, the text the change's style points at.
void InterfaceTextObjectSprite::Change(uint32_t flags, DOChange* change) {
    if (flags == 0x100) ev("SpriteChange", {lab(this), flags, lab(change)}, {narrow(change->mText->mText).c_str()});
    else ev("SpriteChange", {lab(this), flags, lab(change)});
}

// swprintf: the buffer size, the format and the number: an int for a "%d" format, otherwise a double. The text written is "n" + the number as hex digits,
// so a change of the number shows in the next event.
int32_t swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    va_list args;
    va_start(args, format);
    char16_t text[40];
    uint32_t length = 0;
    if (format[0] == u'%' && format[1] == u'd') {
        const int32_t value = va_arg(args, int);
        ev("swprintf", {size, value}, {narrow(format).c_str()});
        static const char kHex[] = "0123456789abcdef";
        text[length++] = u'n';
        for (int s = 28; s >= 0; s -= 4) text[length++] = static_cast<char16_t>(kHex[(static_cast<uint32_t>(value) >> s) & 15]);
    } else {
        const double value = va_arg(args, double);
        uint64_t bits;
        std::memcpy(&bits, &value, 8);
        ev("swprintf", {size, static_cast<int64_t>(bits)}, {narrow(format).c_str()});
        static const char kHex[] = "0123456789abcdef";
        text[length++] = u'f';
        for (int s = 60; s >= 0; s -= 4) text[length++] = static_cast<char16_t>(kHex[(bits >> s) & 15]);
    }
    va_end(args);
    text[length] = 0;
    if (size != 0) {
        const uint32_t kept = length < size - 1 ? length : size - 1;
        for (uint32_t i = 0; i < kept; ++i) buffer[i] = text[i];
        buffer[kept] = 0;
    }
    return static_cast<int32_t>(length);
}

// The string table's FormatString: the table, the size, the id and the integer tag it is given (magic word, the tag's id, its value).
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    TbVTag* tag = va_arg(args, TbVTag*);
    va_end(args);
    ev("FormatString", {lab(this), size, id, static_cast<int64_t>(tag->mMagic), static_cast<int64_t>(tag->mId),
                        static_cast<int64_t>(static_cast<TbIntegerVTag*>(tag)->mValue)});
    static const char kHex[] = "0123456789abcdef";
    char16_t text[12];
    uint32_t length = 0;
    text[length++] = u's';
    const uint32_t value = static_cast<uint32_t>(static_cast<TbIntegerVTag*>(tag)->mValue);
    for (int s = 28; s >= 0; s -= 4) text[length++] = static_cast<char16_t>(kHex[(value >> s) & 15]);
    for (uint32_t i = 0; i < length && i + 1 < size; ++i) out[i] = text[i];
    out[length < size ? length : size - 1] = 0;
    return static_cast<int32_t>(length);
}

// The integer tag's own Format is never called here (it is only needed to make the vtable).
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

// ---- the entries of the test
extern "C" {
void* tpw_pr_addr(int32_t which) {
    switch (which) {
        case 0: return gAnalyser;
        case 1: return WorldPtr();
        case 2: return gTableBlock;
        case 3: return gObject;
        case 4: return &gChange;
        case 5: return gTail;
        case 6: return gCells;
        default: return gThings[which - 10];   // 10 .. 17
    }
}
void tpw_pr_turn(uint32_t turn) { std::memcpy(gTail + 0x1c, &turn, 4); }
void tpw_pr_val(int32_t worked, uint32_t timeSig, int32_t value) {
    CParkAnalyser* a = Analyser();
    a->mVal1F0B8.mWorkedOut = worked != 0;
    std::memcpy(reinterpret_cast<unsigned char*>(&a->mVal1F0B8.mWorkedOut), &worked, 1);
    a->mVal1F0B8.mTimeSig = timeSig;
    a->mVal1F0B8.mVal = value;
}
void tpw_pr_row(int32_t id, int32_t slotPlusOne) {
    gThingArray[id].mThing = slotPlusOne != 0 ? static_cast<void*>(gThings[slotPlusOne - 1]) : nullptr;
}
void tpw_pr_thing(int32_t slot, int32_t kind, int32_t next) {
    CThing* t = reinterpret_cast<CThing*>(gThings[slot]);
    t->mKind = static_cast<uint8_t>(kind);
    t->mCellNext = static_cast<uint16_t>(next);
}
void tpw_pr_cells_clear() { std::memset(gCells, 0, sizeof gCells); }
void tpw_pr_cell(int32_t index, int32_t who) { reinterpret_cast<CMapCell*>(gCells + index * 0x44)->mWho = static_cast<uint16_t>(who); }
void tpw_pr_globals(int32_t table) {
    gWorldMap = reinterpret_cast<CMap*>(gCells);
    Text().mResourcePointers[6] = table != 0 ? static_cast<void*>(gTableBlock) : nullptr;
}
int32_t tpw_pr_howbusy() { return Analyser()->HowBusyIsPark(); }
int32_t tpw_pr_visitors() { return Analyser()->NumberOfVisitorsInPark(); }
int32_t tpw_pr_count(int32_t model, uint32_t id, int32_t a, int32_t b) {
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> map;
    map.mIndex = static_cast<uint16_t>(id);
    return Analyser()->CountNeighboursOfType(static_cast<CThing::ThingModel>(model), map, a, b);
}
void tpw_pr_change(int32_t kind, uint32_t flags, int32_t value) {
    std::memset(&gChange, 0, sizeof gChange);
    std::memcpy(gChange.mF00, &value, 4);
    gStyle.mText = gPassText;
    gChange.mText = &gStyle;
    if (kind == 0) reinterpret_cast<InterfaceNumberTextObject*>(gObject)->InterfaceNumberTextObject::Change(flags, &gChange);
    else reinterpret_cast<InterfaceCashAndGTickTextObject*>(gObject)->InterfaceCashAndGTickTextObject::Change(flags, &gChange);
}
}
