// Outbox::ReadMailData (unit v5148), main app: email.cpp.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <list>
#include <string>

#include "email.h"

namespace {

const char kFailure[] = "ios_base failure in clear";   // 0x1024fde4
const char kDatName[] = ":%08x.dat";                   // 0x1024fe04
const char kJpgName[] = ":%08x.jpg";                   // 0x1024fe0e

// The failbit (and badbit when there is no buffer) is set, and the stream throws when the state meets its exception mask.
void RaiseIfMasked(SaveStream& in) {
    SaveIos* ios = in.ios;
    ios->mState |= 4;
    if (ios->mRdbuf == 0) ios->mState |= 1;
    if ((ios->mState & ios->mMask) != 0) SaveStreamThrow(kFailure);
}

// widen('\n') then getline(buffer, 0x100, that character). The Mac's buffer is 132 bytes (a longer line overruns the frame).
void ReadLine(SaveStream& in, char* buffer) {
    char delim = outbox_callee::StreamWiden(&in);
    outbox_callee::StreamGetline(&in, buffer, 0x100, delim);
}

}  // namespace

bool Outbox::ReadMailData(int32_t index) {
    using namespace outbox_callee;
    char buffer[0x100 + 4];
    SaveStream in;
    StreamCtor(&in);
    std::string name(mF00);
    std::sprintf(buffer, kDatName, static_cast<unsigned>(index));
    name.append(buffer, std::strlen(buffer));
    if (!StreamOpen(&in, name.c_str())) RaiseIfMasked(in);
    if (in.filebufOpen == 0) {
        StreamDestroy(&in);
        return false;
    }
    int32_t value;
    ReadLine(in, buffer);
    value = std::atoi(buffer);
    std::memcpy(mF1C + 0, &value, 4);   // +0x1c
    ReadLine(in, buffer);
    value = std::atoi(buffer);
    std::memcpy(mF1C + 4, &value, 4);   // +0x20
    ReadLine(in, buffer);
    value = std::atoi(buffer);
    std::memcpy(mF1C + 8, &value, 4);   // +0x24
    ReadLine(in, buffer);
    mF04.assign(buffer, std::strlen(buffer));
    ReadLine(in, buffer);
    mF08.assign(buffer, std::strlen(buffer));
    mF0C.clear();
    for (;;) {
        ReadLine(in, buffer);
        bool good = (in.ios->mState & 5) == 0 && in.ios->mRdbuf != 0;
        if (!good) break;
        mF0C.push_back(std::string(buffer));
    }
    if (!SaveStreamClose(&in)) RaiseIfMasked(in);
    mF18 = mF00;
    std::sprintf(buffer, kJpgName, static_cast<unsigned>(index));
    mF18.append(buffer, std::strlen(buffer));
    StreamDestroy(&in);
    return true;
}

// The host's real streams (weak: the test library defines recording fakes).
namespace outbox_callee {
__attribute__((weak)) void StreamCtor(SaveStream* s) {
    s->ios = new SaveIos();
    std::memset(s->ios, 0, sizeof(SaveIos));
    s->ios->mRdbuf = 1;
    s->filebufOpen = 0;
    s->impl = new std::ifstream();
}
__attribute__((weak)) bool StreamOpen(SaveStream* s, const char* name) {
    std::ifstream* f = static_cast<std::ifstream*>(s->impl);
    f->open(name, std::ios::in | std::ios::binary);
    s->filebufOpen = f->is_open() ? 1 : 0;
    return f->is_open();
}
__attribute__((weak)) char StreamWiden(SaveStream*) { return '\n'; }
__attribute__((weak)) void StreamGetline(SaveStream* s, char* buffer, int32_t size, char delim) {
    std::ifstream* f = static_cast<std::ifstream*>(s->impl);
    f->getline(buffer, size, delim);
    if (f->eof()) s->ios->mState |= 2;
    if (f->fail()) s->ios->mState |= 4;
}
__attribute__((weak)) void StreamDestroy(SaveStream* s) {
    delete static_cast<std::ifstream*>(s->impl);
    delete s->ios;
    s->impl = nullptr;
    s->ios = nullptr;
}
}  // namespace outbox_callee
