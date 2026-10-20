// TPCoaster.cpp (main app): roller coaster building. PARTIAL; untranslated.
// CoasterUserInputTick is in the TPConfig.cpp range of the inventory.
#pragma once

#include <cstdint>

class CObject;

void CoasterTickPreRender();    // 0x10043028
void CoasterTickPostRender();   // 0x10043200
void CoasterUserInputTick();    // 0x100554b4
int32_t IsCoaster(uint32_t instance);                 // 0x10043330 (44 bytes, src/TPCoaster_instances.cpp, unit w186)
int32_t IsCoasterUseable(CObject* object);            // 0x10052954 (untranslated; unit w199: a coaster ride the guests can use, r3 is the CObject)
uint32_t GetCoaster1stPersonCar(uint32_t entrance);   // 0x100433f4 (92 bytes, src/TPCoaster_trains.cpp, unit w192)
void GetCoasterTrackStats(void* object, float* a, float* height, float* c, uint32_t* loops);   // 0x1004d970 (116 bytes, untranslated; unit w153)
void CoasterAcceptLeftMouseClick(int32_t down);   // 0x10057f80 (1,372 bytes, unit v5943; unit w144: CMMCoaster click and release); answers StartMovePylon's answer in mode 2, else 0

#include <cstddef>
#include <cstdint>

#include "TPEngine.h"

// ---- coaster instances (src/CoasterDestroy.cpp, unit w105): native pointers, the Mac offsets in comments
// A queue of 0x20 bytes on the Mac (AllocateQueue 0x10049180 fills it): the capacity, the block of that many words, then the
// read and write places. DestroyQueue (0x10049228, TPConfig.cpp) frees the block.
struct sWraparoundQueue {
    uint32_t mCapacity;               // 0x00
    void* mBlock;                     // 0x04
    uint8_t mF08[0x18];               // 0x08..0x1f on the Mac (words and two pointers into the block)
};
struct tag_sTrackBrace;
struct sCarType;
struct tag_sCoasterType;
// Unit w192 (src/TPCoaster_trains.cpp): the cars and trains InstanceCoasterTrains (0x10042084) makes. The Mac's own names (from the mangled
// names) are sCarInstance (0x60 bytes) and sTrainInstance (0x80 bytes); the host structs are bigger (native pointers) and have no sizeof assert.
// Members this unit does not touch are kept as words: a later unit that finds a pointer there changes it. Names invented unless said.
struct sCarInstance {
    uint32_t mFlags;                  // 0x00  bit 0: the car's mesh is drawn
    sCarType* mType;                  // 0x04
    uint32_t mIndex;                  // 0x08  the car's place in its train
    uint32_t mRide;                   // 0x0c  the car's ride instance number (0 for none)
    float mF10[3];                    // 0x10  cleared by a memset of 12 bytes
    uint32_t mF1C;                    // 0x1c
    uint32_t* mHeadsA;                // 0x20  mType->mHeadCount words of the instance's head block
    uint32_t* mHeadsB;                // 0x24  the next mType->mHeadCount words
    uint32_t* mCurrentA;              // 0x28  set to mHeadsA; the save writes from it
    uint32_t* mCurrentB;              // 0x2c  set to mHeadsB; the save writes from it
    uint32_t mCountA;                 // 0x30  saved as a byte
    uint32_t mCountB;                 // 0x34  saved as a byte
    float mF38;                       // 0x38
    uint32_t mF3C;                    // 0x3c
    float mF40;                       // 0x40  BendCarToTrack reads a float
    uint32_t mF44;                    // 0x44  saved as a byte
    float mF48[3];                    // 0x48
    float mF54[3];                    // 0x54..0x5f  the point half a sample ahead of mF48 (CoasterCarRideTrackTick, unit w215)
};
struct sTrainInstance {
    uint32_t mFlags;                  // 0x00  2 active, 4 restored from a save, 0x20 loading, 0x40, 0x80 (src/TPCoaster_instances.cpp)
    // ON THE MAC this word is the coaster type PLUS 0x2f0: the type's embedded train type (its +0 is the type's mF2F0, +4 the cars per
    // train mF2F4, +8 the heads per train mF2F8, +0xc..+0x18 the car type pointers 0x2fc..0x308). On the host those are separate members of
    // tag_sCoasterType used in many files, so the train points at the TYPE and readers use mType->mF2F4 and so on.
    const tag_sCoasterType* mType;    // 0x04
    uint32_t mF08;                    // 0x08
    uint32_t mCarsInUse;              // 0x0c  saved as a byte; SetTrainCapacity sets it
    uint32_t mF10;                    // 0x10  saved as a halfword
    uint32_t mF14;                    // 0x14  HandleTrainLoading's counters
    uint32_t mF18;                    // 0x18
    uint32_t mF1C;                    // 0x1c
    float mFrontSample;               // 0x20
    float mRearSample;                // 0x24
    sCarInstance* mCars;              // 0x28
    uint32_t mCentreCar;              // 0x2c
    float mF30;                       // 0x30
    sCarInstance* mFirstPersonCar;    // 0x34  mCars, or mCars + 1 when the type's car types A and B differ
    uint32_t mF38;                    // 0x38  saved; SetTrainSoundState
    float mF3C[3];                    // 0x3c  the first car's point (CoasterCarRideTrackTick, unit w215)
    float mF48[3];                    // 0x48  StartTrainSounds
    uint32_t mF54[4];                 // 0x54  sound words
    int32_t mF64;                     // 0x64  sound word, saved
    int32_t mF68;                     // 0x68  sound word, saved
    uint32_t mF6C;                    // 0x6c
    uint32_t mF70;                    // 0x70
    float mPosition;                  // 0x74  saved
    float mF78;                       // 0x78  saved
    float mF7C;                       // 0x7c  CoasterCarRideTrackTick
};
// A coaster built by InstanceCoaster (0x100434ec); the instances form a list headed by gCoasterInstances. Only the words the
// destroyers read are named.
struct tag_sCoasterInstance {
    uint32_t mFlags;                  // 0x00  1 the track must be generated this frame, 4 the editor's preview, 0x800 a train is active (unit w192), 0x2000 "recorded" (unit w137)
    const uint32_t* mType;            // 0x04  the coaster's type (a tag_sCoasterType*, reinterpreted: its word +0 bit 4 = UV animation; unit w137, w186)
    uint32_t mState;                  // 0x08  0xe any running kind, 0xc the loading kinds, 0x40 (unit w137)
    uint32_t mTypeID;                 // 0x0c  InstanceCoaster's type argument (name invented, unit w186)
    uint32_t mEntranceID;             // 0x10  the entrance's id (DestroyCoasterInstanceFromEntranceID)
    uint32_t mObjectInstanceID;       // 0x14  the number in the object instance table
    tag_sCoasterInstance* mNext;      // 0x18  the next instance of the list
    uint32_t mX;                      // 0x1c  InstanceCoaster's x (name invented, unit w186)
    uint32_t mZ;                      // 0x20  InstanceCoaster's z (name invented)
    int32_t mScriptID;                // 0x24  SetCoasterScriptID (name invented)
    void* mCObject;                   // 0x28  SetCoasterCObject (name invented)
    float mF2C;                       // 0x2c  1.0 or 0.0, SetCoasterState (name invented)
    uint8_t mF30[0x34 - 0x30];        // 0x30
    float mSpeedLimit;                // 0x34  the track's speed limit, the cap of every sample's speed (unit w212; name invented)
    struct tag_sDMeshSet* mF38;       // 0x38  the track's current mesh set, the head of the chain of the earlier ones (unit w213; name invented)
    uint32_t mBlock3C;                // 0x3c  the embedded block gCoasterCurrentData points at (unit w137)
    uint32_t mF40;                    // 0x40  nonzero for the editor's coaster (CoasterUserInputTick; name invented, unit w137)
    uint8_t mF44[0x54 - 0x44];        // 0x44
    tag_sTrackBrace* mLastBrace;      // 0x54  the last brace of the list, the head of the chain of the braces' +0x64 (unit w212; name invented)
    tag_sTrackBrace* mBraces;         // 0x58  the head of the brace list, linked by the brace's +0x68 (name invented, unit w186)
    uint8_t mF5C[0x70 - 0x5c];        // 0x5c
    float mF70;                       // 0x70  the last ring's t (unit w213; name invented)
    float mF74;                       // 0x74  the texture V: the track length extruded so far, divided by 6, its fraction kept per mesh set (unit w213; name invented)
    float mF78;                       // 0x78  the orientation block (a sCoasterOrientation, 0x1c bytes, up to mF90; the stand-ins write it, unit w213; names invented)
    float mF7C;                       // 0x7c  the first angle of a ring (ExtrudeCrossSectionVertices' a2)
    float mF80;                       // 0x80  the second angle (a3)
    float mF84;                       // 0x84  the third angle (a4): the bank, ICS adds the banking to it and copies it to mF90
    float mF88;                       // 0x88  the angles of the ring before, the extra ring of a new mesh set (a2)
    float mF8C;                       // 0x8c  (a3)
    float mF90;                       // 0x90  (a4)
    uint8_t mF94[0x9c - 0x94];        // 0x94
    struct sTrackSample* mSamples;    // 0x9c  the sampled track, 0x38 bytes a sample on the Mac (unit w212; name invented)
    union {
        uint8_t mFA0[0xa4 - 0xa0];    // 0xa0
        uint32_t mSegmentSpan;        // 0xa0  the most samples between two braces plus 1 (LinearSampleTrack): the collision chunk buffers hold so many (unit v5L20; name invented)
    };
    uint32_t mSampleCount;            // 0xa4  the number of samples (unit w212; name invented)
    float mLength;                    // 0xa8  the track's length in samples, the position a train wraps at (unit w212; name invented)
    float mFAC;                       // 0xac  3.0 from InstanceCoaster (name invented)
    sWraparoundQueue mQueueB0;        // 0xb0
    sWraparoundQueue mQueueD0;        // 0xd0
    uint8_t mFF0[0xf4 - 0xf0];        // 0xf0
    int32_t mTrainCount;              // 0xf4  the number of trains (unit w137)
    uint8_t mFF8[0xfc - 0xf8];        // 0xf8
    float mFFC;                       // 0xfc  CoasterCarRideTrackTick clears a train's flag 0x40 once its place is past this (unit w215; name invented)
    float mRatingA;                   // 0x100  CollateCoasterStats' first rating, clamped to [gRatingAMin, gRatingAMax] (unit w212; name invented)
    float mRatingB;                   // 0x104  the second rating, clamped to [gRatingBMin, gRatingBMax] (unit w212; name invented)
    uint32_t mMarkedBraces;           // 0x108  the braces whose flags have both 0x40 and 0x80 (unit w212; name invented)
    float mTrackLength;               // 0x10c  mLength * mFAC (unit w212; name invented)
    float mTopHeight;                 // 0x110  the highest sample's y (unit w212; name invented)
    float mTopSpeed;                  // 0x114  the highest speed (unit w212; name invented)
    float mF118;                      // 0x118  the type's mFA4 (name invented)
    float mF11C;                      // 0x11c  0.0 from InstanceCoaster (name invented)
    uint32_t mF120;                   // 0x120  an animation time (untouched by unit w192; name invented)
    sTrainInstance* mLeadTrain;       // 0x124  the train with the smallest mPosition (GetCoaster1stPersonCar reads it; name invented, unit w192)
    sTrainInstance* mTrains;          // 0x128  the type's mF2E8 trains (mTrainCount of them; word +0 bit 2 = running; unit w137, w192)
    uint32_t* mTrainWords;            // 0x12c  one word per train (name invented, unit w192)
    sCarInstance* mCars;              // 0x130  all the trains' cars, mF2E8 * mF2F4 of them (name invented, unit w192)
    float mBankFadeStart;             // 0x134  the sample from which the banking fades out (unit w212; name invented)
    uint8_t mF138[0x13c - 0x138];     // 0x138
    float* mUVOffsets;                // 0x13c  one float per type texture, allocated when the type's flag 4 is set (name invented)
    union {
        uint8_t mF140[0x148 - 0x140];  // 0x140
        uint32_t mCollisionRefs;       // 0x140  the entries of all the braces' collision lists (unit v5L20; name invented)
    };
    uint32_t* mHeads;                 // 0x148  mF318 * 2 words, the cars' head words (name invented, unit w192); the Mac allocates 0x14c bytes; the host sizeof(tag_sCoasterInstance)
    struct sCoasterPylon* mPylons;    // 0x4c  (unit v5038, appended: the Mac offset, the host's is further) the head of the pylon list, linked by the node's +0x28
    tag_sTrackBrace* mPreviewBrace;   // 0x60  (unit v5054, appended: the Mac offset, the host's is further) the editor's brace (HandleCoasterDataChange compares it)
    struct sPylonRecord* mPylonTail;      // 0x50  (unit v5091, appended: the Mac offset) the last pylon of the list headed by mPylons
    struct sPylonRecord* mPreviewPylon;   // 0x68  (unit v5091, appended: the Mac offset) the editor's preview pylon (NewPylon with flag 4)
    // Unit v5190 (appended: the Mac offsets, the host's are further; names invented)
    uint32_t mBraceCount;                 // 0x44  the number of braces (PlacePylon compares the last brace's number with it minus 1)
    tag_sTrackBrace* mBraceB5C;           // 0x5c  GetNewBraceLoftInterval's `other` brace
    tag_sTrackBrace* mBrace64;            // 0x64  the brace PlacePylon looks for among a pylon's braces
    // Unit v5337 (appended: the Mac offset, the host's is further; name invented)
    tag_sTrackBrace* mFlashBrace;         // 0x144  the brace whose ride animation set flashes (DeleteTrackBrace stops it)
    // Unit v5366 (appended: the Mac offset, the host's is further; name invented)
    tag_sTrackBrace* mBrace6C;            // 0x6c  the editor's new brace (LinkPreviewBrace links it)
    // Unit v5157 (appended: the Mac offset, the host's is further; name invented)
    float mF48;                           // 0x48  grows by 1.0 for every brace after the first (NewTrackBrace)
};
extern tag_sCoasterInstance* gCoasterInstances;   // 0x102b68b4, the list's head
extern int32_t gCoasterInstanceCount;             // 0x102b68ac
// The object instance table the coasters' ids index (names invented; RemoveObjectInstanceFromTable 0x101ef310 keeps it):
extern uint32_t gObjectTableCount;                // 0x102b730c
extern uint32_t gObjectTableFirstFree;            // 0x102b7308
extern uint32_t gObjectTableLowest;               // 0x102b7304
extern void* gObjectTable[0x100];                 // 0x102b6f04 .. 0x102b7303, indexed by the object instance number

