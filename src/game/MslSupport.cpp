// MslSupport.cpp: the decomp's stand-ins for MSL library code (the C++ standard library the binary links), not an original source file.

#include "Advisor.h"
#include "analyser.h"
#include "CellBase.h"
#include "Cheat.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "email.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_System.h"
#include "Game.h"
#include "GameSys.h"
#include "helpers/MslSupportHelpers.h"
#include "History.h"
#include "HLSprite.h"
#include "InterfaceWindow.h"
#include "LbFile.h"
#include "Light.h"
#include "lobby.h"
#include "Main.h"
#include "MapCell.h"
#include "MsgCheatMode.h"
#include "MslSubscriptionTree.h"
#include "MslWStringTree.h"
#include "online.h"
#include "platform/arena.h"
#include "platform/d3d_types.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "Postcard.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "Serialise.h"
#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"
#include "std_basic_istream.h"
#include "std_basic_streambuf_wchar.h"
#include "std_cdeque.h"
#include "std_ctype_wchar.h"
#include "std_deque_errorthingy.h"
#include "std_exception_what.h"
#include "std_list_uint.h"
#include "tb_ddraw.h"
#include "tb_ezsprite.h"
#include "tb_mapzoom.h"
#include "tb_resource_texture.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbPixFmt.h"
#include "TbRaConvert.h"
#include "TbTimeDiff.h"
#include "TbTList.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_Postcard.h"
#include "ui_publishParkPanel.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include "World.h"
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <new>
#include <string>

// Argument::Argument(const Argument&) 0x10201878: the copy constructor. Each of the two strings is copied with the library's
// init(first, last), after a length check that throws std::length_error (the fake throw returns, so the copy goes on).

namespace {

// The length_error the library builds on the stack and throws (the original's message "string constructor: n > max_size").
void ThrowLengthError() {
    static const char kMessage[] = "string constructor: n > max_size";
    static const char kType[] = "!std::exception!!std::logic_error!!std::length_error!!";   // 0x1024f9a0
    IME::MslExceptionObject object;
    object.mVtbl = IME::gMslExcVtblC;   // 0x10278bc0 (dead store in the original too)
    object.mVtbl = IME::gMslExcVtblB;   // 0x10278bb0
    uint32_t size = static_cast<uint32_t>(IME::MslStrlen(kMessage)) + 1;
    char* text = static_cast<char*>(IME::MslNewArray(size));
    IME::MslRefCountedCharsCtor(&object.mRc, text);
    IME::MslStrcpy(object.mRc.mPtr, kMessage);
    object.mVtbl = IME::gMslLengthErrorVtbl;
    IME::MslThrow(kType, &object, IME::gMslLengthErrorDtorTV);
}

}  // namespace

// Callers: std::vector<Argument> (the original takes this and the source, and returns this).
void std_vector_callee::ArgumentCopy(Argument* self, const Argument* other) {
    // The string fields hold 32-bit arena offsets here (Ptr32 style); the representation's data pointer is native at +0x10.
    const IME::MslWideStringRep* narrowRep = reinterpret_cast<const IME::MslWideStringRep*>(
        reinterpret_cast<uintptr_t>(tpw::gArenaBase) + other->mString);
    const char* text = reinterpret_cast<const char*>(narrowRep->mData);
    self->mString = 0;
    uint32_t length = static_cast<uint32_t>(IME::MslStrlen(text));
    if (length > 0xFFFFFFFEu) ThrowLengthError();
    argument_callee::StringInit(&self->mString, text, text + length);

    const IME::MslWideStringRep* wideRep = reinterpret_cast<const IME::MslWideStringRep*>(
        reinterpret_cast<uintptr_t>(tpw::gArenaBase) + other->mWString);
    const char16_t* wtext = wideRep->mData;
    self->mWString = 0;
    uint32_t wlength = online_parser_callee::WcsLen(wtext);
    if (wlength > 0x7FFFFFFEu) ThrowLengthError();
    IME::MslWideString_init(reinterpret_cast<IME::MslWideString*>(&self->mWString), wtext, wtext + wlength);

    self->mCommand = other->mCommand;
}

// Unit v5956: std::vector<Argument>::clear / tear_down, CToken copy and (string, member pointer) constructors, CParser::GetChar,
// COnlinePerson::COnlinePerson and std::list<std::wstring>::insert.

// 0x10201310: destroy the elements and free the storage when there is any
void online_callee::ArgumentVectorTearDown(void* vec) {
    ArgumentVector* v = static_cast<ArgumentVector*>(vec);
    char* first = reinterpret_cast<char*>(v->mData);
    if (first != nullptr) {
        std_destroy::DestroyRangeTagged(v->mData, reinterpret_cast<Argument*>(first + v->mSize * 12u));
        slc_callee::OperatorDelete(v->mData);
    }
}

// 0x10213a08 std::queue<std::vector<Argument>, std::deque<...>>::~queue (deleting form when the flag is > 0).
// The two address tests on the member (this + 4) in the listing can never fail and are dropped.
MslArgumentQueue* ArgumentQueueDestruct(MslArgumentQueue* self, int16_t deleteFlag) {
    if (self != nullptr) {
        online_callee::DequeTearDown(self);
        if (self->mBlocks != 0) online_callee::OperatorDelete(reinterpret_cast<void*>(static_cast<uintptr_t>(self->mBlocks)));
        if (deleteFlag > 0) online_callee::OperatorDelete(self);
    }
    return self;
}

// 0x10212b2c: std::set<wchar_t>'s tree, insert_node_at. The length check throws (the fakes of __throw return, so the code goes on).
WcharSetTree::node* WcharSetTree::insert_node_at(node* parent, bool left, bool leftmost, const char16_t& value) {
    static const char kMessage[] = "tree::insert length error";
    static const char kTypes[] = "!std::exception!!std::logic_error!!std::length_error!!";
    if (mCount > 0xfffffffeu) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kMessage);
        object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            object.mRc.mCount = static_cast<int32_t*>(IME::MslNew(4));
            if (object.mRc.mCount != nullptr) *object.mRc.mCount = 1;
        }
        IME::MslStrcpy(object.mRc.mPtr, kMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kTypes, &object, IME::gMslLengthErrorDtorTV);
    }
    node* made = static_cast<node*>(IME::MslNew(0xe));
    if (made != nullptr) made->mValue = value;
    made->mLeft = nullptr;
    made->mRight = nullptr;
    made->mParent = reinterpret_cast<uintptr_t>(parent) | (made->mParent & 1);
    if (left)
        parent->mLeft = made;
    else
        parent->mRight = made;
    mCount = mCount + 1;
    WcharTreeCallee::BalanceInsert(made, mRoot);
    if (leftmost) mBegin = made;
    return made;
}

// Unit v51105: CUI2DMap::Register, GetMapGridCell, the destructor, CUI2DMapSprite::SetPosition, the empty grid cell constructor, and
// small pieces of the Metrowerks library (ctype<wchar_t>, the Argument deque, the nocase wide string).

namespace {
// A Mac pointer word as a host pointer: the host's heap lies above 4 GB, gMslMacAddressHigh is the upper half (0 on the Mac).
void* FromWord(uint32_t word) { return reinterpret_cast<void*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | word)); }
}  // namespace

// 0x102b1f90 (initial word 0x2a): elements per block of a new deque (this constructor's slot).
uint32_t gArgDequeCtorBlockSize = 0x2a;
// 0x102b208c (initial word 0): the character the nocase string's data ends with.
uint16_t gNocaseWStringTerminator = 0;

// ---- std::ctype<wchar_t>

// 0x10229860 (88 bytes): two vtable stores (the first is overwritten), then the deleting form.
StdCtypeWchar* CtypeWcharDestruct(StdCtypeWchar* self, int16_t deleteFlag) {
    if (self != nullptr) {
        At<uint32_t>(self, 0) = 0x102b20bc;
        At<uint32_t>(self, 0) = 0x1029d2a0;
        if (deleteFlag > 0) online_callee::OperatorDelete(self);
    }
    return self;
}

// 0x10229808 (36 bytes)
char16_t StdCtypeWchar::do_toupper(char16_t c) const {
    if (c < 0x100) return gWUpperMap[c];
    return c;
}

// 0x102297b0 (36 bytes)
char16_t StdCtypeWchar::do_tolower(char16_t c) const {
    if (c < 0x100) return gSwearLowerMap[c];
    return c;
}

// 0x10229760 (28 bytes): a character below 0xff narrows (signed byte), the rest answers the default.
char StdCtypeWchar::do_narrow(char16_t c, char dflt) const {
    if (c < 0xff) return static_cast<char>(c);
    return dflt;
}

// ---- the Argument deque

namespace argdeque_new {
// 0x102295ec (28 bytes): the iterator copy constructor, three words.
void CdequeIterCopy(MslArgumentCursor* dest, const MslArgumentCursor* src) {
    dest->mCdeque = src->mCdeque;
    dest->mIndex = src->mIndex;
    dest->mSlot = src->mSlot;
}

// 0x10228e2c (240 bytes): the same body as make_iterator 0x10228c84.
void MakeIterator(MslArgumentDequeIter* out, const MslArgumentDeque* self, uint32_t index) {
    MslCdequeIter cursor;
    cursor.mDeque = reinterpret_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(
        const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(self)) + 4);
    cursor.mIndex = 0;
    cursor.mPtr = static_cast<uint32_t*>(FromWord(self->mTable + self->mStart * 4));
    uint32_t blockSize = self->mBlockSize;
    uint32_t block;
    uint32_t element;
    if (index < blockSize) {
        block = (self->mCount != 0) ? *cursor.mPtr : 0;
        element = block + index * 12;
    } else {
        cdeque_callee::IterAdd(&cursor, static_cast<int32_t>(index / blockSize));
        block = *cursor.mPtr;
        element = block + (index % blockSize) * 12;
    }
    out->mCursor.mCdeque = ToWord(cursor.mDeque);
    out->mCursor.mIndex = cursor.mIndex;
    out->mCursor.mSlot = ToWord(cursor.mPtr);
    out->mElement = element;
    out->mBlockEnd = block + blockSize * 12;
    out->mIndex = index;
}
}  // namespace argdeque_new

namespace argdestroy_callee {
// 0x1022935c (80 bytes): each vector from first up to last (0xc bytes a vector) is destroyed with the flag -1.
void DestroyRange(uint32_t first, uint32_t last) {
    for (uint32_t p = first; p < last; p += 0xc) argdeque_callee::VectorDtor(FromWord(p), -1);
}
}  // namespace argdestroy_callee

namespace online_callee {
// 0x10225938 (44 bytes)
void DequeCtor(void* self) {
    uint32_t* w = static_cast<uint32_t*>(self);
    w[0] = gArgDequeCtorBlockSize;
    w[1] = 0;
    w[2] = 0;
    w[3] = 0;
    w[4] = 0;
    w[5] = 0;
    w[6] = 0;
}
}  // namespace online_callee

namespace argdeque_callee {
// 0x10225874 (8 bytes): the element word of the iterator.
void* IterDeref(const MslArgumentDequeIter* it) { return FromWord(it->mElement); }
}  // namespace argdeque_callee

// ---- the wide strings (one pointer to a representation: length, capacity, refs, data)

namespace nocase_callee {
// 0x10228770 (56 bytes): the copy constructor is the substring constructor over the whole string.
nocase_string* StringCopy(nocase_string* self, const nocase_string* other) {
    online_callee::NocaseWStringCopy(self, other, 0, 0xffffffffu);
    return self;
}
}  // namespace nocase_callee

namespace online_callee {
// 0x1022705c (264 bytes): allocates the representation and the data, copies the characters, terminates. (The original's catch
// block frees the representation and rethrows; the runtime's operator new never fails, so it is not reachable.)
void NocaseWStringInit(void* self, const char16_t* first, const char16_t* last) {
    IME::MslWideStringRep** slot = static_cast<IME::MslWideStringRep**>(self);
    *slot = static_cast<IME::MslWideStringRep*>(OperatorNew(0x10));
    int32_t bytes = static_cast<int32_t>(ToWord(last) - ToWord(first));
    int32_t n = bytes / 2;
    IME::MslWideStringRep* rep = *slot;
    if (rep != nullptr) {
        rep->mCapacity = static_cast<uint32_t>(n + 3 - (n & 3));
        rep->mRefs = 1;
        rep->mData = static_cast<char16_t*>(OperatorNew((rep->mCapacity + 1) * 2));
    }
    wmemcpy((*slot)->mData, first, static_cast<size_t>(n));
    (*slot)->mLength = static_cast<uint32_t>(n);
    (*slot)->mData[n] = gNocaseWStringTerminator;
}
}  // namespace online_callee

namespace online_lt {
// 0x10226134 (12 bytes)
uint32_t NocaseWStringSize(const void* self) { return (*static_cast<IME::MslWideStringRep* const*>(self))->mLength; }
// 0x102260c8 (12 bytes)
const char16_t* NocaseWStringBegin(const void* self) { return (*static_cast<IME::MslWideStringRep* const*>(self))->mData; }
}  // namespace online_lt

namespace wstring_less_callee {
// 0x1022605c (12 bytes)
uint32_t Size(const MslWideString* s) { return (*reinterpret_cast<IME::MslWideStringRep* const*>(s))->mLength; }
// 0x10225ff0 (12 bytes)
const char16_t* Begin(const MslWideString* s) { return (*reinterpret_cast<IME::MslWideStringRep* const*>(s))->mData; }
}  // namespace wstring_less_callee

// std::basic_string<wchar_t> of the Metrowerks library, main app: init 0x10146cf8, constructors 0x10146b8c (pointer, length),
// 0x10146a14 (C string), 0x101e0b84 (empty) and change_size 0x101b2cd8. The string is one pointer to a representation
// (length +0, capacity +4, reference count +8, data +0xc; the data pointer is native here). The addresses are computed
// in 32 bits like the Mac's, so a huge length wraps the same way. The fakes of __throw return, so the code goes on after
// the inline throw sequences.

namespace IME {

char16_t gMslWcharNulInit = 0;    // 0x102a4d3c: the terminator init writes
char16_t gMslWcharNulResize = 0;  // 0x102ac356: the terminator change_size writes
char16_t gMslWcharNulEmpty = 0;   // 0x102b006c: the terminator of the empty string

namespace {

MslWideStringRep*& RepOf(MslWideString* s) { return reinterpret_cast<MslWideStringRep*&>(s->mData); }

// The address of data + index (in characters) with the Mac's 32-bit wrap.
char16_t* At(char16_t* data, uint32_t index) {
    return reinterpret_cast<char16_t*>(reinterpret_cast<uintptr_t>(data) + static_cast<intptr_t>(static_cast<int32_t>(index * 2u)));
}

// The exception object the library builds inline for std::length_error, then __throw (the fakes return).
void ThrowLengthError(const char* msg, const char* type) {
    MslExceptionObject ex;
    ex.mVtbl = gMslExcVtblC;
    ex.mVtbl = gMslExcVtblB;
    size_t n = MslStrlen(msg);
    char* p = static_cast<char*>(MslNewArray(static_cast<uint32_t>(n + 1)));
    MslRefCountedCharsCtor(&ex.mRc, p);
    MslStrcpy(ex.mRc.mPtr, msg);
    ex.mVtbl = gMslLengthErrorVtbl;
    MslThrow(type, &ex, gMslLengthErrorDtorTV);
}

const char kTypeName[] = "!std::exception!!std::logic_error!!std::length_error!!";

}  // namespace

// 0x10146cf8: init(first, last)
void MslWideString_init(MslWideString* self, const char16_t* first, const char16_t* last) {
    RepOf(self) = static_cast<MslWideStringRep*>(MslNewRep());
    int32_t d = static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(last)) -
                                     static_cast<uint32_t>(reinterpret_cast<uintptr_t>(first)));
    uint32_t n = static_cast<uint32_t>((d >> 1) + ((d < 0 && (d & 1) != 0) ? 1 : 0));
    MslWideStringRep* rep = RepOf(self);
    if (rep != nullptr) {
        rep->mCapacity = n + 3 - (n & 3);
        rep->mRefs = 1;
        rep->mData = static_cast<char16_t*>(MslNew((rep->mCapacity + 1) * 2));
    }
    MslWmemcpy(RepOf(self)->mData, first, n);
    RepOf(self)->mLength = n;
    *At(RepOf(self)->mData, n) = gMslWcharNulInit;
}

// 0x10146b8c: basic_string(const wchar_t*, unsigned long, const allocator&)
MslWideString* MslWideString_ctor(MslWideString* self, const char16_t* text, uint32_t n) {
    self->mData = nullptr;
    if (n > 0x7ffffffeu) ThrowLengthError("string constructor: n > max_size", kTypeName);
    MslWideString_init(self, text, At(const_cast<char16_t*>(text), n));
    return self;
}

// 0x10146a14: basic_string(const wchar_t*, const allocator&)
MslWideString* MslWideString_ctorCStr(MslWideString* self, const char16_t* text) {
    self->mData = nullptr;
    uint32_t n = wcslen(text);
    if (n > 0x7ffffffeu) ThrowLengthError("string constructor: n > max_size", kTypeName);
    MslWideString_init(self, text, At(const_cast<char16_t*>(text), n));
    return self;
}

// 0x101e0b84: basic_string(const allocator&): the empty string
MslWideString* MslWideString_ctorEmpty(MslWideString* self) {
    self->mData = nullptr;
    RepOf(self) = static_cast<MslWideStringRep*>(MslNewRep());
    MslWideStringRep* rep = RepOf(self);
    if (rep != nullptr) {
        rep->mCapacity = 3;
        rep->mRefs = 1;
        rep->mData = static_cast<char16_t*>(MslNew((rep->mCapacity + 1) * 2));
    }
    *RepOf(self)->mData = gMslWcharNulEmpty;
    RepOf(self)->mLength = 0;
    return self;
}

// 0x101b2cd8: change_size(n, keep)
void MslWideString_changeSize(MslWideString* self, uint32_t n, bool keep) {
    if (n > 0x7ffffffeu) {
        MslExceptionObject ex;
        ex.mVtbl = gMslExcVtblC;
        ex.mVtbl = gMslExcVtblB;
        const char msg[] = "basic_string::length error.  Resizing out of range";
        size_t len = MslStrlen(msg);
        ex.mRc.mPtr = static_cast<char*>(MslNewArray(static_cast<uint32_t>(len + 1)));
        ex.mRc.mCount = nullptr;
        if (ex.mRc.mPtr != nullptr) {
            int32_t* count = static_cast<int32_t*>(MslNew(4));
            if (count != nullptr) *count = 1;
            ex.mRc.mCount = count;
        }
        MslStrcpy(ex.mRc.mPtr, msg);
        ex.mVtbl = gMslLengthErrorVtbl;
        MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &ex, gMslLengthErrorDtorTV);
    }
    if (RepOf(self)->mRefs > 1) {
        MslWideStringRep* old = RepOf(self);
        MslWideStringRep* fresh = static_cast<MslWideStringRep*>(MslNewRep());
        if (fresh != nullptr) {
            fresh->mCapacity = n + 3 - (n & 3);
            fresh->mRefs = 1;
            fresh->mData = static_cast<char16_t*>(MslNew((fresh->mCapacity + 1) * 2));
        }
        old->mRefs = old->mRefs - 1;
        if (keep) {
            uint32_t m = old->mLength < n ? old->mLength : n;
            MslWmemcpy(fresh->mData, old->mData, m + 1);
            fresh->mLength = old->mLength;
        }
        RepOf(self) = fresh;
    }
    MslWideStringRep* rep = RepOf(self);
    uint32_t cap = rep->mCapacity;
    if (n > cap) {
        uint32_t x = cap;
        while (x < n) x <<= 1;
        uint32_t want = x < rep->mLength ? rep->mLength : x;
        uint32_t newCap = want + 3 - (want & 3);
        if (newCap != rep->mCapacity) {
            char16_t* oldData = rep->mData;
            rep->mData = static_cast<char16_t*>(MslNew((newCap + 1) * 2));
            MslWmemcpy(rep->mData, oldData, rep->mLength + 1);
            MslDelete(oldData);
            rep->mCapacity = newCap;
        }
    }
    *At(RepOf(self)->mData, n) = gMslWcharNulResize;
    RepOf(self)->mLength = n;
    rep = RepOf(self);
    rep->mRefs = rep->mRefs == 0 ? 1 : rep->mRefs;
}

}  // namespace IME

// The theme map's red-black tree (std::__tree of the CFSGlobalMasterSave theme map): find, clear and insert_one.

uint8_t gParkTreeInserted = 1;      // 0x102a7238
uint8_t gParkTreeNotInserted = 0;   // 0x102a7239

// 0x1016cf68: the lower bound of the key; end() when that node's key is greater than the key.
ParkThemeTree::Iterator ParkThemeTree::find(const Key& key) {
    node* last = end();
    node* cur = mRoot;
    while (cur != nullptr) {
        if (!(cur->mValue.first < key)) {
            last = cur;
            cur = cur->mLeft;
        } else {
            cur = cur->mRight;
        }
    }
    if (last != end() && !(key < last->mValue.first)) return Iterator{last};
    return Iterator{end()};
}

// 0x1016cd78
void ParkThemeTree::clear() {
    if (mRoot != nullptr) {
        destroy(mRoot);
        mCount = 0;
        mRoot = nullptr;
        mBegin = end();
    }
}

// 0x1016e0b8
ParkThemeTree::InsertResult ParkThemeTree::insert_one(const Pair& value) {
    const Key& key = value.first;
    bool left = true;
    bool leftmost = true;
    node* parent = end();
    node* last = nullptr;
    node* cur = mRoot;
    while (cur != nullptr) {
        parent = cur;
        if (key < cur->mValue.first) {
            cur = cur->mLeft;
            left = true;
        } else {
            last = cur;
            cur = cur->mRight;
            left = false;
            leftmost = false;
        }
    }
    if (last == nullptr || last->mValue.first < key) {
        node* made = insert_node_at(parent, left, leftmost, value);
        return InsertResult{made, gParkTreeInserted};
    }
    return InsertResult{last, gParkTreeNotInserted};
}

// Unit v5917: std::__tree<unsigned short> find 0x1016d1f8, insert_one 0x1016e9c4, insert_node_at 0x1016f164; IPSystem::Startup 0x1016f3c8
// and Shutdown 0x1016f448; ccParkInfo::~ccParkInfo 0x1016ff48.

uint8_t gTreeInsertedTrue = 1;    // 0x102a7230
uint8_t gTreeInsertedFalse = 0;   // 0x102a7231

// 0x1016d1f8: the first node whose value is not below the key, or the end node when the key is below that node's value (or none).
UshortSetTree::Iterator UshortSetTree::find(const uint16_t& key) {
    node* candidate = end();
    node* current = mRoot;
    while (current != nullptr) {
        if (current->mValue < key) {
            current = current->mRight;
        } else {
            candidate = current;
            current = current->mLeft;
        }
    }
    Iterator result;
    if (candidate != end() && key >= candidate->mValue)
        result.mNode = candidate;
    else
        result.mNode = end();
    return result;
}

// 0x1016e9c4: walks down remembering the last node whose value is not above the key (the candidate) and the last node visited with the side
// taken; inserts when there is no candidate or its value is below the key.
UshortSetTree::InsertResult UshortSetTree::insert_one(const uint16_t& key) {
    bool left = true;
    bool leftmost = true;
    node* candidate = nullptr;
    node* parent = end();
    node* current = mRoot;
    while (current != nullptr) {
        parent = current;
        if (key < current->mValue) {
            current = current->mLeft;
            left = true;
        } else {
            candidate = current;
            current = current->mRight;
            left = false;
            leftmost = false;
        }
    }
    InsertResult result;
    if (candidate == nullptr || candidate->mValue < key) {
        result.mNode = insert_node_at(parent, left, leftmost, key);
        result.mInserted = gTreeInsertedTrue;
    } else {
        result.mNode = candidate;
        result.mInserted = gTreeInsertedFalse;
    }
    return result;
}

// 0x1016f164. A count of 0xffffffff builds the length_error and throws it (the host's throw returns, like the test fakes; the original
// would not); the node is made either way.
UshortSetTree::node* UshortSetTree::insert_node_at(node* parent, bool left, bool leftmost, const uint16_t& value) {
    static const char kMessage[] = "tree::insert length error";
    static const char kTypes[] = "!std::exception!!std::logic_error!!std::length_error!!";
    if (mCount > 0xfffffffeu) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kMessage);
        char* text = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        IME::MslRefCountedCharsCtor(&object.mRc, text);
        IME::MslStrcpy(object.mRc.mPtr, kMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kTypes, &object, IME::gMslLengthErrorDtorTV);
    }
    node* made = static_cast<node*>(IME::MslNew(0xe));
    if (made != nullptr) made->mValue = value;
    made->mLeft = nullptr;
    made->mRight = nullptr;
    made->mParent = reinterpret_cast<uintptr_t>(parent) | (made->mParent & 1);
    if (left)
        parent->mLeft = made;
    else
        parent->mRight = made;
    mCount = mCount + 1;
    v5917_callee::BalanceInsert(made, mRoot);
    if (leftmost) mBegin = made;
    return made;
}

// 0x1016f080: std::pair<const TbDynamicStringTemplate<char>, CFSGlobalParkData*>::pair(const pair&).
// Copy-constructs the key (0x10238308), then copies the record pointer (Mac offset 0x14). The original returns this; the declaration is void.
void ParkThemeTreeCallee::PairCopy(ParkThemeTree::Pair* pair, const ParkThemeTree::Pair& value) {
    new (const_cast<ParkThemeTree::Key*>(&pair->first)) ParkThemeTree::Key(value.first);
    pair->second = value.second;
}

// OnlineConfig::~OnlineConfig (main 0x10003508, 892 B; the out-of-line copy of the destructor that follows FS_Registry's code).
// The host OnlineConfig holds real std::strings (their destructors are the implicit ones), so this is the Mac's destructor written over
// the Mac's object seen as an array of string words (Mac offset / 4): the strings at +0 +4 +8 +0xc +0x10 +0x14 and +0x24, each the
// address of an MSL string rep (len +0, cap +4, refs +8, data +0xc).

namespace {

// One string member: the reference count is raised to 1 when 0, lowered by one; the rep is kept only when it reached 0 (otherwise the
// member is cleared and nothing is freed); a kept rep frees its data and itself.
void ReleaseString(MslCharString* s) {
    MslCharRep* rep = s->mRep;
    if (rep->refs == 0) rep->refs = 1;
    rep->refs = rep->refs - 1;
    if (rep->refs != 0) rep = nullptr;
    s->mRep = rep;
    if (s->mRep != nullptr) {
        online_config_callee::OperatorDelete(s->mRep->data);
        online_config_callee::OperatorDelete(s->mRep);
    }
}

}  // namespace

namespace online_config_mac {

// 0x10003508: members in the order +0x24, +0x14, +0x10, +0xc, +8, +4, +0, then (flag above 0 as a signed 16 bit) delete this. Answers this.
MslCharString* Destroy(MslCharString* self, int16_t flag) {
    if (self != nullptr) {
        ReleaseString(self + 9);
        ReleaseString(self + 5);
        ReleaseString(self + 4);
        ReleaseString(self + 3);
        ReleaseString(self + 2);
        ReleaseString(self + 1);
        ReleaseString(self + 0);
        if (flag > 0) online_config_callee::OperatorDelete(self);
    }
    return self;
}

}  // namespace online_config_mac

// Unit v51070: FTPClient::_threadfn and small library functions of the Metrowerks runtime on the Mac's word layouts (main app).

namespace {
inline uint32_t Addr(const void* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }
const char kSpliceMessage[] = "list::splice length error";                                    // 0x1024ffbb
const char kLengthErrorType[] = "!std::exception!!std::logic_error!!std::length_error!!";   // 0x102500f9

struct SpliceException {
    const void* mVtbl;     // 0x00
    char* mMessage;        // 0x04
    uint32_t* mRefs;       // 0x08
};
}  // namespace

namespace v51070 {

// 0x1020abec (432): moves all of other before pos. A list that would grow past 0x7fffffff elements throws std::length_error
// (the fake __throw returns, and the splice goes on).
void StrListSplice(uint32_t* self, uint32_t pos, uint32_t* other) {
    uint32_t otherSize = other[0];
    if (otherSize == 0) return;
    if (self[0] > 0x7fffffffu - otherSize) {
        SpliceException object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        uint32_t length = v51070_callee::Strlen(kSpliceMessage);
        object.mMessage = static_cast<char*>(v51070_callee::NewArray(length + 1));
        object.mRefs = nullptr;
        if (object.mMessage != nullptr) {
            uint32_t* refs = static_cast<uint32_t*>(v51070_callee::New(4));
            if (refs != nullptr) *refs = 1;
            object.mRefs = refs;
        }
        v51070_callee::Strcpy(object.mMessage, kSpliceMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        v51070_callee::Throw(kLengthErrorType, &object, IME::gMslLengthErrorDtorTV);
    }
    uint32_t sentinel = Addr(other) + 4;
    uint32_t first = other[2];
    uint32_t last = other[1];
    Words(sentinel)[1] = sentinel;
    Words(sentinel)[0] = sentinel;
    self[0] = self[0] + other[0];
    other[0] = 0;
    Words(Words(pos)[0])[1] = first;
    Words(first)[0] = Words(pos)[0];
    Words(pos)[0] = last;
    Words(last)[1] = pos;
}

// 0x1021f130 (76): compare(0, a.size, b.data, b.size) == 0
bool WStringEq(const uint32_t* a, const uint32_t* b) {
    const uint32_t* repB = Words(b[0]);
    const uint32_t* repA = Words(a[0]);
    return v51070_callee::WCompare(a, 0, repA[0], Words(repB[3]), repB[0]) == 0;
}

// 0x1021f254 (76)
bool NocaseWStringEq(const uint32_t* a, const uint32_t* b) {
    const uint32_t* repB = Words(b[0]);
    const uint32_t* repA = Words(a[0]);
    return v51070_callee::NocaseCompare(a, 0, repA[0], Words(repB[3]), repB[0]) == 0;
}

// 0x1021f7b0 (132): flushes the stream when its flags say unitbuf and the sentry did not already.
uint8_t* OstreamSentryDtor(uint8_t* self, int16_t flag) {
    if (self) {
        uint32_t streamWord;
        std::memcpy(&streamWord, self + 2, 4);
        uint32_t* stream = Words(streamWord);
        const uint8_t* ios = reinterpret_cast<const uint8_t*>(Words(stream[0]));
        uint32_t state, flags;
        std::memcpy(&state, ios + 0x34, 4);
        std::memcpy(&flags, ios + 0x30, 4);
        if ((state & 5) == 0 && (flags & 0x2000) != 0 && self[1] == 0) v51070_callee::OstreamFlush(stream);
        if (flag > 0) v51070_callee::Delete(self);
    }
    return self;
}

// 0x1021f888 (8)
uint32_t IosTie(const uint8_t* ios) {
    uint32_t word;
    std::memcpy(&word, ios + 0x3c, 4);
    return word;
}

namespace {
// find: lower bound, then the end node (tree + 4) when the key is smaller than the found node's. Nodes { left 0, right 4, key 0xc }.
template <bool (*Less)(const void*, const void*)>
void TreeFind(uint32_t* result, const uint32_t* tree, const uint32_t* key) {
    uint32_t end = Addr(tree) + 4;
    uint32_t best = end;
    uint32_t node = tree[1];
    while (node != 0) {
        if (!Less(Words(node + 0xc), key)) {
            best = node;
            node = Words(node)[0];
        } else {
            node = Words(node)[1];
        }
    }
    if (best == end || Less(key, Words(best + 0xc))) *result = end;
    else *result = best;
}
}  // namespace

// 0x1021fae4 (196)
void WStringSetFind(uint32_t* result, const uint32_t* tree, const uint32_t* key) {
    TreeFind<v51070_callee::WLess>(result, tree, key);
}

// 0x1021fd4c (196)
void NocaseSetFind(uint32_t* result, const uint32_t* tree, const uint32_t* key) {
    TreeFind<v51070_callee::NocaseLess>(result, tree, key);
}

// 0x10220194 (20): the base class answers fpos(-1) and a zero state.
void StreambufSeekpos(uint32_t* result) {
    result[0] = 0xffffffffu;
    result[1] = 0;
}

// 0x1022021c (20)
void StreambufSeekoff(uint32_t* result) {
    result[0] = 0xffffffffu;
    result[1] = 0;
}

// 0x1022083c (68)
uint32_t* IstreamSentryDtor(uint32_t* self, int16_t flag) {
    if (self && flag > 0) v51070_callee::Delete(self);
    return self;
}

// 0x101f5d6c (28)
uint32_t* ErrorDequeIterCopy(uint32_t* dst, const uint32_t* src) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    return dst;
}

}  // namespace v51070

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

// Metrowerks std::vector<const void*> insert / erase / resize and std::__locale_imp<__int128>::~__locale_imp (main app, unit v5916).

