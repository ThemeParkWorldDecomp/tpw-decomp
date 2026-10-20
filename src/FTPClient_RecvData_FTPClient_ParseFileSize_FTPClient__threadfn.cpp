// Unit v51070: FTPClient::_threadfn and small library functions of the Metrowerks runtime on the Mac's word layouts (main app).
#include <cstdint>
#include <cstring>
#include "FS_Online.h"
#include "ui_ime.h"
#include "platform/arena.h"

// Placeholders for the data the IPBase constructor points the globals at (only the first words are known).
const int16_t gIPErrorCodesData[] = {0x2714, 0x2719, 0};   // 0x102b1804
const char* const gIPErrorTextsData[] = {nullptr};         // 0x102b186c
char gSockVtblData[8];                                     // 0x102b1954

namespace {
// A Mac word that points into the arena is an arena offset here (0 is null).
inline uint32_t* Words(uint32_t offset) { return offset != 0 ? reinterpret_cast<uint32_t*>(tpw::gArenaBase + offset) : nullptr; }
inline uint32_t Addr(const void* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }
// The address of a global (outside the arena) as the Mac word the object stores: truncated.
inline uint32_t GAddr(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
const char kSpliceMessage[] = "list::splice length error";                                    // 0x1024ffbb
const char kLengthErrorType[] = "!std::exception!!std::logic_error!!std::length_error!!";   // 0x102500f9

struct SpliceException {
    const void* mVtbl;     // 0x00
    char* mMessage;        // 0x04
    uint32_t* mRefs;       // 0x08
};
}  // namespace

// 0x1020c860 (64): the worker thread: runs the loop, then wakes the second condition. Answers 0.
void* FTPClient::_threadfn(void* self) {
    FTPClient* client = static_cast<FTPClient*>(self);
    client->ProcessEvents();
    reinterpret_cast<bthread_cond*>(client->mCondB)->signal();
    return nullptr;
}

namespace v51070 {

// 0x1020ca80 (36)
void IPBaseCtor(uint32_t* self) {
    self[0] = GAddr(gIPSystemVtableBlock + 8);
    gIPErrorCodes = gIPErrorCodesData;
    gIPErrorTexts = gIPErrorTextsData;
}

// 0x1020cb7c (76)
uint32_t* IPBaseDtor(uint32_t* self, int16_t flag) {
    if (self) {
        self[0] = GAddr(gIPSystemVtableBlock + 8);
        if (flag > 0) v51070_callee::Delete(self);
    }
    return self;
}

// 0x1020db8c (312): erase(first, last) of a list of wide strings; the result iterator is stored at *result.
uint32_t* EraseWideStrings(uint32_t* result, uint32_t* list, uint32_t first, uint32_t last) {
    if (first == last) {
        *result = last;
        return result;
    }
    uint32_t* lastPrev = Words(Words(last)[0]);
    uint32_t* firstPrev = Words(Words(first)[0]);
    firstPrev[1] = lastPrev[1];
    uint32_t* after = Words(lastPrev[1]);
    after[0] = Words(first)[0];
    uint32_t node = first;
    while (node != last) {
        uint32_t* string = Words(node + 8);
        if (string != nullptr) {
            uint32_t* rep = Words(string[0]);
            if (rep[2] == 0) rep[2] = 1;
            uint32_t refs = rep[2] - 1;
            rep[2] = refs;
            if (refs != 0) string[0] = 0;
            if (string[0] != 0) {
                v51070_callee::Delete(Words(Words(string[0])[3]));
                v51070_callee::Delete(Words(string[0]));
            }
        }
        uint32_t current = node;
        node = Words(node)[1];
        v51070_callee::Delete(Words(current));
        list[0] = list[0] - 1;
    }
    *result = last;
    return result;
}

// 0x1020e2a0 (20)
void SockCtor(uint32_t* self) {
    self[0] = GAddr(gSockVtblData);
    self[1] = 0xffffffffu;
}

// 0x1020e2d4 (100)
Sock* SockDtor(Sock* self, int16_t flag) {
    if (self) {
        self->mVtable = GAddr(gSockVtblData);
        self->Close();
        if (flag > 0) v51070_callee::Delete(self);
    }
    return self;
}

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

// 0x1020e560 (92)
uint32_t* CTokenListDtor(uint32_t* self, int16_t flag) {
    if (self) {
        v51070_callee::CTokenListClear(self);
        if (flag > 0) v51070_callee::Delete(self);
    }
    return self;
}

// 0x1020eeec (96)
uint32_t* CTokenDtor(uint32_t* self, int16_t flag) {
    if (self) {
        v51070_callee::WStringDtor(self, -1);
        if (flag > 0) v51070_callee::Delete(self);
    }
    return self;
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
