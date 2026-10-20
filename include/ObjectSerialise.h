#pragma once
#include "ThingReceivers.h"
#include "Serialise.h"
#include "TbDynamicStringTemplate.h"

// Unit w229 (src/Object_serialise.cpp)
template<> int32_t LOADVAR<CObject::ObjectState>(void*, CObject::ObjectState*, const char*, int32_t); // 0x10111958
template<> int32_t SAVEVAR<CObject::ObjectState>(void*, CObject::ObjectState*, const char*, int32_t); // 0x10111bbc
extern int32_t gObjectStaticC384; // 0x1034c384, meaning unknown
extern int32_t gObjectStaticC380; // 0x1034c380, meaning unknown
extern TbDynamicStringTemplate<char16_t> gObjectStaticC360; // 0x1034c360
