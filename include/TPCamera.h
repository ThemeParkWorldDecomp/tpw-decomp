// TPCamera.cpp (main app): the park camera. PARTIAL (InitCamera in src/TPCamera_init.cpp, HandleCamera in src/TPCamera_handle.cpp,
// Will_HandleCameraMessages in src/TPCamera_messages.cpp).
// The globals are in this file's part of the BSS (0x102b59f8..0x102b5c08); their names are invented.
#pragma once
#include <cstdint>

#include "tb_matrix.h"

void HandleCamera();   // 0x1003491c
void SetExternalCamera(int32_t enabled); // 0x10033dc0
extern sMatrix gExternalCameraMatrix;    // 0x102b5a50, supplied by the lobby
extern sMatrix gInverseCameraMatrix;     // 0x102b5ab0
extern sMatrix* gCurrentCameraMatrix;    // 0x102b5af0
// The external-camera branch of HandleCamera, used by the lobby. The launcher still
// reports the unimplemented park-camera branch when this returns false.
bool HandleExternalCamera();
void RoundRotationToNearest90Degrees();   // 0x100346a0

// Bits of gFirstPersonFlags that mean a first-person view (riding a ride, walking).
constexpr uint32_t kFirstPersonAny = 0x16;
extern uint32_t gFirstPersonFlags;   // 0x102b5a40
// The camera: row 1 up, row 2 forward, row 3 the position (0x102b5b74).
extern sMatrix gCameraMatrix;
// The point the camera looks at (0x102b5bd8, three floats).
extern float gCameraTarget[3];
// The ride being ridden in first person (0x102b5a28).
struct tag_sRideAnimSet;
extern tag_sRideAnimSet* gFirstPersonRideSet;
// Bit of gFirstPersonFlags that says the player rides a ride in first person (IsRideFirstPersonRide, 0x100329cc,
// src/lobby_exit.cpp).
constexpr uint32_t kFirstPersonRideBit = 4;
// The instance of that ride (0x102b5a3c; the name is invented); 0 for none.
extern uint32_t gFirstPersonRideInstance;

// ---- InitCamera (src/TPCamera_init.cpp, unit w106)
// 0x100334d4 (344 bytes): the camera at the start of a park: the home position, the three camera matrices as identities with the
// position in the row 3 of the main one, the engine's view and projection matrices and gamut from the field of view, and the
// camera's modes and time stamp reset.
void InitCamera();
// 0x10032558 (4 bytes): returns at once (the first-person ride mesh is loaded elsewhere).
void Load1stPersonMesh();
// Initialised data at 0x1029cc68..0x1029cc87 (names invented). InitCamera sets mF68 to 110.0 (its initial value), copies the start
// position over the position and reads the field of view.
extern float gCameraF68;                // 0x1029cc68  110.0
extern float gCameraStartPosition[3];   // 0x1029cc6c  475.0, 0.0, 175.0
extern float gCameraPosition[3];        // 0x1029cc78  the start position at first (__sinit_TPCamera_cpp copies it); the matrix's row 3 is set from it
extern float gCameraFov;                // 0x1029cc84  pi / 2 (radians); InitCamera makes it 90 degrees for the projection
// BSS words and matrices InitCamera sets (0x102b5a90..0x102b5bf4; names invented).
extern uint32_t gCameraClockStamp;      // 0x102b5a90  gClock.ReadFixed() at the start
extern uint32_t gCameraModeA;           // 0x102b5aa4  InitCamera: (itself & 0x200) | 0x80
extern uint32_t gCameraModeB;           // 0x102b5aa8  the same value
extern uint32_t gCameraF5AAC;           // 0x102b5aac  1
extern sMatrix gCameraMatrixA;          // 0x102b5af4  an identity
extern sMatrix gCameraMatrixB;          // 0x102b5b34  an identity
extern float gCameraF5BB4[3];           // 0x102b5bb4  the camera velocity (unit w135 fixed the type; InitCamera clears it)
extern float gCameraF5BF4;              // 0x102b5bf4  0.0

// ---- Unit w134 (CalculatePointerWorldPosn, src/TPRide_pointer.cpp; names invented)
struct s3DPoint;
extern float gPointerScreenXY[2];   // 0x102b5a94  the pointer's position in screen pixels (x, y); set by Will_HandleCameraMessages
extern s3DPoint gCameraViewPoint;   // 0x102b5bc0  the point the pointer's ray starts from (x +0, y +4, z +8)

