// UIInterface_context.cpp (main, unit w131): UIInterface::CheckMouseContext 0x101efb0c (40 bytes).
#include "UIInterface.h"
#include "ui_main.h"

// Hands the mouse context to ContextMouse (0x10182548); the answer of the Mac is nothing.
void UIInterface::CheckMouseContext(int context) { ContextMouse(context); }
