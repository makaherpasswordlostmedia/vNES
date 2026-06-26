/*
 * vNESAppView.cpp — CCoeControl
 * Рендеринг кадра NES на экран Symbian + обработка ввода.
 */
#include "vNESAppView.h"
#include "NESSystem.h"
#include <w32std.h>
#include <eikenv.h>

// Период таймера (16666 мкс ≈ 60 fps)
const TInt KFrameTimerPeriod = 16666;

// ---------------------------------------------------------------------------
CVNESAppView* CVNESAppView::NewL(const TRect& aRect)
{
    CVNESAppView* self = NewLC(aRect);
    CleanupStack::Pop(self);
    return self;
}

CVNESAppView* CVNESAppView::NewLC(const TRect& aRect)
{
    CVNESAppView* self = new (ELeave) CVNESAppView();
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    return self;
}

CVNESAppView::CVNESAppView()
    : iNes(NULL), iTimer(NULL), iBitmap(NULL)
{
    iButtons[0] = iButtons[1] = 0;
}

void CVNESAppView::ConstructL(const TRect& aRect)
{
    CreateWindowL();
    SetRect(aRect);

    iScreenSize = aRect.Size();

    // Вычислить прямоугольник рисования с сохранением пропорций 4:3 (NES)
    TInt w = iScreenSize.iWidth;
    TInt h = iScreenSize.iHeight;
    TInt nesW = (h * 256) / 240;
    if (nesW <= w)
        iDrawRect = TRect((w - nesW) / 2, 0, (w + nesW) / 2, h);
    else
    {
        TInt nesH = (w * 240) / 256;
        iDrawRect = TRect(0, (h - nesH) / 2, w, (h + nesH) / 2);
    }

    // Создать bitmap размером экрана для BitBlt
    iBitmap = new (ELeave) CFbsBitmap();
    User::LeaveIfError(iBitmap->Create(iScreenSize, EColor16MU));

    ActivateL();
}

CVNESAppView::~CVNESAppView()
{
    Stop();
    delete iBitmap;
}

// ---------------------------------------------------------------------------
void CVNESAppView::StartL()
{
    if (!iTimer)
    {
        iTimer = CPeriodic::NewL(CActive::EPriorityStandard);
        iTimer->Start(KFrameTimerPeriod, KFrameTimerPeriod,
                      TCallBack(TimerCallback, this));
    }
}

void CVNESAppView::Stop()
{
    if (iTimer)
    {
        iTimer->Cancel();
        delete iTimer;
        iTimer = NULL;
    }
}

// ---------------------------------------------------------------------------
TInt CVNESAppView::TimerCallback(TAny* aPtr)
{
    static_cast<CVNESAppView*>(aPtr)->OnTimer();
    return KErrNone;
}

void CVNESAppView::OnTimer()
{
    if (!iNes) return;

    // Передать состояние кнопок в NES
    iNes->SetJoyState(0, iButtons[0]);

    // Выполнить один кадр эмуляции
    TRAPD(err, iNes->RunFrameL());
    if (err != KErrNone) return;

    // Обновить экран
    if (iNes->FrameBuffer())
    {
        BlitFrameBuffer(iNes->FrameBuffer());
        DrawNow();
    }
}

// ---------------------------------------------------------------------------
// Масштабирование кадрового буфера PPU (256×240 ARGB) → CFbsBitmap
// ---------------------------------------------------------------------------
void CVNESAppView::BlitFrameBuffer(const u32* aSrc)
{
    TSize bmpSize = iDrawRect.Size();
    TInt  dstW    = bmpSize.iWidth;
    TInt  dstH    = bmpSize.iHeight;

    // Простое биlinear-free масштабирование ближайшим соседом
    iBitmap->Resize(bmpSize);

    TBitmapUtil bmpUtil(iBitmap);
    bmpUtil.Begin(TPoint(0, 0));

    for (TInt dy = 0; dy < dstH; dy++)
    {
        TInt sy = (dy * KNES_HEIGHT) / dstH;
        const u32* srcRow = aSrc + sy * KNES_WIDTH;
        for (TInt dx = 0; dx < dstW; dx++)
        {
            TInt sx = (dx * KNES_WIDTH) / dstW;
            u32 argb = srcRow[sx];
            // CFbsBitmap EColor16MU: 0x00RRGGBB
            bmpUtil.SetPos(TPoint(dx, dy));
            bmpUtil.SetPixel(argb & 0x00FFFFFF);
        }
    }
    bmpUtil.End();
}

// ---------------------------------------------------------------------------
void CVNESAppView::Draw(const TRect& /*aRect*/) const
{
    CWindowGc& gc = SystemGc();
    gc.SetBrushColor(KRgbBlack);
    gc.Clear(Rect());

    if (iBitmap && iNes)
        gc.BitBlt(iDrawRect.iTl, iBitmap);
}

TSize CVNESAppView::MinimumSize()
{
    return iScreenSize;
}

// ---------------------------------------------------------------------------
// Обработка нажатий клавиш
// ---------------------------------------------------------------------------
TInt CVNESAppView::MapKey(TInt aScanCode) const
{
    // Nokia 5800 / N97 / типичная Symbian-клавиатура
    // Адаптировать под конкретный телефон!
    switch (aScanCode)
    {
    case EStdKeyUpArrow:    return NESButtons::KUp;
    case EStdKeyDownArrow:  return NESButtons::KDown;
    case EStdKeyLeftArrow:  return NESButtons::KLeft;
    case EStdKeyRightArrow: return NESButtons::KRight;
    case EStdKeyDevice3:    // Joystick centre / OK → Start
        return NESButtons::KStart;
    case '1':               // 1 → Select
        return NESButtons::KSelect;
    case EStdKeyHash:       // # → B
    case 'Z':
        return NESButtons::KB;
    case EStdKeyNkpAsterisk: // * → A
    case 'X':
        return NESButtons::KA;
    default:
        return 0;
    }
}

TKeyResponse CVNESAppView::OfferKeyEventL(const TKeyEvent& aKeyEvent,
                                           TEventCode aType)
{
    TInt bit = MapKey(aKeyEvent.iScanCode);
    if (bit == 0) return EKeyWasNotConsumed;

    if (aType == EEventKeyDown)
        iButtons[0] |=  (u8)bit;
    else if (aType == EEventKeyUp)
        iButtons[0] &= ~(u8)bit;

    return EKeyWasConsumed;
}
