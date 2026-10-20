#include "TPAdvisor.h"

// 0x10032358. The original inlines the destruction of the embedded CLight (its virtual base CMapLinkBase
// vtable stores, which only rewrite vptrs and offsets) and then deletes the object when the hidden
// "delete storage" flag is positive. Both are what the compiler generates for the member destruction
// and for a delete expression, so the body is empty. The destructor is defined inline in
// include/TPAdvisor.h so every library that uses sAdvisorStore links.
