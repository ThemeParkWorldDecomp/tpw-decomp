// Engine start-up glue of the main app (EngineStartup*, the engine's Mac* hooks,
// EngineTick*, the memory and scratch geometry buffers, procedural texturing, the level
// set-up and the pointer). The original file name is unknown: the inventory files this
// range under TPRide.cpp. src/TPEngine.cpp translates part of it; the rest is declared for
// callers.
#pragma once
#include <cstdint>

#include "platform/samsutils.h"
#include "m3d2.h"
#include "RideDB.h"

class IdxDirectDraw;
struct tag_sRideAnimSet;
struct tag_sDMeshSet;
struct tag_sMesh;
class CTPItemListResource;
class CTPItemResource;

// ---- start-up and shut-down (src/TPEngine.cpp)
// 0x1005adbc: reads the options, opens the window and the OpenGL context, starts the engine;
// 1 on success, 0 (gExitError set when it is the window or the engine) on failure.
int32_t EngineStartup();
void EngineStartup_SetOptions();                    // 0x1005a0fc: the options table to engine flags
// 0x1005a6a8: loads low.sam (flag 0x800), med.sam (0x1000) or high.sam (0x2000) into the
// options table and applies it; 1 on success.
int32_t EngineStartup_OptionsFile(uint32_t flags);
void EngineStartup_Dialog();                        // 0x1005a7e0: the options dialog's flags
int32_t EngineStartup_SettingsFile();               // 0x1005a9b4: a preset by detail level; 1 on success
// 0x1005ab18: installs the hooks, starts DirectDraw, the renderer and the engine; 1 on success.
int32_t EngineStartup_Final(int32_t depth, int32_t a, int32_t fullScreen, int32_t textureFormat);
void EngineCloseDown();                             // 0x1005b5d8
void InitialiseInstancing(uint32_t flags);          // 0x10061aec (empty in this build)
void InitialiseMemorySystem();                      // 0x10064cb0

// ---- the engine's hooks (installed by EngineStartup_Final; see Wavelet.h)
void MacLoadDebugSettings();                        // 0x10059c70
int32_t MacIsFirstPerson();                         // 0x10059e40
uint32_t MacReadClock();                            // 0x10059e78
void MacUpdateMouse();                              // 0x10059ec4
void* MacOpen(const char* name, const char* mode);  // 0x10059ef0
uint32_t MacRead(void* dest, uint32_t size, uint32_t count, void* file);   // 0x10059f38
uint32_t MacClose(void* file);                      // 0x10059f84
uint32_t MacSeek(void* file, uint32_t offset, uint32_t whence);            // 0x10059fc8
int32_t MacSize(void* file);                        // 0x1005a010 (installed as the engine's "tell")
void* MacMalloc(uint32_t size);                     // 0x1005a054
void MacFree(void*& block);                         // 0x1005a0a0
char* FindFilePath(const char* name, int32_t flags);   // 0x1005ad58: the engine's name hook

// ---- the frame (src/TPEngine.cpp)
void AnimateRides(uint32_t mode);             // 0x1005b36c: 2 the first-person ride only, else every ride
void ProcessRideAnim(tag_sRideAnimSet* set, int32_t animate);   // 0x1005b24c, src/RideAnimStep.cpp
void EngineTick1();                           // 0x1005b4d4
void EngineTick2();                           // 0x1005b540: renders the map
void EngineTick3();                           // 0x1005b590: flips the screen
void TPForce3DRefresh(uint32_t flags);        // 0x1005b68c
void ResetWorldBoundingArea();                // 0x1005b634
void HandleActiveHoardings(tag_sRideAnimSet* set, float step);   // 0x100619d4, src/RideAnimStep.cpp
void EnableRideHoardings(tag_sRideAnimSet* set, int32_t enable); // 0x10061060, untranslated
void SetRideHoardingLevel(tag_sRideAnimSet* set, float level);  // 0x10061268, untranslated

// ---- memory (src/TPEngine.cpp)
struct sHeapInfo;
int32_t HNewHeap(uint32_t a, uint32_t b, uint32_t c, sHeapInfo** heap);   // 0x1005b734: a dummy heap, 1
int32_t HFreeHeap(sHeapInfo* heap);           // 0x1005b774: 1
// 0x10064bd4: a Win32 heap block (zeroed when flags bit 0 is set); NULL for size 0.
void* MReleaseAllocF(uint32_t size, uint32_t flags);
// 0x10064c54: frees a block (GlobalFree's result). For NULL the original returns an
// uninitialised value (its caller's return address); no caller uses the result.
void* MReleaseFreeF(void* block);

// ---- procedural texturing (gGameOptions 0x2000 available, 0x4000 on; src/TPEngine.cpp)
void EnableProceduralTexturing();             // 0x100635cc
void DisableProceduralTexturing();            // 0x1006367c
int32_t IsProceduralTexturingActive();        // 0x10063700

