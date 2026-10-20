// ui_IME.cpp (main app): the edit box's IME state. Mac offsets below; native pointers on the host.
// Unit w92 adds the lifetime of the classes: IME::CIME (TPEmbed.cpp: constructor 0x101fe934, virtual destructor 0x101fe9ec), the
// MSL vector tear_downs 0x101feb58 and 0x101f6ecc, IME::list_cand_data_t::~list_cand_data_t 0x101f6bec (ui_IME.cpp) and
// CUIIME::~CUIIME 0x101f7d18 (ui_iterators.cpp). Sources: src/TPEmbed_ime.cpp and src/ui_ime_lifetime.cpp.
#pragma once
#include <cstdint>

#include "TbTextFormatter.h"   // MslVector

class InterfaceWindow;
class InterfaceEditBox;
struct GrafPort;
namespace IME {
struct ActivationResult {
    int32_t mWords[4];
};

// std::basic_string<wchar_t> of the Metrowerks library: one pointer to the shared representation on the Mac (4 bytes). Both
// members are stand-ins on purpose (the string library is not translated): the constructor is 0x101e0b84 (it gets the stack
// allocator in r4) and the destructor 0x101468f8 (the deleting flag in r4 is -1: never frees the string itself).
struct MslWideString {
    char16_t* mData;   // +0x00 (a native pointer here)
    MslWideString();
    ~MslWideString();
};

// One candidate list of the IME, Mac 0x20 bytes. Nothing in this unit touches +0x00..+0x0f; the destructor lets go of the strings.
struct list_cand_data_t {
    uint8_t mF00[0x10];                 // +0x00  not read or written by the lifetime code
    MslVector<MslWideString> mStrings;  // +0x10  capacity, size, data (Mac +0x10, +0x14, +0x18)
    ~list_cand_data_t();                // 0x101f6bec (src/ui_ime_lifetime.cpp)
};
}
// MslVector<IME::MslWideString> (the vector of strings) and MslVector<IME::list_cand_data_t> (the vector of candidate lists).
template <> void MslVector<IME::MslWideString>::tear_down();       // 0x101f6ecc (src/ui_ime_lifetime.cpp): ~string of each, then the block
template <> void MslVector<IME::list_cand_data_t>::tear_down();    // 0x101feb58 (src/TPEmbed_ime.cpp): each list's strings, then the block

namespace IME {
// The input method editor of the build, Mac 0x138 bytes at CUIIME +0x1c. Polymorphic with one virtual, the destructor
// (vtable 0x102b15b0). The constructor leaves +0x04, +0x30 and +0x38..+0x137 as they were.
class CIME {
public:
    CIME();                    // 0x101fe934 (src/TPEmbed_ime.cpp)
    virtual ~CIME();           // 0x101fe9ec: the candidate vector first, then the strings +0x10, +0x0c, +0x08
    ActivationResult Activate(GrafPort* window); // 0x101feab0: Mac blr stub (src/TPEmbed_ime.cpp: the host answers a zeroed record)
    void UnActivate();                         // 0x101feae8: Mac blr stub (src/TPEmbed_ime.cpp)
    void SetMaxCompositionLength(int32_t length); // 0x101feb18: Mac blr stub (src/TPEmbed_ime.cpp)

    uint32_t mF04;                          // +0x04  never written by the lifetime code
    MslWideString mString08;                // +0x08
    MslWideString mString0C;                // +0x0c
    MslWideString mString10;                // +0x10
    MslVector<list_cand_data_t> mCandidates;   // +0x14  capacity, size, data (Mac +0x14, +0x18, +0x1c)
    uint8_t mF20;                           // +0x20  set to 0
    uint32_t mF24;                          // +0x24  set to 0
    uint32_t mF28;                          // +0x28  set to 0
    int32_t mF2C;                           // +0x2c  set to -1
    uint32_t mF30;                          // +0x30  never written by the lifetime code
    uint32_t mF34;                          // +0x34  set to 0
    uint8_t mF38[0x100];                    // +0x38..+0x137  never written by the lifetime code
};
}
class CUIIME {
public:
    InterfaceWindow* mOwner;     // 0x00
    InterfaceEditBox* mEditBox;   // 0x04
    uint8_t mF08[4];
    uint8_t mActive;             // 0x0c
    uint8_t mF0D[0x1c - 0x0d];
    IME::CIME mIME;              // 0x1c
    IME::list_cand_data_t mCandidates;   // 0x154
    uint8_t mF174[8];            // 0x174..0x17b  not used by the lifetime code
    int16_t mStoredLength;       // 0x17c
    int16_t mStoredInvalid;      // 0x17e
    ~CUIIME();                   // 0x101f7d18 (src/ui_ime_lifetime.cpp): the candidate list, then the IME
    void Activate(InterfaceWindow* window); // 0x101f6c84
    void DeActivate();                      // 0x101f6d68
    void InvalidateStoredString(InterfaceWindow* window); // 0x101f6e0c
};
extern CUIIME gUIIME; // 0x103966c0: an object, not a pointer

