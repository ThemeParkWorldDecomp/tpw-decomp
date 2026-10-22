// MslContainerGlue.cpp: the real definitions of the callee shims the translated Metrowerks deque, tree, vector and string code of
// MslSupport.cpp and online.cpp calls through (std_cdeque.h, std_deque_errorthingy.h, online.h, std_basic_filebuf.h,
// FS_MasterSave.h, ui_main.h, InterfaceFontWrapper.h), and of the two staff panel text shims. Only libtpw compiles this file: the
// unit test libraries list their own sources and define these names themselves (recording fakes), so nothing here changes what a
// unit test compiles.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceFontWrapper.h"
#include "MslSubscriptionTree.h"
#include "TPGlobals.h"
#include "TbITable.h"
#include "TbResource.h"
#include "online.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "std_basic_filebuf.h"
#include "std_cdeque.h"
#include "std_deque_errorthingy.h"
#include "ui_ime.h"
#include "ui_main.h"

namespace {

using Cdeque = std::__cdeque<const void*, std::allocator<const void*>>;

// operator new through the arena, zeroed, with room to spare as IME::MslNew grants (the translated code keeps the result in a
// 32-bit field).
void* ArenaNew(uint32_t size) {
    const size_t n = static_cast<size_t>(size) * 2 + 32;
    return tpw::ArenaAllocZeroed(n < 64 ? 64 : n);
}

// operator delete: arena blocks go back to the arena, other host pointers to the heap. A word below 4 GB is a truncated host
// address (the translation's second pointer model, see gMslMacAddressHigh) with nothing behind it on this host: it is left alone.
void AnyDelete(void* p) {
    if (p == nullptr) return;
    if (tpw::InArena(p)) {
        tpw::ArenaFree(p);
    } else if (reinterpret_cast<uintptr_t>(p) > 0xffffffffu) {
        ::operator delete(p);
    }
}

// The element a Mac word stands for in the deque code (an arena offset; 0 is the null pointer).
const void* ElementOf(uint32_t word) { return word == 0 ? nullptr : static_cast<const void*>(tpw::gArenaBase + word); }

}  // namespace

// ---- The staff panel's state line (UIMain_SetStaffStateText 0x10180aa0) and the font wrapper's multibyte value.

// 0x10238b78 TbITable::GetConstString, on the table gText holds as resource 10.
const unsigned char* staffstate_callee::GetConstString(void* table, uint32_t id) {
    return static_cast<const TbITable*>(table)->GetConstString(id);
}

// 0x10238b90 TbIMBToUnicode::ConvertString: at most `size` characters and the terminator.
void staffstate_callee::ConvertString(void* converter, char16_t* out, const unsigned char* source, uint16_t size) {
    static_cast<TbIMBToUnicode*>(converter)->ConvertString(out, source, size);
}

// 0x1023b9c8 TbIUnicodeToMB::GetMBValue.
uint16_t fontwrapper_callee::GetMBValue(void* converter, char16_t character) {
    return static_cast<TbIUnicodeToMB*>(converter)->GetMBValue(character);
}

// ---- std::__cdeque<const void*> members, reached from the error and argument deques (the Mac passes the address of a word that
// holds the element, always an arena offset or zero; the deque code keeps its elements as arena offsets).

namespace {
void DequePopFront(void* cdeque) { static_cast<Cdeque*>(cdeque)->pop_front(); }                   // 0x101f25e8
void DequeReserve(void* cdeque, uint32_t n) { static_cast<Cdeque*>(cdeque)->reserve(n); }         // 0x101f2730
void DequePushBack(void* cdeque, const uint32_t* value) {                                         // 0x101f459c
    const void* element = ElementOf(*value);
    static_cast<Cdeque*>(cdeque)->push_back(element);
}
void DequeResize(void* cdeque, uint32_t n, const void* fill) {                                    // 0x101f2a10
    static_cast<Cdeque*>(cdeque)->resize(n, fill == nullptr ? nullptr : ElementOf(*static_cast<const uint32_t*>(fill)));
}
}  // namespace

void argdeque_callee::CdequePopFront(void* cdeque) { DequePopFront(cdeque); }
void argdeque_callee::CdequeReserve(void* cdeque, uint32_t n) { DequeReserve(cdeque, n); }
void argdeque_callee::CdequePushBack(void* cdeque, const uint32_t* value) { DequePushBack(cdeque, value); }
void argdeque_callee::CdequeResize(void* cdeque, uint32_t n, const void* fill) { DequeResize(cdeque, n, fill); }

void errdeque_callee::CdequePopFront(void* cdeque) { DequePopFront(cdeque); }
void errdeque_callee::CdequeReserve(void* cdeque, uint32_t n) { DequeReserve(cdeque, n); }
void errdeque_callee::CdequePushBack(void* cdeque, const uint32_t* value) { DequePushBack(cdeque, value); }
void errdeque_callee::CdequeResize(void* cdeque, uint32_t n, const void* fill) { DequeResize(cdeque, n, fill); }