// ---- meshes and the scratch geometry buffer (12-byte vertices; src/TPEngine.cpp)
// The mesh header and its nodes are the engine's (include/m3d2.h, the file's own layout).
// The game's record of a loaded mesh (0x40 bytes on the Mac, from MReleaseAllocF, cleared).
class CMesh;
struct tag_sMesh {
    uint32_t mF00;               // 0x00
    tag_sM3D2Header* mHeader;    // 0x04
    tag_sMesh* mF08;             // 0x08  when set, CreateEngineMesh counts on that mesh instead
    uint8_t mF0C[0x10 - 0x0c];
    int32_t mF10;                // 0x10  InstanceRide hands it to CalculateRootNodeFaceNormals
    union {
        uint8_t mF14[2 * sizeof(void*)];   // 0x14..0x1b on the Mac (two 32-bit words), two pointers here
        struct {
            void* mBlock14;                // 0x14  a block DestroyMesh frees and clears
            void* mSkirtVertices;          // 0x18  the skirt's vertices: DestroySkirtVertices frees and clears it
        };
    };
    CMesh* mEngineMesh;          // 0x1c  CreateEngineMesh
    CMesh* mTypeEngineMesh;      // 0x20  CreateMeshTypeEngineMesh
    union {
        uint8_t mF24[4];         // 0x24
        void* mAllocation;       // 0x24  InstanceMesh: the block the instance lives in (as the allocator gave it,
                                 //       before the alignment), for DestroyMeshInstance to free; a native pointer
    };
    void* mF28;                  // 0x28  TickRideHeads runs when it is set
    tag_sNode* mRootNode;        // 0x2c  InstanceRide: the root node whose face normals it calculates
    s3DPoint* mF30;              // 0x30  InstanceRide: the root node's points past its vertex count
    int32_t mEngineMeshes;       // 0x34  engine meshes made by CreateEngineMesh
    uint8_t mF38[8];
};
tag_sMesh* CreateMeshFromM3D2Header(tag_sM3D2Header* header);   // 0x10064cf0 (src/MeshLoad.cpp)
void CreateEngineMesh(tag_sMesh* mesh);                         // 0x10064f40 (src/MeshLoad.cpp)
struct sMatrix;
// 0x10065004 (src/MeshDestroy.cpp): the mesh is taken out of the map lists when it is drawn, the engine meshes' count (on the mesh
// whose count it was added to, mF08 when set) goes down and the engine mesh is deleted (its virtual destructor with the delete flag).
void DestroyEngineMesh(tag_sMesh* mesh);
// 0x100631c4 (src/MeshDestroy.cpp): the same, then the block the instance lives in (mAllocation) is given back with MReleaseFreeF. The Mac
// answers 1 (r3; the callers ignore it) and does not use its two other arguments.
void DestroyMeshInstance(tag_sMesh* mesh, sHeapInfo* heap, uint32_t flags);
void EnableMeshDraw(tag_sMesh* mesh, sMatrix* matrix, int32_t flags);       // 0x10065200
void DisableMeshDraw(tag_sMesh* mesh);                                      // 0x100653a4
// Two 16-bit words passed in one register: a place on the map (the draw calls below link
// the mesh into the map's cells from one to the other).
struct HFieldXZ {
    uint16_t mF00;
    uint16_t mF02;
};
void EnableMeshDraw(tag_sMesh* mesh);                                       // 0x10065178
void EnableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to);           // 0x100650c4
void DisableMeshDraw(tag_sMesh* mesh, HFieldXZ from, HFieldXZ to);          // 0x1006548c
int32_t IsMeshDrawEnabled(tag_sMesh* mesh);                                // 0x10065364 (src/RideAnimTick.cpp): the mesh's flag 1
// 0x1006553c (src/advisor_leaves.cpp): the mesh's header is drawn in the interface's space (header flag 0x200).
void SetMeshInUISpace(tag_sMesh*& mesh);                                    // 0x1006553c
void ScaleMesh(tag_sMesh* mesh, s3DPoint* scale);                           // 0x100655d8
struct sRideAnimEntry {
    uint32_t mIndex;
    tag_sAnimHeader* mAnimation;
};
struct sRideAnimGroup {
    uint32_t mFlags;
    uint32_t mCount;
    sRideAnimEntry* mEntries;
};
// A ride animation's time control (0x38 bytes on the Mac; the set's mTimeControls holds
// mTimeCount of them). Only the words the set-up code uses are named (src/RideAnimInit.cpp).
struct sAnimTimeControl {
    uint32_t mFlags;                  // 0x00  0x40 selects TPGlobals' 0x4018 word over 0x4010
    uint32_t mF04;                    // 0x04  0xc when InitRideAnimSet's second argument is set
    uint32_t mF08;                    // 0x08  the animation's place in its kind (AnimateRideSet)
    float mF0C;                       // 0x0c  the rate the time is divided by
    int32_t mF10;                     // 0x10
    int32_t mF14;                     // 0x14
    int32_t mF18;                     // 0x18
    float mF1C;                       // 0x1c  the animation's length in frames
    float mF20;                       // 0x20
    uint32_t mF24;                    // 0x24  0xc when InitRideAnimSet's second argument is set
    uint32_t mF28;                    // 0x28  DoAnimateRideSet: the queued animation's place in its kind
    uint32_t mF2C;                    // 0x2c  its flags (or-ed into SetNewAnim's when it starts)
    float mF30;                       // 0x30  its speed; 0 for none
    Ptr32<tag_sAnimHeader> mF34;      // 0x34  the animation DoAnimateRideSet applied last (an arena block)
};
static_assert(sizeof(sAnimTimeControl) == 0x38, "sAnimTimeControl layout");
// Three axes of three floats (0x24 bytes): the orientation RACalcOrientationBetweenControlPoints
// works out for AnimateNode (src/RideAnimTick.cpp). The Mac's debug name; the layout is read from
// the listing, where SetLMatrixOrientation gets the three axes as three s3DPoint pointers.
struct s3DAxis {
    s3DPoint mX, mY, mZ;
};
static_assert(sizeof(s3DAxis) == 0x24, "s3DAxis layout");
// The embedded data of a mesh (tag_sMesh::mF28 points at one, built by TPInitialiseEmbeddedData;
// src/EmbeddedData.cpp). 0x14 bytes on the Mac; native pointers here.
struct sEmbeddedRecord {
    uint32_t mFlags;                  // 0x00  0x01: has a block; 0x02: a head (TickRideHeads); 0x10: node has 0x400; 0x20: set by the setup
    uint8_t* mBlock;                  // 0x04  a 0x40-byte block, when mFlags has 0x01; TickRideHeads reads it as the head's 4 x 4 matrix
    uint32_t mF08;                    // 0x08
    uint32_t mF0C;                    // 0x0c  0xffffffff when TPInitialiseEmbeddedData flags its node; TickRideHeads: the head's script object handle
    uint32_t mF10;                    // 0x10
};
struct sEmbeddedSet {
    uint32_t mFlags;                  // 0x00  0x01: some record has a block; 0x04: TickRideHeads runs over the records; 0x08: an entry has flag 0x02
    sEmbeddedRecord* mRecords;        // 0x04  one per embedded entry of the header (mEmbeddedCount)
    uint32_t mF08;                    // 0x08  (the number of enabled heads, as far as is known: DisableRideHead decrements it, zero clears flag 4)
    uint32_t mBlockCount;             // 0x0c  the blocks handed out (when mFlags has 0x01)
    uint8_t* mBlocks;                 // 0x10  mBlockCount blocks of 0x40 bytes
};
// A dynamic mesh set (the coaster track's master set and its instances; 0x40 bytes on the Mac, 0x30-byte
// entries): native pointers, with the Mac offsets in comments. Only InstanceDynamicMeshSet is translated, so
// the words nothing here reads are left unnamed (src/RideInstance.cpp copies them as they are).
struct sDMeshEntry {
    union {
        uint32_t mF00[10];            // 0x00..0x27
        struct {
            uint32_t mFlags;          // 0x00  bit 4 clear: DMCreateMesh counts one point more per vertex
            uint16_t mFaceCount;      // 0x04
            uint16_t mVertexCount;    // 0x06
            uint16_t mUVCount;        // 0x08
            uint16_t mFaceSetCount;   // 0x0a
            uint16_t mPointCount;     // 0x0c
            uint16_t mF0E;            // 0x0e
            float mBox[6];            // 0x10  minimum corner, then maximum corner (the node's box)
        };
    };
    sMatrix* mMatrix;                 // 0x28  InstanceDynamicMeshSet: the matrix of the instance's node (node + 0x10)
    sFaceSet* mFaceSets;              // 0x2c  and that node's face sets
};
// What a set's last word leads to: a record that counts the instances made of the set.
struct sDMeshShared {
    uint32_t mF00[7];                 // 0x00
    uint32_t mInstances;              // 0x1c  raised by InstanceDynamicMeshSet
};
struct tag_sDMeshSet {
    uint32_t mF00;                    // 0x00
    uint16_t mCount;                  // 0x04  the entries
    uint16_t mF06;                    // 0x06
    sDMeshEntry* mEntries;            // 0x08  InstanceDynamicMeshSet: one block after the set, copied from the original
    union {
        uint32_t mF0C;                // 0x0c
        sMatrix* mMatrix;             // 0x0c  DMEnableMeshRender: the matrix the mesh is drawn with, when set (the Mac's word; a pointer here)
    };
    sVertexPosnSet* mVertexSets;      // 0x10  the instance's mesh header's vertex sets (header + 0x58),
    s3DPoint* mPoints;                // 0x14  points (header + 0x5c),
    sUVSet* mUVSets;                  // 0x18  UV sets (header + 0x60)
    sFace* mFaces;                    // 0x1c  and faces (header + 0x68)
    union {
        uint32_t mF20;                // 0x20
        void* mBlock20;               // 0x20  a block DMFreeMesh frees and clears (a pointer on the Mac)
    };
    tag_sMesh* mMesh;                 // 0x24  the set's mesh; InstanceDynamicMeshSet makes an instance of it
    union {
        struct {
            uint32_t mF28, mF2C, mF30;        // 0x28
        };
        struct {                              // DMCreateMesh's totals over the entries (16 bit sums)
            uint16_t mTextureCount;           // 0x28
            uint16_t mVertexCount;            // 0x2a
            uint16_t mFaceSetCount;           // 0x2c
            uint16_t mFaceCount;              // 0x2e
            uint16_t mUVCount;                // 0x30
            uint16_t mPointCount;             // 0x32
        };
    };
    void* mAllocation;                // 0x34  the block as the allocator gave it
    tag_sDMeshSet* mF38;              // 0x38  the track's previous set (unit w213: GetNewTrackMeshSet chains the sets; name kept)
    sDMeshShared* mShared;            // 0x3c
};
// What a set's +0xb0 leads to (the hoarding geometry): only the block at +0xc is known, DestroyRideAnimSet frees it.
struct sHoardingGeometry {
    uint32_t mF00[3];                 // 0x00
    void* mBlock;                     // 0x0c
};
// A ride's animation set: native pointers, with original Mac offsets in comments.
struct tag_sRideAnimSet {
    tag_sRideAnimSet* mSelf;      // 0x00
    uint32_t mFlags;             // 0x04  0x20 has hoardings; 0x3000 no animation; 0x4000 drawn over
                                 //       a range of the map (mFC0, mFC4); 0x200000 skip one
                                 //       frame; 0x800000 set once animated
    tag_sMesh* mHeads;           // 0x08  the set's mesh
    uint16_t mF0C;              // 0x0c
    uint16_t mTimeCount;        // 0x0e
    void* mTimeControls;        // 0x10, mTimeCount records of 0x38 bytes
    union {
        void* mF14;              // 0x14  animated only when set
        uintptr_t mAnimatedCount;   // 0x14  SetNewAnim: how many time controls have an animation (a kind below 0xc)
    };
    sRideAnimGroup mGroups[12]; // 0x18..0xa4
    uint32_t mAnimationCount;   // 0xa8
    tag_sAnimHeader** mAnimations; // 0xac
    union {
        uint8_t mFB0[0xc0 - 0xb0];       // 0xb0
        tag_sDMeshSet* mDynamicMeshSet;  // 0xb4 on the Mac: hoarding state, instanced by InstanceRideAnimSet
    };
    union {
        void* mHoardingMesh;     // 0xb0 on the Mac: geometry table; SetRideHoardingLevel reads **mHoardingMesh as a count
        sHoardingGeometry* mHoardingGeometry;   // 0xb0  DestroyRideAnimSet frees its block (+0xc) and then it
    };
    float mHoardingLevel;        // 0xb8 on the Mac: clamped to [0, 1]
    float mHoardingSpeed;        // 0xbc on the Mac: level change per unit of frame time
    HFieldXZ mFC0;               // 0xc0
    HFieldXZ mFC4;               // 0xc4
    union {
        uint32_t mFC8;           // 0xc8
        sMatrix* mMatrix;        // 0xc8  SetCurRideAnim (set flag 8): the matrix the mesh is drawn with; a native pointer
    };
    union {
        uint32_t mFCC;           // 0xcc
        void* mHeightMap;        // 0xcc  the height map block (a pointer on the Mac): DestroyRideHeightMap frees it;
                                 //       LoadRideType copies it to the "p" variant (mFD0)
    };
    tag_sRideAnimSet* mFD0;      // 0xd0  the "p<name>" variant LoadRideType loads (flag 0x20000)
    int32_t mFD4;                // 0xd4  InstanceRide's fifth argument
    sRideAnimEntry* mEntryAllocation; // 0xd8
    tag_sM3D2Header** mAnimationHeaders; // 0xdc
    uint32_t mFE0;               // 0xe0
    uint32_t mFE4;               // 0xe4  InstanceRide's flags
    int32_t mFE8;                // 0xe8  InstanceRide: the place's x
    int32_t mFEC;                // 0xec  InstanceRide: the place's z
    int32_t mFF0;                // 0xf0  LoadRideType: the ride's width in cells
    int32_t mFF4;                // 0xf4  LoadRideType: the ride's depth in cells
    uint32_t mFF8;               // 0xf8  LoadRideType: the ride's number
    tag_sRideAnimSet* mInstanceSelf; // 0xe0 on the Mac (a word of its own there): InstanceRideAnimSet points an
                                     // instance at itself; a native pointer needs the room, so it is added here
    void* mBraceBlock;           // 0xd4 on the Mac (the word mFD4 above): GetPylonFromBraceMesh reads the pointer there; a native
                                 // pointer needs the room, so it is added here (unit v5314: an sBraceMeshBlock*, TPCoaster.h)
};

