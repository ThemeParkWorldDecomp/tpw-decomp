// Sound-library containers and file-record swaps, sound 0x1001f410..0x10022e48.
#include "TbMapStreamer.h"
#include <cstring>

namespace {
// Files hold little-endian fields. The test build forces swaps to check their exact offsets.
template<class T> void file_swap(T& value) {
#if defined(TPW_SWAP_FILE_WORDS) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    if constexpr (sizeof(T) == 2) value = sound::SamsUtilities::USwap16(value);
    else value = sound::SamsUtilities::USwap32(value);
#endif
}
}

bool TbGUID::operator==(const TbGUID& other) const {
    return mData1 == other.mData1 && mData2 == other.mData2 && mData3 == other.mData3 &&
        std::memcmp(mData4, other.mData4, 8) == 0;
}
void TbGUID::Byteswap() {
    file_swap(mData1);
    file_swap(mData2);
    file_swap(mData3);
}
void CFileHeader::Byteswap() {
    mGUID.Byteswap();
    file_swap(mVersion);
}
void CBankDataMap::Byteswap() {
    file_swap(mCount);
    file_swap(mBanks.mOffset);
}
void CSFXDataMap::Byteswap() {
    file_swap(mCount);
    file_swap(mCatagories.mOffset);
}
void CCatagoryData::Byteswap() {
    file_swap(mEventCount);
    file_swap(mEvents.mOffset);
    file_swap(mF08);
    file_swap(mFlags);
    file_swap(mF0C);
    file_swap(mF10);
    file_swap(mF14);
}
void CEventData::Byteswap() {
    file_swap(mID);
    file_swap(mSoundCount);
    file_swap(mSounds.mOffset);
    file_swap(mF0C);
    file_swap(mFlags);
    file_swap(mF12);
}
void CSoundData::Byteswap() {
    file_swap(mSampleCount);
    file_swap(mBranchCount);
    file_swap(mSamples.mOffset);
    file_swap(mF10);
    file_swap(mF12);
    file_swap(mF16);
    file_swap(mF18);
    file_swap(mF1A);
    file_swap(mF1C);
    file_swap(mF1E);
    file_swap(mF22);
    file_swap(mBranches.mOffset);
}
void CSampleData::Byteswap() {
    file_swap(mF00);
    file_swap(mF04);
    file_swap(mF08);
    file_swap(mBankIndex);
    file_swap(mF0E);
}
void CSoundBranchData::Byteswap() {
    file_swap(mSound.mOffset);
    file_swap(mF04);
}

CCatagoryContainer::CCatagoryContainer(uint32_t index)
    : mData(nullptr), mIndex(index), mElements(nullptr), mName() {}
CCatagoryContainer::~CCatagoryContainer() {
    TbMapStreamer::Instance()->UnloadCatagory(this);
    if (mData != nullptr) {
        tpw::ArenaFree(mData);
        mData = nullptr;
    }
    if (mElements != nullptr) {
        mElements->Release();
        mElements = nullptr;
    }
}
bool CCatagoryContainer::Find(uint32_t value) {
    for (CElement* p = mElements; p != nullptr; p = p->mNext) {
        if (p->mValue == value) return true;
    }
    return false;
}
void CCatagoryContainer::Add(uint32_t value) {
    mElements = new (std::nothrow) CElement(mElements, value);
}
void CCatagoryContainer::CElement::Release() {
    if (mNext != nullptr) mNext->Release();
    delete this;
}
CBankContainer::CBankContainer(const CBankData& bank)
    : mBank(nullptr), mName(bank.mName), mFlags(bank.mFlags), mStorage() {}
