/*
 * NESPPU.cpp — Аналог PPU.java
 * Построчный рендерер видеопроцессора Ricoh 2C02.
 *
 * Упрощения относительно оригинала:
 *  - рендеринг tile-by-tile (не pixel-perfect), достаточно для корректного
 *    вывода большинства игр
 *  - sprite-0 hit определяется по x/y координатам
 */
#include "NESPPU.h"
#include "NESMapper.h"
#include "NESMemory.h"
#include <s32strm.h>

// NTSC палитра (64 цвета) — ARGB8888, такая же как в оригинале vNES
static const u32 KNTSCPalette[64] =
{
    0xFF757575,0xFF271B8F,0xFF0000AB,0xFF47009F,0xFF8F0077,0xFFAB0013,0xFFA70000,0xFF7F0B00,
    0xFF432F00,0xFF004700,0xFF005100,0xFF003F17,0xFF1B3F5F,0xFF000000,0xFF000000,0xFF000000,
    0xFFBCBCBC,0xFF0073EF,0xFF233BEF,0xFF8300F3,0xFFBF00BF,0xFFE7005B,0xFFDB2B00,0xFFCB4F0F,
    0xFF8B7300,0xFF009700,0xFF00AB00,0xFF00933B,0xFF00838B,0xFF000000,0xFF000000,0xFF000000,
    0xFFFFFFFF,0xFF3FBFFF,0xFF5F97FF,0xFFA78BFD,0xFFF77BFF,0xFFFF77B7,0xFFFF7763,0xFFFF9B3B,
    0xFFF3BF3F,0xFF83D313,0xFF4FDF4B,0xFF58F898,0xFF00EBDB,0xFF000000,0xFF000000,0xFF000000,
    0xFFFFFFFF,0xFFABE7FF,0xFFC7D7FF,0xFFD7CBFF,0xFFFFC7FF,0xFFFFC7DB,0xFFFFBFB3,0xFFFFDBAB,
    0xFFFFE7A3,0xFFE3FFA3,0xFFABF3BF,0xFFB3FFCF,0xFF9FFFF3,0xFF000000,0xFF000000,0xFF000000
};

// Длины спрайтов (lookup-таблица из APU — заимствована в PPU для LEN-счётчика)
static const u8 KSprLengthTable[32] =
{
    10,254,20, 2,40, 4,80, 6,160, 8,60,10,14,12,26,14,
    12, 16,24,18,48,20,96,22,192,24,72,26,16,28,32,30
};

// ---------------------------------------------------------------------------
CNESPPU* CNESPPU::NewLC(CNESMemory* aPpuMem, CNESMemory* aSprMem)
{
    CNESPPU* self = new (ELeave) CNESPPU();
    CleanupStack::PushL(self);
    self->ConstructL(aPpuMem, aSprMem);
    return self;
}

CNESPPU::CNESPPU()
    : iMapper(NULL), iPpuMem(NULL), iSprMem(NULL),
      iFrameReady(EFalse), iCurX(0), scanline(0), iCyclesBuf(0),
      mapperIrqCounter(0)
{}

void CNESPPU::ConstructL(CNESMemory* aPpuMem, CNESMemory* aSprMem)
{
    iPpuMem = aPpuMem;
    iSprMem = aSprMem;
    Mem::Copy(iRGBPalette, KNTSCPalette, sizeof(KNTSCPalette));
}

CNESPPU::~CNESPPU() {}

void CNESPPU::Init()
{
    Reset();
}

void CNESPPU::Reset()
{
    f_nmiOnVblank    = 0;
    f_spriteSize     = 0;
    f_bgPatternTable = 0;
    f_spPatternTable = 0;
    f_addrInc        = 0;
    f_nTblAddress    = 0;
    f_color          = 0;
    f_spVisibility   = 0;
    f_bgVisibility   = 0;
    f_spClipping     = 0;
    f_bgClipping     = 0;
    f_dispType       = 0;
    iStatusReg       = 0;
    iVramAddress     = 0;
    iVramTmpAddress  = 0;
    iVramBuffered    = 0;
    iFirstWrite      = ETrue;
    iSRAMAddr        = 0;
    cntFV = cntV = cntH = cntVT = cntHT = 0;
    regFV = regV = regH = regVT = regHT = regFH = regS = 0;
    scanline   = 0;
    iCurX      = 0;
    iCyclesBuf = 0;
    iFrameReady= EFalse;
    iHitSpr0   = EFalse;
    iSpr0HitX = iSpr0HitY = 0;

    Mem::FillZ(iNameTable,    sizeof(iNameTable));
    Mem::FillZ(iPaletteMem,   sizeof(iPaletteMem));
    Mem::FillZ(iFrameBuffer,  sizeof(iFrameBuffer));

    // Построить таблицу тайлов
    for (TInt i = 0; i < KNumTiles; i++)
    {
        Mem::FillZ(iTile[i].pix, sizeof(iTile[i].pix));
        iTile[i].dirty = ETrue;
    }

    RebuildMirrorTable(EHorizontalMirroring);
}

