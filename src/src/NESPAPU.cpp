/*
 * NESPAPU.cpp — Аналог PAPU.java
 * APU: Square×2, Triangle, Noise, DMC + frame sequencer.
 * Вывод звука через QAudioOutput (Qt Multimedia).
 */
#include "NESPAPU.h"
#include "NESCPU.h"
#include <QAudioFormat>
#include <e32math.h>

// Таблицы периодов каналов (NTSC)
static const TInt KNoisePeriodTable[16] =
{ 4,8,16,32,64,96,128,160,202,254,380,508,762,1016,2034,4068 };

static const TInt KDMCRateTable[16] =
{ 428,380,340,320,286,254,226,214,190,160,142,128,106,84,72,54 };

static const u8 KDutyTable[4][8] =
{
    {0,1,0,0,0,0,0,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,0,0,0},
    {1,0,0,1,1,1,1,1}
};

static const u8 KLengthTable[32] =
{ 10,254,20,2,40,4,80,6,160,8,60,10,14,12,26,14,
  12,16,24,18,48,20,96,22,192,24,72,26,16,28,32,30 };

static const TInt KTriangleTable[32] =
{ 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,
   0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 };

// ---------------------------------------------------------------------------
// CNESSquareChannel
// ---------------------------------------------------------------------------
CNESSquareChannel::CNESSquareChannel(TBool aIsChannel2)
    : iIsChannel2(aIsChannel2), iDuty(0), iLengthHalt(EFalse),
      iEnvelopeLoop(EFalse), iEnvelopeDisable(EFalse), iVolume(0),
      iSweepEnabled(EFalse), iSweepPeriod(0), iSweepNegate(EFalse),
      iSweepShift(0), iTimerPeriod(0), iTimerValue(0), iDutyValue(0),
      iLengthValue(0), iEnvelopeValue(0), iEnvelopePeriod(0),
      iConstantVolume(0), iSweepValue(0)
{}

void CNESSquareChannel::Reset()
{
    iDuty = iLengthValue = iTimerValue = iDutyValue = 0;
    iTimerPeriod = iEnvelopePeriod = iConstantVolume = 0;
    iSweepEnabled = iLengthHalt = iEnvelopeLoop = iEnvelopeDisable = EFalse;
}

void CNESSquareChannel::WriteReg(TInt aReg, u8 aVal)
{
    switch (aReg & 3)
    {
    case 0:
        iDuty            = (aVal >> 6) & 3;
        iLengthHalt      = (aVal & 0x20) != 0;
        iEnvelopeLoop    = iLengthHalt;
        iEnvelopeDisable = (aVal & 0x10) != 0;
        iVolume          = aVal & 0x0F;
        iConstantVolume  = iVolume;
        break;
    case 1:
        iSweepEnabled = (aVal & 0x80) != 0;
        iSweepPeriod  = ((aVal >> 4) & 7) + 1;
        iSweepNegate  = (aVal & 0x08) != 0;
        iSweepShift   = aVal & 7;
        iSweepValue   = 0;
        break;
    case 2:
        iTimerPeriod = (iTimerPeriod & 0xFF00) | aVal;
        break;
    case 3:
        iLengthValue = KLengthTable[(aVal >> 3) & 31];
        iTimerPeriod = (iTimerPeriod & 0x00FF) | ((aVal & 7) << 8);
        iEnvelopeValue = 15;
        iDutyValue = 0;
        break;
    }
}

TReal32 CNESSquareChannel::Sample()
{
    if (iLengthValue == 0 || iTimerPeriod < 8 || iTimerPeriod > 0x7FF)
        return 0.0f;
    if (KDutyTable[iDuty][iDutyValue] == 0) return 0.0f;

    TReal32 vol = (TReal32)(iEnvelopeDisable ? iConstantVolume : iEnvelopeValue);

    iTimerValue--;
    if (iTimerValue <= 0)
    {
        iTimerValue = (iTimerPeriod + 1) * 2;
        iDutyValue  = (iDutyValue + 1) & 7;
    }
    return vol / 15.0f;
}

void CNESSquareChannel::ClockLength()
{
    if (!iLengthHalt && iLengthValue > 0) iLengthValue--;
}

void CNESSquareChannel::ClockEnvelope()
{
    if (iEnvelopeValue > 0)
        iEnvelopeValue--;
    else
    {
        if (iEnvelopeLoop) iEnvelopeValue = 15;
        iEnvelopePeriod = iVolume;
    }
}

