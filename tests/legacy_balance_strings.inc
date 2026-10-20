template<> TPW_WEAK TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    if (auto* event = balance_note(4, this)) {
        std::strncpy(event->mText, text, sizeof event->mText - 1);
    }
    mCapacity ^= static_cast<int32_t>(tpw_balance_mask);
    mData = const_cast<char*>(text);
    mLength = static_cast<int32_t>(std::strlen(text));
    mGrowBy ^= static_cast<int32_t>(tpw_balance_mask + mLength);
}
template<> TPW_WEAK TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {
    balance_note(5, this);
}