// 0x10073ed4 (736): insert n copies of *value before pos. The length error is built and "thrown" through the library's __throw; the
// fakes of the tests return from it, and the original goes on (kept).
void MslPtrVector::insert(const void** pos, uint32_t n, const void* const* value) {
    if (n == 0) return;
    if (n > 0x3fffffffu || mSize > 0x3fffffffu - n) {
        IME::MslExceptionObject exc;
        exc.mVtbl = IME::gMslExcVtblC;
        static const char kMessage[] = "vector::insert length error";   // 0x1023dc08
        exc.mVtbl = IME::gMslExcVtblB;
        exc.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(kMessage) + 1)));
        exc.mRc.mCount = nullptr;
        if (exc.mRc.mPtr != nullptr) {
            int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
            if (count != nullptr) *count = 1;
            exc.mRc.mCount = count;
        }
        IME::MslStrcpy(exc.mRc.mPtr, kMessage);
        exc.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &exc, IME::gMslLengthErrorDtorTV);
    }
    const uint32_t newSize = mSize + n;
    if (newSize <= mCapacity) {
        const void** end = mData + mSize;
        if (end - pos != 0) {
            const void** src = end;
            const void** dst = end + n;
            while (src != pos) {
                --src;
                --dst;
                *dst = *src;
            }
        }
        for (uint32_t i = 0; i < n; i++) {
            *pos = *value;
            pos++;
        }
        mSize += n;
        return;
    }
    const void** old = mData;
    const uint32_t index = static_cast<uint32_t>(pos - old);
    uint32_t capacity = mCapacity;
    if (capacity == 0) capacity = 1;
    while (newSize > capacity) capacity <<= 1;
    mData = static_cast<const void**>(IME::MslNew(capacity * static_cast<uint32_t>(sizeof(void*))));
    if (index != 0) {
        const void** to = mData;
        for (const void** from = old; from != old + index; from++) {
            *to = *from;
            to++;
        }
    }
    for (uint32_t i = 0; i < n; i++) mData[index + i] = *value;
    const uint32_t oldSize = mSize;
    if (index < oldSize) {
        const void** to = mData + index + n;
        for (const void** from = old + index; from != old + oldSize; from++) {
            *to = *from;
            to++;
        }
    }
    if (old != nullptr) IME::MslDelete(const_cast<void**>(old));
    mSize = newSize;
    mCapacity = capacity;
}

// 0x10073e10 (116)
const void** MslPtrVector::erase(const void** first, const void** last) {
    if (first == last) return first;
    const void** end = mData + mSize;
    if (end - last != 0) {
        const void** to = first;
        for (const void** from = last; from != end; from++) {
            *to = *from;
            to++;
        }
    }
    mSize -= static_cast<uint32_t>(last - first);
    return first;
}

// 0x10073d58 (108)
void MslPtrVector::resize(uint32_t n, const void* value) {
    const uint32_t size = mSize;
    if (n > size) {
        insert(mData + size, n - size, &value);
    } else if (n < size) {
        erase(mData + n, mData + size);
    }
}

// 0x100737a0 (352): the facets lose a reference each (the last one deletes the facet through its virtual destructor), the
// character array loses one, then the arrays and, for a positive flag, the object itself are freed.
LocaleImp* LocaleImpDestroy(LocaleImp* self, int16_t deleteFlag) {
    if (self == nullptr) return self;
    const uint32_t count = self->mFacets.mSize;
    for (uint32_t i = 0; i < count; i++) {
        UseFacetObj* facet = static_cast<UseFacetObj*>(const_cast<void*>(self->mFacets.mData[i]));
        if (facet == nullptr) continue;
        facet->mRefs = facet->mRefs - 1;
        if (facet->mRefs == 0) use_facet_callee::FacetDelete(facet, 1);
    }
    LocaleCharArray* chars = self->mChars;
    if (chars->mRefs == 0) chars->mRefs = 1;
    chars->mRefs = chars->mRefs - 1;
    if (chars->mRefs != 0) chars = nullptr;
    self->mChars = chars;
    if (self->mChars != nullptr) {
        IME::MslDelete(self->mChars->mData);
        IME::MslDelete(self->mChars);
    }
    if (self->mFacets.mData != nullptr) IME::MslDelete(const_cast<void**>(self->mFacets.mData));
    if (deleteFlag > 0) IME::MslDelete(self);
    return self;
}

// The block words hold 32-bit Mac addresses; the host's heap lies above 4 GB (gMslMacAddressHigh, 0 on the Mac).
namespace {
uint32_t* BlockPtr(uint32_t word) {
    return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | word));
}
uint32_t PtrWord(const uint32_t* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
}  // namespace

// 0x101f5bdc: iterator::operator-=(long). A count of INT_MIN is the one value that reaches operator+=.
void MslCdequeIterSub(MslCdequeIter* it, int32_t n) {
    std::__cdeque<const void*, std::allocator<const void*>>* d = it->mDeque;
    if (n >= 0) {
        uint32_t un = static_cast<uint32_t>(n);
        if (un <= d->mWord8 + it->mIndex) {
            it->mPtr = BlockPtr(PtrWord(it->mPtr) - (un << 2));
        } else {
            it->mPtr = BlockPtr(d->mWordC + ((d->mWord0 - un) << 2));
        }
        it->mIndex = it->mIndex - un;
        return;
    }
    int32_t count = static_cast<int32_t>(0u - static_cast<uint32_t>(n));
    if (count < 0) {
        cdeque_callee::IterAdd(it, count);
        return;
    }
    uint32_t sum = it->mIndex + static_cast<uint32_t>(count) + d->mWord8;
    if (sum < d->mWord0) {
        it->mPtr = BlockPtr(PtrWord(it->mPtr) + (static_cast<uint32_t>(count) << 2));
    } else {
        it->mPtr = BlockPtr(d->mWordC + ((sum - d->mWord0) << 2));
    }
    it->mIndex = it->mIndex - static_cast<uint32_t>(n);
}

// 0x101f5200: queue::~queue (the queue is its deque at offset 0; the block table at 4).
MslErrorDeque* MslQueueErrorDestroy(MslErrorDeque* self, int16_t deleteFlag) {
    if (self != nullptr) {
        errdeque_callee::TearDown(self);
        errdeque_callee::QueueCdequeDtor(&self->mBlocks, -1);
        if (deleteFlag > 0) errdeque_callee::OperatorDelete(self);
    }
    return self;
}

// 0x101f2730
void std::__cdeque<const void*, std::allocator<const void*>>::reserve(uint32_t n) {
    using namespace IME;
    if (n > cdeque_callee::MaxSize(this)) {
        MslExceptionObject object;
        object.mVtbl = gMslExcVtblC;
        object.mVtbl = gMslExcVtblB;
        const char* message = "cdeque::reserve length error";
        size_t length = MslStrlen(message);
        object.mRc.mPtr = static_cast<char*>(MslNewArray(static_cast<uint32_t>(length + 1)));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            int32_t* counter = static_cast<int32_t*>(MslNew(4));
            if (counter != nullptr) *counter = 1;
            object.mRc.mCount = counter;
        }
        MslStrcpy(object.mRc.mPtr, message);
        object.mVtbl = gMslLengthErrorVtbl;
        MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, gMslLengthErrorDtorTV);
    }
    if (n > mWord0) {
        uint32_t* block = static_cast<uint32_t*>(MslNew(n << 2));
        if (mWordC != 0) {
            uint32_t* source = BlockPtr(mWordC) + mWord8;
            uint32_t* destination = block;
            for (uint32_t index = 0; index != mWord4; index++) {
                *destination = *source;
                source++;
                if (source == BlockPtr(mWordC) + mWord0) source = BlockPtr(mWordC);
                destination++;
            }
            if (mWordC != 0) MslDelete(BlockPtr(mWordC));
        }
        mWord0 = n;
        mWordC = PtrWord(block);
        mWord8 = 0;
    }
}

// std::basic_ofstream<char>: constructor 0x10071e34 and destructor 0x1006fdc0 (unit v5527).
// The object keeps the Mac's 32-bit words: +0 the pointer to the virtual ios part (an arena offset here), +4 the vtable word,
// +8 the filebuf, +0x48 the ios part. Addresses of globals (vtables, __files) are stored truncated to 32 bits.

char gOfstreamVt[0x20];   // 0x1029d1a8 (initial word 0x1029d1a0): the ofstream's own vtable block

namespace {
inline uint32_t& W(void* base, uint32_t off) { return *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(base) + off); }
inline uint32_t A(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }   // a global's address
inline uint32_t AO(const void* p) { return tpw::ArenaOffset(p); }                                    // an object's address
inline uint8_t* PO(uint32_t w) { return tpw::gArenaBase + w; }
}  // namespace

// 0x10071e34 (180)
void* BasicOfstreamCtor(void* self, int16_t withVirtualBase) {
    uint8_t* obj = static_cast<uint8_t*>(self);
    if (withVirtualBase != 0) {
        uint8_t* ios = obj + 0x48;
        W(obj, 0) = AO(ios);
        W(ios, 0) = A(gIosBaseVt);
        W(ios, 0) = A(gOstreamVt1);
    }
    BasicOstreamCtor(reinterpret_cast<MacOstream*>(obj), 0, AO(obj + 8));
    W(obj, 4) = A(gOfstreamVt);
    uint8_t* q = PO(W(obj, 0));
    W(q, 0) = A(gOfstreamVt) + 0xc;
    q = PO(W(obj, 0));
    W(q, 0x44) = AO(obj + 0x48) - AO(q);
    reinterpret_cast<BasicFilebufChar*>(obj + 8)->construct(nullptr);
    return self;
}

// 0x1006fdc0 (324): the deleting destructor, `flags` is the hidden short after this.
void* BasicOfstreamDtor(void* self, int16_t flags) {
    if (self == nullptr) return self;
    uint8_t* obj = static_cast<uint8_t*>(self);
    W(obj, 4) = A(gOfstreamVt);
    uint8_t* q = PO(W(obj, 0));
    W(q, 0) = A(gOfstreamVt) + 0xc;
    q = PO(W(obj, 0));
    W(q, 0x44) = AO(obj + 0x48) - AO(q);
    // the filebuf part
    W(obj, 8) = A(gFilebufCharVtblB);
    uint32_t file = W(obj, 0x2c);
    uint32_t files = A(gIfsWFiles);
    if (file != files && file != files + 0x54 && file != files + 0xa8) OfstreamClose(obj + 8);
    W(obj, 8) = A(gFilebufCharVtblA);
    OfstreamLocaleDtor(obj + 0x24, -1);
    // the ostream part
    W(obj, 4) = A(gOstreamVt2);
    q = PO(W(obj, 0));
    W(q, 0) = A(gOstreamVt2) + 0xc;
    q = PO(W(obj, 0));
    W(q, 0x44) = AO(obj + 8) - AO(q);
    if (flags != 0) {
        W(obj, 0x48) = A(gOstreamVt1);
        OstreamIosBaseDtor(reinterpret_cast<MacOstreamIos*>(obj + 0x48), 0);
    }
    if (flags > 0) OstreamDelete(self);
    return self;
}

// std::basic_ostream<char> constructor (0x10072828) and destructor (0x10070000), unit v5116.

// Data blocks the vtable words point at (the Mac's 0x1029d0b0, 0x1029d0d8, 0x103bd034).
char gOstreamVt1[0x28];
char gOstreamVt2[0x20];
char gIosBaseVt[0x20];

namespace {
uint32_t Word(const void* address) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(address)); }
}  // namespace

// 0x10070000, 152 bytes. deleteFlags is the hidden short: nonzero destroys the virtual ios part, above zero frees.
MacOstream* BasicOstreamDtor(MacOstream* self, int16_t deleteFlags) {
    if (self == nullptr) return self;
    self->mVtbl = Word(gOstreamVt2);
    MacOstreamIos* ios = self->mIos.get();
    ios->mVtbl = Word(gOstreamVt2) + 0xc;
    ios = self->mIos.get();
    ios->mVbaseOffset = static_cast<int32_t>(reinterpret_cast<intptr_t>(&self->mIosPart) - reinterpret_cast<intptr_t>(ios));
    if (deleteFlags != 0) {
        self->mIosPart.mVtbl = Word(gOstreamVt1);
        OstreamIosBaseDtor(&self->mIosPart, 0);
    }
    if (deleteFlags > 0) OstreamDelete(self);
    return self;
}

// 0x10072828, 188 bytes. withVirtualBase (a short) nonzero: this is the most derived class and builds the ios part at +8.
MacOstream* BasicOstreamCtor(MacOstream* self, int16_t withVirtualBase, uint32_t streambuf) {
    if (withVirtualBase != 0) {
        self->mIos = &self->mIosPart;
        self->mIosPart.mVtbl = Word(gIosBaseVt);
        self->mIosPart.mVtbl = Word(gOstreamVt1);
    }
    self->mVtbl = Word(gOstreamVt2);
    MacOstreamIos* ios = self->mIos.get();
    ios->mVtbl = Word(gOstreamVt2) + 0xc;
    ios = self->mIos.get();
    ios->mVbaseOffset = static_cast<int32_t>(reinterpret_cast<intptr_t>(&self->mIosPart) - reinterpret_cast<intptr_t>(ios));
    ios = self->mIos.get();
    OstreamIosInit(ios, streambuf);
    ios->mF3c = 0;
    ios->mFill = static_cast<uint8_t>(OstreamWiden(ios, ' '));
    return self;
}

// ParkThemeTree::destroy (0x1016d784, 496 B) and insert_node_at (0x1016eb98, 400 B): the theme map's tree.

using namespace ParkThemeTreeCallee;

namespace {

using node = ParkThemeTree::node;

// The original inlines two levels of the recursion: a grandchild is taken down with the allocator's destroy and deallocate, a child
// with the pair's destructor and operator delete.
void DropInner(ParkThemeTree* tree, node* g) {
    if (g->mLeft != nullptr) tree->destroy(g->mLeft);
    if (g->mRight != nullptr) tree->destroy(g->mRight);
    PairDestroy(tree, &g->mValue);
    NodeDeallocate(tree, g, 1);
}

void DropMid(ParkThemeTree* tree, node* m) {
    node* g = m->mLeft;
    if (g != nullptr) DropInner(tree, g);
    g = m->mRight;
    if (g != nullptr) DropInner(tree, g);
    PairDtor(&m->mValue, -1);
    IME::MslDelete(m);
}

}  // namespace

void ParkThemeTree::destroy(node* subtree) {
    node* child = subtree->mLeft;
    if (child != nullptr) DropMid(this, child);
    child = subtree->mRight;
    if (child != nullptr) DropMid(this, child);
    subtree->mValue.first.TbDynamicStringTemplate<char>::~TbDynamicStringTemplate();   // the key, then the node itself
    IME::MslDelete(subtree);
}

ParkThemeTree::node* ParkThemeTree::insert_node_at(node* parent, bool left, bool leftmost, const Pair& value) {
    static const char kMessage[] = "tree::insert length error";
    static const char kTypes[] = "!std::exception!!std::logic_error!!std::length_error!!";
    if (mCount > 0xfffffffeu) {   // the original goes on when the (faked) throw returns
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kMessage);
        char* copy = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        IME::MslRefCountedCharsCtor(&object.mRc, copy);
        IME::MslStrcpy(object.mRc.mPtr, kMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kTypes, &object, IME::gMslLengthErrorDtorTV);
    }
    node* made = static_cast<node*>(IME::MslNew(sizeof(node)));   // 0x24 on the Mac
    if (made != nullptr) PairCopy(&made->mValue, value);
    made->mLeft = nullptr;
    made->mRight = nullptr;
    made->mParent = reinterpret_cast<uintptr_t>(parent) | (made->mParent & 1);   // the new block's old colour bit survives
    if (left)
        parent->mLeft = made;
    else
        parent->mRight = made;
    mCount = mCount + 1;
    BalanceInsert(made, mRoot);
    if (leftmost) mBegin = made;
    return made;
}

// std::basic_string<char> of the Metrowerks library: assign(str, pos, n) 0x10001c80 and change_size 0x100027b0.
// The string is one word, the address of the shared MslCharRep (len +0, cap +4, refs +8, data +0xc; refs 0 = not shared yet).
// The throw paths build the exception object by hand (strlen, new[], counter, strcpy, __throw); the fakes of __throw return,
// so the code goes on afterwards, as the original does under the test.

namespace msl_char_string {

namespace {

const char kExceptionNames[] = "!std::exception!!std::logic_error!!std::out_of_range!!";   // 0x1023bdb6
const char kLengthErrorNames[] = "!std::exception!!std::logic_error!!std::length_error!!";  // 0x1023bdb6 + 0x37
const char kAppendMessage[] = "basic_string::append pos out of range";                      // 0x1023bc41
const char kResizeMessage[] = "basic_string::length error.  Resizing out of range";         // 0x1023bc92

}  // namespace

// 0x100027b0: makes the string's own representation (refs <= 1) hold n characters, with a terminator after them.
void change_size(MslCharString* self, uint32_t n, bool keep) {
    if (n > 0xFFFFFFFEu) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kResizeMessage);
        object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
            if (count != nullptr) *count = 1;
            object.mRc.mCount = count;
        }
        IME::MslStrcpy(object.mRc.mPtr, kResizeMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kLengthErrorNames, &object, IME::gMslLengthErrorDtorTV);
    }
    if (self->mRep->refs > 1) {
        MslCharRep* old = self->mRep;
        MslCharRep* fresh = static_cast<MslCharRep*>(IME::MslNewRep());
        fresh->cap = (n + 3) - (n & 3);
        fresh->refs = 1;
        fresh->data = static_cast<char*>(IME::MslNew(fresh->cap + 1));
        old->refs = old->refs - 1;
        if (keep) {
            uint32_t count = old->len < n ? old->len : n;
            msl_string_callee::Memcpy(fresh->data, old->data, count + 1);
            fresh->len = old->len;
        }
        self->mRep = fresh;
    }
    MslCharRep* rep = self->mRep;
    if (n > rep->cap) {
        uint32_t size = rep->cap;
        while (size < n) size <<= 1;
        if (size < rep->len) size = rep->len;
        uint32_t cap = (size + 3) - (size & 3);
        if (cap != rep->cap) {   // both listing branches (smaller, larger) do the same copy
            char* old = rep->data;
            rep->data = static_cast<char*>(IME::MslNew(cap + 1));
            msl_string_callee::Memcpy(rep->data, old, rep->len + 1);
            IME::MslDelete(old);
            rep->cap = cap;
        }
    }
    // the address add wraps at 32 bits on the Mac: an index of 2^31 or more reaches below the block
    self->mRep->data[static_cast<int64_t>(static_cast<int32_t>(n))] = gMslChangeSizeNul;
    self->mRep->len = n;
    uint32_t refs = self->mRep->refs;
    if (refs == 0) refs = 1;
    self->mRep->refs = refs;
}

// 0x10001c80: this = src[pos, pos + min(n, size - pos)).
MslCharString* assign(MslCharString* self, const MslCharString* src, uint32_t pos, uint32_t n) {
    if (pos > src->mRep->len) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kAppendMessage);
        char* text = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        IME::MslRefCountedCharsCtor(&object.mRc, text);
        IME::MslStrcpy(object.mRc.mPtr, kAppendMessage);
        object.mVtbl = IME::gMslExcVtblA;
        IME::MslThrow(kExceptionNames, &object, IME::gMslOutOfRangeDtorTV);
    }
    uint32_t rest = src->mRep->len - pos;
    uint32_t count = rest < n ? rest : n;
    MslCharRep* rep = src->mRep;
    if (count == rep->len && rep->refs != 0) {
        MslCharRep* mine = self->mRep;
        if (mine == rep) return self;
        if (mine->refs == 0) mine->refs = 1;
        uint32_t left = mine->refs - 1;
        mine->refs = left;
        if (left != 0) mine = nullptr;
        self->mRep = mine;
        if (self->mRep != nullptr) {
            IME::MslDelete(self->mRep->data);
            IME::MslDelete(self->mRep);
        }
        MslCharRep* from = src->mRep;
        MslCharRep* shared;
        if (from->refs != 0) {
            from->refs = from->refs + 1;
            shared = from;
        } else {
            shared = static_cast<MslCharRep*>(IME::MslNewRep());
            shared->len = from->len;
            shared->cap = (from->len + 3) - (from->len & 3);
            shared->refs = 1;
            shared->data = static_cast<char*>(IME::MslNew(shared->cap + 1));
            msl_string_callee::Memcpy(shared->data, from->data, shared->len + 1);
        }
        self->mRep = shared;
    } else if (self == src) {
        replace(self, 0, pos, 0, 0);
        change_size(self, count, true);
    } else {
        change_size(self, count, false);
        msl_string_callee::Memcpy(self->mRep->data, src->mRep->data + pos, count);
    }
    return self;
}

}  // namespace msl_char_string

// std::basic_string<char> of the Metrowerks library, main app: constructors 0x10002338 and 0x1007363c, destructor 0x1000221c, init
// 0x10002b8c, assign(n, c) 0x100730e8, resize 0x10073558, assign(text, n) 0x101b2440, append 0x1020100c. The string is one word, the
// address of the shared representation (len +0, cap +4, refs +8, data +0xc; native data pointer on the host).

char gMslInitNul;   // 0x10278bd2
char gMslCtorNul;   // 0x1029d294

namespace msl_string_callee {
__attribute__((weak)) void* Memcpy(void* to, const void* from, uint32_t n) { return std::memcpy(to, from, n); }
__attribute__((weak)) void* Memset(void* to, int32_t c, uint32_t n) { return std::memset(to, c, n); }
}  // namespace msl_string_callee

namespace {
const char kLengthErrorName[] = "!std::exception!!std::logic_error!!std::length_error!!";   // 0x1023bded
const char kMaxSizeMessage[] = "string constructor: n > max_size";                          // 0x1023bc67
}  // namespace

namespace msl_char_string {

// 0x1007363c: basic_string(const allocator&)
MslCharString* constructEmpty(MslCharString* self) {
    self->mRep = nullptr;
    self->mRep = static_cast<MslCharRep*>(IME::MslNewRep());
    MslCharRep* rep = self->mRep;
    if (rep != nullptr) {
        rep->cap = 3;
        rep->refs = 1;
        rep->data = static_cast<char*>(IME::MslNew(rep->cap + 1));
    }
    self->mRep->data[0] = gMslCtorNul;
    self->mRep->len = 0;
    return self;
}

// 0x10002b8c: init(first, last, allocator, random_access_iterator_tag)
void init(MslCharString* self, const char* first, const char* last) {
    self->mRep = static_cast<MslCharRep*>(IME::MslNewRep());
    uint32_t n = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(last) - reinterpret_cast<uintptr_t>(first));
    MslCharRep* rep = self->mRep;
    if (rep != nullptr) {
        rep->cap = (n + 3) - (n & 3);
        rep->refs = 1;
        rep->data = static_cast<char*>(IME::MslNew(rep->cap + 1));
    }
    msl_string_callee::Memcpy(self->mRep->data, first, n);
    self->mRep->len = n;
    // The address wraps at 32 bits on the Mac: n = 0xffffffff writes one byte before the data.
    self->mRep->data[static_cast<int32_t>(n)] = gMslInitNul;
}

// 0x10002338: basic_string(const char*, const allocator&)
MslCharString* constructFromCStr(MslCharString* self, const char* text, const void* allocator) {
    (void)allocator;
    self->mRep = nullptr;
    uint32_t n = static_cast<uint32_t>(IME::MslStrlen(text));
    if (n > 0xfffffffeu) {   // n == npos: length_error("string constructor: n > max_size")
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        uint32_t size = static_cast<uint32_t>(IME::MslStrlen(kMaxSizeMessage)) + 1;
        char* message = static_cast<char*>(IME::MslNewArray(size));
        IME::MslRefCountedCharsCtor(&object.mRc, message);
        IME::MslStrcpy(object.mRc.mPtr, kMaxSizeMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kLengthErrorName, &object, IME::gMslLengthErrorDtorTV);
    }
    init(self, text, reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(text) + n));
    return self;
}

// 0x1000221c: ~basic_string(), r4 = the delete flag (a short)
MslCharString* destroy(MslCharString* self, int16_t flag) {
    if (self != nullptr) {
        MslCharRep* rep = self->mRep;
        if (rep->refs == 0) {
            uint32_t refs = rep->refs;
            if (refs == 0) refs = 1;
            rep->refs = refs;
        }
        uint32_t refs = rep->refs - 1;
        rep->refs = refs;
        if (refs != 0) rep = nullptr;
        self->mRep = rep;
        if (self->mRep != nullptr) {
            IME::MslDelete(self->mRep->data);
            IME::MslDelete(self->mRep);
        }
        if (flag > 0) IME::MslDelete(self);
    }
    return self;
}

// 0x100730e8: assign(n, c)
MslCharString* assignFill(MslCharString* self, uint32_t n, char c) {
    change_size(self, n, false);
    msl_string_callee::Memset(self->mRep->data, static_cast<uint8_t>(c), n);
    return self;
}

// 0x10073558: resize(n, c)
void resize(MslCharString* self, uint32_t n, char c) {
    uint32_t old = self->mRep->len;
    change_size(self, n, true);
    if (old < n) msl_string_callee::Memset(self->mRep->data + old, static_cast<uint8_t>(c), n - old);
}

// 0x101b2440: assign(text, n)
MslCharString* assignChars(MslCharString* self, const char* text, uint32_t n) {
    change_size(self, n, false);
    msl_string_callee::Memcpy(self->mRep->data, text, n);
    return self;
}

// 0x1020100c: append(text, n)
MslCharString* appendChars(MslCharString* self, const char* text, uint32_t n) {
    if (n != 0) {
        uint32_t old = self->mRep->len;
        change_size(self, old + n, true);
        msl_string_callee::Memcpy(self->mRep->data + old, text, n);
    }
    return self;
}

}  // namespace msl_char_string

// std::exception::~exception (main app 0x10002184): the MSL exception destructor. A NULL object is left alone; else the vtable
// word is stored and, for a flag above 0 (signed 16 bit), the runtime's operator delete frees it. Answers this.
// The listing's landing pad (__unexpected) is not reachable.

namespace TPMainExc {

Exception* ExceptionDtor(Exception* self, int16_t flag) {   // 0x10002184
    if (self != nullptr) {
        self->mVtbl = IME::gMslExcVtblC;
        if (flag > 0) OperatorDelete(self);
    }
    return self;
}

}  // namespace TPMainExc

// Unit v5949: the Metrowerks exception objects' destructors, the char reference-counted pointer and the 3D line blend.

namespace IME {

// std::_RefCountedPtr<char, std::_Array<char>>::~_RefCountedPtr 0x100025e0 (flag is the hidden delete flag, signed 16 bit).
MslRefCountedChars* MslRefCountedCharsDtor25e0(MslRefCountedChars* self, int16_t deleteFlag) {
    if (self != nullptr) {
        int32_t* count = self->mCount;
        if (count != nullptr) {
            int32_t v = *count - 1;
            *count = v;
            if (v == 0) {
                MslDeleteArray(self->mPtr);
                MslDelete(self->mCount);
            }
        }
        if (deleteFlag > 0) MslDelete(self);
    }
    return self;
}

// std::_RefCountedPtr<char, std::_Array<char>>::_RefCountedPtr(char*) 0x100026ac. (The exception cleanup path of the
// original, delete[] then throw, is only reached when operator new throws, which it never does.)
void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    self->mPtr = p;
    self->mCount = nullptr;
    if (self->mPtr != nullptr) {
        int32_t* count = static_cast<int32_t*>(MslNew(4));
        if (count != nullptr) *count = 1;
        self->mCount = count;
    }
}

// std::logic_error::~logic_error 0x100020cc
MslExceptionObject* MslLogicErrorDtor(MslExceptionObject* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mVtbl = gMslExcVtblB;
        MslRefCountedCharsDtor25e0(&self->mRc, -1);
        self->mVtbl = gMslExcVtblC;
        if (deleteFlag > 0) MslDelete(self);
    }
    return self;
}

// std::out_of_range::~out_of_range 0x10002008
MslExceptionObject* MslOutOfRangeDtor(MslExceptionObject* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mVtbl = gMslExcVtblA;
        self->mVtbl = gMslExcVtblB;
        MslRefCountedCharsDtor25e0(&self->mRc, -1);
        self->mVtbl = gMslExcVtblC;
        if (deleteFlag > 0) MslDelete(self);
    }
    return self;
}

}  // namespace IME

// std::basic_string<wchar_t> of the Metrowerks library (16-bit characters), main app:
//   compare 0x101d5864, assign 0x101f7014, substring constructor 0x101f7424.
// The string object is one pointer to a shared representation (see include/ui_ime.h). The Mac's exception landing pads
// (unreachable code after the inline throw sequences) are not translated. The fakes of __throw return, so the code after the
// throw runs on, as in the original.

namespace IME {

char gMslExcVtblA[16];        // 0x10278ba0
char gMslExcVtblB[16];        // 0x10278bb0
char gMslExcVtblC[16];        // 0x10278bc0
char gMslOutOfRangeDtorTV[8]; // 0x10265698

namespace {

inline MslWideStringRep*& RepOf_2(const MslWideString* s) {
    return *reinterpret_cast<MslWideStringRep**>(const_cast<MslWideString*>(s));
}

const char kOutOfRangeType[] = "!std::exception!!std::logic_error!!std::out_of_range!!";

// The throw sequence of compare: the counted message goes through the _RefCountedPtr constructor.
void ThrowCompare() {
    const char* msg = "string compare: pos > len";
    MslExceptionObject ex;
    ex.mVtbl = gMslExcVtblC;
    ex.mVtbl = gMslExcVtblB;
    size_t n = MslStrlen(msg);
    char* p = static_cast<char*>(MslNewArray(static_cast<uint32_t>(n + 1)));
    MslRefCountedCharsCtor(&ex.mRc, p);
    MslStrcpy(ex.mRc.mPtr, msg);
    ex.mVtbl = gMslExcVtblA;
    MslThrow(kOutOfRangeType, &ex, gMslOutOfRangeDtorTV);
}

// The inline throw sequence of assign and the constructor: the counter is built by hand.
void ThrowInline(const char* msg) {
    MslExceptionObject ex;
    ex.mVtbl = gMslExcVtblC;
    ex.mVtbl = gMslExcVtblB;
    size_t n = MslStrlen(msg);
    ex.mRc.mPtr = static_cast<char*>(MslNewArray(static_cast<uint32_t>(n + 1)));
    ex.mRc.mCount = nullptr;
    if (ex.mRc.mPtr != nullptr) {
        int32_t* count = static_cast<int32_t*>(MslNew(4));
        if (count != nullptr) *count = 1;
        ex.mRc.mCount = count;
    }
    MslStrcpy(ex.mRc.mPtr, msg);
    ex.mVtbl = gMslExcVtblA;
    MslThrow(kOutOfRangeType, &ex, gMslOutOfRangeDtorTV);
}

// Shares a representation: one more reference, or (not shared yet) a copy with a count of 1.
MslWideStringRep* ShareRep(MslWideStringRep* r) {
    if (r->mRefs != 0) {
        r->mRefs = r->mRefs + 1;
        return r;
    }
    MslWideStringRep* p = static_cast<MslWideStringRep*>(MslNew(0x10));
    if (p != nullptr) {
        p->mLength = r->mLength;
        p->mCapacity = (r->mLength + 3) - (r->mLength & 3);
        p->mRefs = 1;
        p->mData = static_cast<char16_t*>(MslNew((p->mCapacity + 1) * 2));
        MslWmemcpy(p->mData, r->mData, p->mLength + 1);
    }
    return p;
}

}  // namespace

// 0x101d5864: compare(pos, n1, s, n2) const
int32_t MslWideString_compare(const MslWideString* self, uint32_t pos, uint32_t n1, const char16_t* s, uint32_t n2) {
    if (pos > RepOf_2(self)->mLength) ThrowCompare();
    uint32_t avail = RepOf_2(self)->mLength - pos;
    uint32_t m1 = (n1 < avail) ? n1 : avail;
    uint32_t m2 = (n2 < m1) ? n2 : m1;
    int32_t r = MslWmemcmp(RepOf_2(self)->mData + pos, s, m2);
    if (r != 0) return r;
    if (m1 < n2) return -1;
    if (m1 == n2) return 0;
    return 1;
}

// 0x101f7014: assign(str, pos, n)
MslWideString* MslWideString_assign(MslWideString* self, const MslWideString* str, uint32_t pos, uint32_t n) {
    if (pos > RepOf_2(str)->mLength) ThrowInline("basic_string::append pos out of range");
    uint32_t avail = RepOf_2(str)->mLength - pos;
    uint32_t cnt = (avail < n) ? avail : n;
    MslWideStringRep* r = RepOf_2(str);
    if (cnt == r->mLength && r->mRefs != 0) {
        MslWideStringRep* mine = RepOf_2(self);
        if (mine == r) return self;
        if (mine->mRefs == 0) mine->mRefs = 1;
        uint32_t v = mine->mRefs;
        mine->mRefs = v - 1;
        if (v - 1 != 0) mine = nullptr;
        RepOf_2(self) = mine;
        if (RepOf_2(self) != nullptr) {
            MslDelete(RepOf_2(self)->mData);
            MslDelete(RepOf_2(self));
        }
        MslWideStringRep* p = ShareRep(RepOf_2(str));
        RepOf_2(self) = p;
    } else if (self == str) {
        MslWideString_replace(self, 0, pos, 0, u'\0');
        MslWideString_changeSize(self, cnt, true);
    } else {
        MslWideString_changeSize(self, cnt, false);
        MslWmemcpy(RepOf_2(self)->mData, RepOf_2(str)->mData + pos, cnt);
    }
    return self;
}

// 0x101f7424: basic_string(str, pos, n)
MslWideString* MslWideString_construct(MslWideString* self, const MslWideString* str, uint32_t pos, uint32_t n) {
    RepOf_2(self) = RepOf_2(str);
    uint32_t len = RepOf_2(str)->mLength;
    if (pos > len) ThrowInline("string copy constructor: pos > str len");
    uint32_t cnt = len - pos;
    if (cnt > n) cnt = n;
    if (cnt == len) {
        MslWideStringRep* p = ShareRep(RepOf_2(str));
        RepOf_2(self) = p;
    } else {
        const char16_t* data = RepOf_2(str)->mData;
        MslWideString_init(self, data + pos, data + pos + cnt);
    }
    return self;
}

}  // namespace IME

