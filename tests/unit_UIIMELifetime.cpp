// Unit w92: the lifetime of the IME classes (src/TPEmbed_ime.cpp: IME::CIME::CIME, ~CIME and the candidate vector's tear_down;
// src/ui_ime_lifetime.cpp: list_cand_data_t::~list_cand_data_t, the string vector's tear_down, CUIIME::~CUIIME).
// The only boundaries are the string library's constructor and destructor (MslWideString, Mac 0x101e0b84 and 0x101468f8) and
// operator delete (recorded by calllog.cpp for every labelled block). The constructor fake gives each string a token word
// (0x7000 + its ordinal) so that the test sees which strings were built and which words the destructors find.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

#include "calllog.h"
#include "ui_ime.h"
using namespace calllog;

namespace {

int32_t gStringOrdinal;

// Labels of the fixture (the PowerPC side uses the same numbers).
constexpr int kObject = 1;          // the CIME, the CUIIME or the candidate list the operation runs on
constexpr int kStringBase = 4;      // the CIME's strings +0x08, +0x0c, +0x10 are 4, 5, 6
constexpr int kVectorBlock = 7;     // the block of the CIME's candidate vector
constexpr int kMemberBlock = 8;     // the string block of the CUIIME's candidate list or of the standalone list
constexpr int kMemberStrings = 90;  // its strings 1.. are 91.. (string 0 is the block)
constexpr int kElementBlock = 20;   // the string block of element i is 20 + i
constexpr int kElementStrings = 40; // string j of element i is 40 + 6 * i + j (string 0 is the block)

// Every block the fixture took from operator new, so that those the operation did not free are freed afterwards.
std::vector<void*> gBlocks;

struct Reader {
    const int64_t* next;
    int64_t take() { return *next++; }
    uint32_t word() { return static_cast<uint32_t>(take()); }
};

// One string vector: capacity, size, whether it has a block, and the tokens of the strings the block holds.
struct StringsSpec {
    uint32_t capacity, size;
    bool hasBlock;
    std::vector<uint32_t> tokens;
};

StringsSpec ReadStrings(Reader& in) {
    StringsSpec s;
    s.capacity = in.word();
    s.size = in.word();
    s.hasBlock = in.take() != 0;
    const int64_t count = in.take();
    for (int64_t i = 0; i < count; ++i) s.tokens.push_back(in.word());
    return s;
}

void Label(int label, const void* object) { tpw_cl_label(label, object); }

// Fills a string vector from its description; the block labels follow the table above (string 0 shares the block's address).
void BuildStrings(MslVector<IME::MslWideString>& vector, const StringsSpec& s, int blockLabel, int stringLabel) {
    vector.mCapacity = s.capacity;
    vector.mSize = s.size;
    vector.mData = nullptr;
    if (!s.hasBlock) return;
    const std::size_t count = s.tokens.empty() ? 1 : s.tokens.size();
    auto* block = static_cast<IME::MslWideString*>(::operator new(count * sizeof(IME::MslWideString)));
    gBlocks.push_back(block);
    std::memset(static_cast<void*>(block), 0, count * sizeof(IME::MslWideString));
    for (std::size_t j = 0; j < s.tokens.size(); ++j)
        block[j].mData = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(s.tokens[j]));
    Label(blockLabel, block);
    for (std::size_t j = 1; j < count; ++j) Label(stringLabel + static_cast<int>(j), &block[j]);
    vector.mData = block;
}

// The CIME's fixture: three string tokens and the candidate vector (capacity, size, block, and one string vector per element).
struct CimeSpec {
    uint32_t tokens[3];
    uint32_t capacity, size;
    bool hasBlock;
    std::vector<StringsSpec> elements;
};

CimeSpec ReadCime(Reader& in) {
    CimeSpec c;
    for (uint32_t& token : c.tokens) token = in.word();
    c.capacity = in.word();
    c.size = in.word();
    c.hasBlock = in.take() != 0;
    const int64_t count = in.take();
    for (int64_t i = 0; i < count; ++i) c.elements.push_back(ReadStrings(in));
    return c;
}

