// OnlineChatData constructor 0x1021e068 and destructor 0x10213cac (online.cpp).
#include "online.h"

OnlineChatData* OnlineChatData::Construct(OnlineChatData* self) {
    online_callee::WStringCtor(&self->mString0);
    online_callee::WStringCtor(&self->mString4);
    chatdata_callee::TreeWStringCtor(&self->mTreeA);
    chatdata_callee::TreeBuddyCtor(&self->mTreeB);
    chatdata_callee::TreeNocaseCtor(&self->mTreeC);
    self->mFlag30 = 0;
    self->mFlag31 = 0;
    chatdata_callee::ListCtor(&self->mList);
    return self;
}

OnlineChatData* OnlineChatData::Destruct(OnlineChatData* self, int16_t deleteFlag) {
    if (self == nullptr) return self;
    chatdata_callee::ListClear(&self->mList);
    if (self->mTreeC[1] != 0) chatdata_callee::TreeNocaseDestroy(&self->mTreeC, self->mTreeC[1]);
    if (self->mTreeB[1] != 0) chatdata_callee::TreeBuddyDestroy(&self->mTreeB, self->mTreeB[1]);
    if (self->mTreeA[1] != 0) chatdata_callee::TreeWStringDestroy(&self->mTreeA, self->mTreeA[1]);
    online_callee::WStringDtor(&self->mString4, -1);
    online_callee::WStringDtor(&self->mString0, -1);
    if (deleteFlag > 0) online_callee::OperatorDelete(self);
    return self;
}
