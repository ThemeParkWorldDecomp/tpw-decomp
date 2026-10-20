// lobby.cpp (main app): the 3D island lobby. The lobby object and the base class of its
// systems are translated (src/lobby.cpp); the four systems themselves (meshes, view,
// flying meshes, screens) are declared here with what the lobby object needs of them and
// are untranslated.
#pragma once
#include <cstdint>

#include "Advisor.h"
#include "Ptr32.h"
#include "TbDynamicStringTemplate.h"
#include "m3d2.h"
#include "TbTList.h"

class InterfaceWindow;
class CLBLobbySystem;
class CFSOnlinePark;
class CLBIsland;
class CLBScreenComponentIslands;
// The online globe's globes and their city flags (untranslated; the globe's destructor deletes them through their virtual
// destructors, slot 8 of their vtables, so only that is declared).
class CLBMeshInstance;
class CLBScreenComponentOnlineGlobe;
class CLBGlobe {
public:
    virtual ~CLBGlobe();
    // Unit v5923 (Mac object 0x18 bytes, vtable 0x1029e8e8; Show makes the one globe).
    CLBMeshInstance* mMesh;                    // 0x04
    CLBScreenComponentOnlineGlobe* mScreen;    // 0x08
};
class CFSOnlineCity;
class CLBGlobeCityFlag;
// Unit v5546: the two virtuals of the online globe the flag calls (vtable slots 0x40 add, 0x44 remove; the slots
// 0x30..0x3c in between are other virtuals, not declared here).
class CLBGlobeFlagOwner {
public:
    virtual void AddFlag(CLBGlobeCityFlag* flag) = 0;      // slot 0x40
    virtual void RemoveFlag(CLBGlobeCityFlag* flag) = 0;   // slot 0x44
};
extern char16_t gGlobeNoCityName[];   // 0x1023e91e: the wide string the city's name is compared with
class CLBGlobeCityFlag {
public:
    // Unit v5546: 0x100ac5c8 (580). Mac object 0x10 bytes, vtable 0x1029e8d4.
    CLBGlobeCityFlag(const CLBScreenComponentOnlineGlobe* globe, const CFSOnlineCity* city);
    CLBGlobeCityFlag() = default;      // derived classes of other units need it
    virtual ~CLBGlobeCityFlag();       // 0x100ac86c
    CLBMeshInstance* mMesh;                       // 0x04
    const CLBScreenComponentOnlineGlobe* mGlobe;  // 0x08
    const CFSOnlineCity* mCity;                   // 0x0c
};

// The lobby's description: the numbers of "data:lobby:lobby.txt" (src/lobby_parser.cpp).
// Mac 0x6c bytes, vtable 0x1029e3f0 (the destructor alone). Nothing sets the fields but
// the file's lines.
class CLBLobbyGlobals {
public:
    virtual ~CLBLobbyGlobals();       // 0x100a6180
    void ParseLobbyDescriptor();      // 0x100b55b8
    float mIslandFov;                 // 0x04  ISLANDFOV(
    float mSpinSpeed;                 // 0x08  SPINSPEED(
    float mSpinRadius;                // 0x0c  SPINRADIUS(
    float mVerticalOffset;            // 0x10  VERTICALOFFSET(
    float mGlobeRadiusOut;            // 0x14  GLOBERADIUSOUT(
    float mGlobeRadiusIn;             // 0x18  GLOBERADIUSIN(
    float mCameraX[10];               // 0x1c  ISLANDCAMERAPOSITION(index,x,y)
    float mCameraY[10];               // 0x44
};
// 0x100b5c78: the text at *cursor up to `delimiter`; the cursor moves past the delimiter.
TbDynamicStringTemplate<char> LB_ReadTbStringUntilNext(char** cursor, char delimiter);
extern TbDynamicStringTemplate<char> gLobbyFolder;           // 0x102e4978 "data:lobby:"
extern TbDynamicStringTemplate<char> gLobbyDescriptorFile;   // 0x102e4958 "lobby.txt"

// Mac 0x38 bytes; the vtable (0x1029e3d8) has Register and UnRegister only.
class CLBLobby {
public:
    CLBLobby();                 // 0x100a57e4
    ~CLBLobby();                // 0x100a58c8 (not virtual): destroys the systems still registered
    // 0x100a5b40: the window, the description, the four systems, every system's Init, then
    // the screen the lobby opens on. The result is the state, as for the next three.
    int32_t Init();
    int32_t Tick();             // 0x100a5e8c: the time step, then every system's Tick
    int32_t Render();           // 0x100a5f78: every system's Render
    // 0x100a6004: every system's Close, the four systems and the description destroyed.
    int32_t Close();
    void DeleteSystemList();    // 0x100a61f8
    // 0x100aa98c (untranslated): the lobby's random numbers. seed = seed * 0x19660d + 0x3c6ef35f on
    // the word at 0x34 (mStarted, which time() seeded), and the new seed is the answer.
    uint32_t GetRand();
    virtual void Register(CLBLobbySystem* system);     // 0x100a634c
    virtual void UnRegister(CLBLobbySystem* system);   // 0x100a63ac
    // 0x100a6484 (src/lobby_flyingmesh.cpp): the lobby window's messages. 0x10003 sets the size, 0x10004 and 0x10005 clear
    // and set the bits (1, 2, 4) of the word at 0x1c, 0x22 clears that word, 0x14 forgets the window (when a lobby is
    // set), 0x11 with 1 updates the outbox's mail icon; then every system gets the message and the window's default
    // handler has the last word (its answer is the result).
    int32_t WindowMessagesFn(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
    // unit w47, translated by unit w48 (src/lobby_sendquit.cpp): 0x100a643c, 32 bytes. Ignores `this`: sets gGame's word at 0x18 (mRestart)
    // to 1 and the word at 0x14 (mState) of gLobby to 3.
    void SendQuit();

    // Unit v51060 (src/MapDelta_map_CMapCell_IsQueue.cpp): 0x100a66b0 (12 bytes), mF1C and the flag (the enum ELBKeyDownFlag is a bit mask).
    int32_t IsKeyDown(uint32_t flag);

    InterfaceWindow* mWindow;                // 0x04
    float mTickUnits;                        // 0x08  the last step: its milliseconds / 100 (not set by the constructor)
    int32_t mF0C;                            // 0x0c
    CLBLobbyGlobals* mGlobals;               // 0x10
    int32_t mState;                          // 0x14  1 while the lobby runs; what Close reports (2: a park was chosen)
    TbPtrList<CLBLobbySystem>* mSystems;     // 0x18
    int32_t mF1C;                            // 0x1c  bits 1, 2, 4: set by window message 0x10005 and cleared by 0x10004 (argument 0, 1, 2), all cleared by 0x22
    int32_t mWidth;                          // 0x20  0x400
    int32_t mHeight;                         // 0x24  0x300
    int32_t mLastTime;                       // 0x28
    int32_t mTime;                           // 0x2c
    uint32_t mTickTime;                      // 0x30  milliseconds, at most 500
    uint32_t mStarted;                       // 0x34  time() when it was made
    // Unit v5571 (stand-in): 0x100a66b0 (12), the lobby's key state test (flag 2: the right button).
    int32_t IsKeyDown(int32_t flag);
};
extern CLBLobby* gLobby;   // 0x102e4888, set while the lobby runs (original name unknown)

// A part of the lobby: it registers with the lobby (gLobby) when it is made and leaves it
// when it is destroyed. Vtable 0x1029e42c: the destructor, then five pure members.
class CLBLobbySystem {
public:
    CLBLobbySystem();                // 0x100a56c8
    virtual ~CLBLobbySystem();       // 0x100a5744
    virtual void Init() = 0;         // slot 0x0c
    virtual void Tick() = 0;         // slot 0x10
    virtual void Render() = 0;       // slot 0x14
    virtual void Close() = 0;        // slot 0x18
    virtual void WindowMessagesFn(int32_t message, int32_t a, int32_t b) = 0;   // slot 0x1c
};

// ---- the four systems (untranslated: the members the lobby object uses)
class CLBMeshType;
class CLBMeshInstance;
class CLBFlyingMeshGroup;

// A screen of the lobby. It registers with the screen system (gLobbyScreens) when it is
// made and leaves it when it is destroyed; the system passes Init, Tick, Render, Close, the
// window messages and "cancel" to the screens, the last four only to visible ones. Mac 8
// bytes; vtable 0x1029eb30 (src/lobby_screen.cpp).
class CLBScreenComponent {
public:
    CLBScreenComponent();                    // 0x100b6ba4
    virtual ~CLBScreenComponent();           // 0x100b6c2c
    virtual void Init() = 0;                 // slot 0x0c
    virtual void Tick() = 0;                 // slot 0x10
    virtual void Render() = 0;               // slot 0x14
    virtual void Close() = 0;                // slot 0x18
    virtual void WindowMessagesFn(int32_t message, int32_t a, int32_t b) = 0;   // slot 0x1c
    virtual uint32_t ProcessCancel() = 0;    // slot 0x20: nonzero when the screen dealt with it
    virtual void Show();                     // slot 0x24 (0x100b6cd0)
    virtual void Hide();                     // slot 0x28 (0x100b6d0c)
    virtual int32_t IsVisible();             // slot 0x2c (0x100a6fb8)
    int32_t mVisible;                        // 0x04
};

// ---- the concrete screens: declared with the members the screen system reaches (each class
// the virtual members its own vtable has). The islands are 0xe8 bytes (the online ones 0xec),
// the globes 0x7c, the advisor 0x2c. The advisor is translated (src/lobby_advisor.cpp) and the
// islands' set-up, list, Show and Hide (src/lobby_islands.cpp); the rest is not.
// One island on the lobby's map (src/lobby_islands.cpp). Mac 0x74 bytes, vtable 0x1029eaa4
// (the destructor alone). It is made by ParseThemeDescriptor from an ISLAND( line and
// registers itself with its screen; the lines that follow set its rain, lightning and sky
// colour. Two meshes stand for it: both are placed and turned alike.
class CLBIsland {
public:
    // 0x100afe88: the angle is in degrees; `position` is the island's place in the lobby (the
    // meshes stand at it with y = 0); the signs go to both meshes' loading.
    CLBIsland(const CLBScreenComponentIslands* owner, int32_t order, const char* name, const char* folder,
              const char* meshA, const char* meshB, float angle, const s3DPoint& position, const char16_t* title,
              const CFSOnlinePark* park, const char16_t* sign1, const char16_t* sign2);
    virtual ~CLBIsland();                    // 0x100b0108 (src/lobby_islands_rest.cpp): leaves its screen's list
    void Show();                             // 0x100b01c0 (src/lobby_islands_rest.cpp): both meshes on, once

