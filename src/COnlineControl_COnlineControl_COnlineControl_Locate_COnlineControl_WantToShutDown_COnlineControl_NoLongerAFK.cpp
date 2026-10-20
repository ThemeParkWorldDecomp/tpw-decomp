// Unit v51062: four small COnlineControl members, Argument::StrVal / BStrVal, and the library functions around them (wide streambuf,
// ios and ostream destructors, ctype<wchar_t>::do_widen, the buddy map's erase_one, nocase_string's destructor).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include "online.h"
#include "UI_pump.h"
#include "analyser.h"
#include "ThingReceivers.h"
#include "platform/samsutils.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "InterfaceWindow.h"

namespace {

template <class T>
T* Field(void* object, size_t offset) {
    return reinterpret_cast<T*>(static_cast<char*>(object) + offset);
}

// A 32-bit pointer word of the Mac object, read raw (arena memory is 32-bit addressable).
void* PointerAt(void* object, size_t offset) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(*Field<uint32_t>(object, offset)));
}

// A MapPos is passed in the high halfword of its register; the code reads it back with lha (signed).
int32_t Halfword(MapPos pos) {
    return static_cast<int16_t>((static_cast<uint32_t>(pos.mCell) << 8) | pos.mOffset);
}

}  // namespace

// 0x10219634: the flag argument is overwritten (r4 = this) and never read.
void COnlineControl::NoLongerAFK(bool) {
    *Field<uint8_t>(this, 0xe51) = 0;
    docommand_callee::ChatNotAFK(*Field<uint32_t>(this, 0), this);
}

// 0x102183f4
void COnlineControl::DestroyVisitor(std::u16string& name) {
    COnlinePerson* person = GetPerson(name);
    if (person != nullptr) reinterpret_cast<CThing*>(person)->KillMe();
}

// 0x1021823c: mode 1
void COnlineControl::UpdateAvatarPos(MapPos x, MapPos y) {
    online_callee::ChatMovePlayer(PointerAt(this, 0), Halfword(x), Halfword(y), 1, this);
}

// 0x10217a38: mode 2, the position is the halfwords at 4 and 6 of the own avatar (the word at 4)
void COnlineControl::HideAvatar() {
    uint32_t avatarWord = *Field<uint32_t>(this, 4);   // an arena offset on the host
    void* avatar = avatarWord != 0 ? static_cast<void*>(tpw::gArenaBase + avatarWord) : nullptr;
    online_callee::ChatMovePlayer(PointerAt(this, 0), *Field<int16_t>(avatar, 4), *Field<int16_t>(avatar, 6), 2, this);
}

// 0x10217684 Argument::StrVal: the narrow string at the Argument's start, copied whole.
namespace chat_satellite_callee {
void ArgumentStrVal(MslCharString* result, const Argument* argument) {
    msl_char_string::construct(result, reinterpret_cast<const MslCharString*>(argument), 0, 0xffffffffu);
}
}  // namespace chat_satellite_callee

// 0x10217764 Argument::BStrVal: the wide string at the Argument's offset 4, copied whole.
namespace docommand_callee {
void ArgumentBStrVal(void* result, const void* argument) {
    online_callee::WStringCopy(result, static_cast<const char*>(argument) + 4, 0, 0xffffffffu);
}
}  // namespace docommand_callee

namespace nocase_callee {
// 0x102176d8 nocase_string::~nocase_string with the delete flag (the original answers this).
void StringDtor(nocase_string* self, int16_t deleteFlag) {
    if (self != nullptr) {
        online_callee::NocaseWStringDtor(self, 0);
        if (deleteFlag > 0) online_callee::OperatorDelete(self);
    }
}

// 0x1021eda4 std::allocator<set<nocase_string> node>::deallocate: the size is unused.
void Deallocate(void* allocator, void* node, unsigned long count) {
    (void)allocator;
    (void)count;
    online_callee::OperatorDelete(node);
}
}  // namespace nocase_callee

namespace ignore_callee {
// 0x1021cbb8 std::basic_ios<wchar_t>::~basic_ios (the original answers this).
void IosDtor(void* ios, int16_t flag) {
    if (ios != nullptr) {
        *static_cast<const void**>(ios) = gIfsWVtblE;
        ifstream_callee::IosBaseDtor(ios, 0);
        if (flag > 0) ifstream_callee::Delete(ios);
    }
}

// 0x1021cc68 std::basic_ostream<wchar_t>::~basic_ostream (the original answers this). The stream's first word points to the virtual base
// table; the ios part starts at Mac offset 8 (mRest here). A non-zero flag, positive or not, tears the ios part down.
void OstreamDtor(IgnoreStream* stream, int16_t flag) {
    if (stream != nullptr) {
        void* ios = stream->mRest;
        stream->mSlot = gOfsWVtblB;
        stream->mIos->mVtable = reinterpret_cast<const char*>(gOfsWVtblB) + 0xc;
        stream->mIos->mBaseOffset = static_cast<int32_t>(static_cast<char*>(ios) - reinterpret_cast<char*>(stream->mIos));
        if (flag != 0) {
            *static_cast<const void**>(ios) = gIfsWVtblE;
            ifstream_callee::IosBaseDtor(ios, 0);
        }
        if (flag > 0) ifstream_callee::Delete(stream);
    }
}
}  // namespace ignore_callee

// 0x1021d584 std::ctype<wchar_t>::do_widen(char): sign extension.
char16_t CtypeWcharDoWiden(const void*, char c) {
    return static_cast<char16_t>(static_cast<int8_t>(c));
}

// 0x1021e14c ~basic_streambuf<wchar_t> (the original answers this).
BasicStreambufWchar* BasicStreambufWchar::destroy(int16_t flag) {
    if (this != nullptr) {
        mVtable = gIfsWVtblC;
        LocaleRefPtrDestroy(&mLocale, -1);
        if (flag > 0) ifstream_callee::Delete(this);
    }
    return this;
}

// 0x1021e2d4
void BasicStreambufWchar::imbue(const void* loc) {
    mLocale = *static_cast<const LocaleRefPtr*>(loc);
}

BasicStreambufWchar* BasicStreambufWchar::setbuf(char16_t*, int32_t) { return this; }   // 0x1021e378
int32_t BasicStreambufWchar::sync() { return 0; }                                         // 0x1021e3cc
int32_t BasicStreambufWchar::showmanyc() { return 0; }                                    // 0x1021e420
int32_t BasicStreambufWchar::underflow() { return 0xffff; }                               // 0x1021e47c
int32_t BasicStreambufWchar::pbackfail(int32_t) { return 0xffff; }                        // 0x1021e5a8
int32_t BasicStreambufWchar::overflow(int32_t) { return 0xffff; }                         // 0x1021e608

// 0x1021e8e8 erase_one: the first node whose pair is not below the key (value_compare is at this + 8), erased when the key is not below it.
uint32_t BuddyTreeEraseOneCount(BuddyTree* self, const void* key) {
    BuddyTreeNode* end = reinterpret_cast<BuddyTreeNode*>(&self->mRoot);
    BuddyTreeNode* found = end;
    BuddyTreeNode* node = self->mRoot;
    while (node != nullptr) {
        if (!buddy_erase_callee::ValueCompareNodeKey(&self->mField8, node->mPair, key)) {
            found = node;
            node = node->mLeft;
        } else {
            node = node->mRight;
        }
    }
    if (found == end || buddy_erase_callee::ValueCompareKeyNode(&self->mField8, key, found->mPair)) found = end;
    if (found == end) return 0;
    BuddyTreeErase(self, found);
    return 1;
}
