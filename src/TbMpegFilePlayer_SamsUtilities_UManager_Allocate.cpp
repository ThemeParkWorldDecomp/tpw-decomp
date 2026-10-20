#include "CMpegBase.h"

namespace SamsUtilities {

// 0x1002bc7c: the object is only initialised when the allocation succeeded; the pointer is stored either way.
template <>
void UManager<UAllocator>::Allocate() {
    void* object = UAllocatorMake(8);
    if (object != nullptr) UAllocatorInit(object);
    mObject = object;
}

}  // namespace SamsUtilities
