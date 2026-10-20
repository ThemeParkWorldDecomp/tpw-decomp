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
#include "lobby.h"

#include <cstdlib>
#include <cstring>

#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_System.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "LbFile.h"
#include "LbTime.h"
#include "TPSound.h"
#include "TPString.h"
#include "platform/macdoze.h"

TbDynamicStringTemplate<char> gLobbyIslandTheme("");   // 0x102e48f8

namespace {

// What the lobby's window sends the screen: message 0x1000b with a key code in the first argument (0x2500 and
// 0x2700 look like the left and right arrow keys' virtual codes in the high byte; names invented) and message
// 0x10005, which selects when its first argument is 0.
constexpr int32_t kMessageClick = 0x10005;
constexpr int32_t kMessageKey = 0x1000b;
constexpr int32_t kKeyLeft = 0x2500;
constexpr int32_t kKeyRight = 0x2700;
constexpr int32_t kKeyReturn = 0x0d;

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

// ---- the lobby's generator
// seed = seed * 0x19660d + 0x3c6ef35f on the word at 0x34, which time() seeded; the new seed is the answer.
uint32_t CLBLobby::GetRand() {
    mStarted = mStarted * 0x19660du + 0x3c6ef35fu;
    return mStarted;
}

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

// ---- CLBIsland
// Both meshes are turned on, once.
void CLBIsland::Show() {
    if (mShown != 0) return;
    mMeshA->Show();
    mMeshB->Show();
    mShown = 1;
}

// The island leaves its screen's list (the vtable's slot 0x3c of the screen; a plain member here); the
// name and the title are destroyed, then the object freed when the destructor is the deleting one.
CLBIsland::~CLBIsland() { mOwner->UnRegister(this); }

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

// ---- CLBScreenComponentIslands
// The current island's two looping sounds are faded and cleared; then the sounds of the FIRST island of the
// list (not the current one) are started again, from its set's row of the table.
void CLBScreenComponentIslands::ResetIslandLoopingSounds() {
    if (CLBIsland* current = mCurrent; current != nullptr) {
        SOUND_FadeSoundEffect(current->mSound1);
        SOUND_FadeSoundEffect(current->mSound2);
        current->mSound2 = 0;
        current->mSound1 = 0;
    }
    if (mIslands != nullptr && mIslands->mCount != 0) {
        CLBIsland* first = mIslands->mHead->mData;
        if (first != nullptr) {
            first->mSound1 = SOUND_SendEvent(first->mSound1, gLobbyIslandSounds[first->mSoundSet * 2 + 1], 1, 0, 0, 0);
            first->mSound2 = SOUND_SendEvent(first->mSound2, gLobbyIslandSounds[first->mSoundSet * 2], 2, 0, 0, 0);
            SOUND_Tick();
        }
    }
}

// ---- CLBScreenComponentThemeIslands
// (The base destructor's text, inlined in the original, runs as the base class's.)
CLBScreenComponentThemeIslands::~CLBScreenComponentThemeIslands() {}

// A key or a click on the lobby: left and right arrows go to the previous and next island, Return and a
// click with nothing in the first argument select the current one.
void CLBScreenComponentThemeIslands::WindowMessagesFn(int32_t message, int32_t a, int32_t b) {
    if (message == kMessageKey) {
        if (a == kKeyLeft) GotoPreviousIsland();
        else if (a == kKeyRight) GotoNextIsland();
        else if (a == kKeyReturn) SelectCurrentIsland();
    } else if (message == kMessageClick) {
        if (a == 0) SelectCurrentIsland();
    }
}

// While zooming in (state 1 or 2) it backs out to the browsing state; from the state 2 the island's second
// mesh plays its animation 1 once. The answer is whether it did something. The island lobby's panels come
// back either way.
uint32_t CLBScreenComponentThemeIslands::ProcessCancel() {
    uint32_t handled = 0;
    if (mCameraState != 0) {
        if (mCameraState == 2) mCurrent->mMeshB->PlayAnim(1, 0);
        mCameraState = 0;
        handled = 1;
    }
    ShowIslandLobbyController();
    UpdateIslandLobbyUI();
    return handled;
}

// Browsing with a current island: the easy mode goes in free, anything else asks the master save whether
// the player holds the keys the island costs.
void CLBScreenComponentThemeIslands::SelectCurrentIsland() {
    if (mCameraState != 0 || mCurrent == nullptr) return;
    if (TheGameType().mType == kGameTypeEasy) {
        StartCurrentIslandZoom();
        return;
    }
    CFSGlobalParkData* park;
    {
        const TbDynamicStringTemplate<char> name(mCurrent->mName.mData);
        if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
        park = gMasterSave->GetInfoOn(name);
    }
    if (park == nullptr) return;
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    const int32_t keys = gMasterSave->HowManyKeys();
    const int32_t cost = park->KeyCostToEnter();
    if (cost <= keys) StartCurrentIslandZoom();
}

void CLBScreenComponentThemeIslands::StartCurrentIslandZoom() {
    mCameraState = 1;
    KeyParticlePOUFFE();
    HideIslandLobbyController();
}

// The zoom has finished: the theme of the island becomes the current theme and the lobby is done (state 2).
void CLBScreenComponentThemeIslands::EnterCurrentIsland() {
    if (mCurrent == nullptr) return;
    gLobbyIslandTheme = mCurrent->mName.mData;
    gGame.SetCurrentTheme(TbDynamicStringTemplate<char>(mCurrent->mName.mData));
    gLobby->mState = 2;
}

void CLBScreenComponentThemeIslands::SetCurrentIsland(const CLBIsland* island) {
    if (island == nullptr) return;
    mCurrent = const_cast<CLBIsland*>(island);
    mCameraState = 0;
}

// Only while browsing (state 0), and not in the easy mode: the base class moves, then the panels follow.
void CLBScreenComponentThemeIslands::GotoNextIsland() {
    if (mCameraState != 0) return;
    if (TheGameType().mType == kGameTypeEasy) return;
    CLBScreenComponentIslands::GotoNextIsland();
    ShowIslandLobbyController();
    UpdateIslandLobbyUI();
}

void CLBScreenComponentThemeIslands::GotoPreviousIsland() {
    if (mCameraState != 0) return;
    if (TheGameType().mType == kGameTypeEasy) return;
    CLBScreenComponentIslands::GotoPreviousIsland();
    ShowIslandLobbyController();
    UpdateIslandLobbyUI();
}

void CLBScreenComponentThemeIslands::ResetAdvisorTimeout() { mFE4 = LbTime_GetClock(); }
