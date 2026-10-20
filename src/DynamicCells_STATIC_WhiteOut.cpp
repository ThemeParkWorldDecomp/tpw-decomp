#include "DynamicMesh.h"
#include "Clock.h"
#include "Debug.h"
#include "TPCamera.h"
#include "World.h"
#include "analyser.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "platform/opengl.h"

int32_t Rand();   // 0x10239148 rand

// 0x102a03a4: four RGBA colours of doubles; the original's initial data is not known here (zero).
double gWhiteOutColors[16];

// 0x100e28b4 (368 bytes): fills the screen with 4-quad-sized random tiles (quads of 0.00625 x 0.008333 over [-1, 1]), each tile
// coloured by one of the four colours picked with rand() & 3. The colour table is copied to the stack first.
void STATIC_WhiteOut() {
    double colors[16];
    for (int i = 0; i < 16; ++i) colors[i] = gWhiteOutColors[i];
    glPushAttrib(0x2000);
    glDisable(0xb71);
    glDisable(0xde1);
    glDisable(0xbc0);
    glDisable(0xbe2);
    glDisable(0xb60);
    glBegin(7);
    double x = -1.0;
    while (x <= 1.0) {
        double y = -1.0;
        while (y <= 1.0) {
            const uint32_t r = static_cast<uint32_t>(Rand());
            glColor4dv(&colors[(r & 3) * 4]);
            const double x2 = x + 0.00625;
            const double y2 = y + 0.008333333333333333;
            glVertex2d(x, y);
            glVertex2d(x2, y);
            glVertex2d(x2, y2);
            glVertex2d(x, y2);
            y = y2;
        }
        x = x + 0.00625;
    }
    glEnd();
    glPopAttrib();
}
