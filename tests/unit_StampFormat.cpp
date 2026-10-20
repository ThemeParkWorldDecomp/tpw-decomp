// Unit w217: recording callees and string doubles for the bullfrog date formatter.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include "TbTimeDiff.h"
#include "TbDynamicStringTemplate.h"
#include "platform/macdoze.h"
#include "calllog.h"

using namespace calllog;
namespace {
NS_MacDoze::_SYSTEMTIME calendar;
std::string replacement;
bool replaceName;
const void* objects[3];
int32_t initialFlags[3];
int32_t objectIndex(const void* p) {
    for (int i = 0; i < 3; ++i) if (objects[i] == p) return i;
    return -1;
}
// Log every code unit as four hex digits, including non-ASCII and sign-extended bytes.
template<class Char> std::string hexText(const Char* text) {
    std::string out;
    const char* hex = "0123456789abcdef";
    while (*text != 0) {
        uint16_t value = sizeof(Char) == 1 ? static_cast<unsigned char>(*text++) : static_cast<uint16_t>(*text++);
        for (int shift = 12; shift >= 0; shift -= 4) out += hex[(value >> shift) & 15];
    }
    return out;
}
// Append doubles use an identical capacity rule on both processors.
template<bool Fixed, class Str, class Char> void append(Str* self, const Char* text, int slot) {
    const std::string value = hexText(text);
    ev("appendText", {objectIndex(self), slot}, {value.c_str()});
    for (; *text != 0; ++text) {
        if constexpr (Fixed) {
            if (self->mLength + 1 >= self->mCapacity) {
                self->mTruncated = 1;
                break;
            }
        }
        self->mData[self->mLength++] = *text;
        self->mData[self->mLength] = 0;
    }
}
template<bool Fixed, class Str, class Char> void appendChar(Str* self, const Char& ch, int slot) {
    const uint16_t value = sizeof(Char) == 1 ? static_cast<unsigned char>(ch) : static_cast<uint16_t>(ch);
    ev("appendChar", {objectIndex(self), slot, value});
    if (ch == 0) return;
    if constexpr (Fixed) {
        if (self->mLength + 1 >= self->mCapacity) {
            self->mTruncated = 1;
            return;
        }
    }
    self->mData[self->mLength++] = ch;
    self->mData[self->mLength] = 0;
}
}

// All out-of-unit virtuals report an unexpected call; only the two append slots are used.
template<class C> TbDynamicStringTemplate<C>::TbDynamicStringTemplate() { mGrowBy = 0; }
template<class C> TbDynamicStringTemplate<C>::~TbDynamicStringTemplate() {}
template<class C> void TbDynamicStringTemplate<C>::Format(const C*, ...) { ev("unexpected"); }
template<class C> void TbDynamicStringTemplate<C>::ResizeTo(uint32_t) { ev("unexpected"); }
template<class C> void TbDynamicStringTemplate<C>::AllocBuffer(uint32_t) { ev("unexpected"); }
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>&) {
    ev("unexpected");
    return *this;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate&) {
    ev("unexpected");
    return *this;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C*) {
    ev("unexpected");
    return *this;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C&) {
    ev("unexpected");
    return *this;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>&) {
    ev("unexpected");
    return *this;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C&) {
    ev("unexpected");
    return *this;
}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C*) {
    ev("unexpected");
    return *this;
}
template<class C> TbStaticStringTemplate<C>::TbStaticStringTemplate(C* buffer, uint32_t capacity, int32_t) {
    mData = buffer;
    mCapacity = capacity;
    mLength = 0;
    mTruncated = 0;
}
template<class C> TbStaticStringTemplate<C>::~TbStaticStringTemplate() {}
template<class C> void TbStaticStringTemplate<C>::Format(const C*, ...) { ev("unexpected"); }
template<class C> void TbStaticStringTemplate<C>::ResizeTo(uint32_t) { ev("unexpected"); }
template<class C> TbStaticStringTemplate<C>& TbStaticStringTemplate<C>::operator=(const TbStringBase<C>&) {
    ev("unexpected");
    return *this;
}
template<class C> TbStaticStringTemplate<C>& TbStaticStringTemplate<C>::operator=(const C*) {
    ev("unexpected");
    return *this;
}
template<class C> TbStaticStringTemplate<C>& TbStaticStringTemplate<C>::operator=(const C&) {
    ev("unexpected");
    return *this;
}
template<class C> TbStaticStringTemplate<C>& TbStaticStringTemplate<C>::operator+=(const TbStringBase<C>&) {
    ev("unexpected");
    return *this;
}
template<class C> TbStaticStringTemplate<C>& TbStaticStringTemplate<C>::operator+=(const C&) {
    ev("unexpected");
    return *this;
}
template<class C> TbStaticStringTemplate<C>& TbStaticStringTemplate<C>::operator+=(const C*) {
    ev("unexpected");
    return *this;
}

