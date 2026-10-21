// IPBase.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of IPBase code, named after it.

#include "analyser.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "online.h"
#include "std_basic_istream.h"
#include "ThingReceivers.h"
#include "ui_ime.h"
#include "WinScrn.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <new>
#include <string>

// SwearFilter (OnlinePerson.cpp, main app): Filter (0x1020d2cc) and LoadWordList (0x1020cd08), unit v5094.

namespace {

const char kFailureText[] = "ios_base failure in clear";                   // 0x10250722
const char kFailureType[] = "!std::exception!!std::ios_base::failure!!";   // 0x10250846

// The __wctype_map test of the listing: a character below 0x100 whose class has a bit of mask 6.
bool IsSeparator(char16_t c) { return c < 0x100 ? (gSwearWctypeMap[c] & 6) != 0 : false; }

// The stack exception the library builds and throws (the fakes return).
void ThrowFailure() {
    IME::MslExceptionObject ex;
    ex.mVtbl = IME::gMslExcVtblC;
    ex.mVtbl = gIosFailureVtbl;
    uint32_t length = static_cast<uint32_t>(IME::MslStrlen(kFailureText));
    IME::MslRefCountedCharsCtor(&ex.mRc, static_cast<char*>(IME::MslNewArray(length + 1)));
    IME::MslStrcpy(ex.mRc.mPtr, kFailureText);
    IME::MslThrow(kFailureType, &ex, gIosFailureDtorVec);
}

// basic_ios::setstate(failbit) after a failed open or close: the state gets fail (and bad when there is no buffer), and the
// exception is raised when the state meets the mask.
void FailureCheck(SwearIfstream& s) {
    s.mIos->mState |= 4;
    if (s.mIos->mRdbuf == 0) s.mIos->mState |= 1;
    if ((s.mIos->mState & s.mIos->mMask) != 0) ThrowFailure();
}

// The stream's tear-down, inline in every exit.
void Destroy(SwearIfstream& s) {
    s.mVtbl = gIfstreamVtbl;
    s.mIos->mVtbl = gIfstreamVtbl + 0xc;
    s.mIos->mOffset = reinterpret_cast<char*>(&s.mIosStorage) - reinterpret_cast<char*>(s.mIos);
    swear_callee::FilebufDtor(&s.mFilebuf, -1);
    swear_callee::IstreamDtor(&s, 0);
    swear_callee::IosDtor(&s.mIosStorage, 0);
}

uint16_t BigEndian(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }

}  // namespace

// 0x1020cd08
bool SwearFilter::LoadWordList(const char* path, std::list<std::u16string>& words, bool flag) {
    SwearIfstream s;
    swear_callee::IfstreamCtor(&s, 1);
    if (!flag) {
        Destroy(s);
        return false;
    }
    if (swear_callee::FilebufOpen(&s.mFilebuf, path, 0xc) == nullptr) FailureCheck(s);
    if (s.mFilebuf.mFile == 0) {
        Destroy(s);
        return false;
    }
    uint8_t buffer[0x100] = {};   // the Mac's holds 0xac bytes (the frame at 0x204)
    char16_t line[0x81];          // the frame at 0x6e
    uint8_t* at = buffer;
    while ((s.mIos->mState & 2) == 0) {
        swear_callee::IstreamRead(&s, reinterpret_cast<char*>(at), 2);
        if (BigEndian(at) != 0xa) {
            at += 2;
            continue;
        }
        uint8_t first = buffer[0];
        at += 2;
        if (first == 0x23) continue;   // a comment: the buffer position is kept
        const uint8_t* from = buffer;
        char16_t* to = line;
        while (BigEndian(from) != 0xd && BigEndian(from) != 0xa) {
            *to++ = BigEndian(from);
            from += 2;
        }
        *to = 0;
        for (char16_t* p = line; *p != 0; ++p) *p = static_cast<char16_t>(*p ^ 0xffff);
        std::u16string word(line);
        words.insert(words.end(), word);
        at = buffer;
    }
    if (swear_callee::FilebufClose(&s.mFilebuf) == nullptr) FailureCheck(s);
    Destroy(s);
    return true;
}

// online.cpp / OnlinePerson.cpp (main app), unit v51009:
//   VoteNotifyCallback 0x10213590, SwearFilter::Init 0x1020d1bc, stristr 0x1020cbec, IPBase::ShowError 0x1020cac8,
//   FTPClient::ProcessEvents 0x1020c8cc.

// ---- SwearFilter::Init
__attribute__((weak)) void swear_callee::EraseList(std::list<std::u16string>* list) { list->erase(list->begin(), list->end()); }

// 0x1020d1bc
bool SwearFilter::Init(const char* wordFile, const char* allowedFile, bool flag) {
    swear_callee::EraseList(&mWords);
    swear_callee::EraseList(&mAllowed);
    if (!LoadWordList(wordFile, mWords, flag)) return false;
    LoadWordList(allowedFile, mAllowed, flag);
    return true;
}

// ---- stristr
uint16_t gSwearLowerMap[256];   // 0x103bcf9c, an import on the Mac; the ASCII mapping here
static const bool sLowerMapReady = [] {
    for (int i = 0; i < 256; ++i) gSwearLowerMap[i] = static_cast<uint16_t>((i >= 'A' && i <= 'Z') ? i + 32 : i);
    return true;
}();

