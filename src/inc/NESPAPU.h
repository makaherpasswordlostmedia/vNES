/*
 * NESPAPU.h — Аналог PAPU.java
 * Эмулятор аудиопроцессора (pseudo-APU) Ricoh 2A03.
 * На Qt/Symbian^3 используется QAudioOutput для вывода звука.
 *
 * Каналы: 2 × Square, 1 × Triangle, 1 × Noise, 1 × DMC
 */

#ifndef NESPAPU_H
#define NESPAPU_H

#include "NESTypes.h"
#include <QAudioOutput>
#include <QIODevice>
#include <QObject>

class CNESCPU;

// Размер аудио-буфера (в сэмплах)
const TInt KAudioBufSamples = 2048;
// Частота дискретизации
const TInt KSampleRate = 44100;

// ---------------------------------------------------------------------------
// Базовый канал APU
// ---------------------------------------------------------------------------
class CNESAPUChannel : public CBase
{
public:
    virtual ~CNESAPUChannel() {}
    virtual void Reset()                    = 0;
    virtual void WriteReg(TInt aReg, u8 aVal) = 0;
    // Генерировать 1 выходной сэмпл (возвращает значение -1.0 … 1.0)
    virtual TReal32 Sample()               = 0;
    virtual void ClockLength()             {}
    virtual void ClockEnvelope()           {}
    virtual void ClockSweep()              {}
    virtual void ClockLinear()             {}
};

// ---------------------------------------------------------------------------
// Square channel (pulse)
// ---------------------------------------------------------------------------
class CNESSquareChannel : public CNESAPUChannel
{
public:
    CNESSquareChannel(TBool aIsChannel2);
    void Reset() override;
    void WriteReg(TInt aReg, u8 aVal) override;
    TReal32 Sample() override;
    void ClockLength()   override;
    void ClockEnvelope() override;
    void ClockSweep()    override;

private:
    TBool  iIsChannel2;
    u8     iDuty;
    TBool  iLengthHalt;
    TBool  iEnvelopeLoop;
    TBool  iEnvelopeDisable;
    TInt   iVolume;
    TBool  iSweepEnabled;
    TInt   iSweepPeriod;
    TBool  iSweepNegate;
    TInt   iSweepShift;
    TInt   iTimerPeriod;
    TInt   iTimerValue;
    TInt   iDutyValue;
    TInt   iLengthValue;
    TInt   iEnvelopeValue;
    TInt   iEnvelopePeriod;
    TInt   iConstantVolume;
    TInt   iSweepValue;
};

// ---------------------------------------------------------------------------
// Triangle channel
// ---------------------------------------------------------------------------
class CNESTriangleChannel : public CNESAPUChannel
{
public:
    void Reset() override;
    void WriteReg(TInt aReg, u8 aVal) override;
    TReal32 Sample() override;
    void ClockLength() override;
    void ClockLinear() override;

private:
    TBool  iLengthHalt;
    TBool  iCounterReload;
    TInt   iLinearPeriod;
    TInt   iLinearValue;
    TInt   iTimerPeriod;
    TInt   iTimerValue;
    TInt   iDutyValue;
    TInt   iLengthValue;
};

// ---------------------------------------------------------------------------
// Noise channel
// ---------------------------------------------------------------------------
class CNESNoiseChannel : public CNESAPUChannel
{
public:
    void Reset() override;
    void WriteReg(TInt aReg, u8 aVal) override;
    TReal32 Sample() override;
    void ClockLength()   override;
    void ClockEnvelope() override;

private:
    TBool  iMode;
    TInt   iTimerPeriod;
    TInt   iTimerValue;
    u16    iShiftReg;
    TBool  iLengthHalt;
    TBool  iEnvelopeLoop;
    TBool  iEnvelopeDisable;
    TInt   iVolume;
    TInt   iEnvelopeValue;
    TInt   iEnvelopePeriod;
    TInt   iConstantVolume;
    TInt   iLengthValue;
};

// ---------------------------------------------------------------------------
// DMC channel (delta modulation)
// ---------------------------------------------------------------------------
class CNESDMCChannel : public CNESAPUChannel
{
public:
    void SetCPU(CNESCPU* aCpu) { iCpu = aCpu; }
    void Reset() override;
    void WriteReg(TInt aReg, u8 aVal) override;
    TReal32 Sample() override;

private:
    CNESCPU* iCpu;
    TBool    iIRQEnabled;
    TBool    iLoop;
    TInt     iRateIndex;
    TInt     iOutputLevel;
    TInt     iSampleAddr;
    TInt     iSampleLen;
    TInt     iCurrentAddr;
    TInt     iCurrentLen;
    u8       iShiftReg;
    TInt     iBitCount;
    TInt     iTickPeriod;
    TInt     iTickValue;
    TBool    iSilenceFlag;
};

// ---------------------------------------------------------------------------
// Главный класс APU
// ---------------------------------------------------------------------------
class CNESPAPU : public QObject, public CBase
{
public:
    static CNESPAPU* NewLC(CNESCPU* aCpu);
    ~CNESPAPU();

    void Reset();
    void Start();
    void Stop();
    TBool IsRunning() const { return iRunning; }

    // Запись в регистры APU (адреса 0x4000–0x4017)
    void WriteReg(TInt aAddr, u8 aVal);

    // Тактирование: вызывается из CPU каждый CPU-цикл
    void Clock();

    // Установить частоту дискретизации
    void SetSampleRate(TInt aRate);
    TInt SampleRate() const { return iSampleRate; }

    // Qt audio
    void initAudio();

    void StateSave(RWriteStream& aStream) const;
    void StateLoad(RReadStream&  aStream);

private:
    CNESPAPU();
    void ConstructL(CNESCPU* aCpu);
    void FlushBuffer();
    void MixSample();

    CNESCPU*           iCpu;
    CNESSquareChannel* iSq1;
    CNESSquareChannel* iSq2;
    CNESTriangleChannel* iTri;
    CNESNoiseChannel*  iNoise;
    CNESDMCChannel*    iDMC;

    TBool  iRunning;
    TInt   iSampleRate;
    TInt   iCyclesPerSample;
    TInt   iCycleAcc;

    TInt   iFrameCounter;
    TBool  i5StepMode;
    TBool  iIRQInhibit;

    s16    iAudioBuf[2][KAudioBufSamples];
    TInt   iBufWrite;
    TInt   iBufPos;

    QAudioOutput* iAudioOutput;
    QIODevice*    iAudioDevice;
};

#endif // NESPAPU_H
