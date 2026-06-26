/*
 * NESSystem.cpp — Аналог NES.java
 * Создаёт и координирует все компоненты системы.
 */
#include "NESSystem.h"
#include "NESCPU.h"
#include "NESPPU.h"
#include "NESPAPU.h"
#include "NESMemory.h"
#include "NESMapper.h"
#include "NESROM.h"
#include "NESTypes.h"
#include <s32file.h>
#include <f32file.h>

// ---------------------------------------------------------------------------
// Конструкция
// ---------------------------------------------------------------------------
CNESSystem* CNESSystem::NewLC()
{
    CNESSystem* self = new (ELeave) CNESSystem();
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}

CNESSystem::CNESSystem()
    : iCpu(NULL), iPpu(NULL), iPapu(NULL),
      iCpuMem(NULL), iPpuMem(NULL), iSprMem(NULL),
      iRom(NULL), iMapper(NULL), iRunning(EFalse),
      iJoyStrobe(EFalse)
{
    iJoyState[0] = iJoyState[1] = 0;
    iJoyShift[0] = iJoyShift[1] = 0;
}

void CNESSystem::ConstructL()
{
    // Создать память
    iCpuMem = CNESMemory::NewLC(KCPU_MEM_SIZE); CleanupStack::Pop();
    iPpuMem = CNESMemory::NewLC(KPPU_MEM_SIZE); CleanupStack::Pop();
    iSprMem = CNESMemory::NewLC(KSPR_MEM_SIZE); CleanupStack::Pop();

    // PPU нужна до CPU (CPU тактирует PPU)
    iPpu  = CNESPPU::NewLC(iPpuMem, iSprMem); CleanupStack::Pop();

    // APU
    iPapu = CNESPAPU::NewLC(NULL); CleanupStack::Pop(); // CPU устанавливается позже

    // CPU
    iCpu  = CNESCPU::NewLC(iCpuMem, iPpu, iPapu); CleanupStack::Pop();

    // Инициализация
    iPpu->Init();
    iCpu->Init();

    // Инициализация регистров APU
    const TInt kInitRegs[] = {
        0x4000, 0x4001, 0x4002, 0x4003, 0x4004, 0x4005, 0x4006, 0x4007,
        0x4008, 0x4009, 0x400A, 0x400B, 0x400C, 0x400D, 0x400E, 0x400F,
        0x4010, 0x4011, 0x4012, 0x4013
    };
    for (TUint i = 0; i < sizeof(kInitRegs)/sizeof(kInitRegs[0]); i++)
        iPapu->WriteReg(kInitRegs[i], (kInitRegs[i] == 0x4010) ? 0x10 : 0x00);

    ClearCPUMemory();
}

CNESSystem::~CNESSystem()
{
    StopEmulation();
    delete iCpu;
    delete iPpu;
    delete iPapu;
    delete iMapper;
    delete iRom;
    delete iCpuMem;
    delete iPpuMem;
    delete iSprMem;
}

// ---------------------------------------------------------------------------
// Загрузка ROM
// ---------------------------------------------------------------------------
TBool CNESSystem::LoadRomL(const TDesC& aFileName)
{
    if (iRunning) StopEmulation();

    delete iRom; iRom = NULL;
    delete iMapper; iMapper = NULL;

    iRom = CNESROM::NewLC(); CleanupStack::Pop();
    iRom->LoadL(aFileName);

    if (!iRom->IsValid()) return EFalse;

    iRomPath = aFileName;
    Reset();

    // Создать маппер
    iMapper = CNESMapper::CreateL(iRom->MapperType());
    iMapper->InitL(iRom, iCpuMem, iPpuMem, iPpu, iCpu);
    iCpu->SetMapper(iMapper);
    iPpu->SetMapper(iMapper);
    iPpu->SetMirroring(iRom->MirroringType());

    // Загрузить SRAM если есть battery
    if (iRom->HasBatteryRAM())
    {
        TFileName sramPath = aFileName;
        sramPath.Replace(sramPath.Length() - 4, 4, _L(".sav"));
        TRAP_IGNORE(iRom->LoadSRAML(sramPath));
    }

    return ETrue;
}

// ---------------------------------------------------------------------------
// Reset
// ---------------------------------------------------------------------------
void CNESSystem::Reset()
{
    iCpuMem->Reset();
    iPpuMem->Reset();
    iSprMem->Reset();
    ClearCPUMemory();
    iCpu->Reset();
    iCpu->Init();
    iPpu->Reset();
    if (iMapper) iMapper->Reset();
}

