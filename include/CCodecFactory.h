// CCodecFactory.h: the sound library's codec factory and the bank file wrapper (the Mac's "sound symbols shared" library, Code at
// 0x10000000, TOC base r2 = 0x1003abf0), unit w80b (LbMpeg.cpp; the constructor and destructor are in TbSoundSampleWAV.cpp's range).
// This header REPLACES the placeholder of unit w79c (the sample store makes a CCodecFactory on its stack for CreateNewSample and
// CreateNewControl and calls CreateCodec and CreateController).
//
// CCodecFactory is 4 bytes on the Mac: a CAutoCriticalSection at +0 (the nesting word the codec pools are made under). It has no
// virtuals. Every function that returns a TbError does so through the hidden r3 buffer on the Mac, so its arguments start at r4.
//
// _CFileAccess (24 bytes: the file wrapper the MPEG file player reads the bank through; unit w80c's player holds it) keeps the
// position relative to the start of the sample's data. +0 vptr (0x10035c94, slots +8 destructor, +0xc Read, +0x10 Seek), +4
// mPosition, +8 mStart, +0xc mFile (a CFile by value, copy constructed from *file by BOTH constructors: the import 0x1002ed7c is
// CFile(const CFile&)), +0x14 mSemaphore (taken by Read).
#pragma once
#include <cstdint>

#include "LbSemaphore.h"
#include "TbError.h"
#include "TbSoundInfo.h"
#include "TbSoundPipe.h"
#include "bfl_FileSystem.h"
#include "snd_handles.h"

class TbSoundPipeChannel;

class CCodecFactory {
public:
    CCodecFactory();                                                  // 0x10010b10 (48)
    ~CCodecFactory();                                                 // 0x10010a88 (92): the deleting form frees 4 bytes

    // Builds the pipe chain for a sample: the codec (WAV or MPEG by the header, in memory or streamed by the bank), the loop and
    // callback modifiers, the handle and the info. Answers NULL (after releasing what it made and the channel's pipe) when any
    // step fails.
    TbSoundSamplePipe* CreateCodec(TbSoundPipeChannel* channel, TbSoundSampleInfo& info);   // 0x100007c4 (872)
    // The completion callback pipe when the info asks for one (flag 0x2000, which it clears), else NULL.
    TbSoundSamplePipe* CreateController(TbSoundSampleInfo& info);     // 0x10000054 (116)
    // 1 when the info names a bank and a sample and the bank gave the sample's header (into *header), else 0.
    int32_t GetNextSampleFromBank(TbSampleHeader** header, TbSoundSampleInfo& info);   // 0x10000390 (72)
    // A loop codec in front of `pipe` when the info loops (flag 0x40) more than once.
    TbSoundSamplePipe* CreateCodecModifier(TbSoundSampleInfo& info, TbSampleHeader* header, TbSoundSamplePipe* pipe);   // 0x10000438 (156)

    CAutoCriticalSection mLock;   // +0
};

class _CFileAccess {
public:
    _CFileAccess(bfl::FileSystem::CFile* file);                       // 0x10001cf0 (92): start 0, a copy of the file (not tested for NULL)
    _CFileAccess(bfl::FileSystem::CFile* file, uint32_t start);       // 0x10001d90 (92): the start, a copy of the file
    virtual ~_CFileAccess();                                          // +0x08, 0x100020a8 (120): deleting form frees 0x18 bytes
    // Seeks the file to the position first when it is elsewhere, reads, and answers the NEW POSITION (not the count; -1 reads
    // move it back).
    virtual int32_t Read(void* buffer, uint32_t count);               // +0x0c, 0x10001e30 (220)
    virtual void Seek(int32_t offset);                                // +0x10, 0x10001fb0 (84): the position is what the file answers - start
    void SeekToDataStart();                                           // 0x10002030 (68)

    int32_t mPosition;                       // +4
    uint32_t mStart;                         // +8
    bfl::FileSystem::CFile mFile;            // +0xc
    _LbSemaphore mSemaphore;                 // +0x14
};