    CLBMeshInstance* mMeshA;                 // 0x04  InstanceMeshType(folder, meshA, 0, 2, ...): given a random idle animation by Tick
    CLBMeshInstance* mMeshB;                 // 0x08  InstanceMeshType(folder, meshB, 0, 1, ...): plays animation 0 when the zoom starts
    CLBScreenComponentIslands* mOwner;       // 0x0c  the screen it is listed in
    int32_t mShown;                          // 0x10  set by Show (both meshes shown)
    float mAngle;                            // 0x14  radians: 0.017453292 * the angle given
    int32_t mSoundSet;                       // 0x18  SND_RegisterLocalLobbySounds(name): the row of gLobbyIslandSounds
    int32_t mSound1;                         // 0x1c  the sound handles of the island's two looping sounds (events 1, 2)
    int32_t mSound2;                         // 0x20
    int32_t mOrder;                          // 0x24  the list is kept in this order (lowest first)
    TbDynamicStringTemplate<char> mName;     // 0x28
    s3DPoint mPosition;                      // 0x3c
    TbDynamicStringTemplate<char16_t> mTitle;   // 0x48
    int32_t mLightning;                      // 0x5c  set from the LIGHTNING( line; Tick flashes at random while it is above 0
    int32_t mRain;                           // 0x60  set from the RAINY( line; nonzero adds to the rain
    uint32_t mSky[3];                        // 0x64  sky colour, 16.16: random until the SKYCOLOUR( line sets it; Tick copies the current island's into the view's sky target
    const CFSOnlinePark* mPark;              // 0x70  the online park on the island, if any
};

// The base of the theme islands and the online islands (src/lobby_islands.cpp). Mac 0xe8 bytes, vtable
// 0x1029ea1c; the derived classes are made by CLBScreenSystem::Init from this constructor with their
// own vtable put in afterwards. The camera has two modes: it orbits the current island (mCameraState,
// below) or, with no current island, flies a random path over the map (the Roam and Look fields).
// The meaning of the camera fields is read from Tick (0x100b0ab8) and CalcCamera (0x100b1bac).
class CLBScreenComponentIslands : public CLBScreenComponent {
public:
    CLBScreenComponentIslands();             // 0x100b023c
    ~CLBScreenComponentIslands() override;   // 0x100b08e4
    void Init() override;                    // 0x100b0a54
    void Render() override;                  // 0x100b1d94
    void Close() override;                   // 0x100b1dd0
    // The base class's Show and Hide (0x100b1fe0, 0x100b2174; slots 0x24, 0x28 of the base vtable): the
    // theme and online screens inline them in their own.
    void Show() override;
    void Hide() override;
    void DeleteIslandListEntries();          // 0x100b2374: destroys every island and empties the list
    // Slots 0x38 and 0x3c of the Mac vtable (both derived classes share these two); here they are
    // plain members, since only the islands call them.
    void Register(CLBIsland* island);        // 0x100b2420: in front of the first island with an order not below its own
    void UnRegister(CLBIsland* island);      // 0x100b2534: no effect when it is not listed
    void SetCurrentIsland(const CLBIsland* island);   // 0x100b25d0: not for NULL; the camera state goes back to 0
    // 0x100b0ab8, src/lobby_islands_tick.cpp: the islands' own per-frame work, slot 0x10 of the
    // vtable the base constructor installs. The theme and online islands' Ticks call it. It is
    // named TickIslands and not declared `Tick() override` because every unit library that
    // builds this class's vtable would then need a fake of it; the launcher's
    // CLBScreenComponentIslands::Tick (launcher/pending_manual.cpp) forwards to it.
    void TickIslands();
    // 0x100b1f3c (unit v51046): the base's ProcessCancel (slot 0x20). Named apart from ProcessCancel for the same reason as TickIslands.
    // Answers 0 when the camera is orbiting; otherwise sends the current island's second mesh back to animation 0 when zooming in (state 2),
    // sets the state to 0 and answers 1.
    uint32_t ProcessCancelIslands();
    void CalcCamera();                       // 0x100b1bac: the camera round the current island, into the lobby view
    void GotoNextIsland();                   // 0x100b2638 (the theme islands' 0x100b35ac replaces it in slot 0x40)
    void GotoPreviousIsland();               // 0x100b2808 (0x100b3684 in the theme islands)
    // Slot 0x50 of the vtable, called by Tick when the zoom's fade completes: the theme islands'
    // 0x100b346c is translated (src/lobby_islands_rest.cpp); the online islands' 0x100b45d0 is not,
    // so nothing happens for them. (Slot 0x54 is SetCurrentIsland above: the theme override
    // 0x100b3540 is a plain member of the theme class with the same text; the online override
    // 0x100b4690 is untranslated and not declared, so the base's runs for it.)
    virtual void EnterCurrentIsland() {}
    // 0x100b5d54 (src/lobby_islands_rest.cpp): reads the theme's lobby description file and makes its islands
    // and flying meshes. The gate names go to the islands' signs.
    void ParseThemeDescriptor(const char* theme, const CFSOnlinePark* park, int32_t order,
                              const char16_t* gate1, const char16_t* gate2);
    // 0x100b2228 (src/lobby_islands_rest.cpp): fades the current island's looping sounds and restarts the
    // sounds of the FIRST island in the list.
    void ResetIslandLoopingSounds();
    // Unit v51106 (src/CLBScreenComponentOnlineIslands_GotoNextIsland_CLBScreenComponentOnlineIslands_StartCurrentIsla.cpp).
    // 0x100b1e3c, slot 0x1c of the base vtable (named apart so no unit library needs it): the arrow and return keys and clicks, through slots 0x44, 0x40, 0x48.
    void WindowMessagesFnIslands(int32_t message, int32_t a, int32_t b);
    void Slot48();                           // slot 0x48 of the base vtable (body not translated)