void CNESSquareChannel::ClockSweep()
{
    if (!iSweepEnabled || iSweepShift == 0 || iLengthValue == 0) return;
    iSweepValue--;
    if (iSweepValue > 0) return;
    iSweepValue = iSweepPeriod;

    TInt delta = iTimerPeriod >> iSweepShift;
    iTimerPeriod += iSweepNegate ? -delta - (iIsChannel2 ? 0 : 1) : delta;
}

// ---------------------------------------------------------------------------
// CNESTriangleChannel
// ---------------------------------------------------------------------------
void CNESTriangleChannel::Reset()
{
    iLengthHalt = iCounterReload = EFalse;
    iLinearPeriod = iLinearValue = 0;
    iTimerPeriod  = iTimerValue  = 0;
    iDutyValue    = iLengthValue = 0;
}

void CNESTriangleChannel::WriteReg(TInt aReg, u8 aVal)
{
    switch (aReg & 3)
    {
    case 0:
        iLengthHalt   = (aVal & 0x80) != 0;
        iLinearPeriod = aVal & 0x7F;
        break;
    case 2:
        iTimerPeriod  = (iTimerPeriod & 0xFF00) | aVal;
        break;
    case 3:
        iLengthValue  = KLengthTable[(aVal >> 3) & 31];
        iTimerPeriod  = (iTimerPeriod & 0x00FF) | ((aVal & 7) << 8);
        iCounterReload= ETrue;
        break;
    }
}

TReal32 CNESTriangleChannel::Sample()
{
    if (iLengthValue == 0 || iLinearValue == 0) return 0.0f;
    TReal32 s = (TReal32)KTriangleTable[iDutyValue] / 15.0f;

    iTimerValue--;
    if (iTimerValue <= 0)
    {
        iTimerValue = iTimerPeriod + 1;
        iDutyValue  = (iDutyValue + 1) & 31;
    }
    return s;
}

void CNESTriangleChannel::ClockLength()
{
    if (!iLengthHalt && iLengthValue > 0) iLengthValue--;
}

void CNESTriangleChannel::ClockLinear()
{
    if (iCounterReload)
        iLinearValue = iLinearPeriod;
    else if (iLinearValue > 0)
        iLinearValue--;
    if (!iLengthHalt) iCounterReload = EFalse;
}

// ---------------------------------------------------------------------------
// CNESNoiseChannel
// ---------------------------------------------------------------------------
void CNESNoiseChannel::Reset()
{
    iMode = EFalse; iShiftReg = 1;
    iTimerPeriod = iTimerValue = 0;
    iLengthHalt = iEnvelopeLoop = iEnvelopeDisable = EFalse;
    iVolume = iEnvelopeValue = iEnvelopePeriod = iConstantVolume = 0;
    iLengthValue = 0;
}

void CNESNoiseChannel::WriteReg(TInt aReg, u8 aVal)
{
    switch (aReg & 3)
    {
    case 0:
        iLengthHalt      = (aVal & 0x20) != 0;
        iEnvelopeLoop    = iLengthHalt;
        iEnvelopeDisable = (aVal & 0x10) != 0;
        iVolume          = aVal & 0x0F;
        iConstantVolume  = iVolume;
        break;
    case 2:
        iMode        = (aVal & 0x80) != 0;
        iTimerPeriod = KNoisePeriodTable[aVal & 0x0F];
        break;
    case 3:
        iLengthValue   = KLengthTable[(aVal >> 3) & 31];
        iEnvelopeValue = 15;
        break;
    }
}

TReal32 CNESNoiseChannel::Sample()
{
    if (iLengthValue == 0 || (iShiftReg & 1)) return 0.0f;
    TReal32 vol = (TReal32)(iEnvelopeDisable ? iConstantVolume : iEnvelopeValue);

    iTimerValue--;
    if (iTimerValue <= 0)
    {
        iTimerValue = iTimerPeriod;
        TInt bit    = iMode ? ((iShiftReg >> 6) & 1) : ((iShiftReg >> 1) & 1);
        TInt fb     = (iShiftReg & 1) ^ bit;
        iShiftReg   = (iShiftReg >> 1) | (fb << 14);
    }
    return vol / 15.0f;
}

void CNESNoiseChannel::ClockLength()
{
    if (!iLengthHalt && iLengthValue > 0) iLengthValue--;
}

