/*
 * Globals.cpp
 */
#include "NESTypes.h"

namespace Globals
{
    TInt  preferredFrameRate = 60;
    TInt  frameTime          = 1000000 / 60;
    u8    memoryFlushValue   = 0xFF;

    TBool appletMode    = EFalse;
    TBool disableSprites= EFalse;
    TBool timeEmulation = ETrue;
    TBool palEmulation  = EFalse;
    TBool enableSound   = ETrue;
    TBool focused       = ETrue;
}
