// online.cpp / OnlinePerson.cpp (main app), unit v51009:
//   VoteNotifyCallback 0x10213590, SwearFilter::Init 0x1020d1bc, stristr 0x1020cbec, IPBase::ShowError 0x1020cac8,
//   FTPClient::ProcessEvents 0x1020c8cc.
#include <cstdint>
#include <cstring>
#include <new>
#include <list>
#include <string>
#include "analyser.h"
#include "Debug.h"
#include "ThingReceivers.h"
#include "online.h"
#include "FS_Online.h"
#include "FS_MasterSave.h"
#include "WinScrn.h"

// ---- VoteNotifyCallback: the vote connection's callback; sends command 0x2711 and the first argument to the online control
// (made on first use) as a vector of two Arguments (three words each: char string, wide string, number).
static void PushArgument(uint32_t* vec, int32_t number) {
    uint32_t argument[3];
    online_callee::StringCtor(&argument[0]);
    online_callee::WStringCtor(&argument[1]);
    argument[2] = static_cast<uint32_t>(number);
    online_callee::ArgumentVectorPushBack(vec, argument);
    online_callee::WStringDtor(&argument[1], -1);
    online_callee::StringDtor(&argument[0], -1);
}

// 0x10213590 (the last two arguments are not read)
void VoteNotifyCallback(int32_t a, int32_t b, void* c, void* d) {
    uint32_t vec[3] = {0, 0, 0};
    PushArgument(vec, 0x2711);
    PushArgument(vec, a);
    if (gOnlineControl == nullptr) {
        void* block = online_callee::OperatorNew(0xe8c);
        if (block != nullptr) new (block) COnlineControl();
        gOnlineControl = static_cast<COnlineControl*>(block);
    }
    gOnlineControl->CallbackFn(0, vec);
    online_callee::ArgumentVectorTearDown(vec);
}

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

// 0x1020c8cc. The result keeps its value from the pass before when a state does nothing (the original's register).
void FTPClient::ProcessEvents() {
    bool stop = false;
    int32_t result = 0;
    int32_t& control = *reinterpret_cast<int32_t*>(mF04);   // Mac 0x04: the control socket
    while (!stop) {
        ftp_callee::CondWait(mCondA, -1);
        const int32_t state = mF276c;
        mReady = 1;
        if (state == 2) {
            if (control == -1) {
                result = 0x38;
            } else {
                ftp_callee::SendCommandLine(this, "QUIT\r\n");
                const int32_t code = ftp_callee::RecvResponse(this, nullptr, 0, true);
                if (code == 0 || code == 0xdd) {
                    ftp_callee::SockClose(this);
                    if (mListenSocket != -1) {
                        ftp_callee::Shutdown(mListenSocket, 1);
                        while (ftp_callee::Recv(mListenSocket, mBuffer, 0x400, 0) > 0) {
                        }
                        ftp_callee::CloseSocket(mListenSocket);
                        mListenSocket = -1;
                    }
                    result = 0;
                } else {
                    result = 0x43;
                }
            }
        } else if (state < 2) {
            if (state == 0) {
                stop = true;
                result = 0;
            } else if (state > 0) {
                result = _Connect();
            }
        } else if (state < 4) {
            result = _Download();
        }
        ftp_callee::ControlCallback(mControl, state, result);
    }
}
