#include "NesAppView.h"
#include "NESSystem.h"
#include "NESTypes.h"
#include <fbs.h>
#include <w32std.h>

// ~16 мс = 60 fps, как в оригинальной Qt-версии (KFrameMs)
static const TInt KFrameIntervalUs = 16 * 1000;

CNesAppView::CNesAppView(CNESSystem& aNesSystem)
    : iNesSystem(aNesSystem)
    , iFrameBitmap(NULL)
    , iTimer(NULL)
    , iButtons(0)
{
}

CNesAppView* CNesAppView::NewL(const TRect& aRect, CNESSystem& aNesSystem)
{
    CNesAppView* self = new (ELeave) CNesAppView(aNesSystem);
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    CleanupStack::Pop(self);
    return self;
}

void CNesAppView::ConstructL(const TRect& aRect)
{
    CreateWindowL();

    iFrameBitmap = new (ELeave) CFbsBitmap();
    User::LeaveIfError(iFrameBitmap->Create(TSize(KNES_WIDTH, KNES_HEIGHT), EColor16MU));

    iTimer = CPeriodic::NewL(CActive::EPriorityStandard);
    iTimer->Start(KFrameIntervalUs, KFrameIntervalUs, TCallBack(TimerCallback, this));

    SetRect(aRect);
    RecalcDrawRect();
    ActivateL();
}

CNesAppView::~CNesAppView()
{
    if (iTimer)
    {
        iTimer->Cancel();
        delete iTimer;
    }
    delete iFrameBitmap;
}

TInt CNesAppView::TimerCallback(TAny* aPtr)
{
    static_cast<CNesAppView*>(aPtr)->OnTick();
    return 1; // keep running
}

void CNesAppView::OnTick()
{
    TRAPD(err, iNesSystem.RunFrameL());
    if (err != KErrNone) return;

    TRAP(err, BlitFrameL());
    if (err != KErrNone) return;

    DrawNow();
}

void CNesAppView::BlitFrameL()
{
    const u32* src = iNesSystem.FrameBuffer();
    if (!src) return;

    iFrameBitmap->LockHeap();
    TUint32* dst = reinterpret_cast<TUint32*>(iFrameBitmap->DataAddress());
    const TInt stride = iFrameBitmap->DataStride() / sizeof(TUint32);

    for (TInt y = 0; y < KNES_HEIGHT; y++)
    {
        TUint32* dstRow = dst + y * stride;
        const u32* srcRow = src + y * KNES_WIDTH;
        for (TInt x = 0; x < KNES_WIDTH; x++)
            dstRow[x] = srcRow[x] | 0xFF000000; // добавить альфа, как в Qt-версии
    }
    iFrameBitmap->UnlockHeap();
}

void CNesAppView::Draw(const TRect& /*aRect*/) const
{
    CWindowGc& gc = SystemGc();
    gc.Clear(Rect());
    if (iFrameBitmap)
        gc.DrawBitmap(iDrawRect, iFrameBitmap, TRect(TPoint(0, 0), TSize(KNES_WIDTH, KNES_HEIGHT)));
}

void CNesAppView::RecalcDrawRect()
{
    // Сохранить пропорции 4:3 внутри клиентской области — как в
    // recalcDrawRect() из Qt-версии.
    TInt w = Rect().Width();
    TInt h = Rect().Height();

    TInt fw = (h * 4) / 3;
    if (fw <= w)
        iDrawRect = TRect(TPoint((w - fw) / 2, 0), TSize(fw, h));
    else
    {
        TInt fh = (w * 3) / 4;
        iDrawRect = TRect(TPoint(0, (h - fh) / 2), TSize(w, fh));
    }
}

void CNesAppView::UpdateButton(TUint aScanCode, TBool aPressed)
{
    TUint8 bit = 0;
    switch (aScanCode)
    {
    case EStdKeyUpArrow:    bit = 0x08; break; // NESBtn::Up
    case EStdKeyDownArrow:  bit = 0x04; break; // NESBtn::Down
    case EStdKeyLeftArrow:  bit = 0x02; break; // NESBtn::Left
    case EStdKeyRightArrow: bit = 0x01; break; // NESBtn::Right
    case EStdKeyDevice3:    bit = 0x10; break; // центр джойстика → Start
    case EStdKeyBackspace:  bit = 0x20; break; // Select
    case EStdKeyIncVolume:  bit = 0x80; break; // A
    case EStdKeyDecVolume:  bit = 0x40; break; // B
    default: return;
    }

    if (aPressed) iButtons |= bit;
    else          iButtons &= static_cast<TUint8>(~bit);

    iNesSystem.SetJoyState(0, iButtons);
}

TKeyResponse CNesAppView::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
{
    if (aType == EEventKeyDown)
    {
        UpdateButton(aKeyEvent.iScanCode, ETrue);
        return EKeyWasConsumed;
    }
    if (aType == EEventKeyUp)
    {
        UpdateButton(aKeyEvent.iScanCode, EFalse);
        return EKeyWasConsumed;
    }
    return EKeyWasNotConsumed;
}
