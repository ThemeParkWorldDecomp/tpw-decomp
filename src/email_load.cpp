// email.cpp range of the main app (SimTheme Park 2.04b3), unit w90: the outbox's destructor 0x10203ba8 (276 bytes), Outbox::Load
// 0x10203ce0 (1,696 bytes) and AddressBook::Load 0x10206b2c (1,236 bytes).
//
// The original reads its two files with the MSL stream classes (a std::basic_ifstream on a local std::string, its getline into a
// 128-byte buffer, the stream's state bits tested by hand). The host reads the file through the platform's file calls instead
// (NS_MacDoze::CreateFile resolves the game's ':' paths against the user and game folders, which a plain std::ifstream could
// not) and TextFile below follows the MSL stream's rules: the same state bits and the same getline.
#include "email.h"

#include <cstdint>
#include <cstdlib>
#include <string>

#include "TPMain.h"
#include "platform/macdoze.h"

namespace {

// The strings of the main app's data (TOC word 0x102641c0 plus 0x2e5, 0x2ed and 0x330).
const char kOutboxFolder[] = ":outbox";
const char kOutboxFile[] = ":outbox.dat";
const char kAddressBookFile[] = ":addrbook.dat";

// atoi of the Mac's C library: a number beyond the 32-bit limits gives the limit (the host's atoi would wrap, or be undefined).
int32_t Atoi(const char* text) {
    const long long value = std::strtoll(text, nullptr, 10);
    if (value > INT32_MAX) return INT32_MAX;
    if (value < INT32_MIN) return INT32_MIN;
    return static_cast<int32_t>(value);
}

// The part of an MSL std::ifstream the loaders use: open for reading, getline into 128 characters, close. A file is read whole
// when it opens (the platform has no stream), and the handle stays open until Close, as the Mac's file does.
class TextFile {
public:
    ~TextFile() { Close(); }

    // basic_filebuf::open(name, in). False when there is no such file.
    bool Open(const char* name) {
        NS_MacDoze::InternalHandle* const handle =
            NS_MacDoze::CreateFile(name, 0x80000000u, 1, nullptr, 3, 0, nullptr);
        if (handle == nullptr || handle == reinterpret_cast<NS_MacDoze::InternalHandle*>(static_cast<intptr_t>(-1))) return false;
        mHandle = handle;
        const uint32_t size = NS_MacDoze::GetFileSize(mHandle, nullptr);
        if (size != 0 && size != 0xffffffffu) {
            mBytes.resize(size);
            uint32_t done = 0;
            NS_MacDoze::ReadFile(mHandle, &mBytes[0], size, &done, nullptr);
            mBytes.resize(done);
        }
        return true;
    }

    // basic_filebuf::close.
    void Close() {
        if (mHandle != nullptr) NS_MacDoze::CloseHandle(mHandle);
        mHandle = nullptr;
    }

    // basic_istream::getline(line, 128, '\n') at 0x102079f8, then the callers' test that the stream is still usable (neither
    // the fail nor the bad bit). A stream that already failed or reached the end is not read (it gets the fail bit, `line` stays);
    // a line stops at the newline (taken, not stored), at the end of the file (the end bit; a line read before it counts) or after
    // 127 characters (the fail bit, the next character stays); a call that took no character at all fails.
    bool GetLine(char (&line)[128]) {
        if (mEof || mFail) {
            mFail = true;
            return false;
        }
        uint32_t taken = 0;
        uint32_t stored = 0;
        while (true) {
            if (mPos >= mBytes.size()) {
                mEof = true;
                break;
            }
            const char c = mBytes[mPos];
            if (c == '\n') {
                ++mPos;
                ++taken;
                break;
            }
            if (stored == 127) {
                mFail = true;
                break;
            }
            line[stored++] = c;
            ++mPos;
            ++taken;
        }
        if (taken == 0) mFail = true;
        line[stored] = '\0';
        return !mFail;
    }

private:
    NS_MacDoze::InternalHandle* mHandle = nullptr;
    std::string mBytes;
    size_t mPos = 0;
    bool mEof = false;
    bool mFail = false;
};

// One block of the outbox file: numbers, one per line, up to an empty line or the end of the stream.
void ReadNumbers(TextFile& file, std::list<uint32_t>& numbers) {
    char line[128];
    while (file.GetLine(line) && line[0] != '\0') numbers.push_back(static_cast<uint32_t>(Atoi(line)));
}

}  // namespace

// The e-mail the outbox is writing is deleted (Email::~Email with the delete flag 1) and forgotten; the members' own
// destructors follow. (The Mac's tests against 0xffffffc0 and the like before the members' destructors are the Metrowerks
// sub-object NULL checks: nothing to write.)
Outbox::~Outbox() {
    if (mEmail != nullptr) {
        delete mEmail;
        mEmail = nullptr;
    }
}

// The lists are emptied first (mF28, mF34, mF40), then the folder name is built, then the file opened. The three blocks go to
// mF28, mF40 and mF34 in that order (the listing's inserts name +0x28, +0x40 and +0x34). The original's exception paths (the
// stream's "ios_base failure in clear" text, thrown only when an exception mask is set, which nothing sets) are not translated.
int32_t Outbox::Load(const char* path) {
    TextFile file;
    mF28.clear();
    mF34.clear();
    mF40.clear();
    mF00.assign(path);
    mF00.append(kOutboxFolder);
    std::string local(mF00);
    local.append(kOutboxFile);
    if (!file.Open(local.c_str())) return NS_MacDoze::CreateDirectory(mF00.c_str(), nullptr) != 0 ? 1 : 0;
    ReadNumbers(file, mF28);
    ReadNumbers(file, mF40);
    ReadNumbers(file, mF34);
    file.Close();
    return 1;
}

// Every line that reads is an address (an empty line too, unlike the outbox file). The complaint address is added whether or
// not the file opened, then the list is sorted and its neighbouring repeats removed. (The original then walks the list to the
// complaint address with std::operator== and drops the answer: nothing to translate.)
int32_t AddressBook::Load(const char* path) {
    TextFile file;
    std::string local(path);
    local.append(kAddressBookFile);
    const bool opened = file.Open(local.c_str());
    if (opened) {
        char line[128];
        while (file.GetLine(line)) mAddresses.push_back(std::string(line));
        file.Close();
    }
    mAddresses.push_back(gOnlineConfig.mComplainEmail);
    mAddresses.sort();
    mAddresses.unique();
    return opened ? 1 : 0;
}
