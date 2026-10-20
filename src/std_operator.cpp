// std::operator<<(basic_ostream<wchar_t>&, const wchar_t*) 0x1021f374 (unit v5640).
// The sentry (tie flush, good check), __putnumber of the text with the stream's fill, the failure state and the ios_base::failure
// thrown by the inlined clear(), then the unitbuf flush. The catch landing pads and the inlined setstate block are unreachable here.
#include <cstdint>
#include <cstring>

#include "online.h"
#include "std_basic_istream.h"

namespace wc = wostream_callee;
using ignore_callee::IgnoreIos;
using ignore_callee::IgnoreStream;

namespace {
// The failure object the code builds on its stack: vtable, message (counted buffer), counter.
struct FailureObject {
    const void* mVtbl;
    char* mMessage;
    int32_t* mCount;
};
const char kType[] = "!std::exception!!std::ios_base::failure!!";   // 0x102517e6
const char kMessage[] = "ios_base failure in clear";                 // 0x1025148b

uint32_t FlagsOf(IgnoreIos* ios) {   // Mac 0x30 (host 0x34)
    uint32_t value;
    std::memcpy(&value, &ios->mPad28[8], 4);
    return value;
}
}  // namespace

void ignore_callee::OstreamWrite(IgnoreStream* stream, const char16_t* text) {
    bool sentry = false;
    IgnoreIos* ios = stream->mIos;
    if (!wc::Good(ios)) {
        wc::Setstate(ios, 4);
    } else {
        if (wc::Tie(ios) != nullptr) wc::Flush(wc::Tie(ios));
        if (wc::Good(ios)) sentry = true;
        else wc::Setstate(ios, 4);
    }
    if (sentry) {
        ios = stream->mIos;
        void* iter = reinterpret_cast<void*>(static_cast<uintptr_t>(ios->mStreamBuf));
        int32_t length = static_cast<int32_t>(wc::Wcslen(text));
        uint16_t fill;
        std::memcpy(&fill, &ios->mPad3c[4], 2);   // Mac 0x40
        void* result = wc::PutNumber(iter, ios, static_cast<char16_t>(fill), nullptr, 0, text, length);
        if (result == nullptr) {
            ios = stream->mIos;
            ios->mState |= 5;
            if (ios->mStreamBuf == 0) ios->mState |= 1;
            if ((ios->mState & ios->mExceptions) != 0) {
                FailureObject object;
                object.mVtbl = gIosFailureVtbl;
                object.mMessage = wc::NewBytes(wc::Strlen(kMessage) + 1);
                object.mCount = nullptr;
                if (object.mMessage != nullptr) {
                    object.mCount = wc::NewInt();
                    if (object.mCount != nullptr) *object.mCount = 1;
                }
                wc::CopyString(object.mMessage, kMessage);
                wc::Throw(kType, &object, gIosFailureDtorVec);
            }
        }
    }
    ios = stream->mIos;
    if ((ios->mState & 5) == 0 && (FlagsOf(ios) & 0x2000) != 0) wc::Flush(stream);
}
