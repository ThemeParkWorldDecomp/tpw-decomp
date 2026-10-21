// The park's second-stage systems that LevelInit2 (0x100639b0) and LevelDeInit (0x10063b80) call (unit w111, src/game/park/TPHField.cpp, src/game/rides/CoasterType.cpp, src/game/rides/RidePreview.cpp, src/game/rides/TPCoaster.cpp, src/game/TPLevel.cpp, src/game/MeshLoad.cpp, src/game/park/TPHoardings.cpp
// unless noted; the main app's TPRide.cpp, Debug.cpp, TPCoaster.cpp and rsse.cpp ranges). The coaster types are in TPCoaster.h, the
// cell system in LLCellSystem.h, the script system and the dynamic cell meshes in analyser.h, PT_SetRange in tb_hfield.h.
#pragma once
#include <cstdint>

struct sDTextureSet;   // include/TPCoaster.h

// 0x1005be44 (124 bytes): "<theme path>:Terrain:Base.map" through FindFilePath into the world's map (CMap::LoadAttribMap); true
// when the map loaded. (The Mac formats into a 1,032 byte buffer with sprintf; the port bounds the write.)
bool LoadLevelAttributeMap();
// 0x1005c7e8 (60 bytes) and 0x1005c850 (60 bytes): the poly search table, gPolySearchCount (TPEngine.h) entries of 12 bytes from
// MReleaseAllocF (flag 1); the second frees it and clears the pointer.
void SetupForPolySearch();
void SetdownFromPolySearch();
extern void* gPolySearchTable;              // 0x102c1e1c
// 0x1005f6f4 (24 bytes): the two heightmap words (floats, names invented) are both 0.02f.
void SetupHeightmapSystem();
extern float gHeightmapScaleA;              // 0x102c1e0c  set first
extern float gHeightmapScaleB;              // 0x102c1e08
// 0x10061448 (40 bytes): gives back the ride hoardings' textures.
void ClosedownRideHoardings();
extern sDTextureSet gRideHoardingTextures;  // 0x1029cf58 (initialised data, defined in src/game/DynamicMeshSet.cpp)
// 0x1006446c (280 bytes): takes the park's light (gGlobalLight, LevelInit.h) out of the map's list 0x70 and deletes it: type 2
// the directional light and then the point light (never made, so a NULL link), 3 the point light, 1 sets GlobalDiffuse to
// (1, 0, 0, 0) and then does what type 2 does, 0 reports "Invalid lightsource creation attempted" through DataErrorHandler; any
// other type does nothing.
void DestroyLevelLights();
// 0x1006d1d0 (104 bytes, Debug.cpp) and 0x1006d26c (88 bytes): the highlight colour buffers, 12 bytes for each of
// gMaxMeshFaces4 + gMaxMeshF3E and 4 for each of gMaxMeshNodes (TPEngine.h); the second frees both (the faces first) and clears them.
void CreateHighlightColourBuffers();
void DestroyHighlightColourBuffers();
extern void* gHighlightFaceColours;         // 0x102d1f10
extern void* gHighlightNodeColours;         // 0x102d1f0c
// 0x100b9834 (72 bytes, rsse.cpp): the Mac shows a warning box (OK and Cancel, titled "M3D2 Exporter - Data Error") and answers
// false for Cancel. The port writes the text to the log through SamsUtilities::UDebug and answers true, as an OK.
bool DataErrorHandler(char* text);
// 0x1005b7a8 (112 bytes, src/game/park/TPHField.cpp, unit w112c; TPRide.cpp in the inventory): 0 for a cell outside the height field or one whose
// quad has bit 0 set, 1 for the rest, and also 1 when there is no height field. x and y are compared as unsigned numbers.
int32_t HFieldCellStatus(int32_t x, int32_t y);

// Unit v5258 (src/game/park/TPHField.cpp)
struct tag_sNode;
struct s3DPoint;
struct tag_sMesh;
struct tag_sRideAnimSet;
// 0x1005c8b8 (140 bytes): the node's own position (its matrix row 3, or zero without a parent link at +4), then the sum of the
// positions of every ancestor that itself has a parent link. The third argument is unused.
void GetNodePositionSimple(tag_sNode* node, s3DPoint* out, int32_t unused);
// 0x1005cc10 (156 bytes) is declared in rsse.h.
// 0x1005c988 (584 bytes, not translated yet)
void SetupMeshForPolySearchSimple(tag_sMesh* mesh, int32_t flag);
// 0x1005f080 (184 bytes, not translated yet)
int32_t GetRideIDAtCell(int32_t x, int32_t y);
// 0x1005c1b0 (112 bytes): the ride set at the cell, x and y clamped (unsigned) to the height field's size at +0x18 and +0x1c; 0 for ride 0.
tag_sRideAnimSet* GetMeshPtrAtHFieldCell(uint32_t x, uint32_t y);

// Unit v5902 (src/game/park/TPHField.cpp)
struct sMatrix;
// 0x1005c2b0 (676 bytes): the matrix is passed by value (16 words: r4..r10 and the stack).
void UpdateHFieldDarkeningNormals(tag_sNode* node, sMatrix matrix);
extern int32_t gParkDarkeningFlag;   // 0x102c1e04 (name invented): RecalculateParkDarkening clears it
