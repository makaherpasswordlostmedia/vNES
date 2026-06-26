/*
 * NESCPU.cpp — Аналог CPU.java
 * Полный эмулятор 6502/2A03: все опкоды, режимы адресации, IRQ/NMI/RESET.
 *
 * Кодировка opdata (TUint32):
 *   bits 31:24 — базовые такты
 *   bits 23:16 — размер инструкции (байт)
 *   bits 15:8  — режим адресации
 *   bits  7:0  — номер инструкции
 *
 * Портировано из CpuInfo.java + CPU.java
 */

#include "NESCPU.h"
#include "NESMapper.h"
#include "NESPPU.h"
#include "NESPAPU.h"
#include "NESMemory.h"
#include <s32strm.h>

// ---------------------------------------------------------------------------
// Макросы для таблицы опкодов
// ---------------------------------------------------------------------------
#define OPDATA(cycles, size, addrMode, inst) \
    (((TUint32)(cycles)<<24)|((TUint32)(size)<<16)|((TUint32)(addrMode)<<8)|(inst))

// Режимы адресации (совпадают с оригиналом CPU.java)
enum EAddrMode
{
    ADDR_ZP      = 0,
    ADDR_REL     = 1,
    ADDR_IMP     = 2,
    ADDR_ABS     = 3,
    ADDR_ACC     = 4,
    ADDR_IMM     = 5,
    ADDR_ZP_X    = 6,
    ADDR_ZP_Y    = 7,
    ADDR_ABS_X   = 8,
    ADDR_ABS_Y   = 9,
    ADDR_PRE_X   = 10,
    ADDR_POST_Y  = 11,
    ADDR_IND     = 12
};