// ---- Unit w135 (HandleCamera, src/TPCamera_handle.cpp; every name below is invented, the addresses are the original's)
// The park camera's frame: keyboard and edge-scroll movement of the position, the smoothed yaw, the zoom, the first-person walk and ride
// views, the camera matrix and the engine's view matrix. gCameraF68 is the zoom (a distance), gCameraF5BF4 the yaw the keys turn,
// gCameraF5BB4 the velocity, gCameraModeA the held-key and mode bits.
extern float gCameraYaw;               // 0x102b5bf0  the smoothed yaw the matrix is built from
extern float gCameraPitch;             // 0x102b5a4c  the pitch of the park view
extern float gCameraTilt[2];           // 0x102b5a44  the tilt from the velocity (x, z)
extern float gCameraGroundHeight;      // 0x102b5a24  the smoothed ground height under the target
extern sMatrix* gFirstPersonRideMatrix; // 0x102b5a2c  the ridden ride's matrix (first person on a ride)
extern float gFirstPersonTurn;         // 0x102b5be4  the turn rate of the first-person view
extern float gFirstPersonPitch;        // 0x102b5be8  the pitch of the first-person view
extern float gCameraPointerAnchor[2];  // 0x102b5a9c  the pointer position when the right button went down
extern int8_t gCameraWideZoomOnce;     // 0x102b5a08  set when the minimum zoom has been given its value
extern float gCameraWideZoomMin;       // 0x102b5a0c  40.0 once set
extern int8_t gCameraWideZoomMaxOnce;  // 0x102b5a00  set when the maximum zoom has been given its value
extern float gCameraWideZoomMax;       // 0x102b5a04  300.0 once set
extern int8_t gCameraWalkOnce;         // 0x102b59f8  set once by the first-person block
extern float gCameraF59FC;             // 0x102b59fc  0.0 once set; nothing else reads it
// Initialised data (0x1029cc60..0x1029ccb8).
extern float gCameraYawSnap;           // 0x1029cc60  5e-4: the smoothed yaw snaps to the target inside this
extern float gCameraStopSpeed;         // 0x1029cc64  1e-4: a velocity this small in all three axes is zeroed
extern float gCameraYawRateIso;        // 0x1029cc9c  200.0 (isometric)
extern float gCameraYawRate;           // 0x1029cca0  140.0
extern float gCameraPitchMin;          // 0x1029cca4  0.0
extern float gCameraPitchMax;          // 0x1029cca8  1.4707963
extern float gCameraTiltMinX;          // 0x1029ccac  0.0
extern float gCameraTiltMaxX;          // 0x1029ccb0  0.0
extern float gCameraTiltMinZ;          // 0x1029ccb4  0.0
extern float gCameraTiltMaxZ;          // 0x1029ccb8  0.0
// Callees HandleCamera makes (src/TPCamera_callees.cpp and src/TPRide_height.cpp, unit w140; the unit tests of HandleCamera fake them).
float DetermineHFieldHeight(float x, float z, int32_t flag);    // 0x1005ea70  the ground height at (x, z)
int32_t GetNextGridsquare(s3DPoint* from, s3DPoint* step, s3DPoint* out);   // 0x10033e14  moves from by step to the next grid square's edge; nonzero: more to go
int32_t RideRideIfRequired(s3DPoint* point);                    // 0x100345f8  nonzero: the point is on a ride (the walk stops)
int32_t MatrixDifferenceTiny(sMatrix* a, sMatrix* b);           // 0x10034784  NONZERO when the two matrices are almost the same (the sum of the 12 absolute differences of rows 0 to 3, x y z, is below 1e-5f)
void ApplyHeadsOnRidesBumpage(sMatrix* ride, sMatrix* previous, float dt, sMatrix* out);   // 0x100336f4

// ---- Lead 2026-10-14 (Will_HandleCameraMessages, src/TPCamera_messages.cpp; the four statics' names are invented)
// The game window's messages for the camera: the pointer's position (screen pixels, from the UI's 2048 x 1536 message space), bit
// 0x100 of gFirstPersonFlags while the pointer is outside the window (no edge scroll), the mouse buttons' bits of gCameraModeA (left 1,
// middle 2, right 4), the wheel's zoom (gCameraF68 +- 10) and, with the middle button held, the frame's turn of gCameraF5BF4.
void Will_HandleCameraMessages(int32_t message, int32_t a, int32_t b);   // 0x100331c4
extern int8_t gCameraYawDragScaleOnce;    // 0x102b5a18  set when the scale below has its value
extern float gCameraYawDragScale;         // 0x102b5a1c  1/2048 once set: the middle-button turn's factor
extern int8_t gCameraMessagesF5A10Once;   // 0x102b5a10
extern float gCameraMessagesF5A14;        // 0x102b5a14  0.0 once set, never read here