namespace std {
void __cdeque<const void*, allocator<const void*>>::swap(__cdeque& other) {
    if (this == &other) return;
    uint32_t t = mWord0; mWord0 = other.mWord0; other.mWord0 = t;
    t = mWord4; mWord4 = other.mWord4; other.mWord4 = t;
    t = mWord8; mWord8 = other.mWord8; other.mWord8 = t;
    t = mWordC; mWordC = other.mWordC; other.mWordC = t;
}
}  // namespace std

// Unit v5115: the CodeWarrior std::deque<errorthingy>: pop_front 0x101f2074, ~deque 0x101f52e0, make_iterator 0x101f53b4.

namespace {
// The deque's words are Mac addresses; on the host they are offsets from the arena base (like Ptr32), 0 staying null.
inline void* Ptr(uint32_t word) { return word != 0 ? static_cast<void*>(tpw::gArenaBase + word) : nullptr; }
inline uint32_t* Words_2(uint32_t word) { return static_cast<uint32_t*>(Ptr(word)); }
inline uint32_t Word_2(const void* address) { return address != nullptr ? tpw::ArenaOffset(address) : 0; }
}  // namespace

// Reads the block of the front record (the answer is dropped), moves the front on, and when the deque became empty lets go of every
// block but two (the table's last used slots, from the back) and recentres the front index. Bug kept: a block count under 2 loops
// about 2^32 times.
void MslErrorDeque::pop_front() {
    errdeque_callee::CdequeIndex(&mBlocks, mFirst / mBlockSize);
    mFirst = mFirst + 1;
    mSize = mSize - 1;
    if (mSize == 0) {
        uint32_t left = mBlocks.mWord4 - 2;
        while (left != 0) {
            mBlocks.mWord4 = mBlocks.mWord4 - 1;
            uint32_t index = mBlocks.mWord4 - 1 + mBlocks.mWord8;
            if (index >= mBlocks.mWord0) index = index - mBlocks.mWord0;
            uint32_t* slot = Words_2(mBlocks.mWordC) + index;
            errdeque_callee::OperatorDelete(Ptr(*slot));
            left = left - 1;
            *slot = 0;
        }
        mFirst = mBlockSize >> 1;
    }
}

// The iterator at `index`: inside the first block the record is read from the first slot's block (0 when the table is empty), past it
// the cursor is moved on by index / blockSize blocks.
void MslErrorDeque::make_iterator(MslErrorDequeIterator* out, uint32_t index) const {
    MslErrorDequeCursor cursor;
    cursor.mCdeque = Word_2(&mBlocks);
    cursor.mWord4 = 0;
    cursor.mSlot = mBlocks.mWordC + mBlocks.mWord8 * 4;
    uint32_t block;
    uint32_t element;
    if (index < mBlockSize) {
        block = mBlocks.mWord4 != 0 ? *Words_2(cursor.mSlot) : 0;
        element = block + index * 8;
    } else {
        errdeque_callee::CursorAdd(&cursor, static_cast<int32_t>(index / mBlockSize));
        block = *Words_2(cursor.mSlot);
        element = block + (index - index / mBlockSize * mBlockSize) * 8;
    }
    out->mCursor = cursor;
    out->mElement = element;
    out->mBlockEnd = block + mBlockSize * 8;
    out->mIndex = index;
}

// tear_down, then the block table (the member's destructor, guarded by two address tests that only fail at the top of the Mac's address
// space), then the object itself when the delete flag is positive.
MslErrorDeque* MslErrorDequeDestroy(MslErrorDeque* self, int16_t deleteFlag) {
    if (self != nullptr) {
        errdeque_callee::TearDown(self);
        const uint32_t member = Word_2(self) + 4;
        if (member != 0) {
            if (member != 0) {
                if (self->mBlocks.mWordC != 0) errdeque_callee::OperatorDelete(Ptr(self->mBlocks.mWordC));
            }
        }
        if (deleteFlag > 0) errdeque_callee::OperatorDelete(self);
    }
    return self;
}

// std::deque<errorthingy> of the Metrowerks library: push_back and the block-table growth. All words are the Mac's 32-bit words;
// block pointers are kept as 32-bit values (the heap of the host tests is 32-bit addressable).
uint32_t gMslErrorDequePad = 0;   // 0x102b141c

namespace {
using Cd = std::__cdeque<const void*, std::allocator<const void*>>;

// A 32-bit Mac address as a host pointer (the upper half is gMslMacAddressHigh; 0 stays null).
uint32_t* Native(uint32_t word) {
    return word == 0 ? nullptr : reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | word));
}
uint32_t* TableOf(const Cd& cd) { return Native(cd.mWordC); }
}  // namespace

// 0x101f2e7c: adds blocks until `to - from` records are covered. The block is allocated before the table is looked at; a full table
// is swapped with `spare`, grown and refilled from it (once); the new block replaces the last table slot, then the table gets one more
// word. Answers whether the swap happened.
uint32_t MslErrorDeque_grow_buf_back(MslErrorDeque* self, uint32_t from, Cd* spare, uint32_t* added, uint32_t to) {
    uint32_t remaining = to - from;
    uint32_t swapped = 0;
    for (;;) {
        void* block = IME::MslNew(self->mBlockSize << 3);
        Cd& mine = self->mBlocks;
        if (swapped == 0 && mine.mWord4 == mine.mWord0) {
            if (&mine != spare) {
                uint32_t t = mine.mWord0;
                mine.mWord0 = spare->mWord0;
                spare->mWord0 = t;
                t = mine.mWord4;
                mine.mWord4 = spare->mWord4;
                spare->mWord4 = t;
                t = mine.mWord8;
                mine.mWord8 = spare->mWord8;
                spare->mWord8 = t;
                t = mine.mWordC;
                mine.mWordC = spare->mWordC;
                spare->mWordC = t;
            }
            swapped = 1;
            errdeque_callee::CdequeReserve(&mine, spare->mWord0 << 1);
            uint32_t pos = spare->mWord8 + spare->mWord4;
            uint32_t* tab = TableOf(*spare);
            MslErrorDequeAssignIter last;
            last.mCdeque = spare;
            last.mIndex = spare->mWord4;
            last.mSlot = pos < spare->mWord0 ? tab + pos : tab + (pos - spare->mWord0);
            MslErrorDequeAssignIter first;
            first.mCdeque = spare;
            first.mIndex = 0;
            first.mSlot = tab + spare->mWord8;
            errdeque_callee::CdequeDoAssign(&mine, first, last);
        }
        uint32_t index = mine.mWord4 - 1 + mine.mWord8;
        if (index >= mine.mWord0) index -= mine.mWord0;
        TableOf(mine)[index] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(block));
        errdeque_callee::CdequePushBack(&mine, &gMslErrorDequePad);
        *added = *added + 1;
        if (remaining <= self->mBlockSize) break;
        remaining -= self->mBlockSize;
    }
    return swapped;
}

// 0x101f21ac: the length check throws (the fakes of __throw return, so the code goes on), an empty block table gets two slots and one
// entry, the table grows when the last block is full, the record is copied, and the leading blocks are released once a block and a
// half lies in front of the first record.
void MslErrorDeque_push_back(MslErrorDeque* self, const uint32_t* record) {
    if (self->mSize > 0x1ffffffe) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblB;
        const char* message = "deque::push_back length error";
        uint32_t length = static_cast<uint32_t>(IME::MslStrlen(message));
        char* text = static_cast<char*>(IME::MslNewArray(length + 1));
        IME::MslRefCountedCharsCtor(&object.mRc, text);
        IME::MslStrcpy(object.mRc.mPtr, message);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, IME::gMslLengthErrorDtorTV);
    }
    if (self->mBlocks.mWord4 == 0) {
        errdeque_callee::CdequeReserve(&self->mBlocks, 2);
        errdeque_callee::CdequeResize(&self->mBlocks, 1, nullptr);
    }
    Cd spare;
    spare.mWord0 = 0;
    spare.mWord4 = 0;
    spare.mWord8 = 0;
    spare.mWordC = 0;
    uint32_t added = 0;
    if (self->mFirst + self->mSize == self->mBlockSize * (self->mBlocks.mWord4 - 1))
        MslErrorDeque_grow_buf_back(self, 0, &spare, &added, 1);
    uint32_t index = self->mFirst + self->mSize;
    uint32_t element;
    if (index < self->mBlockSize) {
        element = TableOf(self->mBlocks)[self->mBlocks.mWord8] + index * 8;
    } else {
        uint32_t number = self->mBlockSize != 0 ? index / self->mBlockSize : 0;
        const uint32_t* slot = static_cast<const uint32_t*>(errdeque_callee::CdequeIndex(&self->mBlocks, number));
        uint32_t rest = index - (self->mBlockSize != 0 ? index / self->mBlockSize : 0) * self->mBlockSize;
        element = *slot + rest * 8;
    }
    if (element != 0) {
        uint32_t* target = Native(element);
        uint32_t w0 = record[0];
        uint32_t w1 = record[1];
        target[0] = w0;
        target[1] = w1;
    }
    self->mSize = self->mSize + 1;
    if (spare.mWordC != 0) errdeque_callee::OperatorDelete(Native(spare.mWordC));
    while (self->mFirst >= (self->mBlockSize * 3) >> 1) {
        errdeque_callee::OperatorDelete(Native(TableOf(self->mBlocks)[self->mBlocks.mWord8]));
        errdeque_callee::CdequePopFront(&self->mBlocks);
        self->mFirst = self->mFirst - self->mBlockSize;
    }
}

// std::basic_ifstream<char>::~basic_ifstream, the copy at 0x1020448c (unit v5642).

extern char gIfstreamVtbl[];   // 0x102b1778 (also declared in FS_MasterSave.h)

namespace {
// A Mac address as a 32-bit word: an arena offset for blocks, the low 32 bits for the program's own globals.
uint32_t A_2(const void* p) {
    return tpw::InArena(p) ? tpw::ArenaOffset(p) : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}
void* P(uint32_t a) { return tpw::gArenaBase + a; }
uint32_t& W_2(uint32_t base, uint32_t off) { return *reinterpret_cast<uint32_t*>(tpw::gArenaBase + (base + off)); }
}  // namespace

void* BasicIfstreamDtorEmail(void* selfp, int16_t flags) {
    if (selfp == nullptr) return selfp;
    uint32_t self = A_2(selfp);
    uint32_t files = A_2(gIfsWFiles);
    W_2(self, 8) = A_2(gIfstreamVtbl);
    W_2(W_2(self, 0), 0) = A_2(gIfstreamVtbl) + 0xc;
    W_2(W_2(self, 0), 0x44) = (self + 0x4c) - W_2(self, 0);
    if (self + 0xc != 0) {
        W_2(self, 0xc) = A_2(gFilebufCharVtblB);
        uint32_t file = W_2(self, 0x30);
        if (file != files && file != files + 0x54 && file != files + 0xa8) OfstreamClose(P(self + 0xc));
        if (self + 0xc != 0) {
            W_2(self, 0xc) = A_2(gFilebufCharVtblA);
            if (self + 0x28 != 0) OfstreamLocaleDtor(P(self + 0x28), -1);
        }
    }
    W_2(self, 8) = A_2(gIfstreamVtblC);
    W_2(W_2(self, 0), 0) = A_2(gIfstreamVtblC) + 0xc;
    W_2(W_2(self, 0), 0x44) = (self + 0xc) - W_2(self, 0);
    if (flags != 0 && self + 0x4c != 0) {
        W_2(self, 0x4c) = A_2(gOstreamVt1);
        OstreamIosBaseDtor(static_cast<MacOstreamIos*>(P(self + 0x4c)), 0);
    }
    if (flags > 0) OstreamDelete(selfp);
    return selfp;
}

// std::operator<<(basic_ostream<char>&, const char*) 0x10207150 (unit v5700): the sentry (tie flush, good check), the text written by
// __putnumber with the stream's fill, the failure state and the ios_base::failure throw, then the unitbuf flush.
// The exception-cleanup blocks of the listing (0x10207220 on) are unreachable and not translated.

namespace {
const char kType[] = "!std::exception!!std::ios_base::failure!!";   // 0x10250098
const char kMessage[] = "ios_base failure in clear";                 // 0x1024fde4
}  // namespace

MacWriteStream* OstreamWriteText(MacWriteStream* os, const char* text) {
    bool ok = false;
    const bool sentryDone = false;   // the byte at 0xd1 is only set on the unwinding path
    MacIosBase* base = reinterpret_cast<MacIosBase*>(os->mIos.get());
    if (!MslIosGood(base)) {
        MslIosSetstate(base, 4);
    } else {
        if (MslIosTie(base) != nullptr) MslOstreamFlush(MslIosTie(base));
        if (MslIosGood(base)) ok = true;
        else MslIosSetstate(base, 4);
    }
    if (ok) {
        MacWriteIos* ios = os->mIos.get();
        MacPutStreamBuf* iter = ios->mStreamBuf.get();
        int32_t length = static_cast<int32_t>(IME::MslStrlen(text));
        MacPutStreamBuf* result =
            PutNumberChar(iter, reinterpret_cast<MacPutIos*>(ios), static_cast<char>(ios->mFill), nullptr, 0, text, length);
        if (result == nullptr) {
            ios = os->mIos.get();
            ios->mState |= 5;
            if (ios->mStreamBuf.get() == nullptr) ios->mState |= 1;
            if ((ios->mState & ios->mExceptions) != 0) {
                MslThrowObject object;
                object.mVtbl = gIosFailureVtbl;
                char* message = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(kMessage) + 1)));
                object.mMessage = message;
                object.mCount = nullptr;
                if (message != nullptr) {
                    int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
                    if (count != nullptr) *count = 1;
                    object.mCount = count;
                }
                IME::MslStrcpy(object.mMessage, kMessage);
                ::MslThrow(kType, &object, gIosFailureDtorVec);
            }
        }
    }
    MacWriteIos* last = os->mIos.get();
    if ((last->mState & 5) == 0 && (last->mFlags & 0x2000) != 0 && !sentryDone) MslOstreamFlush(os);
    return os;
}

// std::operator< for std::basic_string<wchar_t, nocase_wchar_traits>, main app, 0x10225d2c (140 B).

// 0x10225d2c: a.compare(0, a.size(), b.begin(), b.size()) < 0. The calls run in the listing's order: b.size(), b.begin(), a.size().
bool NocaseWStringLess(const void* a, const void* b) {
    uint32_t bSize = online_lt::NocaseWStringSize(b);
    const char16_t* bBegin = online_lt::NocaseWStringBegin(b);
    uint32_t aSize = online_lt::NocaseWStringSize(a);
    int32_t result = online_callee::NocaseWStringCompare(a, 0, aSize, bBegin, bSize);
    return result < 0;
}

// std::__red_black_tree<__int128>: the Metrowerks standard library's red-black tree code that every set and map shares (main app
// 0x10152db0 increment, 0x10153290 balance_erase, 0x10153dec rotate_left, 0x10153efc rotate_right; the inventory files them under
// RideDB.cpp). Unit w107. Colour: bit 0 of a node's parent word, 1 = red. A null child counts as black. balance_insert (0x10153b24) is
// not part of this unit.

namespace {

using node_base = MslRedBlackTree::node_base;

inline node_base* ParentOf(const node_base* node) { return reinterpret_cast<node_base*>(node->mParent & ~uintptr_t(1)); }
inline bool IsRed(const node_base* node) { return (node->mParent & 1) != 0; }
inline void MakeRed(node_base* node) { node->mParent |= 1; }
inline void MakeBlack(node_base* node) { node->mParent &= ~uintptr_t(1); }
// A parent pointer with the child's own colour kept.
inline void SetParent(node_base* node, node_base* parent) { node->mParent = reinterpret_cast<uintptr_t>(parent) | (node->mParent & 1); }
// null or black
inline bool IsBlack(const node_base* node) { return node == nullptr || !IsRed(node); }

}  // namespace

// The successor: with a right child, its leftmost descendant; otherwise climb until we come up out of a left child (the root's parent is
// the end node, whose left word is the root, so the walk stops there after the last node).
void MslRedBlackTree::increment(node_base*& iterator) {
    node_base* right = iterator->mRight;
    if (right == nullptr) {
        node_base* parent;
        do {
            const node_base* current = iterator;
            parent = ParentOf(current);
            iterator = parent;
            if (current == parent->mLeft) break;
        } while (true);
        return;
    }
    node_base* node;
    do {
        node = right;
        right = node->mLeft;
    } while (right != nullptr);
    iterator = node;
}

void MslRedBlackTree::rotate_left(node_base* node, node_base*& root) {
    node_base* pivot = node->mRight;
    if (root == node) root = pivot;
    node->mRight = pivot->mLeft;
    if (pivot->mLeft != nullptr) SetParent(pivot->mLeft, node);
    pivot->mParent = (pivot->mParent & 1) | (node->mParent & ~uintptr_t(1));
    node_base* parent = ParentOf(node);
    if (node == parent->mLeft)
        parent->mLeft = pivot;
    else
        parent->mRight = pivot;
    pivot->mLeft = node;
    SetParent(node, pivot);
}

void MslRedBlackTree::rotate_right(node_base* node, node_base*& root) {
    node_base* pivot = node->mLeft;
    if (root == node) root = pivot;
    node->mLeft = pivot->mRight;
    if (pivot->mRight != nullptr) SetParent(pivot->mRight, node);
    pivot->mParent = (pivot->mParent & 1) | (node->mParent & ~uintptr_t(1));
    node_base* parent = ParentOf(node);
    if (node == parent->mLeft)
        parent->mLeft = pivot;
    else
        parent->mRight = pivot;
    pivot->mRight = node;
    SetParent(node, pivot);
}

// The textbook delete: y (z, or z's in-order successor when z has two children) is spliced out and, when it takes z's place, adopts
// z's links and colour; if a black node left, the colours are repaired from x (y's only child, possibly null) upwards. `root` is a
// local copy of the root: the rotations update it, and the loop compares with it. The Mac's loop keeps x's parent and which side x is
// on in two variables because x can be null.
void MslRedBlackTree::balance_erase(node_base* z, node_base* root) {
    node_base* y = z;
    if (z->mLeft != nullptr) {
        node_base* right = z->mRight;
        if (right != nullptr) {
            while (right->mLeft != nullptr) right = right->mLeft;
            y = right;
        }
    }
    node_base* x = y->mLeft;
    if (x == nullptr) x = y->mRight;
    node_base* xParent = ParentOf(y);
    if (x != nullptr) SetParent(x, xParent);
    bool xIsLeft;
    node_base* yParent = ParentOf(y);
    if (y == yParent->mLeft) {
        yParent->mLeft = x;
        xIsLeft = true;
    } else {
        xIsLeft = false;
        yParent->mRight = x;
    }
    bool rebalance = !IsRed(y);
    if (y == z) {
        // the tree's last node leaves nothing to repair
        if (x == nullptr && root == z) rebalance = false;
    } else {
        // y takes z's place
        y->mParent = (y->mParent & 1) | (z->mParent & ~uintptr_t(1));
        node_base* parent = ParentOf(y);
        if (z == parent->mLeft)
            parent->mLeft = y;
        else
            parent->mRight = y;
        y->mLeft = z->mLeft;
        if (y->mLeft != nullptr)
            SetParent(y->mLeft, y);
        else
            xParent = y;
        y->mRight = z->mRight;
        if (y->mRight != nullptr)
            SetParent(y->mRight, y);
        else
            xParent = y;
        if (IsRed(z))
            MakeRed(y);
        else
            MakeBlack(y);
        if (root == z) root = y;
    }
    if (!rebalance) return;
    while (x != root && IsBlack(x)) {
        if (xIsLeft) {
            node_base* sibling = xParent->mRight;
            if (IsRed(sibling)) {
                MakeBlack(sibling);
                MakeRed(xParent);
                rotate_left(xParent, root);
                sibling = xParent->mRight;
            }
            if (IsBlack(sibling->mLeft) && IsBlack(sibling->mRight)) {
                MakeRed(sibling);
                x = xParent;
                node_base* parent = ParentOf(xParent);
                xIsLeft = parent->mLeft == xParent;
                xParent = parent;
            } else {
                if (IsBlack(sibling->mRight)) {
                    MakeBlack(sibling->mLeft);
                    MakeRed(sibling);
                    rotate_right(sibling, root);
                    sibling = xParent->mRight;
                }
                if (IsRed(xParent))
                    MakeRed(sibling);
                else
                    MakeBlack(sibling);
                MakeBlack(xParent);
                MakeBlack(sibling->mRight);
                rotate_left(xParent, root);
                x = root;
            }
        } else {
            node_base* sibling = xParent->mLeft;
            if (IsRed(sibling)) {
                MakeBlack(sibling);
                MakeRed(xParent);
                rotate_right(xParent, root);
                sibling = xParent->mLeft;
            }
            if (IsBlack(sibling->mLeft) && IsBlack(sibling->mRight)) {
                MakeRed(sibling);
                x = xParent;
                node_base* parent = ParentOf(xParent);
                xIsLeft = parent->mLeft == xParent;
                xParent = parent;
            } else {
                if (IsBlack(sibling->mLeft)) {
                    MakeBlack(sibling->mRight);
                    MakeRed(sibling);
                    rotate_left(sibling, root);
                    sibling = xParent->mLeft;
                }
                if (IsRed(xParent))
                    MakeRed(sibling);
                else
                    MakeBlack(sibling);
                MakeBlack(xParent);
                MakeBlack(sibling->mLeft);
                rotate_right(xParent, root);
                x = root;
            }
        }
    }
    if (x != nullptr) MakeBlack(x);
}

// SubscriptionTree::insert_one, main app 0x101530e4 (280 bytes), unit w124: the unique insertion of the message centre's subscription tree (called by
// CMessageCentre::Subscribe every time). The key compares by its thing id, unsigned 16 bit. The Mac answers {node, inserted} through a hidden buffer.

SubscriptionInsertResult SubscriptionTree::insert_one(const CSubscription& key) {
    node_base* parent = end();
    node_base* current = mRoot;
    node_base* candidate = nullptr;
    bool left = true;
    bool leftmost = true;
    while (current != nullptr) {
        parent = current;
        uint16_t value = static_cast<node*>(current)->mValue.mThingId;
        if (key.mThingId < value) {
            left = true;
            current = current->mLeft;
        } else {
            left = false;
            leftmost = false;
            candidate = current;
            current = current->mRight;
        }
    }
    if (candidate != nullptr && static_cast<node*>(candidate)->mValue.mThingId >= key.mThingId) {
        return SubscriptionInsertResult{candidate, 0};
    }
    node_base* inserted = insert_node_at(parent, left, leftmost, key);
    return SubscriptionInsertResult{inserted, 1};
}

// std::__tree<CSubscription, std::less<CSubscription>, std::allocator<CSubscription>> (a std::set of thing ids) and its allocators,
// main app 0x1000fae0 .. 0x1000ff04 and 0x10152c58 .. 0x10152f3c (the inventory files them under Advisor.cpp and RideDB.cpp). Unit
// w107. insert_one (0x101530e4), insert_node_at (0x101538d8) and balance_insert are the subscribe path's and not part of this unit.
// The node: left +0, right +4, parent +8 (colour in bit 0), the id at +0xc; the tree keeps its count and root (the begin word is
// derived on the host, see include/MslSubscriptionTree.h).

// 0x1000ff04: zero count (the Mac copies a static word that holds 0), no root; begin = end follows from the empty root.
void SubscriptionTree::Construct() {
    mCount = 0;
    mRoot = nullptr;
}

SubscriptionTree::node_base* SubscriptionTree::begin() {
    node_base* node = mRoot;
    if (node == nullptr) return end();
    while (node->mLeft != nullptr) node = node->mLeft;
    return node;
}

// 0x1000fae0: the Mac unrolls two levels of this recursion; the frees come in the same order, children left to right and then the
// node itself. (std::allocator<CSubscription>::destroy between them does nothing.)
void SubscriptionTree::destroy(node* subtree) {
    if (subtree->mLeft != nullptr) destroy(static_cast<node*>(subtree->mLeft));
    if (subtree->mRight != nullptr) destroy(static_cast<node*>(subtree->mRight));
    ::operator delete(subtree);
}

// 0x10152c58: the lower bound (the first node whose id is not below the key), accepted when it is not above the key either, else end().
// The ids compare as unsigned 16-bit numbers.
SubscriptionTree::node_base* SubscriptionTree::find(const CSubscription& key) {
    node_base* candidate = end();
    node_base* current = mRoot;
    while (current != nullptr) {
        if (static_cast<node*>(current)->mValue.mThingId < key.mThingId) {
            current = current->mRight;
        } else {
            candidate = current;
            current = current->mLeft;
        }
    }
    if (candidate != end() && static_cast<node*>(candidate)->mValue.mThingId <= key.mThingId) return candidate;
    return end();
}

// 0x10152e64: nothing happens to a tree without a root (its count is left as it is).
void SubscriptionTree::clear() {
    if (mRoot != nullptr) {
        destroy(static_cast<node*>(mRoot));
        mCount = 0;
        mRoot = nullptr;
    }
}

// 0x10152f3c: unlinks and frees the node and counts it off. The Mac first moves its begin word to the successor when the first node
// goes (loads only); the host derives begin, so there is nothing to do for it.
void SubscriptionTree::erase(node_base* position) {
    MslRedBlackTree::balance_erase(position, mRoot);
    ::operator delete(position);
    mCount = mCount - 1;
}

void SubscriptionAllocator::destroy(CSubscription*) {}

void SubscriptionAllocator::deallocate(SubscriptionTree::node* node, unsigned long) { ::operator delete(node); }

// ui_ErrorDisplay.cpp's static initialiser (0x101f499c, unit v5134).

// The CodeWarrior std::deque<errorthingy>: tear_down 0x101f4a74, copy constructor 0x101f4d14 and destroy 0x101f584c (unit v5680).
// Words are the Mac's 32-bit words; the host address of one is (gMslMacAddressHigh << 32) | word.

uint32_t gMslErrorDequeBlockSize = 0x40;   // 0x102b142c: records per block of a new deque
uint32_t gMslErrorDequeFill = 0;           // 0x102b1430: the word the block table is padded with (its address is passed to push_back)

namespace {

uint32_t* W_3(uint32_t word) { return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | word)); }
uint32_t Lo(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

volatile uint32_t gSink;   // the original reads the blocks it "destroys" (the loops have no other effect)

void Touch(uint32_t block, uint32_t count) {
    for (uint32_t p = block; p < block + count * 8; p += 8) gSink = 0;
}

}  // namespace

// 0x101f584c: destroys the records [first, last). errorthingy has no destructor: only the walk over the blocks remains.
void MslErrorDeque_destroy(MslErrorDeque* self, const MslErrorDequeIterator* first, const MslErrorDequeIterator* last) {
    if (first->mElement == last->mElement) return;
    uint32_t blockSize = self->mBlockSize;
    uint32_t index = first->mIndex;
    uint32_t total = last->mIndex - index;
    uint32_t blocks = blockSize != 0 ? index / blockSize : 0;   // the emulator answers 0 for divwu by 0
    uint32_t offset = index - blocks * blockSize;
    uint32_t span = offset + total;
    uint32_t limit = span < self->mBlockSize ? span : self->mBlockSize;

    MslErrorDequeCursor start;
    start.mCdeque = Lo(&self->mBlocks);
    start.mWord4 = 0;
    start.mSlot = self->mBlocks.mWordC + self->mBlocks.mWord8 * 4;
    MslErrorDequeCursor copy1;
    MslErrorDequeCursor* moved = errdeque_callee::CursorCopy(&copy1, &start);
    moved = MslErrorDequeCursorAddErrorthingy(moved, static_cast<int32_t>(blocks));
    MslErrorDequeCursor cursor;
    errdeque_callee::CursorCopy(&cursor, moved);

    uint32_t slot = cursor.mSlot;
    uint32_t block = *W_3(slot);
    Touch(block + offset * 8, limit - offset);
    uint32_t rest = total - (limit - offset);
    if (rest == 0) return;
    uint32_t* cdeque = W_3(cursor.mCdeque);
    uint32_t table = cdeque[3];
    uint32_t wrapEnd = table + cdeque[0] * 4;
    slot += 4;
    if (slot == wrapEnd) slot = table;
    for (;;) {
        uint32_t size = self->mBlockSize;
        if (rest < size) break;
        uint32_t b = *W_3(slot);
        Touch(b, size);
        slot += 4;
        if (slot == wrapEnd) slot = table;
        rest -= size;
    }
    if (rest == 0) return;
    Touch(*W_3(slot), rest);
}

// 0x101f4a74
void errdeque_callee::TearDown(MslErrorDeque* self) {
    if (self->mBlocks.mWord4 == 0) return;
    MslErrorDequeIterator endIt;
    MslErrorDequeIterator beginIt;
    errdeque_callee::MakeIteratorMutable(&endIt, self, self->mFirst + self->mSize);
    errdeque_callee::MakeIteratorMutable(&beginIt, self, self->mFirst);
    MslErrorDequeIterator endCopy = endIt;
    MslErrorDeque_destroy(self, &beginIt, &endCopy);

    uint32_t first = self->mBlocks.mWord8;
    uint32_t count = self->mBlocks.mWord4;
    uint32_t capacity = self->mBlocks.mWord0;
    uint32_t sum = first + count;
    uint32_t position = sum < capacity ? sum : sum - capacity;
    MslErrorDequeCursor cursor;
    cursor.mCdeque = Lo(&self->mBlocks);
    cursor.mWord4 = count;
    cursor.mSlot = self->mBlocks.mWordC + position * 4;
    MslErrorDequeCursor copy = cursor;
    errdeque_callee::CdequeDecrement(&copy);
    uint32_t n = copy.mWord4;

    uint32_t slot = self->mBlocks.mWordC + self->mBlocks.mWord8 * 4;
    for (uint32_t i = 0; i != n;) {
        errdeque_callee::OperatorDelete(W_3(*W_3(slot)));
        slot += 4;
        i++;
        uint32_t end = self->mBlocks.mWordC + self->mBlocks.mWord0 * 4;
        if (slot == end) slot = self->mBlocks.mWordC;
    }
}

// 0x101f4d14: the copy constructor
void errsinit_callee::DequeCopy(void* selfPtr, const void* otherPtr) {
    MslErrorDeque* self = static_cast<MslErrorDeque*>(selfPtr);
    const MslErrorDeque* other = static_cast<const MslErrorDeque*>(otherPtr);
    self->mBlockSize = gMslErrorDequeBlockSize;
    self->mBlocks.mWord0 = 0;
    self->mBlocks.mWord4 = 0;
    self->mBlocks.mWord8 = 0;
    self->mBlocks.mWordC = 0;
    self->mFirst = 0;
    self->mSize = other->mSize;
    if (self->mSize == 0) return;

    errdeque_callee::CdequeReserve(&self->mBlocks, 2);
    errdeque_callee::CdequeResize(&self->mBlocks, 1, nullptr);
    if (self->mBlockSize < self->mSize) self->mBlockSize = self->mSize;
    uint32_t block = Lo(errdeque_callee::OperatorNew(self->mBlockSize << 3));
    *W_3(self->mBlocks.mWordC + self->mBlocks.mWord8 * 4) = block;
    errdeque_callee::CdequePushBack(&self->mBlocks, &gMslErrorDequeFill);
    self->mFirst = (self->mBlockSize - self->mSize) >> 1;
    uint32_t dst = block + self->mFirst * 8;

    MslErrorDequeIterator endIt;
    MslErrorDequeIterator it;
    other->make_iterator(&endIt, other->mFirst + other->mSize);
    other->make_iterator(&it, other->mFirst);
    while (it.mElement != endIt.mElement) {
        if (dst != 0) {
            const uint32_t* src = W_3(it.mElement);
            uint32_t* d = W_3(dst);
            d[0] = src[0];
            d[1] = src[1];
        }
        it.mIndex++;
        dst += 8;
        it.mElement += 8;
        if (it.mElement == it.mBlockEnd) {
            uint32_t current = *W_3(it.mCursor.mSlot);
            int32_t records = static_cast<int32_t>(it.mBlockEnd - current) / 8;
            it.mCursor.mWord4++;
            uint32_t slot = it.mCursor.mSlot + 4;
            const uint32_t* cdeque = W_3(it.mCursor.mCdeque);
            if (slot == cdeque[3] + cdeque[0] * 4) slot = cdeque[3];
            it.mCursor.mSlot = slot;
            it.mElement = *W_3(slot);
            it.mBlockEnd = it.mElement + static_cast<uint32_t>(records) * 8;
        }
    }
}

// std::_RefCountedPtr<std::__locale_imp<__int128>, std::_Single<...>>::operator= (0x10072128, 368 B).

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

// std::__cdeque<const void*>::do_assign<std::__cdeque<errorthingy*>::iterator> 0x101f3278 (832 bytes): assigns the range [first, last)
// of the errorthingy block table to this block table. Word0 is the capacity, word4 the size, word8 the start, wordC the block.

