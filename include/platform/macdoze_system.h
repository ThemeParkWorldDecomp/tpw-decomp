// The system calls of macdoze that the platform layer implements in platform/macdoze_system.cpp and
// platform/registry.cpp (threads, sleeping, critical sections, string helpers, the clock's text, disk and
// temp folder, the registry). Other callers declare the ones they use in their own headers
// (include/FS_Online.h, include/LbSCBack.h, include/ui_Postcard.h); these are the same functions.
#pragma once

#include <cstdint>

namespace NS_MacDoze {

struct InternalHandle;
struct _SYSTEMTIME;

void Sleep(uint32_t milliseconds);                                    // macdoze 0x1000c5b4
uint32_t _beginthreadex(void* security, uint32_t stackSize, void* (*start)(void*), void* arg, uint32_t flags,
                        uint32_t* id);                                // macdoze 0x1000c478
void InitializeCriticalSection(void** section);                       // macdoze 0x1000ba90
void DeleteCriticalSection(void** section);                           // macdoze 0x1000bad4
int32_t SetEvent(InternalHandle* handle);                             // macdoze 0x1000be2c

char* _itoa(int32_t value, char* buffer, int32_t radix);              // macdoze 0x10002f40
int32_t _strnicmp(const char* a, const char* b, int32_t count);       // macdoze 0x10003224
int32_t _wcsnicmp(const char16_t* a, const char16_t* b, uint32_t count);   // macdoze 0x100037c8

int32_t GetDiskFreeSpace(const char* root, uint32_t* sectorsPerCluster, uint32_t* bytesPerSector,
                         uint32_t* freeClusters, uint32_t* totalClusters);   // macdoze 0x10005068
uint32_t GetTempPath(uint32_t size, char* path);                      // macdoze 0x1000515c
int32_t GetDateFormatA(uint32_t locale, uint32_t flags, const _SYSTEMTIME* time, const char* picture, char* out,
                       int32_t size);                                 // macdoze 0x100064cc
int32_t GetTimeFormatA(uint32_t locale, uint32_t flags, const _SYSTEMTIME* time, const char* picture, char* out,
                       int32_t size);                                 // macdoze 0x1000646c

}  // namespace NS_MacDoze
