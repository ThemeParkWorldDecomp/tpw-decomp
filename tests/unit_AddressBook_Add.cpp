// Test library for AddressBook::Add and AddressBook::Remove (unit v5722; libtpw_unit_AddressBook_Add.dylib, tests/test_AddressBook_Add.py), main app:
//   src/game/online/email.cpp
//
// The host keeps a real std::list<std::string>; the wrapper loads it from a newline-joined text, calls the function and writes the
// list back the same way.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <new>
#include <string>

#include "email.h"

int32_t AddressBookAddIndex(AddressBook* self, const std::string& address);

namespace {

struct Book {
    alignas(16) unsigned char storage[sizeof(AddressBook) + 64];
    AddressBook* book() { return reinterpret_cast<AddressBook*>(storage); }
};

void Load(AddressBook* b, const char* joined, int32_t n) {
    new (&b->mAddresses) std::list<std::string>();
    const char* p = joined;
    for (int32_t i = 0; i < n; i++) {
        const char* e = std::strchr(p, '\n');
        size_t len = e ? static_cast<size_t>(e - p) : std::strlen(p);
        b->mAddresses.push_back(std::string(p, len));
        p += len + (e ? 1 : 0);
    }
}

void Dump(AddressBook* b, char* out, int32_t* outn) {
    std::string s;
    int32_t n = 0;
    for (const std::string& a : b->mAddresses) {
        if (n++) s += '\n';
        s += a;
    }
    std::memcpy(out, s.c_str(), s.size() + 1);
    *outn = n;
    b->mAddresses.~list();
}

}  // namespace

extern "C" {

int32_t tpw_abadd_add(const char* joined, int32_t n, const char* address, char* out, int32_t* outn) {
    Book holder;
    AddressBook* b = holder.book();
    Load(b, joined, n);
    int32_t r = AddressBookAddIndex(b, std::string(address));
    Dump(b, out, outn);
    return r;
}

int32_t tpw_abadd_remove(const char* joined, int32_t n, int32_t position, char* out, int32_t* outn) {
    Book holder;
    AddressBook* b = holder.book();
    Load(b, joined, n);
    int32_t r = b->Remove(position);
    Dump(b, out, outn);
    return r;
}

}  // extern "C"