// Sets every field of a live CIME from the fixture (the vtable pointer stays), with `fill` in the fields the code does not write.
void BuildCime(IME::CIME* c, const CimeSpec& spec, uint8_t fill) {
    std::memset(&c->mF04, fill, sizeof c->mF04);
    c->mString08.mData = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(spec.tokens[0]));
    c->mString0C.mData = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(spec.tokens[1]));
    c->mString10.mData = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(spec.tokens[2]));
    c->mCandidates.mCapacity = spec.capacity;
    c->mCandidates.mSize = spec.size;
    c->mCandidates.mData = nullptr;
    // +0x20 through +0x137: the byte, its padding, the words and the unused block.
    std::memset(&c->mF20, fill, static_cast<std::size_t>((reinterpret_cast<uint8_t*>(c->mF38) + sizeof c->mF38) - &c->mF20));
    Label(kStringBase, &c->mString08);
    Label(kStringBase + 1, &c->mString0C);
    Label(kStringBase + 2, &c->mString10);
    if (!spec.hasBlock) return;
    const std::size_t count = spec.elements.empty() ? 1 : spec.elements.size();
    auto* block = static_cast<IME::list_cand_data_t*>(::operator new(count * sizeof(IME::list_cand_data_t)));
    gBlocks.push_back(block);
    std::memset(static_cast<void*>(block), fill, count * sizeof(IME::list_cand_data_t));
    for (std::size_t i = 0; i < spec.elements.size(); ++i)
        BuildStrings(block[i].mStrings, spec.elements[i], kElementBlock + static_cast<int>(i),
                     kElementStrings + 6 * static_cast<int>(i));
    Label(kVectorBlock, block);
    c->mCandidates.mData = block;
}

// A standalone or embedded candidate list: the first 0x10 bytes get the fill, the strings come from the description.
void BuildCand(IME::list_cand_data_t* l, const StringsSpec& spec, uint8_t fill) {
    std::memset(l->mF00, fill, sizeof l->mF00);
    BuildStrings(l->mStrings, spec, kMemberBlock, kMemberStrings);
}

// ---- the dump: what the Mac image shows, as words (pointers as labels; the fields the Mac gives words the host lacks are skipped)

int64_t* gOut;
int32_t gCount;
void Put(int64_t value) { gOut[gCount++] = value; }
uint32_t Low(const IME::MslWideString& s) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(s.mData)); }
void PutBytes(const void* p, std::size_t n) {
    const auto* bytes = static_cast<const uint8_t*>(p);
    for (std::size_t i = 0; i < n; ++i) Put(bytes[i]);
}
void PutWord(const void* p) {
    uint32_t w;
    std::memcpy(&w, p, 4);
    Put(w);
}

void DumpCime(const IME::CIME* c) {
    Put(c->mF04);
    Put(Low(c->mString08));
    Put(Low(c->mString0C));
    Put(Low(c->mString10));
    Put(c->mCandidates.mCapacity);
    Put(c->mCandidates.mSize);
    Put(tpw_cl_lab(c->mCandidates.mData));
    PutBytes(&c->mF20, 1);
    PutBytes(reinterpret_cast<const uint8_t*>(&c->mF20) + 1, 3);   // the padding the byte store at +0x20 must not reach
    Put(c->mF24);
    Put(c->mF28);
    Put(c->mF2C);
    Put(c->mF30);
    Put(c->mF34);
    for (int i = 0; i < 0x100; i += 4) PutWord(&c->mF38[i]);
}

void DumpCand(const IME::list_cand_data_t* l) {
    for (int i = 0; i < 0x10; i += 4) PutWord(&l->mF00[i]);
    Put(l->mStrings.mCapacity);
    Put(l->mStrings.mSize);
    Put(tpw_cl_lab(l->mStrings.mData));
}

void DumpCuiime(const CUIIME* u) {
    PutBytes(u->mF08, sizeof u->mF08);
    PutBytes(&u->mActive, 1);
    PutBytes(u->mF0D, sizeof u->mF0D);
    DumpCime(&u->mIME);
    DumpCand(&u->mCandidates);
    PutBytes(u->mF174, sizeof u->mF174);
    Put(static_cast<uint16_t>(u->mStoredLength));
    Put(static_cast<uint16_t>(u->mStoredInvalid));
}

}  // namespace

// The string library's boundary: the constructor hands out the next token, the destructor records the word it finds and the flag -1.
IME::MslWideString::MslWideString() {
    ++gStringOrdinal;
    mData = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(0x7000 + gStringOrdinal));
    ev("wstr.ctor", {lab(this)});
}
IME::MslWideString::~MslWideString() {
    ev("wstr.dtor", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(mData) & 0xffffffffu), -1});
}

