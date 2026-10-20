#include "email.h"
#include <cstdint>
#include <cstdlib>
#include <string>
#include "TPMain.h"
#include "platform/macdoze.h"

// EmailInfo::~EmailInfo 0x10203aec: the members go in reverse order (the string at +0x14, the list at +8, the strings at +4 and +0),
// then the object is deleted when the hidden flag is above 0. The list's address test in the listing (this + 8 is never 0) is
// always true.
EmailInfo* EmailInfo::Destruct(EmailInfo* self, int16_t deleteFlag) {
    if (self != nullptr) {
        email_callee::StringDtor(&self->mString14, -1);
        email_callee::ListClear(&self->mList08);
        email_callee::StringDtor(&self->mString04, -1);
        email_callee::StringDtor(&self->mString00, -1);
        if (deleteFlag > 0) email_callee::OperatorDelete(self);
    }
    return self;
}
