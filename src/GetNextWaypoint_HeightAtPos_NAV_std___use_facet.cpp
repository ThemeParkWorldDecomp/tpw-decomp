#include <cmath>
#include <cstddef>
#include <cstdint>
#include "HLSprite.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "std_basic_filebuf.h"

// std::__use_facet<F>(const std::locale&): the locale holds the impl pointer; the impl holds a std::vector<facet*>
// (+4 count, +8 data). The pointers are native here (4 bytes on the Mac), so the data slots are 8 bytes wide.
namespace {

// The id of a facet class: assigned on first use from the shared counter.
inline void AssignId(uint32_t* idp) {
    if (*idp == 0) {
        const uint32_t next = gLocaleIdCount + 1;
        gLocaleIdCount = next;
        *idp = next;
    }
}

// After the facet is built: grow the vector, release the old entry, store the new facet (reference count + 1).
inline UseFacetObj* Install(const UseFacetLocale* locale, uint32_t* idp, UseFacetObj* facet, uint32_t id, bool useInsert) {
    UseFacetImp* imp = locale->mImp;
    AssignId(idp);
    const uint32_t slot = *idp;
    if (slot >= imp->mCount) {
        if (useInsert) {
            void* zero = nullptr;
            const uint32_t want = slot + 1;
            const uint32_t have = imp->mCount;
            if (have < want) {
                use_facet_callee::VecInsert(imp, imp->mData + have, want - have, &zero);
            } else if (want < have) {
                use_facet_callee::VecErase(imp, imp->mData + want, imp->mData + have);
            }
        } else {
            use_facet_callee::VecResize(imp, slot + 1, nullptr);
        }
    }
    void** entry = imp->mData + slot;
    UseFacetObj* old = static_cast<UseFacetObj*>(entry[0]);
    if (old != nullptr) {
        const int32_t refs = old->mRefs - 1;
        old->mRefs = refs;
        if (refs != 0) old = nullptr;
        if (old != nullptr) use_facet_callee::FacetDelete(old, 1);
    }
    facet->mRefs = facet->mRefs + 1;
    entry[0] = facet;
    return static_cast<UseFacetObj*>(locale->mImp->mData[id]);
}

// The lookup shared by the four functions: the facet when the locale already holds one for the id.
inline UseFacetObj* Lookup(const UseFacetLocale* locale, uint32_t id) {
    if (id < locale->mImp->mCount) return static_cast<UseFacetObj*>(locale->mImp->mData[id]);
    return nullptr;
}

}  // namespace

// 0x10073b54
void* UseFacetCtypeChar(const UseFacetLocale* locale) {
    AssignId(&gIdCtypeChar);
    const uint32_t id = gIdCtypeChar;
    if (UseFacetObj* found = Lookup(locale, id)) return found;
    UseFacetObj* p = static_cast<UseFacetObj*>(use_facet_callee::New(0x10));
    if (p != nullptr) use_facet_callee::CtypeCharCtor(p, nullptr, false, 0);
    return Install(locale, &gIdCtypeChar, p, id, true);
}

// 0x100739a0
void* UseFacetCodecvtCharChar(const UseFacetLocale* locale) {
    AssignId(&gIdCodecvtCharChar);
    const uint32_t id = gIdCodecvtCharChar;
    if (UseFacetObj* found = Lookup(locale, id)) return found;
    UseFacetObj* p = static_cast<UseFacetObj*>(use_facet_callee::New(8));
    if (p != nullptr) use_facet_callee::CodecvtCharCtor(p, 0);
    return Install(locale, &gIdCodecvtCharChar, p, id, false);
}

// 0x102261a0 (the ctype<wchar_t> constructor is inline: two stores, the vtable word last)
void* UseFacetCtypeWchar(const UseFacetLocale* locale) {
    AssignId(&gIdCtypeWchar);
    const uint32_t id = gIdCtypeWchar;
    if (UseFacetObj* found = Lookup(locale, id)) return found;
    UseFacetObj* p = static_cast<UseFacetObj*>(use_facet_callee::New(8));
    if (p != nullptr) {
        p->mVtable = 0x1029d2a0;
        p->mRefs = 0;
        p->mVtable = 0x102b20bc;
    }
    return Install(locale, &gIdCtypeWchar, p, id, false);
}

// 0x102298e4
void* UseFacetCodecvtWcharChar(const UseFacetLocale* locale) {
    AssignId(&gIdCodecvtWcharChar);
    const uint32_t id = gIdCodecvtWcharChar;
    if (UseFacetObj* found = Lookup(locale, id)) return found;
    UseFacetObj* p = static_cast<UseFacetObj*>(use_facet_callee::New(8));
    if (p != nullptr) use_facet_callee::CodecvtWcharCtor(p, 0);
    return Install(locale, &gIdCodecvtWcharChar, p, id, true);
}