template<class Char> class RecStatic : public TbStaticStringTemplate<Char> {
public:
    RecStatic() : TbStaticStringTemplate<Char>(nullptr, 0, 0) {}
    TbStaticStringTemplate<Char>& operator+=(const Char& c) override {
        appendChar<true>(this, c, 0x24);
        return *this;
    }
    TbStaticStringTemplate<Char>& operator+=(const Char* s) override {
        append<true>(this, s, 0x28);
        return *this;
    }
};
template<class Char> class RecDynamic : public TbDynamicStringTemplate<Char> {
public:
    TbDynamicStringTemplate<Char>& operator+=(const Char& c) override {
        appendChar<false>(this, c, 0x2c);
        return *this;
    }
    TbDynamicStringTemplate<Char>& operator+=(const Char* s) override {
        append<false>(this, s, 0x30);
        return *this;
    }
};

// Each calendar fake records the full input, fills its output even on failure, and ignores no fields.
int32_t NS_MacDoze::FileTimeToSystemTime(const tagFILETIME* ft, _SYSTEMTIME* st) {
    ev("FileTimeToSystemTime", {ft->dwLowDateTime, ft->dwHighDateTime});
    *st = calendar;
    return static_cast<int32_t>(ret("ftres", 1));
}
namespace {
int32_t api(const char* event, uint32_t locale, uint32_t flags, const NS_MacDoze::_SYSTEMTIME* t,
            const char* picture, char* out, int32_t size) {
    ev(event, {locale, flags, t->wYear, t->wMonth, t->wDayOfWeek, t->wDay, t->wHour, t->wMinute,
               t->wSecond, t->wMilliseconds, size}, {picture});
    const char* answer = std::strcmp(picture, "dddd") == 0 ? "Wednesday" :
                         std::strcmp(picture, "ddd") == 0 ? "Wed" :
                         std::strcmp(picture, "MMMM") == 0 ? "September" :
                         std::strcmp(picture, "MMM") == 0 ? "Sep" :
                         std::strcmp(picture, "tt") == 0 ? "PM" : "P";
    if (replaceName) answer = replacement.c_str();
    const size_t n = std::min(std::strlen(answer), static_cast<size_t>(size - 1));
    std::memcpy(out, answer, n);
    out[n] = 0;
    return static_cast<int32_t>(ret("apires", 1));
}
// No data members in the doubles: pointer steps must use the base object's stride.
template<class Char, class Rec, class Base> int32_t stringRun(int wrapper, int64_t ticks,
        const Char* format, int capacity, uint16_t* out, int32_t* state) {
    static_assert(sizeof(Rec) == sizeof(Base));
    Rec strings[3];
    Char buffers[3][512];
    for (int i = 0; i < 3; ++i) {
        objects[i] = &strings[i];
        std::fill(buffers[i], buffers[i] + 512, static_cast<Char>(0x7e));
        buffers[i][0] = static_cast<Char>('A' + i);
        buffers[i][1] = 0;
        strings[i].mData = buffers[i];
        strings[i].mLength = 1;
        strings[i].mCapacity = capacity;
        // Dynamic uses the last field as grow-by, and never reads it here.
        if constexpr (std::is_same_v<Rec, RecStatic<Char>>) strings[i].mTruncated = initialFlags[i];
    }
    const TbTimeStamp stamp{ticks};
    Base* target = &strings[2];
    int32_t result;
    if (wrapper) result = stamp.Format(target, format);
    else if constexpr (std::is_same_v<Rec, RecStatic<Char>>) result = TemplatedStaticFormat(target, format, &stamp);
    else result = TemplatedDynamicFormat(target, format, &stamp);
    for (int i = 0; i < 3; ++i) {
        state[2 * i] = strings[i].mLength;
        if constexpr (std::is_same_v<Rec, RecStatic<Char>>) state[2 * i + 1] = strings[i].mTruncated;
        else state[2 * i + 1] = 0;
        for (int j = 0; j < 512; ++j) out[i * 512 + j] = sizeof(Char) == 1 ?
            static_cast<unsigned char>(buffers[i][j]) : static_cast<uint16_t>(buffers[i][j]);
    }
    return result;
}
}
int32_t NS_MacDoze::GetDateFormatA(uint32_t l, uint32_t f, const _SYSTEMTIME* t, const char* p, char* o, int32_t n) {
    return api("GetDateFormatA", l, f, t, p, o, n);
}
int32_t NS_MacDoze::GetTimeFormatA(uint32_t l, uint32_t f, const _SYSTEMTIME* t, const char* p, char* o, int32_t n) {
    return api("GetTimeFormatA", l, f, t, p, o, n);
}