// Unit v5288 (src/RideAnimTick_StaticifyCreateOnlyAnimNodes.cpp)
void StaticifyCreateOnlyAnimNodes(tag_sRideAnimSet* set);   // 0x100c6b54 (144)
int32_t LoadRideAnimSet(char* name, uint32_t flags, tag_sRideAnimSet** output, char* loadedName, uint32_t timeCount); // 0x1006840c
void CorrectNodeFlags1(tag_sRideAnimSet* set);                                // 0x10067cf4
void OverrideActiveChannels(tag_sRideAnimSet* set);    // 0x100c609c (src/RideInstance.cpp)
// 0x101ee150 (src/RideWrapping.cpp): StripNodeWrapping on the one node, or on every node of the set's mesh header that has not flag 0x200.
void StripRideWrapping(tag_sRideAnimSet* set, tag_sNode* node, float amount, void* data, s3DPoint* point);
// 0x101edafc (src/NodeWrapping.cpp; mangled .StripNodeWrapping__FP15tag_sM3D2HeaderP9tag_sNodefP8LLCC_RBIP8s3DPointRi): brings the node's
// texture coordinates into the unit square (the whole part of each face's smallest coordinates is taken off, fractions within `amount` of
// a whole number are pushed to it) and checks them against the byte map `data` (the Mac's LLCC_RBI pointer, 16 bytes per row, cells of 10 units
// of `point`'s x and z; used with `point`, the vertices' positions) before clearing flag bits 0x30 of face sets with no checked face. `counter`
// is a word the caller keeps across the nodes of one StripRideWrapping call (it starts at 0): the bad-coordinate message is given once.
void StripNodeWrapping(tag_sM3D2Header* header, tag_sNode* node, float amount, void* data, s3DPoint* point, int32_t& counter);
void TPInitialiseEmbeddedData(tag_sMesh* mesh);
void DestroyMeshHeader(tag_sM3D2Header* header);
int32_t RAInitType(tag_sMesh* mesh, tag_sAnimHeader* animation);   // 0x100c3430, returns 1
void ResetAnimTimeStats(sAnimTimeControl* control, tag_sAnimHeader* animation, int32_t mode, float limit);   // 0x100c5a90
// Unit v5576
void GetAnimTimeStats(sAnimTimeControl* control, uint32_t* time);   // 0x100c5bcc
void RestoreAnimTimeStats(sAnimTimeControl* control, tag_sAnimHeader* animation);   // 0x100c5c68
void GetNumActiveChannels(tag_sRideAnimSet* set);   // 0x100c5fd4
int32_t TPSetupEmbeddedHeirarchy(tag_sMesh* mesh, tag_sNode* node, tag_sM3D2Header* header);   // 0x101ff674
void RAInitNodeAnimNodes(tag_sMesh* mesh, tag_sAnimHeader* animation, int32_t firstKind);   // 0x100c32ac
int32_t InitRideAnimSet(tag_sRideAnimSet* set, int32_t init, int32_t mode, int32_t keepNodes);
// Unit v5561 (src/RestoreAnimTime.cpp)
struct sAnimTimeControlSave {      // 0x2c bytes, the saved part of an sAnimTimeControl
    uint32_t mF00, mF04, mF08;     // 0x00 -> control 0x00, 0x04, 0x08
    uint32_t mF0C, mF10, mF14;     // 0x0c -> control 0x10, 0x14, 0x18
    float mF18;                    // 0x18 -> control 0x0c
    uint32_t mF1C, mF20, mF24;     // 0x1c -> control 0x24, 0x28, 0x2c
    float mF28;                    // 0x28 -> control 0x30
};
void RestoreAnimTimeStats(sAnimTimeControl* control, tag_sAnimHeader* animation);   // 0x100c5c68
bool RestoreAnimTime(tag_sRideAnimSet* set, sAnimTimeControl* control, sAnimTimeControlSave* save);   // 0x1006b6f4
// ---- ride types and instances (src/RideTypes.cpp), the level's height field
// Where a ride is placed (InstanceRide's third argument; built by the code that places rides).
struct sFixedRideParams {
    int32_t mX;                   // 0x00  the cell's x
    int32_t mZ;                   // 0x04  the cell's z
    int32_t mRotation;            // 0x08  the ride's turn
    void* mF0C;                   // 0x0c  when set (with flag 8) the height comes from GetRideRequiredYOffset
};
// 0x10068fd0 (1,216 bytes): loads the ride mesh `name` of `folder` into a set, and its "p" variant
// (flag 0x20000), makes the ride's sign and records the set as a new ride type. The result is the
// type's number (1 up), 0 when the mesh does not load. `a` points at two ints (width and depth in
// cells) when the last two arguments are 0; `b` goes to GenerateHoardingMeshes (flag 8); `c` is
// the number of time controls. With no `sign1` the sign carries the name of the type `id` in the
// ride database.
uint32_t LoadRideType(char* folder, char* name, void* a, void* b, uint32_t flags, uint32_t id, uint32_t c,
                      const char16_t* sign1, const char16_t* sign2, int32_t d, int32_t e);