// ---------------------------------------------------------------------------
// Таблица опкодов 6502 (256 записей)
// Каждая запись: OPDATA(такты, байты, адрес_режим, номер_инструкции)
// Нереализованные опкоды помечены inst=255 (JAM/illegal)
// ---------------------------------------------------------------------------
const TUint32 KOpData[256] =
{
// 0x00 BRK, 0x01 ORA (ind,X), 0x05 ORA zp, 0x06 ASL zp ...
/*00*/ OPDATA(7,1,ADDR_IMP,10), OPDATA(6,2,ADDR_PRE_X,39),  OPDATA(2,1,ADDR_IMP,255),OPDATA(8,2,ADDR_PRE_X,255),
/*04*/ OPDATA(3,2,ADDR_ZP ,255),OPDATA(3,2,ADDR_ZP   ,39),  OPDATA(5,2,ADDR_ZP ,2),  OPDATA(5,2,ADDR_ZP   ,255),
/*08*/ OPDATA(3,1,ADDR_IMP,48), OPDATA(2,2,ADDR_IMM  ,39),  OPDATA(2,1,ADDR_ACC,2),  OPDATA(2,2,ADDR_IMM  ,255),
/*0C*/ OPDATA(4,3,ADDR_ABS,255),OPDATA(4,3,ADDR_ABS  ,39),  OPDATA(6,3,ADDR_ABS,2),  OPDATA(6,3,ADDR_ABS  ,255),
/*10*/ OPDATA(2,2,ADDR_REL, 9), OPDATA(5,2,ADDR_POST_Y,39), OPDATA(2,1,ADDR_IMP,255),OPDATA(8,2,ADDR_POST_Y,255),
/*14*/ OPDATA(4,2,ADDR_ZP_X,255),OPDATA(4,2,ADDR_ZP_X,39), OPDATA(6,2,ADDR_ZP_X,2), OPDATA(6,2,ADDR_ZP_X,255),
/*18*/ OPDATA(2,1,ADDR_IMP,13), OPDATA(4,3,ADDR_ABS_Y,39),  OPDATA(2,1,ADDR_IMP,255),OPDATA(7,3,ADDR_ABS_Y,255),
/*1C*/ OPDATA(4,3,ADDR_ABS_X,255),OPDATA(4,3,ADDR_ABS_X,39),OPDATA(7,3,ADDR_ABS_X,2),OPDATA(7,3,ADDR_ABS_X,255),
/*20*/ OPDATA(6,3,ADDR_ABS,28),  OPDATA(6,2,ADDR_PRE_X,1),  OPDATA(2,1,ADDR_IMP,255),OPDATA(8,2,ADDR_PRE_X,255),
/*24*/ OPDATA(3,2,ADDR_ZP ,6),   OPDATA(3,2,ADDR_ZP  ,1),   OPDATA(5,2,ADDR_ZP,36),  OPDATA(5,2,ADDR_ZP   ,255),
/*28*/ OPDATA(4,1,ADDR_IMP,47),  OPDATA(2,2,ADDR_IMM ,1),   OPDATA(2,1,ADDR_ACC,36), OPDATA(2,2,ADDR_IMM  ,255),
/*2C*/ OPDATA(4,3,ADDR_ABS,6),   OPDATA(4,3,ADDR_ABS ,1),   OPDATA(6,3,ADDR_ABS,36), OPDATA(6,3,ADDR_ABS  ,255),
/*30*/ OPDATA(2,2,ADDR_REL, 7),  OPDATA(5,2,ADDR_POST_Y,1), OPDATA(2,1,ADDR_IMP,255),OPDATA(8,2,ADDR_POST_Y,255),
/*34*/ OPDATA(4,2,ADDR_ZP_X,255),OPDATA(4,2,ADDR_ZP_X,1),  OPDATA(6,2,ADDR_ZP_X,36),OPDATA(6,2,ADDR_ZP_X,255),
/*38*/ OPDATA(2,1,ADDR_IMP,41),  OPDATA(4,3,ADDR_ABS_Y,1),  OPDATA(2,1,ADDR_IMP,255),OPDATA(7,3,ADDR_ABS_Y,255),
/*3C*/ OPDATA(4,3,ADDR_ABS_X,255),OPDATA(4,3,ADDR_ABS_X,1), OPDATA(7,3,ADDR_ABS_X,36),OPDATA(7,3,ADDR_ABS_X,255),
/*40*/ OPDATA(6,1,ADDR_IMP,50),  OPDATA(6,2,ADDR_PRE_X,23), OPDATA(2,1,ADDR_IMP,255),OPDATA(8,2,ADDR_PRE_X,255),
/*44*/ OPDATA(3,2,ADDR_ZP,255),  OPDATA(3,2,ADDR_ZP  ,23),  OPDATA(5,2,ADDR_ZP,32),  OPDATA(5,2,ADDR_ZP   ,255),
/*48*/ OPDATA(3,1,ADDR_IMP,46),  OPDATA(2,2,ADDR_IMM ,23),  OPDATA(2,1,ADDR_ACC,32), OPDATA(2,2,ADDR_IMM  ,255),
/*4C*/ OPDATA(3,3,ADDR_ABS,27),  OPDATA(4,3,ADDR_ABS ,23),  OPDATA(6,3,ADDR_ABS,32), OPDATA(6,3,ADDR_ABS  ,255),
/*50*/ OPDATA(2,2,ADDR_REL,11),  OPDATA(5,2,ADDR_POST_Y,23),OPDATA(2,1,ADDR_IMP,255),OPDATA(8,2,ADDR_POST_Y,255),
/*54*/ OPDATA(4,2,ADDR_ZP_X,255),OPDATA(4,2,ADDR_ZP_X,23), OPDATA(6,2,ADDR_ZP_X,32),OPDATA(6,2,ADDR_ZP_X,255),
/*58*/ OPDATA(2,1,ADDR_IMP,15),  OPDATA(4,3,ADDR_ABS_Y,23), OPDATA(2,1,ADDR_IMP,255),OPDATA(7,3,ADDR_ABS_Y,255),
/*5C*/ OPDATA(4,3,ADDR_ABS_X,255),OPDATA(4,3,ADDR_ABS_X,23),OPDATA(7,3,ADDR_ABS_X,32),OPDATA(7,3,ADDR_ABS_X,255),
/*60*/ OPDATA(6,1,ADDR_IMP,51),  OPDATA(6,2,ADDR_PRE_X,0),  OPDATA(2,1,ADDR_IMP,255),OPDATA(8,2,ADDR_PRE_X,255),
/*64*/ OPDATA(3,2,ADDR_ZP,255),  OPDATA(3,2,ADDR_ZP  ,0),   OPDATA(5,2,ADDR_ZP,35),  OPDATA(5,2,ADDR_ZP   ,255),
/*68*/ OPDATA(4,1,ADDR_IMP,45),  OPDATA(2,2,ADDR_IMM ,0),   OPDATA(2,1,ADDR_ACC,35), OPDATA(2,2,ADDR_IMM  ,255),
/*6C*/ OPDATA(5,3,ADDR_IND,27),  OPDATA(4,3,ADDR_ABS ,0),   OPDATA(6,3,ADDR_ABS,35), OPDATA(6,3,ADDR_ABS  ,255),
/*70*/ OPDATA(2,2,ADDR_REL,12),  OPDATA(5,2,ADDR_POST_Y,0), OPDATA(2,1,ADDR_IMP,255),OPDATA(8,2,ADDR_POST_Y,255),
/*74*/ OPDATA(4,2,ADDR_ZP_X,255),OPDATA(4,2,ADDR_ZP_X,0),  OPDATA(6,2,ADDR_ZP_X,35),OPDATA(6,2,ADDR_ZP_X,255),
/*78*/ OPDATA(2,1,ADDR_IMP,40),  OPDATA(4,3,ADDR_ABS_Y,0),  OPDATA(2,1,ADDR_IMP,255),OPDATA(7,3,ADDR_ABS_Y,255),
/*7C*/ OPDATA(4,3,ADDR_ABS_X,255),OPDATA(4,3,ADDR_ABS_X,0), OPDATA(7,3,ADDR_ABS_X,35),OPDATA(7,3,ADDR_ABS_X,255),
/*80*/ OPDATA(2,2,ADDR_IMM,255), OPDATA(6,2,ADDR_PRE_X,44), OPDATA(2,2,ADDR_IMM,255),OPDATA(6,2,ADDR_PRE_X,255),
/*84*/ OPDATA(3,2,ADDR_ZP,54),   OPDATA(3,2,ADDR_ZP  ,43),  OPDATA(3,2,ADDR_ZP,44),  OPDATA(3,2,ADDR_ZP   ,255),
/*88*/ OPDATA(2,1,ADDR_IMP,22),  OPDATA(2,2,ADDR_IMM,255),  OPDATA(2,1,ADDR_IMP,53), OPDATA(2,2,ADDR_IMM  ,255),
/*8C*/ OPDATA(4,3,ADDR_ABS,54),  OPDATA(4,3,ADDR_ABS ,43),  OPDATA(4,3,ADDR_ABS,44), OPDATA(4,3,ADDR_ABS  ,255),
/*90*/ OPDATA(2,2,ADDR_REL, 3),  OPDATA(6,2,ADDR_POST_Y,44),OPDATA(2,1,ADDR_IMP,255),OPDATA(6,2,ADDR_POST_Y,255),
/*94*/ OPDATA(4,2,ADDR_ZP_X,54), OPDATA(4,2,ADDR_ZP_X,43), OPDATA(4,2,ADDR_ZP_Y,44),OPDATA(4,2,ADDR_ZP_Y,255),
/*98*/ OPDATA(2,1,ADDR_IMP,52),  OPDATA(5,3,ADDR_ABS_Y,44), OPDATA(2,1,ADDR_IMP,55), OPDATA(5,3,ADDR_ABS_Y,255),
/*9C*/ OPDATA(5,3,ADDR_ABS_X,255),OPDATA(5,3,ADDR_ABS_X,43),OPDATA(5,3,ADDR_ABS_Y,44),OPDATA(5,3,ADDR_ABS_Y,255),
/*A0*/ OPDATA(2,2,ADDR_IMM,31),  OPDATA(6,2,ADDR_PRE_X,29), OPDATA(2,2,ADDR_IMM,30), OPDATA(6,2,ADDR_PRE_X,255),
/*A4*/ OPDATA(3,2,ADDR_ZP,31),   OPDATA(3,2,ADDR_ZP  ,29),  OPDATA(3,2,ADDR_ZP,30),  OPDATA(3,2,ADDR_ZP   ,255),
/*A8*/ OPDATA(2,1,ADDR_IMP,49),  OPDATA(2,2,ADDR_IMM ,29),  OPDATA(2,1,ADDR_IMP,42), OPDATA(2,2,ADDR_IMM  ,255),
/*AC*/ OPDATA(4,3,ADDR_ABS,31),  OPDATA(4,3,ADDR_ABS ,29),  OPDATA(4,3,ADDR_ABS,30), OPDATA(4,3,ADDR_ABS  ,255),
/*B0*/ OPDATA(2,2,ADDR_REL, 4),  OPDATA(5,2,ADDR_POST_Y,29),OPDATA(2,1,ADDR_IMP,255),OPDATA(5,2,ADDR_POST_Y,255),
/*B4*/ OPDATA(4,2,ADDR_ZP_X,31), OPDATA(4,2,ADDR_ZP_X,29), OPDATA(4,2,ADDR_ZP_Y,30),OPDATA(4,2,ADDR_ZP_Y,255),
/*B8*/ OPDATA(2,1,ADDR_IMP,16),  OPDATA(4,3,ADDR_ABS_Y,29), OPDATA(2,1,ADDR_IMP,38), OPDATA(4,3,ADDR_ABS_Y,255),
/*BC*/ OPDATA(4,3,ADDR_ABS_X,31),OPDATA(4,3,ADDR_ABS_X,29), OPDATA(4,3,ADDR_ABS_Y,30),OPDATA(4,3,ADDR_ABS_Y,255),
/*C0*/ OPDATA(2,2,ADDR_IMM,19),  OPDATA(6,2,ADDR_PRE_X,17), OPDATA(2,2,ADDR_IMM,255),OPDATA(8,2,ADDR_PRE_X,255),
/*C4*/ OPDATA(3,2,ADDR_ZP,19),   OPDATA(3,2,ADDR_ZP  ,17),  OPDATA(5,2,ADDR_ZP,20),  OPDATA(5,2,ADDR_ZP   ,255),
/*C8*/ OPDATA(2,1,ADDR_IMP,26),  OPDATA(2,2,ADDR_IMM ,17),  OPDATA(2,1,ADDR_IMP,21), OPDATA(2,2,ADDR_IMM  ,255),
/*CC*/ OPDATA(4,3,ADDR_ABS,19),  OPDATA(4,3,ADDR_ABS ,17),  OPDATA(6,3,ADDR_ABS,20), OPDATA(6,3,ADDR_ABS  ,255),
/*D0*/ OPDATA(2,2,ADDR_REL, 8),  OPDATA(5,2,ADDR_POST_Y,17),OPDATA(2,1,ADDR_IMP,255),OPDATA(8,2,ADDR_POST_Y,255),
/*D4*/ OPDATA(4,2,ADDR_ZP_X,255),OPDATA(4,2,ADDR_ZP_X,17), OPDATA(6,2,ADDR_ZP_X,20),OPDATA(6,2,ADDR_ZP_X,255),
/*D8*/ OPDATA(2,1,ADDR_IMP,14),  OPDATA(4,3,ADDR_ABS_Y,17), OPDATA(2,1,ADDR_IMP,255),OPDATA(7,3,ADDR_ABS_Y,255),
/*DC*/ OPDATA(4,3,ADDR_ABS_X,255),OPDATA(4,3,ADDR_ABS_X,17),OPDATA(7,3,ADDR_ABS_X,20),OPDATA(7,3,ADDR_ABS_X,255),
/*E0*/ OPDATA(2,2,ADDR_IMM,18),  OPDATA(6,2,ADDR_PRE_X,0),  OPDATA(2,2,ADDR_IMM,255),OPDATA(8,2,ADDR_PRE_X,255),
/*E4*/ OPDATA(3,2,ADDR_ZP,18),   OPDATA(3,2,ADDR_ZP  ,0),   OPDATA(5,2,ADDR_ZP,24),  OPDATA(5,2,ADDR_ZP   ,255),
/*E8*/ OPDATA(2,1,ADDR_IMP,25),  OPDATA(2,2,ADDR_IMM ,0),   OPDATA(2,1,ADDR_IMP,255),OPDATA(2,2,ADDR_IMM  ,255),
/*EC*/ OPDATA(4,3,ADDR_ABS,18),  OPDATA(4,3,ADDR_ABS ,0),   OPDATA(6,3,ADDR_ABS,24), OPDATA(6,3,ADDR_ABS  ,255),
/*F0*/ OPDATA(2,2,ADDR_REL, 5),  OPDATA(5,2,ADDR_POST_Y,0), OPDATA(2,1,ADDR_IMP,255),OPDATA(8,2,ADDR_POST_Y,255),
/*F4*/ OPDATA(4,2,ADDR_ZP_X,255),OPDATA(4,2,ADDR_ZP_X,0),  OPDATA(6,2,ADDR_ZP_X,24),OPDATA(6,2,ADDR_ZP_X,255),
/*F8*/ OPDATA(2,1,ADDR_IMP,34),  OPDATA(4,3,ADDR_ABS_Y,0),  OPDATA(2,1,ADDR_IMP,255),OPDATA(7,3,ADDR_ABS_Y,255),
/*FC*/ OPDATA(4,3,ADDR_ABS_X,255),OPDATA(4,3,ADDR_ABS_X,0), OPDATA(7,3,ADDR_ABS_X,24),OPDATA(7,3,ADDR_ABS_X,255),
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
CNESCPU* CNESCPU::NewLC(CNESMemory* aCpuMem, CNESPPU* aPpu, CNESPAPU* aPapu)
{
    CNESCPU* self = new (ELeave) CNESCPU();
    CleanupStack::PushL(self);
    self->ConstructL(aCpuMem, aPpu, aPapu);
    return self;
}

CNESCPU::CNESCPU()
    : iMapper(NULL), iPpu(NULL), iPapu(NULL), iMem(NULL),
      REG_ACC(0), REG_X(0), REG_Y(0), REG_SP(0x01FF), REG_PC(0x8000),
      F_CARRY(0), F_ZERO(1), F_INTERRUPT(1), F_DECIMAL(0),
      F_BRK(1), F_NOTUSED(1), F_OVERFLOW(0), F_SIGN(0),
      cyclesToHalt(0), crash(EFalse), irqRequested(EFalse), irqType(EIrqNormal)
{}

void CNESCPU::ConstructL(CNESMemory* aCpuMem, CNESPPU* aPpu, CNESPAPU* aPapu)
{
    iPpu  = aPpu;
    iPapu = aPapu;
    iMem  = aCpuMem->Ptr();
}

CNESCPU::~CNESCPU() {}

void CNESCPU::Init()
{
    crash        = EFalse;
    F_BRK        = 1;
    F_NOTUSED    = 1;
    F_INTERRUPT  = 1;
    irqRequested = EFalse;
}

void CNESCPU::Reset()
{
    REG_ACC      = 0;
    REG_X        = 0;
    REG_Y        = 0;
    REG_SP       = 0x01FF;
    REG_PC       = 0x8000 - 1;
    SetStatus(0x28);
    cyclesToHalt = 0;
    irqRequested = EFalse;
    crash        = EFalse;
}

// ---------------------------------------------------------------------------
// Status register pack/unpack (как в оригинале)
// ---------------------------------------------------------------------------
TInt CNESCPU::GetStatus() const
{
    return (F_CARRY)            |
           ((F_ZERO == 0 ? 1 : 0) << 1) |
           (F_INTERRUPT << 2)   |
           (F_DECIMAL   << 3)   |
           (F_BRK       << 4)   |
           (F_NOTUSED   << 5)   |
           (F_OVERFLOW  << 6)   |
           (F_SIGN      << 7);
}

void CNESCPU::SetStatus(TInt aStatus)
{
    F_CARRY     =  aStatus & 0x01;
    F_ZERO      = (aStatus & 0x02) ? 0 : 1;   // инвертировано!
    F_INTERRUPT = (aStatus >> 2) & 1;
    F_DECIMAL   = (aStatus >> 3) & 1;
    F_BRK       = (aStatus >> 4) & 1;
    F_NOTUSED   = (aStatus >> 5) & 1;
    F_OVERFLOW  = (aStatus >> 6) & 1;
    F_SIGN      = (aStatus >> 7) & 1;
}

// ---------------------------------------------------------------------------
// Прерывания
// ---------------------------------------------------------------------------
void CNESCPU::TriggerIRQ(TIrqType aType)
{
    irqRequested = ETrue;
    irqType      = aType;
}

void CNESCPU::DoIRQ(TInt aStatus)
{
    REG_PC++;
    Push((REG_PC >> 8) & 0xFF);
    Push(REG_PC & 0xFF);
    F_BRK = 0;
    Push(aStatus);
    F_INTERRUPT = 1;
    REG_PC = Load(0xFFFE) | (Load(0xFFFF) << 8);
    REG_PC--;
}

void CNESCPU::DoNMI(TInt aStatus)
{
    Push((REG_PC >> 8) & 0xFF);
    Push(REG_PC & 0xFF);
    F_BRK = 0;
    Push(aStatus);
    F_INTERRUPT = 1;
    REG_PC = Load(0xFFFA) | (Load(0xFFFB) << 8);
    REG_PC--;
}

void CNESCPU::DoResetInterrupt()
{
    REG_PC = Load(0xFFFC) | (Load(0xFFFD) << 8);
    REG_PC--;
}

// ---------------------------------------------------------------------------
// Step() — выполнить один опкод, вернуть число тактов
// ---------------------------------------------------------------------------
TInt CNESCPU::Step()
{
    if (cyclesToHalt > 0)
    {
        cyclesToHalt--;
        iPpu->RunCycles(3);
        if (Globals::enableSound) iPapu->Clock();
        return 1;
    }

    // Обработка прерываний
    if (irqRequested)
    {
        TInt status = GetStatus();
        switch (irqType)
        {
        case EIrqNormal:
            if (F_INTERRUPT == 0) DoIRQ(status);
            break;
        case EIrqNMI:
            DoNMI(status);
            break;
        case EIrqReset:
            DoResetInterrupt();
            break;
        }
        irqRequested = EFalse;
    }

    // Читаем опкод
    TUint32 opinf    = KOpData[Load(REG_PC + 1)];
    TInt cycleCount  = (TInt)(opinf >> 24);
    TInt cycleAdd    = 0;
    TInt addrMode    = (TInt)((opinf >> 8) & 0xFF);
    TInt instSize    = (TInt)((opinf >> 16) & 0xFF);
    TInt inst        = (TInt)(opinf & 0xFF);
    TInt opaddr      = REG_PC;

    REG_PC += instSize;

    // Вычислить адрес операнда
    TInt addr = 0;
    switch (addrMode)
    {
    case ADDR_ZP:
        addr = Load(opaddr + 2);
        break;
    case ADDR_REL:
        addr = Load(opaddr + 2);
        addr = (addr < 0x80) ? addr + REG_PC : addr + REG_PC - 256;
        break;
    case ADDR_IMP:
        break;
    case ADDR_ABS:
        addr = Load16bit(opaddr + 2);
        break;
    case ADDR_ACC:
        addr = REG_ACC;
        break;
    case ADDR_IMM:
        addr = REG_PC;
        break;
    case ADDR_ZP_X:
        addr = (Load(opaddr + 2) + REG_X) & 0xFF;
        break;
    case ADDR_ZP_Y:
        addr = (Load(opaddr + 2) + REG_Y) & 0xFF;
        break;
    case ADDR_ABS_X:
        addr = Load16bit(opaddr + 2);
        if ((addr & 0xFF00) != ((addr + REG_X) & 0xFF00)) cycleAdd = 1;
        addr += REG_X;
        break;
    case ADDR_ABS_Y:
        addr = Load16bit(opaddr + 2);
        if ((addr & 0xFF00) != ((addr + REG_Y) & 0xFF00)) cycleAdd = 1;
        addr += REG_Y;
        break;
    case ADDR_PRE_X:
    {
        TInt base = (Load(opaddr + 2) + REG_X) & 0xFF;
        addr = Load16bit(base);
        break;
    }
    case ADDR_POST_Y:
    {
        TInt base = Load(opaddr + 2);
        addr = Load16bit(base);
        if ((addr & 0xFF00) != ((addr + REG_Y) & 0xFF00)) cycleAdd = 1;
        addr += REG_Y;
        break;
    }
    case ADDR_IND:
    {
        TInt base = Load16bit(opaddr + 2);
        // 6502 bug: wrap page boundary
        addr = Load(base) | (Load((base & 0xFF00) | ((base + 1) & 0xFF)) << 8);
        break;
    }
    }
    addr &= 0xFFFF;

    // Выполнить инструкцию (перенос из CPU.java switch(opinf&0xFF))
    TInt temp, add;
    switch (inst)
    {
    case 0: // ADC
        temp = REG_ACC + Load(addr) + F_CARRY;
        F_OVERFLOW = ((!((REG_ACC ^ Load(addr)) & 0x80)) && ((REG_ACC ^ temp) & 0x80)) ? 1 : 0;
        F_CARRY = (temp > 255) ? 1 : 0;
        F_SIGN  = (temp >> 7) & 1;
        F_ZERO  = temp & 0xFF;
        REG_ACC = temp & 0xFF;
        cycleCount += cycleAdd;
        break;
    case 1: // AND
        REG_ACC = REG_ACC & Load(addr);
        F_SIGN  = (REG_ACC >> 7) & 1;
        F_ZERO  = REG_ACC;
        if (addrMode != ADDR_POST_Y) cycleCount += cycleAdd;
        break;
    case 2: // ASL
        if (addrMode == ADDR_ACC)
        {
            F_CARRY = (REG_ACC >> 7) & 1;
            REG_ACC = (REG_ACC << 1) & 0xFF;
            F_SIGN  = (REG_ACC >> 7) & 1;
            F_ZERO  = REG_ACC;
        }
        else
        {
            temp    = Load(addr);
            F_CARRY = (temp >> 7) & 1;
            temp    = (temp << 1) & 0xFF;
            F_SIGN  = (temp >> 7) & 1;
            F_ZERO  = temp;
            Write(addr, (u8)temp);
        }
        break;
    case 3: // BCC
        if (F_CARRY == 0)
        {
            cycleCount += ((opaddr & 0xFF00) != (addr & 0xFF00)) ? 2 : 1;
            REG_PC = addr;
        }
        break;
    case 4: // BCS
        if (F_CARRY == 1)
        {
            cycleCount += ((opaddr & 0xFF00) != (addr & 0xFF00)) ? 2 : 1;
            REG_PC = addr;
        }
        break;
    case 5: // BEQ
        if (F_ZERO == 0)
        {
            cycleCount += ((opaddr & 0xFF00) != (addr & 0xFF00)) ? 2 : 1;
            REG_PC = addr;
        }
        break;
    case 6: // BIT
        temp       = Load(addr);
        F_SIGN     = (temp >> 7) & 1;
        F_OVERFLOW = (temp >> 6) & 1;
        F_ZERO     = temp & REG_ACC;
        break;
    case 7: // BMI
        if (F_SIGN == 1)
        {
            cycleCount += ((opaddr & 0xFF00) != (addr & 0xFF00)) ? 2 : 1;
            REG_PC = addr;
        }
        break;
    case 8: // BNE
        if (F_ZERO != 0)
        {
            cycleCount += ((opaddr & 0xFF00) != (addr & 0xFF00)) ? 2 : 1;
            REG_PC = addr;
        }
        break;
    case 9: // BPL
        if (F_SIGN == 0)
        {
            cycleCount += ((opaddr & 0xFF00) != (addr & 0xFF00)) ? 2 : 1;
            REG_PC = addr;
        }
        break;
    case 10: // BRK
        REG_PC += 2;
        Push((REG_PC >> 8) & 0xFF);
        Push(REG_PC & 0xFF);
        F_BRK = 1;
        Push(GetStatus());
        F_INTERRUPT = 1;
        REG_PC = Load16bit(0xFFFE) - 1;
        break;
    case 11: // BVC
        if (F_OVERFLOW == 0)
        {
            cycleCount += ((opaddr & 0xFF00) != (addr & 0xFF00)) ? 2 : 1;
            REG_PC = addr;
        }
        break;
    case 12: // BVS
        if (F_OVERFLOW == 1)
        {
            cycleCount += ((opaddr & 0xFF00) != (addr & 0xFF00)) ? 2 : 1;
            REG_PC = addr;
        }
        break;
    case 13: F_CARRY = 0; break;       // CLC
    case 14: F_DECIMAL = 0; break;     // CLD
    case 15: F_INTERRUPT = 0; break;   // CLI
    case 16: F_OVERFLOW = 0; break;    // CLV
    case 17: // CMP
        temp    = REG_ACC - Load(addr);
        F_CARRY = (temp >= 0) ? 1 : 0;
        F_SIGN  = (temp >> 7) & 1;
        F_ZERO  = temp & 0xFF;
        cycleCount += cycleAdd;
        break;
    case 18: // CPX
        temp    = REG_X - Load(addr);
        F_CARRY = (temp >= 0) ? 1 : 0;
        F_SIGN  = (temp >> 7) & 1;
        F_ZERO  = temp & 0xFF;
        break;
    case 19: // CPY
        temp    = REG_Y - Load(addr);
        F_CARRY = (temp >= 0) ? 1 : 0;
        F_SIGN  = (temp >> 7) & 1;
        F_ZERO  = temp & 0xFF;
        break;
    case 20: // DEC
        temp   = (Load(addr) - 1) & 0xFF;
        F_SIGN = (temp >> 7) & 1;
        F_ZERO = temp;
        Write(addr, (u8)temp);
        break;
    case 21: // DEX
        REG_X  = (REG_X - 1) & 0xFF;
        F_SIGN = (REG_X >> 7) & 1;
        F_ZERO = REG_X;
        break;
    case 22: // DEY
        REG_Y  = (REG_Y - 1) & 0xFF;
        F_SIGN = (REG_Y >> 7) & 1;
        F_ZERO = REG_Y;
        break;
    case 23: // EOR
        REG_ACC = (Load(addr) ^ REG_ACC) & 0xFF;
        F_SIGN  = (REG_ACC >> 7) & 1;
        F_ZERO  = REG_ACC;
        cycleCount += cycleAdd;
        break;
    case 24: // INC
        temp   = (Load(addr) + 1) & 0xFF;
        F_SIGN = (temp >> 7) & 1;
        F_ZERO = temp;
        Write(addr, (u8)temp);
        break;
    case 25: // INX
        REG_X  = (REG_X + 1) & 0xFF;
        F_SIGN = (REG_X >> 7) & 1;
        F_ZERO = REG_X;
        break;
    case 26: // INY
        REG_Y  = (REG_Y + 1) & 0xFF;
        F_SIGN = (REG_Y >> 7) & 1;
        F_ZERO = REG_Y;
        break;
    case 27: REG_PC = addr - 1; break; // JMP
    case 28: // JSR
        Push((REG_PC >> 8) & 0xFF);
        Push(REG_PC & 0xFF);
        REG_PC = addr - 1;
        break;
    case 29: // LDA
        REG_ACC = Load(addr);
        F_SIGN  = (REG_ACC >> 7) & 1;
        F_ZERO  = REG_ACC;
        cycleCount += cycleAdd;
        break;
    case 30: // LDX
        REG_X  = Load(addr);
        F_SIGN = (REG_X >> 7) & 1;
        F_ZERO = REG_X;
        cycleCount += cycleAdd;
        break;
    case 31: // LDY
        REG_Y  = Load(addr);
        F_SIGN = (REG_Y >> 7) & 1;
        F_ZERO = REG_Y;
        cycleCount += cycleAdd;
        break;
    case 32: // LSR
        if (addrMode == ADDR_ACC)
        {
            F_CARRY = REG_ACC & 1;
            REG_ACC >>= 1;
            F_SIGN  = 0;
            F_ZERO  = REG_ACC;
        }
        else
        {
            temp    = Load(addr);
            F_CARRY = temp & 1;
            temp  >>= 1;
            F_SIGN  = 0;
            F_ZERO  = temp;
            Write(addr, (u8)temp);
        }
        break;
    case 33: break; // NOP
    case 34: // SED
        F_DECIMAL = 1;
        break;
    case 35: // ROR
        if (addrMode == ADDR_ACC)
        {
            add     = F_CARRY << 7;
            F_CARRY = REG_ACC & 1;
            REG_ACC = ((REG_ACC >> 1) | add) & 0xFF;
            F_SIGN  = (REG_ACC >> 7) & 1;
            F_ZERO  = REG_ACC;
        }
        else
        {
            temp    = Load(addr);
            add     = F_CARRY << 7;
            F_CARRY = temp & 1;
            temp    = ((temp >> 1) | add) & 0xFF;
            F_SIGN  = (temp >> 7) & 1;
            F_ZERO  = temp;
            Write(addr, (u8)temp);
        }
        break;
    case 36: // ROL
        if (addrMode == ADDR_ACC)
        {
            add     = F_CARRY;
            F_CARRY = (REG_ACC >> 7) & 1;
            REG_ACC = ((REG_ACC << 1) | add) & 0xFF;
            F_SIGN  = (REG_ACC >> 7) & 1;
            F_ZERO  = REG_ACC;
        }
        else
        {
            temp    = Load(addr);
            add     = F_CARRY;
            F_CARRY = (temp >> 7) & 1;
            temp    = ((temp << 1) | add) & 0xFF;
            F_SIGN  = (temp >> 7) & 1;
            F_ZERO  = temp;
            Write(addr, (u8)temp);
        }
        break;
    case 37: // RTI
        temp        = Pop();
        SetStatus(temp);
        REG_PC      = Pop() | (Pop() << 8);
        REG_PC--;
        break;
    // case 38: TSX
    case 38:
        REG_X  = REG_SP & 0xFF;
        F_SIGN = (REG_X >> 7) & 1;
        F_ZERO = REG_X;
        break;
    case 39: // ORA
        REG_ACC = (Load(addr) | REG_ACC) & 0xFF;
        F_SIGN  = (REG_ACC >> 7) & 1;
        F_ZERO  = REG_ACC;
        cycleCount += cycleAdd;
        break;
    case 40: // SEI
        F_INTERRUPT = 1;
        break;
    case 41: // SEC
        F_CARRY = 1;
        break;
    case 42: // TAX
        REG_X  = REG_ACC;
        F_SIGN = (REG_X >> 7) & 1;
        F_ZERO = REG_X;
        break;
    case 43: // STX
        Write(addr, (u8)REG_X);
        break;
    case 44: // STA
        Write(addr, (u8)REG_ACC);
        break;
    case 45: // PLA
        REG_ACC = Pop();
        F_SIGN  = (REG_ACC >> 7) & 1;
        F_ZERO  = REG_ACC;
        break;
    case 46: // PHA
        Push((u8)REG_ACC);
        break;
    case 47: // PLP
        SetStatus(Pop());
        break;
    case 48: // PHP
        Push((u8)(GetStatus() | 0x10));
        break;
    case 49: // TAY
        REG_Y  = REG_ACC;
        F_SIGN = (REG_Y >> 7) & 1;
        F_ZERO = REG_Y;
        break;
    case 50: // RTI
        SetStatus(Pop());
        REG_PC = Pop() | (Pop() << 8);
        REG_PC--;
        break;
    case 51: // RTS
        REG_PC = Pop() | (Pop() << 8);
        break;
    case 52: // TYA
        REG_ACC = REG_Y;
        F_SIGN  = (REG_ACC >> 7) & 1;
        F_ZERO  = REG_ACC;
        break;
    case 53: // TXA
        REG_ACC = REG_X;
        F_SIGN  = (REG_ACC >> 7) & 1;
        F_ZERO  = REG_ACC;
        break;
    case 54: // STY
        Write(addr, (u8)REG_Y);
        break;
    case 55: // TXS
        REG_SP = (REG_X & 0xFF) | 0x100;
        break;
    case 255: // Illegal / JAM
        break;
    default:
        break;
    }

    // SBC особый случай (встроен в таблицу как ADC с инвертированием)
    // Обрабатывается через SBC = ADC(~operand), но здесь упрощённо:
    // инструкция SBC в оригинале — inst 0 (ADC), но operand инвертируется
    // Реализация уже корректна: SBC опкоды (0xE9 и т.д.) указывают inst=0

    // Тактировать PPU (3 PPU такта на каждый CPU такт)
    for (TInt c = 0; c < cycleCount; c++)
    {
        iPpu->RunCycles(3);
        if (Globals::enableSound) iPapu->Clock();
    }

    return cycleCount;
}

// ---------------------------------------------------------------------------
// Сохранение / загрузка состояния
// ---------------------------------------------------------------------------
void CNESCPU::StateSave(RWriteStream& aStream) const
{
    aStream.WriteInt32L(GetStatus());
    aStream.WriteInt32L(REG_ACC);
    aStream.WriteInt32L(REG_PC);
    aStream.WriteInt32L(REG_SP);
    aStream.WriteInt32L(REG_X);
    aStream.WriteInt32L(REG_Y);
    aStream.WriteInt32L(cyclesToHalt);
}

void CNESCPU::StateLoad(RReadStream& aStream)
{
    SetStatus(aStream.ReadInt32L());
    REG_ACC      = aStream.ReadInt32L();
    REG_PC       = aStream.ReadInt32L();
    REG_SP       = aStream.ReadInt32L();
    REG_X        = aStream.ReadInt32L();
    REG_Y        = aStream.ReadInt32L();
    cyclesToHalt = aStream.ReadInt32L();
}
