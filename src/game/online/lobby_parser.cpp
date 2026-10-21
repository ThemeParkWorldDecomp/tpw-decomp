// lobby_parser.cpp: name proven by __sinit_lobby_parser_cpp in the main application.

#define TPW_GLOBE_SHOWHIDE_REAL   // the download globe's Show calls the online globe's real Show (0x100ad788), not the inline base
#include "ESP_sprites.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "HLSprite.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "LbFile.h"
#include "LbTime.h"
#include "lobby.h"
#include "platform/macdoze.h"
#include "tb_printf_opengl.h"
#include "TPSound.h"
#include "TPString.h"
#include "World.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// lobby_parser.cpp (main app of SimTheme Park 2.04b3): the lobby's description file.
// PARTIAL: CLBLobbyGlobals::ParseLobbyDescriptor (0x100b55b8), LB_ReadTbStringUntilNext
// (0x100b5c78) and two of the file's three global strings so far; the theme descriptors
// (CLBScreenComponentIslands::ParseThemeDescriptor) are still to do.
//
// "data:lobby:lobby.txt" is a list of lines "KEYWORD(value)" or, for the island cameras,
// "ISLANDCAMERAPOSITION(index,x,y)". A line is matched by its first characters; anything
// else is skipped. Nothing checks that a matched line has its brackets and commas.

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

// The lobby's island screen, part 3: the theme descriptor parser and the rest of the island and theme
// islands members (main app of SimTheme Park 2.04b3). CLBScreenComponentIslands::ParseThemeDescriptor
// 0x100b5d54, ResetIslandLoopingSounds 0x100b2228; CLBIsland::~CLBIsland 0x100b0108, Show 0x100b01c0;
// CLBLobby::GetRand 0x100aa98c; CLBScreenComponentThemeIslands: WindowMessagesFn 0x100b2af8, ProcessCancel
// 0x100b2c00, SelectCurrentIsland 0x100b3218, StartCurrentIslandZoom 0x100b33e8, EnterCurrentIsland
// 0x100b346c, SetCurrentIsland 0x100b3540, GotoNextIsland 0x100b35ac, GotoPreviousIsland 0x100b3684,
// ResetCurrentIsland 0x100b3760, ResetAdvisorTimeout 0x100b3854, UpdateSigns 0x100b38d4, destructor
// 0x100b4cec; and LB_ReadTbUniStringUntilNext 0x100b69f8, which the parser needs (the parser's other
// string reader is in src/lobby_parser.cpp).
//
// Each theme has a description file, "data:lobby:<theme>.txt": one island per ISLAND( line, optional
// FLYINGMESH( lines for the things that fly over the sea, and, after an island's line, the LIGHTNING(,
// RAINY( and SKYCOLOUR( lines that set that island's weather. The theme islands screen reads every
// theme's file when it is shown (src/lobby_islands.cpp).

namespace {

// The cursor moves past the next `ch` (it stops on the character, then steps over it). Nothing looks for
// the end of the text: a line missing a bracket, a quote or a comma runs on.
void PastNext(char*& cursor, char ch) {
    while (*cursor != ch) ++cursor;
    ++cursor;
}

// The text up to `delimiter` as a number.
int32_t ReadInteger(char*& cursor, char delimiter) {
    const TbDynamicStringTemplate<char> text(LB_ReadTbStringUntilNext(&cursor, delimiter));
    return std::atoi(text.mData);
}
float ReadReal(char*& cursor, char delimiter) {
    const TbDynamicStringTemplate<char> text(LB_ReadTbStringUntilNext(&cursor, delimiter));
    return static_cast<float>(std::atof(text.mData));
}

// A folder written with slashes or backslashes gets the Mac's separator.
void ColonSeparators(TbDynamicStringTemplate<char>& text) {
    char* p = text.LockBuffer(static_cast<uint32_t>(text.Length()));
    while (*p != '\0') {
        if (*p == '\\' || *p == '/') *p = ':';
        ++p;
    }
    text.ReleaseBuffer(0x7fffffff);
}

}  // namespace