    TbPtrList<CLBIsland>* mIslands;          // 0x08  in the order of CLBIsland::mOrder
    CLBIsland* mCurrent;                     // 0x0c  the island the camera is on (the nearest one while flying)
    CLBIsland* mLast;                        // 0x10  the island registered last: the description's lines after ISLAND( set its fields
    int32_t mCameraState;                    // 0x14  0 orbiting, 1 turning to the island's front, 2 zooming in (Tick)
    float mAngle;                            // 0x18  orbit angle in radians
    float mSpinSpeed;                        // 0x1c  taken from the lobby description (+8) by Tick
    float mRadius;                           // 0x20  the orbit radius (description +0xc); shrinks while zooming in
    float mHeight;                           // 0x24  the camera's height over the island (description +0x10)
    s3DPoint mCameraPos;                     // 0x28  the camera now (from the view's position), eased to the goal
    s3DPoint mCameraGoal;                    // 0x34  where it is going
    s3DPoint mLookAt;                        // 0x40  what it looks at now (from the view's target)
    s3DPoint mLookAtGoal;                    // 0x4c
    s3DPoint mRoamCentre;                    // 0x58  free flight: the middle of the box it flies in (500, 75, 500)
    s3DPoint mRoamExtent;                    // 0x64  the box's size (400, 50, 400)
    float mRoamSpeed;                        // 0x70  1.0, times the step
    float mRoamArriveDistSq;                 // 0x74  100.0: a goal this close (squared) is reached and a new one picked
    s3DPoint mRoamPos;                       // 0x78  the flight position, random in the box
    s3DPoint mRoamGoal;                      // 0x84  where it flies to, random in the box
    s3DPoint mRoamDir;                       // 0x90  the direction of flight: unit, towards the goal at first, then eased
    s3DPoint mRoamGoalDir;                   // 0x9c  the unit direction to the goal (Tick; not set by the constructor)
    float mLookMaxSpeed;                     // 0xa8  2.0
    float mLookSpeed;                        // 0xac  2.0 at first: speeds up to the maximum far from the goal, slows to 0 near it
    float mLookSlowDistSq;                   // 0xb0  50.0: the squared distance inside which it slows down
    s3DPoint mLookPos;                       // 0xb4  free flight's look-at point, random in the box
    s3DPoint mLookGoal;                      // 0xc0  where it looks next (random; the nearest island's position when there are islands)
    s3DPoint mLookDir;                       // 0xcc  unit, towards mLookGoal at first, then eased
    s3DPoint mLookGoalDir;                   // 0xd8  the unit direction to mLookGoal (Tick; not set by the constructor)
    uint32_t mFE4;                           // 0xe4  never read or written by the translated members
};
class CLBScreenComponentThemeIslands : public CLBScreenComponentIslands {
public:
    ~CLBScreenComponentThemeIslands() override;   // 0x100b4cec (the base destructor's text, inlined)
    void Tick() override;                    // 0x100b29dc
    void WindowMessagesFn(int32_t message, int32_t a, int32_t b) override;   // 0x100b2af8
    uint32_t ProcessCancel() override;       // 0x100b2c00
    void Show() override;                    // 0x100b2cc4
    void Hide() override;                    // 0x100b2fe4
    // Slots 0x30 and 0x34 of the Mac vtable (the online islands have their own); plain members here.
    void ShowPanels();                       // 0x100b3148: the island lobby's panels
    void HidePanels();                       // 0x100b31b0
    // Slot 0x50 (the base class's empty virtual): called by Tick when the zoom's fade completes. It is the only
    // one of the theme islands' slot overrides declared virtual here: nothing else reaches the others through
    // the base class (the Mac's vtable has them at 0x40..0x4c and 0x54; they are plain members here, hiding the
    // base class's of the same name, so that the unit libraries that build this class's vtable need one fake).
    void EnterCurrentIsland() override;      // 0x100b346c (src/lobby_islands_rest.cpp)
    void SetCurrentIsland(const CLBIsland* island);   // 0x100b3540, slot 0x54: the base class's text again
    void GotoNextIsland();                   // 0x100b35ac, slot 0x40: nothing in the easy game type
    void GotoPreviousIsland();               // 0x100b3684, slot 0x44
    void SelectCurrentIsland();              // 0x100b3218, slot 0x48: the zoom starts when the player can pay the keys
    void StartCurrentIslandZoom();           // 0x100b33e8, slot 0x4c
    void ResetCurrentIsland(int32_t a);      // 0x100b3760: the first island, or the one the player last entered
    void ResetAdvisorTimeout();              // 0x100b3854: mFE4 := the clock
    void UpdateSigns();                      // 0x100b38d4: the rides' signs (the player's, or the gates' names)
};
class CLBScreenComponentOnlineIslands : public CLBScreenComponentIslands {
public:
    ~CLBScreenComponentOnlineIslands() override;  // 0x100b4be0
    void Tick() override;                    // 0x100b3b28
    void WindowMessagesFn(int32_t message, int32_t a, int32_t b) override;   // 0x100b3bc8
    uint32_t ProcessCancel() override;       // 0x100b3cd0
    void Show() override;                    // 0x100b3ec8
    void Hide() override;                    // 0x100b4170
    // Unit v5255 (src/CLBScreenComponentOnlineIslands_GotoNextIsland.cpp). Plain members (the vtable slots 0x40, 0x48 and 0x50 etc. are not
    // overridden here, so no unit library needs a fake of them).
    // 0x100b46fc, slot 0x40: the next island, unless the camera is zooming; then the city lobby controller shows.
    void GotoNextIsland();
    void SelectCurrentIsland();              // 0x100b44ac, slot 0x48: opens the park info panel of the current island
    void EnterCurrentIslandOnline();         // 0x100b45d0, slot 0x50 (EnterCurrentIsland of the original): the online game type, state 2
    void DownloadFailFunction();             // 0x100b4a98
    void HandleOnlineBacktrack();            // 0x100b4b48: this->Hide(), then the download globe's Show
    uint8_t mFE8[4];                         // 0xe8  (read and written as an int32 by the members above: 1 while the controller shows)
    // Unit v5639 (src/CLBScreenComponentOnlineIslands_GotoNextIsland_CLBScreenComponentOnlineIslands_CreateParks.cpp)
    void CreateParks(TbPtrList<CFSOnlinePark>* parks);   // 0x100b4880: ParseThemeDescriptor for each park, then the first island is current
    // Unit v51106 (src/CLBScreenComponentOnlineIslands_GotoNextIsland_CLBScreenComponentOnlineIslands_StartCurrentIsla.cpp). Plain members here.
    void StartCurrentIslandZoom();           // 0x100b4574, slot 0x4c: the camera state goes to 1
    void SetCurrentIsland(const CLBIsland* island);   // 0x100b4690, slot 0x54: the base class's text again
    void GotoPreviousIsland();               // 0x100b4790, slot 0x44
    void HidePanels();                       // 0x100b4354, slot 0x34
    void ShowPanels();                       // 0x100b42d4, slot 0x30
    void ShowInfoOverlayPanel();             // 0x100b4828: the controller shows (mFE8 := 1)
};
// 0x100b3e28 (untranslated; its TVector is 0x10266778): the callback Show passes to CFSOnlineSystemParkList_InCityTop10::GetList.
void PostGetTop10ParksListCallbackFunction(int32_t id, const void* context, TbPtrList<CFSOnlinePark>* parks);
// The online globe's base class (0x7c bytes on the Mac, vtable 0x1029e830). The constructor (src/lobby_globe.cpp) makes the two lists
// (a failed allocation leaves the field NULL, as the Mac's operator new answers NULL) and sets nine floats; the words it does not write
// are kept as opaque arrays. The floats' names are the Mac offsets (their use is untranslated).
class CLBScreenComponentOnlineGlobe : public CLBScreenComponent {
public:
    CLBScreenComponentOnlineGlobe();             // 0x100ac90c
    ~CLBScreenComponentOnlineGlobe() override;   // 0x100aca4c (src/lobby_globe_close.cpp)
    void Render() override;                  // 0x100ad4cc
    void Close() override;                   // 0x100ad50c: nothing
    uint32_t ProcessCancel() override;       // 0x100ad690
    void DeleteGlobeListEntries();           // 0x100adf48: removes and deletes every globe
    void DeleteCityListEntries();            // 0x100ae104: the same for the city flags
    TbTList<CLBGlobe*, CLBGlobe*>* mGlobes;                // 0x08  made by the constructor (vtable 0x1029e8c0)
    uint8_t mF0C[0x1c - 0x0c];
    float mF1C;                                            // 0x1c  0.4
    float mF20;                                            // 0x20  0.15
    float mF24;                                            // 0x24  0.5
    float mF28;                                            // 0x28  0.2
    float mF2C;                                            // 0x2c  0.075
    float mF30;                                            // 0x30  0.25
    uint8_t mF34[4];                                       // 0x34  not written by the constructor
    float mF38;                                            // 0x38  300
    float mF3C;                                            // 0x3c  150
    float mF40;                                            // 0x40  1.5
    uint8_t mF44[0x6c - 0x44];
    TbTList<CLBGlobeCityFlag*, CLBGlobeCityFlag*>* mCityFlags;   // 0x6c  made by the constructor (vtable 0x1029e898)
    int32_t mF70;                                          // 0x70  cleared by the download and publish globes' Init
    int32_t mF74;                                          // 0x74  0
    int32_t mF78;                                          // 0x78  0
    // Unit v5226 (untranslated; the world lobby window's arrow keys and zoom).
    void GlobeLeft();      // 0x100adb54
    void GlobeRight();     // 0x100adba8
    void GlobeUp();        // 0x100adbfc
    void GlobeDown();      // 0x100adc50
    void GlobeZoomIn();    // 0x100adca4
    void GlobeZoomOut();   // 0x100add2c
    // Unit v5923.
#ifdef TPW_GLOBE_SHOWHIDE_REAL
    void Show() override;                  // 0x100ad788
    void Hide() override;                  // 0x100ada68
#else
    // Units that do not link the translation get the base behaviour (inline, weak); the real ones win at link.
    void Show() override { CLBScreenComponent::Show(); }   // 0x100ad788
    void Hide() override { CLBScreenComponent::Hide(); }   // 0x100ada68
#endif
    void SortOutZoomConstants();           // 0x100addb4
    void CreateCities(TbPtrList<CFSOnlineCity>* cities);   // 0x100ade44
    void UnRegister(CLBGlobeCityFlag* flag);               // 0x100ae228
    // Slots 0x38 and 0x48 (the download and publish globes override them; inline defaults keep those classes buildable).
    virtual void RegisterGlobe(CLBGlobe* globe) {}         // slot 0x38
    virtual int32_t IsCityWanted(CFSOnlineCity* city) { return 0; }   // slot 0x48
    // The Mac words at 0x0c (the globe made by Show), 0x10..0x18 (the camera constants: the host keeps them in
    // mF0C + 4, + 8, + 12), 0x34, and 0x44..0x68 (inside mF44) are reached through mF0C, mF34 and mF44.
    CLBGlobe* mGlobeObj;                   // 0x0c (host: a member of its own, the pointer is 8 bytes)
    // Unit v5571: 0x100acce0 (1968) and 0x100ad54c (252). The Mac words Tick uses are inside mF0C, mF34 and mF44 (above):
    // 0x10 the tick scale (mF0C + 4), 0x18 the damping (mF0C + 12), 0x34 the size now (mF34), 0x44..0x50 the spins and their
    // speeds, 0x54 the radius, 0x58 zoomed in, 0x5c..0x64 the hit point, 0x68 picking (mF44 + 0 .. + 0x24); mF70 the flag
    // under the cursor (an arena offset, as the download and publish globes read it).
    void Tick() override;
    void WindowMessagesFn(int32_t message, int32_t a, int32_t b) override;
    // Unit v51031: slot 0x3c, called by ~CLBGlobe on its screen with the globe (name unknown; the base does nothing).
    virtual void UnRegisterGlobe(CLBGlobe* globe) {}
    // Unit v51106.
    void TickGlobe();                      // 0x100acce0: the globe's own Tick, which the download and publish globes' Ticks call
    void Register(CLBGlobeCityFlag* flag); // 0x100ae1b0: at the end of the city flag list
    // Unit v51119 (src/lobby_globe_close_CLBScreenComponentOnlineGlobe_Show_2_CLBScreenComponentOnlineGlobe_GlobeRight.cpp).
    // The camera step is the Mac float at 0x14 (host mF0C + 8); the offsets are the Mac float at 0x48 (mF44 + 4) and 0x50 (mF44 + 12).
#ifdef TPW_GLOBE_INIT_REAL
    void Init() override;                      // 0x100acc98: clears 0x70
#else
    void Init() override { mF70 = 0; }         // 0x100acc98 (units that do not link the translation)
#endif
    void Register(CLBGlobe* globe);            // 0x100adff4: the globe list's AddTail
    void UnRegister(CLBGlobe* globe);          // 0x100ae064: Find, then RemoveAt
};
void HideWorldLobbySats();   // 0x1022b85c
void ShowWorldLobbySats();   // 0x1022b7e8
// Unit v5571: the globe's sphere centre (0x1029e71c) and the view's target (0x1029e728), 12 bytes each, defined in
// src/lobby_globe_close_CLBScreenComponentOnlineGlobe_Tick.cpp.
extern s3DPoint gGlobeCentre;       // 0x1029e71c
extern s3DPoint gGlobeViewTarget;   // 0x1029e728
void WorldLobbySetZoomButton(int32_t zoomedIn);   // 0x1022ba30 (stand-in)
class CLBScreenComponentDownloadGlobe : public CLBScreenComponentOnlineGlobe {
public:
    ~CLBScreenComponentDownloadGlobe() override;  // 0x100af330
    void Init() override;                    // 0x100ae2d0
    void Tick() override;                    // 0x100ae318
    void WindowMessagesFn(int32_t message, int32_t a, int32_t b) override;   // 0x100ae374
    uint32_t ProcessCancel() override;       // 0x100ae46c
    void Show() override;                    // 0x100ae590
    void Hide() override;                    // 0x100ae63c
    virtual void Slot50() {}                 // slot 0x50 (unit v5130: EnterChatSatellite calls it on the lobby's download globe; body unknown)
    // Unit v5957 (src/lobby_globe_CLBScreenComponentPublishGlobe_WindowMessagesFn.cpp).
    void HandleOnlineBacktrack();            // 0x100ae9c0: this->Hide(), then the theme islands' Show
    void CreateCities(TbPtrList<CFSOnlineCity>* cities);   // 0x100ae8a0: a flag per wanted city, then the advisor says line 0x256
    // Unit v51106 (plain members; the Mac has them in vtable slots).
    int32_t IsCityValid(const CFSOnlineCity* city);   // 0x100aea58: the city's value is above 0
    void HidePanels();                       // 0x100ae828
    void ShowPanels();                       // 0x100ae7b0
};
class CLBScreenComponentPublishGlobe : public CLBScreenComponentOnlineGlobe {
public:
    ~CLBScreenComponentPublishGlobe() override;   // 0x100af1c8
    void Init() override;                    // 0x100aeac8
    void Tick() override;                    // 0x100aeb10
    void WindowMessagesFn(int32_t message, int32_t a, int32_t b) override;   // 0x100aeb6c
    void Show() override;                    // 0x100aec3c
    void Hide() override;                    // 0x100aece8
    // Unit v5957 (src/lobby_globe_CLBScreenComponentPublishGlobe_WindowMessagesFn.cpp).
    void CreateCities(TbPtrList<CFSOnlineCity>* cities);   // 0x100aef90
    void HandleOnlineBacktrack();            // 0x100af094: this->Hide(), then the theme islands' Show and EnterCurrentIsland
    // Unit v51106 (plain members; the Mac has them in vtable slots).
    int32_t IsCityValid(const CFSOnlineCity* city);   // 0x100af148
    void HidePanels();                       // 0x100aef20
    void ShowPanels();                       // 0x100aeeb0
};
// 0x100ad54c (untranslated; a plain function here so that the online globe's vtable is not changed): the base class's
// window messages, which the publish globe's WindowMessagesFn calls first.
void CLBScreenComponentOnlineGlobe_WindowMessagesFn(CLBScreenComponentOnlineGlobe* globe, int32_t message, int32_t a, int32_t b);
// The advisor's screen (src/lobby_advisor.cpp): a queue of lines for the advisor to say,
// newest first, said one a tick from the old end while he is silent, and a line to say when
// nothing has been said for a while. Mac 0x2c bytes, vtable 0x1029e474. The constructor is
// inline in CLBScreenSystem::Init (the base's, then an empty list).
class CLBScreenComponentAdvisor : public CLBScreenComponent {
public:
    ~CLBScreenComponentAdvisor() override;   // 0x100a7234
    void Init() override;                    // 0x100a6c98
    void Tick() override;                    // 0x100a6ce8
    void Render() override;                  // 0x100a6e10: nothing
    void Close() override;                   // 0x100a6e4c
    void WindowMessagesFn(int32_t message, int32_t a, int32_t b) override;   // 0x100a6ea8: nothing
    uint32_t ProcessCancel() override;       // 0x100a6ef0: 0
    void Show() override;                    // 0x100a6f38
    void Hide() override;                    // 0x100a6ff4
    void Add(AdvisorResponse response, int32_t clearFirst);                  // 0x100a7074
    void ClearAll();                         // 0x100a7128
    void SetTimeoutResponse(AdvisorResponse response, uint32_t milliseconds);   // 0x100a71c8

