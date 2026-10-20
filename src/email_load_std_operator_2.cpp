// std::operator<<(basic_ostream<char>&, const char*) 0x10207150 (unit v5700): the sentry (tie flush, good check), the text written by
// __putnumber with the stream's fill, the failure state and the ios_base::failure throw, then the unitbuf flush.
// The exception-cleanup blocks of the listing (0x10207220 on) are unreachable and not translated.
#include "email.h"
#include <cstdint>
#include <cstdlib>
#include <string>
#include "TPMain.h"
#include "platform/macdoze.h"
#include "ui_ime.h"

namespace {
const char kType[] = "!std::exception!!std::ios_base::failure!!";   // 0x10250098
const char kMessage[] = "ios_base failure in clear";                 // 0x1024fde4
}  // namespace

MacWriteStream* OstreamWriteText(MacWriteStream* os, const char* text) {
    bool ok = false;
    const bool sentryDone = false;   // the byte at 0xd1 is only set on the unwinding path
    MacIosBase* base = reinterpret_cast<MacIosBase*>(os->mIos.get());
    if (!MslIosGood(base)) {
        MslIosSetstate(base, 4);
    } else {
        if (MslIosTie(base) != nullptr) MslOstreamFlush(MslIosTie(base));
        if (MslIosGood(base)) ok = true;
        else MslIosSetstate(base, 4);
    }
    if (ok) {
        MacWriteIos* ios = os->mIos.get();
        MacPutStreamBuf* iter = ios->mStreamBuf.get();
        int32_t length = static_cast<int32_t>(IME::MslStrlen(text));
        MacPutStreamBuf* result =
            PutNumberChar(iter, reinterpret_cast<MacPutIos*>(ios), static_cast<char>(ios->mFill), nullptr, 0, text, length);
        if (result == nullptr) {
            ios = os->mIos.get();
            ios->mState |= 5;
            if (ios->mStreamBuf.get() == nullptr) ios->mState |= 1;
            if ((ios->mState & ios->mExceptions) != 0) {
                MslThrowObject object;
                object.mVtbl = gIosFailureVtbl;
                char* message = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(kMessage) + 1)));
                object.mMessage = message;
                object.mCount = nullptr;
                if (message != nullptr) {
                    int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
                    if (count != nullptr) *count = 1;
                    object.mCount = count;
                }
                IME::MslStrcpy(object.mMessage, kMessage);
                ::MslThrow(kType, &object, gIosFailureDtorVec);
            }
        }
    }
    MacWriteIos* last = os->mIos.get();
    if ((last->mState & 5) == 0 && (last->mFlags & 0x2000) != 0 && !sentryDone) MslOstreamFlush(os);
    return os;
}