extern "C" {
void tpw_sfm_config(const uint16_t* fields, const char* name, const int32_t* flags) {
    std::memcpy(&calendar, fields, 16);
    replaceName = name != nullptr;
    replacement = name ? name : "";
    std::copy(flags, flags + 3, initialFlags);
}
int32_t tpw_sfm_buffer(int64_t ticks, char* buffer, int32_t size, const char* format) {
    return TbTimeStamp{ticks}.Format(buffer, size, format);
}
void tpw_sfm_tag8(char* buffer, int32_t room, const char* format, int32_t* state) {
    char* cursor = buffer;
    const char* f = format;
    ProcessFormatTag(&cursor, &room, &f, calendar);
    state[0] = static_cast<int32_t>(cursor - buffer);
    state[1] = room;
    state[2] = static_cast<int32_t>(f - format);
}
void tpw_sfm_tag16(char16_t* buffer, int32_t room, const char16_t* format, int32_t* state) {
    char16_t* cursor = buffer;
    const char16_t* f = format;
    ProcessFormatTag(&cursor, &room, &f, calendar);
    state[0] = static_cast<int32_t>(cursor - buffer);
    state[1] = room;
    state[2] = static_cast<int32_t>(f - format);
}
int32_t tpw_sfm_static8(int wrapper, int64_t ticks, const char* format, int capacity, uint16_t* out, int32_t* state) {
    return stringRun<char, RecStatic<char>, TbStaticStringTemplate<char>>(wrapper, ticks, format, capacity, out, state);
}
int32_t tpw_sfm_static16(int wrapper, int64_t ticks, const char16_t* format, int capacity, uint16_t* out, int32_t* state) {
    return stringRun<char16_t, RecStatic<char16_t>, TbStaticStringTemplate<char16_t>>(wrapper, ticks, format, capacity, out, state);
}
int32_t tpw_sfm_dynamic8(int wrapper, int64_t ticks, const char* format, int capacity, uint16_t* out, int32_t* state) {
    return stringRun<char, RecDynamic<char>, TbDynamicStringTemplate<char>>(wrapper, ticks, format, capacity, out, state);
}
int32_t tpw_sfm_dynamic16(int wrapper, int64_t ticks, const char16_t* format, int capacity, uint16_t* out, int32_t* state) {
    return stringRun<char16_t, RecDynamic<char16_t>, TbDynamicStringTemplate<char16_t>>(wrapper, ticks, format, capacity, out, state);
}
}
