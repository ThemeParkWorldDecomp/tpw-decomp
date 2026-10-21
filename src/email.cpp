// email.cpp range of the main app (SimTheme Park 2.04b3): the player's outbox and address book constructors, and the bullfrog
// thread library's mutex (the inventory files bthread_mutex here; UI_pump.h declares the class). PARTIAL: the outbox's destructor and the
// two loaders are in src/email_load.cpp (unit w90); the rest of the file is untranslated.
#include "email.h"

#include "UI_pump.h"
#include "platform/macdoze.h"

// The Mac constructs the members with the MSL constructors, in this order (the offsets of the listing): the strings at 0x00,
// 0x04, 0x08, the list of strings at 0x0c, the string at 0x18, the three lists of numbers at 0x28, 0x34 and 0x40 and the string
// at 0x50; then the short at 0x54 is 25 and the words at 0x58 and 0x5c are 0. The words 0x1c..0x27 and 0x4c are not written.
Outbox::Outbox() : mF00(), mF04(), mF08(), mF0C(), mF18(), mF28(), mF34(), mF40(), mF50(), mF54(25), mF58(0), mEmail(nullptr) {}

// The list's constructor, then the word at +0xc (the cursor) takes the word at +8 (the first node).
AddressBook::AddressBook() : mAddresses(), mCursor(mAddresses.begin()) {}

namespace {
// What the cursor functions answer a pointer to. The Mac answers the address of the node's string (node + 8), whose representation
// holds the characters' address at +0xc; at the end, the address of the empty string gEmailBlob (0x10396aa8). The callers read
// the text at once, before the next call.
AddressBookNode gAddressEntry;
AddressBookNode* gAddressEntryRef = &gAddressEntry;
}  // namespace

// 0x10206534: the cursor to the first node, then Next.
AddressBookNode** AddressBook::First() {
    mCursor = mAddresses.begin();
    return Next();
}

// 0x10206594: the cursor's address, the cursor moving on; at the end (the sentinel) the empty text, the cursor staying.
AddressBookNode** AddressBook::Next() {
    if (mCursor == mAddresses.end()) {
        gAddressEntry.mText = "";
        return &gAddressEntryRef;
    }
    gAddressEntry.mText = mCursor->c_str();
    ++mCursor;
    return &gAddressEntryRef;
}

// Inline in the original (the list's clear, an MSL instance at 0x1017fc04).
AddressBook::~AddressBook() {}

// The critical section's handle is the mutex object itself. The Mac's functions also load 0 into r3 afterwards.
void bthread_mutex::lock() { NS_MacDoze::EnterCriticalSection(&mHandle); }

void bthread_mutex::unlock() { NS_MacDoze::LeaveCriticalSection(&mHandle); }
