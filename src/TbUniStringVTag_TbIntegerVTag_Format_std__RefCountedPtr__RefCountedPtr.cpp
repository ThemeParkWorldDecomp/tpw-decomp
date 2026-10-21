// Unit v51035 (ltms): std::_RefCountedPtr<char>::_RefCountedPtr(char*) 0x10008adc, std::vector<CLineInfo>::tear_down 0x1000839c,
// std::logic_error::~logic_error 0x100081c4, TbDoubleVTag::Format 0x10009408.
#include <cstring>

#include "TbVTag.h"
#include "platform/msl.h"
#include "TbTextFormatter.h"

MslRefCountedCharsBlock* MslRefCountedCharsBlockCtor(MslRefCountedCharsBlock* self, char* p) {
    self->mPtr = p;
    self->mCount = nullptr;
    if (self->mPtr != nullptr) {
        int32_t* counter = static_cast<int32_t*>(LengthErrorOperatorNew(4));
        if (counter != nullptr) *counter = 1;
        self->mCount = counter;
    }
    return self;
}

// MslVector<CLineInfo>::tear_down (0x1000839c) lives in src/ltms_textformatter.cpp.

LengthError* LogicErrorDtor(LengthError* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mVtable = gLogicErrorVtbl;
        MslRefCountedCharsDtor(&self->mMessage, -1);
        self->mVtable = gExceptionVtbl;
        if (deleteFlag > 0) LengthErrorOperatorDelete(self);
    }
    return self;
}

// The string at 0x1000fc8e + 6.
static const char16_t kDoubleFormat[] = u"%f";

// The Mac passes the two words in the register pair a variadic double takes (r5:r6, high word first), so "%f" shows the double whose
// bits they are; the host passes that double itself (two ints would not reach a double's va_arg).
int32_t TbDoubleVTag::Format(CStaticBuffer& out, TbVarFormat&) {
    char16_t buffer[0x11];
    const uint64_t bits = (static_cast<uint64_t>(mFirst) << 32) | mSecond;
    double value;
    std::memcpy(&value, &bits, sizeof value);
    int32_t length = swprintf(buffer, 0x11, kDoubleFormat, value);
    out.AppendUnicode(buffer, static_cast<uint32_t>(length));
    return 0;
}
