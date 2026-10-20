// std::basic_ostream<wchar_t>::flush 0x102268f8 and the constructor 0x10226c94 (unit v5898).
#include <cstdint>
#include <cstring>

#include "email.h"
#include "std_basic_ifstream.h"
#include "std_basic_istream.h"

uint32_t gWostreamVtbl[4] = {0x102b1f00, 0, 0, 0};   // 0x102b1f08 (TOC -0x5eb4)

namespace {
const char kFailureText[] = "ios_base failure in clear";                 // 0x1025148b
const char kFailureType[] = "!std::exception!!std::ios_base::failure!!";  // 0x102517e6

uint32_t Lo32(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
void W32(void* p, uint32_t v) { std::memcpy(p, &v, 4); }
uint32_t R32(const void* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
uint32_t Off(const void* p) { return tpw::ArenaOffset(p); }
}  // namespace

WostreamW* WostreamFlush(WostreamW* self) {
    // Stack flags of the original: the "good" flag (0x98) is never read; the done flag (0x99) is set before the end, so the
    // final "unitbuf and not done" flush never runs. The catch handler code at 0x102269c0.. is not reachable.
    MacIosBase* ios = self->mIos.get();
    if (!wostream_u::Good(ios)) {
        wostream_u::Setstate(ios, 4);
    } else {
        void* tie = wostream_u::Tie(self->mIos.get());
        if (tie != nullptr) {
            void* tied = wostream_u::Tie(self->mIos.get());
            WostreamFlush(static_cast<WostreamW*>(tied));
        }
        if (!wostream_u::Good(self->mIos.get())) wostream_u::Setstate(self->mIos.get(), 4);
    }
    MacIosBase* io = self->mIos.get();
    MacStreamBuf* sb = io->mStreamBuf.get();
    if (sb != nullptr) {
        uint32_t entry = R32(sb->mVtable.get() + 0x1c);
        bool atEnd = wostream_u::Virt(sb, entry) == -1;
        if (atEnd) {
            io = self->mIos.get();
            io->mState |= 1;
            io = self->mIos.get();
            if (io->mStreamBuf.get() == nullptr) io->mState |= 1;
            if ((io->mState & io->mExceptions) != 0) {
                MslThrowObject object;
                object.mVtbl = gIosFailureVtbl;
                size_t len = std::strlen(kFailureText);
                object.mMessage = wostream_u::NewArray(static_cast<uint32_t>(len + 1));
                object.mCount = nullptr;
                if (object.mMessage != nullptr) {
                    int32_t* count = wostream_u::NewInt();
                    if (count != nullptr) *count = 1;
                    object.mCount = count;
                }
                std::strcpy(object.mMessage, kFailureText);
                wostream_u::Throw(kFailureType, &object, gIosFailureDtorVec);
            }
        }
    }
    return self;
}

WostreamW* WostreamConstruct(WostreamW* self, int16_t flag, uint32_t streambuf) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (flag != 0) {
        W32(s, Off(s + 8));
        W32(s + 8, Lo32(gIosBaseVt));
        W32(s + 8, Lo32(gIfsWVtblE));
    }
    W32(s + 4, Lo32(gWostreamVtbl));
    uint8_t* ios = tpw::gArenaBase + R32(s);
    W32(ios, Lo32(gWostreamVtbl) + 0xc);
    W32(ios + 0x44, Off(s + 8) - R32(s));
    wostream_u::IosInit(ios, streambuf);
    W32(ios + 0x3c, 0);
    char16_t fill = wostream_u::Widen(ios, 0x20);
    std::memcpy(ios + 0x40, &fill, 2);
    return self;
}