// ---- LB_ReadTbUniStringUntilNext
// The characters up to `delimiter`, each widened with its sign (a character above 0x7f becomes 0xff80 and
// up); the cursor ends past the delimiter.
TbDynamicStringTemplate<char16_t> LB_ReadTbUniStringUntilNext(char** cursor, char delimiter) {
    TbDynamicStringTemplate<char16_t> text;
    while (**cursor != delimiter) {
        const char16_t wide = static_cast<char16_t>(static_cast<int16_t>(static_cast<int8_t>(**cursor)));
        text += wide;
        ++*cursor;
    }
    ++*cursor;
    return text;
}

// ---- ParseThemeDescriptor
// The theme's file is read whole and its line feeds become ends of strings; each line is then tried
// against the five keywords in turn and, matched or not, the walk goes on to the end of the line. The
// last line is expected to end in a line feed (the walk to the end of a line does not look at the text's
// size), and a matched line always has its brackets, quotes and commas.
//
//   ISLAND(n,"folder","mesh","mesh","title",angle,y)   an island: n is its place in the list (and picks the
//       lobby description's camera place; a non-negative `order` argument replaces it), the meshes are in
//       the folder (slashes become colons), the island stands at the description's place n with height y.
//   FLYINGMESH("folder","mesh",count,dx,dy,dz,speed)   a group of flying meshes above the last island read
//       (50 higher), with no check that there is one.
//   LIGHTNING(n), RAINY(n), SKYCOLOUR(r,g,b)           the last island's weather (the colour in 16.16).
void CLBScreenComponentIslands::ParseThemeDescriptor(const char* theme, const CFSOnlinePark* park, int32_t order,
                                                     const char16_t* gate1, const char16_t* gate2) {
    TbDynamicStringTemplate<char> path(gLobbyFolder);
    path += theme;
    path += ".txt";
    bfl::FileSystem::CFile* file = TPFile_Open(path.mData, "r");
    TPFile_Seek(file, 0, 2);
    const uint32_t size = static_cast<uint32_t>(TPFile_Size(file));
    char* const text = static_cast<char*>(LbMem_Alloc(size));
    TPFile_Seek(file, 0, 0);
    TPFile_Read(text, 1, size, file);
    TPFile_Close(file);
    char* const end = text + size;
    char* cursor;
    for (cursor = text; cursor < end; ++cursor) {
        if (*cursor == '\n') *cursor = '\0';
    }
    cursor = text;
    while (cursor < end) {
        if (std::strncmp("ISLAND(", cursor, 7) == 0) {
            PastNext(cursor, '(');
            int32_t number = ReadInteger(cursor, ',');
            if (order >= 0) number = order;
            PastNext(cursor, '"');
            TbDynamicStringTemplate<char> folder(LB_ReadTbStringUntilNext(&cursor, '"'));
            ColonSeparators(folder);
            PastNext(cursor, '"');
            TbDynamicStringTemplate<char> meshA(LB_ReadTbStringUntilNext(&cursor, '"'));
            PastNext(cursor, '"');
            TbDynamicStringTemplate<char> meshB(LB_ReadTbStringUntilNext(&cursor, '"'));
            PastNext(cursor, '"');
            TbDynamicStringTemplate<char16_t> title(LB_ReadTbUniStringUntilNext(&cursor, '"'));
            PastNext(cursor, ',');
            const float angle = ReadReal(cursor, ',');
            s3DPoint position;
            position.x = gLobby->mGlobals->mCameraX[number];
            position.y = ReadReal(cursor, ')');
            position.z = gLobby->mGlobals->mCameraY[number];
            CLBIsland* island = new CLBIsland(this, number, theme, folder.mData, meshA.mData, meshB.mData, angle, position,
                                              title.mData, park, gate1, gate2);
            island->Show();
        } else if (std::strncmp("FLYINGMESH(", cursor, 11) == 0) {
            PastNext(cursor, '(');
            PastNext(cursor, '"');
            TbDynamicStringTemplate<char> folder(LB_ReadTbStringUntilNext(&cursor, '"'));
            ColonSeparators(folder);
            PastNext(cursor, '"');
            TbDynamicStringTemplate<char> name(LB_ReadTbStringUntilNext(&cursor, '"'));
            PastNext(cursor, ',');
            const int32_t count = ReadInteger(cursor, ',');
            // (The last island read; NULL before the first ISLAND( line, which the original dereferences.)
            const s3DPoint& island = mLast->mPosition;
            s3DPoint position;
            position.x = island.x;
            position.y = static_cast<float>(static_cast<double>(island.y) + 50.0);
            position.z = island.z;
            s3DPoint direction;
            direction.x = ReadReal(cursor, ',');
            direction.y = ReadReal(cursor, ',');
            direction.z = ReadReal(cursor, ',');
            const float speed = ReadReal(cursor, ')');
            CLBFlyingMeshGroup* group = new CLBFlyingMeshGroup(count, folder.mData, name.mData, position, direction, speed, 500.0f);
            group->Show();
        } else if (std::strncmp("LIGHTNING(", cursor, 10) == 0) {
            PastNext(cursor, '(');
            const int32_t value = ReadInteger(cursor, ')');
            mLast->mLightning = value;
        } else if (std::strncmp("RAINY(", cursor, 6) == 0) {
            PastNext(cursor, '(');
            const int32_t value = ReadInteger(cursor, ')');
            mLast->mRain = value;
        } else if (std::strncmp("SKYCOLOUR(", cursor, 10) == 0) {
            PastNext(cursor, '(');
            const int32_t red = ReadInteger(cursor, ',');
            const int32_t green = ReadInteger(cursor, ',');
            const int32_t blue = ReadInteger(cursor, ')');
            mLast->mSky[0] = static_cast<uint32_t>(red) << 16;
            mLast->mSky[1] = static_cast<uint32_t>(green) << 16;
            mLast->mSky[2] = static_cast<uint32_t>(blue) << 16;
        }
        while (*cursor != '\0') ++cursor;
        ++cursor;
    }
    LbMem_Free(text);
}

