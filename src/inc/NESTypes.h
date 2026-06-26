/*
 * vNES for Symbian C++
 * Порт оригинального vNES (Java) на нативный Symbian C++
 * Оригинал: Copyright (C) 2006-2013 Open Emulation Project (GPL v3)
 *
 * NESTypes.h — базовые типы и глобальные константы
 */

#ifndef NESTYPES_H
#define NESTYPES_H

#include <e32base.h>
#include <e32std.h>

// ---------------------------------------------------------------------------
// Базовые типы эмулятора
// ---------------------------------------------------------------------------
typedef TUint8  u8;
typedef TUint16 u16;
typedef TUint32 u32;
typedef TInt8   s8;
typedef TInt16  s16;
typedef TInt32  s32;

// ---------------------------------------------------------------------------
// Глобальные настройки эмулятора (аналог Globals.java)
// ---------------------------------------------------------------------------
namespace Globals
{
    const TReal64 KCpuFreqNTSC = 1789772.5;
    const TReal64 KCpuFreqPAL  = 1773447.4;

    extern TInt   preferredFrameRate;   // default 60
    extern TInt   frameTime;            // microseconds per frame
    extern u8     memoryFlushValue;     // default 0xFF

    extern TBool  appletMode;
    extern TBool  disableSprites;
    extern TBool  timeEmulation;
    extern TBool  palEmulation;
    extern TBool  enableSound;
    extern TBool  focused;
}

// ---------------------------------------------------------------------------
// Типы прерываний CPU (аналог констант в CPU.java)
// ---------------------------------------------------------------------------
enum TIrqType
{
    EIrqNormal = 0,
    EIrqNMI    = 1,
    EIrqReset  = 2
};

// ---------------------------------------------------------------------------
// Типы зеркалирования ROM (аналог констант в ROM.java)
// ---------------------------------------------------------------------------
enum TMirroringType
{
    EVerticalMirroring     = 0,
    EHorizontalMirroring   = 1,
    EFourScreenMirroring   = 2,
    ESingleScreen1         = 3,
    ESingleScreen2         = 4,
    ESingleScreen3         = 5,
    ESingleScreen4         = 6,
    ECHRROMMirroring       = 7
};

// ---------------------------------------------------------------------------
// Размеры памяти NES
// ---------------------------------------------------------------------------
const TInt KCPU_MEM_SIZE  = 0x10000;  // 64 KB
const TInt KPPU_MEM_SIZE  = 0x8000;   // 32 KB
const TInt KSPR_MEM_SIZE  = 0x100;    // 256 bytes
const TInt KSRAM_SIZE     = 0x2000;   // 8 KB Save RAM

// ---------------------------------------------------------------------------
// Экран NES
// ---------------------------------------------------------------------------
const TInt KNES_WIDTH  = 256;
const TInt KNES_HEIGHT = 240;

#endif // NESTYPES_H