void DestroyCoasterInstance(tag_sCoasterInstance* instance);   // 0x100437cc: out of the object table and the list, its trains, track,
                                                              // pylons and queues destroyed, freed
void DestroyCoasterInstances();                                // 0x10043994: all of them, then the head is cleared
// Untranslated (the unit's fakes stand in for them):
void DestroyCoasterTrains(tag_sCoasterInstance* instance);     // 0x10042344 (240 bytes, src/TPCoaster_trains.cpp, unit w192)
void DestroyCoasterTrack(tag_sCoasterInstance* instance);      // 0x10050df4
void DestroyCoasterPylons(tag_sCoasterInstance* instance);     // 0x10050e84
void DestroyQueue(sWraparoundQueue* queue);                    // 0x10049228
// Unit v51069 (src/AdvisorScoring_sinit_Advisor_cpp.cpp).
void GetCoasterPeepStats(uint32_t id, float* ratingA, float* ratingB);   // 0x1004d920: the object table's instance's two ratings
extern uint32_t gTPCoasterWord7314;   // 0x102b7314 (name invented), set to 0x10000 by __sinit_TPCoaster_cpp
extern uint32_t gTPCoasterWord7318;   // 0x102b7318 (name invented), cleared by __sinit_TPCoaster_cpp
void __sinit_TPCoaster_cpp();         // 0x100459f4
// 0x101ef310 (ui_ErrorDisplay.cpp): takes object number `id` out of the table `table`; the three counters are the table's.
void RemoveObjectInstanceFromTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest, void** table, uint32_t id);

// ---- the coaster types (src/LevelSystems.cpp, unit w111): native pointers, the Mac offsets in comments; only the words
// DestroyCoasterTypes frees are named (names invented).
// A car type of a coaster type (0x34 bytes on the Mac; the type's array of them is at +0x308, and +0x2fc to +0x304 point into it).
// Unit w138 (SetupCarAndTrainTypesFromSamFile) names the words it writes (names invented); the host record is bigger (native
// pointers). The block at +0x2c is never written by the loaders (zero from the allocator); DestroyCoasterTypes frees it.
struct sCarType {
    uint32_t mFlags;                  // 0x00  bit 0 from the .sam flag
    int32_t mValue;                   // 0x04  the .sam value (never 0)
    char* mName;                      // 0x08  a copy of the .sam name (not freed by DestroyCoasterTypes)
    float mF10;                       // 0x10
    float mF14;                       // 0x14
    float mF18;                       // 0x18
    float mF1C;                       // 0x1c  0.1f unless the .sam float is set
    void* mBlock;                     // 0x2c
    // Unit w148 (SetupCarType; names invented, the Mac offsets in comments):
    uint32_t mRideType;               // 0x0c  LoadRideType's number for the car's mesh (0: it did not load)
    uint32_t mNodeIndex;              // 0x20  a car with flag 1: the node with the largest vertex count
    float mF24;                       // 0x24  the box's maximum z plus mF10
    float mF28;                       // 0x28  minus the box's minimum z, plus mF14
    uint32_t mHeadCount;              // 0x30  the mesh's embedded entries of kind 0x80 (the heads a car can receive)
};
// A texture entry of a coaster type's texture set (0x10 bytes on the Mac): the block at +4 is freed.
struct sDTextureEntry {
    union {
        void* mBlock;                 // 0x04  in the static sets a texture file name (a literal), in a coaster type's set a block
        char* mName;                  // 0x04  the texture's file name (DMCreateMesh copies it into the mesh's texture info)
    };
    uint32_t mFlags;                  // 0x00  7 or 3 in the image: bit 0 -> the texture record's flag 2, bit 1 -> its flags 0x30; 0 in the hoarding set
    uint32_t mF08;                    // 0x08  1 in the hoarding set, 0 in the dynamic set (unit w112a)
    uint32_t mF0C;                    // 0x0c  (unit w112a); in a coaster type's dynamic entries the scroll rate (float bits, unit w138)
};
// A texture set (the static one at 0x1029cf58 for the ride hoardings, 0x102a02fc for the dynamic cell meshes, and one allocated
// per coaster type). Only the entry count and the entries are named.
struct sDTextureSet {
    uint16_t mCount;                  // 0x04  all entries; in a coaster type's set the LAST gCoasterTextureBase of them are the static ones
    sDTextureEntry* mEntries;         // 0x10  mCount entries
    uint32_t mFlags;                  // 0x00  bit 0 = the textures were made (DMCreateMesh sets it, DMFreeTextures clears it)
    const char* mFolder;              // 0x08
    const char* mSharedFolder;        // 0x0c
    sTextureInfo* mInfos;             // 0x14  the header's mTextureInfos
    sTexture* mTextures;              // 0x18  the header's mTextures
    sDMeshShared mUse;                // 0x1c  the Mac's word is the use count; the host wraps it so that tag_sDMeshSet::mShared finds it
};
// The static sets' entry arrays (src/LevelLeaves.cpp; initialised data of the image).
extern sDTextureEntry gRideHoardingTextureEntries[4];    // 0x1029cf18
extern sDTextureEntry gDynamicCellTextureEntries[20];    // 0x102a01bc
// Unit w147 (src/TPCoaster_xsect.cpp; both struct names invented): the rows of the two blocks the cross-section builders fill.
struct sCrossSectionPoint {
    float mF00;                       // 0x00
    float mF04;                       // 0x04  the .sam word plus the type's mF40 (fadds)
    float mF08;                       // 0x08
    float mF0C;                       // 0x0c
    float mF10;                       // 0x10
};
static_assert(sizeof(sCrossSectionPoint) == 0x14, "cross-section point size");
struct sCrossSectionEdge {
    uint32_t mF00;                    // 0x00  never written by the builder
    uint16_t mF04;                    // 0x04  the low 16 bits of a .sam word
    uint16_t mF06;                    // 0x06
    uint16_t mF08;                    // 0x08
    uint16_t mPad0A;                  // 0x0a  never written
};
static_assert(sizeof(sCrossSectionEdge) == 0xc, "cross-section edge size");

// ---- Unit w213 (src/TPConfig_extrude.cpp): the coaster track's mesh: a spline between two braces, the rings extruded along it, their
// faces and the track's mesh sets (TPConfig.cpp). Every name below is invented unless said; Mac offsets in the comments.
struct sCoasterRecord;
using sCrossSection = sCoasterRecord;     // the Mac's sCrossSection is the record of the table at +0x88, defined below
struct sTrackSample;                      // w212's, at the end of this header
struct sCoasterTrackParams;               // the instance's +0x3c block; InterpolateCoasterSpline reads its word 0 (bit 8) through a uint32_t view
// One record of an extrusion table (0x10 bytes on the Mac, 0x18 on the host): the walk of ExtrudeTrackSection stops at the first record
// whose mF00 is not below the ring's t; ExtrudeCrossSectionVertices blends from the record before to this one.
struct sExtrusionParams {
    float mF00;                           // 0x00  the walk's key
    float mF04;                           // 0x04  the blend's key (the ratio runs from the record before's to this one's)
    float mF08;                           // 0x08  the weight of the section's second row
    sCoasterRecord* mSection;             // 0x0c  the cross section of the record
};
// A control of a brace's spline (0x58 bytes; the host pointer covers the Mac word at 0x14, so an array keeps the Mac stride).
struct tag_sBezierControl {
    uint8_t mF00[8];                      // 0x00
    float mF08;                           // 0x08  the control's place along the track: the sample number the banking indexes (an unsigned conversion), and ExtrudeTrackSection's ring t
    float mF0C;                           // 0x0c  added to the brace's number (or to its +0x6c) for the twist and the bank blend
    tag_sTrackBrace* mBrace;              // 0x10  the control's brace (word 0 the brace's number, +0x6c, +0xe4 read; taken for a brace)
    tag_sBezierControl* mNext;            // 0x18  the next control of the list (the host pointer covers the Mac word at 0x1c as mBrace covers 0x14; unit w215, name invented)
    uint8_t mF20[0x40 - 0x20];            // 0x20
    s3DPoint mF40;                        // 0x40  the control's point (the ring before the first starts at the spline's first control's)
    uint8_t mF4C[0x58 - 0x4c];            // 0x4c
};
static_assert(sizeof(tag_sBezierControl) == 0x58, "bezier control size");
// A brace's spline block (0xc bytes on the Mac): which array is which is not known (A holds the controls the position and the
// tangent use, B the ones the banking uses).
struct sBraceSpline {
    uint32_t mCount;                      // 0x00  the number of controls of each array
    tag_sBezierControl* mControlsA;       // 0x04
    tag_sBezierControl* mControlsB;       // 0x08
};
// The orientation the stand-ins write at the instance's +0x78 (mF78 to mF90).
struct sCoasterOrientation {
    float mF00;                           // 0x00
    float mF04;                           // 0x04  the first angle of the ring
    float mF08;                           // 0x08  the second
    float mF0C;                           // 0x0c  the third (the bank: InterpolateCoasterSpline adds the banking to it)
    float mF10;                           // 0x10  the angles of the ring before
    float mF14;                           // 0x14
    float mF18;                           // 0x18  a copy of mF0C
};
static_assert(sizeof(sCoasterOrientation) == 0x1c, "orientation size");
static_assert(offsetof(tag_sCoasterInstance, mF90) - offsetof(tag_sCoasterInstance, mF78) == 0x18, "the instance's orientation words");
// One face entry of a texture container (0x10 bytes: the face as the mesh keeps it and the list's links, arena offsets on the host).
struct sExtrudeFace {
    sFace mFace;                          // 0x00  copied to the set's faces; the vertex indices are its mF02
    Ptr32<sExtrudeFace> mNext;            // 0x08
    Ptr32<sExtrudeFace> mPrev;            // 0x0c
};
static_assert(sizeof(sExtrudeFace) == 0x10, "extrude face size");
// One texture container of the face store B (0x1c bytes): the entries linked to one texture and the face set that holds them.
struct sFaceExtrudeTextureCont {
    uint32_t mFlags;                      // 0x00  bit 1 is copied to the face set's +0xc byte (the rest is not read)
    uint32_t mCount;                      // 0x04  the entries linked
    Ptr32<sExtrudeFace> mHead;            // 0x08
    Ptr32<sExtrudeFace> mTail;            // 0x0c
    Ptr32<sFaceSet> mFaceSet;             // 0x10  the node's face set, set by the first link
    uint32_t mMin;                        // 0x14  the lowest vertex index of the entries (10000 when empty)
    uint32_t mMax;                        // 0x18  the highest
};
static_assert(sizeof(sFaceExtrudeTextureCont) == 0x1c, "texture container size");
extern sExtrusionParams gExtrudeParamsStatic[2];   // 0x102b7650  [0] the start and branch C's params, [1] (0x102b7660) branch A's
extern sExtrusionParams* gExtrudeParamsTable;      // 0x102b7670  branch B's params
extern int32_t gExtrudeTextureKind;                // 0x102b74a8  read as a word, stored as 16 bits
extern sExtrudeFace* gExtrudeFaceNext;             // 0x102b74ac  the next free entry of store A (0x10 bytes each on the Mac, one on the host)
extern int32_t gExtrudeKindFlag;                   // 0x102c1674  1 after branch C, 0 after branch B
extern int32_t gExtrudeWinding;                    // 0x102c1678  SkinCrossSectionFaces' toggle
extern float gExtrudeTOffset;                      // 0x102c167c  subtracted from the ring's t on the spline
extern s3DPoint gExtrudeMax;                       // 0x102c1680  the box of the vertices so far (reset to -1e6)
extern s3DPoint gExtrudeMin;                       // 0x102c168c  (reset to 1e6)
extern int32_t gExtrudeFaceSetCount;               // 0x102c1698  the face sets linked so far
extern int32_t gExtrudeFacesUsed;                  // 0x102c169c
extern int32_t gExtrudePointsUsed;                 // 0x102c16a0  written 0, never read in this unit
extern int32_t gExtrudeUVsUsed;                    // 0x102c16a4
extern int32_t gExtrudeVertsUsed;                  // 0x102c16a8  the next vertex index
extern int32_t gExtrudeFacesLeft;                  // 0x102c16ac  what the mesh set still has room for (from the type's first entry)
extern int32_t gExtrudePointsLeft;                 // 0x102c16b0
extern int32_t gExtrudeUVsLeft;                    // 0x102c16b4
extern int32_t gExtrudeVertsLeft;                  // 0x102c16b8
void ExtrudeTrackSection(tag_sTrackBrace* brace1, tag_sTrackBrace* brace2, int32_t count, float* ts, int32_t force);   // 0x1004f628 (1,816 bytes)
void ExtrudeCrossSectionVertices(float t, float a2, float a3, float a4, s3DPoint& point, s3DPoint& min, s3DPoint& max, int32_t first,
                                 sCrossSection* section, sExtrusionParams* params);   // 0x1004ed8c (1,392 bytes)