void CNESNoiseChannel::ClockEnvelope()
{
    if (iEnvelopeValue > 0) iEnvelopeValue--;
    else { if (iEnvelopeLoop) iEnvelopeValue = 15; }
}

// ---------------------------------------------------------------------------
// CNESDMCChannel (упрощённая реализация)
// ---------------------------------------------------------------------------
void CNESDMCChannel::Reset()
{
    iOutputLevel = 0; iCurrentLen = 0; iBitCount = 0;
    iTickPeriod = KDMCRateTable[0]; iTickValue = 0; iSilenceFlag = ETrue;
}

void CNESDMCChannel::WriteReg(TInt aReg, u8 aVal)
{
    switch (aReg & 3)
    {
    case 0:
        iIRQEnabled = (aVal & 0x80) != 0;
        iLoop       = (aVal & 0x40) != 0;
        iRateIndex  = aVal & 0x0F;
        iTickPeriod = KDMCRateTable[iRateIndex];
        break;
    case 1:
        iOutputLevel = aVal & 0x7F;
        break;
    case 2:
        iSampleAddr = 0xC000 + (aVal << 6);
        break;
    case 3:
        iSampleLen = (aVal << 4) + 1;
        break;
    }
}

TReal32 CNESDMCChannel::Sample()
{
    return iSilenceFlag ? 0.0f : (TReal32)iOutputLevel / 127.0f;
}

// ---------------------------------------------------------------------------
// CNESPAPU
// ---------------------------------------------------------------------------
CNESPAPU* CNESPAPU::NewLC(CNESCPU* aCpu)
{
    CNESPAPU* self = new (ELeave) CNESPAPU();
    CleanupStack::PushL(self);
    self->ConstructL(aCpu);
    return self;
}

CNESPAPU::CNESPAPU()
    : iCpu(NULL), iSq1(NULL), iSq2(NULL), iTri(NULL), iNoise(NULL), iDMC(NULL),
      iRunning(EFalse), iSampleRate(KSampleRate),
      iCyclesPerSample(0), iCycleAcc(0),
      iFrameCounter(0), i5StepMode(EFalse), iIRQInhibit(EFalse),
      iBufWrite(0), iBufPos(0),
      iOutputStream(NULL), iStreamOpen(EFalse)
{
    Mem::FillZ(iAudioBuf, sizeof(iAudioBuf));
}

void CNESPAPU::ConstructL(CNESCPU* aCpu)
{
    iCpu   = aCpu;
    iSq1   = new (ELeave) CNESSquareChannel(EFalse);
    iSq2   = new (ELeave) CNESSquareChannel(ETrue);
    iTri   = new (ELeave) CNESTriangleChannel();
    iNoise = new (ELeave) CNESNoiseChannel();
    iDMC   = new (ELeave) CNESDMCChannel();
    iDMC->SetCPU(aCpu);

    iCyclesPerSample = 41; // 1789773 / 44100

    initAudio();
}

void CNESPAPU::initAudio()
{
    QAudioFormat fmt;
    fmt.setSampleRate(KSampleRate);
    fmt.setChannelCount(1);
    fmt.setSampleSize(16);
    fmt.setCodec("audio/pcm");
    fmt.setByteOrder(QAudioFormat::LittleEndian);
    fmt.setSampleType(QAudioFormat::SignedInt);

    iAudioOutput = new QAudioOutput(fmt);
    iAudioOutput->setBufferSize(KAudioBufSamples * 2 * 2); // двойная буферизация
    iAudioDevice = iAudioOutput->start(); // push mode
}

CNESPAPU::~CNESPAPU()
{
    Stop();
    delete iAudioOutput;
    delete iSq1; delete iSq2; delete iTri; delete iNoise; delete iDMC;
}

void CNESPAPU::Reset()
{
    iSq1->Reset(); iSq2->Reset(); iTri->Reset(); iNoise->Reset(); iDMC->Reset();
    iFrameCounter = 0; i5StepMode = EFalse; iIRQInhibit = EFalse;
    iBufPos = 0; iBufWrite = 0;
    Mem::FillZ(iAudioBuf, sizeof(iAudioBuf));
}

void CNESPAPU::Start()  { iRunning = ETrue;  }
void CNESPAPU::Stop()   { iRunning = EFalse; }