// 0x1020cbec: a match that fails is not retried at the character after its start (the original's behaviour).
char16_t* stristr(char16_t* haystack, char16_t* needle) {
    const uint16_t first = *needle;
    char16_t* np = nullptr;
    char16_t* start = nullptr;
    if (first == 0) return haystack;
    for (; *haystack != 0; ++haystack) {
        uint16_t ch = *haystack;
        if (ch < 0x100) ch = gSwearLowerMap[ch];
        if (np == nullptr) {
            uint16_t f = first;
            if (f < 0x100) f = gSwearLowerMap[f];
            if (ch == f) {
                np = needle;
                start = haystack;
            }
        } else {
            uint16_t n = *np;
            if (n == 0) return start;
            if (n < 0x100) n = gSwearLowerMap[n];
            if (ch != n) {
                np = nullptr;
                start = nullptr;
            }
        }
        if (np != nullptr) ++np;
    }
    if (np != nullptr && *np == 0) return start;
    return nullptr;
}

// ---- IPBase::ShowError
const int16_t* gIPErrorCodes;         // 0x10396d1c
const char* const* gIPErrorTexts;     // 0x10396d18
__attribute__((weak)) int32_t ip_callee::WSAGetLastError() { return 0; }

// 0x1020cac8: the text of the last winsock error in a message box (the first text when the number is not listed: the ending 0's)
void IPBase::ShowError() {
    int32_t error = ip_callee::WSAGetLastError();
    int32_t i = 0;
    for (;; ++i) {
        int16_t code = gIPErrorCodes[i];
        if (error == code || code == 0) break;
    }
    NS_MacDoze::MessageBoxA(nullptr, gIPErrorTexts[i], "IP Error", 0);
}

// ---- FTPClient::ProcessEvents
__attribute__((weak)) void ftp_callee::ControlCallback(CFTPControl*, int32_t, int32_t) {}

// 0x1020d2cc
bool SwearFilter::Filter(std::u16string& text, bool replace) {
    bool result = false;                  // r25
    char16_t* p26;
    char16_t* p22;
    char16_t* p21;
    const char16_t* w;                    // r31
    int32_t mode;                         // r30
    bool found;
    for (auto word = mWords.begin(); word != mWords.end(); ++word) {
        w = word->data();
        mode = 1;
        if (*w == 0x20) {
            mode = 0;
            ++w;
        } else if (*w == 0x2a) {
            mode = 2;
            ++w;
        }
        p22 = &text[0];
        p26 = p22;
        for (;;) {
            if (p26 == nullptr || *w == 0) break;
            p26 = stristr(p22, const_cast<char16_t*>(w));
            if (p26 == nullptr) continue;
            p21 = p26;
            if (mode == 1) {
                while (p26 != p22 && !IsSeparator(*p26)) --p26;
                if (IsSeparator(*p26)) ++p26;
                goto word_start;
            }
            if (mode == 0) goto word_start;
            if (mode >= 3) continue;
            // mode 2: anywhere
            found = false;
            for (auto a = mAllowed.begin(); a != mAllowed.end(); ++a) {
                if (NS_MacDoze::_wcsnicmp(p26, a->data(), static_cast<uint32_t>(a->size())) == 0) {
                    found = true;
                    break;
                }
            }
            if (found) {
                for (const char16_t* t = w; *t != 0; ++t) ++p26;
            } else {
                result = true;
                for (const char16_t* t = w; *t != 0; ++t) {
                    if (replace) *p26 = u'*';
                    ++p26;
                }
            }
            p22 = p26;
            continue;

        word_start:
            if (p26 != p22 && !IsSeparator(p26[-1])) {
                // not at the start of a word: skipped
                while (p26 != p21) ++p26;
                for (const char16_t* t = w; *t != 0; ++t) ++p26;
                p22 = p26;
                continue;
            }
            found = false;
            for (auto a = mAllowed.begin(); a != mAllowed.end(); ++a) {
                if (NS_MacDoze::_wcsnicmp(p26, a->data(), static_cast<uint32_t>(a->size())) == 0) {
                    found = true;
                    break;
                }
            }
            if (found) {
                while (p26 != p21) ++p26;
                for (const char16_t* t = w; *t != 0; ++t) ++p26;
                p22 = p26;
                continue;
            }
            result = true;
            while (p26 != p21) {
                if (replace) *p26 = u'*';
                ++p26;
            }
            for (const char16_t* t = w; *t != 0; ++t) {
                if (replace) *p26 = u'*';
                ++p26;
            }
            while (*p26 != 0 && !IsSeparator(*p26)) {
                if (replace) *p26 = u'*';
                ++p26;
            }
            p22 = p26;
        }
    }
    return result;
}

// 0x1020e358: shut the socket down for writing, read until recv answers 0 or less, close it.
void Sock::Close() {
    if (mSocket != -1) {
        char buffer[256];
        ftp_callee::Shutdown(mSocket, 1);
        while (ftp_callee::Recv(mSocket, buffer, 0x100, 0) > 0) {
        }
        ftp_callee::CloseSocket(mSocket);
        mSocket = -1;
    }
}
