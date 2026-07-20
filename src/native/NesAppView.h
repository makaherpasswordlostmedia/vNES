/*
 * NesAppView.h — CCoeControl. Аналог NESWidget.cpp из Qt-версии:
 * таймер ~60 fps (CPeriodic), копирование ARGB-буфера PPU в
 * CFbsBitmap, отрисовка через CWindowGc с сохранением пропорций 4:3,
 * обработка клавиш через OfferKeyEventL.
 */
#ifndef NESAPPVIEW_H
#define NESAPPVIEW_H

#include <coecntrl.h>

class CNESSystem;
class CFbsBitmap;
class CPeriodic;

class CNesAppView : public CCoeControl
{
public:
    static CNesAppView* NewL(const TRect& aRect, CNESSystem& aNesSystem);
    ~CNesAppView();

    void Draw(const TRect& aRect) const;
    TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);

private:
    void ConstructL(const TRect& aRect);
    CNesAppView(CNESSystem& aNesSystem);

    static TInt TimerCallback(TAny* aPtr);
    void OnTick();
    void RecalcDrawRect();
    void UpdateButton(TUint aScanCode, TBool aPressed);
    void BlitFrameL();

private:
    CNESSystem& iNesSystem;
    CFbsBitmap* iFrameBitmap; // 256x240, EColor16MU — заполняется из PPU
    CPeriodic*  iTimer;
    TRect       iDrawRect;    // область вывода с пропорциями 4:3
    TUint8      iButtons;     // текущее состояние кнопок игрока 1
};

#endif // NESAPPVIEW_H
