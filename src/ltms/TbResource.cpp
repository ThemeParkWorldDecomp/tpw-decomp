// TbResource.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbResource code, named after it.

#include "platform/ltms.h"
#include "TbResource.h"

void TbResource::AddRef() {
    NS_MacDoze::InterlockedIncrement(&mReferences);
}
int32_t TbResource::Release() {
    if (NS_MacDoze::InterlockedDecrement(&mReferences) == 0) {
        delete this;
        return 0;
    }
    return mReferences;
}
int32_t TbResource::ManagerRelease() {
    if (NS_MacDoze::InterlockedDecrement(&mReferences) == 0) {
        delete this;
        return 0;
    }
    return mReferences;
}
void TbResource::ManagerAddRef() {
    NS_MacDoze::InterlockedIncrement(&mReferences);
}
