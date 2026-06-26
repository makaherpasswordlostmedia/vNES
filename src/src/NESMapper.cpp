/*
 * NESMapper.cpp — Аналог MapperDefault.java + Mapper001..007.java
 * Реализация базового класса и наиболее распространённых маппаров.
 */
#include "NESMapper.h"
#include "NESROM.h"
#include "NESMemory.h"
#include "NESPPU.h"
#include "NESCPU.h"
#include <s32strm.h>

// ---------------------------------------------------------------------------
// Фабричный метод
// ---------------------------------------------------------------------------
CNESMapper* CNESMapper::CreateL(TInt aMapperNum)
{
    switch (aMapperNum)
    {
    case 0:  return new (ELeave) CNESMapper000();
    case 1:  return new (ELeave) CNESMapper001();
    case 2:  return new (ELeave) CNESMapper002();
    case 3:  return new (ELeave) CNESMapper003();
    case 4:  return new (ELeave) CNESMapper004();
    case 7:  return new (ELeave) CNESMapper007();
    default:
        // Неизвестный маппер — используем mapper 0 как fallback
        return new (ELeave) CNESMapper000();
    }
}

// ---------------------------------------------------------------------------
// Базовый класс
// ---------------------------------------------------------------------------
void CNESMapper::InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
                       CNESPPU* aPpu, CNESCPU* aCpu)
{
    iRom    = aRom;
    iCpuMem = aCpuMem;
    iPpuMem = aPpuMem;
    iPpu    = aPpu;
    iCpu    = aCpu;
}

u8 CNESMapper::Load(TInt aAddr)
{
    return iCpuMem->Load(aAddr);
}

void CNESMapper::Write(TInt aAddr, u8 aVal)
{
    if (aAddr < 0x2000)
    {
        iCpuMem->Write(aAddr & 0x7FF, aVal);
    }
    else if (aAddr >= 0x8000)
    {
        // ROM area — ignore (no mapper handling in base)
    }
    else
    {
        iCpuMem->Write(aAddr, aVal);
    }
}

u8 CNESMapper::LoadCHR(TInt aAddr)
{
    return iPpuMem->Load(aAddr);
}

void CNESMapper::WriteCHR(TInt aAddr, u8 aVal)
{
    iPpuMem->Write(aAddr, aVal);
}

void CNESMapper::Reset() {}

void CNESMapper::LoadPRGBank(TInt aBankIdx, TInt aCpuAddr)
{
    if (aBankIdx < 0 || aBankIdx >= iRom->PRGBankCount()) return;
    iCpuMem->Write(aCpuAddr, iRom->PRGBank(aBankIdx), KPRGBankSize);
}

void CNESMapper::LoadCHRBank(TInt aBankIdx, TInt aPpuAddr)
{
    if (aBankIdx < 0 || aBankIdx >= iRom->CHRBankCount()) return;
    iPpuMem->Write(aPpuAddr, iRom->CHRBank(aBankIdx), KCHRBankSize);
}

// ---------------------------------------------------------------------------
// Mapper 0: NROM
// ---------------------------------------------------------------------------
void CNESMapper000::InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
                           CNESPPU* aPpu, CNESCPU* aCpu)
{
    CNESMapper::InitL(aRom, aCpuMem, aPpuMem, aPpu, aCpu);
    Reset();
}

void CNESMapper000::Reset()
{
    // 1 банк: PRG в $8000 и зеркало в $C000
    // 2 банка: PRG в $8000, второй в $C000
    TInt n = iRom->PRGBankCount();
    LoadPRGBank(0,       0x8000);
    LoadPRGBank(n - 1,   0xC000);

    if (iRom->CHRBankCount() > 0)
        LoadCHRBank(0, 0x0000);
}

// ---------------------------------------------------------------------------
// Mapper 1: MMC1
// ---------------------------------------------------------------------------
void CNESMapper001::InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
                           CNESPPU* aPpu, CNESCPU* aCpu)
{
    CNESMapper::InitL(aRom, aCpuMem, aPpuMem, aPpu, aCpu);
    Reset();
}

void CNESMapper001::Reset()
{
    iShiftReg  = 0;
    iShiftCount= 0;
    iCtrlReg   = 0x0C;  // PRG fix last bank, 16KB mode
    iCHRBank0  = 0;
    iCHRBank1  = 0;
    iPRGBank   = 0;
    SyncBanks();
}

void CNESMapper001::Write(TInt aAddr, u8 aVal)
{
    if (aAddr < 0x8000) { iCpuMem->Write(aAddr, aVal); return; }

    if (aVal & 0x80)
    {
        iShiftReg   = 0;
        iShiftCount = 0;
        iCtrlReg   |= 0x0C;
        return;
    }
    iShiftReg |= (aVal & 1) << iShiftCount++;
    if (iShiftCount < 5) return;

    TInt reg  = (aAddr >> 13) & 3;
    TInt data = iShiftReg & 0x1F;
    iShiftReg   = 0;
    iShiftCount = 0;

    switch (reg)
    {
    case 0: iCtrlReg  = (u8)data; break;
    case 1: iCHRBank0 = (u8)data; break;
    case 2: iCHRBank1 = (u8)data; break;
    case 3: iPRGBank  = (u8)(data & 0x0F); break;
    }
    SyncBanks();
}