void GetNewTrackMeshSet(int32_t flag);                     // 0x1004ea30 (816 bytes)
void SkinCrossSectionFaces(sCrossSection* section, int32_t edges, int32_t pts);   // 0x1004f410 (472 bytes)
sExtrudeFace* LinkFEStoreToFETextureCont(sFaceExtrudeTextureCont* cont);   // 0x1004e790 (228 bytes)
void CopyExtrudedFaceData();                               // 0x1004e6a0 (196 bytes)
void InterpolateCoasterSpline(sTrackSample* samples, sCoasterTrackParams* params, tag_sBezierControl* a, tag_sBezierControl* b,
                              tag_sBezierControl* c, tag_sBezierControl* d, float t, s3DPoint* point,
                              sCoasterOrientation* orient);   // 0x1004e318 (556 bytes)
// Untranslated callees (the unit's fakes stand in for them):
void BezierPosition(tag_sBezierControl* a, tag_sBezierControl* b, float t, s3DPoint* point);   // 0x1004da98 (268 bytes)
void BezierTangent(tag_sBezierControl* a, tag_sBezierControl* b, float t, s3DPoint* tangent);   // 0x1004dc00 (260 bytes)
void GetAlignmentParamsQuick(s3DPoint tangent, float twist, sCoasterOrientation* orient, float* out);   // 0x1004de98 (356 bytes)
void GetBankParams(s3DPoint* point, tag_sBezierControl* c, tag_sBezierControl* d, float t, sCoasterOrientation* orient);   // 0x1004e04c (372 bytes)
void SetMeshPosition(tag_sMesh* mesh, float x, float y, float z);   // 0x10065588 (28 bytes)

// A record of the table at +0x88 (0x18 bytes on the Mac; 0x28 on the host). Unit w138 names the words (names invented); unit w147
// found +0x08 and +0x10 are pointers into the points block (the union keeps the old word for the tests that log it).
struct sCoasterRecord {
    uint32_t mF00;                    // 0x00  the point count of the section's row
    uint32_t mPointCount;             // 0x04  the EDGE count of the section's edge row (name kept)
    union {
        uint32_t mF08;                // 0x08
        sCrossSectionPoint* mPoints;  // 0x08  the points of the section's row (names invented)
    };
    void* mBlock;                     // 0x0c  the edges of the section's edge row (0xc bytes each)
    union {
        uint32_t mF10;                // 0x10
        sCrossSectionPoint* mPointsB; // 0x10  the points of the second row, or what the caller stored
    };
    uint32_t mF14;                    // 0x14  the bits of a float
};
struct tag_sDMeshSet;
// Pylon control blocks in a coaster type; their fields are not yet named.
// Unit w138 (SetCoasterTypeDefaults, SetupPylonControlsFromSamFile) names the words they write (names invented, Mac offsets in
// the comments; the host name pointer takes 8 bytes, so the struct has the same size and the words after it the same offsets).
struct sPylonControl {
    union {
        uint8_t mUnknown[0x124];
        struct {
            uint32_t mFlags;          // 0x00  4 (pylon A) or 0x4004 (B) by default; bit 4 from the .sam
            uint8_t mPad04[8];        // 0x04
            uint32_t mF0C;            // 0x0c
            char* mName;              // 0x10  a copy of the .sam name (not freed by DestroyCoasterTypes)
            // 0x18..0x54: unit w148 (SetupPylonData) names the words it writes (invented): indexes of the pylon mesh's embedded
            // entries and counts. The Mac's name word is 4 bytes and its ride type number is at 0x14 (see mRideType, at the end).
            int32_t mF18;             // 0x18  entry of kind 0x100000, or -1
            int32_t mF1C;             // 0x1c  kind 0x80000 entry whose value is 1
            int32_t mF20;             // 0x20  ... 2
            int32_t mF24;             // 0x24  ... 3
            int32_t mF28;             // 0x28  ... 4
            int32_t mF2C;             // 0x2c  kind 0x800000
            int32_t mF30;             // 0x30
            int32_t mF34;             // 0x34  kind 0x1000000
            int32_t mF38;             // 0x38  kind 0x2000000
            int32_t mF3C;             // 0x3c
            int32_t mF40;             // 0x40
            int32_t mF44;             // 0x44
            int32_t mF48;             // 0x48
            int32_t mF4C;             // 0x4c
            int32_t mF50;             // 0x50
            int32_t mF54;             // 0x54
            float mF58;               // 0x58
            float mF5C;               // 0x5c
            float mF60;               // 0x60
            float mF64;               // 0x64
            float mF68;               // 0x68
            float mF6C;               // 0x6c
            float mF70;               // 0x70
            float mAngles[10];        // 0x74  the .sam degrees times pi/180
            int32_t mF9C;             // 0x9c  the .sam value squared
            int32_t mFA0;             // 0xa0
            float mFA4;               // 0xa4  the .sam value, or the root of the squares in the defaults
            float mFA8;               // 0xa8
            float mFAC;               // 0xac
            float mFB0;               // 0xb0
            int32_t mFB4;             // 0xb4
            int32_t mFB8;             // 0xb8
            float mFBC;               // 0xbc  unit w148: the height of the pylon's embedded entry at animation time 0
            float mFC0;               // 0xc0  ... at time 1
            float mFC4;               // 0xc4  the other entry's height at time 0 minus mFBC
            uint32_t mPadC8;          // 0xc8
            int32_t mFCC;             // 0xcc
            float mFD0[20];           // 0xd0
            uint32_t mRideType;       // 0x14 on the Mac (LoadRideType's number); here after the rest, because the host name takes 8 bytes (unit w148)
        };
    };
};
static_assert(sizeof(sPylonControl) == 0x128, "pylon control size");
static_assert(offsetof(sPylonControl, mName) == 0x10 && offsetof(sPylonControl, mF58) == 0x58 &&
              offsetof(sPylonControl, mFCC) == 0xcc && offsetof(sPylonControl, mFD0) == 0xd0, "pylon control offsets");
struct tag_sCrossSectionSelect;
struct tag_sCoasterType {
    uint32_t mFlags;                  // 0x00  (unit w138, names invented for the words the loaders write)
    float mF08;                       // 0x08
    float mF0C;                       // 0x0c
    float mF10;                       // 0x10
    void* mF14;                       // 0x14  the first sDMeshEntry of the track's master set (unit w148: CreateTrackSectionMesh fills it)
    tag_sDMeshSet* mMeshSet;          // 0x18
    sDTextureSet* mTextures;          // 0x1c
    uint32_t mTextureCount;           // 0x20  the dynamic (.sam) textures
    float mF24;                       // 0x24
    float mF28;                       // 0x28
    float mF2C;                       // 0x2c
    int32_t mF30;                     // 0x30
    int32_t mF34;                     // 0x34
    int32_t mF38;                     // 0x38
    float mF40;                       // 0x40
    float mF44;                       // 0x44
    float mF48;                       // 0x48
    float mF4C;                       // 0x4c
    float mF50;                       // 0x50
    float mF54;                       // 0x54
    float mF58;                       // 0x58
    float mF5C;                       // 0x5c
    float mF60;                       // 0x60
    float mF64;                       // 0x64
    float mF68;                       // 0x68
    float mF6C;                       // 0x6c
    float mF70;                       // 0x70
    void* mF78;                        // 0x78  the cross-section selects (tag_sCrossSectionSelect array, unit w148)
    sCoasterRecord* mRecords;         // 0x88
    uint32_t mRecordIndex;            // 0x8c  the record whose block DestroyCoasterTypes frees (the table is freed after)
    void* mF90;                       // 0x90  the points block (0x14 bytes each; BuildCrossSectionPoints)
    void* mF94;                       // 0x94  the edges block (0xc bytes each; BuildCrossSectionEdges)
    void* mF98;                       // 0x98  the optimised points block (unit w148: OptimiseCrossSections)
    float* mPairs;                    // 0x9c  gCoasterDefaultPairs by default, then a block of mPairCount pairs (never freed)
    uint32_t mPairCount;              // 0xa0
    float mFA4;                       // 0xa4
    sCarType* mCarTypeA;              // 0x2fc
    sCarType* mCarTypeB;              // 0x300  the one the others are compared with
    sCarType* mCarTypeC;              // 0x304
    sCarType* mCarTypes;              // 0x308  the array the three point into
    int32_t mF3C;                     // 0x3c, extrusion face count
    sPylonControl mPylonsA;           // 0xa8
    sPylonControl mPylonsB;           // 0x1c8
    int32_t mF2E8;                    // 0x2e8, product factor
    int32_t mF2EC;                    // 0x2ec
    uint32_t mF2F0;                   // 0x2f0  bit 0: the A and B sets differ, bit 1: C and B differ
    int32_t mF2F4;                    // 0x2f4  3 by default
    int32_t mF2F8;                    // 0x2f8, product factor
    float mF30C;                      // 0x30c
    float mF310;                      // 0x310
    float mF314;                      // 0x314
    int32_t mF318;                    // 0x318, product
    float mF31C;                      // 0x31c
    float mF320;                      // 0x320
    float mF324;                      // 0x324
    float mF328;                      // 0x328
    float mF32C;                      // 0x32c
    float mF330;                      // 0x330
    float mF334;                      // 0x334
    float mF338;                      // 0x338
    float mF33C;                      // 0x33c
    float mF340;                      // 0x340
    float mF344;                      // 0x344
    float mF348;                      // 0x348
    float mF34C;                      // 0x34c
    float mF350;                      // 0x350
    float mF354;                      // 0x354
    float mF358;                      // 0x358
    uint32_t mRideTypeNumber;          // 0x04, set by LoadCoasterTypeDataFile
    // Unit w148 (SetCrossSectionSelectDefaults; names invented; the Mac offsets in comments, the host keeps them after the rest):
    uint32_t mSelectFlags;             // 0x74  bit 0 set when the .sam asks for it
    uint32_t mSelectCount;             // 0x7c
    sCoasterRecord* mSelectA;          // 0x80  the record after the last section's
    sCoasterRecord* mSelectB;          // 0x84  the record the .sam names, or 0
};
class CCoasterBalance;
void LoadCoasterType(uint32_t type, char* folder, CCoasterBalance& balance); // 0x1003ab7c
tag_sCoasterType* LoadCoasterTypeDataFile(uint32_t type, char* folder, char* texturePath, CCoasterBalance& balance); // 0x10039a28
void SetCoasterTypeDefaults(tag_sCoasterType* type); // 0x10036664
int32_t CreateCoasterFromSamFile(tag_sCoasterType* type, char* texturePath, CCoasterBalance* balance); // 0x10039518
void CreateTrackSectionMesh(tag_sCoasterType* type); // 0x10036564
void SetupTrainType(tag_sCoasterType* type, char* folder); // 0x1003a150
void LoadCoasterPylons(tag_sCoasterType* type, sPylonControl* first, sPylonControl* second, char* folder); // 0x1003a9a8
constexpr uint32_t kMaxCoasterTypes = 50;   // 0x102b68bc .. 0x102b6983
extern uint32_t gCoasterTypeCount;                     // 0x102b68b8
extern tag_sCoasterType* gCoasterTypes[kMaxCoasterTypes];   // 0x102b68bc
extern uint32_t gCoasterTextureBase;                   // 0x1029ce20  3: the static entries at the start of a texture set
// 0x1003acf0 (472 bytes): frees every coaster type (see src/LevelSystems.cpp) and clears the count.
void DestroyCoasterTypes();
// 0x10042478 (72 bytes): CreateExtrudeFaceStorage(*0x102b6988, *0x102b6984) and SetupObjectInstanceTable on the object table's counters.
void InitialiseCoasterTypes();
extern uint32_t gExtrudeFaceCountA;                    // 0x102b6988  (names invented)
extern uint32_t gExtrudeFaceCountB;                    // 0x102b6984
void CreateExtrudeFaceStorage(uint32_t a, uint32_t b); // 0x1004e60c (96 bytes, src/LevelLeaves.cpp)
void SetupObjectInstanceTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest);   // 0x101ef1b0 (12 bytes, src/LevelLeaves.cpp)
// The coaster track extruder's two storage blocks (names invented; src/LevelLeaves.cpp): the face count, the records (0x1c bytes
// each) and the points (0x10 bytes each) CreateExtrudeFaceStorage allocates (zeroed).
extern uint32_t gExtrudeFaceStoreCount;                // 0x102b74b8
extern void* gExtrudeFaceStoreB;                       // 0x102b74b4  0x1c bytes each
extern void* gExtrudeFaceStoreA;                       // 0x102b74b0  0x10 bytes each

// ---- Unit w138 (src/TPCoaster_sam.cpp): the loaders CreateCoasterFromSamFile builds a coaster type with.
class CCoasterBalance;
// 0x10036b20 (480 bytes): the track's master mesh set and texture set. Returns 0 without any dynamic texture (.sam count at +0xfc).
int32_t CreateCoasterTrackMasterDMeshSet(tag_sCoasterType* type, char* texturePath, CCoasterBalance* balance);
int32_t CreateCrossSectionsFromSamFile(tag_sCoasterType* type, CCoasterBalance* balance);    // 0x10038ae4 (452 bytes)
int32_t SetupPylonControlsFromSamFile(tag_sCoasterType* type, CCoasterBalance* balance);     // 0x10038d00 (1400 bytes)
int32_t SetupCarAndTrainTypesFromSamFile(tag_sCoasterType* type, CCoasterBalance* balance);  // 0x100392cc (500 bytes)
// The cross-section builders CreateCrossSectionsFromSamFile calls: translated in src/TPCoaster_xsect.cpp (unit w147); the CoasterTypes
// test library keeps recording fakes of these two.
int32_t BuildCrossSectionPoints(tag_sCoasterType* type, sCoasterRecord* records, CCoasterBalance* balance, int32_t* out); // 0x10036d5c
int32_t BuildCrossSectionEdges(tag_sCoasterType* type, sCoasterRecord* records, CCoasterBalance* balance);                 // 0x10037aa8
// Unit w148 (src/TPCoaster_loaders.cpp): the last argument is a FLAG (it goes to SetCrossSectionSelectDefaults), not an out pointer.
int32_t CreateCrossSectionSelectsFromSamFile(tag_sCoasterType* type, sCoasterRecord* records, uint32_t count,
                                             CCoasterBalance* balance, int32_t flag);                                      // 0x1003851c