namespace {

// The Mac's word is the whole address; the host's heap lies above 4 GB, the upper half is gMslMacAddressHigh.
uint32_t* BlockOf(uint32_t word) { return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | word)); }

// The copy loop both branches run: one word per index, the source pointer wrapping at the end of the source's block.
void CopyRange(uint32_t* dst, const MslErrorDequeAssignIter& first, uint32_t lastIndex) {
    const std::__cdeque<const void*, std::allocator<const void*>>* src =
        static_cast<const std::__cdeque<const void*, std::allocator<const void*>>*>(first.mCdeque);
    uint32_t index = first.mIndex;
    uint32_t* cur = first.mSlot;
    while (index != lastIndex) {
        *dst = *cur;
        index = index + 1;
        cur = cur + 1;
        uint32_t* block = BlockOf(src->mWordC);
        if (cur == block + src->mWord0) cur = block;
        dst = dst + 1;
    }
}

}  // namespace

namespace errdeque_callee {

void CdequeDoAssign(void* cdeque, const MslErrorDequeAssignIter& first, const MslErrorDequeAssignIter& last) {
    std::__cdeque<const void*, std::allocator<const void*>>* self =
        static_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(cdeque);
    uint32_t count = last.mIndex - first.mIndex;
    if (count > 0x3fffffffu) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        const char* message = "cdeque::assign length error";
        object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(message) + 1)));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            object.mRc.mCount = static_cast<int32_t*>(IME::MslNew(4));
            *object.mRc.mCount = 1;
        }
        IME::MslStrcpy(object.mRc.mPtr, message);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, IME::gMslLengthErrorDtorTV);
    }
    if (count > self->mWord0) {
        uint32_t old = self->mWordC;
        void* block = IME::MslNew(count * 4);
        self->mWordC = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(block));
        CopyRange(BlockOf(self->mWordC), first, last.mIndex);
        if (old != 0) IME::MslDelete(BlockOf(old));
        self->mWord0 = count;
        self->mWord4 = count;
        self->mWord8 = 0;
    } else {
        CopyRange(BlockOf(self->mWordC), first, last.mIndex);
        self->mWord8 = 0;
        self->mWord4 = count;
    }
}

}  // namespace errdeque_callee

// std::__cdeque<const void*>: erase 0x101f3704, resize 0x101f2a10, push_back 0x101f459c.
// The elements are 4-byte words on the Mac; the block is addressed as uint32_t here.

// The Mac's pointer words are offsets from tpw::gArenaBase here (0: null), as in the other 32-bit structures.
namespace {
using Cdeque = std::__cdeque<const void*, std::allocator<const void*>>;

inline uint32_t* BufOf(const Cdeque* d) {
    return d->mWordC != 0 ? reinterpret_cast<uint32_t*>(tpw::gArenaBase + d->mWordC) : nullptr;
}

// Step an iterator back one element (wrapping at the front of the block).
inline void StepBack(MslCdequeIter& it) {
    uint32_t* buf = BufOf(it.mDeque);
    if (it.mPtr == buf) it.mPtr = buf + it.mDeque->mWord0;
    it.mPtr -= 1;
}

// Step forward one element (wrapping at the end of the block).
inline void StepForward(MslCdequeIter& it) {
    it.mPtr += 1;
    uint32_t* buf = BufOf(it.mDeque);
    if (it.mPtr == buf + it.mDeque->mWord0) it.mPtr = buf;
}

inline MslCdequeIter IterAt(Cdeque* d, uint32_t index) {
    uint32_t pos = d->mWord8 + index;
    MslCdequeIter it;
    it.mDeque = d;
    it.mIndex = index;
    it.mPtr = pos < d->mWord0 ? BufOf(d) + pos : BufOf(d) + (pos - d->mWord0);
    return it;
}
}  // namespace

MslCdequeIter std::__cdeque<const void*, std::allocator<const void*>>::erase(MslCdequeIter first, MslCdequeIter last) {
    if (first.mIndex == last.mIndex) return first;
    uint32_t count = last.mIndex - first.mIndex;
    uint32_t after = mWord4 - last.mIndex;
    if (first.mIndex < after) {
        // fewer elements before the range: move them up, from the back
        if (first.mIndex != 0) {
            MslCdequeIter dst = last;
            MslCdequeIter src = first;
            while (src.mIndex != 0) {
                dst.mIndex -= 1;
                StepBack(dst);
                src.mIndex -= 1;
                StepBack(src);
                *dst.mPtr = *src.mPtr;
            }
        }
        mWord8 += count;
        if (mWord8 >= mWord0) mWord8 -= mWord0;
    } else if (after != 0) {
        // move the elements after the range down
        uint32_t size = mWord4;
        MslCdequeIter dst = first;
        MslCdequeIter src = last;
        while (src.mIndex != size) {
            *dst.mPtr = *src.mPtr;
            src.mIndex += 1;
            StepForward(src);
            dst.mIndex += 1;
            StepForward(dst);
        }
    }
    mWord4 -= count;
    return first;
}

void std::__cdeque<const void*, std::allocator<const void*>>::resize(uint32_t n, const void* value) {
    uint32_t size = mWord4;
    if (n > size) {
        MslCdequeIter pos = IterAt(this, size);
        cdeque_callee::Insert(this, pos, n - mWord4, &value);
    } else if (n < size) {
        MslCdequeIter end = IterAt(this, size);
        MslCdequeIter begin;
        begin.mDeque = this;
        begin.mIndex = 0;
        begin.mPtr = BufOf(this) + mWord8;
        MslCdequeIter t1, t2;
        MslCdequeIter* it = cdeque_callee::IterCopy(&t1, &begin);
        it = cdeque_callee::IterAdd(it, static_cast<int32_t>(n));
        cdeque_callee::IterCopy(&t2, it);
        erase(t2, end);
    }
}

void std::__cdeque<const void*, std::allocator<const void*>>::push_back(const void* const& value) {
    static const char kMessage[] = "cdeque::push_back length error";
    static const char kType[] = "!std::exception!!std::logic_error!!std::length_error!!";
    uint32_t element = value != nullptr ? tpw::ArenaOffset(value) : 0;
    if (mWord4 == 0x3fffffffu) {
        // the inlined length_error construction; the fakes of __throw return, so the code goes on afterwards
        IME::MslExceptionObject exc;
        exc.mVtbl = IME::gMslExcVtblB;
        char* text = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(kMessage) + 1)));
        exc.mRc.mPtr = text;
        exc.mRc.mCount = nullptr;
        if (text != nullptr) {
            int32_t* counter = static_cast<int32_t*>(IME::MslNew(4));
            if (counter != nullptr) *counter = 1;
            exc.mRc.mCount = counter;
        }
        IME::MslStrcpy(exc.mRc.mPtr, kMessage);
        exc.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kType, &exc, IME::gMslLengthErrorDtorTV);
    }
    uint32_t cap = mWord0;
    uint32_t size = mWord4;
    if (size < cap) {
        uint32_t index = size + mWord8;
        if (index >= cap) index -= cap;
        BufOf(this)[index] = element;
        mWord4 = mWord4 + 1;
        return;
    }
    uint32_t base = cap < gCdequeMinCap ? gCdequeMinCap : cap;
    uint32_t newCap = base;
    if (size + 1 > base) newCap = base << 1;
    uint32_t* block = static_cast<uint32_t*>(IME::MslNew(newCap << 2));
    block[mWord4] = element;
    if (mWord4 != 0) {
        uint32_t* dst = block;
        MslCdequeIter src = IterAt(this, 0);
        src.mPtr = BufOf(this) + mWord8;
        uint32_t total = mWord4;
        while (src.mIndex != total) {
            *dst = *src.mPtr;
            src.mIndex += 1;
            StepForward(src);
            dst += 1;
        }
    }
    if (mWordC != 0) IME::MslDelete(BufOf(this));
    mWordC = tpw::ArenaOffset(block);
    mWord4 = mWord4 + 1;
    mWord0 = newCap;
    mWord8 = 0;
}

// std::__cdeque<const void*, std::allocator<const void*>>::insert(iterator pos, unsigned long count, const void* const& value)
// 0x101f3b10 (2564 B). Unit v51030. Word0 capacity, word4 size, word8 start, wordC block (offset from the arena base) of 4-byte elements.

namespace {

using Deque = std::__cdeque<const void*, std::allocator<const void*>>;

uint32_t* Blk(const Deque* d) { return reinterpret_cast<uint32_t*>(tpw::ArenaBase() + d->mWordC); }

// The element pointer for logical position sum (start + index): wrapped once when it reaches the capacity.
uint32_t* At_2(const Deque* d, uint32_t sum) {
    if (sum < d->mWord0) return Blk(d) + sum;
    return Blk(d) + (sum - d->mWord0);
}

// Fills count elements from p (wrapping at the end of the block of d).
void Fill(const Deque* d, uint32_t* p, uint32_t count, const uint32_t* value) {
    for (; count != 0; count--) {
        *p = *value;
        p++;
        if (p == Blk(d) + d->mWord0) p = Blk(d);
    }
}

}  // namespace

namespace cdeque_callee {

void Insert(Deque* self, MslCdequeIter pos, uint32_t count, const void* const* valueArg) {
    const uint32_t* value = reinterpret_cast<const uint32_t*>(valueArg);
    if (count == 0) return;
    const uint32_t kMax = 0x3fffffffu;
    if (count > kMax || self->mWord4 > kMax - count) {
        IME::MslExceptionObject obj;
        obj.mVtbl = IME::gMslExcVtblC;
        obj.mVtbl = IME::gMslExcVtblB;
        const char* msg = "cdeque::insert length error";
        uint32_t len = static_cast<uint32_t>(IME::MslStrlen(msg));
        obj.mRc.mPtr = static_cast<char*>(IME::MslNewArray(len + 1));
        obj.mRc.mCount = nullptr;
        if (obj.mRc.mPtr != nullptr) {
            int32_t* counter = static_cast<int32_t*>(IME::MslNew(4));
            if (counter != nullptr) *counter = 1;
            obj.mRc.mCount = counter;
        }
        IME::MslStrcpy(obj.mRc.mPtr, msg);
        obj.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &obj, IME::gMslLengthErrorDtorTV);
    }
    uint32_t cap = self->mWord0;
    uint32_t size = self->mWord4;
    uint32_t n = size + count;
    uint32_t idx = pos.mIndex;
    if (n > cap) {
        const uint32_t* from = cap >= gCdequeMinCap ? &self->mWord0 : &gCdequeMinCap;
        uint32_t newCap = *from;
        while (n > newCap) newCap <<= 1;
        uint32_t* nb = static_cast<uint32_t*>(IME::MslNew(newCap << 2));
        uint32_t* dst = nb;
        if (idx != 0) {
            uint32_t* s = Blk(self) + self->mWord8;
            for (uint32_t counter = 0; counter != idx;) {
                *dst = *s;
                counter++;
                s++;
                if (s == Blk(self) + self->mWord0) s = Blk(self);
                dst++;
            }
        }
        uint32_t* f = nb + idx;
        for (uint32_t c = count; c != 0; c--) {
            *f = *value;
            f++;
        }
        if (idx < size) {
            dst = nb + idx + count;
            uint32_t counter = pos.mIndex;
            uint32_t* s = pos.mPtr;
            while (counter != size) {
                *dst = *s;
                counter++;
                s++;
                if (s == Blk(pos.mDeque) + pos.mDeque->mWord0) s = Blk(pos.mDeque);
                dst++;
            }
        }
        if (self->mWordC != 0) IME::MslDelete(Blk(self));
        self->mWordC = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(nb) - reinterpret_cast<uintptr_t>(tpw::ArenaBase()));
        self->mWord4 = n;
        self->mWord0 = newCap;
        self->mWord8 = 0;
        return;
    }
    uint32_t distance = size - idx;
    if (idx < distance) {
        uint32_t start = self->mWord8;
        if (start >= count)
            self->mWord8 = start - count;
        else
            self->mWord8 = self->mWord0 - (count - start);
        if (idx == 0) {
            Fill(self, Blk(self) + self->mWord8, count, value);
        } else {
            MslCdequeIter it0{self, 0, Blk(self) + self->mWord8};
            MslCdequeIter a, last, b, srcIt, c, fillIt;
            cdeque_callee::IterCopy(&last, cdeque_callee::IterAdd(cdeque_callee::IterCopy(&a, &it0), count + idx));
            cdeque_callee::IterCopy(&srcIt, cdeque_callee::IterAdd(cdeque_callee::IterCopy(&b, &it0), count));
            MslCdequeIter d = it0;
            while (srcIt.mIndex != last.mIndex) {
                *d.mPtr = *srcIt.mPtr;
                srcIt.mIndex++;
                srcIt.mPtr++;
                if (srcIt.mPtr == Blk(srcIt.mDeque) + srcIt.mDeque->mWord0) srcIt.mPtr = Blk(srcIt.mDeque);
                d.mIndex++;
                d.mPtr++;
                if (d.mPtr == Blk(d.mDeque) + d.mDeque->mWord0) d.mPtr = Blk(d.mDeque);
            }
            cdeque_callee::IterCopy(&fillIt, cdeque_callee::IterAdd(cdeque_callee::IterCopy(&c, &it0), idx));
            Fill(fillIt.mDeque, fillIt.mPtr, count, value);
        }
    } else if (distance == 0) {
        Fill(self, At_2(self, self->mWord8 + self->mWord4), count, value);
    } else {
        MslCdequeIter e1{self, self->mWord4, At_2(self, self->mWord8 + self->mWord4)};
        MslCdequeIter a, ne;
        cdeque_callee::IterCopy(&ne, cdeque_callee::IterAdd(cdeque_callee::IterCopy(&a, &e1), count));
        MslCdequeIter e2{self, self->mWord4, At_2(self, self->mWord8 + self->mWord4)};
        while (e2.mIndex != pos.mIndex) {
            ne.mIndex--;
            if (ne.mPtr == Blk(ne.mDeque)) ne.mPtr = Blk(ne.mDeque) + ne.mDeque->mWord0;
            ne.mPtr--;
            e2.mIndex--;
            if (e2.mPtr == Blk(e2.mDeque)) e2.mPtr = Blk(e2.mDeque) + e2.mDeque->mWord0;
            e2.mPtr--;
            *ne.mPtr = *e2.mPtr;
        }
        Fill(pos.mDeque, pos.mPtr, count, value);
    }
    self->mWord4 = self->mWord4 + count;
}

}  // namespace cdeque_callee

// std::__cdeque<const void*>::iterator::operator+=(long) at 0x101f2bec (408 B).

namespace cdeque_callee {

// Words of the deque: 0 capacity, 4 size, 8 start, C block. Wraps around the block.
// mWordC is the block's address on the Mac; here a 32-bit offset from the arena base (Ptr32 rule).
// The original recurses forever for n == INT32_MIN (negation stays negative); not reproduced (it would overflow the stack).
MslCdequeIter* IterAdd(MslCdequeIter* it, int32_t n) {
    auto* d = it->mDeque;
    uint32_t* block = reinterpret_cast<uint32_t*>(tpw::ArenaBase() + d->mWordC);
    if (n >= 0) {
        uint32_t sum = d->mWord8 + it->mIndex + static_cast<uint32_t>(n);
        if (sum < d->mWord0)
            it->mPtr = it->mPtr + n;
        else
            it->mPtr = block + static_cast<int32_t>((sum - d->mWord0) << 2) / 4;  // slwi 2 wraps in 32 bits
        it->mIndex += static_cast<uint32_t>(n);
        return it;
    }
    if (n == INT32_MIN) return it;
    uint32_t m = static_cast<uint32_t>(-n);
    if (m > d->mWord8 + it->mIndex)
        it->mPtr = block + static_cast<int32_t>(d->mWord0 - m);  // the 32-bit result may be negative
    else
        it->mPtr = it->mPtr - m;
    it->mIndex += static_cast<uint32_t>(n);
    return it;
}

}  // namespace cdeque_callee

// std::__destroy<Argument*> and std::__destroy<Argument> of the Metrowerks library, main app (unit v5310).

namespace std_destroy {

// 0x102017d4: std::__destroy<Argument>(Argument*): destroys the two strings of one Argument (wide string at 4, then string at 0).
void DestroyOne(Argument* argument) {
    if (argument != nullptr) {
        email_callee::WStringDtor(&argument->mWString, -1);   // 0x101468f8
        email_callee::StringDtor(&argument->mString, -1);     // 0x1000221c
    }
}

// 0x10201744: std::__destroy<Argument*>(first, last): DestroyOne for every element (12 bytes each)
void DestroyRange(Argument* first, Argument* last) {
    for (; first < last; first++) DestroyOne(first);
}

// 0x102013c0: std::__destroy<Argument*>(first, last, random_access_iterator_tag): the destructor for every element
void DestroyRangeTagged(Argument* first, Argument* last) {
    for (; first < last; first++) Argument::Destruct(first, -1);   // 0x1020127c
}

}  // namespace std_destroy

// std::__putnumber<char, ostreambuf_iterator<char>> 0x1020a140: writes the prefix, suffix and the fill padding by the adjust field.

namespace {

// ostreambuf_iterator::operator=(char): store at pptr or call overflow; a failure (-1) nulls the iterator.
inline void Put_3(MacPutStreamBuf*& sb, uint8_t c) {
    if (sb == nullptr) return;
    int32_t r;
    uint8_t* p = sb->mPptr.get();
    if (p < sb->mEpptr.get()) {
        sb->mPptr = p + 1;
        *p = c;
        r = *p;
    } else {
        r = MslPutOverflow(sb, c);
    }
    if (r == -1) sb = nullptr;
}

}  // namespace

MacPutStreamBuf* PutNumberChar(MacPutStreamBuf* iter, MacPutIos* ios, char fill, const char* prefix, int32_t prefixLen,
                               const char* suffix, int32_t suffixLen) {
    MacPutStreamBuf* sb = iter;
    int32_t pad = 0;
    int32_t total = prefixLen + suffixLen;
    if (ios->mWidth > total) pad = ios->mWidth - total;
    uint32_t adjust = ios->mFlags & 0xb0;
    const uint8_t f = static_cast<uint8_t>(fill);
    if (adjust != 0x20 && adjust != 0x10)
        for (int32_t i = 0; i < pad; i++) Put_3(sb, f);
    for (int32_t i = 0; i < prefixLen; i++) Put_3(sb, static_cast<uint8_t>(*prefix++));
    if (adjust == 0x10)
        for (int32_t i = 0; i < pad; i++) Put_3(sb, f);
    for (int32_t i = 0; i < suffixLen; i++) Put_3(sb, static_cast<uint8_t>(*suffix++));
    if (adjust == 0x20)
        for (int32_t i = 0; i < pad; i++) Put_3(sb, f);
    ios->mWidth = 0;
    return sb;
}

// std::__putnumber<wchar_t, ostreambuf_iterator<wchar_t>> 0x10226358, basic_ios<wchar_t>::widen 0x10226828 and
// std::deque<std::vector<Argument>>::const_iterator::operator++ 0x1022571c.

extern uint64_t gMslMacAddressHigh;

namespace {

// ostreambuf_iterator<wchar_t>::operator= : store at pptr while pptr < epptr, else overflow() through the vtable (0x38).
// A stored or returned 0xffff (eof as 16 bits) makes the iterator fail (NULL).
void Put_2(MacPutStreamBuf*& sb, uint32_t c) {
    if (sb == nullptr) return;
    uint32_t p = sb->mPptr.mOffset;
    uint16_t r;
    if (p < sb->mEpptr.mOffset) {
        sb->mPptr.mOffset = p + 2;
        uint16_t v = static_cast<uint16_t>(c);
        std::memcpy(sb->mPptr.get() - 2, &v, 2);
        r = v;
    } else {
        r = static_cast<uint16_t>(MslPutOverflow(sb, c));
    }
    if (r == 0xffff) sb = nullptr;
}

}  // namespace

namespace wostream_callee {
void* PutNumber(void* iter, void* iosPtr, char16_t fill, const char16_t* prefix, int32_t prefixLen, const char16_t* suffix,
                int32_t suffixLen) {
    MacPutStreamBuf* sb = static_cast<MacPutStreamBuf*>(iter);
    MacPutIos* ios = static_cast<MacPutIos*>(iosPtr);
    int32_t pad = 0;
    int32_t total = prefixLen + suffixLen;
    if (ios->mWidth > total) pad = ios->mWidth - total;
    uint32_t adjust = ios->mFlags & 0xb0;
    if (adjust != 0x20 && adjust != 0x10)
        for (int32_t i = 0; i < pad; i++) Put_2(sb, fill);
    for (int32_t i = 0; i < prefixLen; i++) {
        uint16_t c = *prefix++;   // read even when the iterator has failed
        Put_2(sb, c);
    }
    if (adjust == 0x10)
        for (int32_t i = 0; i < pad; i++) Put_2(sb, fill);
    for (int32_t i = 0; i < suffixLen; i++) {
        uint16_t c = *suffix++;
        Put_2(sb, c);
    }
    if (adjust == 0x20)
        for (int32_t i = 0; i < pad; i++) Put_2(sb, fill);
    ios->mWidth = 0;
    return sb;
}
}  // namespace wostream_callee

namespace wistream_callee {
char16_t Widen(void* ios, char c) {
    ignore_callee::IgnoreLocale locale;
    ignore_callee::IosGetloc(&locale, static_cast<ignore_callee::IgnoreIos*>(ios));
    void* facet = ignore_callee::UseFacetCtype(&locale);
    char16_t r = ignore_callee::CtypeWiden(facet, c);
    ignore_callee::LocaleDtor(&locale, -1);
    return r;
}
}  // namespace wistream_callee

namespace wostream_u {
char16_t Widen(void* ios, char c) { return wistream_callee::Widen(ios, c); }
}  // namespace wostream_u

namespace argdeque_callee {
void IterInc(MslArgumentDequeIter* it) {
    it->mIndex = it->mIndex + 1;
    uint32_t element = it->mElement;
    it->mElement = element + 0xc;
    uint32_t blockEnd = it->mBlockEnd;
    if (element + 0xc != blockEnd) return;
    uint32_t blockStart = *W_3(it->mCursor.mSlot);
    it->mCursor.mIndex = it->mCursor.mIndex + 1;
    int32_t diff = static_cast<int32_t>(blockEnd - blockStart);
    int32_t q = static_cast<int32_t>((static_cast<int64_t>(0x2aaaaaab) * diff) >> 32) >> 1;
    uint32_t slot = it->mCursor.mSlot;
    it->mCursor.mSlot = slot + 4;
    q = q + static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
    uint32_t* cdeque = W_3(it->mCursor.mCdeque);
    uint32_t count = cdeque[0];
    uint32_t table = cdeque[3];
    if (slot + 4 == table + count * 4) it->mCursor.mSlot = table;
    uint32_t block = *W_3(it->mCursor.mSlot);
    it->mElement = block;
    it->mBlockEnd = block + static_cast<uint32_t>(q) * 0xc;
}
}  // namespace argdeque_callee

// std::__tree<nocase_string, std::less<nocase_string>, std::allocator<nocase_string>> (a std::set<nocase_string>):
//   erase 0x1022419c, destroy 0x10223e64, insert_node_at 0x10228458.
// The Mac tree is count +0, root +4 (the end node's left word), begin +8; a node is left, right, parent|colour and the nocase_string
// (one word: a pointer to the shared representation) at +0xc. The host keeps native pointers (see NocaseSetTree in online.h).

namespace {

using node_2 = NocaseSetTree::node;

nocase_string* ValueOf(node_2* n) { return reinterpret_cast<nocase_string*>(&n->mValue); }

// The inlined nocase_string destructor: drops one reference (a count of 0 is read as 1); on the last one frees the data and the rep.
void ReleaseValue(node_2* n) {
    NocaseRep* rep = n->mValue;
    if (rep->mRefs == 0) {
        uint32_t refs = rep->mRefs;
        if (refs == 0) refs = 1;
        rep->mRefs = refs;
    }
    uint32_t left = rep->mRefs - 1;
    rep->mRefs = left;
    if (left != 0) rep = nullptr;
    n->mValue = rep;
    if (n->mValue != nullptr) {
        IME::MslDelete(n->mValue->mData);
        IME::MslDelete(n->mValue);
    }
}

// A grandchild, expanded in place by the compiler: its subtrees, then the allocator's destroy and deallocate.
void DestroyInner(NocaseSetTree* tree, node_2* x) {
    if (x->mLeft != nullptr) tree->destroy(x->mLeft);
    if (x->mRight != nullptr) tree->destroy(x->mRight);
    nocase_callee::AllocDestroy(tree, ValueOf(x));
    nocase_callee::Deallocate(reinterpret_cast<char*>(tree) + 4, x, 1);
}

// A child, expanded in place: its two children as above, then the string destructor and operator delete.
void DestroyChild(NocaseSetTree* tree, node_2* x) {
    if (x->mLeft != nullptr) DestroyInner(tree, x->mLeft);
    if (x->mRight != nullptr) DestroyInner(tree, x->mRight);
    nocase_callee::StringDtor(ValueOf(x), -1);
    IME::MslDelete(x);
}

}  // namespace

void NocaseSetTree::erase(node_2* position) {
    if (position == mBegin) {
        node_2* right = position->mRight;
        node_2* result;
        if (right == nullptr) {
            node_2* current = position;
            node_2* parent;
            bool again;
            do {
                parent = reinterpret_cast<node_2*>(current->mParent & ~static_cast<uintptr_t>(1));
                again = current != parent->mLeft;
                current = parent;
            } while (again);
            result = parent;
        } else {
            result = right;
            while (result->mLeft != nullptr) result = result->mLeft;
        }
        mBegin = result;
    }
    MslRedBlackTree::balance_erase(reinterpret_cast<MslRedBlackTree::node_base*>(position),
                                   reinterpret_cast<MslRedBlackTree::node_base*>(mRoot));
    ReleaseValue(position);
    IME::MslDelete(position);
    mCount = mCount - 1;
}

void NocaseSetTree::destroy(node_2* subtree) {
    if (subtree->mLeft != nullptr) DestroyChild(this, subtree->mLeft);
    if (subtree->mRight != nullptr) DestroyChild(this, subtree->mRight);
    ReleaseValue(subtree);
    IME::MslDelete(subtree);
}

NocaseSetTree::node* NocaseSetTree::insert_node_at(node_2* parent, bool left, bool leftmost, const nocase_string& key) {
    static const char kMessage[] = "tree::insert length error";
    static const char kTypes[] = "!std::exception!!std::logic_error!!std::length_error!!";
    if (mCount > 0xfffffffeu) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kMessage);
        object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            object.mRc.mCount = static_cast<int32_t*>(IME::MslNew(4));
            if (object.mRc.mCount != nullptr) *object.mRc.mCount = 1;
        }
        IME::MslStrcpy(object.mRc.mPtr, kMessage);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kTypes, &object, IME::gMslLengthErrorDtorTV);
    }
    node_2* made = static_cast<node_2*>(IME::MslNew(sizeof(node_2)));   // 0x10 bytes on the Mac
    // The original tests the placement-new result (the address of the value, never NULL) before constructing.
    nocase_callee::StringCopy(ValueOf(made), &key);
    made->mRight = nullptr;
    made->mLeft = nullptr;
    made->mParent = reinterpret_cast<uintptr_t>(parent) | (made->mParent & 1);
    if (left)
        parent->mLeft = made;
    else
        parent->mRight = made;
    mCount = mCount + 1;
    nocase_callee::BalanceInsert(made, mRoot);
    if (leftmost) mBegin = made;
    return made;
}

// 0x1021e664: std::__tree<wstring, less<wstring>, allocator<wstring>>::erase_one<wstring>(const wstring&)

static const MslWideString* ValueOf_2(WStringTree::node_base* node) {
    return reinterpret_cast<const MslWideString*>(reinterpret_cast<const uint8_t*>(node) + sizeof(WStringTree::node_base));
}

bool WStringTree::erase_one(const MslWideString& key) {
    node_base* found = end();
    node_base* current = mRoot;
    while (current != nullptr) {
        if (!WideStringLess(mLess, ValueOf_2(current), &key)) {
            found = current;
            current = current->mLeft;
        } else {
            current = current->mRight;
        }
    }
    if (found == end() || WideStringLess(mLess, &key, ValueOf_2(found))) found = end();
    if (found != end()) erase(found);
    return found != end();
}

// std::__tree<std::wstring, std::less<std::wstring>, std::allocator<std::wstring>> (a std::set<std::wstring>):
// insert_node_at 0x10227c78, erase 0x10228104, destroy 0x10223204.
// Mac words are 32 bits, the host's native. The tree is count +0, root +4 (the end node's left word), begin +8 (the end node's right
// word; the mLess bytes of WStringTree on the host). A node is left +0, right +4, parent|colour +8, the string (one pointer to its
// representation) +0x0c.

namespace {

const char kMessage_2[] = "tree::insert length error";
const char kType_2[] = "!std::exception!!std::logic_error!!std::length_error!!";

// The inlined ~basic_string of a node's string (0x10228194.. in erase, 0x102233b0.. in destroy): the count 0 is taken as 1, the
// representation is freed when it was the last reference, otherwise the node's word is cleared. The node is not freed here.
void ReleaseString_2(WStringTreeNode* node) {
    IME::MslWideStringRep* rep = node->mString;
    if (rep->mRefs == 0) rep->mRefs = 1;
    uint32_t refs = rep->mRefs - 1;
    rep->mRefs = refs;
    if (refs != 0) rep = nullptr;
    node->mString = rep;
    if (rep != nullptr) {
        IME::MslDelete(rep->mData);
        IME::MslDelete(rep);
    }
}

}  // namespace

// Stand-in for the allocator's deallocate until it is translated (the test library fakes it).
__attribute__((weak)) void wstring_tree_callee::NodeDeallocate(void* allocator, WStringTreeNode* node, unsigned long count) {
    (void)allocator;
    (void)count;
    IME::MslDelete(node);
}

WStringTreeNode* WStringTree::insert_node_at(node_base* parent, bool left, bool leftmost, const IME::MslWideString* key) {
    if (mCount > 0xfffffffeu) {
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblC;   // dead store in the original too
        object.mVtbl = IME::gMslExcVtblB;
        size_t length = IME::MslStrlen(kMessage_2);
        object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length) + 1));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
            if (count != nullptr) *count = 1;
            object.mRc.mCount = count;
        }
        IME::MslStrcpy(object.mRc.mPtr, kMessage_2);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kType_2, &object, IME::gMslLengthErrorDtorTV);   // the fakes return, and the original runs on
    }
    // The Mac asks for 0x10 bytes; the host node is larger.
    WStringTreeNode* node = static_cast<WStringTreeNode*>(IME::MslNew(sizeof(WStringTreeNode)));
    // placement new (0x10201850) of the string at node + 0xc, then basic_string(key, 0, npos)
    IME::MslWideString_construct(reinterpret_cast<IME::MslWideString*>(&node->mString), key, 0, 0xffffffffu);
    node->mRight = nullptr;
    node->mLeft = nullptr;
    node->mParent = reinterpret_cast<uintptr_t>(parent) | (node->mParent & 1);   // the colour bit of the fresh memory survives
    if (left)
        parent->mLeft = node;
    else
        parent->mRight = node;
    mCount = mCount + 1;
    MslRedBlackTree::balance_insert(node, mRoot);
    if (leftmost) std::memcpy(mLess, &node, sizeof(node));
    return node;
}

void WStringTree::erase(node_base* position) {
    node_base* begin;
    std::memcpy(&begin, mLess, sizeof(begin));
    if (position == begin) {
        // begin moves to the in-order successor
        node_base* next = position->mRight;
        if (next != nullptr) {
            while (next->mLeft != nullptr) next = next->mLeft;
        } else {
            node_base* current = position;
            for (;;) {
                node_base* up = reinterpret_cast<node_base*>(current->mParent & ~static_cast<uintptr_t>(1));
                if (current != up->mLeft) {
                    current = up;
                    continue;
                }
                next = up;
                break;
            }
        }
        std::memcpy(mLess, &next, sizeof(next));
    }
    MslRedBlackTree::balance_erase(position, mRoot);
    // (the original tests "&node->string != 0" here, always true)
    ReleaseString_2(static_cast<WStringTreeNode*>(position));
    IME::MslDelete(position);
    mCount = mCount - 1;
}

void WStringTree::destroy(node_base* nodeBase) {
    WStringTreeNode* node = static_cast<WStringTreeNode*>(nodeBase);
    // a grandchild: its subtrees, its string through the allocator, then the block through the allocator
    auto grandchild = [this](node_base* gb) {
        WStringTreeNode* g = static_cast<WStringTreeNode*>(gb);
        if (g->mLeft != nullptr) destroy(g->mLeft);
        if (g->mRight != nullptr) destroy(g->mRight);
        wstring_alloc::Destroy(this, &g->mString);
        wstring_tree_callee::NodeDeallocate(reinterpret_cast<char*>(this) + 4, g, 1);
    };
    // a child: both grandchildren, then its string destructor and operator delete
    auto child = [&](node_base* cb) {
        WStringTreeNode* c = static_cast<WStringTreeNode*>(cb);
        if (c->mLeft != nullptr) grandchild(c->mLeft);
        if (c->mRight != nullptr) grandchild(c->mRight);
        masterlist_callee::WStringDtor(&c->mString, -1);
        IME::MslDelete(c);
    };
    if (node->mLeft != nullptr) child(node->mLeft);
    if (node->mRight != nullptr) child(node->mRight);
    ReleaseString_2(node);
    IME::MslDelete(node);
}

