// Object saved state differential library (unit w229). Outside callees are recording fakes.
#include "ObjectSerialise.h"
#include "TbTimeDiff.h"
#include "calllog.h"
#include <algorithm>
#include <cstdlib>
#include <string>

using namespace calllog;
CSaveLoad gSaveLoad;
namespace {
uint8_t* gObject;
std::string gFile;
uint32_t gPosition;
char gHandle;
int32_t gCalls, gFailAt, gFailValue, gBase, gError, gCount, gPartial, gTimeError;
int32_t gCalendar[8];
int32_t gEnumN, gEnumFailure;
uintptr_t gCalendarBase, gFloat;
int32_t gInitWords[2];
char16_t gInitText[8];

std::string Hex(const void* data, uint32_t n) {
    constexpr char digits[] = "0123456789abcdef";
    std::string s;
    const auto* p = static_cast<const uint8_t*>(data);
    for (uint32_t i = 0; i < n; ++i) {
        s += digits[p[i] >> 4];
        s += digits[p[i] & 15];
    }
    return s;
}
int32_t Result() {
    return gCalls++ == gFailAt ? gFailValue : 0;
}
int64_t Destination(const void* p, const char* name) {
    const auto a = reinterpret_cast<uintptr_t>(p);
    const auto b = reinterpret_cast<uintptr_t>(gObject);
    if (a >= b && a < b + sizeof(CObject)) return a - b;
    if (std::strcmp(name, "tv[t]") == 0) {
        if (gCalendarBase == 0) gCalendarBase = a;
        return -1000 + static_cast<int64_t>(a - gCalendarBase);
    }
    if (std::strcmp(name, "pv") == 0) {
        if (gFloat == 0) gFloat = a;
        return a == gFloat ? -2000 : -2001;
    }
    return -3000;
}
template<class V> int32_t Scalar(const char* kind, void* file, V* value, const char* name, int32_t noLog, bool save) {
    const std::string event = std::string(save ? "S:" : "L:") + kind;
    ev(event.c_str(), {lab(file), Destination(value, name), noLog}, {name, save ? Hex(value, sizeof(V)).c_str() : ""});
    if (save) {
        gFile.append(reinterpret_cast<const char*>(value), sizeof(V));
        gPosition += sizeof(V);
    } else {
        const uint32_t n = gCalls == gFailAt ? std::min<uint32_t>(gPartial, sizeof(V)) : sizeof(V);
        std::memcpy(value, gFile.data() + gPosition, n);
        gPosition += sizeof(V);
    }
    return Result();
}
int32_t History(void* object, const char* kind, void* file, int32_t save, int32_t version, uint32_t unused) {
    const auto offset = static_cast<uint8_t*>(object) - gObject;
    ev(kind, {offset, lab(file), save, version, unused});
    auto* p = static_cast<uint8_t*>(object);
    for (uint32_t i = 0; i < 0x88; ++i) p[i] ^= 0x5a;
    return Result();
}
}

#define SCALAR(Type, Kind) \
template<> int32_t LOADVAR<Type>(void* f, Type* v, const char* n, int32_t l) { return Scalar(Kind, f, v, n, l, false); } \
template<> int32_t SAVEVAR<Type>(void* f, Type* v, const char* n, int32_t l) { return Scalar(Kind, f, v, n, l, true); }
SCALAR(int32_t, "int")
SCALAR(uint32_t, "uint")
SCALAR(MacLong, "long")
SCALAR(MacULong, "ulong")
SCALAR(uint16_t, "ushort")
SCALAR(uint8_t, "uchar")
SCALAR(char16_t, "wide")
SCALAR(float, "float")
#undef SCALAR
#define STRUCT(Tag, Kind) \
template<> int32_t LOADVARSTRUCT<tag_IDX_TYPE<Tag, uint16_t>>(void* f, tag_IDX_TYPE<Tag, uint16_t>* v, const char* n, int32_t l) { return Scalar(Kind, f, v, n, l, false); } \
template<> int32_t SAVEVARSTRUCT<tag_IDX_TYPE<Tag, uint16_t>>(void* f, tag_IDX_TYPE<Tag, uint16_t>* v, const char* n, int32_t l) { return Scalar(Kind, f, v, n, l, true); }
STRUCT(IDXTYPEUniqueNameObjectId, "ObjectId")
STRUCT(IDXTYPEUniqueNameMapId, "MapId")
STRUCT(IDXTYPEUniqueNameThingId, "ThingId")
#undef STRUCT
template<> int32_t CHistory<uint32_t, 30>::Serialise(void* f, int32_t s, int32_t v, uint32_t u) {
    return History(this, "History<uint>", f, s, v, u);
}
template<> int32_t CHistory<int32_t, 30>::Serialise(void* f, int32_t s, int32_t v, uint32_t u) {
    return History(this, "History<int>", f, s, v, u);
}
int32_t CThing::Serialise(void* f, int32_t s, int32_t v, uint32_t u) {
    ev("Base", {static_cast<uint8_t*>(static_cast<void*>(this)) - gObject, lab(f), s, v, u});
    auto* p = reinterpret_cast<uint8_t*>(this);
    for (uint32_t i = 0; i < 12; ++i) p[i] ^= 0xa5;
    return gBase;
}
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("Log", {lab(this), size, state}, {name});
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("Assert", {condition}, {format});
}
TbError LbFile_Read(void* file, void* dest, uint32_t size, uint32_t* transferred) {
    const int32_t fail = Result();
    const uint32_t count = fail != 0 ? 3 : gCount;
    const uint32_t n = std::min(count, size);
    gEnumN = n;
    gEnumFailure = fail != 0 || gError < 0 || count != 4;
    ev("Read", {lab(file), Destination(dest, "mState"), size, *transferred, count}, {Hex(gFile.data() + gPosition, n).c_str()});
    std::memcpy(dest, gFile.data() + gPosition, n);
    gPosition += n;
    *transferred = count;
    return TbError(fail != 0 ? -1 : gError);
}
TbError LbFile_Write(void* file, const void* source, uint32_t size, uint32_t* transferred) {
    const int32_t fail = Result();
    const uint32_t count = fail != 0 ? 3 : gCount;
    const uint32_t n = std::min(count, size);
    ev("Write", {lab(file), size, *transferred, count}, {Hex(source, size).c_str()});
    gFile.append(static_cast<const char*>(source), n);
    gPosition += n;
    *transferred = count;
    return TbError(fail != 0 ? -1 : gError);
}
TbError TbTimeStamp::GetTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* h, int32_t* m, int32_t* s, int32_t* ms) const {
    int32_t* p[] = {a, b, c, d, h, m, s, ms};
    gCalendarBase = reinterpret_cast<uintptr_t>(a);
    ev("GetTime", {reinterpret_cast<const uint8_t*>(this) - gObject,
        b - a, c - a, d - a, h - a, m - a, s - a, ms - a});
    ev("Calendar", {gCalendar[0], gCalendar[1], gCalendar[2], gCalendar[3], gCalendar[4], gCalendar[5], gCalendar[6], gCalendar[7]});
    for (int32_t i = 0; i < 8; ++i) *p[i] = gCalendar[i];
    auto* stamp = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(this));
    for (int32_t i = 0; i < 8; ++i) stamp[i] ^= 0x3c;
    return TbError(gTimeError);
}
TbError TbTimeStamp::SetTime(int32_t y, int32_t mo, int32_t d, int32_t h, int32_t mi, int32_t s, int32_t ms) {
    ev("SetTime", {reinterpret_cast<uint8_t*>(this) - gObject, y, mo, d, h, mi, s, ms});
    uint32_t words[] = {static_cast<uint32_t>(y) ^ static_cast<uint32_t>(h) ^ static_cast<uint32_t>(ms),
        static_cast<uint32_t>(mo) ^ static_cast<uint32_t>(d) ^ static_cast<uint32_t>(mi) ^ static_cast<uint32_t>(s)};
    std::memcpy(this, words, 8);
    return TbError(gTimeError);
}