void OptimiseCrossSections(tag_sCoasterType* type, sCoasterRecord* records, uint32_t count);                               // 0x100388cc
// The image's initialised data (names invented; src/TPCoaster_sam.cpp): the three static texture entries at 0x1029cdf0 (flags 7,
// the names "m_erase.tga", "red.tga", "blue.tga"; they end every coaster type's texture set) and the default pairs the type points
// at from +0x9c (0x1029cce4, five pairs of floats; the word pair at 0x1029cd0c is {pointer, 5}).
extern sDTextureEntry gCoasterStaticTextureEntries[3];
extern float gCoasterDefaultPairs[10];
constexpr uint32_t kCoasterDefaultPairCount = 5;

// ---- Unit w148 (src/TPCoaster_loaders.cpp): the coaster type's remaining loaders: track mesh, train and car types, pylons, cross-section
// selects. CreateTrackSectionMesh, SetupTrainType, LoadCoasterPylons, CreateCrossSectionSelectsFromSamFile and OptimiseCrossSections are
// declared above (CreateTrackSectionMesh answers the type's mesh set in r3 on the Mac; the callers ignore it, so it stays void).
// A cross-section select (0x2c bytes on the Mac, 0x38 on the host; names invented): four pairs of floats (the limits, +-FLT_MAX by
// default, or the .sam's degrees turned into radians) and three records of the section table.
struct tag_sCrossSectionSelect {
    float mF00[8];                    // 0x00  minimum, maximum, minimum, maximum ... (default -FLT_MAX, FLT_MAX)
    sCoasterRecord* mF20;             // 0x20  (a pointer: 4 bytes on the Mac, so the struct is bigger here)
    sCoasterRecord* mF24;             // 0x24
    sCoasterRecord* mF28;             // 0x28
};
struct tag_sRideAnimSet;
void SetupCarType(sCarType* car, char* folder);                                                 // 0x10039bc0 (1,376 bytes)
void SetupPylonData(tag_sCoasterType* type, sPylonControl* pylon, uint32_t rideType);           // 0x1003a28c (1,748 bytes); `type` unused
// 0x100369d8 (200 bytes): allocates `count` selects (zeroed), fills the limits, stores them in the type; `out` (when set) gets the block.
void SetCrossSectionSelectDefaults(tag_sCoasterType* type, uint32_t count, tag_sCrossSectionSelect** out, sCoasterRecord* a,
                                   sCoasterRecord* b, int32_t flag);
// 0x100387b4 (208 bytes): moves the points of `record` whose `used` entry is nonzero to `dst` and renumbers `used` and the edges.
void RepackCrossSection(uint32_t* used, sCoasterRecord* record, sCrossSectionPoint* dst);
// 0x1003d5b0 (44 bytes): fills ONE static params record (x, z, zero turn, a pointer to a static pair {a, b}) and answers it; the same
// record every call (the Mac's BSS 0x102b5bf8 and 0x102b5c08; names invented).
sFixedRideParams* GetTempFixedRideParams(int32_t a, int32_t b, int32_t x, int32_t z);
// Untranslated (TPEmbed.cpp; unit w148 calls them): the number of the embedded entries of kind `kind` (bit mask) of the ride set, and the
// index of the `n`th (1 up) of them in the header's embedded array, -1 when there is none.
uint32_t CountDataNodesByType(tag_sRideAnimSet* set, uint32_t kind);                             // 0x101fff0c
int32_t FindNthDataNodeIndex(tag_sRideAnimSet* set, uint32_t kind, uint32_t n);                  // 0x101ffe74

// ---- Unit w137 (src/TPCoaster_ticks.cpp): the coasters' per-frame ticks and the track editor's pointer input.
// The words of the dragged brace CoasterUserInputTick reads (native pointers; the Mac offsets in comments).
struct sTrackBraceData {
    uint32_t mF00;                    // 0x00
    uint32_t mFlags;                  // 0x04  bit 1 (value 2): the drag costs nothing; bit 0 marks an end pylon brace (unit w186)
    // Unit v5026: the rest of the pylon record (the brace's +0x100 word is one; GetPylonAtCell answers them too).
    uint32_t mF08;                    // 0x08
    uint32_t mCellX;                  // 0x0c
    uint32_t mCellZ;                  // 0x10
    uint32_t mF14;                    // 0x14  compared with the brace's pylon by DoMovePylon (name invented)
    uint32_t mF18;                    // 0x18  the same
    uint8_t mF1C[0x30 - 0x1c];        // 0x1c
    uint32_t mF30;                    // 0x30  above 1: the second kind of pylon (StartMovePylon)
    struct sPylonOwner* mOwner;       // 0x34  (native pointer, so 0x38 on the host)
    const float* mF18Table;           // 0x18  (unit v5214, appended: the native pointer the Mac word at 0x18 is; a float table, the animation ranges at +0xd0)
};
struct tag_sTrackBrace {
    union {
        uint8_t mF00[8];              // 0x00  the bytes (TPConfig_stats.cpp copies the first word)
        uint32_t mNumber;             // 0x00  the brace's number along the track (unit w213; name invented)
    };
    uint32_t mFlags;                  // 0x08  0x200000: the collision needs an update (FlagCollisionUpdateTrackAroundBrace; unit w186)
    uint8_t mF0C[0x64 - 0x0c];        // 0x0c
    tag_sTrackBrace* mPrev;           // 0x64  the previous brace of the instance's list (unit w212)
    tag_sTrackBrace* mNext;           // 0x68  the next brace of the instance's list (unit w186)
    uint8_t mF70[0xb0 - 0x70];        // 0x70
    uint32_t mFB0;                    // 0xb0  nonzero: ExtrudeTrackSection takes the fixed extrusion (unit w213; name invented). The length of mCollisions (unit v5L20)
    uint8_t mFB4[0xe0 - 0xb4];        // 0xb4  the Mac's collision list (mCollisions on the host)
    float mAngle;                     // 0xe0  the yaw the drag turns by
    float mFE4;                       // 0xe4  the bank, blended by InterpolateCoasterSpline (unit w213; name invented)
    uint8_t mFE8[0xfc - 0xe8];        // 0xe8
    uint32_t mSample;                 // 0xfc  the sample the brace stands at (unit w212; name invented)
    const sTrackBraceData* mData;     // 0x100
    struct sBraceSpline* mF90;        // 0x90  the brace's spline block (unit w213; name invented); the Mac word is inside mF70, here appended
    float mF6C;                       // 0x6c  the twist the spline's controls add their mF0C to (unit w213; name invented); on the Mac the word after mNext, which is 8 bytes on the host
    uint32_t mWord70;                 // 0x70  (unit v5026, appended: the Mac offset, the host's is further) DoMovePylon answers it
    float mFD8;                       // 0xd8  (unit v5039, appended: the Mac offset) the wobble x
    float mFDC;                       // 0xdc  (unit v5039, appended) the wobble z
    float mFE8w;                      // 0xe8  (unit v5039, appended) the loft
    float mFF8;                       // 0xf8  (unit v5039, appended) the wobble when the brace has flag 0x40
    float mFD4;                       // 0xd4  (unit v5144, appended: the Mac offset) the rotate drag's centre angle
    float mFEC;                       // 0xec  (unit v5214, appended: the Mac offset) the second animation's value
    float mFF0;                       // 0xf0  (unit v5214, appended) the fourth animation's value
    float mFF4;                       // 0xf4  (unit v5214, appended) the fifth animation's value
    // Unit v5365 (appended: the Mac offsets, the host's are further; names invented): the four blocks DestroyTrackBrace frees
    void* mBlk7C;                     // 0x7c  not freed when the pylon data's flag 1 is set
    void* mBlk80;                     // 0x80
    void* mBlk88;                     // 0x88  not freed when the pylon data's flag 1 is set
    void* mBlk8C;                     // 0x8c
    const uint32_t* mF74;            // 0x74  (unit v5366, appended: the Mac offset) the word whose value indexes the spline's controls (0x58 bytes each)
    // Unit v5157 (appended: the Mac offsets, the host's are further; names invented). The Mac's sBraceInfo at 0x78 is {count, 0x7c block, 0x80
    // block} and the one at 0x84 {count, 0x88 block, 0x8c block}; on the host mF90 points at mCount78 or mCount84 for them.
    float mX4C;                       // 0x4c  NewTrackBrace's x (pylon flag 1)
    float mL50;                       // 0x50  its loft
    float mZ54;                       // 0x54  its z
    uint32_t mCount78;                // 0x78  the first sBraceInfo's count (1 for a single brace)
    uint32_t mCount84;                // 0x84  the second's
    // Unit v5667 (appended: the Mac offsets, the host's are further; names invented): words RestorePylonDetails copies
    float mF94w;                      // 0x94
    union {
        struct {
            uint32_t mW98;            // 0x98
            uint32_t mW9C;            // 0x9c
            uint32_t mWA0;            // 0xa0
            uint32_t mWA4;            // 0xa4
            uint32_t mWA8;            // 0xa8
            uint32_t mWAC;            // 0xac
        };
        sAlignedBox mCollisionBounds;  // 0x98  the box of the track from this brace to the one before it, widened by the pylon radius (unit v5L20)
    };
    // Unit v5L20 (appended: the Mac's list is the 8 words at 0xb4, here pointer-sized): the braces this one's track section crosses,
    // mFB0 of them. Slots 8 to 10 are the Mac's words 0xd4 to 0xdc (mFD4, mFD8, mFDC), which only LoadCoasters writes, for a count
    // above 8 read from the file. SaveCoasters writes each entry's number.
    tag_sTrackBrace* mCollisions[11];
};
struct tag_sRideAnimSet;
// The ticks (names invented unless noted):
extern tag_sCoasterInstance* gCoasterCurrent;       // 0x102b68a8  the instance being ticked, cleared after each loop
extern void* gCoasterCurrentData;                   // 0x102b68a4  the address of its +0x3c block (not cleared)
extern tag_sCoasterInstance* gCoasterEditInstance;  // 0x102b68b0  the editor's coaster (the preview brace is linked only into it)
extern uint32_t gCoasterWord7310;                   // 0x102b7310  CoasterTickPreRender clears it last
// The input tick. The flag bits: 0x3f a drag is active, 0x1e00 the drag kinds, 0x2000 the delta turns by the camera yaw, 0x400 and 0x1000
// the vertical drags (move, turn points), 0x200 and 0x800 the horizontal ones, 0x13f and 0x180 the multi-brace preview.
extern uint32_t gCoasterInputFlags;                 // 0x102c1794
extern tag_sTrackBrace* gCoasterDragBrace;          // 0x102c178c
extern float gCoasterDragAnchor[2];                 // 0x102c16ec  the screen place the drag started at
extern float gCoasterDragScaleX;                    // 0x102c16f4
extern float gCoasterDragScaleY;                    // 0x102c16f8
extern float gCoasterDragLastDX;                    // 0x102c16e4
extern float gCoasterDragLastDY;                    // 0x102c16e8
extern float gCoasterSavedVertical1[3];             // 0x102c1738
extern float gCoasterSavedVertical2[3];             // 0x102c172c
extern float gCoasterSavedHorizontal1[3];           // 0x102c1708
extern float gCoasterSavedHorizontal2[3];           // 0x102c16fc
extern float* gCoasterVerticalTarget1;              // 0x102c1768
extern float* gCoasterVerticalTarget2;              // 0x102c1764  may be NULL
extern float* gCoasterHorizontalTarget1;            // 0x102c1760
extern float* gCoasterHorizontalTarget2;            // 0x102c175c  may be NULL
extern float gCoasterVerticalBase1;                 // 0x102c1748
extern float gCoasterVerticalMin1;                  // 0x102c1758
extern float gCoasterVerticalMax1;                  // 0x102c1750
extern float gCoasterVerticalBase2;                 // 0x102c1744
extern float gCoasterVerticalMin2;                  // 0x102c1754
extern float gCoasterVerticalMax2;                  // 0x102c174c
extern float gCoasterHorizontalBase1;               // 0x102c1718
extern float gCoasterHorizontalMin1;                // 0x102c1728
extern float gCoasterHorizontalMax1;                // 0x102c1720
extern float gCoasterHorizontalBase2;               // 0x102c1714
extern float gCoasterHorizontalMin2;                // 0x102c1724
extern float gCoasterHorizontalMax2;                // 0x102c171c
extern float gCoasterCostBefore;                    // 0x102c1770  the session cost before the drag
extern int32_t gCoasterGridCellX;                   // 0x102c16d0  the pointer's world x / 10, truncated
extern int32_t gCoasterGridCellZ;                   // 0x102c16cc
extern tag_sRideAnimSet* gCoasterPreviousPointedRide;   // 0x102c16c0
extern uint32_t gCoasterWord16E0;                   // 0x102c16e0  bit 0: FlagGenerateTrack(edit instance, 0) at the end of the input tick

