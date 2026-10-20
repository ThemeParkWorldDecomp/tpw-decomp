// std::basic_istream<wchar_t> ~basic_istream 0x1021d4a0, getline 0x102202b0 and the constructor 0x102266e4 (unit v5708).
// The compiler's exception handler regions (the setstate/clear catch blocks) are unreachable without a real exception
// and are not translated.
#include <cstdint>
#include <cstring>
#include <cstdio>
#include "FS_UserInfo.h"
#include "TPGlobals.h"
#include "online.h"
#include "std_basic_ifstream.h"
#include "std_basic_istream.h"

namespace {

using ignore_callee::IgnoreIos;
using ignore_callee::IgnoreStream;

const char kIosFailureText[] = "ios_base failure in clear";                                // 0x1025148b
const char kIosFailureType[] = "!std::exception!!std::ios_base::failure!!";                // 0x102517e6

inline WideIstream* Wide(IgnoreStream* stream) { return reinterpret_cast<WideIstream*>(stream); }

inline IgnoreIos* IosPart(WideIstream* self) {
    return reinterpret_cast<IgnoreIos*>(reinterpret_cast<uint8_t*>(self) + kWideIstreamIosPart);
}

inline MacStreamBuf* StreamBufOf(IgnoreIos* ios) { return Ptr32<MacStreamBuf>{ios->mStreamBuf}.get(); }

inline uint32_t Entry(MacStreamBuf* sb, uint32_t offset) { return *reinterpret_cast<uint32_t*>(sb->mVtable.get() + offset); }

// Builds the ios_base::failure the stream throws once its state meets the exception mask (the fake __throw returns).
void ThrowFailure() {
    MslThrowObject object;
    object.mVtbl = gIosFailureVtbl;
    uint32_t length = wostream_callee::Strlen(kIosFailureText);
    object.mMessage = wostream_callee::NewBytes(length + 1);
    object.mCount = nullptr;
    if (object.mMessage != nullptr) {
        int32_t* count = wostream_callee::NewInt();
        if (count != nullptr) *count = 1;
        object.mCount = count;
    }
    wostream_callee::CopyString(object.mMessage, kIosFailureText);
    wostream_callee::Throw(kIosFailureType, &object, gIosFailureDtorVec);
}

// Sets the stream state bits, then raises when the state meets the exception mask.
void SetStateBits(IgnoreIos* ios, uint32_t bits) {
    ios->mState |= bits;
    if (ios->mStreamBuf == 0) ios->mState |= 1;
    if ((ios->mState & ios->mExceptions) != 0) ThrowFailure();
}

void Bump(IgnoreIos* ios) {
    MacStreamBuf* sb = StreamBufOf(ios);
    if (sb->mGptr.mOffset < sb->mEgptr.mOffset) sb->mGptr.mOffset += 2;
    else MslStreamBufVirtual(sb, Entry(sb, 0x2c));
}

}  // namespace

namespace ignore_callee {

// The original answers this; the declaration in online.h is void.
void IstreamDtor(IgnoreStream* stream, int16_t flag) {
    WideIstream* self = Wide(stream);
    if (self != nullptr) {
        self->mVtable = gIfsWVtblA;
        IgnoreIos* part = IosPart(self);
        self->mIos->mVtable = reinterpret_cast<const uint8_t*>(gIfsWVtblA) + 0xc;
        self->mIos->mBaseOffset = static_cast<int32_t>(reinterpret_cast<uint8_t*>(part) - reinterpret_cast<uint8_t*>(self->mIos));
        if (flag != 0) {
            // The original also tests this + 0xc != 0, always true for a real object.
            part->mVtable = gIfsWVtblE;
            ifstream_callee::IosBaseDtor(part, 0);
        }
        if (flag > 0) MslOperatorDelete(self);
    }
}

IgnoreStream* IstreamGetline(IgnoreStream* stream, char16_t* buffer, int32_t count, char16_t delimiter) {
    WideIstream* self = Wide(stream);
    bool ok = false;
    self->mGcount = 0;
    if (!wostream_callee::Good(self->mIos)) {
        wostream_callee::Setstate(self->mIos, 4);
    } else {
        if (wostream_callee::Tie(self->mIos) != nullptr) wostream_callee::Flush(wostream_callee::Tie(self->mIos));
        if (wostream_callee::Good(self->mIos)) ok = true;
        else wostream_callee::Setstate(self->mIos, 4);
    }
    if (buffer == nullptr || count <= 0) {
        SetStateBits(self->mIos, 4);
        return stream;
    }
    if (!ok) return stream;
    uint32_t flags = 0;
    for (;;) {
        MacStreamBuf* sb = StreamBufOf(self->mIos);
        uint16_t c;
        if (sb->mGptr.mOffset < sb->mEgptr.mOffset) c = *reinterpret_cast<uint16_t*>(sb->mGptr.get());
        else c = static_cast<uint16_t>(MslStreamBufVirtual(sb, Entry(sb, 0x28)));
        if (c == 0xffff) {
            flags |= 2;
            break;
        }
        if (c == delimiter) {
            Bump(self->mIos);
            self->mGcount = self->mGcount + 1;
            break;
        }
        if (count == 1) {
            flags |= 4;
            break;
        }
        Bump(self->mIos);
        self->mGcount = self->mGcount + 1;
        count--;
        *buffer = c;
        buffer++;
    }
    if (self->mGcount == 0) flags |= 4;
    *buffer = 0;
    SetStateBits(self->mIos, flags);
    return stream;
}

IgnoreStream* IstreamCtor(IgnoreStream* stream, int16_t withVirtualBase, uint32_t streambuf) {
    WideIstream* self = Wide(stream);
    IgnoreIos* part = IosPart(self);
    if (withVirtualBase != 0) {
        self->mIos = part;
        part->mVtable = gIfsWVtblE;   // the original first stores __vt__ios_base, overwritten at once
    }
    self->mVtable = gIfsWVtblA;
    self->mIos->mVtable = reinterpret_cast<const uint8_t*>(gIfsWVtblA) + 0xc;
    self->mIos->mBaseOffset = static_cast<int32_t>(reinterpret_cast<uint8_t*>(part) - reinterpret_cast<uint8_t*>(self->mIos));
    self->mGcount = 0;
    IgnoreIos* ios = self->mIos;
    wistream_callee::IosInit(ios, streambuf);
    std::memset(&ios->mPad3c[0], 0, 4);          // Mac 0x3c
    char16_t fill = wistream_callee::Widen(ios, ' ');
    std::memcpy(&ios->mPad3c[4], &fill, 2);      // Mac 0x40
    return stream;
}

}  // namespace ignore_callee
