// lobby_parser.cpp (main app of SimTheme Park 2.04b3): the lobby's description file.
// PARTIAL: CLBLobbyGlobals::ParseLobbyDescriptor (0x100b55b8), LB_ReadTbStringUntilNext
// (0x100b5c78) and two of the file's three global strings so far; the theme descriptors
// (CLBScreenComponentIslands::ParseThemeDescriptor) are still to do.
//
// "data:lobby:lobby.txt" is a list of lines "KEYWORD(value)" or, for the island cameras,
// "ISLANDCAMERAPOSITION(index,x,y)". A line is matched by its first characters; anything
// else is skipped. Nothing checks that a matched line has its brackets and commas.
#include "lobby.h"

#include <cstdlib>
#include <cstring>

#include "FS_System.h"
#include "LbFile.h"

TbDynamicStringTemplate<char> gLobbyFolder("data:lobby:");            // 0x102e4978
TbDynamicStringTemplate<char> gLobbyDescriptorFile("lobby.txt");      // 0x102e4958

// The characters at the cursor up to `delimiter`; the cursor ends past the delimiter.
TbDynamicStringTemplate<char> LB_ReadTbStringUntilNext(char** cursor, char delimiter) {
    TbDynamicStringTemplate<char> text;
    while (**cursor != delimiter) {
        text += **cursor;
        ++*cursor;
    }
    ++*cursor;
    return text;
}

namespace {
// Past the keyword's bracket, then the value up to `delimiter` as text.
void ToValue(char*& cursor) {
    while (*cursor != '(') ++cursor;
    ++cursor;
}
double Number(char*& cursor, char delimiter) {
    const TbDynamicStringTemplate<char> text(LB_ReadTbStringUntilNext(&cursor, delimiter));
    return std::atof(text.mData);
}
}  // namespace

// The whole file is read, its line feeds become ends of strings, and each line is tried
// against the seven keywords in turn. The last line is expected to end in a line feed (the
// walk to the end of a line does not look at the text's size).
void CLBLobbyGlobals::ParseLobbyDescriptor() {
    TbDynamicStringTemplate<char> path(gLobbyFolder);
    path += static_cast<const TbStringBase<char>&>(gLobbyDescriptorFile);
    bfl::FileSystem::CFile* file = TPFile_Open(path.mData, "r");
    TPFile_Seek(file, 0, 2);
    const uint32_t size = static_cast<uint32_t>(TPFile_Size(file));
    char* text = static_cast<char*>(LbMem_Alloc(size));
    TPFile_Seek(file, 0, 0);
    TPFile_Read(text, 1, size, file);
    TPFile_Close(file);
    char* cursor;
    for (cursor = text; cursor < text + size; ++cursor) {
        if (*cursor == '\n') *cursor = '\0';
    }
    cursor = text;
    while (cursor < text + size) {
        if (std::strncmp("ISLANDFOV(", cursor, 10) == 0) {
            ToValue(cursor);
            mIslandFov = static_cast<float>(Number(cursor, ')'));
        } else if (std::strncmp("SPINSPEED(", cursor, 10) == 0) {
            ToValue(cursor);
            mSpinSpeed = static_cast<float>(Number(cursor, ')'));
        } else if (std::strncmp("SPINRADIUS(", cursor, 11) == 0) {
            ToValue(cursor);
            mSpinRadius = static_cast<float>(Number(cursor, ')'));
        } else if (std::strncmp("VERTICALOFFSET(", cursor, 15) == 0) {
            ToValue(cursor);
            mVerticalOffset = static_cast<float>(Number(cursor, ')'));
        } else if (std::strncmp("GLOBERADIUSOUT(", cursor, 15) == 0) {
            ToValue(cursor);
            mGlobeRadiusOut = static_cast<float>(Number(cursor, ')'));
        } else if (std::strncmp("GLOBERADIUSIN(", cursor, 14) == 0) {
            ToValue(cursor);
            mGlobeRadiusIn = static_cast<float>(Number(cursor, ')'));
        } else if (std::strncmp("ISLANDCAMERAPOSITION(", cursor, 21) == 0) {
            ToValue(cursor);
            int32_t index;
            {
                const TbDynamicStringTemplate<char> number(LB_ReadTbStringUntilNext(&cursor, ','));
                index = std::atoi(number.mData);
            }
            const double x = Number(cursor, ',');
            const double y = Number(cursor, ')');
            if (index >= 0 && index < 10) {
                mCameraX[index] = static_cast<float>(x);
                mCameraY[index] = static_cast<float>(y);
            }
        }
        while (*cursor != '\0') ++cursor;
        ++cursor;
    }
    LbMem_Free(text);
}
