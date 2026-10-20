#include "TbIResourceIndex.h"
#include "platform/ltms.h"
#include "ltms_file_fakes.h"
#include <new>
#include <cstdlib>

namespace {
class Resource : public TbResource {
public:
    ~Resource() override { ev("~Resource", {lab(this)}); }
    int32_t Release() override { ev("Release", {lab(this)}); return 0; }
    void AddRef() override { ev("AddRef", {lab(this)}); }
    int32_t ManagerRelease() override { ev("ManagerRelease", {lab(this)}); return 0; }
    void ManagerAddRef() override { ev("ManagerAddRef", {lab(this)}); }
} resource;
TbError Create(const TbResourceFactory* p, CFile& file, void** out, uint32_t id, TbIResourceIndex* index) {
    ev("Create", {lab(p), lab(&file), id, lab(index)});
    *out = &resource;
    return TbError(static_cast<int32_t>(ret("Create")));
}
}
#define FACTORY(Name) TbError Name::CreateInterface(CFile& f, void** o, uint32_t id, TbIResourceIndex* i) const { return Create(this, f, o, id, i); }
FACTORY(MBToUnicodeFactory)
FACTORY(UnicodeToMBFactory)
FACTORY(MBInfoFactory)
FACTORY(TableFactory)
FACTORY(RLE4bitFontFactory)
#undef FACTORY

int32_t NS_MacDoze::InterlockedIncrement(int32_t* p) {
    ev("Increment", {*p});
    return ++*p;
}
int32_t NS_MacDoze::InterlockedDecrement(int32_t* p) {
    ev("Decrement", {*p});
    return --*p;
}

extern "C" {
void* tpw_index_new() { return new TbIResourceIndex; }
void* tpw_index_storage(TbIResourceIndex* p) { return &p->mStorage; }
void* tpw_index_resource() { return &resource; }
void* tpw_index_factory(int32_t n) { return gLtmsResourceFactories[n]; }
void tpw_index_seed(TbIResourceIndex* p, const uint32_t* words, uint32_t count,
                    const uint8_t* data, uint32_t size) {
    p->mItems.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        p->mItems[i] = {words[i * 4], uint16_t(words[i * 4 + 1]), uint16_t(words[i * 4 + 2]),
                       words[i * 4 + 3] ? &resource : nullptr};
    }
    p->mFirst = p->mItems.data();
    p->mLast = p->mFirst + count - 1;
    p->mData = new uint8_t[size];
    std::memcpy(p->mData, data, size);
}
int32_t tpw_index_call(TbIResourceIndex* p, int32_t op, int32_t slot, uint32_t id,
                       const uint8_t* raw, uint32_t count, int32_t* out) {
    void* result = reinterpret_cast<void*>(uintptr_t(7));
    auto* item = slot < 0 ? nullptr : &p->mItems[slot];
    int32_t value = 0;
    p->mReferences = static_cast<int32_t>(ret("References", p->mReferences));
    switch (op) {
    case 0: new (p) TbIResourceIndex; break;
    case 1: value = p->GetResource(item, &result); break;
    case 2: value = p->OpenResource(item, &result); break;
    case 3: {
        CFile f;
        value = p->OpenResourceFile(f, item);
        break;
    }
    case 4: {
        auto* b = new uint8_t[count * 12 + 4];
        std::memcpy(b, raw, count * 12);
        tpw_cl_label(4, b);
        value = p->Initialise("folder:", b, count);
        break;
    }
    case 5: value = p->GetResourceType(item); break;
    case 6: {
        auto* found = static_cast<TbIResourceIndex::resourceIndexItem*>(p->GetResourceItemHandle(id));
        value = found ? int32_t(found - p->mFirst) : -1;
        break;
    }
    case 7: new (&resource) Resource; break;
    case 8:
        if (slot < 0) p->mFirst = nullptr;
        p->Close();
        break;
    case 9: p->~TbIResourceIndex(); break;
    }
    *out = lab(result);
    return value;
}
void tpw_index_destroy(TbIResourceIndex* p, int32_t flag, uint32_t* state) {
    p->mReferences = static_cast<int32_t>(ret("References", p->mReferences));
    const auto count = p->mItems.size();
    const auto* first = p->mItems.data();
    p->~TbIResourceIndex();
    state[0] = p->mFirst != nullptr;
    state[1] = p->mReferences;
    state[2] = p->mLast ? static_cast<uint32_t>(p->mLast - first) : 0xfffffffeu;
    if (flag > 0) ::operator delete(p);
    (void)count;
}
void tpw_index_read(TbIResourceIndex* p, uint32_t* out) {
    out[0] = p->mFirst != nullptr;
    out[1] = p->mReferences;
    for (size_t i = 0; i < p->mItems.size(); ++i) {
        auto& item = p->mItems[i];
        out[2 + i * 4] = item.mId;
        out[3 + i * 4] = item.mType;
        out[4 + i * 4] = item.mFilenameOffset;
        out[5 + i * 4] = lab(item.mResource);
    }
}
uint32_t tpw_index_count(TbIResourceIndex* p) { return p->mItems.size(); }
int32_t tpw_index_last(TbIResourceIndex* p) {
    return p->mLast ? static_cast<int32_t>(p->mLast - p->mItems.data()) : -2;
}
int32_t tpw_resource_call(int32_t op, int32_t references, int32_t* after) {
    auto* p = new Resource;
    p->mReferences = references;
    tpw_cl_label(3, p);
    int32_t value = 0;
    if (op == 0) p->TbResource::AddRef();
    if (op == 1) value = p->TbResource::Release();
    if (op == 2) p->TbResource::ManagerAddRef();
    if (op == 3) value = p->TbResource::ManagerRelease();
    *after = (op == 1 || op == 3) && references == 1 ? 0 : p->mReferences;
    return value;
}
}
