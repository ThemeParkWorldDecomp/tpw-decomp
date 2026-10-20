// std::__destroy<Argument*> and std::__destroy<Argument> of the Metrowerks library, main app (unit v5310).
#include "email.h"
#include "online.h"

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