// Unit v5008: std::basic_string<wchar_t> compare / assign / substring constructor (src/UI_Base_std_basic_string_compare.cpp).
// The object IME::MslWideString holds one pointer to a shared representation. Representation, Mac offsets:
//   +0x00 length, +0x04 capacity, +0x08 reference count (0 = not shared yet), +0x0c data pointer (native here: +0x10).
// Written as free functions taking the string object (the stand-in class above is not touched).
#include <cstddef>
namespace IME {
struct MslWideStringRep {
    uint32_t mLength;      // +0x00
    uint32_t mCapacity;    // +0x04
    uint32_t mRefs;        // +0x08
    char16_t* mData;       // +0x0c (native pointer here)
};
struct MslRefCountedChars {   // std::_RefCountedPtr<char, std::_Array<char>>: the pointer, then the counter
    char* mPtr;                // +0x00
    int32_t* mCount;           // +0x04
};
struct MslExceptionObject {   // the stack object the library builds to throw: vtable word, then the message
    const void* mVtbl;         // +0x00
    MslRefCountedChars mRc;    // +0x04
};
// Globals of the library nobody names (TOC slots -0x7684, -0x7680, -0x767c, -0x7688); stand-in storage here.
extern char gMslExcVtblA[16];      // 0x10278ba0
extern char gMslExcVtblB[16];      // 0x10278bb0
extern char gMslExcVtblC[16];      // 0x10278bc0
extern char gMslOutOfRangeDtorTV[8]; // 0x10265698: TVector of std::out_of_range::~out_of_range

int32_t MslWideString_compare(const MslWideString* self, uint32_t pos, uint32_t n1, const char16_t* s, uint32_t n2); // 0x101d5864
MslWideString* MslWideString_assign(MslWideString* self, const MslWideString* str, uint32_t pos, uint32_t n);       // 0x101f7014
MslWideString* MslWideString_construct(MslWideString* self, const MslWideString* str, uint32_t pos, uint32_t n);    // 0x101f7424

// Callees outside the unit (the runtime imports and the other string members); fakes in the tests.
size_t MslStrlen(const char* s);                       // strlen
char* MslStrcpy(char* d, const char* s);               // strcpy
void* MslNew(uint32_t size);                           // operator new
void* MslNewArray(uint32_t size);                      // operator new[]
void MslDelete(void* p);                               // operator delete
void MslDeleteArray(void* p);                          // operator delete[]
void MslThrow(const char* typeName, void* object, const void* dtorTV); // __throw (the fakes return)
int32_t MslWmemcmp(const char16_t* a, const char16_t* b, uint32_t n);   // wmemcmp
char16_t* MslWmemcpy(char16_t* d, const char16_t* s, uint32_t n);       // wmemcpy
void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p);         // 0x100026ac
// Unit v5949 (src/TPMain_std_logic_error_Dtor_logic_error.cpp): the destructors take the hidden signed 16 bit delete flag and answer this.
MslRefCountedChars* MslRefCountedCharsDtor25e0(MslRefCountedChars* self, int16_t deleteFlag); // 0x100025e0
MslExceptionObject* MslLogicErrorDtor(MslExceptionObject* self, int16_t deleteFlag);          // 0x100020cc std::logic_error
MslExceptionObject* MslOutOfRangeDtor(MslExceptionObject* self, int16_t deleteFlag);          // 0x10002008 std::out_of_range
void MslWideString_changeSize(MslWideString* self, uint32_t n, bool keep);                      // 0x101b2cd8
void MslWideString_replace(MslWideString* self, uint32_t pos, uint32_t n1, uint32_t n2, char16_t c); // 0x101f7740
void MslWideString_init(MslWideString* self, const char16_t* first, const char16_t* last);      // 0x10146cf8

// Unit v5044: replace 0x101f7740 (declared above), substr 0x1021212c, begin 0x10212448 (src/ui_iterators_std_basic_string_replace.cpp).
MslWideString* MslWideString_substr(MslWideString* result, const MslWideString* self, uint32_t pos, uint32_t n); // 0x1021212c (const)
char16_t* MslWideString_begin(MslWideString* self);                                                            // 0x10212448
extern char gMslLengthErrorVtbl[16];     // 0x10278b74: std::length_error vtable pointer stand-in
extern char gMslLengthErrorDtorTV[8];    // 0x10265680: TVector of std::length_error::~length_error
extern char16_t gMslWcharNulA;           // 0x102b1434: the terminator replace writes on the copy path
extern char16_t gMslWcharNulB;           // 0x102b1436: the terminator replace writes in place
void* MslNewRep();                       // operator new(0x10): the representation (Mac size; the host object is larger)
char16_t* MslWmemset(char16_t* d, char16_t c, uint32_t n);          // wmemset
char16_t* MslWmemmove(char16_t* d, const char16_t* s, uint32_t n);  // wmemmove
}