// Untranslated callees of TPCoaster.cpp and of the TPConfig.cpp coaster range (the unit's fakes stand in for them):
struct tag_sTrackBrace;
void LinkPreviewBrace(tag_sCoasterInstance* instance);      // 0x1003f9fc
void UnLinkPreviewBrace(tag_sCoasterInstance* instance);    // 0x1003fedc
void CoasterUVAnimTick(tag_sCoasterInstance* instance);     // 0x100428d4
void HandleTrainLoading(tag_sCoasterInstance* instance, sTrainInstance* train);   // 0x10049fbc
void GenerateTrack(tag_sCoasterInstance* instance);         // 0x10050fcc
void FlagGenerateTrack(tag_sCoasterInstance* instance, int32_t flag);   // 0x10040f08
int32_t RoundUp(float value);                               // 0x1004da1c
tag_sCoasterInstance* GetCoasterInstanceFromCObject(void* object);   // 0x100527e0 (56 bytes, untranslated; unit v5275 fakes it)
// Unit v5039
tag_sTrackBrace* GetTrackBrace(sCoasterTrackParams* params, uint32_t id);   // 0x1004de40
extern uint32_t gDrawUpSplineFlag;                          // 0x102b74a4
void SetLoftPylon(float value, uint32_t id, int32_t a, int32_t b);          // 0x10055e0c
void LayoutSetLoftPylon(float value, uint32_t id);                          // 0x10055eac
void SetRotatePylon(float value, uint32_t id, int32_t setFlag, int32_t b);  // 0x10055f38
void LayoutSetRotatePylon(float value, uint32_t id);                        // 0x10055ff0
void SetWobblePylon(float x, float z, uint32_t id, int32_t setFlags, int32_t b);   // 0x1005608c
void LayoutSetWobblePylon(float x, float z, uint32_t id);                   // 0x10056180
void CorrectBraceNumbering(tag_sTrackBrace* brace);                         // 0x100564ac
void HandleCoasterDataChange(uint32_t flags, tag_sTrackBrace* brace, tag_sCoasterInstance* instance, int32_t a, int32_t b);   // 0x10052318
void HandleMultiBracePreviewChange(tag_sCoasterInstance* instance, tag_sRideAnimSet* previous, tag_sRideAnimSet* pointed, int32_t changed,
                                   uint32_t cellX, uint32_t cellZ);   // 0x10054ea8
float CalculateBraceCost(tag_sCoasterInstance* instance, int32_t a, tag_sTrackBrace* brace, float* b, float* c, float* d, float* e);   // 0x10058924

// Unit w186 (src/TPCoaster_instances.cpp): the coaster instances (TPCoaster.cpp on the Mac). IsCoaster is declared at the top, IsCoasterType
// and InstanceCoaster in TPEngine.h, FlagGenerateTrack above.
// The pylon-area words SetPotentialPylonArea (0x10041a58, untranslated) fills (names invented):
extern uint32_t gCoasterWord689C;   // 0x102b689c  GetPotentialPylonPositionArea's first answer
extern uint32_t gCoasterWord6898;   // 0x102b6898
extern uint32_t gCoasterWord6894;   // 0x102b6894
extern uint32_t gCoasterWord6890;   // 0x102b6890
void SetCurrentEditCoaster(tag_sCoasterInstance* instance);                           // 0x1003aca0 (12 bytes)
float GetIntervalForHeight(float height, sPylonControl* pylon);                       // 0x10040e10 (24 bytes)
float GetIntervalForHeightDelta(float height, sPylonControl* pylon);                  // 0x10040e64 (20 bytes)
float GetHeightForInterval(float interval, sPylonControl* pylon);                     // 0x10040eb8 (20 bytes)
void FlagCollisionUpdateTrackAroundBrace(tag_sTrackBrace* brace);                     // 0x10040fa4 (40 bytes)
float XZAngleBetweenPylons(uint32_t x1, uint32_t z1, uint32_t x2, uint32_t z2, uint32_t x3, uint32_t z3);   // 0x10041018 (288 bytes)
void GetPotentialPylonPositionArea(uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d);   // 0x10041170 (52 bytes)
void DestroyEndPylons(tag_sCoasterInstance* instance);                                // 0x10042728 (104 bytes)
void EndTrack(int32_t flag);                                                          // 0x100427d0 (228 bytes)
void SetCoasterState(tag_sCoasterInstance* instance, uint32_t state);                 // 0x10042bb8 (1,072 bytes)
tag_sCoasterInstance* FindCoasterInstanceFromEntranceID(uint32_t entrance);           // 0x10043380 (56 bytes)
void SetCoasterCObject(uint32_t entrance, void* object);                              // 0x10043480 (64 bytes)
void SetCoasterScriptID(uint32_t entrance, int32_t id);                               // 0x1004373c (100 bytes)
int32_t ui_GetNumCoasterTracks();                                                     // 0x100458ac (12 bytes)
// Untranslated callees (the unit's fakes stand in for them):
void RecalcTrackParams(tag_sCoasterInstance* instance);                               // 0x10050f88 (4 bytes)
void DeleteTrackBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace);        // 0x10056660 (976 bytes)
void SetCoasterEditState(uint32_t state);                                             // 0x10053de8 (152 bytes)
void CoasterPanelEndTrack();                                                          // 0x1019c158 (344 bytes)
// The allocation of CoasterPanelEndTrack (0x102384b8): defined weak in its src file, faked by the test library.
namespace coaster_end_callee {
void* OperatorNew(uint32_t size);
}
void PlaceEndPylons(tag_sCoasterInstance* instance, float height);                    // 0x100424f0 (508 bytes)
void KickPeepsFromEntranceQueue(tag_sCoasterInstance* instance);                      // 0x10049858 (324 bytes)
void UnloadTrain(tag_sCoasterInstance* instance, sTrainInstance* train, int32_t flag);       // 0x100499e4 (452 bytes)
void ActivateCoasterTrain(tag_sCoasterInstance* instance, sTrainInstance* train);            // 0x1004a8a0 (380 bytes)
void DeactivateCoasterTrain(tag_sCoasterInstance* instance, sTrainInstance* train);          // 0x1004aa70 (200 bytes)
void FlashEndPylons(tag_sCoasterInstance* instance, uint32_t mode);                   // 0x10052158 (248 bytes)
void SetTrainCapacity(tag_sCoasterInstance* instance, uint32_t a, uint32_t b, uint32_t c);   // 0x1004a328 (380 bytes)
void CoasterSetSpeed(tag_sCoasterInstance* instance, uint32_t speed, int32_t flag);   // 0x1004b94c (188 bytes; rsse.h has another overload)
void PlaceEntrancePylons(tag_sCoasterInstance* instance, uint32_t x, uint32_t z, float height, int32_t flag);   // 0x10040acc (764 bytes)
void ClearCoasterButtons();   // 0x10053d88 (52 bytes, stand-in; unit v5372)
void InstanceCoasterTrains(tag_sCoasterType* type, tag_sCoasterInstance* instance, int32_t flag);   // 0x10042084 (620 bytes, unit w192)
void RestoreCoasterSounds(tag_sCoasterInstance* instance);                            // 0x100472c4 (220 bytes)
// 0x101ef1f4 (212 bytes): the table's three counters by reference (RemoveObjectInstanceFromTable's form); answers the new number, 0 for none.
uint32_t AllocateObjectInstanceToTable(uint32_t& count, uint32_t& firstFree, uint32_t& lowest, uint32_t flags, void** table, void* object,
                                       uint32_t extra);
void AllocateQueue(sWraparoundQueue* queue, uint32_t capacity);                       // 0x10049180 (112 bytes)
// Unit v5269 (src/AllocateQueue.cpp)
void HACKCoasterReturnLostPeep(tag_sCoasterInstance* instance, uint32_t peep);        // 0x10049324 (224 bytes): the first queue with room takes the peep
extern int32_t gCoasterQueueSpaceLimit;                                               // 0x1029ce24, read by CoasterGetQueueSpace (name invented)

// Unit w192 (src/TPCoaster_trains.cpp): the coaster trains and cars. GetCoaster1stPersonCar is declared at the top, InstanceCoasterTrains above,
// DestroyCoasterTrains above (0x10042344, 240 bytes, unit w192; the declaration stays in the destroyers' list).
int32_t SaveTrainsAndCars(void* file, tag_sCoasterInstance* instance);   // 0x10043a18 (864 bytes): the trains' records and the cars' (answers 0 on a short write)
int32_t LoadTrainsAndCars(void* file, tag_sCoasterInstance* instance);   // 0x10043e8c (948 bytes): the same read back (answers 0 on a short read)
// The little-endian save file's swaps as TPCoaster.cpp instantiates them (the Mac has one copy per source file; the float's is the main app's one
// copy, 0x10021bb4, which the later SaveCoasters and SaveRides call too): real on a big-endian host or with TPW_SWAP_FILE_WORDS, else no-ops.
namespace TPCoasterSwap {
void USwapInplace16UShort(uint16_t& value);   // 0x10043db8 (12 bytes)
void USwapInplace32Int(int32_t& word);        // 0x10043e00 (12 bytes)
void USwapInplace32ULong(uint32_t& word);     // 0x10043e44 (12 bytes)
void USwapInplaceFloat(float& value);         // 0x10021bb4 (12 bytes)
}  // namespace TPCoasterSwap
// Unit w192 (untranslated): TPConfig.cpp, the car's draw switch of a ride.
void DisableCoasterCarDrawSafe(uint32_t ride);   // 0x1004a648 (224 bytes)

// ---- Unit w212 (src/TPConfig_stats.cpp): the track's statistics and the trains' placing (TPConfig.cpp; names invented)
float ModF(float value, int32_t* whole);   // 0x1004a75c (84 bytes): the fraction of value (modf); *whole, when given, gets the whole part truncated to an int
void SetCoasterCapacity(tag_sCoasterInstance* instance, uint32_t trains, uint32_t unused);   // 0x1004a248 (156 bytes): unloads the trains past the new count (the third argument is never read)
void UpdateExcitementAtSample(tag_sCoasterInstance* instance, uint32_t sample, uint32_t word, float speed, float bank, s3DPoint* g, float time);   // 0x1004d424 (844 bytes; word and time are never read)
void SpreadTrainsOnTrack(tag_sCoasterInstance* instance, float time);   // 0x1004b6c4 (580 bytes; the float is never read)
void CollateCoasterStats(tag_sCoasterInstance* instance, float height, float speed, float time);   // 0x1004d7c8 (276 bytes; time is never read)
void CalculateTrackGForcesSpeedAndExcitement(tag_sCoasterInstance* instance);   // 0x1004ba48 (2,340 bytes)
void UpdateCoasterTrainPosns(tag_sCoasterInstance* instance);   // 0x1004b4ac (468 bytes)
void UpdateCoasterTrainsActive(tag_sCoasterInstance* instance);   // 0x1004b320 (324 bytes)
void SetInitialExcitementValues(tag_sCoasterInstance* instance);   // 0x1004d338 (164 bytes)
void StopTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train);   // 0x100471bc (184 bytes; untranslated)
// The piece of track a sample stands on (only its brace is read: the +0x10 word, taken for a brace because its +8 is a flags word like the brace's).
struct sTrackSamplePiece {
    uint8_t mF00[0x10];               // 0x00
    tag_sTrackBrace* mBrace;          // 0x10
};
// One sample of the sampled track: 0x38 bytes on the Mac, 0x40 on the host (the piece pointer is 8 bytes there).
struct sTrackSample {
    uint32_t mFlags;                  // 0x00  8 the track climbs here, 4 and 2 the two rises of the excitement
    s3DPoint mPos;                    // 0x04
    float mYaw;                       // 0x10
    float mPitch;                     // 0x14
    float mBank;                      // 0x18
    float mSpeed;                     // 0x1c
    s3DPoint mG;                      // 0x20  the g-forces
    sTrackSamplePiece* mPiece;        // 0x2c
    tag_sBezierControl* mPieceB;      // 0x30  the second control list, walked beside mPiece's (unit w215; name invented)
    uint8_t mF34[0x38 - 0x34];        // 0x34
};
// TPConfig.cpp's tuning words: the first twelve are data on the Mac (starting values below), the other ten are bss.
extern float gTrackGravity;           // 0x1029ce30  1.0
extern int32_t gCoasterMinTrains;     // 0x1029ce34  1
extern float gBankFadeStartMin;       // 0x1029ce38  0.0
extern float gBankFadeMax;            // 0x1029ce3c  1.0
extern float gBankFadeMin;            // 0x1029ce40  0.0
extern float gExciteBankFloorA;       // 0x1029ce44  0.0
extern float gExciteBankFloorB;       // 0x1029ce48  0.0
extern float gExciteFloor;            // 0x1029ce4c  0.0
extern float gRatingBMax;             // 0x1029ce50  100.0
extern float gRatingBMin;             // 0x1029ce54  0.0
extern float gRatingAMax;             // 0x1029ce58  100.0
extern float gRatingAMin;             // 0x1029ce5c  0.0
extern int32_t gExciteRiseA;          // 0x102b7474
extern int32_t gExciteRiseB;          // 0x102b7478
extern float gExcitePrevB;            // 0x102b747c
extern float gExciteMaxB;             // 0x102b7480
extern float gExciteMaxA;             // 0x102b7484
extern float gExcitePrevBank;         // 0x102b7488  read, never written after SetInitialExcitementValues
extern float gExcitePrevG[3];         // 0x102b748c  the same
extern int32_t gExciteWord;           // 0x102b7498  written 0, never read
extern float gExciteB;                // 0x102b749c
extern float gExciteA;                // 0x102b74a0

// ---- Unit w214 (src/TPConfig_bend.cpp): a coaster car bent to the track, and a running train's sounds and screams
struct tag_sNode;
struct sVertexPosnSet;
extern float gBendAngleClamps[8];   // 0x1029cd8c (name invented): AccumulateBendAngles' acos clamps, (lo, hi) for loop 2's yaw and pitch, then loop 3's; all {0.0f, 1.0f}
void AccumulateBendAngles(tag_sCoasterInstance* instance, sCarInstance* car, float* yaws, float* pitches, float* heights, uint32_t capacity, float position, uint32_t* centre);   // 0x10045ad8 (2,796 bytes)
void RotateNodeVertexPosns(tag_sNode* node, sVertexPosnSet* source, sVertexPosnSet* dest, uint32_t count, float* yaws, float* pitches, float* heights, uint32_t centre, float spacing);   // 0x10046620 (520 bytes)
void BendCarToTrack(tag_sCoasterInstance* instance, sCarInstance* car);   // 0x1004688c (320 bytes)
void HandleScreams(tag_sCoasterInstance* instance, sTrainInstance* train, int32_t x, int32_t y, int32_t z);   // 0x10046d78 (612 bytes)
void TickTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train);   // 0x1004702c (320 bytes)
float InterpAngles(float a, float b, float t);   // 0x1004dd5c (188 bytes)

