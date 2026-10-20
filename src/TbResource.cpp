#include "TbResource.h"
#include "platform/ltms.h"

void TbResource::AddRef() {
    NS_MacDoze::InterlockedIncrement(&mReferences);
}
void TbResource::ManagerAddRef() {
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
