// ---- bullfrog strings (functional, unrecorded). Blocks are never freed: the existing
// ctor(const char*) placeholder keeps the caller's pointer.
template<> TPW_WEAK TbDynamicStringTemplate<char>::TbDynamicStringTemplate() {
    mCapacity = 0;
    mData = kEmptyText;
    mLength = 0;
    mGrowBy = 0;
}
template<> TPW_WEAK TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(
    const TbDynamicStringTemplate& other) {
    mData = copy_string(other.mData);
    mLength = static_cast<int32_t>(length_of(mData));
    return *this;
}
template<> TPW_WEAK TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(
    const TbStringBase<char>& text) {
    const auto& other = static_cast<const TbDynamicStringTemplate<char>&>(text);
    size_t a = length_of(mData), b = length_of(other.mData);
    char* p = static_cast<char*>(std::malloc(a + b + 1));
    std::memcpy(p, mData, a);
    std::memcpy(p + a, other.mData, b + 1);
    mData = p;
    mLength = static_cast<int32_t>(a + b);
    return *this;
}
template<> TPW_WEAK TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() {
    mCapacity = 0;
    mData = kEmptyWide;
    mLength = 0;
    mGrowBy = 0;
}
template<> TPW_WEAK TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    mCapacity = 0;
    mData = copy_string(text);
    mLength = static_cast<int32_t>(length_of(mData));
    mGrowBy = 0;
}
template<> TPW_WEAK TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template<> TPW_WEAK TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(
    const char16_t* text) {
    mData = copy_string(text);
    mLength = static_cast<int32_t>(length_of(mData));
    return *this;
}

