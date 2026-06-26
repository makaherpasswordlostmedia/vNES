/*
 * NESMemory.cpp — Аналог Memory.java
 */
#include "NESMemory.h"
#include <s32strm.h>
#include <e32base.h>

CNESMemory* CNESMemory::NewLC(TInt aSize)
{
    CNESMemory* self = new (ELeave) CNESMemory();
    CleanupStack::PushL(self);
    self->ConstructL(aSize);
    return self;
}

CNESMemory::CNESMemory() : iMem(NULL), iSize(0) {}

void CNESMemory::ConstructL(TInt aSize)
{
    iSize = aSize;
    iMem  = new (ELeave) u8[aSize];
    Mem::FillZ(iMem, aSize);
}

CNESMemory::~CNESMemory()
{
    delete[] iMem;
}

void CNESMemory::Reset()
{
    Mem::FillZ(iMem, iSize);
}

void CNESMemory::Write(TInt aAddr, const u8* aSrc, TInt aLen)
{
    if (aAddr + aLen > iSize) aLen = iSize - aAddr;
    Mem::Copy(iMem + aAddr, aSrc, aLen);
}

void CNESMemory::StateSave(RWriteStream& aStream) const
{
    aStream.WriteL(TPtrC8(iMem, iSize));
}

void CNESMemory::StateLoad(RReadStream& aStream)
{
    TPtr8 ptr(iMem, iSize, iSize);
    aStream.ReadL(ptr, iSize);
}
