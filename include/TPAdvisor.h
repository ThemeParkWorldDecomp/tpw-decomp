// TPAdvisor.cpp (main app): the advisor's 3D model and the text file reader. src/TPAdvisor.cpp translates the
// text file reader and the advisor's model (LoadAdvisor, DestroyAdvisor and their neighbours, 0x10031bc0..0x1003228c, the
// slots' constructor 0x10032454). BMP_Tick is in this file's range in the inventory.
#pragma once
#include <cstdint>

#include "Light.h"
#include "tb_matrix.h"

struct tag_sRideAnimSet;

// One advisor slot (sAdvisorStore, 0xbc bytes on the Mac; the game keeps four. The lobby loads slot 0, ADV_Init looks at
// slots 0 and 1, nothing translated uses 2 and 3). The array is constructed by __sinit_TPAdvisor_cpp (0x1003228c); each
// slot by 0x10032454: the light is made (flags 2, a directional light), then pointed along +z and made white.
// The Mac embeds the light behind a handle (+0x48) and keeps its map link base in the object's tail; the host's
// CLight is its own object.
struct sAdvisorStore {
    sAdvisorStore();                // 0x10032454
    ~sAdvisorStore() {}             // 0x10032358 (the inlined CLight destructor chain, then delete when the hidden flag is positive)

    tag_sRideAnimSet* mSet;         // 0x00  the type's animation set (LoadRideAnimSet)
    tag_sRideAnimSet* mInstance;    // 0x04  the advisor's own copy (InstanceRideAnimSet)
    sMatrix mMatrix;                // 0x08  where the model stands; the instance draws with it (set flag 8)
    CLight mLight;                  // 0x48  the light linked into the map (list 0x10) while the advisor is loaded
};
extern sAdvisorStore gAdvisorStore[4];   // 0x102b56fc

// LoadAdvisor's fourth argument is the slot (the game's only call asks for 0), `variant` is the text in
// "Advisor%s.md2" (NULL: Advisor.md2) and `path` and `folder` go to SetupDirectoryForMeshLoad with it.
void LoadAdvisor(char* path, char* folder, char* variant, uint32_t which);   // 0x10031bc0
void DisableAdvisorDisplay(uint32_t which);                       // 0x10031de8
void TickAdvisor(uint32_t which);                                 // 0x10031ed0
void DestroyAdvisor(uint32_t which);                              // 0x10031f38
// The answer is SetCurRideAnim's time in milliseconds, or 1000 when the slot has no advisor.
uint32_t StartAdvisorAnimation(uint32_t animation, uint32_t which);   // 0x10031ff4
// Show (Enable) or hide (Disable) the node of the embedded entry (flag 0x400) with that id.
void EnableAdvisorAccessory(uint32_t accessory, uint32_t which);      // 0x100320a8
void DisableAdvisorAccessory(uint32_t accessory, uint32_t which);     // 0x10032178
tag_sRideAnimSet* GetAdvisorRideAnim(uint32_t which);                 // 0x1003224c
void BMP_Tick();                                                  // 0x10028cb0
// 0x10028030 (3,156 bytes, untranslated): draws the cars of a bumper car ride; `ride` is the word at +0x24 of the CObject, which the
// callee masks with 0xff (unit w121).
void BMP_DrawCarsForRide(int32_t ride, float step);
// Unit v5672 (src/BMP_DrawCarsForRide.cpp): its data (names invented).
extern float gDrawCarsAngles[3];     // 0x1029ca9c, initial 0
extern float gDrawCarsClampLo;       // 0x1029caa8, initial -1.0
extern float gDrawCarsClampHi;       // 0x1029caac, initial 1.0
extern int8_t gDrawCarsInit;         // 0x102b4eb8 (a char)
extern float gDrawCarsScale;         // 0x102b4ebc, 49143.0 once gDrawCarsInit is set
// 0x10026f20 (392 bytes, untranslated; unit w153): the track sections of the ride at the object's +0x24; the outs are unnamed.
int32_t BMP_CountTrackSections(int32_t ride, int32_t* a, int32_t* b, int32_t* c, int32_t* d);

// Unit v51069 (src/AdvisorScoring_sinit_Advisor_cpp.cpp). __arraydtor$3112 0x10032300 destroys gAdvisorStore (4 slots of 0xbc bytes on
// the Mac) through __destroy_arr; the TVector 0x10266010 of ~sAdvisorStore is a stand-in.
extern unsigned char gAdvisorStoreDtorVector[16];   // stand-in for 0x10266010
void __arraydtor_3112();                            // 0x10032300
// The data node table GetDataNodeWorldMatrix reads (tag_sMesh::mF28): entries of 0x14 bytes on the Mac, the matrix pointer at +4.
struct tagDataNodeEntry {
    uint32_t mF00;       // 0x00
    float* mMatrix;      // 0x04 on the Mac (a native pointer here)
    uint32_t mF08[3];    // 0x08
};
struct tagDataNodeTable {
    uint32_t mF00;                   // 0x00
    tagDataNodeEntry* mEntries;      // 0x04 on the Mac
};

// A text being parsed; the cursor and the position advance together.
struct sFileParse {
    char* mCursor;     // 0x00
    uint32_t mPos;     // 0x04
    uint32_t mSize;    // 0x08
};
// 0x10030c08: skips to the next letter, digit, punctuation or comment character; nonzero at
// the end of the text.
int32_t SkipWhitespace(sFileParse* parse);
int32_t IsComment(sFileParse* parse);    // 0x10030d28: '#' or ';' at the cursor
int32_t IsLetter(sFileParse* parse);     // 0x10030d80
int32_t IsNumber(sFileParse* parse);     // 0x10030dec
int32_t IsMiscChar(sFileParse* parse);   // 0x10030e44: printable ASCII that is no letter, digit or space
// 0x10030edc: nonzero when the character at the cursor is none of those.
int32_t IsWhiteSpace(sFileParse* parse);
// 0x10031014: copies the run of characters at the cursor that are letters, digits,
// punctuation or comment characters (each class when its flag is set) into `text`, at most
// size - 1 of them; nonzero when the end of the text was reached.
int32_t GetString(sFileParse* parse, char* text, int32_t size, int32_t letters, int32_t digits, int32_t punctuation,
                  int32_t comments);