// 0x10069b24 (1,496 bytes): an instance of a ride type; the result is its number in gRideAnimSets
// (1 up), 0 when there is no such type or no instance could be made. `a` is the number of time
// controls, `b` is kept in the set (+0xd4), `c` and `g` go to InstanceCoaster (flag 0x200), `d` and `e`
// are not used, `f` is the number to give the instance (0xffffffff: the first free one).
uint32_t InstanceRide(uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, int32_t b, void* c,
                      int32_t d, int32_t e, uint32_t f, int32_t g);
void DestroyRideInstance(uint32_t instance);                  // 0x1006a3ec (464 bytes)
void DestroyAllRideTypes();                                   // 0x1006a9f0 (84 bytes)
// 0x1006a140 (608 bytes, TPRide.cpp, untranslated; unit w120 calls it): finds the existing instance of `type` at the place `params` names
// (x, z, rotation and size pair) and marks it restored; answers its number or 0. The arguments follow InstanceRide's, with the instance first.
uint32_t RestoreRideInstance(uint32_t instance, uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, void* c, int32_t d,
                             int32_t e, int32_t f);
// 0x101eefd0 and 0x10044b10 (untranslated; unit w132's CObject::UpdateEngine and CreateRideScript call them): gives the ride instance its script's handle;
// puts the queue stands of the restored coaster back.
void SetRideScriptID(uint32_t instance, uint32_t script);
void HACKRestoreCoasterQueues(uint32_t instance);
// ---- instancing (src/RideInstance.cpp)
// 0x10062e1c (840 bytes): a ride's working copy of its animation set `set`, made in one block from the
// allocator (the set, a copy of its time controls, its animation array and its group entries) with an
// instance of its mesh (InstanceMesh with `flags` less 0x800, `timeCount` as the mask), of its dynamic mesh
// set and of each animation (InstanceAnim). 0 for no set, else 1 with the copy in *output. Flag 0x2000 makes
// the copy's engine mesh. (The original tests flag 0x800, to leave the animations out, only after clearing
// it: the animations are always copied.) A heap of 0 becomes the default heap, which nothing uses.
int32_t InstanceRideAnimSet(tag_sRideAnimSet* set, sHeapInfo* heap, uint32_t flags, tag_sRideAnimSet** output,
                            uint32_t timeCount);