// Unit v51126: the tree constructors and insert_one of the nocase set, the wstring set and the buddy map, the allocator deallocates,
// the Argument destroyers, the nocase wide string destructor, the tear_down of the deque of Argument vectors, two make_iterator /
// getloc pieces, and four forwarders of the UI. The structures keep the Mac's 32-bit words (the arena is 32-bit addressable).

namespace v51126 {

uint32_t gNocaseTreeCount = 0;          // 0x102b1f9c
uint8_t gNocaseTreeInserted = 1;        // 0x102b1fa0
uint8_t gNocaseTreeNotInserted = 0;     // 0x102b1fa1
uint32_t gWStringTreeCount = 0;         // 0x102b1fa4
uint8_t gWStringTreeInserted = 1;       // 0x102b1fa8
uint8_t gWStringTreeNotInserted = 0;    // 0x102b1fa9
uint32_t gBuddyTreeCount = 0;           // 0x102b1fac

namespace {

using LessFn = bool (*)(const void*, const void*);
using InsertAtFn = uint32_t (*)(void*, uint32_t, bool, bool, const void*);

// insert_one of both set trees: walk down comparing, then insert unless the last node not above the key equals it.
void InsertOne(TreeInsertResult* result, TreeHeader3* tree, const void* key, LessFn less, InsertAtFn insertAt, const uint8_t* inserted,
               const uint8_t* notInserted) {
    uint32_t parent = 0;
    uint32_t current = tree->mRoot;
    uint32_t where = A(&tree->mRoot);
    bool left = true;
    bool leftmost = true;
    while (current != 0) {
        where = current;
        if (less(key, P(current + 0xc))) {
            current = W(current)[0];
            left = true;
        } else {
            parent = current;
            current = W(current)[1];
            left = false;
            leftmost = false;
        }
    }
    if (parent == 0 || less(P(parent + 0xc), key)) {
        uint32_t node = insertAt(tree, where, left, leftmost, key);
        result->mNode = node;
        result->mInserted = *inserted;
    } else {
        result->mNode = parent;
        result->mInserted = *notInserted;
    }
}

}  // namespace

void NocaseTreeCtor(TreeHeader3* self, const uint8_t* compare) {
    self->mCount = gNocaseTreeCount;
    self->mRoot = 0;
    (void)*compare;   // the byte is stored at 0x08 and the word store below overwrites it
    self->mBegin = 0;
    self->mBegin = A(&self->mRoot);
}

void WStringTreeCtor(TreeHeader3* self, const uint8_t* compare) {
    self->mCount = gWStringTreeCount;
    self->mRoot = 0;
    (void)*compare;
    self->mBegin = 0;
    self->mBegin = A(&self->mRoot);
}

void BuddyTreeCtor(TreeHeader4* self, const uint8_t* compare) {
    self->mCount = gBuddyTreeCount;
    self->mRoot = 0;
    self->mByte8[0] = *compare;
    self->mBegin = 0;
    self->mBegin = A(&self->mRoot);
}

void NocaseTreeInsertOne(TreeInsertResult* result, TreeHeader3* self, const void* key) {
    InsertOne(result, self, key, v51126_callee::NocaseLess, v51126_callee::NocaseInsertNodeAt, &gNocaseTreeInserted, &gNocaseTreeNotInserted);
}

void WStringTreeInsertOne(TreeInsertResult* result, TreeHeader3* self, const void* key) {
    InsertOne(result, self, key, v51126_callee::WStringLess, v51126_callee::WStringInsertNodeAt, &gWStringTreeInserted,
              &gWStringTreeNotInserted);
}

void BuddyNodeDeallocate(void* allocator, void* node, uint32_t count) {
    (void)allocator;
    (void)count;
    v51126_callee::OperatorDelete(node);
}

void WStringNodeDeallocate(void* allocator, void* node, uint32_t count) {
    (void)allocator;
    (void)count;
    v51126_callee::OperatorDelete(node);
}

void* NocaseWStringDestruct(uint32_t* self, int16_t deleteFlag) {
    if (self != nullptr) {
        uint32_t rep = *self;
        uint32_t* r = W(rep);
        if (r[2] == 0) r[2] = 1;
        uint32_t refs = r[2] - 1;
        r[2] = refs;
        if (refs != 0) rep = 0;
        *self = rep;
        uint32_t now = *self;
        if (now != 0) {
            v51126_callee::OperatorDelete(P(W(now)[3]));
            v51126_callee::OperatorDelete(P(*self));
        }
        if (deleteFlag > 0) v51126_callee::OperatorDelete(self);
    }
    return self;
}

void ArgumentDestroy(Argument* argument) {
    if (argument != nullptr) {
        v51126_callee::WStringDtor(&argument->mWString, -1);   // 0x101468f8
        v51126_callee::StringDtor(&argument->mString, -1);     // 0x1000221c
    }
}

}  // namespace v51126

// std::basic_filebuf<char>: constructor 0x10072bbc, open 0x10071c44, uflow 0x1007165c (unit v5046).

// The vtables the constructor stores (TOC slots -0x7020 and -0x7018); the program's own objects are not rebuilt here.
uint32_t gFilebufCharVtblA[4];   // 0x1029d0f8
uint32_t gFilebufCharVtblB[4];   // 0x1029d148

namespace {

// A 32-bit pointer field holds an offset from the arena base on the host (0: null); the vtable words are the low half of the
// static's address and the FILE* is an opaque 32-bit token.
uint32_t W_4(const void* p) { return p != nullptr ? static_cast<uint32_t>(tpw::ArenaOffset(p)) : 0u; }
uint8_t* P_2(uint32_t a) { return a != 0 ? reinterpret_cast<uint8_t*>((uintptr_t)tpw::gArenaBase + a) : nullptr; }
uint32_t Wraw(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
void* File(uint32_t token) { return reinterpret_cast<void*>(static_cast<uintptr_t>(token)); }

// The fopen mode string of an openmode with the ate bit (2) cleared; the jump table at 0x1029d1c0 covers 8..0x3c and sends the
// gaps to the default (return 0).
const char* ModeString(uint32_t m) {
    switch (m) {
        case 0x10: case 0x30: return "w";
        case 0x11: return "a";
        case 0x08: return "r";
        case 0x18: return "r+";
        case 0x38: return "w+";
        case 0x14: case 0x34: return "wb";
        case 0x15: return "ab";
        case 0x0c: return "rb";
        case 0x1c: return "r+b";
        case 0x3c: return "w+b";
        default: return nullptr;
    }
}

}  // namespace

using namespace filebuf_char_callee;

BasicFilebufChar* BasicFilebufChar::construct(void* file) {
    mVtable = Wraw(gFilebufCharVtblA);
    mEback = 0;
    mGptr = 0;
    mEgptr = 0;
    mZero10[0] = 0;
    mZero10[1] = 0;
    mZero10[2] = 0;
    LocaleCtor(&mLocale);
    mVtable = Wraw(gFilebufCharVtblB);
    mFile = Wraw(file);
    mState = 0;
    mFlag3e = 0;
    mEback = W_4(this) + 0x30;
    mGptr = W_4(this) + 0x30;
    mEgptr = W_4(this) + 0x30;
    uint32_t loc[2];
    Getloc(loc, this);
    void* facet = UseFacetCodecvt(loc);
    mCodecvt = W_4(facet);
    LocaleDtor(loc, -1);
    mUnbuffered = static_cast<uint8_t>(CodecvtAlwaysNoconv(P_2(mCodecvt)));
    return this;
}

BasicFilebufChar* BasicFilebufChar::open(const char* name, int32_t mode) {
    if (mFile != 0) return nullptr;
    const uint32_t m = static_cast<uint32_t>(mode) & 0xfffffffdu;
    if (m - 8 > 0x34) return nullptr;
    const char* str = ModeString(m);
    if (str == nullptr) return nullptr;
    mFile = Fopen(name, str);
    if (mFile == 0) return nullptr;
    if ((static_cast<uint32_t>(mode) & 2) != 0 && Fseek(mFile, 0, 2) != 0) {
        DeleteArray(P_2(mEback));
        if (mFile != 0) {
            if (mFlag3e != 0) {
                if ((Unshift(this) & 0xff) == 0) return nullptr;
                mFlag3e = 0;
            }
            mEback = W_4(this) + 0x30;
            mGptr = W_4(this) + 0x30;
            mEgptr = W_4(this) + 0x30;
            if (Fclose(mFile) == 0) mFile = 0;
        }
        return nullptr;
    }
    return this;
}

int32_t BasicFilebufChar::uflow() {
    mFlag3e = 0;
    if (mFile == 0) return -1;
    if (mGptr < mEgptr) {
        mGptr = mGptr + 1;
        return *P_2(mGptr - 1);
    }

    // Puts the byte in the pushback area (the area moves down by 4 when it holds 14 bytes).
    auto pushback = [this](uint8_t byte) {
        uint32_t n = mEgptr - mEback;
        if (n == 0xe) {
            Memmove(P_2(mEback), P_2(mEback + 4), 10);
            n = static_cast<uint32_t>((static_cast<uint64_t>(0xAAAAAAABu) * n) >> 32) >> 1;
        }
        mEback = W_4(this) + 0x30;
        mGptr = W_4(this) + n + 0x31;
        mEgptr = W_4(this) + n + 0x31;
        *P_2(mGptr - 1) = byte;
    };

    if (mUnbuffered != 0) {
        const int32_t c = filebuf_callee::Fgetc(File(mFile));
        if (c == -1) return -1;
        pushback(static_cast<uint8_t>(static_cast<int8_t>(c)));
        return static_cast<uint8_t>(static_cast<int8_t>(c));
    }

    void* cvt = P_2(mCodecvt);
    const int32_t maxlen = CodecvtMaxLength(cvt);
    if (maxlen == 1) {
        const int32_t c = filebuf_callee::Fgetc(File(mFile));
        if (c == -1) return -1;
        char from = static_cast<char>(c);
        char to;
        const char* fromNext;
        char* toNext;
        const int32_t r = CodecvtInChar(cvt, &mState, &from, &from + 1, &fromNext, &to, &to + 1, &toNext);
        if (r == 3) {
            to = from;
        } else if (r < 3 && r > 0) {
            return -1;
        }
        pushback(static_cast<uint8_t>(to));
        return static_cast<uint8_t>(to);
    }

    // Several bytes per character: gather them in a string. The loop below has no exit but the returns (the original's
    // code after it is unreachable): end of file or a codecvt answer of 2 leaves with -1.
    MslStrRep* str;
    uint8_t allocator[4];
    StringCtor(&str, allocator);
    for (int32_t n = maxlen; n > 1; --n) {
        const int32_t c = filebuf_callee::Fgetc(File(mFile));
        if (c == -1) {
            StringDtor(&str, -1);
            return -1;
        }
        filebuf_callee::StringAppend(&str, 1, static_cast<char>(c));
    }
    for (;;) {
        const int32_t c = filebuf_callee::Fgetc(File(mFile));
        if (c == -1) {
            StringDtor(&str, -1);
            return -1;
        }
        filebuf_callee::StringAppend(&str, 1, static_cast<char>(c));
        const char* from = reinterpret_cast<const char*>(P_2(str->data));
        char to;
        const char* fromNext;
        char* toNext;
        const int32_t r = CodecvtInChar(cvt, &mState, from, from + str->len, &fromNext, &to, &to + 1, &toNext);
        if (r == 3) {
            to = *filebuf_callee::StringIndex(&str, 0);
        } else if (r < 3 && r >= 2) {
            StringDtor(&str, -1);
            return -1;
        }
    }
}

// Unit v5944: std::basic_filebuf<wchar_t>::basic_filebuf(FILE*) 0x10226ef4, std::basic_filebuf<wchar_t>::open 0x10221c30 and
// std::__tree<std::pair<const nocase_string, BuddyInfo>, ...>::insert_node_at 0x102273bc.
// The object words are the Mac's 32-bit fields; the arena is 32-bit addressable so pointers are stored as words.

using IME::gMslExcVtblB;
using IME::gMslLengthErrorDtorTV;
using IME::gMslLengthErrorVtbl;

namespace {

// 0x10226ef4
inline uint32_t Word_3(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
inline void* Ptr_2(uint32_t w) { return reinterpret_cast<void*>(static_cast<uintptr_t>(w)); }

// The mode strings in the order of the original's targets (TOC 0x10250e7f + 0x63c ...).
const char* const kModes[10] = {"w", "a", "r", "r+", "w+", "wb", "ab", "rb", "r+b", "w+b"};
// The jump table at 0x102b1fb4, indexed by (mode & ~2) - 8; -1 = the default (return NULL).
const int8_t kModeIndex[0x35] = {
    2, -1, -1, -1, 7, -1, -1, -1, 0, 1, -1, -1, 5, 6, -1, -1, 3, -1, -1, -1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1, -1, 5, -1, -1, -1, 4, -1, -1, -1, 9,
};

// The exception object the original builds on its stack: vtable, then a reference counted string (data, count).
struct LenExc {
    const void* vtbl;
    char* data;
    int32_t* refs;
};

}  // namespace

// 0x10226ef4 (272): basic_filebuf(FILE*)
BasicFilebufWchar* FilebufWcharConstruct(BasicFilebufWchar* self, uint32_t file) {
    self->mVtable = Word_3(gIfsWVtblC);
    self->mEback = 0;
    self->mGptr = 0;
    self->mEgptr = 0;
    for (int i = 0; i < 12; ++i) self->pad10[i] = 0;   // 0x10 .. 0x1b
    filebuf_char_callee::LocaleCtor(self->pad10 + 12);   // 0x1c
    self->mVtable = Word_3(gIfsWVtblD);
    self->mFile = file;
    for (int i = 0; i < 4; ++i) self->mState[i] = 0;
    self->mFlag4c = 0;
    self->mEback = Word_3(self->mBuf);
    self->mGptr = Word_3(self->mBuf);
    self->mEgptr = Word_3(self->mBuf);
    uint32_t loc[2];
    filebuf_wclose_callee::Getloc(loc, self);
    void* cvt = filebuf_wclose_callee::UseFacetCodecvtW(loc);   // the second argument is an uninitialised slot in the original
    self->mCodecvt = Word_3(cvt);
    filebuf_char_callee::LocaleDtor(loc, -1);
    self->mUnbuffered = static_cast<uint8_t>(filebuf_char_callee::CodecvtAlwaysNoconv(Ptr_2(self->mCodecvt)));
    return self;
}

// 0x10221c30 (396): open(name, mode); mode bit 2 = ate
BasicFilebufWchar* FilebufWcharOpen(BasicFilebufWchar* self, const char* name, uint32_t mode) {
    if (self->mFile != 0) return nullptr;
    uint32_t idx = (mode & ~2u) - 8;
    if (idx > 0x34) return nullptr;
    int k = kModeIndex[idx];
    if (k < 0) return nullptr;
    self->mFile = filebuf_char_callee::Fopen(name, kModes[k]);
    if (self->mFile == 0) return nullptr;
    if (mode & 2) {
        if (filebuf_char_callee::Fseek(self->mFile, 0, 2) != 0) {
            filebuf_char_callee::DeleteArray(Ptr_2(self->mEback));
            if (self->mFile != 0) {
                bool go = true;
                if (self->mFlag4c != 0) {
                    if (filebuf_wseek_callee::Unshift(self))
                        self->mFlag4c = 0;
                    else
                        go = false;
                }
                if (go) {
                    self->mEback = Word_3(self->mBuf);
                    self->mGptr = Word_3(self->mBuf);
                    self->mEgptr = Word_3(self->mBuf);
                    if (filebuf_char_callee::Fclose(self->mFile) == 0) self->mFile = 0;
                }
            }
            return nullptr;
        }
    }
    return self;
}

// 0x102273bc (536): insert_node_at(parent, left, setLeftmost, value)
BuddyTreeNode* BuddyTreeInsertNodeAt(BuddyTree* self, BuddyTreeNode* parent, bool left, bool setLeftmost, const void* pair) {
    if (self->mCount > 0xfffffffeu) {
        const char* msg = "tree::insert length error";
        LenExc exc;
        exc.vtbl = gMslExcVtblB;
        exc.data = static_cast<char*>(buddy_tree_callee::NewArray(buddy_tree_callee::Strlen(msg) + 1));
        exc.refs = nullptr;
        if (exc.data != nullptr) {
            int32_t* refs = static_cast<int32_t*>(buddy_tree_callee::New(4));
            if (refs != nullptr) *refs = 1;
            exc.refs = refs;
        }
        buddy_tree_callee::Strcpy(exc.data, msg);
        exc.vtbl = gMslLengthErrorVtbl;
        buddy_tree_callee::Throw("!std::exception!!std::logic_error!!std::length_error!!", &exc, gMslLengthErrorDtorTV);
        // the real __throw does not return; the fake does and the original falls through
    }
    BuddyTreeNode* node = static_cast<BuddyTreeNode*>(buddy_tree_callee::New(0x14));
    buddy_tree_callee::PairCtor(node->mPair, pair);
    node->mRight = nullptr;
    node->mLeft = nullptr;
    node->mParentColor = reinterpret_cast<uintptr_t>(parent) | (node->mParentColor & 1u);
    if (left)
        parent->mLeft = node;
    else
        parent->mRight = node;
    self->mCount = self->mCount + 1;
    buddy_tree_callee::BalanceInsert(node, self->mRoot);
    if (setLeftmost) self->mLeftmost = node;
    return node;
}

// std::basic_filebuf<wchar_t>::close, underflow, pbackfail, imbue (unit v5137).

using namespace filebuf_wclose_callee;

// The Ptr32 fields hold the low half of a host address (arena memory is above 4 GB): the high half is taken from the object.
static inline void* Hi(const void* self, uint32_t w) {
    return w ? reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(self) & ~static_cast<uintptr_t>(0xffffffffu)) | w) : nullptr;
}

// 0x10221b40
BasicFilebufWchar* BasicFilebufWchar::close() {
    if (mFile == 0) return nullptr;
    if (mFlag4c != 0) {
        if (!filebuf_wseek_callee::Unshift(this)) return nullptr;
        mFlag4c = 0;
    }
    uint32_t area = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)) + 0x30;
    mEback = area;
    mGptr = area;
    mEgptr = area;
    if (filebuf_char_callee::Fclose(mFile) != 0) return nullptr;
    mFile = 0;
    return this;
}

// 0x10221a64
uint32_t BasicFilebufWchar::underflow() {
    mFlag4c = 0;
    if (mFile == 0) return 0xffff;
    if (mGptr < mEgptr) return *static_cast<const char16_t*>(Hi(this, mGptr));
    uint32_t r = static_cast<uint32_t>(VirtualUnderflow(this));
    if ((r & 0xffff) != 0xffff) mGptr -= 2;
    return r;
}

// 0x10221360. Returns 0 for eof, otherwise the character.
char16_t BasicFilebufWchar::pbackfail(char16_t c) {
    if (mFile == 0 || !(mEback < mGptr)) return 0xffff;
    uint32_t g = mGptr - 2;
    mGptr = g;
    if (c != 0xffff) *static_cast<char16_t*>(Hi(this, g)) = c;
    return c == 0xffff ? 0 : c;
}

// 0x10220964
void BasicFilebufWchar::imbue(const void* loc) {
    streambuf_callee::LocaleAssign(reinterpret_cast<uint8_t*>(this) + 0x1c, loc);
    struct {
        uint32_t impl;
        uint32_t rc;
    } l;
    filebuf_wclose_callee::Getloc(&l, this);
    mCodecvt = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(UseFacetCodecvtW(&l)));
    if (l.rc != 0) {
        int32_t* p = static_cast<int32_t*>(Hi(this, l.rc));
        int32_t n = *p - 1;
        *p = n;
        if (n == 0) {
            LocaleImpDtor(Hi(this, l.impl), 1);
            filebuf_wseek_callee::Delete(p);
        }
    }
    mUnbuffered = static_cast<uint8_t>(filebuf_char_callee::CodecvtAlwaysNoconv(Hi(this, mCodecvt)));
}

// std::basic_filebuf<wchar_t>::seekoff, seekpos, overflow (unit v5093).

using namespace filebuf_wseek_callee;

namespace {

void* CodecvtOf(uint32_t p) { return reinterpret_cast<void*>(static_cast<uintptr_t>(p)); }

// The original's inlined release of the reference-counted string (refs 0 is first made 1).
void ReleaseStr(MslStrRep*& s) {
    if (s->refs == 0) s->refs = 1;
    int32_t t = s->refs;
    s->refs = t - 1;
    if (t - 1 != 0) s = nullptr;
    if (s != nullptr) {
        Delete(reinterpret_cast<void*>(static_cast<uintptr_t>(s->data)));
        Delete(s);
    }
}

}  // namespace

// 0x10220cd4
FposInt BasicFilebufWchar::seekoff(int32_t off, int32_t dir, uint32_t mode) {
    const FposInt fail = {-1, 0};
    int32_t n = CodecvtEncoding(CodecvtOf(mCodecvt));
    if (mFile == 0 || (off != 0 && n <= 0)) return fail;
    if (!(dir == 2 && off == 0)) {
        if (mFlag4c != 0 && n < 0) {
            if (!Unshift(this)) return fail;
        }
    }
    int32_t whence;
    if (dir == 3) return fail;
    if (dir < 3) {
        if (dir == 1) {
            whence = 0;
        } else if (dir > 1) {  // 2
            whence = 1;
            if (mode & 8) {
                int32_t diff = static_cast<int32_t>(mEgptr - mGptr);
                off = static_cast<int32_t>(static_cast<uint32_t>(off) - static_cast<uint32_t>(diff / 2));
            }
        } else {
            return fail;
        }
    } else {
        if (dir >= 5) return fail;
        whence = 2;  // 4
    }
    if (n < 0) n = 0;
    if (filebuf_wseek_callee::Fseek(mFile, static_cast<int32_t>(static_cast<uint32_t>(n) * static_cast<uint32_t>(off)), whence) != 0) return fail;
    if (mode & 8) {
        uint32_t self30 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)) + 0x30;
        mEback = self30;
        mGptr = self30;
        mEgptr = self30;
    }
    mFlag4c = 0;
    FposInt r;
    r.off = Ftell(mFile);
    r.state = 0;
    return r;
}

// 0x10220b30
FposInt BasicFilebufWchar::seekpos(int32_t off, int32_t state, uint32_t mode) {
    const FposInt fail = {-1, 0};
    if (mFile == 0 || (mode & 0x18) == 0) return fail;
    int32_t n = CodecvtEncoding(CodecvtOf(mCodecvt));
    if ((mode & 0x10) && mFlag4c != 0 && n < 0) {
        if (!Unshift(this)) return fail;
    }
    if (filebuf_wseek_callee::Fseek(mFile, off, 0) != 0) return fail;
    if (mode & 8) {
        uint32_t self30 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)) + 0x30;
        mEback = self30;
        mGptr = self30;
        mEgptr = self30;
    }
    mFlag4c = 0;
    FposInt r;
    r.off = off;
    r.state = state;
    return r;
}

// 0x10220f18. The listing's loop never leaves except by the error result (r28 stays 1); the code after it
// (the fputc loop at 0x102211cc) is unreachable and is not translated.
char16_t BasicFilebufWchar::overflow(char16_t c) {
    mFlag4c = 1;
    if (mFile == 0) return 0xffff;
    if (c == 0xffff) return 0;
    if (mUnbuffered != 0) {
        if (Fputc(c, mFile) == -1) return 0xffff;
        return c;
    }
    void* cvt = CodecvtOf(mCodecvt);
    int32_t n = CodecvtEncoding(cvt);
    if (n == 1) {
        char16_t ch = c;
        const char16_t* fromNext;
        char b = 0;
        char* toNext;
        int32_t r = CodecvtOut(cvt, mState, &ch, &ch + 1, &fromNext, &b, &b + 1, &toNext);
        if (r == 3) {
            b = static_cast<char>(ch);
        } else if (r >= 1 && r < 3) {
            return 0xffff;
        }
        if (Fputc(static_cast<int32_t>(b), mFile) == -1) return 0xffff;
        return c;
    }
    MslStrRep* str = nullptr;
    char alloc;
    StringInit(&str, 3, 0, &alloc);
    char16_t ch = c;
    for (;;) {
        char* to = reinterpret_cast<char*>(static_cast<uintptr_t>(str->data));
        int32_t len = str->len;
        const char16_t* fromNext;
        char* toNext;
        int32_t r = CodecvtOut(cvt, mState, &ch, &ch + 1, &fromNext, to, to + len, &toNext);
        if (r == 2) {
            ReleaseStr(str);
            return 0xffff;
        }
        if (r == 0) {
            StringResize(&str, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(toNext) - reinterpret_cast<uintptr_t>(to)), 0);
        } else if (r == 1) {
            StringResize(&str, static_cast<uint32_t>(str->len) * 2 + 1, 0);
        } else if (r == 3) {
            StringAssign(&str, 1, static_cast<char>(ch));
        }
    }
}

// std::basic_filebuf<char>::seekpos 0x10070eb4, seekoff 0x10071058, overflow 0x10071294 (unit v5233).

namespace {
namespace fc = filebuf_char_callee;
namespace fw = filebuf_wseek_callee;

inline void* Cvt(uint32_t p) { return reinterpret_cast<void*>(static_cast<uintptr_t>(p)); }
}  // namespace

FposInt BasicFilebufChar::seekpos(int32_t off, int32_t state, uint32_t mode) {
    const FposInt fail = {-1, 0};
    if (mFile == 0 || (mode & 0x18) == 0) return fail;
    int32_t enc = fw::CodecvtEncoding(Cvt(mCodecvt));
    if ((mode & 0x10) != 0 && mFlag3e != 0 && enc < 0) {
        if ((fc::Unshift(this) & 0xff) == 0) return fail;
    }
    if (fc::Fseek(mFile, off, 0) != 0) return fail;
    if ((mode & 8) != 0) {
        uint32_t p = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)) + 0x30;
        mEback = p;
        mGptr = p;
        mEgptr = p;
    }
    mFlag3e = 0;
    return FposInt{off, state};
}

FposInt BasicFilebufChar::seekoff(int32_t off, int32_t dir, uint32_t mode) {
    const FposInt fail = {-1, 0};
    int32_t enc = fw::CodecvtEncoding(Cvt(mCodecvt));
    if (mFile == 0 || (off != 0 && enc <= 0)) return fail;
    if ((dir != 2 || off != 0) && mFlag3e != 0 && enc < 0) {
        if ((fc::Unshift(this) & 0xff) == 0) return fail;
    }
    int32_t whence;
    if (dir == 3) return fail;
    if (dir < 3) {
        if (dir == 1) {
            whence = 0;
        } else if (dir > 1) {
            whence = 1;
            if ((mode & 8) != 0) off = off - static_cast<int32_t>(mEgptr - mGptr);
        } else {
            return fail;
        }
    } else {
        if (dir >= 5) return fail;
        whence = 2;
    }
    if (enc < 0) enc = 0;
    if (fc::Fseek(mFile, enc * off, whence) != 0) return fail;
    if ((mode & 8) != 0) {
        uint32_t p = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)) + 0x30;
        mEback = p;
        mGptr = p;
        mEgptr = p;
    }
    mFlag3e = 0;
    return FposInt{fw::Ftell(mFile), 0};
}

int32_t BasicFilebufChar::overflow(int32_t c) {
    mFlag3e = 1;
    if (mFile == 0) return -1;
    if (c == -1) return 0;
    if (mUnbuffered != 0) {
        if (fw::Fputc(c, mFile) == -1) return -1;
        return c;
    }
    void* cvt = Cvt(mCodecvt);
    int32_t enc = fw::CodecvtEncoding(cvt);
    if (enc == 1) {
        char in = static_cast<char>(c);
        char out = 0;   // the original leaves this stack byte unset unless the facet writes it
        const char* fromNext;
        char* toNext;
        int32_t r = filebuf_charout_callee::CodecvtOutChar(cvt, &mState, &in, &in + 1, &fromNext, &out, &out + 1, &toNext);
        if (r == 3) {
            out = in;
        } else if (r < 3 && r > 0) {
            return -1;
        }
        if (fw::Fputc(static_cast<int32_t>(out), mFile) == -1) return -1;
        return c;
    }
    MslCharRep* s = nullptr;
    char allocator = 0;
    filebuf_charstr_callee::StringInit(&s, 3, 0, &allocator);
    char in = static_cast<char>(c);
    // The original never leaves this loop except through the error answer (the flag it tests is never changed);
    // the fputc loop after it is unreachable.
    for (;;) {
        char* data = s->data;
        int32_t len = static_cast<int32_t>(s->len);
        const char* fromNext;
        char* toNext;
        int32_t r = filebuf_charout_callee::CodecvtOutChar(cvt, &mState, &in, &in + 1, &fromNext, data, data + len, &toNext);
        if (r == 2) {
            filebuf_charstr_callee::StringDtor(&s, -1);
            return -1;
        }
        if (r < 2) {
            if (r == 0) {
                filebuf_charstr_callee::StringResize(&s, static_cast<uint32_t>(toNext - data), 0);
            } else if (r > 0) {
                filebuf_charstr_callee::StringResize(&s, static_cast<uint32_t>(s->len) * 2 + 1, 0);
            }
        } else if (r < 4) {
            filebuf_charstr_callee::StringAssign(&s, 1, static_cast<char>(static_cast<uint8_t>(in)));
        }
    }
}

// std::basic_filebuf<wchar_t>::uflow (unit v5025).

using namespace filebuf_callee;

namespace {

// 32-bit fields hold offsets from the arena base (the Mac's pointers).
inline void* P_3(uint32_t v) { return v != 0 ? static_cast<void*>(tpw::gArenaBase + v) : nullptr; }
inline uint32_t A_3(const void* p) { return tpw::ArenaOffset(p); }

// The refcount release the compiler inlined at every exit of the string path.
void StringRelease(MslStrRep*& str) {
    MslStrRep* rep = str;
    if (rep->refs == 0) {
        int32_t t = rep->refs;
        if (t == 0) t = 1;
        rep->refs = t;
    }
    int32_t n = rep->refs;
    rep->refs = n - 1;
    if (n - 1 != 0) rep = nullptr;
    str = rep;
    if (str != nullptr) {
        filebuf_callee::Delete(P_3(str->data));
        filebuf_callee::Delete(str);
    }
}

// Puts one character in the pushback buffer and points the get area behind it.
void PutBack(BasicFilebufWchar* t, uint16_t ch) {
    uint32_t eback = t->mEback;
    int32_t diff = static_cast<int32_t>(t->mEgptr - eback);
    uint32_t k = static_cast<uint32_t>(diff / 2);
    if (k == 0xe) {
        Wmemmove(P_3(eback), P_3(eback + 8), 10);
        k = static_cast<uint32_t>((static_cast<uint64_t>(0xAAAAAAABu) * 14u) >> 32) >> 1;   // 4
    }
    uint32_t self = A_3(t);
    t->mEback = self + 0x30;
    uint32_t p = self + (k << 1) + 0x32;
    t->mGptr = p;
    t->mEgptr = p;
    *static_cast<uint16_t*>(P_3(t->mGptr - 2)) = ch;
}

}  // namespace

// 0x10221410
uint32_t BasicFilebufWchar::uflow() {
    mFlag4c = 0;
    if (mFile == 0) return 0xffff;
    if (mGptr < mEgptr) {
        mGptr += 2;
        return *static_cast<uint16_t*>(P_3(mGptr - 2));
    }
    if (mUnbuffered != 0) {
        int32_t c = Fgetc(P_3(mFile));
        if (c == -1) return 0xffff;
        PutBack(this, static_cast<uint16_t>(c));
        return static_cast<uint16_t>(c);
    }
    int32_t n = filebuf_callee::CodecvtMaxLength(P_3(mCodecvt));
    const char* fromNext;
    char16_t* toNext;
    if (n == 1) {
        int32_t c = Fgetc(P_3(mFile));
        if (c == -1) return 0xffff;
        int8_t b = static_cast<int8_t>(c);
        char16_t to = 0;
        int32_t res = CodecvtIn(P_3(mCodecvt), mState, reinterpret_cast<const char*>(&b),
                                reinterpret_cast<const char*>(&b) + 1, &fromNext, &to, &to + 1, &toNext);
        if (res == 3) {
            to = static_cast<char16_t>(static_cast<int16_t>(b));
        } else if (res <= 3 && res >= 1) {
            return 0xffff;
        }
        PutBack(this, to);
        return to;
    }
    // Multibyte path: grow a string byte by byte until the facet converts it. The code after the
    // loop in the listing (r27 != 1) is unreachable: r27 stays 1, so only the returns below leave.
    MslStrRep* str = nullptr;
    MslStrRep* rep = static_cast<MslStrRep*>(New(0x10));
    str = rep;
    if (rep != nullptr) {
        rep->cap = 3;
        rep->refs = 1;
        rep->data = A_3(New(static_cast<uint32_t>(rep->cap + 1)));
    }
    *static_cast<char*>(P_3(str->data)) = gMslEmptyChar;
    str->len = 0;
    for (; n > 1; n--) {
        int32_t c = Fgetc(P_3(mFile));
        if (c == -1) {
            StringRelease(str);
            return 0xffff;
        }
        StringAppend(&str, 1, static_cast<char>(c));
    }
    for (;;) {
        int32_t c = Fgetc(P_3(mFile));
        if (c == -1) {
            StringRelease(str);
            return 0xffff;
        }
        StringAppend(&str, 1, static_cast<char>(c));
        char16_t to = 0;
        const char* data = static_cast<const char*>(P_3(str->data));
        int32_t res = CodecvtIn(P_3(mCodecvt), mState, data, data + str->len, &fromNext, &to, &to + 1, &toNext);
        if (res == 3) {
            to = static_cast<char16_t>(static_cast<int16_t>(*StringIndex(&str, 0)));
            (void)to;
            continue;
        }
        if (res > 3) continue;
        if (res >= 2) {
            StringRelease(str);
            return 0xffff;
        }
    }
}