// ---- Unit w215 (src/TPConfig_ridetick.cpp): a coaster's cars ridden along the track each frame, the turn of a car, the trains' collisions and
// their sounds (TPConfig.cpp; names invented)
struct tag_sRideAnimSet;
void CoasterCarRideTrackTick(tag_sCoasterInstance* instance);   // 0x1004c4f0 (3,512 bytes)
void SetCarOrientation(sCarInstance* car, sCoasterOrientation* orient, s3DPoint* point);   // 0x1004ab8c (760 bytes): sets the car's ride node's matrix to the orientation; turns the point (when given) the same way
void SetTrainSoundState(sTrainInstance* train, uint32_t state, uint32_t value);   // 0x100473e4 (236 bytes)
void HandleCollisions(tag_sCoasterInstance* instance, sTrainInstance* train);   // 0x1004c3c0 (224 bytes)
void InterpolateCoasterSpline(tag_sCoasterInstance* instance, float position, s3DPoint* point, sCoasterOrientation* orient);   // 0x1004b0b0 (224 bytes): the instance overload, beside the sample overload at 0x1004e318
void PlayCollisionSound(tag_sCoasterInstance* instance, sTrainInstance* train);   // 0x10047510 (212 bytes)
void GetLinearSamplePosn(tag_sCoasterInstance* instance, int32_t sample, float t, s3DPoint* out);   // 0x1004afa4 (96 bytes)
float DistanceBetweenPoints(s3DPoint& a, s3DPoint& b);   // 0x1004f374 (92 bytes): the reference overload; the pointer overload (0x100c3198) is in TPEngine.h
// Untranslated callees of CoasterCarRideTrackTick (the unit's fakes stand in for them; the types are the decompilation's):
uint32_t GetRideAnimTime(int32_t which);   // 0x100c3268 (28 bytes)
void UpdateRideAnimSpeedImmediate(tag_sRideAnimSet* set, float speed, int32_t flag);   // 0x100c7b58 (20 bytes)
extern uint8_t gCarRideTickInit;   // 0x102b746c  the tick's function-local static: set once the speed cap below is made (name invented)
extern float gCarRideTickMax;      // 0x102b7470  the largest float, written by the first call (nothing in this unit reads it; name invented)

// ---- Unit v5205 (src/TPConfig_bend_LayoutStartAllCoasterSounds.cpp): a train's sounds started
void StartTrainSounds(tag_sCoasterInstance* instance, sTrainInstance* train, int32_t noState);   // 0x10046a18 (344 bytes): stops, then starts the train's four sound words and two loops; state 2 at 100 unless noState
void LayoutStartAllCoasterSounds();   // 0x10046bc0 (388 bytes): StartTrainSounds' body for every train of every coaster, always setting state 2

// ---- Unit v5369 (src/TPConfig_stats_RecalcDistancesFromCentreCar.cpp)
void RecalcDistancesFromCentreCar(tag_sCoasterInstance* instance, sTrainInstance* train);   // 0x1004a4e8 (260 bytes): each car's distance from the centre car (mF38, and mF3C divided by the instance's mFAC)
void GetTrainFrontAndRearSamples(tag_sCoasterInstance* instance, sTrainInstance* train);    // 0x1004a7d0 (116 bytes; untranslated, the unit's fake stands in)

// ---- Unit v5011 (src/GetTangentsFromZVector.cpp)
// 0x1003be20 (1,784 bytes): the control's tangents (floats 0x1c and 0x28), then its two handle points (0x34 and 0x4c). The pylon (word 0 bit 0x4000)
// shapes the lengths from the track's banking; the last three arguments are never read (the flag's two branches are the same).
void GetTangentsFromZVector(tag_sCoasterType* coaster, s3DPoint* point, tag_sBezierControl* control, float lengthA, float lengthB,
                            sPylonControl* pylon, s3DPoint* unusedA, s3DPoint* unusedB, int32_t flag, int32_t unusedC);

// ---- Unit v5562 (src/GetActualBraceTrackPositions.cpp)
extern float gBraceTrackScale2;   // 0x1029cd14  2.0 at start (name invented): the distance array B's points stand above A's along the node's up axis
// 0x1003cc34 (1,012 bytes): array B's points from array A's (the ride set's node when the brace has none of the end-pylon flags); stores the bank in mFE4.
void GetActualBraceTrackPositions(tag_sRideAnimSet* set, tag_sTrackBrace* brace);
// 0x1003c950 (656 bytes): for each control the direction, both span lengths (GetTangentLengths) and the tangents (GetTangentsFromZVector) of both arrays.
void GetActualBraceTangents(tag_sTrackBrace* brace, tag_sRideAnimSet* set);
// 0x1003c59c (864 bytes): the lengths of the spans before and after control `index` of the brace's array A (useA != 0) or B, and the span vectors (nullable).
void GetTangentLengths(uint32_t index, tag_sTrackBrace* brace, float* before, float* after, int32_t useA, s3DPoint* beforeVec, s3DPoint* afterVec);

// ---- Unit v5026 (src/DoMovePylon.cpp): dragging a pylon of the track editor (TPConfig.cpp; names invented)
typedef sTrackBraceData tag_sPylon;
struct sPylonOwner {
    uint8_t mF00[0x70];               // 0x00
    uint32_t mF70;                    // 0x70  Assign3DObjectToAIWorld's third word
};
extern uint32_t gCoasterWord16BC;           // 0x102c16bc  cleared first by both
extern int32_t gPylonMoveZ;                 // 0x102c16c4  the cell the drag started at
extern int32_t gPylonMoveX;                 // 0x102c16c8
extern float gPylonSecondCost;              // 0x102c176c  the cost of the second brace before the drag
extern tag_sPylon* gPylonMoveOrigData;      // 0x102c1774  the dragged brace's pylon record
extern float gPylonMoveHeight;              // 0x102c1778
extern tag_sTrackBrace* gPylonMoveBrace;    // 0x102c177c  the dragged brace
extern tag_sTrackBrace* gPylonMoveMark2;    // 0x102c1780
extern tag_sTrackBrace* gPylonMoveMark1;    // 0x102c1784
tag_sPylon* GetPylonAtCell(uint32_t x, uint32_t z, uint32_t* out);   // 0x1003f954 (124 bytes, src/TPConfig_collision.cpp, unit v5L20)
void MoveBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, tag_sPylon* pylon, uint32_t x, uint32_t z, float* a, float* b, int32_t c,
               int32_t d);   // 0x10054978 (1,232 bytes)
void LLCC_AbortPylonMove();                 // 0x1008f1a4 (292 bytes)
void LLCC_ResetPylonMove();                 // 0x1008f2f4 (16 bytes)
void LLCC_StartPylonMove(int32_t x, int32_t z, int32_t flag);   // 0x1008f0b4 (196 bytes)
void LockPointedAtRide(int32_t flag);       // 0x10067288 (52 bytes, src/TPRide_pointed.cpp, unit v5L06)
void DisableCurrentPylonEditState();        // 0x10053b58 (508 bytes)
void Assign3DObjectToAIWorld(void* instance, uint32_t a, uint32_t b, uint32_t c, int32_t d);   // 0x1005f53c (384 bytes)
float CalculateBraceHeight(tag_sTrackBrace* brace);   // 0x1003e6e0 (192 bytes)
void SetTrackPreview2ndBrace(tag_sTrackBrace* brace, int32_t flag);   // 0x10053a04 (276 bytes)
void SetPotentialPylonArea();               // 0x10041a58 (472 bytes)
uint32_t DoMovePylon(uint32_t x, uint32_t z);   // 0x100579cc (1,024 bytes): drops the dragged brace on the cell, pays the difference; answers the brace's word 0x70 when it moved
bool StartMovePylon(tag_sTrackBrace* brace);    // 0x10057738 (604 bytes): begins dragging; answers whether the pylon is of the second kind

// ---- Unit v5038 (src/DestroyCoasterPylons.cpp): the destroyers of a coaster's track and pylons, and the track position control (TPConfig.cpp; names invented)
struct sBraceInfo;
// A node of a coaster's pylon list (native pointers, the Mac offsets in comments). Flag bit 0: a single brace (word 0x34) with its info at brace + 0x78.
struct sCoasterPylon {
    uint32_t mF00;                    // 0x00
    uint32_t mFlags;                  // 0x04  bit 0: the node holds a brace in mBraces[0]
    uint32_t mF08;                    // 0x08
    uint32_t mF0C;                    // 0x0c  Destroy3DObjectFromAIWorld's first argument
    uint32_t mF10;                    // 0x10  its second
    uint32_t mF14;                    // 0x14
    sPylonControl* mControl;          // 0x18  the pylon's control (word 0 bit 4: the braces are destroyed one by one)
    sCoasterPylon* mNext;             // 0x28 on the Mac
    uint32_t mCount;                  // 0x30  the braces
    tag_sTrackBrace* mBraces[1];      // 0x34  inline, mCount of them
};
// The place on the track: the first control (a sample of the brace mBrace) and the second (of mBraceB); the Mac words 0, 4, 8, 0xc.
struct sTrackPosnControl {
    uint32_t mIndex;                  // 0x00
    tag_sTrackBrace* mBrace;          // 0x04
    uint32_t mIndexB;                 // 0x08
    tag_sTrackBrace* mBraceB;         // 0x0c
};
void DestroyCoasterTrack(tag_sCoasterInstance* instance);       // 0x10050df4 (80 bytes), declared above
void DestroyCoasterPylons(tag_sCoasterInstance* instance);      // 0x10050e84 (192 bytes), declared above
void IncrementTrackPosnControl(sTrackPosnControl* control);     // 0x10051300 (184 bytes)
void DecrementTrackPosnControl(sTrackPosnControl* control);     // 0x100513fc (176 bytes)
void GetTrackPosition(sTrackPosnControl* control, float t, s3DPoint* point);   // 0x100514f0 (84 bytes)
// Untranslated callees (the unit's fakes stand in for them):
void DestroyTrackBrace(tag_sTrackBrace* brace, sPylonControl* pylon);       // 0x1004094c
void Destroy3DObjectFromAIWorld(uint32_t a, uint32_t b);                    // 0x1005f3fc
void DestroyBraceInfo(tag_sTrackBrace* brace, sBraceInfo* info);            // 0x1003e064
void DestroyDynamicMeshSetInstance(tag_sDMeshSet* set, sHeapInfo* heap, uint32_t flags);   // 0x10063264

// ---- Unit v5053 (src/GetNewBraceLoftInterval.cpp): the loft of a new brace and the link level of a pylon (TPCoaster.cpp; names invented)
// The pylon the track editor links: native pointers, so the host offsets differ from the Mac ones in the comments.
struct sLinkPylon {
    uint32_t mF00;                    // 0x00
    uint32_t mFlags;                  // 0x04
    uint8_t mF08[0x18 - 0x08];        // 0x08
    sPylonControl* mControl;          // 0x18  the pylon's control
    float mX;                         // 0x1c
    float mY;                         // 0x20
    float mZ;                         // 0x24
    sLinkPylon* mOther;               // 0x2c  the pylon at the other end (its x, y, z at 0x1c..0x24)
    uint32_t mCount;                  // 0x30  the braces
    tag_sTrackBrace* mBraces[1];      // 0x34  inline, mCount of them
};
// 0x1003e108 (456 bytes): the height of a new brace (and the loft of the brace `brace`, set when the pylon control wants it); `other` is read
// unconditionally in that branch (the original keeps f31 of its caller when it is NULL; not reproduced).
float GetNewBraceLoftInterval(sLinkPylon* pylon, sPylonControl* control, tag_sTrackBrace* brace, tag_sTrackBrace* other, int32_t a, int32_t b,
                              float* out);
// 0x1003e7dc (1,000 bytes): finds the level the pylon's link stands at; answers 1 always.
int32_t DetermineLinkLevel(float height, sLinkPylon* pylon, uint32_t* level, float* out, int32_t quiet);

// ---- Unit v5054 (src/HandleCoasterDataChange.cpp; HandleCoasterDataChange is declared above)
struct sCoasterType;
void DoImmediatePhysicsFeedback(tag_sCoasterInstance* instance);                     // 0x1004d2ec (4 bytes, untranslated)
void SetupBraceAnimFrameData(sCoasterType* type, tag_sTrackBrace* brace);            // 0x1003bb44 (652 bytes, untranslated)
void UpdatePylonStackMeshes(sPylonControl* control, tag_sPylon* pylon, int32_t a, int32_t b);   // 0x1003d958 (1,416 bytes, untranslated)
int32_t ToggleLoftPylonState();                                                      // 0x10052ac4 (876 bytes): always answers 1
extern float gCoasterVerticalFloor;   // 0x1029ce80  (name invented; zero) the lowest the loft drag may go, defined in src/HandleCoasterDataChange.cpp

// ---- Unit v5208 (src/TPConfig_extrude_GetCrossSectionFromParams.cpp)
// 0x1004fd94 (300 bytes): the type (Mac layout, Ptr32 fields: 0x78 the table of 0x2c-byte entries, 0x7c the count; an entry holds the
// float ranges 0x00..0x1c and the answer word at 0x20) entry whose four ranges hold the angle and the orientation's mF04, mF08, mF0C;
// the last entry's word when none does (the last entry is never tested; the first float argument is unused). Needs a count of 1 or more.
// The host type has a native table pointer at 0x78 (mCount follows at 0x80 on the host, 0x7c on the Mac).
struct sCoasterCrossSectionEntry {
    float mRange[8];     // 0x00  low and high of the angle and of the orientation's mF04, mF08, mF0C
    int32_t mAnswer;     // 0x20
    uint8_t mF24[0x2c - 0x24];
};
struct sCoasterType {
    uint8_t mF00[0x78];                      // 0x00
    sCoasterCrossSectionEntry* mTable;       // 0x78
    uint32_t mCount;                         // 0x7c on the Mac
    uint8_t mF84[0x100 - 0x88];
};
int32_t GetCrossSectionFromParams(sCoasterType* type, float unused, sCoasterOrientation* orientation, float angle);

