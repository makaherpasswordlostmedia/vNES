/*
 * NESWidget.h — Qt UI для vNES на Nokia C7 (Symbian^3)
 * Заменяет vNESAppView.h + vNESAppUi.h
 *
 * QWidget рисует кадр через QPainter::drawImage()
 * QTimer тикает ~60 раз в секунду
 */

#ifndef NESWIDGET_H
#define NESWIDGET_H

#include <QWidget>
#include <QImage>
#include <QTimer>
#include <QKeyEvent>
#include "NESTypes.h"

class CNESSystem;

class NESWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NESWidget(QWidget* parent = 0);
    ~NESWidget();

    // Загрузить ROM и запустить
    bool loadROM(const QString& path);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onTimer();

private:
    void updateButtons(int key, bool pressed);
    void recalcDrawRect();

    CNESSystem* iNes;
    QTimer*     iTimer;
    QImage      iFrame;   // 256×240 RGB32 — заполняется из PPU буфера
    QRect       iDrawRect;// область вывода с сохранением пропорций 4:3
};

// Маппинг кнопок NES
namespace NESBtn {
    const u8 Right  = 0x01;
    const u8 Left   = 0x02;
    const u8 Down   = 0x04;
    const u8 Up     = 0x08;
    const u8 Start  = 0x10;
    const u8 Select = 0x20;
    const u8 B      = 0x40;
    const u8 A      = 0x80;
}

#endif // NESWIDGET_H