// ---------------------------------------------------------------------------
// Зеркалирование VRAM (аналог setMirroring из PPU.java)
// ---------------------------------------------------------------------------
void CNESPPU::SetMirroring(TMirroringType aType)
{
    RebuildMirrorTable(aType);
}

void CNESPPU::RebuildMirrorTable(TMirroringType aType)
{
    // 4 таблицы имён по 0x400 байт (итого 0x1000 адресов $2000-$2FFF)
    // Физически у нас 2 буфера: iNameTable[0] и iNameTable[1]
    // (остальные — зеркала)
    for (TInt i = 0; i < 0x1000; i++)
    {
        TInt page = i >> 10;  // 0..3
        TInt off  = i & 0x3FF;
        TInt phys;
        switch (aType)
        {
        case EHorizontalMirroring:
            // NT0=NT1, NT2=NT3
            phys = (page < 2) ? off : 0x400 + off;
            break;
        case EVerticalMirroring:
            // NT0=NT2, NT1=NT3
            phys = (page & 1) ? 0x400 + off : off;
            break;
        case ESingleScreen1:
            phys = off;
            break;
        case ESingleScreen2:
            phys = 0x400 + off;
            break;
        case EFourScreenMirroring:
        default:
            phys = i;  // нет зеркалирования (нужно 4KB VRAM)
            break;
        }
        iMirrorTable[i] = phys;
    }
}

// ---------------------------------------------------------------------------
// Доступ к PPU-памяти через таблицу зеркалирования
// ---------------------------------------------------------------------------
inline u8 CNESPPU::PPULoad(TInt aAddr)
{
    aAddr &= 0x3FFF;
    if (aAddr < 0x2000)
        return iPpuMem->Load(aAddr);
    if (aAddr < 0x3F00)
    {
        TInt phys = iMirrorTable[aAddr - 0x2000];
        return (phys < 0x800)
            ? (u8)(*(reinterpret_cast<u8*>(iNameTable) + phys))
            : 0;
    }
    // Палитра $3F00-$3FFF
    aAddr = (aAddr - 0x3F00) & 0x1F;
    if ((aAddr & 0x03) == 0) aAddr &= 0x0F; // зеркало фона
    return iPaletteMem[aAddr];
}

inline void CNESPPU::PPUWrite(TInt aAddr, u8 aVal)
{
    aAddr &= 0x3FFF;
    if (aAddr < 0x2000)
    {
        iPpuMem->Write(aAddr, aVal);
        // Пометить затронутый тайл как грязный
        TInt tileIdx = aAddr >> 4;
        if (tileIdx < KNumTiles) iTile[tileIdx].dirty = ETrue;
    }
    else if (aAddr < 0x3F00)
    {
        TInt phys = iMirrorTable[aAddr - 0x2000];
        if (phys < 0x800)
            *(reinterpret_cast<u8*>(iNameTable) + phys) = aVal;
    }
    else
    {
        aAddr = (aAddr - 0x3F00) & 0x1F;
        if ((aAddr & 0x03) == 0) aAddr &= 0x0F;
        iPaletteMem[aAddr] = aVal;
    }
}

