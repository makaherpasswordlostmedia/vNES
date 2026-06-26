/*
 * NESROM.h — Аналог ROM.java
 * Загружает iNES-образ (.nes), разбивает на PRG-ROM и CHR-ROM банки.
 */

#ifndef NESROM_H
#define NESROM_H

#include "NESTypes.h"

// Максимальное количество банков
const TInt KMaxPRGBanks = 64;   // 64 * 16KB = 1 MB PRG
const TInt KMaxCHRBanks = 64;   // 64 * 8KB  = 512 KB CHR

const TInt KPRGBankSize = 0x4000;  // 16 KB
const TInt KCHRBankSize = 0x2000;  // 8 KB

class CNESROM : public CBase
{
public:
    static CNESROM* NewLC();
    ~CNESROM();

    // Загрузить .nes файл
    void LoadL(const TDesC& aFileName);

    TBool IsValid() const { return iValid; }

    // Доступ к PRG-ROM
    u8*  PRGBank(TInt aBank) const { return iPRGRom[aBank]; }
    TInt PRGBankCount() const      { return iPRGCount; }

    // Доступ к CHR-ROM
    u8*  CHRBank(TInt aBank) const { return iCHRRom[aBank]; }
    TInt CHRBankCount() const      { return iCHRCount; }

    // Метаданные
    TInt          MapperType()    const { return iMapperType; }
    TMirroringType MirroringType() const { return iMirroring; }
    TBool         HasBatteryRAM() const { return iBatteryRam; }
    TBool         HasTrainer()    const { return iTrainer; }
    TBool         FourScreen()    const { return iFourScreen; }

    // Save RAM (SRAM / battery-backed RAM)
    u8*  SaveRAM() { return iSaveRAM; }

    // Загрузить / сохранить SRAM на диск
    void LoadSRAML(const TDesC& aPath);
    void SaveSRAML(const TDesC& aPath) const;

private:
    CNESROM();
    void ConstructL();
    void ParseHeaderL(const u8* aHdr);

    TBool         iValid;
    TInt          iPRGCount;
    TInt          iCHRCount;
    TInt          iMapperType;
    TMirroringType iMirroring;
    TBool         iBatteryRam;
    TBool         iTrainer;
    TBool         iFourScreen;

    u8*  iPRGRom[KMaxPRGBanks];
    u8*  iCHRRom[KMaxCHRBanks];
    u8*  iSaveRAM;   // 8 KB battery-backed SRAM
    u8*  iRawData;   // весь файл в памяти
    TInt iRawSize;
};

#endif // NESROM_H
