// online.cpp (main app): COnlineControl chat commands Locate, Page, Goto, BlackMark and BuddyOn (unit v5023).
// Each builds a vector of two Arguments (the command number, then a copy of the name) and hands it to DoCommand.
#include <cstdint>
#include <string>

#include "online.h"

namespace {

// An Argument as the code builds it: char string word, wide string word, command number.
struct ArgumentWords {
    uint32_t mString;    // 0
    uint32_t mWString;   // 4
    int32_t mType;       // 8
};

void SendCommand(COnlineControl* self, int32_t type, std::u16string& name) {
    uint32_t vec[3] = {0, 0, 0};
    ArgumentWords first;
    online_callee::StringCtor(&first.mString);
    online_callee::WStringCtor(&first.mWString);
    first.mType = type;
    online_callee::ArgumentVectorPushBack(vec, &first);
    online_callee::WStringDtor(&first.mWString, -1);
    online_callee::StringDtor(&first.mString, -1);

    uint32_t copy;
    ArgumentWords second;
    online_callee::WStringCopy(&copy, &name, 0, 0xffffffffu);
    online_callee::ArgumentCtor(&second, &copy);
    online_callee::ArgumentVectorPushBack(vec, &second);
    online_callee::WStringDtor(&second.mWString, -1);
    online_callee::StringDtor(&second.mString, -1);
    online_callee::WStringDtor(&copy, -1);
    self->DoCommand(vec);
    online_callee::ArgumentVectorTearDown(vec);
}

}  // namespace

// 0x102184a0
void COnlineControl::BlackMark(std::u16string& name) { SendCommand(this, 0xe, name); }

// 0x1021864c
void COnlineControl::Page(std::u16string& name) { SendCommand(this, 0x11, name); }

// 0x102187f4
void COnlineControl::Locate(std::u16string& name) { SendCommand(this, 0x14, name); }

// 0x1021899c
void COnlineControl::Goto(std::u16string& name) { SendCommand(this, 0x13, name); }

// 0x10218c44
void COnlineControl::BuddyOn(std::u16string& name) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    uint32_t key;
    online_callee::NocaseWStringCopy(&key, &name, 0, 0xffffffffu);
    void* found = online_callee::BuddyTreeFind(self + 0xe34, &key);
    online_callee::NocaseWStringDtor(&key, 0);
    if (found == self + 0xe38) SendCommand(this, 0xf, name);
}