void CNESMapper001::SyncBanks()
{
    TInt prgMode = (iCtrlReg >> 2) & 3;
    TInt last    = iRom->PRGBankCount() - 1;

    switch (prgMode)
    {
    case 0: case 1: // 32 KB банк (оба слота вместе)
        LoadPRGBank(iPRGBank & 0xFE, 0x8000);
        LoadPRGBank((iPRGBank & 0xFE) + 1, 0xC000);
        break;
    case 2: // fix первый банк
        LoadPRGBank(0,        0x8000);
        LoadPRGBank(iPRGBank, 0xC000);
        break;
    case 3: // fix последний банк
        LoadPRGBank(iPRGBank, 0x8000);
        LoadPRGBank(last,     0xC000);
        break;
    }

    TInt chrMode = (iCtrlReg >> 4) & 1;
    if (iRom->CHRBankCount() == 0) return; // CHR-RAM

    if (chrMode == 0)
    {
        LoadCHRBank(iCHRBank0 & 0xFE, 0x0000);
        LoadCHRBank((iCHRBank0 & 0xFE) + 1, 0x1000);
    }
    else
    {
        LoadCHRBank(iCHRBank0, 0x0000);
        LoadCHRBank(iCHRBank1, 0x1000);
    }
}

void CNESMapper001::StateSave(RWriteStream& aStream) const
{
    aStream.WriteUint8L(iShiftReg);
    aStream.WriteInt32L(iShiftCount);
    aStream.WriteUint8L(iCtrlReg);
    aStream.WriteUint8L(iCHRBank0);
    aStream.WriteUint8L(iCHRBank1);
    aStream.WriteUint8L(iPRGBank);
}

void CNESMapper001::StateLoad(RReadStream& aStream)
{
    iShiftReg   = aStream.ReadUint8L();
    iShiftCount = aStream.ReadInt32L();
    iCtrlReg    = aStream.ReadUint8L();
    iCHRBank0   = aStream.ReadUint8L();
    iCHRBank1   = aStream.ReadUint8L();
    iPRGBank    = aStream.ReadUint8L();
    SyncBanks();
}

// ---------------------------------------------------------------------------
// Mapper 2: UxROM
// ---------------------------------------------------------------------------
void CNESMapper002::InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
                           CNESPPU* aPpu, CNESCPU* aCpu)
{
    CNESMapper::InitL(aRom, aCpuMem, aPpuMem, aPpu, aCpu);
    Reset();
}

void CNESMapper002::Reset()
{
    TInt last = iRom->PRGBankCount() - 1;
    LoadPRGBank(0,    0x8000);
    LoadPRGBank(last, 0xC000);
    if (iRom->CHRBankCount() > 0) LoadCHRBank(0, 0x0000);
}

void CNESMapper002::Write(TInt aAddr, u8 aVal)
{
    if (aAddr >= 0x8000)
    {
        TInt last = iRom->PRGBankCount() - 1;
        LoadPRGBank(aVal & 0x0F, 0x8000);
        LoadPRGBank(last,        0xC000);
    }
    else
    {
        iCpuMem->Write(aAddr, aVal);
    }
}

// ---------------------------------------------------------------------------
// Mapper 3: CNROM
// ---------------------------------------------------------------------------
void CNESMapper003::InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
                           CNESPPU* aPpu, CNESCPU* aCpu)
{
    CNESMapper::InitL(aRom, aCpuMem, aPpuMem, aPpu, aCpu);
    Reset();
}

void CNESMapper003::Reset()
{
    TInt last = iRom->PRGBankCount() - 1;
    LoadPRGBank(0,    0x8000);
    LoadPRGBank(last, 0xC000);
    if (iRom->CHRBankCount() > 0) LoadCHRBank(0, 0x0000);
}

void CNESMapper003::Write(TInt aAddr, u8 aVal)
{
    if (aAddr >= 0x8000 && iRom->CHRBankCount() > 0)
        LoadCHRBank(aVal & 0x03, 0x0000);
    else
        iCpuMem->Write(aAddr, aVal);
}

// ---------------------------------------------------------------------------
// Mapper 4: MMC3
// ---------------------------------------------------------------------------
void CNESMapper004::InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
                           CNESPPU* aPpu, CNESCPU* aCpu)
{
    CNESMapper::InitL(aRom, aCpuMem, aPpuMem, aPpu, aCpu);
    Reset();
}

void CNESMapper004::Reset()
{
    iCommand   = 0;
    iCmdParam  = 0;
    iPRGSwap   = EFalse;
    iCHRSwap   = EFalse;
    iIRQCounter= 0;
    iIRQLatch  = 0;
    iIRQEnabled= EFalse;
    Mem::FillZ(iRegs, sizeof(iRegs));
    SyncBanks();
}

