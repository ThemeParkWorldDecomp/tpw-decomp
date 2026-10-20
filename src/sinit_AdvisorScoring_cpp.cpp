// Static initialiser of AdvisorScoring.cpp (0x1001c34c): constructs the file's global string (0x102b4110), registers it for
// destruction, then copies the 351 scorer pointers to member (12 bytes each on the Mac, from the sources 0x1027df30 + 12 * k) into
// the scorer slot (+0x10) of record k of the message table (0x1027efa4 + 0x30 * k).
#include <new>

#include "AdvisorScoring.h"

alignas(8) unsigned char gAdvisorStringStorage[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x102b4110
unsigned char gAdvisorStringNode[8];                                                                            // 0x102b4124 (the registration node)

// The text at 0x1023c570.
static const char16_t kAdvisorStringText[] = u"gms.dat";

void DestroyAdvisorString(void* object, int16_t) {
    static_cast<TbDynamicStringTemplate<char16_t>*>(object)->~TbDynamicStringTemplate();
}

void __sinit_AdvisorScoring_cpp() {
    auto* text = new (gAdvisorStringStorage) TbDynamicStringTemplate<char16_t>(kAdvisorStringText);
    __register_global_object(text, reinterpret_cast<void*>(&DestroyAdvisorString), gAdvisorStringNode);
    for (int32_t k = 0; k < kAdvisorMessageCount; k++) gAdvisorMessages[k].mScorer = gAdvisorScorerSources[k];
}
