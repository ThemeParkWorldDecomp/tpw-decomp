// OnlineConfig::~OnlineConfig (main 0x10003508, 892 B; the out-of-line copy of the destructor that follows FS_Registry's code).
// The host OnlineConfig holds real std::strings (their destructors are the implicit ones), so this is the Mac's destructor written over
// the Mac's object seen as an array of string words (Mac offset / 4): the strings at +0 +4 +8 +0xc +0x10 +0x14 and +0x24, each the
// address of an MSL string rep (len +0, cap +4, refs +8, data +0xc).
#include <cstdint>

#include "FS_Online.h"
#include "std_basic_filebuf.h"

namespace {

// One string member: the reference count is raised to 1 when 0, lowered by one; the rep is kept only when it reached 0 (otherwise the
// member is cleared and nothing is freed); a kept rep frees its data and itself.
void ReleaseString(MslCharString* s) {
    MslCharRep* rep = s->mRep;
    if (rep->refs == 0) rep->refs = 1;
    rep->refs = rep->refs - 1;
    if (rep->refs != 0) rep = nullptr;
    s->mRep = rep;
    if (s->mRep != nullptr) {
        online_config_callee::OperatorDelete(s->mRep->data);
        online_config_callee::OperatorDelete(s->mRep);
    }
}

}  // namespace

namespace online_config_mac {

// 0x10003508: members in the order +0x24, +0x14, +0x10, +0xc, +8, +4, +0, then (flag above 0 as a signed 16 bit) delete this. Answers this.
MslCharString* Destroy(MslCharString* self, int16_t flag) {
    if (self != nullptr) {
        ReleaseString(self + 9);
        ReleaseString(self + 5);
        ReleaseString(self + 4);
        ReleaseString(self + 3);
        ReleaseString(self + 2);
        ReleaseString(self + 1);
        ReleaseString(self + 0);
        if (flag > 0) online_config_callee::OperatorDelete(self);
    }
    return self;
}

}  // namespace online_config_mac
