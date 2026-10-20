// The serialisation instances for float (unit w114): LOADVAR<float> 0x100f16b4 (236 bytes) and SAVEVAR<float> 0x100f18d8 (260 bytes).
// Each is one explicit specialisation over the shared bodies in Serialise.h, so it is a separate symbol as the original's. The four
// bytes are moved and swapped as a word and never converted, so a float keeps its bits (the original uses integer loads and byte
// reversing stores in the loader, and an lfs/stfs pair in the saver that copies the word as it is).
//
// Quirks kept: as for the other 4-byte instances (src/Serialise_basic.cpp): a failed read or write leaves the value alone and answers 1;
// the loader's two asserts come before the swap and the saver's before the write.
#include "Serialise.h"

template<> int32_t LOADVAR<float>(void* file, float* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);   // 0x100f16b4, 236 bytes
}

template<> int32_t SAVEVAR<float>(void* file, float* value, const char* name, int32_t noLog) {
    return serialise::SaveVar(file, value, name, noLog);   // 0x100f18d8, 260 bytes
}
