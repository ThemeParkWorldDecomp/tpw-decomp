// ActionRecorder.cpp (main app): CActionRecorder. PARTIAL: ResetActionRecording and DoTick (unit w133, src/ActionRecorder.cpp);
// ReplayRecording and the rest are untranslated.
#pragma once
#include <cstdint>

enum ReplayType : int32_t {};
enum RecordedAction : int32_t {};   // unit v5075: the first argument of RecordAction 0x1014430c

class CActionRecorder {
public:
    void ReplayRecording(ReplayType type);   // 0x10144e2c
    // 0x10143e08 (260 bytes, src/ActionRecorder.cpp; CWorld::SetupNewLevel calls it): closes the action file (and, in mode 0, the
    // timing and camera files), opens them again (0x200 bytes each) and records the action 0xe7 with a zeroed 128-byte buffer.
    void ResetActionRecording();
    // 0x10144f80 (872 bytes, src/ActionRecorder.cpp; CWorld::DoGameTurn calls it): while a replay runs, plays the recorded actions.
    void DoTick();

    // Untranslated callees of DoTick and ResetActionRecording (stand-ins in the launcher).
    void EndReplay();                                       // 0x10144ce4
    void GetNextTimingFileEntry();                          // 0x10145318
    void GetNextCameraFileEntry();                          // 0x10145360
    int32_t ReadAndProcessNextAction();                     // 0x101453a8  0 when the recording has ended
    void RecordAction(int32_t action, uint8_t* data);       // 0x10144bec
    void RecordAction(int32_t action, uint16_t value);      // 0x101441fc (untranslated; CoasterTickPreRender records 0x43 and the coaster id)
    void RecordAction(int32_t action, uint16_t value, uint8_t extra);   // 0x101445d0 (236 bytes, untranslated; unit w185)
    void RecordAction(int32_t action, uint16_t a, uint16_t b, uint16_t c);   // 0x1014445c (296 bytes, untranslated; unit v5026)
    void RecordAction(int32_t action, float value, uint16_t id);   // 0x10144704 (284 bytes, untranslated; unit v5054)

    // The Mac's layout is noted per member; the host's three files (bfl::FileSystem::CMemFile, 8 bytes on the Mac at +0x0c, +0x14,
    // +0x1c) are storage here, made in place on first use (the Mac builds them at start-up), because CMemFile has a vtable and a native
    // pointer. The host's object is therefore larger than the Mac's 0x34 bytes; only word 0x24 is read elsewhere (CreateRideScript).
    int32_t mState;                          // 0x00  ResetActionRecording clears it, then sets 1
    int32_t mMode;                           // 0x04  0: the timing and camera files are in use
    int32_t mF08;                            // 0x08
    alignas(8) uint8_t mActionFile[16];      // 0x0c  CMemFile
    alignas(8) uint8_t mTimingFile[16];      // 0x14  CMemFile
    alignas(8) uint8_t mCameraFile[16];      // 0x1c  CMemFile
    int32_t mRecording;                      // 0x24  nonzero while a recording runs or is replayed (DoTick does nothing when 0)
    ReplayType mReplayType;                  // 0x28  0: all at once; 1 and 2: timed (1 caps the wait at 2 turns after an action)
    int32_t mF2C;                            // 0x2c
    int32_t mTurnsToNext;                    // 0x30  game turns until the next recorded action
    uint32_t mActionKind[256];               // 0x34  per action byte: how its operands are read (0 to 11; larger traces an error)

    // The twelve overloads ReadAndProcessNextAction calls (stand-ins in the launcher); the action is passed as a word.
    void ProcessAction(int32_t action);                                                       // 0x10145b28
    void ProcessAction(int32_t action, uint8_t a, uint8_t b, uint16_t c);                     // 0x10145d34
    void ProcessAction(int32_t action, uint8_t a, uint8_t b);                                 // 0x10145c24
    void ProcessAction(int32_t action, uint16_t a);                                           // 0x10145e38
    void ProcessAction(int32_t action, uint16_t a, uint8_t b);                                // 0x10146058
    void ProcessAction(int32_t action, float a, uint16_t b);                                  // 0x10146140
    void ProcessAction(int32_t action, float a, float b, uint16_t c);                         // 0x10146248
    void ProcessAction(int32_t action, uint16_t a, uint16_t b);                               // 0x1014634c
    void ProcessAction(int32_t action, uint16_t a, uint16_t b, uint16_t c);                   // 0x1014644c
    void ProcessAction(int32_t action, uint8_t count, char16_t* text);                        // 0x10146524
    void ProcessAction(int32_t action, uint16_t a, uint8_t count, char16_t* text);            // 0x101465a8
    void ProcessAction(int32_t action, uint8_t* data);                                        // 0x10146850

