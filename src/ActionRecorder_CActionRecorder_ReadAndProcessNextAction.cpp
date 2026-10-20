// CActionRecorder::ReadAndProcessNextAction (main app 0x101453a8, 1856 bytes; unit v5077).
#include <cstdint>

#include "ActionRecorder.h"
#include "Debug.h"
#include "bfl_FileSystem.h"
#include "platform/samsutils.h"

// Reads the next recorded action from the action file: one byte (the action), then the operands its kind (mActionKind[action]) needs.
// The file holds the operands little-endian (the Mac swaps them); the host reads them straight into native values. Any short read
// answers 0 (the recording has ended); otherwise the matching ProcessAction runs and the answer is 1. A kind above 11 traces an error
// and answers 1. The original reads the wide text of kinds 9 and 10 into a stack buffer that a count above 40 (kind 9) or 72 (kind 10)
// overruns; the host buffers hold the 255 characters.
int32_t CActionRecorder::ReadAndProcessNextAction() {
    using bfl::FileSystem::CFile;
    CFile* file = reinterpret_cast<CFile*>(mActionFile);
    auto read = [file](void* dest, uint32_t size) { return static_cast<uint32_t>(file->Read(dest, size)) == size; };

    uint8_t action;
    if (!read(&action, 1)) return 0;
    const int32_t id = action;

    switch (mActionKind[action]) {
        case 0: {   // 0x101454e0
            uint8_t b, a;
            if (!read(&b, 1)) return 0;
            if (!read(&a, 1)) return 0;
            ProcessAction(id, b, a);
            break;
        }
        case 1: {   // 0x10145554
            uint16_t a;
            if (!read(&a, 2)) return 0;
            ProcessAction(id, a);
            break;
        }
        case 2: {   // 0x1014542c
            uint8_t b, a;
            uint16_t c;
            if (!read(&b, 1)) return 0;
            if (!read(&a, 1)) return 0;
            if (!read(&c, 2)) return 0;
            ProcessAction(id, b, a, c);
            break;
        }
        case 3: {   // 0x101455a8
            uint16_t a;
            uint8_t b;
            if (!read(&a, 2)) return 0;
            if (!read(&b, 1)) return 0;
            ProcessAction(id, a, b);
            break;
        }
        case 4: {   // 0x1014562c
            float f;
            uint16_t a;
            if (!read(&f, 4)) return 0;
            if (!read(&a, 2)) return 0;
            ProcessAction(id, f, a);
            break;
        }
        case 5: {   // 0x101456c8
            float f, g;
            uint16_t a;
            if (!read(&f, 4)) return 0;
            if (!read(&g, 4)) return 0;
            if (!read(&a, 2)) return 0;
            ProcessAction(id, f, g, a);
            break;
        }
        case 6: {   // 0x101457ac
            uint16_t a, b;
            if (!read(&a, 2)) return 0;
            if (!read(&b, 2)) return 0;
            ProcessAction(id, a, b);
            break;
        }
        case 7: {   // 0x10145840
            uint16_t a, b, c;
            if (!read(&a, 2)) return 0;
            if (!read(&b, 2)) return 0;
            if (!read(&c, 2)) return 0;
            ProcessAction(id, a, b, c);
            break;
        }
        case 8:     // 0x10145418
            ProcessAction(id);
            break;
        case 9: {   // 0x10145914
            uint8_t count;
            char16_t text[256];
            if (!read(&count, 1)) return 0;
            if (!read(text, static_cast<uint32_t>(count) * 2)) return 0;
            SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(text), count);
            ProcessAction(id, count, text);
            break;
        }
        case 10: {  // 0x101459a4
            uint16_t a;
            uint8_t count;
            char16_t text[256];
            if (!read(&a, 2)) return 0;
            if (!read(&count, 1)) return 0;
            if (!read(text, static_cast<uint32_t>(count) * 2)) return 0;
            SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(text), count);
            ProcessAction(id, a, count, text);
            break;
        }
        case 11: {  // 0x10145a74
            uint8_t data[128];
            if (!read(data, 0x80)) return 0;
            ProcessAction(id, data);
            break;
        }
        default:
            CTRACE(2, "Erroneous stuff going down\n");
            break;
    }
    return 1;
}