// Unit v5086: std::list<std::string> of the Metrowerks library (src/ui_Postcard_panel_std_list_std_basic_string_std_allocator_std_basi.cpp).
// Mac offsets in comments, native pointers here. A list is the count, then the sentinel link (prev, next); the sentinel's
// address is the list +4 (the iterator end()). A node is a link and a string.
namespace IME {
struct MslStrRep {          // the shared representation of a std::string
    uint32_t mLength;       // +0x00
    uint32_t mCapacity;     // +0x04
    uint32_t mRefs;         // +0x08  0 = not shared yet
    char* mData;            // +0x0c
};
struct MslString {          // std::string: one pointer to the representation (4 bytes on the Mac)
    MslStrRep* mRep;        // +0x00
};
struct MslStrNode {
    MslStrNode* mPrev;      // +0x00
    MslStrNode* mNext;      // +0x04
    MslString mStr;         // +0x08
};
struct MslStrLink {         // a node without its string: the sentinel
    MslStrNode* mPrev;      // +0x00
    MslStrNode* mNext;      // +0x04
};
struct MslStrList {
    uint32_t mSize;         // +0x00
    MslStrLink mHead;       // +0x04  prev = the last node, next = the first node; its address is end()
};
// The iterator is the node address. insert answers the new node, the erases the node after the removed ones.
MslStrNode* MslStrList_insert(MslStrList* self, MslStrNode* pos, const MslString* str);  // 0x101b25e0
void MslStrList_sort(MslStrList* self);                                                 // 0x1020811c
MslStrNode* MslStrList_erase(MslStrList* self, MslStrNode* first, MslStrNode* last);    // 0x102084d8
MslStrNode* MslStrList_erase1(MslStrList* self, MslStrNode* pos);                       // 0x10208838
void MslStrList_merge(MslStrList* self, MslStrList* other);                             // 0x10209898
// Callees outside the unit (fakes in the tests)
void MslString_construct(MslString* self, const MslString* src, uint32_t pos, uint32_t n); // 0x101b2918 basic_string(const&, pos, n)
bool MslString_less(const MslString* a, const MslString* b);                            // 0x10209f04 operator<
void MslStrList_clear(MslStrList* self);                                                // 0x1017fc04
void MslStrList_splice(MslStrList* self, MslStrNode* pos, MslStrList* other);           // 0x1020abec
}

// Unit v5099: std::basic_string<wchar_t>::operator[](unsigned long) 0x10212298 (src/std_basic_string_operator.cpp): makes the
// representation unshared (copy when the reference count is above 1), marks it 0 and answers the address of the character.
namespace IME {
char16_t* MslWideString_index(MslWideString* self, uint32_t index);   // 0x10212298
}

// Unit v5645: std::basic_string<wchar_t> constructors and init, change_size (src/Cheat_std_basic_string_init.cpp). The allocator
// arguments are dropped. The three terminators are globals of the library (initial word 0).
namespace IME {
extern char16_t gMslWcharNulInit;     // 0x102a4d3c: written after the last character by init
extern char16_t gMslWcharNulResize;   // 0x102ac356: written by change_size
extern char16_t gMslWcharNulEmpty;    // 0x102b006c: written by the empty string's constructor
MslWideString* MslWideString_ctor(MslWideString* self, const char16_t* text, uint32_t n);   // 0x10146b8c basic_string(const wchar_t*, unsigned long, alloc)
MslWideString* MslWideString_ctorCStr(MslWideString* self, const char16_t* text);           // 0x10146a14 basic_string(const wchar_t*, alloc)
MslWideString* MslWideString_ctorEmpty(MslWideString* self);                                // 0x101e0b84 basic_string(alloc)
}

// Unit v5768 (src/ui_iterators_sinit_ui_IME_cpp.cpp): ui_IME.cpp's static initialiser.
void __sinit_ui_IME_cpp();                 // 0x101f7c74 (124)
extern uint32_t gUIIMESinitDtorTV[2];      // 0x10268600: TVector of CUIIME::~CUIIME (stand-in)
extern uint32_t gUIIMESinitNode[3];        // 0x10396a40: its __register_global_object record