// Unit v51046: nine small functions of the lobby, the interface and the ESP sprites (main app).
//   OnlineIslandParkDownloadCompleteCallback 0x100b4420, PostGetTop10ParksListCallbackFunction 0x100b3e28,
//   CLBScreenComponentIslands::ProcessCancel 0x100b1f3c, __sinit_lobby_parser_cpp 0x100b6ae4,
//   CLBScreenComponentDownloadGlobe::Show 0x100ae590, CLBScreenComponentDownloadGlobe::ProcessCancel 0x100ae46c,
//   CIntrface::DrawFPSCounter 0x1007dae8, __sinit_HLSprite_cpp 0x1007d53c, ESP_GetRandomActionFrom 0x1007c9f8.

void UIMain_DestroyString(void* object);   // 0x103bcd38, the narrow string destructor glue
// ---- static initialisers

namespace {
const char16_t kWideText[] = u"upload";   // 0x1023eac4
}

alignas(16) uint8_t gLobbyParserWide[0x30];
uint8_t gLobbyParserWideNode[12];
uint8_t gLobbyFolderNode[12];
uint8_t gLobbyDescriptorFileNode[12];

void __sinit_lobby_parser_cpp() {
    void* wideDestroy = reinterpret_cast<void*>(&UIMain_DestroyWString);
    void* narrowDestroy = reinterpret_cast<void*>(&UIMain_DestroyString);
    static_assert(sizeof(TbDynamicStringTemplate<char16_t>) <= sizeof gLobbyParserWide, "storage");
    auto* wide = ::new (gLobbyParserWide) TbDynamicStringTemplate<char16_t>(kWideText);
    __register_global_object(wide, wideDestroy, gLobbyParserWideNode);
    auto* folder = ::new (&gLobbyFolder) TbDynamicStringTemplate<char>("data:lobby:");        // 0x1023eaad
    __register_global_object(folder, narrowDestroy, gLobbyFolderNode);
    auto* file = ::new (&gLobbyDescriptorFile) TbDynamicStringTemplate<char>("lobby.txt");    // 0x1023eab9
    __register_global_object(file, narrowDestroy, gLobbyDescriptorFileNode);
}