// ---------------------------------------------------------------------------
// Чтение / запись регистров PPU (CPU адреса $2000–$2007, $4014)
// ---------------------------------------------------------------------------
u8 CNESPPU::ReadReg(TInt aAddr)
{
    u8 val = 0;
    switch (aAddr)
    {
    case 0x2002: // PPUSTATUS
        val          = (u8)(iStatusReg & 0xE0);
        iStatusReg  &= ~(1 << STATUS_VBLANK);
        iFirstWrite  = ETrue;
        break;
    case 0x2004: // OAMDATA
        val = iSprMem->Load(iSRAMAddr);
        break;
    case 0x2007: // PPUDATA
    {
        TInt va = iVramAddress & 0x3FFF;
        if (va < 0x3F00)
        {
            val           = iVramBuffered;
            iVramBuffered = PPULoad(va);
        }
        else
        {
            val           = PPULoad(va);
            iVramBuffered = PPULoad(va - 0x1000);
        }
        iVramAddress += (f_addrInc ? 32 : 1);
        break;
    }
    default: break;
    }
    return val;
}

void CNESPPU::WriteReg(TInt aAddr, u8 aVal)
{
    switch (aAddr)
    {
    case 0x2000: // PPUCTRL
        f_nmiOnVblank    = (aVal >> 7) & 1;
        f_spriteSize     = (aVal >> 5) & 1;
        f_bgPatternTable = (aVal >> 4) & 1;
        f_spPatternTable = (aVal >> 3) & 1;
        f_addrInc        = (aVal >> 2) & 1;
        f_nTblAddress    =  aVal       & 3;
        regV = (aVal >> 1) & 1;
        regH =  aVal       & 1;
        regS = (regV << 1) | regH;
        iVramTmpAddress  = (iVramTmpAddress & 0xF3FF) | ((aVal & 3) << 10);
        break;
    case 0x2001: // PPUMASK
        f_color        = (aVal >> 5) & 7;
        f_spVisibility = (aVal >> 4) & 1;
        f_bgVisibility = (aVal >> 3) & 1;
        f_spClipping   = (aVal >> 2) & 1;
        f_bgClipping   = (aVal >> 1) & 1;
        f_dispType     =  aVal       & 1;
        break;
    case 0x2003: // OAMADDR
        iSRAMAddr = aVal;
        break;
    case 0x2004: // OAMDATA
        iSprMem->Write(iSRAMAddr++, aVal);
        break;
    case 0x2005: // PPUSCROLL
        if (iFirstWrite)
        {
            regFH           = aVal & 7;
            regHT           = aVal >> 3;
            iVramTmpAddress = (iVramTmpAddress & 0xFFE0) | (aVal >> 3);
        }
        else
        {
            regFV           = aVal & 7;
            regVT           = aVal >> 3;
            iVramTmpAddress = (iVramTmpAddress & 0x8C1F)
                            | ((aVal & 7) << 12)
                            | ((aVal & 0xF8) << 2);
        }
        iFirstWrite = !iFirstWrite;
        break;
    case 0x2006: // PPUADDR
        if (iFirstWrite)
        {
            iVramTmpAddress = (iVramTmpAddress & 0x00FF) | ((aVal & 0x3F) << 8);
        }
        else
        {
            iVramTmpAddress = (iVramTmpAddress & 0xFF00) | aVal;
            iVramAddress    = iVramTmpAddress;
        }
        iFirstWrite = !iFirstWrite;
        break;
    case 0x2007: // PPUDATA
        PPUWrite(iVramAddress, aVal);
        iVramAddress += (f_addrInc ? 32 : 1);
        break;
    case 0x4014: // OAMDMA (DMA спрайтов)
    {
        TInt base = aVal << 8;
        for (TInt i = 0; i < 256; i++)
            iSprMem->Write(i, iPpuMem->Load((base + i) & 0xFFFF));
        break;
    }
    default: break;
    }
}

// ---------------------------------------------------------------------------
// RunCycles — тактирование PPU (3 PPU такта на каждый CPU такт)
// ---------------------------------------------------------------------------
void CNESPPU::RunCycles(TInt nCycles)
{
    iCyclesBuf += nCycles;

    // 1 пиксель = 1 PPU такт; строка = 341 такт; кадр = 262 строки
    while (iCyclesBuf >= 341)
    {
        iCyclesBuf -= 341;

        if (scanline < 240)
        {
            if (f_bgVisibility || f_spVisibility)
                RenderScanline(scanline);
        }
        else if (scanline == 241)
        {
            // VBlank начало
            iStatusReg |= (1 << STATUS_VBLANK);
            iFrameReady = ETrue;

            // NMI если разрешено
            // (CPU сам проверит irqRequested — мы не имеем прямой ссылки на CPU,
            //  поэтому сигнализируем через маппер или внешний вызов)
            // В NESSystem.cpp после RunFrameL() NMI уже тактируется внутри CPU
        }
        else if (scanline == 261) // pre-render
        {
            iStatusReg &= ~((1 << STATUS_VBLANK) |
                            (1 << STATUS_SPRITE0HIT) |
                            (1 << STATUS_SLSPRITECOUNT));
            iHitSpr0    = EFalse;

            // Сбросить счётчики прокрутки
            cntFV = regFV;
            cntV  = regV;
            cntH  = regH;
            cntVT = regVT;
            cntHT = regHT;
        }

        scanline++;
        if (scanline > 261) scanline = 0;
    }
}