// 0x10061b1c (3,540 bytes): an instance of the mesh `mesh`, in one block (the game's record, a copy of the
// header, then what the flags ask for, copied and made to point at the copies): 1 nodes (and mini nodes), 4
// vertex sets (with 0x100: and the nodes' points; forces 1), 8 points, 0x20 faces, 0x40 face sets, 0x80 UV
// sets (0x10: only the UV sets of the nodes with flag 0x80), 0x200 (or a header with 0x1000, or flag 8
// while game option 0x2000 is on) the textures and their infos; without 1 only the header's node mF78 is
// copied. The embedded set (when the header has embedded entries) and its records and blocks are copied
// too. Afterwards the nodes of the embedded entries whose word 8 has a bit of `mask` are hidden
// (DisableDataNodesRender; with flag 0x1000 the ones whose word 8 is not equal to `mask`:
// DisableDataNodesRenderExcept). Always 1; the new mesh is in *output.
int32_t InstanceMesh(tag_sMesh* mesh, sHeapInfo* heap, uint32_t flags, tag_sMesh** output, uint32_t mask);
// 0x10062938 (540 bytes): an instance of an animation header: the header and its node animations in one
// block, with a copy of the morph set of each node animation that has flag 0x1000 (and not 0x4000) and of its
// key data. Always 1; the new header is in *output. `flags` is not used.
int32_t InstanceAnim(tag_sAnimHeader* animation, sHeapInfo* heap, uint32_t flags, tag_sAnimHeader** output);
// 0x10062ba8 (536 bytes): an instance of a dynamic mesh set (the set, its entries, an instance of its mesh
// from InstanceMesh(mesh, heap, flags, &set->mMesh, 0) with its engine mesh). The answer is InstanceMesh's, 1.
int32_t InstanceDynamicMeshSet(tag_sDMeshSet* set, sHeapInfo* heap, uint32_t flags, tag_sDMeshSet** output);
// 0x101ffc4c and 0x101ffd08 (128 bytes each, TPEmbed.cpp): flag every node of an embedded entry whose word 8
// shares a bit with `mask` (or, for the second, differs from it) with 0x80000010 (not drawn).
void DisableDataNodesRender(tag_sMesh*& mesh, uint32_t mask);
void DisableDataNodesRenderExcept(tag_sMesh*& mesh, uint32_t mask);
// The default heap (0x102c6c50): nothing in the main app ever writes it, so it is 0.
extern sHeapInfo* gDefaultHeap;
// 0x102c1e18 (the name is invented): the largest vertex count CorrectFaceFlags1 has seen in a mesh;
// SetupForPolySearch (0x1005c7e8) allocates 12 bytes for each.
extern uint32_t gPolySearchCount;
// What the ride types and instances call (untranslated; the unit's fakes stand in for them).
void DestroyRideAnimSetInstance(tag_sRideAnimSet* set, sHeapInfo* heap, uint32_t flags);   // 0x10063338
void DestroyRideType(uint32_t type);                                 // 0x1006a768
int32_t IsCoasterType(uint32_t type);                                // 0x100432dc
uint32_t InstanceCoaster(uint32_t type, uint32_t instance, uint32_t x, uint32_t z, int32_t a, void* b);   // 0x100434ec (544 bytes, src/TPCoaster_instances.cpp; answers the object number, 0 for none)
void DestroyCoasterInstanceFromEntranceID(uint32_t instance);        // 0x100438fc
int32_t IsRideFirstPersonRide(uint32_t instance);                    // 0x100329cc
void StopRidingRideIn1stPerson();                                    // 0x10032584
void ToggleFirstPersonWalkAround();                                  // 0x10033b90 (508 bytes, src/TPCamera_modes.cpp, unit w141)
void UnhighlightRide();                                              // 0x1006d0f8
void HighlightRide();                                                // 0x1006ce28 (untranslated; unit w142)
void DisableRideHighlight(int32_t flag);                             // 0x100672e4 (untranslated; unit w141)
void StartFlashingObject(tag_sRideAnimSet* set);                     // 0x1006d2f8
void StopFlashingObject(tag_sRideAnimSet* set);                      // 0x1006d430
void GetBlendedBaseTextures(tag_sRideAnimSet* set);                  // 0x10069770
void AnimateRideCallback(tag_sM3D2Header* header);                   // 0x10069a98, src/RideAnimStep.cpp (CMesh::mCallback of every ride)
void CalculateRootNodeFaceNormals(tag_sNode* node, int32_t a, int32_t& result);   // 0x100694c8
void DoMeshSkirtDeform(tag_sMesh* mesh, tag_sM3D2Header* header, float y, uint32_t a);   // 0x1006e930
void SetHoardingYOffsets(tag_sRideAnimSet* set);                     // 0x10060c80
// Unit v5674: the cells of a height field for a rotation of 0, 90, 180 or 270 degrees (other angles write nothing); the floats are the
// two corners' coordinates and the two fractions.
void GetRotatedHFieldCoord(int32_t angle, float a1, float a2, float a3, float a4, float f5, float f6, uint32_t* outX, uint32_t* outZ);   // 0x10060ab8 (400 bytes)
void GenerateHoardingMeshes(const char* theme, tag_sRideAnimSet* set, void* b);   // 0x1005fa70
void CalculateSkirtVertices(tag_sRideAnimSet* set, tag_sM3D2Header* header, void* a, int32_t w, int32_t h);   // 0x1006f748
void SetBaseBlendFlags(tag_sMesh* mesh, tag_sNode* node, int32_t on);   // 0x1006ebec
void CorrectFaceFlags1(tag_sMesh* mesh, int32_t a);                  // 0x10067fdc (src/RideInstance.cpp)
void CorrectNodeFlags2(tag_sRideAnimSet* set);                       // 0x10067e1c (src/RideInstance.cpp)
void SetupRideHeads(tag_sMesh*& mesh);                               // 0x10200b28 (src/RideInstance.cpp)
void GetRideHeightMap(tag_sRideAnimSet* set, uint32_t w, uint32_t h, char* path, void* a, int32_t b);   // 0x1005e8e8
// Unit v5127 (src/RideDestroy_LoadFileIntoMemory.cpp)
struct sRideHeightMap;
struct RideShape;
char* LoadFileIntoMemory(char* path, int32_t binary);   // 0x1005e300
sRideHeightMap* LoadRideHeightMap(tag_sRideAnimSet* set, uint32_t w, uint32_t h, char* path);   // 0x1005e40c
sRideHeightMap* GenerateRideHeightMap(tag_sRideAnimSet* set, uint32_t w, uint32_t h, RideShape* shape, int32_t flag);   // 0x1005db4c
void SaveRideHeightMap(sRideHeightMap* map, char* path);   // 0x1005e6a0
// Unit v5675 (GenerateRideHeightMap's callees that had no declaration)
void GetAnimStats(tag_sRideAnimSet* set, uint32_t index, uint32_t* kind, uint32_t* place, uint32_t* other);   // 0x101ef044 (unit v5934)
const char* GetAnimStatsName(tag_sRideAnimSet* set, uint32_t index, uint32_t* kind, uint32_t* place, uint32_t* other);   // the same, answering the group's name (r3)
void SetRideAnimTime(uint32_t time);   // 0x100c3234
float GetAnimRemainingMilliseconds(sAnimTimeControl* control);   // 0x100c5db8
// The TPRide instances of the byte swaps (the names carry the address: other instances of the same templates exist elsewhere).
void USwapInplace32ULong_1005e658(uint32_t& word);   // 0x1005e658 SamsUtilities::USwapInplace32<unsigned long>
void USwapInplace16UShort_1005e610(uint16_t& half);  // 0x1005e610 SamsUtilities::USwapInplace16<unsigned short>
uint32_t USwap32UCharPtr_1005e5b4(uint32_t word);    // 0x1005e5b4 SamsUtilities::USwap32<unsigned char*> (a pointer word)
// 0x101ffb0c (264 bytes, src/RideAnimTick.cpp): moves a mesh's embedded records on the mesh's nodes.
void TPProcessEmbeddedData(tag_sMesh* mesh);
// 0x101ffdc8 (104 bytes, src/advisor_leaves.cpp): the place (from 0) of the first embedded entry whose id is `id` and
// whose flags share a bit with `mask`; 0xffffffff for none. A mask with none of the bits 0x3da1f83 is taken as
// 0x3da1f82.
uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id);   // 0x101ffdc8
// 0x101fff90 (408 bytes, src/advisor_leaves.cpp): the place of the advisor's node with the name for `id` (-1: "bug head",
// 1 to 5: the mouth shapes "mouth - normal", "- aah", "- eee", "- ooh", "- sss"; compared without regard to case,
// the nodes taken as full nodes), 0 for another id or none. `mask` is not used.
uint32_t FindDataNodeIndexByIDAdvisorTempHack(tag_sRideAnimSet* set, uint32_t mask, uint32_t id);   // 0x101fff90
// 0x101fef24 (1,744 bytes, src/RideAnimTick.cpp): works out the world matrices of a node and the
// nodes below it. `matrix` (by value: six words in registers, ten on the stack) is the parent's
// world matrix and `world` points to a matrix of the same value (the identity for the root, which
// TPProcessEmbeddedData passes). A node with flag 0x20000000 keeps its own world matrix in the block
// of its record in `records` (its mF50 less the header's mF46 picks the record); when the entry has
// a track morph (flags 0x40 or 0x40000) and the node's parent has flag 0x200000, the matrix first
// moves to the point of the parent's face that the morph names, carried through `world`. (The
// original calls the type of the records sEmbeddedDataNodeParams.)
void DoWorldTransformsForEmbeddedData(tag_sNode* node, tag_sM3D2Header* header, sMatrix matrix, sMatrix* world,
                                      sEmbeddedRecord* records);
// 0x101fec54 (untranslated): the orientation of a track morph node from its triangle's three
// carried points, the interpolated point, the face normal and the morph record; writes `matrix`.
void CalculateOrientFromTrackMorph(s3DPoint* points, s3DPoint* position, s3DPoint* normal,
                                   sTrackMorphingParent* morph, sMatrix* matrix);
