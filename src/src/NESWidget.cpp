/*
 * NESWidget.cpp — Qt UI
 */
#include "NESWidget.h"
#include "NESSystem.h"
#include <QPainter>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QApplication>

// ~16 мс = 60 fps
static const int KFrameMs = 16;

NESWidget::NESWidget(QWidget* parent)
    : QWidget(parent)
    , iNes(NULL)
    , iTimer(new QTimer(this))
    , iFrame(KNES_WIDTH, KNES_HEIGHT, QImage::Format_RGB32)
{
    // Чёрный фон
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
    setAutoFillBackground(true);

    // На Symbian^3 показать полноэкранно
    showFullScreen();

    recalcDrawRect();

    connect(iTimer, SIGNAL(timeout()), this, SLOT(onTimer()));
}

NESWidget::~NESWidget()
{
    iTimer->stop();
    delete iNes;
}

bool NESWidget::loadROM(const QString& path)
{
    if (!iNes)
    {
        iNes = CNESSystem::NewLC();
        CleanupStack::Pop(); // управляем вручную через delete
    }

    // Конвертируем QString → TDesC (Symbian)
    TPtrC symPath(reinterpret_cast<const TUint16*>(path.utf16()), path.length());

    bool ok = false;
    TRAPD(err, ok = iNes->LoadRomL(symPath));
    if (err != KErrNone || !ok) return false;

    iNes->StartEmulation();
    iTimer->start(KFrameMs);
    return true;
}

void NESWidget::onTimer()
{
    if (!iNes) return;

    TRAPD(err, iNes->RunFrameL());
    if (err != KErrNone) return;

    // Скопировать ARGB-буфер PPU в QImage
    const u32* src = iNes->FrameBuffer();
    if (!src) return;

    for (int y = 0; y < KNES_HEIGHT; y++)
    {
        QRgb* dst = reinterpret_cast<QRgb*>(iFrame.scanLine(y));
        const u32* row = src + y * KNES_WIDTH;
        for (int x = 0; x < KNES_WIDTH; x++)
            dst[x] = row[x] | 0xFF000000; // добавить альфа
    }

    update(iDrawRect);
}

void NESWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (!iFrame.isNull())
        p.drawImage(iDrawRect, iFrame);
}

void NESWidget::resizeEvent(QResizeEvent*)
{
    recalcDrawRect();
}

void NESWidget::recalcDrawRect()
{
    int w = width();
    int h = height();
    // Сохранить пропорции 4:3 (NES = 256:240 ≈ 4:3.75, используем 4:3)
    int fw = (h * 4) / 3;
    if (fw <= w)
        iDrawRect = QRect((w - fw) / 2, 0, fw, h);
    else
    {
        int fh = (w * 3) / 4;
        iDrawRect = QRect(0, (h - fh) / 2, w, fh);
    }
}

// ---------------------------------------------------------------------------
// Ввод
// ---------------------------------------------------------------------------
void NESWidget::updateButtons(int key, bool pressed)
{
    if (!iNes) return;
    u8 bit = 0;
    switch (key)
    {
    case Qt::Key_Up:       bit = NESBtn::Up;     break;
    case Qt::Key_Down:     bit = NESBtn::Down;   break;
    case Qt::Key_Left:     bit = NESBtn::Left;   break;
    case Qt::Key_Right:    bit = NESBtn::Right;  break;
    // Nokia C7: центральная кнопка → Enter/Select
    case Qt::Key_Return:
    case Qt::Key_Enter:    bit = NESBtn::Start;  break;
    // Backspace → Select
    case Qt::Key_Backspace:bit = NESBtn::Select; break;
    // Громкость вверх → A (удобно держать)
    case Qt::Key_VolumeUp: bit = NESBtn::A;      break;
    // Громкость вниз → B
    case Qt::Key_VolumeDown: bit = NESBtn::B;    break;
    // Для тач-экрана можно добавить QTouchEvent позже
    default: return;
    }

    // Читаем текущее состояние, меняем бит, отправляем обратно
    // (CNESSystem хранит состояние внутри)
    static u8 buttons = 0;
    if (pressed) buttons |=  bit;
    else         buttons &= ~bit;
    iNes->SetJoyState(0, buttons);
}

void NESWidget::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape)
    {
        iTimer->stop();
        if (iNes) iNes->StopEmulation();
        close();
        return;
    }
    updateButtons(e->key(), true);
}

void NESWidget::keyReleaseEvent(QKeyEvent* e)
{
    updateButtons(e->key(), false);
}
