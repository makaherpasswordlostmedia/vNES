/*
 * NESPPU.h — Аналог PPU.java
 * Эмулятор видеопроцессора Ricoh 2C02.
 * Рендеринг построчный (scanline-based), как в оригинале.
 */

#ifndef NESPPU_H
#define NESPPU_H

#include "NESTypes.h"

class CNESMapper;
class CNESMemory;
class CNESCPU;
class RWriteStream;
class RReadStream;

// Число тайлов в паттерн-таблицах (2 таблицы × 256 тайлов)
const TInt KNumTiles = 512;

// Кэш одного тайла 8×8 (пиксели как индексы палитры 0..3)
struct TTile
{
    u8 pix[8][8];
    // Кэшированные строки с учётом палитры (для скорости рендеринга)
    u32 lineCache[8];      // ARGB8888 уже смешанный с палитрой
    TBool dirty;           // нужна перерисовка кэша
};

class CNESPPU : public CBase
{
public:
    static CNESPPU* NewLC(CNESMemory* aPpuMem, CNESMemory* aSprMem);
    ~CNESPPU();

    void Init();
    void Reset();

    // Вызывается из CPU: выполнить nCycles тактов PPU (3 PPU такта / CPU такт)
    void RunCycles(TInt nCycles);

    // Чтение / запись регистров PPU (адреса 0x2000–0x2007 и 0x4014)
    u8   ReadReg(TInt aAddr);
    void WriteReg(TInt aAddr, u8 aVal);

    // Обновить таблицу зеркалирования
    void SetMirroring(TMirroringType aType);

    // Получить указатель на готовый кадровый буфер ARGB8888 256×240
    const u32* FrameBuffer() const { return iFrameBuffer; }
    TBool FrameReady() const       { return iFrameReady; }
    void  ClearFrameReady()        { iFrameReady = EFalse; }

    // Установить маппер (для IRQ-счётчика, VROM-доступа)
    void SetMapper(CNESMapper* aMapper) { iMapper = aMapper; }

    // Сохранение / загрузка состояния
    void StateSave(RWriteStream& aStream) const;
    void StateLoad(RReadStream&  aStream);

    // Публичные счётчики (нужны маппера типа MMC3)
    TInt scanline;
    TInt mapperIrqCounter;

    // Палитра NTSC (загружается из данных)
    u32 iRGBPalette[64];   // ARGB8888

private:
    CNESPPU();
    void ConstructL(CNESMemory* aPpuMem, CNESMemory* aSprMem);

    void RenderScanline(TInt aScanline);
    void RenderBackground(TInt aScanline);
    void RenderSprites(TInt aScanline, TBool aBehindBG);
    void UpdateTileCache(TInt aTileIdx);
    void RebuildMirrorTable(TMirroringType aType);

    inline u8   PPULoad(TInt aAddr);
    inline void PPUWrite(TInt aAddr, u8 aVal);

    // Контрольные регистры
    TInt f_nmiOnVblank;
    TInt f_spriteSize;        // 0=8×8, 1=8×16
    TInt f_bgPatternTable;
    TInt f_spPatternTable;
    TInt f_addrInc;
    TInt f_nTblAddress;
    TInt f_color;
    TInt f_spVisibility;
    TInt f_bgVisibility;
    TInt f_spClipping;
    TInt f_bgClipping;
    TInt f_dispType;

    // Статусные биты
    TInt iStatusReg;

    // VRAM
    TInt  iVramAddress;
    TInt  iVramTmpAddress;
    u8    iVramBuffered;
    TBool iFirstWrite;        // Hi/Lo latch

    // Счётчики прокрутки (внутренние регистры PPU)
    TInt cntFV, cntV, cntH, cntVT, cntHT;
    TInt regFV, regV,  regH, regVT, regHT, regFH, regS;

    // OAM (Object Attribute Memory = Sprite RAM)
    u8    iSRAMAddr;

    // Тайтлы паттерн-таблиц
    TTile iTile[KNumTiles];

    // Таблица зеркалирования VRAM (1024 записи → физический адрес)
    TInt  iMirrorTable[0x1000];

    // Name table данные (4 × 1024)
    u8    iNameTable[4][0x400];

    // Палитра (32 байта PPU палитры)
    u8    iPaletteMem[0x20];

    // Спрайты (разобранные)
    struct TSprite
    {
        TInt x, y, tile, col;
        TBool vertFlip, horiFlip, bgPriority;
    } iSprite[64];

    // Кадровый буфер ARGB8888
    u32   iFrameBuffer[KNES_WIDTH * KNES_HEIGHT];
    TBool iFrameReady;

    // Текущая позиция луча
    TInt  iCurX;
    TInt  iCyclesBuf;    // накопленные PPU-такты

    // Sprite-0 hit
    TInt  iSpr0HitX, iSpr0HitY;
    TBool iHitSpr0;

    CNESMapper* iMapper;
    CNESMemory* iPpuMem;
    CNESMemory* iSprMem;
};

#endif // NESPPU_H