// ---- Unit v5084 (src/DisableCurrentPylonEditState.cpp; DisableCurrentPylonEditState and SetTrackPreview2ndBrace are declared above)
void ExchangeTrackBraces(tag_sTrackBrace* first, tag_sTrackBrace* second);   // 0x10053594 (316 bytes)
void ToggleSwapUpPylonState();                  // 0x10053720 (328 bytes)
void ToggleSwapDownPylonState();                // 0x10053898 (316 bytes)
void ToggleRotatePylonState();                  // 0x100530a8 (540 bytes, untranslated)
void ToggleLoopTurnPylonState();                // 0x10052e5c (540 bytes, untranslated)
void ToggleBankInclinePylonState();             // 0x100532f4 (620 bytes, untranslated)
void UpdatePylonStackStructures(sPylonControl* control, tag_sPylon* pylon);   // 0x1003d844 (196 bytes, untranslated)
extern uint32_t gCoasterWord1788;               // 0x102c1788  (name invented) cleared by DisableCurrentPylonEditState
extern uint32_t gCoasterWord1790;               // 0x102c1790  (name invented) cleared by DisableCurrentPylonEditState
extern uint32_t gCoasterWord16DC;               // 0x102c16dc  (name invented) cleared by DisableCurrentPylonEditState

// ---- Unit v5091 (src/TPCoaster_loaders_NewPylon.cpp): NewPylon and UpdatePylonStackMeshes (names invented)
// A pylon of the editor (0x48 bytes on the Mac, MReleaseAllocF'd). Native pointers: the host offsets of mControl, mX.. and mCount, mBraces
// are those of sLinkPylon (the same object); the Mac offsets are in comments, the two words the layout leaves out are appended.
struct sPylonRecord {
    uint32_t mNumber;                 // 0x00  the pylon's number in the list
    uint32_t mFlags;                  // 0x04  4: the editor's preview pylon
    uint32_t mIndex;                  // 0x08  x + z * the height field's row stride (only when made without a point)
    uint32_t mCellX;                  // 0x0c
    uint32_t mCellZ;                  // 0x10
    uint8_t mF14[0x18 - 0x14];        // 0x14  (the Mac holds the instance here; appended below as mInstance)
    sPylonControl* mControl;          // 0x18
    float mX;                         // 0x1c
    float mY;                         // 0x20
    float mZ;                         // 0x24
    sPylonRecord* mPrev;              // 0x2c  the pylon before it in the list
    uint32_t mCount;                  // 0x30  the braces (written by others)
    tag_sTrackBrace* mBraces[5];      // 0x34..0x47
    sPylonRecord* mNext;              // 0x28  (appended)
    tag_sCoasterInstance* mInstance;  // 0x14  (appended) the editor's coaster
};
sPylonRecord* NewPylon(uint32_t x, uint32_t z, uint32_t flags, int32_t unused, sPylonControl* control, s3DPoint* point);   // 0x1003d610 (500 bytes)
// 0x10237cd8 (untranslated runtime function): a float turned into an integer (the cell of a coordinate); the argument is in f1.
int32_t FloatToCell(double value);
// Unit v5524
int32_t CalcNumTrackSamples(tag_sTrackBrace* brace, float step, float* total);   // 0x1005162c (260 bytes)
// Unit v5664 (src/DestroyCoasterPylons_CalcNumTrackSamples_LinearSampleTrack.cpp)
void GetWrappedTrackPosition(sTrackPosnControl* control, tag_sTrackBrace* brace, float t, s3DPoint* point);   // 0x1005158c (64 bytes, stand-in)
void SetEntranceLinearSamples(tag_sCoasterInstance* instance);   // 0x10049ddc (104 bytes, stand-in)
void LinearSampleTrack(tag_sCoasterInstance* instance);          // 0x10051770 (2,472 bytes)

// ---- Unit v5366 (src/LinkPreviewBrace.cpp): the editor's preview brace (TPCoaster.cpp; names invented)
// The pylon of the instance's list is an sPylonRecord (its mNext is the Mac's +0x28). Copies of the pylon and its braces LinkPreviewBraceToPylon
// keeps (the Mac copies 0x48 and 0x104 bytes; the host copies the whole records).
extern uint32_t gLinkSavedFlag;               // 0x102b6efc  set to 1 with the copies
extern sPylonRecord gLinkSavedPylon;          // 0x102b6eb4
extern tag_sTrackBrace gLinkSavedBraces[5];   // 0x102b69a0
extern float gLinkSavedHeights[5];            // 0x102b698c  the braces' y
extern sPylonRecord* gLinkFoundPylon;         // 0x102b6f00  the pylon of the pointer's cell, set by LinkPreviewBrace
void LinkPreviewBraceToPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, sPylonRecord* pylon, float unused, int32_t copy);   // 0x1003f4fc (740 bytes)
// Untranslated callees (the unit's fakes stand in for them):
int32_t IsValidPylonPosition(uint32_t x, uint32_t z, void* a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);   // 0x100411e4
void LinkBrace(tag_sTrackBrace* brace, sCoasterTrackParams* params, int32_t valid);                            // 0x1003e48c
void LinkPylon(sPylonRecord* pylon, sCoasterTrackParams* params, uint32_t x, uint32_t z, uint32_t index);      // 0x1003e344
void InsertBraceIntoPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, sLinkPylon* pylon, uint32_t level, float height, float* out,
                          int32_t a, int32_t b, int32_t c, int32_t d);                                          // 0x1003ef78
// What mesh->mF28 points at in UpdatePylonStackMeshes (0x14 bytes an entry on the Mac; the host strides by sizeof).
struct sPylonHeadRec {
    uint8_t mF00[0x34];               // 0x00
    float mY;                         // 0x34
};
struct sPylonHeadEntry {
    uint32_t mF00;                    // 0x00
    sPylonHeadRec* mRec;              // 0x04
};
struct sPylonHeadTable {
    uint32_t mF00;                    // 0x00
    sPylonHeadEntry* mEntries;        // 0x04
};
extern uint32_t gPylonStackSkip;      // 0x102b68a0  (defined in src/TPCoaster_loaders_NewPylon.cpp) nonzero: UpdatePylonStackMeshes does nothing unless told to

// ---- Unit v5190 (src/TPCoaster_trains_SetPotentialPylonArea.cpp): SetPotentialPylonArea is declared above
// 0x10041c5c (988 bytes): puts a pylon (the one at the cell, or a new one) and a brace on it into the editor's coaster. Answers 0 when the
// last brace already stands there (flag 0), else 1. `level` 0xff takes the link level DetermineLinkLevel finds. The original's pointer
// types are tag_sPylon** and tag_sTrackBrace**; the pylons it walks are sPylonRecords. With flag != 0 the original reads two uninitialised stack words (zero here).
int32_t PlacePylon(uint32_t x, uint32_t z, int32_t flag, tag_sPylon** outPylon, tag_sTrackBrace** outBrace, uint8_t level, uint32_t extra);
// Untranslated callees (the unit's fakes stand in for them):
void RejoinTrack(tag_sCoasterInstance* instance);            // 0x10053eac (180 bytes)
// 0x10040304 (1,532 bytes): a new brace on the pylon; the three floats are the pylon's x, z and the loft interval.
tag_sTrackBrace* NewTrackBrace(sPylonRecord* pylon, float x, float z, float loft, uint32_t isEnd, sPylonControl* control, uint32_t loops,
                               uint32_t level, uint32_t extra);
