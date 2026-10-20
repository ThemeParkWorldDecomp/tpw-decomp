// Unused virtual string slots for isolated main-app tests. These are functional fakes.
#pragma once
template<class Char> void TbDynamicStringTemplate<Char>::Format(const Char*, ...) {}
template<class Char> void TbDynamicStringTemplate<Char>::AllocBuffer(uint32_t length) {
    mLength = length;
    mCapacity = length;
    mGrowBy = 8;
    mData = static_cast<Char*>(std::calloc(length + 1, sizeof(Char)));
}
template<class Char> void TbDynamicStringTemplate<Char>::ResizeTo(uint32_t length) {
    Char* data = static_cast<Char*>(std::calloc(length + 1, sizeof(Char)));
    std::memcpy(data, mData, std::min(length, static_cast<uint32_t>(mLength)) * sizeof(Char));
    mData = data;
    mLength = length;
    mCapacity = length;
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char& ch) {
    const Char text[2] = {ch, 0};
    return operator=(text);
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char& ch) {
    const Char text[2] = {ch, 0};
    return operator+=(text);
}
