// email.cpp neighbour (main app): Argument's constructor and destructor (unit v5138).
#include <cstdint>
#include <list>
#include <string>

#include "email.h"
#include "online.h"

// 0x102011a8: the char string is made empty, the wide string is a copy of the argument (whole), the command is cleared.
Argument* Argument::Construct(Argument* self, const void* text) {
    char* base = reinterpret_cast<char*>(self);
    online_callee::StringCtor(base);                                    // 0x1007363c
    online_callee::WStringCopy(base + 4, text, 0, 0xffffffffu);         // 0x101f7424
    self->mCommand = 0;
    return self;
}

// 0x1020127c: a NULL object is left alone; the wide string then the char string are destroyed, and the memory is freed when the
// hidden flag (a signed short) is above 0.
Argument* Argument::Destruct(Argument* self, int16_t deleteFlag) {
    if (self != nullptr) {
        char* base = reinterpret_cast<char*>(self);
        online_callee::WStringDtor(base + 4, -1);   // 0x101468f8
        online_callee::StringDtor(base, -1);        // 0x1000221c
        if (deleteFlag > 0) online_callee::OperatorDelete(self);   // 0x10238218
    }
    return self;
}
