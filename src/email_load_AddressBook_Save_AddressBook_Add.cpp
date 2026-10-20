#include <cstdio>
#include <cstdint>
#include <fstream>
#include <ios>
#include <list>
#include <string>
#include "email.h"

// AddressBook::Add (0x102065f8): inserts the address at the end of the list (insert before end(), the sentinel at +4), sorts the
// list, removes the repeats, then answers the position of the first entry equal to the address, or -1.
// The header declares Add as void (another unit's stand-in defines it so); the original answers the position in r3, which
// AddressBookAddIndex keeps and the test checks.
int32_t AddressBookAddIndex(AddressBook* self, const std::string& address) {
    self->mAddresses.push_back(address);
    self->mAddresses.sort();
    self->mAddresses.unique();
    int32_t index = 0;
    for (std::list<std::string>::iterator it = self->mAddresses.begin(); it != self->mAddresses.end(); ++it, ++index) {
        if (*it == address) return index;
    }
    return -1;
}

void AddressBook::Add(const std::string& address) {
    AddressBookAddIndex(this, address);
}

// AddressBook::Remove (0x10206728): erases the entry at the given position (a signed compare, so a negative position never matches).
// Answers 1 when an entry was erased, else 0.
int32_t AddressBook::Remove(int32_t position) {
    int32_t index = 0;
    for (std::list<std::string>::iterator it = mAddresses.begin(); it != mAddresses.end(); ++it) {
        int32_t current = index++;
        if (current == position) {
            mAddresses.erase(it);
            return 1;
        }
    }
    return 0;
}
