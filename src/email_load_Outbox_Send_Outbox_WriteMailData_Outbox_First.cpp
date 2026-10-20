// Outbox cursor and mark functions (First, Next, MarkToSend, MarkToDelete, Delete), std::operator== on strings, the std::basic_ifstream<char>
// constructor and Email::Attachment::~Attachment (unit v5950).
//
// The Mac's cursor (the word at +0x4c) is a node of the list at +0x28. The host list is a std::list, so the cursor is kept as the node's index
// in the four bytes of mF4C (index == size is the end node). The mark functions copy the cursor's id to the end of the list, sort it and
// remove the repeats, as the original's insert, sort and unique do.
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <iterator>
#include <list>
#include <string>
#include "email.h"
#include "Ptr32.h"
#include "platform/macdoze.h"
#include "std_basic_filebuf.h"

extern char gIfstreamVtbl[];

namespace {

uint32_t Cursor(const Outbox* box) {
    uint32_t value;
    std::memcpy(&value, box->mF4C, 4);
    return value;
}

void SetCursor(Outbox* box, uint32_t value) { std::memcpy(box->mF4C, &value, 4); }

uint32_t IdAt(const std::list<uint32_t>& list, uint32_t index) {
    auto it = list.begin();
    std::advance(it, index);
    return *it;
}

// The word the Mac stores for a global's address: an arena offset, else the truncated address (test libraries define the globals outside).
uint32_t A(const void* p) {
    return tpw::InArena(p) ? tpw::ArenaOffset(p) : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

}  // namespace

// 0x10205718: the cursor to the first mail; ReadMailData of its id, 0 when there is none.
void Outbox::First() {
    SetCursor(this, 0);
    if (mF28.empty()) return;
    ReadMailData(static_cast<int32_t>(IdAt(mF28, Cursor(this))));
}

// 0x1020578c: the cursor to the next mail; at the end it goes back to the first and answers 0.
bool Outbox::Next() {
    if (mF28.empty()) return false;
    uint32_t next = Cursor(this) + 1;
    SetCursor(this, next);
    if (next == mF28.size()) {
        SetCursor(this, 0);
        return false;
    }
    return ReadMailData(static_cast<int32_t>(IdAt(mF28, next)));
}

// 0x10205828: the cursor's id into the list at +0x34, sorted, without repeats.
void Outbox::MarkToSend() {
    mF34.push_back(IdAt(mF28, Cursor(this)));
    mF34.sort();
    mF34.unique();
}

// 0x10205910: the same for the list at +0x40.
void Outbox::MarkToDelete() {
    mF40.push_back(IdAt(mF28, Cursor(this)));
    mF40.sort();
    mF40.unique();
}

// 0x10206278: deletes every mail marked in the list at +0x40 (its files, and its id from the lists at +0x34 and +0x28), empties that
// list, saves, and puts the cursor on the first mail as First does.
void Outbox::Delete() {
    for (std::list<uint32_t>::iterator it = mF40.begin(); it != mF40.end(); ++it) {
        uint32_t id = *it;
        DeleteMail(static_cast<int32_t>(id));
        mF34.remove(id);
        mF28.remove(id);
    }
    mF40.clear();
    Save();
    SetCursor(this, 0);
    if (mF28.empty()) return;
    ReadMailData(static_cast<int32_t>(IdAt(mF28, Cursor(this))));
}

// 0x1020702c
bool msl_char_string::equal(const MslCharString* a, const MslCharString* b) {
    return msl_char_string::compare(a, 0, a->mRep->len, b->mRep->data, b->mRep->len) == 0;
}

// 0x1020801c: the ios part is built only with the virtual-base flag (a halfword, tested after extsh); its vtable word takes the ios_base
// vtable and then the ostream one (the second store wins).
MacIfstream* BasicIfstreamCtor(MacIfstream* self, int16_t withVirtualBase) {
    const uint32_t iosBaseVt = A(gIosBaseVt);
    const uint32_t ostreamVt = A(gOstreamVt1);
    const uint32_t ifstreamVt = A(gIfstreamVtbl);
    if (withVirtualBase != 0) {
        self->mIos.set(&self->mIosPart);
        self->mIosPart.mVtbl = iosBaseVt;
        self->mIosPart.mVtbl = ostreamVt;
    }
    ifstream_char_callee::IstreamCtor(self, 0, A(&self->mFilebuf));
    self->mVtable = ifstreamVt;
    self->mIos->mVtbl = ifstreamVt + 0xc;
    self->mIos->mVbaseOffset = static_cast<int32_t>(reinterpret_cast<intptr_t>(&self->mIosPart) - reinterpret_cast<intptr_t>(self->mIos.get()));
    ifstream_char_callee::FilebufCtor(&self->mFilebuf, 0);
    return self;
}

// 0x10202894: frees the data block, destroys the two strings (the one at +4 first) and, for a positive flag, the object.
Email::Attachment* Email::Attachment::Destruct(Attachment* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mData.get() != nullptr) {
            outbox_callee::OperatorDeleteArray(self->mData.get());
            self->mData.mOffset = 0;
        }
        email_callee::StringDtor(&self->mString04, -1);
        email_callee::StringDtor(&self->mString00, -1);
        if (deleteFlag > 0) email_callee::OperatorDelete(self);
    }
    return self;
}