// std::basic_filebuf<wchar_t>: unshift 0x10226dcc, sync 0x1021e22c, ~basic_filebuf 0x1021cd4c (unit v5743).

using namespace filebuf_unshift_callee;

// Writes the codecvt's shift-back bytes to the file. 2 (noconv) answers false; 0 and 1 write the bytes (1 asks again);
// any other answer ends the loop with true.
bool BasicFilebufWchar::unshift() {
    char buf[4];
    int32_t result = 1;
    while (result == 1) {
        char* toNext = buf;
        result = CodecvtUnshift(reinterpret_cast<void*>(tpw::gArenaBase + mCodecvt),
                                &mState[0], buf, buf + 4, &toNext);   // gArenaBase is a byte pointer
        if (result == 2) return false;
        if (result >= 0 && result < 2) {
            const char* p = buf;
            while (p < toNext) {
                char c = *p++;
                if (filebuf_wseek_callee::Fputc(static_cast<int32_t>(c), mFile) == -1) return false;
            }
        }
    }
    return true;
}

int32_t BasicFilebufWchar::sync() {
    bool failed = true;
    if (mFile != 0) {
        if (Fflush(mFile) == 0) failed = false;
    }
    return failed ? -1 : 0;
}

BasicFilebufWchar* BasicFilebufWchar::destroy(int16_t flag) {
    BasicFilebufWchar* self = this;
    if (self != nullptr) {
        // The host's 32-bit value of an address is its offset from the arena base (the globals are outside the arena: wraps).
        const uintptr_t base = reinterpret_cast<uintptr_t>(tpw::gArenaBase);
        uint32_t files = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gIfsWFiles) - base);
        self->mVtable = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gIfsWVtblD) - base);
        uint32_t f = self->mFile;
        if (f != files && f != files + 0x54 && f != files + 0xa8) self->close();
        self->mVtable = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gIfsWVtblC) - base);
        filebuf_unshift_callee::LocaleDtor(reinterpret_cast<uint8_t*>(self) + 0x1c, -1);
        if (flag > 0) filebuf_wseek_callee::Delete(self);
    }
    return self;
}

// std::basic_ifstream<wchar_t>::~basic_ifstream 0x1021d5bc (324 B), unit v5201.

BasicIfstreamWchar* BasicIfstreamWchar::destroy(int16_t flag) {
    if (this == nullptr) return this;
    auto* filebuf = reinterpret_cast<BasicFilebufWchar*>(reinterpret_cast<uint8_t*>(this) + 0x10);   // Mac this+0xc
    void* locale = mLocale;                                                                          // Mac this+0x28
    void* ios = &mIosVtable;                                                                         // Mac this+0x5c
    const uint8_t* files = gIfsWFiles;

    mVtable = gIfsWVtblB;
    mVbase->mFirst = &gIfsWVtblB[3];   // vtable + 0xc
    mVbase->mBaseOffset = reinterpret_cast<const uint8_t*>(ios) - reinterpret_cast<const uint8_t*>(mVbase);
    // The original tests this+0xc and this+0x28 against 0 (never true for a real object).
    mFilebufVt = gIfsWVtblD;
    if (mFile != files && mFile != files + 0x54 && mFile != files + 0xa8) filebuf->close();
    mFilebufVt = gIfsWVtblC;
    ifstream_callee::LocaleDtor(locale, -1);

    mVtable = gIfsWVtblA;
    mVbase->mFirst = &gIfsWVtblA[3];
    mVbase->mBaseOffset = reinterpret_cast<const uint8_t*>(filebuf) - reinterpret_cast<const uint8_t*>(mVbase);
    if (flag != 0) {
        mIosVtable = gIfsWVtblE;
        ifstream_callee::IosBaseDtor(ios, 0);
    }
    if (flag > 0) ifstream_callee::Delete(this);
    return this;
}

// std::basic_ifstream<wchar_t>::basic_ifstream() 0x10221e20 (180 B), unit v5724.

extern char gIosBaseVt[];   // 0x103bd034 data import __vt__Q23std8ios_base

BasicIfstreamWchar* BasicIfstreamWchar::construct(int16_t flag) {
    // The Mac stores the virtual-base pointer (this + 0x5c) and the ios_base vtable words only when the flag is set.
    if (flag != 0) {
        mVbase = reinterpret_cast<IfstreamWVbTable*>(&mIosVtable);
        mIosVtable = gIosBaseVt;
        mIosVtable = gIfsWVtblE;
    }
    ifstream_callee::IstreamCtor(this, 0, &mFilebufVt);   // basic_istream(streambuf* = this + 0xc), flag 0
    mVtable = gIfsWVtblB;
    mVbase->mFirst = reinterpret_cast<const uint8_t*>(gIfsWVtblB) + 0xc;
    mVbase->mBaseOffset = reinterpret_cast<intptr_t>(&mIosVtable) - reinterpret_cast<intptr_t>(mVbase);
    ifstream_callee::FilebufCtor(&mFilebufVt, nullptr);   // basic_filebuf(FILE* = NULL)
    return this;
}

// std::basic_istream<char>::read 0x1020d6b4 and ::getline 0x102079f8 (unit v5014).

namespace {

const char kFailureType[] = "!std::exception!!std::ios_base::failure!!";   // 0x10250846 / 0x10250098
const char kFailureText[] = "ios_base failure in clear";                    // 0x10250722

// The exception object is built on the stack; __throw returns in the test, so execution goes on afterwards.
void RaiseFailure() {
    MslThrowObject object;
    object.mVtbl = gIosFailureVtbl;
    size_t length = std::strlen(kFailureText);
    object.mMessage = new char[length + 1];
    object.mCount = nullptr;
    if (object.mMessage) {
        object.mCount = new int32_t;
        *object.mCount = 1;
    }
    std::strcpy(object.mMessage, kFailureText);
    MslThrow(kFailureType, &object, gIosFailureDtorVec);
}

// state |= bits; no buffer sets badbit; a state bit under the exception mask throws.
void Finish(MacIosBase* ios, uint32_t bits) {
    ios->mState |= bits;
    if (ios->mStreamBuf.mOffset == 0) ios->mState |= 1;
    if ((ios->mState & ios->mExceptions) != 0) RaiseFailure();
}

// The start of both: gcount = 0, tie flush, good check. True when reading may go on.
bool Prepare(BasicIstreamChar* self) {
    MacIosBase* ios = self->mIos.get();
    bool ok = false;
    self->mGcount = 0;
    if (!MslIosGood(ios)) {
        MslIosSetstate(ios, 4);
    } else {
        if (MslIosTie(ios) != nullptr) MslOstreamFlush(MslIosTie(ios));
        if (MslIosGood(ios)) ok = true;
        else MslIosSetstate(ios, 4);
    }
    return ok;
}

inline uint32_t Entry(MacStreamBuf* sb, uint32_t offset) { return *reinterpret_cast<uint32_t*>(sb->mVtable.get() + offset); }

// The stream buffer's bump: the get pointer moves on, or the virtual uflow runs.
inline void Bump(MacStreamBuf* sb) {
    if (sb->mGptr.mOffset < sb->mEgptr.mOffset) sb->mGptr.mOffset = sb->mGptr.mOffset + 1;
    else MslStreamBufVirtual(sb, Entry(sb, 0x2c));
}

}  // namespace

BasicIstreamChar* BasicIstreamChar::read(char* buffer, int32_t count) {
    bool ok = Prepare(this);
    if (buffer == nullptr || count <= 0) {
        Finish(mIos.get(), 4);
        return this;
    }
    if (!ok) return this;
    uint32_t flags = 0;
    while (count > 0) {
        MacStreamBuf* sb = mIos->mStreamBuf.get();
        int32_t c;
        if (sb->mGptr.mOffset < sb->mEgptr.mOffset) {
            uint8_t* p = sb->mGptr.get();
            sb->mGptr.mOffset = sb->mGptr.mOffset + 1;
            c = *p;
        } else {
            c = MslStreamBufVirtual(sb, Entry(sb, 0x2c));
        }
        if (c == -1) {
            flags |= 6;
            break;
        }
        mGcount = mGcount + 1;
        count--;
        *buffer = static_cast<char>(c);
        buffer++;
    }
    Finish(mIos.get(), flags);
    return this;
}

BasicIstreamChar* BasicIstreamChar::getline(char* buffer, int32_t count, char delimiter) {
    bool ok = Prepare(this);
    if (buffer == nullptr || count <= 0) {
        Finish(mIos.get(), 4);
        return this;
    }
    if (!ok) return this;
    uint32_t flags = 0;
    for (;;) {
        MacStreamBuf* sb = mIos->mStreamBuf.get();
        int32_t c;
        if (sb->mGptr.mOffset < sb->mEgptr.mOffset) c = *sb->mGptr.get();
        else c = MslStreamBufVirtual(sb, Entry(sb, 0x28));
        if (c == -1) {
            flags |= 2;
            break;
        }
        char ch = static_cast<char>(c);
        if (ch == delimiter) {
            Bump(mIos->mStreamBuf.get());
            mGcount = mGcount + 1;
            break;
        }
        if (count == 1) {
            flags |= 4;
            break;
        }
        Bump(mIos->mStreamBuf.get());
        count--;
        mGcount = mGcount + 1;
        *buffer = ch;
        buffer++;
    }
    if (mGcount == 0) flags |= 4;
    *buffer = 0;
    Finish(mIos.get(), flags);
    return this;
}

// std::basic_ofstream<wchar_t>::basic_ofstream() 0x10221f20 (unit v5755).
// Mac layout: +0 virtual-base pointer, +4 vtable word, +8 filebuf, +0x58 the ios_base part (the virtual base).
// The host object uses native pointers: the slot sits at +8, the filebuf at +0x10 and the ios part at +0x38 (as basic_ifstream's).

extern uint32_t gOfsWVtbl[4];     // 0x102b1f50 (TOC -0x5eac)
extern uint32_t gIfsWVtblE[4];    // 0x102b1f20 (TOC -0x5eb0)

namespace ignore_callee {

void OfstreamCtor(IgnoreStream* stream, int32_t flag) {
    uint8_t* base = reinterpret_cast<uint8_t*>(stream);
    IgnoreIos* ios = reinterpret_cast<IgnoreIos*>(base + 0x38);
    // The virtual-base pointer and the ios_base vtable words are stored only when the flag (a short) is set.
    if (static_cast<int16_t>(flag) != 0) {
        stream->mIos = ios;
        ios->mVtable = gIosBaseVt;
        ios->mVtable = gIfsWVtblE;
    }
    WostreamCtor(stream, 0, base + 0x10);                 // basic_ostream(streambuf* = this + 8), flag 0
    stream->mSlot = gOfsWVtbl;
    stream->mIos->mVtable = reinterpret_cast<const uint8_t*>(gOfsWVtbl) + 0xc;
    stream->mIos->mBaseOffset = static_cast<int32_t>(reinterpret_cast<intptr_t>(ios) - reinterpret_cast<intptr_t>(stream->mIos));
    ifstream_callee::FilebufCtor(base + 0x10, nullptr);   // basic_filebuf(FILE* = NULL)
}

}  // namespace ignore_callee

// std::basic_ostream<char>::flush 0x1020a604 (unit v5663).
// The catch handlers of the listing (0x1020a6cc, 0x1020a794, 0x1020a8ac) are only reached through the exception tables,
// never by fall-through or branch; they are not translated. The sentry flag at 0x99 is set to 1 before the final check, so
// the final recursive flush of the listing never runs (kept as the dead test below).

namespace {
const char kFailureTypeName[] = "!std::exception!!std::ios_base::failure!!";   // 0x10250098
const char kFailureMessage[] = "ios_base failure in clear";                    // 0x1024fde4
}  // namespace

BasicIstreamChar* OstreamFlush(BasicIstreamChar* self) {
    bool done = false;
    bool sentryDone = false;
    if (!MslIosGood(self->mIos.get())) {
        MslIosSetstate(self->mIos.get(), 4);
    } else {
        if (MslIosTie(self->mIos.get()) != nullptr) {
            OstreamFlush(static_cast<BasicIstreamChar*>(MslIosTie(self->mIos.get())));
        }
        if (MslIosGood(self->mIos.get())) {
            done = true;
        } else {
            MslIosSetstate(self->mIos.get(), 4);
        }
    }
    (void)done;
    sentryDone = true;

    MacIosBase* ios = self->mIos.get();
    if (ios->mStreamBuf.get() != nullptr) {
        MacStreamBuf* sb = ios->mStreamBuf.get();
        bool failed = false;
        if (MslStreamBufVirtual(sb, 0x1c) == -1) failed = true;
        if (failed) {
            ios = self->mIos.get();
            ios->mState |= 1;
            if (ios->mStreamBuf.get() == nullptr) ios->mState |= 1;
            if ((ios->mState & ios->mExceptions) != 0) {
                MslThrowObject object;
                object.mVtbl = gIosFailureVtbl;
                char* message =
                    static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(kFailureMessage) + 1)));
                object.mMessage = message;
                object.mCount = nullptr;
                if (message != nullptr) {
                    int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
                    if (count != nullptr) *count = 1;
                    object.mCount = count;
                }
                IME::MslStrcpy(object.mMessage, kFailureMessage);
                ::MslThrow(kFailureTypeName, &object, gIosFailureDtorVec);
            }
        }
    }

    MacIosBase* last = self->mIos.get();
    MacSentryIos* flagsView = reinterpret_cast<MacSentryIos*>(last);
    if ((last->mState & 5) == 0 && (flagsView->mFlags & 0x2000) != 0 && !sentryDone) OstreamFlush(self);
    return self;
}

// std::basic_ostream<wchar_t>::flush 0x102268f8 and the constructor 0x10226c94 (unit v5898).

uint32_t gWostreamVtbl[4] = {0x102b1f00, 0, 0, 0};   // 0x102b1f08 (TOC -0x5eb4)

namespace {
uint32_t Lo32(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
void W32(void* p, uint32_t v) { std::memcpy(p, &v, 4); }
uint32_t R32(const void* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
uint32_t Off(const void* p) { return tpw::ArenaOffset(p); }
}  // namespace

WostreamW* WostreamFlush(WostreamW* self) {
    // Stack flags of the original: the "good" flag (0x98) is never read; the done flag (0x99) is set before the end, so the
    // final "unitbuf and not done" flush never runs. The catch handler code at 0x102269c0.. is not reachable.
    MacIosBase* ios = self->mIos.get();
    if (!wostream_u::Good(ios)) {
        wostream_u::Setstate(ios, 4);
    } else {
        void* tie = wostream_u::Tie(self->mIos.get());
        if (tie != nullptr) {
            void* tied = wostream_u::Tie(self->mIos.get());
            WostreamFlush(static_cast<WostreamW*>(tied));
        }
        if (!wostream_u::Good(self->mIos.get())) wostream_u::Setstate(self->mIos.get(), 4);
    }
    MacIosBase* io = self->mIos.get();
    MacStreamBuf* sb = io->mStreamBuf.get();
    if (sb != nullptr) {
        uint32_t entry = R32(sb->mVtable.get() + 0x1c);
        bool atEnd = wostream_u::Virt(sb, entry) == -1;
        if (atEnd) {
            io = self->mIos.get();
            io->mState |= 1;
            io = self->mIos.get();
            if (io->mStreamBuf.get() == nullptr) io->mState |= 1;
            if ((io->mState & io->mExceptions) != 0) {
                MslThrowObject object;
                object.mVtbl = gIosFailureVtbl;
                size_t len = std::strlen(kFailureText);
                object.mMessage = wostream_u::NewArray(static_cast<uint32_t>(len + 1));
                object.mCount = nullptr;
                if (object.mMessage != nullptr) {
                    int32_t* count = wostream_u::NewInt();
                    if (count != nullptr) *count = 1;
                    object.mCount = count;
                }
                std::strcpy(object.mMessage, kFailureText);
                wostream_u::Throw(kFailureType, &object, gIosFailureDtorVec);
            }
        }
    }
    return self;
}

WostreamW* WostreamConstruct(WostreamW* self, int16_t flag, uint32_t streambuf) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (flag != 0) {
        W32(s, Off(s + 8));
        W32(s + 8, Lo32(gIosBaseVt));
        W32(s + 8, Lo32(gIfsWVtblE));
    }
    W32(s + 4, Lo32(gWostreamVtbl));
    uint8_t* ios = tpw::gArenaBase + R32(s);
    W32(ios, Lo32(gWostreamVtbl) + 0xc);
    W32(ios + 0x44, Off(s + 8) - R32(s));
    wostream_u::IosInit(ios, streambuf);
    W32(ios + 0x3c, 0);
    char16_t fill = wostream_u::Widen(ios, 0x20);
    std::memcpy(ios + 0x40, &fill, 2);
    return self;
}

// std::basic_ostream<char>::sentry::~sentry 0x1020758c (unit v5251).

MacSentryObject* OstreamSentryDtor(MacSentryObject* self, int16_t deleteFlag) {
    if (self != nullptr) {
        MacSentryStream* stream = self->mStream;
        MacSentryIos* ios = stream->mIos;
        if ((ios->mState & 5) == 0 && (ios->mFlags & 0x2000) != 0 && self->mDone == 0) MslOstreamFlush(stream);
        if (deleteFlag > 0) MslOperatorDelete(self);
    }
    return self;
}

// std::basic_streambuf<char> base functions (unit v5104).

// 0x10070690: the virtual at vtable 0x28 (underflow) is called; -1 is passed on, else the byte at gptr is
// consumed and returned (unsigned).
int32_t BasicStreambufChar::uflow() {
    int32_t result = -1;
    if (streambuf_callee::Underflow(this) != -1) {
        const uint8_t* p = mGptr;
        mGptr = p + 1;
        result = *p;
    }
    return result;
}

// 0x1007048c
void BasicStreambufChar::imbue(const void* loc) { streambuf_callee::LocaleAssign(&mLocale, loc); }

// 0x10070e1c: copies the locale into the hidden result.
void BasicStreambufChar::getloc(void* out) const { streambuf_callee::LocaleCopyCtor(out, &mLocale); }

// 0x10070304: a NULL this does nothing; the vtable is reset, the locale destroyed, and the object
// deleted when the (signed 16-bit) flag is positive.
BasicStreambufChar* BasicStreambufChar::destroy(int16_t flag) {
    if (this != nullptr) {
        mVtable = gFilebufCharVtblA;
        filebuf_char_callee::LocaleDtor(&mLocale, -1);
        if (flag > 0) filebuf_callee::Delete(this);
    }
    return this;
}

// std::basic_streambuf<char> xsgetn / xsputn (unit v5147).

// 0x10070974. Copies what the get area holds, then pulls single characters through the virtual uflow (slot 0x2c).
// Like the original, a negative available count is returned as the base of the result.
int32_t BasicStreambufCharIO::xsgetn(char* dst, int32_t count) {
    int32_t avail = static_cast<int32_t>(mEgptr - mGptr);
    int32_t n = avail < count ? avail : count;
    if (n > 0) {
        std::memcpy(dst, mGptr, static_cast<size_t>(n));
        dst += n;
        mGptr += n;
        count -= n;
    }
    while (count > 0) {
        int32_t c;
        if (mGptr < mEgptr) {
            c = *mGptr++;
        } else {
            c = streambuf_io_callee::Uflow(this);
        }
        if (c == -1) break;
        n++;
        *dst++ = static_cast<char>(c);
        count--;
    }
    return n;
}

// 0x10070808. Copies into the put area, then pushes single characters through the virtual overflow (slot 0x38).
int32_t BasicStreambufCharIO::xsputn(const char* src, int32_t count) {
    int32_t avail = static_cast<int32_t>(mEpptr - mPptr);
    int32_t n = avail < count ? avail : count;
    if (n > 0) {
        std::memcpy(mPptr, src, static_cast<size_t>(n));
        src += n;
        mPptr += n;
        count -= n;
    }
    while (count > 0) {
        uint8_t ch = static_cast<uint8_t>(*src++);
        int32_t r;
        if (mPptr < mEpptr) {
            uint8_t* p = mPptr++;
            *p = ch;
            r = *p;
        } else {
            r = streambuf_io_callee::Overflow(this, ch);
        }
        if (r == -1) break;
        n++;
        count--;
    }
    return n;
}

// std::basic_streambuf<wchar_t>::xsputn 0x1021feac, ::xsgetn 0x10220024.

// Bytes between two pointers as the Mac's 32-bit subtraction, halved toward zero (srawi + addze).
static int32_t HalfBytes(const char16_t* hi, const char16_t* lo) {
    int32_t d = static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hi)) -
                                     static_cast<uint32_t>(reinterpret_cast<uintptr_t>(lo)));
    return d / 2;
}

int32_t MslWStreambuf::xsputn(const char16_t* s, int32_t n) {
    int32_t avail = HalfBytes(mPEnd, mPNext);
    int32_t k = avail < n ? avail : n;
    if (k > 0) {
        wmemcpy(mPNext, s, static_cast<size_t>(k));
        s += k;
        mPNext = reinterpret_cast<char16_t*>(reinterpret_cast<char*>(mPNext) + k * 2);
        n -= k;
    }
    while (n > 0) {
        char16_t c = *s++;
        uint32_t r;
        if (mPNext < mPEnd) {
            char16_t* p = mPNext;
            mPNext = p + 1;
            *p = c;
            r = *p;
        } else {
            r = overflow(c);
        }
        if (static_cast<uint16_t>(r) == 0xffff) break;
        k++;
        n--;
    }
    return k;
}

int32_t MslWStreambuf::xsgetn(char16_t* s, int32_t n) {
    int32_t avail = HalfBytes(mGEnd, mGNext);
    int32_t k = avail < n ? avail : n;
    if (k > 0) {
        wmemcpy(s, mGNext, static_cast<size_t>(k));
        s += k;
        mGNext = reinterpret_cast<char16_t*>(reinterpret_cast<char*>(mGNext) + k * 2);
        n -= k;
    }
    while (n > 0) {
        uint32_t r;
        if (mGNext < mGEnd) {
            char16_t* p = mGNext;
            mGNext = p + 1;
            r = *p;
        } else {
            r = uflow();
        }
        if (static_cast<uint16_t>(r) == 0xffff) break;
        *s++ = static_cast<char16_t>(r);
        k++;
        n--;
    }
    return k;
}

// std::basic_string<char> of the Metrowerks library, main app: compare(pos, n1, s, n2) 0x10209cac (492 B), the substring
// constructor basic_string(const&, pos, n) 0x101b2918 (792 B) and append(n, c) 0x100731b4 (408 B). The string is one pointer to
// a shared representation (len, cap, refs, data; see include/std_basic_filebuf.h).

char gMslCharNul = 0;   // 0x102ac358

namespace {

const char kOutOfRangeName[] = "!std::exception!!std::logic_error!!std::out_of_range!!";
// The exception object built on the stack (vtable word, message string with its counter) and thrown. The fake __throw returns.
void ThrowMessage(const char* typeName, const char* message, const void* vtbl, const void* dtorTV) {
    IME::MslExceptionObject object;
    object.mVtbl = vtbl;
    size_t length = IME::MslStrlen(message);
    object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(length + 1)));
    object.mRc.mCount = nullptr;
    if (object.mRc.mPtr != nullptr) {
        int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
        object.mRc.mCount = count;
        if (count != nullptr) *count = 1;
    }
    IME::MslStrcpy(object.mRc.mPtr, message);
    IME::MslThrow(typeName, &object, dtorTV);
}

}  // namespace

namespace msl_char_string {

// 0x10209cac
int32_t compare(const MslCharString* self, uint32_t pos, uint32_t n1, const char* s, uint32_t n2) {
    if (pos > self->mRep->len)
        ThrowMessage(kOutOfRangeName, "string compare: pos > len", IME::gMslExcVtblA, IME::gMslOutOfRangeDtorTV);
    uint32_t rest = self->mRep->len - pos;
    uint32_t l1 = n1 < rest ? n1 : rest;
    uint32_t count = n2 < l1 ? n2 : l1;
    int32_t result = std::memcmp(self->mRep->data + pos, s, count);
    if (result != 0) return result;
    if (l1 < n2) return -1;
    if (l1 == n2) return 0;
    return 1;
}

// 0x101b2918
MslCharString* construct(MslCharString* self, const MslCharString* src, uint32_t pos, uint32_t n) {
    self->mRep = src->mRep;
    uint32_t length = src->mRep->len;
    if (pos > length)
        ThrowMessage(kOutOfRangeName, "string copy constructor: pos > str len", IME::gMslExcVtblA, IME::gMslOutOfRangeDtorTV);
    uint32_t count = length - pos;
    if (count > n) count = n;
    if (count == length) {
        MslCharRep* rep = src->mRep;
        MslCharRep* result;
        if (rep->refs != 0) {
            rep->refs = rep->refs + 1;
            result = rep;
        } else {
            MslCharRep* copy = static_cast<MslCharRep*>(IME::MslNew(0x10));
            result = copy;
            if (copy != nullptr) {
                copy->len = rep->len;
                uint32_t len = rep->len;
                copy->cap = len + 3 - (len & 3);
                copy->refs = 1;
                copy->data = static_cast<char*>(IME::MslNew(copy->cap + 1));
                std::memcpy(copy->data, rep->data, copy->len + 1);
            }
        }
        self->mRep = result;
    } else {
        char* data = src->mRep->data;
        const char* start = data + pos;
        self->mRep = static_cast<MslCharRep*>(IME::MslNew(0x10));
        MslCharRep* rep = self->mRep;
        if (rep != nullptr) {
            rep->cap = count + 3 - (count & 3);
            rep->refs = 1;
            rep->data = static_cast<char*>(IME::MslNew(rep->cap + 1));
        }
        std::memcpy(self->mRep->data, start, count);
        self->mRep->len = count;
        self->mRep->data[count] = gMslCharNul;
    }
    return self;
}

// 0x100731b4
MslCharString* append(MslCharString* self, uint32_t n, char c) {
    if (n != 0) {
        bool tooLong;
        if (n > 0xfffffffeu) tooLong = true;
        else tooLong = !(self->mRep->len <= 0xfffffffeu - n);
        if (tooLong)
            ThrowMessage(kLengthErrorName, "basic_string::append results in string > max_size", IME::gMslLengthErrorVtbl,
                         IME::gMslLengthErrorDtorTV);
        uint32_t oldLength = self->mRep->len;
        change_size(self, oldLength + n, true);
        // The Mac adds 32-bit addresses (a length near 2^32 wraps below the data, which the throw path above reaches since the
        // fake __throw returns); the signed offset is the same wrap on the host.
        std::memset(self->mRep->data + static_cast<int32_t>(oldLength), static_cast<uint8_t>(c), n);
    }
    return self;
}

}  // namespace msl_char_string

// std::basic_string<wchar_t, nocase_wchar_traits, std::allocator<wchar_t>>: compare 0x10225a5c (496 bytes) and the substring
// constructor 0x1022213c (628 bytes). The string object is one pointer to a shared representation (length, capacity, reference
// count, data pointer; IME::MslWideStringRep). Both functions raise std::out_of_range when pos > length; the throw fake returns
// and the code runs on (as the listing does after the call).

namespace NS_MacDoze {
int32_t _wcsnicmp(const char16_t* a, const char16_t* b, uint32_t count);   // glue 0x1023a660
}

namespace {

// The inline construction of the exception object and the throw (strlen, new[], new, strcpy, __throw).
void ThrowOutOfRange(const char* message) {
    IME::MslExceptionObject ex;
    ex.mVtbl = IME::gMslExcVtblC;
    ex.mVtbl = IME::gMslExcVtblB;
    ex.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(message)) + 1));
    ex.mRc.mCount = nullptr;
    if (ex.mRc.mPtr != nullptr) {
        int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
        if (count != nullptr) *count = 1;
        ex.mRc.mCount = count;
    }
    IME::MslStrcpy(ex.mRc.mPtr, message);
    ex.mVtbl = IME::gMslExcVtblA;
    IME::MslThrow(kOutOfRangeName, &ex, IME::gMslOutOfRangeDtorTV);
}

IME::MslWideStringRep* RepOf(const void* self) { return *static_cast<IME::MslWideStringRep* const*>(self); }

}  // namespace

namespace online_callee {

// 0x10225a5c: compare(pos, n1, s, n2) const
int32_t NocaseWStringCompare(const void* self, uint32_t pos, uint32_t n1, const char16_t* s, uint32_t n2) {
    if (pos > RepOf(self)->mLength) ThrowOutOfRange("string compare: pos > len");
    const IME::MslWideStringRep* rep = RepOf(self);
    uint32_t avail = rep->mLength - pos;
    uint32_t a = (n1 < avail) ? n1 : avail;
    uint32_t m = (n2 < a) ? n2 : a;
    int32_t result = NS_MacDoze::_wcsnicmp(RepOf(self)->mData + pos, s, m);
    if (result == 0) {
        if (a < n2) return -1;
        if (a == n2) return 0;
        return 1;
    }
    return result;
}

// 0x1022213c: basic_string(const basic_string& other, pos, count)
void NocaseWStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    *static_cast<IME::MslWideStringRep**>(self) = RepOf(other);
    uint32_t length = RepOf(other)->mLength;
    if (pos > length) ThrowOutOfRange("string copy constructor: pos > str len");
    uint32_t n = length - pos;
    if (n > count) n = count;
    if (n == length) {
        IME::MslWideStringRep* rep = RepOf(other);
        if (rep->mRefs != 0) {
            rep->mRefs = rep->mRefs + 1;
        } else {
            IME::MslWideStringRep* copy = static_cast<IME::MslWideStringRep*>(IME::MslNewRep());
            if (copy != nullptr) {
                copy->mLength = rep->mLength;
                copy->mCapacity = (rep->mLength + 3) - (rep->mLength & 3);
                copy->mRefs = 1;
                copy->mData = static_cast<char16_t*>(IME::MslNew((copy->mCapacity + 1) * 2));
                IME::MslWmemcpy(copy->mData, rep->mData, copy->mLength + 1);
            }
            rep = copy;
        }
        *static_cast<IME::MslWideStringRep**>(self) = rep;
    } else {
        const char16_t* first = RepOf(other)->mData + pos;
        NocaseWStringInit(self, first, first + n);
    }
}

}  // namespace online_callee

// std::basic_string<wchar_t> operator[] 0x10212298 (336 B), main app. The string is one pointer to a representation
// (length +0, capacity +4, references +8, data +0xc on the Mac). A shared representation (references above 1) is copied
// first; the answer is the data address plus the index. The exception cleanup the listing carries is not reachable.

namespace IME {

char16_t* MslWideString_index(MslWideString* self, uint32_t index) {
    MslWideStringRep*& slot = *reinterpret_cast<MslWideStringRep**>(self);
    if (slot->mRefs > 1) {
        uint32_t length = slot->mLength;
        MslWideStringRep* old = slot;
        if (old->mRefs > 1) {
            MslWideStringRep* rep = static_cast<MslWideStringRep*>(MslNewRep());
            if (rep) {
                rep->mCapacity = (length + 3) - (length & 3);
                rep->mRefs = 1;
                rep->mData = static_cast<char16_t*>(MslNew((rep->mCapacity + 1) * 2));
            }
            old->mRefs = old->mRefs - 1;
            uint32_t n = old->mLength < length ? old->mLength : length;
            MslWmemcpy(rep->mData, old->mData, n + 1);
            rep->mLength = old->mLength;
            old = rep;
        }
        slot = old;
    }
    slot->mRefs = 0;
    return slot->mData + index;
}

}  // namespace IME

// Unit v51033: std::basic_string<char>::operator[] 0x100733b0, std::locale::locale() 0x10072d24,
// std::basic_ios<char>::widen 0x10072960, std::_RefCountedPtr<__locale_imp>::~_RefCountedPtr 0x10071f34,
// std::basic_filebuf<char>::close 0x10071b54 and underflow 0x10071a80.

// operator[]: the non-const access un-shares the string (copy on write when refs > 1), then marks the rep unshared (refs = 0).
// The catch blocks of the listing (operator delete of the new rep, __throw, __end__catch) are only reached by an exception.
char* MslCharStringIndex(MslCharString* self, uint32_t i) {
    MslCharRep* rep = self->mRep;
    if (rep->refs > 1) {
        uint32_t len = rep->len;
        MslCharRep* copy = static_cast<MslCharRep*>(filebuf_callee::New(0x10));
        if (copy) {
            copy->cap = (len + 3) - (len & 3);
            copy->refs = 1;
            copy->data = static_cast<char*>(filebuf_callee::New(copy->cap + 1));
        }
        rep->refs = rep->refs - 1;
        uint32_t n = rep->len < len ? rep->len : len;
        msl_string_callee::Memcpy(copy->data, rep->data, n + 1);
        copy->len = rep->len;
        rep = copy;
        self->mRep = rep;
    }
    rep->refs = 0;
    return rep->data + i;
}

