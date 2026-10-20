// FS_System.cpp (main app 0x10003fd4..0x1000525c; src/FS_System.cpp): the game's file
// functions (the engine reads its files through them, see MacOpen...), the directory
// helpers and the paths of the game's folders (CSYS_TPFileInfo).
#pragma once
#include <cstdint>

#include "TbDynamicStringTemplate.h"
#include "bfl_FileSystem.h"

// The game's folders (0x64 bytes on the Mac). The level and language paths are kept
// under the data path. Its destructor is implicit (out-of-line copy 0x10005288).
class CSYS_TPFileInfo {
public:
    void SetLocalPath(const TbDynamicStringTemplate<char>& path);        // 0x10004ccc
    const TbDynamicStringTemplate<char>& GetLocalPath() const;           // 0x10004d4c
    void SetRemotePath(const TbDynamicStringTemplate<char>& path);       // 0x10004d88
    const TbDynamicStringTemplate<char>& GetRemotePath() const;          // 0x10004e08
    void SetDataPath(const TbDynamicStringTemplate<char>& path);         // 0x10004e48
    const TbDynamicStringTemplate<char>& GetDataPath() const;            // 0x10004ec4
    // 0x10004f00: the data path, a separator unless it ends in one, and `path`.
    void SetLevelPath(const TbDynamicStringTemplate<char>& path);
    const TbDynamicStringTemplate<char>& GetLevelPath() const;           // 0x10005050
    void SetLanguagePath(const TbDynamicStringTemplate<char>& path);     // 0x10005090, as SetLevelPath
    static void operator delete(void* object);                           // 0x10238218, the runtime's operator delete
    ~CSYS_TPFileInfo() {}                                             // 0x10005288 (160): the five strings, last first; the deleting form frees through operator delete

    TbDynamicStringTemplate<char> mLocalPath;      // 0x00  the project directory
    TbDynamicStringTemplate<char> mRemotePath;     // 0x14
    TbDynamicStringTemplate<char> mDataPath;       // 0x28
    TbDynamicStringTemplate<char> mLevelPath;      // 0x3c
    TbDynamicStringTemplate<char> mLanguagePath;   // 0x50
};
extern CSYS_TPFileInfo gTPFileInfo;   // 0x102b3458, original name unknown

// 0x10003fd4: opens `name` through the current resource's last storage, the current
// resource or the resource manager; NULL when none has it. `mode` is unused (files open
// for reading).
bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode);
// 0x10004260: reads count items of size bytes; the file's Read result.
uint32_t TPFile_Read(void* dest, uint32_t size, uint32_t count, bfl::FileSystem::CFile* file);
uint32_t TPFile_Close(bfl::FileSystem::CFile* file);                                        // 0x10004354: 0
// 0x100043ec: whence 0 start, 1 current, 2 end; 0 on success, 1 on failure or another whence.
uint32_t TPFile_Seek(bfl::FileSystem::CFile* file, uint32_t offset, uint32_t whence);
int32_t TPFile_Size(bfl::FileSystem::CFile* file);                                          // 0x100044e8

// Looks `name` up in the game's data folders and puts the full path in `path`
// (0x100045c8); 1 when found. The movie players treat only 1 as found. The file it
// opens to check is never closed (as in the original).
uint8_t FS_FindFilePath(const TbDynamicStringTemplate<char>& name, TbDynamicStringTemplate<char>& path);
// 0x10004764: a directory whose name does not start with '.'.
bool FS_IsSubDirectory(const bfl::FileSystem::CFileInfo& info);
void FS_DelTree(const TbDynamicStringTemplate<char>& path);   // 0x100047d8: deletes a directory and its contents
bool FS_IsWadFile(const bfl::FileSystem::CFileInfo& info);    // 0x10004ab8: the name ends in ".wad"
// Appends the path separator to `path` unless it already ends in one (0x10004bc0).
void FS_AddDirectorySeparator(TbDynamicStringTemplate<char>& path);