    TbPtrList<AdvisorResponse> mResponses;   // 0x08  (count at 0x14)
    int32_t mHasTimeout;                     // 0x18
    uint32_t mTimeout;                       // 0x1c  milliseconds of silence before the timeout line
    uint32_t mLastTime;                      // 0x20
    AdvisorResponse mTimeoutResponse;        // 0x24  (not set by Init)
    int32_t mSpoke;                          // 0x28  the last tick found him talking or gave him a line
};

// ---- the lobby's meshes (src/lobby_meshes.cpp). The lobby draws with the park's own
// machinery: its ground is the height field of the level "lobby", and everything on it is
// an instance of a "ride type" (a mesh file of a folder).
// A mesh file loaded as a ride type. Mac 0x2c bytes, no vtable; made and destroyed inline
// (CLBLobbyMeshes::InstanceMeshType and DeleteMeshTypes).
class CLBMeshType {
public:
    // Loads the type (number 18000 + `index`) and registers with gLobbyMeshes.
    CLBMeshType(const char* folder, const char* name, int32_t overMapRange, uint32_t a, const char16_t* sign1,
                const char16_t* sign2, uint32_t index);
    ~CLBMeshType();                          // leaves gLobbyMeshes
    uint32_t mRideType;                      // 0x00  LoadRideType's result
    TbDynamicStringTemplate<char> mFolder;   // 0x04
    TbDynamicStringTemplate<char> mName;     // 0x18
};
// One mesh in the lobby: an instance of a type, not drawn until Show. Mac 0x34 bytes,
// vtable 0x1029e5f8 (the destructor alone). The constructor is inline in InstanceMeshType.
class CLBMeshInstance {
public:
    CLBMeshInstance(CLBMeshType* type, const char* folder, const char* name);
    virtual ~CLBMeshInstance();              // 0x100a7794: leaves gLobbyMeshes, destroys the ride instance
    void Show();                             // 0x100a7860
    void SetPosition(const s3DPoint& position);    // 0x100a790c
    // 0x100a798c: turns the mesh to face along `direction` (not normalised here), upright.
    void SetDirection(const s3DPoint& direction);
    void RotateY(float angle);               // 0x100a7c0c
    void Scale(float x, float y, float z);   // 0x100a7c90
    void PlayAnim(int32_t anim, int32_t loop);     // 0x100a7d28
    uint32_t GetAnimTimeRemaining();         // 0x100a7df0
    void SetRideSigns(const char16_t* sign1, const char16_t* sign2);   // 0x100a7e84