// std::locale::locale(): copies the global locale, takes a reference, falls back to the classic locale when it has no impl.
LocaleRefPtr* LocaleConstruct(LocaleRefPtr* self) {
    self->mImp = gLocaleGlobal.mImp;
    self->mCount = gLocaleGlobal.mCount;
    if (self->mCount) *self->mCount = *self->mCount + 1;
    if (self->mImp == nullptr) self->operator=(*v51033_callee::LocaleClassic());
    return self;
}

// ~_RefCountedPtr with the hidden delete flag (signed short): the last reference destroys the impl and the count.
LocaleRefPtr* LocaleRefPtrDestroy(LocaleRefPtr* self, int16_t flag) {
    if (self) {
        int32_t* count = self->mCount;
        if (count) {
            int32_t v = *count - 1;
            *count = v;
            if (v == 0) {
                filebuf_wclose_callee::LocaleImpDtor(self->mImp, 1);
                locale_imp_callee::Delete(self->mCount);
            }
        }
        if (flag > 0) locale_imp_callee::Delete(self);
    }
    return self;
}

// basic_ios<char>::widen(c) const: getloc, use_facet<ctype<char>>, virtual widen (vtable 0x1c), destroy the temporary locale.
char BasicIosWiden(const BasicIosChar* self, char c) {
    LocaleRefPtr loc{};
    filebuf_char_callee::Getloc(&loc, const_cast<BasicIosChar*>(self));
    void* facet = UseFacetCtypeChar(reinterpret_cast<const UseFacetLocale*>(&loc));
    char r = v51033_callee::CtypeWiden(facet, c);
    LocaleRefPtrDestroy(&loc, -1);
    return r;
}

BasicFilebufChar* FilebufCharClose(BasicFilebufChar* self) {
    if (self->mFile == 0) return nullptr;
    if (self->mFlag3e != 0) {
        if (static_cast<uint8_t>(filebuf_char_callee::Unshift(self)) == 0) return nullptr;
        self->mFlag3e = 0;
    }
    uint32_t buf = tpw::ArenaOffset(self) + 0x30;   // the object's own 32-bit address (arena offset on the host)
    self->mEback = buf;
    self->mGptr = buf;
    self->mEgptr = buf;
    if (filebuf_char_callee::Fclose(self->mFile) != 0) return nullptr;
    self->mFile = 0;
    return self;
}

int32_t FilebufCharUnderflow(BasicFilebufChar* self) {
    self->mFlag3e = 0;
    if (self->mFile == 0) return -1;
    if (self->mGptr < self->mEgptr)
        return *(tpw::ArenaBase() + self->mGptr);
    int32_t r = filebuf_wclose_callee::VirtualUnderflow(self);
    if (r != -1) self->mGptr = self->mGptr - 1;
    return r;
}

// std::basic_string<char> of the Metrowerks library, main app: replace(pos, n1, n2, c) 0x10002d20 and init(n, c, allocator) 0x10072e6c.
// The throw paths build the exception object by hand (strlen, new[], counter, strcpy, __throw); the fakes of __throw return,
// so the code goes on after them, as in the original. The string is one word: the address of the shared representation.

char gMslReplaceNulCopy = 0;    // 0x10278bd0
char gMslReplaceNulPlace = 0;   // 0x10278bd1
char gMslInitFillNul = 0;       // 0x1029d295

namespace {

const char kOutOfRangeNames[] = "!std::exception!!std::logic_error!!std::out_of_range!!";   // 0x1023bdb6
const char kLengthErrorNames[] = "!std::exception!!std::logic_error!!std::length_error!!";  // 0x1023bded, 0x1023dc4e
const char kPosMessage[] = "basic_string::replace pos out of range";                          // 0x1023bcc5
const char kLengthMessage[] = "basic_string::replace length error";                           // 0x1023bcec
const char kCtorMessage[] = "string constructor: n > max_size";                               // 0x1023db82

// The inline construction of the exception object and the throw (strlen, new[], new, strcpy, __throw).
void ThrowMsg(const char* names, const char* message, const void* vtbl, const void* dtors) {
    IME::MslExceptionObject object;
    object.mVtbl = IME::gMslExcVtblC;
    object.mVtbl = IME::gMslExcVtblB;
    uint32_t size = static_cast<uint32_t>(IME::MslStrlen(message)) + 1;
    object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(size));
    object.mRc.mCount = nullptr;
    if (object.mRc.mPtr != nullptr) {
        object.mRc.mCount = static_cast<int32_t*>(IME::MslNew(4));
        if (object.mRc.mCount != nullptr) *object.mRc.mCount = 1;
    }
    IME::MslStrcpy(object.mRc.mPtr, message);
    object.mVtbl = vtbl;
    IME::MslThrow(names, &object, dtors);
}

}  // namespace

namespace msl_char_string {

// 0x10002d20
void replace(MslCharString* self, uint32_t pos, uint32_t n1, uint32_t n2, char c) {
    if (pos > self->mRep->len)
        ThrowMsg(kOutOfRangeNames, kPosMessage, IME::gMslExcVtblA, IME::gMslOutOfRangeDtorTV);
    uint32_t avail = self->mRep->len - pos;
    uint32_t n = avail < n1 ? avail : n1;
    if (n2 > 0xfffffffeu || self->mRep->len - n > 0xfffffffeu - n2)
        ThrowMsg(kLengthErrorNames, kLengthMessage, IME::gMslLengthErrorVtbl, IME::gMslLengthErrorDtorTV);
    MslCharRep* rep = self->mRep;
    uint32_t newLen = n2 + (rep->len - n);
    // Offsets are 32-bit sums on the Mac (they wrap), so they are formed as uint32_t before the pointer addition.
    uint32_t tail = pos + n;
    uint32_t head = pos + n2;
    if (rep->refs <= 1 && newLen <= rep->cap) {
        std::memmove(rep->data + head, rep->data + tail, rep->len - tail);
        if (n2 != 0) msl_string_callee::Memset(self->mRep->data + pos, static_cast<uint8_t>(c), n2);
        self->mRep->data[newLen] = gMslReplaceNulPlace;
        self->mRep->len = newLen;
    } else {
        MslCharRep* fresh = static_cast<MslCharRep*>(IME::MslNewRep());
        fresh->cap = newLen + 3 - (newLen & 3);
        fresh->refs = 1;
        fresh->data = static_cast<char*>(IME::MslNew(fresh->cap + 1));
        msl_string_callee::Memcpy(fresh->data, self->mRep->data, pos);
        if (n2 != 0) msl_string_callee::Memset(fresh->data + pos, static_cast<uint8_t>(c), n2);
        msl_string_callee::Memcpy(fresh->data + head, self->mRep->data + tail, self->mRep->len - tail);
        fresh->data[newLen] = gMslReplaceNulCopy;
        fresh->len = newLen;
        MslCharRep* old = self->mRep;
        if (old->refs == 0) old->refs = 1;
        old->refs = old->refs - 1;
        if (old->refs != 0) old = nullptr;
        self->mRep = old;
        if (self->mRep != nullptr) {
            IME::MslDelete(self->mRep->data);
            IME::MslDelete(self->mRep);
        }
        self->mRep = fresh;
    }
}

// 0x10072e6c
void init(MslCharString* self, uint32_t n, char c, const void* allocator) {
    (void)allocator;
    if (n > 0xfffffffeu)
        ThrowMsg(kLengthErrorNames, kCtorMessage, IME::gMslLengthErrorVtbl, IME::gMslLengthErrorDtorTV);
    self->mRep = static_cast<MslCharRep*>(IME::MslNewRep());
    MslCharRep* rep = self->mRep;
    if (rep != nullptr) {
        rep->cap = n + 3 - (n & 3);
        rep->refs = 1;
        rep->data = static_cast<char*>(IME::MslNew(rep->cap + 1));
    }
    msl_string_callee::Memset(self->mRep->data, static_cast<uint8_t>(c), n);
    self->mRep->len = n;
    self->mRep->data[n] = gMslInitFillNul;
}

}  // namespace msl_char_string

// 0x102208d4: true when any bit of the mask is set in classify(c)
bool StdCtypeWchar::do_is(uint32_t m, char16_t c) const {
    return (classify(c) & m) != 0;
}

// Unit v5976: std::deque<std::vector<Argument>>::grow_buf_back 0x102287e4, ::destroy 0x10228fd4 and ::~deque 0x10213b80.
// The unwinding handler of grow_buf_back (0x102289e0 on: free the block, __throw, __end__catch) only runs when a callee throws; the
// runtime's operator new never fails, so it is not translated.

uint32_t gArgDequeGrowFill = 0;   // 0x102b2088

namespace {

// The address a Mac word names, plus index 4-byte elements.
uint32_t* At_3(uint32_t word, uint32_t index) {
    return reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(((gMslMacAddressHigh << 32) | word) + static_cast<uint64_t>(index) * 4));
}

uint32_t WordOf(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

namespace argdeque_callee {

bool GrowBufBack(MslArgumentDeque* self, uint32_t from, void* spareObject, uint32_t* added, uint32_t to) {
    uint32_t* spare = static_cast<uint32_t*>(spareObject);
    uint32_t* table = &self->mCapacity;
    uint32_t remaining = to - from;
    bool swapped = false;
    for (;;) {
        uint32_t block = WordOf(online_callee::OperatorNew(self->mBlockSize * 0xc));
        if (!swapped && self->mCount == self->mCapacity) {
            if (table != spare) {
                for (int i = 0; i < 4; i++) {
                    uint32_t temp = table[i];
                    table[i] = spare[i];
                    spare[i] = temp;
                }
            }
            swapped = true;
            CdequeReserve(table, spare[0] * 2);
            uint32_t endSlot = spare[2] + spare[1];
            uint32_t endIndex = endSlot < spare[0] ? endSlot : endSlot - spare[0];
            MslCdequeIter last{reinterpret_cast<Cd*>(spare), spare[1], At_3(spare[3], endIndex)};
            MslCdequeIter first{reinterpret_cast<Cd*>(spare), 0, At_3(spare[3], spare[2])};
            CdequeDoAssignArgumentIter(reinterpret_cast<Cd*>(table), first, last);
        }
        uint32_t position = self->mCount - 1 + self->mStart;
        if (position >= self->mCapacity) position -= self->mCapacity;
        *At_3(self->mTable, position) = block;
        CdequePushBack(table, &gArgDequeGrowFill);
        *added = *added + 1;
        if (remaining <= self->mBlockSize) break;
        remaining -= self->mBlockSize;
    }
    return swapped;
}

}  // namespace argdeque_callee

void ArgumentDequeDestroy(MslArgumentDeque* self, MslArgumentDequeIter first, MslArgumentDequeIter last) {
    if (first.mElement == last.mElement) return;
    uint32_t blockSize = self->mBlockSize;
    uint32_t count = last.mIndex - first.mIndex;
    uint32_t blockNumber = blockSize != 0 ? first.mIndex / blockSize : 0;
    uint32_t offset = first.mIndex - blockNumber * blockSize;
    uint32_t total = offset + count;
    uint32_t end = total < blockSize ? total : blockSize;
    Cd* ring = reinterpret_cast<Cd*>(&self->mCapacity);
    MslCdequeIter start{ring, 0, At_3(self->mTable, self->mStart)};
    MslCdequeIter copy;
    cdeque_callee::IterCopy(&copy, &start);
    MslCdequeIter it;
    cdeque_callee::IterCopy(&it, cdeque_callee::IterAdd(&copy, static_cast<int32_t>(blockNumber)));
    uint32_t* slot = it.mPtr;
    uint32_t block = *slot;
    argdestroy_callee::DestroyRange(block + offset * 0xc, block + end * 0xc);
    uint32_t left = count - (end - offset);
    if (left == 0) return;
    slot++;
    if (slot == At_3(it.mDeque->mWordC, it.mDeque->mWord0)) slot = At_3(it.mDeque->mWordC, 0);
    while (left >= blockSize) {
        block = *slot;
        argdestroy_callee::DestroyRange(block, block + blockSize * 0xc);
        slot++;
        if (slot == At_3(it.mDeque->mWordC, it.mDeque->mWord0)) slot = At_3(it.mDeque->mWordC, 0);
        left -= blockSize;
    }
    if (left != 0) {
        block = *slot;
        argdestroy_callee::DestroyRange(block, block + left * 0xc);
    }
}

MslArgumentDeque* ArgumentDequeDestruct(MslArgumentDeque* self, int16_t deleteFlag) {
    if (self != nullptr) {
        online_callee::DequeTearDown(self);
        // The original also tests this + 4 against 0 twice; that only fails at the top of the Mac's address space.
        if (self->mTable != 0) online_callee::OperatorDelete(At_3(self->mTable, 0));
        if (deleteFlag > 0) online_callee::OperatorDelete(self);
    }
    return self;
}

// Unit v5938: the Metrowerks std::deque<std::vector<Argument>>: push_back 0x10224a3c, pop_front 0x1022488c, the copy constructor
// 0x10224fbc and make_iterator 0x10228c84. Elements are 0xc bytes (the Mac's vector); all words are the Mac's 32-bit words.

uint32_t gArgDequeBlockSize = 0x2a;   // 0x102b1f94
uint32_t gArgDequeFill = 0;           // 0x102b1f98

namespace {

void* Native_2(uint32_t word) { return reinterpret_cast<void*>(static_cast<uintptr_t>(gMslMacAddressHigh | word)); }
uint32_t Word_4(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
uint32_t* Table(const MslArgumentDeque* d) { return static_cast<uint32_t*>(Native_2(d->mTable)); }

}  // namespace

namespace online_callee {

void ArgumentDequePushBack(void* dequeObject, const void* vec) {
    MslArgumentDeque* self = static_cast<MslArgumentDeque*>(dequeObject);
    if (self->mSize > 0x15555554) {
        // deque::push_back length error; the library throws a std::length_error (the fake __throw returns and the code runs on)
        IME::MslExceptionObject object;
        object.mVtbl = IME::gMslExcVtblB;
        const char* message = "deque::push_back length error";
        uint32_t length = static_cast<uint32_t>(IME::MslStrlen(message));
        object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(length + 1));
        object.mRc.mCount = nullptr;
        if (object.mRc.mPtr != nullptr) {
            int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
            if (count != nullptr) *count = 1;
            object.mRc.mCount = count;
        }
        IME::MslStrcpy(object.mRc.mPtr, message);
        object.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &object, IME::gMslLengthErrorDtorTV);
    }
    if (self->mCount == 0) {
        argdeque_callee::CdequeReserve(&self->mCapacity, 2);
        argdeque_callee::CdequeResize(&self->mCapacity, 1, nullptr);
    }
    uint32_t spare[4] = {0, 0, 0, 0};   // a std::__cdeque<const void*>
    uint32_t added = 0;
    if (self->mFirst + self->mSize == self->mBlockSize * (self->mCount - 1))
        argdeque_callee::GrowBufBack(self, 0, spare, &added, 1);
    uint32_t index = self->mFirst + self->mSize;
    uint32_t element;
    if (index < self->mBlockSize) {
        element = Table(self)[self->mStart] + index * 0xc;
        if (element != 0) argdeque_callee::VectorCopy(Native_2(element), vec);
    } else {
        uint32_t number = self->mBlockSize != 0 ? index / self->mBlockSize : 0;
        const uint32_t* slot = static_cast<const uint32_t*>(argdeque_callee::CdequeIndex(&self->mCapacity, number));
        uint32_t rest = index - (self->mBlockSize != 0 ? index / self->mBlockSize : 0) * self->mBlockSize;
        element = *slot + rest * 0xc;
        if (element != 0) argdeque_callee::VectorCopy(Native_2(element), vec);
    }
    self->mSize = self->mSize + 1;
    if (spare[3] != 0) IME::MslDelete(Native_2(spare[3]));
    while (self->mFirst >= (self->mBlockSize * 3) >> 1) {
        IME::MslDelete(Native_2(Table(self)[self->mStart]));
        argdeque_callee::CdequePopFront(&self->mCapacity);
        self->mFirst = self->mFirst - self->mBlockSize;
    }
}

void ArgumentDequePopFront(void* dequeObject) {
    MslArgumentDeque* self = static_cast<MslArgumentDeque*>(dequeObject);
    uint32_t number = self->mBlockSize != 0 ? self->mFirst / self->mBlockSize : 0;
    const uint32_t* slot = static_cast<const uint32_t*>(argdeque_callee::CdequeIndex(&self->mCapacity, number));
    uint32_t rest = self->mFirst - (self->mBlockSize != 0 ? self->mFirst / self->mBlockSize : 0) * self->mBlockSize;
    argdeque_callee::VectorDtor(Native_2(*slot + rest * 0xc), -1);
    self->mFirst = self->mFirst + 1;
    uint32_t size = self->mSize - 1;
    self->mSize = size;
    if (size == 0) {
        // empty: let go of every block but one (a count below 2 would loop 2^32 times, as on the Mac)
        for (uint32_t left = self->mCount - 2; left != 0; left = left - 1) {
            self->mCount = self->mCount - 1;
            uint32_t at = self->mCount - 1 + self->mStart;
            if (at >= self->mCapacity) at = at - self->mCapacity;
            uint32_t* entry = Table(self) + at;
            IME::MslDelete(Native_2(*entry));
            *entry = 0;
        }
        self->mFirst = self->mBlockSize >> 1;
    }
}

void DequeCopy(void* dequeObject, const void* otherObject) {
    MslArgumentDeque* self = static_cast<MslArgumentDeque*>(dequeObject);
    const MslArgumentDeque* other = static_cast<const MslArgumentDeque*>(otherObject);
    self->mBlockSize = gArgDequeBlockSize;
    self->mCapacity = 0;
    self->mCount = 0;
    self->mStart = 0;
    self->mTable = 0;
    self->mFirst = 0;
    self->mSize = other->mSize;
    if (self->mSize == 0) return;
    argdeque_callee::CdequeReserve(&self->mCapacity, 2);
    argdeque_callee::CdequeResize(&self->mCapacity, 1, nullptr);
    if (self->mBlockSize < self->mSize) self->mBlockSize = self->mSize;
    uint32_t block = Word_4(IME::MslNew(self->mBlockSize * 0xc));
    Table(self)[self->mStart] = block;
    argdeque_callee::CdequePushBack(&self->mCapacity, &gArgDequeFill);
    self->mFirst = (self->mBlockSize - self->mSize) >> 1;
    uint32_t target = block + self->mFirst * 0xc;
    MslArgumentDequeIter last;
    ArgumentDequeMakeIterator(&last, other, other->mFirst + other->mSize);
    MslArgumentDequeIter it;
    ArgumentDequeMakeIterator(&it, other, other->mFirst);
    while (argdeque_callee::IterNotEqual(&it, &last)) {
        // placement new (0x10201850 answers its second argument), then the vector copy constructor
        if (target != 0) argdeque_callee::VectorCopy(Native_2(target), argdeque_callee::IterDeref(&it));
        target = target + 0xc;
        argdeque_callee::IterInc(&it);
    }
}

}  // namespace online_callee

void ArgumentDequeMakeIterator(MslArgumentDequeIter* out, const MslArgumentDeque* self, uint32_t index) {
    MslArgumentCursor cursor;
    cursor.mCdeque = Word_4(&self->mCapacity);
    cursor.mIndex = 0;
    cursor.mSlot = self->mTable + self->mStart * 4;
    uint32_t base;
    uint32_t element;
    if (index < self->mBlockSize) {
        base = self->mCount == 0 ? 0 : *static_cast<uint32_t*>(Native_2(cursor.mSlot));
        element = base + index * 0xc;
    } else {
        argdeque_callee::CursorAdd(&cursor, static_cast<int32_t>(self->mBlockSize != 0 ? index / self->mBlockSize : 0));
        base = *static_cast<uint32_t*>(Native_2(cursor.mSlot));
        uint32_t rest = index - (self->mBlockSize != 0 ? index / self->mBlockSize : 0) * self->mBlockSize;
        element = base + rest * 0xc;
    }
    out->mCursor = cursor;
    out->mElement = element;
    out->mBlockEnd = base + self->mBlockSize * 0xc;
    out->mIndex = index;
}

// Unit v51125: small leaves of the main app (see include/std_exception_what.h).

// ---- the callees outside the unit: weak defaults (the real calls) that the unit's test library replaces with recording fakes.
#define TPW_WEAK __attribute__((weak))
TPW_WEAK void stdexc_callee::OperatorDelete(void* block) { ::operator delete(block); }
TPW_WEAK void stdexc_callee::UStringDtor(SamsUtilities::UString* string, int16_t) { string->~UString(); }
TPW_WEAK void stdexc_callee::StringDtor(void* string, int16_t flag) {
    msl_char_string::destroy(static_cast<MslCharString*>(string), flag);
}
TPW_WEAK void stdexc_callee::DestroyCityInfos(ccCityInfo* infos) { delete[] infos; }
TPW_WEAK void stdexc_callee::DestroyParkInfos(ccParkInfo* infos) { delete[] infos; }
TPW_WEAK void stdexc_callee::DestroyCityRecord(CFSOnlineCity* city) { gCityRecordDeleter(city); }
TPW_WEAK void stdexc_callee::DestroyParkRecord(CFSOnlinePark* park) { delete park; }
TPW_WEAK void stdexc_callee::DestroyCityList(TbPtrList<CFSOnlineCity>* list) { delete list; }
TPW_WEAK void stdexc_callee::DestroyParkList(TbPtrList<CFSOnlinePark>* list) { delete list; }
TPW_WEAK void stdexc_callee::UshortDestroy(UshortSetTree*, uint16_t*) {}
TPW_WEAK void stdexc_callee::UshortNodeDeallocate(UshortSetTree*, UshortSetTree::node* node, unsigned long) { ::operator delete(node); }

// ---- std::exception::what 0x100025a8: the address 0x1023bba2 + 0xe6 is the text "exception".
const char* stdexc::ExceptionWhat(const void*) { return "exception"; }

// ---- std::logic_error::what 0x10002570: the counted message's pointer word (+4 on the Mac).
const char* stdexc::LogicErrorWhat(const IME::MslExceptionObject* self) { return self->mRc.mPtr; }

// ---- std::length_error::~length_error 0x100024ac. The vtable word takes three values in turn (0x10278b74, 0x10278bb0, 0x10278bc0); the
// message's counted pointer (+4) is released between the second and the third.
IME::MslExceptionObject* IME::MslLengthErrorDtor(MslExceptionObject* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->mVtbl = gMslLengthErrorVtbl;
        self->mVtbl = gMslExcVtblB;
        MslRefCountedCharsDtor25e0(&self->mRc, -1);
        self->mVtbl = gMslExcVtblC;
        if (deleteFlag > 0) stdexc_callee::OperatorDelete(self);
    }
    return self;
}

// ---- SamsUtilities::UResourceId::~UResourceId 0x10001ad0: the type string is destroyed (flag -1).
SamsUtilities::UResourceId* stdexc::UResourceIdDestruct(SamsUtilities::UResourceId* self, int16_t deleteFlag) {
    if (self != nullptr) {
        stdexc_callee::UStringDtor(&self->mType, -1);
        if (deleteFlag > 0) stdexc_callee::OperatorDelete(self);
    }
    return self;
}

// ---- CMapLinkBase::~CMapLinkBase 0x10007b04: the Mac stores the vtable word 0x10278c9c, then frees for a flag above 0.
CMapLinkBase* stdexc::CMapLinkBaseDestruct(CMapLinkBase* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (deleteFlag > 0) stdexc_callee::OperatorDelete(self);
    }
    return self;
}

// ---- TbTimeStamp::TbTimeStamp 0x1013dca8
TbTimeStamp* stdexc::TbTimeStampCtor(TbTimeStamp* self) {
    self->mTicks = 0;
    return self;
}

// ---- std::__tree<unsigned short>::destroy 0x1016e65c. The compiler inlined two levels of the recursion: a child of `subtree` is
// handled by hand (its children are freed by the recursive call, the value destroyed and the node given back through the allocator),
// then freed with operator delete; `subtree` is freed last.
namespace {
using Tree = UshortSetTree;
void DestroyGrandChild(Tree* tree, Tree::node* node) {
    if (node->mLeft != nullptr) tree->destroy(node->mLeft);
    if (node->mRight != nullptr) tree->destroy(node->mRight);
    stdexc_callee::UshortDestroy(tree, &node->mValue);
    stdexc_callee::UshortNodeDeallocate(tree, node, 1);
}
void DestroyChild_2(Tree* tree, Tree::node* child) {
    if (child->mLeft != nullptr) DestroyGrandChild(tree, child->mLeft);
    if (child->mRight != nullptr) DestroyGrandChild(tree, child->mRight);
    stdexc_callee::OperatorDelete(child);
}
}  // namespace

void UshortSetTree::destroy(node* subtree) {
    if (subtree->mLeft != nullptr) DestroyChild_2(this, subtree->mLeft);
    if (subtree->mRight != nullptr) DestroyChild_2(this, subtree->mRight);
    stdexc_callee::OperatorDelete(subtree);
}

// ---- the three list destructors 0x101701c8, 0x10170834, 0x10170ca8: the stored answer is released (its count drops, at zero the infos
// are destroyed and the answer freed), the typed list is emptied (each record destroyed through its virtual destructor), two words are
// cleared and the list freed; the object itself is freed for a flag above 0.
namespace {
struct CityTraits {
    static void DestroyInfos(ccCityInfo* infos) { stdexc_callee::DestroyCityInfos(infos); }
    static void DestroyRecord(CFSOnlineCity* record) { stdexc_callee::DestroyCityRecord(record); }
    static void DestroyList(TbPtrList<CFSOnlineCity>* list) { stdexc_callee::DestroyCityList(list); }
};
struct ParkTraits {
    static void DestroyInfos(ccParkInfo* infos) { stdexc_callee::DestroyParkInfos(infos); }
    static void DestroyRecord(CFSOnlinePark* record) { stdexc_callee::DestroyParkRecord(record); }
    static void DestroyList(TbPtrList<CFSOnlinePark>* list) { stdexc_callee::DestroyParkList(list); }
};
template<class Traits, class Self>
Self* DestructList(Self* self, int16_t deleteFlag) {
    if (self != nullptr) {
        auto* received = self->mReceived;
        if (received != nullptr) {
            received->mRefCount = received->mRefCount - 1;
            if (received->mRefCount == 0) {
                if (received->mInfos != nullptr) Traits::DestroyInfos(received->mInfos);
                received->mInfos = nullptr;
                stdexc_callee::OperatorDelete(received);
            }
            self->mReceived = nullptr;
        }
        while (self->mList->GetCount() != 0) {
            auto* record = self->mList->RemoveHead();
            if (record != nullptr) Traits::DestroyRecord(record);
        }
        self->mF24 = 0;
        self->mF28 = 0;
        if (self->mList != nullptr) Traits::DestroyList(self->mList);
        if (deleteFlag > 0) stdexc_callee::OperatorDelete(self);
    }
    return self;
}
}  // namespace

CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>* stdexc::CityListDestruct(
    CFSOnlineSystemList<TbPtrList<CFSOnlineCity>, ccCityList>* self, int16_t deleteFlag) {
    return DestructList<CityTraits>(self, deleteFlag);
}
CFSOnlineSystemParkList_InWorldByParkID* stdexc::ParkListByParkIDDestruct(CFSOnlineSystemParkList_InWorldByParkID* self, int16_t deleteFlag) {
    return DestructList<ParkTraits>(self, deleteFlag);
}
CFSOnlineSystemParkList_InCityTop10* stdexc::ParkListTop10Destruct(CFSOnlineSystemParkList_InCityTop10* self, int16_t deleteFlag) {
    return DestructList<ParkTraits>(self, deleteFlag);
}

// ---- std::list<std::string>::clear 0x1017fc04: nothing for a count of 0; else each node's string is destroyed (flag -1) and the node
// freed (its next link read before), then the list is empty.
void IME::MslStrList_clear(MslStrList* self) {
    if (self->mSize != 0) {
        MslStrNode* node = self->mHead.mNext;
        MslStrNode* end = reinterpret_cast<MslStrNode*>(&self->mHead);
        while (node != end) {
            stdexc_callee::StringDtor(&node->mStr, -1);
            MslStrNode* next = node->mNext;
            stdexc_callee::OperatorDelete(node);
            node = next;
        }
        self->mSize = 0;
        self->mHead.mPrev = end;
        self->mHead.mNext = end;
    }
}

// std::ios_base::setstate 0x102077c0 (unit v5523).

// 0x10250098: the type string __throw is given; 0x1024fde4: the message text.
static const char kSetstateType[] = "!std::exception!!std::ios_base::failure!!";
static const char kSetstateMessage[] = "ios_base failure in clear";

// Adds the bits; a missing stream buffer adds badbit (1). When a bit is also in the exception mask, throws an
// ios_base::failure built on the stack: vtable, a copy of the message, a reference count of 1.
void MslIosSetstate(MacIosBase* ios, int32_t bits) {
    ios->mState = ios->mState | static_cast<uint32_t>(bits);
    if (ios->mStreamBuf.get() == nullptr) ios->mState = ios->mState | 1;
    if ((ios->mState & ios->mExceptions) != 0) {
        MslThrowObject object;
        object.mVtbl = gIosFailureVtbl;
        object.mMessage = new char[std::strlen(kSetstateMessage) + 1];
        object.mCount = nullptr;
        if (object.mMessage != nullptr) {
            object.mCount = new int32_t;
            if (object.mCount != nullptr) *object.mCount = 1;
        }
        std::strcpy(object.mMessage, kSetstateMessage);
        MslThrow(kSetstateType, &object, gIosFailureDtorVec);
    }
}

// Unit v51058: twenty small functions of the main app: the world lobby controller's messages, the static initialisers of
// ui_worldLobbyController.cpp, ui_publishParkPanel.cpp, TPGlobals.cpp and OnlinePerson.cpp, the 2D map callback, two lobby screen
// callbacks that call a virtual slot, a few std::__cdeque / std::less helpers.

// ---- std::__cdeque helpers (the Mac words: count +0, start +8, block table +0xc; a cursor is block table, index, slot)
namespace {
void CursorAddWords(uint32_t* w, int32_t n);

void CursorAddPositive(uint32_t* w, int32_t n) {
    const uint32_t* table = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | w[0]));
    uint32_t t = table[2] + (w[1] + static_cast<uint32_t>(n));
    uint32_t count = table[0];
    if (t < count) w[2] = w[2] + (static_cast<uint32_t>(n) << 2);
    else w[2] = table[3] + ((t - count) << 2);
    w[1] = w[1] + static_cast<uint32_t>(n);
}

void CursorAddWords(uint32_t* w, int32_t n) {
    if (n >= 0) {
        CursorAddPositive(w, n);
        return;
    }
    int32_t negated = static_cast<int32_t>(0u - static_cast<uint32_t>(n));
    if (negated < 0) {   // INT_MIN: the original calls itself with the same arguments, for ever
        CursorAddWords(w, n);
        return;
    }
    const uint32_t* table = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | w[0]));
    uint32_t t = table[2] + w[1];
    uint32_t back = static_cast<uint32_t>(negated);
    if (back > t) w[2] = table[3] + ((table[0] - back) << 2);
    else w[2] = w[2] - (back << 2);
    w[1] = w[1] + static_cast<uint32_t>(n);
}
}  // namespace

// 0x101f5508 const_iterator::operator+=, declared twice (online.h and std_deque_errorthingy.h)
namespace argdeque_callee {
void CursorAdd(MslArgumentCursor* cursor, int32_t count) { CursorAddWords(reinterpret_cast<uint32_t*>(cursor), count); }
}
namespace errdeque_callee {
void CursorAdd(MslErrorDequeCursor* cursor, int32_t count) { CursorAddWords(reinterpret_cast<uint32_t*>(cursor), count); }
}

// 0x10229498 iterator::operator+= of the deque of vector<Argument>*: a negative count is operator-= with the count negated
MslErrorDequeCursor* MslArgumentVectorCursorAdd(MslErrorDequeCursor* cursor, int32_t count) {
    uint32_t* w = reinterpret_cast<uint32_t*>(cursor);
    if (count < 0) {
        errdeque_callee::CursorSub(cursor, static_cast<int32_t>(0u - static_cast<uint32_t>(count)));
        return cursor;
    }
    CursorAddPositive(w, count);
    return cursor;
}

// 0x101f4c84 iterator::operator--
namespace errdeque_callee {
void CdequeDecrement(MslErrorDequeCursor* cursor) {
    cursor->mWord4 = cursor->mWord4 - 1;
    const uint32_t* table = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | cursor->mCdeque));
    uint32_t slot = cursor->mSlot;
    if (slot == table[3]) cursor->mSlot = table[3] + (table[0] << 2);
    cursor->mSlot = cursor->mSlot - 4;
}
}  // namespace errdeque_callee

// 0x101f2004 operator[]: the slot's address
namespace {
void* CdequeIndexWords(void* cdeque, uint32_t index) {
    const uint32_t* d = static_cast<const uint32_t*>(cdeque);
    uint32_t i = index + d[2];
    if (!(i < d[0])) i -= d[0];
    return reinterpret_cast<void*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | static_cast<uint32_t>(d[3] + (i << 2))));
}
}  // namespace
namespace argdeque_callee {
void* CdequeIndex(void* cdeque, uint32_t index) { return CdequeIndexWords(cdeque, index); }
}
namespace errdeque_callee {
void* CdequeIndex(void* cdeque, uint32_t index) { return CdequeIndexWords(cdeque, index); }
}

// 0x102255a0 operator!= of two deque iterators: the element words differ
namespace argdeque_callee {
bool IterNotEqual(const MslArgumentDequeIter* a, const MslArgumentDequeIter* b) { return a->mElement != b->mElement; }
}