// ---------------------------------------------------------------------------
// Вспомогательная: декодировать тайл из CHR-данных
// ---------------------------------------------------------------------------
void CNESPPU::UpdateTileCache(TInt aTileIdx)
{
    if (!iTile[aTileIdx].dirty) return;
    iTile[aTileIdx].dirty = EFalse;

    TInt base = aTileIdx * 16;
    for (TInt row = 0; row < 8; row++)
    {
        u8 lo = PPULoad(base + row);
        u8 hi = PPULoad(base + row + 8);
        for (TInt col = 0; col < 8; col++)
        {
            TInt bit = 7 - col;
            iTile[aTileIdx].pix[row][col] =
                (u8)(((lo >> bit) & 1) | (((hi >> bit) & 1) << 1));
        }
    }
}

// ---------------------------------------------------------------------------
// Рендеринг одной строки
// ---------------------------------------------------------------------------
void CNESPPU::RenderScanline(TInt aScanline)
{
    u32* row = iFrameBuffer + aScanline * KNES_WIDTH;

    // Фон
    if (f_bgVisibility)
        RenderBackground(aScanline);
    else
    {
        u32 bgColor = iRGBPalette[iPaletteMem[0] & 0x3F];
        for (TInt x = 0; x < KNES_WIDTH; x++) row[x] = bgColor;
    }

    // Спрайты за фоном
    if (f_spVisibility)
        RenderSprites(aScanline, ETrue);

    // Спрайты перед фоном
    if (f_spVisibility)
        RenderSprites(aScanline, EFalse);
}

void CNESPPU::RenderBackground(TInt aScanline)
{
    u32* dst = iFrameBuffer + aScanline * KNES_WIDTH;

    TInt ntBase     = 0x2000 + f_nTblAddress * 0x400;
    TInt patBase    = f_bgPatternTable ? 0x1000 : 0x0000;
    TInt scrollX    = regFH + regHT * 8;
    TInt scrollY    = regFV + regVT * 8 + aScanline;
    TInt tileRow    = (scrollY / 8) & 31;
    TInt fineY      = scrollY & 7;

    for (TInt px = 0; px < KNES_WIDTH; px++)
    {
        TInt sx       = (px + scrollX) & 0x1FF;
        TInt tileCol  = (sx / 8) & 31;
        TInt fineX    = sx & 7;

        // Выбрать nametable (горизонтальное зеркалирование)
        TInt nt       = ntBase;
        if (sx >= 256) nt = 0x2000 + ((f_nTblAddress ^ 1) & 3) * 0x400;

        // Номер тайла
        TInt tileIdx  = PPULoad(nt + tileRow * 32 + tileCol);

        // Атрибут (2 бита цвета)
        TInt attrOff  = 0x3C0 + (tileRow / 4) * 8 + (tileCol / 4);
        u8   attr     = PPULoad(nt + attrOff);
        TInt shift    = ((tileRow & 2) << 1) | (tileCol & 2);
        TInt palHigh  = (attr >> shift) & 3;

        // Декодировать тайл
        TInt absIdx   = (patBase >> 4) + tileIdx;
        UpdateTileCache(absIdx);
        u8 pix = iTile[absIdx].pix[fineY][fineX];

        // Цвет
        TInt palAddr = (pix == 0) ? 0 : (palHigh << 2) | pix;
        dst[px] = iRGBPalette[iPaletteMem[palAddr & 0x1F] & 0x3F];
    }
}