// 0x101ed29c, 0x101ed5bc, 0x101ed6ac: where a ride stands on the height field.
float GetRideRequiredYOffset(int32_t x, int32_t z, void* a, int32_t w, int32_t h, uint32_t flags, int32_t rotation);
void CorrectRidePosnForRotation(int32_t* x, int32_t* z, int32_t rotation, int32_t* outX, int32_t* outZ, int32_t w,
                                int32_t h);
void SetRidePositionAndRotation(tag_sRideAnimSet* set, int32_t outX, int32_t outZ, int32_t x, int32_t z,
                                int32_t rotation, float y, uint32_t flags, int32_t w, int32_t h);
enum CELL_MODES : int32_t;
int32_t CellIsRide(CELL_MODES mode);                                 // 0x1006d718
void MoveRide(tag_sRideAnimSet* set, int32_t x, int32_t z, void* a, int32_t w, int32_t h, uint32_t flags);   // 0x101ed9a0
// 0x101ed17c (unit v5886): the node's vertices through `matrix` into gScratchGeomBuffer
void GetNodeWorldVertPosnsIntoGeomBuffer(tag_sNode* node, sMatrix* matrix);
int32_t GetFontPath(char* out);                                    // 0x101eeac8 (src/RideInstance.cpp), 1 on success
int32_t GenerateRideSign(tag_sRideAnimSet* set, char* folder, char* name, char* fontPath, const char16_t* sign1,
                      const char16_t* sign2);                        // 0x101ee290
void DestroyRideSign(tag_sRideAnimSet* set);                         // 0x101eea14
struct IDXTYPEUniqueNameObjectId;
template<class Tag, class T> struct tag_IDX_TYPE;
// 0x10154a10: the ride database (global object at 0x1037f6c8): the name of a ride type. The id
// goes by value in one register (tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, unsigned short>).
class CRideDatabase {
public:
    // 0x10154a10 (src/RideDB_names.cpp, unit w59): the ride type's name, the wide text inside its record (RideInfo::mTypeName),
    // or NULL when the database has no such type. Looks the type up like the three-argument one (the cache first).
    const char16_t* GetTypeName(uint16_t id);
    // 0x10154c24 and 0x10154d68 (src/RideDB_names.cpp, unit w59): the first and the second part of the ride's sign text in
    // `buffer` (at most `size` characters): the name builder's part when the id is in its table, else the ride record's sign
    // text (RideInfo::mSignA, mSignB; a type the database does not have dereferences NULL, as on the Mac).
    void GetSignNameA(uint16_t id, char16_t* buffer, uint32_t size);
    void GetSignNameB(uint16_t id, char16_t* buffer, uint32_t size);
    // 0x10154af0 (src/RideDB_names.cpp, unit w32): the ride type's name in `buffer` (at most `size` characters): the name
    // the ride's own file gave, else the name builder's. The id goes by value in the high half of a register.
    void GetTypeName(uint16_t id, char16_t* buffer, uint32_t size);
    // 0x101546e8 (src/RideDB_clear.cpp): CGame::ExitLevel empties the database with it: the custom effects are deleted, then each ride record
    // (strings given back, record destroyed and freed, slot cleared), the count and the lookup cache reset.
    void ClearDatabase();
    // 0x10156e34 (src/RideDB_effects.cpp, unit w88): from the first ride in the slots on (a NULL slot or the count ends the walk), frees
    // each ride's four emitters and its sound category; the emitter words and the category are left as they were.
    void DeleteCustomEffects();
    // 0x10154ea8: clears the database and scans the five item lists; ignores the path.
    int32_t LoadTheme(const char* path);
    int32_t ScanDirectory(CTPItemListResource& list); // 0x10155c00
    int32_t LoadItem(const CTPItemListResource& list, bfl::FileSystem::CFileStorage& storage, CTPItemResource& item); // 0x1015600c
    void ProcessShape(RideInfo** ride); // 0x101557a4
    // 0x101547b4 (24 bytes, src/RideDB_walk.cpp, unit w114): sets the position (mF72C) to 0 and answers the ride in slot 0 (NULL for an
    // empty database). 0x10154800 (52 bytes): while the position is below the count minus one (signed, the subtraction wraps) moves it on
    // and answers that slot, else answers NULL and leaves the position.
    RideInfo* GetFirstRide();
    RideInfo* GetNextRide();
    // 0x10154868 (104 bytes, src/RideDB_lookup.cpp, unit w200): the ride record of the object type (by value, the HIGH halfword of the register): the cached
    // record when the id is the cached one, else the first slot below the count (signed) whose record starts with that halfword (the id and the record are then
    // cached), else NULL (the cache is left alone).
    RideInfo* GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id);
    const char16_t* GetFileName(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id);   // 0x10154930 (128 bytes, src/RideDB_names.cpp, unit w210): the record's mItemName, a wide string; NULL when no record has the id

    RideInfo* mRides[0x14a];       // 0x000  the ride types, in the order they were loaded
    int32_t mRideCount;            // 0x528
    CRideNameDB mNames;            // 0x52c  the name builder (0x180 bytes)
    uint8_t mF6AC[0x80];           // 0x6ac  not known
    int32_t mF72C;                 // 0x72c  GetFirstRide and GetNextRide's position
    uint16_t mCachedId;            // 0x730  the last type GetTypeName found...
    RideInfo* mCachedRide;         // 0x734  ...and its record
};
extern CRideDatabase gRideDatabase;                                  // 0x1037f6c8, original name unknown
// 0x1005bc44: the level's height field into gHeightfieldRideSet; nonzero on success.
int32_t LoadLevelHeightfield(int32_t a, int32_t b);
void DestroyLevelHeightfield();                               // 0x1005c0fc
int32_t BuildHFieldTextureTable(tag_sM3D2Header* header);       // 0x100319c0 (src/HFieldTextures.cpp)
void FreeHFieldTextureTable();                                 // 0x10031b10 (src/HFieldTextures.cpp)
void SetupHFieldNodeGamuts(tag_sM3D2Header* header);            // 0x100b971c (src/NodeGamuts.cpp)
// 0x100b9304 (src/NodeGamuts.cpp): the cells of the height field that the node's box covers, for the
// node and everything below it; `matrix` is the node's parent's matrix, taken by value.
void DoCalcNodeHFieldGamut(tag_sNode* node, sHField* field, sMatrix matrix);
// 0x1029eb78..0x1029eb84 (names invented): the range a cell index is held to before it is
// stored in a node's byte; -128 .. 127 in the initialised data.
extern int32_t gNodeGamutMinX;                                  // 0x1029eb78
extern int32_t gNodeGamutMaxX;                                  // 0x1029eb7c
extern int32_t gNodeGamutMinZ;                                  // 0x1029eb80
extern int32_t gNodeGamutMaxZ;                                  // 0x1029eb84
void DetermineHFieldFlatness(tag_sM3D2Header* header);          // 0x1005b9d0 (src/HFieldTextures.cpp)
void SaveHFieldCopy(tag_sM3D2Header* header, sHField** copy);    // 0x1005beec
// 0x1005b8fc (164 bytes, src/TPRide_proctex.cpp, unit w114): the engine's PT_Load of "<GetMeshPath()>base.lnd" in `mode`, then, when
// gGameOptions bit 0x4000 is on, CorrectOrientationFlagsForDiffProceduralMode(1), else PT_Disable.
void LoadProceduralTextureSet(int32_t mode);
void DestroyRideAnimSet(tag_sRideAnimSet* set, int32_t mode);
extern tag_sM3D2Header* gHeightfieldHeader;                    // 0x102c6c7c
extern sHFieldQuad* gHeightfieldQuads;                        // 0x102c6c78
extern sHField* gHeightfieldCopy;                             // 0x102c6c80
// LevelInit2 (0x100639b0, src/LevelInit2.cpp, unit w110) keeps the height field's place and size (original names unknown):
extern int32_t gHeightfieldXStart;                           // 0x102c6c70  CWorld::GetHeightfieldXStart's answer
extern int32_t gHeightfieldYStart;                           // 0x102c6c6c
extern int32_t gHeightfieldWidth;                            // 0x102c6c68
extern int32_t gHeightfieldHeight;                           // 0x102c6c64
extern float gHeightfieldXOrigin;                            // 0x102c6c60  header->mHField->mF08[2] * (float)xStart
extern float gHeightfieldYOrigin;                            // 0x102c6c5c  header->mHField->mF08[3] * (float)yStart
extern int32_t gHeightfieldRangeExtraX;                      // 0x102c6c58  added to the width in PT_SetRange's far corner (CGame::InitLevel_Initialise sets it)
extern int32_t gHeightfieldRangeExtraY;                      // 0x102c6c54  the same for the height (the same)
void UpdateScratchGeomBufferSize(tag_sMesh* mesh);   // 0x100682fc
void CreateScratchGeomBuffer(int32_t large);         // 0x10068d14: 0x400 vertices, or the largest mesh's
void DestroyScratchGeomBuffer();                     // 0x10068d90

