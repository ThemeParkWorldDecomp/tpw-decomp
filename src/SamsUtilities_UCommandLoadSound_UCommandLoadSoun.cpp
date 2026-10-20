// SamsUtilities::UCommandLoadSound constructor (0x1002ac30) and destructor with the hidden delete flag (0x1002ab8c).
#include "CSprocketsModule.h"

namespace SamsUtilities {

UCommandLoadSound::UCommandLoadSound(const UReferenceCounter<USoundDescription>& description) {
    UCommand_Construct(reinterpret_cast<UCommand*>(this));
    mVtable4 = 0x1003dfdc;
    USoundDescriptionCounter_Copy(&mDescription, &description);
}

UCommandLoadSound* UCommandLoadSound::Destruct(int16_t deleteFlag) {
    if (this != nullptr) {
        mVtable4 = 0x1003dfdc;
        USoundDescriptionCounter_Destruct(&mDescription, -1);
        if (deleteFlag > 0) USoundChannel_Free(this);
    }
    return this;
}

}  // namespace SamsUtilities