    uint32_t mInstance;                      // 0x04  InstanceRide's result: indexes gRideAnimSets
    int32_t mShown;                          // 0x08
    TbDynamicStringTemplate<char> mFolder;   // 0x0c
    TbDynamicStringTemplate<char> mName;     // 0x20
};
class CLBLobbyMeshes : public CLBLobbySystem {   // Mac 0x10 bytes, vtable 0x1029e570
public:
    CLBLobbyMeshes();                // 0x100a7efc: loads the level "lobby"'s height field, hidden
    ~CLBLobbyMeshes() override;      // 0x100a802c
    void Init() override;            // 0x100a81ac: these five do nothing
    void Tick() override;            // 0x100a81dc
    void Render() override;          // 0x100a820c
    void Close() override;           // 0x100a8240
    void WindowMessagesFn(int32_t message, int32_t a, int32_t b) override;   // 0x100a8270
    virtual void Register(CLBMeshType* type);            // 0x100a8948  slot 0x20
    virtual void UnRegister(CLBMeshType* type);          // 0x100a89ac  slot 0x24
    virtual void Register(CLBMeshInstance* instance);    // 0x100a8a40  slot 0x28
    virtual void UnRegister(CLBMeshInstance* instance);  // 0x100a8aa8  slot 0x2c
    void ShowHeightfield();          // 0x100a82b0
    void HideHeightfield();          // 0x100a835c
    // 0x100a8408: a new, hidden instance of the mesh `name` of `folder`; its type is loaded
    // the first time (names compared without case). `overMapRange` and `a` go to the type's
    // loading, as the two sign texts do.
    CLBMeshInstance* InstanceMeshType(const char* folder, const char* name, int32_t overMapRange, uint32_t a,
                                      const char16_t* sign1, const char16_t* sign2);
    void DeleteMeshTypes();          // 0x100a86fc: every type, the list itself, then every ride type
    void DeleteMeshInstanceListEntries();   // 0x100a88a4

    int32_t mHeightfieldShown;                   // 0x04
    TbPtrList<CLBMeshType>* mTypes;              // 0x08
    TbPtrList<CLBMeshInstance>* mInstances;      // 0x0c
};
class CLBLobbyView : public CLBLobbySystem {     // Mac 100 bytes, vtable 0x1029e53c
public:
    CLBLobbyView();                  // 0x100a8b40
    ~CLBLobbyView() override;        // 0x100a8bb0
    void Init() override;            // 0x100a8c44
    void Tick() override;            // 0x100a8e18
    void Render() override;          // 0x100a8ed8
    void Close() override;           // 0x100a8f08
    void WindowMessagesFn(int32_t message, int32_t a, int32_t b) override;   // 0x100a8fc8
    void MoveView();                    // 0x100a9004
    void SetGlobalAmbientLight(int32_t enabled, uint32_t color); // 0x100a9428
    void SetGlobalDirectionalLight(int32_t enabled, const uint32_t* color,
                                   const s3DPoint* normal, int32_t followsView); // 0x100a94f8
    void SetFogOn();                    // 0x100a992c
    void SetFogOff();                   // 0x100a9970
    void SetFogSettings(uint32_t color, float nearDistance, float farDistance); // 0x100a99b4
    void UpdateSky();                   // 0x100a9a40
    void UpdatedFade();                 // 0x100a9b64

    float mFov;                        // 0x04
    s3DPoint mPosition;                // 0x08
    s3DPoint mTarget;                  // 0x14
    class CLight* mLight;              // 0x20, only member initialised by the constructor
    int32_t mLightFollowsView;         // 0x24
    float mLightUpOffset;              // 0x28
    float mLightRightOffset;           // 0x2c
    int32_t mLightDownward;            // 0x30
    uint32_t mSavedDisplayFlags;       // 0x34
    uint32_t mSavedFogColor;           // 0x38
    float mSavedFogNear, mSavedFogFar;  // 0x3c, 0x40
    int32_t mSkyCurrent[3];            // 0x44, fixed-point RGB
    int32_t mSkyTarget[3];             // 0x50
    int32_t mFadeCurrent, mFadeTarget;  // 0x5c, 0x60
};
// A flock of meshes that fly about over the lobby's islands (src/lobby_flyingmesh.cpp). Mac 0x5c bytes,
// vtable 0x1029e69c (destructor 8, Register 0xc, UnRegister 0x10). It is made by the theme
// description's FLYINGMESH( lines, registers with the flying mesh system (gLobbyFlyingMeshes) and
// then makes its instances a few at a time, one tick after another, up to a share of `number`
// (the LOBBYOBJECTS option, in per cent, at least 1).
class CLBFlyingMeshInstance;
class CLBFlyingMeshGroup {
public:
    // 0x100aae1c: `number` flying meshes of the mesh `name` of `folder`, flying about a box that is
    // `direction` big (the description's dx, dy, dz) round `position`; `speed` is the description's
    // last number, `limit` is always 500 (the squared distance at which an instance has reached its goal).
    CLBFlyingMeshGroup(int32_t number, const char* folder, const char* name, const s3DPoint& position,
                       const s3DPoint& direction, float speed, float limit);
    virtual ~CLBFlyingMeshGroup();                               // 0x100ab008: leaves the system, destroys the instances
    virtual void Register(CLBFlyingMeshInstance* instance);      // 0x100ab464, slot 0x0c: at the end of the list
    virtual void UnRegister(CLBFlyingMeshInstance* instance);    // 0x100ab4d8, slot 0x10: no effect when it is not listed
    void Show();                     // 0x100ab2fc: shows the instances there are, and every later one when it is made
    void Tick();                     // 0x100ab1a8: makes instances (at random) up to the maximum, then ticks them all
    void DeleteFlyingMeshInstanceListEntries();   // 0x100ab3b4: destroys every instance in the list

    int32_t mShown;                  // 0x04  set by Show
    int32_t mMax;                    // 0x08  number x the option / 100, at least 1 and at most `number` (but 1 for number < 1)
    int32_t mCount;                  // 0x0c  the instances made so far
    TbDynamicStringTemplate<char> mFolder;   // 0x10
    TbDynamicStringTemplate<char> mName;     // 0x24
    s3DPoint mPosition;              // 0x38  the middle of the box the instances fly in
    s3DPoint mExtent;                // 0x44  its size (the constructor's `direction`)
    float mSpeed;                    // 0x50
    float mArriveDistSq;             // 0x54  500.0
    TbPtrList<CLBFlyingMeshInstance>* mInstances;   // 0x58
};
// One flying mesh (src/lobby_flyingmesh.cpp). Mac 0x60 bytes, vtable 0x1029e6e0 (the destructor alone).
// It flies from a random point of the group's box to another one, turning towards the goal a tenth
// of the way a step, and picks a new goal when it is near enough. Its mesh is an instance of the
// lobby's mesh system, which keeps it (the destructor does not destroy it).
class CLBFlyingMeshInstance {
public:
    // 0x100aa5d4: the group, which it joins; `position` and `direction` are the group's box.
    CLBFlyingMeshInstance(const CLBFlyingMeshGroup* group, const char* folder, const char* name, const s3DPoint& position,
                          const s3DPoint& direction, float speed, float arrival);
    virtual ~CLBFlyingMeshInstance();        // 0x100aa9dc: leaves its group
    void Tick();                             // 0x100aaa80