// ---- Unit w140 (src/TPCamera_callees.cpp, src/TPRide_height.cpp; the five globals' names are invented, the addresses and values are the original's)
// The camera's small callees: SetCameraPOI points the camera at a place (zeroes the velocity, then copies each non-null input), the
// grid walk's constants and the matrix epsilon are initialised data in the camera's part of the data segment.
void SetCameraPOI(float* x, float* z, float* zoom, float* yaw, float* value);   // 0x10033650
extern float gCameraMatrixEpsilon;   // 0x1029cc5c  1e-5f: MatrixDifferenceTiny's limit
extern float gCameraPOIValue;        // 0x1029cc88  1.0f: SetCameraPOI's fifth input is stored here
extern float gGridEdgeOffset;        // 0x1029cc90  9.999f: GetNextGridsquare puts a point that walks up this far into the next square
extern float gGridStepScale;         // 0x1029cc94  1.01f: GetNextGridsquare splits a tie between the two axes with it
extern float gGridNudge;             // 0x1029cc98  0.001f: GetNextGridsquare's push over a square's edge
enum NavMode : int32_t {};   // MAP_wall_in_way's last argument (GetNextGridsquare passes 2)
// 0x10101ca4 (1,724 bytes, src/MapDelta_wall.cpp, unit w141): nonzero when a wall stands in the way of the square (x, z) in the direction dir
// (0 and 2 along z, 1 and 3 along x as GetNextGridsquare uses them).
int32_t MAP_wall_in_way(uint8_t x, uint8_t z, uint8_t dir, NavMode mode);

// Unit w145 (src/MapDelta_map.cpp): the walk planner's path buffer (0x208 bytes on the caller's stack; the names are invented).
struct IDXTYPEUniqueNameMapId;
template<class Tag, class T> struct tag_IDX_TYPE;
struct wallhug_waypoint { uint8_t mX; uint8_t mY; };
struct wallhug_path {
    uint8_t mCount;                 // 0x0  the number of waypoints
    uint8_t mF01;                   // 0x1
    uint8_t mStartX;                // 0x2
    uint8_t mStartY;                // 0x3
    uint8_t mEndX;                  // 0x4
    uint8_t mEndY;                  // 0x5
    wallhug_waypoint mPoints[(0x208 - 6) / 2];   // 0x6
};
static_assert(sizeof(wallhug_path) == 0x208, "the path buffer is 0x208 bytes");

// Unit w195 (src/World_sight.cpp; the globals' names are invented, the addresses are the original's): the line of sight and the path smoothing.
// bresenham_start sets up a walk between two squares in the six globals below; line_of_sight walks it and answers 1 when no wall stands in the way.
void bresenham_start(wallhug_waypoint from, wallhug_waypoint to);                  // 0x101360ac (324)
int32_t line_of_sight(wallhug_waypoint from, wallhug_waypoint to);                 // 0x1013623c (532)
int32_t line_of_sight_cleanup(wallhug_path* path, uint32_t first);                 // 0x10136498 (248)
uint32_t wallhug_cleanup(wallhug_path* path, uint32_t result);                     // 0x101365cc (2168) answers its second argument
uint32_t wallhug_tricky(wallhug_path* path, NavMode mode);                         // 0x10136e7c (76) answers wallhug_cleanup's answer
extern int32_t gBresenhamXMajor;   // 0x1034cab8  1 when |dy| < |dx|, else 0
extern int32_t gBresenhamXDir;     // 0x1034cabc  1 when dx > 0, else 3
extern int32_t gBresenhamYDir;     // 0x1034cac0  2 when dy > 0, else 0
extern int32_t gBresenhamError;    // 0x1034cac4  |dx| / 2 + |dy| / 2 at the start
extern int32_t gBresenhamDY;       // 0x1034cac8  |dy|
extern int32_t gBresenhamDX;       // 0x1034cacc  |dx|
extern NavMode gWallhugNavMode;    // 0x1034cad0  wallhug_tricky stores its mode here; the cleanup and line_of_sight pass it on
extern const int8_t gWallhugStep[4][2];   // 0x102a45a4  the step (x, y) of directions 0 to 3