// ---- ride and mesh teardown (src/RideDestroy.cpp, unit w105)
// 0x101ffa58 (124 bytes, src/TPEmbed_destroy.cpp, unit w114): the embedded data of a mesh being destroyed: the set's blocks, its records
// and the set itself are given back (MReleaseFreeF each, the pointers cleared), then the mesh's word is cleared. Nothing for a mesh
// without a set.
void TPDestroyEmbeddedData(tag_sMesh* mesh);
// 0x10064de0 (308 bytes): a mesh loaded by CreateMeshFromM3D2Header goes: out of the draw lists (flag 1), its engine meshes, the type's
// engine mesh (CMesh::Close then its virtual destructor with the delete flag; only when no engine meshes are counted on the mesh), the
// block (node word 0x98) of each node but the root that has flag 0x1000, mBlock14, the embedded data, the header's block and the mesh.
void DestroyMesh(tag_sMesh* mesh);
// 0x10030a48 and 0x100309c8 (84 and 68 bytes, src/RideDestroy.cpp; the Mac file is TPAdvisor.cpp): take the set's mesh out of the draw
// lists (over the map range given, or not) and clear the set's flag 0x4000. Both answer 1.
int32_t DMDisableMeshRender(tag_sDMeshSet* set, HFieldXZ from, HFieldXZ to);
int32_t DMDisableMeshRender(tag_sDMeshSet* set);
// 0x10030ae8 (240 bytes): frees a dynamic mesh set's mesh (flag 0x4000: out of the draw lists first; its engine mesh; the instance count
// of mShared goes down and, at 0, the name block of each texture info of the mesh's header is freed and cleared), then the set's mBlock20;
// clears flag 0x2000. Answers 1.
int32_t DMFreeMesh(tag_sDMeshSet* set);
struct sDTextureSet;
// 0x100301c0 (1,668 bytes, TPAdvisor.cpp; src/DynamicMesh.cpp): lays the mesh of a set of entries out in one block of the engine allocator (the
// file's 32-bit layout), makes its mesh record and engine mesh and points the texture set's paths at the dynamic texture folders. flags: 1 fills the
// index buffer, 2 sets the header flag 0x200000. Answers 1.
int32_t DMCreateMesh(tag_sDMeshSet* set, sDTextureSet* textures, const char* name, uint32_t flags);
// 0x1003088c (100 bytes) and 0x10030928 (84): put the set's mesh into the draw lists (over the matrix, or over the map range given) and set the
// set's flag 0x4000. Both answer 1.
int32_t DMEnableMeshRender(tag_sDMeshSet* set);
int32_t DMEnableMeshRender(tag_sDMeshSet* set, HFieldXZ from, HFieldXZ to);
// 0x1005eda8 (676 bytes, src/TPRide_height.cpp, unit w140): the terrain height at a place.
float DetermineHFieldHeightQuick(float x, float z);
// The texture sets of the dynamic meshes (include/TPCoaster.h's sDTextureSet; the loader and the freer are in src/LevelLeaves.cpp):
// 0x1003013c (8 bytes) loads a set's textures and answers nonzero when that worked, 0x10030178 (20 bytes) frees them.
struct sDTextureSet;
int32_t DMLoadTextures(sDTextureSet* set);
void DMFreeTextures(sDTextureSet* set);
// 0x1005e9f4 (60 bytes) and 0x1006fbd8 (68 bytes, Debug.cpp on the Mac): free and clear the height map block (+0xcc) and the skirt's vertices
// of the set's mesh (mesh +0x18).
void DestroyRideHeightMap(tag_sRideAnimSet* set);
void DestroySkirtVertices(tag_sRideAnimSet* set);

// ---- untranslated parts of the range
int32_t LevelInit1();                         // 0x100638b0, nonzero on success
int32_t LevelInit2();                         // 0x100639b0, nonzero on success
void LevelDeInit(bool restart);               // 0x10063b80
void TickLevelLights(int32_t force);          // 0x100645b0

void ResetCursorOverSprites(s3DPoint* position);                       // 0x1005b6c8 (src/TPEngine.cpp)
void CalculatePointerWorldPosn(s3DPoint* position, float distance, s3DPoint* pickup, int32_t* result);   // 0x10065f50
void GetMapCellFromPointerWorldPosn(s3DPoint* position);               // 0x10067084 (src/TPEngine_pointedcell.cpp, unit w139): the map cell under
// the pointer's world position (x and z through the height field's origin and cell size, clamped) into gIntrface.mF24 (0 outside the level)