    CLBFlyingMeshGroup* mGroup;              // 0x04
    CLBMeshInstance* mMesh;                  // 0x08  hidden until the group is shown
    int32_t mShown;                          // 0x0c
    s3DPoint mCentre;                        // 0x10  the box
    s3DPoint mExtent;                        // 0x1c
    float mSpeed;                            // 0x28
    float mArriveDistSq;                     // 0x2c
    s3DPoint mPosition;                      // 0x30  now
    s3DPoint mGoal;                          // 0x3c  where it is flying to
    s3DPoint mDirection;                     // 0x48  unit, turned towards the goal
    s3DPoint mGoalDirection;                 // 0x54  unit, towards the goal (Tick; the constructor does not set it)
};
class CLBFlyingMeshSystem : public CLBLobbySystem {   // Mac 8 bytes, vtable 0x1029e644
public:
    CLBFlyingMeshSystem();           // 0x100ab578 (src/lobby_flyingmesh.cpp)
    ~CLBFlyingMeshSystem() override; // 0x100ab620: destroys the groups
    void Init() override;            // 0x100ab790
    void Tick() override;            // 0x100ab7c4
    void Render() override;          // 0x100ab83c
    void Close() override;           // 0x100ab874: nothing
    void WindowMessagesFn(int32_t message, int32_t a, int32_t b) override;   // 0x100ab8ac: nothing
    // Slots 0x20 and 0x24 of the vtable (src/lobby_flyingmesh.cpp): the groups register when they are made.
    virtual void Register(CLBFlyingMeshGroup* group);      // 0x100ab99c: at the end of the list
    virtual void UnRegister(CLBFlyingMeshGroup* group);    // 0x100aba0c: no effect when it is not listed
    void DeleteFlyingMeshGroupListEntries();     // 0x100ab8f0: destroys every group in the list
    TbPtrList<CLBFlyingMeshGroup>* mGroups;      // 0x04
};
// The screens' owner (src/lobby_screen.cpp). Mac 0x1c bytes, vtable 0x1029ead8 (after the
// system members: Register 0x20, UnRegister 0x24).
class CLBScreenSystem : public CLBLobbySystem {
public:
    CLBScreenSystem();               // 0x100b6d48: an empty list; the five pointers are not set
    ~CLBScreenSystem() override;     // 0x100b6dec
    void Init() override;            // 0x100b6e8c: makes the five screens, then every screen's Init
    void Tick() override;            // 0x100b702c: the visible screens
    void Render() override;          // 0x100b70d0: the visible screens
    void Close() override;           // 0x100b7178: every screen's Close, then the five destroyed
    void WindowMessagesFn(int32_t message, int32_t a, int32_t b) override;   // 0x100b72d0
    virtual void Register(CLBScreenComponent* screen);     // 0x100b7600
    virtual void UnRegister(CLBScreenComponent* screen);   // 0x100b766c
    // 0x100b73c4: asks the visible screens; the main menu when none of them takes it.
    void ProcessCancel();
    void DeleteScreenSystemList();   // 0x100b7498

    CLBScreenComponentThemeIslands* mThemeIslands;       // 0x04  what the lobby opens on
    CLBScreenComponentOnlineIslands* mOnlineIslands;     // 0x08
    CLBScreenComponentDownloadGlobe* mDownloadGlobe;     // 0x0c  opened instead when the lobby's "return" flag is set
    CLBScreenComponentPublishGlobe* mPublishGlobe;       // 0x10  opened when the online system says so
    CLBScreenComponentAdvisor* mAdvisor;                 // 0x14  always opened
    TbPtrList<CLBScreenComponent>* mComponents;          // 0x18
};
void CreateMainMenu(int32_t a);      // 0x101fc05c (ui_mainmenu.cpp, untranslated)
extern CLBScreenSystem* gLobbyScreens;         // 0x102e49b8 (the four: original names unknown)
extern CLBFlyingMeshSystem* gLobbyFlyingMeshes;   // 0x102e48d4
extern CLBLobbyView* gLobbyView;               // 0x102e48cc
extern CLBLobbyMeshes* gLobbyMeshes;           // 0x102e48d0
// The Mac has ONE global here: 0x10395c24 is the interface's root window (gUIRootWindow, UI_Base.h), under which the lobby builds
// its window when the interface has made one. gLobbyWindowDefinition is that same variable (unit w36 removed the separate NULL one).
extern InterfaceWindow* gUIRootWindow;   // 0x10395c24, src/UI_focus.cpp
inline InterfaceWindow*& gLobbyWindowDefinition = gUIRootWindow;
extern int32_t gLobbyReturnFlag;               // 0x102e4860: nonzero opens the screen at 0x0c, once
extern int16_t gLobbyWindowData[];             // 0x1029e382 (src/lobby.cpp, unit w36): InterfaceWindow::Build's description, 38 words
extern int16_t gLoginPanelWindowData[];        // 0x102abbb0 (src/ui_loginpanel_data.cpp, unit w36): the login panel's description, 371 words
// ---- the login panel (src/ui_loginpanel.cpp, unit w39; the file is ui_playerLogin.cpp)
// 0x101ae9e0: asks who is playing. Disables the key tables, hides the lobby's window, builds gLoginPanelWindowData under the root window
// with LoginPanelMsg, styles the four name rows and the quit row (title text objects, font 5, centred, the names' colour), fills the names
// and tells the advisor what to say: lines 0x186 and 0x187 when no player is saved yet (and gLoginPanelFirstPlayer is set), 0x18e when
// there are some.
void CreateLoginPanel();
void FillLoginNames();                         // 0x101ae39c: the saved players' names into the rows (an empty row says string 0x1a5 and hides its button and picture)
// The panel's message functions (untranslated): the panel's own, each name row's and the quit row's.
int32_t LoginPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101add48 (the TVector at 0x10267ce0)
int32_t EntryMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);        // 0x101ade94 (0x10267cd0)
int32_t QuitMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);         // 0x101ae218 (0x10267cc0)
// The panel and whether it was opened for a first player (names invented; RemoveLoginPanel 0x101af02c and the message functions read them).
extern InterfaceWindow* gLoginPanelWindow;     // 0x10394abc: NULL until CreateLoginPanel has built it
extern int32_t gLoginPanelFirstPlayer;         // 0x10394ab8: 1 when no player was saved yet
// The names' colour (names invented): three signed bytes at 0x102ac004 to 0x102ac006 (red 0xec, green 0xa2, blue 0x04; the byte after
// them is 0xff). CreateLoginPanel and LoginPanelMsg pass them to SetFontColour sign-extended.
extern int8_t gLoginNameRed;                   // 0x102ac004
extern int8_t gLoginNameGreen;                 // 0x102ac005
extern int8_t gLoginNameBlue;                  // 0x102ac006
// 0x100b69f8 (src/lobby_islands_rest.cpp): like LB_ReadTbStringUntilNext, into a wide string; each char is
// widened with its sign.
TbDynamicStringTemplate<char16_t> LB_ReadTbUniStringUntilNext(char** cursor, char delimiter);