// ---- strings
namespace wstring_less_callee {
// 0x10225e8c operator<(wstring, wstring): compare of all of a with all of b
bool Lt(const MslWideString* a, const MslWideString* b) {
    uint32_t bSize = Size(b);
    const char16_t* bBegin = Begin(b);
    uint32_t aSize = Size(a);
    return IME::MslWideString_compare(reinterpret_cast<const IME::MslWideString*>(a), 0, aSize, bBegin, bSize) < 0;
}
}  // namespace wstring_less_callee

// 0x1021f000 map<nocase_string, BuddyInfo>::value_compare::operator()(pair, key) const: the pair's first member is at 0
bool BuddyMapValueCompare_PairKey(const void* functor, const void* pair, const void* key) {
    (void)functor;
    return NocaseWStringLess(pair, key);
}
// 0x1021eed0 the same with (key, pair)
bool BuddyMapValueCompare_KeyPair(const void* functor, const void* key, const void* pair) {
    (void)functor;
    return NocaseWStringLess(key, pair);
}

// std::list<unsigned int>::erase (0x1020919c) and clear (0x102090dc), unit v5305.

namespace std_list_uint {

namespace {
// The words are Ptr32 style: offsets from the arena base (include/Ptr32.h); the Mac's are addresses.
inline Node* N(uint32_t a) { return a != 0 ? reinterpret_cast<Node*>((uintptr_t)tpw::gArenaBase + a) : nullptr; }
inline uint32_t A(const void* p) { return tpw::ArenaOffset(p); }
}  // namespace

// operator delete (glue 0x10238218); weak so the unit test can replace it with a recording fake.
__attribute__((weak)) void OperatorDelete(void* p) { ::operator delete(p); }

void Erase(uint32_t* result, List* list, uint32_t first, uint32_t last) {
    if (first == last) {
        *result = last;
        return;
    }
    Node* l = N(last);
    Node* f = N(first);
    Node* lp = N(l->mPrev);
    N(f->mPrev)->mNext = lp->mNext;
    N(lp->mNext)->mPrev = f->mPrev;
    uint32_t cur = first;
    while (cur != last) {
        uint32_t next = N(cur)->mNext;
        OperatorDelete(N(cur));
        list->mSize = list->mSize - 1;
        cur = next;
    }
    *result = last;
}

void Clear(List* list) {
    if (list->mSize == 0) return;
    uint32_t cur = list->mNext;
    uint32_t sentinel = A(&list->mPrev);
    while (cur != sentinel) {
        uint32_t next = N(cur)->mNext;
        OperatorDelete(N(cur));
        cur = next;
    }
    list->mSize = 0;
    list->mNext = sentinel;
    list->mPrev = sentinel;
}

}  // namespace std_list_uint

// std::list<unsigned int, std::allocator<unsigned int>> of the Metrowerks library: insert 0x102092f8, merge 0x102095a0, remove 0x10208f44,
// splice 0x1020a9a0 (unit v5931). Iterators are node addresses (32 bit words in the arena); the sentinel is the list + 4.

namespace std_list_uint {

namespace {

const char kLengthErrorType[] = "!std::exception!!std::logic_error!!std::length_error!!";   // 0x102500f9

// The words of a list are Ptr32 offsets into the arena (include/platform/arena.h); offset 0 is NULL.
Node* N_2(uint32_t a) { return reinterpret_cast<Node*>(tpw::ArenaBase() + a); }
uint32_t A_4(const void* p) { return p == nullptr ? 0 : tpw::ArenaOffset(p); }

// The std::length_error the library builds on the stack and throws (the fake of __throw returns, so the caller goes on).
void ThrowLengthError(const char* message) {
    IME::MslExceptionObject object;
    object.mVtbl = IME::gMslExcVtblB;
    object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(message)) + 1));
    object.mRc.mCount = nullptr;
    if (object.mRc.mPtr != nullptr) {
        int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
        if (count != nullptr) *count = 1;
        object.mRc.mCount = count;
    }
    IME::MslStrcpy(object.mRc.mPtr, message);
    object.mVtbl = IME::gMslLengthErrorVtbl;
    IME::MslThrow(kLengthErrorType, &object, IME::gMslLengthErrorDtorTV);
}

}  // namespace

// 0x102092f8 (428): r3 = where the iterator is stored, r4 = the list, r5 = the position, r6 = the value.
void Insert(uint32_t* result, List* list, uint32_t pos, const uint32_t* value) {
    if (list->mSize > 0x7ffffffeu) ThrowLengthError("list::insert length error");
    Node* node = static_cast<Node*>(IME::MslNew(12));
    if (node != nullptr) node->mData = *value;
    N_2(N_2(pos)->mPrev)->mNext = A_4(node);
    node->mPrev = N_2(pos)->mPrev;
    N_2(pos)->mPrev = A_4(node);
    node->mNext = pos;
    list->mSize = list->mSize + 1;
    *result = A_4(node);
}

// 0x1020a9a0 (432)
void Splice(List* list, uint32_t pos, List* other) {
    if (other->mSize == 0) return;
    if (list->mSize > 0x7fffffffu - other->mSize) ThrowLengthError("list::splice length error");
    uint32_t sentinel = A_4(other) + 4;
    uint32_t first = other->mNext;
    uint32_t last = other->mPrev;
    other->mNext = sentinel;
    other->mPrev = sentinel;
    list->mSize = list->mSize + other->mSize;
    other->mSize = 0;
    N_2(N_2(pos)->mPrev)->mNext = first;
    N_2(first)->mPrev = N_2(pos)->mPrev;
    N_2(pos)->mPrev = last;
    N_2(last)->mNext = pos;
}

// 0x102095a0 (652)
void Merge(List* list, List* other) {
    if (list == other) return;
    if (list->mSize > 0x7fffffffu - other->mSize) ThrowLengthError("list::merge length error");
    uint32_t endA = A_4(list) + 4;
    uint32_t endB = A_4(other) + 4;
    uint32_t a = list->mNext;
    uint32_t b = other->mNext;
    while (a != endA && b != endB) {
        uint32_t value = N_2(a)->mData;
        if (N_2(b)->mData < value) {
            uint32_t count = 1;
            uint32_t p = N_2(b)->mNext;
            while (p != endB && N_2(p)->mData < value) {
                count++;
                p = N_2(p)->mNext;
            }
            uint32_t lastNode = N_2(p)->mPrev;
            N_2(N_2(b)->mPrev)->mNext = N_2(lastNode)->mNext;
            N_2(N_2(lastNode)->mNext)->mPrev = N_2(b)->mPrev;
            N_2(N_2(a)->mPrev)->mNext = b;
            N_2(b)->mPrev = N_2(a)->mPrev;
            N_2(a)->mPrev = lastNode;
            N_2(lastNode)->mNext = a;
            other->mSize = other->mSize - count;
            list->mSize = list->mSize + count;
            b = p;
        }
        a = N_2(a)->mNext;
    }
    if (b != endB) Splice(list, a, other);
}

// 0x10208f44 (336)
void Remove(List* list, const uint32_t* value) {
    uint32_t sentinel = A_4(list) + 4;
    uint32_t p = list->mNext;
    while (p != sentinel) {
        if (*value == N_2(p)->mData) {
            uint32_t q = N_2(p)->mNext;
            while (q != sentinel && *value == N_2(q)->mData) q = N_2(q)->mNext;
            if (p != q) {
                uint32_t before = N_2(q)->mPrev;
                N_2(N_2(p)->mPrev)->mNext = N_2(before)->mNext;
                N_2(N_2(before)->mNext)->mPrev = N_2(p)->mPrev;
                uint32_t d = p;
                while (d != q) {
                    uint32_t next = N_2(d)->mNext;
                    IME::MslDelete(N_2(d));
                    list->mSize = list->mSize - 1;
                    d = next;
                }
            }
            p = q;
            if (p == sentinel) return;
        }
        p = N_2(p)->mNext;
    }
}

}  // namespace std_list_uint

// std::list<std::string>::unique (0x1020833c, 212 bytes), MSL, main app. The list is kept as Mac words at Mac offsets:
// +0 size, +4 the sentinel (prev +4, next +8), a node is prev +0, next +4, string +8 (one word: the rep pointer; the rep holds
// the length at +0 and the text pointer at +0xc). Every pointer word is a Ptr32 (an offset from the arena base).

namespace {
inline uint32_t& W_5(uint32_t off) { return *reinterpret_cast<uint32_t*>(tpw::gArenaBase + off); }
inline void* P_4(uint32_t off) { return reinterpret_cast<void*>(tpw::gArenaBase + off); }
}

// Removes every element equal to the one before it (the original compares each element with the next one; a run of equal
// ones is erased from the second to the last of the run).
void StringListUnique(void* self) {
    uint32_t list = static_cast<uint32_t>(tpw::ArenaOffset(self));
    uint32_t end = list + 4;
    uint32_t a = W_5(list + 8);
    while (a != end) {
        uint32_t b = W_5(a + 4);
        while (b != end) {
            uint32_t rep1 = W_5(a + 8);
            uint32_t rep2 = W_5(b + 8);
            if (msl_char_string::compare(static_cast<const MslCharString*>(P_4(a + 8)), 0, W_5(rep1),
                                         static_cast<const char*>(P_4(W_5(rep2 + 12))), W_5(rep2)) != 0)
                break;
            b = W_5(b + 4);
        }
        a = W_5(a + 4);
        if (a != b) {
            email_callee::ListErase(self, a, P_4(b));
            a = b;   // the erase answers its last iterator
        }
    }
}

// std::list<unsigned int>::unique 0x10208dcc and ::sort 0x10208c04 of the Mac (unit v5683). Words are 32 bit arena addresses.

namespace std_list_uint {

namespace {

// The words are offsets from the arena base (the Mac's 32 bit addresses).
Node* N_3(uint32_t w) { return reinterpret_cast<Node*>(tpw::ArenaBase() + w); }
uint32_t A_5(const void* p) { return tpw::ArenaOffset(p); }

}  // namespace

void Unique(List* list) {
    uint32_t sentinel = A_5(&list->mPrev);
    uint32_t cur = list->mNext;
    while (cur != sentinel) {
        uint32_t first = N_3(cur)->mNext;
        uint32_t f = first;
        while (f != sentinel && N_3(cur)->mData == N_3(f)->mData) f = N_3(f)->mNext;
        cur = first;
        if (first == f) continue;
        uint32_t lastDup = N_3(f)->mPrev;
        N_3(N_3(first)->mPrev)->mNext = N_3(lastDup)->mNext;
        N_3(N_3(lastDup)->mNext)->mPrev = N_3(first)->mPrev;
        uint32_t p = first;
        while (p != f) {
            uint32_t next = N_3(p)->mNext;
            OperatorDelete(N_3(p));
            list->mSize = list->mSize - 1;
            p = next;
        }
        cur = f;
    }
}

void Sort(List* list) {
    uint32_t size = list->mSize;
    if (size == 2) {
        Node* a = N_3(list->mNext);
        Node* b = N_3(a->mNext);
        if (b->mData < a->mData) {
            uint32_t t = a->mPrev;
            a->mPrev = a->mNext;
            a->mNext = t;
            t = b->mPrev;
            b->mPrev = b->mNext;
            b->mNext = t;
            t = list->mPrev;
            list->mPrev = list->mNext;
            list->mNext = t;
        }
    } else if (static_cast<int32_t>(size) > 2 || static_cast<int32_t>(size) < 0) {
        uint32_t half = size >> 1;
        uint32_t mid = list->mNext;
        for (uint32_t i = half; i != 0; --i) mid = N_3(mid)->mNext;
        // the local list lives in the arena: the host stack is not 32 bit addressable
        List* loc = static_cast<List*>(tpw::ArenaAllocZeroed(sizeof(List)));
        uint32_t s = A_5(&loc->mPrev);
        loc->mNext = s;
        loc->mPrev = s;
        uint32_t last = list->mPrev;
        N_3(N_3(mid)->mPrev)->mNext = N_3(last)->mNext;
        N_3(N_3(last)->mNext)->mPrev = N_3(mid)->mPrev;
        loc->mNext = mid;
        N_3(mid)->mPrev = loc->mPrev;
        loc->mPrev = last;
        N_3(last)->mNext = s;
        loc->mSize = list->mSize - half;
        list->mSize = half;
        Sort(list);
        Sort(loc);
        Merge(list, loc);
        if (loc->mSize != 0) {
            uint32_t p = loc->mNext;
            while (p != s) {
                uint32_t next = N_3(p)->mNext;
                OperatorDelete(N_3(p));
                p = next;
            }
        }
        tpw::ArenaFree(loc);
    }
}

}  // namespace std_list_uint

// std::operator<<(basic_ostream<wchar_t>&, const wchar_t*) 0x1021f374 (unit v5640).
// The sentry (tie flush, good check), __putnumber of the text with the stream's fill, the failure state and the ios_base::failure
// thrown by the inlined clear(), then the unitbuf flush. The catch landing pads and the inlined setstate block are unreachable here.

namespace wc = wostream_callee;
using ignore_callee::IgnoreIos;
using ignore_callee::IgnoreStream;

namespace {
// The failure object the code builds on its stack: vtable, message (counted buffer), counter.
struct FailureObject {
    const void* mVtbl;
    char* mMessage;
    int32_t* mCount;
};
uint32_t FlagsOf(IgnoreIos* ios) {   // Mac 0x30 (host 0x34)
    uint32_t value;
    std::memcpy(&value, &ios->mPad28[8], 4);
    return value;
}
}  // namespace

void ignore_callee::OstreamWrite(IgnoreStream* stream, const char16_t* text) {
    bool sentry = false;
    IgnoreIos* ios = stream->mIos;
    if (!wc::Good(ios)) {
        wc::Setstate(ios, 4);
    } else {
        if (wc::Tie(ios) != nullptr) wc::Flush(wc::Tie(ios));
        if (wc::Good(ios)) sentry = true;
        else wc::Setstate(ios, 4);
    }
    if (sentry) {
        ios = stream->mIos;
        void* iter = reinterpret_cast<void*>(static_cast<uintptr_t>(ios->mStreamBuf));
        int32_t length = static_cast<int32_t>(wc::Wcslen(text));
        uint16_t fill;
        std::memcpy(&fill, &ios->mPad3c[4], 2);   // Mac 0x40
        void* result = wc::PutNumber(iter, ios, static_cast<char16_t>(fill), nullptr, 0, text, length);
        if (result == nullptr) {
            ios = stream->mIos;
            ios->mState |= 5;
            if (ios->mStreamBuf == 0) ios->mState |= 1;
            if ((ios->mState & ios->mExceptions) != 0) {
                FailureObject object;
                object.mVtbl = gIosFailureVtbl;
                object.mMessage = wc::NewBytes(wc::Strlen(kMessage) + 1);
                object.mCount = nullptr;
                if (object.mMessage != nullptr) {
                    object.mCount = wc::NewInt();
                    if (object.mCount != nullptr) *object.mCount = 1;
                }
                wc::CopyString(object.mMessage, kMessage);
                wc::Throw(kType, &object, gIosFailureDtorVec);
            }
        }
    }
    ios = stream->mIos;
    if ((ios->mState & 5) == 0 && (FlagsOf(ios) & 0x2000) != 0) wc::Flush(stream);
}

// Unit v51054: small MSL helpers of the locale / streambuf / filebuf / ios_base::failure group (main app).
// Each function is a few instructions; the callees outside the unit are declared in include/std_basic_filebuf.h.

namespace v51054 {

void* UseFacet_CtypeWchar(const UseFacetLocale* locale) {         // 0x1021eb10
    return ::UseFacetCtypeWchar(locale);
}
void* UseFacet_CodecvtWcharChar(const UseFacetLocale* locale) {   // 0x10225cb8
    return ::UseFacetCodecvtWcharChar(locale);
}

}  // namespace v51054

// std::vector<std::locale::facet*>::~vector 0x100724d0 (104): frees the buffer (word at +8) when set, then the vector itself when the
// signed 16 bit delete flag is positive. Answers this. A NULL this does nothing.

namespace locale_imp_callee {
void* VectorFacetDtor(void* vec, int16_t flag) {
    if (vec != nullptr) {
        void* buffer;
        std::memcpy(&buffer, static_cast<uint8_t*>(vec) + 8, sizeof buffer);   // Mac word at +8 (native pointer here)
        if (buffer != nullptr) Delete(buffer);
        if (flag > 0) Delete(vec);
    }
    return vec;
}
}  // namespace locale_imp_callee

// std::vector<Argument> copy constructor (0x10227200) and push_back (0x10201478). An Argument is three words (12 bytes).
// The exception paths (landing pads that destroy and rethrow) are unreachable here: the fakes of __throw return.

using namespace std_vector_callee;

namespace {
const char kLengthErrorText[] = "vector::push_back length error";                       // 0x1024f8b2
inline Argument* At_4(Argument* base, uint32_t index) {
    return reinterpret_cast<Argument*>(reinterpret_cast<char*>(base) + static_cast<int32_t>(index * 12u));   // mulli wraps at 32 bits
}
}  // namespace

ArgumentVector::ArgumentVector(const ArgumentVector& other) {
    mCapacity = other.mSize;
    mSize = other.mSize;
    mData = nullptr;
    if (mCapacity != 0) {
        mData = static_cast<Argument*>(IME::MslNew(mCapacity * 12u));
        Argument* dst = mData;
        Argument* src = other.mData;
        Argument* end = At_4(other.mData, other.mSize);
        for (; src != end; src = At_4(src, 1)) {
            if (dst) ArgumentCopy(dst, src);   // placement new: the address itself
            dst = At_4(dst, 1);
        }
    }
}

void ArgumentVector::push_back(const Argument& argument) {
    if (mSize > 0x15555554u) {
        IME::MslExceptionObject exc;
        exc.mVtbl = IME::gMslExcVtblC;
        exc.mVtbl = IME::gMslExcVtblB;
        char* buffer = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(kLengthErrorText)) + 1));
        IME::MslRefCountedCharsCtor(&exc.mRc, buffer);
        IME::MslStrcpy(exc.mRc.mPtr, kLengthErrorText);
        exc.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow(kLengthErrorType, &exc, IME::gMslLengthErrorDtorTV);
    }
    if (mSize < mCapacity) {
        Argument* slot = At_4(mData, mSize);
        if (slot) ArgumentCopy(slot, &argument);
        mSize = mSize + 1;
        return;
    }
    Argument* old = mData;
    uint32_t newCapacity = mCapacity == 0 ? 1u : mCapacity << 1;
    mData = static_cast<Argument*>(IME::MslNew(newCapacity * 12u));
    uint32_t count = mSize;
    Argument* dst = mData;
    Argument* src = old;
    Argument* end = At_4(old, count);
    for (; src != end; src = At_4(src, 1)) {
        if (dst) ArgumentCopy(dst, src);
        dst = At_4(dst, 1);
    }
    Argument* slot = At_4(mData, mSize);
    if (slot) ArgumentCopy(slot, &argument);
    if (old) {
        ArgumentDestroyRange(old, At_4(old, mSize));
        IME::MslDelete(old);
    }
    mSize = mSize + 1;
    mCapacity = newCapacity;
}

// Unit v51129: std::basic_string<wchar_t>::assign (0x101b2510), the horizontal gradient's destructor (0x10189c0c), the ride list's GetList
// (0x1017b938), the server list's base destructor (0x1017a6e4), the theme list's LoggedInGetList (0x10179f50) and destructor (0x10179d28).

// 0x101b2510 (108): make room for `length` characters (change_size, not keeping the old ones), then copy the text into the string's
// representation, which change_size may have replaced (the pointer is read again). Answers this on the Mac.
void LCString::assign(const char16_t* text, uint32_t length) {
    IME::MslWideString_changeSize(reinterpret_cast<IME::MslWideString*>(this), length, false);
    wmemcpy(mRep->mData, text, length);
}

// std::list<std::string> members of the Metrowerks library (main app): insert 0x101b25e0, sort 0x1020811c, erase 0x102084d8 and
// 0x10208838, merge 0x10209898. Mac offsets are in include/ui_ime.h; the pointers here are native.

namespace IME {
namespace {

const char kLogicLengthType[] = "!std::exception!!std::logic_error!!std::length_error!!";   // 0x1024e382 / 0x102500f9
const char kInsertMessage[] = "list::insert length error";                                   // 0x1024e242
const char kMergeMessage[] = "list::merge length error";                                     // 0x1024fed9

MslStrNode* End(MslStrList* list) { return reinterpret_cast<MslStrNode*>(&list->mHead); }

// The compiler's inlined string destructor: the count (0 means not shared yet) goes down; at 0 the data and the representation are
// deleted, else the node forgets the representation. The Mac's test of the string's address against 0 (node +8) is always true.
void ReleaseString(MslStrNode* node) {
    MslStrRep* rep = node->mStr.mRep;
    if (rep->mRefs == 0) {
        uint32_t v = rep->mRefs;
        if (v == 0) v = 1;
        rep->mRefs = v;
    }
    uint32_t count = rep->mRefs - 1;
    rep->mRefs = count;
    if (count != 0) rep = nullptr;
    node->mStr.mRep = rep;
    if (node->mStr.mRep != nullptr) {
        MslDelete(node->mStr.mRep->mData);
        MslDelete(node->mStr.mRep);
    }
}

}  // namespace

// 0x101b25e0: a copy of *str goes before pos. A list at 0x7fffffff nodes throws length_error (the fakes return, so the
// original's code goes on to insert).
MslStrNode* MslStrList_insert(MslStrList* self, MslStrNode* pos, const MslString* str) {
    if (self->mSize > 0x7ffffffeu) {
        MslExceptionObject obj;
        size_t len = MslStrlen(kInsertMessage);
        char* text = static_cast<char*>(MslNewArray(static_cast<uint32_t>(len + 1)));
        MslRefCountedCharsCtor(&obj.mRc, text);
        MslStrcpy(obj.mRc.mPtr, kInsertMessage);
        obj.mVtbl = gMslLengthErrorVtbl;
        MslThrow(kLogicLengthType, &obj, gMslLengthErrorDtorTV);
    }
    MslStrNode* node = static_cast<MslStrNode*>(MslNew(sizeof(uint32_t) * 3));   // 12 bytes on the Mac
    MslString_construct(&node->mStr, str, 0, 0xffffffffu);
    pos->mPrev->mNext = node;
    node->mPrev = pos->mPrev;
    pos->mPrev = node;
    node->mNext = pos;
    self->mSize = self->mSize + 1;
    return node;
}

// 0x1020811c: merge sort. Two nodes are swapped by exchanging their links; more are cut in half, both halves sorted, merged.
void MslStrList_sort(MslStrList* self) {
    int32_t size = static_cast<int32_t>(self->mSize);
    if (size == 2) {
        MslStrNode* first = self->mHead.mNext;
        MslStrNode* second = first->mNext;
        if (MslString_less(&second->mStr, &first->mStr)) {
            MslStrNode* t = first->mPrev;
            first->mPrev = first->mNext;
            first->mNext = t;
            t = second->mPrev;
            second->mPrev = second->mNext;
            second->mNext = t;
            t = self->mHead.mPrev;
            self->mHead.mPrev = self->mHead.mNext;
            self->mHead.mNext = t;
        }
    } else if (size > 2 || size < 0) {
        uint32_t half = self->mSize >> 1;
        MslStrNode* mid = self->mHead.mNext;
        for (uint32_t k = half; k != 0; --k) mid = mid->mNext;
        // the second half moves to a list of its own on the stack
        MslStrList other;
        other.mHead.mNext = End(&other);
        other.mHead.mPrev = End(&other);
        MslStrNode* last = self->mHead.mPrev;
        mid->mPrev->mNext = last->mNext;
        last->mNext->mPrev = mid->mPrev;
        reinterpret_cast<MslStrNode*>(other.mHead.mPrev)->mNext = mid;
        mid->mPrev = other.mHead.mPrev;
        other.mHead.mPrev = last;
        last->mNext = End(&other);
        other.mSize = self->mSize - half;
        self->mSize = half;
        MslStrList_sort(self);
        MslStrList_sort(&other);
        MslStrList_merge(self, &other);
        MslStrList_clear(&other);
    }
}

// 0x102084d8: removes [first, last), deleting each node and its string; answers last.
MslStrNode* MslStrList_erase(MslStrList* self, MslStrNode* first, MslStrNode* last) {
    if (first == last) return last;
    MslStrNode* lastPrev = last->mPrev;
    first->mPrev->mNext = lastPrev->mNext;
    lastPrev->mNext->mPrev = first->mPrev;
    MslStrNode* cur = first;
    while (cur != last) {
        ReleaseString(cur);
        MslStrNode* next = cur->mNext;
        MslDelete(cur);
        self->mSize = self->mSize - 1;
        cur = next;
    }
    return last;
}

// 0x10208838: removes one node, answers the next.
MslStrNode* MslStrList_erase1(MslStrList* self, MslStrNode* pos) {
    MslStrNode* next = pos->mNext;
    pos->mPrev->mNext = next;
    pos->mNext->mPrev = pos->mPrev;
    ReleaseString(pos);
    MslDelete(pos);
    self->mSize = self->mSize - 1;
    return next;
}

// 0x10209898: merges the sorted list other into the sorted list self, moving runs of nodes; the rest of other goes to the end.
void MslStrList_merge(MslStrList* self, MslStrList* other) {
    if (self == other) return;
    if (self->mSize > 0x7fffffffu - other->mSize) {
        MslExceptionObject obj;
        size_t len = MslStrlen(kMergeMessage);
        obj.mRc.mPtr = static_cast<char*>(MslNewArray(static_cast<uint32_t>(len + 1)));
        obj.mRc.mCount = nullptr;
        if (obj.mRc.mPtr != nullptr) {
            int32_t* count = static_cast<int32_t*>(MslNew(4));
            if (count != nullptr) *count = 1;
            obj.mRc.mCount = count;
        }
        MslStrcpy(obj.mRc.mPtr, kMergeMessage);
        obj.mVtbl = gMslLengthErrorVtbl;
        MslThrow(kLogicLengthType, &obj, gMslLengthErrorDtorTV);
    }
    MslStrNode* end1 = End(self);
    MslStrNode* end2 = End(other);
    MslStrNode* it1 = self->mHead.mNext;
    MslStrNode* it2 = other->mHead.mNext;
    while (it1 != end1 && it2 != end2) {
        if (MslString_less(&it2->mStr, &it1->mStr)) {
            uint32_t count = 1;
            MslStrNode* scan = it2->mNext;
            while (scan != end2 && MslString_less(&scan->mStr, &it1->mStr)) {
                ++count;
                scan = scan->mNext;
            }
            MslStrNode* lastRun = scan->mPrev;
            it2->mPrev->mNext = lastRun->mNext;
            lastRun->mNext->mPrev = it2->mPrev;
            it1->mPrev->mNext = it2;
            it2->mPrev = it1->mPrev;
            it1->mPrev = lastRun;
            lastRun->mNext = it1;
            other->mSize = other->mSize - count;
            self->mSize = self->mSize + count;
            it2 = scan;
        }
        it1 = it1->mNext;
    }
    if (it2 != end2) MslStrList_splice(self, it1, other);
}

}  // namespace IME

// std::basic_string<wchar_t> of the Metrowerks library: replace (0x101f7740), substr (0x1021212c), begin (0x10212448).
// The string object holds one pointer to a shared representation (see MslWideStringRep in ui_ime.h).

namespace IME {
namespace {

MslWideStringRep* RepOf_3(const MslWideString* s) { return reinterpret_cast<MslWideStringRep*>(s->mData); }
void SetRep(MslWideString* s, MslWideStringRep* r) { s->mData = reinterpret_cast<char16_t*>(r); }

const char kOutOfRangeName[] = "!std::exception!!std::logic_error!!std::out_of_range!!";
const char kLengthErrorName[] = "!std::exception!!std::logic_error!!std::length_error!!";

// The inlined exception construction: the message copied into a counted buffer, then __throw (the fakes return).
// inlineCtor: replace builds the counted pointer by hand; substr calls _RefCountedPtr's constructor (0x100026ac).
void ThrowMsg(const char* typeName, const char* msg, const void* vtbl, const void* dtorTV, bool inlineCtor) {
    MslExceptionObject exc;
    exc.mVtbl = vtbl;
    size_t n = MslStrlen(msg);
    char* p = static_cast<char*>(MslNewArray(static_cast<uint32_t>(n + 1)));
    if (inlineCtor) {
        exc.mRc.mPtr = p;
        exc.mRc.mCount = nullptr;
        if (p != nullptr) {
            exc.mRc.mCount = static_cast<int32_t*>(MslNew(4));
            if (exc.mRc.mCount != nullptr) {
                *exc.mRc.mCount = 1;
            }
        }
    } else {
        MslRefCountedCharsCtor(&exc.mRc, p);
    }
    MslStrcpy(exc.mRc.mPtr, msg);
    MslThrow(typeName, &exc, dtorTV);
}

// The inlined representation constructor: capacity rounded to 3 mod 4, count 1, a data block of capacity + 1 characters.
MslWideStringRep* NewRep(uint32_t len) {
    MslWideStringRep* r = static_cast<MslWideStringRep*>(MslNewRep());
    if (r != nullptr) {
        r->mCapacity = (len + 3) - (len & 3);
        r->mRefs = 1;
        r->mData = static_cast<char16_t*>(MslNew((r->mCapacity + 1) * 2));
    }
    return r;
}

}  // namespace

// 0x101f7740. Returns this on the Mac (the declaration above says void).
void MslWideString_replace(MslWideString* self, uint32_t pos, uint32_t n1, uint32_t n2, char16_t c) {
    if (pos > RepOf_3(self)->mLength) {
        ThrowMsg(kOutOfRangeName, "basic_string::replace pos out of range", gMslExcVtblA, gMslOutOfRangeDtorTV, true);
    }
    uint32_t avail = RepOf_3(self)->mLength - pos;
    uint32_t eff = avail < n1 ? avail : n1;
    if (n2 > 0x7ffffffeu || RepOf_3(self)->mLength - eff > 0x7ffffffeu - n2) {
        ThrowMsg(kLengthErrorName, "basic_string::replace length error", gMslLengthErrorVtbl, gMslLengthErrorDtorTV, true);
    }
    MslWideStringRep* rep = RepOf_3(self);
    uint32_t len = rep->mLength;
    uint32_t newLen = n2 + (len - eff);
    if (rep->mRefs > 1 || newLen > rep->mCapacity) {
        MslWideStringRep* nr = NewRep(newLen);
        MslWmemcpy(nr->mData, RepOf_3(self)->mData, pos);
        if (n2 != 0) {
            MslWmemset(nr->mData + pos, c, n2);
        }
        rep = RepOf_3(self);
        MslWmemcpy(nr->mData + static_cast<uint32_t>(pos + n2), rep->mData + static_cast<uint32_t>(pos + eff),
                   rep->mLength - static_cast<uint32_t>(pos + eff));
        nr->mData[newLen] = gMslWcharNulA;
        nr->mLength = newLen;
        // Let go of the old representation: a count of 0 means unshared (one owner), then one owner less.
        rep = RepOf_3(self);
        if (rep->mRefs == 0) {
            rep->mRefs = 1;
        }
        rep->mRefs = rep->mRefs - 1;
        if (rep->mRefs != 0) {
            rep = nullptr;
        }
        SetRep(self, rep);
        if (RepOf_3(self) != nullptr) {
            MslDelete(RepOf_3(self)->mData);
            MslDelete(RepOf_3(self));
        }
        SetRep(self, nr);
    } else {
        uint32_t tail = len - static_cast<uint32_t>(pos + eff);
        MslWmemmove(rep->mData + static_cast<uint32_t>(pos + n2), rep->mData + static_cast<uint32_t>(pos + eff), tail);
        if (n2 != 0) {
            MslWmemset(RepOf_3(self)->mData + pos, c, n2);
        }
        RepOf_3(self)->mData[newLen] = gMslWcharNulB;
        RepOf_3(self)->mLength = newLen;
    }
}

// 0x1021212c. The result is built by the substring constructor.
MslWideString* MslWideString_substr(MslWideString* result, const MslWideString* self, uint32_t pos, uint32_t n) {
    if (pos > RepOf_3(self)->mLength) {
        ThrowMsg(kOutOfRangeName, "basic_string::substr pos out of range", gMslExcVtblA, gMslOutOfRangeDtorTV, false);
    }
    uint32_t avail = RepOf_3(self)->mLength - pos;
    uint32_t eff = avail < n ? avail : n;
    return MslWideString_construct(result, self, pos, eff);
}

// 0x10212448. Makes the string unshared (a private copy when shared), sets its count to 0 and answers the characters.
char16_t* MslWideString_begin(MslWideString* self) {
    if (RepOf_3(self)->mRefs > 1) {
        uint32_t len = RepOf_3(self)->mLength;
        MslWideStringRep* rep = RepOf_3(self);
        if (rep->mRefs > 1) {
            MslWideStringRep* nr = NewRep(len);
            rep->mRefs = rep->mRefs - 1;
            uint32_t cnt = rep->mLength < len ? rep->mLength : len;
            MslWmemcpy(nr->mData, rep->mData, cnt + 1);
            nr->mLength = rep->mLength;
            rep = nr;
        }
        SetRep(self, rep);
    }
    RepOf_3(self)->mRefs = 0;
    return RepOf_3(self)->mData;
}

}  // namespace IME
