// The park's date messages (main app, unit w129): the three constructors 0x10151c24, 0x10151c6c and 0x10151cb4, the GetType functions 0x10112de0, 0x10112da8
// and 0x10112d70, and the destructors 0x1011261c, 0x101125a4 and 0x1011252c. They live together because the native vtable needs a key function (GetType);
// the pattern is src/ThingMessages.cpp's.
#include "DateMessages.h"

// The Mac writes the base vtable, a zero flags word and the message's own vtable, in that order; there is no payload.
CMsgEndOfDay::CMsgEndOfDay() { mFlags = 0; }
CMsgEndOfMonth::CMsgEndOfMonth() { mFlags = 0; }
CMsgEndOfYear::CMsgEndOfYear() { mFlags = 0; }

int32_t CMsgEndOfDay::GetType() const { return 0xb; }
int32_t CMsgEndOfMonth::GetType() const { return 0xc; }
int32_t CMsgEndOfYear::GetType() const { return 0xd; }

// Each Mac destructor resets the vtable and deletes the object for a positive flag (the C++ delete-expression does both).
CMsgEndOfDay::~CMsgEndOfDay() {}
CMsgEndOfMonth::~CMsgEndOfMonth() {}
CMsgEndOfYear::~CMsgEndOfYear() {}