// Unit w196 (src/World_wallhug.cpp; the six globals' names are invented, the addresses are the original's): the walk planner.
// wallhug_trivial plans a walk from (mStartX, mStartY) to (mEndX, mEndY) into the path; 0x70000000 when there is none.
// wallhug_continue_trivial walks the line from `start` (a waypoint by value) to the end, sending two wall huggers round each wall.
uint32_t wallhug_continue_trivial(wallhug_path* path, wallhug_waypoint start, uint32_t limit);   // 0x10136f04 (3500)
int32_t wallhug_trivial(wallhug_path* path, NavMode mode);                                       // 0x10137d04 (96)
extern int32_t gWallhugRecordHits;   // 0x1034caa8  read only: nonzero makes the planner record every wall hit, the last one stays (nothing in the binary sets it)
extern uint32_t gWallhugHitStep;     // 0x1034caac  the step counter at the latest hit
extern uint8_t gWallhugHitDir;       // 0x1034cab0  the direction that hit (the latest)
extern uint8_t gWallhugHitSide;      // 0x1034cab1  0xff when the first hugger's squares were spliced in, 1 when the second's (the latest splice)
extern uint8_t gWallhugHitCount;     // 0x1034cab2  mCount at the latest hit (wallhug_trivial stores 0xff when recording)
extern uint32_t gWallhugSteps;       // 0x1034cab4  the step counter: wallhug_trivial clears it, every pass of either loop adds 1
// 0x10102394 (220 bytes, src/MapDelta_map.cpp): the length of the planned walk in squares (-1 when there is none).
int32_t MAP_count_walking_distance(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> from, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> to, NavMode mode);
class CObject;
// 0x1003277c (140 bytes, untranslated): the entrance object standing at the place, or NULL.
CObject* GetThingAtPosnIfEntrance(float x, float z);

// ---- Unit w141 (src/TPCamera_modes.cpp): the camera modes, the first person ride and walk, camera save and load.
struct tag_sRideAnimSet;
// 0x1003384c (792 bytes): switches the first person flags (mode 2 walk around, 4 ride, 0x40 leaves); toggle flips, force overrides a
// blocked state, out (may be NULL) is set to 1 when a ride that was left returns to walking; resetTurn zeroes the look angles on entering.
// Answers 1 when the mode is on afterwards (modes 2 and 0x40: when it was asked for).
int32_t SetCameraMode(int32_t mode, int32_t toggle, int32_t ride, int32_t force, int32_t* out, int32_t resetTurn);
int32_t LoadCamera(void* file);                                        // 0x10036218 (732 bytes): 1 when all six items were read
int32_t SaveCamera(void* file);                                        // 0x10035f4c (680 bytes): 1 when all six items were written
void Setup1stPersonRideMeshes(int32_t ride);                            // 0x10032c64 (428 bytes)
void ToggleRideRideIn1stPerson(int32_t ride, int32_t a, int32_t b, int32_t* out, int32_t node, int32_t c);   // 0x10032ff4 (408 bytes)
int32_t SetupRideRideMatrixAndSuch(int32_t ride, int32_t node);        // 0x10032e40 (384 bytes)
// 0x10032a44 (untranslated, TPCamera.cpp): whether the object may be ridden, given its anim set index (MAP_wall_in_way's entrance walk).
int32_t IsRideRideable(void* object, uint32_t animSet);
// Names invented, addresses original.
extern tag_sRideAnimSet* gCameraHeadsRideSet;   // 0x102b5a38  BSS 0: the set whose heads SetCameraMode draws on entering
extern uint32_t* gFirstPersonRideFlagWord;      // 0x102b5a30  the flag word of the embedded record of the ridden car (bit 4 set while ridden)
extern void* gFirstPersonRideNode;              // 0x102b5a34  the data node of the ridden car
extern float gCameraSavedPosition[3];           // 0x102b5bcc  the walking camera's position kept while in first person
extern float gCameraSavedYaw;                   // 0x102b5bec
extern int8_t gRideRideOnceGuard;               // 0x102b5a20  set once by ToggleRideRideIn1stPerson
extern int8_t gRideRideOnceValue;               // 0x102b5a21  cleared the first time
// Unit v5764 (src/lobby_exit_IsRideRideable.cpp): StopRidingRideIn1stPerson, GetThingAtPosnIfEntrance(float, float), IsRideRideable.
CObject* GetThingAtPosnIfEntrance(int32_t gridX, int32_t gridZ);   // 0x10032618 (308 bytes, untranslated): the grid square's entrance object
void MoveCameraOverRideExit();                                      // 0x10032838 (356 bytes, untranslated)
// Unit v51013 (src/lobby_exit_IsRideRideable_GetThingAtPosnIfEntrance.cpp): the halfword that ends the thing list GetThingAtPosnIfEntrance walks.
extern uint16_t gThingListEnd;   // 0x1029cc8c (name invented)