extern "C" {
// description: op, fill, then the CIME (op 0..2, 5..7), then the candidate list (ops 3, 4, 5, 6, 8).
//   ops: 0 CIME::CIME on a block of `fill` bytes  1 ~CIME  2 delete of a CIME  3 ~list_cand_data_t  4 delete of a list
//        5 ~CUIIME  6 delete of a CUIIME  7 the candidate vector's tear_down  8 the string vector's tear_down
// out gets the dump of the surviving object (none after a delete), then the number of blocks operator delete handled.
int32_t tpw_ime_run(const int64_t* description, int64_t* out) {
    gOut = out;
    gCount = 0;
    Reader in{description};
    const int32_t op = static_cast<int32_t>(in.take());
    const uint8_t fill = static_cast<uint8_t>(in.take());
    const CimeSpec cime = ReadCime(in);
    const StringsSpec cand = ReadStrings(in);

    tpw_cl_on = 0;
    gBlocks.clear();
    IME::CIME* c = nullptr;
    IME::list_cand_data_t* l = nullptr;
    CUIIME* u = nullptr;
    if (op == 0) {
        // The constructor runs on a block of fill bytes (placement new keeps them where it does not write).
        void* raw = ::operator new(sizeof(IME::CIME));
        gBlocks.push_back(raw);
        std::memset(raw, fill, sizeof(IME::CIME));
        Label(kObject, raw);
        IME::CIME* future = static_cast<IME::CIME*>(raw);   // the members' addresses, for the labels before the constructor runs
        Label(kStringBase, &future->mString08);
        Label(kStringBase + 1, &future->mString0C);
        Label(kStringBase + 2, &future->mString10);
        gStringOrdinal = 0;
        tpw_cl_heap[0] = tpw_cl_heap[1] = tpw_cl_heap[2] = tpw_cl_heap[3] = 0;
        tpw_cl_on = 1;
        c = new (raw) IME::CIME();
        tpw_cl_on = 0;
        // The vtable word is the one thing the host cannot show as the Mac does: a non-null pointer stands for 0x102b15b0.
        Put(*reinterpret_cast<void* const*>(c) != nullptr);
        DumpCime(c);
        Put(tpw_cl_heap[2]);
        for (void* p : gBlocks) ::operator delete(p);
        return gCount;
    }
    if (op == 1 || op == 2 || op == 7) {
        c = new IME::CIME();
        gBlocks.push_back(c);
        BuildCime(c, cime, fill);
        Label(kObject, c);
    } else if (op == 3 || op == 4 || op == 8) {
        l = new IME::list_cand_data_t;
        gBlocks.push_back(l);
        BuildCand(l, cand, fill);
        Label(kObject, l);
    } else {
        u = new CUIIME;
        gBlocks.push_back(u);
        void* vtable;
        std::memcpy(&vtable, static_cast<const void*>(&u->mIME), sizeof vtable);
        std::memset(static_cast<void*>(u), fill, sizeof *u);
        std::memcpy(static_cast<void*>(&u->mIME), &vtable, sizeof vtable);
        BuildCime(&u->mIME, cime, fill);
        BuildCand(&u->mCandidates, cand, fill);
        Label(kObject, u);
    }
    tpw_cl_heap[0] = tpw_cl_heap[1] = tpw_cl_heap[2] = tpw_cl_heap[3] = 0;
    tpw_cl_on = 1;
    switch (op) {
    case 1: c->~CIME(); break;
    case 2: delete c; break;
    case 3: l->~list_cand_data_t(); break;
    case 4: delete l; break;
    case 5: u->~CUIIME(); break;
    case 6: delete u; break;
    case 7: c->mCandidates.tear_down(); break;
    case 8: l->mStrings.tear_down(); break;
    }
    tpw_cl_on = 0;
    switch (op) {
    case 1: case 7: DumpCime(c); break;
    case 3: case 8: DumpCand(l); break;
    case 5: DumpCuiime(u); break;
    }
    Put(tpw_cl_heap[2]);
    // Free what the operation left (a block still labelled was not freed).
    for (void* p : gBlocks)
        if (tpw_cl_lab(p) > 0) ::operator delete(p);
    return gCount;
}
}