    // Unit v5075 (src/ActionRecorder_CActionRecorder_CActionRecorder.cpp).
    enum RecordMode : int32_t {};
    // Not a C++ constructor / destructor: other units define gActionRecorder and would then need them.
    void Construct();                        // 0x10143658: builds the three files, the mode table 0x34..0x3d3 and starts recording mode 0
    void Destruct();                         // 0x10143880: saves the recording (mode 0), closes and destroys the files
    int32_t Serialise(void* file, int32_t saving, int32_t version, uint32_t unused);   // 0x101439dc: 1 on failure
    void RecordAction(RecordedAction action, uint16_t a, uint16_t b);   // 0x1014430c
    void ActivateActionRecording(RecordMode mode);   // 0x10143cf0 (stand-in)
    void SaveRecordingToDisk();                      // 0x10143cac (stand-in)
    void RecordAction(RecordedAction action, uint8_t a, uint8_t b);                  // 0x10143ff8 (160 bytes)
    void RecordAction(RecordedAction action, uint8_t a, uint8_t b, uint16_t c);      // 0x101440e0 (208 bytes)
    void RecordAction(RecordedAction action, float a, float b, uint16_t id);         // 0x10144868 (336 bytes; unit v5144 declares it)

    // Unit v5224 (src/ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ReplayRecording.cpp).
    void RecordAction(RecordedAction action, float value, uint16_t id);   // 0x10144704 (284 bytes), label ...F14RecordedActionfUs
    // 0x10144a00 (416 bytes), label ...F14RecordedActionUsUcPw: records the action, a (swapped), b and the swear-filtered text (UTF-16, swapped).
    void RecordAction(RecordedAction action, uint16_t a, uint8_t b, const char16_t* text);
    // Unit v5337: the action alone (108 bytes), label RecordAction__15CActionRecorderF14RecordedAction, defined in the v5075 file.
    void RecordAction(RecordedAction action);   // 0x10143f48

    // Unit v51041 (src/ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ProcessAction_CActionRecorder_ActivateActionRecording.cpp).
    void RecordAction(RecordedAction action, uint16_t value);                // 0x101441fc, label ...F14RecordedActionUs
    void RecordAction(RecordedAction action, uint16_t value, uint8_t extra); // 0x101445d0, label ...F14RecordedActionUsUc
    void RecordAction(RecordedAction action, uint8_t* data);                 // 0x10144bec, label ...F14RecordedActionPUc
    void ProcessAction(RecordedAction action, uint8_t* data);                // 0x10146850, label ...F14RecordedActionPUc
};
extern CActionRecorder gActionRecorder;   // 0x1037ec44, original name unknown
// Unit v51048 (src/Game_level_CGame_GetLocalisedThemeName_3.cpp): __sinit_ActionRecorder_cpp 0x10146e9c (88 bytes): a wide string (the text at
// 0x10248615) built in gActionRecorderString with its exit record, then the recorder constructed with its exit record. Storage is raw
// (the Mac's string is 0x14 bytes); names invented.
void __sinit_ActionRecorder_cpp();
extern uint8_t gActionRecorderString[];       // 0x1037f024
extern uint8_t gActionRecorderStringNode[];   // 0x1037f038
extern uint8_t gActionRecorderNode[];         // 0x1037f018
extern uint8_t gActionRecorderDtor[];         // stands for the destructor word 0x10266e58

// Unit v5362 (src/ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ProcessAction_CActionRecorder_ProcessAction_2.cpp).
// Two halfwords of initialised data that end the chain of things the replay follows from a map cell (names invented; both 0 at start).
extern uint16_t gReplayChainEndA;   // 0x102a4bd4 (ProcessAction 0x10145e38)
extern uint16_t gReplayChainEndB;   // 0x102a4cec (ProcessAction 0x101465a8)
// Layout functions with an id argument (the listing's overloads; the bodies live in src/TPConfig_LayoutSetMovePylon.cpp).
void LayoutStackPylonUp(uint16_t id);        // 0x10058c08
void LayoutStackPylonDown(uint16_t id);      // 0x10058c80
void LayoutSetCoasterEditOld(uint32_t id);   // 0x10058fa0
void LayoutSetCoasterEdit(uint32_t id);      // 0x1005901c
void LayoutRegenerateTrack(uint32_t id);     // 0x10058ec0

// Unit v51072 (src/ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ProcessAction_3.cpp). The Mac's destructors and constructor
// below take their object by address (and the hidden short "delete storage" flag), so they are free functions on the host.
void WideStringDestroy(void* string, int16_t flag);       // 0x101468f8 std::wstring::~basic_string (the string is one Ptr32 to its shared representation)
void CMemFileDestroy(void* file, int16_t flag);           // 0x101437e0 bfl::FileSystem::CMemFile::~CMemFile
void ThingIdListDestroy(void* list, int16_t flag);        // 0x10141e34 TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, ...>::~TbTList
void ThingArrayEntryInit(uint32_t* entry);                // 0x10142008 CWorld::_CThingArrayEntry::_CThingArrayEntry: five words cleared
void __sinit_InGame_cpp();                                // 0x1014b5cc
void __sinit_ExitErr_cpp();                               // 0x1014bf38
extern uint8_t gInGameFontNode[];                         // 0x1037f1b4 (exit record of gInGameFont, name invented)
extern uint8_t gInGameFontDtor[];                         // stands for the destructor word 0x102673f0
// Callees the host cannot call as the Mac does: weak definitions in the unit's source, recording fakes in the test.
namespace v51072_callee {
void OperatorDelete(void* block);                         // 0x10238218
void ThingIdListRemoveAll(void* list);                    // 0x10141f1c
}
