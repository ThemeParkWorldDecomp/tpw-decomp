// std::list<CToken>::insert (0x10210b44): a length check (the throw sequence is built inline), then one node linked in before the position.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

#include "online.h"
#include "ui_ime.h"

namespace token_list {

void TokenList_insert(TokenListNode** result, TokenList* list, TokenListNode* position, const CToken* token) {   // 0x10210b44
    if (list->mSize > 0x7ffffffeu) {
        // list::insert length error: the exception object is built on the stack and thrown (the fakes of __throw return)
        static const char kMessage[] = "list::insert length error";
        IME::MslExceptionObject exception;
        exception.mVtbl = IME::gMslExcVtblB;
        char* text = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(kMessage)) + 1));
        IME::MslRefCountedCharsCtor(&exception.mRc, text);
        IME::MslStrcpy(exception.mRc.mPtr, kMessage);
        exception.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &exception, IME::gMslLengthErrorDtorTV);
    }
    TokenListNode* node = static_cast<TokenListNode*>(IME::MslNew(0x18));
    ::new (static_cast<void*>(&node->mValue)) CToken(*token);
    position->mPrev->mNext = node;
    node->mPrev = position->mPrev;
    position->mPrev = node;
    node->mNext = position;
    list->mSize = list->mSize + 1;
    *result = node;
}

}  // namespace token_list