void UIHandleEndLoop();                                      // 0x100565cc (108 bytes)
void UIHandleMidLoop();                                      // 0x10056548 (92 bytes)
// Unit v5707 (src/LayoutSetRotatePylon_DeleteLastTrackBrace_UIHandleEndLoop.cpp)
void SetPylonType(uint32_t type);                            // 0x100562d4 (436 bytes)
// ---- Unit v5257 (src/DisableCurrentPylonEditState_SetCoasterEditState.cpp): SetCoasterEditState and RejoinTrack are declared above
void SetCoasterButtonState(uint32_t state, uint32_t arg);    // 0x10057064 (140 bytes, stand-in)
int32_t SetPlaceLoop();                                      // 0x10054554 (92 bytes): always answers 1
// ---- Unit v5365 (src/TPCoaster_instances_DestroyTrackBrace.cpp): DestroyTrackBrace and PlaceEntrancePylons are declared above
tag_sTrackBrace* GetCurSelectedTrackBrace();                 // 0x10052290 (12 bytes, stand-in)
void SetCurSelectedTrackBrace(tag_sTrackBrace* brace);       // 0x100522cc (12 bytes, stand-in)
extern uint32_t gCoasterEditEnabled;                         // 0x102c16d4  (name invented) the enabled buttons mask
extern uint32_t gCoasterEditDown;                            // 0x102c16d8  (name invented) the pressed buttons mask
// ---- Unit v5278 (src/TPCoaster_loaders_GetActualBraceParams.cpp): UpdatePylonStackStructures is declared above
// 0x1003d4cc (184 bytes): walks the braces of the current coaster (the +0x3c block's word at 0x18, linked by +0x64) and recomputes their params.
void GetActualBraceParams();
// The records the two functions walk (native pointers: the host offsets are larger than the Mac's in the comments after the first pointer).
struct sBraceWalkRecord {                 // 0x58 bytes
    uint8_t mPad00[0x0c];
    float mF0C;                           // 0x0c
    uint8_t mPad10[0x28 - 0x10];
    s3DPoint mPoint;                      // 0x28
    uint8_t mPad34[0x58 - 0x34];
};
struct sBraceWalkList {
    uint32_t mCount;                      // 0x00
    sBraceWalkRecord* mRecords;           // 0x04
    sBraceWalkRecord* mEnd;               // 0x08
};
struct sBraceWalkNode {
    uint8_t mPad00[8];
    uint32_t mFlags;                      // 0x08  0x800 cleared unless 0x1000
    uint8_t mPad0C[0x64 - 0x0c];
    sBraceWalkNode* mNext;                // 0x64
    float mF6C;                           // 0x6c
    uint8_t mPad70[0x90 - 0x70];
    sBraceWalkList* mList;                // 0x90
};
struct sBraceWalkData {
    uint8_t mPad00[0x18];
    sBraceWalkNode* mHead;                // 0x18
};
struct sStackQ {
    uint32_t mFlags;                      // 0x00  bit 3 (8)
    uint8_t mPad04[0x30 - 0x04];
    uint8_t mArea30[0x44 - 0x30];         // 0x30
    uint8_t mArea44[0x10];                // 0x44
};
struct sStackCtl {
    uint8_t mPad00[0x18];
    sStackQ* mQ;                          // 0x18
};
struct sStackPylon {
    uint8_t mPad00[8];
    uint32_t mFlags;                      // 0x08  0x100 set unless last, 0x2000
    uint8_t mPad0C[0x74 - 0x0c];
    uint8_t* mF74;                        // 0x74
    uint8_t mArea78[0x84 - 0x78];         // 0x78
    uint8_t mArea84[0x0c];                // 0x84
    uint8_t* mF90;                        // 0x90
    uint8_t mPad94[0x100 - 0x94];
    sStackCtl* mCtl;                      // 0x100
};
struct sPylonStackHead {
    uint8_t mPad00[0x30];
    uint32_t mCount;                      // 0x30
    sStackPylon* mPylons[1];              // 0x34
};
// 0x1004e230 (128 bytes, stand-in): the orientation out buffer is the fourth argument (r6 on the Mac, after the float in r5's slot).
void GetAlignmentParamsSlow(sCoasterTrackParams* params, s3DPoint* point, float f, sCoasterOrientation* orientation, int32_t a, float* out);
void SetInitialOrientationParams(sCoasterOrientation* orient, sCoasterTrackParams* params, int32_t a);   // 0x1005092c (48 bytes, untranslated)
// ---- Unit v5314 (src/TPConfig_LayoutSetMovePylon_SetCoasterEdit.cpp): SetCoasterEdit is declared in LLCellSystem.h
void HandleCoasterButtonClick(uint32_t buttons, uint32_t event);   // 0x10058510 (176 bytes)
void HandleCoasterButtonDown(uint32_t buttons);                    // 0x10056d7c (696 bytes, stand-in)
// The brace mesh lists (native pointers: the host offsets after the first pointer are larger than the Mac's in the comments).
struct sBraceMeshEntry {
    uint8_t mPad00[8];
    uint32_t mFlags;                      // 0x08  0x2000 skipped when the list has flag 0x20
    uint8_t mPad0C[0x70 - 0x0c];
    void* mMesh;                          // 0x70
};
struct sBraceMeshList {
    uint8_t mPad00[4];
    uint32_t mFlags;                      // 0x04  0x20: entries flagged 0x2000 are not counted
    uint8_t mPad08[0x30 - 0x08];
    uint32_t mCount;                      // 0x30
    sBraceMeshEntry* mEntries[1];         // 0x34 (mCount entries)
};
struct sBraceMeshBlock {
    uint8_t mPad00[0x100];
    sBraceMeshList* mList;                // 0x100
};
// 0x100587fc (92 bytes): the brace list of ride set `id` (its mBraceBlock, the Mac's word 0xd4), 0 without braces; *out its count (less one
// with flag 0x20)
sBraceMeshList* GetPylonFromBraceMesh(uint32_t id, uint32_t* out);
void* GetNthBraceMesh(sBraceMeshList* list, uint32_t n);          // 0x10058888 (112 bytes)
// ---- Unit v5157 (src/NewTrackBrace.cpp): NewTrackBrace and UnLinkPreviewBrace are declared above
struct sBraceControl;                                              // the pylon control's +0x30 and +0x44 blocks
void SetupBraceInfo(tag_sTrackBrace* brace, sBraceInfo* info, sBraceControl* control, sPylonControl* pylon);   // 0x1003df2c (212 bytes, stand-in)
void UnLinkBrace(tag_sTrackBrace* brace, sCoasterTrackParams* params);                                          // 0x1003e5c0 (212 bytes, stand-in)
void RestorePylonDetails(tag_sCoasterInstance* instance, tag_sPylon* pylon);                                    // 0x1003ec00 (448 bytes, stand-in)
void RemoveBraceFromCollisionSystem(tag_sTrackBrace* brace, tag_sCoasterInstance* instance);                    // 0x10048fc4 (84 bytes, src/TPConfig_collision.cpp, unit v5L20)
// ---- Unit v5551 (src/TPCoaster_instances_IsValidPylonPosition.cpp): the clamp bounds of IsValidPylonPosition's angle fraction (names invented)
extern float gPylonClampMax;   // 0x1029cd48  1.0f
extern float gPylonClampMin;   // 0x1029cd4c  0.0f
// ---- Unit v5557 (src/TPConfig_extrude_GetCrossSectionFromParams_GenerateExtrusions.cpp): the track's extrusion table (names invented)
extern float gExtrudeBlendMin;                  // 0x1029ce70  0.0f: the lower clamp of a branch blend's ratio
extern float gExtrudeBlendMax;                  // 0x1029ce74  1.0f: the upper clamp
extern sExtrusionParams gExtrudeParamsBuf[2560];   // 0x102b7674  the table GenerateExtrusions fills (0x10 bytes a record on the Mac, 0x18 on the host; 0xa000 bytes up to 0x102c1674)
// The type's two section words: Mac 0x80 and 0x84 (gExtrudeParamsStatic[1] and [0]'s sections); native pointers on the host, at 0x88 and 0x90.
constexpr uint32_t kCoasterTypeSectionB = 0x88;
constexpr uint32_t kCoasterTypeSectionA = 0x90;
int32_t IsEndOfSampleBuffer(tag_sCoasterInstance* instance, float position);   // 0x1004b050 (28 bytes, stand-in)
void SetInitialCoasterInterpParams(sCoasterTrackParams* params);               // 0x100509bc (52 bytes, stand-in)
// 0x1004ff18 (2,536 bytes): walks the track every 0.25 and fills gExtrudeParamsBuf with the keys of each cross section change
void GenerateExtrusions();
// ---- Unit v5647 (src/DisableCurrentPylonEditState_SetCoasterEditState_DoDeleteMultiPylons.cpp)
void DoDeleteMultiPylons();     // 0x10053f98 (1,424 bytes): the delete button of the multi pylon selection
void CoasterPlaceMouseMode();   // 0x1019b018 (220 bytes, stand-in)
// The brace the Mac keeps in the word at 0x102c1788 (gCoasterWord1788 above is only 32 bits, the host's pointer is 64): DoDeleteMultiPylons keeps
// the pointer here and the low 32 bits in gCoasterWord1788; it counts as cleared when gCoasterWord1788 no longer matches (another unit zeroes it).
extern tag_sTrackBrace* gCoasterStartBrace;
// ---- Unit v5667 (src/GetNewBraceLoftInterval_RestorePylonDetails.cpp): CalculateBraceHeight, RestorePylonDetails and InsertBraceIntoPylon are declared above
// 0x1003ee10 (256 bytes): lowers the top braces of a pylon to the control's minimum interval while the stack is too tall; answers 1 if it changed any
int32_t SquishPylonBracesIfRequired(tag_sCoasterInstance* instance, sPylonControl* control, sLinkPylon* pylon);
// ---- Unit v5673 (src/GetActualBraceTrackPositions_RecalcBraceTangents.cpp)
void RecalcBraceTangents();                                                                  // 0x1003d080 (1,056 bytes)
void GetRequiredBraceTangents(tag_sTrackBrace* brace, sCoasterType* type, sCoasterTrackParams* params);   // 0x1003aef4 (3,048 bytes, stand-in)
float GetCarWantedSpeedAtHeight(sCoasterType* type, float a, float b, float c, float d);       // 0x1004aedc (132 bytes, stand-in)
float GetBankAtBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, float f);        // 0x1004b1f8 (216 bytes, stand-in)
// Unit v5688 (src/TPCoaster_ticks_HandleMultiBracePreviewChange.cpp)
tag_sPylon* GetPylonUnderCursor(uint32_t* a, uint32_t* b, uint32_t* c);   // 0x1003f848 (216 bytes, stand-in)
// Unit v5731 (src/GetNewBraceLoftInterval_SetupBraceInfo.cpp): the brace info blocks and the track's brace/pylon lists (names invented)
// The Mac's {count, 0x4 block, 0x8 block}; the blocks hold count entries of 0x58 bytes. Native pointers, so the host offsets are 0, 8, 0x10.
struct sBraceInfo {
    uint32_t mCount;                  // 0x00
    void* mBlockA;                    // 0x04
    void* mBlockB;                    // 0x08
};
// The control's block at +0x30 or +0x44 of the pylon control (all words, so the host offsets are the Mac's).
struct sBraceControl {
    uint8_t mF00[0x0c];               // 0x00
    uint32_t mCount;                  // 0x0c  how many braces
    uint32_t mBase;                   // 0x10  the first id looked up
};
// The track parameters block (the instance's +0x3c block). Native pointers: the host offsets of the pointers are further than the Mac's.
struct sCoasterTrackParams {
    uint32_t mF00;                    // 0x00
    uint32_t mPylonCount;             // 0x04
    uint32_t mBraceCount;             // 0x08
    sPylonRecord* mPylonTail;         // 0x14  the last pylon
    tag_sTrackBrace* mBraceHead;      // 0x1c  the first brace
    tag_sTrackBrace* mBraceTail;      // 0x24  the last brace
    tag_sTrackBrace* mLastBrace;      // 0x18  (unit v51074, appended: the Mac word, the host's is further) GetTrackBrace's start, the chain of the braces' +0x64
};
// ---- Unit v5893 (src/DisableCurrentPylonEditState_SetCoasterEditState_DeletePylon.cpp): the pylon edit helpers (names invented)
// 0x10054640 (380 bytes): takes the brace out of its pylon, closing the gap; the later braces get the removed one's ride node height and re-derive their interval.
void RemoveTrackBraceFromPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace);
// 0x10054814 (284 bytes): unlinks the pylon from the instance's list, renumbers the rest, frees it (flag != 0: the 3D object stays)
void DeletePylon(tag_sCoasterInstance* instance, tag_sPylon* pylon, int32_t flag);
// 0x101ed9a0 (288 bytes, stand-in)
void MoveRide(tag_sRideAnimSet* set, int32_t x, int32_t z, void* params, int32_t a, int32_t b, uint32_t c);
// Unit v5900 (src/AllocateQueue_UnloadTrain_2.cpp)
// Hands up to `count` cars' peeps (the entrance queue's, drawn at random among the cars first..first+count-1) to the train's cars.
void LoadTrain(tag_sCoasterInstance* instance, sTrainInstance* train, uint32_t first, uint32_t count);   // 0x10049bf4 (412 bytes)
// The first and the last car (indexes) whose span overlaps the station [0xf8, 0xfc].
void GetCarsInStation(tag_sCoasterInstance* instance, sTrainInstance* train, uint32_t* first, uint32_t* last);   // 0x10049e8c (220 bytes)
extern int32_t gCoasterCapacityFloor;     // 0x1029ce28  CoasterSetCapacity's lower bound (a max with it; bss, name invented)
// Unit v51008 (src/TPConfig_extrude_GenerateTrackSubdivisions_2.cpp)
int32_t GenerateTrackSubdivisions(tag_sTrackBrace* a, tag_sTrackBrace* b, sExtrusionParams* params, float* out, float span);   // 0x1004e8c0 (260 bytes)
void LayoutRecordCoasterSpeed(tag_sCoasterInstance* instance, uint32_t speed);   // 0x100590a0 (56 bytes, stand-in)
void FlagRecalcAllCoasterCollisions(tag_sCoasterInstance* instance);             // 0x10049078 (92 bytes, src/TPConfig_collision.cpp, unit v5L20)
// Unit v51024 (src/LevelSystems_GetRequiredBraceTangents_2.cpp): the clamp constants of GetRequiredBraceTangents (names invented)
extern float gBraceTangentOne;         // 0x1029cd1c  1.0f
extern float gBraceTangentMinusOne;    // 0x1029cd18  -1.0f
extern int32_t gKickPeepsFloor;          // 0x1029ce2c  KickPeepsFromEntranceQueue's lower bound on the number kicked (a max with it; bss, name invented)
// Unit v5992 (src/DestroyCoasterPylons_SetupBraceBezierCtrlParams.cpp; names invented)
void SetupBraceBezierCtrlParams(tag_sCoasterInstance* instance);   // 0x10050a3c (880 bytes)
void RecalcTrackBounds(tag_sCoasterInstance* instance);            // 0x10047818 (96 bytes, src/TPConfig_collision.cpp, unit v5L20)
void DoCollisionChecking(tag_sCoasterInstance* instance);          // 0x10048f64 (32 bytes, src/TPConfig_collision.cpp, unit v5L20)
extern float gBraceBezierMin;                       // 0x1029ce78  0.0f, the lower bound of the bezier weight after the pivot
extern float gBraceBezierMax;                       // 0x1029ce7c  1.0f, the upper bound
extern const uint32_t* gTrackTextureCacheType;      // 0x102b74bc  the coaster type whose texture flags gExtrudeFaceStoreB holds
extern float gTrackSubdivTimes[100];                // 0x102b74c0  GenerateTrackSubdivisions' output (up to 0x102b7650)
// Unit v51074 (src/DestroyCoasterPylons_SetupBraceBezierCtrlParams_SetInitialOrientationParams.cpp)
int32_t IsTrackClosed(tag_sCoasterInstance* instance);   // 0x10052780 (36 bytes; the overload on the coaster's object is 0x10052850)
void SetPlaceNormalPylon();                              // 0x10058b68 (36 bytes)
void SetPlaceSpecialPylon();                             // 0x10058bb8 (36 bytes)
void LayoutDeleteLastTrackBrace();                       // 0x10058f4c (32 bytes)

// ---- Unit v5L20 (src/TPConfig_collision.cpp): the coaster collision system (TPConfig.cpp on the Mac; names invented). Each brace keeps
// the box of the track from it back to the brace before it (mCollisionBounds) and the braces whose track crosses that section
// (mCollisions, mFB0 of them, at most 8; a brace with more is flagged 0x400000). Flag 0x200000 asks for the brace's collisions to be
// worked out again, 0x10000 keeps the brace out of them. The radius widening each box is the pylon control's word 0x58.
// One stretch of track between two samples and its box (0x1c bytes on the Mac; MReleaseAllocF'd, the instance's mSegmentSpan of them).
struct sTrackChunk {
    uint32_t mSample;                 // 0x00  the index of the first sample
    sAlignedBox mBounds;              // 0x04
};
static_assert(sizeof(sTrackChunk) == 0x1c, "track chunk size");
extern sTrackChunk* gCollisionChunksA;       // 0x102b7460  the first track's chunks (CheckAllTrackToTrackCollisions' blocks)
extern sTrackChunk* gCollisionChunksB;       // 0x102b745c  the second track's
extern void* gCollisionBlockA;               // 0x102b7468  0x78 bytes per chunk, allocated and freed only
extern void* gCollisionBlockB;               // 0x102b7464  the same
extern sTrackSample* gCollisionSampleA;      // 0x102b7330  the first sample of the pair DoProperChunkCollision tests
extern sTrackSample* gCollisionSampleB;      // 0x102b732c
extern tag_sCoasterType* gCollisionTypeA;    // 0x102b7458  (the Mac keeps the address of the type's mPairs and mPairCount)
extern tag_sCoasterType* gCollisionTypeB;    // 0x102b7454
// The two cross sections of each track, the type's pairs turned to the samples' orientation: ring B (0x102b7334) and ring A
// (0x102b73c4), 0x90 bytes each and in that order, so the host keeps them in one array (B then A).
extern s3DPoint gCollisionRings[24];
extern s3DPoint* gCollisionCorner0;          // 0x102b7328  the four corners of ring A's face being tested
extern s3DPoint* gCollisionCorner1;          // 0x102b7324
extern s3DPoint* gCollisionCorner2;          // 0x102b7320
extern s3DPoint* gCollisionCorner3;          // 0x102b731c
void GetTrackBounds(tag_sCoasterInstance* instance, tag_sTrackBrace* a, tag_sTrackBrace* b, sAlignedBox* box);   // 0x10047634 (372 bytes)
void GenerateSamplesInBoundArea(tag_sCoasterInstance* instance, tag_sTrackBrace* a, tag_sTrackBrace* b, const sAlignedBox* box,
                                sTrackChunk* chunks, uint32_t* count);                                          // 0x100478b8 (800 bytes)
int32_t DoProperChunkCollision(sTrackChunk* a, sTrackChunk* b);                                                  // 0x10047c64 (2,052 bytes)
float DotProduct3D(const s3DPoint* a, const s3DPoint* b);                                                        // 0x100484b0 (40 bytes)
void CheckTrackToTrackCollisions(tag_sCoasterInstance* instanceA, tag_sCoasterInstance* instanceB, tag_sTrackBrace* a0,
                                 tag_sTrackBrace* a1, tag_sTrackBrace* b0, tag_sTrackBrace* b1);                 // 0x10048510 (1,196 bytes)
void RemoveAllCollisionsFromBrace(tag_sCoasterInstance* instance, tag_sTrackBrace* brace);                       // 0x10048a64 (528 bytes)
void CheckTrackToAllTracksCollisions(tag_sCoasterInstance* instance, tag_sTrackBrace* brace);                    // 0x10048cd0 (208 bytes)
void CheckAllTrackToTrackCollisions(tag_sCoasterInstance* instance);                                             // 0x10048e00 (280 bytes)
