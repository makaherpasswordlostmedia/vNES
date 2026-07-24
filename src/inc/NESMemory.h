/*
 * NESMemory.h — Аналог Memory.java
 * Линейный массив байт с методами чтения/записи.
 */

#ifndef NESMEMORY_H
#define NESMEMORY_H

#include "NESTypes.h"

class RWriteStream;
class RReadStream;

class CNESMemory : public CBase
{
public:
    static CNESMemory* NewLC(TInt aSize);
    ~CNESMemory();

    inline u8   Load(TInt aAddr) const   { return iMem[aAddr & (iSize-1)]; }
    inline void Write(TInt aAddr, u8 aVal){ iMem[aAddr & (iSize-1)] = aVal; }

    void Reset();
    void Write(TInt aAddr, const u8* aSrc, TInt aLen);

    // Сохранение / загрузка состояния
    void StateSave(RWriteStream& aStream) const;
    void StateLoad(RReadStream&  aStream);

    TInt Size() const { return iSize; }

    // Прямой доступ к буферу (для быстрого чтения из CPU)
    u8* Ptr() { return iMem; }
    const u8* Ptr() const { return iMem; }

private:
    CNESMemory();
    void ConstructL(TInt aSize);

    u8*  iMem;
    TInt iSize;
};

#endif // NESMEMORY_H
