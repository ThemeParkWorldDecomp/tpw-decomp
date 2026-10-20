// std::_RefCountedPtr<std::__locale_imp<__int128>, std::_Single<...>>::operator= (0x10072128, 368 B).
#include "std_basic_filebuf.h"

LocaleRefPtr& LocaleRefPtr::operator=(const LocaleRefPtr& other) {
    using namespace locale_imp_callee;
    if (mImp != other.mImp) {
        if (mCount != nullptr) {
            int32_t left = *mCount - 1;
            *mCount = left;
            if (left == 0) {
                LocaleImpRef* impl = mImp;
                if (impl != nullptr) {
                    uint32_t n = VecSize(impl);
                    for (uint32_t i = 0; i < n; i++) {
                        LocaleFacetRef* facet = *VecIndex(impl, i);
                        if (facet != nullptr) {
                            int32_t refs = facet->mRefs - 1;
                            facet->mRefs = refs;
                            if (refs == 0) FacetDelete(facet, 1);
                        }
                    }
                    // The original tests impl + 0xc != 0 (always true for a real object).
                    impl->mCharArray = CharArrayRemoveRef(impl->mCharArray);
                    if (impl->mCharArray != nullptr) {
                        AllocDestroy(&impl->mCharArray, impl->mCharArray);
                        AllocDeallocate(&impl->mCharArray, impl->mCharArray, 1);
                    }
                    VecDtor(impl, -1);
                    Delete(impl);
                }
                Delete(mCount);
            }
        }
        mImp = other.mImp;
        mCount = other.mCount;
        if (mCount != nullptr) *mCount = *mCount + 1;
    }
    return *this;
}
