// std::basic_istream<char>::read 0x1020d6b4 and ::getline 0x102079f8 (unit v5014).
#include "std_basic_istream.h"

#include <cstddef>
#include <cstring>

namespace {

const char kFailureType[] = "!std::exception!!std::ios_base::failure!!";   // 0x10250846 / 0x10250098
const char kFailureText[] = "ios_base failure in clear";                    // 0x10250722

// The exception object is built on the stack; __throw returns in the test, so execution goes on afterwards.
void RaiseFailure() {
    MslThrowObject object;
    object.mVtbl = gIosFailureVtbl;
    size_t length = std::strlen(kFailureText);
    object.mMessage = new char[length + 1];
    object.mCount = nullptr;
    if (object.mMessage) {
        object.mCount = new int32_t;
        *object.mCount = 1;
    }
    std::strcpy(object.mMessage, kFailureText);
    MslThrow(kFailureType, &object, gIosFailureDtorVec);
}

// state |= bits; no buffer sets badbit; a state bit under the exception mask throws.
void Finish(MacIosBase* ios, uint32_t bits) {
    ios->mState |= bits;
    if (ios->mStreamBuf.mOffset == 0) ios->mState |= 1;
    if ((ios->mState & ios->mExceptions) != 0) RaiseFailure();
}

// The start of both: gcount = 0, tie flush, good check. True when reading may go on.
bool Prepare(BasicIstreamChar* self) {
    MacIosBase* ios = self->mIos.get();
    bool ok = false;
    self->mGcount = 0;
    if (!MslIosGood(ios)) {
        MslIosSetstate(ios, 4);
    } else {
        if (MslIosTie(ios) != nullptr) MslOstreamFlush(MslIosTie(ios));
        if (MslIosGood(ios)) ok = true;
        else MslIosSetstate(ios, 4);
    }
    return ok;
}

inline uint32_t Entry(MacStreamBuf* sb, uint32_t offset) { return *reinterpret_cast<uint32_t*>(sb->mVtable.get() + offset); }

// The stream buffer's bump: the get pointer moves on, or the virtual uflow runs.
inline void Bump(MacStreamBuf* sb) {
    if (sb->mGptr.mOffset < sb->mEgptr.mOffset) sb->mGptr.mOffset = sb->mGptr.mOffset + 1;
    else MslStreamBufVirtual(sb, Entry(sb, 0x2c));
}

}  // namespace

BasicIstreamChar* BasicIstreamChar::read(char* buffer, int32_t count) {
    bool ok = Prepare(this);
    if (buffer == nullptr || count <= 0) {
        Finish(mIos.get(), 4);
        return this;
    }
    if (!ok) return this;
    uint32_t flags = 0;
    while (count > 0) {
        MacStreamBuf* sb = mIos->mStreamBuf.get();
        int32_t c;
        if (sb->mGptr.mOffset < sb->mEgptr.mOffset) {
            uint8_t* p = sb->mGptr.get();
            sb->mGptr.mOffset = sb->mGptr.mOffset + 1;
            c = *p;
        } else {
            c = MslStreamBufVirtual(sb, Entry(sb, 0x2c));
        }
        if (c == -1) {
            flags |= 6;
            break;
        }
        mGcount = mGcount + 1;
        count--;
        *buffer = static_cast<char>(c);
        buffer++;
    }
    Finish(mIos.get(), flags);
    return this;
}

BasicIstreamChar* BasicIstreamChar::getline(char* buffer, int32_t count, char delimiter) {
    bool ok = Prepare(this);
    if (buffer == nullptr || count <= 0) {
        Finish(mIos.get(), 4);
        return this;
    }
    if (!ok) return this;
    uint32_t flags = 0;
    for (;;) {
        MacStreamBuf* sb = mIos->mStreamBuf.get();
        int32_t c;
        if (sb->mGptr.mOffset < sb->mEgptr.mOffset) c = *sb->mGptr.get();
        else c = MslStreamBufVirtual(sb, Entry(sb, 0x28));
        if (c == -1) {
            flags |= 2;
            break;
        }
        char ch = static_cast<char>(c);
        if (ch == delimiter) {
            Bump(mIos->mStreamBuf.get());
            mGcount = mGcount + 1;
            break;
        }
        if (count == 1) {
            flags |= 4;
            break;
        }
        Bump(mIos->mStreamBuf.get());
        count--;
        mGcount = mGcount + 1;
        *buffer = ch;
        buffer++;
    }
    if (mGcount == 0) flags |= 4;
    *buffer = 0;
    Finish(mIos.get(), flags);
    return this;
}
