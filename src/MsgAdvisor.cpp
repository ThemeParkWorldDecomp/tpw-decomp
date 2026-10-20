// CMsgAdvisor (main app; unit w180): the constructor 0x1015190c, GetType 0x100fd9cc and the destructor 0x100fcea8.
#include "MsgAdvisor.h"

// The base constructor stores the CMsg vtable, the flags word (+4) is cleared, the class's own vtable is stored and the request is copied
// (CAdvisorRequest's copy constructor, 0x1000de30: the eight fields, not the padding) into +8.
CMsgAdvisor::CMsgAdvisor(const CAdvisorRequest& request) : mRequest(request) { mFlags = 0; }

int32_t CMsgAdvisor::GetType() const { return 5; }

CMsgAdvisor::~CMsgAdvisor() {}
