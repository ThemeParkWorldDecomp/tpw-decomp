// Bullfrog wildcard masks, Mac size 0x10. The only virtual slot is the destructor.
#pragma once
#include "TbTList.h"
#include "TbDynamicStringTemplate.h"
#include "TbError.h"

class TbCharStringList : public TbTList<TbDynamicStringTemplate<char>, TbDynamicStringTemplate<char>&> {
public:
    virtual ~TbCharStringList(); // Mac slot 8
};

class TbStringMatch : public TbCharStringList {
public:
    ~TbStringMatch() override;
    TbError SetMask(const char* mask);
    bool MatchString(const char* text);
    bool MatchSubString(const char* text, void* position);
};
