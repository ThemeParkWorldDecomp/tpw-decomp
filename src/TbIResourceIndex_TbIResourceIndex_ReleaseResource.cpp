#include "TbIResourceIndex.h"

// 0x100036d8: the handle is an item; its resource (Mac +8) is released through ManagerRelease, then cleared.
void TbIResourceIndex::ReleaseResource(void* handle) {
    auto* item = static_cast<resourceIndexItem*>(handle);
    if (item) {
        if (item->mResource) item->mResource->ManagerRelease();
        item->mResource = nullptr;
    }
}

// 0x1000362c
void TbIResourceIndex::ReleaseItem(resourceIndexItem* item) {
    if (item) {
        if (item->mResource) item->mResource->ManagerRelease();
        item->mResource = nullptr;
    }
}