// 0x101f492c std::__cdeque<const void*>::max_size.
uint32_t cdeque_callee::MaxSize(const Cdeque* self) { return self->max_size(); }

// 0x101f5d6c: the iterator copy, three words; answers the destination.
MslErrorDequeCursor* errdeque_callee::CursorCopy(MslErrorDequeCursor* dst, const MslErrorDequeCursor* src) {
    dst->mCdeque = src->mCdeque;
    dst->mWord4 = src->mWord4;
    dst->mSlot = src->mSlot;
    return dst;
}

// 0x101f5bdc: iterator::operator-=(long) on the cursor's three words (the block table and the slot are the low halves of host
// addresses, rebuilt with gMslMacAddressHigh as MslCdequeIterSub does).
void errdeque_callee::CursorSub(MslErrorDequeCursor* cursor, int32_t count) {
    MslCdequeIter it;
    it.mDeque = reinterpret_cast<Cdeque*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | cursor->mCdeque));
    it.mIndex = cursor->mWord4;
    it.mPtr = reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | cursor->mSlot));
    MslCdequeIterSub(&it, count);
    cursor->mWord4 = it.mIndex;
    cursor->mSlot = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(it.mPtr));
}

// 0x10238218 and 0x102384b8, the runtime's operator delete and new for the error deque's blocks.
void errdeque_callee::OperatorDelete(void* p) { AnyDelete(p); }
void* errdeque_callee::OperatorNew(uint32_t size) { return ArenaNew(size); }

// ---- Argument vector and string members.

// 0x10227200 std::vector<Argument>::vector(const vector&), on raw storage.
void argdeque_callee::VectorCopy(void* target, const void* source) {
    new (target) ArgumentVector(*static_cast<const ArgumentVector*>(source));
}

// 0x10201744 std::__destroy<Argument*>.
void std_vector_callee::ArgumentDestroyRange(Argument* first, Argument* last) { std_destroy::DestroyRange(first, last); }

// 0x10002b8c std::string::init(first, last).
void argument_callee::StringInit(void* self, const char* first, const char* last) {
    msl_char_string::init(static_cast<MslCharString*>(self), first, last);
}

// ---- __red_black_tree::balance_insert 0x10153b24 for the three trees whose nodes begin {left, right, parent|colour}.

void nocase_callee::BalanceInsert(NocaseSetTree::node* node, NocaseSetTree::node* root) {
    MslRedBlackTree::balance_insert(reinterpret_cast<MslRedBlackTree::node_base*>(node),
                                    reinterpret_cast<MslRedBlackTree::node_base*>(root));
}
void WcharTreeCallee::BalanceInsert(WcharSetTree::node* node, WcharSetTree::node* root) {
    MslRedBlackTree::balance_insert(reinterpret_cast<MslRedBlackTree::node_base*>(node),
                                    reinterpret_cast<MslRedBlackTree::node_base*>(root));
}
void ParkThemeTreeCallee::BalanceInsert(ParkThemeTree::node* node, ParkThemeTree::node* root) {
    MslRedBlackTree::balance_insert(reinterpret_cast<MslRedBlackTree::node_base*>(node),
                                    reinterpret_cast<MslRedBlackTree::node_base*>(root));
}

// ---- ignore_callee: the stream, string and tree callees of the online ignore and buddy lists.

namespace {
// The words of a Mac tree node (left +0, right +4, parent with the colour in bit 0 at +8) in the arena, as the buddy and ignore
// maps keep them (the iterator is an arena offset).
uint32_t NodeWord(uint32_t node, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, tpw::gArenaBase + node + offset, sizeof(value));
    return value;
}
}  // namespace

// 0x10152db0 iterator::operator++: with a right child its leftmost descendant, otherwise up until we come out of a left child
// (the root's parent is the end node, whose left word is the root).
void ignore_callee::TreeIncrement(uint32_t* node) {
    uint32_t current = *node;
    const uint32_t right = NodeWord(current, 4);
    if (right != 0) {
        current = right;
        while (NodeWord(current, 0) != 0) current = NodeWord(current, 0);
        *node = current;
        return;
    }
    for (;;) {
        const uint32_t parent = NodeWord(current, 8) & ~1u;
        const bool cameUpLeft = NodeWord(parent, 0) == current;
        current = parent;
        if (cameUpLeft) break;
    }
    *node = current;
}

// char strings (MslCharString, host layout) 0x101b2440 and 0x1020100c.
void ignore_callee::CharStringAssign(void* self, const char* text, uint32_t count) {
    msl_char_string::assignChars(static_cast<MslCharString*>(self), text, count);
}
void ignore_callee::CharStringAppend(void* self, const char* text, uint32_t count) {
    msl_char_string::appendChars(static_cast<MslCharString*>(self), text, count);
}