void CNESSystem::ClearCPUMemory()
{
    // Аналог clearCPUMemory() из NES.java
    u8* mem = iCpuMem->Ptr();
    Mem::Fill(mem, 0x2000, Globals::memoryFlushValue);
    for (TInt p = 0; p < 4; p++)
    {
        TInt base = p * 0x800;
        mem[base + 0x008] = 0xF7;
        mem[base + 0x009] = 0xEF;
        mem[base + 0x00A] = 0xDF;
        mem[base + 0x00F] = 0xBF;
    }
}

// ---------------------------------------------------------------------------
// Эмуляция
// ---------------------------------------------------------------------------
void CNESSystem::StartEmulation()
{
    if (iRom && iRom->IsValid())
    {
        iRunning = ETrue;
        if (Globals::enableSound) iPapu->Start();
    }
}

void CNESSystem::StopEmulation()
{
    iRunning = EFalse;
    iPapu->Stop();

    // Сохранить SRAM
    if (iRom && iRom->IsValid() && iRom->HasBatteryRAM())
    {
        TFileName sramPath = iRomPath;
        if (sramPath.Length() >= 4)
        {
            sramPath.Replace(sramPath.Length() - 4, 4, _L(".sav"));
            TRAP_IGNORE(iRom->SaveSRAML(sramPath));
        }
    }
}

// ---------------------------------------------------------------------------
// RunFrameL — выполнить ровно один кадр (~29780 CPU циклов для NTSC)
// ---------------------------------------------------------------------------
TBool CNESSystem::RunFrameL()
{
    if (!iRunning || !iRom || !iRom->IsValid()) return EFalse;

    // ~29780 CPU тактов = один NTSC-кадр (262 строки × 113.6 циклов)
    const TInt KCyclesPerFrame = 29780;
    TInt cycles = 0;
    while (cycles < KCyclesPerFrame)
    {
        cycles += iCpu->Step();
    }

    TBool frame = iPpu->FrameReady();
    if (frame) iPpu->ClearFrameReady();
    return frame;
}

// ---------------------------------------------------------------------------
// Джойстик
// ---------------------------------------------------------------------------
void CNESSystem::SetJoyState(TInt aPlayer, u8 aButtons)
{
    if (aPlayer >= 0 && aPlayer < 2)
        iJoyState[aPlayer] = aButtons;
}

// ---------------------------------------------------------------------------
// Сохранение состояния
// ---------------------------------------------------------------------------
TFileName CNESSystem::MakeSaveStatePath(TInt aSlot) const
{
    TFileName path = iRomPath;
    path.SetLength(path.Length() - 4); // убрать .nes
    path.AppendFormat(_L(".st%d"), aSlot);
    return path;
}

void CNESSystem::StateSaveL(TInt aSlot) const
{
    TFileName path = MakeSaveStatePath(aSlot);
    RFs fs; User::LeaveIfError(fs.Connect()); CleanupClosePushL(fs);
    RFileWriteStream ws;
    User::LeaveIfError(ws.Replace(fs, path, EFileWrite));
    ws.PushL();

    ws.WriteUint8L(1); // версия

    iCpuMem->StateSave(ws);
    iPpuMem->StateSave(ws);
    iSprMem->StateSave(ws);
    iCpu->StateSave(ws);
    if (iMapper) iMapper->StateSave(ws);
    iPpu->StateSave(ws);

    ws.CommitL();
    CleanupStack::PopAndDestroy(2); // ws, fs
}

void CNESSystem::StateLoadL(TInt aSlot)
{
    TFileName path = MakeSaveStatePath(aSlot);
    RFs fs; User::LeaveIfError(fs.Connect()); CleanupClosePushL(fs);
    RFileReadStream rs;
    User::LeaveIfError(rs.Open(fs, path, EFileRead));
    rs.PushL();

    TUint8 ver = rs.ReadUint8L();
    if (ver == 1)
    {
        iCpuMem->StateLoad(rs);
        iPpuMem->StateLoad(rs);
        iSprMem->StateLoad(rs);
        iCpu->StateLoad(rs);
        if (iMapper) iMapper->StateLoad(rs);
        iPpu->StateLoad(rs);
    }

    CleanupStack::PopAndDestroy(2);
}

const u32* CNESSystem::FrameBuffer() const
{
    return iPpu->FrameBuffer();
}
