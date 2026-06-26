/*
 * NESROM.cpp — Аналог ROM.java
 * Разбор заголовка iNES v1, загрузка PRG/CHR банков.
 */
#include "NESROM.h"
#include <f32file.h>
#include <s32file.h>
#include <e32base.h>

CNESROM* CNESROM::NewLC()
{
    CNESROM* self = new (ELeave) CNESROM();
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}

CNESROM::CNESROM()
    : iValid(EFalse), iPRGCount(0), iCHRCount(0), iMapperType(0),
      iMirroring(EHorizontalMirroring), iBatteryRam(EFalse),
      iTrainer(EFalse), iFourScreen(EFalse),
      iSaveRAM(NULL), iRawData(NULL), iRawSize(0)
{
    Mem::FillZ(iPRGRom, sizeof(iPRGRom));
    Mem::FillZ(iCHRRom, sizeof(iCHRRom));
}

void CNESROM::ConstructL()
{
    iSaveRAM = new (ELeave) u8[KSRAM_SIZE];
    Mem::FillZ(iSaveRAM, KSRAM_SIZE);
}

CNESROM::~CNESROM()
{
    delete[] iRawData;
    delete[] iSaveRAM;
}

// ---------------------------------------------------------------------------
// LoadL: считать весь файл в память и разобрать
// ---------------------------------------------------------------------------
void CNESROM::LoadL(const TDesC& aFileName)
{
    iValid = EFalse;

    RFs   fs;
    User::LeaveIfError(fs.Connect());
    CleanupClosePushL(fs);

    RFile file;
    User::LeaveIfError(file.Open(fs, aFileName, EFileRead | EFileShareReadersOnly));
    CleanupClosePushL(file);

    TInt size = 0;
    User::LeaveIfError(file.Size(size));
    if (size < 16) User::Leave(KErrCorrupt);

    delete[] iRawData;
    iRawData = new (ELeave) u8[size];
    iRawSize = size;

    TPtr8 ptr(iRawData, size, size);
    User::LeaveIfError(file.Read(ptr, size));

    CleanupStack::PopAndDestroy(2); // file, fs

    // Проверка сигнатуры "NES\x1A"
    if (iRawData[0] != 0x4E || iRawData[1] != 0x45 ||
        iRawData[2] != 0x53 || iRawData[3] != 0x1A)
    {
        User::Leave(KErrCorrupt);
    }

    ParseHeaderL(iRawData);
}

void CNESROM::ParseHeaderL(const u8* aHdr)
{
    iPRGCount   = aHdr[4];   // количество PRG-ROM банков по 16 KB
    iCHRCount   = aHdr[5];   // количество CHR-ROM банков по 8 KB

    u8 flags6   = aHdr[6];
    u8 flags7   = aHdr[7];

    iBatteryRam = (flags6 & 0x02) != 0;
    iTrainer    = (flags6 & 0x04) != 0;
    iFourScreen = (flags6 & 0x08) != 0;

    if (iFourScreen)
        iMirroring = EFourScreenMirroring;
    else if (flags6 & 0x01)
        iMirroring = EVerticalMirroring;
    else
        iMirroring = EHorizontalMirroring;

    iMapperType = ((flags6 >> 4) & 0x0F) | (flags7 & 0xF0);

    // Смещение данных в файле
    TInt offset = 16;
    if (iTrainer) offset += 512;

    // Подготовить указатели на PRG-банки
    if (iPRGCount > KMaxPRGBanks) User::Leave(KErrCorrupt);
    for (TInt i = 0; i < iPRGCount; i++)
    {
        iPRGRom[i] = iRawData + offset;
        offset += KPRGBankSize;
        if (offset > iRawSize) User::Leave(KErrCorrupt);
    }

    // Подготовить указатели на CHR-банки
    if (iCHRCount > KMaxCHRBanks) User::Leave(KErrCorrupt);
    for (TInt i = 0; i < iCHRCount; i++)
    {
        iCHRRom[i] = iRawData + offset;
        offset += KCHRBankSize;
        if (offset > iRawSize) User::Leave(KErrCorrupt);
    }

    iValid = ETrue;
}

// ---------------------------------------------------------------------------
// Save / Load SRAM
// ---------------------------------------------------------------------------
void CNESROM::LoadSRAML(const TDesC& aPath)
{
    RFs fs;
    if (fs.Connect() != KErrNone) return;
    CleanupClosePushL(fs);

    RFile file;
    if (file.Open(fs, aPath, EFileRead) == KErrNone)
    {
        TPtr8 ptr(iSaveRAM, KSRAM_SIZE, KSRAM_SIZE);
        file.Read(ptr, KSRAM_SIZE);
        file.Close();
    }
    CleanupStack::PopAndDestroy();
}

void CNESROM::SaveSRAML(const TDesC& aPath) const
{
    RFs fs;
    if (fs.Connect() != KErrNone) return;
    CleanupClosePushL(fs);

    RFile file;
    if (file.Replace(fs, aPath, EFileWrite) == KErrNone)
    {
        TPtrC8 ptr(iSaveRAM, KSRAM_SIZE);
        file.Write(ptr);
        file.Close();
    }
    CleanupStack::PopAndDestroy();
}
