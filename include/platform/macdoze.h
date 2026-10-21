// Elegant Chaos "macdoze" shim (Win32 API on the Mac OS, namespace NS_MacDoze): the calls
// game-side code makes. Declarations only; the platform layer implements them
// (analysis/platform-layer-design.md).
#pragma once
#include <cstdint>
#include <string>
#include "platform/samsutils.h"

struct GrafPort;

namespace NS_MacDoze {

// 0x100067f8 (platform/cursor.cpp): the cursor UCursor::Set makes current; NULL keeps the current one.
void SetCursor(SamsUtilities::UCursor* cursor);

// Critical sections: the handle is the word the caller keeps (bthread_mutex's first word).
void EnterCriticalSection(void** handle);   // macdoze 0x1000bb14
void LeaveCriticalSection(void** handle);   // macdoze 0x1000bb74
void ZeroMemory(void*, uint32_t);
void CopyMemory(void* dest, const void* source, uint32_t size);
int32_t ShowWindow(GrafPort*, int32_t);
int32_t UpdateWindow(GrafPort*);
int32_t GetClientRect(GrafPort*, SamsUtilities::URectangle::LongRect*);
int32_t GetWindowRect(GrafPort*, SamsUtilities::URectangle::LongRect*);
int32_t MoveWindow(GrafPort*, int32_t, int32_t, int32_t, int32_t, int32_t);

// MEMORYSTATUS (winbase.h), 32-bit fields as in the Mac shim.
struct _MEMORYSTATUS {
    uint32_t dwLength;
    uint32_t dwMemoryLoad;
    uint32_t dwTotalPhys;
    uint32_t dwAvailPhys;
    uint32_t dwTotalPageFile;
    uint32_t dwAvailPageFile;
    uint32_t dwTotalVirtual;
    uint32_t dwAvailVirtual;
};

void GlobalMemoryStatus(_MEMORYSTATUS* status);

// MacToWindowsVirtualKey(Mac key code, flags out) -> Windows virtual-key code. The game
// keeps only the low byte of the result; bit 0x100 of the flags asks for the key to be
// treated as an extended (shifted into the high byte) code.
uint16_t MacToWindowsVirtualKey(uint8_t macKeyCode, uint16_t& flags);

// POINT (windef.h).
struct tagPOINT {
    int32_t x;
    int32_t y;
};

// GetAsyncKeyState: nonzero while the key (a Windows virtual-key code; 1 and 2 are the
// left and right mouse buttons) is down.
int16_t GetAsyncKeyState(int32_t virtualKey);
// GetKeyState: the same (macdoze 0x10002974; the Mac's GetAsyncKeyState 0x10002bc8 only calls it): negative while the key
// is down. The online code's CheckShifts asks for shift, control and option (0x10, 0x11, 0x12) at every button down.
int16_t GetKeyState(int32_t virtualKey);
// ScreenToClient: screen to window-content coordinates.
int32_t ScreenToClient(GrafPort* window, tagPOINT* point);
// _stricmp: case-insensitive compare.
int32_t _stricmp(const char* a, const char* b);
struct InternalHandle;
InternalHandle* GetDC(GrafPort* window);                        // macdoze 0x10007bac
int32_t AddFontResource(const char* path);                     // macdoze 0x1000812c
int32_t ReleaseDC(GrafPort* window, InternalHandle* dc);         // macdoze 0x10007c10
int32_t _wcsicmp(const char16_t* a, const char16_t* b);   // the UTF-16 version
// _itow: `value` in base `radix` as UTF-16 text.
char16_t* _itow(int32_t value, char16_t* text, int32_t radix);

// WIN32_FIND_DATAA (cFileName at 0x2c on the Mac, as in Win32).
struct _WIN32_FIND_DATAA {
    uint32_t dwFileAttributes;
    uint32_t ftCreationTime[2];
    uint32_t ftLastAccessTime[2];
    uint32_t ftLastWriteTime[2];
    uint32_t nFileSizeHigh;
    uint32_t nFileSizeLow;
    uint32_t dwReserved0;
    uint32_t dwReserved1;
    char cFileName[260];
    char cAlternateFileName[14];
};
// A Win32 FILETIME: 100 ns ticks since 1 January 1601, the low word first.
struct tagFILETIME {
    uint32_t dwLowDateTime;
    uint32_t dwHighDateTime;
};
// macdoze 0x10006310 (platform/files.cpp): 1, 0 or -1 for `a` later than, in the same second as, or earlier than `b`.
int32_t CompareFileTime(const tagFILETIME* a, const tagFILETIME* b);
struct InternalHandle;
// FindFirstFile(pattern, data): (InternalHandle*)-1 when nothing matches.
InternalHandle* FindFirstFile(const char* pattern, _WIN32_FIND_DATAA* data);
int32_t FindClose(InternalHandle* handle);
uint32_t GetFileAttributes(const char* name);
int32_t CreateDirectory(const char* name, void* security);
int32_t FindNextFile(InternalHandle* search, _WIN32_FIND_DATAA* data);   // 0 when there are no more
int32_t SetFileAttributes(const char* name, uint32_t attributes);
int32_t DeleteFile(const char* name);
int32_t RemoveDirectory(const char* name);
int32_t MoveFile(const char* from, const char* to);
int32_t CopyFile(const char* from, const char* to, int32_t failIfExists);

// Files (winbase.h). A file handle is an InternalHandle*; a failed CreateFile returns
// (InternalHandle*)-1. access: 0x80000000 read, 0x40000000 write; creation: 1 CREATE_NEW,
// 2 CREATE_ALWAYS, 3 OPEN_EXISTING, 4 OPEN_ALWAYS. BOOL results are nonzero for success.
InternalHandle* CreateFile(const char* name, uint32_t access, uint32_t share, void* security, uint32_t creation,
                           uint32_t flags, InternalHandle* templateFile);
int32_t CloseHandle(InternalHandle* handle);
int32_t ReadFile(InternalHandle* file, void* buffer, uint32_t count, uint32_t* done, void* overlapped);
int32_t WriteFile(InternalHandle* file, const void* buffer, uint32_t count, uint32_t* done, void* overlapped);
// method: 0 from the start, 1 from the current position, 2 from the end; 0xffffffff on failure.
uint32_t SetFilePointer(InternalHandle* file, int32_t distance, int32_t* distanceHigh, uint32_t method);
uint32_t GetFileSize(InternalHandle* file, uint32_t* sizeHigh);
int32_t FlushFileBuffers(InternalHandle* file);
int32_t GetMacFileReference(InternalHandle* file);
InternalHandle* CreateFileMapping(InternalHandle* file, void* security, uint32_t protect, uint32_t sizeHigh,
                                  uint32_t sizeLow, const char* name);   // NULL on failure
void* MapViewOfFileEx(InternalHandle* mapping, uint32_t access, uint32_t offsetHigh, uint32_t offsetLow,
                      uint32_t bytes, void* base);
int32_t UnmapViewOfFile(void* view);

// SYSTEM_INFO (winbase.h, 36 bytes): the game reads dwAllocationGranularity (0x1c).
struct _SYSTEM_INFO {
    uint32_t dwOemId;
    uint32_t dwPageSize;
    void* lpMinimumApplicationAddress;
    void* lpMaximumApplicationAddress;
    uint32_t dwActiveProcessorMask;
    uint32_t dwNumberOfProcessors;
    uint32_t dwProcessorType;
    uint32_t dwAllocationGranularity;   // 0x1c on the Mac
    uint16_t wProcessorLevel;
    uint16_t wProcessorRevision;
};
void GetSystemInfo(_SYSTEM_INFO* info);
int32_t InterlockedIncrement(int32_t* value);
int32_t InterlockedDecrement(int32_t* value);
int32_t IsDBCSLeadByte(uint8_t ch);
// Win32 code page conversions (code page 0, no flags here): the characters written (with
// the NUL); with no buffer, the size needed.
int32_t WideCharToMultiByte(uint32_t codePage, uint32_t flags, const char16_t* text, int32_t length, char* out,
                            int32_t size, const char* defaultChar, int32_t* usedDefault);
int32_t MultiByteToWideChar(uint32_t codePage, uint32_t flags, const char* text, int32_t length, char16_t* out,
                            int32_t size);

// GlobalAlloc / GlobalFree: Win32 heap blocks (HGLOBAL); flag 0x40 (GMEM_ZEROINIT) clears
// the block. GlobalFree returns NULL on success.
struct InternalHandle;   // HGLOBAL stand-in (opaque here)
InternalHandle* GlobalAlloc(uint32_t flags, uint32_t size);
InternalHandle* GlobalFree(InternalHandle* block);

// A copy of the text in a block the C library's free() takes back (macdoze's own _strdup).
char* _strdup(const char* text);

// GDI (wingdi.h), used by TrueTypeRender::render_font_unicode. Every handle is an InternalHandle*; the glue stubs are at
// 0x10239d60..0x10239e50 in the main app.
// LOGFONTA (0x3c bytes, the Win32 layout).
struct tagLOGFONTA {
    int32_t lfHeight;
    int32_t lfWidth;
    int32_t lfEscapement;
    int32_t lfOrientation;
    int32_t lfWeight;
    uint8_t lfItalic;
    uint8_t lfUnderline;
    uint8_t lfStrikeOut;
    uint8_t lfCharSet;
    uint8_t lfOutPrecision;
    uint8_t lfClipPrecision;
    uint8_t lfQuality;
    uint8_t lfPitchAndFamily;
    char lfFaceName[32];
};
static_assert(sizeof(tagLOGFONTA) == 0x3c, "LOGFONTA is 0x3c bytes");
// SIZE.
struct tagSIZE {
    int32_t cx;
    int32_t cy;
};
// RGBQUAD, BITMAPINFOHEADER (0x28 bytes) and BITMAPINFO (header plus the colour table).
struct tagRGBQUAD {
    uint8_t rgbBlue;
    uint8_t rgbGreen;
    uint8_t rgbRed;
    uint8_t rgbReserved;
};
struct tagBITMAPINFOHEADER {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
};
static_assert(sizeof(tagBITMAPINFOHEADER) == 0x28, "BITMAPINFOHEADER is 0x28 bytes");
struct tagBITMAPINFO {
    tagBITMAPINFOHEADER bmiHeader;
    tagRGBQUAD bmiColors[1];
};
InternalHandle* CreateCompatibleDC(InternalHandle* dc);                                   // macdoze 0x10007d30
InternalHandle* SelectObject(InternalHandle* dc, InternalHandle* object);                  // macdoze 0x10007dfc, the previous object
int32_t SetMapMode(InternalHandle* dc, int32_t mode);                                      // macdoze 0x10007d94
int32_t DeleteObject(InternalHandle* object);                                              // macdoze 0x10007e8c
int32_t DeleteDC(InternalHandle* dc);                                                      // macdoze 0x10007cbc
uint32_t SetBkColor(InternalHandle* dc, uint32_t colour);                                  // macdoze 0x10007f18
uint32_t SetTextColor(InternalHandle* dc, uint32_t colour);                                // macdoze 0x10009164
// CreateDIBSection(dc, info, usage, bits out, section, offset): the bitmap, with *bits pointing at its pixels.
InternalHandle* CreateDIBSection(InternalHandle* dc, const tagBITMAPINFO* info, uint32_t usage, void** bits,
                                 InternalHandle* section, uint32_t offset);                // macdoze 0x10007f8c
InternalHandle* CreateFontIndirect(const tagLOGFONTA* font);                               // macdoze 0x100090ec
// GetTextExtentPoint(dc, text, length, size out).
int32_t GetTextExtentPoint(InternalHandle* dc, const char* text, int32_t length, tagSIZE* size);   // macdoze 0x100091d8
int32_t TextOut(InternalHandle* dc, int32_t x, int32_t y, const char* text, int32_t length);       // macdoze 0x1000926c

// SYSTEMTIME (winbase.h): eight 16-bit fields.
struct _SYSTEMTIME {
    uint16_t wYear;
    uint16_t wMonth;
    uint16_t wDayOfWeek;
    uint16_t wDay;
    uint16_t wHour;
    uint16_t wMinute;
    uint16_t wSecond;
    uint16_t wMilliseconds;
};
static_assert(sizeof(_SYSTEMTIME) == 16, "SYSTEMTIME is 16 bytes");
// The local date and time (macdoze 0x10005f08). Not implemented by the platform layer yet (unit w113 declares it).
void GetLocalTime(_SYSTEMTIME* time);
// Platform import, declared by w216; untranslated (bullfrog glue 0x1006bbec).
int32_t SystemTimeToFileTime(const _SYSTEMTIME* time, tagFILETIME* fileTime);
// macdoze 0x10005fe4 (240 bytes, platform/macdoze_misc.cpp, unit w125): the FILETIME as a calendar date and time (no time zone; the day
// of the week is the Mac's, Sunday 1; the milliseconds are 0), always 1.
int32_t FileTimeToSystemTime(const tagFILETIME* fileTime, _SYSTEMTIME* time);
// w217: imports used only by the date formatter; test libraries fake them.
int32_t GetDateFormatA(uint32_t locale, uint32_t flags, const _SYSTEMTIME* time, const char* picture,
                       char* out, int32_t size);
int32_t GetTimeFormatA(uint32_t locale, uint32_t flags, const _SYSTEMTIME* time, const char* picture,
                       char* out, int32_t size);
// Unit w221 (src/TbRegistryKey.cpp, src/LbFile_more.cpp). The original C imports
// use this header's C++ namespace linkage. HKEY preserves the Mac's void* word.
typedef uint32_t HKEY;
constexpr HKEY HKEY_CLASSES_ROOT = 0x80000000u;
constexpr HKEY HKEY_CURRENT_USER = 0x80000001u;
constexpr HKEY HKEY_LOCAL_MACHINE = 0x80000002u;
constexpr HKEY HKEY_USERS = 0x80000003u;
constexpr HKEY HKEY_PERFORMANCE_DATA = 0x80000004u;
constexpr HKEY HKEY_CURRENT_CONFIG = 0x80000005u;
constexpr HKEY HKEY_DYN_DATA = 0x80000006u;
int32_t RegOpenKeyA(HKEY, const char*, HKEY*); // macdoze 0x1000735c
int32_t RegOpenKeyExA(HKEY, const char*, uint32_t, uint32_t, HKEY*); // 0x100073b0
int32_t RegSetValueExA(HKEY, const char*, uint32_t, uint32_t, const void*, uint32_t); // 0x100075a8
int32_t RegQueryValueExA(HKEY, const char*, uint32_t*, uint32_t*, void*, uint32_t*); // 0x10007710
int32_t RegCreateKeyExA(HKEY, const char*, uint32_t, char*, uint32_t, uint32_t, void*, HKEY*, uint32_t*); // 0x100078dc
int32_t RegCloseKey(HKEY); // 0x1000792c
// These five original bodies answer 0 and write nothing.
int32_t RegEnumValueA(HKEY, uint32_t, char*, uint32_t*, uint32_t*, uint32_t*, void*, uint32_t*); // 0x100079d8
int32_t RegEnumKeyExA(HKEY, uint32_t, char*, uint32_t*, uint32_t*, char*, uint32_t*, tagFILETIME*); // 0x10007a00
int32_t RegQueryInfoKeyA(HKEY, char*, uint32_t*, uint32_t*, uint32_t*, uint32_t*, uint32_t*, uint32_t*, uint32_t*, uint32_t*, uint32_t*, tagFILETIME*); // 0x10007a28
int32_t RegDeleteValueA(HKEY, const char*); // 0x10007a54
int32_t RegDeleteKeyA(HKEY, const char*); // 0x10007a80
int32_t _strnicmp(const char*, const char*, int32_t); // macdoze 0x10003224
int32_t GetDiskFreeSpace(const char*, uint32_t*, uint32_t*, uint32_t*, uint32_t*); // macdoze 0x10005068
}  // namespace NS_MacDoze

