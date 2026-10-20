#include <cstdint>
#include <cstring>
#include <cstdio>
#include <list>
#include <string>
#include "email.h"
#include "Ptr32.h"
#include "platform/macdoze.h"

// Outbox::Save (0x1020461c) and Outbox::WriteMailData (0x10205154).
namespace {

const char kOutboxFile[] = ":outbox.dat";            // 0x1024fdd8
const char kFailure[] = "ios_base failure in clear";   // 0x1024fde4
const char kNumberLine[] = "%d\n";                     // 0x1024fdfe
const char kNewline[] = "\n";                          // 0x1024fe02
const char kDatName[] = ":%08x.dat";                   // 0x1024fe04
const char kJpgName[] = ":%08x.jpg";                   // 0x1024fe0e
const char kNumber[] = "%d";                           // 0x1024fe18

// The stream's state gets the fail bit (and the bad bit when it has no buffer); the exception is raised when the state meets the mask.
void Fail(SaveStream& out) {
    out.ios->mState |= 4;
    if (out.ios->mRdbuf == 0) out.ios->mState |= 1;
    if ((out.ios->mState & out.ios->mMask) != 0) SaveStreamThrow(kFailure);
}

}  // namespace

void Outbox::Save() {
    char line[128];
    SaveStream out;
    SaveStreamCtor(&out);
    std::string name;
    name.assign(mF00);
    name.append(kOutboxFile, std::strlen(kOutboxFile));
    if (!SaveStreamOpen(&out, name.c_str())) Fail(out);
    if (out.filebufOpen == 0) {
        SaveStreamDestroy(&out);
        return;
    }
    for (uint32_t value : mF28) {
        std::sprintf(line, kNumberLine, static_cast<int>(value));
        SaveStreamWrite(&out, line);
    }
    SaveStreamWrite(&out, kNewline);
    for (uint32_t value : mF40) {
        std::sprintf(line, kNumberLine, static_cast<int>(value));
        SaveStreamWrite(&out, line);
    }
    SaveStreamWrite(&out, kNewline);
    for (uint32_t value : mF34) {
        std::sprintf(line, kNumberLine, static_cast<int>(value));
        SaveStreamWrite(&out, line);
    }
    if (!SaveStreamClose(&out)) Fail(out);
    SaveStreamDestroy(&out);
}

int32_t Outbox::WriteMailData() {
    char line[128];
    SaveStream out;
    std::string datName;   // Mac stack +0x134
    std::string jpgName;   // Mac stack +0x130
    SaveStreamCtor(&out);
    // The next number is one more than the last of mF28 (0 when it is empty).
    int32_t number = mF28.empty() ? 0 : static_cast<int32_t>(mF28.back()) + 1;
    datName.assign(mF00);
    std::sprintf(line, kDatName, static_cast<unsigned>(number));
    datName.append(line, std::strlen(line));
    if (!SaveStreamOpen(&out, datName.c_str())) Fail(out);
    if (out.filebufOpen == 0) {
        SaveStreamDestroy(&out);
        return 0;
    }
    int32_t seconds = outboxwrite_callee::Time();
    const int32_t* tm = outboxwrite_callee::LocalTime(&seconds);
    std::memcpy(mF1C + 0, &tm[3], 4);                   // +0x1c  tm_mday
    int32_t month = tm[4] + 1;
    std::memcpy(mF1C + 4, &month, 4);                   // +0x20  tm_mon + 1
    int32_t year = tm[5] + 0x76c;
    std::memcpy(mF1C + 8, &year, 4);                    // +0x24  tm_year + 1900
    int32_t fields[3];
    std::memcpy(fields, mF1C, 12);
    for (int i = 0; i < 3; i++) {
        std::sprintf(line, kNumber, fields[i]);
        SaveStreamWrite(&out, line);
        SaveStreamWrite(&out, kNewline);
    }
    SaveStreamWrite(&out, mF04.c_str());
    SaveStreamWrite(&out, kNewline);
    SaveStreamWrite(&out, mF08.c_str());
    SaveStreamWrite(&out, kNewline);
    for (const std::string& text : mF0C) {
        SaveStreamWrite(&out, text.c_str());
        SaveStreamWrite(&out, kNewline);
    }
    if (!SaveStreamClose(&out)) Fail(out);
    jpgName.assign(mF00);
    std::sprintf(line, kJpgName, static_cast<unsigned>(number));
    jpgName.append(line, std::strlen(line));
    NS_MacDoze::MoveFile(mF18.c_str(), jpgName.c_str());
    mF18.assign(jpgName);
    mF28.push_back(static_cast<uint32_t>(number));
    mF34.push_back(static_cast<uint32_t>(number));
    mF34.sort();
    mF34.unique();
    Save();
    SaveStreamDestroy(&out);
    return 1;
}
