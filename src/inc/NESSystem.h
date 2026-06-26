/*
 * NESSystem.h — Аналог NES.java
 * Объединяет CPU, PPU, PAPU, Memory, ROM и Mapper в единую систему.
 */

#ifndef NESSYSTEM_H
#define NESSYSTEM_H

#include "NESTypes.h"

class CNESCPU;
class CNESPPU;
class CNESPAPU;
class CNESMemory;
class CNESMapper;
class CNESROM;

class CNESSystem : public CBase
{
public:
    static CNESSystem* NewLC();
    ~CNESSystem();

    // Загрузить ROM-файл (iNES .nes)
    TBool LoadRomL(const TDesC& aFileName);

    // Управление эмуляцией
    void StartEmulation();
    void StopEmulation();
    TBool IsRunning() const { return iRunning; }

    // Сброс системы
    void Reset();

    // Главный цикл: выполнить один кадр (вызывается из таймера)
    // Возвращает ETrue если кадровый буфер обновлён
    TBool RunFrameL();

    // Доступ к кадровому буферу PPU (256×240 ARGB8888)
    const u32* FrameBuffer() const;

    // Входные данные джойстика (биты: A B Select Start Up Down Left Right)
    void SetJoyState(TInt aPlayer, u8 aButtons);

    // Сохранение / загрузка состояния (slot 0..9)
    void StateSaveL(TInt aSlot) const;
    void StateLoadL(TInt aSlot);

    // Компоненты системы (нужны маперам и UI)
    CNESCPU*    Cpu()    const { return iCpu;    }
    CNESPPU*    Ppu()    const { return iPpu;    }
    CNESPAPU*   Papu()   const { return iPapu;   }
    CNESMemory* CpuMem() const { return iCpuMem; }
    CNESMemory* PpuMem() const { return iPpuMem; }
    CNESMemory* SprMem() const { return iSprMem; }
    CNESROM*    Rom()    const { return iRom;    }
    CNESMapper* Mapper() const { return iMapper; }

private:
    CNESSystem();
    void ConstructL();
    void ClearCPUMemory();
    TFileName MakeSaveStatePath(TInt aSlot) const;

    CNESCPU*    iCpu;
    CNESPPU*    iPpu;
    CNESPAPU*   iPapu;
    CNESMemory* iCpuMem;
    CNESMemory* iPpuMem;
    CNESMemory* iSprMem;
    CNESROM*    iRom;
    CNESMapper* iMapper;

    TBool    iRunning;
    TFileName iRomPath;

    // Джойстики (2 портa)
    u8   iJoyState[2];    // текущее состояние кнопок
    u8   iJoyShift[2];    // сдвиговый регистр для последовательного чтения
    TBool iJoyStrobe;
};

#endif // NESSYSTEM_H
