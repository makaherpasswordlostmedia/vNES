/*
 * NESCPU.h — Аналог CPU.java
 * Эмулятор процессора Ricoh 2A03 (6502 без десятичного режима + APU)
 *
 * Все 56 опкодов + 13 режимов адресации портированы из оригинала.
 * Вместо Java-потока используется пошаговый вызов Step() из главного цикла.
 */

#ifndef NESCPU_H
#define NESCPU_H

#include "NESTypes.h"

class CNESMapper;
class CNESPPU;
class CNESPAPU;
class RWriteStream;
class RReadStream;
class CNESMemory;

// Таблица опкодов: 256 записей по 32 бита
// Биты [7:0]   — номер инструкции (0..55)
// Биты [15:8]  — режим адресации (0..12)
// Биты [23:16] — длина инструкции в байтах (1..3)
// Биты [31:24] — базовое число тактов
extern const TUint32 KOpData[256];

class CNESCPU : public CBase
{
public:
    static CNESCPU* NewLC(CNESMemory* aCpuMem,
                          CNESPPU*    aPpu,
                          CNESPAPU*   aPapu);
    ~CNESCPU();

    void Init();
    void Reset();

    // Выполнить один опкод. Возвращает число затраченных тактов.
    TInt Step();

    // Запросить прерывание
    void TriggerIRQ(TIrqType aType);

    // Сохранение / загрузка состояния
    void StateSave(RWriteStream& aStream) const;
    void StateLoad(RReadStream&  aStream);

    // Установить маппер (после загрузки ROM)
    void SetMapper(CNESMapper* aMapper) { iMapper = aMapper; }

    // Публичные регистры (нужны PPU/PAPU для отладки)
    TInt REG_ACC;
    TInt REG_X;
    TInt REG_Y;
    TInt REG_SP;
    TInt REG_PC;

    // Флаги (хранятся раздельно для скорости, как в оригинале)
    TInt F_CARRY;
    TInt F_ZERO;       // 0 = флаг Zero установлен (инвертировано!)
    TInt F_INTERRUPT;
    TInt F_DECIMAL;
    TInt F_BRK;
    TInt F_NOTUSED;
    TInt F_OVERFLOW;
    TInt F_SIGN;

    TInt cyclesToHalt;
    TBool crash;
    TBool irqRequested;
    TIrqType irqType;

private:
    CNESCPU();
    void ConstructL(CNESMemory* aCpuMem, CNESPPU* aPpu, CNESPAPU* aPapu);

    // Упакованный/распакованный статус
    TInt  GetStatus() const;
    void  SetStatus(TInt aStatus);

    // Чтение памяти через маппер
    inline TInt  Load(TInt aAddr);
    inline TInt  Load16bit(TInt aAddr);
    inline void  Write(TInt aAddr, u8 aVal);

    // Стек
    inline void Push(u8 aVal);
    inline u8   Pop();

    // Обработчики прерываний
    void DoIRQ(TInt aStatus);
    void DoNMI(TInt aStatus);
    void DoResetInterrupt();

    // Адресация (вычисляет addr из opaddr и режима)
    TInt ResolveAddr(TInt aAddrMode, TInt aOpAddr, TInt& aCycleAdd);

    CNESMapper* iMapper;
    CNESPPU*    iPpu;
    CNESPAPU*   iPapu;
    u8*         iMem;      // прямой указатель на CPU-память (быстрый доступ)
};

// ---------------------------------------------------------------------------
// Inline: чтение байта из CPU-адресного пространства
// ---------------------------------------------------------------------------
inline TInt CNESCPU::Load(TInt aAddr)
{
    if (aAddr < 0x2000)
        return iMem[aAddr & 0x7FF];        // зеркало внутр. RAM 2KB
    return iMapper->Load(aAddr);
}

inline TInt CNESCPU::Load16bit(TInt aAddr)
{
    return Load(aAddr) | (Load(aAddr + 1) << 8);
}

inline void CNESCPU::Write(TInt aAddr, u8 aVal)
{
    if (aAddr < 0x2000)
        iMem[aAddr & 0x7FF] = aVal;
    else
        iMapper->Write(aAddr, aVal);
}

inline void CNESCPU::Push(u8 aVal)
{
    iMem[REG_SP] = aVal;
    REG_SP = ((REG_SP - 1) & 0xFF) | 0x100;
}

inline u8 CNESCPU::Pop()
{
    REG_SP = ((REG_SP + 1) & 0xFF) | 0x100;
    return iMem[REG_SP];
}

#endif // NESCPU_H