namespace tpw {
// Reads `count` bytes at `offset` of the open file whose Mac reference number (GetMacFileReference) is `reference` into `buffer`:
// the bytes read (fewer than asked at the end of the file), or -1 for a reference no open file has. The File Manager read the
// sound library's MPEG file player (src/TbMpegFilePlayer.cpp) makes in place of PBReadAsync (platform/file_handles.cpp).
int32_t ReadFileReference(int32_t reference, uint32_t offset, void* buffer, uint32_t count);
// The host file a game path ("data:levels:jungle:Features:x:x.hmp") is created or written as: the user folder's copy of that path,
// its folders made (files.cpp's HostPathForWriting). CreateFile, and so TPFile_Open, finds that copy before the game folder's. For the
// game's own fopen writes that the Mac made beside its data (unit v5L44, SaveRideHeightMap) and its MSL output streams (v5L48: the
// address book, the outbox and its mails).
std::string GamePathForWriting(const char* gamePath);
// The host file a game path is read from: the user folder's copy, else the game folder's (files.cpp's FindHostPath, as CreateFile
// finds it). A path neither folder has is given back as it is, so a host path still opens and a missing file still fails. For the
// game's reads through fopen and the MSL input streams (unit v5L48: the outbox's mails and their pictures).
std::string GamePathForReading(const char* gamePath);
}  // namespace tpw