void CNESPPU::RenderSprites(TInt aScanline, TBool aBehindBG)
{
    u32* dst     = iFrameBuffer + aScanline * KNES_WIDTH;
    TInt sprH    = f_spriteSize ? 16 : 8;
    TInt patBase = f_spPatternTable ? 0x1000 : 0x0000;
    TInt sprCount= 0;

    // Разобрать OAM
    const u8* oam = iSprMem->Ptr();

    for (TInt i = 0; i < 64; i++)
    {
        TInt sy   = oam[i * 4 + 0] + 1; // Y+1
        TInt tile = oam[i * 4 + 1];
        u8   attr = oam[i * 4 + 2];
        TInt sx   = oam[i * 4 + 3];

        if (aScanline < sy || aScanline >= sy + sprH) continue;
        sprCount++;
        if (sprCount > 8) { iStatusReg |= (1 << STATUS_SLSPRITECOUNT); break; }

        TBool bgPri   = (attr & 0x20) != 0;
        TBool hFlip   = (attr & 0x40) != 0;
        TBool vFlip   = (attr & 0x80) != 0;
        TInt  palHigh = (attr & 3) + 4; // спрайты используют палитру 4-7

        if (bgPri != aBehindBG) continue;

        TInt row = aScanline - sy;
        if (vFlip) row = sprH - 1 - row;

        TInt absIdx;
        TInt fineY;
        if (sprH == 8)
        {
            absIdx = (patBase >> 4) + tile;
            fineY  = row;
        }
        else // 8×16
        {
            TInt bank = (tile & 1) ? 0x1000 : 0x0000;
            absIdx    = (bank >> 4) + (tile & 0xFE) + (row >= 8 ? 1 : 0);
            fineY     = row & 7;
        }
        UpdateTileCache(absIdx);

        for (TInt col = 0; col < 8; col++)
        {
            TInt px = sx + (hFlip ? 7 - col : col);
            if (px < 0 || px >= KNES_WIDTH) continue;

            u8 pix = iTile[absIdx].pix[fineY][col];
            if (pix == 0) continue; // прозрачный

            // Sprite-0 hit
            if (i == 0 && !iHitSpr0 && f_bgVisibility)
            {
                iStatusReg |= (1 << STATUS_SPRITE0HIT);
                iHitSpr0 = ETrue;
            }

            TInt palAddr  = (palHigh << 2) | pix;
            dst[px] = iRGBPalette[iPaletteMem[palAddr & 0x1F] & 0x3F];
        }
    }
}

// ---------------------------------------------------------------------------
// Сохранение / загрузка состояния
// ---------------------------------------------------------------------------
void CNESPPU::StateSave(RWriteStream& aStream) const
{
    aStream.WriteInt32L(f_nmiOnVblank);
    aStream.WriteInt32L(f_spriteSize);
    aStream.WriteInt32L(f_bgPatternTable);
    aStream.WriteInt32L(f_spPatternTable);
    aStream.WriteInt32L(f_addrInc);
    aStream.WriteInt32L(f_nTblAddress);
    aStream.WriteInt32L(iStatusReg);
    aStream.WriteInt32L(iVramAddress);
    aStream.WriteInt32L(iVramTmpAddress);
    aStream.WriteInt32L(scanline);
    aStream.WriteL(TPtrC8(iPaletteMem, sizeof(iPaletteMem)));
    aStream.WriteL(TPtrC8(reinterpret_cast<const u8*>(iNameTable), sizeof(iNameTable)));
}

void CNESPPU::StateLoad(RReadStream& aStream)
{
    f_nmiOnVblank    = aStream.ReadInt32L();
    f_spriteSize     = aStream.ReadInt32L();
    f_bgPatternTable = aStream.ReadInt32L();
    f_spPatternTable = aStream.ReadInt32L();
    f_addrInc        = aStream.ReadInt32L();
    f_nTblAddress    = aStream.ReadInt32L();
    iStatusReg       = aStream.ReadInt32L();
    iVramAddress     = aStream.ReadInt32L();
    iVramTmpAddress  = aStream.ReadInt32L();
    scanline         = aStream.ReadInt32L();

    TPtr8 palPtr(iPaletteMem, sizeof(iPaletteMem), sizeof(iPaletteMem));
    aStream.ReadL(palPtr, sizeof(iPaletteMem));

    TPtr8 ntPtr(reinterpret_cast<u8*>(iNameTable), sizeof(iNameTable), sizeof(iNameTable));
    aStream.ReadL(ntPtr, sizeof(iNameTable));

    // Все тайлы грязные после загрузки состояния
    for (TInt i = 0; i < KNumTiles; i++) iTile[i].dirty = ETrue;
}
