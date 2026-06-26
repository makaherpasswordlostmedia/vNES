/*
 * vNESAppView.h — Symbian CCoeControl для отображения экрана NES
 * и обработки нажатий клавиш.
 *
 * Рендеринг: CFbsBitmap → BitBlt на экран с масштабированием.
 * Таймер: CPeriodic, 16 мс (~60 fps).
 */

#ifndef VNESAPPVIEW_H
#define VNESAPPVIEW_H

#include <coecntrl.h>
#include <w32std.h>
#include <fbs.h>
#include "NESTypes.h"

class CNESSystem;

class CVNESAppView : public CCoeControl
{
public:
    static CVNESAppView* NewL(const TRect& aRect);
    static CVNESAppView* NewLC(const TRect& aRect);
    ~CVNESAppView();

    // Привязать систему NES (вызывается после загрузки ROM)
    void AttachNES(CNESSystem* aNes) { iNes = aNes; }

    // Запустить / остановить таймер кадров
    void StartL();
    void Stop();

    // CCoeControl
    void Draw(const TRect& aRect) const override;
    TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent,
                                TEventCode aType) override;
    TSize MinimumSize() override;

private:
    CVNESAppView();
    void ConstructL(const TRect& aRect);

    // Callback таймера (static + нестатический)
    static TInt TimerCallback(TAny* aPtr);
    void OnTimer();

    // Масштабировать кадровый буфер PPU в CFbsBitmap
    void BlitFrameBuffer(const u32* aSrc);

    // Маппинг клавиш Symbian → кнопки NES
    TInt MapKey(TInt aScanCode) const;

    CNESSystem*   iNes;
    CPeriodic*    iTimer;

    // Буфер для отображения (масштабированный под размер экрана)
    CFbsBitmap*   iBitmap;
    TSize         iScreenSize;   // размер экрана телефона
    TRect         iDrawRect;     // область рисования (с сохранением пропорций)

    // Текущее состояние кнопок (2 игрока)
    u8  iButtons[2];
};

// ---------------------------------------------------------------------------
// Маппинг кнопок NES (биты)
// ---------------------------------------------------------------------------
namespace NESButtons
{
    const u8 KRight  = 0x01;
    const u8 KLeft   = 0x02;
    const u8 KDown   = 0x04;
    const u8 KUp     = 0x08;
    const u8 KStart  = 0x10;
    const u8 KSelect = 0x20;
    const u8 KB      = 0x40;
    const u8 KA      = 0x80;
}

#endif // VNESAPPVIEW_H