// operator new[] 0x10238668, strcpy 0x10238410 and the reference counted character pointer constructor 0x100026ac.
char* ignore_callee::NewArray(uint32_t size) { return static_cast<char*>(IME::MslNewArray(size)); }
char* ignore_callee::StrCpy(char* to, const char* from) { return IME::MslStrcpy(to, from); }
void ignore_callee::RefPtrCtor(void* self, char* pointer) {
    IME::MslRefCountedCharsCtor(static_cast<IME::MslRefCountedChars*>(self), pointer);
}

// swprintf 0x10239160 (16-bit characters).
void ignore_callee::SwprintfWide(char16_t* buffer, uint32_t count, const char16_t* format, const char16_t* text) {
    swprintf(buffer, count, format, text);
}

// The paths are TbDynamicStringTemplate<char16_t> (vtable, length, data, capacity; 0x28 bytes here, 0x14 on the Mac).
namespace {
using PathString = TbDynamicStringTemplate<char16_t>;
static_assert(sizeof(PathString) <= sizeof(ignore_callee::IgnorePathString), "the path string fits the caller's storage");
PathString* AsPath(ignore_callee::IgnorePathString* path) { return reinterpret_cast<PathString*>(path); }
}  // namespace

// 0x1017f980 CFSUserInfo::ReturnOnlinePath: the result is built in the caller's storage.
void ignore_callee::ReturnOnlinePath(IgnorePathString* result, CFSUserInfo* info) {
    new (result) PathString(info->ReturnOnlinePath());
}

// 0x1017de54 CFSUserInfo::CFSUserInfo on the block the caller took (0xd4 bytes on the Mac; the caller's operator new grants at
// least 0x400 here).
void ignore_callee::UserInfoCtor(void* self) {
    static_assert(sizeof(CFSUserInfo) <= 0x400, "CFSUserInfo fits the block the caller's operator new grants");
    new (self) CFSUserInfo();
}

// 0x102387a0 string copy, 0x10238710 string destructor with the delete flag, 0x10238788 and 0x1023a8a0 append.
void ignore_callee::PathCopy(IgnorePathString* self, const IgnorePathString* other) {
    new (self) PathString(*reinterpret_cast<const PathString*>(other));
}
void ignore_callee::PathDtor(IgnorePathString* self, int16_t flag) {
    AsPath(self)->~PathString();
    if (flag > 0) AnyDelete(self);
}
// The callers pass the Mac address of the separator string inside gTPGlobals (offset 0x44c0), which is not where the host member
// is: that address is mapped to the member.
void ignore_callee::PathAppendString(IgnorePathString* self, const void* other) {
    const void* separator = reinterpret_cast<const uint8_t*>(&gTPGlobals) + 0x44c0;
    const TbStringBase<char16_t>* text = other == separator ? static_cast<const TbStringBase<char16_t>*>(&gTPGlobals.mWideSeparator)
                                                            : static_cast<const TbStringBase<char16_t>*>(other);
    *AsPath(self) += *text;
}
void ignore_callee::PathAppendText(IgnorePathString* self, const char16_t* text) { *AsPath(self) += text; }

// std::locale (two words on the Mac, four here). The locale a stream hands out is the classic one: the host has no facet vector
// behind the global locale (see the locale chain in MslFilebufGlue.cpp) and the host layouts of the stream and its locale pointer
// differ, so the words stay zero and the widen answers as the C locale does.
void ignore_callee::IosGetloc(IgnoreLocale* out, IgnoreIos* ios) {   // 0x10072a34
    (void)ios;
    out->mWords[0] = 0;
    out->mWords[1] = 0;
}
void* ignore_callee::UseFacetCtype(IgnoreLocale* locale) {   // 0x1021eb10
    (void)locale;
    return nullptr;
}
// ctype<wchar_t>::widen(char), a virtual (vtable 0x2c) through __ptr_glue: the classic locale widens a byte to the same value.
char16_t ignore_callee::CtypeWiden(void* facet, char c) {
    (void)facet;
    return static_cast<char16_t>(static_cast<unsigned char>(c));
}
void ignore_callee::LocaleDtor(IgnoreLocale* locale, int16_t flag) {   // 0x10071f34
    (void)flag;
    locale->mWords[0] = 0;
    locale->mWords[1] = 0;
}

// basic_filebuf<wchar_t> open 0x10221c30, close 0x10221b40, destructor 0x1021cd4c. The Mac answers the filebuf, or 0 when it did
// not open / close; the callers only compare with 0, so the answer is 1 or 0.
uint32_t ignore_callee::FilebufOpen(IgnoreFilebuf* filebuf, const char* path, int32_t mode) {
    return FilebufWcharOpen(reinterpret_cast<BasicFilebufWchar*>(filebuf), path, static_cast<uint32_t>(mode)) != nullptr ? 1u : 0u;
}
uint32_t ignore_callee::FilebufClose(IgnoreFilebuf* filebuf) {
    return reinterpret_cast<BasicFilebufWchar*>(filebuf)->close() != nullptr ? 1u : 0u;
}
void ignore_callee::FilebufDtor(IgnoreFilebuf* filebuf, int16_t flag) {
    reinterpret_cast<BasicFilebufWchar*>(filebuf)->destroy(flag);
}