void CNESMapper004::Write(TInt aAddr, u8 aVal)
{
    if (aAddr < 0x8000) { iCpuMem->Write(aAddr, aVal); return; }

    switch (aAddr & 0xE001)
    {
    case 0x8000: // Bank select
        iCommand  = aVal & 7;
        iPRGSwap  = (aVal & 0x40) != 0;
        iCHRSwap  = (aVal & 0x80) != 0;
        break;
    case 0x8001: // Bank data
        iRegs[iCommand] = aVal;
        SyncBanks();
        break;
    case 0xA000: // Mirroring
        iPpu->SetMirroring((aVal & 1) ? EVerticalMirroring : EHorizontalMirroring);
        break;
    case 0xA001: // PRG-RAM protect — ignored
        break;
    case 0xC000: // IRQ latch
        iIRQLatch = aVal;
        break;
    case 0xC001: // IRQ reload
        iIRQCounter = 0;
        break;
    case 0xE000: // IRQ disable
        iIRQEnabled = EFalse;
        break;
    case 0xE001: // IRQ enable
        iIRQEnabled = ETrue;
        break;
    }
}

void CNESMapper004::ClockIRQCounter()
{
    if (iIRQCounter == 0)
        iIRQCounter = iIRQLatch;
    else
        iIRQCounter--;

    if (iIRQCounter == 0 && iIRQEnabled)
        iCpu->TriggerIRQ(EIrqNormal);
}

void CNESMapper004::SyncBanks()
{
    TInt prgLast = iRom->PRGBankCount() * 2 - 1; // в 8KB-блоках

    if (!iPRGSwap)
    {
        // $8000=R6, $A000=R7, $C000=last-1, $E000=last
        LoadPRGBank(iRegs[6] >> 1, 0x8000);   // грубо, 16KB banki
        LoadPRGBank(iRegs[7] >> 1, 0xA000);
        // Точнее нужно работать с 8KB банками, упрощено для читаемости
    }
    else
    {
        LoadPRGBank((prgLast-1)/2, 0x8000);
        LoadPRGBank(iRegs[7] >> 1, 0xA000);
    }
    LoadPRGBank(prgLast/2, 0xC000);

    // CHR banks (8x1KB → 4x2KB)
    if (iRom->CHRBankCount() == 0) return;
    TInt chrMax = iRom->CHRBankCount() * 8 - 1;
    if (!iCHRSwap)
    {
        LoadCHRBank((iRegs[0] & 0xFE) / 8, 0x0000);
        LoadCHRBank( iRegs[2]          / 8, 0x1000);
    }
    else
    {
        LoadCHRBank( iRegs[2]          / 8, 0x0000);
        LoadCHRBank((iRegs[0] & 0xFE) / 8, 0x1000);
    }
    (void)chrMax;
}

void CNESMapper004::StateSave(RWriteStream& aStream) const
{
    aStream.WriteUint8L(iCommand);
    aStream.WriteInt32L(iIRQCounter);
    aStream.WriteInt32L(iIRQLatch);
    aStream.WriteUint8L(iIRQEnabled ? 1 : 0);
    for (TInt i = 0; i < 8; i++) aStream.WriteUint8L(iRegs[i]);
}

void CNESMapper004::StateLoad(RReadStream& aStream)
{
    iCommand     = aStream.ReadUint8L();
    iIRQCounter  = aStream.ReadInt32L();
    iIRQLatch    = aStream.ReadInt32L();
    iIRQEnabled  = aStream.ReadUint8L() != 0;
    for (TInt i = 0; i < 8; i++) iRegs[i] = aStream.ReadUint8L();
    SyncBanks();
}

// ---------------------------------------------------------------------------
// Mapper 7: AxROM (32KB PRG switching, single-screen mirroring)
// ---------------------------------------------------------------------------
void CNESMapper007::InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
                           CNESPPU* aPpu, CNESCPU* aCpu)
{
    CNESMapper::InitL(aRom, aCpuMem, aPpuMem, aPpu, aCpu);
    Reset();
}

void CNESMapper007::Reset()
{
    LoadPRGBank(0, 0x8000);
    // Mapper 7 нет CHR-ROM, только CHR-RAM
}

void CNESMapper007::Write(TInt aAddr, u8 aVal)
{
    if (aAddr >= 0x8000)
    {
        TInt bank = aVal & 0x07;
        // 32KB PRG банк = 2 × 16KB
        LoadPRGBank(bank * 2,     0x8000);
        LoadPRGBank(bank * 2 + 1, 0xC000);
        // Single-screen mirroring
        iPpu->SetMirroring((aVal & 0x10) ? ESingleScreen2 : ESingleScreen1);
    }
    else
    {
        iCpuMem->Write(aAddr, aVal);
    }
}