// The string import and its virtual slots are stand-ins; construction captures the
// values seen during the host runtime's one-time static initialization.
template<> TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    gInitWords[0] = gObjectStaticC384;
    gInitWords[1] = gObjectStaticC380;
    std::memcpy(gInitText, text, 16);
    mLength = 7;
    mCapacity = 8;
    mGrowBy = 16;
    mData = static_cast<char16_t*>(std::malloc(16));
    std::memcpy(mData, text, 16);
}
template<> TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() { std::free(mData); }
template<> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template<> void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template<> void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template<> TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }

extern "C" {
uint64_t tpw_osr_addr(int32_t which) {
    const void* p[] = {&gHandle, &gSaveLoad, &gObjectStaticC384, &gObjectStaticC380, &gObjectStaticC360};
    return reinterpret_cast<uintptr_t>(p[which]);
}
void tpw_osr_reset(uint64_t object, const char* bytes, uint32_t length, const int32_t* config, const int32_t* calendar) {
    gObject = reinterpret_cast<uint8_t*>(object);
    gFile.assign(bytes, length);
    gPosition = 0;
    gCalls = 0;
    gFailAt = config[0];
    gFailValue = config[1];
    gBase = config[2];
    gError = config[3];
    gCount = config[4];
    gPartial = config[5];
    gTimeError = config[6];
    std::memcpy(gCalendar, calendar, sizeof(gCalendar));
    gCalendarBase = gFloat = 0;
    gEnumN = -1;
    gEnumFailure = 0;
}
uint32_t tpw_osr_call(uint64_t object, int32_t save, int32_t version, uint32_t unused) {
    return reinterpret_cast<CObject*>(object)->Serialise(&gHandle, save, version, unused);
}
uint32_t tpw_osr_enum(uint64_t object, int32_t save, int32_t noLog) {
    auto* value = reinterpret_cast<CObject::ObjectState*>(object + 0x198);
    return save ? SAVEVAR(&gHandle, value, "state-name", noLog) : LOADVAR(&gHandle, value, "state-name", noLog);
}
uint32_t tpw_osr_file(char* bytes, uint32_t* position) {
    *position = gPosition;
    std::memcpy(bytes, gFile.data(), gFile.size());
    return gFile.size();
}
int32_t tpw_osr_enum_read(int32_t* failed) {
    *failed = gEnumFailure;
    return gEnumN;
}
int32_t tpw_osr_calls() { return gCalls; }
void tpw_osr_init(int32_t* words, char16_t* text) {
    words[0] = gInitWords[0];
    words[1] = gInitWords[1];
    words[2] = gObjectStaticC384;
    words[3] = gObjectStaticC380;
    words[4] = gObjectStaticC360.mLength;
    std::memcpy(text, gObjectStaticC360.mData, 16);
    std::memcpy(text + 8, gInitText, 16);
}
int32_t tpw_osr_layout(int64_t* out) {
    const int64_t v[] = {sizeof(CObject), offsetof(CObject, mRotation), offsetof(CObject, mBuilt), offsetof(CObject, mHistoryF4),
        offsetof(CObject, mState), offsetof(CObject, mHistory33C), offsetof(CObject, mSignA), offsetof(CObject, mSignB)};
    std::memcpy(out, v, sizeof(v));
    return 8;
}
}
