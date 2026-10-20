#include "TbTextResourceManager.h"
#include "TbIResourceIndex.h"
#include "TbITable.h"
#include "ltms_file_fakes.h"
#include <new>

namespace {
TbResource resource;
TbIResourceIndex::resourceIndexItem item;
const TbTextResourceManager* destroyedManager;
int32_t destroyedState[2];
}
// Index calls are isolated here. Their implementations have their own differential tests.
TbIResourceIndex::TbIResourceIndex() : mFirst(nullptr), mLast(nullptr), mData(nullptr) {
    ev("Index", {newlab(this)});
}
TbIResourceIndex::~TbIResourceIndex() {}
void TbIResourceIndex::Close() {}
TbError TbIResourceIndex::Initialise(const char* path, void* data, uint32_t count) {
    ev("Initialise", {lab(this), count}, {path});
    mData = static_cast<uint8_t*>(data);
    return TbError(static_cast<int32_t>(ret("Initialise")));
}
void* TbIResourceIndex::GetResourceItemHandle(uint32_t id) {
    ev("Find", {lab(this), id});
    return ret("Handle", 1) ? &item : nullptr;
}
uint16_t TbIResourceIndex::GetResourceType(void* handle) {
    ev("Type", {lab(this), handle != nullptr});
    return static_cast<uint16_t>(ret("Type", 6));
}
TbError TbIResourceIndex::GetResource(void* handle, void** out) {
    ev("Get", {lab(this), handle != nullptr});
    *out = ret("Output", 1) ? &resource : nullptr;
    return TbError(static_cast<int32_t>(ret("Get")));
}
int32_t TbResource::Release() { ev("Release", {lab(this)}); return 0; }
void TbResource::AddRef() {}
int32_t TbResource::ManagerRelease() { return 0; }
void TbResource::ManagerAddRef() {}

extern "C" {
void* tpw_manager_new() { return new TbTextResourceManager; }
void* tpw_manager_storage(TbTextResourceManager* p) { return &p->mStorage; }
void* tpw_manager_resource() { return &resource; }
void tpw_manager_seed(TbTextResourceManager* p, int32_t opened, int32_t old,
                      const uint8_t* data, uint32_t size) {
    p->mOpened = opened;
    p->mIndex = old ? reinterpret_cast<TbIResourceIndex*>(&resource) : nullptr;
    fileBytes = data;
    fileLength = size;
    filePosition = 0;
}
int32_t tpw_manager_call(TbTextResourceManager* p, int32_t op, uint32_t id, int32_t* out) {
    destroyedManager = nullptr;
    void* result = reinterpret_cast<void*>(uintptr_t(7));
    int32_t value = 0;
    switch (op) {
    case 0: value = p->Open("folder:"); break;
    case 1: value = p->SetLanguage("language"); break;
    case 2: value = p->LoadResourceIndex("folder:"); break;
    case 3: value = p->GetResource(reinterpret_cast<TbIMBToUnicode**>(&result), id); break;
    case 4: value = p->GetResource(reinterpret_cast<TbIUnicodeToMB**>(&result), id); break;
    case 5: value = p->GetResource(reinterpret_cast<TbITable**>(&result), id); break;
    case 6: value = p->GetResource(reinterpret_cast<TbIFont**>(&result), id); break;
    case 7: new (p) TbTextResourceManager; break;
    case 8: value = p->Close(); break;
    case 9:
        p->~TbTextResourceManager();
        destroyedState[0] = p->mOpened;
        destroyedState[1] = lab(p->mIndex);
        destroyedManager = p;
        if ((id & 3) > 1) ::operator delete(p);
        break;
    }
    *out = lab(result);
    return value;
}
void tpw_manager_read(TbTextResourceManager* p, int32_t* out) {
    if (p == destroyedManager) {
        std::memcpy(out, destroyedState, sizeof(destroyedState));
        return;
    }
    out[0] = p->mOpened;
    out[1] = lab(p->mIndex);
}
}