// ---- globals (original names unknown)
// Defined in src/TPEngine.cpp (this range's part of the BSS):
extern uint8_t gSmallTexturesOnly;           // 0x102c1de4, set when the card has 8 MB or less
extern SamsUtilities::UOpenGL* gOpenGL;      // 0x102c1de8, the OpenGL context
extern IdxDirectDraw* gIdxDirectDraw;        // 0x102c1dec, the DirectDraw shim object
extern int32_t gCursorOverSprite;            // 0x102c1df0, cleared by ResetCursorOverSprites
extern s3DPoint gCursorSpritePosition;       // 0x102c1df4, the pointer position it copies
extern uint32_t g3DRefreshFlags;             // 0x102c1e00, TPForce3DRefresh's bits, cleared every EngineTick1
extern int32_t gMemorySystemInitialised;     // 0x102c6c9c
extern uint32_t gScratchGeomVertices;        // 0x102c798c, the largest mesh's vertex count
extern void* gScratchGeomBuffer;             // 0x102c7990
// Defined elsewhere:
extern tag_sRideAnimSet* gHeightfieldRideSet;   // 0x102c6c74 (the heightfield code), animated by EngineTick1
extern tag_sRideAnimSet* gRideAnimSets[];       // 0x102c79a4, from index 1 (src/RideTypes.cpp, kMaxRideInstances entries)
extern uint32_t gRideCountA;                    // 0x102d19a8, the sets AnimateRides walks are
extern uint32_t gRideCountB;                    // 0x102d19ac, 1 .. gRideCountA + gRideCountB
// InstanceRide and DestroyRideInstance keep gRideCountB = the instances, gRideCountA = the free
// slots among numbers 1 .. gRideCountA + gRideCountB, and gRideFirstFree = the first free number - 1.
constexpr uint32_t kMaxRideInstances = 10240;   // the array's size: 0x102c79a4 .. 0x102d19a4
constexpr uint32_t kMaxRideTypes = 330;         // gRideTypes' size: 0x102d19b0 .. 0x102d1ed8
extern uint32_t gRideFirstFree;                 // 0x102d19a4 (src/RideTypes.cpp)
extern tag_sRideAnimSet* gRideTypes[];          // 0x102d19b0, from index 1 (src/RideTypes.cpp)
extern uint32_t gRideTypeCount;                 // 0x102d1ed8, the types loaded (src/RideTypes.cpp)
extern tag_sRideAnimSet* gHighlightedRide;      // 0x102c6ce8, what GetPointedAtRide returns (names invented)
extern int32_t gRideHighlightDisabled;          // 0x102d1f04 (TOC slot -0x70bc, unit v51049, src/std_locale_Dtor_locale.cpp): DisableRideHighlight's flag, 1 = off
extern tag_sRideAnimSet* gPotentialHighlightedRide;   // 0x102d1f08, set by SetPotentiallyHighlightedRide
extern uint32_t gMaxMeshFaces4;                 // 0x102d1f14, 4 x the largest face count (the ride highlight code)
extern uint32_t gMaxMeshF3E;                    // 0x102d1f18
extern uint32_t gMaxMeshNodes;                  // 0x102d1f1c
extern s3DPoint gPickupWorldPosn;               // 0x102c6cec
extern s3DPoint gPointerWorldPosn;              // 0x102c6cf8
s3DPoint* GetPickupWorldPosn();                 // 0x10067220 (8 bytes, src/TPRide_pickup.cpp, unit w143): &gPickupWorldPosn

// ---- Unit w134 (src/TPRide_pointer.cpp): CalculatePointerWorldPosn's own state and its callees (all names invented)
// The pointer's field of view factor: set to 0.885f the first time CalculatePointerWorldPosn runs (the byte is 0 until then).
extern uint8_t gPointerFovOnce;                 // 0x102c6ca0
extern float gPointerFovFactor;                 // 0x102c6ca4
// What FindPointedRide answered last (SetPotentiallyHighlightedRide gets it), and the place it filled in.
extern tag_sRideAnimSet* gPointedRide;          // 0x102c6ce4
extern s3DPoint gPointedRidePosition;           // 0x102c6ccc
// 0x10065b24 (1,000 bytes, untranslated): the ride under the ray at a place of the walk, or 0; fills gPointedRidePosition. The ray
// is walked in steps of `step` along the axis `axis` (1: x); (cellX, cellZ) is the 10 unit cell of (x, y, z); `direction` and
// `camera` are the ray's direction and the camera point.
tag_sRideAnimSet* FindPointedRide(uint32_t cellX, uint32_t cellZ, int32_t axis, float x, float y, float z, float yStep,
                                  s3DPoint* direction, s3DPoint* camera, float step);
void SetPotentiallyHighlightedRide(tag_sRideAnimSet* ride);                 // 0x1006cfa4 (12 bytes, src/Debug_pointer.cpp, unit v5L06)
tag_sRideAnimSet* GetPointedAtRide();                                       // 0x10067254 (12 bytes, src/TPRide_pointed.cpp, unit v5L06): gHighlightedRide
void VectorThroughLMatrixNoWNoTrans(sMatrix* matrix, s3DPoint* point);      // 0x100c2040 (132 bytes, src/rsse_math.cpp, unit w170)
float DistanceBetweenPoints(const s3DPoint* a, const s3DPoint* b);          // 0x100c3198 (92 bytes, src/rsse_math.cpp, unit w170)

// ---- Unit v51021 (src/advisor_leaves_RayPolyCheckOnRide.cpp): the ray test of the ride pick (names invented)
// 0x10065648 (1,184 bytes): the ray (gRayOrigin, gRayDirection) against the node's faces and then its children; the matrix is
// passed by value (64 bytes: r4..r10 and the stack). Fills gRayHitPoint and gRayHitNormal and answers 1 on a hit.
int32_t RayPolyCheckOnRide(tag_sNode* node, sMatrix matrix);
extern s3DPoint gRayHitNormal;        // 0x102c6ca8
extern s3DPoint gRayHitPoint;         // 0x102c6cb4
extern s3DPoint* gRayOrigin;          // 0x102c6cc0 (a 4-byte word on the Mac)
extern s3DPoint* gRayDirection;       // 0x102c6cc4
extern s3DPoint gPointedRideNormal;   // 0x102c6cd8

// ---- Unit v5042 (src/Debug_blendflags_CalcSkirtTextureDirnFlags.cpp): CalcSkirtTextureDirnFlags and its state (names invented)
struct RideShape;
// One cell of the skirt table: the best face found over the cell, with its two edge directions.
struct SkirtCell {
    float mBestX;            // 0x00
    float mBestZ;            // 0x04
    int32_t mFaceSet;        // 0x08  -1: none
    int32_t mFace;           // 0x0c
    s3DPoint mDirX;          // 0x10
    s3DPoint mDirZ;          // 0x1c
    uint32_t mIndexX;        // 0x28
    uint32_t mIndexZ;        // 0x2c
    s3DPoint mCorner[3];     // 0x30
};
extern SkirtCell* gSkirtCurrentCell;     // 0x102d1fa8
extern SkirtCell gSkirtCells[20][20];    // 0x102d1fac, rows of 0x690 bytes
void CalcSkirtTextureDirnFlags(tag_sRideAnimSet* set, RideShape* shape, int32_t width, int32_t height, int32_t force);   // 0x1006eca0
void DecideRideShapeDeformEdges(RideShape* shape, int32_t width, int32_t height);   // 0x1006d76c (760 bytes, untranslated)
void* LLCC_GetRideBlendInfo(void* shape);                      // 0x1008f574 (732 bytes, untranslated): a 16 x 16 byte table (x * 16 + z) or 0
void GetNodeWorldVertPosnsIntoGeomBuffer(tag_sNode* node);      // 0x10068dec (416 bytes, untranslated)

// ---- Unit v5625 (src/LevelSystems_GenerateHoardingMeshes.cpp): GenerateHoardingMeshes' callees and tables (names invented)
// A point of the plane (8 bytes): DetermineInnerBaseInsets' corners and insets.
struct s2DPoint {
    float x, y;
};
// 0x1005f8c4 (380 bytes, untranslated): the angle round the ride's centre (halfWidth, halfHeight) of the side `side` (0..3) of cell (x, y).
float HoardingCentreAngle(int32_t x, int32_t y, int32_t side, float halfWidth, float halfHeight);
// 0x1005f738 (316 bytes, untranslated): the insets (four points) of a cell's corners (four points) against the ride's base points.
void DetermineInnerBaseInsets(s2DPoint* corners, s3DPoint* points, int32_t count, s2DPoint* insets);
extern const char* gHoardingTexturePathFormat;   // 0x1029cf14  "data:levels:%s:MiscMesh:textures" (initialised data)
extern void* gHoardingSortScratch[1600];          // 0x102c2150  FloatSortByArray's output: the sides by angle
extern void* gHoardingSortSource[1600];           // 0x102c3a50  the sides' angles (pointers into the caller's stack)
extern int32_t gHoardingSortOrder[1600];          // 0x102c5350  per sorted index: the side's place in grid order
