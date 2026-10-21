// AddressBook::Save (0x102067cc, unit v5009): the address book written to "<path>:addrbook.dat".
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <ios>
#include <list>
#include <string>

#include "email.h"
#include "platform/macdoze.h"

namespace {
const char kAddressBookFile[] = ":addrbook.dat";
const char kClearFailure[] = "ios_base failure in clear";

// The stream's clear(): failbit set (and badbit when there is no buffer), raising when the state meets the exceptions mask.
void FailAndMaybeThrow(SaveStream& out) {
    out.ios->mState |= 4;
    if (out.ios->mRdbuf == 0) out.ios->mState |= 1;
    if ((out.ios->mState & out.ios->mMask) != 0) SaveStreamThrow(kClearFailure);
}
}  // namespace

// Weak stand-ins on the real std::ofstream; the unit's test library defines the recording fakes.
__attribute__((weak)) void SaveStreamCtor(SaveStream* s) {
    s->ios = new SaveIos();
    s->filebufOpen = 0;
    s->impl = new std::ofstream();
}
__attribute__((weak)) bool SaveStreamOpen(SaveStream* s, const char* name) {
    auto* f = static_cast<std::ofstream*>(s->impl);
    f->open(name, std::ios::out | std::ios::trunc);
    s->filebufOpen = f->is_open() ? 1 : 0;
    return f->is_open();
}
__attribute__((weak)) void SaveStreamWrite(SaveStream* s, const char* text) { *static_cast<std::ofstream*>(s->impl) << text; }
__attribute__((weak)) bool SaveStreamClose(SaveStream* s) {
    auto* f = static_cast<std::ofstream*>(s->impl);
    f->close();
    return !f->fail();
}
__attribute__((weak)) void SaveStreamDestroy(SaveStream* s) {
    delete static_cast<std::ofstream*>(s->impl);
    delete s->ios;
}
__attribute__((weak)) void SaveStreamThrow(const char* what) { throw std::ios_base::failure(what); }

int32_t AddressBook::Save(const char* path) {
    SaveStream out;
    SaveStreamCtor(&out);
    std::string name;
    name.assign(path, std::char_traits<char>::length(path));
    name.append(kAddressBookFile, sizeof(kAddressBookFile) - 1);
    if (!SaveStreamOpen(&out, name.c_str())) FailAndMaybeThrow(out);
    if (out.filebufOpen == 0) {
        SaveStreamDestroy(&out);
        return 0;
    }
    for (const std::string& address : mAddresses) {
        char line[128];
        std::sprintf(line, "%s\n", address.c_str());
        SaveStreamWrite(&out, line);
    }
    if (!SaveStreamClose(&out)) FailAndMaybeThrow(out);
    SaveStreamDestroy(&out);
    return 1;
}