// ---------------------------------------------------------------------------
// Запись в регистры APU
// ---------------------------------------------------------------------------
void CNESPAPU::WriteReg(TInt aAddr, u8 aVal)
{
    switch (aAddr)
    {
    case 0x4000: case 0x4001: case 0x4002: case 0x4003:
        iSq1->WriteReg(aAddr - 0x4000, aVal); break;
    case 0x4004: case 0x4005: case 0x4006: case 0x4007:
        iSq2->WriteReg(aAddr - 0x4004, aVal); break;
    case 0x4008: case 0x4009: case 0x400A: case 0x400B:
        iTri->WriteReg(aAddr - 0x4008, aVal); break;
    case 0x400C: case 0x400D: case 0x400E: case 0x400F:
        iNoise->WriteReg(aAddr - 0x400C, aVal); break;
    case 0x4010: case 0x4011: case 0x4012: case 0x4013:
        iDMC->WriteReg(aAddr - 0x4010, aVal); break;
    case 0x4015: // Status / enable channels
        // упрощённо: игнорируем enable-биты
        break;
    case 0x4017: // Frame counter
        i5StepMode  = (aVal & 0x80) != 0;
        iIRQInhibit = (aVal & 0x40) != 0;
        iFrameCounter = 0;
        break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// Тактирование (вызывается из CPU каждый CPU-такт)
// ---------------------------------------------------------------------------
void CNESPAPU::Clock()
{
    if (!iRunning || !Globals::enableSound) return;

    // Frame sequencer (~240 Hz)
    iFrameCounter++;
    TInt frameSteps = i5StepMode ? 18641 : 14915;
    if (iFrameCounter >= frameSteps)
    {
        iFrameCounter = 0;
        // Quarter-frame
        iSq1->ClockEnvelope(); iSq2->ClockEnvelope();
        iTri->ClockLinear();   iNoise->ClockEnvelope();
        // Half-frame
        iSq1->ClockLength(); iSq1->ClockSweep();
        iSq2->ClockLength(); iSq2->ClockSweep();
        iTri->ClockLength();
        iNoise->ClockLength();
    }

    // Генерация выходного сэмпла
    iCycleAcc++;
    if (iCycleAcc >= iCyclesPerSample)
    {
        iCycleAcc = 0;
        MixSample();
    }
}

// ---------------------------------------------------------------------------
// Микширование каналов в один PCM-сэмпл
// ---------------------------------------------------------------------------
void CNESPAPU::MixSample()
{
    // Mixer из APU specification (нелинейный lookup упрощён до линейного)
    TReal32 sq1  = iSq1->Sample();
    TReal32 sq2  = iSq2->Sample();
    TReal32 tri  = iTri->Sample();
    TReal32 noi  = iNoise->Sample();
    TReal32 dmc  = iDMC->Sample();

    TReal32 pulse = 0.0f;
    if (sq1 + sq2 > 0.0f)
        pulse = 95.88f / (8128.0f / (sq1 + sq2) + 100.0f);

    TReal32 tnd = 0.0f;
    if (tri + noi + dmc > 0.0f)
        tnd = 159.79f / (1.0f / (tri/8227.0f + noi/12241.0f + dmc/22638.0f) + 100.0f);

    TReal32 out = pulse + tnd;  // 0.0 … ~1.0

    s16 sample = (s16)(out * 32767.0f);
    iAudioBuf[iBufWrite][iBufPos++] = sample;

    if (iBufPos >= KAudioBufSamples)
    {
        iBufPos = 0;
        FlushBuffer();
        iBufWrite ^= 1;
    }
}

void CNESPAPU::FlushBuffer()
{
    if (!iAudioDevice || !iRunning) return;

    TInt src = iBufWrite ^ 1;
    const char* data = reinterpret_cast<const char*>(iAudioBuf[src]);
    qint64 len       = KAudioBufSamples * sizeof(s16);
    qint64 written   = 0;
    while (written < len)
    {
        qint64 n = iAudioDevice->write(data + written, len - written);
        if (n <= 0) break;
        written += n;
    }
}

void CNESPAPU::SetSampleRate(TInt aRate)
{
    iSampleRate      = aRate;
    iCyclesPerSample = 1789773 / aRate;
}

// ---------------------------------------------------------------------------
// Сохранение / загрузка состояния (минимально)
// ---------------------------------------------------------------------------
void CNESPAPU::StateSave(RWriteStream& aStream) const
{
    aStream.WriteInt32L(iSampleRate);
}

void CNESPAPU::StateLoad(RReadStream& aStream)
{
    SetSampleRate(aStream.ReadInt32L());
    Reset();
}