// The lobby window's message function (0x100a5950) and its mail icon's (0x100a59ec).
int32_t LB_Lobby_WindowMessagesFn(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// 0x100a59ec (src/lobby_mailicon.cpp, unit w40). On message 0x10006 (a click on the icon) it makes the online system when there is none,
// then opens the outbox panel when the player is logged in and otherwise a dialog with the interface string 0x1d9 (white, one button,
// no callbacks); every message, that one included, ends in the window's default handler, whose answer is returned.
int32_t LB_Lobby_MailIconMessagesFn(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// 0x101e4f28 (untranslated, 952 bytes; the inventory files it with the online mail panels): the outbox panel. It answers 1 once it has
// built the panel and 0 when it has not (the user info word at 0x8c is 0); CFSOnlineSystem::CompleteLogin asks (unit w69).
int32_t CreateOnlineOutboxPanel();
void UpdateOutboxMailIcon();                   // 0x101e3698 (src/UI_IslandLobby.cpp, unit w32): shows the mail icon when the outbox has mail

// Unit v5017 (src/UI_IslandLobby_CalcEmailAddressScrollBar.cpp): the online outbox panel's helpers.
extern InterfaceWindow* gOutboxWindow;         // 0x10395e3c: the outbox panel's window
extern uint8_t* gOutboxCursor;                 // 0x10395e38: the outbox's cursor part (the outbox + 4); read by Mac offsets
extern uint32_t gOutboxPreview[9];             // 0x10395e10: the preview's size: width at +0x1c (word 7), height at +0x20 (word 8)
extern const char16_t gEmailSeparators[3];     // 0x1024eeca: an empty string, then (at +2) "\n"
void FillOutboxList();                         // 0x101e378c
// Unit v5096 (src/LCPostFormat_2.cpp): the outbox panel's row functions (LCFormat and LCPostFormat of ui_onlineOutbox.cpp) and its
// untranslated message functions and window description.
InterfaceWindow* OutboxLCFormat();             // 0x101e4bdc
void OutboxLCPostFormat(InterfaceWindow* window);   // 0x101e4d20
int32_t OutboxPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);        // 0x101e458c (untranslated)
int32_t AdressesTextPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);  // 0x101e4b00 (untranslated)
extern int16_t gOutboxPanelData[];             // 0x102b05c4 (name invented): the panel's window description
void SetCurrentEmailInfo(int32_t number);      // 0x101e3960
void CalcEmailAddressScrollBar();              // 0x101e3aac
void UpdatePreview();                          // 0x101e3c94
void read_JPEG_file(char* path, uint8_t* pixels);   // 0x101e538c (unit v5981)
int32_t read_JPEG_fileChecked(char* path, uint8_t* pixels);   // unit v5981: the same, answering 1 when decoded, 0 when the file did not open or libjpeg failed
// Unit v5239 (src/UI_IslandLobby_CalcEmailAddressScrollBar_OutboxSendingMsg_OutboxPanelMsg.cpp)
void SendAndDeleteOutbox(bool send);           // 0x101e4378: empties the outbox's lists, sends when `send`
void ColumnClick(int32_t column);              // 0x101e3e98, untranslated
extern uint32_t gOutboxCallbackVector[2];      // 0x10268500: the TVector of OutboxCallback (0x101e4124, untranslated); the outbox keeps its address
// Unit v5579 (src/LCPostFormat_2_ChatPrePostCard.cpp)
extern InterfaceWindow* gOnlineWord80;         // 0x10395e80, original name unknown: not 0 wins in ChatPrePostCard
extern InterfaceWindow* gOnlineSavedWindow84;  // 0x10395e84, original name unknown: ChatPrePostCard's choice
extern uint8_t gOutboxStringStorage[];         // 0x10395e40: the global wide string (0x14 bytes on the Mac)
extern uint8_t gOutboxStringNode[24];          // 0x10395e54: its registration node
void __sinit_ui_onlineOutbox_cpp();            // 0x101e551c
struct jpeg_common_struct { Ptr32<uint8_t> err; };   // 0x00 (Ptr32 jpeg_error_mgr)
void my_error_exit(jpeg_common_struct* cinfo); // 0x101e5310
namespace outbox_callee {
void ErrorOutput(uint32_t tvector, jpeg_common_struct* cinfo);   // __ptr_glue 0x1023baa8, r12 = the manager's word at +8, r3 = cinfo
void JpegLongJmp(void* buffer, int32_t value);                   // 0x10238018 (import)
int32_t JpegSetJmp(void* buffer);                                // 0x10237fa0 (import, setjmp; unit v5981 has a weak stand-in answering 0)
}
// Unit v5981 (src/InterfaceWindow_draw_InterfaceWindow_link.cpp): the island lobby controller's static strings and the online login
// panel's and the outbox address panel's message functions.
extern uint8_t gIslandLobbyText1Storage[];   // 0x10395ce0 (0x14 bytes on the Mac): the global wide string made first
extern uint8_t gIslandLobbyText1Node[24];    // 0x10395cf4: its registration node
extern uint8_t gIslandLobbyText2Storage[];   // 0x10395cc0
extern uint8_t gIslandLobbyText2Node[24];    // 0x10395cd4
void __sinit_ui_islandLobbyController_cpp();   // 0x101e1e3c
int32_t LobbyOnlineLoginPanelDlgMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101e26fc
// The Bullfrog JPEG loader (0x430 bytes on the Mac, made on the stack); its constructor (0x102393a0) and destructor (0x102394c0) are imports.
// Unit v5528: forward declarations and the libjpeg pieces (an import of bullfrog; the Mac's struct layout, pointers are Ptr32)
namespace bfl { namespace FileSystem { class CFile; } }
struct TbRenderArea;
struct TbResult;
struct jpeg_error_mgr { uint8_t mBytes[0x84]; };
struct jpeg_decompress_struct {      // 0x1d0 bytes
    uint32_t err;                    // 0x00 (Ptr32 jpeg_error_mgr)
    uint8_t f04[0x18];               // 0x04
    uint32_t image_width;            // 0x1c
    uint32_t image_height;           // 0x20
    uint8_t f24[0x30];               // 0x24
    int32_t quantize_colors;         // 0x54
    uint8_t f58[0x18];               // 0x58
    uint32_t output_width;           // 0x70
    uint32_t output_height;          // 0x74
    uint8_t f78[0x0c];               // 0x78
    int32_t actual_number_of_colors; // 0x84
    Ptr32<Ptr32<uint8_t>> colormap;  // 0x88 (three channel arrays)
    uint32_t output_scanline;        // 0x8c
    uint8_t f90[0x1d0 - 0x90];
};
jpeg_error_mgr* jpeg_std_error(jpeg_error_mgr* err);                                   // glue 0x1006bae4
void jpeg_CreateDecompress(jpeg_decompress_struct* cinfo, int32_t version, uint32_t size);   // 0x1006bafc
void jpeg_stdio_src(jpeg_decompress_struct* cinfo, void* file);                        // 0x1006bb14
int32_t jpeg_read_header(jpeg_decompress_struct* cinfo, int32_t requireImage);         // 0x1006bb2c
void jpeg_destroy_decompress(jpeg_decompress_struct* cinfo);                           // 0x1006bb44
int32_t jpeg_start_decompress(jpeg_decompress_struct* cinfo);                          // 0x1006bb5c
uint32_t jpeg_read_scanlines(jpeg_decompress_struct* cinfo, uint8_t** rows, uint32_t max);   // 0x1006bb74
int32_t jpeg_finish_decompress(jpeg_decompress_struct* cinfo);                         // 0x1006bb8c
class TbJPEGLoader {
public:
    TbJPEGLoader();
    ~TbJPEGLoader();
    uint8_t mBytes[0x430];
    // Unit v5528 (src/Lbgfsave_iffgiftim_TbJPEGLoader_ReadHeaderInfo.cpp)
    TbResult ReadHeaderInfo(bfl::FileSystem::CFile& file, TbRenderArea& area);   // 0x10024324
    // 0x100244c0: the palette goes to mBytes + 0x2c, four bytes an entry; the last argument is unused
    TbResult ReadData(const TbRenderArea& description, bfl::FileSystem::CFile& file, TbRenderArea& unused);
    bool ValidImage(bfl::FileSystem::CFile& file);                                // 0x100248b8
};

// ---- what the island screens call outside the lobby (all untranslated)
// The island lobby's panels (ui_ files): made and removed with the screen, shown and hidden with it.
void CreateIslandLobbyController();            // 0x101e0f68
void RemoveIslandLobbyController();            // 0x101e11f8 (src/UI_IslandLobby_msg.cpp)
void ShowIslandLobbyController();              // 0x101e1264
void HideIslandLobbyController();              // 0x101e12d4 (src/UI_IslandLobby_msg.cpp)
void UpdateIslandLobbyUI();                    // 0x101e15b4
// The island lobby's controller (src/UI_IslandLobby.cpp, unit w32; the file is ui_islandLobbyController.cpp). CreateIslandLobbyController builds it
// under the lobby's window; Show, Hide and Remove send it messages 6 and 4. It keeps the golden key panel, the key count
// and the park name up to date.
extern InterfaceWindow* gIslandLobbyController;   // 0x10395cbc: the controller's window; NULL until Create has made it
extern int32_t gIslandLobbyKeyEmitter;            // 0x10395cb4: the particle emitter over the key panel (0 for none)
extern int32_t gIslandLobbyKeysShown;             // 0x10395cb8: the keys the player held when the display last looked
extern int16_t gIslandLobbyWindowData[];          // 0x102b006e (src/UI_IslandLobby.cpp, unit w36): InterfaceWindow::Build's description, 269 words
// 0x101e0d98 (src/UI_IslandLobby_msg.cpp, unit w36): the controller panel's message function (window handler).
int32_t IslandLobbyControllerMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
// 0x101e133c (src/UI_IslandLobby_msg.cpp): `on` starts the key emitter over the window's middle (when it is not running),
// otherwise kills it.
void UpdateKeyParticles(InterfaceWindow* window, bool on);
// 0x101e1d40: enables or disables the two buttons and shows or hides the key cost panel and the key count panel.
void EnableIslandLobbyButtons(bool on);
// The sounds of the islands: the registered set's row in gLobbyIslandSounds (two words per island:
// the banks of its two looping sounds, the second word for event 1, the first for event 2).
int32_t SND_RegisterLocalLobbySounds(char* name);   // 0x100df02c
void SND_FreeLocalLobbySounds();               // 0x100df234
void SOUND_FadeSoundEffect(int32_t sound);     // 0x100de0fc
extern int32_t gLobbyThunderBank;               // 0x102f7cbc (0x102f7c94 + 0x28): the lightning's bank (src/TPSound.cpp)
extern int32_t* gLobbyIslandSounds;            // 0x102f7cc0 (the pointer; original name unknown; src/TPSound.cpp)
void KeyParticlePOUFFE();                      // 0x101e1460 (src/lobby_exit.cpp): the sparkle when the player enters an island
// 0x102e48f8 (a file-static of lobby_island.cpp, constructed empty by __sinit_lobby_island_cpp 0x100b54f8): the
// theme of the island the player last entered. EnterCurrentIsland sets it; ResetCurrentIsland(0) picks that
// theme's island again.
extern TbDynamicStringTemplate<char> gLobbyIslandTheme;

