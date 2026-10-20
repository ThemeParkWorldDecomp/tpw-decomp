#include <cstdint>
#include <cstring>
#include <cstdio>
#include <list>
#include <string>
#include "email.h"
#include "Ptr32.h"
#include "platform/macdoze.h"

// Outbox::DeleteMail 0x1020638c: deletes "<mF00>:%08x.dat" and "<mF00>:%08x.jpg" for the mail id.
// The TOC slot -0x3b00 (0x1024faeb) + 0x319 and + 0x323 hold the two formats.
void Outbox::DeleteMail(int32_t mailId) {
    char buffer[0x80];
    std::string name;
    name.assign(mF00, 0, std::string::npos);
    std::sprintf(buffer, ":%08x.dat", static_cast<unsigned>(mailId));
    name.append(buffer, std::strlen(buffer));
    outboxdel_callee::Unlink(name.c_str());
    name.assign(mF00, 0, std::string::npos);
    std::sprintf(buffer, ":%08x.jpg", static_cast<unsigned>(mailId));
    name.append(buffer, std::strlen(buffer));
    outboxdel_callee::Unlink(name.c_str());
}
