/*
 * NESMapper.h — Аналог MemoryMapper.java / MapperDefault.java
 * Базовый класс маппера. Конкретные маппера наследуют от него.
 */

#ifndef NESMAPPER_H
#define NESMAPPER_H

#include "NESTypes.h"

class CNESROM;
class CNESMemory;
class CNESPPU;
class RWriteStream;
class RReadStream;
class CNESCPU;

class CNESMapper : public CBase
{
public:
    virtual ~CNESMapper() {}

    // Инициализация после создания
    virtual void InitL(CNESROM* aRom,
                       CNESMemory* aCpuMem,
                       CNESMemory* aPpuMem,
                       CNESPPU*    aPpu,
                       CNESCPU*    aCpu);

    // Чтение / запись через маппер
    virtual u8   Load(TInt aAddr);
    virtual void Write(TInt aAddr, u8 aVal);

    // PPU-сторона (CHR)
    virtual u8   LoadCHR(TInt aAddr);
    virtual void WriteCHR(TInt aAddr, u8 aVal);

    // IRQ counter tick (для MMC3 и т.п.)
    virtual void ClockIRQCounter() {}

    // Сброс (смена банков в исходное состояние)
    virtual void Reset();

    // Сохранение / загрузка состояния маппера
    virtual void StateSave(RWriteStream& aStream) const {}
    virtual void StateLoad(RReadStream&  aStream)       {}

    // Фабричный метод — создаёт нужный маппер по номеру
    static CNESMapper* CreateL(TInt aMapperNum);

protected:
    // Утилиты для загрузки PRG-банков в CPU-память
    void LoadPRGBank(TInt aBankIdx, TInt aCpuAddr);
    // Утилиты для загрузки CHR-банков в PPU-память
    void LoadCHRBank(TInt aBankIdx, TInt aPpuAddr);

    CNESROM*    iRom;
    CNESMemory* iCpuMem;
    CNESMemory* iPpuMem;
    CNESPPU*    iPpu;
    CNESCPU*    iCpu;
};

// ---------------------------------------------------------------------------
// Mapper 0: NROM (нет переключения банков)
// ---------------------------------------------------------------------------
class CNESMapper000 : public CNESMapper
{
public:
    void InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
               CNESPPU* aPpu, CNESCPU* aCpu) override;
    void Reset() override;
};

// ---------------------------------------------------------------------------
// Mapper 1: Nintendo MMC1
// ---------------------------------------------------------------------------
class CNESMapper001 : public CNESMapper
{
public:
    void InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
               CNESPPU* aPpu, CNESCPU* aCpu) override;
    void Reset() override;
    void Write(TInt aAddr, u8 aVal) override;
    void StateSave(RWriteStream& aStream) const override;
    void StateLoad(RReadStream&  aStream) override;

private:
    void SyncBanks();

    u8   iShiftReg;
    TInt iShiftCount;
    u8   iCtrlReg;
    u8   iCHRBank0;
    u8   iCHRBank1;
    u8   iPRGBank;
};

// ---------------------------------------------------------------------------
// Mapper 2: UxROM
// ---------------------------------------------------------------------------
class CNESMapper002 : public CNESMapper
{
public:
    void InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
               CNESPPU* aPpu, CNESCPU* aCpu) override;
    void Reset() override;
    void Write(TInt aAddr, u8 aVal) override;
};

// ---------------------------------------------------------------------------
// Mapper 3: CNROM
// ---------------------------------------------------------------------------
class CNESMapper003 : public CNESMapper
{
public:
    void InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
               CNESPPU* aPpu, CNESCPU* aCpu) override;
    void Reset() override;
    void Write(TInt aAddr, u8 aVal) override;
};

// ---------------------------------------------------------------------------
// Mapper 4: Nintendo MMC3
// ---------------------------------------------------------------------------
class CNESMapper004 : public CNESMapper
{
public:
    void InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
               CNESPPU* aPpu, CNESCPU* aCpu) override;
    void Reset() override;
    void Write(TInt aAddr, u8 aVal) override;
    void ClockIRQCounter() override;
    void StateSave(RWriteStream& aStream) const override;
    void StateLoad(RReadStream&  aStream) override;

private:
    void SyncBanks();

    u8   iCommand;
    u8   iCmdParam;
    u8   iRegs[8];
    TBool iPRGSwap;
    TBool iCHRSwap;
    TInt iIRQCounter;
    TInt iIRQLatch;
    TBool iIRQEnabled;
};

// ---------------------------------------------------------------------------
// Mapper 7: AxROM
// ---------------------------------------------------------------------------
class CNESMapper007 : public CNESMapper
{
public:
    void InitL(CNESROM* aRom, CNESMemory* aCpuMem, CNESMemory* aPpuMem,
               CNESPPU* aPpu, CNESCPU* aCpu) override;
    void Reset() override;
    void Write(TInt aAddr, u8 aVal) override;
};

#endif // NESMAPPER_H