// unit w43
void RemoveLoginPanel(int32_t cancel);   // 0x101af02c
// ---- unit w43b: the "Create new player" name entry (src/ui_newname.cpp, ui_newname_data.cpp; the file is ui_playerLogin.cpp)
// 0x101af60c: the login panel's "Create new player" button. Nothing happens while the panel does not exist (gLoginPanelWindow NULL).
// Otherwise the advisor is told line 0x23a (clearing his queue), the name entry dialog is made (Dialog with gNewNameWindowData and
// CreatePlayerMsg, modal), kept in gNewNameWindow, and dressed: the edit box 0x70a gets the cursor-position text object, font 6, white,
// no wrapping, the draw object "hilight" as cursor, "\/*?:|<>\"" as the characters it refuses, the interface's string 0xf3 as text, the
// keyboard and the whole text selected, and writes into gNewNameText (16 characters); the dialog keeps `slot` as extra parameter 0; the
// check buttons 0x70c get two text objects (their buttons 0x70d and 0x70e say strings 0xf1 and 0xf2, font 6, black) and the title 0x709
// a title text object (font 5, centred, string 0xef, the title colour). QUIRK: the check buttons are then asked for the current button
// 0x7a17, which none of their children has (an id of the login panel's picture), so no button is selected.
void NewName(int32_t slot);
int32_t GameConfirmDelPlayer_OK(void* data);   // 0x101adddc (src/ui_newname_msg.cpp, unit w47): the delete-player confirmation's OK
int32_t GameConfirmQuit_OK(void* data);        // 0x101ae1b0 (src/ui_newname_msg.cpp, unit w47): the quit confirmation's OK
extern int8_t gLoginHoverRed;    // 0x102ac007
extern int8_t gLoginHoverGreen;  // 0x102ac008
extern int8_t gLoginHoverBlue;   // 0x10394ac0
// 0x101af3fc (src/ui_newname_msg.cpp, unit w47): the name entry dialog's message function.
int32_t CreatePlayerMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);
extern InterfaceWindow* gNewNameWindow;   // 0x10394a90 (name invented): the name entry dialog, NULL until NewName makes it
extern char16_t gNewNameText[18];         // 0x10394a94 (name invented): the name being typed (the edit box's static text, 16 characters; the next global is at 0x10394ab8)
// 0x102abe96 (name invented), 183 words: the name entry dialog's window description for InterfaceWindow::Build, right after the login
// panel's (gLoginPanelWindowData). The dialog 0x708 holds the title 0x709, the edit box 0x70a (kind 5), a picture 0x70b, the check buttons
// 0x70c with their buttons 0x70d and 0x70e, and two buttons with the ids -2 and -1.
extern int16_t gNewNameWindowData[];
// unit w47 (src/ui_newname_msg.cpp)
// 0x101af264: the name entry dialog's OK. The spaces at the end of gNewNameText are cut off in place; an empty name answers false and
// nothing else happens. Otherwise the player is created in `slot` (the dialog's extra parameter 0, a pointer-sized number) with the
// name, easy mode when the check buttons 0x70c have 0x70d as current, and logged in; true.
bool NewNameOK(void* slot);

// Unit v5128 (src/lobby_globe_close_PostGetCityListCallbackFunction.cpp)
class CFSOnlineCity;
// The screen that asked for the city list (class unknown): two virtual slots after IsVisible, called by the callback.
class CLBCityListScreen : public CLBScreenComponent {
public:
    virtual void CityListReceived(TbPtrList<CFSOnlineCity>* cities) = 0;   // slot 0x30: the list arrived (error 0)
    virtual void CityListFailed() = 0;                                     // slot 0x34: any error
};
// 0x100ad6dc: error 0 calls the context's slot 0x30 with the list, otherwise its slot 0x34.
void PostGetCityListCallbackFunction(int error, const void* context, TbPtrList<CFSOnlineCity>* cities);

// Unit v5189 (src/UI_IslandLobby_msg_sinit_ui_cityLobbyController_cpp_UpdateCityLobbyUI.cpp)
class InterfaceWindow;
extern InterfaceWindow* gCityLobbyWindow;   // 0x10395c70 (name invented): the city lobby's controller window, NULL when there is none
extern int16_t gCityLobbyScript[];          // 0x102afe7a (name invented): its window description (only the address is used here)
// 0x101e0388 (untranslated): the cancel callback of the online park list panel; only its address is passed on.
void ListParkOnlineParkInfoPanelCancelCallback();
// The online islands screen's own virtual slots 0x34 and 0x48 (untranslated; slot 0x48 is the online SelectCurrentIsland).
void OnlineIslandsSlot34(CLBScreenComponentOnlineIslands* screen);
void OnlineIslandsSlot48(CLBScreenComponentOnlineIslands* screen);
// 0x101e0928: `island` NULL or without a park hides the controller's two text windows (0x49da, 0x49d9), otherwise shows them with the
// island number (order + 1) and the park's name (swear filtered when the master save's filter is on). No window: nothing.
void UpdateCityLobbyUI(CLBIsland* island);
// 0x101e05e4: makes the controller window once, under the lobby's window, and dresses its two text windows.
void CreateCityLobbyController();
// 0x101e0400: message 0x100 with the button ids 0x49d5 to 0x49dc drives the online islands screen; message 0x14 forgets the window.
int32_t CityLobbyControllerMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);

// Unit v5255 (untranslated callees of the online islands screen)
void ShowCityLobbyController();                                   // 0x101e0858
void HideCityLobbyController();                                   // 0x101e08c0
void IslandOnlineParkInfoPanelCancelCallback();                   // 0x100b43bc (the Mac passes its TVector at 0x10266750)
void OnlineIslandParkDownloadCompleteCallback(int32_t, const void*);   // 0x100b4420 (TVector at 0x10266748)

// Unit v5553 (src/lobby_globe_CLBScreenComponentDownloadGlobe_Dtor_CLBScreenCo.cpp; untranslated callees of the download globe's Hide)
void RemoveWorldLobbyController();                                // 0x1022b6ac
void RemoveOnlineCityInfoOverlay();                               // 0x101e2314

// Unit v51044 (src/std_less_std_basic_string_operator.cpp)
// 0x1022ba30: with the satellite panel's parent window made, the button 0x16006 below it is set (pressed) when `zoom` is 1, else released.
void WorldLobbySetZoomButton(int32_t zoom);

// Unit v51046 (src/CLBScreenComponentOnlineIslands_GotoNextIsland_OnlineIslandParkDownloadCompleteCallback_2.cpp)
// The online islands screen's virtual slot 0x4c (untranslated): OnlineIslandParkDownloadCompleteCallback calls it when the download worked.
void OnlineIslandsSlot4c(CLBScreenComponentOnlineIslands* screen);
// 0x100b6ae4, lobby_parser.cpp's static initialiser: constructs the wide string at 0x102e4998 and the two narrow strings (gLobbyFolder,
// gLobbyDescriptorFile), registering each with its destructor glue and its node (12 bytes on the Mac; names invented).
void __sinit_lobby_parser_cpp();
alignas(16) extern uint8_t gLobbyParserWide[0x30];   // 0x102e4998: a wide string (the host's object is 0x20 bytes)
extern uint8_t gLobbyParserWideNode[12];             // 0x102e49ac
extern uint8_t gLobbyFolderNode[12];                 // 0x102e498c
extern uint8_t gLobbyDescriptorFileNode[12];         // 0x102e496c

// Unit v51058 (src/std_less_std_basic_string_operator_ShowWorldLobbyController.cpp)
void ShowWorldLobbyController();                      // 0x1022b718: message 6 (1, 0) to the satellite parent window
void HideWorldLobbyController();                      // 0x1022b780: message 6 (0, 0)
void __sinit_ui_worldLobbyController_cpp();           // 0x1022bacc
alignas(16) extern uint8_t gWorldLobbyControllerText[0x30];   // 0x10396ff0: a wide string (the host's object is smaller)
extern uint8_t gWorldLobbyControllerTextNode[12];     // 0x10397004
void __sinit_ui_publishParkPanel_cpp();               // 0x101ecfe8
alignas(16) extern uint8_t gPublishParkPanelText[0x30];       // 0x1039639c
extern uint8_t gPublishParkPanelTextNode[12];         // 0x103963b0
void __sinit_TPGlobals_cpp();                         // 0x1023331c
extern uint8_t gTPGlobalsDtorVector[8];               // stand-in for the TVector at 0x10268a08 (~TPGlobals)
extern uint8_t gTPGlobalsNode[12];                    // 0x103bc158
void __sinit_OnlinePerson_cpp();                      // 0x10213548: sets the words 0x10396d20 (0x10000) and 0x10396d24 (0)
extern uint32_t gOnlinePersonWord20;                  // 0x10396d20
extern uint32_t gOnlinePersonWord24;                  // 0x10396d24
// map<nocase_string, BuddyInfo>::value_compare::operator() const (this, a, b): the pair's first member is the key, at 0.
bool BuddyMapValueCompare_PairKey(const void* functor, const void* pair, const void* key);   // 0x1021f000
bool BuddyMapValueCompare_KeyPair(const void* functor, const void* key, const void* pair);   // 0x1021eed0
struct MslErrorDequeCursor;
// 0x10229498 std::__cdeque<vector<Argument>*>::iterator::operator+= (answers the cursor); a negative count goes to CursorSub 0x101f5bdc
MslErrorDequeCursor* MslArgumentVectorCursorAdd(MslErrorDequeCursor* cursor, int32_t count);
