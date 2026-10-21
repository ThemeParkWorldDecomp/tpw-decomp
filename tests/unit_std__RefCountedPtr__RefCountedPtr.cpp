// Test library for unit v51035 (libtpw_unit_std__RefCountedPtr__RefCountedPtr.dylib, tests/test_std__RefCountedPtr__RefCountedPtr.py), ltms:
//   src/ltms/TbVTag.cpp
//
// Fakes: operator new, operator delete, the message destructor 0x10008a10 and swprintf, each recording what the real one reads.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "TbTextFormatter.h"
#include "TbVTag.h"

using namespace calllog;

// The real tear_down lives in src/ltms/TbTextFormatter.cpp, whose link chain is too large for this library; this copy
// uses the same operator delete fake (LengthErrorOperatorDelete) the unit's other tests record.
template <>
void MslVector<CLineInfo>::tear_down() {
    if (mData != nullptr) LengthErrorOperatorDelete(mData);
}

char gLengthErrorVtbl[16];
char gLogicErrorVtbl[16];
char gExceptionVtbl[16];

namespace {
uintptr_t gObj = 0;

int64_t VtClass(const void* p) {
    if (p == gLogicErrorVtbl) return 1;
    if (p == gExceptionVtbl) return 2;
    if (p == gLengthErrorVtbl) return 3;
    return 0;
}

class FakeBuffer : public CStaticBuffer {
public:
    void Close() override {}
    void AppendMB(const unsigned char*, uint32_t) override { ev("unexpected", {1}); }
    void AppendMB(uint16_t) override { ev("unexpected", {2}); }
    void AppendUnicode(const char16_t* text, uint32_t n) override {
        int64_t c[4] = {0, 0, 0, 0};
        for (uint32_t i = 0; i < 4 && i < n; i++) c[i] = text[i];   // only the n characters written
        ev("append", {lab(this), n, c[0], c[1], c[2], c[3]});
    }
    void AppendUnicode(char16_t) override { ev("unexpected", {3}); }
};
}  // namespace

void* LengthErrorOperatorNew(uint32_t size) {
    ev("new", {size});
    return tpw::ArenaAllocZeroed(size);
}

void LengthErrorOperatorDelete(void* block) {
    ev("delete", {lab(block)});
}

void MslRefCountedCharsDtor(MslRefCountedCharsBlock* self, int16_t flag) {
    LengthError* owner = reinterpret_cast<LengthError*>(reinterpret_cast<char*>(self) - 8);
    int64_t off = static_cast<int64_t>(reinterpret_cast<uintptr_t>(self) - gObj);
    if (off == 8) off = 4;   // the Mac's message member is at +4, the host's at +8
    ev("rcdtor", {off, flag, VtClass(owner->mVtable)});
}

int32_t swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    va_list ap;
    va_start(ap, format);
    // the one variadic argument is a double (the Mac's r5:r6 pair, which the PPC fake reads as r6 and r7): its high and low words
    const double value = va_arg(ap, double);
    va_end(ap);
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    uint32_t a = static_cast<uint32_t>(bits >> 32);
    uint32_t b = static_cast<uint32_t>(bits);
    int64_t n = ret("swprintf");
    int64_t f[6] = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 6 && format[i] != 0; i++) f[i] = format[i];   // only the text up to the terminator
    ev("swprintf", {size, f[0], f[1], f[2], f[3], f[4], f[5], a, b});
    for (int64_t i = 0; i < n; i++) buffer[i] = static_cast<char16_t>(0x41 + i + (a & 7));
    buffer[n] = 0;
    return static_cast<int32_t>(n);
}

extern "C" {

uint64_t tpw_rc_addr(const char*) { return 0; }

uint64_t tpw_rc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_rc_new_out() {
    void* p = tpw::ArenaAllocZeroed(sizeof(FakeBuffer));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new (p) FakeBuffer()));
}

int32_t tpw_rc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MslVector<CLineInfo>, mData));
    out[n++] = static_cast<int64_t>(sizeof(MslVector<CLineInfo>));
    out[n++] = static_cast<int64_t>(sizeof(MslRefCountedCharsBlock));
    out[n++] = static_cast<int64_t>(offsetof(LengthError, mMessage));
    return n;
}

void tpw_rc_ctor(uint64_t obj, uint64_t p, int64_t* out) {
    gObj = static_cast<uintptr_t>(obj);
    auto* b = reinterpret_cast<MslRefCountedCharsBlock*>(obj);
    auto* r = MslRefCountedCharsBlockCtor(b, reinterpret_cast<char*>(static_cast<uintptr_t>(p)));
    out[0] = r == b;
    out[1] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(b->mPtr));
    out[2] = b->mCount != nullptr;
    out[3] = b->mCount ? *b->mCount : -1;
}

void tpw_rc_tear(uint32_t cap, uint32_t size, uint64_t data, int64_t* out) {
    MslVector<CLineInfo> v;
    v.mCapacity = cap;
    v.mSize = size;
    v.mData = reinterpret_cast<CLineInfo*>(static_cast<uintptr_t>(data));
    v.tear_down();
    out[0] = v.mCapacity;
    out[1] = v.mSize;
    out[2] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(v.mData));
}

void tpw_rc_dtor(uint64_t obj, int32_t flag, int64_t* out) {
    gObj = static_cast<uintptr_t>(obj);
    auto* e = reinterpret_cast<LengthError*>(obj);
    if (e) e->mVtable = nullptr;
    LengthError* r = LogicErrorDtor(e, static_cast<int16_t>(flag));
    out[0] = r == e;
    out[1] = e ? VtClass(e->mVtable) : 0;
}

int32_t tpw_rc_format(uint32_t a, uint32_t b, uint64_t out) {
    TbDoubleVTag tag(0, a, b);
    TbVarFormat fmt{};
    return tag.Format(*reinterpret_cast<FakeBuffer*>(out), fmt);
}

}  // extern "C"
