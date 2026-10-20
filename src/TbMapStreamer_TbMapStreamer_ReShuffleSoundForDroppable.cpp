// TbMapStreamer: ReShuffleSoundForDroppable 0x1002309c (276) and SaveCatgoryFixupTable 0x100233e0 (576), unit v5047.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "TbError.h"
#include "TbMapStreamer.h"
#include "TbSoundBank.h"
#include "TbSoundManager.h"
#include "platform/samsutils.h"

// 0x1002309c: the droppable samples go to the end of the list. The count word first loses its high half (<< 16) and then gets
// the low half added back sign-extended, as the listing does (the word ends as (count << 16) + (int16_t)count).
void TbMapStreamer::ReShuffleSoundForDroppable(CSoundData* sound) {
    const uint32_t count = sound->mSampleCount;
    CSampleData* first = sound->mSamples.get();
    sound->mSampleCount = count << 16;
    CSampleData* last = first + (static_cast<int32_t>(count & 0xffff) - 1);
    sound->mSampleCount = sound->mSampleCount + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(count)));
    while (first <= last) {
        if (mpSoundManager->IsBankDroppable(first->mBankIndex)) {
            const CSampleData saved = *first;
            *first = *last;
            *last = saved;
            --last;
            sound->mSampleCount = sound->mSampleCount - 1;
        } else {
            ++first;
        }
    }
}

// 0x100233e0: the table is written as: position, count (twice, the original's bug), one zero word per category, then per
// category a length byte (0 for none, else the name's length + 1) and the name with ':' turned into '\'; then the byte size
// of everything after the first two words is patched in at the start and the position goes back to the end.
TbError TbMapStreamer::SaveCatgoryFixupTable(bfl::FileSystem::CFile* file) {
    const int32_t start = file->GetPos();
    const uint32_t count = mCatagories.Size();
    uint32_t word = ::SamsUtilities::USwap32<uint32_t>(count);   // the file is little-endian: native words on the host
    file->Write(&word, 4);
    file->Write(&word, 4);
    for (uint32_t i = 0; i < count; ++i) {
        word = 0;
        file->Write(&word, 4);
    }
    for (uint32_t i = 0; i < mCatagories.Size(); ++i) {
        CCatagoryContainer* category = mCatagories[i];
        uint8_t length = 0;
        if (category != nullptr) length = static_cast<uint8_t>(TbMapStreamerFixup::NameLength(category->mName) + 1);
        file->Write(&length, 1);
        if (length != 0) {
            // Room for a host UString (24 bytes, the text at +16 where StringText reads it; the Mac's is 12 bytes, v5L12).
            alignas(SamsUtilities::UString) uint8_t ustring[sizeof(SamsUtilities::UString)];
            TbMapStreamerFixup::StringInit(ustring, TbMapStreamerFixup::NameText(category->mName));
            TbMapStreamerFixup::StringReplace(ustring, ':', '\\', 0);
            file->Write(TbMapStreamerFixup::StringText(ustring), length);
            TbMapStreamerFixup::StringDestroy(ustring, -1);
        }
    }
    const int32_t end = file->GetPos();
    file->Seek(static_cast<uint32_t>(start), 0);
    word = ::SamsUtilities::USwap32<uint32_t>(static_cast<uint32_t>(end - start - 8));
    file->Write(&word, 4);
    file->Seek(static_cast<uint32_t>(end), 0);
    return TbError(0);
}

// Default bodies of the string helpers (the unit test links its own recording fakes instead). The text lives in the
// UString's mText, where the translated operator char* (StringText, src/TbMapStreamer_TbMapStreamer_FixUpCatagoryIndex.cpp)
// reads it; these bodies kept it at +0, so the save wrote from a stray pointer once v51113 landed (v5L12).
namespace TbMapStreamerFixup {
namespace {
char*& Text(void* ustring) { return static_cast<SamsUtilities::UString*>(ustring)->mText; }
}  // namespace
__attribute__((weak)) uint32_t NameLength(const TbDynamicStringTemplate<char>& name) {
    return static_cast<uint32_t>(std::strlen(NameText(name)));
}
__attribute__((weak)) const char* NameText(const TbDynamicStringTemplate<char>& name) { return name.mData; }
__attribute__((weak)) void StringInit(void* ustring, const char* text) {
    char* copy = static_cast<char*>(std::malloc(std::strlen(text) + 1));
    std::strcpy(copy, text);
    Text(ustring) = copy;
}
__attribute__((weak)) void StringReplace(void* ustring, int32_t from, int32_t to, int32_t flag) {
    (void)flag;
    char* text = Text(ustring);
    for (; *text != 0; ++text)
        if (*text == from) *text = static_cast<char>(to);
}
__attribute__((weak)) char* StringText(void* ustring) { return Text(ustring); }
__attribute__((weak)) void StringDestroy(void* ustring, int32_t flag) {
    (void)flag;
    std::free(Text(ustring));
}
}  // namespace TbMapStreamerFixup
