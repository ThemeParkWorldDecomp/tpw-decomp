// std::ios_base::setstate 0x102077c0 (unit v5523).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "std_basic_istream.h"

// 0x10250098: the type string __throw is given; 0x1024fde4: the message text.
static const char kSetstateType[] = "!std::exception!!std::ios_base::failure!!";
static const char kSetstateMessage[] = "ios_base failure in clear";

// Adds the bits; a missing stream buffer adds badbit (1). When a bit is also in the exception mask, throws an
// ios_base::failure built on the stack: vtable, a copy of the message, a reference count of 1.
void MslIosSetstate(MacIosBase* ios, int32_t bits) {
    ios->mState = ios->mState | static_cast<uint32_t>(bits);
    if (ios->mStreamBuf.get() == nullptr) ios->mState = ios->mState | 1;
    if ((ios->mState & ios->mExceptions) != 0) {
        MslThrowObject object;
        object.mVtbl = gIosFailureVtbl;
        object.mMessage = new char[std::strlen(kSetstateMessage) + 1];
        object.mCount = nullptr;
        if (object.mMessage != nullptr) {
            object.mCount = new int32_t;
            if (object.mCount != nullptr) *object.mCount = 1;
        }
        std::strcpy(object.mMessage, kSetstateMessage);
        MslThrow(kSetstateType, &object, gIosFailureDtorVec);
    }
}
