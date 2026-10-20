// std::basic_ostream<char>::flush 0x1020a604 (unit v5663).
// The catch handlers of the listing (0x1020a6cc, 0x1020a794, 0x1020a8ac) are only reached through the exception tables,
// never by fall-through or branch; they are not translated. The sentry flag at 0x99 is set to 1 before the final check, so
// the final recursive flush of the listing never runs (kept as the dead test below).
#include <cstddef>
#include <cstdint>

#include "std_basic_istream.h"
#include "ui_ime.h"

namespace {
const char kFailureTypeName[] = "!std::exception!!std::ios_base::failure!!";   // 0x10250098
const char kFailureMessage[] = "ios_base failure in clear";                    // 0x1024fde4
}  // namespace

BasicIstreamChar* OstreamFlush(BasicIstreamChar* self) {
    bool done = false;
    bool sentryDone = false;
    if (!MslIosGood(self->mIos.get())) {
        MslIosSetstate(self->mIos.get(), 4);
    } else {
        if (MslIosTie(self->mIos.get()) != nullptr) {
            OstreamFlush(static_cast<BasicIstreamChar*>(MslIosTie(self->mIos.get())));
        }
        if (MslIosGood(self->mIos.get())) {
            done = true;
        } else {
            MslIosSetstate(self->mIos.get(), 4);
        }
    }
    (void)done;
    sentryDone = true;

    MacIosBase* ios = self->mIos.get();
    if (ios->mStreamBuf.get() != nullptr) {
        MacStreamBuf* sb = ios->mStreamBuf.get();
        bool failed = false;
        if (MslStreamBufVirtual(sb, 0x1c) == -1) failed = true;
        if (failed) {
            ios = self->mIos.get();
            ios->mState |= 1;
            if (ios->mStreamBuf.get() == nullptr) ios->mState |= 1;
            if ((ios->mState & ios->mExceptions) != 0) {
                MslThrowObject object;
                object.mVtbl = gIosFailureVtbl;
                char* message =
                    static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(kFailureMessage) + 1)));
                object.mMessage = message;
                object.mCount = nullptr;
                if (message != nullptr) {
                    int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
                    if (count != nullptr) *count = 1;
                    object.mCount = count;
                }
                IME::MslStrcpy(object.mMessage, kFailureMessage);
                ::MslThrow(kFailureTypeName, &object, gIosFailureDtorVec);
            }
        }
    }

    MacIosBase* last = self->mIos.get();
    MacSentryIos* flagsView = reinterpret_cast<MacSentryIos*>(last);
    if ((last->mState & 5) == 0 && (flagsView->mFlags & 0x2000) != 0 && !sentryDone) OstreamFlush(self);
    return self;
}
